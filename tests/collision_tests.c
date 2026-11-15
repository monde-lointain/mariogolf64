/*
 * Characterizes collision.c, which finds what the moving ball hits. The
 * flagstick and the cup need only the cup's position, which the tests set,
 * and their values are worked by hand from the code, as are the bounds of
 * the billboards' hit masks. The rest read holes: on Toad Highlands' last,
 * a path straight down meets the ground at the height that the terrain
 * gives, a path through a billboard hits the cell of its mask there, and a
 * path through a ring's middle or within its radius hits it. A mini-golf
 * hole's ground is read in the same way, and a ball on one of its walls is
 * pushed out of them all. Grids of paths over the holes, every mask cell
 * and the pushed balls are hashed and compared with what the matching
 * build finds.
 *
 * Course space's y points down, so a point above another has a smaller y.
 */

#include <string.h>
#include <ultra64.h>

#include "course/billboard.h"
#include "course/collision.h"
#include "course/course.h"
#include "course/hole_setup.h"
#include "course/hole_triangles.h"
#include "course/pin.h"
#include "course/terrain.h"
#include "course/work_area.h"
#include "debug/game_flags.h"
#include "hole_fixture.h"
#include "math/geometry.h"
#include "round/game_state.h"
#include "shot/ball_simulation.h"
#include "unity_fixture.h"

// The fraction along a path that the hit tests give when nothing is hit
#define NO_HIT_FRACTION 1.1f

// Where the pin tests put the cup, and the course units across the paths
// that they send past it, which start and end half of that from the cup
#define TEST_CUP_X 0x100000
#define TEST_CUP_Y 0x40000
#define TEST_CUP_Z 0x200000
#define PIN_PATH_LENGTH 0x4000
// The flagstick's radius and the flag's reach, in whole course units
#define FLAGSTICK_RADIUS_UNITS 614
#define FLAG_REACH_UNITS 1228
// Heights over the cup: one on the flagstick below the flag, one at the
// flag, and one over the flagstick
#define STICK_HEIGHT 0x1000
#define FLAG_HEIGHT 0xA000
#define OVER_PIN_HEIGHT (FLAGSTICK_HEIGHT + 1)
// A path that passes this far from the cup misses the flagstick but meets
// the flag
#define FLAG_ONLY_DISTANCE 1000
// What a velocity holds before a test, to show whether it was set
#define UNSET_SPEED 0x7777
// The speed along y that a ball keeps when it hits the flag
#define FLAG_HIT_SPEED_Y (-524)
// What a ball bounces off the flagstick with: 0.8 of PIN_PATH_LENGTH, half
// that while flying, and at least 0.2 of it
#define STICK_BOUNCE_SPEED 13107
#define FLYING_STICK_BOUNCE_SPEED 6553
#define LEAST_STICK_BOUNCE_SPEED 3276
// A fall steep enough to take the bounce to its least
#define STEEP_FALL 0x2000

// The cup's edge, in course units from it, to which a ball nearer it moves.
// The ball's place is the cup's plus 614.4 along its way, truncated toward
// zero, so it is a unit farther where its coordinate falls.
#define CUP_EDGE_UNITS 614

// The hole has the course's most flat and upright hole triangles, and two
// rings. Every mini-golf hole has walls.
#define TEST_COURSE COURSE_TOAD_HIGHLANDS
#define TEST_HOLE 17
#define MINI_GOLF_TEST_COURSE COURSE_LUIGIS_GARDEN_FIRST
#define MINI_GOLF_TEST_HOLE 6

// The grid of points over the ground that the ground's tests probe, set in
// from the edges, and how far above and below the ground their paths reach
#define PROBE_COLUMN_COUNT 15
#define PROBE_ROW_COUNT 31
#define PROBE_MARGIN 0x8000
#define PROBE_REACH 0x40000
// The most that a straight-down path's hit and the terrain's height differ,
// in course units, of which there are 1024 to a world unit
#define GROUND_HIT_TOLERANCE 2
// How far above and below the ground a short path down reaches, which
// crosses it by less than the 786 that a rolling ball barely crosses by,
// and how far from halfway the hit can be, which is GROUND_HIT_TOLERANCE
// of the path's length
#define BARE_CROSSING_REACH 100
#define BARE_CROSSING_TOLERANCE FRACTION_TOLERANCE
// How far along x and z either side of a grid point the hashes' slanted
// paths start and end, as parts of the ground's width and length
#define SLANT_X_PARTS 64
#define SLANT_X_SHARE 3
#define SLANT_Z_PARTS 128
#define SLANT_Z_SHARE (-5)

// The heights down a billboard, as fractions of it, that its tests pass
// through, and how far either side of it, in world units, their paths start
#define BILLBOARD_DOWN_COUNT 4
#define BILLBOARD_PATH_REACH 8
// How far along x and z either side of a billboard the hash's paths start
// and end, so that they cross it at a slant
#define BILLBOARD_SLANT_X 0x30000
#define BILLBOARD_SLANT_Z 0x1234

// The FNV-1a hashes of what the matching build finds
#define GROUND_HITS_HASH 0xEC187030
#define HOLE_TRIANGLE_HITS_HASH 0x0FE23F0C
#define BILLBOARD_HITS_HASH 0xBE684BD9
#define MASK_CELLS_HASH 0x0ADE2670
#define MINI_GOLF_GROUND_HITS_HASH 0xB12C6FF2
#define WALL_HITS_HASH 0xCB67E5B0
#define WALL_PUSHES_HASH 0xEFE0C7CB

// How far either side of a ring's middle, in world units, its tests' paths
// start and end, and the radius within which a ring is hit for each unit of
// its scale
#define RING_PATH_REACH 100
#define RING_RADIUS_PER_SCALE 0.1274414f
// The shares of a ring's radius that its tests pass inside and outside
#define INSIDE_RING_SHARE 0.9f
#define OUTSIDE_RING_SHARE 1.1f
// The fraction along a path that its hits can be from where the tests put
// them
#define FRACTION_TOLERANCE 0.01f
// What FindRingHit gives for a path that starts or ends off the ground
#define RING_PATH_OFF_GROUND (-1)

// The texture indices that select a billboard's mask, the kinds of cell
// that a mask can read as, which are clamped to fewer than this, and the
// fraction across or down a billboard of its masks' last cell
#define MASK_TEXTURE_COUNT 64
#define MASK_CELL_KIND_COUNT 16
#define LAST_CELL_FRACTION 0.99f

// Whether the hole loaded for the group, and the ball that the mini-golf
// ground's tests send
static Boolean f_IsHoleLoaded;
static BallSimulation f_MiniGolfBall;
static CoursePoint f_SavedCupPosition;

TEST_GROUP(Collision);

TEST_SETUP(Collision) {
    memcpy(f_SavedCupPosition, g_HoleSetup.cupPosition, sizeof(f_SavedCupPosition));
    ClearGameFlag(GAME_FLAG_RISING_FLAG_HITS);
    ClearGameFlag(GAME_FLAG_FILL_BILLBOARD_MASKS);
}

TEST_TEAR_DOWN(Collision) {
    memcpy(g_HoleSetup.cupPosition, f_SavedCupPosition, sizeof(f_SavedCupPosition));
}

static void PlaceTestCup(void) {
    g_HoleSetup.cupPosition[AXIS_X] = TEST_CUP_X;
    g_HoleSetup.cupPosition[AXIS_Y] = TEST_CUP_Y;
    g_HoleSetup.cupPosition[AXIS_Z] = TEST_CUP_Z;
}

// What a pin test finds: which part of the pin is hit, the fraction along
// the path and the ball's new velocity
typedef struct PinTestHit {
    PinHit part;
    f32 fraction;
    CoursePoint velocity;
} PinTestHit;

// Sends a ball along x past the test cup, PIN_PATH_LENGTH long and centered
// on it, offsetZ from it, from startHeight to endHeight over the cup
static PinTestHit SendPastPin(
    s32 offsetZ,
    s32 startHeight,
    s32 endHeight,
    Boolean hasHitFlag,
    BallState state
) {
    BallSimulation simulation;
    PinTestHit hit;

    PlaceTestCup();
    bzero(&simulation, sizeof(simulation));
    simulation.state = state;
    hit.velocity[AXIS_X] = hit.velocity[AXIS_Y] = hit.velocity[AXIS_Z] = UNSET_SPEED;
    hit.part = FindPinHit(
        TEST_CUP_X - (PIN_PATH_LENGTH / 2),
        TEST_CUP_Y - startHeight,
        TEST_CUP_Z + offsetZ,
        TEST_CUP_X + (PIN_PATH_LENGTH / 2),
        TEST_CUP_Y - endHeight,
        TEST_CUP_Z + offsetZ,
        hit.velocity,
        &hit.fraction,
        hasHitFlag,
        &simulation
    );
    return hit;
}

static void AssertVelocity(s32 x, s32 y, s32 z, const PinTestHit *hit) {
    TEST_ASSERT_EQUAL_INT32(x, hit->velocity[AXIS_X]);
    TEST_ASSERT_EQUAL_INT32(y, hit->velocity[AXIS_Y]);
    TEST_ASSERT_EQUAL_INT32(z, hit->velocity[AXIS_Z]);
}

TEST(Collision, PathBeyondTheFlagsReachMissesThePin) {
    PinTestHit hit = SendPastPin(FLAG_REACH_UNITS + 1, FLAG_HEIGHT, FLAG_HEIGHT, FALSE, 0);

    TEST_ASSERT_EQUAL_INT32(PIN_HIT_NONE, hit.part);
    TEST_ASSERT_EQUAL_FLOAT(NO_HIT_FRACTION, hit.fraction);
    AssertVelocity(UNSET_SPEED, UNSET_SPEED, UNSET_SPEED, &hit);
}

// The ball meets the flagstick halfway and bounces straight back
TEST(Collision, PathThroughTheFlagstickBouncesBack) {
    PinTestHit hit = SendPastPin(0, STICK_HEIGHT, STICK_HEIGHT, FALSE, BALL_STATE_ROLLING);

    TEST_ASSERT_EQUAL_INT32(PIN_HIT_FLAGSTICK, hit.part);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, hit.fraction);
    AssertVelocity(-STICK_BOUNCE_SPEED, 0, 0, &hit);
}

TEST(Collision, FlyingBallBouncesOffTheFlagstickSlower) {
    PinTestHit hit = SendPastPin(0, STICK_HEIGHT, STICK_HEIGHT, FALSE, BALL_STATE_FLYING);

    TEST_ASSERT_EQUAL_INT32(PIN_HIT_FLAGSTICK, hit.part);
    AssertVelocity(-FLYING_STICK_BOUNCE_SPEED, 0, 0, &hit);
}

// The fall takes twice its speed from the bounce, down to a fifth of the
// ball's speed, and the ball keeps falling
TEST(Collision, SteepFallBouncesOffTheFlagstickWithTheLeastSpeed) {
    PinTestHit hit = SendPastPin(
        0,
        STICK_HEIGHT + (STEEP_FALL / 2),
        STICK_HEIGHT - (STEEP_FALL / 2),
        FALSE,
        BALL_STATE_ROLLING
    );

    TEST_ASSERT_EQUAL_INT32(PIN_HIT_FLAGSTICK, hit.part);
    AssertVelocity(-LEAST_STICK_BOUNCE_SPEED, STEEP_FALL, 0, &hit);
}

// The flagstick's edge is FLAGSTICK_RADIUS_UNITS from the cup
TEST(Collision, FlagstickReachesItsRadius) {
    PinTestHit atEdge = SendPastPin(FLAGSTICK_RADIUS_UNITS, STICK_HEIGHT, STICK_HEIGHT, FALSE, 0);
    PinTestHit pastEdge =
        SendPastPin(FLAGSTICK_RADIUS_UNITS + 1, STICK_HEIGHT, STICK_HEIGHT, FALSE, 0);

    TEST_ASSERT_EQUAL_INT32(PIN_HIT_FLAGSTICK, atEdge.part);
    TEST_ASSERT_EQUAL_INT32(PIN_HIT_NONE, pastEdge.part);
}

// The flag is at the top of the flagstick, so the top is hit as the flag
TEST(Collision, PinReachesUpToTheFlagstickHeight) {
    PinTestHit atTop = SendPastPin(0, FLAGSTICK_HEIGHT, FLAGSTICK_HEIGHT, FALSE, 0);
    PinTestHit overTop = SendPastPin(0, OVER_PIN_HEIGHT, OVER_PIN_HEIGHT, FALSE, 0);

    TEST_ASSERT_EQUAL_INT32(PIN_HIT_FLAG, atTop.part);
    TEST_ASSERT_EQUAL_INT32(PIN_HIT_NONE, overTop.part);
}

// The flag stops the ball, which then drops
TEST(Collision, PathThroughTheFlagStopsTheBall) {
    PinTestHit hit = SendPastPin(0, FLAG_HEIGHT, FLAG_HEIGHT, FALSE, BALL_STATE_FLYING);

    TEST_ASSERT_EQUAL_INT32(PIN_HIT_FLAG, hit.part);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, hit.fraction);
    AssertVelocity(0, FLAG_HIT_SPEED_Y, 0, &hit);
}

TEST(Collision, FlagReachesPastTheFlagstick) {
    PinTestHit atFlag = SendPastPin(FLAG_ONLY_DISTANCE, FLAG_HEIGHT, FLAG_HEIGHT, FALSE, 0);
    PinTestHit atStick = SendPastPin(FLAG_ONLY_DISTANCE, STICK_HEIGHT, STICK_HEIGHT, FALSE, 0);

    TEST_ASSERT_EQUAL_INT32(PIN_HIT_FLAG, atFlag.part);
    TEST_ASSERT_EQUAL_INT32(PIN_HIT_NONE, atStick.part);
}

// The flag's edge is FLAG_REACH_UNITS from the cup; the test past it is
// PathBeyondTheFlagsReachMissesThePin
TEST(Collision, FlagReachesItsEdge) {
    PinTestHit hit = SendPastPin(FLAG_REACH_UNITS, FLAG_HEIGHT, FLAG_HEIGHT, FALSE, 0);

    TEST_ASSERT_EQUAL_INT32(PIN_HIT_FLAG, hit.part);
}

// A rising ball passes the flag, unless a game flag lets it hit
TEST(Collision, RisingBallHitsTheFlagstickAtTheFlag) {
    s32 rise = 0x200;
    PinTestHit hit = SendPastPin(0, FLAG_HEIGHT - rise, FLAG_HEIGHT + rise, FALSE, 0);

    TEST_ASSERT_EQUAL_INT32(PIN_HIT_FLAGSTICK, hit.part);
    SetGameFlag(GAME_FLAG_RISING_FLAG_HITS);
    hit = SendPastPin(0, FLAG_HEIGHT - rise, FLAG_HEIGHT + rise, FALSE, 0);
    ClearGameFlag(GAME_FLAG_RISING_FLAG_HITS);
    TEST_ASSERT_EQUAL_INT32(PIN_HIT_FLAG, hit.part);
}

TEST(Collision, FlagIsHitOnlyOnce) {
    PinTestHit hit = SendPastPin(0, FLAG_HEIGHT, FLAG_HEIGHT, TRUE, BALL_STATE_FLYING);

    TEST_ASSERT_EQUAL_INT32(PIN_HIT_FLAGSTICK, hit.part);
}

// A path that ends inside the flagstick, without crossing the cup, hits it
// at its end
TEST(Collision, PathEndingInsideTheFlagstickHitsAtItsEnd) {
    BallSimulation simulation;
    CoursePoint velocity;
    f32 fraction;

    PlaceTestCup();
    bzero(&simulation, sizeof(simulation));
    TEST_ASSERT_EQUAL_INT32(
        PIN_HIT_FLAGSTICK,
        FindPinHit(
            TEST_CUP_X - PIN_PATH_LENGTH,
            TEST_CUP_Y - STICK_HEIGHT,
            TEST_CUP_Z,
            TEST_CUP_X - (FLAGSTICK_RADIUS_UNITS / 2),
            TEST_CUP_Y - STICK_HEIGHT,
            TEST_CUP_Z,
            velocity,
            &fraction,
            FALSE,
            &simulation
        )
    );
    TEST_ASSERT_EQUAL_FLOAT(1.0f, fraction);
}

// Moves a ball at offsetX and offsetZ from the test cup to the cup's edge if
// it is nearer, and asserts where it ends up
static void AssertMovedToCupEdge(s32 offsetX, s32 offsetZ, s32 movedX, s32 movedZ) {
    BallSimulation simulation;

    PlaceTestCup();
    bzero(&simulation, sizeof(simulation));
    simulation.position[AXIS_X] = TEST_CUP_X + offsetX;
    simulation.position[AXIS_Y] = TEST_CUP_Y;
    simulation.position[AXIS_Z] = TEST_CUP_Z + offsetZ;
    MoveBallToCupEdge(&simulation);
    TEST_ASSERT_EQUAL_INT32(TEST_CUP_X + movedX, simulation.position[AXIS_X]);
    TEST_ASSERT_EQUAL_INT32(TEST_CUP_Y, simulation.position[AXIS_Y]);
    TEST_ASSERT_EQUAL_INT32(TEST_CUP_Z + movedZ, simulation.position[AXIS_Z]);
}

TEST(Collision, BallNearTheCupMovesOutToItsEdge) {
    AssertMovedToCupEdge(300, 0, CUP_EDGE_UNITS, 0);
    AssertMovedToCupEdge(0, -300, 0, -(CUP_EDGE_UNITS + 1));
}

// 614.4 / sqrt(2) is 434.45
TEST(Collision, BallMovesOutAlongItsWayFromTheCup) {
    AssertMovedToCupEdge(300, 300, 434, 434);
}

TEST(Collision, BallAtTheCupOrBeyondItsEdgeStays) {
    AssertMovedToCupEdge(0, 0, 0, 0);
    AssertMovedToCupEdge(CUP_EDGE_UNITS + 1, 0, CUP_EDGE_UNITS + 1, 0);
}

// The size of the loaded hole's ground
static s32 GetGroundWidth(void) {
    return g_GameState.isMiniGolf ? MINI_GOLF_GROUND_SIZE : GROUND_WIDTH;
}

static s32 GetGroundLength(void) {
    return g_GameState.isMiniGolf ? MINI_GOLF_GROUND_SIZE : GROUND_LENGTH;
}

// The point of the probe grid at column and row
static s32 ProbeX(s32 column) {
    return PROBE_MARGIN +
        (column * ((GetGroundWidth() - (2 * PROBE_MARGIN)) / (PROBE_COLUMN_COUNT - 1)));
}

static s32 ProbeZ(s32 row) {
    return PROBE_MARGIN +
        (row * ((GetGroundLength() - (2 * PROBE_MARGIN)) / (PROBE_ROW_COUNT - 1)));
}

// FindGroundHit, FindMiniGolfGroundHit for f_MiniGolfBall, or
// FindHoleTriangleHit
typedef s32 (*TriangleHitFinder)(
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    PlaneTriangle *hit,
    f32 *fraction
);

static s32 FindMiniGolfBallHit(
    s32 startX,
    s32 startY,
    s32 startZ,
    s32 endX,
    s32 endY,
    s32 endZ,
    PlaneTriangle *hit,
    f32 *fraction
) {
    return FindMiniGolfGroundHit(
        &f_MiniGolfBall,
        startX,
        startY,
        startZ,
        endX,
        endY,
        endZ,
        hit,
        fraction
    );
}

// Asserts that a path straight down through each point of the probe grid
// meets the ground at the terrain's height
static void AssertPathsDownMeetTheGround(TriangleHitFinder find) {
    PlaneTriangle hit;
    f32 fraction;
    s32 column, row, x, z, height, hitY;

    for (row = 0; row < PROBE_ROW_COUNT; row++) {
        for (column = 0; column < PROBE_COLUMN_COUNT; column++) {
            x = ProbeX(column);
            z = ProbeZ(row);
            height = GetGroundHeight(x, z);
            TEST_ASSERT_TRUE(
                find(x, height - PROBE_REACH, z, x, height + PROBE_REACH, z, &hit, &fraction)
            );
            hitY = (height - PROBE_REACH) + (s32)(fraction * (2 * PROBE_REACH));
            TEST_ASSERT_INT32_WITHIN(GROUND_HIT_TOLERANCE, height, hitY);
        }
    }
}

static void AssertPathsAboveMissTheGround(TriangleHitFinder find) {
    PlaneTriangle hit;
    f32 fraction;
    s32 column, row, x, z, height;

    for (row = 0; row < PROBE_ROW_COUNT; row++) {
        for (column = 0; column < PROBE_COLUMN_COUNT; column++) {
            x = ProbeX(column);
            z = ProbeZ(row);
            height = GetGroundHeight(x, z);
            TEST_ASSERT_FALSE(
                find(x, height - (2 * PROBE_REACH), z, x, height - PROBE_REACH, z, &hit, &fraction)
            );
        }
    }
}

TEST(Collision, PathDownMeetsTheGroundAtItsHeight) {
    TEST_ASSERT_TRUE(f_IsHoleLoaded);
    AssertPathsDownMeetTheGround(FindGroundHit);
}

TEST(Collision, PathAboveTheGroundMissesIt) {
    TEST_ASSERT_TRUE(f_IsHoleLoaded);
    AssertPathsAboveMissTheGround(FindGroundHit);
}

// A billboard stands across the ball's path. A level path through its
// middle, at each of several heights down it, hits it where its mask has a
// cell, halfway along, and misses it elsewhere.
TEST(Collision, PathThroughABillboardHitsItsMasksCell) {
    static const f32 downs[BILLBOARD_DOWN_COUNT] = {0.125f, 0.375f, 0.625f, 0.875f};
    Billboard *billboard;
    BillboardCell expected, cell;
    f32 fraction;
    s32 i, downIndex, depth, y, hitCount = 0, missCount = 0;

    TEST_ASSERT_TRUE(f_IsHoleLoaded);
    for (i = 0; i < BILLBOARD_COUNT; i++) {
        billboard = &g_Billboards[i];
        if (billboard->textureIndex < 0 || billboard->width == 0 || billboard->height == 0) {
            continue;
        }
        for (downIndex = 0; downIndex < BILLBOARD_DOWN_COUNT; downIndex++) {
            depth = downs[downIndex] * billboard->height;
            expected =
                GetBillboardCell(billboard->textureIndex, 0.0f, (f32)depth / billboard->height);
            y = (billboard->y - depth) << WORLD_TO_COURSE_SHIFT;
            cell = BILLBOARD_CELL_EMPTY;
            if (FindBillboardHit(
                    (billboard->x - BILLBOARD_PATH_REACH) << WORLD_TO_COURSE_SHIFT,
                    y,
                    billboard->z << WORLD_TO_COURSE_SHIFT,
                    (billboard->x + BILLBOARD_PATH_REACH) << WORLD_TO_COURSE_SHIFT,
                    y,
                    billboard->z << WORLD_TO_COURSE_SHIFT,
                    &cell,
                    &fraction
                )) {
                TEST_ASSERT_EQUAL_FLOAT(0.5f, fraction);
                hitCount++;
            }
            else {
                missCount++;
            }
            TEST_ASSERT_EQUAL_INT32(expected, cell);
        }
    }
    // Some of each, or the test shows nothing
    TEST_ASSERT_GREATER_THAN_INT32(0, hitCount);
    TEST_ASSERT_GREATER_THAN_INT32(0, missCount);
}

// A fraction across or down a billboard outside it reads the mask's edge
TEST(Collision, MaskCellsStopAtTheMasksEdges) {
    s32 textureIndex;

    for (textureIndex = 0; textureIndex < MASK_TEXTURE_COUNT; textureIndex++) {
        TEST_ASSERT_EQUAL_INT32(
            GetBillboardCell(textureIndex, 0.0f, 0.0f),
            GetBillboardCell(textureIndex, -1.0f, -1.0f)
        );
        TEST_ASSERT_EQUAL_INT32(
            GetBillboardCell(textureIndex, LAST_CELL_FRACTION, LAST_CELL_FRACTION),
            GetBillboardCell(textureIndex, 2.0f, 2.0f)
        );
        TEST_ASSERT_EQUAL_INT32(
            GetBillboardSolidity(textureIndex, 0.0f, LAST_CELL_FRACTION),
            GetBillboardSolidity(textureIndex, -1.0f, 2.0f)
        );
    }
}

// Each cell of every texture's mask, read at the cell's middle, hashed with
// its solidity and counted by kind. The ragged edge of foliage reads as
// empty, so none is ignored.
TEST(Collision, MaskCellsAreTheMatchingBuilds) {
    s32 kindCounts[MASK_CELL_KIND_COUNT];
    u32 hash = FNV_OFFSET_BASIS;
    s32 textureIndex, row, column, cell, solidity;
    f32 across, down;

    bzero(kindCounts, sizeof(kindCounts));
    for (textureIndex = 0; textureIndex < MASK_TEXTURE_COUNT; textureIndex++) {
        for (row = 0; row < BILLBOARD_MASK_SIZE; row++) {
            for (column = 0; column < BILLBOARD_MASK_SIZE; column++) {
                across = (column + 0.5f) / BILLBOARD_MASK_SIZE;
                down = (row + 0.5f) / BILLBOARD_MASK_SIZE;
                cell = GetBillboardCell(textureIndex, across, down);
                solidity = GetBillboardSolidity(textureIndex, across, down);
                TEST_ASSERT_TRUE(cell >= 0 && cell < MASK_CELL_KIND_COUNT);
                kindCounts[cell]++;
                hash = HashBytes(hash, &cell, sizeof(cell));
                hash = HashBytes(hash, &solidity, sizeof(solidity));
            }
        }
    }
    TEST_ASSERT_EQUAL_INT32(0, kindCounts[BILLBOARD_CELL_IGNORED]);
    TEST_ASSERT_GREATER_THAN_INT32(0, kindCounts[BILLBOARD_CELL_TRUNK]);
    TEST_ASSERT_GREATER_THAN_INT32(0, kindCounts[BILLBOARD_CELL_FOLIAGE]);
    TEST_ASSERT_EQUAL_HEX32(MASK_CELLS_HASH, hash);
}

TEST(Collision, FilledMasksMakeEveryCellATrunk) {
    s32 row, column;

    SetGameFlag(GAME_FLAG_FILL_BILLBOARD_MASKS);
    for (row = 0; row < BILLBOARD_MASK_SIZE; row++) {
        for (column = 0; column < BILLBOARD_MASK_SIZE; column++) {
            TEST_ASSERT_EQUAL_INT32(
                BILLBOARD_CELL_TRUNK,
                GetBillboardCell(
                    0,
                    (f32)column / BILLBOARD_MASK_SIZE,
                    (f32)row / BILLBOARD_MASK_SIZE
                )
            );
        }
    }
    ClearGameFlag(GAME_FLAG_FILL_BILLBOARD_MASKS);
}

// Hashes what find gives for a slanted path down through each point of the
// probe grid, and counts the hits. The hit triangle is hashed by its fields:
// the finders copy it whole from their stacks, so its padding holds
// whatever was there, which differs between compilers.
static u32 HashGridHits(TriangleHitFinder find, s32 *hitCount) {
    PlaneTriangle triangle;
    f32 fraction;
    u32 hash = FNV_OFFSET_BASIS;
    s32 slantX = (GetGroundWidth() / SLANT_X_PARTS) * SLANT_X_SHARE;
    s32 slantZ = (GetGroundLength() / SLANT_Z_PARTS) * SLANT_Z_SHARE;
    s32 column, row, x, z, height, isHit;

    *hitCount = 0;
    for (row = 0; row < PROBE_ROW_COUNT; row++) {
        for (column = 0; column < PROBE_COLUMN_COUNT; column++) {
            x = ProbeX(column);
            z = ProbeZ(row);
            height = GetGroundHeight(x, z);
            bzero(&triangle, sizeof(triangle));
            fraction = 0.0f;
            isHit = find(
                x - slantX,
                height - PROBE_REACH,
                z - slantZ,
                x + slantX,
                height + PROBE_REACH,
                z + slantZ,
                &triangle,
                &fraction
            );
            *hitCount += isHit != FALSE;
            hash = HashBytes(hash, &isHit, sizeof(isHit));
            hash = HashBytes(hash, &fraction, sizeof(fraction));
            hash = HashBytes(hash, &triangle.tag, sizeof(triangle.tag));
            hash = HashBytes(hash, triangle.corners, sizeof(triangle.corners));
        }
    }
    return hash;
}

TEST(Collision, GroundHitsAreTheMatchingBuilds) {
    s32 hitCount;
    u32 hash;

    TEST_ASSERT_TRUE(f_IsHoleLoaded);
    hash = HashGridHits(FindGroundHit, &hitCount);
    TEST_ASSERT_EQUAL_INT32(PROBE_COLUMN_COUNT * PROBE_ROW_COUNT, hitCount);
    TEST_ASSERT_EQUAL_HEX32(GROUND_HITS_HASH, hash);
}

TEST(Collision, HoleTriangleHitsAreTheMatchingBuilds) {
    s32 hitCount;
    u32 hash;

    TEST_ASSERT_TRUE(f_IsHoleLoaded);
    hash = HashGridHits(FindHoleTriangleHit, &hitCount);
    TEST_ASSERT_GREATER_THAN_INT32(0, hitCount);
    TEST_ASSERT_EQUAL_HEX32(HOLE_TRIANGLE_HITS_HASH, hash);
}

// A level, slanted path through each billboard at its middle height, hashed
// with what it hits
TEST(Collision, BillboardHitsAreTheMatchingBuilds) {
    Billboard *billboard;
    BillboardCell cell;
    f32 fraction;
    u32 hash = FNV_OFFSET_BASIS;
    s32 i, isHit, x, y, z, hitCount = 0;

    TEST_ASSERT_TRUE(f_IsHoleLoaded);
    for (i = 0; i < BILLBOARD_COUNT; i++) {
        billboard = &g_Billboards[i];
        if (billboard->textureIndex < 0) {
            continue;
        }
        x = billboard->x << WORLD_TO_COURSE_SHIFT;
        y = (billboard->y - (billboard->height / 2)) << WORLD_TO_COURSE_SHIFT;
        z = billboard->z << WORLD_TO_COURSE_SHIFT;
        cell = BILLBOARD_CELL_EMPTY;
        isHit = FindBillboardHit(
            x - BILLBOARD_SLANT_X,
            y,
            z + BILLBOARD_SLANT_Z,
            x + BILLBOARD_SLANT_X,
            y,
            z - BILLBOARD_SLANT_Z,
            &cell,
            &fraction
        );
        hitCount += isHit != FALSE;
        hash = HashBytes(hash, &isHit, sizeof(isHit));
        hash = HashBytes(hash, &cell, sizeof(cell));
        hash = HashBytes(hash, &fraction, sizeof(fraction));
    }
    TEST_ASSERT_GREATER_THAN_INT32(0, hitCount);
    TEST_ASSERT_EQUAL_HEX32(BILLBOARD_HITS_HASH, hash);
}

// Sends a path through ring along the way that it faces, from startShare to
// endShare of RING_PATH_REACH beyond its middle, and offsetY world units
// down from it
static s32 SendThroughRing(
    const HoleTriangle *ring,
    f32 startShare,
    f32 endShare,
    s32 offsetY,
    PlaneTriangle *hit,
    f32 *fraction
) {
    const s16 *middle = ring->vertices[RING_CENTER].v.ob;
    const s16 *facing = ring->vertices[RING_FACING].v.ob;
    f32 normalX = facing[AXIS_X], normalY = facing[AXIS_Y], normalZ = facing[AXIS_Z];
    f32 startReach = startShare * RING_PATH_REACH, endReach = endShare * RING_PATH_REACH;

    guNormalize(&normalX, &normalY, &normalZ);
    return FindRingHit(
        (s32)(middle[AXIS_X] + (normalX * startReach)) << WORLD_TO_COURSE_SHIFT,
        (s32)(middle[AXIS_Y] + offsetY + (normalY * startReach)) << WORLD_TO_COURSE_SHIFT,
        (s32)(middle[AXIS_Z] + (normalZ * startReach)) << WORLD_TO_COURSE_SHIFT,
        (s32)(middle[AXIS_X] + (normalX * endReach)) << WORLD_TO_COURSE_SHIFT,
        (s32)(middle[AXIS_Y] + offsetY + (normalY * endReach)) << WORLD_TO_COURSE_SHIFT,
        (s32)(middle[AXIS_Z] + (normalZ * endReach)) << WORLD_TO_COURSE_SHIFT,
        hit,
        fraction
    );
}

// The ring hit radius of ring, in world units
static s32 GetRingRadius(const HoleTriangle *ring) {
    return ring->vertices[RING_SCALE].v.ob[AXIS_X] * RING_RADIUS_PER_SCALE;
}

// The hole's ringIndex'th ring, or NULL past its last
static HoleTriangle *FindRing(s32 ringIndex) {
    HoleTriangle *triangle;
    s32 i;

    for (i = 0; i < HOLE_TRIANGLE_COUNT; i++) {
        triangle = &g_WorkArea.hole.triangles[i];
        if (triangle->kind == HOLE_TRIANGLE_KIND_RING && ringIndex-- == 0) {
            return triangle;
        }
    }
    return NULL;
}

// A hit ring's tag is its index among the rings. Its corners are those of
// the hole's last ring, whichever was hit; callers read only the tag.
TEST(Collision, PathThroughARingsMiddleHitsIt) {
    PlaneTriangle hit;
    HoleTriangle *ring;
    f32 fraction;
    s32 ringIndex;

    TEST_ASSERT_TRUE(f_IsHoleLoaded);
    TEST_ASSERT_NOT_NULL(FindRing(0));
    for (ringIndex = 0; (ring = FindRing(ringIndex)) != NULL; ringIndex++) {
        TEST_ASSERT_EQUAL_INT32(1, SendThroughRing(ring, -1.0f, 1.0f, 0, &hit, &fraction));
        TEST_ASSERT_EQUAL_INT32(ringIndex, hit.tag);
        TEST_ASSERT_FLOAT_WITHIN(FRACTION_TOLERANCE, 0.5f, fraction);
    }
}

TEST(Collision, RingIsHitWithinItsRadius) {
    PlaneTriangle hit;
    HoleTriangle *ring;
    f32 fraction;
    s32 ringIndex, radius;

    TEST_ASSERT_TRUE(f_IsHoleLoaded);
    for (ringIndex = 0; (ring = FindRing(ringIndex)) != NULL; ringIndex++) {
        radius = GetRingRadius(ring);
        TEST_ASSERT_EQUAL_INT32(
            1,
            SendThroughRing(ring, -1.0f, 1.0f, radius * INSIDE_RING_SHARE, &hit, &fraction)
        );
        TEST_ASSERT_EQUAL_INT32(
            0,
            SendThroughRing(ring, -1.0f, 1.0f, radius * OUTSIDE_RING_SHARE, &hit, &fraction)
        );
    }
}

TEST(Collision, PathShortOfARingMissesIt) {
    PlaneTriangle hit;
    HoleTriangle *ring;
    f32 fraction;
    s32 ringIndex;

    TEST_ASSERT_TRUE(f_IsHoleLoaded);
    for (ringIndex = 0; (ring = FindRing(ringIndex)) != NULL; ringIndex++) {
        TEST_ASSERT_EQUAL_INT32(0, SendThroughRing(ring, -1.0f, -0.5f, 0, &hit, &fraction));
    }
}

TEST(Collision, PathOffTheGroundIsNotTestedForRings) {
    PlaneTriangle hit;
    f32 fraction;

    TEST_ASSERT_EQUAL_INT32(
        RING_PATH_OFF_GROUND,
        FindRingHit(-1, 0, 0, PROBE_MARGIN, 0, PROBE_MARGIN, &hit, &fraction)
    );
}

TEST(Collision, PathDownMeetsTheMiniGolfGroundAtItsHeight) {
    TEST_ASSERT_TRUE(f_IsHoleLoaded);
    f_MiniGolfBall.state = BALL_STATE_FLYING;
    AssertPathsDownMeetTheGround(FindMiniGolfBallHit);
}

TEST(Collision, PathAboveTheMiniGolfGroundMissesIt) {
    TEST_ASSERT_TRUE(f_IsHoleLoaded);
    f_MiniGolfBall.state = BALL_STATE_FLYING;
    AssertPathsAboveMissTheGround(FindMiniGolfBallHit);
}

TEST(Collision, MiniGolfGroundHitsAreTheMatchingBuilds) {
    s32 hitCount;
    u32 hash;

    TEST_ASSERT_TRUE(f_IsHoleLoaded);
    f_MiniGolfBall.state = BALL_STATE_FLYING;
    hash = HashGridHits(FindMiniGolfBallHit, &hitCount);
    TEST_ASSERT_EQUAL_INT32(PROBE_COLUMN_COUNT * PROBE_ROW_COUNT, hitCount);
    TEST_ASSERT_EQUAL_HEX32(MINI_GOLF_GROUND_HITS_HASH, hash);
}

// A rolling ball's path that crosses the ground by less than a bare
// crossing hits it at the path's end; a flying ball's hits it on the way
TEST(Collision, RollingBallBarelyCrossingTheMiniGolfGroundHitsAtItsEnd) {
    PlaneTriangle hit;
    f32 fraction;
    s32 column, row, x, z, height;

    TEST_ASSERT_TRUE(f_IsHoleLoaded);
    for (row = 0; row < PROBE_ROW_COUNT; row++) {
        for (column = 0; column < PROBE_COLUMN_COUNT; column++) {
            x = ProbeX(column);
            z = ProbeZ(row);
            height = GetGroundHeight(x, z);
            f_MiniGolfBall.state = BALL_STATE_ROLLING;
            TEST_ASSERT_TRUE(FindMiniGolfBallHit(
                x,
                height - BARE_CROSSING_REACH,
                z,
                x,
                height + BARE_CROSSING_REACH,
                z,
                &hit,
                &fraction
            ));
            TEST_ASSERT_EQUAL_FLOAT(1.0f, fraction);
            f_MiniGolfBall.state = BALL_STATE_FLYING;
            TEST_ASSERT_TRUE(FindMiniGolfBallHit(
                x,
                height - BARE_CROSSING_REACH,
                z,
                x,
                height + BARE_CROSSING_REACH,
                z,
                &hit,
                &fraction
            ));
            TEST_ASSERT_FLOAT_WITHIN(BARE_CROSSING_TOLERANCE, 0.5f, fraction);
        }
    }
}

TEST(Collision, WallHitsAreTheMatchingBuilds) {
    s32 hitCount;
    u32 hash;

    TEST_ASSERT_TRUE(f_IsHoleLoaded);
    hash = HashGridHits(FindWallHit, &hitCount);
    TEST_ASSERT_GREATER_THAN_INT32(0, hitCount);
    TEST_ASSERT_EQUAL_HEX32(WALL_HITS_HASH, hash);
}

// Puts f_MiniGolfBall on the ground at the middle of wall, moved heightY
// world units up from the ground
static void PutBallOnWall(const HoleTriangle *wall, s32 heightY) {
    s32 corner, x = 0, z = 0;

    for (corner = 0; corner < TRIANGLE_CORNER_COUNT; corner++) {
        x += wall->vertices[corner].v.ob[AXIS_X];
        z += wall->vertices[corner].v.ob[AXIS_Z];
    }
    bzero(&f_MiniGolfBall, sizeof(f_MiniGolfBall));
    f_MiniGolfBall.position[AXIS_X] = (x / TRIANGLE_CORNER_COUNT) << WORLD_TO_COURSE_SHIFT;
    f_MiniGolfBall.position[AXIS_Z] = (z / TRIANGLE_CORNER_COUNT) << WORLD_TO_COURSE_SHIFT;
    f_MiniGolfBall.position[AXIS_Y] =
        GetGroundHeight(f_MiniGolfBall.position[AXIS_X], f_MiniGolfBall.position[AXIS_Z]) - heightY;
}

static Boolean IsBallAt(const CoursePoint position) {
    return f_MiniGolfBall.position[AXIS_X] == position[AXIS_X] &&
        f_MiniGolfBall.position[AXIS_Y] == position[AXIS_Y] &&
        f_MiniGolfBall.position[AXIS_Z] == position[AXIS_Z];
}

// A ball on a wall is pushed out of every wall, so pushing it again leaves
// it where it is. The pushed balls are hashed.
TEST(Collision, BallOnAWallIsPushedOutOfTheWalls) {
    CoursePoint before;
    HoleTriangle *wall;
    u32 hash = FNV_OFFSET_BASIS;
    s32 i, pushedCount = 0;

    TEST_ASSERT_TRUE(f_IsHoleLoaded);
    for (i = 0; i < HOLE_TRIANGLE_COUNT; i++) {
        wall = &g_WorkArea.hole.triangles[i];
        if (wall->kind != HOLE_TRIANGLE_KIND_SHADED) {
            continue;
        }
        PutBallOnWall(wall, 0);
        memcpy(before, f_MiniGolfBall.position, sizeof(before));
        PushBallOutOfWalls(&f_MiniGolfBall, FALSE);
        pushedCount += !IsBallAt(before);
        hash = HashBytes(hash, f_MiniGolfBall.position, sizeof(f_MiniGolfBall.position));
        memcpy(before, f_MiniGolfBall.position, sizeof(before));
        PushBallOutOfWalls(&f_MiniGolfBall, FALSE);
        TEST_ASSERT_TRUE(IsBallAt(before));
    }
    TEST_ASSERT_GREATER_THAN_INT32(0, pushedCount);
    TEST_ASSERT_EQUAL_HEX32(WALL_PUSHES_HASH, hash);
}

TEST_GROUP_RUNNER(Collision) {
    RUN_TEST_CASE(Collision, PathBeyondTheFlagsReachMissesThePin);
    RUN_TEST_CASE(Collision, PathThroughTheFlagstickBouncesBack);
    RUN_TEST_CASE(Collision, FlyingBallBouncesOffTheFlagstickSlower);
    RUN_TEST_CASE(Collision, SteepFallBouncesOffTheFlagstickWithTheLeastSpeed);
    RUN_TEST_CASE(Collision, FlagstickReachesItsRadius);
    RUN_TEST_CASE(Collision, PinReachesUpToTheFlagstickHeight);
    RUN_TEST_CASE(Collision, PathThroughTheFlagStopsTheBall);
    RUN_TEST_CASE(Collision, FlagReachesPastTheFlagstick);
    RUN_TEST_CASE(Collision, FlagReachesItsEdge);
    RUN_TEST_CASE(Collision, RisingBallHitsTheFlagstickAtTheFlag);
    RUN_TEST_CASE(Collision, FlagIsHitOnlyOnce);
    RUN_TEST_CASE(Collision, PathEndingInsideTheFlagstickHitsAtItsEnd);
    RUN_TEST_CASE(Collision, BallNearTheCupMovesOutToItsEdge);
    RUN_TEST_CASE(Collision, BallMovesOutAlongItsWayFromTheCup);
    RUN_TEST_CASE(Collision, BallAtTheCupOrBeyondItsEdgeStays);
    RUN_TEST_CASE(Collision, MaskCellsStopAtTheMasksEdges);
    RUN_TEST_CASE(Collision, MaskCellsAreTheMatchingBuilds);
    RUN_TEST_CASE(Collision, FilledMasksMakeEveryCellATrunk);

    // The rest only read the hole, so they share one load. Collision
    // doesn't see the ground's shading, which takes most of a load.
    ClearAllGameFlags();
    SetGameFlag(GAME_FLAG_UNSHADED_GROUND);
    f_IsHoleLoaded = LoadTestHole(TEST_COURSE, TEST_HOLE);
    RUN_TEST_CASE(Collision, PathDownMeetsTheGroundAtItsHeight);
    RUN_TEST_CASE(Collision, PathAboveTheGroundMissesIt);
    RUN_TEST_CASE(Collision, PathThroughABillboardHitsItsMasksCell);
    RUN_TEST_CASE(Collision, GroundHitsAreTheMatchingBuilds);
    RUN_TEST_CASE(Collision, HoleTriangleHitsAreTheMatchingBuilds);
    RUN_TEST_CASE(Collision, BillboardHitsAreTheMatchingBuilds);
    RUN_TEST_CASE(Collision, PathThroughARingsMiddleHitsIt);
    RUN_TEST_CASE(Collision, RingIsHitWithinItsRadius);
    RUN_TEST_CASE(Collision, PathShortOfARingMissesIt);
    RUN_TEST_CASE(Collision, PathOffTheGroundIsNotTestedForRings);

    f_IsHoleLoaded = LoadTestHole(MINI_GOLF_TEST_COURSE, MINI_GOLF_TEST_HOLE);
    RUN_TEST_CASE(Collision, PathDownMeetsTheMiniGolfGroundAtItsHeight);
    RUN_TEST_CASE(Collision, PathAboveTheMiniGolfGroundMissesIt);
    RUN_TEST_CASE(Collision, MiniGolfGroundHitsAreTheMatchingBuilds);
    RUN_TEST_CASE(Collision, RollingBallBarelyCrossingTheMiniGolfGroundHitsAtItsEnd);
    RUN_TEST_CASE(Collision, WallHitsAreTheMatchingBuilds);
    RUN_TEST_CASE(Collision, BallOnAWallIsPushedOutOfTheWalls);
}
