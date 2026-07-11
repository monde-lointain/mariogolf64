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

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_8005483C);

s32 func_800548DC(s32 arg0, s32 arg1) { return func_80054550(arg0, 2, arg1); }
