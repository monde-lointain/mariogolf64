#include "common.h"
#include "../main/rom_asset.h"

extern u16 D_800C2064[];
extern void** D_ovl3_8023A820;
extern u8 D_80105118[];
extern char D_ovl3_802318E0[];
extern s16 D_ovl3_802300A0;
extern char* D_ovl3_802300B4[];
extern char D_ovl3_802318A8[];

void func_ovl3_80210E3C(Gfx** gfxp, s32 x, s32 y, s32 value, u8 mode, u8 flag);
void func_80071608(Gfx** dl);
void func_80073F24(Gfx** dl, s16 x, s16 y, u8* str);
void func_80074CA8(Gfx** pgfx, s32 r, s32 g, s32 b);
u8* func_8005AF50(void);
u8* func_8005AF74(void);
void func_ovl3_80209478(void);
void func_ovl3_8020F9B8(Gfx** gfxp, s16 idx, u8 mode);

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

void func_ovl3_8020F794(Gfx** gfxp) {
    Gfx* gfx = *gfxp;
    s16 i;
    s16 next;
    s32 row;

    row = 0;
    func_8005AF50();
    func_8005AF74();
    func_ovl3_80209478();
    gDPSetPrimColor(gfx++, 0, 0, 0x40, 0x40, 0x40, 0xBF);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, 0x60, 0x228, 0x4B8, 0x398, G_TX_RENDERTILE, 0, 0, 0x400, 0x400);
    gDPPipeSync(gfx++);

    i = D_ovl3_802300A0;
    if (i != D_ovl3_802300A0 + 4) {
        do {
            func_80071608(&gfx);
            func_80074CA8(&gfx, 0xFF, 0xFF, 0xFF);
            sprintf((char*)D_80105118, D_ovl3_802318A8, D_ovl3_802300B4[i]);
            func_80073F24(&gfx, 0x1E, (s16)(((s32)(row << 16) >> 12) + 0x9A), D_80105118);
            func_ovl3_8020F9B8(&gfx, i, (u8)row);
            next = i + 1;
            i = next;
            row++;
        } while (next != D_ovl3_802300A0 + 4);
    }

    gDPPipeSync(gfx++);
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8020E010", func_ovl3_8020F9B8);

void func_ovl3_8020FD48(Gfx** gfxp, s16 x, s16 y) {
    Gfx* gfx = *gfxp;

    func_80071608(&gfx);
    func_80074CA8(&gfx, 0x3F, 0xFF, 0x3F);
    sprintf(D_80105118, D_ovl3_802318E0, 0x59);
    func_80073F24(&gfx, x, y, D_80105118);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    *gfxp = gfx;
}

void func_ovl3_8020FE20(Gfx** gfxp, s16 x, s16 y) {
    Gfx* gfx = *gfxp;

    func_80071608(&gfx);
    func_80074CA8(&gfx, 0xFF, 0x40, 0xC0);
    sprintf(D_80105118, D_ovl3_802318E0, 0x25);
    func_80073F24(&gfx, x, y, D_80105118);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    *gfxp = gfx;
}

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

extern u8* D_ovl3_8023A82C;

void func_ovl3_802123E0(Gfx** gfxp, s32 x, s32 y, u8 mode) {
    Gfx* gfx = *gfxp;
    s32 frame = 0;
    s32 texOff;

    switch (mode) {
    case 0:
        frame = 0xD;
        break;
    case 1:
        break;
    case 2:
        frame = 0xF;
        break;
    }

    texOff = (frame << 7) + 0x28;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, (u8*)(((u32)D_ovl3_8023A82C + 8) & ~7));
    gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                       (u8*)(((u32)D_ovl3_8023A82C + texOff) & ~7));
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0,
               0, 0, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 63, 2048);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_4b, 1, 0, G_TX_RENDERTILE, 0, 0, 0,
               0, 0, 0, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 15 << G_TEXTURE_IMAGE_FRAC,
                   15 << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, x << G_TEXTURE_IMAGE_FRAC,
                            y << G_TEXTURE_IMAGE_FRAC,
                            (x + 0x10) << G_TEXTURE_IMAGE_FRAC,
                            (y + 0x10) << G_TEXTURE_IMAGE_FRAC, G_TX_RENDERTILE,
                            0, 0, 0x400, 0x400);
    gDPPipeSync(gfx++);
    *gfxp = gfx;
}

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

void func_ovl3_80212C54(Gfx** gfxp, u8* tex, s32* bank, s32* counter, s32 spacing, s32 x, s32 y) {
    Gfx* gfx = *gfxp;
    s32 count;
    s32 left = 0;
    s32 top;
    s32 bankOff;
    s32 i;
    u8* texels;

    count = *counter + 1;
    *counter = count;
    if (count == 4) {
        *bank = (*bank == 0);
        *counter = 0;
    }
    bankOff = (*bank << 9) + 0x28;

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, (u8*)(((u32)tex + 8) & ~7));
    for (i = 0; i != 2; i++) {
        texels = (u8*)((((u32)tex) + (((i + 2) << 7) + bankOff)) & ~7);
        gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 8, texels);
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 1, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
        gDPLoadSync(gfx++);
        gDPLoadTile(gfx++, G_TX_LOADTILE, 0, 0, 30, 60);
        gDPPipeSync(gfx++);
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_4b, 1, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
        gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 15 << G_TEXTURE_IMAGE_FRAC,
                       15 << G_TEXTURE_IMAGE_FRAC);
        gDPPipeSync(gfx++);
        if (i == 0) {
            left = x + spacing / 2;
        }
        if (i == 1) {
            left = x - spacing / 2 - 0x10;
        }
        top = y - 8;
        gSPScisTextureRectangle(gfx++, left << G_TEXTURE_IMAGE_FRAC,
                                top << G_TEXTURE_IMAGE_FRAC,
                                (left + 0x10) << G_TEXTURE_IMAGE_FRAC,
                                (y + 8) << G_TEXTURE_IMAGE_FRAC, G_TX_RENDERTILE, 0, 0, 0x400,
                                0x400);
        gDPPipeSync(gfx++);
    }
    *gfxp = gfx;
}

void func_ovl3_80212FEC(Gfx** gfxp, u8* tex, s32 bank, s32 frame, s32 x, s32 y) {
    s32 bankIdx = bank;
    Gfx* gfx = *gfxp;
    s32 frameIdx;
    s32 bankOff;
    s32 left;
    s32 top;
    u8* texels;

    bankIdx = MAX(bankIdx, 0);
    frameIdx = frame;
    if (bankIdx >= 2) {
        bankIdx = 1;
    }
    frameIdx = MAX(frameIdx, 0);
    if (frameIdx >= 4) {
        frameIdx = 3;
    }
    left = x - 8;
    top = y - 8;
    bankOff = (bankIdx << 9) + 0x28;
    texels = (u8*)((((u32)tex) + ((frameIdx << 7) + bankOff)) & ~7);

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, (u8*)(((u32)tex + 8) & ~7));
    gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 8, texels);
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 1, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadTile(gfx++, G_TX_LOADTILE, 0, 0, 30, 60);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_4b, 1, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 15 << G_TEXTURE_IMAGE_FRAC,
                   15 << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, left << G_TEXTURE_IMAGE_FRAC, top << G_TEXTURE_IMAGE_FRAC,
                            (x + 8) << G_TEXTURE_IMAGE_FRAC,
                            (y + 8) << G_TEXTURE_IMAGE_FRAC, G_TX_RENDERTILE,
                            0, 0, 0x400, 0x400);
    gDPPipeSync(gfx++);
    *gfxp = gfx;
}
