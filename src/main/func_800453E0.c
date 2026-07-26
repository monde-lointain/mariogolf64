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
extern s32 D_800DAEF8;
extern s32 D_800DAF00;
extern s32 D_80132CF0;
extern s32 D_80250AEC;

/* Terrain attribute lookup table entry (mirrors src/main/get_table_entry.c). */
typedef struct {
  /* 0x00 */ u32 id;
  /* 0x04 */ char* name_ptr;
  /* 0x08 */ u8 traction_drag;
  /* 0x09 */ u8 traction_grip;
  /* 0x0A */ u8 slope_roughness;
  /* 0x0B */ u8 pad_0b;
  /* 0x0C */ s16 bounce_a;
  /* 0x0E */ s16 bounce_b;
  /* 0x10 */ u32 param;
  /* 0x14 */ u8 flag_a;
  /* 0x15 */ u8 flag_b;
  /* 0x16 */ u8 flag_c;
  /* 0x17 */ u8 penalty_class;
  /* 0x18 */ u32 sound_id;
} TerrainAttrEntry;

/* Lie probe record: the ball's world position plus the terrain index and the
 * mirror flag the slope samplers below use. func_80045A9C reads the same
 * 0x10/0x18 pair through its s32* view. */
typedef struct {
  /* 0x00 */ u8 unk_00[0x10];
  /* 0x10 */ s32 x;
  /* 0x14 */ s32 y;
  /* 0x18 */ s32 z;
  /* 0x1C */ s8 terrain_idx;
  /* 0x1D */ u8 unk_1D[5];
  /* 0x22 */ s8 mirror;
} SlopeProbe;

extern f32 sinf(f32);
extern f32 cosf(f32);
extern f64 sin(f64);
extern f64 cos(f64);
extern f64 atan2(f64 y, f64 x);
extern TerrainAttrEntry* get_table_entry(u32 idx);
extern s32 get_interpolated_terrain_height_wrapper(s32 x, s32 z);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_800453E0);

/* Cross-slope of the lie: samples the terrain 0x3C00 units to one side of the
 * ball along `heading` rotated a quarter turn (a half turn more when the probe
 * is mirrored), clamps the sample to +/-0x3C00 around the ball's own height,
 * and returns atan2(dh, 0x3C00) as a 16-bit binary angle. A sample on terrain
 * 0x16 is pulled down 307.2 units first. Slopes past +/-0x3C00 collapse to
 * +/-0x1E00 rather than saturating at the bound.
 *
 * `s32 t` in the mirrored arm is load-bearing. Spelled inline as
 * `(f32)(u16)(ang + 0x8000)` the C front end SHORTENS the addition into HImode
 * (`(subreg:HI ang) + -32768`), and cse.c then substitutes ang's own value to
 * reach `aim + -16384`, collapsing the ROM's `ori/addu` pair into one `addiu`
 * -- one instruction short. Assigning through an s32 first keeps the add in
 * SImode with the constant positive, where cse's rtx_cost sees no win
 * (`aim + 0xC000` still needs li+addu) and leaves the ROM's form alone. */
s16 calc_slope_side_pitch(SlopeProbe* p, u16* heading) {
  s32 span = 0x3C00;
  s16 aim = *heading;
  TerrainAttrEntry* entry = get_table_entry(p->terrain_idx);
  s32 ang = aim + 0x4000;
  f32 rad;
  s32 h;
  s32 y;
  s32 lo;
  s32 hi;
  s32 d;

  if (p->mirror != 0) {
    s32 t = ang + 0x8000;

    rad = (f32)(u16)t * 0.000095873799f;
  } else {
    rad = (f32)(u16)ang * 0.000095873799f;
  }

  h = get_interpolated_terrain_height_wrapper((s32)(cos(rad) * span) + p->x,
                                              (s32)(sin(rad) * span) + p->z);
  if (entry->id == 0x16) {
    h = (s32)((f32)h - 307.2f);
  }

  y = p->y;
  lo = y - span;
  if (h < lo) {
    h = lo;
  }
  hi = y + span;
  if (hi < h) {
    h = hi;
  }

  d = h - y;
  if (p->mirror != 0) {
    d = -d;
  }

  if (d > 0x3C00) {
    d = 0x1E00;
  } else if (d < -0x3C00) {
    d = -0x1E00;
  }

  return (s16)(atan2((f64)d, (f64)span) * 10430.37806021874);
}

/* Uphill pitch of the lie: samples the terrain a half step ahead of the ball
 * along `heading` and a full step behind it (6144 units per step, so the two
 * samples are 9216 apart), clamps each sample to +/-0x1800 around the ball's
 * own height, and returns atan2(dh, 9216) as a 16-bit binary angle.
 *
 * `f32 step[2]` must be a stack ARRAY: the ROM re-loads and re-truncates both
 * offsets after the first call, which only happens when the values live in
 * memory (cse.c invalidates MEM across a call). Two plain f32 locals let cse
 * keep one trunc.w.s per axis in a callee-saved register and the body comes out
 * three instructions short. The six clamp temps must likewise be DISTINCT
 * variables: one shared bound temp is a single allocno and lands in one
 * register for all four bounds, where the ROM uses v1/v1 then a1/a0, and
 * gcc-2.7.2 has no live-range splitting to give one pseudo three registers. */
s16 calc_slope_uphill_pitch(SlopeProbe* p, u16* heading) {
  f32 step[2];
  s32 front;
  s32 back;
  s32 ground_f;
  s32 lo_f;
  s32 hi_f;
  s32 ground_b;
  s32 lo_b;
  s32 hi_b;
  f32 rad = *heading * 0.000095873799f;

  step[0] = cosf(rad) * 6144.0f;
  step[1] = sinf(rad) * 6144.0f;

  front = get_interpolated_terrain_height_wrapper((s32)step[0] / 2 + p->x,
                                                  (s32)step[1] / 2 + p->z);
  ground_f = p->y;
  lo_f = ground_f - 0x1800;
  if (front < lo_f) {
    front = lo_f;
  }
  hi_f = ground_f + 0x1800;
  if (hi_f < front) {
    front = hi_f;
  }

  back = get_interpolated_terrain_height_wrapper(p->x - (s32)step[0],
                                                 p->z - (s32)step[1]);
  ground_b = p->y;
  lo_b = ground_b - 0x1800;
  if (back < lo_b) {
    back = lo_b;
  }
  hi_b = ground_b + 0x1800;
  if (hi_b < back) {
    back = hi_b;
  }

  return (s16)(atan2((f64)(front - back), 9216.0) * 10430.37806021874);
}

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

extern s32* D_800DAF4C;
extern s32 D_800BE640;
extern s32 D_800BE644;
extern s32 D_800BE648;
extern s32 D_80250B34;
extern s32 D_80250B20;
extern s32 D_80250B58;
extern s32 get_shot_param(void);

/* Shot-strength tier: 0 when the shot is unavailable or already at full power,
 * 1 below 80% of the meter extent, 2 below 60%.
 *
 * The power test is spelled INVERTED (`if (!(a < b)) {...} else return 1;`) on
 * purpose. Written naturally (`if (a < b) return 1;`) gcc leaves
 * `bc1f Lnext / li v0,1 / j Lret`: jump.c's "conditional jump jumping over an
 * unconditional jump" (jump.c:1737) requires
 * `prev_active_insn (reallabelprev) == insn`, and the `li v0,1` sits between
 * the two jumps, so the inversion never fires (reorg.c:3821 is blocked for the
 * same reason -- its `other` must be the immediately preceding condjump).
 * Moving `return 1` into the ELSE arm makes it an own-thread block that reorg's
 * fill_slots_from_thread consumes into the branch delay slot, giving the ROM's
 * `bc1t Lret / li v0,1`. The `else` form is also what keeps the branch
 * UNannulled -- the `if (...) { ...; return 0; } return 1;` variant emits
 * `bc1tl`. The trailing `return 0` must stay the LAST block so jump.c's
 * cross_jump merges every guard's `v0=0; j Lret` tail onto it; that is what
 * lets each guard branch straight to the shared `move v0,zero` with its own
 * `v0=0` in the delay slot. The second `if` needs no such help: its then/else
 * arms are both single sets of $v0, so jump.c hoists `li v0,2` above the
 * branch by itself. */
s32 get_shot_strength_tier(void) {
  s32 state = D_800DAF4C[0];

  if (state == 0x15) {
    return 0;
  }
  if (state == 0xD) {
    return 0;
  }
  if (state == 0xA) {
    return 0;
  }
  if (D_800BE644 < D_800BE63C) {
    return 0;
  }
  if (D_800BE648 == 1) {
    return 0;
  }
  if (D_80250B34 == 0) {
    return 0;
  }
  if (D_80250B20 != 0) {
    return 0;
  }
  if (D_80250B58 == 2) {
    return 0;
  }
  if (!(((f32)get_shot_param() * 0.8f) < ((f32)D_800BE640 * 0.00007119878f))) {
    if (((f32)get_shot_param() * 0.6f) < ((f32)D_800BE640 * 0.00007119878f)) {
      return 2;
    }
  } else {
    return 1;
  }
  return 0;
}

s32 func_800469E0(void) { return D_800BE688; }

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_800469EC);

/* Camera/wind-target smoothing tick. Early-outs (does nothing) while the round
 * is within its last 10 units (D_80250AEC < D_80132CF0 + 10) AND both wind
 * targets are within +/-0x1E00 of their current values; otherwise lerps the
 * three 1/1024-scaled wind components toward their integer targets at
 * 0.2/0.5/0.2. One reused `f32 t` for the three products is load-bearing: it
 * anti-dep-serializes the three mul.s into the ROM's single reused $f14, which
 * pulls each global load into its ROM slot. */
void func_800479C0(void) {
  f32* p;
  f32 t;

  if (D_80250AEC < D_80132CF0 + 10) {
    if (((f32)D_800DAEF8 < (f32)(D_800DAEEC + 0x1E00)) &&
        ((f32)(D_800DAEEC - 0x1E00) < (f32)D_800DAEF8) &&
        ((f32)D_800DAF00 < (f32)(D_800DAEF4 + 0x1E00)) &&
        ((f32)(D_800DAEF4 - 0x1E00) < (f32)D_800DAF00)) {
      return;
    }
  }
  p = &D_800DAF30;
  t = D_800DAEEC * (1.0f / 1024.0f);
  *p += (t - *p) * 0.2f;
  t = D_800DAEF0 * (1.0f / 1024.0f);
  D_800DAF34 += (t - D_800DAF34) * 0.5f;
  t = D_800DAEF4 * (1.0f / 1024.0f);
  D_800DAF38 += (t - D_800DAF38) * 0.2f;
}

/* Twin of func_800479C0 with an f32 height-offset arg (caller passes 4608.0f):
 * the second target lerps toward (D_800DAEF0 - arg0) instead. The inner block
 * after the early return is load-bearing -- hoisting the `p`/`t` decls to the
 * top materializes &D_800DAF30 in the prologue and re-colours the body. */
void func_80047B34(f32 arg0) {
  if (D_80250AEC < D_80132CF0 + 10 &&
      (f32)D_800DAEF8 < (f32)(D_800DAEEC + 0x1E00) &&
      (f32)(D_800DAEEC - 0x1E00) < (f32)D_800DAEF8 &&
      (f32)D_800DAF00 < (f32)(D_800DAEF4 + 0x1E00) &&
      (f32)(D_800DAEF4 - 0x1E00) < (f32)D_800DAF00) {
    return;
  }
  {
    f32* p = &D_800DAF30;
    f32 t;

    t = D_800DAEEC * (1.0f / 1024.0f);
    *p += (t - *p) * 0.2f;
    t = ((f32)D_800DAEF0 - arg0) * (1.0f / 1024.0f);
    D_800DAF34 += (t - D_800DAF34) * 0.5f;
    t = D_800DAEF4 * (1.0f / 1024.0f);
    D_800DAF38 += (t - D_800DAF38) * 0.2f;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80047CAC);

void func_80047D68(void) {
  D_800DAF30 = D_800DAEEC * (1.0f / 1024.0f);
  D_800DAF34 = D_800DAEF0 * (1.0f / 1024.0f);
  D_800DAF38 = D_800DAEF4 * (1.0f / 1024.0f);
}

/* CARRY (terminal sched.c wall). Body is CONFIRMED CORRECT -- byte-exact under
 * `-fno-schedule-insns -fno-schedule-insns2`, off by exactly one gas nop under
 * the real profile:
 *   void func_80047DBC(f32* arg0) {
 *     f32* p = &D_800DAF24; f32 d;
 *     d = arg0[0] - *p;        if (fabsf(d) > 1.0f) *p += d * 0.2f;
 *     d = arg0[1] - D_800DAF28; if (fabsf(d) > 1.0f) D_800DAF28 += d * 0.2f;
 *     d = arg0[2] - D_800DAF2C; if (fabsf(d) > 1.0f) D_800DAF2C += d * 0.2f;
 *   }
 * The `f32* p` (pointer for the FIRST global, direct names for 2/3) reproduces
 * the ROM's held-$v0 base for block 0 vs inline %hi/%lo for blocks 1/2 exactly.
 * The ONLY divergence: gcc's sched.c places the 1.0f `li.s` BEFORE the `abs.s`
 * that feeds the same compare, so gas fills the mtc1->c.lt.s hazard slot with
 * abs.s and emits no nop -- the ROM keeps the nop (abs.s scheduled first).
 * Root cause (invariant): insn_cost (sched.c:1362) gives the 1.0f load
 * (movsf_internal1 alt3, type "load", mips.md:153) ready-delay 3 vs abssf2
 * (type "fabs", mips.md:226) ready-delay 2, so the const is always ready one
 * clock later and emitted one slot earlier; LUID/source-order is only a
 * same-clock tiebreak, never reached. Verified source-invariant (6 spellings,
 * -ffast-math) and cpu-invariant (9 -mcpu/-mips builds). NOT permuter-eligible
 * (1 instr short, no priority tie). The identical lerp triple recurs in
 * func_80048D7C @0x8004A144 (0.5f/0.1f) with the same order -- a crack banks
 * both. Only untried mechanism: a C shape putting the 1.0f in a different basic
 * block from the compare. See docs/wip/func_80047DBC.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80047DBC);

INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_80047E9C);

extern void func_800934CC(s32 arg0);
extern void play_bgm_by_id(s32 id, s32 flag);
extern s32 func_80052220(void);
extern s32 func_8005244C(s32 arg0, s32 arg1);
extern s8 rumble_disable_flag;
extern s8 D_801061B8;
extern s8 D_801061C0;
extern s8 D_801061C3;
extern s32 D_801B6090;
extern s32 D_801B6098;
extern f32 D_801B5580;
extern s32 D_801B608C;
extern s8 g_terrain_vtx_xform_mode;
extern s32 D_800BE694;
extern s32 D_801B7F70;
extern s32 putting_meter_level;
extern u8 D_801061CE;
extern f32 D_801B5530;

/* func_800484F8: CARRIED (S263 near-match, 99/102). "Start BGM for game state":
 * sets HUD/flag globals + 45.0f consts, then switch(D_801B608C) over a 12-entry
 * jump table (jtbl_800CC818, indices 0/1/6/8->default) picking a bgm id,
 * tail-calls play_bgm_by_id. Structure/dispatch/cases all match; residual = (1)
 * the default block's `x==D_801B6098 && D_801B6090!=1` -> gcc branch-likely
 * (bnel) where the ROM keeps plain beq+nop+j+li
 * (#value-select-if-else-vs-branch-likely, goto-PROOF, confirmed 3 spellings) +
 * (2) bgm held in a2 (move a0,a2 at the tail) vs the ROM's a0-per-case
 * (#call-result-a0-vs-v0 regalloc). 3-instr count deficit from (1); not
 * permuter-eligible (goto-proof branch-target/count residual). See
 * docs/wip/func_800484F8.near-match.md. Jtbl would also need a rodata carve. */
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
 * != 0) D_800BE654 = D_800CC860 - atan2f();   // f64 D_800CC860, f32
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

extern f32 sqrtf(f32);

/* Fills a w*h byte grid with a radial intensity ramp measured from the corner
 * (w, h): dead zone under 5, a linear ramp 10..255 over 5..54, a short 255..39
 * falloff over 55..61, and 0 beyond 62. The only caller builds the 0x40 x 0x40
 * (0x1000-byte) buffer D_800FE3D4. arg3 is passed (0x3E) but never read. The TU
 * is -ffast-math so sqrtf() lowers to a bare `sqrt.s` (see mk/main.mk). */
void build_radial_falloff_texture(u8* buf, s32 w, s32 h, s32 arg3) {
  s32 i;
  s32 j;
  s32 k;
  s32 d;
  s32 v;
  f32 s;

  k = 0;
  for (i = 0; i != h; i++) {
    for (j = 0; j != w; j++) {
      s = sqrtf((f32)((i - h) * (i - h) + (j - w) * (j - w)));
      d = (s32)s;
      if (d < 5) {
        v = 0;
      } else if (d < 55) {
        v = (s32)(s - 5.0f) * 5 + 10;
      } else if (d < 62) {
        v = 255 - (s32)(s - 55.0f) * 36;
      } else {
        v = 0;
      }
      buf[k] = v;
      k++;
    }
  }
}

extern s16 D_800BE6C0[];
extern s16* D_801B56F0;
extern s32 rand(void);
/* func_8004C958: CARRIED (S263 near-match, ~248/274). Recursive diamond-square
 * cloud/plasma midpoint-displacement (self-calls 4x into quadrants). Residual =
 * ROM spills step (sh/lhu sp+0x1E) + 3 corner values (tr/br/bl) to stack and
 * reloads with sign-extension in each of the 5 midpoint blocks, under the
 * recursion's s-reg pressure; my faithful-C build keeps them in registers (26
 * instr shorter). Not reproducible from source (mine is MORE optimal);
 * #local-alloc-qty-permutation / register-pressure spill class. See
 * docs/wip/func_8004C958.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_8004C958);

extern s32 D_800BE6D4;
extern s8 D_800BE6D9;
extern s16 D_800BE6D0;
extern s16 D_801062C0[2][64][64];
extern s16* D_801B56F0;
extern u8* sky_cloud_texture_ptr;
extern s32 flag_is_set(s32 flag);
extern void func_8004C958(s32 level, s32 x, s32 y);

/* func_8004CDA0: CARRIED (S263 near-match, 227/234, structure exact, tail byte-
 * identical). Cloud-buffer blend driver (calls func_8004C958 diamond-square).
 * Residual = #local-alloc-qty-permutation: register naming (i:t0/a3,
 * base:a1/t0)
 * + ROM extra preserving-copies (buf move a0,v0 x2; abs move a1,v0; dst move
 * v0,t0). Not permuter-eligible (7-instr count deficit, 0 local-alloc-qty
 * cracks). See docs/wip/func_8004CDA0.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/func_800453E0", func_8004CDA0);

void func_8004D148(void) {
  s32 i;

  D_800BE6D8 = 1;
  for (i = 0; i < 0x40; i++) {
    func_8004CDA0();
  }
}
