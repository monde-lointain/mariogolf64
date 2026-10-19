#include "common.h"

extern f32 D_ovl7_8022BE30;
extern f32 D_ovl7_8022BE34;
extern f32 D_ovl7_8022BE38;
extern f32 D_ovl7_8022BE3C;
extern f32 D_ovl7_8022BE40;
extern f32 D_ovl7_8022BE44;
void func_ovl7_80203620(void);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_802033B0", func_ovl7_802033B0);

void func_ovl7_8020356C(f32 eye_x, f32 eye_y, f32 eye_z, f32 target_x, f32 target_y, f32 target_z) {
    D_ovl7_8022BE30 = eye_x;
    D_ovl7_8022BE34 = eye_y;
    D_ovl7_8022BE38 = eye_z;
    D_ovl7_8022BE3C = target_x;
    D_ovl7_8022BE40 = target_y;
    D_ovl7_8022BE44 = target_z;
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

extern f64 D_ovl7_8020C098;
extern f64 D_ovl7_8020C0A0;
extern f32 D_ovl7_8022BE48;
extern f32 D_ovl7_8022BE4C;
extern Ovl7TweenState D_ovl7_8022BE98;
extern Ovl7TweenStateB D_ovl7_8022BE94;
extern Ovl7Word D_ovl7_8022BEB0;
extern Ovl7Word D_ovl7_8022BEB4;
extern f32 D_ovl7_8022BED4;
extern f32 D_ovl7_8022BED0;
extern f32 fabsf(f32);
extern Ovl7Word D_ovl7_8022BEB8;
extern Ovl7Word D_ovl7_8022BEBC;
extern s32 D_ovl7_8022BEC0;
extern s32 D_ovl7_8022BEC4;

void func_ovl7_802035E4(s32 unused, s32 arg1, s32 arg2) {
  D_ovl7_8022BEB0.i = arg1;
  D_ovl7_8022BEB4.i = arg2;
}

void func_ovl7_802035F8(s32 unused, s32 arg1, s32 arg2) {
  D_ovl7_8022BEB8.i = arg1;
  D_ovl7_8022BEBC.i = arg2;
}

void func_ovl7_8020360C(s32 unused, s32 arg1, s32 arg2) {
  D_ovl7_8022BEC0 = arg1;
  D_ovl7_8022BEC4 = arg2;
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_802033B0", func_ovl7_80203620);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_802033B0", func_ovl7_8020370C);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_802033B0", func_ovl7_80203984);

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
    D_ovl7_8022BED4 = (f32) ((f64) arg1 * D_ovl7_8020C098);
    D_ovl7_8022BEB0.f = step;
    D_ovl7_8022BEB4.f = step / (f32) (quarter + 1);
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
    D_ovl7_8022BED0 = (f32) ((f64) arg1 * D_ovl7_8020C0A0);
    D_ovl7_8022BEB8.f = step;
    D_ovl7_8022BEBC.f = step / (f32) (quarter + 1);
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

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_802033B0", func_ovl7_80203E14);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_802033B0", func_ovl7_80203F70);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_802033B0", func_ovl7_80204EA8);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_802033B0", func_ovl7_802065FC);
