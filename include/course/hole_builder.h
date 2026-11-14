/*
 * Building a hole's ground, display lists and billboards from its files.
 */

#ifndef HOLE_BUILDER_H
#define HOLE_BUILDER_H

#include <ultra64.h>

#include "course/course.h"
#include "math/geometry.h"

// The host's hole files are numbered by course from 1 to this
#define HOLE_FILE_COURSE_COUNT 9

// The units per grid step of the distances that ListOffsetsByDistance lists
#define OFFSET_DISTANCE_SCALE 256

// A column and row offset from a point, with its distance
typedef struct GridOffset {
    s8 column;
    s8 row;
    u16 distance;
} GridOffset;

// The stack of the thread that loads a hole, which runs the hole builder, so
// the builder's overlay holds it
#define LOADER_THREAD_STACK_SIZE 0x8000
extern u8 g_LoaderThreadStack[LOADER_THREAD_STACK_SIZE];

// How ShrinkMiniGolfHole scales a course down to mini-golf size, by axis
extern Vec3 g_MiniGolfScale;
s32 ListOffsetsByDistance(s32 span, GridOffset *offsets, s32 countMax);
f32 MeasureGroundTiles(void);
void BuildHole(CourseRecordIndex courseRecordIndex, s32 holeIndex);
void SaveHoleFiles(s32 courseNumber, s32 holeNumber);
void ToggleGroundTileBorders(void);
// shadeVariant is 2 at a round's start, 0 at its end and 1 for the driving
// range, but the original ignores it
void ShadeGround(s32 shadeVariant);

void BuildGroundDisplayLists(void);
void LoadToolCodeMap(void);
void ToggleCodeMapCleanup(void);

#endif
