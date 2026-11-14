#ifndef WORK_AREA_H
#define WORK_AREA_H

#include <ultra64.h>

#include "course/hole_triangles.h"
#include "course/terrain.h"

#define WORK_AREA_SIZE 0x827E0
#define TERRAIN_CODE_OFFSET 0x5E920

typedef union WorkArea {
    u8 bytes[WORK_AREA_SIZE];

    struct {
        GroundTile tiles[GROUND_TILE_COUNT];
        Vtx gridVertices[GROUND_GRID_VERTEX_SLOT_COUNT];
        Vtx midpoints[GROUND_TILE_COUNT][TILE_MIDPOINT_COUNT];
        HoleTriangle triangles[HOLE_TRIANGLE_COUNT];
        u8 unusedAfterTriangles[TERRAIN_CODE_OFFSET - 0x5E120];
        u8 terrainCodes[WORK_AREA_SIZE - TERRAIN_CODE_OFFSET];
    } hole;
} WorkArea;

extern WorkArea g_WorkArea;

static inline Vtx *GridVertex(u32 column, u32 row) {
    Vtx *vertex;

    vertex = &g_WorkArea.hole.gridVertices
                  [(row * GROUND_GRID_COLUMN_COUNT) + (column >> 1) +
                      ((column & 1) * GROUND_GRID_EVEN_COLUMN_COUNT)];
    return vertex;
}

#endif
