#include "common.h"

extern s32 D_800BE62C;
extern u8 D_800BE6D8;
extern s32 D_800BE688;
extern s32 D_800BE68C;
extern u8 D_801061CE;
extern s32 D_800DAEEC;
extern s32 D_800DAEF0;
extern s32 D_800DAEF4;
extern f32 D_800DAF30;
extern f32 D_800DAF34;
extern f32 D_800DAF38;
extern int guRandom(void);
extern void* func_80040E3C(s32 x, s32 z);
extern void func_8004CDA0(void);
extern void func_800719A0(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4,
                          s32 arg5, s32 arg6, s32 arg7, s32 arg8, s32 arg9);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_800453E0);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", calc_slope_side_pitch);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", calc_slope_uphill_pitch);

void* func_80045A9C(s32* arg0) { return func_80040E3C(arg0[4], arg0[6]); }

s32 func_80045AC0(void) { return D_800BE62C == 4; }

extern s8 D_800DAEE0[];
extern s32 D_800BE6A0;
extern s32 rand(void);

/* Shuffle-bag draw. arg0 == 0 reshuffles: fill 0..9 then 5 random swaps, return
 * -1. Otherwise draw the next entry, and reshuffle once the bag runs out.
 * func_80045AD4 is a GCC NESTED function: it homes the static chain in $v0
 * (`addiu sp,-8; sw $v0,0(sp)`) without ever reading it, and the caller sets
 * the chain with `addiu $v0,$sp,0x10` before the jal. */
s32 next_shuffled_index(s32 arg0) {
  void swap_entries(s32 i, s32 j) {
    u8 t = D_800DAEE0[j];
    s32 u = D_800DAEE0[i];

    D_800DAEE0[i] = t;
    D_800DAEE0[j] = u;
  }
  s32 i;
  s32 count;
  s32 val;

  if (arg0 != 0) {
    goto draw;
  }

  D_800BE6A0 = 0;
  i = 0;
  count = 10;
init_loop:
  D_800DAEE0[i] = i;
  i++;
  if (i != count) {
    goto init_loop;
  }

  i = 0;
  do {
    swap_entries(i, rand() % 10);
    i++;
  } while (i != 5);
  return -1;

draw:
  val = D_800DAEE0[D_800BE6A0];
  D_800BE6A0++;
  if (D_800BE6A0 == 10) {
    next_shuffled_index(0);
  }
  return val;
}

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80045C00);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80045CE0);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80046604);

s32 func_800467DC(s32 arg0) {
  if (arg0 == 0) {
    arg0 = 1;
  }
  return (guRandom() >> 2) % arg0;
}

extern s32 camera_position_y;
extern s32 D_800BE63C;
extern s32 D_800BE664;

/* func_8004683C: CARRIED at 23/23 EXACT (S260). Body below is byte-exact apart
 * from ONE register decision -- ROM {ret=v0, scratch=v1/a0}, build
 * {ret=a2, scratch=v1/v0}, which also costs ROM's `nop` in the third guard's
 * delay slot (a2 lets the build sink the D_800BE664 lui there):
 *
 *   s32 ret = 0; s32 bound;
 *   if (arg0 < 0x1E00) {
 *     bound = camera_position_y - 0x1A400;
 *     if (bound < arg1) {
 *       bound = 0x4AFFF;
 *       ret = bound >= D_800BE63C && (D_800BE664 & 2) != 0;
 *     }
 *   }
 *   return ret;
 *
 * Two S259/S260 levers already applied: the trailing `(x & 2) != 0` must be the
 * last term of an && CHAIN (a standalone store-flag takes do_store_flag's
 * pow2 path -> srl+andi, where the ROM has andi+sltu), and `bound` is REUSED
 * for the 0x4AFFF limit so the anti-dependency emits the `li` BEFORE the
 * D_800BE63C load (matching the ROM's delay-slot lui). Residual is a pure
 * global-alloc permutation: ret spans basic blocks so local-alloc takes v0 for
 * scratch first. */
INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_8004683C);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80046898);

s32 func_800469E0(void) { return D_800BE688; }

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_800469EC);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_800479C0);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80047B34);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80047CAC);

void func_80047D68(void) {
  D_800DAF30 = D_800DAEEC * (1.0f / 1024.0f);
  D_800DAF34 = D_800DAEF0 * (1.0f / 1024.0f);
  D_800DAF38 = D_800DAEF4 * (1.0f / 1024.0f);
}

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80047DBC);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80047E9C);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_800484F8);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80048690);

void func_8004876C(void) {
  D_801061CE = 0;
  func_800719A0(0, 0, -1, 1, 0, 0, 0, 0, 0, 0);
  func_800719A0(0, 0, -1, 1, 1, 0, 0, 0, 0, 0);
}

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_800487E4);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_8004887C);

/* func_80048CF8: CARRIED (near-match). Logic fully RE'd:
 *   if ((s32)(D_800DAF30 - D_800DAF24) != 0 || (s32)(D_800DAF38 - D_800DAF2C)
 * != 0) D_800BE654 = D_800CC860 - func_80059FAC();   // f64 D_800CC860, f32
 * result Wall = #pervasive-regalloc-classical-main (S158 FP class): ROM homes
 * the FP temps in $f12/$f2/$f4 + eager-schedules the 2nd sub into the 1st bnez
 * delay slot; gcc-2.7.2 allocates $f0/$f2 and reorders the load block. No
 * source lever. */
INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80048CF8);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80048D7C);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_8004AC04);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_8004B474);

void func_8004C510(s32 arg0) { D_800BE68C = arg0; }

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_8004C51C);

void func_8004C6A8(void) {}

void func_8004C6B0(void) {}

void func_8004C6B8(void) {}

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_8004C6C0);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_8004C860);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_8004C958);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_8004CDA0);

void func_8004D148(void) {
  s32 i;

  D_800BE6D8 = 1;
  for (i = 0; i < 0x40; i++) {
    func_8004CDA0();
  }
}
