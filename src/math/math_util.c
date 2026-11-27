/*
 * Math helpers: vector lengths in floats and integers, the random number
 * generator and a CRC-16.
 */

#include "math/math_util.h"

#include <math.h>
#include <ultra64.h>

#include "system/c_language.h"
#include "system/compiler.h"
#include "system/memory_address.h"

// The integer lengths halve their components until they are below this
// magnitude
#define INT_LENGTH_COMPONENT_LIMIT 0x20000000
// Whether -INT_LENGTH_COMPONENT_LIMIT <= value < INT_LENGTH_COMPONENT_LIMIT
#define IsSmallComponent(value) \
    ((u32)(value) - INT_LENGTH_COMPONENT_LIMIT >= (u32)(-2 * INT_LENGTH_COMPONENT_LIMIT))

// CRC-16/X.25: reflected CCITT polynomial, all ones in and inverted out
#define CRC16_REFLECTED_POLYNOMIAL 0x8408
#define CRC16_INITIAL_VALUE 0xFFFF

static s32 f_RandomSeed = 0;
// Only the N64 divides by it
static f32 f_ZeroDivisor UNUSED = 0.0f;
static s32 f_UnusedWordsAfterZeroDivisor[2] UNUSED = {0};

// Halves the components until they are small enough, and doubles the length
// back
u32 IntLength3D(s32 x, s32 y, s32 z) {
    s32 shiftCount;

    shiftCount = 0;
    while (TRUE) {
        if (IsSmallComponent(x)) {
            if (IsSmallComponent(y)) {
                if (IsSmallComponent(z)) {
                    break;
                }
            }
        }

        shiftCount++;
        x >>= 1;
        y >>= 1;
        z >>= 1;
    }

    return (u32)sqrt(((f64)x * x) + ((f64)y * y) + ((f64)z * z)) << shiftCount;
}

// The same in two dimensions
u32 IntLength2D(s32 x, s32 y) {
    s32 shiftCount;

    shiftCount = 0;
    while (TRUE) {
        if (IsSmallComponent(x)) {
            if (IsSmallComponent(y)) {
                break;
            }
        }

        shiftCount++;
        x >>= 1;
        y >>= 1;
    }

    return (u32)sqrt(((f64)x * x) + ((f64)y * y)) << shiftCount;
}

// The random generator that NextRandom steps
void SetRandomSeed(s32 seed) {
    f_RandomSeed = seed;
}

s32 NextRandom(void) {
    return f_RandomSeed = NextRandomSeed(f_RandomSeed);
}

f32 Length2D(f32 x, f32 y) {
    return sqrtf((x * x) + (y * y));
}

f32 Length3D(f32 x, f32 y, f32 z) {
    return sqrtf((x * x) + (y * y) + (z * z));
}

// The CRC-16 of size bytes, which it also prints to the host
u16 ComputeCrc16(u8 *bytes, u32 size) {
    u32 crc;
    u32 bitIndex;
    u8 *end;

    crc = CRC16_INITIAL_VALUE;
    if (size != 0) {
        end = (u8 *)(size + AddressNumber(bytes));
        do {
            crc ^= *bytes;
            for (bitIndex = 0; bitIndex < BITS_PER_BYTE; bitIndex++) {
                if (crc & 1) {
                    crc = (crc >> 1) ^ CRC16_REFLECTED_POLYNOMIAL;
                }
                else {
                    crc >>= 1;
                }
            }

            bytes++;
        }
        while (bytes < end);
    }

    crc = ~crc;
    crc &= 0xFFFF;
    osSyncPrintf("CRC = %x\n", crc);
    return crc;
}

// Prints the caller, then forces a CPU exception: truncating the infinite
// quotient to an integer is an unimplemented operation, which the FPU cannot
// mask
s32 ReportDivisionError(void) {
    u32 returnAddress;

#ifdef __mips__
    asm volatile("move %0, $31" : "=r"(returnAddress));
    osSyncPrintf("kDivError %x\n", returnAddress);
    return 1.0f / f_ZeroDivisor;
#else
    // Other CPUs give a number for the quotient, so the error stops the game
    // as the N64's exception did
    returnAddress = (uintptr_t)__builtin_return_address(0);
    osSyncPrintf("kDivError %x\n", returnAddress);
    __builtin_trap();
#endif
}
