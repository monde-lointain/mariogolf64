/*
 * Tests of the game's allocator: which block each allocation takes, when it
 * splits a block, how frees merge neighbors, and what the sizes it reports
 * say. The heaps and the main memory have routines of their own that work
 * alike, so the tests of what they share run on both. The sizes sit on each
 * side of the split's boundary, where a block's leftover is one header or
 * one alignment step more.
 */

#include <ultra64.h>

#include "system/c_language.h"
#include "system/heap.h"
#include "system/memory_address.h"
#include "test_support.h"

// The header that heap.c puts before each block: a size, a tag and the
// list's two links, 16 bytes as on the N64, so that the game's memory runs
// out where the original's does
#define HEADER_SIZE 16

#define HEAP_REGION_SIZE 512

#define EXPANSION_SIZE 0x200000

// Link seams for what heap.c calls: the interrupt mask, which the fake keeps
// so that the tests can check that every path restores it, and the log
_Alignas(64) u8 osPcRdram[PC_RDRAM_SIZE];
static OSIntMask f_InterruptMask = OS_IM_ALL;
static int f_LogCount;

OSIntMask osSetIntMask(OSIntMask mask) {
    OSIntMask previous = f_InterruptMask;

    f_InterruptMask = mask;
    return previous;
}

void osSyncPrintf(const char *format, ...) {
    (void)format;
    f_LogCount++;
}

// The routines of a heap or of the main memory, one region of which each
// test starts from
typedef struct Allocator {
    u8 *(*Start)(void); // Empties it, gives it one region and returns its start
    u32 regionSize;
    void *(*Alloc)(u32 size);
    void (*Free)(void *pointerToBlock);
    u32 (*FreeSize)(void);
} Allocator;

// A static region, as the game's heaps are, which the program's start
// leaves untyped under the type sanitizer as it does the RDRAM's
static _Alignas(8) u8 f_HeapRegion[HEAP_REGION_SIZE];

static u8 *StartHeap(void) {
    ResetHeap(HEAP_BILLBOARD);
    AddHeapRegion(HEAP_BILLBOARD, f_HeapRegion, f_HeapRegion + HEAP_REGION_SIZE);
    return f_HeapRegion;
}

static void *AllocFromTestHeap(u32 size) {
    return AllocFromHeap(HEAP_BILLBOARD, size);
}

static void FreeFromTestHeap(void *pointerToBlock) {
    FreeFromHeap(HEAP_BILLBOARD, pointerToBlock);
}

static u32 GetTestHeapFreeSize(void) {
    return GetHeapFreeSize(HEAP_BILLBOARD);
}

static u8 *StartMainMemory(void) {
    InitHeapRegions(FALSE);
    return FixedAddress(MAIN_REGION_START);
}

static const Allocator f_Heap =
    {StartHeap, HEAP_REGION_SIZE, AllocFromTestHeap, FreeFromTestHeap, GetTestHeapFreeSize};

static const Allocator f_MainMemory =
    {StartMainMemory, MAIN_REGION_SIZE, AllocMemory, FreeMemory, GetFreeMemorySize};

// The memory size that gives a block of blockSize, header included
static u32 MemorySizeFor(u32 blockSize) {
    return blockSize - (u32)HEADER_SIZE;
}

// One alignment step more than a header is left, so the rest becomes a free
// block of its own, from which a small allocation is then taken
static void TestSplitsOffTheRest(const Allocator *allocator) {
    u8 *start = allocator->Start();
    u32 blockSize = allocator->regionSize - HEADER_SIZE - 8;

    EXPECT(allocator->Alloc(MemorySizeFor(blockSize)) == start + HEADER_SIZE);
    EXPECT(allocator->FreeSize() == HEADER_SIZE + 8);
    EXPECT(allocator->Alloc(1) == start + blockSize + HEADER_SIZE);
    EXPECT(allocator->FreeSize() == 0);
    EXPECT(f_InterruptMask == OS_IM_ALL);
}

// Exactly a header is left, so the caller gets the whole block
static void TestKeepsALeftoverHeaderInTheBlock(const Allocator *allocator) {
    u8 *start = allocator->Start();

    EXPECT(
        allocator->Alloc(MemorySizeFor(allocator->regionSize - HEADER_SIZE)) == start + HEADER_SIZE
    );
    EXPECT(allocator->FreeSize() == 0);
}

// A byte less still rounds up to the same block
static void TestRoundsBlocksUp(const Allocator *allocator) {
    allocator->Start();
    EXPECT(allocator->Alloc(MemorySizeFor(allocator->regionSize - HEADER_SIZE) - 1) != NULL);
    EXPECT(allocator->FreeSize() == 0);
}

static void TestFailsWhenNothingFits(const Allocator *allocator) {
    u8 *start = allocator->Start();
    int logCount = f_LogCount;

    EXPECT(allocator->Alloc(MemorySizeFor(allocator->regionSize) + 1) == NULL);
    EXPECT(f_LogCount == logCount + 1);
    EXPECT(f_InterruptMask == OS_IM_ALL);
    EXPECT(allocator->FreeSize() == allocator->regionSize);
    // The whole region fits exactly
    EXPECT(allocator->Alloc(MemorySizeFor(allocator->regionSize)) == start + HEADER_SIZE);
}

// Frees the first and third of blocks of the given sizes, which a fourth
// keeps apart from the rest of the region
static u8 *FreeFirstAndThird(const Allocator *allocator, u32 firstSize, u32 otherSize) {
    u8 *start = allocator->Start();
    u8 *first = allocator->Alloc(MemorySizeFor(firstSize));
    u8 *second = allocator->Alloc(MemorySizeFor(otherSize));
    u8 *third = allocator->Alloc(MemorySizeFor(otherSize));

    EXPECT(allocator->Alloc(MemorySizeFor(otherSize)) == third + otherSize);
    allocator->Free(&first);
    allocator->Free(&third);
    EXPECT(first == NULL && third == NULL && second != NULL);
    return start;
}

// The smaller of two blocks that fit is taken, though the larger comes
// first
static void TestTakesTheSmallestBlockThatFits(const Allocator *allocator) {
    u8 *start = FreeFirstAndThird(allocator, 128, 64);

    EXPECT(allocator->Alloc(MemorySizeFor(64)) == start + 128 + 64 + HEADER_SIZE);
}

// Of two the same size, the first
static void TestTakesTheFirstOfEqualBlocks(const Allocator *allocator) {
    u8 *start = FreeFirstAndThird(allocator, 64, 64);

    EXPECT(allocator->Alloc(MemorySizeFor(64)) == start + HEADER_SIZE);
}

// Freeing the middle of three blocks, with both neighbors already free,
// gives back one block the region's size
static void TestMergesFreeNeighbors(const Allocator *allocator) {
    u8 *start = allocator->Start();
    u32 regionSize = allocator->regionSize;
    u8 *first = allocator->Alloc(MemorySizeFor(64));
    u8 *middle = allocator->Alloc(MemorySizeFor(64));
    u8 *last = allocator->Alloc(MemorySizeFor(64));

    EXPECT(middle == first + 64 && last == middle + 64);
    allocator->Free(&first);
    allocator->Free(&last);
    EXPECT(allocator->FreeSize() == regionSize - 64);
    EXPECT(allocator->Alloc(MemorySizeFor(regionSize - 64)) == NULL);

    allocator->Free(&middle);
    EXPECT(middle == NULL);
    EXPECT(f_InterruptMask == OS_IM_ALL);
    EXPECT(allocator->FreeSize() == regionSize);
    EXPECT(allocator->Alloc(MemorySizeFor(regionSize)) == start + HEADER_SIZE);
}

static void TestLogsFreeingNull(const Allocator *allocator) {
    void *pointer = NULL;
    int logCount = f_LogCount;

    allocator->Free(&pointer);
    EXPECT(f_LogCount == logCount + 1);
    EXPECT(f_InterruptMask == OS_IM_ALL);
}

// A block that its last user stored its own types in is freed, and a
// header for the rest of a smaller block is put over them. The header's
// stores must begin new objects rather than store over the user's under a
// type that strict aliasing lets the compiler assume differs, which the
// type sanitizer reports; the other builds only check where the blocks go.
static void TestPutsHeadersOverUsedMemory(const Allocator *allocator) {
    u8 *start = allocator->Start();
    u32 memorySize = MemorySizeFor(allocator->regionSize);
    s32 *memory = allocator->Alloc(memorySize);
    u32 i;

    for (i = 0; i < memorySize / sizeof(s32); i++) {
        memory[i] = (s32)i;
    }

    allocator->Free(&memory);
    EXPECT(allocator->Alloc(MemorySizeFor(64)) == start + HEADER_SIZE);
    EXPECT(allocator->Alloc(MemorySizeFor(64)) == start + 64 + HEADER_SIZE);
    EXPECT(allocator->FreeSize() == allocator->regionSize - 128);
}

static void TestAllocator(const Allocator *allocator) {
    TestSplitsOffTheRest(allocator);
    TestKeepsALeftoverHeaderInTheBlock(allocator);
    TestRoundsBlocksUp(allocator);
    TestFailsWhenNothingFits(allocator);
    TestTakesTheSmallestBlockThatFits(allocator);
    TestTakesTheFirstOfEqualBlocks(allocator);
    TestMergesFreeNeighbors(allocator);
    TestLogsFreeingNull(allocator);
    TestPutsHeadersOverUsedMemory(allocator);
}

static void TestAddsRegionsAsFreeMemory(void) {
    ResetHeap(HEAP_BILLBOARD);
    // Nothing has measured the largest block yet
    EXPECT(GetHeapLargestFreeSize(HEAP_BILLBOARD) == (u32)-1);
    AddHeapRegion(HEAP_BILLBOARD, f_HeapRegion, f_HeapRegion + 64);
    AddHeapRegion(HEAP_BILLBOARD, f_HeapRegion + 128, f_HeapRegion + 256);
    EXPECT(GetHeapFreeSize(HEAP_BILLBOARD) == 64 + 128);
}

// Each allocation measures the largest block that will be free after it,
// of those large enough for it
static void TestMeasuresTheLargestFreeBlock(void) {
    StartHeap();
    AllocFromHeap(HEAP_BILLBOARD, MemorySizeFor(64));
    EXPECT(GetHeapLargestFreeSize(HEAP_BILLBOARD) == HEAP_REGION_SIZE - 64);

    StartHeap();
    AllocFromHeap(HEAP_BILLBOARD, MemorySizeFor(HEAP_REGION_SIZE - HEADER_SIZE));
    EXPECT(GetHeapLargestFreeSize(HEAP_BILLBOARD) == HEADER_SIZE);

    // Blocks too small for the request aren't measured, so a failure records
    // no free memory at all, though the region is free
    StartHeap();
    AllocFromHeap(HEAP_BILLBOARD, MemorySizeFor(HEAP_REGION_SIZE) + 1);
    EXPECT(GetHeapLargestFreeSize(HEAP_BILLBOARD) == 0);

    // Free blocks of 128 and 64, then the region's last 192 bytes: the 64
    // goes, and the 128 and 192 stay
    FreeFirstAndThird(&f_Heap, 128, 64);
    AllocFromHeap(HEAP_BILLBOARD, MemorySizeFor(64));
    EXPECT(GetHeapLargestFreeSize(HEAP_BILLBOARD) == HEAP_REGION_SIZE - 128 - 3 * 64);
}

// Blocks next to each other in the list but not in memory stay apart,
// whichever is freed first
static void TestKeepsSeparateRegionsApart(Boolean isFirstFreedFirst) {
    u32 halfSize = HEAP_REGION_SIZE / 2;
    u8 *first;
    u8 *second;

    ResetHeap(HEAP_BILLBOARD);
    AddHeapRegion(HEAP_BILLBOARD, f_HeapRegion, f_HeapRegion + halfSize - 8);
    AddHeapRegion(HEAP_BILLBOARD, f_HeapRegion + halfSize, f_HeapRegion + HEAP_REGION_SIZE);
    first = AllocFromHeap(HEAP_BILLBOARD, MemorySizeFor(halfSize - 8));
    second = AllocFromHeap(HEAP_BILLBOARD, MemorySizeFor(halfSize));
    EXPECT(first != NULL && second != NULL);
    if (isFirstFreedFirst) {
        FreeFromHeap(HEAP_BILLBOARD, &first);
        FreeFromHeap(HEAP_BILLBOARD, &second);
    }
    else {
        FreeFromHeap(HEAP_BILLBOARD, &second);
        FreeFromHeap(HEAP_BILLBOARD, &first);
    }

    EXPECT(GetHeapFreeSize(HEAP_BILLBOARD) == HEAP_REGION_SIZE - 8);
    EXPECT(AllocFromHeap(HEAP_BILLBOARD, MemorySizeFor(halfSize) + 8) == NULL);
}

// The main memory takes the Expansion Pak's memory too when there is one,
// from which only what doesn't fit in the smaller main region comes
static void TestAddsTheExpansionPak(void) {
    u8 *start = FixedAddress(MAIN_REGION_START);

    InitHeapRegions(TRUE);
    EXPECT(GetFreeMemorySize() == MAIN_REGION_SIZE + EXPANSION_SIZE);
    EXPECT(FindLargestFreeMemorySize() == EXPANSION_SIZE);
    EXPECT(AllocMemory(MemorySizeFor(64)) == start + HEADER_SIZE);
    EXPECT(AllocMemory(MemorySizeFor(MAIN_REGION_SIZE)) == FixedAddress(0x80600000) + HEADER_SIZE);

    InitHeapRegions(FALSE);
    EXPECT(GetFreeMemorySize() == MAIN_REGION_SIZE);
    EXPECT(FindLargestFreeMemorySize() == MAIN_REGION_SIZE);
}

int main(void) {
    TestAllocator(&f_Heap);
    TestAllocator(&f_MainMemory);
    TestAddsRegionsAsFreeMemory();
    TestMeasuresTheLargestFreeBlock();
    TestKeepsSeparateRegionsApart(TRUE);
    TestKeepsSeparateRegionsApart(FALSE);
    TestAddsTheExpansionPak();
    return ReportTestResults("heap_test");
}
