/*
 * Curves: periodic splines, Hermite interpolation, and stepping along a path of
 * points at a pace.
 */

#include "math/curve.h"

#include <ultra64.h>

#include "math/geometry.h"
#include "system/compiler.h"

// The splines have this many segments, between one more knots
#define SPLINE_SEGMENT_COUNT 10

static s32 f_PathPointIndex = 0;
static s32 f_PathStepIndex = 0;
/* clang-format off */
static f32 f_UnusedFloatsAfterPathStep[33] UNUSED = {
     10.0f,  20.0f,  30.0f,  50.0f,  60.0f,  70.0f,  90.0f, 100.0f, 120.0f, 130.0f, 150.0f,
     30.0f,  50.0f,  60.0f,  80.0f,  90.0f, 120.0f, 150.0f, 190.0f, 100.0f,  50.0f,  30.0f,
      3.0f,   5.0f,   6.0f,   8.0f,   9.0f,  12.0f,  15.0f,  19.0f,  10.0f,   5.0f,   3.0f,
};
/* clang-format on */
static f32 f_PathPosition[AXIS_COUNT];

// The line the target runs along
typedef struct PathSegment {
    Vec3 start;
    Vec3 end;
} PathSegment;

static PathSegment f_PathSegment;
static f32 f_PathProgress;
static f32 f_PathProgressStep;
static f32 f_PathStepsPerSegment;
static f32 f_PathCatchUp;

// Starts the next StepAlongPath call at the first point of its path
void ResetPath(void) {
    f_PathStepIndex = 0;
    f_PathPointIndex = 0;
}

// Moves position toward a target that runs along the lines between the
// points, from the second point to the last, and returns PATH_END_REACHED
// once it has reached the last point. Starting from the first point, the target
// takes one step per call through each line, and the position covers
// a fraction of its remaining distance each step, in whole units, as pace
// sets them (see PathPace); other paces keep the last path's.
//
// The points are structures, as the original's must have been: GCC takes a
// store through a plain f32 pointer, at no offset, to alias any global, and
// would hold the loads after position's first copy back behind it.
s32 StepAlongPath(PathPoint *points, s32 pointCount, s32 pace, PathPoint *position) {
    s32 nextPointIndex;
    f32 stepsPerSegment;
    f32 progress;
    f32 catchUpFraction;

    if (f_PathPointIndex >= pointCount - 1) {
        position->x = points[pointCount - 1].x;
        position->y = points[pointCount - 1].y;
        f_PathStepIndex = 0;
        f_PathPointIndex = 0;
        position->z = points[pointCount - 1].z;
        return PATH_END_REACHED;
    }

    if (f_PathPointIndex == 0 && f_PathStepIndex == 0) {
        switch (pace) {
            case PathPace(1, 2):
                f_PathCatchUp = 1.0f;
                f_PathStepsPerSegment = 2.0f;
                break;
            case PathPace(1, 4):
                f_PathCatchUp = 1.0f;
                f_PathStepsPerSegment = 4.0f;
                break;
            case PathPace(1, 8):
                f_PathCatchUp = 1.0f;
                f_PathStepsPerSegment = 8.0f;
                break;
            case PathPace(2, 4):
                f_PathCatchUp = 2.0f;
                f_PathStepsPerSegment = 4.0f;
                break;
            case PathPace(2, 8):
                f_PathCatchUp = 2.0f;
                f_PathStepsPerSegment = 8.0f;
                break;
            case PathPace(2, 16):
                f_PathCatchUp = 2.0f;
                f_PathStepsPerSegment = 16.0f;
                break;
            case PathPace(3, 4):
                f_PathCatchUp = 3.0f;
                f_PathStepsPerSegment = 4.0f;
                break;
            case PathPace(4, 8):
                f_PathCatchUp = 4.0f;
                f_PathStepsPerSegment = 8.0f;
                break;
            case PathPace(4, 16):
                f_PathCatchUp = 4.0f;
                f_PathStepsPerSegment = 16.0f;
                break;
        }

        f_PathPosition[AXIS_X] = points[0].x;
        f_PathPosition[AXIS_Y] = points[0].y;
        f_PathPointIndex = 1;
        f_PathPosition[AXIS_Z] = points[0].z;
        f_PathProgressStep = 1.0f / f_PathStepsPerSegment;
    }

    if (f_PathStepIndex == 0) {
        f_PathSegment.start[AXIS_X] = points[f_PathPointIndex].x;
        f_PathSegment.start[AXIS_Y] = points[f_PathPointIndex].y;
        f_PathSegment.start[AXIS_Z] = points[f_PathPointIndex].z;
        f_PathSegment.end[AXIS_X] = points[f_PathPointIndex + 1].x;
        f_PathSegment.end[AXIS_Y] = points[f_PathPointIndex + 1].y;
        f_PathProgress = 0.0f;
        f_PathSegment.end[AXIS_Z] = points[f_PathPointIndex + 1].z;
    }

    stepsPerSegment = f_PathStepsPerSegment;
    catchUpFraction = (1.0f / stepsPerSegment) * f_PathCatchUp;
    progress = f_PathProgress;
    f_PathPosition[AXIS_X] +=
        (s32)((f_PathSegment.start[AXIS_X] +
                  ((f_PathSegment.end[AXIS_X] - f_PathSegment.start[AXIS_X]) * progress) -
                  f_PathPosition[AXIS_X]) *
            catchUpFraction);
    f_PathPosition[AXIS_Y] +=
        (s32)((f_PathSegment.start[AXIS_Y] +
                  ((f_PathSegment.end[AXIS_Y] - f_PathSegment.start[AXIS_Y]) * progress) -
                  f_PathPosition[AXIS_Y]) *
            catchUpFraction);
    f_PathPosition[AXIS_Z] +=
        (s32)((f_PathSegment.start[AXIS_Z] +
                  ((f_PathSegment.end[AXIS_Z] - f_PathSegment.start[AXIS_Z]) * progress) -
                  f_PathPosition[AXIS_Z]) *
            catchUpFraction);
    position->x = f_PathPosition[AXIS_X];
    f_PathProgress = progress + f_PathProgressStep;
    f_PathStepIndex++;
    position->y = f_PathPosition[AXIS_Y];
    position->z = f_PathPosition[AXIS_Z];
    if (stepsPerSegment <= f_PathStepIndex) {
        nextPointIndex = f_PathPointIndex + 1;
        f_PathSegment.start[AXIS_X] = points[nextPointIndex].x;
        f_PathSegment.start[AXIS_Y] = points[nextPointIndex].y;
        f_PathSegment.start[AXIS_Z] = points[nextPointIndex].z;
        f_PathSegment.end[AXIS_X] = points[nextPointIndex + 1].x;
        f_PathSegment.end[AXIS_Y] = points[nextPointIndex + 1].y;
        f_PathSegment.end[AXIS_Z] = points[nextPointIndex + 1].z;
        f_PathStepIndex = 0;
        f_PathPointIndex = nextPointIndex;
    }

    // At most 0 here, since f_PathPointIndex is at least 1
    return 1 - f_PathPointIndex;
}

// Fills coefficients for EvaluatePeriodicSpline with the curve through
// the knots that repeats with the period from the first knot to the
// last
void InitPeriodicSpline(f32 knotX[], f32 knotY[], f32 coefficients[]) {
    s32 index;
    f32 ratio;
    f32 widths[SPLINE_SEGMENT_COUNT + 1];
    f32 diagonal[SPLINE_SEGMENT_COUNT + 1];
    f32 slopes[SPLINE_SEGMENT_COUNT + 1];

    for (index = 0; index != SPLINE_SEGMENT_COUNT; index++) {
        widths[index] = knotX[index + 1] - knotX[index];
        slopes[index] = (knotY[index + 1] - knotY[index]) / widths[index];
    }

    slopes[SPLINE_SEGMENT_COUNT] = slopes[0];
    for (index = 1; index != SPLINE_SEGMENT_COUNT; index++) {
        diagonal[index] = 2.0f * (knotX[index + 1] - knotX[index - 1]);
    }

    diagonal[SPLINE_SEGMENT_COUNT] = 2.0f * (widths[SPLINE_SEGMENT_COUNT - 1] + widths[0]);
    // This leaves coefficients[SPLINE_SEGMENT_COUNT] as it was, though it is read
    // below
    for (index = 1; index != SPLINE_SEGMENT_COUNT; index++) {
        coefficients[index] = slopes[index] - slopes[index - 1];
    }

    // slopes is reused from here as the column that the period adds, to match:
    // an array of its own grows the frame, and gives 30 words differing
    // (none in shape) declared after slopes, or 212 declared first
    slopes[1] = widths[0];
    slopes[SPLINE_SEGMENT_COUNT - 1] = widths[SPLINE_SEGMENT_COUNT - 1];
    slopes[SPLINE_SEGMENT_COUNT] = diagonal[SPLINE_SEGMENT_COUNT];
    for (index = 2; index != SPLINE_SEGMENT_COUNT - 1; index++) {
        slopes[index] = 0.0f;
    }

    for (index = 1; index != SPLINE_SEGMENT_COUNT; index++) {
        ratio = widths[index] / diagonal[index];
        coefficients[index + 1] -= coefficients[index] * ratio;
        diagonal[index + 1] -= widths[index] * ratio;
        slopes[index + 1] -= slopes[index] * ratio;
    }

    slopes[0] = slopes[SPLINE_SEGMENT_COUNT];
    coefficients[0] = coefficients[SPLINE_SEGMENT_COUNT];
    for (index = SPLINE_SEGMENT_COUNT - 2; index >= 0; index--) {
        ratio = widths[index] / diagonal[index + 1];
        coefficients[index] -= coefficients[index + 1] * ratio;
        slopes[index] -= slopes[index + 1] * ratio;
    }

    ratio = coefficients[0] / slopes[0];
    coefficients[0] = ratio;
    coefficients[SPLINE_SEGMENT_COUNT] = ratio;
    for (index = 1; index != SPLINE_SEGMENT_COUNT; index++) {
        coefficients[index] = (coefficients[index] - (slopes[index] * ratio)) / diagonal[index];
    }
}

// Returns the height at x of the spline that InitPeriodicSpline set up
f32 EvaluatePeriodicSpline(f32 x, f32 knotX[], f32 knotY[], f32 coefficients[]) {
    s32 low;
    s32 high;
    s32 middle;
    f32 width;
    f32 offset;
    f32 period;

    period = knotX[SPLINE_SEGMENT_COUNT] - knotX[0];
    while (x > knotX[SPLINE_SEGMENT_COUNT]) {
        x -= period;
    }
    while (x < knotX[0]) {
        x += period;
    }

    low = 0;
    high = SPLINE_SEGMENT_COUNT;
    while (low < high) {
        middle = (low + high) / 2;
        if (knotX[middle] < x) {
            low = middle + 1;
        }
        else {
            high = middle;
        }
    }

    if (low > 0) {
        low--;
    }

    width = knotX[low + 1] - knotX[low];
    offset = x - knotX[low];
    return ((((((coefficients[low + 1] - coefficients[low]) * offset / width) +
                  (coefficients[low] * 3.0f)) *
                 offset) +
                (((knotY[low + 1] - knotY[low]) / width) -
                    (((2.0f * coefficients[low]) + coefficients[low + 1]) * width))) *
               offset) +
        knotY[low];
}

// Returns the value at position in values, between the whole numbers
// of values' indices, by a Hermite curve whose slope at each index is
// the difference to the next value. Each value and difference is
// truncated to a whole number first.
f32 InterpolateHermite(f32 values[], f32 position) {
    f32 nearValues[3];
    s32 index;
    f32 fraction;
    s32 start;
    s32 end;
    s32 startSlope;
    s32 endSlope;

    index = position;
    fraction = position - index;
    nearValues[0] = values[index];
    nearValues[1] = values[index + 1];
    nearValues[2] = values[index + 2];
    start = nearValues[0];
    end = nearValues[1];
    startSlope = nearValues[1] - nearValues[0];
    endSlope = nearValues[2] - nearValues[1];
    return ((((start - end) * 2) + startSlope + endSlope) * fraction * fraction * fraction) +
        ((-(start * 3) + (end * 3) - (startSlope * 2) - endSlope) * fraction * fraction) +
        (startSlope * fraction) + start;
}

// Adds nothing to the display list
void DrawNothing(Gfx **displayListHead) {
    *displayListHead = *displayListHead;
}
