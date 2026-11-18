/*
 * Runs a fuzz target over the files named on its command line, as libFuzzer
 * does, for the builds without libFuzzer, so that each input in a target's
 * corpus is a test on every host.
 */

#include <SDL3/SDL.h>

#include "fuzz_target.h"

// Gives the target a copy of exactly the file's size, as libFuzzer does,
// so that the address sanitizer sees a read past its end
static bool RunInput(const char *path) {
    size_t size = 0;
    void *contents = SDL_LoadFile(path, &size);
    void *data;

    if (contents == NULL) {
        SDL_Log("Can't read %s: %s", path, SDL_GetError());
        return false;
    }

    data = SDL_malloc(size != 0 ? size : 1);
    if (data == NULL) {
        SDL_free(contents);
        return false;
    }

    SDL_memcpy(data, contents, size);
    SDL_free(contents);
    (void)LLVMFuzzerTestOneInput(data, size);
    SDL_free(data);
    return true;
}

int main(int argc, char **argv) {
    int i;

    if (argc < 2) {
        SDL_Log("Usage: %s INPUT...", argv[0]);
        return 1;
    }

    for (i = 1; i < argc; i++) {
        if (!RunInput(argv[i])) {
            return 1;
        }
    }

    SDL_Log("Ran %d inputs", argc - 1);
    return 0;
}
