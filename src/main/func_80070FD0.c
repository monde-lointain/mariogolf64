#include "common.h"

extern u8* func_8005AF50(void);
extern void func_8005DF54(u8*, s32);
extern void func_8007FE44(s32);

extern s8 D_801B60C5;
extern s32 D_801B608C;
extern s32 D_801B6098;
extern s32 scenario_mode_id;
extern char D_800D1570[];

typedef struct {
  u8 pad[0xF4];
  s8 tbl[6][0x12];      /* +0xF4 .. 0x160 */
  u8 wins[13][0x12][2]; /* +0x160: [row][col][counter] */
} SaveBlock;

void func_80070FD0(s32 param_1, s32 param_2, s32 param_3) {
  SaveBlock* base = (SaveBlock*)(func_8005AF50() + 0x990);
  s32 t;

  if (param_3 != 0) {
    s32 old = base->wins[param_1][param_2][1];
    u8 count = old & 0x7f;

    if (count < 99) {
      count = count + 1;
    }
    t = count | (old & 0x80);
    old = t;
    if (D_801B60C5 == 0) {
      old = t | 0x80;
    }
    base->wins[param_1][param_2][1] = old;
    osSyncPrintf(D_800D1570, old);
  } else {
    t = base->wins[param_1][param_2][0] + 1;
    base->wins[param_1][param_2][0] = t;
    if (99 < (u8)t) {
      base->wins[param_1][param_2][0] = 99;
    }
  }

  func_8005DF54(func_8005AF50(), 1);
}

void func_800710C4(void) {
  SaveBlock* base = (SaveBlock*)(func_8005AF50() + 0x990);

  if (D_801B608C == 5) {
    base->tbl[scenario_mode_id][D_801B6098] += 1;
    if (99 < base->tbl[scenario_mode_id][D_801B6098]) {
      base->tbl[scenario_mode_id][D_801B6098] = 99;
    }
    func_8005DF54(func_8005AF50(), 1);
  }
}

void func_8007117C(void) {
  SaveBlock* base = (SaveBlock*)(func_8005AF50() + 0x990);

  if (D_801B608C == 5 && base->tbl[scenario_mode_id][D_801B6098] == 0) {
    s32 count = 0;
    s32 i;
    for (i = 0; i != 6; i++) {
      s32 j;
      for (j = 0; j != 0x12; j++) {
        s32 c = base->tbl[i][j];
        count += (c != 0);
      }
    }
    func_8007FE44(count);
  }
}
