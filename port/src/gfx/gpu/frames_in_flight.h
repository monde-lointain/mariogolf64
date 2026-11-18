/*
 * Frames in flight: the fences of the frames that the GPU may still be
 * drawing, oldest first. SDL keeps each submitted frame's command buffer,
 * descriptor sets and cycled transfer buffers until the GPU finishes it,
 * so a CPU that never waits, as in a headless run on a slow GPU, makes
 * them pile up without bound.
 */

#ifndef GFX_GPU_FRAMES_IN_FLIGHT_H
#define GFX_GPU_FRAMES_IN_FLIGHT_H

#include <SDL3/SDL_gpu.h>

// How many frames the CPU may submit before it waits for the oldest: two,
// so that it records one while the GPU draws the other
#define FRAMES_IN_FLIGHT 2

// Starts empty when zeroed
typedef struct FramesInFlight {
    SDL_GPUFence *fences[FRAMES_IN_FLIGHT];
    unsigned oldest; // Index of the oldest fence in fences
    unsigned count;
} FramesInFlight;

// Adds a submitted frame's fence. Returns the oldest fence when there
// would otherwise be more than FRAMES_IN_FLIGHT, which the caller must
// wait for and release; otherwise NULL.
SDL_GPUFence *AddFrameInFlight(FramesInFlight *frames, SDL_GPUFence *fence);

// Removes and returns the oldest fence, or NULL when there is none
SDL_GPUFence *TakeOldestFrameInFlight(FramesInFlight *frames);

#endif
