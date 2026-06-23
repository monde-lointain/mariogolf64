/*
 * Single-precision sine (software, no FPU intrinsic).
 *
 * Splits the input by magnitude (read from the exponent bits) into bands: tiny
 * angles return unchanged, small angles skip range reduction, and mid-range
 * angles are reduced modulo pi before the polynomial. Evaluation is done in
 * double precision for accuracy. The names fsin/sinf alias this one body.
 */

#include "guint.h"

#pragma weak fsin = __sinf
#pragma weak sinf = __sinf
#define fsin __sinf

// Minimax polynomial coefficients c0..c4, as exact IEEE-754 bit patterns so the
// compiler cannot perturb them; result ~ dx + dx*xsq*(c1 + xsq*(c2 + ...)).
// clang-format off: hand-aligned coefficient table reads as one block
static const du P[] = {
    {0x3ff00000, 0x00000000}, {0xbfc55554, 0xbc83656d},
    {0x3f8110ed, 0x3804c2a0}, {0xbf29f6ff, 0xeea56814},
    {0x3ec5dbdf, 0x0e314bfe},
};
// clang-format on

static const du rpi = {0x3fd45f30, 0x6dc9c883};   // 1/pi, for the period count
static const du pihi = {0x400921fb, 0x50000000};  // pi, high half (exact bits)
static const du pilo = {0x3e6110b4, 0x611a6263};  // pi, low correction term
static const fu zero = {0x00000000};              // exact +0.0

float fsin(float x) {
  double dx, xsq, poly;
  double dn;
  int n;
  double result;
  int ix, xpt;

  // Pull the biased exponent from the bit pattern to band the magnitude without
  // an FPU compare.
  ix = *(int*)&x;
  xpt = (ix >> 22);
  xpt &= 0x1ff;

  // Small-magnitude band: |x| already within one period, no reduction needed.
  if (xpt < 0xff) {
    dx = x;

    // Above 0xe6 the polynomial is required; below it x is so tiny that
    // sin(x) == x to float precision, so return x untouched.
    if (xpt >= 0xe6) {
      xsq = dx * dx;
      poly = ((P[4].d * xsq + P[3].d) * xsq + P[2].d) * xsq + P[1].d;
      result = dx + (dx * xsq) * poly;
      return ((float)result);
    }
    return (x);
  }

  // Mid band: reduce modulo pi (n = nearest multiple), then evaluate. pi is
  // split hi/lo so the large multiply keeps its low bits.
  if (xpt < 0x136) {
    dx = x;
    dn = dx * rpi.d;
    n = ROUND(dn);
    dn = n;
    dx = dx - dn * pihi.d;
    dx = dx - dn * pilo.d;
    xsq = dx * dx;
    poly = ((P[4].d * xsq + P[3].d) * xsq + P[2].d) * xsq + P[1].d;
    result = dx + (dx * xsq) * poly;

    // Odd n means an odd number of half periods were removed: flip the sign.
    if ((n & 1) == 0) return ((float)result);
    return (-(float)result);
  }

  // x is NaN (only value not equal to itself) or too large to reduce.
  if (x != x) {
#ifdef _IP_NAN_SETS_ERRNO
    *__errnoaddr = EDOM;
#endif
    return (__libm_qnan_f);
  }
  return (zero.f);
}
