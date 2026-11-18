/*
 * Fuzzes the reading of a cartridge image's byte order. Whatever the bytes,
 * an image is converted exactly when its first word names an order and it
 * is of whole words; a converted image holds the same bytes, each pair or
 * word put back in order, and begins in the N64's order; one that isn't
 * converted is left alone. No input is of the US release's size, so none
 * is supported, which also checks that the support check reads no header
 * past a short image's end.
 */

#include <SDL3/SDL.h>

#include "fuzz_target.h"
#include "rom_image.h"

// Where a converted image's byte came from in the dump
static size_t SourceOffset(RomByteOrder order, size_t offset) {
    if (order == ROM_ORDER_HALFWORD_SWAPPED) {
        return offset ^ 1;
    }

    if (order == ROM_ORDER_LITTLE) {
        return offset ^ 3;
    }

    return offset;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint8_t *image = SDL_malloc(size != 0 ? size : 1);
    const RomByteOrder order = DetectRomByteOrder(data, size);
    bool isConverted;
    size_t i;

    if (image == NULL) {
        return 0;
    }

    SDL_memcpy(image, data, size);
    isConverted = ConvertRomToBigEndian(image, size);
    FUZZ_CHECK(isConverted == (order != ROM_ORDER_UNKNOWN && size % 4 == 0));
    for (i = 0; i < size; i++) {
        FUZZ_CHECK(image[i] == data[isConverted ? SourceOffset(order, i) : i]);
    }

    FUZZ_CHECK(!isConverted || DetectRomByteOrder(image, size) == ROM_ORDER_BIG);
    FUZZ_CHECK(!IsSupportedRom(image, size));
    SDL_free(image);
    return 0;
}
