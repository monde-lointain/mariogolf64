#include "common.h"

typedef struct FaultFlagLabel {
  u32 mask;
  u32 value;
  const char* label;
} FaultFlagLabel;

extern s32 D_800C2C14;
extern char D_800FF428[];
extern const char D_800D11D8[];
extern const char D_800D11E4[];
extern const char D_800D11E8[];
extern const char D_800D11EC[];
extern const char D_800D11F0[];

extern void func_8004D580(char* str, s32 x, s32 y);
extern void func_8005E360(void);

/*
 * func_8005E380: OSThread fault register/flag dump printer (game reimpl of the
 * SDK __osDumpThreadContext idea, output via the game text renderer func_8005E360
 * / func_8004D580 instead of osSyncPrintf). CARRIED S205 (CSE base-canon wall).
 *
 * FULLY REVERSE-ENGINEERED, semantically exact, 100% structural (535/535 asm rows,
 * same mnemonic stream). Body (see git note / S205 scratch near_match_5E380.c):
 *   ctx = &thread->context;  D_800C2C10 = 0;
 *   sprintf(buf, D_800D0EA4, thread->id); print; row(5);
 *   sprintf(buf, D_800D0EBC, ctx->pc);    print; row(6);
 *   func_8005EAD4(ctx->cause, D_800D0ECC, D_800C2C18);
 *   func_8005EAD4(ctx->sr,    D_800D0ED4, D_800C2D68);
 *   sprintf(buf, D_800D0ED8, ctx->badvaddr); print; row(D_800C2C14++);
 *   10 GPR lines (at/v0/v1 .. t9/gp/sp) fmt-array D_800D0EEC..102C (+0x28 stride),
 *     each 3x u64 + print + row(++);  then s8/ra (D_800D1054, 2x u64) + row(++);
 *   func_8005EAD4(ctx->fpcsr, D_800D1070, D_800C2EDC);
 *   4 single-FP lines (fp0..fp30, (f64)ctx->fpN.f.f_even) fmt D_800D1078..10F0;
 *   8 double-FP lines (fp0..fp30, ctx->fpN.d) fmt D_800D1118..11C0; each + print.
 *
 * WALL (CSE derived-pointer base-canonicalization). Target keeps ctx in a base
 * reg: `addiu $17,$4,0x20` (s1=&context), buf in s0, accesses `0xN($17)`. Every
 * build folds it the other way: `move $16,$4` (thread kept in s0), buf in s1, and
 * folds +0x20 into each displacement (`0x11C($16)`). GCC 2.7.2 CSE always
 * canonicalizes context/thread pointer arithmetic to the single base PARAMETER
 * `$4`; the `thread+0x20` intermediate never survives as the base.
 *   Ruled out (all inert): register/char*-cast/eager-temp/decl-order/ctx[-3] id
 *   read (GCC re-folds ctx[-3] back to thread+0x14) = 8 source forms; -O0/-O1/-O3/
 *   -g/-funroll/-fno-{cse-follow-jumps,gcse,expensive-opt,defer-pop,...} = 19
 *   flag/opt variants. Permuter (best-only, --main, ~45k iters) valid-floor only
 *   ~1200 permuter-score (base 27760 / 0.48 in decomp_loop); lower scores are
 *   semantically INVALID (dropped print calls, ctx used uninitialized).
 * NOT blind-retryable: needs a from-scratch permuter seeded PAST the base fold, or
 * a source form that forces GCC to materialize thread+0x20 as the base (none found).
 */
INCLUDE_ASM("asm/nonmatchings/main/func_8005E380", func_8005E380);

void func_8005EAD4(u32 flags, const char* name, FaultFlagLabel* labels) {
  u32 mask;
  s32 first;

  sprintf(D_800FF428, D_800D11D8, name, flags);
  func_8005E360();
  first = TRUE;
  sprintf(D_800FF428, D_800D11E4);
  func_8005E360();

  mask = labels->mask;
  if (mask != 0) {
    do {
      if ((flags & mask) == labels->value) {
        if (first) {
          first = FALSE;
        } else {
          sprintf(D_800FF428, D_800D11E8);
          func_8005E360();
        }

        sprintf(D_800FF428, D_800D11EC, labels->label);
        func_8005E360();
        func_8004D580(D_800FF428, 3, D_800C2C14++);
      }

      labels++;
      mask = labels->mask;
    } while (mask != 0);
  }

  sprintf(D_800FF428, D_800D11F0);
  func_8005E360();
}
