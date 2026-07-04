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
/* D_800DC6E0[3].total, referenced as its own offset-0 symbol so a read-modify-
 * write re-materializes %hi/%lo (matching the ROM) instead of CSE-folding the
 * D_800DC6E0+0x58 address into a base register. */
extern s32 D_800DC738;

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

/* func_8004E1E0 is the slot-3 heap initializer (resets D_800DC6E0[3] to an
 * empty self-linked list, then appends one or two RAM regions via the nested
 * helper func_8004E184; the second, the 2 MB expansion-pak window, only when
 * param==1). func_8004E184 is a GCC nested function (dead static-chain `sw
 * v0,0(sp)`, banks perfectly as `void func_8004E184(Slot* node, void* end)`
 * nested in the parent), so the two are ONE TU and are carried together.
 *
 * CARRY (both): func_8004E1E0's init emits `next = prev` as a RELOAD of
 * D_800DC734 that no faithful C reproduces under KMC GCC 2.7.2 -O2 (subagent +
 * cse.c: the reload is a CSE varying-address invalidation, mutually exclusive
 * with the ROM's pure-absolute field stores; register pressure does not trigger
 * it). Permuter is also blocked (pycparser rejects the nested fn). Do NOT bank
 * func_8004E184 alone. See #nested-function-static-chain-spill. */
INCLUDE_ASM("asm/nonmatchings/main/func_8004DE60", func_8004E184);

INCLUDE_ASM("asm/nonmatchings/main/func_8004DE60", func_8004E1E0);

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

/* func_8004E2DC is heap_alloc for slot 3 (best-fit over D_800DC6E0[3]'s free
 * list, same block-split + 0x12345678 guard as heap_alloc, ra-capture for the
 * OOM osSyncPrintf). CARRY: the clean structural source (drop the `head` local,
 * use `p != &D_800DC6E0[3]` inline) matches 104/104 instrs and every register
 * EXCEPT a 6-value caller-saved rotation driven by mask->$a0. Subagent +
 * global.c: mask is live across the whole fn (conflicts with all loop values)
 * and copy-prefers $a0 (the osSetIntMask(mask) arg), which prune_preferences
 * reserves, so mask takes $a0; the ROM needs mask->$t1 (bsize->$a0), reachable
 * only if bsize carries its own $a0 copy-pref (synthetic). The matched
 * variable-index heap_alloc escapes this because its `head` is callee-saved
 * ($s3, crosses the osSetIntMask call), leaving 5 caller-saved competitors; the
 * slot-3 constant head is a 6th. No clean trigger; permuter-escalation only
 * (register hints are -O2 no-ops). See #pervasive-regalloc-classical-main /
 * #loop-weight-and-live-length-regalloc-steering. */
INCLUDE_ASM("asm/nonmatchings/main/func_8004DE60", func_8004E2DC);

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
