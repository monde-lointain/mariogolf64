#include "system/heap.h"

#include <ultra64.h>

#include "math_util.h"
#include "system/c_language.h"
#include "system/memory_address.h"

#define HEAP_COUNT 3

#define GET_RETURN_ADDRESS(variable) __asm__ volatile("addu %0, $31, $0" : "=r"(variable))

enum BlockTag {
    BLOCK_SENTINEL = -1,
    BLOCK_FREE,
    BLOCK_USED = 0x12345678
};

typedef s32 BlockTag;

#define NO_BLOCK_SIZE S32_MAX
#define UNMEASURED_SIZE (-1)

#define EXPANSION_REGION_START 0x80600000
#define EXPANSION_REGION_END 0x80800000

typedef struct MemoryBlock {
    u32 size;
    BlockTag tag;
    struct MemoryBlock *next;
    struct MemoryBlock *previous;
} MemoryBlock;

#define BLOCK_ALIGNMENT 8

#define BlockAt(block, offset) ((MemoryBlock *)AddressPast((block), (offset)))
#define IsBlockFollowedBy(first, second) (BlockAt((first), (first)->size) == (second))
#define BlockMemory(block) ((block) + 1)
#define BlockOfMemory(pointer) ((MemoryBlock *)(pointer) - 1)

typedef struct Heap {
    MemoryBlock sentinel;
    u32 freeSize;
    u32 largestFreeSize;
} Heap;

static Heap f_Heaps[HEAP_COUNT];
static volatile MemoryBlock f_MemorySentinel;
static u32 f_FreeMemorySize;
static u32 f_LargestFreeMemorySize;

void AddHeapRegion(s32 heapIndex, void *start, void *end) {
    MemoryBlock *block = start;
    u32 size = BytesBetween(start, end);

    block->size = size;
    f_Heaps[heapIndex].freeSize += size;
    block->tag = BLOCK_FREE;
    block->next = &f_Heaps[heapIndex].sentinel;
    block->previous = f_Heaps[heapIndex].sentinel.previous;
    f_Heaps[heapIndex].sentinel.previous->next = block;
    f_Heaps[heapIndex].sentinel.previous = block;
}

void ResetHeap(s32 heapIndex) {
    Heap *heap = &f_Heaps[heapIndex];

    f_Heaps[heapIndex].freeSize = 0;
    f_Heaps[heapIndex].sentinel.size = 0;
    f_Heaps[heapIndex].sentinel.tag = BLOCK_SENTINEL;
    heap->sentinel.previous = &heap->sentinel;
    f_Heaps[heapIndex].sentinel.next = &heap->sentinel;
    f_Heaps[heapIndex].largestFreeSize = UNMEASURED_SIZE;
}

s32 GetHeapFreeSize(s32 heapIndex) {
    return f_Heaps[heapIndex].freeSize;
}

s32 GetHeapLargestFreeSize(s32 heapIndex) {
    return f_Heaps[heapIndex].largestFreeSize;
}

void *AllocFromHeap(s32 heapIndex, u32 size) {
    u32 callerAddress;
    u32 largestFreeSize;
    Heap *heap;
    s32 mask;
    u32 bestSize;
    MemoryBlock *block;
    MemoryBlock *bestBlock;
    MemoryBlock *rest;

    GET_RETURN_ADDRESS(callerAddress);
    largestFreeSize = 0;
    heap = &f_Heaps[heapIndex];
    mask = osSetIntMask(OS_IM_NONE);

    size = RoundUpTo(size + sizeof(MemoryBlock), BLOCK_ALIGNMENT);
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
            }
            else if (largestFreeSize < block->size) {
                largestFreeSize = block->size;
            }
        }
    }

    f_Heaps[heapIndex].largestFreeSize = largestFreeSize;
    if (bestBlock == NULL) {
        osSetIntMask(mask);
        osSyncPrintf("メモリが確保できません %x\n", callerAddress);
        return NULL;
    }

    if (bestBlock->size - size > sizeof(MemoryBlock)) {
        f_Heaps[heapIndex].freeSize -= size;
        rest = BlockAt(bestBlock, size);
        rest->size = bestBlock->size - size;
        rest->tag = BLOCK_FREE;
        bestBlock->next->previous = rest;
        rest->next = bestBlock->next;
        bestBlock->next = rest;
        rest->previous = bestBlock;
        bestBlock->size = size;
    }
    else {
        f_Heaps[heapIndex].freeSize -= bestSize;
    }

    bestBlock->tag = BLOCK_USED;
    osSetIntMask(mask);
    return BlockMemory(bestBlock);
}

void FreeFromHeap(s32 heapIndex, void *pointerToBlock) {
    void *pointer;
    s32 mask;
    MemoryBlock *block;

    mask = osSetIntMask(OS_IM_NONE);
    pointer = *(void **)pointerToBlock;

    if (pointer == NULL) {
        osSetIntMask(mask);

        osSyncPrintf("NULLポインタがkFree()に渡されました");
        return;
    }

    block = BlockOfMemory(pointer);
    f_Heaps[heapIndex].freeSize += block->size;
    if (block->previous->tag == BLOCK_FREE && IsBlockFollowedBy(block->previous, block)) {
        block->next->previous = block->previous;
        block->previous->next = block->next;
        block->previous->size += block->size;
        block = block->previous;
    }

    if (block->next->tag == BLOCK_FREE && IsBlockFollowedBy(block, block->next)) {
        block->next->next->previous = block;
        block->size += block->next->size;
        block->next = block->next->next;
    }

    block->tag = BLOCK_FREE;
    *(void **)pointerToBlock = NULL;
    osSetIntMask(mask);
}

void InitHeapRegions(Boolean hasExpansionPak) {
    void AddMemoryRegion(void *start, void *end) {
        MemoryBlock *block = start;
        u32 size = BytesBetween(start, end);

        block->size = size;
        block->tag = BLOCK_FREE;
        block->next = (MemoryBlock *)&f_MemorySentinel;
        block->previous = f_MemorySentinel.previous;
        f_FreeMemorySize += size;
        f_MemorySentinel.previous->next = block;
        f_MemorySentinel.previous = block;
    }

    f_MemorySentinel.size = 0;
    f_MemorySentinel.tag = BLOCK_SENTINEL;
    f_MemorySentinel.next = f_MemorySentinel.previous = (MemoryBlock *)&f_MemorySentinel;
    f_FreeMemorySize = 0;
    AddMemoryRegion((void *)MAIN_REGION_START, (void *)MAIN_REGION_END);
    if (hasExpansionPak == TRUE) {
        AddMemoryRegion((void *)EXPANSION_REGION_START, (void *)EXPANSION_REGION_END);
    }

    f_LargestFreeMemorySize = UNMEASURED_SIZE;
}

u32 GetFreeMemorySize(void) {
    return f_FreeMemorySize;
}

u32 FindLargestFreeMemorySize(void) {
    MemoryBlock *block = f_MemorySentinel.next;
    u32 largestFreeSize = 0;

    for (; block != &f_MemorySentinel; block = block->next) {
        if (block->tag == BLOCK_FREE && largestFreeSize < block->size) {
            largestFreeSize = block->size;
        }
    }

    f_LargestFreeMemorySize = largestFreeSize;
    return largestFreeSize;
}

void *AllocMemory(u32 size) {
    u32 callerAddress;
    u32 largestFreeSize;
    s32 mask;
    u32 bestSize;
    MemoryBlock *bestBlock;
    MemoryBlock *block;
    MemoryBlock *rest;

    GET_RETURN_ADDRESS(callerAddress);
    largestFreeSize = 0;
    mask = osSetIntMask(OS_IM_NONE);

    size = RoundUpTo(size + sizeof(MemoryBlock), BLOCK_ALIGNMENT);
    bestSize = NO_BLOCK_SIZE;
    bestBlock = NULL;
    block = f_MemorySentinel.next;
    for (; block != &f_MemorySentinel; block = block->next) {
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
            }
            else if (largestFreeSize < block->size) {
                largestFreeSize = block->size;
            }
        }
    }

    f_LargestFreeMemorySize = largestFreeSize;
    if (bestBlock == NULL) {
        osSetIntMask(mask);
        osSyncPrintf("メモリが確保できません %x\n", callerAddress);
        return NULL;
    }

    if (bestBlock->size - size > sizeof(MemoryBlock)) {
        f_FreeMemorySize -= size;
        rest = BlockAt(bestBlock, size);
        rest->size = bestBlock->size - size;
        rest->tag = BLOCK_FREE;
        bestBlock->next->previous = rest;
        rest->next = bestBlock->next;
        bestBlock->next = rest;
        rest->previous = bestBlock;
        bestBlock->size = size;
    }
    else {
        f_FreeMemorySize -= bestSize;
    }

    bestBlock->tag = BLOCK_USED;
    osSetIntMask(mask);
    return BlockMemory(bestBlock);
}

void FreeMemory(void *pointerToBlock) {
    u32 callerAddress;
    void *pointer;
    s32 mask;
    MemoryBlock *block;
    MemoryBlock *previous;

    GET_RETURN_ADDRESS(callerAddress);
    mask = osSetIntMask(OS_IM_NONE);
    pointer = *(void **)pointerToBlock;
    if (pointer == NULL) {
        osSetIntMask(mask);
        osSyncPrintf("NULLポインタがfree()に渡されました. call from %x\n", callerAddress);
        return;
    }

    block = BlockOfMemory(pointer);
    previous = block->previous;
    f_FreeMemorySize += block->size;
    if (previous->tag == BLOCK_FREE && IsBlockFollowedBy(previous, block)) {
        block->next->previous = previous;
        block->previous->next = block->next;
        block->previous->size += block->size;
        block = block->previous;
    }

    if (block->next->tag == BLOCK_FREE && IsBlockFollowedBy(block, block->next)) {
        block->next->next->previous = block;
        block->size += block->next->size;
        block->next = block->next->next;
    }

    block->tag = BLOCK_FREE;
    *(void **)pointerToBlock = NULL;
    osSetIntMask(mask);
}
