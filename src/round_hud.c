#include "common.h"

#include "round_hud.h"

#include "asset.h"
#include "billboard.h"
#include "burst.h"
#include "camera.h"
#include "character.h"
#include "club_panel.h"
#include "controller.h"
#include "course.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "debug_text.h"
#include "effects.h"
#include "functions.h"
#include "game_flags.h"
#include "golf_balls.h"
#include "heap.h"
#include "hole_display.h"
#include "hole_objects.h"
#include "libc.h"
#include "math_util.h"
#include "movie.h"
#include "overlay_manager.h"
#include "pause_menu.h"
#include "pin.h"
#include "round_events.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "screen.h"
#include "screen_layers.h"
#include "shot_effects.h"
#include "sound.h"
#include "surface.h"
#include "text.h"
#include "variables.h"

/* f_SceneryState values */
#define SCENERY_UNLOADED 0
#define SCENERY_BILLBOARDS_READY 1 /* The billboard heap is set up */
/* The images of the hole's scenery are loaded too */
#define SCENERY_LOADED 2

/* Bytes of the billboard manager's heap */
#define BILLBOARD_HEAP_SIZE 0x7118

/* Bytes of each buffer the scenery images come with */
#define SCENERY_BUFFER_A_SIZE 0x128
/* Bytes of the buffer that StartShotPanel loads the lie's image into */
#define LIE_IMAGE_SIZE 0x1208

/* Images of the round's scenery, effects and HUD, named after their asset ids
 * until their users are decompiled */
#define ASSET_PIN_FLAG_IMAGES 0x541
#define ASSET_WIND_ICON_IMAGE 0x5A4
#define ASSET_PLAYER_TAG_IMAGES 0x5E3
#define ASSET_LEADER_LINE_IMAGE 0x972
#define ASSET_BACKDROP_PATTERN_IMAGE 0x93C
#define ASSET_SHOT_PREVIEW_DASH_IMAGE 0x4DB
#define ASSET_SHADOW_TEXTURE 0x4C3
#define ASSET_ROUND_IMAGE_722 0x722
#define ASSET_BLENDED_TEXTURE_SCROLLING_LAYER 0x4D7
#define ASSET_BLENDED_TEXTURE_STILL_LAYER 0x4D9
#define ASSET_DRIVER_HEAD_IMAGE 0x4E3
#define ASSET_ROUND_IMAGE_648 0x648
#define ASSET_ROUND_IMAGE_563 0x563
#define ASSET_ROUND_IMAGE_564 0x564
#define ASSET_ROUND_IMAGE_562 0x562
#define ASSET_SHOT_OFFSET_MARKER_IMAGE 0x57C
#define ASSET_WIND_SPEED_FONT 0x97B
#define ASSET_ROUND_IMAGE_725 0x725
#define ASSET_ROUND_IMAGE_568 0x568
#define ASSET_ROUND_IMAGE_56B 0x56B
#define ASSET_ROUND_IMAGE_56A 0x56A
#define ASSET_ROUND_IMAGE_569 0x569
#define ASSET_ROUND_IMAGE_723 0x723
#define ASSET_DISTANCE_SIGN_IMAGE 0x724

/* The image of the hole's par, one per par from 3 to 5 */
#define ASSET_PAR_THREE_IMAGE 0x565
#define ASSET_PAR_FOUR_IMAGE 0x566
#define ASSET_PAR_FIVE_IMAGE 0x567

/* The first asset of each course record's hole images, one per hole */
extern u16 f_HoleImageAssets[COURSE_RECORD_COUNT];
extern void* f_HoleImage;
#define HOLE_IMAGE_WIDTH 48
#define HOLE_IMAGE_HEIGHT 42
extern void* f_ParImage;

/* Set while the overlay of the course selection is loaded for the hole */
extern s32 f_IsCourseSelectLoaded;

/*
 * The hole's flag, which shows the hole image, is a grid of vertices. pin.c
 * draws it at the cup and DrawHoleImageModel draws it on the screen. Its
 * front and back sides each have one grid per matrix buffer; the back's
 * texture runs the other way.
 */
#define HOLE_FLAG_COLUMN_COUNT 6
#define HOLE_FLAG_ROW_COUNT 5
#define HOLE_FLAG_VERTEX_COUNT (HOLE_FLAG_COLUMN_COUNT * HOLE_FLAG_ROW_COUNT)
#define HOLE_FLAG_SIDE_COUNT 2
#define MATRIX_BUFFER_COUNT 2
#define HOLE_FLAG_CELL_SIZE 160
#define HOLE_FLAG_TEXEL_COLUMN_STEP 0x266 /* In s10.5 texture coordinates */
#define HOLE_FLAG_TEXEL_ROW_STEP 0x2A0
#define HOLE_FLAG_BACK_TEXEL_START 0xC00
#define HOLE_FLAG_START_SHADE 0xB7
#define HOLE_FLAG_VERTEX(row, column)        \
  f_HoleFlagVertices[0][g_MatrixBufferIndex] \
                    [(row) * HOLE_FLAG_COLUMN_COUNT + (column)]
#define HOLE_FLAG_SHADE(row, column) \
  f_HoleFlagShades[(row) * HOLE_FLAG_COLUMN_COUNT + (column)]

/* The flag can be skewed by HOLE_FLAG_SKEW per column, but it is zero */
#define HOLE_FLAG_SKEW 0.0f
#define HOLE_FLAG_ROW_SKEW_SCALE 5.0f
/* Wind speeds here are in WIND_SPEED_UNIT */
#define WIND_SPEED_UNIT 0x10000
#define HOLE_FLAG_MIN_WIND_SPEED 1.5f
#define HOLE_FLAG_GALE_WIND_SPEED 9.0f
#define HOLE_FLAG_CALM_SPREAD 0.01f
#define HOLE_FLAG_SPREAD_STEP 0.1f
#define HOLE_FLAG_SPREAD_STEP_COUNT 10.0f
#define HOLE_FLAG_SWING_DEGREES_PER_SPEED 2.0f
#define HOLE_FLAG_MIN_SWING_DEGREES 2.0f
#define HOLE_FLAG_SWING_JITTER_RANGE 16
#define HOLE_FLAG_ROW_JITTER_PER_SPEED 4.0f
#define HOLE_FLAG_BULGE_PER_SPEED 0.03f
#define HOLE_FLAG_MIN_BULGE 3.5f
#define HOLE_FLAG_BULGE_SCALE 0.1f
#define HOLE_FLAG_BULGE_COLUMN_BASE 8
#define HOLE_FLAG_BULGE_ROW_BASE 10
#define HOLE_FLAG_BULGE_ROW_SCALE 10.0f
#define HOLE_FLAG_FIRST_PULLED_COLUMN 2
#define HOLE_FLAG_DEPTH_PER_PULL 4
extern f32 f_HoleFlagWavePhase;

/* The near and far planes of the screen's orthographic projection */
#define SCREEN_ORTHO_NEAR 1.0f
#define SCREEN_ORTHO_FAR 1000.0f

extern Vtx f_HoleFlagVertices[HOLE_FLAG_SIDE_COUNT][MATRIX_BUFFER_COUNT]
                             [HOLE_FLAG_VERTEX_COUNT];
/* s8 because WaveHoleFlag loads them as signed bytes when it spreads them */
extern s8 f_HoleFlagShades[HOLE_FLAG_VERTEX_COUNT];
extern s32 D_800C5478;
extern Mtx D_80105278;

/*
 * A solid arrow: a triangular head on a rectangular shaft, from -160 to 160
 * along y, with a face at each z of +-40, and a larger outline around it.
 * The body's texture runs along y, a texel row every
 * ARROW_UNITS_PER_TEXEL_ROW units.
 */
#define ARROW_COUNT 2
#define ARROW_VERTEX_COUNT 14
#define ARROW_UNITS_PER_TEXEL_ROW 10
#define ARROW_CENTER_TEXEL_ROW 16 /* The texel row at y = 0 */
/* Texture coordinates are in s10.5 fixed point */
#define TEXEL_COORDINATE_SHIFT 5

typedef struct ArrowShape {
  /* 0x000 */ Vtx body[ARROW_VERTEX_COUNT];
  /* 0x0E0 */ Vtx outline[ARROW_VERTEX_COUNT];
} ArrowShape; /* size = 0x1C0 */

extern ArrowShape f_Arrows[ARROW_COUNT];

/*
 * The marker of the current shot's offset, with the positions it had in the
 * previous frames for its trail. The offset is clamped to a circle, in shot
 * units; the marker moves a pixel every SHOT_OFFSET_UNITS_PER_PIXEL units.
 */
#define SHOT_OFFSET_MARKER_COUNT 5
#define SHOT_OFFSET_MAX_RADIUS 56.0f
#define SHOT_OFFSET_UNITS_PER_PIXEL 4

extern s32 f_ShotOffsetMarkerXs[SHOT_OFFSET_MARKER_COUNT + 1];
extern s32 f_ShotOffsetMarkerYs[SHOT_OFFSET_MARKER_COUNT + 1];

/*
 * The marker is an animated 16x16 image, drawn at an offset from the shot
 * panel. Its asset holds the frames of the animation, then a still frame.
 */
#define SHOT_OFFSET_MARKER_SIZE 16
#define SHOT_OFFSET_MARKER_FRAME_COUNT 11
#define SHOT_OFFSET_MARKER_FRAME_SIZE \
  (SHOT_OFFSET_MARKER_SIZE * SHOT_OFFSET_MARKER_SIZE / 2)
#define SHOT_OFFSET_MARKER_STILL_FRAME SHOT_OFFSET_MARKER_FRAME_COUNT
#define SHOT_OFFSET_MARKER_X 264
#define SHOT_OFFSET_MARKER_Y 174

extern s32 f_ShotPanelOffsetY;
extern s32 f_ShotPanelOffsetX;
extern s32 f_ShotOffsetMarkerFrame;
extern s32 f_ShotOffsetMarkerTexelOffset;
extern void* f_ShotOffsetMarkerImage;

/* The images that a hole's intro shows for the bets and the match standing */
#define ASSET_LONGEST_DRIVE_IMAGE 0x554
#define ASSET_NEAREST_PIN_IMAGE 0x555
#define ASSET_DORMIE_IMAGE 0x8FC
#define ASSET_UNK8FF_IMAGE 0x8FF

/* Which of the hole intro's images are loaded */
#define HOLE_INTRO_CONTEST_IMAGE_LOADED (1 << 0)
#define HOLE_INTRO_MATCH_IMAGE_LOADED (1 << 1)
/* The intro images are drawn opaque, keeping the coverage of their alpha */
#define HOLE_INTRO_IMAGE_RENDER_MODE               \
  AA_EN | CVG_X_ALPHA | ALPHA_CVG_SEL | FORCE_BL | \
      GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1)
#define HOLE_INTRO_IMAGE_RENDER_MODE2              \
  AA_EN | CVG_X_ALPHA | ALPHA_CVG_SEL | FORCE_BL | \
      GBL_c2(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1)
/* The contest image is 256x64 and the match standing image 160x40, centered */
#define CONTEST_IMAGE_WIDTH 256
#define CONTEST_IMAGE_STRIP_HEIGHT 8
#define CONTEST_IMAGE_STRIP_COUNT 8
#define CONTEST_IMAGE_X ((SCREEN_WIDTH - CONTEST_IMAGE_WIDTH) / 2)
#define CONTEST_IMAGE_Y 72
#define MATCH_IMAGE_WIDTH 160
#define MATCH_IMAGE_STRIP_HEIGHT 10
#define MATCH_IMAGE_STRIP_COUNT 4
#define MATCH_IMAGE_X ((SCREEN_WIDTH - MATCH_IMAGE_WIDTH) / 2)
#define MATCH_IMAGE_Y 140

extern s32 f_LoadedHoleIntroImages;
extern void* f_HoleContestImage;
extern void* f_MatchStandingImage;
extern s32 D_801B60A0;

/* The ball's recent world positions, which its trail is drawn through */
#define BALL_TRAIL_LENGTH 128
extern f32 f_BallTrail[BALL_TRAIL_LENGTH][3];
extern s32 f_BallTrailHead; /* The latest position */

/*
 * The scrolling of the wind arrow's texture, a step for every
 * WIND_SPEED_PER_ARROW_SCROLL_STEP of wind; its texture repeats every
 * ARROW_TEXTURE_PERIOD of texture coordinate.
 */
#define WIND_SPEED_PER_ARROW_SCROLL_STEP 0x2000
#define ARROW_TEXTURE_PERIOD 0x2000
#define ARROW_TEXTURE_COORDINATE_LIMIT 0x4000

/* A debugging option that lets the second controller nudge the HUD vertically
 */
#define GAME_FLAG_ADJUST_HUD_OFFSET 0x6E
#define HUD_OFFSET_CONTROLLER 1

/*
 * A debug screen for trying out text colours: controller 2 picks a channel
 * of the primitive or environment colour and changes it, and moves and
 * shades the sample text
 */
#define COLOR_TEST_CONTROLLER 1
#define COLOR_TEST_COLOR_COUNT 2 /* The primitive and environment colours */
#define COLOR_TEST_CHANNEL_COUNT 3
#define COLOR_TEST_PRIM 0
#define COLOR_TEST_ENV 1
#define COLOR_TEST_STEP 4
#define COLOR_CHANNEL_MAX 255
#define COLOR_TEST_TEXT_COLUMN 20
#define COLOR_TEST_TEXT_ROW 5
/* Columns from one channel's value to the next */
#define COLOR_TEST_COLUMN_WIDTH 5
extern s32 f_TestColors[COLOR_TEST_COLOR_COUNT][COLOR_TEST_CHANNEL_COUNT];
extern s32 f_TestTextX;
extern s32 f_TestTextY;
extern s32 f_TestChannelIndex;
extern s32 f_TestColorIndex;
extern s32 f_IsTestGradientShown;
/* "Sample" in half-width katakana, then "ABCDEFG" */
extern char f_ColorTestSampleText[];
/* Printed over a cursor to erase it */
extern char f_BlankText[];
#define HUD_OFFSET_TEXT_COLUMN 20
#define HUD_OFFSET_TEXT_ROW 6
extern s32 f_HudOffsetY;

/* How long the shot camera waits after the ball lands, in frames */
#define LANDING_WAIT_FRAME_COUNT 40
#define LANDING_WAIT_LONG_FRAME_COUNT 60

/* The inset view that shows where the ball landed, and the one Toad's movie
 * plays in */
#define LANDING_INSET_CENTER_X 160
#define LANDING_INSET_CENTER_Y 160
#define LANDING_INSET_WIDTH 64
#define LANDING_INSET_HEIGHT 56
#define MOVIE_INSET_CENTER_X 160
#define MOVIE_INSET_CENTER_Y 128
#define MOVIE_INSET_SIZE 128
#define LANDING_INSET_UNK8 77
#define LANDING_INSET_UNKC 77
#define MOVIE_INSET_UNKC 101

/* The text of the landing reaction, in two glyph groups */
#define LANDING_GLYPH_GROUP_FIRST 0
#define LANDING_GLYPH_GROUP_SECOND 1

extern s32 f_LandingReaction;
extern s32 f_LandingReactionFrame;
/* The reaction stops counting its frames here */
#define LANDING_REACTION_FRAME_COUNT 30
#define LANDING_NAME_GLYPH_Y 8
#define LANDING_DISTANCE_GLYPH_Y (-8)
#define LANDING_NAME_Y 96
#define LANDING_DISTANCE_Y 112
#define HUNDREDTHS_PER_UNIT 100
/* The translucent black panel behind the text spins half a turn as it grows */
#define LANDING_PANEL_ALPHA 159
#define LANDING_PANEL_START_DEGREES 180.0f
#define LANDING_PANEL_DEGREES_PER_FRAME 18.0f
#define LANDING_PANEL_GROWTH_FRAME_COUNT 10
#define LANDING_PANEL_GROWTH_PER_FRAME (1.0f / LANDING_PANEL_GROWTH_FRAME_COUNT)
#define LANDING_PANEL_X (-72.0f)
#define LANDING_PANEL_Y 32.0f
#define LANDING_PANEL_Z (-100.0f)
#define LANDING_PANEL_PIVOT_X 72.0f
#define LANDING_PANEL_PIVOT_Y (-20.0f)
extern Vtx f_LandingPanelVertices[4];
extern Mtx f_LandingPanelMatrices[MATRIX_BUFFER_COUNT];

#define RING_TEXTURE_OUTER_RADIUS 62

/* The HUD keeps this far in from the edges of the screen */
#define HUD_MARGIN 8

extern void* f_DistanceFont;
extern s32 D_800E21BC;

/* The black bars that grow over the top and bottom of the HUD's area */
#define LETTERBOX_GROWTH_STEP 2
#define LETTERBOX_MAX_HEIGHT 16
#define LETTERBOX_STATE_UNK5 5

extern s32 D_800C308C;

/* Where the hole image sits in front of the perspective camera, at a tenth of
 * its size */
#define HOLE_IMAGE_MODEL_X (-240.0f)
#define HOLE_IMAGE_MODEL_Y 180.0f
#define HOLE_IMAGE_MODEL_Z (-500.0f)
#define HOLE_IMAGE_MODEL_SCALE 0.1f

extern Mtx f_HoleImageMatrix;

/*
 * StartShotEffect's long shot effect, which DrawShotEffect draws, plays a
 * sound, and a different, louder one for the Wario and Game Boy ball 7
 * contact styles.
 */
#define SHOT_EFFECT_TIME_OFF (-1.0f)
#define SOUND_LONG_SHOT 0x70
#define SOUND_LONG_SHOT_HEAVY 0x71
#define LONG_SHOT_SOUND_CHANNEL 14
#define LONG_SHOT_SOUND_VOLUME 0x40
#define LONG_SHOT_HEAVY_SOUND_VOLUME 0x60
#define LONG_SHOT_SOUND_REVERB 30
#define LONG_SHOT_SOUND_PAN 127

extern f32 f_ShotEffectTime;
extern s32 f_ShotEffect;

/*
 * The loaded texture of distance glyphs: the digits, then the units and the
 * decimal point, all DISTANCE_GLYPH_HEIGHT high
 */
#define DISTANCE_GLYPH_HEIGHT 12
#define DISTANCE_DIGIT_WIDTH 8
#define DISTANCE_METRES_S 80
#define DISTANCE_YARDS_S 88
#define DISTANCE_FEET_S 96
#define DISTANCE_FEET_WIDTH 16
#define DISTANCE_POINT_S 112
#define DISTANCE_POINT_WIDTH 4

extern void* f_SceneryBillboardHeap;
extern void* f_WindIconImage;
extern void* g_ShadowTexture;
/* The image asset of the club's head that the HUD shows, loaded into
 * f_ClubHeadImage */
extern s32 f_ClubHeadImageAsset;
extern void* f_ClubHeadImage;
/* The selected club's name image, loaded into each buffer in turn as
 * f_ClubNameImageIndex alternates */
extern void* f_ClubNameImages[2];
/* The image asset of the lie that the HUD shows, loaded into f_LieImage */
extern s32 f_LieImageAsset;
extern void* f_LieImage;
#define ANIMATED_ICON_LABEL_COUNT 3
/* The images of the animated icon and its labels; see DrawAnimatedIcon */
extern void* f_AnimatedIconImages[];
extern void* f_AnimatedIconLabelImages[ANIMATED_ICON_LABEL_COUNT];

extern void* f_WindSpeedFont;
extern void* f_RingImages;
extern void* f_RestLabelImage;
extern void* f_FlatLabelImage;
extern void* f_DownLabelImage;
extern void* f_UpLabelImage;
extern void* D_800E2178;

/*
 * The wind indicator is a model in module 9, lit by f_WindArrowLights. It
 * sits in front of the camera, tilted to its pitch and turned to the wind.
 */
#define MODULE_UNK9 9
#define WIND_INDICATOR_DEPTH -300.0f
#define WIND_INDICATOR_TILT 0.3f
#define WIND_INDICATOR_SCALE -0.07f
#define WIND_INDICATOR_FACING_DEGREES 180.0
extern Lights1 f_WindArrowLights;

/*
 * The wind speed display's icon pulses around these half sizes, and is wider
 * for two-digit speeds. Its texture is WIND_ICON_TEXTURE_WIDTH by HEIGHT.
 */
extern f32 f_WindIconPulseAngle;
#define WIND_ICON_PULSE_STEP (M_PI / 18)
#define WIND_SPEED_TO_METERS_SCALE 3600
#define WIND_SPEED_TO_METERS_DIVISOR 104857600
#define WIND_SPEED_TWO_DIGITS 10
#define WIND_ICON_WIDE_HALF_WIDTH 32.0f
#define WIND_ICON_NARROW_HALF_WIDTH 20.0f
#define WIND_ICON_HALF_HEIGHT 13.0f
#define WIND_ICON_TEXTURE_WIDTH 32
#define WIND_ICON_TEXTURE_HEIGHT 16
#define HALF_TEXEL 16.0f
#define WIND_WIDE_LEAN_X 18.0f
#define WIND_NARROW_LEAN_X 12.0f
#define WIND_LEAN_Y 16.0f
#define WIND_OFFSET_X 146.0f
#define WIND_OFFSET_Y 4.0f
#define WIND_POSITIVE_BIAS 64
#define WIND_ICON_OFFSET_X 13
#define WIND_ICON_OFFSET_Y 6
#define WIND_WIDE_TEXT_SHIFT_X 6.0f
#define WIND_ARROWS_OFFSET_X 52.0f
#define WIND_ARROWS_OFFSET_Y 56.0f
/* Arrows either side of the wind speed; they sit closer when there is no wind
 */
#define WIND_ARROWS_WIDE_GAP 56
#define WIND_ARROWS_NARROW_GAP 44
/* The wind speed font has ten digits and then a wider 'm' */
#define WIND_FONT_WIDTH 144
#define WIND_FONT_HEIGHT 14
#define WIND_DIGIT_WIDTH 12
#define WIND_METERS_GLYPH_S 119
#define WIND_METERS_GLYPH_WIDTH 14
#define WIND_TEXT_RAISE 1
#define WIND_TEXT_RENDER_MODE        \
  IM_RD | CLR_ON_CVG | CVG_X_ALPHA | \
      GBL_c1(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)
#define WIND_TEXT_RENDER_MODE2       \
  IM_RD | CLR_ON_CVG | CVG_X_ALPHA | \
      GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)
#define WIND_TEXT_PRIM_RED 0x3F
#define WIND_TEXT_PRIM_GREEN 0x3F
#define WIND_TEXT_PRIM_BLUE 0xFF
#define WIND_TEXT_ENV_RED 0x00
#define WIND_TEXT_ENV_GREEN 0x20
#define WIND_TEXT_ENV_BLUE 0x7F
extern Mtx f_WindIndicatorMatrices[2];
extern Gfx D_80222400[];
void DrawWindSpeed(Gfx** displayListHead, f32 x, f32 y);
extern s32 f_SceneryState;

/* Heights in meters or feet nearer zero than this */
#define LEVEL_HEIGHT 0.01f
/* On the tee ground, the shot panel sits this much higher */
#define TEE_GROUND_PANEL_OFFSET_Y -3
#define SHOT_PANEL_START_OFFSET_X 64
extern s8 D_801B60B9;
/* The colors of each course's name, which gradients from the first to the
 * second */
extern s32 g_CourseNameEnvColors[];
extern s32 g_CourseNamePrimColors[];
/* "%02d:%02d:%02d", which DrawRoundHud also uses */
extern char D_800D1C84[];

s32 func_80242418(void);
void func_80242424(Gfx** displayListHead);
void func_80242D40(Gfx** displayListHead);
void func_802427E8(Gfx** displayListHead);

#define MODULE_UNK21 21
#define MODULE_UNK26 26
#define STOPWATCH_X 168
#define STOPWATCH_Y 176
#define STOPWATCH_ENV_COLOR 0xFFFF0000
#define STOPWATCH_PRIM_COLOR 0x30FF0000
#define HOLE_TITLE_X (SCREEN_WIDTH / 2)
#define COURSE_NAME_Y 32
#define HOLE_NUMBER_Y 48

void LoadCourseSelect(s32);
void FreeCourseSelect(void);
void func_80216EDC(void);
void func_80216F8C(void);

/* Sets up what the hole's drawing needs before its first frame */
void PrepareHoleDisplay(void) {
  s32 i;

  g_MatrixBufferIndex = 0;
  for (i = 0; i != ARROW_VERTEX_COUNT; i++) {
    f_Arrows[0].body[i].v.tc[1] =
        (f_Arrows[0].body[i].v.ob[1] / ARROW_UNITS_PER_TEXEL_ROW +
            ARROW_CENTER_TEXEL_ROW)
        << TEXEL_COORDINATE_SHIFT;
    f_Arrows[1].body[i].v.tc[1] =
        (f_Arrows[1].body[i].v.ob[1] / ARROW_UNITS_PER_TEXEL_ROW +
            ARROW_CENTER_TEXEL_ROW)
        << TEXEL_COORDINATE_SHIFT;
  }
  BuildPinShadow();
  InitSkyColors();
}

/* Lays out the wind flag and builds the projections the HUD draws with */
#ifdef NON_MATCHING
void InitRoundHud(void) {
  s32 grid;
  s32 row;
  s32 column;
  Vtx* vertex;

  D_800C5478 = 0;
  for (grid = 0; grid != HOLE_FLAG_SIDE_COUNT * MATRIX_BUFFER_COUNT; grid++) {
    vertex = f_HoleFlagVertices[0][grid];
    for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
      for (column = 0; column != HOLE_FLAG_COLUMN_COUNT; column++) {
        vertex->v.ob[0] = column * HOLE_FLAG_CELL_SIZE;
        vertex->v.ob[1] = -row * HOLE_FLAG_CELL_SIZE;
        vertex->v.ob[2] = 0;
        vertex->v.tc[0] = column * HOLE_FLAG_TEXEL_COLUMN_STEP;
        vertex->v.tc[1] = row * HOLE_FLAG_TEXEL_ROW_STEP;
        vertex->v.cn[0] = HOLE_FLAG_START_SHADE;
        vertex->v.cn[1] = HOLE_FLAG_START_SHADE;
        vertex->v.cn[2] = HOLE_FLAG_START_SHADE;
        vertex->v.cn[3] = HOLE_FLAG_START_SHADE;
        vertex++;
      }
    }
  }
  for (grid = 0; grid != MATRIX_BUFFER_COUNT; grid++) {
    vertex = f_HoleFlagVertices[1][grid];
    for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
      for (column = 0; column != HOLE_FLAG_COLUMN_COUNT; column++) {
        vertex->v.tc[0] =
            HOLE_FLAG_BACK_TEXEL_START - column * HOLE_FLAG_TEXEL_COLUMN_STEP;
        vertex++;
      }
    }
  }
  for (column = 0; column != HOLE_FLAG_VERTEX_COUNT; column++) {
    f_HoleFlagShades[column] = HOLE_FLAG_START_SHADE;
  }
  guOrtho(&g_ScreenOrthoMatrix, -SCREEN_WIDTH / 2, SCREEN_WIDTH / 2,
      -SCREEN_HEIGHT / 2, SCREEN_HEIGHT / 2, SCREEN_ORTHO_NEAR,
      SCREEN_ORTHO_FAR, 1.0f);
  guPerspective(&D_80105278, &g_ScreenEffectPerspNorm, 10.0f, 4.0f / 3.0f,
      10.0f, 30000.0f, 1.0f);
  guPerspective(&g_ScreenEffectProjection, &g_ScreenEffectPerspNorm, 45.0f,
      4.0f / 3.0f, 10.0f, 8000.0f, 1.0f);
  g_SkyState.viewedWindAngles[0] = 0.0f;
  g_SkyState.horizonLevels[0] = 0.0f;
  g_SkyState.lookAtLevels[0] = 0.0f;
  g_SkyState.pitches[0] = 0.0f;
  g_SkyState.yaws[0] = 0.0f;
  g_SkyState.screenHeights[0] = 0.0f;
  g_SkyState.viewedWindAngles[1] = 0.0f;
  g_SkyState.horizonLevels[1] = 0.0f;
  g_SkyState.lookAtLevels[1] = 0.0f;
  g_SkyState.pitches[1] = 0.0f;
  g_SkyState.yaws[1] = 0.0f;
  g_SkyState.screenHeights[1] = 0.0f;
  g_InsetView.animation = INSET_VIEW_STILL;
  g_InsetView.step = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/round_hud", InitRoundHud);
#endif

/* Loads the face of a player's character in the colours of the given palette */
void LoadPlayerFace(s32 playerIndex, s32 character, s32 paletteIndex) {
  EncodedAsset asset;
  AssetFile file;

  OpenEncodedAsset(
      g_CharacterImageAssets[character] + CHARACTER_FACE_IMAGE_OFFSET, &asset);
  ReadEncodedAsset(g_PlayerFaceImages[playerIndex], &asset);
  OpenAssetFile(g_CharacterPaletteAssets[character] + paletteIndex, &file);
  ReadAssetFile(
      g_PlayerFacePalettes[playerIndex], GetAssetFileSize(&file), &file);
}

/* Frees the previous hole's mode images and loads the scenery images if they
 * are not loaded */
void EndHoleIntro(void) {
  if (g_GameMode == GAME_MODE_RING_SHOT) {
    if (f_IsCourseSelectLoaded == TRUE) {
      f_IsCourseSelectLoaded = FALSE;
      FreeCourseSelect();
    }
  } else if (g_GameMode == GAME_MODE_TRAINING) {
    if (f_IsCourseSelectLoaded == TRUE) {
      f_IsCourseSelectLoaded = FALSE;
      FreeCourseSelect();
    }
  } else {
    FreeHoleIntroImages();
  }
  if (f_SceneryState == SCENERY_BILLBOARDS_READY) {
    LoadSceneryImages();
  }
}

/* Starts the hole's weather and loads the images of the hole and its par */
void StartHoleIntro(void) {
  EncodedAsset asset;

  EndWeather();
  StartWeather(WEATHER_RAIN);
  if (g_HoleConditions.isRaining == TRUE) {
    PlaySound(RAIN_SOUND, RAIN_SOUND_CHANNEL, RAIN_SOUND_START_VOLUME);
    g_RainSoundVolume = RAIN_SOUND_START_VOLUME;
  } else {
    StopSound(RAIN_SOUND_CHANNEL);
  }
  RevertBillboardManager();
  OpenEncodedAsset(
      f_HoleImageAssets[GetCourseRecordIndex()] + GetHoleIndexInRecord(),
      &asset);
  ReadEncodedAsset(f_HoleImage, &asset);
  switch ((s8)g_CourseRecords[GetCourseRecordIndex()]
          .holes[GetHoleRecordIndex()]
          .par) {
    case 3:
      OpenEncodedAsset(ASSET_PAR_THREE_IMAGE, &asset);
      break;
    case 4:
      OpenEncodedAsset(ASSET_PAR_FOUR_IMAGE, &asset);
      break;
    case 5:
      OpenEncodedAsset(ASSET_PAR_FIVE_IMAGE, &asset);
      break;
  }
  ReadEncodedAsset(f_ParImage, &asset);
  if (f_SceneryState == SCENERY_LOADED) {
    FreeSceneryImages();
  }
  if (g_GameMode == GAME_MODE_RING_SHOT) {
    if (f_IsCourseSelectLoaded == FALSE) {
      f_IsCourseSelectLoaded = TRUE;
      LoadCourseSelect(0);
    }
  } else if (g_GameMode == GAME_MODE_TRAINING) {
    if (f_IsCourseSelectLoaded == FALSE) {
      f_IsCourseSelectLoaded = TRUE;
      LoadCourseSelect(1);
    }
  } else {
    LoadHoleIntroImages();
  }
}

/* Loads the images the round's scenery, effects and HUD are drawn with */
void LoadSceneryImages(void) {
  EncodedAsset asset;

  f_SceneryState = SCENERY_LOADED;
  LoadGolfBallImages();
  LoadEffectImages();
  LoadHoleCreatureImages();
  func_80216EDC();
  AllocMovieBuffer();
  LoadClubPanelImages();
  g_PinFlagImages =
      AllocMemory(OpenEncodedAsset(ASSET_PIN_FLAG_IMAGES, &asset));
  ReadEncodedAsset(g_PinFlagImages, &asset);
  f_WindIconImage =
      AllocMemory(OpenEncodedAsset(ASSET_WIND_ICON_IMAGE, &asset));
  ReadEncodedAsset(f_WindIconImage, &asset);
  g_PlayerTagImages =
      AllocMemory(OpenEncodedAsset(ASSET_PLAYER_TAG_IMAGES, &asset));
  ReadEncodedAsset(g_PlayerTagImages, &asset);
  g_LeaderLineImage =
      AllocMemory(OpenEncodedAsset(ASSET_LEADER_LINE_IMAGE, &asset));
  ReadEncodedAsset(g_LeaderLineImage, &asset);
  g_BackdropPatternImage =
      AllocMemory(OpenEncodedAsset(ASSET_BACKDROP_PATTERN_IMAGE, &asset));
  ReadEncodedAsset(g_BackdropPatternImage, &asset);
  g_ShotPreviewDashImage =
      AllocMemory(OpenEncodedAsset(ASSET_SHOT_PREVIEW_DASH_IMAGE, &asset));
  ReadEncodedAsset(g_ShotPreviewDashImage, &asset);
  g_ShadowTexture = AllocMemory(OpenEncodedAsset(ASSET_SHADOW_TEXTURE, &asset));
  ReadEncodedAsset(g_ShadowTexture, &asset);
  f_ClubHeadImageAsset = -1;
  f_ClubHeadImage =
      AllocMemory(OpenEncodedAsset(ASSET_DRIVER_HEAD_IMAGE, &asset));
  ReadEncodedAsset(f_ClubHeadImage, &asset);
  f_ClubNameImages[0] = AllocMemory(SCENERY_BUFFER_A_SIZE);
  f_ClubNameImages[1] = AllocMemory(SCENERY_BUFFER_A_SIZE);
  f_LieImageAsset = -1;
  f_LieImage = AllocMemory(LIE_IMAGE_SIZE);
  f_AnimatedIconImages[0] =
      AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_648, &asset));
  ReadEncodedAsset(f_AnimatedIconImages[0], &asset);
  f_AnimatedIconLabelImages[0] =
      AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_563, &asset));
  ReadEncodedAsset(f_AnimatedIconLabelImages[0], &asset);
  f_AnimatedIconLabelImages[1] =
      AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_564, &asset));
  ReadEncodedAsset(f_AnimatedIconLabelImages[1], &asset);
  f_AnimatedIconLabelImages[2] =
      AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_562, &asset));
  ReadEncodedAsset(f_AnimatedIconLabelImages[2], &asset);
  f_ShotOffsetMarkerImage =
      AllocMemory(OpenEncodedAsset(ASSET_SHOT_OFFSET_MARKER_IMAGE, &asset));
  ReadEncodedAsset(f_ShotOffsetMarkerImage, &asset);
  f_WindSpeedFont =
      AllocMemory(OpenEncodedAsset(ASSET_WIND_SPEED_FONT, &asset));
  ReadEncodedAsset(f_WindSpeedFont, &asset);
  f_RingImages = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_725, &asset));
  ReadEncodedAsset(f_RingImages, &asset);
  f_RestLabelImage =
      AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_568, &asset));
  ReadEncodedAsset(f_RestLabelImage, &asset);
  f_FlatLabelImage =
      AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_56B, &asset));
  ReadEncodedAsset(f_FlatLabelImage, &asset);
  f_DownLabelImage =
      AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_56A, &asset));
  ReadEncodedAsset(f_DownLabelImage, &asset);
  f_UpLabelImage = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_569, &asset));
  ReadEncodedAsset(f_UpLabelImage, &asset);
  D_800E2178 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_723, &asset));
  ReadEncodedAsset(D_800E2178, &asset);
  g_DistanceSignImage =
      AllocMemory(OpenEncodedAsset(ASSET_DISTANCE_SIGN_IMAGE, &asset));
  ReadEncodedAsset(g_DistanceSignImage, &asset);
  LoadBursts(GetCourseRecordIndex());
}

/* Frees what LoadSceneryImages loaded */
void FreeSceneryImages(void) {
  if (f_SceneryState == SCENERY_LOADED) {
    f_SceneryState = SCENERY_BILLBOARDS_READY;
    FreeEffectImages();
    FreeHoleCreatureImages();
    func_80216F8C();
    FreeMovieBuffer();
    FreeClubPanelImages();
    FreeMemory(&g_PinFlagImages);
    FreeMemory(&f_WindIconImage);
    FreeMemory(&g_PlayerTagImages);
    FreeMemory(&g_LeaderLineImage);
    FreeMemory(&g_BackdropPatternImage);
    FreeMemory(&g_ShotPreviewDashImage);
    FreeMemory(&g_ShadowTexture);
    FreeMemory(&f_ClubHeadImage);
    FreeMemory(&f_ClubNameImages[0]);
    FreeMemory(&f_ClubNameImages[1]);
    FreeMemory(&f_LieImage);
    FreeMemory(&f_AnimatedIconImages[0]);
    FreeMemory(&f_AnimatedIconLabelImages[0]);
    FreeMemory(&f_AnimatedIconLabelImages[1]);
    FreeMemory(&f_AnimatedIconLabelImages[2]);
    FreeMemory(&f_ShotOffsetMarkerImage);
    FreeMemory(&f_WindSpeedFont);
    FreeMemory(&f_RingImages);
    FreeMemory(&f_RestLabelImage);
    FreeMemory(&f_FlatLabelImage);
    FreeMemory(&f_DownLabelImage);
    FreeMemory(&f_UpLabelImage);
    FreeMemory(&D_800E2178);
    FreeMemory(&g_DistanceSignImage);
    FreeBursts();
  }
}

/* Sets up the billboard manager's heap, once per round */
void InitScenery(void) {
  EncodedAsset unusedAsset; /* Left from code that loaded assets here */

  if (f_SceneryState == SCENERY_UNLOADED) {
    f_SceneryBillboardHeap = AllocMemory(BILLBOARD_HEAP_SIZE);
    InitBillboardManager(f_SceneryBillboardHeap, BILLBOARD_HEAP_SIZE);
    f_SceneryState = SCENERY_BILLBOARDS_READY;
  }
}

/* Frees everything the round's scenery holds, when its overlay is unloaded */
void FreeScenery(void) {
  if (f_SceneryState != SCENERY_UNLOADED) {
    DestroyBillboardManager();
    FreeMemory(&f_SceneryBillboardHeap);
    EndWeather();
    ClearShotEffects(1);
    FreeSceneryImages();
    f_SceneryState = SCENERY_UNLOADED;
    memset(&g_RoundHud, 0, sizeof(g_RoundHud));
  }
}

/*
 * Updates the shot panel for a new shot: the ball's height above the cup and
 * distance from it, the lie's image, and the scores that the game mode shows.
 * The target addresses g_HoleIndex from g_PlayerCount's base, as if the two
 * were fields of one struct.
 */
#ifdef NON_MATCHING
void StartShotPanel(void) {
  EncodedAsset asset;
  s32 height;
  s32 playerIndex;
  s32 holeIndex;

  height = g_CurrentShot.ballPosition[AXIS_Y] - g_CupPositionY;
  g_RoundHud.ballHeightYards = height * YARDS_PER_UNIT;
  g_RoundHud.ballHeightMeters = height * METERS_PER_UNIT;
  g_RoundHud.ballHeightFeet = (height * FEET_PER_YARD) * YARDS_PER_UNIT;
  if (g_RoundHud.ballHeightYards == 0) {
    g_RoundHud.ballHeightYardsDirection = HEIGHT_LEVEL;
  } else if (g_RoundHud.ballHeightYards > 0) {
    g_RoundHud.ballHeightYardsDirection = HEIGHT_ABOVE;
  } else {
    g_RoundHud.ballHeightYardsDirection = HEIGHT_BELOW;
  }
  if (g_RoundHud.ballHeightMeters < LEVEL_HEIGHT &&
      g_RoundHud.ballHeightMeters > -LEVEL_HEIGHT) {
    g_RoundHud.ballHeightMetersDirection = HEIGHT_LEVEL;
  } else if (g_RoundHud.ballHeightMeters > 0.0f) {
    g_RoundHud.ballHeightMetersDirection = HEIGHT_ABOVE;
  } else {
    g_RoundHud.ballHeightMetersDirection = HEIGHT_BELOW;
  }
  /* The direction in feet is taken from the height in meters */
  if (g_RoundHud.ballHeightFeet < LEVEL_HEIGHT &&
      g_RoundHud.ballHeightFeet > -LEVEL_HEIGHT) {
    g_RoundHud.ballHeightFeetDirection = HEIGHT_LEVEL;
  } else if (g_RoundHud.ballHeightMeters > 0.0f) {
    g_RoundHud.ballHeightFeetDirection = HEIGHT_ABOVE;
  } else {
    g_RoundHud.ballHeightFeetDirection = HEIGHT_BELOW;
  }
  g_RoundHud.cupDistance = (f32)GetCupDistance(
      g_CurrentShot.ballPosition[AXIS_X] - D_800BA9D8.cupPosition[AXIS_X],
      g_CurrentShot.ballPosition[AXIS_Z] - g_CupPositionZ);
  if (GetSurfaceInfo(g_CurrentShot.surface)->unk10 != f_LieImageAsset) {
    f_LieImageAsset = GetSurfaceInfo(g_CurrentShot.surface)->unk10;
    if (GetSurfaceInfo(g_CurrentShot.surface)->kind ==
        SURFACE_KIND_TEE_GROUND) {
      f_ShotPanelOffsetY = TEE_GROUND_PANEL_OFFSET_Y;
    } else {
      f_ShotPanelOffsetY = 0;
    }
    OpenEncodedAsset(f_LieImageAsset, &asset);
    ReadEncodedAsset(f_LieImage, &asset);
  }
  if (D_801B60B9) {
    g_RoundHud.skinsTotals[0] = 0;
  } else {
    switch (g_GameMode) {
      case GAME_MODE_STROKE:
      case GAME_MODE_UNK1:
      case GAME_MODE_TOURNAMENT:
      case GAME_MODE_SPEED_GOLF:
        for (playerIndex = 0; playerIndex < g_PlayerCount; playerIndex++) {
          g_RoundHud.skinsTotals[playerIndex] =
              GetNetScoreToPar(g_HoleIndex, playerIndex);
        }
        break;
      case GAME_MODE_MATCH_GAME:
      case GAME_MODE_GET_CHARACTER:
        g_RoundHud.skinsTotals[0] = 0;
        for (holeIndex = 0; holeIndex != g_HoleIndex; holeIndex++) {
          if (g_Players[0].holes[holeIndex].strokes <
              g_Players[1].holes[holeIndex].strokes) {
            g_RoundHud.skinsTotals[0]--;
          } else if (g_Players[1].holes[holeIndex].strokes <
              g_Players[0].holes[holeIndex].strokes) {
            g_RoundHud.skinsTotals[0]++;
          }
        }
        g_RoundHud.skinsTotals[1] = -g_RoundHud.skinsTotals[0];
        break;
      case GAME_MODE_SKINS_MATCH:
      case GAME_MODE_CLUB_SLOTS:
        TallySkins(GetHoleRecordIndex());
        break;
    }
  }
  f_ShotPanelOffsetX = SHOT_PANEL_START_OFFSET_X;
  ResetShotModeChange();
  LoadContactImages(&g_Players[g_RoundHud.unk87]);
}
#else
INCLUDE_ASM("asm/nonmatchings/round_hud", StartShotPanel);
#endif

/*
 * Allocates and loads what the HUD draws for the whole round. The buffers of
 * the hole and par images are sized by the first of them, and later holes
 * read theirs into the same buffers.
 */
void LoadRoundHud(void) {
  AssetFile file;
  EncodedAsset asset;
  s32 i;
  u32 paletteSize;

  g_CourseSkyImage =
      AllocMemory(OpenEncodedAsset(ASSET_COURSE_SKY_IMAGE_0, &asset));
  ReadEncodedAsset(g_CourseSkyImage, &asset);
  g_SkyImage = AllocMemory(OpenEncodedAsset(ASSET_SKY_IMAGE_0, &asset));
  ReadEncodedAsset(g_SkyImage, &asset);
  f_HoleImage = AllocMemory(OpenEncodedAsset(f_HoleImageAssets[0], &asset));
  ReadEncodedAsset(f_HoleImage, &asset);
  f_ParImage = AllocMemory(OpenEncodedAsset(ASSET_PAR_FIVE_IMAGE, &asset));
  ReadEncodedAsset(f_ParImage, &asset);
  f_DistanceFont = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_722, &asset));
  ReadEncodedAsset(f_DistanceFont, &asset);
  g_RingTexels = AllocMemory(RING_TEXTURE_SIZE * RING_TEXTURE_SIZE);
  BuildRingTexels(g_RingTexels, RING_TEXTURE_SIZE, RING_TEXTURE_SIZE,
      RING_TEXTURE_OUTER_RADIUS);
  for (i = 0; i != SHOT_OFFSET_MARKER_COUNT; i++) {
    f_ShotOffsetMarkerXs[i] = 0;
    f_ShotOffsetMarkerYs[i] = 0;
  }
  g_HudScissor.left = HUD_MARGIN;
  g_HudScissor.top = HUD_MARGIN;
  g_HudScissor.right = SCREEN_WIDTH - HUD_MARGIN;
  D_800E21BC = 0;
  g_HudScissor.bottom = SCREEN_HEIGHT - HUD_MARGIN;
  g_NoiseTexels = AllocMemory(NOISE_GRID_SIZE * NOISE_GRID_SIZE);
  g_BlendedTextureScrollingLayer = AllocMemory(
      OpenEncodedAsset(ASSET_BLENDED_TEXTURE_SCROLLING_LAYER, &asset));
  ReadEncodedAsset(g_BlendedTextureScrollingLayer, &asset);
  g_BlendedTextureStillLayer =
      AllocMemory(OpenEncodedAsset(ASSET_BLENDED_TEXTURE_STILL_LAYER, &asset));
  ReadEncodedAsset(g_BlendedTextureStillLayer, &asset);
  for (i = 0; i != MAX_PLAYER_COUNT; i++) {
    g_PlayerFaceImages[i] = AllocMemory(OpenEncodedAsset(
        g_CharacterImageAssets[i] + CHARACTER_FACE_IMAGE_OFFSET, &asset));
    ReadEncodedAsset(g_PlayerFaceImages[i], &asset);
    OpenAssetFile(g_CharacterPaletteAssets[i], &file);
    paletteSize = GetAssetFileSize(&file);
    g_PlayerFacePalettes[i] = AllocMemory(paletteSize);
    ReadAssetFile(g_PlayerFacePalettes[i], paletteSize, &file);
  }
  InitScreenLayers();
  LoadPinTextures();
  ClearEffects();
}

/* Frees what the round HUD loaded for the round */
void FreeRoundHudTextures(void) {
  FreePinTextures();
}

/* The inset view's frame is a 16x3 I4 texture, wrapped along its lines */
#define INSET_FRAME_TEXTURE_WIDTH 16
#define INSET_FRAME_TEXTURE_HEIGHT 3
#define INSET_FRAME_SIDE_TEXTURE_HEIGHT 1
#define INSET_FRAME_TEXTURE_MASK 1
/* Where in the texture's first row the frame's sides start */
#define INSET_FRAME_SIDE_TEXEL 5
#define INSET_FRAME_SIDE_HALF_WIDTH 2
#define INSET_FRAME_BORDER_OUTSIDE 1
#define INSET_FRAME_BORDER_INSIDE 2
extern u8 f_InsetFrameTexture[];

/* Draws the frame around the inset view */
void DrawInsetViewFrame(void) {
  SetUpTextDrawing(&g_DisplayListHead);
  gDPPipeSync(g_DisplayListHead++);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
  gDPPipeSync(g_DisplayListHead++);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetRenderMode(g_DisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gSPTexture(g_DisplayListHead++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetAlphaCompare(g_DisplayListHead++, G_AC_NONE);
  gDPSetBlendColor(g_DisplayListHead++, 0, 0, 0, 1);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTexturePersp(g_DisplayListHead++, G_TP_NONE);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTextureFilter(g_DisplayListHead++, G_TF_POINT);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetAlphaDither(g_DisplayListHead++, G_AD_DISABLE);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTextureLUT(g_DisplayListHead++, G_TT_NONE);
  gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, 255);
  gDPSetCombineMode(g_DisplayListHead++, G_CC_MODULATEIDECALA_PRIM,
      G_CC_MODULATEIDECALA_PRIM);
  gDPLoadTextureBlock_4b(g_DisplayListHead++, (u32)f_InsetFrameTexture & ~7,
      G_IM_FMT_I, INSET_FRAME_TEXTURE_WIDTH, INSET_FRAME_TEXTURE_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,
      INSET_FRAME_TEXTURE_MASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(g_DisplayListHead++);
  if (g_Cameras[INSET_CAMERA].view.scissorWidth > 0) {
    gSPTextureRectangle(g_DisplayListHead++,
        g_Cameras[INSET_CAMERA].view.scissorX << 2,
        (g_Cameras[INSET_CAMERA].view.scissorY - INSET_FRAME_BORDER_OUTSIDE)
            << 2,
        (g_Cameras[INSET_CAMERA].view.scissorX +
            g_Cameras[INSET_CAMERA].view.scissorWidth)
            << 2,
        (g_Cameras[INSET_CAMERA].view.scissorY + INSET_FRAME_BORDER_INSIDE)
            << 2,
        G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    gSPTextureRectangle(g_DisplayListHead++,
        g_Cameras[INSET_CAMERA].view.scissorX << 2,
        (g_Cameras[INSET_CAMERA].view.scissorY +
            g_Cameras[INSET_CAMERA].view.scissorHeight -
            INSET_FRAME_BORDER_INSIDE)
            << 2,
        (g_Cameras[INSET_CAMERA].view.scissorX +
            g_Cameras[INSET_CAMERA].view.scissorWidth)
            << 2,
        (g_Cameras[INSET_CAMERA].view.scissorY +
            g_Cameras[INSET_CAMERA].view.scissorHeight +
            INSET_FRAME_BORDER_OUTSIDE)
            << 2,
        G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
  }
  gDPPipeSync(g_DisplayListHead++);
  gDPLoadTextureBlock_4b(g_DisplayListHead++, (u32)f_InsetFrameTexture & ~7,
      G_IM_FMT_I, INSET_FRAME_TEXTURE_WIDTH, INSET_FRAME_SIDE_TEXTURE_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK,
      INSET_FRAME_TEXTURE_MASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(g_DisplayListHead++);
  if (g_Cameras[INSET_CAMERA].view.scissorHeight > 0) {
    gSPTextureRectangle(g_DisplayListHead++,
        (g_Cameras[INSET_CAMERA].view.scissorX - INSET_FRAME_SIDE_HALF_WIDTH)
            << 2,
        g_Cameras[INSET_CAMERA].view.scissorY << 2,
        (g_Cameras[INSET_CAMERA].view.scissorX + INSET_FRAME_SIDE_HALF_WIDTH)
            << 2,
        (g_Cameras[INSET_CAMERA].view.scissorY +
            g_Cameras[INSET_CAMERA].view.scissorHeight)
            << 2,
        G_TX_RENDERTILE, INSET_FRAME_SIDE_TEXEL << 5, 0, 1 << 10, 1 << 10);
    gSPTextureRectangle(g_DisplayListHead++,
        (g_Cameras[INSET_CAMERA].view.scissorX +
            g_Cameras[INSET_CAMERA].view.scissorWidth -
            INSET_FRAME_SIDE_HALF_WIDTH)
            << 2,
        g_Cameras[INSET_CAMERA].view.scissorY << 2,
        (g_Cameras[INSET_CAMERA].view.scissorX +
            g_Cameras[INSET_CAMERA].view.scissorWidth +
            INSET_FRAME_SIDE_HALF_WIDTH)
            << 2,
        (g_Cameras[INSET_CAMERA].view.scissorY +
            g_Cameras[INSET_CAMERA].view.scissorHeight)
            << 2,
        G_TX_RENDERTILE, INSET_FRAME_SIDE_TEXEL << 5, 0, 1 << 10, 1 << 10);
  }
  gDPPipeSync(g_DisplayListHead++);
}

/* Scrolls the wind arrow's texture along it, faster in stronger wind */
void ScrollWindArrow(void) {
  s32 step = g_HoleConditions.windSpeed / WIND_SPEED_PER_ARROW_SCROLL_STEP;
  s32 i;

  for (i = 0; i != ARROW_VERTEX_COUNT; i++) {
    f_Arrows[g_MatrixBufferIndex].body[i].v.tc[1] += step;
  }
  if (f_Arrows[g_MatrixBufferIndex].body[0].v.tc[1] >
      ARROW_TEXTURE_COORDINATE_LIMIT) {
    for (i = 0; i != ARROW_VERTEX_COUNT; i++) {
      f_Arrows[g_MatrixBufferIndex].body[i].v.tc[1] -= ARROW_TEXTURE_PERIOD;
    }
  }
}

/*
 * Starts the reaction to where the ball landed, or clears it away with
 * LANDING_REACTION_NONE. Out of bounds, Toad's movie plays in the inset view
 * in place of a reaction; elsewhere the inset view shows the ball.
 */
void StartLandingReaction(s32 reaction) {
  s32 previousReaction;

  f_LandingReactionFrame = 0;
  if (reaction == LANDING_REACTION_NONE) {
    f_LandingReaction = reaction;
    PlaceGlyphGroup(LANDING_GLYPH_GROUP_FIRST, NO_FONT_SIZE, 0, 0, 0, NULL);
    PlaceGlyphGroup(LANDING_GLYPH_GROUP_SECOND, NO_FONT_SIZE, 0, 0, 0, NULL);
    StartMovie(MOVIE_NONE);
    return;
  }
  previousReaction = f_LandingReaction;
  if (previousReaction != LANDING_REACTION_NONE) {
    return;
  }
  f_LandingReaction = reaction;
  switch ((u32)GetSurfaceInfo(g_BallSurface)->kind) {
    case SURFACE_KIND_OUT_OF_BOUNDS:
      g_InsetView.animation = INSET_VIEW_OPENING;
      g_InsetView.unk8 = LANDING_INSET_UNK8;
      f_LandingReaction = previousReaction;
      g_InsetView.unkC = MOVIE_INSET_UNKC;
      SetInsetViewRect(MOVIE_INSET_CENTER_X, MOVIE_INSET_CENTER_Y,
          MOVIE_INSET_SIZE, MOVIE_INSET_SIZE);
      SetLandingWaitFrameCount(LANDING_WAIT_LONG_FRAME_COUNT);
      StartMovie(MOVIE_TOAD_OUT_OF_BOUNDS);
      osSyncPrintf("MOVIE KINOP \n");
      break;
    case SURFACE_KIND_OUT_OF_COURSE:
    case SURFACE_KIND_WATER_HAZARD:
      f_LandingReaction = LANDING_REACTION_UNK2;
      SetLandingWaitFrameCount(LANDING_WAIT_LONG_FRAME_COUNT);
      break;
    case SURFACE_KIND_GREEN:
      SetLandingWaitFrameCount(LANDING_WAIT_FRAME_COUNT);
      break;
    default:
      g_InsetView.animation = INSET_VIEW_OPENING;
      g_InsetView.unk8 = LANDING_INSET_UNK8;
      g_InsetView.unkC = LANDING_INSET_UNKC;
      SetLandingWaitFrameCount(LANDING_WAIT_FRAME_COUNT);
      SetInsetViewRect(LANDING_INSET_CENTER_X, LANDING_INSET_CENTER_Y,
          LANDING_INSET_WIDTH, LANDING_INSET_HEIGHT);
      break;
  }
}

/*
 * Draws the landing reaction: a panel with the name of the surface the ball
 * landed on and, for LANDING_REACTION_REST, how far it rests from the cup.
 * The panel spins and grows into place over its first frames, while the text
 * is drawn as glyph groups in its perspective; after that the text is drawn
 * flat.
 *
 * Doesn't match: the hundredths of the distance go in a different register.
 */
#ifdef NON_MATCHING
void DrawLandingReaction(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  Vec3 scale;
  Vec3 translation;
  Vec3 rotation;
  Mat4 matrix;
  Mat4 offset;
  SurfaceInfo* surface;
  s32 distance;
  s32 hundredths;
  f32 angle;
  f32 size;

  if (f_LandingReaction != LANDING_REACTION_NONE) {
    surface = GetSurfaceInfo(g_BallSurface);
    if (f_LandingReactionFrame == 1) {
      PlaceGlyphGroup(LANDING_GLYPH_GROUP_FIRST, FONT_SIZE_LARGE, 0, 0,
          LANDING_NAME_GLYPH_Y, surface->name);
    }
    if (f_LandingReaction == LANDING_REACTION_REST && !IsOnDrivingRange()) {
      distance = GetCupDistance(
          g_BallPosition[AXIS_X] - D_800BA9D8.cupPosition[AXIS_X],
          g_BallPosition[AXIS_Z] - g_CupPositionZ);
      /* Mini golf and the green measure in feet */
      if (g_IsMiniGolf || surface->kind == SURFACE_KIND_GREEN) {
        hundredths =
            distance * (FEET_PER_YARD * HUNDREDTHS_PER_UNIT) * YARDS_PER_UNIT;
        sprintf(g_PrintBuffer, "%2d.%02d%c REST",
            hundredths / HUNDREDTHS_PER_UNIT, hundredths % HUNDREDTHS_PER_UNIT,
            FEET_GLYPH);
      } else {
        hundredths = distance * HUNDREDTHS_PER_UNIT * YARDS_PER_UNIT;
        sprintf(g_PrintBuffer, "%3d.%02d%c REST",
            hundredths / HUNDREDTHS_PER_UNIT, hundredths % HUNDREDTHS_PER_UNIT,
            YARDS_GLYPH);
      }
      if (f_LandingReactionFrame == 1) {
        PlaceGlyphGroup(LANDING_GLYPH_GROUP_SECOND, FONT_SIZE_MEDIUM, 0, 0,
            LANDING_DISTANCE_GLYPH_Y, g_PrintBuffer);
      }
    }
    if (f_LandingReactionFrame < LANDING_REACTION_FRAME_COUNT) {
      f_LandingReactionFrame++;
    }
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, LANDING_PANEL_ALPHA);
    gDPPipeSync(gfx++);
    angle = DEGREES_TO_RADIANS(
        f_LandingReactionFrame * LANDING_PANEL_DEGREES_PER_FRAME +
        LANDING_PANEL_START_DEGREES);
    if (angle > 2 * M_PI) {
      angle = 2 * M_PI;
    }
    size = f_LandingReactionFrame * LANDING_PANEL_GROWTH_PER_FRAME;
    if (size > 1.0f) {
      size = 1.0f;
    }
    rotation[AXIS_X] = 0.0f;
    rotation[AXIS_Y] = 0.0f;
    scale[AXIS_X] = size;
    scale[AXIS_Y] = size;
    scale[AXIS_Z] = size;
    rotation[AXIS_Z] = -angle;
    translation[AXIS_X] = LANDING_PANEL_X;
    translation[AXIS_Y] = LANDING_PANEL_Y;
    translation[AXIS_Z] = LANDING_PANEL_Z;
    BuildTransformMatrix(matrix, rotation, translation, scale);
    /* Spins the panel around a point away from its center */
    guTranslateF(offset, LANDING_PANEL_PIVOT_X, LANDING_PANEL_PIVOT_Y, 0.0f);
    guMtxCatF(offset, matrix, matrix);
    ConvertMatrixToFixed(matrix, &f_LandingPanelMatrices[g_MatrixBufferIndex]);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gfx++,
        OS_K0_TO_PHYSICAL(&f_LandingPanelMatrices[g_MatrixBufferIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPVertex(
        gfx++, f_LandingPanelVertices, ARRAY_COUNT(f_LandingPanelVertices), 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    if (f_LandingReactionFrame < LANDING_PANEL_GROWTH_FRAME_COUNT) {
      SetUpPerspectiveTextDrawing(&gfx);
      DrawGlyphGroup(&gfx, LANDING_GLYPH_GROUP_FIRST);
      if (f_LandingReaction == LANDING_REACTION_REST) {
        DrawGlyphGroup(&gfx, LANDING_GLYPH_GROUP_SECOND);
      }
    } else {
      SetUpTextDrawing(&gfx);
      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
      DrawCenteredLargeText(
          &gfx, SCREEN_WIDTH / 2, LANDING_NAME_Y, surface->name);
      if (f_LandingReaction == LANDING_REACTION_REST && !IsOnDrivingRange()) {
        DrawCenteredMediumText(
            &gfx, SCREEN_WIDTH / 2, LANDING_DISTANCE_Y, g_PrintBuffer);
      }
    }
  }
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/round_hud", DrawLandingReaction);
#endif

/*
 * Returns white tinted toward red by a positive warmth, or toward blue by a
 * negative one, as an RGBA colour with the given alpha. Nothing calls it.
 */
u32 GetWarmthTint(s32 warmth, s32 alpha) {
  s32 red;
  s32 green;
  s32 blue;

  blue = 0xFF;
  if (warmth == 0) {
    green = 0xFF;
    red = 0xFF;
  } else if (warmth > 0) {
    red = 0xFF;
    blue = warmth;
    blue = red - blue;
    green = blue;
  } else {
    green = warmth + 0xFF;
    red = green;
  }
  return (red << 24) | (green << 16) | (blue << 8) | (alpha & 0xFF);
}

/*
 * The grid drawn over the ground around a shot's target, as
 * GREEN_GRID_COLUMN_COUNT columns of points that run along the aim. Each point
 * is tinted by how far it is above or below the target, and the grid's lines
 * scroll towards the target while it shows.
 */
#define GREEN_GRID_COLUMN_COUNT 8
#define GREEN_GRID_MAX_ROW_COUNT 32
#define GREEN_GRID_POINT_COUNT \
  (GREEN_GRID_MAX_ROW_COUNT * GREEN_GRID_COLUMN_COUNT)
#define GREEN_GRID_SHORT_ROW_COUNT 8 /* For the longest putt */
#define GREEN_GRID_LANDING_ROW_COUNT 16
/* With GREEN_GRID_MAX_ROW_COUNT rows, only every other row is sampled, up to
 * here */
#define GREEN_GRID_FIRST_FULL_ROW 30
#define GREEN_GRID_MAX_PUTT_DISTANCE 30
/* Shots shorter than this show the grid at the ball, longer ones at the landing
 * point */
#define GREEN_GRID_LANDING_DISTANCE 61
#define GREEN_GRID_PUTT_ROW_SHIFT 2.2f
#define GREEN_GRID_LANDING_ROW_SHIFT 12.0f
#define GREEN_GRID_PUTT_COLUMN_SPACING 18000.0f
#define GREEN_GRID_LANDING_WIDTH 307200.0f
#define GREEN_GRID_SHORT_ROW_SPACING 25600.0f
#define GREEN_GRID_PUTT_ROW_SPACING 4681.728f
#define GREEN_GRID_ROW_SPACING 15360.0f
#define GREEN_GRID_LANDING_ROW_SPACING 28800.0f
#define GREEN_GRID_SCROLL_STEP 0.025f
#define GREEN_GRID_SLOPE_SCALE 1.5f
#define GREEN_GRID_MAX_SLOPE 255
#define GREEN_GRID_LINE_HALF_WIDTH 4.5f
#define GREEN_GRID_DEPTH_BIAS 15.0f
#define GREEN_GRID_ALPHA 0x7F
#define GREEN_GRID_DIM_ALPHA 0x3F
#define GREEN_GRID_FADE_STEP 8
#define GREEN_GRID_FADE_END_ALPHA 0x48
#define GAME_FLAG_HIDE_GREEN_GRID 0x79
#define GAME_STATE_UNK6 6
#define GAME_STATE_UNK11 11
/* g_RoundHud.greenGridState values */
#define GREEN_GRID_HIDDEN 0
#define GREEN_GRID_SHOWN 1 /* Samples the ground and scrolls */
#define GREEN_GRID_FADING 2
#define TINT_CHANNEL_MAX 0xFF

typedef struct GridScreenPoint {
  /* 0x00 */ s32 x; /* In quarter pixels */
  /* 0x04 */ s32 y;
  /* 0x08 */ s32 isVisible;
  /* 0x0C */ s32 depth;
} GridScreenPoint; /* size = 0x10 */

extern s32 D_800BB0E8;
/* The quad that each segment of the ball trail, and of the green grid, moves
 * into place on the screen */
extern Vtx f_BallTrailVertices[4];
extern s32 f_GreenGridAlpha;
extern f32 f_GreenGridScroll;
extern Mtx f_GreenGridMatrix;
extern s32 f_GreenGridHeights[GREEN_GRID_POINT_COUNT];

#ifdef NON_MATCHING
/*
 * Sets red, green and blue for a point of the grid from its slope. White
 * lines warm towards red where the ground rises and cool towards blue where
 * it falls; black lines turn red or blue.
 */
#define SET_GREEN_GRID_TINT(slope)                          \
  if ((s8)g_Players[g_CurrentShot.playerIndex].gridColor == \
      GRID_COLOR_WHITE) {                                   \
    blue = TINT_CHANNEL_MAX;                                \
    if ((slope) == 0) {                                     \
      green = TINT_CHANNEL_MAX;                             \
      red = TINT_CHANNEL_MAX;                               \
    } else if ((slope) > 0) {                               \
      red = TINT_CHANNEL_MAX;                               \
      blue = (slope);                                       \
      blue = red - blue;                                    \
      green = blue;                                         \
    } else {                                                \
      green = (slope) + TINT_CHANNEL_MAX;                   \
      red = green;                                          \
    }                                                       \
  } else {                                                  \
    blue = 0;                                               \
    if ((slope) == 0) {                                     \
      green = 0;                                            \
      red = 0;                                              \
    } else if ((slope) > 0) {                               \
      red = (slope);                                        \
      blue = 0;                                             \
      green = 0;                                            \
    } else {                                                \
      blue = -(slope);                                      \
      green = 0;                                            \
      red = 0;                                              \
    }                                                       \
  }

void DrawGreenGrid(Gfx** displayListHead, CameraMatrices* matrices) {
  f32 points[GREEN_GRID_POINT_COUNT][3];
  GridScreenPoint screenPoints[GREEN_GRID_POINT_COUNT];
  s32 slopes[GREEN_GRID_POINT_COUNT];
  s32 unused[GREEN_GRID_POINT_COUNT * 3];
  CoursePoint center;
  ScreenPoint projected;
  Gfx* gfx = *displayListHead;
  Club* club;
  s32 distance;
  s32 worldDistance;
  s32 rowCount;
  s32 columnCount = GREEN_GRID_COLUMN_COUNT;
  s32 maxPuttDistance;
  s32 row;
  s32 column;
  s32 i;
  s32 rowStart;
  s32 point;
  s32 rowPosition;
  s32 isUnscrolledRow;
  s32 neighbor;
  s32 red;
  s32 green;
  s32 blue;
  f32 yaw;
  f32 sideAngle;
  f32 rowShift;
  f32 columnSpacing;
  f32 rowSpacing;
  f32 forwardX;
  f32 forwardZ;
  f32 sideX;
  f32 sideZ;
  f32 shift;
  f32 centering;
  f32 originX;
  f32 originZ;
  f32 stepX;
  f32 stepZ;
  f32 scrollX;
  f32 scrollZ;
  f32 height;
  f32 dx;
  f32 dy;
  f32 scale;

  if (g_RoundHud.greenGridState == GREEN_GRID_HIDDEN || g_IsCupViewShown == 1 ||
      IsGameFlagSet(GAME_FLAG_HIDE_GREEN_GRID) ||
      (g_GameState.unk0 != GAME_STATE_UNK6 &&
          g_GameState.unk0 != GAME_STATE_UNK11)) {
    return;
  }
  club = GetSelectedClub();
  yaw = (u16)g_CurrentShot.unk0 * BINARY_ANGLE_TO_RADIANS;
  distance = GetSelectedShotDistance();
  worldDistance = GetSelectedShotWorldDistance();
  if (club->surfaceMask & CLUB_SURFACE_GREEN) {
    maxPuttDistance = GREEN_GRID_MAX_PUTT_DISTANCE;
    if (distance == maxPuttDistance) {
      rowShift = GREEN_GRID_PUTT_ROW_SHIFT;
      columnSpacing = GREEN_GRID_PUTT_COLUMN_SPACING;
      rowSpacing = GREEN_GRID_SHORT_ROW_SPACING;
      rowCount = GREEN_GRID_SHORT_ROW_COUNT;
      center[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
      center[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
      center[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
    } else {
      rowShift = GREEN_GRID_PUTT_ROW_SHIFT;
      columnSpacing = GREEN_GRID_PUTT_COLUMN_SPACING;
      rowCount = GREEN_GRID_MAX_ROW_COUNT;
      center[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
      center[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
      center[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
      rowSpacing = distance * GREEN_GRID_PUTT_ROW_SPACING / maxPuttDistance;
    }
  } else if (distance < GREEN_GRID_LANDING_DISTANCE) {
    rowShift = GREEN_GRID_PUTT_ROW_SHIFT;
    columnSpacing = GREEN_GRID_PUTT_COLUMN_SPACING;
    rowCount = GREEN_GRID_MAX_ROW_COUNT;
    center[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
    center[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
    center[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
    rowSpacing =
        distance * GREEN_GRID_ROW_SPACING / GREEN_GRID_MAX_PUTT_DISTANCE;
  } else {
    center[AXIS_X] =
        cosf(yaw) * worldDistance + g_CurrentShot.ballPosition[AXIS_X];
    rowShift = GREEN_GRID_LANDING_ROW_SHIFT;
    rowSpacing = GREEN_GRID_LANDING_ROW_SPACING;
    center[AXIS_Z] =
        sinf(yaw) * worldDistance + g_CurrentShot.ballPosition[AXIS_Z];
    D_800BB0E8 = 1;
    center[AXIS_Y] = GetGroundHeight(center[AXIS_X], center[AXIS_Z]);
    rowCount = GREEN_GRID_LANDING_ROW_COUNT;
    columnSpacing = GREEN_GRID_LANDING_WIDTH / columnCount;
  }
  forwardX = -cosf(yaw);
  forwardZ = -sinf(yaw);
  sideAngle = yaw + M_PI / 2;
  sideX = -cosf(sideAngle) * columnSpacing;
  sideZ = -sinf(sideAngle) * columnSpacing;
  shift = (rowShift - rowCount) * rowSpacing;
  centering = 0.5f - columnCount * 0.5f;
  originX = forwardX * shift + sideX * centering;
  originZ = forwardZ * shift + sideZ * centering;
  stepX = -cosf(yaw) * rowSpacing;
  stepZ = -sinf(yaw) * rowSpacing;
  if (g_RoundHud.greenGridState == GREEN_GRID_SHOWN) {
    f_GreenGridScroll -= GREEN_GRID_SCROLL_STEP;
    if (rowCount != GREEN_GRID_MAX_ROW_COUNT) {
      if (f_GreenGridScroll < 0.0f) {
        f_GreenGridScroll += 1.0f;
      }
    } else if (f_GreenGridScroll < -1.0f) {
      f_GreenGridScroll += 2.0f;
    }
  }
  scrollX = stepX * f_GreenGridScroll;
  scrollZ = stepZ * f_GreenGridScroll;

  for (row = 0, rowStart = 0; row != rowCount;) {
    for (column = 0; column != columnCount; column++) {
      i = rowStart + column;
      isUnscrolledRow = row == rowCount - 1 || row == 0;
      rowPosition = row;
      if (row != 0) {
        rowPosition = row - 1;
      }
      if (isUnscrolledRow || (f_GreenGridScroll < 0.0f && row == 1)) {
        points[i][AXIS_X] =
            center[AXIS_X] + originX + stepX * rowPosition + sideX * column;
        points[i][AXIS_Z] =
            center[AXIS_Z] + originZ + stepZ * rowPosition + sideZ * column;
      } else {
        points[i][AXIS_X] = center[AXIS_X] + originX + stepX * rowPosition +
            sideX * column + scrollX;
        points[i][AXIS_Z] = center[AXIS_Z] + originZ + stepZ * rowPosition +
            sideZ * column + scrollZ;
      }
      if (g_RoundHud.greenGridState == GREEN_GRID_SHOWN) {
        D_800BB0E8 = 1;
        points[i][AXIS_Y] =
            GetGroundHeight(points[i][AXIS_X], points[i][AXIS_Z]);
        f_GreenGridHeights[i] = points[i][AXIS_Y];
      } else {
        points[i][AXIS_Y] = f_GreenGridHeights[i];
      }
    }
    if (rowCount != GREEN_GRID_MAX_ROW_COUNT) {
      rowStart += columnCount;
      row++;
    } else if (row >= GREEN_GRID_FIRST_FULL_ROW) {
      rowStart += columnCount;
      row++;
    } else {
      rowStart += columnCount * 2;
      row += 2;
    }
  }
  /* Fills in the rows that were skipped from their neighbors */
  if (rowCount == GREEN_GRID_MAX_ROW_COUNT) {
    for (row = 1; row != rowCount - 1; row += 2) {
      for (column = 0; column != columnCount; column++) {
        points[row * columnCount + column][AXIS_X] =
            (points[(row - 1) * columnCount + column][AXIS_X] +
                points[(row + 1) * columnCount + column][AXIS_X]) *
            0.5f;
        points[row * columnCount + column][AXIS_Y] =
            (points[(row - 1) * columnCount + column][AXIS_Y] +
                points[(row + 1) * columnCount + column][AXIS_Y]) *
            0.5f;
        points[row * columnCount + column][AXIS_Z] =
            (points[(row - 1) * columnCount + column][AXIS_Z] +
                points[(row + 1) * columnCount + column][AXIS_Z]) *
            0.5f;
      }
    }
  }
  for (row = 0, rowStart = 0; row != rowCount; row++, rowStart += columnCount) {
    for (column = 0; column != columnCount; column++) {
      i = rowStart + column;
      if (ProjectPoint(points[i][AXIS_X] * COURSE_TO_WORLD_SCALE,
              points[i][AXIS_Y] * COURSE_TO_WORLD_SCALE,
              points[i][AXIS_Z] * COURSE_TO_WORLD_SCALE, &projected) < 0) {
        screenPoints[i].x = projected.x * 4;
        screenPoints[i].y = projected.y * 4;
        screenPoints[i].isVisible = TRUE;
        screenPoints[i].depth =
            projected.depth - projected.depthStep * GREEN_GRID_DEPTH_BIAS;
      } else {
        screenPoints[i].isVisible = FALSE;
      }
      height = points[i][AXIS_Y] - center[AXIS_Y];
      if (height < 0.0f) {
        slopes[i] = sqrtf(-height) * GREEN_GRID_SLOPE_SCALE;
      } else if (height > 0.0f) {
        slopes[i] = -sqrtf(height) * GREEN_GRID_SLOPE_SCALE;
      } else {
        slopes[i] = 0;
      }
      if (slopes[i] > GREEN_GRID_MAX_SLOPE) {
        slopes[i] = GREEN_GRID_MAX_SLOPE;
      }
      if (slopes[i] < -GREEN_GRID_MAX_SLOPE) {
        slopes[i] = -GREEN_GRID_MAX_SLOPE;
      }
    }
  }

  guMtxIdent(&f_GreenGridMatrix);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
  gSPSetGeometryMode(gfx++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_ZB_CLD_SURF, G_RM_ZB_CLD_SURF2);
  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PRIM);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_GreenGridMatrix),
      G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
  gSPVertex(gfx++, f_BallTrailVertices, ARRAY_COUNT(f_BallTrailVertices), 0);
  if (g_PauseScreen == PAUSE_SCREEN_UNK4) {
    f_GreenGridAlpha = GREEN_GRID_DIM_ALPHA;
  } else if (g_RoundHud.greenGridState != GREEN_GRID_FADING) {
    f_GreenGridAlpha = GREEN_GRID_ALPHA;
  } else if (f_GreenGridAlpha < GREEN_GRID_FADE_END_ALPHA) {
    f_GreenGridAlpha = GREEN_GRID_DIM_ALPHA;
  } else {
    f_GreenGridAlpha -= GREEN_GRID_FADE_STEP;
  }
  if (g_RoundHud.greenGridState != GREEN_GRID_HIDDEN) {
    /* The lines across the aim, one quad between each pair of points in a row
     */
    for (row = 0, rowStart = 0; row != rowCount;) {
      for (column = 0; column != columnCount; column++) {
        point = rowStart + column;
        if (screenPoints[point].isVisible != TRUE) {
          continue;
        }
        if (!(column & 1)) {
          neighbor = point + 1;
        } else {
          neighbor = point - 1;
        }
        dx = screenPoints[point].x - screenPoints[neighbor].x;
        dy = screenPoints[point].y - screenPoints[neighbor].y;
        if (dx == 0.0f && dy == 0.0f) {
          screenPoints[point].isVisible = FALSE;
          continue;
        }
        scale = GREEN_GRID_LINE_HALF_WIDTH / sqrtf(dx * dx + dy * dy);
        dx *= scale;
        dy *= scale;
        SET_GREEN_GRID_TINT(slopes[point]);
        if (column & 1) {
          gSPModifyVertex(gfx++, 2, G_MWO_POINT_XYSCREEN,
              (screenPoints[point].x << 16) + screenPoints[point].y);
          gSPModifyVertex(gfx++, 3, G_MWO_POINT_XYSCREEN,
              ((screenPoints[point].x + (s32)dy) << 16) +
                  (screenPoints[point].y - (s32)dx));
          gSPModifyVertex(gfx++, 2, G_MWO_POINT_RGBA,
              (red << 24) | (green << 16) | (blue << 8) | f_GreenGridAlpha);
          gSPModifyVertex(gfx++, 3, G_MWO_POINT_RGBA,
              (red << 24) | (green << 16) | (blue << 8) | f_GreenGridAlpha);
          if (screenPoints[point - 1].isVisible == TRUE) {
            gDPPipeSync(gfx++);
            if (screenPoints[point].depth < screenPoints[point - 1].depth) {
              gDPSetPrimDepth(gfx++, screenPoints[point].depth, 0);
            } else {
              gDPSetPrimDepth(gfx++, screenPoints[point - 1].depth, 0);
            }
            gDPPipeSync(gfx++);
            gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
          }
        } else {
          gSPModifyVertex(gfx++, 0, G_MWO_POINT_XYSCREEN,
              (screenPoints[point].x << 16) + screenPoints[point].y);
          gSPModifyVertex(gfx++, 1, G_MWO_POINT_XYSCREEN,
              ((screenPoints[point].x + (s32)dy) << 16) +
                  (screenPoints[point].y - (s32)dx));
          gSPModifyVertex(gfx++, 0, G_MWO_POINT_RGBA,
              (red << 24) | (green << 16) | (blue << 8) | f_GreenGridAlpha);
          gSPModifyVertex(gfx++, 1, G_MWO_POINT_RGBA,
              (red << 24) | (green << 16) | (blue << 8) | f_GreenGridAlpha);
          if (column != 0 && screenPoints[point - 1].isVisible == TRUE) {
            gDPPipeSync(gfx++);
            if (screenPoints[point].depth < screenPoints[point - 1].depth) {
              gDPSetPrimDepth(gfx++, screenPoints[point].depth, 0);
            } else {
              gDPSetPrimDepth(gfx++, screenPoints[point - 1].depth, 0);
            }
            gDPPipeSync(gfx++);
            gSP2Triangles(gfx++, 2, 1, 0, 0, 3, 2, 0, 0);
          }
        }
      }
      if (rowCount != GREEN_GRID_MAX_ROW_COUNT) {
        rowStart += columnCount;
        row++;
      } else if (row >= GREEN_GRID_FIRST_FULL_ROW) {
        rowStart += columnCount;
        row++;
      } else {
        rowStart += columnCount * 2;
        row += 2;
      }
    }
    /* The lines along the aim, one quad between each pair of points in a column
     */
    for (column = 0; column != columnCount; column++) {
      for (row = 0; row != rowCount - 1; row++) {
        point = row * columnCount + column;

        if (screenPoints[point].isVisible != TRUE ||
            screenPoints[point + columnCount].isVisible != TRUE) {
          continue;
        }
        dx = screenPoints[point].x - screenPoints[point + columnCount].x;
        dy = screenPoints[point].y - screenPoints[point + columnCount].y;
        if (dx == 0.0f && dy == 0.0f) {
          continue;
        }
        scale = GREEN_GRID_LINE_HALF_WIDTH / sqrtf(dx * dx + dy * dy);
        dx *= scale;
        dy *= scale;
        SET_GREEN_GRID_TINT(slopes[point]);
        gSPModifyVertex(gfx++, 0, G_MWO_POINT_RGBA,
            (red << 24) | (green << 16) | (blue << 8) | f_GreenGridAlpha);
        gSPModifyVertex(gfx++, 1, G_MWO_POINT_RGBA,
            (red << 24) | (green << 16) | (blue << 8) | f_GreenGridAlpha);
        SET_GREEN_GRID_TINT(slopes[point + columnCount]);
        gSPModifyVertex(gfx++, 2, G_MWO_POINT_RGBA,
            (red << 24) | (green << 16) | (blue << 8) | f_GreenGridAlpha);
        gSPModifyVertex(gfx++, 3, G_MWO_POINT_RGBA,
            (red << 24) | (green << 16) | (blue << 8) | f_GreenGridAlpha);
        gSPModifyVertex(gfx++, 0, G_MWO_POINT_XYSCREEN,
            (screenPoints[point].x << 16) + screenPoints[point].y);
        gSPModifyVertex(gfx++, 1, G_MWO_POINT_XYSCREEN,
            ((screenPoints[point].x - (s32)dy) << 16) +
                (screenPoints[point].y + (s32)dx));
        gSPModifyVertex(gfx++, 2, G_MWO_POINT_XYSCREEN,
            (screenPoints[point + columnCount].x << 16) +
                screenPoints[point + columnCount].y);
        gSPModifyVertex(gfx++, 3, G_MWO_POINT_XYSCREEN,
            ((screenPoints[point + columnCount].x - (s32)dy) << 16) +
                (screenPoints[point + columnCount].y + (s32)dx));
        gDPPipeSync(gfx++);
        if (screenPoints[point].depth <
            screenPoints[point + columnCount].depth) {
          gDPSetPrimDepth(gfx++, screenPoints[point].depth, 0);
        } else {
          gDPSetPrimDepth(gfx++, screenPoints[point + columnCount].depth, 0);
        }
        gDPPipeSync(gfx++);
        gSP2Triangles(gfx++, 1, 0, 2, 0, 1, 2, 3, 0);
        gDPPipeSync(gfx++);
      }
    }
  }
  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PIXEL);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&matrices->projection),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
  *displayListHead = gfx;
}
#else
INCLUDE_RODATA("asm/nonmatchings/round_hud", D_800D1B10);

INCLUDE_ASM("asm/nonmatchings/round_hud", DrawGreenGrid);
#endif

/* Gathers the whole ball trail at the ball, so it starts out empty */
void ResetBallTrail(void) {
  s32 i;

  f_BallTrailHead = 0;
  for (i = 0; i != BALL_TRAIL_LENGTH; i++) {
    f_BallTrail[i][AXIS_X] = g_BallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
    f_BallTrail[i][AXIS_Y] = g_BallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    f_BallTrail[i][AXIS_Z] = g_BallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
  }
}

/* Adds the ball's position to its trail each frame */
#define BALL_TRAIL_RECORDING 1
/* The trail is drawn this far below the ball's center, scaled like the pin */
#define BALL_TRAIL_DROP 0.27f
/* And this many depth steps nearer the camera, scaled like the pin */
#define BALL_TRAIL_DEPTH_BIAS 7
/* The depth of a trail point that is behind the camera */
#define BALL_TRAIL_HIDDEN (-1)
/* Half the trail's width, in quarter pixels */
#define BALL_TRAIL_HALF_WIDTH 4.0f
/* The trail is white; its newest segment has this alpha, fading by one every
 * few segments */
#define BALL_TRAIL_COLOR 0xFFFFFF00
#define BALL_TRAIL_ALPHA 64
#define BALL_TRAIL_SEGMENTS_PER_FADE 4
#define BALL_TRAIL_SEGMENT_COLOR(segment)                          \
  ((BALL_TRAIL_ALPHA - (segment) / BALL_TRAIL_SEGMENTS_PER_FADE) | \
      BALL_TRAIL_COLOR)

/* A point of the ball's trail on the screen, in quarter pixels */
typedef struct TrailPoint {
  /* 0x00 */ s32 x;
  /* 0x04 */ s32 y;
  /* 0x08 */ s32 depth;
} TrailPoint; /* size = 0x0C */

extern s32 D_800C5AAC;
extern TrailPoint f_BallTrailPoints[BALL_TRAIL_LENGTH];
extern Mtx f_BallTrailMatrix;

/*
 * Draws the ball's trail through its recent positions as screen-space quads
 * that fade out with age. While D_800C5AAC is BALL_TRAIL_RECORDING it also
 * adds the ball's current position.
 *
 * Doesn't match: a display list pointer's spill is scheduled differently,
 * and the offsets' floating-point registers are swapped.
 */
#ifdef NON_MATCHING
void DrawBallTrail(Gfx** displayListHead, CameraMatrices* matrices) {
  Gfx* gfx = *displayListHead;
  ScreenPoint point;
  s32 i;
  s32 index;
  f32 deltaX;
  f32 deltaY;
  f32 scale;
  f32 offsetX;
  f32 offsetY;

  if (D_800C5AAC != 0 && g_RoundHud.unk17) {
    if (D_800C5AAC == BALL_TRAIL_RECORDING) {
      if (++f_BallTrailHead == BALL_TRAIL_LENGTH) {
        f_BallTrailHead = 0;
      }
      f_BallTrail[f_BallTrailHead][AXIS_X] =
          g_BallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
      f_BallTrail[f_BallTrailHead][AXIS_Y] =
          g_BallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE -
          g_PinVertexScale * BALL_TRAIL_DROP;
      f_BallTrail[f_BallTrailHead][AXIS_Z] =
          g_BallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    }
    if (g_IsCupViewShown != 1) {
      /* Projects the trail from its newest position back */
      index = f_BallTrailHead;
      for (i = 0; i != BALL_TRAIL_LENGTH; i++, index--) {
        if (index == -1) {
          index = BALL_TRAIL_LENGTH - 1;
        }
        if (ProjectPoint(f_BallTrail[index][AXIS_X], f_BallTrail[index][AXIS_Y],
                f_BallTrail[index][AXIS_Z], &point) < 0) {
          f_BallTrailPoints[i].x = point.x * 4;
          f_BallTrailPoints[i].y = point.y * 4;
          f_BallTrailPoints[i].depth = point.depth -
              point.depthStep * (g_PinVertexScale * BALL_TRAIL_DEPTH_BIAS);
        } else {
          f_BallTrailPoints[i].depth = BALL_TRAIL_HIDDEN;
        }
      }
      guMtxIdent(&f_BallTrailMatrix);
      gDPPipeSync(gfx++);
      gDPPipeSync(gfx++);
      gDPSetCycleType(gfx++, G_CYC_1CYCLE);
      gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
      gSPSetGeometryMode(
          gfx++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
      gDPPipeSync(gfx++);
      gDPSetTexturePersp(gfx++, G_TP_PERSP);
      gDPPipeSync(gfx++);
      gDPSetTextureFilter(gfx++, G_TF_BILERP);
      gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
      gDPPipeSync(gfx++);
      gDPSetDepthSource(gfx++, G_ZS_PRIM);
      gDPPipeSync(gfx++);
      gDPSetAlphaCompare(gfx++, G_AC_NONE);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
      gDPPipeSync(gfx++);
      gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix),
          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
      gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_BallTrailMatrix),
          G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
      gSPVertex(
          gfx++, f_BallTrailVertices, ARRAY_COUNT(f_BallTrailVertices), 0);
      for (i = 0; i != BALL_TRAIL_LENGTH - 1; i++) {
        if (f_BallTrailPoints[i].depth != BALL_TRAIL_HIDDEN &&
            f_BallTrailPoints[i + 1].depth != BALL_TRAIL_HIDDEN) {
          deltaX = f_BallTrailPoints[i].x - f_BallTrailPoints[i + 1].x;
          deltaY = f_BallTrailPoints[i].y - f_BallTrailPoints[i + 1].y;
          /* The empty branch for a segment of no length is there to match */
          if (deltaX == 0.0f & deltaY == 0.0f) {
          } else {
            scale = BALL_TRAIL_HALF_WIDTH /
                sqrtf(deltaX * deltaX + deltaY * deltaY);
            offsetX = deltaY * scale;
            offsetY = deltaX * scale;
            gDPPipeSync(gfx++);
            gDPSetPrimDepth(gfx++, f_BallTrailPoints[i].depth, 0);
            gDPPipeSync(gfx++);
            gSPModifyVertex(
                gfx++, 0, G_MWO_POINT_RGBA, BALL_TRAIL_SEGMENT_COLOR(i));
            gSPModifyVertex(
                gfx++, 1, G_MWO_POINT_RGBA, BALL_TRAIL_SEGMENT_COLOR(i));
            gSPModifyVertex(
                gfx++, 2, G_MWO_POINT_RGBA, BALL_TRAIL_SEGMENT_COLOR(i));
            gSPModifyVertex(
                gfx++, 3, G_MWO_POINT_RGBA, BALL_TRAIL_SEGMENT_COLOR(i));
            gSPModifyVertex(gfx++, 0, G_MWO_POINT_XYSCREEN,
                ((f_BallTrailPoints[i].x + (s32)offsetX) << 16) +
                    (f_BallTrailPoints[i].y - (s32)offsetY));
            gSPModifyVertex(gfx++, 1, G_MWO_POINT_XYSCREEN,
                ((f_BallTrailPoints[i].x - (s32)offsetX) << 16) +
                    (f_BallTrailPoints[i].y + (s32)offsetY));
            gSPModifyVertex(gfx++, 2, G_MWO_POINT_XYSCREEN,
                ((f_BallTrailPoints[i + 1].x + (s32)offsetX) << 16) +
                    (f_BallTrailPoints[i + 1].y - (s32)offsetY));
            gSPModifyVertex(gfx++, 3, G_MWO_POINT_XYSCREEN,
                ((f_BallTrailPoints[i + 1].x - (s32)offsetX) << 16) +
                    (f_BallTrailPoints[i + 1].y + (s32)offsetY));
            gSP2Triangles(gfx++, 1, 0, 2, 0, 1, 2, 3, 0);
          }
        }
      }
      gDPPipeSync(gfx++);
      gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&matrices->projection),
          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
      gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
      gDPPipeSync(gfx++);
      gDPSetDepthSource(gfx++, G_ZS_PIXEL);
      gDPPipeSync(gfx++);
      *displayListHead = gfx;
    }
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/round_hud", DrawBallTrail);
#endif

/*
 * Draws the wind's icon, which pulses in size, and the wind's speed in
 * meters below it. Both lean away from the given point along the viewed wind.
 *
 * Doesn't match: the target reads *displayListHead before the other loads and
 * schedules the last commands before reading
 * g_RoundHud.areWindSpeedArrowsShown. Its rodata also has an empty string after
 * "%dm", which this doesn't emit.
 */
#ifdef NON_MATCHING
void DrawWindSpeed(Gfx** displayListHead, f32 x, f32 y) {
  Gfx* gfx = *displayListHead;
  s32 halfWidthRounded;
  s32 halfHeightRounded;
  s32 textureS;
  s32 textureT;
  s32 dsdx;
  s32 dtdy;
  s32 speed;
  f32 halfWidth;
  f32 halfHeight;
  f32 centerX;
  f32 centerY;
  s32 roundedCenterX;
  s32 roundedCenterY;

  /* Draws text of digits, spaces and 'm' from the wind speed font, with its top
   * left at the given point */
  void DrawWindSpeedText(s32 textX, s32 textY, char* text) {
    s32 offsetX;
    s32 glyphS;
    s32 glyphWidth;

    for (offsetX = 0; *text != '\0'; text++, offsetX += WIND_DIGIT_WIDTH) {
      if (*text == ' ') {
        continue;
      }
      if (*text == 'm') {
        glyphS = WIND_METERS_GLYPH_S;
        glyphWidth = WIND_METERS_GLYPH_WIDTH;
      } else {
        glyphS = (*text - '0') * WIND_DIGIT_WIDTH;
        glyphWidth = WIND_DIGIT_WIDTH;
      }
      gSPScisTextureRectangle(gfx++, (textX + offsetX) << 2,
          (textY - WIND_TEXT_RAISE) << 2, (textX + offsetX + glyphWidth) << 2,
          (textY - WIND_TEXT_RAISE + WIND_FONT_HEIGHT) << 2, G_TX_RENDERTILE,
          glyphS << 5, 0, 1 << 10, 1 << 10);
    }
  }

  f_WindIconPulseAngle += WIND_ICON_PULSE_STEP;
  speed = (u32)g_HoleConditions.windSpeed * WIND_SPEED_TO_METERS_SCALE /
      WIND_SPEED_TO_METERS_DIVISOR;
  if (speed >= WIND_SPEED_TWO_DIGITS) {
    halfWidth = sinf(f_WindIconPulseAngle) + WIND_ICON_WIDE_HALF_WIDTH;
  } else {
    halfWidth = sinf(f_WindIconPulseAngle) + WIND_ICON_NARROW_HALF_WIDTH;
  }
  halfHeight = sinf(f_WindIconPulseAngle) + WIND_ICON_HALF_HEIGHT;
  halfWidthRounded = halfWidth + 0.5f;
  halfHeightRounded = halfHeight + 0.5f;
  dsdx = (WIND_ICON_TEXTURE_WIDTH / 2 << 10) / halfWidth;
  dtdy = (WIND_ICON_TEXTURE_HEIGHT / 2 << 10) / halfHeight;
  /* Moves the texture by the rounding, in 10.5 fixed point, from half a texel
   * in */
  textureS = HALF_TEXEL - (halfWidthRounded - halfWidth) * HALF_TEXEL;
  textureT = HALF_TEXEL - (halfHeightRounded - halfHeight) * HALF_TEXEL;
  /* The centers are kept positive by WIND_POSITIVE_BIAS, so rounding by
   * truncation works */
  if (speed >= WIND_SPEED_TWO_DIGITS) {
    centerX = -sinf(g_SkyState.viewedWindAngles[0]) * WIND_WIDE_LEAN_X +
        (x + WIND_OFFSET_X) + WIND_POSITIVE_BIAS;
  } else {
    centerX = -sinf(g_SkyState.viewedWindAngles[0]) * WIND_NARROW_LEAN_X +
        (x + WIND_OFFSET_X) + WIND_POSITIVE_BIAS;
  }
  centerY = -cosf(g_SkyState.viewedWindAngles[0]) * WIND_LEAN_Y +
      (SCREEN_HEIGHT / 2 - y) - WIND_OFFSET_Y + WIND_POSITIVE_BIAS;
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
  gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0, 0, 0, 0,
      PRIMITIVE, 0, 0, 0, TEXEL0);
  gDPLoadTextureBlock_4b(gfx++, ((u32)f_WindIconImage + IMAGE_HEADER_SIZE) & ~7,
      G_IM_FMT_I, WIND_ICON_TEXTURE_WIDTH, WIND_ICON_TEXTURE_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  roundedCenterX = centerX + 0.5f;
  roundedCenterY = centerY + 0.5f;
  gSPScisTextureRectangle(gfx++,
      (roundedCenterX - WIND_POSITIVE_BIAS + WIND_ICON_OFFSET_X -
          halfWidthRounded)
          << 2,
      (roundedCenterY - WIND_POSITIVE_BIAS + WIND_ICON_OFFSET_Y -
          halfHeightRounded)
          << 2,
      (roundedCenterX - WIND_POSITIVE_BIAS + WIND_ICON_OFFSET_X +
          halfWidthRounded)
          << 2,
      (roundedCenterY - WIND_POSITIVE_BIAS + WIND_ICON_OFFSET_Y +
          halfHeightRounded)
          << 2,
      G_TX_RENDERTILE, textureS, textureT, dsdx, dtdy);
  gDPPipeSync(gfx++);
  SetUpTextDrawing(&gfx);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, WIND_TEXT_RENDER_MODE, WIND_TEXT_RENDER_MODE2);
  gDPPipeSync(gfx++);
  gDPSetAlphaDither(gfx++, G_AD_DISABLE);
  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_DISABLE);
  gDPSetPrimColor(gfx++, 0, 0, WIND_TEXT_PRIM_RED, WIND_TEXT_PRIM_GREEN,
      WIND_TEXT_PRIM_BLUE, 255);
  gDPSetEnvColor(
      gfx++, WIND_TEXT_ENV_RED, WIND_TEXT_ENV_GREEN, WIND_TEXT_ENV_BLUE, 255);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(gfx++, 0, ((u32)f_WindSpeedFont + IMAGE_HEADER_SIZE) & ~7);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTextureBlock_4b(gfx++,
      ((u32)f_WindSpeedFont + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
      WIND_FONT_WIDTH, WIND_FONT_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  sprintf(g_PrintBuffer, "%dm", speed);
  if (speed >= WIND_SPEED_TWO_DIGITS) {
    DrawWindSpeedText(centerX - WIND_POSITIVE_BIAS - WIND_WIDE_TEXT_SHIFT_X,
        centerY - WIND_POSITIVE_BIAS, g_PrintBuffer);
  } else {
    DrawWindSpeedText(centerX - WIND_POSITIVE_BIAS,
        centerY - WIND_POSITIVE_BIAS, g_PrintBuffer);
  }
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  if (g_RoundHud.areWindSpeedArrowsShown == 1) {
    if (speed != 0) {
      DrawPauseArrows(&gfx, centerX - WIND_ARROWS_OFFSET_X,
          centerY - WIND_ARROWS_OFFSET_Y, WIND_ARROWS_WIDE_GAP);
    } else {
      DrawPauseArrows(&gfx, centerX - WIND_ARROWS_OFFSET_X,
          centerY - WIND_ARROWS_OFFSET_Y, WIND_ARROWS_NARROW_GAP);
    }
  }
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/round_hud", DrawWindSpeedText);

INCLUDE_ASM("asm/nonmatchings/round_hud", DrawWindSpeed);
#endif

/*
 * Draws the wind indicator at the given point. DrawWindSpeed's part is drawn
 * after the model while the viewed wind angle is between a quarter and three
 * quarters of a turn, and before it otherwise.
 */
void DrawWindIndicator(Gfx** displayListHead, f32 x, f32 y) {
  Vec3 viewDirection;
  Vec3 rotation;
  Vec3 translation;
  Vec3 unusedScale;
  Mat4 matrix;
  Mat4 transform;
  Gfx* gfx = *displayListHead;

  viewDirection[AXIS_X] =
      g_Cameras[0].view.lookAt[AXIS_X] - g_Cameras[0].view.eye[AXIS_X];
  viewDirection[AXIS_Z] =
      g_Cameras[0].view.lookAt[AXIS_Z] - g_Cameras[0].view.eye[AXIS_Z];
  viewDirection[AXIS_Y] =
      g_Cameras[0].view.lookAt[AXIS_Y] - g_Cameras[0].view.eye[AXIS_Y];
  rotation[AXIS_X] = atan2(viewDirection[AXIS_Y],
                         sqrtf(viewDirection[AXIS_X] * viewDirection[AXIS_X] +
                             viewDirection[AXIS_Z] * viewDirection[AXIS_Z])) +
      M_PI + WIND_INDICATOR_TILT;
  rotation[AXIS_Y] = 0.0f;
  rotation[AXIS_Z] = 0.0f;
  translation[AXIS_X] = x;
  translation[AXIS_Y] = y;
  translation[AXIS_Z] = WIND_INDICATOR_DEPTH;
  BuildRotationMatrix(matrix, rotation, translation);
  guRotateRPYF(transform, 0.0f,
      WIND_INDICATOR_FACING_DEGREES -
          g_SkyState.viewedWindAngles[0] * 360.0f * (1.0 / (2 * M_PI)),
      0.0f);
  guMtxCatF(transform, matrix, matrix);
  guScaleF(transform, WIND_INDICATOR_SCALE, WIND_INDICATOR_SCALE,
      WIND_INDICATOR_SCALE);
  guMtxCatF(transform, matrix, matrix);
  guMtxF2L(matrix, &f_WindIndicatorMatrices[g_MatrixBufferIndex]);
  if (!(g_SkyState.viewedWindAngles[0] > M_PI / 2 &&
          g_SkyState.viewedWindAngles[0] < 3 * M_PI / 2)) {
    DrawWindSpeed(&gfx, x, y);
  }
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gSPMatrix(gfx++,
      OS_K0_TO_PHYSICAL(&f_WindIndicatorMatrices[g_MatrixBufferIndex]),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
  gSPSetLights1(gfx++, f_WindArrowLights);
  if (g_IsModuleAlive[MODULE_UNK9]) {
    g_ModuleRdpRequestFrames[MODULE_UNK9] = g_FrameCount;
    gSPDisplayList(gfx++, D_80222400);
  }
  if (g_SkyState.viewedWindAngles[0] > M_PI / 2 &&
      g_SkyState.viewedWindAngles[0] < 3 * M_PI / 2) {
    DrawWindSpeed(&gfx, x, y);
  }
  *displayListHead = gfx;
}

/* Lets the second controller's C buttons nudge the HUD up and down, and prints
 * the offset */
void AdjustHudOffset(void) {
  if (IsGameFlagSet(GAME_FLAG_ADJUST_HUD_OFFSET)) {
    if (g_ControllerStates[HUD_OFFSET_CONTROLLER].pressedButtons & U_CBUTTONS) {
      f_HudOffsetY--;
    }
    if (g_ControllerStates[HUD_OFFSET_CONTROLLER].pressedButtons & D_CBUTTONS) {
      f_HudOffsetY++;
    }
    sprintf(g_PrintBuffer, "%3d", f_HudOffsetY);
    PrintDebugText(g_PrintBuffer, HUD_OFFSET_TEXT_COLUMN, HUD_OFFSET_TEXT_ROW);
  }
}

#define MODULE_PAUSE_MENU 20
#define MODULE_UNK25 25
/* Where the shot panel's wind indicator sits, unless the shot is on the green,
 * where the wind doesn't count */
#define SHOT_PANEL_WIND_INDICATOR_X 100.0f
#define SHOT_PANEL_WIND_INDICATOR_Y 76.0f
/* The arrows beside a pause menu panel; they take screen coordinates, while the
 * panels sit in the centred view */
#define PAUSE_PANEL_ARROWS_GAP 64
/* The shot panel's icons, in the centred orthographic view and in front of the
 * camera */
#define CLUB_HEAD_ICON_X -128.0f
#define CLUB_HEAD_ICON_Y -76.0f
#define LIE_ICON_X 112
#define LIE_ICON_Y -70.0f
#define SHOT_PANEL_ICON_Z -300.0f
/* The icons tilt by half of their angle */
#define SHOT_PANEL_ICON_TILT 0.5f
/* RoundHud.flags bits that light the club head icon fully, otherwise it is
 * dimmed */
#define HUD_FLAG_LIGHT_CLUB_HEAD 0x2
#define SHOT_PANEL_ICON_DIM 191
#define CLUB_HEAD_IMAGE_SIZE 32
/* The lie image is two halves, each its own texture on its own quad */
#define LIE_IMAGE_WIDTH 48
#define LIE_IMAGE_HALF_HEIGHT 24
#define LIE_IMAGE_HALF_SIZE (LIE_IMAGE_WIDTH * LIE_IMAGE_HALF_HEIGHT * 2)

extern Vtx f_ClubHeadIconVertices[4];
extern Vtx f_LieIconVertices[8];
extern Mtx f_ClubHeadIconMatrices[2];
extern Mtx f_LieIconMatrices[2];
extern s32 f_ClubNameImageIndex;
/* The name image of each club slot */
extern u16 f_ClubNameAssets[];

/*
 * Draws the shot panel's wind indicator, club head and lie, or the wind
 * and lie panels beside the pause menu while it sets them
 */
void DrawShotPanelIcons(Gfx** displayListHead, CameraView* view) {
  Vec3 rotation;
  Vec3 translation;
  Vec3 unusedScale;
  Mat4 matrix;
  s32 unused[18];
  Gfx* gfx;
  Club* club;
  f32 clubHeadAngle;
  f32 lieAngle;

  gfx = *displayListHead;
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
  gSPSetGeometryMode(gfx++, G_SHADE | G_CULL_BACK);
  if (g_RoundHud.areWindSpeedArrowsShown == TRUE ||
      g_RoundHud.areWindDirectionArrowsShown == TRUE) {
    if (g_IsModuleAlive[MODULE_PAUSE_MENU]) {
      DrawWindIndicator(
          &gfx, g_WindPanelPosition[AXIS_X], g_WindPanelPosition[AXIS_Y]);
      if (g_RoundHud.areWindDirectionArrowsShown == TRUE) {
        DrawPauseArrows(&gfx, g_WindPanelPosition[AXIS_X] + SCREEN_WIDTH / 2,
            SCREEN_HEIGHT / 2 - g_WindPanelPosition[AXIS_Y],
            PAUSE_PANEL_ARROWS_GAP);
      }
    }
  } else if (g_HoleConditions.windSpeed != 0 &&
      g_RoundHud.isWindIndicatorShown &&
      GetSurfaceInfo(g_CurrentShot.surface)->kind != SURFACE_KIND_GREEN &&
      g_IsCupViewShown == 0) {
    DrawWindIndicator(&gfx, f_ShotPanelOffsetX + SHOT_PANEL_WIND_INDICATOR_X,
        SHOT_PANEL_WIND_INDICATOR_Y);
  }
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  gDPPipeSync(gfx++);
  gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
  gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH | G_CULL_BACK);
  if (g_IsCupViewShown == 0 && g_IsModuleAlive[MODULE_UNK25]) {
    DrawClubPanelIfShown(&gfx);
  }
  if (g_RoundHud.isClubHeadIconShown && g_IsCupViewShown == 0) {
    club = GetClub(
        g_Players[g_CurrentShot.playerIndex].clubs[g_CurrentShot.clubSlot]);
    if (club->headImageAsset != f_ClubHeadImageAsset) {
      EncodedAsset asset;

      f_ClubHeadImageAsset = club->headImageAsset;
      OpenEncodedAsset(club->headImageAsset, &asset);
      ReadEncodedAsset(f_ClubHeadImage, &asset);
      f_ClubNameImageIndex ^= 1;
      OpenEncodedAsset(f_ClubNameAssets[g_CurrentShot.clubSlot], &asset);
      ReadEncodedAsset(f_ClubNameImages[f_ClubNameImageIndex], &asset);
    }
    clubHeadAngle = g_RoundHud.clubHeadTilt * BINARY_ANGLE_TO_RADIANS;
    if (clubHeadAngle > M_PI) {
      clubHeadAngle -= 2 * M_PI;
    }
    rotation[AXIS_X] = 0;
    rotation[AXIS_Y] = 0;
    rotation[AXIS_Z] = -clubHeadAngle * SHOT_PANEL_ICON_TILT;
    translation[AXIS_X] = CLUB_HEAD_ICON_X - f_ShotPanelOffsetX;
    translation[AXIS_Y] = CLUB_HEAD_ICON_Y - f_HudOffsetY;
    translation[AXIS_Z] = SHOT_PANEL_ICON_Z;
    BuildRotationMatrix(matrix, rotation, translation);
    ConvertMatrixToFixed(matrix, &f_ClubHeadIconMatrices[g_MatrixBufferIndex]);
    gDPPipeSync(gfx++);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
    if (g_RoundHud.flags & HUD_FLAG_LIGHT_CLUB_HEAD) {
      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    } else {
      gDPSetPrimColor(gfx++, 0, 0, SHOT_PANEL_ICON_DIM, SHOT_PANEL_ICON_DIM,
          SHOT_PANEL_ICON_DIM, 255);
    }
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPLoadTextureBlock(gfx++, ((u32)f_ClubHeadImage + IMAGE_HEADER_SIZE) & ~7,
        G_IM_FMT_RGBA, G_IM_SIZ_16b, CLUB_HEAD_IMAGE_SIZE, CLUB_HEAD_IMAGE_SIZE,
        0, G_TX_WRAP, G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++,
        OS_K0_TO_PHYSICAL(&f_ClubHeadIconMatrices[g_MatrixBufferIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPVertex(gfx++, f_ClubHeadIconVertices, 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPPipeSync(gfx++);
  }
  if ((g_RoundHud.isLieIconShown || g_RoundHud.areLieArrowsShown) &&
      g_IsCupViewShown == 0) {
    lieAngle = g_RoundHud.lieTilt * BINARY_ANGLE_TO_RADIANS;
    if (lieAngle > M_PI) {
      lieAngle -= 2 * M_PI;
    }
    rotation[AXIS_X] = 0;
    rotation[AXIS_Y] = 0;
    rotation[AXIS_Z] = lieAngle * SHOT_PANEL_ICON_TILT;
    if (g_RoundHud.isLieIconShown) {
      translation[AXIS_X] = f_ShotPanelOffsetX + LIE_ICON_X;
      translation[AXIS_Y] = LIE_ICON_Y - f_HudOffsetY;
      translation[AXIS_Z] = SHOT_PANEL_ICON_Z;
    } else if (g_IsModuleAlive[MODULE_PAUSE_MENU]) {
      translation[AXIS_X] = g_LiePanelPosition[AXIS_X];
      translation[AXIS_Y] = g_LiePanelPosition[AXIS_Y];
      translation[AXIS_Z] = SHOT_PANEL_ICON_Z;
      DrawPauseArrows(&gfx, g_LiePanelPosition[AXIS_X] + SCREEN_WIDTH / 2,
          SCREEN_HEIGHT / 2 - g_LiePanelPosition[AXIS_Y],
          PAUSE_PANEL_ARROWS_GAP);
    }
    BuildRotationMatrix(matrix, rotation, translation);
    ConvertMatrixToFixed(matrix, &f_LieIconMatrices[g_MatrixBufferIndex]);
    if (g_RoundHud.flags & HUD_FLAG_ANIMATE_OFFSET_MARKER) {
      gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    } else {
      gDPSetPrimColor(gfx++, 0, 0, SHOT_PANEL_ICON_DIM, SHOT_PANEL_ICON_DIM,
          SHOT_PANEL_ICON_DIM, 255);
    }
    gDPPipeSync(gfx++);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)f_LieImage + IMAGE_HEADER_SIZE) & ~7);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_LieIconMatrices[g_MatrixBufferIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPVertex(gfx++, f_LieIconVertices, 8, 0);
    gDPLoadTextureBlock(gfx++, ((u32)f_LieImage + IMAGE_HEADER_SIZE) & ~7,
        G_IM_FMT_RGBA, G_IM_SIZ_16b, LIE_IMAGE_WIDTH, LIE_IMAGE_HALF_HEIGHT, 0,
        G_TX_WRAP, G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPLoadTextureBlock(gfx++,
        ((u32)f_LieImage + IMAGE_HEADER_SIZE + LIE_IMAGE_HALF_SIZE) & ~7,
        G_IM_FMT_RGBA, G_IM_SIZ_16b, LIE_IMAGE_WIDTH, LIE_IMAGE_HALF_HEIGHT, 0,
        G_TX_WRAP, G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSP2Triangles(gfx++, 4, 5, 6, 0, 4, 6, 7, 0);
    gDPPipeSync(gfx++);
  }
  *displayListHead = gfx;
}

/*
 * Draws the flag grid with the hole image on it. With isBackDrawn, it then
 * draws the back too, by changing the grid's texture coordinates to the
 * back's and mirroring its shades around HOLE_FLAG_START_SHADE.
 *
 * Doesn't match: the loop that modifies the back's vertices allocates its
 * registers differently.
 */
#ifdef NON_MATCHING
void DrawHoleFlag(s32 isBackDrawn) {
  s32 row;
  s32 column;
  s32 index;
  Vtx* vertex;

  gDPPipeSync(g_DisplayListHead++);
  gDPSetCombineMode(g_DisplayListHead++, G_CC_MODULATERGBA, G_CC_MODULATERGBA);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
  gDPSetPrimColor(g_DisplayListHead++, 0, 0, 0, 0, 255, 255);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetAlphaCompare(g_DisplayListHead++, G_AC_NONE);
  gDPSetBlendColor(g_DisplayListHead++, 255, 255, 255, 1);
  gDPLoadTLUT_pal256(
      g_DisplayListHead++, ((u32)f_HoleImage + IMAGE_HEADER_SIZE) & ~7);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTextureLUT(g_DisplayListHead++, G_TT_RGBA16);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
  gSPTexture(g_DisplayListHead++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPLoadTextureBlock(g_DisplayListHead++,
      ((u32)f_HoleImage + LARGE_PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
      G_IM_SIZ_8b, HOLE_IMAGE_WIDTH, HOLE_IMAGE_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(g_DisplayListHead++);
  gSPVertex(g_DisplayListHead++, f_HoleFlagVertices[0][g_MatrixBufferIndex],
      HOLE_FLAG_VERTEX_COUNT, 0);
  for (row = 0; row != HOLE_FLAG_ROW_COUNT - 1; row++) {
    for (column = 0; column != HOLE_FLAG_COLUMN_COUNT - 1; column++) {
      index = row * HOLE_FLAG_COLUMN_COUNT + column;
      gSP2Triangles(g_DisplayListHead++, index, index + HOLE_FLAG_COLUMN_COUNT,
          index + HOLE_FLAG_COLUMN_COUNT + 1, 0, index,
          index + HOLE_FLAG_COLUMN_COUNT + 1, index + 1, 0);
    }
  }
  if (isBackDrawn == TRUE) {
    index = 0;
    vertex = f_HoleFlagVertices[0][g_MatrixBufferIndex];
    for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
      for (column = 0; column != HOLE_FLAG_COLUMN_COUNT;
          column++, index++, vertex++) {
        /* These texture coordinates are after the gSPTexture scale, which
         * halves them */
        gSPModifyVertex(g_DisplayListHead++, index, G_MWO_POINT_ST,
            ((HOLE_FLAG_BACK_TEXEL_START / 2 -
                 column * (HOLE_FLAG_TEXEL_COLUMN_STEP / 2))
                << 16) +
                row * (HOLE_FLAG_TEXEL_ROW_STEP / 2));
        gSPModifyVertex(g_DisplayListHead++, index, G_MWO_POINT_RGBA,
            ((2 * HOLE_FLAG_START_SHADE - vertex->v.cn[0]) << 24) |
                ((2 * HOLE_FLAG_START_SHADE - vertex->v.cn[1]) << 16) |
                ((2 * HOLE_FLAG_START_SHADE - vertex->v.cn[2]) << 8) |
                HOLE_FLAG_START_SHADE);
      }
    }
    for (row = 0; row != HOLE_FLAG_ROW_COUNT - 1; row++) {
      for (column = 0; column != HOLE_FLAG_COLUMN_COUNT - 1; column++) {
        index = row * HOLE_FLAG_COLUMN_COUNT + column;
        gSP2Triangles(g_DisplayListHead++, index,
            index + HOLE_FLAG_COLUMN_COUNT + 1, index + HOLE_FLAG_COLUMN_COUNT,
            0, index, index + 1, index + HOLE_FLAG_COLUMN_COUNT + 1, 0);
      }
    }
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/round_hud", DrawHoleFlag);
#endif

/* Draws the hole image as a model in the perspective of the screen effects */
void DrawHoleImageModel(void) {
  Vec3 translation;
  Vec3 scale;
  Vec3 rotation;
  Mat4 matrix;

  rotation[AXIS_X] = 0.0f;
  rotation[AXIS_Y] = 0.0f;
  rotation[AXIS_Z] = 0.0f;
  translation[AXIS_X] = HOLE_IMAGE_MODEL_X;
  translation[AXIS_Y] = HOLE_IMAGE_MODEL_Y;
  translation[AXIS_Z] = HOLE_IMAGE_MODEL_Z;
  scale[AXIS_X] = HOLE_IMAGE_MODEL_SCALE;
  scale[AXIS_Y] = HOLE_IMAGE_MODEL_SCALE;
  scale[AXIS_Z] = HOLE_IMAGE_MODEL_SCALE;
  BuildTransformMatrix(matrix, rotation, translation, scale);
  ConvertMatrixToFixed(matrix, &f_HoleImageMatrix);
  gDPPipeSync(g_DisplayListHead++);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
  gSPClearGeometryMode(g_DisplayListHead++, 0xFFFFFFFF);
  gSPSetGeometryMode(
      g_DisplayListHead++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetRenderMode(g_DisplayListHead++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
  gSPPerspNormalize(g_DisplayListHead++, g_ScreenEffectPerspNorm);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
  gSPMatrix(g_DisplayListHead++, OS_K0_TO_PHYSICAL(&g_ScreenEffectProjection),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  gSPMatrix(g_DisplayListHead++, OS_K0_TO_PHYSICAL(&f_HoleImageMatrix),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
  DrawHoleFlag(FALSE);
  gDPPipeSync(g_DisplayListHead++);
}

/* Starts one of the effects over a shot in flight, or stops it with
 * SHOT_EFFECT_NONE */
void StartShotEffect(s32 effect) {
  s32 contactStyle;

  if (effect == SHOT_EFFECT_NONE) {
    f_ShotEffectTime = SHOT_EFFECT_TIME_OFF;
    return;
  }
  if (effect == SHOT_EFFECT_LONG_SHOT) {
    contactStyle = GetContactStyle(&g_Players[g_RoundHud.unk87]);
    if (contactStyle != CONTACT_STYLE_GAME_BOY_BALL_7 &
        contactStyle != CONTACT_STYLE_WARIO) {
      PlaySoundWithEffects(SOUND_LONG_SHOT, LONG_SHOT_SOUND_CHANNEL,
          LONG_SHOT_SOUND_VOLUME, LONG_SHOT_SOUND_REVERB, LONG_SHOT_SOUND_PAN);
    } else {
      PlaySoundWithEffects(SOUND_LONG_SHOT_HEAVY, LONG_SHOT_SOUND_CHANNEL,
          LONG_SHOT_HEAVY_SOUND_VOLUME, LONG_SHOT_SOUND_REVERB,
          LONG_SHOT_SOUND_PAN);
    }
  }
  f_ShotEffect = effect;
  f_ShotEffectTime = 0.0f;
}

/* The effect grows and shrinks over half a turn of its time */
#define SHOT_EFFECT_TIME_STEP 0.2f
#define SHOT_EFFECT_SCALE 0.5f
#define SHOT_EFFECT_SPIN_START 0.7f
#define SHOT_EFFECT_SPIN_RATE 0.5f
#define SHOT_EFFECT_Z -100.0f
/* The heavy long shot of the Wario and Game Boy ball 7 contact styles, an
 * animation that flickers round */
#define HEAVY_LONG_SHOT_SCALE 0.4f
#define HEAVY_LONG_SHOT_FRAME_COUNT 6
#define HEAVY_LONG_SHOT_FRAME_TICKS 4
#define HEAVY_LONG_SHOT_FRAMES_OFFSET 0x970
#define HEAVY_LONG_SHOT_FRAME_SIZE 0xC08
#define HEAVY_LONG_SHOT_FRAME_WIDTH 48
#define HEAVY_LONG_SHOT_FRAME_HEIGHT 128
#define HEAVY_LONG_SHOT_ALPHA 191
#define LONG_SHOT_GROUP_COUNT 4
#define HEAVY_LONG_SHOT_QUAD_COUNT 2

extern Mtx f_ShotEffectMatrices[2];
extern Vec3 f_ShotEffectTranslation;
extern Vec3 f_ShotEffectScale;
extern Vec3 f_ShotEffectRotation;
/* Four groups of eight vertices, each three pairs of triangles */
extern Vtx f_LongShotVertices[LONG_SHOT_GROUP_COUNT * 8];
extern Vtx f_HeavyLongShotVertices[16];
extern s32 f_HeavyLongShotFrameAssets[HEAVY_LONG_SHOT_FRAME_COUNT];
extern s32 f_HeavyLongShotFrame;
extern s32 f_HeavyLongShotFrameTimer;
/* The two frame buffers in g_SharedEffectImages, each loaded in turn and both
 * drawn */
extern s32 f_HeavyLongShotBufferIndex;

/* Draws the effect that StartShotEffect started at the ball, until its time
 * runs out */
void DrawShotEffect(Gfx** displayListHead) {
  Mat4 matrix;
  ScreenPoint point;
  EncodedAsset asset;
  Gfx* gfx = *displayListHead;
  s32 contactStyle;
  f32 scale;
  s32 i;

  if (f_ShotEffectTime < 0.0f) {
    return;
  }
  f_ShotEffectTime += SHOT_EFFECT_TIME_STEP;
  if (f_ShotEffectTime > M_PI) {
    f_ShotEffectTime = SHOT_EFFECT_TIME_OFF;
    return;
  }
  if (ProjectPoint(g_BallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
          g_BallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE,
          g_BallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE, &point) >= 0) {
    return;
  }
  contactStyle = GetContactStyle(&g_Players[g_RoundHud.unk87]);
  if (contactStyle != CONTACT_STYLE_GAME_BOY_BALL_7 &
      contactStyle != CONTACT_STYLE_WARIO) {
    scale = sinf(f_ShotEffectTime) * SHOT_EFFECT_SCALE;
    f_ShotEffectRotation[AXIS_X] = 0.0f;
    f_ShotEffectRotation[AXIS_Y] = 0.0f;
    f_ShotEffectRotation[AXIS_Z] =
        (f_ShotEffectTime + SHOT_EFFECT_SPIN_START) * SHOT_EFFECT_SPIN_RATE;
  } else {
    scale = sinf(f_ShotEffectTime) * HEAVY_LONG_SHOT_SCALE;
    f_ShotEffectRotation[AXIS_X] = 0.0f;
    f_ShotEffectRotation[AXIS_Y] = 0.0f;
    f_ShotEffectRotation[AXIS_Z] = (guRandom() % 360) * (M_PI / 180);
  }
  f_ShotEffectScale[AXIS_X] = scale;
  f_ShotEffectScale[AXIS_Y] = scale;
  f_ShotEffectScale[AXIS_Z] = scale;
  f_ShotEffectTranslation[AXIS_X] = point.x - SCREEN_WIDTH / 2;
  f_ShotEffectTranslation[AXIS_Y] = -point.y + SCREEN_HEIGHT / 2;
  f_ShotEffectTranslation[AXIS_Z] = SHOT_EFFECT_Z;
  BuildTransformMatrix(
      matrix, f_ShotEffectRotation, f_ShotEffectTranslation, f_ShotEffectScale);
  ConvertMatrixToFixed(matrix, &f_ShotEffectMatrices[g_MatrixBufferIndex]);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
  gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetAlphaDither(gfx++, G_AD_DISABLE);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPSetBlendColor(gfx++, 255, 255, 255, 1);
  if (contactStyle != CONTACT_STYLE_GAME_BOY_BALL_7 &
      contactStyle != CONTACT_STYLE_WARIO) {
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_OFF);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 255, 255);
    gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    if (f_ShotEffect == SHOT_EFFECT_LONG_SHOT) {
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    } else {
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    }
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gfx++,
        OS_K0_TO_PHYSICAL(&f_ShotEffectMatrices[g_MatrixBufferIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPVertex(gfx++, f_LongShotVertices, LONG_SHOT_GROUP_COUNT * 8, 0);
    for (i = 0; i != LONG_SHOT_GROUP_COUNT; i++) {
      gSP2Triangles(gfx++, i * 8, i * 8 + 1, i * 8 + 2, 0, i * 8, i * 8 + 2,
          i * 8 + 3, 0);
      gSP2Triangles(gfx++, i * 8 + 1, i * 8 + 4, i * 8 + 5, 0, i * 8 + 1,
          i * 8 + 5, i * 8 + 2, 0);
      gSP2Triangles(gfx++, i * 8 + 4, i * 8 + 6, i * 8 + 7, 0, i * 8 + 4,
          i * 8 + 7, i * 8 + 5, 0);
    }
  } else {
    if (++f_HeavyLongShotFrameTimer == HEAVY_LONG_SHOT_FRAME_TICKS) {
      f_HeavyLongShotFrameTimer = 0;
      f_HeavyLongShotFrame =
          (f_HeavyLongShotFrame + 1) % HEAVY_LONG_SHOT_FRAME_COUNT;
      OpenEncodedAsset(
          f_HeavyLongShotFrameAssets[f_HeavyLongShotFrame], &asset);
      ReadEncodedAsset(g_SharedEffectImages + HEAVY_LONG_SHOT_FRAMES_OFFSET +
              f_HeavyLongShotBufferIndex * HEAVY_LONG_SHOT_FRAME_SIZE,
          &asset);
      f_HeavyLongShotBufferIndex ^= 1;
    }
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    if (contactStyle == CONTACT_STYLE_GAME_BOY_BALL_7) {
      gDPSetEnvColor(gfx++, 255, 255, 127, 255);
      gDPSetPrimColor(gfx++, 0, 0, guRandom() % 255, guRandom() % 255,
          guRandom() % 255, HEAVY_LONG_SHOT_ALPHA);
    } else {
      gDPSetPrimColor(gfx++, 0, 0, 191, 0, 191, HEAVY_LONG_SHOT_ALPHA);
      gDPSetEnvColor(gfx++, 255, 255, 255, 255);
    }
    gDPSetCombineLERP(gfx++, ENVIRONMENT, PRIMITIVE, TEXEL0, PRIMITIVE,
        PRIMITIVE, 0, TEXEL0, 0, ENVIRONMENT, PRIMITIVE, TEXEL0, PRIMITIVE,
        PRIMITIVE, 0, TEXEL0, 0);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gfx++,
        OS_K0_TO_PHYSICAL(&f_ShotEffectMatrices[g_MatrixBufferIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPVertex(gfx++, f_HeavyLongShotVertices, 16, 0);
    gDPLoadTextureBlock_4b(gfx++,
        ((u32)g_SharedEffectImages + HEAVY_LONG_SHOT_FRAMES_OFFSET +
            IMAGE_HEADER_SIZE) &
            ~7,
        G_IM_FMT_I, HEAVY_LONG_SHOT_FRAME_WIDTH, HEAVY_LONG_SHOT_FRAME_HEIGHT,
        0, G_TX_WRAP, G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
    gDPPipeSync(gfx++);
    for (i = 0; i != HEAVY_LONG_SHOT_QUAD_COUNT; i++) {
      gSP2Triangles(gfx++, i * 4, i * 4 + 1, i * 4 + 2, 0, i * 4, i * 4 + 2,
          i * 4 + 3, 0);
    }
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(gfx++,
        ((u32)g_SharedEffectImages + HEAVY_LONG_SHOT_FRAMES_OFFSET +
            HEAVY_LONG_SHOT_FRAME_SIZE + IMAGE_HEADER_SIZE) &
            ~7,
        G_IM_FMT_I, HEAVY_LONG_SHOT_FRAME_WIDTH, HEAVY_LONG_SHOT_FRAME_HEIGHT,
        0, G_TX_WRAP, G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
    gDPPipeSync(gfx++);
    for (i = 0; i != HEAVY_LONG_SHOT_QUAD_COUNT; i++) {
      gSP2Triangles(gfx++, i * 4, i * 4 + 1, i * 4 + 2, 0, i * 4, i * 4 + 2,
          i * 4 + 3, 0);
    }
    gDPPipeSync(gfx++);
  }
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/*
 * Waves the hole's flag in the wind. A shade swings down the flagstick side
 * of the flag and spreads across it, faster the stronger the wind. Each
 * vertex then bulges out of the flag by its shade, and the columns draw in to
 * make up for the bulges.
 *
 * factor and otherFactor hold several unrelated factors in turn, as in the
 * original; separate variables change the registers.
 *
 * Doesn't match: the zero for the skew factors goes in otherFactor's register
 * first instead of factor's.
 */
#ifdef NON_MATCHING
void WaveHoleFlag(void) {
  s32 row;
  s32 column;
  s32 shade;
  s32 rowShade;
  s32 depth;
  s32 pull;
  f32 windSpeed;
  f32 factor;
  f32 otherFactor;
  f32 bulge;

  ScrollWindArrow();
  windSpeed = g_HoleConditions.windSpeed / WIND_SPEED_UNIT;
  factor = HOLE_FLAG_SKEW;
  otherFactor = factor;
  for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
    for (column = 1; column != HOLE_FLAG_COLUMN_COUNT; column++) {
      HOLE_FLAG_VERTEX(row, column).v.ob[0] =
          (s16)(column * HOLE_FLAG_CELL_SIZE - column * factor) -
          row * otherFactor * HOLE_FLAG_ROW_SKEW_SCALE;
      if (HOLE_FLAG_VERTEX(row, column).v.ob[0] < column * otherFactor) {
        HOLE_FLAG_VERTEX(row, column).v.ob[0] = column * otherFactor;
      }
      HOLE_FLAG_VERTEX(row, column).v.ob[1] =
          -row * HOLE_FLAG_CELL_SIZE - column * factor;
    }
  }
  if (windSpeed > 0.0f && windSpeed < HOLE_FLAG_MIN_WIND_SPEED) {
    windSpeed = HOLE_FLAG_MIN_WIND_SPEED;
  }
  /* factor is how much of its neighbor's shade a vertex takes; otherFactor is
   * how much of its own it keeps */
  if (windSpeed < 1.0f) {
    factor = HOLE_FLAG_CALM_SPREAD;
    otherFactor = 1.0f - HOLE_FLAG_CALM_SPREAD;
  } else if (windSpeed > HOLE_FLAG_GALE_WIND_SPEED) {
    factor = 1.0f;
    otherFactor = 0.0f;
  } else {
    factor = windSpeed * HOLE_FLAG_SPREAD_STEP + HOLE_FLAG_SPREAD_STEP;
    otherFactor =
        (HOLE_FLAG_SPREAD_STEP_COUNT - windSpeed) * HOLE_FLAG_SPREAD_STEP -
        HOLE_FLAG_SPREAD_STEP;
  }
  for (column = HOLE_FLAG_COLUMN_COUNT - 1; column != 0; column--) {
    for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
      /* Indexing the left neighbor as HOLE_FLAG_SHADE(row, column - 1) changes
       * the code */
      shade =
          f_HoleFlagShades[row * HOLE_FLAG_COLUMN_COUNT + column - 1] * factor +
          HOLE_FLAG_SHADE(row, column) * otherFactor;
      HOLE_FLAG_SHADE(row, column) = shade;
    }
  }
  f_HoleFlagWavePhase += (HOLE_FLAG_SWING_DEGREES_PER_SPEED * windSpeed +
                             HOLE_FLAG_MIN_SWING_DEGREES) *
      DEGREES_TO_RADIANS(1);
  if (f_HoleFlagWavePhase > 2 * M_PI) {
    f_HoleFlagWavePhase -= 2 * M_PI;
  }
  if (f_HoleFlagWavePhase < 0.0f) {
    f_HoleFlagWavePhase += 2 * M_PI;
  }
  /* factor is the swing's amplitude here */
  factor = windSpeed + rand() % HOLE_FLAG_SWING_JITTER_RANGE;
  shade = (s32)(sinf(f_HoleFlagWavePhase) * factor) + HOLE_FLAG_START_SHADE;
  for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
    if ((s32)windSpeed == 0) {
      rowShade = shade;
    } else {
      rowShade =
          (f32)(shade +
              rand() % (s32)(windSpeed * HOLE_FLAG_ROW_JITTER_PER_SPEED)) -
          HOLE_FLAG_ROW_JITTER_PER_SPEED / 2 * windSpeed;
    }
    HOLE_FLAG_SHADE(row, 0) = rowShade;
  }
  for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
    for (column = 1; column != HOLE_FLAG_COLUMN_COUNT; column++) {
      HOLE_FLAG_VERTEX(row, column).v.cn[0] = HOLE_FLAG_SHADE(row, column);
      HOLE_FLAG_VERTEX(row, column).v.cn[1] = HOLE_FLAG_SHADE(row, column);
      HOLE_FLAG_VERTEX(row, column).v.cn[2] = HOLE_FLAG_SHADE(row, column);
    }
  }
  /* Bulges are deeper toward the flag's free edge and its top */
  factor = windSpeed * HOLE_FLAG_BULGE_PER_SPEED + HOLE_FLAG_MIN_BULGE;
  for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
    for (column = 1; column != HOLE_FLAG_COLUMN_COUNT; column++) {
      bulge =
          (f32)(HOLE_FLAG_VERTEX(row, column).v.cn[0] - HOLE_FLAG_START_SHADE) /
          (f32)(HOLE_FLAG_BULGE_COLUMN_BASE - column) * factor;
      otherFactor = HOLE_FLAG_BULGE_SCALE;
      HOLE_FLAG_VERTEX(row, column).v.ob[2] = bulge * otherFactor *
          (f32)(HOLE_FLAG_BULGE_ROW_BASE - row) * HOLE_FLAG_BULGE_ROW_SCALE;
    }
  }
  for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
    pull = 0;
    for (column = HOLE_FLAG_FIRST_PULLED_COLUMN;
        column != HOLE_FLAG_COLUMN_COUNT; column++) {
      depth = HOLE_FLAG_VERTEX(row, column).v.ob[2];
      if (depth < 0) {
        depth = -depth;
      }
      pull += depth / HOLE_FLAG_DEPTH_PER_PULL;
      HOLE_FLAG_VERTEX(row, column).v.ob[0] =
          column * HOLE_FLAG_CELL_SIZE - pull;
    }
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/round_hud", WaveHoleFlag);
#endif

/* Moves the shot offset marker and its trail back to the center */
void ResetShotOffsetMarker(void) {
  s32 i;

  for (i = 0; i != SHOT_OFFSET_MARKER_COUNT; i++) {
    f_ShotOffsetMarkerXs[i] = 0;
    f_ShotOffsetMarkerYs[i] = 0;
  }
}

/*
 * Moves the shot offset marker to the given offset. The test is inverted, so
 * the length is only measured when it is zero and the offset is never
 * clamped to SHOT_OFFSET_MAX_RADIUS.
 */
void SetShotOffsetMarker(s32 x, s32 y) {
  f32 length;
  f32 scale;

  if (x == 0 && y == 0) {
    length = sqrtf(x * x + y * y);
  } else {
    length = 0.0f;
  }
  if (length > SHOT_OFFSET_MAX_RADIUS) {
    scale = SHOT_OFFSET_MAX_RADIUS / length;
    x = x * scale;
    y = y * scale;
  }
  f_ShotOffsetMarkerXs[0] = x / SHOT_OFFSET_UNITS_PER_PIXEL;
  f_ShotOffsetMarkerYs[0] = -y / SHOT_OFFSET_UNITS_PER_PIXEL;
}

/* Draws the shot offset marker, animated while HUD_FLAG_ANIMATE_OFFSET_MARKER
 * is set */
#ifdef NON_MATCHING
void DrawShotOffsetMarker(void) {
  gDPPipeSync(g_DisplayListHead++);
  gDPLoadTLUT_pal16(g_DisplayListHead++, 0,
      ((u32)f_ShotOffsetMarkerImage + IMAGE_HEADER_SIZE) & ~7);
  f_ShotOffsetMarkerFrame =
      (f_ShotOffsetMarkerFrame + 1) % SHOT_OFFSET_MARKER_FRAME_COUNT;
  if (g_RoundHud.flags & HUD_FLAG_ANIMATE_OFFSET_MARKER) {
    f_ShotOffsetMarkerTexelOffset =
        f_ShotOffsetMarkerFrame * SHOT_OFFSET_MARKER_FRAME_SIZE;
  } else {
    f_ShotOffsetMarkerTexelOffset =
        SHOT_OFFSET_MARKER_STILL_FRAME * SHOT_OFFSET_MARKER_FRAME_SIZE;
  }
  gDPLoadTextureBlock_4b(g_DisplayListHead++,
      ((u32)f_ShotOffsetMarkerImage + PALETTED_TEXELS_OFFSET +
          f_ShotOffsetMarkerTexelOffset) &
          ~7,
      G_IM_FMT_CI, SHOT_OFFSET_MARKER_SIZE, SHOT_OFFSET_MARKER_SIZE, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(g_DisplayListHead++);
  gSPScisTextureRectangle(g_DisplayListHead++,
      (f_ShotOffsetMarkerXs[0] + (f_ShotPanelOffsetX + SHOT_OFFSET_MARKER_X))
          << 2,
      (f_ShotOffsetMarkerYs[0] + (f_ShotPanelOffsetY + SHOT_OFFSET_MARKER_Y) +
          f_HudOffsetY)
          << 2,
      (f_ShotOffsetMarkerXs[0] + (f_ShotPanelOffsetX + SHOT_OFFSET_MARKER_X) +
          SHOT_OFFSET_MARKER_SIZE)
          << 2,
      (f_ShotOffsetMarkerYs[0] +
          (f_ShotPanelOffsetY + SHOT_OFFSET_MARKER_Y +
              SHOT_OFFSET_MARKER_SIZE) +
          f_HudOffsetY)
          << 2,
      G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
}
#else
INCLUDE_ASM("asm/nonmatchings/round_hud", DrawShotOffsetMarker);
#endif

/* The icon cycles through ANIMATED_ICON_FRAME_COUNT images, a new one every few
 * frames */
#define ANIMATED_ICON_FRAME_COUNT 8
#define ANIMATED_ICON_FRAME_LENGTH 4
#define ANIMATED_ICON_SIZE 32
/* The icon eases a fifth of the way toward its place each frame */
#define ANIMATED_ICON_X 96.0f
#define ANIMATED_ICON_Y 24.0f
#define ANIMATED_ICON_EASING 0.2f
#define ANIMATED_ICON_MODE_UNK2 2
#define ANIMATED_ICON_LABEL_WIDTH 64
#define ANIMATED_ICON_LABEL_HEIGHT 16
#define ANIMATED_ICON_LABEL_OFFSET_X 24
#define ANIMATED_ICON_LABEL_UNK0 0
#define ANIMATED_ICON_LABEL_UNK1 1
#define ANIMATED_ICON_LABEL_UNK2 2
extern u32 f_AnimatedIconFrameAssets[ANIMATED_ICON_FRAME_COUNT];
extern s32 f_AnimatedIconFrameTime;
extern s32 f_AnimatedIconFrame;
extern s32 f_AnimatedIconImageIndex;
extern f32 f_AnimatedIconX;
extern f32 f_AnimatedIconY;

/*
 * Draws an animated icon that eases into place, with a label beside it that
 * depends on D_800C308C. Each new frame of the icon is read into the same
 * image.
 */
void DrawAnimatedIcon(Gfx** displayListHead, s32 mode) {
  Gfx* gfx = *displayListHead;
  EncodedAsset asset;
  s32 label;

  if (f_SceneryState == SCENERY_LOADED) {
    if (mode == ANIMATED_ICON_MODE_UNK2) {
      f_AnimatedIconX = 0.0f;
      f_AnimatedIconY = 0.0f;
    } else {
      f_AnimatedIconX +=
          (ANIMATED_ICON_X - f_AnimatedIconX) * ANIMATED_ICON_EASING;
      f_AnimatedIconY +=
          (ANIMATED_ICON_Y - f_AnimatedIconY) * ANIMATED_ICON_EASING;
    }
    if (++f_AnimatedIconFrameTime == ANIMATED_ICON_FRAME_LENGTH) {
      f_AnimatedIconFrameTime = 0;
      f_AnimatedIconFrame =
          (f_AnimatedIconFrame + 1) % ANIMATED_ICON_FRAME_COUNT;
      OpenEncodedAsset(f_AnimatedIconFrameAssets[f_AnimatedIconFrame], &asset);
      ReadEncodedAsset(f_AnimatedIconImages[f_AnimatedIconImageIndex], &asset);
    }
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++,
        (u32)f_AnimatedIconImages[f_AnimatedIconImageIndex] +
                IMAGE_HEADER_SIZE &
            ~7);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(
        gfx++, HOLE_INTRO_IMAGE_RENDER_MODE, HOLE_INTRO_IMAGE_RENDER_MODE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTextureBlock(gfx++,
        (u32)f_AnimatedIconImages[f_AnimatedIconImageIndex] +
                LARGE_PALETTED_TEXELS_OFFSET &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, ANIMATED_ICON_SIZE, ANIMATED_ICON_SIZE, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, (s32)f_AnimatedIconX << 2,
        (s32)f_AnimatedIconY << 2,
        ((s32)f_AnimatedIconX + ANIMATED_ICON_SIZE) << 2,
        ((s32)f_AnimatedIconY + ANIMATED_ICON_SIZE) << 2, G_TX_RENDERTILE, 0, 0,
        1 << 10, 1 << 10);
    gDPPipeSync(gfx++);
    if (D_800C308C == 1) {
      label = ANIMATED_ICON_LABEL_UNK1;
    } else if (D_800C308C == 2) {
      label = ANIMATED_ICON_LABEL_UNK0;
    } else {
      label = ANIMATED_ICON_LABEL_UNK2;
    }
    gDPLoadTLUT_pal16(gfx++, 0,
        (u32)f_AnimatedIconLabelImages[label] + IMAGE_HEADER_SIZE & ~7);
    gDPLoadTextureBlock_4b(gfx++,
        (u32)f_AnimatedIconLabelImages[label] + PALETTED_TEXELS_OFFSET & ~7,
        G_IM_FMT_CI, ANIMATED_ICON_LABEL_WIDTH, ANIMATED_ICON_LABEL_HEIGHT, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++,
        ((s32)f_AnimatedIconX + ANIMATED_ICON_LABEL_OFFSET_X) << 2,
        (s32)f_AnimatedIconY << 2,
        ((s32)f_AnimatedIconX + ANIMATED_ICON_LABEL_OFFSET_X +
            ANIMATED_ICON_LABEL_WIDTH)
            << 2,
        ((s32)f_AnimatedIconY + ANIMATED_ICON_LABEL_HEIGHT) << 2,
        G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    *displayListHead = gfx;
  }
}

/*
 * Draws black bars at the top and bottom of the HUD's area, growing them each
 * frame up to LETTERBOX_MAX_HEIGHT. Returns their new height.
 */
s32 DrawLetterbox(Gfx** gfxP, s32 height) {
  Gfx* gfx = *gfxP;

  if (g_RoundHud.isLetterboxShown) {
    if (D_800C308C != 0 & D_800C308C != LETTERBOX_STATE_UNK5) {
      gDPPipeSync(gfx++);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(
          gfx++, G_RM_XLU_SURF | ZMODE_XLU, G_RM_XLU_SURF2 | ZMODE_XLU);
      gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
      gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 255);
      gDPPipeSync(gfx++);
      if (height > 0) {
        gSPScisTextureRectangle(gfx++, HUD_MARGIN << 2, HUD_MARGIN << 2,
            (SCREEN_WIDTH - HUD_MARGIN) << 2, (HUD_MARGIN + height) << 2,
            G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        gSPScisTextureRectangle(gfx++, HUD_MARGIN << 2,
            (SCREEN_HEIGHT - HUD_MARGIN - height) << 2,
            (SCREEN_WIDTH - HUD_MARGIN) << 2, (SCREEN_HEIGHT - HUD_MARGIN) << 2,
            G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
      }
      gDPPipeSync(gfx++);
      height += LETTERBOX_GROWTH_STEP;
      if (height > LETTERBOX_MAX_HEIGHT) {
        height = LETTERBOX_MAX_HEIGHT;
      }
    } else {
      height = 0;
    }
  }
  *gfxP = gfx;
  return height;
}

/*
 * Draws a distance such as "12.5m" with the loaded distance glyphs, from its
 * top left corner. Spaces are skipped without advancing.
 */
void DrawDistanceText(Gfx** gfxP, s32 x, s32 y, char* text) {
  s32 width;
  s32 glyphS;
  Gfx* gfx = *gfxP;

  for (; *text != '\0'; text++) {
    switch (*text) {
      case ' ':
        continue;
      case '.':
        width = DISTANCE_POINT_WIDTH;
        glyphS = DISTANCE_POINT_S;
        break;
      case 'm':
        width = DISTANCE_DIGIT_WIDTH;
        glyphS = DISTANCE_METRES_S;
        break;
      case 'y':
        width = DISTANCE_DIGIT_WIDTH;
        glyphS = DISTANCE_YARDS_S;
        break;
      case 'f':
        width = DISTANCE_FEET_WIDTH;
        glyphS = DISTANCE_FEET_S;
        break;
      default:
        width = DISTANCE_DIGIT_WIDTH;
        glyphS = (*text - '0') * DISTANCE_DIGIT_WIDTH;
        break;
    }
    gSPTextureRectangle(gfx++, x << 2, y << 2, (x + width) << 2,
        (y + DISTANCE_GLYPH_HEIGHT) << 2, G_TX_RENDERTILE, glyphS << 5, 0,
        1 << 10, 1 << 10);
    x += width;
  }
  *gfxP = gfx;
}

INCLUDE_RODATA("asm/nonmatchings/round_hud", f_ColorTestSampleText);

INCLUDE_RODATA("asm/nonmatchings/round_hud", f_BlankText);

/* Draws the colour test screen and handles its controls */
void DrawColorTest(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

  SetUpGradientTextDrawing(&gfx, f_IsTestGradientShown);
  gDPSetPrimColor(gfx++, 0, 0, f_TestColors[COLOR_TEST_PRIM][0],
      f_TestColors[COLOR_TEST_PRIM][1], f_TestColors[COLOR_TEST_PRIM][2],
      COLOR_CHANNEL_MAX);
  gDPSetEnvColor(gfx++, f_TestColors[COLOR_TEST_ENV][0],
      f_TestColors[COLOR_TEST_ENV][1], f_TestColors[COLOR_TEST_ENV][2],
      COLOR_CHANNEL_MAX);
  gDPPipeSync(gfx++);
  /* The casts read only the low halves, as the target does */
  DrawCenteredLargeText(
      &gfx, (s16)f_TestTextX, (s16)f_TestTextY, f_ColorTestSampleText);
  gDPPipeSync(gfx++);
  PrintDebugText(f_BlankText,
      f_TestChannelIndex * COLOR_TEST_COLUMN_WIDTH + COLOR_TEST_TEXT_COLUMN - 1,
      f_TestColorIndex + COLOR_TEST_TEXT_ROW);
  if (g_ControllerStates[COLOR_TEST_CONTROLLER].repeatedButtons & U_CBUTTONS) {
    f_TestColorIndex ^= 1;
  }
  if (g_ControllerStates[COLOR_TEST_CONTROLLER].repeatedButtons & D_CBUTTONS) {
    f_TestColorIndex ^= 1;
  }
  if (g_ControllerStates[COLOR_TEST_CONTROLLER].repeatedButtons & L_CBUTTONS) {
    f_TestChannelIndex = (f_TestChannelIndex + COLOR_TEST_CHANNEL_COUNT - 1) %
        COLOR_TEST_CHANNEL_COUNT;
  }
  if (g_ControllerStates[COLOR_TEST_CONTROLLER].repeatedButtons & R_CBUTTONS) {
    f_TestChannelIndex = (f_TestChannelIndex + 1) % COLOR_TEST_CHANNEL_COUNT;
  }
  if (g_ControllerStates[COLOR_TEST_CONTROLLER].buttons & A_BUTTON) {
    f_TestColors[f_TestColorIndex][f_TestChannelIndex] += COLOR_TEST_STEP;
  }
  if (g_ControllerStates[COLOR_TEST_CONTROLLER].buttons & B_BUTTON) {
    f_TestColors[f_TestColorIndex][f_TestChannelIndex] -= COLOR_TEST_STEP;
  }
  if (g_ControllerStates[COLOR_TEST_CONTROLLER].pressedButtons & R_TRIG) {
    f_IsTestGradientShown ^= 1;
  }
  if (g_ControllerStates[COLOR_TEST_CONTROLLER].buttons & U_JPAD) {
    f_TestTextY -= COLOR_TEST_STEP;
  }
  if (g_ControllerStates[COLOR_TEST_CONTROLLER].buttons & D_JPAD) {
    f_TestTextY += COLOR_TEST_STEP;
  }
  if (g_ControllerStates[COLOR_TEST_CONTROLLER].buttons & L_JPAD) {
    f_TestTextX -= COLOR_TEST_STEP;
  }
  if (g_ControllerStates[COLOR_TEST_CONTROLLER].buttons & R_JPAD) {
    f_TestTextX += COLOR_TEST_STEP;
  }
  if (f_TestColors[f_TestColorIndex][f_TestChannelIndex] > COLOR_CHANNEL_MAX) {
    f_TestColors[f_TestColorIndex][f_TestChannelIndex] = COLOR_CHANNEL_MAX;
  }
  if (f_TestColors[f_TestColorIndex][f_TestChannelIndex] < 0) {
    f_TestColors[f_TestColorIndex][f_TestChannelIndex] = 0;
  }
  sprintf(g_PrintBuffer, "%3d  %3d  %3d", f_TestColors[COLOR_TEST_PRIM][0],
      f_TestColors[COLOR_TEST_PRIM][1], f_TestColors[COLOR_TEST_PRIM][2]);
  PrintDebugText(g_PrintBuffer, COLOR_TEST_TEXT_COLUMN,
      COLOR_TEST_TEXT_ROW + COLOR_TEST_PRIM);
  sprintf(g_PrintBuffer, "%3d  %3d  %3d", f_TestColors[COLOR_TEST_ENV][0],
      f_TestColors[COLOR_TEST_ENV][1], f_TestColors[COLOR_TEST_ENV][2]);
  PrintDebugText(g_PrintBuffer, COLOR_TEST_TEXT_COLUMN,
      COLOR_TEST_TEXT_ROW + COLOR_TEST_ENV);
  PrintDebugText(">",
      f_TestChannelIndex * COLOR_TEST_COLUMN_WIDTH + COLOR_TEST_TEXT_COLUMN - 1,
      f_TestColorIndex + COLOR_TEST_TEXT_ROW);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  *displayListHead = gfx;
}

/* The HUD's letterbox height, which DrawLetterbox grows each frame */
extern s32 f_LetterboxHeight;
/* The shot panel slides in from the left by this much a frame */
#define SHOT_PANEL_SLIDE_STEP 4
/*
 * The scoreboard shows each player's face and strokes, their score in the game
 * mode and their contest distances, in rows down the right of the screen. It
 * moves right as the letterbox grows, and left to make room for handicaps.
 */
#define SCOREBOARD_X 246
#define SCOREBOARD_Y 68
#define SCOREBOARD_ROW_HEIGHT 26
#define SCOREBOARD_LETTERBOX_SHIFT 8
#define SCOREBOARD_HANDICAP_SHIFT 10
#define SCOREBOARD_TEXT_X 22
#define SCOREBOARD_SCORE_Y 76
#define SCOREBOARD_CONTEST_Y 84
#define HANDICAP_COLON_X 38
#define HANDICAP_X 42
#define DRIVE_DISTANCE_X 16
#define PIN_DISTANCE_X 8
/* The face of the player whose turn it is sits up and to the left of the others
 */
#define FACE_WIDTH 32
#define FACE_HEIGHT 24
#define FACE_INSET_X 4
#define FACE_INSET_Y 4
#define SMALL_FACE_WIDTH 24
#define SMALL_FACE_HEIGHT 16
/* The others are shrunk to two-thirds, a texel and a half to a pixel */
#define SMALL_FACE_STEP ((1 << 10) * 3 / 2)
/* The pin flag beside the distance to the cup waves through four frames side by
 * side */
extern s32 f_PinFlagTime;
#define PIN_FLAG_TIME_COUNT 16
#define PIN_FLAG_FRAME_LENGTH 4
#define PIN_FLAG_IMAGE_SIZE 32
#define PIN_FLAG_IMAGE_T 16
#define PIN_FLAG_X 24
#define PIN_FLAG_Y 72
#define PIN_FLAG_WIDTH 8
#define PIN_FLAG_HEIGHT 16
/* Where the labels "REST", by the distance to the cup, and "UP", "DOWN" or
 * "FLAT", by the height, sit */
#define SHOT_PANEL_LABEL_X 34
#define SHOT_PANEL_LABEL_WIDTH 32
#define SHOT_PANEL_LABEL_HEIGHT 12
#define REST_LABEL_Y 68
#define HEIGHT_LABEL_Y 80
#define SHOT_PANEL_DISTANCE_X 64
/* In Ring Shot, a spinning ring beside the count of rings still to pass */
extern s32 f_RingFrame;
extern s32 f_RingFrameTime;
#define RING_FRAME_COUNT 3
#define RING_FRAME_LENGTH 6
#define RING_IMAGE_SIZE 32
#define RING_FRAME_SIZE (RING_IMAGE_SIZE * RING_IMAGE_SIZE / 2)
#define RING_ICON_X 18
#define RING_ICON_Y 92
#define RING_COUNT_X 46
#define RING_COUNT_Y 112
#define DISTANCE_FONT_WIDTH 128
extern u16 f_WhiteDistancePalette[];
#define HOLE_YARDAGE_X 60
#define HOLE_YARDAGE_Y 56
#define PAR_IMAGE_X 24
#define PAR_IMAGE_Y 56
/* The club panel, which module 25 updates, and the selected club's name below
 * it */
#define CLUB_PANEL_X 14
#define CLUB_PANEL_Y 150
#define CLUB_PANEL_WIDTH 48
#define CLUB_PANEL_HEIGHT 42
#define CLUB_NAME_X 22
#define CLUB_NAME_Y 200
#define CLUB_NAME_WIDTH 32
#define CLUB_NAME_HEIGHT 16
#define SHOT_PANEL_TEXT_Y 216
#define CLUB_DISTANCE_X 21
#define LIE_PERCENT_X 240
#define UNK28_TEXT_X 36
#define UNK28_TEXT_Y 176
/* How far the ball has gone from where the shot was played */
#define SHOT_LENGTH_X 132
#define SHOT_LENGTH_Y 200
#define PANEL_STOPWATCH_X 32
#define PANEL_STOPWATCH_Y 92
#define MIRROR_TEXTURE_SIZE 32
#define MIRROR_TEXTURE_SIZE_LOG2 5
#define MIRROR_TEXTURE_X 32
#define MIRROR_TEXTURE_Y 122
#define MODULE_UNK18 18
/* A debugging aid that shows the texture the mirror effects draw */
#define GAME_FLAG_SHOW_MIRROR_TEXTURE 0x70
/* The shadowed font draws "ft" for this character */
#define SHADOWED_FEET_GLYPH '['
void func_80217F54(Gfx** displayListHead);

/* Loads one of the shot panel's labels, which DrawRoundHud does in each branch
 * that picks one */
#define LOAD_SHOT_PANEL_LABEL(image)                                     \
  gDPLoadTLUT_pal16(                                                     \
      g_DisplayListHead++, 0, ((u32)(image) + IMAGE_HEADER_SIZE) & ~7);  \
  gDPLoadTextureBlock_4b(g_DisplayListHead++,                            \
      ((u32)(image) + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,         \
      SHOT_PANEL_LABEL_WIDTH, SHOT_PANEL_LABEL_HEIGHT, 0,                \
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, \
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD)

/*
 * Draws the round's HUD over the course: the letterbox, the players' faces
 * and scores, the shot panel's labels, distances and club, and the debugging
 * aids that game flags turn on
 */
#ifdef NON_MATCHING
void DrawRoundHud(CameraView* view) {
  s32 percentRange[2];
  Club* club;
  s32 hasHandicap;
  s32 scoreboardX;
  s32 i;
  s32 ringCount;
  s32 hundredths;
  s32 height;
  UnkB8* player;
  UnkB8* shooter;
  Club* selectedClub;
  SurfaceInfo* surface;
  f32 shotLength;
  s32 contestDistance;
  s32 totalSeconds;

  club = GetClub(
      g_Players[g_CurrentShot.playerIndex].clubs[g_CurrentShot.clubSlot]);
  hasHandicap = FALSE;
  if (f_ShotPanelOffsetX > 0) {
    f_ShotPanelOffsetX -= SHOT_PANEL_SLIDE_STEP;
  }
  WaveHoleFlag();
  gDPPipeSync(g_DisplayListHead++);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
  gDPPipeSync(g_DisplayListHead++);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetAlphaDither(g_DisplayListHead++, G_AD_DISABLE);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetColorDither(g_DisplayListHead++, G_CD_MAGICSQ);
  f_LetterboxHeight = DrawLetterbox(&g_DisplayListHead, f_LetterboxHeight);
  /* The faces move out of the letterbox's way */
  scoreboardX = f_LetterboxHeight * SCOREBOARD_LETTERBOX_SHIFT +
      (SCOREBOARD_X + f_ShotPanelOffsetX);
  for (i = 0; i != g_PlayerCount; i++) {
    if (g_Players[i].handicap > 0) {
      hasHandicap = TRUE;
      scoreboardX -= SCOREBOARD_HANDICAP_SHIFT;
      break;
    }
  }
  gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, 128);
  gDPPipeSync(g_DisplayListHead++);
  gSPTexture(g_DisplayListHead++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTexturePersp(g_DisplayListHead++, G_TP_NONE);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTextureFilter(g_DisplayListHead++, G_TF_POINT);
  gDPPipeSync(g_DisplayListHead++);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetAlphaCompare(g_DisplayListHead++, G_AC_NONE);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTextureLUT(g_DisplayListHead++, G_TT_NONE);
  gDPSetBlendColor(g_DisplayListHead++, 255, 255, 255, 1);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetRenderMode(g_DisplayListHead++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPPipeSync(g_DisplayListHead++);
  if (g_RoundHud.unkD && !g_IsCupViewShown && !IsOnDrivingRange()) {
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTextureLUT(g_DisplayListHead++, G_TT_NONE);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetRenderMode(g_DisplayListHead++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTextureLUT(g_DisplayListHead++, G_TT_RGBA16);
    gDPSetCombineMode(g_DisplayListHead++, G_CC_MODULATEIDECALA_PRIM,
        G_CC_MODULATEIDECALA_PRIM);
    for (i = 0; i != g_GameState.playerCount; i++) {
      gDPLoadTLUT_pal256(
          g_DisplayListHead++, (u32)g_PlayerFacePalettes[i] & ~7);
      gDPLoadTextureBlock(g_DisplayListHead++, (u32)g_PlayerFaceImages[i] & ~7,
          G_IM_FMT_CI, G_IM_SIZ_8b, FACE_WIDTH, FACE_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      /* The face of the player whose turn it is shows full size and bright */
      if (i == g_CurrentShot.playerIndex) {
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTextureFilter(g_DisplayListHead++, G_TF_POINT);
        gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, 255);
        gDPPipeSync(g_DisplayListHead++);
        gSPScisTextureRectangle(g_DisplayListHead++,
            (scoreboardX - FACE_INSET_X) << 2,
            (SCOREBOARD_Y - FACE_INSET_Y + i * SCOREBOARD_ROW_HEIGHT) << 2,
            (scoreboardX - FACE_INSET_X + FACE_WIDTH) << 2,
            (SCOREBOARD_Y - FACE_INSET_Y + FACE_HEIGHT +
                i * SCOREBOARD_ROW_HEIGHT)
                << 2,
            G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
      } else {
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
        gDPSetPrimColor(g_DisplayListHead++, 0, 0, 127, 127, 127, 127);
        gDPPipeSync(g_DisplayListHead++);
        gSPScisTextureRectangle(g_DisplayListHead++, scoreboardX << 2,
            (SCOREBOARD_Y + i * SCOREBOARD_ROW_HEIGHT) << 2,
            (scoreboardX + SMALL_FACE_WIDTH) << 2,
            (SCOREBOARD_Y + SMALL_FACE_HEIGHT + i * SCOREBOARD_ROW_HEIGHT) << 2,
            G_TX_RENDERTILE, 0, 0, SMALL_FACE_STEP, SMALL_FACE_STEP);
      }
    }
    gDPPipeSync(g_DisplayListHead++);
  }
  if (g_RoundHud.unk8 && !g_IsCupViewShown) {
    DrawHoleImageModel();
  }
  DrawShotPanelIcons(&g_DisplayListHead, view);
  DrawLandingReaction(&g_DisplayListHead);
  gDPPipeSync(g_DisplayListHead++);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetRenderMode(g_DisplayListHead++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(g_DisplayListHead++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTexturePersp(g_DisplayListHead++, G_TP_NONE);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTextureFilter(g_DisplayListHead++, G_TF_POINT);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTextureLUT(g_DisplayListHead++, G_TT_RGBA16);
  if (g_RoundHud.isLieIconShown && !g_IsCupViewShown &&
      !(club->surfaceMask & CLUB_SURFACE_GREEN)) {
    DrawShotOffsetMarker();
  }
  if (g_RoundHud.unkD && !g_IsCupViewShown && !IsOnDrivingRange()) {
    f_PinFlagTime = (f_PinFlagTime + 1) & (PIN_FLAG_TIME_COUNT - 1);
    gDPPipeSync(g_DisplayListHead++);
    gDPLoadTLUT_pal16(g_DisplayListHead++, 0,
        ((u32)g_PinFlagImages + IMAGE_HEADER_SIZE) & ~7);
    gDPLoadTextureBlock_4b(g_DisplayListHead++,
        ((u32)g_PinFlagImages + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
        PIN_FLAG_IMAGE_SIZE, PIN_FLAG_IMAGE_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
        G_TX_NOLOD);
    gDPPipeSync(g_DisplayListHead++);
    gSPScisTextureRectangle(g_DisplayListHead++,
        (PIN_FLAG_X - f_ShotPanelOffsetX) << 2, PIN_FLAG_Y << 2,
        (PIN_FLAG_X + PIN_FLAG_WIDTH - f_ShotPanelOffsetX) << 2,
        (PIN_FLAG_Y + PIN_FLAG_HEIGHT) << 2, G_TX_RENDERTILE,
        (f_PinFlagTime / PIN_FLAG_FRAME_LENGTH) * (PIN_FLAG_WIDTH << 5),
        PIN_FLAG_IMAGE_T << 5, 1 << 10, 1 << 10);
    gDPPipeSync(g_DisplayListHead++);
    gDPLoadTLUT_pal16(g_DisplayListHead++, 0,
        ((u32)f_RestLabelImage + IMAGE_HEADER_SIZE) & ~7);
    gDPLoadTextureBlock_4b(g_DisplayListHead++,
        ((u32)f_RestLabelImage + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
        SHOT_PANEL_LABEL_WIDTH, SHOT_PANEL_LABEL_HEIGHT, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(g_DisplayListHead++);
    gSPScisTextureRectangle(g_DisplayListHead++,
        (SHOT_PANEL_LABEL_X - f_ShotPanelOffsetX) << 2, REST_LABEL_Y << 2,
        (SHOT_PANEL_LABEL_X + SHOT_PANEL_LABEL_WIDTH - f_ShotPanelOffsetX) << 2,
        (REST_LABEL_Y + SHOT_PANEL_LABEL_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
        1 << 10, 1 << 10);
    gDPPipeSync(g_DisplayListHead++);
    /* A putt's height is labelled by its height in meters, which the feet are
     * measured from */
    if ((club->surfaceMask & CLUB_SURFACE_GREEN)
            ? g_RoundHud.ballHeightMetersDirection == HEIGHT_LEVEL
            : g_RoundHud.ballHeightYardsDirection == HEIGHT_LEVEL) {
      LOAD_SHOT_PANEL_LABEL(f_FlatLabelImage);
    } else if (g_RoundHud.ballHeightMetersDirection == HEIGHT_ABOVE) {
      LOAD_SHOT_PANEL_LABEL(f_UpLabelImage);
    } else {
      LOAD_SHOT_PANEL_LABEL(f_DownLabelImage);
    }
    gDPPipeSync(g_DisplayListHead++);
    gSPScisTextureRectangle(g_DisplayListHead++,
        (SHOT_PANEL_LABEL_X - f_ShotPanelOffsetX) << 2, HEIGHT_LABEL_Y << 2,
        (SHOT_PANEL_LABEL_X + SHOT_PANEL_LABEL_WIDTH - f_ShotPanelOffsetX) << 2,
        (HEIGHT_LABEL_Y + SHOT_PANEL_LABEL_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
        1 << 10, 1 << 10);
    gDPPipeSync(g_DisplayListHead++);
    if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
      if (f_RingFrameTime++ % RING_FRAME_LENGTH == 0) {
        f_RingFrame = (f_RingFrame + 1) % RING_FRAME_COUNT;
      }
      gDPPipeSync(g_DisplayListHead++);
      gDPSetRenderMode(
          g_DisplayListHead++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
      gDPLoadTLUT_pal16(
          g_DisplayListHead++, 0, ((u32)f_RingImages + IMAGE_HEADER_SIZE) & ~7);
      gDPLoadTextureBlock_4b(g_DisplayListHead++,
          ((u32)f_RingImages + PALETTED_TEXELS_OFFSET +
              f_RingFrame * RING_FRAME_SIZE) &
              ~7,
          G_IM_FMT_CI, RING_IMAGE_SIZE, RING_IMAGE_SIZE, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(g_DisplayListHead++);
      gSPScisTextureRectangle(g_DisplayListHead++,
          (RING_ICON_X - f_ShotPanelOffsetX) << 2, RING_ICON_Y << 2,
          (RING_ICON_X + RING_IMAGE_SIZE - f_ShotPanelOffsetX) << 2,
          (RING_ICON_Y + RING_IMAGE_SIZE) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10,
          1 << 10);
      gDPPipeSync(g_DisplayListHead++);
      gDPPipeSync(g_DisplayListHead++);
      gDPSetRenderMode(g_DisplayListHead++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    }
  }
  gDPLoadTLUT_pal16(
      g_DisplayListHead++, 0, ((u32)f_DistanceFont + IMAGE_HEADER_SIZE) & ~7);
  gDPLoadTextureBlock_4b(g_DisplayListHead++,
      ((u32)f_DistanceFont + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
      DISTANCE_FONT_WIDTH, DISTANCE_GLYPH_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  if (g_RoundHud.unk8 && !g_IsCupViewShown && !IsOnDrivingRange() &&
      !g_IsMiniGolf) {
    sprintf(g_PrintBuffer, "%3dy",
        (s16)g_CourseRecords[GetCourseRecordIndex()]
            .holes[GetHoleRecordIndex()]
            .yardages[0]);
    DrawDistanceText(&g_DisplayListHead, HOLE_YARDAGE_X - f_ShotPanelOffsetX,
        HOLE_YARDAGE_Y, g_PrintBuffer);
  }
  if (g_RoundHud.unkD && !g_IsCupViewShown && !IsOnDrivingRange()) {
    /* The shot panel's distances are white, rather than the font's yellow */
    gDPLoadTLUT_pal16(g_DisplayListHead++, 0, (u32)f_WhiteDistancePalette & ~7);
    ringCount = 0;
    if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
      shooter = &g_Players[g_RoundHud.unk87];
      for (i = 0; i != RING_COUNT; i++) {
        ringCount += shooter->ringStates[i] == RING_STATE_SHOWN;
      }
      sprintf(g_PrintBuffer, "%d", ringCount);
      DrawDistanceText(&g_DisplayListHead, RING_COUNT_X - f_ShotPanelOffsetX,
          RING_COUNT_Y, g_PrintBuffer);
    }
    if (g_IsDebugEnabled &&
        (g_ControllerStates[HUD_OFFSET_CONTROLLER].pressedButtons &
            D_CBUTTONS)) {
      shooter = &g_Players[g_RoundHud.unk87];
      for (i = 0; i != RING_COUNT; i++) {
        shooter->ringStates[i] = RING_STATE_NONE;
      }
    }
    if (club->surfaceMask & CLUB_SURFACE_GREEN) {
      hundredths = g_RoundHud.cupDistance *
          (FEET_PER_YARD * HUNDREDTHS_PER_UNIT) * YARDS_PER_UNIT;
      sprintf(g_PrintBuffer, "%3d.%02df", hundredths / HUNDREDTHS_PER_UNIT,
          hundredths % HUNDREDTHS_PER_UNIT);
      DrawDistanceText(&g_DisplayListHead,
          SHOT_PANEL_DISTANCE_X - f_ShotPanelOffsetX, REST_LABEL_Y,
          g_PrintBuffer);
      if (g_RoundHud.ballHeightMetersDirection != HEIGHT_LEVEL) {
        sprintf(g_PrintBuffer, "%6.2ff", fabsf(g_RoundHud.ballHeightFeet));
        DrawDistanceText(&g_DisplayListHead,
            SHOT_PANEL_DISTANCE_X - f_ShotPanelOffsetX, HEIGHT_LABEL_Y,
            g_PrintBuffer);
      }
    } else {
      sprintf(g_PrintBuffer, "%3dy",
          (s32)(g_RoundHud.cupDistance * YARDS_PER_UNIT));
      DrawDistanceText(&g_DisplayListHead,
          SHOT_PANEL_DISTANCE_X - f_ShotPanelOffsetX, REST_LABEL_Y,
          g_PrintBuffer);
      if (g_RoundHud.ballHeightYardsDirection != HEIGHT_LEVEL) {
        height = g_RoundHud.ballHeightYards;
        if (height < 0) {
          height = -height;
        }
        sprintf(g_PrintBuffer, "%3dy", height);
        DrawDistanceText(&g_DisplayListHead,
            SHOT_PANEL_DISTANCE_X - f_ShotPanelOffsetX, HEIGHT_LABEL_Y,
            g_PrintBuffer);
      }
    }
  }
  if (g_RoundHud.isClubHeadIconShown && !g_IsCupViewShown) {
    gDPPipeSync(g_DisplayListHead++);
    gDPSetRenderMode(g_DisplayListHead++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(g_DisplayListHead++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTextureLUT(g_DisplayListHead++, G_TT_RGBA16);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTexturePersp(g_DisplayListHead++, G_TP_NONE);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTextureFilter(g_DisplayListHead++, G_TF_POINT);
    if (g_IsModuleAlive[MODULE_UNK25]) {
      UpdateClubPanelImages();
    }
    gDPLoadTLUT_pal256(
        g_DisplayListHead++, ((u32)g_ClubPanelImage + IMAGE_HEADER_SIZE) & ~7);
    gDPLoadTextureBlock(g_DisplayListHead++,
        ((u32)g_ClubPanelImage + LARGE_PALETTED_TEXELS_OFFSET) & ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, CLUB_PANEL_WIDTH, CLUB_PANEL_HEIGHT, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(g_DisplayListHead++);
    gSPScisTextureRectangle(g_DisplayListHead++,
        (CLUB_PANEL_X - f_ShotPanelOffsetX) << 2,
        (f_HudOffsetY + CLUB_PANEL_Y) << 2,
        (CLUB_PANEL_X + CLUB_PANEL_WIDTH - f_ShotPanelOffsetX) << 2,
        (f_HudOffsetY + CLUB_PANEL_Y + CLUB_PANEL_HEIGHT) << 2, G_TX_RENDERTILE,
        0, 0, 1 << 10, 1 << 10);
    gDPPipeSync(g_DisplayListHead++);
    gDPLoadTLUT_pal16(g_DisplayListHead++, 0,
        ((u32)f_ClubNameImages[f_ClubNameImageIndex] + IMAGE_HEADER_SIZE) & ~7);
    gDPLoadTextureBlock_4b(g_DisplayListHead++,
        ((u32)f_ClubNameImages[f_ClubNameImageIndex] + PALETTED_TEXELS_OFFSET) &
            ~7,
        G_IM_FMT_CI, CLUB_NAME_WIDTH, CLUB_NAME_HEIGHT, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(g_DisplayListHead++);
    gSPScisTextureRectangle(g_DisplayListHead++,
        (CLUB_NAME_X - f_ShotPanelOffsetX) << 2,
        (f_HudOffsetY + CLUB_NAME_Y) << 2,
        (CLUB_NAME_X + CLUB_NAME_WIDTH - f_ShotPanelOffsetX) << 2,
        (f_HudOffsetY + CLUB_NAME_Y + CLUB_NAME_HEIGHT) << 2, G_TX_RENDERTILE,
        0, 0, 1 << 10, 1 << 10);
    gDPPipeSync(g_DisplayListHead++);
  }
  if (g_RoundHud.unk8 && !g_IsCupViewShown && !IsOnDrivingRange()) {
    gDPLoadTLUT_pal16(
        g_DisplayListHead++, 0, ((u32)f_ParImage + IMAGE_HEADER_SIZE) & ~7);
    gDPLoadTextureBlock_4b(g_DisplayListHead++,
        ((u32)f_ParImage + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
        SHOT_PANEL_LABEL_WIDTH, SHOT_PANEL_LABEL_HEIGHT, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(g_DisplayListHead++);
    gSPScisTextureRectangle(g_DisplayListHead++,
        (PAR_IMAGE_X - f_ShotPanelOffsetX) << 2, PAR_IMAGE_Y << 2,
        (PAR_IMAGE_X + SHOT_PANEL_LABEL_WIDTH - f_ShotPanelOffsetX) << 2,
        (PAR_IMAGE_Y + SHOT_PANEL_LABEL_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
        1 << 10, 1 << 10);
    gDPPipeSync(g_DisplayListHead++);
  }
  if ((D_800C308C != 0 & D_800C308C != LETTERBOX_STATE_UNK5) &&
      g_RoundHud.isLetterboxShown) {
    DrawAnimatedIcon(&g_DisplayListHead, f_LetterboxHeight);
  }
  BeginShadowedText(&g_DisplayListHead);
  SetTextColor(&g_DisplayListHead, 255, 255, 255);
  SetTextColor(&g_DisplayListHead, 255, 255, 255);
  if (g_RoundHud.isLieIconShown && !g_IsCupViewShown) {
    surface = GetSurfaceInfo(g_CurrentShot.surface);
    GetSurfacePercentRange(surface, GetSelectedClub(), percentRange);
    sprintf(g_PrintBuffer, "%3d-%3d", percentRange[0], percentRange[1]);
    DrawShadowedText(&g_DisplayListHead, f_ShotPanelOffsetX + LIE_PERCENT_X,
        f_HudOffsetY + SHOT_PANEL_TEXT_Y, g_PrintBuffer);
  }
  if (g_RoundHud.isClubHeadIconShown && !g_IsCupViewShown) {
    selectedClub = GetSelectedClub();
    player = &g_Players[g_RoundHud.unk87];
    if (selectedClub->surfaceMask & CLUB_SURFACE_GREEN) {
      sprintf(g_PrintBuffer, "%3d%c", GetSelectedShotDistance(),
          SHADOWED_FEET_GLYPH);
    } else {
      sprintf(g_PrintBuffer, "%3dy", GetSelectedShotDistance());
    }
    DrawShadowedText(&g_DisplayListHead, CLUB_DISTANCE_X - f_ShotPanelOffsetX,
        f_HudOffsetY + SHOT_PANEL_TEXT_Y, g_PrintBuffer);
    if (g_CurrentShot.shotMode == SHOT_MODE_POWER &&
        !(selectedClub->surfaceMask & CLUB_SURFACE_GREEN) &&
        g_IsModuleAlive[MODULE_UNK25] && IsShotModeChangeDone() &&
        player->unk28 != -1) {
      sprintf(g_PrintBuffer, "%d", player->unk28);
      DrawShadowedText(&g_DisplayListHead, UNK28_TEXT_X - f_ShotPanelOffsetX,
          f_HudOffsetY + UNK28_TEXT_Y, g_PrintBuffer);
    }
  }
  if (g_RoundHud.unk13 && !g_IsCupViewShown) {
    player = &g_Players[g_RoundHud.unk87];
    shotLength =
        IntLength2D(g_CurrentShot.ballPosition[AXIS_X] - g_BallPosition[AXIS_X],
            g_CurrentShot.ballPosition[AXIS_Z] - g_BallPosition[AXIS_Z]);
    SetTextColor(&g_DisplayListHead, 255, 255, 63);
    if (GetClub(player->clubs[g_CurrentShot.clubSlot])->surfaceMask &
        CLUB_SURFACE_GREEN) {
      sprintf(g_PrintBuffer, "%5.2f%c",
          shotLength * (FEET_PER_YARD * YARDS_PER_UNIT), SHADOWED_FEET_GLYPH);
    } else {
      sprintf(g_PrintBuffer, "%5.2fy", shotLength * YARDS_PER_UNIT);
    }
    DrawShadowedText(
        &g_DisplayListHead, SHOT_LENGTH_X, SHOT_LENGTH_Y, g_PrintBuffer);
  }
  if (g_RoundHud.unkD && !g_IsCupViewShown && !IsOnDrivingRange()) {
    for (i = 0; i < g_PlayerCount; i++) {
      SetTextColor(&g_DisplayListHead, 255, 255, 255);
      sprintf(g_PrintBuffer, "%d", g_Players[i].strokeCount);
      DrawShadowedText(&g_DisplayListHead, scoreboardX + SCOREBOARD_TEXT_X,
          SCOREBOARD_Y + i * SCOREBOARD_ROW_HEIGHT, g_PrintBuffer);
      if (hasHandicap) {
        DrawShadowedText(&g_DisplayListHead, scoreboardX + HANDICAP_COLON_X,
            SCOREBOARD_Y + i * SCOREBOARD_ROW_HEIGHT, ":");
        sprintf(g_PrintBuffer, "%2d",
            GetHandicapOffset(
                GetCourseRecordIndex(), g_HoleIndex, g_Players[i].handicap));
        DrawShadowedText(&g_DisplayListHead, scoreboardX + HANDICAP_X,
            SCOREBOARD_Y + i * SCOREBOARD_ROW_HEIGHT, g_PrintBuffer);
      }
      SetTextColor(&g_DisplayListHead, 255, 230, 0);
      if (D_801B60B9 != 0) {
        sprintf(g_PrintBuffer, f_BlankText);
      } else {
        switch (g_GameMode) {
          case GAME_MODE_STROKE:
          case GAME_MODE_UNK1:
          case GAME_MODE_TOURNAMENT:
          case GAME_MODE_SPEED_GOLF:
            if ((s16)g_RoundHud.skinsTotals[i] == 0) {
              sprintf(g_PrintBuffer, "#0");
            } else {
              sprintf(g_PrintBuffer, "%+d", (s16)g_RoundHud.skinsTotals[i]);
            }
            break;
          case GAME_MODE_MATCH_GAME:
          case GAME_MODE_GET_CHARACTER:
            if ((s16)g_RoundHud.skinsTotals[i] == 0) {
              sprintf(g_PrintBuffer, "EVEN");
            } else if ((s16)g_RoundHud.skinsTotals[i] < 0) {
              sprintf(g_PrintBuffer, "%dUP", -(s16)g_RoundHud.skinsTotals[i]);
            } else {
              sprintf(g_PrintBuffer, "%dDW", (s16)g_RoundHud.skinsTotals[i]);
            }
            break;
          case GAME_MODE_SKINS_MATCH:
          case GAME_MODE_CLUB_SLOTS:
            sprintf(g_PrintBuffer, "%dP", (s16)g_RoundHud.skinsTotals[i]);
            break;
          case GAME_MODE_TRAINING:
          case GAME_MODE_UNK8:
          case GAME_MODE_RING_SHOT:
            sprintf(g_PrintBuffer, f_BlankText);
            break;
        }
      }
      DrawShadowedText(&g_DisplayListHead, scoreboardX + SCOREBOARD_TEXT_X,
          SCOREBOARD_SCORE_Y + i * SCOREBOARD_ROW_HEIGHT, g_PrintBuffer);
      SetTextColor(&g_DisplayListHead, 255, 255, 255);
      if (g_GameState.betRates[BET_LONGEST_DRIVE] != UNSET_BET_RATE &&
          IsMarkedParFive(GetCourseRecordIndex(), g_HoleIndex)) {
        contestDistance = g_Players[i].driveDistance;
        if (contestDistance != UNMEASURED_DISTANCE) {
          if (contestDistance == UNCOUNTED_DISTANCE) {
            sprintf(g_PrintBuffer, "miss");
          } else {
            sprintf(
                g_PrintBuffer, "%3dy", (s32)(contestDistance * YARDS_PER_UNIT));
          }
          DrawShadowedText(&g_DisplayListHead, scoreboardX + DRIVE_DISTANCE_X,
              SCOREBOARD_CONTEST_Y + i * SCOREBOARD_ROW_HEIGHT, g_PrintBuffer);
        }
      }
      if (g_GameState.betRates[BET_NEAREST_PIN] != UNSET_BET_RATE &&
          IsParThree(GetCourseRecordIndex(), g_HoleIndex)) {
        contestDistance = g_Players[i].pinDistance;
        if (contestDistance != UNMEASURED_DISTANCE) {
          if (contestDistance == UNCOUNTED_DISTANCE) {
            sprintf(g_PrintBuffer, "miss");
          } else {
            hundredths = contestDistance *
                (FEET_PER_YARD * HUNDREDTHS_PER_UNIT) * YARDS_PER_UNIT;
            sprintf(g_PrintBuffer, "%2d.%02d%c",
                hundredths / HUNDREDTHS_PER_UNIT,
                hundredths % HUNDREDTHS_PER_UNIT, SHADOWED_FEET_GLYPH);
          }
          DrawShadowedText(&g_DisplayListHead, scoreboardX + PIN_DISTANCE_X,
              SCOREBOARD_CONTEST_Y + i * SCOREBOARD_ROW_HEIGHT, g_PrintBuffer);
        }
      }
    }
  }
  if (g_RoundHud.unkD && !IsOnDrivingRange() &&
      g_GameMode == GAME_MODE_SPEED_GOLF && D_801B60B9 == 0) {
    hundredths = (s32)(g_GameState.stopwatchFrameCount *
                     ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
        HUNDREDTHS_PER_SECOND;
    totalSeconds = g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND;
    sprintf(g_PrintBuffer, D_800D1C84, totalSeconds / SECONDS_PER_MINUTE,
        totalSeconds % SECONDS_PER_MINUTE, hundredths);
    DrawShadowedText(&g_DisplayListHead, PANEL_STOPWATCH_X - f_ShotPanelOffsetX,
        PANEL_STOPWATCH_Y, g_PrintBuffer);
  }
  DrawNothing(&g_DisplayListHead);
  SetUpTextDrawing(&g_DisplayListHead);
  DrawBirdieBadgeCount(&g_DisplayListHead);
  if (IsGameFlagSet(GAME_FLAG_SHOW_MIRROR_TEXTURE)) {
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTextureLUT(g_DisplayListHead++, G_TT_NONE);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTextureFilter(g_DisplayListHead++, G_TF_POINT);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetRenderMode(g_DisplayListHead++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineMode(g_DisplayListHead++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPLoadTextureBlock(g_DisplayListHead++, (u32)g_MirrorTexture & ~7,
        G_IM_FMT_RGBA, G_IM_SIZ_16b, MIRROR_TEXTURE_SIZE, MIRROR_TEXTURE_SIZE,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_MIRROR | G_TX_WRAP, G_TX_NOMASK,
        MIRROR_TEXTURE_SIZE_LOG2, G_TX_NOLOD, G_TX_NOLOD);
    gSPScisTextureRectangle(g_DisplayListHead++, MIRROR_TEXTURE_X << 2,
        MIRROR_TEXTURE_Y << 2, (MIRROR_TEXTURE_X + MIRROR_TEXTURE_SIZE) << 2,
        (MIRROR_TEXTURE_Y + MIRROR_TEXTURE_SIZE) << 2, G_TX_RENDERTILE, 0, 0,
        1 << 10, 1 << 10);
    gDPPipeSync(g_DisplayListHead++);
  }
  if (g_IsModuleAlive[MODULE_UNK18]) {
    func_80217F54(&g_DisplayListHead);
  }
  /* A debugging aid: B changes the first player's character, and A plays the
   * contest winner's movie */
  if (IsGameFlagSet(GAME_FLAG_ADJUST_HUD_OFFSET)) {
    if (g_ControllerStates[HUD_OFFSET_CONTROLLER].pressedButtons & B_BUTTON) {
      g_Players[0].characterIndex =
          (g_Players[0].characterIndex + 1) % CHARACTER_COUNT;
    }
    if (g_ControllerStates[HUD_OFFSET_CONTROLLER].pressedButtons & A_BUTTON) {
      SetContestWinner(0);
      StartMovie(MOVIE_LONGEST_DRIVE);
    }
  }
  AdjustHudOffset();
}
#else
INCLUDE_ASM("asm/nonmatchings/round_hud", DrawRoundHud);
#endif

INCLUDE_RODATA("asm/nonmatchings/round_hud", D_800D1C84);

/* DrawRoundHud's, which the C version generates itself */
#ifndef NON_MATCHING
INCLUDE_RODATA("asm/nonmatchings/round_hud", D_800D1C98);

INCLUDE_RODATA("asm/nonmatchings/round_hud", jtbl_800D1CA0);
#endif

/* Loads the images that the hole's intro shows for its contest bet and the
 * match standing */
void LoadHoleIntroImages(void) {
  EncodedAsset asset;
  s32 standing;

  f_LoadedHoleIntroImages = 0;
  if (g_GameState.betRates[BET_LONGEST_DRIVE] != UNSET_BET_RATE &&
      IsMarkedParFive(g_CourseIndex, g_HoleIndex)) {
    f_HoleContestImage =
        AllocMemory(OpenEncodedAsset(ASSET_LONGEST_DRIVE_IMAGE, &asset));
    ReadEncodedAsset(f_HoleContestImage, &asset);
    f_LoadedHoleIntroImages |= HOLE_INTRO_CONTEST_IMAGE_LOADED;
  } else if (g_GameState.betRates[BET_NEAREST_PIN] != UNSET_BET_RATE &&
      IsParThree(g_CourseIndex, g_HoleIndex)) {
    f_HoleContestImage =
        AllocMemory(OpenEncodedAsset(ASSET_NEAREST_PIN_IMAGE, &asset));
    ReadEncodedAsset(f_HoleContestImage, &asset);
    f_LoadedHoleIntroImages |= HOLE_INTRO_CONTEST_IMAGE_LOADED;
  }
  if (g_GameMode == GAME_MODE_MATCH_GAME ||
      g_GameMode == GAME_MODE_GET_CHARACTER) {
    if (D_801B60A0 != 0) {
      f_MatchStandingImage =
          AllocMemory(OpenEncodedAsset(ASSET_UNK8FF_IMAGE, &asset));
      ReadEncodedAsset(f_MatchStandingImage, &asset);
      f_LoadedHoleIntroImages |= HOLE_INTRO_MATCH_IMAGE_LOADED;
    } else {
      standing = GetMatchStanding(0, 1);
      if (standing == MATCH_STANDING_PLAYER_DORMIE ||
          standing == MATCH_STANDING_OPPONENT_DORMIE) {
        f_MatchStandingImage =
            AllocMemory(OpenEncodedAsset(ASSET_DORMIE_IMAGE, &asset));
        ReadEncodedAsset(f_MatchStandingImage, &asset);
        f_LoadedHoleIntroImages |= HOLE_INTRO_MATCH_IMAGE_LOADED;
      }
    }
  }
}

/* Frees the images that LoadHoleIntroImages loaded */
void FreeHoleIntroImages(void) {
  if (f_LoadedHoleIntroImages & HOLE_INTRO_CONTEST_IMAGE_LOADED) {
    FreeMemory(&f_HoleContestImage);
  }
  if (f_LoadedHoleIntroImages & HOLE_INTRO_MATCH_IMAGE_LOADED) {
    FreeMemory(&f_MatchStandingImage);
  }
  f_LoadedHoleIntroImages = 0;
}

/*
 * Draws the images that LoadHoleIntroImages loaded, a strip at a time, while
 * the hole's intro text is shown. Ring shot draws its own intro instead.
 *
 * Doesn't match: the match standing image's loop keeps different constants
 * in registers.
 */
#ifdef NON_MATCHING
void DrawHoleIntroImages(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 strip;

  if (g_RoundHud.isHoleIntroTextShown == 1) {
    if (g_GameMode == GAME_MODE_RING_SHOT) {
      if (g_IsModuleAlive[MODULE_UNK21]) {
        func_802427E8(&gfx);
      }
    } else {
      gDPPipeSync(gfx++);
      gDPSetTextureLUT(gfx++, G_TT_RGBA16);
      gDPPipeSync(gfx++);
      gDPSetTextureFilter(gfx++, G_TF_BILERP);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(
          gfx++, HOLE_INTRO_IMAGE_RENDER_MODE, HOLE_INTRO_IMAGE_RENDER_MODE2);
      gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
      if ((g_GameState.betRates[BET_LONGEST_DRIVE] > 0 &&
              IsMarkedParFive(GetCourseRecordIndex(), g_HoleIndex)) ||
          (g_GameState.betRates[BET_NEAREST_PIN] > 0 &&
              IsParThree(GetCourseRecordIndex(), g_HoleIndex))) {
        gDPLoadTLUT_pal256(
            gfx++, (u32)f_HoleContestImage + IMAGE_HEADER_SIZE & ~7);
        for (strip = 0; strip != CONTEST_IMAGE_STRIP_COUNT; strip++) {
          gDPLoadTextureBlock(gfx++,
              (u32)f_HoleContestImage + LARGE_PALETTED_TEXELS_OFFSET +
                      strip *
                          (CONTEST_IMAGE_WIDTH * CONTEST_IMAGE_STRIP_HEIGHT) &
                  ~7,
              G_IM_FMT_CI, G_IM_SIZ_8b, CONTEST_IMAGE_WIDTH,
              CONTEST_IMAGE_STRIP_HEIGHT, 0, G_TX_MIRROR, G_TX_MIRROR,
              G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);
          gSPScisTextureRectangle(gfx++, CONTEST_IMAGE_X << 2,
              (CONTEST_IMAGE_Y + strip * CONTEST_IMAGE_STRIP_HEIGHT) << 2,
              (CONTEST_IMAGE_X + CONTEST_IMAGE_WIDTH) << 2,
              (CONTEST_IMAGE_Y + (strip + 1) * CONTEST_IMAGE_STRIP_HEIGHT) << 2,
              G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
          gDPPipeSync(gfx++);
        }
      }
      if ((g_GameMode == GAME_MODE_MATCH_GAME ||
              g_GameMode == GAME_MODE_GET_CHARACTER) &&
          (f_LoadedHoleIntroImages & HOLE_INTRO_MATCH_IMAGE_LOADED)) {
        gDPLoadTLUT_pal256(
            gfx++, (u32)f_MatchStandingImage + IMAGE_HEADER_SIZE & ~7);
        for (strip = 0; strip != MATCH_IMAGE_STRIP_COUNT; strip++) {
          gDPLoadTextureTile(gfx++,
              (u32)f_MatchStandingImage + LARGE_PALETTED_TEXELS_OFFSET +
                      strip * (MATCH_IMAGE_WIDTH * MATCH_IMAGE_STRIP_HEIGHT) &
                  ~7,
              G_IM_FMT_CI, G_IM_SIZ_8b, MATCH_IMAGE_WIDTH,
              MATCH_IMAGE_STRIP_HEIGHT, 0, 0, MATCH_IMAGE_WIDTH - 1,
              MATCH_IMAGE_STRIP_HEIGHT - 1, 0, G_TX_MIRROR, G_TX_MIRROR,
              G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
          gDPPipeSync(gfx++);
          gSPScisTextureRectangle(gfx++, MATCH_IMAGE_X << 2,
              (MATCH_IMAGE_Y + strip * MATCH_IMAGE_STRIP_HEIGHT) << 2,
              (MATCH_IMAGE_X + MATCH_IMAGE_WIDTH) << 2,
              (MATCH_IMAGE_Y + (strip + 1) * MATCH_IMAGE_STRIP_HEIGHT) << 2,
              G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
          gDPPipeSync(gfx++);
        }
      }
    }
  }
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/round_hud", DrawHoleIntroImages);
#endif

/*
 * Draws the course's name and the hole's number, or in the timed modes a
 * stopwatch, before the hole starts. The target reloads g_CourseIndex for
 * each color, which it matches when declared volatile.
 */
#ifdef NON_MATCHING
void DrawHoleIntroText(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 hundredths;
  s32 minutes;
  s32 seconds;
  s32 totalSeconds;

  if (g_RoundHud.isHoleIntroTextShown == 1) {
    if (g_GameMode == GAME_MODE_CLUB_SLOTS) {
      if (g_IsModuleAlive[MODULE_UNK26] && func_80242418()) {
        func_80242424(&gfx);
      }
    } else if (g_GameMode == GAME_MODE_RING_SHOT) {
    } else if (g_GameMode == GAME_MODE_TRAINING) {
      if (g_IsModuleAlive[MODULE_UNK21]) {
        func_80242D40(&gfx);
      }
    } else if (g_GameMode == GAME_MODE_SPEED_GOLF && D_801B60B9 == 0) {
      hundredths = (s32)(g_GameState.stopwatchFrameCount *
                       ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
          HUNDREDTHS_PER_SECOND;
      totalSeconds = g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND;
      seconds = totalSeconds % SECONDS_PER_MINUTE;
      minutes = totalSeconds / SECONDS_PER_MINUTE;
      SetUpGradientTextDrawing(&gfx, 0);
      sprintf(g_PrintBuffer, D_800D1C84, minutes, seconds, hundredths);
      DrawLargeTextWithShadow(&gfx, STOPWATCH_X, STOPWATCH_Y, g_PrintBuffer,
          STOPWATCH_ENV_COLOR, STOPWATCH_PRIM_COLOR, TEXT_FLAG_FIXED_WIDTH);
    } else {
      SetUpGradientTextDrawing(&gfx, 0);
      gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x00, 0x00, 0xFF);
      gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0x00, 0xFF);
      sprintf(
          g_PrintBuffer, "%s", g_CourseRecords[GetCourseRecordIndex()].name);
      sprintf(
          g_PrintBuffer, "%s", g_CourseRecords[GetCourseRecordIndex()].name);
      DrawLargeTextWithShadow(&gfx, HOLE_TITLE_X, COURSE_NAME_Y, g_PrintBuffer,
          g_CourseNameEnvColors[g_CourseIndex],
          g_CourseNamePrimColors[g_CourseIndex], TEXT_FLAG_CENTERED);
      sprintf(g_PrintBuffer, "Hole %d", g_HoleIndex + 1);
      DrawLargeTextWithShadow(&gfx, HOLE_TITLE_X, HOLE_NUMBER_Y, g_PrintBuffer,
          g_CourseNameEnvColors[g_CourseIndex],
          g_CourseNamePrimColors[g_CourseIndex], TEXT_FLAG_CENTERED);
      gDPPipeSync(gfx++);
      gDPPipeSync(gfx++);
      gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    }
  }
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/round_hud", DrawHoleIntroText);
#endif

/* Switches to the other matrix buffer, which the RSP is not reading */
void SwapMatrixBuffers(void) {
  g_MatrixBufferIndex ^= 1;
}
