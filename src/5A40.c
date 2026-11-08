#include "common.h"

extern s32 D_800B680C;
extern s32 D_800B6810;
extern s32 D_800B6818;
extern s32 D_800B7770;
extern s32 D_800B7774;
extern s32 D_800B7778;
extern s32 D_800B777C;
extern s32 D_800BB034;
extern s32 D_800BB038;
extern s32 D_800BB03C;
extern s32 D_801B552C;
extern u32 D_801B608C;

s32 func_80025D78(s32 arg0);
void func_80026230(s32 arg0);

INCLUDE_ASM("asm/nonmatchings/5A40", func_8002A640);

void func_8002A90C(Mtx *m, f32 x, f32 y, f32 z) {
    Mtx tmp;

    guTranslate(&tmp, x, y, z);
    guMtxCatL(m, &tmp, m);
}

void func_8002A944(Mtx *m, f32 x, f32 y, f32 z) {
    Mtx tmp;

    guScale(&tmp, x, y, z);
    guMtxCatL(m, &tmp, m);
}

void func_8002A97C(s32 arg0, s32 arg1) {
    D_800B680C = arg0;
    D_800B6810 = arg1;
    if (arg0 == 4) {
        D_801B552C = 1;
        D_800B6818 = -0x280;
    } else {
        D_801B552C = 3;
    }
}

INCLUDE_ASM("asm/nonmatchings/5A40", func_8002A9C4);

INCLUDE_ASM("asm/nonmatchings/5A40", func_8002ADD4);

INCLUDE_ASM("asm/nonmatchings/5A40", func_8002B280);

INCLUDE_ASM("asm/nonmatchings/5A40", func_8002B924);

void func_8002BD9C(void) {
}

INCLUDE_ASM("asm/nonmatchings/5A40", func_8002BDA4);

INCLUDE_ASM("asm/nonmatchings/5A40", func_8002BE78);

INCLUDE_ASM("asm/nonmatchings/5A40", func_8002BF1C);

INCLUDE_ASM("asm/nonmatchings/5A40", func_8002CDA8);

INCLUDE_ASM("asm/nonmatchings/5A40", func_8002DAC0);

INCLUDE_ASM("asm/nonmatchings/5A40", func_8002DC10);

INCLUDE_ASM("asm/nonmatchings/5A40", func_80031450);

INCLUDE_ASM("asm/nonmatchings/5A40", func_800318A8);

INCLUDE_ASM("asm/nonmatchings/5A40", func_80031AF4);

s32 func_80025F18(s32);
s32 func_800260C4(s32);
extern s32 D_800B7768;
extern s32 D_800B776C;
extern s32 D_800BB020;
extern s32 D_800BB024;
extern s32 D_800BB02C;
extern s32 D_800BB030;

void func_80032520(void) {
    if (D_800BB020 == 0x19) {
        if (D_800B7768 == 0) {
            D_800B7768 = 1;
            func_80026230(func_80025D78(D_800BB02C));
        } else if (func_80025F18(func_80025D78(D_800BB030)) != 0) {
            func_800260C4(func_80025D78(D_800BB030));
            D_800B7768 = 0;
            D_800BB020 = D_800BB024;
        }
    }
    if (D_800BB03C == 1) {
        if (D_800B776C == 0) {
            D_800B776C = D_800BB03C;
            func_80026230(func_80025D78(D_800BB034));
            return;
        }
        if (func_80025F18(func_80025D78(D_800BB038)) != 0) {
            func_800260C4(func_80025D78(D_800BB038));
            D_800BB03C = 0;
            D_800B776C = 0;
            D_800BB034 = D_800BB038;
        }
    }
}

s32 func_80032658(void) {
    return D_800BB03C == 0;
}

void func_80032668(void) {
    D_800BB03C = 1;
    switch (D_801B608C) {
        case 3:
        case 9:
            D_800BB038 = 0x15;
            break;
        case 11:
            D_800BB038 = 0x1A;
            break;
        default:
            D_800BB038 = 5;
            break;
    }
}

void func_800326C4(void) {
    func_80026230(func_80025D78(D_800BB034));
    D_800BB03C = -1;
}

void func_800326FC(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
    D_800B7778 = arg2;
    D_800B777C = arg3;
    D_800B7770 = arg0;
    D_800B7774 = arg1;
}

INCLUDE_ASM("asm/nonmatchings/5A40", func_80032720);
