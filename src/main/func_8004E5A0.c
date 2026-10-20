#include "common.h"
#include "heap.h"

extern u8 D_800C0D10[];
extern s32 D_800C0E10;
extern Gfx D_800DC880[2][0x200];
extern char D_800DE880[];
extern char D_800CCB58[];
extern char D_800CCB60[];
extern Gfx D_C0C40[];
extern s32 D_800C0D00;
extern u16 *nuGfxCfb_ptr;
extern u32 nuGfxCfbNum;

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

void func_8004FDB4(s32 arg0, s32 arg1) {
    Gfx *glistp;

    glistp = D_800DC880[D_800C0D00];
    gSPDisplayList(glistp++, D_C0C40);
    gDPSetColorImage(glistp++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
                     osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
    gDPPipeSync(glistp++);
    gDPPipeSync(glistp++);
    gDPSetCycleType(glistp++, G_CYC_1CYCLE);
    gDPPipeSync(glistp++);
    gDPSetRenderMode(glistp++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetPrimColor(glistp++, 0, 0, 0, 0, 0, 0xBF);
    gDPSetCombineMode(glistp++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPPipeSync(glistp++);
    gDPSetTexturePersp(glistp++, G_TP_NONE);
    gDPPipeSync(glistp++);
    gDPSetTextureLUT(glistp++, G_TT_NONE);
    gDPPipeSync(glistp++);
    gSPTextureRectangle(glistp++, 250 << G_TEXTURE_IMAGE_FRAC, (arg0 - 8) << G_TEXTURE_IMAGE_FRAC,
                        302 << G_TEXTURE_IMAGE_FRAC, (arg0 + 0x14) << G_TEXTURE_IMAGE_FRAC, 0, 0, 0,
                        1 << 10, 1 << 10);

    sprintf(D_800DE880, D_800CCB58, nuGfxCfbNum);
    func_8004E5A0(&glistp, 0xFC, arg0 - 7, (u8 *)D_800DE880);
    sprintf(D_800DE880, D_800CCB60, heap3_get_total());
    func_8004E5A0(&glistp, 0xFC, arg0 + 2, (u8 *)D_800DE880);
    sprintf(D_800DE880, D_800CCB60, heap3_get_largest_free());
    func_8004E5A0(&glistp, 0xFC, arg0 + 0xA, (u8 *)D_800DE880);

    gDPFullSync(glistp++);
    gSPEndDisplayList(glistp++);
    nuGfxTaskStart(D_800DC880[D_800C0D00], (glistp - D_800DC880[D_800C0D00]) * sizeof(Gfx), 0, arg1);
    D_800C0D00 = (D_800C0D00 + 1) & 1;
}
