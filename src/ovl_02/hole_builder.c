#include "hole_builder.h"

#include <ultra64.h>

#include "billboard.h"
#include "course.h"
#include "debug_text.h"
#include "game_flags.h"
#include "game_state.h"
#include "hole_conditions.h"
#include "hole_loader.h"
#include "hole_setup.h"
#include "hole_triangles.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "play.h"
#include "player.h"
#include "scene.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/host_file.h"
#include "system/memory_address.h"
#include "terrain.h"
#include "text/text.h"
#include "work_area.h"

#define TILE_ORIGIN_SHIFT 9
#define TILE_TEXTURE_ORIGIN 0x100
#define MINI_GOLF_TEXTURE_SCALE_X 4
#define MINI_GOLF_TEXTURE_SCALE_Z 8
#define TILE_SPAN (1 << TILE_ORIGIN_SHIFT)
#define HALF_TILE_SPAN (TILE_SPAN / 2)
#define QuarterTileSpans(count) ((count) * (TILE_SPAN / 4))
#define GRID_ROW_VERTEX_COUNT ((GROUND_TILE_COLUMN_COUNT * 2) + 1)
#define GRID_VERTEX_COUNT (GRID_ROW_VERTEX_COUNT * ((GROUND_TILE_ROW_COUNT * 2) + 1))
#define GRID_TEXTURE_SCALE 4
#define GRID_TEXTURE_BORDER TexelCoordinate(1)
#define VERTEX_FLAG_X_ENCODING 0x3
#define VERTEX_FLAG_Y_ENCODING 0xC
#define VERTEX_FLAG_Y_ENCODING_SHIFT 2
#define VERTEX_FLAG_Z_ENCODING 0x30
#define VERTEX_FLAG_Z_ENCODING_SHIFT 4
#define VERTEX_FLAG_SAME_X 0x40
#define VERTEX_FLAG_SAME_Z 0x80

enum VertexEncoding {
    VERTEX_ENCODING_TWO_BYTES,
    VERTEX_ENCODING_BYTE,
    VERTEX_ENCODING_FINE_BYTE,
    VERTEX_ENCODING_COARSE_BYTE
};

#define VERTEX_FINE_HORIZONTAL_SHIFT 5
#define VERTEX_COARSE_HORIZONTAL_SHIFT 6
#define VERTEX_FINE_HEIGHT_SHIFT 1
#define VERTEX_COARSE_HEIGHT_SHIFT 2
#define VERTEX_LIST_END 9
#define CLEARED_BILLBOARD_WIDTH 64
#define CLEARED_BILLBOARD_HEIGHT 256
#define BUILD_BUFFER_START ((u8 *)0x80315000)

enum HoleAssetKind {
    HOLE_ASSET_VERTICES = 1,
    HOLE_ASSET_MIDPOINTS,
    HOLE_ASSET_TREES,
    HOLE_ASSET_TRIANGLES,
    HOLE_ASSET_ATTRIBUTES
};

typedef s32 HoleAssetKind;
typedef s8 HoleAssetKind8;

#define ASSET_TITLE_HOLE_VERTICES 0x3F0
#define ASSET_TITLE_HOLE_MIDPOINTS 0x3F1
#define ASSET_TITLE_HOLE_TREES 0x3F2
#define ASSET_TITLE_HOLE_SETUP 0x3F3
#define ASSET_TITLE_HOLE_POINTS 0x3F4
#define ASSET_TITLE_HOLE_TRIANGLES 0x3F5
#define ASSET_TITLE_HOLE_ATTRIBUTES 0x3F6
#define LAST_MINI_GOLF_PAINT_LOGIC COURSE_PEACHS_CASTLE_THIRD

#define TERRAIN_CODE_BLOCK_WORD_COUNT (TERRAIN_CODE_BLOCK_SIZE / 4)
#define HOST_PATH_SIZE 0x80
#define HOLE_VERTICES_FILE_SIZE 0x2520
#define HOLE_MIDPOINTS_FILE_SIZE 0x8000
#define HOLE_TREES_FILE_SIZE 0x800
#define HOLE_SETUP_FILE_SIZE sizeof(HoleSetup)
#define HOLE_POINTS_FILE_SIZE 0x120
#define HOLE_TRIANGLES_FILE_SIZE 0x3C00
#define ATTRIBUTE_BUFFER_SIZE 0x20000
#define HOLE_FILE_BUFFER ((u8 *)0x8030F800)

#define TEXEL_RED_MASK 0xF800
#define TEXEL_GREEN_MASK 0x7C0
#define TEXEL_BLUE_MASK 0x3E
#define TEXEL_ALPHA_MASK 1
#define TILE_BORDER_BEFORE 1
#define TERRAIN_BORDER_DISTANCE 2
#define GREEN_CHECK_SIZE 2
#define FAIRWAY_STRIPE_WIDTH 4
#define FAIRWAY_STRIPE_OFFSET (FAIRWAY_STRIPE_WIDTH / 2)
#define MINI_GOLF_CHECK_WIDTH 4
#define MINI_GOLF_CHECK_DEPTH 8
#define IsGreenEdge(code, nearestCode, nearestDistance) \
    ((code) == TERRAIN_CODE_GREEN && \
        (code) != (nearestCode) && \
        (nearestDistance) < TERRAIN_BORDER_DISTANCE)

typedef s32 (*PaintRoutine)(
    TerrainCode code,
    TerrainCode nearestCode,
    s32 nearestDistance,
    s32 x,
    s32 z
);

#define PAINTED_COLOR_SHIFT 16

typedef struct PaintLogic {
    s8 searchSpans[TERRAIN_CODE_COUNT];
    PaintRoutine paint;
} PaintLogic;

#define PAINT_OFFSET_COUNT 100
#define PAINT_OFFSET_SPAN 5

#define OFFSET_SPAN_MAX GROUND_POINT_ROW_COUNT
#define LISTED_DISTANCE 0xFFFF
#define BEYOND_ALL_DISTANCES 0x10000

typedef struct GroundLightColor {
    u16 red;
    u16 green;
    u16 blue;
} GroundLightColor;

#define MAP_COLUMN_COUNT (GROUND_TILE_COLUMN_COUNT * GROUND_TILE_TEXEL_SPAN)
#define MAP_ROW_COUNT (GROUND_TILE_ROW_COUNT * GROUND_TILE_TEXEL_SPAN)
#define HEIGHT_BAND_ROW_COUNT 32
#define NO_HEIGHT (-0x8000)
#define NO_HEIGHT_PAIR 0x80008000
#define HEIGHT_ROW_SPAN_SHIFT 4
#define HEIGHT_ROW_SPAN (1 << HEIGHT_ROW_SPAN_SHIFT)
#define HEIGHT_FRACTION_BITS 12
#define HEIGHT_ROW_STEP_SCALE 65536.0f
#define TEXEL_HEIGHT_FRACTION_BITS 16
#define TEXEL_HEIGHT_ONE 65536.0f
#define HEIGHT_STEP_SCALE_COUNT 32
#define HEIGHTS_PER_FILL 4
#define BAND_DELAY_LOOP_COUNT 0x2000

#define GROUND_TILE_AVERAGED_POINT_COUNT \
    ((GROUND_TILE_POINT_SPAN + 1) * (GROUND_TILE_POINT_SPAN + 1))
#define UNMEASURED_LOWEST_HEIGHT 0x40000000
#define UNMEASURED_HIGHEST_HEIGHT (-0x40000000)
#define GROUND_TILE_HEIGHT_RANGE_MIN 32
#define FLAT_GROUND_TILE_HEIGHT_RANGE 30
#define MAP_TILE_SHIFT 5
#define MAP_TILE_MASK (GROUND_TILE_TEXEL_SPAN - 1)

#define TILE_POINT_SPAN 5

enum TileVertexSlot {
    TILE_SLOT_ROW0_COLUMN0,
    TILE_SLOT_ROW0_COLUMN4,
    TILE_SLOT_ROW4_COLUMN0,
    TILE_SLOT_ROW4_COLUMN4,
    TILE_SLOT_ROW1_COLUMN1,
    TILE_SLOT_ROW1_COLUMN2,
    TILE_SLOT_ROW1_COLUMN3,
    TILE_SLOT_ROW2_COLUMN1,
    TILE_SLOT_ROW2_COLUMN3,
    TILE_SLOT_ROW3_COLUMN1,
    TILE_SLOT_ROW3_COLUMN2,
    TILE_SLOT_ROW3_COLUMN3,
    TILE_SLOT_ROW0_COLUMN1,
    TILE_SLOT_ROW0_COLUMN3,
    TILE_SLOT_ROW1_COLUMN0,
    TILE_SLOT_ROW3_COLUMN0,
    TILE_SLOT_ROW1_COLUMN4,
    TILE_SLOT_ROW3_COLUMN4,
    TILE_SLOT_ROW4_COLUMN1,
    TILE_SLOT_ROW4_COLUMN3,
    TILE_SLOT_ROW0_COLUMN2,
    TILE_SLOT_ROW2_COLUMN0,
    TILE_SLOT_ROW2_COLUMN2,
    TILE_SLOT_ROW2_COLUMN4,
    TILE_SLOT_ROW4_COLUMN2
};

#define GROUND_TEXTURE_LOAD_SIZE ((GROUND_TILE_TEXEL_ROW_COUNT * GROUND_TILE_TEXEL_ROW_LENGTH) - 1)
#define GROUND_TEXTURE_LINE_SIZE (((GROUND_TILE_TEXEL_ROW_LENGTH * G_IM_SIZ_16b_BYTES) + 7) >> 3)
#define GROUND_TEXTURE_DXT CALC_DXT(GROUND_TILE_TEXEL_ROW_LENGTH, G_IM_SIZ_16b_BYTES)

#define GROUND_DISPLAY_LIST_START ((Gfx *)&g_WorkArea.bytes[0x6E920])
#define GROUND_DISPLAY_LIST_END ((Gfx *)&g_WorkArea.bytes[0x77D20])
#define GROUND_VERTEX_START ((Vtx *)&g_WorkArea.bytes[0x77F20])
#define GROUND_VERTEX_END ((Vtx *)&g_WorkArea.bytes[0x82720])

typedef struct HoleAssetIds {
    u16 vertices;
    u16 midpoints;
    u16 trees;
    u16 setup;
    u16 points;
    u16 triangles;
    u16 attributes;
} HoleAssetIds;

#define HOLE_ASSET_IDS(first) \
    {(first), (first) + 1, (first) + 2, (first) + 3, (first) + 4, (first) + 5, (first) + 6}

s32 PaintDefaultCourse(
    TerrainCode code,
    TerrainCode nearestCode,
    s32 nearestDistance,
    s32 x,
    s32 z
);
s32 PaintShyGuyDesert(TerrainCode code, TerrainCode nearestCode, s32 nearestDistance, s32 x, s32 z);
s32 PaintYoshisIsland(TerrainCode code, TerrainCode nearestCode, s32 nearestDistance, s32 x, s32 z);
s32 PaintBooValley(TerrainCode code, TerrainCode nearestCode, s32 nearestDistance, s32 x, s32 z);
s32 PaintFirstMiniGolfCourse(
    TerrainCode code,
    TerrainCode nearestCode,
    s32 nearestDistance,
    s32 x,
    s32 z
);
s32 PaintSecondMiniGolfCourse(
    TerrainCode code,
    TerrainCode nearestCode,
    s32 nearestDistance,
    s32 x,
    s32 z
);
s32 PaintThirdMiniGolfCourse(
    TerrainCode code,
    TerrainCode nearestCode,
    s32 nearestDistance,
    s32 x,
    s32 z
);
s32 PaintDrivingRange(TerrainCode code, TerrainCode nearestCode, s32 nearestDistance, s32 x, s32 z);

static u32 f_PaintSeed = 0;
#define PAINT_RANDOM_SHIFT 16
#define SpeckledSoilColor(random) Rgba16(((random) & 4) + 2, 4 - (((random) >> 5) & 3), 0, 1)

static PaintLogic f_PaintLogics[] = {
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintDefaultCourse},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintDefaultCourse},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintShyGuyDesert},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintYoshisIsland},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintBooValley},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintDefaultCourse},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintFirstMiniGolfCourse},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintSecondMiniGolfCourse},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintThirdMiniGolfCourse},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintFirstMiniGolfCourse},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintSecondMiniGolfCourse},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintThirdMiniGolfCourse},
    {{0, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintDrivingRange},
};
static Boolean f_IsCodeMapCleaned = FALSE;
static HoleAssetIds f_HoleAssetIds[COURSE_RECORD_COUNT][COURSE_HOLE_COUNT] = {
    {
        HOLE_ASSET_IDS(0x0FC),
        HOLE_ASSET_IDS(0x150),
        HOLE_ASSET_IDS(0x10A),
        HOLE_ASSET_IDS(0x165),
        HOLE_ASSET_IDS(0x00E),
        HOLE_ASSET_IDS(0x11F),
        HOLE_ASSET_IDS(0x126),
        HOLE_ASSET_IDS(0x007),
        HOLE_ASSET_IDS(0x134),
        HOLE_ASSET_IDS(0x13B),
        HOLE_ASSET_IDS(0x142),
        HOLE_ASSET_IDS(0x149),
        HOLE_ASSET_IDS(0x103),
        HOLE_ASSET_IDS(0x157),
        HOLE_ASSET_IDS(0x15E),
        HOLE_ASSET_IDS(0x111),
        HOLE_ASSET_IDS(0x16C),
        HOLE_ASSET_IDS(0x173),
    },
    {
        HOLE_ASSET_IDS(0x000),
        HOLE_ASSET_IDS(0x12D),
        HOLE_ASSET_IDS(0x118),
        HOLE_ASSET_IDS(0x015),
        HOLE_ASSET_IDS(0x01C),
        HOLE_ASSET_IDS(0x023),
        HOLE_ASSET_IDS(0x02A),
        HOLE_ASSET_IDS(0x031),
        HOLE_ASSET_IDS(0x038),
        HOLE_ASSET_IDS(0x03F),
        HOLE_ASSET_IDS(0x046),
        HOLE_ASSET_IDS(0x04D),
        HOLE_ASSET_IDS(0x054),
        HOLE_ASSET_IDS(0x05B),
        HOLE_ASSET_IDS(0x062),
        HOLE_ASSET_IDS(0x069),
        HOLE_ASSET_IDS(0x070),
        HOLE_ASSET_IDS(0x077),
    },
    {
        HOLE_ASSET_IDS(0x196),
        HOLE_ASSET_IDS(0x1B2),
        HOLE_ASSET_IDS(0x1CE),
        HOLE_ASSET_IDS(0x181),
        HOLE_ASSET_IDS(0x1AB),
        HOLE_ASSET_IDS(0x1A4),
        HOLE_ASSET_IDS(0x1B9),
        HOLE_ASSET_IDS(0x1F1),
        HOLE_ASSET_IDS(0x1E3),
        HOLE_ASSET_IDS(0x19D),
        HOLE_ASSET_IDS(0x188),
        HOLE_ASSET_IDS(0x1C0),
        HOLE_ASSET_IDS(0x1EA),
        HOLE_ASSET_IDS(0x1D5),
        HOLE_ASSET_IDS(0x1C7),
        HOLE_ASSET_IDS(0x18F),
        HOLE_ASSET_IDS(0x1DC),
        HOLE_ASSET_IDS(0x17A),
    },
    {
        HOLE_ASSET_IDS(0x1F8),
        HOLE_ASSET_IDS(0x1FF),
        HOLE_ASSET_IDS(0x206),
        HOLE_ASSET_IDS(0x20D),
        HOLE_ASSET_IDS(0x214),
        HOLE_ASSET_IDS(0x21B),
        HOLE_ASSET_IDS(0x222),
        HOLE_ASSET_IDS(0x229),
        HOLE_ASSET_IDS(0x230),
        HOLE_ASSET_IDS(0x237),
        HOLE_ASSET_IDS(0x23E),
        HOLE_ASSET_IDS(0x245),
        HOLE_ASSET_IDS(0x24C),
        HOLE_ASSET_IDS(0x253),
        HOLE_ASSET_IDS(0x25A),
        HOLE_ASSET_IDS(0x261),
        HOLE_ASSET_IDS(0x268),
        HOLE_ASSET_IDS(0x26F),
    },
    {
        HOLE_ASSET_IDS(0x07E),
        HOLE_ASSET_IDS(0x085),
        HOLE_ASSET_IDS(0x08C),
        HOLE_ASSET_IDS(0x093),
        HOLE_ASSET_IDS(0x09A),
        HOLE_ASSET_IDS(0x0A1),
        HOLE_ASSET_IDS(0x0A8),
        HOLE_ASSET_IDS(0x0AF),
        HOLE_ASSET_IDS(0x0B6),
        HOLE_ASSET_IDS(0x0BD),
        HOLE_ASSET_IDS(0x0C4),
        HOLE_ASSET_IDS(0x0CB),
        HOLE_ASSET_IDS(0x0D2),
        HOLE_ASSET_IDS(0x0D9),
        HOLE_ASSET_IDS(0x0E0),
        HOLE_ASSET_IDS(0x0E7),
        HOLE_ASSET_IDS(0x0EE),
        HOLE_ASSET_IDS(0x0F5),
    },
    {
        HOLE_ASSET_IDS(0x284),
        HOLE_ASSET_IDS(0x2E6),
        HOLE_ASSET_IDS(0x2C3),
        HOLE_ASSET_IDS(0x28B),
        HOLE_ASSET_IDS(0x2A7),
        HOLE_ASSET_IDS(0x2CA),
        HOLE_ASSET_IDS(0x299),
        HOLE_ASSET_IDS(0x2B5),
        HOLE_ASSET_IDS(0x27D),
        HOLE_ASSET_IDS(0x2DF),
        HOLE_ASSET_IDS(0x276),
        HOLE_ASSET_IDS(0x2BC),
        HOLE_ASSET_IDS(0x2A0),
        HOLE_ASSET_IDS(0x292),
        HOLE_ASSET_IDS(0x2AE),
        HOLE_ASSET_IDS(0x2D8),
        HOLE_ASSET_IDS(0x2D1),
        HOLE_ASSET_IDS(0x2ED),
    },
    {
        HOLE_ASSET_IDS(0x2F4),
        HOLE_ASSET_IDS(0x2FB),
        HOLE_ASSET_IDS(0x302),
        HOLE_ASSET_IDS(0x309),
        HOLE_ASSET_IDS(0x310),
        HOLE_ASSET_IDS(0x317),
        HOLE_ASSET_IDS(0x31E),
        HOLE_ASSET_IDS(0x325),
        HOLE_ASSET_IDS(0x32C),
        HOLE_ASSET_IDS(0x333),
        HOLE_ASSET_IDS(0x33A),
        HOLE_ASSET_IDS(0x341),
        HOLE_ASSET_IDS(0x348),
        HOLE_ASSET_IDS(0x34F),
        HOLE_ASSET_IDS(0x356),
        HOLE_ASSET_IDS(0x35D),
        HOLE_ASSET_IDS(0x364),
        HOLE_ASSET_IDS(0x36B),
    },
    {
        HOLE_ASSET_IDS(0x372),
        HOLE_ASSET_IDS(0x379),
        HOLE_ASSET_IDS(0x380),
        HOLE_ASSET_IDS(0x387),
        HOLE_ASSET_IDS(0x38E),
        HOLE_ASSET_IDS(0x395),
        HOLE_ASSET_IDS(0x39C),
        HOLE_ASSET_IDS(0x3A3),
        HOLE_ASSET_IDS(0x3AA),
        HOLE_ASSET_IDS(0x3B1),
        HOLE_ASSET_IDS(0x3B8),
        HOLE_ASSET_IDS(0x3BF),
        HOLE_ASSET_IDS(0x3C6),
        HOLE_ASSET_IDS(0x3CD),
        HOLE_ASSET_IDS(0x3D4),
        HOLE_ASSET_IDS(0x3DB),
        HOLE_ASSET_IDS(0x3E2),
        HOLE_ASSET_IDS(0x3E9),
    },
    {
        HOLE_ASSET_IDS(0x8F5),
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
        {0},
    },
};
Vec3 g_MiniGolfScale = {1.0f, 1.0f, 1.0f};
static s32 f_HolePaintLogics[HOLE_FILE_COURSE_COUNT][COURSE_HOLE_COUNT] = {
    {1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4},
    {0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
    {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
    {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5},
    {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
    {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
static s32 f_UnusedWordAfterPaintLogics UNUSED = 0;
static s8 f_ShadowlessTextures[] = {0x14, 0x15, 0x16, -1};
static s32 f_UnusedWordsAfterShadowlessTextures[] UNUSED = {0, 0, 0};
static s8 f_TileVertexSlots[TILE_POINT_SPAN][TILE_POINT_SPAN] = {
    {
        TILE_SLOT_ROW0_COLUMN0,
        TILE_SLOT_ROW0_COLUMN1,
        TILE_SLOT_ROW0_COLUMN2,
        TILE_SLOT_ROW0_COLUMN3,
        TILE_SLOT_ROW0_COLUMN4,
    },
    {
        TILE_SLOT_ROW1_COLUMN0,
        TILE_SLOT_ROW1_COLUMN1,
        TILE_SLOT_ROW1_COLUMN2,
        TILE_SLOT_ROW1_COLUMN3,
        TILE_SLOT_ROW1_COLUMN4,
    },
    {
        TILE_SLOT_ROW2_COLUMN0,
        TILE_SLOT_ROW2_COLUMN1,
        TILE_SLOT_ROW2_COLUMN2,
        TILE_SLOT_ROW2_COLUMN3,
        TILE_SLOT_ROW2_COLUMN4,
    },
    {
        TILE_SLOT_ROW3_COLUMN0,
        TILE_SLOT_ROW3_COLUMN1,
        TILE_SLOT_ROW3_COLUMN2,
        TILE_SLOT_ROW3_COLUMN3,
        TILE_SLOT_ROW3_COLUMN4,
    },
    {
        TILE_SLOT_ROW4_COLUMN0,
        TILE_SLOT_ROW4_COLUMN1,
        TILE_SLOT_ROW4_COLUMN2,
        TILE_SLOT_ROW4_COLUMN3,
        TILE_SLOT_ROW4_COLUMN4,
    },
};
static s32 f_UnusedWordAfterTileVertexSlots UNUSED = 0;
static f32 f_HeightStepScales[HEIGHT_STEP_SCALE_COUNT] = {
    0.0f,
    TEXEL_HEIGHT_ONE / 1,
    TEXEL_HEIGHT_ONE / 2,
    TEXEL_HEIGHT_ONE / 3,
    TEXEL_HEIGHT_ONE / 4,
    TEXEL_HEIGHT_ONE / 5,
    TEXEL_HEIGHT_ONE / 6,
    TEXEL_HEIGHT_ONE / 7,
    TEXEL_HEIGHT_ONE / 8,
    TEXEL_HEIGHT_ONE / 9,
    TEXEL_HEIGHT_ONE / 10,
    TEXEL_HEIGHT_ONE / 11,
    TEXEL_HEIGHT_ONE / 12,
    TEXEL_HEIGHT_ONE / 13,
    TEXEL_HEIGHT_ONE / 14,
    TEXEL_HEIGHT_ONE / 15,
    TEXEL_HEIGHT_ONE / 16,
    TEXEL_HEIGHT_ONE / 17,
    TEXEL_HEIGHT_ONE / 18,
    TEXEL_HEIGHT_ONE / 19,
    TEXEL_HEIGHT_ONE / 20,
    TEXEL_HEIGHT_ONE / 21,
    TEXEL_HEIGHT_ONE / 22,
    TEXEL_HEIGHT_ONE / 23,
    TEXEL_HEIGHT_ONE / 24,
    TEXEL_HEIGHT_ONE / 25,
    TEXEL_HEIGHT_ONE / 26,
    TEXEL_HEIGHT_ONE / 27,
    TEXEL_HEIGHT_ONE / 28,
    TEXEL_HEIGHT_ONE / 29,
    TEXEL_HEIGHT_ONE / 30,
    TEXEL_HEIGHT_ONE / 31,
};

void CleanCodeMap(void);
void PaintGroundTiles(void);
void ClearHolePoints(void);
void ShadeGroundTiles(void);
void CastBillboardShadows(void);
void MoveRingsToEnd(void);

static CourseIndex f_PaintLogic ORDERED_BSS = 0;

static u8 f_UnusedBytesBeforeReadCursor[12] ORDERED_BSS UNUSED = {0};

static u8 *f_ReadCursor ORDERED_BSS = NULL;
static AssetFile f_HoleAsset ORDERED_BSS __attribute__((aligned(8))) = {0};

static u8 f_UnusedBytesAfterHoleAsset[8] ORDERED_BSS UNUSED = {0};
u8 g_LoaderThreadStack[LOADER_THREAD_STACK_SIZE] ORDERED_BSS __attribute__((aligned(8))) = {0};

static u8 *f_BuildBuffer;
static u8 *f_BuildCursor;

static inline void ReadFileBytes(void *destination, s32 size) {
    u8 *byte = destination;
    u8 *source = f_ReadCursor;
    u8 *end = f_ReadCursor + size;

    while (source != end) {
        *byte++ = *source++;
    }

    f_ReadCursor = end;
}

void SetPaintSeed(u32 seed) {
    f_PaintSeed = seed;
}

s32 ListOffsetsByDistance(s32 span, GridOffset *offsets, s32 countMax) {
    f32 squares[OFFSET_SPAN_MAX + 1];
    u16 distances[span * span * 2];
    s32 row;
    s32 column;
    s32 listedCount;
    s32 nearestIndex;
    s32 nearestDistance;
    s32 nearestRow;
    s32 nearestColumn;
    s32 mirroredRow;

    squares[0] = 0.0f;
    row = 0;
    while (row < span) {
        IN_OWN_BLOCK(
            row++; squares[row] = (f32)(row * row) * (OFFSET_DISTANCE_SCALE * OFFSET_DISTANCE_SCALE)
        );
    }

    for (row = 0; row < span; row++) {
        for (column = 0; column < span; column++) {
            distances[(row * span) + column] = sqrtf(squares[column] + squares[row]);
        }
    }

    for (listedCount = 0; listedCount < countMax;) {
        nearestDistance = BEYOND_ALL_DISTANCES;
        nearestIndex = 0;
        for (column = 0; column < span * span; column++) {
            if (distances[column] < nearestDistance) {
                nearestIndex = column;
                nearestDistance = distances[column];
            }
        }

        if (nearestDistance >= span * OFFSET_DISTANCE_SCALE) {
            break;
        }

        nearestRow = nearestIndex / span;
        nearestColumn = nearestIndex % span;
        distances[nearestIndex] = LISTED_DISTANCE;
        offsets[listedCount].distance = nearestDistance;
        offsets[listedCount].column = nearestColumn;
        offsets[listedCount].row = nearestRow;
        listedCount++;
        if (listedCount >= countMax) {
            break;
        }

        if (nearestColumn != 0) {
            offsets[listedCount].column = -nearestColumn;
            offsets[listedCount].row = nearestRow;
            offsets[listedCount].distance = nearestDistance;
            listedCount++;
            if (listedCount >= countMax) {
                break;
            }
        }

        if (nearestRow != 0) {
            mirroredRow = -nearestRow;
            offsets[listedCount].column = nearestColumn;
            offsets[listedCount].row = mirroredRow;
            offsets[listedCount].distance = nearestDistance;
            listedCount++;
            if (listedCount >= countMax) {
                break;
            }

            if (nearestColumn != 0) {
                offsets[listedCount].column = -nearestColumn;
                offsets[listedCount].row = mirroredRow;
                offsets[listedCount].distance = nearestDistance;
                listedCount++;
                if (listedCount >= countMax) {
                    break;
                }
            }
        }
    }

    return listedCount;
}

void DrawBandHeights(s16 *heights, s32 band) {
    Vtx *corners[5];
    s32 cachedSpan;
    f32 cachedReciprocal;
    s32 row;
    s32 column;
    f64 fill;

    void DrawTriangleHeights(void) {
        f32 longSlopeX;
        f32 longSlopeHeight;
        s32 spanTotal;
        s32 span;
        s32 lowerSpan;

        inline void DrawHeightRow(s32 x, s32 height, s32 toX, s32 toHeight, s32 rowZ) {
            s32 width;
            s32 rowSpanCount;
            s32 direction;
            s32 texelHeightStep;
            s32 bandRow;
            s16 *rowTexels;
            s32 texelIndex;
            s32 texelHeight;

            width = x - toX;
            if (width < 0) {
                width = -width;
            }

            rowSpanCount = (width + HEIGHT_ROW_SPAN / 2) / HEIGHT_ROW_SPAN;
            direction = (x < toX) ? 1 : -1;
            x >>= HEIGHT_ROW_SPAN_SHIFT;
            if (rowSpanCount < HEIGHT_STEP_SCALE_COUNT) {
                texelHeightStep = f_HeightStepScales[rowSpanCount] * (toHeight - height);
            }
            else {
                texelHeightStep =
                    ((toHeight - height) << TEXEL_HEIGHT_FRACTION_BITS) / rowSpanCount;
            }

            bandRow = (rowZ >> HEIGHT_ROW_SPAN_SHIFT) - band * HEIGHT_BAND_ROW_COUNT;
            height <<= TEXEL_HEIGHT_FRACTION_BITS;
            if (bandRow >= 0 && bandRow < HEIGHT_BAND_ROW_COUNT) {
                rowTexels = &heights[bandRow * MAP_COLUMN_COUNT];
                for (texelIndex = 0; texelIndex <= rowSpanCount; texelIndex++) {
                    if (x >= 0 && x < MAP_COLUMN_COUNT) {
                        texelHeight = height >> TEXEL_HEIGHT_FRACTION_BITS;
                        if (rowTexels[x] != NO_HEIGHT) {
                            texelHeight = (rowTexels[x] + texelHeight) / 2;
                        }

                        rowTexels[x] = texelHeight;
                    }

                    x += direction;
                    height += texelHeightStep;
                }
            }
        }

        spanTotal = corners[2]->v.ob[AXIS_Z] - corners[0]->v.ob[AXIS_Z];
        if (cachedSpan != spanTotal) {
            cachedSpan = spanTotal;
            cachedReciprocal = 1.0f / spanTotal;
        }

        longSlopeX = (corners[2]->v.ob[AXIS_X] - corners[0]->v.ob[AXIS_X]) * cachedReciprocal;
        longSlopeHeight = (corners[2]->v.ob[AXIS_Y] - corners[0]->v.ob[AXIS_Y]) * cachedReciprocal;
        span = corners[1]->v.ob[AXIS_Z] - corners[0]->v.ob[AXIS_Z];
        if (span != 0) {
            f32 shortSlopeX;
            f32 shortSlopeHeight;
            s32 longX;
            s32 longHeight;
            s32 shortX;
            s32 shortHeight;
            s32 longXStep;
            s32 longHeightStep;
            s32 shortXStep;
            s32 shortHeightStep;
            s32 offsetZ;

            if (cachedSpan != span) {
                cachedSpan = span;
                cachedReciprocal = 1.0f / span;
            }

            shortSlopeX = (corners[1]->v.ob[AXIS_X] - corners[0]->v.ob[AXIS_X]) * cachedReciprocal;
            shortSlopeHeight =
                (corners[1]->v.ob[AXIS_Y] - corners[0]->v.ob[AXIS_Y]) * cachedReciprocal;
            longXStep = longSlopeX * HEIGHT_ROW_STEP_SCALE;
            shortXStep = shortSlopeX * HEIGHT_ROW_STEP_SCALE;
            longHeightStep = longSlopeHeight * HEIGHT_ROW_STEP_SCALE;
            shortHeightStep = shortSlopeHeight * HEIGHT_ROW_STEP_SCALE;
            shortX = corners[0]->v.ob[AXIS_X] << HEIGHT_FRACTION_BITS;
            longX = shortX;
            shortHeight = corners[0]->v.ob[AXIS_Y] << HEIGHT_FRACTION_BITS;
            longHeight = shortHeight;
            for (offsetZ = 0; offsetZ < span + HEIGHT_ROW_SPAN / 2; offsetZ += HEIGHT_ROW_SPAN) {
                DrawHeightRow(
                    longX >> HEIGHT_FRACTION_BITS,
                    longHeight >> HEIGHT_FRACTION_BITS,
                    shortX >> HEIGHT_FRACTION_BITS,
                    shortHeight >> HEIGHT_FRACTION_BITS,
                    offsetZ + corners[0]->v.ob[AXIS_Z]
                );
                longX += longXStep;
                shortX += shortXStep;
                longHeight += longHeightStep;
                shortHeight += shortHeightStep;
            }
        }

        lowerSpan = corners[2]->v.ob[AXIS_Z] - corners[1]->v.ob[AXIS_Z];
        if (lowerSpan != 0) {
            f32 shortSlopeX;
            f32 shortSlopeHeight;
            s32 longX;
            s32 longHeight;
            s32 shortX;
            s32 shortHeight;
            s32 longXStep;
            s32 longHeightStep;
            s32 shortXStep;
            s32 shortHeightStep;
            s32 offsetZ;

            if (cachedSpan != lowerSpan) {
                cachedSpan = lowerSpan;
                cachedReciprocal = 1.0f / lowerSpan;
            }

            shortSlopeX = (corners[2]->v.ob[AXIS_X] - corners[1]->v.ob[AXIS_X]) * cachedReciprocal;
            shortSlopeHeight =
                (corners[2]->v.ob[AXIS_Y] - corners[1]->v.ob[AXIS_Y]) * cachedReciprocal;
            longX = ((s32)(longSlopeX * span) + corners[0]->v.ob[AXIS_X]) << HEIGHT_FRACTION_BITS;
            longHeight = ((s32)(longSlopeHeight * span) + corners[0]->v.ob[AXIS_Y])
                << HEIGHT_FRACTION_BITS;
            longXStep = longSlopeX * HEIGHT_ROW_STEP_SCALE;
            shortXStep = shortSlopeX * HEIGHT_ROW_STEP_SCALE;
            longHeightStep = longSlopeHeight * HEIGHT_ROW_STEP_SCALE;
            shortHeightStep = shortSlopeHeight * HEIGHT_ROW_STEP_SCALE;
            shortX = corners[1]->v.ob[AXIS_X] << HEIGHT_FRACTION_BITS;
            shortHeight = corners[1]->v.ob[AXIS_Y] << HEIGHT_FRACTION_BITS;
            for (offsetZ = 0; offsetZ < lowerSpan + HEIGHT_ROW_SPAN / 2;
                offsetZ += HEIGHT_ROW_SPAN) {
                DrawHeightRow(
                    longX >> HEIGHT_FRACTION_BITS,
                    longHeight >> HEIGHT_FRACTION_BITS,
                    shortX >> HEIGHT_FRACTION_BITS,
                    shortHeight >> HEIGHT_FRACTION_BITS,
                    offsetZ + corners[1]->v.ob[AXIS_Z]
                );
                longX += longXStep;
                shortX += shortXStep;
                longHeight += longHeightStep;
                shortHeight += shortHeightStep;
            }
        }
    }

    cachedSpan = -1;
    if (band < GROUND_TILE_ROW_COUNT) {
        ((u32 *)heights)[0] = NO_HEIGHT_PAIR;
        ((u32 *)heights)[1] = NO_HEIGHT_PAIR;
        fill = *(f64 *)heights;
        for (column = 1; column < HEIGHT_BAND_ROW_COUNT * MAP_COLUMN_COUNT / HEIGHTS_PER_FILL;
            column++) {
            ((f64 *)heights)[column] = fill;
        }

        for (row = band * GROUND_TILE_POINT_SPAN; row < (band + 1) * GROUND_TILE_POINT_SPAN;
            row++) {
            for (column = 0; column < GROUND_POINT_COLUMN_COUNT - 1; column++) {
                corners[0] = GetGroundPoint(column, row);
                corners[1] = GetGroundPoint(column + 1, row);
                corners[2] = GetGroundPoint(column, row + 1);
                corners[3] = GetGroundPoint(column + 1, row + 1);
                if (corners[1]->v.ob[AXIS_Z] < corners[0]->v.ob[AXIS_Z]) {
                    corners[4] = corners[0];
                    corners[0] = corners[1];
                    corners[1] = corners[4];
                }

                if (corners[2]->v.ob[AXIS_Z] < corners[1]->v.ob[AXIS_Z]) {
                    corners[4] = corners[1];
                    corners[1] = corners[2];
                    corners[2] = corners[4];
                }

                if (corners[1]->v.ob[AXIS_Z] < corners[0]->v.ob[AXIS_Z]) {
                    corners[4] = corners[0];
                    corners[0] = corners[1];
                    corners[1] = corners[4];
                }

                DrawTriangleHeights();
                corners[0] = GetGroundPoint(column, row);
                corners[1] = GetGroundPoint(column + 1, row);
                corners[2] = GetGroundPoint(column, row + 1);
                corners[3] = GetGroundPoint(column + 1, row + 1);

                corners[0] = corners[3];
                if (corners[1]->v.ob[AXIS_Z] < corners[0]->v.ob[AXIS_Z]) {
                    corners[4] = corners[0];
                    corners[0] = corners[1];
                    corners[1] = corners[4];
                }

                if (corners[2]->v.ob[AXIS_Z] < corners[1]->v.ob[AXIS_Z]) {
                    corners[4] = corners[1];
                    corners[1] = corners[2];
                    corners[2] = corners[4];
                }

                if (corners[1]->v.ob[AXIS_Z] < corners[0]->v.ob[AXIS_Z]) {
                    corners[4] = corners[0];
                    corners[0] = corners[1];
                    corners[1] = corners[4];
                }

                DrawTriangleHeights();
            }
        }

        for (column = 0; column < BAND_DELAY_LOOP_COUNT; column++) {
        }
    }
}

void ResetTerrainCodeCells(void) {
    s32 row;
    s32 column;
    s32 cellOffset;

    g_TerrainCodeBlockCount = 0;
    for (row = 0; row < TERRAIN_CODE_CELL_ROW_COUNT; row++) {
        for (column = TERRAIN_CODE_CELL_COLUMN_COUNT - 1; column >= 0; column--) {
            cellOffset = (row * TERRAIN_CODE_CELL_ROW_SIZE) + (column * TERRAIN_CODE_CELL_SIZE);
            *(s16 *)AddressPast(g_TerrainCodeCells, cellOffset) = -1;
        }
    }
}

s32 PaintDefaultCourse(
    TerrainCode code,
    TerrainCode nearestCode,
    s32 nearestDistance,
    s32 x,
    s32 z
) {
    u32 color;
    TerrainCode paintedCode;
    s32 random;
    s32 fairwayRandom;
    s32 green;
    u32 red;

    switch (code) {
        case TERRAIN_CODE_DEEP_ROUGH:
            color = Rgba16(8, 18, 0, 1);
            paintedCode = TERRAIN_CODE_DEEP_ROUGH;
            break;
        case TERRAIN_CODE_ROCK:
            color = Rgba16(9, 6, 1, 1);
            paintedCode = TERRAIN_CODE_ROCK;
            break;
        case TERRAIN_CODE_FAIRWAY:
            if (nearestDistance >= TERRAIN_BORDER_DISTANCE ||
                nearestCode == TERRAIN_CODE_GREEN ||
                nearestCode == TERRAIN_CODE_FAIRWAY) {
                f_PaintSeed = NextRandomSeed(f_PaintSeed);
                fairwayRandom = f_PaintSeed >> PAINT_RANDOM_SHIFT;
                if ((x + FAIRWAY_STRIPE_OFFSET) & FAIRWAY_STRIPE_WIDTH) {
                    color = Rgba16((fairwayRandom & 3) + 9, 19, 2, 0);
                }
                else {
                    color = Rgba16((fairwayRandom & 3) + 7, 22, 2, 0);
                }

                paintedCode = TERRAIN_CODE_FAIRWAY;
            }
            else {
                color = Rgba16(8, 24, 2, 1);
                paintedCode = TERRAIN_CODE_SEMI_ROUGH;
            }

            break;
        case TERRAIN_CODE_ROUGH:
            if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
                f_PaintSeed = NextRandomSeed(f_PaintSeed);
                red = (f_PaintSeed >> PAINT_RANDOM_SHIFT) & 7;
                green = 16 - (red >> 1);
                color = Rgba16(red, green, 0, 1);
                paintedCode = TERRAIN_CODE_ROUGH;
            }
            else {
                color = Rgba16(1, 18, 0, 1);
                paintedCode = TERRAIN_CODE_ROUGH;
            }

            break;
        case TERRAIN_CODE_BUNKER:
            color = Rgba16(28, 28, 28, 1);
            paintedCode = TERRAIN_CODE_BUNKER;
            break;
        case TERRAIN_CODE_NO_DROP_GROUND:
            paintedCode = TERRAIN_CODE_NO_DROP_GROUND;
            f_PaintSeed = NextRandomSeed(f_PaintSeed);
            random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
            color = SpeckledSoilColor(random);
            break;
        case TERRAIN_CODE_GREEN:
            if (IsGreenEdge(code, nearestCode, nearestDistance)) {
                color = Rgba16(4, 22, 3, 1);
                paintedCode = TERRAIN_CODE_GREEN_EDGE;
            }
            else if (nearestCode == TERRAIN_CODE_GREEN ||
                nearestDistance >= TERRAIN_BORDER_DISTANCE) {
                if (z & GREEN_CHECK_SIZE) {
                    color = (x & GREEN_CHECK_SIZE) ? Rgba16(5, 30, 2, 0) : Rgba16(8, 22, 0, 0);
                }
                else {
                    color = (x & GREEN_CHECK_SIZE) ? Rgba16(7, 25, 1, 0) : Rgba16(6, 28, 2, 0);
                }

                paintedCode = TERRAIN_CODE_GREEN;
            }
            else {
                color = Rgba16(4, 26, 4, 1);
                paintedCode = TERRAIN_CODE_FAIRWAY;
            }

            break;
        case TERRAIN_CODE_CART_WAY:
            color = Rgba16(15, 15, 15, 1);
            paintedCode = TERRAIN_CODE_CART_WAY;
            break;
        case TERRAIN_CODE_OUT_OF_BOUNDS:
            color = Rgba16(5, 9, 0, 1);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
        case TERRAIN_CODE_TEE_GROUND:
            color = Rgba16(4, 26, 4, 0);
            paintedCode = TERRAIN_CODE_TEE_GROUND;
            break;
        default:
            color = Rgba16(31, 31, 31, 1);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
    }

    return (color << PAINTED_COLOR_SHIFT) | paintedCode;
}

s32 PaintPaleRoughCourse(
    TerrainCode code,
    TerrainCode nearestCode,
    s32 nearestDistance,
    s32 x,
    s32 z
) {
    u32 color;
    TerrainCode paintedCode;
    s32 random;
    s32 fairwayRandom;

    random = 0;

    switch (code) {
        case TERRAIN_CODE_DEEP_ROUGH:
            color = Rgba16(8, 18, 0, 1);
            paintedCode = TERRAIN_CODE_DEEP_ROUGH;
            break;
        case TERRAIN_CODE_ROCK:
            color = Rgba16(9, 6, 1, 1);
            paintedCode = TERRAIN_CODE_ROCK;
            break;
        case TERRAIN_CODE_FAIRWAY:
            if (nearestDistance >= TERRAIN_BORDER_DISTANCE ||
                nearestCode == TERRAIN_CODE_GREEN ||
                nearestCode == TERRAIN_CODE_FAIRWAY) {
                f_PaintSeed = NextRandomSeed(f_PaintSeed);
                fairwayRandom = f_PaintSeed >> PAINT_RANDOM_SHIFT;
                if ((x + FAIRWAY_STRIPE_OFFSET) & FAIRWAY_STRIPE_WIDTH) {
                    color = Rgba16((fairwayRandom & 3) + 14, 19, 7, 0);
                }
                else {
                    color = Rgba16((fairwayRandom & 3) + 12, 22, 7, 0);
                }

                paintedCode = TERRAIN_CODE_FAIRWAY;
            }
            else {
                color = Rgba16(16, 24, 10, 1);
                paintedCode = TERRAIN_CODE_SEMI_ROUGH;
            }

            break;
        case TERRAIN_CODE_ROUGH:
            if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
                f_PaintSeed = NextRandomSeed(f_PaintSeed);
                color = Rgba16(27, 30, 27, 1);
                paintedCode = TERRAIN_CODE_ROUGH;
            }
            else {
                color = Rgba16(25, 24, 25, 1);
                paintedCode = TERRAIN_CODE_ROUGH;
            }

            break;
        case TERRAIN_CODE_BUNKER:
            color = Rgba16(28, 28, 28, 1);
            paintedCode = TERRAIN_CODE_BUNKER;
            break;
        case TERRAIN_CODE_NO_DROP_GROUND:
            if (nearestCode == TERRAIN_CODE_NO_DROP_GROUND ||
                nearestDistance >= TERRAIN_BORDER_DISTANCE) {
                paintedCode = TERRAIN_CODE_NO_DROP_GROUND;
                color = Rgba16(0, 0, 31, 0);
            }
            else {
                if (nearestDistance == 1) {
                    color = Rgba16(18, 18, 18, 1);
                }
                else {
                    color = Rgba16((random & 4) + 18, 18 - ((random >> 5) & 3), 18, 1);
                }

                paintedCode = TERRAIN_CODE_CART_WAY;
            }

            break;
        case TERRAIN_CODE_GREEN:
            if (IsGreenEdge(code, nearestCode, nearestDistance)) {
                color = Rgba16(4, 22, 3, 1);
                paintedCode = TERRAIN_CODE_GREEN_EDGE;
            }
            else if (nearestCode == TERRAIN_CODE_GREEN ||
                nearestDistance >= TERRAIN_BORDER_DISTANCE) {
                if (z & GREEN_CHECK_SIZE) {
                    color = (x & GREEN_CHECK_SIZE) ? Rgba16(5, 30, 2, 0) : Rgba16(8, 26, 0, 0);
                }
                else {
                    color = (x & GREEN_CHECK_SIZE) ? Rgba16(7, 28, 1, 0) : Rgba16(6, 28, 2, 0);
                }

                paintedCode = TERRAIN_CODE_GREEN;
            }
            else {
                color = Rgba16(4, 26, 4, 1);
                paintedCode = TERRAIN_CODE_FAIRWAY;
            }

            break;
        case TERRAIN_CODE_CART_WAY:
            color = Rgba16(15, 15, 15, 1);
            paintedCode = TERRAIN_CODE_CART_WAY;
            break;
        case TERRAIN_CODE_OUT_OF_BOUNDS:
            color = Rgba16(23, 27, 23, 1);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
        case TERRAIN_CODE_TEE_GROUND:
            color = Rgba16(4, 26, 4, 0);
            paintedCode = TERRAIN_CODE_TEE_GROUND;
            break;
        default:
            color = Rgba16(31, 31, 31, 1);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
    }

    return (color << PAINTED_COLOR_SHIFT) | paintedCode;
}

s32 PaintShyGuyDesert(
    TerrainCode code,
    TerrainCode nearestCode,
    s32 nearestDistance,
    s32 x,
    s32 z
) {
    u32 color;
    TerrainCode paintedCode;
    s32 random;
    s32 fairwayRandom;
    s32 green;
    u32 red;

    switch (code) {
        case TERRAIN_CODE_DEEP_ROUGH:
            color = Rgba16(28, 23, 9, 0);
            paintedCode = TERRAIN_CODE_WASTE_AREA;
            break;
        case TERRAIN_CODE_ROCK:
            color = Rgba16(9, 6, 1, 1);
            paintedCode = TERRAIN_CODE_ROCK;
            break;
        case TERRAIN_CODE_FAIRWAY:
            if (nearestDistance >= TERRAIN_BORDER_DISTANCE ||
                nearestCode == TERRAIN_CODE_GREEN ||
                nearestCode == TERRAIN_CODE_FAIRWAY) {
                f_PaintSeed = NextRandomSeed(f_PaintSeed);
                fairwayRandom = f_PaintSeed >> PAINT_RANDOM_SHIFT;
                if ((x + FAIRWAY_STRIPE_OFFSET) & FAIRWAY_STRIPE_WIDTH) {
                    color = Rgba16((fairwayRandom & 3) + 19, 26, 4, 0);
                }
                else {
                    color = Rgba16((fairwayRandom & 3) + 14, 23, 3, 0);
                }

                paintedCode = TERRAIN_CODE_FAIRWAY;
            }
            else {
                color = Rgba16(19, 26, 4, 1);
                paintedCode = TERRAIN_CODE_SEMI_ROUGH;
            }

            break;
        case TERRAIN_CODE_ROUGH:
            if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
                f_PaintSeed = NextRandomSeed(f_PaintSeed);
                red = ((f_PaintSeed >> PAINT_RANDOM_SHIFT) & 3) + 10;
                green = red + 3;
                color = Rgba16(red, green, 2, 1);
                paintedCode = TERRAIN_CODE_ROUGH;
            }
            else {
                color = Rgba16(1, 18, 0, 1);
                paintedCode = TERRAIN_CODE_ROUGH;
            }

            break;
        case TERRAIN_CODE_BUNKER:
            color = Rgba16(28, 28, 16, 1);
            paintedCode = TERRAIN_CODE_BUNKER;
            break;
        case TERRAIN_CODE_NO_DROP_GROUND:
            paintedCode = TERRAIN_CODE_NO_DROP_GROUND;
            f_PaintSeed = NextRandomSeed(f_PaintSeed);
            random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
            color = SpeckledSoilColor(random);
            break;
        case TERRAIN_CODE_GREEN:
            if (IsGreenEdge(code, nearestCode, nearestDistance)) {
                color = Rgba16(15, 25, 5, 1);
                paintedCode = TERRAIN_CODE_GREEN_EDGE;
            }
            else if (nearestCode == TERRAIN_CODE_GREEN ||
                nearestDistance >= TERRAIN_BORDER_DISTANCE) {
                if (z & GREEN_CHECK_SIZE) {
                    color = (x & GREEN_CHECK_SIZE) ? Rgba16(15, 25, 5, 0) : Rgba16(14, 29, 3, 0);
                }
                else {
                    color = (x & GREEN_CHECK_SIZE) ? Rgba16(17, 26, 4, 0) : Rgba16(16, 24, 3, 0);
                }

                paintedCode = TERRAIN_CODE_GREEN;
            }
            else {
                color = Rgba16(4, 26, 4, 1);
                paintedCode = TERRAIN_CODE_FAIRWAY;
            }

            break;
        case TERRAIN_CODE_CART_WAY:
            color = Rgba16(15, 15, 15, 1);
            paintedCode = TERRAIN_CODE_CART_WAY;
            break;
        case TERRAIN_CODE_OUT_OF_BOUNDS:
            color = Rgba16(26, 26, 19, 0);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
        case TERRAIN_CODE_TEE_GROUND:
            color = Rgba16(15, 25, 5, 0);
            paintedCode = TERRAIN_CODE_TEE_GROUND;
            break;
        default:
            color = Rgba16(31, 31, 31, 1);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
    }

    return (color << PAINTED_COLOR_SHIFT) | paintedCode;
}

s32 PaintBooValley(TerrainCode code, TerrainCode nearestCode, s32 nearestDistance, s32 x, s32 z) {
    u32 color;
    TerrainCode paintedCode;
    s32 random;
    s32 fairwayRandom;
    s32 green;
    u32 red;

    switch (code) {
        case TERRAIN_CODE_DEEP_ROUGH:
            color = Rgba16(8, 18, 0, 1);
            paintedCode = TERRAIN_CODE_DEEP_ROUGH;
            break;
        case TERRAIN_CODE_ROCK:
            color = Rgba16(9, 6, 1, 1);
            paintedCode = TERRAIN_CODE_ROCK;
            break;
        case TERRAIN_CODE_FAIRWAY:
            if (nearestDistance >= TERRAIN_BORDER_DISTANCE ||
                nearestCode == TERRAIN_CODE_GREEN ||
                nearestCode == TERRAIN_CODE_FAIRWAY) {
                f_PaintSeed = NextRandomSeed(f_PaintSeed);
                fairwayRandom = f_PaintSeed >> PAINT_RANDOM_SHIFT;
                if ((x + FAIRWAY_STRIPE_OFFSET) & FAIRWAY_STRIPE_WIDTH) {
                    color = Rgba16((fairwayRandom & 3) + 9, 19, 2, 0);
                }
                else {
                    color = Rgba16((fairwayRandom & 3) + 7, 22, 2, 0);
                }

                paintedCode = TERRAIN_CODE_FAIRWAY;
            }
            else {
                color = Rgba16(8, 24, 2, 1);
                paintedCode = TERRAIN_CODE_SEMI_ROUGH;
            }

            break;
        case TERRAIN_CODE_ROUGH:
            if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
                f_PaintSeed = NextRandomSeed(f_PaintSeed);
                red = (f_PaintSeed >> PAINT_RANDOM_SHIFT) & 7;
                green = 16 - (red >> 1);
                color = Rgba16(red, green, 0, 1);
                paintedCode = TERRAIN_CODE_ROUGH;
            }
            else {
                color = Rgba16(1, 18, 0, 1);
                paintedCode = TERRAIN_CODE_ROUGH;
            }

            break;
        case TERRAIN_CODE_BUNKER:
            color = Rgba16(28, 28, 16, 1);
            paintedCode = TERRAIN_CODE_BUNKER;
            break;
        case TERRAIN_CODE_NO_DROP_GROUND:
            paintedCode = TERRAIN_CODE_NO_DROP_GROUND;
            f_PaintSeed = NextRandomSeed(f_PaintSeed);
            random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
            color = SpeckledSoilColor(random);
            break;
        case TERRAIN_CODE_GREEN:
            if (IsGreenEdge(code, nearestCode, nearestDistance)) {
                color = Rgba16(4, 22, 3, 1);
                paintedCode = TERRAIN_CODE_GREEN_EDGE;
            }
            else if (nearestCode == TERRAIN_CODE_GREEN ||
                nearestDistance >= TERRAIN_BORDER_DISTANCE) {
                if (z & GREEN_CHECK_SIZE) {
                    color = (x & GREEN_CHECK_SIZE) ? Rgba16(5, 30, 2, 0) : Rgba16(8, 22, 0, 0);
                }
                else {
                    color = (x & GREEN_CHECK_SIZE) ? Rgba16(7, 25, 1, 0) : Rgba16(6, 28, 2, 0);
                }

                paintedCode = TERRAIN_CODE_GREEN;
            }
            else {
                color = Rgba16(4, 26, 4, 1);
                paintedCode = TERRAIN_CODE_FAIRWAY;
            }

            break;
        case TERRAIN_CODE_CART_WAY:
            color = Rgba16(15, 15, 15, 1);
            paintedCode = TERRAIN_CODE_CART_WAY;
            break;
        case TERRAIN_CODE_OUT_OF_BOUNDS:
            color = Rgba16(5, 9, 0, 0);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
        case TERRAIN_CODE_TEE_GROUND:
            color = Rgba16(4, 26, 4, 0);
            paintedCode = TERRAIN_CODE_TEE_GROUND;
            break;
        default:
            color = Rgba16(31, 31, 31, 1);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
    }

    return (color << PAINTED_COLOR_SHIFT) | paintedCode;
}

s32 PaintYoshisIsland(
    TerrainCode code,
    TerrainCode nearestCode,
    s32 nearestDistance,
    s32 x,
    s32 z
) {
    u32 color;
    TerrainCode paintedCode;
    s32 random;
    s32 fairwayRandom;
    s32 green;
    u32 red;

    switch (code) {
        case TERRAIN_CODE_DEEP_ROUGH:
            color = Rgba16(8, 18, 7, 1);
            paintedCode = TERRAIN_CODE_DEEP_ROUGH;
            break;
        case TERRAIN_CODE_ROCK:
            color = Rgba16(9, 6, 1, 1);
            paintedCode = TERRAIN_CODE_ROCK;
            break;
        case TERRAIN_CODE_FAIRWAY:
            if (nearestDistance >= TERRAIN_BORDER_DISTANCE ||
                nearestCode == TERRAIN_CODE_GREEN ||
                nearestCode == TERRAIN_CODE_FAIRWAY) {
                f_PaintSeed = NextRandomSeed(f_PaintSeed);
                fairwayRandom = f_PaintSeed >> PAINT_RANDOM_SHIFT;
                if ((x + FAIRWAY_STRIPE_OFFSET) & FAIRWAY_STRIPE_WIDTH) {
                    color = Rgba16((fairwayRandom & 3) + 7, 19, 10, 0);
                }
                else {
                    color = Rgba16((fairwayRandom & 3) + 7, 22, 8, 0);
                }

                paintedCode = TERRAIN_CODE_FAIRWAY;
            }
            else {
                color = Rgba16(8, 22, 8, 1);
                paintedCode = TERRAIN_CODE_SEMI_ROUGH;
            }

            break;
        case TERRAIN_CODE_ROUGH:
            if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
                f_PaintSeed = NextRandomSeed(f_PaintSeed);
                red = (f_PaintSeed >> PAINT_RANDOM_SHIFT) & 7;
                green = 16 - (red >> 1);
                color = Rgba16(red, green, 6, 1);
                paintedCode = TERRAIN_CODE_ROUGH;
            }
            else {
                color = Rgba16(1, 18, 0, 1);
                paintedCode = TERRAIN_CODE_ROUGH;
            }

            break;
        case TERRAIN_CODE_BUNKER:
            color = Rgba16(29, 28, 27, 1);
            paintedCode = TERRAIN_CODE_BUNKER;
            break;
        case TERRAIN_CODE_NO_DROP_GROUND:
            paintedCode = TERRAIN_CODE_NO_DROP_GROUND;
            f_PaintSeed = NextRandomSeed(f_PaintSeed);
            random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
            color = SpeckledSoilColor(random);
            break;
        case TERRAIN_CODE_GREEN:
            if (IsGreenEdge(code, nearestCode, nearestDistance)) {
                color = Rgba16(7, 16, 2, 0);
                paintedCode = TERRAIN_CODE_GREEN_EDGE;
            }
            else if (nearestCode == TERRAIN_CODE_GREEN ||
                nearestDistance >= TERRAIN_BORDER_DISTANCE) {
                if (z & GREEN_CHECK_SIZE) {
                    color = (x & GREEN_CHECK_SIZE) ? Rgba16(5, 24, 2, 0) : Rgba16(8, 18, 0, 0);
                }
                else {
                    color = (x & GREEN_CHECK_SIZE) ? Rgba16(7, 19, 1, 0) : Rgba16(6, 22, 2, 0);
                }

                paintedCode = TERRAIN_CODE_GREEN;
            }
            else {
                color = Rgba16(4, 26, 23, 1);
                paintedCode = TERRAIN_CODE_FAIRWAY;
            }

            break;
        case TERRAIN_CODE_CART_WAY:
            color = Rgba16(15, 15, 15, 1);
            paintedCode = TERRAIN_CODE_CART_WAY;
            break;
        case TERRAIN_CODE_OUT_OF_BOUNDS:
            color = Rgba16(5, 9, 4, 1);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
        case TERRAIN_CODE_TEE_GROUND:
            color = Rgba16(4, 26, 14, 0);
            paintedCode = TERRAIN_CODE_TEE_GROUND;
            break;
        default:
            color = Rgba16(31, 31, 31, 1);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
    }

    return (color << PAINTED_COLOR_SHIFT) | paintedCode;
}

s32 PaintUnlistedCourse(
    TerrainCode code,
    TerrainCode nearestCode,
    s32 nearestDistance,
    s32 x,
    s32 z
) {
    u32 color;
    TerrainCode paintedCode;
    s32 random;
    s32 fairwayRandom;
    s32 green;
    u32 red;

    switch (code) {
        case TERRAIN_CODE_DEEP_ROUGH:
            color = Rgba16(8, 18, 0, 1);
            paintedCode = TERRAIN_CODE_DEEP_ROUGH;
            break;
        case TERRAIN_CODE_ROCK:
            color = Rgba16(9, 6, 1, 1);
            paintedCode = TERRAIN_CODE_ROCK;
            break;
        case TERRAIN_CODE_FAIRWAY:
            if (nearestDistance >= TERRAIN_BORDER_DISTANCE ||
                nearestCode == TERRAIN_CODE_GREEN ||
                nearestCode == TERRAIN_CODE_FAIRWAY) {
                f_PaintSeed = NextRandomSeed(f_PaintSeed);
                fairwayRandom = f_PaintSeed >> PAINT_RANDOM_SHIFT;
                if ((x + FAIRWAY_STRIPE_OFFSET) & FAIRWAY_STRIPE_WIDTH) {
                    color = Rgba16((fairwayRandom & 3) + 9, 19, 2, 0);
                }
                else {
                    color = Rgba16((fairwayRandom & 3) + 7, 22, 2, 0);
                }

                paintedCode = TERRAIN_CODE_FAIRWAY;
            }
            else {
                color = Rgba16(8, 24, 2, 1);
                paintedCode = TERRAIN_CODE_SEMI_ROUGH;
            }

            break;
        case TERRAIN_CODE_ROUGH:
            if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
                f_PaintSeed = NextRandomSeed(f_PaintSeed);
                red = (f_PaintSeed >> PAINT_RANDOM_SHIFT) & 7;
                green = 16 - (red >> 1);
                color = Rgba16(red, green, 0, 1);
                paintedCode = TERRAIN_CODE_ROUGH;
            }
            else {
                color = Rgba16(1, 18, 0, 1);
                paintedCode = TERRAIN_CODE_ROUGH;
            }

            break;
        case TERRAIN_CODE_BUNKER:
            color = Rgba16(28, 28, 28, 1);
            paintedCode = TERRAIN_CODE_BUNKER;
            break;
        case TERRAIN_CODE_NO_DROP_GROUND:
            paintedCode = TERRAIN_CODE_NO_DROP_GROUND;
            f_PaintSeed = NextRandomSeed(f_PaintSeed);
            random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
            color = Rgba16((random & 7) + 4, 4 - ((random >> 5) & 3), 0, 0);
            break;
        case TERRAIN_CODE_GREEN:
            if (IsGreenEdge(code, nearestCode, nearestDistance)) {
                color = Rgba16(4, 22, 3, 1);
                paintedCode = TERRAIN_CODE_GREEN_EDGE;
            }
            else if (nearestCode == TERRAIN_CODE_GREEN ||
                nearestDistance >= TERRAIN_BORDER_DISTANCE) {
                if (z & GREEN_CHECK_SIZE) {
                    color = (x & GREEN_CHECK_SIZE) ? Rgba16(5, 30, 2, 0) : Rgba16(8, 22, 0, 0);
                }
                else {
                    color = (x & GREEN_CHECK_SIZE) ? Rgba16(7, 25, 1, 0) : Rgba16(6, 28, 2, 0);
                }

                paintedCode = TERRAIN_CODE_GREEN;
            }
            else {
                color = Rgba16(4, 26, 4, 1);
                paintedCode = TERRAIN_CODE_FAIRWAY;
            }

            break;
        case TERRAIN_CODE_CART_WAY:
            color = Rgba16(15, 15, 15, 1);
            paintedCode = TERRAIN_CODE_CART_WAY;
            break;
        case TERRAIN_CODE_OUT_OF_BOUNDS:
            color = Rgba16(5, 9, 0, 1);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
        case TERRAIN_CODE_TEE_GROUND:
            color = Rgba16(4, 26, 4, 0);
            paintedCode = TERRAIN_CODE_TEE_GROUND;
            break;
        default:
            color = Rgba16(31, 31, 31, 1);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
    }

    return (color << PAINTED_COLOR_SHIFT) | paintedCode;
}

s32 PaintFirstMiniGolfCourse(
    TerrainCode code,
    TerrainCode nearestCode,
    s32 nearestDistance,
    s32 x,
    s32 z
) {
    u32 color;
    TerrainCode paintedCode;

    s32 random;

    switch (code) {
        case TERRAIN_CODE_NO_DROP_GROUND:
            paintedCode = TERRAIN_CODE_NO_DROP_GROUND;
            f_PaintSeed = NextRandomSeed(f_PaintSeed);
            random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
            color = Rgba16((random & 7) + 4, 4 - ((random >> 5) & 3), 0, 0);
            break;
        case TERRAIN_CODE_GREEN:
            if (IsGreenEdge(code, nearestCode, nearestDistance)) {
                color = Rgba16(12, 20, 5, 1);
                paintedCode = TERRAIN_CODE_GREEN;
            }
            else if (nearestCode == TERRAIN_CODE_GREEN ||
                nearestDistance >= TERRAIN_BORDER_DISTANCE) {
                if (z & MINI_GOLF_CHECK_DEPTH) {
                    color =
                        (x & MINI_GOLF_CHECK_WIDTH) ? Rgba16(25, 25, 25, 0) : Rgba16(12, 20, 5, 1);
                }
                else {
                    color =
                        (x & MINI_GOLF_CHECK_WIDTH) ? Rgba16(12, 20, 5, 1) : Rgba16(25, 25, 25, 0);
                }

                paintedCode = TERRAIN_CODE_GREEN;
            }
            else {
                color = Rgba16(4, 26, 4, 1);
                paintedCode = TERRAIN_CODE_FAIRWAY;
            }

            break;
        case TERRAIN_CODE_OUT_OF_BOUNDS:
            color = Rgba16(0, 15, 6, 0);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
        default:
            color = Rgba16(31, 31, 31, 1);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
    }

    return (color << PAINTED_COLOR_SHIFT) | paintedCode;
}

s32 PaintSecondMiniGolfCourse(
    TerrainCode code,
    TerrainCode nearestCode,
    s32 nearestDistance,
    s32 x,
    s32 z
) {
    u32 color;
    TerrainCode paintedCode;

    s32 random;

    switch (code) {
        case TERRAIN_CODE_NO_DROP_GROUND:
            paintedCode = TERRAIN_CODE_NO_DROP_GROUND;
            f_PaintSeed = NextRandomSeed(f_PaintSeed);
            random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
            color = SpeckledSoilColor(random);
            break;
        case TERRAIN_CODE_GREEN:
            if (IsGreenEdge(code, nearestCode, nearestDistance)) {
                color = Rgba16(9, 17, 25, 1);
                paintedCode = TERRAIN_CODE_FAIRWAY;
            }
            else if (nearestCode == TERRAIN_CODE_GREEN ||
                nearestDistance >= TERRAIN_BORDER_DISTANCE) {
                if (z & MINI_GOLF_CHECK_DEPTH) {
                    color =
                        (x & MINI_GOLF_CHECK_WIDTH) ? Rgba16(9, 17, 25, 0) : Rgba16(23, 26, 26, 0);
                }
                else {
                    color =
                        (x & MINI_GOLF_CHECK_WIDTH) ? Rgba16(23, 26, 26, 0) : Rgba16(9, 17, 25, 0);
                }

                paintedCode = TERRAIN_CODE_FAIRWAY;
            }
            else {
                color = Rgba16(4, 26, 4, 1);
                paintedCode = TERRAIN_CODE_FAIRWAY;
            }

            break;
        case TERRAIN_CODE_OUT_OF_BOUNDS:
            color = Rgba16(0, 21, 15, 0);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
        default:
            color = Rgba16(31, 31, 31, 1);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
    }

    return (color << PAINTED_COLOR_SHIFT) | paintedCode;
}

s32 PaintThirdMiniGolfCourse(
    TerrainCode code,
    TerrainCode nearestCode,
    s32 nearestDistance,
    s32 x,
    s32 z
) {
    u32 color;
    TerrainCode paintedCode;
    s32 random;

    switch (code) {
        case TERRAIN_CODE_NO_DROP_GROUND:
            paintedCode = TERRAIN_CODE_NO_DROP_GROUND;
            f_PaintSeed = NextRandomSeed(f_PaintSeed);
            random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
            color = SpeckledSoilColor(random);
            break;
        case TERRAIN_CODE_GREEN:
            if (nearestCode != TERRAIN_CODE_GREEN && nearestDistance < TERRAIN_BORDER_DISTANCE) {
                color = Rgba16(27, 17, 16, 1);
                paintedCode = TERRAIN_CODE_CART_WAY;
            }
            else {
                if (z & MINI_GOLF_CHECK_DEPTH) {
                    color =
                        (x & MINI_GOLF_CHECK_WIDTH) ? Rgba16(27, 17, 16, 0) : Rgba16(25, 25, 25, 0);
                }
                else {
                    color =
                        (x & MINI_GOLF_CHECK_WIDTH) ? Rgba16(25, 25, 25, 0) : Rgba16(27, 17, 16, 0);
                }

                paintedCode = TERRAIN_CODE_CART_WAY;
            }

            break;
        case TERRAIN_CODE_OUT_OF_BOUNDS:
            color = Rgba16(15, 14, 0, 0);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
        default:
            color = Rgba16(31, 0, 0, 1);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
    }

    return (color << PAINTED_COLOR_SHIFT) | paintedCode;
}

s32 PaintDrivingRange(
    TerrainCode code,
    TerrainCode nearestCode,
    s32 nearestDistance,
    s32 x,
    s32 z
) {
    u32 color;
    TerrainCode paintedCode;
    s32 random;
    s32 green;
    u32 red;

    switch (code) {
        case TERRAIN_CODE_DEEP_ROUGH:
            color = Rgba16(8, 18, 0, 1);
            paintedCode = TERRAIN_CODE_DEEP_ROUGH;
            break;
        case TERRAIN_CODE_ROCK:
            color = Rgba16(9, 6, 1, 1);
            paintedCode = TERRAIN_CODE_ROCK;
            break;
        case TERRAIN_CODE_FAIRWAY:
            color = ((x + FAIRWAY_STRIPE_OFFSET) & FAIRWAY_STRIPE_WIDTH) ? Rgba16(11, 19, 2, 0) :
                                                                           Rgba16(8, 22, 2, 0);
            paintedCode = TERRAIN_CODE_FAIRWAY;
            break;
        case TERRAIN_CODE_ROUGH:
            if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
                f_PaintSeed = NextRandomSeed(f_PaintSeed);
                red = (f_PaintSeed >> PAINT_RANDOM_SHIFT) & 7;
                green = 16 - (red >> 1);
                color = Rgba16(red, green, 0, 1);
                paintedCode = TERRAIN_CODE_ROUGH;
            }
            else {
                color = Rgba16(1, 18, 0, 1);
                paintedCode = TERRAIN_CODE_ROUGH;
            }

            break;
        case TERRAIN_CODE_BUNKER:
            color = Rgba16(28, 28, 28, 1);
            paintedCode = TERRAIN_CODE_BUNKER;
            break;
        case TERRAIN_CODE_NO_DROP_GROUND:
            paintedCode = TERRAIN_CODE_NO_DROP_GROUND;
            f_PaintSeed = NextRandomSeed(f_PaintSeed);
            random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
            color = SpeckledSoilColor(random);
            break;
        case TERRAIN_CODE_GREEN:
            if (IsGreenEdge(code, nearestCode, nearestDistance)) {
                color = Rgba16(4, 22, 3, 1);
                paintedCode = TERRAIN_CODE_GREEN_EDGE;
            }
            else if (nearestCode == TERRAIN_CODE_GREEN ||
                nearestDistance >= TERRAIN_BORDER_DISTANCE) {
                if (z & GREEN_CHECK_SIZE) {
                    color = (x & GREEN_CHECK_SIZE) ? Rgba16(5, 30, 2, 0) : Rgba16(8, 22, 0, 0);
                }
                else {
                    color = (x & GREEN_CHECK_SIZE) ? Rgba16(7, 25, 1, 0) : Rgba16(6, 28, 2, 0);
                }

                paintedCode = TERRAIN_CODE_GREEN;
            }
            else {
                color = Rgba16(4, 26, 4, 1);
                paintedCode = TERRAIN_CODE_FAIRWAY;
            }

            break;
        case TERRAIN_CODE_CART_WAY:
            color = Rgba16(15, 15, 15, 1);
            paintedCode = TERRAIN_CODE_CART_WAY;
            break;
        case TERRAIN_CODE_OUT_OF_BOUNDS:
            color = Rgba16(5, 9, 0, 1);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
        case TERRAIN_CODE_TEE_GROUND:
            color = Rgba16(4, 26, 4, 0);
            paintedCode = TERRAIN_CODE_TEE_GROUND;
            break;
        default:
            color = Rgba16(31, 31, 31, 1);
            paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
    }

    return (color << PAINTED_COLOR_SHIFT) | paintedCode;
}

static inline u16 *GetMapTexel(s32 column, s32 row) {
    s32 tileOffset = ((column / GROUND_TILE_TEXEL_SPAN) +
                         ((row / GROUND_TILE_TEXEL_SPAN) * GROUND_TILE_COLUMN_COUNT)) *
        sizeof(GroundTile);
    s32 texelOffset =
        (((row & MAP_TILE_MASK) * GROUND_TILE_TEXEL_SPAN) + (column & MAP_TILE_MASK)) * sizeof(u16);

    return (u16 *)((u8 *)g_WorkArea.hole.tiles + texelOffset + tileOffset);
}

static inline u16 ReadMapTexel(s32 column, s32 row) {
    if (column < 0) {
        return 0;
    }

    if (column >= MAP_COLUMN_COUNT) {
        return 0;
    }

    if (row < 0) {
        return 0;
    }

    if (row >= MAP_ROW_COUNT) {
        return 0;
    }

    return *GetMapTexel(column, row);
}

void EncodeCodeMap(void) {
    s32 column;
    s32 row;

    f_BuildCursor = HOLE_FILE_BUFFER;
    f_BuildBuffer = f_BuildCursor;
    for (row = 0; row < MAP_ROW_COUNT; row++) {
        for (column = 0; column < MAP_COLUMN_COUNT; column++) {
            f_BuildCursor = &f_BuildBuffer[(row * MAP_COLUMN_COUNT) + column];
            *f_BuildCursor = GetColorTerrainCode(ReadMapTexel(column, row));
        }
    }
}

static inline void WriteMapTexel(s32 column, s32 row, u16 color) {
    if (column < 0) {
        return;
    }

    if (column >= MAP_COLUMN_COUNT) {
        return;
    }

    if (row < 0) {
        return;
    }

    if (row >= MAP_ROW_COUNT) {
        return;
    }

    *GetMapTexel(column, row) = color;
}

void LoadCodeMap(char *path) {
    u32 size;
    s32 tileIndex;
    u32 texelIndex;
    s32 column;
    s32 row;
    u16 color;

    f_BuildCursor = HOLE_FILE_BUFFER;
    f_BuildBuffer = f_BuildCursor;
    if (ReadHostFile(path, &size) == HOST_FILE_OK && size == MAP_COLUMN_COUNT * MAP_ROW_COUNT) {
        f_BuildCursor = f_BuildBuffer;
    }

    for (tileIndex = 0; tileIndex < GROUND_TILE_COUNT; tileIndex++) {
        for (texelIndex = 0;
            texelIndex < GROUND_TILE_TEXEL_ROW_COUNT * GROUND_TILE_TEXEL_ROW_LENGTH;
            texelIndex++) {
            g_WorkArea.hole.tiles[tileIndex].texels[0][texelIndex] = 0;
        }
    }

    for (row = 0; row < MAP_ROW_COUNT; row++) {
        for (column = 0; column < MAP_COLUMN_COUNT; column++) {
            f_BuildCursor = &f_BuildBuffer[(row * MAP_COLUMN_COUNT) + column];
            color = GetTerrainCodeColor(*f_BuildCursor);
            WriteMapTexel(column, row, color);
        }
    }
}

void LoadBackupCodeMap(void) {
    LoadCodeMap("c:/golf64/attr.tmp");
}

void LoadToolCodeMap(void) {
    LoadCodeMap("tool/golf64.att");
    f_IsCodeMapCleaned = FALSE;
}

#define CodeIndex(column, row) (((row) * MAP_COLUMN_COUNT) + (column))

#define NeighborCodeIndex(column, row, columnStep, rowStep) \
    ((((row) + (rowStep)) * MAP_COLUMN_COUNT) + (column) + (columnStep))

static inline u8 GetBuildCode(s32 index) {
    f_BuildCursor = &f_BuildBuffer[index];
    return *f_BuildCursor;
}

static inline void SetBuildCode(s32 index, u8 code) {
    f_BuildCursor = &f_BuildBuffer[index];
    *f_BuildCursor = code;
}

#define FillCodeGap(index, previous, column, row, columnStep, rowStep) \
    { \
        s32 code; \
        s32 previousAgain; \
        s32 next; \
\
        (index) = CodeIndex(column, row); \
        code = GetBuildCode(index); \
        (index) = NeighborCodeIndex(column, row, -(columnStep), -(rowStep)); \
        (previous) = GetBuildCode(index); \
        (index) = NeighborCodeIndex(column, row, -(columnStep), -(rowStep)); \
        previousAgain = GetBuildCode(index); \
        (index) = NeighborCodeIndex(column, row, columnStep, rowStep); \
        next = GetBuildCode(index); \
        if (code != (previous) && previousAgain == next) { \
            u8 fill; \
\
            (index) = NeighborCodeIndex(column, row, columnStep, rowStep); \
            fill = GetBuildCode(index); \
            (index) = CodeIndex(column, row); \
            SetBuildCode(index, fill); \
        } \
    }

void CleanCodeMap(void) {
    s32 passCount = 0;
    s32 column;
    s32 row;
    s32 index;

    f_BuildCursor = HOLE_FILE_BUFFER;
    f_BuildBuffer = f_BuildCursor;
    for (row = 0; row < MAP_ROW_COUNT; row++) {
        for (column = 0; column < MAP_COLUMN_COUNT; column++) {
            index = CodeIndex(column, row);
            f_BuildCursor = &f_BuildBuffer[index];
            *f_BuildCursor = GetColorTerrainCode(ReadMapTexel(column, row));
        }
    }

    for (; passCount != 0; passCount--) {
        for (row = 0; row < MAP_ROW_COUNT; row++) {
            for (column = 0; column < MAP_COLUMN_COUNT; column++) {
                s32 previous;

                if (column > 0 && column < MAP_COLUMN_COUNT - 1) {
                    FillCodeGap(index, previous, column, row, 1, 0);
                }

                if (row > 0 && row < MAP_ROW_COUNT - 1) {
                    FillCodeGap(index, previous, column, row, 0, 1);
                }
            }
        }
    }
}

void FinishCodeMapCleanup(void) {
}

static inline s32 GetCellOffset(u32 column, u32 row) {
    return ((column / TERRAIN_CODE_CELL_SPAN) * TERRAIN_CODE_CELL_SIZE) +
        ((row / TERRAIN_CODE_CELL_SPAN) * TERRAIN_CODE_CELL_ROW_SIZE);
}

#define CellAt(cellOffset) (*(s16 *)AddressPast(g_TerrainCodeCells, (cellOffset)))

static inline s32 GetBlockCodeOffset(u32 column, u32 row) {
    return ((column % TERRAIN_CODE_CELL_SPAN) / 2) +
        ((row % TERRAIN_CODE_CELL_SPAN) * (TERRAIN_CODE_CELL_SPAN / 2));
}

static inline s32 GetPackedCodeOffset(s32 block, u32 column, u32 row) {
    return GetBlockCodeOffset(column, row) + (block * TERRAIN_CODE_BLOCK_SIZE);
}

static inline s32 GetCellCodeOffset(s32 cellEntry, u32 column, u32 row) {
    return GetBlockCodeOffset(column, row) +
        ((cellEntry & (TERRAIN_CODE_CELL_BLOCK - 1)) * TERRAIN_CODE_BLOCK_SIZE);
}

static inline void SetPackedTerrainCode(s32 block, u32 column, u32 row, s32 code) {
    if (column & 1) {
        s32 offset;

        IN_OWN_BLOCK(offset = GetBlockCodeOffset(column, row));
        offset += block * TERRAIN_CODE_BLOCK_SIZE;
        g_WorkArea.hole.terrainCodes[offset] =
            (g_WorkArea.hole.terrainCodes[offset] & EVEN_COLUMN_CODE_MASK) | code;
    }
    else {
        s32 high = code << TERRAIN_CODE_BITS;

        g_WorkArea.hole.terrainCodes[GetPackedCodeOffset(block, column, row)] =
            (g_WorkArea.hole.terrainCodes[GetPackedCodeOffset(block, column, row)] &
                TERRAIN_CODE_MASK) |
            high;
    }
}

static inline void SetCellTerrainCode(u32 column, u32 row, s32 code) {
    s32 cellOffset = GetCellOffset(column, row);
    s32 cellEntry = CellAt(cellOffset);
    s32 block;
    u32 fill;
    s32 wordIndex;
    u32 *word;

    if (cellEntry == code) {
        return;
    }

    if (cellEntry == NO_CELL_TERRAIN_CODE) {
        CellAt(cellOffset) = code;
    }
    else if (cellEntry >= 0) {
        if (g_TerrainCodeBlockCount < TERRAIN_CODE_BLOCK_COUNT) {
            fill = (cellEntry << TERRAIN_CODE_BITS) | cellEntry;
            block = g_TerrainCodeBlockCount++;
            fill |= fill << BITS_PER_BYTE;
            fill |= fill << (BITS_PER_BYTE * 2);
            word = (u32 *)&g_WorkArea.hole.terrainCodes[block * TERRAIN_CODE_BLOCK_SIZE];
            for (wordIndex = 0; wordIndex < TERRAIN_CODE_BLOCK_WORD_COUNT; wordIndex++) {
                *word++ = fill;
            }

            CellAt(GetCellOffset(column, row)) = block | TERRAIN_CODE_CELL_BLOCK;
            SetPackedTerrainCode(block, column, row, code);
        }
    }
    else {
        if (column & 1) {
            g_WorkArea.hole.terrainCodes[GetCellCodeOffset(cellEntry, column, row)] =
                (g_WorkArea.hole.terrainCodes[GetCellCodeOffset(cellEntry, column, row)] &
                    EVEN_COLUMN_CODE_MASK) |
                code;
        }
        else {
            s32 high = code << TERRAIN_CODE_BITS;

            g_WorkArea.hole.terrainCodes[GetCellCodeOffset(cellEntry, column, row)] =
                (g_WorkArea.hole.terrainCodes[GetCellCodeOffset(cellEntry, column, row)] &
                    TERRAIN_CODE_MASK) |
                high;
        }
    }
}

void PaintGroundTiles(void) {
    GridOffset offsets[PAINT_OFFSET_COUNT];
    u32 codePaintings[TERRAIN_CODE_COUNT];
    u8 *codes;
    s8 *searchSpans;
    PaintRoutine paint = f_PaintLogics[f_PaintLogic].paint;
    u8 *nextCode;
    u16 *texel;
    s32 offsetCount;
    s32 offsetIndex;
    s32 column;
    s32 row;
    u32 neighborColumn;
    u32 neighborRow;
    s32 texelIndex;
    TerrainCode code;
    TerrainCode nearestCode;
    s32 nearestDistance;
    s32 searchDistance;
    u32 painting;

    SetPaintSeed(0);
    ResetTerrainCodeCells();
    offsetCount = ListOffsetsByDistance(PAINT_OFFSET_SPAN, offsets, PAINT_OFFSET_COUNT);
    osSyncPrintf("MakeTex..");
    codes = f_BuildBuffer;
    f_BuildCursor = codes;
    nextCode = f_BuildCursor;
    searchSpans = f_PaintLogics[f_PaintLogic].searchSpans;

    for (row = 0; row < TERRAIN_CODE_COUNT; row++) {
        codePaintings[row] = paint(row, row, 0, 0, 0);
        g_TerrainCodeColors[codePaintings[row] & TERRAIN_CODE_MASK] =
            codePaintings[row] >> PAINTED_COLOR_SHIFT;
    }

    for (row = 0; row < MAP_ROW_COUNT; row++) {
        for (column = 0; column < MAP_COLUMN_COUNT;) {
            texel = &(
                (u16 *)&g_WorkArea.hole.tiles
                    [(column / GROUND_TILE_TEXEL_SPAN) +
                        (row / GROUND_TILE_TEXEL_SPAN * GROUND_TILE_COLUMN_COUNT)]
            )[(row & MAP_TILE_MASK) * GROUND_TILE_TEXEL_SPAN];
            for (texelIndex = 0; texelIndex < GROUND_TILE_TEXEL_SPAN; texelIndex++, column++) {
                nearestCode = *nextCode++;
                code = nearestCode;
                nearestDistance = searchSpans[code];
                if (nearestDistance != 0) {
                    if (nearestDistance < 0) {
                        painting = codePaintings[code];

                        goto PAINTED;
                    }

                    searchDistance = nearestDistance * OFFSET_DISTANCE_SCALE;
                    for (offsetIndex = 1; offsetIndex < offsetCount; offsetIndex++) {
                        if (offsets[offsetIndex].distance >= searchDistance) {
                            break;
                        }

                        neighborColumn = column + offsets[offsetIndex].column;
                        neighborRow = row + offsets[offsetIndex].row;
                        if (neighborColumn < MAP_COLUMN_COUNT && neighborRow < MAP_ROW_COUNT) {
                            nearestCode = codes[(neighborRow * MAP_COLUMN_COUNT) + neighborColumn];
                            if (nearestCode != code) {
                                break;
                            }
                        }
                    }

                    nearestDistance = offsets[offsetIndex].distance / OFFSET_DISTANCE_SCALE;
                }

                painting = paint(code, nearestCode, nearestDistance, column, row);

PAINTED:

                *texel++ = painting >> PAINTED_COLOR_SHIFT;
                SetCellTerrainCode(column, row, painting & TERRAIN_CODE_MASK);
            }
        }
    }

    osSyncPrintf("Comp\n");
    f_BuildCursor = f_BuildBuffer;
}

void ToggleCodeMapCleanup(void) {
    u8 unused[0x640] UNUSED;

    if (!f_IsCodeMapCleaned) {
        EncodeCodeMap();
        f_IsCodeMapCleaned = TRUE;
        CleanCodeMap();
        PaintGroundTiles();
        FinishCodeMapCleanup();
    }
    else {
        LoadBackupCodeMap();
        f_IsCodeMapCleaned = FALSE;
    }
}

f32 MeasureGroundTiles(void) {
    s32 tileRow;
    s32 tileColumn;
    s32 tileIndex;
    s32 pointRow;
    s32 pointColumn;
    s32 xSum;
    s32 ySum;
    s32 zSum;
    s32 lowest;
    s32 highest;
    s32 heightRange;
    Vtx *point;

    tileIndex = 0;
    for (tileRow = 0; tileRow < GROUND_TILE_ROW_COUNT; tileRow++) {
        for (tileColumn = 0; tileColumn < GROUND_TILE_COLUMN_COUNT; tileColumn++) {
            xSum = 0;
            ySum = 0;
            zSum = 0;
            lowest = UNMEASURED_LOWEST_HEIGHT;
            highest = UNMEASURED_HIGHEST_HEIGHT;
            for (pointRow = 0; pointRow <= GROUND_TILE_POINT_SPAN; pointRow++) {
                for (pointColumn = 0; pointColumn <= GROUND_TILE_POINT_SPAN; pointColumn++) {
                    point = GetGroundPoint(
                        (tileColumn * GROUND_TILE_POINT_SPAN) + pointColumn,
                        (tileRow * GROUND_TILE_POINT_SPAN) + pointRow
                    );
                    xSum += point->v.ob[AXIS_X];
                    ySum += point->v.ob[AXIS_Y];
                    zSum += point->v.ob[AXIS_Z];
                    if (point->v.ob[AXIS_Y] < lowest) {
                        lowest = point->v.ob[AXIS_Y];
                    }

                    if (point->v.ob[AXIS_Y] > highest) {
                        highest = point->v.ob[AXIS_Y];
                    }
                }
            }

            heightRange = highest - lowest;
            if (heightRange != 0 && heightRange < GROUND_TILE_HEIGHT_RANGE_MIN) {
                heightRange = GROUND_TILE_HEIGHT_RANGE_MIN;
            }

            if (heightRange == 0) {
                heightRange = FLAT_GROUND_TILE_HEIGHT_RANGE;
            }

            g_GroundTileHeightRanges[tileIndex] = heightRange;
            g_WorkArea.hole.tiles[tileIndex].center[AXIS_X] =
                xSum / GROUND_TILE_AVERAGED_POINT_COUNT;
            g_WorkArea.hole.tiles[tileIndex].center[AXIS_Y] =
                ySum / GROUND_TILE_AVERAGED_POINT_COUNT;
            g_WorkArea.hole.tiles[tileIndex].center[AXIS_Z] =
                zSum / GROUND_TILE_AVERAGED_POINT_COUNT;
            tileIndex++;
        }
    }

    return tileIndex;
}

void ScaleMiniGolfHole(void) {
    Vtx_t point;
    s32 row;
    s32 column;
    s32 index;

    for (row = 0; row < GROUND_POINT_ROW_COUNT; row++) {
        for (column = 0; column < GROUND_POINT_COLUMN_COUNT; column++) {
            point = GetGroundPoint(column, row)->v;
            point.ob[AXIS_X] = point.ob[AXIS_X] * g_MiniGolfScale[AXIS_X];
            point.ob[AXIS_Y] = point.ob[AXIS_Y] * g_MiniGolfScale[AXIS_Y];
            point.ob[AXIS_Z] = point.ob[AXIS_Z] * g_MiniGolfScale[AXIS_Z];
            SetGroundPoint(column, row, &point);
        }
    }

    for (index = 0; index < GROUND_TILE_COUNT; index++) {
        g_WorkArea.hole.tiles[index].center[AXIS_X] *= g_MiniGolfScale[AXIS_X];
        g_WorkArea.hole.tiles[index].center[AXIS_Y] *= g_MiniGolfScale[AXIS_Y];
        g_WorkArea.hole.tiles[index].center[AXIS_Z] *= g_MiniGolfScale[AXIS_Z];
    }

    for (index = 0; index < BILLBOARD_COUNT; index++) {
        if (g_Billboards[index].textureIndex >= 0) {
            g_Billboards[index].x *= g_MiniGolfScale[AXIS_X];
            g_Billboards[index].z *= g_MiniGolfScale[AXIS_Z];
        }
    }

    g_HoleSetup.teePosition[AXIS_X] *= g_MiniGolfScale[AXIS_X];
    g_HoleSetup.teePosition[AXIS_Y] *= g_MiniGolfScale[AXIS_Y];
    g_HoleSetup.teePosition[AXIS_Z] *= g_MiniGolfScale[AXIS_Z];
    g_HoleSetup.cupPosition[AXIS_X] *= g_MiniGolfScale[AXIS_X];
    g_HoleSetup.cupPosition[AXIS_Y] *= g_MiniGolfScale[AXIS_Y];
    g_HoleSetup.cupPosition[AXIS_Z] *= g_MiniGolfScale[AXIS_Z];
}

CourseIndex GetCoursePaintLogic(CourseRecordIndex courseRecordIndex) {
    switch (courseRecordIndex) {
        case COURSE_RECORD_TOAD_HIGHLANDS:
            return COURSE_TOAD_HIGHLANDS;
        case COURSE_RECORD_KOOPA_PARK:
            return COURSE_KOOPA_PARK;
        case COURSE_RECORD_SHY_GUY_DESERT:
            return COURSE_SHY_GUY_DESERT;
        case COURSE_RECORD_YOSHIS_ISLAND:
            return COURSE_YOSHIS_ISLAND;
        case COURSE_RECORD_BOO_VALLEY:
            return COURSE_BOO_VALLEY;
        case COURSE_RECORD_MARIOS_STAR:
            return COURSE_MARIOS_STAR;
        case COURSE_RECORD_LUIGIS_GARDEN:
            return COURSE_LUIGIS_GARDEN_FIRST;
        case COURSE_RECORD_PEACHS_CASTLE:
            return COURSE_LUIGIS_GARDEN_SECOND;
        case COURSE_RECORD_DRIVING_RANGE:
            return COURSE_DRIVING_RANGE;
    }

    return COURSE_TOAD_HIGHLANDS;
}

void ClearHoleBuild(void);
void OpenHoleAsset(CourseRecordIndex courseRecordIndex, s32 holeIndex, HoleAssetKind8 assetKind);
void ReadBillboards(void);
void ReadGridVertices(void);
void ReadHoleTriangles(void);
void ReadTileMidpoints(void);

void FinishHolePoints(CourseRecordIndex courseRecordIndex UNUSED, s32 holeIndex UNUSED) {
}

void BuildHole(CourseRecordIndex courseRecordIndex, s32 holeIndex) {
    AssetFile asset;
    char path[HOST_PATH_SIZE];
    u32 size;
    s32 pointsSize;

    if (courseRecordIndex == COURSE_RECORD_SHY_GUY_DESERT) {
        FillGroundNoise(GROUND_NOISE_RANGE, LIGHT_GROUND_NOISE_BASE);
    }
    else if (courseRecordIndex == COURSE_RECORD_YOSHIS_ISLAND) {
        FillGroundNoise(GROUND_NOISE_RANGE, LIGHT_GROUND_NOISE_BASE);
    }
    else {
        FillGroundNoise(GROUND_NOISE_RANGE, GROUND_NOISE_BASE);
    }

    if (g_GameState.isMiniGolf) {
        f_PaintLogic = g_GameState.courseIndex;
        if (g_GameState.courseIndex > LAST_MINI_GOLF_PAINT_LOGIC) {
            f_PaintLogic = LAST_MINI_GOLF_PAINT_LOGIC;
        }

        FillGroundNoise(MINI_GOLF_GROUND_NOISE_RANGE, MINI_GOLF_GROUND_NOISE_BASE);
    }
    else if (IsGameFlagSet(GAME_FLAG_READ_HOST_HOLE)) {
        f_PaintLogic = f_HolePaintLogics[courseRecordIndex - 1][holeIndex - 1];
        if (IsGameFlagSet(GAME_FLAG_NEXT_HOST_PAINT_LOGIC_A)) {
            f_PaintLogic++;
        }

        if (IsGameFlagSet(GAME_FLAG_NEXT_HOST_PAINT_LOGIC_B)) {
            f_PaintLogic++;
        }
    }
    else {
        f_PaintLogic = GetCoursePaintLogic(courseRecordIndex);
    }

    osSyncPrintf("PaintLogic%d\n", f_PaintLogic);
    if (!IsGameFlagSet(GAME_FLAG_READ_HOST_HOLE)) {
        if (IsGameFlagSet(GAME_FLAG_NEXT_COURSE_FIRST_HOLE) && holeIndex == 0) {
            holeIndex = COURSE_HOLE_COUNT;
        }

        ClearHoleBuild();
        OpenHoleAsset(courseRecordIndex, holeIndex, HOLE_ASSET_VERTICES);
        ReadGridVertices();
        OpenHoleAsset(courseRecordIndex, holeIndex, HOLE_ASSET_MIDPOINTS);
        ReadTileMidpoints();
        OpenHoleAsset(courseRecordIndex, holeIndex, HOLE_ASSET_TREES);
        ReadBillboards();
        if (!g_GameState.isRoundContinued && g_Scene != SCENE_STAFF_ROLL) {
            if (g_GameState.gameMode == GAME_MODE_TITLE_HOLE) {
                OpenAssetFile(ASSET_TITLE_HOLE_SETUP, &asset);
            }
            else {
                OpenAssetFile(f_HoleAssetIds[courseRecordIndex][holeIndex].setup, &asset);
            }

            ReadAssetFile(&g_HoleSetup, GetAssetFileSize(&asset), &asset);
        }

        if (g_GameState.gameMode == GAME_MODE_TITLE_HOLE) {
            OpenAssetFile(ASSET_TITLE_HOLE_POINTS, &asset);
        }
        else {
            OpenAssetFile(f_HoleAssetIds[courseRecordIndex][holeIndex].points, &asset);
        }

        pointsSize = GetAssetFileSize(&asset);
        ReadAssetFile(&g_HolePoints, pointsSize, &asset);
        osSyncPrintf("%d\n", pointsSize);
        FinishHolePoints(courseRecordIndex, holeIndex);
        OpenHoleAsset(courseRecordIndex, holeIndex, HOLE_ASSET_TRIANGLES);
        ReadHoleTriangles();
        OpenHoleAsset(courseRecordIndex, holeIndex, HOLE_ASSET_ATTRIBUTES);
        MeasureGroundTiles();
        PrepareHoleTriangles();
        f_IsCodeMapCleaned = TRUE;
        osSyncPrintf("GT..\n");
        PaintGroundTiles();
        osSyncPrintf("OK!\n");
        PlaceBillboards();
        return;
    }

    ClearHoleBuild();
    if (IsGameFlagSet(GAME_FLAG_SKIP_HOST_HOLE_READ)) {
        ClearGameFlag(GAME_FLAG_SKIP_HOST_HOLE_READ);
    }
    else {
        sprintf(
            path,
            "bin/course%d/hole%d%2.2d.vtx",
            (int)courseRecordIndex,
            (int)courseRecordIndex,
            (int)holeIndex
        );
        ReadHostFile(path, &size);
        sprintf(
            path,
            "bin/course%d/hole%d%2.2d.dtx",
            (int)courseRecordIndex,
            (int)courseRecordIndex,
            (int)holeIndex
        );
        ReadHostFile(path, &size);
        sprintf(
            path,
            "bin/course%d/hole%d%2.2d.tre",
            (int)courseRecordIndex,
            (int)courseRecordIndex,
            (int)holeIndex
        );
        ReadHostFile(path, &size);
        sprintf(
            path,
            "bin/course%d/hole%d%2.2d.hol",
            (int)courseRecordIndex,
            (int)courseRecordIndex,
            (int)holeIndex
        );
        ReadHostFile(path, &size);
        sprintf(
            path,
            "bin/course%d/hole%d%2.2d.pnt",
            (int)courseRecordIndex,
            (int)courseRecordIndex,
            (int)holeIndex
        );
        ReadHostFile(path, &size);
        sprintf(
            path,
            "bin/course%d/hole%d%2.2d.tri",
            (int)courseRecordIndex,
            (int)courseRecordIndex,
            (int)holeIndex
        );
        if (ReadHostFile(path, &size) == HOST_FILE_OK) {
            MoveRingsToEnd();
        }

        sprintf(
            path,
            "bin/course%d/hole%d%2.2d.att",
            (int)courseRecordIndex,
            (int)courseRecordIndex,
            (int)holeIndex
        );
        ReadHostFile(path, &size);
    }

    osSyncPrintf("@1\n");
    MeasureGroundTiles();
    f_IsCodeMapCleaned = TRUE;
    PaintGroundTiles();
    PlaceBillboards();
    osSyncPrintf("Ok!\n");
}

void SaveHoleFiles(s32 courseNumber, s32 holeNumber) {
    char path[HOST_PATH_SIZE];
    s32 column;
    s32 row;

    sprintf(path, "bin/course%d/hole%d%2.2d.vtx", courseNumber, courseNumber, holeNumber);
    WriteHostFile(path, HOLE_VERTICES_FILE_SIZE);
    sprintf(path, "bin/course%d/hole%d%2.2d.dtx", courseNumber, courseNumber, holeNumber);
    WriteHostFile(path, HOLE_MIDPOINTS_FILE_SIZE);
    sprintf(path, "bin/course%d/hole%d%2.2d.tre", courseNumber, courseNumber, holeNumber);
    WriteHostFile(path, HOLE_TREES_FILE_SIZE);
    sprintf(path, "bin/course%d/hole%d%2.2d.hol", courseNumber, courseNumber, holeNumber);
    WriteHostFile(path, HOLE_SETUP_FILE_SIZE);
    sprintf(path, "bin/course%d/hole%d%2.2d.pnt", courseNumber, courseNumber, holeNumber);
    WriteHostFile(path, HOLE_POINTS_FILE_SIZE);
    sprintf(path, "bin/course%d/hole%d%2.2d.tri", courseNumber, courseNumber, holeNumber);
    WriteHostFile(path, HOLE_TRIANGLES_FILE_SIZE);
    if (f_IsCodeMapCleaned) {
        LoadBackupCodeMap();
        f_IsCodeMapCleaned = FALSE;
    }

    f_BuildCursor = HOLE_FILE_BUFFER;
    f_BuildBuffer = f_BuildCursor;
    for (row = 0; row < MAP_ROW_COUNT; row++) {
        for (column = 0; column < MAP_COLUMN_COUNT; column++) {
            f_BuildCursor = &f_BuildBuffer[(row * MAP_COLUMN_COUNT) + column];
            *f_BuildCursor = GetColorTerrainCode(ReadMapTexel(column, row));
        }
    }

    sprintf(path, "bin/course%d/hole%d%2.2d.att", courseNumber, courseNumber, holeNumber);
    if (WriteHostFile(path, MAP_COLUMN_COUNT * MAP_ROW_COUNT) == HOST_FILE_OK) {
        f_BuildCursor = f_BuildBuffer;
    }
}

#define RING_MOVE_NOTICE_COLUMN 19
#define RING_MOVE_NOTICE_ROW 19
#define RING_MOVE_LOG_COLUMN 4
#define RING_MOVE_LOG_ROW (-141)

void MoveRingsToEnd(void) {
    s32 triangleIndex;
    s32 slot;

    PrintDebugText("Change Start", RING_MOVE_NOTICE_COLUMN, RING_MOVE_NOTICE_ROW);
    for (triangleIndex = 0; triangleIndex != HOLE_TRIANGLE_COUNT - RING_COUNT; triangleIndex++) {
        if (g_WorkArea.hole.triangles[triangleIndex].kind == HOLE_TRIANGLE_KIND_RING) {
            for (slot = HOLE_TRIANGLE_COUNT - RING_COUNT; slot != HOLE_TRIANGLE_COUNT;
                slot++, triangleIndex++) {
                sprintf(g_PrintBuffer, "%d <-- %d", (int)slot, (int)triangleIndex);
                PrintDebugText(g_PrintBuffer, RING_MOVE_LOG_COLUMN, RING_MOVE_LOG_ROW);
                g_WorkArea.hole.triangles[slot] = g_WorkArea.hole.triangles[triangleIndex];
                bzero(&g_WorkArea.hole.triangles[triangleIndex], sizeof(HoleTriangle));
            }

            PrintDebugText("Change End", RING_MOVE_NOTICE_COLUMN, RING_MOVE_NOTICE_ROW);
            return;
        }
    }
}

void ClearBillboards(void);
void ClearHoleTriangles(void);
void ClearTileBorders(void);
void ResetBuildBuffer(void);

void ClearHoleBuild(void) {
    ClearHolePoints();
    ClearTileBorders();
    InitTerrainTextures();
    ClearBillboards();
    ClearHoleTriangles();
    ResetBuildBuffer();
}

void ClearHolePoints(void) {
    s32 i;

    if (!g_GameState.isRoundContinued) {
        g_HoleSetup.cupPosition[AXIS_X] = 0;
        g_HoleSetup.cupPosition[AXIS_Y] = 0;
        g_HoleSetup.cupPosition[AXIS_Z] = 0;
        g_HoleSetup.teePosition[AXIS_X] = 0;
        g_HoleSetup.teePosition[AXIS_Y] = 0;
        g_HoleSetup.teePosition[AXIS_Z] = 0;
    }

    for (i = 0; i < CUP_POSITION_COUNT; i++) {
        g_HolePoints.cupPositions[i][AXIS_X] = 0;
        g_HolePoints.cupPositions[i][AXIS_Y] = 0;
        g_HolePoints.cupPositions[i][AXIS_Z] = 0;
        g_HolePoints.teePositions[i][AXIS_X] = 0;
        g_HolePoints.teePositions[i][AXIS_Y] = 0;
        g_HolePoints.teePositions[i][AXIS_Z] = 0;
    }

    for (i = 0; i < ARRAY_COUNT(g_HolePoints.aimPoints); i++) {
        g_HolePoints.aimPoints[i][AXIS_X] = 0;
        g_HolePoints.aimPoints[i][AXIS_Y] = 0;
        g_HolePoints.aimPoints[i][AXIS_Z] = 0;

        *g_HolePoints.routePoints[i] = 0;
        g_HolePoints.routePoints[i][AXIS_Z] = 0;
    }
}

void ResetBuildBuffer(void) {
    f_BuildCursor = BUILD_BUFFER_START;
    f_BuildBuffer = f_BuildCursor;
    f_BuildCursor = f_BuildBuffer;
}

void ClearBillboards(void) {
    s32 billboardIndex;

    for (billboardIndex = 0; billboardIndex < BILLBOARD_COUNT; billboardIndex++) {
        g_Billboards[billboardIndex].x = 0;
        g_Billboards[billboardIndex].y = 0;
        g_Billboards[billboardIndex].z = 0;
        g_Billboards[billboardIndex].textureIndex = -1;
        g_Billboards[billboardIndex].isIgnoredEditorMark = FALSE;
        g_Billboards[billboardIndex].isNearCourseEdge = TRUE;
        g_Billboards[billboardIndex].width = CLEARED_BILLBOARD_WIDTH;
        g_Billboards[billboardIndex].height = CLEARED_BILLBOARD_HEIGHT;
    }
}

void ClearHoleTriangles(void) {
    bzero(g_WorkArea.hole.triangles, sizeof(g_WorkArea.hole.triangles));
}

void ClearTileBorders(void) {
    s32 tileIndex;

    for (tileIndex = 0; tileIndex < GROUND_TILE_COUNT; tileIndex++) {
        g_WorkArea.hole.tiles[tileIndex]
            .texels[GROUND_TILE_TEXEL_ROW_COUNT - 1][GROUND_TILE_TEXEL_ROW_LENGTH - 1] = 0;
        g_WorkArea.hole.tiles[tileIndex].mirroredCells = 0;
    }
}

void InitGridVertices(void) {
    s16 x;
    s16 z;
    s32 vertexIndex;

    x = 0;
    z = 0;
    for (vertexIndex = 0; vertexIndex < GRID_VERTEX_COUNT; vertexIndex++) {
        g_WorkArea.hole.gridVertices[vertexIndex].v.ob[AXIS_X] = x;
        g_WorkArea.hole.gridVertices[vertexIndex].v.tc[TEXTURE_AXIS_S] =
            (x * GRID_TEXTURE_SCALE) + GRID_TEXTURE_BORDER;
        g_WorkArea.hole.gridVertices[vertexIndex].v.ob[AXIS_Y] = 0;
        g_WorkArea.hole.gridVertices[vertexIndex].v.ob[AXIS_Z] = z;
        g_WorkArea.hole.gridVertices[vertexIndex].v.flag = 0;
        g_WorkArea.hole.gridVertices[vertexIndex].v.tc[TEXTURE_AXIS_T] =
            (z * GRID_TEXTURE_SCALE) + GRID_TEXTURE_BORDER;
        g_WorkArea.hole.gridVertices[vertexIndex].v.cn[RED_CHANNEL] = 0;
        g_WorkArea.hole.gridVertices[vertexIndex].v.cn[GREEN_CHANNEL] = 0;
        g_WorkArea.hole.gridVertices[vertexIndex].v.cn[BLUE_CHANNEL] = 0;
        g_WorkArea.hole.gridVertices[vertexIndex].v.cn[ALPHA_CHANNEL] = 0;
        x += TILE_SPAN;
        if (x == (GROUND_TILE_COLUMN_COUNT + 1) * TILE_SPAN) {
            x = HALF_TILE_SPAN;
        }

        if (x == (GROUND_TILE_COLUMN_COUNT * TILE_SPAN) + HALF_TILE_SPAN) {
            x = 0;
            z += HALF_TILE_SPAN;
        }
    }
}

void InitTileMidpoints(void) {
    s16 offsets[TILE_MIDPOINT_COUNT][PLANE_AXIS_COUNT];
    s16 x;
    s16 z;
    s32 tileIndex;
    s32 midpointIndex;

    offsets[0][0] = QuarterTileSpans(1);
    offsets[0][1] = 0;
    offsets[1][0] = QuarterTileSpans(3);
    offsets[1][1] = 0;
    offsets[2][0] = 0;
    offsets[2][1] = QuarterTileSpans(1);
    offsets[3][0] = QuarterTileSpans(4);
    offsets[3][1] = QuarterTileSpans(1);
    offsets[4][0] = 0;
    offsets[4][1] = QuarterTileSpans(3);
    offsets[5][0] = QuarterTileSpans(4);
    offsets[5][1] = QuarterTileSpans(3);
    offsets[6][0] = QuarterTileSpans(1);
    offsets[6][1] = QuarterTileSpans(4);
    offsets[7][0] = QuarterTileSpans(3);
    offsets[7][1] = QuarterTileSpans(4);
    offsets[8][0] = QuarterTileSpans(1);
    offsets[8][1] = QuarterTileSpans(1);
    offsets[9][0] = QuarterTileSpans(2);
    offsets[9][1] = QuarterTileSpans(1);
    offsets[10][0] = QuarterTileSpans(3);
    offsets[10][1] = QuarterTileSpans(1);
    offsets[11][0] = QuarterTileSpans(1);
    offsets[11][1] = QuarterTileSpans(2);
    offsets[12][0] = QuarterTileSpans(3);
    offsets[12][1] = QuarterTileSpans(2);
    offsets[13][0] = QuarterTileSpans(1);
    offsets[13][1] = QuarterTileSpans(3);
    offsets[14][0] = QuarterTileSpans(2);
    offsets[14][1] = QuarterTileSpans(3);
    offsets[15][0] = QuarterTileSpans(3);
    offsets[15][1] = QuarterTileSpans(3);
    x = 0;
    z = 0;
    for (tileIndex = 0; tileIndex < GROUND_TILE_COUNT; tileIndex++) {
        for (midpointIndex = 0; midpointIndex < TILE_MIDPOINT_COUNT; midpointIndex++) {
            g_WorkArea.hole.midpoints[tileIndex][midpointIndex].v.ob[AXIS_X] =
                x + offsets[midpointIndex][0];
            g_WorkArea.hole.midpoints[tileIndex][midpointIndex].v.ob[AXIS_Y] = 0;
            g_WorkArea.hole.midpoints[tileIndex][midpointIndex].v.ob[AXIS_Z] =
                z + offsets[midpointIndex][1];
            g_WorkArea.hole.midpoints[tileIndex][midpointIndex].v.flag = 0;
            g_WorkArea.hole.midpoints[tileIndex][midpointIndex].v.tc[TEXTURE_AXIS_S] =
                ((x + offsets[midpointIndex][0]) * GRID_TEXTURE_SCALE) + GRID_TEXTURE_BORDER;
            g_WorkArea.hole.midpoints[tileIndex][midpointIndex].v.tc[TEXTURE_AXIS_T] =
                ((z + offsets[midpointIndex][1]) * GRID_TEXTURE_SCALE) + GRID_TEXTURE_BORDER;
            g_WorkArea.hole.midpoints[tileIndex][midpointIndex].v.cn[RED_CHANNEL] = 0;
            g_WorkArea.hole.midpoints[tileIndex][midpointIndex].v.cn[GREEN_CHANNEL] = 0;
            g_WorkArea.hole.midpoints[tileIndex][midpointIndex].v.cn[BLUE_CHANNEL] = 0;
            g_WorkArea.hole.midpoints[tileIndex][midpointIndex].v.cn[ALPHA_CHANNEL] = 0;
        }

        x += TILE_SPAN;
        if (x == GROUND_TILE_COLUMN_COUNT * TILE_SPAN) {
            x = 0;
            z += TILE_SPAN;
        }
    }
}

#define UnpackAttribute(packed) ((((packed) & 0xF0) << 4) | ((packed) & 0xF))

void ReadAttributeRuns(void) {
    s16 runLength;
    u8 packed;
    u16 *attribute;
    u16 *bufferEnd;
    u16 *runEnd;
    s32 unpacked;

    attribute = (u16 *)f_BuildCursor;
    bufferEnd = (u16 *)(f_BuildCursor + ATTRIBUTE_BUFFER_SIZE);
    while (TRUE) {
        ReadFileBytes(&runLength, sizeof(runLength));
        if (runLength == 0) {
            break;
        }

        ReadFileBytes(&packed, sizeof(packed));
        runEnd = attribute + runLength;
        if (runEnd > bufferEnd) {
            osSyncPrintf("ER1\n");
            break;
        }

        unpacked = UnpackAttribute(packed);
        while (attribute != runEnd) {
            *attribute++ = unpacked;
        }
    }

    if (attribute != bufferEnd) {
        osSyncPrintf("Whats?\n");
    }
    while (attribute < bufferEnd) {
        *attribute++ = 0;
    }

    f_BuildCursor = f_BuildBuffer;
}

void ReadBillboards(void) {
    u8 billboardCount;
    u8 billboardIndex;

    ReadFileBytes(&billboardCount, sizeof(billboardCount));
    for (billboardIndex = 0; billboardIndex < billboardCount; billboardIndex++) {
        if (billboardIndex >= BILLBOARD_COUNT) {
            osSyncPrintf("ER2\n");
            return;
        }

        ReadFileBytes(&g_Billboards[billboardIndex].x, sizeof(s16));
        ReadFileBytes(&g_Billboards[billboardIndex].y, sizeof(s16));
        ReadFileBytes(&g_Billboards[billboardIndex].z, sizeof(s16));
        ReadFileBytes(&g_Billboards[billboardIndex].textureIndex, sizeof(s8));
        g_Billboards[billboardIndex].shapeIndex = 0;
        ReadFileBytes(&g_Billboards[billboardIndex].isIgnoredEditorMark, sizeof(u8));
        g_Billboards[billboardIndex].isNearCourseEdge = FALSE;
        ReadFileBytes(&g_Billboards[billboardIndex].width, sizeof(u16));
        ReadFileBytes(&g_Billboards[billboardIndex].height, sizeof(u16));
    }
}

void ReadHoleTriangles(void) {
    u8 unused[0x10] UNUSED;
    u8 triangleCount;
    HoleTriangleKind8 kind;
    u8 triangleIndex;
    HoleTriangle *triangle;

    ReadFileBytes(&triangleCount, sizeof(triangleCount));
    for (triangleIndex = 0; triangleIndex < triangleCount; triangleIndex++) {
        if (triangleCount >= HOLE_TRIANGLE_COUNT) {
            osSyncPrintf("ER3\n");
            return;
        }

        triangle = &g_WorkArea.hole.triangles[triangleIndex];
        ReadFileBytes(&kind, sizeof(kind));
        triangle->kind = kind;
        ReadFileBytes(triangle->vertices[0].v.ob, sizeof(s16) * AXIS_COUNT);
        ReadFileBytes(triangle->vertices[1].v.ob, sizeof(s16) * AXIS_COUNT);
        ReadFileBytes(triangle->vertices[2].v.ob, sizeof(s16) * AXIS_COUNT);
    }
}

void ReadTileMirroredCells(void) {
    s32 tileIndex;

    for (tileIndex = 0; tileIndex < GROUND_TILE_COUNT; tileIndex++) {
        ReadFileBytes(&g_WorkArea.hole.tiles[tileIndex].mirroredCells, sizeof(u16));
    }
}

void ReadVertexX(s16 *coordinate, s32 vertexFlags);
void ReadVertexY(s16 *coordinate, s32 vertexFlags);
void ReadVertexZ(s16 *coordinate, s32 vertexFlags);

void ReadGridVertices(void) {
    s8 vertexFlags;
    s16 x;
    s16 y;
    s16 z;
    s32 vertexIndex;

    for (vertexIndex = 0;; vertexIndex++) {
        ReadFileBytes(&vertexFlags, sizeof(vertexFlags));
        if (vertexFlags == VERTEX_LIST_END) {
            break;
        }

        if (vertexIndex >= GROUND_GRID_VERTEX_SLOT_COUNT) {
            osSyncPrintf("ER4\n");
            break;
        }

        if (!(vertexFlags & VERTEX_FLAG_SAME_X)) {
            ReadVertexX(&x, vertexFlags);
            g_WorkArea.hole.gridVertices[vertexIndex].v.ob[AXIS_X] = x;
            g_WorkArea.hole.gridVertices[vertexIndex].v.tc[TEXTURE_AXIS_S] =
                (x * GRID_TEXTURE_SCALE) + GRID_TEXTURE_BORDER;
        }

        ReadVertexY(&y, vertexFlags);
        g_WorkArea.hole.gridVertices[vertexIndex].v.ob[AXIS_Y] = y;
        if (!(vertexFlags & VERTEX_FLAG_SAME_Z)) {
            ReadVertexZ(&z, vertexFlags);
            g_WorkArea.hole.gridVertices[vertexIndex].v.ob[AXIS_Z] = z;
            g_WorkArea.hole.gridVertices[vertexIndex].v.tc[TEXTURE_AXIS_T] =
                (z * GRID_TEXTURE_SCALE) + GRID_TEXTURE_BORDER;
        }
    }
}

void ReadTileMidpoints(void) {
    s8 vertexFlags;
    s16 x;
    s16 y;
    s16 z;
    s32 midpoint;
    s32 tileIndex;

    midpoint = 0;
    tileIndex = 0;
    while (TRUE) {
        ReadFileBytes(&vertexFlags, sizeof(vertexFlags));
        if (vertexFlags == VERTEX_LIST_END) {
            break;
        }

        if (!(vertexFlags & VERTEX_FLAG_SAME_X)) {
            ReadVertexX(&x, vertexFlags);
            g_WorkArea.hole.midpoints[tileIndex][midpoint].v.ob[AXIS_X] = x;
            g_WorkArea.hole.midpoints[tileIndex][midpoint].v.tc[TEXTURE_AXIS_S] =
                (x * GRID_TEXTURE_SCALE) + GRID_TEXTURE_BORDER;
        }

        ReadVertexY(&y, vertexFlags);
        g_WorkArea.hole.midpoints[tileIndex][midpoint].v.ob[AXIS_Y] = y;
        if (!(vertexFlags & VERTEX_FLAG_SAME_Z)) {
            ReadVertexZ(&z, vertexFlags);
            g_WorkArea.hole.midpoints[tileIndex][midpoint].v.ob[AXIS_Z] = z;
            g_WorkArea.hole.midpoints[tileIndex][midpoint].v.tc[TEXTURE_AXIS_T] =
                (z * GRID_TEXTURE_SCALE) + GRID_TEXTURE_BORDER;
        }

        midpoint++;
        if (midpoint >= TILE_MIDPOINT_COUNT) {
            midpoint = 0;
            tileIndex++;
        }
    }
}

void ReadVertexX(s16 *coordinate, s32 vertexFlags) {
    *coordinate = 0;
    switch (vertexFlags & VERTEX_FLAG_X_ENCODING) {
        case VERTEX_ENCODING_BYTE:
            ReadFileBytes(coordinate, 1);
            *coordinate = (u16)*coordinate >> BITS_PER_BYTE;
            break;
        case VERTEX_ENCODING_FINE_BYTE:
            ReadFileBytes(coordinate, 1);
            *coordinate = ((u16)*coordinate >> BITS_PER_BYTE) << VERTEX_FINE_HORIZONTAL_SHIFT;
            break;
        case VERTEX_ENCODING_COARSE_BYTE:
            ReadFileBytes(coordinate, 1);
            *coordinate = ((u16)*coordinate >> BITS_PER_BYTE) << VERTEX_COARSE_HORIZONTAL_SHIFT;
            break;
        case VERTEX_ENCODING_TWO_BYTES:
            ReadFileBytes(coordinate, 2);
            break;
    }
}

void ReadVertexY(s16 *coordinate, s32 vertexFlags) {
    *coordinate = 0;
    switch ((u8)(vertexFlags & VERTEX_FLAG_Y_ENCODING) >> VERTEX_FLAG_Y_ENCODING_SHIFT) {
        case VERTEX_ENCODING_BYTE:
            ReadFileBytes(coordinate, 1);
            *coordinate = (u16)*coordinate >> BITS_PER_BYTE;
            break;
        case VERTEX_ENCODING_FINE_BYTE:
            ReadFileBytes(coordinate, 1);
            *coordinate = ((u16)*coordinate >> BITS_PER_BYTE) << VERTEX_FINE_HEIGHT_SHIFT;
            break;
        case VERTEX_ENCODING_COARSE_BYTE:
            ReadFileBytes(coordinate, 1);
            *coordinate = ((u16)*coordinate >> BITS_PER_BYTE) << VERTEX_COARSE_HEIGHT_SHIFT;
            break;
        case VERTEX_ENCODING_TWO_BYTES:
            ReadFileBytes(coordinate, 2);
            break;
    }
}

void ReadVertexZ(s16 *coordinate, s32 vertexFlags) {
    *coordinate = 0;
    switch ((u8)(vertexFlags & VERTEX_FLAG_Z_ENCODING) >> VERTEX_FLAG_Z_ENCODING_SHIFT) {
        case VERTEX_ENCODING_BYTE:
            ReadFileBytes(coordinate, 1);
            *coordinate = (u16)*coordinate >> BITS_PER_BYTE;
            break;
        case VERTEX_ENCODING_FINE_BYTE:
            ReadFileBytes(coordinate, 1);
            *coordinate = ((u16)*coordinate >> BITS_PER_BYTE) << VERTEX_FINE_HORIZONTAL_SHIFT;
            break;
        case VERTEX_ENCODING_COARSE_BYTE:
            ReadFileBytes(coordinate, 1);
            *coordinate = ((u16)*coordinate >> BITS_PER_BYTE) << VERTEX_COARSE_HORIZONTAL_SHIFT;
            break;
        case VERTEX_ENCODING_TWO_BYTES:
            ReadFileBytes(coordinate, 2);
            break;
    }
}

void OpenHoleAsset(CourseRecordIndex courseRecordIndex, s32 holeIndex, HoleAssetKind8 assetKind) {
    f_ReadCursor = HOLE_FILE_BUFFER;
    switch (assetKind) {
        case HOLE_ASSET_VERTICES:
            if (g_GameState.gameMode == GAME_MODE_TITLE_HOLE) {
                OpenAssetFile(ASSET_TITLE_HOLE_VERTICES, &f_HoleAsset);
            }
            else {
                OpenAssetFile(f_HoleAssetIds[courseRecordIndex][holeIndex].vertices, &f_HoleAsset);
            }

            break;
        case HOLE_ASSET_MIDPOINTS:
            if (g_GameState.gameMode == GAME_MODE_TITLE_HOLE) {
                OpenAssetFile(ASSET_TITLE_HOLE_MIDPOINTS, &f_HoleAsset);
            }
            else {
                OpenAssetFile(f_HoleAssetIds[courseRecordIndex][holeIndex].midpoints, &f_HoleAsset);
            }

            break;
        case HOLE_ASSET_TREES:
            if (g_GameState.gameMode == GAME_MODE_TITLE_HOLE) {
                OpenAssetFile(ASSET_TITLE_HOLE_TREES, &f_HoleAsset);
            }
            else {
                OpenAssetFile(f_HoleAssetIds[courseRecordIndex][holeIndex].trees, &f_HoleAsset);
            }

            break;
        case HOLE_ASSET_TRIANGLES:
            if (g_GameState.gameMode == GAME_MODE_TITLE_HOLE) {
                OpenAssetFile(ASSET_TITLE_HOLE_TRIANGLES, &f_HoleAsset);
            }
            else {
                OpenAssetFile(f_HoleAssetIds[courseRecordIndex][holeIndex].triangles, &f_HoleAsset);
            }

            break;
        case HOLE_ASSET_ATTRIBUTES:
            if (g_GameState.gameMode == GAME_MODE_TITLE_HOLE) {
                OpenAssetFile(ASSET_TITLE_HOLE_ATTRIBUTES, &f_HoleAsset);
            }
            else {
                OpenAssetFile(
                    f_HoleAssetIds[courseRecordIndex][holeIndex].attributes,
                    &f_HoleAsset
                );
            }

            GetAssetFileSize(&f_HoleAsset);
            LoadLzData(f_HoleAsset.romStart, f_BuildCursor, f_HoleAsset.size);
            return;
    }

    ReadAssetFile(f_ReadCursor, GetAssetFileSize(&f_HoleAsset), &f_HoleAsset);
}

#define TEXEL_RED_SHIFT 11
#define TEXEL_GREEN_SHIFT 6
#define TEXEL_BLUE_SHIFT 1
#define TEXEL_CHANNEL_MAX 31

#define BILLBOARD_UNITS_PER_TEXEL 16
#define SHADOW_ROWS_BEFORE 16
#define SHADOW_ROWS_AFTER 25
#define SHADOW_COLUMN_REACH 25
#define SHADOW_LIGHT_DISTANCE 1000.0f
#define SOLIDITY_ROW_SHIFT 5
#define SOLIDITY_ROW_LENGTH (1 << SOLIDITY_ROW_SHIFT)
#define LIT_PERCENT_AT_BASE 50.0f
#define SHADOW_NEARNESS_LIMIT 1.5f
#define SHADOW_NEARNESS_MIN 0.1f
#define SHADOW_SOLIDITY_SCALE 4.0f
#define RED_BRIGHTNESS_SHIFT (RGBA16_RED_SHIFT - 2)
#define GREEN_BRIGHTNESS_SHIFT (RGBA16_GREEN_SHIFT - 3)
#define SHADOW_BRIGHTNESS_FLOOR 60
#define SHADOW_BRIGHTNESS_MAX 163
#define SHADOW_DARKENING_SCALE 4
#define SHADOW_SCALE_FRACTION_BITS 16
#define SHADOW_SCALE_ONE (1 << SHADOW_SCALE_FRACTION_BITS)

static inline u16 *GetShadowedTexel(u32 column, u32 row) {
    u32 tileOffset = ((column / GROUND_TILE_TEXEL_SPAN) +
                         ((row / GROUND_TILE_TEXEL_SPAN) * GROUND_TILE_COLUMN_COUNT)) *
        sizeof(GroundTile);
    u32 texelOffset =
        (((row & MAP_TILE_MASK) * GROUND_TILE_TEXEL_SPAN) + (column & MAP_TILE_MASK)) * sizeof(u16);

    return (u16 *)((u8 *)g_WorkArea.hole.tiles + texelOffset + tileOffset);
}

/* clang-format off */
void CastBillboardShadows(void) {
    Mat4 lightView;
    Vec3 lightPoint;
    u8 solidities[SOLIDITY_ROW_LENGTH * SOLIDITY_ROW_LENGTH];
    Vec3 rowPoint;
    Radians lightYaw;
    Radians lightPitch;
    s32 index;
    s32 listIndex;
    s32 lastTextureIndex;
    Boolean hasShadowStarted;
    Boolean isRowInWidth;
    s32 row;
    s32 column;
    s32 maskRow;
    s32 maskColumn;
    s32 firstRow;
    s32 firstColumn;

    s32 rowEnd;
    s32 columnEnd;
    s32 centerColumn;
    s32 centerRow;
    s32 x;
    s32 z;
    f32 width;
    f32 height;
    f32 widthScale;
    f32 heightScale;
    f32 widthFraction;
    f32 heightFraction;
    f32 nearness;
    s32 litPercent;
    s32 solidLitPercent;
    s32 solidity;
    s32 brightness;
    u32 scale;
    u16 color;
    u32 red;
    u32 green;
    u32 blue;
    f32 viewX;
    f32 viewY;
    u32 alpha;

    lastTextureIndex = -1;
    lightYaw = g_HoleSetup.conditions.lightYaw * BINARY_ANGLE_TO_RADIANS;
    lightPitch = g_HoleSetup.conditions.lightPitch * BINARY_ANGLE_TO_RADIANS;

    for (index = 0; index != BILLBOARD_COUNT; index++) {
        hasShadowStarted = FALSE;
        if (g_Billboards[index].textureIndex < 0)
            continue;

        for (listIndex = 0;; listIndex++) {
            if (f_ShadowlessTextures[listIndex] == g_Billboards[index].textureIndex) {
                listIndex = -1;
                break;
            }

            if (f_ShadowlessTextures[listIndex] < 0)
                break;
        }

        if (listIndex < 0)
            continue;

        if (g_Billboards[index].textureIndex != lastTextureIndex) {
            for (maskRow = 0; maskRow <= BILLBOARD_MASK_SIZE; maskRow++)
                for (maskColumn = 0; maskColumn <= BILLBOARD_MASK_SIZE; maskColumn++)
                    solidities[(maskRow << SOLIDITY_ROW_SHIFT) + maskColumn] = GetBillboardSolidity(
                        g_Billboards[index].textureIndex,
                        maskColumn * (1.0f / BILLBOARD_MASK_SIZE),
                        maskRow * (1.0f / BILLBOARD_MASK_SIZE)
                    );

            lastTextureIndex = g_Billboards[index].textureIndex;
        }

        width = g_Billboards[index].width;
        widthScale = 1.0f / width;
        height = g_Billboards[index].height;
        heightScale = 1.0f / height;
        x = g_Billboards[index].x;
        z = g_Billboards[index].z;
        lightPoint[AXIS_X] = (cosf(lightPitch) * cosf(lightYaw) * SHADOW_LIGHT_DISTANCE) + x;
        lightPoint[AXIS_Z] = (cosf(lightPitch) * sinf(lightYaw) * SHADOW_LIGHT_DISTANCE) + z;
        lightPoint[AXIS_Y] = sinf(lightPitch) * SHADOW_LIGHT_DISTANCE;
        guLookAtF(
            lightView,
            x,
            0.0f,
            z,
            lightPoint[AXIS_X],
            lightPoint[AXIS_Y],
            lightPoint[AXIS_Z],
            0.0f,
            -1.0f,
            0.0f
        );
        centerColumn = x / BILLBOARD_UNITS_PER_TEXEL;
        centerRow = z / BILLBOARD_UNITS_PER_TEXEL;
        lightView[0][0] *= BILLBOARD_UNITS_PER_TEXEL;
        lightView[0][1] *= BILLBOARD_UNITS_PER_TEXEL;
        lightView[0][2] *= BILLBOARD_UNITS_PER_TEXEL;
        lightView[2][0] *= BILLBOARD_UNITS_PER_TEXEL;
        lightView[2][1] *= BILLBOARD_UNITS_PER_TEXEL;
        lightView[2][2] *= BILLBOARD_UNITS_PER_TEXEL;
        firstRow = centerRow - SHADOW_ROWS_BEFORE;
        rowEnd = centerRow + SHADOW_ROWS_AFTER;
        if (firstRow < 0)
            firstRow = 0;

        if (rowEnd >= MAP_ROW_COUNT)
            rowEnd = MAP_ROW_COUNT - 1;

        for (row = firstRow; row < rowEnd; row++) {
            rowPoint[AXIS_X] = (lightView[2][0] * row) + lightView[3][0];
            rowPoint[AXIS_Y] = (lightView[2][1] * row) + lightView[3][1];
            rowPoint[AXIS_Z] = (lightView[2][2] * row) + lightView[3][2];
            isRowInWidth = FALSE;
            firstColumn = centerColumn - SHADOW_COLUMN_REACH;
            columnEnd = centerColumn + SHADOW_COLUMN_REACH;
            if (firstColumn < 0)
                firstColumn = 0;

            if (columnEnd >= MAP_COLUMN_COUNT)
                columnEnd = MAP_COLUMN_COUNT - 1;

            for (column = firstColumn; column < columnEnd; column++) {
                viewX = (lightView[0][0] * column) + rowPoint[AXIS_X];
                if (width <= fabsf(viewX)) {
                    if (!isRowInWidth)
                        continue;

                    break;
                }

                isRowInWidth = TRUE;
                viewY = (lightView[0][1] * column) + rowPoint[AXIS_Y];
                if (((-width <= viewY) & (viewY < height)) == 0)
                    continue;

                litPercent = PERCENT_PER_WHOLE;
                widthFraction = fabsf(viewX) * widthScale;
                heightFraction = viewY * heightScale;
                if (widthFraction + fabsf(heightFraction * 2) < 1.0f)
                    litPercent = (sqrtf(
                                      (widthFraction * widthFraction) +
                                      (heightFraction * heightFraction * 4.0f)
                                  ) * LIT_PERCENT_AT_BASE) +
                        LIT_PERCENT_AT_BASE;

                if (viewY >= 0.0f) {
                    s32 texelRow = heightFraction * BILLBOARD_MASK_SIZE;
                    s32 texelColumn = widthFraction * BILLBOARD_MASK_SIZE;

                    solidity = solidities[(texelRow << SOLIDITY_ROW_SHIFT) + texelColumn];
                    if (solidity != 0) {
                        nearness = SHADOW_NEARNESS_LIMIT -
                            (fabsf((lightView[0][2] * column) + rowPoint[AXIS_Z]) * heightScale);
                        if (nearness > SHADOW_NEARNESS_MIN) {
                            solidLitPercent =
                                PERCENT_PER_WHOLE - (solidity * nearness * SHADOW_SOLIDITY_SCALE);
                            if (solidLitPercent < litPercent)
                                litPercent = solidLitPercent;
                        }
                    }
                }

                if (litPercent == PERCENT_PER_WHOLE)
                    continue;

                color = *GetShadowedTexel(column, row);
                red = color & TEXEL_RED_MASK;
                green = color & TEXEL_GREEN_MASK;
                blue = color & TEXEL_BLUE_MASK;
                brightness = (red >> RED_BRIGHTNESS_SHIFT) + (green >> GREEN_BRIGHTNESS_SHIFT) + blue;
                brightness -= SHADOW_BRIGHTNESS_FLOOR;
                if (brightness < 0)
                    continue;

                if (brightness > SHADOW_BRIGHTNESS_MAX)
                    brightness = SHADOW_BRIGHTNESS_MAX;

                scale = SHADOW_SCALE_ONE -
                    ((PERCENT_PER_WHOLE - litPercent) * brightness * SHADOW_DARKENING_SCALE);
                red = (red * scale) >> SHADOW_SCALE_FRACTION_BITS;
                green = (green * scale) >> SHADOW_SCALE_FRACTION_BITS;
                blue = (blue * scale) >> SHADOW_SCALE_FRACTION_BITS;
                alpha = color & TEXEL_ALPHA_MASK;
                if (red > TEXEL_RED_MASK)
                    red = TEXEL_RED_MASK;
                else
                    red &= TEXEL_RED_MASK;

                if (green > TEXEL_GREEN_MASK)
                    green = TEXEL_GREEN_MASK;
                else
                    green &= TEXEL_GREEN_MASK;

                if (blue > TEXEL_BLUE_MASK)
                    blue = TEXEL_BLUE_MASK;
                else
                    blue &= TEXEL_BLUE_MASK;

                color = red | green | blue | alpha;
                *GetShadowedTexel(column, row) = color;
            }

            if (!isRowInWidth) {
                if (hasShadowStarted == TRUE)
                    break;
            }
            else
                hasShadowStarted = TRUE;
        }
    }
}
/* clang-format on */

void GetGroundLightColors(GroundLightColor *light, GroundLightColor *shade) {
    GroundLightColor keyLights[2];
    GroundLightColor keyShades[2];
    s32 keyIndexes[2];
    s32 keyIndex;
    f32 fraction;

    s32 InterpolateChannel(s32 from, s32 to, f32 fraction) {
        return (to - from) * fraction + from;
    }

    if (g_HoleSetup.conditions.sceneColorIndex == SCENE_COLOR_INDEX_SUDDEN_DEATH) {
        keyIndexes[0] = g_HoleSetup.conditions.sceneColorIndex;
        keyIndexes[1] = keyIndexes[0];
    }
    else if ((s32)g_HoleSetup.conditions.sceneColorIndex >= SCENE_COLOR_INDEX_DRIVING_RANGE) {
        keyIndexes[0] = SCENE_COLOR_INDEX_DRIVING_RANGE;
        keyIndexes[1] = SCENE_COLOR_INDEX_ROUND_END;
    }
    else {
        keyIndexes[0] = SCENE_COLOR_INDEX_ROUND_START;
        keyIndexes[1] = SCENE_COLOR_INDEX_DRIVING_RANGE;
    }

    for (keyIndex = 0; keyIndex < 2; keyIndex++) {
        switch (keyIndexes[keyIndex]) {
            default:
            case SCENE_COLOR_INDEX_ROUND_END: {
                static const GroundLightColor endLight = {23, 18, 14};
                static const GroundLightColor endShade = {10, 6, 0};

                keyLights[keyIndex] = endLight;
                keyShades[keyIndex] = endShade;
                break;
            }
            case SCENE_COLOR_INDEX_DRIVING_RANGE: {
                static const GroundLightColor middleLight = {21, 21, 21};
                static const GroundLightColor middleShade = {4, 4, 4};

                keyLights[keyIndex] = middleLight;
                keyShades[keyIndex] = middleShade;
                break;
            }
            case SCENE_COLOR_INDEX_ROUND_START: {
                static const GroundLightColor startLight = {20, 20, 20};
                static const GroundLightColor startShade = {6, 6, 6};

                keyLights[keyIndex] = startLight;
                keyShades[keyIndex] = startShade;
                break;
            }
            case SCENE_COLOR_INDEX_SUDDEN_DEATH: {
                static const GroundLightColor suddenDeathLight = {20, 20, 2};
                static const GroundLightColor suddenDeathShade = {0, 0, 0};

                keyLights[keyIndex] = suddenDeathLight;
                keyShades[keyIndex] = suddenDeathShade;
                break;
            }
        }
    }

    if (keyIndexes[1] == keyIndexes[0]) {
        fraction = 0.0f;
    }
    else {
        fraction = (f32)((s32)g_HoleSetup.conditions.sceneColorIndex - keyIndexes[0]) /
            (f32)(keyIndexes[1] - keyIndexes[0]);
    }

    light->red = InterpolateChannel(keyLights[0].red, keyLights[1].red, fraction);
    light->green = InterpolateChannel(keyLights[0].green, keyLights[1].green, fraction);
    light->blue = InterpolateChannel(keyLights[0].blue, keyLights[1].blue, fraction);
    shade->red = InterpolateChannel(keyShades[0].red, keyShades[1].red, fraction);
    shade->green = InterpolateChannel(keyShades[0].green, keyShades[1].green, fraction);
    shade->blue = InterpolateChannel(keyShades[0].blue, keyShades[1].blue, fraction);
}

#define HEIGHT_RING_ROW_COUNT (HEIGHT_BAND_ROW_COUNT * 2)
#define HEIGHT_RING_ROW_MASK (HEIGHT_RING_ROW_COUNT - 1)
#define HEIGHT_BAND_LEAD (HEIGHT_BAND_ROW_COUNT / 2)
#define LIGHT_SLOPE_MIN (-32)
#define LIGHT_SLOPE_MAX 31
#define LIGHT_SLOPE_COUNT (LIGHT_SLOPE_MAX - LIGHT_SLOPE_MIN + 1)
#define LIGHT_SCALE_FRACTION_BITS 11
#define LIGHT_SCALE_ONE (1 << LIGHT_SCALE_FRACTION_BITS)
#define LIGHT_SCALE_SLOPE_STEP 0x30
#define LIGHT_SCALE_MIN 0x500
#define LIGHT_SCALE_MAX 0xC00
#define LIGHT_OFFSET_LENGTH 4.5f
#define LIGHT_COLOR_FRACTION_BITS 5
#define HEIGHT_FADE_SCALE 0.008f
#define HEIGHT_FADE_LIMIT 0.25f
#define HEIGHT_FADE_FULL 1.0f

#define SLOPE_LIGHT_WORD_COUNT 4

void ShadeGroundTiles(void) {
    s32 unused[20] UNUSED;
    GroundLightColor light = {24, 22, 16};
    GroundLightColor shade;

    struct {
        u8 red;
        u8 green;
        u8 blue;
    } fadeColor;

    u32 slopeLights[LIGHT_SLOPE_COUNT * SLOPE_LIGHT_WORD_COUNT];

    struct {
        u16 red;
        u16 green;
        u16 blue;
        u16 alpha;
    } shaded;

    Boolean8 isHighGroundFaded;
    Boolean8 isFadeUnlimited;
    Boolean8 isFadeDisabled;
    s16(*heightRows)[MAP_COLUMN_COUNT];
    Radians pitch;
    s32 lightColumnOffset;
    s32 lightRowOffset;
    s32 slope;
    s32 lightSlope;
    s32 scale;
    s32 row;
    s32 bandRow;
    s32 column;
    s32 texelsLeft;
    Boolean isMapEdge;
    s32 trailingColumn;
    s32 leadingColumn;
    s16 *heights;
    s16 *trailing;
    s16 *leading;
    u16 *texel;
    u16 texelColor;
    u32 color;
    u32 channel;
    u32 red;
    u32 green;
    u32 blue;
    s32 fadedRed;
    s32 fadedGreen;
    s32 fadedBlue;
    f32 fade;
    f32 fadeLimit;
    s32 fadeWeight;
    s32 keptWeight;

    isHighGroundFaded = IsGameFlagSet(GAME_FLAG_FADE_HIGH_GROUND);
    isFadeUnlimited = IsGameFlagSet(GAME_FLAG_FADE_HIGH_GROUND_FULLY);
    isFadeDisabled = IsGameFlagSet(GAME_FLAG_NO_HIGH_GROUND_FADE);
    heightRows = (s16(*)[MAP_COLUMN_COUNT])HOLE_FILE_BUFFER;
    fadeColor.red = TEXEL_CHANNEL_MAX;
    fadeColor.green = TEXEL_CHANNEL_MAX;
    fadeColor.blue = TEXEL_CHANNEL_MAX;
    GetGroundLightColors(&light, &shade);
    shade.green <<= TEXEL_GREEN_SHIFT;
    shade.blue <<= TEXEL_BLUE_SHIFT;
    pitch = g_HoleSetup.conditions.lightPitch * BINARY_ANGLE_TO_RADIANS;

    cosf(pitch);
    sinf(pitch);
    for (lightSlope = LIGHT_SLOPE_MIN; lightSlope <= LIGHT_SLOPE_MAX; lightSlope++) {
        scale = LIGHT_SCALE_ONE + (lightSlope * LIGHT_SCALE_SLOPE_STEP);
        if (scale < LIGHT_SCALE_MIN) {
            scale = LIGHT_SCALE_MIN;
        }
        else if (scale > LIGHT_SCALE_MAX) {
            scale = LIGHT_SCALE_MAX;
        }

        slopeLights[((lightSlope - LIGHT_SLOPE_MIN) * SLOPE_LIGHT_WORD_COUNT) + RED_CHANNEL] =
            light.red * scale;
        slopeLights[((lightSlope - LIGHT_SLOPE_MIN) * SLOPE_LIGHT_WORD_COUNT) + GREEN_CHANNEL] =
            light.green * scale;
        slopeLights[((lightSlope - LIGHT_SLOPE_MIN) * SLOPE_LIGHT_WORD_COUNT) + BLUE_CHANNEL] =
            light.blue * scale;
    }

    lightColumnOffset =
        cosf(g_HoleSetup.conditions.lightYaw * BINARY_ANGLE_TO_RADIANS) * LIGHT_OFFSET_LENGTH;
    lightRowOffset =
        sinf(g_HoleSetup.conditions.lightYaw * BINARY_ANGLE_TO_RADIANS) * LIGHT_OFFSET_LENGTH;
    osSyncPrintf("LightXd%d Zd%d\n", lightColumnOffset, lightRowOffset);
    for (row = 1 - HEIGHT_BAND_ROW_COUNT; row < MAP_ROW_COUNT; row++) {
        bandRow = row + HEIGHT_BAND_LEAD;
        if (bandRow >= 0) {
            if (bandRow < MAP_ROW_COUNT && !(bandRow & MAP_TILE_MASK)) {
                DrawBandHeights(
                    heightRows[bandRow & HEIGHT_BAND_ROW_COUNT],
                    bandRow / HEIGHT_BAND_ROW_COUNT
                );
            }
        }

        if (row < 0) {
            continue;
        }

        heights = heightRows[row & HEIGHT_RING_ROW_MASK];
        column = 0;
        do {
            texel = &g_WorkArea.hole
                         .tiles
                             [(column / GROUND_TILE_TEXEL_SPAN) +
                                 ((row / GROUND_TILE_TEXEL_SPAN) * GROUND_TILE_COLUMN_COUNT)]
                         .texels[0][(row & MAP_TILE_MASK) * GROUND_TILE_TEXEL_SPAN];
            isMapEdge = column == 0 || column == MAP_COLUMN_COUNT - GROUND_TILE_TEXEL_SPAN;
            if (!isMapEdge) {
                trailing = &heightRows[(row - lightRowOffset) & HEIGHT_RING_ROW_MASK]
                                      [column - lightColumnOffset];
                leading = &heightRows[(row + lightRowOffset) & HEIGHT_RING_ROW_MASK]
                                     [column + lightColumnOffset];
            }

            for (texelsLeft = GROUND_TILE_TEXEL_SPAN; texelsLeft != 0;
                texelsLeft--, column++, texel++, heights++) {
                if (isMapEdge) {
                    trailingColumn = column - lightColumnOffset;
                    leadingColumn = column + lightColumnOffset;

                    if (trailingColumn < 0) {
                        trailingColumn = 0;
                    }
                    else if (leadingColumn < 0) {
                        leadingColumn = 0;
                    }
                    else if (trailingColumn >= MAP_COLUMN_COUNT) {
                        trailingColumn = MAP_COLUMN_COUNT - 1;
                    }
                    else if (leadingColumn >= MAP_COLUMN_COUNT) {
                        leadingColumn = MAP_COLUMN_COUNT - 1;
                    }

                    slope =
                        heightRows[(row - lightRowOffset) & HEIGHT_RING_ROW_MASK][trailingColumn] -
                        heightRows[(row + lightRowOffset) & HEIGHT_RING_ROW_MASK][leadingColumn];
                }
                else {
                    slope = *trailing - *leading;
                    trailing++;
                    leading++;
                }

                if (slope < LIGHT_SLOPE_MIN) {
                    slope = LIGHT_SLOPE_MIN;
                }
                else if (slope > LIGHT_SLOPE_MAX) {
                    slope = LIGHT_SLOPE_MAX;
                }

                texelColor = *texel;
                if (*heights < 0) {
                    color = ((shade.red + (texelColor >> TEXEL_RED_SHIFT)) *
                                slopeLights
                                    [((slope - LIGHT_SLOPE_MIN) * SLOPE_LIGHT_WORD_COUNT) +
                                        RED_CHANNEL]) >>
                        (LIGHT_SCALE_FRACTION_BITS + LIGHT_COLOR_FRACTION_BITS);
                    if (color > TEXEL_CHANNEL_MAX) {
                        color = TEXEL_CHANNEL_MAX;
                    }

                    color <<= TEXEL_RED_SHIFT;
                    channel = ((shade.green + (texelColor & TEXEL_GREEN_MASK)) *
                                  slopeLights
                                      [((slope - LIGHT_SLOPE_MIN) * SLOPE_LIGHT_WORD_COUNT) +
                                          GREEN_CHANNEL]) >>
                        (LIGHT_SCALE_FRACTION_BITS + LIGHT_COLOR_FRACTION_BITS + TEXEL_GREEN_SHIFT);
                    if (channel > TEXEL_CHANNEL_MAX) {
                        channel = TEXEL_CHANNEL_MAX;
                    }

                    color |= channel << TEXEL_GREEN_SHIFT;
                    channel = ((shade.blue + (texelColor & TEXEL_BLUE_MASK)) *
                                  slopeLights
                                      [((slope - LIGHT_SLOPE_MIN) * SLOPE_LIGHT_WORD_COUNT) +
                                          BLUE_CHANNEL]) >>
                        (LIGHT_SCALE_FRACTION_BITS + LIGHT_COLOR_FRACTION_BITS + TEXEL_BLUE_SHIFT);
                    if (channel > TEXEL_CHANNEL_MAX) {
                        channel = TEXEL_CHANNEL_MAX;
                    }

                    color |= channel << TEXEL_BLUE_SHIFT;
                    color |= texelColor & TEXEL_ALPHA_MASK;
                    *texel = color;
                }
                else {
                    red = ((shade.red + (texelColor >> TEXEL_RED_SHIFT)) *
                              slopeLights
                                  [((slope - LIGHT_SLOPE_MIN) * SLOPE_LIGHT_WORD_COUNT) +
                                      RED_CHANNEL]) >>
                        (LIGHT_SCALE_FRACTION_BITS + LIGHT_COLOR_FRACTION_BITS);
                    shaded.red = red;
                    if (red > TEXEL_CHANNEL_MAX) {
                        shaded.red = TEXEL_CHANNEL_MAX;
                    }

                    green = ((shade.green + (texelColor & TEXEL_GREEN_MASK)) *
                                slopeLights
                                    [((slope - LIGHT_SLOPE_MIN) * SLOPE_LIGHT_WORD_COUNT) +
                                        GREEN_CHANNEL]) >>
                        (LIGHT_SCALE_FRACTION_BITS + LIGHT_COLOR_FRACTION_BITS + TEXEL_GREEN_SHIFT);
                    shaded.green = green;
                    if (green > TEXEL_CHANNEL_MAX) {
                        shaded.green = TEXEL_CHANNEL_MAX;
                    }

                    blue = ((shade.blue + (texelColor & TEXEL_BLUE_MASK)) *
                               slopeLights
                                   [((slope - LIGHT_SLOPE_MIN) * SLOPE_LIGHT_WORD_COUNT) +
                                       BLUE_CHANNEL]) >>
                        (LIGHT_SCALE_FRACTION_BITS + LIGHT_COLOR_FRACTION_BITS + TEXEL_BLUE_SHIFT);
                    shaded.blue = blue;
                    if (blue > TEXEL_CHANNEL_MAX) {
                        shaded.blue = TEXEL_CHANNEL_MAX;
                    }

                    shaded.alpha = texelColor & TEXEL_ALPHA_MASK;

                    if (*heights >= 0 && isHighGroundFaded) {
                        fade = *heights * HEIGHT_FADE_SCALE;
                        if (isFadeUnlimited) {
                            fadeLimit = HEIGHT_FADE_FULL;
                            if (fade >= fadeLimit) {
                                fade = fadeLimit;
                            }
                        }
                        else if (isFadeDisabled) {
                            fade = 0.0f;
                        }
                        else {
                            fadeLimit = HEIGHT_FADE_LIMIT;
                            if (fade >= fadeLimit) {
                                fade = fadeLimit;
                            }
                        }

                        fadeWeight = fade * LIGHT_SCALE_ONE;
                        keptWeight = LIGHT_SCALE_ONE - fadeWeight;
                        fadedRed = shaded.red;
                        fadedGreen = shaded.green;
                        fadedBlue = shaded.blue;
                        fadedRed = (fadedRed * keptWeight) + (fadeColor.red * fadeWeight);
                        if (fadedRed > TEXEL_RED_MASK) {
                            fadedRed = TEXEL_RED_MASK;
                        }
                        else {
                            fadedRed &= TEXEL_RED_MASK;
                        }

                        fadedGreen = ((fadedGreen * keptWeight) + (fadeColor.green * fadeWeight)) >>
                            (LIGHT_SCALE_FRACTION_BITS - TEXEL_GREEN_SHIFT);
                        if (fadedGreen > TEXEL_GREEN_MASK) {
                            fadedGreen = TEXEL_GREEN_MASK;
                        }
                        else {
                            fadedGreen &= TEXEL_GREEN_MASK;
                        }

                        fadedBlue = ((fadedBlue * keptWeight) + (fadeColor.blue * fadeWeight)) >>
                            (LIGHT_SCALE_FRACTION_BITS - TEXEL_BLUE_SHIFT);
                        if (fadedBlue > TEXEL_BLUE_MASK) {
                            fadedBlue = TEXEL_BLUE_MASK;
                        }
                        else {
                            fadedBlue &= TEXEL_BLUE_MASK;
                        }

                        color = fadedRed | fadedGreen | fadedBlue | shaded.alpha;
                        texelColor = color;
                    }
                    else {
                        texelColor = shaded.alpha |
                            ((shaded.red << TEXEL_RED_SHIFT) | (shaded.green << TEXEL_GREEN_SHIFT) |
                                (shaded.blue << TEXEL_BLUE_SHIFT));
                    }

                    *texel = texelColor;
                }
            }
        }
        while (column < MAP_COLUMN_COUNT);
    }
}

void ShadeGround(s32 shadeVariant UNUSED) {
    ShadeGroundTiles();
    osSyncPrintf("TR");
    CastBillboardShadows();
    osSyncPrintf("EE\n");
}

#define TILE_BORDER_END (GROUND_TILE_TEXEL_ROW_LENGTH - TILE_BORDER_BEFORE)
#define BORDERED_TILE_TEXEL_COUNT (GROUND_TILE_TEXEL_ROW_COUNT * GROUND_TILE_TEXEL_ROW_LENGTH)

static inline u16 ReadTileTexel(s32 tileColumn, s32 tileRow, s32 texelColumn, s32 texelRow) {
    s32 tileIndex;
    u16 *texel;

    if (tileColumn < 0) {
        tileColumn = 0;
        texelColumn = 0;
    }
    else if (tileColumn >= GROUND_TILE_COLUMN_COUNT) {
        tileColumn = GROUND_TILE_COLUMN_COUNT - 1;
        texelColumn = -1;
    }

    if (tileRow < 0) {
        tileRow = 0;
        texelRow = 0;
    }
    else if (tileRow >= GROUND_TILE_ROW_COUNT) {
        tileRow = GROUND_TILE_ROW_COUNT - 1;
        texelRow = -1;
    }

    tileIndex = (tileRow * GROUND_TILE_COLUMN_COUNT) + tileColumn;
    if (texelColumn > GROUND_TILE_TEXEL_SPAN) {
        texelColumn = GROUND_TILE_TEXEL_SPAN;
    }

    texelColumn = (texelColumn + GROUND_TILE_TEXEL_SPAN) & MAP_TILE_MASK;
    texelRow = (texelRow + GROUND_TILE_TEXEL_SPAN) & MAP_TILE_MASK;

    if (g_WorkArea.hole.tiles[tileIndex]
            .texels[GROUND_TILE_TEXEL_ROW_COUNT - 1][GROUND_TILE_TEXEL_ROW_LENGTH - 1] != 0) {
        texel = &g_WorkArea.hole.tiles[0].texels[TILE_BORDER_BEFORE][TILE_BORDER_BEFORE] +
            ((texelRow * GROUND_TILE_TEXEL_ROW_LENGTH) + texelColumn);
        texel = (u16 *)AddressPast(texel, tileIndex * sizeof(GroundTile));
        return *texel;
    }
    else {
        texel = (u16 *)((u8 *)g_WorkArea.hole.tiles[0].texels +
            (((texelRow * GROUND_TILE_TEXEL_SPAN) + texelColumn) * sizeof(u16)) +
            (tileIndex * sizeof(GroundTile)));
        return *texel;
    }
}

void ToggleGroundTileBorders(void) {
    u16 texels[GROUND_TILE_TEXEL_ROW_COUNT][GROUND_TILE_TEXEL_ROW_LENGTH];
    s32 tileColumn;
    s32 tileRow;
    s32 texelColumn;
    s32 texelRow;
    u16 *texel;
    u16 *tileTexel;
    f64 *source;
    f64 *sourceEnd;
    f64 *destination;

    if (g_WorkArea.hole.tiles[0]
            .texels[GROUND_TILE_TEXEL_ROW_COUNT - 1][GROUND_TILE_TEXEL_ROW_LENGTH - 1] != 0) {
        s32 strippedTileColumn;
        s32 strippedTileRow;
        s32 strippedTexelColumn;
        s32 strippedTexelRow;
        s32 texelIndex;

        for (strippedTileRow = 0; strippedTileRow < GROUND_TILE_ROW_COUNT; strippedTileRow++) {
            for (strippedTileColumn = 0; strippedTileColumn < GROUND_TILE_COLUMN_COUNT;
                strippedTileColumn++) {
                for (strippedTexelRow = 0; strippedTexelRow < GROUND_TILE_TEXEL_SPAN;
                    strippedTexelRow++) {
                    for (strippedTexelColumn = 0; strippedTexelColumn < GROUND_TILE_TEXEL_SPAN;
                        strippedTexelColumn++) {
                        texels[0]
                              [(strippedTexelRow * GROUND_TILE_TEXEL_SPAN) + strippedTexelColumn] =
                                  ReadTileTexel(
                                      strippedTileColumn,
                                      strippedTileRow,
                                      strippedTexelColumn,
                                      strippedTexelRow
                                  );
                    }
                }

                texels[GROUND_TILE_TEXEL_ROW_COUNT - 1][GROUND_TILE_TEXEL_ROW_LENGTH - 1] = 0;
                for (texelIndex = 0; texelIndex < BORDERED_TILE_TEXEL_COUNT; texelIndex++) {
                    g_WorkArea.hole
                        .tiles[(strippedTileRow * GROUND_TILE_COLUMN_COUNT) + strippedTileColumn]
                        .texels[0][texelIndex] = texels[0][texelIndex];
                }
            }
        }

        return;
    }

    for (tileRow = 0; tileRow < GROUND_TILE_ROW_COUNT; tileRow++) {
        for (tileColumn = 0; tileColumn < GROUND_TILE_COLUMN_COUNT; tileColumn++) {
            texel = &texels[TILE_BORDER_BEFORE][TILE_BORDER_BEFORE];
            tileTexel =
                g_WorkArea.hole.tiles[(tileRow * GROUND_TILE_COLUMN_COUNT) + tileColumn].texels[0];
            for (texelRow = 0; texelRow < GROUND_TILE_TEXEL_SPAN; texelRow++) {
                for (texelColumn = 0; texelColumn < GROUND_TILE_TEXEL_SPAN; texelColumn++) {
                    *texel++ = *tileTexel++;
                }

                texel += GROUND_TILE_TEXEL_ROW_LENGTH - GROUND_TILE_TEXEL_SPAN;
            }

            texel = texels[0];
            for (texelRow = -TILE_BORDER_BEFORE;
                texelRow < GROUND_TILE_TEXEL_ROW_COUNT - TILE_BORDER_BEFORE;
                texelRow++) {
                for (texelColumn = -TILE_BORDER_BEFORE; texelColumn < TILE_BORDER_END;) {
                    *texel = ReadTileTexel(
                        tileColumn + (texelColumn >> MAP_TILE_SHIFT),
                        tileRow + (texelRow >> MAP_TILE_SHIFT),
                        texelColumn,
                        texelRow
                    );
                    if ((u32)texelRow < GROUND_TILE_TEXEL_SPAN &&
                        texelColumn == -TILE_BORDER_BEFORE) {
                        texel += GROUND_TILE_TEXEL_SPAN + 1;
                        texelColumn += GROUND_TILE_TEXEL_SPAN + 1;
                    }
                    else {
                        texel++;
                        texelColumn++;
                    }
                }
            }

            texels[GROUND_TILE_TEXEL_ROW_COUNT - 1][GROUND_TILE_TEXEL_ROW_LENGTH - 1] = 0xFFFF;

            source = (f64 *)texels;
            sourceEnd = (f64 *)&texels[GROUND_TILE_TEXEL_ROW_COUNT];
            destination =
                (f64 *)g_WorkArea.hole.tiles[(tileRow * GROUND_TILE_COLUMN_COUNT) + tileColumn]
                    .texels;
            while (source != sourceEnd) {
                *destination++ = *source++;
                *destination++ = *source++;
            }

            g_WorkArea.hole.tiles[(tileRow * GROUND_TILE_COLUMN_COUNT) + tileColumn]
                .extraPassCount = 0;
        }
    }
}

void SetMiniGolfTileTextureCoordinates(s32 column, s32 row, Vtx_t *start, Vtx_t *end) {
    Vtx_t *vertex;
    s32 coordinate;

    for (vertex = start; vertex < end; vertex++) {
        coordinate =
            TexelCoordinate(
                (vertex->ob[AXIS_X] * MINI_GOLF_TEXTURE_SCALE_X) - (column << TILE_ORIGIN_SHIFT)
            ) +
            TILE_TEXTURE_ORIGIN;
        vertex->tc[TEXTURE_AXIS_S] = coordinate;

        coordinate = (vertex->ob[AXIS_Z] * MINI_GOLF_TEXTURE_SCALE_Z) - (row << TILE_ORIGIN_SHIFT);
        vertex->tc[TEXTURE_AXIS_T] = TexelCoordinate(coordinate) + TILE_TEXTURE_ORIGIN;
    }
}

void SetTileTextureCoordinates(s32 column, s32 row, Vtx_t *start, Vtx_t *end) {
    Vtx_t *vertex;
    s32 coordinate;

    if (g_GameState.isMiniGolf) {
        SetMiniGolfTileTextureCoordinates(column, row, start, end);
        return;
    }

    for (vertex = start; vertex < end; vertex++) {
        coordinate = TexelCoordinate(vertex->ob[AXIS_X] - (column << TILE_ORIGIN_SHIFT)) +
            TILE_TEXTURE_ORIGIN;
        vertex->tc[TEXTURE_AXIS_S] = coordinate;

        coordinate = vertex->ob[AXIS_Z] - (row << TILE_ORIGIN_SHIFT);
        vertex->tc[TEXTURE_AXIS_T] = TexelCoordinate(coordinate) + TILE_TEXTURE_ORIGIN;
    }
}

static inline void CopyVertices(Vtx *destination, Vtx *source, s32 count) {
    s32 vertexIndex;

    for (vertexIndex = 0; vertexIndex < count; vertexIndex++) {
        ((f64 *)&destination[vertexIndex])[0] = ((f64 *)&source[vertexIndex])[0];
        ((f64 *)&destination[vertexIndex])[1] = ((f64 *)&source[vertexIndex])[1];
    }
}

#define SegmentedVertex(segment, vertex) ((u8 *)(vertex) + ((segment) << 24))

void BuildGroundTileDisplayList(s32 column, s32 row, Gfx **displayListHead, Vtx **vertexCursor) {
    void SetTileVertexMidpoint(Vtx_t * from, Vtx_t * to, Vtx_t * midpoint) {
        midpoint->ob[AXIS_X] = (from->ob[AXIS_X] + to->ob[AXIS_X]) / 2;
        midpoint->ob[AXIS_Y] = (from->ob[AXIS_Y] + to->ob[AXIS_Y]) / 2;
        midpoint->ob[AXIS_Z] = (from->ob[AXIS_Z] + to->ob[AXIS_Z]) / 2;
        midpoint->flag = 0;
        midpoint->tc[TEXTURE_AXIS_S] = (from->tc[TEXTURE_AXIS_S] + to->tc[TEXTURE_AXIS_S]) / 2;
        midpoint->tc[TEXTURE_AXIS_T] = (from->tc[TEXTURE_AXIS_T] + to->tc[TEXTURE_AXIS_T]) / 2;
        midpoint->cn[0] = (from->cn[0] + to->cn[0]) / 2;
        midpoint->cn[1] = (from->cn[1] + to->cn[1]) / 2;
        midpoint->cn[2] = (from->cn[2] + to->cn[2]) / 2;
        midpoint->cn[3] = (from->cn[3] + to->cn[3]) / 2;
    }

    Gfx *gfx = *displayListHead;
    Vtx *vertex = *vertexCursor;
    s32 tileIndex = (row * GROUND_TILE_COLUMN_COUNT) + column;
    Vtx *midpoints;
    s32 *coarseOffsets;
    s32 segmentIndex = 0;
    s32 columnBlock;
    s32 rowBlock;
    s32 pointRow;
    s32 pointColumn;

    midpoints = g_WorkArea.hole.midpoints[g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex];
    g_GroundTileDisplayLists[tileIndex] = gfx;
    gDPSetTextureImage(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        1,
        TextureAddress(&g_WorkArea.hole.tiles[tileIndex])
    );
    gDPSetTile(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        0,
        0,
        G_TX_LOADTILE,
        0,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMASK,
        3,
        G_TX_MIRROR | G_TX_WRAP,
        6,
        3
    );
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, GROUND_TEXTURE_LOAD_SIZE, GROUND_TEXTURE_DXT);
    gDPPipeSync(gfx++);
    gDPSetTile(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        GROUND_TEXTURE_LINE_SIZE,
        0,
        1,
        0,
        G_TX_NOMIRROR | G_TX_CLAMP,
        G_TX_NOMASK,
        3,
        G_TX_MIRROR | G_TX_WRAP,
        6,
        3
    );
    gDPSetTileSize(gfx++, 1, 0, 0, 0x86, 0x82);
    coarseOffsets = g_CoarseVertexOffsets[tileIndex];

    CopyVertices(vertex, TileGridVertex(column, row, 0, 0), 2);
    CopyVertices(&vertex[2], TileGridVertex(column, row + 1, 0, 0), 2);
    gSPVertex(gfx++, vertex, QUAD_VERTEX_COUNT, TILE_SLOT_ROW0_COLUMN0);
    vertex += QUAD_VERTEX_COUNT;

    gSPVertex(
        gfx++,
        &midpoints[TILE_FIRST_INNER_MIDPOINT],
        TILE_INNER_MIDPOINT_COUNT,
        TILE_SLOT_ROW1_COLUMN1
    );
    gSPVertex(
        gfx++,
        SegmentedVertex(1, &midpoints[TILE_MIDPOINT_ROW0_COLUMN1]),
        TILE_SIDE_MIDPOINT_COUNT,
        TILE_SLOT_ROW0_COLUMN1
    );
    coarseOffsets[segmentIndex++] = BytesBetween(midpoints, vertex);
    SetTileVertexMidpoint(
        &TileGridVertex(column, row, 0, 0)->v,
        &TileGridVertex(column, row, 1, 0)->v,
        &vertex++->v
    );
    SetTileVertexMidpoint(
        &TileGridVertex(column, row, 1, 0)->v,
        &TileGridVertex(column, row, 2, 0)->v,
        &vertex++->v
    );
    gSPVertex(
        gfx++,
        SegmentedVertex(2, &midpoints[TILE_MIDPOINT_ROW1_COLUMN0]),
        1,
        TILE_SLOT_ROW1_COLUMN0
    );
    gSPVertex(
        gfx++,
        SegmentedVertex(2, &midpoints[TILE_MIDPOINT_ROW3_COLUMN0]),
        1,
        TILE_SLOT_ROW3_COLUMN0
    );
    gSPVertex(
        gfx++,
        SegmentedVertex(3, &midpoints[TILE_MIDPOINT_ROW1_COLUMN4]),
        1,
        TILE_SLOT_ROW1_COLUMN4
    );
    gSPVertex(
        gfx++,
        SegmentedVertex(3, &midpoints[TILE_MIDPOINT_ROW3_COLUMN4]),
        1,
        TILE_SLOT_ROW3_COLUMN4
    );
    coarseOffsets[segmentIndex++] = BytesBetween(&midpoints[TILE_MIDPOINT_ROW1_COLUMN0], vertex);
    SetTileVertexMidpoint(
        &TileGridVertex(column, row, 0, 0)->v,
        &TileGridVertex(column, row, 0, 1)->v,
        &vertex[0].v
    );
    SetTileVertexMidpoint(
        &TileGridVertex(column, row, 0, 1)->v,
        &TileGridVertex(column, row, 0, 2)->v,
        &vertex[2].v
    );
    vertex++;
    coarseOffsets[segmentIndex++] = BytesBetween(&midpoints[TILE_MIDPOINT_ROW1_COLUMN4], vertex);
    SetTileVertexMidpoint(
        &TileGridVertex(column, row, 2, 0)->v,
        &TileGridVertex(column, row, 2, 1)->v,
        &vertex[0].v
    );
    SetTileVertexMidpoint(
        &TileGridVertex(column, row, 2, 1)->v,
        &TileGridVertex(column, row, 2, 2)->v,
        &vertex[2].v
    );
    vertex += 3;
    gSPVertex(
        gfx++,
        SegmentedVertex(4, &midpoints[TILE_MIDPOINT_ROW4_COLUMN1]),
        TILE_SIDE_MIDPOINT_COUNT,
        TILE_SLOT_ROW4_COLUMN1
    );
    coarseOffsets[segmentIndex++] = BytesBetween(&midpoints[TILE_MIDPOINT_ROW4_COLUMN1], vertex);
    SetTileVertexMidpoint(
        &TileGridVertex(column, row, 0, 2)->v,
        &TileGridVertex(column, row, 1, 2)->v,
        &vertex++->v
    );
    SetTileVertexMidpoint(
        &TileGridVertex(column, row, 1, 2)->v,
        &TileGridVertex(column, row, 2, 2)->v,
        &vertex++->v
    );

    CopyVertices(vertex, TileGridVertex(column, row, 1, 0), 1);
    gSPVertex(gfx++, SegmentedVertex(5, vertex), 1, TILE_SLOT_ROW0_COLUMN2);
    vertex++;
    coarseOffsets[segmentIndex++] = sizeof(Vtx);
    SetTileVertexMidpoint(
        &TileGridVertex(column, row, 0, 0)->v,
        &TileGridVertex(column, row, 2, 0)->v,
        &vertex++->v
    );
    CopyVertices(vertex, TileGridVertex(column, row, 0, 1), 1);
    gSPVertex(gfx++, SegmentedVertex(6, vertex), 1, TILE_SLOT_ROW2_COLUMN0);
    vertex++;
    coarseOffsets[segmentIndex++] = sizeof(Vtx);
    SetTileVertexMidpoint(
        &TileGridVertex(column, row, 0, 0)->v,
        &TileGridVertex(column, row, 0, 2)->v,
        &vertex++->v
    );
    CopyVertices(vertex, TileGridVertex(column, row, 1, 1), 1);
    gSPVertex(gfx++, vertex, 1, TILE_SLOT_ROW2_COLUMN2);
    vertex++;
    CopyVertices(vertex, TileGridVertex(column, row, 2, 1), 1);
    gSPVertex(gfx++, SegmentedVertex(7, vertex), 1, TILE_SLOT_ROW2_COLUMN4);
    vertex++;
    coarseOffsets[segmentIndex++] = sizeof(Vtx);
    SetTileVertexMidpoint(
        &TileGridVertex(column, row, 2, 0)->v,
        &TileGridVertex(column, row, 2, 2)->v,
        &vertex++->v
    );
    CopyVertices(vertex, TileGridVertex(column, row, 1, 2), 1);
    gSPVertex(gfx++, SegmentedVertex(8, vertex), 1, TILE_SLOT_ROW4_COLUMN2);
    vertex++;
    coarseOffsets[segmentIndex++] = sizeof(Vtx);
    SetTileVertexMidpoint(
        &TileGridVertex(column, row, 0, 2)->v,
        &TileGridVertex(column, row, 2, 2)->v,
        &vertex++->v
    );

    for (columnBlock = 0; columnBlock < TILE_POINT_SPAN - 1; columnBlock += 2) {
        for (rowBlock = 0; rowBlock < TILE_POINT_SPAN - 1; rowBlock += 2) {
            for (pointRow = rowBlock; pointRow < rowBlock + 2; pointRow++) {
                for (pointColumn = columnBlock; pointColumn < columnBlock + 2; pointColumn++) {
                    gSP2Triangles(
                        gfx++,
                        f_TileVertexSlots[pointRow][pointColumn],
                        f_TileVertexSlots[pointRow][pointColumn + 1],
                        f_TileVertexSlots[pointRow + 1][pointColumn],
                        0,
                        f_TileVertexSlots[pointRow][pointColumn + 1],
                        f_TileVertexSlots[pointRow + 1][pointColumn + 1],
                        f_TileVertexSlots[pointRow + 1][pointColumn],
                        0
                    );
                }
            }
        }
    }

    gSPEndDisplayList(gfx++);
    SetTileTextureCoordinates(column, row, &(*vertexCursor)->v, &vertex->v);
    SetTileTextureCoordinates(column, row, &midpoints->v, &midpoints[TILE_MIDPOINT_COUNT].v);
    *displayListHead = gfx;
    *vertexCursor = vertex;
}

void BuildGroundDisplayLists(void) {
    Gfx *gfx;
    Vtx *vertices;
    s32 column;
    s32 row;

    gfx = GROUND_DISPLAY_LIST_START;
    vertices = GROUND_VERTEX_START;
    for (row = 0; row < GROUND_TILE_ROW_COUNT; row++) {
        for (column = 0; column < GROUND_TILE_COLUMN_COUNT; column++) {
            BuildGroundTileDisplayList(column, row, &gfx, &vertices);
        }
    }

    if ((u32)gfx > (u32)GROUND_DISPLAY_LIST_END || (u32)vertices > (u32)GROUND_VERTEX_END) {
        ReportDivisionError();
    }
}
