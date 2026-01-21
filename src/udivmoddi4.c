#include "common.h"

#ifdef NONMATCHING
/**
 * 64-bit unsigned division/modulo helper.
 * Note: This function appears to be hand-written assembly in the original ROM.
 * Score: 4045 (best achievable with C, structural match but register allocation differs)
 *
 * Arguments passed as split 32-bit hi:lo pairs:
 *   dividend = (dividend_hi << 32) | dividend_lo
 *   divisor  = (divisor_hi << 32) | divisor_lo
 * Returns quotient in v1, remainder in a0:a1 (modified in place)
 */
u32 ___udivmoddi4(u32 dividend_hi, u32 dividend_lo, u32 divisor_hi, u32 divisor_lo)
{
    u32 quotient;
    u32 mask;
    u32 temp;

    quotient = 0;

    /* Compare dividend vs divisor (64-bit) */
    temp = divisor_hi < dividend_hi;
    if (temp) {
        goto do_divide;
    }
    if (dividend_hi != divisor_hi) {
        return 0;
    }
    temp = divisor_lo < dividend_lo;
    if (temp) {
        goto do_divide;
    }
    if (dividend_lo != divisor_lo) {
        return 0;
    }
    /* dividend == divisor: quotient=1, remainder=0 */
    dividend_lo ^= dividend_lo;
    dividend_hi ^= dividend_hi;
    quotient = 1;
    return 0;

do_divide:
    mask = 1;

    /* Shift divisor left until MSB set or divisor > dividend */
    for (;;) {
        temp = divisor_hi & 0x80000000;
        if (temp) {
            break;
        }
        temp = divisor_lo >> 31;
        divisor_hi = (divisor_hi << 1) | temp;
        divisor_lo = divisor_lo << 1;

        temp = divisor_hi < dividend_hi;
        if (temp) {
            mask <<= 1;
            continue;
        }
        if (dividend_hi != divisor_hi) {
            goto shift_back;
        }
        temp = divisor_lo < dividend_lo;
        if (!temp) {
            goto shift_back;
        }
        mask <<= 1;
    }
    goto div_loop;

shift_back:
    /* Went one shift too far, shift back */
    temp = divisor_hi << 31;
    divisor_lo = (divisor_lo >> 1) | temp;
    divisor_hi = divisor_hi >> 1;

div_loop:
    /* Main division loop */
    do {
        temp = divisor_hi < dividend_hi;
        if (temp) {
            goto subtract;
        }
        if (dividend_hi != divisor_hi) {
            goto shift_right;
        }
        temp = dividend_lo < divisor_lo;
        if (temp) {
            goto shift_right;
        }
subtract:
        /* 64-bit subtract: dividend -= divisor */
        temp = dividend_lo < divisor_lo;
        dividend_lo -= divisor_lo;
        dividend_hi -= divisor_hi;
        dividend_hi -= temp;
        quotient |= mask;

shift_right:
        /* Shift divisor right by 1 */
        temp = divisor_hi << 31;
        divisor_lo = (divisor_lo >> 1) | temp;
        divisor_hi = divisor_hi >> 1;
        mask >>= 1;
    } while (mask);

    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/udivmoddi4", ___udivmoddi4);
#endif

#ifdef NONMATCHING
/**
 * 64÷32 unsigned division returning 64-bit quotient.
 * Inputs: dividend_hi:dividend_lo (64-bit), divisor (32-bit)
 * Returns: quotient (64-bit) in v0:v1
 */
u64 ____udiv32(u32 dividend_hi, u32 dividend_lo, u32 unused, u32 divisor)
{
    u32 quot_hi;  /* t2 */
    u32 quot_lo;  /* t0 */
    u32 rem_hi;   /* a0 after first div */
    u32 rem_lo;   /* a1 */
    u32 tmp_lo;   /* v1 */
    u32 tmp_hi;   /* v0 */
    u32 tmp_div;  /* t1 */
    u32 carry;    /* t9 */
    u32 q;        /* v1 */
    u32 prod_lo, prod_hi;

    /* Initial division: dividend_hi / divisor */
    quot_hi = dividend_hi / divisor;
    rem_hi = dividend_hi % divisor;
    quot_lo = 0;
    rem_lo = dividend_lo;

    /* Loop while remainder high word is non-zero */
    while (rem_hi != 0) {
        /* Copy rem_hi:rem_lo and divisor for shifting */
        tmp_lo = rem_lo;
        tmp_hi = rem_hi;
        tmp_div = divisor;

        /* Shift tmp_hi:tmp_lo and tmp_div right until tmp_hi == 0 */
        do {
            carry = tmp_hi << 31;
            tmp_lo = (tmp_lo >> 1) | carry;
            tmp_hi = tmp_hi >> 1;
            tmp_div = tmp_div >> 1;
        } while (tmp_hi != 0);

        tmp_div += 1;

        /* Compute partial quotient */
        q = tmp_lo / tmp_div;

        /* 64-bit add: quot_lo:quot_hi += 0:q */
        tmp_hi ^= tmp_hi;  /* v0 = 0 */
        quot_lo += q;
        carry = quot_lo < q;
        quot_hi += tmp_hi;
        quot_hi += carry;

        /* 64-bit subtract: rem_hi:rem_lo -= (q * divisor) */
        prod_lo = q * divisor;
        prod_hi = (u32)(((u64)q * divisor) >> 32);
        carry = rem_lo < prod_lo;
        rem_lo -= prod_lo;
        rem_hi -= prod_hi;
        rem_hi -= carry;
    }

    /* Final division of remainder */
    q = rem_lo / divisor;
    tmp_hi ^= tmp_hi;  /* v0 = 0 */
    tmp_lo = q + quot_lo;
    carry = tmp_lo < quot_lo;
    tmp_hi += quot_hi;
    tmp_hi += carry;

    return ((u64)tmp_hi << 32) | tmp_lo;
}
#else
INCLUDE_ASM("asm/nonmatchings/udivmoddi4", ____udiv32);
#endif
