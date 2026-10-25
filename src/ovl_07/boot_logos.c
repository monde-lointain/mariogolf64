#include "common.h"
#include "asset.h"
#include "camera.h"
#include "character_slot.h"
#include "controller.h"
#include "functions.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "scene.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"
#include "transfer_pak.h"
#include "variables.h"

/*
 * The logos shown once ovl_08's confirm screen swaps in this module: the
 * Nintendo 64 logo, then Nintendo's, then Camelot's. The first two fade in
 * and out through the primitive alpha; Camelot's only fades in.
 */

/* sBootLogoStage values */
#define BOOT_LOGO_N64 0
#define BOOT_LOGO_NINTENDO 1
#define BOOT_LOGO_CAMELOT 2
#define BOOT_LOGO_LEAVING_NINTENDO 3
#define BOOT_LOGO_LEAVING_CAMELOT 4

/* The frames that the first two logos stay for */
#define LOGO_FRAME_COUNT 40
/* A logo fades in over this many frames... */
#define FADE_IN_FRAME_COUNT 8
#define FADE_IN_ALPHA_STEP 32
/* ...and fades out after this frame */
#define FADE_OUT_FIRST_FRAME 24
#define FADE_OUT_ALPHA_STEP 16

#define FADE_IN_ALPHA(frame) ((frame) * FADE_IN_ALPHA_STEP)
#define FADE_OUT_ALPHA(frame) (255 - ((frame) - FADE_OUT_FIRST_FRAME) * FADE_OUT_ALPHA_STEP)

/* The Nintendo logo's sound plays on this frame */
#define NINTENDO_SOUND_FRAME 3
/* The Camelot logo's frame count stops rising at this */
#define CAMELOT_FRAME_LIMIT 60
/* With g_IsDebugEnabled set, A skips the Camelot logo before this frame */
#define CAMELOT_SKIP_FRAME_LIMIT 9
/* The Camelot logo leaves from this frame, once g_HasLoaderFinished is set */
#define CAMELOT_LEAVE_FRAME 26
/* The count given to StartScreenFadeOut; the stage ends once HasScreenFadeEnded sees it below 2 */
#define LEAVING_COUNTDOWN 16

/* 192 by 42 four-bit texels, read as 96-byte rows */
#define CAMELOT_LOGO_WIDTH 192
#define CAMELOT_LOGO_STRIP_HEIGHT 21
#define CAMELOT_LOGO_STRIP_COUNT 2
#define CAMELOT_LOGO_LEFT 64
#define CAMELOT_LOGO_TOP 98

typedef struct CamelotLogo {
    /* 0x00 */ ImageHeader header;
    /* 0x08 */ u16 palette[16];
    /* 0x28 */ u8 texels[0x11E8];
} CamelotLogo; /* size = 0x1210 */

/* 188 by 48 eight-bit intensities */
#define NINTENDO_LOGO_WIDTH 188
#define NINTENDO_LOGO_STRIP_HEIGHT 16
#define NINTENDO_LOGO_STRIP_COUNT 3
#define NINTENDO_LOGO_LEFT 66
#define NINTENDO_LOGO_TOP 96

typedef struct NintendoLogo {
    /* 0x0 */ ImageHeader header;
    /* 0x8 */ u8 texels[0x2348];
} NintendoLogo; /* size = 0x2350 */

/* 128 by 112 RGBA16 texels */
#define N64_LOGO_WIDTH 128
#define N64_LOGO_STRIP_HEIGHT 8
#define N64_LOGO_STRIP_COUNT 14
#define N64_LOGO_LEFT 96
#define N64_LOGO_TOP 64

typedef struct N64Logo {
    /* 0x0 */ ImageHeader header;
    /* 0x8 */ u16 texels[0x7008 / 2];
} N64Logo; /* size = 0x7010 */

/* The asset IDs of the logos */
#define CAMELOT_LOGO_ASSET 0x702
#define NINTENDO_LOGO_ASSET 0x6F6
#define N64_LOGO_ASSET 0x97F

extern CamelotLogo sCamelotLogo;
extern NintendoLogo sNintendoLogo;
extern N64Logo sN64Logo;

extern s32 D_800B6808;


/* The frames since the stage began */
static s32 sBootLogoFrame = 0;
static s32 sBootLogoStage = BOOT_LOGO_N64;
/* Never read or written */
s32 g_BootLogoUnusedWord = 0;
/* Whether SetUpTitleHole has reset g_GameState during the Camelot logo */
static s32 sHasResetGameState = FALSE;

void LoadBootLogos(void) {
    EncodedAsset asset;

    OpenEncodedAsset(CAMELOT_LOGO_ASSET, &asset);
    ReadEncodedAsset(&sCamelotLogo, &asset);
    OpenEncodedAsset(NINTENDO_LOGO_ASSET, &asset);
    ReadEncodedAsset(&sNintendoLogo, &asset);
    OpenEncodedAsset(N64_LOGO_ASSET, &asset);
    ReadEncodedAsset(&sN64Logo, &asset);
    RequestSongStop();
    g_Cameras[0].view.scissorX = SCREEN_BORDER;
    g_Cameras[0].view.scissorY = SCREEN_BORDER;
    g_Cameras[0].view.scissorWidth = SCREEN_WIDTH - 2 * SCREEN_BORDER;
    g_Cameras[0].view.scissorHeight = SCREEN_HEIGHT - 2 * SCREEN_BORDER;
    g_Cameras[0].view.unreadSettings[0] = SCREEN_HEIGHT - 2 * SCREEN_BORDER;
    g_Cameras[0].view.unreadSettings[1] = SCREEN_HEIGHT - 2 * SCREEN_BORDER;
    g_Cameras[0].view.viewportCenterX = SCREEN_WIDTH;
    g_Cameras[0].view.viewportCenterY = SCREEN_HEIGHT;
    g_Cameras[0].view.eye[0] = 0.0f;
    g_Cameras[0].view.eye[1] = 0.0f;
    g_Cameras[0].view.eye[2] = 0.0f;
    g_Cameras[0].view.lookAt[0] = 0.0f;
    g_Cameras[0].view.lookAt[1] = 0.0f;
    g_Cameras[0].view.drawFlags = CAMERA_DRAW_SCENE;
    g_Cameras[0].view.lookAt[2] = -100.0f;
    g_Cameras[0].view.zoom = 1.0f;
    g_Cameras[0].view.fieldOfView = 45.0f;
}

/* Swaps this module for the next scene's and clears the image slots */
void LeaveBootLogos(void) {
    D_800B6808 = 2;
    if (HasGameBoyPak() && IsBootTransmitPending()) {
        ClearBootTransmitPending();
        g_Scene = SCENE_SWAPPING_MODULES;
        g_SceneModuleToUnload = 0x0C;
        g_NextScene = 29;
        g_SceneModuleToLoad = 0x1B;
        ResetGameBoyTransfer(0);
        StartWipe(WIPE_OPENING);
    } else if (sHasResetGameState && g_HasLoaderFinished == 1 && g_ControllerCount != 0) {
        g_Scene = SCENE_SWAPPING_MODULES;
        g_SceneModuleToUnload = 0x0C;
        g_NextScene = 19;
        g_SceneModuleToLoad = 0x03;
    } else {
        StopLoaderThread();
        g_Scene = SCENE_SWAPPING_MODULES;
        g_SceneModuleToUnload = 0x0C;
        g_NextScene = 21;
        g_SceneModuleToLoad = 0x04;
    }
    /* The position goes unused with NO_MESSAGE */
    SetTextSlot(160, 114, NO_MESSAGE, 1, 0, 0, 0, 0, 0, 0);
    SetTextSlot(160, 114, NO_MESSAGE, 1, 1, 0, 0, 0, 0, 0);
    SetTextSlot(160, 114, NO_MESSAGE, 1, 2, 0, 0, 0, 0, 0);
    SetTextSlot(160, 114, NO_MESSAGE, 1, 3, 0, 0, 0, 0, 0);
    SetTextSlot(160, 114, NO_MESSAGE, 1, 4, 0, 0, 0, 0, 0);
    SetTextSlot(160, 114, NO_MESSAGE, 1, 5, 0, 0, 0, 0, 0);
}

/*
 * With g_IsDebugEnabled set, A or Start on any controller ends the Nintendo
 * logo early, and so does A on the first one during the Camelot logo's first
 * frames
 */
void UpdateBootLogos(void) {
    s32 pressedButtons;
    s32 controllerIndex;

    switch (sBootLogoStage) {
        case BOOT_LOGO_N64:
            sBootLogoFrame++;
            if (sBootLogoFrame == LOGO_FRAME_COUNT) {
                sBootLogoFrame = 0;
                sBootLogoStage = BOOT_LOGO_NINTENDO;
            }
            break;
        case BOOT_LOGO_NINTENDO:
            sBootLogoFrame++;
            pressedButtons = 0;
            if (sBootLogoFrame == NINTENDO_SOUND_FRAME) {
                PlaySound(0x62, 0xF, 0x7F);
            }
            for (controllerIndex = 0; controllerIndex != g_ControllerCount; controllerIndex++) {
                pressedButtons |= g_ControllerStates[controllerIndex].pressedButtons;
            }
            if ((pressedButtons & (A_BUTTON | START_BUTTON)) && g_IsDebugEnabled) {
                sBootLogoFrame = 0;
                sBootLogoStage = BOOT_LOGO_LEAVING_NINTENDO;
            } else if (sBootLogoFrame == LOGO_FRAME_COUNT) {
                sBootLogoFrame = 0;
                sBootLogoStage = BOOT_LOGO_CAMELOT;
            }
            break;
        case BOOT_LOGO_CAMELOT:
            if (sBootLogoFrame == 0) {
                if (IsCharacterSlotBusy(0)) {
                    FreeCharacterSlot(0);
                } else {
                    SetUpTitleHole();
                    sHasResetGameState = TRUE;
                }
            }
            if (sBootLogoFrame < CAMELOT_FRAME_LIMIT) {
                sBootLogoFrame++;
            }
            if (g_IsDebugEnabled && (g_ControllerStates[0].pressedButtons & A_BUTTON)
                && sBootLogoFrame < CAMELOT_SKIP_FRAME_LIMIT) {
                sBootLogoStage = BOOT_LOGO_LEAVING_CAMELOT;
                sBootLogoFrame = 0;
            } else if (g_HasLoaderFinished == 1 && sBootLogoFrame >= CAMELOT_LEAVE_FRAME) {
                if (sBootLogoFrame > 0) {
                    sBootLogoStage = BOOT_LOGO_LEAVING_CAMELOT;
                    sBootLogoFrame = 0;
                }
            }
            break;
        case BOOT_LOGO_LEAVING_NINTENDO:
        case BOOT_LOGO_LEAVING_CAMELOT:
            if (sBootLogoFrame == 0) {
                StartScreenFadeOut(0, 0, 0, LEAVING_COUNTDOWN);
            }
            if (HasScreenFadeEnded()) {
                sBootLogoStage = BOOT_LOGO_N64;
                sBootLogoFrame = 0;
                SetScreenEffect(SCREEN_EFFECT_NONE, 0);
                LeaveBootLogos();
            }
            sBootLogoFrame++;
            break;
    }
}

#ifdef NON_MATCHING
void DrawBootLogos(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 alpha;
    s32 stripIndex;
    s32 top;
    s32 bottom;
    s32 texelOffset;
    u16 *n64Texels;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPPipeSync(gfx++);
    switch (sBootLogoStage) {
        case BOOT_LOGO_N64:
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_NONE);
            if (sBootLogoFrame > FADE_OUT_FIRST_FRAME) {
                gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, MAX(FADE_OUT_ALPHA(sBootLogoFrame), 0));
            } else if (sBootLogoFrame < FADE_IN_FRAME_COUNT) {
                gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, MIN(FADE_IN_ALPHA(sBootLogoFrame), 255));
            }
            gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, TEXEL0, PRIMITIVE, 0, TEXEL0,
                              0);
            n64Texels = sN64Logo.texels;
            top = N64_LOGO_TOP << 2;
            bottom = (N64_LOGO_TOP + N64_LOGO_STRIP_HEIGHT) << 2;
            for (stripIndex = 0; stripIndex != N64_LOGO_STRIP_COUNT; stripIndex++) {
                LoadTextureBlock(&gfx, n64Texels, G_IM_FMT_RGBA, G_IM_SIZ_16b, N64_LOGO_WIDTH, N64_LOGO_STRIP_HEIGHT, 0,
                              0, 0, 0, 0, 0, 0);
                DrawTextureRect(&gfx, N64_LOGO_LEFT << 2, top, (N64_LOGO_LEFT + N64_LOGO_WIDTH) << 2, bottom, 0, 0, 0,
                              TEXEL_STEP_ONE, TEXEL_STEP_ONE);
                bottom += N64_LOGO_STRIP_HEIGHT << 2;
                top += N64_LOGO_STRIP_HEIGHT << 2;
                n64Texels += N64_LOGO_WIDTH * N64_LOGO_STRIP_HEIGHT;
            }
            break;
        case BOOT_LOGO_NINTENDO:
        case BOOT_LOGO_LEAVING_NINTENDO:
            if (sBootLogoStage == BOOT_LOGO_NINTENDO) {
                if (sBootLogoFrame > FADE_OUT_FIRST_FRAME) {
                    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, MAX(FADE_OUT_ALPHA(sBootLogoFrame), 0));
                } else if (sBootLogoFrame < FADE_IN_FRAME_COUNT) {
                    alpha = FADE_IN_ALPHA(sBootLogoFrame);
                    if (alpha > 255) {
                        alpha = 255;
                    }
                    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, alpha);
                }
            }
            gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0,
                              TEXEL0, 0);
            texelOffset = 0;
            top = NINTENDO_LOGO_TOP << 2;
            bottom = (NINTENDO_LOGO_TOP + NINTENDO_LOGO_STRIP_HEIGHT) << 2;
            for (stripIndex = 0; stripIndex != NINTENDO_LOGO_STRIP_COUNT; stripIndex++) {
                gDPLoadTextureTile(gfx++, (u32)&sNintendoLogo.texels[texelOffset] & ~7, G_IM_FMT_I, G_IM_SIZ_8b,
                                   NINTENDO_LOGO_WIDTH, NINTENDO_LOGO_STRIP_HEIGHT, 0, 0, NINTENDO_LOGO_WIDTH - 1,
                                   NINTENDO_LOGO_STRIP_HEIGHT - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
                DrawTextureRect(&gfx, NINTENDO_LOGO_LEFT << 2, top, (NINTENDO_LOGO_LEFT + NINTENDO_LOGO_WIDTH) << 2,
                              bottom, 0, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
                bottom += NINTENDO_LOGO_STRIP_HEIGHT << 2;
                top += NINTENDO_LOGO_STRIP_HEIGHT << 2;
                texelOffset += NINTENDO_LOGO_WIDTH * NINTENDO_LOGO_STRIP_HEIGHT;
            }
            break;
        case BOOT_LOGO_CAMELOT:
        case BOOT_LOGO_LEAVING_CAMELOT:
            if (sBootLogoStage == BOOT_LOGO_CAMELOT && sBootLogoFrame < FADE_IN_FRAME_COUNT) {
                alpha = FADE_IN_ALPHA(sBootLogoFrame);
                if (alpha > 255) {
                    alpha = 255;
                }
                gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, alpha);
            }
            gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, TEXEL0, PRIMITIVE, 0, TEXEL0,
                              0);
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_RGBA16);
            LoadSmallPalette(&gfx, 0, sCamelotLogo.palette);
            texelOffset = 0;
            top = CAMELOT_LOGO_TOP << 2;
            bottom = (CAMELOT_LOGO_TOP + CAMELOT_LOGO_STRIP_HEIGHT) << 2;
            for (stripIndex = 0; stripIndex != CAMELOT_LOGO_STRIP_COUNT; stripIndex++) {
                gDPLoadTextureTile_4b(gfx++, (u32)&sCamelotLogo.texels[texelOffset] & ~7, G_IM_FMT_CI,
                                      CAMELOT_LOGO_WIDTH, CAMELOT_LOGO_STRIP_HEIGHT, 0, 0, CAMELOT_LOGO_WIDTH - 1,
                                      CAMELOT_LOGO_STRIP_HEIGHT - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
                                      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
                DrawTextureRect(&gfx, CAMELOT_LOGO_LEFT << 2, top, (CAMELOT_LOGO_LEFT + CAMELOT_LOGO_WIDTH) << 2,
                              bottom, 0, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
                bottom += CAMELOT_LOGO_STRIP_HEIGHT << 2;
                top += CAMELOT_LOGO_STRIP_HEIGHT << 2;
                texelOffset += CAMELOT_LOGO_WIDTH / 2 * CAMELOT_LOGO_STRIP_HEIGHT;
                gDPPipeSync(gfx++);
            }
            break;
    }
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_07/boot_logos", DrawBootLogos);
#endif
