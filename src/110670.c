#include "common.h"

extern s32 D_ovl3_80230E88;
extern f32 D_ovl3_80230E8C;
extern f32 D_ovl3_80230E90;
extern s32 D_ovl3_80230E9C;
extern s32 D_ovl3_80230EA0;
extern u8 D_ovl3_80230F40;
/* splat labels the byte at 0x80105DC0 as D_80105DBC + 4. */
extern s8 D_80105DBC[];

void *func_8005AF50(void);
void *func_8005AF74(void);

void func_ovl3_802266F0(void) {
    D_ovl3_80230E88 = 0;
    D_ovl3_80230E8C = 0.0f;
    D_ovl3_80230E90 = 0.0f;
    D_ovl3_80230E9C = 0;
    D_ovl3_80230EA0 = 0;
}

void func_ovl3_8022671C(void) {
    D_ovl3_80230E9C = 0;
    D_ovl3_80230EA0 = 0;
    D_ovl3_80230E88 = 1;
    D_ovl3_80230E8C = 1.0f;
    D_ovl3_80230E90 = 1.0f;
}

void func_ovl3_80226754(void) {
    D_ovl3_80230E88 = 2;
}

s32 func_ovl3_80226764(void) {
    if (D_ovl3_80230E88 != 0) {
        return D_ovl3_80230E88 == 1;
    }
    return -1;
}

s32 func_ovl3_80226788(void) {
    if (D_ovl3_80230E88 != 2) {
        return D_ovl3_80230E88 == 3;
    }
    return -1;
}

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_802267B0);

/* Look up a 16-bit id in a table, fill a 0x20-byte buffer for it via
 * func_8005062C and pass both to func_800506D4 (cf. func_8006ACD8 in 456C0). */
s32 func_8005062C(s32 id, void *buf);
void func_800506D4(s32 handle, void *buf);

extern u16 D_800C2074[];
extern s32 D_ovl3_80230E7C;
extern s32 D_ovl3_80230E80;
extern u16 D_ovl3_80230EA4[];

void func_ovl3_80226904(s32 arg0) {
    s32 sp10[8];

    if (arg0 < 0) {
        arg0 = 0;
    }
    if (arg0 >= 10) {
        arg0 = 9;
    }
    func_8005062C(D_800C2074[arg0], sp10);
    func_800506D4(D_ovl3_80230E7C, sp10);
}

void func_ovl3_80226958(s32 arg0) {
    s32 sp10[8];

    if (arg0 < 0) {
        arg0 = 0;
    }
    if (arg0 >= 15) {
        arg0 = 14;
    }
    func_8005062C(D_ovl3_80230EA4[arg0], sp10);
    func_800506D4(D_ovl3_80230E80, sp10);
}

/* func_800504E8 / 80050598 / 800505A0 (2B800) work on a 0x10-byte object. */
s32 func_800504E8(s32 id, void *obj);
s16 func_80050598(void *obj);
s32 func_800505A0(s32 handle, s16 arg1, void *obj);

extern s32 D_ovl3_80230E84;
extern s16 D_ovl3_80230EC4[15][4];

void func_ovl3_802269AC(s32 arg0, s32 arg1) {
    s32 sp10[4];

    if (arg0 < 0) {
        arg0 = 0;
    }
    if (arg0 >= 15) {
        arg0 = 14;
    }
    if (arg1 < 0) {
        arg1 = 0;
    }
    if (arg1 >= 4) {
        arg1 = 3;
    }
    func_800504E8(D_ovl3_80230EC4[arg0][arg1], sp10);
    func_800505A0(D_ovl3_80230E84, func_80050598(sp10), sp10);
}

void func_ovl3_80226A34(void) {
    func_8005AF50();
    func_8005AF74();
}

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_80226A58);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_8022747C);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_802287EC);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_80228A80);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_80228B60);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_80228C00);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_80229278);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_802296C4);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_80229CB0);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_8022A090);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_8022A43C);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_8022A8F4);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_8022ADE8);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_8022B360);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_8022B46C);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_8022B5E0);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_8022B668);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_8022B75C);

INCLUDE_ASM("asm/nonmatchings/110670", func_ovl3_8022B7E0);

s32 func_ovl3_8022BDB0(void) {
    return D_80105DBC[4] == -1;
}

s32 func_ovl3_8022BDC4(void) {
    return D_80105DBC[4];
}

s32 func_ovl3_8022BDD0(void) {
    if (D_ovl3_80230F40 != 0) {
        D_ovl3_80230F40 = 0;
        return 1;
    }
    return 0;
}

void func_ovl3_8022BDF8(void) {
}
