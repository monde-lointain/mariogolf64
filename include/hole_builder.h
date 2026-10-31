#ifndef HOLE_BUILDER_H
#define HOLE_BUILDER_H

#include <ultra64.h>

/* The host's hole files are numbered by course from 1 to this */
#define HOLE_FILE_COURSE_COUNT 9

/* The units per grid step of the distances that ListOffsetsByDistance lists */
#define OFFSET_DISTANCE_SCALE 256

/* A column and row offset from a point, with its distance */
typedef struct GridOffset {
  /* 0x0 */ s8 column;
  /* 0x1 */ s8 row;
  /* 0x2 */ u16 distance;
} GridOffset; /* size = 0x4 */

/*
 * The stack of the thread that loads a hole, which runs the hole builder, so
 * the builder's overlay holds it
 */
#define LOADER_THREAD_STACK_SIZE 0x8000
extern u8 g_LoaderThreadStack[LOADER_THREAD_STACK_SIZE];

s32 ListOffsetsByDistance(s32 span, GridOffset* offsets, s32 maxCount);
f32 MeasureGroundTiles(void);
void BuildHole(s32 course, s32 hole);
void SaveHoleFiles(s32 course, s32 hole);
void ToggleGroundTileBorders(void);
/*
 * shadeVariant is 2 at a round's start, 0 at its end and 1 for the driving
 * range, but the original ignores it
 */
void ShadeGround(s32 shadeVariant);

#endif
