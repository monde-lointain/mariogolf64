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
extern char D_800CCAD0[];
/* D_800DC6E0[3].total (0x800DC738) and .unk_14 (0x800DC73C), referenced as
 * their own offset-0 symbols so a store / read-modify-write re-materializes
 * %hi/%lo (matching the ROM) instead of CSE-folding the D_800DC6E0+0x58 address
 * into a base register. */
extern s32 D_800DC738;
extern s32 D_800DC73C;

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

/* heap3_init resets slot 3 (D_800DC6E0[3]) to an empty self-linked list, then
 * appends one or two RAM regions via the nested helper heap3_add_region (the
 * second, the 2 MB expansion-pak window, only when param==1). heap3_add_region
 * is a GCC nested function (dead static-chain `sw v0,0(sp)`), so the two are
 * ONE C translation unit; GCC emits the child body first, landing it at
 * 0x8004E184 just before the parent at 0x8004E1E0.
 *
 * The head-node reset writes size/unk_04/prev through a volatile-qualified view
 * of D_800DC6E0[3] (the struct itself stays non-volatile, so the slot-list
 * siblings are unaffected). That is the byte-exact lever for the ROM's `next =
 * prev` RELOAD of D_800DC734: KMC GCC 2.7.2 -O2 CSE-forwards a plain absolute
 * store, so a faithful `next = prev` reuses the stored register with no reload;
 * the volatile read forces the reload back from memory, and marking size/unk_04
 * volatile too keeps them scheduled ahead of prev (a non-volatile store would
 * be pulled into the reload's load-delay shadow) so the whole schedule matches.
 */
void heap3_init(s32 param) {
  void heap3_add_region(Slot * node, void* end) {
    s32 size = (u8*)end - (u8*)node;
    node->size = size;
    node->unk_04 = 0;
    node->next = &D_800DC6E0[3];
    node->prev = D_800DC6E0[3].prev;
    D_800DC6E0[3].total += size;
    D_800DC6E0[3].prev->next = node;
    D_800DC6E0[3].prev = node;
  }

  *(volatile s32*)&D_800DC6E0[3].size = 0;
  *(volatile s32*)&D_800DC6E0[3].unk_04 = -1;
  *(Slot* volatile*)&D_800DC6E0[3].prev = &D_800DC6E0[3];
  D_800DC6E0[3].next = *(Slot* volatile*)&D_800DC6E0[3].prev;
  D_800DC6E0[3].total = 0;
  heap3_add_region((Slot*)0x8025D800, (void*)0x802EA000);
  if (param == 1) {
    heap3_add_region((Slot*)0x80600000, (void*)0x80800000);
  }
  D_800DC6E0[3].unk_14 = -1;
}

s32 heap3_get_total(void) { return D_800DC6E0[3].total; }

s32 heap3_get_largest_free(void) {
  Slot* p = D_800DC6E0[3].next;
  u32 max = 0;

  for (; p != &D_800DC6E0[3]; p = p->next) {
    if (p->unk_04 == 0) {
      if (max < (u32)p->size) {
        max = p->size;
      }
    }
  }
  D_800DC6E0[3].unk_14 = max;
  return max;
}

/* heap3_alloc is heap_alloc specialized to slot 3 (best-fit over
 * D_800DC6E0[3]'s free list, same block-split + 0x12345678 guard as heap_alloc,
 * ra-capture for the OOM osSyncPrintf). The sentinel is written inline (`p !=
 * &D_800DC6E0[3]`, no `head` local): with the sentinel expressed as an offset
 * off D_800DC6E0[3], CSE derives it from the `.next` load base (one `addiu
 * v1,v1,-8`), which lets the register allocator keep the sentinel in $v1 for
 * the entry guard and copy it to $t0 for the loop (`move t0,v1`) exactly as the
 * ROM does, with the second 0x7FFFFFFF in $a3. A precomputed `head` local (or
 * the &D_800DC730-8 anchor idiom) instead pins the sentinel into one loop reg
 * with no copy, missing that instruction. */
void* heap3_alloc(u32 need) {
  u32 ra;
  u32 best_rem, minsize, bsize;
  Slot *best, *p;
  OSIntMask mask;

  __asm__ __volatile__("addu %0, $31, $0" : "=r"(ra));
  best_rem = 0;
  mask = osSetIntMask(1);
  need = (need + 0x17) & ~7;
  minsize = 0x7FFFFFFF;
  best = NULL;
  for (p = D_800DC6E0[3].next; p != &D_800DC6E0[3]; p = p->next) {
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
  D_800DC73C = best_rem;
  if (best == NULL) {
    osSetIntMask(mask);
    osSyncPrintf(D_800CCA90, ra);
    return NULL;
  }
  if ((u32)(best->size - need) >= 0x11) {
    u32 sz;
    Slot* split;
    D_800DC738 -= need;
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
    D_800DC738 -= minsize;
  }
  best->unk_04 = 0x12345678;
  osSetIntMask(mask);
  return &best->total;
}

/* heap_free for slot 3 (the coalescing free that mirrors heap_free), with the
 * caller's return address captured for the double-free osSyncPrintf. */
void heap3_free(void** pptr) {
  u32 ra;
  OSIntMask mask;
  void* payload;
  Slot* block;
  Slot* cur;

  __asm__ __volatile__("addu %0, $31, $0" : "=r"(ra));
  mask = osSetIntMask(1);
  payload = *pptr;
  if (payload == NULL) {
    osSetIntMask(mask);
    osSyncPrintf(D_800CCAD0, ra);
    return;
  }
  block = (Slot*)((u8*)payload - 0x10);
  D_800DC738 += block->size;
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
