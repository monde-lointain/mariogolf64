/*
 * Characterizes how fireworks.c loads the fireworks of sudden death: it
 * reads the course's shape from the asset archive in ROM, and gives each
 * firework a particle for each zero cell of the shape. What the loading
 * takes of the main memory measures the shape's decoded size and its
 * particle count.
 */

#include <ultra64.h>

#include "round/fireworks.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/heap.h"
#include "unity_fixture.h"

// The shapes, one for each course record index, and what loading each
// takes of the main memory
#define SHAPE_COUNT 8
static const u32 f_LoadedSizes[SHAPE_COUNT] = {
    0x11438,
    0x11A28,
    0x10F78,
    0x149A8,
    0x111D8,
    0x18508,
    0x12B60,
    0xEF68,
};

TEST_GROUP(Fireworks);

TEST_SETUP(Fireworks) {
    InitHeapRegions(FALSE);
    InitAssetFiles();
}

TEST_TEAR_DOWN(Fireworks) {
    FreeFireworks();
}

// What loading the fireworks for courseRecordIndex takes of the main memory
static u32 LoadedSize(CourseRecordIndex courseRecordIndex) {
    u32 freeSize = GetFreeMemorySize();

    LoadFireworks(courseRecordIndex);
    return freeSize - GetFreeMemorySize();
}

TEST(Fireworks, EachShapeTakesItsSize) {
    u32 loadedSizes[SHAPE_COUNT];
    s32 i;

    for (i = 0; i < SHAPE_COUNT; i++) {
        loadedSizes[i] = LoadedSize(i);
        FreeFireworks();
    }

    TEST_ASSERT_EQUAL_HEX32_ARRAY(f_LoadedSizes, loadedSizes, SHAPE_COUNT);
}

// Course record indices past the shapes wrap around to the first
TEST(Fireworks, IndicesWrapAroundTheShapes) {
    TEST_ASSERT_EQUAL_HEX32(f_LoadedSizes[0], LoadedSize(SHAPE_COUNT));
}

TEST(Fireworks, FreeingGivesBackAllThatLoadingTook) {
    (void)LoadedSize(0);
    FreeFireworks();
    TEST_ASSERT_EQUAL_HEX32(MAIN_REGION_SIZE, GetFreeMemorySize());
}

TEST_GROUP_RUNNER(Fireworks) {
    RUN_TEST_CASE(Fireworks, EachShapeTakesItsSize);
    RUN_TEST_CASE(Fireworks, IndicesWrapAroundTheShapes);
    RUN_TEST_CASE(Fireworks, FreeingGivesBackAllThatLoadingTook);
}
