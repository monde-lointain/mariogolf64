/*
 * The heaps and the general memory pool share one allocator: each keeps
 * its blocks, used and free, in a circular list in address order, around
 * a block of its own that is never free. Allocations take the smallest
 * free block that fits, and frees merge a block with free neighbors.
 */

#include "system/heap.h"

#include <ultra64.h>

#include "math/math_util.h"
#include "system/c_language.h"
#include "system/memory_address.h"

// Heaps, numbered from 0, besides the main memory's
#define HEAP_COUNT 3

// Copies $ra, for the messages that name the caller of a failed call
#define GET_RETURN_ADDRESS(variable) __asm__ volatile("addu %0, $31, $0" : "=r"(variable))

// A block's tag
enum BlockTag {
    BLOCK_SENTINEL = -1,
    BLOCK_FREE,
    BLOCK_USED = 0x12345678
};

typedef s32 BlockTag;

// Larger than any block, so the first block that fits is the smallest so far
#define NO_BLOCK_SIZE S32_MAX
// The largest free size before any allocation has measured it
#define UNMEASURED_SIZE (-1)

// The Expansion Pak's memory
#define EXPANSION_REGION_START 0x80600000
#define EXPANSION_REGION_END 0x80800000

// The header of each block, followed by its memory
typedef struct MemoryBlock {
    u32 size; // Including the header
    BlockTag tag;
    struct MemoryBlock *next;
    struct MemoryBlock *previous;
} MemoryBlock;

// Allocations round each block, header included, up to a multiple of this
#define BLOCK_ALIGNMENT 8

// The block that starts offset bytes into block
#define BlockAt(block, offset) ((MemoryBlock *)AddressPast((block), (offset)))
// Whether second starts where first ends, so that freeing can merge them
#define IsBlockFollowedBy(first, second) (BlockAt((first), (first)->size) == (second))
// The memory that a block's header precedes, which the caller gets
#define BlockMemory(block) ((block) + 1)
// The header of the block whose memory pointer is
#define BlockOfMemory(pointer) ((MemoryBlock *)(pointer) - 1)

typedef struct Heap {
    MemoryBlock sentinel;
    u32 freeSize;
    u32 largestFreeSize; // The largest free block, as the last allocation measured
} Heap;

static Heap f_Heaps[HEAP_COUNT];
// The main memory's heap is laid out as a Heap is, but the original keeps
// its parts in separate variables, which changes the code that uses them.
//
// The sentinel is volatile, as InitHeapRegions needs it to match. The
// allocating and freeing routines mask interrupts around their work on the
// list, so the original took other threads to share it; the routines that
// walk the list compile the same either way.
static volatile MemoryBlock f_MemorySentinel;
static u32 f_FreeMemorySize;
static u32 f_LargestFreeMemorySize; // As the last allocation measured

// Gives the memory from start to end to the heap as a free block
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

// Empties the heap of all its blocks, so that it needs regions added again
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

// Allocates size bytes from the smallest free block of the heap that fits,
// and returns NULL if none does
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

    // The size is reused for the block's, to match: a blockSize local of its
    // own gives 66 words differing, 8 in shape
    size = RoundUpTo(size + sizeof(MemoryBlock), BLOCK_ALIGNMENT);
    bestSize = NO_BLOCK_SIZE;
    block = heap->sentinel.next;
    bestBlock = NULL;

    // Find the smallest free block that fits, and the largest free size that
    // will be left once it is taken
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
        // "Unable to allocate memory %x"
        osSyncPrintf("メモリが確保できません %x\n", callerAddress);
        return NULL;
    }

    // Split off the rest of the block as a free block, if more than a header
    // is left over
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

// Frees the block that *pointerToBlock points to and clears the pointer
void FreeFromHeap(s32 heapIndex, void *pointerToBlock) {
    void *pointer;
    s32 mask;
    MemoryBlock *block;

    mask = osSetIntMask(OS_IM_NONE);
    pointer = *(void **)pointerToBlock;

    if (pointer == NULL) {
        osSetIntMask(mask);
        // "A NULL pointer was passed to kFree()"
        osSyncPrintf("NULLポインタがkFree()に渡されました");
        return;
    }

    block = BlockOfMemory(pointer);
    f_Heaps[heapIndex].freeSize += block->size;
    // Merge the block with the free blocks on either side of it
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

// Fills the main memory's heap with the main region, and with the Expansion
// Pak's memory if there is one
void InitHeapRegions(Boolean hasExpansionPak) {
    // Nested, a GNU C extension, as in the original: the calls pass a static
    // chain that it saves but never uses. The casts on &f_MemorySentinel only
    // drop the qualifier, which the pointers that link the list don't carry.
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

    // The original stores the sentinel's size and tag before anything else,
    // and loads its previous back from memory to set next. Both come of the
    // sentinel being volatile: the scheduler keeps volatile accesses in the
    // order written, and a chained assignment reads a volatile target back
    // after storing it, where it would otherwise hand the stored value on
    // (expr.c, store_expr). Setting next from previous in a statement of its
    // own matches too, as does clearing the free size before these stores.
    //
    // With the sentinel not volatile, the chained assignment gives 10 words
    // differing (4 in shape) and the separate statements 6 (4), as cse takes
    // the address just stored for the value read. Casts to a volatile
    // MemoryBlock match only at all three of the size's store, the tag's
    // store and previous's read; a volatile previous member gives 12 (8).
    // With the sentinel volatile, setting next to &f_MemorySentinel rather
    // than to previous gives 10 (4), chaining previous = next = 6 (0),
    // storing the tag before the size 4 (0), and storing through a pointer
    // local to the sentinel 9 (7).
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

// Measures the largest free block of the main memory's heap
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

// Allocates size bytes from the main memory's heap, as AllocFromHeap does
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

    // The size is reused for the block's, to match: a blockSize local of its
    // own gives 66 words differing, 8 in shape
    size = RoundUpTo(size + sizeof(MemoryBlock), BLOCK_ALIGNMENT);
    bestSize = NO_BLOCK_SIZE;
    bestBlock = NULL;
    block = f_MemorySentinel.next;
    // Find the smallest free block that fits, and the largest free size that
    // will be left once it is taken
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
        // "Unable to allocate memory %x"
        osSyncPrintf("メモリが確保できません %x\n", callerAddress);
        return NULL;
    }

    // Split off the rest of the block as a free block, if more than a header
    // is left over
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

// Frees the block that *pointerToBlock points to and clears the pointer
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
        // "A NULL pointer was passed to free(). Called from %x"
        osSyncPrintf("NULLポインタがfree()に渡されました. call from %x\n", callerAddress);
        return;
    }

    block = BlockOfMemory(pointer);
    previous = block->previous;
    f_FreeMemorySize += block->size;
    // Merge the block with the free blocks on either side of it
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
