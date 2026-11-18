/*
 * The renderer's backend on SDL_GPU: the Backend* functions of
 * gfx_backend.h, and what the presenter needs to show a target.
 */

#ifndef PORT_GFX_SDL_GPU_BACKEND_H
#define PORT_GFX_SDL_GPU_BACKEND_H

#include <SDL3/SDL_gpu.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "frame.h"

// Creates a device that runs the backend's and the presenter's shaders,
// with SDL's checks of its use when isDebug. Gives NULL, with SDL_GetError
// saying why, where there's none.
SDL_GPUDevice *CreateGpuDevice(bool isDebug);

// Readies the backend to draw with the device, into targets scale times the
// N64's resolution each way. The device must outlive the backend.
void InitGpuBackend(SDL_GPUDevice *device, uint32_t scale);

// Waits for the GPU and releases everything that the backend created
void ShutdownGpuBackend(void);

// Creates a shader from code in both of the forms that the build makes,
// SPIR-V and MSL, giving the form that the device takes. Stops the program
// when it can't, as the shaders are the program's own.
SDL_GPUShader *CreateGpuShader(
    SDL_GPUDevice *device,
    SDL_GPUShaderStage stage,
    const unsigned char *spirv,
    size_t spirvSize,
    const unsigned char *msl,
    size_t mslSize,
    uint32_t samplerCount,
    uint32_t storageBufferCount,
    uint32_t uniformBufferCount
);

uint32_t GpuBackendScale(void);

// The texture that holds a color target, scaled
SDL_GPUTexture *GpuTargetTexture(const GfxTarget *target);

#endif
