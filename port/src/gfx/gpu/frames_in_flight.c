/*
 * Frames in flight: the fences of the frames that the GPU may still be
 * drawing, oldest first.
 */

#include "frames_in_flight.h"

#include <SDL3/SDL_assert.h>

SDL_GPUFence *AddFrameInFlight(FramesInFlight *frames, SDL_GPUFence *fence) {
    SDL_GPUFence *oldest = NULL;

    SDL_assert(fence != NULL);
    if (frames->count == FRAMES_IN_FLIGHT) {
        oldest = TakeOldestFrameInFlight(frames);
    }

    frames->fences[(frames->oldest + frames->count) % FRAMES_IN_FLIGHT] = fence;
    frames->count++;
    return oldest;
}

SDL_GPUFence *TakeOldestFrameInFlight(FramesInFlight *frames) {
    SDL_GPUFence *oldest;

    if (frames->count == 0) {
        return NULL;
    }

    oldest = frames->fences[frames->oldest];
    frames->oldest = (frames->oldest + 1) % FRAMES_IN_FLIGHT;
    frames->count--;
    return oldest;
}
