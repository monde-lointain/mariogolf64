/*
 * Reading the cartridge image. Every N64 image begins with the same word,
 * the PI's settings, so its bytes say which order the dump is in.
 */

#include "rom_image.h"

#include <SDL3/SDL.h>

#define HEADER_SIZE 0x40
#define GAME_CODE_OFFSET 0x3B
#define GAME_CODE_SIZE 4
// The US release's code: N64 cartridge, Mario Golf, North America
#define SUPPORTED_GAME_CODE "NMFE"
#define SUPPORTED_SIZE 0x2000000u
// The CRC-32 (zlib's) of the US release in the N64's byte order, whose
// SHA-1 mariogolf64.sha1 gives. The port reads the files at the offsets of
// this image alone, so a revision or a modified dump would give it others'
// data.
#define SUPPORTED_CRC32 0x2FC4C216u
#define SUPPORTED_SHA1 "e2c4e7a905b29529b49a1619a401fe699224829b"
// Where the copy that the player chose is kept: beside the save
#define STORE_ORGANIZATION "mariogolf64"
#define STORE_APPLICATION "mariogolf64"
#define STORED_ROM_NAME "baserom.us.z64"
#define TEMPORARY_SUFFIX ".new"

static const uint8_t f_BigEndianMagic[] = {0x80, 0x37, 0x12, 0x40};
static const uint8_t f_HalfwordSwappedMagic[] = {0x37, 0x80, 0x40, 0x12};
static const uint8_t f_LittleEndianMagic[] = {0x40, 0x12, 0x37, 0x80};

RomByteOrder DetectRomByteOrder(const uint8_t *rom, size_t size) {
    if (rom == NULL || size < sizeof(f_BigEndianMagic)) {
        return ROM_ORDER_UNKNOWN;
    }

    if (SDL_memcmp(rom, f_BigEndianMagic, sizeof(f_BigEndianMagic)) == 0) {
        return ROM_ORDER_BIG;
    }

    if (SDL_memcmp(rom, f_HalfwordSwappedMagic, sizeof(f_HalfwordSwappedMagic)) == 0) {
        return ROM_ORDER_HALFWORD_SWAPPED;
    }

    if (SDL_memcmp(rom, f_LittleEndianMagic, sizeof(f_LittleEndianMagic)) == 0) {
        return ROM_ORDER_LITTLE;
    }

    return ROM_ORDER_UNKNOWN;
}

static void SwapHalfwordBytes(uint8_t *rom, size_t size) {
    size_t i;

    for (i = 0; i + 1 < size; i += 2) {
        const uint8_t first = rom[i];

        rom[i] = rom[i + 1];
        rom[i + 1] = first;
    }
}

static void ReverseWordBytes(uint8_t *rom, size_t size) {
    size_t i;

    for (i = 0; i + 3 < size; i += 4) {
        uint32_t word;

        SDL_memcpy(&word, rom + i, sizeof(word));
        word = SDL_Swap32(word);
        SDL_memcpy(rom + i, &word, sizeof(word));
    }
}

bool ConvertRomToBigEndian(uint8_t *rom, size_t size) {
    const RomByteOrder order = DetectRomByteOrder(rom, size);

    if (order == ROM_ORDER_UNKNOWN || size % 4 != 0) {
        return false;
    }

    if (order == ROM_ORDER_HALFWORD_SWAPPED) {
        SwapHalfwordBytes(rom, size);
    }
    else if (order == ROM_ORDER_LITTLE) {
        ReverseWordBytes(rom, size);
    }

    return true;
}

bool IsSupportedRom(const uint8_t *rom, size_t size) {
    return rom != NULL &&
        size == SUPPORTED_SIZE &&
        DetectRomByteOrder(rom, size) == ROM_ORDER_BIG &&
        SDL_memcmp(rom + GAME_CODE_OFFSET, SUPPORTED_GAME_CODE, GAME_CODE_SIZE) == 0;
}

uint32_t ComputeRomChecksum(const uint8_t *rom, size_t size) {
    SDL_assert(rom != NULL || size == 0);
    return SDL_crc32(0, rom, size);
}

// Releases the image and gives NULL, leaving SDL's error as the problem
static uint8_t *RejectRomImage(uint8_t *rom) {
    SDL_free(rom);
    return NULL;
}

uint8_t *LoadNamedRomImage(const char *path, const char *name, size_t *size) {
    uint8_t *rom;
    uint32_t checksum;

    SDL_assert(path != NULL && name != NULL && size != NULL);
    rom = SDL_LoadFile(path, size);
    if (rom == NULL) {
        SDL_SetError("%s can't be read: %s", name, SDL_GetError());
        return NULL;
    }

    if (*size < HEADER_SIZE || !ConvertRomToBigEndian(rom, *size)) {
        SDL_SetError("%s isn't an N64 ROM image", name);
        return RejectRomImage(rom);
    }

    if (!IsSupportedRom(rom, *size)) {
        SDL_SetError(
            "%s isn't the US release of Mario Golf (game code %s), which the port needs",
            name,
            SUPPORTED_GAME_CODE
        );
        return RejectRomImage(rom);
    }

    checksum = ComputeRomChecksum(rom, *size);
    if (checksum != SUPPORTED_CRC32) {
        SDL_SetError(
            "%s is a revision or a modified dump of Mario Golf's US release: its CRC-32 is "
            "%08" SDL_PRIX32 ", where the port needs the unmodified release's, %08X (SHA-1 %s)",
            name,
            checksum,
            SUPPORTED_CRC32,
            SUPPORTED_SHA1
        );
        return RejectRomImage(rom);
    }

    return rom;
}

uint8_t *LoadRomImage(const char *path, size_t *size) {
    return LoadNamedRomImage(path, path, size);
}

int FindRomPlace(const char *const *places, int count) {
    SDL_PathInfo info;
    int i;

    SDL_assert(places != NULL || count == 0);
    for (i = 0; i < count; i++) {
        if (places[i] != NULL &&
            SDL_GetPathInfo(places[i], &info) &&
            info.type == SDL_PATHTYPE_FILE) {
            return i;
        }
    }

    return ROM_PLACE_NONE;
}

char *FindStoredRomPath(void) {
    char *directory = SDL_GetPrefPath(STORE_ORGANIZATION, STORE_APPLICATION);
    char *path = NULL;

    if (directory == NULL) {
        return NULL;
    }

    if (SDL_asprintf(&path, "%s%s", directory, STORED_ROM_NAME) < 0) {
        path = NULL;
    }

    SDL_free(directory);
    return path;
}

bool StoreRomImage(const char *path, const uint8_t *rom, size_t size) {
    char *temporary = NULL;
    bool isStored;

    SDL_assert(path != NULL && rom != NULL);
    if (SDL_asprintf(&temporary, "%s%s", path, TEMPORARY_SUFFIX) < 0) {
        return false;
    }

    isStored = SDL_SaveFile(temporary, rom, size) && SDL_RenamePath(temporary, path);
    if (!isStored) {
        // Removing a file that isn't there sets SDL's error on some systems
        char *reason = SDL_strdup(SDL_GetError());

        (void)SDL_RemovePath(temporary);
        SDL_SetError(
            "Can't keep a copy of the ROM in %s: %s",
            path,
            reason != NULL ? reason : "out of memory"
        );
        SDL_free(reason);
    }

    SDL_free(temporary);
    return isStored;
}
