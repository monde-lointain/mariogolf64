#include "common.h"

/* Placeholder types; real types unknown. Rename once identified. */

typedef struct Unk800FEC00 {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ u8 padC[0x28];
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 unk38;
    /* 0x3C */ u8 pad3C[0x50];
    /* 0x8C */ s32 unk8C;
} Unk800FEC00;

/* A 0x200-byte block: a little-endian 16-bit sum at 0x30, then the bytes
 * it covers (0x32..0x1FF). Looks like a save block. */
typedef struct UnkSaveBlock {
    /* 0x000 */ u8 pad0[0x30];
    /* 0x030 */ u8 sumLo;
    /* 0x031 */ u8 sumHi;
    /* 0x032 */ u8 data[0x1CE];
} UnkSaveBlock; /* size = 0x200 */

extern Unk800FEC00 D_800FEC00;
extern s32 D_ovl15_801FBB78;
extern s32 D_ovl15_801FBB7C;
extern s32 D_ovl15_801FBB80;

void func_ovl15_801F7664(s32 *arg0);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F4A40);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F4B20);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F4E70);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F4F50);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F54F8);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F5570);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F5644);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F5AF0);

s32 nuContRmbCheck(u32 contNo);
extern s32 D_801049C0[];
extern s32 D_80105218;

/* Returns 1 if nuContRmbCheck() returns 0 for every entry of D_801049C0
 * (D_80105218 entries), else 0. */
s32 func_ovl15_801F5EA8(void) {
    s32 *port;
    s32 i;
    s32 ok = 1;

    i = 0;
    if (D_80105218 != 0) {
        port = D_801049C0;
        do {
            s32 value = *port;

            port++;
            i++;
            ok &= -(nuContRmbCheck(value) == 0);
        } while (i != D_80105218);
    }
    return ok;
}

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F5F1C);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F6388);

void func_ovl15_801F6624(void) {
}

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F662C);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F66EC);

void func_ovl15_801F67F4(void) {
    D_800FEC00.unk0 = 0;
    D_800FEC00.unk4 = 0;
    D_800FEC00.unk8 = 0;
    D_800FEC00.unk8C = 0;
}

void func_ovl15_801F6818(void) {
    D_800FEC00.unk34 = 0;
    D_800FEC00.unk38 = 0;
    D_800FEC00.unk8C = 0;
}

void func_ovl15_801F6834(void) {
    D_800FEC00.unk34 = 0;
    D_800FEC00.unk4 = 0;
    D_800FEC00.unk38 = 0;
    D_800FEC00.unk8C = 0;
}

s32 func_ovl15_801F6858(UnkSaveBlock *arg0) {
    return arg0->sumLo | (arg0->sumHi << 8);
}

s32 func_ovl15_801F686C(UnkSaveBlock *arg0) {
    s32 sum = 0;
    s32 i;

    /* Raw byte offsets with `!=`: using `<` or indexing data[] changes the loop. */
    for (i = 0x32; i != 0x200; i++) {
        sum += ((u8 *)arg0)[i];
    }
    return sum;
}

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F6890);

/* Object passed to func_ovl15_801F68FC; only the mode at 0x7A8 is known. */
typedef struct UnkOvl15_801F68FC {
    /* 0x000 */ u8 pad0[0x7A8];
    /* 0x7A8 */ s32 unk7A8;
} UnkOvl15_801F68FC;

void func_800719A0(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5, s32 arg6, s32 arg7, s32 arg8, s32 arg9);
void func_80071CE4(s32 *arg0, s32 arg1);

void func_ovl15_801F68FC(s32 *arg0, UnkOvl15_801F68FC *arg1) {
    s32 value = *arg0;
    s32 mode = arg1->unk7A8;
    s32 id = -1;

    switch (mode) {
        case 1:
            id = 0x160;
            break;
        case 5:
            id = 0x162;
            break;
        case 3:
            id = 0x161;
            break;
    }
    if (id != -1) {
        func_800719A0(0x1C, 0xBA, id, 1, 3, 0x20, 0xFF, 0xFF, 0xFF, 0xFF);
        func_80071CE4(&value, 3);
    }
    *arg0 = value;
}

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F69B8);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F6E94);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F7074);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F71F8);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F7664);

void func_ovl15_801F77D4(s32 *arg0) {
    s32 tmp = *arg0;

    func_ovl15_801F7664(&tmp);
    *arg0 = tmp;
}

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F780C);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F7ED4);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F8110);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F8BC8);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F8C60);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F91E4);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F93DC);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F9688);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F9CEC);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F9D80);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801FA0BC);

void func_ovl15_801FA320(s32 arg0, s32 arg1) {
    D_ovl15_801FBB78 = arg0;
    if (arg0 == 1) {
        D_ovl15_801FBB80 = 0;
        D_ovl15_801FBB7C = arg1;
    }
    if (arg0 == 2) {
        D_ovl15_801FBB80 = 4;
    }
}

s32 func_ovl15_801FA35C(void) {
    return D_ovl15_801FBB78;
}

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801FA368);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801FA720);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801FA844);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801FA990);

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801FB348);
