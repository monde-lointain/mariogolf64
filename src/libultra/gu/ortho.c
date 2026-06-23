/*
 * Orthographic (parallel) projection matrix.
 *
 * guOrthoF builds the float matrix; guOrtho packs it to fixed point. Unlike a
 * perspective projection there is no foreshortening, so the view volume is the
 * box [l,r] x [b,t] x [n,f] mapped straight onto clip space.
 */

#include "guint.h"

/* Build an orthographic projection mapping the box [l,r] x [b,t] x [n,f] to the
 * canonical view volume. n and f are signed world-space z (not distances), so z
 * is negated; the diagonal scales each axis and the bottom row recenters it.
 * Every element is multiplied by scale, which biases the significant digits the
 * RSP keeps when this is later packed to fixed point. */
void guOrthoF(float mf[4][4], float l, float r, float b, float t, float n,
              float f, float scale) {
  int i;
  int j;

  guMtxIdentF(mf);
  mf[0][0] = 2 / (r - l);
  mf[1][1] = 2 / (t - b);
  mf[2][2] = -2 / (f - n);
  mf[3][0] = -(r + l) / (r - l);
  mf[3][1] = -(t + b) / (t - b);
  mf[3][2] = -(f + n) / (f - n);
  mf[3][3] = 1;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      mf[i][j] *= scale;
    }
  }
}

/* Fixed-point entry point: build the matrix as floats, then pack to Mtx. */
void guOrtho(Mtx* m, float l, float r, float b, float t, float n, float f,
             float scale) {
  Matrix mf;

  guOrthoF(mf, l, r, b, t, n, f, scale);
  guMtxF2L(mf, m);
}
