#include "common.h"

#include "heap.h"

/*
 * The heaps and the general memory pool share one allocator: each keeps
 * its blocks, used and free, in a circular list in address order, around
 * a block of its own that is never free. Allocations take the smallest
 * free block that fits, and frees merge a block with free neighbours.
 */

#define HEAP_COUNT 3

/* Copies $ra, for the messages that name the caller of a failed call */
#define GET_RETURN_ADDRESS(variable) \
  __asm__ volatile("addu %0, $31, $0" : "=r"(variable))

/* A block's tag */
#define BLOCK_FREE 0
#define BLOCK_USED 0x12345678
#define BLOCK_SENTINEL -1

/* Larger than any block, so the first block that fits is the smallest so far */
#define NO_BLOCK_SIZE 0x7FFFFFFF
/* The largest free size before any allocation has measured it */
#define UNMEASURED_SIZE -1

/* The memory the game gives the general pool */
#define MAIN_REGION_START 0x8025D800
#define MAIN_REGION_END 0x802EA000
/* The Expansion Pak's memory */
#define EXPANSION_REGION_START 0x80600000
#define EXPANSION_REGION_END 0x80800000

/* The header of each block, followed by its memory */
typedef struct MemoryBlock {
  /* 0x00 */ u32 size; /* Including the header */
  /* 0x04 */ s32 tag;
  /* 0x08 */ struct MemoryBlock* next;
  /* 0x0C */ struct MemoryBlock* prev;
} MemoryBlock; /* size = 0x10 */

typedef struct Heap {
  /* 0x00 */ MemoryBlock sentinel;
  /* 0x10 */ u32 freeSize;
  /* 0x14 */ u32 largestFreeSize; /* The largest free block, measured by the
                                     last allocation */
} Heap;                           /* size = 0x18 */

extern Heap sHeaps[HEAP_COUNT];
extern Heap sMemory;

void AddHeapRegion(s32 heapIndex, void* start, void* end) {
  MemoryBlock* block = start;
  u32 size = (u8*)end - (u8*)start;

  block->size = size;
  sHeaps[heapIndex].freeSize += size;
  block->tag = BLOCK_FREE;
  block->next = &sHeaps[heapIndex].sentinel;
  block->prev = sHeaps[heapIndex].sentinel.prev;
  sHeaps[heapIndex].sentinel.prev->next = block;
  sHeaps[heapIndex].sentinel.prev = block;
}

void ResetHeap(s32 heapIndex) {
  Heap* heap = &sHeaps[heapIndex];

  sHeaps[heapIndex].freeSize = 0;
  sHeaps[heapIndex].sentinel.size = 0;
  sHeaps[heapIndex].sentinel.tag = BLOCK_SENTINEL;
  heap->sentinel.prev = &heap->sentinel;
  sHeaps[heapIndex].sentinel.next = &heap->sentinel;
  sHeaps[heapIndex].largestFreeSize = UNMEASURED_SIZE;
}

s32 GetHeapFreeSize(s32 heapIndex) { return sHeaps[heapIndex].freeSize; }

s32 GetHeapLargestFreeSize(s32 heapIndex) {
  return sHeaps[heapIndex].largestFreeSize;
}

/* "Unable to allocate memory %x" */
static const char sOutOfMemoryMessage[] =
    "\x83\x81\x83\x82\x83\x8A\x82\xAA\x8Am\x95\xDB\x82\xC5\x82\xAB\x82\xDC\x82"
    "\xB9\x82\xF1 %x\n";

void* AllocFromHeap(s32 heapIndex, s32 size) {
  u32 callerAddress;
  u32 largestFreeSize;
  Heap* heap;
  s32 mask;
  u32 bestSize;
  MemoryBlock* block;
  MemoryBlock* bestBlock;
  MemoryBlock* rest;

  GET_RETURN_ADDRESS(callerAddress);
  largestFreeSize = 0;
  heap = &sHeaps[heapIndex];
  mask = osSetIntMask(OS_IM_NONE);
  /* The size is reused for the block's, to match */
  size = (size + sizeof(MemoryBlock) + 7) & ~7;
  bestSize = NO_BLOCK_SIZE;
  block = heap->sentinel.next;
  bestBlock = NULL;

  for (; block != &heap->sentinel; block = block->next) {
    if (block->tag == BLOCK_FREE && block->size >= size) {
      if (block->size < bestSize) {
        if (largestFreeSize < bestSize && bestSize != NO_BLOCK_SIZE) {
          largestFreeSize = bestSize;
        }
        bestSize = block->size;
        if (largestFreeSize < bestSize - size) {
          largestFreeSize = bestSize - size;
        }
        bestBlock = block;
      } else if (largestFreeSize < block->size) {
        largestFreeSize = block->size;
      }
    }
  }
  sHeaps[heapIndex].largestFreeSize = largestFreeSize;
  if (bestBlock == NULL) {
    osSetIntMask(mask);
    osSyncPrintf(sOutOfMemoryMessage, callerAddress);
    return NULL;
  }
  if (bestBlock->size - size > sizeof(MemoryBlock)) {
    sHeaps[heapIndex].freeSize -= size;
    rest = (MemoryBlock*)((u8*)bestBlock + size);
    rest->size = bestBlock->size - size;
    rest->tag = BLOCK_FREE;
    bestBlock->next->prev = rest;
    rest->next = bestBlock->next;
    bestBlock->next = rest;
    rest->prev = bestBlock;
    bestBlock->size = size;
  } else {
    sHeaps[heapIndex].freeSize -= bestSize;
  }
  bestBlock->tag = BLOCK_USED;
  osSetIntMask(mask);
  return bestBlock + 1;
}

/* Frees the block that *pointerToBlock points to and clears the pointer */
void FreeFromHeap(s32 heapIndex, void* pointerToBlock) {
  void* pointer;
  s32 mask;
  MemoryBlock* block;

  mask = osSetIntMask(OS_IM_NONE);
  pointer = *(void**)pointerToBlock;

  if (pointer == NULL) {
    osSetIntMask(mask);
    /* "A NULL pointer was passed to kFree()" */
    osSyncPrintf(
        "NULL\x83|\x83"
        "C\x83\x93\x83^\x82\xAAkFree()"
        "\x82\xC9\x93n\x82\xB3\x82\xEA\x82\xDC\x82\xB5\x82\xBD");
    return;
  }
  block = (MemoryBlock*)pointer - 1;
  sHeaps[heapIndex].freeSize += block->size;
  if (block->prev->tag == BLOCK_FREE &&
      (MemoryBlock*)((u8*)block->prev + block->prev->size) == block) {
    block->next->prev = block->prev;
    block->prev->next = block->next;
    block->prev->size += block->size;
    block = block->prev;
  }
  if (block->next->tag == BLOCK_FREE &&
      (MemoryBlock*)((u8*)block + block->size) == block->next) {
    block->next->next->prev = block;
    block->size += block->next->size;
    block->next = block->next->next;
  }
  block->tag = BLOCK_FREE;
  *(void**)pointerToBlock = NULL;
  osSetIntMask(mask);
}

/*
 * AddMemoryRegion is nested, a GNU C extension, as in the original: the
 * calls pass a static chain that it saves but never uses.
 */
#ifdef NON_MATCHING
void InitHeapRegions(s32 hasExpansionPak) {
  void AddMemoryRegion(void* start, void* end) {
    MemoryBlock* block = start;
    u32 size = (u8*)end - (u8*)start;

    block->size = size;
    block->tag = BLOCK_FREE;
    block->next = &sMemory.sentinel;
    block->prev = sMemory.sentinel.prev;
    sMemory.freeSize += size;
    sMemory.sentinel.prev->next = block;
    sMemory.sentinel.prev = block;
  }

  sMemory.sentinel.size = 0;
  sMemory.sentinel.tag = BLOCK_SENTINEL;
  sMemory.sentinel.prev = &sMemory.sentinel;
  sMemory.freeSize = 0;
  sMemory.sentinel.next = sMemory.sentinel.prev;
  AddMemoryRegion((void*)MAIN_REGION_START, (void*)MAIN_REGION_END);
  if (hasExpansionPak == TRUE) {
    AddMemoryRegion((void*)EXPANSION_REGION_START, (void*)EXPANSION_REGION_END);
  }
  sMemory.largestFreeSize = UNMEASURED_SIZE;
}

#else
INCLUDE_ASM("asm/nonmatchings/heap", AddMemoryRegion);

INCLUDE_ASM("asm/nonmatchings/heap", InitHeapRegions);
#endif

u32 GetFreeMemorySize(void) { return sMemory.freeSize; }

u32 FindLargestFreeMemorySize(void) {
  MemoryBlock* block = sMemory.sentinel.next;
  u32 largestFreeSize = 0;

  for (; block != &sMemory.sentinel; block = block->next) {
    if (block->tag == BLOCK_FREE && largestFreeSize < block->size) {
      largestFreeSize = block->size;
    }
  }
  sMemory.largestFreeSize = largestFreeSize;
  return largestFreeSize;
}

#ifdef NON_MATCHING
void* AllocMemory(u32 size) {
  u32 callerAddress;
  u32 largestFreeSize;
  s32 mask;
  u32 bestSize;
  MemoryBlock* bestBlock;
  MemoryBlock* block;
  MemoryBlock* rest;
  MemoryBlock* next;

  GET_RETURN_ADDRESS(callerAddress);
  largestFreeSize = 0;
  mask = osSetIntMask(OS_IM_NONE);
  /* The size is reused for the block's, to match */
  size = (size + sizeof(MemoryBlock) + 7) & ~7;
  bestSize = NO_BLOCK_SIZE;
  bestBlock = NULL;
  block = sMemory.sentinel.next;
  for (; block != &sMemory.sentinel; block = block->next) {
    if (block->tag == BLOCK_FREE && block->size >= size) {
      if (block->size < bestSize) {
        if (largestFreeSize < bestSize && bestSize != NO_BLOCK_SIZE) {
          largestFreeSize = bestSize;
        }
        bestSize = block->size;
        if (largestFreeSize < bestSize - size) {
          largestFreeSize = bestSize - size;
        }
        bestBlock = block;
      } else if (largestFreeSize < block->size) {
        largestFreeSize = block->size;
      }
    }
  }
  sMemory.largestFreeSize = largestFreeSize;
  if (bestBlock == NULL) {
    osSetIntMask(mask);
    osSyncPrintf(sOutOfMemoryMessage, callerAddress);
    return NULL;
  }
  if (bestBlock->size - size > sizeof(MemoryBlock)) {
    rest = (MemoryBlock*)((u8*)bestBlock + size);
    rest->tag = BLOCK_FREE;
    rest->size = bestBlock->size - size;
    bestBlock->next->prev = rest;
    next = bestBlock->next;
    sMemory.freeSize -= size;
    rest->next = next;
    bestBlock->next = rest;
    rest->prev = bestBlock;
    bestBlock->size = size;
  } else {
    sMemory.freeSize -= bestSize;
  }
  bestBlock->tag = BLOCK_USED;
  osSetIntMask(mask);
  return bestBlock + 1;
}

#else
INCLUDE_ASM("asm/nonmatchings/heap", AllocMemory);
#endif

/* "A NULL pointer was passed to free(). Called from %x" */
static const char sFreeNullMessage[] =
    "NULL\x83|\x83"
    "C\x83\x93\x83^\x82\xAA"
    "free()\x82\xC9\x93n\x82\xB3"
    "\x82\xEA\x82\xDC\x82\xB5\x82\xBD. call from %x\n";

/* Frees the block that *pointerToBlock points to and clears the pointer */
#ifdef NON_MATCHING
void FreeMemory(void* pointerToBlock) {
  u32 callerAddress;
  void* pointer;
  s32 mask;
  MemoryBlock* block;
  MemoryBlock* prev;

  GET_RETURN_ADDRESS(callerAddress);
  mask = osSetIntMask(OS_IM_NONE);
  pointer = *(void**)pointerToBlock;
  if (pointer == NULL) {
    osSetIntMask(mask);
    osSyncPrintf(sFreeNullMessage, callerAddress);
    return;
  }
  block = (MemoryBlock*)pointer - 1;
  prev = block->prev;
  sMemory.freeSize += block->size;
  if (prev->tag == BLOCK_FREE &&
      (MemoryBlock*)((u8*)prev + prev->size) == block) {
    block->next->prev = prev;
    block->prev->next = block->next;
    block->prev->size += block->size;
    block = block->prev;
  }
  if (block->next->tag == BLOCK_FREE &&
      (MemoryBlock*)((u8*)block + block->size) == block->next) {
    block->next->next->prev = block;
    block->size += block->next->size;
    block->next = block->next->next;
  }
  block->tag = BLOCK_FREE;
  *(void**)pointerToBlock = NULL;
  osSetIntMask(mask);
}

#else
INCLUDE_ASM("asm/nonmatchings/heap", FreeMemory);
#endif
