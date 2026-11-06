#ifndef HOLE_DISPLAY_H
#define HOLE_DISPLAY_H

#include <ultra64.h>

#include "camera.h"
#include "language.h"
#include "math_util.h"

/* SetMenuBackdrop's backdrops */
enum MenuBackdrop {
  MENU_BACKDROP_NONE = -1,
  MENU_BACKDROP_STILL,
  /* The last frame in brown, read as intensity */
  MENU_BACKDROP_TINTED,
  /* The last frame, dimming to half brightness */
  MENU_BACKDROP_DIMMING,
  MENU_BACKDROP_PATTERN /* A scrolling pattern */
};

/* Whether DrawGame draws a menu backdrop in place of the course */
enum MenuBackdropState {
  MENU_BACKDROP_STATE_NONE,
  /* A backdrop of the last frame waits for CaptureMenuBackdrop to capture it */
  MENU_BACKDROP_STATE_CAPTURING,
  MENU_BACKDROP_STATE_SHOWN
};

/* StartHudAnimation's animations, which DrawHudAnimation draws */
enum HudAnimation {
  HUD_ANIMATION_NONE = -1,
  /* Never started, only f_HudAnimation's first value */
  HUD_ANIMATION_UNUSED,
  HUD_ANIMATION_FADE_OUT,
  HUD_ANIMATION_TURN_AWAY,
  HUD_ANIMATION_TILES,
  HUD_ANIMATION_SLICE_CAPTURE,
  HUD_ANIMATION_SLICES,
  HUD_ANIMATION_NEGATIVE, /* The frame with its colors inverted */
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
 * SkyColors' backgroundColor: red, green and blue, then its alpha, the percent
 * of it that InitSkyColors mixes into g_CourseSkyImage
 */
#define SKY_BACKGROUND_ALPHA 3
#define SKY_BACKGROUND_CHANNEL_COUNT 4

/*
 * The scene's colors at one time of day. The sky is shaded from its top
 * color down to its horizon color, and a noise texture tinted with its noise
 * color is either added to that gradient or blends the gradient toward the
 * noise color. DeriveSceneColors also derives other colors from them.
 */
typedef struct SkyColors {
  /* 0x00 */ s32 addsNoise;
  /* 0x04 */ u8 topColor[3];
  /* 0x07 */ u8 horizonColor[3];
  /* 0x0A */ u8 noiseColor[3];
  /* 0x0D */ u8 backgroundColor[SKY_BACKGROUND_CHANNEL_COUNT];
  /* 0x11 */ u8 hasLensFlare;
  /* 0x14 */ struct Sun* sun;
  /* 0x18 */ u8 fogColor[3];
  /* 0x1C */ s16 fogNear;
  /* 0x1E */ s16 fogFar;
} SkyColors; /* size = 0x20 */

/* Each course has colors for each time of day, dry and in the rain */
#define SKY_TIME_COUNT 4
#define SKY_WEATHER_COUNT 2

extern SkyColors g_SkyColors[][SKY_WEATHER_COUNT * SKY_TIME_COUNT];

/* The sun of a sky, and which sky images InitSkyColors loads with it */
typedef struct Sun {
  /* 0x00 */ u8 unusedBeforeHeight[4];
  /* 0x04 */ s32 height; /* Above the horizon */
  /* 0x08 */ u8 primColor[3];
  /* 0x0B */ u8 envColor[3];
  /* 0x10 */ s32 radius;
  /* 0x14 */ s16 unusedBeforeTexelStep;
  /* 0x16 */ s16 texelStep;
  /* 0x18 */ f32 maxGlare; /* The most that DrawLensFlare's glare grows to */
  /* 0x1C */ u8 imageKind; /* A SKY_IMAGE_KIND_* value */
} Sun;

/* A MENU_BACKDROP_STATE_* value */
extern s32 g_MenuBackdropState;

/* Images that InitSkyColors loads and recolors with the sky's colors */
#define ASSET_SKY_IMAGE_0 0x4D4
#define ASSET_SKY_IMAGE_1 0x4D5
#define ASSET_SKY_IMAGE_2_FIRST_HALF 0x706
#define ASSET_SKY_IMAGE_2_SECOND_HALF 0x707
#define SKY_IMAGE_HALF_OFFSET 0x808
#define ASSET_COURSE_SKY_IMAGE_0 0x4BA
extern u8* g_SkyImage;
extern u8* g_CourseSkyImage;

/* What UpdateSky works out about the main and inset cameras, for drawing the
 * sky */
#define SKY_VIEW_COUNT 2

typedef struct SkyState {
  /* 0x00 */ f32 horizonLevels[SKY_VIEW_COUNT];
  /* 0x08 */ f32 screenHeights[SKY_VIEW_COUNT]; /* SCREEN_HEIGHT once UpdateSky
                                                   sets them */
  /* 0x10 */ f32 lookAtLevels[SKY_VIEW_COUNT];
  /* 0x18 */ Radians pitches[SKY_VIEW_COUNT];
  /* 0x20 */ Radians yaws[SKY_VIEW_COUNT];
  /* 0x28 */ f32
      scrollX; /* How far the sky's texture has scrolled with the wind */
  /* 0x2C */ f32 scrollY;
  /* 0x30 */ f32
      viewedWindAngles[SKY_VIEW_COUNT]; /* The wind's direction as each camera
                                           sees it, in radians */
} SkyState; /* size = 0x38 */

extern SkyState g_SkyState;

void CaptureMenuBackdrop(Gfx** displayListHead, s32 unused);
void DrawMenuBackdrop(Gfx** displayListHead);
void DrawLensFlare(Gfx** displayListHead);
/* The 64x64 four-bit paletted image that MENU_BACKDROP_PATTERN tiles */
extern void* g_BackdropPatternImage;
/* How many slices of the frame HUD_ANIMATION_SLICE_CAPTURE has copied */
extern s32 g_CapturedSliceCount;

void SetUpTitleHole(void);
void UsePrimitiveDepth(Boolean isPrimitiveDepthSet);
void SetMenuBackdrop(s32 backdrop);
void StartHudAnimation(s32 animation);
Boolean IsHudAnimationDone(void);
void DrawHudAnimation(void);
void ResetHudScissor(void);
void InitSkyColors(void);

void DrawBackdrop(Gfx** displayListHead, CameraView* view, s32 cameraIndex);

#endif
