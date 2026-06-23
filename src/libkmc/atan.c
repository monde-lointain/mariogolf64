/*
 * atan.c -- arctangent: atan() and atan2(), built on a CORDIC kernel.
 *
 * _xatan() is the CORDIC vectoring iteration that rotates the vector (u, v)
 * toward the x-axis, accumulating the angle of rotation; the per-step angles
 * live in the _atbl[] arctangent table. The public atan()/atan2() do the
 * range reduction and quadrant resolution around that kernel. Values are held
 * in XLONG fixed-point scaled by MBIT (see cordic.h).
 */
#include "_kmclib.h"
#include <math.h>
#include <cordic.h>

// Below this magnitude atan(d) ~= d, so skip the CORDIC kernel.
#define NR 1.0e-7

extern XLONG _atbl[];

/* CORDIC vectoring: rotate (u, v) onto the x-axis in CBIT shrinking steps,
 * summing the rotation angles into z; the result atan(v/u) is returned through
 * atanp, rescaled out of fixed-point. */
_xatan(u, v, atanp) XLONG u, v;
double* atanp;
{
  int i;
  XLONG x, xx, y, z;
  x = u;
  y = v;
  z = 0;
  for (i = 0; i < CBIT; ++i) {
    // Rotate toward y == 0: the sign of y picks the rotation direction, and
    // the shift by i is the 2^-i CORDIC step. Accumulate (or undo) that
    // step's angle from the table accordingly.
    if (y >= 0) {
      xx = x + (y >> i);
      y = y - (x >> i);
      z = z + _atbl[i];
    } else {
      xx = x - (y >> i);
      y = y + (x >> i);
      z = z - _atbl[i];
    }
    x = xx;
  }
  *atanp = (double)z / MBIT;
}

double atan(d)
double d;
{
  double a;
  XLONG u, v;
  int si;

  // CORDIC handles the first quadrant only: fold the sign out and restore it on
  // the result.
  if (d < 0) {
    d = -d;
    si = -1;
  } else {
    si = 1;
  }

  if (d > NR) {
    // Keep the kernel argument in [0, 1] for accuracy: for d >= 1 feed atan of
    // the reciprocal, which the iteration computes as the complementary angle
    // directly via the (u, v) it is handed.
    if (d >= 1.) {
      v = 1 * MBIT;
      u = (1. / d) * MBIT;
    } else {
      u = 1 * MBIT;
      v = d * MBIT;
    }
    _xatan(u, v, &a);
  } else {
    // Tiny argument: atan(d) ~= d.
    a = d;
  }
  return (si < 0 ? -a : a);
}

double atan2(y, x)
double y, x;
{
  double d;

  // Origin has no defined angle: report a domain error.
  if (x == 0 && y == 0) {
    return __matherr(_DOMAIN, "atan2", x, y, 0.0);
  }

  // On the y-axis the slope is infinite, so answer +/- PI/2 by the sign of y.
  if (x == 0) {
    if (y > 0) {
      return PI / 2;
    }
    return -PI / 2;
  }

  // Off the axes: take atan of the slope, then shift by the quadrant. x > 0 is
  // the right half-plane and needs no correction; x < 0 adds or subtracts PI to
  // move the principal-value angle into the correct quadrant.
  d = y / x;
  d = atan(d);
  if (x > 0) {
    return d;
  }
  if (y == 0) {
    return PI;
  }
  if (y > 0) {
    return d + PI;
  }
  return d - PI;
}
