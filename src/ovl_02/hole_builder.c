/*
 * Builds a hole from its files: reading the map and its terrain codes, painting
 * the ground tiles, shaping and shading the ground, building its display lists,
 * and placing the billboards and their shadows.
 */
#include "common.h"

#include "hole_builder.h"

#include "asset.h"
#include "billboard.h"
#include "course.h"
#include "debug_text.h"
#include "game_flags.h"
#include "hole_loader.h"
#include "hole_triangles.h"
#include "host_file.h"
#include "math_util.h"
#include "play.h"
#include "save.h"
#include "scene.h"
#include "screen.h"
#include "terrain.h"
#include "text.h"
#include "work_area.h"

/* A ground tile's vertices start at its column and row shifted by this */
#define TILE_ORIGIN_SHIFT 9
/* Where texture coordinates start at a tile's origin */
#define TILE_TEXTURE_ORIGIN 0x100
/* How much mini golf vertices are scaled along x and z for their textures */
#define MINI_GOLF_TEXTURE_SCALE_X 4
#define MINI_GOLF_TEXTURE_SCALE_Z 8
/* Grid vertices lie on tile corners and halfway between them */
#define TILE_SPAN (1 << TILE_ORIGIN_SHIFT)
#define HALF_TILE_SPAN (TILE_SPAN / 2)
#define GRID_ROW_VERTEX_COUNT ((GROUND_TILE_COLUMN_COUNT * 2) + 1)
#define GRID_VERTEX_COUNT \
  (GRID_ROW_VERTEX_COUNT * ((GROUND_TILE_ROW_COUNT * 2) + 1))
/* Grid texture coordinates: four per unit, starting one texel in */
#define GRID_TEXTURE_SCALE 4
#define GRID_TEXTURE_BORDER TexelCoordinate(1)
/*
 * Vertex flags in the hole's vertex file: a VertexEncoding for each
 * coordinate, and whether x or z repeats the last vertex's
 */
#define VERTEX_FLAG_X_ENCODING 0x3
#define VERTEX_FLAG_Y_ENCODING 0xC
#define VERTEX_FLAG_Y_ENCODING_SHIFT 2
#define VERTEX_FLAG_Z_ENCODING 0x30
#define VERTEX_FLAG_Z_ENCODING_SHIFT 4
#define VERTEX_FLAG_SAME_X 0x40
#define VERTEX_FLAG_SAME_Z 0x80

/*
 * How a coordinate is stored in the vertex file: as two bytes, or as one byte
 * taken whole, or scaled up by a fine or a coarse shift. A one-byte
 * coordinate is read into the high byte and shifted down.
 */
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
/* Flags that end a list of vertices */
#define VERTEX_LIST_END 9
/* The size cleared billboards are given */
#define CLEARED_BILLBOARD_WIDTH 64
#define CLEARED_BILLBOARD_HEIGHT 256
/* Where the build buffer starts */
#define BUILD_BUFFER_START ((u8*)0x80315000)

/* The hole files that OpenHoleAsset opens, in the order of their asset ids */
#define HOLE_ASSET_VERTICES 1
#define HOLE_ASSET_MIDPOINTS 2
#define HOLE_ASSET_TREES 3
#define HOLE_ASSET_TRIANGLES 4
#define HOLE_ASSET_ATTRIBUTES 5
/* The asset ids of the one hole built in GAME_MODE_UNK12 */
#define UNK12_VERTICES_ASSET 0x3F0
#define UNK12_MIDPOINTS_ASSET 0x3F1
#define UNK12_TREES_ASSET 0x3F2
#define UNK12_UNK6_ASSET 0x3F3
#define UNK12_POINTS_ASSET 0x3F4
#define UNK12_TRIANGLES_ASSET 0x3F5
#define UNK12_ATTRIBUTES_ASSET 0x3F6
/* The paint logic of the last mini-golf course; later courses use it too */
#define LAST_MINI_GOLF_PAINT_LOGIC PAINT_LOGIC_PEACHS_CASTLE_THIRD

/* The size of a terrain code cell, and of a row of them */
#define TERRAIN_CODE_CELL_SIZE 2
#define TERRAIN_CODE_CELL_ROW_SIZE \
  (TERRAIN_CODE_CELL_COLUMN_COUNT * TERRAIN_CODE_CELL_SIZE)
/* The words a block of packed codes fills */
#define TERRAIN_CODE_BLOCK_WORD_COUNT (TERRAIN_CODE_BLOCK_SIZE / 4)
/* The size of the host file paths that BuildHole and SaveHoleFiles use */
#define HOST_PATH_SIZE 0x80
/* The sizes of the hole files that SaveHoleFiles writes */
#define HOLE_VERTICES_FILE_SIZE 0x2520
#define HOLE_MIDPOINTS_FILE_SIZE 0x8000
#define HOLE_TREES_FILE_SIZE 0x800
#define HOLE_UNK6_FILE_SIZE 0x34
#define HOLE_POINTS_FILE_SIZE 0x120
#define HOLE_TRIANGLES_FILE_SIZE 0x3C00
/* The size of the attributes unpacked into the build buffer */
#define ATTRIBUTE_BUFFER_SIZE 0x20000
/* Where hole files are read to */
#define HOLE_FILE_BUFFER ((u8*)0x8030F800)

#define TEXEL_RED_MASK 0xF800
#define TEXEL_GREEN_MASK 0x7C0
#define TEXEL_BLUE_MASK 0x3E
#define TEXEL_ALPHA_MASK 1
/* The texels in a tile's border: one before its texels and three after */
#define TILE_BORDER_BEFORE 1
/*
 * The paint routines give terrain nearer than this many cells to other
 * terrain a border's color
 */
#define TERRAIN_BORDER_DISTANCE 2
/*
 * The paint patterns' sizes in texels, each a power of two: greens are
 * checkered, fairways striped with stripes offset by half their width, and
 * the mini golf greens checkered in wide, deep checks
 */
#define GREEN_CHECK_SIZE 2
#define FAIRWAY_STRIPE_WIDTH 4
#define FAIRWAY_STRIPE_OFFSET (FAIRWAY_STRIPE_WIDTH / 2)
#define MINI_GOLF_CHECK_WIDTH 4
#define MINI_GOLF_CHECK_DEPTH 8
/*
 * Whether a green cell lies within one cell of other terrain. It is a macro,
 * as an inline function compiles differently from the original.
 */
#define IsGreenEdge(code, nearestCode, nearestDistance) \
  ((code) == TERRAIN_CODE_GREEN && (code) != (nearestCode) && \
      (nearestDistance) < TERRAIN_BORDER_DISTANCE)

/*
 * Indices of f_PaintLogics. They follow GameState.courseIndex's numbering,
 * which BuildHole uses as the paint logic of a mini-golf course.
 */
enum PaintLogicIndex {
  PAINT_LOGIC_TOAD_HIGHLANDS,
  PAINT_LOGIC_KOOPA_PARK,
  PAINT_LOGIC_SHY_GUY_DESERT,
  PAINT_LOGIC_YOSHIS_ISLAND,
  PAINT_LOGIC_BOO_VALLEY,
  PAINT_LOGIC_MARIOS_STAR,
  PAINT_LOGIC_LUIGIS_GARDEN_FIRST,
  PAINT_LOGIC_LUIGIS_GARDEN_SECOND,
  PAINT_LOGIC_LUIGIS_GARDEN_THIRD,
  PAINT_LOGIC_PEACHS_CASTLE_FIRST,
  PAINT_LOGIC_PEACHS_CASTLE_SECOND,
  PAINT_LOGIC_PEACHS_CASTLE_THIRD,
  PAINT_LOGIC_DRIVING_RANGE
};

/*
 * Paints a map texel of a code, given the nearest other code and its
 * distance. It returns the color, shifted by PAINTED_COLOR_SHIFT, with the
 * terrain code it painted.
 */
typedef s32 (*PaintRoutine)(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z);

#define PAINTED_COLOR_SHIFT 16

/* How a course paints its ground tiles from their terrain codes */
typedef struct PaintLogic {
  /*
   * For each code, how far to search for another code: 0 paints without
   * searching, and a negative span uses the code's painting at the origin
   */
  /* 0x00 */ s8 searchSpans[TERRAIN_CODE_COUNT];
  /* 0x10 */ PaintRoutine paint;
} PaintLogic; /* size = 0x14 */

/* The number of offsets PaintGroundTiles searches, and their largest span */
#define PAINT_OFFSET_COUNT 100
#define PAINT_OFFSET_SPAN 5

/* The largest span ListOffsetsByDistance handles */
#define MAX_OFFSET_SPAN GROUND_POINT_ROW_COUNT
/* Marks an offset that ListOffsetsByDistance has already listed */
#define LISTED_DISTANCE 0xFFFF
/* Farther than any distance ListOffsetsByDistance computes */
#define BEYOND_ALL_DISTANCES 0x10000

/* A color that lights or shades the ground */
typedef struct GroundLightColor {
  /* 0x0 */ u16 red;
  /* 0x2 */ u16 green;
  /* 0x4 */ u16 blue;
} GroundLightColor; /* size = 0x6 */

/*
 * While a hole is built, each ground tile holds a map image of
 * GROUND_TILE_TEXEL_SPAN rows of that many texels, with no border
 */
#define MAP_COLUMN_COUNT (GROUND_TILE_COLUMN_COUNT * GROUND_TILE_TEXEL_SPAN)
#define MAP_ROW_COUNT (GROUND_TILE_ROW_COUNT * GROUND_TILE_TEXEL_SPAN)
/* The map rows that DrawBandHeights fills at a time */
#define HEIGHT_BAND_ROW_COUNT 32
/* Marks a map texel that no triangle has drawn a height on */
#define NO_HEIGHT (-0x8000)
#define NO_HEIGHT_PAIR 0x80008000
/* Ground positions are in units of 1/HEIGHT_ROW_SPAN map texel */
#define HEIGHT_ROW_SPAN_SHIFT 4
#define HEIGHT_ROW_SPAN (1 << HEIGHT_ROW_SPAN_SHIFT)
#define HEIGHT_FRACTION_BITS 12
/* Scales a step per ground unit to a step per map row in HEIGHT_FRACTION_BITS
 * fixed point */
#define HEIGHT_ROW_STEP_SCALE 65536.0f
/*
 * DrawHeightRow steps a height across a row's texels in this many fraction
 * bits; f_HeightStepScales holds TEXEL_HEIGHT_ONE / n for the counts below
 * HEIGHT_STEP_SCALE_COUNT
 */
#define TEXEL_HEIGHT_FRACTION_BITS 16
#define TEXEL_HEIGHT_ONE 65536.0f
#define HEIGHT_STEP_SCALE_COUNT 32
/* The band's heights are filled with NO_HEIGHT four at a time, as doubles */
#define HEIGHTS_PER_FILL 4
/* DrawBandHeights spins this many times after each band; the original has it */
#define BAND_DELAY_LOOP_COUNT 0x2000

/* The points of a tile's grid, edges included, that MeasureGroundTiles averages
 */
#define GROUND_TILE_AVERAGED_POINT_COUNT \
  ((GROUND_TILE_POINT_SPAN + 1) * (GROUND_TILE_POINT_SPAN + 1))
/* A tile's lowest and highest heights start past any point's */
#define UNMEASURED_LOWEST_HEIGHT 0x40000000
#define UNMEASURED_HIGHEST_HEIGHT (-0x40000000)
#define MIN_GROUND_TILE_HEIGHT_RANGE 32
#define FLAT_GROUND_TILE_HEIGHT_RANGE 30
/* Splits a map column or row into a tile and a texel within it */
#define MAP_TILE_SHIFT 5
#define MAP_TILE_MASK (GROUND_TILE_TEXEL_SPAN - 1)

/* A ground tile has 5 by 5 points: its grid vertices and midpoints */
#define TILE_POINT_SPAN 5

/* A ground tile's texture is its bordered 16-bit texels */
#define GROUND_TEXTURE_LOAD_SIZE \
  ((GROUND_TILE_TEXEL_ROW_COUNT * GROUND_TILE_TEXEL_ROW_LENGTH) - 1)
#define GROUND_TEXTURE_LINE_SIZE \
  (((GROUND_TILE_TEXEL_ROW_LENGTH * G_IM_SIZ_16b_BYTES) + 7) >> 3)
#define GROUND_TEXTURE_DXT \
  CALC_DXT(GROUND_TILE_TEXEL_ROW_LENGTH, G_IM_SIZ_16b_BYTES)

/* The work-area buffers that the ground tiles' display lists and vertices are
 * built in */
#define GROUND_DISPLAY_LIST_START ((Gfx*)&g_WorkArea.bytes[0x6E920])
#define GROUND_DISPLAY_LIST_END ((Gfx*)&g_WorkArea.bytes[0x77D20])
#define GROUND_VERTEX_START ((Vtx*)&g_WorkArea.bytes[0x77F20])
#define GROUND_VERTEX_END ((Vtx*)&g_WorkArea.bytes[0x82720])

/* The asset ids of a hole's files */
typedef struct HoleAssetIds {
  /* 0x0 */ u16 vertices;
  /* 0x2 */ u16 midpoints;
  /* 0x4 */ u16 trees;
  /* 0x6 */ u16 unk6;
  /* 0x8 */ u16 points;
  /* 0xA */ u16 triangles;
  /* 0xC */ u16 attributes;
} HoleAssetIds; /* size = 0xE */

/* A hole's files are seven assets in a row */
#define HOLE_ASSET_IDS(first) \
  {(first), (first) + 1, (first) + 2, (first) + 3, (first) + 4, (first) + 5, \
      (first) + 6}

/* g_GameState.isMiniGolf under a symbol of its own, which BuildHole needs to
 * match */

s32 PaintMainCourse(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z);
s32 PaintShyGuyDesert(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z);
s32 PaintYoshisIsland(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z);
s32 PaintBooValley(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z);
s32 PaintFirstMiniGolfCourse(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z);
s32 PaintSecondMiniGolfCourse(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z);
s32 PaintThirdMiniGolfCourse(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z);
s32 PaintDrivingRange(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z);

static u32 f_PaintSeed = 0;
/*
 * The paint routines take their random bits from the seed's high half, since
 * a linear congruential generator's low bits repeat in short cycles
 */
#define PAINT_RANDOM_SHIFT 16

/*
 * The paint logics, which enum PaintLogicIndex indexes. The mini-golf ones
 * repeat, one per course of each record.
 */
static PaintLogic f_PaintLogics[] = {
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintMainCourse},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintMainCourse},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintShyGuyDesert},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintYoshisIsland},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintBooValley},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintMainCourse},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0},
        PaintFirstMiniGolfCourse},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0},
        PaintSecondMiniGolfCourse},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0},
        PaintThirdMiniGolfCourse},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0},
        PaintFirstMiniGolfCourse},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0},
        PaintSecondMiniGolfCourse},
    {{2, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0},
        PaintThirdMiniGolfCourse},
    {{0, 0, 0, 0, 0, -1, 3, -1, -1, -1, 0, 0, 0, 0, 0, 0}, PaintDrivingRange},
};
static Boolean f_IsCodeMapCleaned = FALSE;
/* Indexed by course record, then by hole */
static HoleAssetIds f_HoleAssetIds[COURSE_RECORD_COUNT][COURSE_HOLE_COUNT] = {
    /* COURSE_RECORD_TOAD_HIGHLANDS */
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
    /* COURSE_RECORD_KOOPA_PARK */
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
    /* COURSE_RECORD_SHY_GUY_DESERT */
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
    /* COURSE_RECORD_YOSHIS_ISLAND */
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
    /* COURSE_RECORD_BOO_VALLEY */
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
    /* COURSE_RECORD_MARIOS_STAR */
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
    /* COURSE_RECORD_LUIGIS_GARDEN */
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
    /* COURSE_RECORD_PEACHS_CASTLE */
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
    /* COURSE_RECORD_DRIVING_RANGE */
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
/*
 * The paint logic of each host hole, by course and hole numbers from 1. The
 * entries are enum PaintLogicIndex values, left as numbers because the hole
 * editor's flags step them to the next logic.
 */
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
/* Read by no routine */
static s32 f_UnusedWordAfterPaintLogics UNUSED = 0;
/* The billboard textures that cast no shadow, ending at a negative index */
static s8 f_ShadowlessTextures[] = {0x14, 0x15, 0x16, -1};
/* Read by no routine */
static s32 f_UnusedWordsAfterShadowlessTextures[] UNUSED = {0, 0, 0};
static s8 f_TileVertexSlots[TILE_POINT_SPAN][TILE_POINT_SPAN] = {
    {0, 12, 20, 13, 1},
    {14, 4, 5, 6, 16},
    {21, 7, 22, 8, 23},
    {15, 9, 10, 11, 17},
    {2, 18, 24, 19, 3},
};
/* Read by no routine */
static s32 f_UnusedWordAfterTileVertexSlots UNUSED = 0;
/* There is no step for a count of 0 */
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

void BuildGroundTileDisplayList(
    s32 column, s32 row, Gfx** gfxCursor, Vtx** vertexCursor);

void LoadCodeMap(char* path);
void EncodeCodeMap(void);
void CleanCodeMap(void);
void PaintGroundTiles(void);
void ClearHolePoints(void);
void ShadeGroundTiles(void);
void CastBillboardShadows(void);
void MoveRingsToEnd(void);

/*
 * The objects before the loader thread's stack are defined in order, as the
 * stack is global; see ORDERED_BSS. KMC aligns the statics it allocates
 * more strictly than the others, so the ordered ones state the original's
 * alignment where it differs.
 */
/* A PAINT_LOGIC_* value */
static s32 f_PaintLogic ORDERED_BSS = 0;

/* Read by no routine */
static u8 f_UnusedBytesBeforeReadCursor[12] ORDERED_BSS UNUSED = {0};

static u8* f_ReadCursor ORDERED_BSS = NULL;
static AssetFile f_HoleAsset ORDERED_BSS __attribute__((aligned(8))) = {0};

/* Read by no routine */
static u8 f_UnusedBytesAfterHoleAsset[8] ORDERED_BSS UNUSED = {0};
u8 g_LoaderThreadStack[LOADER_THREAD_STACK_SIZE] ORDERED_BSS
    __attribute__((aligned(8))) = {0};

static u8* f_BuildBuffer;
static u8* f_BuildCursor;

/* Copies bytes from the read cursor and advances it past them */
static inline void ReadFileBytes(void* destination, s32 size) {
  u8* byte = destination;
  u8* source = f_ReadCursor;
  u8* end = f_ReadCursor + size;

  while (source != end) {
    *byte++ = *source++;
  }

  f_ReadCursor = end;
}

/* Seeds the random numbers that paint logic draws */
void SetPaintSeed(u32 seed) {
  f_PaintSeed = seed;
}

/*
 * Fills offsets with up to maxCount column and row offsets less than span
 * from the origin, nearest first, and returns how many it wrote. Distances
 * are in units of 1/OFFSET_DISTANCE_SCALE.
 */
s32 ListOffsetsByDistance(s32 span, GridOffset* offsets, s32 maxCount) {
  f32 squares[MAX_OFFSET_SPAN + 1];
  u16 distances[span * span * 2];
  s32 row;
  s32 column;
  s32 count;
  s32 nearestIndex;
  s32 nearestDistance;
  s32 nearestRow;
  s32 nearestColumn;
  s32 mirroredRow;

  squares[0] = 0.0f;
  row = 0;
  while (row < span) {
    /* The block of their own gives the row counter its register */
    IN_OWN_BLOCK(row++; squares[row] = (f32)(row * row) *
            (OFFSET_DISTANCE_SCALE * OFFSET_DISTANCE_SCALE));
  }

  for (row = 0; row < span; row++) {
    for (column = 0; column < span; column++) {
      distances[(row * span) + column] = sqrtf(squares[column] + squares[row]);
    }
  }

  for (count = 0; count < maxCount;) {
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
    offsets[count].distance = nearestDistance;
    offsets[count].column = nearestColumn;
    offsets[count].row = nearestRow;
    count++;
    if (count >= maxCount) {
      break;
    }

    if (nearestColumn != 0) {
      offsets[count].column = -nearestColumn;
      offsets[count].row = nearestRow;
      offsets[count].distance = nearestDistance;
      count++;
      if (count >= maxCount) {
        break;
      }
    }

    if (nearestRow != 0) {
      mirroredRow = -nearestRow;
      offsets[count].column = nearestColumn;
      offsets[count].row = mirroredRow;
      offsets[count].distance = nearestDistance;
      count++;
      if (count >= maxCount) {
        break;
      }

      if (nearestColumn != 0) {
        offsets[count].column = -nearestColumn;
        offsets[count].row = mirroredRow;
        offsets[count].distance = nearestDistance;
        count++;
        if (count >= maxCount) {
          break;
        }
      }
    }
  }

  return count;
}

/*
 * Fills heights, a band of HEIGHT_BAND_ROW_COUNT map rows by MAP_COLUMN_COUNT
 * columns, with the ground's heights, by drawing the two triangles of each
 * ground grid square that covers the band. Where triangles overlap, it averages
 * their heights. A band at or past GROUND_TILE_ROW_COUNT is left untouched.
 *
 * Doesn't match: the nested DrawTriangleHeights allocates its registers
 * differently, using $s7 where the original reloads the static chain. 323
 * words differ in it.
 */
#ifdef NON_MATCHING
void DrawBandHeights(s16* heights, s32 band) {
  /* corners[4] is where the corners are swapped; it is there to match */
  Vtx* corners[5];
  s32 cachedSpan;
  f32 cachedReciprocal;
  s32 row;
  s32 column;
  f64 fill;

  /* DrawTriangleHeights is a nested function, a GNU C extension. */
  void DrawTriangleHeights(void) {
    f32 longSlopeX;
    f32 longSlopeHeight;
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
    s32 totalSpan;
    s32 span;
    s32 lowerSpan;
    s32 z;

    /* DrawHeightRow is a nested function, a GNU C extension. */
    inline void DrawHeightRow(
        s32 longX, s32 longHeight, s32 shortX, s32 shortHeight, s32 texelRow) {
      s32 fromX;
      s32 fromHeight;
      s32 toX;
      s32 toHeight;
      s32 width;
      s32 count;
      s32 direction;
      s32 texelHeightStep;
      s32 height;
      s32 texelColumn;
      s16* texel;
      s32 texelIndex;
      s32 value;

      fromX = longX >> HEIGHT_FRACTION_BITS;
      fromHeight = longHeight >> HEIGHT_FRACTION_BITS;
      toX = shortX >> HEIGHT_FRACTION_BITS;
      toHeight = shortHeight >> HEIGHT_FRACTION_BITS;
      width = fromX - toX;
      if (width < 0) {
        width = -width;
      }

      count = (width + HEIGHT_ROW_SPAN / 2) / HEIGHT_ROW_SPAN;
      direction = (fromX < toX) ? 1 : -1;
      if (count < HEIGHT_STEP_SCALE_COUNT) {
        texelHeightStep = f_HeightStepScales[count] * (toHeight - fromHeight);
      } else {
        texelHeightStep =
            ((toHeight - fromHeight) << TEXEL_HEIGHT_FRACTION_BITS) / count;
      }

      texelColumn = longX >> (HEIGHT_FRACTION_BITS + HEIGHT_ROW_SPAN_SHIFT);
      texelRow =
          (texelRow >> HEIGHT_ROW_SPAN_SHIFT) - band * HEIGHT_BAND_ROW_COUNT;
      if (texelRow >= 0 && texelRow < HEIGHT_BAND_ROW_COUNT) {
        height = fromHeight << TEXEL_HEIGHT_FRACTION_BITS;
        texel = &heights[texelRow * MAP_COLUMN_COUNT];
        texel += texelColumn;
        for (texelIndex = 0; texelIndex <= count; texelIndex++) {
          if (texelColumn >= 0 && texelColumn < MAP_COLUMN_COUNT) {
            value = height >> TEXEL_HEIGHT_FRACTION_BITS;
            if (*texel != NO_HEIGHT) {
              value = (*texel + value) / 2;
            }

            *texel = value;
          }

          texelColumn += direction;
          texel += direction;
          height += texelHeightStep;
        }
      }
    }

    totalSpan = corners[2]->v.ob[AXIS_Z] - corners[0]->v.ob[AXIS_Z];
    if (cachedSpan != totalSpan) {
      cachedSpan = totalSpan;
      cachedReciprocal = 1.0f / totalSpan;
    }

    longSlopeX = (corners[2]->v.ob[AXIS_X] - corners[0]->v.ob[AXIS_X]) *
        cachedReciprocal;
    longSlopeHeight = (corners[2]->v.ob[AXIS_Y] - corners[0]->v.ob[AXIS_Y]) *
        cachedReciprocal;
    span = corners[1]->v.ob[AXIS_Z] - corners[0]->v.ob[AXIS_Z];
    if (span != 0) {
      if (cachedSpan != span) {
        cachedSpan = span;
        cachedReciprocal = 1.0f / span;
      }

      shortSlopeX = (corners[1]->v.ob[AXIS_X] - corners[0]->v.ob[AXIS_X]) *
          cachedReciprocal;
      shortSlopeHeight = (corners[1]->v.ob[AXIS_Y] - corners[0]->v.ob[AXIS_Y]) *
          cachedReciprocal;
      longXStep = longSlopeX * HEIGHT_ROW_STEP_SCALE;
      shortXStep = shortSlopeX * HEIGHT_ROW_STEP_SCALE;
      longHeightStep = longSlopeHeight * HEIGHT_ROW_STEP_SCALE;
      shortHeightStep = shortSlopeHeight * HEIGHT_ROW_STEP_SCALE;
      shortX = corners[0]->v.ob[AXIS_X] << HEIGHT_FRACTION_BITS;
      longX = shortX;
      shortHeight = corners[0]->v.ob[AXIS_Y] << HEIGHT_FRACTION_BITS;
      longHeight = shortHeight;
      for (z = 0; z < span + HEIGHT_ROW_SPAN / 2; z += HEIGHT_ROW_SPAN) {
        DrawHeightRow(longX, longHeight, shortX, shortHeight,
            corners[0]->v.ob[AXIS_Z] + z);
        longX += longXStep;
        shortX += shortXStep;
        longHeight += longHeightStep;
        shortHeight += shortHeightStep;
      }
    }

    lowerSpan = corners[2]->v.ob[AXIS_Z] - corners[1]->v.ob[AXIS_Z];
    if (lowerSpan != 0) {
      if (cachedSpan != lowerSpan) {
        cachedSpan = lowerSpan;
        cachedReciprocal = 1.0f / lowerSpan;
      }

      longX = ((s32)(longSlopeX * span) + corners[0]->v.ob[AXIS_X])
          << HEIGHT_FRACTION_BITS;
      longHeight = ((s32)(longSlopeHeight * span) + corners[0]->v.ob[AXIS_Y])
          << HEIGHT_FRACTION_BITS;
      shortSlopeX = (corners[2]->v.ob[AXIS_X] - corners[1]->v.ob[AXIS_X]) *
          cachedReciprocal;
      shortSlopeHeight = (corners[2]->v.ob[AXIS_Y] - corners[1]->v.ob[AXIS_Y]) *
          cachedReciprocal;
      longXStep = longSlopeX * HEIGHT_ROW_STEP_SCALE;
      longHeightStep = longSlopeHeight * HEIGHT_ROW_STEP_SCALE;
      shortXStep = shortSlopeX * HEIGHT_ROW_STEP_SCALE;
      shortHeightStep = shortSlopeHeight * HEIGHT_ROW_STEP_SCALE;
      shortX = corners[1]->v.ob[AXIS_X] << HEIGHT_FRACTION_BITS;
      shortHeight = corners[1]->v.ob[AXIS_Y] << HEIGHT_FRACTION_BITS;
      for (z = 0; z < lowerSpan + HEIGHT_ROW_SPAN / 2; z += HEIGHT_ROW_SPAN) {
        DrawHeightRow(longX, longHeight, shortX, shortHeight,
            corners[1]->v.ob[AXIS_Z] + z);
        longX += longXStep;
        shortX += shortXStep;
        shortHeight += shortHeightStep;
        longHeight += longHeightStep;
      }
    }
  }

  cachedSpan = -1;
  if (band < GROUND_TILE_ROW_COUNT) {
    ((u32*)heights)[0] = NO_HEIGHT_PAIR;
    ((u32*)heights)[1] = NO_HEIGHT_PAIR;
    fill = *(f64*)heights;
    for (column = 1;
        column < HEIGHT_BAND_ROW_COUNT * MAP_COLUMN_COUNT / HEIGHTS_PER_FILL;
        column++) {
      ((f64*)heights)[column] = fill;
    }

    for (row = band * GROUND_TILE_POINT_SPAN;
        row < (band + 1) * GROUND_TILE_POINT_SPAN; row++) {
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

        /* The second triangle has the far corner in place of the first */
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
#else
INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", DrawTriangleHeights);
INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", DrawBandHeights);
#endif

/* Empties every terrain code cell and frees the packed code blocks */
void ResetTerrainCodeCells(void) {
  s32 row;
  s32 column;
  s32 cellOffset;

  g_TerrainCodeBlockCount = 0;
  for (row = 0; row < TERRAIN_CODE_CELL_ROW_COUNT; row++) {
    for (column = TERRAIN_CODE_CELL_COLUMN_COUNT - 1; column >= 0; column--) {
      /* Addressed by byte offset to match */
      cellOffset = (row * TERRAIN_CODE_CELL_ROW_SIZE) +
          (column * TERRAIN_CODE_CELL_SIZE);
      *(s16*)AddressPast(g_TerrainCodeCells, cellOffset) = -1;
    }
  }
}

/*
 * The paint logic of Toad Highlands, Koopa Park and Mario's Star. It returns
 * a cell's color in the upper half and its painted terrain code in the
 * lower: fairway near other terrain is painted as semi-rough, fairway and
 * greens are striped, rough away from fairway and TERRAIN_CODE_UNK3 are
 * mottled with random colors, and greens are edged as green edge.
 */
s32 PaintMainCourse(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
  u32 color;
  s32 paintedCode;
  s32 random;
  s32 fairwayRandom;
  s32 green;
  u32 red;

  switch (code) {
    case TERRAIN_CODE_DEEP_ROUGH:
      color = 0x4481;
      paintedCode = TERRAIN_CODE_DEEP_ROUGH;
      break;
    case TERRAIN_CODE_ROCK:
      color = 0x4983;
      paintedCode = TERRAIN_CODE_ROCK;
      break;
    case TERRAIN_CODE_FAIRWAY:
      if (nearestDistance >= TERRAIN_BORDER_DISTANCE ||
          nearestCode == TERRAIN_CODE_GREEN ||
          nearestCode == TERRAIN_CODE_FAIRWAY) {
        f_PaintSeed = NextRandomSeed(f_PaintSeed);
        fairwayRandom = f_PaintSeed >> PAINT_RANDOM_SHIFT;
        if ((x + FAIRWAY_STRIPE_OFFSET) & FAIRWAY_STRIPE_WIDTH) {
          color = (((fairwayRandom & 3) + 9) << RGBA16_RED_SHIFT) | 0x4C4;
        } else {
          color = (((fairwayRandom & 3) + 7) << RGBA16_RED_SHIFT) | 0x584;
        }

        paintedCode = TERRAIN_CODE_FAIRWAY;
      } else {
        color = 0x4605;
        paintedCode = TERRAIN_CODE_SEMI_ROUGH;
      }

      break;
    case TERRAIN_CODE_ROUGH:
      if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
        f_PaintSeed = NextRandomSeed(f_PaintSeed);
        red = (f_PaintSeed >> PAINT_RANDOM_SHIFT) & 7;
        green = 16 - (red >> 1);
        color = (red << RGBA16_RED_SHIFT) | (green << RGBA16_GREEN_SHIFT) | 1;
        paintedCode = TERRAIN_CODE_ROUGH;
      } else {
        color = 0xC81;
        paintedCode = TERRAIN_CODE_ROUGH;
      }

      break;
    case TERRAIN_CODE_BUNKER:
      color = 0xE739;
      paintedCode = TERRAIN_CODE_BUNKER;
      break;
    case TERRAIN_CODE_UNK3:
      paintedCode = TERRAIN_CODE_UNK3;
      f_PaintSeed = NextRandomSeed(f_PaintSeed);
      random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
      color = (((random & 4) + 2) << RGBA16_RED_SHIFT) |
          ((4 - ((random >> 5) & 3)) << RGBA16_GREEN_SHIFT) | 1;
      break;
    case TERRAIN_CODE_GREEN:
      if (IsGreenEdge(code, nearestCode, nearestDistance)) {
        color = 0x2587;
        paintedCode = TERRAIN_CODE_GREEN_EDGE;
      } else if (nearestCode == TERRAIN_CODE_GREEN ||
          nearestDistance >= TERRAIN_BORDER_DISTANCE) {
        if (z & GREEN_CHECK_SIZE) {
          color = (x & GREEN_CHECK_SIZE) ? 0x2F84 : 0x4580;
        } else {
          color = (x & GREEN_CHECK_SIZE) ? 0x3E42 : 0x3704;
        }

        paintedCode = TERRAIN_CODE_GREEN;
      } else {
        color = 0x2689;
        paintedCode = TERRAIN_CODE_FAIRWAY;
      }

      break;
    case TERRAIN_CODE_CART_WAY:
      color = 0x7BDF;
      paintedCode = TERRAIN_CODE_CART_WAY;
      break;
    case TERRAIN_CODE_OUT_OF_BOUNDS:
      color = 0x2A41;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
    case TERRAIN_CODE_TEE_GROUND:
      color = 0x2688;
      paintedCode = TERRAIN_CODE_TEE_GROUND;
      break;
    default:
      color = 0xFFFF;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
  }

  return (color << 16) | paintedCode;
}

/*
 * A paint logic like PaintMainCourse with pale rough, out-of-bounds ground
 * painted 0xBEEF, and TERRAIN_CODE_UNK3 painted as cart way away from more of
 * it. Neither the paint-logic table nor any code refers to it.
 *
 * Doesn't match: the original zeroes $t0, which nothing reads, in the delay
 * slot of the range test, and its two cart-way arms share one jump to the
 * end. 12 words differ. Initializing color, paintedCode or fairwayRandom
 * doesn't bring the zeroing back, and setting paintedCode after the arms
 * lets GCC merge them and drop the test (18).
 */
#ifdef NON_MATCHING
s32 PaintPaleRoughCourse(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
  u32 color;
  s32 paintedCode;
  s32 fairwayRandom;

  switch (code) {
    case TERRAIN_CODE_DEEP_ROUGH:
      color = 0x4481;
      paintedCode = TERRAIN_CODE_DEEP_ROUGH;
      break;
    case TERRAIN_CODE_ROCK:
      color = 0x4983;
      paintedCode = TERRAIN_CODE_ROCK;
      break;
    case TERRAIN_CODE_FAIRWAY:
      if (nearestDistance >= TERRAIN_BORDER_DISTANCE ||
          nearestCode == TERRAIN_CODE_GREEN ||
          nearestCode == TERRAIN_CODE_FAIRWAY) {
        f_PaintSeed = NextRandomSeed(f_PaintSeed);
        fairwayRandom = f_PaintSeed >> PAINT_RANDOM_SHIFT;
        if ((x + FAIRWAY_STRIPE_OFFSET) & FAIRWAY_STRIPE_WIDTH) {
          color = (((fairwayRandom & 3) + 14) << RGBA16_RED_SHIFT) | 0x4CE;
        } else {
          color = ((fairwayRandom & 3) << RGBA16_RED_SHIFT) | 0x658E;
        }

        paintedCode = TERRAIN_CODE_FAIRWAY;
      } else {
        color = 0x8615;
        paintedCode = TERRAIN_CODE_SEMI_ROUGH;
      }

      break;
    case TERRAIN_CODE_ROUGH:
      if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
        /* The random value is unused */
        f_PaintSeed = NextRandomSeed(f_PaintSeed);
        color = 0xDFB7;
        paintedCode = TERRAIN_CODE_ROUGH;
      } else {
        color = 0xCE33;
        paintedCode = TERRAIN_CODE_ROUGH;
      }

      break;
    case TERRAIN_CODE_BUNKER:
      color = 0xE739;
      paintedCode = TERRAIN_CODE_BUNKER;
      break;
    case TERRAIN_CODE_UNK3:
      if (nearestCode == TERRAIN_CODE_UNK3 ||
          nearestDistance >= TERRAIN_BORDER_DISTANCE) {
        paintedCode = TERRAIN_CODE_UNK3;
        color = 0x3E;
      } else {
        /* Both colors are the same in the original. The arms' differing
         * order keeps GCC from merging them, which would drop the test */
        if (nearestDistance == 1) {
          paintedCode = TERRAIN_CODE_CART_WAY;
          color = 0x94A5;
        } else {
          color = 0x94A5;
          paintedCode = TERRAIN_CODE_CART_WAY;
        }
      }

      break;
    case TERRAIN_CODE_GREEN:
      if (IsGreenEdge(code, nearestCode, nearestDistance)) {
        color = 0x2587;
        paintedCode = TERRAIN_CODE_GREEN_EDGE;
      } else if (nearestCode == TERRAIN_CODE_GREEN ||
          nearestDistance >= TERRAIN_BORDER_DISTANCE) {
        if (z & GREEN_CHECK_SIZE) {
          color = (x & GREEN_CHECK_SIZE) ? 0x2F84 : 0x4680;
        } else {
          color = (x & GREEN_CHECK_SIZE) ? 0x3F02 : 0x3704;
        }

        paintedCode = TERRAIN_CODE_GREEN;
      } else {
        color = 0x2689;
        paintedCode = TERRAIN_CODE_FAIRWAY;
      }

      break;
    case TERRAIN_CODE_CART_WAY:
      color = 0x7BDF;
      paintedCode = TERRAIN_CODE_CART_WAY;
      break;
    case TERRAIN_CODE_OUT_OF_BOUNDS:
      color = 0xBEEF;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
    case TERRAIN_CODE_TEE_GROUND:
      color = 0x2688;
      paintedCode = TERRAIN_CODE_TEE_GROUND;
      break;
    default:
      color = 0xFFFF;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
  }

  return (color << 16) | paintedCode;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", PaintPaleRoughCourse);
#endif

/*
 * The paint logic of Shy Guy Desert: like PaintMainCourse with other
 * colors, except that deep rough is painted as waste area and rough away
 * from fairway is mottled with other random colors
 */
s32 PaintShyGuyDesert(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
  u32 color;
  s32 paintedCode;
  s32 random;
  s32 fairwayRandom;
  s32 green;
  u32 red;

  switch (code) {
    case TERRAIN_CODE_DEEP_ROUGH:
      color = 0xE5D2;
      paintedCode = TERRAIN_CODE_WASTE_AREA;
      break;
    case TERRAIN_CODE_ROCK:
      color = 0x4983;
      paintedCode = TERRAIN_CODE_ROCK;
      break;
    case TERRAIN_CODE_FAIRWAY:
      if (nearestDistance >= TERRAIN_BORDER_DISTANCE ||
          nearestCode == TERRAIN_CODE_GREEN ||
          nearestCode == TERRAIN_CODE_FAIRWAY) {
        f_PaintSeed = NextRandomSeed(f_PaintSeed);
        fairwayRandom = f_PaintSeed >> PAINT_RANDOM_SHIFT;
        if ((x + FAIRWAY_STRIPE_OFFSET) & FAIRWAY_STRIPE_WIDTH) {
          color = (((fairwayRandom & 3) + 19) << RGBA16_RED_SHIFT) | 0x688;
        } else {
          color = (((fairwayRandom & 3) + 14) << RGBA16_RED_SHIFT) | 0x5C6;
        }

        paintedCode = TERRAIN_CODE_FAIRWAY;
      } else {
        color = 0x9E89;
        paintedCode = TERRAIN_CODE_SEMI_ROUGH;
      }

      break;
    case TERRAIN_CODE_ROUGH:
      if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
        f_PaintSeed = NextRandomSeed(f_PaintSeed);
        red = ((f_PaintSeed >> PAINT_RANDOM_SHIFT) & 3) + 10;
        green = red + 3;
        color = (red << RGBA16_RED_SHIFT) | (green << RGBA16_GREEN_SHIFT) | 5;
        paintedCode = TERRAIN_CODE_ROUGH;
      } else {
        color = 0xC81;
        paintedCode = TERRAIN_CODE_ROUGH;
      }

      break;
    case TERRAIN_CODE_BUNKER:
      color = 0xE721;
      paintedCode = TERRAIN_CODE_BUNKER;
      break;
    case TERRAIN_CODE_UNK3:
      paintedCode = TERRAIN_CODE_UNK3;
      f_PaintSeed = NextRandomSeed(f_PaintSeed);
      random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
      color = (((random & 4) + 2) << RGBA16_RED_SHIFT) |
          ((4 - ((random >> 5) & 3)) << RGBA16_GREEN_SHIFT) | 1;
      break;
    case TERRAIN_CODE_GREEN:
      if (IsGreenEdge(code, nearestCode, nearestDistance)) {
        color = 0x7E4B;
        paintedCode = TERRAIN_CODE_GREEN_EDGE;
      } else if (nearestCode == TERRAIN_CODE_GREEN ||
          nearestDistance >= TERRAIN_BORDER_DISTANCE) {
        if (z & GREEN_CHECK_SIZE) {
          color = (x & GREEN_CHECK_SIZE) ? 0x7E4A : 0x7746;
        } else {
          color = (x & GREEN_CHECK_SIZE) ? 0x8E88 : 0x8606;
        }

        paintedCode = TERRAIN_CODE_GREEN;
      } else {
        color = 0x2689;
        paintedCode = TERRAIN_CODE_FAIRWAY;
      }

      break;
    case TERRAIN_CODE_CART_WAY:
      color = 0x7BDF;
      paintedCode = TERRAIN_CODE_CART_WAY;
      break;
    case TERRAIN_CODE_OUT_OF_BOUNDS:
      color = 0xD6A6;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
    case TERRAIN_CODE_TEE_GROUND:
      color = 0x7E4A;
      paintedCode = TERRAIN_CODE_TEE_GROUND;
      break;
    default:
      color = 0xFFFF;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
  }

  return (color << 16) | paintedCode;
}

/* The paint logic of Boo Valley: PaintMainCourse with other bunker and
 * out-of-bounds colors */
s32 PaintBooValley(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
  u32 color;
  s32 paintedCode;
  s32 random;
  s32 fairwayRandom;
  s32 green;
  u32 red;

  switch (code) {
    case TERRAIN_CODE_DEEP_ROUGH:
      color = 0x4481;
      paintedCode = TERRAIN_CODE_DEEP_ROUGH;
      break;
    case TERRAIN_CODE_ROCK:
      color = 0x4983;
      paintedCode = TERRAIN_CODE_ROCK;
      break;
    case TERRAIN_CODE_FAIRWAY:
      if (nearestDistance >= TERRAIN_BORDER_DISTANCE ||
          nearestCode == TERRAIN_CODE_GREEN ||
          nearestCode == TERRAIN_CODE_FAIRWAY) {
        f_PaintSeed = NextRandomSeed(f_PaintSeed);
        fairwayRandom = f_PaintSeed >> PAINT_RANDOM_SHIFT;
        if ((x + FAIRWAY_STRIPE_OFFSET) & FAIRWAY_STRIPE_WIDTH) {
          color = (((fairwayRandom & 3) + 9) << RGBA16_RED_SHIFT) | 0x4C4;
        } else {
          color = (((fairwayRandom & 3) + 7) << RGBA16_RED_SHIFT) | 0x584;
        }

        paintedCode = TERRAIN_CODE_FAIRWAY;
      } else {
        color = 0x4605;
        paintedCode = TERRAIN_CODE_SEMI_ROUGH;
      }

      break;
    case TERRAIN_CODE_ROUGH:
      if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
        f_PaintSeed = NextRandomSeed(f_PaintSeed);
        red = (f_PaintSeed >> PAINT_RANDOM_SHIFT) & 7;
        green = 16 - (red >> 1);
        color = (red << RGBA16_RED_SHIFT) | (green << RGBA16_GREEN_SHIFT) | 1;
        paintedCode = TERRAIN_CODE_ROUGH;
      } else {
        color = 0xC81;
        paintedCode = TERRAIN_CODE_ROUGH;
      }

      break;
    case TERRAIN_CODE_BUNKER:
      color = 0xE721;
      paintedCode = TERRAIN_CODE_BUNKER;
      break;
    case TERRAIN_CODE_UNK3:
      paintedCode = TERRAIN_CODE_UNK3;
      f_PaintSeed = NextRandomSeed(f_PaintSeed);
      random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
      color = (((random & 4) + 2) << RGBA16_RED_SHIFT) |
          ((4 - ((random >> 5) & 3)) << RGBA16_GREEN_SHIFT) | 1;
      break;
    case TERRAIN_CODE_GREEN:
      if (IsGreenEdge(code, nearestCode, nearestDistance)) {
        color = 0x2587;
        paintedCode = TERRAIN_CODE_GREEN_EDGE;
      } else if (nearestCode == TERRAIN_CODE_GREEN ||
          nearestDistance >= TERRAIN_BORDER_DISTANCE) {
        if (z & GREEN_CHECK_SIZE) {
          color = (x & GREEN_CHECK_SIZE) ? 0x2F84 : 0x4580;
        } else {
          color = (x & GREEN_CHECK_SIZE) ? 0x3E42 : 0x3704;
        }

        paintedCode = TERRAIN_CODE_GREEN;
      } else {
        color = 0x2689;
        paintedCode = TERRAIN_CODE_FAIRWAY;
      }

      break;
    case TERRAIN_CODE_CART_WAY:
      color = 0x7BDF;
      paintedCode = TERRAIN_CODE_CART_WAY;
      break;
    case TERRAIN_CODE_OUT_OF_BOUNDS:
      color = 0x2A40;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
    case TERRAIN_CODE_TEE_GROUND:
      color = 0x2688;
      paintedCode = TERRAIN_CODE_TEE_GROUND;
      break;
    default:
      color = 0xFFFF;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
  }

  return (color << 16) | paintedCode;
}

/* The paint logic of Yoshi's Island: PaintMainCourse with other colors */
s32 PaintYoshisIsland(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
  u32 color;
  s32 paintedCode;
  s32 random;
  s32 fairwayRandom;
  s32 green;
  u32 red;

  switch (code) {
    case TERRAIN_CODE_DEEP_ROUGH:
      color = 0x448F;
      paintedCode = TERRAIN_CODE_DEEP_ROUGH;
      break;
    case TERRAIN_CODE_ROCK:
      color = 0x4983;
      paintedCode = TERRAIN_CODE_ROCK;
      break;
    case TERRAIN_CODE_FAIRWAY:
      if (nearestDistance >= TERRAIN_BORDER_DISTANCE ||
          nearestCode == TERRAIN_CODE_GREEN ||
          nearestCode == TERRAIN_CODE_FAIRWAY) {
        f_PaintSeed = NextRandomSeed(f_PaintSeed);
        fairwayRandom = f_PaintSeed >> PAINT_RANDOM_SHIFT;
        if ((x + FAIRWAY_STRIPE_OFFSET) & FAIRWAY_STRIPE_WIDTH) {
          color = (((fairwayRandom & 3) + 7) << RGBA16_RED_SHIFT) | 0x4D4;
        } else {
          color = (((fairwayRandom & 3) + 7) << RGBA16_RED_SHIFT) | 0x590;
        }

        paintedCode = TERRAIN_CODE_FAIRWAY;
      } else {
        color = 0x4591;
        paintedCode = TERRAIN_CODE_SEMI_ROUGH;
      }

      break;
    case TERRAIN_CODE_ROUGH:
      if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
        f_PaintSeed = NextRandomSeed(f_PaintSeed);
        red = (f_PaintSeed >> PAINT_RANDOM_SHIFT) & 7;
        green = 16 - (red >> 1);
        color = (red << RGBA16_RED_SHIFT) | (green << RGBA16_GREEN_SHIFT) | 0xD;
        paintedCode = TERRAIN_CODE_ROUGH;
      } else {
        color = 0xC81;
        paintedCode = TERRAIN_CODE_ROUGH;
      }

      break;
    case TERRAIN_CODE_BUNKER:
      color = 0xEF37;
      paintedCode = TERRAIN_CODE_BUNKER;
      break;
    case TERRAIN_CODE_UNK3:
      paintedCode = TERRAIN_CODE_UNK3;
      f_PaintSeed = NextRandomSeed(f_PaintSeed);
      random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
      color = (((random & 4) + 2) << RGBA16_RED_SHIFT) |
          ((4 - ((random >> 5) & 3)) << RGBA16_GREEN_SHIFT) | 1;
      break;
    case TERRAIN_CODE_GREEN:
      if (IsGreenEdge(code, nearestCode, nearestDistance)) {
        color = 0x3C04;
        paintedCode = TERRAIN_CODE_GREEN_EDGE;
      } else if (nearestCode == TERRAIN_CODE_GREEN ||
          nearestDistance >= TERRAIN_BORDER_DISTANCE) {
        if (z & GREEN_CHECK_SIZE) {
          color = (x & GREEN_CHECK_SIZE) ? 0x2E04 : 0x4480;
        } else {
          color = (x & GREEN_CHECK_SIZE) ? 0x3CC2 : 0x3584;
        }

        paintedCode = TERRAIN_CODE_GREEN;
      } else {
        color = 0x26AF;
        paintedCode = TERRAIN_CODE_FAIRWAY;
      }

      break;
    case TERRAIN_CODE_CART_WAY:
      color = 0x7BDF;
      paintedCode = TERRAIN_CODE_CART_WAY;
      break;
    case TERRAIN_CODE_OUT_OF_BOUNDS:
      color = 0x2A49;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
    case TERRAIN_CODE_TEE_GROUND:
      color = 0x269C;
      paintedCode = TERRAIN_CODE_TEE_GROUND;
      break;
    default:
      color = 0xFFFF;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
  }

  return (color << 16) | paintedCode;
}

/*
 * PaintMainCourse with PaintFirstMiniGolfCourse's mottling of
 * TERRAIN_CODE_UNK3. Neither the paint-logic table nor any code refers to it.
 */
s32 PaintUnlistedCourse(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
  u32 color;
  s32 paintedCode;
  s32 random;
  s32 fairwayRandom;
  s32 green;
  u32 red;

  switch (code) {
    case TERRAIN_CODE_DEEP_ROUGH:
      color = 0x4481;
      paintedCode = TERRAIN_CODE_DEEP_ROUGH;
      break;
    case TERRAIN_CODE_ROCK:
      color = 0x4983;
      paintedCode = TERRAIN_CODE_ROCK;
      break;
    case TERRAIN_CODE_FAIRWAY:
      if (nearestDistance >= TERRAIN_BORDER_DISTANCE ||
          nearestCode == TERRAIN_CODE_GREEN ||
          nearestCode == TERRAIN_CODE_FAIRWAY) {
        f_PaintSeed = NextRandomSeed(f_PaintSeed);
        fairwayRandom = f_PaintSeed >> PAINT_RANDOM_SHIFT;
        if ((x + FAIRWAY_STRIPE_OFFSET) & FAIRWAY_STRIPE_WIDTH) {
          color = (((fairwayRandom & 3) + 9) << RGBA16_RED_SHIFT) | 0x4C4;
        } else {
          color = (((fairwayRandom & 3) + 7) << RGBA16_RED_SHIFT) | 0x584;
        }

        paintedCode = TERRAIN_CODE_FAIRWAY;
      } else {
        color = 0x4605;
        paintedCode = TERRAIN_CODE_SEMI_ROUGH;
      }

      break;
    case TERRAIN_CODE_ROUGH:
      if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
        f_PaintSeed = NextRandomSeed(f_PaintSeed);
        red = (f_PaintSeed >> PAINT_RANDOM_SHIFT) & 7;
        green = 16 - (red >> 1);
        color = (red << RGBA16_RED_SHIFT) | (green << RGBA16_GREEN_SHIFT) | 1;
        paintedCode = TERRAIN_CODE_ROUGH;
      } else {
        color = 0xC81;
        paintedCode = TERRAIN_CODE_ROUGH;
      }

      break;
    case TERRAIN_CODE_BUNKER:
      color = 0xE739;
      paintedCode = TERRAIN_CODE_BUNKER;
      break;
    case TERRAIN_CODE_UNK3:
      paintedCode = TERRAIN_CODE_UNK3;
      f_PaintSeed = NextRandomSeed(f_PaintSeed);
      random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
      color = (((random & 7) + 4) << RGBA16_RED_SHIFT) |
          ((4 - ((random >> 5) & 3)) << RGBA16_GREEN_SHIFT);
      break;
    case TERRAIN_CODE_GREEN:
      if (IsGreenEdge(code, nearestCode, nearestDistance)) {
        color = 0x2587;
        paintedCode = TERRAIN_CODE_GREEN_EDGE;
      } else if (nearestCode == TERRAIN_CODE_GREEN ||
          nearestDistance >= TERRAIN_BORDER_DISTANCE) {
        if (z & GREEN_CHECK_SIZE) {
          color = (x & GREEN_CHECK_SIZE) ? 0x2F84 : 0x4580;
        } else {
          color = (x & GREEN_CHECK_SIZE) ? 0x3E42 : 0x3704;
        }

        paintedCode = TERRAIN_CODE_GREEN;
      } else {
        color = 0x2689;
        paintedCode = TERRAIN_CODE_FAIRWAY;
      }

      break;
    case TERRAIN_CODE_CART_WAY:
      color = 0x7BDF;
      paintedCode = TERRAIN_CODE_CART_WAY;
      break;
    case TERRAIN_CODE_OUT_OF_BOUNDS:
      color = 0x2A41;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
    case TERRAIN_CODE_TEE_GROUND:
      color = 0x2688;
      paintedCode = TERRAIN_CODE_TEE_GROUND;
      break;
    default:
      color = 0xFFFF;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
  }

  return (color << 16) | paintedCode;
}

/*
 * The paint logic of the first course of each mini-golf set. It returns a
 * cell's color in the upper half and its painted terrain code in the
 * lower: greens are striped and edged, TERRAIN_CODE_UNK3 is mottled with
 * random colors, and anything other than out of bounds is painted white as
 * out of bounds.
 */
s32 PaintFirstMiniGolfCourse(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
  u32 color;
  s32 paintedCode;
  Boolean isOtherNearest;
  s32 random;

  switch (code) {
    case TERRAIN_CODE_UNK3:
      paintedCode = TERRAIN_CODE_UNK3;
      f_PaintSeed = NextRandomSeed(f_PaintSeed);
      random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
      color = (((random & 7) + 4) << RGBA16_RED_SHIFT) |
          ((4 - ((random >> 5) & 3)) << RGBA16_GREEN_SHIFT);
      break;
    case TERRAIN_CODE_GREEN:
      /* The assignment in the test is there to match */
      if ((isOtherNearest = nearestCode != TERRAIN_CODE_GREEN) &&
          nearestDistance < TERRAIN_BORDER_DISTANCE) {
        color = 0x650B;
        paintedCode = TERRAIN_CODE_GREEN;
      } else if (nearestCode == TERRAIN_CODE_GREEN ||
          nearestDistance >= TERRAIN_BORDER_DISTANCE) {
        if (z & MINI_GOLF_CHECK_DEPTH) {
          color = (x & MINI_GOLF_CHECK_WIDTH) ? 0xCE72 : 0x650B;
        } else {
          color = (x & MINI_GOLF_CHECK_WIDTH) ? 0x650B : 0xCE72;
        }

        paintedCode = TERRAIN_CODE_GREEN;
      } else {
        color = 0x2689;
        paintedCode = TERRAIN_CODE_FAIRWAY;
      }

      break;
    case TERRAIN_CODE_OUT_OF_BOUNDS:
      color = 0x3CC;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
    default:
      color = 0xFFFF;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
  }

  return (color << 16) | paintedCode;
}

/*
 * The paint logic of the second course of each mini-golf set: like
 * PaintFirstMiniGolfCourse with other colors, except that greens are painted
 * as fairway
 */
s32 PaintSecondMiniGolfCourse(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
  u32 color;
  s32 paintedCode;
  Boolean isOtherNearest;
  s32 random;

  switch (code) {
    case TERRAIN_CODE_UNK3:
      paintedCode = TERRAIN_CODE_UNK3;
      f_PaintSeed = NextRandomSeed(f_PaintSeed);
      random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
      color = (((random & 4) + 2) << RGBA16_RED_SHIFT) |
          ((4 - ((random >> 5) & 3)) << RGBA16_GREEN_SHIFT) | 1;
      break;
    case TERRAIN_CODE_GREEN:
      /* The assignment in the test is there to match */
      if ((isOtherNearest = nearestCode != TERRAIN_CODE_GREEN) &&
          nearestDistance < TERRAIN_BORDER_DISTANCE) {
        color = 0x4C73;
        paintedCode = TERRAIN_CODE_FAIRWAY;
      } else if (nearestCode == TERRAIN_CODE_GREEN ||
          nearestDistance >= TERRAIN_BORDER_DISTANCE) {
        if (z & MINI_GOLF_CHECK_DEPTH) {
          color = (x & MINI_GOLF_CHECK_WIDTH) ? 0x4C72 : 0xBEB4;
        } else {
          color = (x & MINI_GOLF_CHECK_WIDTH) ? 0xBEB4 : 0x4C72;
        }

        paintedCode = TERRAIN_CODE_FAIRWAY;
      } else {
        color = 0x2689;
        paintedCode = TERRAIN_CODE_FAIRWAY;
      }

      break;
    case TERRAIN_CODE_OUT_OF_BOUNDS:
      color = 0x55E;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
    default:
      color = 0xFFFF;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
  }

  return (color << 16) | paintedCode;
}

/*
 * The paint logic of the third course of each mini-golf set: like
 * PaintFirstMiniGolfCourse with other colors, except that greens are painted
 * as cart way and other terrain is painted red
 */
s32 PaintThirdMiniGolfCourse(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
  u32 color;
  s32 paintedCode;
  s32 random;

  switch (code) {
    case TERRAIN_CODE_UNK3:
      paintedCode = TERRAIN_CODE_UNK3;
      f_PaintSeed = NextRandomSeed(f_PaintSeed);
      random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
      color = (((random & 4) + 2) << RGBA16_RED_SHIFT) |
          ((4 - ((random >> 5) & 3)) << RGBA16_GREEN_SHIFT) | 1;
      break;
    case TERRAIN_CODE_GREEN:
      if (nearestCode != TERRAIN_CODE_GREEN &&
          nearestDistance < TERRAIN_BORDER_DISTANCE) {
        color = 0xDC61;
        paintedCode = TERRAIN_CODE_CART_WAY;
      } else {
        if (z & MINI_GOLF_CHECK_DEPTH) {
          color = (x & MINI_GOLF_CHECK_WIDTH) ? 0xDC60 : 0xCE72;
        } else {
          color = (x & MINI_GOLF_CHECK_WIDTH) ? 0xCE72 : 0xDC60;
        }

        paintedCode = TERRAIN_CODE_CART_WAY;
      }

      break;
    case TERRAIN_CODE_OUT_OF_BOUNDS:
      color = 0x7B80;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
    default:
      color = 0xF801;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
  }

  return (color << 16) | paintedCode;
}

/*
 * The paint logic of the driving range. It returns a cell's color in the
 * upper half and its painted terrain code in the lower: fairway and greens
 * are striped, rough away from fairway and TERRAIN_CODE_UNK3 are mottled
 * with random colors, and greens are edged as green edge.
 */
s32 PaintDrivingRange(
    s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
  u32 color;
  s32 paintedCode;
  s32 random;
  s32 green;
  u32 red;

  switch (code) {
    case TERRAIN_CODE_DEEP_ROUGH:
      color = 0x4481;
      paintedCode = TERRAIN_CODE_DEEP_ROUGH;
      break;
    case TERRAIN_CODE_ROCK:
      color = 0x4983;
      paintedCode = TERRAIN_CODE_ROCK;
      break;
    case TERRAIN_CODE_FAIRWAY:
      color = ((x + FAIRWAY_STRIPE_OFFSET) & FAIRWAY_STRIPE_WIDTH) ? 0x5CC4
                                                                   : 0x4584;
      paintedCode = TERRAIN_CODE_FAIRWAY;
      break;
    case TERRAIN_CODE_ROUGH:
      if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
        f_PaintSeed = NextRandomSeed(f_PaintSeed);
        red = (f_PaintSeed >> PAINT_RANDOM_SHIFT) & 7;
        green = 16 - (red >> 1);
        color = (red << RGBA16_RED_SHIFT) | (green << RGBA16_GREEN_SHIFT) | 1;
        paintedCode = TERRAIN_CODE_ROUGH;
      } else {
        color = 0xC81;
        paintedCode = TERRAIN_CODE_ROUGH;
      }

      break;
    case TERRAIN_CODE_BUNKER:
      color = 0xE739;
      paintedCode = TERRAIN_CODE_BUNKER;
      break;
    case TERRAIN_CODE_UNK3:
      paintedCode = TERRAIN_CODE_UNK3;
      f_PaintSeed = NextRandomSeed(f_PaintSeed);
      random = f_PaintSeed >> PAINT_RANDOM_SHIFT;
      color = (((random & 4) + 2) << RGBA16_RED_SHIFT) |
          ((4 - ((random >> 5) & 3)) << RGBA16_GREEN_SHIFT) | 1;
      break;
    case TERRAIN_CODE_GREEN:
      if (IsGreenEdge(code, nearestCode, nearestDistance)) {
        color = 0x2587;
        paintedCode = TERRAIN_CODE_GREEN_EDGE;
      } else if (nearestCode == TERRAIN_CODE_GREEN ||
          nearestDistance >= TERRAIN_BORDER_DISTANCE) {
        if (z & GREEN_CHECK_SIZE) {
          color = (x & GREEN_CHECK_SIZE) ? 0x2F84 : 0x4580;
        } else {
          color = (x & GREEN_CHECK_SIZE) ? 0x3E42 : 0x3704;
        }

        paintedCode = TERRAIN_CODE_GREEN;
      } else {
        color = 0x2689;
        paintedCode = TERRAIN_CODE_FAIRWAY;
      }

      break;
    case TERRAIN_CODE_CART_WAY:
      color = 0x7BDF;
      paintedCode = TERRAIN_CODE_CART_WAY;
      break;
    case TERRAIN_CODE_OUT_OF_BOUNDS:
      color = 0x2A41;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
    case TERRAIN_CODE_TEE_GROUND:
      color = 0x2688;
      paintedCode = TERRAIN_CODE_TEE_GROUND;
      break;
    default:
      color = 0xFFFF;
      paintedCode = TERRAIN_CODE_OUT_OF_BOUNDS;
      break;
  }

  return (color << 16) | paintedCode;
}

/*
 * Returns the map texel at a column and row of the whole hole's map. Adding
 * the offsets in bytes, the texel's first, is closest to the original:
 * indexing the tiles or texels, or adding the tile offset to the base first,
 * lets GCC fold the base address in last or first.
 */
static inline u16* GetMapTexel(s32 column, s32 row) {
  /* tileRow is there to match */
  s32 tileRow = row >> MAP_TILE_SHIFT;
  s32 tileOffset = ((column / GROUND_TILE_TEXEL_SPAN) +
                       (tileRow * GROUND_TILE_COLUMN_COUNT)) *
      sizeof(GroundTile);
  s32 texelOffset = (((row & MAP_TILE_MASK) * GROUND_TILE_TEXEL_SPAN) +
                        (column & MAP_TILE_MASK)) *
      sizeof(u16);

  return (u16*)((u8*)g_WorkArea.hole.tiles + (texelOffset + tileOffset));
}

/* Returns the map texel at a column and row, or 0 off the map */
static inline u16 ReadMapTexel(s32 column, s32 row) {
  /* The separate tests are there to match */
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

/*
 * Encodes the map's texels as terrain codes, a byte each, in the hole file
 * buffer. Doesn't match: the original divides the column before
 * shifting the row and adds the work area's address to the tile offset before
 * the texel offset, where this adds the offsets first. Dividing the column
 * into its own local first gives 25 differing words, and a local pointer to
 * the tiles 31 to 47. 7 words differ.
 */
#ifdef NON_MATCHING
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

#else
INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", EncodeCodeMap);
#endif

/* Sets the map texel at a column and row, unless it is off the map */
static inline void WriteMapTexel(s32 column, s32 row, u16 color) {
  /* The separate tests are there to match */
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

/*
 * Clears the map, reads a code map from the host and paints the map's texels
 * with the colors of its terrain codes. Doesn't match: for the same
 * reason as EncodeCodeMap. 6 words differ.
 */
#ifdef NON_MATCHING
void LoadCodeMap(char* path) {
  u32 size;
  s32 tileIndex;
  u32 texelIndex;
  s32 column;
  s32 row;
  u16 color;

  f_BuildCursor = HOLE_FILE_BUFFER;
  f_BuildBuffer = f_BuildCursor;
  if (ReadHostFile(path, &size) == HOST_FILE_OK &&
      size == MAP_COLUMN_COUNT * MAP_ROW_COUNT) {
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
#else
INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", LoadCodeMap);
#endif

/* Repaints the ground tiles from the code map saved before the last clean-up */
void LoadBackupCodeMap(void) {
  LoadCodeMap("c:/golf64/attr.tmp");
}

/* Repaints the ground tiles from the tool's code map */
void LoadToolCodeMap(void) {
  LoadCodeMap("tool/golf64.att");
  f_IsCodeMapCleaned = FALSE;
}

/* Returns the code at a column and row of the code map, pointing the build
 * cursor at it */
static inline u8 GetCode(s32 column, s32 row) {
  f_BuildCursor = &f_BuildBuffer[(row * MAP_COLUMN_COUNT) + column];
  return *f_BuildCursor;
}

/* Sets the code at a column and row of the code map, pointing the build cursor
 * at it */
static inline void SetCode(s32 column, s32 row, u8 code) {
  f_BuildCursor = &f_BuildBuffer[(row * MAP_COLUMN_COUNT) + column];
  *f_BuildCursor = code;
}

/*
 * Fills the code at a column and row of the code map with its neighbors'
 * code, if they are equal and it differs. The neighbors are a step away
 * each side.
 */
static inline void FillCodeGap(
    s32 column, s32 row, s32 columnStep, s32 rowStep) {
  u8 code = GetCode(column, row);
  u8 previousCode = GetCode(column - columnStep, row - rowStep);

  /* The second read of the previous code is there to match */
  u8 rereadPreviousCode = GetCode(column - columnStep, row - rowStep);
  u8 nextCode = GetCode(column + columnStep, row + rowStep);

  if (code != previousCode && rereadPreviousCode == nextCode) {
    SetCode(column, row, GetCode(column + columnStep, row + rowStep));
  }
}

/*
 * Encodes the map as EncodeCodeMap does, then runs the clean-up passes, each
 * filling the codes that differ from two equal neighbors, along the row
 * and then down the column. There are no passes, so it only encodes the map.
 * Doesn't match: the encoding has EncodeCodeMap's texel address order,
 * and the passes compute each neighbor's index from one strength-reduced
 * index and read the previous code's address once, where the original
 * computes it for both reads. 149 words differ.
 */
#ifdef NON_MATCHING
void CleanCodeMap(void) {
  s32 passCount = 0;
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

  for (; passCount != 0; passCount--) {
    for (row = 0; row < MAP_ROW_COUNT; row++) {
      for (column = 0; column < MAP_COLUMN_COUNT; column++) {
        if (column > 0 && column < MAP_COLUMN_COUNT - 1) {
          FillCodeGap(column, row, 1, 0);
        }

        if (row > 0 && row < MAP_ROW_COUNT - 1) {
          FillCodeGap(column, row, 0, 1);
        }
      }
    }
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", CleanCodeMap);
#endif

/* Does nothing */
void FinishCodeMapCleanup(void) {}

/* The byte offset of a map texel's terrain code cell */
static inline s32 GetCellOffset(u32 column, u32 row) {
  return ((column / TERRAIN_CODE_CELL_SPAN) * TERRAIN_CODE_CELL_SIZE) +
      ((row / TERRAIN_CODE_CELL_SPAN) * TERRAIN_CODE_CELL_ROW_SIZE);
}

/* The cell at a byte offset from GetCellOffset */
#define CellAt(cellOffset) \
  (*(s16*)AddressPast(g_TerrainCodeCells, (cellOffset)))

/* The offset of a map texel's code in the packed codes */
static inline s32 GetPackedCodeOffset(s32 block, u32 column, u32 row) {
  return ((column % TERRAIN_CODE_CELL_SPAN) / 2) +
      ((row % TERRAIN_CODE_CELL_SPAN) * (TERRAIN_CODE_CELL_SPAN / 2)) +
      (block * TERRAIN_CODE_BLOCK_SIZE);
}

/*
 * The offset of a map texel's code in the packed codes of its cell, given
 * the cell's block marker
 */
static inline s32 GetCellCodeOffset(s32 cell, u32 column, u32 row) {
  return ((column % TERRAIN_CODE_CELL_SPAN) / 2) +
      ((row % TERRAIN_CODE_CELL_SPAN) * (TERRAIN_CODE_CELL_SPAN / 2)) +
      ((cell & (TERRAIN_CODE_CELL_BLOCK - 1)) * TERRAIN_CODE_BLOCK_SIZE);
}

/*
 * Sets a map texel's code in a block of packed codes, two to a byte, with
 * the even column's code in the high bits
 */
static inline void SetPackedTerrainCode(
    s32 block, u32 column, u32 row, s32 code) {
  if (column & 1) {
    g_WorkArea.hole.terrainCodes[GetPackedCodeOffset(block, column, row)] =
        (g_WorkArea.hole.terrainCodes[GetPackedCodeOffset(block, column, row)] &
            EVEN_COLUMN_CODE_MASK) |
        code;
  } else {
    g_WorkArea.hole.terrainCodes[GetPackedCodeOffset(block, column, row)] =
        (g_WorkArea.hole.terrainCodes[GetPackedCodeOffset(block, column, row)] &
            TERRAIN_CODE_MASK) |
        (code << TERRAIN_CODE_BITS);
  }
}

/*
 * Stores a map texel's terrain code in its cell, giving the cell a block of
 * packed codes once it holds more than one. Once the blocks run out, cells
 * keep their first code.
 */
static inline void SetCellTerrainCode(u32 column, u32 row, s32 code) {
  s32 cellOffset = GetCellOffset(column, row);
  s32 cell = CellAt(cellOffset);
  s32 block;
  u32 fill;
  s32 i;
  u32* word;

  if (cell == code) {
    return;
  }

  if (cell == NO_CELL_TERRAIN_CODE) {
    CellAt(cellOffset) = code;
  } else if (cell >= 0) {
    if (g_TerrainCodeBlockCount < TERRAIN_CODE_BLOCK_COUNT) {
      /* Repeats the cell's code in every half byte of a word */
      fill = (cell << TERRAIN_CODE_BITS) | cell;
      block = g_TerrainCodeBlockCount++;
      fill |= fill << BITS_PER_BYTE;
      fill |= fill << (BITS_PER_BYTE * 2);
      word =
          (u32*)&g_WorkArea.hole.terrainCodes[block * TERRAIN_CODE_BLOCK_SIZE];
      for (i = 0; i < TERRAIN_CODE_BLOCK_WORD_COUNT; i++) {
        *word++ = fill;
      }

      /* Computing the offset again is there to match */
      cellOffset = GetCellOffset(column, row);
      CellAt(cellOffset) = block | TERRAIN_CODE_CELL_BLOCK;
      SetPackedTerrainCode(block, column, row, code);
    }
  } else {
    /* The separate stores are there to match */
    if (column & 1) {
      g_WorkArea.hole.terrainCodes[GetCellCodeOffset(cell, column, row)] =
          (g_WorkArea.hole.terrainCodes[GetCellCodeOffset(cell, column, row)] &
              EVEN_COLUMN_CODE_MASK) |
          code;
    } else {
      g_WorkArea.hole.terrainCodes[GetCellCodeOffset(cell, column, row)] =
          (g_WorkArea.hole.terrainCodes[GetCellCodeOffset(cell, column, row)] &
              TERRAIN_CODE_MASK) |
          (code << TERRAIN_CODE_BITS);
    }
  }
}

/*
 * Paints the map's texels from the code map with the course's paint logic,
 * sets each terrain code's color, and stores the painted terrain codes in
 * their cells. Doesn't match: ours keeps the cell's offset and code in
 * other registers, copies the new block's index before repeating the code
 * across the fill word rather than after, and when the blocks run out it
 * branches to the texel loop's increment, where the original's branch-likely
 * does the increment in its delay slot. 95 words differ.
 */
#ifdef NON_MATCHING
void PaintGroundTiles(void) {
  GridOffset offsets[PAINT_OFFSET_COUNT];
  u32 codePaintings[TERRAIN_CODE_COUNT];
  u8* codes;
  s8* searchSpans;
  PaintRoutine paint = f_PaintLogics[f_PaintLogic].paint;
  u8* nextCode;
  u16* texel;
  s32 offsetCount;
  s32 offsetIndex;
  s32 column;
  s32 row;
  u32 neighborColumn;
  u32 neighborRow;
  s32 texelIndex;
  s32 code;
  s32 nearestCode;
  s32 nearestDistance;
  u32 painting;

  SetPaintSeed(0);
  ResetTerrainCodeCells();
  offsetCount =
      ListOffsetsByDistance(PAINT_OFFSET_SPAN, offsets, PAINT_OFFSET_COUNT);
  osSyncPrintf("MakeTex..");
  codes = f_BuildBuffer;
  f_BuildCursor = codes;
  nextCode = f_BuildCursor;
  searchSpans = f_PaintLogics[f_PaintLogic].searchSpans;

  /* row is there to match */
  for (row = 0; row < TERRAIN_CODE_COUNT; row++) {
    codePaintings[row] = paint(row, row, 0, 0, 0);
    g_TerrainCodeColors[codePaintings[row] & TERRAIN_CODE_MASK] =
        codePaintings[row] >> PAINTED_COLOR_SHIFT;
  }

  for (row = 0; row < MAP_ROW_COUNT; row++) {
    for (column = 0; column < MAP_COLUMN_COUNT;) {
      texel = &((u16*)&g_WorkArea.hole.tiles[(column / GROUND_TILE_TEXEL_SPAN) +
          (row / GROUND_TILE_TEXEL_SPAN * GROUND_TILE_COLUMN_COUNT)])
                  [(row & MAP_TILE_MASK) * GROUND_TILE_TEXEL_SPAN];
      for (texelIndex = 0; texelIndex < GROUND_TILE_TEXEL_SPAN; texelIndex++) {
        nearestCode = *nextCode++;
        code = nearestCode;
        nearestDistance = searchSpans[code];
        if (nearestDistance != 0) {
          if (nearestDistance < 0) {
            painting = codePaintings[code];

            /* The goto is there to match */
            goto PAINTED;
          }

          for (offsetIndex = 1; offsetIndex < offsetCount; offsetIndex++) {
            if (offsets[offsetIndex].distance >=
                nearestDistance * OFFSET_DISTANCE_SCALE) {
              break;
            }

            neighborColumn = column + offsets[offsetIndex].column;
            neighborRow = row + offsets[offsetIndex].row;
            if (neighborColumn < MAP_COLUMN_COUNT &&
                neighborRow < MAP_ROW_COUNT) {
              nearestCode =
                  codes[(neighborRow * MAP_COLUMN_COUNT) + neighborColumn];
              if (nearestCode != code) {
                break;
              }
            }
          }

          nearestDistance =
              offsets[offsetIndex].distance / OFFSET_DISTANCE_SCALE;
        }

        painting = paint(code, nearestCode, nearestDistance, column, row);

PAINTED:

        *texel++ = painting >> PAINTED_COLOR_SHIFT;
        SetCellTerrainCode(column, row, painting & TERRAIN_CODE_MASK);
        column++;
      }
    }
  }

  osSyncPrintf("Comp\n");
  f_BuildCursor = f_BuildBuffer;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", PaintGroundTiles);
#endif

/*
 * Cleans up the terrain code map and repaints the ground tiles from it,
 * or, if that was done last, restores the code map from before
 */
void ToggleCodeMapCleanup(void) {
  /* Unused; it is there to match */
  u8 unused[0x640] UNUSED;

  if (!f_IsCodeMapCleaned) {
    EncodeCodeMap();
    f_IsCodeMapCleaned = TRUE;
    CleanCodeMap();
    PaintGroundTiles();
    FinishCodeMapCleanup();
  } else {
    LoadBackupCodeMap();
    f_IsCodeMapCleaned = FALSE;
  }
}

/*
 * Sets each ground tile's center to the average of its ground points and its
 * height range to the spread of their heights, at least
 * MIN_GROUND_TILE_HEIGHT_RANGE, or FLAT_GROUND_TILE_HEIGHT_RANGE for a flat
 * tile. It returns the tile count as a float, which no caller uses.
 */
f32 MeasureGroundTiles(void) {
  s32 tileRow;
  s32 tileColumn;
  s32 tileIndex;
  s32 pointRow;
  s32 pointColumn;
  s32 sumX;
  s32 sumY;
  s32 sumZ;
  s32 lowest;
  s32 highest;
  s32 heightRange;
  Vtx* point;

  tileIndex = 0;
  for (tileRow = 0; tileRow < GROUND_TILE_ROW_COUNT; tileRow++) {
    for (tileColumn = 0; tileColumn < GROUND_TILE_COLUMN_COUNT; tileColumn++) {
      sumX = 0;
      sumY = 0;
      sumZ = 0;
      lowest = UNMEASURED_LOWEST_HEIGHT;
      highest = UNMEASURED_HIGHEST_HEIGHT;
      for (pointRow = 0; pointRow <= GROUND_TILE_POINT_SPAN; pointRow++) {
        for (pointColumn = 0; pointColumn <= GROUND_TILE_POINT_SPAN;
            pointColumn++) {
          point = GetGroundPoint(
              (tileColumn * GROUND_TILE_POINT_SPAN) + pointColumn,
              (tileRow * GROUND_TILE_POINT_SPAN) + pointRow);
          sumX += point->v.ob[AXIS_X];
          sumY += point->v.ob[AXIS_Y];
          sumZ += point->v.ob[AXIS_Z];
          if (point->v.ob[AXIS_Y] < lowest) {
            lowest = point->v.ob[AXIS_Y];
          }

          if (point->v.ob[AXIS_Y] > highest) {
            highest = point->v.ob[AXIS_Y];
          }
        }
      }

      heightRange = highest - lowest;
      if (heightRange != 0 && heightRange < MIN_GROUND_TILE_HEIGHT_RANGE) {
        heightRange = MIN_GROUND_TILE_HEIGHT_RANGE;
      }

      if (heightRange == 0) {
        heightRange = FLAT_GROUND_TILE_HEIGHT_RANGE;
      }

      g_GroundTileHeightRanges[tileIndex] = heightRange;
      g_WorkArea.hole.tiles[tileIndex].center[0] =
          sumX / GROUND_TILE_AVERAGED_POINT_COUNT;
      g_WorkArea.hole.tiles[tileIndex].center[1] =
          sumY / GROUND_TILE_AVERAGED_POINT_COUNT;
      g_WorkArea.hole.tiles[tileIndex].center[2] =
          sumZ / GROUND_TILE_AVERAGED_POINT_COUNT;
      tileIndex++;
    }
  }

  return tileIndex;
}

/*
 * Scales the hole's ground points, tile centers, billboards, tee and cup
 * with g_MiniGolfScale
 */
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

/*
 * Returns the paint logic (a PAINT_LOGIC_* value) that a course record's
 * terrain is painted with. BuildHole picks the mini-golf courses' logics
 * itself, so the mini-golf records' cases take their logics' numbers.
 */
s32 GetCoursePaintLogic(CourseRecordIndex course) {
  switch (course) {
    case COURSE_RECORD_TOAD_HIGHLANDS:
      return PAINT_LOGIC_TOAD_HIGHLANDS;
    case COURSE_RECORD_KOOPA_PARK:
      return PAINT_LOGIC_KOOPA_PARK;
    case COURSE_RECORD_SHY_GUY_DESERT:
      return PAINT_LOGIC_SHY_GUY_DESERT;
    case COURSE_RECORD_YOSHIS_ISLAND:
      return PAINT_LOGIC_YOSHIS_ISLAND;
    case COURSE_RECORD_BOO_VALLEY:
      return PAINT_LOGIC_BOO_VALLEY;
    case COURSE_RECORD_MARIOS_STAR:
      return PAINT_LOGIC_MARIOS_STAR;
    case COURSE_RECORD_LUIGIS_GARDEN:
      return PAINT_LOGIC_LUIGIS_GARDEN_FIRST;
    case COURSE_RECORD_PEACHS_CASTLE:
      return PAINT_LOGIC_LUIGIS_GARDEN_SECOND;
    case COURSE_RECORD_DRIVING_RANGE:
      return PAINT_LOGIC_DRIVING_RANGE;
  }

  return PAINT_LOGIC_TOAD_HIGHLANDS;
}

void ClearHoleBuild(void);
void OpenHoleAsset(CourseRecordIndex course, s32 hole, s8 kind);
void ReadBillboards(void);
void ReadGridVertices(void);
void ReadHoleTriangles(void);
void ReadTileMidpoints(void);

/* Does nothing; the course and hole are not read */
void FinishHolePoints(CourseRecordIndex course UNUSED, s32 hole UNUSED) {}

/*
 * Builds a hole: reads its files, either from its assets or, with
 * GAME_FLAG_READ_HOST_HOLE, from the host, then measures and paints its
 * ground tiles and places its billboards
 */
void BuildHole(s32 course, s32 hole) {
  AssetFile asset;
  char path[HOST_PATH_SIZE];
  u32 size;
  s32 pointsSize;

  /* The repeated case is there to match */
  if (course == COURSE_RECORD_SHY_GUY_DESERT) {
    FillGroundNoise(GROUND_NOISE_RANGE, LIGHT_GROUND_NOISE_BASE);
  } else if (course == COURSE_RECORD_YOSHIS_ISLAND) {
    FillGroundNoise(GROUND_NOISE_RANGE, LIGHT_GROUND_NOISE_BASE);
  } else {
    FillGroundNoise(GROUND_NOISE_RANGE, GROUND_NOISE_BASE);
  }

  if (g_GameState.isMiniGolf) {
    f_PaintLogic = g_GameState.courseIndex;
    if (g_GameState.courseIndex > LAST_MINI_GOLF_PAINT_LOGIC) {
      f_PaintLogic = LAST_MINI_GOLF_PAINT_LOGIC;
    }

    FillGroundNoise(MINI_GOLF_GROUND_NOISE_RANGE, MINI_GOLF_GROUND_NOISE_BASE);
  } else if (IsGameFlagSet(GAME_FLAG_READ_HOST_HOLE)) {
    f_PaintLogic = f_HolePaintLogics[course - 1][hole - 1];
    if (IsGameFlagSet(GAME_FLAG_UNK6A)) {
      f_PaintLogic++;
    }

    if (IsGameFlagSet(GAME_FLAG_UNK6B)) {
      f_PaintLogic++;
    }
  } else {
    f_PaintLogic = GetCoursePaintLogic(course);
  }

  osSyncPrintf("PaintLogic%d\n", f_PaintLogic);
  if (!IsGameFlagSet(GAME_FLAG_READ_HOST_HOLE)) {
    if (IsGameFlagSet(GAME_FLAG_UNK51) && hole == 0) {
      hole = COURSE_HOLE_COUNT;
    }

    ClearHoleBuild();
    OpenHoleAsset(course, hole, HOLE_ASSET_VERTICES);
    ReadGridVertices();
    OpenHoleAsset(course, hole, HOLE_ASSET_MIDPOINTS);
    ReadTileMidpoints();
    OpenHoleAsset(course, hole, HOLE_ASSET_TREES);
    ReadBillboards();
    if (!g_GameState.unk30 && g_Scene != SCENE_STAFF_ROLL) {
      if (g_GameState.gameMode == GAME_MODE_UNK12) {
        OpenAssetFile(UNK12_UNK6_ASSET, &asset);
      } else {
        OpenAssetFile(f_HoleAssetIds[course][hole].unk6, &asset);
      }

      ReadAssetFile(&g_HoleSetup, GetAssetFileSize(&asset), &asset);
    }

    if (g_GameState.gameMode == GAME_MODE_UNK12) {
      OpenAssetFile(UNK12_POINTS_ASSET, &asset);
    } else {
      OpenAssetFile(f_HoleAssetIds[course][hole].points, &asset);
    }

    pointsSize = GetAssetFileSize(&asset);
    ReadAssetFile(&g_HolePoints, pointsSize, &asset);
    osSyncPrintf("%d\n", pointsSize);
    FinishHolePoints(course, hole);
    OpenHoleAsset(course, hole, HOLE_ASSET_TRIANGLES);
    ReadHoleTriangles();
    OpenHoleAsset(course, hole, HOLE_ASSET_ATTRIBUTES);
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
  } else {
    sprintf(path, "bin/course%d/hole%d%2.2d.vtx", (int)course, (int)course,
        (int)hole);
    ReadHostFile(path, &size);
    sprintf(path, "bin/course%d/hole%d%2.2d.dtx", (int)course, (int)course,
        (int)hole);
    ReadHostFile(path, &size);
    sprintf(path, "bin/course%d/hole%d%2.2d.tre", (int)course, (int)course,
        (int)hole);
    ReadHostFile(path, &size);
    sprintf(path, "bin/course%d/hole%d%2.2d.hol", (int)course, (int)course,
        (int)hole);
    ReadHostFile(path, &size);
    sprintf(path, "bin/course%d/hole%d%2.2d.pnt", (int)course, (int)course,
        (int)hole);
    ReadHostFile(path, &size);
    sprintf(path, "bin/course%d/hole%d%2.2d.tri", (int)course, (int)course,
        (int)hole);
    if (ReadHostFile(path, &size) == HOST_FILE_OK) {
      MoveRingsToEnd();
    }

    sprintf(path, "bin/course%d/hole%d%2.2d.att", (int)course, (int)course,
        (int)hole);
    ReadHostFile(path, &size);
  }

  osSyncPrintf("@1\n");
  MeasureGroundTiles();
  f_IsCodeMapCleaned = TRUE;
  PaintGroundTiles();
  PlaceBillboards();
  osSyncPrintf("Ok!\n");
}

/*
 * Writes a hole's files to the host, the attributes as the map's terrain
 * codes, a byte each. A cleaned code map is first restored from its backup.
 * Doesn't match: for the same reason as EncodeCodeMap. 7 words differ.
 */
#ifdef NON_MATCHING
void SaveHoleFiles(s32 course, s32 hole) {
  char path[HOST_PATH_SIZE];
  s32 column;
  s32 row;

  sprintf(path, "bin/course%d/hole%d%2.2d.vtx", course, course, hole);
  WriteHostFile(path, HOLE_VERTICES_FILE_SIZE);
  sprintf(path, "bin/course%d/hole%d%2.2d.dtx", course, course, hole);
  WriteHostFile(path, HOLE_MIDPOINTS_FILE_SIZE);
  sprintf(path, "bin/course%d/hole%d%2.2d.tre", course, course, hole);
  WriteHostFile(path, HOLE_TREES_FILE_SIZE);
  sprintf(path, "bin/course%d/hole%d%2.2d.hol", course, course, hole);
  WriteHostFile(path, HOLE_UNK6_FILE_SIZE);
  sprintf(path, "bin/course%d/hole%d%2.2d.pnt", course, course, hole);
  WriteHostFile(path, HOLE_POINTS_FILE_SIZE);
  sprintf(path, "bin/course%d/hole%d%2.2d.tri", course, course, hole);
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

  sprintf(path, "bin/course%d/hole%d%2.2d.att", course, course, hole);
  if (WriteHostFile(path, MAP_COLUMN_COUNT * MAP_ROW_COUNT) == HOST_FILE_OK) {
    f_BuildCursor = f_BuildBuffer;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", SaveHoleFiles);
#endif

/*
 * MoveRingsToEnd's debug text cells: its start and end notices share one, and
 * each move it logs goes to a row before the text's buffer, so PutDebugText
 * drops it
 */
#define RING_MOVE_NOTICE_COLUMN 19
#define RING_MOVE_NOTICE_ROW 19
#define RING_MOVE_LOG_COLUMN 4
#define RING_MOVE_LOG_ROW (-141)

/*
 * Moves the rings, from the first triangle that is one, to the last
 * triangle slots
 */
void MoveRingsToEnd(void) {
  s32 i;
  s32 slot;

  PrintDebugText("Change Start", RING_MOVE_NOTICE_COLUMN, RING_MOVE_NOTICE_ROW);
  for (i = 0; i != HOLE_TRIANGLE_COUNT - RING_COUNT; i++) {
    if (g_WorkArea.hole.triangles[i].kind == HOLE_TRIANGLE_KIND_RING) {
      for (slot = HOLE_TRIANGLE_COUNT - RING_COUNT; slot != HOLE_TRIANGLE_COUNT;
          slot++, i++) {
        sprintf(g_PrintBuffer, "%d <-- %d", (int)slot, (int)i);
        PrintDebugText(g_PrintBuffer, RING_MOVE_LOG_COLUMN, RING_MOVE_LOG_ROW);
        g_WorkArea.hole.triangles[slot] = g_WorkArea.hole.triangles[i];
        bzero(&g_WorkArea.hole.triangles[i], sizeof(HoleTriangle));
      }

      PrintDebugText(
          "Change End", RING_MOVE_NOTICE_COLUMN, RING_MOVE_NOTICE_ROW);
      return;
    }
  }
}

void ClearBillboards(void);
void ClearHoleTriangles(void);
void ClearTileBorders(void);
void ResetBuildBuffer(void);

/* Clears everything a hole is built into */
void ClearHoleBuild(void) {
  ClearHolePoints();
  ClearTileBorders();
  InitTerrainTextures();
  ClearBillboards();
  ClearHoleTriangles();
  ResetBuildBuffer();
}

/* Clears the hole's points and, unless g_GameState.unk30 is set, its cup and
 * tee */
void ClearHolePoints(void) {
  s32 i;

  if (!g_GameState.unk30) {
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

  for (i = 0; i < ARRAY_COUNT(g_HolePoints.unk60); i++) {
    g_HolePoints.unk60[i][AXIS_X] = 0;
    g_HolePoints.unk60[i][AXIS_Y] = 0;
    g_HolePoints.unk60[i][AXIS_Z] = 0;

    /* The dereference is there to match */
    *g_HolePoints.routePoints[i] = 0;
    g_HolePoints.routePoints[i][AXIS_Z] = 0;
  }
}

/* Points the build buffer and its cursor at the start of the file area */
void ResetBuildBuffer(void) {
  f_BuildCursor = BUILD_BUFFER_START;
  f_BuildBuffer = f_BuildCursor;
  f_BuildCursor = f_BuildBuffer;
}

/* Marks every billboard unused */
void ClearBillboards(void) {
  s32 i;

  for (i = 0; i < BILLBOARD_COUNT; i++) {
    g_Billboards[i].x = 0;
    g_Billboards[i].y = 0;
    g_Billboards[i].z = 0;
    g_Billboards[i].textureIndex = -1;
    g_Billboards[i].unk8 = 0;
    g_Billboards[i].isNearCourseEdge = TRUE;
    g_Billboards[i].width = CLEARED_BILLBOARD_WIDTH;
    g_Billboards[i].height = CLEARED_BILLBOARD_HEIGHT;
  }
}

/* Clears the hole's triangles */
void ClearHoleTriangles(void) {
  bzero(g_WorkArea.hole.triangles, sizeof(g_WorkArea.hole.triangles));
}

/* Clears each ground tile's last texel and mirrored cells */
void ClearTileBorders(void) {
  s32 i;

  for (i = 0; i < GROUND_TILE_COUNT; i++) {
    g_WorkArea.hole.tiles[i].texels[GROUND_TILE_TEXEL_ROW_COUNT - 1]
                                   [GROUND_TILE_TEXEL_ROW_LENGTH - 1] = 0;
    g_WorkArea.hole.tiles[i].mirroredCells = 0;
  }
}

/*
 * Lays the grid vertices out in rows, each row of corners followed by the
 * points between them, with texture coordinates from their positions
 */
void InitGridVertices(void) {
  s16 x;
  s16 z;
  s32 i;

  x = 0;
  z = 0;
  for (i = 0; i < GRID_VERTEX_COUNT; i++) {
    g_WorkArea.hole.gridVertices[i].v.ob[AXIS_X] = x;
    g_WorkArea.hole.gridVertices[i].v.tc[TEXTURE_AXIS_S] =
        (x * GRID_TEXTURE_SCALE) + GRID_TEXTURE_BORDER;
    g_WorkArea.hole.gridVertices[i].v.ob[AXIS_Y] = 0;
    g_WorkArea.hole.gridVertices[i].v.ob[AXIS_Z] = z;
    g_WorkArea.hole.gridVertices[i].v.flag = 0;
    g_WorkArea.hole.gridVertices[i].v.tc[TEXTURE_AXIS_T] =
        (z * GRID_TEXTURE_SCALE) + GRID_TEXTURE_BORDER;
    g_WorkArea.hole.gridVertices[i].v.cn[0] = 0;
    g_WorkArea.hole.gridVertices[i].v.cn[1] = 0;
    g_WorkArea.hole.gridVertices[i].v.cn[2] = 0;
    g_WorkArea.hole.gridVertices[i].v.cn[3] = 0;
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

/*
 * Places each ground tile's midpoints at fixed quarter and half steps
 * across the tile, with texture coordinates from their positions
 */
void InitTileMidpoints(void) {
  /* Each midpoint's x and z offsets from its tile's origin */
  s16 offsets[TILE_MIDPOINT_COUNT][2];
  s16 x;
  s16 z;
  s32 tile;
  s32 i;

  offsets[0][0] = 0x80;
  offsets[0][1] = 0;
  offsets[1][0] = 0x180;
  offsets[1][1] = 0;
  offsets[2][0] = 0;
  offsets[2][1] = 0x80;
  offsets[3][0] = 0x200;
  offsets[3][1] = 0x80;
  offsets[4][0] = 0;
  offsets[4][1] = 0x180;
  offsets[5][0] = 0x200;
  offsets[5][1] = 0x180;
  offsets[6][0] = 0x80;
  offsets[6][1] = 0x200;
  offsets[7][0] = 0x180;
  offsets[7][1] = 0x200;
  offsets[8][0] = 0x80;
  offsets[8][1] = 0x80;
  offsets[9][0] = 0x100;
  offsets[9][1] = 0x80;
  offsets[10][0] = 0x180;
  offsets[10][1] = 0x80;
  offsets[11][0] = 0x80;
  offsets[11][1] = 0x100;
  offsets[12][0] = 0x180;
  offsets[12][1] = 0x100;
  offsets[13][0] = 0x80;
  offsets[13][1] = 0x180;
  offsets[14][0] = 0x100;
  offsets[14][1] = 0x180;
  offsets[15][0] = 0x180;
  offsets[15][1] = 0x180;
  x = 0;
  z = 0;
  for (tile = 0; tile < GROUND_TILE_COUNT; tile++) {
    for (i = 0; i < TILE_MIDPOINT_COUNT; i++) {
      g_WorkArea.hole.midpoints[tile][i].v.ob[AXIS_X] = x + offsets[i][0];
      g_WorkArea.hole.midpoints[tile][i].v.ob[AXIS_Y] = 0;
      g_WorkArea.hole.midpoints[tile][i].v.ob[AXIS_Z] = z + offsets[i][1];
      g_WorkArea.hole.midpoints[tile][i].v.flag = 0;
      g_WorkArea.hole.midpoints[tile][i].v.tc[TEXTURE_AXIS_S] =
          ((x + offsets[i][0]) * GRID_TEXTURE_SCALE) + GRID_TEXTURE_BORDER;
      g_WorkArea.hole.midpoints[tile][i].v.tc[TEXTURE_AXIS_T] =
          ((z + offsets[i][1]) * GRID_TEXTURE_SCALE) + GRID_TEXTURE_BORDER;
      g_WorkArea.hole.midpoints[tile][i].v.cn[0] = 0;
      g_WorkArea.hole.midpoints[tile][i].v.cn[1] = 0;
      g_WorkArea.hole.midpoints[tile][i].v.cn[2] = 0;
      g_WorkArea.hole.midpoints[tile][i].v.cn[3] = 0;
    }

    x += TILE_SPAN;
    if (x == GROUND_TILE_COLUMN_COUNT * TILE_SPAN) {
      x = 0;
      z += TILE_SPAN;
    }
  }
}

/*
 * Spreads a packed byte's two nibbles to the low nibbles of an attribute's two
 * bytes
 */
#define UnpackAttribute(packed) ((((packed) & 0xF0) << 4) | ((packed) & 0xF))

/*
 * Unpacks runs of attributes from the hole file into the build buffer, each
 * a count and a byte whose two nibbles go to the two bytes of an attribute,
 * then clears the rest of the buffer. Nothing calls it.
 */
void ReadAttributeRuns(void) {
  s16 count;
  u8 packed;
  u16* attribute;
  u16* end;
  u16* runEnd;
  s32 value;

  attribute = (u16*)f_BuildCursor;
  end = (u16*)(f_BuildCursor + ATTRIBUTE_BUFFER_SIZE);
  while (TRUE) {
    ReadFileBytes(&count, sizeof(count));
    if (count == 0) {
      break;
    }

    ReadFileBytes(&packed, sizeof(packed));
    runEnd = attribute + count;
    if (runEnd > end) {
      osSyncPrintf("ER1\n");
      break;
    }

    value = UnpackAttribute(packed);
    while (attribute != runEnd) {
      *attribute++ = value;
    }
  }

  if (attribute != end) {
    osSyncPrintf("Whats?\n");
  }
  while (attribute < end) {
    *attribute++ = 0;
  }

  f_BuildCursor = f_BuildBuffer;
}

/* Reads the billboards from the hole's tree file */
void ReadBillboards(void) {
  u8 count;
  u8 i;

  ReadFileBytes(&count, sizeof(count));
  for (i = 0; i < count; i++) {
    if (i >= BILLBOARD_COUNT) {
      osSyncPrintf("ER2\n");
      return;
    }

    ReadFileBytes(&g_Billboards[i].x, sizeof(s16));
    ReadFileBytes(&g_Billboards[i].y, sizeof(s16));
    ReadFileBytes(&g_Billboards[i].z, sizeof(s16));
    ReadFileBytes(&g_Billboards[i].textureIndex, sizeof(s8));
    g_Billboards[i].shapeIndex = 0;
    ReadFileBytes(&g_Billboards[i].unk8, sizeof(u8));
    g_Billboards[i].isNearCourseEdge = FALSE;
    ReadFileBytes(&g_Billboards[i].width, sizeof(u16));
    ReadFileBytes(&g_Billboards[i].height, sizeof(u16));
  }
}

/* Reads the hole triangles' kinds and corner positions from the hole's triangle
 * file */
void ReadHoleTriangles(void) {
  /* Unused; it is there to match */
  u8 unused[0x10] UNUSED;
  u8 count;
  s8 kind;
  u8 i;
  HoleTriangle* triangle;

  ReadFileBytes(&count, sizeof(count));
  for (i = 0; i < count; i++) {
    /* It tests the count, not the index, on every triangle */
    if (count >= HOLE_TRIANGLE_COUNT) {
      osSyncPrintf("ER3\n");
      return;
    }

    triangle = &g_WorkArea.hole.triangles[i];
    ReadFileBytes(&kind, sizeof(kind));
    triangle->kind = kind;
    ReadFileBytes(triangle->vertices[0].v.ob, sizeof(s16) * AXIS_COUNT);
    ReadFileBytes(triangle->vertices[1].v.ob, sizeof(s16) * AXIS_COUNT);
    ReadFileBytes(triangle->vertices[2].v.ob, sizeof(s16) * AXIS_COUNT);
  }
}

/* Reads each ground tile's mirrored cells from the file */
void ReadTileUnk992s(void) {
  s32 i;

  for (i = 0; i < GROUND_TILE_COUNT; i++) {
    ReadFileBytes(&g_WorkArea.hole.tiles[i].mirroredCells, sizeof(u16));
  }
}

void ReadVertexX(s16* coord, s32 flags);
void ReadVertexY(s16* coord, s32 flags);
void ReadVertexZ(s16* coord, s32 flags);

/*
 * Reads the grid vertices from the hole's vertex file. A vertex flagged
 * with the same x or z as the one before keeps the one already there.
 */
void ReadGridVertices(void) {
  s8 flags;
  s16 x;
  s16 y;
  s16 z;
  s32 i;

  for (i = 0;; i++) {
    ReadFileBytes(&flags, sizeof(flags));
    if (flags == VERTEX_LIST_END) {
      break;
    }

    if (i >= GROUND_GRID_VERTEX_SLOT_COUNT) {
      osSyncPrintf("ER4\n");
      break;
    }

    if (!(flags & VERTEX_FLAG_SAME_X)) {
      ReadVertexX(&x, flags);
      g_WorkArea.hole.gridVertices[i].v.ob[AXIS_X] = x;
      g_WorkArea.hole.gridVertices[i].v.tc[TEXTURE_AXIS_S] =
          (x * GRID_TEXTURE_SCALE) + GRID_TEXTURE_BORDER;
    }

    ReadVertexY(&y, flags);
    g_WorkArea.hole.gridVertices[i].v.ob[AXIS_Y] = y;
    if (!(flags & VERTEX_FLAG_SAME_Z)) {
      ReadVertexZ(&z, flags);
      g_WorkArea.hole.gridVertices[i].v.ob[AXIS_Z] = z;
      g_WorkArea.hole.gridVertices[i].v.tc[TEXTURE_AXIS_T] =
          (z * GRID_TEXTURE_SCALE) + GRID_TEXTURE_BORDER;
    }
  }
}

/* Reads the midpoints of the ground tiles from the hole's vertex file, like
 * ReadGridVertices */
void ReadTileMidpoints(void) {
  s8 flags;
  s16 x;
  s16 y;
  s16 z;
  s32 midpoint;
  s32 tile;

  midpoint = 0;
  tile = 0;
  while (TRUE) {
    ReadFileBytes(&flags, sizeof(flags));
    if (flags == VERTEX_LIST_END) {
      break;
    }

    if (!(flags & VERTEX_FLAG_SAME_X)) {
      ReadVertexX(&x, flags);
      g_WorkArea.hole.midpoints[tile][midpoint].v.ob[AXIS_X] = x;
      g_WorkArea.hole.midpoints[tile][midpoint].v.tc[TEXTURE_AXIS_S] =
          (x * GRID_TEXTURE_SCALE) + GRID_TEXTURE_BORDER;
    }

    ReadVertexY(&y, flags);
    g_WorkArea.hole.midpoints[tile][midpoint].v.ob[AXIS_Y] = y;
    if (!(flags & VERTEX_FLAG_SAME_Z)) {
      ReadVertexZ(&z, flags);
      g_WorkArea.hole.midpoints[tile][midpoint].v.ob[AXIS_Z] = z;
      g_WorkArea.hole.midpoints[tile][midpoint].v.tc[TEXTURE_AXIS_T] =
          (z * GRID_TEXTURE_SCALE) + GRID_TEXTURE_BORDER;
    }

    midpoint++;
    if (midpoint >= TILE_MIDPOINT_COUNT) {
      midpoint = 0;
      tile++;
    }
  }
}

/*
 * Reads a vertex's x coordinate, whose encoding the flags select: two
 * bytes, or one byte scaled up by one of three shifts
 */
void ReadVertexX(s16* coord, s32 flags) {
  *coord = 0;
  switch (flags & VERTEX_FLAG_X_ENCODING) {
    case VERTEX_ENCODING_BYTE:
      ReadFileBytes(coord, 1);
      *coord = (u16)*coord >> BITS_PER_BYTE;
      break;
    case VERTEX_ENCODING_FINE_BYTE:
      ReadFileBytes(coord, 1);
      *coord = ((u16)*coord >> BITS_PER_BYTE) << VERTEX_FINE_HORIZONTAL_SHIFT;
      break;
    case VERTEX_ENCODING_COARSE_BYTE:
      ReadFileBytes(coord, 1);
      *coord = ((u16)*coord >> BITS_PER_BYTE) << VERTEX_COARSE_HORIZONTAL_SHIFT;
      break;
    case VERTEX_ENCODING_TWO_BYTES:
      ReadFileBytes(coord, 2);
      break;
  }
}

/*
 * Reads a vertex's y coordinate, whose encoding the flags select: two
 * bytes, or one byte scaled up by one of three shifts
 */
void ReadVertexY(s16* coord, s32 flags) {
  *coord = 0;
  /* The u8 cast is there to match */
  switch (
      (u8)(flags & VERTEX_FLAG_Y_ENCODING) >> VERTEX_FLAG_Y_ENCODING_SHIFT) {
    case VERTEX_ENCODING_BYTE:
      ReadFileBytes(coord, 1);
      *coord = (u16)*coord >> BITS_PER_BYTE;
      break;
    case VERTEX_ENCODING_FINE_BYTE:
      ReadFileBytes(coord, 1);
      *coord = ((u16)*coord >> BITS_PER_BYTE) << VERTEX_FINE_HEIGHT_SHIFT;
      break;
    case VERTEX_ENCODING_COARSE_BYTE:
      ReadFileBytes(coord, 1);
      *coord = ((u16)*coord >> BITS_PER_BYTE) << VERTEX_COARSE_HEIGHT_SHIFT;
      break;
    case VERTEX_ENCODING_TWO_BYTES:
      ReadFileBytes(coord, 2);
      break;
  }
}

/*
 * Reads a vertex's z coordinate, whose encoding the flags select: two
 * bytes, or one byte scaled up by one of three shifts
 */
void ReadVertexZ(s16* coord, s32 flags) {
  *coord = 0;
  /* The u8 cast is there to match */
  switch (
      (u8)(flags & VERTEX_FLAG_Z_ENCODING) >> VERTEX_FLAG_Z_ENCODING_SHIFT) {
    case VERTEX_ENCODING_BYTE:
      ReadFileBytes(coord, 1);
      *coord = (u16)*coord >> BITS_PER_BYTE;
      break;
    case VERTEX_ENCODING_FINE_BYTE:
      ReadFileBytes(coord, 1);
      *coord = ((u16)*coord >> BITS_PER_BYTE) << VERTEX_FINE_HORIZONTAL_SHIFT;
      break;
    case VERTEX_ENCODING_COARSE_BYTE:
      ReadFileBytes(coord, 1);
      *coord = ((u16)*coord >> BITS_PER_BYTE) << VERTEX_COARSE_HORIZONTAL_SHIFT;
      break;
    case VERTEX_ENCODING_TWO_BYTES:
      ReadFileBytes(coord, 2);
      break;
  }
}

/*
 * Opens one of a hole's files and reads it to the hole file buffer, or
 * unpacks the attributes into the build buffer
 */
void OpenHoleAsset(CourseRecordIndex course, s32 hole, s8 kind) {
  f_ReadCursor = HOLE_FILE_BUFFER;
  switch (kind) {
    case HOLE_ASSET_VERTICES:
      if (g_GameState.gameMode == GAME_MODE_UNK12) {
        OpenAssetFile(UNK12_VERTICES_ASSET, &f_HoleAsset);
      } else {
        OpenAssetFile(f_HoleAssetIds[course][hole].vertices, &f_HoleAsset);
      }

      break;
    case HOLE_ASSET_MIDPOINTS:
      if (g_GameState.gameMode == GAME_MODE_UNK12) {
        OpenAssetFile(UNK12_MIDPOINTS_ASSET, &f_HoleAsset);
      } else {
        OpenAssetFile(f_HoleAssetIds[course][hole].midpoints, &f_HoleAsset);
      }

      break;
    case HOLE_ASSET_TREES:
      if (g_GameState.gameMode == GAME_MODE_UNK12) {
        OpenAssetFile(UNK12_TREES_ASSET, &f_HoleAsset);
      } else {
        OpenAssetFile(f_HoleAssetIds[course][hole].trees, &f_HoleAsset);
      }

      break;
    case HOLE_ASSET_TRIANGLES:
      if (g_GameState.gameMode == GAME_MODE_UNK12) {
        OpenAssetFile(UNK12_TRIANGLES_ASSET, &f_HoleAsset);
      } else {
        OpenAssetFile(f_HoleAssetIds[course][hole].triangles, &f_HoleAsset);
      }

      break;
    case HOLE_ASSET_ATTRIBUTES:
      if (g_GameState.gameMode == GAME_MODE_UNK12) {
        OpenAssetFile(UNK12_ATTRIBUTES_ASSET, &f_HoleAsset);
      } else {
        OpenAssetFile(f_HoleAssetIds[course][hole].attributes, &f_HoleAsset);
      }

      GetAssetFileSize(&f_HoleAsset);
      LoadLzData(f_HoleAsset.romStart, f_BuildCursor, f_HoleAsset.size);
      return;
  }

  ReadAssetFile(f_ReadCursor, GetAssetFileSize(&f_HoleAsset), &f_HoleAsset);
}

/* An RGBA 5551 texel's channels */
#define TEXEL_RED_SHIFT 11
#define TEXEL_GREEN_SHIFT 6
#define TEXEL_BLUE_SHIFT 1
#define TEXEL_CHANNEL_MAX 31

/* A billboard's position is in units of 1/BILLBOARD_UNITS_PER_TEXEL map texel
 */
#define BILLBOARD_UNITS_PER_TEXEL 16
/* The map texels around a billboard's base that its shadow can fall on */
#define SHADOW_ROWS_BEFORE 16
#define SHADOW_ROWS_AFTER 25
#define SHADOW_COLUMN_REACH 25
/* How far toward the light the point that a billboard is viewed toward is */
#define SHADOW_LIGHT_DISTANCE 1000.0f
/* The hit mask's solidities, sampled at BILLBOARD_MASK_SIZE + 1 points a side
 */
#define SOLIDITY_ROW_SHIFT 5
#define SOLIDITY_ROW_LENGTH (1 << SOLIDITY_ROW_SHIFT)
/* How much of a texel's brightness is kept in a shadow, in percent */
#define LIT_PERCENT_FULL 100
#define LIT_PERCENT_AT_BASE 50.0f
#define SHADOW_NEARNESS_LIMIT 1.5f
#define SHADOW_NEARNESS_MIN 0.1f
#define SHADOW_SOLIDITY_SCALE 4.0f
/*
 * A texel's brightness is its red * 4 + green * 8 + blue * 2, less
 * SHADOW_BRIGHTNESS_FLOOR. Each masked channel is shifted from its place in
 * the texel to its weight; blue's place already weighs it 2.
 */
#define RED_BRIGHTNESS_SHIFT (RGBA16_RED_SHIFT - 2)
#define GREEN_BRIGHTNESS_SHIFT (RGBA16_GREEN_SHIFT - 3)
#define SHADOW_BRIGHTNESS_FLOOR 60
#define SHADOW_BRIGHTNESS_MAX 163
#define SHADOW_DARKENING_SCALE 4
#define SHADOW_SCALE_FRACTION_BITS 16
#define SHADOW_SCALE_ONE (1 << SHADOW_SCALE_FRACTION_BITS)

static inline u16* GetShadowedTexel(u32 column, u32 row) {
  return &g_WorkArea.hole
              .tiles[(column / GROUND_TILE_TEXEL_SPAN) +
                  ((row / GROUND_TILE_TEXEL_SPAN) * GROUND_TILE_COLUMN_COUNT)]
              .texels[0][((row & MAP_TILE_MASK) * GROUND_TILE_TEXEL_SPAN) +
                  (column & MAP_TILE_MASK)];
}

/*
 * Darkens the map texels in the billboards' shadows. Each texel near a
 * billboard is put in the view from the billboard's base toward the light.
 * Where that falls within the billboard's width and height, the texel is
 * darkened by the hit mask's solidity there, less with its distance along the
 * light, and within a diamond around the base, more toward the base. Dark
 * texels are kept, and bright ones are darkened the most. A row with no texel
 * within the width ends the shadow once one has started. Billboards with a
 * texture in f_ShadowlessTextures cast no shadow.
 * Doesn't match: ours keeps heightScale in a float register and loads
 * SHADOW_LIGHT_DISTANCE again for each use, where the original keeps them in
 * $s2 and $f22, and the other registers differ from there. 271 words differ, 45
 * in shape.
 */
#ifdef NON_MATCHING
void CastBillboardShadows(void) {
  Mat4 lightView;
  Vec3 lightPoint;
  u8 solidities[SOLIDITY_ROW_LENGTH][SOLIDITY_ROW_LENGTH];
  u8* solidityCells;
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

  /* solidityCells is there to match */
  solidityCells = solidities[0];
  for (index = 0; index != BILLBOARD_COUNT; index++) {
    hasShadowStarted = FALSE;
    if (g_Billboards[index].textureIndex < 0) {
      continue;
    }

    for (listIndex = 0;; listIndex++) {
      if (f_ShadowlessTextures[listIndex] == g_Billboards[index].textureIndex) {
        listIndex = -1;
        break;
      }

      if (f_ShadowlessTextures[listIndex] < 0) {
        break;
      }
    }

    if (listIndex < 0) {
      continue;
    }

    if (g_Billboards[index].textureIndex != lastTextureIndex) {
      for (row = 0; row <= BILLBOARD_MASK_SIZE; row++) {
        for (column = 0; column <= BILLBOARD_MASK_SIZE; column++) {
          solidityCells[(row << SOLIDITY_ROW_SHIFT) + column] =
              GetBillboardSolidity(g_Billboards[index].textureIndex,
                  column * (1.0f / BILLBOARD_MASK_SIZE),
                  row * (1.0f / BILLBOARD_MASK_SIZE));
        }
      }

      lastTextureIndex = g_Billboards[index].textureIndex;
    }

    width = g_Billboards[index].width;
    widthScale = 1.0f / width;
    IN_OWN_BLOCK(height = g_Billboards[index].height);
    heightScale = 1.0f / height;
    x = g_Billboards[index].x;
    z = g_Billboards[index].z;
    lightPoint[AXIS_X] =
        (cosf(lightPitch) * cosf(lightYaw) * SHADOW_LIGHT_DISTANCE) + x;
    lightPoint[AXIS_Z] =
        (cosf(lightPitch) * sinf(lightYaw) * SHADOW_LIGHT_DISTANCE) + z;
    lightPoint[AXIS_Y] = sinf(lightPitch) * SHADOW_LIGHT_DISTANCE;
    guLookAtF(lightView, x, 0.0f, z, lightPoint[AXIS_X], lightPoint[AXIS_Y],
        lightPoint[AXIS_Z], 0.0f, -1.0f, 0.0f);
    centerColumn = x / BILLBOARD_UNITS_PER_TEXEL;
    centerRow = z / BILLBOARD_UNITS_PER_TEXEL;
    lightView[0][0] *= BILLBOARD_UNITS_PER_TEXEL;
    lightView[0][1] *= BILLBOARD_UNITS_PER_TEXEL;
    lightView[0][2] *= BILLBOARD_UNITS_PER_TEXEL;
    lightView[2][0] *= BILLBOARD_UNITS_PER_TEXEL;
    lightView[2][1] *= BILLBOARD_UNITS_PER_TEXEL;
    lightView[2][2] *= BILLBOARD_UNITS_PER_TEXEL;
    row = centerRow - SHADOW_ROWS_BEFORE;
    rowEnd = centerRow + SHADOW_ROWS_AFTER;
    if (row < 0) {
      row = 0;
    }

    if (rowEnd >= MAP_ROW_COUNT) {
      rowEnd = MAP_ROW_COUNT - 1;
    }

    for (; row < rowEnd; row++) {
      rowPoint[AXIS_X] = (lightView[2][0] * row) + lightView[3][0];
      rowPoint[AXIS_Y] = (lightView[2][1] * row) + lightView[3][1];
      rowPoint[AXIS_Z] = (lightView[2][2] * row) + lightView[3][2];
      isRowInWidth = FALSE;
      column = centerColumn - SHADOW_COLUMN_REACH;
      columnEnd = centerColumn + SHADOW_COLUMN_REACH;
      if (column < 0) {
        column = 0;
      }

      if (columnEnd >= MAP_COLUMN_COUNT) {
        columnEnd = MAP_COLUMN_COUNT - 1;
      }

      for (; column < columnEnd; column++) {
        viewX = (lightView[0][0] * column) + rowPoint[AXIS_X];
        if (width <= fabsf(viewX)) {
          if (!isRowInWidth) {
            continue;
          }

          break;
        }

        isRowInWidth = TRUE;
        viewY = (lightView[0][1] * column) + rowPoint[AXIS_Y];
        if (((-width <= viewY) & (viewY < height)) == 0) {
          continue;
        }

        litPercent = LIT_PERCENT_FULL;
        heightFraction = viewY * heightScale;
        widthFraction = fabsf(viewX) * widthScale;
        if (widthFraction + fabsf(heightFraction * 2) < 1.0f) {
          litPercent = (sqrtf((widthFraction * widthFraction) +
                            (heightFraction * heightFraction * 4.0f)) *
                           LIT_PERCENT_AT_BASE) +
              LIT_PERCENT_AT_BASE;
        }

        if (viewY >= 0.0f) {
          solidity = solidityCells[((s32)(heightFraction * BILLBOARD_MASK_SIZE)
                                       << SOLIDITY_ROW_SHIFT) +
              (s32)(widthFraction * BILLBOARD_MASK_SIZE)];
          if (solidity != 0) {
            nearness = SHADOW_NEARNESS_LIMIT -
                (fabsf((lightView[0][2] * column) + rowPoint[AXIS_Z]) *
                    heightScale);
            if (nearness > SHADOW_NEARNESS_MIN) {
              solidLitPercent = LIT_PERCENT_FULL -
                  (solidity * nearness * SHADOW_SOLIDITY_SCALE);
              if (solidLitPercent < litPercent) {
                litPercent = solidLitPercent;
              }
            }
          }
        }

        if (litPercent == LIT_PERCENT_FULL) {
          continue;
        }

        color = *GetShadowedTexel(column, row);
        red = color & TEXEL_RED_MASK;
        green = color & TEXEL_GREEN_MASK;
        blue = color & TEXEL_BLUE_MASK;
        brightness = (red >> RED_BRIGHTNESS_SHIFT) +
            ((green >> GREEN_BRIGHTNESS_SHIFT) +
                (blue - SHADOW_BRIGHTNESS_FLOOR));
        if (brightness < 0) {
          continue;
        }

        if (brightness > SHADOW_BRIGHTNESS_MAX) {
          brightness = SHADOW_BRIGHTNESS_MAX;
        }

        scale = SHADOW_SCALE_ONE -
            ((LIT_PERCENT_FULL - litPercent) * brightness *
                SHADOW_DARKENING_SCALE);
        red = (red * scale) >> SHADOW_SCALE_FRACTION_BITS;
        green = (green * scale) >> SHADOW_SCALE_FRACTION_BITS;
        blue = (blue * scale) >> SHADOW_SCALE_FRACTION_BITS;
        if (red > TEXEL_RED_MASK) {
          red = TEXEL_RED_MASK;
        } else {
          red &= TEXEL_RED_MASK;
        }

        if (green > TEXEL_GREEN_MASK) {
          green = TEXEL_GREEN_MASK;
        } else {
          green &= TEXEL_GREEN_MASK;
        }

        if (blue > TEXEL_BLUE_MASK) {
          blue = TEXEL_BLUE_MASK;
        } else {
          blue &= TEXEL_BLUE_MASK;
        }

        alpha = color & TEXEL_ALPHA_MASK;
        color = red | green | blue | alpha;
        *GetShadowedTexel(column, row) = color;
      }

      if (isRowInWidth) {
        hasShadowStarted = TRUE;
      } else if (hasShadowStarted == TRUE) {
        break;
      }
    }
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", CastBillboardShadows);
#endif

/*
 * Sets the ground's light and shade colors for the scene's color index,
 * interpolating between those of the key indexes around it.
 * InterpolateChannel is a nested function, a GNU C extension.
 */
void GetGroundLightColors(GroundLightColor* light, GroundLightColor* shade) {
  GroundLightColor keyLights[2];
  GroundLightColor keyShades[2];
  s32 keyIndexes[2];
  s32 i;
  f32 fraction;

  s32 InterpolateChannel(s32 from, s32 to, f32 fraction) {
    return (to - from) * fraction + from;
  }

  if (g_HoleSetup.conditions.sceneColorIndex ==
      SCENE_COLOR_INDEX_SUDDEN_DEATH) {
    keyIndexes[0] = g_HoleSetup.conditions.sceneColorIndex;
    keyIndexes[1] = keyIndexes[0];
  } else if ((s32)g_HoleSetup.conditions.sceneColorIndex >=
      SCENE_COLOR_INDEX_DRIVING_RANGE) {
    keyIndexes[0] = SCENE_COLOR_INDEX_DRIVING_RANGE;
    keyIndexes[1] = SCENE_COLOR_INDEX_ROUND_END;
  } else {
    keyIndexes[0] = SCENE_COLOR_INDEX_ROUND_START;
    keyIndexes[1] = SCENE_COLOR_INDEX_DRIVING_RANGE;
  }

  for (i = 0; i < 2; i++) {
    switch (keyIndexes[i]) {
      default:
      case SCENE_COLOR_INDEX_ROUND_END: {
        static const GroundLightColor endLight = {23, 18, 14};
        static const GroundLightColor endShade = {10, 6, 0};

        keyLights[i] = endLight;
        keyShades[i] = endShade;
        break;
      }
      case SCENE_COLOR_INDEX_DRIVING_RANGE: {
        static const GroundLightColor middleLight = {21, 21, 21};
        static const GroundLightColor middleShade = {4, 4, 4};

        keyLights[i] = middleLight;
        keyShades[i] = middleShade;
        break;
      }
      case SCENE_COLOR_INDEX_ROUND_START: {
        static const GroundLightColor startLight = {20, 20, 20};
        static const GroundLightColor startShade = {6, 6, 6};

        keyLights[i] = startLight;
        keyShades[i] = startShade;
        break;
      }
      case SCENE_COLOR_INDEX_SUDDEN_DEATH: {
        static const GroundLightColor suddenDeathLight = {20, 20, 2};
        static const GroundLightColor suddenDeathShade = {0, 0, 0};

        keyLights[i] = suddenDeathLight;
        keyShades[i] = suddenDeathShade;
        break;
      }
    }
  }

  if (keyIndexes[1] == keyIndexes[0]) {
    fraction = 0.0f;
  } else {
    fraction =
        (f32)((s32)g_HoleSetup.conditions.sceneColorIndex - keyIndexes[0]) /
        (f32)(keyIndexes[1] - keyIndexes[0]);
  }

  light->red = InterpolateChannel(keyLights[0].red, keyLights[1].red, fraction);
  light->green =
      InterpolateChannel(keyLights[0].green, keyLights[1].green, fraction);
  light->blue =
      InterpolateChannel(keyLights[0].blue, keyLights[1].blue, fraction);
  shade->red = InterpolateChannel(keyShades[0].red, keyShades[1].red, fraction);
  shade->green =
      InterpolateChannel(keyShades[0].green, keyShades[1].green, fraction);
  shade->blue =
      InterpolateChannel(keyShades[0].blue, keyShades[1].blue, fraction);
}

/* The two bands of height rows that ShadeGroundTiles shades from, in the hole
 * file buffer */
#define HEIGHT_RING_ROW_COUNT (HEIGHT_BAND_ROW_COUNT * 2)
#define HEIGHT_RING_ROW_MASK (HEIGHT_RING_ROW_COUNT - 1)
/* A band is drawn while shading the rows half a band before it */
#define HEIGHT_BAND_LEAD (HEIGHT_BAND_ROW_COUNT / 2)
/* The slopes along the light's yaw that the ground is lit for */
#define LIGHT_SLOPE_MIN (-32)
#define LIGHT_SLOPE_MAX 31
#define LIGHT_SLOPE_COUNT (LIGHT_SLOPE_MAX - LIGHT_SLOPE_MIN + 1)
/* Lighting scales, in LIGHT_SCALE_FRACTION_BITS fixed point */
#define LIGHT_SCALE_FRACTION_BITS 11
#define LIGHT_SCALE_ONE (1 << LIGHT_SCALE_FRACTION_BITS)
#define LIGHT_SCALE_SLOPE_STEP 0x30
#define LIGHT_SCALE_MIN 0x500
#define LIGHT_SCALE_MAX 0xC00
/* How far apart, in map texels, the heights that give a slope are, along the
 * light's yaw */
#define LIGHT_OFFSET_LENGTH 4.5f
/* The light's color is in fractions of full brightness of this many bits */
#define LIGHT_COLOR_FRACTION_BITS 5
/* How much a unit of height fades the ground toward white */
#define HEIGHT_FADE_SCALE 0.008f
#define HEIGHT_FADE_LIMIT 0.25f
#define HEIGHT_FADE_FULL 1.0f

/* The light's color for each slope, a channel per word */
#define SLOPE_LIGHT_RED 0
#define SLOPE_LIGHT_GREEN 1
#define SLOPE_LIGHT_BLUE 2
#define SLOPE_LIGHT_WORD_COUNT 4

/*
 * Lights the ground tiles' map texels by the slope of the ground along the
 * light's yaw, from the heights that DrawBandHeights draws a band at a time.
 * Texels at heights below 0 are only lit. When GAME_FLAG_FADE_HIGH_GROUND is
 * set, the others also fade toward white with height: up to HEIGHT_FADE_LIMIT,
 * fully when GAME_FLAG_FADE_HIGH_GROUND_FULLY is set, or not at all when
 * GAME_FLAG_NO_HIGH_GROUND_FADE is set. Doesn't match: the original computes
 * the leading height row last and keeps a second, spilled copy of the trailing
 * one, and it reads the fade flags from the stack for each tile, where ours
 * extends isHighGroundFaded once per row and keeps it in another stack slot.
 * 110 words differ.
 */
#ifdef NON_MATCHING
void ShadeGroundTiles(void) {
  s32 unused[20] UNUSED; /* Unused; it is there to match */
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

  s8 isHighGroundFaded;
  s8 isFadeUnlimited;
  s8 isFadeDisabled;
  s16(*heightRows)[MAP_COLUMN_COUNT];
  Radians pitch;
  s32 lightColumnOffset;
  s32 lightRowOffset;
  s32 slope;
  s32 scale;
  s32 row;
  s32 bandRow;
  s32 column;
  s32 count;
  Boolean isMapEdge;
  s32 trailingColumn;
  s32 leadingColumn;
  s16* heights;
  s16* trailingHeights;
  s16* leadingHeights;
  s16* trailing;
  s16* leading;
  u16* texel;
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

  /* The light's pitch is unused */
  cosf(pitch);
  sinf(pitch);
  for (slope = LIGHT_SLOPE_MIN; slope <= LIGHT_SLOPE_MAX; slope++) {
    scale = LIGHT_SCALE_ONE + (slope * LIGHT_SCALE_SLOPE_STEP);
    if (scale < LIGHT_SCALE_MIN) {
      scale = LIGHT_SCALE_MIN;
    } else if (scale > LIGHT_SCALE_MAX) {
      scale = LIGHT_SCALE_MAX;
    }

    slopeLights[((slope - LIGHT_SLOPE_MIN) * SLOPE_LIGHT_WORD_COUNT) +
        SLOPE_LIGHT_RED] = light.red * scale;
    slopeLights[((slope - LIGHT_SLOPE_MIN) * SLOPE_LIGHT_WORD_COUNT) +
        SLOPE_LIGHT_GREEN] = light.green * scale;
    slopeLights[((slope - LIGHT_SLOPE_MIN) * SLOPE_LIGHT_WORD_COUNT) +
        SLOPE_LIGHT_BLUE] = light.blue * scale;
  }

  lightColumnOffset =
      cosf(g_HoleSetup.conditions.lightYaw * BINARY_ANGLE_TO_RADIANS) *
      LIGHT_OFFSET_LENGTH;
  lightRowOffset =
      sinf(g_HoleSetup.conditions.lightYaw * BINARY_ANGLE_TO_RADIANS) *
      LIGHT_OFFSET_LENGTH;
  osSyncPrintf("LightXd%d Zd%d\n", lightColumnOffset, lightRowOffset);
  for (row = 1 - HEIGHT_BAND_ROW_COUNT; row < MAP_ROW_COUNT; row++) {
    bandRow = row + HEIGHT_BAND_LEAD;
    if (bandRow >= 0) {
      if (bandRow < MAP_ROW_COUNT && !(bandRow & MAP_TILE_MASK)) {
        DrawBandHeights(heightRows[bandRow & HEIGHT_BAND_ROW_COUNT],
            bandRow / HEIGHT_BAND_ROW_COUNT);
      }
    }

    if (row < 0) {
      continue;
    }

    heights = heightRows[row & HEIGHT_RING_ROW_MASK];
    trailingHeights = heightRows[(row - lightRowOffset) & HEIGHT_RING_ROW_MASK];
    leadingHeights = heightRows[(row + lightRowOffset) & HEIGHT_RING_ROW_MASK];
    column = 0;
    do {
      texel =
          &g_WorkArea.hole
               .tiles[(column / GROUND_TILE_TEXEL_SPAN) +
                   ((row / GROUND_TILE_TEXEL_SPAN) * GROUND_TILE_COLUMN_COUNT)]
               .texels[0][(row & MAP_TILE_MASK) * GROUND_TILE_TEXEL_SPAN];
      /* The offset heights can be off the map only in the edge tiles */
      isMapEdge =
          column == 0 || column == MAP_COLUMN_COUNT - GROUND_TILE_TEXEL_SPAN;
      if (!isMapEdge) {
        /* Indexing heightRows again is there to match */
        trailing = &heightRows[(row - lightRowOffset) & HEIGHT_RING_ROW_MASK]
                              [column - lightColumnOffset];
        leading = &leadingHeights[column + lightColumnOffset];
      }

      for (count = GROUND_TILE_TEXEL_SPAN; count != 0;
          count--, column++, texel++, heights++) {
        if (isMapEdge) {
          trailingColumn = column - lightColumnOffset;
          leadingColumn = column + lightColumnOffset;

          /* Only one of the columns is clamped */
          if (trailingColumn < 0) {
            trailingColumn = 0;
          } else if (leadingColumn < 0) {
            leadingColumn = 0;
          } else if (trailingColumn >= MAP_COLUMN_COUNT) {
            trailingColumn = MAP_COLUMN_COUNT - 1;
          } else if (leadingColumn >= MAP_COLUMN_COUNT) {
            leadingColumn = MAP_COLUMN_COUNT - 1;
          }

          slope =
              trailingHeights[trailingColumn] - leadingHeights[leadingColumn];
        } else {
          slope = *trailing++ - *leading++;
        }

        if (slope < LIGHT_SLOPE_MIN) {
          slope = LIGHT_SLOPE_MIN;
        } else if (slope > LIGHT_SLOPE_MAX) {
          slope = LIGHT_SLOPE_MAX;
        }

        texelColor = *texel;
        if (*heights < 0) {
          color = ((shade.red + (texelColor >> TEXEL_RED_SHIFT)) *
                      slopeLights[((slope - LIGHT_SLOPE_MIN) *
                                      SLOPE_LIGHT_WORD_COUNT) +
                          SLOPE_LIGHT_RED]) >>
              (LIGHT_SCALE_FRACTION_BITS + LIGHT_COLOR_FRACTION_BITS);
          if (color > TEXEL_CHANNEL_MAX) {
            color = TEXEL_CHANNEL_MAX;
          }

          color <<= TEXEL_RED_SHIFT;
          channel = ((shade.green + (texelColor & TEXEL_GREEN_MASK)) *
                        slopeLights[((slope - LIGHT_SLOPE_MIN) *
                                        SLOPE_LIGHT_WORD_COUNT) +
                            SLOPE_LIGHT_GREEN]) >>
              (LIGHT_SCALE_FRACTION_BITS + LIGHT_COLOR_FRACTION_BITS +
                  TEXEL_GREEN_SHIFT);
          if (channel > TEXEL_CHANNEL_MAX) {
            channel = TEXEL_CHANNEL_MAX;
          }

          color |= channel << TEXEL_GREEN_SHIFT;
          channel = ((shade.blue + (texelColor & TEXEL_BLUE_MASK)) *
                        slopeLights[((slope - LIGHT_SLOPE_MIN) *
                                        SLOPE_LIGHT_WORD_COUNT) +
                            SLOPE_LIGHT_BLUE]) >>
              (LIGHT_SCALE_FRACTION_BITS + LIGHT_COLOR_FRACTION_BITS +
                  TEXEL_BLUE_SHIFT);
          if (channel > TEXEL_CHANNEL_MAX) {
            channel = TEXEL_CHANNEL_MAX;
          }

          color |= channel << TEXEL_BLUE_SHIFT;
          *texel = color | (texelColor & TEXEL_ALPHA_MASK);
        } else {
          red = ((shade.red + (texelColor >> TEXEL_RED_SHIFT)) *
                    slopeLights[((slope - LIGHT_SLOPE_MIN) *
                                    SLOPE_LIGHT_WORD_COUNT) +
                        SLOPE_LIGHT_RED]) >>
              (LIGHT_SCALE_FRACTION_BITS + LIGHT_COLOR_FRACTION_BITS);
          if ((shaded.red = red) > TEXEL_CHANNEL_MAX) {
            shaded.red = TEXEL_CHANNEL_MAX;
          }

          green = ((shade.green + (texelColor & TEXEL_GREEN_MASK)) *
                      slopeLights[((slope - LIGHT_SLOPE_MIN) *
                                      SLOPE_LIGHT_WORD_COUNT) +
                          SLOPE_LIGHT_GREEN]) >>
              (LIGHT_SCALE_FRACTION_BITS + LIGHT_COLOR_FRACTION_BITS +
                  TEXEL_GREEN_SHIFT);
          if ((shaded.green = green) > TEXEL_CHANNEL_MAX) {
            shaded.green = TEXEL_CHANNEL_MAX;
          }

          blue = ((shade.blue + (texelColor & TEXEL_BLUE_MASK)) *
                     slopeLights[((slope - LIGHT_SLOPE_MIN) *
                                     SLOPE_LIGHT_WORD_COUNT) +
                         SLOPE_LIGHT_BLUE]) >>
              (LIGHT_SCALE_FRACTION_BITS + LIGHT_COLOR_FRACTION_BITS +
                  TEXEL_BLUE_SHIFT);
          if ((shaded.blue = blue) > TEXEL_CHANNEL_MAX) {
            shaded.blue = TEXEL_CHANNEL_MAX;
          }

          shaded.alpha = texelColor & TEXEL_ALPHA_MASK;

          /* The height test is always true here; the original has it */
          if (*heights >= 0 && isHighGroundFaded) {
            fade = *heights * HEIGHT_FADE_SCALE;
            if (isFadeUnlimited) {
              fadeLimit = HEIGHT_FADE_FULL;
            } else if (isFadeDisabled) {
              fade = 0.0f;

              /* The goto is there to match */
              goto FADED;
            } else {
              fadeLimit = HEIGHT_FADE_LIMIT;
            }

            if (fade >= fadeLimit) {
              fade = fadeLimit;
            }

FADED:

            fadeWeight = fade * LIGHT_SCALE_ONE;
            keptWeight = LIGHT_SCALE_ONE - fadeWeight;
            fadedRed = shaded.red;
            fadedGreen = shaded.green;
            fadedBlue = shaded.blue;
            fadedRed = (fadedRed * keptWeight) + (fadeColor.red * fadeWeight);
            if (fadedRed > TEXEL_RED_MASK) {
              fadedRed = TEXEL_RED_MASK;
            } else {
              fadedRed &= TEXEL_RED_MASK;
            }

            fadedGreen =
                ((fadedGreen * keptWeight) + (fadeColor.green * fadeWeight)) >>
                (LIGHT_SCALE_FRACTION_BITS - TEXEL_GREEN_SHIFT);
            if (fadedGreen > TEXEL_GREEN_MASK) {
              fadedGreen = TEXEL_GREEN_MASK;
            } else {
              fadedGreen &= TEXEL_GREEN_MASK;
            }

            fadedBlue =
                ((fadedBlue * keptWeight) + (fadeColor.blue * fadeWeight)) >>
                (LIGHT_SCALE_FRACTION_BITS - TEXEL_BLUE_SHIFT);
            if (fadedBlue > TEXEL_BLUE_MASK) {
              fadedBlue = TEXEL_BLUE_MASK;
            } else {
              fadedBlue &= TEXEL_BLUE_MASK;
            }

            texelColor = fadedRed | fadedGreen | fadedBlue | shaded.alpha;
          } else {
            texelColor = shaded.alpha |
                ((shaded.red << TEXEL_RED_SHIFT) |
                    (shaded.green << TEXEL_GREEN_SHIFT) |
                    (shaded.blue << TEXEL_BLUE_SHIFT));
          }

          *texel = texelColor;
        }
      }
    } while (column < MAP_COLUMN_COUNT);
  }
}
#else
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", f_DefaultGroundLight);
INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", ShadeGroundTiles);
#endif

/* Shades the ground tiles, then darkens them under the billboards' shadows */
void ShadeGround(s32 shadeVariant UNUSED) {
  ShadeGroundTiles();
  osSyncPrintf("TR");
  CastBillboardShadows();
  osSyncPrintf("EE\n");
}

#define TILE_BORDER_END (GROUND_TILE_TEXEL_ROW_LENGTH - TILE_BORDER_BEFORE)
#define BORDERED_TILE_TEXEL_COUNT \
  (GROUND_TILE_TEXEL_ROW_COUNT * GROUND_TILE_TEXEL_ROW_LENGTH)

/*
 * The texel at texelColumn and texelRow in the tile at tileColumn and
 * tileRow, in either layout. Texels past a tile's last ones are in the next
 * tile; past the map's edges they repeat the edge texels.
 */
static inline u16 ReadTileTexel(
    s32 tileColumn, s32 tileRow, s32 texelColumn, s32 texelRow) {
  s32 tileIndex;
  GroundTile* tile;
  u16* texel;

  if (tileColumn < 0) {
    tileColumn = 0;
    texelColumn = 0;
  } else if (tileColumn >= GROUND_TILE_COLUMN_COUNT) {
    tileColumn = GROUND_TILE_COLUMN_COUNT - 1;
    texelColumn = -1;
  }

  if (tileRow < 0) {
    tileRow = 0;
    texelRow = 0;
  } else if (tileRow >= GROUND_TILE_ROW_COUNT) {
    tileRow = GROUND_TILE_ROW_COUNT - 1;
    texelRow = -1;
  }

  tileIndex = (tileRow * GROUND_TILE_COLUMN_COUNT) + tileColumn;
  if (texelColumn > GROUND_TILE_TEXEL_SPAN) {
    texelColumn = GROUND_TILE_TEXEL_SPAN;
  }

  texelColumn = (texelColumn + GROUND_TILE_TEXEL_SPAN) & MAP_TILE_MASK;
  texelRow = (texelRow + GROUND_TILE_TEXEL_SPAN) & MAP_TILE_MASK;
  tile = &g_WorkArea.hole.tiles[tileIndex];
  if (g_WorkArea.hole.tiles[tileIndex].texels[GROUND_TILE_TEXEL_ROW_COUNT -
          1][GROUND_TILE_TEXEL_ROW_LENGTH - 1] != 0) {
    texel = &tile->texels[texelRow + TILE_BORDER_BEFORE]
                         [texelColumn + TILE_BORDER_BEFORE];
  } else {
    texel = &tile->texels[0][(texelRow * GROUND_TILE_TEXEL_SPAN) + texelColumn];
  }

  return *texel;
}

/*
 * Switches the ground tiles between their two texel layouts: 32 texels a
 * row, or inside a border of their neighbors' texels. Doesn't match:
 * where ReadTileTexel reads a tile, the original adds the tile's address
 * after the layout test, and ours adds it before; ours also masks the texel
 * coordinates in another order. 97 words differ.
 */
#ifdef NON_MATCHING
void ToggleGroundTileBorders(void) {
  u16 texels[GROUND_TILE_TEXEL_ROW_COUNT][GROUND_TILE_TEXEL_ROW_LENGTH];
  s32 tileColumn;
  s32 tileRow;
  s32 texelColumn;
  s32 texelRow;
  u16* texel;
  u16* tileTexel;
  f64* source;
  f64* destination;

  if (g_WorkArea.hole.tiles[0].texels[GROUND_TILE_TEXEL_ROW_COUNT - 1]
                                     [GROUND_TILE_TEXEL_ROW_LENGTH - 1] != 0) {
    s32 strippedTileColumn;
    s32 strippedTileRow;
    s32 strippedTexelColumn;
    s32 strippedTexelRow;
    s32 i;

    for (strippedTileRow = 0; strippedTileRow < GROUND_TILE_ROW_COUNT;
        strippedTileRow++) {
      for (strippedTileColumn = 0;
          strippedTileColumn < GROUND_TILE_COLUMN_COUNT; strippedTileColumn++) {
        for (strippedTexelRow = 0; strippedTexelRow < GROUND_TILE_TEXEL_SPAN;
            strippedTexelRow++) {
          for (strippedTexelColumn = 0;
              strippedTexelColumn != GROUND_TILE_TEXEL_SPAN;
              strippedTexelColumn++) {
            texels[0][(strippedTexelRow * GROUND_TILE_TEXEL_SPAN) +
                strippedTexelColumn] = ReadTileTexel(strippedTileColumn,
                strippedTileRow, strippedTexelColumn, strippedTexelRow);
          }
        }

        texels[GROUND_TILE_TEXEL_ROW_COUNT - 1]
              [GROUND_TILE_TEXEL_ROW_LENGTH - 1] = 0;
        for (i = 0; i < BORDERED_TILE_TEXEL_COUNT; i++) {
          g_WorkArea.hole
              .tiles[(strippedTileRow * GROUND_TILE_COLUMN_COUNT) +
                  strippedTileColumn]
              .texels[0][i] = texels[0][i];
        }
      }
    }

    return;
  }

  for (tileRow = 0; tileRow < GROUND_TILE_ROW_COUNT; tileRow++) {
    for (tileColumn = 0; tileColumn < GROUND_TILE_COLUMN_COUNT; tileColumn++) {
      texel = &texels[TILE_BORDER_BEFORE][TILE_BORDER_BEFORE];
      tileTexel = g_WorkArea.hole
                      .tiles[(tileRow * GROUND_TILE_COLUMN_COUNT) + tileColumn]
                      .texels[0];
      for (texelRow = 0; texelRow < GROUND_TILE_TEXEL_SPAN; texelRow++) {
        for (texelColumn = 0; texelColumn < GROUND_TILE_TEXEL_SPAN;
            texelColumn++) {
          *texel++ = *tileTexel++;
        }

        texel += GROUND_TILE_TEXEL_ROW_LENGTH - GROUND_TILE_TEXEL_SPAN;
      }

      texel = texels[0];
      for (texelRow = -TILE_BORDER_BEFORE;
          texelRow < GROUND_TILE_TEXEL_ROW_COUNT - TILE_BORDER_BEFORE;
          texelRow++) {
        for (texelColumn = -TILE_BORDER_BEFORE;
            texelColumn < TILE_BORDER_END;) {
          *texel = ReadTileTexel(tileColumn + (texelColumn >> MAP_TILE_SHIFT),
              tileRow + (texelRow >> MAP_TILE_SHIFT), texelColumn, texelRow);
          /* Skips the texels already copied */
          if ((u32)texelRow < GROUND_TILE_TEXEL_SPAN &&
              texelColumn == -TILE_BORDER_BEFORE) {
            texel += GROUND_TILE_TEXEL_SPAN + 1;
            texelColumn += GROUND_TILE_TEXEL_SPAN + 1;
          } else {
            texel++;
            texelColumn++;
          }
        }
      }

      texels[GROUND_TILE_TEXEL_ROW_COUNT - 1]
            [GROUND_TILE_TEXEL_ROW_LENGTH - 1] = 0xFFFF;
      destination =
          (f64*)g_WorkArea.hole
              .tiles[(tileRow * GROUND_TILE_COLUMN_COUNT) + tileColumn]
              .texels;
      source = (f64*)texels;
      while (source != (f64*)&texels[GROUND_TILE_TEXEL_ROW_COUNT]) {
        *destination++ = *source++;
        *destination++ = *source++;
      }

      g_WorkArea.hole.tiles[(tileRow * GROUND_TILE_COLUMN_COUNT) + tileColumn]
          .unk990 = 0;
    }
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", ToggleGroundTileBorders);
#endif

/*
 * Gives the vertices from start up to end texture coordinates on the
 * mini golf ground tile at column and row
 */
void SetMiniGolfTileTextureCoords(
    s32 column, s32 row, Vtx_t* start, Vtx_t* end) {
  Vtx_t* vertex;
  s32 coord;

  for (vertex = start; vertex < end; vertex++) {
    coord = TexelCoordinate((vertex->ob[AXIS_X] * MINI_GOLF_TEXTURE_SCALE_X) -
                (column << TILE_ORIGIN_SHIFT)) +
        TILE_TEXTURE_ORIGIN;
    vertex->tc[TEXTURE_AXIS_S] = coord;

    /* The second coordinate's two steps are there to match */
    coord = (vertex->ob[AXIS_Z] * MINI_GOLF_TEXTURE_SCALE_Z) -
        (row << TILE_ORIGIN_SHIFT);
    coord = TexelCoordinate(coord) + TILE_TEXTURE_ORIGIN;
    vertex->tc[TEXTURE_AXIS_T] = coord;
  }
}

/*
 * Gives the vertices from start up to end texture coordinates on the
 * ground tile at column and row
 */
void SetTileTextureCoords(s32 column, s32 row, Vtx_t* start, Vtx_t* end) {
  Vtx_t* vertex;
  s32 coord;

  if (g_GameState.isMiniGolf) {
    SetMiniGolfTileTextureCoords(column, row, start, end);
    return;
  }

  for (vertex = start; vertex < end; vertex++) {
    coord =
        TexelCoordinate(vertex->ob[AXIS_X] - (column << TILE_ORIGIN_SHIFT)) +
        TILE_TEXTURE_ORIGIN;
    vertex->tc[TEXTURE_AXIS_S] = coord;

    /* The second coordinate's two steps are there to match */
    coord = vertex->ob[AXIS_Z] - (row << TILE_ORIGIN_SHIFT);
    coord = TexelCoordinate(coord) + TILE_TEXTURE_ORIGIN;
    vertex->tc[TEXTURE_AXIS_T] = coord;
  }
}

/* Copies count vertices; copying them as f64s is there to match */
static inline void CopyVertices(Vtx* destination, Vtx* source, s32 count) {
  s32 i;

  for (i = 0; i < count; i++) {
    ((f64*)&destination[i])[0] = ((f64*)&source[i])[0];
    ((f64*)&destination[i])[1] = ((f64*)&source[i])[1];
  }
}

/* The address of a vertex through a vertex segment */
#define SegmentedVertex(segment, vertex) ((u8*)(vertex) + ((segment) << 24))

/*
 * Builds the display list and vertices of the ground tile at column and
 * row, at the cursors, and moves the cursors past them
 */
void BuildGroundTileDisplayList(
    s32 column, s32 row, Gfx** gfxCursor, Vtx** vertexCursor) {
  /*
   * Sets midpoint halfway between from and to, as SetVertexMidpoint does.
   * SetTileVertexMidpoint is a nested function, a GNU C extension.
   */
  void SetTileVertexMidpoint(Vtx_t * from, Vtx_t * to, Vtx_t * midpoint) {
    midpoint->ob[AXIS_X] = (from->ob[AXIS_X] + to->ob[AXIS_X]) / 2;
    midpoint->ob[AXIS_Y] = (from->ob[AXIS_Y] + to->ob[AXIS_Y]) / 2;
    midpoint->ob[AXIS_Z] = (from->ob[AXIS_Z] + to->ob[AXIS_Z]) / 2;
    midpoint->flag = 0;
    midpoint->tc[TEXTURE_AXIS_S] =
        (from->tc[TEXTURE_AXIS_S] + to->tc[TEXTURE_AXIS_S]) / 2;
    midpoint->tc[TEXTURE_AXIS_T] =
        (from->tc[TEXTURE_AXIS_T] + to->tc[TEXTURE_AXIS_T]) / 2;
    midpoint->cn[0] = (from->cn[0] + to->cn[0]) / 2;
    midpoint->cn[1] = (from->cn[1] + to->cn[1]) / 2;
    midpoint->cn[2] = (from->cn[2] + to->cn[2]) / 2;
    midpoint->cn[3] = (from->cn[3] + to->cn[3]) / 2;
  }

  Gfx* gfx = *gfxCursor;
  Vtx* vertex = *vertexCursor;
  s32 tileIndex = (row * GROUND_TILE_COLUMN_COUNT) + column;
  Vtx* midpoints;
  s32* coarseOffsets;
  s32 segmentIndex = 0;
  s32 columnBlock;
  s32 rowBlock;
  s32 pointRow;
  s32 pointColumn;

  midpoints =
      g_WorkArea.hole
          .midpoints[g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex];
  g_GroundTileDisplayLists[tileIndex] = gfx;
  gDPSetTextureImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 1,
      TextureAddress(&g_WorkArea.hole.tiles[tileIndex]));
  gDPSetTile(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0,
      G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, 3, G_TX_MIRROR | G_TX_WRAP, 6,
      3);
  gDPLoadSync(gfx++);
  gDPLoadBlock(
      gfx++, G_TX_LOADTILE, 0, 0, GROUND_TEXTURE_LOAD_SIZE, GROUND_TEXTURE_DXT);
  gDPPipeSync(gfx++);
  gDPSetTile(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, GROUND_TEXTURE_LINE_SIZE, 0, 1,
      0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, 3, G_TX_MIRROR | G_TX_WRAP, 6,
      3);
  /* 33.5 by 32.5 texels, in 10.2 fixed point */
  gDPSetTileSize(gfx++, 1, 0, 0, 0x86, 0x82);
  coarseOffsets = g_CoarseVertexOffsets[tileIndex];

  /* The corners */
  CopyVertices(vertex, TileGridVertex(column, row, 0, 0), 2);
  CopyVertices(&vertex[2], TileGridVertex(column, row + 1, 0, 0), 2);
  gSPVertex(gfx++, vertex, 4, 0);
  vertex += 4;

  /* The midpoints inside the tile, then those on its edges */
  gSPVertex(gfx++, &midpoints[TILE_FIRST_INNER_MIDPOINT],
      TILE_INNER_MIDPOINT_COUNT, 4);
  gSPVertex(gfx++, SegmentedVertex(1, &midpoints[TILE_MIDPOINT_ROW0_COLUMN1]),
      TILE_SIDE_MIDPOINT_COUNT, 12);
  coarseOffsets[segmentIndex++] = BytesBetween(midpoints, vertex);
  SetTileVertexMidpoint(&TileGridVertex(column, row, 0, 0)->v,
      &TileGridVertex(column, row, 1, 0)->v, &vertex++->v);
  SetTileVertexMidpoint(&TileGridVertex(column, row, 1, 0)->v,
      &TileGridVertex(column, row, 2, 0)->v, &vertex++->v);
  gSPVertex(
      gfx++, SegmentedVertex(2, &midpoints[TILE_MIDPOINT_ROW1_COLUMN0]), 1, 14);
  gSPVertex(
      gfx++, SegmentedVertex(2, &midpoints[TILE_MIDPOINT_ROW3_COLUMN0]), 1, 15);
  gSPVertex(
      gfx++, SegmentedVertex(3, &midpoints[TILE_MIDPOINT_ROW1_COLUMN4]), 1, 16);
  gSPVertex(
      gfx++, SegmentedVertex(3, &midpoints[TILE_MIDPOINT_ROW3_COLUMN4]), 1, 17);
  coarseOffsets[segmentIndex++] =
      BytesBetween(&midpoints[TILE_MIDPOINT_ROW1_COLUMN0], vertex);
  SetTileVertexMidpoint(&TileGridVertex(column, row, 0, 0)->v,
      &TileGridVertex(column, row, 0, 1)->v, &vertex[0].v);
  SetTileVertexMidpoint(&TileGridVertex(column, row, 0, 1)->v,
      &TileGridVertex(column, row, 0, 2)->v, &vertex[2].v);
  vertex++;
  coarseOffsets[segmentIndex++] =
      BytesBetween(&midpoints[TILE_MIDPOINT_ROW1_COLUMN4], vertex);
  SetTileVertexMidpoint(&TileGridVertex(column, row, 2, 0)->v,
      &TileGridVertex(column, row, 2, 1)->v, &vertex[0].v);
  SetTileVertexMidpoint(&TileGridVertex(column, row, 2, 1)->v,
      &TileGridVertex(column, row, 2, 2)->v, &vertex[2].v);
  vertex += 3;
  gSPVertex(gfx++, SegmentedVertex(4, &midpoints[TILE_MIDPOINT_ROW4_COLUMN1]),
      TILE_SIDE_MIDPOINT_COUNT, 18);
  coarseOffsets[segmentIndex++] =
      BytesBetween(&midpoints[TILE_MIDPOINT_ROW4_COLUMN1], vertex);
  SetTileVertexMidpoint(&TileGridVertex(column, row, 0, 2)->v,
      &TileGridVertex(column, row, 1, 2)->v, &vertex++->v);
  SetTileVertexMidpoint(&TileGridVertex(column, row, 1, 2)->v,
      &TileGridVertex(column, row, 2, 2)->v, &vertex++->v);

  /* The grid vertices at the middles of the edges and at the center */
  CopyVertices(vertex, TileGridVertex(column, row, 1, 0), 1);
  gSPVertex(gfx++, SegmentedVertex(5, vertex), 1, 20);
  vertex++;
  coarseOffsets[segmentIndex++] = sizeof(Vtx);
  SetTileVertexMidpoint(&TileGridVertex(column, row, 0, 0)->v,
      &TileGridVertex(column, row, 2, 0)->v, &vertex++->v);
  CopyVertices(vertex, TileGridVertex(column, row, 0, 1), 1);
  gSPVertex(gfx++, SegmentedVertex(6, vertex), 1, 21);
  vertex++;
  coarseOffsets[segmentIndex++] = sizeof(Vtx);
  SetTileVertexMidpoint(&TileGridVertex(column, row, 0, 0)->v,
      &TileGridVertex(column, row, 0, 2)->v, &vertex++->v);
  CopyVertices(vertex, TileGridVertex(column, row, 1, 1), 1);
  gSPVertex(gfx++, vertex, 1, 22);
  vertex++;
  CopyVertices(vertex, TileGridVertex(column, row, 2, 1), 1);
  gSPVertex(gfx++, SegmentedVertex(7, vertex), 1, 23);
  vertex++;
  coarseOffsets[segmentIndex++] = sizeof(Vtx);
  SetTileVertexMidpoint(&TileGridVertex(column, row, 2, 0)->v,
      &TileGridVertex(column, row, 2, 2)->v, &vertex++->v);
  CopyVertices(vertex, TileGridVertex(column, row, 1, 2), 1);
  gSPVertex(gfx++, SegmentedVertex(8, vertex), 1, 24);
  vertex++;
  coarseOffsets[segmentIndex++] = sizeof(Vtx);
  SetTileVertexMidpoint(&TileGridVertex(column, row, 0, 2)->v,
      &TileGridVertex(column, row, 2, 2)->v, &vertex++->v);

  for (columnBlock = 0; columnBlock < TILE_POINT_SPAN - 1; columnBlock += 2) {
    for (rowBlock = 0; rowBlock < TILE_POINT_SPAN - 1; rowBlock += 2) {
      for (pointRow = rowBlock; pointRow < rowBlock + 2; pointRow++) {
        for (pointColumn = columnBlock; pointColumn < columnBlock + 2;
            pointColumn++) {
          gSP2Triangles(gfx++, f_TileVertexSlots[pointRow][pointColumn],
              f_TileVertexSlots[pointRow][pointColumn + 1],
              f_TileVertexSlots[pointRow + 1][pointColumn], 0,
              f_TileVertexSlots[pointRow][pointColumn + 1],
              f_TileVertexSlots[pointRow + 1][pointColumn + 1],
              f_TileVertexSlots[pointRow + 1][pointColumn], 0);
        }
      }
    }
  }

  gSPEndDisplayList(gfx++);
  SetTileTextureCoords(column, row, &(*vertexCursor)->v, &vertex->v);
  SetTileTextureCoords(
      column, row, &midpoints->v, &midpoints[TILE_MIDPOINT_COUNT].v);
  *gfxCursor = gfx;
  *vertexCursor = vertex;
}

/*
 * Builds every ground tile's display list and vertices in the work area,
 * reporting an error if either overflows its buffer
 */
void BuildGroundDisplayLists(void) {
  Gfx* gfx;
  Vtx* vertices;
  s32 column;
  s32 row;

  gfx = GROUND_DISPLAY_LIST_START;
  vertices = GROUND_VERTEX_START;
  for (row = 0; row < GROUND_TILE_ROW_COUNT; row++) {
    for (column = 0; column < GROUND_TILE_COLUMN_COUNT; column++) {
      BuildGroundTileDisplayList(column, row, &gfx, &vertices);
    }
  }

  if ((u32)gfx > (u32)GROUND_DISPLAY_LIST_END ||
      (u32)vertices > (u32)GROUND_VERTEX_END) {
    ReportDivisionError();
  }
}
