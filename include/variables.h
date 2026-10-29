#ifndef VARIABLES_H
#define VARIABLES_H

#include <ultra64.h>

/* Globals whose defining file is not decompiled yet */

/* libmus plays silence instead of the audio while this is set */
extern u8 __libmus_output_muted;

/* The next free command of the display list being built this frame */
extern Gfx* g_DisplayListHead;

/* The depth buffer, which the screen effects also use to hold a saved frame */
extern u16* g_DepthBuffer;

/* NuSystem's buffers, declared in lib/libnusys/nusys.h */
extern u16* nuGfxCfb_ptr;
extern u16* nuGfxZBuffer;

#define DRAW_BUFFER_COUNT 2

/* The scene light's direction, in binary angles */

/* A 45-degree perspective projection for the screen effects */
extern Mtx g_ScreenEffectProjection;
extern u16 g_ScreenEffectPerspNorm;

/* The display lists, one per draw buffer */
#define DISPLAY_LIST_LENGTH 14000
extern Gfx g_DisplayLists[2][DISPLAY_LIST_LENGTH];

/* The weather and light that a hole is played in */
typedef struct HoleConditions {
  /* 0x00 */ s32 windSpeed; /* As the ball's flight feels it */
  /* 0x04 */ u16 windDirection; /* A full turn is 0x10000 */
  /* Shortens shots and changes the scene's colors */
  /* 0x08 */ Boolean isRaining;
  /* 0x0C */ u32
      sceneColorIndex; /* With isRaining, picks DeriveSceneColors' colors */
  /* 0x10 */ u16 lightYaw;
  /* 0x12 */ u16 lightPitch;
} HoleConditions; /* size = 0x14 */

/* Which nines the round leaves out, as *_NINE_SKIPPED flags */
extern s32 g_SkippedNines;
#define FRONT_NINE_SKIPPED 0x2
#define BACK_NINE_SKIPPED 0x4

/* The game counts time in frames */
#define FRAMES_PER_SECOND 30
#define SECONDS_PER_MINUTE 60
#define HUNDREDTHS_PER_SECOND 100

/* Images that InitSkyColors loads and recolors with the sky's colors */
#define ASSET_SKY_IMAGE_0 0x4D4
#define ASSET_SKY_IMAGE_1 0x4D5
#define ASSET_SKY_IMAGE_2_FIRST_HALF 0x706
#define ASSET_SKY_IMAGE_2_SECOND_HALF 0x707
#define SKY_IMAGE_HALF_OFFSET 0x808
#define ASSET_COURSE_SKY_IMAGE_0 0x4BA
extern u8* g_SkyImage;
extern u8* g_CourseSkyImage;

/* HoleConditions.sceneColorIndex values; over a round it rises from start to
 * end */
#define SCENE_COLOR_INDEX_ROUND_START 0
#define SCENE_COLOR_INDEX_DRIVING_RANGE 6
#define SCENE_COLOR_INDEX_UNK8 8
#define SCENE_COLOR_INDEX_ROUND_LATE 9
#define SCENE_COLOR_INDEX_ROUND_END 10
/* Every hole of a sudden death, whatever its place in the round */
#define SCENE_COLOR_INDEX_SUDDEN_DEATH 11

/* HoleConditions.windSpeed is fixed point, with this as one unit */
#define WIND_SPEED_UNIT 0x10000

/* What UpdateSky works out about the main and inset cameras, for drawing the
 * sky */
#define SKY_VIEW_COUNT 2

typedef struct SkyState {
  /* 0x00 */ f32 horizonLevels[SKY_VIEW_COUNT];
  /* 0x08 */ f32 screenHeights[SKY_VIEW_COUNT]; /* SCREEN_HEIGHT once UpdateSky
                                                   sets them */
  /* 0x10 */ f32 lookAtLevels[SKY_VIEW_COUNT];
  /* 0x18 */ f32 pitches[SKY_VIEW_COUNT]; /* In radians */
  /* 0x20 */ f32 yaws[SKY_VIEW_COUNT];
  /* 0x28 */ f32
      scrollX; /* How far the sky's texture has scrolled with the wind */
  /* 0x2C */ f32 scrollY;
  /* 0x30 */ f32
      viewedWindAngles[SKY_VIEW_COUNT]; /* The wind's direction as each camera
                                           sees it, in radians */
} SkyState; /* size = 0x38 */

extern SkyState g_SkyState;

/* HoleConditions.windDirection's units per radian */
#define WIND_DIRECTION_TO_RADIANS ((f32)(2 * M_PI / 0x10000))

/*
 * The wind's direction as seen from a camera: the arrow turns with the
 * camera's direction of view
 */
#define VIEWED_WIND_ANGLE(windDirection, view) \
  (((windDirection) * WIND_DIRECTION_TO_RADIANS) + M_PI - \
      atan2((view)->eye[2] - (view)->lookAt[2], \
          (view)->eye[0] - (view)->lookAt[0]))

/*
 * Textures that are drawn every frame and that the course model reads
 * through RSP segments, each DYNAMIC_TEXTURE_SIZE pixels square
 */
#define DYNAMIC_TEXTURE_SIZE 32
extern u16 g_MirrorTexture[]; /* Part of the frame, upside down */
extern u16 g_FlameTexture[]; /* Noise scrolling over a flame gradient */
extern u16 g_BlendedTexture[]; /* Two loaded textures, one scrolling */

/* The texture memory that the second texture of a two-texture blend uses */
#define SECOND_TEXTURE_TMEM 0x100

/* The loaded textures that g_BlendedTexture blends, after 8-byte headers */
extern u8* g_BlendedTextureScrollingLayer;
extern u8* g_BlendedTextureStillLayer;

#define NOISE_GRID_SIZE 64
/* An I8 texture of NOISE_GRID_SIZE x NOISE_GRID_SIZE texels */
extern u8* g_NoiseTexels;

/* Formatted text on its way to the screen */
extern char g_PrintBuffer[];

#endif
