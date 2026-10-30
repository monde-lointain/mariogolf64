#ifndef CURVE_H
#define CURVE_H

#include "common.h"

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

void DrawNothing(Gfx** displayListHead);
s32 StepAlongPath(f32 points[][AXIS_COUNT], s32 pointCount, s32 pace,
    f32 position[AXIS_COUNT]);

#endif
