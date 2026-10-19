#include "common.h"

extern s32 D_ovl13_80243EE0;
extern s32 D_ovl13_80243EE4;
extern u8* func_8005AF50(void);
extern s32 flag_is_set(s32 flag);

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

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_80243610);

INCLUDE_ASM("asm/nonmatchings/overlay_13/func_ovl13_80241800", func_ovl13_80243784);
