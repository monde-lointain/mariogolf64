/*
 * Memory allocation. The game divides spare memory into a few heaps,
 * numbered from 0, each a list of blocks taken from regions of memory added
 * to it, and keeps the main memory's heap apart from them.
 */

#ifndef HEAP_H
#define HEAP_H

#include <ultra64.h>

#include "system/c_language.h"

// The memory the game gives the general pool, which ends where the screen
// effects keep their captured frames. Every build gives it the same size,
// so that the allocator chooses the same blocks and runs out at the
// same point however the code before it is laid out. The shiftable layout
// fails to link if the overlays' regions reach into it (see
// tools/shiftable_ld.py).
#define MAIN_REGION_END 0x802EA000
#define MAIN_REGION_SIZE 0x8C800
#define MAIN_REGION_START (MAIN_REGION_END - MAIN_REGION_SIZE)

// The heaps besides the main memory's, by the index that the heap routines
// take
enum HeapIndex {
    HEAP_UNUSED, // Nothing adds a region to it or allocates from it
    HEAP_BILLBOARD,
    HEAP_BALL_IMAGES,
    HEAP_COUNT
};

typedef s32 HeapIndex;

void AddHeapRegion(HeapIndex heapIndex, void *start, void *end);
void ResetHeap(HeapIndex heapIndex);
u32 GetHeapFreeSize(HeapIndex heapIndex);
u32 GetHeapLargestFreeSize(HeapIndex heapIndex);
void *AllocFromHeap(HeapIndex heapIndex, u32 size);
void FreeFromHeap(HeapIndex heapIndex, void *pointerToBlock);

void InitHeapRegions(Boolean hasExpansionPak);
u32 GetFreeMemorySize(void);
u32 FindLargestFreeMemorySize(void);
void *AllocMemory(u32 size);
void FreeMemory(void *pointerToBlock);

#endif
