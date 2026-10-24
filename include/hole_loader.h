#ifndef HOLE_LOADER_H
#define HOLE_LOADER_H

#include <ultra64.h>
#include "common.h"

#define CUP_POSITION_COUNT 4
/* Points that the computer golfer may route its shots through */
#define ROUTE_POINT_COUNT 8

typedef struct HolePoints {
    /* 0x00 */ CoursePoint cupPositions[CUP_POSITION_COUNT]; /* Zero where a hole has fewer */
    /* 0x30 */ CoursePoint unk30[CUP_POSITION_COUNT];
    /* 0x60 */ CoursePoint unk60[8];
    /* 0xC0 */ CoursePoint routePoints[ROUTE_POINT_COUNT]; /* For the computer golfer; zero where unused */
} HolePoints; /* size = 0x120 */

extern HolePoints g_HolePoints;

/* Cleared while the loader thread loads a hole; set once it is done or stopped */
extern s32 g_HasLoaderFinished;

void ResetLoaderThread(void);
void StartLoaderThread(void);
void StopLoaderThread(void);

/* Decompress the size bytes at romAddress into destination, returning the decompressed size */
s32 LoadLzData(u32 romAddress, void *destination, u32 size);
s32 LoadPatchedLzData(u32 romAddress, void *destination, u32 size);

#endif
