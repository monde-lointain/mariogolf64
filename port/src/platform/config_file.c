/*
 * The settings file on disk.
 */

#include "config_file.h"

#include <SDL3/SDL.h>

#define CONFIG_ORGANIZATION "mariogolf64"
#define CONFIG_APPLICATION "mariogolf64"
#define CONFIG_FILE_NAME "config.ini"
#define TEMPORARY_SUFFIX ".new"

char *FindDefaultConfigPath(void) {
    char *directory = SDL_GetPrefPath(CONFIG_ORGANIZATION, CONFIG_APPLICATION);
    char *path = NULL;

    if (directory == NULL) {
        SDL_Log("No folder for the settings: %s", SDL_GetError());
        return NULL;
    }

    if (SDL_asprintf(&path, "%s%s", directory, CONFIG_FILE_NAME) < 0) {
        path = NULL;
    }

    SDL_free(directory);
    return path;
}

static void LogProblem(void *context, int line, const char *problem) {
    const char *path = context;

    if (line > 0) {
        SDL_Log("%s, line %d: %s", path, line, problem);
    }
    else {
        SDL_Log("%s: %s", path, problem);
    }
}

// Reads the file at path into config over what it holds, logging its
// problems and counting them in *problems. Returns its text, freed with
// SDL_free, or NULL when there's no file or it can't be read, the latter
// counting as one problem.
static char *ReadConfigText(const char *path, Config *config, int *problems, size_t *size) {
    char *text;

    *problems = 0;
    *size = 0;
    if (!SDL_GetPathInfo(path, NULL)) {
        return NULL;
    }

    text = SDL_LoadFile(path, size);
    if (text == NULL) {
        SDL_Log("Can't read the settings in %s: %s", path, SDL_GetError());
        *problems = 1;
        return NULL;
    }

    *problems = ParseConfig(text, *size, config, LogProblem, (void *)path);
    return text;
}

int LoadConfigFile(const char *path, Config *config) {
    char *text;
    size_t size;
    int problems;

    SDL_assert(path != NULL && config != NULL);
    SetDefaultConfig(config);
    text = ReadConfigText(path, config, &problems, &size);
    if (text == NULL && problems == 0 && StoreConfigFile(path, config)) {
        SDL_Log("Wrote the settings to %s", path);
    }

    SDL_free(text);
    return problems;
}

bool StoreConfigFile(const char *path, const Config *config) {
    char *text = FormatConfig(config);
    char *temporary = NULL;
    bool isStored = false;

    SDL_assert(path != NULL);
    if (text == NULL || SDL_asprintf(&temporary, "%s%s", path, TEMPORARY_SUFFIX) < 0) {
        SDL_Log("Can't store the settings: out of memory");
        SDL_free(text);
        return false;
    }

    isStored = SDL_SaveFile(temporary, text, SDL_strlen(text)) && SDL_RenamePath(temporary, path);
    if (!isStored) {
        SDL_Log("Can't store the settings in %s: %s", path, SDL_GetError());
        (void)SDL_RemovePath(temporary);
    }

    SDL_free(temporary);
    SDL_free(text);
    return isStored;
}

static bool IsSameWindow(const Config *a, const Config *b) {
    return a->isFullscreen == b->isFullscreen &&
        a->windowWidth == b->windowWidth &&
        a->windowHeight == b->windowHeight &&
        a->hasWindowPosition == b->hasWindowPosition &&
        a->windowX == b->windowX &&
        a->windowY == b->windowY;
}

bool StoreWindowSettings(const char *path, const Config *window) {
    static Config config;
    char *text;
    size_t size;
    int problems;
    bool isFound;

    SDL_assert(path != NULL && window != NULL);
    SetDefaultConfig(&config);
    text = ReadConfigText(path, &config, &problems, &size);
    isFound = (text != NULL);
    SDL_free(text);
    if (problems > 0) {
        SDL_Log("Not remembering the window in %s, as it has problems", path);
        return false;
    }

    if (isFound && IsSameWindow(&config, window)) {
        return true;
    }

    config.isFullscreen = window->isFullscreen;
    config.windowWidth = window->windowWidth;
    config.windowHeight = window->windowHeight;
    config.hasWindowPosition = window->hasWindowPosition;
    config.windowX = window->windowX;
    config.windowY = window->windowY;
    return StoreConfigFile(path, &config);
}
