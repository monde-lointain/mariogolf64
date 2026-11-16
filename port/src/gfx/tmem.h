/*
 * The RDP's texture memory: the loads that fill it from RDRAM, the tiles
 * that describe how to read it, and the decoding of a tile's texels into
 * RGBA8 for the GPU.
 *
 * TMEM holds bytes in the N64's order. A load swaps the 32-bit halves of the
 * words of every odd row, as the RDP does so that it can read four texels of
 * a row at once, and the decoder swaps them back, so that a texture laid out
 * in RDRAM for that swap reads correctly.
 */

#ifndef PORT_GFX_TMEM_H
#define PORT_GFX_TMEM_H

#include <stdint.h>

#define TMEM_SIZE 4096
#define TMEM_WORD_SIZE 8
// Where the palettes start, as the RDP reads them in color index modes
#define TMEM_PALETTE_START 0x800
#define RDP_TILE_COUNT 8

// What a color index texel looks up, as othermode's TLUT field says
typedef enum PaletteType {
    PALETTE_NONE,
    PALETTE_RGBA16,
    PALETTE_IA16,
} PaletteType;

// How a tile reads one axis of a texture: the bits of SetTile's cm, mask and
// shift fields
typedef struct TileAxis {
    uint8_t isClamped;
    uint8_t isMirrored;
    uint8_t maskBits; // 0 for no wrapping
    uint8_t shift; // 0 to 10 divide, 11 to 15 multiply
} TileAxis;

typedef struct RdpTile {
    uint8_t format; // G_IM_FMT_*
    uint8_t size; // G_IM_SIZ_*
    uint8_t palette;
    uint16_t lineBytes; // From one row to the next
    uint16_t tmemAddress;
    TileAxis s;
    TileAxis t;
    // The tile's corners, in 10.2 fixed point texels, which SetTileSize and
    // the loads set
    uint16_t upperLeftS;
    uint16_t upperLeftT;
    uint16_t lowerRightS;
    uint16_t lowerRightT;
} RdpTile;

// The image that SetTextureImage names for the loads that follow
typedef struct TextureImage {
    const uint8_t *address;
    uint8_t format;
    uint8_t size;
    uint16_t width; // In texels
} TextureImage;

typedef struct Tmem {
    _Alignas(16) uint8_t bytes[TMEM_SIZE];
    RdpTile tiles[RDP_TILE_COUNT];
} Tmem;

// The texels of a tile's texture: how many there are each way, and how to
// find them in TMEM
typedef struct TileTexture {
    uint16_t width;
    uint16_t height;
    uint64_t key; // Equal for textures that decode to the same texels
} TileTexture;

// The bytes that a texel of each G_IM_SIZ_* takes, in a 1.1 fixed form:
// halves for 4-bit texels
uint32_t TexelSizeInHalfBytes(int size);

void LoadTmemBlock(
    Tmem *tmem,
    const TextureImage *image,
    int tileIndex,
    uint32_t upperLeftS,
    uint32_t upperLeftT,
    uint32_t lastTexel,
    uint32_t dxt
);
void LoadTmemTile(
    Tmem *tmem,
    const TextureImage *image,
    int tileIndex,
    uint32_t upperLeftS,
    uint32_t upperLeftT,
    uint32_t lowerRightS,
    uint32_t lowerRightT
);
void LoadTmemPalette(
    Tmem *tmem,
    const TextureImage *image,
    int tileIndex,
    uint32_t upperLeftS,
    uint32_t upperLeftT,
    uint32_t lowerRightS,
    uint32_t lowerRightT
);

// The extent of the texels that a tile can read, and a key that changes
// whenever any of them would decode differently
TileTexture DescribeTileTexture(const Tmem *tmem, int tileIndex, PaletteType paletteType);

// Writes the tile's texels as RGBA8, row after row, width by height of them,
// as DescribeTileTexture measured them
void DecodeTileTexture(
    const Tmem *tmem,
    int tileIndex,
    PaletteType paletteType,
    const TileTexture *texture,
    uint32_t *rgba
);

// One RGBA16 texel as RGBA8, red in the lowest byte
uint32_t ExpandRgba16(uint16_t texel);

#endif
