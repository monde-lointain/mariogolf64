#include "common.h"
#include "asset.h"
#include "character.h"
#include "club.h"
#include "controller.h"
#include "game_flags.h"
#include "menu.h"
#include "menu_memory.h"
#include "save.h"
#include "screen.h"
#include "text.h"
#include "variables.h"

/* Slots marked with MARKED_CHARACTER_OFFSET hold values below this; empty ones hold -1 */
#define MARKED_CHARACTER_LIMIT (-10)
/* func_80209568 lets the cursor stop on slots below this only when asked to */
#define CHOOSABLE_MARK_LIMIT (-20)

/* The directions that func_80209568 moves a player's cursor through the grid */
#define GRID_MOVE_UP 1
#define GRID_MOVE_DOWN 2
#define GRID_MOVE_LEFT 3
#define GRID_MOVE_RIGHT 4

/* The diagonals of any controller's stick that func_80209BE0 reports, each once per hold of four frames */
#define STICK_DIAGONAL_NONE 0
#define STICK_DIAGONAL_UP_LEFT 1
#define STICK_DIAGONAL_UP_RIGHT 2
#define STICK_DIAGONAL_DOWN_LEFT 3
#define STICK_DIAGONAL_DOWN_RIGHT 4
#define STICK_DIAGONAL_THRESHOLD 20
#define STICK_HOLD_FRAME_COUNT 4

/* The bytes of the first of the two images that func_8020B438 reads into the menu's buffer */
#define CHARACTER_SELECT_FIRST_IMAGE_SIZE 0x628

/* func_8020C3B8 draws D_8023A568 in strips, then the right half of the second image */
#define BOARD_WIDTH 288
#define BOARD_STRIP_HEIGHT 11
#define BOARD_STRIP_COUNT 16
#define BOARD_LEFT 16
#define BOARD_TOP 48
#define LABEL_IMAGE_WIDTH 80
#define LABEL_IMAGE_HEIGHT 8
#define LABEL_SHOWN_WIDTH 40
#define LABEL_LEFT 200
#define LABEL_TOP 200

/*
 * func_8020DB30 shows a character's shot curve as a meter of the 8x8
 * cells of asset 0x8EE, filled from the middle one toward the draw or the
 * fade, beside the 64x8 label of asset 0x979 that names it
 */
#define CURVE_METER_CENTER 4
#define CURVE_METER_CELL_SIZE 8
#define CURVE_METER_LEFT 204
#define CURVE_METER_TOP 185
#define CURVE_LABEL_WIDTH 64
#define CURVE_LABEL_HEIGHT 8
#define CURVE_LABEL_IMAGE_HEIGHT 24
#define CURVE_LABEL_LEFT 210
#define CURVE_LABEL_TOP 174
#define CURVE_LABEL_STRAIGHT 0
#define CURVE_LABEL_DRAW 1
#define CURVE_LABEL_FADE 2
/* Asset 0x709 in D_801B69EC holds the "R: COM" icon above the "L: LEFTY" icon */
#define BUTTON_ICON_WIDTH 32
#define BUTTON_ICON_HEIGHT 24
#define BUTTON_ICON_TOP 20
#define COMPUTER_ICON_LEFT 270
#define LEFTY_ICON_LEFT 18
#define LEFTY_ICON_OFFSET (BUTTON_ICON_WIDTH * BUTTON_ICON_HEIGHT / 2)

/* What func_8020B590 writes below the board */
#define MESSAGE_CHOOSE_PLAYER_CHARACTER 0x12
#define MESSAGE_CHOOSE_CHARACTER 0x14
#define MESSAGE_FIRST_CHARACTER_NAME 0xA9
#define PROMPT_X 25
#define PROMPT_Y 180
#define CHARACTER_NAME_X 238
#define CHARACTER_NAME_Y 156
#define DRIVE_DISTANCE_X 246
#define DRIVE_DISTANCE_Y 200

/* The buffer that func_8020B2C4 keeps without reading an image into it */
#define CHARACTER_SELECT_UNREAD_KIND 10

/* The characters' faces, the first set of them shown with GAME_FLAG_UNK64 set */
extern u16 D_800C2650[][8];
extern u16 D_800C2780[][8];
extern void *D_80105B78[CHARACTER_COUNT];
extern void *D_8012D430[CHARACTER_COUNT];
extern void *D_801B69EC;
extern s32 D_801B7070;
extern s32 D_8022FFA0_ovl_01;
extern s32 D_8022FFA4_ovl_01;
extern s32 D_8023BFD4_ovl_01;
extern s32 D_8023BFD8_ovl_01;
extern u8 D_8022FF01_ovl_01;
extern s8 D_8022FF04_ovl_01;
extern u8 D_8022FF05_ovl_01;
extern u8 D_8022FF06_ovl_01;
extern s32 D_8022FF2C_ovl_01[];
extern void *D_8023A568_ovl_01;
extern void *D_8023A570_ovl_01;
extern void *D_8023A574_ovl_01;
extern void *D_8023A578_ovl_01;
extern void *D_8023A57C_ovl_01;
extern void *D_8023A580_ovl_01;
extern void *D_8023A584_ovl_01;
extern Vtx D_80230038_ovl_01[MENU_CURSOR_VERTEX_COUNT];
extern f32 D_80230080_ovl_01[MENU_CURSOR_VERTEX_COUNT]; /* The phase of each corner's colour */
extern Mtx D_8023A3A0_ovl_01;
extern Mtx D_8023A3E0_ovl_01[2];
extern Mtx D_8023A460_ovl_01[2];
extern Mtx D_8023A4E0_ovl_01[2];
extern s32 D_8022FF1C_ovl_01[4];
extern s32 D_8022FF50_ovl_01[];
extern char D_800C2A00[][IMPORTED_CHARACTER_NAME_LENGTH];

/* Where the character cursor's translation is measured from, so that it is placed by its corner */
#define CHARACTER_CURSOR_ORIGIN_X 140
#define CHARACTER_CURSOR_ORIGIN_Y 96
/* Moves the cursor by half a pixel so that its edges sit on pixel centres */
#define HALF_PIXEL 0.5f

extern s32 D_80105B68;
extern u8 D_8022FF00_ovl_01;
extern s8 D_8022FF07_ovl_01;
extern s8 D_8022FF08_ovl_01;
extern s8 D_800C1FF4[MAX_PLAYER_COUNT];
/* Each player's tag offset from their cursor's grid cell, as x and y */
extern s8 D_8022FFCC_ovl_01[MAX_PLAYER_COUNT][2];
/* The tags' x when lined up, by the player count less one */
extern s32 D_8022FFD4_ovl_01[MAX_PLAYER_COUNT][MAX_PLAYER_COUNT];
void func_801FC754_ovl_01(Gfx **displayListHead);
void func_801FD7B0_ovl_01(Gfx **displayListHead);
void func_8020BA08_ovl_01(Gfx **displayListHead, u8 isSinglePlayer);
void func_8020C3B8_ovl_01(Gfx **displayListHead);
void func_8020C6D8_ovl_01(Gfx **displayListHead, u8 tagMode);
/* The original passes these a second argument that they ignore */
void func_8020D338_ovl_01();
void func_8020D734_ovl_01();
void func_8020DB30_ovl_01(Gfx **displayListHead, s32 clubSet);

void func_80209500_ovl_01(void) {
    s32 firstSlot = g_CharacterGrid[2][0];
    s32 secondSlot = g_CharacterGrid[2][1];
    s32 thirdSlot = g_CharacterGrid[2][2];
    s32 fourthSlot = g_CharacterGrid[2][3];

    g_CharacterGrid[2][0] = -1;
    g_CharacterGrid[2][1] = -1;
    g_CharacterGrid[2][2] = -1;
    g_CharacterGrid[2][3] = -1;
    D_8022FF1C_ovl_01[0] = firstSlot;
    D_8022FF1C_ovl_01[1] = secondSlot;
    D_8022FF1C_ovl_01[2] = thirdSlot;
    D_8022FF1C_ovl_01[3] = fourthSlot;
}

/*
 * Doesn't match: the original sign-extends the slot and character afresh at
 * each use.
 */
#ifdef NON_MATCHING
s32 func_80209568_ovl_01(s32 playerNumber, u8 direction, u8 canChooseMarked) {
    EncodedAsset asset;
    AssetFile file;
    MenuMemory *memory = func_8005AF74();
    s16 oldColumn = memory->characterColumns[playerNumber];
    s16 oldRow = memory->characterRows[playerNumber];
    s16 column = oldColumn;
    s16 row = oldRow;
    u16 slot;
    u16 character;
    void *faceImage;
    s32 unused[2];
    u16(*faceAssets)[8];

    do {
        if (direction == GRID_MOVE_UP) {
            row--;
        }
        if (direction == GRID_MOVE_DOWN) {
            row++;
        }
        if (direction == GRID_MOVE_LEFT) {
            column--;
        }
        if (direction == GRID_MOVE_RIGHT) {
            column++;
        }
        if (row < 0) {
            row = CHARACTER_GRID_ROW_COUNT - 1;
        }
        if (row >= CHARACTER_GRID_ROW_COUNT) {
            row = 0;
        }
        if (column < 0) {
            column = CHARACTER_GRID_COLUMN_COUNT - 1;
        }
        if (column >= CHARACTER_GRID_COLUMN_COUNT) {
            column = 0;
        }
        if (column == oldColumn && row == oldRow) {
            break;
        }
    } while (g_CharacterGrid[row][column] < 0 &&
             (g_CharacterGrid[row][column] < CHOOSABLE_MARK_LIMIT && canChooseMarked == TRUE) == FALSE);
    memory->characterColumns[playerNumber] = column;
    memory->characterRows[playerNumber] = row;
    if (column != oldColumn || row != oldRow) {
        character = g_CharacterGrid[oldRow][oldColumn];
        D_8022FF04_ovl_01 = 0;
        D_8022FF06_ovl_01 = 0;
        D_8022FF05_ovl_01 ^= 1;
        if (g_CharacterGrid[oldRow][oldColumn] < CHOOSABLE_MARK_LIMIT) {
            character += MARKED_CHARACTER_OFFSET;
        }
        slot = character;
        if ((s16)slot >= FIRST_IMPORTED_CHARACTER) {
            character = g_ImportedCharacters[(s16)slot - FIRST_IMPORTED_CHARACTER].character;
        } else {
            character = slot;
        }
        faceImage = D_8012D430[(s16)slot];
        if (IsGameFlagSet(GAME_FLAG_UNK64)) {
            faceAssets = D_800C2780;
        } else {
            faceAssets = D_800C2650;
        }
        OpenEncodedAsset(faceAssets[(s16)character][D_8022FF04_ovl_01], &asset);
        ReadEncodedAsset(faceImage, &asset);
        OpenAssetFile(D_800C2590[(s16)character].assetId, &file);
        ReadAssetFile(D_80105B78[(s16)slot], (s16)GetAssetFileSize(&file), &file);
    }
    return -(column == oldColumn && row == oldRow);
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_80209568_ovl_01);
#endif

/*
 * Doesn't match: as in func_80209568, the original sign-extends the slot and
 * character afresh at each use and adds the player number the other way round.
 */
#ifdef NON_MATCHING
s32 func_80209894_ovl_01(s32 playerNumber, u8 diagonal, u8 canChooseMarked) {
    EncodedAsset asset;
    AssetFile file;
    MenuMemory *memory = func_8005AF74();
    s32 retryCount = 0;
    s16 oldRow = memory->characterRows[playerNumber];
    s16 oldColumn = memory->characterColumns[playerNumber];
    s32 column = oldColumn;
    s32 row = oldRow;
    u16 slot;
    u16 character;
    void *faceImage;
    s32 unused[2];
    u16(*faceAssets)[8];

    while (TRUE) {
        if (retryCount == 0) {
            if (diagonal == STICK_DIAGONAL_UP_LEFT) {
                row--;
                column--;
            }
            if (diagonal == STICK_DIAGONAL_UP_RIGHT) {
                row--;
                column++;
            }
            if (diagonal == STICK_DIAGONAL_DOWN_LEFT) {
                row++;
                column--;
            }
            if (diagonal == STICK_DIAGONAL_DOWN_RIGHT) {
                row++;
                column++;
            }
        } else {
            column -= diagonal == STICK_DIAGONAL_UP_LEFT || diagonal == STICK_DIAGONAL_DOWN_LEFT;
            column += diagonal == STICK_DIAGONAL_UP_RIGHT || diagonal == STICK_DIAGONAL_DOWN_RIGHT;
        }
        if (row < 0) {
            row = CHARACTER_GRID_ROW_COUNT - 1;
        }
        if (row >= CHARACTER_GRID_ROW_COUNT) {
            row = 0;
        }
        if (column < 0) {
            column = CHARACTER_GRID_COLUMN_COUNT - 1;
        }
        if (column >= CHARACTER_GRID_COLUMN_COUNT) {
            column = 0;
        }
        if (column == oldColumn && row == oldRow) {
            break;
        }
        if (g_CharacterGrid[row][column] >= 0 ||
            (g_CharacterGrid[row][column] < CHOOSABLE_MARK_LIMIT && canChooseMarked == TRUE)) {
            break;
        }
        /* After a blocked diagonal, try moving sideways only from where the cursor started */
        if (retryCount == 0) {
            column = oldColumn;
            row = oldRow;
        }
        retryCount++;
    }
    memory->characterColumns[playerNumber] = column;
    memory->characterRows[playerNumber] = row;
    if (column != oldColumn || row != oldRow) {
        character = g_CharacterGrid[oldRow][oldColumn];
        D_8022FF04_ovl_01 = 0;
        D_8022FF06_ovl_01 = 0;
        D_8022FF05_ovl_01 ^= 1;
        if (g_CharacterGrid[oldRow][oldColumn] < CHOOSABLE_MARK_LIMIT) {
            character += MARKED_CHARACTER_OFFSET;
        }
        slot = character;
        if ((s16)slot >= FIRST_IMPORTED_CHARACTER) {
            character = g_ImportedCharacters[(s16)slot - FIRST_IMPORTED_CHARACTER].character;
        } else {
            character = slot;
        }
        faceImage = D_8012D430[(s16)slot];
        if (IsGameFlagSet(GAME_FLAG_UNK64)) {
            faceAssets = D_800C2780;
        } else {
            faceAssets = D_800C2650;
        }
        OpenEncodedAsset(faceAssets[(s16)character][D_8022FF04_ovl_01], &asset);
        ReadEncodedAsset(faceImage, &asset);
        OpenAssetFile(D_800C2590[(s16)character].assetId, &file);
        ReadAssetFile(D_80105B78[(s16)slot], GetAssetFileSize(&file), &file);
    }
    return -(column == oldColumn && row == oldRow);
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_80209894_ovl_01);
#endif

s32 func_80209BE0_ovl_01(void) {
    s32 stickX;
    s32 stickY;
    s8 anyStick;

    func_8005AF74();
    anyStick = g_ControllerStates[3].stickX |
               (g_ControllerStates[2].stickX | (g_ControllerStates[0].stickX | g_ControllerStates[1].stickX));
    stickX = D_8023BFD4_ovl_01 ^ anyStick;
    stickX &= anyStick;
    if (D_8023BFD4_ovl_01 == stickX) {
        D_8022FFA0_ovl_01++;
        if (D_8022FFA0_ovl_01 >= STICK_HOLD_FRAME_COUNT) {
            D_8022FFA0_ovl_01 = 0;
            D_8023BFD4_ovl_01 = 0;
        }
    } else {
        D_8023BFD4_ovl_01 = stickX;
    }
    anyStick = g_ControllerStates[3].stickY |
               (g_ControllerStates[2].stickY | (g_ControllerStates[0].stickY | g_ControllerStates[1].stickY));
    stickY = D_8023BFD8_ovl_01 ^ anyStick;
    stickY &= anyStick;
    if (D_8023BFD8_ovl_01 == stickY) {
        D_8022FFA4_ovl_01++;
        if (D_8022FFA4_ovl_01 >= STICK_HOLD_FRAME_COUNT) {
            D_8022FFA4_ovl_01 = 0;
            D_8023BFD8_ovl_01 = 0;
        }
    } else {
        D_8023BFD8_ovl_01 = stickY;
    }
    if (stickX < -STICK_DIAGONAL_THRESHOLD && stickY > STICK_DIAGONAL_THRESHOLD) {
        return STICK_DIAGONAL_UP_LEFT;
    }
    if (stickX > STICK_DIAGONAL_THRESHOLD && stickY > STICK_DIAGONAL_THRESHOLD) {
        return STICK_DIAGONAL_UP_RIGHT;
    }
    if (stickX < -STICK_DIAGONAL_THRESHOLD && stickY < -STICK_DIAGONAL_THRESHOLD) {
        return STICK_DIAGONAL_DOWN_LEFT;
    }
    if (stickX > STICK_DIAGONAL_THRESHOLD && stickY < -STICK_DIAGONAL_THRESHOLD) {
        return STICK_DIAGONAL_DOWN_RIGHT;
    }
    return STICK_DIAGONAL_NONE;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_80209D50_ovl_01);

void func_8020B2C4_ovl_01(void *buffer, s16 bufferKind) {
    EncodedAsset asset;
    void *image = buffer;

    switch (bufferKind) {
        case 1:
            D_8023A570_ovl_01 = buffer;
            break;
        case 2:
            D_8023A568_ovl_01 = buffer;
            break;
        case 5:
            D_8023A574_ovl_01 = buffer;
            break;
        case 6:
            D_8023A57C_ovl_01 = buffer;
            break;
        case 7:
            D_8023A580_ovl_01 = buffer;
            break;
        case 8:
            D_8023A584_ovl_01 = buffer;
            break;
        case CHARACTER_SELECT_UNREAD_KIND:
            D_8023A578_ovl_01 = buffer;
            break;
        default:
            D_8023A570_ovl_01 = buffer;
            bufferKind = CHARACTER_SELECT_UNREAD_KIND;
            break;
    }
    if (bufferKind != CHARACTER_SELECT_UNREAD_KIND) {
        OpenEncodedAsset(D_8022FF2C_ovl_01[bufferKind], &asset);
        ReadEncodedAsset(image, &asset);
    }
    D_8022FF01_ovl_01 = FALSE;
}

void func_8020B3CC_ovl_01(void) {
    MenuMemory *memory = func_8005AF74();
    s32 row;
    s32 column;

    for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
        for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
            if (g_CharacterGrid[row][column] < MARKED_CHARACTER_LIMIT) {
                memory->characterColumns[1] = column;
                memory->characterRows[1] = row;
            }
        }
    }
}

/*
 * Doesn't match: the original indexes D_80105B78 afresh in each pass,
 * where this steps a pointer through it and needs one more saved register.
 */
#ifdef NON_MATCHING
void func_8020B438_ovl_01(void) {
    EncodedAsset asset;
    AssetFile file;
    s32 character = 0;
    u8 *images = GetMenuImageBuffer();
    s32 slotIndex;
    void *faceImage;
    s32 unused[2];
    void *characterFile;
    u16 faceAsset;

    OpenEncodedAsset(0x979, &asset);
    ReadEncodedAsset(images, &asset);
    OpenEncodedAsset(0x978, &asset);
    ReadEncodedAsset(images + CHARACTER_SELECT_FIRST_IMAGE_SIZE, &asset);
    for (slotIndex = 0; slotIndex != CHARACTER_COUNT; slotIndex++) {
        faceImage = D_8012D430[slotIndex];
        characterFile = D_80105B78[slotIndex];
        if (slotIndex >= FIRST_IMPORTED_CHARACTER) {
            if (g_ImportedCharacters[slotIndex - FIRST_IMPORTED_CHARACTER].character >= 0) {
                character = g_ImportedCharacters[slotIndex - FIRST_IMPORTED_CHARACTER].character;
            }
        } else {
            character = slotIndex;
        }
        if (IsGameFlagSet(GAME_FLAG_UNK64)) {
            faceAsset = D_800C2780[character][0];
        } else {
            faceAsset = D_800C2650[character][0];
        }
        OpenEncodedAsset(faceAsset, &asset);
        ReadEncodedAsset(faceImage, &asset);
        OpenAssetFile(D_800C2590[character].assetId, &file);
        ReadAssetFile(characterFile, GetAssetFileSize(&file), &file);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020B438_ovl_01);
#endif

/*
 * Doesn't match: the original schedules the shift of the choice index for
 * the character table lookup one instruction later. The menu memory fields
 * are read through integer addresses because the original adds the player
 * number before the field offset.
 */
#ifdef NON_MATCHING
void func_8020B590_ovl_01(Gfx **displayListHead, u8 isSinglePlayer) {
    Gfx *gfx = *displayListHead;
    s32 playerNumber;
    s16 column;
    s16 slot;
    s16 choiceIndex;
    s32 *character;
    s16 clubSet;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
    func_801FC754_ovl_01(&gfx);
    func_801FD7B0_ovl_01(&gfx);
    func_8020C3B8_ovl_01(&gfx);
    func_8020BA08_ovl_01(&gfx, (u8)isSinglePlayer);
    if ((u8)isSinglePlayer == TRUE) {
        func_8020C6D8_ovl_01(&gfx, 2);
    } else {
        func_8020C6D8_ovl_01(&gfx, 1);
    }
    if ((D_8022FF07_ovl_01 == TRUE) & ((u8)isSinglePlayer == FALSE)) {
        func_8020D338_ovl_01(&gfx, 0);
    }
    if (D_8022FF08_ovl_01 == TRUE) {
        func_8020D734_ovl_01(&gfx, 0);
    }
    gDPPipeSync(gfx++);
    if ((u8)isSinglePlayer == FALSE) {
        playerNumber = D_801B7070;
        if (playerNumber >= D_80105B68) {
            playerNumber = D_80105B68 - 1;
        }
    } else {
        playerNumber = 1;
    }
    column = *(s8 *)((s32)&func_8005AF74()->characterColumns[1] + (s16)playerNumber - 1);
    slot = g_CharacterGrid[*(s8 *)((s32)&func_8005AF74()->characterRows[1] + (s16)playerNumber - 1)][column];
    if (slot < CHOOSABLE_MARK_LIMIT) {
        choiceIndex = slot + MARKED_CHARACTER_OFFSET;
    } else {
        choiceIndex = slot;
    }
    character = &D_8022FF50_ovl_01[choiceIndex];
    func_8020DB30_ovl_01(&gfx, *character);
    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    SetTextName(D_800C2A00[choiceIndex]);
    SetTextSlot(CHARACTER_NAME_X, CHARACTER_NAME_Y, MESSAGE_FIRST_CHARACTER_NAME + choiceIndex, 1, 1,
                TEXT_FLAG_CENTERED, 255, 255, 255, 255);
    clubSet = *character;
    BeginShadowedText(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    sprintf(g_PrintBuffer, "%dY", g_Clubs[clubSet * CLUB_SET_SIZE + CLUB_TYPE_DRIVER].distance);
    DrawShadowedText(&gfx, DRIVE_DISTANCE_X, DRIVE_DISTANCE_Y, g_PrintBuffer);
    if (D_8022FF00_ovl_01 == FALSE) {
        if ((u8)isSinglePlayer == TRUE) {
            SetTextSlot(PROMPT_X, PROMPT_Y, MESSAGE_CHOOSE_CHARACTER, 1, 0, 1, 255, 255, 255, 255);
        } else {
            SetTextSlotValue(D_801B7070 + 1, 0, 0);
            /* The prompt names the player in the player's own colour */
            if (D_801B7070 == 0) {
                SetTextPaletteColor(255, 127, 127, 0);
            }
            if (D_801B7070 == 1) {
                SetTextPaletteColor(0, 255, 255, 0);
            }
            if (D_801B7070 == 2) {
                SetTextPaletteColor(255, 255, 0, 0);
            }
            if (D_801B7070 == 3) {
                SetTextPaletteColor(63, 255, 63, 0);
            }
            SetTextSlot(PROMPT_X, PROMPT_Y, MESSAGE_CHOOSE_PLAYER_CHARACTER, 1, 0, 1, 255, 255, 255, 255);
        }
        D_8022FF00_ovl_01 = TRUE;
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020B590_ovl_01);
#endif

INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020BA08_ovl_01);

/* Doesn't match: the first loop's index and count take each other's registers */
#ifdef NON_MATCHING
s32 func_8020C2D8_ovl_01(s32 choiceIndex) {
    s32 freePalette = 0;
    s32 playerIndex = 0;
    s32 sameCount = 0;
    s32 character = D_8022FF50_ovl_01[choiceIndex];
    s32 unused[4];
    s32 samePlayers[MAX_PLAYER_COUNT];
    s32 palette;
    s32 sameIndex;

    for (; playerIndex != D_801B7070; playerIndex++) {
        if ((s8)g_Players[playerIndex].characterIndex == character) {
            samePlayers[sameCount++] = playerIndex;
        }
    }
    if (sameCount != 0) {
        for (palette = 0; palette != CHARACTER_PALETTE_COUNT; palette++) {
            for (sameIndex = 0; sameIndex != sameCount; sameIndex++) {
                if ((s8)g_Players[samePlayers[sameIndex]].paletteIndex == palette) {
                    break;
                }
            }
            if (sameIndex == sameCount) {
                freePalette = palette;
                break;
            }
        }
        return freePalette;
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020C2D8_ovl_01);
#endif

void func_8020C3B8_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;
    u8 *labelImage;

    GetMenuImageBuffer();
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_8023A568_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != BOARD_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock_4b(gfx++,
                               ((u32)D_8023A568_ovl_01 + PALETTED_TEXELS_OFFSET +
                                stripIndex * BOARD_WIDTH * BOARD_STRIP_HEIGHT / 2) & ~7,
                               G_IM_FMT_CI, BOARD_WIDTH, BOARD_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                               G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        stripTop = BOARD_TOP + stripIndex * BOARD_STRIP_HEIGHT;
        gSPScisTextureRectangle(gfx++, BOARD_LEFT << 2, stripTop << 2, (BOARD_LEFT + BOARD_WIDTH) << 2,
                                (stripTop + BOARD_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
                                TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    }
    labelImage = GetMenuImageBuffer();
    labelImage += CHARACTER_SELECT_FIRST_IMAGE_SIZE;
    LoadSmallPalette(&gfx, 0, labelImage + IMAGE_HEADER_SIZE);
    LoadNibbleTextureBlock(&gfx, labelImage + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI, LABEL_IMAGE_WIDTH,
                           LABEL_IMAGE_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    DrawTextureRect(&gfx, LABEL_LEFT << 2, LABEL_TOP << 2, (LABEL_LEFT + LABEL_SHOWN_WIDTH) << 2,
                    (LABEL_TOP + LABEL_IMAGE_HEIGHT) << 2, G_TX_RENDERTILE, LABEL_SHOWN_WIDTH << 5, 0,
                    TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    *displayListHead = gfx;
}

/* How func_8020C6D8 places the player tags. Nothing passes PLAYER_TAG_LINED_UP. */
#define PLAYER_TAG_LINED_UP 0
#define PLAYER_TAG_SINGLE_PLAYER 2 /* The second tag then reads "Com" */
/* The player tag images: 1P to 4P, then 1C to 4C, then Com */
#define PLAYER_TAG_FIRST_COMPUTER 4
#define PLAYER_TAG_COM 8
#define PLAYER_TAG_IMAGE_SIZE 64 /* Bytes, as 16 by 8 nibbles */
#define COMPUTER_TAG_TEXELS_OFFSET (PALETTED_TEXELS_OFFSET + PLAYER_TAG_FIRST_COMPUTER * PLAYER_TAG_IMAGE_SIZE)
#define PLAYER_TAG_TEXELS(offset) ((u32)((u8 *)D_8023A570_ovl_01 + (offset)) & ~7)
#define PLAYER_TAG_WIDTH 16
#define PLAYER_TAG_HEIGHT 8
#define PLAYER_TAG_SHIFT_LEFT 4
#define PLAYER_TAG_TOP 56
#define PLAYER_TAG_RAISE (-14) /* Lined-up tags alternate between two heights */
/* The character grid's cells on screen */
#define GRID_CELL_SIZE 40
#define GRID_LEFT 28
#define GRID_TOP 48
#define GRID_CURSOR_SHIFT_LEFT 8

/*
 * Doesn't match: the original sign-extends the cursor's x to 16 bits before
 * adding GRID_LEFT, where GCC here sees that it already fits and folds the
 * two offsets together.
 */
#ifdef NON_MATCHING
void func_8020C6D8_ovl_01(Gfx **displayListHead, u8 tagMode) {
    MenuMemory *memory = func_8005AF74();
    s16 tagCount = D_801B7070 + 1;
    Gfx *gfx = *displayListHead;
    s8 playerIndex;
    s16 left;
    s16 shiftX; /* Both layouts leave it at 0 */
    s16 top;
    s16 cursorLeft;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_8023A570_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
    if (g_MenuScreen == MENU_SCREEN_UNK24) {
        tagCount = 1;
    }
    for (playerIndex = 0; playerIndex != tagCount; playerIndex++) {
        if (tagMode != PLAYER_TAG_LINED_UP) {
            shiftX = 0;
            left = D_8022FFCC_ovl_01[playerIndex][0] + memory->characterColumns[playerIndex] * GRID_CELL_SIZE;
            top = D_8022FFCC_ovl_01[playerIndex][1] + memory->characterRows[playerIndex] * GRID_CELL_SIZE;
        } else {
            left = D_8022FFD4_ovl_01[tagCount - 1][playerIndex];
            top = ((playerIndex & 1) == 0) ? PLAYER_TAG_RAISE : 0;
            if ((tagCount == 2) & (playerIndex == 1)) {
                top = PLAYER_TAG_RAISE;
            }
            shiftX = 0;
        }
        if ((tagMode == PLAYER_TAG_SINGLE_PLAYER) & (playerIndex == 1)) {
            gDPLoadTextureBlock_4b(gfx++,
                                   PLAYER_TAG_TEXELS(PALETTED_TEXELS_OFFSET + PLAYER_TAG_COM * PLAYER_TAG_IMAGE_SIZE),
                                   G_IM_FMT_CI, PLAYER_TAG_WIDTH, PLAYER_TAG_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        } else if (D_800C1FF4[playerIndex] == NO_CONTROLLER_INDEX) {
            gDPLoadTextureBlock_4b(gfx++,
                                   PLAYER_TAG_TEXELS(playerIndex * PLAYER_TAG_IMAGE_SIZE + COMPUTER_TAG_TEXELS_OFFSET),
                                   G_IM_FMT_CI, PLAYER_TAG_WIDTH, PLAYER_TAG_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        } else if (D_8022FF07_ovl_01 == TRUE && playerIndex == D_801B7070) {
            gDPLoadTextureBlock_4b(gfx++,
                                   PLAYER_TAG_TEXELS(playerIndex * PLAYER_TAG_IMAGE_SIZE + COMPUTER_TAG_TEXELS_OFFSET),
                                   G_IM_FMT_CI, PLAYER_TAG_WIDTH, PLAYER_TAG_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        } else {
            gDPLoadTextureBlock_4b(gfx++,
                                   PLAYER_TAG_TEXELS(playerIndex * PLAYER_TAG_IMAGE_SIZE + PALETTED_TEXELS_OFFSET),
                                   G_IM_FMT_CI, PLAYER_TAG_WIDTH, PLAYER_TAG_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        }
        left = shiftX + (s16)(left + GRID_LEFT);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, (left - PLAYER_TAG_SHIFT_LEFT) << 2, (top + PLAYER_TAG_TOP) << 2,
                                (left - PLAYER_TAG_SHIFT_LEFT + PLAYER_TAG_WIDTH) << 2,
                                (top + PLAYER_TAG_TOP + PLAYER_TAG_HEIGHT) << 2,
                                G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    }
    /* playerIndex now picks whose cursor to draw */
    playerIndex = 0;
    if (g_MenuScreen != MENU_SCREEN_UNK24) {
        playerIndex = D_801B7070;
    }
    cursorLeft = memory->characterColumns[playerIndex] * GRID_CELL_SIZE - GRID_CURSOR_SHIFT_LEFT;
    func_8020CD20_ovl_01(&gfx, cursorLeft + GRID_LEFT,
                         memory->characterRows[playerIndex] * GRID_CELL_SIZE + GRID_TOP);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020C6D8_ovl_01);
#endif

/* Doesn't match: the original loads the 96 for the cursor's y after g_DrawBufferIndex, not before */
#ifdef NON_MATCHING
void func_8020CD20_ovl_01(Gfx **displayListHead, s32 x, s32 y) {
    Gfx *gfx = *displayListHead;
    s32 vertexIndex;
    s32 red;
    s32 blue;
    s32 green;

    for (vertexIndex = 0; vertexIndex != MENU_CURSOR_VERTEX_COUNT; vertexIndex++) {
        D_80230080_ovl_01[vertexIndex] += MENU_CURSOR_PHASE_STEP;
        red = (s32)(sinf(D_80230080_ovl_01[vertexIndex]) * MENU_CURSOR_COLOR_SWING) + MENU_CURSOR_COLOR_MIDDLE;
        blue = (s32)(sinf(D_80230080_ovl_01[vertexIndex] + M_PI / 2) * MENU_CURSOR_COLOR_SWING) +
               MENU_CURSOR_COLOR_MIDDLE;
        green = (s32)(sinf(D_80230080_ovl_01[vertexIndex] + M_PI) * MENU_CURSOR_COLOR_SWING) +
                MENU_CURSOR_COLOR_MIDDLE;
        D_80230038_ovl_01[vertexIndex].v.cn[0] = red;
        D_80230038_ovl_01[vertexIndex].v.cn[2] = blue;
        D_80230038_ovl_01[vertexIndex].v.cn[1] = green;
    }
    guOrtho(&D_8023A3A0_ovl_01, -(SCREEN_WIDTH / 2), SCREEN_WIDTH / 2, -(SCREEN_HEIGHT / 2), SCREEN_HEIGHT / 2,
            MENU_CURSOR_NEAR, MENU_CURSOR_FAR, 1.0f);
    guTranslate(&D_8023A3E0_ovl_01[g_DrawBufferIndex], (f32)(x - CHARACTER_CURSOR_ORIGIN_X) - HALF_PIXEL,
                (f32)(CHARACTER_CURSOR_ORIGIN_Y - y) + HALF_PIXEL, MENU_CURSOR_Z);
    guRotate(&D_8023A460_ovl_01[g_DrawBufferIndex], 0.0f, 0.0f, 1.0f, 0.0f);
    guScale(&D_8023A4E0_ovl_01[g_DrawBufferIndex], 1.0f, 1.0f, 1.0f);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPLoadTextureBlock_4b(gfx++, ((u32)D_8023A574_ovl_01 + IMAGE_HEADER_SIZE) & ~7, G_IM_FMT_IA, MENU_CURSOR_SIZE,
                           MENU_CURSOR_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                           G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    gDPSetCombineMode(gfx++, G_CC_SHADEDECALA, G_CC_SHADEDECALA);
    gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023A3A0_ovl_01), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023A3E0_ovl_01[g_DrawBufferIndex]),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023A460_ovl_01[g_DrawBufferIndex]),
              G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023A4E0_ovl_01[g_DrawBufferIndex]),
              G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
    gSPVertex(gfx++, D_80230038_ovl_01, MENU_CURSOR_VERTEX_COUNT, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", func_8020CD20_ovl_01);
#endif

void func_8020D338_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_801B69EC + IMAGE_HEADER_SIZE) & ~7);
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(gfx++, ((u32)D_801B69EC + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI, BUTTON_ICON_WIDTH,
                           BUTTON_ICON_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                           G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPTextureRectangle(gfx++, COMPUTER_ICON_LEFT << 2, BUTTON_ICON_TOP << 2,
                        (COMPUTER_ICON_LEFT + BUTTON_ICON_WIDTH) << 2,
                        (BUTTON_ICON_TOP + BUTTON_ICON_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void func_8020D734_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_801B69EC + IMAGE_HEADER_SIZE) & ~7);
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(gfx++, ((u32)D_801B69EC + PALETTED_TEXELS_OFFSET + LEFTY_ICON_OFFSET) & ~7, G_IM_FMT_CI,
                           BUTTON_ICON_WIDTH, BUTTON_ICON_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPTextureRectangle(gfx++, LEFTY_ICON_LEFT << 2, BUTTON_ICON_TOP << 2,
                        (LEFTY_ICON_LEFT + BUTTON_ICON_WIDTH) << 2,
                        (BUTTON_ICON_TOP + BUTTON_ICON_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void func_8020DB30_ovl_01(Gfx **displayListHead, s32 clubSet) {
    Gfx *gfx = *displayListHead;
    s32 curve = g_Clubs[clubSet * CLUB_SET_SIZE].curve;
    s32 firstCell;
    s32 cellIndex;
    s32 endCell;
    u8 *labelImage;
    s32 labelRow;

    if (curve < 0) {
        firstCell = CURVE_METER_CENTER + curve;
        endCell = CURVE_METER_CENTER + 1;
    } else if (curve > 0) {
        firstCell = CURVE_METER_CENTER;
        endCell = CURVE_METER_CENTER + 1 + curve;
    } else {
        firstCell = CURVE_METER_CENTER;
        endCell = CURVE_METER_CENTER + 1;
    }
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_8023A584_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
    for (cellIndex = firstCell; cellIndex != endCell; cellIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)D_8023A584_ovl_01 + LARGE_PALETTED_TEXELS_OFFSET +
                             cellIndex * CURVE_METER_CELL_SIZE * CURVE_METER_CELL_SIZE) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, CURVE_METER_CELL_SIZE, CURVE_METER_CELL_SIZE, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, (CURVE_METER_LEFT + cellIndex * CURVE_METER_CELL_SIZE) << 2,
                                CURVE_METER_TOP << 2,
                                (CURVE_METER_LEFT + cellIndex * CURVE_METER_CELL_SIZE + CURVE_METER_CELL_SIZE) << 2,
                                (CURVE_METER_TOP + CURVE_METER_CELL_SIZE) << 2, G_TX_RENDERTILE, 0, 0,
                                TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    }
    labelImage = GetMenuImageBuffer();
    LoadSmallPalette(&gfx, 0, labelImage + IMAGE_HEADER_SIZE);
    LoadNibbleTextureBlock(&gfx, labelImage + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI, CURVE_LABEL_WIDTH,
                           CURVE_LABEL_IMAGE_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    if (g_Clubs[clubSet * CLUB_SET_SIZE].curve > 0) {
        labelRow = CURVE_LABEL_DRAW;
    } else if (g_Clubs[clubSet * CLUB_SET_SIZE].curve < 0) {
        labelRow = CURVE_LABEL_FADE;
    } else {
        labelRow = CURVE_LABEL_STRAIGHT;
    }
    DrawTextureRect(&gfx, CURVE_LABEL_LEFT << 2, CURVE_LABEL_TOP << 2, (CURVE_LABEL_LEFT + CURVE_LABEL_WIDTH) << 2,
                    (CURVE_LABEL_TOP + CURVE_LABEL_HEIGHT) << 2, G_TX_RENDERTILE, 0,
                    (labelRow * CURVE_LABEL_HEIGHT) << 5, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    *displayListHead = gfx;
}

const char D_80231810_ovl_01[] = "Sand Saves";
const char D_8023181C_ovl_01[] = "Par Ons";
const char D_80231824_ovl_01[] = "Par Saves";
const char D_80231830_ovl_01[] = "Avg. Putt";
const char D_8023183C_ovl_01[] = "On Fairway";
const char D_80231848_ovl_01[] = "Avg. Drive";
const char D_80231854_ovl_01[] = "Best Drive";
const char D_80231860_ovl_01[] = "Average";
const char D_80231868_ovl_01[] = "Best Score";
const char D_80231874_ovl_01[] = "Round";
