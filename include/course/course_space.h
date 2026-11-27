/*
 * The course's fixed-point space: its points, its scale to world units,
 * its units in yards, feet and meters, and its gravity
 */

#ifndef COURSE_SPACE_H
#define COURSE_SPACE_H

#include <ultra64.h>

#include "math/geometry.h"

// A point on the course as the game's logic keeps it, in fixed point:
// GetGroundHeight's units, 1024 to a world unit
typedef s32 CoursePoint[AXIS_COUNT];

// Scale between course units and world units
#define WORLD_TO_COURSE_SCALE 1024.0f
#define COURSE_TO_WORLD_SCALE (1.0f / 1024.0f)
// WORLD_TO_COURSE_SCALE as a shift
#define WORLD_TO_COURSE_SHIFT 10

// A world coordinate or size in course units. Course X and Z, and the
// ground's heights, are often negative, which C leaves undefined to shift,
// so other builds than the matching one multiply, which gives the same bits
// and, for any integer, the same type.
#ifdef AVOID_UB
#    define WorldToCourse(value) ((value) * (1 << WORLD_TO_COURSE_SHIFT))
#else
#    define WorldToCourse(value) ((value) << WORLD_TO_COURSE_SHIFT)
#endif

// The cross product of a triangle's edge, in world units, with a point's
// offset from the edge's start, in course units, which tells the side of
// the edge that the point is on. Far from the edge it overflows a word,
// which the N64's multiply and subtract wrap. C leaves a signed overflow
// undefined, so other builds than the matching one work out the same bits
// unsigned.
#ifdef AVOID_UB
#    define EdgeCross(edgeU, edgeV, offsetU, offsetV) \
        ((s32)(((u32)(edgeU) * (u32)(offsetV)) - ((u32)(edgeV) * (u32)(offsetU))))
#else
#    define EdgeCross(edgeU, edgeV, offsetU, offsetV) \
        (((edgeU) * (offsetV)) - ((edgeV) * (offsetU)))
#endif

// Course point units in a yard, a foot and a meter
#define UNITS_PER_YARD 14045.184f
#define UNITS_PER_FOOT 4681.728f
#define UNITS_PER_METER 15360.0f
#define YARDS_PER_UNIT 0.00007119878f
#define METERS_PER_UNIT 0.00006510417f
#define FEET_PER_YARD 3

// What a falling ball or creature gains in downward speed each frame, in
// course units a frame
#define GRAVITY 262.0f

#endif
