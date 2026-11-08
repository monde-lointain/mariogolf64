#include "common.h"

extern void *D_800C2FF0;
extern void *D_800C2FF4;
extern void *D_800C3000;
extern u8 D_800C3010[];
extern s32 D_800C306C;
extern u8 D_800C30A4;
extern s32 D_800C30A8;
extern s32 D_800C30AC;
extern s32 D_800B67C4;
extern s32 D_800FBE88;
extern char D_800FF428[];
extern char D_800D1238[];
extern u8 D_80104EB8[];
extern s32 D_801B6088;

void *__MusIntMemMalloc(s32 size);
void nuPiReadRom(u32 rom_addr, void *buf_ptr, u32 size);
s32 func_800299F4(s32 arg0);
void func_8005B7BC(s32 arg0);
void func_8005EC48(void);
void func_8005EC10(void (*arg0)(void));
void func_8005EDD0(void *arg0);
void func_8005F048(s32 arg0);
void func_8005F0BC(s32 arg0);
void func_800624F0(s32 arg0, s32 arg1);
void func_800999F0(void *arg0);
void func_80099C70(void);
void func_80099CF0(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void func_80216B74(s32 arg0);
void func_80407D64(void);

void func_8005E2C0(void) {
    s32 sp0[6]; /* unused; only sizes the 0x18 stack frame to match */
}

s32 __osGetCurrFaultedThread();
s32 func_80050BF0(s32, s32, s32);
s32 func_8005E380(s32);
extern s8 D_800D0AE4;
extern s8 D_800D0AF8;
extern s32 D_800DF0B0;
extern s32 D_800DF0B4;
extern OSMesgQueue D_800E1270;

void func_8005E2CC(void) {
    void *sp10;
    s32 temp_v0;

    osSetEventMesg(0xCU, &D_800E1270, (void *)0x10);
    D_800DF0B4 = 0;
loop_1:
    osSyncPrintf(&D_800D0AE4);
    osRecvMesg(&D_800E1270, &sp10, 1);
    osSyncPrintf(&D_800D0AF8);
    temp_v0 = __osGetCurrFaultedThread();
    D_800DF0B0 = temp_v0;
    if (temp_v0 == 0) {
        goto loop_1;
    }
    func_80050BF0(0x30, 0xB, 0x7F);
    func_8005E380(D_800DF0B0);
    goto loop_1;
}

void func_8005E360(void) {
    osSyncPrintf(D_800FF428);
}

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005E380);

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005EAD4);

void func_8005EC10(void (*arg0)(void)) {
    OSIntMask mask = osSetIntMask(1);

    D_800C3000 = arg0;
    osSetIntMask(mask);
}

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005EC48);

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005ECC4);

void func_8005ED8C(u8 *arg0) {
    u8 *var_a0;

    var_a0 = arg0;
    if (var_a0 == NULL) {
        var_a0 = D_800C3010;
    }
    func_8005EDD0(var_a0);
    func_8005F048(0xC000);
    func_8005F0BC(0x4000);
    func_8005EC10(func_8005EC48);
}

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005EDD0);

s32 nuScAddClient(s32 *, OSMesgQueue *, s32);
extern void *D_800E1290;
extern void *D_800E1298;
extern s32 D_800E129C;
extern OSMesgQueue D_800FC8C0;
extern OSMesgQueue D_80132198;

void func_8005EE94(void) {
    osCreateMesgQueue(&D_80132198, &D_800E1290, 2);
    osCreateMesgQueue(&D_800FC8C0, &D_800E1298, 1);
    nuScAddClient(&D_800E129C, &D_80132198, 3);
}

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005EEF8);

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005EF88);

void func_8005F048(s32 arg0) {
    D_800C2FF0 = __MusIntMemMalloc(arg0);
}

void func_8005F06C(void) {
    func_800999F0(D_800C2FF0);
}

void func_8005F090(u32 arg0, u32 arg1) {
    /* Rounds the length up to even. */
    nuPiReadRom(arg0, D_800C2FF0, arg1 + (arg1 & 1));
}

void func_8005F0BC(s32 arg0) {
    D_800C2FF4 = __MusIntMemMalloc(arg0);
}

void func_8005F0E0(void) {
    func_80099C70();
}

void func_8005F0FC(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4) {
    func_80099CF0(arg0, arg1, arg2, arg3, arg4);
}

s32 func_8009A47C(void *);
extern s8 D_800D1220;

void func_8005F11C(u32 arg0, s32 arg1) {
    u32 temp_s0;

    temp_s0 = (arg1 & 1) + arg1;
    if (temp_s0 >= 0x4001U) {
        osSyncPrintf(&D_800D1220);
    }
    nuPiReadRom(arg0, D_800C2FF4, temp_s0);
    func_8009A47C(D_800C2FF4);
}

s32 func_8005F180(void) {
    s32 ret = 0;

    if (func_800299F4(0x32)) {
        ret = 1;
    } else if (func_800299F4(0x9B)) {
        ret = 1;
    }
    return ret;
}

extern s32 D_800C3070;
extern s32 D_800C3074;
extern s16 D_800C3078;
extern s16 D_800C307A;
extern s32 D_800C307C;
extern s32 D_800C3080;
extern s32 D_800C3084;
extern s32 D_800C3088;
extern s32 D_800C308C;
extern s32 D_800C3090;

void func_8005F1C8(void) {
    D_800C306C = 0;
    D_800C3070 = 0;
    D_800C3074 = 0;
    D_800C3078 = 0;
    D_800C3084 = 0;
    D_800C3088 = 0;
    D_800C308C = 0;
    D_800C3090 = 0;
    D_800C307A = 0;
    D_800C307C = 0;
    D_800C3080 = 0;
}

s32 func_8005F224(void) {
    s32 v = D_800FBE88;

    if (v >= 31) {
        v = 60 - v;
    }
    return v;
}

s32 func_8005F248(s32 arg0) {
    if (arg0 >= 31) {
        arg0 = 60 - arg0;
    }
    if (arg0 < 0) {
        arg0 = 0;
    }
    return (arg0 << 8) / 30;
}

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005F290);

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005F30C);

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005F360);

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005F4AC);

void func_8005F518(s32 arg0, char *arg1) {
    sprintf(arg1, D_800D1238, arg0 + 1);
}

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005F540);

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005F838);

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005F964);

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005FB58);

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005FCB8);

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005FE8C);

INCLUDE_ASM("asm/nonmatchings/396C0", func_800600C0);

INCLUDE_ASM("asm/nonmatchings/396C0", func_80060128);

INCLUDE_ASM("asm/nonmatchings/396C0", func_80060190);

INCLUDE_ASM("asm/nonmatchings/396C0", func_80060210);

INCLUDE_ASM("asm/nonmatchings/396C0", func_800602B4);

void func_8006034C(u8 arg0) {
    D_800C30A4 = arg0;
}

void func_80060358(void) {
    D_800C306C = 0;
    func_8005B7BC(0);
    func_80216B74(0);
    D_801B6088 = 7;
    D_800C30A8 = 3;
    D_800C30AC = 9;
}

s32 func_800603A8(void) {
    s32 temp_s0;
    s32 temp_s1;
    s32 temp_s2;
    s32 temp_s3;

    temp_s0 = func_800299F4(0x82);
    temp_s3 = func_800299F4(0x83);
    temp_s2 = func_800299F4(0x84);
    temp_s1 = func_800299F4(0x85);
    return (temp_s0 * 0x10) | (temp_s3 * 8) | (temp_s2 * 4) | (temp_s1 * 2) | func_800299F4(0x86);
}

INCLUDE_ASM("asm/nonmatchings/396C0", func_80060434);

INCLUDE_ASM("asm/nonmatchings/396C0", func_800604F4);

INCLUDE_ASM("asm/nonmatchings/396C0", func_800605EC);

INCLUDE_ASM("asm/nonmatchings/396C0", func_800624F0);

void func_800625C8(void) {
    if (D_800B67C4 != 0) {
        func_80407D64();
    }
    func_800624F0(0x60, 0xBE);
}

INCLUDE_ASM("asm/nonmatchings/396C0", func_800625FC);

void func_800627F8(f64 *src, f64 *dst) {
    dst[0] = src[0];
    dst[1] = src[1];
}

INCLUDE_ASM("asm/nonmatchings/396C0", func_8006280C);

void func_800628A0(void) {
    bzero(D_80104EB8, 0x80);
}

INCLUDE_ASM("asm/nonmatchings/396C0", func_800628C4);

INCLUDE_ASM("asm/nonmatchings/396C0", func_80062A24);

INCLUDE_ASM("asm/nonmatchings/396C0", func_80062A8C);
