#include "common.h"

extern u8 D_800C0D10[];
extern s32 D_800C0E10;

void func_8004E5A0(Gfx **gfxp, u16 x, u16 y, u8 *str) {
    Gfx *gfx = *gfxp;
    u8 c;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xC8, 0x00, 0xFF);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureImage(gfx++, G_IM_FMT_IA, G_IM_SIZ_16b, 1, (u8 *)((u32)D_800C0D10 & ~7));
    gDPSetTile(gfx++, G_IM_FMT_IA, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 127, 512);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_IA, G_IM_SIZ_4b, 4, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 63 << G_TEXTURE_IMAGE_FRAC, 7 << G_TEXTURE_IMAGE_FRAC);
    D_800C0E10 = (D_800C0E10 + 1) & 0x3FF;
    gDPPipeSync(gfx++);

    c = *str;
    if (c != 0) {
        while (c != 0) {
            if (c != ' ') {
                gSPTextureRectangle(gfx++, x << G_TEXTURE_IMAGE_FRAC, y << G_TEXTURE_IMAGE_FRAC,
                                    (x + 5) << G_TEXTURE_IMAGE_FRAC, (y + 8) << G_TEXTURE_IMAGE_FRAC,
                                    0, ((c - 0x30) & 0xF) * 5 << 5, 0, 1 << 10, 1 << 10);
            }
            str++;
            c = *str;
            x += 6;
        }
    }

    gDPPipeSync(gfx++);
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/main/func_8004E5A0", func_8004E924);

INCLUDE_ASM("asm/nonmatchings/main/func_8004E5A0", func_8004FDB4);
