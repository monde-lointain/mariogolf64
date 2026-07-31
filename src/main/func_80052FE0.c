#include "common.h"

extern s32 clear_animation_slot(s32 index);
extern s32 func_800542A0(s32 index);
extern s32 func_80054550(s32 id, s32 tag, s32 arg2);
extern u8 polychara_state[];
extern u8 D_800D0488[];
extern u8 D_800CCC50[];
extern u8 D_800CCC84[];
extern u8 D_801F43F0[];
extern u8 D_801F4424[];
extern s32 D_800B67F0;
extern s32 D_800C1DEC;
extern void* get_club_param(u32 id);
extern void func_8005470C(void* cs, f32 mtx[4][4]);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_80052FE0);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_800530FC);

/* kSetMultiTLB: maps `count` pages of size 0x1000 << page_mode from vaddr to
 * paddr, two pages per TLB entry, starting at TLB index `index`; returns the
 * next free index, or -1 on a misaligned address or a page_mode above 7 (both
 * error strings name the function). An odd page_mode halves the page size and
 * doubles the count so the entry's even/odd pair still covers the request; the
 * odd cases fall through into the even case that sets the mask, which is what
 * puts each `srl`/`sll` prologue directly above its shared arm in the table.
 * The `s32 i = index;` copy is load-bearing and must be the FIRST statement:
 * the three parameter copies are latency-1 ties in the entry block, so sched.c
 * breaks them on LUID, and the ROM emits a0's copy after a2's and a3's. Making
 * `index` a body-level copy moves it out of the assign_parms group; declaring
 * it after the `size` computation overshoots (the copy then lands after the
 * `sllv`). */
s32 kSetMultiTLB(s32 index, s32 page_mode, u32 count, u32 vaddr, u32 paddr) {
  s32 i = index;
  u32 size = 0x1000 << page_mode;
  u32 mask;

  if ((vaddr | paddr) & (size - 1)) {
    osSyncPrintf(D_800CCC50);
    return -1;
  }

  switch (page_mode) {
    case 1:
      size >>= 1;
      count <<= 1;
    case 0:
      mask = OS_PM_4K;
      break;
    case 3:
      size >>= 1;
      count <<= 1;
    case 2:
      mask = OS_PM_16K;
      break;
    case 5:
      size >>= 1;
      count <<= 1;
    case 4:
      mask = OS_PM_64K;
      break;
    case 7:
      size >>= 1;
      count <<= 1;
    case 6:
      mask = OS_PM_256K;
      break;
    default:
      osSyncPrintf(D_800CCC84);
      return -1;
  }

  if (vaddr & size) {
    osUnmapTLB(i);
    osMapTLB(i++, mask, (void*)vaddr, -1, paddr, -1);
    vaddr += size;
    paddr += size;
    count--;
  }

  while (count >= 2) {
    osUnmapTLB(i);
    osMapTLB(i++, mask, (void*)vaddr, paddr, paddr + size, -1);
    vaddr += size * 2;
    paddr += size * 2;
    count -= 2;
  }

  if (count != 0) {
    osUnmapTLB(i);
    osMapTLB(i++, mask, (void*)vaddr, paddr, -1, -1);
  }

  return i;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", calculate_bone_matrices);

/* clear_animation_slot: osSyncPrintf(fmt,index) then, if ((u32)index < 4),
 * *(s32*)&polychara_state[index*0x18C] = -1 (clear the per-record sentinel).
 * The s32 return type is load-bearing: the `beqz` delay slot's candidate fill
 * is `sll v0,s0,1`, which writes $v0. An s32 return marks $v0 live at the
 * return block, so reorg's opposite-thread liveness test (reorg.c:3374-3376)
 * rejects the fill and leaves the ROM's nop. A void return leaves $v0 dead and
 * wrongly fills the slot. (Return value is unused by every caller.) */
s32 clear_animation_slot(s32 index) {
  osSyncPrintf(D_800D0488, index);
  if ((u32)index < 4) {
    *(s32*)&polychara_state[index * 0x18C] = -1;
  }
}

/* func_800542A0: returns 1 if (u32)index >= 4, or the per-record sentinel
 * polychara_state[index*0x18C] != -1, or (u32)(D_800B67F0 -
 * D_801F4424[index*0x18C]) < 2; else 0. The single short-circuit `||` store is
 * load-bearing: a two-branch `if(c1) r=1; else if(c2) r=1;` emits two r=1
 * stores, giving the accumulator pseudo 4 refs; global.c's allocno priority
 * (global.c:587-607, floor_log2(n_refs)*n_refs/live_length) then ranks it above
 * the scaled-offset pseudo, so it grabs $a0 first (the a0<->a1 swap). The `||`
 * form drops the accumulator to 3 refs, halving floor_log2, so the offset wins
 * $a0 and matches the ROM. */
s32 func_800542A0(s32 index) {
  s32 r;

  if ((u32)index >= 4) {
    return 1;
  }
  r = 0;
  if (*(s32*)&polychara_state[index * 0x18C] != -1 ||
      (u32)(D_800B67F0 - *(s32*)&D_801F4424[index * 0x18C]) < 2) {
    r = 1;
  }
  return r;
}

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

typedef struct {
  u8 pad0[0xC];
  u32* dl;    /* 0xC: display-list slot */
  s32* slots; /* 0x10: sub-animation id array */
} CharSub;

/* func_800543DC: cs = get_character_state(arg0); NULL -> -1. arg1 == -1: write
 * G_ENDDL (0xDF000000)+0 into cs[0]->dl[0..1], return 0. Else scan
 * cs[0]->slots[0..arg1) for the -1 sentinel (return -1 on hit); then the
 * slots[arg1] slot: if -1 return -1, else write a G_DL branch (0xDE000000) into
 * dl[0] and slots[arg1] into dl[1], return 0. The `goto neg` routing is
 * load-bearing: it splits {null-check, loop-exit}(shared -1 tail) from
 * {post-loop}(inline return -1). Every natural early-return form leaves all
 * three `return -1` tails structurally identical, so cross-jump (jump.c:1969)
 * merges them into one `li v0,-1` block and reorg fills its delay slot; the ROM
 * instead reuses the post-loop compare's v0=-1 with a bare-epilogue jump + nop.
 * The goto makes the post-loop tail no longer a mergeable twin, matching. */
s32 func_800543DC(s32 arg0, s32 arg1) {
  CharSub** cs = (CharSub**)get_character_state(arg0);
  s32 i;

  if (cs == NULL) {
    goto neg;
  }
  i = 0;
  if (arg1 == -1) {
    cs[0]->dl[0] = 0xDF000000;
    cs[0]->dl[1] = 0;
    return 0;
  }
  while (i < arg1) {
    if (cs[0]->slots[i] == -1) {
      goto neg;
    }
    i++;
  }
  if (cs[0]->slots[i] == -1) {
    return -1;
  }
  cs[0]->dl[0] = 0xDE000000;
  cs[0]->dl[1] = cs[0]->slots[i];
  return 0;
neg:
  return -1;
}

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
