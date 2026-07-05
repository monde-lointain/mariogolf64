#include "common.h"

extern s32 scenario_mode_id;
extern s32 D_801B6098;
extern s8 D_800C1435;
extern s8 D_801B71F3;
extern s8 D_801B71F9;
extern s32 func_80051FCC(void);
extern s32 func_80052264(s32 arg0, s32 arg1, s32 arg2);

s32 func_80052250(void) { return scenario_mode_id == 0xC; }

INCLUDE_ASM("asm/nonmatchings/main/func_80052250", func_80052264);

s32 func_80052324(s32 arg0) {
  return (&D_801B71F3)[arg0 * 0xB8] +
         func_80052264(scenario_mode_id, D_801B6098, (&D_801B71F9)[arg0 * 0xB8]);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80052250", func_80052384);

INCLUDE_ASM("asm/nonmatchings/main/func_80052250", func_8005244C);

INCLUDE_ASM("asm/nonmatchings/main/func_80052250", func_800525C4);

INCLUDE_ASM("asm/nonmatchings/main/func_80052250", func_80052834);

INCLUDE_ASM("asm/nonmatchings/main/func_80052250", func_80052A68);

INCLUDE_ASM("asm/nonmatchings/main/func_80052250", func_80052CF0);
