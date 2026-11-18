/*
 * Tests of the port's sinf and cosf: the nearest float to the true value at
 * the edges of each way of reaching it, libultra's answers past them, and
 * the same bits for a sweep of angles on every host.
 *
 * The expected values are the host's double sine and cosine rounded once to
 * float, as Python's math module gave them on Linux. A 60-digit sum of the
 * Taylor series gave the same for the cosines near 2^-12 and the angles
 * that glibc rounds wrongly.
 */

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "port/gu_sine.h"
#include "test_support.h"

// Angles from 0 to 2^28 by this step in their bits
#define SWEEP_STEP 0x10001u
#define SWEEP_END 0x4D800000u
// FNV-1a's, over each result's bits
#define HASH_START 2166136261u
#define HASH_FACTOR 16777619u
// The sweep's hash, from Python's results by the same steps
#define SWEEP_HASH 0xE7A88AD9u

static uint32_t BitsOf(float value) {
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float FloatOf(uint32_t bits) {
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

// The same bits, so that -0 isn't taken for 0
#define EXPECT_BITS(actual, expected) EXPECT(BitsOf(actual) == BitsOf(expected))

// An angle below 2^-12 is its own sine, keeping its sign
static void TestSmallAngleIsItsOwnSine(void) {
    EXPECT_BITS(GuSine(0.0f), 0.0f);
    EXPECT_BITS(GuSine(-0.0f), -0.0f);
    EXPECT_BITS(GuSine(0x1p-149f), 0x1p-149f);
    EXPECT_BITS(GuSine(0x1.fffffep-13f), 0x1.fffffep-13f);
    EXPECT_BITS(GuSine(0x1.000004p-12f), 0x1.000004p-12f);
}

// Each side of 1.5, where libultra started to reduce the angle, and of
// pi/2, where a half turn is first taken off, gives the nearest
static void TestSineAroundReduction(void) {
    EXPECT_BITS(GuSine(1.0f), 0x1.aed548p-1f);
    EXPECT_BITS(GuSine(0x1.7ffffep+0f), 0x1.feb7aap-1f);
    EXPECT_BITS(GuSine(1.5f), 0x1.feb7aap-1f);
    EXPECT_BITS(GuSine(0x1.921fb4p+0f), 1.0f);
    EXPECT_BITS(GuSine(0x1.921fb6p+0f), 1.0f);
    EXPECT_BITS(GuSine(3.0f), 0x1.210386p-3f);
    EXPECT_BITS(GuSine(-3.0f), -0x1.210386p-3f);
}

// Near a multiple of pi, where the sine is small, every bit of pi counts
static void TestSineNearPi(void) {
    EXPECT_BITS(GuSine(0x1.921fb6p+1f), -0x1.777a5cp-24f);
    EXPECT_BITS(GuSine(-0x1.921fb6p+1f), 0x1.777a5cp-24f);
}

// Large angles, up to the last below 2^28
static void TestSineOfLargeAngles(void) {
    EXPECT_BITS(GuSine(100.0f), -0x1.03425cp-1f);
    EXPECT_BITS(GuSine(1000000.0f), -0x1.6664b2p-2f);
    EXPECT_BITS(GuSine(0x1.fffffep+27f), 0x1.cb25fep-1f);
}

// From 2^28 on, and for infinity, both give 0; NaN gives NaN
static void TestOutOfRangeAngles(void) {
    EXPECT_BITS(GuSine(0x1p28f), 0.0f);
    EXPECT_BITS(GuSine(-0x1p28f), 0.0f);
    EXPECT_BITS(GuSine(INFINITY), 0.0f);
    EXPECT_BITS(GuCosine(0x1p28f), 0.0f);
    EXPECT_BITS(GuCosine(-INFINITY), 0.0f);
    EXPECT(isnan(GuSine(NAN)));
    EXPECT(isnan(GuCosine(NAN)));
}

// Small angles' cosines round to 1 until 1 is no longer the nearest. Just
// past 2^-12 the true value lies close below the halfway point, where a
// series of fewer terms would round up to 1.
static void TestCosineOfSmallAngles(void) {
    EXPECT_BITS(GuCosine(0.0f), 1.0f);
    EXPECT_BITS(GuCosine(-0.0f), 1.0f);
    EXPECT_BITS(GuCosine(0x1p-12f), 1.0f);
    EXPECT_BITS(GuCosine(0x1.000004p-12f), 0x1.fffffep-1f);
    EXPECT_BITS(GuCosine(-0x1.000004p-12f), 0x1.fffffep-1f);
    // Two of the four below 11 that the series to x^19 would get wrong
    EXPECT_BITS(GuCosine(0x1.a8872ap-11f), 0x1.fffff6p-1f);
    EXPECT_BITS(GuCosine(0x1.20ffccp-7f), 0x1.fffae8p-1f);
}

// Near an odd multiple of pi/2, where the cosine is small, and elsewhere
static void TestCosineOfOtherAngles(void) {
    EXPECT_BITS(GuCosine(0x1.921fb6p+0f), -0x1.777a5cp-25f);
    EXPECT_BITS(GuCosine(-0x1.921fb6p+0f), -0x1.777a5cp-25f);
    EXPECT_BITS(GuCosine(1.0f), 0x1.14a28p-1f);
    EXPECT_BITS(GuCosine(0x1.7ffffep+0f), 0x1.21bd74p-4f);
    EXPECT_BITS(GuCosine(1.5f), 0x1.21bd54p-4f);
    EXPECT_BITS(GuCosine(3.0f), -0x1.fae04cp-1f);
    EXPECT_BITS(GuCosine(0x1.921fb6p+1f), -1.0f);
    EXPECT_BITS(GuCosine(100.0f), 0x1.b981dcp-1f);
    EXPECT_BITS(GuCosine(1000000.0f), 0x1.df9dfap-1f);
    EXPECT_BITS(GuCosine(0x1.fffffep+27f), 0x1.c51bbp-2f);
}

// The first angles from 1 up where glibc's sinf and cosf give the float
// next to the nearest, as a 60-digit sum of the series confirms
static void TestAnglesThatGlibcRoundsWrongly(void) {
    EXPECT_BITS(GuSine(0x1.000064p+0f), 0x1.aed5b4p-1f);
    EXPECT_BITS(GuSine(0x1.001086p+0f), 0x1.aee722p-1f);
    EXPECT_BITS(GuCosine(0x1.000026p+0f), 0x1.14a242p-1f);
    EXPECT_BITS(GuCosine(0x1.000078p+0f), 0x1.14a1b8p-1f);
}

// Every bit of both, for angles of each sign spread over the whole range,
// is the same as Python's
static void TestSweepGivesSameBits(void) {
    uint32_t hash = HASH_START;
    uint32_t bits;
    uint32_t sign;

    for (bits = 0; bits < SWEEP_END; bits += SWEEP_STEP) {
        for (sign = 0; sign <= 1; sign++) {
            float angle = FloatOf(bits | (sign << 31));

            hash = (hash ^ BitsOf(GuSine(angle))) * HASH_FACTOR;
            hash = (hash ^ BitsOf(GuCosine(angle))) * HASH_FACTOR;
        }
    }

    EXPECT(hash == SWEEP_HASH);
}

int main(void) {
    TestSmallAngleIsItsOwnSine();
    TestSineAroundReduction();
    TestSineNearPi();
    TestSineOfLargeAngles();
    TestOutOfRangeAngles();
    TestCosineOfSmallAngles();
    TestCosineOfOtherAngles();
    TestAnglesThatGlibcRoundsWrongly();
    TestSweepGivesSameBits();
    return ReportTestResults("gu_sine_test");
}
