/*
 * The audio output on an SDL audio stream, which converts the player's rate
 * to the device's and plays what is queued.
 */

#include <SDL3/SDL.h>

#include "audio_output.h"

#define STEREO 2
#define FRAME_SIZE (STEREO * (int)sizeof(float))

static SDL_AudioStream *f_Stream;

bool OpenAudioOutput(int sampleRate) {
    SDL_AudioSpec spec;

    SDL_assert(sampleRate > 0);
    SDL_assert(f_Stream == NULL);
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("No audio: %s", SDL_GetError());
        return false;
    }

    spec.format = SDL_AUDIO_F32;
    spec.channels = STEREO;
    spec.freq = sampleRate;
    f_Stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (f_Stream == NULL) {
        SDL_Log("No audio device: %s", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    SDL_ResumeAudioStreamDevice(f_Stream);
    return true;
}

void CloseAudioOutput(void) {
    if (f_Stream != NULL) {
        SDL_DestroyAudioStream(f_Stream);
        f_Stream = NULL;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

void QueueAudioOutput(const float *frames, int frameCount) {
    SDL_assert(frames != NULL || frameCount == 0);
    if (f_Stream != NULL && frameCount > 0) {
        SDL_PutAudioStreamData(f_Stream, frames, frameCount * FRAME_SIZE);
    }
}

int GetQueuedAudioFrames(void) {
    const int bytes = (f_Stream != NULL) ? SDL_GetAudioStreamQueued(f_Stream) : 0;

    return SDL_max(bytes, 0) / FRAME_SIZE;
}
