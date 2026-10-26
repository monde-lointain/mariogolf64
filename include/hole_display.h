#ifndef HOLE_DISPLAY_H
#define HOLE_DISPLAY_H

#include <ultra64.h>

/* SetMenuBackdrop's backdrops, and the styles that DrawGame draws them in */
#define MENU_BACKDROP_NONE (-1)
#define MENU_BACKDROP_STILL 0
#define MENU_BACKDROP_TINTED 1  /* The last frame in brown, read as intensity */
#define MENU_BACKDROP_DIMMING 2 /* The last frame, dimming to half brightness */
#define MENU_BACKDROP_PATTERN 3 /* A scrolling pattern */
#define MENU_BACKDROP_STYLE_NONE 0
#define MENU_BACKDROP_STYLE_PLAIN 1
#define MENU_BACKDROP_STYLE_UNK2 2

/* StartHudAnimation's animations, which DrawHudAnimation draws */
#define HUD_ANIMATION_NONE (-1)
#define HUD_ANIMATION_UNK0 0
#define HUD_ANIMATION_UNK1 1
#define HUD_ANIMATION_TURN_AWAY 2
#define HUD_ANIMATION_TILES 3
#define HUD_ANIMATION_SLICE_CAPTURE 4
#define HUD_ANIMATION_SLICES 5
#define HUD_ANIMATION_NEGATIVE 6 /* The frame with its colours inverted */
#define HUD_ANIMATION_UNK7 7
#define HUD_ANIMATION_UNK8 8
#define HUD_ANIMATION_UNK9 9
#define HUD_ANIMATION_SPIRAL 10
extern s32 g_MenuBackdropStyle;

void CaptureMenuBackdrop(Gfx **displayListHead, s32 unused);
void DrawMenuBackdrop(Gfx **displayListHead);
void DrawLensFlare(Gfx **displayListHead);
/* The 64x64 four-bit paletted image that MENU_BACKDROP_PATTERN tiles */
extern void *g_BackdropPatternImage;
/* How many slices of the frame HUD_ANIMATION_SLICE_CAPTURE has copied */
extern s32 g_CapturedSliceCount;

void SetUpTitleHole(void);
void UsePrimitiveDepth(s32 isPrimitiveDepthSet);
void SetMenuBackdrop(s32 backdrop);
void StartHudAnimation(s32 animation);
s32 IsHudAnimationDone(void);
void DrawHudAnimation(void);
void ResetHudScissor(void);

#endif
