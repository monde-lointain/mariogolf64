#include "common.h"

typedef struct FaultFlagLabel {
  u32 mask;
  u32 value;
  const char* label;
} FaultFlagLabel;

extern s32 D_800C2C14;
extern char thread_context_report_buffer[];
extern const char D_800D11D8[];
extern const char D_800D11E4[];
extern const char D_800D11E8[];
extern const char D_800D11EC[];
extern const char D_800D11F0[];

extern void func_8004D580(char* str, s32 x, s32 y);
extern void print_thread_context_buffer(void);

/*
 * func_8005E380: OSThread fault register/flag dump printer (game reimpl of the
 * SDK __osDumpThreadContext idea, output via the game text renderer
 * print_thread_context_buffer / func_8004D580 instead of osSyncPrintf).
 *
 * Body: ctx = &thread->context;  D_800C2C10 = 0; sprintf(buf,
 * D_800D0EA4, thread->id); print; row(5); sprintf(buf, D_800D0EBC, ctx->pc);
 * print; row(6); func_8005EAD4(ctx->cause, D_800D0ECC, D_800C2C18);
 *   func_8005EAD4(ctx->sr,    D_800D0ED4, D_800C2D68);
 *   sprintf(buf, D_800D0ED8, ctx->badvaddr); print; row(D_800C2C14++);
 *   10 GPR lines (at/v0/v1 .. t9/gp/sp) fmt-array D_800D0EEC..102C (+0x28
 *   stride), each 3x u64 + print + row(++);  then s8/ra (D_800D1054, 2x u64) +
 *   row(++); func_8005EAD4(ctx->fpcsr, D_800D1070, D_800C2EDC); 4 single-FP lines
 *   (fp0..fp30, (f64)ctx->fpN.f.f_even) fmt D_800D1078..10F0; 8 double-FP lines
 *   (fp0..fp30, ctx->fpN.d) fmt D_800D1118..11C0; each + print.
 *
 * The only known mismatch is the base register for ctx. The ROM keeps ctx in its
 * own register (`addiu $17,$4,0x20`, accesses as `0xN($17)`), while this build
 * keeps the thread pointer in s0 and folds +0x20 into each displacement
 * (`0x11C($16)`). GCC 2.7.2 CSE always canonicalizes context/thread pointer
 * arithmetic to the thread parameter ($4): cse.c:5584-5587 rewrites every access
 * through ctx into thread + (K+0x20) and eliminates the ctx register. Any faithful
 * C that computes ctx = thread + const folds this way. No -f flag or source shape
 * reaches the ROM's choice.
 *
 * Permuter results below the valid floor are semantically INVALID (they drop
 * print calls and use ctx uninitialized), so their lower scores do not count.
 */
INCLUDE_ASM("asm/nonmatchings/main/func_8005E380", func_8005E380);

void func_8005EAD4(u32 register_value, const char* register_name, FaultFlagLabel* label_entry) {
  u32 flag_mask;
  s32 is_first_match;

  sprintf(thread_context_report_buffer, D_800D11D8, register_name, register_value);
  print_thread_context_buffer();
  is_first_match = TRUE;
  sprintf(thread_context_report_buffer, D_800D11E4);
  print_thread_context_buffer();

  flag_mask = label_entry->mask;
  if (flag_mask != 0) {
    do {
      if ((register_value & flag_mask) == label_entry->value) {
        if (is_first_match) {
          is_first_match = FALSE;
        } else {
          sprintf(thread_context_report_buffer, D_800D11E8);
          print_thread_context_buffer();
        }

        sprintf(thread_context_report_buffer, D_800D11EC, label_entry->label);
        print_thread_context_buffer();
        func_8004D580(thread_context_report_buffer, 3, D_800C2C14++);
      }

      label_entry++;
      flag_mask = label_entry->mask;
    } while (flag_mask != 0);
  }

  sprintf(thread_context_report_buffer, D_800D11F0);
  print_thread_context_buffer();
}
