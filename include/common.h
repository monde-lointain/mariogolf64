#ifndef COMMON_H
#define COMMON_H

#include <ultra64.h>
#include "include_asm.h"

/* A point or direction in 3D space */
typedef f32 Vec3[3];
/* Indices of a point's coordinates; y is up */
#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2
#define AXIS_COUNT 3

/*
 * A point on the course as the game's logic keeps it, in fixed point:
 * GetGroundHeight's units, 1024 to a world unit
 */
typedef s32 CoursePoint[AXIS_COUNT];

/* Scale between course units and world units */
#define WORLD_TO_COURSE_SCALE 1024.0f
#define COURSE_TO_WORLD_SCALE (1.0f / 1024.0f)

/* Course point units in a yard, and in a foot */
#define UNITS_PER_YARD 14045.184f
#define UNITS_PER_FOOT 4681.728f
#define YARDS_PER_UNIT 0.00007119878f
#define METERS_PER_UNIT 0.00006510417f
#define FEET_PER_YARD 3

/* A 4 by 4 matrix of floats, indexed by row and then column */
typedef f32 Mat4[4][4];

#define ARRAY_COUNT(array) (s32)(sizeof(array) / sizeof((array)[0]))

/* The magnitude of an integer. The game's code evaluates the value once,
   into a variable of its own, as a GNU statement expression does. */
#define ABS(value) ({ s32 absValue = (value); (absValue < 0) ? -absValue : absValue; })

/* The RDP loads textures from 8-byte boundaries */
#define TEXTURE_ADDRESS(address) ((u32)(address) & ~7)

#endif
