#ifndef TERRAIN_H
#define TERRAIN_H

#include <ultra64.h>
#include "common.h"

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
    /* 0x994 */ s32 unk994;
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

Vtx *GetGroundPoint(u32 column, u32 row);
s32 SetGroundPoint(s32 column, s32 row, Vtx_t *point);
void PrepareHoleTriangles(void);
s32 IsOverlayInExpansionMemory(void);
void UpdateDebugScenes(void);

#endif
