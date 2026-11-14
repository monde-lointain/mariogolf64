/*
 * The Ring Shot records and the saved games, in ovl_01/save_menu.c
 */

#ifndef SAVE_SCREENS_H
#define SAVE_SCREENS_H

#include <ultra64.h>

#include "characters/character.h"
#include "system/asset.h"

// Character palette files that the menu screens read, one per slot
extern PaletteColor g_MenuCharacterPalettes[CHARACTER_COUNT][LARGE_PALETTE_COLOR_COUNT];

// The buffers of LoadSaveGameBuffer, and what it reads into them
enum SaveGameBuffer {
    SAVE_GAME_BUFFER_CONTROLLER_ICONS = 1,
    SAVE_GAME_BUFFER_PLAYER_LABELS,
    SAVE_GAME_BUFFER_FIRST_BOARD,
    SAVE_GAME_BUFFER_SECOND_BOARD,
    SAVE_GAME_BUFFER_THIRD_BOARD,
    SAVE_GAME_BUFFER_COURSE,
    SAVE_GAME_BUFFER_CHOSEN_COURSE
};

typedef u8 SaveGameBuffer;

// What LoadSaveMenuBuffers reads: each saved game's mode image and a frame, or
// the chosen game's two
enum SaveMenuBuffer {
    SAVE_MENU_BUFFER_SLOTS = 1,
    SAVE_MENU_BUFFER_CHOSEN
};

typedef u8 SaveMenuBuffer;

// The buffers that LoadGameModeLabelBuffer keeps, by its caller's kind
enum GameModeLabelBuffer {
    GAME_MODE_LABEL_BUFFER_NAME_PLATE = 1,
    GAME_MODE_LABEL_BUFFER_LABELS
};

typedef u8 GameModeLabelBuffer;

// The buffers that LoadRingShotRecordBuffer keeps, by its caller's kind
enum RingShotRecordBuffer {
    // Kept without reading an image into it
    RING_SHOT_RECORD_BUFFER_UNREAD = 1,
    RING_SHOT_RECORD_BUFFER_RAINBOW_BOX,
    RING_SHOT_RECORD_BUFFER_COURSE_NUMBERS,
    RING_SHOT_RECORD_BUFFER_GREEN_ARROWS
};

typedef s16 RingShotRecordBuffer;
void LoadRingShotRecordBuffer(void *buffer, RingShotRecordBuffer bufferKind);
void LoadRingShotCourseIcons(ImageAsset **buffers);
void UpdateRingShotRecords(void);
void DrawRingShotRecords(Gfx **displayListHead);
void ReadRingShotReplays(void);
void LoadRingShotRecordFaces(void);
void LoadSaveGameBuffer(void *buffer, SaveGameBuffer bufferKind);
void ReadSavedGames(void);
void LoadSaveMenuBuffers(ImageAsset **buffers, SaveMenuBuffer bufferKind);
void LoadGameModeLabelBuffer(void *buffer, GameModeLabelBuffer bufferKind);
void UpdateSavedGames(void);
void DrawSavedGames(Gfx **displayListHead);

#endif
