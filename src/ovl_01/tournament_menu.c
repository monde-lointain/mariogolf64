#include "common.h"
#include "asset.h"
#include "billboard.h"
#include "character.h"
#include "controller.h"
#include "debug_text.h"
#include "game_flags.h"
#include "hole_display.h"
#include "libc.h"
#include "menu.h"
#include "menu_memory.h"
#include "password.h"
#include "save.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
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
#define CODE_KEYBOARD_FIRST_BLANK_COLUMN 6
#define CODE_KEYBOARD_BACK_COLUMN 8
#define CODE_KEY_BLANK 0xFD
#define CODE_KEY_BACK 0xFE
#define CODE_KEY_END 0xFF

/* What func_80228C00 is doing, in D_80230F70 */
#define CLUB_CODE_PHASE_TYPING 0
#define CLUB_CODE_PHASE_DETAILS 1
#define CLUB_CODE_PHASE_INCORRECT 2

/* A tournament code's first byte: the tournament's name, and whether Nintendo holds it */
#define CLUB_CODE_NAME_MASK 0x7F
#define CLUB_CODE_NINTENDO_SHIFT 7
#define CLUB_TOURNAMENT_NAME_COUNT 124

/* Where func_80228C00 prints the tournament code when it can be edited */
#define CODE_DEBUG_TEXT_COLUMN 2
#define CODE_DEBUG_TEXT_FIRST_ROW 3

#define SOUND_CODE_ACCEPTED 0x4A
#define SOUND_CODE_INCORRECT 0x63

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
extern s8 D_80105DBC[7];
extern s32 D_80105DC4;
extern s32 D_80231134_ovl_01; /* The tournament code byte being edited */
extern s32 D_80105B68;
extern s32 D_80105DC8;
extern s8 D_800C1FF4[MAX_PLAYER_COUNT];
extern s32 D_801B60B4;
extern s32 D_801B7070;
extern u8 g_TournamentCode[TOURNAMENT_CODE_SIZE];
s32 GetClubTournamentCourse(void);
void StartLoadingChosenHole(void);
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
#define CLUB_TEXT_SLOT_COUNT 11
/* The window's choice in D_80230F80, which left and right toggle */
#define CLUB_CHOICE_YES 0
#define CLUB_CHOICE_NO 1
#define SOUND_ENTER_TOURNAMENT 0x54
#define SOUND_DECLINE_TOURNAMENT 0x49

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
s32 func_8022B46C_ovl_01(void);
s32 func_8022B5E0_ovl_01(void);

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

    memory->codeKeyColumn = 0;
    memory->codeKeyRow = 0;
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

/*
 * Doesn't match: when the cursor moves, the original reads the other
 * coordinate before it stores the new one, and adds it to the key's
 * address last.
 */
#ifdef NON_MATCHING
void func_80228C00_ovl_01(void) {
    MenuMemory *memory = func_8005AF74();
    s32 index;
    s32 result;
    s32 isInvalid;
    TournamentRules *rules;
    char debugText[128];

    if (IsHudAnimationDone() != TRUE) {
        return;
    }
    if (D_80230F70_ovl_01 == CLUB_CODE_PHASE_INCORRECT) {
        if (!IsTextSlotRevealed(0)) {
            return;
        }
        if (g_MenuRepeatButtons & A_BUTTON) {
            D_80230F70_ovl_01 = CLUB_CODE_PHASE_TYPING;
            D_80230F6C_ovl_01 = CLUB_CODE_STATE_ENTERING;
            func_80228B60_ovl_01();
        } else if (g_MenuRepeatButtons & B_BUTTON) {
            D_80230F70_ovl_01 = CLUB_CODE_PHASE_TYPING;
            D_80230F6C_ovl_01 = CLUB_CODE_STATE_ENTERING;
        }
        return;
    }
    if (D_80230F70_ovl_01 != CLUB_CODE_PHASE_TYPING) {
        func_80229278_ovl_01();
        return;
    }
    /* The second controller edits the tournament code digit by digit */
    if (IsGameFlagSet(GAME_FLAG_EDIT_TOURNAMENT_CODE)) {
        if (g_MenuControllerData[1].button & L_JPAD) {
            D_80231134_ovl_01 = (D_80231134_ovl_01 + TOURNAMENT_CODE_SIZE - 1) % TOURNAMENT_CODE_SIZE;
        }
        if (g_MenuControllerData[1].button & R_JPAD) {
            D_80231134_ovl_01 = (D_80231134_ovl_01 + 1) % TOURNAMENT_CODE_SIZE;
        }
        /* The original's unconditional steps cancel out, so a held direction changes the digit every frame */
        if (g_MenuControllerData[1].button & U_JPAD) {
            g_TournamentCode[D_80231134_ovl_01]++;
        }
        g_TournamentCode[D_80231134_ovl_01]++;
        if (g_MenuControllerData[1].button & D_JPAD) {
            g_TournamentCode[D_80231134_ovl_01]--;
        }
        g_TournamentCode[D_80231134_ovl_01]--;
        for (index = 0; index < TOURNAMENT_CODE_SIZE; index++) {
            if (D_80231134_ovl_01 == index) {
                SetDebugTextHighlight(TRUE);
            }
            sprintf(debugText, "P%d[%d]  ", index, g_TournamentCode[index]);
            PrintDebugText(debugText, CODE_DEBUG_TEXT_COLUMN, index + CODE_DEBUG_TEXT_FIRST_ROW);
        }
    }
    if (g_MenuRepeatButtons & A_BUTTON) {
        PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        result = func_8022B46C_ovl_01();
        if (result == CODE_KEY_RESULT_DONE) {
            if (IsGameFlagSet(GAME_FLAG_EDIT_TOURNAMENT_CODE)) {
                isInvalid = FALSE;
            } else {
                isInvalid = DecodeTournamentPassword(g_TournamentCode, (char *)D_80230FD4_ovl_01);
            }
            if (!isInvalid) {
                D_80230F74_ovl_01 = g_TournamentCode[0] & CLUB_CODE_NAME_MASK;
                if (D_80230F74_ovl_01 >= CLUB_TOURNAMENT_NAME_COUNT) {
                    D_80230F74_ovl_01 = 0;
                }
                D_80105DC4 = g_TournamentCode[0] >> CLUB_CODE_NINTENDO_SHIFT;
                D_80105DC8 = D_80230F74_ovl_01;
                rules = GetTournamentRules(g_TournamentCode[1]);
                *(TournamentRules *)&D_80105DBC[4] = *rules;
                D_80105DCC = g_TournamentCode[2] + 1;
                osSyncPrintf("bkTour %x, %x, %x\n", g_TournamentCode[0], g_TournamentCode[1], g_TournamentCode[2]);
                PlaySound(SOUND_CODE_ACCEPTED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                ResetMenuWindow();
                func_80226904_ovl_01(GetClubTournamentCourse());
                D_80230F70_ovl_01 = CLUB_CODE_PHASE_DETAILS;
                if (func_8022BDC4_ovl_01() >= 0) {
                    LoadMenuWindowImage(func_8022BDC4_ovl_01());
                    func_802269AC_ovl_01(func_8022BDC4_ovl_01(), 0);
                } else {
                    LoadMenuWindowImage(CHARACTER_KID);
                    func_802269AC_ovl_01(CHARACTER_KID, 0);
                }
                return;
            }
            PlaySound(SOUND_CODE_INCORRECT, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            D_80230F70_ovl_01 = CLUB_CODE_PHASE_INCORRECT;
            D_80230F6C_ovl_01 = CLUB_CODE_STATE_INCORRECT;
        } else if (result == CODE_KEY_RESULT_TOO_SHORT) {
            PlaySound(SOUND_CODE_INCORRECT, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            D_80230F70_ovl_01 = CLUB_CODE_PHASE_INCORRECT;
            D_80230F6C_ovl_01 = CLUB_CODE_STATE_INCORRECT;
        }
    } else if (g_MenuRepeatButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
        /* B first deletes the code's characters, and leaves only once the code is empty */
        if (func_8022B5E0_ovl_01()) {
            StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
            g_MenuScreen = MENU_SCREEN_MAIN;
            D_80230F40_ovl_01 = FALSE;
            D_80105DC4 = FALSE;
            for (index = 0; index != CLUB_TEXT_SLOT_COUNT; index++) {
                SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 255, 255, 255, 255);
            }
        }
    } else if (g_MenuRepeatButtons & U_JPAD) {
        PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        memory->codeKeyRow--;
        if (memory->codeKeyRow < 0) {
            memory->codeKeyRow = CODE_KEYBOARD_ROW_COUNT - 1;
        }
        if (D_80230F84_ovl_01[memory->codeKeyRow][memory->codeKeyColumn] == CODE_KEY_BLANK) {
            memory->codeKeyRow = CODE_KEYBOARD_END_ROW - 1;
        }
    } else if (g_MenuRepeatButtons & D_JPAD) {
        PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        if (D_80230F84_ovl_01[++memory->codeKeyRow][memory->codeKeyColumn] == CODE_KEY_BLANK) {
            memory->codeKeyRow = 0;
        }
        if (memory->codeKeyRow >= CODE_KEYBOARD_ROW_COUNT) {
            memory->codeKeyRow = 0;
        }
    } else if (g_MenuRepeatButtons & L_JPAD) {
        PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        if (D_80230F84_ovl_01[memory->codeKeyRow][--memory->codeKeyColumn] == CODE_KEY_BLANK) {
            memory->codeKeyColumn = CODE_KEYBOARD_FIRST_BLANK_COLUMN - 1;
        }
        if (memory->codeKeyColumn < 0) {
            memory->codeKeyColumn = CODE_KEYBOARD_COLUMN_COUNT - 1;
        }
    } else if (g_MenuRepeatButtons & R_JPAD) {
        PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        if (D_80230F84_ovl_01[memory->codeKeyRow][++memory->codeKeyColumn] == CODE_KEY_BLANK) {
            memory->codeKeyColumn = CODE_KEYBOARD_BACK_COLUMN;
        }
        if (memory->codeKeyColumn >= CODE_KEYBOARD_COLUMN_COUNT) {
            memory->codeKeyColumn = 0;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_80228C00_ovl_01);
#endif

void func_80229278_ovl_01(void) {
    s32 isHudAnimationDone;
    s32 index;
    u16 buttons;
    s32 pressedButtons;

    func_8005AF74();
    isHudAnimationDone = IsHudAnimationDone();
    if (isHudAnimationDone != TRUE) {
        return;
    }
    if (GetWipeKind() != WIPE_NONE) {
        for (index = 0; index != CLUB_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 255, 255, 255, 255);
        }
        if (GetWipeProgress() == 0) {
            D_801B60B4 = (g_TournamentCode[0] << 16) | (g_TournamentCode[1] << 8) | g_TournamentCode[2];
            StartLoadingChosenHole();
            g_MenuScreen = MENU_SCREEN_EXIT;
        }
        return;
    }
    if (IsMenuWindowOpen() == -1) {
        return;
    }
    if (IsMenuWindowClosed() == -1) {
        return;
    }
    if (IsMenuWindowClosed()) {
        D_80230F80_ovl_01 = CLUB_CHOICE_NO;
        for (index = 0; index != CLUB_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 255, 255, 255, 255);
        }
        D_80230F70_ovl_01 = FALSE;
        D_80230F80_ovl_01 = CLUB_CHOICE_YES;
        return;
    }
    pressedButtons = g_MenuRepeatButtons;
    if (pressedButtons & A_BUTTON) {
        PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        if (D_80230F80_ovl_01 == CLUB_CHOICE_YES) {
            PlaySound(SOUND_ENTER_TOURNAMENT, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            g_IsClubTournament = TRUE;
            D_80105B68 = 1;
            if (func_8022BDB0_ovl_01()) {
                StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                StartLoadingChosenHole();
                g_MenuScreen = MENU_SCREEN_UNK24;
            } else {
                for (index = 0; index != g_ControllerCount; index++) {
                    if (g_MenuControllerData[index].button & A_BUTTON) {
                        D_800C1FF4[0] = index;
                        break;
                    }
                }
                StartLoadingChosenHole();
                D_801B7070 = 1;
                g_PlayerCount = 1;
                buttons = g_ControllerStates[0].buttons | g_ControllerStates[1].buttons |
                          g_ControllerStates[2].buttons | g_ControllerStates[3].buttons;
                /* The held C buttons pick the player's colours */
                if (buttons & L_CBUTTONS) {
                    g_Players[0].paletteIndex = 1;
                } else if (buttons & D_CBUTTONS) {
                    g_Players[0].paletteIndex = 2;
                } else if (buttons & R_CBUTTONS) {
                    g_Players[0].paletteIndex = 3;
                } else {
                    g_Players[0].paletteIndex = 0;
                }
                LoadWipeShape(GetClubTournamentCourse());
                StartWipe(WIPE_CLOSING);
            }
            for (index = 0; index != CLUB_TEXT_SLOT_COUNT; index++) {
                SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 255, 255, 255, 255);
            }
        } else {
            PlaySound(SOUND_DECLINE_TOURNAMENT, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            func_80228B60_ovl_01();
            CloseMenuWindow();
            for (index = 0; index != CLUB_TEXT_SLOT_COUNT; index++) {
                SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 255, 255, 255, 255);
            }
        }
    } else if (pressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
        D_80230F80_ovl_01 = CLUB_CHOICE_YES;
        D_80230F70_ovl_01 = FALSE;
        for (index = 0; index != CLUB_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 255, 255, 255, 255);
        }
    } else if ((pressedButtons & L_JPAD) || ((u16)pressedButtons & R_JPAD)) {
        /* The cast above stops the compiler from merging the two tests, which the original keeps apart */
        PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        D_80230F80_ovl_01 ^= 1;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_802296C4_ovl_01);

void func_80229CB0_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 width = 0;
    s32 height = 0;
    s32 windowStyle;
    u16 buttons;
    s32 pressedButtons;

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
    u8 key = D_80230F84_ovl_01[memory->codeKeyRow][memory->codeKeyColumn];

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
                    memory->codeKeyColumn = CODE_KEYBOARD_END_COLUMN;
                    memory->codeKeyRow = CODE_KEYBOARD_END_ROW;
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
