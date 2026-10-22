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

#endif
