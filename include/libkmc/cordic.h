/*
 * cordic.h -- fixed-point configuration for the CORDIC math kernels.
 *
 * The KMC trig/log routines (atan, sin, ...) run a CORDIC iteration in
 * fixed-point. This header selects the integer width that holds those
 * fixed-point values and the matching scale, then supplies the math constants
 * the kernels need. The fixed-point format places the binary point at bit CBIT,
 * so MBIT (1 << CBIT) is the value representing 1.0.
 */
#ifndef _CORDEC_H
#define _CORDEC_H

// Two configurations: the disabled Turbo-C build uses a 32-bit long fixed-point
// (point at bit 30); the active GCC build uses 64-bit long long (point at bit
// 52, matching the double mantissa). Hand-aligned so the two columns line up.
#if 0
// clang-format off: aligned config columns
#define TURBOC  1
#define MBIT    0x40000000  /* 1.0 in the 32-bit fixed-point format */
#define CBIT    30          /* binary point position (1 << CBIT == MBIT) */
#define XLONG   long        /* signed integer holding a fixed-point value */
#define XDOUBLE double      /* widest float used by the kernels */
// clang-format on
#else
// clang-format off: aligned config columns
#define GCC     1
#define MBIT    0x1000000000000000LL /* 1.0 in the 64-bit fixed-point format */
#define CBIT    52                   /* binary point position (1 << CBIT == MBIT) */
#define XLONG   long long            /* signed integer holding a fixed-point value */
#define XDOUBLE long double          /* widest float used by the kernels */
// clang-format on
#endif

#define PI 3.141592653589793238  // the constant pi
#define K 0.607252935008881278   // CORDIC gain: product of cos(atan(2^-i))

// Library math-error raiser, shared with math.h.
double __matherr(int type, char* name, double arg1, double arg2, double retval);

#endif
