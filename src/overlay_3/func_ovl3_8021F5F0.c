#include "common.h"

extern u8* func_8005AF50(void);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_8021F5F0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_8021F6E0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_8021F79C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_8021FC5C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80220A30);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80220BB0);

extern s16 D_ovl3_8023B684[];

s16 func_ovl3_80220D68(s8 arg0, s32 arg1) {
    s32 cur;
    s32 step;
    s32 i;
    s32 idx;

    cur = arg0;
    if (cur >= 0) {
        if (cur < 5) {
            step = ((arg1 & 0xFF) == 0) ? -1 : 1;
            idx = cur + step;
            for (i = 0; i != 4; i++) {
                if (idx >= 5) {
                    idx = 0;
                }
                if (idx < 0) {
                    idx = 4;
                }
                if (D_ovl3_8023B684[idx] != -1) {
                    break;
                }
                idx += step;
            }
            if (i != 4) {
                return idx;
            }
        }
    }
    return -1;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80220DF8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80220E28);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80220E34);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_802213C8);

s32 func_ovl3_8022181C(void) {
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

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_802218C0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80221B20);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80221CC0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80221EBC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80222038);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_8022209C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80222C94);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80222CDC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80222F2C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_802230C0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80224AFC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80225358);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80225960);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_80226190);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_8021F5F0", func_ovl3_8022644C);
