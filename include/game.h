#ifndef GAME_H
#define GAME_H

#include <ultra64.h>

/* The globals that game.c, the game's frame, shares */

/* Values of g_FramePacingMode, which the second controller's Start cycles */
#define PACING_MODE_NORMAL 0
#define PACING_MODE_PERFORMANCE_METER 1 /* The other modes print debug text */
#define PACING_MODE_COUNT 4
extern s8 g_FramePacingMode;

/* Which of DRAW_BUFFER_COUNT sets of per-frame matrices and vertices the frame
 * builds */
extern s32 g_DrawBufferIndex;

/*
 * The screen row of the horizon in the main view, from its pitch and zoom.
 * The background drawer sets it.
 */
extern s32 g_HorizonRow;

/*
 * Viewports in pairs, one per draw buffer. The first pair covers the
 * screen; the others start over its bottom-right quarter, and UpdateGame
 * fits the second pair to the inset view every frame.
 */
#define VIEWPORT_FULL_SCREEN 0
#define VIEWPORT_INSET 2
#define VIEWPORT_STAFF_ROLL 4
#define VIEWPORT_COUNT 6
extern Vp g_Viewports[VIEWPORT_COUNT];

#endif
