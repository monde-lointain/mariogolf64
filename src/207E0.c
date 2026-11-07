#include "common.h"

/* Placeholder for the object passed to func_80045A9C; real type unknown. */
typedef struct Unk80045A9C {
    /* 0x00 */ u8 pad0[0x10];
    /* 0x10 */ s32 unk10;
    /* 0x14 */ u8 pad14[0x4];
    /* 0x18 */ s32 unk18;
} Unk80045A9C;

extern s32 D_800BE62C;
extern s32 D_800BE688;
extern s32 D_800BE68C;
extern u8 D_800BE6D8;

void func_80040E3C(s32 arg0, s32 arg1);
void func_8004CDA0(void);

INCLUDE_ASM("asm/nonmatchings/207E0", func_800453E0);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80045734);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80045910);

void func_80045A9C(Unk80045A9C *arg0) {
    func_80040E3C(arg0->unk10, arg0->unk18);
}

s32 func_80045AC0(void) {
    return D_800BE62C == 4;
}

INCLUDE_ASM("asm/nonmatchings/207E0", func_80045AD4);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80045B14);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80045C00);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80045CE0);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80046604);

INCLUDE_ASM("asm/nonmatchings/207E0", func_800467DC);

INCLUDE_ASM("asm/nonmatchings/207E0", func_8004683C);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80046898);

s32 func_800469E0(void) {
    return D_800BE688;
}

INCLUDE_ASM("asm/nonmatchings/207E0", func_800469EC);

INCLUDE_ASM("asm/nonmatchings/207E0", func_800479C0);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80047B34);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80047CAC);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80047D68);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80047DBC);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80047E9C);

INCLUDE_ASM("asm/nonmatchings/207E0", func_800484F8);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80048690);

INCLUDE_ASM("asm/nonmatchings/207E0", func_8004876C);

INCLUDE_ASM("asm/nonmatchings/207E0", func_800487E4);

INCLUDE_ASM("asm/nonmatchings/207E0", func_8004887C);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80048CF8);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80048D7C);

INCLUDE_ASM("asm/nonmatchings/207E0", func_8004AC04);

INCLUDE_ASM("asm/nonmatchings/207E0", func_8004B474);

void func_8004C510(s32 arg0) {
    D_800BE68C = arg0;
}

INCLUDE_ASM("asm/nonmatchings/207E0", func_8004C51C);

void func_8004C6A8(void) {
}

void func_8004C6B0(void) {
}

void func_8004C6B8(void) {
}

INCLUDE_ASM("asm/nonmatchings/207E0", func_8004C6C0);

INCLUDE_ASM("asm/nonmatchings/207E0", func_8004C860);

INCLUDE_ASM("asm/nonmatchings/207E0", func_8004C958);

INCLUDE_ASM("asm/nonmatchings/207E0", func_8004CDA0);

void func_8004D148(void) {
    s32 i;

    D_800BE6D8 = 1;
    for (i = 0; i < 0x40; i++) {
        func_8004CDA0();
    }
}
