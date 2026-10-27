#include "common.h"
#include "asset.h"
#include "menu.h"
#include "menu_memory.h"
#include "save.h"
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
extern u8 D_80230FDC_ovl_01[CLUB_TOURNAMENT_CODE_COUNT][CLUB_TOURNAMENT_CODE_LENGTH];
extern s8 D_80105DBC[5];

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

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_802287EC_ovl_01);

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

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_80229CB0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022A090_ovl_01);

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

INCLUDE_ASM("asm/nonmatchings/ovl_01/tournament_menu", func_8022B46C_ovl_01);

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
