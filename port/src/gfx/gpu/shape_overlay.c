/*
 * The shape overlay. Its shaders make each shape's square from a uniform
 * block, with no vertex buffer, so a frame's shapes cost one uniform push
 * and one instanced draw.
 */

#include "shape_overlay.h"

#include <SDL3/SDL.h>

#include "sdl_gpu_backend.h"
#include "shaders/overlay.frag.h"
#include "shaders/overlay.vert.h"

#define VERTICES_PER_SHAPE 6

// The shaders' uniform block, in std140's layout
typedef struct ShapeUniform {
    float circle[4];
    float color[4];
    uint32_t kind[4];
} ShapeUniform;

typedef struct OverlayUniforms {
    float targetSize[4];
    ShapeUniform shapes[OVERLAY_MAX_SHAPES];
} OverlayUniforms;

SDL_COMPILE_TIME_ASSERT(
    overlay_uniforms_std140,
    sizeof(OverlayUniforms) == (4 * sizeof(float)) + (OVERLAY_MAX_SHAPES * 48)
);

static SDL_GPUDevice *f_Device;
static SDL_GPUGraphicsPipeline *f_Pipeline;

bool InitShapeOverlay(SDL_GPUDevice *device, SDL_GPUTextureFormat format) {
    SDL_GPUGraphicsPipelineCreateInfo info;
    SDL_GPUColorTargetDescription target;

    SDL_assert(device != NULL);
    SDL_zero(target);
    target.format = format;
    target.blend_state.enable_blend = true;
    target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    SDL_zero(info);
    info.vertex_shader = CreateGpuShader(
        device,
        SDL_GPU_SHADERSTAGE_VERTEX,
        overlay_vert_spirv,
        sizeof(overlay_vert_spirv),
        overlay_vert_msl,
        sizeof(overlay_vert_msl),
        0,
        0,
        1
    );
    info.fragment_shader = CreateGpuShader(
        device,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        overlay_frag_spirv,
        sizeof(overlay_frag_spirv),
        overlay_frag_msl,
        sizeof(overlay_frag_msl),
        0,
        0,
        0
    );
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    // The device has no depth clamping, as CreateGpuDevice asks
    info.rasterizer_state.enable_depth_clip = true;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;
    f_Pipeline = SDL_CreateGPUGraphicsPipeline(device, &info);
    SDL_ReleaseGPUShader(device, info.vertex_shader);
    SDL_ReleaseGPUShader(device, info.fragment_shader);
    if (f_Pipeline == NULL) {
        SDL_Log("Can't create the overlay's pipeline: %s", SDL_GetError());
        return false;
    }

    f_Device = device;
    return true;
}

void ShutdownShapeOverlay(void) {
    if (f_Device != NULL) {
        SDL_ReleaseGPUGraphicsPipeline(f_Device, f_Pipeline);
    }

    f_Pipeline = NULL;
    f_Device = NULL;
}

static void PackShape(const OverlayShape *shape, ShapeUniform *uniform) {
    int channel;

    SDL_zerop(uniform);
    uniform->circle[0] = shape->x;
    uniform->circle[1] = shape->y;
    uniform->circle[2] = shape->radius;
    for (channel = 0; channel < 4; channel++) {
        uniform->color[channel] = (float)((shape->color >> (channel * 8)) & 0xFF) / 255.0f;
    }

    uniform->kind[0] = shape->style;
    uniform->kind[1] = shape->label;
}

void DrawShapeOverlay(
    SDL_GPUCommandBuffer *commands,
    SDL_GPUTexture *target,
    uint32_t width,
    uint32_t height,
    const OverlayShape *shapes,
    int count
) {
    OverlayUniforms uniforms;
    SDL_GPUColorTargetInfo targetInfo;
    SDL_GPURenderPass *pass;
    int i;

    SDL_assert(f_Pipeline != NULL && commands != NULL && target != NULL);
    SDL_assert(count >= 0 && count <= OVERLAY_MAX_SHAPES && (count == 0 || shapes != NULL));
    if (count == 0) {
        return;
    }

    SDL_zero(uniforms);
    uniforms.targetSize[0] = (float)width;
    uniforms.targetSize[1] = (float)height;
    for (i = 0; i < count; i++) {
        PackShape(&shapes[i], &uniforms.shapes[i]);
    }

    SDL_zero(targetInfo);
    targetInfo.texture = target;
    targetInfo.load_op = SDL_GPU_LOADOP_LOAD;
    targetInfo.store_op = SDL_GPU_STOREOP_STORE;
    SDL_PushGPUVertexUniformData(commands, 0, &uniforms, sizeof(uniforms));
    pass = SDL_BeginGPURenderPass(commands, &targetInfo, 1, NULL);
    SDL_BindGPUGraphicsPipeline(pass, f_Pipeline);
    SDL_DrawGPUPrimitives(pass, VERTICES_PER_SHAPE, (Uint32)count, 0, 0);
    SDL_EndGPURenderPass(pass);
}
