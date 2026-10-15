#include "common.h"

#include "heap.h"

/* Slot-indexed free-list maintenance, shared by every heap slot. The slot-3
 * hot path has its own specialized copies in func_8004DE60.c. */

/* Append [region_start, region_end) to an initialized slot as one free block.
 * Registration updates total_free but leaves the largest-free cache unchanged.
 */
void heap_add_region(s32 slot_index, HeapBlock* region_start,
                     void* region_end) {
  const s32 region_size_bytes = (u8*)region_end - (u8*)region_start;

  region_start->size = region_size_bytes;
  heap_slots[slot_index].total_free += region_size_bytes;
  region_start->state = HEAP_BLOCK_FREE;
  region_start->next = &heap_slots[slot_index];
  region_start->prev = heap_slots[slot_index].prev;
  heap_slots[slot_index].prev->next = region_start;
  heap_slots[slot_index].prev = region_start;
}

/* Empty the slot without freeing its backing memory. The circular list contains
 * only its sentinel, and the largest-free cache is marked unknown. */
void heap_init(s32 slot_index) {
  HeapBlock* slot_head = &heap_slots[slot_index];

  heap_slots[slot_index].total_free = 0;
  heap_slots[slot_index].size = 0;
  heap_slots[slot_index].state = HEAP_BLOCK_HEAD;
  /* Mixed indexed/pointer stores preserve KMC GCC 2.7.2's retail addressing. */
  slot_head->prev = slot_head;
  heap_slots[slot_index].next = slot_head;
  heap_slots[slot_index].largest_free = HEAP_LARGEST_FREE_UNKNOWN;
}

s32 heap_get_total_free(s32 slot_index) {
  return heap_slots[slot_index].total_free;
}
