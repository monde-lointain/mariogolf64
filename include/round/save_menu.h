/*
 * The save menu and the Controls screen of the pause menu.
 */

#ifndef SAVE_MENU_H
#define SAVE_MENU_H

#include <ultra64.h>

#include "controllers/controller.h"

// What UpdateReplaySaveMenu returns
enum ReplayMenuState {
    REPLAY_MENU_OPEN,
    REPLAY_MENU_CLOSED,
    REPLAY_MENU_PROMPTING
};

typedef s32 ReplayMenuState;

// The pause screens' save menu and Controls screen

void DrawControlsScreen(Gfx **displayListHead);
void DrawGameSaveMenu(Gfx **displayListHead);
void DrawReplaySaveMenu(Gfx **displayListHead);
void LeaveReplaySaveMenu(void);
ReplayMenuState UpdateReplaySaveMenu(ControllerState *controller);

#endif
