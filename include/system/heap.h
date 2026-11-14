#ifndef HEAP_H
#define HEAP_H

#include <ultra64.h>

#include "system/c_language.h"

#define HEAP_BILLBOARD 1

#ifdef __IS_KMC__
#    define MAIN_REGION_START 0x8025D800
#else
extern u8 overlay_regions_VRAM_END[];
#    define MAIN_REGION_START ((u32)overlay_regions_VRAM_END)
#endif
#define MAIN_REGION_END 0x802EA000

void AddHeapRegion(s32 heapIndex, void *start, void *end);
void ResetHeap(s32 heapIndex);
s32 GetHeapFreeSize(s32 heapIndex);
s32 GetHeapLargestFreeSize(s32 heapIndex);
void *AllocFromHeap(s32 heapIndex, u32 size);
void FreeFromHeap(s32 heapIndex, void *pointerToBlock);

void InitHeapRegions(Boolean hasExpansionPak);
u32 GetFreeMemorySize(void);
u32 FindLargestFreeMemorySize(void);
void *AllocMemory(u32 size);
void FreeMemory(void *pointerToBlock);

#endif
