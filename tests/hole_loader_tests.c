/*
 * Characterizes how hole_loader.c and the hole builder load a hole: the
 * builder decodes the hole's ground and points from the asset archive in
 * ROM, and the loader shades the ground for the scene's light. The values
 * are the matching build's. A hash stands for each large state, such as the
 * ground's heights, which a difference anywhere changes. The ground's edges
 * and the first mini-golf course are boundaries, so the tests take points
 * on both sides of each.
 */

#include <ultra64.h>

#include "course/course.h"
#include "course/course_space.h"
#include "course/hole_loader.h"
#include "course/hole_setup.h"
#include "course/surface.h"
#include "course/terrain.h"
#include "debug/game_flags.h"
#include "hole_fixture.h"
#include "math/geometry.h"
#include "unity_fixture.h"

// Toad Highlands' first hole
#define FIRST_HOLE_PAR 4
static const CoursePoint f_FirstHoleTee = {0x203C00, -0x65000, 0x6DF400};
static const CoursePoint f_FirstHoleCups[CUP_POSITION_COUNT] = {
    {0x203603, -0x40000, 0x207B16},
    {0x1F4B72, -0x40000, 0x2149E6},
    {0x1F2B2E, -0x3E000, 0x1DCE4F},
    {0x20636C, -0x3DB08, 0x1C39E5},
};
#define FIRST_HOLE_HEIGHTS_HASH 0x0E622107
#define FIRST_HOLE_SURFACES_HASH 0xAE0499A8
#define FIRST_HOLE_COLORS_HASH 0x14FE0DD8

// Whether the hole that the group's tests share loaded
static Boolean f_IsHoleLoaded;

TEST_GROUP(HoleLoader);

TEST_SETUP(HoleLoader) {
    TEST_ASSERT_TRUE_MESSAGE(f_IsHoleLoaded, "The hole builder's overlay didn't load");
}

TEST_TEAR_DOWN(HoleLoader) {
}

TEST(HoleLoader, LoadsThePar) {
    TEST_ASSERT_EQUAL_INT(FIRST_HOLE_PAR, g_HoleSetup.par);
}

TEST(HoleLoader, LoadsTheTee) {
    TEST_ASSERT_EQUAL_HEX32_ARRAY(
        f_FirstHoleTee,
        g_HolePoints.teePositions[PLAYED_TEE],
        AXIS_COUNT
    );
}

TEST(HoleLoader, LoadsTheCups) {
    TEST_ASSERT_EQUAL_HEX32_ARRAY(
        f_FirstHoleCups,
        g_HolePoints.cupPositions,
        CUP_POSITION_COUNT * AXIS_COUNT
    );
}

TEST(HoleLoader, SetsTheTeeAndCupsOnTheGround) {
    const s32 *tee = g_HolePoints.teePositions[PLAYED_TEE];
    s32 cup;

    TEST_ASSERT_EQUAL_HEX32(tee[AXIS_Y], GetGroundHeight(tee[AXIS_X], tee[AXIS_Z]));
    for (cup = 0; cup < CUP_POSITION_COUNT; cup++) {
        const s32 *position = g_HolePoints.cupPositions[cup];

        TEST_ASSERT_EQUAL_HEX32(
            position[AXIS_Y],
            GetGroundHeight(position[AXIS_X], position[AXIS_Z])
        );
    }
}

TEST(HoleLoader, GivesTheTeeCupAndAimTheirSurfaces) {
    const s32 *tee = g_HolePoints.teePositions[PLAYED_TEE];
    const s32 *cup = g_HolePoints.cupPositions[DEFAULT_CUP];
    const s32 *aim = g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT];

    TEST_ASSERT_EQUAL_INT(SURFACE_TEE_GROUND, GetSurface(tee[AXIS_X], tee[AXIS_Z]));
    TEST_ASSERT_EQUAL_INT(SURFACE_GREEN, GetSurface(cup[AXIS_X], cup[AXIS_Z]));
    TEST_ASSERT_EQUAL_INT(SURFACE_FAIRWAY, GetSurface(aim[AXIS_X], aim[AXIS_Z]));
}

TEST(HoleLoader, KeepsTheGroundsHeights) {
    TEST_ASSERT_EQUAL_HEX32(FIRST_HOLE_HEIGHTS_HASH, HashGroundHeights());
}

TEST(HoleLoader, KeepsTheGroundsSurfaces) {
    TEST_ASSERT_EQUAL_HEX32(FIRST_HOLE_SURFACES_HASH, HashGroundSurfaces());
}

TEST(HoleLoader, ShadesTheGround) {
    u32 hash = FNV_OFFSET_BASIS;
    s32 column, row;

    for (row = 0; row < GROUND_POINT_ROW_COUNT; row++) {
        for (column = 0; column < GROUND_POINT_COLUMN_COUNT; column++) {
            const Vtx *point = GetGroundPoint(column, row);

            hash = HashBytes(hash, point->v.cn, sizeof(point->v.cn));
        }
    }
    TEST_ASSERT_EQUAL_HEX32(FIRST_HOLE_COLORS_HASH, hash);
}

// A meter's margin around the ground is out of the course
#define EDGE_MARGIN ((s32)UNITS_PER_METER)
#define MIDDLE_X (GROUND_WIDTH / 2)
#define MIDDLE_Z (GROUND_LENGTH / 2)

TEST(HoleLoader, MarginAlongTheSidesIsOutOfTheCourse) {
    TEST_ASSERT_EQUAL_INT(SURFACE_OUT_OF_COURSE, GetSurface(EDGE_MARGIN - 1, MIDDLE_Z));
    TEST_ASSERT_NOT_EQUAL_INT(SURFACE_OUT_OF_COURSE, GetSurface(EDGE_MARGIN, MIDDLE_Z));
    TEST_ASSERT_NOT_EQUAL_INT(
        SURFACE_OUT_OF_COURSE,
        GetSurface(GROUND_WIDTH - EDGE_MARGIN - 1, MIDDLE_Z)
    );
    TEST_ASSERT_EQUAL_INT(SURFACE_OUT_OF_COURSE, GetSurface(GROUND_WIDTH - EDGE_MARGIN, MIDDLE_Z));
}

TEST(HoleLoader, MarginAlongTheEndsIsOutOfTheCourse) {
    TEST_ASSERT_EQUAL_INT(SURFACE_OUT_OF_COURSE, GetSurface(MIDDLE_X, EDGE_MARGIN - 1));
    TEST_ASSERT_NOT_EQUAL_INT(SURFACE_OUT_OF_COURSE, GetSurface(MIDDLE_X, EDGE_MARGIN));
    TEST_ASSERT_NOT_EQUAL_INT(
        SURFACE_OUT_OF_COURSE,
        GetSurface(MIDDLE_X, GROUND_LENGTH - EDGE_MARGIN - 1)
    );
    TEST_ASSERT_EQUAL_INT(SURFACE_OUT_OF_COURSE, GetSurface(MIDDLE_X, GROUND_LENGTH - EDGE_MARGIN));
}

// Off the ground, the height is the nearest edge's
TEST(HoleLoader, GroundExtendsPastItsEdges) {
    TEST_ASSERT_EQUAL_INT32(GetGroundHeight(0, MIDDLE_Z), GetGroundHeight(-1, MIDDLE_Z));
    TEST_ASSERT_EQUAL_INT32(GetGroundHeight(0, MIDDLE_Z), GetGroundHeight(-GROUND_WIDTH, MIDDLE_Z));
    TEST_ASSERT_EQUAL_INT32(
        GetGroundHeight(GROUND_WIDTH - 1, MIDDLE_Z),
        GetGroundHeight(GROUND_WIDTH, MIDDLE_Z)
    );
    TEST_ASSERT_EQUAL_INT32(GetGroundHeight(MIDDLE_X, 0), GetGroundHeight(MIDDLE_X, -1));
    TEST_ASSERT_EQUAL_INT32(
        GetGroundHeight(MIDDLE_X, GROUND_LENGTH - 1),
        GetGroundHeight(MIDDLE_X, GROUND_LENGTH)
    );
    TEST_ASSERT_EQUAL_INT32(
        GetGroundHeight(GROUND_WIDTH - 1, GROUND_LENGTH - 1),
        GetGroundHeight(2 * GROUND_WIDTH, 2 * GROUND_LENGTH)
    );
}

TEST_GROUP_RUNNER(HoleLoader) {
    // The tests only read the hole, so they share one load, which takes
    // most of a second of the console's time
    ClearAllGameFlags();
    f_IsHoleLoaded = LoadTestHole(COURSE_TOAD_HIGHLANDS, 0);
    RUN_TEST_CASE(HoleLoader, LoadsThePar);
    RUN_TEST_CASE(HoleLoader, LoadsTheTee);
    RUN_TEST_CASE(HoleLoader, LoadsTheCups);
    RUN_TEST_CASE(HoleLoader, SetsTheTeeAndCupsOnTheGround);
    RUN_TEST_CASE(HoleLoader, GivesTheTeeCupAndAimTheirSurfaces);
    RUN_TEST_CASE(HoleLoader, KeepsTheGroundsHeights);
    RUN_TEST_CASE(HoleLoader, KeepsTheGroundsSurfaces);
    RUN_TEST_CASE(HoleLoader, ShadesTheGround);
    RUN_TEST_CASE(HoleLoader, MarginAlongTheSidesIsOutOfTheCourse);
    RUN_TEST_CASE(HoleLoader, MarginAlongTheEndsIsOutOfTheCourse);
    RUN_TEST_CASE(HoleLoader, GroundExtendsPastItsEdges);
}

// Holes of other courses, which decode other ground. Each loads without
// shading, which leaves the ground's shape as it is.
TEST_GROUP(CourseHoles);

TEST_SETUP(CourseHoles) {
    ClearAllGameFlags();
    SetGameFlag(GAME_FLAG_UNSHADED_GROUND);
}

TEST_TEAR_DOWN(CourseHoles) {
}

// A loaded hole's tee and the hashes of its ground
typedef struct LoadedHole {
    s32 teeX;
    s32 teeZ;
    u32 heightsHash;
    u32 surfacesHash;
} LoadedHole;

static void AssertHoleLoads(CourseIndex courseIndex, s32 holeIndex, const LoadedHole *expected) {
    LoadedHole loaded;

    TEST_ASSERT_TRUE_MESSAGE(
        LoadTestHole(courseIndex, holeIndex),
        "The hole builder's overlay didn't load"
    );
    loaded.teeX = g_HolePoints.teePositions[PLAYED_TEE][AXIS_X];
    loaded.teeZ = g_HolePoints.teePositions[PLAYED_TEE][AXIS_Z];
    loaded.heightsHash = HashGroundHeights();
    loaded.surfacesHash = HashGroundSurfaces();
    TEST_ASSERT_EQUAL_HEX32(expected->teeX, loaded.teeX);
    TEST_ASSERT_EQUAL_HEX32(expected->teeZ, loaded.teeZ);
    TEST_ASSERT_EQUAL_HEX32(expected->heightsHash, loaded.heightsHash);
    TEST_ASSERT_EQUAL_HEX32(expected->surfacesHash, loaded.surfacesHash);
}

TEST(CourseHoles, LoadsShyGuyDesertsThirdHole) {
    static const LoadedHole expected = {0x194400, 0x71B000, 0x860614C2, 0x7BFC06B4};

    AssertHoleLoads(COURSE_SHY_GUY_DESERT, 2, &expected);
}

// The last course of full size, before mini golf's
TEST(CourseHoles, LoadsMariosStarsSixthHole) {
    static const LoadedHole expected = {0x204000, 0x3EAC00, 0x7134D282, 0x3B534B19};

    AssertHoleLoads(COURSE_MARIOS_STAR, 5, &expected);
}

// Mini golf, from MINI_GOLF_COURSE_INDEX on, scales the hole down to its
// smaller ground
TEST(CourseHoles, LoadsAMiniGolfHole) {
    static const LoadedHole expected = {0x91500, 0x9B480, 0xAA6AE04D, 0x9033A4D5};

    AssertHoleLoads(COURSE_LUIGIS_GARDEN_FIRST, 6, &expected);
}

TEST_GROUP_RUNNER(CourseHoles) {
    RUN_TEST_CASE(CourseHoles, LoadsShyGuyDesertsThirdHole);
    RUN_TEST_CASE(CourseHoles, LoadsMariosStarsSixthHole);
    RUN_TEST_CASE(CourseHoles, LoadsAMiniGolfHole);
}
