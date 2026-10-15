#ifndef HEAP_H
#define HEAP_H

#include "common.h"

/* Interrupt-guarded best-fit heap, split into independent slots.
 *
 * Each slot owns a circular doubly-linked list of allocated and free blocks.
 * The array element heap_slots[i] is that slot's list head: a sentinel whose `next`
 * and `prev` chain the real blocks, and whose `total_free` / `largest_free`
 * fields cache slot-wide statistics (they carry no meaning on a non-head
 * block).
 *
 * A block's 16-byte header {size, state, next, prev} is followed by the caller
 * payload, so an allocation of N usable bytes reserves HEAP_HEADER_SIZE + N.
 * The head sentinel reuses the two words past the header for
 * total_free/largest_free.
 *
 * There are two families of operations: the slot-indexed ones (heap_init,
 * heap_alloc, ... take a slot index) and specialized wrappers for slot 3
 * (heap3_init, heap3_alloc, ...), which the game hot path calls without an
 * index.
 *
 * Allocation sizes count usable payload bytes. The header-inclusive aligned
 * size must fit in a positive s32; allocation does not clear the payload.
 * Freeing takes the address of the caller's pointer and clears it on success.
 * That address must be non-NULL, and a non-NULL payload must be a live allocation
 * from the same slot: these routines do not validate ownership or state tags.
 *
 * largest_free is marked unknown at initialization. Allocation updates it from
 * fitting blocks and the selected block's remainder, ignoring undersized free
 * blocks; failure can therefore cache zero while free bytes remain. Freeing
 * leaves the cache unchanged. heap_get_largest_free returns this cache as-is;
 * heap3_get_largest_free scans the dynamic heap to refresh it.
 */

typedef struct HeapBlock {
  /* 0x00 */ s32 size;  /* byte length of this block, header included */
  /* 0x04 */ s32 state; /* one of HEAP_BLOCK_HEAD / _FREE / _IN_USE */
  /* 0x08 */ struct HeapBlock* next;
  /* 0x0C */ struct HeapBlock* prev;
  /* 0x10 */ s32 total_free;   /* head only: free bytes across the slot */
  /* 0x14 */ s32 largest_free; /* head only: largest free block, or unknown */
} HeapBlock;                   /* 0x18; on a non-head block the payload begins
                                * at total_free (offset HEAP_HEADER_SIZE) */

extern HeapBlock heap_slots[];

/* `state` values. IN_USE marks an allocated block; it is not a boolean and
 * heap_free/heap3_free do not check it before releasing a block. */
#define HEAP_BLOCK_HEAD (-1)         /* list-head sentinel */
#define HEAP_BLOCK_FREE 0            /* a free block */
#define HEAP_BLOCK_IN_USE 0x12345678 /* an allocated block */

#define HEAP_HEADER_SIZE 0x10 /* bytes reserved before the caller payload */
#define HEAP_ALIGN 8          /* allocation sizes are rounded up to this */

/* Best-fit search sentinel: no fitting block found yet ("+infinity"). */
#define HEAP_MINSIZE_NONE 0x7FFFFFFF

/* The largest-free cache has not been calculated since initialization. */
#define HEAP_LARGEST_FREE_UNKNOWN (-1)

/* Slot indices must identify an existing heap_slots element. Initialize a slot
 * before registering regions; registration does not allocate the backing memory.
 * Regions must be writable, suitably aligned, non-overlapping, and large enough
 * for a block header. region_end is the exclusive bound of the same region. */
void heap_init(s32 slot_index);
void heap_add_region(s32 slot_index, HeapBlock* region_start, void* region_end);

/* Cached free bytes, including free-block headers, not total heap capacity. */
s32 heap_get_total_free(s32 slot_index);

#endif /* HEAP_H */
