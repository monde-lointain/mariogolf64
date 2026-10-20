#include "common.h"

extern s16 g_abRosterUnlockFlags[4];
extern s32 D_ovl3_8022FF1C[4];
extern u8* func_8005AF74(void);
extern s16 D_800C28E4[3][7];
extern s32 D_ovl3_8023A584;
extern s8 D_800BB288[];
extern void func_8006A4A0(Gfx** pgfx, u32 pal, u32 addr);
extern void load_texture_block_4b(Gfx** pgfx, u32 timg, s32 fmt, s32 width, s32 height, s32 pal, s32 cms, s32 cmt, s32 masks, s32 maskt, s32 shifts, s32 shiftt);
extern void func_8006A2C0(Gfx** pgfx, s32 ulx, s32 uly, s32 lrx, s32 lry, s32 tile, s32 s, s32 t, s32 dsdx, s32 dtdy);
extern Gfx* func_ovl3_801F4A40(void);

void func_ovl3_80209500(void) {
    D_ovl3_8022FF1C[0] = g_abRosterUnlockFlags[0];
    g_abRosterUnlockFlags[0] = -1;
    D_ovl3_8022FF1C[1] = g_abRosterUnlockFlags[1];
    g_abRosterUnlockFlags[1] = -1;
    D_ovl3_8022FF1C[2] = g_abRosterUnlockFlags[2];
    g_abRosterUnlockFlags[2] = -1;
    D_ovl3_8022FF1C[3] = g_abRosterUnlockFlags[3];
    g_abRosterUnlockFlags[3] = -1;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80209500", func_ovl3_80209568);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80209500", func_ovl3_80209894);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80209500", func_ovl3_80209BE0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80209500", func_ovl3_80209D50);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80209500", func_ovl3_8020B2C4);

void func_ovl3_8020B3CC(void) {
    u8 *state;
    s32 row;
    s32 col;

    state = func_8005AF74();
    row = 0;
    do {
        col = 0;
        do {
            if (D_800C28E4[row][col] < -10) {
                state[4] = col;
                state[8] = row;
            }
            col++;
        } while (col != 7);
        row++;
    } while (row != 2);
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80209500", func_ovl3_8020B438);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80209500", func_ovl3_8020B590);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80209500", func_ovl3_8020BA08);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80209500", func_ovl3_8020C2D8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80209500", func_ovl3_8020C3B8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80209500", func_ovl3_8020C6D8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80209500", func_ovl3_8020CD20);

extern void *D_801B69EC;

void func_ovl3_8020D338(Gfx **gfxp) {
    Gfx *gfx = *gfxp;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, AA_EN | IM_RD | CVG_X_ALPHA | ALPHA_CVG_SEL | GBL_c1(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM), AA_EN | IM_RD | CVG_X_ALPHA | ALPHA_CVG_SEL | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM));
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_801B69EC + 8) & ~7);
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(gfx++, ((u32)D_801B69EC + 0x28) & ~7, G_IM_FMT_CI, 32, 24, 0, 0, 0, 0, 0, 0, 0);
    gDPPipeSync(gfx++);
    gSPTextureRectangle(gfx++, 270 << 2, 20 << 2, 302 << 2, 44 << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    gDPPipeSync(gfx++);

    *gfxp = gfx;
}

void func_ovl3_8020D734(Gfx **gfxp) {
    Gfx *gfx = *gfxp;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, AA_EN | IM_RD | CVG_X_ALPHA | ALPHA_CVG_SEL | GBL_c1(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM), AA_EN | IM_RD | CVG_X_ALPHA | ALPHA_CVG_SEL | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM));
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_801B69EC + 8) & ~7);
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock_4b(gfx++, ((u32)D_801B69EC + 0x1A8) & ~7, G_IM_FMT_CI, 32, 24, 0, 0, 0, 0, 0, 0, 0);
    gDPPipeSync(gfx++);
    gSPTextureRectangle(gfx++, 72, 80, 200, 176, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    gDPPipeSync(gfx++);

    *gfxp = gfx;
}

void func_ovl3_8020DB30(Gfx** gfxp, s32 arg1) {
    Gfx* gfx = *gfxp;
    s32 v0 = D_800BB288[arg1 * 728];
    s32 lo;
    s32 hi;
    s32 i;
    s32 x;
    u32 base;
    s32 sel;
    s32 flag;

    if (v0 < 0) {
        lo = v0 + 4;
        hi = 5;
    } else if (v0 > 0) {
        lo = 4;
        hi = v0 + 5;
    } else {
        lo = 4;
        hi = 5;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, (u8*)(((u32)(D_ovl3_8023A584 + 8)) & ~7));

    for (i = lo; i != hi; i++) {
        s32 texoff = (i * 64) + 0x208;

        x = 204 + i * 8;
        gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1, (u8*)(((u32)(D_ovl3_8023A584 + texoff)) & ~7));
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
        gDPLoadSync(gfx++);
        gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 0x1F, 0x800);
        gDPPipeSync(gfx++);
        gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 1, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
        gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 7 << G_TEXTURE_IMAGE_FRAC, 7 << G_TEXTURE_IMAGE_FRAC);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, x << 2, 185 << 2, (x + 8) << 2, 193 << 2, 0, 0, 0, 0x400, 0x400);
        gDPPipeSync(gfx++);
    }

    base = (u32)func_ovl3_801F4A40();
    func_8006A4A0(&gfx, 0, base + 8);
    load_texture_block_4b(&gfx, base + 0x28, G_IM_FMT_CI, 0x40, 0x18, 0, 0, 0, 0, 0, 0, 0);

    sel = (u8)D_800BB288[arg1 * 728] << 24;
    flag = 1;
    if (sel <= 0) {
        flag = (sel >> 31) & 2;
    }
    func_8006A2C0(&gfx, 0x348, 0x2B8, 0x448, 0x2D8, 0, 0, flag << 8, 0x400, 0x400);

    *gfxp = gfx;
}
