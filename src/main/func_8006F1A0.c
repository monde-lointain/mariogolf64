#include "common.h"

extern s32 D_800FE4A8;
extern s32 D_800C42D4;
extern u8 D_800FF1E8[];
extern char D_800D1540[];

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006F1A0);

s32 func_8006F1F0(void) { return D_800FE4A8; }

s32 func_8006F1FC(s32 arg0) { return D_800FF1E8[arg0 * 140] == 1; }

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006F228);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006F24C);

void func_8006F2E8(void) { D_800C42D4 = 0; }

s32 func_8006F2F4(void) { return D_800C42D4; }

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006F300);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006F404);

void func_8006F4F0(void) { nuContRmbForceStop(); }

void func_8006F50C(void) {
  nuContRmbForceStop();
  osSyncPrintf(D_800D1540);
}

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006F534);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006F5E0);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006F734);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006FE88);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_800708B4);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_80070BCC);
