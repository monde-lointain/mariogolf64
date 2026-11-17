/*
 * The RDP's texture memory, its loads and the decoding of its tiles.
 */

#include "tmem.h"

#include <PR/gbi.h>
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_endian.h>
#include <SDL3/SDL_stdinc.h>
#include <stdbool.h>

#include "hash.h"

#define TMEM_ADDRESS_MASK (TMEM_SIZE - 1)
// The half of TMEM that each half of a 32-bit texel goes to
#define TMEM_HALF_SIZE (TMEM_SIZE / 2)
#define TMEM_HALF_MASK (TMEM_HALF_SIZE - 1)
// The 32-bit halves of the 64-bit words of odd rows trade places
#define ODD_ROW_SWAP 4
// The bytes in each half of a word, which the odd rows' swap exchanges
#define HALF_WORD_SIZE 4u
// LoadBlock's dxt counts rows in 1.11 fixed point
#define DXT_FRACTION_BITS 11
// A palette entry fills a 64-bit word of the palette half, four copies wide
#define PALETTE_ENTRY_STRIDE TMEM_WORD_SIZE
#define SMALL_PALETTE_SIZE 16
#define LARGE_PALETTE_SIZE 256
// The largest texture that a tile can describe, beyond any that TMEM holds
#define TILE_TEXTURE_SIDE_MAX 1024

uint32_t TexelSizeInHalfBytes(int size) {
    SDL_assert(size >= G_IM_SIZ_4b && size <= G_IM_SIZ_32b);
    return 1u << size;
}

// Bytes for count texels, rounded up to whole bytes
static uint32_t BytesForTexels(int size, uint32_t count) {
    return ((count * TexelSizeInHalfBytes(size)) + 1) / 2;
}

// Copies bytes into TMEM half a word at a time. Tiles address TMEM in whole
// words, and the swap and the wrap at TMEM's end move whole halves, so each
// half's bytes stay side by side.
static void StoreLoadedBytes(
    Tmem *tmem,
    uint32_t destination,
    const uint8_t *source,
    uint32_t byteCount,
    uint32_t swap,
    bool isHostOrder
) {
    uint32_t i;

    SDL_assert(destination % TMEM_WORD_SIZE == 0);
    for (i = 0; i < byteCount; i += HALF_WORD_SIZE) {
        uint32_t address = ((destination + i) ^ swap) & TMEM_ADDRESS_MASK;

        SDL_memcpy(&tmem->bytes[address], &source[i], SDL_min(HALF_WORD_SIZE, byteCount - i));
        tmem->isHostOrderWord[address / TMEM_WORD_SIZE] = isHostOrder;
    }
}

// Splits 32-bit texels between TMEM's halves: red and green in the low half,
// blue and alpha in the high half, two bytes per texel in each
static void StoreSplitTexels(
    Tmem *tmem,
    uint32_t destination,
    const uint8_t *source,
    uint32_t texelCount,
    uint32_t swap,
    bool isHostOrder
) {
    // The order of the texel's bytes from red to alpha
    uint32_t order = isHostOrder && SDL_BYTEORDER == SDL_LIL_ENDIAN ? 3 : 0;
    uint32_t i;

    for (i = 0; i < texelCount; i++) {
        uint32_t offset = destination + (i * 2);
        uint32_t low = (offset ^ swap) & TMEM_HALF_MASK;
        uint32_t high = low + TMEM_HALF_SIZE;
        const uint8_t *texel = &source[i * 4];

        tmem->bytes[low] = texel[0 ^ order];
        tmem->bytes[low + 1] = texel[1 ^ order];
        tmem->bytes[high] = texel[2 ^ order];
        tmem->bytes[high + 1] = texel[3 ^ order];
        tmem->isHostOrderWord[low / TMEM_WORD_SIZE] = false;
        tmem->isHostOrderWord[high / TMEM_WORD_SIZE] = false;
    }
}

static const uint8_t *ImageTexelAddress(const TextureImage *image, uint32_t s, uint32_t t) {
    uint32_t texelIndex = (t * image->width) + s;

    return image->address + ((texelIndex * TexelSizeInHalfBytes(image->size)) / 2);
}

static void SetTileCorners(
    RdpTile *tile,
    uint32_t upperLeftS,
    uint32_t upperLeftT,
    uint32_t lowerRightS,
    uint32_t lowerRightT
) {
    tile->upperLeftS = (uint16_t)upperLeftS;
    tile->upperLeftT = (uint16_t)upperLeftT;
    tile->lowerRightS = (uint16_t)lowerRightS;
    tile->lowerRightT = (uint16_t)lowerRightT;
}

void LoadTmemBlock(
    Tmem *tmem,
    const TextureImage *image,
    int tileIndex,
    uint32_t upperLeftS,
    uint32_t upperLeftT,
    uint32_t lastTexel,
    uint32_t dxt
) {
    RdpTile *tile = &tmem->tiles[tileIndex];
    const uint8_t *source = ImageTexelAddress(image, upperLeftS, upperLeftT);
    uint32_t texelCount = lastTexel - upperLeftS + 1;
    uint32_t destination = tile->tmemAddress;
    uint32_t rowCounter = 0;
    uint32_t wordCount;
    uint32_t word;

    SDL_assert(tileIndex >= 0 && tileIndex < RDP_TILE_COUNT && lastTexel >= upperLeftS);
    SetTileCorners(tile, upperLeftS, upperLeftT, lastTexel, dxt);
    if (image->size == G_IM_SIZ_32b) {
        // Two texels to a word in each half of TMEM
        wordCount = (texelCount + 1) / 2;
        for (word = 0; word < wordCount; word++) {
            uint32_t swap = ((rowCounter >> DXT_FRACTION_BITS) & 1) ? ODD_ROW_SWAP : 0;
            uint32_t count = SDL_min(2u, texelCount - (word * 2));

            StoreSplitTexels(
                tmem,
                destination + (word * 4),
                source + (word * 8),
                count,
                swap,
                image->isHostOrder
            );
            rowCounter += dxt;
        }
        return;
    }

    wordCount = (BytesForTexels(image->size, texelCount) + TMEM_WORD_SIZE - 1) / TMEM_WORD_SIZE;
    for (word = 0; word < wordCount; word++) {
        uint32_t swap = ((rowCounter >> DXT_FRACTION_BITS) & 1) ? ODD_ROW_SWAP : 0;
        uint32_t bytesLeft = BytesForTexels(image->size, texelCount) - (word * TMEM_WORD_SIZE);

        StoreLoadedBytes(
            tmem,
            destination + (word * TMEM_WORD_SIZE),
            source + (word * TMEM_WORD_SIZE),
            SDL_min((uint32_t)TMEM_WORD_SIZE, bytesLeft),
            swap,
            image->isHostOrder
        );
        rowCounter += dxt;
    }
}

void LoadTmemTile(
    Tmem *tmem,
    const TextureImage *image,
    int tileIndex,
    uint32_t upperLeftS,
    uint32_t upperLeftT,
    uint32_t lowerRightS,
    uint32_t lowerRightT
) {
    RdpTile *tile = &tmem->tiles[tileIndex];
    uint32_t firstColumn = upperLeftS >> 2;
    uint32_t firstRow = upperLeftT >> 2;
    uint32_t texelsPerRow = (lowerRightS >> 2) - firstColumn + 1;
    uint32_t rowCount = (lowerRightT >> 2) - firstRow + 1;
    uint32_t row;

    SDL_assert(tileIndex >= 0 && tileIndex < RDP_TILE_COUNT);
    SDL_assert(lowerRightS >= upperLeftS && lowerRightT >= upperLeftT);
    SetTileCorners(tile, upperLeftS, upperLeftT, lowerRightS, lowerRightT);
    for (row = 0; row < rowCount; row++) {
        const uint8_t *source = ImageTexelAddress(image, firstColumn, firstRow + row);
        uint32_t destination = tile->tmemAddress + (row * tile->lineBytes);
        uint32_t swap = (row & 1) ? ODD_ROW_SWAP : 0;

        if (image->size == G_IM_SIZ_32b) {
            StoreSplitTexels(tmem, destination, source, texelsPerRow, swap, image->isHostOrder);
        }
        else {
            StoreLoadedBytes(
                tmem,
                destination,
                source,
                BytesForTexels(image->size, texelsPerRow),
                swap,
                image->isHostOrder
            );
        }
    }
}

void LoadTmemPalette(
    Tmem *tmem,
    const TextureImage *image,
    int tileIndex,
    uint32_t upperLeftS,
    uint32_t upperLeftT,
    uint32_t lowerRightS,
    uint32_t lowerRightT
) {
    RdpTile *tile = &tmem->tiles[tileIndex];
    const uint8_t *source = ImageTexelAddress(image, upperLeftS >> 2, upperLeftT >> 2);
    uint32_t entryCount = (lowerRightS >> 2) - (upperLeftS >> 2) + 1;
    // Palettes are 16-bit entries, whose bytes a host word swaps
    uint32_t order = image->isHostOrder && SDL_BYTEORDER == SDL_LIL_ENDIAN ? 1 : 0;
    uint32_t entry;

    SDL_assert(tileIndex >= 0 && tileIndex < RDP_TILE_COUNT && lowerRightS >= upperLeftS);
    SetTileCorners(tile, upperLeftS, upperLeftT, lowerRightS, lowerRightT);
    for (entry = 0; entry < entryCount; entry++) {
        uint32_t destination =
            (tile->tmemAddress + (entry * PALETTE_ENTRY_STRIDE)) & TMEM_ADDRESS_MASK;
        uint8_t high = source[(entry * 2) ^ order];
        uint8_t low = source[((entry * 2) + 1) ^ order];
        uint32_t copy;

        for (copy = 0; copy < PALETTE_ENTRY_STRIDE; copy += 2) {
            tmem->bytes[(destination + copy) & TMEM_ADDRESS_MASK] = high;
            tmem->bytes[(destination + copy + 1) & TMEM_ADDRESS_MASK] = low;
        }

        tmem->isHostOrderWord[destination / TMEM_WORD_SIZE] = false;
    }
}

// How many texels a tile can read along an axis: a mask's repeat, or what
// its corners clamp it to
static uint32_t MeasureTileAxis(const TileAxis *axis, uint32_t upperLeft, uint32_t lowerRight) {
    uint32_t extent;

    if (axis->maskBits != 0) {
        extent = 1u << SDL_min(axis->maskBits, 10);
    }
    else {
        extent = (lowerRight >> 2) - (upperLeft >> 2) + 1;
        if (lowerRight < upperLeft) {
            extent = 1;
        }
    }

    return SDL_clamp(extent, 1u, (uint32_t)TILE_TEXTURE_SIDE_MAX);
}

static uint32_t TexelRowBytes(const RdpTile *tile, uint32_t width) {
    if (tile->size == G_IM_SIZ_32b) {
        return width * 2;
    }

    return BytesForTexels(tile->size, width);
}

static uint64_t HashTmemRange(uint64_t hash, const Tmem *tmem, uint32_t start, uint32_t size) {
    uint32_t offset = start & TMEM_ADDRESS_MASK;

    while (size != 0) {
        uint32_t chunk = SDL_min(size, (uint32_t)TMEM_SIZE - offset);

        hash = HashBytes(hash, &tmem->bytes[offset], chunk);
        // The same bytes read as other texels in the other order
        hash = HashBytes(
            hash,
            &tmem->isHostOrderWord[offset / TMEM_WORD_SIZE],
            ((offset + chunk + TMEM_WORD_SIZE - 1) / TMEM_WORD_SIZE) - (offset / TMEM_WORD_SIZE)
        );
        size -= chunk;
        offset = 0;
    }

    return hash;
}

static bool IsColorIndexed(const RdpTile *tile, PaletteType paletteType) {
    return tile->format == G_IM_FMT_CI && paletteType != PALETTE_NONE;
}

TileTexture DescribeTileTexture(const Tmem *tmem, int tileIndex, PaletteType paletteType) {
    const RdpTile *tile = &tmem->tiles[tileIndex];
    TileTexture texture;
    uint64_t hash;
    uint32_t rowBytes;
    uint32_t row;

    SDL_assert(tileIndex >= 0 && tileIndex < RDP_TILE_COUNT);
    texture.width = (uint16_t)MeasureTileAxis(&tile->s, tile->upperLeftS, tile->lowerRightS);
    texture.height = (uint16_t)MeasureTileAxis(&tile->t, tile->upperLeftT, tile->lowerRightT);
    hash = HashWord(
        HASH_SEED,
        ((uint64_t)tile->format << 56) | ((uint64_t)tile->size << 48) |
            ((uint64_t)paletteType << 40) | ((uint64_t)texture.width << 20) | texture.height
    );

    // Rows that start at the same byte read the same texels, and with no line
    // between them every row reads the first
    rowBytes = TexelRowBytes(tile, texture.width);
    if (tile->lineBytes == rowBytes) {
        hash = HashTmemRange(hash, tmem, tile->tmemAddress, rowBytes * texture.height);
    }
    else {
        hash = HashWord(hash, tile->lineBytes);
        for (row = 0; row < texture.height; row++) {
            hash = HashTmemRange(hash, tmem, tile->tmemAddress + (row * tile->lineBytes), rowBytes);
        }
    }

    if (tile->size == G_IM_SIZ_32b) {
        hash = HashTmemRange(
            hash,
            tmem,
            (tile->tmemAddress & TMEM_HALF_MASK) + TMEM_HALF_SIZE,
            rowBytes * texture.height
        );
    }

    if (IsColorIndexed(tile, paletteType)) {
        if (tile->size == G_IM_SIZ_4b) {
            hash = HashTmemRange(
                hash,
                tmem,
                TMEM_PALETTE_START + (tile->palette * SMALL_PALETTE_SIZE * PALETTE_ENTRY_STRIDE),
                SMALL_PALETTE_SIZE * PALETTE_ENTRY_STRIDE
            );
        }
        else {
            hash = HashTmemRange(
                hash,
                tmem,
                TMEM_PALETTE_START,
                LARGE_PALETTE_SIZE * PALETTE_ENTRY_STRIDE
            );
        }
    }

    texture.key = FinishHash(hash);
    return texture;
}

uint32_t ExpandRgba16(uint16_t texel) {
    uint32_t red = (texel >> 11) & 0x1F;
    uint32_t green = (texel >> 6) & 0x1F;
    uint32_t blue = (texel >> 1) & 0x1F;
    uint32_t alpha = (texel & 1) ? 0xFF : 0;

    red = (red << 3) | (red >> 2);
    green = (green << 3) | (green >> 2);
    blue = (blue << 3) | (blue >> 2);
    return red | (green << 8) | (blue << 16) | (alpha << 24);
}

static uint32_t PackGray(uint32_t intensity, uint32_t alpha) {
    return intensity | (intensity << 8) | (intensity << 16) | (alpha << 24);
}

static uint32_t ExpandIa16(uint16_t texel) {
    return PackGray(texel >> 8, texel & 0xFF);
}

static uint16_t ReadTmem16(const Tmem *tmem, uint32_t address) {
    uint32_t first = address & TMEM_ADDRESS_MASK;
    uint16_t texel =
        (uint16_t)((tmem->bytes[first] << 8) | tmem->bytes[(address + 1) & TMEM_ADDRESS_MASK]);

    if (tmem->isHostOrderWord[first / TMEM_WORD_SIZE] && SDL_BYTEORDER == SDL_LIL_ENDIAN) {
        texel = SDL_Swap16(texel);
    }

    return texel;
}

static uint32_t LookUpPalette(const Tmem *tmem, uint32_t index, PaletteType paletteType) {
    uint16_t entry = ReadTmem16(tmem, TMEM_PALETTE_START + (index * PALETTE_ENTRY_STRIDE));

    return paletteType == PALETTE_IA16 ? ExpandIa16(entry) : ExpandRgba16(entry);
}

// The texel's bits, in the tile's size, at (s, t) of its texture
static uint32_t FetchTexelBits(const Tmem *tmem, const RdpTile *tile, uint32_t s, uint32_t t) {
    uint32_t rowStart = tile->tmemAddress + (t * tile->lineBytes);
    uint32_t swap = (t & 1) ? ODD_ROW_SWAP : 0;
    uint32_t address;
    uint8_t byte;

    switch (tile->size) {
        case G_IM_SIZ_4b:
            byte = tmem->bytes[((rowStart + (s >> 1)) ^ swap) & TMEM_ADDRESS_MASK];
            return (s & 1) ? (byte & 0xF) : (byte >> 4);
        case G_IM_SIZ_8b:
            return tmem->bytes[((rowStart + s) ^ swap) & TMEM_ADDRESS_MASK];
        case G_IM_SIZ_16b:
            return ReadTmem16(tmem, (rowStart + (s * 2)) ^ swap);
        default:
            address = ((rowStart + (s * 2)) ^ swap) & TMEM_HALF_MASK;
            return ((uint32_t)ReadTmem16(tmem, address) << 16) |
                ReadTmem16(tmem, address + TMEM_HALF_SIZE);
    }
}

// Expands a texel's bits as its format and size say. The combinations that
// the RDP has no format for read as intensity, as it does.
static uint32_t ExpandTexel(
    const Tmem *tmem,
    const RdpTile *tile,
    uint32_t bits,
    PaletteType paletteType
) {
    uint32_t intensity;

    if (IsColorIndexed(tile, paletteType)) {
        if (tile->size == G_IM_SIZ_4b) {
            bits |= (uint32_t)tile->palette << 4;
        }

        return LookUpPalette(tmem, bits & 0xFF, paletteType);
    }

    switch (tile->size) {
        case G_IM_SIZ_4b:
            if (tile->format == G_IM_FMT_IA) {
                intensity = bits >> 1;
                intensity = (intensity << 5) | (intensity << 2) | (intensity >> 1);
                return PackGray(intensity, (bits & 1) ? 0xFF : 0);
            }

            return PackGray(bits * 0x11, bits * 0x11);
        case G_IM_SIZ_8b:
            if (tile->format == G_IM_FMT_IA) {
                return PackGray((bits >> 4) * 0x11, (bits & 0xF) * 0x11);
            }

            return PackGray(bits, bits);
        case G_IM_SIZ_16b:
            if (tile->format == G_IM_FMT_IA) {
                return ExpandIa16((uint16_t)bits);
            }

            if (tile->format == G_IM_FMT_RGBA) {
                return ExpandRgba16((uint16_t)bits);
            }

            return PackGray(bits >> 8, bits >> 8);
        default:
            return ((bits >> 24) & 0xFF) | (((bits >> 16) & 0xFF) << 8) |
                (((bits >> 8) & 0xFF) << 16) | ((bits & 0xFF) << 24);
    }
}

void DecodeTileTexture(
    const Tmem *tmem,
    int tileIndex,
    PaletteType paletteType,
    const TileTexture *texture,
    uint32_t *rgba
) {
    const RdpTile *tile = &tmem->tiles[tileIndex];
    uint32_t s;
    uint32_t t;

    SDL_assert(tileIndex >= 0 && tileIndex < RDP_TILE_COUNT);
    for (t = 0; t < texture->height; t++) {
        for (s = 0; s < texture->width; s++) {
            *rgba++ = ExpandTexel(tmem, tile, FetchTexelBits(tmem, tile, s, t), paletteType);
        }
    }
}
