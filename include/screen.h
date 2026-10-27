#ifndef SCREEN_H
#define SCREEN_H

#include <ultra64.h>

/* Screen sizes, in pixels */
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

/* The border around the main camera's scissor rectangle, in pixels */
#define SCREEN_BORDER 8

/* A texture rectangle step of one texel per pixel, in 5.10 fixed point */
#define TEXEL_STEP_ONE (1 << 10)

/* SetScreenEffect's effects */
#define SCREEN_EFFECT_SPIN 0 /* Spins the saved frame away */
#define SCREEN_EFFECT_EMPTY 1 /* Draws nothing */
#define SCREEN_EFFECT_TILED_IMAGE 2 /* Repeats part of the saved frame */
#define SCREEN_EFFECT_SAVED_FRAME 3 /* Draws a frame saved elsewhere */
#define SCREEN_EFFECT_NONE 4
extern s32 g_ScreenEffect; /* The effect SetScreenEffect last set */

/* An orthographic projection of one unit per pixel, centred on the screen, with
 * y rising */
extern Mtx g_ScreenOrthoMatrix;

/* A rectangle of the screen, in pixels */
typedef struct ScreenRect {
  /* 0x00 */ s16 left;
  /* 0x02 */ s16 top;
  /* 0x04 */ s16 right;
  /* 0x06 */ s16 bottom;
} ScreenRect; /* size = 0x8 */

/* The rectangle that the HUD is drawn within */
extern ScreenRect g_HudScissor;

void SetScreenEffect(s32 effect, s32 frameCount);

#endif
