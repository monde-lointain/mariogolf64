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

extern u16 D_800C2074[];
extern u8* D_ovl3_80230E70;
extern u8* D_ovl3_80230E74;
extern u8* D_ovl3_80230E78;
extern u8* D_ovl3_80230E7C;
extern u8* D_ovl3_80230E80;
extern u8* D_ovl3_80230E84;
extern u16 D_ovl3_80230EC0;
extern s16 D_ovl3_80230F34;

void func_ovl3_802267B0(u8* block, s32 slot_index) {
    RomAssetLoad asset;
    RomLoadSlot slot;
    s16 index;

    func_8005AF74();
    index = slot_index - 1;
    switch (index) {
    case 0:
        D_ovl3_80230E70 = block;
        get_rom_asset_allocation_size(0x70B, &asset);
        load_rom_asset_payload(D_ovl3_80230E70, &asset);
        break;
    case 1:
        D_ovl3_80230E74 = block;
        get_rom_asset_allocation_size(0x70C, &asset);
        load_rom_asset_payload(D_ovl3_80230E74, &asset);
        break;
    case 2:
        D_ovl3_80230E78 = block;
        get_rom_asset_allocation_size(0x949, &asset);
        load_rom_asset_payload(D_ovl3_80230E78, &asset);
        break;
    case 3:
        D_ovl3_80230E7C = block;
        get_rom_asset_allocation_size(D_800C2074[0], &asset);
        load_rom_asset_payload(D_ovl3_80230E7C, &asset);
        break;
    case 4:
        D_ovl3_80230E80 = block;
        get_rom_asset_allocation_size(D_ovl3_80230EC0, &asset);
        load_rom_asset_payload(D_ovl3_80230E80, &asset);
        break;
    case 5:
        D_ovl3_80230E84 = block;
        open_rom_asset_slot(D_ovl3_80230F34, &slot);
        read_rom_load_slot(D_ovl3_80230E84, (u32) (s16) get_rom_load_slot_size(&slot), &slot);
        break;
    }
    func_ovl3_802266F0();
}

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

void func_8006A2C0(Gfx **pgfx, s32 ulx, s32 uly, s32 lrx, s32 lry, s32 tile, s32 s, s32 t, s32 dsdx, s32 dtdy);
void func_8006A548(Gfx **pgfx, u32 arg1);
void load_texture_block(Gfx **pgfx, u32 timg, s32 fmt, s32 siz, s32 width, s32 height, s32 pal, s32 cms, s32 cmt, s32 masks, s32 maskt, s32 shifts, s32 shiftt);
void load_texture_block_4b(Gfx **pgfx, u32 timg, s32 fmt, s32 width, s32 height, s32 pal, s32 cms, s32 cmt, s32 masks, s32 maskt, s32 shifts, s32 shiftt);

void func_ovl3_80226A58(Gfx **gfxp, s32 width, s32 height, s32 x, s32 y, s32 label, s32 selected) {
    Gfx *gfx;
    f32 w;
    f32 h;
    f32 bw;
    f32 bh;
    s32 hw;
    s32 hh;
    s32 hbw;
    s32 hbh;
    s32 top;
    s32 row;
    s32 bx;
    s32 by;
    s32 grow;
    s32 pulse;
    s16 i;

    gfx = *gfxp;
    func_8005AF74();
    switch (D_ovl3_80230E88) {
    case 0:
        D_ovl3_80230E8C += 0.4f;
        D_ovl3_80230E90 += 0.4f;
        if (D_ovl3_80230E8C > 1.0f) {
            D_ovl3_80230E8C = 1.0f;
        }
        if (D_ovl3_80230E90 > 1.0f) {
            D_ovl3_80230E90 = 1.0f;
        }
        w = width * D_ovl3_80230E8C;
        h = (height / 2) * D_ovl3_80230E90;
        bw = D_ovl3_80230E8C * 48.0f;
        bh = D_ovl3_80230E90 * 16.0f;
        if (D_ovl3_80230E8C == 1.0f && D_ovl3_80230E90 == 1.0f) {
            D_ovl3_80230E88 = 1;
        }
        break;
    case 1:
        w = width;
        h = height / 2;
        bw = 48.0f;
        bh = 16.0f;
        break;
    case 2:
        D_ovl3_80230E8C -= 0.4f;
        D_ovl3_80230E90 -= 0.4f;
        if (D_ovl3_80230E8C < 0.4f) {
            D_ovl3_80230E8C = 0.1f;
        }
        if (D_ovl3_80230E90 < 0.4f) {
            D_ovl3_80230E90 = 0.1f;
        }
        w = width * D_ovl3_80230E8C;
        h = (height / 2) * D_ovl3_80230E90;
        bw = D_ovl3_80230E8C * 48.0f;
        bh = D_ovl3_80230E90 * 16.0f;
        if (D_ovl3_80230E8C <= 0.1f && D_ovl3_80230E90 <= 0.1f) {
            D_ovl3_80230E88 = 3;
        }
        break;
    default:
        return;
    }

    hw = (s32) w / 2;
    hh = (s32) h;
    hbw = (s32) bw / 2;
    hbh = (s32) bh / 2;
    hw = (hw <= 0) ? 1 : hw;
    hh = (hh <= 0) ? 1 : hh;
    hbw = (hbw <= 0) ? 1 : hbw;
    hbh = (hbh <= 0) ? 1 : hbh;
    w = (w <= 0.0f) ? 1.0f : w;
    h = (h <= 0.0f) ? 1.0f : h;
    bw = (bw <= 0.0f) ? 1.0f : bw;
    bh = (bh <= 0.0f) ? 1.0f : bh;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPSetEnvColor(gfx++, 191, 191, 191, 255);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineLERP(gfx++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, TEXEL0,
                      PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0, TEXEL0);
    for (i = 0; i != 2; i++) {
        load_texture_block_4b(&gfx, (u32) &D_ovl3_80230E70[i * 0x1000 + 8], G_IM_FMT_I, 256, 32, 0, 0, 0, 0, 0, 0, 0);
        row = y - hh + i * hh;
        func_8006A2C0(&gfx, (x - hw) * 4, row * 4, (x + hw) * 4, (row + hh) * 4, 0, 0, 0,
                      0x40000 / (s32) w, 0x8000 / (s32) h);
        gDPPipeSync(gfx++);
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    func_8006A548(&gfx, (u32) &D_ovl3_80230E74[8]);
    for (i = 0; i != 2; i++) {
        load_texture_block(&gfx, (u32) &D_ovl3_80230E74[i * 0x6C0 + 0x208], G_IM_FMT_CI, G_IM_SIZ_8b, 72, 24, 0, 0, 0, 0, 0, 0, 0);
        if (i == 0) {
            bx = x - (s32) (D_ovl3_80230E8C * 30.0f);
        } else {
            bx = x + (s32) (D_ovl3_80230E8C * 30.0f);
        }
        by = (s32) ((y + h) - (s32) (D_ovl3_80230E90 * 30.0f));
        grow = 0;
        if (i == selected) {
            pulse = D_ovl3_80230E9C;
            grow = pulse / 2;
        } else {
            pulse = 0;
        }
        func_8006A2C0(&gfx, (bx - hbw - grow) * 4, (by - hbh - grow) * 4, (bx + hbw + grow) * 4,
                      (by + hbh + grow) * 4, 0, 0, 0, 0x12000 / (s32) (bw + pulse),
                      0x6000 / (s32) (bh + pulse));
        gDPPipeSync(gfx++);
    }

    if (D_ovl3_80230EA0 == 0) {
        D_ovl3_80230E9C += 2;
    }
    if (D_ovl3_80230EA0 == 1) {
        D_ovl3_80230E9C -= 2;
    }
    if (D_ovl3_80230E9C == 16) {
        D_ovl3_80230EA0 = 1;
    }
    if (D_ovl3_80230E9C == 0) {
        D_ovl3_80230EA0 = 0;
    }
    if (label != -1 && func_ovl3_80226764() != 0) {
        func_800719A0(x + 8, by - 30, 363, 1, 10, 16, 95, 63, 255, 255);
    }
    *gfxp = gfx;
}

const u32 D_ovl3_80231A68[2] = { 0, 0 };

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802266F0", func_ovl3_8022747C);

void func_8006A2C0(Gfx **pgfx, s32 ulx, s32 uly, s32 lrx, s32 lry, s32 tile, s32 s, s32 t, s32 dsdx,
                  s32 dtdy);
void load_texture_block_4b(Gfx **pgfx, u32 timg, s32 fmt, s32 width, s32 height, s32 pal, s32 cms,
                           s32 cmt, s32 masks, s32 maskt, s32 shifts, s32 shiftt);

/* Draws two stacked 256x32 I4 texture strips centred at (x, y): half-width halfw, strip height hgt,
 * texture scaled to w x h pixels. Note: never writes the advanced cursor back to *pgfx. */
void func_ovl3_802287EC(Gfx **pgfx, s32 x, s32 y, s32 halfw, s32 hgt, s32 w, s32 h) {
  Gfx *gfx;
  s32 i;
  s32 off;
  s32 uly;

  gfx = *pgfx;
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
  gDPSetEnvColor(gfx++, 191, 191, 191, 255);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
  gDPSetCombineMode(gfx++, G_CC_BLENDPEDECALA, G_CC_BLENDPEDECALA);

  for (i = 0; i != 2; i++) {
    off = i * 0x1000 + 8;
    uly = (y - hgt) + i * hgt;
    load_texture_block_4b(&gfx, (u32)D_ovl3_80230E70 + off, G_IM_FMT_I, 256, 32, 0, 0, 0, 0, 0, 0,
                          0);
    func_8006A2C0(&gfx, (x - halfw) << 2, uly << 2, (x + halfw) << 2, (uly + hgt) << 2, 0, 0, 0,
                  0x40000 / w, 0x8000 / h);
    gDPPipeSync(gfx++);
  }
}

extern u8* D_ovl3_80230F44;
extern u8* D_ovl3_80230F48;
extern u8* D_ovl3_80230F4C;
extern u8* D_ovl3_80230F50;
extern u8* D_ovl3_80230F54;

void func_ovl3_80228A80(u8* arg0, s32 arg1) {
    RomAssetLoad asset;

    switch (arg1) {
    case 1:
        D_ovl3_80230F44 = arg0;
        get_rom_asset_allocation_size(0x521, &asset);
        load_rom_asset_payload(D_ovl3_80230F44, &asset);
        break;
    case 2:
        D_ovl3_80230F48 = arg0;
        get_rom_asset_allocation_size(0x93E, &asset);
        load_rom_asset_payload(D_ovl3_80230F48, &asset);
        break;
    case 3:
        D_ovl3_80230F4C = arg0;
        get_rom_asset_allocation_size(0x942, &asset);
        load_rom_asset_payload(D_ovl3_80230F4C, &asset);
        break;
    case 4:
        D_ovl3_80230F50 = arg0;
        get_rom_asset_allocation_size(0x70F, &asset);
        load_rom_asset_payload(D_ovl3_80230F50, &asset);
        break;
    case 5:
        D_ovl3_80230F54 = arg0;
        get_rom_asset_allocation_size(0x6EF, &asset);
        load_rom_asset_payload(D_ovl3_80230F54, &asset);
        break;
    }
}

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

void func_80074CA8(Gfx **pgfx, s32 r, s32 g, s32 b);
void func_8007624C(Gfx **chain, s32 x, s32 y, char *str, u32 fg, u32 bg, s32 arg6);
extern char D_80105118[];
extern char D_ovl3_80231AA8[];

void func_ovl3_8022B360(Gfx **gfxp) {
    Gfx *gfx = *gfxp;
    s32 i;

    gDPPipeSync(gfx++);
    func_80071608(&gfx);
    func_80074CA8(&gfx, 0xFF, 0xFF, 0xFF);
    for (i = 0; i != 8; i++) {
        if (D_ovl3_80230FD4[i] != 0xFF) {
            sprintf(D_80105118, D_ovl3_80231AA8, D_ovl3_80230FD4[i]);
            func_8007624C(&gfx, i * 0x18 + 0x61, 0x96, D_80105118, -1, -1, 0);
        }
    }
    gDPPipeSync(gfx++);
    *gfxp = gfx;
}

extern u8 D_ovl3_80230F84[][10];
extern u8 D_ovl3_80230FD3[];

s32 func_ovl3_8022B46C(void) {
    u8 *state;
    u8 ch;
    s32 ret;
    s32 row;
    s32 col;

    state = func_8005AF74();
    row = (s8)state[0x48];
    col = (s8)state[0x47];
    ch = D_ovl3_80230F84[row][col];
    ret = 0;
    switch (ch) {
    case 0xFE:
        if (D_ovl3_80230F68 > 0) {
            if (D_ovl3_80230F68 == 8) {
                D_ovl3_80230F68 = 7;
            }
            D_ovl3_80230FD4[D_ovl3_80230F68] = 0xFF;
            D_ovl3_80230F68 -= 1;
        } else {
            D_ovl3_80230FD4[0] = 0xFF;
        }
        break;
    case 0xFD:
        break;
    case 0xFF:
        if (D_ovl3_80230F68 < 8) {
            return -1;
        }
        ret = 1;
        break;
    default:
        if (D_ovl3_80230F68 < 8) {
            D_ovl3_80230FD4[D_ovl3_80230F68] = ch;
            D_ovl3_80230F68 = D_ovl3_80230F68 + 1;
            if (D_ovl3_80230F68 == 8) {
                state[0x47] = 9;
                state[0x48] = 3;
            }
        } else {
            D_ovl3_80230FD3[D_ovl3_80230F68] = ch;
        }
        break;
    }
    return ret;
}

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
