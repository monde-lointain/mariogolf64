#include "common.h"

#ifdef NONMATCHING
u32 ___udivmoddi4(u64 dividend, u64 divisor, u64 *remainder) {
    u32 quotient = 0;
    u32 mask = 1;
    u64 next_divisor; /* Declared here for C89 compliance */

    /* 1. Initial Comparison */
    if (divisor > dividend) {
        if (remainder) *remainder = dividend;
        return 0;
    }

    /* 2. Equality Check */
    if (divisor == dividend) {
        if (remainder) *remainder = 0;
        return 1;
    }

    /* 3. Start_Shift */
    while (1) {
        /* Check if MSB (bit 63) is set */
        if ((divisor >> 63) & 1) {
            break;
        }

        next_divisor = divisor << 1;

        /* Check if shifted divisor exceeds dividend */
        if (next_divisor > dividend) {
            break;
        }

        divisor = next_divisor;
        mask <<= 1;
    }

    /* 4. Division_Loop */
    do {
        if (dividend >= divisor) {
            dividend -= divisor;
            quotient |= mask;
        }

        divisor >>= 1;
        mask >>= 1;

    } while (mask != 0);

    /* 5. Return */
    if (remainder) *remainder = dividend;

    return quotient;
}
#else
INCLUDE_ASM("asm/nonmatchings/udivmoddi4", ___udivmoddi4);
#endif

INCLUDE_ASM("asm/nonmatchings/udivmoddi4", ____udiv32);
