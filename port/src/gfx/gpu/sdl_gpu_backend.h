/*
 * The renderer's backend on SDL_GPU: the Backend* functions of
 * gfx_backend.h, and what the presenter needs to show a target.
 */

#ifndef PORT_GFX_SDL_GPU_BACKEND_H
#define PORT_GFX_SDL_GPU_BACKEND_H

#include <SDL3/SDL_gpu.h>
#include <stdint.h>

#include "frame.h"

// Readies the backend to draw with the device, into targets scale times the
// N64's resolution each way. The device must outlive the backend.
void InitGpuBackend(SDL_GPUDevice *device, uint32_t scale);

// Waits for the GPU and releases everything that the backend created
void ShutdownGpuBackend(void);

uint32_t GpuBackendScale(void);

// The texture that holds a color target, scaled
SDL_GPUTexture *GpuTargetTexture(const GfxTarget *target);

#endif
