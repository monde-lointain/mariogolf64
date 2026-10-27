#ifndef HEAP_H
#define HEAP_H

#include <ultra64.h>

/*
 * The game divides spare memory into a few heaps, numbered from 0, each a
 * list of blocks taken from regions of memory added to it.
 */

#define HEAP_BILLBOARD 1

void AddHeapRegion(s32 heapIndex, void* start, void* end);
void ResetHeap(s32 heapIndex);
s32 GetHeapFreeSize(s32 heapIndex);
s32 GetHeapLargestFreeSize(s32 heapIndex);
void* AllocFromHeap(s32 heapIndex, s32 size);
void FreeFromHeap(s32 heapIndex, void* pointerToBlock);

void InitHeapRegions(s32 hasExpansionPak);
u32 GetFreeMemorySize(void);
u32 FindLargestFreeMemorySize(void);
void* AllocMemory(u32 size);
void FreeMemory(void* pointerToBlock);

#endif
