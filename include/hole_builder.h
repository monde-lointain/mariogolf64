#ifndef HOLE_BUILDER_H
#define HOLE_BUILDER_H

#include <ultra64.h>

#include "course.h"
#include "geometry.h"

#define HOLE_FILE_COURSE_COUNT 9

#define OFFSET_DISTANCE_SCALE 256

typedef struct GridOffset {
    s8 column;
    s8 row;
    u16 distance;
} GridOffset;

#define LOADER_THREAD_STACK_SIZE 0x8000
extern u8 g_LoaderThreadStack[LOADER_THREAD_STACK_SIZE];
extern Vec3 g_MiniGolfScale;

s32 ListOffsetsByDistance(s32 span, GridOffset *offsets, s32 countMax);
f32 MeasureGroundTiles(void);
void BuildHole(CourseRecordIndex courseRecordIndex, s32 holeIndex);
void SaveHoleFiles(s32 courseNumber, s32 holeNumber);
void ToggleGroundTileBorders(void);
void ShadeGround(s32 shadeVariant);

void BuildGroundDisplayLists(void);
void LoadToolCodeMap(void);
void ToggleCodeMapCleanup(void);

#endif
