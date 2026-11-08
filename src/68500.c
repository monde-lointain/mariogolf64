#include "common.h"

extern s32 D_800C5EE4;
extern s32 D_800C5EFC;
extern s32 D_800C7304;
extern s32 D_800C730C;

s32 func_800292FC(s32);
s32 func_80029A30(s32);
s32 func_80069F38();
s32 func_80069FBC();
s32 memset(s32 *, s32, s32);
s16 rand();
extern s32 D_800BA9F0;
extern s16 D_800BA9F4;
extern s32 D_800BA9F8;
extern s32 D_800BA9FC;
extern s16 D_800BAA00;
extern s16 D_800BAA02;
extern s32 D_801B6088;
extern s32 D_801B608C;
extern s32 D_801B6090;
extern s32 D_801B6094;
extern s32 D_801B6098;
extern s32 D_801B609C;

void func_8008D100(void) {
    memset(&D_801B6088, 0, 0x44);
    D_801B6098 = 9;
    D_801B6094 = 0;
    D_801B609C = 0;
    D_801B608C = 0xC;
    D_800BA9F8 = 0;
    D_800BA9F0 = (rand() & 0x3F) * 0x1770;
    D_800BA9F4 = rand();
    D_800BA9FC = 6;
    D_800BAA02 = 0x1000;
    D_800BAA00 = 0x6000;
    func_80029A30(7);
    D_801B6090 = 1;
    func_80069FBC();
    func_80069F38();
    func_800292FC(2);
}

INCLUDE_ASM("asm/nonmatchings/68500", func_8008D1DC);

INCLUDE_ASM("asm/nonmatchings/68500", func_8008D3F4);

INCLUDE_ASM("asm/nonmatchings/68500", func_8008DDDC);

INCLUDE_ASM("asm/nonmatchings/68500", func_8008E164);

INCLUDE_ASM("asm/nonmatchings/68500", func_8008E1B4);

INCLUDE_ASM("asm/nonmatchings/68500", func_8008E82C);

INCLUDE_ASM("asm/nonmatchings/68500", func_8008F30C);

void func_8008FF14(s32 arg0) {
    D_800C5EFC = arg0;
}

INCLUDE_ASM("asm/nonmatchings/68500", func_8008FF20);

INCLUDE_ASM("asm/nonmatchings/68500", func_80091110);

INCLUDE_ASM("asm/nonmatchings/68500", func_8009226C);

void func_80092324(void) {
}

INCLUDE_ASM("asm/nonmatchings/68500", func_8009232C);

INCLUDE_ASM("asm/nonmatchings/68500", func_80092E10);

INCLUDE_ASM("asm/nonmatchings/68500", func_80092F18);

void func_800934CC(s32 arg0) {
    if (arg0 == -1) {
        D_800C7304 = 0;
    } else if (arg0 == 3) {
        D_800C730C = arg0;
        D_800C7304 = 2;
    } else {
        D_800C7304 = 1;
        D_800C730C = arg0;
    }
}

INCLUDE_ASM("asm/nonmatchings/68500", func_8009351C);

INCLUDE_ASM("asm/nonmatchings/68500", func_800939E8);

INCLUDE_ASM("asm/nonmatchings/68500", func_80094228);

INCLUDE_ASM("asm/nonmatchings/68500", func_800947A8);

INCLUDE_ASM("asm/nonmatchings/68500", func_80095150);

INCLUDE_ASM("asm/nonmatchings/68500", func_8009548C);

INCLUDE_ASM("asm/nonmatchings/68500", func_800957F0);

INCLUDE_ASM("asm/nonmatchings/68500", func_800958D8);

s32 func_800959F8(void) {
    return D_800C5EE4 < 1;
}
