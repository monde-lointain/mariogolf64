/*
 * Host-side (floating-point) matrix concatenation and point transform.
 *
 * guMtxCatF multiplies two 4x4 matrices; guMtxXFMF applies a matrix to a single
 * point. These run on the CPU so a transform can be computed without handing
 * the matrix to the RSP.
 */

#include "guint.h"

/* Multiply mf * nf into res. A scratch matrix accumulates the product so that
 * res is allowed to alias either input. */
void guMtxCatF(float mf[4][4], float nf[4][4], float res[4][4]) {
  int i;
  int j;
  int k;
  float temp[4][4];

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      temp[i][j] = 0.0;
      for (k = 0; k < 4; k++) {
        temp[i][j] += mf[i][k] * nf[k][j];
      }
    }
  }

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      res[i][j] = temp[i][j];
    }
  }
}

/* Transform the point (x, y, z) by mf, writing the result to *ox/*oy/*oz.
 * The point is treated as having w = 1, so the matrix's bottom row supplies the
 * translation. */
void guMtxXFMF(float mf[4][4], float x, float y, float z, float* ox, float* oy,
               float* oz) {
  *ox = (mf[0][0] * x) + (mf[1][0] * y) + (mf[2][0] * z) + mf[3][0];
  *oy = (mf[0][1] * x) + (mf[1][1] * y) + (mf[2][1] * z) + mf[3][1];
  *oz = (mf[0][2] * x) + (mf[1][2] * y) + (mf[2][2] * z) + mf[3][2];
}
