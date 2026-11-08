#include "common.h"

/* Placeholder for the 0xA00-byte entries cleared by func_ovl4_80250FB4.
 * splat names the fields from neighbouring labels (D_80132D4A + 0x944,
 * D_80133692); the real base and type are unknown. */
typedef struct Unk80132D4A {
    /* 0x000 */ u8 pad0[0x944];
    /* 0x944 */ s16 unk944;
    /* 0x946 */ u8 pad946[0x2];
    /* 0x948 */ s16 unk948;
    /* 0x94A */ u8 pad94A[0xB6];
} Unk80132D4A; /* size = 0xA00 */

extern Unk80132D4A D_80132D4A[];
extern s16 D_800BAC0C[32][16];
extern s32 D_800BB00C;
extern s32 D_ovl4_802542A0;
extern s32 D_ovl4_802543A8;
extern u8 D_ovl4_802550E8[];
extern u8 D_ovl4_802550FC[];
extern char D_ovl4_80255314[];
extern char D_ovl4_80255318[];
extern s32 D_ovl4_8025D350;
extern s32 D_ovl4_8025D354;
extern u8 D_8018D220[];

void func_80041AA8(void);
void func_ovl4_8024F60C(void *arg0);
void func_ovl4_80250DE4(void);
void func_ovl4_80250EE0(void);
void func_ovl4_80250F04(void);
void func_ovl4_80250F90(void);
void func_ovl4_80250FB4(void);
void func_ovl4_80252014(void);
void func_ovl4_80252B24(void);

void func_ovl4_8024D800(s32 arg0) {
    D_ovl4_802542A0 = arg0;
}

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024D80C);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024DA78);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024E02C);

void func_ovl4_8024E288(void) {
    s32 i;
    s32 j;
    s32 off;

    D_800BB00C = 0;
    /* Fills D_800BAC0C[i][15..0] with -1. Only matches with an explicit
     * byte offset stepped in the inner for header; [i][j] indexing keeps a
     * running pointer instead of re-adding the base on each store. */
    for (i = 0; i < 32; i++) {
        for (j = 15, off = i * 32 + 30; j >= 0; j--, off -= 2) {
            *(s16 *)((u8 *)D_800BAC0C + off) = -1;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024E2D4);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024E514);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024E71C);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024E958);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024EB98);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024EDD8);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024F014);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024F118);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024F21C);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024F308);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024F4DC);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024F60C);

void func_ovl4_8024F790(void) {
    func_ovl4_8024F60C(D_ovl4_802550E8);
}

void func_ovl4_8024F7B0(void) {
    func_ovl4_8024F60C(D_ovl4_802550FC);
    D_ovl4_802543A8 = 0;
}

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024F7DC);

void func_ovl4_8024FA98(void) {
}

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024FAA0);

void func_ovl4_8024F4DC(void);
void func_ovl4_8024F7DC(void);
void func_ovl4_8024FAA0(void);

void func_ovl4_8024FF14(void) {
    u8 unused[0x640]; /* unused; only sizes the 0x658 stack frame to match */

    if (D_ovl4_802543A8 == 0) {
        func_ovl4_8024F4DC();
        D_ovl4_802543A8 = 1;
        func_ovl4_8024F7DC();
        func_ovl4_8024FAA0();
        func_ovl4_8024FA98();
    } else {
        func_ovl4_8024F790();
        D_ovl4_802543A8 = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8024FF74);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_80250194);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_80250474);

void func_ovl4_802504E0(void) {
}

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_802504E8);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_802509BC);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_80250C54);

void func_ovl4_80250DA0(void) {
    func_ovl4_80250DE4();
    func_ovl4_80250FB4();
    func_80041AA8();
    func_ovl4_80250F04();
    func_ovl4_80250F90();
    func_ovl4_80250EE0();
}

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_80250DE4);

void func_ovl4_80250EE0(void) {
    /* D_ovl4_8025D354 really is stored twice. */
    D_ovl4_8025D354 = 0x80315000;
    D_ovl4_8025D350 = 0x80315000;
    D_ovl4_8025D354 = 0x80315000;
}

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_80250F04);

void func_ovl4_80250F90(void) {
    bzero(D_8018D220, 0x3C00);
}

void func_ovl4_80250FB4(void) {
    s32 i;

    for (i = 0; i < 0x80; i++) {
        D_80132D4A[i].unk944 = 0;
        D_80132D4A[i].unk948 = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_80250FEC);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_802510D4);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_80251288);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_802513B8);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_80251610);

extern u8 *D_ovl4_80255330;

/* Reads two bytes per entry from the stream D_ovl4_80255330 into unk948 of
 * all 128 D_80132D4A entries, advancing the stream. */
void func_ovl4_8025176C(void) {
    s32 i;
    u8 *src;
    u8 *end;
    u8 *dst;

    for (i = 0; i < 0x80; i++) {
        src = D_ovl4_80255330;
        end = src + 2;
        dst = (u8 *)&D_80132D4A[i].unk948;
        while (src != end) {
            *dst++ = *src++;
        }
        D_ovl4_80255330 = end;
    }
}

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_802517C4);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_802518F0);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_80251A28);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_80251B6C);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_80251CB4);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_80251DFC);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_80252014);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_802527FC);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_8025283C);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_80252B24);

void func_ovl4_80253234(void) {
    func_ovl4_80252B24();
    osSyncPrintf(D_ovl4_80255314);
    func_ovl4_80252014();
    osSyncPrintf(D_ovl4_80255318);
}

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_80253270);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_80253620);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_80253680);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_802536FC);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_802537EC);

INCLUDE_ASM("asm/nonmatchings/128580", func_ovl4_802541E8);
