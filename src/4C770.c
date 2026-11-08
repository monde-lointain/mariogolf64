#include "common.h"

/* Placeholder types; real types unknown. Rename once identified. */

/* 0x2C-byte entries of D_8012F510. splat labels the fields separately
 * (D_8012F51C = unkC, D_8012F524 = unk14, D_8012F52C = unk1C). */
typedef struct Unk8012F510 {
    /* 0x00 */ u8 pad0[0xC];
    /* 0x0C */ s32 unkC;
    /* 0x10 */ u8 pad10[0x4];
    /* 0x14 */ s32 unk14;
    /* 0x18 */ u8 pad18[0x4];
    /* 0x1C */ s32 unk1C[4];
} Unk8012F510; /* size = 0x2C */

/* 3-byte entries of D_8010CFA8 (see func_800718F4). */
typedef struct Unk8010CFA8 {
    /* 0x0 */ u8 unk0;
    /* 0x1 */ u8 unk1;
    /* 0x2 */ u8 unk2;
} Unk8010CFA8; /* size = 0x3 */

/* 0x48-byte entries of D_800FF570 (see func_8007512C). */
typedef struct Unk800FF570 {
    /* 0x00 */ u8 pad0[0x3];
    /* 0x03 */ s8 unk3;
    /* 0x04 */ u8 pad4[0x44];
} Unk800FF570; /* size = 0x48 */

/* 4-byte entries of the array D_800E1C48 points to (see func_80071C9C). */
typedef struct Unk800E1C48 {
    /* 0x0 */ u16 unk0;
    /* 0x2 */ u16 unk2;
} Unk800E1C48; /* size = 0x4 */

extern Unk8012F510 D_8012F510[];
extern Unk8010CFA8 D_8010CFA8[];
extern Unk800FF570 D_800FF570[];
extern Unk800E1C48 *D_800E1C48;
extern u8 D_801321B0[];

INCLUDE_ASM("asm/nonmatchings/4C770", func_80071370);

s32 func_8004E47C(Unk800E1C48 **);
extern Unk800E1C48 *D_800E1C30;
extern Unk800E1C48 *D_800E1C34;
extern Unk800E1C48 *D_800E1C38;
extern Unk800E1C48 *D_800E1C44;
extern Unk800E1C48 *D_800E1C4C;
extern Unk800E1C48 *D_8012F4E0;

void func_800715A0(void) {
    func_8004E47C(&D_8012F4E0);
    func_8004E47C(&D_800E1C30);
    func_8004E47C(&D_800E1C34);
    func_8004E47C(&D_800E1C38);
    func_8004E47C(&D_800E1C4C);
    func_8004E47C(&D_800E1C44);
    func_8004E47C(&D_800E1C48);
}

INCLUDE_ASM("asm/nonmatchings/4C770", func_80071608);

void func_800718C4(void) {
    s32 i;

    for (i = 0; i != 12; i++) {
        D_8012F510[i].unk14 = -1;
    }
}

void func_800718F4(u8 arg0, u8 arg1, u8 arg2, s32 arg3) {
    D_8010CFA8[arg3].unk0 = arg0;
    D_8010CFA8[arg3].unk1 = arg1;
    D_8010CFA8[arg3].unk2 = arg2;
}

void func_80071924(s32 arg0, s32 arg1, s32 arg2) {
    D_8012F510[arg1].unk1C[arg2] = arg0;
}

void func_80071954(u8 *src) {
    u8 *dst = D_801321B0;
    u8 *end = src + 0x13;
    u8 c;

    do {
        c = *src;
        *dst = c;
        if (c >= 'a' && c <= 'z') {
            *dst = c + 0x80;
        }
        if (*src == 0) {
            break;
        }
        src++;
        dst++;
    } while (src != end);
    D_801321B0[0x13] = 0;
}

INCLUDE_ASM("asm/nonmatchings/4C770", func_800719A0);

INCLUDE_ASM("asm/nonmatchings/4C770", func_80071B34);

void func_80071C74(s32 arg0) {
    D_8012F510[arg0].unkC = 0xFF;
}

s32 func_80071C9C(s32 arg0) {
    Unk8012F510 *p = &D_8012F510[arg0];

    /* Indexed as [unk14 + 1]: a field-at-0x4 struct picks different registers. */
    return p->unkC >= D_800E1C48[p->unk14 + 1].unk2;
}

INCLUDE_ASM("asm/nonmatchings/4C770", func_80071CE4);

INCLUDE_ASM("asm/nonmatchings/4C770", func_80072A08);

INCLUDE_ASM("asm/nonmatchings/4C770", func_800734F0);

INCLUDE_ASM("asm/nonmatchings/4C770", func_800738BC);

INCLUDE_ASM("asm/nonmatchings/4C770", func_8007399C);

s32 func_80073BF0(u8 *str) {
    s32 len = 0;

    while (*str != 0) {
        str++;
        len++;
    }
    return len;
}

INCLUDE_ASM("asm/nonmatchings/4C770", func_80073C14);

INCLUDE_ASM("asm/nonmatchings/4C770", func_80073F24);

INCLUDE_ASM("asm/nonmatchings/4C770", func_80074230);

INCLUDE_ASM("asm/nonmatchings/4C770", func_80074500);

u32 func_800738BC(s32, s32, s32);
s32 func_80073F24(s32 *, s16, s16, s32);

void func_800747B0(s32 *arg0, s32 arg1, s16 arg2, s32 arg3) {
    s32 sp10;
    u32 temp_v0;

    sp10 = *arg0;
    temp_v0 = func_800738BC(arg3, 0xC, 2);
    func_80073F24(&sp10, (s16) (arg1 - ((s32) ((temp_v0 >> 0x1F) + temp_v0) >> 1)), arg2, arg3);
    *arg0 = sp10;
}

s32 func_80074230(s32 *, s16, s16, s32);

void func_80074840(s32 *arg0, s32 arg1, s16 arg2, s32 arg3) {
    s32 sp10;
    u32 temp_v0;

    sp10 = *arg0;
    temp_v0 = func_800738BC(arg3, 0xA, 2);
    func_80074230(&sp10, (s16) (arg1 - ((s32) ((temp_v0 >> 0x1F) + temp_v0) >> 1)), arg2, arg3);
    *arg0 = sp10;
}

s32 func_80074500(s32 *, s16, s16, s32);

void func_800748D0(s32 *arg0, s32 arg1, s16 arg2, s32 arg3) {
    s32 sp10;
    u32 temp_v0;

    sp10 = *arg0;
    temp_v0 = func_800738BC(arg3, 8, 2);
    func_80074500(&sp10, (s16) (arg1 - ((s32) ((temp_v0 >> 0x1F) + temp_v0) >> 1)), arg2, arg3);
    *arg0 = sp10;
}

void func_80074960(void) {
}

INCLUDE_ASM("asm/nonmatchings/4C770", func_80074968);

INCLUDE_ASM("asm/nonmatchings/4C770", func_80074CA8);

INCLUDE_ASM("asm/nonmatchings/4C770", func_80074D0C);

INCLUDE_ASM("asm/nonmatchings/4C770", func_80074E5C);

INCLUDE_ASM("asm/nonmatchings/4C770", func_80074EFC);

INCLUDE_ASM("asm/nonmatchings/4C770", func_80075010);

void func_8007512C(void) {
    s32 i;

    for (i = 0; i != 300; i++) {
        D_800FF570[i].unk3 = -1;
    }
}

INCLUDE_ASM("asm/nonmatchings/4C770", func_8007515C);

INCLUDE_ASM("asm/nonmatchings/4C770", func_800754BC);

INCLUDE_ASM("asm/nonmatchings/4C770", func_8007580C);

INCLUDE_ASM("asm/nonmatchings/4C770", func_80075E48);

INCLUDE_ASM("asm/nonmatchings/4C770", func_800760CC);

INCLUDE_ASM("asm/nonmatchings/4C770", func_80076138);

INCLUDE_ASM("asm/nonmatchings/4C770", func_8007624C);
