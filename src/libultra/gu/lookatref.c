/*
 * Viewing matrix plus a LookAt structure for reflection mapping.
 *
 * guLookAtReflectF builds the float matrix and fills the LookAt;
 * guLookAtReflect packs the matrix to fixed point. The two LookAt lights carry
 * the camera's Right and Up axes so the RSP can generate reflection-map texture
 * coordinates.
 */

#include "guint.h"

/* Build the viewing matrix (as guLookAtF) and store the eye-space Right/Up axes
 * into the LookAt so the microcode can do environment/reflection mapping. */
void guLookAtReflectF(float mf[4][4], LookAt* l, float xEye, float yEye,
                      float zEye, float xAt, float yAt, float zAt, float xUp,
                      float yUp, float zUp) {
  float len;
  float xLook;
  float yLook;
  float zLook;
  float xRight;
  float yRight;
  float zRight;

  guMtxIdentF(mf);

  // Look = normalize(eye - at); the leading -1 makes the camera face -Z.
  xLook = xAt - xEye;
  yLook = yAt - yEye;
  zLook = zAt - zEye;
  len = -1.0 / sqrtf((xLook * xLook) + (yLook * yLook) + (zLook * zLook));
  xLook *= len;
  yLook *= len;
  zLook *= len;

  // Right = normalize(up x Look).
  xRight = (yUp * zLook) - (zUp * yLook);
  yRight = (zUp * xLook) - (xUp * zLook);
  zRight = (xUp * yLook) - (yUp * xLook);
  len = 1.0 / sqrtf((xRight * xRight) + (yRight * yRight) + (zRight * zRight));
  xRight *= len;
  yRight *= len;
  zRight *= len;

  // Re-orthogonalize the up vector as Up = normalize(Look x Right).
  xUp = (yLook * zRight) - (zLook * yRight);
  yUp = (zLook * xRight) - (xLook * zRight);
  zUp = (xLook * yRight) - (yLook * xRight);
  len = 1.0 / sqrtf((xUp * xUp) + (yUp * yUp) + (zUp * zUp));
  xUp *= len;
  yUp *= len;
  zUp *= len;

  // Pack the Right and Up axes (as signed 8-bit fractions) into the two LookAt
  // lights; the microcode dots an eye-space normal against these to derive
  // reflection-map s and t texture coordinates. The col bytes are fixed tags
  // (light 1 zeroed, light 2's green byte 0x80) that select the s vs t axis,
  // not real light colors; pad bytes stay zero.
  l->l[0].l.dir[0] = FTOFRAC8(xRight);
  l->l[0].l.dir[1] = FTOFRAC8(yRight);
  l->l[0].l.dir[2] = FTOFRAC8(zRight);
  l->l[1].l.dir[0] = FTOFRAC8(xUp);
  l->l[1].l.dir[1] = FTOFRAC8(yUp);
  l->l[1].l.dir[2] = FTOFRAC8(zUp);
  l->l[0].l.col[0] = 0x00;
  l->l[0].l.col[1] = 0x00;
  l->l[0].l.col[2] = 0x00;
  l->l[0].l.pad1 = 0x00;
  l->l[0].l.colc[0] = 0x00;
  l->l[0].l.colc[1] = 0x00;
  l->l[0].l.colc[2] = 0x00;
  l->l[0].l.pad2 = 0x00;
  l->l[1].l.col[0] = 0x00;
  l->l[1].l.col[1] = 0x80;
  l->l[1].l.col[2] = 0x00;
  l->l[1].l.pad1 = 0x00;
  l->l[1].l.colc[0] = 0x00;
  l->l[1].l.colc[1] = 0x80;
  l->l[1].l.colc[2] = 0x00;
  l->l[1].l.pad2 = 0x00;

  // Columns hold the basis; the bottom row holds -dot(eye, axis).
  mf[0][0] = xRight;
  mf[1][0] = yRight;
  mf[2][0] = zRight;
  mf[3][0] = -((xEye * xRight) + (yEye * yRight) + (zEye * zRight));
  mf[0][1] = xUp;
  mf[1][1] = yUp;
  mf[2][1] = zUp;
  mf[3][1] = -((xEye * xUp) + (yEye * yUp) + (zEye * zUp));
  mf[0][2] = xLook;
  mf[1][2] = yLook;
  mf[2][2] = zLook;
  mf[3][2] = -((xEye * xLook) + (yEye * yLook) + (zEye * zLook));
  mf[0][3] = 0;
  mf[1][3] = 0;
  mf[2][3] = 0;
  mf[3][3] = 1;
}

/* Fixed-point entry point: build the matrix as floats, then pack to Mtx. */
void guLookAtReflect(Mtx* m, LookAt* l, float xEye, float yEye, float zEye,
                     float xAt, float yAt, float zAt, float xUp, float yUp,
                     float zUp) {
  float mf[4][4];

  guLookAtReflectF(mf, l, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);
  guMtxF2L(mf, m);
}
