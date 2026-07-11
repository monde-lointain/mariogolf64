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

/* func_80045AD4: CARRIED (near-match). Logic RE'd: byte-swap of a D_800DAEE0[]
 *   element pair (u8 t = D_800DAEE0[arg1]; D_800DAEE0[arg1] = D_800DAEE0[arg0];
 *   D_800DAEE0[arg0] = t;). Wall = #dead-frame-reload-artifact-regalloc-wall: ROM
 *   opens addiu sp,-8 + sw $v0,0(sp) (dead spill of incoming $v0, never reloaded)
 *   with no address-taken trigger; caller func_80045B14 sets no static chain, so
 *   it is a spurious dead frame, not a nested fn. No faithful-C source form emits
 *   the frame. */
INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80045AD4);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80045B14);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80045C00);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80045CE0);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80046604);

s32 func_800467DC(s32 arg0) {
  if (arg0 == 0) {
    arg0 = 1;
  }
  return (guRandom() >> 2) % arg0;
}

/* func_8004683C: CARRIED (near-match). Logic fully RE'd:
 *   if (arg0 < 0x1E00 && (camera_position_y - 0x1A400) < arg1 && D_800BE63C <=
 * 0x4AFFF) return (D_800BE664 & 2) != 0; return 0; Wall =
 * #local-alloc-qty-permutation: ROM dedicates v0=0 for the return and uses
 * v1/a0 as guard scratch; gcc-2.7.2 allocates v0 as scratch (3 source forms:
 * sequential-return / ret-var / direct-return all permute v0<->v1 identically),
 * plus the final (x&2)!=0 canonicalizes to srl+andi vs ROM andi+sltu. No source
 * lever. */
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
