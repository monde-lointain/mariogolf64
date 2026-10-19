#include "common.h"

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802159C0", func_ovl3_802159C0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802159C0", func_ovl3_80215B84);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802159C0", func_ovl3_80215DC4);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802159C0", func_ovl3_8021640C);

extern s32 D_ovl3_80230248[][2];

void func_ovl3_802169CC(Gfx **arg0) {
    Gfx *gfx;
    s32 i;

    gfx = *arg0;
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPPipeSync(gfx++);
    for (i = 0; i != 0x18; i += 2) {
        gSPScisTextureRectangle(gfx++, D_ovl3_80230248[i][0] * 4,
                                D_ovl3_80230248[i][1] * 4,
                                D_ovl3_80230248[i + 1][0] * 4,
                                D_ovl3_80230248[i + 1][1] * 4,
                                G_TX_RENDERTILE, 0, 0, 0x400, 0x400);
        gDPPipeSync(gfx++);
    }
    *arg0 = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802159C0", func_ovl3_80216C14);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802159C0", func_ovl3_8021730C);

extern u8* D_ovl3_8023AC70;

void func_ovl3_80217464(Gfx** gfxp, s32 x, s32 y) {
    Gfx* gfx = *gfxp;

    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, (u8*)(((u32)D_ovl3_8023AC70 + 8) & ~7));
    gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                       (u8*)(((u32)D_ovl3_8023AC70 + 0x28) & ~7));
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0,
               0, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 191, 1024);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_4b, 2, 0, G_TX_RENDERTILE, 0, 0, 0, 0,
               0, 0, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 31 << G_TEXTURE_IMAGE_FRAC,
                   23 << G_TEXTURE_IMAGE_FRAC);
    gSPScisTextureRectangle(gfx++, x << G_TEXTURE_IMAGE_FRAC,
                            y << G_TEXTURE_IMAGE_FRAC,
                            (x + 0x18) << G_TEXTURE_IMAGE_FRAC,
                            (y + 0x14) << G_TEXTURE_IMAGE_FRAC, G_TX_RENDERTILE,
                            0, 0, 0x555, 0x4CC);
    gDPPipeSync(gfx++);
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_802159C0", func_ovl3_802177D0);
