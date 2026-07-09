#include "common.h"

extern s32 D_800C45D0;
extern s32 D_800C45D4;

void func_800772B0(void) {
  D_800C45D4 = 0;
  D_800C45D0 = 0;
}

INCLUDE_ASM("asm/nonmatchings/main/func_800772B0", func_800772C4);

INCLUDE_ASM("asm/nonmatchings/main/func_800772B0", func_8007775C);

f32 func_800779A8(f32 x, f32* arg1, f32* arg2, f32* arg3) {
  s32 mid;
  s32 lo;
  s32 hi;
  f32 dx;
  f32 q0;
  f32 r0;
  f32 r1;
  f32 step;
  f32 base;
  f32 t;

  step = arg1[10] - arg1[0];
  while (arg1[10] < x) {
    x -= step;
  }
  lo = 0;
  while (x < arg1[0]) {
    x += step;
  }
  lo = 0;
  hi = 0xA;
  do {
    mid = (lo + hi) / 2;
    if (arg1[mid] < x) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  } while (lo < hi);
  lo -= (lo > 0);
  base = arg1[lo];
  r1 = arg3[lo + 1];
  t = x - base;
  r0 = arg3[lo];
  dx = arg1[lo + 1] - base;
  q0 = arg2[lo];
  return (((((((r1 - r0) * t) / dx) + (r0 * 3.0f)) * t) +
           (((arg2[lo + 1] - q0) / dx) - (((r0 + r0) + r1) * dx))) *
          t) +
         q0;
}

INCLUDE_ASM("asm/nonmatchings/main/func_800772B0", func_80077AD4);

s32 func_80077BD8(s32* arg0) { return *arg0 = *arg0; }
