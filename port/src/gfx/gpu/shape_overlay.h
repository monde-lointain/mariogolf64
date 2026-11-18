/*
 * The shape overlay: shapes drawn over a frame that is about to be
 * presented, such as the touch controls, all in one draw.
 */

#ifndef PORT_GFX_SHAPE_OVERLAY_H
#define PORT_GFX_SHAPE_OVERLAY_H

#include <SDL3/SDL_gpu.h>
#include <stdbool.h>
#include <stdint.h>

#include "port/overlay_shape.h"

// Readies the overlay to draw on the device into targets of the format;
// false, with an error logged, when it can't
bool InitShapeOverlay(SDL_GPUDevice *device, SDL_GPUTextureFormat format);
void ShutdownShapeOverlay(void);

// Draws up to OVERLAY_MAX_SHAPES shapes, in their order, over what the
// target holds
void DrawShapeOverlay(
    SDL_GPUCommandBuffer *commands,
    SDL_GPUTexture *target,
    uint32_t width,
    uint32_t height,
    const OverlayShape *shapes,
    int count
);

#endif
