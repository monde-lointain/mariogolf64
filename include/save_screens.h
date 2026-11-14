#ifndef SAVE_SCREENS_H
#define SAVE_SCREENS_H

#include <ultra64.h>

#include "character.h"

#define MENU_PALETTE_FILE_SIZE 0x200
extern u8 g_MenuCharacterPalettes[CHARACTER_COUNT][MENU_PALETTE_FILE_SIZE];

enum SaveGameBuffer {
    SAVE_GAME_BUFFER_CONTROLLER_ICONS = 1,
    SAVE_GAME_BUFFER_PLAYER_LABELS,
    SAVE_GAME_BUFFER_FIRST_BOARD,
    SAVE_GAME_BUFFER_SECOND_BOARD,
    SAVE_GAME_BUFFER_THIRD_BOARD,
    SAVE_GAME_BUFFER_COURSE,
    SAVE_GAME_BUFFER_CHOSEN_COURSE
};

enum SaveMenuBuffer {
    SAVE_MENU_BUFFER_SLOTS = 1,
    SAVE_MENU_BUFFER_CHOSEN
};

enum GameModeLabelBuffer {
    GAME_MODE_LABEL_BUFFER_NAME_PLATE = 1,
    GAME_MODE_LABEL_BUFFER_LABELS
};

enum RingShotRecordBuffer {
    RING_SHOT_RECORD_BUFFER_UNREAD = 1,
    RING_SHOT_RECORD_BUFFER_RAINBOW_BOX,
    RING_SHOT_RECORD_BUFFER_COURSE_NUMBERS,
    RING_SHOT_RECORD_BUFFER_GREEN_ARROWS
};

void LoadRingShotRecordBuffer(void *buffer, s16 bufferKind);
void LoadRingShotCourseIcons(void **buffers);
void UpdateRingShotRecords(void);
void DrawRingShotRecords(Gfx **displayListHead);
void ReadRingShotReplays(void);
void LoadRingShotRecordFaces(void);
void LoadSaveGameBuffer(void *buffer, u8 bufferKind);
void ReadSavedGames(void);
void LoadSaveMenuBuffers(void **buffers, u8 bufferKind);
void LoadGameModeLabelBuffer(void *buffer, u8 bufferKind);
void UpdateSavedGames(void);
void DrawSavedGames(Gfx **displayListHead);

#endif
