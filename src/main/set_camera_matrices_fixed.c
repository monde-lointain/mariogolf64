#include "common.h"

/* Camera matrices live in .bss as two Mtx4f (float[4][4]): proj @ 0x800E16B0,
   view @ 0x800E16F0. These view-matrix col-2 entries (view_matrix[i][2]) are
   the camera-space depth row. */
extern f32 view_matrix[4][4];

INCLUDE_ASM("asm/nonmatchings/main/set_camera_matrices_fixed",
            set_camera_matrices_fixed);

INCLUDE_ASM("asm/nonmatchings/main/set_camera_matrices_fixed",
            set_camera_matrices_float);

extern f32 D_800E16EC;
extern f32 persp_scale_x;
extern f32 D_800E1734;
extern s32 D_800E1738;
extern s32 screen_center_y;

s32 frustum_cull_point_with_radius(f32 x, f32 y, f32 z, f32 radius) {
  f32 depth;
  f32 horizontal;
  f32 vertical;
  f32 reciprocal;

  depth = view_matrix[0][2] * x + view_matrix[1][2] * y +
          view_matrix[2][2] * z + view_matrix[3][2];
  depth -= radius;
  if ((s32)depth >= 0) return 0;
  horizontal = view_matrix[0][0] * x + view_matrix[1][0] * y +
               view_matrix[2][0] * z + view_matrix[3][0];
  if ((s32)D_800E16EC == 1) {
    reciprocal = 1.0f;
  } else {
    reciprocal = 1.0f / depth;
  }
  if (horizontal > 0.0f) {
    horizontal -= radius;
    horizontal = persp_scale_x * horizontal * reciprocal;
    if ((f32)D_800E1738 < horizontal) return 1;
  } else {
    horizontal += radius;
    horizontal = persp_scale_x * horizontal * reciprocal;
    if (horizontal < (f32)-D_800E1738) return 1;
  }
  vertical = view_matrix[0][1] * x + view_matrix[1][1] * y +
             view_matrix[2][1] * z + view_matrix[3][1];
  if (vertical > 0.0f) {
    vertical -= radius;
    vertical = D_800E1734 * vertical * reciprocal;
    if (vertical < (f32)-screen_center_y) return 2;
  } else {
    vertical += radius;
    vertical = D_800E1734 * vertical * reciprocal;
    if ((f32)screen_center_y < vertical) return 2;
  }
  return (s32)depth;
}

s32 project_point_view_depth(f32 x, f32 y, f32 z) {
  return (s32)(view_matrix[0][2] * x + view_matrix[1][2] * y +
               view_matrix[2][2] * z + view_matrix[3][2]);
}

INCLUDE_ASM("asm/nonmatchings/main/set_camera_matrices_fixed",
            project_point_to_screen);

INCLUDE_ASM("asm/nonmatchings/main/set_camera_matrices_fixed", func_80065898);

INCLUDE_ASM("asm/nonmatchings/main/set_camera_matrices_fixed", func_80065A1C);

extern f32 sinf(f32);
extern f32 cosf(f32);

void mtx_from_rts(f32 matrix[4][4], f32 rotation[3], f32 translation[3],
                  f32 scale[3]) {
  f32 sx = sinf(rotation[0]);
  f32 cx = cosf(rotation[0]);
  f32 sy = sinf(rotation[1]);
  f32 cy = cosf(rotation[1]);
  f32 sz = sinf(rotation[2]);
  f32 cz = cosf(rotation[2]);
  f32 sx_sy = sx * sy;
  f32 cx_sy;

  matrix[0][0] = cy * cz * scale[0];
  cx_sy = cx * sy;
  matrix[0][1] = cy * sz * scale[0];
  matrix[0][2] = -sy * scale[0];
  matrix[0][3] = 0;
  matrix[1][0] = (sx_sy * cz - cx * sz) * scale[1];
  matrix[1][1] = (sx_sy * sz + cx * cz) * scale[1];
  matrix[1][2] = sx * cy * scale[1];
  matrix[1][3] = 0;
  matrix[2][0] = (cx_sy * cz + sx * sz) * scale[2];
  matrix[2][1] = (cx_sy * sz - sx * cz) * scale[2];
  matrix[2][2] = cx * cy * scale[2];
  matrix[2][3] = 0;
  matrix[3][0] = translation[0];
  matrix[3][1] = translation[1];
  matrix[3][2] = translation[2];
  matrix[3][3] = 1.0f;
}

/* 4x4 matrix multiply: dst = a * b (row-major, k-loop unrolled, in-place). */
void func_80065D5C(f32 a[4][4], f32 b[4][4], f32 dst[4][4]) {
  s32 i;
  s32 j;

  i = 0;
  while (i != 4) {
    j = 0;
    while (j != 4) {
      dst[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] +
                  a[i][3] * b[3][j];
      j++;
    }
    i++;
  }
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

extern f32 sqrtf(f32);

void func_80065E6C(Vtx* vertices, s32 index0, s32 index1, s32 index2,
                   s32 mode) {
  Vtx* a = (Vtx*)(index0 * sizeof(Vtx) + (u32)vertices);
  Vtx* b = (Vtx*)(index1 * sizeof(Vtx) + (u32)vertices);
  Vtx* c = (Vtx*)(index2 * sizeof(Vtx) + (u32)vertices);
  s32 y0 = a->v.ob[1];
  s32 y1 = b->v.ob[1];
  s32 z1 = b->v.ob[2];
  s32 z2 = c->v.ob[2];
  s32 dy1 = y1 - y0;
  s32 dz2 = z2 - z1;
  s32 dz1 = z1 - a->v.ob[2];
  s32 dx1;
  /* Reuse the loaded x coordinate as the edge scratch; a separate
   * subtraction temporary changes the retail register allocation. */
  s32 dx2 = c->v.ob[0] - (dx1 = b->v.ob[0]);
  s32 dy2 = c->v.ob[1] - y1;
  f32 nx;
  f32 ny;
  f32 nz;
  f32 scale;

  dx1 -= a->v.ob[0];
  nx = dy1 * dz2 - dz1 * dy2;
  ny = dz1 * dx2 - dx1 * dz2;
  nz = dx1 * dy2 - dy1 * dx2;
  scale = 127.0f / sqrtf(nx * nx + ny * ny + nz * nz);

  switch (mode) {
    case 0: {
      u8 x = (s32)(nx * scale);
      u8 y = (s32)(ny * scale);
      u8 z = (s32)(nz * scale);
      a->v.cn[0] = x;
      a->v.cn[1] = y;
      a->v.cn[2] = z;
      break;
    }
    case 1: {
      u8 x = (s32)(nx * scale);
      u8 y = (s32)(ny * scale);
      u8 z = (s32)(nz * scale);
      b->v.cn[0] = x;
      b->v.cn[1] = y;
      b->v.cn[2] = z;
      break;
    }
    case 2: {
      u8 x = (s32)(nx * scale);
      u8 y = (s32)(ny * scale);
      u8 z = (s32)(nz * scale);
      c->v.cn[0] = x;
      c->v.cn[1] = y;
      c->v.cn[2] = z;
      break;
    }
    case 3: {
      u8 x = (s32)(nx * scale);
      u8 y = (s32)(ny * scale);
      u8 z = (s32)(nz * scale);
      a->v.cn[0] = x;
      a->v.cn[1] = y;
      a->v.cn[2] = z;
      b->v.cn[0] = x;
      b->v.cn[1] = y;
      b->v.cn[2] = z;
      c->v.cn[0] = x;
      c->v.cn[1] = y;
      c->v.cn[2] = z;
      break;
    }
  }
}
