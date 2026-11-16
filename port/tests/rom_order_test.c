/*
 * Tests of the registry of memory that holds data in the ROM's byte order.
 */

#include <ultra64.h>

#include "test_support.h"

static u8 f_Memory[256];

// A marked range covers its first byte to its last, and nothing beyond
static void TestMarkedRangeBounds(void) {
    osPcMarkRomOrder(&f_Memory[16], 32);
    EXPECT(!osPcIsRomOrder(&f_Memory[15]));
    EXPECT(osPcIsRomOrder(&f_Memory[16]));
    EXPECT(osPcIsRomOrder(&f_Memory[47]));
    EXPECT(!osPcIsRomOrder(&f_Memory[48]));
    osPcUnmarkRomOrder(f_Memory, sizeof(f_Memory));
    EXPECT(!osPcIsRomOrder(&f_Memory[16]));
}

// Marking a range that overlaps another drops the other's mark, as the
// game loading new data over old does
static void TestOverlapReplaces(void) {
    osPcMarkRomOrder(&f_Memory[0], 64);
    osPcMarkRomOrder(&f_Memory[32], 64);
    EXPECT(!osPcIsRomOrder(&f_Memory[0]));
    EXPECT(osPcIsRomOrder(&f_Memory[32]));
    EXPECT(osPcIsRomOrder(&f_Memory[95]));
    osPcUnmarkRomOrder(f_Memory, sizeof(f_Memory));
}

// A segment tag sits above any address and leaves the address below it
static void TestSegmentAddress(void) {
    uintptr_t tagged = (uintptr_t)osPcSegmentAddress(6, 0x1234);

    EXPECT((tagged >> PC_SEGMENT_SHIFT) == 6);
    EXPECT((tagged & (((uintptr_t)1 << PC_SEGMENT_SHIFT) - 1)) == 0x1234);
}

int main(void) {
    TestMarkedRangeBounds();
    TestOverlapReplaces();
    TestSegmentAddress();
    return ReportTestResults("rom_order_test");
}
