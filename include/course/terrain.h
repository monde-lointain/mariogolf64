/*
 * The terrain: its codes, the surface at a point, and the ground's grid, tiles,
 * triangles and heights.
 */

#ifndef TERRAIN_H
#define TERRAIN_H

#include <ultra64.h>

#include "course/surface.h"
#include "math/geometry.h"
#include "system/c_language.h"
#include "system/compiler.h"

// The ground is a grid of points, GROUND_POINT_COLUMN_COUNT by
// GROUND_POINT_ROW_COUNT. The points at even columns and rows are the grid
// vertices, which the hole sets. The rest are midpoints between them, built
// by averaging. The grid is cut into tiles of GROUND_TILE_POINT_SPAN points
// a side; each tile keeps its own copy of its midpoints.
#define GROUND_POINT_COLUMN_COUNT 33
#define GROUND_POINT_ROW_COUNT 65
#define GROUND_TILE_POINT_SPAN_SHIFT 2
#define GROUND_TILE_POINT_SPAN (1 << GROUND_TILE_POINT_SPAN_SHIFT)
#define GROUND_TILE_COLUMN_COUNT 8
#define GROUND_TILE_ROW_COUNT 16
#define GROUND_TILE_COUNT (GROUND_TILE_COLUMN_COUNT * GROUND_TILE_ROW_COUNT)
// A ground tile spans 1 << GROUND_TILE_SHIFT course units
#define GROUND_TILE_SHIFT 19
#define GROUND_WIDTH (GROUND_TILE_COLUMN_COUNT << GROUND_TILE_SHIFT)
#define GROUND_LENGTH (GROUND_TILE_ROW_COUNT << GROUND_TILE_SHIFT)
// The mini-golf ground is this many course units a side
#define MINI_GOLF_GROUND_SIZE 0x100000
// Each tile's midpoints: the points of its GROUND_TILE_POINT_SPAN + 1 a side,
// edges included, that aren't grid vertices
#define TILE_MIDPOINT_COUNT 16

// The midpoints' slots, by each point's row and column within the tile: the
// edges' first, then the inner ones, which are drawn as one run
enum TileMidpointSlot {
    TILE_MIDPOINT_ROW0_COLUMN1,
    TILE_MIDPOINT_ROW0_COLUMN3,
    TILE_MIDPOINT_ROW1_COLUMN0,
    TILE_MIDPOINT_ROW1_COLUMN4,
    TILE_MIDPOINT_ROW3_COLUMN0,
    TILE_MIDPOINT_ROW3_COLUMN4,
    TILE_MIDPOINT_ROW4_COLUMN1,
    TILE_MIDPOINT_ROW4_COLUMN3,
    TILE_MIDPOINT_ROW1_COLUMN1,
    TILE_MIDPOINT_ROW1_COLUMN2,
    TILE_MIDPOINT_ROW1_COLUMN3,
    TILE_MIDPOINT_ROW2_COLUMN1,
    TILE_MIDPOINT_ROW2_COLUMN3,
    TILE_MIDPOINT_ROW3_COLUMN1,
    TILE_MIDPOINT_ROW3_COLUMN2,
    TILE_MIDPOINT_ROW3_COLUMN3
};

// The inner midpoints' run
#define TILE_FIRST_INNER_MIDPOINT TILE_MIDPOINT_ROW1_COLUMN1
#define TILE_INNER_MIDPOINT_COUNT (TILE_MIDPOINT_COUNT - TILE_FIRST_INNER_MIDPOINT)
// Each side of a tile has a midpoint between each pair of its grid vertices
#define TILE_SIDE_MIDPOINT_COUNT GROUND_TILE_GRID_SPAN
// The grid's vertices are every other ground point each way. Each row of them
// is kept as its even columns, then its odd ones.
#define GROUND_GRID_COLUMN_COUNT ((GROUND_POINT_COLUMN_COUNT + 1) / 2)
#define GROUND_GRID_ROW_COUNT ((GROUND_POINT_ROW_COUNT + 1) / 2)
#define GROUND_GRID_EVEN_COLUMN_COUNT ((GROUND_GRID_COLUMN_COUNT + 1) / 2)
// A tile spans this many grid columns and rows
#define GROUND_TILE_GRID_SPAN (GROUND_TILE_POINT_SPAN / 2)
// Each grid point's slot among its tile's midpoints, by its row and column within the tile. The
// points with the top bit set are grid vertices, which GetGridVertex returns instead.
#define GRID_VERTEX_SLOT_FLAG 0x80
#define GridVertexSlot(index) (GRID_VERTEX_SLOT_FLAG | (index))
// The mini-golf ground's tiles span 1 << these course units
#define MINI_GOLF_TILE_X_SHIFT 17
#define MINI_GOLF_TILE_Z_SHIFT 16
// An interpolation weight runs from 0, all the first end, to this, all the
// second
#define EDGE_WEIGHT_MAX 16
// The grid vertex at a grid column and row within a tile
#define TileGridVertex(tileColumn, tileRow, gridColumn, gridRow) \
    GetGridVertex( \
        ((tileColumn) * GROUND_TILE_GRID_SPAN) + (gridColumn), \
        ((tileRow) * GROUND_TILE_GRID_SPAN) + (gridRow) \
    )
// Grid vertices kept, with room for one more column and row than the grid has
#define GROUND_GRID_VERTEX_COLUMN_COUNT 18
#define GROUND_GRID_VERTEX_ROW_COUNT 34
#define GROUND_GRID_VERTEX_SLOT_COUNT 594

// Each tile's map image: GROUND_TILE_TEXEL_SPAN texels a side, inside a
// border. A texel covers 1 << GROUND_TEXEL_SHIFT course units a side.
#define GROUND_TILE_TEXEL_SPAN 32
#define GROUND_TILE_TEXEL_ROW_COUNT 34
#define GROUND_TILE_TEXEL_ROW_LENGTH 36
// A tile's place in the tile size coordinates, in quarter texels. A bordered
// tile's texture starts half a texel into its place and ends past the start of
// the next tile's place by these many.
#define TILE_COORDINATE_SPAN (GROUND_TILE_TEXEL_SPAN * 4)
#define BORDERED_TILE_FIRST_COORDINATE 2
#define BORDERED_TILE_S_PAST_NEXT 6
#define BORDERED_TILE_T_PAST_NEXT 2
#define GROUND_TEXEL_SHIFT 14
#define GROUND_TEXEL_SIZE (1 << GROUND_TEXEL_SHIFT)

typedef struct TerrainColor {
    s8 red;
    s8 green;
    s8 blue;
    s8 alpha;
} TerrainColor;

// GroundTile.detail, how finely DrawGround draws a tile this frame
enum GroundDetail {
    GROUND_DETAIL_HIDDEN = -1,
    GROUND_DETAIL_COARSE, // Two triangles between the tile's corners
    GROUND_DETAIL_MEDIUM, // Eight triangles between its grid vertices
    GROUND_DETAIL_FINE // Its own display list, midpoints and all
};

typedef s32 GroundDetail;

typedef struct GroundTile {
    u16 texels[GROUND_TILE_TEXEL_ROW_COUNT][GROUND_TILE_TEXEL_ROW_LENGTH]; // RGBA16
    s16 extraPassCount; // Positive to draw the tile again over itself; the game only clears it
    // A bit for each of the tile's cells, row by row, set when the cell is drawn mirrored
    u16 mirroredCells;
    GroundDetail detail; // How finely the tile is drawn this frame, negative when hidden
    Vec3 center; // The tile's middle grid vertex
    s32 midpointBlockIndex; // Negative when the tile has no midpoints
    u8 unusedAfterMidpointBlock[0xA00 - 0x9A8];
} GroundTile;

// The terrain codes that GetTerrainCode reads from the hole
enum TerrainCode {
    TERRAIN_CODE_NONE = -1,
    TERRAIN_CODE_FAIRWAY,
    TERRAIN_CODE_ROUGH,
    TERRAIN_CODE_BUNKER,
    // Bare ground that never becomes the point where a ball in a water hazard
    // is dropped; read as SURFACE_NO_DROP_GROUND
    TERRAIN_CODE_NO_DROP_GROUND,
    TERRAIN_CODE_CART_WAY,
    TERRAIN_CODE_DEEP_ROUGH,
    TERRAIN_CODE_GREEN,
    TERRAIN_CODE_OUT_OF_BOUNDS,
    TERRAIN_CODE_TEE_GROUND,
    TERRAIN_CODE_ROCK,
    TERRAIN_CODE_GREEN_EDGE,
    TERRAIN_CODE_SEMI_ROUGH,
    TERRAIN_CODE_BARE_GROUND,
    TERRAIN_CODE_WASTE_AREA = 14
};

typedef s32 TerrainCode;

#define TERRAIN_CODE_COUNT 16

// Each terrain code's map color, which the hole loader sets
extern u16 g_TerrainCodeColors[TERRAIN_CODE_COUNT];

// The hole's terrain codes, by 16-texel cell: -1 when the cell has none,
// the code itself when the whole cell has one, or else 0x8000 plus the
// index of the cell's block of packed codes in g_WorkArea.hole.terrainCodes
#define TERRAIN_CODE_CELL_ROW_COUNT 32
#define TERRAIN_CODE_CELL_COLUMN_COUNT 16
// The size of a terrain code cell, and of a row of them
#define TERRAIN_CODE_CELL_SIZE 2
#define TERRAIN_CODE_CELL_ROW_SIZE (TERRAIN_CODE_CELL_COLUMN_COUNT * TERRAIN_CODE_CELL_SIZE)
// The map texels a terrain code cell covers each way
#define TERRAIN_CODE_CELL_SPAN 16
// The terrain code grid's columns and rows
#define CODE_GRID_COLUMN_COUNT (TERRAIN_CODE_CELL_COLUMN_COUNT * TERRAIN_CODE_CELL_SPAN)
#define CODE_GRID_ROW_COUNT (TERRAIN_CODE_CELL_ROW_COUNT * TERRAIN_CODE_CELL_SPAN)
// Marks a cell with no terrain code
#define NO_CELL_TERRAIN_CODE (-1)
// Marks a cell whose codes are in a block, with the block's index
#define TERRAIN_CODE_CELL_BLOCK 0x8000
// The size of a block of packed codes, two to a byte, and how many there are
#define TERRAIN_CODE_BLOCK_SIZE (TERRAIN_CODE_CELL_SPAN * TERRAIN_CODE_CELL_SPAN / 2)
#define TERRAIN_CODE_BLOCK_COUNT 0x180
// The bits of a packed terrain code, and the bits of the even column's code
#define TERRAIN_CODE_BITS 4
#define TERRAIN_CODE_MASK 0xF
#define EVEN_COLUMN_CODE_MASK (TERRAIN_CODE_MASK << TERRAIN_CODE_BITS)
extern s16 g_TerrainCodeCells[TERRAIN_CODE_CELL_ROW_COUNT][TERRAIN_CODE_CELL_COLUMN_COUNT];

// Each ground tile's height range, which also serves as the radius of its
// bounding sphere
extern f32 g_GroundTileHeightRanges[GROUND_TILE_COUNT];
// How many of the work area's terrain code blocks hole_builder.c has used
extern s32 g_TerrainCodeBlockCount;
// Set before some ground height queries and around PrepareHoleScene's
// scene set-up, but read by no routine
extern s32 g_UnreadGroundQueryMarker;

// The vertices of a ground tile's edge or middle-line midpoints go through a
// segment, which can move them to coarse vertices halfway between their
// neighbors. g_CoarseVertexOffsets holds each segment's offset to them.
#define TILE_VERTEX_SEGMENT_COUNT 8

extern Gfx *g_GroundTileDisplayLists[GROUND_TILE_COUNT];
extern s32 g_CoarseVertexOffsets[GROUND_TILE_COUNT][TILE_VERTEX_SEGMENT_COUNT];

// The plane through a triangle in fixed point, with a normal of length
// INTEGER_PLANE_UNIT
#define INTEGER_PLANE_SHIFT 8
#define INTEGER_PLANE_UNIT (1 << INTEGER_PLANE_SHIFT)

typedef struct IntegerPlane {
    s32 normal[AXIS_COUNT];
    s32 distance;
    s32 normalLength; // Always INTEGER_PLANE_UNIT
} IntegerPlane;

// Drawing the ground, which DrawGround and the hole editor's copy share

// A tile's edges, in the order of the segments that move their vertices
#define TILE_EDGE_TOP 0
#define TILE_EDGE_LEFT 1
#define TILE_EDGE_RIGHT 2
#define TILE_EDGE_BOTTOM 3
#define TILE_EDGE_COUNT 4

// The ground is drawn in two halves, each a column of groups of four tiles
#define GROUND_TILE_GROUP_SIZE 4
#define GROUND_HALF_COUNT 2
#define GROUND_HALF_VERTEX_COLUMNS (GROUND_TILE_GROUP_SIZE * 2)
// The first vertex column of one of the ground's halves
#define HalfFirstColumn(half) ((half) * GROUND_HALF_VERTEX_COLUMNS)

// A visible tile is drawn finely when its view depth times this is under its
// height range, and at medium detail when the depth times the medium scale is
#define FINE_DEPTH_SCALE (-0.025f)
#define ZOOMED_FINE_DEPTH_SCALE (-0.0125f)
#define MEDIUM_DEPTH_SCALE (-0.01f)
#define GROUND_TILE_RADIUS_MIN 384.0f
#define NEAR_DEPTH_DIVISOR 8
#define NO_NEAR_DEPTH (-1000.0f)

// DrawGround's terrainStyle: the round's view, or one of the map views
enum TerrainStyle {
    TERRAIN_STYLE_NORMAL,
    TERRAIN_STYLE_MAP = 2,
    TERRAIN_STYLE_TOP_DOWN_MAP
};

typedef s32 TerrainStyle;

// Whether a terrain style is one of the map views
#define IsMapTerrainStyle(style) \
    ((style) == TERRAIN_STYLE_MAP || (style) == TERRAIN_STYLE_TOP_DOWN_MAP)

// The vertex cache slots of a row's three vertex rows, which rotate from row to
// row
#define FIRST_TOP_SLOT 9
#define FIRST_MIDDLE_SLOT 18
#define FIRST_BOTTOM_SLOT 0
// A vertex row's odd vertices follow its even ones
#define ODD_SLOT (GROUND_TILE_GROUP_SIZE + 1)

// A tile whose extraPassCount is positive is drawn a second time
#define GROUND_TILE_PASSES_MAX 2

// The noise texture sits in TMEM after a tile's texels, and is blended in by
// this
#define NOISE_TMEM \
    (GROUND_TILE_TEXEL_ROW_COUNT * GROUND_TILE_TEXEL_ROW_LENGTH * G_IM_SIZ_16b_BYTES / 8)
#define NOISE_BLEND_ALPHA 110

#define CLIP_RATIO_COUNT 6
#define CLIP_RATIO_TEXT_COLUMN 10
#define CLIP_RATIO_TEXT_ROW 12
// DrawGround formats the clip ratio's debug text into a buffer this big
#define CLIP_RATIO_TEXT_SIZE 0x98
// osGetCount ticks per unit of the printed time
#define GROUND_TIMING_TICKS 3000
#define GROUND_TIMING_COLUMN 5
#define GROUND_TIMING_ROW 11

// Whether a fine tile's neighbor makes it snap the vertices of their shared
// edge's middle
#define IsCoarse(tileIndex) (g_WorkArea.hole.tiles[tileIndex].detail == GROUND_DETAIL_COARSE)

// Draws a tile's 2 by 2 quads from the cache slots of its three vertex rows
#define DrawTileQuads( \
    top, \
    topOdd, \
    topNext, \
    middle, \
    middleOdd, \
    middleNext, \
    bottom, \
    bottomOdd, \
    bottomNext \
) \
    { \
        gSP2Triangles(gfx++, top, topOdd, middle, 0, topOdd, middleOdd, middle, 0); \
        gSP2Triangles(gfx++, middle, middleOdd, bottom, 0, middleOdd, bottomOdd, bottom, 0); \
        gSP2Triangles(gfx++, topOdd, topNext, middleOdd, 0, topNext, middleNext, middleOdd, 0); \
        gSP2Triangles( \
            gfx++, \
            middleOdd, \
            middleNext, \
            bottomOdd, \
            0, \
            middleNext, \
            bottomNext, \
            bottomOdd, \
            0 \
        ); \
    }

extern Vtx *g_VertexListHead;
extern Boolean8 g_ShowsGroundWaves;
extern f32 g_GroundFarthestDepth;
extern f32 g_GroundNearDepthEighth;

#define TRIANGLE_CORNER_COUNT 3

// A triangle as GetTrianglePlane takes it: each axis's coordinates, by corner
typedef struct PlaneTriangle {
    // What was hit: the kind of a hole triangle, the ring's
    // index from FindRingHit, or the billboard cell that FindBillboardHit
    // writes here
    s32 tag;
    s16 corners[AXIS_COUNT][TRIANGLE_CORNER_COUNT];
} PlaneTriangle;

// The length of the legs of the club test's ground triangle
#define FLAT_TRIANGLE_LEG 0x100

// Sets a triangle's corners to the club test's ground: level at height 0,
// with a right angle at the origin and a leg along each of X and Z.
//
// It is an inline function to match. Taking the triangle's address ahead of
// the stores, as the ball simulations' original code does, needs the address
// in a variable; with the stores written to the local itself, the address is
// only worked out for GetTrianglePlane's call after them. It is at file
// scope, as one nested in StepOpeningBallSimulation changes 174 of its words.
static inline void SetFlatTriangle(PlaneTriangle *triangle) {
    triangle->corners[AXIS_X][0] = 0;
    triangle->corners[AXIS_X][1] = FLAT_TRIANGLE_LEG;
    triangle->corners[AXIS_X][2] = 0;
    triangle->corners[AXIS_Y][0] = 0;
    triangle->corners[AXIS_Y][1] = 0;
    triangle->corners[AXIS_Y][2] = 0;
    triangle->corners[AXIS_Z][0] = 0;
    triangle->corners[AXIS_Z][1] = 0;
    triangle->corners[AXIS_Z][2] = FLAT_TRIANGLE_LEG;
}

// The plane through a triangle: normal . point + distance = 0, with the
// normal's y at least 0
typedef struct TrianglePlane {
    f32 normal[AXIS_COUNT];
    f32 distance;
    f32 normalLength; // Always 1
} TrianglePlane;

// Returns 0, or -1 for a triangle without area, whose plane is then level
Outcome GetTrianglePlane(PlaneTriangle *triangle, TrianglePlane *plane);

Vtx *GetGroundPoint(u32 column, u32 row);
Vtx *GetGridVertex(u32 column, u32 row);
NO_RESULT SetGroundPoint(s32 column, s32 row, Vtx_t *point);
// The original's hole_editor.c calls SetGroundPointHeightAndShade without a
// prototype, so BlendGroundAroundPoint's float height goes as a double, in the
// registers of both the height and the shade: the routine takes the double's
// high word as the height and its low word as the shade, and
// BLENDED_POINT_SHADE is lost. hole_editor.c defines
// CALLS_GROUND_POINT_SETTER_UNPROTOTYPED to do so, unless AVOID_UB is set.
#if !defined(AVOID_UB) && defined(CALLS_GROUND_POINT_SETTER_UNPROTOTYPED)
NO_RESULT SetGroundPointHeightAndShade();
#else
NO_RESULT SetGroundPointHeightAndShade(s32 column, s32 row, s32 height, s32 shade);
#endif
void PrepareHoleTriangles(void);
Boolean IsOverlayInExpansionMemory(void);
void UpdateDebugScenes(void);
s32 GetTerrainCodeColor(TerrainCode code);
Outcome GetTriangleIntegerPlane(PlaneTriangle *triangle, IntegerPlane *plane);
TerrainCode GetColorTerrainCode(s32 color);
s32 BlendSurfaceMapColor(s32 x, s32 z, Surface surface);
Surface FindSurface(s32 x, s32 z, Boolean lowersWaterLevel);
Surface GetSurface(s32 x, s32 z);
s32 FindGroundTriangle(s32 x, s32 z, PlaneTriangle *triangle);
s32 GetGroundOrWaterHeight(s32 x, s32 z);
void InitTerrainTextures(void);
// Returns the height of the ground under a point. Coordinates are in
// 1/1024ths of a unit.
s32 GetGroundHeight(s32 x, s32 z);

void BuildAllTileMidpoints(void);
s32 CrossProductXZ(s32 originX, s32 originZ, s32 firstX, s32 firstZ, s32 secondX, s32 secondZ);
TerrainCode GetTerrainCode(s32 column, s32 row);
void MarkBillboardsOnMap(void);

#endif
