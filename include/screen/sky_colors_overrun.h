/*
 * What DeriveSceneColors reads past the end of g_SkyColors. Its index steps
 * through the table a course's row at a time, by scene color and rain, so
 * the later scene colors, and the rainy ones from the driving range's on,
 * pass the last course's row. The N64 reads the sun glare's state there,
 * then f_SkyVertices and f_TileGridVertices, which follow the table in its
 * memory. These read the same bytes, in the N64's order and layout, from
 * those objects' current values.
 */

#ifndef SKY_COLORS_OVERRUN_H
#define SKY_COLORS_OVERRUN_H

#include <string.h>
#include <ultra64.h>

#include "course/course.h"
#include "screen/sky_and_hud_animations.h"

// The N64's SkyColors, with its 4-byte sun pointer, and a course's row of
// them
#define N64_SKY_COLORS_SIZE 0x20
#define N64_SKY_COLORS_ROW_SIZE (N64_SKY_COLORS_SIZE * SKY_WEATHER_COUNT * SKY_TIME_COUNT)
// Where the colors that DeriveSceneColors reads lie in the N64's SkyColors
#define N64_SKY_TOP_COLOR_OFFSET 4
#define N64_SKY_HORIZON_COLOR_OFFSET 7
#define N64_SKY_NOISE_COLOR_OFFSET 0xA
// Where the objects that the reads reach lie, in bytes past the table's
// end, as the N64's symbols place them
#define AFTER_SKY_COLORS_GLARE_SETTLED 0x4 // f_IsSunGlareSettled, an s32
#define AFTER_SKY_COLORS_GLARE_STRENGTH 0x8 // f_SunGlareStrength, an f32
#define AFTER_SKY_COLORS_LENS_FLARES 0xC // f_LensFlares, its first color first
#define AFTER_SKY_COLORS_SKY_VERTICES 0xAC
#define AFTER_SKY_COLORS_TILE_GRID_VERTICES 0x3E4
// The bytes of a word, and of a Vtx up to its color
#define N64_WORD_SIZE 4
#define N64_VERTEX_SIZE 16
#define N64_VERTEX_COLOR_OFFSET 12
// The vertices' halves: ob's three, flag, then tc's two
#define N64_VERTEX_FLAG_HALF 3
#define N64_VERTEX_TC_HALF 4

// The objects past the table, as DeriveSceneColors finds them
typedef struct SkyColorsNeighbors {
    s32 isGlareSettled;
    f32 glareStrength;
    const u8 *firstFlareColor;
    const void *skyVertices; // Each array whole
    const void *tileGridVertices;
} SkyColorsNeighbors;

// The byte of a big-endian word
static inline u8 N64WordByte(u32 word, u32 offset) {
    return (u8)(word >> ((N64_WORD_SIZE - 1 - offset) * 8));
}

// The byte at an offset into an array of vertices, as the N64 stores
// them. The array is read as a whole, through its bytes, as the reads cross
// the rows of f_SkyVertices.
static inline u8 N64VertexByte(const void *vertices, u32 offset) {
    Vtx copy;
    const Vtx_t *vertex = &copy.v;
    u32 within = offset % N64_VERTEX_SIZE;
    u16 half;

    memcpy(&copy, (const u8 *)vertices + ((offset / N64_VERTEX_SIZE) * sizeof(Vtx)), sizeof(Vtx));

    if (within >= N64_VERTEX_COLOR_OFFSET) {
        return vertex->cn[within - N64_VERTEX_COLOR_OFFSET];
    }

    if (within / 2 < N64_VERTEX_FLAG_HALF) {
        half = (u16)vertex->ob[within / 2];
    }
    else if (within / 2 == N64_VERTEX_FLAG_HALF) {
        half = vertex->flag;
    }
    else {
        half = (u16)vertex->tc[(within / 2) - N64_VERTEX_TC_HALF];
    }

    return (within % 2 == 0) ? (u8)(half >> 8) : (u8)half;
}

// The byte at an offset past the table's end. The reads start 4 bytes in,
// at f_IsSunGlareSettled, and reach only the first flare's red before
// f_SkyVertices, and only vertices within it and f_TileGridVertices.
static inline u8 ReadN64ByteAfterSkyColors(const SkyColorsNeighbors *neighbors, u32 offset) {
    union {
        f32 value;
        u32 bits;
    } strength;

    if (offset >= AFTER_SKY_COLORS_TILE_GRID_VERTICES) {
        return N64VertexByte(
            neighbors->tileGridVertices,
            offset - AFTER_SKY_COLORS_TILE_GRID_VERTICES
        );
    }

    if (offset >= AFTER_SKY_COLORS_SKY_VERTICES) {
        return N64VertexByte(neighbors->skyVertices, offset - AFTER_SKY_COLORS_SKY_VERTICES);
    }

    if (offset >= AFTER_SKY_COLORS_LENS_FLARES) {
        return neighbors->firstFlareColor[offset - AFTER_SKY_COLORS_LENS_FLARES];
    }

    if (offset >= AFTER_SKY_COLORS_GLARE_STRENGTH) {
        strength.value = neighbors->glareStrength;
        return N64WordByte(strength.bits, offset - AFTER_SKY_COLORS_GLARE_STRENGTH);
    }

    return N64WordByte((u32)neighbors->isGlareSettled, offset - AFTER_SKY_COLORS_GLARE_SETTLED);
}

// Reads a color of the SkyColors that starts at an offset past the table
static inline void ReadN64SkyColor(
    const SkyColorsNeighbors *neighbors,
    u32 colorOffset,
    u8 color[RGB_CHANNEL_COUNT]
) {
    s32 channel;

    for (channel = 0; channel < RGB_CHANNEL_COUNT; channel++) {
        color[channel] = ReadN64ByteAfterSkyColors(neighbors, colorOffset + channel);
    }
}

// The colors at a row of the table, which past its end are those that
// the N64 reads there, in pastTable. The row is at most 5 past the end: 3
// for the rain, plus SCENE_COLOR_INDEX_SUDDEN_DEATH.
static inline SkyColors *FindSceneSkyColors(
    SkyColors table[][SKY_WEATHER_COUNT * SKY_TIME_COUNT],
    u32 row,
    const SkyColorsNeighbors *neighbors,
    SkyColors *pastTable
) {
    u32 rowOffset;

    if (row < COURSE_RECORD_COUNT) {
        return table[row];
    }

    rowOffset = (row - COURSE_RECORD_COUNT) * N64_SKY_COLORS_ROW_SIZE;
    ReadN64SkyColor(neighbors, rowOffset + N64_SKY_TOP_COLOR_OFFSET, pastTable->topColor);
    ReadN64SkyColor(neighbors, rowOffset + N64_SKY_HORIZON_COLOR_OFFSET, pastTable->horizonColor);
    ReadN64SkyColor(neighbors, rowOffset + N64_SKY_NOISE_COLOR_OFFSET, pastTable->noiseColor);
    return pastTable;
}

// Finds the objects past the table, in the N64's memory
void GetSkyColorsNeighbors(SkyColorsNeighbors *neighbors);

#endif
