#ifndef SAVE_MENU_H
#define SAVE_MENU_H

#include <ultra64.h>

#include "controller.h"

/* The pause screens' save menu and Controls screen */

void DrawControlsScreen(Gfx** displayListHead);
void DrawGameSaveMenu(Gfx** displayListHead);
void DrawReplaySaveMenu(Gfx** displayListHead);
void LeaveReplaySaveMenu(void);
s32 UpdateReplaySaveMenu(ControllerState* controller);

#endif
