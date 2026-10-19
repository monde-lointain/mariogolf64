#include "common.h"
#include "../main/rom_asset.h"

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_802178B0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_80217B88);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_80217D20);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021826C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_80219460);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_80219948);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_80219BD4);

extern u8* D_ovl3_8023B364;

void func_ovl3_8021B1E0(Gfx** gfxp, s32 x, s32 y, u8 idx) {
    Gfx* gfx = *gfxp;
    s32 texOff = idx * 0x500 + 0x28;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, (u8*)(((u32)D_ovl3_8023B364 + 8) & ~7));
    gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                       (u8*)(((u32)D_ovl3_8023B364 + texOff) & ~7));
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0,
               0, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 639, 256);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_4b, 8, 0, G_TX_RENDERTILE, 0, 0, 0, 0,
               0, 0, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 127 << G_TEXTURE_IMAGE_FRAC,
                   19 << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, (x - 0x40) << G_TEXTURE_IMAGE_FRAC,
                            y << G_TEXTURE_IMAGE_FRAC,
                            (x + 0x40) << G_TEXTURE_IMAGE_FRAC,
                            (y + 0x14) << G_TEXTURE_IMAGE_FRAC, G_TX_RENDERTILE,
                            0, 0, 0x400, 0x400);
    gDPPipeSync(gfx++);
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021B618);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021B87C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021BE88);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021C200);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021C67C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021CBDC);

extern u16 D_800C2064[];
extern void** D_ovl3_8023B630;

void func_ovl3_8021CF10(void** destinations) {
    RomAssetLoad asset;
    s16 i;

    D_ovl3_8023B630 = destinations;
    for (i = 0; i != 6; i++) {
        get_rom_asset_allocation_size(D_800C2064[i], &asset);
        load_rom_asset_payload(D_ovl3_8023B630[i], &asset);
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021CFAC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021D0B8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021D3AC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021E01C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021E340);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021E468);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021E4B0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021E97C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021EF64);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802178B0", func_ovl3_8021F124);
