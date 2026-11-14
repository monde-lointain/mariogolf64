/*
 * The sky, the backdrops and the HUD animations that cover the whole frame.
 */

#ifndef SKY_AND_HUD_ANIMATIONS_H
#define SKY_AND_HUD_ANIMATIONS_H

#include <ultra64.h>

#include "camera/camera.h"
#include "math/math_util.h"
#include "screen/display_list.h"
#include "system/asset.h"
#include "system/c_language.h"

// SetMenuBackdrop's backdrops
enum MenuBackdrop {
    MENU_BACKDROP_NONE = -1,
    MENU_BACKDROP_STILL,
    // The last frame in brown, read as intensity
    MENU_BACKDROP_TINTED,
    // The last frame, dimming to half brightness
    MENU_BACKDROP_DIMMING,
    MENU_BACKDROP_PATTERN // A scrolling pattern
};

typedef s32 MenuBackdrop;

// Whether DrawGame draws a menu backdrop in place of the course
enum MenuBackdropState {
    MENU_BACKDROP_STATE_NONE,
    // A backdrop of the last frame waits for CaptureMenuBackdrop to capture it
    MENU_BACKDROP_STATE_CAPTURING,
    MENU_BACKDROP_STATE_SHOWN
};

typedef s32 MenuBackdropState;

// StartHudAnimation's animations, which DrawHudAnimation draws
enum HudAnimation {
    HUD_ANIMATION_NONE = -1,
    // Never started, only f_HudAnimation's first value
    HUD_ANIMATION_UNUSED,
    HUD_ANIMATION_FADE_OUT,
    HUD_ANIMATION_TURN_AWAY,
    HUD_ANIMATION_TILES,
    HUD_ANIMATION_SLICE_CAPTURE,
    HUD_ANIMATION_SLICES,
    HUD_ANIMATION_NEGATIVE, // The frame with its colors inverted
    HUD_ANIMATION_MOTION_BLUR,
    HUD_ANIMATION_SLIDE_APART,
    HUD_ANIMATION_FAST_FADE_OUT,
    HUD_ANIMATION_SPIRAL
};

typedef s32 HudAnimation;

typedef struct Fog {
    s32 color[RGB_CHANNEL_COUNT];
    s32 near;
    s32 far;
} Fog;

extern Fog g_SceneFog;
// The fog SetSceneFog draws while GAME_FLAG_SKY_EDITOR is set
extern Fog g_DebugFog;

// SkyColors' backgroundColor: red, green and blue, then its alpha, the percent
// of it that InitSkyColors mixes into g_CourseSkyImage
#define SKY_BACKGROUND_ALPHA 3
#define SKY_BACKGROUND_CHANNEL_COUNT 4

// The scene's colors at one time of day. The sky is shaded from its top
// color down to its horizon color, and a noise texture tinted with its noise
// color is either added to that gradient or blends the gradient toward the
// noise color. DeriveSceneColors also derives other colors from them.
typedef struct SkyColors {
    s32 addsNoise;
    u8 topColor[RGB_CHANNEL_COUNT];
    u8 horizonColor[RGB_CHANNEL_COUNT];
    u8 noiseColor[RGB_CHANNEL_COUNT];
    u8 backgroundColor[SKY_BACKGROUND_CHANNEL_COUNT];
    UBoolean8 hasLensFlare;
    struct Sun *sun;
    u8 fogColor[RGB_CHANNEL_COUNT];
    s16 fogNear;
    s16 fogFar;
} SkyColors;

// Each course has colors for each time of day, dry and in the rain
#define SKY_TIME_COUNT 4
#define SKY_WEATHER_COUNT 2

extern SkyColors g_SkyColors[][SKY_WEATHER_COUNT * SKY_TIME_COUNT];

// Sun.imageKind values: which images InitSkyColors loads
enum SkyImageKind {
    SKY_IMAGE_KIND_0, // ASSET_SKY_IMAGE_0
    SKY_IMAGE_KIND_1, // ASSET_SKY_IMAGE_1
    // The two halves of ASSET_SKY_IMAGE_2_*, which DrawSky draws as two textures
    SKY_IMAGE_KIND_TWO_TEXTURES
};

typedef u8 SkyImageKind;

// The sun of a sky, and which sky images InitSkyColors loads with it
typedef struct Sun {
    u8 unusedBeforeHeight[4];
    s32 height; // Above the horizon
    u8 primColor[RGB_CHANNEL_COUNT];
    u8 envColor[RGB_CHANNEL_COUNT];
    s32 radius;
    s16 unusedBeforeTexelStep;
    s16 texelStep;
    f32 glareMax; // The most that DrawLensFlare's glare grows to
    SkyImageKind imageKind;
} Sun;

extern MenuBackdropState g_MenuBackdropState;

// Images that InitSkyColors loads and recolors with the sky's colors
#define ASSET_SKY_IMAGE_0 0x4D4
#define ASSET_SKY_IMAGE_1 0x4D5
#define ASSET_SKY_IMAGE_2_FIRST_HALF 0x706
#define ASSET_SKY_IMAGE_2_SECOND_HALF 0x707
#define SKY_IMAGE_HALF_OFFSET 0x808
#define ASSET_COURSE_SKY_IMAGE_0 0x4BA
extern ImageAsset *g_SkyImage;
extern ImageAsset *g_CourseSkyImage;

// What UpdateSky works out about the main and inset cameras, for drawing the
// sky
#define SKY_VIEW_COUNT 2

typedef struct SkyState {
    f32 horizonLevels[SKY_VIEW_COUNT];
    f32 screenHeights[SKY_VIEW_COUNT]; // SCREEN_HEIGHT once UpdateSky sets them
    f32 lookAtLevels[SKY_VIEW_COUNT];
    Radians pitches[SKY_VIEW_COUNT];
    Radians yaws[SKY_VIEW_COUNT];
    f32 scrollX; // How far the sky's texture has scrolled with the wind
    f32 scrollY;
    f32 viewedWindAngles[SKY_VIEW_COUNT]; // The wind's direction as each camera sees it, in radians
} SkyState;

extern SkyState g_SkyState;

void CaptureMenuBackdrop(Gfx **displayListHead, s32 unused);
void DrawMenuBackdrop(Gfx **displayListHead);
void DrawLensFlare(Gfx **displayListHead);
// The 64x64 four-bit paletted image that MENU_BACKDROP_PATTERN tiles
extern ImageAsset *g_BackdropPatternImage;
// How many slices of the frame HUD_ANIMATION_SLICE_CAPTURE has copied
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
