#ifndef COURSE_SPACE_H
#define COURSE_SPACE_H

#include <ultra64.h>

#include "math/geometry.h"

typedef s32 CoursePoint[AXIS_COUNT];

#define WORLD_TO_COURSE_SCALE 1024.0f
#define COURSE_TO_WORLD_SCALE (1.0f / 1024.0f)
#define WORLD_TO_COURSE_SHIFT 10

#define UNITS_PER_YARD 14045.184f
#define UNITS_PER_FOOT 4681.728f
#define UNITS_PER_METER 15360.0f
#define YARDS_PER_UNIT 0.00007119878f
#define METERS_PER_UNIT 0.00006510417f
#define FEET_PER_YARD 3

#define GRAVITY 262.0f

#endif
