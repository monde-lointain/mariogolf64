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

/* The depth buffer, which the screen effects also use to hold a saved frame */
extern u16 *g_DepthBuffer;

/* NuSystem's buffers, declared in lib/libnusys/nusys.h */
extern u16 *nuGfxCfb_ptr;
extern u16 *nuGfxZBuffer;

#define DRAW_BUFFER_COUNT 2

/* Which of DRAW_BUFFER_COUNT sets of per-frame matrices and vertices the frame builds */
extern s32 g_DrawBufferIndex;

/* The scene light's direction, in binary angles */
/* Where the ball is, in course units */
extern s32 g_BallPosition[3];
/* The SURFACE_* that the ball is on */
extern s8 g_BallSurface;
extern u16 g_LightYaw;
extern u16 g_LightPitch;

/* A 45-degree perspective projection for the screen effects */
extern Mtx g_ScreenEffectProjection;
extern u16 g_ScreenEffectPerspNorm;
/* Which of the two matrix buffers this frame fills */
extern s32 g_MatrixBufferIndex;

/* The display lists, one per draw buffer */
#define DISPLAY_LIST_LENGTH 14000
extern Gfx g_DisplayLists[2][DISPLAY_LIST_LENGTH];

/* The weather and light that a hole is played in */
typedef struct HoleConditions {
    /* 0x00 */ s32 windSpeed;      /* As the ball's flight feels it */
    /* 0x04 */ u16 windDirection;  /* A full turn is 0x10000 */
    /* 0x08 */ s32 isRaining;      /* Shortens shots and changes the scene's colors */
    /* 0x0C */ u32 sceneColorIndex; /* With isRaining, picks DeriveSceneColors' colors */
    /* 0x10 */ u16 lightYaw;   /* g_LightYaw */
    /* 0x12 */ u16 lightPitch; /* g_LightPitch */
} HoleConditions; /* size = 0x14 */

typedef struct Fog {
    /* 0x00 */ s32 color[3];
    /* 0x0C */ s32 near;
    /* 0x10 */ s32 far;
} Fog; /* size = 0x14 */

extern Fog g_SceneFog;
/* The fog SetSceneFog draws while GAME_FLAG_SKY_EDITOR is set */
extern Fog g_DebugFog;
extern s32 g_PlayerCount;
/* The time the stopwatch modes show, in frames */
extern s32 g_StopwatchFrameCount;
/* Whether the course's name and the hole's number show before the flyover */
extern s8 g_IsHoleIntroTextShown;

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
    /* 0x10 */ u8 unk10; /* Percent of unkD that InitSkyColors mixes into g_CourseSkyImage */
    /* 0x11 */ u8 hasLensFlare;
    /* 0x12 */ u8 unk12[2];
    /* 0x14 */ struct Unk800C5F34 *unk14;
    /* 0x18 */ u8 fogColor[3];
    /* 0x1C */ s16 fogNear;
    /* 0x1E */ s16 fogFar;
} SkyColors; /* size = 0x20 */

/* Each course has colors for each time of day, dry and in the rain */
#define SKY_TIME_COUNT 4
#define SKY_WEATHER_COUNT 2

extern SkyColors g_SkyColors[][SKY_WEATHER_COUNT * SKY_TIME_COUNT];

typedef struct Unk800C5F34 {
    /* 0x00 */ u8 unk0[0x18];
    /* 0x18 */ f32 maxSunGlare; /* The most that DrawLensFlare's glare grows to */
    /* 0x1C */ u8 imageKind; /* Which images InitSkyColors loads into g_SkyImage */
} Unk800C5F34;

/* Images that InitSkyColors loads and recolors with the sky's colors */
#define ASSET_SKY_IMAGE_0 0x4D4
#define ASSET_SKY_IMAGE_1 0x4D5
#define ASSET_SKY_IMAGE_2_FIRST_HALF 0x706
#define ASSET_SKY_IMAGE_2_SECOND_HALF 0x707
#define SKY_IMAGE_HALF_OFFSET 0x808
#define ASSET_COURSE_SKY_IMAGE_0 0x4BA
extern u8 *g_SkyImage;
extern u8 *g_CourseSkyImage;

/* HoleConditions.sceneColorIndex values; over a round it rises from start to end */
#define SCENE_COLOR_INDEX_ROUND_START 0
#define SCENE_COLOR_INDEX_DRIVING_RANGE 6
#define SCENE_COLOR_INDEX_UNK8 8
#define SCENE_COLOR_INDEX_ROUND_LATE 9
#define SCENE_COLOR_INDEX_ROUND_END 10
#define SCENE_COLOR_INDEX_UNK11 11

extern HoleConditions g_HoleConditions;

/* What UpdateSky works out about the main and inset cameras, for drawing the sky */
#define SKY_VIEW_COUNT 2
typedef struct SkyState {
    /* 0x00 */ f32 horizonLevels[SKY_VIEW_COUNT];
    /* 0x08 */ f32 screenHeights[SKY_VIEW_COUNT]; /* SCREEN_HEIGHT once UpdateSky sets them */
    /* 0x10 */ f32 lookAtLevels[SKY_VIEW_COUNT];
    /* 0x18 */ f32 pitches[SKY_VIEW_COUNT]; /* In radians */
    /* 0x20 */ f32 yaws[SKY_VIEW_COUNT];
    /* 0x28 */ f32 scrollX; /* How far the sky's texture has scrolled with the wind */
    /* 0x2C */ f32 scrollY;
    /* 0x30 */ f32 viewedWindAngles[SKY_VIEW_COUNT]; /* The wind's direction as each camera sees it, in radians */
} SkyState; /* size = 0x38 */
extern SkyState g_SkyState;

/* HoleConditions.windDirection's units per radian */
#define WIND_DIRECTION_TO_RADIANS ((f32)(2 * M_PI / 0x10000))

/*
 * The wind's direction as seen from a camera: the arrow turns with the
 * camera's direction of view
 */
#define VIEWED_WIND_ANGLE(windDirection, view)            \
    ((windDirection) * WIND_DIRECTION_TO_RADIANS + M_PI - \
     atan2((view)->eye[2] - (view)->lookAt[2], (view)->eye[0] - (view)->lookAt[0]))

/*
 * Textures that are drawn every frame and that the course model reads
 * through RSP segments, each DYNAMIC_TEXTURE_SIZE pixels square
 */
#define DYNAMIC_TEXTURE_SIZE 32
extern u16 g_MirrorTexture[];  /* Part of the frame, upside down */
extern u16 g_FlameTexture[];   /* Noise scrolling over a flame gradient */
extern u16 g_BlendedTexture[]; /* Two loaded textures, one scrolling */

/* The texture memory that the second texture of a two-texture blend uses */
#define SECOND_TEXTURE_TMEM 0x100

/* The loaded textures that g_BlendedTexture blends, after 8-byte headers */
extern u8 *g_BlendedTextureScrollingLayer;
extern u8 *g_BlendedTextureStillLayer;

#define NOISE_GRID_SIZE 64
/* An I8 texture of NOISE_GRID_SIZE x NOISE_GRID_SIZE texels */
extern u8 *g_NoiseTexels;

/*
 * The screen row of the horizon in the main view, from its pitch and zoom.
 * The background drawer sets it.
 */
extern s32 g_HorizonRow;

/* Formatted text on its way to the screen */
extern char g_PrintBuffer[];

/*
 * Viewports in pairs, one per draw buffer. The first pair covers the
 * screen; the others start over its bottom-right quarter, and UpdateGame
 * fits the second pair to the inset view every frame.
 */
#define VIEWPORT_FULL_SCREEN 0
#define VIEWPORT_INSET 2
#define VIEWPORT_STAFF_ROLL 4
extern Vp g_Viewports[];

#endif
