#include "common.h"

extern s8 D_800BA9A8;
extern s8 D_800BA9A9;

INCLUDE_ASM("asm/nonmatchings/main/func_8003E400", func_8003E400);

INCLUDE_ASM("asm/nonmatchings/main/func_8003E400", func_8003E4B4);

INCLUDE_ASM("asm/nonmatchings/main/func_8003E400",
            build_pin_or_cup_matrix_for_dad10);

void func_8003E628(void) { D_800BA9A8 = 1; }

void func_8003E638(void) { D_800BA9A9 = 1; }

INCLUDE_ASM("asm/nonmatchings/main/func_8003E400", func_8003E648);
