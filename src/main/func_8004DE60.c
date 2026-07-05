#include "common.h"

#include "heap.h"

/* Slot 3 is the game's general-purpose dynamic heap. This file holds its
 * specialized allocate / free / query path (heap3_*), plus the generic
 * slot-indexed allocate, free, and largest-free query (heap_*). The free-list
 * maintenance shared by every slot lives in func_8004DD70.c; the HeapBlock
 * layout and shared constants live in heap.h.
 *
 * heap_alloc walks a slot's free list keeping the smallest block that fits
 * (best fit). If the chosen block has enough slack it is split, and the tail
 * returned to the free list; otherwise the whole block is handed out. Freeing
 * coalesces a block with an immediately adjacent free neighbor on either side.
 */

/* Debug-console diagnostics (Shift-JIS), printed on the error paths. */
extern char heap_msg_alloc_fail[]; /* allocation failed; arg is the caller PC */
extern char heap_msg_free_null[];  /* NULL passed to heap_free */
extern char heap3_msg_free_null[]; /* NULL passed to heap3_free */

/* heap_slots[3].total_free and .largest_free, aliased as their own offset-0
 * symbols. KMC GCC 2.7.2 -O2 re-materializes %hi/%lo for a symbol+0 address but
 * folds symbol+offset (heap_slots + 0x58 / + 0x5C) into a reused base register.
 * The ROM re-materializes, so heap3_alloc and heap3_free must reach these two
 * words through the aliases; the heap3_* query/init paths, which the ROM does
 * fold, reach them through heap_slots[3] instead. Do not unify the two forms.
 */
extern s32 heap3_total_free;   /* == heap_slots[3].total_free  (0x800DC738) */
extern s32 heap3_largest_free; /* == heap_slots[3].largest_free (0x800DC73C) */

s32 heap_get_largest_free(s32 slot) { return heap_slots[slot].largest_free; }

void* heap_alloc(s32 slot, u32 need) {
  u32 caller_ra;
  HeapBlock* head;
  u32 largest_free, best_fit_size, block_size;
  HeapBlock *best_fit, *block;
  OSIntMask saved_mask;

  __asm__ __volatile__("addu %0, $31, $0" : "=r"(caller_ra));
  largest_free = 0;
  head = &heap_slots[slot];
  saved_mask = osSetIntMask(1);
  need = (need + HEAP_HEADER_SIZE + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1);
  best_fit_size = HEAP_MINSIZE_NONE;
  best_fit = NULL;
  for (block = head->next; block != head; block = block->next) {
    if (block->state == HEAP_BLOCK_FREE) {
      block_size = block->size;
      if (block_size >= need) {
        if (block_size < best_fit_size) {
          /* Bitwise `&`, not `&&`: the branchless form is what reproduces the
           * ROM's instruction schedule. */
          if ((largest_free < best_fit_size) &
              (best_fit_size != HEAP_MINSIZE_NONE)) {
            largest_free = best_fit_size;
          }
          best_fit_size = block_size;
          if (largest_free < best_fit_size - need) {
            largest_free = best_fit_size - need;
          }
          best_fit = block;
        } else if (largest_free < block_size) {
          largest_free = block_size;
        }
      }
    }
  }
  heap_slots[slot].largest_free = largest_free;
  if (best_fit == NULL) {
    osSetIntMask(saved_mask);
    osSyncPrintf(heap_msg_alloc_fail, caller_ra);
    return NULL;
  }
  /* Split only if the remainder can hold a header plus at least one byte. */
  if ((u32)(best_fit->size - need) >= HEAP_HEADER_SIZE + 1) {
    u32 whole_size;
    HeapBlock* split;
    heap_slots[slot].total_free -= need;
    whole_size = best_fit->size;
    split = (HeapBlock*)((u8*)best_fit + need);
    split->state = HEAP_BLOCK_FREE;
    split->size = whole_size - need;
    best_fit->next->prev = split;
    split->next = best_fit->next;
    best_fit->next = split;
    split->prev = best_fit;
    best_fit->size = need;
  } else {
    heap_slots[slot].total_free -= best_fit_size;
  }
  best_fit->state = HEAP_BLOCK_IN_USE;
  osSetIntMask(saved_mask);
  return &best_fit->total_free; /* the payload overlays total_free on a block */
}

void heap_free(s32 slot, void** payload_ptr) {
  OSIntMask saved_mask = osSetIntMask(1);
  void* payload = *payload_ptr;
  HeapBlock* block;
  HeapBlock* merged;

  if (payload == NULL) {
    osSetIntMask(saved_mask);
    osSyncPrintf(heap_msg_free_null);
    return;
  }

  block = (HeapBlock*)((u8*)payload - HEAP_HEADER_SIZE);
  heap_slots[slot].total_free += block->size;
  merged = block;
  /* Coalesce with the previous block if it is free and physically adjacent. */
  if (block->prev->state == HEAP_BLOCK_FREE &&
      (HeapBlock*)((u8*)block->prev + block->prev->size) == block) {
    block->next->prev = block->prev;
    block->prev->next = block->next;
    block->prev->size += block->size;
    merged = block->prev;
  }
  /* Then with the next block, under the same condition. */
  if (merged->next->state == HEAP_BLOCK_FREE) {
    if ((HeapBlock*)((u8*)merged + merged->size) == merged->next) {
      merged->next->next->prev = merged;
      merged->size += merged->next->size;
      merged->next = merged->next->next;
    }
  }
  merged->state = HEAP_BLOCK_FREE;
  *payload_ptr = NULL;
  osSetIntMask(saved_mask);
}

/* Slot 3 spans main RAM above the loaded program, plus the upper 2 MB of the
 * 8 MB Expansion Pak when it is fitted. */
#define MAIN_HEAP_START 0x8025D800
#define MAIN_HEAP_END 0x802EA000
#define EXPANSION_HEAP_START 0x80600000
#define EXPANSION_HEAP_END 0x80800000

/* heap3_init resets slot 3 to an empty list, then appends one or two RAM
 * regions via the nested helper heap3_add_region (the second, the expansion-pak
 * window, only when with_expansion_pak == 1). heap3_add_region is a GCC nested
 * function (dead static-chain `sw v0,0(sp)`), so the two are ONE C translation
 * unit; GCC emits the child body first, landing it at 0x8004E184 just before
 * the parent at 0x8004E1E0.
 *
 * The head-node reset writes size/state/prev through a volatile-qualified view
 * of heap_slots[3] (the struct itself stays non-volatile, so the slot-list
 * siblings are unaffected). That is the byte-exact lever for the ROM's `next =
 * prev` RELOAD of heap_slots[3].prev: KMC GCC 2.7.2 -O2 CSE-forwards a plain
 * absolute store, so a faithful `next = prev` reuses the stored register with
 * no reload; the volatile read forces the reload back from memory, and marking
 * size/state volatile too keeps them scheduled ahead of prev (a non-volatile
 * store would be pulled into the reload's load-delay shadow), matching the
 * schedule. */
void heap3_init(s32 with_expansion_pak) {
  void heap3_add_region(HeapBlock * block, void* end) {
    s32 size = (u8*)end - (u8*)block;
    block->size = size;
    block->state = HEAP_BLOCK_FREE;
    block->next = &heap_slots[3];
    block->prev = heap_slots[3].prev;
    heap_slots[3].total_free += size;
    heap_slots[3].prev->next = block;
    heap_slots[3].prev = block;
  }

  *(volatile s32*)&heap_slots[3].size = 0;
  *(volatile s32*)&heap_slots[3].state = HEAP_BLOCK_HEAD;
  *(HeapBlock* volatile*)&heap_slots[3].prev = &heap_slots[3];
  heap_slots[3].next = *(HeapBlock* volatile*)&heap_slots[3].prev;
  heap_slots[3].total_free = 0;
  heap3_add_region((HeapBlock*)MAIN_HEAP_START, (void*)MAIN_HEAP_END);
  if (with_expansion_pak == 1) {
    heap3_add_region((HeapBlock*)EXPANSION_HEAP_START,
                     (void*)EXPANSION_HEAP_END);
  }
  heap_slots[3].largest_free = -1;
}

s32 heap3_get_total(void) { return heap_slots[3].total_free; }

s32 heap3_get_largest_free(void) {
  HeapBlock* block = heap_slots[3].next;
  u32 largest = 0;

  for (; block != &heap_slots[3]; block = block->next) {
    if (block->state == HEAP_BLOCK_FREE) {
      if (largest < (u32)block->size) {
        largest = block->size;
      }
    }
  }
  heap_slots[3].largest_free = largest;
  return largest;
}

/* heap3_alloc is heap_alloc specialized to slot 3. The list-head sentinel is
 * written inline (`block != &heap_slots[3]`, with no `head` local): expressed
 * as an offset off heap_slots[3], CSE derives it from the `.next` load base
 * (one `addiu v1,v1,-8`), which lets the allocator keep the sentinel in $v1 for
 * the entry guard and copy it to $t0 for the loop (`move t0,v1`) exactly as the
 * ROM does, with the second HEAP_MINSIZE_NONE constant in $a3. A precomputed
 * `head` local (or a &heap_slots[3] anchor) instead pins the sentinel into one
 * loop register with no copy, one instruction short of the ROM. */
void* heap3_alloc(u32 need) {
  u32 caller_ra;
  u32 largest_free, best_fit_size, block_size;
  HeapBlock *best_fit, *block;
  OSIntMask saved_mask;

  __asm__ __volatile__("addu %0, $31, $0" : "=r"(caller_ra));
  largest_free = 0;
  saved_mask = osSetIntMask(1);
  need = (need + HEAP_HEADER_SIZE + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1);
  best_fit_size = HEAP_MINSIZE_NONE;
  best_fit = NULL;
  for (block = heap_slots[3].next; block != &heap_slots[3];
       block = block->next) {
    if (block->state == HEAP_BLOCK_FREE) {
      block_size = block->size;
      if (block_size >= need) {
        if (block_size < best_fit_size) {
          /* Bitwise `&`, not `&&` (see heap_alloc). */
          if ((largest_free < best_fit_size) &
              (best_fit_size != HEAP_MINSIZE_NONE)) {
            largest_free = best_fit_size;
          }
          best_fit_size = block_size;
          if (largest_free < best_fit_size - need) {
            largest_free = best_fit_size - need;
          }
          best_fit = block;
        } else if (largest_free < block_size) {
          largest_free = block_size;
        }
      }
    }
  }
  heap3_largest_free = largest_free;
  if (best_fit == NULL) {
    osSetIntMask(saved_mask);
    osSyncPrintf(heap_msg_alloc_fail, caller_ra);
    return NULL;
  }
  if ((u32)(best_fit->size - need) >= HEAP_HEADER_SIZE + 1) {
    u32 whole_size;
    HeapBlock* split;
    heap3_total_free -= need;
    whole_size = best_fit->size;
    split = (HeapBlock*)((u8*)best_fit + need);
    split->state = HEAP_BLOCK_FREE;
    split->size = whole_size - need;
    best_fit->next->prev = split;
    split->next = best_fit->next;
    best_fit->next = split;
    split->prev = best_fit;
    best_fit->size = need;
  } else {
    heap3_total_free -= best_fit_size;
  }
  best_fit->state = HEAP_BLOCK_IN_USE;
  osSetIntMask(saved_mask);
  return &best_fit->total_free;
}

/* heap_free specialized to slot 3, with the caller PC captured for the
 * double-free diagnostic. */
void heap3_free(void** payload_ptr) {
  u32 caller_ra;
  OSIntMask saved_mask;
  void* payload;
  HeapBlock* block;
  HeapBlock* merged;

  __asm__ __volatile__("addu %0, $31, $0" : "=r"(caller_ra));
  saved_mask = osSetIntMask(1);
  payload = *payload_ptr;
  if (payload == NULL) {
    osSetIntMask(saved_mask);
    osSyncPrintf(heap3_msg_free_null, caller_ra);
    return;
  }
  block = (HeapBlock*)((u8*)payload - HEAP_HEADER_SIZE);
  heap3_total_free += block->size;
  merged = block;
  if (block->prev->state == HEAP_BLOCK_FREE &&
      (HeapBlock*)((u8*)block->prev + block->prev->size) == block) {
    block->next->prev = block->prev;
    block->prev->next = block->next;
    block->prev->size += block->size;
    merged = block->prev;
  }
  if (merged->next->state == HEAP_BLOCK_FREE) {
    if ((HeapBlock*)((u8*)merged + merged->size) == merged->next) {
      merged->next->next->prev = merged;
      merged->size += merged->next->size;
      merged->next = merged->next->next;
    }
  }
  merged->state = HEAP_BLOCK_FREE;
  *payload_ptr = NULL;
  osSetIntMask(saved_mask);
}
