#include "common.h"

void DrawMenuNumber(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4, u8 style);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020E010_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020E0AC_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020E26C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020E390_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020E898_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020F2D0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020F794_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020F9B8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020FD48_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020FE20_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_8020FEF8_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_80210190_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_802104F0_ovl_01);

void func_80210C18_ovl_01(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4) {
    Gfx *gfx = *displayListHead;

    if (value > 99) {
        value = 99;
    }
    if (value < -99) {
        value = -99;
    }
    DrawMenuNumber(&gfx, x, y, value, arg4, 0);
    *displayListHead = gfx;
}

void func_80210C74_ovl_01(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4) {
    Gfx *gfx = *displayListHead;

    if (value > 9) {
        value = 9;
    }
    if (value < -9) {
        value = -9;
    }
    DrawMenuNumber(&gfx, x, y, value, arg4, 1);
    *displayListHead = gfx;
}

void func_80210CD4_ovl_01(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4) {
    Gfx *gfx = *displayListHead;

    if (value > 99) {
        value = 99;
    }
    if (value < -99) {
        value = -99;
    }
    DrawMenuNumber(&gfx, x, y, value, arg4, 2);
    *displayListHead = gfx;
}

void func_80210D34_ovl_01(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4) {
    Gfx *gfx = *displayListHead;

    if (value > 999) {
        value = 999;
    }
    if (value < -999) {
        value = -999;
    }
    DrawMenuNumber(&gfx, x, y, value, arg4, 3);
    *displayListHead = gfx;
}

void func_80210D94_ovl_01(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4) {
    Gfx *gfx = *displayListHead;

    if (value > 9999) {
        value = 9999;
    }
    if (value < -9999) {
        value = -9999;
    }
    DrawMenuNumber(&gfx, x, y, value, arg4, 4);
    *displayListHead = gfx;
}

void func_80210DF4_ovl_01(Gfx **displayListHead, s32 x, s32 y, s32 value, u8 arg4) {
    Gfx *gfx = *displayListHead;

    DrawMenuNumber(&gfx, x, y, value, arg4, 5);
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", DrawMenuNumber);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_80211A5C_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_802123E0_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_80212858_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_80212C54_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/course_menu", func_80212FEC_ovl_01);
