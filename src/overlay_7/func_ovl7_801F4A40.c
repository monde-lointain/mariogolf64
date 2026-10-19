#include "common.h"
#include "../main/rom_asset.h"

void func_80058D04(s32, s32, s32);
extern Gfx* glistp;
extern s32 D_ovl7_8020AC4C;
extern u8 D_ovl7_80213A40[];
extern u8 D_ovl7_80212830[];
extern u16 D_ovl7_8020AD8A[];
extern u8 D_ovl7_80214C50[];
extern u32 D_ovl7_8020AD70[];
extern f32 D_ovl7_8020AD80[];
extern f64 cos(f64);
extern f64 sin(f64);
extern s32 D_ovl7_8020ADA0[2];
extern s32 D_ovl7_8020ADA8;
extern s32 D_ovl7_8020B210;
extern f64 D_ovl7_8020C020;
extern f64 D_ovl7_8020C028;
extern f64 D_ovl7_8020C030;
extern u16 D_ovl7_8020AC9A[][14];
extern u8 D_ovl7_8020E400[];
extern s32 D_ovl7_80212810[2];
extern s32 D_ovl7_80212818[2];
extern s32 D_ovl7_80212820[2];

void func_ovl7_801F4A40(void) {
    D_ovl7_8020AC4C++;
    if (D_ovl7_8020AC4C == 11) {
        D_ovl7_8020AC4C = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801F4A6C);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801F4AEC);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801F5018);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801F5100);

void func_ovl7_801F5180(s32 view_state_address, Gfx** cursor) {
    s32 slot_index;

    slot_index = 0;
    glistp = *cursor;
    do {
        func_80058D04(view_state_address, 2, slot_index);
        slot_index += 1;
    } while (slot_index < 4);
    *cursor = glistp;
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801F51F0);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801F5454);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801F58EC);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801F6788);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FA4EC);

void func_ovl7_801FA83C(s32 table_index, s32 slot_index) {
    RomAssetLoad asset;

    get_rom_asset_allocation_size(D_ovl7_8020AC9A[table_index][0], &asset);
    load_rom_asset_payload(D_ovl7_8020E400 + slot_index * 0x2208, &asset);
    D_ovl7_80212810[slot_index] = table_index;
    D_ovl7_80212818[slot_index] = 0;
    D_ovl7_80212820[slot_index] = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FA8D8);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FB864);

void func_ovl7_801FBB10(s32 arg0, s32 arg1) {
    RomAssetLoad asset;

    get_rom_asset_allocation_size(D_ovl7_8020AD8A[arg0 * 2], &asset);
    load_rom_asset_payload(D_ovl7_80214C50 + arg1 * 0x1208, &asset);
    D_ovl7_8020AD70[arg1] = 0;
    D_ovl7_8020AD80[arg1] = 1.0f;
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FBB94);

void func_ovl7_801FE270(void) {
    RomAssetLoad asset;

    get_rom_asset_allocation_size(0x908, &asset);
    load_rom_asset_payload(D_ovl7_80212830, &asset);
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FE2A0);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FE7B4);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FE8F8);

void func_ovl7_801FF5E8(void) {
    f32 angle;
    s32 phase;
    s32 scaled_cos;
    s32 x;
    s32 y;

    if (D_ovl7_8020ADA8 != 0) {
        phase = D_ovl7_8020B210;
        angle = (f64)phase * D_ovl7_8020C020;
        phase += 0xC;
        D_ovl7_8020B210 = phase;
        if (phase >= 0x21D) {
            D_ovl7_8020ADA8 = 0;
        }
        x = D_ovl7_8020ADA0[0] + 8;
        scaled_cos = (s32)(cos((f64)angle) * D_ovl7_8020C028);
        y = D_ovl7_8020ADA0[1] - 2;
        D_ovl7_8020ADA0[0] = x + scaled_cos;
        D_ovl7_8020ADA0[1] = y - (s32)(sin((f64)angle) * D_ovl7_8020C030);
    }
}

void func_ovl7_801FF6DC(void) {
    RomAssetLoad asset;

    get_rom_asset_allocation_size(0x93D, &asset);
    load_rom_asset_payload(D_ovl7_80212830, &asset);
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FF70C);

void func_ovl7_801FFC10(void) {
    RomAssetLoad asset;

    get_rom_asset_allocation_size(0x939, &asset);
    load_rom_asset_payload(D_ovl7_80213A40, &asset);
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FFC40);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_802008D8);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_80200B18);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_80200CBC);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_80202D34);
