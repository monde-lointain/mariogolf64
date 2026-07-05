#include "common.h"

#include "heap.h"

/* Slot-indexed free-list maintenance, shared by every heap slot. The slot-3
 * hot path has its own specialized copies in func_8004DE60.c. */

/* Append the region [block, end) to slot `slot`'s free list as one free block,
 * linking it before the head sentinel (i.e. at the tail of the ring). */
void heap_add_region(s32 slot, HeapBlock* block, void* end) {
  s32 size = (u8*)end - (u8*)block;

  block->size = size;
  heap_slots[slot].total_free += size;
  block->state = HEAP_BLOCK_FREE;
  block->next = &heap_slots[slot];
  block->prev = heap_slots[slot].prev;
  heap_slots[slot].prev->next = block;
  heap_slots[slot].prev = block;
}

/* Reset slot `slot` to an empty list: the head sentinel points at itself, with
 * zero free bytes and no largest-free block recorded yet. */
void heap_init(s32 slot) {
  HeapBlock* head = &heap_slots[slot];

  heap_slots[slot].total_free = 0;
  heap_slots[slot].size = 0;
  heap_slots[slot].state = HEAP_BLOCK_HEAD;
  head->prev = head;
  heap_slots[slot].next = head;
  heap_slots[slot].largest_free = -1;
}

s32 heap_get_total(s32 slot) { return heap_slots[slot].total_free; }
