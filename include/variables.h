#ifndef VARIABLES_H
#define VARIABLES_H

#include <ultra64.h>

/* Globals whose defining file is not decompiled yet */

extern s8 g_IsDebugEnabled;
/* Frames the game has drawn since boot */
extern volatile s32 g_FrameCount;

/* libmus plays silence instead of the audio while this is set */
extern u8 __libmus_output_muted;

/* The next free command of the display list being built this frame */
extern Gfx *g_DisplayListHead;

/* The ball's position, in units of 1/1024 */
extern s32 g_BallPosition[3];

/* The audio channel for the next sound effect, alternating between two */
extern s32 g_NextSoundChannel;

/* The depth buffer, which the screen effects also use to hold a saved frame */
extern u16 *g_DepthBuffer;

/* NuSystem's buffers, declared in lib/libnusys/nusys.h */
extern u16 *nuGfxCfb_ptr;
extern u16 *nuGfxZBuffer;

/* Which of two sets of per-frame matrices and vertices the frame builds */
extern s32 g_DrawBufferIndex;

/* A 45-degree perspective projection for the screen effects */
extern Mtx g_ScreenEffectProjection;

/* The display lists, one per draw buffer */
#define DISPLAY_LIST_LENGTH 14000
extern Gfx g_DisplayLists[2][DISPLAY_LIST_LENGTH];

/* The wind, as the ball's flight feels it. A full turn is 0x10000. */
extern s32 g_WindSpeed;
extern u16 g_WindDirection;

/* The wind's direction as each camera sees it, in radians, for its arrow */
extern f32 g_ViewedWindAngles[2];

/* Formatted text on its way to the screen */
extern char g_PrintBuffer[];

/*
 * Viewports in pairs, one per draw buffer. The first pair covers the
 * screen; the others start over its bottom-right quarter, and UpdateGame
 * fits the second pair to the inset view every frame.
 */
#define VIEWPORT_FULL_SCREEN 0
#define VIEWPORT_INSET 2
extern Vp g_Viewports[];

#endif
