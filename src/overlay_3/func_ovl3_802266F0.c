#include "common.h"
#include "../main/rom_asset.h"

extern u8* func_8005AF50(void);
extern u8* func_8005AF74(void);
void func_80071608(Gfx **dl);
void func_800719A0(s32 x, s32 y, s32 run, s32 size, s32 index, s32 flags, u8 red, u8 green, u8 blue, u8 alpha);
void func_80071CE4(Gfx **dl, s32 i);

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

s32 func_ovl3_80226764(void) {
    s32 ret;

    if (D_ovl3_80230E88 == 0) {
        ret = -1;
    } else {
        ret = D_ovl3_80230E88 == 1;
    }
    return ret;
}

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

extern u8* D_ovl3_80230E84;
extern s16 D_ovl3_80230EC4[][4];
extern u32 get_rom_load_slot_size(RomLoadSlot* load_slot);

void func_ovl3_802269AC(s32 arg0, s32 arg1) {
    RomLoadSlot slot;

    arg0 = arg0 & ((s32) ~arg0 >> 0x1F);
    if (arg0 >= 0xF) {
        arg0 = 0xE;
    }
    arg1 = arg1 & ((s32) ~arg1 >> 0x1F);
    if (arg1 >= 4) {
        arg1 = 3;
    }
    open_rom_asset_slot((s32) D_ovl3_80230EC4[arg0][arg1], &slot);
    read_rom_load_slot(D_ovl3_80230E84,
                       (u32) (s16) get_rom_load_slot_size(&slot), &slot);
}

void func_ovl3_80226A34(void) {
  func_8005AF50();
  func_8005AF74();
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80226A58);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022747C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_802287EC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_80228A80);

extern s32 D_ovl3_80230F5C;
extern s32 D_ovl3_80230F60;
extern s32 D_ovl3_80230F64;
extern s32 D_ovl3_80230F68;
extern s32 D_ovl3_80230F6C;
extern s32 D_ovl3_80230F70;
extern s32 D_ovl3_80230F74;
extern s32 D_ovl3_80230F78;
extern s32 D_ovl3_80230F7C;
extern s8 D_ovl3_80230F80;
extern u8 D_ovl3_80230F40;
extern u8 D_ovl3_80230FD4[];

void func_ovl3_80228B60(void) {
    s32 i;
    u8* p;

    p = func_8005AF74();
    i = 0;
    p[0x47] = 0;
    p[0x48] = 0;
    D_ovl3_80230F74 = 0;
    do {
        D_ovl3_80230FD4[i] = 0xFF;
        i += 1;
    } while (i != 8);
    D_ovl3_80230F68 = 0;
    D_ovl3_80230F6C = 0;
    D_ovl3_80230F5C = 0;
    D_ovl3_80230F60 = 0;
    D_ovl3_80230F64 = 0;
    D_ovl3_80230F78 = 0;
    D_ovl3_80230F70 = 0;
    D_ovl3_80230F80 = 0;
    D_ovl3_80230F40 = 1;
    D_ovl3_80230F7C = 0;
}

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

extern u8* D_ovl3_80230F44;

void func_ovl3_8022A43C(Gfx** gfxp) {
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
    gDPLoadTLUT_pal256(gfx++, (u8*)(((u32)D_ovl3_80230F44 + 8) & ~7));
    gDPPipeSync(gfx++);
    for (i = 0; i != 3; i++) {
        gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                           (u8*)(((u32)D_ovl3_80230F44 + 0x208 + i * 0x7D0) & ~7));
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0,
                   0, 0, 0, 0, 0);
        gDPLoadSync(gfx++);
        gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 999, 82);
        gDPPipeSync(gfx++);
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 25, 0, G_TX_RENDERTILE, 0, 0,
                   0, 0, 0, 0, 0);
        gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 199 << G_TEXTURE_IMAGE_FRAC,
                       9 << G_TEXTURE_IMAGE_FRAC);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, 0x70, (56 + i * 32) << G_TEXTURE_IMAGE_FRAC,
                                0x490, (88 + i * 32) << G_TEXTURE_IMAGE_FRAC,
                                G_TX_RENDERTILE, 0, 0, 0x307, 0x140);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, 0x50, (160 + i * 20) << G_TEXTURE_IMAGE_FRAC,
                                0x4B0, (180 + i * 20) << G_TEXTURE_IMAGE_FRAC,
                                G_TX_RENDERTILE, 0, 0, 0x2DB, 0x200);
        gDPPipeSync(gfx++);
    }
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022A8F4);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022ADE8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022B360);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022B46C);

extern u8 D_ovl3_80230FDC[];

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

void func_ovl3_8022B668(Gfx **gfxp) {
    Gfx *gfx = *gfxp;
    s32 x = 0;
    s32 run;
    s32 flags;

    switch (D_ovl3_80230F6C) {
    case 0:
        run = 0;
        x = 0x38;
        flags = (-(func_ovl3_80226764() == 0) & 0x21) | 0x20;
        break;
    case 1:
        run = 1;
        x = 0x18;
        flags = 0x21;
        break;
    default:
        run = -1;
        flags = 0x21;
        break;
    }
    if (run >= 0) {
        gDPPipeSync(gfx++);
        func_80071608(&gfx);
        func_800719A0(x, 0xB4, run + 0x165, 1, 0, flags, 0xFF, 0xFF, 0xFF, 0xFF);
        func_80071CE4(&gfx, 0);
    }
    *gfxp = gfx;
}

s32 func_ovl3_8022B75C(void) {
    s32 i;
    s32 j;
    s32 cnt;
    s32 ret;
    u8 *row;
    u8 *p;

    ret = -1;
    for (i = 0; i != 0x24; i++) {
        cnt = 0;
        row = &D_ovl3_80230FDC[i * 8];
        for (j = 0, p = row; j != 8; j++, p++) {
            if (D_ovl3_80230FD4[j] != *p) {
                break;
            }
            cnt++;
        }
        if (cnt == 8) {
            ret = i;
            break;
        }
    }
    if (ret == -1) {
        D_ovl3_80230F6C = 1;
    }
    return ret;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022B7E0);

extern s8 D_80105DBC[];

s32 func_ovl3_8022BDB0(void) {
    return ~D_80105DBC[4] == 0;
}

s8 func_ovl3_8022BDC4(void) {
    return D_80105DBC[4];
}

s32 func_ovl3_8022BDD0(void) {
    if (D_ovl3_80230F40 != 0) {
        D_ovl3_80230F40 = 0;
        return 1;
    }
    return 0;
}

void func_ovl3_8022BDF8(void) {
}
