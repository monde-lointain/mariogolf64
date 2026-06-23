/*
 * Modeling matrix combining Euler rotation, uniform scale, and translation.
 *
 * guPositionF builds the float matrix; guPosition packs it to fixed point. It
 * is guRotateRPY's rotation with the scale s folded into the 3x3 part and the
 * translation (x, y, z) placed in the bottom row.
 */

#include "guint.h"

/* Place an object: rotate by roll/pitch/heading (r, p, h), scale uniformly by
 * s, and translate to (x, y, z). Angles are in degrees, counter-clockwise
 * positive. Each rotation entry is multiplied by s as it is written, so scale
 * and rotation cost nothing extra; the bottom row holds the translation with w
 * = 1. */
void guPositionF(float mf[4][4], float r, float p, float h, float s, float x,
                 float y, float z) {
  static float dtor = 3.1415926 / 180.0;
  float sinr;
  float sinp;
  float sinh;
  float cosr;
  float cosp;
  float cosh;

  r *= dtor;
  p *= dtor;
  h *= dtor;
  sinr = sinf(r);
  cosr = cosf(r);
  sinp = sinf(p);
  cosp = cosf(p);
  sinh = sinf(h);
  cosh = cosf(h);

  mf[0][0] = (cosp * cosh) * s;
  mf[0][1] = (cosp * sinh) * s;
  mf[0][2] = (-sinp) * s;
  mf[0][3] = 0.0;
  mf[1][0] = ((sinr * sinp * cosh) - (cosr * sinh)) * s;
  mf[1][1] = ((sinr * sinp * sinh) + (cosr * cosh)) * s;
  mf[1][2] = (sinr * cosp) * s;
  mf[1][3] = 0.0;
  mf[2][0] = ((cosr * sinp * cosh) + (sinr * sinh)) * s;
  mf[2][1] = ((cosr * sinp * sinh) - (sinr * cosh)) * s;
  mf[2][2] = (cosr * cosp) * s;
  mf[2][3] = 0.0;
  mf[3][0] = x;
  mf[3][1] = y;
  mf[3][2] = z;
  mf[3][3] = 1.0;
}

/* Fixed-point entry point: build the matrix as floats, then pack to Mtx. */
void guPosition(Mtx* m, float r, float p, float h, float s, float x, float y,
                float z) {
  float mf[4][4];

  guPositionF(mf, r, p, h, s, x, y, z);
  guMtxF2L(mf, m);
}
