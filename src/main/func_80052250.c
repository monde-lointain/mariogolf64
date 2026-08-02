#include "common.h"

extern s32 scenario_mode_id;
extern s32 D_801B6098;
extern s8 D_801B71F3;
extern s8 D_801B71F9;
extern s8 D_801B71FB;
extern s32 D_801B60A0;
extern s32 func_800521C0(void);
extern s32 func_800521DC(void);
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

s32 func_800525C4(s32 arg0, s32 arg1) {
  s32 total;
  s32 limit;
  s32 diff;
  s32 mag;
  s32 val0;
  s32 val1;
  s32 i;
  s32* flag = &D_801B60A0;
  s32* cfg;
  s32* cfg2;

  total = func_800521C0() + func_800521DC();
  diff = 0;

  if (flag[0] != 0) {
    i = func_800521C0();
    if (i >= D_801B6098) {
      return 0;
    }
    cfg = flag - 3;
    do {
      s32 base0 = arg0 * 184;
      s32 base1 = arg1 * 184;
      s32 idx0 = base0 + i * 2;
      s32 idx1 = base1 + i * 2;

      val0 =
          (&D_801B71FB)[idx0] + func_80052264(cfg[0], i, (&D_801B71F9)[base0]);
      val1 =
          (&D_801B71FB)[idx1] + func_80052264(cfg[0], i, (&D_801B71F9)[base1]);
      if (val0 < val1) {
        return 1;
      }
      if (val1 < val0) {
        return 2;
      }
      i++;
    } while (i < cfg[1]);
    return 0;
  }

  limit = total - D_801B6098 - 1;
  i = func_800521C0();
  if (i < D_801B6098) {
    cfg2 = flag - 3;
    do {
      s32 base0 = arg0 * 184;
      s32 base1 = arg1 * 184;
      s32 idx0 = base0 + i * 2;
      s32 idx1 = base1 + i * 2;

      val0 =
          (&D_801B71FB)[idx0] + func_80052264(cfg2[0], i, (&D_801B71F9)[base0]);
      val1 =
          (&D_801B71FB)[idx1] + func_80052264(cfg2[0], i, (&D_801B71F9)[base1]);
      diff += (val0 < val1);
      diff -= (val1 < val0);
      i++;
    } while (i < cfg2[1]);
  }
  mag = (diff < 0) ? -diff : diff;
  if (limit + 1 < mag) {
    if (diff > 0) {
      return 1;
    }
    return 2;
  }
  return 0;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80052250", func_80052834);

INCLUDE_ASM("asm/nonmatchings/main/func_80052250", func_80052A68);

INCLUDE_ASM("asm/nonmatchings/main/func_80052250", func_80052CF0);
