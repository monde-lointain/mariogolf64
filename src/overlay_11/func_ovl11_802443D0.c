#include "common.h"

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_802443D0);

extern void func_8006A548(Gfx** pgfx, u32 tlut);
extern void load_texture_block(Gfx** pgfx, u32 timg, s32 fmt, s32 siz,
                               s32 width, s32 height, s32 pal, s32 cms,
                               s32 cmt, s32 masks, s32 maskt, s32 shifts,
                               s32 shiftt);
extern void func_8006A2C0(Gfx** pgfx, s32 ulx, s32 uly, s32 lrx, s32 lry,
                          s32 tile, s32 s, s32 t, s32 dsdx, s32 dtdy);
extern void* D_801321C8[];
extern void* D_801B55E0[];

/**
 * Draws the 32x24 CI8 texture block selected by buffer_index at pixel (x, y).
 * flip = 1 sets masks = 5 with mirroring, so the S range starting at 32 texels
 * is reflected: a horizontal flip.
 */
void func_ovl11_802444B8(Gfx** pgfx, s32 x, s32 y, s32 buffer_index, s32 flip) {
    Gfx* gfx = *pgfx;

    func_8006A548(&gfx, (u32)D_801321C8[buffer_index]);
    load_texture_block(&gfx, (u32)D_801B55E0[buffer_index], G_IM_FMT_CI,
                       G_IM_SIZ_8b, 0x20, 0x18, 0, 1, 1, flip * 5, 0, 0, 0);
    func_8006A2C0(&gfx, x * 4, y * 4, (x + 0x20) * 4, (y + 0x18) * 4, 0,
                  flip << 0xA, 0, 0x400, 0x400);
    *pgfx = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_802445C0);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_80244748);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_802448D4);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_80244CA0);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_80245184);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_802460F8);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_80246804);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_802478B0);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_80247D80);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_80247E3C);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_802493DC);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_8024C1D8);
