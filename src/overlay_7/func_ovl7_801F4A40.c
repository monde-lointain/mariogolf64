#include "common.h"
#include "../main/rom_asset.h"

void func_80058D04(s32, s32, s32);
extern Gfx* glistp;
extern s32 D_ovl7_8020AC4C;
extern u8 D_ovl7_80213A40[];
extern u8 D_ovl7_80212830[];

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

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FA83C);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FA8D8);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FB864);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FBB10);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FBB94);

void func_ovl7_801FE270(void) {
    RomAssetLoad asset;

    get_rom_asset_allocation_size(0x908, &asset);
    load_rom_asset_payload(D_ovl7_80212830, &asset);
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FE2A0);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FE7B4);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FE8F8);

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_801F4A40", func_ovl7_801FF5E8);

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
