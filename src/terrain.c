#include "animated_box.h"
#include "billboard.h"
#include "camera.h"
#include "character_slot.h"
#include "club.h"
#include "common.h"
#include "controller.h"
#include "current_shot.h"
#include "debug_text.h"
#include "functions.h"
#include "game_flags.h"
#include "libc.h"
#include "scene.h"
#include "sound.h"
#include "surface.h"
#include "terrain.h"
#include "variables.h"
#include "work_area.h"

/* The debug scene menu lists SCENE_MENU_LENGTH scenes from text row SCENE_MENU_TOP_ROW */
#define SCENE_MENU_LENGTH 16
#define SCENE_MENU_TOP_ROW 5

/* The limits of SCENE_MAP_OVERVIEW's controls */
#define MAP_OVERVIEW_ADJUSTMENT_MIN 5
#define MAP_OVERVIEW_ADJUSTMENT_MAX 0x200
#define MAP_OVERVIEW_SELECTION_COUNT 7
#define MAP_OVERVIEW_CAMERA_STEP 8
#define MAP_OVERVIEW_ADJUSTMENT_STEP 2
/* The map camera's eye moves this many units for each unit of cameraX and cameraZ */
#define MAP_OVERVIEW_CAMERA_SCALE 16
/* The stick moves the camera one step for each STICK_STEP_SIZE of tilt */
#define STICK_STEP_SIZE 32

/*
 * What SCENE_MAP_OVERVIEW's controls change. Only the camera position is
 * read back; nothing in the ROM reads the other three.
 */
typedef struct MapOverviewControls {
    /* 0x00 */ s32 cameraX;
    /* 0x04 */ s32 cameraZ;
    /* 0x08 */ s32 selectionIndex; /* A and B cycle it */
    /* 0x0C */ s32 adjustment;     /* L and R step it */
    /* 0x10 */ s32 isToggled;      /* Z flips it */
} MapOverviewControls; /* size = 0x14 */

/*
 * The map image texel at a texel column and row; the first texel row and
 * column of each tile are its border
 */
#define MAP_TEXEL(column, row)                                                                         \
    g_WorkArea.hole                                                                                    \
        .tiles[(column) / GROUND_TILE_TEXEL_SPAN + ((row) / GROUND_TILE_TEXEL_SPAN) * GROUND_TILE_COLUMN_COUNT] \
        .texels[1][((column) & (GROUND_TILE_TEXEL_SPAN - 1)) +                                         \
                   ((row) & (GROUND_TILE_TEXEL_SPAN - 1)) * GROUND_TILE_TEXEL_ROW_LENGTH + 1]

/*
 * Water tests probe a vertical segment from -reach to +reach; the height
 * where it hits water is at the hit's fraction of the way along
 */
#define WATER_PROBE_REACH 3072000
#define MINI_GOLF_WATER_PROBE_REACH 614400
#define WATER_PROBE_HEIGHT(fraction, reach) ((fraction) * (2.0f * (reach)) + -(f32)(reach))
/* Raises the water's height for the surface test, or lowers it when asked */
#define WATER_BIAS 0x266
#define LOWERED_WATER_BIAS (-0x600)

extern u8 sPointSlots[GROUND_TILE_POINT_SPAN + 1][8];
extern f32 D_800BAA0C[GROUND_TILE_COUNT];
extern s16 sTerrainCodeCells[32][16];
extern TerrainColor sTerrainColors[11];
extern u16 g_TerrainCodeColors[];
extern s8 sCellSearchOffsets[4];
extern s32 sFlagPanelFirstRow;
extern s32 sFlagPanelColumn;
extern s32 sFlagPanelRow;
extern char g_RomBuildDate[];
extern char g_SaveBuildDate[];
extern s32 sSceneBeforeFlags;
extern MapOverviewControls sMapOverviewControls;
extern s32 sIsSceneMenuOpen;
extern AnimatedBox sSceneMenuBox;
extern s32 sSceneMenuScenes[SCENE_MENU_LENGTH];
extern s32 sIsFirstDebugUpdate;
extern s32 sSceneMenuSelection;
extern s8 D_800C30D1;

void func_8005D9A0(void);
void func_8005F838(s32);
void func_800802E0(void);
void ChooseConditionsAndInitHole(void);
void func_8024F7B0(void);
void func_8024FF14(void);
void func_80253234(s32);
void func_80253270(void);
void func_8040000C(void);
void func_804002C0(void);
void func_80400FBC(void);
void func_80402428(void);
void func_80402E1C(void);
void func_80403A04(void);
void func_80404B84(void);
void func_80406818(void);
void func_80406C3C(void);

void func_802504E8(s32 courseRecordIndex, s32 holeRecordIndex);
s32 FindSurface(s32 x, s32 z, s32 lowersWaterLevel);
s32 GetSurface(s32 x, s32 z);
Vtx *GetGridVertex(u32 column, u32 row);
void InitGridVertex(s32 column, s32 row, s32 height, u8 shade);
void SetGridVertexHeight(s32 column, s32 row, s32 height);
void SetGridVertex(s32 column, s32 row, Vtx_t *point);
/* The flags panel shows FLAG_PANEL_ROW_COUNT rows of FLAG_PANEL_COLUMN_COUNT flags */
#define FLAG_PANEL_ROW_COUNT 8
#define FLAG_PANEL_COLUMN_COUNT 16
/* The first row of the last page, which ends at flag 0xFF */
#define FLAG_PANEL_FIRST_ROW_MAX 8

void BuildTileMidpoints(Vtx *midpoints, s32 tileColumn, s32 tileRow);
void BuildAllTileMidpoints(void);
s32 GetTerrainHeight(s32 x, s32 z);

/*
 * Returns the terrain code at a code column and row, clamped to the grid,
 * or -1 where there is none
 */
#ifdef NON_MATCHING
s32 GetTerrainCode(s32 column, s32 row) {
    s32 block;
    s32 offset;

    if (column < 0) {
        column = 0;
    } else if (column >= 256) {
        column = 255;
    }
    if (row < 0) {
        row = 0;
    } else if (row >= 512) {
        row = 511;
    }
    block = (sTerrainCodeCells[0] + column / 16)[(row / 16) * 16];
    if (block == -1) {
        return -1;
    }
    if (block >= 0) {
        return block;
    }
    offset = ((column & 0xF) >> 1) + (row & 0xF) * 8 + (block & 0x7FFF) * 128;
    if (!(column & 1)) {
        return g_WorkArea.hole.terrainCodes[offset] >> 4;
    }
    return g_WorkArea.hole.terrainCodes[offset] & 0xF;
}

#else
INCLUDE_ASM("asm/nonmatchings/terrain", GetTerrainCode);
#endif

/* Returns the index of the terrain colour nearest an RGBA16 colour */
s32 FindNearestTerrainColor(s32 color) {
    s32 nearestDistance = 1000000;
    s32 nearestIndex = 0;
    s32 colorRed = (color >> 11) & 0x1F;
    s32 colorGreen = (color >> 6) & 0x1F;
    s32 colorBlue = (color >> 1) & 0x1F;
    s32 i;
    s32 red;
    s32 green;
    s32 blue;
    s32 distance;

    for (i = 0; i < 11; i++) {
        red = colorRed - sTerrainColors[i].red;
        if (red < 0) {
            red = -red;
        }
        green = colorGreen - sTerrainColors[i].green;
        if (green < 0) {
            green = -green;
        }
        blue = colorBlue - sTerrainColors[i].blue;
        distance = red + green + ABS(blue);
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearestIndex = i;
        }
    }
    return nearestIndex;
}

/* Returns a terrain code's colour as RGBA16 */
s32 GetTerrainCodeColor(s32 code) {
    s32 colorIndex = 0;

    switch (code) {
        case TERRAIN_CODE_FAIRWAY:
            colorIndex = 0;
            break;
        case TERRAIN_CODE_GREEN:
            colorIndex = 1;
            break;
        case TERRAIN_CODE_BUNKER:
            colorIndex = 2;
            break;
        case TERRAIN_CODE_ROUGH:
            colorIndex = 3;
            break;
        case TERRAIN_CODE_UNK3:
            colorIndex = 4;
            break;
        case TERRAIN_CODE_OUT_OF_BOUNDS:
            colorIndex = 5;
            break;
        case TERRAIN_CODE_CART_WAY:
            colorIndex = 6;
            break;
        case TERRAIN_CODE_TEE_GROUND:
            colorIndex = 8;
            break;
        case TERRAIN_CODE_ROCK:
            colorIndex = 9;
            break;
        case TERRAIN_CODE_DEEP_ROUGH:
            colorIndex = 10;
            break;
    }
    return (sTerrainColors[colorIndex].red << 11) | (sTerrainColors[colorIndex].green << 6) |
           (sTerrainColors[colorIndex].blue << 1) | sTerrainColors[colorIndex].alpha;
}

/* Returns the terrain code whose colour is nearest an RGBA16 colour */
s32 GetColorTerrainCode(s32 color) {
    switch (FindNearestTerrainColor(color)) {
        case 1:
            return TERRAIN_CODE_GREEN;
        case 2:
            return TERRAIN_CODE_BUNKER;
        case 3:
            return TERRAIN_CODE_ROUGH;
        case 4:
            return TERRAIN_CODE_UNK3;
        case 6:
            return TERRAIN_CODE_CART_WAY;
        case 0:
        case 7:
            return TERRAIN_CODE_FAIRWAY;
        case 8:
            return TERRAIN_CODE_TEE_GROUND;
        case 9:
            return TERRAIN_CODE_ROCK;
        case 10:
            return TERRAIN_CODE_DEEP_ROUGH;
    }
    return TERRAIN_CODE_OUT_OF_BOUNDS;
}

/*
 * Writes a surface's two percents for a club. A club not meant for the
 * surface loses the penalty from each, down to no less than 5.
 */
void GetSurfacePercentRange(SurfaceInfo *info, Club *club, s32 *range) {
    if (club->surfaceMask & GetSurfaceClubMask(TRUE, info)) {
        range[0] = info->minPercent;
        range[1] = info->maxPercent;
    } else {
        range[0] = info->minPercent - info->wrongClubPenalty;
        if (range[0] < 5) {
            range[0] = 5;
        }
        range[1] = info->maxPercent - info->wrongClubPenalty;
        if (range[1] < 5) {
            range[1] = 5;
        }
    }
}

/*
 * Mini-golf's GetSurface. It weighs the four nearest codes like
 * FindSurface but returns the first.
 */
s32 GetMiniGolfSurface(s32 x, s32 z, s32 lowersWaterLevel) {
    s8 surfaces[4];
    f32 weights[4];
    PlaneTriangle hit;
    f32 fraction;
    s32 shiftedX;
    s32 shiftedZ;
    s32 waterBias;
    s32 i;
    s32 column;
    s32 row;
    s32 stepColumn;
    s32 stepRow;
    f32 weight;
    s32 surface;

    shiftedX = x;
    shiftedZ = z;
    shiftedX -= 0x800;
    shiftedZ -= 0x400;
    if ((u32)(shiftedX - 0x3C00) > 0xF87FF || (u32)(shiftedZ - 0x3C00) > 0xF87FF) {
        return SURFACE_OUT_OF_COURSE;
    }
    if (FindHoleTriangleHit(shiftedX, -MINI_GOLF_WATER_PROBE_REACH, shiftedZ, shiftedX, MINI_GOLF_WATER_PROBE_REACH,
                      shiftedZ, &hit, &fraction)) {
        waterBias = WATER_BIAS;
        if (lowersWaterLevel) {
            waterBias = LOWERED_WATER_BIAS;
        }
        if (IsGameFlagSet(0x9F)) {
            char text[0x80];

            sprintf(text, "ratio %f,%x", fraction,
                    (s32)(WATER_PROBE_HEIGHT(fraction, MINI_GOLF_WATER_PROBE_REACH) + waterBias));
            PrintDebugText(text, 10, 11);
        }
        if (hit.unk0 == HOLE_TRIANGLE_KIND_FLAT) {
            /* Reuses i as the water height to match */
            i = WATER_PROBE_HEIGHT(fraction, MINI_GOLF_WATER_PROBE_REACH) + waterBias;
            if (i < GetTerrainHeight(x, z)) {
                return SURFACE_WATER_HAZARD;
            }
        }
    }
    column = shiftedX / 4096;
    row = shiftedZ / 2048;
    stepColumn = ((shiftedX & 0xFFF) < 0x800) ? -1 : 1;
    stepRow = ((shiftedZ & 0x7FF) < 0x400) ? -1 : 1;
    for (i = 0; i < 4; i++) {
        switch (GetTerrainCode((i & 1) ? column + stepColumn : column, (i & 2) ? row + stepRow : row)) {
            case TERRAIN_CODE_FAIRWAY:
                weight = 1.0f;
                surface = SURFACE_MINI_GOLF_FAIRWAY;
                break;
            case TERRAIN_CODE_GREEN:
                weight = 1.1f;
                surface = SURFACE_MINI_GOLF_GREEN;
                break;
            case TERRAIN_CODE_BUNKER:
                weight = 0.7f;
                surface = SURFACE_BUNKER;
                break;
            case TERRAIN_CODE_ROUGH:
                weight = 0.9f;
                surface = SURFACE_ROUGH;
                break;
            case TERRAIN_CODE_OUT_OF_BOUNDS:
                weight = 0.1f;
                surface = SURFACE_OUT_OF_BOUNDS;
                break;
            case TERRAIN_CODE_CART_WAY:
                weight = 1.0f;
                surface = SURFACE_MINI_GOLF_CART_WAY;
                break;
            case TERRAIN_CODE_TEE_GROUND:
                weight = 1.0f;
                surface = SURFACE_TEE_GROUND;
                break;
            case TERRAIN_CODE_ROCK:
                weight = 1.0f;
                surface = SURFACE_ROCK;
                break;
            case TERRAIN_CODE_GREEN_EDGE:
                weight = 1.0f;
                surface = SURFACE_GREEN_EDGE;
                break;
            case TERRAIN_CODE_SEMI_ROUGH:
                weight = 1.0f;
                surface = SURFACE_SEMI_ROUGH;
                break;
            case TERRAIN_CODE_BARE_GROUND:
                weight = 1.0f;
                surface = SURFACE_BARE_GROUND;
                break;
            case TERRAIN_CODE_UNK3:
                weight = 1.0f;
                surface = SURFACE_BARE_GROUND_UNK13;
                break;
            case TERRAIN_CODE_DEEP_ROUGH:
                weight = 0.9f;
                surface = SURFACE_DEEP_ROUGH;
                break;
            case TERRAIN_CODE_WASTE_AREA:
                weight = 0.9f;
                surface = SURFACE_WASTE_AREA;
                break;
            default:
                surface = SURFACE_OUT_OF_BOUNDS;
                weight = 0.0f;
                break;
        }
        surfaces[i] = surface;
        weights[i] = weight;
    }
    return surfaces[0];
}

/* No jal in the ROM calls this */
f32 ConvertToFloat(s32 value) {
    return value;
}

/*
 * Returns the surface at a point: water if the water is above the ground,
 * otherwise the heaviest of the four nearest codes' surfaces
 */
#ifdef NON_MATCHING
s32 FindSurface(s32 x, s32 z, s32 lowersWaterLevel) {
    s8 surfaces[4];
    f32 weights[4];
    PlaneTriangle hit;
    f32 fraction;
    s32 waterBias;
    s32 i;
    s32 column;
    s32 row;
    s32 fractionX;
    s32 fractionZ;
    s32 stepColumn;
    s32 stepRow;
    s32 distanceX;
    s32 distanceZ;
    f32 weight;
    s32 surface;
    s32 bestIndex;
    f32 bestWeight;

    if (g_IsMiniGolf) {
        return GetMiniGolfSurface(x, z, lowersWaterLevel);
    }
    if ((u32)(x - 0x3C00) > 0x3F87FF) {
        return SURFACE_OUT_OF_COURSE;
    }
    if ((u32)(z - 0x3C00) > 0x7F87FF) {
        return SURFACE_OUT_OF_COURSE;
    }
    if (FindHoleTriangleHit(x, -WATER_PROBE_REACH, z, x, WATER_PROBE_REACH, z, &hit, &fraction)) {
        char unusedText[0x80]; /* Unused, but keeps the stack frame the same size as the original */

        waterBias = WATER_BIAS;
        if (lowersWaterLevel) {
            waterBias = LOWERED_WATER_BIAS;
        }
        if ((s32)(WATER_PROBE_HEIGHT(fraction, WATER_PROBE_REACH) + waterBias) < GetTerrainHeight(x, z)) {
            return SURFACE_WATER_HAZARD;
        }
    }
    column = x / 16384;
    row = z / 16384;
    fractionX = x & 0x3FFF;
    stepColumn = (fractionX < 0x2000) ? -1 : 1;
    fractionZ = z & 0x3FFF;
    stepRow = (fractionZ < 0x2000) ? -1 : 1;
    for (i = 0; i < 4; i++) {
        switch (GetTerrainCode((i & 1) ? column + stepColumn : column, (i & 2) ? row + stepRow : row)) {
            case TERRAIN_CODE_FAIRWAY:
                weight = 1.0f;
                surface = SURFACE_FAIRWAY;
                break;
            case TERRAIN_CODE_GREEN:
                weight = 1.1f;
                surface = SURFACE_GREEN;
                break;
            case TERRAIN_CODE_BUNKER:
                weight = 0.7f;
                surface = SURFACE_BUNKER;
                break;
            case TERRAIN_CODE_ROUGH:
                weight = 0.9f;
                surface = SURFACE_ROUGH;
                break;
            case TERRAIN_CODE_OUT_OF_BOUNDS:
                weight = 0.5f;
                surface = SURFACE_OUT_OF_BOUNDS;
                break;
            case TERRAIN_CODE_CART_WAY:
                weight = 1.0f;
                surface = SURFACE_CART_WAY;
                break;
            case TERRAIN_CODE_TEE_GROUND:
                weight = 1.0f;
                surface = SURFACE_TEE_GROUND;
                break;
            case TERRAIN_CODE_ROCK:
                weight = 1.0f;
                surface = SURFACE_ROCK;
                break;
            case TERRAIN_CODE_GREEN_EDGE:
                weight = 1.0f;
                surface = SURFACE_GREEN_EDGE;
                break;
            case TERRAIN_CODE_SEMI_ROUGH:
                weight = 1.0f;
                surface = SURFACE_SEMI_ROUGH;
                break;
            case TERRAIN_CODE_BARE_GROUND:
                weight = 1.0f;
                surface = SURFACE_BARE_GROUND;
                break;
            case TERRAIN_CODE_UNK3:
                weight = 1.0f;
                surface = SURFACE_BARE_GROUND_UNK13;
                break;
            case TERRAIN_CODE_DEEP_ROUGH:
                weight = 0.9f;
                surface = SURFACE_DEEP_ROUGH;
                break;
            case TERRAIN_CODE_WASTE_AREA:
                weight = 0.9f;
                surface = SURFACE_WASTE_AREA;
                break;
            default:
                surface = SURFACE_OUT_OF_BOUNDS;
                weight = 0.0f;
                break;
        }
        surfaces[i] = surface;
        weights[i] = weight;
    }
    for (i = 1; i < 4; i++) {
        if (surfaces[i] != surfaces[0]) {
            break;
        }
    }
    if (i == 4) {
        return surface;
    }
    distanceX = ABS(0x2000 - fractionX);
    distanceZ = ABS(0x2000 - fractionZ);
    if (((stepColumn + 1) ^ (stepRow + 1)) == 0) {
        weights[0] = weights[0] * (3.7252903e-9f * ((f32)(0x4000 - distanceX) * (f32)(0x4000 - distanceZ)));
        weights[1] = weights[1] * (3.7252903e-9f * ((f32)distanceX * (f32)(0x4000 - distanceZ)));
        weights[2] = weights[2] * (6.1035156e-5f * (f32)distanceZ);
    } else if (distanceX < distanceZ) {
        surfaces[1] = surfaces[3];
        weights[0] = weights[0] * (3.7252903e-9f * ((f32)(0x4000 - distanceZ) * (f32)(0x4000 - distanceX)));
        weights[2] = weights[2] * (3.7252903e-9f * ((f32)distanceZ * (f32)(0x4000 - distanceX)));
        weights[1] = weights[3] * (f32)distanceX * 6.1035156e-5f;
    } else {
        surfaces[2] = surfaces[3];
        weights[0] = weights[0] * (3.7252903e-9f * ((f32)(0x4000 - distanceX) * (f32)(0x4000 - distanceZ)));
        weights[1] = weights[1] * (3.7252903e-9f * ((f32)distanceX * (f32)(0x4000 - distanceZ)));
        weights[2] = weights[3] * (f32)distanceZ * 6.1035156e-5f;
    }
    if (surfaces[0] == surfaces[1]) {
        weights[0] += weights[1];
        weights[1] = 0.0f;
    }
    if (surfaces[0] == surfaces[2]) {
        weights[0] += weights[2];
        weights[2] = 0.0f;
    }
    bestIndex = 0;
    if (surfaces[1] == surfaces[2]) {
        weights[1] += weights[2];
        weights[2] = 0.0f;
    }
    bestWeight = 0.0f;
    for (i = 0; i < 3; i++) {
        if (bestWeight < weights[i]) {
            bestIndex = i;
            bestWeight = weights[i];
        }
    }
    return surfaces[bestIndex];
}

#else
INCLUDE_ASM("asm/nonmatchings/terrain", FindSurface);
#endif

s32 GetSurface(s32 x, s32 z) {
    return FindSurface(x, z, FALSE);
}

SurfaceInfo *GetSurfaceInfoAt(s32 x, s32 z) {
    return GetSurfaceInfo(GetSurface(x, z));
}

/* Returns the map image's colour at a point, or 0 off the course */
u16 GetMapColor(s32 x, s32 z) {
    u32 column;
    u32 row;
    s32 color;

    if ((u32)x >= 0x400000) {
        return 0;
    }
    if ((u32)z >= 0x800000) {
        return 0;
    }
    column = x / 16384;
    row = z / 16384;
    return MAP_TEXEL(column, row) | 1;
}

/* Returns the map colour at a point, averaged with the colour of the terrain code for a surface */
s32 BlendSurfaceMapColor(s32 x, s32 z, s32 surface) {
    u16 groundColor = GetMapColor(x, z);
    s32 code = -1;
    s32 groundRed;
    s32 groundGreen;
    s32 groundBlue;
    s32 surfaceRed;
    s32 surfaceGreen;
    s32 surfaceBlue;
    u16 surfaceColor;

    switch (GetSurfaceInfo(surface)->kind) {
        case SURFACE_KIND_FAIRWAY:
        case SURFACE_KIND_FAIRWAY_FAIR_LIE:
        case SURFACE_KIND_FAIRWAY_POOR_LIE:
        case SURFACE_KIND_SEMI_ROUGH:
            code = TERRAIN_CODE_FAIRWAY;
            break;
        case SURFACE_KIND_ROUGH:
        case SURFACE_KIND_ROUGH_POOR_LIE:
            code = TERRAIN_CODE_ROUGH;
            break;
        case SURFACE_KIND_BUNKER:
        case SURFACE_KIND_BUNKER_FAIR_LIE:
        case SURFACE_KIND_BUNKER_POOR_LIE:
        case SURFACE_KIND_BUNKER_UNK9:
            code = TERRAIN_CODE_BUNKER;
            break;
        case SURFACE_KIND_DEEP_ROUGH:
            code = TERRAIN_CODE_DEEP_ROUGH;
            break;
        case SURFACE_KIND_OUT_OF_BOUNDS:
            code = TERRAIN_CODE_OUT_OF_BOUNDS;
            break;
        case SURFACE_KIND_GREEN:
        case SURFACE_KIND_GREEN_EDGE:
            code = TERRAIN_CODE_GREEN;
            break;
        case SURFACE_KIND_ROCK:
            code = TERRAIN_CODE_ROCK;
            break;
        case SURFACE_KIND_BARE_GROUND:
        case SURFACE_KIND_BARE_GROUND_UNK20:
            code = TERRAIN_CODE_BARE_GROUND;
            break;
        case SURFACE_KIND_TEE_GROUND:
            code = TERRAIN_CODE_TEE_GROUND;
            break;
        case SURFACE_KIND_CART_WAY:
            code = TERRAIN_CODE_CART_WAY;
            break;
    }
    if (code >= 0) {
        groundRed = groundColor >> 11;
        groundGreen = (groundColor >> 6) & 0x1F;
        surfaceColor = g_TerrainCodeColors[code];
        /* The original assigns the surface's blue to the ground's and never sets surfaceBlue. */
        groundBlue = (surfaceColor >> 1) & 0x1F;
        surfaceRed = surfaceColor >> 11;
        surfaceGreen = (surfaceColor >> 6) & 0x1F;
        osSyncPrintf("c1(%d,%d,%d) c2(%d,%d,%d)\n", groundRed, groundGreen, groundBlue, surfaceRed, surfaceGreen,
                     surfaceBlue);
        groundRed = (groundRed + surfaceRed) >> 1;
        groundGreen = (groundGreen + surfaceGreen) >> 1;
        groundBlue = (groundBlue + surfaceBlue) / 2;
        return (groundRed << 11) | (groundGreen << 6) | (groundBlue << 1) | 1;
    }
    return groundColor;
}

/* The height is unused */
s32 IsInWater(s32 x, s32 y, s32 z) {
    return GetSurface(x, z) == SURFACE_WATER_HAZARD;
}

/* Sets a vertex halfway between two others */
void SetVertexMidpoint(Vtx *first, Vtx *second, Vtx *midpoint) {
    midpoint->v.ob[0] = (first->v.ob[0] + second->v.ob[0]) / 2;
    midpoint->v.ob[1] = (first->v.ob[1] + second->v.ob[1]) / 2;
    midpoint->v.ob[2] = (first->v.ob[2] + second->v.ob[2]) / 2;
    midpoint->v.flag = 0;
    midpoint->v.tc[0] = (first->v.tc[0] + second->v.tc[0]) / 2;
    midpoint->v.tc[1] = (first->v.tc[1] + second->v.tc[1]) / 2;
    midpoint->v.cn[0] = (first->v.cn[0] + second->v.cn[0]) / 2;
    midpoint->v.cn[1] = (first->v.cn[1] + second->v.cn[1]) / 2;
    midpoint->v.cn[2] = (first->v.cn[2] + second->v.cn[2]) / 2;
    midpoint->v.cn[3] = (first->v.cn[3] + second->v.cn[3]) / 2;
}

/* Sets a vertex weight sixteenths of the way from one vertex to another */
void BlendVertices(Vtx *first, Vtx *second, Vtx *result, s32 weight) {
    s32 firstWeight = 16 - weight;

    result->v.ob[0] = (first->v.ob[0] * firstWeight + second->v.ob[0] * weight) / 16;
    result->v.ob[1] = (first->v.ob[1] * firstWeight + second->v.ob[1] * weight) / 16;
    result->v.ob[2] = (first->v.ob[2] * firstWeight + second->v.ob[2] * weight) / 16;
    result->v.flag = 0;
    result->v.tc[0] = (first->v.tc[0] * firstWeight + second->v.tc[0] * weight) / 16;
    result->v.tc[1] = (first->v.tc[1] * firstWeight + second->v.tc[1] * weight) / 16;
    result->v.cn[0] = (first->v.cn[0] * firstWeight + second->v.cn[0] * weight) / 16;
    result->v.cn[1] = (first->v.cn[1] * firstWeight + second->v.cn[1] * weight) / 16;
    result->v.cn[2] = (first->v.cn[2] * firstWeight + second->v.cn[2] * weight) / 16;
    result->v.cn[3] = (first->v.cn[3] * firstWeight + second->v.cn[3] * weight) / 16;
}

/* Returns the grid vertex at a vertex column and row, or the first one off the grid */
Vtx *GetGridVertex(u32 column, u32 row) {
    Vtx *vertex;

    if (column >= GROUND_GRID_VERTEX_COLUMN_COUNT || row >= GROUND_GRID_VERTEX_ROW_COUNT) {
        vertex = &g_WorkArea.hole.gridVertices[0];
    } else {
        vertex = &g_WorkArea.hole.gridVertices[row * 17 + (column >> 1) + (column & 1) * 9];
    }
    return vertex;
}

s16 GetGridVertexHeight(s32 column, s32 row) {
    return GetGridVertex(column, row)->v.ob[1];
}

void InitGridVertex(s32 column, s32 row, s32 height, u8 shade) {
    Vtx *vertex = GetGridVertex(column, row);

    vertex->v.ob[0] = column << 8;
    vertex->v.ob[2] = row << 8;
    vertex->v.ob[1] = height & ~1;
    vertex->v.flag = 0;
    vertex->v.tc[0] = (column << 10) + 0x20;
    vertex->v.tc[1] = (row << 10) + 0x20;
    if (row == 32) {
        vertex->v.tc[1] = 0x7FFF;
    }
    vertex->v.cn[0] = shade;
    vertex->v.cn[1] = shade;
    vertex->v.cn[2] = shade;
    vertex->v.cn[3] = 0xFF;
}

void SetGridVertexHeight(s32 column, s32 row, s32 height) {
    GetGridVertex(column, row)->v.ob[1] = height & ~1;
}

void SetGridVertexShade(s32 column, s32 row, s8 shade) {
    Vtx *vertex = GetGridVertex(column, row);

    vertex->v.cn[0] = shade;
    vertex->v.cn[1] = shade;
    vertex->v.cn[2] = shade;
}

void SetGridVertex(s32 column, s32 row, Vtx_t *point) {
    GetGridVertex(column, row)->v = *point;
}

/* Returns the ground point at a point column and row: a grid vertex, or a tile's midpoint */
#ifdef NON_MATCHING
Vtx *GetGroundPoint(u32 column, u32 row) {
    u32 tileRow;
    u32 tileColumn;
    s32 slotRow;
    s32 slotColumn;

    if ((column | row) & 1) {
        tileRow = row >> 2;
        if (row == 64) {
            tileRow = 15;
            slotRow = 4;
        } else {
            slotRow = row & 3;
        }
        if (column == 32) {
            tileColumn = 7;
            slotColumn = 4;
        } else {
            tileColumn = column >> 2;
            slotColumn = column & 3;
        }
        return &g_WorkArea.hole.midpoints[tileRow * 8 + tileColumn][sPointSlots[slotRow][slotColumn]];
    }
    return GetGridVertex(column >> 1, row >> 1);
}

#else
INCLUDE_ASM("asm/nonmatchings/terrain", GetGroundPoint);
#endif

/*
 * Whether a midpoint on a tile's first row, or first column, is also kept
 * by the tile before it. They are macros, as inline functions compile
 * differently from the original.
 */
#define IsSharedWithPreviousTileRow(column, row) (((column) & 1) && !((row) & 3) && ((row) / 4 > 0))
#define IsSharedWithPreviousTileColumn(column, row) (((row) & 1) && !((column) & 3) && ((column) / 4 > 0))

/*
 * Copies a point into the ground point, and into every tile that keeps a
 * copy of it. It returns no value; the s32 is there to match.
 */
s32 SetGroundPoint(s32 column, s32 row, Vtx_t *point) {
    s32 blockIndex;
    s32 tileIndex;
    Vtx *midpoint;
    Vtx *midpoints;

    if ((column | row) & 1) {
        if (row < 64 && column < 32) {
            tileIndex = (row / 4) * 8 + column / 4;
            blockIndex = g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex;
            if (blockIndex >= 0) {
                midpoints = g_WorkArea.hole.midpoints[blockIndex];
                midpoint = &midpoints[sPointSlots[row & 3][column & 3]];
                midpoint->v = *point;
            }
        }
        if (IsSharedWithPreviousTileRow(column, row)) {
            tileIndex = (row / 4 - 1) * 8 + column / 4;
            blockIndex = g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex;
            if (blockIndex >= 0) {
                midpoints = g_WorkArea.hole.midpoints[blockIndex];
                midpoint = &midpoints[sPointSlots[4][column & 3]];
                midpoint->v = *point;
            }
        }
        if (IsSharedWithPreviousTileColumn(column, row)) {
            tileIndex = (row / 4) * 8 + (column / 4 - 1);
            blockIndex = g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex;
            if (blockIndex >= 0) {
                midpoints = g_WorkArea.hole.midpoints[blockIndex];
                midpoint = &midpoints[sPointSlots[row & 3][4]];
                midpoint->v = *point;
            }
        }
    } else {
        SetGridVertex(column / 2, row / 2, point);
    }
}

/* SetGroundPoint for just the height and shade. It returns no value; the s32 is there to match. */
s32 SetGroundPointHeightAndShade(s32 column, s32 row, s32 height, s32 shade) {
    s32 blockIndex;
    s32 tileIndex;
    Vtx *point;
    Vtx *midpoints;

    if ((column | row) & 1) {
        if (row < 64 && column < 32) {
            tileIndex = (row / 4) * 8 + column / 4;
            blockIndex = g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex;
            if (blockIndex >= 0) {
                midpoints = g_WorkArea.hole.midpoints[blockIndex];
                point = &midpoints[sPointSlots[row & 3][column & 3]];
                point->v.ob[1] = height;
                point->v.cn[0] = point->v.cn[1] = point->v.cn[2] = shade;
            }
        }
        if (IsSharedWithPreviousTileRow(column, row)) {
            tileIndex = (row / 4 - 1) * 8 + column / 4;
            blockIndex = g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex;
            if (blockIndex >= 0) {
                midpoints = g_WorkArea.hole.midpoints[blockIndex];
                point = &midpoints[sPointSlots[4][column & 3]];
                point->v.ob[1] = height;
                point->v.cn[0] = point->v.cn[1] = point->v.cn[2] = shade;
            }
        }
        if (IsSharedWithPreviousTileColumn(column, row)) {
            tileIndex = (row / 4) * 8 + (column / 4 - 1);
            blockIndex = g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex;
            if (blockIndex >= 0) {
                midpoints = g_WorkArea.hole.midpoints[blockIndex];
                point = &midpoints[sPointSlots[row & 3][4]];
                point->v.ob[1] = height;
                point->v.cn[0] = point->v.cn[1] = point->v.cn[2] = shade;
            }
        }
    } else {
        SetGridVertexHeight(column / 2, row / 2, height);
    }
}

void InitTerrainTextures(void) {
    GroundTile *tile;
    s32 tileIndex;
    s32 texelRow;
    s32 texelColumn;
    s32 row;
    s32 column;

    for (tileIndex = 0; tileIndex < GROUND_TILE_COUNT; tileIndex++) {
        tile = &g_WorkArea.hole.tiles[tileIndex];
        D_800BAA0C[tileIndex] = 1.0f;
        tile->unk994 = 1;
        tile->midpointBlockIndex = -1;
        tile->unk990 = 0;
        for (texelRow = 0; texelRow < 32; texelRow++) {
            for (texelColumn = 0; texelColumn < 32; texelColumn++) {
                tile->texels[0][texelRow * 32 + texelColumn] = 0x7C1;
            }
        }
    }
    for (row = 0; row < 33; row++) {
        for (column = 0; column < 17; column++) {
            InitGridVertex(column, row, 0, 0x80);
        }
    }
    BuildAllTileMidpoints();
}

/* A tile's three by three grid vertices, by row and column */
#define CORNER(row, column) corners[(row) * 3 + (column)]

/* Sets a tile's midpoints between its grid vertices */
void BuildTileMidpoints(Vtx *midpoints, s32 tileColumn, s32 tileRow) {
    Vtx *corners[9];
    s32 cornerIndex = 0;
    s32 row;
    s32 column;

    for (row = 0; row < 3; row++) {
        for (column = 0; column < 3; column++) {
            corners[cornerIndex++] = GetGridVertex(tileColumn * 2 + column, tileRow * 2 + row);
        }
    }
    SetVertexMidpoint(CORNER(0, 0), CORNER(0, 1), &midpoints[0]);
    SetVertexMidpoint(CORNER(0, 1), CORNER(0, 2), &midpoints[1]);
    SetVertexMidpoint(CORNER(0, 0), CORNER(1, 0), &midpoints[2]);
    SetVertexMidpoint(CORNER(0, 1), CORNER(1, 1), &midpoints[9]);
    SetVertexMidpoint(CORNER(0, 2), CORNER(1, 2), &midpoints[3]);
    SetVertexMidpoint(CORNER(1, 0), CORNER(1, 1), &midpoints[11]);
    SetVertexMidpoint(CORNER(1, 1), CORNER(1, 2), &midpoints[12]);
    SetVertexMidpoint(CORNER(1, 0), CORNER(2, 0), &midpoints[4]);
    SetVertexMidpoint(CORNER(1, 1), CORNER(2, 1), &midpoints[14]);
    SetVertexMidpoint(CORNER(1, 2), CORNER(2, 2), &midpoints[5]);
    SetVertexMidpoint(CORNER(2, 0), CORNER(2, 1), &midpoints[6]);
    SetVertexMidpoint(CORNER(2, 1), CORNER(2, 2), &midpoints[7]);
    SetVertexMidpoint(CORNER(0, 1), CORNER(1, 0), &midpoints[8]);
    SetVertexMidpoint(CORNER(0, 2), CORNER(1, 1), &midpoints[10]);
    SetVertexMidpoint(CORNER(1, 1), CORNER(2, 0), &midpoints[13]);
    SetVertexMidpoint(CORNER(1, 2), CORNER(2, 1), &midpoints[15]);
}

/* Gives every tile its own midpoints and centre, and builds them */
void BuildAllTileMidpoints(void) {
    s32 tileIndex = 0;
    s32 tileRow;
    s32 tileColumn;
    Vtx *midpoints;
    Vtx *center;

    for (tileRow = 0; tileRow < 16; tileRow++) {
        for (tileColumn = 0; tileColumn < 8; tileColumn++) {
            g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex = tileIndex;
            midpoints = g_WorkArea.hole.midpoints[tileIndex];
            center = GetGridVertex(tileColumn * 2 + 1, tileRow * 2 + 1);
            g_WorkArea.hole.tiles[tileIndex].center[0] = center->v.ob[0];
            g_WorkArea.hole.tiles[tileIndex].center[1] = center->v.ob[1];
            g_WorkArea.hole.tiles[tileIndex].center[2] = center->v.ob[2];
            if (g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex >= 0) {
                BuildTileMidpoints(midpoints, tileColumn, tileRow);
            }
            tileIndex++;
        }
    }
}

/*
 * Shows the build dates and a page of game flags, which the controller
 * moves through and sets or clears
 */
void UpdateFlagPanel(void) {
    char text[0x10];
    s32 row;
    s32 column;
    s32 textColumn;

    s32 GetSelectedFlag(void) {
        return (sFlagPanelFirstRow + sFlagPanelRow) * FLAG_PANEL_COLUMN_COUNT + sFlagPanelColumn;
    }

    ClearDebugText();
    PrintDebugText("ROM :", 3, 4);
    PrintDebugText(g_RomBuildDate, 8, 4);
    PrintDebugText("SRAM:", 3, 5);
    PrintDebugText(g_SaveBuildDate, 8, 5);
    for (row = 0; row < FLAG_PANEL_ROW_COUNT; row++) {
        sprintf(text, "%x0|", row + sFlagPanelFirstRow);
        PrintDebugText(text, 3, row + 14);
        for (column = 0, textColumn = 6; column < FLAG_PANEL_COLUMN_COUNT; column++, textColumn += 2) {
            if (row == 0) {
                sprintf(text, "%X", column);
                PrintDebugText(text, textColumn, 12);
            }
            sprintf(text, "%X", IsGameFlagSet((row + sFlagPanelFirstRow) * FLAG_PANEL_COLUMN_COUNT + column));
            if (row == sFlagPanelRow && column == sFlagPanelColumn) {
                SetDebugTextHighlight(TRUE);
            }
            PrintDebugText(text, textColumn, row + 14);
        }
    }
    if (g_MenuControllerData[0].button & R_JPAD) {
        sFlagPanelColumn++;
    }
    if (g_MenuControllerData[0].button & L_JPAD) {
        sFlagPanelColumn--;
    }
    if (sFlagPanelColumn < 0) {
        sFlagPanelColumn = FLAG_PANEL_COLUMN_COUNT - 1;
        sFlagPanelRow--;
    }
    if (sFlagPanelColumn >= FLAG_PANEL_COLUMN_COUNT) {
        sFlagPanelColumn = 0;
        sFlagPanelRow++;
    }
    if (g_MenuControllerData[0].button & U_JPAD) {
        sFlagPanelRow--;
    }
    if (g_MenuControllerData[0].button & D_JPAD) {
        sFlagPanelRow++;
    }
    if (g_MenuControllerData[0].button & R_TRIG) {
        sFlagPanelFirstRow += FLAG_PANEL_ROW_COUNT;
    }
    if (g_MenuControllerData[0].button & L_TRIG) {
        sFlagPanelFirstRow -= FLAG_PANEL_ROW_COUNT;
    }
    while (TRUE) {
        if (sFlagPanelRow < 0) {
            sFlagPanelRow++;
            sFlagPanelFirstRow--;
        } else if (sFlagPanelRow >= FLAG_PANEL_ROW_COUNT) {
            sFlagPanelRow--;
            sFlagPanelFirstRow++;
        } else {
            break;
        }
    }
    if (sFlagPanelFirstRow > FLAG_PANEL_FIRST_ROW_MAX) {
        sFlagPanelFirstRow = FLAG_PANEL_FIRST_ROW_MAX;
    }
    if (sFlagPanelFirstRow < 0) {
        sFlagPanelFirstRow = 0;
    }
    if (g_MenuControllerData[0].button & A_BUTTON) {
        SetGameFlag(GetSelectedFlag());
    }
    if (g_MenuControllerData[0].button & B_BUTTON) {
        ClearGameFlag(GetSelectedFlag());
    }
}

/* Returns the debug scene menu's label for a scene */
const char *GetDebugSceneLabel(u32 scene) {
    switch (scene) {
        case SCENE_FLAG_PANEL:
            return "FLAGS ";
        case SCENE_MAP_OVERVIEW:
            return "ABOUT ";
        case SCENE_GROUND_CAMERA:
            return " VIEW ";
        case SCENE_PLAY_HOLE:
            return " PLAY ";
        case SCENE_HEIGHTS:
            return "HEIGHT";
        case SCENE_TREES:
            return " TREE ";
        case SCENE_PIN:
            return " PIN  ";
        case SCENE_UNK9:
            return "DIVINF";
        case SCENE_TRIANGLES:
            return "TRIANG";
        case SCENE_MENU:
            return "MENU";
        case SCENE_UNK10:
            return "XZ-DIV";
        case SCENE_UNK11:
            return " FILE ";
        case SCENE_PLAY_UNK12:
            return "STROKE";
        case SCENE_PLAY_UNK13:
            return "S-SHOT";
        case SCENE_RINGS:
            return "RING  ";
        case SCENE_WALLS:
            return "WALL  ";
        default:
            return "      ";
    }
}

/*
 * Shows the debug scene menu and enters the chosen scene. It returns no
 * value; the s32 is there to match
 */
s32 UpdateDebugSceneMenu(void) {
    u32 selectedIndex;
    u32 i;
    s32 row;

    g_CurrentShot.surface = 0;
    ClearGameFlag(GAME_FLAG_HOLE_LOADED);
    D_800C30D1 = 0;
    if (g_MenuControllerData[0].button & START_BUTTON) {
        ClearDebugText();
        sIsSceneMenuOpen = FALSE;
        sSceneMenuBox.state = BOX_CLOSING;
        sSceneMenuBox.frameIndex = 0;
        PlaySound(0x5F, 15, 127);
        return;
    }
    for (selectedIndex = 0; selectedIndex < SCENE_MENU_LENGTH; selectedIndex++) {
        if (sSceneMenuScenes[selectedIndex] == sSceneMenuSelection) {
            break;
        }
    }
    for (i = 0, row = SCENE_MENU_TOP_ROW; i < SCENE_MENU_LENGTH; i++, row++) {
        if (i == selectedIndex) {
            SetDebugTextHighlight(TRUE);
            PrintDebugText("->", 10, selectedIndex + SCENE_MENU_TOP_ROW);
        } else {
            PrintDebugText("  ", 10, row);
        }
        PrintDebugText(GetDebugSceneLabel(sSceneMenuScenes[i]), 12, row);
    }
    if (g_MenuControllerData[0].button & U_JPAD) {
        selectedIndex = (selectedIndex + SCENE_MENU_LENGTH - 1) % SCENE_MENU_LENGTH;
        sSceneMenuSelection = sSceneMenuScenes[selectedIndex];
        PlaySound(0x5C, 15, 127);
    }
    if (g_MenuControllerData[0].button & D_JPAD) {
        selectedIndex = (selectedIndex + SCENE_MENU_LENGTH + 1) % SCENE_MENU_LENGTH;
        sSceneMenuSelection = sSceneMenuScenes[selectedIndex];
        PlaySound(0x5C, 15, 127);
    }
    if (g_MenuControllerData[0].button & A_BUTTON) {
        ClearDebugText();
        sIsSceneMenuOpen = FALSE;
        sSceneMenuBox.state = BOX_CLOSING;
        sSceneMenuBox.frameIndex = 0;
        g_Scene = sSceneMenuSelection;
        PlaySound(0x5D, 15, 127);
        if (sSceneMenuSelection == SCENE_MENU) {
            func_8005D9A0();
        } else {
            PlaySong(1, 0);
        }
        switch ((u32)g_Scene) {
            case SCENE_PLAY_HOLE:
                func_8005F838(0);
                ChooseConditionsAndInitHole();
                func_800802E0();
                break;
            case SCENE_UNK9:
                break;
            case SCENE_PLAY_UNK12:
                func_8005F838(1);
                func_800802E0();
                break;
            case SCENE_PLAY_UNK13:
                func_8005F838(2);
                func_800802E0();
                break;
        }
    }
}

/* Sets each hole triangle's bounds and texture coordinates, then sets up the rings */
void PrepareHoleTriangles(void) {
    Vtx_t *vertex;
    s32 triangleIndex;
    s32 corner;
    s32 lowest[3];
    s32 highest[3];

    for (triangleIndex = 0; triangleIndex < HOLE_TRIANGLE_COUNT; triangleIndex++) {
        lowest[0] = lowest[1] = lowest[2] = 0x20000000;
        highest[0] = highest[1] = highest[2] = -0x20000000;
        for (corner = 0; corner < 3; corner++) {
            vertex = &g_WorkArea.hole.triangles[triangleIndex].vertices[corner].v;
            if (vertex->ob[0] < lowest[0]) {
                lowest[0] = vertex->ob[0];
            }
            if (vertex->ob[0] > highest[0]) {
                highest[0] = vertex->ob[0];
            }
            if (vertex->ob[1] < lowest[1]) {
                lowest[1] = vertex->ob[1];
            }
            if (vertex->ob[1] > highest[1]) {
                highest[1] = vertex->ob[1];
            }
            if (vertex->ob[2] < lowest[2]) {
                lowest[2] = vertex->ob[2];
            }
            if (vertex->ob[2] > highest[2]) {
                highest[2] = vertex->ob[2];
            }
            vertex->tc[0] = vertex->ob[0];
            vertex->tc[1] = vertex->ob[2];
        }
        g_WorkArea.hole.triangles[triangleIndex].bounds[0][0] = lowest[0];
        g_WorkArea.hole.triangles[triangleIndex].bounds[1][0] = lowest[1];
        g_WorkArea.hole.triangles[triangleIndex].bounds[2][0] = lowest[2];
        g_WorkArea.hole.triangles[triangleIndex].bounds[0][1] = highest[0];
        g_WorkArea.hole.triangles[triangleIndex].bounds[1][1] = highest[1];
        g_WorkArea.hole.triangles[triangleIndex].bounds[2][1] = highest[2];
    }
    InitRings();
}

/*
 * Tests whether func_802504E8 was linked above the first 4 MB of RAM. It
 * is not, so this returns FALSE and the debug scene menu never opens.
 */
s32 IsOverlayInExpansionMemory(void) {
    return (u32)func_802504E8 > 0x803FFFFF;
}

/* Marks each billboard on the map image in red */
void MarkBillboardsOnMap(void) {
    s32 i;
    u16 color = 0xF801;
    u32 column;
    u32 row;

    for (i = 0; i < BILLBOARD_COUNT; i++) {
        if (g_Billboards[i].textureIndex >= 0) {
            column = g_Billboards[i].x / 16;
            row = g_Billboards[i].z / 16;
            MAP_TEXEL(column, row) = color;
        }
    }
}

/*
 * Whether, with debugging on, the first controller asks for the scene
 * menu. It is a macro, as an inline function compiles differently from
 * the original.
 */
#define IsSceneMenuRequested()                                                                                        \
    (g_IsDebugEnabled && (g_MenuControllerData[0].button & START_BUTTON) &&                                           \
     ((g_ControllerData[0].button & (START_BUTTON | L_TRIG)) == (START_BUTTON | L_TRIG)))

/* Runs the debug scene menu and the update routine of the current debug scene */
void UpdateDebugScenes(void) {
    s32 openState = BOX_OPEN; /* A local, rather than the constant, to match */
    s32 stickStep;

    if (sIsFirstDebugUpdate) {
        sIsFirstDebugUpdate = FALSE;
        if (g_ControllerData[0].button & L_TRIG) {
            g_IsDebugEnabled = FALSE;
        }
        g_Scene = SCENE_BOOT_LOGOS;
        return;
    }
    if (IsGameFlagSet(0x1A)) {
        PrintDebugText(GetSurfaceInfoAt(g_CurrentShot.unk44[AXIS_X], g_CurrentShot.unk44[AXIS_Z])->name, 2, 4);
    }
    if (sIsSceneMenuOpen && sSceneMenuBox.state == openState) {
        UpdateDebugSceneMenu();
        QueueAnimatedBox(&sSceneMenuBox);
        return;
    }
    if (IsSceneMenuRequested()) {
        if (IsOverlayInExpansionMemory()) {
            sIsSceneMenuOpen = TRUE;
            ClearGameFlag(GAME_FLAG_HIDE_DEBUG_TEXT);
            FreeAllCharacterSlots();
            sSceneMenuBox.state = BOX_OPENING;
            sSceneMenuBox.frameIndex = 0;
            QueueAnimatedBox(&sSceneMenuBox);
            PlaySound(0x66, 15, 127);
            MarkBillboardsOnMap();
            return;
        }
        if (g_Scene == SCENE_FLAG_PANEL) {
            ClearDebugText();
            g_Scene = sSceneBeforeFlags;
            InitClubs();
            if (IsGameFlagSet(0x9E)) {
                nuContRmbMgrRemove();
            }
        } else {
            sSceneBeforeFlags = g_Scene;
            g_Scene = SCENE_FLAG_PANEL;
        }
        return;
    }
    QueueAnimatedBox(&sSceneMenuBox);
    if ((g_Scene != SCENE_PLAY_UNK12) & (g_Scene != SCENE_MENU)) {
        SetDebugTextHighlight(TRUE);
        PrintDebugText(GetDebugSceneLabel(g_Scene), 2, 3);
    }
    if (g_Scene == SCENE_UNK11) {
        func_80406818();
    }
    if (g_Scene == SCENE_FLAG_PANEL) {
        UpdateFlagPanel();
    }
    if (g_Scene == SCENE_UNK9) {
        func_8040000C();
    } else if (g_Scene == SCENE_UNK10) {
        func_804002C0();
    }
    if (g_Scene == SCENE_TREES) {
        func_80406C3C();
    }
    if (g_Scene == SCENE_TRIANGLES) {
        func_80402428();
    }
    if (g_Scene == SCENE_RINGS) {
        func_80402E1C();
    }
    if (g_Scene == SCENE_WALLS) {
        func_80403A04();
    }
    if (g_Scene == SCENE_MAP_OVERVIEW) {
        if (g_MenuControllerData[0].button & R_JPAD) {
            sMapOverviewControls.cameraZ -= MAP_OVERVIEW_CAMERA_STEP;
        }
        if (g_MenuControllerData[0].button & L_JPAD) {
            sMapOverviewControls.cameraZ += MAP_OVERVIEW_CAMERA_STEP;
        }
        if (g_MenuControllerData[0].button & D_JPAD) {
            sMapOverviewControls.cameraX -= MAP_OVERVIEW_CAMERA_STEP;
        }
        if (g_MenuControllerData[0].button & U_JPAD) {
            sMapOverviewControls.cameraX += MAP_OVERVIEW_CAMERA_STEP;
        }
        stickStep = g_MenuControllerData[0].stick_y / STICK_STEP_SIZE;
        if (stickStep != 0) {
            sMapOverviewControls.cameraX += stickStep;
        }
        stickStep = g_MenuControllerData[0].stick_x / STICK_STEP_SIZE;
        if (stickStep != 0) {
            sMapOverviewControls.cameraZ -= stickStep;
        }
        if (g_MenuControllerData[0].button & L_TRIG) {
            sMapOverviewControls.adjustment -= MAP_OVERVIEW_ADJUSTMENT_STEP;
            if (sMapOverviewControls.adjustment < MAP_OVERVIEW_ADJUSTMENT_MIN) {
                sMapOverviewControls.adjustment = MAP_OVERVIEW_ADJUSTMENT_MIN;
            }
        }
        if (g_MenuControllerData[0].button & R_TRIG) {
            sMapOverviewControls.adjustment += MAP_OVERVIEW_ADJUSTMENT_STEP;
            if (sMapOverviewControls.adjustment > MAP_OVERVIEW_ADJUSTMENT_MAX) {
                sMapOverviewControls.adjustment = MAP_OVERVIEW_ADJUSTMENT_MAX;
            }
        }
        if (g_MenuControllerData[0].button & Z_TRIG) {
            sMapOverviewControls.isToggled ^= TRUE;
        }
        if (g_MenuControllerData[0].button & A_BUTTON) {
            sMapOverviewControls.selectionIndex++;
            if (sMapOverviewControls.selectionIndex >= MAP_OVERVIEW_SELECTION_COUNT) {
                sMapOverviewControls.selectionIndex = 0;
            }
            sMapOverviewControls.isToggled = FALSE;
        }
        if (g_MenuControllerData[0].button & B_BUTTON) {
            sMapOverviewControls.selectionIndex--;
            if (sMapOverviewControls.selectionIndex < 0) {
                sMapOverviewControls.selectionIndex = MAP_OVERVIEW_SELECTION_COUNT - 1;
            }
            sMapOverviewControls.isToggled = FALSE;
        }
        if (g_MenuControllerData[0].trigger & D_CBUTTONS) {
            func_8024F7B0();
        }
        if (g_MenuControllerData[0].trigger & R_CBUTTONS) {
            func_8024FF14();
        }
        if (g_MenuControllerData[0].trigger & U_CBUTTONS) {
            g_LightPitch = 0x3000;
            g_LightYaw = 0x2000;
            func_80253234(1);
        }
        if (g_MenuControllerData[0].trigger & L_CBUTTONS) {
            func_80253270();
        }
        g_MapCamera.view.eye[0] = sMapOverviewControls.cameraX * MAP_OVERVIEW_CAMERA_SCALE;
        g_MapCamera.view.eye[2] = sMapOverviewControls.cameraZ * MAP_OVERVIEW_CAMERA_SCALE;
    }
    if (g_Scene == SCENE_HEIGHTS) {
        func_80404B84();
    }
    if (g_Scene == SCENE_PIN) {
        func_80400FBC();
    }
}

/*
 * Returns the point in a tile's slot: a grid vertex when the slot has bit
 * 0x80, otherwise a midpoint
 */
Vtx *GetTileCellPoint(Vtx *midpoints, s32 tileColumn, s32 tileRow, s32 slot) {
    Vtx *vertex;

    if (slot & 0x80) {
        vertex = GetGridVertex(tileColumn * 2 + ((slot >> 2) & 3), tileRow * 2 + (slot & 3));
    } else {
        vertex = &midpoints[slot];
    }
    return vertex;
}

/* Returns the cross product of two points' offsets from an origin */
s32 CrossProductXZ(s32 originX, s32 originZ, s32 firstX, s32 firstZ, s32 secondX, s32 secondZ) {
    return (firstX - originX) * (secondZ - originZ) - (firstZ - originZ) * (secondX - originX);
}

/*
 * Finds the ground triangle under a point and copies its corners. Returns
 * 0, or -1 off the ground.
 */
#ifdef NON_MATCHING
s32 FindGroundTriangle(s32 x, s32 z, PlaneTriangle *triangle) {
    s32 baseColumn;
    s32 baseRow;
    s32 slotColumn;
    s32 slotRow;
    s32 rowIndex;
    s32 columnIndex;
    s32 column;
    s32 row;

    /* Whether x and z are on the outer side of a triangle's edge from start to end */
    inline s32 IsOutsideEdge(Vtx *start, Vtx *end) {
        return (end->v.ob[0] - start->v.ob[0]) * (z - (start->v.ob[2] << 10)) -
                   (end->v.ob[2] - start->v.ob[2]) * (x - (start->v.ob[0] << 10)) < 0;
    }

    inline s32 ContainsPoint(Vtx *first, Vtx *second, Vtx *third) {
        if (IsOutsideEdge(first, second) || IsOutsideEdge(second, third) || IsOutsideEdge(third, first)) {
            return FALSE;
        }
        triangle->corners[AXIS_X][0] = first->v.ob[0];
        triangle->corners[AXIS_X][1] = second->v.ob[0];
        triangle->corners[AXIS_X][2] = third->v.ob[0];
        triangle->corners[AXIS_Y][0] = first->v.ob[1];
        triangle->corners[AXIS_Y][1] = second->v.ob[1];
        triangle->corners[AXIS_Y][2] = third->v.ob[1];
        triangle->corners[AXIS_Z][0] = first->v.ob[2];
        triangle->corners[AXIS_Z][1] = second->v.ob[2];
        triangle->corners[AXIS_Z][2] = third->v.ob[2];
        return TRUE;
    }

    if (g_IsMiniGolf) {
        if ((u32)x > 0xFFFFF) {
            return -1;
        }
        if ((u32)z > 0xFFFFF) {
            return -1;
        }
    } else if ((u32)x > 0x3FFFFF || (u32)z > 0x7FFFFF) {
        return -1;
    }
    if (g_IsMiniGolf) {
        baseColumn = (x >> 17) * 4;
        baseRow = (z >> 16) * 4;
        slotColumn = (x >> 15) & 3;
        slotRow = (z >> 14) & 3;
    } else {
        baseColumn = (x >> GROUND_TILE_SHIFT) * GROUND_TILE_POINT_SPAN;
        baseRow = (z >> GROUND_TILE_SHIFT) * GROUND_TILE_POINT_SPAN;
        slotColumn = (x >> 17) & 3;
        slotRow = (z >> 17) & 3;
    }
    for (rowIndex = 0; rowIndex < 4; rowIndex++) {
        Vtx *corners[4];

        row = baseRow + ((slotRow + sCellSearchOffsets[rowIndex]) & 3);
        for (columnIndex = 0; columnIndex < 4; columnIndex++) {
            column = baseColumn + ((slotColumn + sCellSearchOffsets[columnIndex]) & 3);
            corners[0] = GetGroundPoint(column, row);
            corners[1] = GetGroundPoint(column + 1, row);
            corners[2] = GetGroundPoint(column, row + 1);
            if (ContainsPoint(corners[0], corners[1], corners[2])) {
                return 0;
            }
            corners[3] = GetGroundPoint(column + 1, row + 1);
            if (ContainsPoint(corners[1], corners[3], corners[2])) {
                return 0;
            }
        }
    }
    PrintDebugText("GetHeightInfo", 10, 11);
    return -1;
}

#else
INCLUDE_ASM("asm/nonmatchings/terrain", FindGroundTriangle);
#endif

s32 AreTrianglesDifferent(PlaneTriangle *first, PlaneTriangle *second) {
    s32 i;

    for (i = 0; i < TRIANGLE_CORNER_COUNT; i++) {
        if (first->corners[AXIS_X][i] != second->corners[AXIS_X][i] ||
            first->corners[AXIS_Y][i] != second->corners[AXIS_Y][i] ||
            first->corners[AXIS_Z][i] != second->corners[AXIS_Z][i]) {
            return TRUE;
        }
    }
    return FALSE;
}

/* The plane through a triangle in fixed point, with a normal of length 256 */
typedef struct IntegerPlane {
    /* 0x00 */ s32 normal[AXIS_COUNT];
    /* 0x0C */ s32 distance;
    /* 0x10 */ s32 unk10; /* Always 256 */
} IntegerPlane; /* size = 0x14 */

/*
 * Whether any of a normal's parts is longer than an integer plane's
 * normals may be. It is a macro, as an inline function compiles
 * differently from the original.
 */
#define IsNormalTooLong(normalX, normalY, normalZ)                                                                    \
    ((ABS(normalX) > 0x4000) || (ABS(normalY) > 0x4000) || (ABS(normalZ) > 0x4000))

/* Integer GetTrianglePlane. Returns 0, or -1 for a degenerate triangle. */

s32 GetTriangleIntegerPlane(PlaneTriangle *triangle, IntegerPlane *plane) {
    s32 firstX;
    s32 secondX;
    s32 firstY;
    s32 secondY;
    s32 firstZ;
    s32 secondZ;
    s32 normalX;
    s32 normalY;
    s32 normalZ;
    s32 distance;
    s32 length;

    secondZ = triangle->corners[AXIS_Z][2] - triangle->corners[AXIS_Z][0];
    firstY = triangle->corners[AXIS_Y][1]; /* Split from the subtraction to match */
    firstY -= triangle->corners[AXIS_Y][0];
    secondY = triangle->corners[AXIS_Y][2] - triangle->corners[AXIS_Y][0];
    firstZ = triangle->corners[AXIS_Z][1] - triangle->corners[AXIS_Z][0];
    firstX = triangle->corners[AXIS_X][1] - triangle->corners[AXIS_X][0];
    secondX = triangle->corners[AXIS_X][2] - triangle->corners[AXIS_X][0];
    normalX = firstY * secondZ - secondY * firstZ;
    normalY = -firstX * secondZ + secondX * firstZ;
    normalZ = -firstY * secondX + secondY * firstX;
    distance = -(normalX * triangle->corners[AXIS_X][0] + normalY * triangle->corners[AXIS_Y][0] +
                 normalZ * triangle->corners[AXIS_Z][0]);
    while (IsNormalTooLong(normalX, normalY, normalZ)) {
        normalX /= 256;
        normalY /= 256;
        normalZ /= 256;
        distance /= 256;
    }
    length = sqrt(normalX * normalX + normalY * normalY + normalZ * normalZ);
    if (length == 0) {
        PrintDebugText("NormalError", 3, 15);
        plane->normal[AXIS_X] = 0;
        plane->normal[AXIS_Y] = 256;
        plane->normal[AXIS_Z] = 0;
        plane->distance = 0;
        plane->unk10 = 256;
        return -1;
    }
    if (normalY < 0) {
        length = -length;
    }
    plane->unk10 = 256;
    plane->normal[AXIS_X] = (normalX << 8) / length;
    plane->normal[AXIS_Y] = (normalY << 8) / length;
    plane->normal[AXIS_Z] = (normalZ << 8) / length;
    plane->distance = (distance << 8) / length;
    return 0;
}

s32 GetTrianglePlane(PlaneTriangle *triangle, TrianglePlane *plane) {
    s32 firstX;
    s32 secondX;
    s32 firstY;
    s32 secondY;
    s32 firstZ;
    s32 secondZ;
    f32 normalX;
    f32 normalY;
    f32 normalZ;
    f32 length;
    f32 distance;
    f32 scale;

    secondZ = triangle->corners[AXIS_Z][2] - triangle->corners[AXIS_Z][0];
    firstY = triangle->corners[AXIS_Y][1]; /* Split from the subtraction to match */
    firstY -= triangle->corners[AXIS_Y][0];
    secondY = triangle->corners[AXIS_Y][2] - triangle->corners[AXIS_Y][0];
    firstZ = triangle->corners[AXIS_Z][1] - triangle->corners[AXIS_Z][0];
    firstX = triangle->corners[AXIS_X][1] - triangle->corners[AXIS_X][0];
    secondX = triangle->corners[AXIS_X][2] - triangle->corners[AXIS_X][0];
    normalX = firstY * secondZ - secondY * firstZ;
    normalY = -firstX * secondZ + secondX * firstZ;
    normalZ = -firstY * secondX + secondY * firstX;
    length = sqrtf(normalX * normalX + normalY * normalY + normalZ * normalZ);
    distance = -(normalX * triangle->corners[AXIS_X][0] + normalY * triangle->corners[AXIS_Y][0] +
                 normalZ * triangle->corners[AXIS_Z][0]);
    if (length == 0.0f) {
        if (g_IsDebugEnabled) {
            PrintDebugText("NormalFError", 3, 15);
        }
        plane->normal[AXIS_X] = 0.0f;
        plane->normal[AXIS_Z] = 0.0f;
        plane->distance = 0.0f;
        plane->normal[AXIS_Y] = 1.0f;
        plane->unk10 = 1.0f;
        return -1;
    }
    if (normalY < 0.0f) {
        length = -length;
    }
    scale = 1.0f / length;
    plane->unk10 = 1.0f;
    plane->normal[AXIS_X] = normalX * scale;
    plane->normal[AXIS_Y] = normalY * scale;
    plane->normal[AXIS_Z] = normalZ * scale;
    plane->distance = distance * scale;
    return 0;
}

/* Returns the ground's height at a point, or -300 off the ground */
#ifdef NON_MATCHING
s32 GetTerrainHeight(s32 x, s32 z) {
    PlaneTriangle triangle;
    s32 firstX;
    s32 secondX;
    s32 firstY;
    s32 secondY;
    s32 firstZ;
    s32 secondZ;
    f32 normalX;
    f32 normalY;
    f32 normalZ;
    s32 offsetX;
    s32 offsetZ;
    s32 height;

    if (g_IsMiniGolf) {
        if ((u32)x > 0xFFFFF) {
            if (x < 0) {
                x = 0;
            } else {
                x = 0xFFFFF;
            }
        }
        if ((u32)z > 0xFFFFF) {
            if (z < 0) {
                z = 0;
            } else {
                z = 0xFFFFF;
            }
        }
    } else {
        if ((u32)x > 0x3FFFFF) {
            if (x < 0) {
                x = 0;
            } else {
                x = 0x3FFFFF;
            }
        }
        if ((u32)z > 0x7FFFFF) {
            if (z < 0) {
                z = 0;
            } else {
                z = 0x7FFFFF;
            }
        }
    }
    if (FindGroundTriangle(x, z, &triangle) < 0) {
        return -300;
    }
    firstY = triangle.corners[AXIS_Y][1] - triangle.corners[AXIS_Y][0];
    secondZ = triangle.corners[AXIS_Z][2] - triangle.corners[AXIS_Z][0];
    secondY = triangle.corners[AXIS_Y][2] - triangle.corners[AXIS_Y][0];
    firstZ = triangle.corners[AXIS_Z][1] - triangle.corners[AXIS_Z][0];
    firstX = triangle.corners[AXIS_X][1] - triangle.corners[AXIS_X][0];
    secondX = triangle.corners[AXIS_X][2] - triangle.corners[AXIS_X][0];
    normalX = firstY * secondZ + secondY * -firstZ;
    normalY = firstX * -secondZ + secondX * firstZ;
    normalZ = firstY * -secondX + secondY * firstX;
    height = triangle.corners[AXIS_Y][0] << 10;
    offsetX = x - (triangle.corners[AXIS_X][0] << 10);
    offsetZ = z - (triangle.corners[AXIS_Z][0] << 10);
    if ((s32)normalY != 0) {
        return height + (s32)((-normalX * offsetX - normalZ * offsetZ) / normalY);
    }
    return 0;
}

#else
INCLUDE_ASM("asm/nonmatchings/terrain", GetTerrainHeight);
#endif

s32 GetGroundHeight(s32 x, s32 z) {
    return GetTerrainHeight(x, z);
}

/* Returns the lower of the ground's height and the water's */
s32 GetGroundOrWaterHeight(s32 x, s32 z) {
    PlaneTriangle hit;
    f32 fraction;
    s32 height = GetTerrainHeight(x, z);
    s32 waterHeight;

    if (FindHoleTriangleHit(x, -WATER_PROBE_REACH, z, x, WATER_PROBE_REACH, z, &hit, &fraction)) {
        char unusedText[0x80]; /* Unused, but keeps the stack frame the same size as the original */

        waterHeight = WATER_PROBE_HEIGHT(fraction, WATER_PROBE_REACH);
        if (waterHeight < height) {
            height = waterHeight;
        }
    }
    return height;
}


/* The original ends its rodata with 8 zero bytes that nothing in the ROM references */
static const char sUnreferencedRodata[8] = { 0 };
