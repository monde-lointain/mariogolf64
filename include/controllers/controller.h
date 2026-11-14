#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <nusys.h>

#include "system/c_language.h"

#define BUTTON_REPEAT_DELAY_FIRST 15
#define BUTTON_REPEAT_DELAY_NEXT 3

#define RUMBLE_FREQUENCY 0x100

#define EDITOR_CONTROLLER 0
#define OPTION_CONTROLLER 1
#define SECOND_EDITOR_CONTROLLER 2

#define DEBUG_CONTROLLER 1

typedef struct ControllerState {
    u16 buttons;
    u16 heldButtons;
    u16 pressedButtons;
    u16 previousButtons;
    u16 repeatedButtons;
    u16 repeatDelay;
    s8 stickX;
    s8 stickY;
} ControllerState;

typedef struct MenuRepeatState {
    s16 repeatDelay;
    s8 stickDirectionX;
    s8 stickDirectionY;
    u16 previousButtons;
} MenuRepeatState;

extern ControllerState g_ControllerStates[MAXCONTROLLERS];

extern NUContData g_ControllerData[MAXCONTROLLERS];
extern NUContData g_PreviousControllerData[MAXCONTROLLERS];

extern NUContData g_MenuControllerData[MAXCONTROLLERS];
extern MenuRepeatState g_MenuRepeatStates[MAXCONTROLLERS];

extern s32 g_ControllerCount;
extern s32 g_PlayerControllerPorts[MAXCONTROLLERS];
extern ControllerState g_TurnController;
extern ControllerState g_MergedController;
extern Boolean g_HasOwnControllers;

void ReadControllers(void);

#endif
