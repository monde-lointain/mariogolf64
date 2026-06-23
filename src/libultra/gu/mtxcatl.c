/*
 * Fixed-point wrappers for matrix concatenation and point transform.
 *
 * These unpack the RSP's fixed-point Mtx form to host floats, defer the actual
 * math to the guMtx*F routines, then repack the result.
 */

#include "guint.h"
#include "os_version.h"

/* Multiply m * n into res, all in fixed-point Mtx form. res may alias an input
 * because guMtxCatF accumulates into a scratch matrix. */
void guMtxCatL(Mtx* m, Mtx* n, Mtx* res) {
  float mf[4][4];
  float nf[4][4];
  float resf[4][4];

  guMtxL2F(mf, m);
  guMtxL2F(nf, n);
  guMtxCatF(mf, nf, resf);
  guMtxF2L(resf, res);
}

// Dropped from the API at VERSION_K; only the floating-point guMtxXFMF survives
// there, so guard the fixed-point wrapper to match the target version.
#if BUILD_VERSION < VERSION_K
/* Transform the point (x, y, z) by the fixed-point matrix m, returning floats.
 */
void guMtxXFML(Mtx* m, float x, float y, float z, float* ox, float* oy,
               float* oz) {
  float mf[4][4];

  guMtxL2F(mf, m);
  guMtxXFMF(mf, x, y, z, ox, oy, oz);
}
#endif
