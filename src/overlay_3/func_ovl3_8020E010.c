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

void func_ovl3_80210DF4(Gfx** gfxp, s32 x, s32 y, s32 value, u8 mode) {
    Gfx* gfx = *gfxp;

    func_ovl3_80210E3C(&gfx, x, y, value, mode, 5);
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80210E3C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80211A5C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_802123E0);

extern u8* D_ovl3_8023A82C;

void func_ovl3_80212858(Gfx** gfxp, s32 x, s32 y) {
    Gfx* gfx = *gfxp;
    s32 i;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, (u8*)(((u32)D_ovl3_8023A82C + 8) & ~7));
    for (i = 0; i != 2; i++) {
        gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                           (u8*)(((u32)D_ovl3_8023A82C + 0x828 + i * 0x80) & ~7));
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0,
                   0, 0, 0, 0, 0, 0);
        gDPLoadSync(gfx++);
        gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 63, 2048);
        gDPPipeSync(gfx++);
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_4b, 1, 0, G_TX_RENDERTILE, 0,
                   0, 0, 0, 0, 0, 0);
        gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 15 << G_TEXTURE_IMAGE_FRAC,
                       15 << G_TEXTURE_IMAGE_FRAC);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, (x + i * 0x10) << G_TEXTURE_IMAGE_FRAC,
                                y << G_TEXTURE_IMAGE_FRAC,
                                (x + i * 0x10 + 0x10) << G_TEXTURE_IMAGE_FRAC,
                                (y + 0x10) << G_TEXTURE_IMAGE_FRAC,
                                G_TX_RENDERTILE, 0, 0, 0x400, 0x400);
        gDPPipeSync(gfx++);
    }
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80212C54);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_80212FEC);
