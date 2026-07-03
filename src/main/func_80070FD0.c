#include "common.h"

extern u8 *func_8005AF50(void);
extern void func_8005DF54(u8 *, s32);
extern void func_8007FE44(s32);

extern s32 D_801B608C;
extern s32 D_801B6098;
extern s32 scenario_mode_id;

typedef struct {
  u8 pad[0xF4];
  s8 tbl[6][0x12];      /* +0xF4 .. 0x160 */
  u8 wins[13][0x12][2]; /* +0x160: [row][col][counter] */
} SaveBlock;

/* Carried (S167): byte-exact except a 3-word branch-direction triple in the
   osSyncPrintf tail. Root cause is a GCC 2.7.2 cse/regalloc wall
   (make_regs_eqv canonicalizes `old` over `t`, folding `t`); the target's
   bnez+separate-t form is unreachable from equivalent single-TU C. Near-match
   (lever-2) preserved in docs/wip/func_80070FD0.near-match.c.txt. */
INCLUDE_ASM("asm/nonmatchings/main/func_80070FD0", func_80070FD0);

void func_800710C4(void) {
  SaveBlock *base = (SaveBlock *)(func_8005AF50() + 0x990);

  if (D_801B608C == 5) {
    base->tbl[scenario_mode_id][D_801B6098] += 1;
    if (99 < base->tbl[scenario_mode_id][D_801B6098]) {
      base->tbl[scenario_mode_id][D_801B6098] = 99;
    }
    func_8005DF54(func_8005AF50(), 1);
  }
}

void func_8007117C(void) {
  SaveBlock *base = (SaveBlock *)(func_8005AF50() + 0x990);

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
