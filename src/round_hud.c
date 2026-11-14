#include "round_hud.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "bets.h"
#include "billboard.h"
#include "camera.h"
#include "character.h"
#include "club.h"
#include "club_panel.h"
#include "club_slots.h"
#include "compiler.h"
#include "controller.h"
#include "course.h"
#include "course_select.h"
#include "course_space.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "curve.h"
#include "debug_text.h"
#include "effects.h"
#include "fireworks.h"
#include "frame_time.h"
#include "game.h"
#include "game_flags.h"
#include "game_state.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "golf_balls.h"
#include "heap.h"
#include "hole_conditions.h"
#include "hole_display.h"
#include "hole_objects.h"
#include "language.h"
#include "main.h"
#include "math_util.h"
#include "movie.h"
#include "overlay_manager.h"
#include "pause_menu.h"
#include "pin.h"
#include "play.h"
#include "player.h"
#include "round_events.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "screen.h"
#include "screen_layers.h"
#include "shot_camera.h"
#include "shot_effects.h"
#include "sound.h"
#include "surface.h"
#include "terrain.h"
#include "text.h"
#include "turn.h"

enum SceneryState {
    SCENERY_UNLOADED,
    SCENERY_BILLBOARDS_READY,
    SCENERY_LOADED
};

typedef s32 SceneryState;

#define BILLBOARD_HEAP_SIZE 0x7118

#define SCENERY_BUFFER_A_SIZE 0x128
#define LIE_IMAGE_SIZE 0x1208

#define ASSET_PIN_FLAG_IMAGES 0x541
#define ASSET_WIND_ICON_IMAGE 0x5A4
#define ASSET_LEADER_LINE_IMAGE 0x972
#define ASSET_BACKDROP_PATTERN_IMAGE 0x93C
#define ASSET_SHOT_PREVIEW_DASH_IMAGE 0x4DB
#define ASSET_BLENDED_TEXTURE_STILL_LAYER 0x4D9
#define ASSET_LAKITU_ICON 0x648
#define ASSET_JUMP_LABEL 0x563
#define ASSET_MODE_LABEL 0x564
#define ASSET_CUP_LABEL 0x562
#define ASSET_SHOT_OFFSET_MARKER_IMAGE 0x57C
#define ASSET_WIND_SPEED_FONT 0x97B
#define ASSET_ROUND_IMAGE_725 0x725
#define ASSET_ROUND_IMAGE_568 0x568
#define ASSET_ROUND_IMAGE_56B 0x56B
#define ASSET_ROUND_IMAGE_56A 0x56A
#define ASSET_ROUND_IMAGE_569 0x569
#define ASSET_ROUND_IMAGE_723 0x723
#define ASSET_DISTANCE_SIGN_IMAGE 0x724

#define ASSET_PAR_THREE_IMAGE 0x565
#define ASSET_PAR_FOUR_IMAGE 0x566
#define ASSET_PAR_FIVE_IMAGE 0x567

#define HOLE_IMAGE_WIDTH 48
#define HOLE_IMAGE_HEIGHT 42

#define HOLE_FLAG_COLUMN_COUNT 6
#define HOLE_FLAG_ROW_COUNT 5
#define HOLE_FLAG_VERTEX_COUNT (HOLE_FLAG_COLUMN_COUNT * HOLE_FLAG_ROW_COUNT)
#define HOLE_FLAG_SIDE_COUNT 2
#define HOLE_FLAG_CELL_SIZE 160
#define HOLE_FLAG_TEXEL_COLUMN_STEP 0x266
#define HOLE_FLAG_TEXEL_ROW_STEP 0x2A0
#define HOLE_FLAG_BACK_TEXEL_START 0xC00
#define HOLE_FLAG_START_SHADE 0xB7
#define HOLE_FLAG_VERTEX(row, column) \
    f_HoleFlagVertices[0][g_MatrixBufferIndex][((row) * HOLE_FLAG_COLUMN_COUNT) + (column)]
#define HOLE_FLAG_SHADE(row, column) f_HoleFlagShades[((row) * HOLE_FLAG_COLUMN_COUNT) + (column)]

#define HOLE_FLAG_SKEW 0.0f
#define HOLE_FLAG_ROW_SKEW_SCALE 5.0f
#define HOLE_FLAG_WIND_SPEED_MIN 1.5f
#define HOLE_FLAG_GALE_WIND_SPEED 9.0f
#define HOLE_FLAG_CALM_SPREAD 0.01f
#define HOLE_FLAG_SPREAD_STEP 0.1f
#define HOLE_FLAG_SPREAD_STEP_COUNT 10.0f
#define HOLE_FLAG_SWING_DEGREES_PER_SPEED 2.0f
#define HOLE_FLAG_SWING_DEGREES_MIN 2.0f
#define HOLE_FLAG_SWING_JITTER_RANGE 16
#define HOLE_FLAG_ROW_JITTER_PER_SPEED 4.0f
#define HOLE_FLAG_BULGE_PER_SPEED 0.03f
#define HOLE_FLAG_BULGE_MIN 3.5f
#define HOLE_FLAG_BULGE_SCALE 0.1f
#define HOLE_FLAG_BULGE_COLUMN_BASE 8
#define HOLE_FLAG_BULGE_ROW_BASE 10
#define HOLE_FLAG_BULGE_ROW_SCALE 10.0f
#define HOLE_FLAG_FIRST_PULLED_COLUMN 2
#define HOLE_FLAG_DEPTH_PER_PULL 4

#define SCREEN_ORTHO_NEAR 1.0f
#define SCREEN_ORTHO_FAR 1000.0f
#define SCREEN_EFFECT_FIELD_OF_VIEW 45.0f
#define SCREEN_EFFECT_NEAR_PLANE 10.0f
#define SCREEN_EFFECT_FAR_PLANE 8000.0f
#define NARROW_FIELD_OF_VIEW 10.0f
#define NARROW_FAR_PLANE 30000.0f

#define ARROW_COUNT 2
#define ARROW_VERTEX_COUNT 14
#define ARROW_UNITS_PER_TEXEL_ROW 10
#define ARROW_CENTER_TEXEL_ROW 16
#define TEXEL_COORDINATE_SHIFT 5

typedef struct ArrowShape {
    Vtx body[ARROW_VERTEX_COUNT];
    Vtx outline[ARROW_VERTEX_COUNT];
} ArrowShape;

#define SHOT_OFFSET_MARKER_COUNT 5
#define SHOT_OFFSET_RADIUS_MAX 56.0f
#define SHOT_OFFSET_UNITS_PER_PIXEL 4

#define SHOT_OFFSET_MARKER_SIZE 16
#define SHOT_OFFSET_MARKER_FRAME_COUNT 11
#define SHOT_OFFSET_MARKER_FRAME_SIZE (SHOT_OFFSET_MARKER_SIZE * SHOT_OFFSET_MARKER_SIZE / 2)
#define SHOT_OFFSET_MARKER_STILL_FRAME SHOT_OFFSET_MARKER_FRAME_COUNT
#define SHOT_OFFSET_MARKER_X 264
#define SHOT_OFFSET_MARKER_Y 174

#define ASSET_LONGEST_DRIVE_IMAGE 0x554
#define ASSET_NEAREST_PIN_IMAGE 0x555
#define ASSET_DORMIE_IMAGE 0x8FC
#define ASSET_SUDDEN_DEATH_IMAGE 0x8FF

#define HOLE_INTRO_CONTEST_IMAGE_LOADED (1 << 0)
#define HOLE_INTRO_MATCH_IMAGE_LOADED (1 << 1)
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

#define BALL_TRAIL_LENGTH 128

#define WIND_SPEED_PER_ARROW_SCROLL_STEP 0x2000
#define ARROW_TEXTURE_PERIOD 0x2000
#define ARROW_TEXTURE_COORDINATE_LIMIT 0x4000

#define GAME_FLAG_ADJUST_HUD_OFFSET 0x6E
#define HUD_OFFSET_CONTROLLER 1

#define COLOR_TEST_CONTROLLER 1
#define COLOR_TEST_COLOR_COUNT 2
#define COLOR_TEST_CHANNEL_COUNT 3
#define COLOR_TEST_PRIM 0
#define COLOR_TEST_ENV 1
#define COLOR_TEST_STEP 4
#define COLOR_TEST_TEXT_COLUMN 20
#define COLOR_TEST_TEXT_ROW 5
#define COLOR_TEST_COLUMN_WIDTH 5
extern const char f_ColorTestSampleText[];
extern const char f_BlankText[];

#define HUD_OFFSET_TEXT_COLUMN 20
#define HUD_OFFSET_TEXT_ROW 6

#define LANDING_WAIT_FRAME_COUNT 40
#define LANDING_WAIT_LONG_FRAME_COUNT 60

#define LANDING_INSET_CENTER_X (SCREEN_WIDTH / 2)
#define LANDING_INSET_CENTER_Y 160
#define LANDING_INSET_WIDTH 64
#define LANDING_INSET_HEIGHT 56
#define MOVIE_INSET_CENTER_X (SCREEN_WIDTH / 2)
#define MOVIE_INSET_CENTER_Y 128
#define MOVIE_INSET_SIZE 128
#define LANDING_INSET_SOURCE_CODE 77
#define LANDING_INSET_SIZE_CODE 77
#define MOVIE_INSET_SIZE_CODE 101

enum LandingGlyphGroup {
    LANDING_GLYPH_GROUP_FIRST,
    LANDING_GLYPH_GROUP_SECOND
};

#define LANDING_REACTION_FRAME_COUNT 30
#define LANDING_NAME_GLYPH_Y 8
#define LANDING_DISTANCE_GLYPH_Y (-8)
#define LANDING_NAME_Y 96
#define LANDING_DISTANCE_Y 112
#define HUNDREDTHS_PER_UNIT 100
#define SHOWS_METERS FALSE
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

#define RING_TEXTURE_OUTER_RADIUS 62

#define HUD_MARGIN 8

#define LETTERBOX_GROWTH_STEP 2
#define LETTERBOX_HEIGHT_MAX 16

#define HOLE_IMAGE_MODEL_X (-240.0f)
#define HOLE_IMAGE_MODEL_Y 180.0f
#define HOLE_IMAGE_MODEL_Z (-500.0f)
#define HOLE_IMAGE_MODEL_SCALE 0.1f

#define SHOT_EFFECT_TIME_OFF (-1.0f)
#define LONG_SHOT_SOUND_CHANNEL 14
#define LONG_SHOT_SOUND_VOLUME 0x40
#define SPARK_TRAIL_SOUND_VOLUME 0x60
#define LONG_SHOT_SOUND_REVERB 30

#define DISTANCE_METERS_S 80
#define DISTANCE_YARDS_S 88
#define DISTANCE_FEET_S 96
#define DISTANCE_FEET_WIDTH 16
#define DISTANCE_POINT_S 112
#define DISTANCE_POINT_WIDTH 4

u8 *g_ClubPanelImage;
ImageAsset *g_DistanceSignImage;
ImageAsset *g_LeaderLineImage;
ImageAsset *g_PinFlagImages;
ImageAsset *g_PlayerTagImages;
u8 *g_RingTexels;
RoundHud g_RoundHud;
u8 *g_SharedEffectImages;
ImageAsset *g_ShotPreviewDashImage;

FILE_STATIC Mtx f_UnusedNarrowProjection;

#define ANIMATED_ICON_LABEL_COUNT 3

#define WIND_INDICATOR_DEPTH (-300.0f)
#define WIND_INDICATOR_TILT 0.3f
#define WIND_INDICATOR_SCALE (-0.07f)
#define WIND_INDICATOR_FACING_DEGREES 180.0

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
#define WIND_ARROWS_WIDE_GAP 56
#define WIND_ARROWS_NARROW_GAP 44
#define WIND_FONT_WIDTH 144
#define WIND_FONT_HEIGHT 14
#define WIND_DIGIT_WIDTH 12
#define WIND_METERS_GLYPH_S 119
#define WIND_METERS_GLYPH_WIDTH 14
#define WIND_TEXT_RAISE 1
#define WIND_TEXT_RENDER_MODE \
    (IM_RD | CLR_ON_CVG | CVG_X_ALPHA | GBL_c1(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM))
#define WIND_TEXT_RENDER_MODE2 \
    (IM_RD | CLR_ON_CVG | CVG_X_ALPHA | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM))
#define WIND_TEXT_PRIM_RED 0x3F
#define WIND_TEXT_PRIM_GREEN 0x3F
#define WIND_TEXT_PRIM_BLUE 0xFF
#define WIND_TEXT_ENV_RED 0x00
#define WIND_TEXT_ENV_GREEN 0x20
#define WIND_TEXT_ENV_BLUE 0x7F

void DrawWindSpeed(Gfx **displayListHead, f32 x, f32 y);

#define LEVEL_HEIGHT 0.01f
#define TEE_GROUND_PANEL_OFFSET_Y (-3)
#define SHOT_PANEL_START_OFFSET_X 64

#define STOPWATCH_X 168
#define STOPWATCH_Y 176
#define STOPWATCH_ENV_COLOR 0xFFFF0000
#define STOPWATCH_PRIM_COLOR 0x30FF0000
#define HOLE_TITLE_X (SCREEN_WIDTH / 2)
#define COURSE_NAME_Y 32
#define HOLE_NUMBER_Y 48

#define NO_IMAGE_ASSET (-1)
#define NO_LANDING_REACTION_FRAME (-1)
#define INITIAL_HUD_OFFSET_Y (-2)
#define GREEN_GRID_ALPHA 0x7F
#define LONG_SHOT_GROUP_COUNT 4
#define SPARK_TRAIL_FRAME_COUNT 6
#define ANIMATED_ICON_FRAME_COUNT 8

static void *f_SceneryBillboardHeap = NULL;
static s32 f_ClubNameImageIndex = 0;
static s32 f_UnusedHudInitWord = 0;
AssetId g_CharacterFirstFaceAssets[CHARACTER_COUNT] = {
    0x7BC,
    0x74C,
    0x7A0,
    0x73E,
    0x784,
    0x7AE,
    0x7CA,
    0x7D8,
    0x768,
    0x792,
    0x7E6,
    0x75A,
    0x776,
    0x984,
    0x7F4,
    0x802,
    0x810,
    0x81E,
};
SignedAssetId g_CharacterFirstPaletteFiles[CHARACTER_COUNT] = {
    0x7B8,
    0x748,
    0x79C,
    0x73A,
    0x780,
    0x7AA,
    0x7C6,
    0x7D4,
    0x764,
    0x78E,
    0x7E2,
    0x756,
    0x772,
    0x980,
    0x7F0,
    0x7FE,
    0x80C,
    0x81A,
};
static s32 f_ShotPanelOffsetY = 0;
s32 g_ShotPanelOffsetX = 0;
static AssetId f_HoleImageAssets[COURSE_RECORD_COUNT] =
    {0x420, 0x432, 0x444, 0x468, 0x456, 0x47A, 0x48C, 0x49E, 0x8F0};
s32 g_MatrixBufferIndex = 0;
static ArrowShape f_Arrows[ARROW_COUNT] = {
    {
        {
            {{{0, -160, 40}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0x80}}},
            {{{-160, 0, 40}, 0, {0, 0x200}, {0x36, 0x36, 0x36, 0x80}}},
            {{{160, 0, 40}, 0, {0, 0x200}, {0x36, 0x36, 0x36, 0x80}}},
            {{{-60, 0, 40}, 0, {0, 0x200}, {0x20, 0x20, 0x20, 0x80}}},
            {{{-60, 160, 40}, 0, {0, 0x400}, {0x2F, 0x2F, 0x2F, 0x80}}},
            {{{60, 160, 40}, 0, {0, 0x400}, {0x20, 0x20, 0x20, 0x80}}},
            {{{60, 0, 40}, 0, {0, 0x200}, {0x7F, 0x7F, 0x7F, 0x80}}},
            {{{0, -160, -40}, 0, {0, 0}, {0x10, 0x10, 0x10, 0x80}}},
            {{{-160, 0, -40}, 0, {0, 0x200}, {0x00, 0x00, 0x00, 0x80}}},
            {{{160, 0, -40}, 0, {0, 0x200}, {0x40, 0x40, 0x40, 0x80}}},
            {{{-60, 0, -40}, 0, {0, 0x200}, {0x10, 0x10, 0x10, 0x80}}},
            {{{-60, 160, -40}, 0, {0, 0x400}, {0x00, 0x00, 0x00, 0x80}}},
            {{{60, 160, -40}, 0, {0, 0x400}, {0x48, 0x48, 0x48, 0x80}}},
            {{{60, 0, -40}, 0, {0, 0x200}, {0x20, 0x20, 0x20, 0x80}}},
        },
        {
            {{{0, -188, 60}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0x80}}},
            {{{-188, 20, 60}, 0, {0, 0x200}, {0x36, 0x36, 0x36, 0x80}}},
            {{{188, 20, 60}, 0, {0, 0x200}, {0x36, 0x36, 0x36, 0x80}}},
            {{{-80, 20, 60}, 0, {0, 0x200}, {0x20, 0x20, 0x20, 0x80}}},
            {{{-80, 180, 60}, 0, {0, 0x400}, {0x2F, 0x2F, 0x2F, 0x80}}},
            {{{80, 180, 60}, 0, {0, 0x400}, {0x20, 0x20, 0x20, 0x80}}},
            {{{80, 20, 60}, 0, {0, 0x200}, {0x7F, 0x7F, 0x7F, 0x80}}},
            {{{0, -180, -60}, 0, {0, 0}, {0x10, 0x10, 0x10, 0x80}}},
            {{{-188, 20, -60}, 0, {0, 0x200}, {0x00, 0x00, 0x00, 0x80}}},
            {{{188, 20, -60}, 0, {0, 0x200}, {0x40, 0x40, 0x40, 0x80}}},
            {{{-80, 20, -60}, 0, {0, 0x200}, {0x10, 0x10, 0x10, 0x80}}},
            {{{-80, 180, -60}, 0, {0, 0x400}, {0x00, 0x00, 0x00, 0x80}}},
            {{{80, 180, -60}, 0, {0, 0x400}, {0x48, 0x48, 0x48, 0x80}}},
            {{{80, 20, -60}, 0, {0, 0x200}, {0x20, 0x20, 0x20, 0x80}}},
        },
    },
    {
        {
            {{{0, -160, 40}, 0, {0, 0x40}, {0x7F, 0x7F, 0x7F, 0x80}}},
            {{{-160, 0, 40}, 0, {0, 0x240}, {0x36, 0x36, 0x36, 0x80}}},
            {{{160, 0, 40}, 0, {0, 0x240}, {0x36, 0x36, 0x36, 0x80}}},
            {{{-60, 0, 40}, 0, {0, 0x240}, {0x20, 0x20, 0x20, 0x80}}},
            {{{-60, 160, 40}, 0, {0, 0x440}, {0x2F, 0x2F, 0x2F, 0x80}}},
            {{{60, 160, 40}, 0, {0, 0x440}, {0x20, 0x20, 0x20, 0x80}}},
            {{{60, 0, 40}, 0, {0, 0x240}, {0x7F, 0x7F, 0x7F, 0x80}}},
            {{{0, -160, -40}, 0, {0, 0x40}, {0x10, 0x10, 0x10, 0x80}}},
            {{{-160, 0, -40}, 0, {0, 0x240}, {0x00, 0x00, 0x00, 0x80}}},
            {{{160, 0, -40}, 0, {0, 0x240}, {0x40, 0x40, 0x40, 0x80}}},
            {{{-60, 0, -40}, 0, {0, 0x240}, {0x10, 0x10, 0x10, 0x80}}},
            {{{-60, 160, -40}, 0, {0, 0x440}, {0x00, 0x00, 0x00, 0x80}}},
            {{{60, 160, -40}, 0, {0, 0x440}, {0x48, 0x48, 0x48, 0x80}}},
            {{{60, 0, -40}, 0, {0, 0x240}, {0x20, 0x20, 0x20, 0x80}}},
        },
        {
            {{{0, -188, 60}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0x80}}},
            {{{-188, 20, 60}, 0, {0, 0x200}, {0x36, 0x36, 0x36, 0x80}}},
            {{{188, 20, 60}, 0, {0, 0x200}, {0x36, 0x36, 0x36, 0x80}}},
            {{{-80, 20, 60}, 0, {0, 0x200}, {0x20, 0x20, 0x20, 0x80}}},
            {{{-80, 180, 60}, 0, {0, 0x400}, {0x2F, 0x2F, 0x2F, 0x80}}},
            {{{80, 180, 60}, 0, {0, 0x400}, {0x20, 0x20, 0x20, 0x80}}},
            {{{80, 20, 60}, 0, {0, 0x200}, {0x7F, 0x7F, 0x7F, 0x80}}},
            {{{0, -180, -60}, 0, {0, 0}, {0x10, 0x10, 0x10, 0x80}}},
            {{{-188, 20, -60}, 0, {0, 0x200}, {0x00, 0x00, 0x00, 0x80}}},
            {{{188, 20, -60}, 0, {0, 0x200}, {0x40, 0x40, 0x40, 0x80}}},
            {{{-80, 20, -60}, 0, {0, 0x200}, {0x10, 0x10, 0x10, 0x80}}},
            {{{-80, 180, -60}, 0, {0, 0x400}, {0x00, 0x00, 0x00, 0x80}}},
            {{{80, 180, -60}, 0, {0, 0x400}, {0x48, 0x48, 0x48, 0x80}}},
            {{{80, 20, -60}, 0, {0, 0x200}, {0x20, 0x20, 0x20, 0x80}}},
        },
    },
};
static Vtx f_ClubHeadIconVertices[QUAD_VERTEX_COUNT] = {
    {{{-16, 16, 0}, 0, {-0x1, -0x1}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-16, -16, 0}, 0, {-0x1, 0x7FF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{16, -16, 0}, 0, {0x7FF, 0x7FF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{16, 16, 0}, 0, {0x7FF, -0x1}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static Vtx f_LieIconVertices[2 * QUAD_VERTEX_COUNT] = {
    {{{-24, 24, 0}, 0, {-0x1, -0x1}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-24, 0, 0}, 0, {-0x1, 0x5FF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{24, 0, 0}, 0, {0xBFF, 0x5FF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{24, 24, 0}, 0, {0xBFF, -0x1}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-24, 0, 0}, 0, {-0x1, -0x1}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-24, -24, 0}, 0, {-0x1, 0x5FF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{24, -24, 0}, 0, {0xBFF, 0x5FF}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{24, 0, 0}, 0, {0xBFF, -0x1}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static Gfx f_UnusedArrowTriangles[] UNUSED = {
    gsSP2Triangles(10, 13, 12, 0, 10, 12, 11, 0),
    gsSP2Triangles(3, 1, 8, 0, 3, 8, 10, 0),
    gsSP2Triangles(13, 9, 2, 0, 13, 2, 6, 0),
    gsSP2Triangles(4, 3, 10, 0, 4, 10, 11, 0),
    gsSP2Triangles(12, 13, 6, 0, 12, 6, 5, 0),
    gsSP2Triangles(5, 4, 11, 0, 5, 11, 12, 0),
    gsSP2Triangles(6, 3, 4, 0, 6, 4, 5, 0),
    gsSP2Triangles(1, 0, 7, 0, 1, 7, 8, 0),
    gsSP2Triangles(9, 7, 0, 0, 9, 0, 2, 0),
    gsSP2Triangles(0, 1, 2, 0, 7, 9, 8, 0),
    gsSPEndDisplayList(),
    gsSP2Triangles(24, 27, 26, 0, 24, 26, 25, 0),
    gsSP2Triangles(17, 15, 22, 0, 17, 22, 24, 0),
    gsSP2Triangles(27, 23, 16, 0, 27, 16, 20, 0),
    gsSP2Triangles(18, 17, 24, 0, 18, 24, 25, 0),
    gsSP2Triangles(26, 27, 20, 0, 26, 20, 19, 0),
    gsSP2Triangles(19, 18, 25, 0, 19, 25, 26, 0),
    gsSP2Triangles(20, 17, 18, 0, 20, 18, 19, 0),
    gsSP2Triangles(15, 14, 21, 0, 15, 21, 22, 0),
    gsSP2Triangles(23, 21, 14, 0, 23, 14, 16, 0),
    gsSP2Triangles(14, 15, 16, 0, 21, 23, 22, 0),
    gsSPEndDisplayList(),
};
static WideAssetId f_ClubHeadImageAsset = NO_IMAGE_ASSET;
static WideAssetId f_LieImageAsset = NO_IMAGE_ASSET;
static SceneryState f_SceneryState = SCENERY_UNLOADED;
static Boolean f_IsCourseSelectLoaded = FALSE;
static Gfx f_UnusedNoOpDisplayList[] UNUSED = {
    gsDPNoOp(),
    gsSPEndDisplayList(),
};
static u8 f_InsetFrameTexture[] = {
#include "assets/round_hud/inset_frame_texture.i4.inc.c"
};
static LandingReaction f_LandingReaction = LANDING_REACTION_NONE;
static s32 f_LandingReactionFrame = NO_LANDING_REACTION_FRAME;
static Vtx f_LandingPanelVertices[4] = {
    {{{-72, 20, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{-72, -20, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{72, -20, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{72, 20, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
static s32 f_GreenGridAlpha = GREEN_GRID_ALPHA;
static Vtx f_BallTrailVertices[4] = {
    {{{0, 0, -100}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x7F}}},
    {{{0, -1, -100}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x7F}}},
    {{{48, -1, -100}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x7F}}},
    {{{48, 0, -100}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x7F}}},
};
static s32 f_UnusedWordBeforeGridScroll UNUSED = 0;
static f32 f_GreenGridScroll = 0.0f;
static s32 f_BallTrailHead = 0;
BallTrailState g_BallTrailState = BALL_TRAIL_OFF;
static Radians f_WindIconPulseAngle = 0.0f;
static Lights1 f_WindArrowLights = gdSPDefLights1(0x80, 0x80, 0x80, 0xFF, 0xFF, 0xFF, 80, -80, 73);
static s32 f_HudOffsetY = INITIAL_HUD_OFFSET_Y;
static AssetId f_ClubNameAssets[CLUB_SET_SIZE] = {
    0x4F1,
    0x4F2,
    0x4F3,
    0x4F4,
    0x4F5,
    0x4F6,
    0x4F7,
    0x4F8,
    0x4F9,
    0x4FA,
    0x4FB,
    0x4FC,
    0x4FD,
    0x4FE,
};
static Vtx f_SparkTrailVertices[16] = {
    {{{-32, -512, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{-32, 0, 0}, 0, {0, 0x2000}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{32, 0, 0}, 0, {0xC00, 0x2000}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{32, -512, 0}, 0, {0xC00, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{-256, 32, 0}, 0, {0xC00, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{-256, -32, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{0, -32, 0}, 0, {0, 0x2000}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{0, 32, 0}, 0, {0xC00, 0x2000}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{-32, 512, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{-32, 0, 0}, 0, {0, 0x2000}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{32, 0, 0}, 0, {0xC00, 0x2000}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{32, 512, 0}, 0, {0xC00, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{512, 32, 0}, 0, {0xC00, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{512, -32, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{0, -32, 0}, 0, {0, 0x2000}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{0, 32, 0}, 0, {0xC00, 0x2000}, {0xFF, 0xFF, 0xFF, 0x80}}},
};
static Vtx f_LongShotVertices[LONG_SHOT_GROUP_COUNT * 8] = {
    {{{-3, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{-2, -32, 0}, 0, {0, 0}, {0xFF, 0xFF, 0x00, 0x60}}},
    {{{2, -32, 0}, 0, {0, 0}, {0xFF, 0xFF, 0x00, 0x60}}},
    {{{3, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{-1, -96, 0}, 0, {0, 0}, {0x7F, 0xFF, 0x00, 0x40}}},
    {{{1, -96, 0}, 0, {0, 0}, {0x7F, 0xFF, 0x00, 0x40}}},
    {{{0, -192, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0x20}}},
    {{{0, -192, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0x20}}},
    {{{3, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{2, 32, 0}, 0, {0, 0}, {0xFF, 0xFF, 0x00, 0x60}}},
    {{{-2, 32, 0}, 0, {0, 0}, {0xFF, 0xFF, 0x00, 0x60}}},
    {{{-3, 0, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{1, 96, 0}, 0, {0, 0}, {0x7F, 0xFF, 0x00, 0x40}}},
    {{{-1, 96, 0}, 0, {0, 0}, {0x7F, 0xFF, 0x00, 0x40}}},
    {{{0, 192, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0x20}}},
    {{{0, 192, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0x20}}},
    {{{0, -3, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{32, -2, 0}, 0, {0, 0}, {0xFF, 0xFF, 0x00, 0x60}}},
    {{{32, 2, 0}, 0, {0, 0}, {0xFF, 0xFF, 0x00, 0x60}}},
    {{{0, 3, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{96, -1, 0}, 0, {0, 0}, {0x7F, 0xFF, 0x00, 0x40}}},
    {{{96, 1, 0}, 0, {0, 0}, {0x7F, 0xFF, 0x00, 0x40}}},
    {{{192, 0, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0x20}}},
    {{{192, 0, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0x20}}},
    {{{0, 3, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{-32, 2, 0}, 0, {0, 0}, {0xFF, 0xFF, 0x00, 0x60}}},
    {{{-32, -2, 0}, 0, {0, 0}, {0xFF, 0xFF, 0x00, 0x60}}},
    {{{0, -3, 0}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0x80}}},
    {{{-96, 1, 0}, 0, {0, 0}, {0x7F, 0xFF, 0x00, 0x40}}},
    {{{-96, -1, 0}, 0, {0, 0}, {0x7F, 0xFF, 0x00, 0x40}}},
    {{{-192, 0, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0x20}}},
    {{{-192, 0, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0x20}}},
};
static f32 f_ShotEffectTime = SHOT_EFFECT_TIME_OFF;
static ShotEffect f_ShotEffect = SHOT_EFFECT_LONG_SHOT;
static WideAssetId f_SparkTrailFrameAssets[SPARK_TRAIL_FRAME_COUNT] =
    {0x6A7, 0x6A8, 0x6A9, 0x6AA, 0x6AB, 0x6AC};
static s32 f_SparkTrailFrame = 1;
static s32 f_SparkTrailFrameTime = 0;
static s32 f_SparkTrailBufferIndex = 0;
static s32 f_UnusedWordAfterSparkTrailIndex UNUSED = 0;
static f32 f_HoleFlagWavePhase = 0.0f;
static s32 f_ShotOffsetMarkerFrame = 0;
static WideAssetId f_AnimatedIconFrameAssets[ANIMATED_ICON_FRAME_COUNT] =
    {0x648, 0x649, 0x64A, 0x64B, 0x64C, 0x64B, 0x64A, 0x649};
static s32 f_AnimatedIconFrameTime = 0;
static s32 f_AnimatedIconFrame = 0;
static s32 f_UnusedIconFrameOrder[4] UNUSED = {0, 1, 2, 1};
static s32 f_AnimatedIconImageIndex = 0;
static f32 f_AnimatedIconX = 0.0f;
static f32 f_AnimatedIconY = 0.0f;
static s32 f_TestColors[COLOR_TEST_COLOR_COUNT][COLOR_TEST_CHANNEL_COUNT] = {
    {COLOR_CHANNEL_MAX, COLOR_CHANNEL_MAX, COLOR_CHANNEL_MAX},
    {COLOR_CHANNEL_MAX, COLOR_CHANNEL_MAX, COLOR_CHANNEL_MAX},
};
static s32 f_TestTextX = SCREEN_WIDTH / 2;
static s32 f_TestTextY = SCREEN_HEIGHT / 2;
static s32 f_TestChannelIndex = 0;
static s32 f_TestColorIndex = 0;
static Boolean f_IsTestGradientShown = FALSE;
static s32 f_UnusedWordAfterTestGradient UNUSED = 0;
static s32 f_LetterboxHeight = 0;
static s32 f_PinFlagTime = 0;
static s32 f_RingFrame = 0;
static s32 f_RingFrameTime = 0;
static Gfx f_UnusedEmptyHudDisplayList[] UNUSED = {
    gsSPEndDisplayList(),
};
static u16 f_WhiteDistancePalette[] = {
#include "assets/round_hud/white_distance_palette.rgba16.inc.c"
};
static s32 f_LoadedHoleIntroImages = 0;

#define GREEN_GRID_COLUMN_COUNT 8
#define GREEN_GRID_ROW_COUNT_MAX 32
#define GREEN_GRID_POINT_COUNT (GREEN_GRID_ROW_COUNT_MAX * GREEN_GRID_COLUMN_COUNT)

static Mtx f_LandingPanelMatrices[MATRIX_BUFFER_COUNT];
static Mtx f_GreenGridMatrix;
static Mtx f_BallTrailMatrix;
static Mtx f_WindIndicatorMatrices[MATRIX_BUFFER_COUNT];
static Mtx f_ClubHeadIconMatrices[MATRIX_BUFFER_COUNT];
static Mtx f_LieIconMatrices[MATRIX_BUFFER_COUNT];
static Mtx f_HoleImageMatrix;
static Mtx f_ShotEffectMatrices[MATRIX_BUFFER_COUNT];
static Vec3 f_ShotEffectTranslation;
static Vec3 f_ShotEffectScale;
static Vec3 f_ShotEffectRotation;
static s32 f_ShotOffsetMarkerTexelOffset;
static ImageAsset *f_LieImage;

static u8 f_UnusedBytesBeforeClubHead[4] UNUSED;

static ImageAsset *f_ClubHeadImage;
static ImageAsset *f_ClubNameImages[2];
static ImageAsset *f_AnimatedIconLabelImages[ANIMATED_ICON_LABEL_COUNT];
static ImageAsset *f_AnimatedIconImages[2];
static ImageAsset *f_DistanceFont;
static ImageAsset *f_WindSpeedFont;
static ImageAsset *f_RingImages;
static ImageAsset *f_RestLabelImage;
static ImageAsset *f_FlatLabelImage;
static ImageAsset *f_DownLabelImage;
static ImageAsset *f_UnusedRoundImage;
static ImageAsset *f_UpLabelImage;
static ImageAsset *f_ParImage;
static ImageAsset *f_ShotOffsetMarkerImage;
static ImageAsset *f_WindIconImage;
static s32 f_ShotOffsetMarkerXs[SHOT_OFFSET_MARKER_COUNT];
static s32 f_ShotOffsetMarkerYs[SHOT_OFFSET_MARKER_COUNT];
static s32 f_UnusedHudScissorWord;
static ImageAsset *f_HoleImage;
static Vtx f_HoleFlagVertices[HOLE_FLAG_SIDE_COUNT][MATRIX_BUFFER_COUNT][HOLE_FLAG_VERTEX_COUNT];
static s8 f_HoleFlagShades[HOLE_FLAG_VERTEX_COUNT];
static s32 f_GreenGridHeights[GREEN_GRID_POINT_COUNT];
static ScreenPosition f_BallTrailPoints[BALL_TRAIL_LENGTH];
static f32 f_BallTrail[BALL_TRAIL_LENGTH][3];
static ImageAsset *f_HoleContestImage;
static ImageAsset *f_MatchStandingImage;

void PrepareHoleDisplay(void) {
    s32 vertexIndex;

    g_MatrixBufferIndex = 0;
    for (vertexIndex = 0; vertexIndex != ARROW_VERTEX_COUNT; vertexIndex++) {
        f_Arrows[0].body[vertexIndex].v.tc[TEXTURE_AXIS_T] =
            ((f_Arrows[0].body[vertexIndex].v.ob[AXIS_Y] / ARROW_UNITS_PER_TEXEL_ROW) +
                ARROW_CENTER_TEXEL_ROW)
            << TEXEL_COORDINATE_SHIFT;
        f_Arrows[1].body[vertexIndex].v.tc[TEXTURE_AXIS_T] =
            ((f_Arrows[1].body[vertexIndex].v.ob[AXIS_Y] / ARROW_UNITS_PER_TEXEL_ROW) +
                ARROW_CENTER_TEXEL_ROW)
            << TEXEL_COORDINATE_SHIFT;
    }

    BuildPinShadow();
    InitSkyColors();
}

#ifdef AVOID_UB
#    define HoleFlagGridVertices(grid) \
        (f_HoleFlagVertices[(grid) / MATRIX_BUFFER_COUNT][(grid) % MATRIX_BUFFER_COUNT])
#else
#    define HoleFlagGridVertices(grid) (f_HoleFlagVertices[0][grid])
#endif

void InitRoundHud(void) {
    s32 gridIndex;
    s32 row;
    s32 column;

    f_UnusedHudInitWord = 0;
    for (gridIndex = 0; gridIndex != HOLE_FLAG_SIDE_COUNT * MATRIX_BUFFER_COUNT; gridIndex++) {
        Vtx *vertex = HoleFlagGridVertices(gridIndex);

        for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
            for (column = 0; column != HOLE_FLAG_COLUMN_COUNT; column++, vertex++) {
                vertex->v.ob[AXIS_X] = column * HOLE_FLAG_CELL_SIZE;
                vertex->v.ob[AXIS_Y] = -row * HOLE_FLAG_CELL_SIZE;
                vertex->v.ob[AXIS_Z] = 0;
                vertex->v.tc[TEXTURE_AXIS_S] = column * HOLE_FLAG_TEXEL_COLUMN_STEP;
                vertex->v.tc[TEXTURE_AXIS_T] = row * HOLE_FLAG_TEXEL_ROW_STEP;
                vertex->v.cn[0] = HOLE_FLAG_START_SHADE;
                vertex->v.cn[1] = HOLE_FLAG_START_SHADE;
                vertex->v.cn[2] = HOLE_FLAG_START_SHADE;
                vertex->v.cn[3] = HOLE_FLAG_START_SHADE;
            }
        }
    }

    for (gridIndex = MATRIX_BUFFER_COUNT; gridIndex != HOLE_FLAG_SIDE_COUNT * MATRIX_BUFFER_COUNT;
        gridIndex++) {
        Vtx *vertex = HoleFlagGridVertices(gridIndex);

        for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
            for (column = 0; column != HOLE_FLAG_COLUMN_COUNT; column++, vertex++) {
                vertex->v.tc[TEXTURE_AXIS_S] =
                    HOLE_FLAG_BACK_TEXEL_START - (column * HOLE_FLAG_TEXEL_COLUMN_STEP);
            }
        }
    }

    for (row = 0; row != HOLE_FLAG_VERTEX_COUNT; row++) {
        f_HoleFlagShades[row] = HOLE_FLAG_START_SHADE;
    }

    guOrtho(
        &g_ScreenOrthoMatrix,
        -SCREEN_WIDTH / 2,
        SCREEN_WIDTH / 2,
        -SCREEN_HEIGHT / 2,
        SCREEN_HEIGHT / 2,
        SCREEN_ORTHO_NEAR,
        SCREEN_ORTHO_FAR,
        1.0f
    );
    guPerspective(
        &f_UnusedNarrowProjection,
        &g_ScreenEffectPerspectiveNormalization,
        NARROW_FIELD_OF_VIEW,
        SCREEN_ASPECT_RATIO,
        SCREEN_EFFECT_NEAR_PLANE,
        NARROW_FAR_PLANE,
        1.0f
    );
    guPerspective(
        &g_ScreenEffectProjection,
        &g_ScreenEffectPerspectiveNormalization,
        SCREEN_EFFECT_FIELD_OF_VIEW,
        SCREEN_ASPECT_RATIO,
        SCREEN_EFFECT_NEAR_PLANE,
        SCREEN_EFFECT_FAR_PLANE,
        1.0f
    );
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

void LoadPlayerFace(s32 playerIndex, CharacterIndex characterIndex, s32 paletteIndex) {
    EncodedAsset asset;
    AssetFile file;

    OpenEncodedAsset(
        g_CharacterFirstFaceAssets[characterIndex] + CHARACTER_FACE_IMAGE_OFFSET,
        &asset
    );
    ReadEncodedAsset(g_PlayerFaceImages[playerIndex], &asset);
    OpenAssetFile(g_CharacterFirstPaletteFiles[characterIndex] + paletteIndex, &file);
    ReadAssetFile(g_PlayerFacePalettes[playerIndex], GetAssetFileSize(&file), &file);
}

void StopHoleIntro(void) {
    if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
        if (f_IsCourseSelectLoaded == TRUE) {
            f_IsCourseSelectLoaded = FALSE;
            FreeCourseSelect();
        }
    }
    else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
        if (f_IsCourseSelectLoaded == TRUE) {
            f_IsCourseSelectLoaded = FALSE;
            FreeCourseSelect();
        }
    }
    else {
        FreeHoleIntroImages();
    }

    if (f_SceneryState == SCENERY_BILLBOARDS_READY) {
        LoadSceneryImages();
    }
}

void StartHoleIntro(void) {
    EncodedAsset asset;

    StopWeather();
    StartWeather(WEATHER_RAIN);
    if (g_HoleSetup.conditions.isRaining == TRUE) {
        PlaySound(RAIN_SOUND, RAIN_SOUND_CHANNEL, RAIN_SOUND_START_VOLUME);
        g_RainSoundVolume = RAIN_SOUND_START_VOLUME;
    }
    else {
        StopSound(RAIN_SOUND_CHANNEL);
    }

    RevertBillboardManager();
    OpenEncodedAsset(f_HoleImageAssets[GetCourseRecordIndex()] + GetHoleIndexInRecord(), &asset);
    ReadEncodedAsset(f_HoleImage, &asset);
    switch (g_CourseRecords[GetCourseRecordIndex()].holes[GetHoleRecordIndex()].par) {
        case PAR_THREE:
            OpenEncodedAsset(ASSET_PAR_THREE_IMAGE, &asset);
            break;
        case PAR_FOUR:
            OpenEncodedAsset(ASSET_PAR_FOUR_IMAGE, &asset);
            break;
        case PAR_FIVE:
            OpenEncodedAsset(ASSET_PAR_FIVE_IMAGE, &asset);
            break;
    }

    ReadEncodedAsset(f_ParImage, &asset);
    if (f_SceneryState == SCENERY_LOADED) {
        FreeSceneryImages();
    }

    if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
        if (f_IsCourseSelectLoaded == FALSE) {
            f_IsCourseSelectLoaded = TRUE;
            LoadCourseSelect(0);
        }
    }
    else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
        if (f_IsCourseSelectLoaded == FALSE) {
            f_IsCourseSelectLoaded = TRUE;
            LoadCourseSelect(1);
        }
    }
    else {
        LoadHoleIntroImages();
    }
}

void LoadSceneryImages(void) {
    EncodedAsset asset;

    f_SceneryState = SCENERY_LOADED;
    LoadGolfBallImages();
    LoadEffectImages();
    LoadHoleCreatureImages();
    LoadShotEffectImages();
    AllocMovieBuffer();
    LoadClubPanelImages();
    g_PinFlagImages = AllocMemory(OpenEncodedAsset(ASSET_PIN_FLAG_IMAGES, &asset));
    ReadEncodedAsset(g_PinFlagImages, &asset);
    f_WindIconImage = AllocMemory(OpenEncodedAsset(ASSET_WIND_ICON_IMAGE, &asset));
    ReadEncodedAsset(f_WindIconImage, &asset);
    g_PlayerTagImages = AllocMemory(OpenEncodedAsset(ASSET_PLAYER_LABELS, &asset));
    ReadEncodedAsset(g_PlayerTagImages, &asset);
    g_LeaderLineImage = AllocMemory(OpenEncodedAsset(ASSET_LEADER_LINE_IMAGE, &asset));
    ReadEncodedAsset(g_LeaderLineImage, &asset);
    g_BackdropPatternImage = AllocMemory(OpenEncodedAsset(ASSET_BACKDROP_PATTERN_IMAGE, &asset));
    ReadEncodedAsset(g_BackdropPatternImage, &asset);
    g_ShotPreviewDashImage = AllocMemory(OpenEncodedAsset(ASSET_SHOT_PREVIEW_DASH_IMAGE, &asset));
    ReadEncodedAsset(g_ShotPreviewDashImage, &asset);
    g_ShadowTexture = AllocMemory(OpenEncodedAsset(ASSET_SHADOW_TEXTURE, &asset));
    ReadEncodedAsset(g_ShadowTexture, &asset);
    f_ClubHeadImageAsset = NO_IMAGE_ASSET;
    f_ClubHeadImage = AllocMemory(OpenEncodedAsset(ASSET_DRIVER_HEAD_IMAGE, &asset));
    ReadEncodedAsset(f_ClubHeadImage, &asset);
    f_ClubNameImages[0] = AllocMemory(SCENERY_BUFFER_A_SIZE);
    f_ClubNameImages[1] = AllocMemory(SCENERY_BUFFER_A_SIZE);
    f_LieImageAsset = NO_IMAGE_ASSET;
    f_LieImage = AllocMemory(LIE_IMAGE_SIZE);
    f_AnimatedIconImages[0] = AllocMemory(OpenEncodedAsset(ASSET_LAKITU_ICON, &asset));
    ReadEncodedAsset(f_AnimatedIconImages[0], &asset);
    f_AnimatedIconLabelImages[0] = AllocMemory(OpenEncodedAsset(ASSET_JUMP_LABEL, &asset));
    ReadEncodedAsset(f_AnimatedIconLabelImages[0], &asset);
    f_AnimatedIconLabelImages[1] = AllocMemory(OpenEncodedAsset(ASSET_MODE_LABEL, &asset));
    ReadEncodedAsset(f_AnimatedIconLabelImages[1], &asset);
    f_AnimatedIconLabelImages[2] = AllocMemory(OpenEncodedAsset(ASSET_CUP_LABEL, &asset));
    ReadEncodedAsset(f_AnimatedIconLabelImages[2], &asset);
    f_ShotOffsetMarkerImage = AllocMemory(OpenEncodedAsset(ASSET_SHOT_OFFSET_MARKER_IMAGE, &asset));
    ReadEncodedAsset(f_ShotOffsetMarkerImage, &asset);
    f_WindSpeedFont = AllocMemory(OpenEncodedAsset(ASSET_WIND_SPEED_FONT, &asset));
    ReadEncodedAsset(f_WindSpeedFont, &asset);
    f_RingImages = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_725, &asset));
    ReadEncodedAsset(f_RingImages, &asset);
    f_RestLabelImage = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_568, &asset));
    ReadEncodedAsset(f_RestLabelImage, &asset);
    f_FlatLabelImage = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_56B, &asset));
    ReadEncodedAsset(f_FlatLabelImage, &asset);
    f_DownLabelImage = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_56A, &asset));
    ReadEncodedAsset(f_DownLabelImage, &asset);
    f_UpLabelImage = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_569, &asset));
    ReadEncodedAsset(f_UpLabelImage, &asset);
    f_UnusedRoundImage = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_723, &asset));
    ReadEncodedAsset(f_UnusedRoundImage, &asset);
    g_DistanceSignImage = AllocMemory(OpenEncodedAsset(ASSET_DISTANCE_SIGN_IMAGE, &asset));
    ReadEncodedAsset(g_DistanceSignImage, &asset);
    LoadFireworks(GetCourseRecordIndex());
}

void FreeSceneryImages(void) {
    if (f_SceneryState == SCENERY_LOADED) {
        f_SceneryState = SCENERY_BILLBOARDS_READY;
        FreeEffectImages();
        FreeHoleCreatureImages();
        FreeShotEffectImages();
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
        FreeMemory(&f_UnusedRoundImage);
        FreeMemory(&g_DistanceSignImage);
        FreeFireworks();
    }
}

void InitScenery(void) {
    EncodedAsset unusedAsset UNUSED;

    if (f_SceneryState == SCENERY_UNLOADED) {
        f_SceneryBillboardHeap = AllocMemory(BILLBOARD_HEAP_SIZE);
        InitBillboardManager(f_SceneryBillboardHeap, BILLBOARD_HEAP_SIZE);
        f_SceneryState = SCENERY_BILLBOARDS_READY;
    }
}

void FreeScenery(void) {
    if (f_SceneryState != SCENERY_UNLOADED) {
        DestroyBillboardManager();
        FreeMemory(&f_SceneryBillboardHeap);
        StopWeather();
        ClearShotEffects(CLEAR_KIND_ALL);
        FreeSceneryImages();
        f_SceneryState = SCENERY_UNLOADED;
        memset(&g_RoundHud, 0, sizeof(g_RoundHud));
    }
}

void StartShotPanel(void) {
    u8 unusedStack[0x20] UNUSED;
    EncodedAsset asset;
    s32 height;
    s32 playerIndex;
    s32 holeIndex;

    height = g_CurrentShot.ballPosition[AXIS_Y] - g_HoleSetup.cupPosition[AXIS_Y];
    g_RoundHud.ballHeightYards = height * YARDS_PER_UNIT;
    g_RoundHud.ballHeightMeters = height * METERS_PER_UNIT;
    g_RoundHud.ballHeightFeet = (height * FEET_PER_YARD) * YARDS_PER_UNIT;
    if (g_RoundHud.ballHeightYards == 0) {
        g_RoundHud.ballHeightYardsDirection = HEIGHT_LEVEL;
    }
    else if (g_RoundHud.ballHeightYards > 0) {
        g_RoundHud.ballHeightYardsDirection = HEIGHT_ABOVE;
    }
    else {
        g_RoundHud.ballHeightYardsDirection = HEIGHT_BELOW;
    }

    if (g_RoundHud.ballHeightMeters < LEVEL_HEIGHT && g_RoundHud.ballHeightMeters > -LEVEL_HEIGHT) {
        g_RoundHud.ballHeightMetersDirection = HEIGHT_LEVEL;
    }
    else if (g_RoundHud.ballHeightMeters > 0.0f) {
        g_RoundHud.ballHeightMetersDirection = HEIGHT_ABOVE;
    }
    else {
        g_RoundHud.ballHeightMetersDirection = HEIGHT_BELOW;
    }

    if (g_RoundHud.ballHeightFeet < LEVEL_HEIGHT && g_RoundHud.ballHeightFeet > -LEVEL_HEIGHT) {
        g_RoundHud.ballHeightFeetDirection = HEIGHT_LEVEL;
    }
    else if (g_RoundHud.ballHeightMeters > 0.0f) {
        g_RoundHud.ballHeightFeetDirection = HEIGHT_ABOVE;
    }
    else {
        g_RoundHud.ballHeightFeetDirection = HEIGHT_BELOW;
    }

    g_RoundHud.cupDistance = (f32)GetCupDistance(
        g_CurrentShot.ballPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
        g_CurrentShot.ballPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]
    );
    if (GetSurfaceTraits(g_CurrentShot.surface)->lieImageAsset != f_LieImageAsset) {
        f_LieImageAsset = GetSurfaceTraits(g_CurrentShot.surface)->lieImageAsset;
        if (GetSurfaceTraits(g_CurrentShot.surface)->kind == SURFACE_KIND_TEE_GROUND) {
            f_ShotPanelOffsetY = TEE_GROUND_PANEL_OFFSET_Y;
        }
        else {
            f_ShotPanelOffsetY = 0;
        }

        OpenEncodedAsset(f_LieImageAsset, &asset);
        ReadEncodedAsset(f_LieImage, &asset);
    }

    if (g_GameState.replayKind != REPLAY_KIND_NONE) {
        g_RoundHud.skinsTotals[0] = 0;
    }
    else {
        switch (g_GameState.gameMode) {
            case GAME_MODE_STROKE:
            case GAME_MODE_UNLISTED_STROKE:
            case GAME_MODE_TOURNAMENT:
            case GAME_MODE_SPEED_GOLF:
                for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
                    g_RoundHud.skinsTotals[playerIndex] =
                        GetNetScoreToPar(g_GameState.holeIndex, playerIndex);
                }

                break;
            case GAME_MODE_MATCH_GAME:
            case GAME_MODE_GET_CHARACTER:
                g_RoundHud.skinsTotals[0] = 0;
                for (holeIndex = 0; holeIndex != g_GameState.holeIndex; holeIndex++) {
                    if (g_Players[0].holes[holeIndex].strokeCount <
                        g_Players[1].holes[holeIndex].strokeCount) {
                        g_RoundHud.skinsTotals[0]--;
                    }
                    else if (g_Players[1].holes[holeIndex].strokeCount <
                        g_Players[0].holes[holeIndex].strokeCount) {
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

    g_ShotPanelOffsetX = SHOT_PANEL_START_OFFSET_X;
    ResetShotModeChange();
    LoadContactImages(&g_Players[g_RoundHud.shooterIndex]);
}

void LoadRoundHud(void) {
    AssetFile file;
    EncodedAsset asset;
    s32 i;
    u32 paletteSize;

    g_CourseSkyImage = AllocMemory(OpenEncodedAsset(ASSET_COURSE_SKY_IMAGE_0, &asset));
    ReadEncodedAsset(g_CourseSkyImage, &asset);
    g_SkyImage = AllocMemory(OpenEncodedAsset(ASSET_SKY_IMAGE_0, &asset));
    ReadEncodedAsset(g_SkyImage, &asset);
    f_HoleImage = AllocMemory(OpenEncodedAsset(f_HoleImageAssets[0], &asset));
    ReadEncodedAsset(f_HoleImage, &asset);
    f_ParImage = AllocMemory(OpenEncodedAsset(ASSET_PAR_FIVE_IMAGE, &asset));
    ReadEncodedAsset(f_ParImage, &asset);
    f_DistanceFont = AllocMemory(OpenEncodedAsset(ASSET_DISTANCE_DIGITS, &asset));
    ReadEncodedAsset(f_DistanceFont, &asset);
    g_RingTexels = AllocMemory(RING_TEXTURE_SIZE * RING_TEXTURE_SIZE);
    BuildRingTexels(g_RingTexels, RING_TEXTURE_SIZE, RING_TEXTURE_SIZE, RING_TEXTURE_OUTER_RADIUS);
    for (i = 0; i != SHOT_OFFSET_MARKER_COUNT; i++) {
        f_ShotOffsetMarkerXs[i] = 0;
        f_ShotOffsetMarkerYs[i] = 0;
    }

    g_HudScissor.left = HUD_MARGIN;
    g_HudScissor.top = HUD_MARGIN;
    g_HudScissor.right = SCREEN_WIDTH - HUD_MARGIN;
    f_UnusedHudScissorWord = 0;
    g_HudScissor.bottom = SCREEN_HEIGHT - HUD_MARGIN;
    g_NoiseTexels = AllocMemory(NOISE_GRID_SIZE * NOISE_GRID_SIZE);
    g_BlendedTextureScrollingLayer =
        AllocMemory(OpenEncodedAsset(ASSET_BLENDED_TEXTURE_SCROLLING_LAYER, &asset));
    ReadEncodedAsset(g_BlendedTextureScrollingLayer, &asset);
    g_BlendedTextureStillLayer =
        AllocMemory(OpenEncodedAsset(ASSET_BLENDED_TEXTURE_STILL_LAYER, &asset));
    ReadEncodedAsset(g_BlendedTextureStillLayer, &asset);
    for (i = 0; i != PLAYER_COUNT_MAX; i++) {
        g_PlayerFaceImages[i] = AllocMemory(
            OpenEncodedAsset(g_CharacterFirstFaceAssets[i] + CHARACTER_FACE_IMAGE_OFFSET, &asset)
        );
        ReadEncodedAsset(g_PlayerFaceImages[i], &asset);
        OpenAssetFile(g_CharacterFirstPaletteFiles[i], &file);
        paletteSize = GetAssetFileSize(&file);
        g_PlayerFacePalettes[i] = AllocMemory(paletteSize);
        ReadAssetFile(g_PlayerFacePalettes[i], paletteSize, &file);
    }

    InitScreenLayers();
    LoadPinTextures();
    ClearEffects();
}

void FreeRoundHudTextures(void) {
    FreePinTextures();
}

#define INSET_FRAME_TEXTURE_WIDTH 16
#define INSET_FRAME_TEXTURE_HEIGHT 3
#define INSET_FRAME_SIDE_TEXTURE_HEIGHT 1
#define INSET_FRAME_TEXTURE_MASK 1
#define INSET_FRAME_SIDE_TEXEL 5
#define INSET_FRAME_SIDE_HALF_WIDTH 2
#define INSET_FRAME_BORDER_OUTSIDE 1
#define INSET_FRAME_BORDER_INSIDE 2

void DrawInsetViewFrame(void) {
    SetUpTextDrawing(&g_DisplayListHead);
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetRenderMode(g_DisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gSPTexture(
        g_DisplayListHead++,
        TEXTURE_SCALE_HALF,
        TEXTURE_SCALE_HALF,
        0,
        G_TX_RENDERTILE,
        G_ON
    );
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
    gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    gDPSetCombineMode(g_DisplayListHead++, G_CC_MODULATEIDECALA_PRIM, G_CC_MODULATEIDECALA_PRIM);
    gDPLoadTextureBlock_4b(
        g_DisplayListHead++,
        TextureAddress(f_InsetFrameTexture),
        G_IM_FMT_I,
        INSET_FRAME_TEXTURE_WIDTH,
        INSET_FRAME_TEXTURE_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP,
        INSET_FRAME_TEXTURE_MASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(g_DisplayListHead++);
    if (g_Cameras[INSET_CAMERA].view.scissorWidth > 0) {
        gSPTextureRectangle(
            g_DisplayListHead++,
            QuarterPixels(g_Cameras[INSET_CAMERA].view.scissorX),
            QuarterPixels(g_Cameras[INSET_CAMERA].view.scissorY - INSET_FRAME_BORDER_OUTSIDE),
            QuarterPixels(
                g_Cameras[INSET_CAMERA].view.scissorX + g_Cameras[INSET_CAMERA].view.scissorWidth
            ),
            QuarterPixels(g_Cameras[INSET_CAMERA].view.scissorY + INSET_FRAME_BORDER_INSIDE),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gSPTextureRectangle(
            g_DisplayListHead++,
            QuarterPixels(g_Cameras[INSET_CAMERA].view.scissorX),
            QuarterPixels(
                g_Cameras[INSET_CAMERA].view.scissorY + g_Cameras[INSET_CAMERA].view.scissorHeight -
                INSET_FRAME_BORDER_INSIDE
            ),
            QuarterPixels(
                g_Cameras[INSET_CAMERA].view.scissorX + g_Cameras[INSET_CAMERA].view.scissorWidth
            ),
            QuarterPixels(
                g_Cameras[INSET_CAMERA].view.scissorY + g_Cameras[INSET_CAMERA].view.scissorHeight +
                INSET_FRAME_BORDER_OUTSIDE
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    gDPPipeSync(g_DisplayListHead++);
    gDPLoadTextureBlock_4b(
        g_DisplayListHead++,
        TextureAddress(f_InsetFrameTexture),
        G_IM_FMT_I,
        INSET_FRAME_TEXTURE_WIDTH,
        INSET_FRAME_SIDE_TEXTURE_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMASK,
        INSET_FRAME_TEXTURE_MASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(g_DisplayListHead++);
    if (g_Cameras[INSET_CAMERA].view.scissorHeight > 0) {
        gSPTextureRectangle(
            g_DisplayListHead++,
            QuarterPixels(g_Cameras[INSET_CAMERA].view.scissorX - INSET_FRAME_SIDE_HALF_WIDTH),
            QuarterPixels(g_Cameras[INSET_CAMERA].view.scissorY),
            QuarterPixels(g_Cameras[INSET_CAMERA].view.scissorX + INSET_FRAME_SIDE_HALF_WIDTH),
            QuarterPixels(
                g_Cameras[INSET_CAMERA].view.scissorY + g_Cameras[INSET_CAMERA].view.scissorHeight
            ),
            G_TX_RENDERTILE,
            TexelCoordinate(INSET_FRAME_SIDE_TEXEL),
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gSPTextureRectangle(
            g_DisplayListHead++,
            QuarterPixels(
                g_Cameras[INSET_CAMERA].view.scissorX + g_Cameras[INSET_CAMERA].view.scissorWidth -
                INSET_FRAME_SIDE_HALF_WIDTH
            ),
            QuarterPixels(g_Cameras[INSET_CAMERA].view.scissorY),
            QuarterPixels(
                g_Cameras[INSET_CAMERA].view.scissorX + g_Cameras[INSET_CAMERA].view.scissorWidth +
                INSET_FRAME_SIDE_HALF_WIDTH
            ),
            QuarterPixels(
                g_Cameras[INSET_CAMERA].view.scissorY + g_Cameras[INSET_CAMERA].view.scissorHeight
            ),
            G_TX_RENDERTILE,
            TexelCoordinate(INSET_FRAME_SIDE_TEXEL),
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    gDPPipeSync(g_DisplayListHead++);
}

void ScrollWindArrow(void) {
    s32 step = g_HoleSetup.conditions.windSpeed / WIND_SPEED_PER_ARROW_SCROLL_STEP;
    s32 vertexIndex;

    for (vertexIndex = 0; vertexIndex != ARROW_VERTEX_COUNT; vertexIndex++) {
        f_Arrows[g_MatrixBufferIndex].body[vertexIndex].v.tc[TEXTURE_AXIS_T] += step;
    }

    if (f_Arrows[g_MatrixBufferIndex].body[0].v.tc[TEXTURE_AXIS_T] >
        ARROW_TEXTURE_COORDINATE_LIMIT) {
        for (vertexIndex = 0; vertexIndex != ARROW_VERTEX_COUNT; vertexIndex++) {
            f_Arrows[g_MatrixBufferIndex].body[vertexIndex].v.tc[TEXTURE_AXIS_T] -=
                ARROW_TEXTURE_PERIOD;
        }
    }
}

void StartLandingReaction(LandingReaction reaction) {
    LandingReaction previousReaction;

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
    switch ((u32)GetSurfaceTraits(g_TurnBallSimulation.surface)->kind) {
        case SURFACE_KIND_OUT_OF_BOUNDS:
            g_InsetView.animation = INSET_VIEW_OPENING;
            g_InsetView.ignoredSourceCode = LANDING_INSET_SOURCE_CODE;
            f_LandingReaction = previousReaction;
            g_InsetView.ignoredSizeCode = MOVIE_INSET_SIZE_CODE;
            SetInsetViewRectangle(
                MOVIE_INSET_CENTER_X,
                MOVIE_INSET_CENTER_Y,
                MOVIE_INSET_SIZE,
                MOVIE_INSET_SIZE
            );
            SetLandingWaitFrameCount(LANDING_WAIT_LONG_FRAME_COUNT);
            StartMovie(MOVIE_TOAD_OUT_OF_BOUNDS);
            osSyncPrintf("MOVIE KINOP \n");
            break;
        case SURFACE_KIND_OUT_OF_COURSE:
        case SURFACE_KIND_WATER_HAZARD:
            f_LandingReaction = LANDING_REACTION_SURFACE_ONLY;
            SetLandingWaitFrameCount(LANDING_WAIT_LONG_FRAME_COUNT);
            break;
        case SURFACE_KIND_GREEN:
            SetLandingWaitFrameCount(LANDING_WAIT_FRAME_COUNT);
            break;
        default:
            g_InsetView.animation = INSET_VIEW_OPENING;
            g_InsetView.ignoredSourceCode = LANDING_INSET_SOURCE_CODE;
            g_InsetView.ignoredSizeCode = LANDING_INSET_SIZE_CODE;
            SetLandingWaitFrameCount(LANDING_WAIT_FRAME_COUNT);
            SetInsetViewRectangle(
                LANDING_INSET_CENTER_X,
                LANDING_INSET_CENTER_Y,
                LANDING_INSET_WIDTH,
                LANDING_INSET_HEIGHT
            );
            break;
    }
}

void DrawLandingReaction(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    Vec3 scale;
    Vec3 translation;
    Vec3 rotation;
    Mat4 matrix;
    Mat4 pivotTranslation;
    SurfaceTraits *surfaceTraits;
    s32 distance;

    Radians angle;
    f32 size;

    if (f_LandingReaction != LANDING_REACTION_NONE) {
        surfaceTraits = GetSurfaceTraits(g_TurnBallSimulation.surface);
        if (f_LandingReactionFrame == 1) {
            PlaceGlyphGroup(
                LANDING_GLYPH_GROUP_FIRST,
                FONT_SIZE_LARGE,
                0,
                0,
                LANDING_NAME_GLYPH_Y,
                surfaceTraits->name
            );
        }

        if (f_LandingReaction == LANDING_REACTION_REST && !IsOnDrivingRange()) {
            distance = GetCupDistance(
                g_TurnBallSimulation.position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
                g_TurnBallSimulation.position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]
            );
            if (g_GameState.isMiniGolf || surfaceTraits->kind == SURFACE_KIND_GREEN) {
                s32 hundredths = distance * (FEET_PER_YARD * HUNDREDTHS_PER_UNIT) * YARDS_PER_UNIT;

                sprintf(
                    g_PrintBuffer,
                    "%2d.%02d%c REST",
                    hundredths / HUNDREDTHS_PER_UNIT,
                    hundredths % HUNDREDTHS_PER_UNIT,
                    FEET_GLYPH
                );
            }
            else {
                s32 hundredths = distance * HUNDREDTHS_PER_UNIT * YARDS_PER_UNIT;

                sprintf(
                    g_PrintBuffer,
                    "%3d.%02d%c REST",
                    hundredths / HUNDREDTHS_PER_UNIT,
                    hundredths % HUNDREDTHS_PER_UNIT,
                    YARDS_GLYPH
                );
            }

            if (f_LandingReactionFrame == 1) {
                PlaceGlyphGroup(
                    LANDING_GLYPH_GROUP_SECOND,
                    FONT_SIZE_MEDIUM,
                    0,
                    0,
                    LANDING_DISTANCE_GLYPH_Y,
                    g_PrintBuffer
                );
            }
        }

        if (f_LandingReactionFrame < LANDING_REACTION_FRAME_COUNT) {
            f_LandingReactionFrame++;
        }

        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
        gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_PERSP);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
        gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, LANDING_PANEL_ALPHA);
        gDPPipeSync(gfx++);
        angle = DegreesToRadians(
            (f_LandingReactionFrame * LANDING_PANEL_DEGREES_PER_FRAME) + LANDING_PANEL_START_DEGREES
        );
        if (angle > FULL_TURN_RADIANS) {
            angle = FULL_TURN_RADIANS;
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

        guTranslateF(pivotTranslation, LANDING_PANEL_PIVOT_X, LANDING_PANEL_PIVOT_Y, 0.0f);
        guMtxCatF(pivotTranslation, matrix, matrix);
        ConvertMatrixToFixed(matrix, &f_LandingPanelMatrices[g_MatrixBufferIndex]);
        gSPMatrix(
            gfx++,
            PhysicalAddress(&g_ScreenOrthoMatrix),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_LandingPanelMatrices[g_MatrixBufferIndex]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
        );
        gSPVertex(gfx++, f_LandingPanelVertices, ARRAY_COUNT(f_LandingPanelVertices), 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        if (f_LandingReactionFrame < LANDING_PANEL_GROWTH_FRAME_COUNT) {
            SetUpPerspectiveTextDrawing(&gfx);
            DrawGlyphGroup(&gfx, LANDING_GLYPH_GROUP_FIRST);
            if (f_LandingReaction == LANDING_REACTION_REST) {
                DrawGlyphGroup(&gfx, LANDING_GLYPH_GROUP_SECOND);
            }
        }
        else {
            SetUpTextDrawing(&gfx);
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
            DrawCenteredLargeText(&gfx, SCREEN_WIDTH / 2, LANDING_NAME_Y, surfaceTraits->name);
            if (f_LandingReaction == LANDING_REACTION_REST && !IsOnDrivingRange()) {
                DrawCenteredMediumText(&gfx, SCREEN_WIDTH / 2, LANDING_DISTANCE_Y, g_PrintBuffer);
            }
        }
    }

    *displayListHead = gfx;
}

u32 GetWarmthTint(s32 warmth, s32 alpha) {
    s32 red;
    s32 green;
    s32 blue;

    blue = COLOR_CHANNEL_MAX;
    if (warmth == 0) {
        green = COLOR_CHANNEL_MAX;
        red = COLOR_CHANNEL_MAX;
    }
    else if (warmth > 0) {
        red = COLOR_CHANNEL_MAX;
        blue = warmth;
        blue = red - blue;
        green = blue;
    }
    else {
        green = warmth + COLOR_CHANNEL_MAX;
        red = green;
    }

    return RgbaColor(red, green, blue, alpha & 0xFF);
}

#define GREEN_GRID_SHORT_ROW_COUNT 8
#define GREEN_GRID_LANDING_ROW_COUNT 16
#define GREEN_GRID_FIRST_FULL_ROW 30
#define GREEN_GRID_PUTT_DISTANCE_MAX 30
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
#define GREEN_GRID_SLOPE_MAX 255
#define GREEN_GRID_LINE_HALF_WIDTH 4.5f
#define GREEN_GRID_DEPTH_BIAS 15.0f
#define GREEN_GRID_DIM_ALPHA 0x3F
#define GREEN_GRID_FADE_STEP 8
#define GREEN_GRID_FADE_END_ALPHA 0x48
#define GAME_FLAG_HIDE_GREEN_GRID 0x79
#define GREEN_GRID_HIDDEN 0
#define GREEN_GRID_SHOWN 1
#define GREEN_GRID_FADING 2

typedef struct GridScreenPoint {
    s32 x;
    s32 y;
    Boolean isVisible;
    s32 depth;
} GridScreenPoint;

#define GridPointIndex(row, column) (((row) * columnCount) + (column))

#define SetGreenGridTint(slope) \
    if (g_Players[g_CurrentShot.playerIndex].gridColor == GRID_COLOR_WHITE) { \
        if ((slope) == 0) { \
            blue = COLOR_CHANNEL_MAX; \
            green = COLOR_CHANNEL_MAX; \
            red = COLOR_CHANNEL_MAX; \
        } \
        else if ((slope) > 0) { \
            red = COLOR_CHANNEL_MAX; \
            blue = red - (slope); \
            green = blue; \
        } \
        else { \
            blue = COLOR_CHANNEL_MAX; \
            green = (slope) + COLOR_CHANNEL_MAX; \
            red = green; \
        } \
    } \
    else { \
        if ((slope) == 0) { \
            blue = 0; \
            green = 0; \
            red = 0; \
        } \
        else if ((slope) > 0) { \
            red = (slope); \
            blue = 0; \
            green = 0; \
        } \
        else { \
            blue = -(slope); \
            green = 0; \
            red = 0; \
        } \
    }

void DrawGreenGrid(Gfx **displayListHead, CameraMatrices *matrices) {
    f32 points[GREEN_GRID_POINT_COUNT][3];
    GridScreenPoint screenPoints[GREEN_GRID_POINT_COUNT];
    s32 slopes[GREEN_GRID_POINT_COUNT];
    f32 unusedPoints[GREEN_GRID_POINT_COUNT][3] UNUSED;
    CoursePoint center;
    ScreenPoint projected;
    Gfx *gfx = *displayListHead;
    Club *club;
    u64 unusedClub UNUSED;
    s32 distance;
    s32 worldDistance;
    s32 rowCount;
    s32 columnCount;
    s32 puttDistanceMax;
    s32 row;
    s32 column;
    s32 drawnRow;
    s32 drawnColumn;
    s32 rowPosition;
    Boolean isUnscrolledRow;
    Radians yaw;
    Radians sideAngle;
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
    f32 scrollX;
    f32 scrollZ;
    f32 height;
    f32 deltaX;
    f32 deltaY;
    f32 scale;

#ifdef AVOID_UB
    columnCount = GREEN_GRID_COLUMN_COUNT;
#endif
    if (g_RoundHud.greenGridState == GREEN_GRID_HIDDEN ||
        g_IsCupViewShown == TRUE ||
        IsGameFlagSet(GAME_FLAG_HIDE_GREEN_GRID) ||
        !IsTurnTakingStep(g_GameState.step)) {
        return;
    }

    club = GetSelectedClub();
    if (club != NULL) {
        if (gfx != NULL) {
            unusedClub = (u32)club;
        }
    }

    yaw = (u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS;
    distance = GetSelectedShotDistance();
    worldDistance = GetSelectedShotWorldDistance();
    if (club->surfaceMask & CLUB_SURFACE_GREEN) {
        puttDistanceMax = GREEN_GRID_PUTT_DISTANCE_MAX;
        if (distance == puttDistanceMax) {
            rowShift = GREEN_GRID_PUTT_ROW_SHIFT;
            columnSpacing = GREEN_GRID_PUTT_COLUMN_SPACING;
            rowSpacing = GREEN_GRID_SHORT_ROW_SPACING;
            rowCount = GREEN_GRID_SHORT_ROW_COUNT;
            center[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
            center[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
            center[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
        }
        else {
            rowShift = GREEN_GRID_PUTT_ROW_SHIFT;
            columnSpacing = GREEN_GRID_PUTT_COLUMN_SPACING;
            rowCount = GREEN_GRID_ROW_COUNT_MAX;
            center[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
            center[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
            center[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
            rowSpacing = distance * GREEN_GRID_PUTT_ROW_SPACING / puttDistanceMax;
        }
    }
    else if (distance < GREEN_GRID_LANDING_DISTANCE) {
        rowShift = GREEN_GRID_PUTT_ROW_SHIFT;
        columnSpacing = GREEN_GRID_PUTT_COLUMN_SPACING;
        rowCount = GREEN_GRID_ROW_COUNT_MAX;
        center[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
        center[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
        center[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
        puttDistanceMax = GREEN_GRID_PUTT_DISTANCE_MAX;
        rowSpacing = distance * GREEN_GRID_ROW_SPACING / puttDistanceMax;
    }
    else {
        center[AXIS_X] = (cosf(yaw) * worldDistance) + g_CurrentShot.ballPosition[AXIS_X];
        rowShift = GREEN_GRID_LANDING_ROW_SHIFT;
        rowSpacing = GREEN_GRID_LANDING_ROW_SPACING;
        center[AXIS_Z] = (sinf(yaw) * worldDistance) + g_CurrentShot.ballPosition[AXIS_Z];
        g_UnreadGroundQueryMarker = 1;
        center[AXIS_Y] = GetGroundHeight(center[AXIS_X], center[AXIS_Z]);
        rowCount = GREEN_GRID_LANDING_ROW_COUNT;
        columnSpacing = GREEN_GRID_LANDING_WIDTH / columnCount;
    }

    forwardX = -cosf(yaw);
    forwardZ = -sinf(yaw);
    sideAngle = yaw + (M_PI / 2);
    sideX = -cosf(sideAngle) * columnSpacing;
    sideZ = -sinf(sideAngle) * columnSpacing;
    shift = (rowShift - rowCount) * rowSpacing;
    centering = 0.5f - (columnCount * 0.5f);
    originX = (forwardX * shift) + (sideX * centering);
    originZ = (forwardZ * shift) + (sideZ * centering);
    forwardX = -cosf(yaw) * rowSpacing;
    forwardZ = -sinf(yaw) * rowSpacing;
    if (g_RoundHud.greenGridState == GREEN_GRID_SHOWN) {
        f_GreenGridScroll -= GREEN_GRID_SCROLL_STEP;
        if (rowCount != GREEN_GRID_ROW_COUNT_MAX) {
            if (f_GreenGridScroll < 0.0f) {
                f_GreenGridScroll += 1.0f;
            }
        }
        else if (f_GreenGridScroll < -1.0f) {
            f_GreenGridScroll += 2.0f;
        }
    }

    scrollX = forwardX * f_GreenGridScroll;
    scrollZ = forwardZ * f_GreenGridScroll;
    columnCount = GREEN_GRID_COLUMN_COUNT;

    for (row = 0; row != rowCount;) {
        for (column = 0; column != columnCount; column++) {
            isUnscrolledRow = row == rowCount - 1 || row == 0;
            rowPosition = row;
            if (row != 0) {
                rowPosition = row - 1;
            }

            if (isUnscrolledRow || (f_GreenGridScroll < 0.0f && row == 1)) {
                points[GridPointIndex(row, column)][AXIS_X] =
                    center[AXIS_X] + originX + (forwardX * rowPosition) + (sideX * column);
                points[GridPointIndex(row, column)][AXIS_Z] =
                    center[AXIS_Z] + originZ + (forwardZ * rowPosition) + (sideZ * column);
            }
            else {
                points[GridPointIndex(row, column)][AXIS_X] = center[AXIS_X] + originX +
                    (forwardX * rowPosition) + (sideX * column) + scrollX;
                points[GridPointIndex(row, column)][AXIS_Z] = center[AXIS_Z] + originZ +
                    (forwardZ * rowPosition) + (sideZ * column) + scrollZ;
            }

            if (g_RoundHud.greenGridState == GREEN_GRID_SHOWN) {
                g_UnreadGroundQueryMarker = 1;
                points[GridPointIndex(row, column)][AXIS_Y] = GetGroundHeight(
                    points[GridPointIndex(row, column)][AXIS_X],
                    points[GridPointIndex(row, column)][AXIS_Z]
                );
                f_GreenGridHeights[GridPointIndex(row, column)] =
                    points[GridPointIndex(row, column)][AXIS_Y];
            }
            else {
                points[GridPointIndex(row, column)][AXIS_Y] =
                    f_GreenGridHeights[GridPointIndex(row, column)];
            }
        }

        if (rowCount != GREEN_GRID_ROW_COUNT_MAX) {
            row++;
        }
        else if (row >= GREEN_GRID_FIRST_FULL_ROW) {
            row++;
        }
        else {
            row += 2;
        }
    }

    if (rowCount == GREEN_GRID_ROW_COUNT_MAX) {
        for (row = 1; row != rowCount - 1; row += 2) {
            for (column = 0; column != columnCount; column++) {
                points[GridPointIndex(row, column)][AXIS_X] =
                    (points[GridPointIndex(row - 1, column)][AXIS_X] +
                        points[GridPointIndex(row + 1, column)][AXIS_X]) *
                    0.5f;
                points[GridPointIndex(row, column)][AXIS_Y] =
                    (points[GridPointIndex(row - 1, column)][AXIS_Y] +
                        points[GridPointIndex(row + 1, column)][AXIS_Y]) *
                    0.5f;
                points[GridPointIndex(row, column)][AXIS_Z] =
                    (points[GridPointIndex(row - 1, column)][AXIS_Z] +
                        points[GridPointIndex(row + 1, column)][AXIS_Z]) *
                    0.5f;
            }
        }
    }

    for (row = 0; row != rowCount; row++) {
        for (column = 0; column != columnCount; column++) {
            if (ProjectPoint(
                    points[GridPointIndex(row, column)][AXIS_X] * COURSE_TO_WORLD_SCALE,
                    points[GridPointIndex(row, column)][AXIS_Y] * COURSE_TO_WORLD_SCALE,
                    points[GridPointIndex(row, column)][AXIS_Z] * COURSE_TO_WORLD_SCALE,
                    &projected
                ) < 0) {
                screenPoints[GridPointIndex(row, column)].x =
                    projected.x * QUARTER_PIXELS_PER_PIXEL;
                screenPoints[GridPointIndex(row, column)].y =
                    projected.y * QUARTER_PIXELS_PER_PIXEL;
                screenPoints[GridPointIndex(row, column)].isVisible = TRUE;
                screenPoints[GridPointIndex(row, column)].depth =
                    projected.depth - (projected.depthStep * GREEN_GRID_DEPTH_BIAS);
            }
            else {
                screenPoints[GridPointIndex(row, column)].isVisible = FALSE;
            }

            height = points[GridPointIndex(row, column)][AXIS_Y] - center[AXIS_Y];
            if (height < 0.0f) {
                slopes[GridPointIndex(row, column)] = sqrtf(-height) * GREEN_GRID_SLOPE_SCALE;
            }
            else if (height > 0.0f) {
                slopes[GridPointIndex(row, column)] = -sqrtf(height) * GREEN_GRID_SLOPE_SCALE;
            }
            else {
                slopes[GridPointIndex(row, column)] = 0;
            }

            if (slopes[GridPointIndex(row, column)] > GREEN_GRID_SLOPE_MAX) {
                slopes[GridPointIndex(row, column)] = GREEN_GRID_SLOPE_MAX;
            }

            if (slopes[GridPointIndex(row, column)] < -GREEN_GRID_SLOPE_MAX) {
                slopes[GridPointIndex(row, column)] = -GREEN_GRID_SLOPE_MAX;
            }
        }
    }

    guMtxIdent(&f_GreenGridMatrix);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
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
    gSPMatrix(
        gfx++,
        PhysicalAddress(&g_ScreenOrthoMatrix),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_GreenGridMatrix),
        G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW
    );
    gSPVertex(gfx++, f_BallTrailVertices, ARRAY_COUNT(f_BallTrailVertices), 0);
    if (g_PauseScreen == PAUSE_SCREEN_FREE_LOOK) {
        f_GreenGridAlpha = GREEN_GRID_DIM_ALPHA;
    }
    else if (g_RoundHud.greenGridState != GREEN_GRID_FADING) {
        f_GreenGridAlpha = GREEN_GRID_ALPHA;
    }
    else if (f_GreenGridAlpha < GREEN_GRID_FADE_END_ALPHA) {
        f_GreenGridAlpha = GREEN_GRID_DIM_ALPHA;
    }
    else {
        f_GreenGridAlpha -= GREEN_GRID_FADE_STEP;
    }

    if (g_RoundHud.greenGridState != GREEN_GRID_HIDDEN) {
        for (drawnRow = 0; drawnRow != rowCount;) {
            for (drawnColumn = 0; drawnColumn != columnCount; drawnColumn++) {
                s32 red;
                s32 green;
                s32 blue;

                if (screenPoints[GridPointIndex(drawnRow, drawnColumn)].isVisible != TRUE) {
                    continue;
                }

                if (drawnColumn & 1) {
                    deltaX = screenPoints[GridPointIndex(drawnRow, drawnColumn)].x -
                        screenPoints[GridPointIndex(drawnRow, drawnColumn) - 1].x;
                    deltaY = screenPoints[GridPointIndex(drawnRow, drawnColumn)].y -
                        screenPoints[GridPointIndex(drawnRow, drawnColumn) - 1].y;
                }
                else {
                    deltaX = screenPoints[GridPointIndex(drawnRow, drawnColumn)].x -
                        screenPoints[GridPointIndex(drawnRow, drawnColumn) + 1].x;
                    deltaY = screenPoints[GridPointIndex(drawnRow, drawnColumn)].y -
                        screenPoints[GridPointIndex(drawnRow, drawnColumn) + 1].y;
                }

                if (deltaX == 0.0f && deltaY == 0.0f) {
                    screenPoints[GridPointIndex(drawnRow, drawnColumn)].isVisible = FALSE;
                    continue;
                }

                scale = GREEN_GRID_LINE_HALF_WIDTH / sqrtf((deltaX * deltaX) + (deltaY * deltaY));
                deltaX *= scale;
                deltaY *= scale;
                SetGreenGridTint(slopes[GridPointIndex(drawnRow, drawnColumn)]);
                if (drawnColumn & 1) {
                    gSPModifyVertex(
                        gfx++,
                        2,
                        G_MWO_POINT_XYSCREEN,
                        ScreenPointWord(
                            screenPoints[GridPointIndex(drawnRow, drawnColumn)].x,
                            screenPoints[GridPointIndex(drawnRow, drawnColumn)].y
                        )
                    );
                    gSPModifyVertex(
                        gfx++,
                        3,
                        G_MWO_POINT_XYSCREEN,
                        ScreenPointWord(
                            screenPoints[GridPointIndex(drawnRow, drawnColumn)].x + (s32)deltaY,
                            screenPoints[GridPointIndex(drawnRow, drawnColumn)].y - (s32)deltaX
                        )
                    );
                    gSPModifyVertex(
                        gfx++,
                        2,
                        G_MWO_POINT_RGBA,
                        RgbaColor(red, green, blue, f_GreenGridAlpha)
                    );
                    gSPModifyVertex(
                        gfx++,
                        3,
                        G_MWO_POINT_RGBA,
                        RgbaColor(red, green, blue, f_GreenGridAlpha)
                    );
                    if (screenPoints[GridPointIndex(drawnRow, drawnColumn) - 1].isVisible == TRUE) {
                        gDPPipeSync(gfx++);
                        if (screenPoints[GridPointIndex(drawnRow, drawnColumn)].depth <
                            screenPoints[GridPointIndex(drawnRow, drawnColumn) - 1].depth) {
                            gDPSetPrimDepth(
                                gfx++,
                                screenPoints[GridPointIndex(drawnRow, drawnColumn)].depth,
                                0
                            );
                        }
                        else {
                            gDPSetPrimDepth(
                                gfx++,
                                screenPoints[GridPointIndex(drawnRow, drawnColumn) - 1].depth,
                                0
                            );
                        }

                        gDPPipeSync(gfx++);
                        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
                    }
                }
                else {
                    gSPModifyVertex(
                        gfx++,
                        0,
                        G_MWO_POINT_XYSCREEN,
                        ScreenPointWord(
                            screenPoints[GridPointIndex(drawnRow, drawnColumn)].x,
                            screenPoints[GridPointIndex(drawnRow, drawnColumn)].y
                        )
                    );
                    gSPModifyVertex(
                        gfx++,
                        1,
                        G_MWO_POINT_XYSCREEN,
                        ScreenPointWord(
                            screenPoints[GridPointIndex(drawnRow, drawnColumn)].x + (s32)deltaY,
                            screenPoints[GridPointIndex(drawnRow, drawnColumn)].y - (s32)deltaX
                        )
                    );
                    gSPModifyVertex(
                        gfx++,
                        0,
                        G_MWO_POINT_RGBA,
                        RgbaColor(red, green, blue, f_GreenGridAlpha)
                    );
                    gSPModifyVertex(
                        gfx++,
                        1,
                        G_MWO_POINT_RGBA,
                        RgbaColor(red, green, blue, f_GreenGridAlpha)
                    );
                    if (drawnColumn != 0 &&
                        screenPoints[GridPointIndex(drawnRow, drawnColumn) - 1].isVisible == TRUE) {
                        gDPPipeSync(gfx++);
                        if (screenPoints[GridPointIndex(drawnRow, drawnColumn)].depth <
                            screenPoints[GridPointIndex(drawnRow, drawnColumn) - 1].depth) {
                            gDPSetPrimDepth(
                                gfx++,
                                screenPoints[GridPointIndex(drawnRow, drawnColumn)].depth,
                                0
                            );
                        }
                        else {
                            gDPSetPrimDepth(
                                gfx++,
                                screenPoints[GridPointIndex(drawnRow, drawnColumn) - 1].depth,
                                0
                            );
                        }

                        gDPPipeSync(gfx++);
                        gSP2Triangles(gfx++, 2, 1, 0, 0, 3, 2, 0, 0);
                    }
                }
            }

            if (rowCount != GREEN_GRID_ROW_COUNT_MAX) {
                drawnRow++;
            }
            else if (drawnRow >= GREEN_GRID_FIRST_FULL_ROW) {
                drawnRow++;
            }
            else {
                drawnRow += 2;
            }
        }

        for (drawnColumn = 0; drawnColumn != columnCount; drawnColumn++) {
            for (drawnRow = 0; drawnRow != rowCount - 1; drawnRow++) {
                s32 red;
                s32 green;
                s32 blue;
                s32 point = GridPointIndex(drawnRow, drawnColumn);

                if (screenPoints[point].isVisible != TRUE ||
                    screenPoints[point + columnCount].isVisible != TRUE) {
                    continue;
                }

                deltaX = screenPoints[point].x - screenPoints[point + columnCount].x;
                deltaY = screenPoints[point].y - screenPoints[point + columnCount].y;
                if (deltaX == 0.0f && deltaY == 0.0f) {
                    continue;
                }

                scale = GREEN_GRID_LINE_HALF_WIDTH / sqrtf((deltaX * deltaX) + (deltaY * deltaY));
                deltaX *= scale;
                deltaY *= scale;
                SetGreenGridTint(slopes[point]);
                gSPModifyVertex(
                    gfx++,
                    0,
                    G_MWO_POINT_RGBA,
                    RgbaColor(red, green, blue, f_GreenGridAlpha)
                );
                gSPModifyVertex(
                    gfx++,
                    1,
                    G_MWO_POINT_RGBA,
                    RgbaColor(red, green, blue, f_GreenGridAlpha)
                );
                SetGreenGridTint(slopes[point + columnCount]);
                gSPModifyVertex(
                    gfx++,
                    2,
                    G_MWO_POINT_RGBA,
                    RgbaColor(red, green, blue, f_GreenGridAlpha)
                );
                gSPModifyVertex(
                    gfx++,
                    3,
                    G_MWO_POINT_RGBA,
                    RgbaColor(red, green, blue, f_GreenGridAlpha)
                );
                gSPModifyVertex(
                    gfx++,
                    0,
                    G_MWO_POINT_XYSCREEN,
                    ScreenPointWord(screenPoints[point].x, screenPoints[point].y)
                );
                gSPModifyVertex(
                    gfx++,
                    1,
                    G_MWO_POINT_XYSCREEN,
                    ScreenPointWord(
                        screenPoints[point].x - (s32)deltaY,
                        screenPoints[point].y + (s32)deltaX
                    )
                );
                gSPModifyVertex(
                    gfx++,
                    2,
                    G_MWO_POINT_XYSCREEN,
                    ScreenPointWord(
                        screenPoints[point + columnCount].x,
                        screenPoints[point + columnCount].y
                    )
                );
                gSPModifyVertex(
                    gfx++,
                    3,
                    G_MWO_POINT_XYSCREEN,
                    ScreenPointWord(
                        screenPoints[point + columnCount].x - (s32)deltaY,
                        screenPoints[point + columnCount].y + (s32)deltaX
                    )
                );
                if (screenPoints[point].depth < screenPoints[point + columnCount].depth) {
                    gDPSetPrimDepth(gfx++, screenPoints[point].depth, 0);
                }
                else {
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
    gSPMatrix(
        gfx++,
        PhysicalAddress(&matrices->projection),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    *displayListHead = gfx;
}

void ResetBallTrail(void) {
    s32 pointIndex;

    f_BallTrailHead = 0;
    for (pointIndex = 0; pointIndex != BALL_TRAIL_LENGTH; pointIndex++) {
        f_BallTrail[pointIndex][AXIS_X] =
            g_TurnBallSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE;
        f_BallTrail[pointIndex][AXIS_Y] =
            g_TurnBallSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        f_BallTrail[pointIndex][AXIS_Z] =
            g_TurnBallSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    }
}

#define BALL_TRAIL_DROP 0.27f
#define BALL_TRAIL_DEPTH_BIAS 7
#define BALL_TRAIL_HIDDEN (-1)
#define BALL_TRAIL_HALF_WIDTH 4.0f
#define BALL_TRAIL_COLOR 0xFFFFFF00
#define BALL_TRAIL_ALPHA 64
#define BALL_TRAIL_SEGMENTS_PER_FADE 4
#define BallTrailSegmentColor(segment) \
    ((BALL_TRAIL_ALPHA - ((segment) / BALL_TRAIL_SEGMENTS_PER_FADE)) | BALL_TRAIL_COLOR)

void DrawBallTrail(Gfx **displayListHead, CameraMatrices *matrices) {
    Gfx *gfx = *displayListHead;
    ScreenPoint point;
    s32 pointIndex;
    s32 index;
    f32 deltaX;
    f32 deltaY;
    f32 scale;
    Gfx *cycleTypeCommand;

    if (g_BallTrailState != BALL_TRAIL_OFF && g_RoundHud.isBallTrailShown) {
        if (g_BallTrailState == BALL_TRAIL_RECORDING) {
            if (++f_BallTrailHead == BALL_TRAIL_LENGTH) {
                f_BallTrailHead = 0;
            }

            f_BallTrail[f_BallTrailHead][AXIS_X] =
                g_TurnBallSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE;
            f_BallTrail[f_BallTrailHead][AXIS_Y] =
                (g_TurnBallSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE) -
                (g_PinVertexScale * BALL_TRAIL_DROP);
            f_BallTrail[f_BallTrailHead][AXIS_Z] =
                g_TurnBallSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        }

        if (g_IsCupViewShown != TRUE) {
            index = f_BallTrailHead;
            for (pointIndex = 0; pointIndex != BALL_TRAIL_LENGTH; pointIndex++, index--) {
                if (index == -1) {
                    index = BALL_TRAIL_LENGTH - 1;
                }

                if (ProjectPoint(
                        f_BallTrail[index][AXIS_X],
                        f_BallTrail[index][AXIS_Y],
                        f_BallTrail[index][AXIS_Z],
                        &point
                    ) < 0) {
                    f_BallTrailPoints[pointIndex].x = point.x * QUARTER_PIXELS_PER_PIXEL;
                    f_BallTrailPoints[pointIndex].y = point.y * QUARTER_PIXELS_PER_PIXEL;
                    f_BallTrailPoints[pointIndex].depth = point.depth -
                        (point.depthStep * (g_PinVertexScale * BALL_TRAIL_DEPTH_BIAS));
                }
                else {
                    f_BallTrailPoints[pointIndex].depth = BALL_TRAIL_HIDDEN;
                }
            }

            guMtxIdent(&f_BallTrailMatrix);
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            cycleTypeCommand = gfx++;
            gDPSetCycleType(cycleTypeCommand, G_CYC_1CYCLE);
            gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
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
            gSPMatrix(
                gfx++,
                PhysicalAddress(&g_ScreenOrthoMatrix),
                G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
            );
            gSPMatrix(
                gfx++,
                PhysicalAddress(&f_BallTrailMatrix),
                G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW
            );
            gSPVertex(gfx++, f_BallTrailVertices, ARRAY_COUNT(f_BallTrailVertices), 0);
            for (pointIndex = 0; pointIndex != BALL_TRAIL_LENGTH - 1; pointIndex++) {
                if (f_BallTrailPoints[pointIndex].depth != BALL_TRAIL_HIDDEN &&
                    f_BallTrailPoints[pointIndex + 1].depth != BALL_TRAIL_HIDDEN) {
                    deltaX = f_BallTrailPoints[pointIndex].x - f_BallTrailPoints[pointIndex + 1].x;
                    deltaY = f_BallTrailPoints[pointIndex].y - f_BallTrailPoints[pointIndex + 1].y;

                    if (deltaX == 0.0f && deltaY == 0.0f) {
                    }
                    else {
                        scale =
                            BALL_TRAIL_HALF_WIDTH / sqrtf((deltaX * deltaX) + (deltaY * deltaY));
                        deltaX *= scale;
                        deltaY *= scale;
                        gDPPipeSync(gfx++);
                        gDPSetPrimDepth(gfx++, f_BallTrailPoints[pointIndex].depth, 0);
                        gDPPipeSync(gfx++);
                        gSPModifyVertex(
                            gfx++,
                            0,
                            G_MWO_POINT_RGBA,
                            BallTrailSegmentColor(pointIndex)
                        );
                        gSPModifyVertex(
                            gfx++,
                            1,
                            G_MWO_POINT_RGBA,
                            BallTrailSegmentColor(pointIndex)
                        );
                        gSPModifyVertex(
                            gfx++,
                            2,
                            G_MWO_POINT_RGBA,
                            BallTrailSegmentColor(pointIndex)
                        );
                        gSPModifyVertex(
                            gfx++,
                            3,
                            G_MWO_POINT_RGBA,
                            BallTrailSegmentColor(pointIndex)
                        );
                        gSPModifyVertex(
                            gfx++,
                            0,
                            G_MWO_POINT_XYSCREEN,
                            ScreenPointWord(
                                f_BallTrailPoints[pointIndex].x + (s32)deltaY,
                                f_BallTrailPoints[pointIndex].y - (s32)deltaX
                            )
                        );
                        gSPModifyVertex(
                            gfx++,
                            1,
                            G_MWO_POINT_XYSCREEN,
                            ScreenPointWord(
                                f_BallTrailPoints[pointIndex].x - (s32)deltaY,
                                f_BallTrailPoints[pointIndex].y + (s32)deltaX
                            )
                        );
                        gSPModifyVertex(
                            gfx++,
                            2,
                            G_MWO_POINT_XYSCREEN,
                            ScreenPointWord(
                                f_BallTrailPoints[pointIndex + 1].x + (s32)deltaY,
                                f_BallTrailPoints[pointIndex + 1].y - (s32)deltaX
                            )
                        );
                        gSPModifyVertex(
                            gfx++,
                            3,
                            G_MWO_POINT_XYSCREEN,
                            ScreenPointWord(
                                f_BallTrailPoints[pointIndex + 1].x - (s32)deltaY,
                                f_BallTrailPoints[pointIndex + 1].y + (s32)deltaX
                            )
                        );
                        gSP2Triangles(gfx++, 1, 0, 2, 0, 1, 2, 3, 0);
                    }
                }
            }

            gDPPipeSync(gfx++);
            gSPMatrix(
                gfx++,
                PhysicalAddress(&matrices->projection),
                G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
            );
            gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
            gDPPipeSync(gfx++);
            gDPSetDepthSource(gfx++, G_ZS_PIXEL);
            gDPPipeSync(gfx++);
            *displayListHead = gfx;
        }
    }
}

void DrawWindSpeed(Gfx **displayListHead, f32 x, f32 y) {
    Gfx *gfx = *displayListHead;
    s32 halfWidthRounded;
    s32 halfHeightRounded;
    s32 textureS;
    s32 textureT;
    s32 dsdx;
    s32 dtdy;
    s32 speed;
    u32 windSpeed;
    u64 unusedWindSpeed UNUSED;
    f32 halfWidth;
    f32 halfHeight;
    f32 centerX;
    f32 centerY;
    s32 roundedCenterX;
    s32 roundedCenterY;

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
            }
            else {
                glyphS = (*text - '0') * WIND_DIGIT_WIDTH;
                glyphWidth = WIND_DIGIT_WIDTH;
            }

            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(textX + offsetX),
                QuarterPixels(textY - WIND_TEXT_RAISE),
                QuarterPixels(textX + offsetX + glyphWidth),
                QuarterPixels(textY - WIND_TEXT_RAISE + WIND_FONT_HEIGHT),
                G_TX_RENDERTILE,
                TexelCoordinate(glyphS),
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    windSpeed = g_HoleSetup.conditions.windSpeed;
    if (windSpeed != 0) {
        unusedWindSpeed = windSpeed;
    }

    speed = (windSpeed * WIND_SPEED_TO_METERS_SCALE) / WIND_SPEED_TO_METERS_DIVISOR;
    f_WindIconPulseAngle += WIND_ICON_PULSE_STEP;
    if (speed >= WIND_SPEED_TWO_DIGITS) {
        halfWidth = sinf(f_WindIconPulseAngle) + WIND_ICON_WIDE_HALF_WIDTH;
    }
    else {
        halfWidth = sinf(f_WindIconPulseAngle) + WIND_ICON_NARROW_HALF_WIDTH;
    }

    halfHeight = sinf(f_WindIconPulseAngle) + WIND_ICON_HALF_HEIGHT;
    halfWidthRounded = halfWidth + 0.5f;
    halfHeightRounded = halfHeight + 0.5f;
    dsdx = TexelRunStep(WIND_ICON_TEXTURE_WIDTH / 2, halfWidth);
    dtdy = TexelRunStep(WIND_ICON_TEXTURE_HEIGHT / 2, halfHeight);

    textureS = HALF_TEXEL - ((halfWidthRounded - halfWidth) * HALF_TEXEL);
    textureT = HALF_TEXEL - ((halfHeightRounded - halfHeight) * HALF_TEXEL);

    if (speed >= WIND_SPEED_TWO_DIGITS) {
        centerX = (-sinf(g_SkyState.viewedWindAngles[0]) * WIND_WIDE_LEAN_X) + (x + WIND_OFFSET_X) +
            WIND_POSITIVE_BIAS;
    }
    else {
        centerX = (-sinf(g_SkyState.viewedWindAngles[0]) * WIND_NARROW_LEAN_X) +
            (x + WIND_OFFSET_X) + WIND_POSITIVE_BIAS;
    }

    centerY = (-cosf(g_SkyState.viewedWindAngles[0]) * WIND_LEAN_Y) + ((SCREEN_HEIGHT / 2) - y) -
        WIND_OFFSET_Y + WIND_POSITIVE_BIAS;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineLERP(
        gfx++,
        0,
        0,
        0,
        PRIMITIVE,
        0,
        0,
        0,
        TEXEL0,
        0,
        0,
        0,
        PRIMITIVE,
        0,
        0,
        0,
        TEXEL0
    );
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_WindIconImage)),
        G_IM_FMT_I,
        WIND_ICON_TEXTURE_WIDTH,
        WIND_ICON_TEXTURE_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    roundedCenterX = centerX + 0.5f;
    roundedCenterY = centerY + 0.5f;
    gSPScisTextureRectangle(
        gfx++,
        QuarterPixels(roundedCenterX - WIND_POSITIVE_BIAS + WIND_ICON_OFFSET_X - halfWidthRounded),
        QuarterPixels(roundedCenterY - WIND_POSITIVE_BIAS + WIND_ICON_OFFSET_Y - halfHeightRounded),
        QuarterPixels(roundedCenterX - WIND_POSITIVE_BIAS + WIND_ICON_OFFSET_X + halfWidthRounded),
        QuarterPixels(roundedCenterY - WIND_POSITIVE_BIAS + WIND_ICON_OFFSET_Y + halfHeightRounded),
        G_TX_RENDERTILE,
        textureS,
        textureT,
        dsdx,
        dtdy
    );
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
    gDPSetPrimColor(
        gfx++,
        0,
        0,
        WIND_TEXT_PRIM_RED,
        WIND_TEXT_PRIM_GREEN,
        WIND_TEXT_PRIM_BLUE,
        OPAQUE_ALPHA
    );
    gDPSetEnvColor(gfx++, WIND_TEXT_ENV_RED, WIND_TEXT_ENV_GREEN, WIND_TEXT_ENV_BLUE, OPAQUE_ALPHA);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_WindSpeedFont)));
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(f_WindSpeedFont)),
        G_IM_FMT_CI,
        WIND_FONT_WIDTH,
        WIND_FONT_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    sprintf(g_PrintBuffer, "%dm", speed);
    if (speed >= WIND_SPEED_TWO_DIGITS) {
        DrawWindSpeedText(
            centerX - WIND_POSITIVE_BIAS - WIND_WIDE_TEXT_SHIFT_X,
            centerY - WIND_POSITIVE_BIAS,
            g_PrintBuffer
        );
    }
    else {
        DrawWindSpeedText(
            centerX - WIND_POSITIVE_BIAS,
            centerY - WIND_POSITIVE_BIAS,
            g_PrintBuffer
        );
    }

    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    if (g_RoundHud.areWindSpeedArrowsShown == TRUE) {
        if (speed != 0) {
            DrawPauseArrows(
                &gfx,
                centerX - WIND_ARROWS_OFFSET_X,
                centerY - WIND_ARROWS_OFFSET_Y,
                WIND_ARROWS_WIDE_GAP
            );
        }
        else {
            DrawPauseArrows(
                &gfx,
                centerX - WIND_ARROWS_OFFSET_X,
                centerY - WIND_ARROWS_OFFSET_Y,
                WIND_ARROWS_NARROW_GAP
            );
        }
    }

    *displayListHead = gfx;
}

void DrawWindIndicator(Gfx **displayListHead, f32 x, f32 y) {
    Vec3 viewDirection;
    Vec3 rotation;
    Vec3 translation;
    Vec3 unusedScale UNUSED;
    Mat4 matrix;
    Mat4 transform;
    Gfx *gfx = *displayListHead;

    viewDirection[AXIS_X] = g_Cameras[0].view.lookAt[AXIS_X] - g_Cameras[0].view.eye[AXIS_X];
    viewDirection[AXIS_Z] = g_Cameras[0].view.lookAt[AXIS_Z] - g_Cameras[0].view.eye[AXIS_Z];
    viewDirection[AXIS_Y] = g_Cameras[0].view.lookAt[AXIS_Y] - g_Cameras[0].view.eye[AXIS_Y];
    rotation[AXIS_X] = atan2(
                           viewDirection[AXIS_Y],
                           sqrtf(
                               (viewDirection[AXIS_X] * viewDirection[AXIS_X]) +
                               (viewDirection[AXIS_Z] * viewDirection[AXIS_Z])
                           )
                       ) +
        M_PI + WIND_INDICATOR_TILT;
    rotation[AXIS_Y] = 0.0f;
    rotation[AXIS_Z] = 0.0f;
    translation[AXIS_X] = x;
    translation[AXIS_Y] = y;
    translation[AXIS_Z] = WIND_INDICATOR_DEPTH;
    BuildRotationMatrix(matrix, rotation, translation);
    guRotateRPYF(
        transform,
        0.0f,
        WIND_INDICATOR_FACING_DEGREES -
            (g_SkyState.viewedWindAngles[0] * DEGREES_PER_TURN * (1.0 / FULL_TURN_RADIANS)),
        0.0f
    );
    guMtxCatF(transform, matrix, matrix);
    guScaleF(transform, WIND_INDICATOR_SCALE, WIND_INDICATOR_SCALE, WIND_INDICATOR_SCALE);
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
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_WindIndicatorMatrices[g_MatrixBufferIndex]),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
    );
    gSPSetLights1(gfx++, f_WindArrowLights);
    if (g_IsModuleAlive[MODULE_WIND_BOO]) {
        g_ModuleRdpRequestFrames[MODULE_WIND_BOO] = g_FrameCount;
        gSPDisplayList(gfx++, g_BooDisplayList);
    }

    if (g_SkyState.viewedWindAngles[0] > M_PI / 2 &&
        g_SkyState.viewedWindAngles[0] < 3 * M_PI / 2) {
        DrawWindSpeed(&gfx, x, y);
    }

    *displayListHead = gfx;
}

void AdjustHudOffset(void) {
    if (IsGameFlagSet(GAME_FLAG_ADJUST_HUD_OFFSET)) {
        if (g_ControllerStates[HUD_OFFSET_CONTROLLER].pressedButtons & U_CBUTTONS) {
            f_HudOffsetY--;
        }

        if (g_ControllerStates[HUD_OFFSET_CONTROLLER].pressedButtons & D_CBUTTONS) {
            f_HudOffsetY++;
        }

        sprintf(g_PrintBuffer, "%3d", (int)f_HudOffsetY);
        PrintDebugText(g_PrintBuffer, HUD_OFFSET_TEXT_COLUMN, HUD_OFFSET_TEXT_ROW);
    }
}

#define SHOT_PANEL_WIND_INDICATOR_X 100.0f
#define SHOT_PANEL_WIND_INDICATOR_Y 76.0f
#define IsWindIndicatorDrawn() \
    (g_HoleSetup.conditions.windSpeed != 0 && \
        g_RoundHud.isWindIndicatorShown && \
        GetSurfaceTraits(g_CurrentShot.surface)->kind != SURFACE_KIND_GREEN && \
        !g_IsCupViewShown)
#define PAUSE_PANEL_ARROWS_GAP 64
#define CLUB_HEAD_ICON_X (-128.0f)
#define CLUB_HEAD_ICON_Y (-76.0f)
#define LIE_ICON_X 112
#define LIE_ICON_Y (-70.0f)
#define SHOT_PANEL_ICON_Z (-300.0f)
#define SHOT_PANEL_ICON_TILT 0.5f
#define SHOT_PANEL_ICON_DIM 191
#define CLUB_HEAD_IMAGE_SIZE 32
#define LIE_IMAGE_WIDTH 48
#define LIE_IMAGE_HALF_HEIGHT 24
#define LIE_IMAGE_HALF_SIZE (LIE_IMAGE_WIDTH * LIE_IMAGE_HALF_HEIGHT * 2)

void DrawShotPanelIcons(Gfx **displayListHead, CameraView *view UNUSED) {
    Vec3 rotation;
    Vec3 translation;
    Vec3 unusedScale UNUSED;
    Mat4 matrix;
    s32 unused[18] UNUSED;
    Gfx *gfx;
    Club *club;
    Radians clubHeadAngle;
    Radians lieAngle;

    gfx = *displayListHead;
    gSPMatrix(
        gfx++,
        PhysicalAddress(&g_ScreenOrthoMatrix),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(gfx++, G_SHADE | G_CULL_BACK);
    if (g_RoundHud.areWindSpeedArrowsShown == TRUE ||
        g_RoundHud.areWindDirectionArrowsShown == TRUE) {
        if (g_IsModuleAlive[MODULE_PAUSE_MENU]) {
            DrawWindIndicator(&gfx, g_WindPanelPosition[AXIS_X], g_WindPanelPosition[AXIS_Y]);
            if (g_RoundHud.areWindDirectionArrowsShown == TRUE) {
                DrawPauseArrows(
                    &gfx,
                    g_WindPanelPosition[AXIS_X] + (SCREEN_WIDTH / 2),
                    (SCREEN_HEIGHT / 2) - g_WindPanelPosition[AXIS_Y],
                    PAUSE_PANEL_ARROWS_GAP
                );
            }
        }
    }
    else if (IsWindIndicatorDrawn()) {
        DrawWindIndicator(
            &gfx,
            g_ShotPanelOffsetX + SHOT_PANEL_WIND_INDICATOR_X,
            SHOT_PANEL_WIND_INDICATOR_Y
        );
    }

    gSPMatrix(
        gfx++,
        PhysicalAddress(&g_ScreenOrthoMatrix),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    gDPPipeSync(gfx++);
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH | G_CULL_BACK);
    if (!g_IsCupViewShown && g_IsModuleAlive[MODULE_CLUB_PANEL]) {
        DrawClubPanelIfShown(&gfx);
    }

    if (g_RoundHud.isClubHeadIconShown && !g_IsCupViewShown) {
        club = GetClub(g_Players[g_CurrentShot.playerIndex].clubs[g_CurrentShot.clubSlot]);
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
            clubHeadAngle -= FULL_TURN_RADIANS;
        }

        rotation[AXIS_X] = 0;
        rotation[AXIS_Y] = 0;
        rotation[AXIS_Z] = -clubHeadAngle * SHOT_PANEL_ICON_TILT;
        translation[AXIS_X] = CLUB_HEAD_ICON_X - g_ShotPanelOffsetX;
        translation[AXIS_Y] = CLUB_HEAD_ICON_Y - f_HudOffsetY;
        translation[AXIS_Z] = SHOT_PANEL_ICON_Z;
        BuildRotationMatrix(matrix, rotation, translation);
        ConvertMatrixToFixed(matrix, &f_ClubHeadIconMatrices[g_MatrixBufferIndex]);
        gDPPipeSync(gfx++);
        gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        if (g_RoundHud.flags & HUD_FLAG_LIGHT_CLUB_HEAD) {
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
        }
        else {
            gDPSetPrimColor(
                gfx++,
                0,
                0,
                SHOT_PANEL_ICON_DIM,
                SHOT_PANEL_ICON_DIM,
                SHOT_PANEL_ICON_DIM,
                OPAQUE_ALPHA
            );
        }

        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(ImageBody(f_ClubHeadImage)),
            G_IM_FMT_RGBA,
            G_IM_SIZ_16b,
            CLUB_HEAD_IMAGE_SIZE,
            CLUB_HEAD_IMAGE_SIZE,
            0,
            G_TX_WRAP,
            G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_ClubHeadIconMatrices[g_MatrixBufferIndex]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
        );
        gSPVertex(gfx++, f_ClubHeadIconVertices, QUAD_VERTEX_COUNT, 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        gDPPipeSync(gfx++);
    }

    if ((g_RoundHud.isLieIconShown || g_RoundHud.areLieArrowsShown) && !g_IsCupViewShown) {
        lieAngle = g_RoundHud.lieTilt * BINARY_ANGLE_TO_RADIANS;
        if (lieAngle > M_PI) {
            lieAngle -= FULL_TURN_RADIANS;
        }

        rotation[AXIS_X] = 0;
        rotation[AXIS_Y] = 0;
        rotation[AXIS_Z] = lieAngle * SHOT_PANEL_ICON_TILT;
        if (g_RoundHud.isLieIconShown) {
            translation[AXIS_X] = g_ShotPanelOffsetX + LIE_ICON_X;
            translation[AXIS_Y] = LIE_ICON_Y - f_HudOffsetY;
            translation[AXIS_Z] = SHOT_PANEL_ICON_Z;
        }
        else if (g_IsModuleAlive[MODULE_PAUSE_MENU]) {
            translation[AXIS_X] = g_LiePanelPosition[AXIS_X];
            translation[AXIS_Y] = g_LiePanelPosition[AXIS_Y];
            translation[AXIS_Z] = SHOT_PANEL_ICON_Z;
            DrawPauseArrows(
                &gfx,
                g_LiePanelPosition[AXIS_X] + (SCREEN_WIDTH / 2),
                (SCREEN_HEIGHT / 2) - g_LiePanelPosition[AXIS_Y],
                PAUSE_PANEL_ARROWS_GAP
            );
        }

        BuildRotationMatrix(matrix, rotation, translation);
        ConvertMatrixToFixed(matrix, &f_LieIconMatrices[g_MatrixBufferIndex]);
        if (g_RoundHud.flags & HUD_FLAG_ANIMATE_OFFSET_MARKER) {
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
        }
        else {
            gDPSetPrimColor(
                gfx++,
                0,
                0,
                SHOT_PANEL_ICON_DIM,
                SHOT_PANEL_ICON_DIM,
                SHOT_PANEL_ICON_DIM,
                OPAQUE_ALPHA
            );
        }

        gDPPipeSync(gfx++);
        gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_LieImage)));
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_LieIconMatrices[g_MatrixBufferIndex]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
        );
        gSPVertex(gfx++, f_LieIconVertices, ARRAY_COUNT(f_LieIconVertices), 0);
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(ImageBody(f_LieImage)),
            G_IM_FMT_RGBA,
            G_IM_SIZ_16b,
            LIE_IMAGE_WIDTH,
            LIE_IMAGE_HALF_HEIGHT,
            0,
            G_TX_WRAP,
            G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(ImageBody(f_LieImage) + LIE_IMAGE_HALF_SIZE),
            G_IM_FMT_RGBA,
            G_IM_SIZ_16b,
            LIE_IMAGE_WIDTH,
            LIE_IMAGE_HALF_HEIGHT,
            0,
            G_TX_WRAP,
            G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSP2Triangles(gfx++, 4, 5, 6, 0, 4, 6, 7, 0);
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void DrawHoleFlag(Boolean isBackDrawn) {
    s32 row;
    s32 column;
    s32 index;
    Vtx *vertex;
    s32 vertexIndex;

    gDPPipeSync(g_DisplayListHead++);
    gDPSetCombineMode(g_DisplayListHead++, G_CC_MODULATERGBA, G_CC_MODULATERGBA);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
    gDPSetPrimColor(g_DisplayListHead++, 0, 0, 0, 0, 255, OPAQUE_ALPHA);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetAlphaCompare(g_DisplayListHead++, G_AC_NONE);
    gDPSetBlendColor(g_DisplayListHead++, 255, 255, 255, 1);
    gDPLoadTLUT_pal256(g_DisplayListHead++, TextureAddress(ImageBody(f_HoleImage)));
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTextureLUT(g_DisplayListHead++, G_TT_RGBA16);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
    gSPTexture(
        g_DisplayListHead++,
        TEXTURE_SCALE_HALF,
        TEXTURE_SCALE_HALF,
        0,
        G_TX_RENDERTILE,
        G_ON
    );
    gDPLoadTextureBlock(
        g_DisplayListHead++,
        TextureAddress(LargeImageTexels(f_HoleImage)),
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        HOLE_IMAGE_WIDTH,
        HOLE_IMAGE_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(g_DisplayListHead++);
    gSPVertex(
        g_DisplayListHead++,
        f_HoleFlagVertices[0][g_MatrixBufferIndex],
        HOLE_FLAG_VERTEX_COUNT,
        0
    );
    for (row = 0; row != HOLE_FLAG_ROW_COUNT - 1; row++) {
        for (column = 0; column != HOLE_FLAG_COLUMN_COUNT - 1; column++) {
            index = (row * HOLE_FLAG_COLUMN_COUNT) + column;
            gSP2Triangles(
                g_DisplayListHead++,
                index,
                index + HOLE_FLAG_COLUMN_COUNT,
                index + HOLE_FLAG_COLUMN_COUNT + 1,
                0,
                index,
                index + HOLE_FLAG_COLUMN_COUNT + 1,
                index + 1,
                0
            );
        }
    }

    if (isBackDrawn == TRUE) {
        vertexIndex = 0;
        vertex = f_HoleFlagVertices[0][g_MatrixBufferIndex];
        for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
            for (column = 0; column != HOLE_FLAG_COLUMN_COUNT; column++, vertexIndex++, vertex++) {
                gSPModifyVertex(
                    g_DisplayListHead++,
                    vertexIndex,
                    G_MWO_POINT_ST,
                    TextureCoordinateWord(
                        (HOLE_FLAG_BACK_TEXEL_START / 2) -
                            (column * (HOLE_FLAG_TEXEL_COLUMN_STEP / 2)),
                        row * (HOLE_FLAG_TEXEL_ROW_STEP / 2)
                    )
                );
                gSPModifyVertex(
                    g_DisplayListHead++,
                    vertexIndex,
                    G_MWO_POINT_RGBA,
                    RgbaColor(
                        2 * HOLE_FLAG_START_SHADE - vertex->v.cn[0],
                        2 * HOLE_FLAG_START_SHADE - vertex->v.cn[1],
                        2 * HOLE_FLAG_START_SHADE - vertex->v.cn[2],
                        HOLE_FLAG_START_SHADE
                    )
                );
            }
        }

        for (row = 0; row != HOLE_FLAG_ROW_COUNT - 1; row++) {
            for (column = 0; column != HOLE_FLAG_COLUMN_COUNT - 1; column++) {
                index = (row * HOLE_FLAG_COLUMN_COUNT) + column;
                gSP2Triangles(
                    g_DisplayListHead++,
                    index,
                    index + HOLE_FLAG_COLUMN_COUNT + 1,
                    index + HOLE_FLAG_COLUMN_COUNT,
                    0,
                    index,
                    index + 1,
                    index + HOLE_FLAG_COLUMN_COUNT + 1,
                    0
                );
            }
        }
    }
}

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
    gSPClearGeometryMode(g_DisplayListHead++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(g_DisplayListHead++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetRenderMode(g_DisplayListHead++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
    gSPPerspNormalize(g_DisplayListHead++, g_ScreenEffectPerspectiveNormalization);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
    gSPMatrix(
        g_DisplayListHead++,
        PhysicalAddress(&g_ScreenEffectProjection),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    gSPMatrix(
        g_DisplayListHead++,
        PhysicalAddress(&f_HoleImageMatrix),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
    );
    DrawHoleFlag(FALSE);
    gDPPipeSync(g_DisplayListHead++);
}

void StartShotEffect(ShotEffect effect) {
    ContactStyle contactStyle;

    if (effect == SHOT_EFFECT_NONE) {
        f_ShotEffectTime = SHOT_EFFECT_TIME_OFF;
        return;
    }

    if (effect == SHOT_EFFECT_LONG_SHOT) {
        contactStyle = GetContactStyle(&g_Players[g_RoundHud.shooterIndex]);
        if (ShowsLongShotEffect(contactStyle)) {
            PlaySoundWithEffects(
                SOUND_LONG_SHOT,
                LONG_SHOT_SOUND_CHANNEL,
                LONG_SHOT_SOUND_VOLUME,
                LONG_SHOT_SOUND_REVERB,
                SOUND_FULL_SCALE
            );
        }
        else {
            PlaySoundWithEffects(
                SOUND_SPARK_TRAIL,
                LONG_SHOT_SOUND_CHANNEL,
                SPARK_TRAIL_SOUND_VOLUME,
                LONG_SHOT_SOUND_REVERB,
                SOUND_FULL_SCALE
            );
        }
    }

    f_ShotEffect = effect;
    f_ShotEffectTime = 0.0f;
}

#define SHOT_EFFECT_TIME_STEP 0.2f
#define SHOT_EFFECT_SCALE 0.5f
#define SHOT_EFFECT_SPIN_START 0.7f
#define SHOT_EFFECT_SPIN_RATE 0.5f
#define SHOT_EFFECT_Z (-100.0f)
#define SPARK_TRAIL_SCALE 0.4f
#define SPARK_TRAIL_FRAME_TICKS 4
#define SPARK_TRAIL_FRAMES_OFFSET 0x970
#define SPARK_TRAIL_FRAME_SIZE 0xC08
#define SPARK_TRAIL_FRAME_WIDTH 48
#define SPARK_TRAIL_FRAME_HEIGHT 128
#define SPARK_TRAIL_ALPHA 191
#define SPARK_TRAIL_QUAD_COUNT 2

void DrawShotEffect(Gfx **displayListHead) {
    Mat4 matrix;
    ScreenPoint point;
    EncodedAsset asset;
    Gfx *gfx = *displayListHead;
    ContactStyle contactStyle;
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

    if (ProjectPoint(
            g_TurnBallSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE,
            g_TurnBallSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE,
            g_TurnBallSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE,
            &point
        ) >= 0) {
        return;
    }

    contactStyle = GetContactStyle(&g_Players[g_RoundHud.shooterIndex]);
    if (ShowsLongShotEffect(contactStyle)) {
        scale = sinf(f_ShotEffectTime) * SHOT_EFFECT_SCALE;
        f_ShotEffectRotation[AXIS_X] = 0.0f;
        f_ShotEffectRotation[AXIS_Y] = 0.0f;
        f_ShotEffectRotation[AXIS_Z] =
            (f_ShotEffectTime + SHOT_EFFECT_SPIN_START) * SHOT_EFFECT_SPIN_RATE;
    }
    else {
        scale = sinf(f_ShotEffectTime) * SPARK_TRAIL_SCALE;
        f_ShotEffectRotation[AXIS_X] = 0.0f;
        f_ShotEffectRotation[AXIS_Y] = 0.0f;
        f_ShotEffectRotation[AXIS_Z] = DegreesToRadians(guRandom() % DEGREES_PER_TURN);
    }

    f_ShotEffectScale[AXIS_X] = scale;
    f_ShotEffectScale[AXIS_Y] = scale;
    f_ShotEffectScale[AXIS_Z] = scale;
    f_ShotEffectTranslation[AXIS_X] = point.x - (SCREEN_WIDTH / 2);
    f_ShotEffectTranslation[AXIS_Y] = -point.y + (SCREEN_HEIGHT / 2);
    f_ShotEffectTranslation[AXIS_Z] = SHOT_EFFECT_Z;
    BuildTransformMatrix(matrix, f_ShotEffectRotation, f_ShotEffectTranslation, f_ShotEffectScale);
    ConvertMatrixToFixed(matrix, &f_ShotEffectMatrices[g_MatrixBufferIndex]);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
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
    if (ShowsLongShotEffect(contactStyle)) {
        gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_OFF);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPSetPrimColor(gfx++, 0, 0, 0, 0, 255, OPAQUE_ALPHA);
        gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
        if (f_ShotEffect == SHOT_EFFECT_LONG_SHOT) {
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        }
        else {
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
        }

        gDPPipeSync(gfx++);
        gSPMatrix(
            gfx++,
            PhysicalAddress(&g_ScreenOrthoMatrix),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_ShotEffectMatrices[g_MatrixBufferIndex]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
        );
        gSPVertex(gfx++, f_LongShotVertices, LONG_SHOT_GROUP_COUNT * 8, 0);
        for (i = 0; i != LONG_SHOT_GROUP_COUNT; i++) {
            gSP2Triangles(
                gfx++,
                i * 8,
                (i * 8) + 1,
                (i * 8) + 2,
                0,
                i * 8,
                (i * 8) + 2,
                (i * 8) + 3,
                0
            );
            gSP2Triangles(
                gfx++,
                (i * 8) + 1,
                (i * 8) + 4,
                (i * 8) + 5,
                0,
                (i * 8) + 1,
                (i * 8) + 5,
                (i * 8) + 2,
                0
            );
            gSP2Triangles(
                gfx++,
                (i * 8) + 4,
                (i * 8) + 6,
                (i * 8) + 7,
                0,
                (i * 8) + 4,
                (i * 8) + 7,
                (i * 8) + 5,
                0
            );
        }
    }
    else {
        if (++f_SparkTrailFrameTime == SPARK_TRAIL_FRAME_TICKS) {
            f_SparkTrailFrameTime = 0;
            f_SparkTrailFrame = (f_SparkTrailFrame + 1) % SPARK_TRAIL_FRAME_COUNT;
            OpenEncodedAsset(f_SparkTrailFrameAssets[f_SparkTrailFrame], &asset);
            ReadEncodedAsset(
                g_SharedEffectImages + SPARK_TRAIL_FRAMES_OFFSET +
                    (f_SparkTrailBufferIndex * SPARK_TRAIL_FRAME_SIZE),
                &asset
            );
            f_SparkTrailBufferIndex ^= 1;
        }

        gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_POINT);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        if (contactStyle == CONTACT_STYLE_GAME_BOY_BALL_7) {
            gDPSetEnvColor(gfx++, 255, 255, 127, OPAQUE_ALPHA);
            gDPSetPrimColor(
                gfx++,
                0,
                0,
                guRandom() % 255,
                guRandom() % 255,
                guRandom() % 255,
                SPARK_TRAIL_ALPHA
            );
        }
        else {
            gDPSetPrimColor(gfx++, 0, 0, 191, 0, 191, SPARK_TRAIL_ALPHA);
            gDPSetEnvColor(gfx++, 255, 255, 255, OPAQUE_ALPHA);
        }

        gDPSetCombineLERP(
            gfx++,
            ENVIRONMENT,
            PRIMITIVE,
            TEXEL0,
            PRIMITIVE,
            PRIMITIVE,
            0,
            TEXEL0,
            0,
            ENVIRONMENT,
            PRIMITIVE,
            TEXEL0,
            PRIMITIVE,
            PRIMITIVE,
            0,
            TEXEL0,
            0
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&g_ScreenOrthoMatrix),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_ShotEffectMatrices[g_MatrixBufferIndex]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
        );
        gSPVertex(gfx++, f_SparkTrailVertices, ARRAY_COUNT(f_SparkTrailVertices), 0);
        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(ImageBody(g_SharedEffectImages + SPARK_TRAIL_FRAMES_OFFSET)),
            G_IM_FMT_I,
            SPARK_TRAIL_FRAME_WIDTH,
            SPARK_TRAIL_FRAME_HEIGHT,
            0,
            G_TX_WRAP,
            G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        for (i = 0; i != SPARK_TRAIL_QUAD_COUNT; i++) {
            gSP2Triangles(
                gfx++,
                i * QUAD_VERTEX_COUNT,
                (i * QUAD_VERTEX_COUNT) + 1,
                (i * QUAD_VERTEX_COUNT) + 2,
                0,
                i * QUAD_VERTEX_COUNT,
                (i * QUAD_VERTEX_COUNT) + 2,
                (i * QUAD_VERTEX_COUNT) + 3,
                0
            );
        }

        gDPPipeSync(gfx++);
        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(
                ImageBody(g_SharedEffectImages + SPARK_TRAIL_FRAMES_OFFSET + SPARK_TRAIL_FRAME_SIZE)
            ),
            G_IM_FMT_I,
            SPARK_TRAIL_FRAME_WIDTH,
            SPARK_TRAIL_FRAME_HEIGHT,
            0,
            G_TX_WRAP,
            G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        for (i = 0; i != SPARK_TRAIL_QUAD_COUNT; i++) {
            gSP2Triangles(
                gfx++,
                i * QUAD_VERTEX_COUNT,
                (i * QUAD_VERTEX_COUNT) + 1,
                (i * QUAD_VERTEX_COUNT) + 2,
                0,
                i * QUAD_VERTEX_COUNT,
                (i * QUAD_VERTEX_COUNT) + 2,
                (i * QUAD_VERTEX_COUNT) + 3,
                0
            );
        }

        gDPPipeSync(gfx++);
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

// factor and otherFactor are reused in turn as the flag's column and row
// skew, the shares of a cell's shade taken from its neighbor and kept
// from itself, the swing amplitude (factor only) and the bulge strength
// and scale; giving each use its own variable breaks the match.
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
    f32 calmOwnShare;

    ScrollWindArrow();
    windSpeed = g_HoleSetup.conditions.windSpeed / WIND_SPEED_UNIT;
    factor = HOLE_FLAG_SKEW;
    calmOwnShare = 1.0f - HOLE_FLAG_CALM_SPREAD;
    otherFactor = factor;
    for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
        for (column = 1; column != HOLE_FLAG_COLUMN_COUNT; column++) {
            HOLE_FLAG_VERTEX(row, column).v.ob[AXIS_X] =
                (s16)((column * HOLE_FLAG_CELL_SIZE) - (column * factor)) -
                (row * otherFactor * HOLE_FLAG_ROW_SKEW_SCALE);
            if (HOLE_FLAG_VERTEX(row, column).v.ob[AXIS_X] < column * otherFactor) {
                HOLE_FLAG_VERTEX(row, column).v.ob[AXIS_X] = column * otherFactor;
            }

            HOLE_FLAG_VERTEX(row, column).v.ob[AXIS_Y] =
                (-row * HOLE_FLAG_CELL_SIZE) - (column * factor);
        }
    }

    if (windSpeed > 0.0f && windSpeed < HOLE_FLAG_WIND_SPEED_MIN) {
        windSpeed = HOLE_FLAG_WIND_SPEED_MIN;
    }

    if (windSpeed < 1.0f) {
        factor = HOLE_FLAG_CALM_SPREAD;
        otherFactor = calmOwnShare;
    }
    else if (windSpeed > HOLE_FLAG_GALE_WIND_SPEED) {
        factor = 1.0f;
        otherFactor = 0.0f;
    }
    else {
        factor = (windSpeed * HOLE_FLAG_SPREAD_STEP) + HOLE_FLAG_SPREAD_STEP;
        otherFactor = ((HOLE_FLAG_SPREAD_STEP_COUNT - windSpeed) * HOLE_FLAG_SPREAD_STEP) -
            HOLE_FLAG_SPREAD_STEP;
    }

    for (column = HOLE_FLAG_COLUMN_COUNT - 1; column != 0; column--) {
        for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
            shade = (f_HoleFlagShades[(row * HOLE_FLAG_COLUMN_COUNT) + column - 1] * factor) +
                (HOLE_FLAG_SHADE(row, column) * otherFactor);
            HOLE_FLAG_SHADE(row, column) = shade;
        }
    }

    f_HoleFlagWavePhase +=
        ((HOLE_FLAG_SWING_DEGREES_PER_SPEED * windSpeed) + HOLE_FLAG_SWING_DEGREES_MIN) *
        DegreesToRadians(1);
    if (f_HoleFlagWavePhase > FULL_TURN_RADIANS) {
        f_HoleFlagWavePhase -= FULL_TURN_RADIANS;
    }

    if (f_HoleFlagWavePhase < 0.0f) {
        f_HoleFlagWavePhase += FULL_TURN_RADIANS;
    }

    factor = windSpeed + (rand() % HOLE_FLAG_SWING_JITTER_RANGE);
    shade = (s32)(sinf(f_HoleFlagWavePhase) * factor) + HOLE_FLAG_START_SHADE;
    for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
        if ((s32)windSpeed == 0) {
            rowShade = shade;
        }
        else {
            rowShade = (f32)(shade + (rand() % (s32)(windSpeed * HOLE_FLAG_ROW_JITTER_PER_SPEED))) -
                (HOLE_FLAG_ROW_JITTER_PER_SPEED / 2 * windSpeed);
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

    factor = (windSpeed * HOLE_FLAG_BULGE_PER_SPEED) + HOLE_FLAG_BULGE_MIN;
    for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
        for (column = 1; column != HOLE_FLAG_COLUMN_COUNT; column++) {
            bulge = (f32)(HOLE_FLAG_VERTEX(row, column).v.cn[0] - HOLE_FLAG_START_SHADE) /
                (f32)(HOLE_FLAG_BULGE_COLUMN_BASE - column) * factor;
            otherFactor = HOLE_FLAG_BULGE_SCALE;
            HOLE_FLAG_VERTEX(row, column).v.ob[AXIS_Z] = bulge * otherFactor *
                (f32)(HOLE_FLAG_BULGE_ROW_BASE - row) * HOLE_FLAG_BULGE_ROW_SCALE;
        }
    }

    for (row = 0; row != HOLE_FLAG_ROW_COUNT; row++) {
        pull = 0;
        for (column = HOLE_FLAG_FIRST_PULLED_COLUMN; column != HOLE_FLAG_COLUMN_COUNT; column++) {
            depth = HOLE_FLAG_VERTEX(row, column).v.ob[AXIS_Z];
            if (depth < 0) {
                depth = -depth;
            }

            pull += depth / HOLE_FLAG_DEPTH_PER_PULL;
            HOLE_FLAG_VERTEX(row, column).v.ob[AXIS_X] = (column * HOLE_FLAG_CELL_SIZE) - pull;
        }
    }
}

void ResetShotOffsetMarker(void) {
    s32 markerIndex;

    for (markerIndex = 0; markerIndex != SHOT_OFFSET_MARKER_COUNT; markerIndex++) {
        f_ShotOffsetMarkerXs[markerIndex] = 0;
        f_ShotOffsetMarkerYs[markerIndex] = 0;
    }
}

void SetShotOffsetMarker(s32 x, s32 y) {
    f32 length;
    f32 scale;

    if (x == 0 && y == 0) {
        length = sqrtf((x * x) + (y * y));
    }
    else {
        length = 0.0f;
    }

    if (length > SHOT_OFFSET_RADIUS_MAX) {
        scale = SHOT_OFFSET_RADIUS_MAX / length;
        x = x * scale;
        y = y * scale;
    }

    f_ShotOffsetMarkerXs[0] = x / SHOT_OFFSET_UNITS_PER_PIXEL;
    f_ShotOffsetMarkerYs[0] = -y / SHOT_OFFSET_UNITS_PER_PIXEL;
}

void DrawShotOffsetMarker(void) {
    gDPPipeSync(g_DisplayListHead++);
    gDPLoadTLUT_pal16(g_DisplayListHead++, 0, TextureAddress(ImageBody(f_ShotOffsetMarkerImage)));
    f_ShotOffsetMarkerFrame = (f_ShotOffsetMarkerFrame + 1) % SHOT_OFFSET_MARKER_FRAME_COUNT;
    if (g_RoundHud.flags & HUD_FLAG_ANIMATE_OFFSET_MARKER) {
        f_ShotOffsetMarkerTexelOffset = f_ShotOffsetMarkerFrame * SHOT_OFFSET_MARKER_FRAME_SIZE;
    }
    else {
        f_ShotOffsetMarkerTexelOffset =
            SHOT_OFFSET_MARKER_STILL_FRAME * SHOT_OFFSET_MARKER_FRAME_SIZE;
    }

    gDPLoadTextureBlock_4b(
        g_DisplayListHead++,
        TextureAddress(ImageTexels(f_ShotOffsetMarkerImage) + f_ShotOffsetMarkerTexelOffset),
        G_IM_FMT_CI,
        SHOT_OFFSET_MARKER_SIZE,
        SHOT_OFFSET_MARKER_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(g_DisplayListHead++);
    gSPScisTextureRectangle(
        g_DisplayListHead++,
        QuarterPixels(SHOT_OFFSET_MARKER_X + f_ShotOffsetMarkerXs[0] + g_ShotPanelOffsetX),
        QuarterPixels(
            SHOT_OFFSET_MARKER_Y + f_ShotOffsetMarkerYs[0] + f_ShotPanelOffsetY + f_HudOffsetY
        ),
        QuarterPixels(
            SHOT_OFFSET_MARKER_X + SHOT_OFFSET_MARKER_SIZE + f_ShotOffsetMarkerXs[0] +
            g_ShotPanelOffsetX
        ),
        QuarterPixels(
            SHOT_OFFSET_MARKER_Y + SHOT_OFFSET_MARKER_SIZE + f_ShotOffsetMarkerYs[0] +
            f_ShotPanelOffsetY + f_HudOffsetY
        ),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
}

#define ANIMATED_ICON_FRAME_DURATION 4
#define ANIMATED_ICON_SIZE 32
#define ANIMATED_ICON_X 96.0f
#define ANIMATED_ICON_Y 24.0f
#define ANIMATED_ICON_EASING 0.2f
#define ANIMATED_ICON_LABEL_WIDTH 64
#define ANIMATED_ICON_LABEL_HEIGHT 16
#define ANIMATED_ICON_LABEL_OFFSET_X 24
#define ANIMATED_ICON_LABEL_JUMP 0
#define ANIMATED_ICON_LABEL_MODE 1
#define ANIMATED_ICON_LABEL_CUP 2

void DrawAnimatedIcon(Gfx **displayListHead, s32 letterboxHeight) {
    Gfx *gfx = *displayListHead;
    EncodedAsset asset;
    s32 label;

    if (f_SceneryState == SCENERY_LOADED) {
        if (letterboxHeight == LETTERBOX_GROWTH_STEP) {
            f_AnimatedIconX = 0.0f;
            f_AnimatedIconY = 0.0f;
        }
        else {
            f_AnimatedIconX += (ANIMATED_ICON_X - f_AnimatedIconX) * ANIMATED_ICON_EASING;
            f_AnimatedIconY += (ANIMATED_ICON_Y - f_AnimatedIconY) * ANIMATED_ICON_EASING;
        }

        if (++f_AnimatedIconFrameTime == ANIMATED_ICON_FRAME_DURATION) {
            f_AnimatedIconFrameTime = 0;
            f_AnimatedIconFrame = (f_AnimatedIconFrame + 1) % ANIMATED_ICON_FRAME_COUNT;
            OpenEncodedAsset(f_AnimatedIconFrameAssets[f_AnimatedIconFrame], &asset);
            ReadEncodedAsset(f_AnimatedIconImages[f_AnimatedIconImageIndex], &asset);
        }

        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal256(
            gfx++,
            TextureAddress(ImageBody(f_AnimatedIconImages[f_AnimatedIconImageIndex]))
        );
        gDPPipeSync(gfx++);
        gDPSetRenderMode(
            gfx++,
            OPAQUE_ALPHA_COVERAGE_RENDER_MODE,
            OPAQUE_ALPHA_COVERAGE_RENDER_MODE2
        );
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(LargeImageTexels(f_AnimatedIconImages[f_AnimatedIconImageIndex])),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            ANIMATED_ICON_SIZE,
            ANIMATED_ICON_SIZE,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels((s32)f_AnimatedIconX),
            QuarterPixels((s32)f_AnimatedIconY),
            QuarterPixels((s32)f_AnimatedIconX + ANIMATED_ICON_SIZE),
            QuarterPixels((s32)f_AnimatedIconY + ANIMATED_ICON_SIZE),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
        if (g_LetterboxState == LETTERBOX_STATE_MOVING_CAMERA) {
            label = ANIMATED_ICON_LABEL_MODE;
        }
        else if (g_LetterboxState == LETTERBOX_STATE_LOOKING_DOWN_RANGE) {
            label = ANIMATED_ICON_LABEL_JUMP;
        }
        else {
            label = ANIMATED_ICON_LABEL_CUP;
        }

        gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_AnimatedIconLabelImages[label])));
        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(ImageTexels(f_AnimatedIconLabelImages[label])),
            G_IM_FMT_CI,
            ANIMATED_ICON_LABEL_WIDTH,
            ANIMATED_ICON_LABEL_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels((s32)f_AnimatedIconX + ANIMATED_ICON_LABEL_OFFSET_X),
            QuarterPixels((s32)f_AnimatedIconY),
            QuarterPixels(
                (s32)f_AnimatedIconX + ANIMATED_ICON_LABEL_OFFSET_X + ANIMATED_ICON_LABEL_WIDTH
            ),
            QuarterPixels((s32)f_AnimatedIconY + ANIMATED_ICON_LABEL_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        *displayListHead = gfx;
    }
}

s32 DrawLetterbox(Gfx **displayListHead, s32 height) {
    Gfx *gfx = *displayListHead;

    if (g_RoundHud.isLetterboxShown) {
        if (IsLetterboxState(g_LetterboxState)) {
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_XLU_SURF | ZMODE_XLU, G_RM_XLU_SURF2 | ZMODE_XLU);
            gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
            gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, OPAQUE_ALPHA);
            gDPPipeSync(gfx++);
            if (height > 0) {
                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels(HUD_MARGIN),
                    QuarterPixels(HUD_MARGIN),
                    QuarterPixels(SCREEN_WIDTH - HUD_MARGIN),
                    QuarterPixels(HUD_MARGIN + height),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels(HUD_MARGIN),
                    QuarterPixels(SCREEN_HEIGHT - HUD_MARGIN - height),
                    QuarterPixels(SCREEN_WIDTH - HUD_MARGIN),
                    QuarterPixels(SCREEN_HEIGHT - HUD_MARGIN),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
            }

            gDPPipeSync(gfx++);
            height += LETTERBOX_GROWTH_STEP;
            if (height > LETTERBOX_HEIGHT_MAX) {
                height = LETTERBOX_HEIGHT_MAX;
            }
        }
        else {
            height = 0;
        }
    }

    *displayListHead = gfx;
    return height;
}

void DrawDistanceText(Gfx **displayListHead, s32 x, s32 y, char *text) {
    s32 width;
    s32 glyphS;
    Gfx *gfx = *displayListHead;

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
                glyphS = DISTANCE_METERS_S;
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

        gSPTextureRectangle(
            gfx++,
            QuarterPixels(x),
            QuarterPixels(y),
            QuarterPixels(x + width),
            QuarterPixels(y + DISTANCE_GLYPH_HEIGHT),
            G_TX_RENDERTILE,
            TexelCoordinate(glyphS),
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        x += width;
    }

    *displayListHead = gfx;
}

const char f_ColorTestSampleText[] = "ｻﾝﾌﾟﾙABCDEFG";
const char f_BlankText[] = " ";

void DrawColorTest(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    SetUpGradientTextDrawing(&gfx, f_IsTestGradientShown);
    gDPSetPrimColor(
        gfx++,
        0,
        0,
        f_TestColors[COLOR_TEST_PRIM][0],
        f_TestColors[COLOR_TEST_PRIM][1],
        f_TestColors[COLOR_TEST_PRIM][2],
        COLOR_CHANNEL_MAX
    );
    gDPSetEnvColor(
        gfx++,
        f_TestColors[COLOR_TEST_ENV][0],
        f_TestColors[COLOR_TEST_ENV][1],
        f_TestColors[COLOR_TEST_ENV][2],
        COLOR_CHANNEL_MAX
    );
    gDPPipeSync(gfx++);

    DrawCenteredLargeText(&gfx, f_TestTextX, f_TestTextY, f_ColorTestSampleText);
    gDPPipeSync(gfx++);
    PrintDebugText(
        f_BlankText,
        (f_TestChannelIndex * COLOR_TEST_COLUMN_WIDTH) + COLOR_TEST_TEXT_COLUMN - 1,
        f_TestColorIndex + COLOR_TEST_TEXT_ROW
    );
    if (g_ControllerStates[COLOR_TEST_CONTROLLER].repeatedButtons & U_CBUTTONS) {
        f_TestColorIndex ^= 1;
    }

    if (g_ControllerStates[COLOR_TEST_CONTROLLER].repeatedButtons & D_CBUTTONS) {
        f_TestColorIndex ^= 1;
    }

    if (g_ControllerStates[COLOR_TEST_CONTROLLER].repeatedButtons & L_CBUTTONS) {
        f_TestChannelIndex =
            (f_TestChannelIndex + COLOR_TEST_CHANNEL_COUNT - 1) % COLOR_TEST_CHANNEL_COUNT;
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

    sprintf(
        g_PrintBuffer,
        "%3d  %3d  %3d",
        (int)f_TestColors[COLOR_TEST_PRIM][0],
        (int)f_TestColors[COLOR_TEST_PRIM][1],
        (int)f_TestColors[COLOR_TEST_PRIM][2]
    );
    PrintDebugText(g_PrintBuffer, COLOR_TEST_TEXT_COLUMN, COLOR_TEST_TEXT_ROW + COLOR_TEST_PRIM);
    sprintf(
        g_PrintBuffer,
        "%3d  %3d  %3d",
        (int)f_TestColors[COLOR_TEST_ENV][0],
        (int)f_TestColors[COLOR_TEST_ENV][1],
        (int)f_TestColors[COLOR_TEST_ENV][2]
    );
    PrintDebugText(g_PrintBuffer, COLOR_TEST_TEXT_COLUMN, COLOR_TEST_TEXT_ROW + COLOR_TEST_ENV);
    PrintDebugText(
        ">",
        (f_TestChannelIndex * COLOR_TEST_COLUMN_WIDTH) + COLOR_TEST_TEXT_COLUMN - 1,
        f_TestColorIndex + COLOR_TEST_TEXT_ROW
    );
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    *displayListHead = gfx;
}

#define SHOT_PANEL_SLIDE_STEP 4
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
#define FACE_INSET_X 4
#define FACE_INSET_Y 4
#define SMALL_FACE_WIDTH 24
#define SMALL_FACE_HEIGHT 16
#define SMALL_FACE_STEP (TEXEL_STEP_ONE * 3 / 2)
#define PIN_FLAG_TIME_COUNT 16
#define PIN_FLAG_FRAME_DURATION 4
#define PIN_FLAG_IMAGES_SIDE 32
#define PIN_FLAG_FRAMES_T 16
#define PIN_FLAG_X 24
#define PIN_FLAG_Y 72
#define PIN_FLAG_WIDTH 8
#define PIN_FLAG_HEIGHT 16
#define SHOT_PANEL_LABEL_X 34
#define SHOT_PANEL_LABEL_WIDTH 32
#define SHOT_PANEL_LABEL_HEIGHT 12
#define REST_LABEL_Y 68
#define HEIGHT_LABEL_Y 80
#define SHOT_PANEL_DISTANCE_X 64
#define RING_FRAME_COUNT 3
#define RING_FRAME_DURATION 6
#define RING_IMAGE_SIZE 32
#define RING_FRAME_SIZE (RING_IMAGE_SIZE * RING_IMAGE_SIZE / 2)
#define RING_ICON_X 18
#define RING_ICON_Y 92
#define RING_COUNT_X 46
#define RING_COUNT_Y 112

#define HOLE_YARDAGE_X 60
#define HOLE_YARDAGE_Y 56
#define PAR_IMAGE_X 24
#define PAR_IMAGE_Y 56
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
#define POWER_SHOT_COUNT_X 36
#define POWER_SHOT_COUNT_Y 176
#define SHOT_LENGTH_X 132
#define SHOT_LENGTH_Y 200
#define PANEL_STOPWATCH_X 32
#define PANEL_STOPWATCH_Y 92
#define MIRROR_TEXTURE_SIZE 32
#define MIRROR_TEXTURE_SIZE_LOG2 5
#define MIRROR_TEXTURE_X 32
#define MIRROR_TEXTURE_Y 122
#define GAME_FLAG_SHOW_MIRROR_TEXTURE 0x70
#define SHADOWED_FEET_GLYPH '['

#define LoadShotPanelLabel(image) \
    { \
        gDPLoadTLUT_pal16( \
            g_DisplayListHead++, \
            0, \
            TextureAddress((u32)(image) + IMAGE_HEADER_SIZE) \
        ); \
        gDPLoadTextureBlock_4b( \
            g_DisplayListHead++, \
            TextureAddress((u32)(image) + PALETTED_TEXELS_OFFSET), \
            G_IM_FMT_CI, \
            SHOT_PANEL_LABEL_WIDTH, \
            SHOT_PANEL_LABEL_HEIGHT, \
            0, \
            G_TX_NOMIRROR | G_TX_WRAP, \
            G_TX_NOMIRROR | G_TX_WRAP, \
            G_TX_NOMASK, \
            G_TX_NOMASK, \
            G_TX_NOLOD, \
            G_TX_NOLOD \
        ); \
    }

void DrawRoundHud(CameraView *view) {
    s32 unused[2] UNUSED;
    s32 percentRange[2];
    Club *club;
    s32 scoreboardX;
    Boolean hasHandicap;
    s32 i;
    s32 ringCount;
    s32 height;
    Player *shooter;
    Club *selectedClub;
    SurfaceTraits *surfaceTraits;
    f32 shotLength;
    s32 contestDistance;
    s32 secondsTotal;

    club = GetClub(g_Players[g_CurrentShot.playerIndex].clubs[g_CurrentShot.clubSlot]);
    hasHandicap = FALSE;
    if (g_ShotPanelOffsetX > 0) {
        g_ShotPanelOffsetX -= SHOT_PANEL_SLIDE_STEP;
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

    scoreboardX =
        (f_LetterboxHeight * SCOREBOARD_LETTERBOX_SHIFT) + SCOREBOARD_X + g_ShotPanelOffsetX;
    for (i = 0; i != g_GameState.playerCount; i++) {
        if (g_Players[i].handicap > 0) {
            hasHandicap = TRUE;
            scoreboardX -= SCOREBOARD_HANDICAP_SHIFT;
            break;
        }
    }

    gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, 128);
    gDPPipeSync(g_DisplayListHead++);
    gSPTexture(
        g_DisplayListHead++,
        TEXTURE_SCALE_HALF,
        TEXTURE_SCALE_HALF,
        0,
        G_TX_RENDERTILE,
        G_ON
    );
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
    if (g_RoundHud.isProgressShown && !g_IsCupViewShown && !IsOnDrivingRange()) {
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTextureLUT(g_DisplayListHead++, G_TT_NONE);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetRenderMode(g_DisplayListHead++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTextureLUT(g_DisplayListHead++, G_TT_RGBA16);
        gDPSetCombineMode(
            g_DisplayListHead++,
            G_CC_MODULATEIDECALA_PRIM,
            G_CC_MODULATEIDECALA_PRIM
        );
        for (i = 0; i != g_GameState.playerCount; i++) {
            gDPLoadTLUT_pal256(g_DisplayListHead++, TextureAddress(g_PlayerFacePalettes[i]));
            gDPLoadTextureBlock(
                g_DisplayListHead++,
                TextureAddress(g_PlayerFaceImages[i]),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                FACE_WIDTH,
                FACE_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            if (i == g_CurrentShot.playerIndex) {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetTextureFilter(g_DisplayListHead++, G_TF_POINT);
                gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
                gDPPipeSync(g_DisplayListHead++);
                gSPScisTextureRectangle(
                    g_DisplayListHead++,
                    QuarterPixels(scoreboardX - FACE_INSET_X),
                    QuarterPixels(SCOREBOARD_Y - FACE_INSET_Y + i * SCOREBOARD_ROW_HEIGHT),
                    QuarterPixels(scoreboardX - FACE_INSET_X + FACE_WIDTH),
                    QuarterPixels(
                        SCOREBOARD_Y - FACE_INSET_Y + FACE_HEIGHT + i * SCOREBOARD_ROW_HEIGHT
                    ),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
            }
            else {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
                gDPSetPrimColor(g_DisplayListHead++, 0, 0, 127, 127, 127, 127);
                gDPPipeSync(g_DisplayListHead++);
                gSPScisTextureRectangle(
                    g_DisplayListHead++,
                    QuarterPixels(scoreboardX),
                    QuarterPixels(SCOREBOARD_Y + i * SCOREBOARD_ROW_HEIGHT),
                    QuarterPixels(scoreboardX + SMALL_FACE_WIDTH),
                    QuarterPixels(SCOREBOARD_Y + SMALL_FACE_HEIGHT + i * SCOREBOARD_ROW_HEIGHT),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    SMALL_FACE_STEP,
                    SMALL_FACE_STEP
                );
            }
        }

        gDPPipeSync(g_DisplayListHead++);
    }

    if (g_RoundHud.isHoleInfoShown && !g_IsCupViewShown) {
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
    if (g_RoundHud.isLieIconShown &&
        !g_IsCupViewShown &&
        !(club->surfaceMask & CLUB_SURFACE_GREEN)) {
        DrawShotOffsetMarker();
    }

    if (g_RoundHud.isProgressShown && !g_IsCupViewShown && !IsOnDrivingRange()) {
        f_PinFlagTime = (f_PinFlagTime + 1) & (PIN_FLAG_TIME_COUNT - 1);
        gDPPipeSync(g_DisplayListHead++);
        gDPLoadTLUT_pal16(g_DisplayListHead++, 0, TextureAddress(ImageBody(g_PinFlagImages)));
        gDPLoadTextureBlock_4b(
            g_DisplayListHead++,
            TextureAddress(ImageTexels(g_PinFlagImages)),
            G_IM_FMT_CI,
            PIN_FLAG_IMAGES_SIDE,
            PIN_FLAG_IMAGES_SIDE,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(g_DisplayListHead++);
        gSPScisTextureRectangle(
            g_DisplayListHead++,
            QuarterPixels(PIN_FLAG_X - g_ShotPanelOffsetX),
            QuarterPixels(PIN_FLAG_Y),
            QuarterPixels(PIN_FLAG_X + PIN_FLAG_WIDTH - g_ShotPanelOffsetX),
            QuarterPixels(PIN_FLAG_Y + PIN_FLAG_HEIGHT),
            G_TX_RENDERTILE,
            (f_PinFlagTime / PIN_FLAG_FRAME_DURATION) * TexelCoordinate(PIN_FLAG_WIDTH),
            TexelCoordinate(PIN_FLAG_FRAMES_T),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(g_DisplayListHead++);
        gDPLoadTLUT_pal16(g_DisplayListHead++, 0, TextureAddress(ImageBody(f_RestLabelImage)));
        gDPLoadTextureBlock_4b(
            g_DisplayListHead++,
            TextureAddress(ImageTexels(f_RestLabelImage)),
            G_IM_FMT_CI,
            SHOT_PANEL_LABEL_WIDTH,
            SHOT_PANEL_LABEL_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(g_DisplayListHead++);
        gSPScisTextureRectangle(
            g_DisplayListHead++,
            QuarterPixels(SHOT_PANEL_LABEL_X - g_ShotPanelOffsetX),
            QuarterPixels(REST_LABEL_Y),
            QuarterPixels(SHOT_PANEL_LABEL_X + SHOT_PANEL_LABEL_WIDTH - g_ShotPanelOffsetX),
            QuarterPixels(REST_LABEL_Y + SHOT_PANEL_LABEL_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(g_DisplayListHead++);

        if ((club->surfaceMask & CLUB_SURFACE_GREEN) ?
                g_RoundHud.ballHeightMetersDirection == HEIGHT_LEVEL :
                g_RoundHud.ballHeightYardsDirection == HEIGHT_LEVEL) {
            LoadShotPanelLabel(f_FlatLabelImage);
        }
        else if (g_RoundHud.ballHeightMetersDirection == HEIGHT_ABOVE) {
            LoadShotPanelLabel(f_UpLabelImage);
        }
        else {
            LoadShotPanelLabel(f_DownLabelImage);
        }

        gDPPipeSync(g_DisplayListHead++);
        gSPScisTextureRectangle(
            g_DisplayListHead++,
            QuarterPixels(SHOT_PANEL_LABEL_X - g_ShotPanelOffsetX),
            QuarterPixels(HEIGHT_LABEL_Y),
            QuarterPixels(SHOT_PANEL_LABEL_X + SHOT_PANEL_LABEL_WIDTH - g_ShotPanelOffsetX),
            QuarterPixels(HEIGHT_LABEL_Y + SHOT_PANEL_LABEL_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(g_DisplayListHead++);
        if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
            if (f_RingFrameTime++ % RING_FRAME_DURATION == 0) {
                f_RingFrame = (f_RingFrame + 1) % RING_FRAME_COUNT;
            }

            gDPPipeSync(g_DisplayListHead++);
            gDPSetRenderMode(g_DisplayListHead++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
            gDPLoadTLUT_pal16(g_DisplayListHead++, 0, TextureAddress(ImageBody(f_RingImages)));
            gDPLoadTextureBlock_4b(
                g_DisplayListHead++,
                TextureAddress(ImageTexels(f_RingImages) + f_RingFrame * RING_FRAME_SIZE),
                G_IM_FMT_CI,
                RING_IMAGE_SIZE,
                RING_IMAGE_SIZE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(g_DisplayListHead++);
            gSPScisTextureRectangle(
                g_DisplayListHead++,
                QuarterPixels(RING_ICON_X - g_ShotPanelOffsetX),
                QuarterPixels(RING_ICON_Y),
                QuarterPixels(RING_ICON_X + RING_IMAGE_SIZE - g_ShotPanelOffsetX),
                QuarterPixels(RING_ICON_Y + RING_IMAGE_SIZE),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(g_DisplayListHead++);
            gDPPipeSync(g_DisplayListHead++);
            gDPSetRenderMode(g_DisplayListHead++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        }
    }

    gDPLoadTLUT_pal16(g_DisplayListHead++, 0, TextureAddress(ImageBody(f_DistanceFont)));
    gDPLoadTextureBlock_4b(
        g_DisplayListHead++,
        TextureAddress(ImageTexels(f_DistanceFont)),
        G_IM_FMT_CI,
        DISTANCE_FONT_WIDTH,
        DISTANCE_GLYPH_HEIGHT,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    if (g_RoundHud.isHoleInfoShown &&
        !g_IsCupViewShown &&
        !IsOnDrivingRange() &&
        !g_GameState.isMiniGolf) {
        sprintf(
            g_PrintBuffer,
            "%3dy",
            (s16)g_CourseRecords[GetCourseRecordIndex()].holes[GetHoleRecordIndex()].yardages[0]
        );
        DrawDistanceText(
            &g_DisplayListHead,
            HOLE_YARDAGE_X - g_ShotPanelOffsetX,
            HOLE_YARDAGE_Y,
            g_PrintBuffer
        );
    }

    if (g_RoundHud.isProgressShown && !g_IsCupViewShown && !IsOnDrivingRange()) {
        gDPLoadTLUT_pal16(g_DisplayListHead++, 0, TextureAddress(f_WhiteDistancePalette));
        ringCount = 0;
        if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
            shooter = &g_Players[g_RoundHud.shooterIndex];
            for (i = 0; i != RING_COUNT; i++) {
                ringCount += shooter->ringStates[i] == RING_STATE_SHOWN;
            }

            sprintf(g_PrintBuffer, "%d", ringCount);
            DrawDistanceText(
                &g_DisplayListHead,
                RING_COUNT_X - g_ShotPanelOffsetX,
                RING_COUNT_Y,
                g_PrintBuffer
            );
        }

        if (g_IsDebugEnabled &&
            (g_ControllerStates[HUD_OFFSET_CONTROLLER].pressedButtons & D_CBUTTONS)) {
            shooter = &g_Players[g_RoundHud.shooterIndex];
            for (i = 0; i != RING_COUNT; i++) {
                shooter->ringStates[i] = RING_STATE_NONE;
            }
        }

        if (club->surfaceMask & CLUB_SURFACE_GREEN) {
            s32 restHundredths =
                g_RoundHud.cupDistance * (FEET_PER_YARD * HUNDREDTHS_PER_UNIT) * YARDS_PER_UNIT;

            sprintf(
                g_PrintBuffer,
                "%3d.%02df",
                restHundredths / HUNDREDTHS_PER_UNIT,
                restHundredths % HUNDREDTHS_PER_UNIT
            );
            DrawDistanceText(
                &g_DisplayListHead,
                SHOT_PANEL_DISTANCE_X - g_ShotPanelOffsetX,
                REST_LABEL_Y,
                g_PrintBuffer
            );
            if (g_RoundHud.ballHeightMetersDirection != HEIGHT_LEVEL) {
                sprintf(g_PrintBuffer, "%6.2ff", fabsf(g_RoundHud.ballHeightFeet));
                DrawDistanceText(
                    &g_DisplayListHead,
                    SHOT_PANEL_DISTANCE_X - g_ShotPanelOffsetX,
                    HEIGHT_LABEL_Y,
                    g_PrintBuffer
                );
            }
        }
        else if (SHOWS_METERS) {
            s32 restHundredths = g_RoundHud.cupDistance * HUNDREDTHS_PER_UNIT * METERS_PER_UNIT;

            sprintf(
                g_PrintBuffer,
                "%2d.%02dm",
                restHundredths / HUNDREDTHS_PER_UNIT,
                restHundredths % HUNDREDTHS_PER_UNIT
            );
            DrawDistanceText(
                &g_DisplayListHead,
                SHOT_PANEL_DISTANCE_X - g_ShotPanelOffsetX,
                REST_LABEL_Y,
                g_PrintBuffer
            );
            if (g_RoundHud.ballHeightMetersDirection != HEIGHT_LEVEL) {
                sprintf(g_PrintBuffer, "%6.2fm", fabsf(g_RoundHud.ballHeightMeters));
                DrawDistanceText(
                    &g_DisplayListHead,
                    SHOT_PANEL_DISTANCE_X - g_ShotPanelOffsetX,
                    HEIGHT_LABEL_Y,
                    g_PrintBuffer
                );
            }
        }
        else {
            sprintf(g_PrintBuffer, "%3dy", (s32)(g_RoundHud.cupDistance * YARDS_PER_UNIT));
            DrawDistanceText(
                &g_DisplayListHead,
                SHOT_PANEL_DISTANCE_X - g_ShotPanelOffsetX,
                REST_LABEL_Y,
                g_PrintBuffer
            );
            if (g_RoundHud.ballHeightYardsDirection != HEIGHT_LEVEL) {
                height = g_RoundHud.ballHeightYards;
                if (height < 0) {
                    height = -height;
                }

                sprintf(g_PrintBuffer, "%3dy", height);
                DrawDistanceText(
                    &g_DisplayListHead,
                    SHOT_PANEL_DISTANCE_X - g_ShotPanelOffsetX,
                    HEIGHT_LABEL_Y,
                    g_PrintBuffer
                );
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
        if (g_IsModuleAlive[MODULE_CLUB_PANEL]) {
            UpdateClubPanelImages();
        }

        gDPLoadTLUT_pal256(g_DisplayListHead++, TextureAddress(ImageBody(g_ClubPanelImage)));
        gDPLoadTextureBlock(
            g_DisplayListHead++,
            TextureAddress(LargeImageTexels(g_ClubPanelImage)),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            CLUB_PANEL_WIDTH,
            CLUB_PANEL_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(g_DisplayListHead++);
        gSPScisTextureRectangle(
            g_DisplayListHead++,
            QuarterPixels(CLUB_PANEL_X - g_ShotPanelOffsetX),
            QuarterPixels(f_HudOffsetY + CLUB_PANEL_Y),
            QuarterPixels(CLUB_PANEL_X + CLUB_PANEL_WIDTH - g_ShotPanelOffsetX),
            QuarterPixels(f_HudOffsetY + CLUB_PANEL_Y + CLUB_PANEL_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(g_DisplayListHead++);
        gDPLoadTLUT_pal16(
            g_DisplayListHead++,
            0,
            TextureAddress(ImageBody(f_ClubNameImages[f_ClubNameImageIndex]))
        );
        gDPLoadTextureBlock_4b(
            g_DisplayListHead++,
            TextureAddress(ImageTexels(f_ClubNameImages[f_ClubNameImageIndex])),
            G_IM_FMT_CI,
            CLUB_NAME_WIDTH,
            CLUB_NAME_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(g_DisplayListHead++);
        gSPScisTextureRectangle(
            g_DisplayListHead++,
            QuarterPixels(CLUB_NAME_X - g_ShotPanelOffsetX),
            QuarterPixels(f_HudOffsetY + CLUB_NAME_Y),
            QuarterPixels(CLUB_NAME_X + CLUB_NAME_WIDTH - g_ShotPanelOffsetX),
            QuarterPixels(f_HudOffsetY + CLUB_NAME_Y + CLUB_NAME_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(g_DisplayListHead++);
    }

    if (g_RoundHud.isHoleInfoShown && !g_IsCupViewShown && !IsOnDrivingRange()) {
        gDPLoadTLUT_pal16(g_DisplayListHead++, 0, TextureAddress(ImageBody(f_ParImage)));
        gDPLoadTextureBlock_4b(
            g_DisplayListHead++,
            TextureAddress(ImageTexels(f_ParImage)),
            G_IM_FMT_CI,
            SHOT_PANEL_LABEL_WIDTH,
            SHOT_PANEL_LABEL_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(g_DisplayListHead++);
        gSPScisTextureRectangle(
            g_DisplayListHead++,
            QuarterPixels(PAR_IMAGE_X - g_ShotPanelOffsetX),
            QuarterPixels(PAR_IMAGE_Y),
            QuarterPixels(PAR_IMAGE_X + SHOT_PANEL_LABEL_WIDTH - g_ShotPanelOffsetX),
            QuarterPixels(PAR_IMAGE_Y + SHOT_PANEL_LABEL_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(g_DisplayListHead++);
    }

    if (IsLetterboxState(g_LetterboxState) && g_RoundHud.isLetterboxShown) {
        DrawAnimatedIcon(&g_DisplayListHead, f_LetterboxHeight);
    }

    SetUpShadowedTextDrawing(&g_DisplayListHead);
    SetTextColor(&g_DisplayListHead, 255, 255, 255);
    SetTextColor(&g_DisplayListHead, 255, 255, 255);
    if (g_RoundHud.isLieIconShown && !g_IsCupViewShown) {
        surfaceTraits = GetSurfaceTraits(g_CurrentShot.surface);
        GetSurfacePercentRange(surfaceTraits, GetSelectedClub(), percentRange);
        sprintf(g_PrintBuffer, "%3d-%3d", percentRange[0], percentRange[1]);
        DrawShadowedText(
            &g_DisplayListHead,
            g_ShotPanelOffsetX + LIE_PERCENT_X,
            f_HudOffsetY + SHOT_PANEL_TEXT_Y,
            g_PrintBuffer
        );
    }

    if (g_RoundHud.isClubHeadIconShown && !g_IsCupViewShown) {
        Player *player;

        selectedClub = GetSelectedClub();
        player = &g_Players[g_RoundHud.shooterIndex];
        if (selectedClub->surfaceMask & CLUB_SURFACE_GREEN) {
            sprintf(g_PrintBuffer, "%3d%c", GetSelectedShotDistance(), SHADOWED_FEET_GLYPH);
        }
        else if (SHOWS_METERS) {
            sprintf(
                g_PrintBuffer,
                "%3dm",
                (s32)(GetSelectedShotDistance() * (METERS_PER_UNIT / YARDS_PER_UNIT))
            );
        }
        else {
            sprintf(g_PrintBuffer, "%3dy", GetSelectedShotDistance());
        }

        DrawShadowedText(
            &g_DisplayListHead,
            CLUB_DISTANCE_X - g_ShotPanelOffsetX,
            f_HudOffsetY + SHOT_PANEL_TEXT_Y,
            g_PrintBuffer
        );
        if (g_CurrentShot.shotMode == SHOT_MODE_POWER &&
            !(selectedClub->surfaceMask & CLUB_SURFACE_GREEN)) {
            if (g_IsModuleAlive[MODULE_CLUB_PANEL] &&
                IsShotModeChangeDone() &&
                player->powerShotCount != POWER_SHOTS_UNLIMITED) {
                sprintf(g_PrintBuffer, "%d", player->powerShotCount);
                DrawShadowedText(
                    &g_DisplayListHead,
                    POWER_SHOT_COUNT_X - g_ShotPanelOffsetX,
                    f_HudOffsetY + POWER_SHOT_COUNT_Y,
                    g_PrintBuffer
                );
            }
        }
    }

    if (g_RoundHud.isShotLengthShown && !g_IsCupViewShown) {
        Player *player;

        player = &g_Players[g_RoundHud.shooterIndex];
        shotLength = IntLength2D(
            g_CurrentShot.ballPosition[AXIS_X] - g_TurnBallSimulation.position[AXIS_X],
            g_CurrentShot.ballPosition[AXIS_Z] - g_TurnBallSimulation.position[AXIS_Z]
        );
        SetTextColor(&g_DisplayListHead, 255, 255, 63);
        if (GetClub(player->clubs[g_CurrentShot.clubSlot])->surfaceMask & CLUB_SURFACE_GREEN) {
            sprintf(
                g_PrintBuffer,
                "%5.2f%c",
                shotLength * (FEET_PER_YARD * YARDS_PER_UNIT),
                SHADOWED_FEET_GLYPH
            );
        }
        else if (SHOWS_METERS) {
            sprintf(g_PrintBuffer, "%5.2fm", shotLength * METERS_PER_UNIT);
        }
        else {
            sprintf(g_PrintBuffer, "%5.2fy", shotLength * YARDS_PER_UNIT);
        }

        DrawShadowedText(&g_DisplayListHead, SHOT_LENGTH_X, SHOT_LENGTH_Y, g_PrintBuffer);
    }

    if (g_RoundHud.isProgressShown && !g_IsCupViewShown && !IsOnDrivingRange()) {
        for (i = 0; i != g_GameState.playerCount; i++) {
            SetTextColor(&g_DisplayListHead, 255, 255, 255);
            sprintf(g_PrintBuffer, "%d", g_Players[i].strokeCount);
            DrawShadowedText(
                &g_DisplayListHead,
                scoreboardX + SCOREBOARD_TEXT_X,
                SCOREBOARD_Y + (i * SCOREBOARD_ROW_HEIGHT),
                g_PrintBuffer
            );
            if (hasHandicap) {
                DrawShadowedText(
                    &g_DisplayListHead,
                    scoreboardX + HANDICAP_COLON_X,
                    SCOREBOARD_Y + (i * SCOREBOARD_ROW_HEIGHT),
                    ":"
                );
                sprintf(
                    g_PrintBuffer,
                    "%2d",
                    GetHandicapOffset(
                        GetCourseRecordIndex(),
                        g_GameState.holeIndex,
                        g_Players[i].handicap
                    )
                );
                DrawShadowedText(
                    &g_DisplayListHead,
                    scoreboardX + HANDICAP_X,
                    SCOREBOARD_Y + (i * SCOREBOARD_ROW_HEIGHT),
                    g_PrintBuffer
                );
            }

            SetTextColor(&g_DisplayListHead, 255, 230, 0);
            if (g_GameState.replayKind != REPLAY_KIND_NONE) {
                sprintf(g_PrintBuffer, f_BlankText);
            }
            else {
                switch (g_GameState.gameMode) {
                    case GAME_MODE_STROKE:
                    case GAME_MODE_UNLISTED_STROKE:
                    case GAME_MODE_TOURNAMENT:
                    case GAME_MODE_SPEED_GOLF:
                        if (g_RoundHud.skinsTotals[i] == 0) {
                            sprintf(g_PrintBuffer, "#0");
                        }
                        else {
                            sprintf(g_PrintBuffer, "%+d", g_RoundHud.skinsTotals[i]);
                        }

                        break;
                    case GAME_MODE_MATCH_GAME:
                    case GAME_MODE_GET_CHARACTER:
                        if (g_RoundHud.skinsTotals[i] == 0) {
                            sprintf(g_PrintBuffer, "EVEN");
                        }
                        else if (g_RoundHud.skinsTotals[i] < 0) {
                            sprintf(g_PrintBuffer, "%dUP", -g_RoundHud.skinsTotals[i]);
                        }
                        else {
                            sprintf(g_PrintBuffer, "%dDW", g_RoundHud.skinsTotals[i]);
                        }

                        break;
                    case GAME_MODE_SKINS_MATCH:
                    case GAME_MODE_CLUB_SLOTS:
                        sprintf(g_PrintBuffer, "%dP", g_RoundHud.skinsTotals[i]);
                        break;
                    case GAME_MODE_TRAINING:
                    case GAME_MODE_RECORD_REPLAY:
                    case GAME_MODE_RING_SHOT:
                        sprintf(g_PrintBuffer, f_BlankText);
                        break;
                }
            }

            DrawShadowedText(
                &g_DisplayListHead,
                scoreboardX + SCOREBOARD_TEXT_X,
                SCOREBOARD_SCORE_Y + (i * SCOREBOARD_ROW_HEIGHT),
                g_PrintBuffer
            );
            SetTextColor(&g_DisplayListHead, 255, 255, 255);
            if (g_GameState.betRates[BET_LONGEST_DRIVE] != UNSET_BET_RATE &&
                IsMarkedParFive(GetCourseRecordIndex(), g_GameState.holeIndex)) {
                contestDistance = g_Players[i].driveDistance;
                if (contestDistance != UNMEASURED_DISTANCE) {
                    if (contestDistance == UNCOUNTED_DISTANCE) {
                        sprintf(g_PrintBuffer, "miss");
                    }
                    else {
                        sprintf(g_PrintBuffer, "%3dy", (s32)(contestDistance * YARDS_PER_UNIT));
                    }

                    DrawShadowedText(
                        &g_DisplayListHead,
                        scoreboardX + DRIVE_DISTANCE_X,
                        SCOREBOARD_CONTEST_Y + (i * SCOREBOARD_ROW_HEIGHT),
                        g_PrintBuffer
                    );
                }
            }

            if (g_GameState.betRates[BET_NEAREST_PIN] != UNSET_BET_RATE &&
                IsParThree(GetCourseRecordIndex(), g_GameState.holeIndex)) {
                contestDistance = g_Players[i].pinDistance;
                if (contestDistance != UNMEASURED_DISTANCE) {
                    if (contestDistance == UNCOUNTED_DISTANCE) {
                        sprintf(g_PrintBuffer, "miss");
                    }
                    else {
                        s32 pinHundredths = contestDistance *
                            (FEET_PER_YARD * HUNDREDTHS_PER_UNIT) * YARDS_PER_UNIT;

                        sprintf(
                            g_PrintBuffer,
                            "%2d.%02d%c",
                            pinHundredths / HUNDREDTHS_PER_UNIT,
                            pinHundredths % HUNDREDTHS_PER_UNIT,
                            SHADOWED_FEET_GLYPH
                        );
                    }

                    DrawShadowedText(
                        &g_DisplayListHead,
                        scoreboardX + PIN_DISTANCE_X,
                        SCOREBOARD_CONTEST_Y + (i * SCOREBOARD_ROW_HEIGHT),
                        g_PrintBuffer
                    );
                }
            }
        }
    }

    if (g_RoundHud.isProgressShown &&
        !IsOnDrivingRange() &&
        g_GameState.gameMode == GAME_MODE_SPEED_GOLF &&
        g_GameState.replayKind == REPLAY_KIND_NONE) {
        s32 hundredths = (s32)(g_GameState.stopwatchFrameCount *
                             ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
            HUNDREDTHS_PER_SECOND;

        secondsTotal = g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND;
        sprintf(
            g_PrintBuffer,
            "%02d:%02d:%02d",
            secondsTotal / SECONDS_PER_MINUTE,
            secondsTotal % SECONDS_PER_MINUTE,
            hundredths
        );
        DrawShadowedText(
            &g_DisplayListHead,
            PANEL_STOPWATCH_X - g_ShotPanelOffsetX,
            PANEL_STOPWATCH_Y,
            g_PrintBuffer
        );
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
        gDPLoadTextureBlock(
            g_DisplayListHead++,
            TextureAddress(g_MirrorTexture),
            G_IM_FMT_RGBA,
            G_IM_SIZ_16b,
            MIRROR_TEXTURE_SIZE,
            MIRROR_TEXTURE_SIZE,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_MIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            MIRROR_TEXTURE_SIZE_LOG2,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gSPScisTextureRectangle(
            g_DisplayListHead++,
            QuarterPixels(MIRROR_TEXTURE_X),
            QuarterPixels(MIRROR_TEXTURE_Y),
            QuarterPixels(MIRROR_TEXTURE_X + MIRROR_TEXTURE_SIZE),
            QuarterPixels(MIRROR_TEXTURE_Y + MIRROR_TEXTURE_SIZE),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(g_DisplayListHead++);
    }

    if (g_IsModuleAlive[MODULE_SHOT_EFFECTS]) {
        DrawShotEffectsHud(&g_DisplayListHead);
    }

    if (IsGameFlagSet(GAME_FLAG_ADJUST_HUD_OFFSET)) {
        if (g_ControllerStates[HUD_OFFSET_CONTROLLER].pressedButtons & B_BUTTON) {
            g_Players[0].characterIndex = (g_Players[0].characterIndex + 1) % CHARACTER_COUNT;
        }

        if (g_ControllerStates[HUD_OFFSET_CONTROLLER].pressedButtons & A_BUTTON) {
            SetContestWinner(0);
            StartMovie(MOVIE_LONGEST_DRIVE);
        }
    }

    AdjustHudOffset();
}

void LoadHoleIntroImages(void) {
    EncodedAsset asset;
    MatchStanding standing;

    f_LoadedHoleIntroImages = 0;
    if (g_GameState.betRates[BET_LONGEST_DRIVE] != UNSET_BET_RATE &&
        IsMarkedParFive(g_GameState.courseIndex, g_GameState.holeIndex)) {
        f_HoleContestImage = AllocMemory(OpenEncodedAsset(ASSET_LONGEST_DRIVE_IMAGE, &asset));
        ReadEncodedAsset(f_HoleContestImage, &asset);
        f_LoadedHoleIntroImages |= HOLE_INTRO_CONTEST_IMAGE_LOADED;
    }
    else if (g_GameState.betRates[BET_NEAREST_PIN] != UNSET_BET_RATE &&
        IsParThree(g_GameState.courseIndex, g_GameState.holeIndex)) {
        f_HoleContestImage = AllocMemory(OpenEncodedAsset(ASSET_NEAREST_PIN_IMAGE, &asset));
        ReadEncodedAsset(f_HoleContestImage, &asset);
        f_LoadedHoleIntroImages |= HOLE_INTRO_CONTEST_IMAGE_LOADED;
    }

    if (IsOneOnOneMatchMode(g_GameState.gameMode)) {
        if (g_GameState.isSuddenDeath) {
            f_MatchStandingImage = AllocMemory(OpenEncodedAsset(ASSET_SUDDEN_DEATH_IMAGE, &asset));
            ReadEncodedAsset(f_MatchStandingImage, &asset);
            f_LoadedHoleIntroImages |= HOLE_INTRO_MATCH_IMAGE_LOADED;
        }
        else {
            standing = GetMatchStanding(0, 1);
            if (standing == MATCH_STANDING_PLAYER_DORMIE ||
                standing == MATCH_STANDING_OPPONENT_DORMIE) {
                f_MatchStandingImage = AllocMemory(OpenEncodedAsset(ASSET_DORMIE_IMAGE, &asset));
                ReadEncodedAsset(f_MatchStandingImage, &asset);
                f_LoadedHoleIntroImages |= HOLE_INTRO_MATCH_IMAGE_LOADED;
            }
        }
    }
}

void FreeHoleIntroImages(void) {
    if (f_LoadedHoleIntroImages & HOLE_INTRO_CONTEST_IMAGE_LOADED) {
        FreeMemory(&f_HoleContestImage);
    }

    if (f_LoadedHoleIntroImages & HOLE_INTRO_MATCH_IMAGE_LOADED) {
        FreeMemory(&f_MatchStandingImage);
    }

    f_LoadedHoleIntroImages = 0;
}

void DrawHoleIntroImages(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 strip;

    if (g_RoundHud.isHoleIntroTextShown == TRUE) {
        if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
            if (g_IsModuleAlive[MODULE_COURSE_SELECT]) {
                DrawRingShotSelect(&gfx);
            }
        }
        else {
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_RGBA16);
            gDPPipeSync(gfx++);
            gDPSetTextureFilter(gfx++, G_TF_BILERP);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(
                gfx++,
                OPAQUE_ALPHA_COVERAGE_RENDER_MODE,
                OPAQUE_ALPHA_COVERAGE_RENDER_MODE2
            );
            gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            if ((g_GameState.betRates[BET_LONGEST_DRIVE] > 0 &&
                    IsMarkedParFive(GetCourseRecordIndex(), g_GameState.holeIndex)) ||
                (g_GameState.betRates[BET_NEAREST_PIN] > 0 &&
                    IsParThree(GetCourseRecordIndex(), g_GameState.holeIndex))) {
                gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(f_HoleContestImage)));
                for (strip = 0; strip != CONTEST_IMAGE_STRIP_COUNT; strip++) {
                    gDPLoadTextureBlock(
                        gfx++,
                        TextureAddress(
                            LargeImageTexels(f_HoleContestImage) +
                            (strip * (CONTEST_IMAGE_WIDTH * CONTEST_IMAGE_STRIP_HEIGHT))
                        ),
                        G_IM_FMT_CI,
                        G_IM_SIZ_8b,
                        CONTEST_IMAGE_WIDTH,
                        CONTEST_IMAGE_STRIP_HEIGHT,
                        0,
                        G_TX_MIRROR,
                        G_TX_MIRROR,
                        G_TX_NOMASK,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOLOD
                    );
                    gDPPipeSync(gfx++);
                    gSPScisTextureRectangle(
                        gfx++,
                        QuarterPixels(CONTEST_IMAGE_X),
                        QuarterPixels(CONTEST_IMAGE_Y + strip * CONTEST_IMAGE_STRIP_HEIGHT),
                        QuarterPixels(CONTEST_IMAGE_X + CONTEST_IMAGE_WIDTH),
                        QuarterPixels(
                            CONTEST_IMAGE_Y + CONTEST_IMAGE_STRIP_HEIGHT +
                            strip * CONTEST_IMAGE_STRIP_HEIGHT
                        ),
                        G_TX_RENDERTILE,
                        0,
                        0,
                        TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE
                    );
                    gDPPipeSync(gfx++);
                }
            }

            if (IsOneOnOneMatchMode(g_GameState.gameMode) &&
                (f_LoadedHoleIntroImages & HOLE_INTRO_MATCH_IMAGE_LOADED)) {
                gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(f_MatchStandingImage)));
                for (strip = 0; strip != MATCH_IMAGE_STRIP_COUNT; strip++) {
                    gDPLoadTextureTile(
                        gfx++,
                        TextureAddress(
                            LargeImageTexels(f_MatchStandingImage) +
                            (strip * (MATCH_IMAGE_WIDTH * MATCH_IMAGE_STRIP_HEIGHT))
                        ),
                        G_IM_FMT_CI,
                        G_IM_SIZ_8b,
                        MATCH_IMAGE_WIDTH,
                        MATCH_IMAGE_STRIP_HEIGHT,
                        0,
                        0,
                        MATCH_IMAGE_WIDTH - 1,
                        MATCH_IMAGE_STRIP_HEIGHT - 1,
                        0,
                        G_TX_MIRROR,
                        G_TX_MIRROR,
                        G_TX_NOMASK,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOLOD
                    );
                    gDPPipeSync(gfx++);
                    gSPScisTextureRectangle(
                        gfx++,
                        QuarterPixels(MATCH_IMAGE_X),
                        QuarterPixels(MATCH_IMAGE_Y + strip * MATCH_IMAGE_STRIP_HEIGHT),
                        QuarterPixels(MATCH_IMAGE_X + MATCH_IMAGE_WIDTH),
                        QuarterPixels(
                            MATCH_IMAGE_Y + MATCH_IMAGE_STRIP_HEIGHT +
                            strip * MATCH_IMAGE_STRIP_HEIGHT
                        ),
                        G_TX_RENDERTILE,
                        0,
                        0,
                        TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE
                    );
                    gDPPipeSync(gfx++);
                }
            }
        }
    }

    *displayListHead = gfx;
}

void DrawHoleIntroText(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 hundredths;
    s32 minutes;
    s32 seconds;
    s32 secondsTotal;

    if (g_RoundHud.isHoleIntroTextShown == TRUE) {
        if (g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
            if (g_IsModuleAlive[MODULE_CLUB_SLOTS] && GetClubSlotsState()) {
                DrawClubSlots(&gfx);
            }
        }
        else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
        }
        else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
            if (g_IsModuleAlive[MODULE_COURSE_SELECT]) {
                DrawHoleSelect(&gfx);
            }
        }
        else if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF &&
            g_GameState.replayKind == REPLAY_KIND_NONE) {
            hundredths = (s32)(g_GameState.stopwatchFrameCount *
                             ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
                HUNDREDTHS_PER_SECOND;
            secondsTotal = g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND;
            seconds = secondsTotal % SECONDS_PER_MINUTE;
            minutes = secondsTotal / SECONDS_PER_MINUTE;
            SetUpGradientTextDrawing(&gfx, 0);
            sprintf(g_PrintBuffer, "%02d:%02d:%02d", minutes, seconds, hundredths);
            DrawLargeTextWithShadow(
                &gfx,
                STOPWATCH_X,
                STOPWATCH_Y,
                g_PrintBuffer,
                STOPWATCH_ENV_COLOR,
                STOPWATCH_PRIM_COLOR,
                TEXT_FLAG_FIXED_WIDTH
            );
        }
        else {
            SetUpGradientTextDrawing(&gfx, 0);
            gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x00, 0x00, OPAQUE_ALPHA);
            gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0x00, OPAQUE_ALPHA);
            sprintf(g_PrintBuffer, "%s", g_CourseRecords[GetCourseRecordIndex()].name);
            sprintf(g_PrintBuffer, "%s", g_CourseRecords[GetCourseRecordIndex()].name);
            DrawLargeTextWithShadow(
                &gfx,
                HOLE_TITLE_X,
                COURSE_NAME_Y,
                g_PrintBuffer,
                g_CourseNameEnvColors[g_GameState.courseIndex],
                g_CourseNamePrimColors[g_GameState.courseIndex],
                TEXT_FLAG_CENTERED
            );
            sprintf(g_PrintBuffer, "Hole %d", (int)(g_GameState.holeIndex + 1));
            DrawLargeTextWithShadow(
                &gfx,
                HOLE_TITLE_X,
                HOLE_NUMBER_Y,
                g_PrintBuffer,
                g_CourseNameEnvColors[g_GameState.courseIndex],
                g_CourseNamePrimColors[g_GameState.courseIndex],
                TEXT_FLAG_CENTERED
            );
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        }
    }

    *displayListHead = gfx;
}

void SwapMatrixBuffers(void) {
    g_MatrixBufferIndex ^= 1;
}
