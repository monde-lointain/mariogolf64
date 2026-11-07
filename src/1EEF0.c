#include "common.h"

/* Placeholder types; real types unknown. Rename once identified. */

/* 0x18-byte entries of D_800CABD0 (see func_80043AF0). */
typedef struct Unk800CABD0 {
    /* 0x00 */ u8 pad0[0x18];
} Unk800CABD0; /* size = 0x18 */

/* 0x34-byte entries of D_800BB258; D_800BE588 is the fallback entry
 * (see func_800448E0 / func_8004491C). */
typedef struct Unk800BB258 {
    /* 0x00 */ u8 pad0[0x34];
} Unk800BB258; /* size = 0x34 */

/* Object read by func_800448D0: an s32 array starting at 0x18. */
typedef struct Unk800448D0 {
    /* 0x00 */ u8 pad0[0x18];
    /* 0x18 */ s32 unk18[1]; /* length unknown */
} Unk800448D0;

extern Unk800CABD0 D_800CABD0[];
extern Unk800BB258 D_800BB258[];
extern Unk800BB258 D_800BE588;
extern s32 D_800FBE54;
extern s32 D_800FBE70;

s32 func_80032E60(s32 arg0);
void func_80043B14(Unk800BB258 *arg0, s32 arg1);
void func_80043BB0(Unk800BB258 *arg0, s32 arg1);
Unk800BB258 *func_8004491C(void);
void func_800449D4(s32 arg0, s32 arg1);
s32 func_80044AC8(s32 arg0, s32 arg1, s32 arg2);

Unk800CABD0 *func_80043AF0(s32 arg0) {
    return &D_800CABD0[arg0];
}

void func_80043B0C(void) {
}

INCLUDE_ASM("asm/nonmatchings/1EEF0", func_80043B14);

INCLUDE_ASM("asm/nonmatchings/1EEF0", func_80043BB0);

INCLUDE_ASM("asm/nonmatchings/1EEF0", func_80043C20);

INCLUDE_ASM("asm/nonmatchings/1EEF0", func_80043C64);

INCLUDE_ASM("asm/nonmatchings/1EEF0", func_80043E60);

INCLUDE_ASM("asm/nonmatchings/1EEF0", func_80043EA4);

INCLUDE_ASM("asm/nonmatchings/1EEF0", func_80044254);

INCLUDE_ASM("asm/nonmatchings/1EEF0", func_80044470);

INCLUDE_ASM("asm/nonmatchings/1EEF0", func_800444B8);

INCLUDE_ASM("asm/nonmatchings/1EEF0", func_80044588);

INCLUDE_ASM("asm/nonmatchings/1EEF0", func_800445CC);

s32 func_800448D0(Unk800448D0 *arg0, s32 arg1) {
    return arg0->unk18[arg1];
}

Unk800BB258 *func_800448E0(u32 arg0) {
    /* A ternary, not if/return: the if form puts the fallback block first. */
    return arg0 < 0xFC ? &D_800BB258[arg0] : &D_800BE588;
}

INCLUDE_ASM("asm/nonmatchings/1EEF0", func_8004491C);

void func_8004497C(void) {
    func_80043B14(func_8004491C(), D_800FBE54);
}

void func_800449A8(void) {
    func_80043BB0(func_8004491C(), D_800FBE54);
}

INCLUDE_ASM("asm/nonmatchings/1EEF0", func_800449D4);

void func_80044A8C(s32 arg0) {
    func_800449D4(arg0, func_80032E60(D_800FBE70));
}

INCLUDE_ASM("asm/nonmatchings/1EEF0", func_80044AC8);

void func_80044C7C(s32 arg0, s32 arg1, s32 *arg2, s32 arg3) {
    s32 n = func_80044AC8(arg0, arg1, arg3);

    if (n > 0) {
        arg2[0] = n * 2 - 1;
        arg2[1] = n - 1;
    } else {
        arg2[0] = 0;
        arg2[1] = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/1EEF0", func_80044CCC);

INCLUDE_ASM("asm/nonmatchings/1EEF0", func_80044FDC);

INCLUDE_ASM("asm/nonmatchings/1EEF0", func_800451E4);
