#include "common.h"
#include "vector.h"
#include "libultra/PR/gu.h"

extern f32 D_800E1CD4;
extern f32 D_800E1CD8;
extern f32 D_800E1CE0[4];
extern f64 D_800D1868;
extern f32 cosf(f32);
extern f32 fabsf(f32);

void func_80076640(f32 pitch, f32 yaw) {
  f32 mtxPitch[4][4];
  f32 mtxYaw[4][4];
  f32 cosPitch;
  register f32 absCosPitch __asm__("$f2");
  register f32 threshold __asm__("$f0");
  register f64 radiansPerDegree __asm__("$f22");

  cosPitch = cosf(pitch);
  absCosPitch = fabsf(cosPitch);
  threshold = 0.1f;
  do {
    if (absCosPitch < threshold) {
      pitch += 0.3490659f;
    }
  } while (0);
  radiansPerDegree = D_800D1868;
  D_800E1CD4 = pitch;
  D_800E1CD8 = yaw;
  guRotateF(mtxPitch, (f32)(s32)((f64)pitch * radiansPerDegree), -1.0f, 0.0f,
            0.0f);
  guRotateF(mtxYaw, (f32)(s32)((f64)yaw * radiansPerDegree), 0.0f, -1.0f,
            0.0f);
  guMtxCatF(mtxYaw, mtxPitch, D_800E1CE0);
}

void transform_point_by_matrix(f32 mf[4][4], Vec3f* in, Vec3f* out) {
  f32 x = in->x;
  f32 y = in->y;
  f32 z = in->z;
  out->x = mf[0][0] * x + mf[1][0] * y + mf[2][0] * z + mf[3][0];
  out->y = mf[0][1] * x + mf[1][1] * y + mf[2][1] * z + mf[3][1];
  out->z = mf[0][2] * x + mf[1][2] * y + mf[2][2] * z + mf[3][2];
}

INCLUDE_ASM("asm/nonmatchings/main/func_80076640", func_8007680C);
