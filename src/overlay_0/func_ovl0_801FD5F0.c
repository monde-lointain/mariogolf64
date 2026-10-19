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

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801FD5F0", func_ovl0_801FD760);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801FD5F0", func_ovl0_801FD798);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801FD5F0", func_ovl0_801FD8DC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801FD5F0", func_ovl0_801FD9C0);

extern f32 D_ovl0_802270C0[4][4];

void func_ovl0_801FD9E4(void) {
    guMtxIdentF(D_ovl0_802270C0);
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801FD5F0", func_ovl0_801FDA04);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801FD5F0", func_ovl0_801FDA64);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801FD5F0", func_ovl0_801FE31C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_801FD5F0", func_ovl0_801FE370);
