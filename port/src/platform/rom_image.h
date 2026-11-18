/*
 * The player's cartridge image: read from a file in any of the byte orders
 * that dumps come in, put in the N64's, and checked to be the release whose
 * offsets the port was built with.
 */

#ifndef PORT_PLATFORM_ROM_IMAGE_H
#define PORT_PLATFORM_ROM_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum RomByteOrder {
    ROM_ORDER_BIG, // .z64, the N64's own
    ROM_ORDER_HALFWORD_SWAPPED, // .v64, each 16-bit pair swapped
    ROM_ORDER_LITTLE, // .n64, each 32-bit word reversed
    ROM_ORDER_UNKNOWN
} RomByteOrder;

// The image's byte order, from the first word of its header
RomByteOrder DetectRomByteOrder(const uint8_t *rom, size_t size);

// Puts the image in the N64's byte order where it lies; false, leaving it
// alone, when its order is unknown or its size isn't whole words
bool ConvertRomToBigEndian(uint8_t *rom, size_t size);

// Whether an image in the N64's order is the US release
bool IsSupportedRom(const uint8_t *rom, size_t size);

// The CRC-32 of size bytes of an image, as zlib computes it
uint32_t ComputeRomChecksum(const uint8_t *rom, size_t size);

// Reads, converts and checks the image at path: the US release, unmodified.
// Gives it, to be released with SDL_free, or NULL with SDL_GetError saying
// why not.
uint8_t *LoadRomImage(const char *path, size_t *size);

// The same, but naming the image in what SDL_GetError says as name, which
// starts it, rather than by its path, which may mean nothing to the player
uint8_t *LoadNamedRomImage(const char *path, const char *name, size_t *size);

#endif
