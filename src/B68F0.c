#include "common.h"

/* Placeholder for the 0xA0-byte entries of D_ovl0_80222600 (32 of them).
 * splat labels the fields separately (D_ovl0_80222604 = unk4,
 * D_ovl0_80222608 = unk8). Real type unknown; rename once identified. */
typedef struct UnkOvl0_80222600 {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ u8 unk4;
    /* 0x05 */ u8 pad5[0x3];
    /* 0x08 */ s32 unk8;
    /* 0x0C */ u8 padC[0x94];
} UnkOvl0_80222600; /* size = 0xA0 */

extern UnkOvl0_80222600 D_ovl0_80222600[32];
extern s32 D_ovl0_80223E24;
extern s32 D_ovl0_80223E30;
extern s32 D_ovl0_80223E34;
extern u8 D_ovl0_80224E90[];
extern u8 D_ovl0_80227040[];
extern u8 D_ovl0_802270C0[];
extern s32 D_ovl0_80227100;

void func_8004D4B8(void *arg0, s32 arg1, void *arg2);
void func_80067CB4(void *arg0);
void func_ovl0_801FD8DC(void);
void func_ovl0_801FD9E4(void);

void func_ovl0_801FD5F0(void) {
}

void func_ovl0_801FD5F8(void) {
    func_8004D4B8(D_ovl0_80224E90, 4, D_ovl0_80227040);
}

INCLUDE_ASM("asm/nonmatchings/B68F0", func_ovl0_801FD624);

INCLUDE_ASM("asm/nonmatchings/B68F0", func_ovl0_801FD760);

INCLUDE_ASM("asm/nonmatchings/B68F0", func_ovl0_801FD798);

INCLUDE_ASM("asm/nonmatchings/B68F0", func_ovl0_801FD8DC);

void func_ovl0_801FD9C0(void) {
    D_ovl0_80223E24 = 1;
    D_ovl0_80223E30 = 1;
    D_ovl0_80223E34 = 10;
}

void func_ovl0_801FD9E4(void) {
    func_80067CB4(D_ovl0_802270C0);
}

void func_ovl0_801FDA04(void) {
    s32 i;

    for (i = 0; i < 32; i++) {
        D_ovl0_80222600[i].unk0 = -1;
        D_ovl0_80222600[i].unk8 = 0;
    }
    func_ovl0_801FD8DC();
    func_ovl0_801FD9E4();
    D_ovl0_80227100 = 0;
}

INCLUDE_ASM("asm/nonmatchings/B68F0", func_ovl0_801FDA64);

s32 func_ovl0_801FE31C(void) {
    s32 found = 0;
    s32 i;

    for (i = 0; i != 32; i++) {
        if (D_ovl0_80222600[i].unk4 == 1 && D_ovl0_80222600[i].unk0 != -1) {
            found = 1;
        }
    }
    return found;
}

INCLUDE_ASM("asm/nonmatchings/B68F0", func_ovl0_801FE370);
