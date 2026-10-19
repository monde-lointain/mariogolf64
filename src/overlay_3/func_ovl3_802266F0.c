#include "common.h"
#include "../main/rom_asset.h"

extern u8* func_8005AF50(void);
extern u8* func_8005AF74(void);

extern s32 D_ovl3_80230E88;
extern f32 D_ovl3_80230E8C;
extern f32 D_ovl3_80230E90;
extern s32 D_ovl3_80230E9C;
extern s32 D_ovl3_80230EA0;

void func_ovl3_802266F0(void) {
    D_ovl3_80230E88 = 0;
    D_ovl3_80230E8C = 0.0f;
    D_ovl3_80230E90 = 0.0f;
    D_ovl3_80230E9C = 0;
    D_ovl3_80230EA0 = 0;
}

void func_ovl3_8022671C(void) {
    D_ovl3_80230E9C = 0;
    D_ovl3_80230EA0 = 0;
    D_ovl3_80230E88 = 1;
    D_ovl3_80230E8C = 1.0f;
    D_ovl3_80230E90 = 1.0f;
}

void func_ovl3_80226754(void) {
    D_ovl3_80230E88 = 2;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80226764);

s32 func_ovl3_80226788(void) {
    s32 ret;

    if (D_ovl3_80230E88 == 2) {
        ret = -1;
    } else {
        ret = D_ovl3_80230E88 == 3;
    }
    return ret;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_802267B0);

extern u16 D_800C2074[];
extern u8* D_ovl3_80230E7C;
extern u8* D_ovl3_80230E80;
extern u16 D_ovl3_80230EA4[];

void func_ovl3_80226904(s32 slot) {
    RomAssetLoad asset;
    s32 index;

    index = slot & ((s32) ~slot >> 0x1F);
    if (index >= 0xA) {
        index = 9;
    }
    get_rom_asset_allocation_size(D_800C2074[index], &asset);
    load_rom_asset_payload(D_ovl3_80230E7C, &asset);
}

void func_ovl3_80226958(s32 slot) {
    RomAssetLoad asset;
    s32 index;

    index = slot & ((s32) ~slot >> 0x1F);
    if (index >= 0xF) {
        index = 0xE;
    }
    get_rom_asset_allocation_size(D_ovl3_80230EA4[index], &asset);
    load_rom_asset_payload(D_ovl3_80230E80, &asset);
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_802269AC);

void func_ovl3_80226A34(void) {
  func_8005AF50();
  func_8005AF74();
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80226A58);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022747C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_802287EC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80228A80);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80228B60);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80228C00);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80229278);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_802296C4);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80229CB0);

extern u8* D_ovl3_80230F48;

void func_ovl3_8022A090(Gfx** gfxp) {
    Gfx* gfx = *gfxp;
    s32 i;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPLoadTLUT_pal256(gfx++, (u8*)(((u32)D_ovl3_80230F48 + 8) & ~7));
    gDPPipeSync(gfx++);
    for (i = 0; i != 4; i++) {
        gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                           (u8*)(((u32)D_ovl3_80230F48 + 0x208 + i * 0x800) & ~7));
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0,
                   0, 0, 0, 0, 0);
        gDPLoadSync(gfx++);
        gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 1023, 64);
        gDPPipeSync(gfx++);
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 32, 0, G_TX_RENDERTILE, 0, 0,
                   0, 0, 0, 0, 0);
        gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 255 << G_TEXTURE_IMAGE_FRAC,
                       7 << G_TEXTURE_IMAGE_FRAC);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, 32 << G_TEXTURE_IMAGE_FRAC,
                                (140 + i * 8) << G_TEXTURE_IMAGE_FRAC,
                                288 << G_TEXTURE_IMAGE_FRAC,
                                (148 + i * 8) << G_TEXTURE_IMAGE_FRAC,
                                G_TX_RENDERTILE, 0, 0, 0x400, 0x400);
        gDPPipeSync(gfx++);
    }
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022A43C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022A8F4);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022ADE8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022B360);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022B46C);

extern s32 D_ovl3_80230F68;
extern u8 D_ovl3_80230FD4[];

s32 func_ovl3_8022B5E0(void) {
    s32 var_a0;

    var_a0 = 1;
    if (D_ovl3_80230F68 > 0) {
        if (D_ovl3_80230F68 == 8) {
            D_ovl3_80230F68 = 7;
        }
        D_ovl3_80230FD4[D_ovl3_80230F68] = 0xFF;
        D_ovl3_80230F68 -= 1;
        var_a0 = 0;
    } else if (D_ovl3_80230FD4[0] != 0xFF) {
        D_ovl3_80230FD4[0] = 0xFF;
        var_a0 = 0;
    }
    return var_a0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022B668);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022B75C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022B7E0);

extern s8 D_80105DBC[];

s32 func_ovl3_8022BDB0(void) {
    return ~D_80105DBC[4] == 0;
}

s8 func_ovl3_8022BDC4(void) {
    return D_80105DBC[4];
}

extern u8 D_ovl3_80230F40;

s32 func_ovl3_8022BDD0(void) {
    if (D_ovl3_80230F40 != 0) {
        D_ovl3_80230F40 = 0;
        return 1;
    }
    return 0;
}

void func_ovl3_8022BDF8(void) {
}
