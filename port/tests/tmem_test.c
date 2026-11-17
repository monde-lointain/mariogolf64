/*
 * Tests of the renderer's TMEM: what each load leaves in it, and what each
 * texture format decodes to.
 */

#include <PR/gbi.h>
#include <string.h>

#include "test_support.h"
#include "tmem.h"

#define RGBA8(red, green, blue, alpha) \
    ((uint32_t)(red) | ((uint32_t)(green) << 8) | ((uint32_t)(blue) << 16) | \
        ((uint32_t)(alpha) << 24))

static Tmem f_Tmem;

static void SetTile(int tileIndex, int format, int size, int lineWords, int tmemWords) {
    RdpTile *tile = &f_Tmem.tiles[tileIndex];

    memset(tile, 0, sizeof(*tile));
    tile->format = (uint8_t)format;
    tile->size = (uint8_t)size;
    tile->lineBytes = (uint16_t)(lineWords * TMEM_WORD_SIZE);
    tile->tmemAddress = (uint16_t)(tmemWords * TMEM_WORD_SIZE);
}

static void SetTileSize(int tileIndex, int width, int height) {
    RdpTile *tile = &f_Tmem.tiles[tileIndex];

    tile->upperLeftS = 0;
    tile->upperLeftT = 0;
    tile->lowerRightS = (uint16_t)((width - 1) << 2);
    tile->lowerRightT = (uint16_t)((height - 1) << 2);
}

// An 8 by 4 RGBA16 image loaded with LoadTile reads back as it was, though
// TMEM holds its odd rows swapped
static void TestLoadTileRoundTrip(void) {
    uint8_t image[8 * 4 * 2];
    TextureImage source = {image, G_IM_FMT_RGBA, G_IM_SIZ_16b, 8};
    uint32_t decoded[8 * 4];
    TileTexture texture;
    int i;

    for (i = 0; i < 8 * 4; i++) {
        // Opaque, with the texel's number in the red bits
        uint16_t texel = (uint16_t)((i << 11) | 1);

        image[i * 2] = (uint8_t)(texel >> 8);
        image[(i * 2) + 1] = (uint8_t)texel;
    }

    memset(&f_Tmem, 0, sizeof(f_Tmem));
    SetTile(G_TX_LOADTILE, G_IM_FMT_RGBA, G_IM_SIZ_16b, 2, 0);
    LoadTmemTile(&f_Tmem, &source, G_TX_LOADTILE, 0, 0, 7 << 2, 3 << 2);
    EXPECT(f_Tmem.bytes[16] == image[16 + 4]);
    EXPECT(f_Tmem.bytes[16 + 4] == image[16]);

    SetTile(G_TX_RENDERTILE, G_IM_FMT_RGBA, G_IM_SIZ_16b, 2, 0);
    SetTileSize(G_TX_RENDERTILE, 8, 4);
    texture = DescribeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_NONE);
    EXPECT(texture.width == 8 && texture.height == 4);
    DecodeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_NONE, &texture, decoded);
    for (i = 0; i < 8 * 4; i++) {
        EXPECT(decoded[i] == ExpandRgba16((uint16_t)((i << 11) | 1)));
    }
}

// LoadBlock swaps the rows that its dxt counts as odd: with dxt for 16-byte
// rows, the words of the second row
static void TestLoadBlockDxt(void) {
    uint8_t image[32];
    TextureImage source = {image, G_IM_FMT_I, G_IM_SIZ_16b, 0};
    int i;

    for (i = 0; i < 32; i++) {
        image[i] = (uint8_t)i;
    }

    memset(&f_Tmem, 0, sizeof(f_Tmem));
    SetTile(G_TX_LOADTILE, G_IM_FMT_I, G_IM_SIZ_16b, 0, 4);
    // Sixteen texels, two words to a row: dxt is half a row per word
    LoadTmemBlock(&f_Tmem, &source, G_TX_LOADTILE, 0, 0, 15, 1 << 10);
    EXPECT(f_Tmem.bytes[32] == 0 && f_Tmem.bytes[32 + 8] == 8);
    EXPECT(f_Tmem.bytes[32 + 16] == 20 && f_Tmem.bytes[32 + 20] == 16);
    EXPECT(f_Tmem.bytes[32 + 24] == 28);
    EXPECT(f_Tmem.tiles[G_TX_LOADTILE].lowerRightS == 15);
}

// Rows that end partway through a word leave the rest of it as it was, and
// a load that runs off the end of TMEM goes on at its start. Here two rows
// of seven 8-bit texels start in TMEM's last word: the first fills all but
// its last byte, and the second, an odd row, wraps to the first word with
// its halves swapped, filling all but the last byte of its high half. Each
// word that a load reaches notes the image's order; the next, which none
// reaches, doesn't.
static void TestLoadTileRowEndsAndWrap(void) {
    uint8_t image[7 * 2];
    TextureImage source = {image, G_IM_FMT_I, G_IM_SIZ_8b, 7, true};
    int i;

    for (i = 0; i < 7 * 2; i++) {
        image[i] = (uint8_t)(i + 1);
    }

    memset(f_Tmem.bytes, 0xEE, sizeof(f_Tmem.bytes));
    memset(f_Tmem.isHostOrderWord, 0, sizeof(f_Tmem.isHostOrderWord));
    SetTile(G_TX_LOADTILE, G_IM_FMT_I, G_IM_SIZ_8b, 1, (TMEM_SIZE / TMEM_WORD_SIZE) - 1);
    LoadTmemTile(&f_Tmem, &source, G_TX_LOADTILE, 0, 0, 6 << 2, 1 << 2);
    for (i = 0; i < 7; i++) {
        EXPECT(f_Tmem.bytes[TMEM_SIZE - TMEM_WORD_SIZE + i] == i + 1);
    }

    EXPECT(f_Tmem.bytes[TMEM_SIZE - 1] == 0xEE);
    EXPECT(f_Tmem.bytes[4] == 8 && f_Tmem.bytes[5] == 9);
    EXPECT(f_Tmem.bytes[6] == 10 && f_Tmem.bytes[7] == 11);
    EXPECT(f_Tmem.bytes[0] == 12 && f_Tmem.bytes[1] == 13 && f_Tmem.bytes[2] == 14);
    EXPECT(f_Tmem.bytes[3] == 0xEE && f_Tmem.bytes[8] == 0xEE);
    EXPECT(f_Tmem.isHostOrderWord[(TMEM_SIZE / TMEM_WORD_SIZE) - 1]);
    EXPECT(f_Tmem.isHostOrderWord[0]);
    EXPECT(!f_Tmem.isHostOrderWord[1]);
}

// A row that spans words notes the image's order in each of them, and in
// no other
static void TestLoadTileNotesEachWordsOrder(void) {
    uint8_t image[12] = {0};
    TextureImage source = {image, G_IM_FMT_I, G_IM_SIZ_8b, 12, true};

    memset(f_Tmem.isHostOrderWord, 0, sizeof(f_Tmem.isHostOrderWord));
    SetTile(G_TX_LOADTILE, G_IM_FMT_I, G_IM_SIZ_8b, 2, 2);
    LoadTmemTile(&f_Tmem, &source, G_TX_LOADTILE, 0, 0, 11 << 2, 0);
    EXPECT(!f_Tmem.isHostOrderWord[1]);
    EXPECT(f_Tmem.isHostOrderWord[2] && f_Tmem.isHostOrderWord[3]);
    EXPECT(!f_Tmem.isHostOrderWord[4]);
}

// A CI4 tile reads its palette's sixteen entries; a CI8 tile all 256
static void TestColorIndexPalettes(void) {
    uint8_t palette[16 * 2];
    uint8_t texels[8] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    TextureImage paletteImage = {palette, G_IM_FMT_RGBA, G_IM_SIZ_16b, 16};
    TextureImage texelImage = {texels, G_IM_FMT_CI, G_IM_SIZ_16b, 4};
    uint32_t decoded[16];
    TileTexture texture;
    int i;

    for (i = 0; i < 16; i++) {
        // Blue rising with the entry, and opaque
        palette[i * 2] = 0;
        palette[(i * 2) + 1] = (uint8_t)((i << 1) | 1);
    }

    memset(&f_Tmem, 0, sizeof(f_Tmem));
    SetTile(G_TX_LOADTILE, G_IM_FMT_RGBA, G_IM_SIZ_4b, 0, 256 + (16 * 3));
    LoadTmemPalette(&f_Tmem, &paletteImage, G_TX_LOADTILE, 0, 0, 15 << 2, 0);
    SetTile(G_TX_LOADTILE, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0);
    LoadTmemBlock(&f_Tmem, &texelImage, G_TX_LOADTILE, 0, 0, 3, 0);

    SetTile(G_TX_RENDERTILE, G_IM_FMT_CI, G_IM_SIZ_4b, 1, 0);
    f_Tmem.tiles[G_TX_RENDERTILE].palette = 3;
    SetTileSize(G_TX_RENDERTILE, 16, 1);
    texture = DescribeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_RGBA16);
    DecodeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_RGBA16, &texture, decoded);
    for (i = 0; i < 16; i++) {
        EXPECT(decoded[i] == ExpandRgba16((uint16_t)((i << 1) | 1)));
    }

    // With the TLUT off, the indices read as intensity
    DecodeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_NONE, &texture, decoded);
    EXPECT(decoded[15] == RGBA8(0xFF, 0xFF, 0xFF, 0xFF));
    EXPECT(decoded[1] == RGBA8(0x11, 0x11, 0x11, 0x11));
}

// Host memory holds 16- and 32-bit texels and palette entries as host
// words, but 8-bit texels as the N64 does, though the game loads both as
// 16-bit blocks
static void TestHostOrderLoads(void) {
    uint16_t texels16[4] = {0xF801, 0x07C1, 0x003F, 0xFFFF};
    uint8_t texels8[8] = {0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70};
    uint16_t palette[16];
    uint32_t texels32[2] = {0x11223344, 0x55667788};
    TextureImage image16 = {(const uint8_t *)texels16, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, true};
    TextureImage image8 = {texels8, G_IM_FMT_I, G_IM_SIZ_16b, 4, true};
    TextureImage paletteImage = {(const uint8_t *)palette, G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, true};
    TextureImage image32 = {(const uint8_t *)texels32, G_IM_FMT_RGBA, G_IM_SIZ_32b, 2, true};
    uint8_t indices[2] = {0x12, 0x3F};
    TextureImage indexImage = {indices, G_IM_FMT_CI, G_IM_SIZ_16b, 1, true};
    uint32_t decoded[8];
    TileTexture texture;
    int i;

    for (i = 0; i < 16; i++) {
        palette[i] = (uint16_t)((i << 1) | 1);
    }

    memset(&f_Tmem, 0, sizeof(f_Tmem));
    SetTile(G_TX_LOADTILE, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0);
    LoadTmemBlock(&f_Tmem, &image16, G_TX_LOADTILE, 0, 0, 3, 0);
    SetTile(G_TX_RENDERTILE, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, 0);
    SetTileSize(G_TX_RENDERTILE, 4, 1);
    texture = DescribeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_NONE);
    DecodeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_NONE, &texture, decoded);
    for (i = 0; i < 4; i++) {
        EXPECT(decoded[i] == ExpandRgba16(texels16[i]));
    }

    SetTile(G_TX_LOADTILE, G_IM_FMT_I, G_IM_SIZ_16b, 0, 0);
    LoadTmemBlock(&f_Tmem, &image8, G_TX_LOADTILE, 0, 0, 3, 0);
    SetTile(G_TX_RENDERTILE, G_IM_FMT_I, G_IM_SIZ_8b, 1, 0);
    SetTileSize(G_TX_RENDERTILE, 8, 1);
    texture = DescribeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_NONE);
    DecodeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_NONE, &texture, decoded);
    for (i = 0; i < 8; i++) {
        EXPECT(decoded[i] == RGBA8(texels8[i], texels8[i], texels8[i], texels8[i]));
    }

    SetTile(G_TX_LOADTILE, G_IM_FMT_RGBA, G_IM_SIZ_4b, 0, 256);
    LoadTmemPalette(&f_Tmem, &paletteImage, G_TX_LOADTILE, 0, 0, 15 << 2, 0);
    SetTile(G_TX_LOADTILE, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0);
    LoadTmemBlock(&f_Tmem, &indexImage, G_TX_LOADTILE, 0, 0, 0, 0);
    SetTile(G_TX_RENDERTILE, G_IM_FMT_CI, G_IM_SIZ_4b, 1, 0);
    SetTileSize(G_TX_RENDERTILE, 4, 1);
    texture = DescribeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_RGBA16);
    DecodeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_RGBA16, &texture, decoded);
    EXPECT(decoded[0] == ExpandRgba16(palette[1]) && decoded[1] == ExpandRgba16(palette[2]));
    EXPECT(decoded[3] == ExpandRgba16(palette[15]));

    SetTile(G_TX_LOADTILE, G_IM_FMT_RGBA, G_IM_SIZ_32b, 0, 0);
    LoadTmemBlock(&f_Tmem, &image32, G_TX_LOADTILE, 0, 0, 1, 0);
    SetTile(G_TX_RENDERTILE, G_IM_FMT_RGBA, G_IM_SIZ_32b, 1, 0);
    SetTileSize(G_TX_RENDERTILE, 2, 1);
    texture = DescribeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_NONE);
    DecodeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_NONE, &texture, decoded);
    EXPECT(decoded[0] == RGBA8(0x11, 0x22, 0x33, 0x44));
    EXPECT(decoded[1] == RGBA8(0x55, 0x66, 0x77, 0x88));
}

static void DecodeOneRow(int format, int size, const uint8_t *bytes, int width, uint32_t *decoded) {
    TileTexture texture;

    memset(&f_Tmem, 0, sizeof(f_Tmem));
    memcpy(f_Tmem.bytes, bytes, 8);
    SetTile(G_TX_RENDERTILE, format, size, 1, 0);
    SetTileSize(G_TX_RENDERTILE, width, 1);
    texture = DescribeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_NONE);
    DecodeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_NONE, &texture, decoded);
}

static void TestIntensityFormats(void) {
    uint8_t bytes[8] = {0x3F, 0xA5, 0xFF, 0x80, 0, 0, 0, 0};
    uint32_t decoded[16];

    DecodeOneRow(G_IM_FMT_I, G_IM_SIZ_4b, bytes, 2, decoded);
    EXPECT(decoded[0] == RGBA8(0x33, 0x33, 0x33, 0x33));
    EXPECT(decoded[1] == RGBA8(0xFF, 0xFF, 0xFF, 0xFF));

    DecodeOneRow(G_IM_FMT_I, G_IM_SIZ_8b, bytes, 2, decoded);
    EXPECT(decoded[1] == RGBA8(0xA5, 0xA5, 0xA5, 0xA5));

    // IA4: three bits of intensity, one of alpha
    DecodeOneRow(G_IM_FMT_IA, G_IM_SIZ_4b, bytes, 2, decoded);
    EXPECT(decoded[0] == RGBA8(0x24, 0x24, 0x24, 0xFF));
    EXPECT(decoded[1] == RGBA8(0xFF, 0xFF, 0xFF, 0xFF));

    DecodeOneRow(G_IM_FMT_IA, G_IM_SIZ_8b, bytes, 2, decoded);
    EXPECT(decoded[0] == RGBA8(0x33, 0x33, 0x33, 0xFF));
    EXPECT(decoded[1] == RGBA8(0xAA, 0xAA, 0xAA, 0x55));

    DecodeOneRow(G_IM_FMT_IA, G_IM_SIZ_16b, bytes, 2, decoded);
    EXPECT(decoded[0] == RGBA8(0x3F, 0x3F, 0x3F, 0xA5));
    EXPECT(decoded[1] == RGBA8(0xFF, 0xFF, 0xFF, 0x80));
}

static void TestRgba16Expansion(void) {
    EXPECT(ExpandRgba16(0xFFFF) == RGBA8(0xFF, 0xFF, 0xFF, 0xFF));
    EXPECT(ExpandRgba16(0xF800) == RGBA8(0xFF, 0, 0, 0));
    EXPECT(ExpandRgba16(0x07C1) == RGBA8(0, 0xFF, 0, 0xFF));
    EXPECT(ExpandRgba16(0x003E) == RGBA8(0, 0, 0xFF, 0));
}

// A mask makes the texture its repeat's size, and the key follows the texels
static void TestTextureKeys(void) {
    uint64_t firstKey;
    TileTexture texture;

    memset(&f_Tmem, 0, sizeof(f_Tmem));
    SetTile(G_TX_RENDERTILE, G_IM_FMT_RGBA, G_IM_SIZ_16b, 4, 0);
    SetTileSize(G_TX_RENDERTILE, 100, 100);
    f_Tmem.tiles[G_TX_RENDERTILE].s.maskBits = 4;
    f_Tmem.tiles[G_TX_RENDERTILE].t.maskBits = 3;
    texture = DescribeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_NONE);
    EXPECT(texture.width == 16 && texture.height == 8);
    firstKey = texture.key;

    // Moving the texels in TMEM keeps the key
    f_Tmem.tiles[G_TX_RENDERTILE].tmemAddress = 0x400;
    EXPECT(DescribeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_NONE).key == firstKey);

    // Changing one texel that the tile reads changes it, and one it doesn't
    // read doesn't
    f_Tmem.bytes[0x400 + (7 * 32) + 31] = 1;
    EXPECT(DescribeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_NONE).key != firstKey);
    f_Tmem.bytes[0x400 + (7 * 32) + 31] = 0;
    f_Tmem.bytes[0x400 + (8 * 32)] = 1;
    EXPECT(DescribeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_NONE).key == firstKey);

    // So does reading the same bytes in another format
    f_Tmem.tiles[G_TX_RENDERTILE].format = G_IM_FMT_IA;
    EXPECT(DescribeTileTexture(&f_Tmem, G_TX_RENDERTILE, PALETTE_NONE).key != firstKey);
}

int main(void) {
    TestLoadTileRoundTrip();
    TestLoadBlockDxt();
    TestLoadTileRowEndsAndWrap();
    TestLoadTileNotesEachWordsOrder();
    TestColorIndexPalettes();
    TestHostOrderLoads();
    TestIntensityFormats();
    TestRgba16Expansion();
    TestTextureKeys();
    return ReportTestResults("tmem_test");
}
