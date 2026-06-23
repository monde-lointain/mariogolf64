/*
 * Modeling matrix for rotation by an angle about an arbitrary axis.
 *
 * guRotateF builds the float matrix; guRotate packs it to fixed point. This is
 * the Rodrigues rotation formula; the axis (x, y, z) is normalized internally.
 */

#include "guint.h"
#include "os_version.h"

/* Rotate by a degrees about the arbitrary axis (x, y, z), counter-clockwise for
 * positive a. ab/bc/ca are the off-diagonal cross terms x*y*(1-cos) etc.; the
 * x*sine pair (see the version split below) supplies the antisymmetric part. */
void guRotateF(float mf[4][4], float a, float x, float y, float z) {
  static float dtor = 3.1415926 / 180.0;
  float sine;
  float cosine;
  float ab, bc, ca, t;
#if BUILD_VERSION >= VERSION_K
  float xxsine;
  float yxsine;
  float zxsine;
#endif

  vec3f_normalize(&x, &y, &z);
  a *= dtor;
  sine = sinf(a);
  cosine = cosf(a);
  t = (1 - cosine);
  ab = x * y * t;
  bc = y * z * t;
  ca = z * x * t;
  guMtxIdentF(mf);

  // VERSION_K hoists the axis*sine products into locals; earlier versions
  // inline them via macros, which changes only how the compiler schedules the
  // math.
#if BUILD_VERSION >= VERSION_K
  xxsine = x * sine;
  yxsine = y * sine;
  zxsine = z * sine;
#else
#define xxsine (x * sine)
#define yxsine (y * sine)
#define zxsine (z * sine)
#endif

  // Each axis: diagonal gets cos-interpolated component^2; the symmetric cross
  // term (ab/bc/ca) +/- the axis*sine term fills the paired off-diagonals.
  t = x * x;
  mf[0][0] = t + (cosine * (1 - t));
  mf[2][1] = bc - xxsine;
  mf[1][2] = bc + xxsine;
  t = y * y;
  mf[1][1] = t + (cosine * (1 - t));
  mf[2][0] = ca + yxsine;
  mf[0][2] = ca - yxsine;
  t = z * z;
  mf[2][2] = t + (cosine * (1 - t));
  mf[1][0] = ab - zxsine;
  mf[0][1] = ab + zxsine;
}

/* Fixed-point entry point: build the matrix as floats, then pack to Mtx. */
void guRotate(Mtx* m, float a, float x, float y, float z) {
  Matrix mf;

  guRotateF(mf, a, x, y, z);
  guMtxF2L(mf, m);
}
