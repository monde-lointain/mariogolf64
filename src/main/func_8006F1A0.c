#include "common.h"

extern s32 D_800FE4A8;
extern s32 D_800C42D4;
extern u8 D_800FF1E8[];
extern u8 D_800FF1E9[];
extern s32 D_800FF210[];
extern char D_800D1540[];

/* near-match (#base-register-vs-displacement,
 * no-source-lever/permuter-can't-flip): logic = D_800FF210[arg0*35] += arg1; if
 * (>=10000) = 9999; (score/stat accumulate+clamp). ROM re-materializes
 * %hi(D_800FF210) + index-reg + %lo-in-displacement PER access (3 stores);
 * gcc-2.7.2 CSEs the la-pair into ONE full base pointer (0(v1)). Pure
 * addressing-mode + allocno choice; array-access and s32* intermediate both
 * give the same base CSE (S210 class, permuter base 220 no crack). F228
 * (single-use address of same array) banked; F1A0's 3x same-slot access
 * triggers the base CSE. Carry. */
INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006F1A0);

s32 func_8006F1F0(void) { return D_800FE4A8; }

s32 func_8006F1FC(s32 arg0) { return D_800FF1E8[arg0 * 140] == 1; }

u8* func_8006F228(s32 arg0) { return &D_800FF1E9[arg0 * 140]; }

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
