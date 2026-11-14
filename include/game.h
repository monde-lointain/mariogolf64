#ifndef GAME_H
#define GAME_H

#include <ultra64.h>

#include "camera.h"

enum PacingMode {
    PACING_MODE_NORMAL,
    PACING_MODE_PERFORMANCE_METER
};

typedef s8 PacingMode;

#define PACING_MODE_COUNT 4
extern PacingMode g_FramePacingMode;

extern s32 g_DrawBufferIndex;

extern s32 g_HorizonRow;

enum ViewportKind {
    VIEWPORT_FULL_SCREEN,
    VIEWPORT_INSET = 2,
    VIEWPORT_STAFF_ROLL = 4
};

#define VIEWPORT_COUNT 6
extern Vp g_Viewports[VIEWPORT_COUNT];

extern Boolean g_UnreadDebugSwitch;

extern Gfx *g_DisplayListHead;

extern u16 *g_DepthBuffer;

#define DRAW_BUFFER_COUNT 2

extern Mtx g_ScreenEffectProjection;
extern u16 g_ScreenEffectPerspectiveNormalization;

#define DISPLAY_LIST_LENGTH 14000

#define DYNAMIC_TEXTURE_SIZE 32
extern u16 g_MirrorTexture[DYNAMIC_TEXTURE_SIZE * DYNAMIC_TEXTURE_SIZE];
extern u16 g_FlameTexture[DYNAMIC_TEXTURE_SIZE * DYNAMIC_TEXTURE_SIZE];
extern u16 g_BlendedTexture[DYNAMIC_TEXTURE_SIZE * DYNAMIC_TEXTURE_SIZE];

#define SECOND_TEXTURE_TMEM 0x100

extern u8 *g_BlendedTextureScrollingLayer;
extern u8 *g_BlendedTextureStillLayer;

void StartGame(void);
void UpdateGame(void);
s32 DrawGame(void);
void DrawResetFade(void);
void BuildDynamicTextures(Gfx **displayListHead);

#endif
