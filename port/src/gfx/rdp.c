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

    // A fill of the depth image that waits for the next draw to use it
    const uint8_t *depthClearAddress;
    float depthClearValue;

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
    if (!f_IsFrameReady) {
        InitFrame(&f_Frame);
        InitTextureCache();
        InitColorImages();
        f_IsFrameReady = true;
    }

    SDL_zero(f_Rdp);
    f_Rdp.isMaterialDirty = true;
}

void ResetRdpForTask(void) {
    // A task starts with no color image of its own, but the RDP's other
    // state carries over from the last, as on the N64
    f_Rdp.colorImage = NULL;
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

    // The N64's addresses, from the ROM's data: segmented, or in KSEG0 or 1
    if (address >= 0x80000000u) {
        resolved = address & 0x1FFFFFFF;
    }
    else {
        resolved = memory->segments[(address >> 24) & 0xF] + (address & 0x00FFFFFF);
    }

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

static bool DoesCombinerRead(const GfxMaterial *material, uint8_t input, uint8_t inputAlpha) {
    int firstWord = (material->modes & MATERIAL_TWO_CYCLE) ? 0 : 2;
    int i;

    for (i = firstWord; i < 4; i++) {
        if (DoSelectorsRead(material->combine[i], input, inputAlpha)) {
            return true;
        }
    }

    return false;
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
    if (kind == MATERIAL_KIND_COMBINED && (low & FORCE_BL)) {
        shape = ShapeBlender(BlenderCycle(low, isTwoCycle ? 1 : 0));
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

    if (Field(high, G_MDSFT_TEXTFILT, 2) != 0) {
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

    if (DoesCombinerRead(material, COMBINER_TEXEL0, COMBINER_TEXEL0_ALPHA)) {
        material->tiles[0] = *UseSampler(tileIndex);
    }

    if (DoesCombinerRead(material, COMBINER_TEXEL1, COMBINER_TEXEL1_ALPHA)) {
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

// Makes the color image's target the current pass's, and returns whether
// there is one to draw into
static bool UseColorImageTarget(void) {
    uint16_t height = SDL_max(f_Rdp.scissor.bottom, 1);

    if (f_Rdp.colorImageAddress == NULL) {
        return false;
    }

    if (f_Rdp.colorImage == NULL || ColorImageHeight(f_Rdp.colorImage) < height) {
        // Uploading what the CPU wrote, or growing the target, must follow
        // the draws recorded so far
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
            f_Rdp.colorImageSize
        );
    }

    return true;
}

static void UseDepthTarget(void) {
    GfxTarget *depth = UseDepthImage(f_Rdp.depthImageAddress, f_Rdp.colorImage);
    float clearValue = -1.0f;

    if (f_Rdp.depthClearAddress == f_Rdp.depthImageAddress) {
        clearValue = f_Rdp.depthClearValue;
        f_Rdp.depthClearAddress = NULL;
    }

    UseFrameDepth(&f_Frame, depth, clearValue);
}

// Readies the frame for a primitive of vertexCount vertices and gives
// where they go, or NULL when there is nowhere to draw
static GfxVertex *BeginPrimitive(
    uint32_t vertexCount,
    int tileIndex,
    MaterialKind kind,
    bool usesDepth,
    uint32_t *materialIndex
) {
    GfxRect scissor = f_Rdp.scissor;

    if (!HasFrameRoom(&f_Frame, vertexCount, 0)) {
        FlushRdpFrame();
    }

    if (!UseColorImageTarget()) {
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
    scissor.bottom = SDL_min(scissor.bottom, ColorImageHeight(f_Rdp.colorImage));
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

void DrawRdpTriangle(
    const RspVertex *first,
    const RspVertex *second,
    const RspVertex *third,
    const RspDrawState *state
) {
    const RspVertex *corners[3] = {first, second, third};
    bool usesDepth = (state->geometryMode & G_ZBUFFER) != 0;
    bool usesPrimitiveDepth = Field(f_Rdp.otherModeLow, G_MDSFT_ZSRCSEL, 1) != 0;
    bool isFlat = (state->geometryMode & G_SHADING_SMOOTH) == 0;
    uint32_t materialIndex = 0;
    GfxVertex *vertices;
    float xScale;
    float yScale;
    int i;

    vertices =
        BeginPrimitive(3, state->textureTile, MATERIAL_KIND_COMBINED, usesDepth, &materialIndex);
    if (vertices == NULL) {
        return;
    }

    xScale = 2.0f / (float)ColorImageWidth(f_Rdp.colorImage);
    yScale = 2.0f / (float)ColorImageHeight(f_Rdp.colorImage);
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

static void StoreRectangle(
    const Rectangle *rectangle,
    int tileIndex,
    MaterialKind kind,
    uint32_t color
) {
    bool usesPrimitiveDepth = Field(f_Rdp.otherModeLow, G_MDSFT_ZSRCSEL, 1) != 0;
    // The upper left, upper right, lower left and lower right
    static const uint8_t cornerOrder[6] = {0, 1, 2, 2, 1, 3};
    uint32_t materialIndex = 0;
    GfxVertex *vertices;
    float xScale;
    float yScale;
    float depth;
    int i;

    vertices = BeginPrimitive(6, tileIndex, kind, usesPrimitiveDepth, &materialIndex);
    if (vertices == NULL) {
        return;
    }

    xScale = 2.0f / (float)ColorImageWidth(f_Rdp.colorImage);
    yScale = 2.0f / (float)ColorImageHeight(f_Rdp.colorImage);
    depth = usesPrimitiveDepth ? PrimitiveDepth() : 0.0f;
    for (i = 0; i < 6; i++) {
        int corner = cornerOrder[i];
        GfxVertex *vertex = &vertices[i];
        float x = (corner & 1) ? rectangle->right : rectangle->left;
        float y = (corner & 2) ? rectangle->bottom : rectangle->top;

        vertex->position[0] = (x * xScale) - 1.0f;
        vertex->position[1] = 1.0f - (y * yScale);
        vertex->position[2] = depth;
        vertex->position[3] = 1.0f;
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

static void DrawRdpFillRectangle(uint32_t w0, uint32_t w1) {
    Rectangle rectangle;

    if (IsDrawingDepthImage()) {
        f_Rdp.depthClearAddress = f_Rdp.depthImageAddress;
        f_Rdp.depthClearValue = DecodeDepthImageValue(f_Rdp.fillColor);
        return;
    }

    PlaceRectangle(&rectangle, w0, w1);
    if (CycleType() == CYCLE_FILL) {
        StoreRectangle(&rectangle, 0, MATERIAL_KIND_FILL, FillColorForImage());
    }
    else {
        StoreRectangle(&rectangle, 0, MATERIAL_KIND_COMBINED, 0);
    }
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
