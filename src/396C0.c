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

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005E2CC);

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

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005ED8C);

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005EDD0);

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005EE94);

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

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005F11C);

s32 func_8005F180(void) {
    s32 ret = 0;

    if (func_800299F4(0x32)) {
        ret = 1;
    } else if (func_800299F4(0x9B)) {
        ret = 1;
    }
    return ret;
}

INCLUDE_ASM("asm/nonmatchings/396C0", func_8005F1C8);

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

INCLUDE_ASM("asm/nonmatchings/396C0", func_800603A8);

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
