#include "common.h"
#include "asset.h"
#include "controller.h"
#include "menu.h"
#include "menu_memory.h"
#include "save.h"
#include "screen.h"
#include "screen_layers.h"
#include "text.h"
#include "variables.h"

extern s32 g_MenuWindowState;
/* How far the window has opened across and down, from 0 to 1 */
extern f32 g_MenuWindowScaleX;
extern f32 g_MenuWindowScaleY;
extern s32 D_80230E9C_ovl_01;
extern s32 D_80230EA0_ovl_01;
/* The images that the window can show, and the buffer that holds the one it shows */
extern u16 g_MenuWindowImageAssets[15];
extern void *g_MenuWindowImage;

extern s32 D_80230F68_ovl_01;
extern u16 D_800C2074[];
extern void *D_80230E70_ovl_01;
extern void *D_80230E74_ovl_01;
extern void *D_80230E78_ovl_01;
extern void *D_80230E7C_ovl_01;
extern void *D_80230E84_ovl_01;
/* The table of images that func_802269AC picks from */
/* The intensity image of D_80230E70 that func_802287EC draws, in two strips */
#define TOURNAMENT_BANNER_WIDTH 256
#define TOURNAMENT_BANNER_STRIP_HEIGHT 32
#define TOURNAMENT_BANNER_STRIP_COUNT 2

#define TOURNAMENT_IMAGE_ROW_COUNT 15
#define TOURNAMENT_IMAGE_COLUMN_COUNT 4

extern s16 D_80230EC4_ovl_01[TOURNAMENT_IMAGE_ROW_COUNT][TOURNAMENT_IMAGE_COLUMN_COUNT];
extern s16 D_80230F34_ovl_01;
extern u8 D_80230F40_ovl_01;
extern s32 D_80230F5C_ovl_01;
extern s32 D_80230F60_ovl_01;
extern s32 D_80230F64_ovl_01;
extern s32 D_80230F70_ovl_01;
extern s32 D_80230F74_ovl_01;
extern s32 D_80230F78_ovl_01;
extern s32 D_80230F7C_ovl_01;
extern u8 D_80230F80_ovl_01;
extern void *D_80230F44_ovl_01;
extern void *D_80230F48_ovl_01;
/* The "CODE ENTRY" bar of D_80230F48 that func_8022A090 draws, in 8-row strips */
#define CODE_ENTRY_BAR_WIDTH 256
#define CODE_ENTRY_BAR_STRIP_HEIGHT 8
#define CODE_ENTRY_BAR_STRIP_COUNT 4
#define CODE_ENTRY_BAR_LEFT 32
#define CODE_ENTRY_BAR_TOP 140
extern void *D_80230F4C_ovl_01;
extern void *D_80230F50_ovl_01;
extern void *D_80230F54_ovl_01;
/* The codes that enter club tournaments: how many there are, and their length */
#define CLUB_TOURNAMENT_CODE_COUNT 36
#define CLUB_TOURNAMENT_CODE_LENGTH 8

extern u8 D_80230FD4_ovl_01[CLUB_TOURNAMENT_CODE_LENGTH];
/* What the club tournament code prompt shows */
#define CLUB_CODE_STATE_ENTERING 0
#define CLUB_CODE_STATE_INCORRECT 1

/* "Please enter a code.", which "The code entered is incorrect." follows */
#define MESSAGE_ENTER_CODE 0x165

/* The keyboard that the club tournament code is typed on, and its keys that are not characters */
#define CODE_KEYBOARD_ROW_COUNT 4
#define CODE_KEYBOARD_COLUMN_COUNT 10
#define CODE_KEYBOARD_END_ROW 3
#define CODE_KEYBOARD_END_COLUMN 9
#define CODE_KEY_BLANK 0xFD
#define CODE_KEY_BACK 0xFE
#define CODE_KEY_END 0xFF

/* What func_8022B46C reports after a key is pressed */
#define CODE_KEY_RESULT_TOO_SHORT -1
#define CODE_KEY_RESULT_TYPING 0
#define CODE_KEY_RESULT_DONE 1

/* Where the entered code is drawn, and the value of its characters not yet entered */
#define CLUB_CODE_FIRST_X 97
#define CLUB_CODE_Y 150
#define CLUB_CODE_CHAR_WIDTH 24
#define CLUB_CODE_NO_CHAR 0xFF

/* Where the prompt's two messages are drawn */
#define CLUB_CODE_PROMPT_X 56
#define CLUB_CODE_INCORRECT_X 24
#define CLUB_CODE_MESSAGE_Y 180

extern s32 D_80230F6C_ovl_01;
extern u8 D_80230F84_ovl_01[CODE_KEYBOARD_ROW_COUNT][CODE_KEYBOARD_COLUMN_COUNT];
extern u8 D_80230FDC_ovl_01[CLUB_TOURNAMENT_CODE_COUNT][CLUB_TOURNAMENT_CODE_LENGTH];
extern s8 D_80105DBC[5];
extern s32 D_80105DC8;
extern s32 D_80105DCC;
/* The first club tournament's name; D_80105DC8 picks the tournament */
#define MESSAGE_FIRST_CLUB_TOURNAMENT_NAME 0x16F
#define CLUB_TOURNAMENT_BOARD_KIND 11
/* The club tournament's details window and the text in it */
#define CLUB_WINDOW_WIDTH 280
#define CLUB_WINDOW_HEIGHT 180
#define CLUB_WINDOW_CENTER_X 160
#define CLUB_WINDOW_CENTER_Y 120
#define CLUB_WINDOW_STYLE_PLAIN 0
#define CLUB_WINDOW_STYLE_NINTENDO 1
#define CLUB_WINDOW_STYLE_PICTURE 2
#define CLUB_NUMBER_Y 68
#define CLUB_TEXT_X 160
#define CLUB_NAME_Y 94
#define CLUB_COURSE_Y 112
#define CLUB_MODE_Y 128
#define CLUB_NAME_SLOT 2
#define CLUB_COURSE_SLOT 4
#define CLUB_MODE_SLOT 5

s32 IsNintendoClubTournament(void);
s32 GetClubTournamentCourseTitleMessage(void);
s32 GetClubTournamentModeMessage(void);
void func_801FB54C_ovl_01(Gfx **displayListHead, s32);
void func_8022747C_ovl_01(Gfx **displayListHead, s32, s32, s32, s32, s32, s32, s32);
void func_802296C4_ovl_01(Gfx **displayListHead, s32, s32, s32);
void func_8022A43C_ovl_01(Gfx **displayListHead);
void func_8022A8F4_ovl_01(Gfx **displayListHead);
void func_8022ADE8_ovl_01(Gfx **displayListHead);
void func_8022B360_ovl_01(Gfx **displayListHead);
void func_8022B668_ovl_01(Gfx **displayListHead);
void func_8022B7E0_ovl_01(Gfx **displayListHead);
s32 func_8022BDC4_ovl_01(void);

void ResetMenuWindow(void) {
    g_MenuWindowState = MENU_WINDOW_OPENING;
    g_MenuWindowScaleX = 0.0f;
    g_MenuWindowScaleY = 0.0f;
    D_80230E9C_ovl_01 = 0;
    D_80230EA0_ovl_01 = 0;
}

void ShowMenuWindow(void) {
    D_80230E9C_ovl_01 = 0;
    D_80230EA0_ovl_01 = 0;
    g_MenuWindowState = MENU_WINDOW_OPEN;
    g_MenuWindowScaleX = 1.0f;
    g_MenuWindowScaleY = 1.0f;
}

void CloseMenuWindow(void) {
    g_MenuWindowState = MENU_WINDOW_CLOSING;
}

s32 IsMenuWindowOpen(void) {
    s32 isOpen;

    if (g_MenuWindowState == MENU_WINDOW_OPENING) {
        isOpen = -1;
    } else {
        isOpen = g_MenuWindowState == MENU_WINDOW_OPEN;
    }
    return isOpen;
}

s32 IsMenuWindowClosed(void) {
    s32 isClosed;

    if (g_MenuWindowState == MENU_WINDOW_CLOSING) {
        isClosed = -1;
    } else {
        isClosed = g_MenuWindowState == MENU_WINDOW_CLOSED;
    }
    return isClosed;
}

void func_802267B0_ovl_01(void *buffer, s16 bufferKind) {
    EncodedAsset asset;
    AssetFile file;

    func_8005AF74();
    switch (bufferKind) {
        case 1:
            D_80230E70_ovl_01 = buffer;
            OpenEncodedAsset(0x70B, &asset);
            ReadEncodedAsset(D_80230E70_ovl_01, &asset);
            break;
        case 2:
            D_80230E74_ovl_01 = buffer;
            OpenEncodedAsset(0x70C, &asset);
            ReadEncodedAsset(D_80230E74_ovl_01, &asset);
            break;
        case 3:
            D_80230E78_ovl_01 = buffer;
            OpenEncodedAsset(0x949, &asset);
            ReadEncodedAsset(D_80230E78_ovl_01, &asset);
            break;
        case 4:
            D_80230E7C_ovl_01 = buffer;
            OpenEncodedAsset(D_800C2074[0], &asset);
            ReadEncodedAsset(D_80230E7C_ovl_01, &asset);
            break;
        case 5:
            g_MenuWindowImage = buffer;
            OpenEncodedAsset(g_MenuWindowImageAssets[ARRAY_COUNT(g_MenuWindowImageAssets) - 1], &asset);
            ReadEncodedAsset(g_MenuWindowImage, &asset);
            break;
        case 6:
            D_80230E84_ovl_01 = buffer;
            OpenAssetFile(D_80230F34_ovl_01, &file);
            ReadAssetFile(D_80230E84_ovl_01, (s16)GetAssetFileSize(&file), &file);
            break;
    }
    ResetMenuWindow();
}

const s32 D_80231A68_ovl_01[2] = { 0, 0 };

void func_80226904_ovl_01(s32 imageIndex) {
    EncodedAsset asset;

    if (imageIndex < 0) {
        imageIndex = 0;
    }
    if (imageIndex >= 10) {
        imageIndex = 9;
    }
    OpenEncodedAsset(D_800C2074[imageIndex], &asset);
    ReadEncodedAsset(D_80230E7C_ovl_01, &asset);
}

void LoadMenuWindowImage(s32 imageIndex) {
    EncodedAsset asset;

    if (imageIndex < 0) {
        imageIndex = 0;
    }
    if (imageIndex >= ARRAY_COUNT(g_MenuWindowImageAssets)) {
        imageIndex = ARRAY_COUNT(g_MenuWindowImageAssets) - 1;
    }
    OpenEncodedAsset(g_MenuWindowImageAssets[imageIndex], &asset);
    ReadEncodedAsset(g_MenuWindowImage, &asset);
}

void func_802269AC_ovl_01(s32 row, s32 column) {
    AssetFile file;

    if (row < 0) {
        row = 0;
    }
    if (row >= TOURNAMENT_IMAGE_ROW_COUNT) {
        row = TOURNAMENT_IMAGE_ROW_COUNT - 1;
    }
    if (column < 0) {
        column = 0;
    }
    if (column >= TOURNAMENT_IMAGE_COLUMN_COUNT) {
        column = TOURNAMENT_IMAGE_COLUMN_COUNT - 1;
    }
    OpenAssetFile(D_80230EC4_ovl_01[row][column], &file);
    ReadAssetFile(D_80230E84_ovl_01, (s16)GetAssetFileSize(&file), &file);
}

void func_80226A34_ovl_01(void) {
    GetSaveData();
    func_8005AF74();
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_80226A58_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022747C_ovl_01);

/* Doesn't match: the original keeps the strip height constant in a saved register instead of the step's numerator */
#ifdef NON_MATCHING
/* The original never stores the list's end back through displayListHead */
void func_802287EC_ovl_01(Gfx **displayListHead, s32 centerX, s32 centerY, s32 halfWidth, s32 stripHeight,
                          s32 width, s32 shownStripHeight) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop = centerY - stripHeight;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(gfx++, 191, 191, 191, 255);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineMode(gfx++, G_CC_BLENDPEDECALA, G_CC_BLENDPEDECALA);
    for (stripIndex = 0; stripIndex != TOURNAMENT_BANNER_STRIP_COUNT; stripIndex++) {
        LoadNibbleTextureBlock(&gfx,
                               (u8 *)D_80230E70_ovl_01 + IMAGE_HEADER_SIZE +
                                   stripIndex * TOURNAMENT_BANNER_WIDTH * TOURNAMENT_BANNER_STRIP_HEIGHT / 2,
                               G_IM_FMT_I, TOURNAMENT_BANNER_WIDTH, TOURNAMENT_BANNER_STRIP_HEIGHT, 0, 0, 0, 0, 0,
                               0, 0);
        DrawTextureRect(&gfx, (centerX - halfWidth) << 2, stripTop << 2, (centerX + halfWidth) << 2,
                        (stripTop + stripHeight) << 2, G_TX_RENDERTILE, 0, 0,
                        (TOURNAMENT_BANNER_WIDTH << 10) / width,
                        (TOURNAMENT_BANNER_STRIP_HEIGHT << 10) / shownStripHeight);
        stripTop += stripHeight;
        gDPPipeSync(gfx++);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_802287EC_ovl_01);
#endif

void func_80228A80_ovl_01(void *buffer, s32 bufferKind) {
    EncodedAsset asset;

    switch (bufferKind) {
        case 1:
            D_80230F44_ovl_01 = buffer;
            OpenEncodedAsset(0x521, &asset);
            ReadEncodedAsset(D_80230F44_ovl_01, &asset);
            break;
        case 2:
            D_80230F48_ovl_01 = buffer;
            OpenEncodedAsset(0x93E, &asset);
            ReadEncodedAsset(D_80230F48_ovl_01, &asset);
            break;
        case 3:
            D_80230F4C_ovl_01 = buffer;
            OpenEncodedAsset(0x942, &asset);
            ReadEncodedAsset(D_80230F4C_ovl_01, &asset);
            break;
        case 4:
            D_80230F50_ovl_01 = buffer;
            OpenEncodedAsset(0x70F, &asset);
            ReadEncodedAsset(D_80230F50_ovl_01, &asset);
            break;
        case 5:
            D_80230F54_ovl_01 = buffer;
            OpenEncodedAsset(0x6EF, &asset);
            ReadEncodedAsset(D_80230F54_ovl_01, &asset);
            break;
    }
}

void func_80228B60_ovl_01(void) {
    MenuMemory *memory = func_8005AF74();
    s32 charIndex;

    memory->unk47 = 0;
    memory->unk48 = 0;
    D_80230F74_ovl_01 = 0;
    for (charIndex = 0; charIndex != CLUB_TOURNAMENT_CODE_LENGTH; charIndex++) {
        D_80230FD4_ovl_01[charIndex] = CLUB_CODE_NO_CHAR;
    }
    D_80230F68_ovl_01 = 0;
    D_80230F6C_ovl_01 = CLUB_CODE_STATE_ENTERING;
    D_80230F5C_ovl_01 = 0;
    D_80230F60_ovl_01 = 0;
    D_80230F64_ovl_01 = 0;
    D_80230F78_ovl_01 = 0;
    D_80230F70_ovl_01 = 0;
    D_80230F80_ovl_01 = 0;
    D_80230F40_ovl_01 = TRUE;
    D_80230F7C_ovl_01 = 0;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_80228C00_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_80229278_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_802296C4_ovl_01);

void func_80229CB0_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 width = 0;
    s32 height = 0;
    s32 windowStyle;
    u16 buttons;

    func_801FB54C_ovl_01(&gfx, CLUB_TOURNAMENT_BOARD_KIND);
    func_8022A43C_ovl_01(&gfx);
    func_8022A090_ovl_01(&gfx);
    func_8022ADE8_ovl_01(&gfx);
    func_8022B360_ovl_01(&gfx);
    func_8022A8F4_ovl_01(&gfx);
    func_8022B7E0_ovl_01(&gfx);
    func_8022B668_ovl_01(&gfx);
    if (D_80230F70_ovl_01 == 1) {
        if (IsNintendoClubTournament()) {
            windowStyle = (func_8022BDC4_ovl_01() != -1) ? CLUB_WINDOW_STYLE_PICTURE | CLUB_WINDOW_STYLE_NINTENDO
                                                          : CLUB_WINDOW_STYLE_NINTENDO;
            /* The held C buttons pick which of the tournament's pictures shows */
            if (windowStyle == (CLUB_WINDOW_STYLE_PICTURE | CLUB_WINDOW_STYLE_NINTENDO) && GetWipeKind() == WIPE_NONE) {
                buttons = g_ControllerStates[0].buttons | g_ControllerStates[1].buttons |
                          g_ControllerStates[2].buttons | g_ControllerStates[3].buttons;
                if (buttons & L_CBUTTONS) {
                    func_802269AC_ovl_01(func_8022BDC4_ovl_01(), 1);
                } else if (buttons & D_CBUTTONS) {
                    func_802269AC_ovl_01(func_8022BDC4_ovl_01(), 2);
                } else if (buttons & R_CBUTTONS) {
                    func_802269AC_ovl_01(func_8022BDC4_ovl_01(), 3);
                } else {
                    func_802269AC_ovl_01(func_8022BDC4_ovl_01(), 0);
                }
            }
            func_8022747C_ovl_01(&gfx, CLUB_WINDOW_WIDTH, CLUB_WINDOW_HEIGHT, CLUB_WINDOW_CENTER_X,
                                 CLUB_WINDOW_CENTER_Y, 0, D_80230F80_ovl_01, windowStyle);
            func_802296C4_ovl_01(&gfx, CLUB_TEXT_X, CLUB_NUMBER_Y, D_80105DCC);
        } else {
            windowStyle = (func_8022BDC4_ovl_01() != -1) ? CLUB_WINDOW_STYLE_PICTURE : CLUB_WINDOW_STYLE_PLAIN;
            if (windowStyle != CLUB_WINDOW_STYLE_PLAIN && GetWipeKind() == WIPE_NONE) {
                buttons = g_ControllerStates[0].buttons | g_ControllerStates[1].buttons |
                          g_ControllerStates[2].buttons | g_ControllerStates[3].buttons;
                if (buttons & L_CBUTTONS) {
                    func_802269AC_ovl_01(func_8022BDC4_ovl_01(), 1);
                } else if (buttons & D_CBUTTONS) {
                    func_802269AC_ovl_01(func_8022BDC4_ovl_01(), 2);
                } else if (buttons & R_CBUTTONS) {
                    func_802269AC_ovl_01(func_8022BDC4_ovl_01(), 3);
                } else {
                    func_802269AC_ovl_01(func_8022BDC4_ovl_01(), 0);
                }
            }
            func_8022747C_ovl_01(&gfx, CLUB_WINDOW_WIDTH, CLUB_WINDOW_HEIGHT, CLUB_WINDOW_CENTER_X,
                                 CLUB_WINDOW_CENTER_Y, 0, D_80230F80_ovl_01, windowStyle);
            func_802296C4_ovl_01(&gfx, CLUB_TEXT_X, CLUB_NUMBER_Y, D_80105DCC);
        }
        if (IsMenuWindowOpen() != -1 && !IsMenuWindowClosed()) {
            MeasureMessage(D_80105DC8 + MESSAGE_FIRST_CLUB_TOURNAMENT_NAME, FONT_SIZE_LARGE, &width, &height, 0);
            MeasureMessage(GetClubTournamentCourseTitleMessage(), FONT_SIZE_LARGE, &width, &height, 0);
            MeasureMessage(GetClubTournamentModeMessage(), FONT_SIZE_LARGE, &width, &height, 0);
            SetTextSlot(CLUB_TEXT_X, CLUB_NAME_Y, D_80105DC8 + MESSAGE_FIRST_CLUB_TOURNAMENT_NAME, 1, CLUB_NAME_SLOT,
                        TEXT_FLAG_CENTERED, 95, 63, 255, 255);
            SetTextSlot(CLUB_TEXT_X, CLUB_COURSE_Y, GetClubTournamentCourseTitleMessage(), 1, CLUB_COURSE_SLOT,
                        TEXT_FLAG_CENTERED, 43, 255, 75, 255);
            SetTextSlot(CLUB_TEXT_X, CLUB_MODE_Y, GetClubTournamentModeMessage(), 1, CLUB_MODE_SLOT,
                        TEXT_FLAG_CENTERED, 255, 63, 127, 255);
        }
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void func_8022A090_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_80230F48_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
    gDPPipeSync(gfx++);
    for (stripIndex = 0; stripIndex != CODE_ENTRY_BAR_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)D_80230F48_ovl_01 + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * CODE_ENTRY_BAR_WIDTH * CODE_ENTRY_BAR_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, CODE_ENTRY_BAR_WIDTH, CODE_ENTRY_BAR_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, CODE_ENTRY_BAR_LEFT << 2,
                                (CODE_ENTRY_BAR_TOP + stripIndex * CODE_ENTRY_BAR_STRIP_HEIGHT) << 2,
                                (CODE_ENTRY_BAR_LEFT + CODE_ENTRY_BAR_WIDTH) << 2,
                                (CODE_ENTRY_BAR_TOP + stripIndex * CODE_ENTRY_BAR_STRIP_HEIGHT +
                                 CODE_ENTRY_BAR_STRIP_HEIGHT)
                                    << 2,
                                G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022A43C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022A8F4_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022ADE8_ovl_01);

void func_8022B360_ovl_01(Gfx **displayListHead) {
    s32 charIndex;
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    for (charIndex = 0; charIndex != CLUB_TOURNAMENT_CODE_LENGTH; charIndex++) {
        if (D_80230FD4_ovl_01[charIndex] != CLUB_CODE_NO_CHAR) {
            sprintf(g_PrintBuffer, "%c", D_80230FD4_ovl_01[charIndex]);
            DrawLargeTextWithShadow(&gfx, charIndex * CLUB_CODE_CHAR_WIDTH + CLUB_CODE_FIRST_X, CLUB_CODE_Y,
                                    g_PrintBuffer, -1, -1, 0);
        }
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* Doesn't match: the menu memory pointer and the result take each other's registers */
#ifdef NON_MATCHING
s32 func_8022B46C_ovl_01(void) {
    MenuMemory *memory = func_8005AF74();
    s32 result = CODE_KEY_RESULT_TYPING;
    u8 key = D_80230F84_ovl_01[memory->unk48][memory->unk47];

    switch (key) {
        case CODE_KEY_BACK:
            if (D_80230F68_ovl_01 > 0) {
                if (D_80230F68_ovl_01 == CLUB_TOURNAMENT_CODE_LENGTH) {
                    D_80230F68_ovl_01 = CLUB_TOURNAMENT_CODE_LENGTH - 1;
                }
                D_80230FD4_ovl_01[D_80230F68_ovl_01] = CLUB_CODE_NO_CHAR;
                D_80230F68_ovl_01--;
            } else {
                D_80230FD4_ovl_01[0] = CLUB_CODE_NO_CHAR;
            }
            break;
        case CODE_KEY_BLANK:
            break;
        case CODE_KEY_END:
            if (D_80230F68_ovl_01 < CLUB_TOURNAMENT_CODE_LENGTH) {
                return CODE_KEY_RESULT_TOO_SHORT;
            }
            result = CODE_KEY_RESULT_DONE;
            break;
        default:
            if (D_80230F68_ovl_01 < CLUB_TOURNAMENT_CODE_LENGTH) {
                D_80230FD4_ovl_01[D_80230F68_ovl_01] = key;
                D_80230F68_ovl_01++;
                if (D_80230F68_ovl_01 == CLUB_TOURNAMENT_CODE_LENGTH) {
                    memory->unk47 = CODE_KEYBOARD_END_COLUMN;
                    memory->unk48 = CODE_KEYBOARD_END_ROW;
                }
            } else {
                D_80230FD4_ovl_01[D_80230F68_ovl_01 - 1] = key;
            }
            break;
    }
    return result;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022B46C_ovl_01);
#endif

s32 func_8022B5E0_ovl_01(void) {
    s32 isEmpty = TRUE;

    if (D_80230F68_ovl_01 > 0) {
        if (D_80230F68_ovl_01 == 8) {
            D_80230F68_ovl_01 = 7;
        }
        D_80230FD4_ovl_01[D_80230F68_ovl_01] = 0xFF;
        D_80230F68_ovl_01--;
        isEmpty = FALSE;
    } else if (D_80230FD4_ovl_01[0] != 0xFF) {
        D_80230FD4_ovl_01[0] = 0xFF;
        isEmpty = FALSE;
    }
    return isEmpty;
}

void func_8022B668_ovl_01(Gfx **displayListHead) {
    s32 x = 0;
    Gfx *gfx = *displayListHead;
    s32 messageOffset;
    s32 flags;

    switch (D_80230F6C_ovl_01) {
        case CLUB_CODE_STATE_ENTERING:
            messageOffset = 0;
            x = CLUB_CODE_PROMPT_X;
            flags = !IsMenuWindowOpen() ? TEXT_FLAG_UNK20 | TEXT_FLAG_TYPED : TEXT_FLAG_UNK20;
            break;
        case CLUB_CODE_STATE_INCORRECT:
            messageOffset = 1;
            x = CLUB_CODE_INCORRECT_X;
            flags = TEXT_FLAG_UNK20 | TEXT_FLAG_TYPED;
            break;
        default:
            messageOffset = -1;
            flags = TEXT_FLAG_UNK20 | TEXT_FLAG_TYPED;
            break;
    }
    if (messageOffset >= 0) {
        gDPPipeSync(gfx++);
        SetUpTextDrawing(&gfx);
        SetTextSlot(x, CLUB_CODE_MESSAGE_Y, MESSAGE_ENTER_CODE + messageOffset, 1, 0, flags, 255, 255, 255, 255);
        DrawLargeTextSlot(&gfx, 0);
    }
    *displayListHead = gfx;
}

s32 func_8022B75C_ovl_01(void) {
    s32 foundIndex = -1;
    s32 codeIndex;
    s32 matchCount;
    s32 charIndex;

    for (codeIndex = 0; codeIndex != CLUB_TOURNAMENT_CODE_COUNT; codeIndex++) {
        matchCount = 0;
        for (charIndex = 0; charIndex != CLUB_TOURNAMENT_CODE_LENGTH; charIndex++) {
            if (D_80230FD4_ovl_01[charIndex] != D_80230FDC_ovl_01[codeIndex][charIndex]) {
                break;
            }
            matchCount++;
        }
        if (matchCount == CLUB_TOURNAMENT_CODE_LENGTH) {
            foundIndex = codeIndex;
            break;
        }
    }
    if (foundIndex == -1) {
        D_80230F6C_ovl_01 = CLUB_CODE_STATE_INCORRECT;
    }
    return foundIndex;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022B7E0_ovl_01);

s32 func_8022BDB0_ovl_01(void) {
    return D_80105DBC[4] == -1;
}

s32 func_8022BDC4_ovl_01(void) {
    return D_80105DBC[4];
}

s32 func_8022BDD0_ovl_01(void) {
    if (D_80230F40_ovl_01) {
        D_80230F40_ovl_01 = FALSE;
        return TRUE;
    }
    return FALSE;
}

void func_8022BDF8_ovl_01(void) {
}
