#include "common.h"

extern u8 D_801B724B;
extern s32 D_ovl16_801F89C0;
extern s32 D_ovl16_80223804;
extern u8 D_ovl16_802237B8;
extern u8 D_ovl16_802237B9;

void func_800560FC(s32 arg0, s32 arg1);
s32 func_ovl16_801F5138(s32 arg0);
s32 func_ovl16_801F5194(s32 arg0);

s32 func_ovl16_801F4A40(void) {
    return D_801B724B & 3;
}

INCLUDE_ASM("asm/nonmatchings/16D660", func_ovl16_801F4A50);

INCLUDE_ASM("asm/nonmatchings/16D660", func_ovl16_801F4A88);

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

INCLUDE_ASM("asm/nonmatchings/16D660", func_ovl16_801F4B9C);

INCLUDE_ASM("asm/nonmatchings/16D660", func_ovl16_801F4E54);

f32 func_ovl16_801F4F24(f32 arg0, f32 arg1, f32 arg2) {
    f32 var_ft0;

    var_ft0 = arg2;
    if (var_ft0 < 0.0f) {
        var_ft0 = 0.0f;
    }
    if (var_ft0 > 1.0f) {
        var_ft0 = 1.0f;
    }
    return (arg0 * (1.0f - var_ft0)) + (arg1 * var_ft0);
}

void func_ovl16_801F4F7C(void) {
    D_ovl16_802237B8 = 0;
    D_ovl16_802237B9 = 0;
}

INCLUDE_ASM("asm/nonmatchings/16D660", func_ovl16_801F4F90);

s32 func_800544B4(void);

/* The explicit case 0 is needed: it changes GCC's compare tree. */
s32 func_ovl16_801F5138(s32 arg0) {
    switch (func_800544B4()) {
        case 0:
            return 0;
        case 1:
            return 3;
        case 2:
            return 6;
        case 3:
            return 9;
        default:
            return 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/16D660", func_ovl16_801F5194);

void func_ovl16_801F5210(s32 arg0) {
    func_800560FC(arg0, func_ovl16_801F5138(arg0));
}

void func_ovl16_801F5240(s32 arg0) {
    func_800560FC(arg0, func_ovl16_801F5194(arg0));
}

INCLUDE_ASM("asm/nonmatchings/16D660", func_ovl16_801F5270);

INCLUDE_ASM("asm/nonmatchings/16D660", func_ovl16_801F53F0);

INCLUDE_ASM("asm/nonmatchings/16D660", func_ovl16_801F544C);

INCLUDE_ASM("asm/nonmatchings/16D660", func_ovl16_801F7148);

INCLUDE_ASM("asm/nonmatchings/16D660", func_ovl16_801F7554);

INCLUDE_ASM("asm/nonmatchings/16D660", func_ovl16_801F803C);

INCLUDE_ASM("asm/nonmatchings/16D660", func_ovl16_801F80B8);

INCLUDE_ASM("asm/nonmatchings/16D660", func_ovl16_801F8438);

void func_ovl16_801F898C(void) {
}
