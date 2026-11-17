/*
 * Converts floats to unsigned words as the N64's code does, without the
 * undefined behavior of converting a negative float to an unsigned type.
 */

#ifndef FLOAT_TO_UNSIGNED_H
#define FLOAT_TO_UNSIGNED_H

#include <ultra64.h>

// 2^31, the first value that doesn't fit a signed word
#define SIGNED_WORD_LIMIT 2147483648.0f
#define UNSIGNED_WORD_TOP_BIT 0x80000000u

// A float truncated to an unsigned word, as KMC GCC compiles the conversion:
// values below 2^31 truncate as signed words, so negative values keep their
// signed bits, and those from 2^31 up lose 2^31 first and take the top bit
// after. The value must be above -2^31 and below 2^32.
static inline u32 TruncateToU32(f32 value) {
    if (value >= SIGNED_WORD_LIMIT) {
        return (u32)(s32)(value - SIGNED_WORD_LIMIT) | UNSIGNED_WORD_TOP_BIT;
    }

    return (u32)(s32)value;
}

#endif
