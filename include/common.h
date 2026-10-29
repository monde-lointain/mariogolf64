#ifndef COMMON_H
#define COMMON_H

#include <ultra64.h>

#include "include_asm.h"

#define BITS_PER_BYTE 8
/* The base of the decimal numbers the game shows */
#define DECIMAL_BASE 10
/* The largest s32 value */
#define S32_MAX 0x7FFFFFFF

/* Indices of a point's coordinates; y is vertical, and in the ball's
 * simulation it grows downward, the way GRAVITY pulls */
#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2
#define AXIS_COUNT 3
/* A point or direction in 3D space */
typedef f32 Vec3[AXIS_COUNT];

/*
 * A point on the course as the game's logic keeps it, in fixed point:
 * GetGroundHeight's units, 1024 to a world unit
 */
typedef s32 CoursePoint[AXIS_COUNT];

/* Scale between course units and world units */
#define WORLD_TO_COURSE_SCALE 1024.0f
#define COURSE_TO_WORLD_SCALE (1.0f / 1024.0f)
/* WORLD_TO_COURSE_SCALE as a shift */
#define WORLD_TO_COURSE_SHIFT 10

/* Course point units in a yard, a foot and a meter */
#define UNITS_PER_YARD 14045.184f
#define UNITS_PER_FOOT 4681.728f
#define UNITS_PER_METER 15360.0f
#define YARDS_PER_UNIT 0.00007119878f
#define METERS_PER_UNIT 0.00006510417f
#define FEET_PER_YARD 3

/* What a falling ball or creature gains in downward speed each frame, in
 * course units a frame */
#define GRAVITY 262.0f

/* A 4 by 4 matrix of floats, indexed by row and then column */
typedef f32 Mat4[4][4];

/* Zero is false, and any other value true, as C tests it */
typedef s32 Boolean;

#define ARRAY_COUNT(array) (s32)(sizeof(array) / sizeof((array)[0]))

/* The size of a struct member, without an instance to apply sizeof to */
#define MemberSize(type, member) (s32)sizeof(((type*)0)->member)

/* The magnitude of an integer. The game's code evaluates the value once,
   into a variable of its own, as a GNU statement expression does. */
#define Abs(value) \
  ({ \
    s32 absValue = (value); \
    (absValue < 0) ? -absValue : absValue; \
  })

/* The bytes from start up to end, which may point at different types */
#define BytesBetween(start, end) ((u8*)(end) - (u8*)(start))
/* The address byteCount bytes past pointer, whatever it points at */
#define AddressPast(pointer, byteCount) ((u8*)(pointer) + (byteCount))
/* value rounded up to a multiple of alignment, which is a power of two */
#define RoundUpTo(value, alignment) \
  (((value) + (alignment) - 1) & ~((alignment) - 1))

/* The RDP loads textures from 8-byte boundaries */
#define TextureAddress(address) ((u32)(address) & ~7)
/* The RDP draws into color and depth images from 8-byte boundaries too */
#define RenderTargetAddress(address) ((u32)(address) & ~7)

/* A 32-bit RGBA color, as gSPModifyVertex takes a vertex color */
#define RgbaColor(red, green, blue, alpha) \
  (((red) << 24) | ((green) << 16) | ((blue) << 8) | (alpha))

#endif
