/*
 * The table of the characters' abilities, in ovl_01/character_table.c
 */

#ifndef CHARACTER_TABLE_H
#define CHARACTER_TABLE_H

#include <ultra64.h>

// The buffers that LoadCharacterTableBuffer keeps, by its caller's kind
enum TableBuffer {
    TABLE_BUFFER_CURSOR = 1,
    TABLE_BUFFER_UNREAD,
    TABLE_BUFFER_ARROWS,
    TABLE_BUFFER_STAR
};

typedef s16 TableBuffer;
void LoadCharacterTableBuffer(void *buffer, TableBuffer bufferKind);
void UpdateCharacterTable(void);
void DrawCharacterTable(Gfx **displayListHead);
void MarkShownTableCharacters(void);
void LoadTableFaces(void);

#endif
