#include "common.h"

extern s32 D_ovl16_801F89C0;
extern s32 D_ovl16_80223804;
extern s8 D_ovl16_802237B8;
extern s8 D_ovl16_802237B9;
extern s32 D_ovl16_802237C0[];
extern u8 D_801B724B;
extern s32 rand(void);
extern s32 flag_is_set(s32 flag);
extern f32 D_ovl16_801F89C8;

s32 func_ovl16_801F4A40(void) {
    return D_801B724B & 3;
}

void func_ovl16_801F4A88(s32 arg0) {
    void func_ovl16_801F4A50(s32 index0, s32 index1) {
        s32 tmp;

        tmp = D_ovl16_802237C0[index0];
        D_ovl16_802237C0[index0] = D_ovl16_802237C0[index1];
        D_ovl16_802237C0[index1] = tmp;
    }
    s32 i;
    s32 s0;
    s32 s1;

    for (i = 0; i < 17; i++) {
        D_ovl16_802237C0[i] = (i + arg0 + 2) % 18;
    }
    if (!flag_is_set(0x99)) {
        for (s1 = 0; s1 < 10; s1++) {
            for (s0 = 0; s0 < 16; s0++) {
                func_ovl16_801F4A50(s0, (rand() & 0x7FFF) % 16);
            }
        }
    }
}

void func_ovl16_801F4B64(s32 arg0) {
    D_ovl16_801F89C0 = -arg0;
    D_ovl16_80223804 = arg0;
}

void func_ovl16_801F4B7C(s32 arg0) {
    D_ovl16_801F89C0 = arg0;
    D_ovl16_80223804 = arg0;
}

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

void func_ovl16_801F4F7C(void) {
    D_ovl16_802237B8 = 0;
    D_ovl16_802237B9 = 0;
}

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

/* Dead 8-byte frame and `sw $v0,0x10(sp)` before `jal rand`: static-chain home of the nested-function original (callers set `addiu $v0,$sp,0x10` in the jal delay slot); `unused` reproduces them, as in func_ovl7_802068B4. */
void func_ovl16_801F53F0(void) {
    s32 r;
    volatile s32 unused = r;

    D_ovl16_801F89C8 = (f32) (((rand() & 0x7F) - 0x20) * 0x46) * 0.0001374275f;
}

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F544C);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F7148);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F7554);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F803C);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F80B8);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F8438);

void func_ovl16_801F898C(void) {}
