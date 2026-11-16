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

uint8_t *LoadRomImage(const char *path, size_t *size) {
    uint8_t *rom;

    SDL_assert(path != NULL && size != NULL);
    rom = SDL_LoadFile(path, size);
    if (rom == NULL) {
        SDL_Log("Can't read the ROM %s: %s", path, SDL_GetError());
        return NULL;
    }

    if (*size < HEADER_SIZE || !ConvertRomToBigEndian(rom, *size)) {
        SDL_Log("%s isn't an N64 ROM image", path);
        SDL_free(rom);
        return NULL;
    }

    if (!IsSupportedRom(rom, *size)) {
        SDL_Log(
            "%s isn't the US release of Mario Golf (code %s), which the port needs",
            path,
            SUPPORTED_GAME_CODE
        );
        SDL_free(rom);
        return NULL;
    }

    return rom;
}
