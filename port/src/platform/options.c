/*
 * The program's command line, which main.c describes.
 */

#include "options.h"

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

bool ParseOptions(int argc, char **argv, Options *options) {
    int i;

    SDL_zerop(options);
    for (i = 1; i < argc; i++) {
        const char *argument = argv[i];

        if (SDL_strcmp(argument, "--scale") == 0 && i + 1 < argc) {
            options->renderScale = SDL_atoi(argv[++i]);
            if (options->renderScale < 1 || options->renderScale > CONFIG_SCALE_MAX) {
                SDL_Log("The scale must be from 1 to %d", CONFIG_SCALE_MAX);
                return false;
            }
        }
        else if (SDL_strcmp(argument, "--save") == 0 && i + 1 < argc) {
            options->savePath = argv[++i];
        }
        else if (SDL_strcmp(argument, "--config") == 0 && i + 1 < argc) {
            options->configPath = argv[++i];
        }
        else if (SDL_strcmp(argument, "--frames") == 0 && i + 1 < argc) {
            options->frameLimit = SDL_strtol(argv[++i], NULL, 10);
            if (options->frameLimit < 1) {
                SDL_Log("The frame count must be at least 1");
                return false;
            }
        }
        else if (SDL_strcmp(argument, "--game-frames") == 0 && i + 1 < argc) {
            options->gameFrameLimit = SDL_strtol(argv[++i], NULL, 10);
            if (options->gameFrameLimit < 1) {
                SDL_Log("The game frame count must be at least 1");
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
