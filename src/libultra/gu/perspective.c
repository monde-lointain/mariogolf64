/*
 * Perspective projection matrix (symmetric frustum).
 *
 * guPerspectiveF builds the float matrix and the companion perspNorm scale;
 * guPerspective packs the matrix to fixed point. The view volume is a truncated
 * pyramid symmetric about the view axis.
 */

#include "guint.h"
#include <ultratypes.h>

/* Build a symmetric perspective projection from a vertical field of view, an
 * aspect ratio, and the near/far clip distances (both must be positive).
 *
 * cot(fovy/2) sets the vertical zoom; dividing by aspect spreads it
 * horizontally. The [2][2]/[3][2] terms map z into the clip range and [2][3]=-1
 * makes w carry -z for the perspective divide. Every element is multiplied by
 * scale to bias the digits the RSP keeps in fixed point.
 *
 * perspNorm (optional) is the w-normalization factor gSPPerspNormalize needs to
 * maximize divide precision in the geometry engine; it is the u16 fixed-point
 * value of 2/(near+far), clamped so it never underflows to 0 or overflows. */
void guPerspectiveF(float mf[4][4], u16* perspNorm, float fovy, float aspect,
                    float near, float far, float scale) {
  float cot;
  int i;
  int j;

  guMtxIdentF(mf);
  fovy *= 3.1415926 / 180.0;
  cot = cosf(fovy / 2) / sinf(fovy / 2);

  mf[0][0] = cot / aspect;
  mf[1][1] = cot;
  mf[2][2] = (near + far) / (near - far);
  mf[2][3] = -1;
  mf[3][2] = (2 * near * far) / (near - far);
  mf[3][3] = 0;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      mf[i][j] *= scale;
    }
  }

  if (perspNorm != (u16*)NULL) {
    if (near + far <= 2.0) {
      *perspNorm = (u16)0xFFFF;
    } else {
      *perspNorm = (u16)((2.0 * 65536.0) / (near + far));
      if (*perspNorm <= 0) {
        *perspNorm = (u16)0x0001;
      }
    }
  }
}

/* Fixed-point entry point: build the matrix as floats, then pack to Mtx. */
void guPerspective(Mtx* m, u16* perspNorm, float fovy, float aspect, float near,
                   float far, float scale) {
  Matrix mf;

  guPerspectiveF(mf, perspNorm, fovy, aspect, near, far, scale);
  guMtxF2L(mf, m);
}
