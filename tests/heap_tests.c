/*
 * Characterizes heap.c's allocator: the heaps that the game gives regions
 * of spare memory, and the main memory's pool. Each block follows a header,
 * and its size, with the header, is rounded up to 8 bytes. A request takes
 * the smallest free block that fits, which is split if more than a header
 * would be left over. A freed block merges with its free neighbors.
 */

#include <ultra64.h>

#include "system/c_language.h"
#include "system/heap.h"
#include "unity_fixture.h"

// The heap that the tests use; the game gives it the billboards' memory
#define TEST_HEAP HEAP_BILLBOARD

#define REGION_SIZE 0x1000
// The header that comes before each block's memory
#define HEADER_SIZE 0x10

// A request small enough that several fit in the region, and what it takes
// with its header
#define SMALL_SIZE 100
#define SMALL_BLOCK_SIZE 0x78

// The memory that the heap divides
static u8 f_Region[REGION_SIZE] __attribute__((aligned(16)));

// The offset of a block's memory in the region
static u32 OffsetInRegion(void *block) {
    return (u32)block - (u32)f_Region;
}

TEST_GROUP(Heap);

TEST_SETUP(Heap) {
    ResetHeap(TEST_HEAP);
    AddHeapRegion(TEST_HEAP, f_Region, f_Region + REGION_SIZE);
}

TEST_TEAR_DOWN(Heap) {
}

TEST(Heap, NewRegionIsAllFree) {
    TEST_ASSERT_EQUAL_HEX32(REGION_SIZE, GetHeapFreeSize(TEST_HEAP));
}

TEST(Heap, FirstBlockFollowsItsHeader) {
    TEST_ASSERT_EQUAL_HEX32(HEADER_SIZE, OffsetInRegion(AllocFromHeap(TEST_HEAP, SMALL_SIZE)));
}

// Each request takes its size with the header, rounded up to 8 bytes
TEST(Heap, RequestTakesItsHeaderRoundedToEightBytes) {
    static const u32 requests[] = {0, 1, 8, 9, 16, 17, SMALL_SIZE};
    static const u32 taken[] = {0x10, 0x18, 0x18, 0x20, 0x20, 0x28, SMALL_BLOCK_SIZE};
    u32 freeSizes[ARRAY_COUNT(requests)];
    u32 expected[ARRAY_COUNT(requests)];
    s32 i;

    for (i = 0; i < ARRAY_COUNT(requests); i++) {
        ResetHeap(TEST_HEAP);
        AddHeapRegion(TEST_HEAP, f_Region, f_Region + REGION_SIZE);
        (void)AllocFromHeap(TEST_HEAP, requests[i]);
        freeSizes[i] = GetHeapFreeSize(TEST_HEAP);
        expected[i] = REGION_SIZE - taken[i];
    }

    TEST_ASSERT_EQUAL_HEX32_ARRAY(expected, freeSizes, ARRAY_COUNT(requests));
}

TEST(Heap, ConsecutiveBlocksAreAdjacent) {
    void *first = AllocFromHeap(TEST_HEAP, SMALL_SIZE);
    void *second = AllocFromHeap(TEST_HEAP, SMALL_SIZE);

    TEST_ASSERT_EQUAL_HEX32(OffsetInRegion(first) + SMALL_BLOCK_SIZE, OffsetInRegion(second));
}

// A block is split only if more than a header would be left over;
// otherwise the request takes all of it
TEST(Heap, BlockIsSplitOnlyIfMoreThanAHeaderIsLeft) {
    (void)AllocFromHeap(TEST_HEAP, REGION_SIZE - (2 * HEADER_SIZE) - 8);
    TEST_ASSERT_EQUAL_HEX32(HEADER_SIZE + 8, GetHeapFreeSize(TEST_HEAP));

    ResetHeap(TEST_HEAP);
    AddHeapRegion(TEST_HEAP, f_Region, f_Region + REGION_SIZE);
    (void)AllocFromHeap(TEST_HEAP, REGION_SIZE - (2 * HEADER_SIZE));
    TEST_ASSERT_EQUAL_HEX32(0, GetHeapFreeSize(TEST_HEAP));
}

TEST(Heap, RequestLargerThanAnyBlockFails) {
    TEST_ASSERT_NULL(AllocFromHeap(TEST_HEAP, REGION_SIZE));
    TEST_ASSERT_EQUAL_HEX32(REGION_SIZE, GetHeapFreeSize(TEST_HEAP));
}

TEST(Heap, FreeingClearsThePointerAndRestoresTheSize) {
    void *block = AllocFromHeap(TEST_HEAP, SMALL_SIZE);

    FreeFromHeap(TEST_HEAP, &block);
    TEST_ASSERT_NULL(block);
    TEST_ASSERT_EQUAL_HEX32(REGION_SIZE, GetHeapFreeSize(TEST_HEAP));
}

TEST(Heap, FreeingNullDoesNothing) {
    void *block = NULL;

    FreeFromHeap(TEST_HEAP, &block);
    TEST_ASSERT_EQUAL_HEX32(REGION_SIZE, GetHeapFreeSize(TEST_HEAP));
}

// Freed in the order first, third, second, the blocks merge back into one
// that the whole region's request fits
TEST(Heap, FreedNeighborsMerge) {
    void *first = AllocFromHeap(TEST_HEAP, 0x200);
    void *second = AllocFromHeap(TEST_HEAP, 0x200);
    void *third = AllocFromHeap(TEST_HEAP, 0x200);

    FreeFromHeap(TEST_HEAP, &first);
    FreeFromHeap(TEST_HEAP, &third);
    FreeFromHeap(TEST_HEAP, &second);
    TEST_ASSERT_EQUAL_HEX32(
        HEADER_SIZE,
        OffsetInRegion(AllocFromHeap(TEST_HEAP, REGION_SIZE - HEADER_SIZE))
    );
}

// Of two free blocks that fit, the smaller is taken, though the larger
// comes first
TEST(Heap, RequestTakesTheBestFit) {
    void *large = AllocFromHeap(TEST_HEAP, 0x400);
    void *small;
    void *smallBlock;

    (void)AllocFromHeap(TEST_HEAP, SMALL_SIZE);
    small = AllocFromHeap(TEST_HEAP, SMALL_SIZE);
    smallBlock = small;
    (void)AllocFromHeap(TEST_HEAP, SMALL_SIZE);
    FreeFromHeap(TEST_HEAP, &large);
    FreeFromHeap(TEST_HEAP, &small);
    TEST_ASSERT_EQUAL_PTR(smallBlock, AllocFromHeap(TEST_HEAP, SMALL_SIZE));
}

// The largest free size is what the last request left in the blocks that
// could hold it. Freeing doesn't measure it again.
TEST(Heap, LargestFreeSizeIsWhatTheLastRequestLeft) {
    void *block = AllocFromHeap(TEST_HEAP, SMALL_SIZE);

    TEST_ASSERT_EQUAL_HEX32(REGION_SIZE - SMALL_BLOCK_SIZE, GetHeapLargestFreeSize(TEST_HEAP));
    FreeFromHeap(TEST_HEAP, &block);
    TEST_ASSERT_EQUAL_HEX32(REGION_SIZE - SMALL_BLOCK_SIZE, GetHeapLargestFreeSize(TEST_HEAP));
}

// A request that fails finds no block that could hold it
TEST(Heap, FailedRequestLeavesNoLargestFreeSize) {
    (void)AllocFromHeap(TEST_HEAP, REGION_SIZE);
    TEST_ASSERT_EQUAL_HEX32(0, GetHeapLargestFreeSize(TEST_HEAP));
}

TEST(Heap, MainMemoryIsTheMainRegion) {
    InitHeapRegions(FALSE);
    TEST_ASSERT_EQUAL_HEX32(MAIN_REGION_SIZE, GetFreeMemorySize());
    TEST_ASSERT_EQUAL_HEX32(MAIN_REGION_SIZE, FindLargestFreeMemorySize());
    TEST_ASSERT_EQUAL_HEX32(MAIN_REGION_START + HEADER_SIZE, (u32)AllocMemory(SMALL_SIZE));
    TEST_ASSERT_EQUAL_HEX32(MAIN_REGION_SIZE - SMALL_BLOCK_SIZE, GetFreeMemorySize());
}

// The Expansion Pak adds 2 MB from 0x80600000, apart from the main region
TEST(Heap, ExpansionPakAddsARegionOfItsOwn) {
    InitHeapRegions(TRUE);
    TEST_ASSERT_EQUAL_HEX32(MAIN_REGION_SIZE + 0x200000, GetFreeMemorySize());
    TEST_ASSERT_EQUAL_HEX32(0x200000, FindLargestFreeMemorySize());
}

TEST(Heap, FreeingMainMemoryClearsThePointerAndRestoresTheSize) {
    void *block;

    InitHeapRegions(FALSE);
    block = AllocMemory(SMALL_SIZE);
    FreeMemory(&block);
    TEST_ASSERT_NULL(block);
    TEST_ASSERT_EQUAL_HEX32(MAIN_REGION_SIZE, GetFreeMemorySize());
}

TEST_GROUP_RUNNER(Heap) {
    RUN_TEST_CASE(Heap, NewRegionIsAllFree);
    RUN_TEST_CASE(Heap, FirstBlockFollowsItsHeader);
    RUN_TEST_CASE(Heap, RequestTakesItsHeaderRoundedToEightBytes);
    RUN_TEST_CASE(Heap, ConsecutiveBlocksAreAdjacent);
    RUN_TEST_CASE(Heap, BlockIsSplitOnlyIfMoreThanAHeaderIsLeft);
    RUN_TEST_CASE(Heap, RequestLargerThanAnyBlockFails);
    RUN_TEST_CASE(Heap, FreeingClearsThePointerAndRestoresTheSize);
    RUN_TEST_CASE(Heap, FreeingNullDoesNothing);
    RUN_TEST_CASE(Heap, FreedNeighborsMerge);
    RUN_TEST_CASE(Heap, RequestTakesTheBestFit);
    RUN_TEST_CASE(Heap, LargestFreeSizeIsWhatTheLastRequestLeft);
    RUN_TEST_CASE(Heap, FailedRequestLeavesNoLargestFreeSize);
    RUN_TEST_CASE(Heap, MainMemoryIsTheMainRegion);
    RUN_TEST_CASE(Heap, ExpansionPakAddsARegionOfItsOwn);
    RUN_TEST_CASE(Heap, FreeingMainMemoryClearsThePointerAndRestoresTheSize);
}
