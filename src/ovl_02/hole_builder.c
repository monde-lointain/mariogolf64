#include "billboard.h"
#include "common.h"
#include "functions.h"
#include "hole_loader.h"
#include "save.h"
#include "terrain.h"
#include "work_area.h"

/* A ground tile's vertices start at its column and row shifted by this */
#define TILE_ORIGIN_SHIFT 9
/* Texture coordinates are S10.5 fixed point */
#define TEXTURE_COORD_FRACTION_BITS 5
/* Where texture coordinates start at a tile's origin */
#define TILE_TEXTURE_ORIGIN 0x100
/* How much mini golf vertices are scaled along x and z for their textures */
#define MINI_GOLF_TEXTURE_SCALE_X 4
#define MINI_GOLF_TEXTURE_SCALE_Z 8
/* Grid vertices lie on tile corners and halfway between them */
#define TILE_SPAN (1 << TILE_ORIGIN_SHIFT)
#define HALF_TILE_SPAN (TILE_SPAN / 2)
#define GRID_ROW_VERTEX_COUNT (GROUND_TILE_COLUMN_COUNT * 2 + 1)
#define GRID_VERTEX_COUNT (GRID_ROW_VERTEX_COUNT * (GROUND_TILE_ROW_COUNT * 2 + 1))
/* Grid texture coordinates: four per unit, starting one texel in */
#define GRID_TEXTURE_SCALE 4
#define GRID_TEXTURE_BORDER (1 << TEXTURE_COORD_FRACTION_BITS)
/* Vertex flags in the hole's vertex file */
#define VERTEX_FLAG_SAME_X 0x40
#define VERTEX_FLAG_SAME_Z 0x80
/* Flags that end a list of vertices */
#define VERTEX_LIST_END 9
/* The size cleared billboards are given */
#define CLEARED_BILLBOARD_WIDTH 64
#define CLEARED_BILLBOARD_HEIGHT 256
/* Where the build buffer starts */
#define BUILD_BUFFER_START ((u8 *)0x80315000)

extern u32 sPaintSeed;
extern s32 sIsCodeMapCleaned;
extern u8 *sReadCursor;
extern u8 *sBuildBuffer;
extern u8 *sBuildCursor;
extern s32 g_TerrainCodeBlockCount;

void LoadCodeMap(char *path);
void EncodeCodeMap(void);
void CleanCodeMap(void);
void PaintGroundTiles(void);
void ClearHolePoints(void);
void ShadeGroundTiles(void);
void CastBillboardShadows(void);

/* Copies bytes from the read cursor and advances it past them */
static inline void ReadFileBytes(void *destination, s32 size) {
    u8 *byte = destination;
    u8 *source = sReadCursor;
    u8 *end = sReadCursor + size;

    while (source != end) {
        *byte++ = *source++;
    }
    sReadCursor = end;
}

/* Seeds the random numbers that paint logic draws */
void SetPaintSeed(u32 seed) {
    sPaintSeed = seed;
}

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_8024D80C_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_8024DA78_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_8024E02C_ovl_02);

/* Empties every terrain code cell and frees the packed code blocks */
#ifdef NON_MATCHING
void ResetTerrainCodeCells(void) {
    s32 row;
    s32 column;

    g_TerrainCodeBlockCount = 0;
    for (row = 0; row < TERRAIN_CODE_CELL_ROW_COUNT; row++) {
        for (column = TERRAIN_CODE_CELL_COLUMN_COUNT - 1; column >= 0; column--) {
            g_TerrainCodeCells[row][column] = -1;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", ResetTerrainCodeCells);
#endif

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_8024E2D4_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_8024E514_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_8024E71C_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_8024E958_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_8024EB98_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_8024EDD8_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_8024F014_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_8024F118_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_8024F21C_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_8024F308_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", EncodeCodeMap);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", LoadCodeMap);

/* Repaints the ground tiles from the code map saved before the last clean-up */
void LoadBackupCodeMap(void) {
    LoadCodeMap("c:/golf64/attr.tmp");
}

/* Repaints the ground tiles from the tool's code map */
void LoadToolCodeMap(void) {
    LoadCodeMap("tool/golf64.att");
    sIsCodeMapCleaned = FALSE;
}

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", CleanCodeMap);

/* Does nothing */
void FinishCodeMapCleanup(void) {
}

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", PaintGroundTiles);

/*
 * Cleans up the terrain code map and repaints the ground tiles from it,
 * or, if that was done last, restores the code map from before
 */
void ToggleCodeMapCleanup(void) {
    /* Unused; it is there to match */
    u8 unused[0x640];

    if (!sIsCodeMapCleaned) {
        EncodeCodeMap();
        sIsCodeMapCleaned = TRUE;
        CleanCodeMap();
        PaintGroundTiles();
        FinishCodeMapCleanup();
    } else {
        LoadBackupCodeMap();
        sIsCodeMapCleaned = FALSE;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_8024FF74_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_80250194_ovl_02);

/* Returns the paint logic that a course's terrain is painted with */
s32 GetCoursePaintLogic(s32 course) {
    switch (course) {
        case 0:
            return 0;
        case 1:
            return 1;
        case 2:
            return 2;
        case 3:
            return 3;
        case 4:
            return 4;
        case 5:
            return 5;
        case 6:
            return 6;
        case 7:
            return 7;
        case 8:
            return 12;
    }
    return 0;
}

/* Does nothing; the course and hole are not read */
void FinishHolePoints(s32 course, s32 hole) {
}

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_802504E8_ovl_02);
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_80255168_ovl_02);
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_80255188_ovl_02);
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_802551A8_ovl_02);
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_802551C8_ovl_02);
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_802551E8_ovl_02);
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_80255208_ovl_02);
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_80255228_ovl_02);
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_80255248_ovl_02);
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_8025524C_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_802509BC_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_80250C54_ovl_02);

/* Clears everything a hole is built into */
void ClearHoleBuild(void) {
    ClearHolePoints();
    ClearTileBorders();
    InitTerrainTextures();
    ClearBillboards();
    ClearHoleTriangles();
    ResetBuildBuffer();
}

/* Clears the hole's points and, unless g_GameState.unk30 is set, its cup and tee */
void ClearHolePoints(void) {
    s32 i;

    if (!g_GameState.unk30) {
        D_800BA9D8.cupPosition[AXIS_X] = 0;
        g_CupPositionY = 0;
        g_CupPositionZ = 0;
        D_800BA9D8.teePosition[AXIS_X] = 0;
        D_800BA9D8.teePosition[AXIS_Y] = 0;
        D_800BA9D8.teePosition[AXIS_Z] = 0;
    }
    for (i = 0; i < CUP_POSITION_COUNT; i++) {
        g_HolePoints.cupPositions[i][AXIS_X] = 0;
        g_HolePoints.cupPositions[i][AXIS_Y] = 0;
        g_HolePoints.cupPositions[i][AXIS_Z] = 0;
        g_HolePoints.unk30[i][AXIS_X] = 0;
        g_HolePoints.unk30[i][AXIS_Y] = 0;
        g_HolePoints.unk30[i][AXIS_Z] = 0;
    }
    for (i = 0; i < ARRAY_COUNT(g_HolePoints.unk60); i++) {
        g_HolePoints.unk60[i][AXIS_X] = 0;
        g_HolePoints.unk60[i][AXIS_Y] = 0;
        g_HolePoints.unk60[i][AXIS_Z] = 0;
        /* The dereference is there to match */
        *g_HolePoints.unkC0[i] = 0;
        g_HolePoints.unkC0[i][AXIS_Z] = 0;
    }
}

/* Points the build buffer and its cursor at the start of the file area */
void ResetBuildBuffer(void) {
    sBuildCursor = sBuildBuffer = sBuildCursor = BUILD_BUFFER_START;
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

/* Clears each ground tile's last texel and unk992 */
void ClearTileBorders(void) {
    s32 i;

    for (i = 0; i < GROUND_TILE_COUNT; i++) {
        g_WorkArea.hole.tiles[i].texels[GROUND_TILE_TEXEL_ROW_COUNT - 1][GROUND_TILE_TEXEL_ROW_LENGTH - 1] = 0;
        g_WorkArea.hole.tiles[i].unk992 = 0;
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
        g_WorkArea.hole.gridVertices[i].v.tc[0] = x * GRID_TEXTURE_SCALE + GRID_TEXTURE_BORDER;
        g_WorkArea.hole.gridVertices[i].v.ob[AXIS_Y] = 0;
        g_WorkArea.hole.gridVertices[i].v.ob[AXIS_Z] = z;
        g_WorkArea.hole.gridVertices[i].v.flag = 0;
        g_WorkArea.hole.gridVertices[i].v.tc[1] = z * GRID_TEXTURE_SCALE + GRID_TEXTURE_BORDER;
        g_WorkArea.hole.gridVertices[i].v.cn[0] = 0;
        g_WorkArea.hole.gridVertices[i].v.cn[1] = 0;
        g_WorkArea.hole.gridVertices[i].v.cn[2] = 0;
        g_WorkArea.hole.gridVertices[i].v.cn[3] = 0;
        x += TILE_SPAN;
        if (x == (GROUND_TILE_COLUMN_COUNT + 1) * TILE_SPAN) {
            x = HALF_TILE_SPAN;
        }
        if (x == GROUND_TILE_COLUMN_COUNT * TILE_SPAN + HALF_TILE_SPAN) {
            x = 0;
            z += HALF_TILE_SPAN;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_802510D4_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_80251288_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_802513B8_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_80251610_ovl_02);

/* Reads each ground tile's unk992 from the file */
void ReadTileUnk992s(void) {
    s32 i;

    for (i = 0; i < GROUND_TILE_COUNT; i++) {
        ReadFileBytes(&g_WorkArea.hole.tiles[i].unk992, sizeof(u16));
    }
}

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
            g_WorkArea.hole.gridVertices[i].v.tc[0] = x * GRID_TEXTURE_SCALE + GRID_TEXTURE_BORDER;
        }
        ReadVertexY(&y, flags);
        g_WorkArea.hole.gridVertices[i].v.ob[AXIS_Y] = y;
        if (!(flags & VERTEX_FLAG_SAME_Z)) {
            ReadVertexZ(&z, flags);
            g_WorkArea.hole.gridVertices[i].v.ob[AXIS_Z] = z;
            g_WorkArea.hole.gridVertices[i].v.tc[1] = z * GRID_TEXTURE_SCALE + GRID_TEXTURE_BORDER;
        }
    }
}

/* Reads the midpoints of the ground tiles from the hole's vertex file, like ReadGridVertices */
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
            g_WorkArea.hole.midpoints[tile][midpoint].v.tc[0] = x * GRID_TEXTURE_SCALE + GRID_TEXTURE_BORDER;
        }
        ReadVertexY(&y, flags);
        g_WorkArea.hole.midpoints[tile][midpoint].v.ob[AXIS_Y] = y;
        if (!(flags & VERTEX_FLAG_SAME_Z)) {
            ReadVertexZ(&z, flags);
            g_WorkArea.hole.midpoints[tile][midpoint].v.ob[AXIS_Z] = z;
            g_WorkArea.hole.midpoints[tile][midpoint].v.tc[1] = z * GRID_TEXTURE_SCALE + GRID_TEXTURE_BORDER;
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
void ReadVertexX(s16 *coord, s32 flags) {
    *coord = 0;
    switch (flags & 3) {
        case 1:
            ReadFileBytes(coord, 1);
            *coord = (u16)*coord >> 8;
            break;
        case 2:
            ReadFileBytes(coord, 1);
            *coord = ((u16)*coord >> 8) << 5;
            break;
        case 3:
            ReadFileBytes(coord, 1);
            *coord = ((u16)*coord >> 8) << 6;
            break;
        case 0:
            ReadFileBytes(coord, 2);
            break;
    }
}

/*
 * Reads a vertex's y coordinate, whose encoding the flags select: two
 * bytes, or one byte scaled up by one of three shifts
 */
void ReadVertexY(s16 *coord, s32 flags) {
    *coord = 0;
    /* The u8 cast is there to match */
    switch ((u8)(flags & 0xC) >> 2) {
        case 1:
            ReadFileBytes(coord, 1);
            *coord = (u16)*coord >> 8;
            break;
        case 2:
            ReadFileBytes(coord, 1);
            *coord = ((u16)*coord >> 8) << 1;
            break;
        case 3:
            ReadFileBytes(coord, 1);
            *coord = ((u16)*coord >> 8) << 2;
            break;
        case 0:
            ReadFileBytes(coord, 2);
            break;
    }
}

/*
 * Reads a vertex's z coordinate, whose encoding the flags select: two
 * bytes, or one byte scaled up by one of three shifts
 */
void ReadVertexZ(s16 *coord, s32 flags) {
    *coord = 0;
    /* The u8 cast is there to match */
    switch ((u8)(flags & 0x30) >> 4) {
        case 1:
            ReadFileBytes(coord, 1);
            *coord = (u16)*coord >> 8;
            break;
        case 2:
            ReadFileBytes(coord, 1);
            *coord = ((u16)*coord >> 8) << 5;
            break;
        case 3:
            ReadFileBytes(coord, 1);
            *coord = ((u16)*coord >> 8) << 6;
            break;
        case 0:
            ReadFileBytes(coord, 2);
            break;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_80251DFC_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", CastBillboardShadows);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_802527FC_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_8025283C_ovl_02);
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_802552BC_ovl_02);
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_802552C4_ovl_02);
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_802552CC_ovl_02);
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_802552D4_ovl_02);
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_802552DC_ovl_02);
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_802552E4_ovl_02);
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_802552EC_ovl_02);
INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_802552F4_ovl_02);

INCLUDE_RODATA("asm/nonmatchings/ovl_02/hole_builder", D_802552FC_ovl_02);
INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", ShadeGroundTiles);

/* Shades the ground tiles, then darkens them under the billboards' shadows */
void ShadeGround(void) {
    ShadeGroundTiles();
    osSyncPrintf("TR");
    CastBillboardShadows();
    osSyncPrintf("EE\n");
}

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_80253270_ovl_02);

/*
 * Gives the vertices from start up to end texture coordinates on the
 * mini golf ground tile at column and row
 */
void SetMiniGolfTileTextureCoords(s32 column, s32 row, Vtx_t *start, Vtx_t *end) {
    Vtx_t *vertex;
    s32 coord;

    for (vertex = start; vertex < end; vertex++) {
        coord = ((vertex->ob[AXIS_X] * MINI_GOLF_TEXTURE_SCALE_X - (column << TILE_ORIGIN_SHIFT))
                << TEXTURE_COORD_FRACTION_BITS) + TILE_TEXTURE_ORIGIN;
        vertex->tc[0] = coord;
        /* The second coordinate's two steps are there to match */
        coord = vertex->ob[AXIS_Z] * MINI_GOLF_TEXTURE_SCALE_Z - (row << TILE_ORIGIN_SHIFT);
        coord = (coord << TEXTURE_COORD_FRACTION_BITS) + TILE_TEXTURE_ORIGIN;
        vertex->tc[1] = coord;
    }
}

/*
 * Gives the vertices from start up to end texture coordinates on the
 * ground tile at column and row
 */
void SetTileTextureCoords(s32 column, s32 row, Vtx_t *start, Vtx_t *end) {
    Vtx_t *vertex;
    s32 coord;

    if (g_GameState.isMiniGolf) {
        SetMiniGolfTileTextureCoords(column, row, start, end);
        return;
    }
    for (vertex = start; vertex < end; vertex++) {
        coord = ((vertex->ob[AXIS_X] - (column << TILE_ORIGIN_SHIFT))
                << TEXTURE_COORD_FRACTION_BITS) + TILE_TEXTURE_ORIGIN;
        vertex->tc[0] = coord;
        /* The second coordinate's two steps are there to match */
        coord = vertex->ob[AXIS_Z] - (row << TILE_ORIGIN_SHIFT);
        coord = (coord << TEXTURE_COORD_FRACTION_BITS) + TILE_TEXTURE_ORIGIN;
        vertex->tc[1] = coord;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_802536FC_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_802537EC_ovl_02);

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", func_802541E8_ovl_02);

