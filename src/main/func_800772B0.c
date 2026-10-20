#include "common.h"

extern s32 D_800C45D0;
extern s32 D_800C45D4;

void func_800772B0(void) {
  D_800C45D4 = 0;
  D_800C45D0 = 0;
}

INCLUDE_ASM("asm/nonmatchings/main/func_800772B0", func_800772C4);

/* Periodic cubic spline solver over 11 knots x[0..10] with values y[0..10];
 * x[10] wraps to x[0]. Writes out[0..10] in the layout func_800779A8 reads as
 * arg3. out[10] is read on entry and must hold the wrap-row RHS. */
void func_8007775C(f32* x, f32* y, f32* out) {
  f32 h[12];
  f32 d[12];
  f32 e[11];
  f32 r;
  s32 i;

  for (i = 0; i != 10; i++) {
    h[i] = x[i + 1] - x[i];
    e[i] = (y[i + 1] - y[i]) / h[i];
  }
  e[10] = e[0];
  for (i = 1; i != 10; i++) {
    d[i] = (x[i + 1] - x[i - 1]) + (x[i + 1] - x[i - 1]);
  }
  d[10] = (h[9] + h[0]) + (h[9] + h[0]);
  for (i = 1; i != 10; i++) {
    out[i] = e[i] - e[i - 1];
  }
  e[1] = h[0];
  e[9] = h[9];
  e[10] = d[10];
  for (i = 2; i != 9; i++) {
    e[i] = 0.0f;
  }
  for (i = 1; i != 10; i++) {
    r = h[i] / d[i];
    out[i + 1] = out[i + 1] - out[i] * r;
    d[i + 1] = d[i + 1] - h[i] * r;
    e[i + 1] = e[i + 1] - e[i] * r;
  }
  e[0] = e[10];
  out[0] = out[10];
  for (i = 8; i >= 0; i--) {
    r = h[i] / d[i + 1];
    out[i] = out[i] - out[i + 1] * r;
    e[i] = e[i] - e[i + 1] * r;
  }
  r = out[0] / e[0];
  out[0] = r;
  out[10] = r;
  for (i = 1; i != 10; i++) {
    out[i] = (out[i] - e[i] * r) / d[i];
  }
}

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
  while (x < arg1[0]) {
    x += step;
  }
  lo = 0;
  hi = 0xA;
  while (lo < hi) {
    mid = (lo + hi) / 2;
    if (arg1[mid] < x) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
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

/* Cubic finite-difference interpolation of the float table arg0 at the
 * fractional index arg1: samples v[0..2] = arg0[idx..idx+2] (idx = trunc arg1),
 * truncates them to integers ia0/ia1 and the forward differences d0/d1, and
 * evaluates a non-Horner cubic in frac = arg1 - idx. Computing ia0*3 as an
 * explicit temp reproduces the ROM's early hoist of that product, which reaches
 * exact instruction count AND steers local-alloc to the ROM's ia0/ia1/d0/d1 ->
 * $a2/$a0/$a1/$a3 coloring. */
f32 interp_cubic_finite_diff(f32* arg0, f32 arg1) {
  f32 v[3];
  s32 idx;
  f32 frac;
  s32 ia0;
  s32 ia1;
  s32 d0;
  s32 d1;
  s32 ia0_3;

  idx = (s32)arg1;
  v[0] = arg0[idx];
  v[1] = arg0[idx + 1];
  v[2] = arg0[idx + 2];
  frac = arg1 - (f32)idx;
  ia0 = (s32)v[0];
  ia1 = (s32)v[1];
  ia0_3 = ia0 * 3;
  d0 = (s32)(v[1] - v[0]);
  d1 = (s32)(v[2] - v[1]);
  return ((f32)(((ia0 - ia1) * 2) + d0 + d1) * frac * frac * frac) +
         ((f32)(((ia1 * 3) - ia0_3 - (d0 * 2)) - d1) * frac * frac) +
         ((f32)d0 * frac) + (f32)ia0;
}

s32 func_80077BD8(s32* arg0) { return *arg0 = *arg0; }
