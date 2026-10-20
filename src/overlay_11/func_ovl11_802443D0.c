#include "common.h"
#include "overlay_11.h"

extern void func_8006A548(Gfx** pgfx, u32 tlut);
extern void func_8006A4A0(Gfx** pgfx, u32 pal, u32 addr);
extern void load_texture_block(Gfx** pgfx, u32 timg, s32 fmt, s32 siz,
                               s32 width, s32 height, s32 pal, s32 cms,
                               s32 cmt, s32 masks, s32 maskt, s32 shifts,
                               s32 shiftt);
extern void func_8006A2C0(Gfx** pgfx, s32 ulx, s32 uly, s32 lrx, s32 lry,
                          s32 tile, s32 s, s32 t, s32 dsdx, s32 dtdy);
extern void* D_801321C8[];
extern void* D_801B55E0[];
extern const char D_ovl11_8024C8A8[];
extern const char D_ovl11_8024C8B8[];
extern const char D_ovl11_8024C8C8[];
extern const char D_ovl11_8024C8D4[];
extern const char D_ovl11_8024C8E0[];
extern const char D_ovl11_8024C8E4[];

typedef struct Ovl11Record {
    /* 0x00 */ u8 unk_00[2];
    /* 0x02 */ s16 unk2;
    /* 0x04 */ s8 unk4;
    /* 0x05 */ u8 unk5;
} Ovl11Record;

/**
 * Loads the index-th 32x32 CI8 texture (palette in unkAC, image in unk9C) and
 * draws it at pixel (x, y).
 */
void func_ovl11_802443D0(Gfx** pgfx, s32 x, s32 y, s32 index) {
    Gfx* gfx = *pgfx;

    func_8006A548(&gfx, D_80104FD0.unkAC[index]);
    load_texture_block(&gfx, D_80104FD0.unk9C[index], G_IM_FMT_CI,
                       G_IM_SIZ_8b, 0x20, 0x20, 0, 0, 0, 0, 0, 0, 0);
    func_8006A2C0(&gfx, x * 4, y * 4, (x + 0x20) * 4, (y + 0x20) * 4, 0, 0, 0,
                  0x400, 0x400);
    *pgfx = gfx;
}

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

void func_ovl11_802445C0(Gfx** pgfx, s32 x, s32 y, s32 index) {
    Gfx* gfx;
    s32 offset;
    s32 uly;
    s32 lry;

    gfx = *pgfx;
    func_8006A548(&gfx, D_80104FD0.unk38 + 8);
    load_texture_block(&gfx, (D_80104FD0.unk38 + 0x1208) + (index << 0xA),
                       G_IM_FMT_CI, G_IM_SIZ_8b, 0x20, 0x20, 0, 0, 0, 0, 0,
                       0, 0);
    uly = y * 4;
    lry = (y + 0x20) * 4;
    func_8006A2C0(&gfx, (x + 0x18) * 4, uly, (x + 0x38) * 4, lry, 0, 0, 0,
                  0x400, 0x400);
    offset = (index << 0xA) + 0x208;
    load_texture_block(&gfx, D_80104FD0.unk38 + offset, G_IM_FMT_CI,
                       G_IM_SIZ_8b, 0x20, 0x20, 0, 0, 0, 0, 0, 0, 0);
    func_8006A2C0(&gfx, x * 4, uly, (x + 0x20) * 4, lry, 0, 0, 0, 0x400, 0x400);
    *pgfx = gfx;
}

/**
 * Loads the palette at unk20 + 8, then the 80x18 4-bit texture at
 * unk20 + index * 0x2D0 + 0x28 (8-byte aligned), and draws a 54x18 rect at (x, y).
 */
void func_ovl11_80244748(Gfx** pgfx, s32 x, s32 y, s32 index) {
    Gfx* gfx;
    s32 offset;

    gfx = *pgfx;
    func_8006A4A0(&gfx, 0, D_80104FD0.unk20 + 8);
    offset = (index * 0x2D0) + 0x28;
    gDPLoadTextureTile_4b(gfx++, (D_80104FD0.unk20 + offset) & ~7,
                          G_IM_FMT_CI, 80, 18, 0, 0, 79, 17, 0, 0, 0, 0, 0, 0, 0);
    func_8006A2C0(&gfx, x * 4, y * 4, (x + 0x36) * 4, (y + 0x12) * 4, 0, 0, 0,
                  0x400, 0x400);
    *pgfx = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_802448D4);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_80244CA0);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_80245184);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_802460F8);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_80246804);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_802478B0);

/**
 * Formats the record into arg0: unk5 selects the "%2dT" form when bit 7 is set,
 * unk2 is printed at arg0 + 0xC, unk4 at arg0 + 0x12.
 */
void func_ovl11_80247D80(char *arg0, Ovl11Record *arg1) {
    s32 var_a2_2;
    const char *var_a1;
    const char *var_a1_2;
    char *var_a0_2;
    char *var_a0;
    u8 var_a2;

    var_a2 = arg1->unk5;
    if (var_a2 & 0x80) {
        var_a0 = arg0;
        var_a1 = D_ovl11_8024C8A8;
        var_a2 &= 0x7F;
    } else {
        var_a0 = arg0;
        var_a1 = D_ovl11_8024C8B8;
    }
    sprintf(var_a0, var_a1, var_a2);
    var_a0_2 = arg0 + 0xC;
    if (arg1->unk2 == 0) {
        var_a1_2 = D_ovl11_8024C8C8;
        var_a2_2 = 0;
    } else {
        var_a1_2 = D_ovl11_8024C8D4;
        var_a2_2 = arg1->unk2;
    }
    sprintf(var_a0_2, var_a1_2, var_a2_2);
    if (arg1->unk4 == 0) {
        sprintf(arg0 + 0x12, D_ovl11_8024C8E0);
    } else {
        sprintf(arg0 + 0x12, D_ovl11_8024C8E4, arg1->unk4);
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_80247E3C);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_802493DC);

INCLUDE_ASM("asm/nonmatchings/overlay_11/func_ovl11_802443D0", func_ovl11_8024C1D8);
