/*
 * Memory allocation. The game divides spare memory into a few heaps,
 * numbered from 0, each a list of blocks taken from regions of memory added
 * to it, and keeps the main memory's heap apart from them.
 */

#ifndef HEAP_H
#define HEAP_H

#include <ultra64.h>

#include "system/c_language.h"

// The memory the game gives the general pool, from the end of the overlays'
// regions. The original builds the start as a constant, which a symbol's
// address does not compile to; other builds take it from the link, which
// packs the regions.
#ifdef __IS_KMC__
#    define MAIN_REGION_START 0x8025D800
#else
extern u8 overlay_regions_VRAM_END[];
#    define MAIN_REGION_START ((u32)overlay_regions_VRAM_END)
#endif
#define MAIN_REGION_END 0x802EA000

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
s32 GetHeapFreeSize(HeapIndex heapIndex);
s32 GetHeapLargestFreeSize(HeapIndex heapIndex);
void *AllocFromHeap(HeapIndex heapIndex, u32 size);
void FreeFromHeap(HeapIndex heapIndex, void *pointerToBlock);

void InitHeapRegions(Boolean hasExpansionPak);
u32 GetFreeMemorySize(void);
u32 FindLargestFreeMemorySize(void);
void *AllocMemory(u32 size);
void FreeMemory(void *pointerToBlock);

#endif
