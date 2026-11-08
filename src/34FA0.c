#include "common.h"

/* Placeholder types; real types unknown. Rename once identified. */

/* D_8012F720, returned by func_8005AF50. */
typedef struct Unk8012F720 {
    /* 0x00 */ u8 pad0[0x28];
    /* 0x28 */ s8 unk28;
    /* 0x29 */ s8 unk29;
    /* 0x2A */ s8 unk2A;
    /* 0x2B */ s8 unk2B;
} Unk8012F720;

/* D_801323A0, returned by func_8005AF74. */
typedef struct Unk801323A0 {
    /* 0x00 */ u8 pad0[0x2E];
    /* 0x2E */ u8 unk2E;
    /* 0x2F */ u8 unk2F;
} Unk801323A0;

extern Unk8012F720 D_8012F720;
extern Unk801323A0 D_801323A0;
extern s32 D_800C2B28[6];
extern u32 D_8012F724;
extern u8 D_8012D400[3];
extern s8 D_80105DC1;
extern s8 D_80105DC2;
extern s32 D_80105DC4;
extern s32 D_80105DC8;
extern s32 D_80105DCC;
extern s32 D_801B6094;
extern s32 D_801B6098;
extern s32 D_800FE334;
extern u8 D_801052E8;
extern u8 D_801B9410;
extern u8 D_800C2BD4;
extern s32 D_801EFFA0;

void func_80099490(void);
void func_8005DF54(Unk8012F720 *arg0, s32 arg1);
Unk8012F720 *func_8005AF50(void);
Unk801323A0 *func_8005AF74(void);
s32 func_8005D23C(void);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_80059BA0);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_80059BC0);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_80059FAC);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005A2AC);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005A580);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005ACF8);

Unk8012F720 *func_8005AF50(void) {
    func_8005AF74();
    return &D_8012F720;
}

Unk801323A0 *func_8005AF74(void) {
    return &D_801323A0;
}

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005AF80);

void func_8005B03C(void) {
    Unk8012F720 *p = func_8005AF50();

    func_80099490();
    func_8005DF54(p, 0);
}

s32 func_8005B070(s32 arg0) {
    s32 i;

    for (i = 5; i != 0; i--) {
        if (arg0 >= D_800C2B28[i]) {
            break;
        }
    }
    return i + 1;
}

s32 func_8005B0A0(s32 arg0) {
    return D_800C2B28[arg0];
}

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005B0B4);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005B150);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005B28C);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005B314);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005B7BC);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005BC10);

void func_8005C018(u32 arg0) {
    u32 *p = &D_8012F724;

    if (*p < arg0) {
        *p = arg0;
    }
}

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005C038);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005C458);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005C4B4);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005C510);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005C5B4);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005C614);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005C674);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005CA48);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005CEE0);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005CF78);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005D0D8);

void func_8005D1E4(void) {
    Unk801323A0 *p = func_8005AF74();

    p->unk2E = D_801B6098;
    p->unk2F = D_801B6094;
}

s32 func_8005D218(void) {
    /* case 0 shares the default; the redundant test is in the original. */
    switch (D_80105DC1) {
        case 0:
        default:
            return 0;
        case 1:
            return 7;
    }
}

s32 func_8005D23C(void) {
    return D_80105DC2;
}

s32 func_8005D248(void) {
    return D_80105DC4 != 0;
}

s32 func_8005D258(void) {
    s32 v = D_80105DC8;

    if (v < 0) {
        v = 0;
    }
    return v + 0x16F;
}

s32 func_8005D274(void) {
    s32 v = func_8005D23C();

    if (v < 0) {
        v = 0;
    }
    return v + 0x1EB;
}

s32 func_8005D2A0(void) {
    s32 v = D_80105DCC;

    if (v <= 0) {
        v = 1;
    }
    return v;
}

void func_8005D2B8(u8 *dst) {
    s32 i;

    for (i = 0; i != 3; i++) {
        *dst++ = D_8012D400[i];
    }
}

s32 func_8005D2E4(void) {
    /* case 0 shares the default; the redundant test is in the original. */
    switch (D_80105DC1) {
        case 0:
        default:
            return 0x9E;
        case 1:
            return 0x9D;
    }
}

s32 func_8005D308(void) {
    s32 v = D_80105DC2;
    s32 ret;

    if (v >= 6) {
        if (v >= 9) {
            ret = 0xC8;
        } else {
            ret = 0xC7;
        }
    } else {
        ret = v + 0x1EB;
    }
    return ret;
}

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005D334);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005D3B8);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005D9A0);

void func_8005DAD4(void) {
    D_800FE334 = 0;
    D_801052E8 = 1;
    D_801B9410 = 0;
    D_800C2BD4 = 0;
}

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005DAFC);

s32 func_8005D9A0();
extern s8 D_800C1FF4;
extern s8 D_800C1FF5;
extern s8 D_800C1FF6;
extern s8 D_800C1FF7;
extern s8 D_800FE3EC;
extern s8 D_800FE402;
extern s8 D_800FE418;
extern s8 D_800FE42E;

void func_8005DC50(void) {
    func_8005D9A0();
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    D_800FE3EC = 0;
    D_800FE402 = 0;
    D_800FE418 = 0;
    D_800FE42E = 0;
    D_800C1FF4 = -1;
    D_800C1FF5 = -1;
    D_800C1FF6 = -1;
    D_800C1FF7 = -1;
    D_801EFFA0 = 0;
}

void func_8005DCDC(void) {
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    D_801EFFA0 = 0x17;
}

void func_8005DD10(void) {
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    D_801EFFA0 = 5;
}

void func_8005DD44(void) {
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    D_801EFFA0 = 0x12;
}

void func_8005DD78(void) {
    D_801052E8 = 1;
    D_801B9410 = 1;
    D_800FE334 = 0;
    D_800C2BD4 = 0;
    D_801EFFA0 = 0xD;
}

s32 func_80029A6C(s32);
extern s8 D_801B60B9;

void func_8005DDAC(void) {
    D_800FE334 = 0;
    D_801052E8 = 1;
    D_800C2BD4 = 0;
    D_801B9410 = 1;
    D_801B60B9 = 0;
    func_80029A6C(0x1E);
    D_801EFFA0 = 0xC;
}

s32 func_8005DE00(void) {
    return func_8005AF50()->unk28;
}

s32 func_8005DE20(void) {
    return func_8005AF50()->unk29;
}

s32 func_8005DE40(void) {
    return func_8005AF50()->unk2B;
}

void func_8005DE60(void) {
    func_8005DF54(func_8005AF50(), 1);
}

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005DE88);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005DF54);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005DFE8);

INCLUDE_ASM("asm/nonmatchings/34FA0", func_8005E180);
