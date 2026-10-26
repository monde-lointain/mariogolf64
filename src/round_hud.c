#include "common.h"
#include "controller.h"
#include "debug_text.h"
#include "game_flags.h"
#include "asset.h"
#include "billboard.h"
#include "burst.h"
#include "camera.h"
#include "character.h"
#include "course.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "effects.h"
#include "functions.h"
#include "heap.h"
#include "hole_display.h"
#include "libc.h"
#include "math_util.h"
#include "movie.h"
#include "overlay_manager.h"
#include "pin.h"
#include "round_stats.h"
#include "save.h"
#include "scoring.h"
#include "round_hud.h"
#include "screen.h"
#include "scene.h"
#include "screen_layers.h"
#include "sound.h"
#include "surface.h"
#include "text.h"
#include "variables.h"

/* sSceneryState values */
#define SCENERY_UNLOADED 0
#define SCENERY_BILLBOARDS_READY 1 /* The billboard heap is set up */
#define SCENERY_LOADED 2           /* The images of the hole's scenery are loaded too */

/* Bytes of the billboard manager's heap */
#define BILLBOARD_HEAP_SIZE 0x7118

/* Bytes of each buffer the scenery images come with */
#define SCENERY_BUFFER_A_SIZE 0x128
/* Bytes of the buffer that StartShotPanel loads the lie's image into */
#define LIE_IMAGE_SIZE 0x1208

/* Images of the round's scenery, effects and HUD, named after their asset ids until their users are decompiled */
#define ASSET_ROUND_IMAGE_541 0x541
#define ASSET_WIND_ICON_IMAGE 0x5A4
#define ASSET_ROUND_IMAGE_5E3 0x5E3
#define ASSET_ROUND_IMAGE_972 0x972
#define ASSET_BACKDROP_PATTERN_IMAGE 0x93C
#define ASSET_ROUND_IMAGE_4DB 0x4DB
#define ASSET_SHADOW_TEXTURE 0x4C3
#define ASSET_ROUND_IMAGE_722 0x722
#define ASSET_BLENDED_TEXTURE_SCROLLING_LAYER 0x4D7
#define ASSET_BLENDED_TEXTURE_STILL_LAYER 0x4D9
#define ASSET_ROUND_IMAGE_4E3 0x4E3
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
#define ASSET_ROUND_IMAGE_724 0x724

/* The sound of the rain, played at full volume when a hole starts in the rain */
#define RAIN_SOUND 0
#define RAIN_SOUND_START_VOLUME 100

/* The image of the hole's par, one per par from 3 to 5 */
#define ASSET_PAR_THREE_IMAGE 0x565
#define ASSET_PAR_FOUR_IMAGE 0x566
#define ASSET_PAR_FIVE_IMAGE 0x567

/* The first asset of each course record's hole images, one per hole */
extern u16 sHoleImageAssets[COURSE_RECORD_COUNT];
extern void *sHoleImage;
#define HOLE_IMAGE_WIDTH 48
#define HOLE_IMAGE_HEIGHT 42
extern void *sParImage;

/* Set while the overlay of the course selection is loaded for the hole */
extern s32 sIsCourseSelectLoaded;
extern f32 g_RainSoundVolume;

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
#define HOLE_FLAG_VERTEX(row, column) \
    sHoleFlagVertices[0][g_MatrixBufferIndex][(row) * HOLE_FLAG_COLUMN_COUNT + (column)]
#define HOLE_FLAG_SHADE(row, column) sHoleFlagShades[(row) * HOLE_FLAG_COLUMN_COUNT + (column)]

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
extern f32 sHoleFlagWavePhase;

/* The near and far planes of the screen's orthographic projection */
#define SCREEN_ORTHO_NEAR 1.0f
#define SCREEN_ORTHO_FAR 1000.0f

extern Vtx sHoleFlagVertices[HOLE_FLAG_SIDE_COUNT][MATRIX_BUFFER_COUNT][HOLE_FLAG_VERTEX_COUNT];
/* s8 because WaveHoleFlag loads them as signed bytes when it spreads them */
extern s8 sHoleFlagShades[HOLE_FLAG_VERTEX_COUNT];
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
#define TEXEL_COORDINATE_SHIFT 5 /* Texture coordinates are in s10.5 fixed point */

typedef struct ArrowShape {
    /* 0x000 */ Vtx body[ARROW_VERTEX_COUNT];
    /* 0x0E0 */ Vtx outline[ARROW_VERTEX_COUNT];
} ArrowShape; /* size = 0x1C0 */

extern ArrowShape sArrows[ARROW_COUNT];

/*
 * The marker of the current shot's offset, with the positions it had in the
 * previous frames for its trail. The offset is clamped to a circle, in shot
 * units; the marker moves a pixel every SHOT_OFFSET_UNITS_PER_PIXEL units.
 */
#define SHOT_OFFSET_MARKER_COUNT 5
#define SHOT_OFFSET_MAX_RADIUS 56.0f
#define SHOT_OFFSET_UNITS_PER_PIXEL 4

extern s32 sShotOffsetMarkerXs[SHOT_OFFSET_MARKER_COUNT + 1];
extern s32 sShotOffsetMarkerYs[SHOT_OFFSET_MARKER_COUNT + 1];

/*
 * The marker is an animated 16x16 image, drawn at an offset from the shot
 * panel. Its asset holds the frames of the animation, then a still frame.
 */
#define SHOT_OFFSET_MARKER_SIZE 16
#define SHOT_OFFSET_MARKER_FRAME_COUNT 11
#define SHOT_OFFSET_MARKER_FRAME_SIZE (SHOT_OFFSET_MARKER_SIZE * SHOT_OFFSET_MARKER_SIZE / 2)
#define SHOT_OFFSET_MARKER_STILL_FRAME SHOT_OFFSET_MARKER_FRAME_COUNT
#define SHOT_OFFSET_MARKER_X 264
#define SHOT_OFFSET_MARKER_Y 174

extern s32 sShotPanelOffsetY;
extern s32 sShotPanelOffsetX;
extern s32 sShotOffsetMarkerFrame;
extern s32 sShotOffsetMarkerTexelOffset;
extern void *sShotOffsetMarkerImage;

/* The images that a hole's intro shows for the bets and the match standing */
#define ASSET_LONGEST_DRIVE_IMAGE 0x554
#define ASSET_NEAREST_PIN_IMAGE 0x555
#define ASSET_DORMIE_IMAGE 0x8FC
#define ASSET_UNK8FF_IMAGE 0x8FF

/* Which of the hole intro's images are loaded */
#define HOLE_INTRO_CONTEST_IMAGE_LOADED (1 << 0)
#define HOLE_INTRO_MATCH_IMAGE_LOADED (1 << 1)
/* The intro images are drawn opaque, keeping the coverage of their alpha */
#define HOLE_INTRO_IMAGE_RENDER_MODE \
    AA_EN | CVG_X_ALPHA | ALPHA_CVG_SEL | FORCE_BL | GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1)
#define HOLE_INTRO_IMAGE_RENDER_MODE2 \
    AA_EN | CVG_X_ALPHA | ALPHA_CVG_SEL | FORCE_BL | GBL_c2(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1)
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

extern s32 sLoadedHoleIntroImages;
extern void *sHoleContestImage;
extern void *sMatchStandingImage;
extern s32 D_801B60A0;

/* The ball's recent world positions, which its trail is drawn through */
#define BALL_TRAIL_LENGTH 128
extern f32 sBallTrail[BALL_TRAIL_LENGTH][3];
extern s32 sBallTrailHead; /* The latest position */

/*
 * The scrolling of the wind arrow's texture, a step for every
 * WIND_SPEED_PER_ARROW_SCROLL_STEP of wind; its texture repeats every
 * ARROW_TEXTURE_PERIOD of texture coordinate.
 */
#define WIND_SPEED_PER_ARROW_SCROLL_STEP 0x2000
#define ARROW_TEXTURE_PERIOD 0x2000
#define ARROW_TEXTURE_COORDINATE_LIMIT 0x4000

/* A debugging option that lets the second controller nudge the HUD vertically */
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
#define COLOR_TEST_COLUMN_WIDTH 5 /* Columns from one channel's value to the next */
extern s32 sTestColors[COLOR_TEST_COLOR_COUNT][COLOR_TEST_CHANNEL_COUNT];
extern s32 sTestTextX;
extern s32 sTestTextY;
extern s32 sTestChannelIndex;
extern s32 sTestColorIndex;
extern s32 sIsTestGradientShown;
/* "Sample" in half-width katakana, then "ABCDEFG" */
extern char sColorTestSampleText[];
/* Printed over a cursor to erase it */
extern char sBlankText[];
#define HUD_OFFSET_TEXT_COLUMN 20
#define HUD_OFFSET_TEXT_ROW 6
extern s32 sHudOffsetY;

/* How long the shot camera waits after the ball lands, in frames */
#define LANDING_WAIT_FRAME_COUNT 40
#define LANDING_WAIT_LONG_FRAME_COUNT 60

/* The inset view that shows where the ball landed, and the one Toad's movie plays in */
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

extern s32 sLandingReaction;
extern s32 sLandingReactionFrame;
/* The reaction stops counting its frames here */
#define LANDING_REACTION_FRAME_COUNT 30
#define LANDING_NAME_GLYPH_Y 8
#define LANDING_DISTANCE_GLYPH_Y (-8)
#define LANDING_NAME_Y 96
#define LANDING_DISTANCE_Y 112
#define HUNDREDTHS_PER_UNIT 100
/* The characters that the fonts draw as the symbols for yards and feet */
#define YARDS_GLYPH 0xF9
#define FEET_GLYPH 0xFB
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
extern Vtx sLandingPanelVertices[4];
extern Mtx sLandingPanelMatrices[MATRIX_BUFFER_COUNT];

#define RING_TEXTURE_OUTER_RADIUS 62

/* The HUD keeps this far in from the edges of the screen */
#define HUD_MARGIN 8

extern void *D_800E2160;
extern s32 D_800E21BC;

/* The black bars that grow over the top and bottom of the HUD's area */
#define LETTERBOX_GROWTH_STEP 2
#define LETTERBOX_MAX_HEIGHT 16
#define LETTERBOX_STATE_UNK5 5

extern s32 D_800C308C;

/* Where the hole image sits in front of the perspective camera, at a tenth of its size */
#define HOLE_IMAGE_MODEL_X (-240.0f)
#define HOLE_IMAGE_MODEL_Y 180.0f
#define HOLE_IMAGE_MODEL_Z (-500.0f)
#define HOLE_IMAGE_MODEL_SCALE 0.1f

extern Mtx sHoleImageMatrix;


/*
 * StartShotEffect's long shot effect, which func_80086864 draws, plays a
 * sound, and a different, louder one for some kinds of golfer.
 */
#define SHOT_EFFECT_TIME_OFF (-1.0f)
#define GOLFER_KIND_UNK2 2
#define GOLFER_KIND_UNK10 10
#define SOUND_LONG_SHOT 0x70
#define SOUND_LONG_SHOT_HEAVY 0x71
#define LONG_SHOT_SOUND_CHANNEL 14
#define LONG_SHOT_SOUND_VOLUME 0x40
#define LONG_SHOT_HEAVY_SOUND_VOLUME 0x60
#define LONG_SHOT_SOUND_REVERB 30
#define LONG_SHOT_SOUND_PAN 127

extern f32 sShotEffectTime;
extern s32 sShotEffect;

s32 func_80213C78(UnkB8 *player);

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

extern void *sSceneryBillboardHeap;
extern void *D_801B55DC;
extern void *sWindIconImage;
extern void *D_80132360;
extern void *D_800FED0C;
extern void *D_8012D428;
extern void *g_ShadowTexture;
extern s32 D_800C59D8;
extern void *D_800E2140;
extern void *D_800E2144[2];
/* The image asset of the lie that the HUD shows, loaded into sLieImage */
extern s32 sLieImageAsset;
extern void *sLieImage;
#define ANIMATED_ICON_LABEL_COUNT 3
/* The images of the animated icon and its labels; see DrawAnimatedIcon */
extern void *sAnimatedIconImages[];
extern void *sAnimatedIconLabelImages[ANIMATED_ICON_LABEL_COUNT];

extern void *sWindSpeedFont;
extern void *D_800E2168;
extern void *D_800E216C;
extern void *D_800E2170;
extern void *D_800E2174;
extern void *D_800E217C;
extern void *D_800E2178;

/*
 * The wind indicator is a model in module 9, lit by sWindArrowLights. It
 * sits in front of the camera, tilted to its pitch and turned to the wind.
 */
#define MODULE_UNK9 9
#define WIND_INDICATOR_DEPTH -300.0f
#define WIND_INDICATOR_TILT 0.3f
#define WIND_INDICATOR_SCALE -0.07f
#define WIND_INDICATOR_FACING_DEGREES 180.0
extern Lights1 sWindArrowLights;

/*
 * The wind speed display's icon pulses around these half sizes, and is wider
 * for two-digit speeds. Its texture is WIND_ICON_TEXTURE_WIDTH by HEIGHT.
 */
extern f32 sWindIconPulseAngle;
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
#define WIND_HIGHLIGHT_OFFSET_X 52.0f
#define WIND_HIGHLIGHT_OFFSET_Y 56.0f
#define WIND_HIGHLIGHT_WIDE_SIZE 56
#define WIND_HIGHLIGHT_NARROW_SIZE 44
/* The wind speed font has ten digits and then a wider 'm' */
#define WIND_FONT_WIDTH 144
#define WIND_FONT_HEIGHT 14
#define WIND_DIGIT_WIDTH 12
#define WIND_METERS_GLYPH_S 119
#define WIND_METERS_GLYPH_WIDTH 14
#define WIND_TEXT_RAISE 1
#define WIND_TEXT_RENDER_MODE IM_RD | CLR_ON_CVG | CVG_X_ALPHA | GBL_c1(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)
#define WIND_TEXT_RENDER_MODE2 IM_RD | CLR_ON_CVG | CVG_X_ALPHA | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)
#define WIND_TEXT_PRIM_RED 0x3F
#define WIND_TEXT_PRIM_GREEN 0x3F
#define WIND_TEXT_PRIM_BLUE 0xFF
#define WIND_TEXT_ENV_RED 0x00
#define WIND_TEXT_ENV_GREEN 0x20
#define WIND_TEXT_ENV_BLUE 0x7F
void func_802199EC(Gfx **displayListHead, s32 x, s32 y, s32 size);
extern Mtx sWindIndicatorMatrices[2];
extern Gfx D_80222400[];
void DrawWindSpeed(Gfx **displayListHead, f32 x, f32 y);
extern void *D_80105214;
extern s32 sSceneryState;

#define LEVEL_HEIGHT 0.01f /* Heights in meters or feet nearer zero than this */
/* On the tee ground, the shot panel sits this much higher */
#define TEE_GROUND_PANEL_OFFSET_Y -3
#define SHOT_PANEL_START_OFFSET_X 64
void func_8021E710(void);
void func_8021419C(UnkB8 *player);
extern s8 D_801B60B9;
/* The colors of each course's name, which gradients from the first to the second */
extern s32 g_CourseNameEnvColors[];
extern s32 g_CourseNamePrimColors[];
/* "%02d:%02d:%02d", which func_80089094 also uses */
extern char D_800D1C84[];

s32 func_80242418(void);
void func_80242424(Gfx **displayListHead);
void func_80242D40(Gfx **displayListHead);
void func_802427E8(Gfx **displayListHead);

/* The game counts time in frames */
#define FRAMES_PER_SECOND 30
#define SECONDS_PER_MINUTE 60
#define HUNDREDTHS_PER_SECOND 100
#define MODULE_UNK21 21
#define MODULE_UNK26 26
#define STOPWATCH_X 168
#define STOPWATCH_Y 176
#define STOPWATCH_ENV_COLOR 0xFFFF0000
#define STOPWATCH_PRIM_COLOR 0x30FF0000
#define HOLE_TITLE_X (SCREEN_WIDTH / 2)
#define COURSE_NAME_Y 32
#define HOLE_NUMBER_Y 48

void func_80216B74(s32);
void LoadCourseSelect(s32);
void FreeCourseSelect(void);
void func_801FD624(void);
void func_8020D6F0(void);
void func_80216EDC(void);
void func_8020E740(void);
void func_8021E5F4(void);
void func_8020D784(void);
void func_80216F8C(void);
void func_8020E78C(void);
void func_8021E6B8(void);

/* Sets up what the hole's drawing needs before its first frame */
void PrepareHoleDisplay(void) {
    s32 i;

    g_MatrixBufferIndex = 0;
    for (i = 0; i != ARROW_VERTEX_COUNT; i++) {
        sArrows[0].body[i].v.tc[1] =
            (sArrows[0].body[i].v.ob[1] / ARROW_UNITS_PER_TEXEL_ROW + ARROW_CENTER_TEXEL_ROW) << TEXEL_COORDINATE_SHIFT;
        sArrows[1].body[i].v.tc[1] =
            (sArrows[1].body[i].v.ob[1] / ARROW_UNITS_PER_TEXEL_ROW + ARROW_CENTER_TEXEL_ROW) << TEXEL_COORDINATE_SHIFT;
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
    Vtx *vertex;

    D_800C5478 = 0;
    for (grid = 0; grid != HOLE_FLAG_SIDE_COUNT * MATRIX_BUFFER_COUNT; grid++) {
        vertex = sHoleFlagVertices[0][grid];
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
        vertex = sHoleFlagVertices[1][grid];
        for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
            for (column = 0; column != HOLE_FLAG_COLUMN_COUNT; column++) {
                vertex->v.tc[0] = HOLE_FLAG_BACK_TEXEL_START - column * HOLE_FLAG_TEXEL_COLUMN_STEP;
                vertex++;
            }
        }
    }
    for (column = 0; column != HOLE_FLAG_VERTEX_COUNT; column++) {
        sHoleFlagShades[column] = HOLE_FLAG_START_SHADE;
    }
    guOrtho(&g_ScreenOrthoMatrix, -SCREEN_WIDTH / 2, SCREEN_WIDTH / 2, -SCREEN_HEIGHT / 2, SCREEN_HEIGHT / 2,
            SCREEN_ORTHO_NEAR, SCREEN_ORTHO_FAR, 1.0f);
    guPerspective(&D_80105278, &g_ScreenEffectPerspNorm, 10.0f, 4.0f / 3.0f, 10.0f, 30000.0f, 1.0f);
    guPerspective(&g_ScreenEffectProjection, &g_ScreenEffectPerspNorm, 45.0f, 4.0f / 3.0f, 10.0f, 8000.0f, 1.0f);
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

    OpenEncodedAsset(g_CharacterImageAssets[character] + CHARACTER_FACE_IMAGE_OFFSET, &asset);
    ReadEncodedAsset(g_PlayerFaceImages[playerIndex], &asset);
    OpenAssetFile(g_CharacterPaletteAssets[character] + paletteIndex, &file);
    ReadAssetFile(g_PlayerFacePalettes[playerIndex], GetAssetFileSize(&file), &file);
}

/* Frees the previous hole's mode images and loads the scenery images if they are not loaded */
void EndHoleIntro(void) {
    if (g_GameMode == GAME_MODE_RING_SHOT) {
        if (sIsCourseSelectLoaded == TRUE) {
            sIsCourseSelectLoaded = FALSE;
            FreeCourseSelect();
        }
    } else if (g_GameMode == GAME_MODE_UNK3) {
        if (sIsCourseSelectLoaded == TRUE) {
            sIsCourseSelectLoaded = FALSE;
            FreeCourseSelect();
        }
    } else {
        FreeHoleIntroImages();
    }
    if (sSceneryState == SCENERY_BILLBOARDS_READY) {
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
    OpenEncodedAsset(sHoleImageAssets[GetCourseRecordIndex()] + GetHoleIndexInRecord(), &asset);
    ReadEncodedAsset(sHoleImage, &asset);
    switch ((s8)g_CourseRecords[GetCourseRecordIndex()].holes[GetHoleRecordIndex()].par) {
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
    ReadEncodedAsset(sParImage, &asset);
    if (sSceneryState == SCENERY_LOADED) {
        FreeSceneryImages();
    }
    if (g_GameMode == GAME_MODE_RING_SHOT) {
        if (sIsCourseSelectLoaded == FALSE) {
            sIsCourseSelectLoaded = TRUE;
            LoadCourseSelect(0);
        }
    } else if (g_GameMode == GAME_MODE_UNK3) {
        if (sIsCourseSelectLoaded == FALSE) {
            sIsCourseSelectLoaded = TRUE;
            LoadCourseSelect(1);
        }
    } else {
        LoadHoleIntroImages();
    }
}

/* Loads the images the round's scenery, effects and HUD are drawn with */
void LoadSceneryImages(void) {
    EncodedAsset asset;

    sSceneryState = SCENERY_LOADED;
    func_801FD624();
    LoadEffectImages();
    func_8020D6F0();
    func_80216EDC();
    func_8020E740();
    func_8021E5F4();
    D_801B55DC = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_541, &asset));
    ReadEncodedAsset(D_801B55DC, &asset);
    sWindIconImage = AllocMemory(OpenEncodedAsset(ASSET_WIND_ICON_IMAGE, &asset));
    ReadEncodedAsset(sWindIconImage, &asset);
    D_80132360 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_5E3, &asset));
    ReadEncodedAsset(D_80132360, &asset);
    D_800FED0C = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_972, &asset));
    ReadEncodedAsset(D_800FED0C, &asset);
    g_BackdropPatternImage = AllocMemory(OpenEncodedAsset(ASSET_BACKDROP_PATTERN_IMAGE, &asset));
    ReadEncodedAsset(g_BackdropPatternImage, &asset);
    D_8012D428 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_4DB, &asset));
    ReadEncodedAsset(D_8012D428, &asset);
    g_ShadowTexture = AllocMemory(OpenEncodedAsset(ASSET_SHADOW_TEXTURE, &asset));
    ReadEncodedAsset(g_ShadowTexture, &asset);
    D_800C59D8 = -1;
    D_800E2140 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_4E3, &asset));
    ReadEncodedAsset(D_800E2140, &asset);
    D_800E2144[0] = AllocMemory(SCENERY_BUFFER_A_SIZE);
    D_800E2144[1] = AllocMemory(SCENERY_BUFFER_A_SIZE);
    sLieImageAsset = -1;
    sLieImage = AllocMemory(LIE_IMAGE_SIZE);
    sAnimatedIconImages[0] = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_648, &asset));
    ReadEncodedAsset(sAnimatedIconImages[0], &asset);
    sAnimatedIconLabelImages[0] = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_563, &asset));
    ReadEncodedAsset(sAnimatedIconLabelImages[0], &asset);
    sAnimatedIconLabelImages[1] = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_564, &asset));
    ReadEncodedAsset(sAnimatedIconLabelImages[1], &asset);
    sAnimatedIconLabelImages[2] = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_562, &asset));
    ReadEncodedAsset(sAnimatedIconLabelImages[2], &asset);
    sShotOffsetMarkerImage = AllocMemory(OpenEncodedAsset(ASSET_SHOT_OFFSET_MARKER_IMAGE, &asset));
    ReadEncodedAsset(sShotOffsetMarkerImage, &asset);
    sWindSpeedFont = AllocMemory(OpenEncodedAsset(ASSET_WIND_SPEED_FONT, &asset));
    ReadEncodedAsset(sWindSpeedFont, &asset);
    D_800E2168 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_725, &asset));
    ReadEncodedAsset(D_800E2168, &asset);
    D_800E216C = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_568, &asset));
    ReadEncodedAsset(D_800E216C, &asset);
    D_800E2170 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_56B, &asset));
    ReadEncodedAsset(D_800E2170, &asset);
    D_800E2174 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_56A, &asset));
    ReadEncodedAsset(D_800E2174, &asset);
    D_800E217C = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_569, &asset));
    ReadEncodedAsset(D_800E217C, &asset);
    D_800E2178 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_723, &asset));
    ReadEncodedAsset(D_800E2178, &asset);
    D_80105214 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_724, &asset));
    ReadEncodedAsset(D_80105214, &asset);
    LoadBursts(GetCourseRecordIndex());
}

/* Frees what LoadSceneryImages loaded */
void FreeSceneryImages(void) {
    if (sSceneryState == SCENERY_LOADED) {
        sSceneryState = SCENERY_BILLBOARDS_READY;
        FreeEffectImages();
        func_8020D784();
        func_80216F8C();
        func_8020E78C();
        func_8021E6B8();
        FreeMemory(&D_801B55DC);
        FreeMemory(&sWindIconImage);
        FreeMemory(&D_80132360);
        FreeMemory(&D_800FED0C);
        FreeMemory(&g_BackdropPatternImage);
        FreeMemory(&D_8012D428);
        FreeMemory(&g_ShadowTexture);
        FreeMemory(&D_800E2140);
        FreeMemory(&D_800E2144[0]);
        FreeMemory(&D_800E2144[1]);
        FreeMemory(&sLieImage);
        FreeMemory(&sAnimatedIconImages[0]);
        FreeMemory(&sAnimatedIconLabelImages[0]);
        FreeMemory(&sAnimatedIconLabelImages[1]);
        FreeMemory(&sAnimatedIconLabelImages[2]);
        FreeMemory(&sShotOffsetMarkerImage);
        FreeMemory(&sWindSpeedFont);
        FreeMemory(&D_800E2168);
        FreeMemory(&D_800E216C);
        FreeMemory(&D_800E2170);
        FreeMemory(&D_800E2174);
        FreeMemory(&D_800E217C);
        FreeMemory(&D_800E2178);
        FreeMemory(&D_80105214);
        FreeBursts();
    }
}

/* Sets up the billboard manager's heap, once per round */
void InitScenery(void) {
    EncodedAsset unusedAsset; /* Left from code that loaded assets here */

    if (sSceneryState == SCENERY_UNLOADED) {
        sSceneryBillboardHeap = AllocMemory(BILLBOARD_HEAP_SIZE);
        InitBillboardManager(sSceneryBillboardHeap, BILLBOARD_HEAP_SIZE);
        sSceneryState = SCENERY_BILLBOARDS_READY;
    }
}

/* Frees everything the round's scenery holds, when its overlay is unloaded */
void FreeScenery(void) {
    if (sSceneryState != SCENERY_UNLOADED) {
        DestroyBillboardManager();
        FreeMemory(&sSceneryBillboardHeap);
        EndWeather();
        func_80216B74(1);
        FreeSceneryImages();
        sSceneryState = SCENERY_UNLOADED;
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
    if (g_RoundHud.ballHeightMeters < LEVEL_HEIGHT && g_RoundHud.ballHeightMeters > -LEVEL_HEIGHT) {
        g_RoundHud.ballHeightMetersDirection = HEIGHT_LEVEL;
    } else if (g_RoundHud.ballHeightMeters > 0.0f) {
        g_RoundHud.ballHeightMetersDirection = HEIGHT_ABOVE;
    } else {
        g_RoundHud.ballHeightMetersDirection = HEIGHT_BELOW;
    }
    /* The direction in feet is taken from the height in meters */
    if (g_RoundHud.ballHeightFeet < LEVEL_HEIGHT && g_RoundHud.ballHeightFeet > -LEVEL_HEIGHT) {
        g_RoundHud.ballHeightFeetDirection = HEIGHT_LEVEL;
    } else if (g_RoundHud.ballHeightMeters > 0.0f) {
        g_RoundHud.ballHeightFeetDirection = HEIGHT_ABOVE;
    } else {
        g_RoundHud.ballHeightFeetDirection = HEIGHT_BELOW;
    }
    g_RoundHud.cupDistance = (f32)GetCupDistance(g_CurrentShot.ballPosition[AXIS_X] - D_800BA9D8.cupPosition[AXIS_X],
                                     g_CurrentShot.ballPosition[AXIS_Z] - g_CupPositionZ);
    if (GetSurfaceInfo(g_CurrentShot.surface)->unk10 != sLieImageAsset) {
        sLieImageAsset = GetSurfaceInfo(g_CurrentShot.surface)->unk10;
        if (GetSurfaceInfo(g_CurrentShot.surface)->kind == SURFACE_KIND_TEE_GROUND) {
            sShotPanelOffsetY = TEE_GROUND_PANEL_OFFSET_Y;
        } else {
            sShotPanelOffsetY = 0;
        }
        OpenEncodedAsset(sLieImageAsset, &asset);
        ReadEncodedAsset(sLieImage, &asset);
    }
    if (D_801B60B9) {
        g_RoundHud.skinsTotals[0] = 0;
    } else {
        switch (g_GameMode) {
            case GAME_MODE_STROKE:
            case GAME_MODE_UNK1:
            case GAME_MODE_TOURNAMENT:
            case GAME_MODE_UNK7:
                for (playerIndex = 0; playerIndex < g_PlayerCount; playerIndex++) {
                    g_RoundHud.skinsTotals[playerIndex] = GetNetScoreToPar(g_HoleIndex, playerIndex);
                }
                break;
            case GAME_MODE_MATCH_GAME:
            case GAME_MODE_GET_CHARACTER:
                g_RoundHud.skinsTotals[0] = 0;
                for (holeIndex = 0; holeIndex != g_HoleIndex; holeIndex++) {
                    if (g_Players[0].holes[holeIndex].strokes < g_Players[1].holes[holeIndex].strokes) {
                        g_RoundHud.skinsTotals[0]--;
                    } else if (g_Players[1].holes[holeIndex].strokes < g_Players[0].holes[holeIndex].strokes) {
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
    sShotPanelOffsetX = SHOT_PANEL_START_OFFSET_X;
    func_8021E710();
    func_8021419C(&g_Players[g_RoundHud.unk87]);
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

    g_CourseSkyImage = AllocMemory(OpenEncodedAsset(ASSET_COURSE_SKY_IMAGE_0, &asset));
    ReadEncodedAsset(g_CourseSkyImage, &asset);
    g_SkyImage = AllocMemory(OpenEncodedAsset(ASSET_SKY_IMAGE_0, &asset));
    ReadEncodedAsset(g_SkyImage, &asset);
    sHoleImage = AllocMemory(OpenEncodedAsset(sHoleImageAssets[0], &asset));
    ReadEncodedAsset(sHoleImage, &asset);
    sParImage = AllocMemory(OpenEncodedAsset(ASSET_PAR_FIVE_IMAGE, &asset));
    ReadEncodedAsset(sParImage, &asset);
    D_800E2160 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_722, &asset));
    ReadEncodedAsset(D_800E2160, &asset);
    g_RingTexels = AllocMemory(RING_TEXTURE_SIZE * RING_TEXTURE_SIZE);
    BuildRingTexels(g_RingTexels, RING_TEXTURE_SIZE, RING_TEXTURE_SIZE, RING_TEXTURE_OUTER_RADIUS);
    for (i = 0; i != SHOT_OFFSET_MARKER_COUNT; i++) {
        sShotOffsetMarkerXs[i] = 0;
        sShotOffsetMarkerYs[i] = 0;
    }
    g_HudScissor.left = HUD_MARGIN;
    g_HudScissor.top = HUD_MARGIN;
    g_HudScissor.right = SCREEN_WIDTH - HUD_MARGIN;
    D_800E21BC = 0;
    g_HudScissor.bottom = SCREEN_HEIGHT - HUD_MARGIN;
    g_NoiseTexels = AllocMemory(NOISE_GRID_SIZE * NOISE_GRID_SIZE);
    g_BlendedTextureScrollingLayer = AllocMemory(OpenEncodedAsset(ASSET_BLENDED_TEXTURE_SCROLLING_LAYER, &asset));
    ReadEncodedAsset(g_BlendedTextureScrollingLayer, &asset);
    g_BlendedTextureStillLayer = AllocMemory(OpenEncodedAsset(ASSET_BLENDED_TEXTURE_STILL_LAYER, &asset));
    ReadEncodedAsset(g_BlendedTextureStillLayer, &asset);
    for (i = 0; i != MAX_PLAYER_COUNT; i++) {
        g_PlayerFaceImages[i] =
            AllocMemory(OpenEncodedAsset(g_CharacterImageAssets[i] + CHARACTER_FACE_IMAGE_OFFSET, &asset));
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
extern u8 sInsetFrameTexture[];

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
    gDPSetCombineMode(g_DisplayListHead++, G_CC_MODULATEIDECALA_PRIM, G_CC_MODULATEIDECALA_PRIM);
    gDPLoadTextureBlock_4b(g_DisplayListHead++, (u32)sInsetFrameTexture & ~7, G_IM_FMT_I, INSET_FRAME_TEXTURE_WIDTH,
                           INSET_FRAME_TEXTURE_HEIGHT, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,
                           INSET_FRAME_TEXTURE_MASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(g_DisplayListHead++);
    if (g_Cameras[INSET_CAMERA].view.scissorWidth > 0) {
        gSPTextureRectangle(g_DisplayListHead++, g_Cameras[INSET_CAMERA].view.scissorX << 2,
                            (g_Cameras[INSET_CAMERA].view.scissorY - INSET_FRAME_BORDER_OUTSIDE) << 2,
                            (g_Cameras[INSET_CAMERA].view.scissorX + g_Cameras[INSET_CAMERA].view.scissorWidth) << 2,
                            (g_Cameras[INSET_CAMERA].view.scissorY + INSET_FRAME_BORDER_INSIDE) << 2,
                            G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        gSPTextureRectangle(g_DisplayListHead++, g_Cameras[INSET_CAMERA].view.scissorX << 2,
                            (g_Cameras[INSET_CAMERA].view.scissorY + g_Cameras[INSET_CAMERA].view.scissorHeight -
                             INSET_FRAME_BORDER_INSIDE) << 2,
                            (g_Cameras[INSET_CAMERA].view.scissorX + g_Cameras[INSET_CAMERA].view.scissorWidth) << 2,
                            (g_Cameras[INSET_CAMERA].view.scissorY + g_Cameras[INSET_CAMERA].view.scissorHeight +
                             INSET_FRAME_BORDER_OUTSIDE) << 2,
                            G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    }
    gDPPipeSync(g_DisplayListHead++);
    gDPLoadTextureBlock_4b(g_DisplayListHead++, (u32)sInsetFrameTexture & ~7, G_IM_FMT_I, INSET_FRAME_TEXTURE_WIDTH,
                           INSET_FRAME_SIDE_TEXTURE_HEIGHT, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP,
                           G_TX_NOMASK, INSET_FRAME_TEXTURE_MASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(g_DisplayListHead++);
    if (g_Cameras[INSET_CAMERA].view.scissorHeight > 0) {
        gSPTextureRectangle(g_DisplayListHead++,
                            (g_Cameras[INSET_CAMERA].view.scissorX - INSET_FRAME_SIDE_HALF_WIDTH) << 2,
                            g_Cameras[INSET_CAMERA].view.scissorY << 2,
                            (g_Cameras[INSET_CAMERA].view.scissorX + INSET_FRAME_SIDE_HALF_WIDTH) << 2,
                            (g_Cameras[INSET_CAMERA].view.scissorY + g_Cameras[INSET_CAMERA].view.scissorHeight) << 2,
                            G_TX_RENDERTILE, INSET_FRAME_SIDE_TEXEL << 5, 0, 1 << 10, 1 << 10);
        gSPTextureRectangle(g_DisplayListHead++,
                            (g_Cameras[INSET_CAMERA].view.scissorX + g_Cameras[INSET_CAMERA].view.scissorWidth -
                             INSET_FRAME_SIDE_HALF_WIDTH) << 2,
                            g_Cameras[INSET_CAMERA].view.scissorY << 2,
                            (g_Cameras[INSET_CAMERA].view.scissorX + g_Cameras[INSET_CAMERA].view.scissorWidth +
                             INSET_FRAME_SIDE_HALF_WIDTH) << 2,
                            (g_Cameras[INSET_CAMERA].view.scissorY + g_Cameras[INSET_CAMERA].view.scissorHeight) << 2,
                            G_TX_RENDERTILE, INSET_FRAME_SIDE_TEXEL << 5, 0, 1 << 10, 1 << 10);
    }
    gDPPipeSync(g_DisplayListHead++);
}

/* Scrolls the wind arrow's texture along it, faster in stronger wind */
void ScrollWindArrow(void) {
    s32 step = g_HoleConditions.windSpeed / WIND_SPEED_PER_ARROW_SCROLL_STEP;
    s32 i;

    for (i = 0; i != ARROW_VERTEX_COUNT; i++) {
        sArrows[g_MatrixBufferIndex].body[i].v.tc[1] += step;
    }
    if (sArrows[g_MatrixBufferIndex].body[0].v.tc[1] > ARROW_TEXTURE_COORDINATE_LIMIT) {
        for (i = 0; i != ARROW_VERTEX_COUNT; i++) {
            sArrows[g_MatrixBufferIndex].body[i].v.tc[1] -= ARROW_TEXTURE_PERIOD;
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

    sLandingReactionFrame = 0;
    if (reaction == LANDING_REACTION_NONE) {
        sLandingReaction = reaction;
        PlaceGlyphGroup(LANDING_GLYPH_GROUP_FIRST, NO_FONT_SIZE, 0, 0, 0, NULL);
        PlaceGlyphGroup(LANDING_GLYPH_GROUP_SECOND, NO_FONT_SIZE, 0, 0, 0, NULL);
        StartMovie(MOVIE_NONE);
        return;
    }
    previousReaction = sLandingReaction;
    if (previousReaction != LANDING_REACTION_NONE) {
        return;
    }
    sLandingReaction = reaction;
    switch ((u32)GetSurfaceInfo(g_BallSurface)->kind) {
        case SURFACE_KIND_OUT_OF_BOUNDS:
            g_InsetView.animation = INSET_VIEW_OPENING;
            g_InsetView.unk8 = LANDING_INSET_UNK8;
            sLandingReaction = previousReaction;
            g_InsetView.unkC = MOVIE_INSET_UNKC;
            SetInsetViewRect(MOVIE_INSET_CENTER_X, MOVIE_INSET_CENTER_Y, MOVIE_INSET_SIZE, MOVIE_INSET_SIZE);
            SetLandingWaitFrameCount(LANDING_WAIT_LONG_FRAME_COUNT);
            StartMovie(MOVIE_TOAD_OUT_OF_BOUNDS);
            osSyncPrintf("MOVIE KINOP \n");
            break;
        case SURFACE_KIND_OUT_OF_COURSE:
        case SURFACE_KIND_WATER_HAZARD:
            sLandingReaction = LANDING_REACTION_UNK2;
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
            SetInsetViewRect(LANDING_INSET_CENTER_X, LANDING_INSET_CENTER_Y, LANDING_INSET_WIDTH, LANDING_INSET_HEIGHT);
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
void DrawLandingReaction(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    Vec3 scale;
    Vec3 translation;
    Vec3 rotation;
    Mat4 matrix;
    Mat4 offset;
    SurfaceInfo *surface;
    s32 distance;
    s32 hundredths;
    f32 angle;
    f32 size;

    if (sLandingReaction != LANDING_REACTION_NONE) {
        surface = GetSurfaceInfo(g_BallSurface);
        if (sLandingReactionFrame == 1) {
            PlaceGlyphGroup(LANDING_GLYPH_GROUP_FIRST, FONT_SIZE_LARGE, 0, 0, LANDING_NAME_GLYPH_Y, surface->name);
        }
        if (sLandingReaction == LANDING_REACTION_REST && !IsOnDrivingRange()) {
            distance = GetCupDistance(g_BallPosition[AXIS_X] - D_800BA9D8.cupPosition[AXIS_X],
                                    g_BallPosition[AXIS_Z] - g_CupPositionZ);
            /* Mini golf and the green measure in feet */
            if (g_IsMiniGolf || surface->kind == SURFACE_KIND_GREEN) {
                hundredths = distance * (FEET_PER_YARD * HUNDREDTHS_PER_UNIT) * YARDS_PER_UNIT;
                sprintf(g_PrintBuffer, "%2d.%02d%c REST", hundredths / HUNDREDTHS_PER_UNIT,
                        hundredths % HUNDREDTHS_PER_UNIT, FEET_GLYPH);
            } else {
                hundredths = distance * HUNDREDTHS_PER_UNIT * YARDS_PER_UNIT;
                sprintf(g_PrintBuffer, "%3d.%02d%c REST", hundredths / HUNDREDTHS_PER_UNIT,
                        hundredths % HUNDREDTHS_PER_UNIT, YARDS_GLYPH);
            }
            if (sLandingReactionFrame == 1) {
                PlaceGlyphGroup(LANDING_GLYPH_GROUP_SECOND, FONT_SIZE_MEDIUM, 0, 0, LANDING_DISTANCE_GLYPH_Y,
                                g_PrintBuffer);
            }
        }
        if (sLandingReactionFrame < LANDING_REACTION_FRAME_COUNT) {
            sLandingReactionFrame++;
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
        angle = DEGREES_TO_RADIANS(sLandingReactionFrame * LANDING_PANEL_DEGREES_PER_FRAME + LANDING_PANEL_START_DEGREES);
        if (angle > 2 * M_PI) {
            angle = 2 * M_PI;
        }
        size = sLandingReactionFrame * LANDING_PANEL_GROWTH_PER_FRAME;
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
        ConvertMatrixToFixed(matrix, &sLandingPanelMatrices[g_MatrixBufferIndex]);
        gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
        gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sLandingPanelMatrices[g_MatrixBufferIndex]),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPVertex(gfx++, sLandingPanelVertices, ARRAY_COUNT(sLandingPanelVertices), 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        if (sLandingReactionFrame < LANDING_PANEL_GROWTH_FRAME_COUNT) {
            SetUpPerspectiveTextDrawing(&gfx);
            DrawGlyphGroup(&gfx, LANDING_GLYPH_GROUP_FIRST);
            if (sLandingReaction == LANDING_REACTION_REST) {
                DrawGlyphGroup(&gfx, LANDING_GLYPH_GROUP_SECOND);
            }
        } else {
            SetUpTextDrawing(&gfx);
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
            DrawCenteredLargeText(&gfx, SCREEN_WIDTH / 2, LANDING_NAME_Y, surface->name);
            if (sLandingReaction == LANDING_REACTION_REST && !IsOnDrivingRange()) {
                DrawCenteredMediumText(&gfx, SCREEN_WIDTH / 2, LANDING_DISTANCE_Y, g_PrintBuffer);
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

INCLUDE_RODATA("asm/nonmatchings/round_hud", D_800D1B10);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80082534);

/* Gathers the whole ball trail at the ball, so it starts out empty */
void ResetBallTrail(void) {
    s32 i;

    sBallTrailHead = 0;
    for (i = 0; i != BALL_TRAIL_LENGTH; i++) {
        sBallTrail[i][AXIS_X] = g_BallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
        sBallTrail[i][AXIS_Y] = g_BallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        sBallTrail[i][AXIS_Z] = g_BallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
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
/* The trail is white; its newest segment has this alpha, fading by one every few segments */
#define BALL_TRAIL_COLOR 0xFFFFFF00
#define BALL_TRAIL_ALPHA 64
#define BALL_TRAIL_SEGMENTS_PER_FADE 4
#define BALL_TRAIL_SEGMENT_COLOR(segment) ((BALL_TRAIL_ALPHA - (segment) / BALL_TRAIL_SEGMENTS_PER_FADE) | BALL_TRAIL_COLOR)

/* A point of the ball's trail on the screen, in quarter pixels */
typedef struct TrailPoint {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 y;
    /* 0x08 */ s32 depth;
} TrailPoint; /* size = 0x0C */

extern s32 D_800C5AAC;
extern s32 D_800BE69C;
extern TrailPoint sBallTrailPoints[BALL_TRAIL_LENGTH];
extern Mtx sBallTrailMatrix;
/* The quad that each segment of the trail moves into place on the screen */
extern Vtx sBallTrailVertices[4];

/*
 * Draws the ball's trail through its recent positions as screen-space quads
 * that fade out with age. While D_800C5AAC is BALL_TRAIL_RECORDING it also
 * adds the ball's current position.
 *
 * Doesn't match: a display list pointer's spill is scheduled differently,
 * and the offsets' floating-point registers are swapped.
 */
#ifdef NON_MATCHING
void DrawBallTrail(Gfx **displayListHead, CameraMatrices *matrices) {
    Gfx *gfx = *displayListHead;
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
            if (++sBallTrailHead == BALL_TRAIL_LENGTH) {
                sBallTrailHead = 0;
            }
            sBallTrail[sBallTrailHead][AXIS_X] = g_BallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
            sBallTrail[sBallTrailHead][AXIS_Y] =
                g_BallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE - g_PinVertexScale * BALL_TRAIL_DROP;
            sBallTrail[sBallTrailHead][AXIS_Z] = g_BallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        }
        if (D_800BE69C != 1) {
            /* Projects the trail from its newest position back */
            index = sBallTrailHead;
            for (i = 0; i != BALL_TRAIL_LENGTH; i++, index--) {
                if (index == -1) {
                    index = BALL_TRAIL_LENGTH - 1;
                }
                if (ProjectPoint(sBallTrail[index][AXIS_X], sBallTrail[index][AXIS_Y], sBallTrail[index][AXIS_Z],
                                 &point) < 0) {
                    sBallTrailPoints[i].x = point.x * 4;
                    sBallTrailPoints[i].y = point.y * 4;
                    sBallTrailPoints[i].depth = point.depth - point.depthStep * (g_PinVertexScale * BALL_TRAIL_DEPTH_BIAS);
                } else {
                    sBallTrailPoints[i].depth = BALL_TRAIL_HIDDEN;
                }
            }
            guMtxIdent(&sBallTrailMatrix);
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            gDPSetCycleType(gfx++, G_CYC_1CYCLE);
            gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
            gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
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
            gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
            gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sBallTrailMatrix), G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPVertex(gfx++, sBallTrailVertices, ARRAY_COUNT(sBallTrailVertices), 0);
            for (i = 0; i != BALL_TRAIL_LENGTH - 1; i++) {
                if (sBallTrailPoints[i].depth != BALL_TRAIL_HIDDEN && sBallTrailPoints[i + 1].depth != BALL_TRAIL_HIDDEN) {
                    deltaX = sBallTrailPoints[i].x - sBallTrailPoints[i + 1].x;
                    deltaY = sBallTrailPoints[i].y - sBallTrailPoints[i + 1].y;
                    /* The empty branch for a segment of no length is there to match */
                    if (deltaX == 0.0f & deltaY == 0.0f) {
                    } else {
                        scale = BALL_TRAIL_HALF_WIDTH / sqrtf(deltaX * deltaX + deltaY * deltaY);
                        offsetX = deltaY * scale;
                        offsetY = deltaX * scale;
                        gDPPipeSync(gfx++);
                        gDPSetPrimDepth(gfx++, sBallTrailPoints[i].depth, 0);
                        gDPPipeSync(gfx++);
                        gSPModifyVertex(gfx++, 0, G_MWO_POINT_RGBA, BALL_TRAIL_SEGMENT_COLOR(i));
                        gSPModifyVertex(gfx++, 1, G_MWO_POINT_RGBA, BALL_TRAIL_SEGMENT_COLOR(i));
                        gSPModifyVertex(gfx++, 2, G_MWO_POINT_RGBA, BALL_TRAIL_SEGMENT_COLOR(i));
                        gSPModifyVertex(gfx++, 3, G_MWO_POINT_RGBA, BALL_TRAIL_SEGMENT_COLOR(i));
                        gSPModifyVertex(gfx++, 0, G_MWO_POINT_XYSCREEN,
                                        ((sBallTrailPoints[i].x + (s32)offsetX) << 16) + (sBallTrailPoints[i].y - (s32)offsetY));
                        gSPModifyVertex(gfx++, 1, G_MWO_POINT_XYSCREEN,
                                        ((sBallTrailPoints[i].x - (s32)offsetX) << 16) + (sBallTrailPoints[i].y + (s32)offsetY));
                        gSPModifyVertex(gfx++, 2, G_MWO_POINT_XYSCREEN,
                                        ((sBallTrailPoints[i + 1].x + (s32)offsetX) << 16) +
                                            (sBallTrailPoints[i + 1].y - (s32)offsetY));
                        gSPModifyVertex(gfx++, 3, G_MWO_POINT_XYSCREEN,
                                        ((sBallTrailPoints[i + 1].x - (s32)offsetX) << 16) +
                                            (sBallTrailPoints[i + 1].y + (s32)offsetY));
                        gSP2Triangles(gfx++, 1, 0, 2, 0, 1, 2, 3, 0);
                    }
                }
            }
            gDPPipeSync(gfx++);
            gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&matrices->projection), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
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
 * schedules the last commands before reading g_RoundHud.unk8A. Its rodata also has
 * an empty string after "%dm", which this doesn't emit.
 */
#ifdef NON_MATCHING
void DrawWindSpeed(Gfx **displayListHead, f32 x, f32 y) {
    Gfx *gfx = *displayListHead;
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

    /* Draws text of digits, spaces and 'm' from the wind speed font, with its top left at the given point */
    void DrawWindSpeedText(s32 textX, s32 textY, char *text) {
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
            gSPScisTextureRectangle(gfx++, (textX + offsetX) << 2, (textY - WIND_TEXT_RAISE) << 2,
                                    (textX + offsetX + glyphWidth) << 2,
                                    (textY - WIND_TEXT_RAISE + WIND_FONT_HEIGHT) << 2, G_TX_RENDERTILE,
                                    glyphS << 5, 0, 1 << 10, 1 << 10);
        }
    }

    sWindIconPulseAngle += WIND_ICON_PULSE_STEP;
    speed = (u32)g_HoleConditions.windSpeed * WIND_SPEED_TO_METERS_SCALE / WIND_SPEED_TO_METERS_DIVISOR;
    if (speed >= WIND_SPEED_TWO_DIGITS) {
        halfWidth = sinf(sWindIconPulseAngle) + WIND_ICON_WIDE_HALF_WIDTH;
    } else {
        halfWidth = sinf(sWindIconPulseAngle) + WIND_ICON_NARROW_HALF_WIDTH;
    }
    halfHeight = sinf(sWindIconPulseAngle) + WIND_ICON_HALF_HEIGHT;
    halfWidthRounded = halfWidth + 0.5f;
    halfHeightRounded = halfHeight + 0.5f;
    dsdx = (WIND_ICON_TEXTURE_WIDTH / 2 << 10) / halfWidth;
    dtdy = (WIND_ICON_TEXTURE_HEIGHT / 2 << 10) / halfHeight;
    /* Moves the texture by the rounding, in 10.5 fixed point, from half a texel in */
    textureS = HALF_TEXEL - (halfWidthRounded - halfWidth) * HALF_TEXEL;
    textureT = HALF_TEXEL - (halfHeightRounded - halfHeight) * HALF_TEXEL;
    /* The centers are kept positive by WIND_POSITIVE_BIAS, so rounding by truncation works */
    if (speed >= WIND_SPEED_TWO_DIGITS) {
        centerX = -sinf(g_SkyState.viewedWindAngles[0]) * WIND_WIDE_LEAN_X + (x + WIND_OFFSET_X) + WIND_POSITIVE_BIAS;
    } else {
        centerX = -sinf(g_SkyState.viewedWindAngles[0]) * WIND_NARROW_LEAN_X + (x + WIND_OFFSET_X) + WIND_POSITIVE_BIAS;
    }
    centerY = -cosf(g_SkyState.viewedWindAngles[0]) * WIND_LEAN_Y + (SCREEN_HEIGHT / 2 - y) - WIND_OFFSET_Y + WIND_POSITIVE_BIAS;
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
    gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0);
    gDPLoadTextureBlock_4b(gfx++, ((u32)sWindIconImage + IMAGE_HEADER_SIZE) & ~7, G_IM_FMT_I, WIND_ICON_TEXTURE_WIDTH,
                           WIND_ICON_TEXTURE_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    roundedCenterX = centerX + 0.5f;
    roundedCenterY = centerY + 0.5f;
    gSPScisTextureRectangle(gfx++, (roundedCenterX - WIND_POSITIVE_BIAS + WIND_ICON_OFFSET_X - halfWidthRounded) << 2,
                            (roundedCenterY - WIND_POSITIVE_BIAS + WIND_ICON_OFFSET_Y - halfHeightRounded) << 2,
                            (roundedCenterX - WIND_POSITIVE_BIAS + WIND_ICON_OFFSET_X + halfWidthRounded) << 2,
                            (roundedCenterY - WIND_POSITIVE_BIAS + WIND_ICON_OFFSET_Y + halfHeightRounded) << 2,
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
    gDPSetPrimColor(gfx++, 0, 0, WIND_TEXT_PRIM_RED, WIND_TEXT_PRIM_GREEN, WIND_TEXT_PRIM_BLUE, 255);
    gDPSetEnvColor(gfx++, WIND_TEXT_ENV_RED, WIND_TEXT_ENV_GREEN, WIND_TEXT_ENV_BLUE, 255);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)sWindSpeedFont + IMAGE_HEADER_SIZE) & ~7);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTextureBlock_4b(gfx++, ((u32)sWindSpeedFont + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI, WIND_FONT_WIDTH,
                           WIND_FONT_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                           G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    sprintf(g_PrintBuffer, "%dm", speed);
    if (speed >= WIND_SPEED_TWO_DIGITS) {
        DrawWindSpeedText(centerX - WIND_POSITIVE_BIAS - WIND_WIDE_TEXT_SHIFT_X, centerY - WIND_POSITIVE_BIAS,
                      g_PrintBuffer);
    } else {
        DrawWindSpeedText(centerX - WIND_POSITIVE_BIAS, centerY - WIND_POSITIVE_BIAS, g_PrintBuffer);
    }
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    if (g_RoundHud.unk8A == 1) {
        if (speed != 0) {
            func_802199EC(&gfx, centerX - WIND_HIGHLIGHT_OFFSET_X, centerY - WIND_HIGHLIGHT_OFFSET_Y,
                          WIND_HIGHLIGHT_WIDE_SIZE);
        } else {
            func_802199EC(&gfx, centerX - WIND_HIGHLIGHT_OFFSET_X, centerY - WIND_HIGHLIGHT_OFFSET_Y,
                          WIND_HIGHLIGHT_NARROW_SIZE);
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
void DrawWindIndicator(Gfx **displayListHead, f32 x, f32 y) {
    Vec3 viewDirection;
    Vec3 rotation;
    Vec3 translation;
    Vec3 unusedScale;
    Mat4 matrix;
    Mat4 transform;
    Gfx *gfx = *displayListHead;

    viewDirection[AXIS_X] = g_Cameras[0].view.lookAt[AXIS_X] - g_Cameras[0].view.eye[AXIS_X];
    viewDirection[AXIS_Z] = g_Cameras[0].view.lookAt[AXIS_Z] - g_Cameras[0].view.eye[AXIS_Z];
    viewDirection[AXIS_Y] = g_Cameras[0].view.lookAt[AXIS_Y] - g_Cameras[0].view.eye[AXIS_Y];
    rotation[AXIS_X] = atan2(viewDirection[AXIS_Y], sqrtf(viewDirection[AXIS_X] * viewDirection[AXIS_X] +
                                                          viewDirection[AXIS_Z] * viewDirection[AXIS_Z])) +
                       M_PI + WIND_INDICATOR_TILT;
    rotation[AXIS_Y] = 0.0f;
    rotation[AXIS_Z] = 0.0f;
    translation[AXIS_X] = x;
    translation[AXIS_Y] = y;
    translation[AXIS_Z] = WIND_INDICATOR_DEPTH;
    BuildRotationMatrix(matrix, rotation, translation);
    guRotateRPYF(transform, 0.0f,
                 WIND_INDICATOR_FACING_DEGREES - g_SkyState.viewedWindAngles[0] * 360.0f * (1.0 / (2 * M_PI)), 0.0f);
    guMtxCatF(transform, matrix, matrix);
    guScaleF(transform, WIND_INDICATOR_SCALE, WIND_INDICATOR_SCALE, WIND_INDICATOR_SCALE);
    guMtxCatF(transform, matrix, matrix);
    guMtxF2L(matrix, &sWindIndicatorMatrices[g_MatrixBufferIndex]);
    if (!(g_SkyState.viewedWindAngles[0] > M_PI / 2 && g_SkyState.viewedWindAngles[0] < 3 * M_PI / 2)) {
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
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&sWindIndicatorMatrices[g_MatrixBufferIndex]),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPSetLights1(gfx++, sWindArrowLights);
    if (g_IsModuleAlive[MODULE_UNK9]) {
        g_ModuleRdpRequestFrames[MODULE_UNK9] = g_FrameCount;
        gSPDisplayList(gfx++, D_80222400);
    }
    if (g_SkyState.viewedWindAngles[0] > M_PI / 2 && g_SkyState.viewedWindAngles[0] < 3 * M_PI / 2) {
        DrawWindSpeed(&gfx, x, y);
    }
    *displayListHead = gfx;
}

/* Lets the second controller's C buttons nudge the HUD up and down, and prints the offset */
void AdjustHudOffset(void) {
    if (IsGameFlagSet(GAME_FLAG_ADJUST_HUD_OFFSET)) {
        if (g_ControllerStates[HUD_OFFSET_CONTROLLER].pressedButtons & U_CBUTTONS) {
            sHudOffsetY--;
        }
        if (g_ControllerStates[HUD_OFFSET_CONTROLLER].pressedButtons & D_CBUTTONS) {
            sHudOffsetY++;
        }
        sprintf(g_PrintBuffer, "%3d", sHudOffsetY);
        PrintDebugText(g_PrintBuffer, HUD_OFFSET_TEXT_COLUMN, HUD_OFFSET_TEXT_ROW);
    }
}

INCLUDE_ASM("asm/nonmatchings/round_hud", func_8008534C);

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
    Vtx *vertex;

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
    gDPLoadTLUT_pal256(g_DisplayListHead++, ((u32)sHoleImage + IMAGE_HEADER_SIZE) & ~7);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTextureLUT(g_DisplayListHead++, G_TT_RGBA16);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
    gSPTexture(g_DisplayListHead++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPLoadTextureBlock(g_DisplayListHead++, ((u32)sHoleImage + LARGE_PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
                        G_IM_SIZ_8b, HOLE_IMAGE_WIDTH, HOLE_IMAGE_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(g_DisplayListHead++);
    gSPVertex(g_DisplayListHead++, sHoleFlagVertices[0][g_MatrixBufferIndex], HOLE_FLAG_VERTEX_COUNT, 0);
    for (row = 0; row != HOLE_FLAG_ROW_COUNT - 1; row++) {
        for (column = 0; column != HOLE_FLAG_COLUMN_COUNT - 1; column++) {
            index = row * HOLE_FLAG_COLUMN_COUNT + column;
            gSP2Triangles(g_DisplayListHead++, index, index + HOLE_FLAG_COLUMN_COUNT, index + HOLE_FLAG_COLUMN_COUNT + 1,
                          0, index, index + HOLE_FLAG_COLUMN_COUNT + 1, index + 1, 0);
        }
    }
    if (isBackDrawn == TRUE) {
        index = 0;
        vertex = sHoleFlagVertices[0][g_MatrixBufferIndex];
        for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
            for (column = 0; column != HOLE_FLAG_COLUMN_COUNT; column++, index++, vertex++) {
                /* These texture coordinates are after the gSPTexture scale, which halves them */
                gSPModifyVertex(g_DisplayListHead++, index, G_MWO_POINT_ST,
                                ((HOLE_FLAG_BACK_TEXEL_START / 2 - column * (HOLE_FLAG_TEXEL_COLUMN_STEP / 2)) << 16) +
                                    row * (HOLE_FLAG_TEXEL_ROW_STEP / 2));
                gSPModifyVertex(g_DisplayListHead++, index, G_MWO_POINT_RGBA,
                                ((2 * HOLE_FLAG_START_SHADE - vertex->v.cn[0]) << 24) |
                                    ((2 * HOLE_FLAG_START_SHADE - vertex->v.cn[1]) << 16) |
                                    ((2 * HOLE_FLAG_START_SHADE - vertex->v.cn[2]) << 8) | HOLE_FLAG_START_SHADE);
            }
        }
        for (row = 0; row != HOLE_FLAG_ROW_COUNT - 1; row++) {
            for (column = 0; column != HOLE_FLAG_COLUMN_COUNT - 1; column++) {
                index = row * HOLE_FLAG_COLUMN_COUNT + column;
                gSP2Triangles(g_DisplayListHead++, index, index + HOLE_FLAG_COLUMN_COUNT + 1,
                              index + HOLE_FLAG_COLUMN_COUNT, 0, index, index + 1, index + HOLE_FLAG_COLUMN_COUNT + 1, 0);
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
    ConvertMatrixToFixed(matrix, &sHoleImageMatrix);
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
    gSPClearGeometryMode(g_DisplayListHead++, 0xFFFFFFFF);
    gSPSetGeometryMode(g_DisplayListHead++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetRenderMode(g_DisplayListHead++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
    gSPPerspNormalize(g_DisplayListHead++, g_ScreenEffectPerspNorm);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
    gSPMatrix(g_DisplayListHead++, OS_K0_TO_PHYSICAL(&g_ScreenEffectProjection),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(g_DisplayListHead++, OS_K0_TO_PHYSICAL(&sHoleImageMatrix), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    DrawHoleFlag(FALSE);
    gDPPipeSync(g_DisplayListHead++);
}

/* Starts one of the effects over a shot in flight, or stops it with SHOT_EFFECT_NONE */
void StartShotEffect(s32 effect) {
    s32 golferKind;

    if (effect == SHOT_EFFECT_NONE) {
        sShotEffectTime = SHOT_EFFECT_TIME_OFF;
        return;
    }
    if (effect == SHOT_EFFECT_LONG_SHOT) {
        golferKind = func_80213C78(&g_Players[g_RoundHud.unk87]);
        if (golferKind != GOLFER_KIND_UNK10 & golferKind != GOLFER_KIND_UNK2) {
            PlaySoundWithEffects(SOUND_LONG_SHOT, LONG_SHOT_SOUND_CHANNEL, LONG_SHOT_SOUND_VOLUME, LONG_SHOT_SOUND_REVERB,
                                 LONG_SHOT_SOUND_PAN);
        } else {
            PlaySoundWithEffects(SOUND_LONG_SHOT_HEAVY, LONG_SHOT_SOUND_CHANNEL, LONG_SHOT_HEAVY_SOUND_VOLUME,
                                 LONG_SHOT_SOUND_REVERB, LONG_SHOT_SOUND_PAN);
        }
    }
    sShotEffect = effect;
    sShotEffectTime = 0.0f;
}

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80086864);

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
            HOLE_FLAG_VERTEX(row, column).v.ob[0] = (s16)(column * HOLE_FLAG_CELL_SIZE - column * factor) -
                                                    row * otherFactor * HOLE_FLAG_ROW_SKEW_SCALE;
            if (HOLE_FLAG_VERTEX(row, column).v.ob[0] < column * otherFactor) {
                HOLE_FLAG_VERTEX(row, column).v.ob[0] = column * otherFactor;
            }
            HOLE_FLAG_VERTEX(row, column).v.ob[1] = -row * HOLE_FLAG_CELL_SIZE - column * factor;
        }
    }
    if (windSpeed > 0.0f && windSpeed < HOLE_FLAG_MIN_WIND_SPEED) {
        windSpeed = HOLE_FLAG_MIN_WIND_SPEED;
    }
    /* factor is how much of its neighbor's shade a vertex takes; otherFactor is how much of its own it keeps */
    if (windSpeed < 1.0f) {
        factor = HOLE_FLAG_CALM_SPREAD;
        otherFactor = 1.0f - HOLE_FLAG_CALM_SPREAD;
    } else if (windSpeed > HOLE_FLAG_GALE_WIND_SPEED) {
        factor = 1.0f;
        otherFactor = 0.0f;
    } else {
        factor = windSpeed * HOLE_FLAG_SPREAD_STEP + HOLE_FLAG_SPREAD_STEP;
        otherFactor = (HOLE_FLAG_SPREAD_STEP_COUNT - windSpeed) * HOLE_FLAG_SPREAD_STEP - HOLE_FLAG_SPREAD_STEP;
    }
    for (column = HOLE_FLAG_COLUMN_COUNT - 1; column != 0; column--) {
        for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
            /* Indexing the left neighbor as HOLE_FLAG_SHADE(row, column - 1) changes the code */
            shade = sHoleFlagShades[row * HOLE_FLAG_COLUMN_COUNT + column - 1] * factor +
                    HOLE_FLAG_SHADE(row, column) * otherFactor;
            HOLE_FLAG_SHADE(row, column) = shade;
        }
    }
    sHoleFlagWavePhase +=
        (HOLE_FLAG_SWING_DEGREES_PER_SPEED * windSpeed + HOLE_FLAG_MIN_SWING_DEGREES) * DEGREES_TO_RADIANS(1);
    if (sHoleFlagWavePhase > 2 * M_PI) {
        sHoleFlagWavePhase -= 2 * M_PI;
    }
    if (sHoleFlagWavePhase < 0.0f) {
        sHoleFlagWavePhase += 2 * M_PI;
    }
    /* factor is the swing's amplitude here */
    factor = windSpeed + rand() % HOLE_FLAG_SWING_JITTER_RANGE;
    shade = (s32)(sinf(sHoleFlagWavePhase) * factor) + HOLE_FLAG_START_SHADE;
    for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
        if ((s32)windSpeed == 0) {
            rowShade = shade;
        } else {
            rowShade = (f32)(shade + rand() % (s32)(windSpeed * HOLE_FLAG_ROW_JITTER_PER_SPEED)) -
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
            bulge = (f32)(HOLE_FLAG_VERTEX(row, column).v.cn[0] - HOLE_FLAG_START_SHADE) /
                    (f32)(HOLE_FLAG_BULGE_COLUMN_BASE - column) * factor;
            otherFactor = HOLE_FLAG_BULGE_SCALE;
            HOLE_FLAG_VERTEX(row, column).v.ob[2] =
                bulge * otherFactor * (f32)(HOLE_FLAG_BULGE_ROW_BASE - row) * HOLE_FLAG_BULGE_ROW_SCALE;
        }
    }
    for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
        pull = 0;
        for (column = HOLE_FLAG_FIRST_PULLED_COLUMN; column != HOLE_FLAG_COLUMN_COUNT; column++) {
            depth = HOLE_FLAG_VERTEX(row, column).v.ob[2];
            if (depth < 0) {
                depth = -depth;
            }
            pull += depth / HOLE_FLAG_DEPTH_PER_PULL;
            HOLE_FLAG_VERTEX(row, column).v.ob[0] = column * HOLE_FLAG_CELL_SIZE - pull;
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
        sShotOffsetMarkerXs[i] = 0;
        sShotOffsetMarkerYs[i] = 0;
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
    sShotOffsetMarkerXs[0] = x / SHOT_OFFSET_UNITS_PER_PIXEL;
    sShotOffsetMarkerYs[0] = -y / SHOT_OFFSET_UNITS_PER_PIXEL;
}

/* Draws the shot offset marker, animated while HUD_FLAG_ANIMATE_OFFSET_MARKER is set */
#ifdef NON_MATCHING
void DrawShotOffsetMarker(void) {
    gDPPipeSync(g_DisplayListHead++);
    gDPLoadTLUT_pal16(g_DisplayListHead++, 0, ((u32)sShotOffsetMarkerImage + IMAGE_HEADER_SIZE) & ~7);
    sShotOffsetMarkerFrame = (sShotOffsetMarkerFrame + 1) % SHOT_OFFSET_MARKER_FRAME_COUNT;
    if (g_RoundHud.flags & HUD_FLAG_ANIMATE_OFFSET_MARKER) {
        sShotOffsetMarkerTexelOffset = sShotOffsetMarkerFrame * SHOT_OFFSET_MARKER_FRAME_SIZE;
    } else {
        sShotOffsetMarkerTexelOffset = SHOT_OFFSET_MARKER_STILL_FRAME * SHOT_OFFSET_MARKER_FRAME_SIZE;
    }
    gDPLoadTextureBlock_4b(g_DisplayListHead++,
                           ((u32)sShotOffsetMarkerImage + PALETTED_TEXELS_OFFSET + sShotOffsetMarkerTexelOffset) & ~7,
                           G_IM_FMT_CI, SHOT_OFFSET_MARKER_SIZE, SHOT_OFFSET_MARKER_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(g_DisplayListHead++);
    gSPScisTextureRectangle(g_DisplayListHead++, (sShotOffsetMarkerXs[0] + (sShotPanelOffsetX + SHOT_OFFSET_MARKER_X)) << 2,
                            (sShotOffsetMarkerYs[0] + (sShotPanelOffsetY + SHOT_OFFSET_MARKER_Y) + sHudOffsetY) << 2,
                            (sShotOffsetMarkerXs[0] + (sShotPanelOffsetX + SHOT_OFFSET_MARKER_X) + SHOT_OFFSET_MARKER_SIZE) << 2,
                            (sShotOffsetMarkerYs[0] + (sShotPanelOffsetY + SHOT_OFFSET_MARKER_Y + SHOT_OFFSET_MARKER_SIZE) +
                             sHudOffsetY) << 2,
                            G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
}
#else
INCLUDE_ASM("asm/nonmatchings/round_hud", DrawShotOffsetMarker);
#endif

/* The icon cycles through ANIMATED_ICON_FRAME_COUNT images, a new one every few frames */
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
extern u32 sAnimatedIconFrameAssets[ANIMATED_ICON_FRAME_COUNT];
extern s32 sAnimatedIconFrameTime;
extern s32 sAnimatedIconFrame;
extern s32 sAnimatedIconImageIndex;
extern f32 sAnimatedIconX;
extern f32 sAnimatedIconY;

/*
 * Draws an animated icon that eases into place, with a label beside it that
 * depends on D_800C308C. Each new frame of the icon is read into the same
 * image.
 */
void DrawAnimatedIcon(Gfx **displayListHead, s32 mode) {
    Gfx *gfx = *displayListHead;
    EncodedAsset asset;
    s32 label;

    if (sSceneryState == SCENERY_LOADED) {
        if (mode == ANIMATED_ICON_MODE_UNK2) {
            sAnimatedIconX = 0.0f;
            sAnimatedIconY = 0.0f;
        } else {
            sAnimatedIconX += (ANIMATED_ICON_X - sAnimatedIconX) * ANIMATED_ICON_EASING;
            sAnimatedIconY += (ANIMATED_ICON_Y - sAnimatedIconY) * ANIMATED_ICON_EASING;
        }
        if (++sAnimatedIconFrameTime == ANIMATED_ICON_FRAME_LENGTH) {
            sAnimatedIconFrameTime = 0;
            sAnimatedIconFrame = (sAnimatedIconFrame + 1) % ANIMATED_ICON_FRAME_COUNT;
            OpenEncodedAsset(sAnimatedIconFrameAssets[sAnimatedIconFrame], &asset);
            ReadEncodedAsset(sAnimatedIconImages[sAnimatedIconImageIndex], &asset);
        }
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal256(gfx++, (u32)sAnimatedIconImages[sAnimatedIconImageIndex] + IMAGE_HEADER_SIZE & ~7);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, HOLE_INTRO_IMAGE_RENDER_MODE, HOLE_INTRO_IMAGE_RENDER_MODE2);
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPLoadTextureBlock(gfx++,
                            (u32)sAnimatedIconImages[sAnimatedIconImageIndex] + LARGE_PALETTED_TEXELS_OFFSET & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, ANIMATED_ICON_SIZE, ANIMATED_ICON_SIZE, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, (s32)sAnimatedIconX << 2, (s32)sAnimatedIconY << 2,
                                ((s32)sAnimatedIconX + ANIMATED_ICON_SIZE) << 2,
                                ((s32)sAnimatedIconY + ANIMATED_ICON_SIZE) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10,
                                1 << 10);
        gDPPipeSync(gfx++);
        if (D_800C308C == 1) {
            label = ANIMATED_ICON_LABEL_UNK1;
        } else if (D_800C308C == 2) {
            label = ANIMATED_ICON_LABEL_UNK0;
        } else {
            label = ANIMATED_ICON_LABEL_UNK2;
        }
        gDPLoadTLUT_pal16(gfx++, 0, (u32)sAnimatedIconLabelImages[label] + IMAGE_HEADER_SIZE & ~7);
        gDPLoadTextureBlock_4b(gfx++, (u32)sAnimatedIconLabelImages[label] + PALETTED_TEXELS_OFFSET & ~7,
                               G_IM_FMT_CI, ANIMATED_ICON_LABEL_WIDTH, ANIMATED_ICON_LABEL_HEIGHT, 0,
                               G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                               G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, ((s32)sAnimatedIconX + ANIMATED_ICON_LABEL_OFFSET_X) << 2,
                                (s32)sAnimatedIconY << 2,
                                ((s32)sAnimatedIconX + ANIMATED_ICON_LABEL_OFFSET_X + ANIMATED_ICON_LABEL_WIDTH) << 2,
                                ((s32)sAnimatedIconY + ANIMATED_ICON_LABEL_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                                1 << 10, 1 << 10);
        *displayListHead = gfx;
    }
}

/*
 * Draws black bars at the top and bottom of the HUD's area, growing them each
 * frame up to LETTERBOX_MAX_HEIGHT. Returns their new height.
 */
s32 DrawLetterbox(Gfx **gfxP, s32 height) {
    Gfx *gfx = *gfxP;

    if (g_RoundHud.isLetterboxShown) {
        if (D_800C308C != 0 & D_800C308C != LETTERBOX_STATE_UNK5) {
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_XLU_SURF | ZMODE_XLU, G_RM_XLU_SURF2 | ZMODE_XLU);
            gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
            gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 255);
            gDPPipeSync(gfx++);
            if (height > 0) {
                gSPScisTextureRectangle(gfx++, HUD_MARGIN << 2, HUD_MARGIN << 2, (SCREEN_WIDTH - HUD_MARGIN) << 2,
                                        (HUD_MARGIN + height) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
                gSPScisTextureRectangle(gfx++, HUD_MARGIN << 2, (SCREEN_HEIGHT - HUD_MARGIN - height) << 2,
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
void DrawDistanceText(Gfx **gfxP, s32 x, s32 y, char *text) {
    s32 width;
    s32 glyphS;
    Gfx *gfx = *gfxP;

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
        gSPTextureRectangle(gfx++, x << 2, y << 2, (x + width) << 2, (y + DISTANCE_GLYPH_HEIGHT) << 2, G_TX_RENDERTILE,
                            glyphS << 5, 0, 1 << 10, 1 << 10);
        x += width;
    }
    *gfxP = gfx;
}

INCLUDE_RODATA("asm/nonmatchings/round_hud", sColorTestSampleText);

INCLUDE_RODATA("asm/nonmatchings/round_hud", sBlankText);

/* Draws the colour test screen and handles its controls */
void DrawColorTest(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    SetUpGradientTextDrawing(&gfx, sIsTestGradientShown);
    gDPSetPrimColor(gfx++, 0, 0, sTestColors[COLOR_TEST_PRIM][0], sTestColors[COLOR_TEST_PRIM][1],
                    sTestColors[COLOR_TEST_PRIM][2], COLOR_CHANNEL_MAX);
    gDPSetEnvColor(gfx++, sTestColors[COLOR_TEST_ENV][0], sTestColors[COLOR_TEST_ENV][1],
                   sTestColors[COLOR_TEST_ENV][2], COLOR_CHANNEL_MAX);
    gDPPipeSync(gfx++);
    /* The casts read only the low halves, as the target does */
    DrawCenteredLargeText(&gfx, (s16)sTestTextX, (s16)sTestTextY, sColorTestSampleText);
    gDPPipeSync(gfx++);
    PrintDebugText(sBlankText, sTestChannelIndex * COLOR_TEST_COLUMN_WIDTH + COLOR_TEST_TEXT_COLUMN - 1,
                   sTestColorIndex + COLOR_TEST_TEXT_ROW);
    if (g_ControllerStates[COLOR_TEST_CONTROLLER].repeatedButtons & U_CBUTTONS) {
        sTestColorIndex ^= 1;
    }
    if (g_ControllerStates[COLOR_TEST_CONTROLLER].repeatedButtons & D_CBUTTONS) {
        sTestColorIndex ^= 1;
    }
    if (g_ControllerStates[COLOR_TEST_CONTROLLER].repeatedButtons & L_CBUTTONS) {
        sTestChannelIndex = (sTestChannelIndex + COLOR_TEST_CHANNEL_COUNT - 1) % COLOR_TEST_CHANNEL_COUNT;
    }
    if (g_ControllerStates[COLOR_TEST_CONTROLLER].repeatedButtons & R_CBUTTONS) {
        sTestChannelIndex = (sTestChannelIndex + 1) % COLOR_TEST_CHANNEL_COUNT;
    }
    if (g_ControllerStates[COLOR_TEST_CONTROLLER].buttons & A_BUTTON) {
        sTestColors[sTestColorIndex][sTestChannelIndex] += COLOR_TEST_STEP;
    }
    if (g_ControllerStates[COLOR_TEST_CONTROLLER].buttons & B_BUTTON) {
        sTestColors[sTestColorIndex][sTestChannelIndex] -= COLOR_TEST_STEP;
    }
    if (g_ControllerStates[COLOR_TEST_CONTROLLER].pressedButtons & R_TRIG) {
        sIsTestGradientShown ^= 1;
    }
    if (g_ControllerStates[COLOR_TEST_CONTROLLER].buttons & U_JPAD) {
        sTestTextY -= COLOR_TEST_STEP;
    }
    if (g_ControllerStates[COLOR_TEST_CONTROLLER].buttons & D_JPAD) {
        sTestTextY += COLOR_TEST_STEP;
    }
    if (g_ControllerStates[COLOR_TEST_CONTROLLER].buttons & L_JPAD) {
        sTestTextX -= COLOR_TEST_STEP;
    }
    if (g_ControllerStates[COLOR_TEST_CONTROLLER].buttons & R_JPAD) {
        sTestTextX += COLOR_TEST_STEP;
    }
    if (sTestColors[sTestColorIndex][sTestChannelIndex] > COLOR_CHANNEL_MAX) {
        sTestColors[sTestColorIndex][sTestChannelIndex] = COLOR_CHANNEL_MAX;
    }
    if (sTestColors[sTestColorIndex][sTestChannelIndex] < 0) {
        sTestColors[sTestColorIndex][sTestChannelIndex] = 0;
    }
    sprintf(g_PrintBuffer, "%3d  %3d  %3d", sTestColors[COLOR_TEST_PRIM][0], sTestColors[COLOR_TEST_PRIM][1],
            sTestColors[COLOR_TEST_PRIM][2]);
    PrintDebugText(g_PrintBuffer, COLOR_TEST_TEXT_COLUMN, COLOR_TEST_TEXT_ROW + COLOR_TEST_PRIM);
    sprintf(g_PrintBuffer, "%3d  %3d  %3d", sTestColors[COLOR_TEST_ENV][0], sTestColors[COLOR_TEST_ENV][1],
            sTestColors[COLOR_TEST_ENV][2]);
    PrintDebugText(g_PrintBuffer, COLOR_TEST_TEXT_COLUMN, COLOR_TEST_TEXT_ROW + COLOR_TEST_ENV);
    PrintDebugText(">", sTestChannelIndex * COLOR_TEST_COLUMN_WIDTH + COLOR_TEST_TEXT_COLUMN - 1,
                   sTestColorIndex + COLOR_TEST_TEXT_ROW);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80089094);

INCLUDE_RODATA("asm/nonmatchings/round_hud", D_800D1C84);

INCLUDE_RODATA("asm/nonmatchings/round_hud", D_800D1C98);

INCLUDE_RODATA("asm/nonmatchings/round_hud", jtbl_800D1CA0);

/* Loads the images that the hole's intro shows for its contest bet and the match standing */
void LoadHoleIntroImages(void) {
    EncodedAsset asset;
    s32 standing;

    sLoadedHoleIntroImages = 0;
    if (g_GameState.betRates[BET_LONGEST_DRIVE] != UNSET_BET_RATE && IsMarkedParFive(g_CourseIndex, g_HoleIndex)) {
        sHoleContestImage = AllocMemory(OpenEncodedAsset(ASSET_LONGEST_DRIVE_IMAGE, &asset));
        ReadEncodedAsset(sHoleContestImage, &asset);
        sLoadedHoleIntroImages |= HOLE_INTRO_CONTEST_IMAGE_LOADED;
    } else if (g_GameState.betRates[BET_NEAREST_PIN] != UNSET_BET_RATE && IsParThree(g_CourseIndex, g_HoleIndex)) {
        sHoleContestImage = AllocMemory(OpenEncodedAsset(ASSET_NEAREST_PIN_IMAGE, &asset));
        ReadEncodedAsset(sHoleContestImage, &asset);
        sLoadedHoleIntroImages |= HOLE_INTRO_CONTEST_IMAGE_LOADED;
    }
    if (g_GameMode == GAME_MODE_MATCH_GAME || g_GameMode == GAME_MODE_GET_CHARACTER) {
        if (D_801B60A0 != 0) {
            sMatchStandingImage = AllocMemory(OpenEncodedAsset(ASSET_UNK8FF_IMAGE, &asset));
            ReadEncodedAsset(sMatchStandingImage, &asset);
            sLoadedHoleIntroImages |= HOLE_INTRO_MATCH_IMAGE_LOADED;
        } else {
            standing = GetMatchStanding(0, 1);
            if (standing == MATCH_STANDING_PLAYER_DORMIE || standing == MATCH_STANDING_OPPONENT_DORMIE) {
                sMatchStandingImage = AllocMemory(OpenEncodedAsset(ASSET_DORMIE_IMAGE, &asset));
                ReadEncodedAsset(sMatchStandingImage, &asset);
                sLoadedHoleIntroImages |= HOLE_INTRO_MATCH_IMAGE_LOADED;
            }
        }
    }
}

/* Frees the images that LoadHoleIntroImages loaded */
void FreeHoleIntroImages(void) {
    if (sLoadedHoleIntroImages & HOLE_INTRO_CONTEST_IMAGE_LOADED) {
        FreeMemory(&sHoleContestImage);
    }
    if (sLoadedHoleIntroImages & HOLE_INTRO_MATCH_IMAGE_LOADED) {
        FreeMemory(&sMatchStandingImage);
    }
    sLoadedHoleIntroImages = 0;
}

/*
 * Draws the images that LoadHoleIntroImages loaded, a strip at a time, while
 * the hole's intro text is shown. Ring shot draws its own intro instead.
 *
 * Doesn't match: the match standing image's loop keeps different constants
 * in registers.
 */
#ifdef NON_MATCHING
void DrawHoleIntroImages(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
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
            gDPSetRenderMode(gfx++, HOLE_INTRO_IMAGE_RENDER_MODE, HOLE_INTRO_IMAGE_RENDER_MODE2);
            gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            if ((g_GameState.betRates[BET_LONGEST_DRIVE] > 0 && IsMarkedParFive(GetCourseRecordIndex(), g_HoleIndex)) ||
                (g_GameState.betRates[BET_NEAREST_PIN] > 0 && IsParThree(GetCourseRecordIndex(), g_HoleIndex))) {
                gDPLoadTLUT_pal256(gfx++, (u32)sHoleContestImage + IMAGE_HEADER_SIZE & ~7);
                for (strip = 0; strip != CONTEST_IMAGE_STRIP_COUNT; strip++) {
                    gDPLoadTextureBlock(gfx++,
                                        (u32)sHoleContestImage + LARGE_PALETTED_TEXELS_OFFSET +
                                                strip * (CONTEST_IMAGE_WIDTH * CONTEST_IMAGE_STRIP_HEIGHT) & ~7,
                                        G_IM_FMT_CI, G_IM_SIZ_8b, CONTEST_IMAGE_WIDTH, CONTEST_IMAGE_STRIP_HEIGHT, 0,
                                        G_TX_MIRROR, G_TX_MIRROR, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
                    gDPPipeSync(gfx++);
                    gSPScisTextureRectangle(gfx++, CONTEST_IMAGE_X << 2,
                                            (CONTEST_IMAGE_Y + strip * CONTEST_IMAGE_STRIP_HEIGHT) << 2,
                                            (CONTEST_IMAGE_X + CONTEST_IMAGE_WIDTH) << 2,
                                            (CONTEST_IMAGE_Y + (strip + 1) * CONTEST_IMAGE_STRIP_HEIGHT) << 2,
                                            G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
                    gDPPipeSync(gfx++);
                }
            }
            if ((g_GameMode == GAME_MODE_MATCH_GAME || g_GameMode == GAME_MODE_GET_CHARACTER) &&
                (sLoadedHoleIntroImages & HOLE_INTRO_MATCH_IMAGE_LOADED)) {
                gDPLoadTLUT_pal256(gfx++, (u32)sMatchStandingImage + IMAGE_HEADER_SIZE & ~7);
                for (strip = 0; strip != MATCH_IMAGE_STRIP_COUNT; strip++) {
                    gDPLoadTextureTile(gfx++,
                                       (u32)sMatchStandingImage + LARGE_PALETTED_TEXELS_OFFSET +
                                               strip * (MATCH_IMAGE_WIDTH * MATCH_IMAGE_STRIP_HEIGHT) & ~7,
                                       G_IM_FMT_CI, G_IM_SIZ_8b, MATCH_IMAGE_WIDTH, MATCH_IMAGE_STRIP_HEIGHT, 0, 0,
                                       MATCH_IMAGE_WIDTH - 1, MATCH_IMAGE_STRIP_HEIGHT - 1, 0, G_TX_MIRROR, G_TX_MIRROR,
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
void DrawHoleIntroText(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
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
        } else if (g_GameMode == GAME_MODE_UNK3) {
            if (g_IsModuleAlive[MODULE_UNK21]) {
                func_80242D40(&gfx);
            }
        } else if (g_GameMode == GAME_MODE_UNK7 && D_801B60B9 == 0) {
            hundredths = (s32)(g_StopwatchFrameCount * ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) % HUNDREDTHS_PER_SECOND;
            totalSeconds = g_StopwatchFrameCount / FRAMES_PER_SECOND;
            seconds = totalSeconds % SECONDS_PER_MINUTE;
            minutes = totalSeconds / SECONDS_PER_MINUTE;
            SetUpGradientTextDrawing(&gfx, 0);
            sprintf(g_PrintBuffer, D_800D1C84, minutes, seconds, hundredths);
            DrawLargeTextWithShadow(&gfx, STOPWATCH_X, STOPWATCH_Y, g_PrintBuffer, STOPWATCH_ENV_COLOR,
                                    STOPWATCH_PRIM_COLOR, TEXT_FLAG_FIXED_WIDTH);
        } else {
            SetUpGradientTextDrawing(&gfx, 0);
            gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x00, 0x00, 0xFF);
            gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0x00, 0xFF);
            sprintf(g_PrintBuffer, "%s", g_CourseRecords[GetCourseRecordIndex()].name);
            sprintf(g_PrintBuffer, "%s", g_CourseRecords[GetCourseRecordIndex()].name);
            DrawLargeTextWithShadow(&gfx, HOLE_TITLE_X, COURSE_NAME_Y, g_PrintBuffer, g_CourseNameEnvColors[g_CourseIndex],
                                    g_CourseNamePrimColors[g_CourseIndex], TEXT_FLAG_CENTERED);
            sprintf(g_PrintBuffer, "Hole %d", g_HoleIndex + 1);
            DrawLargeTextWithShadow(&gfx, HOLE_TITLE_X, HOLE_NUMBER_Y, g_PrintBuffer, g_CourseNameEnvColors[g_CourseIndex],
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
