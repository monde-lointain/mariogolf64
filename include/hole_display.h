#ifndef HOLE_DISPLAY_H
#define HOLE_DISPLAY_H

#include <ultra64.h>

/* SetMenuBackdrop's backdrops, and the styles that DrawGame draws them in */
#define MENU_BACKDROP_NONE (-1)
#define MENU_BACKDROP_STILL 0
/* The last frame in brown, read as intensity */
#define MENU_BACKDROP_TINTED 1
/* The last frame, dimming to half brightness */
#define MENU_BACKDROP_DIMMING 2
#define MENU_BACKDROP_PATTERN 3 /* A scrolling pattern */
#define MENU_BACKDROP_STYLE_NONE 0
#define MENU_BACKDROP_STYLE_PLAIN 1
#define MENU_BACKDROP_STYLE_UNK2 2

/* StartHudAnimation's animations, which DrawHudAnimation draws */
enum HudAnimation {
  HUD_ANIMATION_NONE = -1,
  HUD_ANIMATION_UNK0,
  HUD_ANIMATION_FADE_OUT,
  HUD_ANIMATION_TURN_AWAY,
  HUD_ANIMATION_TILES,
  HUD_ANIMATION_SLICE_CAPTURE,
  HUD_ANIMATION_SLICES,
  HUD_ANIMATION_NEGATIVE, /* The frame with its colours inverted */
  HUD_ANIMATION_MOTION_BLUR,
  HUD_ANIMATION_SLIDE_APART,
  HUD_ANIMATION_FAST_FADE_OUT,
  HUD_ANIMATION_SPIRAL
};

typedef struct Fog {
  /* 0x00 */ s32 color[3];
  /* 0x0C */ s32 near;
  /* 0x10 */ s32 far;
} Fog; /* size = 0x14 */

extern Fog g_SceneFog;
/* The fog SetSceneFog draws while GAME_FLAG_SKY_EDITOR is set */
extern Fog g_DebugFog;

/*
 * The scene's colors at one time of day. The sky's two colors go to its
 * vertices; DeriveSceneColors also derives other colors from them.
 */
typedef struct SkyColors {
  /* 0x00 */ s32 unk0; /* UpdateSkyEditor shows it as "CC" */
  /* 0x04 */ u8 unk4[3];
  /* 0x07 */ u8 unk7[3];
  /* 0x0A */ u8 unkA[3];
  /* 0x0D */ u8 unkD[3];
  /* 0x10 */ u8 unk10; /* Percent of unkD that InitSkyColors mixes into
                          g_CourseSkyImage */
  /* 0x11 */ u8 hasLensFlare;
  /* 0x12 */ u8 unk12[2];
  /* 0x14 */ struct Unk800C5F34* unk14;
  /* 0x18 */ u8 fogColor[3];
  /* 0x1C */ s16 fogNear;
  /* 0x1E */ s16 fogFar;
} SkyColors; /* size = 0x20 */

/* Each course has colors for each time of day, dry and in the rain */
#define SKY_TIME_COUNT 4
#define SKY_WEATHER_COUNT 2

extern SkyColors g_SkyColors[][SKY_WEATHER_COUNT * SKY_TIME_COUNT];

typedef struct Unk800C5F34 {
  /* 0x00 */ u8 unk0[4];
  /* 0x04 */ s32 sunHeight; /* Above the horizon */
  /* 0x08 */ u8 sunPrimColor[3];
  /* 0x0B */ u8 sunEnvColor[3];
  /* 0x0E */ u8 unkE[2];
  /* 0x10 */ s32 sunRadius;
  /* 0x14 */ s16 unk14;
  /* 0x16 */ s16 sunTexelStep;
  /* 0x18 */ f32 maxSunGlare; /* The most that DrawLensFlare's glare grows to */
  /* 0x1C */ u8 imageKind; /* A SKY_IMAGE_KIND_* value */
} Unk800C5F34;

extern s32 g_MenuBackdropStyle;

void CaptureMenuBackdrop(Gfx** displayListHead, s32 unused);
void DrawMenuBackdrop(Gfx** displayListHead);
void DrawLensFlare(Gfx** displayListHead);
/* The 64x64 four-bit paletted image that MENU_BACKDROP_PATTERN tiles */
extern void* g_BackdropPatternImage;
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
