#include "common.h"

extern s32 D_ovl16_801F89C0;

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F4A40);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F4A50);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F4A88);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F4B64);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F4B7C);

void func_ovl16_801F4B90(void) {
    D_ovl16_801F89C0 = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F4B9C);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F4E54);

f32 func_ovl16_801F4F24(f32 near, f32 far, f32 t) {
  if (t < 0.0f) {
    t = 0.0f;
  }
  if (t > 1.0f) {
    t = 1.0f;
  }
  return near * (1.0f - t) + far * t;
}

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F4F7C);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F4F90);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F5138);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F5194);

extern void animation_play(s32 id, s32 animation);
s32 func_ovl16_801F5138(s32 arg0, s32 arg1);

void func_ovl16_801F5210(s32 arg0, s32 arg1) {
    animation_play(arg0, func_ovl16_801F5138(arg0, arg1));
}

s32 func_ovl16_801F5194(s32 arg0, s32 arg1, s32 arg2);

void func_ovl16_801F5240(s32 arg0, s32 arg1, s32 arg2) {
    animation_play(arg0, func_ovl16_801F5194(arg0, arg1, arg2));
}

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F5270);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F53F0);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F544C);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F7148);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F7554);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F803C);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F80B8);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F8438);

void func_ovl16_801F898C(void) {}
