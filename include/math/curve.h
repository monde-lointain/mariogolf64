#ifndef CURVE_H
#define CURVE_H

#include <ultra64.h>

#define PATH_END_REACHED 1
#define PathPace(catchUp, stepsPerLine) \
    ((catchUp) * ((stepsPerLine) < 10 ? 10 : 100) + (stepsPerLine))

typedef struct PathPoint {
    f32 x;
    f32 y;
    f32 z;
} PathPoint;

void DrawNothing(Gfx **displayListHead);
s32 StepAlongPath(PathPoint *points, s32 pointCount, s32 pace, PathPoint *position);

f32 InterpolateHermite(f32 values[], f32 position);
void ResetPath(void);

#endif
