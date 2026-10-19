#include "common.h"

extern s32 func_8005B0A0(s32);

s32 func_ovl0_801F4A40(s32 arg0) {
    s32 sum;
    s32 i;

    sum = 0;
    for (i = 1; i < arg0 + 1; i++) {
        sum += func_8005B0A0(i);
    }
    return sum;
}

extern s32 D_ovl0_8021FC74;
extern s32 D_ovl0_8021FC80;
void func_ovl0_801F4AEC(void);
void func_ovl0_801F618C(void);

void func_ovl0_801F4AA4(s32 arg0) {
    if (arg0 == -1) {
        func_ovl0_801F618C();
        return;
    }
    func_ovl0_801F4AEC();
    D_ovl0_8021FC80 = 0x140;
    D_ovl0_8021FC74 = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F4AEC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F5C24);

extern void heap3_free(void** payload_ptr);
extern void* D_ovl0_802256C8;
extern void* D_ovl0_802256CC;
extern void* D_ovl0_802256D0;
extern void* D_ovl0_8022570C;
extern void* D_ovl0_80225710;
extern void* D_ovl0_80225714;
extern s32 D_ovl0_8022573C;

void func_ovl0_801F5E9C(void) {
    heap3_free(&D_ovl0_80225710);
    heap3_free(&D_ovl0_80225714);
    if (D_ovl0_8022573C == 1) {
        heap3_free(&D_ovl0_802256C8);
    }
    if (D_ovl0_8022573C == 2) {
        heap3_free(&D_ovl0_8022570C);
        heap3_free(&D_ovl0_802256CC);
        heap3_free(&D_ovl0_802256D0);
    }
    D_ovl0_8022573C = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F5F2C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F618C);

extern s32 D_ovl0_802257D8[4];
extern s32 D_ovl0_802257E8[4];
extern s32 D_ovl0_802257F8[6];

void func_ovl0_801F63CC(s32 arg0, s32 arg1) {
    D_ovl0_802257D8[arg1] = arg0;
    D_ovl0_802257E8[arg1] = 0;
    D_ovl0_802257F8[arg1] = 0;
}

extern s32 D_800C0E60;
extern s32 D_ovl0_8021FC88;
extern s32 D_ovl0_8021FDB8;
extern void play_sound_effect(s32 sfx, s32 channel, s32 volume);

void func_ovl0_801F63F8(void) {
    s32 tick_count;

    tick_count = D_ovl0_8021FDB8 + 1;
    D_ovl0_8021FDB8 = tick_count;
    if ((D_ovl0_8021FC88 != 1) || !(tick_count & 3)) {
        play_sound_effect(0x20, D_800C0E60, 0x7F);
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F6450);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F6718);

extern void func_8006A548(Gfx** pgfx, u32 tlut);
extern void load_texture_block(Gfx** pgfx, u32 timg, s32 fmt, s32 siz,
                               s32 width, s32 height, s32 pal, s32 cms,
                               s32 cmt, s32 masks, s32 maskt, s32 shifts,
                               s32 shiftt);
extern void func_8006A2C0(Gfx** pgfx, s32 ulx, s32 uly, s32 lrx, s32 lry,
                          s32 tile, s32 s, s32 t, s32 dsdx, s32 dtdy);
extern void* D_ovl0_80225704;
void func_ovl0_801F7740(Gfx** pgfx);
void func_ovl0_801F8094(Gfx** pgfx);

/**
 * Loads the shared palette at block+8 and the index-th 32x32 CI8 texture at
 * block+0x208 + index * 0x400, then draws it at pixel (x, y).
 */
void func_ovl0_801F6D0C(Gfx** pgfx, s32 x, s32 y, s32 index) {
    Gfx* gfx = *pgfx;
    s32 offset;

    func_8006A548(&gfx, (u32)D_ovl0_80225704 + 8);
    offset = (index << 0xA) + 0x208;
    load_texture_block(&gfx, (u32)D_ovl0_80225704 + offset, G_IM_FMT_CI,
                       G_IM_SIZ_8b, 0x20, 0x20, 0, 0, 0, 0, 0, 0, 0);
    func_8006A2C0(&gfx, x * 4, y * 4, (x + 0x20) * 4, (y + 0x20) * 4, 0, 0, 0,
                  0x400, 0x400);
    *pgfx = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F6DFC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F7294);

extern void* D_ovl0_802256D8[];
extern void* D_ovl0_802256E8[];

/**
 * Loads the player's palette and 32x32 CI8 texture, then draws the texture at
 * pixel (x, y). The tile uses S-mirror with a 32-texel mask, so flip = 1 starts
 * S at 32 texels and shows the mirrored copy.
 */
void func_ovl0_801F7418(Gfx** pgfx, s32 x, s32 y, s32 player_index, s32 flip) {
    Gfx* gfx = *pgfx;

    func_8006A548(&gfx, (u32)D_ovl0_802256E8[player_index]);
    load_texture_block(&gfx, (u32)D_ovl0_802256D8[player_index], G_IM_FMT_CI,
                       G_IM_SIZ_8b, 0x20, 0x20, 0, G_TX_MIRROR, G_TX_WRAP, 5,
                       0, 0, 0);
    func_8006A2C0(&gfx, x * 4, y * 4, (x + 0x20) * 4, (y + 0x20) * 4, 0,
                  flip << 0xA, 0, 0x400, 0x400);
    *pgfx = gfx;
}

extern s32 D_ovl0_80225748;
extern s32 D_ovl0_8022574C;
extern s32 D_ovl0_80225750;
extern s32 D_ovl0_80225754;
extern f32 D_ovl0_80225758;
extern s32 D_ovl0_8022575C;
extern s32 D_ovl0_80225760;
extern s32 D_ovl0_80225764;
extern s32 D_ovl0_80225768;
extern s32 D_ovl0_8022576C;

/**
 * Sets the overlay_0 mode D_ovl0_8022573C. Mode 1 resets the mode-1 state
 * (0x80225748..0x80225758), mode 2 zeroes the mode-2 state
 * (0x8022575C..0x8022576C); other values only store the mode.
 */
void func_ovl0_801F7518(s32 arg0) {
    D_ovl0_8022573C = arg0;
    switch (arg0) {
    case 0:
        break;
    case 1:
        D_ovl0_80225748 = 0;
        D_ovl0_8022574C = 0;
        D_ovl0_80225750 = 0x140;
        D_ovl0_80225754 = 0;
        D_ovl0_80225758 = -1.57f;
        break;
    case 2:
        D_ovl0_8022575C = 0;
        D_ovl0_80225760 = 0;
        D_ovl0_80225764 = 0;
        D_ovl0_80225768 = 0;
        D_ovl0_8022576C = 0;
        break;
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F75B0);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F7740);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F7F7C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F8094);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F8A30);

extern s32 D_ovl0_8021FD94;

s32 func_ovl0_801F8B7C(void) {
    return D_ovl0_8021FD94;
}

extern s32 D_ovl0_8021FD98;
extern s32 D_ovl0_8021FD9C;
extern s32 D_ovl0_8021FDA0;

void func_ovl0_801F8B88(s32 state, s32 sub_state) {
    D_ovl0_8021FD94 = state;
    D_ovl0_8021FD98 = sub_state;
    if (state == 0) {
        D_ovl0_8021FD9C = 0x140;
        D_ovl0_8021FDA0 = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F8BB8);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801F9AD0);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801FA098);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801FA248);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801FA684);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801FAD2C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801FB02C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801FB500);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801FBBBC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801FC3D4);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801F4A40", func_ovl0_801FCC48);

void func_ovl0_801FD578(Gfx** pgfx) {
    Gfx* gfx;

    gfx = *pgfx;
    switch (D_ovl0_8022573C) {
    case 0:
        break;
    case 1:
        func_ovl0_801F7740(&gfx);
        break;
    case 2:
        func_ovl0_801F8094(&gfx);
        break;
    }
    *pgfx = gfx;
}
