#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <nusys.h>

/*
 * The controllers are read once per game frame. Besides NuSystem's own
 * readings, the game keeps two views of each controller in which the
 * analog stick also presses the directions of the control pad:
 *
 * - a ControllerState, for gameplay;
 * - menu readings, in which a held button repeats like a key on a
 *   keyboard, for moving through menus.
 */

/* A held button repeats first after this many frames... */
#define BUTTON_REPEAT_DELAY_FIRST 15
/* ...and then every this many frames */
#define BUTTON_REPEAT_DELAY_NEXT 3

typedef struct ControllerState {
  /* 0x0 */ u16 buttons; /* As read, without the stick */
  /* 0x2 */ u16 heldButtons; /* Including the stick's directions */
  /* 0x4 */ u16 pressedButtons; /* Pressed since the last frame */
  /* 0x6 */ u16 previousButtons; /* heldButtons of the last frame */
  /* 0x8 */ u16 repeatedButtons; /* heldButtons, but only when they repeat */
  /* 0xA */ u16 repeatDelay; /* Frames until heldButtons repeat */
  /* 0xC */ s8 stickX; /* Outside the dead zone, from 0 */
  /* 0xD */ s8 stickY;
} ControllerState; /* size = 0xE */

typedef struct MenuRepeatState {
  /* 0x0 */ s16 repeatDelay; /* Frames until the buttons repeat */
  /* 0x2 */ s8 stickDirectionX; /* -1, 0 or 1 */
  /* 0x3 */ s8 stickDirectionY;
  /* 0x4 */ u16 previousButtons;
} MenuRepeatState; /* size = 0x6 */

/* The game's view of the controller in each port */
extern ControllerState g_ControllerStates[MAXCONTROLLERS];

/* Readings of each port this frame and the last */
extern NUContData g_ControllerData[MAXCONTROLLERS];
extern NUContData g_PreviousControllerData[MAXCONTROLLERS];

/* Readings for menus, and the state that repeats their held inputs */
extern NUContData g_MenuControllerData[MAXCONTROLLERS];
extern MenuRepeatState g_MenuRepeatStates[MAXCONTROLLERS];

/* The players' controllers: how many there are and their ports */
extern s32 g_ControllerCount;
extern s32 g_PlayerControllerPorts[MAXCONTROLLERS];
/* The controller of the player whose turn it is, cleared for a computer player
 */
extern ControllerState g_TurnController;
/*
 * The buttons of every human player's controller ORed together, or the first
 * controller's if every player is a computer
 */
extern ControllerState g_MergedController;
extern s32 g_HasOwnControllers; /* Set unless two players share a controller */

void ReadControllers(void);

#endif
