/*
 * sin.c -- trigonometric functions: sin(), cos(), tan(), on a CORDIC kernel.
 *
 * _xsincos() is the CORDIC rotation iteration: starting from the vector
 * (K, 0) it rotates by the target angle, leaving cos in x and sin in y. Seeding
 * x with the CORDIC gain constant K cancels the iteration's built-in growth so
 * the result is already normalized. The public functions reduce the angle into
 * a half-period around 0, call the kernel, then fix up the sign by the half it
 * landed in. Values are XLONG fixed-point scaled by MBIT (see cordic.h).
 */
#include "_kmclib.h"
#include <math.h>
#include <cordic.h>

// Below this magnitude sin(th) ~= th, so skip the CORDIC kernel.
#define NR 1.0e-8

extern XLONG _atbl[];

/* CORDIC rotation: rotate (K, 0) by angle th in CBIT steps; the per-step angles
 * are the _atbl[] table. Returns sin(th) through sinp and cos(th) through cosp.
 */
_xsincos(th, sinp, cosp) double th;
double *sinp, *cosp;
{
  int i;
  XLONG x, xx, y, z;
  x = (double)K * MBIT;
  y = 0;
  z = th * MBIT;
  for (i = 0; i < CBIT; ++i) {
    // Drive the residual angle z toward 0: its sign chooses the rotation
    // direction, the shift by i is the 2^-i step, and the step's angle is
    // added to or removed from z from the table.
    if (z >= 0) {
      xx = x - (y >> i);
      y = y + (x >> i);
      z = z - _atbl[i];
    } else {
      xx = x + (y >> i);
      y = y - (x >> i);
      z = z + _atbl[i];
    }
    x = xx;
  }
  *sinp = (double)y / MBIT;
  *cosp = (double)x / MBIT;
}

double sin(th)
double th;
{
  double s;
  double c;
  int ti;
  int sign;

  // Range-reduce to (-PI/2, PI/2]: ti is the count of half-periods nearest th
  // (rounding via the +/- 0.5 bias), and removing ti*PI leaves the residual the
  // kernel can handle. The odd/even parity of ti then flips the sign below.
  ti = (th / PI) + (th >= 0 ? 0.5 : -0.5);
  th = th - (ti * PI);
  if (th < NR && th > -NR) {
    return th;
  }
  _xsincos(th, &s, &c);
  return (ti & 1 ? -s : s);
}

double cos(th)
double th;
{
  double s;
  double c;
  int ti;

  // Same half-period reduction as sin(); cos picks up its sign from ti's parity
  // (cos negates across each successive half-period).
  ti = (th / PI) + (th >= 0 ? 0.5 : -0.5);
  th = th - (ti * PI);
  _xsincos(th, &s, &c);
  return (ti & 1 ? -c : c);
}

double tan(th)
double th;
{
  double s;
  double c;
  int ti;

  // tan = sin/cos: the half-period sign flips cancel in the ratio, so reduce,
  // compute both, and divide.
  ti = (th / PI) + (th >= 0 ? 0.5 : -0.5);
  th = th - (ti * PI);
  if (th < NR && th > -NR) {
    return th;
  }
  _xsincos(th, &s, &c);
  return s / c;
}
