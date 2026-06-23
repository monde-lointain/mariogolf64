/*
 * Translation modeling matrix.
 *
 * guTranslateF builds the float matrix; guTranslate packs it to fixed point.
 * The result translates an object by (x, y, z).
 */

#include "guint.h"

/* Identity with the translation placed in the bottom row (row-vector
 * convention, so the offset lives in row 3 rather than column 3). */
void guTranslateF(float mf[4][4], float x, float y, float z) {
  guMtxIdentF(mf);
  mf[3][0] = x;
  mf[3][1] = y;
  mf[3][2] = z;
}

/* Fixed-point entry point: build the matrix as floats, then pack to Mtx. */
void guTranslate(Mtx* m, float x, float y, float z) {
  Matrix mf;

  guTranslateF(mf, x, y, z);
  guMtxF2L(mf, m);
}
