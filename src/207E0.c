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

s32 func_800510EC(s32, s32);
s32 func_8009226C(s32);
s32 func_80205AE4(s32, s32);
s32 func_80216B74(s32);
extern s32 D_800BE660;
extern s32 D_800BE698;
extern s32 D_800BE69C;
extern s32 D_800C5AAC;
extern s8 D_800CC790;
extern s8 D_800CC7A0;
extern s32 D_800FBE74;
extern s32 D_80132CF0;
extern f32 D_801B5530;
extern s32 D_801B557C;
extern s32 D_801B7F70;
extern s32 D_801B7F74;
extern s32 D_80250AEC;
extern s8 D_80250B61;

void func_80045C00(void) {
    D_800BE698 = 1;
    D_800BE69C = 0;
    D_800BE660 = 0;
    D_800C5AAC = 0;
    D_801B5530 = 45.0f;
    func_8009226C(-1);
    D_801B7F70 = 0;
    D_801B7F74 = 0;
    D_801B557C = 0;
    func_800510EC(0xB, 0x78);
    func_800510EC(0xF, 0x78);
    func_80216B74(0);
    D_800BE62C = -1;
    if (D_80132CF0 == D_80250AEC) {
        osSyncPrintf(&D_800CC790);
        if (D_80250B61 != 0) {
            osSyncPrintf(&D_800CC7A0);
            func_80205AE4(D_800FBE74, 0xB);
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/207E0", func_80045CE0);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80046604);

s32 func_800467DC(s32 arg0) {
    s32 var_s0;

    var_s0 = arg0;
    if (var_s0 == 0) {
        var_s0 = 1;
    }
    return (s32) (guRandom() >> 2) % var_s0;
}

INCLUDE_ASM("asm/nonmatchings/207E0", func_8004683C);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80046898);

s32 func_800469E0(void) {
    return D_800BE688;
}

INCLUDE_ASM("asm/nonmatchings/207E0", func_800469EC);

INCLUDE_ASM("asm/nonmatchings/207E0", func_800479C0);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80047B34);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80047CAC);

extern s32 D_800DAEEC;
extern s32 D_800DAEF0;
extern s32 D_800DAEF4;
extern f32 D_800DAF30;
extern f32 D_800DAF34;
extern f32 D_800DAF38;

void func_80047D68(void) {
    D_800DAF30 = (f32) D_800DAEEC * 0.0009765625f;
    D_800DAF34 = (f32) D_800DAEF0 * 0.0009765625f;
    D_800DAF38 = (f32) D_800DAEF4 * 0.0009765625f;
}

INCLUDE_ASM("asm/nonmatchings/207E0", func_80047DBC);

INCLUDE_ASM("asm/nonmatchings/207E0", func_80047E9C);

INCLUDE_ASM("asm/nonmatchings/207E0", func_800484F8);

s32 func_800326FC(s32, s32, s32, s32);
s32 func_8004887C();
s32 func_800719A0(s32, s32, s32, s32, s32, s32, s32, s32, s32, s32);
extern s32 D_800BE694;
extern s8 D_801061CE;
extern s32 D_801B5534;
extern s32 D_801B5584;

void func_80048690(void) {
    D_800BE694 = 0;
    D_801061CE = 0;
    D_801B7F70 = 0;
    D_801B7F74 = 0;
    D_801B557C = 0;
    D_801B5534 = 1;
    D_801B5584 = 1;
    D_801B5530 = 80.0f;
    func_800719A0(0, 0, -1, 1, 0, 0, 0, 0, 0, 0);
    func_800719A0(0, 0, -1, 1, 1, 0, 0, 0, 0, 0);
    func_800326FC(0xA0, 0x70, 0x80, 0x80);
    func_8004887C();
}

void func_8004876C(void) {
    D_801061CE = 0;
    func_800719A0(0, 0, -1, 1, 0, 0, 0, 0, 0, 0);
    func_800719A0(0, 0, -1, 1, 1, 0, 0, 0, 0, 0);
}

s32 func_800299F4(s32 arg0);
void func_80216130(s32 arg0);
extern s32 D_800BA9F0;
extern s32 D_801B608C;
extern s8 D_801B60C2;

/* s32 with no return: implicit int (empty delay-slot pattern). */
s32 func_800487E4(void) {
    D_800BE694 = -1;
    D_801B7F70 = 0;
    D_801B7F74 = 0;
    D_801B557C = 0;
    if (D_801B608C != 7 && D_801B60C2 == 0) {
        if (func_800299F4(0x48) != 0 || D_800BA9F0 / 0x10000 >= 6) {
            func_80216130(1);
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/207E0", func_8004887C);

f32 func_80059FAC(f32, f32);
extern f32 D_800BE654;
extern f64 D_800CC860;
extern f32 D_800DAF24;
extern f32 D_800DAF2C;

void func_80048CF8(void) {
    f32 temp_fa0;
    f32 temp_fa1;

    temp_fa0 = D_800DAF30 - D_800DAF24;
    temp_fa1 = D_800DAF38 - D_800DAF2C;
    if (((s32) temp_fa0 != 0) || ((s32) temp_fa1 != 0)) {
        D_800BE654 = (f32) (D_800CC860 - (f64) func_80059FAC(temp_fa0, temp_fa1));
    }
}

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
