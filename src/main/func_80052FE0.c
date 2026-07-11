#include "common.h"

extern void clear_animation_slot(s32 index);
extern s32 func_800542A0(s32 index);
extern s32 func_80054550(s32 id, s32 tag, s32 arg2);
extern u8 polychara_state[];
extern u8 D_801F43F0[];
extern u8 D_801F4424[];
extern s32 D_800B67F0;
extern s32 D_800C1DEC;
extern void* get_club_param(u32 id);
extern void func_8005470C(void* cs, f32 mtx[4][4]);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_80052FE0);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_800530FC);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_8005342C);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", calculate_bone_matrices);

/* clear_animation_slot: osSyncPrintf(fmt,index) then, if ((u32)index < 4),
 * *(s32*)&polychara_state[index*0x18C] = -1 (clear the per-record sentinel at
 * D_801F43F0+8). Body fully RE'd; carried INCLUDE_ASM as a 1-nop delay-slot
 * reorg near-match. The ROM keeps a nop in the `beqz v0` delay slot; my build
 * fills it with the fall-through `sll v0,s0,1`. The sibling get_character_state
 * (identical bound+multiply shape, NO preceding call) fills the slot and
 * MATCHES; the only structural difference is the preceding osSyncPrintf jal,
 * which shifts the reorg-pass delay-slot decision
 * (#delay-slot-fill-across-call). Not source-leverable (all valid C forms of a
 * printf-then-guarded-store emit the fill); not permuter-reachable
 * (post-schedule reorg). */
INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", clear_animation_slot);

/* func_800542A0: returns 1 if (u32)index >= 4, or the per-record sentinel
 * polychara_state[index*0x18C] != -1, or (u32)(D_800B67F0 -
 * D_801F4424[index*0x18C]) < 2; else 0. Body fully RE'd; the accumulator form
 * below matches STRUCTURE + LENGTH exactly, leaving only an a0<->a1 coloring
 * swap: the ROM holds the index*0x18C offset in a0 and the 0/1 accumulator in
 * a1; my build swaps them. #local-alloc-qty-permutation (REG_ALLOC_ORDER
 * undefined -> ascending default; the higher-refcount accumulator greedily
 * grabs a0). No source lever flips it (explicit off-temp, inverted guard both
 * tried; inverting also breaks the branch layout); permuter skipped for this
 * class (0 project cracks). Carried INCLUDE_ASM.
 *
 *   s32 func_800542A0(s32 index) {
 *     s32 r;
 *     if ((u32)index >= 4) return 1;
 *     r = 0;
 *     if (*(s32*)&polychara_state[index * 0x18C] != -1) r = 1;
 *     else if ((u32)(D_800B67F0 - *(s32*)&D_801F4424[index * 0x18C]) < 2) r =
 * 1; return r;
 *   }
 */
INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_800542A0);

s32 func_80054310(void) {
  s32 i;

  for (i = 0; i < 4; i++) {
    if (func_800542A0(i)) {
      return 1;
    }
  }
  return 0;
}

u8* get_character_state(s32 id) {
  if ((u32)id < 4) {
    if (*(s32*)&polychara_state[id * 0x18C] != -1) {
      return &D_801F43F0[id * 0x18C];
    }
  }
  return NULL;
}

void func_800543A4(void) {
  s32 i;

  for (i = 0; i < 4; i++) {
    clear_animation_slot(i);
  }
}

/* func_800543DC(arg0, arg1) -> s32 (extern-typed void by its lone caller in
 * func_80054900.c, which ignores the return). cs = get_character_state(arg0);
 * NULL -> -1. arg1 == -1: write G_ENDDL (0xDF000000) + 0 into cs[0]->dl[0..1],
 * return 0. Else scan cs[0]->slots[0..arg1) for the -1 sentinel (return -1 on
 * hit); then the slots[arg1] slot: if -1 return -1, else write G_DL branch
 * (0xDE000000) into dl[0] and slots[arg1] into dl[1], return 0. Body fully
 * RE'd; the form below matches STRUCTURE, FRAME, and the loop exactly, leaving
 * one instruction: the ROM keeps the post-loop `slots[arg1]==-1` return
 * SEPARATE (reusing the compare's v0=-1, bare-epilogue jump, nop delay) while
 * my build cross-jump-merges it into the shared `li v0,-1` block (delay filled
 * with the next lui). #cross-jump-tail-merge: ROM's EBB layout links the
 * compare's -1 to the return (CSE across the extended BB) so the li is skipped;
 * my layout does not. The `==` form merges the post-loop return; inverting to
 * `!=` merges the loop-exit return into the null path instead (+8) -- neither
 * source form splits {null,loop-exit}(merged) from {post-loop}(separate) the
 * way the ROM does. Permuter-candidate (statement-reorder / EBB perturbation),
 * not a hard wall; carried INCLUDE_ASM as a stretch near-match.
 *
 *   typedef struct { u8 pad0[0xC]; u32* dl; s32* slots; } CharSub;  // dl@0xC,
 * slots@0x10 s32 func_800543DC(s32 arg0, s32 arg1) { CharSub** cs =
 * (CharSub**)get_character_state(arg0); s32 i; if (cs == NULL) return -1; i =
 * 0; if (arg1 == -1) { cs[0]->dl[0] = 0xDF000000; cs[0]->dl[1] = 0; return 0; }
 *     while (i < arg1) { if (cs[0]->slots[i] == -1) return -1; i++; }
 *     if (cs[0]->slots[i] == -1) return -1;
 *     cs[0]->dl[0] = 0xDE000000;
 *     cs[0]->dl[1] = cs[0]->slots[i];
 *     return 0;
 *   }
 */
INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_800543DC);

s32 func_800544B4(s32 arg0, s32 arg1) {
  u8* cs;
  s16 kind;
  s32 result = -1;

  cs = get_character_state(arg0);
  if (cs != NULL) {
    D_800C1DEC = *(s32*)(cs + 8);
    kind = *(s16*)((u8*)get_club_param(arg1) + 0x24);
    switch (kind) {
      case 0:
        result = 0;
        break;
      case 1:
        result = 1;
        break;
      case 2:
        result = 2;
        break;
      case 3:
        result = 3;
        break;
      default:
        result = -1;
        break;
    }
  }
  return result;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_80054550);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_8005470C);

s32 func_8005483C(s32 arg0, s32 arg1, f32* out) {
  f32 mtx[4][4];
  f32 pos[3];
  f32 zero;
  s32 idx;

  idx = func_80054550(arg0, arg1, (s32)pos);
  if (idx < 0) {
    zero = 0.0f;
    out[2] = zero;
    out[1] = zero;
    out[0] = zero;
  } else {
    func_8005470C(get_character_state(arg0), mtx);
    guMtxXFMF(mtx, pos[0], pos[1], pos[2], &out[0], &out[1], &out[2]);
  }
  return idx;
}

s32 func_800548DC(s32 arg0, s32 arg1) { return func_80054550(arg0, 2, arg1); }
