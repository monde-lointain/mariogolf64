#include "common.h"

extern s16 g_abRosterUnlockFlags[4];
extern s32 D_ovl3_8022FF1C[4];
extern u8* func_8005AF74(void);
extern s16 D_800C28E4[3][7];

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

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80209500", func_ovl3_8020D338);

extern void *D_801B69EC;

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

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_80209500", func_ovl3_8020DB30);
