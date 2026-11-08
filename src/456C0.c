#include "common.h"

extern s32 D_800C4010;
extern s32 D_800C4014;
extern s32 D_800C4018;
extern s32 D_800C4020;
extern s32 D_800C4060;
extern s32 D_800C4064;
extern s32 D_800C4068;
extern s32 D_800C4124;
extern s32 D_800C4144;
extern s32 D_800E1C00;
extern s32 D_800E1C04;
extern s32 D_800E1C08;
extern s32 D_800E1C0C;
extern s32 D_800FF4B0;
extern s32 D_800FF4B4;
extern s32 D_800FF4B8;

s32 func_8004E2DC(s32 arg0);
void func_8004E47C(s32 *arg0);
s32 func_8005062C(s32 arg0, void *arg1);
void func_800506D4(s32 arg0, void *arg1);
void func_8006B54C(void);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006A2C0);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006A4A0);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006A548);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006A5E4);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006A84C);

void func_8006ACD8(void) {
    s32 sp10[8];
    s32 handle;

    handle = func_8004E2DC(func_8005062C(0x63F, sp10));
    D_800E1C04 = handle;
    func_800506D4(handle, sp10);
    func_8006B54C();
}

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006AD1C);

void func_8006AD88(void) {
    D_800C4020 = 0;
}

s32 func_8006AD94(void) {
    return D_800C4020;
}

extern f32 D_800C401C;

s32 func_8006ADA0(void) {
    if (D_800C4010 == 0) {
        return (s32) D_800C401C;
    }
    switch (D_800C4014) {
    case 1:
    default:
        return 0;
    case 3:
        D_800C4010 = 0;
        return 0x14;
    }
}

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006ADF8);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006AEA4);

void func_8006B54C(void) {
    D_800C4068 = -1;
}

s32 func_8006B55C(void) {
    return D_800C4064 < 2;
}

void func_8006B56C(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
    D_800C4068 = 0;
    D_800C4060 = arg3;
    D_800C4064 = arg3;
    D_800FF4B0 = arg0;
    D_800FF4B4 = arg1;
    D_800FF4B8 = arg2;
}

void func_8006B5A0(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
    D_800C4068 = 1;
    D_800C4060 = arg3;
    D_800C4064 = arg3;
    D_800FF4B0 = arg0;
    D_800FF4B4 = arg1;
    D_800FF4B8 = arg2;
}

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006B5D8);

void func_8006B980(void) {
    s32 sp10[8];
    s32 handle;
    s32 handle2;

    handle = func_8004E2DC(func_8005062C(0x70B, sp10));
    D_800E1C08 = handle;
    func_800506D4(handle, sp10);
    handle2 = func_8004E2DC(func_8005062C(0x70C, sp10));
    D_800E1C0C = handle2;
    func_800506D4(handle2, sp10);
}

void func_8006B9E4(void) {
    func_8004E47C(&D_800E1C08);
    func_8004E47C(&D_800E1C0C);
}

s32 func_8006BA10(void) {
    return D_800C4124 == -1;
}

/* 0x10-byte entries of D_800C406C. Placeholder. */
typedef struct Unk800C406C {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ s32 unk4;
    /* 0x8 */ u8 pad8[0x8];
} Unk800C406C;

void func_80050BF0(s32 arg0, s32 arg1, s32 arg2);

extern Unk800C406C D_800C406C[];
extern s32 D_800C0E60;
extern s32 D_800C411C;
extern s32 D_800C4120;
extern s32 D_800C4128;

void func_8006BA24(s32 arg0) {
    Unk800C406C *entry;

    if (arg0 != -1) {
        entry = &D_800C406C[arg0];
        D_800C4120 = 0;
        D_800C411C = arg0;
        func_8006B980();
        D_800C4124 = 0;
        D_800C4128 = entry->unk4;
        func_80050BF0(0x64, D_800C0E60, 0x64);
    }
}

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006BA94);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006BC80);

void func_8006C3E8(void) {
    s32 sp10[8];
    s32 handle;

    if (D_800C4010 == 0) {
        handle = func_8004E2DC(func_8005062C(0x955, sp10));
        D_800E1C00 = handle;
        func_800506D4(handle, sp10);
    }
    D_800C4010 = 1;
    D_800C4014 = 0;
    D_800C4018 = 0x90;
}

void func_8006C450(void) {
    if (D_800C4010 != 0) {
        func_8004E47C(&D_800E1C00);
    }
    D_800C4010 = 0;
}

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006C484);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006C8CC);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006C918);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006CD50);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006CE88);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006D058);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006D164);

s32 func_8006D1FC(void) {
    return D_800C4144;
}

void func_8006D208(s32 arg0) {
    D_800C4144 = arg0;
}

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006D214);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006D38C);

void func_8006D4DC(void) {
    D_800C4144 = -1;
}

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006D4EC);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006D6D0);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006DDCC);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006DE44);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006DEB4);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006DF84);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006DFF0);

INCLUDE_ASM("asm/nonmatchings/456C0", func_8006E210);
