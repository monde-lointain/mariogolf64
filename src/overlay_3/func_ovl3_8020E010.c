#include "common.h"
#include "../main/rom_asset.h"

extern u16 D_800C2064[];
extern void** D_ovl3_8023A820;

void func_ovl3_80210E3C(Gfx** gfxp, s32 x, s32 y, s32 value, u8 mode, u8 flag);

void func_ovl3_8020E010(void** destinations) {
    RomAssetLoad asset;
    RomLoadSlot slot;
    s16 i;

    D_ovl3_8023A820 = destinations;
    for (i = 0; i != 7; i++) {
        get_rom_asset_allocation_size(D_800C2064[i], &asset);
        load_rom_asset_payload(D_ovl3_8023A820[i], &asset);
    }
}


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

void func_ovl3_80210C18(Gfx** gfxp, s32 x, s32 y, s32 value, u8 mode) {
    Gfx* gfx = *gfxp;

    if (value > 99) {
        value = 99;
    }
    if (value < -99) {
        value = -99;
    }
    func_ovl3_80210E3C(&gfx, x, y, value, mode, 0);
    *gfxp = gfx;
}

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

void func_ovl3_80210CD4(Gfx** gfxp, s32 x, s32 y, s32 value, u8 mode) {
    Gfx* gfx = *gfxp;

    if (value > 99) {
        value = 99;
    }
    if (value < -99) {
        value = -99;
    }
    func_ovl3_80210E3C(&gfx, x, y, value, mode, 2);
    *gfxp = gfx;
}

void func_ovl3_80210D34(Gfx** gfxp, s32 x, s32 y, s32 value, u8 mode) {
    Gfx* gfx = *gfxp;

    if (value > 999) {
        value = 999;
    }
    if (value < -999) {
        value = -999;
    }
    func_ovl3_80210E3C(&gfx, x, y, value, mode, 3);
    *gfxp = gfx;
}

void func_ovl3_80210D94(Gfx** gfxp, s32 x, s32 y, s32 value, u8 mode) {
    Gfx* gfx = *gfxp;

    if (value > 9999) {
        value = 9999;
    }
    if (value < -9999) {
        value = -9999;
    }
    func_ovl3_80210E3C(&gfx, x, y, value, mode, 4);
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80210DF4);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80210E3C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80211A5C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_802123E0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80212858);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80212C54);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80212FEC);
