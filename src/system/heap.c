/*
 * The heaps and the general memory pool share one allocator: each keeps
 * its blocks, used and free, in a circular list in address order, around
 * a block of its own that is never free. Allocations take the smallest
 * free block that fits, and frees merge a block with free neighbors.
 */

#include "system/heap.h"

#include <ultra64.h>

#ifdef TARGET_PC
#    include <assert.h>
#endif

#include "math/math_util.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/memory_address.h"

// Copies $ra, for the messages that name the caller of a failed call
#ifdef __mips__
#    define GET_RETURN_ADDRESS(variable) __asm__ volatile("addu %0, $31, $0" : "=r"(variable))
#else
#    define GET_RETURN_ADDRESS(variable) ((variable) = (uintptr_t)__builtin_return_address(0))
#endif

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

// How a block's header links it to the next and previous blocks of its list.
// The native port links each block to another by the signed distance in
// bytes from it, not by a pointer, so that a header is 16 bytes there as it
// is on the N64. Every block takes the same room, and the game's memory runs
// out where the N64's does. A heap's blocks and the sentinel that it
// keeps are in the program's own memory, osPcRdram or its other variables,
// so a distance fits in 32 bits.
#ifdef TARGET_PC
typedef s32 BlockLink;
#else
typedef struct MemoryBlock *BlockLink;
#endif

// The header of each block, followed by its memory
typedef struct MemoryBlock {
    u32 size; // Including the header
    BlockTag tag;
    BlockLink next;
    BlockLink previous;
} MemoryBlock;

// The link from a block to another, and the routines that read and set a
// block's next and previous blocks. These take the block itself, not its
// address.
#ifdef TARGET_PC
_Static_assert(sizeof(MemoryBlock) == 16, "A block's header is the N64's size");

static BlockLink LinkBetween(const MemoryBlock *block, const MemoryBlock *other) {
    SignedAddress distance = (SignedAddress)(AddressNumber(other) - AddressNumber(block));

    assert(distance >= S32_MIN && distance <= S32_MAX);
    return (BlockLink)distance;
}

static MemoryBlock *LinkedBlock(const MemoryBlock *block, BlockLink link) {
    return (MemoryBlock *)(AddressNumber(block) + (UnsignedAddress)(SignedAddress)link);
}

#    define BlockLinkTo(block, other) LinkBetween((block), (other))
#    define NextBlock(block) LinkedBlock(&(block), (block).next)
#    define PreviousBlock(block) LinkedBlock(&(block), (block).previous)
#else
#    define BlockLinkTo(block, other) (other)
#    define NextBlock(block) ((block).next)
#    define PreviousBlock(block) ((block).previous)
#endif
#define SetNextBlock(block, other) ((block).next = BlockLinkTo(&(block), (other)))
#define SetPreviousBlock(block, other) ((block).previous = BlockLinkTo(&(block), (other)))

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
// The main memory's heap, laid out as a Heap is but kept in separate
// variables.
//
// Under KMC the sentinel is volatile. The qualifier does no work: the
// allocating and freeing routines mask interrupts around their work on the
// list, and the pointers that link the list reach the sentinel without it.
// Other builds declare it plain.
#ifdef __IS_KMC__
static volatile MemoryBlock f_MemorySentinel;
#else
static MemoryBlock f_MemorySentinel;
#endif
static u32 f_FreeMemorySize;
static u32 f_LargestFreeMemorySize; // As the last allocation measured

// Gives the memory from start to end to the heap as a free block
void AddHeapRegion(HeapIndex heapIndex, void *start, void *end) {
    MemoryBlock *block = start;
    u32 size = BytesBetween(start, end);

    block->size = size;
    f_Heaps[heapIndex].freeSize += size;
    block->tag = BLOCK_FREE;
    SetNextBlock(*block, &f_Heaps[heapIndex].sentinel);
    SetPreviousBlock(*block, PreviousBlock(f_Heaps[heapIndex].sentinel));
    SetNextBlock(*PreviousBlock(f_Heaps[heapIndex].sentinel), block);
    SetPreviousBlock(f_Heaps[heapIndex].sentinel, block);
}

// Empties the heap of all its blocks, so that it needs regions added again
void ResetHeap(HeapIndex heapIndex) {
    Heap *heap = &f_Heaps[heapIndex];

    f_Heaps[heapIndex].freeSize = 0;
    f_Heaps[heapIndex].sentinel.size = 0;
    f_Heaps[heapIndex].sentinel.tag = BLOCK_SENTINEL;
    SetPreviousBlock(heap->sentinel, &heap->sentinel);
    SetNextBlock(f_Heaps[heapIndex].sentinel, &heap->sentinel);
    f_Heaps[heapIndex].largestFreeSize = UNMEASURED_SIZE;
}

u32 GetHeapFreeSize(HeapIndex heapIndex) {
    return f_Heaps[heapIndex].freeSize;
}

u32 GetHeapLargestFreeSize(HeapIndex heapIndex) {
    return f_Heaps[heapIndex].largestFreeSize;
}

// Allocates size bytes from the smallest free block of the heap that fits,
// and returns NULL if none does
void *AllocFromHeap(HeapIndex heapIndex, u32 size) {
    u32 callerAddress;
    u32 largestFreeSize;
    Heap *heap;
    OSIntMask mask;
    u32 bestSize;
    MemoryBlock *block;
    MemoryBlock *bestBlock;
    MemoryBlock *rest;

    GET_RETURN_ADDRESS(callerAddress);
    largestFreeSize = 0;
    heap = &f_Heaps[heapIndex];
    mask = osSetIntMask(OS_IM_NONE);

    // From here on, size is the whole block's, header included
    size = RoundUpTo(size + sizeof(MemoryBlock), BLOCK_ALIGNMENT);
    bestSize = NO_BLOCK_SIZE;
    block = NextBlock(heap->sentinel);
    bestBlock = NULL;

    // Find the smallest free block that fits, and the largest free size that
    // will be left once it is taken
    for (; block != &heap->sentinel; block = NextBlock(*block)) {
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
        ForgetMemoryTypes(rest, sizeof(MemoryBlock));
        rest->size = bestBlock->size - size;
        rest->tag = BLOCK_FREE;
        SetPreviousBlock(*NextBlock(*bestBlock), rest);
        SetNextBlock(*rest, NextBlock(*bestBlock));
        SetNextBlock(*bestBlock, rest);
        SetPreviousBlock(*rest, bestBlock);
        bestBlock->size = size;
    }
    else {
        f_Heaps[heapIndex].freeSize -= bestSize;
    }

    bestBlock->tag = BLOCK_USED;
    ForgetMemoryTypes(BlockMemory(bestBlock), bestBlock->size - sizeof(MemoryBlock));
    osSetIntMask(mask);
    return BlockMemory(bestBlock);
}

// Frees the block that *pointerToBlock points to and clears the pointer
void FreeFromHeap(HeapIndex heapIndex, void *pointerToBlock) {
    void *pointer;
    OSIntMask mask;
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
    if (PreviousBlock(*block)->tag == BLOCK_FREE &&
        IsBlockFollowedBy(PreviousBlock(*block), block)) {
        SetPreviousBlock(*NextBlock(*block), PreviousBlock(*block));
        SetNextBlock(*PreviousBlock(*block), NextBlock(*block));
        PreviousBlock(*block)->size += block->size;
        block = PreviousBlock(*block);
    }

    if (NextBlock(*block)->tag == BLOCK_FREE && IsBlockFollowedBy(block, NextBlock(*block))) {
        SetPreviousBlock(*NextBlock(*NextBlock(*block)), block);
        block->size += NextBlock(*block)->size;
        SetNextBlock(*block, NextBlock(*NextBlock(*block)));
    }

    block->tag = BLOCK_FREE;
    *(void **)pointerToBlock = NULL;
    osSetIntMask(mask);
}

// Fills the main memory's heap with the main region, and with the Expansion
// Pak's memory if there is one
void InitHeapRegions(Boolean hasExpansionPak) {
    // Nested, a GNU C extension. The casts on &f_MemorySentinel only drop the
    // qualifier, which the pointers that link the list don't carry.
    void AddMemoryRegion(void *start, void *end) {
        MemoryBlock *block = start;
        u32 size = BytesBetween(start, end);

        block->size = size;
        block->tag = BLOCK_FREE;
        SetNextBlock(*block, (MemoryBlock *)&f_MemorySentinel);
        SetPreviousBlock(*block, PreviousBlock(f_MemorySentinel));
        f_FreeMemorySize += size;
        SetNextBlock(*PreviousBlock(f_MemorySentinel), block);
        SetPreviousBlock(f_MemorySentinel, block);
    }

    f_MemorySentinel.size = 0;
    f_MemorySentinel.tag = BLOCK_SENTINEL;
    f_MemorySentinel.next = f_MemorySentinel.previous =
        BlockLinkTo((MemoryBlock *)&f_MemorySentinel, (MemoryBlock *)&f_MemorySentinel);
    f_FreeMemorySize = 0;

    AddMemoryRegion(FixedAddress(MAIN_REGION_START), FixedAddress(MAIN_REGION_END));
    if (hasExpansionPak == TRUE) {
        AddMemoryRegion(FixedAddress(EXPANSION_REGION_START), FixedAddress(EXPANSION_REGION_END));
    }

    f_LargestFreeMemorySize = UNMEASURED_SIZE;
}

u32 GetFreeMemorySize(void) {
    return f_FreeMemorySize;
}

// Measures the largest free block of the main memory's heap
u32 FindLargestFreeMemorySize(void) {
    MemoryBlock *block = NextBlock(f_MemorySentinel);
    u32 largestFreeSize = 0;

    for (; block != &f_MemorySentinel; block = NextBlock(*block)) {
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
    OSIntMask mask;
    u32 bestSize;
    MemoryBlock *bestBlock;
    MemoryBlock *block;
    MemoryBlock *rest;

    GET_RETURN_ADDRESS(callerAddress);
    largestFreeSize = 0;
    mask = osSetIntMask(OS_IM_NONE);

    // From here on, size is the whole block's, header included
    size = RoundUpTo(size + sizeof(MemoryBlock), BLOCK_ALIGNMENT);
    bestSize = NO_BLOCK_SIZE;
    bestBlock = NULL;
    block = NextBlock(f_MemorySentinel);
    // Find the smallest free block that fits, and the largest free size that
    // will be left once it is taken
    for (; block != &f_MemorySentinel; block = NextBlock(*block)) {
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
        ForgetMemoryTypes(rest, sizeof(MemoryBlock));
        rest->size = bestBlock->size - size;
        rest->tag = BLOCK_FREE;
        SetPreviousBlock(*NextBlock(*bestBlock), rest);
        SetNextBlock(*rest, NextBlock(*bestBlock));
        SetNextBlock(*bestBlock, rest);
        SetPreviousBlock(*rest, bestBlock);
        bestBlock->size = size;
    }
    else {
        f_FreeMemorySize -= bestSize;
    }

    bestBlock->tag = BLOCK_USED;
    ForgetMemoryTypes(BlockMemory(bestBlock), bestBlock->size - sizeof(MemoryBlock));
    osSetIntMask(mask);
    return BlockMemory(bestBlock);
}

// Frees the block that *pointerToBlock points to and clears the pointer
void FreeMemory(void *pointerToBlock) {
    u32 callerAddress;
    void *pointer;
    OSIntMask mask;
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
    previous = PreviousBlock(*block);
    f_FreeMemorySize += block->size;
    // Merge the block with the free blocks on either side of it
    if (previous->tag == BLOCK_FREE && IsBlockFollowedBy(previous, block)) {
        SetPreviousBlock(*NextBlock(*block), previous);
        SetNextBlock(*PreviousBlock(*block), NextBlock(*block));
        PreviousBlock(*block)->size += block->size;
        block = PreviousBlock(*block);
    }

    if (NextBlock(*block)->tag == BLOCK_FREE && IsBlockFollowedBy(block, NextBlock(*block))) {
        SetPreviousBlock(*NextBlock(*NextBlock(*block)), block);
        block->size += NextBlock(*block)->size;
        SetNextBlock(*block, NextBlock(*NextBlock(*block)));
    }

    block->tag = BLOCK_FREE;
    *(void **)pointerToBlock = NULL;
    osSetIntMask(mask);
}
