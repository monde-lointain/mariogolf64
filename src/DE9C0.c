#include "common.h"

/* D_801323A0: placeholder, real type unknown. splat labels part of it as
 * D_801323B8 (= +0x18), so 0x2B / 0x2C appear as D_801323B8 + 0x13 / 0x14. */
typedef struct Unk801323A0 {
    /* 0x00 */ u8 pad0[0x2];
    /* 0x02 */ u8 unk2;
    /* 0x03 */ u8 pad3[0x13];
    /* 0x16 */ s8 unk16;
    /* 0x17 */ u8 pad17[0x14];
    /* 0x2B */ s8 unk2B;
    /* 0x2C */ s8 unk2C;
    /* 0x2D */ u8 pad2D[0x2];
    /* 0x2F */ s8 unk2F;
} Unk801323A0;

extern Unk801323A0 D_801323A0;
extern s8 D_801B60C2;
extern s32 D_800C2004;
extern s32 D_801EFFA0;
extern u8 D_ovl3_80232300[];
extern u8 D_ovl3_80231198[];
extern u8 D_ovl3_80231284[];

void func_80050BF0(s32 arg0, s32 arg1, s32 arg2);
void func_80074D0C(s32 *arg0, s32 arg1, s32 arg2, void *arg3);
void func_ovl3_801FB54C(s32 *arg0, s32 arg1);

u8 *func_ovl3_801F4A40(void) {
    return D_ovl3_80232300;
}

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801F4A4C);

s32 func_ovl3_801F4C34(void) {
    s32 off;

    if (D_801B60C2 != 0) {
        /* Separate statement: inline, GCC reassociates the sum. */
        off = D_801323A0.unk2B * 3 + 6;
        return D_801323A0.unk2C + off;
    }
    return D_801323A0.unk16;
}

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801F4C78);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801F6A38);

void func_ovl3_801F6B20(void) {
}

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801F6B28);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801F70F0);

/* D_800FBD40: an s32 array starting at offset 8 (indexed by D_801323A0.unk2). */
typedef struct Unk800FBD40 {
    /* 0x0 */ u8 pad0[0x8];
    /* 0x8 */ s32 unk8[1]; /* length unknown */
} Unk800FBD40;

extern Unk800FBD40 D_800FBD40;
extern s32 D_ovl3_8022BE0C;

/* s32 with no return: implicit int (empty delay-slot pattern). */
s32 func_ovl3_801F75EC(void) {
    u8 next;

    D_801323A0.unk2 = 0;
    if (D_800FBD40.unk8[0] == 1) {
        do {
            next = D_801323A0.unk2 + 1;
            D_801323A0.unk2 = next;
            if (D_ovl3_8022BE0C - 1 < (s8)next) {
                D_801323A0.unk2 = 0;
            }
        } while (D_800FBD40.unk8[(s8)D_801323A0.unk2] == 1);
    }
}

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801F7660);

s32 func_800543A4();
s32 func_ovl3_801F7660(s32, s32);
extern s32 D_801B7070;

void func_ovl3_801F7CD0(void) {
    s32 temp_v0;

    temp_v0 = func_800959F8();
    if ((temp_v0 == 1) && (func_ovl3_801F7660(1, 2) == -1)) {
        D_801B7070 = temp_v0;
        D_801EFFA0 = 0;
        func_800543A4();
    }
}

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801F7D28);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801F8BAC);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801F93D8);

s32 func_8005B7BC(s32);
extern s32 D_801B608C;

void func_ovl3_801F9BCC(void) {
    if (func_800959F8() == 1) {
        if (D_800C2004 & 0x8000) {
            func_80050BF0(0x5D, 0xF, 0x50);
            D_801EFFA0 = 0x13;
            D_801B608C = 0;
            func_8005B7BC(1);
            return;
        }
        if (D_800C2004 & 0x4000) {
            func_80050BF0(0x5F, 0xF, 0x7F);
            D_801EFFA0 = 0;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801F9C58);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801F9E38);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801F9FF8);

s32 func_800959F8();

void func_ovl3_801FA1B8(void) {
    if ((func_800959F8() == 1) && (D_800C2004 & 0x4000)) {
        func_80050BF0(0x5F, 0xF, 0x50);
        D_801EFFA0 = 2;
    }
}

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801FA20C);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801FADBC);

s32 func_8006AD1C(s32);
s32 func_8006AD94();
s32 func_8006ADA0();
s32 func_8006ADF8(s32);
extern s8 D_801052E8;
extern u8 D_801B9410;

void func_ovl3_801FAF08(void) {
    if ((func_8006AD94() != 0) && (D_801B9410 == 0)) {
        if (func_8006ADA0() == 0) {
            D_801EFFA0 = 0x13;
        }
    } else {
        func_8006AD1C(0);
        func_8006ADF8(1);
        D_801052E8 = 0;
        D_801B9410 = 0;
    }
}

void func_ovl3_801FAF78(void) {
    if ((func_8006AD94() != 0) && (D_801B9410 == 0)) {
        if (func_8006ADA0() == 0) {
            D_801EFFA0 = 0x13;
        }
    } else {
        func_8006AD1C(D_801323A0.unk2F);
        func_8006ADF8(1);
        D_801052E8 = 0;
        D_801B9410 = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801FAFEC);

void func_ovl3_801FB11C(void) {
    if (D_800C2004 & 0x4000) {
        func_80050BF0(0x5F, 0xF, 0x7F);
        D_801EFFA0 = 2;
    }
}

void func_ovl3_801FB15C(void) {
    if (D_800C2004 & 0x8000) {
        func_80050BF0(0x5D, 0xF, 0x50);
        D_801EFFA0 = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801FB198);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801FB54C);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801FC60C);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801FC754);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801FCB74);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801FCEF8);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801FD1B4);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801FD4F0);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801FD7B0);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801FDA40);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_801FE4D0);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_802004A0);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_80200A40);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_80200EF0);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_80201574);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_80202000);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_802047F0);

void func_ovl3_80204FF0(s32 *arg0) {
    s32 tmp = *arg0;

    func_80074D0C(&tmp, 0x60, 0x40, D_ovl3_80231198);
    *arg0 = tmp;
}

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_80205038);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_802058B8);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_80206180);

void func_ovl3_80206A2C(s32 *arg0) {
    s32 tmp = *arg0;

    func_ovl3_801FB54C(&tmp, 0x3D);
    *arg0 = tmp;
}

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_80206A68);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_80207D08);

void func_ovl3_80207E18(void) {
}

void func_ovl3_80207E20(void) {
}

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_80207E28);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_802081E8);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_8020879C);

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_80208F8C);

void func_ovl3_80209430(s32 *arg0) {
    s32 tmp = *arg0;

    func_80074D0C(&tmp, 0x40, 0x20, D_ovl3_80231284);
    *arg0 = tmp;
}

INCLUDE_ASM("asm/nonmatchings/DE9C0", func_ovl3_80209478);
