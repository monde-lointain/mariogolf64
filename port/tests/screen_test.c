/*
 * Tests of the conversions into the units and words that display lists
 * take. The cases sit on each side of zero, where the values turn negative,
 * and at the ends of the fields that take them, so that the results can be
 * worked out by hand.
 */

#include <ultra64.h>

#include "screen/display_list.h"
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

// A vertex's screen position packs x into the high half and adds y, so a
// negative y borrows from x, as on the N64. x may be negative, or reach the
// sign bit.
static void TestPacksScreenPoints(void) {
    EXPECT((u32)ScreenPointWord(0, 0) == 0);
    EXPECT((u32)ScreenPointWord(1, 2) == 0x00010002);
    EXPECT((u32)ScreenPointWord(-1, 0) == 0xFFFF0000);
    EXPECT((u32)ScreenPointWord(1, -1) == 0x0000FFFF);
    EXPECT((u32)ScreenPointWord(0x7FFF, 0) == 0x7FFF0000);
    EXPECT((u32)ScreenPointWord(0x8000, 0) == 0x80000000);
}

// Texture coordinates pack the same way, as the shot's trail starts a texel
// before its texture
static void TestPacksTextureCoordinates(void) {
    EXPECT((u32)TextureCoordinateWord(1, 2) == 0x00010002);
    EXPECT((u32)TextureCoordinateWord(-1, 0x400) == 0xFFFF0400);
    EXPECT((u32)TextureCoordinateWord(1, -1) == 0x0000FFFF);
}

// A vertex color packs red, green, blue and alpha from the top byte down. A
// red of 0x80 or more reaches the sign bit. The channels come from volatile
// variables, as constants would fold the shifts before UBSan saw them.
static u32 PackVertexColor(s32 red, s32 green, s32 blue, s32 alpha) {
    volatile s32 channels[4];

    channels[0] = red;
    channels[1] = green;
    channels[2] = blue;
    channels[3] = alpha;
    return RgbaColor(channels[0], channels[1], channels[2], channels[3]);
}

static void TestPacksVertexColors(void) {
    EXPECT(PackVertexColor(0, 0, 0, 0) == 0);
    EXPECT(PackVertexColor(0x12, 0x34, 0x56, 0x78) == 0x12345678);
    EXPECT(PackVertexColor(0x7F, 0, 0, 0) == 0x7F000000);
    EXPECT(PackVertexColor(0x80, 0, 0, 0) == 0x80000000);
    EXPECT(PackVertexColor(0xFF, 0xFF, 0xFF, 0xFF) == 0xFFFFFFFF);
}

int main(void) {
    TestConvertsPixelsToQuarterPixels();
    TestConvertsTexelsToCoordinates();
    TestPacksScreenPoints();
    TestPacksTextureCoordinates();
    TestPacksVertexColors();
    return ReportTestResults("screen_test");
}
