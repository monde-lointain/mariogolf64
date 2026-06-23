/*
 * Modeling matrix for rotation by an angle about an arbitrary axis.
 *
 * guAlignF builds the float matrix; guAlign packs it to fixed point. The axis
 * (x, y, z) need not be unit length; it is normalized internally.
 */

#include "guint.h"

/* Rotate by a degrees about the arbitrary axis (x, y, z).
 *
 * The standard arbitrary-axis rotation: rotate the axis to point along -Z,
 * spin by a about Z, then rotate back. The combined transform is folded
 * directly into the matrix columns below. h is the length of the axis projected
 * onto the XZ plane; when h == 0 the axis already lies along Y and the identity
 * built by guMtxIdentF is the answer, so the else branch is intentionally
 * empty. */
void guAlignF(float mf[4][4], float a, float x, float y, float z) {
  // degrees -> radians; static so it is computed once and shared across calls
  static float dtor = 3.1415926 / 180.0;
  float s;
  float c;
  float h;
  float hinv;

  vec3f_normalize(&x, &y, &z);
  a *= dtor;
  s = sinf(a);
  c = cosf(a);
  h = sqrtf((x * x) + (z * z));

  guMtxIdentF(mf);
  if (h != 0) {
    hinv = 1 / h;

    // clang-format off: hand-aligned matrix columns mirror the basis vectors
    mf[0][0] = ((-z * c) - (s * y * x)) * hinv;
    mf[1][0] = ( (z * s) - (c * y * x)) * hinv;
    mf[2][0] = -x;
    mf[3][0] = 0;

    mf[0][1] = s * h;
    mf[1][1] = c * h;
    mf[2][1] = -y;
    mf[3][1] = 0;

    mf[0][2] = ( (c * x) - (s * y * z)) * hinv;
    mf[1][2] = ((-s * x) - (c * y * z)) * hinv;
    mf[2][2] = -z;
    mf[3][2] = 0;

    mf[0][3] = 0;
    mf[1][3] = 0;
    mf[2][3] = 0;
    mf[3][3] = 1;
    // clang-format on
  } else {
  }
}

/* Fixed-point entry point: build the matrix as floats, then pack to Mtx. */
void guAlign(Mtx* m, float a, float x, float y, float z) {
  Matrix mf;

  guAlignF(mf, a, x, y, z);
  guMtxF2L(mf, m);
}
