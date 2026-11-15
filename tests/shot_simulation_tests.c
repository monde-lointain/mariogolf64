/*
 * Characterizes shot_simulation.c's SimulateShot, which flies a shot with
 * the ball simulation over a loaded hole to show where it will land. The
 * tests strike from Toad Highlands' first hole, as Plum, in calm weather
 * unless they change it. Some check what any golf game should do, such as
 * rain shortening a drive; the rest keep the matching build's values. The
 * putts that drop have edges in power and heading, so the tests take putts
 * on both sides of each.
 */

#include <ultra64.h>

#include "characters/character.h"
#include "course/course.h"
#include "course/hole_conditions.h"
#include "course/hole_setup.h"
#include "course/surface.h"
#include "course/terrain.h"
#include "debug/game_flags.h"
#include "hole_fixture.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "round/player.h"
#include "shot/ball_simulation.h"
#include "shot/club.h"
#include "shot/current_shot.h"
#include "shot/shot_simulation.h"
#include "system/overlay_manager.h"
#include "unity_fixture.h"

#define TEST_PLAYER 0
#define TEST_CHARACTER CHARACTER_PLUM

// A full drive onto the fairway, which the tests vary
static const ShotLaunch f_Drive = {
    .heading = 0xC400,
    .clubSlot = CLUB_TYPE_DRIVER,
    .power = LAUNCH_POWER_MAX,
    .impactTiming = 0,
    .playerIndex = TEST_PLAYER,
    .shotMode = SHOT_MODE_NORMAL,
};

// A putt that drops, from the green beside the default cup
#define PUTT_OFFSET_X 0x6000
#define PUTT_OFFSET_Z 0x3000
static const ShotLaunch f_Putt = {
    .heading = 0x9200,
    .clubSlot = CLUB_TYPE_PUTTER,
    .power = 0x40,
    .impactTiming = 0,
    .playerIndex = TEST_PLAYER,
    .shotMode = PUTT_MODE_SHORT,
};

// The wind that the wind tests blow, in each of four directions
#define TEST_WIND_SPEED (6 * WIND_SPEED_UNIT)
#define TEST_WIND_COUNT 4
static const BinaryAngle f_WindDirections[TEST_WIND_COUNT] = {0x0000, 0x4000, 0x8000, 0xC000};
// Which way each of those blows, as the signs of x and z
static const s32 f_DownwindSigns[TEST_WIND_COUNT][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
// The drive flies toward -z, so this one blows along it
#define TAILWIND_INDEX 3

// Whether the hole and the simulation's overlay loaded for the group
static Boolean f_IsReady;

// Give the test player TEST_CHARACTER's clubs, a slot for each club type.
// The simulation looks some clubs up by slot and others by the slot's club,
// which the first character's set keeps the same.
static void GiveClubs(void) {
    Player *player = &g_Players[TEST_PLAYER];
    s32 slot;

    bzero(player, sizeof(*player));
    player->characterIndex = TEST_CHARACTER;
    for (slot = 0; slot < CLUB_SLOT_COUNT; slot++) {
        player->clubs[slot] =
            slot < CLUB_SET_SIZE ? TEST_CHARACTER * CLUB_SET_SIZE + slot : NO_CLUB;
    }
}

// Put the ball on the ground at x and z, as the shot and the player see it
static void PlaceBall(s32 x, s32 z) {
    Player *player = &g_Players[TEST_PLAYER];

    g_CurrentShot.ballPosition[AXIS_X] = player->ballPosition[AXIS_X] = x;
    g_CurrentShot.ballPosition[AXIS_Y] = player->ballPosition[AXIS_Y] = GetGroundHeight(x, z);
    g_CurrentShot.ballPosition[AXIS_Z] = player->ballPosition[AXIS_Z] = z;
    g_CurrentShot.surface = player->ballSurface = GetSurface(x, z);
}

static void PlaceBallForPutt(void) {
    PlaceBall(
        g_HoleSetup.cupPosition[AXIS_X] + PUTT_OFFSET_X,
        g_HoleSetup.cupPosition[AXIS_Z] + PUTT_OFFSET_Z
    );
}

// Simulate shot from the ball into the last shot's result, as the game does
static void SimulateOverLastShot(ShotLaunch shot) {
    g_CurrentShot.playerIndex = shot.playerIndex;
    g_CurrentShot.clubSlot = shot.clubSlot;
    g_CurrentShot.shotMode = shot.shotMode;
    SimulateShot(&shot);
}

// Simulate shot from the ball. The simulated shot is cleared first, so that
// what the tests read of it is this shot's.
static void Simulate(ShotLaunch shot) {
    bzero(&g_SimulatedShot, sizeof(g_SimulatedShot));
    SimulateOverLastShot(shot);
}

static f32 HorizontalDistance(const s32 *from, const s32 *to) {
    f32 x = to[AXIS_X] - from[AXIS_X];
    f32 z = to[AXIS_Z] - from[AXIS_Z];

    return sqrtf(x * x + z * z);
}

// How far the last simulated shot went, along the ground
static f32 ShotDistance(void) {
    return HorizontalDistance(g_SimulatedShot.startPosition, g_SimulatedShot.endPosition);
}

// How many steps of the last simulated shot's path were kept; a longer
// flight keeps only its first PATH_STEP_COUNT
static s32 KeptStepCount(void) {
    return MIN(g_SimulatedShot.stepCount, PATH_STEP_COUNT);
}

// The step at which the last simulated shot first hit the ground, or its
// last kept step if it never did
static s32 FirstGroundHitStep(void) {
    s32 lastStep = KeptStepCount() - 1;
    s32 step = 0;

    while (step < lastStep && !(g_SimulatedShot.pathFlags[step] & STEP_FLAG_GROUND_HIT)) {
        step++;
    }
    return step;
}

// How far the last simulated shot flew before it first hit the ground
static f32 ShotCarry(void) {
    return HorizontalDistance(
        g_SimulatedShot.startPosition,
        g_SimulatedShot.path[FirstGroundHitStep()]
    );
}

// How far the last simulated shot went after it first hit the ground
static f32 ShotRoll(void) {
    return HorizontalDistance(
        g_SimulatedShot.path[FirstGroundHitStep()],
        g_SimulatedShot.endPosition
    );
}

TEST_GROUP(ShotSimulation);

TEST_SETUP(ShotSimulation) {
    TEST_ASSERT_TRUE_MESSAGE(f_IsReady, "The hole or the shot simulation didn't load");
    GiveClubs();
    g_HoleSetup.conditions.windSpeed = 0;
    g_HoleSetup.conditions.windDirection = 0;
    g_HoleSetup.conditions.isRaining = FALSE;
    PlaceBall(g_HoleSetup.teePosition[AXIS_X], g_HoleSetup.teePosition[AXIS_Z]);
}

TEST_TEAR_DOWN(ShotSimulation) {
}

// The fairway gives the ball a lie where it stops; this drive's is the
// fairway's fair lie
TEST(ShotSimulation, DriveFindsTheFairway) {
    static const CoursePoint end = {0x24C8DA, -0x38F07, 0x3FF073};

    Simulate(f_Drive);

    TEST_ASSERT_EQUAL_INT(SURFACE_FAIRWAY_FAIR_LIE, g_SimulatedShot.endSurface);
    TEST_ASSERT_EQUAL_HEX32_ARRAY(end, g_SimulatedShot.endPosition, AXIS_COUNT);
    TEST_ASSERT_EQUAL_INT(0x113, g_SimulatedShot.stepCount);
}

TEST(ShotSimulation, LongerClubsCarryFarther) {
    f32 carries[CLUB_TYPE_PUTTER];
    ShotLaunch shot = f_Drive;
    s32 club;

    for (club = CLUB_TYPE_DRIVER; club < CLUB_TYPE_PUTTER; club++) {
        shot.clubSlot = club;
        Simulate(shot);
        carries[club] = ShotCarry();
    }

    for (club = CLUB_TYPE_DRIVER + 1; club < CLUB_TYPE_PUTTER; club++) {
        TEST_ASSERT_LESS_THAN_FLOAT(carries[club - 1], carries[club]);
    }
}

TEST(ShotSimulation, ShotModesSetTheDistance) {
    ShotLaunch shot = f_Drive;
    f32 power, normal, approach;

    shot.shotMode = SHOT_MODE_POWER;
    Simulate(shot);
    power = ShotDistance();
    Simulate(f_Drive);
    normal = ShotDistance();
    shot.shotMode = SHOT_MODE_APPROACH;
    Simulate(shot);
    approach = ShotDistance();

    TEST_ASSERT_GREATER_THAN_FLOAT(normal, power);
    TEST_ASSERT_LESS_THAN_FLOAT(normal, approach);
}

TEST(ShotSimulation, HalfPowerFallsShort) {
    ShotLaunch shot = f_Drive;
    f32 full;

    Simulate(f_Drive);
    full = ShotDistance();
    shot.power = LAUNCH_POWER_MAX / 2;
    Simulate(shot);

    TEST_ASSERT_LESS_THAN_FLOAT(full, ShotDistance());
}

TEST(ShotSimulation, MistimedSwingsFallShort) {
    ShotLaunch shot = f_Drive;
    f32 onTime;

    Simulate(f_Drive);
    onTime = ShotDistance();

    shot.impactTiming = -8;
    Simulate(shot);
    TEST_ASSERT_LESS_THAN_FLOAT(onTime, ShotDistance());
    shot.impactTiming = 8;
    Simulate(shot);
    TEST_ASSERT_LESS_THAN_FLOAT(onTime, ShotDistance());
}

// Rain slows the ball's launch, takes some of its spin, and slows its roll
TEST(ShotSimulation, RainShortensTheCarry) {
    f32 dry;

    Simulate(f_Drive);
    dry = ShotCarry();
    g_HoleSetup.conditions.isRaining = TRUE;
    Simulate(f_Drive);

    TEST_ASSERT_LESS_THAN_FLOAT(dry, ShotCarry());
}

TEST(ShotSimulation, RainShortensTheRoll) {
    f32 dry;

    Simulate(f_Drive);
    dry = ShotRoll();
    g_HoleSetup.conditions.isRaining = TRUE;
    Simulate(f_Drive);

    TEST_ASSERT_LESS_THAN_FLOAT(dry, ShotRoll());
}

TEST(ShotSimulation, WindCarriesTheBallDownwind) {
    CoursePoint calmEnd;
    s32 wind, axis;
    f32 downwind;

    Simulate(f_Drive);
    for (axis = 0; axis < AXIS_COUNT; axis++) {
        calmEnd[axis] = g_SimulatedShot.endPosition[axis];
    }

    g_HoleSetup.conditions.windSpeed = TEST_WIND_SPEED;
    for (wind = 0; wind < TEST_WIND_COUNT; wind++) {
        g_HoleSetup.conditions.windDirection = f_WindDirections[wind];
        Simulate(f_Drive);
        downwind = (f32)(g_SimulatedShot.endPosition[AXIS_X] - calmEnd[AXIS_X]) *
                f_DownwindSigns[wind][0] +
            (f32)(g_SimulatedShot.endPosition[AXIS_Z] - calmEnd[AXIS_Z]) * f_DownwindSigns[wind][1];
        TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, downwind);
    }
}

// How much farther a tailwind carries the drive than calm air does, in the
// weather given. Dry weather cuts the wind's push along the shot; rain
// leaves it whole.
static f32 TailwindDrift(Boolean isRaining) {
    f32 calmZ;

    g_HoleSetup.conditions.isRaining = isRaining;
    g_HoleSetup.conditions.windSpeed = 0;
    Simulate(f_Drive);
    calmZ = g_SimulatedShot.endPosition[AXIS_Z];
    g_HoleSetup.conditions.windSpeed = TEST_WIND_SPEED;
    g_HoleSetup.conditions.windDirection = f_WindDirections[TAILWIND_INDEX];
    Simulate(f_Drive);
    return calmZ - g_SimulatedShot.endPosition[AXIS_Z];
}

TEST(ShotSimulation, RainLetsTheTailwindPushHarder) {
    f32 dryDrift = TailwindDrift(FALSE);

    TEST_ASSERT_GREATER_THAN_FLOAT(dryDrift, TailwindDrift(TRUE));
}

// The game simulates shot after shot into the same result, without clearing
// it, so none of a shot's result may depend on the one before. The putt
// keeps fewer steps than the drive, so the drive's steps cover all it left.
TEST(ShotSimulation, SimulatingAgainGivesTheSameFlight) {
    u32 first;

    Simulate(f_Drive);
    first = HashBytes(FNV_OFFSET_BASIS, &g_SimulatedShot, sizeof(g_SimulatedShot));
    PlaceBallForPutt();
    SimulateOverLastShot(f_Putt);
    PlaceBall(g_HoleSetup.teePosition[AXIS_X], g_HoleSetup.teePosition[AXIS_Z]);
    SimulateOverLastShot(f_Drive);

    TEST_ASSERT_EQUAL_HEX32(
        first,
        HashBytes(FNV_OFFSET_BASIS, &g_SimulatedShot, sizeof(g_SimulatedShot))
    );
}

TEST(ShotSimulation, PuttOnTheLineDrops) {
    PlaceBallForPutt();
    Simulate(f_Putt);

    TEST_ASSERT_TRUE(g_SimulatedShot.endsInCup);
    TEST_ASSERT_EQUAL_INT(0x38, g_SimulatedShot.stepCount);
}

TEST(ShotSimulation, SoftPuttStopsShort) {
    ShotLaunch putt = f_Putt;
    f32 startDistance;

    PlaceBallForPutt();
    startDistance = HorizontalDistance(g_CurrentShot.ballPosition, g_HoleSetup.cupPosition);
    putt.power /= 2;
    Simulate(putt);

    TEST_ASSERT_FALSE(g_SimulatedShot.endsInCup);
    TEST_ASSERT_TRUE(g_SimulatedShot.endsOnGreen);
    TEST_ASSERT_LESS_THAN_FLOAT(
        startDistance,
        HorizontalDistance(g_SimulatedShot.endPosition, g_HoleSetup.cupPosition)
    );
}

// Putts on either side of the edges of the putts that drop, as the matching
// build plays them. Too soft stops short and too hard lips out; along the
// heading, the edge is ragged where the ball rides the lip. A small change
// in the arithmetic moves these edges first.
typedef struct PuttEdgeCase {
    const char *name;
    BinaryAngle heading;
    s32 power;
    Boolean dropsInCup;
} PuttEdgeCase;

static const PuttEdgeCase f_PuttEdgeCases[] = {
    {"Too soft", 0x9200, 0x33, FALSE},
    {"Softest that drops", 0x9200, 0x34, TRUE},
    {"Hardest that drops", 0x9200, 0x6E, TRUE},
    {"Too hard", 0x9200, 0x6F, FALSE},
    {"Heading too low", 0x906E, 0x40, FALSE},
    {"Lowest heading that drops", 0x906F, 0x40, TRUE},
    {"Lips out", 0x9080, 0x40, FALSE},
    {"Drops after the lip", 0x9084, 0x40, TRUE},
    {"Highest heading that drops", 0x95C4, 0x40, TRUE},
    {"Heading too high", 0x95C5, 0x40, FALSE},
};

TEST(ShotSimulation, PuttsDropOnlyInsideTheEdges) {
    ShotLaunch putt = f_Putt;
    s32 i;

    PlaceBallForPutt();
    for (i = 0; i < ARRAY_COUNT(f_PuttEdgeCases); i++) {
        putt.heading = f_PuttEdgeCases[i].heading;
        putt.power = f_PuttEdgeCases[i].power;
        Simulate(putt);
        TEST_ASSERT_EQUAL_INT_MESSAGE(
            f_PuttEdgeCases[i].dropsInCup,
            g_SimulatedShot.endsInCup,
            f_PuttEdgeCases[i].name
        );
    }
}

// The bounds cover the path, and the steps that set them reach them. A
// bound that the ball never passes stays at the start, with step 0.
TEST(ShotSimulation, BoundsCoverThePath) {
    s32 keptStepCount, step, axis;
    const s32 *start = g_SimulatedShot.startPosition;

    Simulate(f_Drive);
    keptStepCount = KeptStepCount();

    for (axis = 0; axis < AXIS_COUNT; axis++) {
        s32 max = g_SimulatedShot.positionMax[axis];
        s32 min = g_SimulatedShot.positionMin[axis];

        for (step = 0; step < keptStepCount; step++) {
            TEST_ASSERT_LESS_OR_EQUAL_INT32(max, g_SimulatedShot.path[step][axis]);
            TEST_ASSERT_GREATER_OR_EQUAL_INT32(min, g_SimulatedShot.path[step][axis]);
        }
        if (max != start[axis]) {
            TEST_ASSERT_EQUAL_INT32(
                max,
                g_SimulatedShot.path[g_SimulatedShot.positionStepMax[axis]][axis]
            );
        }
        if (min != start[axis]) {
            TEST_ASSERT_EQUAL_INT32(
                min,
                g_SimulatedShot.path[g_SimulatedShot.positionStepMin[axis]][axis]
            );
        }
    }
}

// Shots with a sample of the clubs, each as the hash of all that the
// simulation keeps of it: the driver, a long and a short iron, the sand
// wedge and the putter. The whole set would take about two seconds more.
#define KEPT_FLIGHT_COUNT 5

TEST(ShotSimulation, ClubsKeepTheirFlights) {
    static const u32 expected[KEPT_FLIGHT_COUNT] = {
        0xD7F34170,
        0x3EABF298,
        0x68510ABB,
        0xCC49229B,
        0x958CD2D8,
    };
    static const ClubSlot clubs[KEPT_FLIGHT_COUNT - 1] = {
        CLUB_TYPE_DRIVER,
        CLUB_TYPE_THREE_IRON,
        CLUB_TYPE_EIGHT_IRON,
        CLUB_TYPE_SAND_WEDGE,
    };
    u32 hashes[KEPT_FLIGHT_COUNT];
    ShotLaunch shot = f_Drive;
    s32 flight;

    for (flight = 0; flight < KEPT_FLIGHT_COUNT - 1; flight++) {
        shot.clubSlot = clubs[flight];
        Simulate(shot);
        hashes[flight] = HashBytes(FNV_OFFSET_BASIS, &g_SimulatedShot, sizeof(g_SimulatedShot));
    }
    PlaceBallForPutt();
    Simulate(f_Putt);
    hashes[flight] = HashBytes(FNV_OFFSET_BASIS, &g_SimulatedShot, sizeof(g_SimulatedShot));

    TEST_ASSERT_EQUAL_HEX32_ARRAY(expected, hashes, KEPT_FLIGHT_COUNT);
}

TEST_GROUP_RUNNER(ShotSimulation) {
    s32 overlayId;

    // The tests only read the hole, so they share one load. The ball doesn't
    // see the ground's shading, which takes most of a load.
    ClearAllGameFlags();
    SetGameFlag(GAME_FLAG_UNSHADED_GROUND);
    f_IsReady = LoadTestHole(COURSE_TOAD_HIGHLANDS, 0);
    InitClubs();
    overlayId = GetOverlayOfModule(MODULE_SHOT_SIMULATION);
    f_IsReady = f_IsReady && LoadOverlay(overlayId);
    RUN_TEST_CASE(ShotSimulation, DriveFindsTheFairway);
    RUN_TEST_CASE(ShotSimulation, LongerClubsCarryFarther);
    RUN_TEST_CASE(ShotSimulation, ShotModesSetTheDistance);
    RUN_TEST_CASE(ShotSimulation, HalfPowerFallsShort);
    RUN_TEST_CASE(ShotSimulation, MistimedSwingsFallShort);
    RUN_TEST_CASE(ShotSimulation, RainShortensTheCarry);
    RUN_TEST_CASE(ShotSimulation, RainShortensTheRoll);
    RUN_TEST_CASE(ShotSimulation, WindCarriesTheBallDownwind);
    RUN_TEST_CASE(ShotSimulation, RainLetsTheTailwindPushHarder);
    RUN_TEST_CASE(ShotSimulation, SimulatingAgainGivesTheSameFlight);
    RUN_TEST_CASE(ShotSimulation, PuttOnTheLineDrops);
    RUN_TEST_CASE(ShotSimulation, SoftPuttStopsShort);
    RUN_TEST_CASE(ShotSimulation, PuttsDropOnlyInsideTheEdges);
    RUN_TEST_CASE(ShotSimulation, BoundsCoverThePath);
    RUN_TEST_CASE(ShotSimulation, ClubsKeepTheirFlights);
    UnloadOverlay(overlayId);
}
