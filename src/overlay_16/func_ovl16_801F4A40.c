#include "common.h"

extern s32 D_ovl16_801F89C0;
extern s32 D_ovl16_80223804;
extern s8 D_ovl16_802237B8;
extern s8 D_ovl16_802237B9;
extern s32 D_ovl16_802237C0[];
extern u8 D_801B724B;
extern s32 rand(void);
extern s32 flag_is_set(s32 flag);
extern f32 D_ovl16_801F89C8;
extern u16 *D_ovl16_80223788;
extern s32 D_ovl16_8022378C;
extern u8 D_ovl16_801F89E0[];
extern void *D_ovl16_80223790;
extern void *D_ovl16_80223794[];
extern void *D_801B7EE0;
extern f32 D_801B5528;
extern s32 gameplay_viewport_x;
extern s32 gameplay_viewport_y;
extern s32 gameplay_viewport_w;
extern s32 gameplay_viewport_h;
extern void func_80029B58(void);
extern void func_800543A4(void);
extern void func_80078910(void);
extern void heap3_free(void **payload_address);
extern void set_clear_color(s32 red, s32 green, s32 blue);

s32 func_ovl16_801F4A40(void) {
    return D_801B724B & 3;
}

void func_ovl16_801F4A88(s32 arg0) {
    void func_ovl16_801F4A50(s32 index0, s32 index1) {
        s32 tmp;

        tmp = D_ovl16_802237C0[index0];
        D_ovl16_802237C0[index0] = D_ovl16_802237C0[index1];
        D_ovl16_802237C0[index1] = tmp;
    }
    s32 i;
    s32 s0;
    s32 s1;

    for (i = 0; i < 17; i++) {
        D_ovl16_802237C0[i] = (i + arg0 + 2) % 18;
    }
    if (!flag_is_set(0x99)) {
        for (s1 = 0; s1 < 10; s1++) {
            for (s0 = 0; s0 < 16; s0++) {
                func_ovl16_801F4A50(s0, (rand() & 0x7FFF) % 16);
            }
        }
    }
}

void func_ovl16_801F4B64(s32 arg0) {
    D_ovl16_801F89C0 = -arg0;
    D_ovl16_80223804 = arg0;
}

void func_ovl16_801F4B7C(s32 arg0) {
    D_ovl16_801F89C0 = arg0;
    D_ovl16_80223804 = arg0;
}

void func_ovl16_801F4B90(void) {
    D_ovl16_801F89C0 = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F4B9C);

void func_ovl16_801F4E54(void) {
    s32 i;

    func_80078910();
    gameplay_viewport_x = 8;
    gameplay_viewport_y = 8;
    gameplay_viewport_w = 0x130;
    gameplay_viewport_h = 0xE0;
    D_801B5528 = 1.0f;
    func_800543A4();
    for (i = 0; i < 1; i++) {
        heap3_free(&D_ovl16_80223794[i]);
    }
    func_80029B58();
    set_clear_color(0, 0, 0);
    heap3_free(&D_ovl16_80223790);
    heap3_free((void **)&D_ovl16_80223788);
    heap3_free((void **)&D_ovl16_8022378C);
    heap3_free(&D_801B7EE0);
}

f32 func_ovl16_801F4F24(f32 near, f32 far, f32 t) {
  if (t < 0.0f) {
    t = 0.0f;
  }
  if (t > 1.0f) {
    t = 1.0f;
  }
  return near * (1.0f - t) + far * t;
}

void func_ovl16_801F4F7C(void) {
    D_ovl16_802237B8 = 0;
    D_ovl16_802237B9 = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F4F90);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F5138);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F5194);

extern void animation_play(s32 id, s32 animation);
s32 func_ovl16_801F5138(s32 arg0, s32 arg1);

void func_ovl16_801F5210(s32 arg0, s32 arg1) {
    animation_play(arg0, func_ovl16_801F5138(arg0, arg1));
}

s32 func_ovl16_801F5194(s32 arg0, s32 arg1, s32 arg2);

void func_ovl16_801F5240(s32 arg0, s32 arg1, s32 arg2) {
    animation_play(arg0, func_ovl16_801F5194(arg0, arg1, arg2));
}

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F5270);

/* Dead 8-byte frame and `sw $v0,0x10(sp)` before `jal rand`: static-chain home of the nested-function original (callers set `addiu $v0,$sp,0x10` in the jal delay slot); `unused` reproduces them, as in func_ovl7_802068B4. */
void func_ovl16_801F53F0(void) {
    s32 r;
    volatile s32 unused = r;

    D_ovl16_801F89C8 = (f32) (((rand() & 0x7F) - 0x20) * 0x46) * 0.0001374275f;
}

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F544C);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F7148);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F7554);

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F803C);

void func_ovl16_801F803C(s32 index, s32 *out0, s32 *out1);

void func_ovl16_801F80B8(Gfx **gfxp, s32 x, s32 y, s32 index) {
    Gfx *gfx;
    s32 stride;
    s32 start;
    s32 count;
    s32 i;
    s32 code;
    s32 row;
    s32 col;
    s32 row3;
    s32 col3;
    s32 texAddr;
    s32 unused0;
    s32 unused1;

    gfx = *gfxp;
    start = D_ovl16_80223788[2 + index * 2];
    count = D_ovl16_80223788[3 + index * 2];
    stride = D_ovl16_80223788[1] * 2 + 2;
    func_ovl16_801F803C(index, &unused0, &unused1);

    gDPPipeSync(gfx++);
    i = 0;
    if (count != 0) {
        do {
            code = D_ovl16_80223788[start + i + stride];
            if (code < 0x100) {
                row = code / 16;
                col = code - row * 16;
                col3 = col * 3;
                row3 = row * 3;
                texAddr = (D_ovl16_8022378C + 8) & ~7;

                gDPPipeSync(gfx++);
                gDPSetTextureImage(gfx++, G_IM_FMT_I, G_IM_SIZ_8b, 96, texAddr);
                gDPSetTile(gfx++, G_IM_FMT_I, G_IM_SIZ_8b, 1, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
                gDPLoadSync(gfx++);
                gDPLoadTile(gfx++, G_TX_LOADTILE, col3 * 8, row3 << 4, (col * 12 + 11) * 2, (row * 12 + 11) * 4);
                gDPPipeSync(gfx++);
                gDPSetTile(gfx++, G_IM_FMT_I, G_IM_SIZ_4b, 1, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
                gDPSetTileSize(gfx++, G_TX_RENDERTILE, col * 48, row * 48, (col * 12 + 11) * 4, (row * 12 + 11) * 4);
                gDPPipeSync(gfx++);

                if ((u32)(code - 0xDE) < 2U) {
                    gSPTextureRectangle(gfx++, (x - 6) * 4, (y - 4) * 4, (x + 6) * 4, (y + 8) * 4,
                                        G_TX_RENDERTILE, col * 384, row * 384, 0x400, 0x400);
                } else {
                    gSPTextureRectangle(gfx++, x * 4, y * 4, (x + 12) * 4, (y + 12) * 4,
                                        G_TX_RENDERTILE, col * 384, row * 384, 0x400, 0x400);
                    x += D_ovl16_801F89E0[code];
                }
            }
            i++;
        } while (i != count);
    }
    gDPPipeSync(gfx++);
    *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_16/func_ovl16_801F4A40", func_ovl16_801F8438);

void func_ovl16_801F898C(void) {}
