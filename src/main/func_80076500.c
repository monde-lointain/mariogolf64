#include "common.h"

typedef struct {
  f32 x, y, z;
} Vec3f;

extern Vec3f D_80105B6C;
extern Vec3f D_80105F30;
extern Vec3f D_800E1C50[];
extern f32 D_800E1CB0[];
extern s32 D_800E1CD0;
extern u16 D_800FBDA6[];
extern u8 D_800D1850[];

void func_80076500(Vec3f* a0, Vec3f* a1) {
  a0->x = D_80105B6C.x;
  a0->y = D_80105B6C.y;
  a0->z = D_80105B6C.z;
  a1->x = D_80105F30.x;
  a1->y = D_80105F30.y;
  a1->z = D_80105F30.z;
}

void func_8007654C(void) { D_800E1CD0 = 0; }

void func_80076558(Vec3f* a0, f32 a1) {
  s32 i = D_800E1CD0;

  if (i < 8) {
    D_800E1C50[i].x = a0->x;
    D_800E1C50[i].y = a0->y;
    D_800E1C50[i].z = a0->z;
    D_800E1CB0[i] = a1;
    if (D_800FBDA6[0] & 4) {
      osSyncPrintf((const char*)D_800D1850, (f64)D_800E1C50[i].x,
                   (f64)D_800E1C50[i].y, (f64)D_800E1C50[i].z);
    }
    D_800E1CD0 = D_800E1CD0 + 1;
  }
}
