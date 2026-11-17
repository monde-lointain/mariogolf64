/*
 * The save memory's file. A store writes a new file beside the old and
 * then renames it into place, so that the save survives a crash or a full
 * disk in the middle of one.
 */

#include "save_memory.h"

#include <SDL3/SDL.h>

#include "port/nusys_host.h"

#define SAVE_ORGANIZATION "mariogolf64"
#define SAVE_APPLICATION "mariogolf64"
#define SAVE_FILE_NAME "mariogolf64.sra"
#define TEMPORARY_SUFFIX ".new"

static SDL_Mutex *f_Lock; // Saves come from the game's threads
static char *f_Path;

void SetSaveMemoryPath(const char *path) {
    if (f_Lock == NULL) {
        f_Lock = SDL_CreateMutex();
    }

    SDL_LockMutex(f_Lock);
    SDL_free(f_Path);
    f_Path = (path != NULL) ? SDL_strdup(path) : NULL;
    SDL_UnlockMutex(f_Lock);
}

char *FindDefaultSaveMemoryPath(void) {
    char *directory = SDL_GetPrefPath(SAVE_ORGANIZATION, SAVE_APPLICATION);
    char *path = NULL;

    if (directory == NULL) {
        SDL_Log("No directory for saves: %s", SDL_GetError());
        return NULL;
    }

    if (SDL_asprintf(&path, "%s%s", directory, SAVE_FILE_NAME) < 0) {
        path = NULL;
    }

    SDL_free(directory);
    return path;
}

// A save file of the wrong size came from elsewhere; the game's own checks
// would reject what it held, so it is ignored rather than half loaded
void PcLoadSaveMemory(u8 *image, u32 size) {
    size_t fileSize = 0;
    void *contents;

    SDL_assert(image != NULL);
    if (f_Lock == NULL) {
        return;
    }

    SDL_LockMutex(f_Lock);
    contents = (f_Path != NULL) ? SDL_LoadFile(f_Path, &fileSize) : NULL;
    if (contents != NULL && fileSize == size) {
        SDL_memcpy(image, contents, size);
    }
    else if (contents != NULL) {
        SDL_Log(
            "Ignoring %s: it holds %" SDL_PRIu64 " bytes, not %u",
            f_Path,
            (Uint64)fileSize,
            size
        );
    }

    SDL_UnlockMutex(f_Lock);
    SDL_free(contents);
}

bool ChecksumSaveMemory(uint32_t size, uint32_t *crc) {
    size_t fileSize = 0;
    void *contents = NULL;
    bool isFound;

    SDL_assert(crc != NULL);
    if (f_Lock == NULL) {
        return false;
    }

    SDL_LockMutex(f_Lock);
    if (f_Path != NULL) {
        contents = SDL_LoadFile(f_Path, &fileSize);
    }

    SDL_UnlockMutex(f_Lock);
    isFound = contents != NULL && fileSize == size;
    if (isFound) {
        *crc = SDL_crc32(0, contents, fileSize);
    }

    SDL_free(contents);
    return isFound;
}

void PcStoreSaveMemory(const u8 *image, u32 size) {
    char *temporary = NULL;

    SDL_assert(image != NULL);
    if (f_Lock == NULL) {
        return;
    }

    SDL_LockMutex(f_Lock);
    if (f_Path != NULL && SDL_asprintf(&temporary, "%s%s", f_Path, TEMPORARY_SUFFIX) >= 0) {
        if (!SDL_SaveFile(temporary, image, size) || !SDL_RenamePath(temporary, f_Path)) {
            SDL_Log("Can't save to %s: %s", f_Path, SDL_GetError());
            (void)SDL_RemovePath(temporary);
        }

        SDL_free(temporary);
    }

    SDL_UnlockMutex(f_Lock);
}
