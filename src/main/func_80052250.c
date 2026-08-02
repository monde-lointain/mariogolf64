#include "common.h"

extern s32 scenario_mode_id;
extern u32 D_801B608C;
extern s32 D_801B6090[];
extern s32 D_801B6098;
extern s8 D_801B71F3;
extern s8 D_801B71F6;
extern s8 D_801B71F9;
extern s8 D_801B71FB;
extern s32 D_801B60A0;
extern s32 func_8005244C(s32 arg0, s32 arg1);
extern s32 func_800521C0(void);
extern s32 func_800521DC(void);
extern s8 D_800C1435[];
extern s32 func_80051FCC(s32 mode);
extern void rank_scores_descending(s32* src, s32* dst, s32 count);
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
    rank_scores_descending(buf, &buf[18], 18);
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

s32 func_80052834(s32 arg0, s32 arg1) {
  s32 rank;
  s32 row;

  row = arg0 * 184;
  if ((&D_801B71F6)[row] == 0) {
    row = arg1 * 184;
    if ((&D_801B71F6)[row] == 0) {
      goto undecided;
    }
  } else {
    row = arg1 * 184;
    if ((&D_801B71F6)[row] != 0) {
      if (func_80052324(arg0) < func_80052324(arg1)) {
        return 1;
      }
      return (func_80052324(arg1) >= func_80052324(arg0)) ? 3 : 2;
    }
    if (func_80052324(arg0) < func_80052324(arg1) + 1) {
      return 1;
    }
  }

  row = arg1 * 184;
  if ((&D_801B71F6)[row] != 0 &&
      func_80052324(arg1) < func_80052324(arg0) + 1) {
    return 2;
  }

  rank = func_8005244C(arg0, arg1);
  if (rank == 1) {
    row = arg0 * 184;
    if ((&D_801B71F6)[row] != 0 &&
        func_80052324(arg0) <= func_80052324(arg1) + 1) {
      return 3;
    }
  }
  if (rank == 2) {
    row = arg1 * 184;
    if ((&D_801B71F6)[row] != 0 &&
        func_80052324(arg1) <= func_80052324(arg0) + 1) {
      return 3;
    }
  }
undecided:
  return 0;
}

s32 func_80052A68(s8* out) {
  s8 marks[4];
  s32 min;
  s32 best;
  s32 ties;
  s32 cnt3;
  s32 ok;
  s32 i;

  min = 0x40000000;
  best = -1;
  ok = 0;
  ties = 0;
  cnt3 = 0;

  if (out == NULL) {
    out = marks;
  }

  for (i = 0; i < D_801B6090[0]; i++) {
    s32 row = i * 184;

    if ((&D_801B71F6)[row] == 1) {
      if (func_80052324(i) < min) {
        min = func_80052324(i);
        best = i;
        ties = 0;
      } else {
        ties += (min == func_80052324(i));
      }
    } else {
      cnt3 += ((&D_801B71F6)[row] == 3);
    }
  }

  if (best == -1) {
    for (i = 0; i < D_801B6090[0]; i++) {
      out[i] = 0;
    }
    if (cnt3 == D_801B6090[0]) {
      return 2;
    }
    return ok;
  }

  ok = 1;
  for (i = 0; i < D_801B6090[0]; i++) {
    s32 row = i * 184;

    out[i] = 0;
    if ((&D_801B71F6)[row] == 0) {
      if (func_80052324(i) >= min) {
        out[i] = 2;
      } else if (func_80052324(i) < min - 1) {
        ok = 0;
      } else {
        ok = ties ? ok : 0;
      }
    } else if (min < func_80052324(i)) {
      out[i] = 2;
    } else if ((&D_801B71F6)[row] >= 2) {
      out[i] = 2;
    }
  }

  if (ok == 0) {
    return 0;
  }
  if (ties > 0) {
    return 2;
  }

  for (i = 0; i < D_801B6090[0]; i++) {
    s32 row = i * 184;

    if ((&D_801B71F6)[row] == 1 && func_80052324(i) == min) {
      out[i] = 1;
    }
  }
  return 1;
}

s32 func_80052CF0(s32 arg0) {
  s32 result;
  s32 opp;
  s32 min;
  s32 any;
  s32 i;

  result = 3;

  switch (D_801B608C) {
    case 2:
    case 4:
      opp = arg0 ^ 1;
      if (func_80052324(arg0) == func_80052324(opp) - 1 &&
          (&D_801B71F6)[opp * 184] != 0) {
        result = 4;
        if (func_8005244C(arg0, opp) == 1) {
          result = 2;
        }
        break;
      }
      if (func_80052324(arg0) < func_80052324(opp)) {
        result = (func_8005244C(arg0, opp) == 3) * 2;
        break;
      }
      if (func_80052324(arg0) >= func_80052324(opp) + 1) {
        break;
      }
      if ((&D_801B71F6)[opp * 184] == 1) {
        break;
      }
      if (func_8005244C(arg0, opp) == 1) {
        result = 2;
      }
      break;

    case 10:
    case 11:
      any = 0;
      min = 0x40000000;
      result = 0;
      for (i = 0; i < D_801B6090[0]; i++) {
        s32 row = i * 184;

        if ((&D_801B71F6)[row] == 1) {
          any = 1;
          if (func_80052324(i) < min) {
            min = func_80052324(i);
          }
        }
      }
      for (i = 0; i < D_801B6090[0]; i++) {
        s32 row = i * 184;

        if (i != arg0 && (&D_801B71F6)[row] == 0) {
          if (func_80052324(i) < func_80052324(arg0)) {
            result = 3;
            break;
          }
          if (func_80052324(i) == func_80052324(arg0)) {
            result = 1;
          }
        }
      }
      if (result == 3) {
        break;
      }
      if (any != 0) {
        if (func_80052324(arg0) + 1 == min) {
          result = 1;
        }
        if (func_80052324(arg0) + 1 >= min) {
          return result;
        }
      }
      if (result == 1) {
        result = 3;
      }
      return result;
  }
  return result;
}
