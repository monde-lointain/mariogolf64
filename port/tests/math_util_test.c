/*
 * Tests of the game's random generator, the seeds it steps through, and of
 * the conversion of angles to stored binary angles, worked out by hand.
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

// Converts an angle that the compiler can't know, as the game's are worked
// out at run time
static SignedBinaryAngle ConvertToSignedBinaryAngle(f64 angle) {
    volatile f64 value = angle;

    return TruncateToSignedBinaryAngle(value);
}

// Angles within a short's range truncate toward zero, on each side of it,
// up to the last that fits
static void TestTruncatesAnglesThatFit(void) {
    EXPECT(ConvertToSignedBinaryAngle(1000.9) == 1000);
    EXPECT(ConvertToSignedBinaryAngle(-1000.9) == -1000);
    EXPECT(ConvertToSignedBinaryAngle(32767.9) == 32767);
    EXPECT(ConvertToSignedBinaryAngle(-32768.0) == -32768);
}

// The staff roll's cast headings in session 16 go past a half turn: KMC
// GCC's code truncates them to a word and stores its low half, so that
// -43690.7 gives -43690, 0xFFFF5556, and 21846; -65536 a whole turn, 0;
// and -87381.3 gives -87381, 0xFFFEAAAB, and -21845. A half turn wraps to
// minus a half turn.
static void TestWrapsAnglesPastAHalfTurn(void) {
    EXPECT(ConvertToSignedBinaryAngle(-43690.7) == 21846);
    EXPECT(ConvertToSignedBinaryAngle(-65536.0) == 0);
    EXPECT(ConvertToSignedBinaryAngle(-87381.3) == -21845);
    EXPECT(ConvertToSignedBinaryAngle(32768.0) == -32768);
}

int main(void) {
    TestTruncatesAnglesThatFit();
    TestWrapsAnglesPastAHalfTurn();
    TestStepsSeedFrom(0, 1);
    TestStepsSeedFrom(1, 0x5D588B66);
    TestStepsSeedFrom(2, 0xBAB116CB);
    TestStepsSeedFrom(-1, 0xA2A7749C);
    TestStepsSeedFrom(-2, 0x454EE937);
    return ReportTestResults("math_util_test");
}
