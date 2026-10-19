#include "common.h"

extern void heap3_free(void** payload_ptr);
extern void func_80051A88(s32 volume);
extern s32 D_ovl0_80224A7C;
extern void* D_ovl0_8022B530;
extern void* D_ovl0_8022B534;
extern void* D_ovl0_8022B538;
extern void* D_ovl0_8022B53C;

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_80218CC0);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_80218D1C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_80218DB0);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_80218DD0);

void func_ovl0_80218FDC(void) {
    if (D_ovl0_80224A7C == 1) {
        D_ovl0_80224A7C = 0;
        heap3_free(&D_ovl0_8022B530);
        heap3_free(&D_ovl0_8022B534);
        heap3_free(&D_ovl0_8022B538);
    }
    func_80051A88(0x7F);
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_80219034);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_802199EC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_80219C74);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_8021A62C);

void func_ovl0_8021A690(void) {
    heap3_free(&D_ovl0_8022B53C);
    heap3_free(&D_ovl0_8022B534);
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_8021A6BC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_8021A724);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80218CC0", func_ovl0_8021A940);
