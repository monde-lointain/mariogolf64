#include <ultra64.h>

extern s32 *D_800C5EE0;   /* dest scratch buffer base pointer */
extern s32 D_800C5EE8;    /* transition mode selector */
extern s16 D_800C6CF8[];  /* vertex/coord grid */
extern s32 D_800C72F8;    /* trigger flag */
extern s32 D_800C72FC;    /* completion flag */
extern s32 D_800C7300;    /* wipe progress counter */
extern s32 *nuGfxCfb_ptr; /* current framebuffer */

void func_80095150(void) {
    s32 mode;
    s32 *cfb;
    s32 idx;
    s32 col, row, seg, k;
    s32 *dst;

    if (D_800C72F8 == 0) {
        return;
    }
    mode = D_800C5EE8;
    cfb = nuGfxCfb_ptr;
    D_800C72F8 = 0;
    idx = 0;

    if ((u32)(mode - 2) < 2U) {
        s32 *scratch = D_800C5EE0;
        s32 base;
        s16 *pl, *pr, *pl1, *pr1;
        s16 xval, yval;

        base = 0;
        for (row = 0; row != 7; row++, base += 0x1400) {
            s32 rbase = base;
            for (seg = 0; seg != 5; seg++, rbase += 0x20) {
                s32 cbase = 0x500;
                for (col = 8; col != 0x28; col++, cbase += 0xA0) {
                    s32 c0 = cbase;
                    dst = &scratch[idx];
                    for (k = 4; k != 0x24; k++) {
                        *dst = cfb[c0 + k + rbase];
                        idx++;
                        dst++;
                    }
                }
            }
        }

        pl = &D_800C6CF8[0];
        pr = &D_800C6CF8[0x180];
        yval = 0x70;
        for (row = 0; row != 8; row++, yval -= 0x20) {
            pl1 = pr + 2;
            pr1 = pl + 2;
            xval = -0x98;
            for (col = 0; col != 5; col++) {
                *pl = xval;
                *(pr1 - 1) = yval;
                *pr1 = 0;
                *pr = xval;
                xval += 0x40;
                pr1 += 8;
                pl += 8;
                *(pl1 - 1) = yval;
                *pl1 = 0;
                pl1 += 8;
                pr += 8;
            }
            pl1 = pr + 2;
            pr1 = pl + 2;
            xval = 0x98;
            for (col = 5; col != 6; col++) {
                *pl = xval;
                *(pr1 - 1) = yval;
                *pr1 = 0;
                *pr = xval;
                xval += 0x40;
                pr1 += 8;
                pl += 8;
                *(pl1 - 1) = yval;
                *pl1 = 0;
                pl1 += 8;
                pr += 8;
            }
        }
    } else if (((mode == 1) | (mode == 9)) || ((mode == 8) | (mode == 0xA))) {
        s32 *scratch = D_800C5EE0;
        s32 cbase = 0x500;
        for (col = 8; col != 0xE8; col++, cbase += 0xA0) {
            dst = &scratch[idx];
            for (k = 4; k != 0x9C; k++) {
                *dst = cfb[cbase + k];
                idx++;
                dst++;
            }
        }
    } else if (mode == 4) {
        if (D_800C7300 < 8) {
            s32 *scratch = D_800C5EE0;
            s32 cbase = 0x3700;
            s32 run = 0;
            for (col = 0x58; col != 0xD0; col++, cbase += 0xA0) {
                for (k = 0x28; k != 0x50; k++) {
                    s32 srcv = cfb[cbase + k];
                    s32 dsti = D_800C7300 * 0x12C0 + run;
                    run++;
                    scratch[dsti] = srcv;
                }
            }
            D_800C7300++;
        }
    } else if (mode == 6) {
        s32 *scratch = D_800C5EE0;
        s32 cbase = 0x500;
        for (col = 8; col != 0xE8; col++, cbase += 0xA0) {
            dst = &scratch[idx];
            for (k = 4; k != 0x9C; k++) {
                *dst = ~cfb[cbase + k];
                idx++;
                dst++;
            }
        }
    }
    D_800C72FC = 1;
}
