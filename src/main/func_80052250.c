#include "common.h"

extern s32 scenario_mode_id;
extern s32 D_801B6098;
extern s8 D_801B71F3;
extern s8 D_801B71F9;
extern s8 D_800C1435[];
extern s32 func_80051FCC(s32 mode);
extern void func_8005B150(s32* src, s32* dst, s32 count);
extern s32 func_80052264(s32 arg0, s32 arg1, s32 arg2);

s32 func_80052250(void) { return scenario_mode_id == 0xC; }

s32 func_80052264(s32 arg0, s32 arg1, s32 arg2) {
  return -arg2 / 18 -
         (arg2 % 18 >= D_800C1435[func_80051FCC(arg0) * 200 + arg1 * 10]);
}

s32 func_80052324(s32 arg0) {
  return (&D_801B71F3)[arg0 * 0xB8] + func_80052264(scenario_mode_id,
                                                    D_801B6098,
                                                    (&D_801B71F9)[arg0 * 0xB8]);
}

void func_80052384(void) {
  s32 buf[36];
  s32* bufp = buf;
  s32 count = 18;
  s32 base;
  s32 idx;
  s32 i;

  for (base = 0; base < 1600; base += 200) {
    for (i = 0; i < 18; i++) {
      idx = base + i * 10;
      bufp[i] = D_800C1435[idx];
    }
    func_8005B150(buf, &buf[18], 18);
    for (i = 0, idx = base; i < 18; i++) {
      s32 slot = bufp[18 + i] * 10 + idx;
      D_800C1435[slot] = count - i;
    }
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80052250", func_8005244C);

INCLUDE_ASM("asm/nonmatchings/main/func_80052250", func_800525C4);

INCLUDE_ASM("asm/nonmatchings/main/func_80052250", func_80052834);

INCLUDE_ASM("asm/nonmatchings/main/func_80052250", func_80052A68);

INCLUDE_ASM("asm/nonmatchings/main/func_80052250", func_80052CF0);
