#ifndef TERRAIN_H
#define TERRAIN_H

#include <ultra64.h>
#include "common.h"
#include "functions.h"

/*
 * The ground is a grid of points, GROUND_POINT_COLUMN_COUNT by
 * GROUND_POINT_ROW_COUNT. The points at even columns and rows are the grid
 * vertices, which the hole sets. The rest are midpoints between them, built
 * by averaging. The grid is cut into tiles of GROUND_TILE_POINT_SPAN points
 * a side; each tile keeps its own copy of its midpoints.
 */
#define GROUND_POINT_COLUMN_COUNT 33
#define GROUND_POINT_ROW_COUNT 65
#define GROUND_TILE_POINT_SPAN 4
#define GROUND_TILE_COLUMN_COUNT 8
#define GROUND_TILE_ROW_COUNT 16
#define GROUND_TILE_COUNT (GROUND_TILE_COLUMN_COUNT * GROUND_TILE_ROW_COUNT)
/* A ground tile spans 1 << GROUND_TILE_SHIFT course units */
#define GROUND_TILE_SHIFT 19
#define GROUND_WIDTH (GROUND_TILE_COLUMN_COUNT << GROUND_TILE_SHIFT)
#define GROUND_LENGTH (GROUND_TILE_ROW_COUNT << GROUND_TILE_SHIFT)
/* Each tile's midpoints: the odd points of its top-left 4 by 4 */
#define TILE_MIDPOINT_COUNT 16
/* Grid vertices kept, with room for one more column and row than the grid has */
#define GROUND_GRID_VERTEX_COLUMN_COUNT 18
#define GROUND_GRID_VERTEX_ROW_COUNT 34
#define GROUND_GRID_VERTEX_SLOT_COUNT 594

/*
 * Each tile's map image: GROUND_TILE_TEXEL_SPAN texels a side, inside a
 * border. A texel covers 1 << GROUND_TEXEL_SHIFT course units a side.
 */
#define GROUND_TILE_TEXEL_SPAN 32
#define GROUND_TILE_TEXEL_ROW_COUNT 34
#define GROUND_TILE_TEXEL_ROW_LENGTH 36
#define GROUND_TEXEL_SHIFT 14

typedef struct TerrainColor {
    /* 0x0 */ s8 red;
    /* 0x1 */ s8 green;
    /* 0x2 */ s8 blue;
    /* 0x3 */ s8 alpha;
} TerrainColor; /* size = 0x4 */

typedef struct GroundTile {
    /* 0x000 */ u16 texels[GROUND_TILE_TEXEL_ROW_COUNT][GROUND_TILE_TEXEL_ROW_LENGTH]; /* RGBA16 */
    /* 0x990 */ s16 unk990;
    /* 0x992 */ u16 unk992;
    /* 0x994 */ s32 detail; /* How finely the tile is drawn this frame, negative when hidden */
    /* 0x998 */ Vec3 center; /* The tile's middle grid vertex */
    /* 0x9A4 */ s32 midpointBlockIndex; /* Negative when the tile has no midpoints */
    /* 0x9A8 */ u8 unk9A8[0xA00 - 0x9A8];
} GroundTile; /* size = 0xA00 */

/* The terrain codes that GetTerrainCode reads from the hole */
#define TERRAIN_CODE_FAIRWAY 0
#define TERRAIN_CODE_ROUGH 1
#define TERRAIN_CODE_BUNKER 2
#define TERRAIN_CODE_UNK3 3 /* Read as SURFACE_BARE_GROUND_UNK13 */
#define TERRAIN_CODE_CART_WAY 4
#define TERRAIN_CODE_DEEP_ROUGH 5
#define TERRAIN_CODE_GREEN 6
#define TERRAIN_CODE_OUT_OF_BOUNDS 7
#define TERRAIN_CODE_TEE_GROUND 8
#define TERRAIN_CODE_ROCK 9
#define TERRAIN_CODE_GREEN_EDGE 10
#define TERRAIN_CODE_SEMI_ROUGH 11
#define TERRAIN_CODE_BARE_GROUND 12
#define TERRAIN_CODE_WASTE_AREA 14
#define TERRAIN_CODE_COUNT 16

/* Each terrain code's map colour, which the hole loader sets */
extern u16 g_TerrainCodeColors[TERRAIN_CODE_COUNT];

/*
 * The hole's terrain codes, by 16-texel cell: -1 when the cell has none,
 * the code itself when the whole cell has one, or else 0x8000 plus the
 * index of the cell's block of packed codes in g_WorkArea.hole.terrainCodes
 */
#define TERRAIN_CODE_CELL_ROW_COUNT 32
#define TERRAIN_CODE_CELL_COLUMN_COUNT 16
/* The map texels a terrain code cell covers each way */
#define TERRAIN_CODE_CELL_SPAN 16
/* Marks a cell with no terrain code */
#define NO_CELL_TERRAIN_CODE -1
/* Marks a cell whose codes are in a block, with the block's index */
#define TERRAIN_CODE_CELL_BLOCK 0x8000
/* The size of a block of packed codes, two to a byte, and how many there are */
#define TERRAIN_CODE_BLOCK_SIZE (TERRAIN_CODE_CELL_SPAN * TERRAIN_CODE_CELL_SPAN / 2)
#define TERRAIN_CODE_BLOCK_COUNT 0x180
/* The bits of a packed terrain code, and the bits of the even column's code */
#define TERRAIN_CODE_BITS 4
#define TERRAIN_CODE_MASK 0xF
#define EVEN_COLUMN_CODE_MASK (TERRAIN_CODE_MASK << TERRAIN_CODE_BITS)
extern s16 g_TerrainCodeCells[TERRAIN_CODE_CELL_ROW_COUNT][TERRAIN_CODE_CELL_COLUMN_COUNT];

/* Each ground tile's height range, which also serves as the radius of its bounding sphere */
extern f32 g_GroundTileHeightRanges[GROUND_TILE_COUNT];

Vtx *GetGroundPoint(u32 column, u32 row);
Vtx *GetGridVertex(u32 column, u32 row);
s32 SetGroundPoint(s32 column, s32 row, Vtx_t *point);
void PrepareHoleTriangles(void);
s32 IsOverlayInExpansionMemory(void);
void UpdateDebugScenes(void);
s32 GetTerrainCodeColor(s32 code);
s32 GetColorTerrainCode(s32 color);
s32 BlendSurfaceMapColor(s32 x, s32 z, s32 surface);
s32 FindSurface(s32 x, s32 z, s32 lowersWaterLevel);
s32 GetSurface(s32 x, s32 z);
s32 FindGroundTriangle(s32 x, s32 z, PlaneTriangle *triangle);
s32 GetGroundOrWaterHeight(s32 x, s32 z);

#endif
