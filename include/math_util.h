#ifndef MATH_UTIL_H
#define MATH_UTIL_H

#include <ultra64.h>

/* Converts binary angles, where 0x10000 is a turn, to radians */
#define BINARY_ANGLE_TO_RADIANS (3.1415927f / 32768.0f)
/* Keeps a binary angle within one turn */
#define BINARY_ANGLE_MASK 0xFFFF
/* A full turn as a binary angle */
#define BINARY_ANGLE_TURN 0x10000
/* A full turn in degrees */
#define DEGREES_PER_TURN 360
/* Converts radians to binary angles */
#define RADIANS_TO_BINARY_ANGLE (32768.0 / (f32)M_PI)
/* A quarter turn in radians */
#define QUARTER_TURN_RADIANS 1.5707964f

/* The linear congruential step that the game's random generators share */
#define RANDOM_MULTIPLIER 0x5D588B65
#define NextRandomSeed(seed) (((seed) * RANDOM_MULTIPLIER) + 1)

/* Angles as floats: sinf and atan2f work in radians, guRotate in degrees */
typedef f32 Radians;
typedef f32 Degrees;

/* An angle where BINARY_ANGLE_TURN is a full turn, as the game stores them */
typedef u16 BinaryAngle;
/* A binary angle stored signed, from minus a half turn to under a half turn */
typedef s16 SignedBinaryAngle;

/* The radians to degrees factor of rotations */
#define DegreesToRadians(degrees) ((degrees) * (M_PI / 180))
#define RADIANS_TO_DEGREES (1.0 / ((f32)M_PI / 180.0))

/*
 * An identity matrix in Mtx's fixed point: the integer halves, two to a word,
 * then the fractions
 */
/* clang-format off */
#define IDENTITY_MTX \
  {{{0x10000, 0, 1, 0}, \
    {0, 0x10000, 0, 1}, \
    {0, 0, 0, 0}, \
    {0, 0, 0, 0}}}
/* clang-format on */

u32 IntLength3D(s32 x, s32 y, s32 z);
u32 IntLength2D(s32 x, s32 y);
void SetRandomSeed(s32 seed);
s32 NextRandom(void);
f32 Length2D(f32 x, f32 y);
f32 Length3D(f32 x, f32 y, f32 z);
u16 ComputeCrc16(u8* data, u32 size);
s32 ReportDivisionError(void);

/* float_math.c */
float fabsf(float x);
float acosf(float x);
float atan2f(float y, float x);
float atanf(float x);

#endif
