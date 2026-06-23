/*
 * Scaling modeling matrix.
 *
 * guScaleF builds the float matrix; guScale packs it to fixed point. The result
 * scales an object by (x, y, z) about the origin.
 */

#include "guint.h"

/* Identity with the diagonal replaced by the per-axis scale factors. */
void guScaleF(float mf[4][4], float x, float y, float z) {
  guMtxIdentF(mf);
  mf[0][0] = x;
  mf[1][1] = y;
  mf[2][2] = z;
  mf[3][3] = 1;
}

/* Fixed-point entry point: build the matrix as floats, then pack to Mtx. */
void guScale(Mtx* m, float x, float y, float z) {
  Matrix mf;

  guScaleF(mf, x, y, z);
  guMtxF2L(mf, m);
}
