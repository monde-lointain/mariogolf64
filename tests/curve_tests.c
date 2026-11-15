/*
 * Characterizes curve.c's paths and Hermite curves. StepAlongPath moves a
 * position toward a target that runs along a path's lines, a step a call,
 * as the wind's Fly Guy flies; InterpolateHermite gives the values between
 * samples, as the swing's trail draws them. The values below are worked by
 * hand from the code, and exact in floats, so each build must give them.
 */

#include <ultra64.h>

#include "math/curve.h"
#include "system/c_language.h"
#include "unity_fixture.h"

// The path that most tests step along: the target runs from the second
// point to the third and then to the fourth
#define TEST_PATH_POINT_COUNT 4
static PathPoint f_TestPath[TEST_PATH_POINT_COUNT] = {
    {0.0f, 0.0f, 0.0f},
    {100.0f, 0.0f, 0.0f},
    {100.0f, 40.0f, 0.0f},
    {100.0f, 40.0f, -61.0f},
};
// The calls that take a position along it before the one that ends it, at
// half the remaining distance each step and two steps a line
#define TEST_PATH_PACE PathPace(1, 2)
#define TEST_PATH_STEP_COUNT 4

// A path of one line, 1600 units along x from the second point, on which
// the first step shows a pace's catch-up
#define LINE_POINT_COUNT 3
#define LINE_LENGTH 1600.0f
static PathPoint f_Line[LINE_POINT_COUNT] = {
    {0.0f, 0.0f, 0.0f},
    {LINE_LENGTH, 0.0f, 0.0f},
    {LINE_LENGTH, 0.0f, 0.0f},
};

// A pace that none of the cases of StepAlongPath gives
#define UNKNOWN_PACE 99

TEST_GROUP(Curve);

TEST_SETUP(Curve) {
    ResetPath();
}

TEST_TEAR_DOWN(Curve) {
}

static void AssertPosition(f32 x, f32 y, f32 z, const PathPoint *position) {
    TEST_ASSERT_EQUAL_FLOAT(x, position->x);
    TEST_ASSERT_EQUAL_FLOAT(y, position->y);
    TEST_ASSERT_EQUAL_FLOAT(z, position->z);
}

// Steps along a path with pace until it ends, and returns how many calls
// that took, the last included
static s32 CountStepsToEnd(PathPoint *points, s32 pointCount, s32 pace) {
    PathPoint position;
    s32 callCount = 1;

    while (StepAlongPath(points, pointCount, pace, &position) != PATH_END_REACHED) {
        callCount++;
    }
    return callCount;
}

// Each step covers half the distance to the target, truncated toward zero
// on each axis: the last step's z moves -15.25, of which -15 is taken
TEST(Curve, PositionChasesTheTargetAlongEachLine) {
    static const PathPoint expected[TEST_PATH_STEP_COUNT] = {
        {50.0f, 0.0f, 0.0f},
        {75.0f, 10.0f, 0.0f},
        {87.0f, 25.0f, 0.0f},
        {93.0f, 32.0f, -15.0f},
    };
    PathPoint position;
    s32 i;

    for (i = 0; i < TEST_PATH_STEP_COUNT; i++) {
        TEST_ASSERT_LESS_OR_EQUAL_INT32(
            0,
            StepAlongPath(f_TestPath, TEST_PATH_POINT_COUNT, TEST_PATH_PACE, &position)
        );
        AssertPosition(expected[i].x, expected[i].y, expected[i].z, &position);
    }
}

TEST(Curve, PathEndsOnItsLastPoint) {
    PathPoint position;
    s32 i;

    for (i = 0; i < TEST_PATH_STEP_COUNT; i++) {
        (void)StepAlongPath(f_TestPath, TEST_PATH_POINT_COUNT, TEST_PATH_PACE, &position);
    }
    TEST_ASSERT_EQUAL_INT32(
        PATH_END_REACHED,
        StepAlongPath(f_TestPath, TEST_PATH_POINT_COUNT, TEST_PATH_PACE, &position)
    );
    AssertPosition(100.0f, 40.0f, -61.0f, &position);
}

TEST(Curve, PathStartsOverOnceEnded) {
    PathPoint position;

    (void)CountStepsToEnd(f_TestPath, TEST_PATH_POINT_COUNT, TEST_PATH_PACE);
    (void)StepAlongPath(f_TestPath, TEST_PATH_POINT_COUNT, TEST_PATH_PACE, &position);
    AssertPosition(50.0f, 0.0f, 0.0f, &position);
}

TEST(Curve, ResetPathStartsOverPartway) {
    PathPoint position;

    (void)StepAlongPath(f_TestPath, TEST_PATH_POINT_COUNT, TEST_PATH_PACE, &position);
    (void)StepAlongPath(f_TestPath, TEST_PATH_POINT_COUNT, TEST_PATH_PACE, &position);
    (void)StepAlongPath(f_TestPath, TEST_PATH_POINT_COUNT, TEST_PATH_PACE, &position);
    ResetPath();
    (void)StepAlongPath(f_TestPath, TEST_PATH_POINT_COUNT, TEST_PATH_PACE, &position);
    AssertPosition(50.0f, 0.0f, 0.0f, &position);
}

// Each pace's first step covers catchUp / stepsPerLine of the line, and
// the path ends after stepsPerLine calls a line and one more
TEST(Curve, EachPaceSetsItsCatchUpAndSteps) {
    static const s32 paces[] = {
        PathPace(1, 2),
        PathPace(1, 4),
        PathPace(1, 8),
        PathPace(2, 4),
        PathPace(2, 8),
        PathPace(2, 16),
        PathPace(3, 4),
        PathPace(4, 8),
        PathPace(4, 16),
    };
    static const f32 firstStepX[ARRAY_COUNT(paces)] =
        {800.0f, 400.0f, 200.0f, 800.0f, 400.0f, 200.0f, 1200.0f, 800.0f, 400.0f};
    static const s32 callCounts[ARRAY_COUNT(paces)] = {3, 5, 9, 5, 9, 17, 5, 9, 17};
    PathPoint position;
    s32 i;

    for (i = 0; i < ARRAY_COUNT(paces); i++) {
        ResetPath();
        (void)StepAlongPath(f_Line, LINE_POINT_COUNT, paces[i], &position);
        TEST_ASSERT_EQUAL_FLOAT(firstStepX[i], position.x);
        ResetPath();
        TEST_ASSERT_EQUAL_INT32(callCounts[i], CountStepsToEnd(f_Line, LINE_POINT_COUNT, paces[i]));
    }
}

TEST(Curve, UnknownPaceKeepsTheLastPathsPace) {
    PathPoint position;

    (void)CountStepsToEnd(f_Line, LINE_POINT_COUNT, PathPace(3, 4));
    (void)StepAlongPath(f_Line, LINE_POINT_COUNT, UNKNOWN_PACE, &position);
    TEST_ASSERT_EQUAL_FLOAT(1200.0f, position.x);
}

TEST(Curve, HermiteGivesEachSampleAtItsIndex) {
    f32 values[] = {7.0f, -3.0f, 12.0f, 5.0f};

    TEST_ASSERT_EQUAL_FLOAT(7.0f, InterpolateHermite(values, 0.0f));
    TEST_ASSERT_EQUAL_FLOAT(-3.0f, InterpolateHermite(values, 1.0f));
}

TEST(Curve, HermiteFollowsEvenlySpacedSamplesInAStraightLine) {
    f32 values[] = {0.0f, 10.0f, 20.0f, 30.0f};

    TEST_ASSERT_EQUAL_FLOAT(2.5f, InterpolateHermite(values, 0.25f));
    TEST_ASSERT_EQUAL_FLOAT(15.0f, InterpolateHermite(values, 1.5f));
}

// Between 0 and 10 with slopes 10 and -10: -20f^3 + 20f^2 + 10f
TEST(Curve, HermiteBulgesTowardAPeak) {
    f32 values[] = {0.0f, 10.0f, 0.0f};

    TEST_ASSERT_EQUAL_FLOAT(3.4375f, InterpolateHermite(values, 0.25f));
    TEST_ASSERT_EQUAL_FLOAT(7.5f, InterpolateHermite(values, 0.5f));
}

// The values truncate to 0 and 2, but the slopes are the differences of
// the values before truncating, 1.5, truncated to 1: -2f^3 + 3f^2 + f
TEST(Curve, HermiteTruncatesSamplesAndSlopesApart) {
    f32 values[] = {0.5f, 2.0f, 3.5f};

    TEST_ASSERT_EQUAL_FLOAT(1.0f, InterpolateHermite(values, 0.5f));
}

TEST_GROUP_RUNNER(Curve) {
    RUN_TEST_CASE(Curve, PositionChasesTheTargetAlongEachLine);
    RUN_TEST_CASE(Curve, PathEndsOnItsLastPoint);
    RUN_TEST_CASE(Curve, PathStartsOverOnceEnded);
    RUN_TEST_CASE(Curve, ResetPathStartsOverPartway);
    RUN_TEST_CASE(Curve, EachPaceSetsItsCatchUpAndSteps);
    RUN_TEST_CASE(Curve, UnknownPaceKeepsTheLastPathsPace);
    RUN_TEST_CASE(Curve, HermiteGivesEachSampleAtItsIndex);
    RUN_TEST_CASE(Curve, HermiteFollowsEvenlySpacedSamplesInAStraightLine);
    RUN_TEST_CASE(Curve, HermiteBulgesTowardAPeak);
    RUN_TEST_CASE(Curve, HermiteTruncatesSamplesAndSlopesApart);
}
