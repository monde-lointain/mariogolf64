#ifndef MATH_UTIL_H
#define MATH_UTIL_H

#include <ultra64.h>

/* Converts binary angles, where 0x10000 is a turn, to radians */
#define BINARY_ANGLE_TO_RADIANS (3.1415927f / 32768.0f)
/* Keeps a binary angle within one turn */
#define BINARY_ANGLE_MASK 0xFFFF
/* A full turn as a binary angle */
#define BINARY_ANGLE_TURN 0x10000

/* The radians to degrees factor of rotations */
#define DEGREES_TO_RADIANS(degrees) ((degrees) * (M_PI / 180))
#define RADIANS_TO_DEGREES (1.0 / ((f32)M_PI / 180.0))

u32 IntLength3D(s32 x, s32 y, s32 z);
u32 IntLength2D(s32 x, s32 y);
void SetRandomSeed(s32 seed);
s32 NextRandom(void);
f32 Length2D(f32 x, f32 y);
f32 Length3D(f32 x, f32 y, f32 z);
u16 ComputeCrc16(u8 *data, u32 size);
s32 ReportDivisionError(void);

/* float_math.c */
float fabsf(float x);
float acosf(float x);
float atan2f(float y, float x);
float atanf(float x);

#endif
