/*
 * Recording a frame's draws for the GPU.
 */

#include "frame.h"

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_stdinc.h>

#include "port/os_host.h"

_Static_assert(sizeof(GfxVertex) == 32, "GfxVertex must match the shader's vertex input");
_Static_assert(sizeof(GfxTileSampler) == 48, "GfxTileSampler must match the shader's std430");
_Static_assert(sizeof(GfxMaterial) == 208, "GfxMaterial must match the shader's std430");

static void *AllocateArray(uint32_t count, size_t elementSize) {
    void *array = SDL_calloc(count, elementSize);

    if (array == NULL) {
        PcFatalError("Out of memory for the renderer's frame");
    }

    return array;
}

void InitFrame(GfxFrame *frame) {
    SDL_zerop(frame);
    frame->vertices = AllocateArray(FRAME_VERTEX_CAPACITY, sizeof(GfxVertex));
    frame->materials = AllocateArray(FRAME_MATERIAL_CAPACITY, sizeof(GfxMaterial));
    frame->draws = AllocateArray(FRAME_DRAW_CAPACITY, sizeof(GfxDraw));
    frame->passes = AllocateArray(FRAME_PASS_CAPACITY, sizeof(GfxPass));
    frame->uploads = AllocateArray(FRAME_UPLOAD_CAPACITY, sizeof(GfxTextureUpload));
    frame->texels = AllocateArray(FRAME_TEXEL_CAPACITY, sizeof(uint32_t));
    frame->copies = AllocateArray(FRAME_COPY_CAPACITY, sizeof(GfxCopy));
}

void ResetFrame(GfxFrame *frame) {
    frame->vertexCount = 0;
    frame->materialCount = 0;
    frame->drawCount = 0;
    frame->passCount = 0;
    frame->uploadCount = 0;
    frame->texelCount = 0;
    frame->copyCount = 0;
}

static GfxPass *CurrentPass(GfxFrame *frame) {
    return frame->passCount != 0 ? &frame->passes[frame->passCount - 1] : NULL;
}

static GfxPass *AppendPass(GfxFrame *frame, GfxTarget *target, GfxTarget *depth) {
    GfxPass *pass;

    SDL_assert(frame->passCount < FRAME_PASS_CAPACITY);
    pass = &frame->passes[frame->passCount++];
    SDL_zerop(pass);
    pass->target = target;
    pass->depth = depth;
    pass->firstDraw = frame->drawCount;
    pass->firstCopy = frame->copyCount;
    return pass;
}

void BeginFramePass(GfxFrame *frame, GfxTarget *target) {
    GfxPass *pass = CurrentPass(frame);

    SDL_assert(target != NULL);
    if (pass == NULL || pass->target != target) {
        AppendPass(frame, target, NULL);
    }
}

void UseFrameDepth(GfxFrame *frame, GfxTarget *depth, float clearValue) {
    GfxPass *pass = CurrentPass(frame);
    bool isClearing = clearValue >= 0.0f;

    SDL_assert(pass != NULL && depth != NULL);
    if (pass->depth != NULL && pass->depth != depth) {
        pass = AppendPass(frame, pass->target, depth);
    }
    else if (isClearing && pass->drawCount != 0) {
        pass = AppendPass(frame, pass->target, depth);
    }

    pass->depth = depth;
    if (isClearing) {
        pass->clearsDepth = true;
        pass->depthClearValue = clearValue;
    }
}

bool HasFrameRoom(const GfxFrame *frame, uint32_t vertexCount, uint32_t texelCount) {
    return frame->vertexCount + vertexCount <= FRAME_VERTEX_CAPACITY &&
        frame->texelCount + texelCount <= FRAME_TEXEL_CAPACITY &&
        frame->materialCount < FRAME_MATERIAL_CAPACITY &&
        frame->drawCount < FRAME_DRAW_CAPACITY &&
        frame->passCount < FRAME_PASS_CAPACITY &&
        frame->uploadCount < FRAME_UPLOAD_CAPACITY &&
        frame->copyCount < FRAME_COPY_CAPACITY;
}

uint32_t AddFrameMaterial(GfxFrame *frame, const GfxMaterial *material) {
    if (frame->materialCount != 0 &&
        SDL_memcmp(&frame->materials[frame->materialCount - 1], material, sizeof(*material)) == 0) {
        return frame->materialCount - 1;
    }

    SDL_assert(frame->materialCount < FRAME_MATERIAL_CAPACITY);
    frame->materials[frame->materialCount] = *material;
    return frame->materialCount++;
}

static bool IsSameRect(const GfxRect *first, const GfxRect *second) {
    return first->left == second->left &&
        first->top == second->top &&
        first->right == second->right &&
        first->bottom == second->bottom;
}

bool DoesCopyCarryOn(const GfxCopy *copy, const GfxCopy *next) {
    return next->source == copy->source &&
        next->sourceX == copy->sourceX &&
        next->x == copy->x &&
        next->width == copy->width &&
        next->sourceY == copy->sourceY + copy->height &&
        next->y == copy->y + copy->height;
}

void AddFrameCopy(GfxFrame *frame, GfxTarget *target, const GfxCopy *copy) {
    GfxPass *pass = CurrentPass(frame);

    SDL_assert(target != NULL && copy->source != NULL && copy->source != target);
    if (pass == NULL || pass->target != target || pass->drawCount != 0) {
        pass = AppendPass(frame, target, NULL);
    }

    if (pass->copyCount != 0) {
        GfxCopy *last = &frame->copies[frame->copyCount - 1];

        if (DoesCopyCarryOn(last, copy)) {
            last->height = (uint16_t)(last->height + copy->height);
            return;
        }
    }

    SDL_assert(frame->copyCount < FRAME_COPY_CAPACITY);
    frame->copies[frame->copyCount++] = *copy;
    pass->copyCount++;
}

// Appends vertices to the current pass, extending its last draw when the
// key, scissor and source are the same
static GfxVertex *AddDrawVertices(
    GfxFrame *frame,
    uint32_t vertexCount,
    uint16_t pipelineKey,
    const GfxRect *scissor,
    GfxTarget *source
) {
    GfxPass *pass = CurrentPass(frame);
    GfxDraw *draw = NULL;
    GfxVertex *vertices;

    SDL_assert(pass != NULL);
    SDL_assert(frame->vertexCount + vertexCount <= FRAME_VERTEX_CAPACITY);
    if (pass->drawCount != 0) {
        draw = &frame->draws[frame->drawCount - 1];
        if (draw->pipelineKey != pipelineKey ||
            !IsSameRect(&draw->scissor, scissor) ||
            draw->source != source) {
            draw = NULL;
        }
    }

    if (draw == NULL) {
        SDL_assert(frame->drawCount < FRAME_DRAW_CAPACITY);
        draw = &frame->draws[frame->drawCount++];
        draw->firstVertex = frame->vertexCount;
        draw->vertexCount = 0;
        draw->pipelineKey = pipelineKey;
        draw->scissor = *scissor;
        draw->source = source;
        pass->drawCount++;
    }

    vertices = &frame->vertices[frame->vertexCount];
    draw->vertexCount += vertexCount;
    frame->vertexCount += vertexCount;
    return vertices;
}

GfxVertex *AddFrameVertices(
    GfxFrame *frame,
    uint32_t vertexCount,
    uint16_t pipelineKey,
    const GfxRect *scissor
) {
    return AddDrawVertices(frame, vertexCount, pipelineKey, scissor, NULL);
}

GfxVertex *AddFrameDepthCopyVertices(
    GfxFrame *frame,
    GfxTarget *source,
    uint32_t vertexCount,
    const GfxRect *scissor
) {
    const GfxPass *pass = CurrentPass(frame);

    SDL_assert(source != NULL && pass != NULL && pass->depth != NULL && pass->depth != source);
    return AddDrawVertices(frame, vertexCount, DEPTH_COPY_KEY, scissor, source);
}

uint32_t *AddFrameTextureUpload(
    GfxFrame *frame,
    uint16_t layer,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height
) {
    GfxTextureUpload *upload;
    uint32_t texelCount = (uint32_t)width * height;
    uint32_t *texels;

    SDL_assert(frame->uploadCount < FRAME_UPLOAD_CAPACITY);
    SDL_assert(frame->texelCount + texelCount <= FRAME_TEXEL_CAPACITY);
    upload = &frame->uploads[frame->uploadCount++];
    upload->texelOffset = frame->texelCount;
    upload->x = x;
    upload->y = y;
    upload->layer = layer;
    upload->width = width;
    upload->height = height;
    texels = &frame->texels[frame->texelCount];
    frame->texelCount += texelCount;
    return texels;
}
