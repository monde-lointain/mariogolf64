#ifndef SAVE_SCREENS_H
#define SAVE_SCREENS_H

#include <ultra64.h>

#include "characters/character.h"
#include "system/asset.h"

extern PaletteColor g_MenuCharacterPalettes[CHARACTER_COUNT][LARGE_PALETTE_COLOR_COUNT];

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

enum SaveMenuBuffer {
    SAVE_MENU_BUFFER_SLOTS = 1,
    SAVE_MENU_BUFFER_CHOSEN
};

typedef u8 SaveMenuBuffer;

enum GameModeLabelBuffer {
    GAME_MODE_LABEL_BUFFER_NAME_PLATE = 1,
    GAME_MODE_LABEL_BUFFER_LABELS
};

typedef u8 GameModeLabelBuffer;

enum RingShotRecordBuffer {
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
