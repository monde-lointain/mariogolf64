/*
 * Draws what surrounds the course: the sky, the sun's glare and lens flare, the
 * course's backdrop and the menus' backdrops. It also draws the HUD animations
 * that fade, spiral, tile, turn away or slice the whole frame, and sets up the
 * hole behind the title screen.
 */
#include "common.h"

#include "hole_display.h"

#include "asset.h"
#include "camera.h"
#include "controller.h"
#include "course.h"
#include "cpu_projection.h"
#include "debug_text.h"
#include "fireworks.h"
#include "game_flags.h"
#include "hole_loader.h"
#include "libc.h"
#include "math_util.h"
#include "round_hud.h"
#include "save.h"
#include "scene.h"
#include "screen.h"
#include "variables.h"

/*
 * The hole that plays behind the title screen: the last hole of the first
 * course, calm and lit from a fixed direction.
 */
#define TITLE_HOLE_INDEX 9
#define TITLE_COURSE_INDEX 0
#define TITLE_WIND_SPEED_MASK 0x3F
#define TITLE_WIND_SPEED_STEP 6000
#define TITLE_LIGHT_PITCH 0x1000
#define TITLE_LIGHT_YAW 0x6000
#define GAME_FLAG_UNK7 0x07
#define TITLE_FRAMEBUFFER_COUNT 2

/*
 * The sun's glare grows while the sun is on the screen with nothing in front
 * of it, flaring past SUN_GLARE_FLARE_STRENGTH before settling at full
 * strength, and fades when something covers it. Only the z-buffer within
 * SUN_DEPTH_MARGIN of the screen's edges is read.
 */
#define SUN_DEPTH_MARGIN 16.0f
#define SUN_GLARE_MARGIN 120.0f
#define SUN_SKY_DEPTH 0xFFFC /* The z-buffer value of the sky */
#define SUN_GLARE_FADE_STEP 0.1f
#define SUN_GLARE_GROWTH_STEP 0.3f
#define SUN_GLARE_FLARE_STRENGTH 1.5f
#define SUN_GLARE_FULL_STRENGTH 1.0f

extern f32 f_SunScreenX[SKY_VIEW_COUNT]; /* For each view */
extern f32 f_SunScreenY[SKY_VIEW_COUNT];
extern u16 f_SunDepth;
extern s32 f_IsSunGlareSettled;
extern f32 f_SunGlareStrength;

/* The sky's vertices, for each view and each matrix buffer */
#define SKY_VERTEX_COUNT 10
#define SKY_COLORED_VERTEX_COUNT 5
#define SKY_BLEND_STEP_COUNT 1000 /* f_SkyBlendStep counts to this at most */

extern Vtx f_SkyVertices[SKY_VIEW_COUNT * 2][SKY_VERTEX_COUNT];
extern s32 f_SkyTimeIndex;
extern s32 f_SkyBlendStep;
extern s32 f_SkyBlendStepCount;

/*
 * Each scene color index has a time of day for the sky's colors, and a time
 * and a percentage to blend from towards the next time. The tables are read
 * one entry past the scene color index too, for the next index's values.
 */
#define SKY_PERCENT_TO_STEPS (SKY_BLEND_STEP_COUNT / 100)
#define PERCENT_COUNT 100
#define SKY_IMAGE_KIND_0 0
#define SKY_IMAGE_KIND_1 1
/* The course sky image is paletted, and InitSkyColors recolors its palette */
#define PALETTE_START (IMAGE_HEADER_SIZE / PALETTE_COLOR_SIZE)
#define SKY_IMAGE_CHANNEL_MAX 31
/* The palette colors mix five parts of unk7 with one of unk4, shifted down to
 * five bits */
#define SKY_IMAGE_UNK7_PARTS 5
#define SKY_IMAGE_DIVISOR (6 * 8)
#define RGB_CHANNEL_COUNT 3
extern s32 f_SkyColorTimeIndex;
extern s8 f_SceneSkyColorTimes[];
extern s8 f_SceneSkyBlendTimes[];
extern s8 f_SceneSkyBlendPercents[];
extern s32 f_SkyHorizonColor[RGB_CHANNEL_COUNT];
extern s32 f_CourseSkyImageAssets[];

extern s32 D_801B609C;
extern s32 f_IsPrimitiveDepthSet;
extern s32 f_MenuBackdrop;
extern s32 f_HudAnimationFramesLeft;

extern s32 f_HudAnimation;
/*
 * The frame that the HUD animations break apart, copied two pixels to a word
 * from inside the HUD's margins. The tile animations copy it as a grid of
 * tiles, and HUD_ANIMATION_SLICE_CAPTURE copies a slice of the middle of the
 * frame each time it starts, for HUD_ANIMATION_SLICES to show.
 */
#define FRAME_WORDS_PER_ROW (SCREEN_WIDTH / 2)
#define FRAME_MARGIN_ROWS 8
#define FRAME_MARGIN_WORDS 4
#define FRAME_TILE_ROW_COUNT 7
#define FRAME_TILE_COLUMN_COUNT 5
#define FRAME_TILE_ROWS 32
#define FRAME_TILE_WORDS 32
#define FRAME_SLICE_COUNT 8
#define FRAME_SLICE_TOP 88
#define FRAME_SLICE_BOTTOM 208
#define FRAME_SLICE_LEFT_WORD 40
#define FRAME_SLICE_RIGHT_WORD 80
#define FRAME_SLICE_WORD_COUNT \
  ((FRAME_SLICE_BOTTOM - FRAME_SLICE_TOP) * \
      (FRAME_SLICE_RIGHT_WORD - FRAME_SLICE_LEFT_WORD))

/*
 * The slices that the slice animation copied fill the screen, a column of
 * them at a time, each drawn in strips that fit in texture memory. A new
 * slice appears every SLICE_FRAME_COUNT frames, with a sound.
 */
#define SLICE_ANIMATION_FRAME_COUNT 64
#define SLICE_FRAME_COUNT 8
#define SLICE_WIDTH ((FRAME_SLICE_RIGHT_WORD - FRAME_SLICE_LEFT_WORD) * 2)
#define SLICE_HEIGHT (FRAME_SLICE_BOTTOM - FRAME_SLICE_TOP)
#define SLICE_COLUMN_COUNT (SCREEN_WIDTH / SLICE_WIDTH)
#define SLICE_STRIP_COUNT 8
#define SLICE_STRIP_HEIGHT (SLICE_HEIGHT / SLICE_STRIP_COUNT)
#define SLICE_STRIP_WORD_COUNT (SLICE_STRIP_HEIGHT * SLICE_WIDTH / 2)
#define SOUND_SLICE 0x45
#define SLICE_SOUND_CHANNEL 13
#define SLICE_SOUND_VOLUME 127

/* The corners of the tiles, for each of two frames */
#define TILE_GRID_ROW_COUNT 8
#define TILE_GRID_COLUMN_COUNT 6
#define TILE_GRID_LEFT (-152)
#define TILE_GRID_RIGHT 152
#define TILE_GRID_TOP 112
#define TILE_GRID_SPACING 64
#define TILE_GRID_ROW_SPACING 32

extern u32* f_CapturedFrame;
extern Vtx f_TileGridVertices[2][TILE_GRID_ROW_COUNT * TILE_GRID_COLUMN_COUNT];
extern s32 f_IsFrameCapturePending;
extern s32 f_HudAnimationFrame;
/* The frame buffer of the last frame, kept for the next frame's captures */
extern u16* f_LastFrameBuffer;

/*
 * The plain backdrop copies the last frame, inside a border, into
 * f_CapturedFrame in strips. Each strip loads two rows more than it steps,
 * and the last loads only the rows that are left.
 */
#define BACKDROP_BORDER 8
#define BACKDROP_RIGHT (SCREEN_WIDTH - BACKDROP_BORDER - 1)
#define BACKDROP_STRIP_COUNT 38
#define BACKDROP_STRIP_STEP 6
#define BACKDROP_STRIP_HEIGHT 8
#define BACKDROP_LAST_STRIP_HEIGHT 2
/* Written with | because the target computes it without a branch, and + or a
 * plain ?: branches */
#define STRIP_HEIGHT(strip) \
  (BACKDROP_LAST_STRIP_HEIGHT | \
      ((strip) != BACKDROP_STRIP_COUNT - 1 \
              ? BACKDROP_STRIP_HEIGHT - BACKDROP_LAST_STRIP_HEIGHT \
              : 0))
#define COPY_MODE_DSDX (4 << 10) /* Copy mode writes four texels a cycle */
#define BACKDROP_START_FADE_LEVEL 255
/* Each frame the fade level steps down until it is at most
 * BACKDROP_LAST_FADE_LEVEL */
#define BACKDROP_FADE_STEP 32
#define BACKDROP_LAST_FADE_LEVEL 128
extern s32 f_BackdropFadeLevel;

/*
 * HUD_ANIMATION_SPIRAL cuts the frame inside the margins into a square grid.
 * Each frame, it draws as many fewer tiles as it fades.
 */
#define SPIRAL_GRID_SIZE 8
#define SPIRAL_TILE_COUNT (SPIRAL_GRID_SIZE * SPIRAL_GRID_SIZE)
#define SPIRAL_FRAME_WIDTH (SCREEN_WIDTH - (2 * FRAME_MARGIN_ROWS))
#define SPIRAL_TILE_WIDTH (SPIRAL_FRAME_WIDTH / SPIRAL_GRID_SIZE)
#define SPIRAL_TILE_HEIGHT \
  ((SCREEN_HEIGHT - (2 * FRAME_MARGIN_ROWS)) / SPIRAL_GRID_SIZE)
#define SPIRAL_FADING_TILE_COUNT 4
#define SPIRAL_OPAQUE_ALPHA 255
#define SPIRAL_ALPHA_STEP (SPIRAL_OPAQUE_ALPHA / (SPIRAL_FADING_TILE_COUNT + 1))
#define SPIRAL_RIGHT 0
#define SPIRAL_DOWN 1
#define SPIRAL_LEFT 2
#define SPIRAL_UP 3

/*
 * The frame's tiles turn over and fly apart towards the camera as the
 * animation goes on. The last column is narrower than the others.
 */
#define FRAME_TILE_COUNT (FRAME_TILE_ROW_COUNT * FRAME_TILE_COLUMN_COUNT)
#define FRAME_TILE_WIDTH (FRAME_TILE_WORDS * 2)
#define FRAME_TILE_WORD_COUNT (FRAME_TILE_ROWS * FRAME_TILE_WORDS)
#define TILE_ANIMATION_FRAME_COUNT 255
#define TILE_ANIMATION_STEP 10
#define TILE_ANIMATION_GAP_X 64
#define TILE_ANIMATION_GAP_Y 32
#define TILE_ANIMATION_OFFSET_X 8
#define TILE_ANIMATION_DEPTH -290.0f
#define TILE_ANIMATION_SPEED_Z 2
#define TILE_ANIMATION_UNUSED_ANGLE_OFFSET 0.2f
#define TILE_VERTEX_COUNT 4
extern Mtx f_TileMatrices[2][FRAME_TILE_COUNT];
extern Vtx f_TileVertices[TILE_VERTEX_COUNT];
extern Vtx f_LastColumnTileVertices[TILE_VERTEX_COUNT];

void CaptureHudAnimationFrame(void);
void DrawTileAnimation(void);
void DrawSliceAnimation(void);
void DrawSpiralAnimation(Gfx** displayListHead);

void SetFramebufferCount(s32 count);

/* Sets up a single-player game of the hole behind the title screen */
void SetUpTitleHole(void) {
  memset(&g_GameState, 0, sizeof(GameState));
  g_HoleIndex = TITLE_HOLE_INDEX;
  g_CourseIndex = TITLE_COURSE_INDEX;
  D_801B609C = 0;
  g_GameMode = GAME_MODE_UNK12;
  g_HoleConditions.isRaining = FALSE;
  g_HoleConditions.windSpeed =
      (rand() & TITLE_WIND_SPEED_MASK) * TITLE_WIND_SPEED_STEP;
  g_HoleConditions.windDirection = rand();
  g_HoleConditions.sceneColorIndex = SCENE_COLOR_INDEX_DRIVING_RANGE;
  g_LightPitch = TITLE_LIGHT_PITCH;
  g_LightYaw = TITLE_LIGHT_YAW;
  SetGameFlag(GAME_FLAG_UNK7);
  g_PlayerCount = 1;
  StopLoaderThread();
  StartLoaderThread();
  SetFramebufferCount(TITLE_FRAMEBUFFER_COUNT);
}

/* Grows or fades the sun's glare by whether the z-buffer shows anything in
 * front of the sun */
void UpdateSunGlare(void) {
  u16* zBuffer;

  if (f_SunScreenX[0] >= SUN_DEPTH_MARGIN &&
      f_SunScreenX[0] <= SCREEN_WIDTH - SUN_DEPTH_MARGIN &&
      f_SunScreenY[0] >= SUN_DEPTH_MARGIN &&
      f_SunScreenY[0] <= SCREEN_HEIGHT - SUN_DEPTH_MARGIN) {
    zBuffer = nuGfxZBuffer;
    f_SunDepth =
        zBuffer[((s32)f_SunScreenY[0] * SCREEN_WIDTH) + (s32)f_SunScreenX[0]];
    if (f_SunDepth >= SUN_SKY_DEPTH) {
      goto GROW;
    }

    f_IsSunGlareSettled = FALSE;
    f_SunGlareStrength -= SUN_GLARE_FADE_STEP;
    if (f_SunGlareStrength < 0.0f) {
      f_SunGlareStrength = 0.0f;
    }
  } else if (f_SunScreenX[0] > -SUN_GLARE_MARGIN &&
      f_SunScreenX[0] < SCREEN_WIDTH + SUN_GLARE_MARGIN &&
      f_SunScreenY[0] > -SUN_GLARE_MARGIN &&
      f_SunScreenY[0] < SCREEN_HEIGHT + SUN_GLARE_MARGIN) {
GROW:

    if (!f_IsSunGlareSettled) {
      f_SunGlareStrength += SUN_GLARE_GROWTH_STEP;
    }

    if (f_SunGlareStrength > SUN_GLARE_FLARE_STRENGTH && !f_IsSunGlareSettled) {
      f_IsSunGlareSettled = TRUE;
      f_SunGlareStrength = SUN_GLARE_FULL_STRENGTH;
    }
  } else {
    f_SunDepth = 0;
    f_IsSunGlareSettled = FALSE;
    f_SunGlareStrength = 0.0f;
  }
}

/*
 * The lens flare is drawn while the sun is on the screen, away from its
 * edges, and the inset view isn't drawing the scene. The wider bounds that
 * are also tested can't fail after the narrower ones. The glare grows with the
 * sun's distance from the nearer side and from the top, or from the bottom
 * within GLARE_BOTTOM_ROWS of it, and nothing is drawn below MIN_VISIBLE_GLARE.
 */
#define LENS_FLARE_TOP_MARGIN 64.0f
#define GLARE_BOTTOM_ROWS 96.0f
#define GLARE_BASE 16.0f
#define MIN_VISIBLE_GLARE 8.0f
/* The flares lie along the line from the sun through this point */
#define LENS_FLARE_AXIS_X (SCREEN_WIDTH / 2)
#define LENS_FLARE_AXIS_Y 96.0f
/* A flare's half width is its size times (glare + LENS_FLARE_SIZE_BIAS) / 128
 */
#define LENS_FLARE_SIZE_BIAS 16.0f
#define LENS_FLARE_SIZE_SCALE (1.0f / 128)
/* Only texels above this alpha are drawn */
#define LENS_FLARE_ALPHA_THRESHOLD 1
#define LENS_FLARE_COUNT 8

typedef struct LensFlare {
  /* 0x00 */ u8 color[3]; /* Where the ring texture is brightest */
  /* 0x03 */ u8
      envColor[3]; /* Where it is darkest, and the texels blend between them */
  /* 0x08 */ s32 size;
  /* 0x0C */ s32 unkC;
  /* 0x10 */ f32
      position; /* Along the line, 0 at the sun and 1 at its axis point */
} LensFlare; /* size = 0x14 */

extern LensFlare f_LensFlares[LENS_FLARE_COUNT];

/*
 * Draws the sun's lens flare, as rings along a line through the sun.
 *
 * Doesn't match: four of the display list pointers kept on the stack are
 * given each other's slots.
 */
#ifdef NON_MATCHING
void DrawLensFlare(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  SkyColors* colors =
      &g_SkyColors[GetCourseRecordIndex()]
                  [(g_HoleConditions.isRaining * SKY_TIME_COUNT) +
                      f_SkyColorTimeIndex];
  f32 edgeDistanceX;
  f32 edgeDistanceY;
  f32 glare;
  f32 centerX;
  f32 centerY;
  s32 size;
  s32 texelStep;
  s32 i;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  if (!colors->hasLensFlare || f_SunScreenX[0] <= SUN_DEPTH_MARGIN ||
      f_SunScreenX[0] >= SCREEN_WIDTH - SUN_DEPTH_MARGIN ||
      f_SunScreenY[0] <= SUN_DEPTH_MARGIN ||
      f_SunScreenY[0] >= SCREEN_HEIGHT - SUN_DEPTH_MARGIN ||
      f_SunScreenY[0] < -LENS_FLARE_TOP_MARGIN ||
      f_SunScreenY[0] > SCREEN_HEIGHT + SUN_GLARE_MARGIN ||
      f_SunScreenX[0] < -SUN_GLARE_MARGIN ||
      f_SunScreenX[0] > SCREEN_WIDTH + SUN_GLARE_MARGIN ||
      g_Cameras[INSET_CAMERA].view.drawFlags == CAMERA_DRAW_SCENE) {
    return;
  }

  edgeDistanceX = SCREEN_WIDTH - f_SunScreenX[0];
  if (edgeDistanceX > SCREEN_WIDTH / 2) {
    edgeDistanceX = SCREEN_WIDTH - edgeDistanceX;
  }

  edgeDistanceY = SCREEN_HEIGHT - f_SunScreenY[0];
  if (edgeDistanceY > GLARE_BOTTOM_ROWS) {
    edgeDistanceY = SCREEN_HEIGHT - edgeDistanceY;
  }

  glare = (edgeDistanceX + edgeDistanceY + GLARE_BASE) * f_SunGlareStrength;
  if (glare < MIN_VISIBLE_GLARE) {
    return;
  }

  if (glare > colors->unk14->maxSunGlare) {
    glare = colors->unk14->maxSunGlare;
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
  gDPSetBlendColor(gfx++, 255, 255, 255, LENS_FLARE_ALPHA_THRESHOLD);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
  gDPSetCombineLERP(gfx++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT,
      PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT,
      PRIMITIVE, 0, TEXEL0, 0);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetAlphaDither(gfx++, G_AD_PATTERN);
  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_MAGICSQ);
  gDPLoadTextureBlock(gfx++, (u32)g_RingTexels & ~7, G_IM_FMT_I, G_IM_SIZ_8b,
      RING_TEXTURE_SIZE, RING_TEXTURE_SIZE, 0, G_TX_MIRROR | G_TX_WRAP,
      G_TX_MIRROR | G_TX_WRAP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
  for (i = 0; i != LENS_FLARE_COUNT; i++) {
    centerX = f_SunScreenX[0] +
        ((LENS_FLARE_AXIS_X - f_SunScreenX[0]) * f_LensFlares[i].position);
    centerY = f_SunScreenY[0] +
        ((LENS_FLARE_AXIS_Y - f_SunScreenY[0]) * f_LensFlares[i].position);
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, f_LensFlares[i].color[0],
        f_LensFlares[i].color[1], f_LensFlares[i].color[2], glare);
    gDPSetEnvColor(gfx++, f_LensFlares[i].envColor[0],
        f_LensFlares[i].envColor[1], f_LensFlares[i].envColor[2], 255);
    size = f_LensFlares[i].size * (glare + LENS_FLARE_SIZE_BIAS) *
        LENS_FLARE_SIZE_SCALE;
    texelStep = (RING_TEXTURE_SIZE << 10) / size;
    gSPScisTextureRectangle(gfx++, (s32)(centerX - size) << 2,
        (s32)(centerY - size) << 2, (s32)(centerX + size) << 2,
        (s32)(centerY + size) << 2, G_TX_RENDERTILE, 0, 0, texelStep,
        texelStep);
  }

  gDPPipeSync(gfx++);
  gDPSetAlphaDither(gfx++, G_AD_PATTERN);
  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_DISABLE);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_display", DrawLensFlare);
#endif

/*
 * Moves the sky's and the fog's colors a step on from those of one time of
 * day towards those of the next
 */
void BlendSkyColors(void) {
  SkyColors* from;
  SkyColors* to;
  s32 group;
  s32 channel;

  if (f_SkyBlendStep < f_SkyBlendStepCount) {
    f_SkyBlendStep++;
  }

  from = &g_SkyColors[GetCourseRecordIndex()]
                     [(g_HoleConditions.isRaining * SKY_TIME_COUNT) +
                         f_SkyTimeIndex];
  to = &g_SkyColors[GetCourseRecordIndex()]
                   [(g_HoleConditions.isRaining * SKY_TIME_COUNT) +
                       f_SkyTimeIndex + 1];
  for (group = 0; group != SKY_VIEW_COUNT * 2; group++) {
    for (channel = 0; channel != 3; channel++) {
      f_SkyVertices[group][0].v.cn[channel] = from->unk4[channel] +
          ((to->unk4[channel] - from->unk4[channel]) * f_SkyBlendStep /
              SKY_BLEND_STEP_COUNT);
      f_SkyVertices[group][1].v.cn[channel] = from->unk7[channel] +
          ((to->unk7[channel] - from->unk7[channel]) * f_SkyBlendStep /
              SKY_BLEND_STEP_COUNT);
      f_SkyVertices[group][2].v.cn[channel] = from->unk7[channel] +
          ((to->unk7[channel] - from->unk7[channel]) * f_SkyBlendStep /
              SKY_BLEND_STEP_COUNT);
      f_SkyVertices[group][3].v.cn[channel] = from->unk4[channel] +
          ((to->unk4[channel] - from->unk4[channel]) * f_SkyBlendStep /
              SKY_BLEND_STEP_COUNT);
      f_SkyVertices[group][4].v.cn[channel] = from->unk4[channel] +
          ((to->unk4[channel] - from->unk4[channel]) * f_SkyBlendStep /
              SKY_BLEND_STEP_COUNT);
    }
  }

  for (channel = 0; channel != 3; channel++) {
    g_SceneFog.color[channel] = from->fogColor[channel] +
        ((to->fogColor[channel] - from->fogColor[channel]) * f_SkyBlendStep /
            SKY_BLEND_STEP_COUNT);
  }

  g_SceneFog.near = from->fogNear +
      ((to->fogNear - from->fogNear) * f_SkyBlendStep / SKY_BLEND_STEP_COUNT);
  g_SceneFog.far = from->fogFar +
      ((to->fogFar - from->fogFar) * f_SkyBlendStep / SKY_BLEND_STEP_COUNT);
}

/*
 * Sets the sky up for the hole's scene color index: its vertices' colors, the
 * blend between times of day that BlendSkyColors steps through, and
 * g_SkyImage and g_CourseSkyImage, recolored to match
 *
 * Doesn't match: the target keeps &g_HoleConditions.isRaining in a register
 * across the three GetCourseRecordIndex calls, and allocates other registers.
 */
#ifdef NON_MATCHING
void InitSkyColors(void) {
  EncodedAsset asset;
  u8 unused[0x10]; /* Pads the frame to the target's size */
  s32 blended[RGB_CHANNEL_COUNT];
  SkyColors* colors;
  SkyColors* from;
  SkyColors* to;
  s32 group;
  s32 channel;
  s32 colorIndex;
  u16 color;
  s32 red;
  s32 green;
  s32 blue;
  s32 alpha;
  s32 outerMix;
  s32 innerMix;

  /* Unused */
  s32 Lerp(s32 start, s32 end, f32 fraction) {
    return start * (1.0f - fraction) + end * fraction;
  }

  f_SkyColorTimeIndex = f_SceneSkyColorTimes[g_HoleConditions.sceneColorIndex];
  f_SkyTimeIndex = f_SceneSkyBlendTimes[g_HoleConditions.sceneColorIndex];
  f_SkyBlendStep = f_SceneSkyBlendPercents[g_HoleConditions.sceneColorIndex] *
      SKY_PERCENT_TO_STEPS;
  f_SkyBlendStepCount =
      f_SceneSkyBlendPercents[g_HoleConditions.sceneColorIndex + 1] *
      SKY_PERCENT_TO_STEPS;
  if (f_SkyBlendStepCount == 0 &&
      f_SceneSkyBlendTimes[g_HoleConditions.sceneColorIndex] !=
          f_SceneSkyBlendTimes[g_HoleConditions.sceneColorIndex + 1]) {
    f_SkyBlendStepCount = SKY_BLEND_STEP_COUNT;
  }

  colors = &g_SkyColors[GetCourseRecordIndex()]
                       [(g_HoleConditions.isRaining * SKY_TIME_COUNT) +
                           f_SkyColorTimeIndex];
  from = &g_SkyColors[GetCourseRecordIndex()]
                     [(g_HoleConditions.isRaining * SKY_TIME_COUNT) +
                         f_SkyTimeIndex];
  to = &g_SkyColors[GetCourseRecordIndex()]
                   [(g_HoleConditions.isRaining * SKY_TIME_COUNT) +
                       f_SkyTimeIndex + 1];
  g_SkyState.scrollX = 0.0f;
  g_SkyState.scrollY = 0.0f;
  for (group = 0; group != SKY_VIEW_COUNT * 2; group++) {
    for (channel = 0; channel != RGB_CHANNEL_COUNT; channel++) {
      f_SkyVertices[group][0].v.cn[channel] = colors->unk4[channel];
      f_SkyVertices[group][1].v.cn[channel] = colors->unk7[channel];
      f_SkyVertices[group][2].v.cn[channel] = colors->unk7[channel];
      f_SkyVertices[group][3].v.cn[channel] = colors->unk4[channel];
      f_SkyVertices[group][4].v.cn[channel] = colors->unk4[channel];
      innerMix = (colors->unk4[channel] + colors->unk7[channel]) * 0.5f;
      outerMix = (colors->unk4[channel] + f_SkyHorizonColor[channel]) * 0.5f;
      innerMix = (innerMix + f_SkyHorizonColor[channel]) * 0.5f;
      f_SkyVertices[group][5].v.cn[channel] = outerMix;
      f_SkyVertices[group][6].v.cn[channel] = innerMix;
      f_SkyVertices[group][7].v.cn[channel] = innerMix;
      f_SkyVertices[group][8].v.cn[channel] = outerMix;
      f_SkyVertices[group][9].v.cn[channel] = outerMix;
    }
  }

  if (colors->unk14->imageKind == SKY_IMAGE_KIND_0) {
    OpenEncodedAsset(ASSET_SKY_IMAGE_0, &asset);
    ReadEncodedAsset(g_SkyImage, &asset);
  } else if (colors->unk14->imageKind == SKY_IMAGE_KIND_1) {
    OpenEncodedAsset(ASSET_SKY_IMAGE_1, &asset);
    ReadEncodedAsset(g_SkyImage, &asset);
  } else {
    OpenEncodedAsset(ASSET_SKY_IMAGE_2_FIRST_HALF, &asset);
    ReadEncodedAsset(g_SkyImage, &asset);
    OpenEncodedAsset(ASSET_SKY_IMAGE_2_SECOND_HALF, &asset);
    ReadEncodedAsset(g_SkyImage + SKY_IMAGE_HALF_OFFSET, &asset);
  }

  OpenEncodedAsset((u16)f_CourseSkyImageAssets[GetCourseRecordIndex()], &asset);
  ReadEncodedAsset(g_CourseSkyImage, &asset);
  for (colorIndex = 0; colorIndex != LARGE_PALETTE_COLOR_COUNT; colorIndex++) {
    color = ((u16*)g_CourseSkyImage)[PALETTE_START + colorIndex];
    red = (color >> 11) & SKY_IMAGE_CHANNEL_MAX;
    green = (color >> 6) & SKY_IMAGE_CHANNEL_MAX;
    blue = (color >> 1) & SKY_IMAGE_CHANNEL_MAX;
    alpha = color & 1;
    if (!alpha) {
      red = ((colors->unk7[0] * SKY_IMAGE_UNK7_PARTS) + colors->unk4[0]) /
          SKY_IMAGE_DIVISOR;
      green = ((colors->unk7[1] * SKY_IMAGE_UNK7_PARTS) + colors->unk4[1]) /
          SKY_IMAGE_DIVISOR;
      blue = ((colors->unk7[2] * SKY_IMAGE_UNK7_PARTS) + colors->unk4[2]) /
          SKY_IMAGE_DIVISOR;
    } else {
      blended[0] = from->unkD[0] +
          ((to->unkD[0] - from->unkD[0]) * f_SkyBlendStep /
              SKY_BLEND_STEP_COUNT);
      blended[1] = from->unkD[1] +
          ((to->unkD[1] - from->unkD[1]) * f_SkyBlendStep /
              SKY_BLEND_STEP_COUNT);
      blended[2] = from->unkD[2] +
          ((to->unkD[2] - from->unkD[2]) * f_SkyBlendStep /
              SKY_BLEND_STEP_COUNT);
      red = ((red * (PERCENT_COUNT - colors->unk10)) +
                (blended[0] * colors->unk10)) /
          PERCENT_COUNT;
      green = ((green * (PERCENT_COUNT - colors->unk10)) +
                  (blended[1] * colors->unk10)) /
          PERCENT_COUNT;
      blue = ((blue * (PERCENT_COUNT - colors->unk10)) +
                 (blended[2] * colors->unk10)) /
          PERCENT_COUNT;
    }

    if (red < 0) {
      red = 0;
    }

    if (green < 0) {
      green = 0;
    }

    if (blue < 0) {
      blue = 0;
    }

    if (red > SKY_IMAGE_CHANNEL_MAX) {
      red = SKY_IMAGE_CHANNEL_MAX;
    }

    if (green > SKY_IMAGE_CHANNEL_MAX) {
      green = SKY_IMAGE_CHANNEL_MAX;
    }

    if (blue > SKY_IMAGE_CHANNEL_MAX) {
      blue = SKY_IMAGE_CHANNEL_MAX;
    }

    ((u16*)g_CourseSkyImage)[PALETTE_START + colorIndex] =
        (red << 11) | (green << 6) | (blue << 1) | alpha;
  }

  DeriveSceneColors();
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_display", Lerp);

INCLUDE_ASM("asm/nonmatchings/hole_display", InitSkyColors);
#endif

/* The sky editor's table, in debug text cells */
#define SKY_EDITOR_CONTROLLER 1
#define SKY_EDITOR_TEXT_COLUMN 13
#define SKY_EDITOR_TEXT_ROW 11
#define SKY_EDITOR_COLUMN_WIDTH 4
#define SKY_EDITOR_ERASE_COLUMN 14
#define SKY_EDITOR_ERASE_ROW 5
#define SKY_EDITOR_FRAMEBUFFER_COUNT 3

/* The table's columns: top, bottom, the unkA colors, the background image's
 * color, and fog */
#define SKY_EDITOR_TOP 0
#define SKY_EDITOR_BOTTOM 1
#define SKY_EDITOR_UNKA 2
#define SKY_EDITOR_BACKGROUND 3
#define SKY_EDITOR_FOG 4
/* Rows past blue: the background's alpha, and the fog's near and far */
#define SKY_EDITOR_ALPHA_ROW 3
#define SKY_EDITOR_FAR_ROW 4
#define SKY_EDITOR_LAST_COLOR_ROW 2

#define SKY_EDITOR_COLOR_STEP 4
#define SKY_EDITOR_FOG_STEP 4
#define BYTE_VALUE_COUNT 256
#define FOG_COLOR_MAX 255
#define FOG_DEPTH_MAX 1200
/* f_SkyHorizonColor's starting value in each channel */
#define SKY_HORIZON_LEVEL 63

extern s32 f_SkyEditorColumn;
extern s32 f_SkyEditorRow;
extern SkyColors* f_EditedSkyColors;

/*
 * While GAME_FLAG_SKY_EDITOR is set, lets controller 2 edit the current sky
 * colors and g_DebugFog, shows them in a table, and returns TRUE. The D-pad
 * or C buttons move the cursor, and holding A or B raises or lowers the value
 * under it.
 */
s32 UpdateSkyEditor(void) {
  EncodedAsset asset;
  s32 isImageChanged;
  s32 index; /* Counts the vertex groups, then the palette colors */
  s32 channel;
  u16 color;
  s32 red;
  s32 green;
  s32 blue;
  s32 alpha;
  s32 outerMix;
  s32 innerMix;
  u16 pressedButtons;

  isImageChanged = FALSE;
  if (!IsGameFlagSet(GAME_FLAG_SKY_EDITOR)) {
    return FALSE;
  }

  PrintDebugText(" ",
      (f_SkyEditorColumn * SKY_EDITOR_COLUMN_WIDTH) + SKY_EDITOR_ERASE_COLUMN,
      f_SkyEditorRow + SKY_EDITOR_ERASE_ROW);
  f_EditedSkyColors =
      &g_SkyColors[GetCourseRecordIndex()]
                  [(g_HoleConditions.isRaining * SKY_TIME_COUNT) +
                      f_SkyColorTimeIndex];
  if ((g_ControllerStates[SKY_EDITOR_CONTROLLER].pressedButtons &
          (L_JPAD | L_CBUTTONS)) &&
      f_SkyEditorColumn > 0) {
    f_SkyEditorColumn--;
  }

  if ((g_ControllerStates[SKY_EDITOR_CONTROLLER].pressedButtons &
          (R_JPAD | R_CBUTTONS)) &&
      f_SkyEditorColumn < SKY_EDITOR_FOG) {
    f_SkyEditorColumn++;
  }

  pressedButtons = g_ControllerStates[SKY_EDITOR_CONTROLLER].pressedButtons;
  if (pressedButtons & (U_JPAD | U_CBUTTONS)) {
    f_SkyEditorRow--;
  }

  if (pressedButtons & (D_JPAD | D_CBUTTONS)) {
    f_SkyEditorRow++;
  }

  if (f_SkyEditorRow < 0) {
    f_SkyEditorRow = 0;
  }

  if (f_SkyEditorColumn == SKY_EDITOR_BACKGROUND) {
    if (f_SkyEditorRow > SKY_EDITOR_ALPHA_ROW) {
      f_SkyEditorRow = SKY_EDITOR_ALPHA_ROW;
    }
  } else if (f_SkyEditorColumn == SKY_EDITOR_FOG) {
    if (f_SkyEditorRow > SKY_EDITOR_FAR_ROW) {
      f_SkyEditorRow = SKY_EDITOR_FAR_ROW;
    }
  } else if (f_SkyEditorRow > SKY_EDITOR_LAST_COLOR_ROW) {
    f_SkyEditorRow = SKY_EDITOR_LAST_COLOR_ROW;
  }

  /* The rows of the fog column run past g_DebugFog.color into near and far */
  if (g_ControllerStates[SKY_EDITOR_CONTROLLER].buttons & A_BUTTON) {
    switch (f_SkyEditorColumn) {
      case SKY_EDITOR_TOP:
        f_EditedSkyColors->unk4[f_SkyEditorRow] += SKY_EDITOR_COLOR_STEP;
        f_EditedSkyColors->unk4[f_SkyEditorRow] %= BYTE_VALUE_COUNT;
        break;
      case SKY_EDITOR_BOTTOM:
        f_EditedSkyColors->unk7[f_SkyEditorRow] += SKY_EDITOR_COLOR_STEP;
        f_EditedSkyColors->unk7[f_SkyEditorRow] %= BYTE_VALUE_COUNT;
        break;
      case SKY_EDITOR_UNKA:
        f_EditedSkyColors->unkA[f_SkyEditorRow] += SKY_EDITOR_COLOR_STEP;
        f_EditedSkyColors->unkA[f_SkyEditorRow] %= BYTE_VALUE_COUNT;
        break;
      case SKY_EDITOR_BACKGROUND:
        f_EditedSkyColors->unkD[f_SkyEditorRow]++;
        isImageChanged = TRUE;
        if (f_SkyEditorRow == SKY_EDITOR_ALPHA_ROW) {
          /* Wraps the percent past either end */
          if (f_EditedSkyColors->unkD[f_SkyEditorRow] == PERCENT_COUNT + 1) {
            f_EditedSkyColors->unkD[f_SkyEditorRow] = 0;
          }

          if (f_EditedSkyColors->unkD[f_SkyEditorRow] > PERCENT_COUNT + 1) {
            f_EditedSkyColors->unkD[f_SkyEditorRow] = PERCENT_COUNT;
          }
        } else {
          f_EditedSkyColors->unkD[f_SkyEditorRow] &= SKY_IMAGE_CHANNEL_MAX;
        }

        break;
      case SKY_EDITOR_FOG:
        g_DebugFog.color[f_SkyEditorRow] += SKY_EDITOR_FOG_STEP;
        break;
    }
  } else if (g_ControllerStates[SKY_EDITOR_CONTROLLER].buttons & B_BUTTON) {
    switch (f_SkyEditorColumn) {
      case SKY_EDITOR_TOP:
        f_EditedSkyColors->unk4[f_SkyEditorRow] -= SKY_EDITOR_COLOR_STEP;
        f_EditedSkyColors->unk4[f_SkyEditorRow] %= BYTE_VALUE_COUNT;
        break;
      case SKY_EDITOR_BOTTOM:
        f_EditedSkyColors->unk7[f_SkyEditorRow] -= SKY_EDITOR_COLOR_STEP;
        f_EditedSkyColors->unk7[f_SkyEditorRow] %= BYTE_VALUE_COUNT;
        break;
      case SKY_EDITOR_UNKA:
        f_EditedSkyColors->unkA[f_SkyEditorRow] -= SKY_EDITOR_COLOR_STEP;
        f_EditedSkyColors->unkA[f_SkyEditorRow] %= BYTE_VALUE_COUNT;
        break;
      case SKY_EDITOR_BACKGROUND:
        f_EditedSkyColors->unkD[f_SkyEditorRow]--;
        isImageChanged = TRUE;
        if (f_SkyEditorRow == SKY_EDITOR_ALPHA_ROW) {
          if (f_EditedSkyColors->unkD[f_SkyEditorRow] == PERCENT_COUNT + 1) {
            f_EditedSkyColors->unkD[f_SkyEditorRow] = 0;
          }

          if (f_EditedSkyColors->unkD[f_SkyEditorRow] > PERCENT_COUNT + 1) {
            f_EditedSkyColors->unkD[f_SkyEditorRow] = PERCENT_COUNT;
          }
        } else {
          f_EditedSkyColors->unkD[f_SkyEditorRow] &= SKY_IMAGE_CHANNEL_MAX;
        }

        break;
      case SKY_EDITOR_FOG:
        g_DebugFog.color[f_SkyEditorRow] -= SKY_EDITOR_FOG_STEP;
        break;
    }
  }

  if (g_DebugFog.color[0] > FOG_COLOR_MAX) {
    g_DebugFog.color[0] = FOG_COLOR_MAX;
  }

  if (g_DebugFog.color[1] > FOG_COLOR_MAX) {
    g_DebugFog.color[1] = FOG_COLOR_MAX;
  }

  if (g_DebugFog.color[2] > FOG_COLOR_MAX) {
    g_DebugFog.color[2] = FOG_COLOR_MAX;
  }

  if (g_DebugFog.near > FOG_DEPTH_MAX) {
    g_DebugFog.near = FOG_DEPTH_MAX;
  }

  if (g_DebugFog.far > FOG_DEPTH_MAX) {
    g_DebugFog.far = FOG_DEPTH_MAX;
  }

  if (g_DebugFog.color[0] < 0) {
    g_DebugFog.color[0] = 0;
  }

  if (g_DebugFog.color[1] < 0) {
    g_DebugFog.color[1] = 0;
  }

  if (g_DebugFog.color[2] < 0) {
    g_DebugFog.color[2] = 0;
  }

  if (g_DebugFog.near < 0) {
    g_DebugFog.near = 0;
  }

  if (g_DebugFog.far < 0) {
    g_DebugFog.far = 0;
  }

  for (index = 0; index != SKY_VIEW_COUNT * 2; index++) {
    for (channel = 0; channel != RGB_CHANNEL_COUNT; channel++) {
      f_SkyVertices[index][0].v.cn[channel] = f_EditedSkyColors->unk4[channel];
      f_SkyVertices[index][1].v.cn[channel] = f_EditedSkyColors->unk7[channel];
      f_SkyVertices[index][2].v.cn[channel] = f_EditedSkyColors->unk7[channel];
      f_SkyVertices[index][3].v.cn[channel] = f_EditedSkyColors->unk4[channel];
      f_SkyVertices[index][4].v.cn[channel] = f_EditedSkyColors->unk4[channel];
      innerMix = (u16)(f_EditedSkyColors->unk4[channel] +
                     f_EditedSkyColors->unk7[channel]) *
          0.5f;
      outerMix = (f_EditedSkyColors->unk4[channel] + SKY_HORIZON_LEVEL) * 0.5f;
      innerMix = (innerMix + SKY_HORIZON_LEVEL) * 0.5f;
      f_SkyVertices[index][5].v.cn[channel] = outerMix;
      f_SkyVertices[index][6].v.cn[channel] = innerMix;
      f_SkyVertices[index][7].v.cn[channel] = innerMix;
      f_SkyVertices[index][8].v.cn[channel] = outerMix;
      f_SkyVertices[index][9].v.cn[channel] = outerMix;
    }
  }

  if (isImageChanged) {
    OpenEncodedAsset(
        (u16)f_CourseSkyImageAssets[GetCourseRecordIndex()], &asset);
    ReadEncodedAsset(g_CourseSkyImage, &asset);
    for (index = 0; index != LARGE_PALETTE_COLOR_COUNT; index++) {
      color = ((u16*)g_CourseSkyImage)[PALETTE_START + index];
      red = (color >> 11) & SKY_IMAGE_CHANNEL_MAX;
      green = (color >> 6) & SKY_IMAGE_CHANNEL_MAX;
      blue = (color >> 1) & SKY_IMAGE_CHANNEL_MAX;
      alpha = color & 1;
      if (!alpha) {
        red = ((f_EditedSkyColors->unk7[0] * SKY_IMAGE_UNK7_PARTS) +
                  f_EditedSkyColors->unk4[0]) /
            SKY_IMAGE_DIVISOR;
        green = ((f_EditedSkyColors->unk7[1] * SKY_IMAGE_UNK7_PARTS) +
                    f_EditedSkyColors->unk4[1]) /
            SKY_IMAGE_DIVISOR;
        blue = ((f_EditedSkyColors->unk7[2] * SKY_IMAGE_UNK7_PARTS) +
                   f_EditedSkyColors->unk4[2]) /
            SKY_IMAGE_DIVISOR;
      } else {
        red = ((red * (PERCENT_COUNT - f_EditedSkyColors->unk10)) +
                  (f_EditedSkyColors->unkD[0] * f_EditedSkyColors->unk10)) /
            PERCENT_COUNT;
        green = ((green * (PERCENT_COUNT - f_EditedSkyColors->unk10)) +
                    (f_EditedSkyColors->unkD[1] * f_EditedSkyColors->unk10)) /
            PERCENT_COUNT;
        blue = ((blue * (PERCENT_COUNT - f_EditedSkyColors->unk10)) +
                   (f_EditedSkyColors->unkD[2] * f_EditedSkyColors->unk10)) /
            PERCENT_COUNT;
      }

      if (red < 0) {
        red = 0;
      }

      if (green < 0) {
        green = 0;
      }

      if (blue < 0) {
        blue = 0;
      }

      if (red > SKY_IMAGE_CHANNEL_MAX) {
        red = SKY_IMAGE_CHANNEL_MAX;
      }

      if (green > SKY_IMAGE_CHANNEL_MAX) {
        green = SKY_IMAGE_CHANNEL_MAX;
      }

      if (blue > SKY_IMAGE_CHANNEL_MAX) {
        blue = SKY_IMAGE_CHANNEL_MAX;
      }

      ((u16*)g_CourseSkyImage)[PALETTE_START + index] =
          (red << 11) | (green << 6) | (blue << 1) | alpha;
    }
  }

  PrintDebugText(
      "   T   B   C   BG  FOG", SKY_EDITOR_TEXT_COLUMN, SKY_EDITOR_TEXT_ROW);
  sprintf(g_PrintBuffer, "R: %3d %3d %3d %3d %4d  ", f_EditedSkyColors->unk4[0],
      f_EditedSkyColors->unk7[0], f_EditedSkyColors->unkA[0],
      f_EditedSkyColors->unkD[0], g_DebugFog.color[0]);
  PrintDebugText(
      g_PrintBuffer, SKY_EDITOR_TEXT_COLUMN, SKY_EDITOR_TEXT_ROW + 1);
  sprintf(g_PrintBuffer, "G: %3d %3d %3d %3d %4d  ", f_EditedSkyColors->unk4[1],
      f_EditedSkyColors->unk7[1], f_EditedSkyColors->unkA[1],
      f_EditedSkyColors->unkD[1], g_DebugFog.color[1]);
  PrintDebugText(
      g_PrintBuffer, SKY_EDITOR_TEXT_COLUMN, SKY_EDITOR_TEXT_ROW + 2);
  sprintf(g_PrintBuffer, "B: %3d %3d %3d %3d %4d  ", f_EditedSkyColors->unk4[2],
      f_EditedSkyColors->unk7[2], f_EditedSkyColors->unkA[2],
      f_EditedSkyColors->unkD[2], g_DebugFog.color[2]);
  PrintDebugText(
      g_PrintBuffer, SKY_EDITOR_TEXT_COLUMN, SKY_EDITOR_TEXT_ROW + 3);
  sprintf(g_PrintBuffer, "A:             %3d %4d  ", f_EditedSkyColors->unk10,
      g_DebugFog.near);
  PrintDebugText(
      g_PrintBuffer, SKY_EDITOR_TEXT_COLUMN, SKY_EDITOR_TEXT_ROW + 4);
  sprintf(g_PrintBuffer, "CC:%d               %4d  ", f_EditedSkyColors->unk0,
      g_DebugFog.far);
  PrintDebugText(
      g_PrintBuffer, SKY_EDITOR_TEXT_COLUMN, SKY_EDITOR_TEXT_ROW + 5);
  PrintDebugText(">",
      (f_SkyEditorColumn * SKY_EDITOR_COLUMN_WIDTH) + SKY_EDITOR_TEXT_COLUMN +
          2,
      f_SkyEditorRow + SKY_EDITOR_TEXT_ROW + 1);
  SetFramebufferCount(SKY_EDITOR_FRAMEBUFFER_COUNT);
  SetScreenEffect(SCREEN_EFFECT_NONE, 0);
  return TRUE;
}

/* UpdateSky's limits and scales */
#define SKY_PITCH_LIMIT 1.5f
#define SKY_SLOPE_DISTANCE 200.0
#define SKY_REFERENCE_FIELD_OF_VIEW 60.0f
#define SKY_EYE_HEIGHT_SCALE 0.05f
#define SKY_HORIZON_LEVEL_MIN (-320.0f)
#define SKY_HORIZON_BIAS 10.0f
#define SKY_HORIZON_TO_HEIGHT 7.3f
#define SKY_TOP_DEPTH (-2048.0f)
#define SKY_PITCH_TO_DEPTH (-2048.0f / 3.14f)
#define SKY_HEIGHT_LIMIT 0x500
/* Scales how far the clipped heights push the texture */
#define SKY_CLIPPED_HEIGHT_SHIFT 5
#define SKY_SCROLL_PER_WIND_SPEED 0.5f
#define SKY_SCROLL_X_WRAP 8192.0f
#define SKY_SCROLL_Y_WRAP 8192.0f
#define SKY_SCROLL_Y_PERIOD 16384.0f
#define SKY_TEXTURE_HALF_WIDTH 640.0f
#define SKY_TEXTURE_FAR_DISTANCE 800.0f
#define SKY_TEXTURE_NEAR_DISTANCE 160.0
#define SKY_TEXTURE_HORIZON_DISTANCE 640.0
#define SKY_TEXTURE_SCALE 16
#define SKY_DEBUG_CYCLE_LENGTH 3

/*
 * Points the sky of a view at its camera, and scrolls the sky's texture with
 * the wind. The first view also blends the sky's colors, unless the sky
 * editor is open.
 */
void UpdateSky(CameraView* view, s32 viewIndex) {
  f32* pitches;
  f32 pitchScale; /* Narrower fields of view move the horizon further for each
                     radian of pitch */
  Vec3 direction;
  s32 unused[8]; /* The original frame has room for this */
  Vtx* vertices;
  f32 slope;
  f32 scroll;
  s32 clippedHeight;
  f32 sine;
  f32 cosine;
  s32 distance;
  f32 coordinate;
  f32 horizonHalfWidth;

  if (!UpdateSkyEditor() && viewIndex == 0) {
    BlendSkyColors();
  }

  direction[0] = view->lookAt[0] - view->eye[0];
  direction[1] = view->lookAt[1] - view->eye[1];
  direction[2] = view->lookAt[2] - view->eye[2];
  vertices = f_SkyVertices[g_MatrixBufferIndex + (viewIndex * 2)];
  g_SkyState.yaws[viewIndex] = atan2(direction[0], direction[2]);
  g_SkyState.pitches[viewIndex] = -atan2(direction[1],
      sqrtf((direction[0] * direction[0]) + (direction[2] * direction[2])));
  if (g_SkyState.pitches[viewIndex] < -SKY_PITCH_LIMIT) {
    g_SkyState.pitches[viewIndex] = -SKY_PITCH_LIMIT;
  }

  if (g_SkyState.pitches[viewIndex] > SKY_PITCH_LIMIT) {
    g_SkyState.pitches[viewIndex] = SKY_PITCH_LIMIT;
  }

  slope = (view->lookAt[1] - view->eye[1]) *
      (SKY_SLOPE_DISTANCE /
          sqrtf((direction[0] * direction[0]) + (direction[2] * direction[2])));
  pitchScale = SKY_REFERENCE_FIELD_OF_VIEW / view->fieldOfView * SCREEN_HEIGHT;
  g_SkyState.horizonLevels[viewIndex] =
      (-g_SkyState.pitches[viewIndex] * pitchScale) +
      (view->eye[1] * SKY_EYE_HEIGHT_SCALE);
  if (g_SkyState.horizonLevels[viewIndex] < SKY_HORIZON_LEVEL_MIN) {
    g_SkyState.horizonLevels[viewIndex] = SKY_HORIZON_LEVEL_MIN;
  }

  g_SkyState.screenHeights[viewIndex] = SCREEN_HEIGHT;
  g_SkyState.lookAtLevels[viewIndex] = g_SkyState.horizonLevels[viewIndex] -
      (((view->lookAt[1] * slope) - view->eye[1]) * SKY_EYE_HEIGHT_SCALE);

  if (IsGameFlagSet(GAME_FLAG_CYCLE_SKY_CONDITIONS)) {
    if (g_ControllerStates[0].pressedButtons & U_CBUTTONS) {
      g_HoleConditions.isRaining =
          (u32)(g_HoleConditions.isRaining + 1) % SKY_DEBUG_CYCLE_LENGTH;
      InitSkyColors();
    }

    if (g_ControllerStates[0].pressedButtons & D_CBUTTONS) {
      g_HoleConditions.sceneColorIndex =
          (g_HoleConditions.sceneColorIndex + 1) % SKY_DEBUG_CYCLE_LENGTH;
      InitSkyColors();
    }
  }

  clippedHeight = 0;
  scroll = -(f32)(g_HoleConditions.windSpeed / WIND_SPEED_UNIT) *
      SKY_SCROLL_PER_WIND_SPEED;
  if (g_ScreenEffect != SCREEN_EFFECT_SAVED_FRAME) {
    g_SkyState.scrollY += scroll;
  }

  if (g_SkyState.scrollX > SKY_SCROLL_X_WRAP) {
    g_SkyState.scrollX -= SKY_SCROLL_X_WRAP;
  }

  if (g_SkyState.scrollX < -SKY_SCROLL_X_WRAP) {
    g_SkyState.scrollX += SKY_SCROLL_X_WRAP;
  }

  if (scroll > 0.0f) {
    if (g_SkyState.scrollY > SKY_SCROLL_Y_WRAP) {
      g_SkyState.scrollY -= SKY_SCROLL_Y_PERIOD;
    }
  } else if (g_SkyState.scrollY < -SKY_SCROLL_Y_WRAP) {
    g_SkyState.scrollY += SKY_SCROLL_Y_PERIOD;
  }

  /* Vertices 1, 2, 6 and 7 follow the horizon level; 0, 3 and 4 follow the
   * pitch alone */
  if (g_SkyState.pitches[viewIndex] < 0.0f) {
    vertices[0].v.ob[1] = (SCREEN_HEIGHT / 2) -
        (g_SkyState.pitches[viewIndex] * (SCREEN_HEIGHT / 2));
    vertices[3].v.ob[1] = (SCREEN_HEIGHT / 2) -
        (g_SkyState.pitches[viewIndex] * (SCREEN_HEIGHT / 2));
    vertices[4].v.ob[1] = (SCREEN_HEIGHT / 2) -
        (g_SkyState.pitches[viewIndex] * (SCREEN_HEIGHT / 2));
  } else {
    vertices[0].v.ob[1] = SCREEN_HEIGHT / 2;
    vertices[3].v.ob[1] = SCREEN_HEIGHT / 2;
    vertices[4].v.ob[1] = SCREEN_HEIGHT / 2;
  }

  vertices[1].v.ob[1] =
      (g_SkyState.horizonLevels[viewIndex] + SKY_HORIZON_BIAS) *
      SKY_HORIZON_TO_HEIGHT;
  vertices[2].v.ob[1] =
      (g_SkyState.horizonLevels[viewIndex] + SKY_HORIZON_BIAS) *
      SKY_HORIZON_TO_HEIGHT;
  vertices[1].v.ob[2] =
      SKY_TOP_DEPTH - (g_SkyState.pitches[viewIndex] * SKY_PITCH_TO_DEPTH);
  vertices[2].v.ob[2] =
      SKY_TOP_DEPTH - (g_SkyState.pitches[viewIndex] * SKY_PITCH_TO_DEPTH);
  vertices[6].v.ob[2] =
      SKY_TOP_DEPTH - (-g_SkyState.pitches[viewIndex] * SKY_PITCH_TO_DEPTH);
  vertices[7].v.ob[2] =
      SKY_TOP_DEPTH - (-g_SkyState.pitches[viewIndex] * SKY_PITCH_TO_DEPTH);
  if (vertices[1].v.ob[1] < -SKY_HEIGHT_LIMIT) {
    clippedHeight = (vertices[1].v.ob[1] + SKY_HEIGHT_LIMIT) /
        (1 << SKY_CLIPPED_HEIGHT_SHIFT);
    vertices[1].v.ob[1] = -SKY_HEIGHT_LIMIT;
  }

  if (vertices[2].v.ob[1] < -SKY_HEIGHT_LIMIT) {
    vertices[2].v.ob[1] = -SKY_HEIGHT_LIMIT;
  }

  if (vertices[1].v.ob[1] > SKY_HEIGHT_LIMIT) {
    vertices[1].v.ob[1] = SKY_HEIGHT_LIMIT;
  }

  if (vertices[2].v.ob[1] > SKY_HEIGHT_LIMIT) {
    vertices[2].v.ob[1] = SKY_HEIGHT_LIMIT;
  }

  vertices[6].v.ob[1] = vertices[1].v.ob[1];
  vertices[7].v.ob[1] = vertices[2].v.ob[1];

  /* Each row of texture coordinates lies at a distance along the viewed wind
   * direction */
  sine = sinf(g_SkyState.viewedWindAngles[viewIndex]);
  cosine = cosf(g_SkyState.viewedWindAngles[viewIndex]);
  distance = clippedHeight + SKY_TEXTURE_FAR_DISTANCE;
  coordinate = (sine * distance) - (cosine * SKY_TEXTURE_HALF_WIDTH);
  vertices[1].v.tc[0] =
      ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
  coordinate = (cosine * distance) + (sine * SKY_TEXTURE_HALF_WIDTH);
  vertices[1].v.tc[1] =
      ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
  coordinate = (sine * distance) + (cosine * SKY_TEXTURE_HALF_WIDTH);
  vertices[2].v.tc[0] =
      ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
  coordinate = (cosine * distance) - (sine * SKY_TEXTURE_HALF_WIDTH);
  pitches = g_SkyState.pitches;
  vertices[2].v.tc[1] =
      ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
  horizonHalfWidth = SCREEN_WIDTH;
  distance = SKY_TEXTURE_NEAR_DISTANCE -
      (pitches[viewIndex] * horizonHalfWidth * (2 / M_PI)) + clippedHeight;
  coordinate = (sine * distance) - (cosine * SKY_TEXTURE_HALF_WIDTH);
  vertices[0].v.tc[0] =
      ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
  coordinate = (cosine * distance) + (sine * SKY_TEXTURE_HALF_WIDTH);
  vertices[0].v.tc[1] =
      ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
  coordinate = (sine * distance) + (cosine * SKY_TEXTURE_HALF_WIDTH);
  vertices[3].v.tc[0] =
      ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
  coordinate = (cosine * distance) - (sine * SKY_TEXTURE_HALF_WIDTH);
  vertices[3].v.tc[1] =
      ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
  vertices[4].v.tc[0] =
      ((s32)(sine * distance) * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
  vertices[4].v.tc[1] =
      ((s32)(cosine * distance) * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
  distance = (SKY_TEXTURE_HORIZON_DISTANCE -
                 (-pitches[viewIndex] * horizonHalfWidth * (2 / M_PI))) *
      horizonHalfWidth / SCREEN_WIDTH;
  coordinate = (sine * distance) - (cosine * horizonHalfWidth);
  vertices[6].v.tc[0] =
      ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
  coordinate = (cosine * distance) + (sine * horizonHalfWidth);
  vertices[6].v.tc[1] =
      ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
  coordinate = (sine * distance) + (cosine * horizonHalfWidth);
  vertices[7].v.tc[0] =
      ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
  coordinate = (cosine * distance) - (sine * horizonHalfWidth);
  vertices[7].v.tc[1] =
      ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
  distance = (SKY_TEXTURE_NEAR_DISTANCE -
                 (-pitches[viewIndex] * horizonHalfWidth * (2 / M_PI))) *
      horizonHalfWidth / SCREEN_WIDTH;
  coordinate = (sine * distance) - (cosine * horizonHalfWidth);
  vertices[5].v.tc[0] =
      ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
  coordinate = (cosine * distance) + (sine * horizonHalfWidth);
  vertices[5].v.tc[1] =
      ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
  coordinate = (sine * distance) + (cosine * horizonHalfWidth);
  vertices[8].v.tc[0] =
      ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
  coordinate = (cosine * distance) - (sine * horizonHalfWidth);
  vertices[8].v.tc[1] =
      ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
  vertices[9].v.tc[0] =
      ((s32)(sine * distance) * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
  vertices[9].v.tc[1] =
      ((s32)(cosine * distance) * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
}

/* Sets whether the course's drawing gives its primitives a fixed depth */
void UsePrimitiveDepth(s32 isPrimitiveDepthSet) {
  f_IsPrimitiveDepthSet = isPrimitiveDepthSet;
}

/* The sky dome is drawn this far in front of the camera */
#define SKY_DOME_DISTANCE -300.0f
#define SKY_NOISE_IMAGE_SIZE 64
#define SKY_NOISE_MASK 6
#define SKY_DEPTH 0x7FFB
/* The sun moves SUN_TURN_WIDTH pixels across the screen as the camera turns
 * once */
#define SUN_TURN_WIDTH 1536.0f
#define SUN_YAW_OFFSET (M_PI / 4)
#define SUN_HORIZON_Y 112.0f
#define SUN_CULL_MARGIN 64.0f
#define SUN_DEPTH 0x7FF7
#define SUN_IMAGE_KIND_TWO_TEXTURES 2
#define SUN_IMAGE_SIZE 32
#define SUN_GLOW_IMAGE_SIZE 64
#define SUN_GLOW_MASK 6
#define SUN_MASK_TMEM 0x100
#define LIGHT_YAW_TO_RADIANS ((f32)(2 * M_PI / 0x10000))
extern Mtx f_SkyModelMatrix;

/*
 * Draws the sky dome around the camera and, when the scene has one, the sun.
 * The sun is a textured rectangle placed from the camera's yaw against the
 * light's, and the dome's height above the horizon.
 */
void DrawSky(Gfx** displayListHead, f32 eyeY, s32 arg2, s32 cameraIndex) {
  Gfx* gfx = *displayListHead;
  SkyColors* colors;
  Mat4 matrix;
  s32 unused[16];
  Vec3 rotation;
  Vec3 translation;
  f32 angle;

  UpdateSky(&g_Cameras[cameraIndex].view, cameraIndex);

  /* Unlike the other lookups of g_SkyColors, this steps by three times, not
   * SKY_TIME_COUNT, in the rain */
  colors = &g_SkyColors[GetCourseRecordIndex()]
                       [(g_HoleConditions.isRaining * 3) + f_SkyColorTimeIndex];
  translation[0] = 0.0f;
  translation[1] = 0.0f;
  rotation[0] = 0.0f;
  rotation[1] = 0.0f;
  rotation[2] = 0.0f;
  translation[2] = SKY_DOME_DISTANCE;
  BuildRotationMatrix(matrix, rotation, translation);
  ConvertMatrixToFixed(matrix, &f_SkyModelMatrix);
  gDPSetPrimColor(gfx++, 0, 0, 127, 127, 127, 255);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  if (f_IsPrimitiveDepthSet) {
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    gDPSetPrimDepth(gfx++, SKY_DEPTH, 0);
  } else {
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  }

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
  gSPSetGeometryMode(gfx++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
  if (colors->unk0 == 0) {
    gDPSetCombineLERP(gfx++, ENVIRONMENT, SHADE, TEXEL0, SHADE, 0, 0, 0, TEXEL0,
        ENVIRONMENT, SHADE, TEXEL0, SHADE, 0, 0, 0, TEXEL0);
  } else {
    gDPSetCombineLERP(gfx++, TEXEL0, 0, ENVIRONMENT, SHADE, 0, 0, 0, TEXEL0,
        TEXEL0, 0, ENVIRONMENT, SHADE, 0, 0, 0, TEXEL0);
  }

  gDPSetEnvColor(gfx++, colors->unkA[0], colors->unkA[1], colors->unkA[2], 255);
  gDPLoadTextureBlock(gfx++, (u32)g_NoiseTexels & ~7, G_IM_FMT_I, G_IM_SIZ_8b,
      SKY_NOISE_IMAGE_SIZE, SKY_NOISE_IMAGE_SIZE, 0, G_TX_WRAP, G_TX_WRAP,
      SKY_NOISE_MASK, SKY_NOISE_MASK, G_TX_NOLOD, G_TX_NOLOD);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenEffectProjection),
      G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_SkyModelMatrix),
      G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPPerspNormalize(gfx++, g_ScreenEffectPerspNorm);
  gSPVertex(gfx++, f_SkyVertices[g_MatrixBufferIndex + (cameraIndex * 2)],
      SKY_VERTEX_COUNT, 0);
  gDPPipeSync(gfx++);
  gSP1Triangle(gfx++, 0, 1, 4, 0);
  gSP1Triangle(gfx++, 1, 2, 4, 0);
  gSP1Triangle(gfx++, 4, 2, 3, 0);
  gDPPipeSync(gfx++);
  if (colors->hasLensFlare == TRUE) {
    angle = -g_SkyState.yaws[cameraIndex] - SUN_YAW_OFFSET -
        (g_LightYaw * LIGHT_YAW_TO_RADIANS);
    if (angle > 2 * M_PI) {
      angle -= 2 * M_PI;
    }

    if (angle < 0.0f) {
      angle += 2 * M_PI;
    }

    if (angle > 2 * M_PI) {
      angle -= 2 * M_PI;
    }

    if (angle < 0.0f) {
      angle += 2 * M_PI;
    }

    f_SunScreenX[cameraIndex] = angle * SUN_TURN_WIDTH * (1 / (2 * M_PI));
    f_SunScreenY[cameraIndex] = -g_SkyState.horizonLevels[cameraIndex] -
        colors->unk14->sunHeight + SUN_HORIZON_Y;
    if (f_SunScreenX[cameraIndex] > SUN_TURN_WIDTH / 2) {
      f_SunScreenX[cameraIndex] -= SUN_TURN_WIDTH;
    }

    if (f_SunScreenX[cameraIndex] > -SUN_CULL_MARGIN &&
        f_SunScreenX[cameraIndex] < SCREEN_WIDTH + SUN_CULL_MARGIN &&
        f_SunScreenY[cameraIndex] > -SUN_CULL_MARGIN &&
        f_SunScreenY[cameraIndex] < SCREEN_HEIGHT + SUN_CULL_MARGIN) {
      gDPPipeSync(gfx++);
      gDPSetAlphaCompare(gfx++, G_AC_NONE);
      gDPPipeSync(gfx++);
      gDPSetTexturePersp(gfx++, G_TP_NONE);
      gDPPipeSync(gfx++);
      gDPSetTextureLUT(gfx++, G_TT_NONE);
      gDPPipeSync(gfx++);
      gDPPipeSync(gfx++);
      gDPSetCycleType(gfx++, G_CYC_1CYCLE);
      gDPPipeSync(gfx++);
      gDPSetTextureFilter(gfx++, G_TF_AVERAGE);
      gDPPipeSync(gfx++);
      gDPSetColorDither(gfx++, G_CD_MAGICSQ);
      if (f_IsPrimitiveDepthSet) {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF | Z_UPD, G_RM_ZB_XLU_SURF2);
        gDPSetPrimDepth(gfx++, SUN_DEPTH, 0);
      } else {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
      }

      gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
      if (colors->unk14->imageKind == SUN_IMAGE_KIND_TWO_TEXTURES) {
        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_PATTERN);
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_2CYCLE);
        gDPSetCombineLERP(gfx++, TEXEL1, 0, TEXEL0, 0, 0, 0, 0, TEXEL1, TEXEL1,
            0, COMBINED, 0, 0, 0, 0, COMBINED);
        gDPLoadTextureBlock(gfx++, ((u32)g_SkyImage + IMAGE_HEADER_SIZE) & ~7,
            G_IM_FMT_RGBA, G_IM_SIZ_16b, SUN_IMAGE_SIZE, SUN_IMAGE_SIZE, 0,
            G_TX_WRAP, G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
            G_TX_NOLOD);
        gDPLoadMultiBlock_4b(gfx++,
            ((u32)g_SkyImage + SKY_IMAGE_HALF_OFFSET + IMAGE_HEADER_SIZE) & ~7,
            SUN_MASK_TMEM, 1, G_IM_FMT_I, SUN_IMAGE_SIZE, SUN_IMAGE_SIZE, 0,
            G_TX_WRAP, G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
            G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++,
            ((s32)f_SunScreenX[cameraIndex] - colors->unk14->sunRadius + 1)
                << 2,
            ((s32)f_SunScreenY[cameraIndex] - colors->unk14->sunRadius + 1)
                << 2,
            ((s32)f_SunScreenX[cameraIndex] + colors->unk14->sunRadius) << 2,
            ((s32)f_SunScreenY[cameraIndex] + colors->unk14->sunRadius) << 2,
            G_TX_RENDERTILE, 0, 0, colors->unk14->sunTexelStep,
            colors->unk14->sunTexelStep);
        gDPPipeSync(gfx++);
      } else {
        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_NOISE);
        gDPSetCombineLERP(gfx++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0,
            0, 0, TEXEL0, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0,
            TEXEL0);
        gDPLoadTextureBlock(gfx++, ((u32)g_SkyImage + IMAGE_HEADER_SIZE) & ~7,
            G_IM_FMT_I, G_IM_SIZ_8b, SUN_GLOW_IMAGE_SIZE, SUN_GLOW_IMAGE_SIZE,
            0, G_TX_MIRROR, G_TX_MIRROR, SUN_GLOW_MASK, SUN_GLOW_MASK,
            G_TX_NOLOD, G_TX_NOLOD);
        gDPSetPrimColor(gfx++, 0, 0, colors->unk14->sunPrimColor[0],
            colors->unk14->sunPrimColor[1], colors->unk14->sunPrimColor[2],
            255);
        gDPSetEnvColor(gfx++, colors->unk14->sunEnvColor[0],
            colors->unk14->sunEnvColor[1], colors->unk14->sunEnvColor[2], 255);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++,
            ((s32)f_SunScreenX[cameraIndex] - colors->unk14->sunRadius + 1)
                << 2,
            ((s32)f_SunScreenY[cameraIndex] - colors->unk14->sunRadius + 1)
                << 2,
            ((s32)f_SunScreenX[cameraIndex] + colors->unk14->sunRadius) << 2,
            ((s32)f_SunScreenY[cameraIndex] + colors->unk14->sunRadius) << 2,
            G_TX_RENDERTILE, 0, 0, colors->unk14->sunTexelStep,
            colors->unk14->sunTexelStep);
        gDPPipeSync(gfx++);
      }
    }
  }

  *displayListHead = gfx;
}

/* The course's backdrop image, drawn behind the course in strips from the
 * horizon down */
#define COURSE_BACKDROP_STRIP_COUNT 16
#define COURSE_BACKDROP_STRIP_HEIGHT 4
#define BACKDROP_IMAGE_WIDTH 512
#define BACKDROP_IMAGE_WIDTH_MASK 9 /* The image's width is 1 << this */
#define BACKDROP_HORIZON_LEVEL 64.0f
#define STAFF_ROLL_BACKDROP_RISE 32
/* How far the backdrop scrolls as the camera turns once */
#define BACKDROP_YAW_TEXELS 49152.0f
#define BACKDROP_FIRST_CULLED_ROW (SCREEN_HEIGHT - SCREEN_BORDER + 1)
#define BACKDROP_ABOVE_SCREEN 32
#define FULL_VIEW_WIDTH (SCREEN_WIDTH - (2 * SCREEN_BORDER))
#define FULL_VIEW_HEIGHT (SCREEN_HEIGHT - (2 * SCREEN_BORDER))
extern s32 f_BackdropStripCount;
extern s32 f_BackdropStripHeight;
extern s32 f_BackdropImageWidth;
extern s32 f_BackdropWidthMask;

#ifdef NON_MATCHING
/* Draws the sky and, below it, the course's backdrop in strips that follow the
 * camera's yaw and pitch */
void DrawBackdrop(Gfx** displayListHead, CameraView* view, s32 cameraIndex) {
  Gfx* gfx;
  s32 isNibbleImage;
  s32 centreShift;
  s32 yawOffset;
  s32 y;
  s32 row;
  s32 imageRow;
  s32 texelWord;
  s32 texelByte;
  s32 bottom;
  s32 top;
  s32 texelStepX;
  s32 texelStepY;
  s32 textureX;
  f32 inverseZoom;
  f32 zoom;

  gfx = *displayListHead;
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_BAYER);
  centreShift = ((view->zoom * FULL_VIEW_HEIGHT) -
                    (view->viewportCenterY - (view->scissorY * 2))) *
      0.5f;
  DrawSky(&gfx, view->eye[1], 1, cameraIndex);
  isNibbleImage = FALSE;
  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_DISABLE);
  if (cameraIndex == 0) {
    UpdateAndDrawFireworks(&gfx);
  }

  yawOffset =
      g_SkyState.yaws[cameraIndex] * BACKDROP_YAW_TEXELS * (1 / (2 * M_PI));
  if (g_Scene == SCENE_STAFF_ROLL) {
    y = (s32)((BACKDROP_HORIZON_LEVEL - g_SkyState.horizonLevels[cameraIndex]) *
            view->zoom) -
        STAFF_ROLL_BACKDROP_RISE;
  } else {
    y = (BACKDROP_HORIZON_LEVEL - g_SkyState.horizonLevels[cameraIndex]) *
        view->zoom;
    if (cameraIndex == 0) {
      g_HorizonRow = y;
    }
  }

  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPLoadTLUT_pal256(gfx++, ((u32)g_CourseSkyImage + IMAGE_HEADER_SIZE) & ~7);
  f_BackdropStripCount = COURSE_BACKDROP_STRIP_COUNT;
  f_BackdropStripHeight = COURSE_BACKDROP_STRIP_HEIGHT;
  f_BackdropImageWidth = BACKDROP_IMAGE_WIDTH;
  f_BackdropWidthMask = BACKDROP_IMAGE_WIDTH_MASK;
  if (view->scissorWidth == FULL_VIEW_WIDTH &&
      view->scissorHeight == FULL_VIEW_HEIGHT) {
    /* The full screen copies the strips unscaled */
    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 255, 255, 255, 1);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, 0, 0);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_COPY);
    for (row = 0;; row++, y += f_BackdropStripHeight) {
      s32 clip;

      if (y >= BACKDROP_FIRST_CULLED_ROW) {
        break;
      }

      if (y < -f_BackdropStripHeight - BACKDROP_ABOVE_SCREEN) {
        continue;
      }

      if (row < f_BackdropStripCount - 1) {
        imageRow = row;
        texelByte = (f_BackdropImageWidth * row * f_BackdropStripHeight) +
            (LARGE_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE);
      } else {
        texelByte = (f_BackdropImageWidth * (f_BackdropStripCount - 1) *
                        f_BackdropStripHeight) +
            (LARGE_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE);
        imageRow = f_BackdropStripCount - 1;
      }

      texelWord = texelByte / 2;
      if (!isNibbleImage) {
        gDPLoadTextureBlock(gfx++,
            (u32)((u16*)g_CourseSkyImage + PALETTE_START + texelWord) & ~7,
            G_IM_FMT_CI, G_IM_SIZ_8b, f_BackdropImageWidth,
            f_BackdropStripHeight, 0, G_TX_WRAP, G_TX_WRAP, f_BackdropWidthMask,
            0, 0, 0);
      } else {
        gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
            (u32)((u16*)g_CourseSkyImage + texelWord) & ~7);
        gDPLoadSync(gfx++);
        gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0,
            (((f_BackdropImageWidth * f_BackdropStripHeight) + 3) >> 2) - 1,
            CALC_DXT_4b(f_BackdropImageWidth));
        gDPPipeSync(gfx++);
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_4b,
            ((f_BackdropImageWidth >> 1) + 7) >> 3, 0, G_TX_RENDERTILE,
            imageRow, G_TX_WRAP, 0, 0, G_TX_WRAP, f_BackdropWidthMask, 0);
      }

      clip = 0;
      if (y < 0) {
        clip = -y;
      }

      if (row < f_BackdropStripCount - 1) {
        bottom = f_BackdropStripHeight + y + SCREEN_BORDER - 1;
        if (bottom > SCREEN_HEIGHT - SCREEN_BORDER) {
          bottom = SCREEN_HEIGHT - SCREEN_BORDER;
        }

        gSPTextureRectangle(gfx++, SCREEN_BORDER << 2,
            (SCREEN_BORDER + clip + y) << 2,
            (SCREEN_WIDTH - SCREEN_BORDER - 1) << 2, bottom << 2,
            G_TX_RENDERTILE, yawOffset, clip << 5, 4 << 10, TEXEL_STEP_ONE);
      } else {
        /* The last strip stretches to the bottom of the screen */
        gSPTextureRectangle(gfx++, SCREEN_BORDER << 2,
            (SCREEN_BORDER + clip + y) << 2,
            (SCREEN_WIDTH - SCREEN_BORDER - 1) << 2,
            (SCREEN_HEIGHT - SCREEN_BORDER) << 2, G_TX_RENDERTILE, yawOffset,
            clip << 5, 4 << 10, TEXEL_STEP_ONE);
        break;
      }
    }
  } else {
    /* A smaller view scales the strips by its zoom */
    inverseZoom = 1.0f / view->zoom;
    texelStepX = inverseZoom * 1024.0f;
    texelStepY = inverseZoom * 2048.0f;
    if (f_IsPrimitiveDepthSet) {
      gDPSetPrimDepth(gfx++, 0x7FF3, 0);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
    } else {
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    }

    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    zoom = view->zoom;
    textureX = yawOffset +
        (s32)(((zoom * FULL_VIEW_WIDTH) - view->scissorWidth) * 16.0f / zoom);
    top = y + view->scissorY - centreShift;
    bottom =
        (f32)y + (f_BackdropStripHeight * zoom) + view->scissorY - centreShift;
    for (row = 0;; row++, top = bottom,
        bottom = bottom + (f_BackdropStripHeight * view->zoom)) {
      if (top < view->scissorY - (f_BackdropStripHeight * view->zoom)) {
        continue;
      }

      if (row < f_BackdropStripCount - 1) {
        imageRow = row;
        texelByte = (f_BackdropImageWidth * row * f_BackdropStripHeight) +
            (LARGE_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE);
      } else {
        texelByte = (f_BackdropImageWidth * (f_BackdropStripCount - 1) *
                        f_BackdropStripHeight) +
            (LARGE_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE);
        imageRow = f_BackdropStripCount - 1;
      }

      texelWord = texelByte / 2;
      if (!isNibbleImage) {
        gDPLoadTextureBlock(gfx++,
            (u32)((u16*)g_CourseSkyImage + texelWord) & ~7, G_IM_FMT_CI,
            G_IM_SIZ_8b, f_BackdropImageWidth, f_BackdropStripHeight, 0,
            G_TX_WRAP, G_TX_WRAP, f_BackdropWidthMask, 0, 0, 0);
      } else {
        gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
            (u32)((u16*)g_CourseSkyImage + texelWord) & ~7);
        gDPLoadSync(gfx++);
        gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0,
            (((f_BackdropImageWidth * f_BackdropStripHeight) + 3) >> 2) - 1,
            CALC_DXT_4b(f_BackdropImageWidth));
        gDPPipeSync(gfx++);
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_4b,
            ((f_BackdropImageWidth >> 1) + 7) >> 3, 0, G_TX_RENDERTILE,
            imageRow, G_TX_WRAP, 0, 0, G_TX_WRAP, f_BackdropWidthMask, 0);
      }

      if (row < f_BackdropStripCount - 1) {
        if (bottom > view->scissorY + view->scissorHeight) {
          bottom = view->scissorY + view->scissorHeight;
        }
      } else {
        bottom = view->scissorY + view->scissorHeight;
      }

      if (bottom > view->scissorY + view->scissorHeight) {
        bottom = view->scissorY + view->scissorHeight;
      }

      gSPTextureRectangle(gfx++, view->scissorX << 2, top << 2,
          (view->scissorX + view->scissorWidth) << 2, bottom << 2,
          G_TX_RENDERTILE, textureX, 0, texelStepX, texelStepY);
      if (bottom == view->scissorY + view->scissorHeight) {
        break;
      }
    }
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PIXEL);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_display", DrawBackdrop);
#endif

/* Starts one of the HUD's animations, which lasts a given number of frames, or
 * stops it with HUD_ANIMATION_NONE */
void StartHudAnimation(s32 animation) {
  if (animation == HUD_ANIMATION_NONE) {
    f_IsFrameCapturePending = FALSE;
    f_HudAnimationFramesLeft = 0;
    return;
  }

  if (animation != HUD_ANIMATION_SLICE_CAPTURE &&
      animation != HUD_ANIMATION_SLICES) {
    g_CapturedSliceCount = 0;
  }

  switch (animation) {
    case HUD_ANIMATION_UNK0:
      f_HudAnimationFramesLeft = 352;
      break;
    case HUD_ANIMATION_FADE_OUT:
    case HUD_ANIMATION_FAST_FADE_OUT:
      f_HudAnimationFramesLeft = 255;
      break;
    case HUD_ANIMATION_TURN_AWAY:
      f_HudAnimationFramesLeft = 255;
      break;
    case HUD_ANIMATION_SLIDE_APART:
      f_HudAnimationFramesLeft = 160;
      break;
    case HUD_ANIMATION_SPIRAL:
      f_HudAnimationFramesLeft = 72;
      break;
    case HUD_ANIMATION_TILES:
      f_HudAnimationFramesLeft = 255;
      break;
    case HUD_ANIMATION_SLICES:
      f_HudAnimationFramesLeft = 64;
      break;
    case HUD_ANIMATION_NEGATIVE:
    case HUD_ANIMATION_MOTION_BLUR:
      f_HudAnimationFramesLeft = 32;
      break;
  }

  f_HudAnimation = animation;
  f_IsFrameCapturePending = TRUE;
  f_HudAnimationFrame = 0;
}

void HandleFrameBufferSwap(void) {}

/*
 * The fading animations draw f_CapturedFrame, which holds the frame inside
 * the border, in strips. Each strip is loaded as a block, and the tiles are
 * set up with the first.
 */
#define FADE_FRAME_WIDTH (SCREEN_WIDTH - (2 * BACKDROP_BORDER))
#define FADE_FRAME_WORDS_PER_ROW (FADE_FRAME_WIDTH / 2)
#define FADE_STEP 12
#define FAST_FADE_STEP 24
#define NEGATIVE_BRIGHTNESS 127
#define MOTION_BLUR_ALPHA 95
/* Blends nothing, but forces blending on */
#define RM_FORCED_PASS \
  GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1) | FORCE_BL
#define RM_FORCED_PASS2 GBL_c2(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1)
#define FADE_COUNT_PERIOD 8

#define LOAD_FADE_FRAME_STRIP(gfx, texels, height, isTileSet) \
  { \
    if (!(isTileSet)) { \
      gDPSetTextureImage( \
          gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, (u32)(texels) & ~7); \
      gDPSetTile(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, \
          0, 0, 0, 0, 0, 0); \
      gDPLoadSync(gfx++); \
      gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, \
          ((height) * FADE_FRAME_WIDTH) - 1, \
          CALC_DXT(FADE_FRAME_WIDTH, G_IM_SIZ_16b_BYTES)); \
      gDPPipeSync(gfx++); \
      gDPSetTile(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, \
          ((FADE_FRAME_WIDTH * G_IM_SIZ_16b_BYTES) + 7) >> 3, 0, \
          G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0); \
      gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, \
          (FADE_FRAME_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC, \
          ((height) - 1) << G_TEXTURE_IMAGE_FRAC); \
      isTileSet = TRUE; \
    } else { \
      gDPSetTextureImage( \
          gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, (u32)(texels) & ~7); \
      gDPLoadSync(gfx++); \
      gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, \
          ((height) * FADE_FRAME_WIDTH) - 1, \
          CALC_DXT(FADE_FRAME_WIDTH, G_IM_SIZ_16b_BYTES)); \
      gDPPipeSync(gfx++); \
    } \
  }

extern s32 D_800C7308;

/*
 * Draws the HUD animations that show the whole frame: fading it out,
 * drawing it at half brightness, blurring the scene with the last frame, or
 * splitting the frame into halves that slide apart. When a fade ends,
 * D_800C7308 counts up, wrapping at FADE_COUNT_PERIOD.
 *
 * Doesn't match: registers are allocated differently, and f_CapturedFrame is
 * kept in a saved register and on the stack.
 */
#ifdef NON_MATCHING
void DrawFadeAnimation(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  u32* frame = f_CapturedFrame;
  s32 isTileSet;
  s32 strip;
  s32 row;
  s32 height;
  u16* source;

  if (f_HudAnimationFramesLeft > 0) {
    isTileSet = FALSE;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    if (g_MenuBackdropStyle == MENU_BACKDROP_STYLE_NONE) {
      if (f_HudAnimation == HUD_ANIMATION_NEGATIVE) {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, RM_FORCED_PASS, RM_FORCED_PASS2);
        gDPSetPrimColor(gfx++, 0, 0, NEGATIVE_BRIGHTNESS, NEGATIVE_BRIGHTNESS,
            NEGATIVE_BRIGHTNESS, 255);
      } else if (f_HudAnimation == HUD_ANIMATION_FADE_OUT |
          f_HudAnimation == HUD_ANIMATION_FAST_FADE_OUT) {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, f_HudAnimationFramesLeft);
      } else if (f_HudAnimation == HUD_ANIMATION_MOTION_BLUR) {
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, MOTION_BLUR_ALPHA);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        gDPSetCombineMode(gfx++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
        source = f_LastFrameBuffer;
        for (strip = 0; strip != BACKDROP_STRIP_COUNT; strip++) {
          height = STRIP_HEIGHT(strip);
          gDPLoadTextureTile(gfx++, (u32)source & ~7, G_IM_FMT_RGBA,
              G_IM_SIZ_16b, SCREEN_WIDTH, 0, BACKDROP_BORDER, BACKDROP_BORDER,
              BACKDROP_RIGHT, BACKDROP_BORDER + height - 1, 0,
              G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
              G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gSPScisTextureRectangle(gfx++, BACKDROP_BORDER << 2,
              (strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER) << 2,
              (BACKDROP_RIGHT + 1) << 2,
              (strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER + height) << 2,
              G_TX_RENDERTILE, BACKDROP_BORDER << 5, BACKDROP_BORDER << 5,
              1 << 10, 1 << 10);
          source += BACKDROP_STRIP_STEP * SCREEN_WIDTH;
        }

        *displayListHead = gfx;
        return;
      } else if (f_HudAnimation == HUD_ANIMATION_SLIDE_APART |
          f_HudAnimation == HUD_ANIMATION_SPIRAL) {
        /* DrawHudAnimation draws HUD_ANIMATION_SPIRAL with DrawSpiralAnimation
         * instead */
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, RM_FORCED_PASS, RM_FORCED_PASS2);
        gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
        gDPPipeSync(gfx++);
        for (strip = 0; strip != BACKDROP_STRIP_COUNT; strip++) {
          height = STRIP_HEIGHT(strip);
          LOAD_FADE_FRAME_STRIP(gfx,
              &frame[strip * BACKDROP_STRIP_STEP * FADE_FRAME_WORDS_PER_ROW],
              height, isTileSet);
          gDPPipeSync(gfx++);
          gSPScisTextureRectangle(gfx++, BACKDROP_BORDER << 2,
              (strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER) << 2,
              f_HudAnimationFramesLeft << 2,
              (strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER + height) << 2,
              G_TX_RENDERTILE,
              ((SCREEN_WIDTH / 2) - f_HudAnimationFramesLeft) << 5, 0, 1 << 10,
              1 << 10);
          gSPScisTextureRectangle(gfx++,
              (SCREEN_WIDTH - f_HudAnimationFramesLeft) << 2,
              (strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER) << 2,
              (BACKDROP_RIGHT + 1) << 2,
              (strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER + height) << 2,
              G_TX_RENDERTILE, (FADE_FRAME_WIDTH / 2) << 5, 0, 1 << 10,
              1 << 10);
          gDPPipeSync(gfx++);
        }

        f_HudAnimationFramesLeft -= FADE_STEP;
        *displayListHead = gfx;
        return;
      }
    }

    gDPSetCombineMode(gfx++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
    gDPPipeSync(gfx++);
    for (row = BACKDROP_BORDER; row < SCREEN_HEIGHT - BACKDROP_BORDER;
        row += BACKDROP_STRIP_STEP) {
      height = row + BACKDROP_STRIP_STEP <= SCREEN_HEIGHT - BACKDROP_BORDER
          ? BACKDROP_STRIP_HEIGHT
          : BACKDROP_LAST_STRIP_HEIGHT;
      LOAD_FADE_FRAME_STRIP(gfx, frame, height, isTileSet);
      gSPTextureRectangle(gfx++, BACKDROP_BORDER << 2, row << 2,
          (BACKDROP_RIGHT + 1) << 2, (row + height) << 2, G_TX_RENDERTILE, 0, 0,
          1 << 10, 1 << 10);
      frame += BACKDROP_STRIP_STEP * FADE_FRAME_WORDS_PER_ROW;
    }

    if (f_HudAnimation == HUD_ANIMATION_FAST_FADE_OUT) {
      f_HudAnimationFramesLeft -= FAST_FADE_STEP;
    } else {
      f_HudAnimationFramesLeft -= FADE_STEP;
    }

    if (f_HudAnimationFramesLeft < 0) {
      D_800C7308 = (D_800C7308 + 1) % FADE_COUNT_PERIOD;
    }
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_display", DrawFadeAnimation);
#endif

/*
 * Draws HUD_ANIMATION_SPIRAL, where the frame is cut into a grid of tiles. It
 * draws the tiles along a spiral from the top left corner inwards, and draws
 * fewer of them each frame, so the frame disappears from the centre out. The
 * few tiles after the opaque ones fade, more the further in they are.
 */
void DrawSpiralAnimation(Gfx** displayListHead) {
  s32 tileColumn;
  s32 rightColumn;
  s32 topRow;
  s32 direction;
  s32 tileRow;
  s32 bottomRow;
  s32 leftColumn;
  Gfx* gfx = *displayListHead;
  u32* frame = f_CapturedFrame;
  s32 tileCount;
  s32 tileIndex;
  s32 alpha;

  /* Steps to the next tile of the spiral, turning at each edge and moving the
   * edge in */
  void StepSpiral(void) {
    switch (direction) {
      case SPIRAL_RIGHT:
        tileColumn++;
        if (tileColumn == rightColumn) {
          topRow++;
          direction++;
        }

        break;
      case SPIRAL_DOWN:
        tileRow++;
        if (tileRow == bottomRow) {
          rightColumn--;
          direction++;
        }

        break;
      case SPIRAL_LEFT:
        tileColumn--;
        if (tileColumn == leftColumn) {
          bottomRow--;
          direction++;
        }

        break;
      case SPIRAL_UP:
        tileRow--;
        if (tileRow == topRow) {
          direction = SPIRAL_RIGHT;
          leftColumn++;
        }

        break;
    }
  }

  if (f_HudAnimationFramesLeft > 0) {
    tileCount = f_HudAnimationFramesLeft - SPIRAL_FADING_TILE_COUNT;
    alpha = SPIRAL_OPAQUE_ALPHA;
    bottomRow = SPIRAL_GRID_SIZE - 1;
    rightColumn = SPIRAL_GRID_SIZE - 1;
    topRow = 0;
    leftColumn = 0;
    direction = SPIRAL_RIGHT;
    tileColumn = 0;
    tileRow = 0;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
    if (tileCount > SPIRAL_TILE_COUNT) {
      tileCount = SPIRAL_TILE_COUNT;
    }

    for (tileIndex = 0; tileIndex != tileCount; tileIndex++) {
      gDPLoadTextureTile(gfx++, (u32)frame & ~7, G_IM_FMT_RGBA, G_IM_SIZ_16b,
          SPIRAL_FRAME_WIDTH, 0, tileColumn * SPIRAL_TILE_WIDTH,
          tileRow * SPIRAL_TILE_HEIGHT,
          (tileColumn * SPIRAL_TILE_WIDTH) + SPIRAL_TILE_WIDTH - 1,
          (tileRow * SPIRAL_TILE_HEIGHT) + SPIRAL_TILE_HEIGHT - 1, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPTextureRectangle(gfx++,
          ((tileColumn * SPIRAL_TILE_WIDTH) + FRAME_MARGIN_ROWS) << 2,
          ((tileRow * SPIRAL_TILE_HEIGHT) + FRAME_MARGIN_ROWS) << 2,
          ((tileColumn * SPIRAL_TILE_WIDTH) + FRAME_MARGIN_ROWS +
              SPIRAL_TILE_WIDTH)
              << 2,
          ((tileRow * SPIRAL_TILE_HEIGHT) + FRAME_MARGIN_ROWS +
              SPIRAL_TILE_HEIGHT)
              << 2,
          G_TX_RENDERTILE, (tileColumn * SPIRAL_TILE_WIDTH) << 5,
          (tileRow * SPIRAL_TILE_HEIGHT) << 5, 1 << 10, 1 << 10);
      gDPPipeSync(gfx++);
      StepSpiral();
    }

    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE, 0, 0, 0,
        TEXEL0, 0, 0, 0, PRIMITIVE);
    tileCount += SPIRAL_FADING_TILE_COUNT;
    if (tileCount > SPIRAL_TILE_COUNT) {
      tileCount = SPIRAL_TILE_COUNT;
    }

    for (; tileIndex != tileCount; tileIndex++) {
      alpha -= SPIRAL_ALPHA_STEP;
      gDPLoadTextureTile(gfx++, (u32)frame & ~7, G_IM_FMT_RGBA, G_IM_SIZ_16b,
          SPIRAL_FRAME_WIDTH, 0, tileColumn * SPIRAL_TILE_WIDTH,
          tileRow * SPIRAL_TILE_HEIGHT,
          (tileColumn * SPIRAL_TILE_WIDTH) + SPIRAL_TILE_WIDTH - 1,
          (tileRow * SPIRAL_TILE_HEIGHT) + SPIRAL_TILE_HEIGHT - 1, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPSetPrimColor(gfx++, 0, 0, SPIRAL_OPAQUE_ALPHA, SPIRAL_OPAQUE_ALPHA,
          SPIRAL_OPAQUE_ALPHA, alpha);
      gDPPipeSync(gfx++);
      gSPTextureRectangle(gfx++,
          ((tileColumn * SPIRAL_TILE_WIDTH) + FRAME_MARGIN_ROWS) << 2,
          ((tileRow * SPIRAL_TILE_HEIGHT) + FRAME_MARGIN_ROWS) << 2,
          ((tileColumn * SPIRAL_TILE_WIDTH) + FRAME_MARGIN_ROWS +
              SPIRAL_TILE_WIDTH)
              << 2,
          ((tileRow * SPIRAL_TILE_HEIGHT) + FRAME_MARGIN_ROWS +
              SPIRAL_TILE_HEIGHT)
              << 2,
          G_TX_RENDERTILE, (tileColumn * SPIRAL_TILE_WIDTH) << 5,
          (tileRow * SPIRAL_TILE_HEIGHT) << 5, 1 << 10, 1 << 10);
      gDPPipeSync(gfx++);
      StepSpiral();
    }

    f_HudAnimationFramesLeft -= SPIRAL_FADING_TILE_COUNT;
  }

  *displayListHead = gfx;
}

/* Shows a menu's backdrop in place of the course, or MENU_BACKDROP_NONE to show
 * the course again */
void SetMenuBackdrop(s32 backdrop) {
  if (backdrop == MENU_BACKDROP_NONE) {
    g_MenuBackdropStyle = MENU_BACKDROP_STYLE_NONE;
    return;
  }

  if (backdrop == MENU_BACKDROP_PATTERN) {
    f_MenuBackdrop = backdrop;
    g_MenuBackdropStyle = MENU_BACKDROP_STYLE_UNK2;
    return;
  }

  g_MenuBackdropStyle = MENU_BACKDROP_STYLE_PLAIN;
  f_MenuBackdrop = backdrop;
}

/* Starts the plain menu backdrop by capturing the last frame for it */
#ifdef NON_MATCHING
void CaptureMenuBackdrop(Gfx** displayListHead, s32 unused) {
  Gfx* gfx = *displayListHead;
  s32 strip;
  s32 height;
  u16* source;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_COPY);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_DISABLE);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  if (g_MenuBackdropStyle == MENU_BACKDROP_STYLE_PLAIN) {
    g_MenuBackdropStyle = MENU_BACKDROP_STYLE_UNK2;
    f_BackdropFadeLevel = BACKDROP_START_FADE_LEVEL;
    gDPPipeSync(gfx++);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
        OS_K0_TO_PHYSICAL(f_CapturedFrame) & ~7);
    gDPPipeSync(gfx++);
    source = f_LastFrameBuffer;
    for (strip = 0; strip != BACKDROP_STRIP_COUNT; strip++) {
      height = STRIP_HEIGHT(strip);
      gDPLoadTextureTile(gfx++, (u32)source & ~7, G_IM_FMT_RGBA, G_IM_SIZ_16b,
          SCREEN_WIDTH, 0, BACKDROP_BORDER, BACKDROP_BORDER, BACKDROP_RIGHT,
          BACKDROP_BORDER + height - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
          G_TX_NOLOD);
      gSPScisTextureRectangle(gfx++, BACKDROP_BORDER << 2,
          (strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER) << 2,
          BACKDROP_RIGHT << 2,
          (strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER + height - 1) << 2,
          G_TX_RENDERTILE, BACKDROP_BORDER << 5, BACKDROP_BORDER << 5,
          COPY_MODE_DSDX, 1 << 10);
      gDPPipeSync(gfx++);
      source += BACKDROP_STRIP_STEP * SCREEN_WIDTH;
    }

    gDPPipeSync(gfx++);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH,
        OS_K0_TO_PHYSICAL(nuGfxCfb_ptr) & ~7);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_display", CaptureMenuBackdrop);
#endif

/* MENU_BACKDROP_TINTED's color */
#define BACKDROP_TINT_RED 144
#define BACKDROP_TINT_GREEN 104
#define BACKDROP_TINT_BLUE 32
/* MENU_BACKDROP_PATTERN's image, which scrolls diagonally by a texel a frame */
#define BACKDROP_PATTERN_SIZE 64
#define BACKDROP_PATTERN_MASK 6 /* log2(BACKDROP_PATTERN_SIZE) */
extern s32 f_BackdropPatternScroll;

/*
 * Draws the menu backdrop that SetMenuBackdrop chose, inside the backdrop's
 * border.
 *
 * Doesn't match: registers are allocated differently in the pattern's display
 * list, and the strip height is chosen with a branch instead of without one.
 */
#ifdef NON_MATCHING
void DrawMenuBackdrop(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 strip;
  s32 height;

  if (f_BackdropFadeLevel > BACKDROP_LAST_FADE_LEVEL) {
    f_BackdropFadeLevel -= BACKDROP_FADE_STEP;
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetCombineMode(gfx++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
  if (f_MenuBackdrop == MENU_BACKDROP_PATTERN) {
    f_BackdropPatternScroll++;
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(gfx++,
        ((u32)g_BackdropPatternImage + PALETTED_TEXELS_OFFSET) & ~7,
        G_IM_FMT_CI, BACKDROP_PATTERN_SIZE, BACKDROP_PATTERN_SIZE, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
        BACKDROP_PATTERN_MASK, BACKDROP_PATTERN_MASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(
        gfx++, 0, ((u32)g_BackdropPatternImage + IMAGE_HEADER_SIZE) & ~7);
    gDPPipeSync(gfx++);
    gSPTextureRectangle(gfx++, BACKDROP_BORDER << 2, BACKDROP_BORDER << 2,
        (SCREEN_WIDTH - BACKDROP_BORDER) << 2,
        (SCREEN_HEIGHT - BACKDROP_BORDER) << 2, G_TX_RENDERTILE,
        f_BackdropPatternScroll << 5, f_BackdropPatternScroll << 5, 1 << 10,
        1 << 10);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
  } else if (f_MenuBackdrop == MENU_BACKDROP_STILL |
      f_MenuBackdrop == MENU_BACKDROP_DIMMING) {
    if (f_MenuBackdrop == MENU_BACKDROP_STILL) {
      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    } else {
      gDPSetPrimColor(gfx++, 0, 0, (u8)f_BackdropFadeLevel,
          (u8)f_BackdropFadeLevel, (u8)f_BackdropFadeLevel, 255);
    }

    for (strip = 0; strip != BACKDROP_STRIP_COUNT; strip++) {
      height = STRIP_HEIGHT(strip);
      gDPLoadTextureTile(gfx++,
          (u32)&f_CapturedFrame[strip * BACKDROP_STRIP_STEP *
              FRAME_WORDS_PER_ROW] &
              ~7,
          G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_WIDTH, 0, BACKDROP_BORDER,
          BACKDROP_BORDER, BACKDROP_RIGHT, BACKDROP_BORDER + height - 1, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gSPScisTextureRectangle(gfx++, BACKDROP_BORDER << 2,
          (strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER) << 2,
          (BACKDROP_RIGHT + 1) << 2,
          (strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER + height) << 2,
          G_TX_RENDERTILE, BACKDROP_BORDER << 5, BACKDROP_BORDER << 5, 1 << 10,
          1 << 10);
      gDPPipeSync(gfx++);
    }
  } else {
    gDPSetPrimColor(gfx++, 0, 0, BACKDROP_TINT_RED, BACKDROP_TINT_GREEN,
        BACKDROP_TINT_BLUE, 255);
    for (strip = 0; strip != BACKDROP_STRIP_COUNT; strip++) {
      height = STRIP_HEIGHT(strip);
      gDPLoadTextureTile(gfx++,
          (u32)&f_CapturedFrame[strip * BACKDROP_STRIP_STEP *
              FRAME_WORDS_PER_ROW] &
              ~7,
          G_IM_FMT_IA, G_IM_SIZ_16b, SCREEN_WIDTH, 0, BACKDROP_BORDER,
          BACKDROP_BORDER, BACKDROP_RIGHT, BACKDROP_BORDER + height - 1, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gSPScisTextureRectangle(gfx++, BACKDROP_BORDER << 2,
          (strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER) << 2,
          (BACKDROP_RIGHT + 1) << 2,
          (strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER + height) << 2,
          G_TX_RENDERTILE, BACKDROP_BORDER << 5, BACKDROP_BORDER << 5, 1 << 10,
          1 << 10);
      gDPPipeSync(gfx++);
    }
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_display", DrawMenuBackdrop);
#endif

/* Draws HUD_ANIMATION_TILES, where the frame's tiles turn over and fly apart */
#ifdef NON_MATCHING
void DrawTileAnimation(void) {
  Vec3 rotation;
  Vec3 translation;
  Mat4 matrix;
  f32 angle;
  f32 unusedSine;
  f32 unusedCosine;
  s32 progress;
  s32 tileRow;
  s32 tileColumn;
  s32 wordIndex = 0;

  if (f_HudAnimationFramesLeft > 0) {
    angle = (f32)((M_PI / 2) -
                (f_HudAnimationFramesLeft *
                    (M_PI / 2 / TILE_ANIMATION_FRAME_COUNT))) +
        TILE_ANIMATION_UNUSED_ANGLE_OFFSET;
    unusedSine = sinf(angle);
    unusedCosine = cosf(angle);
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
    gDPPipeSync(g_DisplayListHead++);
    gSPClearGeometryMode(g_DisplayListHead++, 0xFFFFFFFF);
    gSPSetGeometryMode(
        g_DisplayListHead++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetRenderMode(g_DisplayListHead++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gSPPerspNormalize(g_DisplayListHead++, g_ScreenEffectPerspNorm);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
    gDPSetCombineMode(g_DisplayListHead++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
    gSPMatrix(g_DisplayListHead++, OS_K0_TO_PHYSICAL(&g_ScreenEffectProjection),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gDPPipeSync(g_DisplayListHead++);
    rotation[AXIS_X] = 0.0f;
    rotation[AXIS_Y] = (M_PI / 2) -
        (f_HudAnimationFramesLeft * (M_PI / 2 / TILE_ANIMATION_FRAME_COUNT));
    rotation[AXIS_Z] = rotation[AXIS_Y];
    for (tileRow = 0; tileRow != FRAME_TILE_ROW_COUNT; tileRow++) {
      for (tileColumn = 0; tileColumn != FRAME_TILE_COLUMN_COUNT;
          tileColumn++) {
        progress = TILE_ANIMATION_FRAME_COUNT - f_HudAnimationFramesLeft;
        translation[AXIS_Z] =
            TILE_ANIMATION_DEPTH - (progress * TILE_ANIMATION_SPEED_Z);
        translation[AXIS_X] =
            ((progress + TILE_ANIMATION_GAP_X) *
                (tileColumn - (FRAME_TILE_COLUMN_COUNT / 2))) +
            TILE_ANIMATION_OFFSET_X;
        translation[AXIS_Y] = -(progress + TILE_ANIMATION_GAP_Y) *
            (tileRow - (FRAME_TILE_ROW_COUNT / 2));
        BuildRotationMatrix(matrix, rotation, translation);
        ConvertMatrixToFixed(matrix,
            &f_TileMatrices[g_MatrixBufferIndex]
                           [(tileRow * FRAME_TILE_COLUMN_COUNT) + tileColumn]);
        gSPMatrix(g_DisplayListHead++,
            OS_K0_TO_PHYSICAL(&f_TileMatrices[g_MatrixBufferIndex][tileRow *
                    FRAME_TILE_COLUMN_COUNT +
                tileColumn]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        if (tileColumn == FRAME_TILE_COLUMN_COUNT - 1) {
          gSPVertex(g_DisplayListHead++, f_LastColumnTileVertices,
              TILE_VERTEX_COUNT, 0);
        } else {
          gSPVertex(g_DisplayListHead++, f_TileVertices, TILE_VERTEX_COUNT, 0);
        }

        gDPLoadTextureBlock(g_DisplayListHead++,
            (u32)&f_CapturedFrame[wordIndex] & ~7, G_IM_FMT_RGBA, G_IM_SIZ_16b,
            FRAME_TILE_WIDTH, FRAME_TILE_ROWS, 0, G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
            G_TX_NOLOD);
        wordIndex += FRAME_TILE_WORD_COUNT;
        gSP2Triangles(g_DisplayListHead++, 0, 1, 2, 0, 0, 2, 3, 0);
        gDPPipeSync(g_DisplayListHead++);
      }
    }

    gDPPipeSync(g_DisplayListHead++);
    f_HudAnimationFramesLeft -= TILE_ANIMATION_STEP;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_display", DrawTileAnimation);
#endif

/*
 * HUD_ANIMATION_TURN_AWAY turns the whole frame away about two axes as it
 * moves away from the camera. The frame's tiles are drawn on the tile grid,
 * the upper rows from one load of its vertices and the rest from another;
 * only the upper rows have a back, drawn in their shade.
 */
#define TURN_AWAY_UPPER_ROW_COUNT 4
#define TURN_AWAY_LOWER_ROW_COUNT \
  (FRAME_TILE_ROW_COUNT - TURN_AWAY_UPPER_ROW_COUNT)
/* The last column's texture coordinates span this many texels */
#define LAST_TILE_TEXEL_WIDTH 47
/* The lower rows' texture coordinates stop a quarter texel short, in 10.5 fixed
 * point */
#define LOWER_TILE_TEXCOORD_INSET 8
extern Mtx f_TurnAwayMatrices[2];

/*
 * Draws HUD_ANIMATION_TURN_AWAY.
 *
 * Doesn't match: the target computes each corner's vertex index from its own
 * per-row induction variable, and registers are allocated differently.
 */
#ifdef NON_MATCHING
void DrawTurnAwayAnimation(void) {
  Vec3 rotation;
  Vec3 translation;
  Mat4 matrix;
  s32 wordIndex = 0;
  s32 row;
  s32 column;
  s32 width;
  s32 vertex;

  if (f_HudAnimationFramesLeft > 0) {
    rotation[AXIS_X] = 0.0f;
    rotation[AXIS_Y] = (M_PI / 2) -
        (f_HudAnimationFramesLeft * (M_PI / 2 / TILE_ANIMATION_FRAME_COUNT));
    rotation[AXIS_Z] = rotation[AXIS_Y];
    translation[AXIS_X] = 0.0f;
    translation[AXIS_Y] = 0.0f;
    translation[AXIS_Z] = TILE_ANIMATION_DEPTH -
        (TILE_ANIMATION_FRAME_COUNT - f_HudAnimationFramesLeft);
    BuildRotationMatrix(matrix, rotation, translation);
    ConvertMatrixToFixed(matrix, &f_TurnAwayMatrices[g_MatrixBufferIndex]);
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
    gDPPipeSync(g_DisplayListHead++);
    gSPClearGeometryMode(g_DisplayListHead++, 0xFFFFFFFF);
    gSPSetGeometryMode(
        g_DisplayListHead++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetRenderMode(g_DisplayListHead++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gSPPerspNormalize(g_DisplayListHead++, g_ScreenEffectPerspNorm);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
    gSPMatrix(g_DisplayListHead++, OS_K0_TO_PHYSICAL(&g_ScreenEffectProjection),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(g_DisplayListHead++,
        OS_K0_TO_PHYSICAL(&f_TurnAwayMatrices[g_MatrixBufferIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPVertex(g_DisplayListHead++, f_TileGridVertices[g_MatrixBufferIndex],
        (TURN_AWAY_UPPER_ROW_COUNT + 1) * TILE_GRID_COLUMN_COUNT, 0);
    gDPSetCombineMode(g_DisplayListHead++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPPipeSync(g_DisplayListHead++);
    for (row = 0; row != TURN_AWAY_UPPER_ROW_COUNT; row++) {
      for (column = 0; column != FRAME_TILE_COLUMN_COUNT; column++) {
        gDPLoadTextureBlock(g_DisplayListHead++,
            (u32)&f_CapturedFrame[wordIndex] & ~7, G_IM_FMT_RGBA, G_IM_SIZ_16b,
            FRAME_TILE_WIDTH, FRAME_TILE_ROWS, 0, G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
            G_TX_NOLOD);
        wordIndex += FRAME_TILE_WORD_COUNT;
        width = FRAME_TILE_WIDTH;
        if (column == FRAME_TILE_COLUMN_COUNT - 1) {
          width = LAST_TILE_TEXEL_WIDTH;
        }

        vertex = (row * TILE_GRID_COLUMN_COUNT) + column;
        gSPModifyVertex(g_DisplayListHead++, vertex, G_MWO_POINT_ST, 0);
        gSPModifyVertex(g_DisplayListHead++, vertex + TILE_GRID_COLUMN_COUNT,
            G_MWO_POINT_ST, FRAME_TILE_ROWS << 5);
        gSPModifyVertex(g_DisplayListHead++,
            vertex + TILE_GRID_COLUMN_COUNT + 1, G_MWO_POINT_ST,
            (width << 21) + (FRAME_TILE_ROWS << 5));
        gSPModifyVertex(
            g_DisplayListHead++, vertex + 1, G_MWO_POINT_ST, width << 21);
        gDPPipeSync(g_DisplayListHead++);
        gSP2Triangles(g_DisplayListHead++, vertex,
            vertex + TILE_GRID_COLUMN_COUNT,
            vertex + TILE_GRID_COLUMN_COUNT + 1, 0, vertex,
            vertex + TILE_GRID_COLUMN_COUNT + 1, vertex + 1, 0);
        gDPPipeSync(g_DisplayListHead++);
      }
    }

    gDPPipeSync(g_DisplayListHead++);
    gDPSetCombineMode(g_DisplayListHead++, G_CC_SHADE, G_CC_SHADE);
    gDPPipeSync(g_DisplayListHead++);
    for (row = 0; row != TURN_AWAY_UPPER_ROW_COUNT; row++) {
      for (column = 0; column != FRAME_TILE_COLUMN_COUNT; column++) {
        vertex = (row * TILE_GRID_COLUMN_COUNT) + column;
        gSP2Triangles(g_DisplayListHead++, vertex + TILE_GRID_COLUMN_COUNT + 1,
            vertex + TILE_GRID_COLUMN_COUNT, vertex, 0, vertex + 1,
            vertex + TILE_GRID_COLUMN_COUNT + 1, vertex, 0);
      }
    }

    gDPPipeSync(g_DisplayListHead++);
    gDPSetCombineMode(g_DisplayListHead++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPPipeSync(g_DisplayListHead++);
    gSPVertex(g_DisplayListHead++,
        &f_TileGridVertices[g_MatrixBufferIndex]
                           [TURN_AWAY_UPPER_ROW_COUNT * TILE_GRID_COLUMN_COUNT],
        (TURN_AWAY_LOWER_ROW_COUNT + 1) * TILE_GRID_COLUMN_COUNT, 0);
    for (row = 0; row != TURN_AWAY_LOWER_ROW_COUNT; row++) {
      for (column = 0; column != FRAME_TILE_COLUMN_COUNT; column++) {
        gDPLoadTextureBlock(g_DisplayListHead++,
            (u32)&f_CapturedFrame[wordIndex] & ~7, G_IM_FMT_RGBA, G_IM_SIZ_16b,
            FRAME_TILE_WIDTH, FRAME_TILE_ROWS, 0, G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
            G_TX_NOLOD);
        wordIndex += FRAME_TILE_WORD_COUNT;
        width = FRAME_TILE_WIDTH;
        if (column == FRAME_TILE_COLUMN_COUNT - 1) {
          width = LAST_TILE_TEXEL_WIDTH;
        }

        vertex = (row * TILE_GRID_COLUMN_COUNT) + column;
        gDPPipeSync(g_DisplayListHead++);
        gSPModifyVertex(g_DisplayListHead++, vertex, G_MWO_POINT_ST, 0);
        gSPModifyVertex(g_DisplayListHead++, vertex + TILE_GRID_COLUMN_COUNT,
            G_MWO_POINT_ST, (FRAME_TILE_ROWS << 5) - LOWER_TILE_TEXCOORD_INSET);
        gSPModifyVertex(g_DisplayListHead++,
            vertex + TILE_GRID_COLUMN_COUNT + 1, G_MWO_POINT_ST,
            (((width << 5) - LOWER_TILE_TEXCOORD_INSET) << 16) +
                ((FRAME_TILE_ROWS << 5) - LOWER_TILE_TEXCOORD_INSET));
        gSPModifyVertex(g_DisplayListHead++, vertex + 1, G_MWO_POINT_ST,
            ((width << 5) - LOWER_TILE_TEXCOORD_INSET) << 16);
        gSP2Triangles(g_DisplayListHead++, vertex,
            vertex + TILE_GRID_COLUMN_COUNT,
            vertex + TILE_GRID_COLUMN_COUNT + 1, 0, vertex,
            vertex + TILE_GRID_COLUMN_COUNT + 1, vertex + 1, 0);
        gDPPipeSync(g_DisplayListHead++);
      }
    }

    gDPPipeSync(g_DisplayListHead++);
    gDPSetCombineMode(g_DisplayListHead++, G_CC_SHADE, G_CC_SHADE);
    gDPPipeSync(g_DisplayListHead++);
    f_HudAnimationFramesLeft -= TILE_ANIMATION_STEP;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_display", DrawTurnAwayAnimation);
#endif

/* Copies the frame that a HUD animation that has just started breaks apart */
#ifdef NON_MATCHING
void CaptureHudAnimationFrame(void) {
  u32* frame;
  s32 wordIndex;
  s32 tileRow;
  s32 tileColumn;
  s32 row;
  s32 column;
  Vtx* vertex;
  Vtx* nextVertex;
  s32 y;
  s32 x;

  if (!f_IsFrameCapturePending) {
    return;
  }

  frame = (u32*)nuGfxCfb_ptr;
  f_IsFrameCapturePending = FALSE;
  wordIndex = 0;
  if (f_HudAnimation == HUD_ANIMATION_TURN_AWAY ||
      f_HudAnimation == HUD_ANIMATION_TILES) {
    for (tileRow = 0; tileRow != FRAME_TILE_ROW_COUNT; tileRow++) {
      for (tileColumn = 0; tileColumn != FRAME_TILE_COLUMN_COUNT;
          tileColumn++) {
        for (row = FRAME_MARGIN_ROWS;
            row != FRAME_MARGIN_ROWS + FRAME_TILE_ROWS; row++) {
          for (column = FRAME_MARGIN_WORDS;
              column != FRAME_MARGIN_WORDS + FRAME_TILE_WORDS; column++) {
            f_CapturedFrame[wordIndex++] = frame[(row * FRAME_WORDS_PER_ROW) +
                column + (tileColumn * FRAME_TILE_WORDS) +
                (tileRow * FRAME_TILE_ROWS * FRAME_WORDS_PER_ROW)];
          }
        }
      }
    }

    vertex = f_TileGridVertices[0];
    nextVertex = f_TileGridVertices[1];
    for (row = 0, y = TILE_GRID_TOP; row != TILE_GRID_ROW_COUNT;
        row++, y -= TILE_GRID_ROW_SPACING) {
      for (column = 0, x = TILE_GRID_LEFT; column != TILE_GRID_COLUMN_COUNT - 1;
          column++, x += TILE_GRID_SPACING) {
        vertex->v.ob[0] = x;
        vertex->v.ob[1] = y;
        vertex->v.ob[2] = 0;
        nextVertex->v.ob[0] = x;
        nextVertex->v.ob[1] = y;
        nextVertex->v.ob[2] = 0;
        vertex++;
        nextVertex++;
      }

      /* The last column is narrower, ending inside the HUD's margin */
      for (column = TILE_GRID_COLUMN_COUNT - 1, x = TILE_GRID_RIGHT;
          column != TILE_GRID_COLUMN_COUNT; column++, x += TILE_GRID_SPACING) {
        vertex->v.ob[0] = x;
        vertex->v.ob[1] = y;
        vertex->v.ob[2] = 0;
        nextVertex->v.ob[0] = x;
        nextVertex->v.ob[1] = y;
        nextVertex->v.ob[2] = 0;
        vertex++;
        nextVertex++;
      }
    }
  } else if (f_HudAnimation == HUD_ANIMATION_FADE_OUT ||
      f_HudAnimation == HUD_ANIMATION_FAST_FADE_OUT ||
      f_HudAnimation == HUD_ANIMATION_SLIDE_APART ||
      f_HudAnimation == HUD_ANIMATION_SPIRAL) {
    for (row = FRAME_MARGIN_ROWS; row != SCREEN_HEIGHT - FRAME_MARGIN_ROWS;
        row++) {
      for (column = FRAME_MARGIN_WORDS;
          column != FRAME_WORDS_PER_ROW - FRAME_MARGIN_WORDS; column++) {
        f_CapturedFrame[wordIndex++] =
            frame[(row * FRAME_WORDS_PER_ROW) + column];
      }
    }
  } else if (f_HudAnimation == HUD_ANIMATION_SLICE_CAPTURE) {
    if (g_CapturedSliceCount < FRAME_SLICE_COUNT) {
      wordIndex = 0;
      for (row = FRAME_SLICE_TOP; row != FRAME_SLICE_BOTTOM; row++) {
        for (column = FRAME_SLICE_LEFT_WORD;
            column != FRAME_SLICE_RIGHT_WORD;) {
          f_CapturedFrame[(g_CapturedSliceCount * FRAME_SLICE_WORD_COUNT) +
              wordIndex++] = frame[(row * FRAME_WORDS_PER_ROW) + column++];
        }
      }

      g_CapturedSliceCount++;
    }
  } else if (f_HudAnimation == HUD_ANIMATION_MOTION_BLUR) {
  } else if (f_HudAnimation == HUD_ANIMATION_NEGATIVE) {
    for (row = FRAME_MARGIN_ROWS; row != SCREEN_HEIGHT - FRAME_MARGIN_ROWS;
        row++) {
      for (column = FRAME_MARGIN_WORDS;
          column != FRAME_WORDS_PER_ROW - FRAME_MARGIN_WORDS; column++) {
        f_CapturedFrame[wordIndex++] =
            ~frame[(row * FRAME_WORDS_PER_ROW) + column];
      }
    }
  }

  f_HudAnimationFrame = 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_display", CaptureHudAnimationFrame);
#endif

/* Draws the slice animation, which fills the screen with copies of its middle
 */
#ifdef NON_MATCHING
void DrawSliceAnimation(void) {
  s32 frame;
  s32 sliceCount;
  s32 slice;
  s32 strip;
  s32 column;
  s32 wordIndex;
  s32 sliceY;
  s32 y;

  if (f_HudAnimationFramesLeft <= 0) {
    return;
  }

  gDPPipeSync(g_DisplayListHead++);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
  gDPPipeSync(g_DisplayListHead++);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTexturePersp(g_DisplayListHead++, G_TP_NONE);
  gDPSetCombineMode(
      g_DisplayListHead++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
  gDPPipeSync(g_DisplayListHead++);
  column = 0;
  wordIndex = 0;
  frame = SLICE_ANIMATION_FRAME_COUNT - f_HudAnimationFramesLeft;
  sliceCount = (frame / SLICE_FRAME_COUNT) + 1;
  if (frame % SLICE_FRAME_COUNT == 0) {
    PlaySound(SOUND_SLICE, SLICE_SOUND_CHANNEL, SLICE_SOUND_VOLUME);
  }

  sliceY = 0;
  for (slice = 0; slice != sliceCount; slice++) {
    y = sliceY;
    for (strip = 0; strip != SLICE_STRIP_COUNT; strip++) {
      gDPLoadTextureBlock(g_DisplayListHead++,
          (u32)&f_CapturedFrame[wordIndex] & ~7, G_IM_FMT_RGBA, G_IM_SIZ_16b,
          SLICE_WIDTH, SLICE_STRIP_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
          G_TX_NOLOD);
      wordIndex += SLICE_STRIP_WORD_COUNT;
      gSPTextureRectangle(g_DisplayListHead++, column * SLICE_WIDTH << 2,
          y << 2, (column * SLICE_WIDTH + SLICE_WIDTH) << 2,
          (y + SLICE_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10,
          1 << 10);
      gDPPipeSync(g_DisplayListHead++);
      y += SLICE_STRIP_HEIGHT;
    }

    column++;
    if (column == SLICE_COLUMN_COUNT) {
      column = 0;
      sliceY += SLICE_HEIGHT;
    }
  }

  f_HudAnimationFramesLeft--;
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_display", DrawSliceAnimation);
#endif

/* Draws the animation that StartHudAnimation started over the frame */
void DrawHudAnimation(void) {
  CaptureHudAnimationFrame();
  gDPPipeSync(g_DisplayListHead++);
  gDPSetColorDither(g_DisplayListHead++, G_CD_DISABLE);
  switch (f_HudAnimation) {
    case HUD_ANIMATION_FADE_OUT:
    case HUD_ANIMATION_NEGATIVE:
    case HUD_ANIMATION_MOTION_BLUR:
    case HUD_ANIMATION_SLIDE_APART:
    case HUD_ANIMATION_FAST_FADE_OUT:
      DrawFadeAnimation(&g_DisplayListHead);
      break;
    case HUD_ANIMATION_TURN_AWAY:
      DrawTurnAwayAnimation();
      break;
    case HUD_ANIMATION_TILES:
      DrawTileAnimation();
      break;
    case HUD_ANIMATION_SLICES:
      DrawSliceAnimation();
      break;
    case HUD_ANIMATION_SPIRAL:
      DrawSpiralAnimation(&g_DisplayListHead);
      break;
  }

  f_LastFrameBuffer = nuGfxCfb_ptr;
}

/* Lets the HUD draw over the whole screen again */
void ResetHudScissor(void) {
  g_HudScissor.left = 0;
  g_HudScissor.right = SCREEN_WIDTH;
  g_HudScissor.bottom = SCREEN_HEIGHT;
  g_HudScissor.top = 0;
  gDPPipeSync(g_DisplayListHead++);
  gDPSetScissor(g_DisplayListHead++, G_SC_NON_INTERLACE, g_HudScissor.left,
      g_HudScissor.top, g_HudScissor.right, g_HudScissor.bottom);
}

/* Whether the animation that StartHudAnimation started has finished */
s32 IsHudAnimationDone(void) {
  return f_HudAnimationFramesLeft <= 0;
}
