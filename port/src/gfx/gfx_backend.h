/*
 * What the renderer needs of a GPU. The SDL_GPU backend provides it; the
 * tests provide a double that keeps what it is given.
 *
 * Sizes are in the N64's pixels. The backend renders at its own scale and
 * scales back when it reads a target.
 */

#ifndef PORT_GFX_BACKEND_H
#define PORT_GFX_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

#include "frame.h"

GfxTarget *BackendCreateTarget(uint32_t width, uint32_t height, bool isDepth);
void BackendDestroyTarget(GfxTarget *target);

// Copies the top rows of one target to another of the same width, for a
// target that has grown
void BackendCopyTarget(GfxTarget *source, GfxTarget *destination, uint32_t width, uint32_t height);

// Uploads the frame's vertices, materials and textures, draws its passes,
// and submits the work, without waiting for it
void BackendExecuteFrame(const GfxFrame *frame);

// Reads a color target back at the N64's resolution, waiting for the GPU,
// as RGBA8 with red in the lowest byte
void BackendReadTarget(GfxTarget *target, uint32_t width, uint32_t height, uint32_t *rgba);

// Replaces rowCount rows of a color target's pixels, from firstRow down,
// with these. It goes ahead of the draws that wait for the next frame.
void BackendWriteTarget(
    GfxTarget *target,
    uint32_t width,
    uint32_t firstRow,
    uint32_t rowCount,
    const uint32_t *rgba
);

#endif
