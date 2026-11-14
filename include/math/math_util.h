/*
 * Arithmetic that the whole game shares: angle units, random numbers, lengths,
 * magnitudes and rounding, and the float routines of math_library.c.
 */

#ifndef MATH_UTIL_H
#define MATH_UTIL_H

#include <ultra64.h>

// Converts binary angles, where 0x10000 is a turn, to radians
#define BINARY_ANGLE_TO_RADIANS (3.1415927f / 32768.0f)
// Keeps a binary angle within one turn
#define BINARY_ANGLE_MASK 0xFFFF
// A full turn as a binary angle
#define BINARY_ANGLE_TURN 0x10000
#define BINARY_ANGLE_HALF_TURN (BINARY_ANGLE_TURN / 2)
#define BINARY_ANGLE_QUARTER_TURN (BINARY_ANGLE_TURN / 4)
// A full turn in degrees
#define DEGREES_PER_TURN 360
// Converts radians to binary angles
#define RADIANS_TO_BINARY_ANGLE (32768.0 / (f32)M_PI)
// A quarter turn in radians
#define QUARTER_TURN_RADIANS 1.5707964f
// A full turn in radians
#define FULL_TURN_RADIANS (2 * M_PI)

#define HUNDREDTHS_PER_WHOLE 100
#define PERCENT_PER_WHOLE 100

// The linear congruential step that the game's random generators share
#define RANDOM_MULTIPLIER 0x5D588B65
#define NextRandomSeed(seed) (((seed) * RANDOM_MULTIPLIER) + 1)

// Angles as floats: sinf and atan2f work in radians, guRotate in degrees
typedef f32 Radians;
typedef f32 Degrees;

// An angle where BINARY_ANGLE_TURN is a full turn, as the game stores them
typedef u16 BinaryAngle;
// A binary angle stored signed, from minus a half turn to under a half turn
typedef s16 SignedBinaryAngle;
// A binary angle held in a full word, as some routines take and return one
typedef s32 WideBinaryAngle;

// Conversions between degrees and radians
#define DegreesToRadians(degrees) ((degrees) * (M_PI / 180))
#define RADIANS_TO_DEGREES (1.0 / ((f32)M_PI / 180.0))
// The original's single-precision approximation of pi / 180, a step above
// the nearest float
#define DEGREES_TO_RADIANS 0.017453294f
/* clang-format off */

// An identity matrix in Mtx's fixed point: the integer halves, two to a word,
// then the fractions
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

// The magnitude of an integer. The game's code evaluates the value once,
// into a variable of its own, as a GNU statement expression does. GCC's
// built-in abs compiles differently: with it, the matched code of 10 files
// changes.
#define Abs(value) \
    ({ \
        s32 absValue = (value); \
        (absValue < 0) ? -absValue : absValue; \
    })

// The magnitude of a float, as a compare and branch where fabsf compiles to
// abs.s. It is a macro because the ball simulations compile further from the
// original when these sites call an inline function instead: a static one
// takes StepBallSimulation from 478 differing words to 580 and
// StepOpeningBallSimulation from 247 to 472, and a function nested in
// StepBallSimulation gives it 580 too.
#define AbsFloat(value) \
    ({ \
        f32 absValue = (value); \
        (absValue < 0.0f) ? -absValue : absValue; \
    })

// A float moved half a unit toward zero, as the title positions are before
// converting them to whole pixels
#define HalfTowardZero(value) ((value) < 0.0f ? (value) + 0.5f : (value) - 0.5f)
// value rounded up to a multiple of alignment, which is a power of two
#define RoundUpTo(value, alignment) (((value) + (alignment) - 1) & ~((alignment) - 1))

u32 IntLength3D(s32 x, s32 y, s32 z);
u32 IntLength2D(s32 x, s32 y);
void SetRandomSeed(s32 seed);
s32 NextRandom(void);
f32 Length2D(f32 x, f32 y);
f32 Length3D(f32 x, f32 y, f32 z);
u16 ComputeCrc16(u8 *bytes, u32 size);
s32 ReportDivisionError(void);

// math_library.c
float acosf(float x);
float atan2f(float y, float x);
float atanf(float x);

// The game's own fabsf, an abs.s in volatile inline assembly. Local-alloc
// can't give its result the operand's register, as it can the built-in's,
// and the scheduler keeps it in place. Every matched function compiles the
// same with it, and DrawRoundHud and IsEasedEyeHeightZero, which differed
// only at an abs.s, match with it; without volatile, 5 and 7 of their words
// differ. The matched functions that need the built-in's abs.s take fabs
// of a float, which GCC narrows to one.
static inline float fabsf(float value) {
    float magnitude;

    __asm__ volatile("abs.s %0, %1" : "=f"(magnitude) : "f"(value));
    return magnitude;
}

// Built into GCC, which compiles it to a sqrt.s instruction. Unlike fabsf, the
// game uses the built-in: as inline assembly like fabsf's, volatile or not,
// the matched code of 20 files compiles differently.
float sqrtf(float value);

#endif
