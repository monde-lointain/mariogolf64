#include "common.h"

void func_ovl3_80210E3C(Gfx** gfxp, s32 x, s32 y, s32 value, u8 mode, u8 flag);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_8020E010);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_8020E0AC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_8020E26C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_8020E390);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_8020E898);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_8020F2D0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_8020F794);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_8020F9B8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_8020FD48);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_8020FE20);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_8020FEF8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80210190);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_802104F0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80210C18);

void func_ovl3_80210C74(Gfx** gfxp, s32 x, s32 y, s32 value, u8 mode) {
    Gfx* gfx = *gfxp;

    if (value > 9) {
        value = 9;
    }
    if (value < -9) {
        value = -9;
    }
    func_ovl3_80210E3C(&gfx, x, y, value, mode, 1);
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80210CD4);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80210D34);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80210D94);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80210DF4);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80210E3C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80211A5C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_802123E0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80212858);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80212C54);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80212FEC);
