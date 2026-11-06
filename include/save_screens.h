#ifndef SAVE_SCREENS_H
#define SAVE_SCREENS_H

#include <ultra64.h>

#include "character.h"

/* The Ring Shot records and the saved games, in ovl_01/save_menu.c */

/* The most bytes of each character palette file that the menus keep */
#define MENU_PALETTE_FILE_SIZE 0x200
/* Character palette files that the menu screens read, one per slot */
extern u8 g_MenuCharacterPalettes[CHARACTER_COUNT][MENU_PALETTE_FILE_SIZE];

/* The buffers of LoadSaveGameBuffer, and what it reads into them */
enum SaveGameBuffer {
  SAVE_GAME_BUFFER_CONTROLLER_ICONS = 1,
  SAVE_GAME_BUFFER_PLAYER_LABELS,
  SAVE_GAME_BUFFER_FIRST_BOARD,
  SAVE_GAME_BUFFER_SECOND_BOARD,
  SAVE_GAME_BUFFER_THIRD_BOARD,
  SAVE_GAME_BUFFER_COURSE,
  SAVE_GAME_BUFFER_CHOSEN_COURSE
};

/* What LoadSaveMenuBuffers reads: each saved game's mode image and a frame, or
 * the chosen game's two */
enum SaveMenuBuffer {
  SAVE_MENU_BUFFER_SLOTS = 1,
  SAVE_MENU_BUFFER_CHOSEN
};

/* The buffers that LoadGameModeLabelBuffer keeps, by its caller's kind */
enum GameModeLabelBuffer {
  GAME_MODE_LABEL_BUFFER_NAME_PLATE = 1,
  GAME_MODE_LABEL_BUFFER_LABELS
};

/* The buffers that LoadRingShotRecordBuffer keeps, by its caller's kind */
enum RingShotRecordBuffer {
  /* Kept without reading an image into it */
  RING_SHOT_RECORD_BUFFER_UNREAD = 1,
  RING_SHOT_RECORD_BUFFER_RAINBOW_BOX,
  RING_SHOT_RECORD_BUFFER_COURSE_NUMBERS,
  RING_SHOT_RECORD_BUFFER_GREEN_ARROWS
};

void LoadRingShotRecordBuffer(void* buffer, s16 bufferKind);
void LoadRingShotCourseIcons(void** buffers);
void UpdateRingShotRecords(void);
void DrawRingShotRecords(Gfx** displayListHead);
void ReadRingShotReplays(void);
void LoadRingShotRecordFaces(void);
void LoadSaveGameBuffer(void* buffer, u8 bufferKind);
void ReadSavedGames(void);
void LoadSaveMenuBuffers(void** buffers, u8 bufferKind);
void LoadGameModeLabelBuffer(void* buffer, u8 bufferKind);
void UpdateSavedGames(void);
void DrawSavedGames(Gfx** displayListHead);

#endif
