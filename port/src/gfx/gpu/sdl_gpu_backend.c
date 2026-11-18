/*
 * The renderer's backend on SDL_GPU.
 *
 * Each flush of the interpreter's frame becomes one command buffer: a copy
 * pass that uploads the frame's vertices, materials and new textures, then
 * a render pass per run of draws into one color target. The vertex and
 * material buffers are bound once per pass and the atlas once per pass, so
 * a draw costs a pipeline bind only when its fixed-function state changes,
 * a scissor and the draw call itself.
 *
 * Everything here runs on the game's graphics thread, apart from
 * presenting, which runs on the window's thread with its own command
 * buffer.
 */

#include "sdl_gpu_backend.h"

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_stdinc.h>

#include "frame.h"
#include "frames_in_flight.h"
#include "gfx_backend.h"
#include "port/os_host.h"
#include "shaders/rdp.frag.h"
#include "shaders/rdp.vert.h"
#include "texture_cache.h"

#define COLOR_FORMAT SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM
// Room for a frame's buffers grows in steps of this many bytes and more
#define BUFFER_GROWTH_MIN (256 * 1024)
// How far a decal is pulled toward the eye, in the depth buffer's units
#define DECAL_DEPTH_BIAS (-2.0f)
#define DECAL_SLOPE_BIAS (-1.0f)

struct GfxTarget {
    SDL_GPUTexture *texture;
    uint32_t width; // The N64's pixels; the texture is scaled
    uint32_t height;
    bool isDepth;
    bool needsClear; // A new depth target, whose contents are undefined
};

// A buffer on the GPU with the bytes that it has room for
typedef struct GrowableBuffer {
    SDL_GPUBuffer *buffer;
    uint32_t size;
} GrowableBuffer;

typedef struct GrowableTransfer {
    SDL_GPUTransferBuffer *buffer;
    uint32_t size;
} GrowableTransfer;

static struct {
    SDL_GPUDevice *device;
    uint32_t scale;
    SDL_GPUTextureFormat depthFormat;
    SDL_GPUShader *vertexShader;
    SDL_GPUShader *fragmentShader;
    // By pipeline key, without and with a depth target
    SDL_GPUGraphicsPipeline *pipelines[PIPELINE_KEY_COUNT][2];
    SDL_GPUTexture *atlas;
    SDL_GPUSampler *atlasSampler;
    GrowableBuffer vertices;
    GrowableBuffer materials;
    GrowableTransfer upload;
    GrowableTransfer pixelUpload;
    GrowableTransfer pixelDownload;
    // A texture at the N64's resolution, between memory and scaled targets
    SDL_GPUTexture *staging;
    uint32_t stagingWidth;
    uint32_t stagingHeight;
    uint32_t noiseSeed;
    FramesInFlight framesInFlight;
} f_Gpu;

static uint32_t GrownSize(uint32_t needed) {
    uint32_t size = BUFFER_GROWTH_MIN;

    while (size < needed) {
        size *= 2;
    }

    return size;
}

static void ReserveBuffer(GrowableBuffer *buffer, uint32_t size, SDL_GPUBufferUsageFlags usage) {
    SDL_GPUBufferCreateInfo info;

    if (buffer->size >= size) {
        return;
    }

    // A buffer that pending work still reads is released when the work is
    // done
    if (buffer->buffer != NULL) {
        SDL_ReleaseGPUBuffer(f_Gpu.device, buffer->buffer);
    }

    SDL_zero(info);
    info.usage = usage;
    info.size = GrownSize(size);
    buffer->buffer = SDL_CreateGPUBuffer(f_Gpu.device, &info);
    if (buffer->buffer == NULL) {
        PcFatalError("Can't create a GPU buffer of %u bytes: %s", info.size, SDL_GetError());
    }

    buffer->size = info.size;
}

static void ReserveTransfer(
    GrowableTransfer *transfer,
    uint32_t size,
    SDL_GPUTransferBufferUsage usage
) {
    SDL_GPUTransferBufferCreateInfo info;

    if (transfer->size >= size) {
        return;
    }

    if (transfer->buffer != NULL) {
        SDL_ReleaseGPUTransferBuffer(f_Gpu.device, transfer->buffer);
    }

    SDL_zero(info);
    info.usage = usage;
    info.size = GrownSize(size);
    transfer->buffer = SDL_CreateGPUTransferBuffer(f_Gpu.device, &info);
    if (transfer->buffer == NULL) {
        PcFatalError("Can't create a transfer buffer of %u bytes: %s", info.size, SDL_GetError());
    }

    transfer->size = info.size;
}

static SDL_GPUShader *CreateShader(
    SDL_GPUShaderStage stage,
    const unsigned char *spirv,
    size_t spirvSize,
    const unsigned char *msl,
    size_t mslSize,
    uint32_t samplerCount,
    uint32_t storageBufferCount,
    uint32_t uniformBufferCount
) {
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(f_Gpu.device);
    SDL_GPUShaderCreateInfo info;
    SDL_GPUShader *shader;

    SDL_zero(info);
    info.stage = stage;
    info.num_samplers = samplerCount;
    info.num_storage_buffers = storageBufferCount;
    info.num_uniform_buffers = uniformBufferCount;
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = spirv;
        info.code_size = spirvSize;
        info.entrypoint = "main";
    }
    else if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        info.format = SDL_GPU_SHADERFORMAT_MSL;
        info.code = msl;
        info.code_size = mslSize - 1; // Without its terminating zero
        info.entrypoint = "main0";
    }
    else {
        PcFatalError("The GPU takes neither SPIR-V nor MSL shaders");
    }

    shader = SDL_CreateGPUShader(f_Gpu.device, &info);
    if (shader == NULL) {
        PcFatalError("Can't create a shader: %s", SDL_GetError());
    }

    return shader;
}

static SDL_GPUBlendFactor DestinationBlendFactor(uint32_t destination) {
    switch (destination) {
        case DESTINATION_ZERO:
            return SDL_GPU_BLENDFACTOR_ZERO;
        case DESTINATION_ONE:
            return SDL_GPU_BLENDFACTOR_ONE;
        case DESTINATION_SOURCE_ALPHA:
            return SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        case DESTINATION_DESTINATION_ALPHA:
            return SDL_GPU_BLENDFACTOR_DST_ALPHA;
        case DESTINATION_ONE_MINUS_SOURCE_ALPHA:
            return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        default:
            SDL_assert(!"An unknown destination factor");
            return SDL_GPU_BLENDFACTOR_ZERO;
    }
}

// The blend that a key's bits ask for. The shader gives its share of the
// blender's sum, so the GPU adds memory times the destination factor.
static SDL_GPUColorTargetBlendState BlendState(uint16_t key) {
    SDL_GPUColorTargetBlendState blend;
    uint32_t destination = (key & PIPELINE_DESTINATION_MASK) >> PIPELINE_DESTINATION_SHIFT;

    SDL_zero(blend);
    blend.color_write_mask =
        SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B;
    if (key & PIPELINE_WRITE_ALPHA) {
        blend.color_write_mask |= SDL_GPU_COLORCOMPONENT_A;
    }

    blend.enable_color_write_mask = true;
    if (key & PIPELINE_BLEND) {
        blend.enable_blend = true;
        blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend.dst_color_blendfactor = DestinationBlendFactor(destination);
        blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
        // Alpha isn't written where draws blend
        blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        blend.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    }

    return blend;
}

static SDL_GPUGraphicsPipeline *CreatePipeline(uint16_t key, bool hasDepth) {
    SDL_GPUVertexBufferDescription vertexBuffer;
    SDL_GPUVertexAttribute attributes[4];
    SDL_GPUColorTargetDescription colorTarget;
    SDL_GPUGraphicsPipelineCreateInfo info;
    SDL_GPUGraphicsPipeline *pipeline;

    SDL_zero(vertexBuffer);
    vertexBuffer.slot = 0;
    vertexBuffer.pitch = sizeof(GfxVertex);
    vertexBuffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_zeroa(attributes);
    attributes[0].location = 0;
    attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attributes[0].offset = offsetof(GfxVertex, position);
    attributes[1].location = 1;
    attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[1].offset = offsetof(GfxVertex, texCoord);
    attributes[2].location = 2;
    attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
    attributes[2].offset = offsetof(GfxVertex, color);
    attributes[3].location = 3;
    attributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_UINT;
    attributes[3].offset = offsetof(GfxVertex, material);

    SDL_zero(colorTarget);
    colorTarget.format = COLOR_FORMAT;
    colorTarget.blend_state = BlendState(key);

    SDL_zero(info);
    info.vertex_shader = f_Gpu.vertexShader;
    info.fragment_shader = f_Gpu.fragmentShader;
    info.vertex_input_state.vertex_buffer_descriptions = &vertexBuffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attributes;
    info.vertex_input_state.num_vertex_attributes = SDL_arraysize(attributes);
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.rasterizer_state.enable_depth_clip = true;
    if (key & PIPELINE_DEPTH_DECAL) {
        info.rasterizer_state.enable_depth_bias = true;
        info.rasterizer_state.depth_bias_constant_factor = DECAL_DEPTH_BIAS;
        info.rasterizer_state.depth_bias_slope_factor = DECAL_SLOPE_BIAS;
    }

    // A GPU writes depth only where it tests it, so a write without a test
    // tests with a compare that always passes
    if (hasDepth && (key & (PIPELINE_DEPTH_TEST | PIPELINE_DEPTH_WRITE))) {
        info.depth_stencil_state.enable_depth_test = true;
        info.depth_stencil_state.enable_depth_write = (key & PIPELINE_DEPTH_WRITE) != 0;
        info.depth_stencil_state.compare_op = (key & PIPELINE_DEPTH_TEST) ?
            SDL_GPU_COMPAREOP_LESS_OR_EQUAL :
            SDL_GPU_COMPAREOP_ALWAYS;
    }

    info.target_info.color_target_descriptions = &colorTarget;
    info.target_info.num_color_targets = 1;
    info.target_info.has_depth_stencil_target = hasDepth;
    info.target_info.depth_stencil_format = f_Gpu.depthFormat;

    pipeline = SDL_CreateGPUGraphicsPipeline(f_Gpu.device, &info);
    if (pipeline == NULL) {
        PcFatalError("Can't create the pipeline for key 0x%02X: %s", key, SDL_GetError());
    }

    return pipeline;
}

static SDL_GPUGraphicsPipeline *UsePipeline(uint16_t key, bool hasDepth) {
    SDL_GPUGraphicsPipeline **pipeline = &f_Gpu.pipelines[key][hasDepth ? 1 : 0];

    SDL_assert(key < PIPELINE_KEY_COUNT);
    if (*pipeline == NULL) {
        *pipeline = CreatePipeline(key, hasDepth);
    }

    return *pipeline;
}

static SDL_GPUTextureFormat ChooseDepthFormat(void) {
    if (SDL_GPUTextureSupportsFormat(
            f_Gpu.device,
            SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
            SDL_GPU_TEXTURETYPE_2D,
            SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
        )) {
        return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    }

    return SDL_GPU_TEXTUREFORMAT_D24_UNORM;
}

static SDL_GPUSampler *CreateSampler(SDL_GPUFilter filter) {
    SDL_GPUSamplerCreateInfo info;
    SDL_GPUSampler *sampler;

    SDL_zero(info);
    info.min_filter = filter;
    info.mag_filter = filter;
    info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler = SDL_CreateGPUSampler(f_Gpu.device, &info);
    if (sampler == NULL) {
        PcFatalError("Can't create a sampler: %s", SDL_GetError());
    }

    return sampler;
}

static SDL_GPUTexture *CreateTexture(
    SDL_GPUTextureFormat format,
    SDL_GPUTextureUsageFlags usage,
    uint32_t width,
    uint32_t height,
    uint32_t layerCount
) {
    SDL_GPUTextureCreateInfo info;
    SDL_GPUTexture *texture;

    SDL_zero(info);
    info.type = layerCount > 1 ? SDL_GPU_TEXTURETYPE_2D_ARRAY : SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = usage;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = layerCount;
    info.num_levels = 1;
    texture = SDL_CreateGPUTexture(f_Gpu.device, &info);
    if (texture == NULL) {
        PcFatalError("Can't create a %ux%u texture: %s", width, height, SDL_GetError());
    }

    return texture;
}

void InitGpuBackend(SDL_GPUDevice *device, uint32_t scale) {
    SDL_assert(device != NULL && scale >= 1);
    SDL_zero(f_Gpu);
    f_Gpu.device = device;
    f_Gpu.scale = scale;
    f_Gpu.depthFormat = ChooseDepthFormat();
    f_Gpu.vertexShader = CreateShader(
        SDL_GPU_SHADERSTAGE_VERTEX,
        rdp_vert_spirv,
        sizeof(rdp_vert_spirv),
        rdp_vert_msl,
        sizeof(rdp_vert_msl),
        0,
        0,
        0
    );
    f_Gpu.fragmentShader = CreateShader(
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        rdp_frag_spirv,
        sizeof(rdp_frag_spirv),
        rdp_frag_msl,
        sizeof(rdp_frag_msl),
        1,
        1,
        1
    );
    f_Gpu.atlas = CreateTexture(
        COLOR_FORMAT,
        SDL_GPU_TEXTUREUSAGE_SAMPLER,
        ATLAS_SIDE,
        ATLAS_SIDE,
        ATLAS_LAYER_COUNT
    );
    // The shader fetches texels itself, so the sampler never filters
    f_Gpu.atlasSampler = CreateSampler(SDL_GPU_FILTER_NEAREST);
}

void ShutdownGpuBackend(void) {
    SDL_GPUFence *fence;
    int key;

    if (f_Gpu.device == NULL) {
        return;
    }

    SDL_WaitForGPUIdle(f_Gpu.device);
    while ((fence = TakeOldestFrameInFlight(&f_Gpu.framesInFlight)) != NULL) {
        SDL_ReleaseGPUFence(f_Gpu.device, fence);
    }

    for (key = 0; key < PIPELINE_KEY_COUNT; key++) {
        SDL_ReleaseGPUGraphicsPipeline(f_Gpu.device, f_Gpu.pipelines[key][0]);
        SDL_ReleaseGPUGraphicsPipeline(f_Gpu.device, f_Gpu.pipelines[key][1]);
    }

    SDL_ReleaseGPUShader(f_Gpu.device, f_Gpu.vertexShader);
    SDL_ReleaseGPUShader(f_Gpu.device, f_Gpu.fragmentShader);
    SDL_ReleaseGPUTexture(f_Gpu.device, f_Gpu.atlas);
    SDL_ReleaseGPUTexture(f_Gpu.device, f_Gpu.staging);
    SDL_ReleaseGPUSampler(f_Gpu.device, f_Gpu.atlasSampler);
    SDL_ReleaseGPUBuffer(f_Gpu.device, f_Gpu.vertices.buffer);
    SDL_ReleaseGPUBuffer(f_Gpu.device, f_Gpu.materials.buffer);
    SDL_ReleaseGPUTransferBuffer(f_Gpu.device, f_Gpu.upload.buffer);
    SDL_ReleaseGPUTransferBuffer(f_Gpu.device, f_Gpu.pixelUpload.buffer);
    SDL_ReleaseGPUTransferBuffer(f_Gpu.device, f_Gpu.pixelDownload.buffer);
    SDL_zero(f_Gpu);
}

uint32_t GpuBackendScale(void) {
    return f_Gpu.scale;
}

SDL_GPUTexture *GpuTargetTexture(const GfxTarget *target) {
    return target->texture;
}

GfxTarget *BackendCreateTarget(uint32_t width, uint32_t height, bool isDepth) {
    GfxTarget *target = SDL_calloc(1, sizeof(*target));

    if (target == NULL) {
        PcFatalError("Out of memory for a render target");
    }

    target->width = width;
    target->height = height;
    target->isDepth = isDepth;
    target->needsClear = isDepth;
    target->texture = CreateTexture(
        isDepth ? f_Gpu.depthFormat : COLOR_FORMAT,
        isDepth ? SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET :
                  SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
        width * f_Gpu.scale,
        height * f_Gpu.scale,
        1
    );
    return target;
}

void BackendDestroyTarget(GfxTarget *target) {
    if (target == NULL) {
        return;
    }

    // SDL_GPU keeps the texture until the work that uses it is done
    SDL_ReleaseGPUTexture(f_Gpu.device, target->texture);
    SDL_free(target);
}

static SDL_GPUCommandBuffer *AcquireCommandBuffer(void) {
    SDL_GPUCommandBuffer *commands = SDL_AcquireGPUCommandBuffer(f_Gpu.device);

    if (commands == NULL) {
        PcFatalError("Can't acquire a GPU command buffer: %s", SDL_GetError());
    }

    return commands;
}

static void Submit(SDL_GPUCommandBuffer *commands) {
    if (!SDL_SubmitGPUCommandBuffer(commands)) {
        PcFatalError("Can't submit GPU commands: %s", SDL_GetError());
    }
}

// Submits a frame, then waits for the oldest frame in flight once there
// are more than FRAMES_IN_FLIGHT, so that the CPU never runs far ahead of
// the GPU. Its fence also covers the copies submitted before it.
static void SubmitFrame(SDL_GPUCommandBuffer *commands) {
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
    SDL_GPUFence *oldest;

    if (fence == NULL) {
        PcFatalError("Can't submit GPU commands: %s", SDL_GetError());
    }

    oldest = AddFrameInFlight(&f_Gpu.framesInFlight, fence);
    if (oldest != NULL) {
        SDL_WaitForGPUFences(f_Gpu.device, true, &oldest, 1);
        SDL_ReleaseGPUFence(f_Gpu.device, oldest);
    }
}

void BackendCopyTarget(GfxTarget *source, GfxTarget *destination, uint32_t width, uint32_t height) {
    SDL_GPUCommandBuffer *commands = AcquireCommandBuffer();
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(commands);
    SDL_GPUTextureLocation from;
    SDL_GPUTextureLocation to;

    SDL_zero(from);
    SDL_zero(to);
    from.texture = source->texture;
    to.texture = destination->texture;
    SDL_CopyGPUTextureToTexture(
        copy,
        &from,
        &to,
        width * f_Gpu.scale,
        height * f_Gpu.scale,
        1,
        false
    );
    SDL_EndGPUCopyPass(copy);
    Submit(commands);
}

// The frame's uploads into one transfer buffer: vertices, then materials,
// then texels
static void UploadFrame(SDL_GPUCommandBuffer *commands, const GfxFrame *frame) {
    uint32_t vertexBytes = frame->vertexCount * (uint32_t)sizeof(GfxVertex);
    uint32_t materialBytes = frame->materialCount * (uint32_t)sizeof(GfxMaterial);
    uint32_t texelBytes = frame->texelCount * (uint32_t)sizeof(uint32_t);
    uint32_t materialOffset = vertexBytes;
    uint32_t texelOffset = materialOffset + materialBytes;
    SDL_GPUCopyPass *copy;
    uint8_t *mapped;
    uint32_t i;

    ReserveTransfer(&f_Gpu.upload, texelOffset + texelBytes, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD);
    ReserveBuffer(&f_Gpu.vertices, vertexBytes, SDL_GPU_BUFFERUSAGE_VERTEX);
    ReserveBuffer(&f_Gpu.materials, materialBytes, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ);

    // Cycling gives a transfer buffer that no pending upload reads
    mapped = SDL_MapGPUTransferBuffer(f_Gpu.device, f_Gpu.upload.buffer, true);
    if (mapped == NULL) {
        PcFatalError("Can't map the upload buffer: %s", SDL_GetError());
    }

    SDL_memcpy(mapped, frame->vertices, vertexBytes);
    SDL_memcpy(mapped + materialOffset, frame->materials, materialBytes);
    SDL_memcpy(mapped + texelOffset, frame->texels, texelBytes);
    SDL_UnmapGPUTransferBuffer(f_Gpu.device, f_Gpu.upload.buffer);

    copy = SDL_BeginGPUCopyPass(commands);
    if (vertexBytes != 0) {
        SDL_GPUTransferBufferLocation source = {f_Gpu.upload.buffer, 0};
        SDL_GPUBufferRegion destination = {f_Gpu.vertices.buffer, 0, vertexBytes};

        // Cycling leaves the vertices of frames still being drawn alone
        SDL_UploadToGPUBuffer(copy, &source, &destination, true);
    }

    if (materialBytes != 0) {
        SDL_GPUTransferBufferLocation source = {f_Gpu.upload.buffer, materialOffset};
        SDL_GPUBufferRegion destination = {f_Gpu.materials.buffer, 0, materialBytes};

        SDL_UploadToGPUBuffer(copy, &source, &destination, true);
    }

    for (i = 0; i < frame->uploadCount; i++) {
        const GfxTextureUpload *upload = &frame->uploads[i];
        SDL_GPUTextureTransferInfo source;
        SDL_GPUTextureRegion destination;

        SDL_zero(source);
        source.transfer_buffer = f_Gpu.upload.buffer;
        source.offset = texelOffset + (upload->texelOffset * (uint32_t)sizeof(uint32_t));
        source.pixels_per_row = upload->width;
        source.rows_per_layer = upload->height;
        SDL_zero(destination);
        destination.texture = f_Gpu.atlas;
        destination.layer = upload->layer;
        destination.x = upload->x;
        destination.y = upload->y;
        destination.w = upload->width;
        destination.h = upload->height;
        destination.d = 1;
        // The atlas keeps the textures uploaded before, so no cycling
        SDL_UploadToGPUTexture(copy, &source, &destination, false);
    }

    SDL_EndGPUCopyPass(copy);
}

static SDL_GPURenderPass *BeginPass(SDL_GPUCommandBuffer *commands, const GfxPass *pass) {
    SDL_GPUColorTargetInfo color;
    SDL_GPUDepthStencilTargetInfo depth;

    SDL_zero(color);
    color.texture = pass->target->texture;
    color.load_op = SDL_GPU_LOADOP_LOAD;
    color.store_op = SDL_GPU_STOREOP_STORE;
    if (pass->depth == NULL) {
        return SDL_BeginGPURenderPass(commands, &color, 1, NULL);
    }

    SDL_zero(depth);
    depth.texture = pass->depth->texture;
    depth.load_op = SDL_GPU_LOADOP_LOAD;
    depth.store_op = SDL_GPU_STOREOP_STORE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    if (pass->clearsDepth || pass->depth->needsClear) {
        depth.load_op = SDL_GPU_LOADOP_CLEAR;
        depth.clear_depth = pass->clearsDepth ? pass->depthClearValue : 1.0f;
        pass->depth->needsClear = false;
    }

    return SDL_BeginGPURenderPass(commands, &color, 1, &depth);
}

static void DrawPass(SDL_GPUCommandBuffer *commands, const GfxFrame *frame, const GfxPass *pass) {
    SDL_GPURenderPass *render;
    SDL_GPUBufferBinding vertexBinding = {f_Gpu.vertices.buffer, 0};
    SDL_GPUTextureSamplerBinding atlasBinding = {f_Gpu.atlas, f_Gpu.atlasSampler};
    SDL_GPUGraphicsPipeline *boundPipeline = NULL;
    GfxRect boundScissor;
    bool hasScissor = false;
    bool hasDepth = pass->depth != NULL;
    uint32_t i;

    render = BeginPass(commands, pass);
    if (render == NULL) {
        PcFatalError("Can't begin a render pass: %s", SDL_GetError());
    }

    if (pass->drawCount != 0) {
        SDL_BindGPUVertexBuffers(render, 0, &vertexBinding, 1);
        SDL_BindGPUFragmentSamplers(render, 0, &atlasBinding, 1);
        SDL_BindGPUFragmentStorageBuffers(render, 0, &f_Gpu.materials.buffer, 1);
    }

    for (i = pass->firstDraw; i < pass->firstDraw + pass->drawCount; i++) {
        const GfxDraw *draw = &frame->draws[i];
        SDL_GPUGraphicsPipeline *pipeline = UsePipeline(draw->pipelineKey, hasDepth);

        if (pipeline != boundPipeline) {
            SDL_BindGPUGraphicsPipeline(render, pipeline);
            boundPipeline = pipeline;
        }

        if (!hasScissor || SDL_memcmp(&draw->scissor, &boundScissor, sizeof(boundScissor)) != 0) {
            SDL_Rect scissor;

            scissor.x = draw->scissor.left * (int)f_Gpu.scale;
            scissor.y = draw->scissor.top * (int)f_Gpu.scale;
            scissor.w = (draw->scissor.right - draw->scissor.left) * (int)f_Gpu.scale;
            scissor.h = (draw->scissor.bottom - draw->scissor.top) * (int)f_Gpu.scale;
            SDL_SetGPUScissor(render, &scissor);
            boundScissor = draw->scissor;
            hasScissor = true;
        }

        SDL_DrawGPUPrimitives(render, draw->vertexCount, 1, draw->firstVertex, 0);
    }

    SDL_EndGPURenderPass(render);
}

void BackendExecuteFrame(const GfxFrame *frame) {
    SDL_GPUCommandBuffer *commands;
    uint32_t i;

    if (frame->passCount == 0 && frame->uploadCount == 0) {
        return;
    }

    commands = AcquireCommandBuffer();
    UploadFrame(commands, frame);
    f_Gpu.noiseSeed++;
    SDL_PushGPUFragmentUniformData(commands, 0, &f_Gpu.noiseSeed, sizeof(f_Gpu.noiseSeed));
    for (i = 0; i < frame->passCount; i++) {
        const GfxPass *pass = &frame->passes[i];

        if (pass->drawCount != 0 || pass->clearsDepth) {
            DrawPass(commands, frame, pass);
        }
    }

    SubmitFrame(commands);
}

// The staging texture, at least as big as the N64's image
static SDL_GPUTexture *UseStaging(uint32_t width, uint32_t height) {
    if (f_Gpu.staging == NULL || f_Gpu.stagingWidth < width || f_Gpu.stagingHeight < height) {
        SDL_ReleaseGPUTexture(f_Gpu.device, f_Gpu.staging);
        f_Gpu.stagingWidth = SDL_max(width, f_Gpu.stagingWidth);
        f_Gpu.stagingHeight = SDL_max(height, f_Gpu.stagingHeight);
        f_Gpu.staging = CreateTexture(
            COLOR_FORMAT,
            SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
            f_Gpu.stagingWidth,
            f_Gpu.stagingHeight,
            1
        );
    }

    return f_Gpu.staging;
}

static void BlitTexture(
    SDL_GPUCommandBuffer *commands,
    SDL_GPUTexture *source,
    uint32_t sourceWidth,
    uint32_t sourceHeight,
    SDL_GPUTexture *destination,
    uint32_t destinationTop,
    uint32_t destinationWidth,
    uint32_t destinationHeight,
    SDL_GPUFilter filter
) {
    SDL_GPUBlitInfo blit;

    SDL_zero(blit);
    blit.source.texture = source;
    blit.source.w = sourceWidth;
    blit.source.h = sourceHeight;
    blit.destination.texture = destination;
    blit.destination.y = destinationTop;
    blit.destination.w = destinationWidth;
    blit.destination.h = destinationHeight;
    blit.load_op = SDL_GPU_LOADOP_LOAD;
    blit.filter = filter;
    SDL_BlitGPUTexture(commands, &blit);
}

void BackendReadTarget(GfxTarget *target, uint32_t width, uint32_t height, uint32_t *rgba) {
    uint32_t byteCount = width * height * (uint32_t)sizeof(uint32_t);
    SDL_GPUCommandBuffer *commands = AcquireCommandBuffer();
    SDL_GPUTexture *source = target->texture;
    SDL_GPUTextureRegion region;
    SDL_GPUTextureTransferInfo destination;
    SDL_GPUCopyPass *copy;
    SDL_GPUFence *fence;
    const uint8_t *mapped;

    SDL_assert(!target->isDepth);
    ReserveTransfer(&f_Gpu.pixelDownload, byteCount, SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD);
    if (f_Gpu.scale != 1) {
        // Down to the N64's pixels, each the average of those it covers
        source = UseStaging(width, height);
        BlitTexture(
            commands,
            target->texture,
            width * f_Gpu.scale,
            height * f_Gpu.scale,
            source,
            0,
            width,
            height,
            SDL_GPU_FILTER_LINEAR
        );
    }

    SDL_zero(region);
    region.texture = source;
    region.w = width;
    region.h = height;
    region.d = 1;
    SDL_zero(destination);
    destination.transfer_buffer = f_Gpu.pixelDownload.buffer;
    destination.pixels_per_row = width;
    destination.rows_per_layer = height;
    copy = SDL_BeginGPUCopyPass(commands);
    SDL_DownloadFromGPUTexture(copy, &region, &destination);
    SDL_EndGPUCopyPass(copy);

    fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
    if (fence == NULL) {
        PcFatalError("Can't submit a read of a target: %s", SDL_GetError());
    }

    SDL_WaitForGPUFences(f_Gpu.device, true, &fence, 1);
    SDL_ReleaseGPUFence(f_Gpu.device, fence);
    mapped = SDL_MapGPUTransferBuffer(f_Gpu.device, f_Gpu.pixelDownload.buffer, false);
    if (mapped == NULL) {
        PcFatalError("Can't map the download buffer: %s", SDL_GetError());
    }

    SDL_memcpy(rgba, mapped, byteCount);
    SDL_UnmapGPUTransferBuffer(f_Gpu.device, f_Gpu.pixelDownload.buffer);
}

void BackendWriteTarget(
    GfxTarget *target,
    uint32_t width,
    uint32_t firstRow,
    uint32_t rowCount,
    const uint32_t *rgba
) {
    uint32_t byteCount = width * rowCount * (uint32_t)sizeof(uint32_t);
    uint32_t destinationRow = firstRow;
    SDL_GPUCommandBuffer *commands = AcquireCommandBuffer();
    SDL_GPUTexture *destinationTexture = target->texture;
    SDL_GPUTextureTransferInfo source;
    SDL_GPUTextureRegion destination;
    SDL_GPUCopyPass *copy;
    void *mapped;

    SDL_assert(!target->isDepth);
    ReserveTransfer(&f_Gpu.pixelUpload, byteCount, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD);
    mapped = SDL_MapGPUTransferBuffer(f_Gpu.device, f_Gpu.pixelUpload.buffer, true);
    if (mapped == NULL) {
        PcFatalError("Can't map the pixel upload buffer: %s", SDL_GetError());
    }

    SDL_memcpy(mapped, rgba, byteCount);
    SDL_UnmapGPUTransferBuffer(f_Gpu.device, f_Gpu.pixelUpload.buffer);

    if (f_Gpu.scale != 1) {
        destinationTexture = UseStaging(width, rowCount);
        destinationRow = 0;
    }

    SDL_zero(source);
    source.transfer_buffer = f_Gpu.pixelUpload.buffer;
    source.pixels_per_row = width;
    source.rows_per_layer = rowCount;
    SDL_zero(destination);
    destination.texture = destinationTexture;
    destination.y = destinationRow;
    destination.w = width;
    destination.h = rowCount;
    destination.d = 1;
    copy = SDL_BeginGPUCopyPass(commands);
    SDL_UploadToGPUTexture(copy, &source, &destination, false);
    SDL_EndGPUCopyPass(copy);

    if (f_Gpu.scale != 1) {
        // Each of the N64's pixels becomes a square of the target's
        BlitTexture(
            commands,
            destinationTexture,
            width,
            rowCount,
            target->texture,
            firstRow * f_Gpu.scale,
            width * f_Gpu.scale,
            rowCount * f_Gpu.scale,
            SDL_GPU_FILTER_NEAREST
        );
    }

    Submit(commands);
}
