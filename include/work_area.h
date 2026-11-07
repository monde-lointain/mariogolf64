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

#endif
