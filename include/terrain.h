#ifndef TERRAIN_H
#define TERRAIN_H

#include <ultra64.h>

#include "compiler.h"
#include "geometry.h"
#include "language.h"
#include "surface.h"

#define GROUND_POINT_COLUMN_COUNT 33
#define GROUND_POINT_ROW_COUNT 65
#define GROUND_TILE_POINT_SPAN_SHIFT 2
#define GROUND_TILE_POINT_SPAN (1 << GROUND_TILE_POINT_SPAN_SHIFT)
#define GROUND_TILE_COLUMN_COUNT 8
#define GROUND_TILE_ROW_COUNT 16
#define GROUND_TILE_COUNT (GROUND_TILE_COLUMN_COUNT * GROUND_TILE_ROW_COUNT)
#define GROUND_TILE_SHIFT 19
#define GROUND_WIDTH (GROUND_TILE_COLUMN_COUNT << GROUND_TILE_SHIFT)
#define GROUND_LENGTH (GROUND_TILE_ROW_COUNT << GROUND_TILE_SHIFT)
#define MINI_GOLF_GROUND_SIZE 0x100000
#define TILE_MIDPOINT_COUNT 16

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

#define TILE_FIRST_INNER_MIDPOINT TILE_MIDPOINT_ROW1_COLUMN1
#define TILE_INNER_MIDPOINT_COUNT (TILE_MIDPOINT_COUNT - TILE_FIRST_INNER_MIDPOINT)
#define TILE_SIDE_MIDPOINT_COUNT GROUND_TILE_GRID_SPAN
#define GROUND_GRID_COLUMN_COUNT ((GROUND_POINT_COLUMN_COUNT + 1) / 2)
#define GROUND_GRID_ROW_COUNT ((GROUND_POINT_ROW_COUNT + 1) / 2)
#define GROUND_GRID_EVEN_COLUMN_COUNT ((GROUND_GRID_COLUMN_COUNT + 1) / 2)
#define GROUND_TILE_GRID_SPAN (GROUND_TILE_POINT_SPAN / 2)
#define GRID_VERTEX_SLOT_FLAG 0x80
#define GridVertexSlot(index) (GRID_VERTEX_SLOT_FLAG | (index))
#define MINI_GOLF_TILE_X_SHIFT 17
#define MINI_GOLF_TILE_Z_SHIFT 16
#define EDGE_WEIGHT_MAX 16
#define TileGridVertex(tileColumn, tileRow, gridColumn, gridRow) \
    GetGridVertex( \
        ((tileColumn) * GROUND_TILE_GRID_SPAN) + (gridColumn), \
        ((tileRow) * GROUND_TILE_GRID_SPAN) + (gridRow) \
    )
#define GROUND_GRID_VERTEX_COLUMN_COUNT 18
#define GROUND_GRID_VERTEX_ROW_COUNT 34
#define GROUND_GRID_VERTEX_SLOT_COUNT 594

#define GROUND_TILE_TEXEL_SPAN 32
#define GROUND_TILE_TEXEL_ROW_COUNT 34
#define GROUND_TILE_TEXEL_ROW_LENGTH 36
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

enum GroundDetail {
    GROUND_DETAIL_HIDDEN = -1,
    GROUND_DETAIL_COARSE,
    GROUND_DETAIL_MEDIUM,
    GROUND_DETAIL_FINE
};

typedef s32 GroundDetail;

typedef struct GroundTile {
    u16 texels[GROUND_TILE_TEXEL_ROW_COUNT][GROUND_TILE_TEXEL_ROW_LENGTH];
    s16 extraPassCount;
    u16 mirroredCells;
    GroundDetail detail;
    Vec3 center;
    s32 midpointBlockIndex;
    u8 unusedAfterMidpointBlock[0xA00 - 0x9A8];
} GroundTile;

enum TerrainCode {
    TERRAIN_CODE_NONE = -1,
    TERRAIN_CODE_FAIRWAY,
    TERRAIN_CODE_ROUGH,
    TERRAIN_CODE_BUNKER,
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

extern u16 g_TerrainCodeColors[TERRAIN_CODE_COUNT];

#define TERRAIN_CODE_CELL_ROW_COUNT 32
#define TERRAIN_CODE_CELL_COLUMN_COUNT 16
#define TERRAIN_CODE_CELL_SIZE 2
#define TERRAIN_CODE_CELL_ROW_SIZE (TERRAIN_CODE_CELL_COLUMN_COUNT * TERRAIN_CODE_CELL_SIZE)
#define TERRAIN_CODE_CELL_SPAN 16
#define CODE_GRID_COLUMN_COUNT (TERRAIN_CODE_CELL_COLUMN_COUNT * TERRAIN_CODE_CELL_SPAN)
#define CODE_GRID_ROW_COUNT (TERRAIN_CODE_CELL_ROW_COUNT * TERRAIN_CODE_CELL_SPAN)
#define NO_CELL_TERRAIN_CODE (-1)
#define TERRAIN_CODE_CELL_BLOCK 0x8000
#define TERRAIN_CODE_BLOCK_SIZE (TERRAIN_CODE_CELL_SPAN * TERRAIN_CODE_CELL_SPAN / 2)
#define TERRAIN_CODE_BLOCK_COUNT 0x180
#define TERRAIN_CODE_BITS 4
#define TERRAIN_CODE_MASK 0xF
#define EVEN_COLUMN_CODE_MASK (TERRAIN_CODE_MASK << TERRAIN_CODE_BITS)
extern s16 g_TerrainCodeCells[TERRAIN_CODE_CELL_ROW_COUNT][TERRAIN_CODE_CELL_COLUMN_COUNT];

extern f32 g_GroundTileHeightRanges[GROUND_TILE_COUNT];
extern s32 g_TerrainCodeBlockCount;
extern s32 g_UnreadGroundQueryMarker;

#define TILE_VERTEX_SEGMENT_COUNT 8

extern Gfx *g_GroundTileDisplayLists[GROUND_TILE_COUNT];
extern s32 g_CoarseVertexOffsets[GROUND_TILE_COUNT][TILE_VERTEX_SEGMENT_COUNT];

#define INTEGER_PLANE_SHIFT 8
#define INTEGER_PLANE_UNIT (1 << INTEGER_PLANE_SHIFT)

typedef struct IntegerPlane {
    s32 normal[AXIS_COUNT];
    s32 distance;
    s32 normalLength;
} IntegerPlane;

#define TILE_EDGE_TOP 0
#define TILE_EDGE_LEFT 1
#define TILE_EDGE_RIGHT 2
#define TILE_EDGE_BOTTOM 3
#define TILE_EDGE_COUNT 4

#define GROUND_TILE_GROUP_SIZE 4
#define GROUND_HALF_COUNT 2
#define GROUND_HALF_VERTEX_COLUMNS (GROUND_TILE_GROUP_SIZE * 2)
#define HalfFirstColumn(half) ((half) * GROUND_HALF_VERTEX_COLUMNS)

#define FINE_DEPTH_SCALE (-0.025f)
#define ZOOMED_FINE_DEPTH_SCALE (-0.0125f)
#define MEDIUM_DEPTH_SCALE (-0.01f)
#define GROUND_TILE_RADIUS_MIN 384.0f
#define NEAR_DEPTH_DIVISOR 8
#define NO_NEAR_DEPTH (-1000.0f)

enum TerrainStyle {
    TERRAIN_STYLE_NORMAL,
    TERRAIN_STYLE_MAP = 2,
    TERRAIN_STYLE_TOP_DOWN_MAP
};

typedef s32 TerrainStyle;

#define IsMapTerrainStyle(style) \
    ((style) == TERRAIN_STYLE_MAP || (style) == TERRAIN_STYLE_TOP_DOWN_MAP)

#define FIRST_TOP_SLOT 9
#define FIRST_MIDDLE_SLOT 18
#define FIRST_BOTTOM_SLOT 0
#define ODD_SLOT (GROUND_TILE_GROUP_SIZE + 1)

#define GROUND_TILE_PASSES_MAX 2

#define NOISE_TMEM \
    (GROUND_TILE_TEXEL_ROW_COUNT * GROUND_TILE_TEXEL_ROW_LENGTH * G_IM_SIZ_16b_BYTES / 8)
#define NOISE_BLEND_ALPHA 110

#define CLIP_RATIO_COUNT 6
#define CLIP_RATIO_TEXT_COLUMN 10
#define CLIP_RATIO_TEXT_ROW 12
#define CLIP_RATIO_TEXT_SIZE 0x98
#define GROUND_TIMING_TICKS 3000
#define GROUND_TIMING_COLUMN 5
#define GROUND_TIMING_ROW 11

#define IsCoarse(tileIndex) (g_WorkArea.hole.tiles[tileIndex].detail == GROUND_DETAIL_COARSE)

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

typedef struct PlaneTriangle {
    s32 tag;
    s16 corners[AXIS_COUNT][TRIANGLE_CORNER_COUNT];
} PlaneTriangle;

#define FLAT_TRIANGLE_LEG 0x100

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

typedef struct TrianglePlane {
    f32 normal[AXIS_COUNT];
    f32 distance;
    f32 normalLength;
} TrianglePlane;

Outcome GetTrianglePlane(PlaneTriangle *triangle, TrianglePlane *plane);

Vtx *GetGroundPoint(u32 column, u32 row);
Vtx *GetGridVertex(u32 column, u32 row);
NO_RESULT SetGroundPoint(s32 column, s32 row, Vtx_t *point);
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
s32 GetGroundHeight(s32 x, s32 z);

void BuildAllTileMidpoints(void);
s32 CrossProductXZ(s32 originX, s32 originZ, s32 firstX, s32 firstZ, s32 secondX, s32 secondZ);
TerrainCode GetTerrainCode(s32 column, s32 row);
void MarkBillboardsOnMap(void);

#endif
