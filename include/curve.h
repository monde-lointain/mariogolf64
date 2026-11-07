#ifndef CURVE_H
#define CURVE_H

#include <ultra64.h>

/* Periodic splines, Hermite interpolation and stepping along paths */

/* What StepAlongPath returns once the position reaches the path's last point */
#define PATH_END_REACHED 1
/*
 * A pace for StepAlongPath, whose position covers catchUp / stepsPerLine of
 * its remaining distance each step and whose target takes stepsPerLine steps
 * through each line: the decimal digits of catchUp, then of stepsPerLine
 */
#define PathPace(catchUp, stepsPerLine) \
    ((catchUp) * ((stepsPerLine) < 10 ? 10 : 100) + (stepsPerLine))

/* A point of a path, or the position that StepAlongPath moves along one */
typedef struct PathPoint {
    /* 0x0 */ f32 x;
    /* 0x4 */ f32 y;
    /* 0x8 */ f32 z;
} PathPoint;

void DrawNothing(Gfx **displayListHead);
s32 StepAlongPath(PathPoint *points, s32 pointCount, s32 pace, PathPoint *position);

f32 InterpolateHermite(f32 values[], f32 position);
void ResetPath(void);

#endif
