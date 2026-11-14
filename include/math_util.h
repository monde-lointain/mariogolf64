#ifndef MATH_UTIL_H
#define MATH_UTIL_H

#include <ultra64.h>

#define BINARY_ANGLE_TO_RADIANS (3.1415927f / 32768.0f)
#define BINARY_ANGLE_MASK 0xFFFF
#define BINARY_ANGLE_TURN 0x10000
#define BINARY_ANGLE_HALF_TURN (BINARY_ANGLE_TURN / 2)
#define BINARY_ANGLE_QUARTER_TURN (BINARY_ANGLE_TURN / 4)
#define DEGREES_PER_TURN 360
#define RADIANS_TO_BINARY_ANGLE (32768.0 / (f32)M_PI)
#define QUARTER_TURN_RADIANS 1.5707964f
#define FULL_TURN_RADIANS (2 * M_PI)

#define HUNDREDTHS_PER_WHOLE 100
#define PERCENT_PER_WHOLE 100

#define RANDOM_MULTIPLIER 0x5D588B65
#define NextRandomSeed(seed) (((seed) * RANDOM_MULTIPLIER) + 1)

typedef f32 Radians;
typedef f32 Degrees;

typedef u16 BinaryAngle;
typedef s16 SignedBinaryAngle;
// A binary angle held in an s32 so that sums and steps can run past a
// turn before they are masked or truncated back to 16 bits.
typedef s32 WideBinaryAngle;

#define DegreesToRadians(degrees) ((degrees) * (M_PI / 180))
#define RADIANS_TO_DEGREES (1.0 / ((f32)M_PI / 180.0))
#define DEGREES_TO_RADIANS 0.017453294f

/* clang-format off */
#define IDENTITY_MTX \
    { \
        { \
            {0x10000, 0, 1, 0}, \
            {0, 0x10000, 0, 1}, \
            {0, 0, 0, 0}, \
            {0, 0, 0, 0}, \
        } \
    }
/* clang-format on */

#define Abs(value) \
    ({ \
        s32 absValue = (value); \
        (absValue < 0) ? -absValue : absValue; \
    })

#define AbsFloat(value) \
    ({ \
        f32 absValue = (value); \
        (absValue < 0.0f) ? -absValue : absValue; \
    })

#define HalfTowardZero(value) ((value) < 0.0f ? (value) + 0.5f : (value) - 0.5f)
#define RoundUpTo(value, alignment) (((value) + (alignment) - 1) & ~((alignment) - 1))

u32 IntLength3D(s32 x, s32 y, s32 z);
u32 IntLength2D(s32 x, s32 y);
void SetRandomSeed(s32 seed);
s32 NextRandom(void);
f32 Length2D(f32 x, f32 y);
f32 Length3D(f32 x, f32 y, f32 z);
u16 ComputeCrc16(u8 *bytes, u32 size);
s32 ReportDivisionError(void);

float acosf(float x);
float atan2f(float y, float x);
float atanf(float x);

static inline float fabsf(float value) {
    float magnitude;

    __asm__ volatile("abs.s %0, %1" : "=f"(magnitude) : "f"(value));
    return magnitude;
}

float sqrtf(float value);

#endif
