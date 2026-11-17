/*
 * Tests of the game's random generator: the seeds it steps through,
 * worked out by hand from its linear congruential step.
 */

#include <ultra64.h>

#include "math/math_util.h"
#include "test_support.h"

// A link seam for the libultra routine that math_util.c logs through
void osSyncPrintf(const char *format, ...) {
    (void)format;
}

// Each step multiplies the seed by RANDOM_MULTIPLIER, 0x5D588B65, and adds 1,
// keeping the low 32 bits as the N64's multiply did. The seeds sit on each
// side of the product's overflow, where the results can be worked out by
// hand: 0 gives the increment alone, 1 and -1 products that fit, and 2 and -2
// the first that overflow, twice the multiplier and 2^32 less that.
static void TestStepsSeedFrom(s32 seed, u32 expected) {
    SetRandomSeed(seed);
    EXPECT((u32)NextRandom() == expected);
}

int main(void) {
    TestStepsSeedFrom(0, 1);
    TestStepsSeedFrom(1, 0x5D588B66);
    TestStepsSeedFrom(2, 0xBAB116CB);
    TestStepsSeedFrom(-1, 0xA2A7749C);
    TestStepsSeedFrom(-2, 0x454EE937);
    return ReportTestResults("math_util_test");
}
