/*
 * Tests the port's KMC trigonometry (port/src/kmc/math.c) to the last bit,
 * against results from an exact-integer model of the N64's routines: 52
 * CORDIC rotations in fixed point with 60 fractional bits, converted back
 * by truncating to 53 bits. Each differs from the host's libm in its low
 * bits, so a stray call to libm fails too.
 */

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "port/kmc.h"
#include "test_support.h"

#define EXPECT_BITS(actual, expected) EXPECT(BitsOf(actual) == (expected))

static uint64_t BitsOf(double value) {
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void TestSineOfNominalAngles(void) {
    EXPECT_BITS(KmcSine(0.5), UINT64_C(0x3FDEAEE8744B05F2));
    EXPECT_BITS(KmcSine(-0.5), UINT64_C(0xBFDEAEE8744B05F2));
    EXPECT_BITS(KmcSine(1.0), UINT64_C(0x3FEAED548F090CEB));
    // A quarter turn falls just short of 1
    EXPECT_BITS(KmcSine(1.5707963267948966), UINT64_C(0x3FEFFFFFFFFFFFFF));
}

// Whole half turns are taken off first, flipping the sign when odd
static void TestSineOfReducedAngles(void) {
    EXPECT_BITS(KmcSine(2.0), UINT64_C(0x3FED18F6EAD1B446));
    EXPECT_BITS(KmcSine(-3.0), UINT64_C(0xBFC210386DB6D55D));
    EXPECT_BITS(KmcSine(10.0), UINT64_C(0xBFE1689EF5F34F52));
    EXPECT_BITS(KmcSine(100.0), UINT64_C(0xBFE03425B78C4D98));
}

// Closer to 0 than 1e-8, the sine is the angle; from 1e-8 on, CORDIC's
static void TestSineNearZero(void) {
    EXPECT(KmcSine(1e-9) == 1e-9);
    EXPECT(KmcSine(-1e-9) == -1e-9);
    EXPECT(KmcSine(9.999999999999999e-9) == 9.999999999999999e-9);
    EXPECT_BITS(KmcSine(1e-8), UINT64_C(0x3E45798ED6280000));
    EXPECT_BITS(KmcSine(-1e-8), UINT64_C(0xBE45798ED6280000));
    EXPECT_BITS(KmcSine(2e-8), UINT64_C(0x3E55798EDAFC0000));
}

static void TestCosine(void) {
    // The gain's truncation leaves the cosine of 0 just short of 1
    EXPECT_BITS(KmcCosine(0.0), UINT64_C(0x3FEFFFFFFFFFFFFF));
    EXPECT_BITS(KmcCosine(0.5), UINT64_C(0x3FEC1528065B7D4E));
    EXPECT_BITS(KmcCosine(-0.5), UINT64_C(0x3FEC1528065B7D4E));
    EXPECT_BITS(KmcCosine(1.0), UINT64_C(0x3FE14A280FB5068E));
    EXPECT_BITS(KmcCosine(2.0), UINT64_C(0xBFDAA226575371FE));
    EXPECT_BITS(KmcCosine(-3.0), UINT64_C(0xBFEFAE04BE85E5D1));
    EXPECT_BITS(KmcCosine(1.5707963267948966), UINT64_C(0x3CB3F00000000000));
    EXPECT_BITS(KmcCosine(3.141592653589793), UINT64_C(0xBFEFFFFFFFFFFFFF));
}

static void TestTangent(void) {
    EXPECT_BITS(KmcTangent(0.5), UINT64_C(0x3FE17B4F5BF3474C));
    EXPECT_BITS(KmcTangent(-1.0), UINT64_C(0xBFF8EB245CBEE39F));
    EXPECT_BITS(KmcTangent(1.2), UINT64_C(0x400493C43ACB1651));
    // A half turn on, the tangent's sign doesn't flip
    EXPECT_BITS(KmcTangent(3.0), UINT64_C(0xBFC23EF71254B872));
    EXPECT(KmcTangent(1e-9) == 1e-9);
}

// Below 1 the vector is (1, value); from 1 on, (1 / value, 1)
static void TestArcTangent(void) {
    EXPECT_BITS(KmcArcTangent(0.5), UINT64_C(0x3FDDAC670561BB50));
    EXPECT_BITS(KmcArcTangent(-0.5), UINT64_C(0xBFDDAC670561BB50));
    EXPECT_BITS(KmcArcTangent(0.999999), UINT64_C(0x3FE921FA47D4AA3F));
    EXPECT_BITS(KmcArcTangent(1.0), UINT64_C(0x3FE921FB54442D1B));
    EXPECT_BITS(KmcArcTangent(2.0), UINT64_C(0x3FF1B6E192EBBE44));
    EXPECT_BITS(KmcArcTangent(-100.0), UINT64_C(0xBFF8F905EB2DEF22));
}

// Up to 1e-7 from 0, the arc tangent is the value; past it, CORDIC's
static void TestArcTangentNearZero(void) {
    EXPECT(KmcArcTangent(1e-8) == 1e-8);
    EXPECT(KmcArcTangent(-1e-8) == -1e-8);
    EXPECT(KmcArcTangent(5e-8) == 5e-8);
    EXPECT(KmcArcTangent(1e-7) == 1e-7);
    EXPECT(KmcArcTangent(-1e-7) == -1e-7);
    EXPECT_BITS(KmcArcTangent(1.0000000000000002e-7), UINT64_C(0x3E7AD7F29AC60000));
    EXPECT_BITS(KmcArcTangent(2e-7), UINT64_C(0x3E8AD7F29B620000));
}

static void TestArcTangent2InEachQuadrant(void) {
    EXPECT_BITS(KmcArcTangent2(1.0, 2.0), UINT64_C(0x3FDDAC670561BB50));
    EXPECT_BITS(KmcArcTangent2(1.0, -2.0), UINT64_C(0x40056C6E7397F5AE));
    EXPECT_BITS(KmcArcTangent2(-1.0, -2.0), UINT64_C(0xC0056C6E7397F5AE));
    EXPECT_BITS(KmcArcTangent2(-1.0, 2.0), UINT64_C(0xBFDDAC670561BB50));
}

static void TestArcTangent2OnAxes(void) {
    EXPECT(KmcArcTangent2(0.0, 1.0) == 0.0);
    EXPECT(KmcArcTangent2(0.0, -1.0) == 3.141592653589793);
    EXPECT(KmcArcTangent2(2.0, 0.0) == 3.141592653589793 / 2);
    EXPECT(KmcArcTangent2(-2.0, 0.0) == -3.141592653589793 / 2);
    // The N64 reports an error here, and gives 0
    EXPECT_BITS(KmcArcTangent2(0.0, 0.0), UINT64_C(0));
}

// Angles that the N64 couldn't reduce, where it raised an exception
static void TestRejectsUnreducibleAngles(void) {
    EXPECT(isnan(KmcSine(NAN)));
    EXPECT(isnan(KmcCosine(INFINITY)));
    EXPECT(isnan(KmcTangent(-INFINITY)));
    EXPECT(isnan(KmcSine(1e10)));
    // The largest reducible angles still give a result
    EXPECT(!isnan(KmcSine(6.7e9)));
    EXPECT(!isnan(KmcCosine(-6.7e9)));
}

int main(void) {
    TestSineOfNominalAngles();
    TestSineOfReducedAngles();
    TestSineNearZero();
    TestCosine();
    TestTangent();
    TestArcTangent();
    TestArcTangentNearZero();
    TestArcTangent2InEachQuadrant();
    TestArcTangent2OnAxes();
    TestRejectsUnreducibleAngles();
    return ReportTestResults("kmc_math_test");
}
