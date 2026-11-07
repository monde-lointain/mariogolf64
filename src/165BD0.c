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

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F5EA8);

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

INCLUDE_ASM("asm/nonmatchings/165BD0", func_ovl15_801F68FC);

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
