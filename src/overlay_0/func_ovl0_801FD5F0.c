#include "common.h"

void func_ovl0_801FD5F0(void) {
}

void func_8004D4B8(char* source_text, s32 glyph_count, u16* destination);
extern char D_ovl0_80224E90[8];
extern u16 D_ovl0_80227040[64];

void func_ovl0_801FD5F8(void) {
    func_8004D4B8(D_ovl0_80224E90, 4, D_ovl0_80227040);
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801FD5F0", func_ovl0_801FD624);

/* sqrtf is inlined as sqrt.s (-ffast-math). The dead 8-byte frame and `sw $v0,0(sp)` are the static-chain home of the nested-function original; `unused` reproduces them, as in func_ovl7_802068B4. */
f32 func_ovl0_801FD760(f32 arg0, f32 arg1, f32 arg2) {
    s32 r;
    volatile s32 unused = r;
    return sqrtf(arg0 * arg0 + arg1 * arg1 + arg2 * arg2);
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801FD5F0", func_ovl0_801FD798);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801FD5F0", func_ovl0_801FD8DC);

extern s32 D_ovl0_80223E24;
extern s32 D_ovl0_80223E30;
extern s32 D_ovl0_80223E34;

void func_ovl0_801FD9C0(void) {
    D_ovl0_80223E24 = 1;
    D_ovl0_80223E30 = 1;
    D_ovl0_80223E34 = 10;
}

extern f32 D_ovl0_802270C0[4][4];

void func_ovl0_801FD9E4(void) {
    guMtxIdentF(D_ovl0_802270C0);
}

typedef struct Ovl0Record {
    f32 unk_00[3];
    s32 unk_0C;
    u8 unk_10;
    u8 pad_11[3];
    s32 unk_14;
    u8 pad_18[0x88];
} Ovl0Record;

extern Ovl0Record D_ovl0_802225F4[32];
extern s32 D_ovl0_80227100;
void func_ovl0_801FD8DC(void);

void func_ovl0_801FDA04(void) {
    s32 i;

    i = 0;
    do {
        D_ovl0_802225F4[i].unk_0C = -1;
        D_ovl0_802225F4[i].unk_14 = 0;
        i += 1;
    } while (i < 32);
    func_ovl0_801FD8DC();
    func_ovl0_801FD9E4();
    D_ovl0_80227100 = 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801FD5F0", func_ovl0_801FDA64);

/* Returns 1 when any of the 32 records has its flag (unk_10) set to 1 and a
 * bound object id (unk_0C) other than -1. */
s32 func_ovl0_801FE31C(void) {
    s32 found;
    s32 i;

    found = 0;
    i = 0;
    do {
        if (D_ovl0_802225F4[i].unk_10 == 1 && D_ovl0_802225F4[i].unk_0C != -1) {
            found = 1;
        }
        i += 1;
    } while (i != 32);
    return found;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801FD5F0", func_ovl0_801FE370);
