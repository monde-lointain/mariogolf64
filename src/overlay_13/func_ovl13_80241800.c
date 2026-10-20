#include "common.h"

extern s32 D_ovl13_80243EE0;
extern s32 D_ovl13_80243EE4;
extern u8* func_8005AF50(void);
extern s32 flag_is_set(s32 flag);
extern void func_8006A2C0(Gfx** pgfx, s32 ulx, s32 uly, s32 lrx, s32 lry,
                          s32 tile, s32 s, s32 t, s32 dsdx, s32 dtdy);
extern void func_8006A548(Gfx** pgfx, u32 arg1);
extern void load_texture_block(Gfx** pgfx, u32 timg, s32 fmt, s32 siz, s32 width,
                               s32 height, s32 pal, s32 cms, s32 cmt, s32 masks,
                               s32 maskt, s32 shifts, s32 shiftt);
extern s32 D_ovl13_80243F50;

s32 func_ovl13_80241800(void) {
    u8* ret = func_8005AF50();
    s32 count = 0, i = 0, n = 6;
    u8* base = ret;
    do {
        u32 p = (u32)base + 0x1320;
        u32 end = n + p;
        do {
            count += (*(s8*)p > 0);
            p++;
        } while (p != end);
        base += 6;
        i++;
    } while (i != n);
    if (flag_is_set(0x5E)) {
        return 6;
    }
    if (count >= 0x14) {
        return 6;
    }
    if (count >= 0x10) {
        return 5;
    }
    if (count >= 0xC) {
        return 4;
    }
    if (count >= 8) {
        return 3;
    }
    if (count >= 4) {
        return 2;
    }
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_802418B8);

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_80241BD8);

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_80241CE8);

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_80241DF0);

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_80241FF0);

s32 func_ovl13_80242374(void) {
    return D_ovl13_80243EE0 == 0;
}

void func_ovl13_80242384(s32 state) {
    if (D_ovl13_80243EE4 != state) {
        D_ovl13_80243EE4 = state;
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_802423A4);

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_802427E8);

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_80242D40);

void func_ovl13_80243610(Gfx** gfxp, s32 x, s32 y) {
    Gfx* gfx;
    s32 i;
    s32 yy;

    gfx = *gfxp;
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    func_8006A548(&gfx, D_ovl13_80243F50 + 8);
    for (i = 0; i != 3; i++) {
        load_texture_block(&gfx, D_ovl13_80243F50 + 0x208 + i * 0x7D0, G_IM_FMT_CI, G_IM_SIZ_8b, 0xC8, 0xA, 0, 0, 0, 0, 0, 0, 0);
        gDPPipeSync(gfx++);
        yy = y + i * 10;
        func_8006A2C0(&gfx, (x - 0x68) * 4, (yy - 0xF) * 4, (x + 0x68) * 4, (yy - 5) * 4, 0, 0, 0, 0x3D8, 0x400);
        gDPPipeSync(gfx++);
    }
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_80243784);
