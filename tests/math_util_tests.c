/*
 * Characterizes the integer helpers of math_util.c: the CRC that guards
 * saved data, and the lengths of integer vectors.
 */

#include <ultra64.h>

#include "math/math_util.h"
#include "unity_fixture.h"

// CRC-16/X-25's check value, its CRC of these nine digits
#define CHECK_DIGITS "123456789"
#define CHECK_DIGITS_CRC 0x906E

TEST_GROUP(MathUtil);

TEST_SETUP(MathUtil) {
}

TEST_TEAR_DOWN(MathUtil) {
}

TEST(MathUtil, Crc16OfNothingIsZero) {
    u8 data[1] = {0};

    TEST_ASSERT_EQUAL_HEX16(0x0000, ComputeCrc16(data, 0));
}

TEST(MathUtil, Crc16IsX25) {
    u8 digits[] = CHECK_DIGITS;

    TEST_ASSERT_EQUAL_HEX16(CHECK_DIGITS_CRC, ComputeCrc16(digits, sizeof(digits) - 1));
}

TEST(MathUtil, IntLength2DOfSmallComponents) {
    TEST_ASSERT_EQUAL_UINT32(5, IntLength2D(3, -4));
}

TEST(MathUtil, IntLength2DHalvesLargeComponents) {
    TEST_ASSERT_EQUAL_UINT32(5 << 28, IntLength2D(3 << 28, 4 << 28));
}

TEST(MathUtil, IntLength3DOfSmallComponents) {
    TEST_ASSERT_EQUAL_UINT32(7, IntLength3D(2, -3, 6));
}

TEST_GROUP_RUNNER(MathUtil) {
    RUN_TEST_CASE(MathUtil, Crc16OfNothingIsZero);
    RUN_TEST_CASE(MathUtil, Crc16IsX25);
    RUN_TEST_CASE(MathUtil, IntLength2DOfSmallComponents);
    RUN_TEST_CASE(MathUtil, IntLength2DHalvesLargeComponents);
    RUN_TEST_CASE(MathUtil, IntLength3DOfSmallComponents);
}
