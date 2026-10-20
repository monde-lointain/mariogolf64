#include "common.h"
#include "../main/rom_asset.h"

extern s32 D_ovl13_80243EE0;
extern s32 D_ovl13_80243EE4;
extern u8* func_8005AF50(void);
extern s32 flag_is_set(s32 flag);
extern void func_8006A2C0(Gfx** pgfx, s32 ulx, s32 uly, s32 lrx, s32 lry,
                          s32 tile, s32 s, s32 t, s32 dsdx, s32 dtdy);
extern void func_8006A548(Gfx** pgfx, u32 arg1);
extern void load_texture_block(Gfx** pgfx, u32 timg, s32 fmt, s32 siz, s32 width,
                               s32 height, s32 pal, s32 cms, s32 cmt, s32 masks,
                               s32 maskt, s32 shifts, s32 shiftt);
extern s32 D_ovl13_80243F50;
extern void* heap3_alloc(s32 size);

extern u16 D_ovl13_80243E60[13];
extern u16 D_ovl13_80243E7C[];
extern s32 D_ovl13_80243E98;
extern u8 D_ovl13_80243E9C;
extern u8 D_ovl13_80243E9D;
extern s32 D_ovl13_80243EA8;
extern s32 D_ovl13_80243EAC;
extern s32 D_ovl13_80243EB0;
extern s32 D_ovl13_80243EB4;
extern s32 D_ovl13_80243EB8;
extern s32 D_ovl13_80243EBC;
extern u8 D_ovl13_80243EC0;
extern char D_ovl13_80243EF0[];
extern void* D_ovl13_80243F58[13];
extern void* D_ovl13_80243F8C;
extern void* D_ovl13_80243F90;
extern void* D_ovl13_80243F94;
extern void* D_ovl13_80243F98;
extern void* D_ovl13_80243F9C;
extern void* D_ovl13_80243FA0;
extern void* D_ovl13_80243FA4;
extern void* D_ovl13_80243FA8;
extern void* D_ovl13_80243FAC;
extern void* D_ovl13_80243FB0;
extern s32 scenario_mode_id;

s32 func_ovl13_80241800(void) {
    u8* ret = func_8005AF50();
    s32 count = 0, i = 0, n = 6;
    u8* base = ret;
    do {
        u32 p = (u32)base + 0x1320;
        u32 end = n + p;
        do {
            count += (*(s8*)p > 0);
            p++;
        } while (p != end);
        base += 6;
        i++;
    } while (i != n);
    if (flag_is_set(0x5E)) {
        return 6;
    }
    if (count >= 0x14) {
        return 6;
    }
    if (count >= 0x10) {
        return 5;
    }
    if (count >= 0xC) {
        return 4;
    }
    if (count >= 8) {
        return 3;
    }
    if (count >= 4) {
        return 2;
    }
    return 1;
}

void func_ovl13_802418B8(s32 arg0) {
    RomAssetLoad asset;
    RomLoadSlot slot;
    s32 i;
    u32 size;

    if (D_ovl13_80243EA8 != 0) {
        osSyncPrintf(D_ovl13_80243EF0);
        return;
    }
    D_ovl13_80243EC0 = 0;
    if (arg0 == 0) {
        D_ovl13_80243EA8 = 1;
        D_ovl13_80243F50 = (s32)heap3_alloc(get_rom_asset_allocation_size(0x521, &asset));
        load_rom_asset_payload((void*)D_ovl13_80243F50, &asset);
        D_ovl13_80243F98 = heap3_alloc(get_rom_asset_allocation_size(0x5BE, &asset));
        load_rom_asset_payload(D_ovl13_80243F98, &asset);
        D_ovl13_80243F9C = heap3_alloc(get_rom_asset_allocation_size(0x5BE, &asset));
        load_rom_asset_payload(D_ovl13_80243F9C, &asset);
        D_ovl13_80243FA0 = heap3_alloc(get_rom_asset_allocation_size(0x5BE, &asset));
        load_rom_asset_payload(D_ovl13_80243FA0, &asset);
        D_ovl13_80243FA4 = heap3_alloc(get_rom_asset_allocation_size(0x5E5, &asset));
        load_rom_asset_payload(D_ovl13_80243FA4, &asset);
        D_ovl13_80243FA8 = heap3_alloc(get_rom_asset_allocation_size(0x5E5, &asset));
        load_rom_asset_payload(D_ovl13_80243FA8, &asset);
        D_ovl13_80243FAC = heap3_alloc(get_rom_asset_allocation_size(0x5E5, &asset));
        load_rom_asset_payload(D_ovl13_80243FAC, &asset);
        D_ovl13_80243FB0 = heap3_alloc(get_rom_asset_allocation_size(0x8ED, &asset));
        load_rom_asset_payload(D_ovl13_80243FB0, &asset);
    } else {
        D_ovl13_80243EA8 = 2;
        D_ovl13_80243EAC = 0x40;
        D_ovl13_80243F50 = (s32)heap3_alloc(get_rom_asset_allocation_size(0x739, &asset));
        load_rom_asset_payload((void*)D_ovl13_80243F50, &asset);
    }
    D_ovl13_80243EB8 = 0;
    D_ovl13_80243EBC = 0;
    D_ovl13_80243EB0 = scenario_mode_id;
    D_ovl13_80243EB4 = scenario_mode_id;
    for (i = 0; i != 13; i++) {
        D_ovl13_80243F58[i] = heap3_alloc(get_rom_asset_allocation_size(D_ovl13_80243E60[i], &asset));
        load_rom_asset_payload(D_ovl13_80243F58[i], &asset);
    }
    D_ovl13_80243F8C = heap3_alloc(get_rom_asset_allocation_size(D_ovl13_80243E7C[scenario_mode_id] + 2, &asset));
    load_rom_asset_payload(D_ovl13_80243F8C, &asset);
    open_rom_asset_slot(0x876, &slot);
    size = get_rom_load_slot_size(&slot);
    D_ovl13_80243F90 = heap3_alloc(size);
    read_rom_load_slot(D_ovl13_80243F90, size, &slot);
    D_ovl13_80243F94 = heap3_alloc(get_rom_asset_allocation_size(0x71E, &asset));
    load_rom_asset_payload(D_ovl13_80243F94, &asset);
    D_ovl13_80243E98 = 0;
    D_ovl13_80243E9C = 0;
    D_ovl13_80243E9D = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_80241BD8);

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_80241CE8);

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_80241DF0);

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_80241FF0);

s32 func_ovl13_80242374(void) {
    return D_ovl13_80243EE0 == 0;
}

void func_ovl13_80242384(s32 state) {
    if (D_ovl13_80243EE4 != state) {
        D_ovl13_80243EE4 = state;
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_802423A4);

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_802427E8);

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_80242D40);

void func_ovl13_80243610(Gfx** gfxp, s32 x, s32 y) {
    Gfx* gfx;
    s32 i;
    s32 yy;

    gfx = *gfxp;
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    func_8006A548(&gfx, D_ovl13_80243F50 + 8);
    for (i = 0; i != 3; i++) {
        load_texture_block(&gfx, D_ovl13_80243F50 + 0x208 + i * 0x7D0, G_IM_FMT_CI, G_IM_SIZ_8b, 0xC8, 0xA, 0, 0, 0, 0, 0, 0, 0);
        gDPPipeSync(gfx++);
        yy = y + i * 10;
        func_8006A2C0(&gfx, (x - 0x68) * 4, (yy - 0xF) * 4, (x + 0x68) * 4, (yy - 5) * 4, 0, 0, 0, 0x3D8, 0x400);
        gDPPipeSync(gfx++);
    }
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_80243784);
