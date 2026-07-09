#include "common.h"

extern s32 D_800C45D0;
extern s32 D_800C45D4;

void func_800772B0(void) {
  D_800C45D4 = 0;
  D_800C45D0 = 0;
}

INCLUDE_ASM("asm/nonmatchings/main/func_800772B0", func_800772C4);

INCLUDE_ASM("asm/nonmatchings/main/func_800772B0", func_8007775C);

INCLUDE_ASM("asm/nonmatchings/main/func_800772B0", func_800779A8);

INCLUDE_ASM("asm/nonmatchings/main/func_800772B0", func_80077AD4);

s32 func_80077BD8(s32* arg0) { return *arg0 = *arg0; }
