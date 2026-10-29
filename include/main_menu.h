#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include "common.h"

#include "character.h"
#include "menu_memory.h"

/*
 * The menus set the top byte of g_MapCamera's view mode. Reading it as a
 * byte of the field rather than of the whole camera lets the compiler move
 * the read past display list words, as the original does.
 */
#define MAP_CAMERA_MODE_TOP_BYTE (*(u8*)&g_MapCamera.view.mode)

extern void* D_8012D420[2];
extern void* D_801B5CD0[2];
extern s16 D_800FE33C;
extern u8 D_800FBDB9;

extern void* D_801B60D0;
/* The player count chosen on the title screen, which the saved games, the
 * menus and the club tournament set to 1 */
extern s32 g_ChosenPlayerCount;
extern MenuMemory D_801323A0;
extern u8 D_8022BE88_ovl_01;
extern u8 D_8022BE89_ovl_01;
extern s32 D_8022F8C0_ovl_01;
extern u8 D_8022F8C4_ovl_01;
extern s8 D_8022F8C5_ovl_01;
extern s32 D_8023BFD0_ovl_01;
extern u8 D_800FF4C0;
extern s32 D_801B7070;
extern s32 D_8012F480[MAX_PLAYER_COUNT];
extern char D_8022BF70_ovl_01[CHARACTER_COUNT][IMPORTED_CHARACTER_NAME_LENGTH];

s32 GetOpenCourseCount(void);
void func_801F6A38_ovl_01(void);
void func_801FB54C_ovl_01(Gfx** displayListHead, s32);

#endif
