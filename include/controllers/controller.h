/*
 * The controllers are read once per game frame. Besides NuSystem's own
 * readings, the game keeps two views of each controller in which the
 * analog stick also presses the directions of the control pad:
 *
 * - a ControllerState, for gameplay;
 * - menu readings, in which a held button repeats like a key on a
 * keyboard, for moving through menus.
 */

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <nusys.h>

#include "system/c_language.h"

// A held button repeats first after this many frames...
#define BUTTON_REPEAT_DELAY_FIRST 15
// ...and then every this many frames
#define BUTTON_REPEAT_DELAY_NEXT 3
#define RUMBLE_FREQUENCY 0x100

// The controllers that the debug tools read
#define EDITOR_CONTROLLER 0
#define OPTION_CONTROLLER 1
#define SECOND_EDITOR_CONTROLLER 2
#define DEBUG_CONTROLLER 1

typedef struct ControllerState {
    u16 buttons; // As read, without the stick
    u16 heldButtons; // Including the stick's directions
    u16 pressedButtons; // Pressed since the last frame
    u16 previousButtons; // heldButtons of the last frame
    u16 repeatedButtons; // heldButtons, but only when they repeat
    u16 repeatDelay; // Frames until heldButtons repeat
    s8 stickX; // Outside the dead zone, from 0
    s8 stickY;
} ControllerState;

typedef struct MenuRepeatState {
    s16 repeatDelay; // Frames until the buttons repeat
    s8 stickDirectionX; // -1, 0 or 1
    s8 stickDirectionY;
    u16 previousButtons;
} MenuRepeatState;

// The game's view of the controller in each port
extern ControllerState g_ControllerStates[MAXCONTROLLERS];

// Readings of each port this frame and the last
extern NUContData g_ControllerData[MAXCONTROLLERS];
extern NUContData g_PreviousControllerData[MAXCONTROLLERS];

// Readings for menus, and the state that repeats their held inputs
extern NUContData g_MenuControllerData[MAXCONTROLLERS];
extern MenuRepeatState g_MenuRepeatStates[MAXCONTROLLERS];

// The players' controllers: how many there are and their ports
extern s32 g_ControllerCount;
extern s32 g_PlayerControllerPorts[MAXCONTROLLERS];
// The controller of the player whose turn it is, cleared for a computer player
extern ControllerState g_TurnController;
// The buttons of every human player's controller ORed together, or the first
// controller's if every player is a computer
extern ControllerState g_MergedController;
extern Boolean g_HasOwnControllers; // Set unless two players share a controller

void ReadControllers(void);

#endif
