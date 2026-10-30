#ifndef CURVE_H
#define CURVE_H

#include "common.h"

/* Periodic splines, Hermite interpolation and stepping along paths */

void DrawNothing(Gfx** displayListHead);
s32 StepAlongPath(f32 points[][AXIS_COUNT], s32 pointCount, s32 pace,
    f32 position[AXIS_COUNT]);

#endif
