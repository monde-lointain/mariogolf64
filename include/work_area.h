#ifndef WORK_AREA_H
#define WORK_AREA_H

#include <ultra64.h>

#include "hole_triangles.h"
#include "terrain.h"

/* Bytes in g_WorkArea, part of which holds the characters' buffers */
#define WORK_AREA_SIZE 0x827E0
/* Where the hole's packed terrain codes start, two to a byte */
#define TERRAIN_CODE_OFFSET 0x5E920

/*
 * A buffer whose parts serve the characters, the frames' vertices and,
 * while a hole is played, the hole's triangles
 */
typedef union WorkArea {
    u8 bytes[WORK_AREA_SIZE];

    struct {
        /* 0x00000 */ GroundTile tiles[GROUND_TILE_COUNT];
        /* 0x50000 */ Vtx gridVertices[GROUND_GRID_VERTEX_SLOT_COUNT];
        /* 0x52520 */ Vtx midpoints[GROUND_TILE_COUNT][TILE_MIDPOINT_COUNT];
        /* 0x5A520 */ HoleTriangle triangles[HOLE_TRIANGLE_COUNT];
        /* 0x5E120 */ u8 unusedAfterTriangles[TERRAIN_CODE_OFFSET - 0x5E120];
        /* 0x5E920 */ u8 terrainCodes[WORK_AREA_SIZE - TERRAIN_CODE_OFFSET];
    } hole;
} WorkArea;

extern WorkArea g_WorkArea;

/*
 * Returns the grid vertex at a vertex column and row, as GetGridVertex finds
 * it, without checking that they are on the grid. Each row holds its even
 * columns' vertices and then its odd columns'.
 *
 * It is an inline function that returns a local, to match: see DrawGround.
 */
static inline Vtx *GridVertex(u32 column, u32 row) {
    Vtx *vertex;

    vertex = &g_WorkArea.hole.gridVertices
                  [(row * GROUND_GRID_COLUMN_COUNT) + (column >> 1) +
                      ((column & 1) * GROUND_GRID_EVEN_COLUMN_COUNT)];
    return vertex;
}

#endif
