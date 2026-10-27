#ifndef MENU_MEMORY_H
#define MENU_MEMORY_H

#include <ultra64.h>
#include "save.h"

/* The values of unkE, which func_80217B88 also turns into the settings of D_801B60BA */
#define MENU_UNKE_COUNT 8

/* What the menus keep between visits, such as the last choices made */
typedef struct MenuMemory {
    /* 0x00 */ s8 playerCountChoice; /* The title screen's player count, less one */
    /* 0x01 */ s8 titleChoice;       /* The title screen's row: a new game, the saved games or the menus */
    /* 0x02 */ s8 unk2;
    /* 0x03 */ s8 characterColumns[MAX_PLAYER_COUNT]; /* Each player's character select cursor column */
    /* 0x07 */ s8 characterRows[MAX_PLAYER_COUNT];    /* And row */
    /* 0x0B */ s8 modeChoice; /* The mode select cursor's place in the player count's modes */
    /* 0x0C */ u8 unkC;
    /* 0x0D */ s8 unkD;
    /* 0x0E */ s8 unkE[MENU_UNKE_COUNT]; /* Set from one of func_80217B88's presets */
    /* 0x16 */ s8 courseChoice; /* The course select's course: an open one, or the next one still shut */
    /* 0x17 */ s8 courseSpin;   /* How far the courses still have to turn to reach it */
    /* 0x18 */ u8 unk18[0x20 - 0x18];
    /* 0x20 */ s8 helpTopicIndex;
    /* 0x21 */ u8 unk21[0x29 - 0x21];
    /* 0x29 */ s8 unk29;
    /* 0x2A */ s8 unk2A;
    /* 0x2B */ u8 unk2B[0x2E - 0x2B];
    /* 0x2E */ s8 holeIndex;   /* The hole the Ring Shot select points at */
    /* 0x2F */ s8 courseIndex; /* And its course */
    /* 0x30 */ s8 statsCourse; /* The course whose statistics show; one past the open courses totals them */
    /* 0x31 */ u8 unk31;
    /* 0x32 */ s8 statsSpin; /* How far the courses still have to turn to reach it */
    /* 0x33 */ s8 bestTimesCourse; /* The open course whose best times show */
    /* 0x34 */ s8 bestTimesSpin;   /* How far the courses still have to turn to reach it */
    /* 0x35 */ s8 tableColumn;      /* The cursor's place among a table's shown columns */
    /* 0x36 */ s8 tableRow;         /* And among its shown rows */
    /* 0x37 */ s8 tableFirstColumn; /* The first column shown of a table wider than the screen */
    /* 0x38 */ s8 tableFirstRow;    /* And the first row shown of one taller */
    /* 0x39 */ s8 unk39;
    /* 0x3A */ s8 unk3A;
    /* 0x3B */ s8 ringShotRow; /* The Ring Shot records' row: a course, or the replays after them */
    /* 0x3C */ s8 ringShotFirstCharacters[RING_SHOT_COURSE_COUNT]; /* Each course's first character shown */
    /* 0x42 */ s8 ringShotReplaySlot; /* The Ring Shot replay chosen */
    /* 0x43 */ s8 savedGameIndex;
    /* 0x44 */ u8 unk44[0x46 - 0x44];
    /* 0x46 */ u8 unk46;
    /* 0x47 */ s8 codeKeyColumn; /* The club tournament code keyboard's chosen key */
    /* 0x48 */ s8 codeKeyRow;
} MenuMemory;

MenuMemory *func_8005AF74(void);

#endif
