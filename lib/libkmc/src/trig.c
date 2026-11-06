#include "libm.h"

/* 1 / the gain of CORDIC_ITERATION_COUNT rotations, so the result has unit
 * length */
#define CORDIC_INVERSE_GAIN 0x09B74EDA8435E580LL

/* Rotates a unit vector from the x axis by the angle, through ever smaller
 * known angles */
void CordicSineCosine(double angle, double* sine, double* cosine) {
  long long x;
  long long y;
  long long remainingAngle;
  long long rotatedX;
  int i;

  x = CORDIC_INVERSE_GAIN;
  y = 0;
  remainingAngle = angle * FIXED_POINT_ONE_F;
  for (i = 0; i < CORDIC_ITERATION_COUNT; i++) {
    if (remainingAngle >= 0) {
      rotatedX = x - (y >> i);
      y += x >> i;
      remainingAngle -= g_CordicAngles[i];
    } else {
      rotatedX = x + (y >> i);
      y -= x >> i;
      remainingAngle += g_CordicAngles[i];
    }
    x = rotatedX;
  }
  *sine = y / FIXED_POINT_ONE_F;
  *cosine = x / FIXED_POINT_ONE_F;
}

double sin(double x) {
  int halfTurnCount;
  double sine;
  double cosine;

  /* CORDIC only converges within about 1.74 radians of 0 */
  halfTurnCount = x / M_PI + ((x >= 0.0) ? 0.5 : -0.5);
  x -= halfTurnCount * M_PI;
  /* sin(x) is x this close to 0 */
  if (x < 1e-8 && x > -1e-8) {
    return x;
  }
  CordicSineCosine(x, &sine, &cosine);
  return (halfTurnCount & 1) ? -sine : sine;
}

double cos(double x) {
  int halfTurnCount;
  double sine;
  double cosine;

  halfTurnCount = x / M_PI + ((x >= 0.0) ? 0.5 : -0.5);
  x -= halfTurnCount * M_PI;
  CordicSineCosine(x, &sine, &cosine);
  return (halfTurnCount & 1) ? -cosine : cosine;
}

double tan(double x) {
  int halfTurnCount;
  double sine;
  double cosine;

  halfTurnCount = x / M_PI + ((x >= 0.0) ? 0.5 : -0.5);
  x -= halfTurnCount * M_PI;
  if (x < 1e-8 && x > -1e-8) {
    return x;
  }
  CordicSineCosine(x, &sine, &cosine);
  return sine / cosine;
}
