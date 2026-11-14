#include "character_select.h"

#include <stdlib.h>
#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "character.h"
#include "character_slot.h"
#include "club.h"
#include "compiler.h"
#include "controller.h"
#include "debug_text.h"
#include "game.h"
#include "game_flags.h"
#include "game_state.h"
#include "gbi_helpers.h"
#include "language.h"
#include "main_menu.h"
#include "menu.h"
#include "menu_memory.h"
#include "player.h"
#include "screen.h"
#include "screen_layers.h"
#include "sky_and_hud_animations.h"
#include "sound.h"
#include "text.h"
#include "transfer_pak.h"

#define CHOOSABLE_MARK_LIMIT (-20)

enum GridMove {
    GRID_MOVE_UP = 1,
    GRID_MOVE_DOWN,
    GRID_MOVE_LEFT,
    GRID_MOVE_RIGHT
};

typedef u8 GridMove;

enum StickDiagonal {
    STICK_DIAGONAL_NONE,
    STICK_DIAGONAL_UP_LEFT,
    STICK_DIAGONAL_UP_RIGHT,
    STICK_DIAGONAL_DOWN_LEFT,
    STICK_DIAGONAL_DOWN_RIGHT
};

typedef s32 StickDiagonal;
typedef u8 UStickDiagonal8;

#define STICK_DIAGONAL_THRESHOLD 20
#define STICK_HOLD_FRAME_COUNT 4

#define CHARACTER_SELECT_FIRST_IMAGE_SIZE 0x628

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

#define CURVE_METER_CENTER 4
#define CURVE_METER_CELL_SIZE 8
#define CURVE_METER_LEFT 204
#define CURVE_METER_TOP 185
#define CURVE_LABEL_WIDTH 64
#define CURVE_LABEL_HEIGHT 8
#define CURVE_LABEL_IMAGE_HEIGHT 24
#define CURVE_LABEL_LEFT 210
#define CURVE_LABEL_TOP 174

enum CurveLabel {
    CURVE_LABEL_STRAIGHT,
    CURVE_LABEL_DRAW,
    CURVE_LABEL_FADE
};

typedef s32 CurveLabel;

#define BUTTON_ICON_WIDTH 32
#define BUTTON_ICON_HEIGHT 24
#define BUTTON_ICON_TOP 20
#define COMPUTER_ICON_LEFT 270
#define LEFTY_ICON_LEFT 18
#define LEFTY_ICON_OFFSET (BUTTON_ICON_WIDTH * BUTTON_ICON_HEIGHT / 2)

#define MESSAGE_CHOOSE_PLAYER_CHARACTER 0x12
#define MESSAGE_CHOOSE_CHARACTER 0x14
#define MESSAGE_FIRST_CHARACTER_NAME 0xA9
#define PROMPT_X 25
#define PROMPT_Y 180
#define CHARACTER_NAME_X 238
#define CHARACTER_NAME_Y 156
#define DRIVE_DISTANCE_X 246
#define DRIVE_DISTANCE_Y 200

#define CHARACTER_SELECT_IMAGE_KIND_COUNT (CHARACTER_SELECT_BUFFER_CURVE_CELLS + 1)

#define CHARACTER_CURSOR_ORIGIN_X 140
#define CHARACTER_CURSOR_ORIGIN_Y 96
#define HALF_PIXEL 0.5f

typedef struct TagOffset {
    s8 x;
    s8 y;
} TagOffset;

void DrawGridFaces(Gfx **displayListHead, UBoolean8 isSinglePlayer);
void DrawCharacterBoard(Gfx **displayListHead);
void DrawPlayerTags(Gfx **displayListHead, u8 tagMode);
void DrawComputerIcon(Gfx **displayListHead, s32 unused);
void DrawLeftyIcon(Gfx **displayListHead, s32 unused);
void DrawCurveMeter(Gfx **displayListHead, s32 clubSet);

static UBoolean8 f_IsPromptShown = FALSE;
static UBoolean8 f_UnreadBufferFlag = FALSE;
static u8 f_UnreadSelectByte UNUSED = 0;
static u8 f_UnreadSelectDoneByte = 0;
static s8 f_FaceFrame = 0;
static u8 f_UnreadFaceToggle = 0;
static s8 f_FaceFrameTime = 0;
static Boolean8 f_IsComputerButtonHeld = FALSE;
static Boolean8 f_IsLeftyButtonHeld = FALSE;
static CButtonPalette8 f_PreviewedPalette = NO_PALETTE;
static s32 f_UnreadSelectWords[4] UNUSED = {-1, -1, -1, -1};
static s32 f_SavedGameBoySlots[GAME_BOY_CHARACTER_COUNT] =
    {EMPTY_GRID_SLOT, EMPTY_GRID_SLOT, EMPTY_GRID_SLOT, EMPTY_GRID_SLOT};
static s32 f_SelectBufferAssets[CHARACTER_SELECT_IMAGE_KIND_COUNT] =
    {0x08E8, 0x05E3, 0x08EF, 0x050F, 0x0521, 0x06EF, 0x05FF, 0x0600, 0x08EE};
static CharacterIndex f_ChoiceCharacters[CHARACTER_COUNT] = {
    CHARACTER_PLUM,
    CHARACTER_CHARLIE,
    CHARACTER_PEACH,
    CHARACTER_BABY_MARIO,
    CHARACTER_LUIGI,
    CHARACTER_YOSHI,
    CHARACTER_SONNY,
    CHARACTER_WARIO,
    CHARACTER_HARRY,
    CHARACTER_MARIO,
    CHARACTER_MAPLE,
    CHARACTER_DONKEY_KONG,
    CHARACTER_BOWSER,
    CHARACTER_METAL_MARIO,
    CHARACTER_KID,
    CHARACTER_SHERRY,
    CHARACTER_AZALEA,
    CHARACTER_JOE,
};
static u8 f_UnreadSelectBytes[8] UNUSED = {0x71, 0x76, 0x81, 0x6C, 0x9D, 0x00, 0x00, 0x00};
static s32 f_StickXHoldTimer = 0;
static s32 f_StickYHoldTimer = 0;
static char f_UnreadCurveNames[3][10] UNUSED = {"Straight", "Draw", "Fade"};
static s16 f_UnreadFaceColumn = 0;
static s16 f_UnreadFaceRow = 0;
static s16 f_UnreadSelectHalfword UNUSED = 0;
static TagOffset f_PlayerTagOffsets[PLAYER_COUNT_MAX] = {{0, 0}, {16, 0}, {0, 24}, {16, 24}};
static s32 f_LinedUpTagLefts[PLAYER_COUNT_MAX][PLAYER_COUNT_MAX] =
    {{102, -26, -26, -26}, {22, 182, -26, -26}, {38, 102, 166, 230}, {6, 70, 134, 198}};
static s16 f_UnreadSelectOrder[16] UNUSED = {1, 4, 5, 6, 3, 2, 7, 9, 12, 10, 13, 15, 14, 11, 8, 0};
static Vtx f_GridCursorVertices[MENU_CURSOR_VERTEX_COUNT] = {
    {{{-24, 24, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-24, -24, 0}, 0, {0, 3072}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{24, -24, 0}, 0, {3072, 3072}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{24, 24, 0}, 0, {3072, 0}, {0xFF, 0xFF, 0x00, 0xFF}}},
};
static s32 f_UnreadSelectPair[2] UNUSED = {0, 0};
static f32 f_GridCursorPhases[MENU_CURSOR_VERTEX_COUNT] = {0.0f, 2.0f, 4.0f, 6.0f};
static s32 f_UnreadSelectQuad[4] UNUSED = {0, 0, 0, 0};

static Mtx f_GridCursorProjection;
static Mtx f_GridCursorTranslations[DRAW_BUFFER_COUNT];
static Mtx f_GridCursorRotations[DRAW_BUFFER_COUNT];
static Mtx f_GridCursorScales[DRAW_BUFFER_COUNT];

static u8 f_UnreadSelectPad[0x8] UNUSED;

static ImageAsset *f_CharacterBoard;

static u8 f_UnreadSelectGap[0x4] UNUSED;

static ImageAsset *f_PlayerTags;
static ImageAsset *f_GridCursorBox;
static ImageAsset *f_UnreadNamePlate;
static ImageAsset *f_UnreadControllerIcons;
static void *f_UnreadColorBars;
static ImageAsset *f_CurveMeterCells;

static u8 f_UnreadSelectBlock[0x208] UNUSED;

s32 f_PreviousStickX;
s32 f_PreviousStickY;

void HideGameBoyCharacters(void) {
    s32 firstSlot = g_CharacterGrid[GAME_BOY_CHARACTER_ROW][0];
    s32 secondSlot = g_CharacterGrid[GAME_BOY_CHARACTER_ROW][1];
    s32 thirdSlot = g_CharacterGrid[GAME_BOY_CHARACTER_ROW][2];
    s32 fourthSlot = g_CharacterGrid[GAME_BOY_CHARACTER_ROW][3];

    g_CharacterGrid[GAME_BOY_CHARACTER_ROW][0] = EMPTY_GRID_SLOT;
    g_CharacterGrid[GAME_BOY_CHARACTER_ROW][1] = EMPTY_GRID_SLOT;
    g_CharacterGrid[GAME_BOY_CHARACTER_ROW][2] = EMPTY_GRID_SLOT;
    g_CharacterGrid[GAME_BOY_CHARACTER_ROW][3] = EMPTY_GRID_SLOT;
    f_SavedGameBoySlots[0] = firstSlot;
    f_SavedGameBoySlots[1] = secondSlot;
    f_SavedGameBoySlots[2] = thirdSlot;
    f_SavedGameBoySlots[3] = fourthSlot;
}

Boolean MoveGridCursor(s32 playerIndex, GridMove direction, UBoolean8 canChooseMarked) {
    EncodedAsset asset;
    AssetFile file;
    MenuMemory *memory = GetMenuMemory();
    s16 oldColumn = memory->characterColumns[playerIndex];
    s16 oldRow = memory->characterRows[playerIndex];
    s16 column = oldColumn;
    s16 row = oldRow;
    u16 faceSlot;
    u16 characterIndex;
    ImageAsset *faceImage;
    s32 unused[2] UNUSED;

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
    }
    while (g_CharacterGrid[row][column] < 0 &&
        (g_CharacterGrid[row][column] < CHOOSABLE_MARK_LIMIT && canChooseMarked == TRUE) == FALSE);
    memory->characterColumns[playerIndex] = column;
    memory->characterRows[playerIndex] = row;
    if (column != oldColumn || row != oldRow) {
        characterIndex = g_CharacterGrid[oldRow][oldColumn];
        f_FaceFrame = 0;
        f_FaceFrameTime = 0;
        f_UnreadFaceToggle ^= 1;
        if (g_CharacterGrid[oldRow][oldColumn] < CHOOSABLE_MARK_LIMIT) {
            characterIndex += MARKED_CHARACTER_OFFSET;
        }

        if ((s16)characterIndex >= FIRST_IMPORTED_CHARACTER) {
            faceSlot = characterIndex;
            characterIndex =
                g_ImportedCharacters[(s16)faceSlot - FIRST_IMPORTED_CHARACTER].characterIndex;
        }
        else {
            faceSlot = characterIndex;
        }

        faceImage = g_CharacterFaceImages[(s16)faceSlot];
        if (IsGameFlagSet(GAME_FLAG_ALTERNATE_SELECT_FACES)) {
            OpenEncodedAsset(
                g_AlternateFaceAnimationAssets[(s16)characterIndex][f_FaceFrame],
                &asset
            );
        }
        else {
            OpenEncodedAsset(g_FaceAnimationAssets[(s16)characterIndex][f_FaceFrame], &asset);
        }

        ReadEncodedAsset(faceImage, &asset);
        OpenAssetFile(g_CharacterFacePaletteFiles[(s16)characterIndex][0], &file);
        ReadAssetFile(g_CharacterFacePalettes[(s16)faceSlot], (s16)GetAssetFileSize(&file), &file);
    }

    return -(column == oldColumn && row == oldRow);
}

Boolean MoveGridCursorDiagonal(
    s32 playerIndex,
    UStickDiagonal8 diagonal,
    UBoolean8 canChooseMarked
) {
    EncodedAsset asset;
    AssetFile file;
    MenuMemory *memory = GetMenuMemory();
    s32 retryCount = 0;
    s16 oldRow = memory->characterRows[playerIndex];
    s16 oldColumn = memory->characterColumns[playerIndex];
    s32 column = oldColumn;
    s32 row = oldRow;
    u16 faceSlot;
    u16 characterIndex;
    ImageAsset *faceImage;
    s32 unused[2] UNUSED;

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
        }
        else {
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

        if (retryCount == 0) {
            column = oldColumn;
            row = oldRow;
        }

        retryCount++;
    }

    memory->characterColumns[playerIndex] = column;
    memory->characterRows[playerIndex] = row;
    if (column != oldColumn || row != oldRow) {
        characterIndex = g_CharacterGrid[oldRow][oldColumn];
        f_FaceFrame = 0;
        f_FaceFrameTime = 0;
        f_UnreadFaceToggle ^= 1;
        if (g_CharacterGrid[oldRow][oldColumn] < CHOOSABLE_MARK_LIMIT) {
            characterIndex += MARKED_CHARACTER_OFFSET;
        }

        if ((s16)characterIndex >= FIRST_IMPORTED_CHARACTER) {
            faceSlot = characterIndex;
            characterIndex =
                g_ImportedCharacters[(s16)faceSlot - FIRST_IMPORTED_CHARACTER].characterIndex;
        }
        else {
            faceSlot = characterIndex;
        }

        faceImage = g_CharacterFaceImages[(s16)faceSlot];
        if (IsGameFlagSet(GAME_FLAG_ALTERNATE_SELECT_FACES)) {
            OpenEncodedAsset(
                g_AlternateFaceAnimationAssets[(s16)characterIndex][f_FaceFrame],
                &asset
            );
        }
        else {
            OpenEncodedAsset(g_FaceAnimationAssets[(s16)characterIndex][f_FaceFrame], &asset);
        }

        ReadEncodedAsset(faceImage, &asset);
        OpenAssetFile(g_CharacterFacePaletteFiles[(s16)characterIndex][0], &file);
        ReadAssetFile(g_CharacterFacePalettes[(s16)faceSlot], GetAssetFileSize(&file), &file);
    }

    if (column == oldColumn && row == oldRow) {
        return -TRUE;
    }

    return FALSE;
}

StickDiagonal GetStickDiagonal(void) {
    s32 stickX;
    s32 stickY;
    s8 anyStick;

    GetMenuMemory();
    anyStick = g_ControllerStates[3].stickX |
        (g_ControllerStates[2].stickX |
            (g_ControllerStates[0].stickX | g_ControllerStates[1].stickX));
    stickX = f_PreviousStickX ^ anyStick;
    stickX &= anyStick;
    if (f_PreviousStickX == stickX) {
        f_StickXHoldTimer++;
        if (f_StickXHoldTimer >= STICK_HOLD_FRAME_COUNT) {
            f_StickXHoldTimer = 0;
            f_PreviousStickX = 0;
        }
    }
    else {
        f_PreviousStickX = stickX;
    }

    anyStick = g_ControllerStates[3].stickY |
        (g_ControllerStates[2].stickY |
            (g_ControllerStates[0].stickY | g_ControllerStates[1].stickY));
    stickY = f_PreviousStickY ^ anyStick;
    stickY &= anyStick;
    if (f_PreviousStickY == stickY) {
        f_StickYHoldTimer++;
        if (f_StickYHoldTimer >= STICK_HOLD_FRAME_COUNT) {
            f_StickYHoldTimer = 0;
            f_PreviousStickY = 0;
        }
    }
    else {
        f_PreviousStickY = stickY;
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

#define CHARACTER_SELECT_TEXT_SLOT_COUNT 4
#define CHARACTER_SELECT_PROMPT_SLOT 0

#define Z_OR_L_BUTTONS (Z_TRIG | L_TRIG)

#define BLANKED_DEBUG_TEXT_COLUMN 15
#define BLANKED_DEBUG_TEXT_ROW 10

#define PLAYER_COUNT_WITHOUT_CONTROLLER_MIN 2

s32 UpdateCharacterSelect(s32 firstPlayerIndex, s32 playerCount, Boolean canChooseMarked) {
    EncodedAsset asset;
    AssetFile file;
    s32 sameCharacterPlayers[PLAYER_COUNT_MAX];
    MenuMemory *memory = GetMenuMemory();
    s32 playerIndex;
    s32 index;
    s32 otherIndex;
    s16 sameCharacterCount;
    CharacterIndex characterIndex;
    s16 gridMark;
    s32 controllerIndex;
    u16 button;
    CButtonPalette paletteIndex;
    u16 column;
    s8 chosenColumn;
    u16 row;
    u16 faceSlot;
    u16 faceCharacter;
    ImageAsset *faceImage;
    SoundChannel channel;

    if (IsHudAnimationDone() != TRUE) {
        return UNSET_RESULT;
    }

    if (GetWipeKind() != WIPE_NONE) {
        for (index = 0; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                index,
                0,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }

        if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
            g_MenuScreen = MENU_SCREEN_EXIT;
            g_GameState.roundSeed = (g_TournamentCode[0] << (2 * BITS_PER_BYTE)) |
                (g_TournamentCode[1] << BITS_PER_BYTE) | g_TournamentCode[2];
        }

        return UNSET_RESULT;
    }

    playerIndex = g_ChoosingPlayerIndex;
    if (g_MenuRepeatButtons == (U_JPAD | L_JPAD)) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        if (MoveGridCursorDiagonal(playerIndex, STICK_DIAGONAL_UP_LEFT, (u8)canChooseMarked)) {
            return 0;
        }

        for (index = 1; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                index,
                0,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }

        return 0;
    }

    if (g_MenuRepeatButtons == (U_JPAD | R_JPAD)) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        if (MoveGridCursorDiagonal(playerIndex, STICK_DIAGONAL_UP_RIGHT, (u8)canChooseMarked)) {
            return 0;
        }

        for (index = 1; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                index,
                0,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }

        return 0;
    }

    if (g_MenuRepeatButtons == (D_JPAD | L_JPAD)) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        if (MoveGridCursorDiagonal(playerIndex, STICK_DIAGONAL_DOWN_LEFT, (u8)canChooseMarked)) {
            return 0;
        }

        for (index = 1; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                index,
                0,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }

        return 0;
    }

    if (g_MenuRepeatButtons == (D_JPAD | R_JPAD)) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        if (MoveGridCursorDiagonal(playerIndex, STICK_DIAGONAL_DOWN_RIGHT, (u8)canChooseMarked)) {
            return 0;
        }

        for (index = 1; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                index,
                0,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }

        return 0;
    }

    if (g_MenuRepeatButtons & U_JPAD) {
        PrintDebugText("          ", BLANKED_DEBUG_TEXT_COLUMN, BLANKED_DEBUG_TEXT_ROW);
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        if (MoveGridCursor(playerIndex, GRID_MOVE_UP, (u8)canChooseMarked)) {
            return 0;
        }

        for (index = 1; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                index,
                0,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }

        return 0;
    }

    if (g_MenuRepeatButtons & D_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        if (MoveGridCursor(playerIndex, GRID_MOVE_DOWN, (u8)canChooseMarked)) {
            return 0;
        }

        for (index = 1; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                index,
                0,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }

        return 0;
    }

    if (g_MenuRepeatButtons & L_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        if (MoveGridCursor(playerIndex, GRID_MOVE_LEFT, (u8)canChooseMarked)) {
            return 0;
        }

        for (index = 1; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                index,
                0,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }

        return 0;
    }

    if (g_MenuRepeatButtons & R_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        if (MoveGridCursor(playerIndex, GRID_MOVE_RIGHT, (u8)canChooseMarked)) {
            return 0;
        }

        for (index = 1; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                index,
                0,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }

        return 0;
    }

    if (g_MenuPressedButtons & A_BUTTON) {
        for (index = 0; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                index,
                0,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }

        characterIndex = g_CharacterGrid[memory->characterRows[playerIndex]]
                                        [memory->characterColumns[playerIndex]];
        if (((u8)canChooseMarked == TRUE) & (characterIndex < CHOOSABLE_MARK_LIMIT)) {
            characterIndex += MARKED_CHARACTER_OFFSET;
        }

        characterIndex = f_ChoiceCharacters[characterIndex];
        if (characterIndex >= FIRST_IMPORTED_CHARACTER) {
            if (g_ImportedCharacters[characterIndex - FIRST_IMPORTED_CHARACTER].isChosen == TRUE) {
                PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                f_IsPromptShown = FALSE;
                return UNSET_RESULT;
            }

            index = characterIndex - FIRST_IMPORTED_CHARACTER;
            characterIndex = g_ImportedCharacters[index].characterIndex;
            g_ImportedCharacters[index].isChosen = TRUE;
            g_GameState.isImportedCharacterChosen = TRUE;
            g_ImportedCharacterColumns[playerIndex] = memory->characterColumns[playerIndex];
            if (g_ImportedCharacterColumns[playerIndex] < 0) {
                g_ImportedCharacterColumns[playerIndex] = 0;
            }

            if (g_ImportedCharacterColumns[playerIndex] >= IMPORTED_CHARACTER_COUNT) {
                g_ImportedCharacterColumns[playerIndex] = IMPORTED_CHARACTER_COUNT - 1;
            }
        }

        for (controllerIndex = 0; controllerIndex != g_ControllerCount; controllerIndex++) {
            button = g_MenuControllerData[controllerIndex].button;
            if (button & A_BUTTON) {
                if ((u8)canChooseMarked == FALSE) {
                    if (((button & R_TRIG) != 0) &
                            (playerCount >= PLAYER_COUNT_WITHOUT_CONTROLLER_MIN) &&
                        characterIndex < FIRST_IMPORTED_CHARACTER) {
                        if (g_ChosenPlayerCount - 1 == playerIndex &&
                            !IsGameFlagSet(GAME_FLAG_ALLOW_ALL_COMPUTER_PLAYERS)) {
                            for (otherIndex = 0; otherIndex != playerIndex; otherIndex++) {
                                if (g_PlayerControllerIndices[otherIndex] != NO_CONTROLLER_INDEX) {
                                    break;
                                }
                            }

                            if (otherIndex == playerIndex) {
                                g_PlayerControllerIndices[playerIndex] = controllerIndex;
                            }
                            else {
                                g_PlayerControllerIndices[playerIndex] = NO_CONTROLLER_INDEX;
                            }
                        }
                        else {
                            g_PlayerControllerIndices[playerIndex] = NO_CONTROLLER_INDEX;
                        }
                    }
                    else {
                        g_PlayerControllerIndices[playerIndex] = controllerIndex;
                    }
                }
                else {
                    g_PlayerControllerIndices[playerIndex] = NO_CONTROLLER_INDEX;
                }

                g_ChoosingControllerIndices[playerIndex] = controllerIndex;
                SetTextSlot(
                    0,
                    0,
                    NO_MESSAGE,
                    TEXT_SLOT_LARGE_FONT,
                    CHARACTER_SELECT_PROMPT_SLOT,
                    TEXT_FLAG_TYPED,
                    255,
                    255,
                    255,
                    OPAQUE_ALPHA
                );
                f_IsPromptShown = FALSE;
                break;
            }
        }

        g_ChosenCharacters[playerIndex] = characterIndex;
        g_Players[playerIndex].characterIndex = characterIndex;
        if (g_MenuRepeatButtons & Z_OR_L_BUTTONS) {
            if (characterIndex < FIRST_IMPORTED_CHARACTER) {
                g_ChosenLefties[playerIndex] = TRUE;
            }
        }

        if (characterIndex >= FIRST_IMPORTED_CHARACTER) {
            g_ChosenLefties[playerIndex] =
                g_GameBoyCharacters[memory->characterColumns[playerIndex]].isLefty;
        }

        if (g_MenuRepeatButtons & U_CBUTTONS) {
            paletteIndex = UP_C_PALETTE;
        }
        else if (g_MenuRepeatButtons & D_CBUTTONS) {
            paletteIndex = DOWN_C_PALETTE;
        }
        else if (g_MenuRepeatButtons & L_CBUTTONS) {
            paletteIndex = LEFT_C_PALETTE;
        }
        else if (g_MenuRepeatButtons & R_CBUTTONS) {
            paletteIndex = RIGHT_C_PALETTE;
        }
        else {
            paletteIndex = UP_C_PALETTE;
        }

        for (index = 0, sameCharacterCount = 0; index != playerIndex; index++) {
            if (g_Players[index].characterIndex == g_Players[playerIndex].characterIndex) {
                sameCharacterPlayers[sameCharacterCount] = index;
                sameCharacterCount++;
            }
        }

        for (index = 0; index != sameCharacterCount; index++) {
            if (g_Players[sameCharacterPlayers[index]].paletteIndex == paletteIndex) {
                break;
            }
        }

        if (index == sameCharacterCount) {
            g_Players[playerIndex].paletteIndex = paletteIndex;
        }
        else {
            for (index = 0; index != CHARACTER_PALETTE_COUNT; index++) {
                for (otherIndex = 0; otherIndex != sameCharacterCount; otherIndex++) {
                    if (g_Players[sameCharacterPlayers[otherIndex]].paletteIndex == index) {
                        break;
                    }
                }

                if (otherIndex == sameCharacterCount) {
                    g_Players[playerIndex].paletteIndex = index;
                    break;
                }
            }

            if (index == CHARACTER_PALETTE_COUNT) {
                g_Players[playerIndex].paletteIndex = UP_C_PALETTE;
            }
        }

        column = memory->characterColumns[playerIndex];
        row = memory->characterRows[playerIndex];
        playerIndex++;
        f_FaceFrame = 0;
        f_FaceFrameTime = 0;
        f_UnreadFaceToggle ^= 1;
        if (playerIndex != playerCount) {
            gridMark = g_CharacterGrid[(s16)row][(s16)column];
            faceCharacter = g_CharacterGrid[(s16)row][(s16)column];
            if (gridMark < CHOOSABLE_MARK_LIMIT) {
                faceSlot = faceCharacter + MARKED_CHARACTER_OFFSET;
                faceCharacter = faceSlot;
            }
            else if (gridMark >= FIRST_IMPORTED_CHARACTER) {
                faceSlot = faceCharacter;
                faceCharacter =
                    g_ImportedCharacters[gridMark - FIRST_IMPORTED_CHARACTER].characterIndex;
            }
            else {
                faceSlot = faceCharacter;
            }

            faceImage = g_CharacterFaceImages[(s16)faceSlot];
            if (IsGameFlagSet(GAME_FLAG_ALTERNATE_SELECT_FACES)) {
                OpenEncodedAsset(
                    g_AlternateFaceAnimationAssets[(s16)faceCharacter][f_FaceFrame],
                    &asset
                );
            }
            else {
                OpenEncodedAsset(g_FaceAnimationAssets[(s16)faceCharacter][f_FaceFrame], &asset);
            }

            ReadEncodedAsset(faceImage, &asset);
            OpenAssetFile(g_CharacterFacePaletteFiles[(s16)faceCharacter][0], &file);
            ReadAssetFile(g_CharacterFacePalettes[(s16)faceSlot], GetAssetFileSize(&file), &file);
        }

        if (playerIndex == playerCount) {
            if ((u8)canChooseMarked == FALSE) {
                if (g_MenuScreen == MENU_SCREEN_TOURNAMENT_CHARACTER_SELECT) {
                    LoadWipeShape(GetClubTournamentCourse());
                    StartWipe(WIPE_CLOSING);
                }
                else {
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_MODE_SELECT;
                }
            }
            else {
                StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                g_MenuScreen = MENU_SCREEN_COURSE_SELECT;
                g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
                COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_PLAIN;
                g_BetSet = BET_SET_NONE;
            }

            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                CHARACTER_SELECT_PROMPT_SLOT,
                TEXT_FLAG_TYPED,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
            f_IsPromptShown = FALSE;
            FreeAllCharacterSlots();
            f_UnreadSelectDoneByte = 0;
            f_FaceFrame = 0;
            f_UnreadFaceToggle = 0;
            f_FaceFrameTime = 0;
        }

        f_PreviewedPalette = NO_PALETTE;
        g_ChoosingPlayerIndex = playerIndex;
        f_IsComputerButtonHeld = FALSE;
        f_IsLeftyButtonHeld = FALSE;
        if (playerIndex == 1) {
            channel = FIRST_CHARACTER_SOUND_CHANNEL;
        }
        else if (playerIndex == 2) {
            channel = FIRST_CHARACTER_SOUND_CHANNEL + 1;
        }
        else if (playerIndex == 3) {
            channel = FIRST_CHARACTER_SOUND_CHANNEL + 2;
        }
        else if (playerIndex == 4) {
            channel = FIRST_CHARACTER_SOUND_CHANNEL + 3;
        }
        else {
            channel = MENU_SOUND_CHANNEL;
        }

        if (rand() & 1) {
            PlayCharacterSound(
                characterIndex,
                channel - FIRST_CHARACTER_SOUND_CHANNEL,
                CHARACTER_SOUND_CHOSEN,
                SOUND_FULL_SCALE,
                SOUND_FULL_SCALE
            );
        }
        else {
            PlayCharacterSound(
                characterIndex,
                channel - FIRST_CHARACTER_SOUND_CHANNEL,
                CHARACTER_SOUND_OTHER_CHOSEN,
                SOUND_FULL_SCALE,
                SOUND_FULL_SCALE
            );
        }

        return 0;
    }

    if (g_MenuPressedButtons & B_BUTTON) {
        for (index = 0; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                index,
                0,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }

        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        column = memory->characterColumns[playerIndex];
        row = memory->characterRows[playerIndex];
        if (playerIndex == firstPlayerIndex) {
            StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
            if (g_MenuScreen != MENU_SCREEN_OPPONENT_SELECT) {
                memory->modeChoice = 0;
            }

            if ((u8)canChooseMarked == FALSE) {
                if (g_MenuScreen == MENU_SCREEN_TOURNAMENT_CHARACTER_SELECT) {
                    g_GameState.isClubTournament = FALSE;
                    g_MenuScreen = MENU_SCREEN_CLUB_TOURNAMENT;
                }
                else {
                    g_MenuScreen = MENU_SCREEN_TITLE;
                }
            }
            else {
                g_MenuScreen = MENU_SCREEN_MODE_SELECT;
            }

            g_ChoosingControllerIndices[playerIndex] = UNCHOSEN_CONTROLLER;
            g_PlayerControllerIndices[playerIndex] = UNCHOSEN_CONTROLLER;
            characterIndex = g_CharacterGrid[memory->characterRows[playerIndex]]
                                            [memory->characterColumns[playerIndex]];
            if (characterIndex >= FIRST_IMPORTED_CHARACTER) {
                g_ImportedCharacters[characterIndex - FIRST_IMPORTED_CHARACTER].isChosen = FALSE;
                g_ImportedCharacterColumns[playerIndex] = NO_IMPORTED_COLUMN;
            }

            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                CHARACTER_SELECT_PROMPT_SLOT,
                TEXT_FLAG_TYPED,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
            f_IsPromptShown = FALSE;
            f_FaceFrame = 0;
            f_UnreadFaceToggle = 0;
            f_FaceFrameTime = 0;
            if ((u8)canChooseMarked == TRUE) {
                g_CharacterGrid[GAME_BOY_CHARACTER_ROW][0] = f_SavedGameBoySlots[0];
                g_CharacterGrid[GAME_BOY_CHARACTER_ROW][1] = f_SavedGameBoySlots[1];
                g_CharacterGrid[GAME_BOY_CHARACTER_ROW][2] = f_SavedGameBoySlots[2];
                g_CharacterGrid[GAME_BOY_CHARACTER_ROW][3] = f_SavedGameBoySlots[3];
            }
        }
        else {
            g_Players[playerIndex].isLefty = FALSE;
            playerIndex--;
            f_FaceFrame = 0;
            f_FaceFrameTime = 0;
            f_UnreadFaceToggle ^= 1;
            g_ChoosingControllerIndices[playerIndex] = UNCHOSEN_CONTROLLER;
            g_PlayerControllerIndices[playerIndex] = UNCHOSEN_CONTROLLER;
            characterIndex = g_CharacterGrid[memory->characterRows[playerIndex]]
                                            [memory->characterColumns[playerIndex]];
            if (characterIndex >= FIRST_IMPORTED_CHARACTER) {
                g_ImportedCharacters[characterIndex - FIRST_IMPORTED_CHARACTER].isChosen = FALSE;
                g_ImportedCharacterColumns[playerIndex] = NO_IMPORTED_COLUMN;
            }

            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                CHARACTER_SELECT_PROMPT_SLOT,
                TEXT_FLAG_TYPED,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
            f_IsPromptShown = FALSE;
        }

        gridMark = g_CharacterGrid[(s16)row][(s16)column];
        faceCharacter = g_CharacterGrid[(s16)row][(s16)column];
        if (gridMark < CHOOSABLE_MARK_LIMIT) {
            faceSlot = faceCharacter + MARKED_CHARACTER_OFFSET;
            faceCharacter = faceSlot;
        }
        else if (gridMark >= FIRST_IMPORTED_CHARACTER) {
            faceSlot = faceCharacter;
            faceCharacter =
                g_ImportedCharacters[gridMark - FIRST_IMPORTED_CHARACTER].characterIndex;
        }
        else {
            faceSlot = faceCharacter;
        }

        faceImage = g_CharacterFaceImages[(s16)faceSlot];
        if (IsGameFlagSet(GAME_FLAG_ALTERNATE_SELECT_FACES)) {
            OpenEncodedAsset(
                g_AlternateFaceAnimationAssets[(s16)faceCharacter][f_FaceFrame],
                &asset
            );
        }
        else {
            OpenEncodedAsset(g_FaceAnimationAssets[(s16)faceCharacter][f_FaceFrame], &asset);
        }

        ReadEncodedAsset(faceImage, &asset);
        OpenAssetFile(g_CharacterFacePaletteFiles[(s16)faceCharacter][0], &file);
        ReadAssetFile(g_CharacterFacePalettes[(s16)faceSlot], GetAssetFileSize(&file), &file);
        g_ChoosingPlayerIndex = playerIndex;
        f_IsComputerButtonHeld = FALSE;
        f_IsLeftyButtonHeld = FALSE;
        f_PreviewedPalette = NO_PALETTE;
    }
    else {
        button = g_ControllerStates[3].buttons |
            (g_ControllerStates[2].buttons |
                (g_ControllerStates[0].buttons | g_ControllerStates[1].buttons));
        if (button & U_CBUTTONS) {
            paletteIndex = UP_C_PALETTE;
        }
        else if (button & D_CBUTTONS) {
            paletteIndex = DOWN_C_PALETTE;
        }
        else if (button & L_CBUTTONS) {
            paletteIndex = LEFT_C_PALETTE;
        }
        else if (button & R_CBUTTONS) {
            paletteIndex = RIGHT_C_PALETTE;
        }
        else {
            paletteIndex = NO_PALETTE;
        }

        if (paletteIndex != NO_PALETTE) {
            chosenColumn = memory->characterColumns[playerIndex];
            characterIndex = g_CharacterGrid[memory->characterRows[playerIndex]][chosenColumn];
            if (((u8)canChooseMarked == TRUE) & (characterIndex < CHOOSABLE_MARK_LIMIT)) {
                characterIndex += MARKED_CHARACTER_OFFSET;
            }

            characterIndex = f_ChoiceCharacters[characterIndex];
            if (characterIndex >= FIRST_IMPORTED_CHARACTER) {
                if (g_ImportedCharacters[characterIndex - FIRST_IMPORTED_CHARACTER].isChosen ==
                    TRUE) {
                    return UNSET_RESULT;
                }

                characterIndex =
                    g_ImportedCharacters[characterIndex - FIRST_IMPORTED_CHARACTER].characterIndex;
                g_ImportedCharacterColumns[playerIndex] = chosenColumn;
                if (chosenColumn < 0) {
                    g_ImportedCharacterColumns[playerIndex] = 0;
                }

                if (g_ImportedCharacterColumns[playerIndex] >= IMPORTED_CHARACTER_COUNT) {
                    g_ImportedCharacterColumns[playerIndex] = IMPORTED_CHARACTER_COUNT - 1;
                }
            }

            for (index = 0, sameCharacterCount = 0; index != playerIndex; index++) {
                if (g_Players[index].characterIndex == characterIndex) {
                    sameCharacterPlayers[sameCharacterCount] = index;
                    sameCharacterCount++;
                }
            }

            for (index = 0; index != sameCharacterCount; index++) {
                if (g_Players[sameCharacterPlayers[index]].paletteIndex == paletteIndex) {
                    break;
                }
            }

            if (index == sameCharacterCount) {
                f_PreviewedPalette = paletteIndex;
            }
            else {
                for (index = 0; index != CHARACTER_PALETTE_COUNT; index++) {
                    for (otherIndex = 0; otherIndex != sameCharacterCount; otherIndex++) {
                        if (g_Players[sameCharacterPlayers[otherIndex]].paletteIndex == index) {
                            break;
                        }
                    }

                    if (otherIndex == sameCharacterCount) {
                        f_PreviewedPalette = index;
                        break;
                    }
                }

                if (index == CHARACTER_PALETTE_COUNT) {
                    f_PreviewedPalette = UP_C_PALETTE;
                }
            }
        }
        else {
            f_PreviewedPalette = NO_PALETTE;
        }

        if (((g_ControllerStates[3].buttons |
                 (g_ControllerStates[2].buttons |
                     (g_ControllerStates[0].buttons | g_ControllerStates[1].buttons))) &
                R_TRIG) &&
            memory->characterRows[playerIndex] != GAME_BOY_CHARACTER_ROW &&
            (u8)canChooseMarked == FALSE) {
            if (g_ChosenPlayerCount >= PLAYER_COUNT_WITHOUT_CONTROLLER_MIN) {
                if (g_ChoosingPlayerIndex == g_ChosenPlayerCount - 1) {
                    for (index = 0; index != g_ChoosingPlayerIndex; index++) {
                        if (g_PlayerControllerIndices[index] != NO_CONTROLLER_INDEX) {
                            break;
                        }
                    }

                    if (index < g_ChosenPlayerCount - 1) {
                        f_IsComputerButtonHeld = TRUE;
                    }
                }
                else {
                    f_IsComputerButtonHeld = TRUE;
                }
            }
        }
        else {
            f_IsComputerButtonHeld = FALSE;
        }

        if (((g_ControllerStates[3].buttons |
                 (g_ControllerStates[2].buttons |
                     (g_ControllerStates[0].buttons | g_ControllerStates[1].buttons))) &
                Z_OR_L_BUTTONS) &&
            memory->characterRows[playerIndex] != GAME_BOY_CHARACTER_ROW) {
            f_IsLeftyButtonHeld = TRUE;
        }
        else {
            f_IsLeftyButtonHeld = FALSE;
        }
    }

    return 0;
}

void LoadCharacterSelectBuffer(void *buffer, CharacterSelectBuffer bufferKind) {
    EncodedAsset asset;
    ImageAsset *image = buffer;

    switch (bufferKind) {
        case CHARACTER_SELECT_BUFFER_PLAYER_LABELS:
            f_PlayerTags = buffer;
            break;
        case CHARACTER_SELECT_BUFFER_BOARD:
            f_CharacterBoard = buffer;
            break;
        case CHARACTER_SELECT_BUFFER_CURSOR_BOX:
            f_GridCursorBox = buffer;
            break;
        case CHARACTER_SELECT_BUFFER_CONTROLLER_ICONS:
            f_UnreadControllerIcons = buffer;
            break;
        case CHARACTER_SELECT_BUFFER_COLOR_BARS:
            f_UnreadColorBars = buffer;
            break;
        case CHARACTER_SELECT_BUFFER_CURVE_CELLS:
            f_CurveMeterCells = buffer;
            break;
        case CHARACTER_SELECT_BUFFER_UNREAD:
            f_UnreadNamePlate = buffer;
            break;
        default:
            f_PlayerTags = buffer;
            bufferKind = CHARACTER_SELECT_BUFFER_UNREAD;
            break;
    }

    if (bufferKind != CHARACTER_SELECT_BUFFER_UNREAD) {
        OpenEncodedAsset(f_SelectBufferAssets[bufferKind], &asset);
        ReadEncodedAsset(image, &asset);
    }

    f_UnreadBufferFlag = FALSE;
}

void PlaceRivalCursor(void) {
    MenuMemory *memory = GetMenuMemory();
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

void LoadCharacterFaces(void) {
    EncodedAsset asset;
    AssetFile file;
    CharacterIndex characterIndex = 0;
    u8 *images = GetMenuImageBuffer();
    s32 slotIndex;
    AssetId faceAsset;

    OpenEncodedAsset(ASSET_SHOT_TYPES, &asset);
    ReadEncodedAsset(images, &asset);
    OpenEncodedAsset(ASSET_STAT_LABELS, &asset);
    ReadEncodedAsset(images + CHARACTER_SELECT_FIRST_IMAGE_SIZE, &asset);
    for (slotIndex = 0; slotIndex != CHARACTER_COUNT; slotIndex++) {
        ImageAsset *faceImage = g_CharacterFaceImages[slotIndex];
        AssetFile *paletteFile = &file;
        PaletteColor *palette = g_CharacterFacePalettes[slotIndex];

        if (slotIndex >= FIRST_IMPORTED_CHARACTER) {
            if (g_ImportedCharacters[slotIndex - FIRST_IMPORTED_CHARACTER].characterIndex >= 0) {
                characterIndex =
                    g_ImportedCharacters[slotIndex - FIRST_IMPORTED_CHARACTER].characterIndex;
            }
        }
        else {
            characterIndex = slotIndex;
        }

        if (IsGameFlagSet(GAME_FLAG_ALTERNATE_SELECT_FACES)) {
            faceAsset = g_AlternateFaceAnimationAssets[characterIndex][0];
        }
        else {
            faceAsset = g_FaceAnimationAssets[characterIndex][0];
        }

        OpenEncodedAsset(faceAsset, &asset);
        ReadEncodedAsset(faceImage, &asset);
        OpenAssetFile(g_CharacterFacePaletteFiles[characterIndex][0], paletteFile);
        ReadAssetFile(palette, GetAssetFileSize(paletteFile), paletteFile);
    }
}

static inline s32 ByteAddress(s8 *byte) {
    return (s32)byte;
}

void DrawCharacterSelect(Gfx **displayListHead, UBoolean8 isSinglePlayer) {
    Gfx *gfx = *displayListHead;
    s32 playerIndex;
    s16 column;
    s16 gridMark;
    s16 choiceIndex;
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
    DrawMenuCourseScene(&gfx);
    DrawMenuBanner(&gfx);
    DrawCharacterBoard(&gfx);
    DrawGridFaces(&gfx, isSinglePlayer);
    if (isSinglePlayer == TRUE) {
        DrawPlayerTags(&gfx, 2);
    }
    else {
        DrawPlayerTags(&gfx, 1);
    }

    if ((f_IsComputerButtonHeld == TRUE) & (isSinglePlayer == FALSE)) {
        DrawComputerIcon(&gfx, 0);
    }

    if (f_IsLeftyButtonHeld == TRUE) {
        DrawLeftyIcon(&gfx, 0);
    }

    gDPPipeSync(gfx++);
    if (isSinglePlayer == FALSE) {
        playerIndex = g_ChoosingPlayerIndex;
        if (playerIndex >= g_ChosenPlayerCount) {
            playerIndex = g_ChosenPlayerCount - 1;
        }
    }
    else {
        playerIndex = 1;
    }

    column = *(s8 *)(ByteAddress(&GetMenuMemory()->characterColumns[1]) + (s16)playerIndex - 1);
    gridMark = g_CharacterGrid[*(s8 *)(ByteAddress(&GetMenuMemory()->characterRows[1]) +
        (s16)playerIndex - 1)][column];
    if (gridMark < CHOOSABLE_MARK_LIMIT) {
        choiceIndex = gridMark + MARKED_CHARACTER_OFFSET;
    }
    else {
        choiceIndex = gridMark;
    }

    DrawCurveMeter(&gfx, f_ChoiceCharacters[choiceIndex]);
    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    SetTextName(g_CharacterNames[choiceIndex]);
    SetTextSlot(
        CHARACTER_NAME_X,
        CHARACTER_NAME_Y,
        MESSAGE_FIRST_CHARACTER_NAME + choiceIndex,
        TEXT_SLOT_LARGE_FONT,
        1,
        TEXT_FLAG_CENTERED,
        255,
        255,
        255,
        OPAQUE_ALPHA
    );
    clubSet = f_ChoiceCharacters[choiceIndex];
    SetUpShadowedTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    sprintf(g_PrintBuffer, "%dY", g_Clubs[(clubSet * CLUB_SET_SIZE) + CLUB_TYPE_DRIVER].distance);
    DrawShadowedText(&gfx, DRIVE_DISTANCE_X, DRIVE_DISTANCE_Y, g_PrintBuffer);
    if (f_IsPromptShown == FALSE) {
        if (isSinglePlayer == TRUE) {
            SetTextSlot(
                PROMPT_X,
                PROMPT_Y,
                MESSAGE_CHOOSE_CHARACTER,
                TEXT_SLOT_LARGE_FONT,
                0,
                TEXT_FLAG_TYPED,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }
        else {
            SetTextSlotValue(g_ChoosingPlayerIndex + 1, 0, 0);

            if (g_ChoosingPlayerIndex == 0) {
                SetTextPaletteColor(255, 127, 127, 0);
            }

            if (g_ChoosingPlayerIndex == 1) {
                SetTextPaletteColor(0, 255, 255, 0);
            }

            if (g_ChoosingPlayerIndex == 2) {
                SetTextPaletteColor(255, 255, 0, 0);
            }

            if (g_ChoosingPlayerIndex == 3) {
                SetTextPaletteColor(63, 255, 63, 0);
            }

            SetTextSlot(
                PROMPT_X,
                PROMPT_Y,
                MESSAGE_CHOOSE_PLAYER_CHARACTER,
                TEXT_SLOT_LARGE_FONT,
                0,
                TEXT_FLAG_TYPED,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }

        f_IsPromptShown = TRUE;
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

s32 GetUnusedPalette(s32 choiceIndex);

#define FACE_SPACING 40
#define FACE_GRID_LEFT 24
#define FACE_GRID_TOP 56
#define HIDDEN_MARK_LIMIT (-10)
#define FACE_FRAME_DURATION 4

void DrawGridFaces(Gfx **displayListHead, UBoolean8 isSinglePlayer) {
    EncodedAsset asset;
    AssetFile file;
    Gfx *gfx = *displayListHead;
    MenuMemory *memory = GetMenuMemory();
    s16 row;
    s16 column;
    u16 faceSlot;
    s16 gridMark;
    u16 characterIndex;
    ImageAsset *faceImage;
    PaletteColor *facePalette;

    for (row = 0; row != CHARACTER_GRID_ROW_COUNT;) {
        for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT;) {
            faceSlot = g_CharacterGrid[row][column];
            if (g_MenuScreen == MENU_SCREEN_OPPONENT_SELECT &&
                row == memory->characterRows[0] &&
                column == memory->characterColumns[0] &&
                g_Players[0].characterIndex >= FIRST_IMPORTED_CHARACTER) {
                faceSlot = column + FIRST_IMPORTED_CHARACTER;
            }

            if ((s16)faceSlot >= 0) {
                if ((s16)faceSlot >= CHARACTER_COUNT) {
                    faceSlot = CHARACTER_COUNT - 1;
                }

                faceImage = g_CharacterFaceImages[(s16)faceSlot];
                facePalette = g_CharacterFacePalettes[(s16)faceSlot];
                gDPLoadTLUT_pal256(gfx++, TextureAddress(facePalette));
                gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress(faceImage),
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    FACE_FRAME_SIZE,
                    FACE_FRAME_SIZE,
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
                    QuarterPixels(column * FACE_SPACING + FACE_GRID_LEFT),
                    QuarterPixels(row * FACE_SPACING + FACE_GRID_TOP),
                    QuarterPixels(column * FACE_SPACING + FACE_GRID_LEFT + FACE_FRAME_SIZE),
                    QuarterPixels(row * FACE_SPACING + FACE_GRID_TOP + FACE_FRAME_SIZE),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                gDPPipeSync(gfx++);
            }
            else if (isSinglePlayer == TRUE && g_CharacterGrid[row][column] < HIDDEN_MARK_LIMIT) {
                faceImage = g_CharacterFaceImages[(s16)faceSlot + MARKED_CHARACTER_OFFSET];
                facePalette = g_CharacterFacePalettes[(s16)faceSlot + MARKED_CHARACTER_OFFSET];
                gDPLoadTLUT_pal256(gfx++, TextureAddress(facePalette));
                gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
                gDPSetPrimColor(gfx++, 0, 0, 10, 10, 10, OPAQUE_ALPHA);
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress(faceImage),
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    FACE_FRAME_SIZE,
                    FACE_FRAME_SIZE,
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
                    QuarterPixels(column * FACE_SPACING + FACE_GRID_LEFT),
                    QuarterPixels(row * FACE_SPACING + FACE_GRID_TOP),
                    QuarterPixels(column * FACE_SPACING + FACE_GRID_LEFT + FACE_FRAME_SIZE),
                    QuarterPixels(row * FACE_SPACING + FACE_GRID_TOP + FACE_FRAME_SIZE),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                gDPPipeSync(gfx++);
                IN_OWN_BLOCK(;);
            }
            IN_OWN_BLOCK(column++);
        }
        IN_OWN_BLOCK(row++);
    }

    f_FaceFrameTime++;
    if (f_FaceFrameTime == FACE_FRAME_DURATION) {
        f_FaceFrameTime = 0;
        f_FaceFrame++;
        if (f_FaceFrame >= FACE_ANIMATION_FRAME_COUNT) {
            f_FaceFrame = 0;
        }

        f_UnreadFaceToggle ^= 1;
        gridMark = g_CharacterGrid[memory->characterRows[g_ChoosingPlayerIndex]]
                                  [memory->characterColumns[g_ChoosingPlayerIndex]];
        characterIndex = g_CharacterGrid[memory->characterRows[g_ChoosingPlayerIndex]]
                                        [memory->characterColumns[g_ChoosingPlayerIndex]];
        if (gridMark < CHOOSABLE_MARK_LIMIT) {
            characterIndex += MARKED_CHARACTER_OFFSET;
            faceSlot = characterIndex;
        }
        else if (gridMark >= FIRST_IMPORTED_CHARACTER) {
            faceSlot = characterIndex;
            characterIndex =
                g_ImportedCharacters[gridMark - FIRST_IMPORTED_CHARACTER].characterIndex;
        }
        else {
            faceSlot = characterIndex;
        }

        faceImage = g_CharacterFaceImages[(s16)faceSlot];
        facePalette = g_CharacterFacePalettes[(s16)faceSlot];
        if (f_PreviewedPalette == UP_C_PALETTE) {
            OpenAssetFile(g_CharacterFacePaletteFiles[(s16)characterIndex][UP_C_PALETTE], &file);
        }
        else if (f_PreviewedPalette == LEFT_C_PALETTE) {
            OpenAssetFile(g_CharacterFacePaletteFiles[(s16)characterIndex][LEFT_C_PALETTE], &file);
        }
        else if (f_PreviewedPalette == DOWN_C_PALETTE) {
            OpenAssetFile(g_CharacterFacePaletteFiles[(s16)characterIndex][DOWN_C_PALETTE], &file);
        }
        else if (f_PreviewedPalette == RIGHT_C_PALETTE) {
            OpenAssetFile(g_CharacterFacePaletteFiles[(s16)characterIndex][RIGHT_C_PALETTE], &file);
        }
        else {
            OpenAssetFile(
                g_CharacterFacePaletteFiles[(s16)characterIndex]
                                           [(s16)GetUnusedPalette((s16)characterIndex)],
                &file
            );
        }

        ReadAssetFile(facePalette, (s16)GetAssetFileSize(&file), &file);
        if (IsGameFlagSet(GAME_FLAG_ALTERNATE_SELECT_FACES)) {
            OpenEncodedAsset(
                g_AlternateFaceAnimationAssets[(s16)characterIndex][f_FaceFrame],
                &asset
            );
        }
        else {
            OpenEncodedAsset(g_FaceAnimationAssets[(s16)characterIndex][f_FaceFrame], &asset);
        }
        ReadEncodedAsset(faceImage, &asset);
        f_UnreadFaceColumn = memory->characterColumns[g_ChoosingPlayerIndex];
        f_UnreadFaceRow = memory->characterRows[g_ChoosingPlayerIndex];
    }

    *displayListHead = gfx;
}

s32 GetUnusedPalette(s32 choiceIndex) {
    s32 freePalette = 0;
    s32 playerIndex = 0;
    s32 sameCount = 0;
    CharacterIndex characterIndex = f_ChoiceCharacters[choiceIndex];
    s32 unused[4] UNUSED;
    s32 samePlayers[PLAYER_COUNT_MAX];
    s32 sameIndex;

    for (; playerIndex != g_ChoosingPlayerIndex; playerIndex++) {
        if (g_Players[playerIndex].characterIndex == characterIndex) {
            samePlayers[sameCount++] = playerIndex;
        }
    }

    if (sameCount != 0) {
        for (playerIndex = 0; playerIndex != CHARACTER_PALETTE_COUNT; playerIndex++) {
            for (sameIndex = 0; sameIndex != sameCount; sameIndex++) {
                if (g_Players[samePlayers[sameIndex]].paletteIndex == playerIndex) {
                    break;
                }
            }

            if (sameIndex == sameCount) {
                freePalette = playerIndex;
                break;
            }
        }

        return freePalette;
    }

    return 0;
}

void DrawCharacterBoard(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;
    ImageAsset *labelImage;

    GetMenuImageBuffer();
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_CharacterBoard)));
    for (stripIndex = 0; stripIndex != BOARD_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(
                ImageTexels(f_CharacterBoard) + (stripIndex * BOARD_WIDTH * BOARD_STRIP_HEIGHT / 2)
            ),
            G_IM_FMT_CI,
            BOARD_WIDTH,
            BOARD_STRIP_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        stripTop = BOARD_TOP + (stripIndex * BOARD_STRIP_HEIGHT);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(BOARD_LEFT),
            QuarterPixels(stripTop),
            QuarterPixels(BOARD_LEFT + BOARD_WIDTH),
            QuarterPixels(stripTop + BOARD_STRIP_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    labelImage = GetMenuImageBuffer();
    labelImage += CHARACTER_SELECT_FIRST_IMAGE_SIZE;
    LoadSmallPalette(&gfx, 0, ImagePalette(labelImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(labelImage),
        G_IM_FMT_CI,
        LABEL_IMAGE_WIDTH,
        LABEL_IMAGE_HEIGHT,
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
        QuarterPixels(LABEL_LEFT),
        QuarterPixels(LABEL_TOP),
        QuarterPixels(LABEL_LEFT + LABEL_SHOWN_WIDTH),
        QuarterPixels(LABEL_TOP + LABEL_IMAGE_HEIGHT),
        G_TX_RENDERTILE,
        TexelCoordinate(LABEL_SHOWN_WIDTH),
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    *displayListHead = gfx;
}

void DrawCharacterGridCursor(Gfx **displayListHead, s32 x, s32 y);

#define PLAYER_TAG_LINED_UP 0
#define PLAYER_TAG_SINGLE_PLAYER 2
#define PLAYER_TAG_FIRST_PLAYER 0
#define PLAYER_TAG_FIRST_COMPUTER 4
#define PLAYER_TAG_COMPUTER 8
#define PLAYER_TAG_IMAGE_SIZE 64
#define PlayerTagTexels(firstTag, playerIndex) \
    (TextureAddress(ImageTexelsAt( \
        f_PlayerTags, \
        ((playerIndex) * PLAYER_TAG_IMAGE_SIZE) + ((firstTag) * PLAYER_TAG_IMAGE_SIZE) \
    )))
#define PLAYER_TAG_WIDTH 16
#define PLAYER_TAG_HEIGHT 8
#define PLAYER_TAG_SHIFT_LEFT 4
#define PLAYER_TAG_TOP 56
#define PLAYER_TAG_RAISE (-14)
#define GRID_CELL_SIZE 40
#define GRID_LEFT 28
#define GRID_TOP 48
#define GRID_CURSOR_SHIFT_LEFT 8

void DrawPlayerTags(Gfx **displayListHead, u8 tagMode) {
    MenuMemory *memory = GetMenuMemory();
    s16 tagCount = g_ChoosingPlayerIndex + 1;
    Gfx *gfx = *displayListHead;
    s8 playerIndex;
    s16 left;
    s16 shiftX;
    s16 top;
    s16 cursorLeft;
    s16 cursorTop;
    s8 column;
    s8 row;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_PlayerTags)));
    if (g_MenuScreen == MENU_SCREEN_TOURNAMENT_CHARACTER_SELECT) {
        tagCount = 1;
    }

    for (playerIndex = 0; playerIndex != tagCount; playerIndex++) {
        if (tagMode != PLAYER_TAG_LINED_UP) {
            shiftX = 0;
            left = f_PlayerTagOffsets[playerIndex].x +
                (memory->characterColumns[playerIndex] * GRID_CELL_SIZE);
            top = f_PlayerTagOffsets[playerIndex].y +
                (memory->characterRows[playerIndex] * GRID_CELL_SIZE);
        }
        else {
            left = f_LinedUpTagLefts[tagCount - 1][playerIndex];
            top = ((playerIndex & 1) == 0) ? PLAYER_TAG_RAISE : 0;
            if ((tagCount == 2) & (playerIndex == 1)) {
                top = PLAYER_TAG_RAISE;
            }

            shiftX = 0;
        }

        if ((tagMode == PLAYER_TAG_SINGLE_PLAYER) & (playerIndex == 1)) {
            gDPLoadTextureBlock_4b(
                gfx++,
                PlayerTagTexels(PLAYER_TAG_COMPUTER, 0),
                G_IM_FMT_CI,
                PLAYER_TAG_WIDTH,
                PLAYER_TAG_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
        }
        else if (g_PlayerControllerIndices[playerIndex] == NO_CONTROLLER_INDEX) {
            gDPLoadTextureBlock_4b(
                gfx++,
                PlayerTagTexels(PLAYER_TAG_FIRST_COMPUTER, playerIndex),
                G_IM_FMT_CI,
                PLAYER_TAG_WIDTH,
                PLAYER_TAG_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
        }
        else if (f_IsComputerButtonHeld == TRUE && playerIndex == g_ChoosingPlayerIndex) {
            gDPLoadTextureBlock_4b(
                gfx++,
                PlayerTagTexels(PLAYER_TAG_FIRST_COMPUTER, playerIndex),
                G_IM_FMT_CI,
                PLAYER_TAG_WIDTH,
                PLAYER_TAG_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
        }
        else {
            gDPLoadTextureBlock_4b(
                gfx++,
                PlayerTagTexels(PLAYER_TAG_FIRST_PLAYER, playerIndex),
                G_IM_FMT_CI,
                PLAYER_TAG_WIDTH,
                PLAYER_TAG_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
        }

        left = shiftX + (s16)(left + GRID_LEFT);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(left - PLAYER_TAG_SHIFT_LEFT),
            QuarterPixels(top + PLAYER_TAG_TOP),
            QuarterPixels(left - PLAYER_TAG_SHIFT_LEFT + PLAYER_TAG_WIDTH),
            QuarterPixels(top + PLAYER_TAG_TOP + PLAYER_TAG_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    playerIndex = 0;
    if (g_MenuScreen != MENU_SCREEN_TOURNAMENT_CHARACTER_SELECT) {
        playerIndex = g_ChoosingPlayerIndex;
    }

    column = memory->characterColumns[playerIndex];
    row = memory->characterRows[playerIndex];
    cursorLeft = column * GRID_CELL_SIZE;
    cursorTop = (row * GRID_CELL_SIZE) + GRID_TOP;
    cursorLeft -= GRID_CURSOR_SHIFT_LEFT;
    DrawCharacterGridCursor(&gfx, cursorLeft + GRID_LEFT, cursorTop);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawCharacterGridCursor(Gfx **displayListHead, s32 x, s32 y) {
    Gfx *gfx = *displayListHead;
    s32 vertexIndex;
    s32 red;
    s32 blue;
    s32 green;
    f32 screenX;
    f32 screenY;

    for (vertexIndex = 0; vertexIndex != MENU_CURSOR_VERTEX_COUNT; vertexIndex++) {
        f_GridCursorPhases[vertexIndex] += MENU_CURSOR_PHASE_STEP;
        red = (s32)(sinf(f_GridCursorPhases[vertexIndex]) * MENU_CURSOR_COLOR_SWING) +
            MENU_CURSOR_COLOR_MIDDLE;
        blue = (s32)(sinf(f_GridCursorPhases[vertexIndex] + (M_PI / 2)) * MENU_CURSOR_COLOR_SWING) +
            MENU_CURSOR_COLOR_MIDDLE;
        green = (s32)(sinf(f_GridCursorPhases[vertexIndex] + M_PI) * MENU_CURSOR_COLOR_SWING) +
            MENU_CURSOR_COLOR_MIDDLE;
        f_GridCursorVertices[vertexIndex].v.cn[RED_CHANNEL] = red;
        f_GridCursorVertices[vertexIndex].v.cn[BLUE_CHANNEL] = blue;
        f_GridCursorVertices[vertexIndex].v.cn[GREEN_CHANNEL] = green;
    }

    guOrtho(
        &f_GridCursorProjection,
        -(SCREEN_WIDTH / 2),
        SCREEN_WIDTH / 2,
        -(SCREEN_HEIGHT / 2),
        SCREEN_HEIGHT / 2,
        MENU_CURSOR_NEAR,
        MENU_CURSOR_FAR,
        1.0f
    );
    screenX = (f32)(x - CHARACTER_CURSOR_ORIGIN_X) - HALF_PIXEL;
    screenY = (f32)(CHARACTER_CURSOR_ORIGIN_Y - y) + HALF_PIXEL;
    guTranslate(&f_GridCursorTranslations[g_DrawBufferIndex], screenX, screenY, MENU_CURSOR_Z);
    guRotate(&f_GridCursorRotations[g_DrawBufferIndex], 0.0f, 0.0f, 1.0f, 0.0f);
    guScale(&f_GridCursorScales[g_DrawBufferIndex], 1.0f, 1.0f, 1.0f);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_GridCursorBox)),
        G_IM_FMT_IA,
        MENU_CURSOR_SIZE,
        MENU_CURSOR_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    gDPSetCombineMode(gfx++, G_CC_SHADEDECALA, G_CC_SHADEDECALA);
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_GridCursorProjection),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_GridCursorTranslations[g_DrawBufferIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_GridCursorRotations[g_DrawBufferIndex]),
        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_GridCursorScales[g_DrawBufferIndex]),
        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
    );
    gDPPipeSync(gfx++);
    gSPVertex(gfx++, f_GridCursorVertices, MENU_CURSOR_VERTEX_COUNT, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawComputerIcon(Gfx **displayListHead, s32 unused UNUSED) {
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
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(g_ComputerLeftyIcons)));
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(g_ComputerLeftyIcons)),
        G_IM_FMT_CI,
        BUTTON_ICON_WIDTH,
        BUTTON_ICON_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gSPTextureRectangle(
        gfx++,
        QuarterPixels(COMPUTER_ICON_LEFT),
        QuarterPixels(BUTTON_ICON_TOP),
        QuarterPixels(COMPUTER_ICON_LEFT + BUTTON_ICON_WIDTH),
        QuarterPixels(BUTTON_ICON_TOP + BUTTON_ICON_HEIGHT),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawLeftyIcon(Gfx **displayListHead, s32 unused UNUSED) {
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
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(g_ComputerLeftyIcons)));
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(g_ComputerLeftyIcons) + LEFTY_ICON_OFFSET),
        G_IM_FMT_CI,
        BUTTON_ICON_WIDTH,
        BUTTON_ICON_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gSPTextureRectangle(
        gfx++,
        QuarterPixels(LEFTY_ICON_LEFT),
        QuarterPixels(BUTTON_ICON_TOP),
        QuarterPixels(LEFTY_ICON_LEFT + BUTTON_ICON_WIDTH),
        QuarterPixels(BUTTON_ICON_TOP + BUTTON_ICON_HEIGHT),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawCurveMeter(Gfx **displayListHead, s32 clubSet) {
    Gfx *gfx = *displayListHead;
    s32 curve = g_Clubs[clubSet * CLUB_SET_SIZE].curve;
    s32 firstCell;
    s32 cellIndex;
    s32 endCell;
    ImageAsset *labelImage;
    CurveLabel labelRow;

    if (curve < 0) {
        firstCell = CURVE_METER_CENTER + curve;
        endCell = CURVE_METER_CENTER + 1;
    }
    else if (curve > 0) {
        firstCell = CURVE_METER_CENTER;
        endCell = CURVE_METER_CENTER + 1 + curve;
    }
    else {
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
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_CurveMeterCells)));
    for (cellIndex = firstCell; cellIndex != endCell; cellIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(f_CurveMeterCells) +
                (cellIndex * CURVE_METER_CELL_SIZE * CURVE_METER_CELL_SIZE)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            CURVE_METER_CELL_SIZE,
            CURVE_METER_CELL_SIZE,
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
            QuarterPixels(CURVE_METER_LEFT + (cellIndex * CURVE_METER_CELL_SIZE)),
            QuarterPixels(CURVE_METER_TOP),
            QuarterPixels(
                CURVE_METER_LEFT + (cellIndex * CURVE_METER_CELL_SIZE) + CURVE_METER_CELL_SIZE
            ),
            QuarterPixels(CURVE_METER_TOP + CURVE_METER_CELL_SIZE),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    labelImage = GetMenuImageBuffer();
    LoadSmallPalette(&gfx, 0, ImagePalette(labelImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(labelImage),
        G_IM_FMT_CI,
        CURVE_LABEL_WIDTH,
        CURVE_LABEL_IMAGE_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    if (g_Clubs[clubSet * CLUB_SET_SIZE].curve > 0) {
        labelRow = CURVE_LABEL_DRAW;
    }
    else if (g_Clubs[clubSet * CLUB_SET_SIZE].curve < 0) {
        labelRow = CURVE_LABEL_FADE;
    }
    else {
        labelRow = CURVE_LABEL_STRAIGHT;
    }

    DrawTextureRectangle(
        &gfx,
        QuarterPixels(CURVE_LABEL_LEFT),
        QuarterPixels(CURVE_LABEL_TOP),
        QuarterPixels(CURVE_LABEL_LEFT + CURVE_LABEL_WIDTH),
        QuarterPixels(CURVE_LABEL_TOP + CURVE_LABEL_HEIGHT),
        G_TX_RENDERTILE,
        0,
        TexelCoordinate(labelRow * CURVE_LABEL_HEIGHT),
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    *displayListHead = gfx;
}
