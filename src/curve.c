/*
 * Curves: periodic splines, Hermite interpolation, and stepping along a path of
 * points at a pace.
 */
#include "common.h"

/* The splines have this many segments, between one more knots */
#define SPLINE_SEGMENT_COUNT 10

/* The X, Y and Z of a point */
#define AXIS_COUNT 3

extern s32 f_PathPointIndex;
extern s32 f_PathStepIndex;
extern f32 f_PathPosition[AXIS_COUNT];
extern f32
    f_PathSegment[2][AXIS_COUNT]; /* The start and end of the target's line */
extern f32 f_PathProgress;
extern f32 f_PathProgressStep;
extern f32 f_PathStepsPerSegment;
extern f32 f_PathCatchUp;

void ResetPath(void) {
  f_PathStepIndex = 0;
  f_PathPointIndex = 0;
}

#ifdef NON_MATCHING
/*
 * Moves position toward a target that runs along the lines between the
 * points, from the second point to the last, and returns 1 once it has
 * reached the last point. Starting from the first point, the target
 * takes one step per call through each line, and the position covers
 * a fraction of its remaining distance each step, in whole units. The
 * decimal digits of pace are that fraction's numerator and then the
 * steps per line, which are also its denominator; other paces keep the
 * last path's.
 */
s32 StepAlongPath(f32 points[][AXIS_COUNT], s32 pointCount, s32 pace,
    f32 position[AXIS_COUNT]) {
  s32 nextPointIndex;
  f32 stepsPerSegment;
  f32 progress;
  f32 catchUpFraction;

  if (f_PathPointIndex >= pointCount - 1) {
    position[0] = points[pointCount - 1][0];
    position[1] = points[pointCount - 1][1];
    f_PathStepIndex = 0;
    f_PathPointIndex = 0;
    position[2] = points[pointCount - 1][2];
    return 1;
  }
  if (f_PathPointIndex == 0 && f_PathStepIndex == 0) {
    switch (pace) {
      case 12:
        f_PathCatchUp = 1.0f;
        f_PathStepsPerSegment = 2.0f;
        break;
      case 14:
        f_PathCatchUp = 1.0f;
        f_PathStepsPerSegment = 4.0f;
        break;
      case 18:
        f_PathCatchUp = 1.0f;
        f_PathStepsPerSegment = 8.0f;
        break;
      case 24:
        f_PathCatchUp = 2.0f;
        f_PathStepsPerSegment = 4.0f;
        break;
      case 28:
        f_PathCatchUp = 2.0f;
        f_PathStepsPerSegment = 8.0f;
        break;
      case 216:
        f_PathCatchUp = 2.0f;
        f_PathStepsPerSegment = 16.0f;
        break;
      case 34:
        f_PathCatchUp = 3.0f;
        f_PathStepsPerSegment = 4.0f;
        break;
      case 48:
        f_PathCatchUp = 4.0f;
        f_PathStepsPerSegment = 8.0f;
        break;
      case 416:
        f_PathCatchUp = 4.0f;
        f_PathStepsPerSegment = 16.0f;
        break;
    }
    f_PathPosition[0] = points[0][0];
    f_PathPosition[1] = points[0][1];
    f_PathPointIndex = 1;
    f_PathPosition[2] = points[0][2];
    f_PathProgressStep = 1.0f / f_PathStepsPerSegment;
  }
  if (f_PathStepIndex == 0) {
    f_PathSegment[0][0] = points[f_PathPointIndex][0];
    f_PathSegment[0][1] = points[f_PathPointIndex][1];
    f_PathSegment[0][2] = points[f_PathPointIndex][2];
    f_PathSegment[1][0] = points[f_PathPointIndex + 1][0];
    f_PathSegment[1][1] = points[f_PathPointIndex + 1][1];
    f_PathProgress = 0.0f;
    f_PathSegment[1][2] = points[f_PathPointIndex + 1][2];
  }
  stepsPerSegment = f_PathStepsPerSegment;
  catchUpFraction = (1.0f / stepsPerSegment) * f_PathCatchUp;
  progress = f_PathProgress;
  f_PathPosition[0] +=
      (s32)((f_PathSegment[0][0] +
                ((f_PathSegment[1][0] - f_PathSegment[0][0]) * progress) -
                f_PathPosition[0]) *
          catchUpFraction);
  f_PathPosition[1] +=
      (s32)((f_PathSegment[0][1] +
                ((f_PathSegment[1][1] - f_PathSegment[0][1]) * progress) -
                f_PathPosition[1]) *
          catchUpFraction);
  f_PathPosition[2] +=
      (s32)((f_PathSegment[0][2] +
                ((f_PathSegment[1][2] - f_PathSegment[0][2]) * progress) -
                f_PathPosition[2]) *
          catchUpFraction);
  position[0] = f_PathPosition[0];
  position[1] = f_PathPosition[1];
  position[2] = f_PathPosition[2];
  f_PathStepIndex++;
  f_PathProgress = progress + f_PathProgressStep;
  if (stepsPerSegment <= f_PathStepIndex) {
    nextPointIndex = f_PathPointIndex + 1;
    f_PathSegment[0][0] = points[nextPointIndex][0];
    f_PathSegment[0][1] = points[nextPointIndex][1];
    f_PathSegment[0][2] = points[nextPointIndex][2];
    f_PathSegment[1][0] = points[nextPointIndex + 1][0];
    f_PathSegment[1][1] = points[nextPointIndex + 1][1];
    f_PathSegment[1][2] = points[nextPointIndex + 1][2];
    f_PathStepIndex = 0;
    f_PathPointIndex = nextPointIndex;
  }
  /* At most 0 here, since f_PathPointIndex is at least 1 */
  return 1 - f_PathPointIndex;
}

#else
INCLUDE_ASM("asm/nonmatchings/curve", StepAlongPath);
#endif

/*
 * Fills coefficients for EvaluatePeriodicSpline with the curve through
 * the knots that repeats with the period from the first knot to the
 * last
 */
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
  diagonal[SPLINE_SEGMENT_COUNT] =
      2.0f * (widths[SPLINE_SEGMENT_COUNT - 1] + widths[0]);
  /* This leaves coefficients[SPLINE_SEGMENT_COUNT] as it was, though it is read
   * below */
  for (index = 1; index != SPLINE_SEGMENT_COUNT; index++) {
    coefficients[index] = slopes[index] - slopes[index - 1];
  }
  /* slopes is reused from here as the column that the period adds, to match */
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
    coefficients[index] =
        (coefficients[index] - (slopes[index] * ratio)) / diagonal[index];
  }
}

/* Returns the height at x of the spline that InitPeriodicSpline set up */
f32 EvaluatePeriodicSpline(
    f32 x, f32 knotX[], f32 knotY[], f32 coefficients[]) {
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
    } else {
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
                  (((2.0f * coefficients[low]) + coefficients[low + 1]) *
                      width))) *
             offset) +
      knotY[low];
}

/*
 * Returns the value at position in values, between the whole numbers
 * of values' indices, by a Hermite curve whose slope at each index is
 * the difference to the next value. Each value and difference is
 * truncated to a whole number first.
 */
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
  return ((((start - end) * 2) + startSlope + endSlope) * fraction * fraction *
             fraction) +
      ((-(start * 3) + (end * 3) - (startSlope * 2) - endSlope) * fraction *
          fraction) +
      (startSlope * fraction) + start;
}

/* Adds nothing to the display list */
void DrawNothing(Gfx** displayListHead) {
  *displayListHead = *displayListHead;
}
