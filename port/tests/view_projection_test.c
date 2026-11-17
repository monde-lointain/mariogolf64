/*
 * Tests of ConvertMatrixToFixed, which splits each element of a float matrix
 * into the 16.16 halves of a Mtx: the integer halves of all sixteen elements
 * first, then the fractions, two elements to a word.
 */

#include <ultra64.h>

#include "camera/view_projection.h"
#include "test_support.h"

// Link seams for the routines view_projection.c calls elsewhere, which the
// conversion doesn't use
void guMtxL2F(float mf[4][4], Mtx *m) {
    (void)mf;
    (void)m;
    EXPECT(!"guMtxL2F is called");
}

void guMtxXFMF(float mf[4][4], float x, float y, float z, float *ox, float *oy, float *oz) {
    (void)mf;
    (void)x;
    (void)y;
    (void)z;
    (void)ox;
    (void)oy;
    (void)oz;
    EXPECT(!"guMtxXFMF is called");
}

#define INTEGER_WORD_COUNT 8

// The integer and fraction words of a Mtx, as the RSP reads them
static u32 IntegerWord(Mtx *fixed, s32 index) {
    return (u32)fixed->m[index / 4][index % 4];
}

static u32 FractionWord(Mtx *fixed, s32 index) {
    return IntegerWord(fixed, INTEGER_WORD_COUNT + index);
}

// Converts a matrix holding value at [row][column] and zeros elsewhere
static void ConvertOneElement(s32 row, s32 column, f32 value, Mtx *fixed) {
    Mat4 matrix = {{0}};

    matrix[row][column] = value;
    ConvertMatrixToFixed(matrix, fixed);
}

// An element in a word's high half, on either side of the fraction's sign
// bit, with a whole part, and negative, which takes the whole part's borrow
static void TestSplitsHighHalfElement(void) {
    Mtx fixed;

    ConvertOneElement(0, 0, 0x7FFF / 65536.0f, &fixed);
    EXPECT(IntegerWord(&fixed, 0) == 0);
    EXPECT(FractionWord(&fixed, 0) == 0x7FFF0000);
    ConvertOneElement(0, 0, 0.5f, &fixed);
    EXPECT(IntegerWord(&fixed, 0) == 0);
    EXPECT(FractionWord(&fixed, 0) == 0x80000000);
    ConvertOneElement(0, 0, 1.0f, &fixed);
    EXPECT(IntegerWord(&fixed, 0) == 0x00010000);
    EXPECT(FractionWord(&fixed, 0) == 0);
    ConvertOneElement(0, 0, -1 / 65536.0f, &fixed);
    EXPECT(IntegerWord(&fixed, 0) == 0xFFFF0000);
    EXPECT(FractionWord(&fixed, 0) == 0xFFFF0000);
}

// An element in a word's low half, and the last element, which ends both
// halves of the Mtx
static void TestPlacesElements(void) {
    Mtx fixed;

    ConvertOneElement(0, 1, -1 / 65536.0f, &fixed);
    EXPECT(IntegerWord(&fixed, 0) == 0x0000FFFF);
    EXPECT(FractionWord(&fixed, 0) == 0x0000FFFF);
    ConvertOneElement(3, 3, 1.5f, &fixed);
    EXPECT(IntegerWord(&fixed, 7) == 0x00000001);
    EXPECT(FractionWord(&fixed, 7) == 0x00008000);
    EXPECT(IntegerWord(&fixed, 0) == 0);
}

int main(void) {
    TestSplitsHighHalfElement();
    TestPlacesElements();
    return ReportTestResults("view_projection_test");
}
