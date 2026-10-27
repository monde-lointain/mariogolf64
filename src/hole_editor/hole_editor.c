#include "common.h"
#include "host_file.h"
#include "screen.h"
#include "terrain.h"
#include "variables.h"
#include "work_area.h"

/* The points along a tile's edge, its corners included */
#define EDGE_POINT_COUNT (GROUND_TILE_POINT_SPAN + 1)
/* An interpolation weight runs from 0, all the first end, to this, all the second */
#define EDGE_WEIGHT_MAX 16

/* The screen capture, a 24-bit BMP of the frame's top-left corner */
#define CAPTURE_HEADER_SIZE 54
#define CAPTURE_WIDTH 64
#define CAPTURE_HEIGHT 128
#define CAPTURE_BYTES_PER_PIXEL 3
/* Where the 5-bit channels sit in an RGBA16 pixel, and the shift that widens one to 8 bits */
#define RGBA16_RED_SHIFT 11
#define RGBA16_GREEN_SHIFT 6
#define RGBA16_BLUE_SHIFT 1
#define RGBA16_CHANNEL_MASK 0x1F
#define RGBA16_CHANNEL_WIDEN_SHIFT 3
/* Adds to a cached KSEG0 address to read the same memory uncached through KSEG1 */
#define KSEG1_OFFSET 0x20000000

extern u8 D_80451A60[];
extern u8 D_8040C580_hole_editor[EDGE_POINT_COUNT][EDGE_POINT_COUNT];
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

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_8040582C_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80405870_hole_editor);

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

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80406570_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_804066C8_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80406818_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80406C3C_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_804077B0_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_804079B8_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80407D64_hole_editor);

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

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_8040C124_hole_editor);

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
    for (row = CAPTURE_HEIGHT - 1; row != -1; row--) {
        for (column = 0; column != CAPTURE_WIDTH; column++) {
            capture[byteIndex++] = ((frame[row * SCREEN_WIDTH + column] >> RGBA16_BLUE_SHIFT) & RGBA16_CHANNEL_MASK)
                                    << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] = ((frame[row * SCREEN_WIDTH + column] >> RGBA16_GREEN_SHIFT) & RGBA16_CHANNEL_MASK)
                                    << RGBA16_CHANNEL_WIDEN_SHIFT;
            capture[byteIndex++] = ((frame[row * SCREEN_WIDTH + column] >> RGBA16_RED_SHIFT) & RGBA16_CHANNEL_MASK)
                                    << RGBA16_CHANNEL_WIDEN_SHIFT;
        }
    }
    sprintf(g_PrintBuffer, "c:/golf/capture/temp%02d.bmp", captureIndex);
    WriteHostFile(g_PrintBuffer, CAPTURE_HEADER_SIZE + SCREEN_WIDTH * SCREEN_HEIGHT * CAPTURE_BYTES_PER_PIXEL);
}

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_8040C360_hole_editor);
