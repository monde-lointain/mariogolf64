#include "common.h"

/* Placeholder types; real types unknown. Rename once identified. */

/* 0xA8-byte entries of D_ovl0_8022B5D0 (10 of them). */
typedef struct UnkOvl0_8022B5D0 {
    /* 0x00 */ u8 pad0[0xA7];
    /* 0xA7 */ u8 unkA7;
} UnkOvl0_8022B5D0; /* size = 0xA8 */

/* Object passed to func_ovl0_80207014 / 80207048 / 802078C4: three s8
 * flags at 0x78. */
typedef struct UnkOvl0_80207014 {
    /* 0x00 */ u8 pad0[0x78];
    /* 0x78 */ s8 unk78[3];
} UnkOvl0_80207014;

/* 0x10 bytes at D_800FBE78; only unk4 and unkC are written here. */
typedef struct Unk800FBE78 {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ s32 unk4;
    /* 0x8 */ s32 unk8;
    /* 0xC */ s32 unkC;
} Unk800FBE78; /* size = 0x10 */

extern UnkOvl0_8022B5D0 D_ovl0_8022B5D0[10];
extern Unk800FBE78 D_800FBE78;
extern s32 D_ovl0_80227198;
extern s32 D_ovl0_80227C90[2];
extern s32 D_ovl0_80227C98;
extern s32 D_800C3070;
extern s32 D_800C3080;
extern u16 D_800C307A;
extern u16 D_801B55CC;
extern s8 D_801B60B9;
extern s32 D_801B608C;

void func_80054240(s32 arg0);
void func_800560FC(s32 arg0, s32 arg1);
void func_8004E47C(s32 *arg0);
void func_80095D64(void);
void func_ovl0_80203B4C(void);
s32 func_ovl0_80207250(s32 arg0);
s32 func_ovl0_802072AC(s32 arg0);

void func_ovl0_80202A00(void) {
    nuSiSendMesg(0x305, NULL);
}

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80202A20);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80202D04);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_802038D0);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80203B4C);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80203DAC);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80203E58);

void func_ovl0_80203EEC(void) {
    s32 sp10[4]; /* unused; only sizes the stack frame (0x28) to match */

    D_800FBE78.unk4 = 0xFFFF1000;
    D_800FBE78.unkC = -0x4E00;
    D_800C3070 = 0;
    D_800C3080 = 0;
    func_ovl0_80203B4C();
}

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80203F30);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80204104);

void func_ovl0_802041C8(void) {
    if (D_801B55CC & 2) {
        D_800C307A -= 200;
    } else if (D_801B55CC & 1) {
        D_800C307A += 200;
    }
}

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80204210);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_8020439C);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80204460);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80204568);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_802045BC);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_8020466C);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80204910);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_802054CC);

void func_ovl0_802055F8(void) {
    D_ovl0_80227198 = osGetCount();
}

s32 func_ovl0_8020561C(void) {
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80205624);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_802059C0);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80205AE4);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80205BB8);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80205C78);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80205D44);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80205DE8);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80206354);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80206BD0);

s32 func_ovl0_80206CC8(void) {
    if (D_801B60B9 == 0) {
        return D_801B608C == 9;
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80206CF0);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80206E88);

s32 func_ovl0_80207014(UnkOvl0_80207014 *arg0) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (arg0->unk78[i] != 0) {
            return 0;
        }
    }
    return 1;
}

s32 func_ovl0_80207048(UnkOvl0_80207014 *arg0) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (arg0->unk78[i] == 1) {
            return 0;
        }
    }
    return 1;
}

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80207080);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_802071B8);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80207250);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_802072AC);

void func_ovl0_80207328(s32 arg0) {
    func_800560FC(arg0, func_ovl0_80207250(arg0));
}

void func_ovl0_80207358(s32 arg0) {
    func_800560FC(arg0, func_ovl0_802072AC(arg0));
}

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80207388);

void func_ovl0_80207660(void) {
}

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80207668);

void func_ovl0_802078C4(UnkOvl0_80207014 *arg0) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (arg0->unk78[i] >= 2) {
            arg0->unk78[i] = 0;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_802078F4);

void func_ovl0_80207A10(void) {
    func_80054240(0);
    func_80095D64();
}

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80207A34);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80207B8C);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80207BEC);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80207CAC);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80207E88);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80207F70);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_802080B4);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_802080D4);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80208120);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_8020816C);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_802081F8);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80208354);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_8020845C);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_802087D0);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80208810);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_80208850);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_802088B0);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_8020D6F0);

void func_ovl0_8020D784(void) {
    func_8004E47C(&D_ovl0_80227C90[0]);
    func_8004E47C(&D_ovl0_80227C90[1]);
    func_8004E47C(&D_ovl0_80227C98);
}

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_8020D7C4);

s32 func_ovl0_8020D838(void) {
    UnkOvl0_8022B5D0 *p;
    s32 i;

    for (i = 0; i != 10; i++) {
        p = &D_ovl0_8022B5D0[i];
        if (p->unkA7 == 2) {
            return 1;
        }
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_8020D874);

void func_ovl0_8020D9F0(void) {
    UnkOvl0_8022B5D0 *p;
    s32 i;

    for (i = 0; i != 10; i++) {
        p = &D_ovl0_8022B5D0[i];
        p->unkA7 = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_8020DA18);

INCLUDE_ASM("asm/nonmatchings/BBD00", func_ovl0_8020DCA4);
