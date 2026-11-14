#include "screen_layers.h"

#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "compiler.h"
#include "controller.h"
#include "debug_text.h"
#include "gbi_helpers.h"
#include "heap.h"
#include "language.h"
#include "main.h"
#include "math_util.h"
#include "screen.h"
#include "sound.h"
#include "text.h"

#define ASSET_DEFAULT_WIPE_SHAPE 0x63F
#define WIPE_SHAPE_COUNT 14
#define WIPE_CLOSE_START_PROGRESS 12.0f
#define WIPE_OPEN_START_PROGRESS (-2.0f)
#define WIPE_SOUND_CHANNEL 2
#define WIPE_SOUND_VOLUME 0x28
#define WIPE_SOUND_REVERB 0x14
#define WIPE_SHAPE_GROWTH 38.0f
#define WIPE_SHAPE_TEXELS 64
#define WIPE_ALIGNED_PROGRESS 8.0f
#define WIPE_FADE_START_PROGRESS 10.0f
#define WIPE_FADE_ALPHA_STEP 25.4f
#define WIPE_SMALLEST_SHAPE 2
#define WIPE_SQUARE_SIZE 304
#define WIPE_SQUARE_LEFT SCREEN_BORDER
#define WIPE_SQUARE_TOP ((SCREEN_HEIGHT / 2) - (WIPE_SQUARE_SIZE / 2))

#define ASSET_HALF_TURN_IMAGE 0x955
#define HALF_TURN_FRAMES 144
#define HALF_TURN_WIDTH 304
#define HALF_TURN_CENTER_COLUMN (HALF_TURN_PAIRS_PER_ROW / 2)
#define HALF_TURN_CENTER_ROW 112
#define HALF_TURN_SHOW_STEPS 8
#define HALF_TURN_HIDE_STEPS 12
#define HALF_TURN_SHOW_SOUND_RADIUS 96
#define HALF_TURN_HIDE_SOUND_RADIUS 48
#define HALF_TURN_WAIT_FRAMES 150
#define HALF_TURN_DEBUG_COLUMN 20
#define HALF_TURN_DEBUG_ROW 5
#define HALF_TURN_STRIP_COUNT 38
#define HALF_TURN_STRIP_ROWS 6
#define HALF_TURN_LAST_STRIP_ROWS 2
#define HALF_TURN_STRIP_SIZE (HALF_TURN_WIDTH * HALF_TURN_STRIP_ROWS * 2)
#define HALF_TURN_PAIRS_PER_ROW 152
#define HALF_TURN_ALPHA_BITS 0x10001
#define HALF_TURN_LAST_COLUMN_OFFSET 75
#define HALF_TURN_LAST_ROW_OFFSET 111
#define HALF_TURN_PAIR(column, row) texelPairs[((row) * HALF_TURN_PAIRS_PER_ROW) + (column)]
#define MarkCirclePoints(operation) \
    { \
        radiusColumnOffset = radius; \
        stepOffset = step; \
        if (radius > HALF_TURN_LAST_COLUMN_OFFSET) { \
            radiusColumnOffset = HALF_TURN_LAST_COLUMN_OFFSET; \
        } \
        if (step > HALF_TURN_LAST_ROW_OFFSET) { \
            stepOffset = HALF_TURN_LAST_ROW_OFFSET; \
        } \
        MarkOctantPoints(radiusColumnOffset, stepOffset, operation); \
        radiusRowOffset = radius; \
        stepOffset = step; \
        if (radius > HALF_TURN_LAST_ROW_OFFSET - 1) { \
            radiusRowOffset = HALF_TURN_LAST_ROW_OFFSET - 1; \
        } \
        if (step > HALF_TURN_LAST_COLUMN_OFFSET) { \
            stepOffset = HALF_TURN_LAST_COLUMN_OFFSET; \
        } \
        MarkOctantPoints(stepOffset, radiusRowOffset, operation); \
        error -= (step * 2) + 1; \
        if (error < 0) { \
            error += (radius - 1) * 2; \
            radius--; \
        } \
        step++; \
    }
#define MarkOctantPoints(column, row, operation) \
    { \
        HALF_TURN_PAIR(centerColumn + (column), centerRow + (row)) operation; \
        HALF_TURN_PAIR(centerColumn + (column), centerRow - (row)) operation; \
        HALF_TURN_PAIR(centerColumn + (column), centerRow + (row) + 1) operation; \
        HALF_TURN_PAIR(centerColumn + (column), centerRow - (row) + 1) operation; \
        HALF_TURN_PAIR(centerColumn - (column), centerRow + (row)) operation; \
        HALF_TURN_PAIR(centerColumn - (column), centerRow - (row)) operation; \
        HALF_TURN_PAIR(centerColumn - (column), centerRow + (row) + 1) operation; \
        HALF_TURN_PAIR(centerColumn - (column), centerRow - (row) + 1) operation; \
    }

enum HalfTurnState {
    HALF_TURN_STATE_SHOWING,
    HALF_TURN_STATE_EXPIRED,
    HALF_TURN_STATE_LEAVING,
    HALF_TURN_STATE_DONE
};

typedef s32 HalfTurnState;

#define FADE_ALPHA_RANGE 256

enum PromptState {
    PROMPT_STATE_CLOSED = -1,
    PROMPT_STATE_OPENING,
    PROMPT_STATE_CHOOSING,
    PROMPT_STATE_CHOSEN,
    PROMPT_STATE_CLOSING,
    PROMPT_STATE_CLOSED_AFTER_CHOICE
};

typedef s32 PromptState;

#define PROMPT_CHOICE_YES 0
#define PROMPT_CHOICE_NO 1
#define SAVING_RESULT_COUNT (PROMPT_RESULT_SAVE_REPLAY + 1)
#define PROMPT_TEXT_Y 108
#define SAVE_SOUND_CHANNEL 15
#define PROMPT_GROWTH_FRAMES 4
#define PROMPT_CLOUD_WIDTH 256
#define PROMPT_CLOUD_HALF_TEXELS 32
#define PROMPT_CLOUD_HALF_WIDTH 136
#define PROMPT_CLOUD_MARGIN 24
#define PROMPT_CLOUD_SMALLEST_HEIGHT 48
#define PROMPT_CENTER_X (SCREEN_WIDTH / 2)
#define PROMPT_CLOUD_MIDDLE_Y 116
#define YES_TEXELS_OFFSET LARGE_PALETTED_TEXELS_OFFSET
#define NO_TEXELS_OFFSET (YES_TEXELS_OFFSET + (YES_NO_BUTTON_WIDTH * YES_NO_BUTTON_HEIGHT))
#define YES_CENTER_X 128
#define NO_CENTER_X 192
#define CHOICES_MIDDLE_Y 134
#define CHOICE_HALF_WIDTH 24
#define CHOICE_HALF_HEIGHT 8
#define CHOSEN_HALF_WIDTH 32.0f
#define CHOSEN_HALF_HEIGHT 12.0f
#define CHOSEN_WIDTH_BOB 6.0f
#define CHOSEN_HEIGHT_BOB 2.0f
#define CHOICE_BOB_STEP_DEGREES 20
#define PROMPT_SOUND_VOLUME 0x64

typedef struct Prompt {
    MessageId messageId;
    s32 defaultChoice;
    s32 results[2];
} Prompt;

RgbColor g_ScreenFadeColor;

Gfx g_DecalTextureSetup[] = {
    gsDPPipeSync(),
    gsDPPipeSync(),
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsDPSetCombineMode(G_CC_DECALRGBA, G_CC_DECALRGBA),
    gsDPPipeSync(),
    gsDPSetRenderMode(G_RM_TEX_EDGE, G_RM_TEX_EDGE2),
    gsSPTexture(0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON),
    gsDPPipeSync(),
    gsDPSetAlphaCompare(G_AC_NONE),
    gsSPEndDisplayList(),
};
static Boolean f_IsHalfTurnShown = FALSE;
static HalfTurnState f_HalfTurnState = HALF_TURN_STATE_SHOWING;
static s32 f_HalfTurnTimer = 9;
static f32 f_WipeProgress = 0.0f;
static WipeState f_WipeKind = WIPE_NONE;
static s32 f_WipeShapeIds[WIPE_SHAPE_COUNT] = {
    0x643,
    0x645,
    0x642,
    0x641,
    0x644,
    0x646,
    0x90D,
    0x90D,
    0x90D,
    0x90E,
    0x90E,
    0x90E,
    ASSET_DEFAULT_WIPE_SHAPE,
    0x640,
};
static s32 f_UnusedWordAfterWipeShapeIds UNUSED = 0x63E;
s32 g_ScreenFadeDuration = 0;
s32 g_ScreenFadeFramesLeft = 0;
FadeDirection g_ScreenFadeKind = FADE_NONE;
static Prompt f_Prompts[] = {
    {0x2D, PROMPT_CHOICE_NO, {PROMPT_RESULT_SAVE_GAME, PROMPT_RESULT_STAY_IN_SAVE_MENU}},
    {0x2E, PROMPT_CHOICE_NO, {PROMPT_RESULT_END_GAME, PROMPT_RESULT_STAY_IN_SAVE_MENU}},
    {0x2D, PROMPT_CHOICE_NO, {PROMPT_RESULT_SAVE_REPLAY, PROMPT_RESULT_STAY_IN_SAVE_MENU}},
    {0x2F, PROMPT_CHOICE_NO, {PROMPT_RESULT_GIVE_UP_HOLE, PROMPT_RESULT_RESUME}},
    {0x30, PROMPT_CHOICE_NO, {PROMPT_RESULT_QUIT_GAME, PROMPT_RESULT_RESUME}},
    {0x31, PROMPT_CHOICE_NO, {PROMPT_RESULT_SKIP_REPLAY, PROMPT_RESULT_STAY_IN_SAVE_MENU}},
    {0x32, PROMPT_CHOICE_YES, {PROMPT_RESULT_BRING_MORE, PROMPT_RESULT_BRING_NO_MORE}},
    {0x33, PROMPT_CHOICE_NO, {PROMPT_RESULT_QUIT_GAME, PROMPT_RESULT_RESUME}},
    {0x34, PROMPT_CHOICE_NO, {PROMPT_RESULT_QUIT_GAME, PROMPT_RESULT_RESUME}},
    {0x35, PROMPT_CHOICE_NO, {PROMPT_RESULT_YES, PROMPT_RESULT_NO}},
    {0x36, PROMPT_CHOICE_NO, {PROMPT_RESULT_YES, PROMPT_RESULT_NO}},
};
static s32 f_PromptIndex = 0;
static s32 f_PromptFrame = 0;
static PromptState f_PromptState = PROMPT_STATE_CLOSED;
static s32 f_PromptChoice = PROMPT_CHOICE_YES;
static Radians f_ChoiceBobAngle = 0.0f;
static ImageAsset *f_HalfTurnImage;
static ImageAsset *f_WipeShapeImage;
static ImageAsset *f_PromptCloudImage;
static ImageAsset *f_YesNoImage;

void DrawTextureRectangle(
    Gfx **displayListHead,
    s32 xl,
    s32 yl,
    s32 xh,
    s32 yh,
    s32 tile,
    s32 s,
    s32 t,
    s32 dsdx,
    s32 dtdy
) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, xl, yl, xh, yh, tile, s, t, dsdx, dtdy);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void LoadSmallPalette(Gfx **displayListHead, s32 paletteIndex, PaletteColor *tlut) {
    Gfx *gfx = *displayListHead;

    gDPLoadTLUT_pal16(gfx++, paletteIndex, TextureAddress(tlut));
    *displayListHead = gfx;
}

void LoadLargePalette(Gfx **displayListHead, PaletteColor *tlut) {
    Gfx *gfx = *displayListHead;

    gDPLoadTLUT_pal256(gfx++, TextureAddress(tlut));
    *displayListHead = gfx;
}

void LoadNibbleTextureBlock(
    Gfx **displayListHead,
    void *texels,
    s32 format,
    s32 width,
    s32 height,
    s32 paletteIndex,
    s32 cms,
    s32 cmt,
    s32 masks,
    s32 maskt,
    s32 shifts,
    s32 shiftt
) {
    Gfx *gfx = *displayListHead;

    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(texels),
        format,
        width,
        height,
        paletteIndex,
        cms,
        cmt,
        masks,
        maskt,
        shifts,
        shiftt
    );
    *displayListHead = gfx;
}

#define LoadTextureBlockWithLoadSize( \
    pkt, \
    timg, \
    fmt, \
    loadSiz, \
    siz, \
    width, \
    height, \
    pal, \
    cms, \
    cmt, \
    masks, \
    maskt, \
    shifts, \
    shiftt \
) \
    { \
        gDPSetTextureImage((pkt)++, fmt, loadSiz##_LOAD_BLOCK, 1, TextureAddress(timg)); \
        gDPSetTile( \
            (pkt)++, \
            fmt, \
            loadSiz##_LOAD_BLOCK, \
            0, \
            0, \
            G_TX_LOADTILE, \
            0, \
            cmt, \
            maskt, \
            shiftt, \
            cms, \
            masks, \
            shifts \
        ); \
        gDPLoadSync((pkt)++); \
        gDPLoadBlock( \
            (pkt)++, \
            G_TX_LOADTILE, \
            0, \
            0, \
            ((((width) * (height)) + loadSiz##_INCR) >> loadSiz##_SHIFT) - 1, \
            CALC_DXT(width, loadSiz##_BYTES) \
        ); \
        gDPPipeSync((pkt)++); \
        gDPSetTile( \
            (pkt)++, \
            fmt, \
            siz, \
            (((width) * loadSiz##_LINE_BYTES) + 7) >> 3, \
            0, \
            G_TX_RENDERTILE, \
            pal, \
            cmt, \
            maskt, \
            shiftt, \
            cms, \
            masks, \
            shifts \
        ); \
        gDPSetTileSize( \
            (pkt)++, \
            G_TX_RENDERTILE, \
            0, \
            0, \
            ((width) - 1) << G_TEXTURE_IMAGE_FRAC, \
            ((height) - 1) << G_TEXTURE_IMAGE_FRAC \
        ); \
    }

void LoadTextureBlock(
    Gfx **displayListHead,
    void *texels,
    s32 format,
    s32 size,
    s32 width,
    s32 height,
    s32 paletteIndex,
    s32 cms,
    s32 cmt,
    s32 masks,
    s32 maskt,
    s32 shifts,
    s32 shiftt
) {
    Gfx *gfx = *displayListHead;

    if (size == G_IM_SIZ_8b) {
        LoadTextureBlockWithLoadSize(
            gfx,
            texels,
            format,
            G_IM_SIZ_8b,
            size,
            width,
            height,
            paletteIndex,
            cms,
            cmt,
            masks,
            maskt,
            shifts,
            shiftt
        );
    }
    else {
        LoadTextureBlockWithLoadSize(
            gfx,
            texels,
            format,
            G_IM_SIZ_16b,
            size,
            width,
            height,
            paletteIndex,
            cms,
            cmt,
            masks,
            maskt,
            shifts,
            shiftt
        );
    }

    *displayListHead = gfx;
}

void InitScreenLayers(void) {
    EncodedAsset asset;

    f_WipeShapeImage = AllocMemory(OpenEncodedAsset(ASSET_DEFAULT_WIPE_SHAPE, &asset));
    ReadEncodedAsset(f_WipeShapeImage, &asset);
    StopScreenFade();
}

void LoadWipeShape(CourseIndex courseIndex) {
    EncodedAsset asset;

    OpenEncodedAsset(f_WipeShapeIds[courseIndex % WIPE_SHAPE_COUNT], &asset);
    ReadEncodedAsset(f_WipeShapeImage, &asset);
}

void StopWipe(void) {
    f_WipeKind = WIPE_NONE;
}

s32 GetWipeKind(void) {
    return f_WipeKind;
}

s32 GetWipeProgress(void) {
    if (!f_IsHalfTurnShown) {
        return f_WipeProgress;
    }

    switch (f_HalfTurnState) {
        case HALF_TURN_STATE_EXPIRED:
        default:
            return 0;
        case HALF_TURN_STATE_DONE:
            f_IsHalfTurnShown = FALSE;
            return WIPE_OPEN_PROGRESS;
    }
}

void StartWipe(WipeState kind) {
    if (f_IsHalfTurnShown == TRUE) {
        if (kind == WIPE_OPENING || kind == WIPE_OPENING_FAST) {
            f_HalfTurnState = HALF_TURN_STATE_LEAVING;
            f_HalfTurnTimer = -1;
        }
    }
    else {
        f_WipeKind = kind;
        switch (kind) {
            case WIPE_CLOSING:
                f_WipeProgress = WIPE_CLOSE_START_PROGRESS;
                PlaySoundWithEffects(
                    SOUND_PROMPT_OPEN,
                    WIPE_SOUND_CHANNEL,
                    WIPE_SOUND_VOLUME,
                    WIPE_SOUND_REVERB,
                    SOUND_FULL_SCALE
                );
                break;
            case WIPE_OPENING:
            case WIPE_OPENING_FAST:
                f_WipeProgress = WIPE_OPEN_START_PROGRESS;
                PlaySoundWithEffects(
                    SOUND_PROMPT_CLOSE,
                    WIPE_SOUND_CHANNEL,
                    WIPE_SOUND_VOLUME,
                    WIPE_SOUND_REVERB,
                    SOUND_FULL_SCALE
                );
                break;
        }
    }
}

void DrawWipe(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 shapeSize;
    s32 shapeOffset;
    s32 alpha;
    s32 texelOffset;
    f32 alignment;

    if (f_WipeKind == WIPE_NONE) {
        return;
    }

    if (f_WipeKind == WIPE_CLOSING) {
        f_WipeProgress -= 1.0f;
        if (f_WipeProgress < 0.0f) {
            f_WipeProgress = 0.0f;
        }
    }
    else if (f_WipeKind == WIPE_OPENING || f_WipeKind == WIPE_OPENING_FAST) {
        f_WipeProgress += (f_WipeKind == WIPE_OPENING_FAST) ? 2.0f : 1.0f;
        if (f_WipeProgress > WIPE_OPEN_PROGRESS) {
            f_WipeProgress = WIPE_OPEN_PROGRESS;
            f_WipeKind = WIPE_NONE;
        }
    }

    shapeSize = f_WipeProgress * WIPE_SHAPE_GROWTH;
    alignment = (WIPE_ALIGNED_PROGRESS - f_WipeProgress) / WIPE_ALIGNED_PROGRESS;
    shapeOffset = alignment * -(WIPE_SQUARE_SIZE / 2);
    if (f_WipeProgress < WIPE_FADE_START_PROGRESS) {
        alpha = OPAQUE_ALPHA;
    }
    else {
        alpha = (WIPE_OPEN_PROGRESS - f_WipeProgress) * WIPE_FADE_ALPHA_STEP;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 255, 255, 255, 1);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_DISABLE);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, alpha);
    gDPPipeSync(gfx++);
    gDPSetCombineLERP(
        gfx++,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    if (shapeSize < WIPE_SMALLEST_SHAPE) {
        gDPPipeSync(gfx++);
        gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gSPTextureRectangle(
            gfx++,
            QuarterPixels(SCREEN_BORDER),
            QuarterPixels(SCREEN_BORDER),
            QuarterPixels(SCREEN_WIDTH - SCREEN_BORDER),
            QuarterPixels(SCREEN_HEIGHT - SCREEN_BORDER),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }
    else {
        texelOffset = TexelCoordinate(shapeOffset * WIPE_SHAPE_TEXELS) / shapeSize;
        gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(ImageBody(f_WipeShapeImage)),
            G_IM_FMT_I,
            WIPE_SHAPE_TEXELS,
            WIPE_SHAPE_TEXELS,
            0,
            G_TX_NOMIRROR | G_TX_CLAMP,
            G_TX_NOMIRROR | G_TX_CLAMP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(WIPE_SQUARE_LEFT),
            QuarterPixels(WIPE_SQUARE_TOP),
            QuarterPixels(WIPE_SQUARE_LEFT + WIPE_SQUARE_SIZE),
            QuarterPixels(WIPE_SQUARE_TOP + WIPE_SQUARE_SIZE),
            G_TX_RENDERTILE,
            texelOffset,
            texelOffset,
            WIPE_SHAPE_TEXELS * TEXEL_STEP_ONE / (f32)shapeSize,
            WIPE_SHAPE_TEXELS * TEXEL_STEP_ONE / (f32)shapeSize
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void StopScreenFade(void) {
    g_ScreenFadeKind = FADE_NONE;
}

Boolean IsScreenFadeDone(void) {
    return g_ScreenFadeFramesLeft < 2;
}

void StartScreenFadeOut(s32 red, s32 green, s32 blue, s32 frameCount) {
    g_ScreenFadeKind = FADE_OUT;
    g_ScreenFadeDuration = frameCount;
    g_ScreenFadeFramesLeft = g_ScreenFadeDuration;
    g_ScreenFadeColor.red = red;
    g_ScreenFadeColor.green = green;
    g_ScreenFadeColor.blue = blue;
}

void StartScreenFadeIn(s32 red, s32 green, s32 blue, s32 frameCount) {
    g_ScreenFadeKind = FADE_IN;
    g_ScreenFadeDuration = frameCount;
    g_ScreenFadeFramesLeft = g_ScreenFadeDuration;
    g_ScreenFadeColor.red = red;
    g_ScreenFadeColor.green = green;
    g_ScreenFadeColor.blue = blue;
}

void DrawScreenFade(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 alpha;

    if (g_ScreenFadeKind != FADE_NONE) {
        if (g_ScreenFadeKind == FADE_OUT) {
            alpha = FADE_ALPHA_RANGE -
                ((g_ScreenFadeFramesLeft * FADE_ALPHA_RANGE) / g_ScreenFadeDuration);
        }
        else {
            alpha = (g_ScreenFadeFramesLeft * FADE_ALPHA_RANGE) / g_ScreenFadeDuration;
        }

        if (alpha >= FADE_ALPHA_RANGE) {
            alpha = FADE_ALPHA_RANGE - 1;
        }

        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
        gDPSetBlendColor(gfx++, 255, 255, 255, 1);
        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_PATTERN);
        gDPSetPrimColor(
            gfx++,
            0,
            0,
            g_ScreenFadeColor.red,
            g_ScreenFadeColor.green,
            g_ScreenFadeColor.blue,
            alpha
        );
        gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPPipeSync(gfx++);
        gSPTextureRectangle(
            gfx++,
            QuarterPixels(SCREEN_BORDER),
            QuarterPixels(SCREEN_BORDER),
            QuarterPixels(SCREEN_WIDTH - SCREEN_BORDER),
            QuarterPixels(SCREEN_HEIGHT - SCREEN_BORDER),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
        g_ScreenFadeFramesLeft--;
        if (g_ScreenFadeFramesLeft == 0) {
            g_ScreenFadeKind = FADE_NONE;
        }

        *displayListHead = gfx;
    }
}

void LoadPromptImages(void) {
    EncodedAsset asset;

    f_PromptCloudImage = AllocMemory(OpenEncodedAsset(ASSET_WIDE_CLOUD, &asset));
    ReadEncodedAsset(f_PromptCloudImage, &asset);
    f_YesNoImage = AllocMemory(OpenEncodedAsset(ASSET_YES_NO, &asset));
    ReadEncodedAsset(f_YesNoImage, &asset);
}

void FreePromptImages(void) {
    FreeMemory(&f_PromptCloudImage);
    FreeMemory(&f_YesNoImage);
}

Boolean IsPromptClosed(void) {
    return f_PromptState == PROMPT_STATE_CLOSED;
}

void OpenPrompt(PromptId promptIndex) {
    Prompt *prompt;

    if (promptIndex != NO_PROMPT) {
        prompt = &f_Prompts[promptIndex];
        f_PromptFrame = 0;
        f_PromptIndex = promptIndex;
        LoadPromptImages();
        f_PromptState = PROMPT_STATE_OPENING;
        f_PromptChoice = prompt->defaultChoice;
        PlaySound(SOUND_PROMPT_OPEN, g_NextSoundChannel, PROMPT_SOUND_VOLUME);
    }
}

PromptResult UpdatePrompt(ControllerState *controller) {
    Prompt *prompt = &f_Prompts[f_PromptIndex];

    if (f_PromptState == PROMPT_STATE_CHOOSING) {
        SetTextSlot(
            PROMPT_CENTER_X,
            PROMPT_TEXT_Y,
            prompt->messageId,
            TEXT_SLOT_LARGE_FONT,
            PROMPT_TEXT_SLOT,
            TEXT_FLAG_CENTERED,
            0x5F,
            0x3F,
            0xFF,
            OPAQUE_ALPHA
        );
        if (controller->pressedButtons == A_BUTTON) {
            f_PromptState = PROMPT_STATE_CHOSEN;
            if (prompt->results[f_PromptChoice] >= 0 &&
                prompt->results[f_PromptChoice] < SAVING_RESULT_COUNT) {
                PlaySound(SOUND_SAVED, SAVE_SOUND_CHANNEL, PROMPT_SOUND_VOLUME);
            }
            else {
                PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, PROMPT_SOUND_VOLUME);
            }

            return NO_PROMPT_RESULT;
        }

        if (controller->pressedButtons == B_BUTTON) {
            f_PromptChoice = PROMPT_CHOICE_NO;
            f_PromptState = PROMPT_STATE_CHOSEN;
            PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, PROMPT_SOUND_VOLUME);
            return NO_PROMPT_RESULT;
        }

        if (controller->repeatedButtons & (L_JPAD | R_JPAD)) {
            f_PromptChoice ^= 1;
            PlaySound(SOUND_MENU_MOVE, g_NextSoundChannel, PROMPT_SOUND_VOLUME);
            return NO_PROMPT_RESULT;
        }
    }
    else if (f_PromptState == PROMPT_STATE_CHOSEN) {
        f_PromptState = PROMPT_STATE_CLOSING;
        PlaySound(SOUND_PROMPT_CLOSE, g_NextSoundChannel, PROMPT_SOUND_VOLUME);
        SetTextSlot(
            0,
            0,
            NO_MESSAGE,
            TEXT_SLOT_LARGE_FONT,
            PROMPT_TEXT_SLOT,
            TEXT_FLAG_TYPED,
            0,
            0,
            0,
            0
        );
    }
    else if (f_PromptState == PROMPT_STATE_CLOSED_AFTER_CHOICE) {
        f_PromptState = PROMPT_STATE_CLOSED;
        FreePromptImages();
        return prompt->results[f_PromptChoice];
    }

    return NO_PROMPT_RESULT;
}

void DrawPrompt(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    Prompt *prompt = &f_Prompts[f_PromptIndex];
    s32 textWidth;
    s32 textHeight;
    s32 cloudHeight;
    s32 halfHeight;
    s32 halfWidth;
    s32 halfIndex;
    s32 bottom;

    if (f_PromptState == PROMPT_STATE_CLOSED) {
        return;
    }

    MeasureMessage(prompt->messageId, FONT_SIZE_LARGE, &textWidth, &textHeight, 0);
    cloudHeight = textHeight + PROMPT_CLOUD_MARGIN;
    if (cloudHeight < PROMPT_CLOUD_SMALLEST_HEIGHT) {
        cloudHeight = PROMPT_CLOUD_SMALLEST_HEIGHT;
    }

    if (f_PromptState == PROMPT_STATE_OPENING) {
        f_PromptFrame++;
        if (f_PromptFrame == PROMPT_GROWTH_FRAMES) {
            f_PromptState = PROMPT_STATE_CHOOSING;
        }
    }
    else if (f_PromptState == PROMPT_STATE_CLOSING) {
        f_PromptFrame--;
        if (f_PromptFrame == 0) {
            f_PromptState = PROMPT_STATE_CLOSED_AFTER_CHOICE;
            return;
        }
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    gDPSetEnvColor(gfx++, 191, 191, 191, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineMode(gfx++, G_CC_BLENDPEDECALA, G_CC_BLENDPEDECALA);

    for (halfIndex = 0; halfIndex != 2; halfIndex++) {
        if (f_PromptState == PROMPT_STATE_CHOOSING) {
            halfWidth = PROMPT_CLOUD_HALF_WIDTH;
            halfHeight = cloudHeight;
        }
        else {
            halfWidth = f_PromptFrame * (PROMPT_CLOUD_HALF_WIDTH / PROMPT_GROWTH_FRAMES);
            halfHeight = cloudHeight * f_PromptFrame / PROMPT_GROWTH_FRAMES;
        }

        LoadNibbleTextureBlock(
            &gfx,
            AddressPast(
                f_PromptCloudImage,
                IMAGE_HEADER_SIZE +
                    (halfIndex * (PROMPT_CLOUD_WIDTH * PROMPT_CLOUD_HALF_TEXELS / 2))
            ),
            G_IM_FMT_I,
            PROMPT_CLOUD_WIDTH,
            PROMPT_CLOUD_HALF_TEXELS,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        bottom = PROMPT_CLOUD_MIDDLE_Y + (halfIndex * halfHeight);
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(PROMPT_CENTER_X - halfWidth),
            QuarterPixels(bottom - halfHeight),
            QuarterPixels(PROMPT_CENTER_X + halfWidth),
            QuarterPixels(bottom),
            G_TX_RENDERTILE,
            0,
            0,
            PROMPT_CLOUD_WIDTH / 2 * TEXEL_STEP_ONE / halfWidth,
            PROMPT_CLOUD_HALF_TEXELS * TEXEL_STEP_ONE / halfHeight
        );
    }

    if (f_PromptState == PROMPT_STATE_CHOOSING) {
        s32 unused[2] UNUSED;

        f_ChoiceBobAngle += DegreesToRadians(CHOICE_BOB_STEP_DEGREES);
        if (f_PromptChoice == PROMPT_CHOICE_YES) {
            halfWidth = (sinf(f_ChoiceBobAngle) * CHOSEN_WIDTH_BOB) + CHOSEN_HALF_WIDTH;
            halfHeight = CHOSEN_HALF_HEIGHT;
            halfHeight += CHOSEN_HEIGHT_BOB * sinf(f_ChoiceBobAngle);
        }
        else {
            halfWidth = CHOICE_HALF_WIDTH;
            halfHeight = CHOICE_HALF_HEIGHT;
        }

        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        LoadLargePalette(&gfx, ImagePalette(f_YesNoImage));
        LoadTextureBlock(
            &gfx,
            AddressPast(f_YesNoImage, YES_TEXELS_OFFSET),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            YES_NO_BUTTON_WIDTH,
            YES_NO_BUTTON_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(YES_CENTER_X - halfWidth),
            QuarterPixels(CHOICES_MIDDLE_Y - halfHeight + (cloudHeight / 4)),
            QuarterPixels(YES_CENTER_X + halfWidth),
            QuarterPixels(CHOICES_MIDDLE_Y + halfHeight + (cloudHeight / 4)),
            G_TX_RENDERTILE,
            0,
            0,
            YES_NO_BUTTON_WIDTH / 2 * TEXEL_STEP_ONE / halfWidth,
            YES_NO_BUTTON_HEIGHT / 2 * TEXEL_STEP_ONE / halfHeight
        );
        if (f_PromptChoice == PROMPT_CHOICE_NO) {
            halfWidth = (sinf(f_ChoiceBobAngle) * CHOSEN_WIDTH_BOB) + CHOSEN_HALF_WIDTH;
            halfHeight = CHOSEN_HALF_HEIGHT;
            halfHeight += CHOSEN_HEIGHT_BOB * sinf(f_ChoiceBobAngle);
        }
        else {
            halfWidth = CHOICE_HALF_WIDTH;
            halfHeight = CHOICE_HALF_HEIGHT;
        }

        LoadTextureBlock(
            &gfx,
            AddressPast(f_YesNoImage, NO_TEXELS_OFFSET),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            YES_NO_BUTTON_WIDTH,
            YES_NO_BUTTON_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(NO_CENTER_X - halfWidth),
            QuarterPixels(CHOICES_MIDDLE_Y - halfHeight + (cloudHeight / 4)),
            QuarterPixels(NO_CENTER_X + halfWidth),
            QuarterPixels(CHOICES_MIDDLE_Y + halfHeight + (cloudHeight / 4)),
            G_TX_RENDERTILE,
            0,
            0,
            YES_NO_BUTTON_WIDTH / 2 * TEXEL_STEP_ONE / halfWidth,
            YES_NO_BUTTON_HEIGHT / 2 * TEXEL_STEP_ONE / halfHeight
        );
    }

    *displayListHead = gfx;
}

void ShowHalfTurn(void) {
    EncodedAsset asset;

    if (!f_IsHalfTurnShown) {
        f_HalfTurnImage = AllocMemory(OpenEncodedAsset(ASSET_HALF_TURN_IMAGE, &asset));
        ReadEncodedAsset(f_HalfTurnImage, &asset);
    }

    f_IsHalfTurnShown = TRUE;
    f_HalfTurnState = HALF_TURN_STATE_SHOWING;
    f_HalfTurnTimer = HALF_TURN_FRAMES;
}

void HideHalfTurn(void) {
    if (f_IsHalfTurnShown) {
        FreeMemory(&f_HalfTurnImage);
    }

    f_IsHalfTurnShown = FALSE;
}

void MarkHalfTurnCircle(u32 *texelPairs, s32 centerColumn, s32 centerRow, s32 radius);

void MarkHalfTurnCircle(u32 *texelPairs, s32 centerColumn, s32 centerRow, s32 radius) {
    s32 step = 0;
    s32 error = radius;
    s32 stepOffset;
    s32 radiusRowOffset;
    s32 radiusColumnOffset;

    if (f_HalfTurnState == HALF_TURN_STATE_SHOWING) {
        while (radius >= step) {
            MarkCirclePoints(&= ~HALF_TURN_ALPHA_BITS);
        }
    }
    else {
        while (radius >= step) {
            MarkCirclePoints(|= HALF_TURN_ALPHA_BITS);
        }
    }
}

#define IsDismissPressed(controller) \
    (f_HalfTurnTimer < 0 && ((controller)->pressedButtons & A_BUTTON))

Boolean IsHalfTurnDismissed(ControllerState *controller) {
    if (f_IsHalfTurnShown &&
        f_HalfTurnState == HALF_TURN_STATE_SHOWING &&
        !IsDismissPressed(controller)) {
        return FALSE;
    }

    return TRUE;
}

void DrawHalfTurn(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 i;
    s32 stripRows;

    if (!f_IsHalfTurnShown) {
        return;
    }

    if (f_HalfTurnState == HALF_TURN_STATE_SHOWING) {
        if (f_HalfTurnTimer > 0) {
            for (i = 0; i != HALF_TURN_SHOW_STEPS; i++) {
                MarkHalfTurnCircle(
                    (u32 *)(ImageBody(f_HalfTurnImage)),
                    HALF_TURN_CENTER_COLUMN,
                    HALF_TURN_CENTER_ROW,
                    f_HalfTurnTimer
                );
                f_HalfTurnTimer--;
            }

            if (f_HalfTurnTimer == HALF_TURN_SHOW_SOUND_RADIUS) {
                PlaySoundWithEffects(
                    SOUND_PROMPT_CLOSE,
                    WIPE_SOUND_CHANNEL,
                    WIPE_SOUND_VOLUME,
                    WIPE_SOUND_REVERB,
                    SOUND_FULL_SCALE
                );
            }
        }
        else if (f_HalfTurnTimer > -HALF_TURN_WAIT_FRAMES) {
            f_HalfTurnTimer--;
        }
        else {
            f_HalfTurnState = HALF_TURN_STATE_EXPIRED;
        }
    }

    if (f_HalfTurnState == HALF_TURN_STATE_LEAVING) {
        if (f_HalfTurnTimer < 0) {
            f_HalfTurnTimer++;
        }
        else if (f_HalfTurnTimer < HALF_TURN_FRAMES) {
            for (i = 0; i != HALF_TURN_HIDE_STEPS; i++) {
                MarkHalfTurnCircle(
                    (u32 *)(ImageBody(f_HalfTurnImage)),
                    HALF_TURN_CENTER_COLUMN,
                    HALF_TURN_CENTER_ROW,
                    f_HalfTurnTimer
                );
                f_HalfTurnTimer++;
            }

            if (f_HalfTurnTimer == HALF_TURN_HIDE_SOUND_RADIUS) {
                PlaySoundWithEffects(
                    SOUND_PROMPT_OPEN,
                    WIPE_SOUND_CHANNEL,
                    WIPE_SOUND_VOLUME,
                    WIPE_SOUND_REVERB,
                    SOUND_FULL_SCALE
                );
            }
        }
        else {
            f_HalfTurnState = HALF_TURN_STATE_DONE;

            f_IsHalfTurnShown = HALF_TURN_STATE_LEAVING;
            HideHalfTurn();
            return;
        }
    }

    if (g_IsDebugEnabled) {
        sprintf(g_PrintBuffer, "%3d", (int)f_HalfTurnTimer);
        PrintDebugText(g_PrintBuffer, HALF_TURN_DEBUG_COLUMN, HALF_TURN_DEBUG_ROW);
    }

    gSPDisplayList(gfx++, g_DecalTextureSetup);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);

    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 1, TEXEL0, 1, 0, 0, 0, TEXEL0, 0, 1, TEXEL0, 1);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    for (i = 0; i != HALF_TURN_STRIP_COUNT; i++) {
        stripRows =
            (i == HALF_TURN_STRIP_COUNT - 1) ? HALF_TURN_LAST_STRIP_ROWS : HALF_TURN_STRIP_ROWS;
        gDPLoadTextureTile(
            gfx++,
            TextureAddress(ImageBody(f_HalfTurnImage) + (i * HALF_TURN_STRIP_SIZE)),
            G_IM_FMT_RGBA,
            G_IM_SIZ_16b,
            HALF_TURN_WIDTH,
            stripRows,
            0,
            0,
            HALF_TURN_WIDTH - 1,
            stripRows - 1,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(SCREEN_BORDER),
            QuarterPixels(SCREEN_BORDER + (i * HALF_TURN_STRIP_ROWS)),
            QuarterPixels(SCREEN_BORDER + HALF_TURN_WIDTH),
            QuarterPixels(SCREEN_BORDER + (i * HALF_TURN_STRIP_ROWS) + stripRows),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    *displayListHead = gfx;
}
