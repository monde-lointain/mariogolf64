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

int main(void) {
    TestScalesBothSigns();
    TestScalesABillboardLeftOfZero();
    TestGivesASignedInt();
    return ReportTestResults("course_space_test");
}
