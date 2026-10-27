#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include "common.h"
#include "character.h"
#include "menu_memory.h"

/* The most modes the mode select offers for a player count */
#define MODE_CHOICE_LIMIT 12

extern u8 D_8022BEBC_ovl_01[MAX_PLAYER_COUNT][MODE_CHOICE_LIMIT];

/* The choices of both main menus; the one without Transmit stops at its options */
#define MAIN_CHOICE_CLUBHOUSE 0
#define MAIN_CHOICE_HELP 1
#define MAIN_CHOICE_OPTIONS 2
#define MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT 3
#define MAIN_CHOICE_CLUB_TOURNAMENT 2
#define MAIN_CHOICE_FULL_OPTIONS 3
#define MAIN_CHOICE_COUNT 4

/* The clubhouse's rows, and the rooms that D_8022BF1C gives each of them */
#define CLUBHOUSE_CHOICE_COUNT 5
#define CLUBHOUSE_ROOM_COURSE_STATS 0
#define CLUBHOUSE_ROOM_UNK12 1
#define CLUBHOUSE_ROOM_UNK13 2
#define CLUBHOUSE_ROOM_BIRDIE_BADGES 3
#define CLUBHOUSE_ROOM_CHARACTER_TABLE 4
#define CLUBHOUSE_ROOM_BEST_TIMES 5

extern s16 D_8022BF1C_ovl_01[CLUBHOUSE_CHOICE_COUNT];

/*
 * The menus set the top byte of g_MapCamera's view mode. Reading it as a
 * byte of the field rather than of the whole camera lets the compiler move
 * the read past display list words, as the original does.
 */
#define MAP_CAMERA_MODE_TOP_BYTE (*(u8 *)&g_MapCamera.view.mode)

/* The debug caddy select's two caddies */
#define CADDY_COUNT 2

/* The mini-golf course select's records, of three courses each */
#define MINI_GOLF_RECORD_COUNT 2

/* The chosen label grows two pixels a frame until it passes the limit, then shrinks until it passes 0 */
#define CHOSEN_LABEL_GROWTH_LIMIT 6
#define CHOSEN_LABEL_GROWTH_STEP 2
#define LABEL_GROWING 0
#define LABEL_SHRINKING 1

extern s16 D_8022BE8E_ovl_01;
extern s16 D_8022BE90_ovl_01;
extern void *D_8012D420[2];
extern void *D_801B5CD0[2];
extern s16 D_800FE33C;
extern u8 D_800FBDB9;

extern void *D_801B60D0;
extern s32 D_80105B68;
extern MenuMemory D_801323A0;
extern u8 D_8022BE88_ovl_01;
extern s32 D_8023BFD0_ovl_01;
extern u8 D_800FF4C0;
extern s32 D_801B7070;
extern s32 D_8012F480[MAX_PLAYER_COUNT];
extern s32 D_8022BE28_ovl_01;
extern char D_8022BF70_ovl_01[][IMPORTED_CHARACTER_NAME_LENGTH];
extern const char D_80231514_ovl_01[];

s32 GetOpenCourseCount(void);
void func_801FB54C_ovl_01(Gfx **displayListHead, s32);
void func_801FE4D0_ovl_01(Gfx **displayListHead);
void func_80201574_ovl_01(Gfx **displayListHead);
void func_80202000_ovl_01(Gfx **displayListHead);
void func_80205038_ovl_01(Gfx **displayListHead);
void func_802058B8_ovl_01(Gfx **displayListHead);
void func_80206180_ovl_01(Gfx **displayListHead);
void func_80206A68_ovl_01(Gfx **displayListHead);
void func_80207D08_ovl_01(Gfx **displayListHead);
void func_80207E18_ovl_01(Gfx **displayListHead);
void DrawBirdieBadgeLabel(Gfx **displayListHead);

#endif
