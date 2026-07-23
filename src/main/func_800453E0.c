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

extern s32 D_800BE698;
extern s32 D_800BE69C;
extern s32 D_800BE660;
extern s32 D_800C5AAC;
extern f32 D_801B5530;
extern s32 D_801B7F70;
extern s32 putting_meter_level;
extern s32 D_801B557C;
extern s32 D_80132CF0;
extern s32 D_80250AEC;
extern s8 D_80250B61;
extern s32 active_player_idx;
extern char D_800CC790[];
extern char D_800CC7A0[];
extern void func_8009226C(s32 arg0);
extern void func_800510EC(s32 arg0, s32 arg1);
extern void func_80216B74(s32 arg0);
extern void func_80205AE4(s32 arg0, s32 arg1);

void func_80045C00(void) {
  D_800BE698 = 1;
  D_800BE69C = 0;
  D_800BE660 = 0;
  D_800C5AAC = 0;
  D_801B5530 = 45.0f;
  func_8009226C(-1);
  D_801B7F70 = 0;
  putting_meter_level = 0;
  D_801B557C = 0;
  func_800510EC(0xB, 0x78);
  func_800510EC(0xF, 0x78);
  func_80216B74(0);
  D_800BE62C = -1;
  if (D_80132CF0 == D_80250AEC) {
    osSyncPrintf(D_800CC790);
    if (D_80250B61 != 0) {
      osSyncPrintf(D_800CC7A0);
      func_80205AE4(active_player_idx, 0xB);
    }
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80045CE0);

/* func_80046604: CARRIED (S262 near-match, logic fully RE'd, 119 vs 118 instr,
 * one over). Club/terrain sound-effect dispatch:
 *
 *   club  = get_club_param(*(s16*)(D_801B7222 + D_80250B54*0xB8 +
 * D_80250B40*2)); entry = get_table_entry(D_800FBE70);   // club held in $s0
 * across this call, kind  = *(s16*)((u8*)club + 0x24);     // read AFTER so $s0
 * reuses to entry if (kind == 0) { if (D_80250B58 == 0 && D_80250B34 != 0) {
 * play_sound_effect(0x50,2,0x7F); func_80213D70(0); } else { id = *(s32*)entry;
 * func_80213D70(2); play_sound_effect(id == 0x16 ? 0x50 : 0x52, 2, 0x7F); }
 *     func_80216CAC();
 *   } else if ((u16)(kind - 1) < 2) {          // kind in {1,2}
 *     a = 2; if (D_80250B58==0 && D_80250B34!=0 && active_club_id<6) a = 1;
 *     func_80213D70(a);
 *     id = *(s32*)entry;                        // -> 0x53/0x54/0x56 by id
 *     switch(id){0x16:0x53; {0,1,2}:0x54; {6,7,8}:0x56; default:0x54}
 *     play_sound_effect(a, 2, 0x7F);
 *   } else { play_sound_effect(0x57,2,0x7F);
 *            if (D_800BE62C != 0x13) func_80213D70(3); }
 *
 * The head (index math, both callee scheduling, kind read, kind==0 arm) is
 * byte-exact. Residual = three branch-scheduling coins in the kind-in-{1,2} arm
 * that no source form flips together (#value-select-if-else-vs-branch-likely):
 *   1. the `&&` guard chain: ROM fills the plain `beqz/bnez` delay slots with
 *      a0=1/a0=2; every source form emits branch-likely (`beqzl/bnezl`,
 * annulled) instead.
 *   2. the terminal `id==8 ? 0x56 : 0x54`: gcc if-converts to a branchless
 *      xori/sltiu/negu/andi/ori select; the ROM keeps `bne v1,8` + two `li a0`.
 *      A goto to a shared label does NOT stop the merge (jump-opt rejoins it).
 *   3. the ROM shares one `a0=0x54` (set in the id<2 delay slot) across the
 *      id<2, id==2 and default arms via fall/branch-through; an if-chain or
 *      switch each re-materialises 0x54, and a switch lowers 6 instr SHORTER.
 * All three are the same const-select / branch-likely wall class; permuter is
 * blind to internal branch-target/annul bits (see collect_keyframe_events_at).
 */
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
 * D_800BE63C load (matching the ROM's delay-slot lui).
 *
 * Residual root-caused (S260): `ret` spans basic blocks, so it is a GLOBAL
 * allocno while every guard temp is a single-block LOCAL one. local-alloc runs
 * first, and MIPS defines no REG_ALLOC_ORDER, so find_free_reg
 * (local-alloc.c:2156) scans regno-ascending and hands $2 to the first temp.
 * global.c then masks the return copy's own preference for $2
 * (AND_COMPL_HARD_REG_SET (hard_reg_preferences[allocno], used), global.c:1037)
 * and `ret` lands on the first register above the live-in argument regs, a2.
 * Bisect: the same && chain with only TWO terms DOES keep the return value in
 * v0, so the ROM's build had fewer competing local quantities here, not a
 * different lever. Permuter: 934k iterations, base 170, best 150, no crack. */
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

extern s32 D_800BE694;
extern s32 D_801B7F70;
extern s32 putting_meter_level;
extern s32 D_801B557C;
extern s32 D_801B5534;
extern s32 D_801B5584;
extern f32 D_801B5530;
extern void func_800326FC(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
extern void func_8004887C(void);

void func_80048690(void) {
  D_800BE694 = 0;
  D_801061CE = 0;
  D_801B7F70 = 0;
  putting_meter_level = 0;
  D_801B557C = 0;
  D_801B5534 = 1;
  D_801B5584 = 1;
  D_801B5530 = 80.0f;
  func_800719A0(0, 0, -1, 1, 0, 0, 0, 0, 0, 0);
  func_800719A0(0, 0, -1, 1, 1, 0, 0, 0, 0, 0);
  func_800326FC(0xA0, 0x70, 0x80, 0x80);
  func_8004887C();
}

void func_8004876C(void) {
  D_801061CE = 0;
  func_800719A0(0, 0, -1, 1, 0, 0, 0, 0, 0, 0);
  func_800719A0(0, 0, -1, 1, 1, 0, 0, 0, 0, 0);
}

extern s32 D_801B608C;
extern s32 D_800BE694;
extern s32 D_801B7F70;
extern s32 putting_meter_level;
extern s32 D_801B557C;
extern s8 g_terrain_vtx_xform_mode;
extern s32 wind_magnitude;
extern s32 flag_is_set(s32 flag);
extern void func_80216130(s32 arg0);

void func_800487E4(void) {
  D_800BE694 = -1;
  D_801B7F70 = 0;
  putting_meter_level = 0;
  D_801B557C = 0;
  if (D_801B608C == 7) {
    return;
  }
  if (g_terrain_vtx_xform_mode != 0) {
    return;
  }
  if (!flag_is_set(0x48)) {
    if (wind_magnitude / 0x10000 < 6) {
      return;
    }
  }
  func_80216130(1);
}

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
