#include "common.h"

typedef struct {
  f32 x, y, z;
} Vec3f;

INCLUDE_ASM("asm/nonmatchings/main/func_80076640", func_80076640);

void func_80076778(f32 mf[4][4], Vec3f* in, Vec3f* out) {
  f32 x = in->x;
  f32 y = in->y;
  f32 z = in->z;
  out->x = mf[0][0] * x + mf[1][0] * y + mf[2][0] * z + mf[3][0];
  out->y = mf[0][1] * x + mf[1][1] * y + mf[2][1] * z + mf[3][1];
  out->z = mf[0][2] * x + mf[1][2] * y + mf[2][2] * z + mf[3][2];
}

INCLUDE_ASM("asm/nonmatchings/main/func_80076640", func_8007680C);
