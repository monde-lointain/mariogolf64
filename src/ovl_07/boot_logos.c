#include "boot_logos.h"

#include <ultra64.h>

#include "asset.h"
#include "c_language.h"
#include "camera.h"
#include "character_model.h"
#include "compiler.h"
#include "controller.h"
#include "gbi_helpers.h"
#include "hole_loader.h"
#include "main.h"
#include "memory_address.h"
#include "overlay_manager.h"
#include "scene.h"
#include "screen.h"
#include "screen_layers.h"
#include "sky_and_hud_animations.h"
#include "sound.h"
#include "text.h"
#include "transfer_pak.h"

enum BootLogo {
    BOOT_LOGO_N64,
    BOOT_LOGO_NINTENDO,
    BOOT_LOGO_CAMELOT,
    BOOT_LOGO_LEAVING_NINTENDO,
    BOOT_LOGO_LEAVING_CAMELOT
};

typedef s32 BootLogo;

#define LOGO_FRAME_COUNT 40
#define FADE_IN_FRAME_COUNT 8
#define FADE_IN_ALPHA_STEP 32
#define FADE_OUT_FIRST_FRAME 24
#define FADE_OUT_ALPHA_STEP 16

#define FadeInAlpha(frame) ((frame) * FADE_IN_ALPHA_STEP)
#define FadeOutAlpha(frame) (255 - (((frame) - FADE_OUT_FIRST_FRAME) * FADE_OUT_ALPHA_STEP))

#define NINTENDO_SOUND_FRAME 3
#define NINTENDO_SOUND_CHANNEL 15
#define CAMELOT_FRAME_LIMIT 60
#define CAMELOT_SKIP_FRAME_LIMIT 9
#define CAMELOT_LEAVE_FRAME 26
#define LEAVING_FADE_FRAMES 16

#define CAMELOT_LOGO_WIDTH 192
#define CAMELOT_LOGO_STRIP_HEIGHT 21
#define CAMELOT_LOGO_STRIP_COUNT 2
#define CAMELOT_LOGO_LEFT 64
#define CAMELOT_LOGO_TOP 98

typedef struct CamelotLogo {
    ImageHeader header;
    u16 palette[16];
    u8 texels[0x11E8];
} CamelotLogo;

#define NINTENDO_LOGO_WIDTH 188
#define NINTENDO_LOGO_STRIP_HEIGHT 16
#define NINTENDO_LOGO_STRIP_COUNT 3
#define NINTENDO_LOGO_LEFT 66
#define NINTENDO_LOGO_TOP 96

typedef struct NintendoLogo {
    ImageHeader header;
    u8 texels[0x2348];
} NintendoLogo;

#define N64_LOGO_WIDTH 128
#define N64_LOGO_STRIP_HEIGHT 8
#define N64_LOGO_STRIP_COUNT 14
#define N64_LOGO_LEFT 96
#define N64_LOGO_TOP 64

typedef struct N64Logo {
    ImageHeader header;
    u16 texels[0x7008 / 2];
} N64Logo;

#define ASSET_CAMELOT_LOGO 0x702
#define ASSET_NINTENDO_LOGO 0x6F6
#define ASSET_N64_LOGO 0x97F

static s32 f_BootLogoFrame = 0;
static BootLogo f_BootLogoStage = BOOT_LOGO_N64;
static s32 f_BootLogoUnusedWord UNUSED = 0;
static Boolean f_HasResetGameState = FALSE;

static u8 f_UnusedBufferBeforeCamelotLogo[0x20] UNUSED;
static CamelotLogo f_CamelotLogo;
static NintendoLogo f_NintendoLogo;
static N64Logo f_N64Logo;

void LoadBootLogos(void) {
    EncodedAsset asset;

    OpenEncodedAsset(ASSET_CAMELOT_LOGO, &asset);
    ReadEncodedAsset(&f_CamelotLogo, &asset);
    OpenEncodedAsset(ASSET_NINTENDO_LOGO, &asset);
    ReadEncodedAsset(&f_NintendoLogo, &asset);
    OpenEncodedAsset(ASSET_N64_LOGO, &asset);
    ReadEncodedAsset(&f_N64Logo, &asset);
    RequestSongStop();
    g_Cameras[MAIN_CAMERA].view.scissorX = SCREEN_BORDER;
    g_Cameras[MAIN_CAMERA].view.scissorY = SCREEN_BORDER;
    g_Cameras[MAIN_CAMERA].view.scissorWidth = SCREEN_WIDTH - (2 * SCREEN_BORDER);
    g_Cameras[MAIN_CAMERA].view.scissorHeight = SCREEN_HEIGHT - (2 * SCREEN_BORDER);
    g_Cameras[MAIN_CAMERA].view.unreadSettings[0] = SCREEN_HEIGHT - (2 * SCREEN_BORDER);
    g_Cameras[MAIN_CAMERA].view.unreadSettings[1] = SCREEN_HEIGHT - (2 * SCREEN_BORDER);
    g_Cameras[MAIN_CAMERA].view.viewportCenterX = SCREEN_WIDTH;
    g_Cameras[MAIN_CAMERA].view.viewportCenterY = SCREEN_HEIGHT;
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] = 0.0f;
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] = 0.0f;
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] = 0.0f;
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] = 0.0f;
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] = 0.0f;
    g_Cameras[MAIN_CAMERA].view.drawFlags = CAMERA_DRAW_SCENE;
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] = -100.0f;
    g_Cameras[MAIN_CAMERA].view.zoom = 1.0f;
    g_Cameras[MAIN_CAMERA].view.fieldOfView = 45.0f;
}

void LeaveBootLogos(void) {
    g_TitleScreenEntrance = TITLE_ENTRANCE_FROM_BOOT_LOGOS;
    if (HasGameBoyPak() && IsBootTransmitPending()) {
        ClearBootTransmitPending();
        g_Scene = SCENE_SWAPPING_MODULES;
        g_SceneModuleToUnload = MODULE_BOOT_LOGOS;
        g_NextScene = SCENE_TRANSMIT;
        g_SceneModuleToLoad = MODULE_TRANSMIT;
        ResetGameBoyTransfer(0);
        StartWipe(WIPE_OPENING);
    }
    else if (f_HasResetGameState && g_HasLoaderFinished == TRUE && g_ControllerCount != 0) {
        g_Scene = SCENE_SWAPPING_MODULES;
        g_SceneModuleToUnload = MODULE_BOOT_LOGOS;
        g_NextScene = SCENE_OPENING;
        g_SceneModuleToLoad = MODULE_OPENING;
    }
    else {
        StopLoaderThread();
        g_Scene = SCENE_SWAPPING_MODULES;
        g_SceneModuleToUnload = MODULE_BOOT_LOGOS;
        g_NextScene = SCENE_TITLE_SCREEN;
        g_SceneModuleToLoad = MODULE_TITLE_SCREEN;
    }

    SetTextSlot(CLEARED_TEXT_X, CLEARED_TEXT_Y, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 0, 0, 0, 0, 0, 0);
    SetTextSlot(CLEARED_TEXT_X, CLEARED_TEXT_Y, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 1, 0, 0, 0, 0, 0);
    SetTextSlot(CLEARED_TEXT_X, CLEARED_TEXT_Y, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 2, 0, 0, 0, 0, 0);
    SetTextSlot(CLEARED_TEXT_X, CLEARED_TEXT_Y, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 3, 0, 0, 0, 0, 0);
    SetTextSlot(CLEARED_TEXT_X, CLEARED_TEXT_Y, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 4, 0, 0, 0, 0, 0);
    SetTextSlot(CLEARED_TEXT_X, CLEARED_TEXT_Y, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, 5, 0, 0, 0, 0, 0);
}

void UpdateBootLogos(void) {
    s32 pressedButtons;
    s32 controllerIndex;

    switch (f_BootLogoStage) {
        case BOOT_LOGO_N64:
            f_BootLogoFrame++;
            if (f_BootLogoFrame == LOGO_FRAME_COUNT) {
                f_BootLogoFrame = 0;
                f_BootLogoStage = BOOT_LOGO_NINTENDO;
            }

            break;
        case BOOT_LOGO_NINTENDO:
            f_BootLogoFrame++;
            pressedButtons = 0;
            if (f_BootLogoFrame == NINTENDO_SOUND_FRAME) {
                PlaySound(SOUND_IMPACT_ON_MARK, NINTENDO_SOUND_CHANNEL, SOUND_FULL_SCALE);
            }

            for (controllerIndex = 0; controllerIndex != g_ControllerCount; controllerIndex++) {
                pressedButtons |= g_ControllerStates[controllerIndex].pressedButtons;
            }

            if ((pressedButtons & (A_BUTTON | START_BUTTON)) && g_IsDebugEnabled) {
                f_BootLogoFrame = 0;
                f_BootLogoStage = BOOT_LOGO_LEAVING_NINTENDO;
            }
            else if (f_BootLogoFrame == LOGO_FRAME_COUNT) {
                f_BootLogoFrame = 0;
                f_BootLogoStage = BOOT_LOGO_CAMELOT;
            }

            break;
        case BOOT_LOGO_CAMELOT:
            if (f_BootLogoFrame == 0) {
                if (IsCharacterSlotBusy(0)) {
                    FreeCharacterSlot(0);
                }
                else {
                    SetUpTitleHole();
                    f_HasResetGameState = TRUE;
                }
            }

            if (f_BootLogoFrame < CAMELOT_FRAME_LIMIT) {
                f_BootLogoFrame++;
            }

            if (g_IsDebugEnabled &&
                (g_ControllerStates[0].pressedButtons & A_BUTTON) &&
                f_BootLogoFrame < CAMELOT_SKIP_FRAME_LIMIT) {
                f_BootLogoStage = BOOT_LOGO_LEAVING_CAMELOT;
                f_BootLogoFrame = 0;
            }
            else if (g_HasLoaderFinished == TRUE && f_BootLogoFrame >= CAMELOT_LEAVE_FRAME) {
                if (f_BootLogoFrame > 0) {
                    f_BootLogoStage = BOOT_LOGO_LEAVING_CAMELOT;
                    f_BootLogoFrame = 0;
                }
            }

            break;
        case BOOT_LOGO_LEAVING_NINTENDO:
        case BOOT_LOGO_LEAVING_CAMELOT:
            if (f_BootLogoFrame == 0) {
                StartScreenFadeOut(0, 0, 0, LEAVING_FADE_FRAMES);
            }

            if (IsScreenFadeDone()) {
                f_BootLogoStage = BOOT_LOGO_N64;
                f_BootLogoFrame = 0;
                SetScreenEffect(SCREEN_EFFECT_NONE, 0);
                LeaveBootLogos();
            }

            f_BootLogoFrame++;
            break;
    }
}

void DrawBootLogos(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
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
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    switch (f_BootLogoStage) {
        case BOOT_LOGO_N64:
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_NONE);
            if (f_BootLogoFrame > FADE_OUT_FIRST_FRAME) {
                s32 fadeOutAlpha = MAX(FadeOutAlpha(f_BootLogoFrame), 0);

                gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, fadeOutAlpha);
            }
            else if (f_BootLogoFrame < FADE_IN_FRAME_COUNT) {
                s32 fadeInAlpha = MIN(FadeInAlpha(f_BootLogoFrame), 255);

                gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, fadeInAlpha);
            }

            gDPSetCombineLERP(
                gfx++,
                0,
                0,
                0,
                TEXEL0,
                PRIMITIVE,
                0,
                TEXEL0,
                0,
                0,
                0,
                0,
                TEXEL0,
                PRIMITIVE,
                0,
                TEXEL0,
                0
            );
            for (stripIndex = 0; stripIndex != N64_LOGO_STRIP_COUNT; stripIndex++) {
                n64Texels =
                    f_N64Logo.texels + stripIndex * (N64_LOGO_WIDTH * N64_LOGO_STRIP_HEIGHT);
                LoadTextureBlock(
                    &gfx,
                    n64Texels,
                    G_IM_FMT_RGBA,
                    G_IM_SIZ_16b,
                    N64_LOGO_WIDTH,
                    N64_LOGO_STRIP_HEIGHT,
                    0,
                    0,
                    0,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(N64_LOGO_LEFT),
                    QuarterPixels(N64_LOGO_TOP + (stripIndex * N64_LOGO_STRIP_HEIGHT)),
                    QuarterPixels(N64_LOGO_LEFT + N64_LOGO_WIDTH),
                    QuarterPixels(
                        N64_LOGO_TOP + N64_LOGO_STRIP_HEIGHT + (stripIndex * N64_LOGO_STRIP_HEIGHT)
                    ),
                    0,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
            }

            break;
        case BOOT_LOGO_NINTENDO:
        case BOOT_LOGO_LEAVING_NINTENDO:
            if (f_BootLogoStage == BOOT_LOGO_NINTENDO) {
                if (f_BootLogoFrame > FADE_OUT_FIRST_FRAME) {
                    s32 fadeOutAlpha = MAX(FadeOutAlpha(f_BootLogoFrame), 0);

                    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, fadeOutAlpha);
                }
                else if (f_BootLogoFrame < FADE_IN_FRAME_COUNT) {
                    s32 fadeInAlpha = MIN(FadeInAlpha(f_BootLogoFrame), 255);

                    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, fadeInAlpha);
                }
            }

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
            for (stripIndex = 0; stripIndex != NINTENDO_LOGO_STRIP_COUNT; stripIndex++) {
                texelOffset = stripIndex * (NINTENDO_LOGO_WIDTH * NINTENDO_LOGO_STRIP_HEIGHT);
                gDPLoadTextureTile(
                    gfx++,
                    TextureAddress(&f_NintendoLogo.texels[texelOffset]),
                    G_IM_FMT_I,
                    G_IM_SIZ_8b,
                    NINTENDO_LOGO_WIDTH,
                    NINTENDO_LOGO_STRIP_HEIGHT,
                    0,
                    0,
                    NINTENDO_LOGO_WIDTH - 1,
                    NINTENDO_LOGO_STRIP_HEIGHT - 1,
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
                    QuarterPixels(NINTENDO_LOGO_LEFT),
                    QuarterPixels(NINTENDO_LOGO_TOP + (stripIndex * NINTENDO_LOGO_STRIP_HEIGHT)),
                    QuarterPixels(NINTENDO_LOGO_LEFT + NINTENDO_LOGO_WIDTH),
                    QuarterPixels(
                        NINTENDO_LOGO_TOP + NINTENDO_LOGO_STRIP_HEIGHT +
                        (stripIndex * NINTENDO_LOGO_STRIP_HEIGHT)
                    ),
                    0,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
            }

            break;
        case BOOT_LOGO_CAMELOT:
        case BOOT_LOGO_LEAVING_CAMELOT:
            if (f_BootLogoStage == BOOT_LOGO_CAMELOT && f_BootLogoFrame < FADE_IN_FRAME_COUNT) {
                s32 fadeInAlpha = MIN(FadeInAlpha(f_BootLogoFrame), 255);

                gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, fadeInAlpha);
            }

            gDPSetCombineLERP(
                gfx++,
                0,
                0,
                0,
                TEXEL0,
                PRIMITIVE,
                0,
                TEXEL0,
                0,
                0,
                0,
                0,
                TEXEL0,
                PRIMITIVE,
                0,
                TEXEL0,
                0
            );
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_RGBA16);
            LoadSmallPalette(&gfx, 0, f_CamelotLogo.palette);
            for (stripIndex = 0; stripIndex != CAMELOT_LOGO_STRIP_COUNT; stripIndex++) {
                texelOffset = stripIndex * (CAMELOT_LOGO_WIDTH / 2 * CAMELOT_LOGO_STRIP_HEIGHT);
                gDPLoadTextureTile_4b(
                    gfx++,
                    TextureAddress(&f_CamelotLogo.texels[texelOffset]),
                    G_IM_FMT_CI,
                    CAMELOT_LOGO_WIDTH,
                    CAMELOT_LOGO_STRIP_HEIGHT,
                    0,
                    0,
                    CAMELOT_LOGO_WIDTH - 1,
                    CAMELOT_LOGO_STRIP_HEIGHT - 1,
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
                    QuarterPixels(CAMELOT_LOGO_LEFT),
                    QuarterPixels(CAMELOT_LOGO_TOP + (stripIndex * CAMELOT_LOGO_STRIP_HEIGHT)),
                    QuarterPixels(CAMELOT_LOGO_LEFT + CAMELOT_LOGO_WIDTH),
                    QuarterPixels(
                        CAMELOT_LOGO_TOP + CAMELOT_LOGO_STRIP_HEIGHT +
                        (stripIndex * CAMELOT_LOGO_STRIP_HEIGHT)
                    ),
                    0,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                gDPPipeSync(gfx++);
            }

            break;
    }

    *displayListHead = gfx;
}
