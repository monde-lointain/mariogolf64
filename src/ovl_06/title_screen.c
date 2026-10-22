#include "common.h"
#include "asset.h"
#include "controller.h"
#include "functions.h"
#include "game_flags.h"
#include "scene.h"
#include "screen.h"
#include "unlocks.h"
#include "variables.h"

/*
 * The title screen, module 0x04, which LeaveBootLogos can swap in. A or
 * Start on any controller swaps in module 0x02; otherwise it goes back to
 * the logos after a while. Without a controller, it shows a screen asking
 * for one and stays there. Button codes entered on the first controller
 * unlock parts of the save or enable debugging.
 */

/* sTitleStage values */
#define TITLE_SHOWING 0
#define TITLE_SWAPPING_MODULES 1
#define TITLE_GOING_TO_LOGOS 2

/* After this many frames, the title screen goes back to the logos */
#define TITLE_IDLE_FRAME_COUNT 630
/* sTitleFrame stops rising at this */
#define TITLE_FRAME_LIMIT 900
/* The count given to func_8006B5A0 as the screen first appears */
#define TITLE_ENTERING_COUNTDOWN 8
/* The count given to func_8006B56C; the stage ends once func_8006B55C sees it below 2 */
#define LEAVING_COUNTDOWN 16

/* The asset IDs of the images */
#define TITLE_IMAGE_ASSET 0x6F4
#define NO_CONTROLLER_IMAGE_ASSET 0x948
#define JAPANESE_LOGO_ASSET 0x6F5
#define PRESS_START_TEXT_ASSET 0x943

/* 304 by 224 RGBA16 texels, either the title or the screen asking for a controller */
#define TITLE_IMAGE_WIDTH (SCREEN_WIDTH - 2 * SCREEN_BORDER)
#define TITLE_IMAGE_HEIGHT (SCREEN_HEIGHT - 2 * SCREEN_BORDER)
#define TITLE_STRIP_HEIGHT 6
#define TITLE_STRIP_COUNT 38
#define TITLE_LAST_STRIP_HEIGHT 2

typedef struct TitleImage {
    /* 0x0 */ ImageHeader header;
    /* 0x8 */ u16 texels[TITLE_IMAGE_WIDTH * TITLE_IMAGE_HEIGHT];
} TitleImage; /* size = 0x21408 */

/* マリオゴルフ64 ("Mario Golf 64"), 288 by 80 RGBA16 texels; read in, but never drawn here */
#define JAPANESE_LOGO_WIDTH 288
#define JAPANESE_LOGO_HEIGHT 80

typedef struct JapaneseLogo {
    /* 0x0 */ ImageHeader header;
    /* 0x8 */ u16 texels[JAPANESE_LOGO_WIDTH * JAPANESE_LOGO_HEIGHT];
} JapaneseLogo; /* size = 0xB408 */

/* "PRESS START", 120 by 12 four-bit intensities */
#define PRESS_START_WIDTH 120
#define PRESS_START_HEIGHT 12
#define PRESS_START_LEFT 100
#define PRESS_START_TOP 188

typedef struct PressStartText {
    /* 0x0 */ ImageHeader header;
    /* 0x8 */ u8 texels[PRESS_START_WIDTH * PRESS_START_HEIGHT / 2];
} PressStartText; /* size = 0x2E0 */

/* A texture rectangle step of one texel per pixel, in 5.10 fixed point */
#define TEXEL_STEP_ONE (1 << 10)

#define DEGREES_TO_RADIANS(degrees) ((degrees) * (M_PI / 180))

/* ButtonCode.progress once the code has been entered, after which it is ignored */
#define CODE_ENTERED (-1)
/* This entry of sButtonCodes is only read while the first controller holds Z, L and R */
#define DEBUG_CODE_INDEX 3
#define DEBUG_CODE_HELD_BUTTONS (Z_TRIG | L_TRIG | R_TRIG)

/*
 * A sequence of buttons to press on the first controller. Any other button
 * pressed starts the sequence over.
 */
typedef struct ButtonCode {
    /* 0x0 */ char *sequence; /* One letter for each button; see UpdateButtonCodes */
    /* 0x4 */ s32 progress;   /* How many of the buttons have been pressed */
    /* 0x8 */ void (*onEntered)(void);
} ButtonCode; /* size = 0xC */

#define GAME_FLAG_UNK44 0x44
#define GAME_FLAG_UNK65 0x65

extern TitleImage sTitleImage;
extern JapaneseLogo sJapaneseLogo;
extern PressStartText sPressStartText;

extern s32 D_800B6808;

void func_80050BF0(s32, s32, s32);
void func_80051C08(s32, s32);
void func_8005DC50(void);
void func_8006A2C0(Gfx **displayListHead, s32 xl, s32 yl, s32 xh, s32 yh, s32 tile, s32 s, s32 t, s32 dsdx,
                   s32 dtdy);
s32 func_8006B55C(void);
void func_8006B56C(s32, s32, s32, s32);
void func_8006B5A0(s32, s32, s32, s32);
void func_8006F50C(void);
void func_8009226C(s32);
s32 func_800959F8(void);

void OnUnlockCodeEntered(void);
void EnableDebug(void);

/* The frames since the title screen appeared */
static s32 sTitleFrame = 0;
static s32 sTitleStage = TITLE_SHOWING;
static ButtonCode sButtonCodes[] = {
    { "AABAUUDULRlr", 0, OnUnlockCodeEntered },
    { "DDLLLRRRddlllrrr", 0, OnUnlockCodeEntered },
    { "UUDUDDUDLLRLRRLR", 0, GrantEveryTournamentPlace },
    { "BBuududdudllrlrrlrBBlr", 0, EnableDebug },
};
/* Never read or written */
s32 g_TitleUnusedWord = 0;
/* "PRESS START" turns from red to green and back as this goes round */
static f32 sPressStartPhase = 0.0f;

void OnUnlockCodeEntered(void) {
    UnlockEverything();
}

void EnableDebug(void) {
    g_IsDebugEnabled = TRUE;
}

/* No code calls this */
void SetUnknownGameFlags(void) {
    SetGameFlag(GAME_FLAG_UNK44);
    SetGameFlag(GAME_FLAG_UNK65);
}

void ResetButtonCodes(void) {
    u32 codeIndex;

    for (codeIndex = 0; codeIndex < ARRAY_COUNT(sButtonCodes); codeIndex++) {
        sButtonCodes[codeIndex].progress = 0;
    }
}

/*
 * Each letter of a sequence names a button: 'U', 'D', 'L' and 'R' those of
 * the control pad, and their lower cases the C buttons. 'A' is matched by
 * the B button, the same as 'B'; A itself starts the game.
 */
void UpdateButtonCodes(void) {
    u32 codeIndex;
    s32 progress;
    s32 isMatched;

    for (codeIndex = 0; codeIndex < ARRAY_COUNT(sButtonCodes); codeIndex++) {
        progress = sButtonCodes[codeIndex].progress;
        isMatched = FALSE;
        if (progress == CODE_ENTERED) {
            continue;
        }
        if ((g_ControllerData[0].button & DEBUG_CODE_HELD_BUTTONS) != DEBUG_CODE_HELD_BUTTONS) {
            if (codeIndex == DEBUG_CODE_INDEX) {
                continue;
            }
        }
        switch (sButtonCodes[codeIndex].sequence[progress]) {
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
        if (sButtonCodes[codeIndex].sequence[progress] == '\0') {
            progress = CODE_ENTERED;
            sButtonCodes[codeIndex].onEntered();
        }
        sButtonCodes[codeIndex].progress = progress;
    }
}

void LoadTitleScreen(void) {
    EncodedAsset asset;

    ResetButtonCodes();
    func_8006F50C();
    func_8005DC50();
    if (g_ControllerCount == 0) {
        OpenEncodedAsset(NO_CONTROLLER_IMAGE_ASSET, &asset);
        ReadEncodedAsset(&sTitleImage, &asset);
        func_80051C08(5, 0x80);
    } else {
        OpenEncodedAsset(TITLE_IMAGE_ASSET, &asset);
        ReadEncodedAsset(&sTitleImage, &asset);
        func_80051C08(0x2A, 0x80);
    }
    OpenEncodedAsset(JAPANESE_LOGO_ASSET, &asset);
    ReadEncodedAsset(&sJapaneseLogo, &asset);
    OpenEncodedAsset(PRESS_START_TEXT_ASSET, &asset);
    ReadEncodedAsset(&sPressStartText, &asset);
    sTitleStage = TITLE_SHOWING;
}

void UnloadTitleScreen(void) {
    func_8006F50C();
    SetFillColor(0, 0, 0);
}

/* With g_IsDebugEnabled set, B also starts going back to the logos */
void UpdateTitleScreen(void) {
    s32 pressedButtons;
    s32 controllerIndex;

    if (g_ControllerCount == 0) {
        return;
    }
    UpdateButtonCodes();
    if (sTitleFrame == 0) {
        /* func_8006B5A0 is given white when D_800B6808 is 1, else black */
        if (D_800B6808 == 1) {
            func_8006B5A0(255, 255, 255, TITLE_ENTERING_COUNTDOWN);
        } else {
            func_8006B5A0(0, 0, 0, TITLE_ENTERING_COUNTDOWN);
        }
        D_800B6808 = 3;
    }
    switch (sTitleStage) {
        case TITLE_SHOWING:
            if (func_8006B55C() && func_800959F8()) {
                pressedButtons = 0;
                for (controllerIndex = 0; controllerIndex != g_ControllerCount; controllerIndex++) {
                    pressedButtons |= g_ControllerStates[controllerIndex].pressedButtons;
                }
                if (pressedButtons & (A_BUTTON | START_BUTTON)) {
                    func_8009226C(10);
                    g_Scene = SCENE_SWAPPING_MODULES;
                    g_NextScene = 14;
                    g_SceneModuleToUnload = 0x04;
                    g_SceneModuleToLoad = 0x02;
                    sTitleFrame = 0;
                    sTitleStage = TITLE_SWAPPING_MODULES;
                    func_80050BF0(0x5D, 0xF, 0x7F);
                } else if (((pressedButtons & B_BUTTON) && g_IsDebugEnabled)
                           || sTitleFrame == TITLE_IDLE_FRAME_COUNT) {
                    sTitleStage = TITLE_GOING_TO_LOGOS;
                    func_8006B56C(0, 0, 0, LEAVING_COUNTDOWN);
                }
            }
            break;
        case TITLE_GOING_TO_LOGOS:
            if (func_8006B55C()) {
                g_Scene = SCENE_SWAPPING_MODULES;
                g_NextScene = 27;
                g_SceneModuleToUnload = 0x04;
                g_SceneModuleToLoad = 0x0C;
                sTitleFrame = 0;
            }
            break;
    }
    if (sTitleFrame < TITLE_FRAME_LIMIT) {
        sTitleFrame++;
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
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
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
        stripHeight = (stripIndex == TITLE_STRIP_COUNT - 1) ? TITLE_LAST_STRIP_HEIGHT : TITLE_STRIP_HEIGHT;
        gDPLoadTextureTile(gfx++,
                           (u32)&sTitleImage.texels[stripIndex * TITLE_STRIP_HEIGHT * TITLE_IMAGE_WIDTH] & ~7,
                           G_IM_FMT_RGBA, G_IM_SIZ_16b, TITLE_IMAGE_WIDTH, 0, 0, 0, TITLE_IMAGE_WIDTH - 1,
                           stripHeight - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                           G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, SCREEN_BORDER << 2, (SCREEN_BORDER + stripIndex * TITLE_STRIP_HEIGHT) << 2,
                                (SCREEN_BORDER + TITLE_IMAGE_WIDTH) << 2,
                                (SCREEN_BORDER + stripIndex * TITLE_STRIP_HEIGHT + stripHeight) << 2,
                                G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    }
    if (g_ControllerCount != 0) {
        sPressStartPhase += DEGREES_TO_RADIANS(20);
        red = sinf(sPressStartPhase) * 127.0f + 127.0f;
        green = sinf(sPressStartPhase + M_PI) * 127.0f + 127.0f;
        gDPSetPrimColor(gfx++, 0, 0, red, green, 64, 255);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
        gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, 0, TEXEL0, 0, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE,
                          0, TEXEL0, 0);
        gDPLoadTextureTile_4b(gfx++, (u32)sPressStartText.texels & ~7, G_IM_FMT_I, PRESS_START_WIDTH,
                              PRESS_START_HEIGHT, 0, 0, PRESS_START_WIDTH - 1, PRESS_START_HEIGHT - 1, 0,
                              G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                              G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        func_8006A2C0(&gfx, PRESS_START_LEFT << 2, PRESS_START_TOP << 2, (PRESS_START_LEFT + PRESS_START_WIDTH) << 2,
                      (PRESS_START_TOP + PRESS_START_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
                      TEXEL_STEP_ONE);
    }
    *displayListHead = gfx;
}
