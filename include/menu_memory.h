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
    /* 0x30 */ u8 unk30[0x46 - 0x30];
    /* 0x46 */ u8 unk46;
} MenuMemory;

MenuMemory *func_8005AF74(void);

#endif
