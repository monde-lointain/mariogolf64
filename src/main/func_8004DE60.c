#include "common.h"

#include "heap.h"

/* Interrupt-guarded best-fit allocation for arbitrary heap slots and the
 * game's dynamic heap (slot 3). Each circular block list includes allocated
 * and free blocks; releasing a block coalesces physically adjacent free nodes.
 * Shared slot initialization and region registration live in func_8004DD70.c.
 */
#define DYNAMIC_HEAP_SLOT 3

/* Debug-console diagnostics (Shift-JIS). Allocation failures report the caller
 * address; freeing a NULL payload reports an error without changing the list. */
extern char heap_msg_alloc_fail[];
extern char heap_msg_free_null[];
extern char heap3_msg_free_null[];

/* These symbols alias the dynamic slot's statistics. Offset-zero accesses
 * preserve retail address materialization in heap3_alloc/heap3_free; replacing
 * them with indexed member accesses changes KMC GCC 2.7.2's generated code. */
extern s32 heap3_total_free;
extern s32 heap3_largest_free;

/* Return the cache as-is, including HEAP_LARGEST_FREE_UNKNOWN after init. */
s32 heap_get_largest_free(s32 slot_index) {
  return heap_slots[slot_index].largest_free;
}

/* size_bytes initially counts payload bytes; round it in place to include the
 * header and alignment (required for retail register allocation). The slot
 * must be initialized. Return NULL on failure; payload contents are untouched.
 */
void* heap_alloc(s32 slot_index, u32 size_bytes) {
  u32 caller_address;
  u32 best_fit_size_bytes;
  HeapBlock* best_fit;
  OSIntMask previous_interrupt_mask;
  u32 largest_free_bytes;
  HeapBlock* slot_head;
  HeapBlock* current_block;

  /* KMC's return-address builtin reads the frame rather than the saved $ra. */
  __asm__ __volatile__("addu %0, $31, $0" : "=r"(caller_address));
  largest_free_bytes = 0;
  slot_head = &heap_slots[slot_index];
  previous_interrupt_mask = osSetIntMask(OS_IM_NONE);
  size_bytes = (size_bytes + HEAP_HEADER_SIZE + HEAP_ALIGN - 1) &
               ~(HEAP_ALIGN - 1);
  best_fit_size_bytes = HEAP_MINSIZE_NONE;
  best_fit = NULL;
  current_block = slot_head->next;
  while (current_block != slot_head) {
    if (current_block->state == HEAP_BLOCK_FREE &&
        (u32)current_block->size >= size_bytes) {
      const u32 block_size_bytes = current_block->size;

      if (block_size_bytes < best_fit_size_bytes) {
        /* Bitwise & preserves the ROM's branchless instruction schedule. */
        if ((largest_free_bytes < best_fit_size_bytes) &
            (best_fit_size_bytes != HEAP_MINSIZE_NONE)) {
          largest_free_bytes = best_fit_size_bytes;
        }
        best_fit_size_bytes = block_size_bytes;
        if (largest_free_bytes < best_fit_size_bytes - size_bytes) {
          largest_free_bytes = best_fit_size_bytes - size_bytes;
        }
        best_fit = current_block;
      } else if (largest_free_bytes < block_size_bytes) {
        largest_free_bytes = block_size_bytes;
      }
    }
    current_block = current_block->next;
  }
  heap_slots[slot_index].largest_free = largest_free_bytes;

  if (best_fit == NULL) {
    osSetIntMask(previous_interrupt_mask);
    osSyncPrintf(heap_msg_alloc_fail, caller_address);
    return NULL;
  }

  /* Split only when the tail can hold a header and at least one payload byte. */
  if ((u32)(best_fit->size - size_bytes) >= HEAP_HEADER_SIZE + 1) {
    u32 original_size_bytes;
    HeapBlock* remainder_block;

    heap_slots[slot_index].total_free -= size_bytes;
    original_size_bytes = best_fit->size;
    remainder_block = (HeapBlock*)((u8*)best_fit + size_bytes);
    remainder_block->state = HEAP_BLOCK_FREE;
    remainder_block->size = original_size_bytes - size_bytes;
    best_fit->next->prev = remainder_block;
    remainder_block->next = best_fit->next;
    best_fit->next = remainder_block;
    remainder_block->prev = best_fit;
    best_fit->size = size_bytes;
  } else {
    heap_slots[slot_index].total_free -= best_fit_size_bytes;
  }
  best_fit->state = HEAP_BLOCK_IN_USE;
  osSetIntMask(previous_interrupt_mask);
  /* Payload starts where the sentinel's statistics would otherwise live. */
  return &best_fit->total_free;
}

/* payload_address must point to a pointer returned by this slot's allocator.
 * Release that allocation and clear the caller's pointer; NULL is diagnosed.
 */
void heap_free(s32 slot_index, void** payload_address) {
  OSIntMask previous_interrupt_mask = osSetIntMask(OS_IM_NONE);
  void* allocation_payload = *payload_address;
  HeapBlock* released_block;
  HeapBlock* coalesced_block;

  if (allocation_payload == NULL) {
    osSetIntMask(previous_interrupt_mask);
    osSyncPrintf(heap_msg_free_null);
    return;
  }

  released_block = (HeapBlock*)((u8*)allocation_payload - HEAP_HEADER_SIZE);
  heap_slots[slot_index].total_free += released_block->size;
  coalesced_block = released_block;
  /* List neighbors can belong to separate RAM regions: check adjacency. */
  if (released_block->prev->state == HEAP_BLOCK_FREE &&
      (HeapBlock*)((u8*)released_block->prev + released_block->prev->size) ==
          released_block) {
    released_block->next->prev = released_block->prev;
    released_block->prev->next = released_block->next;
    released_block->prev->size += released_block->size;
    coalesced_block = released_block->prev;
  }
  if (coalesced_block->next->state == HEAP_BLOCK_FREE &&
      (HeapBlock*)((u8*)coalesced_block + coalesced_block->size) ==
          coalesced_block->next) {
    coalesced_block->next->next->prev = coalesced_block;
    coalesced_block->size += coalesced_block->next->size;
    coalesced_block->next = coalesced_block->next->next;
  }
  coalesced_block->state = HEAP_BLOCK_FREE;
  *payload_address = NULL;
  osSetIntMask(previous_interrupt_mask);
}

/* The dynamic heap uses RAM above the program and, optionally, the upper
 * 2 MB of the 8 MB Expansion Pak. Region ends are exclusive. */
#define MAIN_HEAP_START 0x8025D800
#define MAIN_HEAP_END 0x802EA000
#define EXPANSION_HEAP_START 0x80600000
#define EXPANSION_HEAP_END 0x80800000

/* Reset the dynamic heap; add Expansion Pak RAM only if has_expansion_pak == 1.
 * The nested helper preserves the retail static-chain store and function order.
 */
void heap3_init(s32 has_expansion_pak) {
  void heap3_add_region(HeapBlock * region_start, void* region_end) {
    const s32 region_size_bytes = (u8*)region_end - (u8*)region_start;

    region_start->size = region_size_bytes;
    region_start->state = HEAP_BLOCK_FREE;
    region_start->next = &heap_slots[DYNAMIC_HEAP_SLOT];
    region_start->prev = heap_slots[DYNAMIC_HEAP_SLOT].prev;
    heap_slots[DYNAMIC_HEAP_SLOT].total_free += region_size_bytes;
    heap_slots[DYNAMIC_HEAP_SLOT].prev->next = region_start;
    heap_slots[DYNAMIC_HEAP_SLOT].prev = region_start;
  }

  /* Volatile accesses retain the retail prev reload and preceding store order.
   * The next/statistic stores stay nonvolatile for their retail scheduling. */
  ((volatile HeapBlock*)&heap_slots[DYNAMIC_HEAP_SLOT])->size = 0;
  ((volatile HeapBlock*)&heap_slots[DYNAMIC_HEAP_SLOT])->state = HEAP_BLOCK_HEAD;
  ((volatile HeapBlock*)&heap_slots[DYNAMIC_HEAP_SLOT])->prev =
      &heap_slots[DYNAMIC_HEAP_SLOT];
  heap_slots[DYNAMIC_HEAP_SLOT].next =
      ((volatile HeapBlock*)&heap_slots[DYNAMIC_HEAP_SLOT])->prev;
  heap_slots[DYNAMIC_HEAP_SLOT].total_free = 0;
  heap3_add_region((HeapBlock*)MAIN_HEAP_START, (void*)MAIN_HEAP_END);
  if (has_expansion_pak == 1) {
    heap3_add_region((HeapBlock*)EXPANSION_HEAP_START,
                     (void*)EXPANSION_HEAP_END);
  }
  heap_slots[DYNAMIC_HEAP_SLOT].largest_free = HEAP_LARGEST_FREE_UNKNOWN;
}

/* Free bytes, including block headers, rather than the heap's total capacity. */
s32 heap3_get_total(void) {
  return heap_slots[DYNAMIC_HEAP_SLOT].total_free;
}

/* Recompute the dynamic heap's largest free block, including its header. */
s32 heap3_get_largest_free(void) {
  const HeapBlock* current_block = heap_slots[DYNAMIC_HEAP_SLOT].next;
  u32 largest_free_bytes = 0;

  while (current_block != &heap_slots[DYNAMIC_HEAP_SLOT]) {
    if (current_block->state == HEAP_BLOCK_FREE &&
        largest_free_bytes < (u32)current_block->size) {
      largest_free_bytes = current_block->size;
    }
    current_block = current_block->next;
  }
  heap_slots[DYNAMIC_HEAP_SLOT].largest_free = largest_free_bytes;
  return largest_free_bytes;
}

/* heap_alloc specialized for the dynamic heap. Keep the sentinel inline:
 * caching its address changes the retail loop registers and instruction count.
 */
void* heap3_alloc(u32 size_bytes) {
  u32 caller_address;
  u32 best_fit_size_bytes;
  HeapBlock* best_fit;
  OSIntMask previous_interrupt_mask;
  u32 largest_free_bytes;
  HeapBlock* current_block;

  __asm__ __volatile__("addu %0, $31, $0" : "=r"(caller_address));
  largest_free_bytes = 0;
  previous_interrupt_mask = osSetIntMask(OS_IM_NONE);
  size_bytes = (size_bytes + HEAP_HEADER_SIZE + HEAP_ALIGN - 1) &
               ~(HEAP_ALIGN - 1);
  best_fit_size_bytes = HEAP_MINSIZE_NONE;
  best_fit = NULL;
  current_block = heap_slots[DYNAMIC_HEAP_SLOT].next;
  while (current_block != &heap_slots[DYNAMIC_HEAP_SLOT]) {
    if (current_block->state == HEAP_BLOCK_FREE &&
        (u32)current_block->size >= size_bytes) {
      const u32 block_size_bytes = current_block->size;

      if (block_size_bytes < best_fit_size_bytes) {
        /* Bitwise &, as in heap_alloc, is required for matching. */
        if ((largest_free_bytes < best_fit_size_bytes) &
            (best_fit_size_bytes != HEAP_MINSIZE_NONE)) {
          largest_free_bytes = best_fit_size_bytes;
        }
        best_fit_size_bytes = block_size_bytes;
        if (largest_free_bytes < best_fit_size_bytes - size_bytes) {
          largest_free_bytes = best_fit_size_bytes - size_bytes;
        }
        best_fit = current_block;
      } else if (largest_free_bytes < block_size_bytes) {
        largest_free_bytes = block_size_bytes;
      }
    }
    current_block = current_block->next;
  }
  heap3_largest_free = largest_free_bytes;

  if (best_fit == NULL) {
    osSetIntMask(previous_interrupt_mask);
    osSyncPrintf(heap_msg_alloc_fail, caller_address);
    return NULL;
  }

  if ((u32)(best_fit->size - size_bytes) >= HEAP_HEADER_SIZE + 1) {
    u32 original_size_bytes;
    HeapBlock* remainder_block;

    heap3_total_free -= size_bytes;
    original_size_bytes = best_fit->size;
    remainder_block = (HeapBlock*)((u8*)best_fit + size_bytes);
    remainder_block->state = HEAP_BLOCK_FREE;
    remainder_block->size = original_size_bytes - size_bytes;
    best_fit->next->prev = remainder_block;
    remainder_block->next = best_fit->next;
    best_fit->next = remainder_block;
    remainder_block->prev = best_fit;
    best_fit->size = size_bytes;
  } else {
    heap3_total_free -= best_fit_size_bytes;
  }
  best_fit->state = HEAP_BLOCK_IN_USE;
  osSetIntMask(previous_interrupt_mask);
  return &best_fit->total_free;
}

/* heap_free specialized for the dynamic heap; NULL diagnostics include the
 * caller address. This does not validate wild pointers or allocation tags. */
void heap3_free(void** payload_address) {
  u32 caller_address;
  OSIntMask previous_interrupt_mask;
  void* allocation_payload;
  HeapBlock* released_block;
  HeapBlock* coalesced_block;

  __asm__ __volatile__("addu %0, $31, $0" : "=r"(caller_address));
  previous_interrupt_mask = osSetIntMask(OS_IM_NONE);
  allocation_payload = *payload_address;
  if (allocation_payload == NULL) {
    osSetIntMask(previous_interrupt_mask);
    osSyncPrintf(heap3_msg_free_null, caller_address);
    return;
  }

  released_block = (HeapBlock*)((u8*)allocation_payload - HEAP_HEADER_SIZE);
  heap3_total_free += released_block->size;
  coalesced_block = released_block;
  if (released_block->prev->state == HEAP_BLOCK_FREE &&
      (HeapBlock*)((u8*)released_block->prev + released_block->prev->size) ==
          released_block) {
    released_block->next->prev = released_block->prev;
    released_block->prev->next = released_block->next;
    released_block->prev->size += released_block->size;
    coalesced_block = released_block->prev;
  }
  if (coalesced_block->next->state == HEAP_BLOCK_FREE &&
      (HeapBlock*)((u8*)coalesced_block + coalesced_block->size) ==
          coalesced_block->next) {
    coalesced_block->next->next->prev = coalesced_block;
    coalesced_block->size += coalesced_block->next->size;
    coalesced_block->next = coalesced_block->next->next;
  }
  coalesced_block->state = HEAP_BLOCK_FREE;
  *payload_address = NULL;
  osSetIntMask(previous_interrupt_mask);
}
