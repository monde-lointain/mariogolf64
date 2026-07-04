#include "common.h"

/* Interrupt-guarded best-fit heap allocator over the per-slot free lists in
 * D_800DC6E0[] (see the Slot struct in func_8004DD70.c). Each Slot node carries
 * a 16-byte header {size, unk_04 (in-use flag / 0x12345678 guard), next, prev};
 * the payload begins at node+0x10. The head element D_800DC6E0[i] doubles as
 * the list sentinel, `total` holds the slot's free byte count, and `unk_14`
 * tracks the largest free block seen. */
typedef struct Slot {
  /* 0x00 */ s32 size;
  /* 0x04 */ s32 unk_04;
  /* 0x08 */ struct Slot* next;
  /* 0x0C */ struct Slot* prev;
  /* 0x10 */ s32 total;
  /* 0x14 */ s32 unk_14;
} Slot; /* 0x18 */

extern Slot D_800DC6E0[];
extern char D_800CCA90[];
extern char D_800CCAAC[];

s32 heap_get_largest_free(s32 i) { return D_800DC6E0[i].unk_14; }

void* heap_alloc(s32 i, u32 need) {
  u32 ra;
  Slot* head;
  u32 best_rem, minsize, bsize;
  Slot *best, *p;
  OSIntMask mask;

  __asm__ __volatile__("addu %0, $31, $0" : "=r"(ra));
  best_rem = 0;
  head = &D_800DC6E0[i];
  mask = osSetIntMask(1);
  need = (need + 0x17) & ~7;
  minsize = 0x7FFFFFFF;
  best = NULL;
  for (p = head->next; p != head; p = p->next) {
    if (p->unk_04 == 0) {
      bsize = p->size;
      if (bsize >= need) {
        if (bsize < minsize) {
          if ((best_rem < minsize) & (minsize != 0x7FFFFFFF)) {
            best_rem = minsize;
          }
          minsize = bsize;
          if (best_rem < minsize - need) {
            best_rem = minsize - need;
          }
          best = p;
        } else if (best_rem < bsize) {
          best_rem = bsize;
        }
      }
    }
  }
  D_800DC6E0[i].unk_14 = best_rem;
  if (best == NULL) {
    osSetIntMask(mask);
    osSyncPrintf(D_800CCA90, ra);
    return NULL;
  }
  if ((u32)(best->size - need) >= 0x11) {
    u32 sz;
    Slot* split;
    D_800DC6E0[i].total -= need;
    sz = best->size;
    split = (Slot*)((u8*)best + need);
    split->unk_04 = 0;
    split->size = sz - need;
    best->next->prev = split;
    split->next = best->next;
    best->next = split;
    split->prev = best;
    best->size = need;
  } else {
    D_800DC6E0[i].total -= minsize;
  }
  best->unk_04 = 0x12345678;
  osSetIntMask(mask);
  return &best->total;
}

void heap_free(s32 i, void** pptr) {
  OSIntMask mask = osSetIntMask(1);
  void* payload = *pptr;
  Slot* block;
  Slot* cur;

  if (payload == NULL) {
    osSetIntMask(mask);
    osSyncPrintf(D_800CCAAC);
    return;
  }

  block = (Slot*)((u8*)payload - 0x10);
  D_800DC6E0[i].total += block->size;
  cur = block;
  if (block->prev->unk_04 == 0 &&
      (Slot*)((u8*)block->prev + block->prev->size) == block) {
    block->next->prev = block->prev;
    block->prev->next = block->next;
    block->prev->size += block->size;
    cur = block->prev;
  }
  if (cur->next->unk_04 == 0) {
    if ((Slot*)((u8*)cur + cur->size) == cur->next) {
      cur->next->next->prev = cur;
      cur->size += cur->next->size;
      cur->next = cur->next->next;
    }
  }
  cur->unk_04 = 0;
  *pptr = NULL;
  osSetIntMask(mask);
}

/* func_8004E184 is a GCC nested function of func_8004E1E0 (in the still-asm
 * cluster [0x295E0]): the dead `sw v0, 0(sp)` in the ROM is GCC saving the
 * incoming static chain, which o32 passes in $v0 (STATIC_CHAIN_REGNUM=$2).
 * Bank it as the real nested function once func_8004E1E0's TU is decompiled;
 * carried this sprint. */
INCLUDE_ASM("asm/nonmatchings/main/func_8004DE60", func_8004E184);
