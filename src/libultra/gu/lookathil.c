/*
 * Viewing matrix plus LookAt and Hilite structures for specular highlights.
 *
 * guLookAtHiliteF builds the float matrix, fills the LookAt (as the reflect
 * variant does), and additionally computes two specular-highlight texture
 * offsets into the Hilite from two light directions; guLookAtHilite packs the
 * matrix to fixed point. The highlight is placed where the bisector of a light
 * and the view direction projects onto the camera's Right/Up axes.
 */

#include "guint.h"

/* Build the viewing matrix and derive two highlight texture offsets.
 *
 * twidth/theight are the highlight texture's half-extents (in 1/4-texel units):
 * the *4 term centers the offset and the *2 term scales the projected bisector,
 * so the highlight tracks the light as the camera turns. */
void guLookAtHiliteF(float mf[4][4], LookAt* l, Hilite* h, float xEye,
                     float yEye, float zEye, float xAt, float yAt, float zAt,
                     float xUp, float yUp, float zUp, float xl1, float yl1,
                     float zl1, float xl2, float yl2, float zl2, int twidth,
                     int theight) {
  float len;
  float xLook;
  float yLook;
  float zLook;
  float xRight;
  float yRight;
  float zRight;
  float xHilite;
  float yHilite;
  float zHilite;

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

  // Highlight 1: normalize light 1, form its bisector with the view direction,
  // and project that onto the Right/Up axes to get the texture offset.
  len = 1.0 / sqrtf((xl1 * xl1) + (yl1 * yl1) + (zl1 * zl1));
  xl1 *= len;
  yl1 *= len;
  zl1 *= len;
#define THRESH2 0.1
  xHilite = xl1 + xLook;
  yHilite = yl1 + yLook;
  zHilite = zl1 + zLook;
  len = sqrtf((xHilite * xHilite) + (yHilite * yHilite) + (zHilite * zHilite));

  // When the bisector is well defined, project it; below THRESH2 the light and
  // view nearly oppose, so fall back to the texture center.
  if (len > THRESH2) {
    len = 1.0 / len;
    xHilite *= len;
    yHilite *= len;
    zHilite *= len;
    h->h.x1 = (twidth * 4) +
              (((xHilite * xRight) + (yHilite * yRight) + (zHilite * zRight)) *
               twidth * 2);
    h->h.y1 =
        (theight * 4) +
        (((xHilite * xUp) + (yHilite * yUp) + (zHilite * zUp)) * theight * 2);
  } else {
    h->h.x1 = twidth * 2;
    h->h.y1 = theight * 2;
  }

  // Highlight 2: same projection for the second light direction.
  len = 1.0 / sqrtf((xl2 * xl2) + (yl2 * yl2) + (zl2 * zl2));
  xl2 *= len;
  yl2 *= len;
  zl2 *= len;
  xHilite = xl2 + xLook;
  yHilite = yl2 + yLook;
  zHilite = zl2 + zLook;
  len = sqrtf((xHilite * xHilite) + (yHilite * yHilite) + (zHilite * zHilite));
  if (len > THRESH2) {
    len = 1.0 / len;
    xHilite *= len;
    yHilite *= len;
    zHilite *= len;
    h->h.x2 = (twidth * 4) +
              (((xHilite * xRight) + (yHilite * yRight) + (zHilite * zRight)) *
               twidth * 2);
    h->h.y2 =
        (theight * 4) +
        (((xHilite * xUp) + (yHilite * yUp) + (zHilite * zUp)) * theight * 2);
  } else {
    h->h.x2 = twidth * 2;
    h->h.y2 = theight * 2;
  }

  // Pack the Right and Up axes into the LookAt lights (as in guLookAtReflectF);
  // the col bytes are fixed s/t axis tags, not colors, and pads stay zero.
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
void guLookAtHilite(Mtx* m, LookAt* l, Hilite* h, float xEye, float yEye,
                    float zEye, float xAt, float yAt, float zAt, float xUp,
                    float yUp, float zUp, float xl1, float yl1, float zl1,
                    float xl2, float yl2, float zl2, int twidth, int theight) {
  Matrix mf;

  guLookAtHiliteF(mf, l, h, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp, xl1,
                  yl1, zl1, xl2, yl2, zl2, twidth, theight);
  guMtxF2L(mf, m);
}
