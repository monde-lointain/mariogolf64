#ifndef HOLE_DISPLAY_H
#define HOLE_DISPLAY_H

#include <ultra64.h>

#include "asset.h"
#include "camera.h"
#include "language.h"
#include "math_util.h"

enum MenuBackdrop {
    MENU_BACKDROP_NONE = -1,
    MENU_BACKDROP_STILL,
    MENU_BACKDROP_TINTED,
    MENU_BACKDROP_DIMMING,
    MENU_BACKDROP_PATTERN
};

typedef s32 MenuBackdrop;

enum MenuBackdropState {
    MENU_BACKDROP_STATE_NONE,
    MENU_BACKDROP_STATE_CAPTURING,
    MENU_BACKDROP_STATE_SHOWN
};

typedef s32 MenuBackdropState;

enum HudAnimation {
    HUD_ANIMATION_NONE = -1,
    HUD_ANIMATION_UNUSED,
    HUD_ANIMATION_FADE_OUT,
    HUD_ANIMATION_TURN_AWAY,
    HUD_ANIMATION_TILES,
    HUD_ANIMATION_SLICE_CAPTURE,
    HUD_ANIMATION_SLICES,
    HUD_ANIMATION_NEGATIVE,
    HUD_ANIMATION_MOTION_BLUR,
    HUD_ANIMATION_SLIDE_APART,
    HUD_ANIMATION_FAST_FADE_OUT,
    HUD_ANIMATION_SPIRAL
};

typedef s32 HudAnimation;

typedef struct Fog {
    s32 color[3];
    s32 near;
    s32 far;
} Fog;

extern Fog g_SceneFog;
extern Fog g_DebugFog;

#define SKY_BACKGROUND_ALPHA 3
#define SKY_BACKGROUND_CHANNEL_COUNT 4

typedef struct SkyColors {
    s32 addsNoise;
    u8 topColor[3];
    u8 horizonColor[3];
    u8 noiseColor[3];
    u8 backgroundColor[SKY_BACKGROUND_CHANNEL_COUNT];
    UBoolean8 hasLensFlare;
    struct Sun *sun;
    u8 fogColor[3];
    s16 fogNear;
    s16 fogFar;
} SkyColors;

#define SKY_TIME_COUNT 4
#define SKY_WEATHER_COUNT 2

extern SkyColors g_SkyColors[][SKY_WEATHER_COUNT * SKY_TIME_COUNT];

enum SkyImageKind {
    SKY_IMAGE_KIND_0,
    SKY_IMAGE_KIND_1,
    SKY_IMAGE_KIND_TWO_TEXTURES
};

typedef u8 SkyImageKind;

typedef struct Sun {
    u8 unusedBeforeHeight[4];
    s32 height;
    u8 primColor[3];
    u8 envColor[3];
    s32 radius;
    s16 unusedBeforeTexelStep;
    s16 texelStep;
    f32 glareMax;
    SkyImageKind imageKind;
} Sun;

extern MenuBackdropState g_MenuBackdropState;

#define ASSET_SKY_IMAGE_0 0x4D4
#define ASSET_SKY_IMAGE_1 0x4D5
#define ASSET_SKY_IMAGE_2_FIRST_HALF 0x706
#define ASSET_SKY_IMAGE_2_SECOND_HALF 0x707
#define SKY_IMAGE_HALF_OFFSET 0x808
#define ASSET_COURSE_SKY_IMAGE_0 0x4BA
extern ImageAsset *g_SkyImage;
extern ImageAsset *g_CourseSkyImage;

#define SKY_VIEW_COUNT 2

typedef struct SkyState {
    f32 horizonLevels[SKY_VIEW_COUNT];
    f32 screenHeights[SKY_VIEW_COUNT];
    f32 lookAtLevels[SKY_VIEW_COUNT];
    Radians pitches[SKY_VIEW_COUNT];
    Radians yaws[SKY_VIEW_COUNT];
    f32 scrollX;
    f32 scrollY;
    f32 viewedWindAngles[SKY_VIEW_COUNT];
} SkyState;

extern SkyState g_SkyState;

void CaptureMenuBackdrop(Gfx **displayListHead, s32 unused);
void DrawMenuBackdrop(Gfx **displayListHead);
void DrawLensFlare(Gfx **displayListHead);
extern ImageAsset *g_BackdropPatternImage;
extern s32 g_CapturedSliceCount;

void SetUpTitleHole(void);
void UsePrimitiveDepth(Boolean isPrimitiveDepthSet);
void SetMenuBackdrop(MenuBackdrop backdrop);
void StartHudAnimation(HudAnimation animationId);
Boolean IsHudAnimationDone(void);
void DrawHudAnimation(void);
void ResetHudScissor(void);
void InitSkyColors(void);

void DrawBackdrop(Gfx **displayListHead, CameraView *view, s32 cameraIndex);

#endif
