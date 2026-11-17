/*
 * Tests of the packing macros that GenerateGbi.cmake changes in the port's
 * gbi.h. Their inputs come from volatile variables, as constants would fold
 * the shifts before UBSan saw them.
 */

#include <ultra64.h>

#include "test_support.h"

// The blender's first cycle packs four 2-bit inputs from bit 30 down. Its top
// input of 2 or more reaches the sign bit.
static u32 PackFirstBlenderCycle(s32 m1a, s32 m1b, s32 m2a, s32 m2b) {
    volatile s32 inputs[4];

    inputs[0] = m1a;
    inputs[1] = m1b;
    inputs[2] = m2a;
    inputs[3] = m2b;
    return GBL_c1(inputs[0], inputs[1], inputs[2], inputs[3]);
}

static void TestPacksFirstBlenderCycle(void) {
    EXPECT(PackFirstBlenderCycle(0, 0, 0, 0) == 0);
    EXPECT(PackFirstBlenderCycle(1, 0, 0, 0) == 0x40000000);
    EXPECT(PackFirstBlenderCycle(2, 0, 0, 0) == 0x80000000);
    EXPECT(PackFirstBlenderCycle(3, 0, 0, 0) == 0xC0000000);
    EXPECT(PackFirstBlenderCycle(3, 2, 1, 0) == 0xC8400000);
}

int main(void) {
    TestPacksFirstBlenderCycle();
    return ReportTestResults("gbi_test");
}
