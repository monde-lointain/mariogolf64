#include "common.h"
#include "../main/vector.h"

extern GolfCamera D_ovl7_8022BE30;
void func_ovl7_80203620(void);
void func_ovl7_8020370C(s32 idx, f32 x, f32 y, f32 z, s32 frames);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_802033B0", func_ovl7_802033B0);

void func_ovl7_8020356C(f32 eye_x, f32 eye_y, f32 eye_z, f32 target_x, f32 target_y, f32 target_z) {
    D_ovl7_8022BE30.eye.x = eye_x;
    D_ovl7_8022BE30.eye.y = eye_y;
    D_ovl7_8022BE30.eye.z = eye_z;
    D_ovl7_8022BE30.at.x = target_x;
    D_ovl7_8022BE30.at.y = target_y;
    D_ovl7_8022BE30.at.z = target_z;
    func_ovl7_80203620();
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_802033B0", func_ovl7_802035BC);

typedef union {
  s32 i;
  f32 f;
} Ovl7Word;

typedef struct {
  f32 value;
  u8 pad4[0x40];
  s32 flags[2];
} Ovl7TweenState;

typedef struct {
  f32 value;
  u8 pad4[0x44];
  s32 flags[2];
} Ovl7TweenStateB;

typedef struct {
  f32 x;
  f32 y;
  f32 z;
} Ovl7Vec3;

typedef struct {
  Ovl7Vec3 vecA[2];
  Ovl7Vec3 vecB[2];
  u8 pad30[0x18];
  f32 fA[2];
  f32 fB[2];
  Ovl7Word fC[4];
  Ovl7Word fD[2];
  s32 iA[2];
  f32 fE[3];
  s32 flags[2];
  s32 iB[2];
} Ovl7Region;

extern f64 D_ovl7_8020C098;
extern f64 D_ovl7_8020C0A0;
extern f32 D_ovl7_8022BE48;
extern f32 D_ovl7_8022BE4C;
extern Ovl7TweenState D_ovl7_8022BE98;
extern Ovl7TweenStateB D_ovl7_8022BE94;
extern Ovl7Region D_ovl7_8022BE58;
extern f32 fabsf(f32);

void func_ovl7_802035E4(s32 unused, s32 arg1, s32 arg2) {
  D_ovl7_8022BE58.fC[0].i = arg1;
  D_ovl7_8022BE58.fC[1].i = arg2;
}

void func_ovl7_802035F8(s32 unused, s32 arg1, s32 arg2) {
  D_ovl7_8022BE58.fC[2].i = arg1;
  D_ovl7_8022BE58.fC[3].i = arg2;
}

void func_ovl7_8020360C(s32 unused, s32 arg1, s32 arg2) {
  D_ovl7_8022BE58.fD[0].i = arg1;
  D_ovl7_8022BE58.fD[1].i = arg2;
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_802033B0", func_ovl7_80203620);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_802033B0", func_ovl7_8020370C);

void func_ovl7_80203984(s32 idx, f32 x, f32 y, f32 z, s32 frames) {
  s32 *flags;
  s32 *active;
  s32 *inactive;
  s32 word;

  flags = D_ovl7_8022BE58.flags;
  active = &flags[idx];
  word = *active;
  if (!(word & 0x80)) {
    *active = word ^ 0x80;
  }
  inactive = &flags[idx ^ 1];
  word = *inactive;
  if (word & 0x80) {
    *inactive = word ^ 0x80;
  }
  word = *active;
  if (!(word & 8)) {
    *active = word ^ 8;
  }
  word = *inactive;
  if (word & 8) {
    *inactive = word ^ 8;
  }
  func_ovl7_8020370C(idx, x, y, z, frames);
}

void func_ovl7_80203A30(s32 arg0, f32 arg1, s32 arg2) {
  f32 *accum;
  Ovl7TweenState *state;
  s32 *flags;
  s32 *active;
  s32 *inactive;
  s32 word;
  s32 quarter;
  f32 absolute;
  f32 threshold;
  f32 step;

  absolute = fabsf(arg1);
  /* Keep the threshold load after abs.s, matching the target's COP1 hazard slot. */
  do {
    threshold = 0.001f;
  } while (0);
  if (absolute < threshold) {
    accum = &D_ovl7_8022BE48;
    *accum += arg1;
    return;
  }
  if (arg2 != 0) {
    quarter = arg2 / 4;
    step = arg1 / (f32) (arg2 - quarter);
    D_ovl7_8022BE58.fE[1] = (f32) ((f64) arg1 * D_ovl7_8020C098);
    D_ovl7_8022BE58.fC[0].f = step;
    D_ovl7_8022BE58.fC[1].f = step / (f32) (quarter + 1);
  }
  state = &D_ovl7_8022BE98;
  state->value = arg1;
  flags = state->flags;
  active = &flags[arg0];
  word = *active;
  if (!(word & 8)) {
    *active = word ^ 8;
  }
  inactive = &flags[arg0 ^ 1];
  word = *inactive;
  if (word & 8) {
    *inactive = word ^ 8;
  }
  word = *active;
  if (word & 0x10) {
    *active = word ^ 0x10;
  }
  word = *inactive;
  if (!(word & 0x10)) {
    *inactive = word ^ 0x10;
  }
}

void func_ovl7_80203B60(s32 arg0, f32 arg1, s32 arg2) {
  f32 *accum;
  Ovl7TweenStateB *state;
  s32 *flags;
  s32 *active;
  s32 *inactive;
  s32 word;
  s32 quarter;
  f32 absolute;
  f32 threshold;
  f32 step;

  absolute = fabsf(arg1);
  do {
    threshold = 0.001f;
  } while (0);
  if (absolute < threshold) {
    accum = &D_ovl7_8022BE4C;
    *accum += arg1;
    return;
  }
  if (arg2 != 0) {
    quarter = arg2 / 4;
    step = arg1 / (f32) (arg2 - quarter);
    D_ovl7_8022BE58.fE[0] = (f32) ((f64) arg1 * D_ovl7_8020C0A0);
    D_ovl7_8022BE58.fC[2].f = step;
    D_ovl7_8022BE58.fC[3].f = step / (f32) (quarter + 1);
  }
  state = &D_ovl7_8022BE94;
  state->value = arg1;
  flags = state->flags;
  active = &flags[arg0];
  word = *active;
  if (!(word & 8)) {
    *active = word ^ 8;
  }
  inactive = &flags[arg0 ^ 1];
  word = *inactive;
  if (word & 8) {
    *inactive = word ^ 8;
  }
  word = *active;
  if (word & 0x20) {
    *active = word ^ 0x20;
  }
  word = *inactive;
  if (!(word & 0x20)) {
    *inactive = word ^ 0x20;
  }
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_802033B0", func_ovl7_80203C90);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_802033B0", func_ovl7_80203DC4);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_802033B0", func_ovl7_80203DE8);

void func_ovl7_80203E14(void) {
  s32 i;

  i = 0;
  do {
    D_ovl7_8022BE58.vecA[i].x = 0.0f;
    D_ovl7_8022BE58.vecA[i].y = 0.0f;
    D_ovl7_8022BE58.vecA[i].z = 0.0f;
    D_ovl7_8022BE58.vecB[i].x = 0.0f;
    D_ovl7_8022BE58.vecB[i].y = 0.0f;
    D_ovl7_8022BE58.vecB[i].z = 0.0f;
    D_ovl7_8022BE58.vecB[i].x = 0.0f;
    D_ovl7_8022BE58.vecB[i].y = 0.0f;
    D_ovl7_8022BE58.vecB[i].z = 0.0f;
    D_ovl7_8022BE58.fA[i] = 30.0f;
    D_ovl7_8022BE58.fB[i] = 3.0f;
    D_ovl7_8022BE58.iA[i] = 0;
    D_ovl7_8022BE58.iB[i] = 0;
    D_ovl7_8022BE58.flags[i] = 0;
    i++;
  } while (i != 2);
  D_ovl7_8022BE58.fE[0] = 0.0f;
  D_ovl7_8022BE58.fE[1] = 0.0f;
  D_ovl7_8022BE58.fE[2] = 0.0f;
  D_ovl7_8022BE58.fC[0].f = 0.104719758f;
  D_ovl7_8022BE58.fC[1].f = 0.0349065848f;
  D_ovl7_8022BE58.fC[2].f = 0.052359879f;
  D_ovl7_8022BE58.fC[3].f = 0.0174532924f;
  D_ovl7_8022BE58.fD[0].f = 10.0f;
  D_ovl7_8022BE58.fD[1].f = 1.0f;
  func_ovl7_80203620();
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_802033B0", func_ovl7_80203F70);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_802033B0", func_ovl7_80204EA8);

void func_ovl7_802065FC(Vec3f* delta) {
    D_ovl7_8022BE30.at.x += delta->x;
    D_ovl7_8022BE30.at.y += delta->y;
    D_ovl7_8022BE30.at.z += delta->z;
    D_ovl7_8022BE30.eye.x += delta->x;
    D_ovl7_8022BE30.eye.y += delta->y;
    D_ovl7_8022BE30.eye.z += delta->z;
}
