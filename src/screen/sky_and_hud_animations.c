/*
 * Draws what surrounds the course: the sky, the sun's glare and lens flare, the
 * course's backdrop and the menus' backdrops. It also draws the HUD animations
 * that fade, spiral, tile, turn away or slice the whole frame, and sets up the
 * hole behind the title screen.
 */

#include "screen/sky_and_hud_animations.h"

#include <framebuffer_count.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>

#include "audio/sound.h"
#include "camera/camera.h"
#include "camera/shot_camera.h"
#include "camera/view_projection.h"
#include "controllers/controller.h"
#include "course/course.h"
#include "course/hole_conditions.h"
#include "course/hole_loader.h"
#include "course/hole_setup.h"
#include "debug/debug_text.h"
#include "debug/game_flags.h"
#include "game/game.h"
#include "game/scene.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "round/fireworks.h"
#include "round/game_state.h"
#include "round/round_hud.h"
#include "screen/display_list.h"
#include "screen/scene_colors.h"
#include "screen/screen.h"
#include "system/asset.h"
#include "system/big_endian.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/heap.h"
#include "system/memory_address.h"
#include "text/text.h"
#ifdef AVOID_UB
#    include "screen/sky_colors_overrun.h"
#endif

// The hole that plays behind the title screen: the last hole of the first
// course, calm and lit from a fixed direction.
#define TITLE_HOLE_INDEX 9
#define TITLE_COURSE_INDEX 0
#define TITLE_WIND_SPEED_STEP 6000
#define TITLE_LIGHT_PITCH 0x1000
#define TITLE_LIGHT_YAW 0x6000

// The sun's glare grows while the sun is on the screen with nothing in front
// of it, flaring past SUN_GLARE_FLARE_STRENGTH before settling at full
// strength, and fades when something covers it. Only the z-buffer within
// SUN_DEPTH_MARGIN of the screen's edges is read.
#define SUN_DEPTH_MARGIN 16.0f // ASSET_SKY_IMAGE_0; ASSET_SKY_IMAGE_1
#define SUN_GLARE_MARGIN 120.0f
// Whether a point lies at least inset within each of the screen's edges
#define IsInsetOnScreen(x, y, inset) \
    ((x) >= (inset) && \
        (x) <= SCREEN_WIDTH - (inset) && \
        (y) >= (inset) && \
        (y) <= SCREEN_HEIGHT - (inset))
// Whether a point lies less than margin beyond each of the screen's edges
#define IsNearScreen(x, y, margin) \
    ((x) > -(margin) && \
        (x) < SCREEN_WIDTH + (margin) && \
        (y) > -(margin) && \
        (y) < SCREEN_HEIGHT + (margin))
// Whether a point lies on or beyond the line inset within any of the screen's
// edges
#define IsOnOrPastInset(x, y, inset) \
    ((x) <= (inset) || \
        (x) >= SCREEN_WIDTH - (inset) || \
        (y) <= (inset) || \
        (y) >= SCREEN_HEIGHT - (inset))
// The z-buffer value of the sky
#define SUN_SKY_DEPTH 0xFFFC
#define SUN_GLARE_FADE_STEP 0.1f
#define SUN_GLARE_GROWTH_STEP 0.3f
#define SUN_GLARE_FLARE_STRENGTH 1.5f
#define SUN_GLARE_FULL_STRENGTH 1.0f

// The corners of the sky's fan, then its mirror's
#define SKY_TOP_LEFT 0
#define SKY_HORIZON_LEFT 1
#define SKY_HORIZON_RIGHT 2
#define SKY_TOP_RIGHT 3
#define SKY_TOP_CENTER 4
#define SKY_FAN_VERTEX_COUNT 5
#define MIRRORED_SKY_FAN SKY_FAN_VERTEX_COUNT
// The sky's vertices, for each view and each matrix buffer
#define SKY_VERTEX_COUNT (SKY_FAN_VERTEX_COUNT * 2)
// f_SkyBlendStep counts to this at most
#define SKY_BLEND_STEP_COUNT 1000

// Each scene color index has a time of day for the sky's colors, and a time
// and a percentage to blend from towards the next time. The tables are read
// one entry past the scene color index too, for the next index's values.
#define SKY_PERCENT_TO_STEPS (SKY_BLEND_STEP_COUNT / PERCENT_PER_WHOLE)
// The course sky image is paletted, and InitSkyColors recolors its palette
#define PALETTE_START (IMAGE_HEADER_SIZE / PALETTE_COLOR_SIZE)
// A color of the course sky image's palette, which a host keeps in the
// cartridge's order for the RDP
#ifdef TARGET_PC
#    define CourseSkyColor(index) \
        FromBigEndian16(((u16 *)g_CourseSkyImage)[PALETTE_START + (index)])
#    define SetCourseSkyColor(index, color) \
        StoreBigEndian16(&((u16 *)g_CourseSkyImage)[PALETTE_START + (index)], (color))
#else
#    define CourseSkyColor(index) ((u16 *)g_CourseSkyImage)[PALETTE_START + (index)]
#    define SetCourseSkyColor(index, color) \
        (((u16 *)g_CourseSkyImage)[PALETTE_START + (index)] = (color))
#endif
#define SKY_IMAGE_CHANNEL_MAX 31
// The palette colors mix five parts of the horizon color with one of the top
// color, shifted down to five bits
#define SKY_IMAGE_HORIZON_PARTS 5
#define SKY_IMAGE_DIVISOR (6 * 8)
// The frame that the HUD animations break apart, copied two pixels to a word
// from inside the HUD's margins. The tile animations copy it as a grid of
// tiles, and HUD_ANIMATION_SLICE_CAPTURE copies a slice of the middle of the
// frame each time it starts, for HUD_ANIMATION_SLICES to show.
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
    ((FRAME_SLICE_BOTTOM - FRAME_SLICE_TOP) * (FRAME_SLICE_RIGHT_WORD - FRAME_SLICE_LEFT_WORD))

// The slices that the slice animation copied fill the screen, a column of
// them at a time, each drawn in strips that fit in texture memory. A new
// slice appears every SLICE_FRAME_COUNT frames, with a sound.
#define SLICE_ANIMATION_FRAME_COUNT 64
#define FRAMES_PER_SLICE 8
#define SLICE_WIDTH ((FRAME_SLICE_RIGHT_WORD - FRAME_SLICE_LEFT_WORD) * 2)
#define SLICE_HEIGHT (FRAME_SLICE_BOTTOM - FRAME_SLICE_TOP)
#define SLICE_COLUMN_COUNT (SCREEN_WIDTH / SLICE_WIDTH)
#define SLICE_STRIP_COUNT 8
#define SLICE_STRIP_HEIGHT (SLICE_HEIGHT / SLICE_STRIP_COUNT)
#define SLICE_STRIP_WORD_COUNT (SLICE_STRIP_HEIGHT * SLICE_WIDTH / 2)
#define SLICE_SOUND_CHANNEL 13

// The corners of the tiles, for each of two frames
#define TILE_GRID_ROW_COUNT 8
#define TILE_GRID_COLUMN_COUNT 6
#define TILE_GRID_LEFT (-152)
#define TILE_GRID_RIGHT 152
#define TILE_GRID_TOP 112
#define TILE_GRID_SPACING 64
#define TILE_GRID_ROW_SPACING 32
// The height of a row of the tile grid's vertices
#define TileGridRowY(row) (TILE_GRID_TOP - ((row) * TILE_GRID_ROW_SPACING))
// The x of a column of the tile grid's vertices, other than the last
#define TileGridColumnX(column) (TILE_GRID_LEFT + ((column) * TILE_GRID_SPACING))
// The x of the last column, which is narrower; columns count on from it by the spacing
#define TileGridLastColumnX(column) \
    (TILE_GRID_RIGHT + (((column) - (TILE_GRID_COLUMN_COUNT - 1)) * TILE_GRID_SPACING))

// The plain backdrop copies the last frame, inside a border, into
// f_CapturedFrame in strips. Each strip loads two rows more than it steps,
// and the last loads only the rows that are left.
#define BACKDROP_BORDER 8
#define BACKDROP_RIGHT (SCREEN_WIDTH - BACKDROP_BORDER - 1)
#define BACKDROP_STRIP_COUNT 38
#define BACKDROP_STRIP_STEP 6
#define BACKDROP_STRIP_HEIGHT 8
#define BACKDROP_LAST_STRIP_HEIGHT 2
// A backdrop strip's height: BACKDROP_STRIP_HEIGHT, or
// BACKDROP_LAST_STRIP_HEIGHT for the last strip
#define StripHeight(strip) \
    (BACKDROP_LAST_STRIP_HEIGHT | \
        ((strip) != BACKDROP_STRIP_COUNT - 1 ? \
                BACKDROP_STRIP_HEIGHT - BACKDROP_LAST_STRIP_HEIGHT : \
                0))
#define BACKDROP_START_FADE_LEVEL 255
// Each frame the fade level steps down until it is at most
// BACKDROP_LAST_FADE_LEVEL
#define BACKDROP_FADE_STEP 32
#define BACKDROP_LAST_FADE_LEVEL 128

// HUD_ANIMATION_SPIRAL cuts the frame inside the margins into a square grid.
// Each frame, it draws as many fewer tiles as it fades.
#define SPIRAL_GRID_SIZE 8
#define SPIRAL_TILE_COUNT (SPIRAL_GRID_SIZE * SPIRAL_GRID_SIZE)
#define SPIRAL_FRAME_WIDTH (SCREEN_WIDTH - (2 * FRAME_MARGIN_ROWS))
#define SPIRAL_TILE_WIDTH (SPIRAL_FRAME_WIDTH / SPIRAL_GRID_SIZE)
#define SPIRAL_TILE_HEIGHT ((SCREEN_HEIGHT - (2 * FRAME_MARGIN_ROWS)) / SPIRAL_GRID_SIZE)
#define SPIRAL_FADING_TILE_COUNT 4
// The tiles are drawn white, fading by SPIRAL_ALPHA_STEP each
#define SPIRAL_SHADE 255
#define SPIRAL_ALPHA_STEP (OPAQUE_ALPHA / (SPIRAL_FADING_TILE_COUNT + 1))
// The count of tiles left drops by SPIRAL_FADING_TILE_COUNT each frame, from
// this start that keeps the whole grid opaque for two frames
#define SPIRAL_START_TILE_COUNT (SPIRAL_TILE_COUNT + (2 * SPIRAL_FADING_TILE_COUNT))

enum SpiralDirection {
    SPIRAL_RIGHT,
    SPIRAL_DOWN,
    SPIRAL_LEFT,
    SPIRAL_UP
};

typedef s32 SpiralDirection;

// The sky editor table's columns: the top, horizon and noise colors, the
// background image's color, and fog
enum SkyEditorColumn {
    SKY_EDITOR_TOP,
    SKY_EDITOR_HORIZON,
    SKY_EDITOR_NOISE,
    SKY_EDITOR_BACKGROUND,
    SKY_EDITOR_FOG
};

typedef s32 SkyEditorColumn;

// The frame's tiles turn over and fly apart towards the camera as the
// animation goes on. The last column is narrower than the others.
#define FRAME_TILE_COUNT (FRAME_TILE_ROW_COUNT * FRAME_TILE_COLUMN_COUNT)
#define FRAME_TILE_WIDTH (FRAME_TILE_WORDS * 2)
#define FRAME_TILE_WORD_COUNT (FRAME_TILE_ROWS * FRAME_TILE_WORDS)
#define TILE_ANIMATION_FRAME_COUNT 255
#define TILE_ANIMATION_STEP 10
#define TILE_ANIMATION_GAP_X 64
#define TILE_ANIMATION_GAP_Y 32
#define TILE_ANIMATION_OFFSET_X 8
#define TILE_ANIMATION_DEPTH (-290.0f)
#define TILE_ANIMATION_SPEED_Z 2
#define TILE_ANIMATION_UNUSED_ANGLE_OFFSET 0.2f
#define TILE_VERTEX_COUNT 4

void CaptureHudAnimationFrame(void);
void DrawSliceAnimation(void);

// f_SkyHorizonColor's starting value in each channel
#define SKY_HORIZON_LEVEL 63
// The image's width is 1 << this
#define BACKDROP_IMAGE_WIDTH_MASK 9
// The backdrop's strips before DrawBackdrop sets them for the course, which it
// does before reading them. The width mask is the course's, for an image twice
// as wide.
#define INITIAL_BACKDROP_STRIP_COUNT 4
#define INITIAL_BACKDROP_STRIP_HEIGHT 8
#define INITIAL_BACKDROP_IMAGE_WIDTH 256
#define LENS_FLARE_COUNT 8

typedef struct LensFlare {
    u8 color[RGB_CHANNEL_COUNT]; // Where the ring texture is brightest
    u8 envColor[RGB_CHANNEL_COUNT]; // Where it is darkest, and the texels blend between them
    s32 size;
    // Nothing reads it. Each is about 0x10000 over a size from 8 to 64, though
    // not always this flare's size.
    s32 ignoredScaleValue;
    f32 position; // Along the line, 0 at the sun and 1 at its axis point
} LensFlare;

static u32 *f_CapturedFrame = (u32 *)FixedAddress(MAIN_REGION_END);
static s32 f_HudAnimationFramesLeft = 0;
static HudAnimation f_HudAnimation = HUD_ANIMATION_UNUSED;
static s32 f_SkyTimeIndex = 0;
static s32 f_SkyColorTimeIndex = 0;
static s32 f_SkyBlendStep = 0;
static s32 f_SkyBlendStepCount = 0;
static Boolean f_IsPrimitiveDepthSet = FALSE;
static s8 f_SceneSkyBlendTimes[] = {0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2};
static s8 f_SceneSkyColorTimes[] = {0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3};
static s8 f_SceneSkyBlendPercents[] = {0, 80, 0, 0, 0, 0, 0, 20, 50, 75, 100, 100};
static f32 f_SunScreenX[SKY_VIEW_COUNT] = {0.0f, 0.0f}; // For each view
static f32 f_SunScreenY[SKY_VIEW_COUNT] = {0.0f, 0.0f};
#define MORNING_SUN 0
#define MIDDAY_SUN 1
#define RAINY_SUNSET_SUN 2
#define SUNSET_SUN 4
#define MOON 6
#define SUN_COUNT 7
// The sky's suns, which g_SkyColors points to; none points to the fourth or the sixth
static Sun f_Suns[SUN_COUNT] = {
    {
        {0, 0, 0, 0},
        24,
        {0xFF, 0xEF, 0xBF},
        {0xFF, 0xFF, 0x7F},
        64,
        0,
        0x400,
        64.0f,
        SKY_IMAGE_KIND_1,
    },
    {
        {0, 0, 0, 0},
        192,
        {0xFF, 0xEF, 0xE0},
        {0xFF, 0xEF, 0xE0},
        32,
        0,
        0x400,
        96.0f,
        SKY_IMAGE_KIND_0,
    },
    {
        {0, 0, 0, 0},
        128,
        {0xFF, 0xEF, 0xE0},
        {0xFF, 0xBF, 0xFF},
        32,
        0,
        0x400,
        96.0f,
        SKY_IMAGE_KIND_0,
    },
    {
        {0, 0, 0, 0},
        24,
        {0xF7, 0xFF, 0xBF},
        {0xFF, 0xBF, 0x7F},
        96,
        0,
        0x2AB,
        64.0f,
        SKY_IMAGE_KIND_1,
    },
    {
        {0, 0, 0, 0},
        24,
        {0xFF, 0x40, 0x00},
        {0xFF, 0xEF, 0x7F},
        96,
        0,
        0x2AB,
        64.0f,
        SKY_IMAGE_KIND_1,
    },
    {
        {0, 0, 0, 0},
        24,
        {0xFF, 0xCF, 0x00},
        {0xFF, 0xEF, 0x7F},
        96,
        0,
        0x2AB,
        64.0f,
        SKY_IMAGE_KIND_1,
    },
    {
        {0, 0, 0, 0},
        64,
        {0xFF, 0xCF, 0x00},
        {0xFF, 0xEF, 0x7F},
        48,
        0,
        0x155,
        0.0f,
        SKY_IMAGE_KIND_TWO_TEXTURES,
    },
};
// By course record, then the times of day when dry, then in the rain
SkyColors g_SkyColors[COURSE_RECORD_COUNT][SKY_WEATHER_COUNT * SKY_TIME_COUNT] = {
    {
        {
            0,
            {0x0A, 0xC8, 0xFF},
            {0xE6, 0xFF, 0xC8},
            {0xFF, 0xFF, 0xFF},
            {0x19, 0x19, 0x19, 55},
            TRUE,
            &f_Suns[MORNING_SUN],
            {0xBE, 0xDC, 0xBE},
            974,
            1100,
        },
        {
            0,
            {0x32, 0x64, 0xFF},
            {0x64, 0xFF, 0xFF},
            {0xFF, 0xFF, 0xFF},
            {0x19, 0x19, 0x19, 30},
            TRUE,
            &f_Suns[MIDDAY_SUN],
            {0xB4, 0xBE, 0xB4},
            974,
            1100,
        },
        {
            0,
            {0xFF, 0x3C, 0x64},
            {0xFF, 0xFF, 0x00},
            {0xFF, 0xC8, 0x64},
            {0x19, 0x0A, 0x00, 50},
            TRUE,
            &f_Suns[SUNSET_SUN],
            {0xE6, 0xAA, 0x96},
            975,
            1100,
        },
        {
            0,
            {0x00, 0x00, 0x64},
            {0x00, 0x00, 0xFF},
            {0x00, 0x00, 0xC8},
            {0x00, 0x00, 0x0A, 70},
            FALSE,
            &f_Suns[MOON],
            {0x00, 0x00, 0x7F},
            970,
            1100,
        },
        {
            0,
            {0x96, 0x96, 0x96},
            {0xFF, 0xFF, 0xFF},
            {0x96, 0x96, 0x96},
            {0x19, 0x19, 0x19, 65},
            FALSE,
            &f_Suns[MORNING_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1060,
        },
        {
            0,
            {0xB0, 0xB4, 0xCB},
            {0xAF, 0xAB, 0xAB},
            {0x3F, 0x3F, 0x57},
            {0x14, 0x14, 0x18, 60},
            FALSE,
            &f_Suns[MIDDAY_SUN],
            {0x80, 0x7A, 0xBC},
            970,
            1080,
        },
        {
            0,
            {0x96, 0x96, 0x96},
            {0xFF, 0xFF, 0xC8},
            {0x96, 0x96, 0x96},
            {0x19, 0x14, 0x14, 60},
            FALSE,
            &f_Suns[RAINY_SUNSET_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1080,
        },
        {
            0,
            {0x20, 0x20, 0x2F},
            {0x50, 0x50, 0x50},
            {0x64, 0x68, 0x50},
            {0x06, 0x06, 0x08, 90},
            FALSE,
            &f_Suns[MOON],
            {0xC8, 0xE6, 0xC8},
            970,
            1080,
        },
    },
    {
        {
            0,
            {0x32, 0x96, 0xFF},
            {0xFF, 0xFF, 0xD7},
            {0xFF, 0xFF, 0xFF},
            {0x19, 0x19, 0x19, 55},
            TRUE,
            &f_Suns[MORNING_SUN],
            {0xC8, 0xE8, 0xC8},
            970,
            1080,
        },
        {
            0,
            {0x32, 0x32, 0xFF},
            {0x64, 0xFF, 0xFF},
            {0xFF, 0xFF, 0xFF},
            {0x19, 0x19, 0x19, 30},
            TRUE,
            &f_Suns[MIDDAY_SUN],
            {0xC8, 0xE6, 0xC8},
            974,
            1120,
        },
        {
            0,
            {0xFF, 0x3C, 0x64},
            {0xFF, 0xFF, 0x00},
            {0x97, 0x1F, 0x00},
            {0x19, 0x05, 0x01, 50},
            TRUE,
            &f_Suns[SUNSET_SUN],
            {0xFA, 0x57, 0x50},
            942,
            1144,
        },
        {
            0,
            {0x00, 0x00, 0x28},
            {0x00, 0x00, 0xC8},
            {0x00, 0x97, 0x14},
            {0x00, 0x00, 0x0A, 85},
            FALSE,
            &f_Suns[MOON],
            {0x00, 0x00, 0x7F},
            970,
            1110,
        },
        {
            0,
            {0xA0, 0x96, 0x96},
            {0xFF, 0xFF, 0xD7},
            {0x96, 0x96, 0x96},
            {0x16, 0x16, 0x13, 65},
            FALSE,
            &f_Suns[MORNING_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1060,
        },
        {
            0,
            {0xAC, 0xB4, 0xCB},
            {0xAF, 0xAB, 0xAB},
            {0x96, 0x96, 0x96},
            {0x14, 0x14, 0x16, 60},
            FALSE,
            &f_Suns[MIDDAY_SUN],
            {0x80, 0x7A, 0xA8},
            970,
            1080,
        },
        {
            0,
            {0x96, 0x96, 0x96},
            {0xFF, 0xFF, 0xC8},
            {0x96, 0x96, 0x64},
            {0x16, 0x12, 0x12, 60},
            FALSE,
            &f_Suns[RAINY_SUNSET_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1080,
        },
        {
            0,
            {0x20, 0x20, 0x2F},
            {0x64, 0x64, 0x64},
            {0x50, 0x50, 0x64},
            {0x06, 0x06, 0x08, 85},
            FALSE,
            &f_Suns[MOON],
            {0xC8, 0xE6, 0xC8},
            970,
            1080,
        },
    },
    {
        {
            0,
            {0x1F, 0x1F, 0xFF},
            {0xFF, 0xE7, 0x3F},
            {0xDF, 0xBF, 0xDF},
            {0x17, 0x14, 0x11, 50},
            TRUE,
            &f_Suns[MORNING_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1100,
        },
        {
            0,
            {0x00, 0x1F, 0xFF},
            {0xFF, 0xFF, 0xFF},
            {0xBF, 0xBF, 0xFF},
            {0x0F, 0x0F, 0x0F, 35},
            TRUE,
            &f_Suns[MIDDAY_SUN],
            {0xB4, 0xC8, 0xB4},
            974,
            1110,
        },
        {
            0,
            {0xFF, 0x3F, 0x00},
            {0xBF, 0xFF, 0x8F},
            {0xFF, 0xBF, 0x00},
            {0x0A, 0x05, 0x01, 50},
            TRUE,
            &f_Suns[SUNSET_SUN],
            {0xFF, 0x96, 0x3C},
            970,
            1110,
        },
        {
            0,
            {0x00, 0x00, 0x32},
            {0x00, 0x00, 0xFF},
            {0x00, 0x00, 0x96},
            {0x00, 0x00, 0x0F, 80},
            FALSE,
            &f_Suns[MOON],
            {0x00, 0x00, 0x7F},
            970,
            1100,
        },
        {
            0,
            {0x96, 0x96, 0xC8},
            {0xFF, 0xFF, 0xFF},
            {0x96, 0x96, 0x96},
            {0x19, 0x19, 0x19, 65},
            FALSE,
            &f_Suns[MORNING_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1060,
        },
        {
            0,
            {0xB4, 0xB4, 0xC8},
            {0xAF, 0xAF, 0xA0},
            {0x96, 0x96, 0x96},
            {0x14, 0x14, 0x16, 60},
            FALSE,
            &f_Suns[MIDDAY_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1080,
        },
        {
            0,
            {0x96, 0x96, 0x96},
            {0xC8, 0x64, 0x96},
            {0x50, 0x50, 0x64},
            {0x12, 0x0C, 0x12, 60},
            FALSE,
            &f_Suns[RAINY_SUNSET_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1080,
        },
        {
            0,
            {0x20, 0x03, 0x20},
            {0x64, 0x64, 0x64},
            {0x50, 0x50, 0x64},
            {0x06, 0x05, 0x08, 90},
            FALSE,
            &f_Suns[MOON],
            {0xC8, 0xE6, 0xC8},
            970,
            1080,
        },
    },
    {
        {
            0,
            {0x97, 0x64, 0xFF},
            {0xF3, 0xEF, 0x83},
            {0xC8, 0xC8, 0xC8},
            {0x14, 0x14, 0x14, 55},
            TRUE,
            &f_Suns[MORNING_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1080,
        },
        {
            0,
            {0x64, 0xB4, 0xEF},
            {0xC8, 0xEF, 0xB4},
            {0xC8, 0xC8, 0xC8},
            {0x14, 0x14, 0x14, 35},
            TRUE,
            &f_Suns[MIDDAY_SUN],
            {0xC8, 0xE6, 0xC8},
            974,
            1100,
        },
        {
            0,
            {0x90, 0x32, 0xC8},
            {0xFF, 0xC8, 0x50},
            {0xFF, 0x64, 0x64},
            {0x14, 0x05, 0x0A, 50},
            TRUE,
            &f_Suns[SUNSET_SUN],
            {0xFA, 0x64, 0x64},
            970,
            1100,
        },
        {
            0,
            {0x00, 0x00, 0x64},
            {0x00, 0x00, 0xFF},
            {0x00, 0x00, 0x8B},
            {0x00, 0x02, 0x08, 80},
            FALSE,
            &f_Suns[MOON],
            {0x00, 0x00, 0x7F},
            970,
            1100,
        },
        {
            0,
            {0x6F, 0x64, 0xAB},
            {0xAF, 0xBF, 0xC8},
            {0x64, 0x64, 0x64},
            {0x10, 0x10, 0x10, 65},
            FALSE,
            &f_Suns[MORNING_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1060,
        },
        {
            0,
            {0x9B, 0x9B, 0xDF},
            {0xAF, 0xBF, 0xC8},
            {0x64, 0x64, 0x64},
            {0x12, 0x12, 0x12, 55},
            FALSE,
            &f_Suns[MIDDAY_SUN],
            {0xC8, 0xE6, 0xC8},
            966,
            1080,
        },
        {
            0,
            {0xB7, 0x64, 0xAB},
            {0xBF, 0xBF, 0x9F},
            {0x50, 0x50, 0x50},
            {0x10, 0x0C, 0x0C, 55},
            FALSE,
            &f_Suns[RAINY_SUNSET_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1080,
        },
        {
            0,
            {0x32, 0x32, 0x32},
            {0x4B, 0x4B, 0x4B},
            {0x00, 0x9F, 0x8B},
            {0x05, 0x05, 0x07, 90},
            FALSE,
            &f_Suns[MOON],
            {0xC8, 0xE6, 0xC8},
            970,
            1080,
        },
    },
    {
        {
            0,
            {0x32, 0x96, 0xC8},
            {0xFF, 0xEB, 0xC8},
            {0x96, 0x96, 0xFF},
            {0x0F, 0x0F, 0x0F, 50},
            TRUE,
            &f_Suns[MORNING_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1080,
        },
        {
            0,
            {0x00, 0x64, 0xFF},
            {0xC8, 0xEB, 0xFF},
            {0xC8, 0xC8, 0xFF},
            {0x0F, 0x0F, 0x0F, 40},
            TRUE,
            &f_Suns[MIDDAY_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1100,
        },
        {
            0,
            {0xC8, 0x32, 0xC8},
            {0xFF, 0xEB, 0x64},
            {0xC8, 0xC8, 0xFF},
            {0x19, 0x05, 0x0A, 50},
            TRUE,
            &f_Suns[SUNSET_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1100,
        },
        {
            0,
            {0x00, 0x00, 0x64},
            {0x00, 0x1F, 0xFF},
            {0x00, 0x00, 0x96},
            {0x00, 0x02, 0x0F, 80},
            FALSE,
            &f_Suns[MOON],
            {0x20, 0x20, 0xBF},
            970,
            1100,
        },
        {
            0,
            {0x64, 0x96, 0x96},
            {0xFF, 0xEB, 0xC8},
            {0x64, 0x64, 0x64},
            {0x14, 0x14, 0x14, 60},
            FALSE,
            &f_Suns[MORNING_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1060,
        },
        {
            0,
            {0x96, 0x96, 0x96},
            {0xEB, 0xEB, 0xEB},
            {0x64, 0x64, 0x96},
            {0x14, 0x14, 0x14, 60},
            FALSE,
            &f_Suns[MIDDAY_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1080,
        },
        {
            0,
            {0x96, 0x64, 0x96},
            {0xFF, 0xEB, 0xC8},
            {0x64, 0x64, 0x64},
            {0x12, 0x0F, 0x12, 60},
            FALSE,
            &f_Suns[RAINY_SUNSET_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1080,
        },
        {
            0,
            {0x32, 0x20, 0x32},
            {0x50, 0x50, 0x50},
            {0x64, 0x64, 0x64},
            {0x08, 0x06, 0x08, 90},
            FALSE,
            &f_Suns[MOON],
            {0xC8, 0xE6, 0xC8},
            970,
            1080,
        },
    },
    {
        {
            0,
            {0x7B, 0x93, 0xFF},
            {0xFF, 0xBF, 0xA3},
            {0xDF, 0xFF, 0xE7},
            {0x14, 0x14, 0x14, 55},
            TRUE,
            &f_Suns[MORNING_SUN],
            {0xA0, 0xB6, 0xC8},
            962,
            1024,
        },
        {
            0,
            {0x74, 0x1F, 0xFF},
            {0xB7, 0xFF, 0xFF},
            {0xFF, 0xFF, 0xFF},
            {0x12, 0x12, 0x12, 40},
            TRUE,
            &f_Suns[MIDDAY_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1100,
        },
        {
            0,
            {0xDF, 0x1F, 0xFF},
            {0xFF, 0xCF, 0x4B},
            {0x77, 0x0B, 0xBF},
            {0x14, 0x0A, 0x08, 50},
            TRUE,
            &f_Suns[SUNSET_SUN],
            {0xFF, 0x62, 0xB4},
            970,
            1048,
        },
        {
            0,
            {0x3F, 0x03, 0x74},
            {0x1B, 0x07, 0x9B},
            {0x67, 0x77, 0xC8},
            {0x00, 0x00, 0x09, 80},
            FALSE,
            &f_Suns[MOON],
            {0x08, 0x00, 0xB4},
            954,
            1024,
        },
        {
            0,
            {0x93, 0x93, 0x93},
            {0xB7, 0xB7, 0x9B},
            {0x64, 0x73, 0x97},
            {0x0C, 0x0C, 0x0E, 65},
            FALSE,
            &f_Suns[MORNING_SUN],
            {0x94, 0xA6, 0xBC},
            954,
            1016,
        },
        {
            0,
            {0x7B, 0x93, 0x9B},
            {0xAB, 0xAB, 0xAB},
            {0x4B, 0x73, 0x64},
            {0x0A, 0x0E, 0x0E, 60},
            FALSE,
            &f_Suns[MIDDAY_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1048,
        },
        {
            0,
            {0x7B, 0x93, 0xA3},
            {0xC8, 0x83, 0x7B},
            {0x37, 0x73, 0x37},
            {0x0A, 0x0A, 0x08, 60},
            FALSE,
            &f_Suns[RAINY_SUNSET_SUN],
            {0x8F, 0x72, 0xB7},
            962,
            1040,
        },
        {
            0,
            {0x27, 0x33, 0x4C},
            {0x37, 0x27, 0x2F},
            {0x6F, 0x3F, 0x9C},
            {0x02, 0x03, 0x07, 80},
            FALSE,
            &f_Suns[MOON],
            {0x10, 0x34, 0x7C},
            954,
            1012,
        },
    },
    {
        {
            0,
            {0x00, 0x00, 0x9F},
            {0xBF, 0xBF, 0xFF},
            {0x00, 0x00, 0xB7},
            {0x17, 0x14, 0x11, 0},
            TRUE,
            &f_Suns[MORNING_SUN],
            {0xFF, 0xFF, 0xFF},
            1070,
            1128,
        },
        {
            0,
            {0x00, 0x00, 0xFF},
            {0xBF, 0xEF, 0xFF},
            {0xBF, 0xFF, 0xFF},
            {0x0F, 0x0F, 0x0F, 0},
            TRUE,
            &f_Suns[MIDDAY_SUN],
            {0xFF, 0xFF, 0xFF},
            1070,
            1128,
        },
        {
            0,
            {0x8F, 0x5F, 0xCF},
            {0xFF, 0xDF, 0x00},
            {0xFF, 0x3F, 0x00},
            {0x0A, 0x05, 0x01, 0},
            TRUE,
            &f_Suns[SUNSET_SUN],
            {0xFF, 0xFF, 0xFF},
            1070,
            1128,
        },
        {
            0,
            {0x00, 0x00, 0x7F},
            {0x00, 0x00, 0x3F},
            {0x7F, 0x7F, 0x7F},
            {0x0A, 0x05, 0x01, 0},
            FALSE,
            &f_Suns[MOON],
            {0xFF, 0xFF, 0xFF},
            1070,
            1128,
        },
        {
            0,
            {0xA0, 0xC4, 0xAF},
            {0x0F, 0x2F, 0x4B},
            {0xFF, 0xDB, 0xBB},
            {0x0F, 0x0F, 0x0F, 60},
            FALSE,
            &f_Suns[MORNING_SUN],
            {0xFF, 0xFF, 0xFF},
            1070,
            1128,
        },
        {
            0,
            {0x6F, 0x6F, 0x7F},
            {0x7F, 0x7F, 0x5F},
            {0x40, 0x40, 0x40},
            {0x1E, 0x1E, 0x1E, 50},
            FALSE,
            &f_Suns[MIDDAY_SUN],
            {0xFF, 0xFF, 0xFF},
            1070,
            1128,
        },
        {
            0,
            {0xDF, 0xBF, 0xFF},
            {0xFF, 0xEF, 0x3F},
            {0x5F, 0x3F, 0x1F},
            {0x14, 0x0F, 0x0A, 70},
            FALSE,
            &f_Suns[RAINY_SUNSET_SUN],
            {0xFF, 0xFF, 0xFF},
            1070,
            1128,
        },
        {
            0,
            {0x00, 0x00, 0x7F},
            {0x00, 0x00, 0x3F},
            {0x7F, 0x7F, 0x7F},
            {0x0A, 0x05, 0x01, 0},
            FALSE,
            &f_Suns[MOON],
            {0xFF, 0xFF, 0xFF},
            1070,
            1128,
        },
    },
    {
        {
            0,
            {0x00, 0x00, 0x9F},
            {0xBF, 0xBF, 0xFF},
            {0x00, 0x00, 0xB7},
            {0x17, 0x14, 0x11, 0},
            TRUE,
            &f_Suns[MORNING_SUN],
            {0xFF, 0xFF, 0xFF},
            1070,
            1128,
        },
        {
            0,
            {0x00, 0x00, 0xFF},
            {0xBF, 0xEF, 0xFF},
            {0xBF, 0xFF, 0xFF},
            {0x0F, 0x0F, 0x0F, 0},
            TRUE,
            &f_Suns[MIDDAY_SUN],
            {0xFF, 0xFF, 0xFF},
            1070,
            1128,
        },
        {
            0,
            {0x8F, 0x5F, 0xCF},
            {0xFF, 0xDF, 0x00},
            {0xFF, 0x3F, 0x00},
            {0x0A, 0x05, 0x01, 0},
            TRUE,
            &f_Suns[SUNSET_SUN],
            {0xFF, 0xFF, 0xFF},
            1070,
            1128,
        },
        {
            0,
            {0x00, 0x00, 0x7F},
            {0x00, 0x00, 0x3F},
            {0x7F, 0x7F, 0x7F},
            {0x0A, 0x05, 0x01, 0},
            FALSE,
            &f_Suns[MOON],
            {0xFF, 0xFF, 0xFF},
            1070,
            1128,
        },
        {
            0,
            {0xA0, 0xC4, 0xAF},
            {0x0F, 0x2F, 0x4B},
            {0xFF, 0xDB, 0xBB},
            {0x0F, 0x0F, 0x0F, 60},
            FALSE,
            &f_Suns[MORNING_SUN],
            {0xFF, 0xFF, 0xFF},
            1070,
            1128,
        },
        {
            0,
            {0x6F, 0x6F, 0x7F},
            {0x7F, 0x7F, 0x5F},
            {0x40, 0x40, 0x40},
            {0x1E, 0x1E, 0x1E, 50},
            FALSE,
            &f_Suns[MIDDAY_SUN],
            {0xFF, 0xFF, 0xFF},
            1070,
            1128,
        },
        {
            0,
            {0xDF, 0xBF, 0xFF},
            {0xFF, 0xEF, 0x3F},
            {0x5F, 0x3F, 0x1F},
            {0x14, 0x0F, 0x0A, 70},
            FALSE,
            &f_Suns[RAINY_SUNSET_SUN],
            {0xFF, 0xFF, 0xFF},
            1070,
            1128,
        },
        {
            0,
            {0x00, 0x00, 0x7F},
            {0x00, 0x00, 0x3F},
            {0x7F, 0x7F, 0x7F},
            {0x0A, 0x05, 0x01, 0},
            FALSE,
            &f_Suns[MOON],
            {0xFF, 0xFF, 0xFF},
            1070,
            1128,
        },
    },
    {
        {
            0,
            {0x32, 0x64, 0xFF},
            {0x64, 0xFF, 0xFF},
            {0xFF, 0xFF, 0xFF},
            {0x19, 0x19, 0x19, 35},
            TRUE,
            &f_Suns[MIDDAY_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1100,
        },
        {
            0,
            {0x32, 0x64, 0xFF},
            {0x64, 0xFF, 0xFF},
            {0xFF, 0xFF, 0xFF},
            {0x19, 0x19, 0x19, 35},
            TRUE,
            &f_Suns[MIDDAY_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1100,
        },
        {
            0,
            {0x32, 0x64, 0xFF},
            {0x64, 0xFF, 0xFF},
            {0xFF, 0xFF, 0xFF},
            {0x19, 0x19, 0x19, 35},
            TRUE,
            &f_Suns[MIDDAY_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1100,
        },
        {
            0,
            {0x00, 0x00, 0x64},
            {0x00, 0x00, 0xFF},
            {0x00, 0x00, 0xC8},
            {0x00, 0x00, 0x0F, 90},
            FALSE,
            &f_Suns[MOON],
            {0xC8, 0xE6, 0xC8},
            970,
            1100,
        },
        {
            0,
            {0x96, 0x96, 0x96},
            {0xFF, 0xFF, 0xFF},
            {0x96, 0x96, 0x96},
            {0x19, 0x19, 0x19, 65},
            FALSE,
            &f_Suns[MORNING_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1060,
        },
        {
            0,
            {0xB0, 0xB4, 0xCB},
            {0xAF, 0xAB, 0xAB},
            {0x3F, 0x3F, 0x57},
            {0x14, 0x14, 0x18, 60},
            FALSE,
            &f_Suns[MIDDAY_SUN],
            {0x80, 0x7A, 0xBC},
            970,
            1080,
        },
        {
            0,
            {0x96, 0x96, 0x96},
            {0xFF, 0xFF, 0xC8},
            {0x96, 0x96, 0x96},
            {0x19, 0x14, 0x14, 60},
            FALSE,
            &f_Suns[RAINY_SUNSET_SUN],
            {0xC8, 0xE6, 0xC8},
            970,
            1080,
        },
        {
            0,
            {0x20, 0x20, 0x2F},
            {0x50, 0x50, 0x50},
            {0x64, 0x68, 0x50},
            {0x06, 0x06, 0x08, 90},
            FALSE,
            &f_Suns[MOON],
            {0xC8, 0xE6, 0xC8},
            970,
            1080,
        },
    },
};
static u16 f_SunDepth = 0;
static Boolean f_IsSunGlareSettled = FALSE;
static f32 f_SunGlareStrength = 0.0f;
static LensFlare f_LensFlares[LENS_FLARE_COUNT] = {
    {{0xDF, 0xDF, 0xFF}, {0xFF, 0xFF, 0xFF}, 24, 2728, -1.0f},
    {{0xC0, 0xC0, 0xFF}, {0xC0, 0xFF, 0xFF}, 8, 8192, -0.5f},
    {{0xFF, 0xDF, 0xC0}, {0xFF, 0x00, 0xC0}, 8, 5460, 0.3f},
    {{0xC0, 0xFF, 0xDF}, {0xFF, 0xFF, 0xFF}, 16, 4096, 0.5f},
    {{0xFF, 0xFF, 0xBF}, {0xFF, 0xFF, 0xFF}, 16, 2728, 0.8f},
    {{0xBF, 0xFF, 0xBF}, {0xFF, 0xFF, 0xFF}, 8, 2048, 1.0f},
    {{0xDF, 0xFF, 0xDF}, {0xFF, 0xFF, 0xFF}, 48, 1024, 1.3f},
    {{0xBF, 0xFF, 0xBF}, {0xFF, 0x00, 0x00}, 32, 2728, 1.7f},
};
static Vtx f_SkyVertices[SKY_VIEW_COUNT * MATRIX_BUFFER_COUNT][SKY_VERTEX_COUNT] = {
    {
        {{{-1200, 0, 0}, 0, {0, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{-1200, -160, -2048}, 0, {0, 0x3C00}, {0xC0, 0x40, 0x00, 0xFF}}},
        {{{1200, -160, -2048}, 0, {0x5000, 0x3C00}, {0xC0, 0x40, 0x00, 0xFF}}},
        {{{1200, 0, 0}, 0, {0x5000, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{0, 0, 0}, 0, {0x2800, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{-1200, -320, 0}, 0, {0, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{-1200, -160, -2048}, 0, {0, 0x3C00}, {0xC0, 0x40, 0x00, 0xFF}}},
        {{{1200, -160, -2048}, 0, {0x5000, 0x3C00}, {0xC0, 0x40, 0x00, 0xFF}}},
        {{{1200, -320, 0}, 0, {0x5000, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{0, -320, 0}, 0, {0x2800, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
    },
    {
        {{{-1200, 0, 0}, 0, {0, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{-1200, -160, -2048}, 0, {0, 0x3C00}, {0xC0, 0x40, 0x00, 0xFF}}},
        {{{1200, -160, -2048}, 0, {0x5000, 0x3C00}, {0xC0, 0x40, 0x00, 0xFF}}},
        {{{1200, 0, 0}, 0, {0x5000, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{0, 0, 0}, 0, {0x2800, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{-1200, -320, 0}, 0, {0, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{-1200, -160, -2048}, 0, {0, 0x3C00}, {0xC0, 0x40, 0x00, 0xFF}}},
        {{{1200, -160, -2048}, 0, {0x5000, 0x3C00}, {0xC0, 0x40, 0x00, 0xFF}}},
        {{{1200, -320, 0}, 0, {0x5000, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{0, -320, 0}, 0, {0x2800, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
    },
    {
        {{{-1200, 0, 0}, 0, {0, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{-1200, -160, -2048}, 0, {0, 0x3C00}, {0xC0, 0x40, 0x00, 0xFF}}},
        {{{1200, -160, -2048}, 0, {0x5000, 0x3C00}, {0xC0, 0x40, 0x00, 0xFF}}},
        {{{1200, 0, 0}, 0, {0x5000, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{0, 0, 0}, 0, {0x2800, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{-1200, -320, 0}, 0, {0, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{-1200, -160, -2048}, 0, {0, 0x3C00}, {0xC0, 0x40, 0x00, 0xFF}}},
        {{{1200, -160, -2048}, 0, {0x5000, 0x3C00}, {0xC0, 0x40, 0x00, 0xFF}}},
        {{{1200, -320, 0}, 0, {0x5000, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{0, -320, 0}, 0, {0x2800, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
    },
    {
        {{{-1200, 0, 0}, 0, {0, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{-1200, -160, -2048}, 0, {0, 0x3C00}, {0xC0, 0x40, 0x00, 0xFF}}},
        {{{1200, -160, -2048}, 0, {0x5000, 0x3C00}, {0xC0, 0x40, 0x00, 0xFF}}},
        {{{1200, 0, 0}, 0, {0x5000, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{0, 0, 0}, 0, {0x2800, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{-1200, -320, 0}, 0, {0, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{-1200, -160, -2048}, 0, {0, 0x3C00}, {0xC0, 0x40, 0x00, 0xFF}}},
        {{{1200, -160, -2048}, 0, {0x5000, 0x3C00}, {0xC0, 0x40, 0x00, 0xFF}}},
        {{{1200, -320, 0}, 0, {0x5000, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
        {{{0, -320, 0}, 0, {0x2800, 0}, {0xFF, 0xE7, 0x90, 0xFF}}},
    },
};
static s32 f_BackdropStripCount = INITIAL_BACKDROP_STRIP_COUNT;
static s32 f_BackdropStripHeight = INITIAL_BACKDROP_STRIP_HEIGHT;
static s32 f_BackdropImageWidth = INITIAL_BACKDROP_IMAGE_WIDTH;
static s32 f_BackdropWidthMask = BACKDROP_IMAGE_WIDTH_MASK;
static WideAssetId f_CourseSkyImageAssets[COURSE_RECORD_COUNT] =
    {0x4BA, 0x4BB, 0x4BC, 0x4BE, 0x4BD, 0x4BF, 0x4C0, 0x4C1, 0x4BA};
Fog g_SceneFog = {{0, 0, 0}, 970, 1024};
static s32 f_UnusedWordsAfterSceneFog[3] UNUSED = {0, 0, 1};
static s32 f_SkyHorizonColor[RGB_CHANNEL_COUNT] =
    {SKY_HORIZON_LEVEL, SKY_HORIZON_LEVEL, SKY_HORIZON_LEVEL};
static f32 f_UnusedPlaneCornerPoints[12] UNUSED =
    {0.0f, 0.0f, 0.0f, 4096.0f, 0.0f, 0.0f, 0.0f, 0.0f, 8192.0f, 4096.0f, 0.0f, 8192.0f};
// The fog SetSceneFog draws while GAME_FLAG_SKY_EDITOR is set
Fog g_DebugFog = {{200, 230, 200}, 970, 1100};
static SkyEditorColumn f_SkyEditorColumn = SKY_EDITOR_TOP;
static s32 f_SkyEditorRow = 0;
static s32 f_UnusedWordsAfterSkyEditorRow[3] UNUSED = {0};
static Vtx f_TileGridVertices[2][TILE_GRID_ROW_COUNT * TILE_GRID_COLUMN_COUNT] = {
    {
        {{{-152, 104, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-88, 104, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-24, 104, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{40, 104, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{104, 104, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{168, 104, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-152, 72, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-88, 72, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-24, 72, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{40, 72, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{104, 72, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{168, 72, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-152, 40, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-88, 40, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-24, 40, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{40, 40, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{104, 40, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{168, 40, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-152, 8, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-88, 8, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-24, 8, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{40, 8, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{104, 8, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{168, 8, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-152, -24, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-88, -24, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-24, -24, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{40, -24, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{104, -24, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{168, -24, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-152, -56, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-88, -56, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-24, -56, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{40, -56, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{104, -56, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{168, -56, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-152, -88, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-88, -88, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-24, -88, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{40, -88, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{104, -88, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{168, -88, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-152, -120, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-88, -120, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-24, -120, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{40, -120, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{104, -120, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{168, -120, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
    },
    {
        {{{-152, 104, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-88, 104, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-24, 104, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{40, 104, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{104, 104, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{168, 104, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-152, 72, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-88, 72, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-24, 72, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{40, 72, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{104, 72, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{168, 72, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-152, 40, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-88, 40, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-24, 40, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{40, 40, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{104, 40, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{168, 40, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-152, 8, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-88, 8, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-24, 8, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{40, 8, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{104, 8, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{168, 8, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-152, -24, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-88, -24, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-24, -24, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{40, -24, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{104, -24, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{168, -24, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-152, -56, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-88, -56, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-24, -56, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{40, -56, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{104, -56, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{168, -56, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-152, -88, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-88, -88, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-24, -88, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{40, -88, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{104, -88, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{168, -88, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-152, -120, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-88, -120, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{-24, -120, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{40, -120, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{104, -120, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
        {{{168, -120, 0}, 0, {0, 0}, {0x7F, 0x7F, 0x7F, 0xFF}}},
    },
};

#ifdef AVOID_UB
void GetSkyColorsNeighbors(SkyColorsNeighbors *neighbors) {
    neighbors->isGlareSettled = f_IsSunGlareSettled;
    neighbors->glareStrength = f_SunGlareStrength;
    neighbors->firstFlareColor = f_LensFlares[0].color;
    neighbors->skyVertices = f_SkyVertices;
    neighbors->tileGridVertices = f_TileGridVertices;
}
#endif

static Boolean f_IsFrameCapturePending = FALSE;
static s32 f_HudAnimationFrame = 0;
s32 g_CapturedSliceCount = 0;
MenuBackdropState g_MenuBackdropState = MENU_BACKDROP_STATE_NONE;
// Counted by DrawFadeAnimation, and read by no other routine
static s32 f_UnusedFadeEndCount = 0;
static MenuBackdrop f_MenuBackdrop = MENU_BACKDROP_STILL;
static s32 f_BackdropFadeLevel = 0;
static s32 f_BackdropPatternScroll = 0;
static Vtx f_TileVertices[TILE_VERTEX_COUNT] = {
    {{{-32, 16, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-32, -16, 0}, 0, {0, 0x800}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{32, -16, 0}, 0, {0x1000, 0x800}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{32, 16, 0}, 0, {0x1000, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
};
static Vtx f_LastColumnTileVertices[TILE_VERTEX_COUNT] = {
    {{{-32, 16, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-32, -16, 0}, 0, {0, 0x800}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{16, -16, 0}, 0, {0xBC0, 0x800}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{16, 16, 0}, 0, {0xBC0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
};

ImageAsset *g_BackdropPatternImage;
ImageAsset *g_CourseSkyImage;
ImageAsset *g_SkyImage;
SkyState g_SkyState;

// The frame buffer of the last frame, kept for the next frame's captures
FILE_STATIC u16 *f_LastFrameBuffer;

// Sets up a single-player game of the hole behind the title screen
void SetUpTitleHole(void) {
    memset(&g_GameState, 0, sizeof(GameState));
    g_GameState.holeIndex = TITLE_HOLE_INDEX;
    g_GameState.courseIndex = TITLE_COURSE_INDEX;
    g_GameState.unusedHoleValue = 0;
    g_GameState.gameMode = GAME_MODE_TITLE_HOLE;
    g_HoleSetup.conditions.isRaining = FALSE;
    g_HoleSetup.conditions.windSpeed = (rand() & RANDOM_WIND_STEP_MASK) * TITLE_WIND_SPEED_STEP;
    g_HoleSetup.conditions.windDirection = rand();
    g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_DRIVING_RANGE;
    g_HoleSetup.conditions.lightPitch = TITLE_LIGHT_PITCH;
    g_HoleSetup.conditions.lightYaw = TITLE_LIGHT_YAW;
    SetGameFlag(GAME_FLAG_HOLE_LOADED);
    g_GameState.playerCount = 1;
    StopLoaderThread();
    StartLoaderThread();
    SetFramebufferCount(DOUBLE_BUFFERING);
}

// Grows the sun's glare until it flares, then holds it at full strength
static inline void GrowSunGlare(void) {
    if (!f_IsSunGlareSettled) {
        f_SunGlareStrength += SUN_GLARE_GROWTH_STEP;
    }

    if (f_SunGlareStrength > SUN_GLARE_FLARE_STRENGTH && !f_IsSunGlareSettled) {
        f_IsSunGlareSettled = TRUE;
        f_SunGlareStrength = SUN_GLARE_FULL_STRENGTH;
    }
}

#ifdef AVOID_UB
// The scheduler runs UpdateSunGlare as each frame's task ends, and it reads
// that frame's z-buffer where the sun is. The matching build reads f_SunScreenX
// and f_SunScreenY, which the graphics thread may already have moved for a
// frame queued after, so which pixel it reads depends on how far the RDP
// lags. Each framebuffer keeps where the sun was in the frame queued to it
// instead. While the game paces frames normally, it queues a frame only
// while fewer than two are pending, so the frame queued before to the same
// framebuffer has ended by then.
static f32 f_DrawnSunScreenX[TRIPLE_BUFFERING];
static f32 f_DrawnSunScreenY[TRIPLE_BUFFERING];

void RecordDrawnSunPosition(void) {
    f_DrawnSunScreenX[nuGfxCfbCounter] = f_SunScreenX[0];
    f_DrawnSunScreenY[nuGfxCfbCounter] = f_SunScreenY[0];
}

// The index in nuGfxCfb of FRAMEBUFFER, one of its framebuffers
static s32 FramebufferIndex(const void *framebuffer) {
    s32 index;

    for (index = 0; index < TRIPLE_BUFFERING - 1; index++) {
        if (nuGfxCfb[index] == framebuffer) {
            break;
        }
    }
    return index;
}

#    define GlareSunX(frameIndex) f_DrawnSunScreenX[frameIndex]
#    define GlareSunY(frameIndex) f_DrawnSunScreenY[frameIndex]
#else
#    define GlareSunX(frameIndex) f_SunScreenX[0]
#    define GlareSunY(frameIndex) f_SunScreenY[0]
#endif

// Grows or fades the sun's glare by whether the z-buffer shows anything in
// front of the sun. AVOID_UB passes the framebuffer of the frame that ended.
#ifdef AVOID_UB
void UpdateSunGlare(const void *framebuffer) {
    u16 *zBuffer;
    s32 frameIndex = FramebufferIndex(framebuffer);
#else
void UpdateSunGlare(void) {
    u16 *zBuffer;
#endif

    if (IsInsetOnScreen(GlareSunX(frameIndex), GlareSunY(frameIndex), SUN_DEPTH_MARGIN)) {
        zBuffer = nuGfxZBuffer;
        f_SunDepth =
            zBuffer[((s32)GlareSunY(frameIndex) * SCREEN_WIDTH) + (s32)GlareSunX(frameIndex)];
        if (f_SunDepth >= SUN_SKY_DEPTH) {
            GrowSunGlare();
        }
        else {
            f_IsSunGlareSettled = FALSE;
            f_SunGlareStrength -= SUN_GLARE_FADE_STEP;
            if (f_SunGlareStrength < 0.0f) {
                f_SunGlareStrength = 0.0f;
            }
        }
    }
    else if (IsNearScreen(GlareSunX(frameIndex), GlareSunY(frameIndex), SUN_GLARE_MARGIN)) {
        GrowSunGlare();
    }
    else {
        f_SunDepth = 0;
        f_IsSunGlareSettled = FALSE;
        f_SunGlareStrength = 0.0f;
    }
}

// The lens flare is drawn while the sun is on the screen, away from its
// edges, and the inset view isn't drawing the scene. The wider bounds that
// are also tested can't fail after the narrower ones. The glare grows with the
// sun's distance from the nearer side and from the top, or from the bottom
// within GLARE_BOTTOM_ROWS of it, and nothing is drawn below MIN_VISIBLE_GLARE.
#define LENS_FLARE_TOP_MARGIN 64.0f
// Whether a point lies further above the screen than LENS_FLARE_TOP_MARGIN, or
// further past its other edges than SUN_GLARE_MARGIN
#define IsPastLensFlareMargins(x, y) \
    ((y) < -LENS_FLARE_TOP_MARGIN || \
        (y) > SCREEN_HEIGHT + SUN_GLARE_MARGIN || \
        (x) < -SUN_GLARE_MARGIN || \
        (x) > SCREEN_WIDTH + SUN_GLARE_MARGIN)
#define GLARE_BOTTOM_ROWS 96.0f
#define GLARE_BASE 16.0f
#define VISIBLE_GLARE_MIN 8.0f
// The flares lie along the line from the sun through this point
#define LENS_FLARE_AXIS_X (SCREEN_WIDTH / 2)
#define LENS_FLARE_AXIS_Y 96.0f
// A flare's half width is its size times (glare + LENS_FLARE_SIZE_BIAS) / 128
#define LENS_FLARE_SIZE_BIAS 16.0f
#define LENS_FLARE_SIZE_SCALE (1.0f / 128)
// Only texels above this alpha are drawn
#define LENS_FLARE_ALPHA_THRESHOLD 1

// Draws the sun's lens flare, as rings along a line through the sun.
void DrawLensFlare(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    SkyColors *colors =
        &g_SkyColors[GetCourseRecordIndex()]
                    [(g_HoleSetup.conditions.isRaining * SKY_TIME_COUNT) + f_SkyColorTimeIndex];
    f32 edgeDistanceX;
    f32 edgeDistanceY;
    f32 glare;
    f32 centerX;
    f32 centerY;
    s32 size;
    s32 texelStep;
    s32 flareIndex;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    if (!colors->hasLensFlare ||
        IsOnOrPastInset(f_SunScreenX[0], f_SunScreenY[0], SUN_DEPTH_MARGIN) ||
        IsPastLensFlareMargins(f_SunScreenX[0], f_SunScreenY[0]) ||
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
    if (glare < VISIBLE_GLARE_MIN) {
        return;
    }

    if (glare > colors->sun->glareMax) {
        glare = colors->sun->glareMax;
    }

    gDPPipeSync(gfx++);
    AppendSyncedRdpState(gfx, gDPSetTexturePersp, G_TP_NONE);
    AppendSyncedRdpState(gfx, gDPSetTextureLUT, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 255, 255, 255, LENS_FLARE_ALPHA_THRESHOLD);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineLERP(
        gfx++,
        PRIMITIVE,
        ENVIRONMENT,
        TEXEL0,
        ENVIRONMENT,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        PRIMITIVE,
        ENVIRONMENT,
        TEXEL0,
        ENVIRONMENT,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(g_RingTexels),
        G_IM_FMT_I,
        G_IM_SIZ_8b,
        RING_TEXTURE_SIZE,
        RING_TEXTURE_SIZE,
        0,
        G_TX_MIRROR | G_TX_WRAP,
        G_TX_MIRROR | G_TX_WRAP,
        TextureMask(RING_TEXTURE_SIZE),
        TextureMask(RING_TEXTURE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    for (flareIndex = 0; flareIndex != LENS_FLARE_COUNT; flareIndex++) {
        centerX = f_SunScreenX[0] +
            ((LENS_FLARE_AXIS_X - f_SunScreenX[0]) * f_LensFlares[flareIndex].position);
        centerY = f_SunScreenY[0] +
            ((LENS_FLARE_AXIS_Y - f_SunScreenY[0]) * f_LensFlares[flareIndex].position);
        gDPPipeSync(gfx++);
        gDPSetPrimColor(
            gfx++,
            0,
            0,
            f_LensFlares[flareIndex].color[RED_CHANNEL],
            f_LensFlares[flareIndex].color[GREEN_CHANNEL],
            f_LensFlares[flareIndex].color[BLUE_CHANNEL],
            glare
        );
        gDPSetEnvColor(
            gfx++,
            f_LensFlares[flareIndex].envColor[RED_CHANNEL],
            f_LensFlares[flareIndex].envColor[GREEN_CHANNEL],
            f_LensFlares[flareIndex].envColor[BLUE_CHANNEL],
            OPAQUE_ALPHA
        );
        size =
            f_LensFlares[flareIndex].size * (glare + LENS_FLARE_SIZE_BIAS) * LENS_FLARE_SIZE_SCALE;
        texelStep = TexelRunStep(RING_TEXTURE_SIZE, size);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels((s32)(centerX - size)),
            QuarterPixels((s32)(centerY - size)),
            QuarterPixels((s32)(centerX + size)),
            QuarterPixels((s32)(centerY + size)),
            G_TX_RENDERTILE,
            0,
            0,
            texelStep,
            texelStep
        );
    }

    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

// Moves the sky's and the fog's colors a step on from those of one time of
// day towards those of the next
void BlendSkyColors(void) {
    SkyColors *from;
    SkyColors *to;
    s32 skyIndex;
    s32 channel;

    if (f_SkyBlendStep < f_SkyBlendStepCount) {
        f_SkyBlendStep++;
    }

    from = &g_SkyColors[GetCourseRecordIndex()]
                       [(g_HoleSetup.conditions.isRaining * SKY_TIME_COUNT) + f_SkyTimeIndex];
    to = &g_SkyColors[GetCourseRecordIndex()]
                     [(g_HoleSetup.conditions.isRaining * SKY_TIME_COUNT) + f_SkyTimeIndex + 1];
    for (skyIndex = 0; skyIndex != SKY_VIEW_COUNT * MATRIX_BUFFER_COUNT; skyIndex++) {
#ifdef AVOID_UB
        // The matching build also writes the other matrix buffer's vertices, which
        // the RSP may still be reading for the previous frame. Each frame
        // blends the buffer it draws, so they need not be written now.
        if (skyIndex % MATRIX_BUFFER_COUNT != g_MatrixBufferIndex) {
            continue;
        }
#endif
        for (channel = 0; channel != RGB_CHANNEL_COUNT; channel++) {
            f_SkyVertices[skyIndex][SKY_TOP_LEFT].v.cn[channel] = from->topColor[channel] +
                ((to->topColor[channel] - from->topColor[channel]) * f_SkyBlendStep /
                    SKY_BLEND_STEP_COUNT);
            f_SkyVertices[skyIndex][SKY_HORIZON_LEFT].v.cn[channel] = from->horizonColor[channel] +
                ((to->horizonColor[channel] - from->horizonColor[channel]) * f_SkyBlendStep /
                    SKY_BLEND_STEP_COUNT);
            f_SkyVertices[skyIndex][SKY_HORIZON_RIGHT].v.cn[channel] = from->horizonColor[channel] +
                ((to->horizonColor[channel] - from->horizonColor[channel]) * f_SkyBlendStep /
                    SKY_BLEND_STEP_COUNT);
            f_SkyVertices[skyIndex][SKY_TOP_RIGHT].v.cn[channel] = from->topColor[channel] +
                ((to->topColor[channel] - from->topColor[channel]) * f_SkyBlendStep /
                    SKY_BLEND_STEP_COUNT);
            f_SkyVertices[skyIndex][SKY_TOP_CENTER].v.cn[channel] = from->topColor[channel] +
                ((to->topColor[channel] - from->topColor[channel]) * f_SkyBlendStep /
                    SKY_BLEND_STEP_COUNT);
        }
    }

    for (channel = 0; channel != RGB_CHANNEL_COUNT; channel++) {
        g_SceneFog.color[channel] = from->fogColor[channel] +
            ((to->fogColor[channel] - from->fogColor[channel]) * f_SkyBlendStep /
                SKY_BLEND_STEP_COUNT);
    }

    g_SceneFog.near =
        from->fogNear + ((to->fogNear - from->fogNear) * f_SkyBlendStep / SKY_BLEND_STEP_COUNT);
    g_SceneFog.far =
        from->fogFar + ((to->fogFar - from->fogFar) * f_SkyBlendStep / SKY_BLEND_STEP_COUNT);
}

// Sets the sky up for the hole's scene color index: its vertices' colors, the
// blend between times of day that BlendSkyColors steps through, and
// g_SkyImage and g_CourseSkyImage, recolored to match
void InitSkyColors(void) {
    EncodedAsset asset;
    u8 unused[0x10] UNUSED;
    s32 blended[RGB_CHANNEL_COUNT];
    SkyColors *colors;
    SkyColors *from;
    SkyColors *to;
    // Counts the vertex groups and the palette colors
    s32 i;
    s32 channel;
    u16 color;
    s32 red;
    s32 green;
    s32 blue;
    s32 alpha;
    s32 outerMix;
    s32 innerMix;

    f_SkyColorTimeIndex = f_SceneSkyColorTimes[g_HoleSetup.conditions.sceneColorIndex];
    f_SkyTimeIndex = f_SceneSkyBlendTimes[g_HoleSetup.conditions.sceneColorIndex];
    f_SkyBlendStep =
        f_SceneSkyBlendPercents[g_HoleSetup.conditions.sceneColorIndex] * SKY_PERCENT_TO_STEPS;
    f_SkyBlendStepCount =
        f_SceneSkyBlendPercents[g_HoleSetup.conditions.sceneColorIndex + 1] * SKY_PERCENT_TO_STEPS;
    if (f_SkyBlendStepCount == 0 &&
        f_SceneSkyBlendTimes[g_HoleSetup.conditions.sceneColorIndex] !=
            f_SceneSkyBlendTimes[g_HoleSetup.conditions.sceneColorIndex + 1]) {
        f_SkyBlendStepCount = SKY_BLEND_STEP_COUNT;
    }

    colors =
        &g_SkyColors[GetCourseRecordIndex()]
                    [(g_HoleSetup.conditions.isRaining * SKY_TIME_COUNT) + f_SkyColorTimeIndex];
    from = &g_SkyColors[GetCourseRecordIndex()]
                       [(g_HoleSetup.conditions.isRaining * SKY_TIME_COUNT) + f_SkyTimeIndex];
    to = &g_SkyColors[GetCourseRecordIndex()]
                     [(g_HoleSetup.conditions.isRaining * SKY_TIME_COUNT) + f_SkyTimeIndex + 1];
    // An unused nested function, a GNU C extension
    {
        UNUSED_FUNCTION s32 Lerp(s32 start, s32 end, f32 fraction) {
            return start * (1.0f - fraction) + end * fraction;
        }
    }

    g_SkyState.scrollX = 0.0f;
    g_SkyState.scrollY = 0.0f;
    for (i = 0; i != SKY_VIEW_COUNT * MATRIX_BUFFER_COUNT; i++) {
        for (channel = 0; channel != RGB_CHANNEL_COUNT; channel++) {
            f_SkyVertices[i][SKY_TOP_LEFT].v.cn[channel] = colors->topColor[channel];
            f_SkyVertices[i][SKY_HORIZON_LEFT].v.cn[channel] = colors->horizonColor[channel];
            f_SkyVertices[i][SKY_HORIZON_RIGHT].v.cn[channel] = colors->horizonColor[channel];
            f_SkyVertices[i][SKY_TOP_RIGHT].v.cn[channel] = colors->topColor[channel];
            f_SkyVertices[i][SKY_TOP_CENTER].v.cn[channel] = colors->topColor[channel];
            outerMix = (colors->topColor[channel] + f_SkyHorizonColor[channel]) * 0.5f;
            innerMix = (colors->horizonColor[channel] + colors->topColor[channel]) * 0.5f;
            innerMix = (innerMix + f_SkyHorizonColor[channel]) * 0.5f;
            f_SkyVertices[i][MIRRORED_SKY_FAN + SKY_TOP_LEFT].v.cn[channel] = outerMix;
            f_SkyVertices[i][MIRRORED_SKY_FAN + SKY_HORIZON_LEFT].v.cn[channel] = innerMix;
            f_SkyVertices[i][MIRRORED_SKY_FAN + SKY_HORIZON_RIGHT].v.cn[channel] = innerMix;
            f_SkyVertices[i][MIRRORED_SKY_FAN + SKY_TOP_RIGHT].v.cn[channel] = outerMix;
            f_SkyVertices[i][MIRRORED_SKY_FAN + SKY_TOP_CENTER].v.cn[channel] = outerMix;
        }
    }

    if (colors->sun->imageKind == SKY_IMAGE_KIND_0) {
        OpenEncodedAsset(ASSET_SKY_IMAGE_0, &asset);
        ReadEncodedAsset(g_SkyImage, &asset);
    }
    else if (colors->sun->imageKind == SKY_IMAGE_KIND_1) {
        OpenEncodedAsset(ASSET_SKY_IMAGE_1, &asset);
        ReadEncodedAsset(g_SkyImage, &asset);
    }
    else {
        OpenEncodedAsset(ASSET_SKY_IMAGE_2_FIRST_HALF, &asset);
        ReadEncodedAsset(g_SkyImage, &asset);
        OpenEncodedAsset(ASSET_SKY_IMAGE_2_SECOND_HALF, &asset);
        ReadEncodedAsset(g_SkyImage + SKY_IMAGE_HALF_OFFSET, &asset);
    }

    OpenEncodedAsset(f_CourseSkyImageAssets[GetCourseRecordIndex()], &asset);
    ReadEncodedAsset(g_CourseSkyImage, &asset);
    for (i = 0; i != LARGE_PALETTE_COLOR_COUNT; i++) {
        color = CourseSkyColor(i);
        red = (color >> 11) & SKY_IMAGE_CHANNEL_MAX;
        green = (color >> 6) & SKY_IMAGE_CHANNEL_MAX;
        blue = (color >> 1) & SKY_IMAGE_CHANNEL_MAX;
        alpha = color & 1;
        if (!alpha) {
            red = ((colors->horizonColor[RED_CHANNEL] * SKY_IMAGE_HORIZON_PARTS) +
                      colors->topColor[RED_CHANNEL]) /
                SKY_IMAGE_DIVISOR;
            green = ((colors->horizonColor[GREEN_CHANNEL] * SKY_IMAGE_HORIZON_PARTS) +
                        colors->topColor[GREEN_CHANNEL]) /
                SKY_IMAGE_DIVISOR;
            blue = ((colors->horizonColor[BLUE_CHANNEL] * SKY_IMAGE_HORIZON_PARTS) +
                       colors->topColor[BLUE_CHANNEL]) /
                SKY_IMAGE_DIVISOR;
        }
        else {
            blended[0] = from->backgroundColor[RED_CHANNEL] +
                ((to->backgroundColor[RED_CHANNEL] - from->backgroundColor[RED_CHANNEL]) *
                    f_SkyBlendStep / SKY_BLEND_STEP_COUNT);
            blended[1] = from->backgroundColor[GREEN_CHANNEL] +
                ((to->backgroundColor[GREEN_CHANNEL] - from->backgroundColor[GREEN_CHANNEL]) *
                    f_SkyBlendStep / SKY_BLEND_STEP_COUNT);
            blended[2] = from->backgroundColor[BLUE_CHANNEL] +
                ((to->backgroundColor[BLUE_CHANNEL] - from->backgroundColor[BLUE_CHANNEL]) *
                    f_SkyBlendStep / SKY_BLEND_STEP_COUNT);
            red = ((red * (PERCENT_PER_WHOLE - colors->backgroundColor[SKY_BACKGROUND_ALPHA])) +
                      (blended[0] * colors->backgroundColor[SKY_BACKGROUND_ALPHA])) /
                PERCENT_PER_WHOLE;
            green = ((green * (PERCENT_PER_WHOLE - colors->backgroundColor[SKY_BACKGROUND_ALPHA])) +
                        (blended[1] * colors->backgroundColor[SKY_BACKGROUND_ALPHA])) /
                PERCENT_PER_WHOLE;
            blue = ((blue * (PERCENT_PER_WHOLE - colors->backgroundColor[SKY_BACKGROUND_ALPHA])) +
                       (blended[2] * colors->backgroundColor[SKY_BACKGROUND_ALPHA])) /
                PERCENT_PER_WHOLE;
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

        SetCourseSkyColor(
            i,
            (red << RGBA16_RED_SHIFT) | (green << RGBA16_GREEN_SHIFT) |
                (blue << RGBA16_BLUE_SHIFT) | alpha
        );
    }

    DeriveSceneColors();
}

// The sky editor's table, in debug text cells
#define SKY_EDITOR_CONTROLLER 1
#define SKY_EDITOR_TEXT_COLUMN 13
#define SKY_EDITOR_TEXT_ROW 11
#define SKY_EDITOR_COLUMN_WIDTH 4
#define SKY_EDITOR_ERASE_COLUMN 14
#define SKY_EDITOR_ERASE_ROW 5

// Rows past blue: the background's alpha, and the fog's near and far
#define SKY_EDITOR_ALPHA_ROW SKY_BACKGROUND_ALPHA
#define SKY_EDITOR_FAR_ROW 4
#define SKY_EDITOR_LAST_COLOR_ROW 2

#define SKY_EDITOR_COLOR_STEP 4
#define SKY_EDITOR_FOG_STEP 4
#define BYTE_VALUE_COUNT 256
#define FOG_DEPTH_MAX 1200
static SkyColors *f_EditedSkyColors;

// While GAME_FLAG_SKY_EDITOR is set, lets controller 2 edit the current sky
// colors and g_DebugFog, shows them in a table, and returns TRUE. The D-pad
// or C buttons move the cursor, and holding A or B raises or lowers the value
// under it.
Boolean UpdateSkyEditor(void) {
    EncodedAsset asset;
    Boolean isImageChanged;
    s32 index; // Counts the vertex groups, then the palette colors
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

    PrintDebugText(
        " ",
        (f_SkyEditorColumn * SKY_EDITOR_COLUMN_WIDTH) + SKY_EDITOR_ERASE_COLUMN,
        f_SkyEditorRow + SKY_EDITOR_ERASE_ROW
    );
    f_EditedSkyColors =
        &g_SkyColors[GetCourseRecordIndex()]
                    [(g_HoleSetup.conditions.isRaining * SKY_TIME_COUNT) + f_SkyColorTimeIndex];
    if ((g_ControllerStates[SKY_EDITOR_CONTROLLER].pressedButtons & (L_JPAD | L_CBUTTONS)) &&
        f_SkyEditorColumn > 0) {
        f_SkyEditorColumn--;
    }

    if ((g_ControllerStates[SKY_EDITOR_CONTROLLER].pressedButtons & (R_JPAD | R_CBUTTONS)) &&
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
    }
    else if (f_SkyEditorColumn == SKY_EDITOR_FOG) {
        if (f_SkyEditorRow > SKY_EDITOR_FAR_ROW) {
            f_SkyEditorRow = SKY_EDITOR_FAR_ROW;
        }
    }
    else if (f_SkyEditorRow > SKY_EDITOR_LAST_COLOR_ROW) {
        f_SkyEditorRow = SKY_EDITOR_LAST_COLOR_ROW;
    }

    // The rows of the fog column run past g_DebugFog.color into near and far
    if (g_ControllerStates[SKY_EDITOR_CONTROLLER].buttons & A_BUTTON) {
        switch (f_SkyEditorColumn) {
            case SKY_EDITOR_TOP:
                f_EditedSkyColors->topColor[f_SkyEditorRow] += SKY_EDITOR_COLOR_STEP;
                f_EditedSkyColors->topColor[f_SkyEditorRow] %= BYTE_VALUE_COUNT;
                break;
            case SKY_EDITOR_HORIZON:
                f_EditedSkyColors->horizonColor[f_SkyEditorRow] += SKY_EDITOR_COLOR_STEP;
                f_EditedSkyColors->horizonColor[f_SkyEditorRow] %= BYTE_VALUE_COUNT;
                break;
            case SKY_EDITOR_NOISE:
                f_EditedSkyColors->noiseColor[f_SkyEditorRow] += SKY_EDITOR_COLOR_STEP;
                f_EditedSkyColors->noiseColor[f_SkyEditorRow] %= BYTE_VALUE_COUNT;
                break;
            case SKY_EDITOR_BACKGROUND:
                f_EditedSkyColors->backgroundColor[f_SkyEditorRow]++;
                isImageChanged = TRUE;
                if (f_SkyEditorRow == SKY_EDITOR_ALPHA_ROW) {
                    // Wraps the percent past either end
                    if (f_EditedSkyColors->backgroundColor[f_SkyEditorRow] ==
                        PERCENT_PER_WHOLE + 1) {
                        f_EditedSkyColors->backgroundColor[f_SkyEditorRow] = 0;
                    }

                    if (f_EditedSkyColors->backgroundColor[f_SkyEditorRow] >
                        PERCENT_PER_WHOLE + 1) {
                        f_EditedSkyColors->backgroundColor[f_SkyEditorRow] = PERCENT_PER_WHOLE;
                    }
                }
                else {
                    f_EditedSkyColors->backgroundColor[f_SkyEditorRow] &= SKY_IMAGE_CHANNEL_MAX;
                }

                break;
            case SKY_EDITOR_FOG:
                g_DebugFog.color[f_SkyEditorRow] += SKY_EDITOR_FOG_STEP;
                break;
        }
    }
    else if (g_ControllerStates[SKY_EDITOR_CONTROLLER].buttons & B_BUTTON) {
        switch (f_SkyEditorColumn) {
            case SKY_EDITOR_TOP:
                f_EditedSkyColors->topColor[f_SkyEditorRow] -= SKY_EDITOR_COLOR_STEP;
                f_EditedSkyColors->topColor[f_SkyEditorRow] %= BYTE_VALUE_COUNT;
                break;
            case SKY_EDITOR_HORIZON:
                f_EditedSkyColors->horizonColor[f_SkyEditorRow] -= SKY_EDITOR_COLOR_STEP;
                f_EditedSkyColors->horizonColor[f_SkyEditorRow] %= BYTE_VALUE_COUNT;
                break;
            case SKY_EDITOR_NOISE:
                f_EditedSkyColors->noiseColor[f_SkyEditorRow] -= SKY_EDITOR_COLOR_STEP;
                f_EditedSkyColors->noiseColor[f_SkyEditorRow] %= BYTE_VALUE_COUNT;
                break;
            case SKY_EDITOR_BACKGROUND:
                f_EditedSkyColors->backgroundColor[f_SkyEditorRow]--;
                isImageChanged = TRUE;
                if (f_SkyEditorRow == SKY_EDITOR_ALPHA_ROW) {
                    if (f_EditedSkyColors->backgroundColor[f_SkyEditorRow] ==
                        PERCENT_PER_WHOLE + 1) {
                        f_EditedSkyColors->backgroundColor[f_SkyEditorRow] = 0;
                    }

                    if (f_EditedSkyColors->backgroundColor[f_SkyEditorRow] >
                        PERCENT_PER_WHOLE + 1) {
                        f_EditedSkyColors->backgroundColor[f_SkyEditorRow] = PERCENT_PER_WHOLE;
                    }
                }
                else {
                    f_EditedSkyColors->backgroundColor[f_SkyEditorRow] &= SKY_IMAGE_CHANNEL_MAX;
                }

                break;
            case SKY_EDITOR_FOG:
                g_DebugFog.color[f_SkyEditorRow] -= SKY_EDITOR_FOG_STEP;
                break;
        }
    }

    if (g_DebugFog.color[RED_CHANNEL] > COLOR_CHANNEL_MAX) {
        g_DebugFog.color[RED_CHANNEL] = COLOR_CHANNEL_MAX;
    }

    if (g_DebugFog.color[GREEN_CHANNEL] > COLOR_CHANNEL_MAX) {
        g_DebugFog.color[GREEN_CHANNEL] = COLOR_CHANNEL_MAX;
    }

    if (g_DebugFog.color[BLUE_CHANNEL] > COLOR_CHANNEL_MAX) {
        g_DebugFog.color[BLUE_CHANNEL] = COLOR_CHANNEL_MAX;
    }

    if (g_DebugFog.near > FOG_DEPTH_MAX) {
        g_DebugFog.near = FOG_DEPTH_MAX;
    }

    if (g_DebugFog.far > FOG_DEPTH_MAX) {
        g_DebugFog.far = FOG_DEPTH_MAX;
    }

    if (g_DebugFog.color[RED_CHANNEL] < 0) {
        g_DebugFog.color[RED_CHANNEL] = 0;
    }

    if (g_DebugFog.color[GREEN_CHANNEL] < 0) {
        g_DebugFog.color[GREEN_CHANNEL] = 0;
    }

    if (g_DebugFog.color[BLUE_CHANNEL] < 0) {
        g_DebugFog.color[BLUE_CHANNEL] = 0;
    }

    if (g_DebugFog.near < 0) {
        g_DebugFog.near = 0;
    }

    if (g_DebugFog.far < 0) {
        g_DebugFog.far = 0;
    }

    for (index = 0; index != SKY_VIEW_COUNT * MATRIX_BUFFER_COUNT; index++) {
        for (channel = 0; channel != RGB_CHANNEL_COUNT; channel++) {
            f_SkyVertices[index][SKY_TOP_LEFT].v.cn[channel] = f_EditedSkyColors->topColor[channel];
            f_SkyVertices[index][SKY_HORIZON_LEFT].v.cn[channel] =
                f_EditedSkyColors->horizonColor[channel];
            f_SkyVertices[index][SKY_HORIZON_RIGHT].v.cn[channel] =
                f_EditedSkyColors->horizonColor[channel];
            f_SkyVertices[index][SKY_TOP_RIGHT].v.cn[channel] =
                f_EditedSkyColors->topColor[channel];
            f_SkyVertices[index][SKY_TOP_CENTER].v.cn[channel] =
                f_EditedSkyColors->topColor[channel];
            innerMix = (u16)(f_EditedSkyColors->topColor[channel] +
                           f_EditedSkyColors->horizonColor[channel]) *
                0.5f;
            outerMix = (f_EditedSkyColors->topColor[channel] + SKY_HORIZON_LEVEL) * 0.5f;
            innerMix = (innerMix + SKY_HORIZON_LEVEL) * 0.5f;
            f_SkyVertices[index][MIRRORED_SKY_FAN + SKY_TOP_LEFT].v.cn[channel] = outerMix;
            f_SkyVertices[index][MIRRORED_SKY_FAN + SKY_HORIZON_LEFT].v.cn[channel] = innerMix;
            f_SkyVertices[index][MIRRORED_SKY_FAN + SKY_HORIZON_RIGHT].v.cn[channel] = innerMix;
            f_SkyVertices[index][MIRRORED_SKY_FAN + SKY_TOP_RIGHT].v.cn[channel] = outerMix;
            f_SkyVertices[index][MIRRORED_SKY_FAN + SKY_TOP_CENTER].v.cn[channel] = outerMix;
        }
    }

    if (isImageChanged) {
        OpenEncodedAsset(f_CourseSkyImageAssets[GetCourseRecordIndex()], &asset);
        ReadEncodedAsset(g_CourseSkyImage, &asset);
        for (index = 0; index != LARGE_PALETTE_COLOR_COUNT; index++) {
            color = CourseSkyColor(index);
            red = (color >> 11) & SKY_IMAGE_CHANNEL_MAX;
            green = (color >> 6) & SKY_IMAGE_CHANNEL_MAX;
            blue = (color >> 1) & SKY_IMAGE_CHANNEL_MAX;
            alpha = color & 1;
            if (!alpha) {
                red = ((f_EditedSkyColors->horizonColor[RED_CHANNEL] * SKY_IMAGE_HORIZON_PARTS) +
                          f_EditedSkyColors->topColor[RED_CHANNEL]) /
                    SKY_IMAGE_DIVISOR;
                green =
                    ((f_EditedSkyColors->horizonColor[GREEN_CHANNEL] * SKY_IMAGE_HORIZON_PARTS) +
                        f_EditedSkyColors->topColor[GREEN_CHANNEL]) /
                    SKY_IMAGE_DIVISOR;
                blue = ((f_EditedSkyColors->horizonColor[BLUE_CHANNEL] * SKY_IMAGE_HORIZON_PARTS) +
                           f_EditedSkyColors->topColor[BLUE_CHANNEL]) /
                    SKY_IMAGE_DIVISOR;
            }
            else {
                red = ((red *
                           (PERCENT_PER_WHOLE -
                               f_EditedSkyColors->backgroundColor[SKY_BACKGROUND_ALPHA])) +
                          (f_EditedSkyColors->backgroundColor[RED_CHANNEL] *
                              f_EditedSkyColors->backgroundColor[SKY_BACKGROUND_ALPHA])) /
                    PERCENT_PER_WHOLE;
                green = ((green *
                             (PERCENT_PER_WHOLE -
                                 f_EditedSkyColors->backgroundColor[SKY_BACKGROUND_ALPHA])) +
                            (f_EditedSkyColors->backgroundColor[GREEN_CHANNEL] *
                                f_EditedSkyColors->backgroundColor[SKY_BACKGROUND_ALPHA])) /
                    PERCENT_PER_WHOLE;
                blue = ((blue *
                            (PERCENT_PER_WHOLE -
                                f_EditedSkyColors->backgroundColor[SKY_BACKGROUND_ALPHA])) +
                           (f_EditedSkyColors->backgroundColor[BLUE_CHANNEL] *
                               f_EditedSkyColors->backgroundColor[SKY_BACKGROUND_ALPHA])) /
                    PERCENT_PER_WHOLE;
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

            SetCourseSkyColor(
                index,
                (red << RGBA16_RED_SHIFT) | (green << RGBA16_GREEN_SHIFT) |
                    (blue << RGBA16_BLUE_SHIFT) | alpha
            );
        }
    }

    PrintDebugText("   T   B   C   BG  FOG", SKY_EDITOR_TEXT_COLUMN, SKY_EDITOR_TEXT_ROW);
    sprintf(
        g_PrintBuffer,
        "R: %3d %3d %3d %3d %4d  ",
        f_EditedSkyColors->topColor[RED_CHANNEL],
        f_EditedSkyColors->horizonColor[RED_CHANNEL],
        f_EditedSkyColors->noiseColor[RED_CHANNEL],
        f_EditedSkyColors->backgroundColor[RED_CHANNEL],
        (int)g_DebugFog.color[RED_CHANNEL]
    );
    PrintDebugText(g_PrintBuffer, SKY_EDITOR_TEXT_COLUMN, SKY_EDITOR_TEXT_ROW + 1);
    sprintf(
        g_PrintBuffer,
        "G: %3d %3d %3d %3d %4d  ",
        f_EditedSkyColors->topColor[GREEN_CHANNEL],
        f_EditedSkyColors->horizonColor[GREEN_CHANNEL],
        f_EditedSkyColors->noiseColor[GREEN_CHANNEL],
        f_EditedSkyColors->backgroundColor[GREEN_CHANNEL],
        (int)g_DebugFog.color[GREEN_CHANNEL]
    );
    PrintDebugText(g_PrintBuffer, SKY_EDITOR_TEXT_COLUMN, SKY_EDITOR_TEXT_ROW + 2);
    sprintf(
        g_PrintBuffer,
        "B: %3d %3d %3d %3d %4d  ",
        f_EditedSkyColors->topColor[BLUE_CHANNEL],
        f_EditedSkyColors->horizonColor[BLUE_CHANNEL],
        f_EditedSkyColors->noiseColor[BLUE_CHANNEL],
        f_EditedSkyColors->backgroundColor[BLUE_CHANNEL],
        (int)g_DebugFog.color[BLUE_CHANNEL]
    );
    PrintDebugText(g_PrintBuffer, SKY_EDITOR_TEXT_COLUMN, SKY_EDITOR_TEXT_ROW + 3);
    sprintf(
        g_PrintBuffer,
        "A:             %3d %4d  ",
        f_EditedSkyColors->backgroundColor[SKY_BACKGROUND_ALPHA],
        (int)g_DebugFog.near
    );
    PrintDebugText(g_PrintBuffer, SKY_EDITOR_TEXT_COLUMN, SKY_EDITOR_TEXT_ROW + 4);
    sprintf(
        g_PrintBuffer,
        "CC:%d               %4d  ",
        (int)f_EditedSkyColors->addsNoise,
        (int)g_DebugFog.far
    );
    PrintDebugText(g_PrintBuffer, SKY_EDITOR_TEXT_COLUMN, SKY_EDITOR_TEXT_ROW + 5);
    PrintDebugText(
        ">",
        (f_SkyEditorColumn * SKY_EDITOR_COLUMN_WIDTH) + SKY_EDITOR_TEXT_COLUMN + 2,
        f_SkyEditorRow + SKY_EDITOR_TEXT_ROW + 1
    );
    SetFramebufferCount(TRIPLE_BUFFERING);
    SetScreenEffect(SCREEN_EFFECT_NONE, 0);
    return TRUE;
}

// UpdateSky's limits and scales
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
// Scales how far the clipped heights push the texture
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

// Points the sky of a view at its camera, and scrolls the sky's texture with
// the wind. The first view also blends the sky's colors, unless the sky
// editor is open.
void UpdateSky(CameraView *view, s32 viewIndex) {
    Radians *pitches;
    f32 pitchScale; // Narrower fields of view move the horizon further for each radian of pitch
    Vec3 direction;
    s32 unused[8] UNUSED;
    Vtx *vertices;
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

    direction[AXIS_X] = view->lookAt[AXIS_X] - view->eye[AXIS_X];
    direction[AXIS_Y] = view->lookAt[AXIS_Y] - view->eye[AXIS_Y];
    direction[AXIS_Z] = view->lookAt[AXIS_Z] - view->eye[AXIS_Z];
    vertices = f_SkyVertices[g_MatrixBufferIndex + (viewIndex * MATRIX_BUFFER_COUNT)];
    g_SkyState.yaws[viewIndex] = atan2(direction[AXIS_X], direction[AXIS_Z]);
    g_SkyState.pitches[viewIndex] = -atan2(
        direction[AXIS_Y],
        sqrtf((direction[AXIS_X] * direction[AXIS_X]) + (direction[AXIS_Z] * direction[AXIS_Z]))
    );
    if (g_SkyState.pitches[viewIndex] < -SKY_PITCH_LIMIT) {
        g_SkyState.pitches[viewIndex] = -SKY_PITCH_LIMIT;
    }

    if (g_SkyState.pitches[viewIndex] > SKY_PITCH_LIMIT) {
        g_SkyState.pitches[viewIndex] = SKY_PITCH_LIMIT;
    }

    slope = (view->lookAt[AXIS_Y] - view->eye[AXIS_Y]) *
        (SKY_SLOPE_DISTANCE /
            sqrtf(
                (direction[AXIS_X] * direction[AXIS_X]) + (direction[AXIS_Z] * direction[AXIS_Z])
            ));
    pitchScale = SKY_REFERENCE_FIELD_OF_VIEW / view->fieldOfView * SCREEN_HEIGHT;
    g_SkyState.horizonLevels[viewIndex] =
        (-g_SkyState.pitches[viewIndex] * pitchScale) + (view->eye[AXIS_Y] * SKY_EYE_HEIGHT_SCALE);
    if (g_SkyState.horizonLevels[viewIndex] < SKY_HORIZON_LEVEL_MIN) {
        g_SkyState.horizonLevels[viewIndex] = SKY_HORIZON_LEVEL_MIN;
    }

    g_SkyState.screenHeights[viewIndex] = SCREEN_HEIGHT;
    g_SkyState.lookAtLevels[viewIndex] = g_SkyState.horizonLevels[viewIndex] -
        (((view->lookAt[AXIS_Y] * slope) - view->eye[AXIS_Y]) * SKY_EYE_HEIGHT_SCALE);

    if (IsGameFlagSet(GAME_FLAG_CYCLE_SKY_CONDITIONS)) {
        if (g_ControllerStates[0].pressedButtons & U_CBUTTONS) {
            g_HoleSetup.conditions.isRaining =
                (u32)(g_HoleSetup.conditions.isRaining + 1) % SKY_DEBUG_CYCLE_LENGTH;
            InitSkyColors();
        }

        if (g_ControllerStates[0].pressedButtons & D_CBUTTONS) {
            g_HoleSetup.conditions.sceneColorIndex =
                (g_HoleSetup.conditions.sceneColorIndex + 1) % SKY_DEBUG_CYCLE_LENGTH;
            InitSkyColors();
        }
    }

    clippedHeight = 0;
    scroll = -(f32)(g_HoleSetup.conditions.windSpeed / WIND_SPEED_UNIT) * SKY_SCROLL_PER_WIND_SPEED;
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
    }
    else if (g_SkyState.scrollY < -SKY_SCROLL_Y_WRAP) {
        g_SkyState.scrollY += SKY_SCROLL_Y_PERIOD;
    }

    // Vertices 1, 2, 6 and 7 follow the horizon level; 0, 3 and 4 follow the
    // pitch alone
    if (g_SkyState.pitches[viewIndex] < 0.0f) {
        vertices[SKY_TOP_LEFT].v.ob[AXIS_Y] =
            (SCREEN_HEIGHT / 2) - (g_SkyState.pitches[viewIndex] * (SCREEN_HEIGHT / 2));
        vertices[SKY_TOP_RIGHT].v.ob[AXIS_Y] =
            (SCREEN_HEIGHT / 2) - (g_SkyState.pitches[viewIndex] * (SCREEN_HEIGHT / 2));
        vertices[SKY_TOP_CENTER].v.ob[AXIS_Y] =
            (SCREEN_HEIGHT / 2) - (g_SkyState.pitches[viewIndex] * (SCREEN_HEIGHT / 2));
    }
    else {
        vertices[SKY_TOP_LEFT].v.ob[AXIS_Y] = SCREEN_HEIGHT / 2;
        vertices[SKY_TOP_RIGHT].v.ob[AXIS_Y] = SCREEN_HEIGHT / 2;
        vertices[SKY_TOP_CENTER].v.ob[AXIS_Y] = SCREEN_HEIGHT / 2;
    }

    vertices[SKY_HORIZON_LEFT].v.ob[AXIS_Y] =
        (g_SkyState.horizonLevels[viewIndex] + SKY_HORIZON_BIAS) * SKY_HORIZON_TO_HEIGHT;
    vertices[SKY_HORIZON_RIGHT].v.ob[AXIS_Y] =
        (g_SkyState.horizonLevels[viewIndex] + SKY_HORIZON_BIAS) * SKY_HORIZON_TO_HEIGHT;
    vertices[SKY_HORIZON_LEFT].v.ob[AXIS_Z] =
        SKY_TOP_DEPTH - (g_SkyState.pitches[viewIndex] * SKY_PITCH_TO_DEPTH);
    vertices[SKY_HORIZON_RIGHT].v.ob[AXIS_Z] =
        SKY_TOP_DEPTH - (g_SkyState.pitches[viewIndex] * SKY_PITCH_TO_DEPTH);
    vertices[MIRRORED_SKY_FAN + SKY_HORIZON_LEFT].v.ob[AXIS_Z] =
        SKY_TOP_DEPTH - (-g_SkyState.pitches[viewIndex] * SKY_PITCH_TO_DEPTH);
    vertices[MIRRORED_SKY_FAN + SKY_HORIZON_RIGHT].v.ob[AXIS_Z] =
        SKY_TOP_DEPTH - (-g_SkyState.pitches[viewIndex] * SKY_PITCH_TO_DEPTH);
    if (vertices[SKY_HORIZON_LEFT].v.ob[AXIS_Y] < -SKY_HEIGHT_LIMIT) {
        clippedHeight = (vertices[SKY_HORIZON_LEFT].v.ob[AXIS_Y] + SKY_HEIGHT_LIMIT) /
            (1 << SKY_CLIPPED_HEIGHT_SHIFT);
        vertices[SKY_HORIZON_LEFT].v.ob[AXIS_Y] = -SKY_HEIGHT_LIMIT;
    }

    if (vertices[SKY_HORIZON_RIGHT].v.ob[AXIS_Y] < -SKY_HEIGHT_LIMIT) {
        vertices[SKY_HORIZON_RIGHT].v.ob[AXIS_Y] = -SKY_HEIGHT_LIMIT;
    }

    if (vertices[SKY_HORIZON_LEFT].v.ob[AXIS_Y] > SKY_HEIGHT_LIMIT) {
        vertices[SKY_HORIZON_LEFT].v.ob[AXIS_Y] = SKY_HEIGHT_LIMIT;
    }

    if (vertices[SKY_HORIZON_RIGHT].v.ob[AXIS_Y] > SKY_HEIGHT_LIMIT) {
        vertices[SKY_HORIZON_RIGHT].v.ob[AXIS_Y] = SKY_HEIGHT_LIMIT;
    }

    vertices[MIRRORED_SKY_FAN + SKY_HORIZON_LEFT].v.ob[AXIS_Y] =
        vertices[SKY_HORIZON_LEFT].v.ob[AXIS_Y];
    vertices[MIRRORED_SKY_FAN + SKY_HORIZON_RIGHT].v.ob[AXIS_Y] =
        vertices[SKY_HORIZON_RIGHT].v.ob[AXIS_Y];

    // Each row of texture coordinates lies at a distance along the viewed wind
    // direction
    sine = sinf(g_SkyState.viewedWindAngles[viewIndex]);
    cosine = cosf(g_SkyState.viewedWindAngles[viewIndex]);
    distance = clippedHeight + SKY_TEXTURE_FAR_DISTANCE;
    coordinate = (sine * distance) - (cosine * SKY_TEXTURE_HALF_WIDTH);
    vertices[SKY_HORIZON_LEFT].v.tc[TEXTURE_AXIS_S] =
        ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
    coordinate = (cosine * distance) + (sine * SKY_TEXTURE_HALF_WIDTH);
    vertices[SKY_HORIZON_LEFT].v.tc[TEXTURE_AXIS_T] =
        ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
    coordinate = (sine * distance) + (cosine * SKY_TEXTURE_HALF_WIDTH);
    vertices[SKY_HORIZON_RIGHT].v.tc[TEXTURE_AXIS_S] =
        ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
    coordinate = (cosine * distance) - (sine * SKY_TEXTURE_HALF_WIDTH);
    pitches = g_SkyState.pitches;
    vertices[SKY_HORIZON_RIGHT].v.tc[TEXTURE_AXIS_T] =
        ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
    horizonHalfWidth = SCREEN_WIDTH;
    distance = SKY_TEXTURE_NEAR_DISTANCE - (pitches[viewIndex] * horizonHalfWidth * (2 / M_PI)) +
        clippedHeight;
    coordinate = (sine * distance) - (cosine * SKY_TEXTURE_HALF_WIDTH);
    vertices[SKY_TOP_LEFT].v.tc[TEXTURE_AXIS_S] =
        ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
    coordinate = (cosine * distance) + (sine * SKY_TEXTURE_HALF_WIDTH);
    vertices[SKY_TOP_LEFT].v.tc[TEXTURE_AXIS_T] =
        ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
    coordinate = (sine * distance) + (cosine * SKY_TEXTURE_HALF_WIDTH);
    vertices[SKY_TOP_RIGHT].v.tc[TEXTURE_AXIS_S] =
        ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
    coordinate = (cosine * distance) - (sine * SKY_TEXTURE_HALF_WIDTH);
    vertices[SKY_TOP_RIGHT].v.tc[TEXTURE_AXIS_T] =
        ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
    vertices[SKY_TOP_CENTER].v.tc[TEXTURE_AXIS_S] =
        ((s32)(sine * distance) * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
    vertices[SKY_TOP_CENTER].v.tc[TEXTURE_AXIS_T] =
        ((s32)(cosine * distance) * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
    distance =
        (SKY_TEXTURE_HORIZON_DISTANCE - (-pitches[viewIndex] * horizonHalfWidth * (2 / M_PI))) *
        horizonHalfWidth / SCREEN_WIDTH;
    coordinate = (sine * distance) - (cosine * horizonHalfWidth);
    vertices[MIRRORED_SKY_FAN + SKY_HORIZON_LEFT].v.tc[TEXTURE_AXIS_S] =
        ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
    coordinate = (cosine * distance) + (sine * horizonHalfWidth);
    vertices[MIRRORED_SKY_FAN + SKY_HORIZON_LEFT].v.tc[TEXTURE_AXIS_T] =
        ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
    coordinate = (sine * distance) + (cosine * horizonHalfWidth);
    vertices[MIRRORED_SKY_FAN + SKY_HORIZON_RIGHT].v.tc[TEXTURE_AXIS_S] =
        ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
    coordinate = (cosine * distance) - (sine * horizonHalfWidth);
    vertices[MIRRORED_SKY_FAN + SKY_HORIZON_RIGHT].v.tc[TEXTURE_AXIS_T] =
        ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
    distance = (SKY_TEXTURE_NEAR_DISTANCE - (-pitches[viewIndex] * horizonHalfWidth * (2 / M_PI))) *
        horizonHalfWidth / SCREEN_WIDTH;
    coordinate = (sine * distance) - (cosine * horizonHalfWidth);
    vertices[MIRRORED_SKY_FAN + SKY_TOP_LEFT].v.tc[TEXTURE_AXIS_S] =
        ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
    coordinate = (cosine * distance) + (sine * horizonHalfWidth);
    vertices[MIRRORED_SKY_FAN + SKY_TOP_LEFT].v.tc[TEXTURE_AXIS_T] =
        ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
    coordinate = (sine * distance) + (cosine * horizonHalfWidth);
    vertices[MIRRORED_SKY_FAN + SKY_TOP_RIGHT].v.tc[TEXTURE_AXIS_S] =
        ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
    coordinate = (cosine * distance) - (sine * horizonHalfWidth);
    vertices[MIRRORED_SKY_FAN + SKY_TOP_RIGHT].v.tc[TEXTURE_AXIS_T] =
        ((s32)coordinate * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
    vertices[MIRRORED_SKY_FAN + SKY_TOP_CENTER].v.tc[TEXTURE_AXIS_S] =
        ((s32)(sine * distance) * SKY_TEXTURE_SCALE) + g_SkyState.scrollX;
    vertices[MIRRORED_SKY_FAN + SKY_TOP_CENTER].v.tc[TEXTURE_AXIS_T] =
        ((s32)(cosine * distance) * SKY_TEXTURE_SCALE) + g_SkyState.scrollY;
}

// Sets whether the course's drawing gives its primitives a fixed depth
void UsePrimitiveDepth(Boolean isPrimitiveDepthSet) {
    f_IsPrimitiveDepthSet = isPrimitiveDepthSet;
}

// The sky dome is drawn this far in front of the camera
#define SKY_DOME_DISTANCE (-300.0f)
#define SKY_NOISE_IMAGE_SIZE 64
#define SKY_NOISE_MASK 6
#define SKY_DEPTH 0x7FFB
// The sun moves SUN_TURN_WIDTH pixels across the screen as the camera turns
// once
#define SUN_TURN_WIDTH 1536.0f
#define SUN_YAW_OFFSET (M_PI / 4)
#define SUN_HORIZON_Y 112.0f
#define SUN_CULL_MARGIN 64.0f
#define SUN_DEPTH 0x7FF7
#define SUN_IMAGE_SIZE 32
#define SUN_GLOW_IMAGE_SIZE 64
#define SUN_GLOW_MASK 6
#define SUN_MASK_TMEM 0x100
#define LIGHT_YAW_TO_RADIANS ((f32)(FULL_TURN_RADIANS / 0x10000))
static Mtx f_SkyModelMatrix;

static u8 f_UnusedBufferAfterSkyMatrix[0x88] UNUSED;

static Mtx f_TileMatrices[MATRIX_BUFFER_COUNT][FRAME_TILE_COUNT];

#ifdef AVOID_UB
// StepNoiseTexture rewrites the noise texture every frame, while the task
// drawing the previous frame, queued before, may not have loaded it yet; the
// RDP then sees some of the new rows, depending on timing. Each matrix
// buffer's frame loads a copy taken while it is drawn instead.
static u8 f_DrawnNoiseTexels[MATRIX_BUFFER_COUNT]
                            [SKY_NOISE_IMAGE_SIZE * SKY_NOISE_IMAGE_SIZE] RCP_ALIGNED;

static u8 *NoiseTexelsToDraw(void) {
    u8 *texels = f_DrawnNoiseTexels[g_MatrixBufferIndex];

    memcpy(texels, g_NoiseTexels, sizeof(f_DrawnNoiseTexels[0]));
    return texels;
}
#else
#    define NoiseTexelsToDraw() g_NoiseTexels
#endif

// Draws the sky dome around the camera and, when the scene has one, the sun.
// The sun is a textured rectangle placed from the camera's yaw against the
// light's, and the dome's height above the horizon.
void DrawSky(Gfx **displayListHead, f32 eyeY UNUSED, s32 unusedSetting UNUSED, s32 cameraIndex) {
    Gfx *gfx = *displayListHead;
    SkyColors *colors;
    Mat4 matrix;
    s32 unused[16] UNUSED;
    Vec3 rotation;
    Vec3 translation;
    Radians angle;

    UpdateSky(&g_Cameras[cameraIndex].view, cameraIndex);

    // Unlike the other lookups of g_SkyColors, this steps by three times, not
    // SKY_TIME_COUNT, in the rain
    colors = &g_SkyColors[GetCourseRecordIndex()]
                         [(g_HoleSetup.conditions.isRaining * 3) + f_SkyColorTimeIndex];
    translation[AXIS_X] = 0.0f;
    translation[AXIS_Y] = 0.0f;
    rotation[AXIS_X] = 0.0f;
    rotation[AXIS_Y] = 0.0f;
    rotation[AXIS_Z] = 0.0f;
    translation[AXIS_Z] = SKY_DOME_DISTANCE;
    BuildRotationMatrix(matrix, rotation, translation);
    ConvertMatrixToFixed(matrix, &f_SkyModelMatrix);
    gDPSetPrimColor(gfx++, 0, 0, 127, 127, 127, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    if (f_IsPrimitiveDepthSet) {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
        gDPPipeSync(gfx++);
        gDPSetDepthSource(gfx++, G_ZS_PRIM);
        gDPSetPrimDepth(gfx++, SKY_DEPTH, 0);
    }
    else {
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
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(gfx++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
    if (colors->addsNoise == 0) {
        gDPSetCombineLERP(
            gfx++,
            ENVIRONMENT,
            SHADE,
            TEXEL0,
            SHADE,
            0,
            0,
            0,
            TEXEL0,
            ENVIRONMENT,
            SHADE,
            TEXEL0,
            SHADE,
            0,
            0,
            0,
            TEXEL0
        );
    }
    else {
        gDPSetCombineLERP(
            gfx++,
            TEXEL0,
            0,
            ENVIRONMENT,
            SHADE,
            0,
            0,
            0,
            TEXEL0,
            TEXEL0,
            0,
            ENVIRONMENT,
            SHADE,
            0,
            0,
            0,
            TEXEL0
        );
    }

    gDPSetEnvColor(
        gfx++,
        colors->noiseColor[RED_CHANNEL],
        colors->noiseColor[GREEN_CHANNEL],
        colors->noiseColor[BLUE_CHANNEL],
        OPAQUE_ALPHA
    );
    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(NoiseTexelsToDraw()),
        G_IM_FMT_I,
        G_IM_SIZ_8b,
        SKY_NOISE_IMAGE_SIZE,
        SKY_NOISE_IMAGE_SIZE,
        0,
        G_TX_WRAP,
        G_TX_WRAP,
        SKY_NOISE_MASK,
        SKY_NOISE_MASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gSPMatrix(
        gfx++,
        PhysicalAddress(&g_ScreenEffectProjection),
        G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_SkyModelMatrix),
        G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH
    );
    gSPPerspNormalize(gfx++, g_ScreenEffectPerspectiveNormalization);
    gSPVertex(
        gfx++,
        f_SkyVertices[g_MatrixBufferIndex + (cameraIndex * MATRIX_BUFFER_COUNT)],
        SKY_VERTEX_COUNT,
        0
    );
    gDPPipeSync(gfx++);
    gSP1Triangle(gfx++, SKY_TOP_LEFT, SKY_HORIZON_LEFT, SKY_TOP_CENTER, 0);
    gSP1Triangle(gfx++, SKY_HORIZON_LEFT, SKY_HORIZON_RIGHT, SKY_TOP_CENTER, 0);
    gSP1Triangle(gfx++, SKY_TOP_CENTER, SKY_HORIZON_RIGHT, SKY_TOP_RIGHT, 0);
    gDPPipeSync(gfx++);
    if (colors->hasLensFlare == TRUE) {
        angle = -g_SkyState.yaws[cameraIndex] - SUN_YAW_OFFSET -
            (g_HoleSetup.conditions.lightYaw * LIGHT_YAW_TO_RADIANS);
        if (angle > FULL_TURN_RADIANS) {
            angle -= FULL_TURN_RADIANS;
        }

        if (angle < 0.0f) {
            angle += FULL_TURN_RADIANS;
        }

        if (angle > FULL_TURN_RADIANS) {
            angle -= FULL_TURN_RADIANS;
        }

        if (angle < 0.0f) {
            angle += FULL_TURN_RADIANS;
        }

        f_SunScreenX[cameraIndex] = angle * SUN_TURN_WIDTH * (1 / FULL_TURN_RADIANS);
        f_SunScreenY[cameraIndex] =
            -g_SkyState.horizonLevels[cameraIndex] - colors->sun->height + SUN_HORIZON_Y;
        if (f_SunScreenX[cameraIndex] > SUN_TURN_WIDTH / 2) {
            f_SunScreenX[cameraIndex] -= SUN_TURN_WIDTH;
        }

        if (IsNearScreen(f_SunScreenX[cameraIndex], f_SunScreenY[cameraIndex], SUN_CULL_MARGIN)) {
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
            }
            else {
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            }

            gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
            if (colors->sun->imageKind == SKY_IMAGE_KIND_TWO_TEXTURES) {
                gDPPipeSync(gfx++);
                gDPSetAlphaDither(gfx++, G_AD_PATTERN);
                gDPPipeSync(gfx++);
                gDPPipeSync(gfx++);
                gDPSetCycleType(gfx++, G_CYC_2CYCLE);
                gDPSetCombineLERP(
                    gfx++,
                    TEXEL1,
                    0,
                    TEXEL0,
                    0,
                    0,
                    0,
                    0,
                    TEXEL1,
                    TEXEL1,
                    0,
                    COMBINED,
                    0,
                    0,
                    0,
                    0,
                    COMBINED
                );
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress(ImageBody(g_SkyImage)),
                    G_IM_FMT_RGBA,
                    G_IM_SIZ_16b,
                    SUN_IMAGE_SIZE,
                    SUN_IMAGE_SIZE,
                    0,
                    G_TX_WRAP,
                    G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                gDPLoadMultiBlock_4b(
                    gfx++,
                    TextureAddress(ImageBody(g_SkyImage + SKY_IMAGE_HALF_OFFSET)),
                    SUN_MASK_TMEM,
                    1,
                    G_IM_FMT_I,
                    SUN_IMAGE_SIZE,
                    SUN_IMAGE_SIZE,
                    0,
                    G_TX_WRAP,
                    G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                gDPPipeSync(gfx++);
                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels((s32)f_SunScreenX[cameraIndex] - colors->sun->radius + 1),
                    QuarterPixels((s32)f_SunScreenY[cameraIndex] - colors->sun->radius + 1),
                    QuarterPixels((s32)f_SunScreenX[cameraIndex] + colors->sun->radius),
                    QuarterPixels((s32)f_SunScreenY[cameraIndex] + colors->sun->radius),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    colors->sun->texelStep,
                    colors->sun->texelStep
                );
                gDPPipeSync(gfx++);
            }
            else {
                gDPPipeSync(gfx++);
                gDPSetAlphaDither(gfx++, G_AD_NOISE);
                gDPSetCombineLERP(
                    gfx++,
                    PRIMITIVE,
                    ENVIRONMENT,
                    TEXEL0,
                    ENVIRONMENT,
                    0,
                    0,
                    0,
                    TEXEL0,
                    PRIMITIVE,
                    ENVIRONMENT,
                    TEXEL0,
                    ENVIRONMENT,
                    0,
                    0,
                    0,
                    TEXEL0
                );
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress(ImageBody(g_SkyImage)),
                    G_IM_FMT_I,
                    G_IM_SIZ_8b,
                    SUN_GLOW_IMAGE_SIZE,
                    SUN_GLOW_IMAGE_SIZE,
                    0,
                    G_TX_MIRROR,
                    G_TX_MIRROR,
                    SUN_GLOW_MASK,
                    SUN_GLOW_MASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                gDPSetPrimColor(
                    gfx++,
                    0,
                    0,
                    colors->sun->primColor[RED_CHANNEL],
                    colors->sun->primColor[GREEN_CHANNEL],
                    colors->sun->primColor[BLUE_CHANNEL],
                    OPAQUE_ALPHA
                );
                gDPSetEnvColor(
                    gfx++,
                    colors->sun->envColor[RED_CHANNEL],
                    colors->sun->envColor[GREEN_CHANNEL],
                    colors->sun->envColor[BLUE_CHANNEL],
                    OPAQUE_ALPHA
                );
                gDPPipeSync(gfx++);
                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels((s32)f_SunScreenX[cameraIndex] - colors->sun->radius + 1),
                    QuarterPixels((s32)f_SunScreenY[cameraIndex] - colors->sun->radius + 1),
                    QuarterPixels((s32)f_SunScreenX[cameraIndex] + colors->sun->radius),
                    QuarterPixels((s32)f_SunScreenY[cameraIndex] + colors->sun->radius),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    colors->sun->texelStep,
                    colors->sun->texelStep
                );
                gDPPipeSync(gfx++);
            }
        }
    }

    *displayListHead = gfx;
}

// The course's backdrop image, drawn behind the course in strips from the
// horizon down
#define COURSE_BACKDROP_STRIP_COUNT 16
#define COURSE_BACKDROP_STRIP_HEIGHT 4
#define BACKDROP_IMAGE_WIDTH 512
#define BACKDROP_HORIZON_LEVEL 64.0f
#define STAFF_ROLL_BACKDROP_RISE 32
// How far the backdrop scrolls as the camera turns once
#define BACKDROP_YAW_TEXELS 49152.0f
#define BACKDROP_FIRST_CULLED_ROW (SCREEN_HEIGHT - SCREEN_BORDER + 1)
#define BACKDROP_ABOVE_SCREEN 32
// The scissor box's size before the shrink, and its changes per frame of it
#define FULL_VIEW_WIDTH (SCREEN_WIDTH - (2 * SCREEN_BORDER))
#define FULL_VIEW_HEIGHT (SCREEN_HEIGHT - (2 * SCREEN_BORDER))

// Draws the sky and, below it, the course's backdrop in strips that follow the
// camera's yaw and pitch
void DrawBackdrop(Gfx **displayListHead, CameraView *view, s32 cameraIndex) {
    Gfx *gfx;
    Boolean isNibbleImage;
    s32 centerShift;
    s32 yawOffset;
    s32 stripY;
    s32 row;
    s32 imageRow;
    s32 texelWord;
    s32 bottom;
    s32 top;
    s32 texelStepX;
    s32 texelStepY;
    s32 textureX;
    f32 inverseZoom;
    f32 zoom;

    gfx = *displayListHead;
    {
        f32 scaledHeight = view->zoom * FULL_VIEW_HEIGHT;
        // Centers the backdrop in the height difference between the two views
        f32 centerFraction = 0.5f;
        f32 viewportHeight = view->viewportCenterY - (view->scissorY * 2);

        centerShift = (scaledHeight - viewportHeight) * centerFraction;
    }
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_BAYER);
    DrawSky(&gfx, view->eye[AXIS_Y], 1, cameraIndex);
    isNibbleImage = FALSE;
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    if (cameraIndex == 0) {
        UpdateAndDrawFireworks(&gfx);
    }

    yawOffset = g_SkyState.yaws[cameraIndex] * BACKDROP_YAW_TEXELS * (1 / FULL_TURN_RADIANS);
    if (g_Scene == SCENE_STAFF_ROLL) {
        stripY =
            (s32)((BACKDROP_HORIZON_LEVEL - g_SkyState.horizonLevels[cameraIndex]) * view->zoom) -
            STAFF_ROLL_BACKDROP_RISE;
    }
    else {
        stripY = (BACKDROP_HORIZON_LEVEL - g_SkyState.horizonLevels[cameraIndex]) * view->zoom;
        if (cameraIndex == 0) {
            g_HorizonRow = stripY;
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
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(g_CourseSkyImage)));
    f_BackdropStripCount = COURSE_BACKDROP_STRIP_COUNT;
    f_BackdropStripHeight = COURSE_BACKDROP_STRIP_HEIGHT;
    f_BackdropImageWidth = BACKDROP_IMAGE_WIDTH;
    f_BackdropWidthMask = BACKDROP_IMAGE_WIDTH_MASK;
    if (view->scissorWidth == FULL_VIEW_WIDTH && view->scissorHeight == FULL_VIEW_HEIGHT) {
        s32 imageRow;
        s32 bottom;

        // The full screen copies the strips unscaled
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
        for (row = 0;; row++, stripY += f_BackdropStripHeight) {
            s32 clip;

            if (stripY >= BACKDROP_FIRST_CULLED_ROW) {
                break;
            }

            if (stripY < -f_BackdropStripHeight - BACKDROP_ABOVE_SCREEN) {
                continue;
            }

            if (row < f_BackdropStripCount - 1) {
                imageRow = row;
                texelWord = ((f_BackdropImageWidth * row * f_BackdropStripHeight) +
                                (LARGE_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE)) /
                    2;
            }
            else {
                texelWord =
                    ((f_BackdropImageWidth * (f_BackdropStripCount - 1) * f_BackdropStripHeight) +
                        (LARGE_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE)) /
                    2;
                imageRow = f_BackdropStripCount - 1;
            }

            if (!isNibbleImage) {
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress((u16 *)g_CourseSkyImage + PALETTE_START + texelWord),
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    f_BackdropImageWidth,
                    f_BackdropStripHeight,
                    0,
                    G_TX_WRAP,
                    G_TX_WRAP,
                    f_BackdropWidthMask,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
            }
            else {
                gDPSetTextureImage(
                    gfx++,
                    G_IM_FMT_CI,
                    G_IM_SIZ_16b,
                    1,
                    TextureAddress((u16 *)g_CourseSkyImage + texelWord)
                );
                gDPLoadSync(gfx++);
                gDPLoadBlock(
                    gfx++,
                    G_TX_LOADTILE,
                    0,
                    0,
                    (((f_BackdropImageWidth * f_BackdropStripHeight) + 3) >> 2) - 1,
                    CALC_DXT_4b(f_BackdropImageWidth)
                );
                gDPPipeSync(gfx++);
                gDPSetTile(
                    gfx++,
                    G_IM_FMT_CI,
                    G_IM_SIZ_4b,
                    ((f_BackdropImageWidth >> 1) + 7) >> 3,
                    0,
                    G_TX_RENDERTILE,
                    imageRow,
                    G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_WRAP,
                    f_BackdropWidthMask,
                    G_TX_NOLOD
                );
            }

            if (stripY >= 0) {
                clip = 0;
            }
            else {
                clip = -stripY;
            }

            if (row < f_BackdropStripCount - 1) {
                bottom = f_BackdropStripHeight + stripY + SCREEN_BORDER - 1;
                if (bottom > SCREEN_HEIGHT - SCREEN_BORDER) {
                    bottom = SCREEN_HEIGHT - SCREEN_BORDER;
                }

                gSPTextureRectangle(
                    gfx++,
                    QuarterPixels(SCREEN_BORDER),
                    QuarterPixels(SCREEN_BORDER + stripY + clip),
                    QuarterPixels(SCREEN_WIDTH - SCREEN_BORDER - 1),
                    QuarterPixels(bottom),
                    G_TX_RENDERTILE,
                    yawOffset,
                    TexelCoordinate(clip),
                    COPY_MODE_TEXEL_STEP,
                    TEXEL_STEP_ONE
                );
            }
            else {
                // The last strip stretches to the bottom of the screen
                gSPTextureRectangle(
                    gfx++,
                    QuarterPixels(SCREEN_BORDER),
                    QuarterPixels(SCREEN_BORDER + stripY + clip),
                    QuarterPixels(SCREEN_WIDTH - SCREEN_BORDER - 1),
                    QuarterPixels(SCREEN_HEIGHT - SCREEN_BORDER),
                    G_TX_RENDERTILE,
                    yawOffset,
                    TexelCoordinate(clip),
                    COPY_MODE_TEXEL_STEP,
                    TEXEL_STEP_ONE
                );
                break;
            }
        }
    }
    else {
        // A smaller view scales the strips by its zoom
        inverseZoom = 1.0f / view->zoom;
        texelStepX = inverseZoom * 1024.0f;
        texelStepY = inverseZoom * 2048.0f;
        if (f_IsPrimitiveDepthSet) {
            gDPSetPrimDepth(gfx++, 0x7FF3, 0);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
        }
        else {
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
        textureX =
            yawOffset + (s32)(((zoom * FULL_VIEW_WIDTH) - view->scissorWidth) * 16.0f / zoom);
        top = stripY + view->scissorY - centerShift;
        bottom = (f32)stripY + (f_BackdropStripHeight * zoom) + view->scissorY - centerShift;
        for (row = 0;; row++) {
            // A skipped strip steps on to the next by itself
            if (top < view->scissorY - (f_BackdropStripHeight * view->zoom)) {
                top = bottom;
                bottom = bottom + (f_BackdropStripHeight * view->zoom);
                continue;
            }

            if (row < f_BackdropStripCount - 1) {
                imageRow = row;
                texelWord = ((f_BackdropImageWidth * row * f_BackdropStripHeight) +
                                (LARGE_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE)) /
                    2;
            }
            else {
                texelWord =
                    ((f_BackdropImageWidth * (f_BackdropStripCount - 1) * f_BackdropStripHeight) +
                        (LARGE_PALETTE_COLOR_COUNT * PALETTE_COLOR_SIZE)) /
                    2;
                imageRow = f_BackdropStripCount - 1;
            }

            if (!isNibbleImage) {
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress((u16 *)g_CourseSkyImage + texelWord),
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    f_BackdropImageWidth,
                    f_BackdropStripHeight,
                    0,
                    G_TX_WRAP,
                    G_TX_WRAP,
                    f_BackdropWidthMask,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
            }
            else {
                gDPSetTextureImage(
                    gfx++,
                    G_IM_FMT_CI,
                    G_IM_SIZ_16b,
                    1,
                    TextureAddress((u16 *)g_CourseSkyImage + texelWord)
                );
                gDPLoadSync(gfx++);
                gDPLoadBlock(
                    gfx++,
                    G_TX_LOADTILE,
                    0,
                    0,
                    (((f_BackdropImageWidth * f_BackdropStripHeight) + 3) >> 2) - 1,
                    CALC_DXT_4b(f_BackdropImageWidth)
                );
                gDPPipeSync(gfx++);
                gDPSetTile(
                    gfx++,
                    G_IM_FMT_CI,
                    G_IM_SIZ_4b,
                    ((f_BackdropImageWidth >> 1) + 7) >> 3,
                    0,
                    G_TX_RENDERTILE,
                    imageRow,
                    G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_WRAP,
                    f_BackdropWidthMask,
                    G_TX_NOLOD
                );
            }

            if (row < f_BackdropStripCount - 1) {
                if (bottom > view->scissorY + view->scissorHeight) {
                    bottom = view->scissorY + view->scissorHeight;
                }
            }
            else {
                bottom = view->scissorY + view->scissorHeight;
            }

            if (bottom > view->scissorY + view->scissorHeight) {
                bottom = view->scissorY + view->scissorHeight;
            }

            gSPTextureRectangle(
                gfx++,
                QuarterPixels(view->scissorX),
                QuarterPixels(top),
                QuarterPixels(view->scissorX + view->scissorWidth),
                QuarterPixels(bottom),
                G_TX_RENDERTILE,
                textureX,
                0,
                texelStepX,
                texelStepY
            );
            top = bottom;
            if (bottom == view->scissorY + view->scissorHeight) {
                break;
            }

            bottom = bottom + (f_BackdropStripHeight * view->zoom);
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

// Starts one of the HUD's animations, which lasts a given number of frames, or
// stops it with HUD_ANIMATION_NONE
void StartHudAnimation(HudAnimation animationId) {
    if (animationId == HUD_ANIMATION_NONE) {
        f_IsFrameCapturePending = FALSE;
        f_HudAnimationFramesLeft = 0;
        return;
    }

    if (animationId != HUD_ANIMATION_SLICE_CAPTURE && animationId != HUD_ANIMATION_SLICES) {
        g_CapturedSliceCount = 0;
    }

    switch (animationId) {
        case HUD_ANIMATION_UNUSED:
            f_HudAnimationFramesLeft = 352;
            break;
        case HUD_ANIMATION_FADE_OUT:
        case HUD_ANIMATION_FAST_FADE_OUT:
            f_HudAnimationFramesLeft = OPAQUE_ALPHA;
            break;
        case HUD_ANIMATION_TURN_AWAY:
            f_HudAnimationFramesLeft = TILE_ANIMATION_FRAME_COUNT;
            break;
        case HUD_ANIMATION_SLIDE_APART:
            f_HudAnimationFramesLeft = SCREEN_WIDTH / 2;
            break;
        case HUD_ANIMATION_SPIRAL:
            f_HudAnimationFramesLeft = SPIRAL_START_TILE_COUNT;
            break;
        case HUD_ANIMATION_TILES:
            f_HudAnimationFramesLeft = TILE_ANIMATION_FRAME_COUNT;
            break;
        case HUD_ANIMATION_SLICES:
            f_HudAnimationFramesLeft = SLICE_ANIMATION_FRAME_COUNT;
            break;
        case HUD_ANIMATION_NEGATIVE:
        case HUD_ANIMATION_MOTION_BLUR:
            f_HudAnimationFramesLeft = 32;
            break;
    }

    f_HudAnimation = animationId;
    f_IsFrameCapturePending = TRUE;
    f_HudAnimationFrame = 0;
}

void DrawFadeAnimation(Gfx **displayListHead);

// The scheduler calls this with the frame buffer that the ended frame's task
// drew, as each frame's task ends, and it does nothing
void HandleFrameBufferSwap(void *frameBuffer UNUSED) {
}

// The fading animations draw f_CapturedFrame, which holds the frame inside
// the border, in strips. Each strip is loaded as a block, and the tiles are
// set up with the first.
#define FADE_FRAME_WIDTH (SCREEN_WIDTH - (2 * BACKDROP_BORDER))
#define FADE_STEP 12
#define FAST_FADE_STEP 24
#define NEGATIVE_BRIGHTNESS 127
#define MOTION_BLUR_ALPHA 95
// Blends nothing, but forces blending on
#define RM_FORCED_PASS GBL_c1(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1) | FORCE_BL
#define RM_FORCED_PASS2 GBL_c2(G_BL_CLR_IN, G_BL_0, G_BL_CLR_IN, G_BL_1)
#define FADE_COUNT_PERIOD 8

#define LoadFadeFrameStrip(gfx, texels, height, isTileSet) \
    { \
        if (!(isTileSet)) { \
            gDPSetTextureImage((gfx)++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, TextureAddress(texels)); \
            gDPSetTile( \
                (gfx)++, \
                G_IM_FMT_RGBA, \
                G_IM_SIZ_16b, \
                0, \
                0, \
                G_TX_LOADTILE, \
                0, \
                0, \
                G_TX_NOMASK, \
                G_TX_NOLOD, \
                0, \
                G_TX_NOMASK, \
                G_TX_NOLOD \
            ); \
            gDPLoadSync((gfx)++); \
            gDPLoadBlock( \
                (gfx)++, \
                G_TX_LOADTILE, \
                0, \
                0, \
                ((height) * FADE_FRAME_WIDTH) - 1, \
                CALC_DXT(FADE_FRAME_WIDTH, G_IM_SIZ_16b_BYTES) \
            ); \
            gDPPipeSync((gfx)++); \
            gDPSetTile( \
                (gfx)++, \
                G_IM_FMT_RGBA, \
                G_IM_SIZ_16b, \
                ((FADE_FRAME_WIDTH * G_IM_SIZ_16b_BYTES) + 7) >> 3, \
                0, \
                G_TX_RENDERTILE, \
                0, \
                0, \
                G_TX_NOMASK, \
                G_TX_NOLOD, \
                0, \
                G_TX_NOMASK, \
                G_TX_NOLOD \
            ); \
            gDPSetTileSize( \
                (gfx)++, \
                G_TX_RENDERTILE, \
                0, \
                0, \
                (FADE_FRAME_WIDTH - 1) << G_TEXTURE_IMAGE_FRAC, \
                ((height) - 1) << G_TEXTURE_IMAGE_FRAC \
            ); \
            (isTileSet) = TRUE; \
        } \
        else { \
            gDPSetTextureImage((gfx)++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, TextureAddress(texels)); \
            gDPLoadSync((gfx)++); \
            gDPLoadBlock( \
                (gfx)++, \
                G_TX_LOADTILE, \
                0, \
                0, \
                ((height) * FADE_FRAME_WIDTH) - 1, \
                CALC_DXT(FADE_FRAME_WIDTH, G_IM_SIZ_16b_BYTES) \
            ); \
            gDPPipeSync((gfx)++); \
        } \
    }

// Draws the HUD animations that show the whole frame: fading it out,
// drawing it at half brightness, blurring the scene with the last frame, or
// splitting the frame into halves that slide apart. When a fade ends,
// the unread f_UnusedFadeEndCount counts up, wrapping at FADE_COUNT_PERIOD.
void DrawFadeAnimation(Gfx **displayListHead) {
    // The frame drawn: the captured one, or the last one for the motion blur
    u16 *frame = (u16 *)f_CapturedFrame;
    Gfx *gfx = *displayListHead;
    Boolean isTileSet;
    // The strip, or in the last loop its top row
    s32 i;
    // The strip of the captured frame, stepped beside i
    s32 strip;

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
        if (g_MenuBackdropState == MENU_BACKDROP_STATE_NONE) {
            if (f_HudAnimation == HUD_ANIMATION_NEGATIVE) {
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, RM_FORCED_PASS, RM_FORCED_PASS2);
                gDPSetPrimColor(
                    gfx++,
                    0,
                    0,
                    NEGATIVE_BRIGHTNESS,
                    NEGATIVE_BRIGHTNESS,
                    NEGATIVE_BRIGHTNESS,
                    OPAQUE_ALPHA
                );
            }
            else if ((f_HudAnimation == HUD_ANIMATION_FADE_OUT) ||
                (f_HudAnimation == HUD_ANIMATION_FAST_FADE_OUT)) {
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
                gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, f_HudAnimationFramesLeft);
            }
            else if (f_HudAnimation == HUD_ANIMATION_MOTION_BLUR) {
                gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, MOTION_BLUR_ALPHA);
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
                gDPSetCombineMode(gfx++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
                frame = f_LastFrameBuffer;
                for (i = 0; i != BACKDROP_STRIP_COUNT; i++) {
                    s32 height = StripHeight(i);

                    gDPLoadTextureTile(
                        gfx++,
                        TextureAddress(&frame[i * BACKDROP_STRIP_STEP * SCREEN_WIDTH]),
                        G_IM_FMT_RGBA,
                        G_IM_SIZ_16b,
                        SCREEN_WIDTH,
                        0,
                        BACKDROP_BORDER,
                        BACKDROP_BORDER,
                        BACKDROP_RIGHT,
                        BACKDROP_BORDER + height - 1,
                        0,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOLOD
                    );
                    gSPScisTextureRectangle(
                        gfx++,
                        QuarterPixels(BACKDROP_BORDER),
                        QuarterPixels(i * BACKDROP_STRIP_STEP + BACKDROP_BORDER),
                        QuarterPixels(BACKDROP_RIGHT + 1),
                        QuarterPixels(i * BACKDROP_STRIP_STEP + BACKDROP_BORDER + height),
                        G_TX_RENDERTILE,
                        TexelCoordinate(BACKDROP_BORDER),
                        TexelCoordinate(BACKDROP_BORDER),
                        TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE
                    );
                }

                *displayListHead = gfx;
                return;
            }
            else if ((f_HudAnimation == HUD_ANIMATION_SLIDE_APART) ||
                (f_HudAnimation == HUD_ANIMATION_SPIRAL)) {
                // DrawHudAnimation draws HUD_ANIMATION_SPIRAL with DrawSpiralAnimation
                // instead
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, RM_FORCED_PASS, RM_FORCED_PASS2);
                gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
                gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
                gDPPipeSync(gfx++);
                strip = 0;
                for (i = 0; i != BACKDROP_STRIP_COUNT; i++) {
                    s32 height = StripHeight(i);
                    s32 framesLeft;

                    LoadFadeFrameStrip(
                        gfx,
                        &frame[strip * BACKDROP_STRIP_STEP * FADE_FRAME_WIDTH],
                        height,
                        isTileSet
                    );
                    gDPPipeSync(gfx++);
                    framesLeft = f_HudAnimationFramesLeft;
                    gSPScisTextureRectangle(
                        gfx++,
                        QuarterPixels(BACKDROP_BORDER),
                        QuarterPixels(i * BACKDROP_STRIP_STEP + BACKDROP_BORDER),
                        QuarterPixels(framesLeft),
                        QuarterPixels(i * BACKDROP_STRIP_STEP + BACKDROP_BORDER + height),
                        G_TX_RENDERTILE,
                        TexelCoordinate((SCREEN_WIDTH / 2) - framesLeft),
                        0,
                        TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE
                    );
                    gSPScisTextureRectangle(
                        gfx++,
                        QuarterPixels(SCREEN_WIDTH - framesLeft),
                        QuarterPixels(i * BACKDROP_STRIP_STEP + BACKDROP_BORDER),
                        QuarterPixels(BACKDROP_RIGHT + 1),
                        QuarterPixels(i * BACKDROP_STRIP_STEP + BACKDROP_BORDER + height),
                        G_TX_RENDERTILE,
                        TexelCoordinate(FADE_FRAME_WIDTH / 2),
                        0,
                        TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE
                    );
                    gDPPipeSync(gfx++);
                    strip++;
                }

                f_HudAnimationFramesLeft -= FADE_STEP;
                *displayListHead = gfx;
                return;
            }
        }

        gDPSetCombineMode(gfx++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
        gDPPipeSync(gfx++);
        strip = 0;
        for (i = BACKDROP_BORDER; i < SCREEN_HEIGHT - BACKDROP_BORDER; i += BACKDROP_STRIP_STEP) {
            // Written with | for the reason StripHeight is
            s32 height = BACKDROP_LAST_STRIP_HEIGHT |
                ((i + BACKDROP_STRIP_STEP <= SCREEN_HEIGHT - BACKDROP_BORDER) ?
                        (BACKDROP_STRIP_HEIGHT - BACKDROP_LAST_STRIP_HEIGHT) :
                        0);

            LoadFadeFrameStrip(
                gfx,
                &frame[strip * BACKDROP_STRIP_STEP * FADE_FRAME_WIDTH],
                height,
                isTileSet
            );
            gSPTextureRectangle(
                gfx++,
                QuarterPixels(BACKDROP_BORDER),
                QuarterPixels(i),
                QuarterPixels(BACKDROP_RIGHT + 1),
                QuarterPixels(i + height),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            strip++;
        }

        if (f_HudAnimation == HUD_ANIMATION_FAST_FADE_OUT) {
            f_HudAnimationFramesLeft -= FAST_FADE_STEP;
        }
        else {
            f_HudAnimationFramesLeft -= FADE_STEP;
        }

        if (f_HudAnimationFramesLeft < 0) {
            f_UnusedFadeEndCount = (f_UnusedFadeEndCount + 1) % FADE_COUNT_PERIOD;
        }
    }

    *displayListHead = gfx;
}

// Draws HUD_ANIMATION_SPIRAL, where the frame is cut into a grid of tiles. It
// draws the tiles along a spiral from the top left corner inwards, and draws
// fewer of them each frame, so the frame disappears from the center out. The
// few tiles after the opaque ones fade, more the further in they are.
void DrawSpiralAnimation(Gfx **displayListHead) {
    s32 tileColumn;
    s32 rightColumn;
    s32 topRow;
    SpiralDirection direction;
    s32 tileRow;
    s32 bottomRow;
    s32 leftColumn;
    Gfx *gfx = *displayListHead;
    u32 *frame = f_CapturedFrame;
    s32 tileCount;
    s32 tileIndex;
    s32 alpha;

    // Steps to the next tile of the spiral, turning at each edge and moving the
    // edge in
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
        alpha = OPAQUE_ALPHA;
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
            gDPLoadTextureTile(
                gfx++,
                TextureAddress(frame),
                G_IM_FMT_RGBA,
                G_IM_SIZ_16b,
                SPIRAL_FRAME_WIDTH,
                0,
                tileColumn * SPIRAL_TILE_WIDTH,
                tileRow * SPIRAL_TILE_HEIGHT,
                (tileColumn * SPIRAL_TILE_WIDTH) + SPIRAL_TILE_WIDTH - 1,
                (tileRow * SPIRAL_TILE_HEIGHT) + SPIRAL_TILE_HEIGHT - 1,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPTextureRectangle(
                gfx++,
                QuarterPixels((tileColumn * SPIRAL_TILE_WIDTH) + FRAME_MARGIN_ROWS),
                QuarterPixels((tileRow * SPIRAL_TILE_HEIGHT) + FRAME_MARGIN_ROWS),
                QuarterPixels(
                    (tileColumn * SPIRAL_TILE_WIDTH) + FRAME_MARGIN_ROWS + SPIRAL_TILE_WIDTH
                ),
                QuarterPixels(
                    (tileRow * SPIRAL_TILE_HEIGHT) + FRAME_MARGIN_ROWS + SPIRAL_TILE_HEIGHT
                ),
                G_TX_RENDERTILE,
                TexelCoordinate(tileColumn * SPIRAL_TILE_WIDTH),
                TexelCoordinate(tileRow * SPIRAL_TILE_HEIGHT),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
            StepSpiral();
        }

        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        gDPSetCombineLERP(
            gfx++,
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
            TEXEL0,
            0,
            0,
            0,
            PRIMITIVE
        );
        tileCount += SPIRAL_FADING_TILE_COUNT;
        if (tileCount > SPIRAL_TILE_COUNT) {
            tileCount = SPIRAL_TILE_COUNT;
        }

        for (; tileIndex != tileCount; tileIndex++) {
            alpha -= SPIRAL_ALPHA_STEP;
            gDPLoadTextureTile(
                gfx++,
                TextureAddress(frame),
                G_IM_FMT_RGBA,
                G_IM_SIZ_16b,
                SPIRAL_FRAME_WIDTH,
                0,
                tileColumn * SPIRAL_TILE_WIDTH,
                tileRow * SPIRAL_TILE_HEIGHT,
                (tileColumn * SPIRAL_TILE_WIDTH) + SPIRAL_TILE_WIDTH - 1,
                (tileRow * SPIRAL_TILE_HEIGHT) + SPIRAL_TILE_HEIGHT - 1,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPSetPrimColor(gfx++, 0, 0, SPIRAL_SHADE, SPIRAL_SHADE, SPIRAL_SHADE, alpha);
            gDPPipeSync(gfx++);
            gSPTextureRectangle(
                gfx++,
                QuarterPixels((tileColumn * SPIRAL_TILE_WIDTH) + FRAME_MARGIN_ROWS),
                QuarterPixels((tileRow * SPIRAL_TILE_HEIGHT) + FRAME_MARGIN_ROWS),
                QuarterPixels(
                    (tileColumn * SPIRAL_TILE_WIDTH) + FRAME_MARGIN_ROWS + SPIRAL_TILE_WIDTH
                ),
                QuarterPixels(
                    (tileRow * SPIRAL_TILE_HEIGHT) + FRAME_MARGIN_ROWS + SPIRAL_TILE_HEIGHT
                ),
                G_TX_RENDERTILE,
                TexelCoordinate(tileColumn * SPIRAL_TILE_WIDTH),
                TexelCoordinate(tileRow * SPIRAL_TILE_HEIGHT),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
            StepSpiral();
        }

        f_HudAnimationFramesLeft -= SPIRAL_FADING_TILE_COUNT;
    }

    *displayListHead = gfx;
}

// Shows a menu's backdrop in place of the course, or MENU_BACKDROP_NONE to show
// the course again
void SetMenuBackdrop(MenuBackdrop backdrop) {
    if (backdrop == MENU_BACKDROP_NONE) {
        g_MenuBackdropState = MENU_BACKDROP_STATE_NONE;
        return;
    }

    if (backdrop == MENU_BACKDROP_PATTERN) {
        f_MenuBackdrop = backdrop;
        g_MenuBackdropState = MENU_BACKDROP_STATE_SHOWN;
        return;
    }

    g_MenuBackdropState = MENU_BACKDROP_STATE_CAPTURING;
    f_MenuBackdrop = backdrop;
}

// Starts the plain menu backdrop by capturing the last frame for it
void CaptureMenuBackdrop(Gfx **displayListHead, s32 unused UNUSED) {
    Gfx *gfx = *displayListHead;
    s32 strip;
    s16 height;
    u16 *source;

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
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    if (g_MenuBackdropState == MENU_BACKDROP_STATE_CAPTURING) {
        source = f_LastFrameBuffer;
        g_MenuBackdropState = MENU_BACKDROP_STATE_SHOWN;
        f_BackdropFadeLevel = BACKDROP_START_FADE_LEVEL;
        gDPPipeSync(gfx++);
        gDPSetColorImage(
            gfx++,
            G_IM_FMT_RGBA,
            G_IM_SIZ_16b,
            SCREEN_WIDTH,
            RenderTargetAddress(PhysicalAddress(f_CapturedFrame))
        );
        gDPPipeSync(gfx++);
        for (strip = 0; strip != BACKDROP_STRIP_COUNT; strip++) {
            height = StripHeight(strip);
            gDPLoadTextureTile(
                gfx++,
                TextureAddress(&source[strip * BACKDROP_STRIP_STEP * SCREEN_WIDTH]),
                G_IM_FMT_RGBA,
                G_IM_SIZ_16b,
                SCREEN_WIDTH,
                0,
                BACKDROP_BORDER,
                BACKDROP_BORDER,
                BACKDROP_RIGHT,
                BACKDROP_BORDER + height - 1,
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
                QuarterPixels(BACKDROP_BORDER),
                QuarterPixels(strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER),
                QuarterPixels(BACKDROP_RIGHT),
                QuarterPixels(strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER + height - 1),
                G_TX_RENDERTILE,
                TexelCoordinate(BACKDROP_BORDER),
                TexelCoordinate(BACKDROP_BORDER),
                COPY_MODE_TEXEL_STEP,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }

        gDPPipeSync(gfx++);
        gDPSetColorImage(
            gfx++,
            G_IM_FMT_RGBA,
            G_IM_SIZ_16b,
            SCREEN_WIDTH,
            RenderTargetAddress(PhysicalAddress(nuGfxCfb_ptr))
        );
    }

    *displayListHead = gfx;
}

// MENU_BACKDROP_TINTED's color
#define BACKDROP_TINT_RED 144
#define BACKDROP_TINT_GREEN 104
#define BACKDROP_TINT_BLUE 32
// MENU_BACKDROP_PATTERN's image, which scrolls diagonally by a texel a frame
#define BACKDROP_PATTERN_SIZE 64
// log2(BACKDROP_PATTERN_SIZE)
#define BACKDROP_PATTERN_MASK 6

// Draws the menu backdrop that SetMenuBackdrop chose, inside the backdrop's
// border.
void DrawMenuBackdrop(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 strip;
    s16 height;

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
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
        gDPPipeSync(gfx++);
        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(ImageTexels(g_BackdropPatternImage)),
            G_IM_FMT_CI,
            BACKDROP_PATTERN_SIZE,
            BACKDROP_PATTERN_SIZE,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            BACKDROP_PATTERN_MASK,
            BACKDROP_PATTERN_MASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(g_BackdropPatternImage)));
        gDPPipeSync(gfx++);
        gSPTextureRectangle(
            gfx++,
            QuarterPixels(BACKDROP_BORDER),
            QuarterPixels(BACKDROP_BORDER),
            QuarterPixels(SCREEN_WIDTH - BACKDROP_BORDER),
            QuarterPixels(SCREEN_HEIGHT - BACKDROP_BORDER),
            G_TX_RENDERTILE,
            TexelCoordinate(f_BackdropPatternScroll),
            TexelCoordinate(f_BackdropPatternScroll),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gDPPipeSync(gfx++);
    }
    else if ((f_MenuBackdrop == MENU_BACKDROP_STILL) | (f_MenuBackdrop == MENU_BACKDROP_DIMMING)) {
        if (f_MenuBackdrop == MENU_BACKDROP_STILL) {
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
        }
        else {
            gDPSetPrimColor(
                gfx++,
                0,
                0,
                (u8)f_BackdropFadeLevel,
                (u8)f_BackdropFadeLevel,
                (u8)f_BackdropFadeLevel,
                OPAQUE_ALPHA
            );
        }

        for (strip = 0; strip != BACKDROP_STRIP_COUNT; strip++) {
            height = StripHeight(strip);
            gDPLoadTextureTile(
                gfx++,
                AddressNumber(&f_CapturedFrame[strip * BACKDROP_STRIP_STEP * FRAME_WORDS_PER_ROW]) &
                    ~7,
                G_IM_FMT_RGBA,
                G_IM_SIZ_16b,
                SCREEN_WIDTH,
                0,
                BACKDROP_BORDER,
                BACKDROP_BORDER,
                BACKDROP_RIGHT,
                BACKDROP_BORDER + height - 1,
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
                QuarterPixels(BACKDROP_BORDER),
                QuarterPixels(strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER),
                QuarterPixels(BACKDROP_RIGHT + 1),
                QuarterPixels(strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER + height),
                G_TX_RENDERTILE,
                TexelCoordinate(BACKDROP_BORDER),
                TexelCoordinate(BACKDROP_BORDER),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }
    }
    else {
        gDPSetPrimColor(
            gfx++,
            0,
            0,
            BACKDROP_TINT_RED,
            BACKDROP_TINT_GREEN,
            BACKDROP_TINT_BLUE,
            OPAQUE_ALPHA
        );
        for (strip = 0; strip != BACKDROP_STRIP_COUNT; strip++) {
            height = StripHeight(strip);
            gDPLoadTextureTile(
                gfx++,
                AddressNumber(&f_CapturedFrame[strip * BACKDROP_STRIP_STEP * FRAME_WORDS_PER_ROW]) &
                    ~7,
                G_IM_FMT_IA,
                G_IM_SIZ_16b,
                SCREEN_WIDTH,
                0,
                BACKDROP_BORDER,
                BACKDROP_BORDER,
                BACKDROP_RIGHT,
                BACKDROP_BORDER + height - 1,
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
                QuarterPixels(BACKDROP_BORDER),
                QuarterPixels(strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER),
                QuarterPixels(BACKDROP_RIGHT + 1),
                QuarterPixels(strip * BACKDROP_STRIP_STEP + BACKDROP_BORDER + height),
                G_TX_RENDERTILE,
                TexelCoordinate(BACKDROP_BORDER),
                TexelCoordinate(BACKDROP_BORDER),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }
    }

    *displayListHead = gfx;
}

// Draws HUD_ANIMATION_TILES, where the frame's tiles turn over and fly apart
void DrawTileAnimation(void) {
    Vec3 rotation;
    Vec3 translation;
    Mat4 matrix;
    Radians angle;
    f32 unusedSine UNUSED;
    f32 unusedCosine UNUSED;
    s32 progress;
    s32 tileRow;
    s32 tileColumn;
    s32 wordIndex;

    if (f_HudAnimationFramesLeft > 0) {
        angle = (f32)((M_PI / 2) -
                    (f_HudAnimationFramesLeft * (M_PI / 2 / TILE_ANIMATION_FRAME_COUNT))) +
            TILE_ANIMATION_UNUSED_ANGLE_OFFSET;
        unusedSine = sinf(angle);
        unusedCosine = cosf(angle);
        gDPPipeSync(g_DisplayListHead++);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
        gDPPipeSync(g_DisplayListHead++);
        gSPClearGeometryMode(g_DisplayListHead++, ALL_GEOMETRY_MODES);
        gSPSetGeometryMode(g_DisplayListHead++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetRenderMode(g_DisplayListHead++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
        gSPPerspNormalize(g_DisplayListHead++, g_ScreenEffectPerspectiveNormalization);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
        gDPSetCombineMode(g_DisplayListHead++, G_CC_DECALRGB, G_CC_DECALRGB);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
        gSPMatrix(
            g_DisplayListHead++,
            PhysicalAddress(&g_ScreenEffectProjection),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
        );
        gDPPipeSync(g_DisplayListHead++);
        rotation[AXIS_X] = 0.0f;
        rotation[AXIS_Y] =
            (M_PI / 2) - (f_HudAnimationFramesLeft * (M_PI / 2 / TILE_ANIMATION_FRAME_COUNT));
        rotation[AXIS_Z] = rotation[AXIS_Y];
        wordIndex = 0;
        for (tileRow = 0; tileRow != FRAME_TILE_ROW_COUNT; tileRow++) {
            for (tileColumn = 0; tileColumn != FRAME_TILE_COLUMN_COUNT; tileColumn++) {
                progress = TILE_ANIMATION_FRAME_COUNT - f_HudAnimationFramesLeft;
                translation[AXIS_X] = ((progress + TILE_ANIMATION_GAP_X) *
                                          (tileColumn - (FRAME_TILE_COLUMN_COUNT / 2))) +
                    TILE_ANIMATION_OFFSET_X;
                translation[AXIS_Y] =
                    -(progress + TILE_ANIMATION_GAP_Y) * (tileRow - (FRAME_TILE_ROW_COUNT / 2));
                translation[AXIS_Z] = TILE_ANIMATION_DEPTH - (progress * TILE_ANIMATION_SPEED_Z);
                BuildRotationMatrix(matrix, rotation, translation);
                ConvertMatrixToFixed(
                    matrix,
                    &f_TileMatrices[g_MatrixBufferIndex]
                                   [(tileRow * FRAME_TILE_COLUMN_COUNT) + tileColumn]
                );
                gSPMatrix(
                    g_DisplayListHead++,
                    PhysicalAddress(
                        &f_TileMatrices[g_MatrixBufferIndex]
                                       [tileRow * FRAME_TILE_COLUMN_COUNT + tileColumn]
                    ),
                    G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
                );
                if (tileColumn == FRAME_TILE_COLUMN_COUNT - 1) {
                    gSPVertex(g_DisplayListHead++, f_LastColumnTileVertices, TILE_VERTEX_COUNT, 0);
                }
                else {
                    gSPVertex(g_DisplayListHead++, f_TileVertices, TILE_VERTEX_COUNT, 0);
                }

                gDPLoadTextureBlock(
                    g_DisplayListHead++,
                    TextureAddress(&f_CapturedFrame[wordIndex]),
                    G_IM_FMT_RGBA,
                    G_IM_SIZ_16b,
                    FRAME_TILE_WIDTH,
                    FRAME_TILE_ROWS,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                gDPPipeSync(g_DisplayListHead++);
                wordIndex += FRAME_TILE_WORD_COUNT;
                gSP2Triangles(g_DisplayListHead++, 0, 1, 2, 0, 0, 2, 3, 0);
                gDPPipeSync(g_DisplayListHead++);
            }
        }

        gDPPipeSync(g_DisplayListHead++);
        f_HudAnimationFramesLeft -= TILE_ANIMATION_STEP;
    }
}

// HUD_ANIMATION_TURN_AWAY turns the whole frame away about two axes as it
// moves away from the camera. The frame's tiles are drawn on the tile grid,
// the upper rows from one load of its vertices and the rest from another;
// only the upper rows have a back, drawn in their shade.
#define TURN_AWAY_UPPER_ROW_COUNT 4
#define TURN_AWAY_LOWER_ROW_COUNT (FRAME_TILE_ROW_COUNT - TURN_AWAY_UPPER_ROW_COUNT)
// The last column's texture coordinates span this many texels
#define LAST_TILE_TEXEL_WIDTH 47
// The lower rows' texture coordinates stop a quarter texel short, in 10.5 fixed
// point
#define LOWER_TILE_TEXTURE_COORDINATE_INSET 8
// The index of the tile grid vertex at a row and column
#define GridVertex(row, column) ((column) + ((row) * TILE_GRID_COLUMN_COUNT))
// The grid vertices at the corners of the tile at a row and column. The lower
// ones are counted on from the upper left in the grid's row order, a row's
// vertices further on.
#define TileUpperLeft(row, column) GridVertex(row, column)
#define TileUpperRight(row, column) GridVertex(row, (column) + 1)
#define TileLowerLeft(row, column) GridVertex(row, (column) + TILE_GRID_COLUMN_COUNT)
#define TileLowerRight(row, column) GridVertex(row, (column) + TILE_GRID_COLUMN_COUNT + 1)
static Mtx f_TurnAwayMatrices[MATRIX_BUFFER_COUNT];

void DrawTurnAwayAnimation(void);

// Draws HUD_ANIMATION_TURN_AWAY
void DrawTurnAwayAnimation(void) {
    Vec3 rotation;
    Vec3 translation;
    Mat4 matrix;
    s32 wordIndex;
    s32 row;
    s32 column;
    s32 width;

    if (f_HudAnimationFramesLeft > 0) {
        rotation[AXIS_X] = 0.0f;
        rotation[AXIS_Y] =
            (M_PI / 2) - (f_HudAnimationFramesLeft * (M_PI / 2 / TILE_ANIMATION_FRAME_COUNT));
        rotation[AXIS_Z] = rotation[AXIS_Y];
        translation[AXIS_X] = 0.0f;
        translation[AXIS_Y] = 0.0f;
        translation[AXIS_Z] =
            TILE_ANIMATION_DEPTH - (TILE_ANIMATION_FRAME_COUNT - f_HudAnimationFramesLeft);
        BuildRotationMatrix(matrix, rotation, translation);
        ConvertMatrixToFixed(matrix, &f_TurnAwayMatrices[g_MatrixBufferIndex]);
        gDPPipeSync(g_DisplayListHead++);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
        gDPPipeSync(g_DisplayListHead++);
        gSPClearGeometryMode(g_DisplayListHead++, ALL_GEOMETRY_MODES);
        gSPSetGeometryMode(g_DisplayListHead++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetRenderMode(g_DisplayListHead++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
        gSPPerspNormalize(g_DisplayListHead++, g_ScreenEffectPerspectiveNormalization);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
        gDPPipeSync(g_DisplayListHead++);
        gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
        gSPMatrix(
            g_DisplayListHead++,
            PhysicalAddress(&g_ScreenEffectProjection),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
        );
        gSPMatrix(
            g_DisplayListHead++,
            PhysicalAddress(&f_TurnAwayMatrices[g_MatrixBufferIndex]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
        );
        gSPVertex(
            g_DisplayListHead++,
            f_TileGridVertices[g_MatrixBufferIndex],
            (TURN_AWAY_UPPER_ROW_COUNT + 1) * TILE_GRID_COLUMN_COUNT,
            0
        );
        wordIndex = 0;
        gDPSetCombineMode(g_DisplayListHead++, G_CC_DECALRGB, G_CC_DECALRGB);
        gDPPipeSync(g_DisplayListHead++);
        for (row = 0; row != TURN_AWAY_UPPER_ROW_COUNT; row++) {
            for (column = 0; column != FRAME_TILE_COLUMN_COUNT; column++) {
                gDPLoadTextureBlock(
                    g_DisplayListHead++,
                    TextureAddress(&f_CapturedFrame[wordIndex]),
                    G_IM_FMT_RGBA,
                    G_IM_SIZ_16b,
                    FRAME_TILE_WIDTH,
                    FRAME_TILE_ROWS,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                wordIndex += FRAME_TILE_WORD_COUNT;
                if (column == FRAME_TILE_COLUMN_COUNT - 1) {
                    width = LAST_TILE_TEXEL_WIDTH;
                }
                else {
                    width = FRAME_TILE_WIDTH;
                }

                gSPModifyVertex(g_DisplayListHead++, TileUpperLeft(row, column), G_MWO_POINT_ST, 0);
                gSPModifyVertex(
                    g_DisplayListHead++,
                    TileLowerLeft(row, column),
                    G_MWO_POINT_ST,
                    TexelCoordinate(FRAME_TILE_ROWS)
                );
                gSPModifyVertex(
                    g_DisplayListHead++,
                    TileLowerRight(row, column),
                    G_MWO_POINT_ST,
                    TextureCoordinateWord(TexelCoordinate(width), TexelCoordinate(FRAME_TILE_ROWS))
                );
                gSPModifyVertex(
                    g_DisplayListHead++,
                    TileUpperRight(row, column),
                    G_MWO_POINT_ST,
                    TextureCoordinateWord(TexelCoordinate(width), 0)
                );
                gDPPipeSync(g_DisplayListHead++);
                gSP2Triangles(
                    g_DisplayListHead++,
                    TileUpperLeft(row, column),
                    TileLowerLeft(row, column),
                    TileLowerRight(row, column),
                    0,
                    TileUpperLeft(row, column),
                    TileLowerRight(row, column),
                    TileUpperRight(row, column),
                    0
                );
                gDPPipeSync(g_DisplayListHead++);
            }
        }

        gDPPipeSync(g_DisplayListHead++);
        gDPSetCombineMode(g_DisplayListHead++, G_CC_SHADE, G_CC_SHADE);
        gDPPipeSync(g_DisplayListHead++);
        for (row = 0; row != TURN_AWAY_UPPER_ROW_COUNT; row++) {
            for (column = 0; column != FRAME_TILE_COLUMN_COUNT; column++) {
                gSP2Triangles(
                    g_DisplayListHead++,
                    TileLowerRight(row, column),
                    TileLowerLeft(row, column),
                    TileUpperLeft(row, column),
                    0,
                    TileUpperRight(row, column),
                    TileLowerRight(row, column),
                    TileUpperLeft(row, column),
                    0
                );
            }
        }

        gDPPipeSync(g_DisplayListHead++);
        gDPSetCombineMode(g_DisplayListHead++, G_CC_DECALRGB, G_CC_DECALRGB);
        gDPPipeSync(g_DisplayListHead++);
        gSPVertex(
            g_DisplayListHead++,
            &f_TileGridVertices[g_MatrixBufferIndex]
                               [TURN_AWAY_UPPER_ROW_COUNT * TILE_GRID_COLUMN_COUNT],
            (TURN_AWAY_LOWER_ROW_COUNT + 1) * TILE_GRID_COLUMN_COUNT,
            0
        );
        for (row = 0; row != TURN_AWAY_LOWER_ROW_COUNT; row++) {
            for (column = 0; column != FRAME_TILE_COLUMN_COUNT; column++) {
                gDPLoadTextureBlock(
                    g_DisplayListHead++,
                    TextureAddress(&f_CapturedFrame[wordIndex]),
                    G_IM_FMT_RGBA,
                    G_IM_SIZ_16b,
                    FRAME_TILE_WIDTH,
                    FRAME_TILE_ROWS,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                wordIndex += FRAME_TILE_WORD_COUNT;
                if (column == FRAME_TILE_COLUMN_COUNT - 1) {
                    width = LAST_TILE_TEXEL_WIDTH;
                }
                else {
                    width = FRAME_TILE_WIDTH;
                }

                gDPPipeSync(g_DisplayListHead++);
                gSPModifyVertex(g_DisplayListHead++, TileUpperLeft(row, column), G_MWO_POINT_ST, 0);
                gSPModifyVertex(
                    g_DisplayListHead++,
                    TileLowerLeft(row, column),
                    G_MWO_POINT_ST,
                    TexelCoordinate(FRAME_TILE_ROWS) - LOWER_TILE_TEXTURE_COORDINATE_INSET
                );
                gSPModifyVertex(
                    g_DisplayListHead++,
                    TileLowerRight(row, column),
                    G_MWO_POINT_ST,
                    TextureCoordinateWord(
                        TexelCoordinate(width) - LOWER_TILE_TEXTURE_COORDINATE_INSET,
                        TexelCoordinate(FRAME_TILE_ROWS) - LOWER_TILE_TEXTURE_COORDINATE_INSET
                    )
                );
                gSPModifyVertex(
                    g_DisplayListHead++,
                    TileUpperRight(row, column),
                    G_MWO_POINT_ST,
                    TextureCoordinateWord(
                        TexelCoordinate(width) - LOWER_TILE_TEXTURE_COORDINATE_INSET,
                        0
                    )
                );
                gSP2Triangles(
                    g_DisplayListHead++,
                    TileUpperLeft(row, column),
                    TileLowerLeft(row, column),
                    TileLowerRight(row, column),
                    0,
                    TileUpperLeft(row, column),
                    TileLowerRight(row, column),
                    TileUpperRight(row, column),
                    0
                );
                gDPPipeSync(g_DisplayListHead++);
            }
        }

        gDPPipeSync(g_DisplayListHead++);
        gDPSetCombineMode(g_DisplayListHead++, G_CC_SHADE, G_CC_SHADE);
        gDPPipeSync(g_DisplayListHead++);
        f_HudAnimationFramesLeft -= TILE_ANIMATION_STEP;
    }
}

// Copies the frame that a HUD animation that has just started breaks apart
void CaptureHudAnimationFrame(void) {
    u32 *frame;
    s32 tileStart;
    // Counts the tiles' and the grid's rows, and the fade and negative copies' words
    s32 tileRow;
    // Counts the tiles' and the grid's columns, and the slice copy's words
    s32 tileColumn;
    s32 row;
    s32 column;
    Vtx *vertex;
    Vtx *nextVertex;

    if (!f_IsFrameCapturePending) {
        return;
    }

    frame = (u32 *)nuGfxCfb_ptr;
#ifdef TARGET_PC
    // A native build's RDP leaves the frame on the GPU until it is read
    osPcSyncRdpWrites(frame, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(u16));
#endif
    f_IsFrameCapturePending = FALSE;
    if (f_HudAnimation == HUD_ANIMATION_TURN_AWAY || f_HudAnimation == HUD_ANIMATION_TILES) {
        s32 wordIndex = 0;

        for (tileRow = 0; tileRow != FRAME_TILE_ROW_COUNT; tileRow++) {
            for (tileColumn = 0; tileColumn != FRAME_TILE_COLUMN_COUNT; tileColumn++) {
                // The word the tile's rows count from
                tileStart = (tileRow * FRAME_TILE_ROWS * FRAME_WORDS_PER_ROW) +
                    (tileColumn * FRAME_TILE_WORDS);
                for (row = FRAME_MARGIN_ROWS; row != FRAME_MARGIN_ROWS + FRAME_TILE_ROWS; row++) {
                    for (column = FRAME_MARGIN_WORDS;
                        column != FRAME_MARGIN_WORDS + FRAME_TILE_WORDS;
                        column++) {
                        f_CapturedFrame[wordIndex++] =
                            frame[(row * FRAME_WORDS_PER_ROW) + column + tileStart];
                    }
                }
            }
        }

        vertex = f_TileGridVertices[0];
        nextVertex = f_TileGridVertices[1];
        for (tileRow = 0; tileRow != TILE_GRID_ROW_COUNT; tileRow++) {
            for (tileColumn = 0; tileColumn != TILE_GRID_COLUMN_COUNT - 1;
                tileColumn++, vertex++, nextVertex++) {
                vertex->v.ob[AXIS_X] = TileGridColumnX(tileColumn);
                vertex->v.ob[AXIS_Y] = TileGridRowY(tileRow);
                vertex->v.ob[AXIS_Z] = 0;
                nextVertex->v.ob[AXIS_X] = TileGridColumnX(tileColumn);
                nextVertex->v.ob[AXIS_Y] = TileGridRowY(tileRow);
                nextVertex->v.ob[AXIS_Z] = 0;
            }

            // The last column is narrower, ending inside the HUD's margin
            for (tileColumn = TILE_GRID_COLUMN_COUNT - 1; tileColumn != TILE_GRID_COLUMN_COUNT;
                tileColumn++, vertex++, nextVertex++) {
                vertex->v.ob[AXIS_X] = TileGridLastColumnX(tileColumn);
                vertex->v.ob[AXIS_Y] = TileGridRowY(tileRow);
                vertex->v.ob[AXIS_Z] = 0;
                nextVertex->v.ob[AXIS_X] = TileGridLastColumnX(tileColumn);
                nextVertex->v.ob[AXIS_Y] = TileGridRowY(tileRow);
                nextVertex->v.ob[AXIS_Z] = 0;
            }
        }
    }
    else if (f_HudAnimation == HUD_ANIMATION_FADE_OUT ||
        f_HudAnimation == HUD_ANIMATION_FAST_FADE_OUT ||
        f_HudAnimation == HUD_ANIMATION_SLIDE_APART ||
        f_HudAnimation == HUD_ANIMATION_SPIRAL) {
        tileRow = 0;
        for (row = FRAME_MARGIN_ROWS; row != SCREEN_HEIGHT - FRAME_MARGIN_ROWS; row++) {
            for (column = FRAME_MARGIN_WORDS; column != FRAME_WORDS_PER_ROW - FRAME_MARGIN_WORDS;
                column++, tileRow++) {
                f_CapturedFrame[tileRow] = frame[(row * FRAME_WORDS_PER_ROW) + column];
            }
        }
    }
    else if (f_HudAnimation == HUD_ANIMATION_SLICE_CAPTURE) {
        if (g_CapturedSliceCount < FRAME_SLICE_COUNT) {
            for (row = FRAME_SLICE_TOP, tileColumn = 0; row != FRAME_SLICE_BOTTOM; row++) {
                for (column = FRAME_SLICE_LEFT_WORD; column != FRAME_SLICE_RIGHT_WORD;
                    column++, tileColumn++) {
                    f_CapturedFrame[(g_CapturedSliceCount * FRAME_SLICE_WORD_COUNT) + tileColumn] =
                        frame[(row * FRAME_WORDS_PER_ROW) + column];
                }
            }

            g_CapturedSliceCount++;
        }
    }
    else if (f_HudAnimation == HUD_ANIMATION_MOTION_BLUR) {
    }
    else if (f_HudAnimation == HUD_ANIMATION_NEGATIVE) {
        tileRow = 0;
        for (row = FRAME_MARGIN_ROWS; row != SCREEN_HEIGHT - FRAME_MARGIN_ROWS; row++) {
            for (column = FRAME_MARGIN_WORDS; column != FRAME_WORDS_PER_ROW - FRAME_MARGIN_WORDS;
                column++, tileRow++) {
                f_CapturedFrame[tileRow] = ~frame[(row * FRAME_WORDS_PER_ROW) + column];
            }
        }
    }

    f_HudAnimationFrame = 1;
}

// Draws the slice animation from the captured frame, a strip at a time; each
// strip syncs before its rectangle and after it.
void DrawSliceAnimation(void) {
    s32 frame;
    s32 sliceCount;
    s32 slice;
    s32 strip;
    s32 column;
    s32 wordIndex;
    s32 row;

    if (f_HudAnimationFramesLeft <= 0) {
        return;
    }

    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTexturePersp(g_DisplayListHead++, G_TP_NONE);
    gDPSetCombineMode(g_DisplayListHead++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
    gDPPipeSync(g_DisplayListHead++);
    column = 0;
    row = 0;
    wordIndex = 0;
    frame = SLICE_ANIMATION_FRAME_COUNT - f_HudAnimationFramesLeft;
    sliceCount = (frame / FRAMES_PER_SLICE) + 1;
    if (frame % FRAMES_PER_SLICE == 0) {
        PlaySound(SOUND_SLICE, SLICE_SOUND_CHANNEL, SOUND_FULL_SCALE);
    }

    for (slice = 0; slice != sliceCount; slice++) {
        s32 left = column * SLICE_WIDTH;
        s32 top = row * SLICE_HEIGHT;

        for (strip = 0; strip != SLICE_STRIP_COUNT; strip++) {
            s32 y = top + strip * SLICE_STRIP_HEIGHT;
            s32 quarterRight = QuarterPixels(left + SLICE_WIDTH);

            gDPLoadTextureBlock(
                g_DisplayListHead++,
                TextureAddress(&f_CapturedFrame[wordIndex]),
                G_IM_FMT_RGBA,
                G_IM_SIZ_16b,
                SLICE_WIDTH,
                SLICE_STRIP_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(g_DisplayListHead++);
            gSPTextureRectangle(
                g_DisplayListHead++,
                QuarterPixels(left),
                QuarterPixels(y),
                quarterRight,
                QuarterPixels(y + SLICE_STRIP_HEIGHT),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            wordIndex += SLICE_STRIP_WORD_COUNT;
            gDPPipeSync(g_DisplayListHead++);
        }

        column++;
        if (column == SLICE_COLUMN_COUNT) {
            column = 0;
            row++;
        }
    }

    f_HudAnimationFramesLeft--;
}

// Draws the animation that StartHudAnimation started over the frame
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

// Lets the HUD draw over the whole screen again
void ResetHudScissor(void) {
    g_HudScissor.left = 0;
    g_HudScissor.right = SCREEN_WIDTH;
    g_HudScissor.bottom = SCREEN_HEIGHT;
    g_HudScissor.top = 0;
    gDPPipeSync(g_DisplayListHead++);
    gDPSetScissor(
        g_DisplayListHead++,
        G_SC_NON_INTERLACE,
        g_HudScissor.left,
        g_HudScissor.top,
        g_HudScissor.right,
        g_HudScissor.bottom
    );
}

// Whether the animation that StartHudAnimation started has finished
Boolean IsHudAnimationDone(void) {
    return f_HudAnimationFramesLeft <= 0;
}
