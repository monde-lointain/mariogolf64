#include "asset.h"
#include "billboard.h"
#include "common.h"
#include "course.h"
#include "debug_text.h"
#include "functions.h"
#include "game_flags.h"
#include "hole_loader.h"
#include "hole_triangles.h"
#include "host_file.h"
#include "math_util.h"
#include "save.h"
#include "scene.h"
#include "terrain.h"
#include "variables.h"
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
#define LAST_MINI_GOLF_PAINT_LOGIC 11
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
#define HOLE_FILE_BUFFER ((u8 *)0x8030F800)
/* The table of hole asset ids has a row for each course and then some */
#define HOLE_ASSET_COURSE_COUNT 9

/* A column and row offset from a point, with its distance */
typedef struct GridOffset {
    /* 0x0 */ s8 column;
    /* 0x1 */ s8 row;
    /* 0x2 */ u16 distance;
} GridOffset; /* size = 0x4 */

/* The largest span ListOffsetsByDistance handles */
#define MAX_OFFSET_SPAN GROUND_POINT_ROW_COUNT
/* The units per grid step of the distances that ListOffsetsByDistance lists */
#define OFFSET_DISTANCE_SCALE 256
/* Marks an offset that ListOffsetsByDistance has already listed */
#define LISTED_DISTANCE 0xFFFF
/* Farther than any distance ListOffsetsByDistance computes */
#define BEYOND_ALL_DISTANCES 0x10000

/* A colour that lights or shades the ground */
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
#define HEIGHT_ROW_SPAN 16
#define HEIGHT_FRACTION_BITS 12
/* Scales a step per ground unit to a step per map row in HEIGHT_FRACTION_BITS fixed point */
#define HEIGHT_ROW_STEP_SCALE 65536.0f
/* sHeightStepScales holds 65536 / n for the counts below this */
#define HEIGHT_STEP_SCALE_COUNT 32

/* The points of a tile's grid, edges included, that MeasureGroundTiles averages */
#define GROUND_TILE_AVERAGED_POINT_COUNT ((GROUND_TILE_POINT_SPAN + 1) * (GROUND_TILE_POINT_SPAN + 1))
#define MIN_GROUND_TILE_HEIGHT_RANGE 32
#define FLAT_GROUND_TILE_HEIGHT_RANGE 30
/* Splits a map column or row into a tile and a texel within it */
#define MAP_TILE_SHIFT 5
#define MAP_TILE_MASK (GROUND_TILE_TEXEL_SPAN - 1)

/* The work-area buffers that the ground tiles' display lists and vertices are built in */
#define GROUND_DISPLAY_LIST_START ((Gfx *)&g_WorkArea.bytes[0x6E920])
#define GROUND_DISPLAY_LIST_END ((Gfx *)&g_WorkArea.bytes[0x77D20])
#define GROUND_VERTEX_START ((Vtx *)&g_WorkArea.bytes[0x77F20])
#define GROUND_VERTEX_END ((Vtx *)&g_WorkArea.bytes[0x82720])

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

extern HoleAssetIds sHoleAssetIds[HOLE_ASSET_COURSE_COUNT][COURSE_HOLE_COUNT];
extern AssetFile sHoleAsset;
extern u32 sPaintSeed;
extern s32 sIsCodeMapCleaned;
extern u8 *sReadCursor;
extern u8 *sBuildBuffer;
extern u8 *sBuildCursor;
extern s32 g_TerrainCodeBlockCount;
extern f32 sHeightStepScales[];
extern s32 sPaintLogic;
extern Vec3 g_MiniGolfScale;
extern s32 sHolePaintLogics[][COURSE_HOLE_COUNT];

void BuildGroundTileDisplayList(s32 column, s32 row, Gfx **gfx, Vtx **vertices);

void LoadCodeMap(char *path);
void EncodeCodeMap(void);
void CleanCodeMap(void);
void PaintGroundTiles(void);
void ClearHolePoints(void);
void ShadeGroundTiles(void);
void CastBillboardShadows(void);
void MoveRingsToEnd(void);
void FillRandomGrid(s32 range, s32 base);

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

/*
 * Fills offsets with up to maxCount column and row offsets less than span
 * from the origin, nearest first, and returns how many it wrote. Distances
 * are in units of 1/OFFSET_DISTANCE_SCALE. It does not match: the row
 * counter and the squares pointer swap registers.
 */
#ifdef NON_MATCHING
s32 ListOffsetsByDistance(s32 span, GridOffset *offsets, s32 maxCount) {
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
        row++;
        squares[row] = (f32)(row * row) * (OFFSET_DISTANCE_SCALE * OFFSET_DISTANCE_SCALE);
    }
    for (row = 0; row < span; row++) {
        for (column = 0; column < span; column++) {
            distances[row * span + column] = sqrtf(squares[column] + squares[row]);
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
#else
INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", ListOffsetsByDistance);
#endif

/*
 * Fills heights, a band of HEIGHT_BAND_ROW_COUNT map rows by MAP_COLUMN_COUNT
 * columns, with the ground's heights, by drawing the two triangles of each
 * ground grid square that covers the band. Where triangles overlap, it averages
 * their heights. A band at or past GROUND_TILE_ROW_COUNT is left untouched.
 * It does not match: the nested DrawTriangleHeights allocates its registers
 * differently, using $s7 where the original reloads the static chain.
 */
#ifdef NON_MATCHING
void DrawBandHeights(s16 *heights, s32 band) {
    /* corners[4] is where the corners are swapped; it is there to match */
    Vtx *corners[5];
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
        inline void DrawHeightRow(s32 longX, s32 longHeight, s32 shortX, s32 shortHeight, s32 texelRow) {
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
            s16 *texel;
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
                texelHeightStep = sHeightStepScales[count] * (toHeight - fromHeight);
            } else {
                texelHeightStep = ((toHeight - fromHeight) << 16) / count;
            }
            texelColumn = longX >> (HEIGHT_FRACTION_BITS + 4);
            texelRow = (texelRow >> 4) - band * HEIGHT_BAND_ROW_COUNT;
            if (texelRow >= 0 && texelRow < HEIGHT_BAND_ROW_COUNT) {
                height = fromHeight << 16;
                texel = &heights[texelRow * MAP_COLUMN_COUNT];
                texel += texelColumn;
                for (texelIndex = 0; texelIndex <= count; texelIndex++) {
                    if (texelColumn >= 0 && texelColumn < MAP_COLUMN_COUNT) {
                        value = height >> 16;
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

        totalSpan = corners[2]->v.ob[2] - corners[0]->v.ob[2];
        if (cachedSpan != totalSpan) {
            cachedSpan = totalSpan;
            cachedReciprocal = 1.0f / totalSpan;
        }
        longSlopeX = (corners[2]->v.ob[0] - corners[0]->v.ob[0]) * cachedReciprocal;
        longSlopeHeight = (corners[2]->v.ob[1] - corners[0]->v.ob[1]) * cachedReciprocal;
        span = corners[1]->v.ob[2] - corners[0]->v.ob[2];
        if (span != 0) {
            if (cachedSpan != span) {
                cachedSpan = span;
                cachedReciprocal = 1.0f / span;
            }
            shortSlopeX = (corners[1]->v.ob[0] - corners[0]->v.ob[0]) * cachedReciprocal;
            shortSlopeHeight = (corners[1]->v.ob[1] - corners[0]->v.ob[1]) * cachedReciprocal;
            longXStep = longSlopeX * HEIGHT_ROW_STEP_SCALE;
            shortXStep = shortSlopeX * HEIGHT_ROW_STEP_SCALE;
            longHeightStep = longSlopeHeight * HEIGHT_ROW_STEP_SCALE;
            shortHeightStep = shortSlopeHeight * HEIGHT_ROW_STEP_SCALE;
            longX = shortX = corners[0]->v.ob[0] << HEIGHT_FRACTION_BITS;
            longHeight = shortHeight = corners[0]->v.ob[1] << HEIGHT_FRACTION_BITS;
            for (z = 0; z < span + HEIGHT_ROW_SPAN / 2; z += HEIGHT_ROW_SPAN) {
                DrawHeightRow(longX, longHeight, shortX, shortHeight, corners[0]->v.ob[2] + z);
                longX += longXStep;
                shortX += shortXStep;
                longHeight += longHeightStep;
                shortHeight += shortHeightStep;
            }
        }
        lowerSpan = corners[2]->v.ob[2] - corners[1]->v.ob[2];
        if (lowerSpan != 0) {
            if (cachedSpan != lowerSpan) {
                cachedSpan = lowerSpan;
                cachedReciprocal = 1.0f / lowerSpan;
            }
            longX = ((s32)(longSlopeX * span) + corners[0]->v.ob[0]) << HEIGHT_FRACTION_BITS;
            longHeight = ((s32)(longSlopeHeight * span) + corners[0]->v.ob[1]) << HEIGHT_FRACTION_BITS;
            shortSlopeX = (corners[2]->v.ob[0] - corners[1]->v.ob[0]) * cachedReciprocal;
            shortSlopeHeight = (corners[2]->v.ob[1] - corners[1]->v.ob[1]) * cachedReciprocal;
            longXStep = longSlopeX * HEIGHT_ROW_STEP_SCALE;
            shortXStep = shortSlopeX * HEIGHT_ROW_STEP_SCALE;
            longHeightStep = longSlopeHeight * HEIGHT_ROW_STEP_SCALE;
            shortHeightStep = shortSlopeHeight * HEIGHT_ROW_STEP_SCALE;
            shortX = corners[1]->v.ob[0] << HEIGHT_FRACTION_BITS;
            shortHeight = corners[1]->v.ob[1] << HEIGHT_FRACTION_BITS;
            for (z = 0; z < lowerSpan + HEIGHT_ROW_SPAN / 2; z += HEIGHT_ROW_SPAN) {
                DrawHeightRow(longX, longHeight, shortX, shortHeight, corners[1]->v.ob[2] + z);
                longX += longXStep;
                shortX += shortXStep;
                longHeight += longHeightStep;
                shortHeight += shortHeightStep;
            }
        }
    }

    cachedSpan = -1;
    if (band < GROUND_TILE_ROW_COUNT) {
        ((u32 *)heights)[0] = NO_HEIGHT_PAIR;
        ((u32 *)heights)[1] = NO_HEIGHT_PAIR;
        fill = *(f64 *)heights;
        for (column = 1; column < HEIGHT_BAND_ROW_COUNT * MAP_COLUMN_COUNT / 4; column++) {
            ((f64 *)heights)[column] = fill;
        }
        for (row = band * GROUND_TILE_POINT_SPAN; row < (band + 1) * GROUND_TILE_POINT_SPAN; row++) {
            for (column = 0; column < GROUND_POINT_COLUMN_COUNT - 1; column++) {
                corners[0] = GetGroundPoint(column, row);
                corners[1] = GetGroundPoint(column + 1, row);
                corners[2] = GetGroundPoint(column, row + 1);
                corners[3] = GetGroundPoint(column + 1, row + 1);
                if (corners[1]->v.ob[2] < corners[0]->v.ob[2]) {
                    corners[4] = corners[0];
                    corners[0] = corners[1];
                    corners[1] = corners[4];
                }
                if (corners[2]->v.ob[2] < corners[1]->v.ob[2]) {
                    corners[4] = corners[1];
                    corners[1] = corners[2];
                    corners[2] = corners[4];
                }
                if (corners[1]->v.ob[2] < corners[0]->v.ob[2]) {
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
                if (corners[1]->v.ob[2] < corners[0]->v.ob[2]) {
                    corners[4] = corners[0];
                    corners[0] = corners[1];
                    corners[1] = corners[4];
                }
                if (corners[2]->v.ob[2] < corners[1]->v.ob[2]) {
                    corners[4] = corners[1];
                    corners[1] = corners[2];
                    corners[2] = corners[4];
                }
                if (corners[1]->v.ob[2] < corners[0]->v.ob[2]) {
                    corners[4] = corners[0];
                    corners[0] = corners[1];
                    corners[1] = corners[4];
                }
                DrawTriangleHeights();
            }
        }
        /* An empty loop; the original has it */
        for (column = 0; column < 0x2000; column++) {
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", DrawTriangleHeights);
INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", DrawBandHeights);
#endif

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

/*
 * Whether a green cell lies within one cell of other terrain. It is a macro,
 * as an inline function compiles differently from the original.
 */
#define IsGreenEdge(code, nearestCode, nearestDistance) \
    ((code) == TERRAIN_CODE_GREEN && (code) != (nearestCode) && (nearestDistance) < 2)

/*
 * The paint logic of Toad Highlands, Koopa Park and Mario's Star. It returns
 * a cell's colour in the upper half and its painted terrain code in the
 * lower: fairway near other terrain is painted as semi-rough, fairway and
 * greens are striped, rough away from fairway and TERRAIN_CODE_UNK3 are
 * mottled with random colours, and greens are edged as green edge.
 */
s32 PaintMainCourse(s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
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
            if (nearestDistance >= 2 || nearestCode == TERRAIN_CODE_GREEN || nearestCode == TERRAIN_CODE_FAIRWAY) {
                sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
                fairwayRandom = sPaintSeed >> 16;
                if ((x + 2) & 4) {
                    color = (((fairwayRandom & 3) + 9) << 11) | 0x4C4;
                } else {
                    color = (((fairwayRandom & 3) + 7) << 11) | 0x584;
                }
                paintedCode = TERRAIN_CODE_FAIRWAY;
            } else {
                color = 0x4605;
                paintedCode = TERRAIN_CODE_SEMI_ROUGH;
            }
            break;
        case TERRAIN_CODE_ROUGH:
            if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
                sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
                red = (sPaintSeed >> 16) & 7;
                green = 16 - (red >> 1);
                color = (red << 11) | (green << 6) | 1;
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
            sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
            random = sPaintSeed >> 16;
            color = (((random & 4) + 2) << 11) | ((4 - ((random >> 5) & 3)) << 6) | 1;
            break;
        case TERRAIN_CODE_GREEN:
            if (IsGreenEdge(code, nearestCode, nearestDistance)) {
                color = 0x2587;
                paintedCode = TERRAIN_CODE_GREEN_EDGE;
            } else if (nearestCode == TERRAIN_CODE_GREEN || nearestDistance >= 2) {
                if (z & 2) {
                    color = (x & 2) ? 0x2F84 : 0x4580;
                } else {
                    color = (x & 2) ? 0x3E42 : 0x3704;
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
 * it. Neither the paint-logic table nor any code refers to it. It does not
 * match: the original keeps the test between the two equal cart-way colours
 * and has no stack frame.
 */
#ifdef NON_MATCHING
s32 PaintPaleRoughCourse(s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
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
            if (nearestDistance >= 2 || nearestCode == TERRAIN_CODE_GREEN || nearestCode == TERRAIN_CODE_FAIRWAY) {
                sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
                fairwayRandom = sPaintSeed >> 16;
                if ((x + 2) & 4) {
                    color = (((fairwayRandom & 3) + 14) << 11) | 0x4CE;
                } else {
                    color = ((fairwayRandom & 3) << 11) | 0x658E;
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
                sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
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
            if (nearestCode == TERRAIN_CODE_UNK3 || nearestDistance >= 2) {
                color = 0x3E;
                paintedCode = TERRAIN_CODE_UNK3;
            } else {
                /* Both colours are the same in the original */
                if (nearestDistance == 1) {
                    color = 0x94A5;
                } else {
                    color = 0x94A5;
                }
                paintedCode = TERRAIN_CODE_CART_WAY;
            }
            break;
        case TERRAIN_CODE_GREEN:
            if (IsGreenEdge(code, nearestCode, nearestDistance)) {
                color = 0x2587;
                paintedCode = TERRAIN_CODE_GREEN_EDGE;
            } else if (nearestCode == TERRAIN_CODE_GREEN || nearestDistance >= 2) {
                if (z & 2) {
                    color = (x & 2) ? 0x2F84 : 0x4680;
                } else {
                    color = (x & 2) ? 0x3F02 : 0x3704;
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
 * colours, except that deep rough is painted as waste area and rough away
 * from fairway is mottled with other random colours
 */
s32 PaintShyGuyDesert(s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
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
            if (nearestDistance >= 2 || nearestCode == TERRAIN_CODE_GREEN || nearestCode == TERRAIN_CODE_FAIRWAY) {
                sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
                fairwayRandom = sPaintSeed >> 16;
                if ((x + 2) & 4) {
                    color = (((fairwayRandom & 3) + 19) << 11) | 0x688;
                } else {
                    color = (((fairwayRandom & 3) + 14) << 11) | 0x5C6;
                }
                paintedCode = TERRAIN_CODE_FAIRWAY;
            } else {
                color = 0x9E89;
                paintedCode = TERRAIN_CODE_SEMI_ROUGH;
            }
            break;
        case TERRAIN_CODE_ROUGH:
            if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
                sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
                red = ((sPaintSeed >> 16) & 3) + 10;
                green = red + 3;
                color = (red << 11) | (green << 6) | 5;
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
            sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
            random = sPaintSeed >> 16;
            color = (((random & 4) + 2) << 11) | ((4 - ((random >> 5) & 3)) << 6) | 1;
            break;
        case TERRAIN_CODE_GREEN:
            if (IsGreenEdge(code, nearestCode, nearestDistance)) {
                color = 0x7E4B;
                paintedCode = TERRAIN_CODE_GREEN_EDGE;
            } else if (nearestCode == TERRAIN_CODE_GREEN || nearestDistance >= 2) {
                if (z & 2) {
                    color = (x & 2) ? 0x7E4A : 0x7746;
                } else {
                    color = (x & 2) ? 0x8E88 : 0x8606;
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

/* The paint logic of Boo Valley: PaintMainCourse with other bunker and out-of-bounds colours */
s32 PaintBooValley(s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
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
            if (nearestDistance >= 2 || nearestCode == TERRAIN_CODE_GREEN || nearestCode == TERRAIN_CODE_FAIRWAY) {
                sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
                fairwayRandom = sPaintSeed >> 16;
                if ((x + 2) & 4) {
                    color = (((fairwayRandom & 3) + 9) << 11) | 0x4C4;
                } else {
                    color = (((fairwayRandom & 3) + 7) << 11) | 0x584;
                }
                paintedCode = TERRAIN_CODE_FAIRWAY;
            } else {
                color = 0x4605;
                paintedCode = TERRAIN_CODE_SEMI_ROUGH;
            }
            break;
        case TERRAIN_CODE_ROUGH:
            if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
                sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
                red = (sPaintSeed >> 16) & 7;
                green = 16 - (red >> 1);
                color = (red << 11) | (green << 6) | 1;
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
            sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
            random = sPaintSeed >> 16;
            color = (((random & 4) + 2) << 11) | ((4 - ((random >> 5) & 3)) << 6) | 1;
            break;
        case TERRAIN_CODE_GREEN:
            if (IsGreenEdge(code, nearestCode, nearestDistance)) {
                color = 0x2587;
                paintedCode = TERRAIN_CODE_GREEN_EDGE;
            } else if (nearestCode == TERRAIN_CODE_GREEN || nearestDistance >= 2) {
                if (z & 2) {
                    color = (x & 2) ? 0x2F84 : 0x4580;
                } else {
                    color = (x & 2) ? 0x3E42 : 0x3704;
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

/* The paint logic of Yoshi's Island: PaintMainCourse with other colours */
s32 PaintYoshisIsland(s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
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
            if (nearestDistance >= 2 || nearestCode == TERRAIN_CODE_GREEN || nearestCode == TERRAIN_CODE_FAIRWAY) {
                sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
                fairwayRandom = sPaintSeed >> 16;
                if ((x + 2) & 4) {
                    color = (((fairwayRandom & 3) + 7) << 11) | 0x4D4;
                } else {
                    color = (((fairwayRandom & 3) + 7) << 11) | 0x590;
                }
                paintedCode = TERRAIN_CODE_FAIRWAY;
            } else {
                color = 0x4591;
                paintedCode = TERRAIN_CODE_SEMI_ROUGH;
            }
            break;
        case TERRAIN_CODE_ROUGH:
            if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
                sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
                red = (sPaintSeed >> 16) & 7;
                green = 16 - (red >> 1);
                color = (red << 11) | (green << 6) | 0xD;
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
            sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
            random = sPaintSeed >> 16;
            color = (((random & 4) + 2) << 11) | ((4 - ((random >> 5) & 3)) << 6) | 1;
            break;
        case TERRAIN_CODE_GREEN:
            if (IsGreenEdge(code, nearestCode, nearestDistance)) {
                color = 0x3C04;
                paintedCode = TERRAIN_CODE_GREEN_EDGE;
            } else if (nearestCode == TERRAIN_CODE_GREEN || nearestDistance >= 2) {
                if (z & 2) {
                    color = (x & 2) ? 0x2E04 : 0x4480;
                } else {
                    color = (x & 2) ? 0x3CC2 : 0x3584;
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
s32 PaintUnlistedCourse(s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
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
            if (nearestDistance >= 2 || nearestCode == TERRAIN_CODE_GREEN || nearestCode == TERRAIN_CODE_FAIRWAY) {
                sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
                fairwayRandom = sPaintSeed >> 16;
                if ((x + 2) & 4) {
                    color = (((fairwayRandom & 3) + 9) << 11) | 0x4C4;
                } else {
                    color = (((fairwayRandom & 3) + 7) << 11) | 0x584;
                }
                paintedCode = TERRAIN_CODE_FAIRWAY;
            } else {
                color = 0x4605;
                paintedCode = TERRAIN_CODE_SEMI_ROUGH;
            }
            break;
        case TERRAIN_CODE_ROUGH:
            if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
                sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
                red = (sPaintSeed >> 16) & 7;
                green = 16 - (red >> 1);
                color = (red << 11) | (green << 6) | 1;
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
            sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
            random = sPaintSeed >> 16;
            color = (((random & 7) + 4) << 11) | ((4 - ((random >> 5) & 3)) << 6);
            break;
        case TERRAIN_CODE_GREEN:
            if (IsGreenEdge(code, nearestCode, nearestDistance)) {
                color = 0x2587;
                paintedCode = TERRAIN_CODE_GREEN_EDGE;
            } else if (nearestCode == TERRAIN_CODE_GREEN || nearestDistance >= 2) {
                if (z & 2) {
                    color = (x & 2) ? 0x2F84 : 0x4580;
                } else {
                    color = (x & 2) ? 0x3E42 : 0x3704;
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
 * cell's colour in the upper half and its painted terrain code in the
 * lower: greens are striped and edged, TERRAIN_CODE_UNK3 is mottled with
 * random colours, and anything other than out of bounds is painted white as
 * out of bounds.
 */
s32 PaintFirstMiniGolfCourse(s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
    u32 color;
    s32 paintedCode;
    s32 isOtherNearest;
    s32 random;

    switch (code) {
        case TERRAIN_CODE_UNK3:
            paintedCode = TERRAIN_CODE_UNK3;
            sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
            random = sPaintSeed >> 16;
            color = (((random & 7) + 4) << 11) | ((4 - ((random >> 5) & 3)) << 6);
            break;
        case TERRAIN_CODE_GREEN:
            /* The assignment in the test is there to match */
            if ((isOtherNearest = nearestCode != TERRAIN_CODE_GREEN) && nearestDistance < 2) {
                color = 0x650B;
                paintedCode = TERRAIN_CODE_GREEN;
            } else if (nearestCode == TERRAIN_CODE_GREEN || nearestDistance >= 2) {
                if (z & 8) {
                    color = (x & 4) ? 0xCE72 : 0x650B;
                } else {
                    color = (x & 4) ? 0x650B : 0xCE72;
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
 * PaintFirstMiniGolfCourse with other colours, except that greens are painted as
 * fairway
 */
s32 PaintSecondMiniGolfCourse(s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
    u32 color;
    s32 paintedCode;
    s32 isOtherNearest;
    s32 random;

    switch (code) {
        case TERRAIN_CODE_UNK3:
            paintedCode = TERRAIN_CODE_UNK3;
            sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
            random = sPaintSeed >> 16;
            color = (((random & 4) + 2) << 11) | ((4 - ((random >> 5) & 3)) << 6) | 1;
            break;
        case TERRAIN_CODE_GREEN:
            /* The assignment in the test is there to match */
            if ((isOtherNearest = nearestCode != TERRAIN_CODE_GREEN) && nearestDistance < 2) {
                color = 0x4C73;
                paintedCode = TERRAIN_CODE_FAIRWAY;
            } else if (nearestCode == TERRAIN_CODE_GREEN || nearestDistance >= 2) {
                if (z & 8) {
                    color = (x & 4) ? 0x4C72 : 0xBEB4;
                } else {
                    color = (x & 4) ? 0xBEB4 : 0x4C72;
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
 * PaintFirstMiniGolfCourse with other colours, except that greens are painted as
 * cart way and other terrain is painted red
 */
s32 PaintThirdMiniGolfCourse(s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
    u32 color;
    s32 paintedCode;
    s32 random;

    switch (code) {
        case TERRAIN_CODE_UNK3:
            paintedCode = TERRAIN_CODE_UNK3;
            sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
            random = sPaintSeed >> 16;
            color = (((random & 4) + 2) << 11) | ((4 - ((random >> 5) & 3)) << 6) | 1;
            break;
        case TERRAIN_CODE_GREEN:
            if (nearestCode != TERRAIN_CODE_GREEN && nearestDistance < 2) {
                color = 0xDC61;
                paintedCode = TERRAIN_CODE_CART_WAY;
            } else {
                if (z & 8) {
                    color = (x & 4) ? 0xDC60 : 0xCE72;
                } else {
                    color = (x & 4) ? 0xCE72 : 0xDC60;
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
 * The paint logic of the driving range. It returns a cell's colour in the
 * upper half and its painted terrain code in the lower: fairway and greens
 * are striped, rough away from fairway and TERRAIN_CODE_UNK3 are mottled
 * with random colours, and greens are edged as green edge.
 */
s32 PaintDrivingRange(s32 code, s32 nearestCode, s32 nearestDistance, s32 x, s32 z) {
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
            color = ((x + 2) & 4) ? 0x5CC4 : 0x4584;
            paintedCode = TERRAIN_CODE_FAIRWAY;
            break;
        case TERRAIN_CODE_ROUGH:
            if (nearestCode != TERRAIN_CODE_FAIRWAY || nearestDistance > 0) {
                sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
                red = (sPaintSeed >> 16) & 7;
                green = 16 - (red >> 1);
                color = (red << 11) | (green << 6) | 1;
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
            sPaintSeed = sPaintSeed * 0x5D588B65 + 1;
            random = sPaintSeed >> 16;
            color = (((random & 4) + 2) << 11) | ((4 - ((random >> 5) & 3)) << 6) | 1;
            break;
        case TERRAIN_CODE_GREEN:
            if (IsGreenEdge(code, nearestCode, nearestDistance)) {
                color = 0x2587;
                paintedCode = TERRAIN_CODE_GREEN_EDGE;
            } else if (nearestCode == TERRAIN_CODE_GREEN || nearestDistance >= 2) {
                if (z & 2) {
                    color = (x & 2) ? 0x2F84 : 0x4580;
                } else {
                    color = (x & 2) ? 0x3E42 : 0x3704;
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

/* Returns the map texel at a column and row of the whole hole's map */
static inline u16 *GetMapTexel(s32 column, s32 row) {
    /* tileRow is there to match */
    s32 tileRow = row >> MAP_TILE_SHIFT;
    u16 *texels = (u16 *)&g_WorkArea.hole.tiles[column / GROUND_TILE_TEXEL_SPAN +
                                                tileRow * GROUND_TILE_COLUMN_COUNT];

    return &texels[(row & MAP_TILE_MASK) * GROUND_TILE_TEXEL_SPAN + (column & MAP_TILE_MASK)];
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
 * buffer. It does not match: the original adds the work area's address to
 * the tile offset after it computes the texel offset, so it reads the texel
 * into $v0 rather than $v1.
 */
#ifdef NON_MATCHING
void EncodeCodeMap(void) {
    s32 column;
    s32 row;

    sBuildBuffer = sBuildCursor = HOLE_FILE_BUFFER;
    for (row = 0; row < MAP_ROW_COUNT; row++) {
        for (column = 0; column < MAP_COLUMN_COUNT; column++) {
            sBuildCursor = &sBuildBuffer[row * MAP_COLUMN_COUNT + column];
            *sBuildCursor = GetColorTerrainCode(ReadMapTexel(column, row));
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
 * with the colours of its terrain codes. It does not match: like
 * EncodeCodeMap, the original adds the work area's address to the tile
 * offset after it computes the texel offset.
 */
#ifdef NON_MATCHING
void LoadCodeMap(char *path) {
    u32 size;
    s32 tileIndex;
    u32 texelIndex;
    s32 column;
    s32 row;
    u16 color;

    sBuildBuffer = sBuildCursor = HOLE_FILE_BUFFER;
    if (ReadHostFile(path, &size) == HOST_FILE_OK && size == MAP_COLUMN_COUNT * MAP_ROW_COUNT) {
        sBuildCursor = sBuildBuffer;
    }
    for (tileIndex = 0; tileIndex < GROUND_TILE_COUNT; tileIndex++) {
        for (texelIndex = 0; texelIndex < GROUND_TILE_TEXEL_ROW_COUNT * GROUND_TILE_TEXEL_ROW_LENGTH; texelIndex++) {
            g_WorkArea.hole.tiles[tileIndex].texels[0][texelIndex] = 0;
        }
    }
    for (row = 0; row < MAP_ROW_COUNT; row++) {
        for (column = 0; column < MAP_COLUMN_COUNT; column++) {
            sBuildCursor = &sBuildBuffer[row * MAP_COLUMN_COUNT + column];
            color = GetTerrainCodeColor(*sBuildCursor);
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

/*
 * Sets each ground tile's centre to the average of its ground points and its
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
    Vtx *point;

    tileIndex = 0;
    for (tileRow = 0; tileRow < GROUND_TILE_ROW_COUNT; tileRow++) {
        for (tileColumn = 0; tileColumn < GROUND_TILE_COLUMN_COUNT; tileColumn++) {
            sumX = 0;
            sumY = 0;
            sumZ = 0;
            lowest = 0x40000000;
            highest = -0x40000000;
            for (pointRow = 0; pointRow <= GROUND_TILE_POINT_SPAN; pointRow++) {
                for (pointColumn = 0; pointColumn <= GROUND_TILE_POINT_SPAN; pointColumn++) {
                    point = GetGroundPoint(tileColumn * GROUND_TILE_POINT_SPAN + pointColumn,
                                           tileRow * GROUND_TILE_POINT_SPAN + pointRow);
                    sumX += point->v.ob[0];
                    sumY += point->v.ob[1];
                    sumZ += point->v.ob[2];
                    if (point->v.ob[1] < lowest) {
                        lowest = point->v.ob[1];
                    }
                    if (point->v.ob[1] > highest) {
                        highest = point->v.ob[1];
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
            g_WorkArea.hole.tiles[tileIndex].center[0] = sumX / GROUND_TILE_AVERAGED_POINT_COUNT;
            g_WorkArea.hole.tiles[tileIndex].center[1] = sumY / GROUND_TILE_AVERAGED_POINT_COUNT;
            g_WorkArea.hole.tiles[tileIndex].center[2] = sumZ / GROUND_TILE_AVERAGED_POINT_COUNT;
            tileIndex++;
        }
    }
    return tileIndex;
}

/*
 * Scales the hole's ground points, tile centres, billboards, tee and cup
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
    D_800BA9D8.teePosition[AXIS_X] *= g_MiniGolfScale[AXIS_X];
    D_800BA9D8.teePosition[AXIS_Y] *= g_MiniGolfScale[AXIS_Y];
    D_800BA9D8.teePosition[AXIS_Z] *= g_MiniGolfScale[AXIS_Z];
    D_800BA9D8.cupPosition[AXIS_X] *= g_MiniGolfScale[AXIS_X];
    D_800BA9D8.cupPosition[AXIS_Y] *= g_MiniGolfScale[AXIS_Y];
    D_800BA9D8.cupPosition[AXIS_Z] *= g_MiniGolfScale[AXIS_Z];
}

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
    if (course == 2) {
        FillRandomGrid(0x60, 0xC8);
    } else if (course == 3) {
        FillRandomGrid(0x60, 0xC8);
    } else {
        FillRandomGrid(0x60, 0xBE);
    }
    if (g_IsMiniGolf) {
        sPaintLogic = g_CourseIndex;
        if (g_CourseIndex > LAST_MINI_GOLF_PAINT_LOGIC) {
            sPaintLogic = LAST_MINI_GOLF_PAINT_LOGIC;
        }
        FillRandomGrid(0x40, 0xD2);
    } else if (IsGameFlagSet(GAME_FLAG_READ_HOST_HOLE)) {
        sPaintLogic = sHolePaintLogics[course][hole - 1];
        if (IsGameFlagSet(GAME_FLAG_UNK6A)) {
            sPaintLogic++;
        }
        if (IsGameFlagSet(GAME_FLAG_UNK6B)) {
            sPaintLogic++;
        }
    } else {
        sPaintLogic = GetCoursePaintLogic(course);
    }
    osSyncPrintf("PaintLogic%d\n", sPaintLogic);
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
            if (g_GameMode == GAME_MODE_UNK12) {
                OpenAssetFile(UNK12_UNK6_ASSET, &asset);
            } else {
                OpenAssetFile(sHoleAssetIds[course][hole].unk6, &asset);
            }
            ReadAssetFile(&D_800BA9D8, GetAssetFileSize(&asset), &asset);
        }
        if (g_GameMode == GAME_MODE_UNK12) {
            OpenAssetFile(UNK12_POINTS_ASSET, &asset);
        } else {
            OpenAssetFile(sHoleAssetIds[course][hole].points, &asset);
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
        sIsCodeMapCleaned = TRUE;
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
        sprintf(path, "bin/course%d/hole%d%2.2d.vtx", course, course, hole);
        ReadHostFile(path, &size);
        sprintf(path, "bin/course%d/hole%d%2.2d.dtx", course, course, hole);
        ReadHostFile(path, &size);
        sprintf(path, "bin/course%d/hole%d%2.2d.tre", course, course, hole);
        ReadHostFile(path, &size);
        sprintf(path, "bin/course%d/hole%d%2.2d.hol", course, course, hole);
        ReadHostFile(path, &size);
        sprintf(path, "bin/course%d/hole%d%2.2d.pnt", course, course, hole);
        ReadHostFile(path, &size);
        sprintf(path, "bin/course%d/hole%d%2.2d.tri", course, course, hole);
        if (ReadHostFile(path, &size) == HOST_FILE_OK) {
            MoveRingsToEnd();
        }
        sprintf(path, "bin/course%d/hole%d%2.2d.att", course, course, hole);
        ReadHostFile(path, &size);
    }
    osSyncPrintf("@1\n");
    MeasureGroundTiles();
    sIsCodeMapCleaned = TRUE;
    PaintGroundTiles();
    PlaceBillboards();
    osSyncPrintf("Ok!\n");
}

/*
 * Writes a hole's files to the host, the attributes as the map's terrain
 * codes, a byte each. A cleaned code map is first restored from its backup.
 * It does not match for the same reason as EncodeCodeMap.
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
    if (sIsCodeMapCleaned) {
        LoadBackupCodeMap();
        sIsCodeMapCleaned = FALSE;
    }
    sBuildBuffer = sBuildCursor = HOLE_FILE_BUFFER;
    for (row = 0; row < MAP_ROW_COUNT; row++) {
        for (column = 0; column < MAP_COLUMN_COUNT; column++) {
            sBuildCursor = &sBuildBuffer[row * MAP_COLUMN_COUNT + column];
            *sBuildCursor = GetColorTerrainCode(ReadMapTexel(column, row));
        }
    }
    sprintf(path, "bin/course%d/hole%d%2.2d.att", course, course, hole);
    if (WriteHostFile(path, MAP_COLUMN_COUNT * MAP_ROW_COUNT) == HOST_FILE_OK) {
        sBuildCursor = sBuildBuffer;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", SaveHoleFiles);
#endif

/*
 * Moves the rings, from the first triangle that is one, to the last
 * triangle slots
 */
void MoveRingsToEnd(void) {
    s32 i;
    s32 slot;

    PrintDebugText("Change Start", 19, 19);
    for (i = 0; i != HOLE_TRIANGLE_COUNT - RING_COUNT; i++) {
        if (g_WorkArea.hole.triangles[i].kind == HOLE_TRIANGLE_KIND_RING) {
            for (slot = HOLE_TRIANGLE_COUNT - RING_COUNT; slot != HOLE_TRIANGLE_COUNT; slot++, i++) {
                sprintf(g_PrintBuffer, "%d <-- %d", slot, i);
                PrintDebugText(g_PrintBuffer, 4, -141);
                g_WorkArea.hole.triangles[slot] = g_WorkArea.hole.triangles[i];
                bzero(&g_WorkArea.hole.triangles[i], sizeof(HoleTriangle));
            }
            PrintDebugText("Change End", 19, 19);
            return;
        }
    }
}

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
            g_WorkArea.hole.midpoints[tile][i].v.tc[0] = (x + offsets[i][0]) * GRID_TEXTURE_SCALE + GRID_TEXTURE_BORDER;
            g_WorkArea.hole.midpoints[tile][i].v.tc[1] = (z + offsets[i][1]) * GRID_TEXTURE_SCALE + GRID_TEXTURE_BORDER;
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
 * Unpacks runs of attributes from the hole file into the build buffer, each
 * a count and a byte whose two nibbles go to the two bytes of an attribute,
 * then clears the rest of the buffer. Nothing calls it.
 */
void ReadAttributeRuns(void) {
    s16 count;
    u8 packed;
    u16 *attribute;
    u16 *end;
    u16 *runEnd;
    s32 value;

    attribute = (u16 *)sBuildCursor;
    end = (u16 *)(sBuildCursor + ATTRIBUTE_BUFFER_SIZE);
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
        value = ((packed & 0xF0) << 4) | (packed & 0xF);
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
    sBuildCursor = sBuildBuffer;
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
        ReadFileBytes(&g_Billboards[i].unk8, sizeof(s8));
        g_Billboards[i].isNearCourseEdge = FALSE;
        ReadFileBytes(&g_Billboards[i].width, sizeof(u16));
        ReadFileBytes(&g_Billboards[i].height, sizeof(u16));
    }
}

/* Reads the hole triangles' kinds and corner positions from the hole's triangle file */
void ReadHoleTriangles(void) {
    /* Unused; it is there to match */
    u8 unused[0x10];
    u8 count;
    s8 kind;
    u8 i;
    HoleTriangle *triangle;

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

/*
 * Opens one of a hole's files and reads it to the hole file buffer, or
 * unpacks the attributes into the build buffer
 */
void OpenHoleAsset(s32 course, s32 hole, s8 kind) {
    sReadCursor = HOLE_FILE_BUFFER;
    switch (kind) {
        case HOLE_ASSET_VERTICES:
            if (g_GameMode == GAME_MODE_UNK12) {
                OpenAssetFile(UNK12_VERTICES_ASSET, &sHoleAsset);
            } else {
                OpenAssetFile(sHoleAssetIds[course][hole].vertices, &sHoleAsset);
            }
            break;
        case HOLE_ASSET_MIDPOINTS:
            if (g_GameMode == GAME_MODE_UNK12) {
                OpenAssetFile(UNK12_MIDPOINTS_ASSET, &sHoleAsset);
            } else {
                OpenAssetFile(sHoleAssetIds[course][hole].midpoints, &sHoleAsset);
            }
            break;
        case HOLE_ASSET_TREES:
            if (g_GameMode == GAME_MODE_UNK12) {
                OpenAssetFile(UNK12_TREES_ASSET, &sHoleAsset);
            } else {
                OpenAssetFile(sHoleAssetIds[course][hole].trees, &sHoleAsset);
            }
            break;
        case HOLE_ASSET_TRIANGLES:
            if (g_GameMode == GAME_MODE_UNK12) {
                OpenAssetFile(UNK12_TRIANGLES_ASSET, &sHoleAsset);
            } else {
                OpenAssetFile(sHoleAssetIds[course][hole].triangles, &sHoleAsset);
            }
            break;
        case HOLE_ASSET_ATTRIBUTES:
            if (g_GameMode == GAME_MODE_UNK12) {
                OpenAssetFile(UNK12_ATTRIBUTES_ASSET, &sHoleAsset);
            } else {
                OpenAssetFile(sHoleAssetIds[course][hole].attributes, &sHoleAsset);
            }
            GetAssetFileSize(&sHoleAsset);
            LoadLzData(sHoleAsset.romStart, sBuildCursor, sHoleAsset.size);
            return;
    }
    ReadAssetFile(sReadCursor, GetAssetFileSize(&sHoleAsset), &sHoleAsset);
}

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", CastBillboardShadows);

/*
 * Sets the ground's light and shade colours for the scene's colour index,
 * interpolating between those of the key indexes around it.
 * InterpolateChannel is a nested function, a GNU C extension.
 */
void GetGroundLightColors(GroundLightColor *light, GroundLightColor *shade) {
    GroundLightColor keyLights[2];
    GroundLightColor keyShades[2];
    s32 keyIndexes[2];
    s32 i;
    f32 fraction;

    s32 InterpolateChannel(s32 from, s32 to, f32 fraction) {
        return (to - from) * fraction + from;
    }

    if (g_HoleConditions.sceneColorIndex == SCENE_COLOR_INDEX_UNK11) {
        keyIndexes[1] = keyIndexes[0] = g_HoleConditions.sceneColorIndex;
    } else if ((s32)g_HoleConditions.sceneColorIndex >= SCENE_COLOR_INDEX_DRIVING_RANGE) {
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
                static const GroundLightColor endLight = { 23, 18, 14 };
                static const GroundLightColor endShade = { 10, 6, 0 };

                keyLights[i] = endLight;
                keyShades[i] = endShade;
                break;
            }
            case SCENE_COLOR_INDEX_DRIVING_RANGE: {
                static const GroundLightColor middleLight = { 21, 21, 21 };
                static const GroundLightColor middleShade = { 4, 4, 4 };

                keyLights[i] = middleLight;
                keyShades[i] = middleShade;
                break;
            }
            case SCENE_COLOR_INDEX_ROUND_START: {
                static const GroundLightColor startLight = { 20, 20, 20 };
                static const GroundLightColor startShade = { 6, 6, 6 };

                keyLights[i] = startLight;
                keyShades[i] = startShade;
                break;
            }
            case SCENE_COLOR_INDEX_UNK11: {
                static const GroundLightColor unk11Light = { 20, 20, 2 };
                static const GroundLightColor unk11Shade = { 0, 0, 0 };

                keyLights[i] = unk11Light;
                keyShades[i] = unk11Shade;
                break;
            }
        }
    }
    if (keyIndexes[1] == keyIndexes[0]) {
        fraction = 0.0f;
    } else {
        fraction = (f32)((s32)g_HoleConditions.sceneColorIndex - keyIndexes[0]) / (f32)(keyIndexes[1] - keyIndexes[0]);
    }
    light->red = InterpolateChannel(keyLights[0].red, keyLights[1].red, fraction);
    light->green = InterpolateChannel(keyLights[0].green, keyLights[1].green, fraction);
    light->blue = InterpolateChannel(keyLights[0].blue, keyLights[1].blue, fraction);
    shade->red = InterpolateChannel(keyShades[0].red, keyShades[1].red, fraction);
    shade->green = InterpolateChannel(keyShades[0].green, keyShades[1].green, fraction);
    shade->blue = InterpolateChannel(keyShades[0].blue, keyShades[1].blue, fraction);
}

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

INCLUDE_ASM("asm/nonmatchings/ovl_02/hole_builder", BuildGroundTileDisplayList);

/*
 * Builds every ground tile's display list and vertices in the work area,
 * reporting an error if either overflows its buffer
 */
void BuildGroundDisplayLists(void) {
    Gfx *gfx;
    Vtx *vertices;
    s32 column;
    s32 row;

    gfx = GROUND_DISPLAY_LIST_START;
    vertices = GROUND_VERTEX_START;
    for (row = 0; row < GROUND_TILE_ROW_COUNT; row++) {
        for (column = 0; column < GROUND_TILE_COLUMN_COUNT; column++) {
            BuildGroundTileDisplayList(column, row, &gfx, &vertices);
        }
    }
    if ((u32)gfx > (u32)GROUND_DISPLAY_LIST_END || (u32)vertices > (u32)GROUND_VERTEX_END) {
        ReportDivisionError();
    }
}

