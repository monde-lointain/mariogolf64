/*
 * Single-precision cosine (software, no FPU intrinsic).
 *
 * Reduces the argument to a half-period offset, then evaluates a degree-8 even
 * polynomial in double precision for accuracy before narrowing back to float.
 * The public names fcos/cosf both alias this one implementation.
 */

#include "guint.h"

#pragma weak fcos = __cosf
#pragma weak cosf = __cosf
#define fcos __cosf

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

float fcos(float x) {
  float absx;
  double dx;
  double xsq;
  double poly;
  double dn;
  int n;
  double result;
  int ix;
  int xpt;

  // Pull the biased exponent straight from the float's bit pattern to test
  // magnitude without an FPU compare.
  ix = *(int*)&x;
  xpt = (ix >> 22);
  xpt &= 0x1ff;

  // Below the threshold the argument is small enough that range reduction stays
  // accurate; above it, only the NaN/large fallback below applies.
  if (xpt < 0x136) {
    // cos is even, so work with |x| and recover the sign from the half-period
    // count n.
    absx = ABS(x);
    dx = absx;

    // n = number of half periods; subtracting (n - 0.5)*pi folds x into the
    // range centered on a cosine extremum. pi is split hi/lo so the large
    // multiply keeps its low bits.
    dn = (dx * rpi.d) + 0.5;
    n = ROUND(dn);
    dn = n;
    dn -= 0.5;
    dx = dx - (dn * pihi.d);
    dx = dx - (dn * pilo.d);

    // Horner evaluation of the odd-symmetric polynomial on the reduced angle.
    xsq = dx * dx;
    poly = (((((P[4].d * xsq) + P[3].d) * xsq) + P[2].d) * xsq) + P[1].d;
    result = dx + ((dx * xsq) * poly);

    // Each successive half period flips the sign of the result.
    if ((n & 1) == 0) {
      return ((float)result);
    }
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
