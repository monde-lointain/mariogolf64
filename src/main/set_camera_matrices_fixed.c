#include "common.h"

/* Camera matrices live in .bss as two Mtx4f (float[4][4]): proj @ 0x800E16B0,
   view @ 0x800E16F0. These view-matrix col-2 entries (view_matrix[i][2]) are
   the camera-space depth row. */
extern f32 D_800E16F8; /* view_matrix[0][2] */
extern f32 D_800E1708; /* view_matrix[1][2] */
extern f32 D_800E1718; /* view_matrix[2][2] */
extern f32 D_800E1728; /* view_matrix[3][2] */

INCLUDE_ASM("asm/nonmatchings/main/set_camera_matrices_fixed",
            set_camera_matrices_fixed);

INCLUDE_ASM("asm/nonmatchings/main/set_camera_matrices_fixed",
            set_camera_matrices_float);

INCLUDE_ASM("asm/nonmatchings/main/set_camera_matrices_fixed",
            frustum_cull_point_with_radius);

s32 project_point_view_depth(f32 x, f32 y, f32 z) {
  return (s32)(D_800E16F8 * x + D_800E1708 * y + D_800E1718 * z + D_800E1728);
}

INCLUDE_ASM("asm/nonmatchings/main/set_camera_matrices_fixed",
            project_point_to_screen);

INCLUDE_ASM("asm/nonmatchings/main/set_camera_matrices_fixed", func_80065898);

INCLUDE_ASM("asm/nonmatchings/main/set_camera_matrices_fixed", func_80065A1C);

INCLUDE_ASM("asm/nonmatchings/main/set_camera_matrices_fixed", mtx_from_rts);

/* 4x4 matrix multiply: dst = a * b (row-major, k-loop unrolled, in-place). */
void func_80065D5C(f32 a[4][4], f32 b[4][4], f32 dst[4][4]) {
  s32 i;
  s32 j;

  i = 0;
  do {
    j = 0;
    do {
      dst[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] +
                  a[i][3] * b[3][j];
      j++;
    } while (j != 4);
    i++;
  } while (i != 4);
}

/* Game-embedded guMtxF2L variant (a second copy; the canonical guMtxF2L lives
   at 0x80067B00). Packs a float matrix into a fixed-point Mtx (integer halves
   in the first 8 words, fraction halves in the next 8). Structurally ultralib
   gu/mtxutil.c guMtxF2L, but with `!=` loop bounds and no redundant
   `& 0xffff0000` on the fraction word (matches the -O2 game-profile bytes;
   stock ultralib's own build uses `<`/slti and keeps the mask). */
void convert_and_pack_floats_to_fixed(float mf[4][4], Mtx* m) {
  int i, j;
  int e1, e2;
  int *ai, *af;

  ai = (int*)&m->m[0][0];
  af = (int*)&m->m[2][0];

  for (i = 0; i != 4; i++)
    for (j = 0; j != 2; j++) {
      e1 = FTOFIX32(mf[i][j * 2]);
      e2 = FTOFIX32(mf[i][j * 2 + 1]);
      *(ai++) = (e1 & 0xffff0000) | ((e2 >> 16) & 0xffff);
      *(af++) = (e1 << 16) | (e2 & 0xffff);
    }
}

INCLUDE_ASM("asm/nonmatchings/main/set_camera_matrices_fixed", func_80065E6C);
