#include "animation.h"
#include "club.h"

extern s32 clear_animation_slot(s32 index);
extern s32 func_800542A0(s32 index);
extern s32 func_80054550(s32 id, s32 tag, s32 arg2);
extern u8 D_800D0488[];
extern u8 D_800CCC50[];
extern u8 D_800CCC84[];
extern s32 g_frame_counter;
extern s32 D_800C1DEC;
extern ClubShot* get_club_param(u32 id);
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

/* clear_animation_slot writes the inactive sentinel at CharacterState +0x08;
 * `polychara_state` is the symbol alias at that interior offset, not a separate
 * array. The s32 return type is load-bearing: its live result keeps the
 * `beqz` delay slot empty in the ROM. */
s32 clear_animation_slot(s32 index) {
  osSyncPrintf(D_800D0488, index);
  if ((u32)index < 4) {
    D_801F43F0[index].resource_index = -1;
  }
}

/* func_800542A0 returns 1 outside the four slots, while the resource_index
 * field at +0x08 is active, or less than two frames after last_update_frame
 * at +0x34. The single short-circuit || form emits fewer result stores than a
 * two-branch form and preserves the ROM's register allocation. */
s32 func_800542A0(s32 index) {
  s32 r;

  if ((u32)index >= 4) {
    return 1;
  }
  r = 0;
  if (D_801F43F0[index].resource_index != -1 ||
      (u32)(g_frame_counter - D_801F43F0[index].last_update_frame) < 2) {
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

CharacterState* get_character_state(s32 id) {
  if ((u32)id < 4) {
    if (D_801F43F0[id].resource_index != -1) {
      return &D_801F43F0[id];
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

/* func_800543DC: cs = get_character_state(arg0); NULL -> -1. arg1 == -1: write
 * G_ENDDL (0xDF000000)+0 into cs->model_resource->display_list[0..1], return 0. Else scan
 * cs->model_resource->animation_display_lists[0..arg1) for the -1 sentinel (return -1 on hit); then the
 * slots[arg1] slot: if -1 return -1, else write a G_DL branch (0xDE000000) into
 * dl[0] and slots[arg1] into dl[1], return 0. The `goto neg` routing is
 * load-bearing: it splits {null-check, loop-exit}(shared -1 tail) from
 * {post-loop}(inline return -1). Every natural early-return form leaves all
 * three `return -1` tails structurally identical, so cross-jump (jump.c:1969)
 * merges them into one `li v0,-1` block and reorg fills its delay slot; the ROM
 * instead reuses the post-loop compare's v0=-1 with a bare-epilogue jump + nop.
 * The goto makes the post-loop tail no longer a mergeable twin, matching. */
s32 func_800543DC(s32 arg0, s32 arg1) {
  CharacterState* cs = get_character_state(arg0);
  s32 i;

  if (cs == NULL) {
    goto neg;
  }
  i = 0;
  if (arg1 == -1) {
    cs->model_resource->display_list[0] = 0xDF000000;
    cs->model_resource->display_list[1] = 0;
    return 0;
  }
  while (i < arg1) {
    if (cs->model_resource->animation_display_lists[i] == -1) {
      goto neg;
    }
    i++;
  }
  if (cs->model_resource->animation_display_lists[i] == -1) {
    return -1;
  }
  cs->model_resource->display_list[0] = 0xDE000000;
  cs->model_resource->display_list[1] = cs->model_resource->animation_display_lists[i];
  return 0;
neg:
  return -1;
}

s32 func_800544B4(s32 arg0, s32 arg1) {
  CharacterState* cs;
  s16 kind;
  s32 result = -1;

  cs = get_character_state(arg0);
  if (cs != NULL) {
    D_800C1DEC = cs->resource_index;
    kind = get_club_param(arg1)->kind;
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

s32 func_8005483C(s32 arg0, s32 arg1, Vec3f* out) {
  f32 mtx[4][4];
  f32 pos[3];
  f32 zero;
  s32 idx;

  idx = func_80054550(arg0, arg1, (s32)pos);
  if (idx < 0) {
    zero = 0.0f;
    out->z = zero;
    out->y = zero;
    out->x = zero;
  } else {
    func_8005470C(get_character_state(arg0), mtx);
    guMtxXFMF(mtx, pos[0], pos[1], pos[2], &out->x, &out->y, &out->z);
  }
  return idx;
}

s32 func_800548DC(s32 arg0, s32 arg1) { return func_80054550(arg0, 2, arg1); }
