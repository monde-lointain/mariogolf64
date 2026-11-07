#include "common.h"

extern s32 D_800C5EE4;
extern s32 D_800C5EFC;
extern s32 D_800C7304;
extern s32 D_800C730C;

INCLUDE_ASM("asm/nonmatchings/68500", func_8008D100);

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
