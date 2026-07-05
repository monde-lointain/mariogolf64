#ifndef HEAP_H
#define HEAP_H

#include "common.h"

/* Interrupt-guarded best-fit heap, split into independent slots.
 *
 * Each slot owns a circular doubly-linked free list of HeapBlock nodes. The
 * array element heap_slots[i] is that slot's list head: a sentinel whose `next`
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
 * index. */

typedef struct HeapBlock {
  /* 0x00 */ s32 size;  /* byte length of this block, header included */
  /* 0x04 */ s32 state; /* one of HEAP_BLOCK_HEAD / _FREE / _IN_USE */
  /* 0x08 */ struct HeapBlock* next;
  /* 0x0C */ struct HeapBlock* prev;
  /* 0x10 */ s32 total_free;   /* head only: free bytes across the slot */
  /* 0x14 */ s32 largest_free; /* head only: largest free block in the slot */
} HeapBlock;                   /* 0x18; on a non-head block the payload begins
                                * at total_free (offset HEAP_HEADER_SIZE) */

extern HeapBlock heap_slots[];

/* `state` values. IN_USE is a fixed sentinel, not a boolean: it also guards
 * against freeing a wild or already-freed pointer. */
#define HEAP_BLOCK_HEAD (-1)         /* list-head sentinel */
#define HEAP_BLOCK_FREE 0            /* a free block */
#define HEAP_BLOCK_IN_USE 0x12345678 /* an allocated block */

#define HEAP_HEADER_SIZE 0x10 /* bytes reserved before the caller payload */
#define HEAP_ALIGN 8          /* allocation sizes are rounded up to this */

/* Best-fit search sentinel: no fitting block found yet ("+infinity"). */
#define HEAP_MINSIZE_NONE 0x7FFFFFFF

#endif /* HEAP_H */
