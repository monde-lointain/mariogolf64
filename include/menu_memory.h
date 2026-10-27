#ifndef MENU_MEMORY_H
#define MENU_MEMORY_H

#include <ultra64.h>

/* The values of unkE, which func_80217B88 also turns into the settings of D_801B60BA */
#define MENU_UNKE_COUNT 8

/* What the menus keep between visits, such as the last choices made */
typedef struct MenuMemory {
    /* 0x00 */ u8 unk0[4];
    /* 0x04 */ s8 characterColumn; /* The character select cursor's column */
    /* 0x05 */ u8 unk5[3];
    /* 0x08 */ s8 characterRow; /* And its row */
    /* 0x09 */ u8 unk9[0xD - 0x9];
    /* 0x0D */ s8 unkD;
    /* 0x0E */ s8 unkE[MENU_UNKE_COUNT]; /* Set from one of func_80217B88's presets */
    /* 0x16 */ u8 unk16[0x20 - 0x16];
    /* 0x20 */ s8 helpTopicIndex;
    /* 0x21 */ u8 unk21[0x29 - 0x21];
    /* 0x29 */ s8 unk29;
    /* 0x2A */ s8 unk2A;
    /* 0x2B */ u8 unk2B[0x2E - 0x2B];
    /* 0x2E */ s8 holeIndex;   /* The hole the Ring Shot select points at */
    /* 0x2F */ s8 courseIndex; /* And its course */
    /* 0x30 */ s8 unk30; /* A course, or COURSE_COUNT for none */
    /* 0x31 */ u8 unk31[0x35 - 0x31];
    /* 0x35 */ s8 tableColumn;      /* The cursor's place among a table's shown columns */
    /* 0x36 */ s8 tableRow;         /* And among its shown rows */
    /* 0x37 */ s8 tableFirstColumn; /* The first column shown of a table wider than the screen */
    /* 0x38 */ s8 tableFirstRow;    /* And the first row shown of one taller */
    /* 0x39 */ s8 unk39;
    /* 0x3A */ s8 unk3A;
    /* 0x3B */ u8 unk3B[0x42 - 0x3B];
    /* 0x42 */ s8 unk42;
    /* 0x43 */ s8 savedGameIndex;
    /* 0x44 */ u8 unk44[0x46 - 0x44];
    /* 0x46 */ u8 unk46;
    /* 0x47 */ s8 unk47;
    /* 0x48 */ s8 unk48;
} MenuMemory;

MenuMemory *func_8005AF74(void);

#endif
