/*
 * The RDP's half of the interpreter: its state, its texture loads, and the
 * recording of each primitive for the GPU.
 *
 * Each primitive's fragments read a material that holds the combiner, the
 * blender's terms that the shader computes, the colors and the textures'
 * places in the atlas. Building one means hashing what the tiles read, so
 * a material is built only for a primitive after a change, and the tiles'
 * samplers are kept until a load or a tile command changes them.
 */

#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_endian.h>
#include <SDL3/SDL_stdinc.h>
#include <ultra64.h>

#include "color_images.h"
#include "frame.h"
#include "gfx_backend.h"
#include "port/os_host.h"
#include "rcp.h"
#include "texture_cache.h"
#include "tmem.h"

// The fields of the RDP's commands
#define Field(word, shift, width) (((uint32_t)(word) >> (shift)) & ((1u << (width)) - 1))

// The blender's color inputs and factors, as othermode's low word holds them
#define BLENDER_COLOR_IN 0
#define BLENDER_COLOR_MEMORY 1
#define BLENDER_A_ZERO 3
#define BLENDER_B_ONE_MINUS_A 0
#define BLENDER_B_MEMORY_ALPHA 1
#define BLENDER_B_ONE 2
#define BLENDER_B_ZERO 3

// The depth that a cleared depth image reads as
#define DEPTH_FAR 1.0f
#define DEPTH_IMAGE_MAX 0x3FFF
#define PRIMITIVE_DEPTH_MAX 0x7FFF

// The cycle types, as othermode's field holds them
#define CYCLE_TWO 1
#define CYCLE_COPY 2
#define CYCLE_FILL 3

#define DEPTH_CLEAR_MAX 4

// A fill of memory, as the depth image or as a color image, that the next
// draw to use that memory as the depth image clears depth to
typedef struct DepthClear {
    const uint8_t *address; // NULL for an unused entry
    float value;
} DepthClear;

#define DEPTH_WRITE_MAX 4
// The vertices of a rectangle's two triangles
#define RECTANGLE_VERTEX_COUNT 6

// A write of part of the depth image, in its pixels, that the next draw to
// use that memory as the depth image draws first: a fill of value, or a
// copy of another depth target's pixels
typedef struct DepthWrite {
    const uint8_t *address; // NULL for an unused entry
    float left;
    float top;
    float right;
    float bottom;
    float value;
    GfxCopy copy; // A copy's pixels; the source is NULL for a fill
    uint64_t sourceUse; // A copy's source's use when copied
} DepthWrite;

typedef struct TileSamplerCache {
    bool isValid;
    GfxTileSampler sampler;
} TileSamplerCache;

typedef struct RdpState {
    Tmem tmem;
    TextureImage textureImage;
    uint32_t otherModeHigh;
    uint32_t otherModeLow;
    uint32_t combineHigh;
    uint32_t combineLow;
    uint32_t primitiveColor;
    uint32_t environmentColor;
    uint32_t fogColor;
    uint32_t blendColor;
    uint32_t fillColor;
    uint8_t primitiveLodFraction;
    uint16_t primitiveDepth;
    GfxRect scissor;

    uint8_t *colorImageAddress;
    uint16_t colorImageWidth;
    uint8_t colorImageSize;
    ColorImage *colorImage; // NULL until a draw needs it
    const uint8_t *depthImageAddress;

    // The fills that wait for a draw to use their memory as the depth
    // image, oldest first
    DepthClear depthClears[DEPTH_CLEAR_MAX];
    // Writes of part of a depth image that wait so, oldest first, the used
    // entries before the unused
    DepthWrite depthWrites[DEPTH_WRITE_MAX];

    TileSamplerCache samplers[RDP_TILE_COUNT];
    bool isMaterialDirty;
    int materialTile; // The tile that the material's TEXEL0 reads
    int materialKind;
    GfxMaterial material;
    uint32_t materialIndex;
    bool isMaterialInFrame;
} RdpState;

// What a primitive draws with, beyond the othermode's cycle type
typedef enum MaterialKind {
    MATERIAL_KIND_COMBINED, // Through the combiner and blender
    MATERIAL_KIND_FILL, // The fill color, as is
    MATERIAL_KIND_COPY, // The texel, as is
} MaterialKind;

static RdpState f_Rdp;
static GfxFrame f_Frame;
static bool f_IsFrameReady;

void InitRdp(void) {
    // ResolveRcpAddress tells the host's addresses from the N64's by
    // whether they need more than 32 bits, so the program's own data must
    // lie above 4 GiB, as it does in a position-independent executable
    if (((uintptr_t)&f_Rdp >> 32) == 0) {
        PcFatalError(
            "The program's data lies below 4 GiB: build it as a position-independent executable"
        );
    }

    if (!f_IsFrameReady) {
        InitFrame(&f_Frame);
        InitTextureCache();
        InitColorImages();
        f_IsFrameReady = true;
    }

    SDL_zero(f_Rdp);
    f_Rdp.isMaterialDirty = true;
}

void ShutdownRdp(void) {
    FlushRdpFrame();
    ReleaseColorImages();
    f_Rdp.colorImage = NULL;
}

void ResetRdpForTask(void) {
    // A task starts with no color image of its own, but the RDP's other
    // state carries over from the last, as on the N64
    f_Rdp.colorImage = NULL;
    BeginColorImagesTask();
}

void InvalidateRdpMaterial(void) {
    f_Rdp.isMaterialDirty = true;
}

static void InvalidateSamplers(void) {
    int i;

    for (i = 0; i < RDP_TILE_COUNT; i++) {
        f_Rdp.samplers[i].isValid = false;
    }

    f_Rdp.isMaterialDirty = true;
}

void FlushRdpFrame(void) {
    if (f_Frame.passCount != 0 || f_Frame.uploadCount != 0) {
        BackendExecuteFrame(&f_Frame);
    }

    ResetFrame(&f_Frame);
    BeginTextureCacheFrame();
    // The samplers' textures must be found again, to count as used by the
    // next frame, and the material added to it
    InvalidateSamplers();
    f_Rdp.isMaterialInFrame = false;
}

void SyncRdpMemory(const void *start, uintptr_t size) {
    if (HasUnsyncedColorImage(start, size)) {
        FlushRdpFrame();
        SyncColorImagesToMemory(start, size);
    }
}

void osPcSyncRdpWrites(const void *start, u32 size) {
    SyncRdpMemory(start, size);
}

const void *ResolveRcpAddress(const RcpMemory *memory, uintptr_t address) {
    uintptr_t resolved;

    // A host's address, which osPcSegmentAddress may have tagged with a
    // segment in its top byte
    if ((address >> 32) != 0) {
        uint32_t segment = (uint32_t)(address >> PC_SEGMENT_SHIFT);

        if (segment == 0) {
            return (const void *)address;
        }

        return (const void *)(memory->segments[segment & 0xF] +
            (address & (((uintptr_t)1 << PC_SEGMENT_SHIFT) - 1)));
    }

    // The N64's addresses, from the ROM's data. The microcode takes the
    // segment from bits 24 to 27, whatever the bits above, so that KSEG0's
    // and KSEG1's addresses are segment 0's, and adds its base to the low 24
    // bits. The RSP's DMA keeps 24 bits of an N64 base's sum.
    resolved = memory->segments[(address >> 24) & 0xF];
    if ((resolved >> 32) != 0) {
        return (const void *)(resolved + (address & 0x00FFFFFF));
    }

    resolved = (resolved + (address & 0x00FFFFFF)) & 0x00FFFFFF;
    if (resolved < PC_RDRAM_SIZE) {
        return &osPcRdram[resolved];
    }

    return (const void *)resolved;
}

static void SetOtherModes(uint32_t high, uint32_t low) {
    uint32_t changed = (high ^ f_Rdp.otherModeHigh);

    if (changed & (3u << G_MDSFT_TEXTLUT)) {
        InvalidateSamplers();
    }

    f_Rdp.otherModeHigh = high;
    f_Rdp.otherModeLow = low;
    f_Rdp.isMaterialDirty = true;
}

static int CycleType(void) {
    return (int)Field(f_Rdp.otherModeHigh, G_MDSFT_CYCLETYPE, 2);
}

static PaletteType CurrentPaletteType(void) {
    switch (Field(f_Rdp.otherModeHigh, G_MDSFT_TEXTLUT, 2)) {
        case 2:
            return PALETTE_RGBA16;
        case 3:
            return PALETTE_IA16;
        default:
            return PALETTE_NONE;
    }
}

static void SetScissor(uint32_t w0, uint32_t w1) {
    uint32_t left = Field(w0, 12, 12) >> 2;
    uint32_t top = Field(w0, 0, 12) >> 2;
    uint32_t right = (Field(w1, 12, 12) + 3) >> 2;
    uint32_t bottom = (Field(w1, 0, 12) + 3) >> 2;

    f_Rdp.scissor.left = (uint16_t)left;
    f_Rdp.scissor.top = (uint16_t)top;
    f_Rdp.scissor.right = (uint16_t)SDL_max(right, left);
    f_Rdp.scissor.bottom = (uint16_t)SDL_max(bottom, top);
}

static void SetTile(uint32_t w0, uint32_t w1) {
    int tileIndex = (int)Field(w1, 24, 3);
    RdpTile *tile = &f_Rdp.tmem.tiles[tileIndex];

    tile->format = (uint8_t)Field(w0, 21, 3);
    tile->size = (uint8_t)Field(w0, 19, 2);
    tile->lineBytes = (uint16_t)(Field(w0, 9, 9) * TMEM_WORD_SIZE);
    tile->tmemAddress = (uint16_t)(Field(w0, 0, 9) * TMEM_WORD_SIZE);
    tile->palette = (uint8_t)Field(w1, 20, 4);
    tile->t.isMirrored = (uint8_t)Field(w1, 18, 1);
    tile->t.isClamped = (uint8_t)Field(w1, 19, 1);
    tile->t.maskBits = (uint8_t)Field(w1, 14, 4);
    tile->t.shift = (uint8_t)Field(w1, 10, 4);
    tile->s.isMirrored = (uint8_t)Field(w1, 8, 1);
    tile->s.isClamped = (uint8_t)Field(w1, 9, 1);
    tile->s.maskBits = (uint8_t)Field(w1, 4, 4);
    tile->s.shift = (uint8_t)Field(w1, 0, 4);
    f_Rdp.samplers[tileIndex].isValid = false;
    f_Rdp.isMaterialDirty = true;
}

static void SetTileSize(uint32_t w0, uint32_t w1) {
    int tileIndex = (int)Field(w1, 24, 3);
    RdpTile *tile = &f_Rdp.tmem.tiles[tileIndex];

    tile->upperLeftS = (uint16_t)Field(w0, 12, 12);
    tile->upperLeftT = (uint16_t)Field(w0, 0, 12);
    tile->lowerRightS = (uint16_t)Field(w1, 12, 12);
    tile->lowerRightT = (uint16_t)Field(w1, 0, 12);
    f_Rdp.samplers[tileIndex].isValid = false;
    f_Rdp.isMaterialDirty = true;
}

// Brings up to date the memory that a load reads: the bytes from a first
// texel to a last, whose rows are the image's
static void SyncLoadSource(
    uint32_t firstRow,
    uint32_t firstColumn,
    uint32_t lastRow,
    uint32_t lastColumn
) {
    const TextureImage *image = &f_Rdp.textureImage;
    uint32_t halfBytes = TexelSizeInHalfBytes(image->size);
    uintptr_t first = (((uintptr_t)firstRow * image->width) + firstColumn) * halfBytes / 2;
    uintptr_t end = ((((uintptr_t)lastRow * image->width) + lastColumn + 1) * halfBytes + 1) / 2;

    if (end > first) {
        SyncRdpMemory(image->address + first, end - first);
    }
}

static void RunLoad(uint8_t command, uint32_t w0, uint32_t w1) {
    int tileIndex = (int)Field(w1, 24, 3);
    uint32_t upperLeftS = Field(w0, 12, 12);
    uint32_t upperLeftT = Field(w0, 0, 12);
    uint32_t lowerRightS = Field(w1, 12, 12);
    uint32_t lowerRightT = Field(w1, 0, 12);

    if (f_Rdp.textureImage.address == NULL) {
        return;
    }

    if (command == G_LOADBLOCK) {
        if (lowerRightS < upperLeftS) {
            return;
        }

        // A block's corners count texels, and its lower right is its last
        // texel and dxt
        SyncLoadSource(upperLeftT, upperLeftS, upperLeftT, lowerRightS);
        LoadTmemBlock(
            &f_Rdp.tmem,
            &f_Rdp.textureImage,
            tileIndex,
            upperLeftS,
            upperLeftT,
            lowerRightS,
            lowerRightT
        );
    }
    else {
        SyncLoadSource(upperLeftT >> 2, upperLeftS >> 2, lowerRightT >> 2, lowerRightS >> 2);
        if (command == G_LOADTILE) {
            LoadTmemTile(
                &f_Rdp.tmem,
                &f_Rdp.textureImage,
                tileIndex,
                upperLeftS,
                upperLeftT,
                lowerRightS,
                lowerRightT
            );
        }
        else {
            LoadTmemPalette(
                &f_Rdp.tmem,
                &f_Rdp.textureImage,
                tileIndex,
                upperLeftS,
                upperLeftT,
                lowerRightS,
                lowerRightT
            );
        }
    }

    // Any tile may read the bytes that the load changed
    InvalidateSamplers();
}

static void SetColorImage(uint32_t w0, const void *address) {
    f_Rdp.colorImageAddress = (uint8_t *)address;
    f_Rdp.colorImageSize = (uint8_t)Field(w0, 19, 2);
    f_Rdp.colorImageWidth = (uint16_t)(Field(w0, 0, 12) + 1);
    f_Rdp.colorImage = NULL;
}

// The depth that the RDP's compressed depth format means, roughly: the
// game clears to the farthest depth and to nothing else
static float DecodeDepthImageValue(uint32_t fillColor) {
    uint32_t depth = (fillColor >> 18) & DEPTH_IMAGE_MAX;

    return depth == DEPTH_IMAGE_MAX ? DEPTH_FAR : (float)depth / DEPTH_IMAGE_MAX;
}

static bool IsDrawingDepthImage(void) {
    return f_Rdp.colorImageAddress != NULL && f_Rdp.colorImageAddress == f_Rdp.depthImageAddress;
}

// The combiner's selectors for each of its inputs, as CombinerInputs. The
// chroma key's center, scale and the color conversion's K4 and K5 read as
// zero: the game never sets them.
static const uint8_t f_ColorA[16] = {
    COMBINER_COMBINED,
    COMBINER_TEXEL0,
    COMBINER_TEXEL1,
    COMBINER_PRIMITIVE,
    COMBINER_SHADE,
    COMBINER_ENVIRONMENT,
    COMBINER_ONE,
    COMBINER_NOISE,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
};
static const uint8_t f_ColorB[16] = {
    COMBINER_COMBINED,
    COMBINER_TEXEL0,
    COMBINER_TEXEL1,
    COMBINER_PRIMITIVE,
    COMBINER_SHADE,
    COMBINER_ENVIRONMENT,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
};
static const uint8_t f_ColorC[32] = {
    COMBINER_COMBINED,
    COMBINER_TEXEL0,
    COMBINER_TEXEL1,
    COMBINER_PRIMITIVE,
    COMBINER_SHADE,
    COMBINER_ENVIRONMENT,
    COMBINER_ZERO,
    COMBINER_COMBINED_ALPHA,
    COMBINER_TEXEL0_ALPHA,
    COMBINER_TEXEL1_ALPHA,
    COMBINER_PRIMITIVE_ALPHA,
    COMBINER_SHADE_ALPHA,
    COMBINER_ENVIRONMENT_ALPHA,
    COMBINER_LOD_FRACTION,
    COMBINER_PRIMITIVE_LOD_FRACTION,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
    COMBINER_ZERO,
};
// The color's d, and the alpha's a, b and d
static const uint8_t f_Addend[8] = {
    COMBINER_COMBINED,
    COMBINER_TEXEL0,
    COMBINER_TEXEL1,
    COMBINER_PRIMITIVE,
    COMBINER_SHADE,
    COMBINER_ENVIRONMENT,
    COMBINER_ONE,
    COMBINER_ZERO,
};
static const uint8_t f_AlphaC[8] = {
    COMBINER_LOD_FRACTION,
    COMBINER_TEXEL0,
    COMBINER_TEXEL1,
    COMBINER_PRIMITIVE,
    COMBINER_SHADE,
    COMBINER_ENVIRONMENT,
    COMBINER_PRIMITIVE_LOD_FRACTION,
    COMBINER_ZERO,
};

static uint32_t PackSelectors(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return a | ((uint32_t)b << 8) | ((uint32_t)c << 16) | ((uint32_t)d << 24);
}

static void UnpackCombiner(uint32_t high, uint32_t low, uint32_t combine[4]) {
    combine[0] = PackSelectors(
        f_ColorA[Field(high, 20, 4)],
        f_ColorB[Field(low, 28, 4)],
        f_ColorC[Field(high, 15, 5)],
        f_Addend[Field(low, 15, 3)]
    );
    combine[1] = PackSelectors(
        f_Addend[Field(high, 12, 3)],
        f_Addend[Field(low, 12, 3)],
        f_AlphaC[Field(high, 9, 3)],
        f_Addend[Field(low, 9, 3)]
    );
    combine[2] = PackSelectors(
        f_ColorA[Field(high, 5, 4)],
        f_ColorB[Field(low, 24, 4)],
        f_ColorC[Field(high, 0, 5)],
        f_Addend[Field(low, 6, 3)]
    );
    combine[3] = PackSelectors(
        f_Addend[Field(low, 21, 3)],
        f_Addend[Field(low, 3, 3)],
        f_AlphaC[Field(low, 18, 3)],
        f_Addend[Field(low, 0, 3)]
    );
}

static bool DoSelectorsRead(uint32_t selectors, uint8_t input, uint8_t inputAlpha) {
    int i;

    for (i = 0; i < 4; i++) {
        uint8_t selector = (uint8_t)(selectors >> (i * 8));

        if (selector == input || selector == inputAlpha) {
            return true;
        }
    }

    return false;
}

// Whether a cycle of the combiner reads an input, in its color or alpha
static bool DoesCycleRead(
    const GfxMaterial *material,
    int cycle,
    uint8_t input,
    uint8_t inputAlpha
) {
    return DoSelectorsRead(material->combine[cycle * 2], input, inputAlpha) ||
        DoSelectorsRead(material->combine[(cycle * 2) + 1], input, inputAlpha);
}

// Whether the combiner reads the texel of the tile that TEXEL0 names, or of
// the next tile. The RDP hands each cycle's texel on down its pipeline, so
// two cycles' second cycle reads the next tile's texel as TEXEL0 and the
// first tile's as TEXEL1. One cycle reads only the first tile.
static void FindReadTiles(const GfxMaterial *material, bool readsTile[2]) {
    bool readsTexel0 = DoesCycleRead(material, 1, COMBINER_TEXEL0, COMBINER_TEXEL0_ALPHA);
    bool readsTexel1 = DoesCycleRead(material, 1, COMBINER_TEXEL1, COMBINER_TEXEL1_ALPHA);

    if ((material->modes & MATERIAL_TWO_CYCLE) == 0) {
        readsTile[0] = readsTexel0 || readsTexel1;
        readsTile[1] = false;
        return;
    }

    readsTile[0] =
        DoesCycleRead(material, 0, COMBINER_TEXEL0, COMBINER_TEXEL0_ALPHA) || readsTexel1;
    readsTile[1] =
        DoesCycleRead(material, 0, COMBINER_TEXEL1, COMBINER_TEXEL1_ALPHA) || readsTexel0;
}

// An RDP color, red in the top byte, as the shader's floats
static void UnpackColor(uint32_t rgba, float color[4]) {
    color[0] = (float)(rgba >> 24) / 255.0f;
    color[1] = (float)((rgba >> 16) & 0xFF) / 255.0f;
    color[2] = (float)((rgba >> 8) & 0xFF) / 255.0f;
    color[3] = (float)(rgba & 0xFF) / 255.0f;
}

// A tile's shift as the factor that it multiplies coordinates by
static float ShiftFactor(uint8_t shift) {
    if (shift <= 10) {
        return 1.0f / (float)(1u << shift);
    }

    return (float)(1u << (16 - shift));
}

static uint32_t AxisModes(const TileAxis *axis) {
    uint32_t modes = SDL_min(axis->maskBits, 10);

    // The RDP clamps an axis that doesn't wrap
    if (axis->isClamped || axis->maskBits == 0) {
        modes |= TILE_AXIS_CLAMP;
    }

    if (axis->isMirrored) {
        modes |= TILE_AXIS_MIRROR;
    }

    return modes;
}

// The texels between a tile's corners, less one, which a clamped axis keeps
// to
static int32_t ClampLimit(uint32_t upperLeft, uint32_t lowerRight, uint16_t size) {
    int32_t limit = (int32_t)(lowerRight >> 2) - (int32_t)(upperLeft >> 2);

    return SDL_clamp(limit, 0, (int32_t)size - 1);
}

// Finds the tile's texture in the atlas, decoding it into the frame's
// uploads if it isn't there, and describes how the shader samples it
static void BuildSampler(int tileIndex, GfxTileSampler *sampler) {
    const RdpTile *tile = &f_Rdp.tmem.tiles[tileIndex];
    PaletteType paletteType = CurrentPaletteType();
    TileTexture texture = DescribeTileTexture(&f_Rdp.tmem, tileIndex, paletteType);
    uint32_t texelCount = (uint32_t)texture.width * texture.height;
    AtlasPlace place;
    TextureLookup lookup;

    if (!HasFrameRoom(&f_Frame, 0, texelCount)) {
        FlushRdpFrame();
    }

    lookup = FindTexture(texture.key, texture.width, texture.height, &place);
    if (lookup == TEXTURE_NEEDS_FLUSH) {
        FlushRdpFrame();
        lookup = FindTexture(texture.key, texture.width, texture.height, &place);
        SDL_assert_always(lookup != TEXTURE_NEEDS_FLUSH);
    }

    if (lookup == TEXTURE_PLACED) {
        uint32_t *texels = AddFrameTextureUpload(
            &f_Frame,
            place.layer,
            place.x,
            place.y,
            texture.width,
            texture.height
        );

        DecodeTileTexture(&f_Rdp.tmem, tileIndex, paletteType, &texture, texels);
    }

    SDL_zerop(sampler);
    sampler->scale[0] = ShiftFactor(tile->s.shift);
    sampler->scale[1] = ShiftFactor(tile->t.shift);
    sampler->offset[0] = (float)tile->upperLeftS / 4.0f;
    sampler->offset[1] = (float)tile->upperLeftT / 4.0f;
    sampler->atlasOrigin[0] = place.x;
    sampler->atlasOrigin[1] = place.y;
    sampler->atlasLayer = place.layer;
    sampler->axisModes = AxisModes(&tile->s) | (AxisModes(&tile->t) << 8);
    sampler->size[0] = texture.width;
    sampler->size[1] = texture.height;
    sampler->clampLimit[0] = ClampLimit(tile->upperLeftS, tile->lowerRightS, texture.width);
    sampler->clampLimit[1] = ClampLimit(tile->upperLeftT, tile->lowerRightT, texture.height);
}

static const GfxTileSampler *UseSampler(int tileIndex) {
    TileSamplerCache *cache = &f_Rdp.samplers[tileIndex];

    if (!cache->isValid) {
        BuildSampler(tileIndex, &cache->sampler);
        cache->isValid = true;
    }

    return &cache->sampler;
}

static uint8_t BlenderCycle(uint32_t low, int cycle) {
    int shift = cycle * 2;

    return (uint8_t)(Field(low, 30 - shift, 2) | (Field(low, 26 - shift, 2) << 2) |
        (Field(low, 22 - shift, 2) << 4) | (Field(low, 18 - shift, 2) << 6));
}

static BlendShape ShapeBlender(uint8_t lastCycle) {
    uint32_t first = lastCycle & 3;
    uint32_t second = (lastCycle >> 4) & 3;

    if (first == BLENDER_COLOR_MEMORY && second == BLENDER_COLOR_MEMORY) {
        return BLEND_KEEP_MEMORY;
    }

    if (second == BLENDER_COLOR_MEMORY) {
        return BLEND_FIRST_OVER_MEMORY;
    }

    if (first == BLENDER_COLOR_MEMORY) {
        return BLEND_SECOND_OVER_MEMORY;
    }

    return BLEND_NO_MEMORY;
}

static void BuildBlender(GfxMaterial *material, MaterialKind kind) {
    uint32_t low = f_Rdp.otherModeLow;
    bool isTwoCycle = (material->modes & MATERIAL_TWO_CYCLE) != 0;
    BlendShape shape = BLEND_FIRST_ONLY;

    material->blender = BlenderCycle(low, 0) | ((uint32_t)BlenderCycle(low, 1) << 8);
    if (kind == MATERIAL_KIND_COMBINED) {
        uint8_t lastCycle = BlenderCycle(low, isTwoCycle ? 1 : 0);

        if (low & FORCE_BL) {
            shape = ShapeBlender(lastCycle);
        }
        else if ((lastCycle & 3) == BLENDER_COLOR_MEMORY) {
            // Unblended, the last cycle gives its first color as it is
            shape = BLEND_KEEP_MEMORY;
        }
    }

    if (kind != MATERIAL_KIND_COMBINED) {
        // The color goes through the blender's first input as it is
        material->blender = 0;
    }

    material->modes |= (uint32_t)shape << MATERIAL_BLEND_SHIFT;
}

// Rebuilds the material for a primitive whose TEXEL0 reads tileIndex
static void BuildMaterial(int tileIndex, MaterialKind kind) {
    GfxMaterial *material = &f_Rdp.material;
    uint32_t low = f_Rdp.otherModeLow;
    uint32_t high = f_Rdp.otherModeHigh;
    bool readsTile[2];

    SDL_zerop(material);
    if (kind == MATERIAL_KIND_COMBINED) {
        UnpackCombiner(f_Rdp.combineHigh, f_Rdp.combineLow, material->combine);
        if (CycleType() == CYCLE_TWO) {
            material->modes |= MATERIAL_TWO_CYCLE;
        }
    }
    else {
        uint8_t input = kind == MATERIAL_KIND_FILL ? COMBINER_SHADE : COMBINER_TEXEL0;
        uint32_t selectors = PackSelectors(COMBINER_ZERO, COMBINER_ZERO, COMBINER_ZERO, input);

        material->combine[2] = selectors;
        material->combine[3] = selectors;
    }

    // Only the combiner's texels pass through the filter; the copy cycle
    // copies them as they are
    if (Field(high, G_MDSFT_TEXTFILT, 2) != 0 && kind == MATERIAL_KIND_COMBINED) {
        material->modes |= MATERIAL_FILTERED;
    }

    // The compare's threshold is the blend color's alpha, or with dithering
    // a random one for each pixel
    if (Field(low, G_MDSFT_ALPHACOMPARE, 2) != 0 && kind != MATERIAL_KIND_FILL) {
        material->modes |= MATERIAL_ALPHA_THRESHOLD;
        if ((low & G_AC_DITHER) == G_AC_DITHER) {
            material->modes |= MATERIAL_ALPHA_DITHER;
        }
    }

    // Coverage times alpha leaves the faintest fragments uncovered, and
    // without forced blending a pixel needs most of its coverage to be drawn
    if ((low & CVG_X_ALPHA) && kind == MATERIAL_KIND_COMBINED) {
        material->modes |= MATERIAL_ALPHA_COVERAGE;
        material->coverageThreshold = (low & FORCE_BL) ? 1.0f / 8.0f : 0.5f;
    }

    BuildBlender(material, kind);
    UnpackColor(f_Rdp.primitiveColor, material->primitiveColor);
    UnpackColor(f_Rdp.environmentColor, material->environmentColor);
    UnpackColor(f_Rdp.fogColor, material->fogColor);
    UnpackColor(f_Rdp.blendColor, material->blendColor);
    material->primitiveLodFraction = (float)f_Rdp.primitiveLodFraction / 255.0f;

    FindReadTiles(material, readsTile);
    if (readsTile[0]) {
        material->tiles[0] = *UseSampler(tileIndex);
    }

    if (readsTile[1]) {
        material->tiles[1] = *UseSampler((tileIndex + 1) & (RDP_TILE_COUNT - 1));
    }
}

// The material's index in the frame, built again if the state has changed
static uint32_t UseMaterial(int tileIndex, MaterialKind kind) {
    if (f_Rdp.isMaterialDirty ||
        f_Rdp.materialTile != tileIndex ||
        f_Rdp.materialKind != (int)kind) {
        // A flush while the samplers are found leaves the textures found
        // before it uncounted in the new frame, so it takes another pass
        do {
            f_Rdp.isMaterialDirty = false;
            BuildMaterial(tileIndex, kind);
        }
        while (f_Rdp.isMaterialDirty);

        f_Rdp.materialTile = tileIndex;
        f_Rdp.materialKind = (int)kind;
        f_Rdp.isMaterialInFrame = false;
    }

    if (!f_Rdp.isMaterialInFrame) {
        f_Rdp.materialIndex = AddFrameMaterial(&f_Frame, &f_Rdp.material);
        f_Rdp.isMaterialInFrame = true;
    }

    return f_Rdp.materialIndex;
}

static uint16_t PipelineKey(bool usesDepth, MaterialKind kind) {
    uint32_t low = f_Rdp.otherModeLow;
    uint32_t shape = (f_Rdp.material.modes & MATERIAL_BLEND_MASK) >> MATERIAL_BLEND_SHIFT;
    uint16_t key = 0;

    if (usesDepth) {
        if (low & Z_CMP) {
            key |= PIPELINE_DEPTH_TEST;
        }

        if (low & Z_UPD) {
            key |= PIPELINE_DEPTH_WRITE;
        }

        if ((low & ZMODE_DEC) == ZMODE_DEC) {
            key |= PIPELINE_DEPTH_DECAL;
        }
    }

    if (kind != MATERIAL_KIND_COMBINED) {
        key |= PIPELINE_WRITE_ALPHA;
    }

    if (shape == BLEND_FIRST_OVER_MEMORY ||
        shape == BLEND_SECOND_OVER_MEMORY ||
        shape == BLEND_KEEP_MEMORY) {
        DestinationFactor destination = DESTINATION_SOURCE_ALPHA;

        if (shape == BLEND_KEEP_MEMORY) {
            destination = DESTINATION_ONE;
        }
        else if (shape == BLEND_FIRST_OVER_MEMORY) {
            static const uint8_t factors[4] = {
                DESTINATION_ONE_MINUS_SOURCE_ALPHA,
                DESTINATION_DESTINATION_ALPHA,
                DESTINATION_ONE,
                DESTINATION_ZERO,
            };
            int cycle = (f_Rdp.material.modes & MATERIAL_TWO_CYCLE) ? 1 : 0;

            destination = factors[(BlenderCycle(low, cycle) >> 6) & 3];
        }

        key |= PIPELINE_BLEND | (uint16_t)(destination << PIPELINE_DESTINATION_SHIFT);
    }

    return key;
}

// The rows of the color image that a primitive reaching down to bottom, in
// pixels, can draw in. The RDP writes only the pixels that primitives
// cover, and the game draws into small textures with the scissor left at
// the screen's size, so an image is as tall as its drawing reaches.
static uint16_t FindDrawnHeight(float bottom) {
    uint16_t height = SDL_max(f_Rdp.scissor.bottom, 1);

    if (bottom < (float)height) {
        height = (uint16_t)SDL_max(SDL_ceilf(bottom), 1.0f);
    }

    return height;
}

// Makes the color image's target the current pass's, and returns whether
// there is one to draw into, for a primitive reaching down to bottom
static bool UseColorImageTarget(float bottom) {
    uint16_t height = FindDrawnHeight(bottom);

    if (f_Rdp.colorImageAddress == NULL) {
        return false;
    }

    if (f_Rdp.colorImage == NULL || ColorImageHeight(f_Rdp.colorImage) < height) {
        // Uploading what the CPU wrote, or growing the target, must follow
        // the draws recorded so far. Taking in more rows of memory needn't:
        // no draw has reached them.
        if (!IsColorImageCurrent(
                f_Rdp.colorImageAddress,
                f_Rdp.colorImageWidth,
                height,
                f_Rdp.colorImageSize
            )) {
            FlushRdpFrame();
        }

        f_Rdp.colorImage = UseColorImage(
            f_Rdp.colorImageAddress,
            f_Rdp.colorImageWidth,
            height,
            SDL_max(height, f_Rdp.scissor.bottom),
            f_Rdp.colorImageSize
        );
    }

    return true;
}

// Notes that the memory at address holds value as depth throughout, in
// place of any older note of it. When the notes are full the oldest goes.
// Forgets the waiting writes of part of the depth image at address
static void ForgetDepthWrites(const uint8_t *address) {
    int keptCount = 0;
    int i;

    for (i = 0; i < DEPTH_WRITE_MAX; i++) {
        if (f_Rdp.depthWrites[i].address != address) {
            f_Rdp.depthWrites[keptCount++] = f_Rdp.depthWrites[i];
        }
    }

    for (i = keptCount; i < DEPTH_WRITE_MAX; i++) {
        f_Rdp.depthWrites[i].address = NULL;
    }
}

static int CountDepthWrites(void) {
    int count = 0;

    while (count < DEPTH_WRITE_MAX && f_Rdp.depthWrites[count].address != NULL) {
        count++;
    }

    return count;
}

// A new write of the depth image at address, after the others, dropping
// the oldest write when there are too many
static DepthWrite *AppendDepthWrite(const uint8_t *address) {
    int count = CountDepthWrites();
    DepthWrite *write;

    SDL_assert(address != NULL);
    if (count == DEPTH_WRITE_MAX) {
        SDL_memmove(
            &f_Rdp.depthWrites[0],
            &f_Rdp.depthWrites[1],
            (DEPTH_WRITE_MAX - 1) * sizeof(DepthWrite)
        );
        count--;
    }

    write = &f_Rdp.depthWrites[count];
    SDL_zerop(write);
    write->address = address;
    return write;
}

// Notes a fill of the depth image at address from left, top to right,
// bottom
static void NoteDepthFill(
    const uint8_t *address,
    float left,
    float top,
    float right,
    float bottom,
    float value
) {
    DepthWrite *fill;

    SDL_assert(right > left && bottom > top);
    MarkDepthImageBehind(address);
    fill = AppendDepthWrite(address);
    fill->left = left;
    fill->top = top;
    fill->right = right;
    fill->bottom = bottom;
    fill->value = value;
}

// Notes a copy of pixels into the depth image at address from their
// source, a depth target at its use sourceUse. A copy that carries on the
// rows of the last write, a copy from the same source, lengthens it.
static void NoteDepthCopy(const uint8_t *address, const GfxCopy *pixels, uint64_t sourceUse) {
    int count = CountDepthWrites();
    DepthWrite *copy;

    SDL_assert(pixels->source != NULL && pixels->width != 0 && pixels->height != 0);
    if (count != 0) {
        DepthWrite *last = &f_Rdp.depthWrites[count - 1];

        if (last->address == address &&
            last->copy.source != NULL &&
            last->sourceUse == sourceUse &&
            DoesCopyCarryOn(&last->copy, pixels)) {
            last->copy.height = (uint16_t)(last->copy.height + pixels->height);
            last->bottom += (float)pixels->height;
            return;
        }
    }

    copy = AppendDepthWrite(address);
    copy->left = (float)pixels->x;
    copy->top = (float)pixels->y;
    copy->right = (float)(pixels->x + pixels->width);
    copy->bottom = (float)(pixels->y + pixels->height);
    copy->copy = *pixels;
    copy->sourceUse = sourceUse;
}

static void NoteDepthClear(const uint8_t *address, float value) {
    int replaced = 0;
    int i;

    SDL_assert(address != NULL);
    MarkDepthImageBehind(address);
    // The clear covers whatever the earlier writes drew
    ForgetDepthWrites(address);
    for (i = 0; i < DEPTH_CLEAR_MAX; i++) {
        if (f_Rdp.depthClears[i].address == address) {
            replaced = i;
        }
    }

    SDL_memmove(
        &f_Rdp.depthClears[replaced],
        &f_Rdp.depthClears[replaced + 1],
        (DEPTH_CLEAR_MAX - 1 - replaced) * sizeof(DepthClear)
    );
    f_Rdp.depthClears[DEPTH_CLEAR_MAX - 1].address = address;
    f_Rdp.depthClears[DEPTH_CLEAR_MAX - 1].value = value;
}

// The depth that a fill left the memory at address, forgetting the fill, or
// a negative value when no fill waits for it
static float TakeDepthClear(const uint8_t *address) {
    int i;

    for (i = 0; i < DEPTH_CLEAR_MAX; i++) {
        if (f_Rdp.depthClears[i].address == address) {
            f_Rdp.depthClears[i].address = NULL;
            return f_Rdp.depthClears[i].value;
        }
    }

    return -1.0f;
}

static void DrawDepthWrites(const uint8_t *address, const GfxTarget *depth);

static void UseDepthTarget(void) {
    GfxTarget *depth = UseDepthImage(f_Rdp.depthImageAddress, f_Rdp.colorImage);

    UseFrameDepth(&f_Frame, depth, TakeDepthClear(f_Rdp.depthImageAddress));
    DrawDepthWrites(f_Rdp.depthImageAddress, depth);
}

// Whether the frame has room for a primitive of vertexCount vertices, and
// for the writes of the depth image that may come first, each with its own
// draw, and the fills with their material
static bool HasRoomForPrimitive(uint32_t vertexCount) {
    uint32_t writeCount = (uint32_t)CountDepthWrites();

    return HasFrameRoom(&f_Frame, vertexCount + (writeCount * RECTANGLE_VERTEX_COUNT), 0) &&
        (writeCount == 0 ||
            (f_Frame.drawCount + writeCount < FRAME_DRAW_CAPACITY &&
                f_Frame.materialCount + 1 < FRAME_MATERIAL_CAPACITY));
}

// Readies the frame for a primitive of vertexCount vertices, reaching down
// to bottom in pixels, and gives where they go, or NULL when there is
// nowhere to draw
static GfxVertex *BeginPrimitive(
    float bottom,
    uint32_t vertexCount,
    int tileIndex,
    MaterialKind kind,
    bool usesDepth,
    uint32_t *materialIndex
) {
    GfxRect scissor = f_Rdp.scissor;

    if (!HasRoomForPrimitive(vertexCount)) {
        FlushRdpFrame();
    }

    if (!UseColorImageTarget(bottom)) {
        return NULL;
    }

    *materialIndex = UseMaterial(tileIndex, kind);
    usesDepth =
        usesDepth && f_Rdp.depthImageAddress != NULL && (f_Rdp.otherModeLow & (Z_CMP | Z_UPD)) != 0;
    BeginFramePass(&f_Frame, ColorImageTarget(f_Rdp.colorImage));
    if (usesDepth) {
        UseDepthTarget();
    }

    scissor.right = SDL_min(scissor.right, ColorImageWidth(f_Rdp.colorImage));
    scissor.bottom = SDL_min(scissor.bottom, ColorImageTargetHeight(f_Rdp.colorImage));
    if (scissor.right <= scissor.left || scissor.bottom <= scissor.top) {
        return NULL;
    }

    MarkColorImageDrawn(f_Rdp.colorImage);
    return AddFrameVertices(&f_Frame, vertexCount, PipelineKey(usesDepth, kind), &scissor);
}

static float PrimitiveDepth(void) {
    return (float)(f_Rdp.primitiveDepth & PRIMITIVE_DEPTH_MAX) / PRIMITIVE_DEPTH_MAX;
}

static uint32_t PackShade(const uint8_t color[4]) {
    return color[0] | ((uint32_t)color[1] << 8) | ((uint32_t)color[2] << 16) |
        ((uint32_t)color[3] << 24);
}

// The lowest row, in pixels, that a triangle's corners reach. One with a
// corner at or behind the eye has no bound on the screen, and takes the
// whole scissor.
static float FindTriangleBottom(const RspVertex *const corners[3]) {
    float bottom = 0.0f;
    int i;

    for (i = 0; i < 3; i++) {
        float w = corners[i]->screen[3];

        if (!(w > 0.0f)) {
            return (float)UINT16_MAX;
        }

        bottom = SDL_max(bottom, corners[i]->screen[1] / w);
    }

    return bottom;
}

void DrawRdpTriangle(
    const RspVertex *first,
    const RspVertex *second,
    const RspVertex *third,
    const RspDrawState *state
) {
    const RspVertex *corners[3] = {first, second, third};
    bool usesPrimitiveDepth = Field(f_Rdp.otherModeLow, G_MDSFT_ZSRCSEL, 1) != 0;
    // The primitive's depth takes the place of the triangle's own, which
    // the RSP sends only under G_ZBUFFER
    bool usesDepth = (state->geometryMode & G_ZBUFFER) != 0 || usesPrimitiveDepth;
    bool isFlat = (state->geometryMode & G_SHADING_SMOOTH) == 0;
    uint32_t materialIndex = 0;
    GfxVertex *vertices;
    float xScale;
    float yScale;
    int i;

    vertices = BeginPrimitive(
        FindTriangleBottom(corners),
        3,
        state->textureTile,
        MATERIAL_KIND_COMBINED,
        usesDepth,
        &materialIndex
    );
    if (vertices == NULL) {
        return;
    }

    xScale = 2.0f / (float)ColorImageWidth(f_Rdp.colorImage);
    yScale = 2.0f / (float)ColorImageTargetHeight(f_Rdp.colorImage);
    for (i = 0; i < 3; i++) {
        const RspVertex *corner = corners[i];
        GfxVertex *vertex = &vertices[i];
        float w = corner->screen[3];

        vertex->position[0] = (corner->screen[0] * xScale) - w;
        vertex->position[1] = w - (corner->screen[1] * yScale);
        vertex->position[2] = usesPrimitiveDepth ? PrimitiveDepth() * w : corner->screen[2];
        vertex->position[3] = w;
        vertex->texCoord[0] = corner->texCoord[0];
        vertex->texCoord[1] = corner->texCoord[1];
        // A flat triangle takes the first vertex's color
        vertex->color = PackShade(isFlat ? first->color : corner->color);
        vertex->material = materialIndex;
    }
}

// The corners of a rectangle, in pixels, and the texture coordinates of the
// upper left, the upper right and the lower left
typedef struct Rectangle {
    float left;
    float top;
    float right;
    float bottom;
    float texCoords[3][2];
} Rectangle;

// Whether the RDP draws nothing of the rectangle: it draws rows from the top
// down to the bottom, and each from the left to the right, so nothing where
// either pair is the wrong way round. A ball or shadow projected about a
// whole image's height below the screen leaves its rectangle so, its edges
// wrapped in their 12 bits to either side of 0.
static bool IsRectangleEmpty(const Rectangle *rectangle) {
    return rectangle->right <= rectangle->left || rectangle->bottom <= rectangle->top;
}

// The corners of a rectangle's two triangles, in their order: the upper
// left is 0, the upper right 1, the lower left 2 and the lower right 3
static const uint8_t f_RectangleCorners[RECTANGLE_VERTEX_COUNT] = {0, 1, 2, 2, 1, 3};

// Places a rectangle's vertex at the corner of left, top to right, bottom
// in the color image's pixels, at depth
static void PlaceRectangleVertex(
    GfxVertex *vertex,
    int corner,
    const float bounds[4],
    float depth
) {
    float x = (corner & 1) ? bounds[2] : bounds[0];
    float y = (corner & 2) ? bounds[3] : bounds[1];

    float xScale = 2.0f / (float)ColorImageWidth(f_Rdp.colorImage);
    float yScale = 2.0f / (float)ColorImageTargetHeight(f_Rdp.colorImage);

    vertex->position[0] = (x * xScale) - 1.0f;
    vertex->position[1] = 1.0f - (y * yScale);
    vertex->position[2] = depth;
    vertex->position[3] = 1.0f;
}

// Draws a copy into the current pass's depth: the texture coordinates are
// the source's pixels
static void DrawDepthCopy(const DepthWrite *copy, const GfxRect *scissor) {
    const float bounds[4] = {copy->left, copy->top, copy->right, copy->bottom};
    GfxVertex *vertices =
        AddFrameDepthCopyVertices(&f_Frame, copy->copy.source, RECTANGLE_VERTEX_COUNT, scissor);
    int i;

    SDL_memset(vertices, 0, RECTANGLE_VERTEX_COUNT * sizeof(GfxVertex));
    for (i = 0; i < RECTANGLE_VERTEX_COUNT; i++) {
        int corner = f_RectangleCorners[i];

        PlaceRectangleVertex(&vertices[i], corner, bounds, 0.0f);
        vertices[i].texCoord[0] =
            (float)(copy->copy.sourceX + ((corner & 1) ? copy->copy.width : 0));
        vertices[i].texCoord[1] =
            (float)(copy->copy.sourceY + ((corner & 2) ? copy->copy.height : 0));
    }
}

// Draws the waiting writes of part of the depth image at address into the
// current pass's depth, in their order, and forgets them. They write no
// color. Of the material that a fill's fragments read, nothing matters but
// that it discards none of them. A copy whose source has been drawn on or
// dropped since is dropped too: the RDP read the depth that is gone.
static void DrawDepthWrites(const uint8_t *address, const GfxTarget *depth) {
    static const GfxMaterial noMaterial;
    GfxRect wholeImage = {0, 0, 0, 0};
    uint32_t materialIndex = 0;
    bool hasMaterial = false;
    int i;

    wholeImage.right = ColorImageWidth(f_Rdp.colorImage);
    wholeImage.bottom = ColorImageTargetHeight(f_Rdp.colorImage);
    for (i = 0; i < DEPTH_WRITE_MAX; i++) {
        const DepthWrite *fill = &f_Rdp.depthWrites[i];
        const float bounds[4] = {fill->left, fill->top, fill->right, fill->bottom};
        GfxVertex *vertices;
        int j;

        if (fill->address != address) {
            continue;
        }

        if (fill->copy.source != NULL) {
            if (fill->copy.source != depth &&
                IsDepthTargetUnchanged(fill->copy.source, fill->sourceUse)) {
                DrawDepthCopy(fill, &wholeImage);
            }

            continue;
        }

        if (!hasMaterial) {
            materialIndex = AddFrameMaterial(&f_Frame, &noMaterial);
            hasMaterial = true;
        }

        vertices = AddFrameVertices(
            &f_Frame,
            RECTANGLE_VERTEX_COUNT,
            PIPELINE_DEPTH_WRITE | PIPELINE_DEPTH_ONLY,
            &wholeImage
        );
        SDL_memset(vertices, 0, RECTANGLE_VERTEX_COUNT * sizeof(GfxVertex));
        for (j = 0; j < RECTANGLE_VERTEX_COUNT; j++) {
            PlaceRectangleVertex(&vertices[j], f_RectangleCorners[j], bounds, fill->value);
            vertices[j].material = materialIndex;
        }
    }

    ForgetDepthWrites(address);
}

static void StoreRectangle(
    const Rectangle *rectangle,
    int tileIndex,
    MaterialKind kind,
    uint32_t color
) {
    bool usesPrimitiveDepth = Field(f_Rdp.otherModeLow, G_MDSFT_ZSRCSEL, 1) != 0;
    const float bounds[4] = {rectangle->left, rectangle->top, rectangle->right, rectangle->bottom};
    uint32_t materialIndex = 0;
    GfxVertex *vertices;
    float depth;
    int i;

    if (IsRectangleEmpty(rectangle)) {
        return;
    }

    vertices = BeginPrimitive(
        rectangle->bottom,
        RECTANGLE_VERTEX_COUNT,
        tileIndex,
        kind,
        usesPrimitiveDepth,
        &materialIndex
    );
    if (vertices == NULL) {
        return;
    }

    depth = usesPrimitiveDepth ? PrimitiveDepth() : 0.0f;
    for (i = 0; i < RECTANGLE_VERTEX_COUNT; i++) {
        int corner = f_RectangleCorners[i];
        GfxVertex *vertex = &vertices[i];

        PlaceRectangleVertex(vertex, corner, bounds, depth);
        if (corner == 3) {
            // The lower right is the upper right moved as the lower left is
            vertex->texCoord[0] = rectangle->texCoords[1][0] + rectangle->texCoords[2][0] -
                rectangle->texCoords[0][0];
            vertex->texCoord[1] = rectangle->texCoords[1][1] + rectangle->texCoords[2][1] -
                rectangle->texCoords[0][1];
        }
        else {
            vertex->texCoord[0] = rectangle->texCoords[corner][0];
            vertex->texCoord[1] = rectangle->texCoords[corner][1];
        }

        vertex->color = color;
        vertex->material = materialIndex;
    }
}

static bool IsFillOrCopy(void) {
    int cycleType = CycleType();

    return cycleType == CYCLE_FILL || cycleType == CYCLE_COPY;
}

// The rectangle's corners from a command's 10.2 coordinates. In the fill
// and copy modes the lower right pixels are drawn too.
static void PlaceRectangle(Rectangle *rectangle, uint32_t w0, uint32_t w1) {
    SDL_zerop(rectangle);
    rectangle->left = (float)Field(w1, 12, 12) / 4.0f;
    rectangle->top = (float)Field(w1, 0, 12) / 4.0f;
    if (IsFillOrCopy()) {
        rectangle->left = SDL_floorf(rectangle->left);
        rectangle->top = SDL_floorf(rectangle->top);
        rectangle->right = (float)((Field(w0, 12, 12) >> 2) + 1);
        rectangle->bottom = (float)((Field(w0, 0, 12) >> 2) + 1);
    }
    else {
        rectangle->right = (float)Field(w0, 12, 12) / 4.0f;
        rectangle->bottom = (float)Field(w0, 0, 12) / 4.0f;
    }
}

// The fill color as RGBA8, red lowest, for the color image's size
static uint32_t FillColorForImage(void) {
    uint32_t color = f_Rdp.fillColor;

    if (f_Rdp.colorImageSize == G_IM_SIZ_32b) {
        return SDL_Swap32(color);
    }

    return ExpandRgba16((uint16_t)(color >> 16));
}

// Whether the rectangle covers all that the scissor lets the RDP draw
static bool DoesRectangleCoverScissor(const Rectangle *rectangle) {
    return rectangle->left <= f_Rdp.scissor.left &&
        rectangle->top <= f_Rdp.scissor.top &&
        rectangle->right >= SDL_min(f_Rdp.scissor.right, f_Rdp.colorImageWidth) &&
        rectangle->bottom >= f_Rdp.scissor.bottom;
}

// Fills the depth image: a fill of all that the scissor lets it reach
// clears the image for its next use, and a fill of less waits to be drawn
// then, within the scissor
static void FillDepthImage(const Rectangle *rectangle) {
    float value = DecodeDepthImageValue(f_Rdp.fillColor);
    float left;
    float top;
    float right;
    float bottom;

    if (DoesRectangleCoverScissor(rectangle)) {
        NoteDepthClear(f_Rdp.depthImageAddress, value);
        return;
    }

    left = SDL_max(rectangle->left, (float)f_Rdp.scissor.left);
    top = SDL_max(rectangle->top, (float)f_Rdp.scissor.top);
    right = SDL_min(rectangle->right, (float)SDL_min(f_Rdp.scissor.right, f_Rdp.colorImageWidth));
    bottom = SDL_min(rectangle->bottom, (float)f_Rdp.scissor.bottom);
    if (right > left && bottom > top) {
        NoteDepthFill(f_Rdp.depthImageAddress, left, top, right, bottom, value);
    }
}

static void DrawRdpFillRectangle(uint32_t w0, uint32_t w1) {
    Rectangle rectangle;

    PlaceRectangle(&rectangle, w0, w1);
    if (IsDrawingDepthImage()) {
        FillDepthImage(&rectangle);
        return;
    }

    if (CycleType() == CYCLE_FILL) {
        // The game fills the saved frame's depth as a color image before it
        // uses it as the depth image
        if (f_Rdp.colorImageAddress != NULL && DoesRectangleCoverScissor(&rectangle)) {
            NoteDepthClear(f_Rdp.colorImageAddress, DecodeDepthImageValue(f_Rdp.fillColor));
        }

        StoreRectangle(&rectangle, 0, MATERIAL_KIND_FILL, FillColorForImage());
    }
    else {
        StoreRectangle(&rectangle, 0, MATERIAL_KIND_COMBINED, 0);
    }
}

// Whether the copy mode takes the tile's texels from texCoords a texel to a
// pixel, down and across, raw as a target holds them: whole RGBA texels, a
// step of one each way, no palette, and no alpha compare to leave any out,
// into a 16-bit image. A negative coordinate passes, to find no texels in
// TMEM.
static bool IsCopyOneToOne(int tileIndex, uint32_t texCoords, uint32_t texSteps, bool isFlipped) {
    const RdpTile *tile = &f_Rdp.tmem.tiles[tileIndex];

    return !isFlipped &&
        texSteps == ((4u << 10) << 16 | (1u << 10)) &&
        (texCoords & 0x001F001Fu) == 0 &&
        tile->format == G_IM_FMT_RGBA &&
        Field(f_Rdp.otherModeHigh, G_MDSFT_TEXTLUT, 2) == 0 &&
        Field(f_Rdp.otherModeLow, G_MDSFT_ALPHACOMPARE, 2) == 0 &&
        f_Rdp.colorImageSize == G_IM_SIZ_16b;
}

// Notes a copy, for the color image's memory's next use as the depth image,
// of the depth that the GPU drew into the copied pixels' memory as the
// depth image, when it drew there last
static void NoteDepthOfCopy(const TileSource *texels, const GfxCopy *pixels) {
    GfxCopy depth = *pixels;
    uint64_t sourceUse;

    depth.source = FindDepthPixelsTarget(
        texels->address,
        texels->imageWidth,
        pixels->width,
        pixels->height,
        &depth.sourceX,
        &depth.sourceY,
        &sourceUse
    );
    if (depth.source != NULL) {
        NoteDepthCopy(f_Rdp.colorImageAddress, &depth, sourceUse);
    }
}

// Copies the rectangle's pixels, within the scissor, from the target that
// holds the texels' image to the color image's, at the renderer's scale,
// when there is one, along with any depth drawn into their memory; the
// texels start at whole texel s, t. Returns whether it copied them.
static bool CopyHeldPixels(const Rectangle *rectangle, int tileIndex, uint32_t s, uint32_t t) {
    GfxTarget *destination;
    GfxTarget *source;
    TileSource texels;
    GfxCopy copy;
    int left;
    int top;
    int right;
    int bottom;

    if (IsRectangleEmpty(rectangle)) {
        return false;
    }

    if (!HasFrameRoom(&f_Frame, 0, 0)) {
        FlushRdpFrame();
    }

    // The color image comes first: making it can drop an image that its
    // memory overlaps
    if (!UseColorImageTarget(rectangle->bottom)) {
        return false;
    }

    left = SDL_max((int)rectangle->left, (int)f_Rdp.scissor.left);
    top = SDL_max((int)rectangle->top, (int)f_Rdp.scissor.top);
    right = SDL_min((int)rectangle->right, (int)f_Rdp.scissor.right);
    right = SDL_min(right, (int)ColorImageWidth(f_Rdp.colorImage));
    bottom = SDL_min((int)rectangle->bottom, (int)f_Rdp.scissor.bottom);
    bottom = SDL_min(bottom, (int)ColorImageTargetHeight(f_Rdp.colorImage));
    if (right <= left || bottom <= top) {
        return false;
    }

    s += (uint32_t)(left - (int)rectangle->left);
    t += (uint32_t)(top - (int)rectangle->top);
    if (!FindTileSource(
            &f_Rdp.tmem,
            tileIndex,
            s,
            t,
            (uint32_t)(right - left),
            (uint32_t)(bottom - top),
            &texels
        )) {
        return false;
    }

    destination = ColorImageTarget(f_Rdp.colorImage);
    source = FindPixelsTarget(
        texels.address,
        texels.imageWidth,
        (uint16_t)(right - left),
        (uint16_t)(bottom - top),
        &copy.sourceX,
        &copy.sourceY
    );
    if (source == NULL || source == destination) {
        return false;
    }

    copy.source = source;
    copy.x = (uint16_t)left;
    copy.y = (uint16_t)top;
    copy.width = (uint16_t)(right - left);
    copy.height = (uint16_t)(bottom - top);
    AddFrameCopy(&f_Frame, destination, &copy);
    MarkColorImageDrawn(f_Rdp.colorImage);
    NoteDepthOfCopy(&texels, &copy);
    return true;
}

void DrawRdpTextureRectangle(
    uint32_t w0,
    uint32_t w1,
    uint32_t texCoords,
    uint32_t texSteps,
    bool isFlipped
) {
    int tileIndex = (int)Field(w1, 24, 3);
    float s = (float)(int16_t)(texCoords >> 16) / 32.0f;
    float t = (float)(int16_t)texCoords / 32.0f;
    float sStep = (float)(int16_t)(texSteps >> 16) / 1024.0f;
    float tStep = (float)(int16_t)texSteps / 1024.0f;
    float width;
    float height;
    Rectangle rectangle;
    MaterialKind kind = MATERIAL_KIND_COMBINED;

    PlaceRectangle(&rectangle, w0, w1);
    if (CycleType() == CYCLE_COPY) {
        // Copying between the targets keeps the renderer's scale, which a
        // draw of the texels that memory holds would lose
        if (IsCopyOneToOne(tileIndex, texCoords, texSteps, isFlipped) &&
            CopyHeldPixels(&rectangle, tileIndex, texCoords >> 21, (texCoords & 0xFFFF) >> 5)) {
            return;
        }

        // Copying steps four texels at a time
        sStep /= 4.0f;
        kind = MATERIAL_KIND_COPY;
    }

    width = rectangle.right - rectangle.left;
    height = rectangle.bottom - rectangle.top;
    rectangle.texCoords[0][0] = s;
    rectangle.texCoords[0][1] = t;
    if (isFlipped) {
        // S goes down the rectangle and T across it
        rectangle.texCoords[1][0] = s;
        rectangle.texCoords[1][1] = t + (tStep * width);
        rectangle.texCoords[2][0] = s + (sStep * height);
        rectangle.texCoords[2][1] = t;
    }
    else {
        rectangle.texCoords[1][0] = s + (sStep * width);
        rectangle.texCoords[1][1] = t;
        rectangle.texCoords[2][0] = s;
        rectangle.texCoords[2][1] = t + (tStep * height);
    }

    StoreRectangle(&rectangle, tileIndex, kind, 0);
}

void RunRdpCommand(uint32_t w0, uintptr_t w1, const RcpMemory *memory) {
    uint8_t command = (uint8_t)(w0 >> 24);
    uint32_t low = (uint32_t)w1;

    switch (command) {
        case G_SETCIMG:
            SetColorImage(w0, ResolveRcpAddress(memory, w1));
            break;
        case G_SETZIMG:
            f_Rdp.depthImageAddress = ResolveRcpAddress(memory, w1);
            break;
        case G_SETTIMG:
            f_Rdp.textureImage.address = ResolveRcpAddress(memory, w1);
            f_Rdp.textureImage.format = (uint8_t)Field(w0, 21, 3);
            f_Rdp.textureImage.size = (uint8_t)Field(w0, 19, 2);
            f_Rdp.textureImage.width = (uint16_t)(Field(w0, 0, 12) + 1);
            f_Rdp.textureImage.isHostOrder =
                f_Rdp.textureImage.address != NULL && !osPcIsRomOrder(f_Rdp.textureImage.address);
            break;
        case G_SETCOMBINE:
            f_Rdp.combineHigh = w0 & 0xFFFFFF;
            f_Rdp.combineLow = low;
            f_Rdp.isMaterialDirty = true;
            break;
        case G_SETENVCOLOR:
            f_Rdp.environmentColor = low;
            f_Rdp.isMaterialDirty = true;
            break;
        case G_SETPRIMCOLOR:
            f_Rdp.primitiveColor = low;
            f_Rdp.primitiveLodFraction = (uint8_t)w0;
            f_Rdp.isMaterialDirty = true;
            break;
        case G_SETBLENDCOLOR:
            f_Rdp.blendColor = low;
            f_Rdp.isMaterialDirty = true;
            break;
        case G_SETFOGCOLOR:
            f_Rdp.fogColor = low;
            f_Rdp.isMaterialDirty = true;
            break;
        case G_SETFILLCOLOR:
            f_Rdp.fillColor = low;
            break;
        case G_SETPRIMDEPTH:
            f_Rdp.primitiveDepth = (uint16_t)(low >> 16);
            break;
        case G_SETSCISSOR:
            SetScissor(w0, low);
            break;
        case G_RDPSETOTHERMODE:
            SetOtherModes(w0 & 0xFFFFFF, low);
            break;
        case G_SETTILE:
            SetTile(w0, low);
            break;
        case G_SETTILESIZE:
            SetTileSize(w0, low);
            break;
        case G_LOADBLOCK:
        case G_LOADTILE:
        case G_LOADTLUT:
            RunLoad(command, w0, low);
            break;
        case G_FILLRECT:
            DrawRdpFillRectangle(w0, low);
            break;
        default:
            // The syncs, and the chroma key and color conversion, which the
            // game leaves as they start
            break;
    }
}
