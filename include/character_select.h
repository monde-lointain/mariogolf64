#ifndef CHARACTER_SELECT_H
#define CHARACTER_SELECT_H

#include <ultra64.h>

#include "system/c_language.h"

enum CharacterSelectBuffer {
    CHARACTER_SELECT_BUFFER_PLAYER_LABELS = 1,
    CHARACTER_SELECT_BUFFER_BOARD,
    CHARACTER_SELECT_BUFFER_TITLE,
    CHARACTER_SELECT_BUFFER_NAME_PLATE,
    CHARACTER_SELECT_BUFFER_CURSOR_BOX,
    CHARACTER_SELECT_BUFFER_CONTROLLER_ICONS,
    CHARACTER_SELECT_BUFFER_COLOR_BARS,
    CHARACTER_SELECT_BUFFER_CURVE_CELLS,
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
