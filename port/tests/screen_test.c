/*
 * Tests of the conversions into the units that display lists take. The
 * cases sit on each side of zero, where the values turn negative, and at
 * the ends of the fields that take them, so that the results can be worked
 * out by hand.
 */

#include <ultra64.h>

#include "screen/screen.h"
#include "test_support.h"

// Quarter pixels are four to a pixel, either side of the screen's origin,
// as the screen wipe starts 32 pixels above it
static void TestConvertsPixelsToQuarterPixels(void) {
    EXPECT(QuarterPixels(0) == 0);
    EXPECT(QuarterPixels(1) == 4);
    EXPECT(QuarterPixels(-1) == -4);
    EXPECT(QuarterPixels(-32) == -128);
    EXPECT(QuarterPixels(1023) == 4092);
}

// Texture coordinates are 10.5 fixed point, 32 to a texel, and run from
// -1024 to 1023 texels; they stay signed, as the game divides them
static void TestConvertsTexelsToCoordinates(void) {
    EXPECT(TexelCoordinate(0) == 0);
    EXPECT(TexelCoordinate(1) == 32);
    EXPECT(TexelCoordinate(-1) == -32);
    EXPECT(TexelCoordinate(-1024) == -32768);
    EXPECT(TexelCoordinate(1023) == 32736);
    EXPECT(TexelCoordinate(-2) / 2 == -32);
}

int main(void) {
    TestConvertsPixelsToQuarterPixels();
    TestConvertsTexelsToCoordinates();
    return ReportTestResults("screen_test");
}
