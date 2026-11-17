/*
 * Tests of the colors that DeriveSceneColors reads past the end of
 * g_SkyColors, against the N64's bytes worked out by hand.
 *
 * Each half of a vertex is its index in the high byte and its place in the
 * low one: 0x10 for ob[0] through 0x60 for tc[1]. The tile grid's indices
 * start at 0x80, so that its bytes differ from the sky's. Each vertex's
 * colors are 0x80, 0x90, 0xA0 and 0xB0, plus its index.
 */

#include <ultra64.h>

#include "screen/sky_colors_overrun.h"
#include "system/c_language.h"
#include "test_support.h"

#define SKY_VERTEX_TOTAL 40
#define TILE_GRID_VERTEX_TOTAL 96
#define TILE_GRID_INDEX_BASE 0x80
// A byte that no read gives, to show what was left unwritten
#define UNWRITTEN 0xEE

static SkyColors f_Table[COURSE_RECORD_COUNT][SKY_WEATHER_COUNT * SKY_TIME_COUNT];
static Vtx f_SkyVertices[SKY_VERTEX_TOTAL];
static Vtx f_TileGridVertices[TILE_GRID_VERTEX_TOTAL];
static const u8 f_FlareColor[RGB_CHANNEL_COUNT] = {0xA1, 0xB2, 0xC3};

static void FillVertex(Vtx *vertex, u32 index) {
    s32 channel;

    vertex->v.ob[0] = (s16)((index << 8) | 0x10);
    vertex->v.ob[1] = (s16)((index << 8) | 0x20);
    vertex->v.ob[2] = (s16)((index << 8) | 0x30);
    vertex->v.flag = (u16)((index << 8) | 0x40);
    vertex->v.tc[0] = (s16)((index << 8) | 0x50);
    vertex->v.tc[1] = (s16)((index << 8) | 0x60);
    for (channel = 0; channel < 4; channel++) {
        vertex->v.cn[channel] = (u8)(0x80 + (channel * 0x10) + index);
    }
}

// Neighbors whose glare has settled at pi, 0x40490FDB as bits
static SkyColorsNeighbors MakeNeighbors(void) {
    SkyColorsNeighbors neighbors;
    u32 index;

    for (index = 0; index < SKY_VERTEX_TOTAL; index++) {
        FillVertex(&f_SkyVertices[index], index);
    }

    for (index = 0; index < TILE_GRID_VERTEX_TOTAL; index++) {
        FillVertex(&f_TileGridVertices[index], TILE_GRID_INDEX_BASE + index);
    }

    neighbors.isGlareSettled = TRUE;
    neighbors.glareStrength = 3.14159274f;
    neighbors.firstFlareColor = f_FlareColor;
    neighbors.skyVertices = f_SkyVertices;
    neighbors.tileGridVertices = f_TileGridVertices;
    return neighbors;
}

static int HasColor(const u8 *color, u8 red, u8 green, u8 blue) {
    return (color[0] == red) && (color[1] == green) && (color[2] == blue);
}

// Finds the colors at a row that the compiler can't know, into a past-table
// entry filled with UNWRITTEN
static SkyColors *FindColors(u32 row, SkyColors *pastTable) {
    SkyColorsNeighbors neighbors = MakeNeighbors();
    volatile u32 volatileRow = row;

    memset(pastTable, UNWRITTEN, sizeof(*pastTable));
    return FindSceneSkyColors(f_Table, volatileRow, &neighbors, pastTable);
}

// The table's rows, up to the last course's, are its own
static void TestKeepsTheTablesRows(void) {
    SkyColors pastTable;

    EXPECT(FindColors(0, &pastTable) == f_Table[0]);
    EXPECT(FindColors(COURSE_RECORD_COUNT - 1, &pastTable) == f_Table[COURSE_RECORD_COUNT - 1]);
}

// The first row past the end starts 4 bytes before f_IsSunGlareSettled, so
// its top color is that word's first three bytes, 0, 0, 0. The horizon
// color is its last byte, then f_SunGlareStrength's first two; the noise
// color the strength's last two, then the first flare's red.
static void TestReadsTheGlareFirst(void) {
    SkyColors pastTable;
    const SkyColors *colors = FindColors(COURSE_RECORD_COUNT, &pastTable);

    EXPECT(colors == &pastTable);
    EXPECT(HasColor(colors->topColor, 0x00, 0x00, 0x00));
    EXPECT(HasColor(colors->horizonColor, 0x01, 0x40, 0x49));
    EXPECT(HasColor(colors->noiseColor, 0x0F, 0xDB, 0xA1));
}

// The next three rows start 0x58 bytes into f_SkyVertices, then 0x100 and
// 0x200 bytes further: in vertices 5, 21 and 37, at tc, then the colors,
// then the next vertex's ob[0]
static void TestReadsTheSkyVertices(void) {
    SkyColors pastTable;
    const SkyColors *colors = FindColors(COURSE_RECORD_COUNT + 1, &pastTable);

    EXPECT(HasColor(colors->topColor, 0x05, 0x50, 0x05));
    EXPECT(HasColor(colors->horizonColor, 0x60, 0x85, 0x95));
    EXPECT(HasColor(colors->noiseColor, 0xA5, 0xB5, 0x06));

    colors = FindColors(COURSE_RECORD_COUNT + 2, &pastTable);
    EXPECT(HasColor(colors->topColor, 0x15, 0x50, 0x15));
    EXPECT(HasColor(colors->horizonColor, 0x60, 0x95, 0xA5));
    EXPECT(HasColor(colors->noiseColor, 0xB5, 0xC5, 0x16));

    colors = FindColors(COURSE_RECORD_COUNT + 3, &pastTable);
    EXPECT(HasColor(colors->topColor, 0x25, 0x50, 0x25));
    EXPECT(HasColor(colors->horizonColor, 0x60, 0xA5, 0xB5));
    EXPECT(HasColor(colors->noiseColor, 0xC5, 0xD5, 0x26));
}

// The last two rows, the rainy sunset's and sudden death's, start 0x20 and
// 0x120 bytes into f_TileGridVertices: vertices 2 and 18, from ob[0] to
// tc[0]'s first byte
static void TestReadsTheTileGridVertices(void) {
    SkyColors pastTable;
    const SkyColors *colors = FindColors(COURSE_RECORD_COUNT + 4, &pastTable);

    EXPECT(HasColor(colors->topColor, 0x82, 0x10, 0x82));
    EXPECT(HasColor(colors->horizonColor, 0x20, 0x82, 0x30));
    EXPECT(HasColor(colors->noiseColor, 0x82, 0x40, 0x82));

    colors = FindColors(COURSE_RECORD_COUNT + 5, &pastTable);
    EXPECT(HasColor(colors->topColor, 0x92, 0x10, 0x92));
    EXPECT(HasColor(colors->horizonColor, 0x20, 0x92, 0x30));
    EXPECT(HasColor(colors->noiseColor, 0x92, 0x40, 0x92));
}

// Only the colors are read; the rest of the entry is left as it was
static void TestWritesOnlyTheColors(void) {
    SkyColors pastTable;

    FindColors(COURSE_RECORD_COUNT, &pastTable);
    EXPECT(pastTable.backgroundColor[0] == UNWRITTEN);
    EXPECT(pastTable.fogColor[0] == UNWRITTEN);
}

int main(void) {
    TestKeepsTheTablesRows();
    TestReadsTheGlareFirst();
    TestReadsTheSkyVertices();
    TestReadsTheTileGridVertices();
    TestWritesOnlyTheColors();
    return ReportTestResults("sky_colors_overrun_test");
}
