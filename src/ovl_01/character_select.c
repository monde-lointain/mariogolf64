/*
 * The character select screen, which Get Character's opponent select and the
 * Club Tournament's character select draw as well. Each player moves a cursor
 * over the character grid, and the chosen character's shot curve shows as a
 * meter.
 */
#include "character_select.h"

#include <stdlib.h>
#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "camera.h"
#include "character.h"
#include "character_slot.h"
#include "club.h"
#include "compiler.h"
#include "controller.h"
#include "debug_text.h"
#include "game.h"
#include "game_flags.h"
#include "gbi_helpers.h"
#include "hole_display.h"
#include "include_asm.h"
#include "language.h"
#include "main_menu.h"
#include "menu.h"
#include "menu_memory.h"
#include "password.h"
#include "save.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"
#include "transfer_pak.h"

/* MoveGridCursor lets the cursor stop on slots below this only when asked to */
#define CHOOSABLE_MARK_LIMIT (-20)

/* The directions MoveGridCursor moves a player's cursor through the grid */
enum GridMove {
  GRID_MOVE_UP = 1,
  GRID_MOVE_DOWN,
  GRID_MOVE_LEFT,
  GRID_MOVE_RIGHT
};

/* The diagonals of any controller's stick that GetStickDiagonal reports, each
 * once per hold of four frames */
#define STICK_DIAGONAL_NONE 0
#define STICK_DIAGONAL_UP_LEFT 1
#define STICK_DIAGONAL_UP_RIGHT 2
#define STICK_DIAGONAL_DOWN_LEFT 3
#define STICK_DIAGONAL_DOWN_RIGHT 4
#define STICK_DIAGONAL_THRESHOLD 20
#define STICK_HOLD_FRAME_COUNT 4

/* The bytes of the first of the two images that LoadCharacterFaces reads
 * into the menu's buffer */
#define CHARACTER_SELECT_FIRST_IMAGE_SIZE 0x628
/* The trajectory labels Straight, Draw and Fade, and the labels L: and Drive:
 */
#define TRAJECTORY_LABELS_IMAGE_ASSET 0x979
#define STAT_LABELS_IMAGE_ASSET 0x978

/* DrawCharacterBoard draws f_CharacterBoard in strips, then the right half of
 * the second image */
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
 * DrawCurveMeter shows a character's shot curve as a meter of the 8x8
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
/* g_ComLeftyIcons, asset 0x0709, holds the "R: COM" icon above "L: LEFTY" */
#define BUTTON_ICON_WIDTH 32
#define BUTTON_ICON_HEIGHT 24
#define BUTTON_ICON_TOP 20
#define COMPUTER_ICON_LEFT 270
#define LEFTY_ICON_LEFT 18
#define LEFTY_ICON_OFFSET (BUTTON_ICON_WIDTH * BUTTON_ICON_HEIGHT / 2)

/* What DrawCharacterSelect writes below the board */
#define MESSAGE_CHOOSE_PLAYER_CHARACTER 0x12
#define MESSAGE_CHOOSE_CHARACTER 0x14
#define MESSAGE_FIRST_CHARACTER_NAME 0xA9
#define PROMPT_X 25
#define PROMPT_Y 180
#define CHARACTER_NAME_X 238
#define CHARACTER_NAME_Y 156
#define DRIVE_DISTANCE_X 246
#define DRIVE_DISTANCE_Y 200

/* The buffer kinds that f_SelectBufferAssets has an image for, from 0 */
#define CHARACTER_SELECT_IMAGE_KIND_COUNT \
  (CHARACTER_SELECT_BUFFER_CURVE_CELLS + 1)

/* The palettes that the C buttons choose, which the A button wears and which
 * the faces preview while held */
enum CButtonPalette {
  NO_PALETTE = -1,
  UP_C_PALETTE,
  LEFT_C_PALETTE,
  DOWN_C_PALETTE,
  RIGHT_C_PALETTE
};

/* Where the character cursor's translation is measured from, so that it is
 * placed by its corner */
#define CHARACTER_CURSOR_ORIGIN_X 140
#define CHARACTER_CURSOR_ORIGIN_Y 96
/* Moves the cursor by half a pixel so that its edges sit on pixel centers */
#define HALF_PIXEL 0.5f

/* A player tag's offset from the corner of its cursor's grid cell */
typedef struct TagOffset {
  /* 0x0 */ s8 x;
  /* 0x1 */ s8 y;
} TagOffset; /* size = 0x2 */

void DrawGridFaces(Gfx** displayListHead, u8 isSinglePlayer);
void DrawCharacterBoard(Gfx** displayListHead);
void DrawPlayerTags(Gfx** displayListHead, u8 tagMode);
void DrawComIcon(Gfx** displayListHead, s32 unused);
void DrawLeftyIcon(Gfx** displayListHead, s32 unused);
void DrawCurveMeter(Gfx** displayListHead, s32 clubSet);

/* Whether DrawCharacterSelect has set the choosing player's prompt */
static u8 f_IsPromptShown = FALSE;
/* LoadCharacterSelectBuffer clears it, but no routine reads it */
static u8 f_UnreadBufferFlag = FALSE;
/* Read by no routine */
static u8 f_UnreadSelectByte UNUSED = 0;
/* Cleared once every player has chosen, but read by no routine */
static u8 f_UnreadSelectDoneByte = 0;
/* The animation frame of the face under the choosing player's cursor */
static s8 f_FaceFrame = 0;
/* Flipped whenever the grid face changes, but read by no routine */
static u8 f_UnreadFaceToggle = 0;
/* The draws that f_FaceFrame has been shown for */
static s8 f_FaceFrameTimer = 0;
/* Whether R, or Z or L, is held, which shows the COM or LEFTY icon */
static s8 f_IsComButtonHeld = FALSE;
static s8 f_IsLeftyButtonHeld = FALSE;
/* The CButtonPalette that the grid face previews while a C button is held */
static s8 f_PreviewedPalette = NO_PALETTE;
/* Read by no routine */
static s32 f_UnreadSelectWords[4] UNUSED = {-1, -1, -1, -1};
/* The Game Boy row's slots, which HideGameBoyCharacters saves here and
 * UpdateCharacterSelect puts back */
static s32 f_SavedGameBoySlots[GAME_BOY_CHARACTER_COUNT] = {
    EMPTY_GRID_SLOT, EMPTY_GRID_SLOT, EMPTY_GRID_SLOT, EMPTY_GRID_SLOT};
/* The AssetId that LoadCharacterSelectBuffer reads for each buffer kind */
static s32 f_SelectBufferAssets[CHARACTER_SELECT_IMAGE_KIND_COUNT] = {
    0x08E8, 0x05E3, 0x08EF, 0x050F, 0x0521, 0x06EF, 0x05FF, 0x0600, 0x08EE};
/* The CharacterId of each CharacterChoice */
static CharacterId f_ChoiceCharacters[CHARACTER_COUNT] = {CHARACTER_PLUM,
    CHARACTER_CHARLIE, CHARACTER_PEACH, CHARACTER_BABY_MARIO, CHARACTER_LUIGI,
    CHARACTER_YOSHI, CHARACTER_SONNY, CHARACTER_WARIO, CHARACTER_HARRY,
    CHARACTER_MARIO, CHARACTER_MAPLE, CHARACTER_DONKEY_KONG, CHARACTER_BOWSER,
    CHARACTER_METAL_MARIO, CHARACTER_KID, CHARACTER_SHERRY, CHARACTER_AZALEA,
    CHARACTER_JOE};
/* Read by no routine */
static u8 f_UnreadSelectBytes[8] UNUSED = {
    0x71, 0x76, 0x81, 0x6C, 0x9D, 0x00, 0x00, 0x00};
/* GetStickDiagonal's count of unchanged readings on each axis */
static s32 f_StickXHoldTimer = 0;
static s32 f_StickYHoldTimer = 0;
/* Read by no routine */
static char f_UnreadCurveNames[3][10] UNUSED = {"Straight", "Draw", "Fade"};
/* The cell of the face DrawGridFaces last read, read by no routine */
static s16 f_UnreadFaceColumn = 0;
static s16 f_UnreadFaceRow = 0;
/* Read by no routine */
static s16 f_UnreadSelectHalfword UNUSED = 0;
/* Each player's tag offset from their cursor's grid cell, as x and y */
static TagOffset f_PlayerTagOffsets[MAX_PLAYER_COUNT] = {
    {0, 0}, {16, 0}, {0, 24}, {16, 24}};
/* The tags' x when lined up, by the player count less one */
static s32 f_LinedUpTagLefts[MAX_PLAYER_COUNT][MAX_PLAYER_COUNT] = {
    {102, -26, -26, -26}, {22, 182, -26, -26}, {38, 102, 166, 230},
    {6, 70, 134, 198}};
/*
 * Read by no routine: an order of 0 to 15, the last 16 values of
 * player_stats.c's f_UnusedCharacterOrder
 */
static s16 f_UnreadSelectOrder[16] UNUSED = {
    1, 4, 5, 6, 3, 2, 7, 9, 12, 10, 13, 15, 14, 11, 8, 0};
/* The grid cursor's corners, whose colors DrawCharacterGridCursor cycles */
static Vtx f_GridCursorVertices[MENU_CURSOR_VERTEX_COUNT] = {
    {{{-24, 24, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-24, -24, 0}, 0, {0, 3072}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{24, -24, 0}, 0, {3072, 3072}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{24, 24, 0}, 0, {3072, 0}, {0xFF, 0xFF, 0x00, 0xFF}}}};
/* Read by no routine */
static s32 f_UnreadSelectPair[2] UNUSED = {0, 0};
/* The angle, in radians, of each corner's color cycle */
static f32 f_GridCursorPhases[MENU_CURSOR_VERTEX_COUNT] = {
    0.0f, 2.0f, 4.0f, 6.0f};
/* Read by no routine */
static s32 f_UnreadSelectQuad[4] UNUSED = {0, 0, 0, 0};

/* The camera and placements that DrawCharacterGridCursor draws with */
static Mtx f_GridCursorProjection;
static Mtx f_GridCursorTranslations[DRAW_BUFFER_COUNT];
static Mtx f_GridCursorRotations[DRAW_BUFFER_COUNT];
static Mtx f_GridCursorScales[DRAW_BUFFER_COUNT];

/* Read by no routine */
static u8 f_UnreadSelectPad[0x8] UNUSED;

static void* f_CharacterBoard;

/* Read by no routine */
static u8 f_UnreadSelectGap[0x4] UNUSED;

/*
 * Image buffers that LoadCharacterSelectBuffer keeps, as are f_CharacterBoard
 * and f_CurveMeterCells. No routine reads the f_Unread ones, and no caller
 * passes the kinds of the controller icons or the color bars.
 */
static void* f_PlayerTags;
static void* f_GridCursorBox;
static void* f_UnreadNamePlate;
static void* f_UnreadControllerIcons;
static void* f_UnreadColorBars;
static void* f_CurveMeterCells;

/* Read by no routine */
static u8 f_UnreadSelectBlock[0x208] UNUSED;

/* Not static: the original left these to the linker's common block */
s32 f_PreviousStickX;
s32 f_PreviousStickY;

/*
 * Empties the Game Boy row of the character grid for the opponent select,
 * saving its slots to put back afterward
 */
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

/*
 * Moves the player's grid cursor a step in the GRID_MOVE_* direction, wrapping
 * at the edges and skipping slots that can't be chosen, and reads the face of
 * the character it leaves. Returns -1 if it couldn't move, otherwise 0.
 */
s32 MoveGridCursor(s32 playerNumber, u8 direction, u8 canChooseMarked) {
  EncodedAsset asset;
  AssetFile file;
  MenuMemory* memory = GetMenuMemory();
  s16 oldColumn = memory->characterColumns[playerNumber];
  s16 oldRow = memory->characterRows[playerNumber];
  s16 column = oldColumn;
  s16 row = oldRow;
  u16 slot;
  u16 character;
  void* faceImage;
  /* Pads the frame to match; without it 36 words differ */
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
  } while (g_CharacterGrid[row][column] < 0 &&
      (g_CharacterGrid[row][column] < CHOOSABLE_MARK_LIMIT &&
          canChooseMarked == TRUE) == FALSE);
  memory->characterColumns[playerNumber] = column;
  memory->characterRows[playerNumber] = row;
  if (column != oldColumn || row != oldRow) {
    character = g_CharacterGrid[oldRow][oldColumn];
    f_FaceFrame = 0;
    f_FaceFrameTimer = 0;
    f_UnreadFaceToggle ^= 1;
    if (g_CharacterGrid[oldRow][oldColumn] < CHOOSABLE_MARK_LIMIT) {
      character += MARKED_CHARACTER_OFFSET;
    }

    if ((s16)character >= FIRST_IMPORTED_CHARACTER) {
      /*
       * Set in each branch to match: setting slot before the test gives 48
       * words differing, 6 in shape
       */
      slot = character;
      character =
          g_ImportedCharacters[(s16)slot - FIRST_IMPORTED_CHARACTER].character;
    } else {
      slot = character;
    }

    faceImage = g_CharacterFaceImages[(s16)slot];
    if (IsGameFlagSet(GAME_FLAG_ALTERNATE_SELECT_FACES)) {
      OpenEncodedAsset(
          g_AlternateFaceAnimationAssets[(s16)character][f_FaceFrame], &asset);
    } else {
      OpenEncodedAsset(
          g_FaceAnimationAssets[(s16)character][f_FaceFrame], &asset);
    }

    ReadEncodedAsset(faceImage, &asset);
    OpenAssetFile(g_CharacterFacePaletteFiles[(s16)character][0], &file);
    ReadAssetFile(g_CharacterFacePalettes[(s16)slot],
        (s16)GetAssetFileSize(&file), &file);
  }

  return -(column == oldColumn && row == oldRow);
}

/*
 * Moves the player's grid cursor a step in the STICK_DIAGONAL_*
 * direction, then sideways only if that slot can't be chosen, as
 * MoveGridCursor does. Returns -1 if it couldn't move, otherwise 0.
 */
s32 MoveGridCursorDiagonal(s32 playerNumber, u8 diagonal, u8 canChooseMarked) {
  EncodedAsset asset;
  AssetFile file;
  MenuMemory* memory = GetMenuMemory();
  s32 retryCount = 0;
  s16 oldRow = memory->characterRows[playerNumber];
  s16 oldColumn = memory->characterColumns[playerNumber];
  s32 column = oldColumn;
  s32 row = oldRow;
  u16 slot;
  u16 character;
  void* faceImage;
  /* Pads the frame to match; without it 36 words differ */
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
    } else {
      column -= diagonal == STICK_DIAGONAL_UP_LEFT ||
          diagonal == STICK_DIAGONAL_DOWN_LEFT;
      column += diagonal == STICK_DIAGONAL_UP_RIGHT ||
          diagonal == STICK_DIAGONAL_DOWN_RIGHT;
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
        (g_CharacterGrid[row][column] < CHOOSABLE_MARK_LIMIT &&
            canChooseMarked == TRUE)) {
      break;
    }

    /* After a blocked diagonal, try moving sideways only from where the cursor
     * started */
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
    f_FaceFrame = 0;
    f_FaceFrameTimer = 0;
    f_UnreadFaceToggle ^= 1;
    if (g_CharacterGrid[oldRow][oldColumn] < CHOOSABLE_MARK_LIMIT) {
      character += MARKED_CHARACTER_OFFSET;
    }

    if ((s16)character >= FIRST_IMPORTED_CHARACTER) {
      /*
       * Set in each branch to match: setting slot before the test gives 42
       * words differing, 6 in shape
       */
      slot = character;
      character =
          g_ImportedCharacters[(s16)slot - FIRST_IMPORTED_CHARACTER].character;
    } else {
      slot = character;
    }

    faceImage = g_CharacterFaceImages[(s16)slot];
    if (IsGameFlagSet(GAME_FLAG_ALTERNATE_SELECT_FACES)) {
      OpenEncodedAsset(
          g_AlternateFaceAnimationAssets[(s16)character][f_FaceFrame], &asset);
    } else {
      OpenEncodedAsset(
          g_FaceAnimationAssets[(s16)character][f_FaceFrame], &asset);
    }

    ReadEncodedAsset(faceImage, &asset);
    OpenAssetFile(g_CharacterFacePaletteFiles[(s16)character][0], &file);
    ReadAssetFile(
        g_CharacterFacePalettes[(s16)slot], GetAssetFileSize(&file), &file);
  }

  if (column == oldColumn && row == oldRow) {
    return -1;
  }

  return 0;
}

/*
 * Returns the STICK_DIAGONAL_* direction of the controllers' sticks combined,
 * counting only the bits of each axis that its previous reading lacked, so
 * that a held stick isn't reported every frame.
 */
s32 GetStickDiagonal(void) {
  s32 stickX;
  s32 stickY;
  s8 anyStick;

  GetMenuMemory();
  anyStick = g_ControllerStates[3].stickX |
      (g_ControllerStates[2].stickX |
          (g_ControllerStates[0].stickX | g_ControllerStates[1].stickX));
  /*
   * The newly pushed directions, in two statements to match: in one, either
   * as (previous ^ any) & any or as any & ~previous, 20 words differ (none
   * and 2 in shape), and as any & ~previous for both axes 44 (4)
   */
  stickX = f_PreviousStickX ^ anyStick;
  stickX &= anyStick;
  if (f_PreviousStickX == stickX) {
    f_StickXHoldTimer++;
    if (f_StickXHoldTimer >= STICK_HOLD_FRAME_COUNT) {
      f_StickXHoldTimer = 0;
      f_PreviousStickX = 0;
    }
  } else {
    f_PreviousStickX = stickX;
  }

  anyStick = g_ControllerStates[3].stickY |
      (g_ControllerStates[2].stickY |
          (g_ControllerStates[0].stickY | g_ControllerStates[1].stickY));
  /*
   * In two statements to match, as for x: in one, 24 words differ, none in
   * shape
   */
  stickY = f_PreviousStickY ^ anyStick;
  stickY &= anyStick;
  if (f_PreviousStickY == stickY) {
    f_StickYHoldTimer++;
    if (f_StickYHoldTimer >= STICK_HOLD_FRAME_COUNT) {
      f_StickYHoldTimer = 0;
      f_PreviousStickY = 0;
    }
  } else {
    f_PreviousStickY = stickY;
  }

  if (stickX < -STICK_DIAGONAL_THRESHOLD && stickY > STICK_DIAGONAL_THRESHOLD) {
    return STICK_DIAGONAL_UP_LEFT;
  }

  if (stickX > STICK_DIAGONAL_THRESHOLD && stickY > STICK_DIAGONAL_THRESHOLD) {
    return STICK_DIAGONAL_UP_RIGHT;
  }

  if (stickX < -STICK_DIAGONAL_THRESHOLD &&
      stickY < -STICK_DIAGONAL_THRESHOLD) {
    return STICK_DIAGONAL_DOWN_LEFT;
  }

  if (stickX > STICK_DIAGONAL_THRESHOLD && stickY < -STICK_DIAGONAL_THRESHOLD) {
    return STICK_DIAGONAL_DOWN_RIGHT;
  }

  return STICK_DIAGONAL_NONE;
}

/* The text slots that the character select writes, the prompt's first */
#define CHARACTER_SELECT_TEXT_SLOT_COUNT 4
#define PROMPT_TEXT_SLOT 0

#define Z_OR_L_BUTTONS (Z_TRIG | L_TRIG)

/* Where pressing up blanks a line of debug text */
#define BLANKED_DEBUG_TEXT_COLUMN 15
#define BLANKED_DEBUG_TEXT_ROW 10

/* R leaves a player without a controller only among this many players */
#define MIN_PLAYER_COUNT_WITHOUT_CONTROLLER 2

/*
 * Runs the character select for the players from firstPlayerIndex up to
 * playerCount, one at a time: the D-Pad moves the current player's cursor, A
 * chooses the character with the palette of a held C button, and B goes back
 * a player or leaves the screen. Holding R as A is pressed leaves the player
 * without a controller. Holding a C button previews its palette in
 * f_PreviewedPalette, and holding R, or Z or L, sets f_IsComButtonHeld or
 * f_IsLeftyButtonHeld. Once a club tournament's choice is made, the closing
 * wipe ends the menus and g_GameState.roundSeed, which RankTournament takes,
 * gets the tournament code.
 */

/*
 * Doesn't match: the original sets up the chosen character's sound with the
 * volume first, filling the random test's delay slot with it, where this
 * fills it with the character. The register oracle's closest result is these
 * 10 words with no register forced, so allocation isn't the cause. Testing
 * for the other sound first gives 10 words, 2 in shape; the test written !=
 * 0 or as % 2 changes nothing, and choosing the sound with ?: in one call
 * gives 27 (19 in shape). 10 words differ, 4 in shape.
 *
 * The grid marks of a player whose choice is undone are read into character,
 * as the original keeps them in character's register; in gridMark, 136 words
 * differ, and for either of the two undos alone 16. It reads canChooseMarked
 * as a byte throughout, hence the casts; declared u8 instead, 22 words
 * differ, 12 in shape. Clearing the same-character count before the preview
 * loops, not in their initializers, gives 26. Declaring the button as an s16
 * gives 63 words, 7 in shape, and as a u32 or s32 28; the original loads it
 * with lhu, so it is unsigned. Reading an imported character's character
 * after marking it chosen gives 18 words, 10 in shape, and the target loads
 * it before the stores.
 *
 * Permuter runs on older bases found no natural form: one best returned from
 * the diagonal move only when it failed, so a successful move would go on to
 * the other buttons' tests, and another copied the button through an
 * unsigned long long temporary.
 */
#ifdef NON_MATCHING
s32 UpdateCharacterSelect(
    s32 firstPlayerIndex, s32 playerCount, Boolean canChooseMarked) {
  EncodedAsset asset;
  AssetFile file;
  s32 sameCharacterPlayers[MAX_PLAYER_COUNT];
  MenuMemory* memory = GetMenuMemory();
  s32 playerIndex;
  s32 index;
  s32 otherIndex;
  s16 sameCharacterCount;
  /* The cursor's grid mark, then the character it is; also an undone choice's
   * grid mark */
  s32 character;
  s16 gridMark;
  s32 controllerIndex;
  u16 button;
  s32 palette;
  /*
   * The cursor's column, a u16 like the row, as the original sign-extends
   * it to 16 bits at each use: with an s8 column, also used for the chosen
   * character, 46 words differ, but the register oracle's best is 24 words
   * against 10 with this one
   */
  u16 column;
  s8 chosenColumn;
  u16 row;
  u16 slot;
  u16 faceCharacter;
  void* faceImage;
  SoundChannel channel;

  if (IsHudAnimationDone() != TRUE) {
    return UNSET_RESULT;
  }

  if (GetWipeKind() != WIPE_NONE) {
    for (index = 0; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
      SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, index, 0, 255, 255,
          255, OPAQUE_ALPHA);
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
    if (MoveGridCursorDiagonal(
            playerIndex, STICK_DIAGONAL_UP_LEFT, (u8)canChooseMarked) == 0) {
      for (index = 1; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, index, 0, 255, 255,
            255, OPAQUE_ALPHA);
      }
    }

    return 0;
  }

  if (g_MenuRepeatButtons == (U_JPAD | R_JPAD)) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    if (MoveGridCursorDiagonal(
            playerIndex, STICK_DIAGONAL_UP_RIGHT, (u8)canChooseMarked) == 0) {
      for (index = 1; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, index, 0, 255, 255,
            255, OPAQUE_ALPHA);
      }
    }

    return 0;
  }

  if (g_MenuRepeatButtons == (D_JPAD | L_JPAD)) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    if (MoveGridCursorDiagonal(
            playerIndex, STICK_DIAGONAL_DOWN_LEFT, (u8)canChooseMarked) == 0) {
      for (index = 1; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, index, 0, 255, 255,
            255, OPAQUE_ALPHA);
      }
    }

    return 0;
  }

  if (g_MenuRepeatButtons == (D_JPAD | R_JPAD)) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    if (MoveGridCursorDiagonal(
            playerIndex, STICK_DIAGONAL_DOWN_RIGHT, (u8)canChooseMarked) == 0) {
      for (index = 1; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, index, 0, 255, 255,
            255, OPAQUE_ALPHA);
      }
    }

    return 0;
  }

  if (g_MenuRepeatButtons & U_JPAD) {
    PrintDebugText(
        "          ", BLANKED_DEBUG_TEXT_COLUMN, BLANKED_DEBUG_TEXT_ROW);
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    if (MoveGridCursor(playerIndex, GRID_MOVE_UP, (u8)canChooseMarked) == 0) {
      for (index = 1; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, index, 0, 255, 255,
            255, OPAQUE_ALPHA);
      }
    }

    return 0;
  }

  if (g_MenuRepeatButtons & D_JPAD) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    if (MoveGridCursor(playerIndex, GRID_MOVE_DOWN, (u8)canChooseMarked) == 0) {
      for (index = 1; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, index, 0, 255, 255,
            255, OPAQUE_ALPHA);
      }
    }

    return 0;
  }

  if (g_MenuRepeatButtons & L_JPAD) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    if (MoveGridCursor(playerIndex, GRID_MOVE_LEFT, (u8)canChooseMarked) == 0) {
      for (index = 1; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, index, 0, 255, 255,
            255, OPAQUE_ALPHA);
      }
    }

    return 0;
  }

  if (g_MenuRepeatButtons & R_JPAD) {
    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    if (MoveGridCursor(playerIndex, GRID_MOVE_RIGHT, (u8)canChooseMarked) ==
        0) {
      for (index = 1; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, index, 0, 255, 255,
            255, OPAQUE_ALPHA);
      }
    }

    return 0;
  }

  if (g_MenuPressedButtons & A_BUTTON) {
    for (index = 0; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
      SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, index, 0, 255, 255,
          255, OPAQUE_ALPHA);
    }

    character = g_CharacterGrid[memory->characterRows[playerIndex]]
                               [memory->characterColumns[playerIndex]];
    if (((u8)canChooseMarked == TRUE) & (character < CHOOSABLE_MARK_LIMIT)) {
      character += MARKED_CHARACTER_OFFSET;
    }

    character = f_ChoiceCharacters[character];
    if (character >= FIRST_IMPORTED_CHARACTER) {
      /* Only one player may choose each imported character */
      if (g_ImportedCharacters[character - FIRST_IMPORTED_CHARACTER].isChosen ==
          TRUE) {
        PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        f_IsPromptShown = FALSE;
        return UNSET_RESULT;
      }

      index = character - FIRST_IMPORTED_CHARACTER;
      character = g_ImportedCharacters[index].character;
      g_ImportedCharacters[index].isChosen = TRUE;
      g_GameState.isImportedCharacterChosen = TRUE;
      g_ImportedCharacterColumns[playerIndex] =
          memory->characterColumns[playerIndex];
      if (g_ImportedCharacterColumns[playerIndex] < 0) {
        g_ImportedCharacterColumns[playerIndex] = 0;
      }

      if (g_ImportedCharacterColumns[playerIndex] >= IMPORTED_CHARACTER_COUNT) {
        g_ImportedCharacterColumns[playerIndex] = IMPORTED_CHARACTER_COUNT - 1;
      }
    }

    /* The first controller to have pressed A takes the player, unless R
     * leaves the player without one; the last player only when another player
     * has one */
    for (controllerIndex = 0; controllerIndex != g_ControllerCount;
        controllerIndex++) {
      button = g_MenuControllerData[controllerIndex].button;
      if (button & A_BUTTON) {
        if ((u8)canChooseMarked == FALSE) {
          if (((button & R_TRIG) != 0) &
                  (playerCount >= MIN_PLAYER_COUNT_WITHOUT_CONTROLLER) &&
              character < FIRST_IMPORTED_CHARACTER) {
            if (g_ChosenPlayerCount - 1 == playerIndex &&
                !IsGameFlagSet(GAME_FLAG_ALLOW_ALL_COMPUTER_PLAYERS)) {
              for (otherIndex = 0; otherIndex != playerIndex; otherIndex++) {
                if (g_PlayerControllerIndices[otherIndex] !=
                    NO_CONTROLLER_INDEX) {
                  break;
                }
              }

              if (otherIndex == playerIndex) {
                g_PlayerControllerIndices[playerIndex] = controllerIndex;
              } else {
                g_PlayerControllerIndices[playerIndex] = NO_CONTROLLER_INDEX;
              }
            } else {
              g_PlayerControllerIndices[playerIndex] = NO_CONTROLLER_INDEX;
            }
          } else {
            g_PlayerControllerIndices[playerIndex] = controllerIndex;
          }
        } else {
          g_PlayerControllerIndices[playerIndex] = NO_CONTROLLER_INDEX;
        }

        g_ChoosingControllerIndices[playerIndex] = controllerIndex;
        SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, PROMPT_TEXT_SLOT,
            TEXT_FLAG_TYPED, 255, 255, 255, OPAQUE_ALPHA);
        f_IsPromptShown = FALSE;
        break;
      }
    }

    g_ChosenCharacters[playerIndex] = character;
    g_Players[playerIndex].characterIndex = character;
    if (g_MenuRepeatButtons & Z_OR_L_BUTTONS) {
      if (character < FIRST_IMPORTED_CHARACTER) {
        g_ChosenLefties[playerIndex] = TRUE;
      }
    }

    if (character >= FIRST_IMPORTED_CHARACTER) {
      g_ChosenLefties[playerIndex] =
          g_GameBoyCharacters[memory->characterColumns[playerIndex]].isLefty;
    }

    if (g_MenuRepeatButtons & U_CBUTTONS) {
      palette = UP_C_PALETTE;
    } else if (g_MenuRepeatButtons & D_CBUTTONS) {
      palette = DOWN_C_PALETTE;
    } else if (g_MenuRepeatButtons & L_CBUTTONS) {
      palette = LEFT_C_PALETTE;
    } else if (g_MenuRepeatButtons & R_CBUTTONS) {
      palette = RIGHT_C_PALETTE;
    } else {
      palette = UP_C_PALETTE;
    }

    /* A palette that an earlier player of the same character wears gives way
     * to the first one that none of them does */
    for (index = 0, sameCharacterCount = 0; index != playerIndex; index++) {
      if (g_Players[index].characterIndex ==
          g_Players[playerIndex].characterIndex) {
        sameCharacterPlayers[sameCharacterCount] = index;
        sameCharacterCount++;
      }
    }

    for (index = 0; index != sameCharacterCount; index++) {
      if (g_Players[sameCharacterPlayers[index]].paletteIndex == palette) {
        break;
      }
    }

    if (index == sameCharacterCount) {
      g_Players[playerIndex].paletteIndex = palette;
    } else {
      for (index = 0; index != CHARACTER_PALETTE_COUNT; index++) {
        for (otherIndex = 0; otherIndex != sameCharacterCount; otherIndex++) {
          if (g_Players[sameCharacterPlayers[otherIndex]].paletteIndex ==
              index) {
            break;
          }
        }

        if (otherIndex == sameCharacterCount) {
          g_Players[playerIndex].paletteIndex = index;
          break;
        }
      }

      if (index == CHARACTER_PALETTE_COUNT) {
        g_Players[playerIndex].paletteIndex = 0;
      }
    }

    /* Put back the first frame of the chosen face */
    column = memory->characterColumns[playerIndex];
    row = memory->characterRows[playerIndex];
    playerIndex++;
    f_FaceFrame = 0;
    f_FaceFrameTimer = 0;
    f_UnreadFaceToggle ^= 1;
    if (playerIndex != playerCount) {
      gridMark = g_CharacterGrid[(s16)row][(s16)column];
      faceCharacter = g_CharacterGrid[(s16)row][(s16)column];
      if (gridMark < CHOOSABLE_MARK_LIMIT) {
        slot = faceCharacter + MARKED_CHARACTER_OFFSET;
        faceCharacter = slot;
      } else if (gridMark >= FIRST_IMPORTED_CHARACTER) {
        /*
         * Set in each branch to match: setting slot before the tests, here
         * and in the other face load, gives 94 words differing (20 in shape)
         * against the draft's 10 (4)
         */
        slot = faceCharacter;
        faceCharacter =
            g_ImportedCharacters[gridMark - FIRST_IMPORTED_CHARACTER].character;
      } else {
        slot = faceCharacter;
      }

      faceImage = g_CharacterFaceImages[(s16)slot];
      if (IsGameFlagSet(GAME_FLAG_ALTERNATE_SELECT_FACES)) {
        OpenEncodedAsset(
            g_AlternateFaceAnimationAssets[(s16)faceCharacter][f_FaceFrame],
            &asset);
      } else {
        OpenEncodedAsset(
            g_FaceAnimationAssets[(s16)faceCharacter][f_FaceFrame], &asset);
      }

      ReadEncodedAsset(faceImage, &asset);
      OpenAssetFile(g_CharacterFacePaletteFiles[(s16)faceCharacter][0], &file);
      ReadAssetFile(
          g_CharacterFacePalettes[(s16)slot], GetAssetFileSize(&file), &file);
    }

    if (playerIndex == playerCount) {
      if ((u8)canChooseMarked == FALSE) {
        if (g_MenuScreen == MENU_SCREEN_TOURNAMENT_CHARACTER_SELECT) {
          LoadWipeShape(GetClubTournamentCourse());
          StartWipe(WIPE_CLOSING);
        } else {
          StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
          g_MenuScreen = MENU_SCREEN_MODE_SELECT;
        }
      } else {
        StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
        g_MenuScreen = MENU_SCREEN_COURSE_SELECT;
        g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
        COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_PLAIN;
        g_BetSet = BET_SET_NONE;
      }

      SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, PROMPT_TEXT_SLOT,
          TEXT_FLAG_TYPED, 255, 255, 255, OPAQUE_ALPHA);
      f_IsPromptShown = FALSE;
      FreeAllCharacterSlots();
      f_UnreadSelectDoneByte = 0;
      f_FaceFrame = 0;
      f_UnreadFaceToggle = 0;
      f_FaceFrameTimer = 0;
    }

    f_PreviewedPalette = NO_PALETTE;
    g_ChoosingPlayerIndex = playerIndex;
    f_IsComButtonHeld = FALSE;
    f_IsLeftyButtonHeld = FALSE;
    if (playerIndex == 1) {
      channel = FIRST_CHARACTER_SOUND_CHANNEL;
    } else if (playerIndex == 2) {
      channel = FIRST_CHARACTER_SOUND_CHANNEL + 1;
    } else if (playerIndex == 3) {
      channel = FIRST_CHARACTER_SOUND_CHANNEL + 2;
    } else if (playerIndex == 4) {
      channel = FIRST_CHARACTER_SOUND_CHANNEL + 3;
    } else {
      channel = MENU_SOUND_CHANNEL;
    }

    if (rand() & 1) {
      PlayCharacterSound(character, channel - FIRST_CHARACTER_SOUND_CHANNEL,
          CHARACTER_SOUND_CHOSEN, SOUND_FULL_SCALE, SOUND_FULL_SCALE);
    } else {
      PlayCharacterSound(character, channel - FIRST_CHARACTER_SOUND_CHANNEL,
          CHARACTER_SOUND_OTHER_CHOSEN, SOUND_FULL_SCALE, SOUND_FULL_SCALE);
    }

    return 0;
  }

  if (g_MenuPressedButtons & B_BUTTON) {
    for (index = 0; index != CHARACTER_SELECT_TEXT_SLOT_COUNT; index++) {
      SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, index, 0, 255, 255,
          255, OPAQUE_ALPHA);
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
        } else {
          g_MenuScreen = MENU_SCREEN_TITLE;
        }
      } else {
        g_MenuScreen = MENU_SCREEN_MODE_SELECT;
      }

      g_ChoosingControllerIndices[playerIndex] = UNCHOSEN_CONTROLLER;
      g_PlayerControllerIndices[playerIndex] = UNCHOSEN_CONTROLLER;
      character = g_CharacterGrid[memory->characterRows[playerIndex]]
                                 [memory->characterColumns[playerIndex]];
      if (character >= FIRST_IMPORTED_CHARACTER) {
        g_ImportedCharacters[character - FIRST_IMPORTED_CHARACTER].isChosen =
            FALSE;
        g_ImportedCharacterColumns[playerIndex] = NO_IMPORTED_COLUMN;
      }

      SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, PROMPT_TEXT_SLOT,
          TEXT_FLAG_TYPED, 255, 255, 255, OPAQUE_ALPHA);
      f_IsPromptShown = FALSE;
      f_FaceFrame = 0;
      f_UnreadFaceToggle = 0;
      f_FaceFrameTimer = 0;
      /* Bring back the Game Boy characters that HideGameBoyCharacters hid */
      if ((u8)canChooseMarked == TRUE) {
        g_CharacterGrid[GAME_BOY_CHARACTER_ROW][0] = f_SavedGameBoySlots[0];
        g_CharacterGrid[GAME_BOY_CHARACTER_ROW][1] = f_SavedGameBoySlots[1];
        g_CharacterGrid[GAME_BOY_CHARACTER_ROW][2] = f_SavedGameBoySlots[2];
        g_CharacterGrid[GAME_BOY_CHARACTER_ROW][3] = f_SavedGameBoySlots[3];
      }
    } else {
      g_Players[playerIndex].isLefty = 0;
      playerIndex--;
      f_FaceFrame = 0;
      f_FaceFrameTimer = 0;
      f_UnreadFaceToggle ^= 1;
      g_ChoosingControllerIndices[playerIndex] = UNCHOSEN_CONTROLLER;
      g_PlayerControllerIndices[playerIndex] = UNCHOSEN_CONTROLLER;
      character = g_CharacterGrid[memory->characterRows[playerIndex]]
                                 [memory->characterColumns[playerIndex]];
      if (character >= FIRST_IMPORTED_CHARACTER) {
        g_ImportedCharacters[character - FIRST_IMPORTED_CHARACTER].isChosen =
            FALSE;
        g_ImportedCharacterColumns[playerIndex] = NO_IMPORTED_COLUMN;
      }

      SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, PROMPT_TEXT_SLOT,
          TEXT_FLAG_TYPED, 255, 255, 255, OPAQUE_ALPHA);
      f_IsPromptShown = FALSE;
    }

    /* Put back the first frame of the face the cursor left */
    gridMark = g_CharacterGrid[(s16)row][(s16)column];
    faceCharacter = g_CharacterGrid[(s16)row][(s16)column];
    if (gridMark < CHOOSABLE_MARK_LIMIT) {
      slot = faceCharacter + MARKED_CHARACTER_OFFSET;
      faceCharacter = slot;
    } else if (gridMark >= FIRST_IMPORTED_CHARACTER) {
      /*
       * Set in each branch to match: setting slot before the tests, here
       * and in the other face load, gives 94 words differing (20 in shape)
       * against the draft's 10 (4)
       */
      slot = faceCharacter;
      faceCharacter =
          g_ImportedCharacters[gridMark - FIRST_IMPORTED_CHARACTER].character;
    } else {
      slot = faceCharacter;
    }

    faceImage = g_CharacterFaceImages[(s16)slot];
    if (IsGameFlagSet(GAME_FLAG_ALTERNATE_SELECT_FACES)) {
      OpenEncodedAsset(
          g_AlternateFaceAnimationAssets[(s16)faceCharacter][f_FaceFrame],
          &asset);
    } else {
      OpenEncodedAsset(
          g_FaceAnimationAssets[(s16)faceCharacter][f_FaceFrame], &asset);
    }

    ReadEncodedAsset(faceImage, &asset);
    OpenAssetFile(g_CharacterFacePaletteFiles[(s16)faceCharacter][0], &file);
    ReadAssetFile(
        g_CharacterFacePalettes[(s16)slot], GetAssetFileSize(&file), &file);
    g_ChoosingPlayerIndex = playerIndex;
    f_IsComButtonHeld = FALSE;
    f_IsLeftyButtonHeld = FALSE;
    f_PreviewedPalette = NO_PALETTE;
    return 0;
  }

  button = g_ControllerStates[3].buttons |
      (g_ControllerStates[2].buttons |
          (g_ControllerStates[0].buttons | g_ControllerStates[1].buttons));
  if (button & U_CBUTTONS) {
    palette = UP_C_PALETTE;
  } else if (button & D_CBUTTONS) {
    palette = DOWN_C_PALETTE;
  } else if (button & L_CBUTTONS) {
    palette = LEFT_C_PALETTE;
  } else if (button & R_CBUTTONS) {
    palette = RIGHT_C_PALETTE;
  } else {
    palette = NO_PALETTE;
  }

  if (palette != NO_PALETTE) {
    chosenColumn = memory->characterColumns[playerIndex];
    character =
        g_CharacterGrid[memory->characterRows[playerIndex]][chosenColumn];
    if (((u8)canChooseMarked == TRUE) & (character < CHOOSABLE_MARK_LIMIT)) {
      character += MARKED_CHARACTER_OFFSET;
    }

    character = f_ChoiceCharacters[character];
    if (character >= FIRST_IMPORTED_CHARACTER) {
      if (g_ImportedCharacters[character - FIRST_IMPORTED_CHARACTER].isChosen ==
          TRUE) {
        return UNSET_RESULT;
      }

      character =
          g_ImportedCharacters[character - FIRST_IMPORTED_CHARACTER].character;
      g_ImportedCharacterColumns[playerIndex] = chosenColumn;
      if (chosenColumn < 0) {
        g_ImportedCharacterColumns[playerIndex] = 0;
      }

      if (g_ImportedCharacterColumns[playerIndex] >= IMPORTED_CHARACTER_COUNT) {
        g_ImportedCharacterColumns[playerIndex] = IMPORTED_CHARACTER_COUNT - 1;
      }
    }

    /* The preview gives way as the A button's palette does */
    for (index = 0, sameCharacterCount = 0; index != playerIndex; index++) {
      if (g_Players[index].characterIndex == character) {
        sameCharacterPlayers[sameCharacterCount] = index;
        sameCharacterCount++;
      }
    }

    for (index = 0; index != sameCharacterCount; index++) {
      if (g_Players[sameCharacterPlayers[index]].paletteIndex == palette) {
        break;
      }
    }

    if (index == sameCharacterCount) {
      f_PreviewedPalette = palette;
    } else {
      for (index = 0; index != CHARACTER_PALETTE_COUNT; index++) {
        for (otherIndex = 0; otherIndex != sameCharacterCount; otherIndex++) {
          if (g_Players[sameCharacterPlayers[otherIndex]].paletteIndex ==
              index) {
            break;
          }
        }

        if (otherIndex == sameCharacterCount) {
          f_PreviewedPalette = index;
          break;
        }
      }

      if (index == CHARACTER_PALETTE_COUNT) {
        f_PreviewedPalette = 0;
      }
    }
  } else {
    f_PreviewedPalette = NO_PALETTE;
  }

  /* Set only where A with R would leave the player without a controller */
  if (((g_ControllerStates[3].buttons |
           (g_ControllerStates[2].buttons |
               (g_ControllerStates[0].buttons |
                   g_ControllerStates[1].buttons))) &
          R_TRIG) &&
      memory->characterRows[playerIndex] != GAME_BOY_CHARACTER_ROW &&
      (u8)canChooseMarked == FALSE) {
    if (g_ChosenPlayerCount >= MIN_PLAYER_COUNT_WITHOUT_CONTROLLER) {
      if (g_ChoosingPlayerIndex == g_ChosenPlayerCount - 1) {
        for (index = 0; index != g_ChoosingPlayerIndex; index++) {
          if (g_PlayerControllerIndices[index] != NO_CONTROLLER_INDEX) {
            break;
          }
        }

        if (index < g_ChosenPlayerCount - 1) {
          f_IsComButtonHeld = TRUE;
        }
      } else {
        f_IsComButtonHeld = TRUE;
      }
    }
  } else {
    f_IsComButtonHeld = FALSE;
  }

  if (((g_ControllerStates[3].buttons |
           (g_ControllerStates[2].buttons |
               (g_ControllerStates[0].buttons |
                   g_ControllerStates[1].buttons))) &
          Z_OR_L_BUTTONS) &&
      memory->characterRows[playerIndex] != GAME_BOY_CHARACTER_ROW) {
    f_IsLeftyButtonHeld = TRUE;
    return 0;
  }

  f_IsLeftyButtonHeld = FALSE;
  return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", UpdateCharacterSelect);
#endif

void LoadCharacterSelectBuffer(void* buffer, s16 bufferKind) {
  EncodedAsset asset;
  void* image = buffer;

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

/*
 * Puts the second player's grid cursor on the last marked slot of the choice
 * rows, the one that holds the rival.
 */
void PlaceRivalCursor(void) {
  MenuMemory* memory = GetMenuMemory();
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
 * Reads the drive shapes' names and the "L:" and "Drive:" labels into the
 * menu's image buffer, then each slot's first face frame and its palette.
 */
void LoadCharacterFaces(void) {
  EncodedAsset asset;
  AssetFile file;
  CharacterId character = 0;
  u8* images = GetMenuImageBuffer();
  s32 slotIndex;
  AssetId faceAsset;

  OpenEncodedAsset(TRAJECTORY_LABELS_IMAGE_ASSET, &asset);
  ReadEncodedAsset(images, &asset);
  OpenEncodedAsset(STAT_LABELS_IMAGE_ASSET, &asset);
  ReadEncodedAsset(images + CHARACTER_SELECT_FIRST_IMAGE_SIZE, &asset);
  for (slotIndex = 0; slotIndex != CHARACTER_COUNT; slotIndex++) {
    void* faceImage = g_CharacterFaceImages[slotIndex];
    AssetFile* paletteFile = &file;
    void* palette = g_CharacterFacePalettes[slotIndex];

    if (slotIndex >= FIRST_IMPORTED_CHARACTER) {
      if (g_ImportedCharacters[slotIndex - FIRST_IMPORTED_CHARACTER]
              .character >= 0) {
        character = g_ImportedCharacters[slotIndex - FIRST_IMPORTED_CHARACTER]
                        .character;
      }
    } else {
      character = slotIndex;
    }

    if (IsGameFlagSet(GAME_FLAG_ALTERNATE_SELECT_FACES)) {
      faceAsset = g_AlternateFaceAnimationAssets[character][0];
    } else {
      faceAsset = g_FaceAnimationAssets[character][0];
    }

    OpenEncodedAsset(faceAsset, &asset);
    ReadEncodedAsset(faceImage, &asset);
    OpenAssetFile(g_CharacterFacePaletteFiles[character][0], paletteFile);
    ReadAssetFile(palette, GetAssetFileSize(paletteFile), paletteFile);
  }
}

/* A byte's address as an integer, for adding a player number to */
static inline s32 ByteAddress(s8* byte) {
  return (s32)byte;
}

/*
 * The menu memory fields are read through ByteAddress because the original
 * adds the player number before the field offset
 */
void DrawCharacterSelect(Gfx** displayListHead, u8 isSinglePlayer) {
  Gfx* gfx = *displayListHead;
  s32 playerNumber;
  s16 column;
  s16 slot;
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
  DrawMenuScenery(&gfx);
  DrawMenuBanner(&gfx);
  DrawCharacterBoard(&gfx);
  DrawGridFaces(&gfx, isSinglePlayer);
  if (isSinglePlayer == TRUE) {
    DrawPlayerTags(&gfx, 2);
  } else {
    DrawPlayerTags(&gfx, 1);
  }

  if ((f_IsComButtonHeld == TRUE) & (isSinglePlayer == FALSE)) {
    DrawComIcon(&gfx, 0);
  }

  if (f_IsLeftyButtonHeld == TRUE) {
    DrawLeftyIcon(&gfx, 0);
  }

  gDPPipeSync(gfx++);
  if (isSinglePlayer == FALSE) {
    playerNumber = g_ChoosingPlayerIndex;
    if (playerNumber >= g_ChosenPlayerCount) {
      playerNumber = g_ChosenPlayerCount - 1;
    }
  } else {
    playerNumber = 1;
  }

  column = *(s8*)(ByteAddress(&GetMenuMemory()->characterColumns[1]) +
      (s16)playerNumber - 1);
  slot =
      g_CharacterGrid[*(s8*)(ByteAddress(&GetMenuMemory()->characterRows[1]) +
          (s16)playerNumber - 1)][column];
  if (slot < CHOOSABLE_MARK_LIMIT) {
    choiceIndex = slot + MARKED_CHARACTER_OFFSET;
  } else {
    choiceIndex = slot;
  }

  DrawCurveMeter(&gfx, f_ChoiceCharacters[choiceIndex]);
  SetUpTextDrawing(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  SetTextName(g_CharacterNames[choiceIndex]);
  SetTextSlot(CHARACTER_NAME_X, CHARACTER_NAME_Y,
      MESSAGE_FIRST_CHARACTER_NAME + choiceIndex, TEXT_SLOT_LARGE_FONT, 1,
      TEXT_FLAG_CENTERED, 255, 255, 255, OPAQUE_ALPHA);
  clubSet = f_ChoiceCharacters[choiceIndex];
  BeginShadowedText(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  sprintf(g_PrintBuffer, "%dY",
      g_Clubs[(clubSet * CLUB_SET_SIZE) + CLUB_TYPE_DRIVER].distance);
  DrawShadowedText(&gfx, DRIVE_DISTANCE_X, DRIVE_DISTANCE_Y, g_PrintBuffer);
  if (f_IsPromptShown == FALSE) {
    if (isSinglePlayer == TRUE) {
      SetTextSlot(PROMPT_X, PROMPT_Y, MESSAGE_CHOOSE_CHARACTER,
          TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_TYPED, 255, 255, 255,
          OPAQUE_ALPHA);
    } else {
      SetTextSlotValue(g_ChoosingPlayerIndex + 1, 0, 0);

      /* The prompt names the player in the player's own color */
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

      SetTextSlot(PROMPT_X, PROMPT_Y, MESSAGE_CHOOSE_PLAYER_CHARACTER,
          TEXT_SLOT_LARGE_FONT, 0, TEXT_FLAG_TYPED, 255, 255, 255,
          OPAQUE_ALPHA);
    }

    f_IsPromptShown = TRUE;
  }

  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

s32 GetUnusedPalette(s32 choiceIndex);

/* DrawGridFaces draws each grid cell's 32 by 32 face, its row and column 40
 * pixels apart */
#define FACE_SIZE 32
#define FACE_SPACING 40
#define FACE_GRID_LEFT 24
#define FACE_GRID_TOP 56
/* Grid marks below this are characters still to be won, drawn as dark shapes in
 * single player */
#define HIDDEN_MARK_LIMIT (-10)
/* The faces' animation steps a frame every four draws, through eight frames */
#define FACE_FRAME_DURATION 4

/*
 * Draws each grid cell's face, with characters still to be won as dark shapes
 * in single player, and every few draws steps the current player's face to its
 * next frame.
 * Doesn't match: the original keeps row in a temporary register and spills the
 * menu memory pointer, where GCC keeps the pointer in a saved register and
 * spills row instead. Comparing row as a byte with the cursor's row, the
 * permuter's best form, gives 175 words, 13 in shape.
 *
 * The original sets the alternate faces' table on the flag test's
 * fall-through path, as a positive test of the flag would, and jumps over
 * the other table. Testing the flag positively, with if/else or ?:, gives
 * that order but 97 words (25 in shape); opening the asset in each arm gives
 * 134 (24), or 140 (26) with the negated test. 91 words differ, 25 in shape.
 *
 * The register oracle's closest result is these 91 words with no register
 * forced, so allocation isn't the cause.
 */
#ifdef NON_MATCHING
void DrawGridFaces(Gfx** displayListHead, u8 isSinglePlayer) {
  EncodedAsset asset;
  AssetFile file;
  Gfx* gfx = *displayListHead;
  MenuMemory* memory = GetMenuMemory();
  s16 row;
  s16 column;
  u16 slot;
  s16 gridMark;
  u16 character;
  void* faceImage;
  void* facePalette;
  u16(*faceAssets)[8];

  for (row = 0; row != CHARACTER_GRID_ROW_COUNT; row++) {
    for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
      /*
       * In its own block, as without it 105 words differ, 35 in shape, against
       * 91 and 25
       */
      IN_OWN_BLOCK(slot = g_CharacterGrid[row][column]);
      if (g_MenuScreen == MENU_SCREEN_OPPONENT_SELECT &&
          row == memory->characterRows[0] &&
          column == memory->characterColumns[0] &&
          g_Players[0].characterIndex >= FIRST_IMPORTED_CHARACTER) {
        slot = column + FIRST_IMPORTED_CHARACTER;
      }

      if ((s16)slot >= 0) {
        if ((s16)slot >= CHARACTER_COUNT) {
          slot = CHARACTER_COUNT - 1;
        }

        faceImage = g_CharacterFaceImages[(s16)slot];
        facePalette = g_CharacterFacePalettes[(s16)slot];
        gDPLoadTLUT_pal256(gfx++, TextureAddress(facePalette));
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPLoadTextureBlock(gfx++, TextureAddress(faceImage), G_IM_FMT_CI,
            G_IM_SIZ_8b, FACE_SIZE, FACE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
            G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++,
            QuarterPixels(column * FACE_SPACING + FACE_GRID_LEFT),
            QuarterPixels(row * FACE_SPACING + FACE_GRID_TOP),
            QuarterPixels(column * FACE_SPACING + FACE_GRID_LEFT + FACE_SIZE),
            QuarterPixels(row * FACE_SPACING + FACE_GRID_TOP + FACE_SIZE),
            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
      } else if (isSinglePlayer == TRUE &&
          g_CharacterGrid[row][column] < HIDDEN_MARK_LIMIT) {
        faceImage = g_CharacterFaceImages[(s16)slot + MARKED_CHARACTER_OFFSET];
        facePalette =
            g_CharacterFacePalettes[(s16)slot + MARKED_CHARACTER_OFFSET];
        gDPLoadTLUT_pal256(gfx++, TextureAddress(facePalette));
        gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        gDPSetPrimColor(gfx++, 0, 0, 10, 10, 10, OPAQUE_ALPHA);
        gDPLoadTextureBlock(gfx++, TextureAddress(faceImage), G_IM_FMT_CI,
            G_IM_SIZ_8b, FACE_SIZE, FACE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
            G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++,
            QuarterPixels(column * FACE_SPACING + FACE_GRID_LEFT),
            QuarterPixels(row * FACE_SPACING + FACE_GRID_TOP),
            QuarterPixels(column * FACE_SPACING + FACE_GRID_LEFT + FACE_SIZE),
            QuarterPixels(row * FACE_SPACING + FACE_GRID_TOP + FACE_SIZE),
            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
      }
    }
  }

  f_FaceFrameTimer++;
  if (f_FaceFrameTimer == FACE_FRAME_DURATION) {
    f_FaceFrameTimer = 0;
    f_FaceFrame++;
    if (f_FaceFrame >= FACE_ANIMATION_FRAME_COUNT) {
      f_FaceFrame = 0;
    }

    f_UnreadFaceToggle ^= 1;
    gridMark = g_CharacterGrid[memory->characterRows[g_ChoosingPlayerIndex]]
                              [memory->characterColumns[g_ChoosingPlayerIndex]];
    character =
        g_CharacterGrid[memory->characterRows[g_ChoosingPlayerIndex]]
                       [memory->characterColumns[g_ChoosingPlayerIndex]];
    if (gridMark < CHOOSABLE_MARK_LIMIT) {
      character += MARKED_CHARACTER_OFFSET;
      slot = character;
    } else if (gridMark >= FIRST_IMPORTED_CHARACTER) {
      slot = character;
      character =
          g_ImportedCharacters[gridMark - FIRST_IMPORTED_CHARACTER].character;
    } else {
      slot = character;
    }

    faceImage = g_CharacterFaceImages[(s16)slot];
    facePalette = g_CharacterFacePalettes[(s16)slot];
    if (f_PreviewedPalette == 0) {
      OpenAssetFile(g_CharacterFacePaletteFiles[(s16)character][0], &file);
    } else if (f_PreviewedPalette == 1) {
      OpenAssetFile(g_CharacterFacePaletteFiles[(s16)character][1], &file);
    } else if (f_PreviewedPalette == 2) {
      OpenAssetFile(g_CharacterFacePaletteFiles[(s16)character][2], &file);
    } else if (f_PreviewedPalette == 3) {
      OpenAssetFile(g_CharacterFacePaletteFiles[(s16)character][3], &file);
    } else {
      OpenAssetFile(g_CharacterFacePaletteFiles[(s16)character][(
                        s16)GetUnusedPalette((s16)character)],
          &file);
    }

    ReadAssetFile(facePalette, (s16)GetAssetFileSize(&file), &file);
    if (!IsGameFlagSet(GAME_FLAG_ALTERNATE_SELECT_FACES)) {
      faceAssets = g_FaceAnimationAssets;
    } else {
      faceAssets = g_AlternateFaceAnimationAssets;
    }

    OpenEncodedAsset(faceAssets[(s16)character][f_FaceFrame], &asset);
    ReadEncodedAsset(faceImage, &asset);
    f_UnreadFaceColumn = memory->characterColumns[g_ChoosingPlayerIndex];
    f_UnreadFaceRow = memory->characterRows[g_ChoosingPlayerIndex];
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/character_select", DrawGridFaces);
#endif

/* The first palette that no earlier player with the same character uses, so
 * that players who choose one character still look different */
s32 GetUnusedPalette(s32 choiceIndex) {
  s32 freePalette = 0;
  s32 playerIndex = 0;
  s32 sameCount = 0;
  CharacterId character = f_ChoiceCharacters[choiceIndex];
  /* Pads the frame to match; without it 8 words differ */
  s32 unused[4] UNUSED;
  s32 samePlayers[MAX_PLAYER_COUNT];
  s32 sameIndex;

  for (; playerIndex != g_ChoosingPlayerIndex; playerIndex++) {
    if (g_Players[playerIndex].characterIndex == character) {
      samePlayers[sameCount++] = playerIndex;
    }
  }

  if (sameCount != 0) {
    /* The palette search reuses playerIndex, which the original does to keep
     * the first loop's index and count in their registers */
    for (playerIndex = 0; playerIndex != CHARACTER_PALETTE_COUNT;
        playerIndex++) {
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

/* Draws the wooden board of character cells, then its "Drive:" label */
void DrawCharacterBoard(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 stripIndex;
  s32 stripTop;
  u8* labelImage;

  GetMenuImageBuffer();
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_CharacterBoard)));
  for (stripIndex = 0; stripIndex != BOARD_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock_4b(gfx++,
        TextureAddress(ImageTexels(f_CharacterBoard) +
            (stripIndex * BOARD_WIDTH * BOARD_STRIP_HEIGHT / 2)),
        G_IM_FMT_CI, BOARD_WIDTH, BOARD_STRIP_HEIGHT, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    stripTop = BOARD_TOP + (stripIndex * BOARD_STRIP_HEIGHT);
    gSPScisTextureRectangle(gfx++, QuarterPixels(BOARD_LEFT),
        QuarterPixels(stripTop), QuarterPixels(BOARD_LEFT + BOARD_WIDTH),
        QuarterPixels(stripTop + BOARD_STRIP_HEIGHT), G_TX_RENDERTILE, 0, 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  /*
   * Offset in a second statement, to match: adding the offset to the call's
   * result gives 6 words differing, 2 in shape
   */
  labelImage = GetMenuImageBuffer();
  labelImage += CHARACTER_SELECT_FIRST_IMAGE_SIZE;
  LoadSmallPalette(&gfx, 0, ImageBody(labelImage));
  LoadNibbleTextureBlock(&gfx, ImageTexels(labelImage), G_IM_FMT_CI,
      LABEL_IMAGE_WIDTH, LABEL_IMAGE_HEIGHT, 0, 0, 0, G_TX_NOMASK, G_TX_NOMASK,
      G_TX_NOLOD, G_TX_NOLOD);
  DrawTextureRect(&gfx, QuarterPixels(LABEL_LEFT), QuarterPixels(LABEL_TOP),
      QuarterPixels(LABEL_LEFT + LABEL_SHOWN_WIDTH),
      QuarterPixels(LABEL_TOP + LABEL_IMAGE_HEIGHT), G_TX_RENDERTILE,
      TexelCoordinate(LABEL_SHOWN_WIDTH), 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  *displayListHead = gfx;
}

void DrawCharacterGridCursor(Gfx** displayListHead, s32 x, s32 y);

/* How DrawPlayerTags places the tags. Nothing passes PLAYER_TAG_LINED_UP. */
#define PLAYER_TAG_LINED_UP 0
/* The second tag then reads "Com" */
#define PLAYER_TAG_SINGLE_PLAYER 2
/* The player tag images: 1P to 4P, then 1C to 4C, then Com */
#define PLAYER_TAG_FIRST_PLAYER 0
#define PLAYER_TAG_FIRST_COMPUTER 4
#define PLAYER_TAG_COM 8
/* Bytes, as 16 by 8 nibbles */
#define PLAYER_TAG_IMAGE_SIZE 64
/*
 * The texels of the tag playerIndex tags past firstTag, as the tags sit side
 * by side in one image. The two offsets are scaled apart, to match: scaling
 * their sum gives 14 words differing (10 in shape) in DrawPlayerTags' draft,
 * against 2 (2).
 */
#define PlayerTagTexels(firstTag, playerIndex) \
  (TextureAddress(ImageTexelsAt(f_PlayerTags, \
      ((playerIndex) * PLAYER_TAG_IMAGE_SIZE) + \
          ((firstTag) * PLAYER_TAG_IMAGE_SIZE))))
#define PLAYER_TAG_WIDTH 16
#define PLAYER_TAG_HEIGHT 8
#define PLAYER_TAG_SHIFT_LEFT 4
#define PLAYER_TAG_TOP 56
/* Lined-up tags alternate between two heights */
#define PLAYER_TAG_RAISE (-14)
/* The character grid's cells on screen */
#define GRID_CELL_SIZE 40
#define GRID_LEFT 28
#define GRID_TOP 48
#define GRID_CURSOR_SHIFT_LEFT 8

/*
 * Draws each player's tag on the cell where that player's cursor sits, then the
 * grid cursor of the player choosing.
 */
void DrawPlayerTags(Gfx** displayListHead, u8 tagMode) {
  MenuMemory* memory = GetMenuMemory();
  s16 tagCount = g_ChoosingPlayerIndex + 1;
  Gfx* gfx = *displayListHead;
  s8 playerIndex;
  s16 left;
  s16 shiftX; /* Both layouts leave it at 0 */
  s16 top;
  s16 cursorLeft;
  s16 cursorTop;
  s8 column;
  s8 row;

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_PlayerTags)));
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
    } else {
      left = f_LinedUpTagLefts[tagCount - 1][playerIndex];
      top = ((playerIndex & 1) == 0) ? PLAYER_TAG_RAISE : 0;
      if ((tagCount == 2) & (playerIndex == 1)) {
        top = PLAYER_TAG_RAISE;
      }

      shiftX = 0;
    }

    if ((tagMode == PLAYER_TAG_SINGLE_PLAYER) & (playerIndex == 1)) {
      gDPLoadTextureBlock_4b(gfx++, PlayerTagTexels(PLAYER_TAG_COM, 0),
          G_IM_FMT_CI, PLAYER_TAG_WIDTH, PLAYER_TAG_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    } else if (g_PlayerControllerIndices[playerIndex] == NO_CONTROLLER_INDEX) {
      gDPLoadTextureBlock_4b(gfx++,
          PlayerTagTexels(PLAYER_TAG_FIRST_COMPUTER, playerIndex), G_IM_FMT_CI,
          PLAYER_TAG_WIDTH, PLAYER_TAG_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
          G_TX_NOLOD);
    } else if (f_IsComButtonHeld == TRUE &&
        playerIndex == g_ChoosingPlayerIndex) {
      gDPLoadTextureBlock_4b(gfx++,
          PlayerTagTexels(PLAYER_TAG_FIRST_COMPUTER, playerIndex), G_IM_FMT_CI,
          PLAYER_TAG_WIDTH, PLAYER_TAG_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
          G_TX_NOLOD);
    } else {
      gDPLoadTextureBlock_4b(gfx++,
          PlayerTagTexels(PLAYER_TAG_FIRST_PLAYER, playerIndex), G_IM_FMT_CI,
          PLAYER_TAG_WIDTH, PLAYER_TAG_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
          G_TX_NOLOD);
    }

    left = shiftX + (s16)(left + GRID_LEFT);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, QuarterPixels(left - PLAYER_TAG_SHIFT_LEFT),
        QuarterPixels(top + PLAYER_TAG_TOP),
        QuarterPixels(left - PLAYER_TAG_SHIFT_LEFT + PLAYER_TAG_WIDTH),
        QuarterPixels(top + PLAYER_TAG_TOP + PLAYER_TAG_HEIGHT),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  /* playerIndex now picks whose cursor to draw */
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

/*
 * Draws the menu cursor at (x, y), its vertex colors cycling a little further
 * each frame. The screen position goes in locals before the call because the
 * original loads the 96 for its y after g_DrawBufferIndex.
 */
void DrawCharacterGridCursor(Gfx** displayListHead, s32 x, s32 y) {
  Gfx* gfx = *displayListHead;
  s32 vertexIndex;
  s32 red;
  s32 blue;
  s32 green;
  f32 screenX;
  f32 screenY;

  for (vertexIndex = 0; vertexIndex != MENU_CURSOR_VERTEX_COUNT;
      vertexIndex++) {
    f_GridCursorPhases[vertexIndex] += MENU_CURSOR_PHASE_STEP;
    red =
        (s32)(sinf(f_GridCursorPhases[vertexIndex]) * MENU_CURSOR_COLOR_SWING) +
        MENU_CURSOR_COLOR_MIDDLE;
    blue = (s32)(sinf(f_GridCursorPhases[vertexIndex] + (M_PI / 2)) *
               MENU_CURSOR_COLOR_SWING) +
        MENU_CURSOR_COLOR_MIDDLE;
    green = (s32)(sinf(f_GridCursorPhases[vertexIndex] + M_PI) *
                MENU_CURSOR_COLOR_SWING) +
        MENU_CURSOR_COLOR_MIDDLE;
    f_GridCursorVertices[vertexIndex].v.cn[0] = red;
    f_GridCursorVertices[vertexIndex].v.cn[2] = blue;
    f_GridCursorVertices[vertexIndex].v.cn[1] = green;
  }

  guOrtho(&f_GridCursorProjection, -(SCREEN_WIDTH / 2), SCREEN_WIDTH / 2,
      -(SCREEN_HEIGHT / 2), SCREEN_HEIGHT / 2, MENU_CURSOR_NEAR,
      MENU_CURSOR_FAR, 1.0f);
  screenX = (f32)(x - CHARACTER_CURSOR_ORIGIN_X) - HALF_PIXEL;
  screenY = (f32)(CHARACTER_CURSOR_ORIGIN_Y - y) + HALF_PIXEL;
  guTranslate(&f_GridCursorTranslations[g_DrawBufferIndex], screenX, screenY,
      MENU_CURSOR_Z);
  guRotate(&f_GridCursorRotations[g_DrawBufferIndex], 0.0f, 0.0f, 1.0f, 0.0f);
  guScale(&f_GridCursorScales[g_DrawBufferIndex], 1.0f, 1.0f, 1.0f);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPLoadTextureBlock_4b(gfx++, TextureAddress(ImageBody(f_GridCursorBox)),
      G_IM_FMT_IA, MENU_CURSOR_SIZE, MENU_CURSOR_SIZE, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
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
  gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
  gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
  gDPPipeSync(gfx++);
  gSPMatrix(gfx++, PhysicalAddress(&f_GridCursorProjection),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  gSPMatrix(gfx++,
      PhysicalAddress(&f_GridCursorTranslations[g_DrawBufferIndex]),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
  gSPMatrix(gfx++, PhysicalAddress(&f_GridCursorRotations[g_DrawBufferIndex]),
      G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
  gSPMatrix(gfx++, PhysicalAddress(&f_GridCursorScales[g_DrawBufferIndex]),
      G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
  gDPPipeSync(gfx++);
  gSPVertex(gfx++, f_GridCursorVertices, MENU_CURSOR_VERTEX_COUNT, 0);
  gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/* Draws the "R: COM" icon, shown while R is held outside single player. Its
 * caller passes a second argument that it ignores. */
void DrawComIcon(Gfx** displayListHead, s32 unused UNUSED) {
  Gfx* gfx = *displayListHead;

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
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(g_ComLeftyIcons)));
  gDPPipeSync(gfx++);
  gDPLoadTextureBlock_4b(gfx++, TextureAddress(ImageTexels(g_ComLeftyIcons)),
      G_IM_FMT_CI, BUTTON_ICON_WIDTH, BUTTON_ICON_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPTextureRectangle(gfx++, QuarterPixels(COMPUTER_ICON_LEFT),
      QuarterPixels(BUTTON_ICON_TOP),
      QuarterPixels(COMPUTER_ICON_LEFT + BUTTON_ICON_WIDTH),
      QuarterPixels(BUTTON_ICON_TOP + BUTTON_ICON_HEIGHT), G_TX_RENDERTILE, 0,
      0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/* Draws the "L: LEFTY" icon, shown while Z or L is held. Its caller passes a
 * second argument that it ignores. */
void DrawLeftyIcon(Gfx** displayListHead, s32 unused UNUSED) {
  Gfx* gfx = *displayListHead;

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
  gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(g_ComLeftyIcons)));
  gDPPipeSync(gfx++);
  gDPLoadTextureBlock_4b(gfx++,
      TextureAddress(ImageTexels(g_ComLeftyIcons) + LEFTY_ICON_OFFSET),
      G_IM_FMT_CI, BUTTON_ICON_WIDTH, BUTTON_ICON_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPTextureRectangle(gfx++, QuarterPixels(LEFTY_ICON_LEFT),
      QuarterPixels(BUTTON_ICON_TOP),
      QuarterPixels(LEFTY_ICON_LEFT + BUTTON_ICON_WIDTH),
      QuarterPixels(BUTTON_ICON_TOP + BUTTON_ICON_HEIGHT), G_TX_RENDERTILE, 0,
      0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/*
 * Draws the shot curve of the club set's driver as a meter filled from its
 * middle toward the draw or the fade, beside the label that names the curve.
 */
void DrawCurveMeter(Gfx** displayListHead, s32 clubSet) {
  Gfx* gfx = *displayListHead;
  s32 curve = g_Clubs[clubSet * CLUB_SET_SIZE].curve;
  s32 firstCell;
  s32 cellIndex;
  s32 endCell;
  u8* labelImage;
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
  gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(f_CurveMeterCells)));
  for (cellIndex = firstCell; cellIndex != endCell; cellIndex++) {
    gDPLoadTextureBlock(gfx++,
        TextureAddress(LargeImageTexels(f_CurveMeterCells) +
            (cellIndex * CURVE_METER_CELL_SIZE * CURVE_METER_CELL_SIZE)),
        G_IM_FMT_CI, G_IM_SIZ_8b, CURVE_METER_CELL_SIZE, CURVE_METER_CELL_SIZE,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++,
        QuarterPixels(CURVE_METER_LEFT + (cellIndex * CURVE_METER_CELL_SIZE)),
        QuarterPixels(CURVE_METER_TOP),
        QuarterPixels(CURVE_METER_LEFT + (cellIndex * CURVE_METER_CELL_SIZE) +
            CURVE_METER_CELL_SIZE),
        QuarterPixels(CURVE_METER_TOP + CURVE_METER_CELL_SIZE), G_TX_RENDERTILE,
        0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }

  labelImage = GetMenuImageBuffer();
  LoadSmallPalette(&gfx, 0, ImageBody(labelImage));
  LoadNibbleTextureBlock(&gfx, ImageTexels(labelImage), G_IM_FMT_CI,
      CURVE_LABEL_WIDTH, CURVE_LABEL_IMAGE_HEIGHT, 0, 0, 0, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  if (g_Clubs[clubSet * CLUB_SET_SIZE].curve > 0) {
    labelRow = CURVE_LABEL_DRAW;
  } else if (g_Clubs[clubSet * CLUB_SET_SIZE].curve < 0) {
    labelRow = CURVE_LABEL_FADE;
  } else {
    labelRow = CURVE_LABEL_STRAIGHT;
  }

  DrawTextureRect(&gfx, QuarterPixels(CURVE_LABEL_LEFT),
      QuarterPixels(CURVE_LABEL_TOP),
      QuarterPixels(CURVE_LABEL_LEFT + CURVE_LABEL_WIDTH),
      QuarterPixels(CURVE_LABEL_TOP + CURVE_LABEL_HEIGHT), G_TX_RENDERTILE, 0,
      TexelCoordinate(labelRow * CURVE_LABEL_HEIGHT), TEXEL_STEP_ONE,
      TEXEL_STEP_ONE);
  *displayListHead = gfx;
}
