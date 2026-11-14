/*
 * The globals that game.c, the game's frame, shares
 */

#ifndef GAME_H
#define GAME_H

#include <ultra64.h>

#include "camera/camera.h"

// Values of g_FramePacingMode, which the second controller's Start cycles
enum PacingMode {
    PACING_MODE_NORMAL,
    PACING_MODE_PERFORMANCE_METER // The other modes print debug text
};

typedef s8 PacingMode;

#define PACING_MODE_COUNT 4
extern PacingMode g_FramePacingMode;

// Which of DRAW_BUFFER_COUNT sets of per-frame matrices and vertices the frame
// builds
extern s32 g_DrawBufferIndex;

// The screen row of the horizon in the main view, from its pitch and zoom.
// The background drawer sets it.
extern s32 g_HorizonRow;

// Viewports in pairs, one per draw buffer. The first pair covers the
// screen; the others start over its bottom-right quarter, and UpdateGame
// fits the second pair to the inset view every frame.
enum ViewportKind {
    VIEWPORT_FULL_SCREEN,
    VIEWPORT_INSET = 2,
    VIEWPORT_STAFF_ROLL = 4
};

#define VIEWPORT_COUNT 6
extern Vp g_Viewports[VIEWPORT_COUNT];

// Toggled by a debug button; nothing reads it
extern Boolean g_UnreadDebugSwitch;

// The next free command of the display list being built this frame
extern Gfx *g_DisplayListHead;

// The depth buffer, which the screen effects also use to hold a saved frame
extern u16 *g_DepthBuffer;

#define DRAW_BUFFER_COUNT 2

// A 45-degree perspective projection for the screen effects
extern Mtx g_ScreenEffectProjection;
extern u16 g_ScreenEffectPerspectiveNormalization;

// The display lists, one per draw buffer
#define DISPLAY_LIST_LENGTH 14000

// Textures that are drawn every frame and that the course model reads
// through RSP segments, each DYNAMIC_TEXTURE_SIZE pixels square
#define DYNAMIC_TEXTURE_SIZE 32
// Part of the frame, upside down
extern u16 g_MirrorTexture[DYNAMIC_TEXTURE_SIZE * DYNAMIC_TEXTURE_SIZE];
// Noise scrolling over a flame gradient
extern u16 g_FlameTexture[DYNAMIC_TEXTURE_SIZE * DYNAMIC_TEXTURE_SIZE];
// Two loaded textures, one scrolling
extern u16 g_BlendedTexture[DYNAMIC_TEXTURE_SIZE * DYNAMIC_TEXTURE_SIZE];

// The texture memory that the second texture of a two-texture blend uses
#define SECOND_TEXTURE_TMEM 0x100

// The loaded textures that g_BlendedTexture blends, after 8-byte headers
extern u8 *g_BlendedTextureScrollingLayer;
extern u8 *g_BlendedTextureStillLayer;

void StartGame(void);
void UpdateGame(void);
s32 DrawGame(void);
void DrawResetFade(void);
// Draws the textures the course model reads through RSP segments
void BuildDynamicTextures(Gfx **displayListHead);

#endif
