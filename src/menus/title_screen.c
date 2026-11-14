#include "menus/title_screen.h"

#include <ultra64.h>

#include "audio/sound.h"
#include "controllers/controller.h"
#include "controllers/transfer_pak.h"
#include "debug/game_flags.h"
#include "math/math_util.h"
#include "menus/menu.h"
#include "menus/unlocks.h"
#include "scene.h"
#include "screen/display_list.h"
#include "screen/rcp_init.h"
#include "screen/screen.h"
#include "screen/screen_layers.h"
#include "screen/sky_and_hud_animations.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/main.h"
#include "system/memory_address.h"
#include "system/overlay_manager.h"

enum TitleState {
    TITLE_SHOWING,
    TITLE_SWAPPING_MODULES,
    TITLE_GOING_TO_LOGOS
};

typedef s32 TitleState;

#define TITLE_IDLE_FRAME_COUNT 630
#define TITLE_FRAME_LIMIT 900
#define TITLE_FADE_IN_FRAMES 8
#define LEAVING_FADE_FRAMES 16

#define ASSET_TITLE_IMAGE 0x6F4
#define ASSET_NO_CONTROLLER_IMAGE 0x948
#define ASSET_JAPANESE_LOGO 0x6F5
#define ASSET_PRESS_START_TEXT 0x943

#define TITLE_IMAGE_WIDTH (SCREEN_WIDTH - (2 * SCREEN_BORDER))
#define TITLE_IMAGE_HEIGHT (SCREEN_HEIGHT - (2 * SCREEN_BORDER))
#define TITLE_STRIP_HEIGHT 6
#define TITLE_STRIP_COUNT 38
#define TITLE_LAST_STRIP_HEIGHT 2

typedef struct TitleImage {
    ImageHeader header;
    u16 texels[TITLE_IMAGE_WIDTH * TITLE_IMAGE_HEIGHT];
} TitleImage;

#define JAPANESE_LOGO_WIDTH 288
#define JAPANESE_LOGO_HEIGHT 80

typedef struct JapaneseLogo {
    ImageHeader header;
    u16 texels[JAPANESE_LOGO_WIDTH * JAPANESE_LOGO_HEIGHT];
} JapaneseLogo;

#define PRESS_START_WIDTH 120
#define PRESS_START_HEIGHT 12
#define PRESS_START_LEFT 100
#define PRESS_START_TOP 188

typedef struct PressStartText {
    ImageHeader header;
    u8 texels[PRESS_START_WIDTH * PRESS_START_HEIGHT / 2];
} PressStartText;

#define CODE_ENTERED (-1)
#define DEBUG_CODE_INDEX 3
#define DEBUG_CODE_HELD_BUTTONS (Z_TRIG | L_TRIG | R_TRIG)

typedef struct ButtonCode {
    char *sequence;
    s32 progress;
    void (*onEntered)(void);
} ButtonCode;

void OnUnlockCodeEntered(void);
void EnableDebug(void);

static s32 f_TitleFrame = 0;
static TitleState f_TitleStage = TITLE_SHOWING;
static ButtonCode f_ButtonCodes[] = {
    {"AABAUUDULRlr", 0, OnUnlockCodeEntered},
    {"DDLLLRRRddlllrrr", 0, OnUnlockCodeEntered},
    {"UUDUDDUDLLRLRRLR", 0, GrantEveryTournamentPlace},
    {"BBuududdudllrlrrlrBBlr", 0, EnableDebug},
};
static s32 f_TitleUnusedWord UNUSED = 0;
static f32 f_PressStartPhase = 0.0f;

static u8 f_UnusedBufferAfterStartPhase[0x20] UNUSED;
static TitleImage f_TitleImage;
static u8 f_UnusedBytesAfterTitleImage[8] UNUSED;
static u8 f_UnusedBufferAfterTitleImage[0x9800] UNUSED;

static JapaneseLogo f_JapaneseLogo;

static u8 f_UnusedBytesAfterJapaneseLogo[8] UNUSED;

static PressStartText f_PressStartText;

void OnUnlockCodeEntered(void) {
    UnlockEverything();
}

void EnableDebug(void) {
    g_IsDebugEnabled = TRUE;
}

void ShowStaffRollAfterHole(void) {
    SetGameFlag(GAME_FLAG_END_ROUND_AFTER_HOLE);
    SetGameFlag(GAME_FLAG_SHOW_STAFF_ROLL);
}

void ResetButtonCodes(void) {
    u32 codeIndex;

    for (codeIndex = 0; codeIndex < ARRAY_COUNT(f_ButtonCodes); codeIndex++) {
        f_ButtonCodes[codeIndex].progress = 0;
    }
}

void UpdateButtonCodes(void) {
    u32 codeIndex;
    s32 progress;
    Boolean isMatched;

    for (codeIndex = 0; codeIndex < ARRAY_COUNT(f_ButtonCodes); codeIndex++) {
        progress = f_ButtonCodes[codeIndex].progress;
        isMatched = FALSE;
        if (progress == CODE_ENTERED) {
            continue;
        }

        if ((g_ControllerData[0].button & DEBUG_CODE_HELD_BUTTONS) != DEBUG_CODE_HELD_BUTTONS) {
            if (codeIndex == DEBUG_CODE_INDEX) {
                continue;
            }
        }

        switch (f_ButtonCodes[codeIndex].sequence[progress]) {
            case 'A':
                if (g_ControllerStates[0].pressedButtons & B_BUTTON) {
                    isMatched = TRUE;
                }

                break;
            case 'B':
                if (g_ControllerStates[0].pressedButtons & B_BUTTON) {
                    isMatched = TRUE;
                }

                break;
            case 'L':
                if (g_ControllerStates[0].pressedButtons & L_JPAD) {
                    isMatched = TRUE;
                }

                break;
            case 'R':
                if (g_ControllerStates[0].pressedButtons & R_JPAD) {
                    isMatched = TRUE;
                }

                break;
            case 'U':
                if (g_ControllerStates[0].pressedButtons & U_JPAD) {
                    isMatched = TRUE;
                }

                break;
            case 'D':
                if (g_ControllerStates[0].pressedButtons & D_JPAD) {
                    isMatched = TRUE;
                }

                break;
            case 'l':
                if (g_ControllerStates[0].pressedButtons & L_CBUTTONS) {
                    isMatched = TRUE;
                }

                break;
            case 'r':
                if (g_ControllerStates[0].pressedButtons & R_CBUTTONS) {
                    isMatched = TRUE;
                }

                break;
            case 'u':
                if (g_ControllerStates[0].pressedButtons & U_CBUTTONS) {
                    isMatched = TRUE;
                }

                break;
            case 'd':
                if (g_ControllerStates[0].pressedButtons & D_CBUTTONS) {
                    isMatched = TRUE;
                }

                break;
        }

        if (g_ControllerStates[0].pressedButtons != 0 && !isMatched) {
            progress = 0;
        }

        progress += isMatched;
        if (f_ButtonCodes[codeIndex].sequence[progress] == '\0') {
            progress = CODE_ENTERED;
            f_ButtonCodes[codeIndex].onEntered();
        }

        f_ButtonCodes[codeIndex].progress = progress;
    }
}

void LoadTitleScreen(void) {
    EncodedAsset asset;

    ResetButtonCodes();
    ForceStopRumble();
    EnterMenuAtTitle();
    if (g_ControllerCount == 0) {
        OpenEncodedAsset(ASSET_NO_CONTROLLER_IMAGE, &asset);
        ReadEncodedAsset(&f_TitleImage, &asset);
        PlaySong(SONG_HOLE_OUT_OVER_BOGEY, SONG_FLAG_RESTART);
    }
    else {
        OpenEncodedAsset(ASSET_TITLE_IMAGE, &asset);
        ReadEncodedAsset(&f_TitleImage, &asset);
        PlaySong(SONG_ATTRACT_TITLE, SONG_FLAG_RESTART);
    }

    OpenEncodedAsset(ASSET_JAPANESE_LOGO, &asset);
    ReadEncodedAsset(&f_JapaneseLogo, &asset);
    OpenEncodedAsset(ASSET_PRESS_START_TEXT, &asset);
    ReadEncodedAsset(&f_PressStartText, &asset);
    f_TitleStage = TITLE_SHOWING;
}

void UnloadTitleScreen(void) {
    ForceStopRumble();
    SetFillColor(0, 0, 0);
}

#define IsLeavingForLogos(pressedButtons) \
    ((((pressedButtons) & B_BUTTON) && g_IsDebugEnabled) || \
        (f_TitleFrame == TITLE_IDLE_FRAME_COUNT))

void UpdateTitleScreen(void) {
    s32 pressedButtons;
    s32 controllerIndex;

    if (g_ControllerCount == 0) {
        return;
    }

    UpdateButtonCodes();
    if (f_TitleFrame == 0) {
        if (g_TitleScreenEntrance == TITLE_ENTRANCE_FROM_OPENING) {
            StartScreenFadeIn(255, 255, 255, TITLE_FADE_IN_FRAMES);
        }
        else {
            StartScreenFadeIn(0, 0, 0, TITLE_FADE_IN_FRAMES);
        }

        g_TitleScreenEntrance = TITLE_ENTRANCE_SHOWN;
    }

    switch (f_TitleStage) {
        case TITLE_SHOWING:
            if (IsScreenFadeDone() && IsHudAnimationDone()) {
                pressedButtons = 0;
                for (controllerIndex = 0; controllerIndex != g_ControllerCount; controllerIndex++) {
                    pressedButtons |= g_ControllerStates[controllerIndex].pressedButtons;
                }

                if (pressedButtons & (A_BUTTON | START_BUTTON)) {
                    StartHudAnimation(HUD_ANIMATION_SPIRAL);
                    g_Scene = SCENE_SWAPPING_MODULES;
                    g_NextScene = SCENE_MENU;
                    g_SceneModuleToUnload = MODULE_TITLE_SCREEN;
                    g_SceneModuleToLoad = MODULE_MENU;
                    f_TitleFrame = 0;
                    f_TitleStage = TITLE_SWAPPING_MODULES;
                    PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
                }
                else if (IsLeavingForLogos(pressedButtons)) {
                    f_TitleStage = TITLE_GOING_TO_LOGOS;
                    StartScreenFadeOut(0, 0, 0, LEAVING_FADE_FRAMES);
                }
            }

            break;
        case TITLE_GOING_TO_LOGOS:
            if (IsScreenFadeDone()) {
                g_Scene = SCENE_SWAPPING_MODULES;
                g_NextScene = SCENE_BOOT_LOGOS;
                g_SceneModuleToUnload = MODULE_TITLE_SCREEN;
                g_SceneModuleToLoad = MODULE_BOOT_LOGOS;
                f_TitleFrame = 0;
            }

            break;
    }

    if (f_TitleFrame < TITLE_FRAME_LIMIT) {
        f_TitleFrame++;
    }
}

void DrawTitleScreen(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripHeight;
    s32 red;
    s32 green;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
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
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 255, 255, 255, 1);
    gDPPipeSync(gfx++);
    for (stripIndex = 0; stripIndex != TITLE_STRIP_COUNT; stripIndex++) {
        stripHeight =
            (stripIndex == TITLE_STRIP_COUNT - 1) ? TITLE_LAST_STRIP_HEIGHT : TITLE_STRIP_HEIGHT;
        gDPLoadTextureTile(
            gfx++,
            (u32)&f_TitleImage.texels[stripIndex * TITLE_STRIP_HEIGHT * TITLE_IMAGE_WIDTH] & ~7,
            G_IM_FMT_RGBA,
            G_IM_SIZ_16b,
            TITLE_IMAGE_WIDTH,
            0,
            0,
            0,
            TITLE_IMAGE_WIDTH - 1,
            stripHeight - 1,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(SCREEN_BORDER),
            QuarterPixels(SCREEN_BORDER + (stripIndex * TITLE_STRIP_HEIGHT)),
            QuarterPixels(SCREEN_BORDER + TITLE_IMAGE_WIDTH),
            QuarterPixels(SCREEN_BORDER + (stripIndex * TITLE_STRIP_HEIGHT) + stripHeight),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    if (g_ControllerCount != 0) {
        f_PressStartPhase += DegreesToRadians(20);
        red = (sinf(f_PressStartPhase) * 127.0f) + 127.0f;
        green = (sinf(f_PressStartPhase + M_PI) * 127.0f) + 127.0f;
        gDPSetPrimColor(gfx++, 0, 0, red, green, 64, OPAQUE_ALPHA);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
        gDPSetCombineLERP(
            gfx++,
            TEXEL0,
            0,
            PRIMITIVE,
            0,
            PRIMITIVE,
            0,
            TEXEL0,
            0,
            TEXEL0,
            0,
            PRIMITIVE,
            0,
            PRIMITIVE,
            0,
            TEXEL0,
            0
        );
        gDPLoadTextureTile_4b(
            gfx++,
            TextureAddress(f_PressStartText.texels),
            G_IM_FMT_I,
            PRESS_START_WIDTH,
            PRESS_START_HEIGHT,
            0,
            0,
            PRESS_START_WIDTH - 1,
            PRESS_START_HEIGHT - 1,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(PRESS_START_LEFT),
            QuarterPixels(PRESS_START_TOP),
            QuarterPixels(PRESS_START_LEFT + PRESS_START_WIDTH),
            QuarterPixels(PRESS_START_TOP + PRESS_START_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    *displayListHead = gfx;
}
