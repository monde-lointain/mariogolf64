/*
 * The heaps and the general memory pool share one allocator: each keeps
 * its blocks, used and free, in a circular list in address order, around
 * a block of its own that is never free. Allocations take the smallest
 * free block that fits, and frees merge a block with free neighbors.
 */
#include "common.h"

#include "heap.h"

#define HEAP_COUNT 3

/* Copies $ra, for the messages that name the caller of a failed call */
#define GET_RETURN_ADDRESS(variable) \
  __asm__ volatile("addu %0, $31, $0" : "=r"(variable))

/* A block's tag */
#define BLOCK_FREE 0
#define BLOCK_USED 0x12345678
#define BLOCK_SENTINEL -1

/* Larger than any block, so the first block that fits is the smallest so far */
#define NO_BLOCK_SIZE S32_MAX
/* The largest free size before any allocation has measured it */
#define UNMEASURED_SIZE -1

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

/* Allocations round each block, header included, up to a multiple of this */
#define BLOCK_ALIGNMENT 8
#define RoundUpToBlockAlignment(size) \
  (((size) + BLOCK_ALIGNMENT - 1) & ~(BLOCK_ALIGNMENT - 1))

/* The bytes from start up to end */
#define RegionSize(start, end) ((u8*)(end) - (u8*)(start))
/* The block that starts offset bytes into block */
#define BlockAt(block, offset) ((MemoryBlock*)((u8*)(block) + (offset)))
/* Whether second starts where first ends, so that freeing can merge them */
#define IsBlockFollowedBy(first, second) \
  (BlockAt((first), (first)->size) == (second))
/* The memory that a block's header precedes, which the caller gets */
#define BlockMemory(block) ((block) + 1)
/* The header of the block whose memory pointer is */
#define BlockOfMemory(pointer) ((MemoryBlock*)(pointer) - 1)

typedef struct Heap {
  /* 0x00 */ MemoryBlock sentinel;
  /* 0x10 */ u32 freeSize;
  /* 0x14 */ u32 largestFreeSize; /* The largest free block, measured by the
                                     last allocation */
} Heap; /* size = 0x18 */

static Heap f_Heaps[HEAP_COUNT];
static Heap f_Memory;

void AddHeapRegion(s32 heapIndex, void* start, void* end) {
  MemoryBlock* block = start;
  u32 size = RegionSize(start, end);

  block->size = size;
  f_Heaps[heapIndex].freeSize += size;
  block->tag = BLOCK_FREE;
  block->next = &f_Heaps[heapIndex].sentinel;
  block->prev = f_Heaps[heapIndex].sentinel.prev;
  f_Heaps[heapIndex].sentinel.prev->next = block;
  f_Heaps[heapIndex].sentinel.prev = block;
}

void ResetHeap(s32 heapIndex) {
  Heap* heap = &f_Heaps[heapIndex];

  f_Heaps[heapIndex].freeSize = 0;
  f_Heaps[heapIndex].sentinel.size = 0;
  f_Heaps[heapIndex].sentinel.tag = BLOCK_SENTINEL;
  heap->sentinel.prev = &heap->sentinel;
  f_Heaps[heapIndex].sentinel.next = &heap->sentinel;
  f_Heaps[heapIndex].largestFreeSize = UNMEASURED_SIZE;
}

s32 GetHeapFreeSize(s32 heapIndex) {
  return f_Heaps[heapIndex].freeSize;
}

s32 GetHeapLargestFreeSize(s32 heapIndex) {
  return f_Heaps[heapIndex].largestFreeSize;
}

/* "Unable to allocate memory %x" */
static const char f_OutOfMemoryMessage[] = "メモリが確保できません %x\n";

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
  heap = &f_Heaps[heapIndex];
  mask = osSetIntMask(OS_IM_NONE);

  /* The size is reused for the block's, to match */
  size = RoundUpToBlockAlignment(size + sizeof(MemoryBlock));
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

  f_Heaps[heapIndex].largestFreeSize = largestFreeSize;
  if (bestBlock == NULL) {
    osSetIntMask(mask);
    osSyncPrintf(f_OutOfMemoryMessage, callerAddress);
    return NULL;
  }

  if (bestBlock->size - size > sizeof(MemoryBlock)) {
    f_Heaps[heapIndex].freeSize -= size;
    rest = BlockAt(bestBlock, size);
    rest->size = bestBlock->size - size;
    rest->tag = BLOCK_FREE;
    bestBlock->next->prev = rest;
    rest->next = bestBlock->next;
    bestBlock->next = rest;
    rest->prev = bestBlock;
    bestBlock->size = size;
  } else {
    f_Heaps[heapIndex].freeSize -= bestSize;
  }

  bestBlock->tag = BLOCK_USED;
  osSetIntMask(mask);
  return BlockMemory(bestBlock);
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
    osSyncPrintf("NULLポインタがkFree()に渡されました");
    return;
  }

  block = BlockOfMemory(pointer);
  f_Heaps[heapIndex].freeSize += block->size;
  if (block->prev->tag == BLOCK_FREE && IsBlockFollowedBy(block->prev, block)) {
    block->next->prev = block->prev;
    block->prev->next = block->next;
    block->prev->size += block->size;
    block = block->prev;
  }

  if (block->next->tag == BLOCK_FREE && IsBlockFollowedBy(block, block->next)) {
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
void InitHeapRegions(Boolean hasExpansionPak) {
  void AddMemoryRegion(void* start, void* end) {
    MemoryBlock* block = start;
    u32 size = RegionSize(start, end);

    block->size = size;
    block->tag = BLOCK_FREE;
    block->next = &f_Memory.sentinel;
    block->prev = f_Memory.sentinel.prev;
    f_Memory.freeSize += size;
    f_Memory.sentinel.prev->next = block;
    f_Memory.sentinel.prev = block;
  }

  f_Memory.sentinel.size = 0;
  f_Memory.sentinel.tag = BLOCK_SENTINEL;
  f_Memory.sentinel.prev = &f_Memory.sentinel;
  f_Memory.freeSize = 0;
  f_Memory.sentinel.next = f_Memory.sentinel.prev;
  AddMemoryRegion((void*)MAIN_REGION_START, (void*)MAIN_REGION_END);
  if (hasExpansionPak == TRUE) {
    AddMemoryRegion((void*)EXPANSION_REGION_START, (void*)EXPANSION_REGION_END);
  }

  f_Memory.largestFreeSize = UNMEASURED_SIZE;
}

#else
INCLUDE_ASM("asm/nonmatchings/heap", AddMemoryRegion);

INCLUDE_ASM("asm/nonmatchings/heap", InitHeapRegions);
#endif

u32 GetFreeMemorySize(void) {
  return f_Memory.freeSize;
}

u32 FindLargestFreeMemorySize(void) {
  MemoryBlock* block = f_Memory.sentinel.next;
  u32 largestFreeSize = 0;

  for (; block != &f_Memory.sentinel; block = block->next) {
    if (block->tag == BLOCK_FREE && largestFreeSize < block->size) {
      largestFreeSize = block->size;
    }
  }

  f_Memory.largestFreeSize = largestFreeSize;
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
  size = RoundUpToBlockAlignment(size + sizeof(MemoryBlock));
  bestSize = NO_BLOCK_SIZE;
  bestBlock = NULL;
  block = f_Memory.sentinel.next;
  for (; block != &f_Memory.sentinel; block = block->next) {
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

  f_Memory.largestFreeSize = largestFreeSize;
  if (bestBlock == NULL) {
    osSetIntMask(mask);
    osSyncPrintf(f_OutOfMemoryMessage, callerAddress);
    return NULL;
  }

  if (bestBlock->size - size > sizeof(MemoryBlock)) {
    rest = BlockAt(bestBlock, size);
    rest->tag = BLOCK_FREE;
    rest->size = bestBlock->size - size;
    bestBlock->next->prev = rest;
    next = bestBlock->next;
    f_Memory.freeSize -= size;
    rest->next = next;
    bestBlock->next = rest;
    rest->prev = bestBlock;
    bestBlock->size = size;
  } else {
    f_Memory.freeSize -= bestSize;
  }

  bestBlock->tag = BLOCK_USED;
  osSetIntMask(mask);
  return BlockMemory(bestBlock);
}

#else
INCLUDE_ASM("asm/nonmatchings/heap", AllocMemory);
#endif

/* "A NULL pointer was passed to free(). Called from %x" */
static const char f_FreeNullMessage[] =
    "NULLポインタがfree()に渡されました. call from %x\n";

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
    osSyncPrintf(f_FreeNullMessage, callerAddress);
    return;
  }

  block = BlockOfMemory(pointer);
  prev = block->prev;
  f_Memory.freeSize += block->size;
  if (prev->tag == BLOCK_FREE && IsBlockFollowedBy(prev, block)) {
    block->next->prev = prev;
    block->prev->next = block->next;
    block->prev->size += block->size;
    block = block->prev;
  }

  if (block->next->tag == BLOCK_FREE && IsBlockFollowedBy(block, block->next)) {
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
