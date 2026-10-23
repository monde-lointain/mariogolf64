#include "common.h"
#include "asset.h"
#include "controller.h"
#include "heap.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"

/* The wipe's shapes are 64 by 64 intensity masks */
#define DEFAULT_WIPE_SHAPE 0x63F
#define WIPE_SHAPE_COUNT 14
#define WIPE_CLOSE_START_PROGRESS 12.0f
#define WIPE_OPEN_START_PROGRESS -2.0f
#define WIPE_OPEN_PROGRESS 20
#define SOUND_WIPE_CLOSE 0x64
#define SOUND_WIPE_OPEN 0x65
#define WIPE_SOUND_CHANNEL 2
#define WIPE_SOUND_VOLUME 0x28
#define WIPE_SOUND_REVERB 0x14
#define WIPE_SOUND_PAN 0x7F
/* The shape grows by this many pixels with each step of progress */
#define WIPE_SHAPE_GROWTH 38.0f
#define WIPE_SHAPE_TEXELS 64
/* The progress at which the shape's texels start at the square's corner */
#define WIPE_ALIGNED_PROGRESS 8.0f
/* Opening wipes fade out over their last steps */
#define WIPE_FADE_START_PROGRESS 10.0f
#define WIPE_FADE_ALPHA_STEP 25.4f
/* A shape smaller than this, in pixels, is drawn as a covered screen */
#define WIPE_SMALLEST_SHAPE 2
/* The square that the shape is stretched over, centred on the screen */
#define WIPE_SQUARE_SIZE 304
#define WIPE_SQUARE_LEFT SCREEN_BORDER
#define WIPE_SQUARE_TOP (SCREEN_HEIGHT / 2 - WIPE_SQUARE_SIZE / 2)

#define HALF_TURN_IMAGE 0x955
#define HALF_TURN_FRAMES 144
#define HALF_TURN_STATE_SHOWING 0
#define HALF_TURN_STATE_UNK1 1
#define HALF_TURN_STATE_LEAVING 2
#define HALF_TURN_STATE_DONE 3

#define FADE_NONE -1
#define FADE_OUT 0
#define FADE_IN 1
/* The fade's opacity runs over this many steps */
#define FADE_ALPHA_RANGE 256

#define PROMPT_CLOUD_IMAGE 0x70B
#define YES_NO_IMAGE 0x70C
#define PROMPT_STATE_CLOSED -1
#define PROMPT_STATE_OPENING 0
#define PROMPT_STATE_CHOOSING 1
#define PROMPT_STATE_CHOSEN 2
#define PROMPT_STATE_CLOSING 3
#define PROMPT_STATE_CLOSED_AFTER_CHOICE 4
#define PROMPT_CHOICE_NO 1
#define NO_PROMPT_RESULT -1
/* The results of answers that overwrite a save; they get the save's sound */
#define SAVING_RESULT_COUNT 2
#define PROMPT_TEXT_X 160
#define PROMPT_TEXT_Y 108
#define PROMPT_TEXT_SLOT 0
/* Given this as its text, func_800719A0 clears the slot */
#define NO_TEXT -1
#define SOUND_PROMPT_SAVE 0x3B
#define SAVE_SOUND_CHANNEL 15
#define SOUND_PROMPT_MOVE 0x5C
#define SOUND_PROMPT_CONFIRM 0x5D
#define SOUND_PROMPT_CLOSE 0x65
#define SOUND_PROMPT_OPEN 0x64
#define PROMPT_SOUND_VOLUME 0x64

/* A yes-or-no question; the choices index results */
typedef struct Prompt {
    /* 0x0 */ s32 messageId;
    /* 0x4 */ s32 defaultChoice;
    /* 0x8 */ s32 results[2];
} Prompt; /* size = 0x10 */

extern s32 sIsHalfTurnShown;
extern s32 sHalfTurnState;
extern s32 sHalfTurnTimer;
extern f32 sWipeProgress;
extern s32 sWipeKind;
extern s32 sWipeShapeIds[WIPE_SHAPE_COUNT];
extern s32 sScreenFadeDuration;
extern s32 sScreenFadeFramesLeft;
extern s32 sScreenFadeKind;
extern Prompt sPrompts[];
extern s32 sPromptIndex;
extern s32 sPromptFrame;
extern s32 sPromptState;
extern s32 sPromptChoice;
extern void *sHalfTurnImage;
extern void *sWipeShapeImage;
extern void *sPromptCloudImage;
extern void *sYesNoImage;
extern s32 sScreenFadeRed;
extern s32 sScreenFadeGreen;
extern s32 sScreenFadeBlue;

void func_800719A0(s32 x, s32 y, s32 image, s32, s32 slot, s32, s32, s32, s32, s32);

/* The RDP loads textures from 8-byte boundaries */
#define TEXTURE_ADDRESS(address) ((u32)(address) & ~7)

/*
 * Draws a texture rectangle between pipe syncs, cut where it reaches past
 * the screen's top or left
 */
void DrawTextureRect(Gfx **displayListHead, s32 xl, s32 yl, s32 xh, s32 yh, s32 tile, s32 s, s32 t, s32 dsdx,
                     s32 dtdy) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, xl, yl, xh, yh, tile, s, t, dsdx, dtdy);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void LoadSmallPalette(Gfx **displayListHead, s32 palette, void *tlut) {
    Gfx *gfx = *displayListHead;

    gDPLoadTLUT_pal16(gfx++, palette, TEXTURE_ADDRESS(tlut));
    *displayListHead = gfx;
}

void LoadLargePalette(Gfx **displayListHead, void *tlut) {
    Gfx *gfx = *displayListHead;

    gDPLoadTLUT_pal256(gfx++, TEXTURE_ADDRESS(tlut));
    *displayListHead = gfx;
}

void LoadNibbleTextureBlock(Gfx **displayListHead, void *texels, s32 format, s32 width, s32 height, s32 palette,
                            s32 cms, s32 cmt, s32 masks, s32 maskt, s32 shifts, s32 shiftt) {
    Gfx *gfx = *displayListHead;

    gDPLoadTextureBlock_4b(gfx++, TEXTURE_ADDRESS(texels), format, width, height, palette, cms, cmt, masks, maskt,
                           shifts, shiftt);
    *displayListHead = gfx;
}

/*
 * gDPLoadTextureBlock, but with the texel size given twice: as the constant
 * whose loading parameters are used, and as the value set in the render
 * tile
 */
#define LOAD_TEXTURE_BLOCK(pkt, timg, fmt, loadSiz, siz, width, height, pal, cms, cmt, masks, maskt, shifts,         \
                           shiftt)                                                                                   \
    {                                                                                                                \
        gDPSetTextureImage(pkt++, fmt, loadSiz##_LOAD_BLOCK, 1, TEXTURE_ADDRESS(timg));                              \
        gDPSetTile(pkt++, fmt, loadSiz##_LOAD_BLOCK, 0, 0, G_TX_LOADTILE, 0, cmt, maskt, shiftt, cms, masks,         \
                   shifts);                                                                                          \
        gDPLoadSync(pkt++);                                                                                          \
        gDPLoadBlock(pkt++, G_TX_LOADTILE, 0, 0, (((width) * (height) + loadSiz##_INCR) >> loadSiz##_SHIFT) - 1,     \
                     CALC_DXT(width, loadSiz##_BYTES));                                                              \
        gDPPipeSync(pkt++);                                                                                          \
        gDPSetTile(pkt++, fmt, siz, (((width) * loadSiz##_LINE_BYTES) + 7) >> 3, 0, G_TX_RENDERTILE, pal,            \
                   cmt, maskt, shiftt, cms, masks, shifts);                                                          \
        gDPSetTileSize(pkt++, G_TX_RENDERTILE, 0, 0, ((width) - 1) << G_TEXTURE_IMAGE_FRAC,                          \
                       ((height) - 1) << G_TEXTURE_IMAGE_FRAC);                                                      \
    }

void LoadTextureBlock(Gfx **displayListHead, void *texels, s32 format, s32 size, s32 width, s32 height,
                      s32 palette, s32 cms, s32 cmt, s32 masks, s32 maskt, s32 shifts, s32 shiftt) {
    Gfx *gfx = *displayListHead;

    if (size == G_IM_SIZ_8b) {
        LOAD_TEXTURE_BLOCK(gfx, texels, format, G_IM_SIZ_8b, size, width, height, palette, cms, cmt, masks, maskt,
                           shifts, shiftt);
    } else {
        LOAD_TEXTURE_BLOCK(gfx, texels, format, G_IM_SIZ_16b, size, width, height, palette, cms, cmt, masks, maskt,
                           shifts, shiftt);
    }
    *displayListHead = gfx;
}

/* Loads the default wipe shape and stops any fade */
void InitScreenLayers(void) {
    EncodedAsset asset;

    sWipeShapeImage = AllocMemory(OpenEncodedAsset(DEFAULT_WIPE_SHAPE, &asset));
    ReadEncodedAsset(sWipeShapeImage, &asset);
    StopScreenFade();
}

void LoadWipeShape(s32 courseIndex) {
    EncodedAsset asset;

    OpenEncodedAsset(sWipeShapeIds[courseIndex % WIPE_SHAPE_COUNT], &asset);
    ReadEncodedAsset(sWipeShapeImage, &asset);
}

void StopWipe(void) {
    sWipeKind = WIPE_NONE;
}

s32 GetWipeKind(void) {
    return sWipeKind;
}

/*
 * While the Half Turn picture is shown, it stands in for the wipe: the
 * wipe reads as open once the picture has finished, which hides it
 */
s32 GetWipeProgress(void) {
    if (!sIsHalfTurnShown) {
        return sWipeProgress;
    }
    switch (sHalfTurnState) {
        case HALF_TURN_STATE_UNK1:
        default:
            return 0;
        case HALF_TURN_STATE_DONE:
            sIsHalfTurnShown = FALSE;
            return WIPE_OPEN_PROGRESS;
    }
}

/* While the Half Turn picture is shown, an opening wipe ends it instead */
void StartWipe(s32 kind) {
    if (sIsHalfTurnShown == TRUE) {
        if (kind == WIPE_OPENING || kind == WIPE_OPENING_FAST) {
            sHalfTurnState = HALF_TURN_STATE_LEAVING;
            sHalfTurnTimer = -1;
        }
    } else {
        sWipeKind = kind;
        switch (kind) {
            case WIPE_CLOSING:
                sWipeProgress = WIPE_CLOSE_START_PROGRESS;
                PlaySoundWithEffects(SOUND_WIPE_CLOSE, WIPE_SOUND_CHANNEL, WIPE_SOUND_VOLUME, WIPE_SOUND_REVERB,
                                     WIPE_SOUND_PAN);
                break;
            case WIPE_OPENING:
            case WIPE_OPENING_FAST:
                sWipeProgress = WIPE_OPEN_START_PROGRESS;
                PlaySoundWithEffects(SOUND_WIPE_OPEN, WIPE_SOUND_CHANNEL, WIPE_SOUND_VOLUME, WIPE_SOUND_REVERB,
                                     WIPE_SOUND_PAN);
                break;
        }
    }
}

/*
 * Advances the wipe and draws it: a black screen seen through the wipe's
 * shape, which grows as the wipe opens and shrinks as it closes
 */
void DrawWipe(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 shapeSize;
    s32 shapeOffset;
    s32 alpha;
    s32 texelOffset;
    f32 alignment;

    if (sWipeKind == WIPE_NONE) {
        return;
    }
    if (sWipeKind == WIPE_CLOSING) {
        sWipeProgress -= 1.0f;
        if (sWipeProgress < 0.0f) {
            sWipeProgress = 0.0f;
        }
    } else if (sWipeKind == WIPE_OPENING || sWipeKind == WIPE_OPENING_FAST) {
        sWipeProgress += (sWipeKind == WIPE_OPENING_FAST) ? 2.0f : 1.0f;
        if (sWipeProgress > WIPE_OPEN_PROGRESS) {
            sWipeProgress = WIPE_OPEN_PROGRESS;
            sWipeKind = WIPE_NONE;
        }
    }

    shapeSize = sWipeProgress * WIPE_SHAPE_GROWTH;
    alignment = (WIPE_ALIGNED_PROGRESS - sWipeProgress) / WIPE_ALIGNED_PROGRESS;
    shapeOffset = alignment * -(WIPE_SQUARE_SIZE / 2);
    if (sWipeProgress < WIPE_FADE_START_PROGRESS) {
        alpha = 255;
    } else {
        alpha = (WIPE_OPEN_PROGRESS - sWipeProgress) * WIPE_FADE_ALPHA_STEP;
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
    gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0,
                      0);
    if (shapeSize < WIPE_SMALLEST_SHAPE) {
        gDPPipeSync(gfx++);
        gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gSPTextureRectangle(gfx++, SCREEN_BORDER << 2, SCREEN_BORDER << 2, (SCREEN_WIDTH - SCREEN_BORDER) << 2,
                            (SCREEN_HEIGHT - SCREEN_BORDER) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
                            TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    } else {
        texelOffset = (shapeOffset * WIPE_SHAPE_TEXELS << 5) / shapeSize;
        gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
        gDPLoadTextureBlock_4b(gfx++, TEXTURE_ADDRESS((u8 *)sWipeShapeImage + IMAGE_HEADER_SIZE), G_IM_FMT_I,
                               WIPE_SHAPE_TEXELS, WIPE_SHAPE_TEXELS, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                               G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, WIPE_SQUARE_LEFT << 2, WIPE_SQUARE_TOP << 2,
                                (WIPE_SQUARE_LEFT + WIPE_SQUARE_SIZE) << 2, (WIPE_SQUARE_TOP + WIPE_SQUARE_SIZE) << 2,
                                G_TX_RENDERTILE, texelOffset, texelOffset,
                                WIPE_SHAPE_TEXELS * TEXEL_STEP_ONE / (f32)shapeSize,
                                WIPE_SHAPE_TEXELS * TEXEL_STEP_ONE / (f32)shapeSize);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

void StopScreenFade(void) {
    sScreenFadeKind = FADE_NONE;
}

/* This is true from the fade's last frame on */
s32 HasScreenFadeEnded(void) {
    return sScreenFadeFramesLeft < 2;
}

void StartScreenFadeOut(s32 red, s32 green, s32 blue, s32 frameCount) {
    sScreenFadeKind = FADE_OUT;
    sScreenFadeFramesLeft = sScreenFadeDuration = frameCount;
    sScreenFadeRed = red;
    sScreenFadeGreen = green;
    sScreenFadeBlue = blue;
}

void StartScreenFadeIn(s32 red, s32 green, s32 blue, s32 frameCount) {
    sScreenFadeKind = FADE_IN;
    sScreenFadeFramesLeft = sScreenFadeDuration = frameCount;
    sScreenFadeRed = red;
    sScreenFadeGreen = green;
    sScreenFadeBlue = blue;
}

/* Covers the screen with the fade's colour, at the opacity it has reached */
#ifdef NON_MATCHING
void DrawScreenFade(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 alpha;

    if (sScreenFadeKind != FADE_NONE) {
        if (sScreenFadeKind == FADE_OUT) {
            alpha = FADE_ALPHA_RANGE - (sScreenFadeFramesLeft * FADE_ALPHA_RANGE) / sScreenFadeDuration;
        } else {
            alpha = (sScreenFadeFramesLeft * FADE_ALPHA_RANGE) / sScreenFadeDuration;
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
        gDPSetPrimColor(gfx++, 0, 0, sScreenFadeRed, sScreenFadeGreen, sScreenFadeBlue, alpha);
        gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPPipeSync(gfx++);
        gSPTextureRectangle(gfx++, SCREEN_BORDER << 2, SCREEN_BORDER << 2, (SCREEN_WIDTH - SCREEN_BORDER) << 2,
                            (SCREEN_HEIGHT - SCREEN_BORDER) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
                            TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
        sScreenFadeFramesLeft--;
        if (sScreenFadeFramesLeft == 0) {
            sScreenFadeKind = FADE_NONE;
        }
        *displayListHead = gfx;
    }
}

#else
INCLUDE_ASM("asm/nonmatchings/screen_layers", DrawScreenFade);
#endif

void LoadPromptImages(void) {
    EncodedAsset asset;

    sPromptCloudImage = AllocMemory(OpenEncodedAsset(PROMPT_CLOUD_IMAGE, &asset));
    ReadEncodedAsset(sPromptCloudImage, &asset);
    sYesNoImage = AllocMemory(OpenEncodedAsset(YES_NO_IMAGE, &asset));
    ReadEncodedAsset(sYesNoImage, &asset);
}

void FreePromptImages(void) {
    FreeMemory(&sPromptCloudImage);
    FreeMemory(&sYesNoImage);
}

s32 IsPromptClosed(void) {
    return sPromptState == PROMPT_STATE_CLOSED;
}

void OpenPrompt(s32 prompt) {
    Prompt *entry;

    if (prompt != NO_PROMPT) {
        entry = &sPrompts[prompt];
        sPromptFrame = 0;
        sPromptIndex = prompt;
        LoadPromptImages();
        sPromptState = PROMPT_STATE_OPENING;
        sPromptChoice = entry->defaultChoice;
        PlaySound(SOUND_PROMPT_OPEN, g_NextSoundChannel, PROMPT_SOUND_VOLUME);
    }
}

/*
 * Takes the player's answer to the open prompt. It returns the result of
 * the chosen answer once the prompt has closed, and NO_PROMPT_RESULT
 * until then.
 */
s32 UpdatePrompt(ControllerState *controller) {
    Prompt *prompt = &sPrompts[sPromptIndex];

    if (sPromptState == PROMPT_STATE_CHOOSING) {
        func_800719A0(PROMPT_TEXT_X, PROMPT_TEXT_Y, prompt->messageId, 1, PROMPT_TEXT_SLOT, 0x10, 0x5F, 0x3F, 0xFF,
                      0xFF);
        if (controller->pressedButtons == A_BUTTON) {
            sPromptState = PROMPT_STATE_CHOSEN;
            if (prompt->results[sPromptChoice] >= 0 && prompt->results[sPromptChoice] < SAVING_RESULT_COUNT) {
                PlaySound(SOUND_PROMPT_SAVE, SAVE_SOUND_CHANNEL, PROMPT_SOUND_VOLUME);
            } else {
                PlaySound(SOUND_PROMPT_CONFIRM, g_NextSoundChannel, PROMPT_SOUND_VOLUME);
            }
            return NO_PROMPT_RESULT;
        }
        if (controller->pressedButtons == B_BUTTON) {
            sPromptChoice = PROMPT_CHOICE_NO;
            sPromptState = PROMPT_STATE_CHOSEN;
            PlaySound(SOUND_PROMPT_CONFIRM, g_NextSoundChannel, PROMPT_SOUND_VOLUME);
            return NO_PROMPT_RESULT;
        }
        if (controller->repeatedButtons & (L_JPAD | R_JPAD)) {
            sPromptChoice ^= 1;
            PlaySound(SOUND_PROMPT_MOVE, g_NextSoundChannel, PROMPT_SOUND_VOLUME);
            return NO_PROMPT_RESULT;
        }
    } else if (sPromptState == PROMPT_STATE_CHOSEN) {
        sPromptState = PROMPT_STATE_CLOSING;
        PlaySound(SOUND_PROMPT_CLOSE, g_NextSoundChannel, PROMPT_SOUND_VOLUME);
        func_800719A0(0, 0, NO_TEXT, 1, PROMPT_TEXT_SLOT, 1, 0, 0, 0, 0);
    } else if (sPromptState == PROMPT_STATE_CLOSED_AFTER_CHOICE) {
        sPromptState = PROMPT_STATE_CLOSED;
        FreePromptImages();
        return prompt->results[sPromptChoice];
    }
    return NO_PROMPT_RESULT;
}

INCLUDE_ASM("asm/nonmatchings/screen_layers", DrawPrompt);

void ShowHalfTurn(void) {
    EncodedAsset asset;

    if (!sIsHalfTurnShown) {
        sHalfTurnImage = AllocMemory(OpenEncodedAsset(HALF_TURN_IMAGE, &asset));
        ReadEncodedAsset(sHalfTurnImage, &asset);
    }
    sIsHalfTurnShown = TRUE;
    sHalfTurnState = HALF_TURN_STATE_SHOWING;
    sHalfTurnTimer = HALF_TURN_FRAMES;
}

void HideHalfTurn(void) {
    if (sIsHalfTurnShown) {
        FreeMemory(&sHalfTurnImage);
    }
    sIsHalfTurnShown = FALSE;
}

INCLUDE_ASM("asm/nonmatchings/screen_layers", func_8006C484);

/* The picture can be dismissed with A once its time is up */
s32 IsHalfTurnDismissed(ControllerState *controller) {
    if (sIsHalfTurnShown && sHalfTurnState == HALF_TURN_STATE_SHOWING &&
        !(sHalfTurnTimer < 0 && (controller->pressedButtons & A_BUTTON))) {
        return FALSE;
    }
    return TRUE;
}

INCLUDE_ASM("asm/nonmatchings/screen_layers", DrawHalfTurn);
