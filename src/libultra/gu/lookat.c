/*
 * Viewing (camera) matrix from an eye point, a look-at target, and an up
 * vector.
 *
 * guLookAtF builds the float matrix; guLookAt packs it to fixed point. The
 * result transforms world space into eye space (camera at the origin looking
 * down -Z).
 */

#include "guint.h"

/* Build the viewing matrix for a camera at (xEye,yEye,zEye) aimed at
 * (xAt,yAt,zAt) with the given up hint.
 *
 * Constructs an orthonormal eye-space basis: Look points from the eye toward
 * the target (negated and normalized so the camera faces -Z), Right is up x
 * Look, and the up hint is re-derived as Look x Right so it is exactly
 * perpendicular. The basis fills the rotation part; the bottom row is -dot(eye,
 * axis), i.e. the eye translated into the new frame. */
void guLookAtF(float mf[4][4], float xEye, float yEye, float zEye, float xAt,
               float yAt, float zAt, float xUp, float yUp, float zUp) {
  float len, xLook, yLook, zLook, xRight, yRight, zRight;

  guMtxIdentF(mf);

  // Look = normalize(eye - at); the leading -1 makes the camera face -Z.
  xLook = xAt - xEye;
  yLook = yAt - yEye;
  zLook = zAt - zEye;
  len = -1.0 / sqrtf(xLook * xLook + yLook * yLook + zLook * zLook);
  xLook *= len;
  yLook *= len;
  zLook *= len;

  // Right = normalize(up x Look).
  xRight = yUp * zLook - zUp * yLook;
  yRight = zUp * xLook - xUp * zLook;
  zRight = xUp * yLook - yUp * xLook;
  len = 1.0 / sqrtf(xRight * xRight + yRight * yRight + zRight * zRight);
  xRight *= len;
  yRight *= len;
  zRight *= len;

  // Re-orthogonalize the up vector as Up = normalize(Look x Right).
  xUp = yLook * zRight - zLook * yRight;
  yUp = zLook * xRight - xLook * zRight;
  zUp = xLook * yRight - yLook * xRight;
  len = 1.0 / sqrtf(xUp * xUp + yUp * yUp + zUp * zUp);
  xUp *= len;
  yUp *= len;
  zUp *= len;

  // Columns hold the basis; the bottom row holds -dot(eye, axis).
  mf[0][0] = xRight;
  mf[1][0] = yRight;
  mf[2][0] = zRight;
  mf[3][0] = -(xEye * xRight + yEye * yRight + zEye * zRight);
  mf[0][1] = xUp;
  mf[1][1] = yUp;
  mf[2][1] = zUp;
  mf[3][1] = -(xEye * xUp + yEye * yUp + zEye * zUp);
  mf[0][2] = xLook;
  mf[1][2] = yLook;
  mf[2][2] = zLook;
  mf[3][2] = -(xEye * xLook + yEye * yLook + zEye * zLook);
  mf[0][3] = 0;
  mf[1][3] = 0;
  mf[2][3] = 0;
  mf[3][3] = 1;
}

/* Fixed-point entry point: build the matrix as floats, then pack to Mtx. */
void guLookAt(Mtx* m, float xEye, float yEye, float zEye, float xAt, float yAt,
              float zAt, float xUp, float yUp, float zUp) {
  Matrix mf;

  guLookAtF(mf, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);
  guMtxF2L(mf, m);
}
