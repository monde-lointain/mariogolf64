#include "common.h"
#include "heap.h"
#include "host_file.h"
#include "libc.h"
#include "main.h"
#include "screen.h"
#include "terrain.h"
#include "variables.h"
#include "work_area.h"

/* The points along a tile's edge, its corners included */
#define EDGE_POINT_COUNT (GROUND_TILE_POINT_SPAN + 1)
/* An interpolation weight runs from 0, all the first end, to this, all the second */
#define EDGE_WEIGHT_MAX 16

/* func_80405870 gathers the terrain codes of cells at least this far from a diagonal */
#define DIAGONAL_MIN_DISTANCE 3.0f
/* The sides of the two diagonals that func_80405870 gathers codes for */
#define DIAGONAL_SIDE_COUNT 4
/* How many different codes each side keeps */
#define SIDE_CODE_COUNT 2
#define NO_TERRAIN_CODE (-1)
#define HOLE_EDITOR_DIAGONAL_ASSERT_LINE 2046
/* The vertex units in a terrain code column or row */
#define VERTEX_UNITS_PER_CODE 16

/* The screen captures, 24-bit BMPs; the corner capture covers the frame's top-left corner */
#define CAPTURE_HEADER_SIZE 54
#define CORNER_CAPTURE_WIDTH 64
#define CORNER_CAPTURE_HEIGHT 128
#define CAPTURE_BYTES_PER_PIXEL 3
/* The size of a whole-screen capture */
#define SCREEN_CAPTURE_SIZE (CAPTURE_HEADER_SIZE + SCREEN_WIDTH * SCREEN_HEIGHT * CAPTURE_BYTES_PER_PIXEL)
/* Where the 5-bit channels sit in an RGBA16 pixel, and the shift that widens one to 8 bits */
#define RGBA16_RED_SHIFT 11
#define RGBA16_GREEN_SHIFT 6
#define RGBA16_BLUE_SHIFT 1
#define RGBA16_CHANNEL_MASK 0x1F
#define RGBA16_CHANNEL_WIDEN_SHIFT 3
/* Adds to a cached KSEG0 address to read the same memory uncached through KSEG1 */
#define KSEG1_OFFSET 0x20000000

extern u8 D_80451A60[];

/* func_80407D64's 32x32 IA8 wave pattern */
#define WAVE_TEXTURE_WIDTH 32
#define WAVE_TEXTURE_HEIGHT 32
/* A texel step's angle, pi/16, times the texel steps between samples */
#define WAVE_ANGLE_STEP 0.19635
#define WAVE_TEXEL_STEP 4
#define WAVE_AMPLITUDE 256.0
/* Cubing the amplitude-256 wave gives 24 bits; the top four are the level */
#define WAVE_LEVEL_SHIFT 20
#define WAVE_LEVEL_MIN 1
#define WAVE_LEVEL_MAX 15
#define IA8_ALPHA_SHIFT 4
extern u8 D_80491A60[WAVE_TEXTURE_HEIGHT * WAVE_TEXTURE_WIDTH];

s32 GetTerrainCode(s32 column, s32 row);
extern u8 D_8040C580_hole_editor[EDGE_POINT_COUNT][EDGE_POINT_COUNT];
extern s32 sScreenCaptureNumber;
extern u8 D_8040C710_hole_editor[CAPTURE_HEADER_SIZE];
extern u8 D_8040C748_hole_editor[CAPTURE_HEADER_SIZE];

void *func_80400000_hole_editor(void) {
    return D_80451A60;
}

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_8040000C_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_804002C0_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80400FBC_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80402428_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80402E1C_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80403A04_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_804045C0_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80404850_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80404B04_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80404B84_hole_editor);

static const char sEditorAssertionText[] = "EX";
static const char sEditorFileName[] = "c:/usgolf64/hiedit.c";

/* A plain if leaves GCC to invert the condition, which the game's code does not */
#define EDITOR_ASSERT(condition, lineNumber) \
    ((condition) ? (void)0 : ReportFailedAssertion(sEditorAssertionText, sEditorFileName, lineNumber))

/*
 * Gathers the terrain codes on each side of the two diagonals of the
 * rectangle from code column and row (column, row) to (endColumn,
 * endRow), keeping up to two different codes a side and skipping cells
 * within DIAGONAL_MIN_DISTANCE of the diagonal. Sides 0 and 1 are those
 * of the diagonal through offsets (0, 0) and (width, height), and sides 2
 * and 3 those of the one through (0, height) and (width, 0). Returns
 * whether sides 0 and 1 both have two codes while side 2 or 3 does not.
 */
s32 func_80405870_hole_editor(s32 column, s32 row, s32 endColumn, s32 endRow) {
    s32 sideCodes[DIAGONAL_SIDE_COUNT][SIDE_CODE_COUNT];
    s32 width;
    s32 height;
    s32 i;
    s32 rowOffset;
    s32 columnOffset;
    s32 code;
    f32 inverseLength;
    f32 distance;
    f32 absDistance;

    void AddSideCode(s32 *codes, s32 code) {
        s32 i;

        for (i = 0; i < SIDE_CODE_COUNT; i++) {
            if (codes[i] == code) {
                return;
            }
            if (codes[i] == NO_TERRAIN_CODE) {
                codes[i] = code;
                return;
            }
        }
    }

    width = endColumn - column;
    height = endRow - row;
    EDITOR_ASSERT(width > 0 && height > 0, HOLE_EDITOR_DIAGONAL_ASSERT_LINE);
    for (i = 0; i < DIAGONAL_SIDE_COUNT; i++) {
        sideCodes[i][0] = NO_TERRAIN_CODE;
        sideCodes[i][1] = NO_TERRAIN_CODE;
    }
    inverseLength = 1.0f / sqrtf(width * width + height * height);
    for (rowOffset = 0; rowOffset < height; rowOffset++) {
        for (columnOffset = 0; columnOffset < width; columnOffset++) {
            code = GetTerrainCode(column + columnOffset, row + rowOffset);
            distance = (width * (rowOffset - height) + columnOffset * height) * inverseLength;
            absDistance = distance < 0.0f ? -distance : distance;
            if (absDistance >= DIAGONAL_MIN_DISTANCE) {
                if (distance >= 0.0f) {
                    AddSideCode(sideCodes[2], code);
                } else {
                    AddSideCode(sideCodes[3], code);
                }
            }
            distance = (-height * columnOffset + rowOffset * width) * inverseLength;
            absDistance = distance < 0.0f ? -distance : distance;
            if (absDistance >= DIAGONAL_MIN_DISTANCE) {
                if (distance >= 0.0f) {
                    AddSideCode(sideCodes[0], code);
                } else {
                    AddSideCode(sideCodes[1], code);
                }
            }
        }
    }
    if (sideCodes[0][1] != NO_TERRAIN_CODE && sideCodes[1][1] != NO_TERRAIN_CODE &&
        (sideCodes[2][1] == NO_TERRAIN_CODE || sideCodes[3][1] == NO_TERRAIN_CODE)) {
        return TRUE;
    }
    return FALSE;
}

/*
 * Toggles the unk992 bit of one cell of the ground. The cells are
 * GROUND_TILE_POINT_SPAN a side in each tile.
 */
void func_80405AF4_hole_editor(s32 column, s32 row) {
    s32 tileIndex;
    s32 cellBits;

    if (column < 0 || column >= GROUND_TILE_COLUMN_COUNT * GROUND_TILE_POINT_SPAN
        || row >= GROUND_TILE_ROW_COUNT * GROUND_TILE_POINT_SPAN || row < 0) {
        return;
    }
    tileIndex = (row >> 2) * GROUND_TILE_COLUMN_COUNT + column / GROUND_TILE_POINT_SPAN;
    cellBits = g_WorkArea.hole.tiles[tileIndex].unk992;
    g_WorkArea.hole.tiles[tileIndex].unk992 = cellBits ^ (1 << ((row & 3) * GROUND_TILE_POINT_SPAN + (column & 3)));
}

/* Sets the x and s of a column of points between grid vertices (column, row) and (column + 1, row) */
void func_80405B80_hole_editor(Vtx *vertices, s32 column, s32 row, s32 weight) {
    Vtx *ends[2];
    s32 firstWeight;
    s32 i;

    firstWeight = EDGE_WEIGHT_MAX - weight;
    ends[0] = GetGridVertex(column, row);
    ends[1] = GetGridVertex(column + 1, row);
    for (i = 0; i < EDGE_POINT_COUNT; i++) {
        Vtx *vertex = &vertices[D_8040C580_hole_editor[i][(column & 1) * 2 + 1]];

        vertex->v.ob[0] = (ends[0]->v.ob[0] * firstWeight + ends[1]->v.ob[0] * weight) / EDGE_WEIGHT_MAX;
        vertex->v.tc[0] = (ends[0]->v.tc[0] * firstWeight + ends[1]->v.tc[0] * weight) / EDGE_WEIGHT_MAX;
    }
}

/* Sets the z and t of a row of points between grid vertices (column, row) and (column, row + 1) */
void func_80405C94_hole_editor(Vtx *vertices, s32 column, s32 row, s32 weight) {
    Vtx *ends[2];
    s32 firstWeight;
    s32 i;

    firstWeight = EDGE_WEIGHT_MAX - weight;
    ends[0] = GetGridVertex(column, row);
    ends[1] = GetGridVertex(column, row + 1);
    for (i = 0; i < EDGE_POINT_COUNT; i++) {
        Vtx *vertex = &vertices[D_8040C580_hole_editor[(row & 1) * 2 + 1][i]];

        vertex->v.ob[2] = (ends[0]->v.ob[2] * firstWeight + ends[1]->v.ob[2] * weight) / EDGE_WEIGHT_MAX;
        vertex->v.tc[1] = (ends[0]->v.tc[1] * firstWeight + ends[1]->v.tc[1] * weight) / EDGE_WEIGHT_MAX;
    }
}

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80405DB0_hole_editor);

/*
 * Sets each ground tile's unk992 bits, one for each of its cells, to
 * func_80405870 of the codes between the cell's opposite corners
 */
void func_80406570_hole_editor(void) {
    s32 tileRow;
    s32 tileColumn;
    s32 cellRow;
    s32 cellColumn;
    s32 tileIndex;
    s32 cellBits;
    Vtx *corners[2];

    for (tileRow = 0; tileRow < GROUND_TILE_ROW_COUNT; tileRow++) {
        for (tileColumn = 0; tileColumn < GROUND_TILE_COLUMN_COUNT; tileColumn++) {
            cellBits = 0;
            tileIndex = tileRow * GROUND_TILE_COLUMN_COUNT + tileColumn;
            for (cellRow = 0; cellRow < GROUND_TILE_POINT_SPAN; cellRow++) {
                for (cellColumn = 0; cellColumn < GROUND_TILE_POINT_SPAN; cellColumn++) {
                    corners[0] = GetGroundPoint(tileColumn * GROUND_TILE_POINT_SPAN + cellColumn,
                                                tileRow * GROUND_TILE_POINT_SPAN + cellRow);
                    corners[1] = GetGroundPoint(tileColumn * GROUND_TILE_POINT_SPAN + cellColumn + 1,
                                                tileRow * GROUND_TILE_POINT_SPAN + cellRow + 1);
                    cellBits |= func_80405870_hole_editor(corners[0]->v.ob[0] / VERTEX_UNITS_PER_CODE,
                                                          corners[0]->v.ob[2] / VERTEX_UNITS_PER_CODE,
                                                          corners[1]->v.ob[0] / VERTEX_UNITS_PER_CODE,
                                                          corners[1]->v.ob[2] / VERTEX_UNITS_PER_CODE)
                                << (cellRow * GROUND_TILE_POINT_SPAN + cellColumn);
                }
            }
            g_WorkArea.hole.tiles[tileIndex].unk992 = cellBits;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_804066C8_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80406818_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80406C3C_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_804077B0_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_804079B8_hole_editor);

/*
 * Fills D_80491A60 with the cube of cos(column * pi/4) * sin(row * pi/4),
 * as matching IA8 intensity and alpha levels from 1 to 15
 */
void func_80407D64_hole_editor(void) {
    s32 row;
    s32 column;
    s32 texelIndex;
    s32 level;

    texelIndex = 0;
    for (row = 0; row < WAVE_TEXTURE_HEIGHT; row++) {
        for (column = 0; column < WAVE_TEXTURE_WIDTH; column++) {
            level = cos((f32)(column * WAVE_TEXEL_STEP) * WAVE_ANGLE_STEP) *
                    sin((f32)(row * WAVE_TEXEL_STEP) * WAVE_ANGLE_STEP) * WAVE_AMPLITUDE;
            level *= level * level;
            level /= 1 << WAVE_LEVEL_SHIFT;
            if (level <= 0) {
                level = WAVE_LEVEL_MIN;
            }
            if (level > WAVE_LEVEL_MAX) {
                level = WAVE_LEVEL_MAX;
            }
            D_80491A60[texelIndex++] = (level << IA8_ALPHA_SHIFT) | level;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80407EC4_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80407F08_hole_editor);

/*
 * Doesn't match: the original opens with an 8-byte frame and stores an unset
 * $v0 into it, and it clears the flag only after loading ob[2].
 */
#ifdef NON_MATCHING
void GetMidpointVertex(Vtx_t *first, Vtx_t *second, Vtx_t *midpoint) {
    s32 unused;

    midpoint->ob[0] = (first->ob[0] + second->ob[0]) / 2;
    midpoint->ob[1] = (first->ob[1] + second->ob[1]) / 2;
    midpoint->flag = 0;
    midpoint->ob[2] = (first->ob[2] + second->ob[2]) / 2;
    midpoint->tc[0] = (first->tc[0] + second->tc[0]) / 2;
    midpoint->tc[1] = (first->tc[1] + second->tc[1]) / 2;
    midpoint->cn[0] = (first->cn[0] + second->cn[0]) / 2;
    midpoint->cn[1] = (first->cn[1] + second->cn[1]) / 2;
    midpoint->cn[2] = (first->cn[2] + second->cn[2]) / 2;
    midpoint->cn[3] = (first->cn[3] + second->cn[3]) / 2;
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", GetMidpointVertex);
#endif

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80408170_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_804083E0_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_8040BF64_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_8040BFC8_hole_editor);

/* Writes the whole frame to the host as the next numbered capture file */
void func_8040C124_hole_editor(void) {
    u16 *frame;
    u8 *capture;
    s32 byteIndex;
    s32 row;
    s32 column;

    frame = (u16 *)((u8 *)nuGfxCfb_ptr + KSEG1_OFFSET);
    capture = AllocMemory(SCREEN_CAPTURE_SIZE);
    for (byteIndex = 0; byteIndex != CAPTURE_HEADER_SIZE; byteIndex++) {
        capture[byteIndex] = D_8040C710_hole_editor[byteIndex];
    }
    for (row = SCREEN_HEIGHT - 1; row != -1; row--) {
        for (column = 0; column != SCREEN_WIDTH; column++) {
            capture[byteIndex++] = ((frame[row * SCREEN_WIDTH + column] >> RGBA16_BLUE_SHIFT) & RGBA16_CHANNEL_MASK)
                                    << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] = ((frame[row * SCREEN_WIDTH + column] >> RGBA16_GREEN_SHIFT) & RGBA16_CHANNEL_MASK)
                                    << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] = ((frame[row * SCREEN_WIDTH + column] >> RGBA16_RED_SHIFT) & RGBA16_CHANNEL_MASK)
                                    << RGBA16_CHANNEL_WIDEN_SHIFT;
        }
    }
    sprintf(g_PrintBuffer, "x:/usgolf64/capture/cap%02d.bmp", sScreenCaptureNumber);
    WriteHostFile(g_PrintBuffer, SCREEN_CAPTURE_SIZE);
    FreeMemory(&capture);
    sScreenCaptureNumber++;
}

/*
 * Writes the frame's top-left corner to the host as capture file
 * captureIndex. The size written is that of a whole-screen capture.
 */
void func_8040C260_hole_editor(s32 captureIndex) {
    u16 *frame;
    u8 *capture;
    s32 byteIndex;
    s32 row;
    s32 column;

    capture = g_WorkArea.bytes;
    frame = (u16 *)((u8 *)nuGfxCfb_ptr + KSEG1_OFFSET);
    for (byteIndex = 0; byteIndex != CAPTURE_HEADER_SIZE; byteIndex++) {
        capture[byteIndex] = D_8040C748_hole_editor[byteIndex];
    }
    /* BMP rows run from the bottom up, and each pixel is blue, green, red */
    for (row = CORNER_CAPTURE_HEIGHT - 1; row != -1; row--) {
        for (column = 0; column != CORNER_CAPTURE_WIDTH; column++) {
            capture[byteIndex++] = ((frame[row * SCREEN_WIDTH + column] >> RGBA16_BLUE_SHIFT) & RGBA16_CHANNEL_MASK)
                                    << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] = ((frame[row * SCREEN_WIDTH + column] >> RGBA16_GREEN_SHIFT) & RGBA16_CHANNEL_MASK)
                                    << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] = ((frame[row * SCREEN_WIDTH + column] >> RGBA16_RED_SHIFT) & RGBA16_CHANNEL_MASK)
                                    << RGBA16_CHANNEL_WIDEN_SHIFT;
        }
    }
    sprintf(g_PrintBuffer, "c:/golf/capture/temp%02d.bmp", captureIndex);
    WriteHostFile(g_PrintBuffer, SCREEN_CAPTURE_SIZE);
}

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_8040C360_hole_editor);
