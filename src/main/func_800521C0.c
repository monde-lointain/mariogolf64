#include "common.h"

extern s32 flag_is_set(s32 flag);
extern s32 D_801B60AC;

s32 func_800521C0(void) { return -((D_801B60AC & 2) != 0) & 9; }

s32 func_800521DC(void) {
  if (flag_is_set(0x3D)) {
    return 2;
  }
  if (D_801B60AC & 6) {
    return 9;
  }
  return 0x12;
}

s32 func_80052220(void) { return func_800521C0() + func_800521DC() - 1; }
