/*
 * Tests of TruncateToU32, which converts a float to an unsigned word as
 * KMC GCC does on the N64: values below 2^31 truncate as signed words, and
 * those from 2^31 up lose 2^31 first and set the top bit after.
 */

#include <ultra64.h>

#include "math/float_to_unsigned.h"
#include "system/c_language.h"
#include "test_support.h"

static u32 Truncate(f32 value) {
    volatile f32 volatileValue = value;

    return TruncateToU32(volatileValue);
}

// Fractions truncate toward zero, either side of it
static void TestTruncatesTowardZero(void) {
    EXPECT(Truncate(0.0f) == 0);
    EXPECT(Truncate(0.75f) == 0);
    EXPECT(Truncate(-0.75f) == 0);
    EXPECT(Truncate(1.0f) == 1);
}

// Negative values keep their signed word's bits, as the ball depths do
static void TestKeepsNegativeValuesBits(void) {
    EXPECT(Truncate(-1.0f) == 0xFFFFFFFF);
    EXPECT(Truncate(-2457.82f) == 0xFFFFF667);
}

// The largest float below 2^31 is still a signed word; 2^31 itself, and the
// largest float below 2^32, take the top bit
static void TestSetsTheTopBitFromHalfTheRange(void) {
    EXPECT(Truncate(2147483520.0f) == 0x7FFFFF80);
    EXPECT(Truncate(2147483648.0f) == 0x80000000);
    EXPECT(Truncate(2147483904.0f) == 0x80000100);
    EXPECT(Truncate(4294967040.0f) == 0xFFFFFF00);
}

int main(void) {
    TestTruncatesTowardZero();
    TestKeepsNegativeValuesBits();
    TestSetsTheTopBitFromHalfTheRange();
    return ReportTestResults("float_to_unsigned_test");
}
