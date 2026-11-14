/*
 * The character select, in ovl_01/character_select.c
 */

#ifndef CHARACTER_SELECT_H
#define CHARACTER_SELECT_H

#include <ultra64.h>

#include "system/c_language.h"

// The buffers that LoadCharacterSelectBuffer keeps, by the kind its caller
// gives, and the image of f_SelectBufferAssets that it reads into each. Callers
// pass no kind below 1, nor 3, 4, 6, 7 or 9
enum CharacterSelectBuffer {
    CHARACTER_SELECT_BUFFER_PLAYER_LABELS = 1,
    // The wooden board of character cells
    CHARACTER_SELECT_BUFFER_BOARD,
    // The CHARACTER SELECT title
    CHARACTER_SELECT_BUFFER_TITLE,
    CHARACTER_SELECT_BUFFER_NAME_PLATE,
    CHARACTER_SELECT_BUFFER_CURSOR_BOX,
    // Small controllers numbered 1 to 4
    CHARACTER_SELECT_BUFFER_CONTROLLER_ICONS,
    // A blue bar and a green bar
    CHARACTER_SELECT_BUFFER_COLOR_BARS,
    // The cells of the shot curve meter
    CHARACTER_SELECT_BUFFER_CURVE_CELLS,
    // Kept without reading an image into it
    CHARACTER_SELECT_BUFFER_UNREAD = 10
};

typedef s16 CharacterSelectBuffer;

void HideGameBoyCharacters(void);
s32 UpdateCharacterSelect(s32 firstPlayerIndex, s32 playerCount, Boolean canChooseMarked);
void LoadCharacterSelectBuffer(void *buffer, CharacterSelectBuffer bufferKind);
void PlaceRivalCursor(void);
void LoadCharacterFaces(void);
void DrawCharacterSelect(Gfx **displayListHead, UBoolean8 isSinglePlayer);

#endif
