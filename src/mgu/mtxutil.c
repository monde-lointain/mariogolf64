/*
 * Matrix conversion utilities: move 4x4 transforms between the float form the
 * game computes with and the s15.16 fixed-point Mtx the RSP consumes.
 *
 * An N64 Mtx packs each s15.16 element as two 16-bit halves stored far apart:
 * the integer halves of all 16 elements occupy the first 8 words, the fraction
 * halves the last 8 words. So a single fixed-point value is reassembled from
 * the high word (via ai) and the matching low word (via af), and vice versa.
 */
#include <ultra64.h>

/*
 * Pack a float matrix into the fixed-point Mtx the graphics microcode reads.
 *
 * ai walks the integer-half block (rows 0-1), af the fraction-half block (rows
 * 2-3); each loop pass converts two adjacent elements to s15.16 and splices
 * their high/low 16 bits into the two destination words. The clamp to
 * [-32768, 32766] before conversion is a game guard against s15.16 overflow and
 * is not in the stock SGI source.
 */
void guMtxF2L(float mf[4][4], Mtx* m) {
  int i;
  int j;
  int e1;
  int e2;
  int* ai;
  int* af;

  ai = (int*)&m->m[0][0];
  af = (int*)&m->m[2][0];

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 2; j++) {
      // Saturate to the representable s15.16 range before fixed-point
      // conversion.
      if (mf[i][j * 2] < -32768.0F) {
        mf[i][j * 2] = -32768.0F;
      }
      if (mf[i][j * 2] > 32766.0F) {
        mf[i][j * 2] = 32766.0F;
      }
      if (mf[i][(j * 2) + 1] < -32768.0F) {
        mf[i][(j * 2) + 1] = -32768.0F;
      }
      if (mf[i][(j * 2) + 1] > 32766.0F) {
        mf[i][(j * 2) + 1] = 32766.0F;
      }

      e1 = FTOFIX32(mf[i][j * 2]);
      e2 = FTOFIX32(mf[i][j * 2 + 1]);

      // Integer halves go to ai, fraction halves to af.
      *(ai++) = (e1 & 0xffff0000) | ((e2 >> 16) & 0xffff);
      *(af++) = ((e1 << 16) & 0xffff0000) | (e2 & 0xffff);
    }
  }
}

/*
 * Unpack a fixed-point Mtx back into a float matrix.
 *
 * Reassembles each s15.16 value from its high word (ai, the integer-half block)
 * and low word (af, the fraction-half block), reinterprets the bits as a signed
 * integer, then scales back to float.
 */
void guMtxL2F(float mf[4][4], Mtx* m) {
  int i;
  int j;
  unsigned int e1;
  unsigned int e2;
  unsigned int* ai;
  unsigned int* af;
  int q1;
  int q2;

  ai = (unsigned int*)&m->m[0][0];
  af = (unsigned int*)&m->m[2][0];

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 2; j++) {
      e1 = (*ai & 0xffff0000) | ((*af >> 16) & 0xffff);
      e2 = ((*(ai++) << 16) & 0xffff0000) | (*(af++) & 0xffff);

      // Reinterpret the packed bits as signed before scaling to float.
      q1 = *((int*)&e1);
      q2 = *((int*)&e2);

      mf[i][j * 2] = FIX32TOF(q1);
      mf[i][(j * 2) + 1] = FIX32TOF(q2);
    }
  }
}

// Set a float matrix to the 4x4 identity.
void guMtxIdentF(float mf[4][4]) {
  int i;
  int j;

  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      if (i == j) {
        mf[i][j] = 1.0;
      } else {
        mf[i][j] = 0.0;
      }
    }
  }
}

// Build the fixed-point identity Mtx via the float form.
void guMtxIdent(Mtx* m) {
  float mf[4][4];

  guMtxIdentF(mf);
  guMtxF2L(mf, m);
}
