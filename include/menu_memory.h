#ifndef MENU_MEMORY_H
#define MENU_MEMORY_H

#include <ultra64.h>

/* What the menus keep between visits, such as the last choices made */
typedef struct MenuMemory {
    /* 0x00 */ u8 unk0[0x20];
    /* 0x20 */ s8 helpTopicIndex;
    /* 0x21 */ u8 unk21[0x2E - 0x21];
    /* 0x2E */ s8 holeIndex;   /* The hole the Ring Shot select points at */
    /* 0x2F */ s8 courseIndex; /* And its course */
    /* 0x30 */ u8 unk30[0x35 - 0x30];
    /* 0x35 */ s8 tableColumn;      /* The cursor's place among a table's shown columns */
    /* 0x36 */ s8 tableRow;         /* And among its shown rows */
    /* 0x37 */ s8 tableFirstColumn; /* The first column shown of a table wider than the screen */
    /* 0x38 */ s8 tableFirstRow;    /* And the first row shown of one taller */
    /* 0x39 */ u8 unk39[0x46 - 0x39];
    /* 0x46 */ u8 unk46;
} MenuMemory;

MenuMemory *func_8005AF74(void);

#endif
