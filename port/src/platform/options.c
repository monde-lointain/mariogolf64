/*
 * The program's command line, which main.c describes.
 */

#include "options.h"

// The largest count of frames: the largest long where it has 32 bits, as
// on Windows, so that every host takes the same counts
#define FRAME_COUNT_MAX 2147483647L

#include <SDL3/SDL.h>

#include "config.h"

static void PrintUsage(const char *program) {
    SDL_Log(
        "Usage: %s [--scale N] [--fullscreen] [--save PATH] [--config PATH] [--gpu-debug] "
        "[--headless] "
        "[--frames N] [--game-frames N] [--screenshot PATH] [--input-script PATH] "
        "[--record PATH] [ROM]",
        program
    );
}

// Reads the whole word as a number from low to high. It reads a long long,
// 64 bits on every host, so that a number past a 32-bit long is refused,
// not taken as the largest.
static bool ReadNumber(const char *word, long low, long high, long *value) {
    char *end;
    long long number = SDL_strtoll(word, &end, 10);

    if (end == word || *end != '\0' || number < low || number > high) {
        return false;
    }

    *value = (long)number;
    return true;
}

bool ParseOptions(int argc, char **argv, Options *options) {
    int i;
    long number;

    SDL_zerop(options);
    for (i = 1; i < argc; i++) {
        const char *argument = argv[i];

        if (SDL_strcmp(argument, "--scale") == 0 && i + 1 < argc) {
            if (!ReadNumber(argv[++i], 1, CONFIG_SCALE_MAX, &number)) {
                SDL_Log("The scale must be from 1 to %d", CONFIG_SCALE_MAX);
                return false;
            }
            options->renderScale = (int)number;
        }
        else if (SDL_strcmp(argument, "--save") == 0 && i + 1 < argc) {
            options->savePath = argv[++i];
        }
        else if (SDL_strcmp(argument, "--config") == 0 && i + 1 < argc) {
            options->configPath = argv[++i];
        }
        else if (SDL_strcmp(argument, "--frames") == 0 && i + 1 < argc) {
            if (!ReadNumber(argv[++i], 1, FRAME_COUNT_MAX, &options->frameLimit)) {
                SDL_Log("The frame count must be from 1 to %ld", FRAME_COUNT_MAX);
                return false;
            }
        }
        else if (SDL_strcmp(argument, "--game-frames") == 0 && i + 1 < argc) {
            if (!ReadNumber(argv[++i], 1, FRAME_COUNT_MAX, &options->gameFrameLimit)) {
                SDL_Log("The game frame count must be from 1 to %ld", FRAME_COUNT_MAX);
                return false;
            }
        }
        else if (SDL_strcmp(argument, "--screenshot") == 0 && i + 1 < argc) {
            options->screenshotPath = argv[++i];
        }
        else if (SDL_strcmp(argument, "--input-script") == 0 && i + 1 < argc) {
            options->inputScriptPath = argv[++i];
        }
        else if (SDL_strcmp(argument, "--record") == 0 && i + 1 < argc) {
            options->recordingPath = argv[++i];
        }
        else if (SDL_strcmp(argument, "--headless") == 0) {
            options->isHeadless = true;
        }
        else if (SDL_strcmp(argument, "--fullscreen") == 0) {
            options->isFullscreen = true;
        }
        else if (SDL_strcmp(argument, "--gpu-debug") == 0) {
            options->isGpuDebug = true;
        }
        else if (argument[0] != '-' && options->romPath == NULL) {
            options->romPath = argument;
        }
        else {
            PrintUsage(argv[0]);
            return false;
        }
    }

    return true;
}

bool HasRunReachedLimit(const Options *options, long frames, long gameFrames) {
    bool isLimited;

    SDL_assert(options != NULL && frames >= 0 && gameFrames >= 0);
    isLimited = options->frameLimit != 0 || options->gameFrameLimit != 0;
    return !isLimited ||
        (options->frameLimit != 0 && frames >= options->frameLimit) ||
        (options->gameFrameLimit != 0 && gameFrames >= options->gameFrameLimit);
}
