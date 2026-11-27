/*
 * Tests of the conversion from world units to the course's fixed point,
 * 1024 course units to a world unit, worked out by hand.
 */

#include <ultra64.h>

#include "course/course_space.h"
#include "test_support.h"

// Converts a world coordinate that the compiler can't know, as the game's
// come from the hole's data
static s32 ConvertWorldToCourse(s16 world) {
    volatile s16 value = world;

    return WorldToCourse(value);
}

// A world coordinate keeps its sign: zero stays zero, and the nearest
// values on each side of it, and the farthest that the s16 coordinates of
// vertices and billboards reach, scale by 1024. Course X and Z, and the
// ground's heights, are as often negative as positive.
static void TestScalesBothSigns(void) {
    EXPECT(ConvertWorldToCourse(0) == 0);
    EXPECT(ConvertWorldToCourse(1) == 1024);
    EXPECT(ConvertWorldToCourse(-1) == -1024);
    EXPECT(ConvertWorldToCourse(32767) == 33553408); // 32768 * 1024 - 1024
    EXPECT(ConvertWorldToCourse(-32768) == -33554432); // -(1 << 25)
}

// A billboard's X in session 03, which a character's reaction spot is
// measured from
static void TestScalesABillboardLeftOfZero(void) {
    EXPECT(ConvertWorldToCourse(-2985) == -3056640);
}

// A short coordinate gives a signed int, as the shift that the original
// makes does, so that differences of course units stay signed
static void TestGivesASignedInt(void) {
    volatile s16 value = -1;

    EXPECT(sizeof(WorldToCourse(value)) == sizeof(s32));
    EXPECT(WorldToCourse(value) - 1 < 0);
}

// Works out an edge's cross product with operands that the compiler can't
// know, as the game's come from the hole's triangles and the ball's path
static s32 CrossEdge(s32 edgeU, s32 edgeV, s32 offsetU, s32 offsetV) {
    volatile s32 values[4];

    values[0] = edgeU;
    values[1] = edgeV;
    values[2] = offsetU;
    values[3] = offsetV;
    return EdgeCross(values[0], values[1], values[2], values[3]);
}

// Products that fit a word give the cross product itself, with its sign:
// which side of the edge the point is on, or zero on it
static void TestCrossesSmallEdges(void) {
    EXPECT(CrossEdge(3, 0, 0, 5) == 15);
    EXPECT(CrossEdge(0, 3, 5, 0) == -15);
    EXPECT(CrossEdge(2, 4, 3, 6) == 0);
}

// An overflow that FindHoleTriangleHit met at Mario's Star in session 16:
// -1603 * 1470717 is -2357559351, which keeps the low word 1937407945, and
// the second product was -1155791070, here 1190 * -971253. The difference,
// 3093199015, keeps the low word -1201768281, as the N64's mult and subu
// leave them.
static void TestKeepsTheLowWordOfOverflows(void) {
    EXPECT(CrossEdge(-1603, 1190, -971253, 1470717) == -1201768281);
}

int main(void) {
    TestCrossesSmallEdges();
    TestKeepsTheLowWordOfOverflows();
    TestScalesBothSigns();
    TestScalesABillboardLeftOfZero();
    TestGivesASignedInt();
    return ReportTestResults("course_space_test");
}
