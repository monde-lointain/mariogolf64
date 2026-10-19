#include "common.h"
#include "../main/rom_asset.h"

extern void heap3_free(void** payload_ptr);
extern void func_80051A88(s32 volume);
extern s32 D_ovl0_80224A7C;
extern s32 D_ovl0_80224A80;
extern s32 D_ovl0_8022D678;
extern void* D_ovl0_8022B530;
extern void* heap3_alloc(u32 size);
extern void* D_ovl0_8022B534;
extern void* D_ovl0_8022B538;
extern void* D_ovl0_8022B53C;

extern s8 D_801061BD;
extern s8 D_801061BF;
extern s8 D_801061C0;
extern s8 D_801061C1;
extern s8 D_801061C2;
extern s8 D_801061C3;
extern s8 D_801061C4;
extern s8 D_801061C5;
extern u8 D_80106241;
extern u8 D_80106242;
extern u8 D_80106243;

void func_ovl0_80218CC0(void) {
    D_801061BD = 0;
    D_801061C4 = 0;
    D_801061BF = 0;
    D_80106241 = 0;
    D_80106242 = 0;
    D_80106243 = 0;
    D_801061C0 = 0;
    D_801061C1 = 0;
    D_801061C2 = 0;
    D_801061C3 = 0;
    D_801061C5 = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_80218D1C);

void func_ovl0_80218DB0(s32 arg0, s32 arg1) {
    if (arg1 == 0) {
        D_ovl0_80224A80 = 1;
    } else {
        D_ovl0_80224A80 = -1;
    }
    D_ovl0_8022D678 = arg0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_80218DD0);

void func_ovl0_80218FDC(void) {
    if (D_ovl0_80224A7C == 1) {
        D_ovl0_80224A7C = 0;
        heap3_free(&D_ovl0_8022B530);
        heap3_free(&D_ovl0_8022B534);
        heap3_free(&D_ovl0_8022B538);
    }
    func_80051A88(0x7F);
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_80219034);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_802199EC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_80219C74);

void func_ovl0_8021A62C(void) {
    RomAssetLoad asset;

    D_ovl0_8022B53C = heap3_alloc(get_rom_asset_allocation_size(0x70F, &asset));
    load_rom_asset_payload(D_ovl0_8022B53C, &asset);
    D_ovl0_8022B534 = heap3_alloc(get_rom_asset_allocation_size(0x71F, &asset));
    load_rom_asset_payload(D_ovl0_8022B534, &asset);
}

void func_ovl0_8021A690(void) {
    heap3_free(&D_ovl0_8022B53C);
    heap3_free(&D_ovl0_8022B534);
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_8021A6BC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_8021A724);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_8021A940);
