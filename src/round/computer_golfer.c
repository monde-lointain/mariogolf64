/*
 * The computer golfer, which plans its shot on a thread of its own by trying
 * shots in the ball simulation.
 */

#include "round/computer_golfer.h"

#include <math.h>
#include <stdlib.h>
#include <ultra64.h>

#include "characters/character.h"
#include "course/collision.h"
#include "course/course.h"
#include "course/course_space.h"
#include "course/hole_conditions.h"
#include "course/hole_loader.h"
#include "course/hole_setup.h"
#include "course/surface.h"
#include "course/terrain.h"
#include "debug/debug_text.h"
#include "debug/game_flags.h"
#include "game/scene.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "round/bets.h"
#include "round/game_state.h"
#include "round/play.h"
#include "round/player.h"
#include "save/save.h"
#include "shot/ball_simulation.h"
#include "shot/club.h"
#include "shot/current_shot.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/thread_wait.h"

enum PlanThreadState {
    PLAN_THREAD_RUNNING,

    // The shot planning thread's states, in f_PlanThreadState
    PLAN_THREAD_COLLECTED,
    PLAN_THREAD_DONE
};

typedef s32 PlanThreadState;
#define PLAN_THREAD_ID 11
#define PLAN_THREAD_PRIORITY 20
#define PLAN_THREAD_STACK_SIZE 0x6000
// PlanShot's result while it has more to do
#define PLAN_UNFINISHED (-1)

// ChooseShotOffset's results, indices into f_ShotOffsets. It never chooses
// the table's last offset, SHOT_OFFSET_UNUSED
enum ShotOffsetIndex {
    SHOT_OFFSET_NONE,
    SHOT_OFFSET_PLUS_SMALL,
    SHOT_OFFSET_PLUS_MEDIUM,
    SHOT_OFFSET_PLUS_LARGE,
    SHOT_OFFSET_MINUS_SMALL,
    SHOT_OFFSET_MINUS_MEDIUM,
    SHOT_OFFSET_MINUS_LARGE,
    SHOT_OFFSET_UNUSED,
    SHOT_OFFSET_COUNT
};

typedef s32 ShotOffsetIndex;

// The evaluation of a lost ball, and the one that the best starts at
#define LOST_BALL_EVALUATION 0x10000000
#define SORT_DESCENDING 1
// How far below the paths the ground test starts
#define PATH_GROUND_CLEARANCE 0x600
// FindGreenEdgeOnLine's results
#define GREEN_EDGE_FOUND 1

// GolferTraits.strongNine values
enum StrongNine {
    STRONG_NINE_NEITHER,
    STRONG_NINE_FRONT, // Strokes move from the front nine to the back
    STRONG_NINE_BACK // Strokes move from the back nine to the front
};

// GolferTraits.consistency values
enum Consistency {
    CONSISTENCY_LOW,
    CONSISTENCY_MIDDLE,
    CONSISTENCY_HIGH,
    CONSISTENCY_COUNT
};

// GolferTraits.windPlay values
enum WindPlay {
    WIND_PLAY_HEAD_WINDS, // Offsets shots for head winds
    // Offsets shots for the strongest winds either way
    WIND_PLAY_STRONG_WINDS,
    WIND_PLAY_FOLLOWING_WINDS // Offsets shots for following winds
};

typedef s32 StrongNine;
typedef s8 StrongNine8;
typedef s32 Consistency;
typedef s8 Consistency8;
typedef s32 WindPlay;
typedef s8 WindPlay8;

// How a character plays when the computer controls it
typedef struct GolferTraits {
    StrongNine8 strongNine;
    Consistency8 consistency; // Steadier golfers mishit less
    s8 clubBoldness; // BOLD_CLUB_CHOICE or less
    s8 ignoredBoldnessValue; // 6, or 12 when clubBoldness is above 0; nothing reads it
    s8 ignoredTraitLevel; // 0 to 2; nothing reads it
    WindPlay8 windPlay; // Which winds the golfer offsets shots for
    s8 powerShotEagerness; // 0 to 2; only nonzero is tested
    s8 courseTargetOffsets[COURSE_COUNT]; // Added to baseTargetScore on each course
    s8 baseTargetScore; // Its target score to par before the course's offset
} GolferTraits;

// The GolferTraits.clubBoldness of a golfer who chooses clubs regardless of poor lies, swings
// harder at planned shots and takes power shots readily
#define BOLD_CLUB_CHOICE 2

// The spin-like launch values that each shot offset gives a shot
typedef struct ShotOffset {
    s8 offsetX; // Becomes f_Shot.offsetX
    s8 offsetY; // Becomes f_Shot.offsetY
} ShotOffset;

#define APPROACH_BAND_COUNT 4

// The rows of f_ClubKindChances
enum ClubChanceSet {
    CLUB_CHANCE_SET_FOLLOWING_WIND,
    CLUB_CHANCE_SET_HEAD_WIND,
    CLUB_CHANCE_SET_COUNT
};

typedef s32 ClubChanceSet;

// Woods, irons and the other clubs
enum ClubChanceCategory {
    CLUB_CHANCE_WOOD,
    CLUB_CHANCE_IRON,
    CLUB_CHANCE_OTHER,
    CLUB_CHANCE_CATEGORY_COUNT
};

typedef s32 ClubChanceCategory;

// A club's name in the table that no routine reads
typedef struct ClubName {
    char name[4];
    ClubType clubType;
} ClubName;

// The directions from the ball, in BINARY_ANGLE_TURN / DIRECTION_SECTOR_COUNT
// sectors
#define DIRECTION_SECTOR_COUNT 256
#define DIRECTION_SECTOR_SHIFT 8

// The route points and the tee, as PlanRoute gathers them
typedef struct RouteCoordinates {
    s32 x[ROUTE_POINT_COUNT + 1];
    s32 z[ROUTE_POINT_COUNT + 1];
} RouteCoordinates;

enum PlanStage {
    PLAN_STAGE_UNSTARTED,
    PLAN_STAGE_STARTED,
    PLAN_STAGE_TARGET_CHOSEN,
    PLAN_STAGE_TRIAL_TAKEN,
    PLAN_STAGE_UNUSED, // Never set; IsTrialStage counts it
    PLAN_STAGE_LEVEL_DONE,
    PLAN_STAGE_TRIAL_RUNNING
};

typedef s8 PlanStage;

// The traits of each character, by CharacterIndex
static GolferTraits f_GolferTraits[CHARACTER_COUNT] = {
    // Plum
    {0, 0, 0, 6, 0, WIND_PLAY_STRONG_WINDS, 0, {0, 2, 2, 2, 2, 2}, 10},
    // Charlie
    {2, 0, 0, 6, 0, WIND_PLAY_HEAD_WINDS, 0, {0, 2, 2, 2, 2, 2}, 8},
    // Maple
    {0, 0, 0, 6, 0, WIND_PLAY_STRONG_WINDS, 0, {-2, 0, 0, 2, 0, 2}, -3},
    // Baby Mario
    {1, 1, 1, 12, 1, WIND_PLAY_FOLLOWING_WINDS, 1, {0, 2, 2, 2, 2, 2}, 4},
    // Luigi
    {0, 0, 0, 6, 0, WIND_PLAY_HEAD_WINDS, 0, {0, 2, 2, 2, 2, 2}, 2},
    // Peach
    {0, 0, 0, 6, 1, WIND_PLAY_STRONG_WINDS, 1, {0, 2, 2, 2, 2, 2}, 6},
    // Sonny
    {2, 2, 2, 12, 2, WIND_PLAY_FOLLOWING_WINDS, 2, {0, 0, 0, 2, 2, 2}, -2},
    // Wario
    {1, 1, 2, 12, 0, WIND_PLAY_STRONG_WINDS, 2, {-2, 0, 0, 0, 0, 2}, -4},
    // Harry
    {2, 2, 1, 12, 1, WIND_PLAY_FOLLOWING_WINDS, 1, {-2, -2, 0, 2, 0, 0}, -6},
    // Mario
    {0, 1, 0, 6, 2, WIND_PLAY_STRONG_WINDS, 1, {-2, -2, -2, 0, 2, 0}, -9},
    // Yoshi
    {0, 0, 0, 6, 0, WIND_PLAY_HEAD_WINDS, 0, {0, 0, 2, -2, 2, 2}, 0},
    // Donkey Kong
    {0, 0, 1, 12, 1, WIND_PLAY_HEAD_WINDS, 2, {-2, -2, 0, 2, 2, 0}, -11},
    // Bowser
    {0, 2, 2, 12, 2, WIND_PLAY_FOLLOWING_WINDS, 2, {-2, 2, -2, 2, -2, 0}, -12},
    // Metal Mario
    {0, 2, 2, 12, 2, WIND_PLAY_FOLLOWING_WINDS, 2, {-2, -2, -2, 2, -2, 0}, -16},
    // Kid
    {0, 0, 0, 6, 0, WIND_PLAY_HEAD_WINDS, 0, {0, 0, 0, 0, 0, 0}, 0},
    // Sherry
    {0, 0, 0, 6, 0, WIND_PLAY_HEAD_WINDS, 0, {0, 0, 0, 0, 0, 0}, 0},
    // Azalea
    {0, 0, 0, 6, 0, WIND_PLAY_HEAD_WINDS, 0, {0, 0, 0, 0, 0, 0}, 0},
    // Joe
    {0, 0, 0, 6, 0, WIND_PLAY_HEAD_WINDS, 0, {0, 0, 0, 0, 0, 0}, 0},
};
static s8 f_ClubCategoryChances[CLUB_CHANCE_SET_COUNT][CLUB_CHANCE_CATEGORY_COUNT] = {
    {75, 50, 25},
    {75, 25, 50},
};
static ShotOffset f_ShotOffsets[SHOT_OFFSET_COUNT] = {
    {0, 0},
    {0, 16},
    {0, 32},
    {0, 48},
    {0, -16},
    {0, -32},
    {0, -48},
    {0, 0},
};
static PlanThreadState f_PlanThreadState = PLAN_THREAD_RUNNING;
static u32 f_UnusedWordAfterThreadState UNUSED = 0xD24CC5E5;
// Whether there is a best evaluation yet
static Boolean f_HasBestEvaluation = FALSE;
// The upper limits of each approach band, as shares of the cup distance that
// lie short of the green, for each value of the golfer's consistency
static f32 f_ApproachBandLimits[APPROACH_BAND_COUNT][CONSISTENCY_COUNT] = {
    {0.2f, 0.15f, 0.1f},
    {0.25f, 0.25f, 0.25f},
    {0.4f, 0.45f, 0.35f},
    {0.6f, 0.7f, 0.5f},
};
// Whether the trial ball, falling before its first bounce, hit the ground or a
// billboard, on a later trial level with no evaluation kept yet
static Boolean f_HasFirstFallHit = FALSE;
// Read by no routine. The 2W and AW share the 3W's and SW's types.
static ClubName f_UnusedPlanClubNames[] UNUSED = {
    {"1W", CLUB_TYPE_DRIVER},
    {"3W", CLUB_TYPE_THREE_WOOD},
    {"4W", CLUB_TYPE_FOUR_WOOD},
    {"2I", CLUB_TYPE_TWO_IRON},
    {"3I", CLUB_TYPE_THREE_IRON},
    {"4I", CLUB_TYPE_FOUR_IRON},
    {"5I", CLUB_TYPE_FIVE_IRON},
    {"6I", CLUB_TYPE_SIX_IRON},
    {"7I", CLUB_TYPE_SEVEN_IRON},
    {"8I", CLUB_TYPE_EIGHT_IRON},
    {"9I", CLUB_TYPE_NINE_IRON},
    {"PW", CLUB_TYPE_PITCHING_WEDGE},
    {"SW", CLUB_TYPE_SAND_WEDGE},
    {"PT", CLUB_TYPE_PUTTER},
    {"AW", CLUB_TYPE_SAND_WEDGE},
    {"2W", CLUB_TYPE_THREE_WOOD},
};

s32 PlanShot(CurrentShot *plannedShot);
ClubType ChooseClubForDistance(s32 distance);

s32 StepTrialShot(void);
Boolean IsTrialShotSafe(void);
ShotOffsetIndex ChooseShotOffset(void);
ClubSlot FindClubSlot(s32 unused, ClubType clubType);
s32 PlanRoute(s8 *route);
void ChooseLandingTarget(s32 x, s32 z, s32 distance, s32 *targetX, s32 *targetZ);
void PlanClubAndTarget(CoursePoint target);
void ChooseLongShot(s32 unused, ClubType *clubType, s32 *targetX, s32 *targetZ, ShotMode *shotMode);
s32 ChooseHoleStrokeTarget(void);

static OSThread f_PlanThread;
static CurrentShot f_PlannedShot;
// The direction sectors in which no target is chosen
static Boolean8 f_IsDirectionExcluded[DIRECTION_SECTOR_COUNT];
static u8 f_UnusedBytesAfterExcludedSectors[8] UNUSED;
static ShotLaunch *f_TrialLaunch;
static ShotLaunch f_TrialLaunchValues;
// The best evaluation so far
static s32 f_BestEvaluation;
static s32 f_AimChangeCount;
static u8 f_PlanThreadStack[PLAN_THREAD_STACK_SIZE] STACK_ALIGNED;
static u8 f_UnusedBufferAfterPlanStack[8192] UNUSED;
// f_HoleStrokeTarget less the hole's par
static s32 f_HoleTargetToPar;
// The strokes the golfer aims to take on the hole
static s32 f_HoleStrokeTarget;
// Whether f_TrialShot holds a copy of f_Shot yet
static Boolean f_HasTrialShot;
// Where the planned shot aims
static CoursePoint f_PlanTarget;
// PlanShot clears these, and nothing in the overlay reads them
static s32 f_UnusedPlanStartValue;
// The route points nearest the cup first, as PlanRoute orders them
static s8 f_RouteOrder[ROUTE_POINT_COUNT];
static u8 f_UnusedBytesAfterRouteOrder[4] UNUSED;
// Whether the trial's club is one for the green
static Boolean f_IsPutting;
// How much worse than ideal an evaluation may be for the trial to be taken
static s32 f_AcceptableEvaluation;
// How many times PlanShot has changed the club or carry
static s32 f_TrialLevel;
static s32 f_RouteOrderCount;
static CoursePoint f_LandingPosition;
static RouteCoordinates f_Route;
// Where the shot to try starts, as PlanShot sets it up
static CurrentShot f_TrialShot;
static Boolean8 f_IsClubUsable[CLUB_SLOT_COUNT];
// Whether the flying trial ball has come down on the ground yet, and where it
// first did
static Boolean f_HasLanded;
// The carry that the landing target is chosen for, and what the club's carry is
// scaled by for it
static s32 f_TargetCarry;
// The size of the golfer's club set
static s32 f_ClubCount;
// The count of trials to take at the level
static s32 f_LevelTrialCount;
// The offset that PlanShot chose for the shot
static ShotOffsetIndex f_ShotOffset;
static s32 f_TrialStepCount;
// Set when the trial is good enough to take
static Boolean f_IsTrialAccepted;
static BallSimulation f_TrialSimulation;
// Whether the trial ended out of bounds, off the course or in water
static Boolean f_IsTrialLost;
// Set when the level needs a new landing target
static Boolean f_NeedsNewTarget;
// The nearest that a putt has come to the target
static s32 f_ClosestTargetDistance;
static u8 f_UnusedBytesAfterClosestDistance[4] UNUSED;
static f32 f_CarryScale;
// The count of trials at this level
static s32 f_TrialIndex;
// Set when the trial asks PlanShot for a longer or a shorter club
static Boolean f_NeedsLongerClub;
// The trial's evaluation; lower is better
static s32 f_TrialEvaluation;
static u8 f_UnusedBytesAfterTrialEvaluation[4] UNUSED;
// Where the trial's ball rests, or for a putt where it passes nearest the
// target
static CoursePoint f_TrialEndPoint;
// PlanShot clears it too, and nothing in the overlay reads it
static s8 f_OtherUnusedPlanStartValue;
// The count of trials that ended with the ball lost
static s32 f_LostTrialCount;
// How far PlanShot has got with the shot
static PlanStage f_PlanStage;
static Boolean f_NeedsShorterClub;
static u8 f_UnusedBytesAfterNeedsShorterClub[8] UNUSED;
static CurrentShot f_Shot;
// The traits of the golfer whose shot is planned
#define ShotGolferTraits() (f_GolferTraits[g_Players[f_Shot.playerIndex].characterIndex])

void RestartShotPlan(void) {
    f_PlanStage = PLAN_STAGE_UNSTARTED;
}

// Starts planning the shot that sets g_CurrentShot up, on its own thread
void StartShotPlan(void) {
    void RunShotPlan(void *arg);

    RestartShotPlan();
    f_PlanThreadState = PLAN_THREAD_RUNNING;
    osCreateThread(
        &f_PlanThread,
        PLAN_THREAD_ID,
        RunShotPlan,
        NULL,
        f_PlanThreadStack + PLAN_THREAD_STACK_SIZE,
        PLAN_THREAD_PRIORITY
    );
    osStartThread(&f_PlanThread);
}

// The planning thread, which waits to be destroyed once the plan is made
void RunShotPlan(void *arg UNUSED) {
    s32 planResult;
    do {
        planResult = PlanShot(&f_PlannedShot);
        LetWokenThreadsRun();
    }

    while (planResult == PLAN_UNFINISHED);
    f_PlanThreadState = PLAN_THREAD_DONE;
    while (TRUE) {
        WaitForOtherThreads();
    }
}

// Copies the planned shot to shot. It returns -1 while the plan is unfinished,
// and 0 once it is made, when it ends the planning thread.
s32 GetPlannedShot(CurrentShot *shot) {
    if (f_PlanThreadState != PLAN_THREAD_RUNNING) {
        *shot = f_PlannedShot;
        if (f_PlanThreadState == PLAN_THREAD_DONE) {
            osDestroyThread(&f_PlanThread);
            f_PlanThreadState = PLAN_THREAD_COLLECTED;
        }

        return 0;
    }

    return PLANNED_SHOT_UNFINISHED;
}

s32 GetClubBoldness(void) {
    return ShotGolferTraits().clubBoldness;
}

Boolean IsStrokeLimitReached(void) {
    return g_Players[f_Shot.playerIndex].strokeCount >= g_HoleSetup.par + f_HoleTargetToPar - 1;
}

// Course units, of which there are 1024 to a world unit
#define SIDE_SLOPE_DISTANCE_MIN 15360.0f
#define SIDE_SLOPE_SAMPLE_SPACING 7680.0f
#define SIDE_SLOPE_SAMPLE_REACH 1536.0f
#define SIDE_SLOPE_STEP 64
#define SIDE_SLOPE_LIMIT 512

// Returns how the ground slopes across the line from the ball to the cup: the
// count of points along the line where the ground is lower toward (-z, x) of
// the line than the other way, less the count where it is higher, times
// SIDE_SLOPE_STEP. It returns 0 for a cup within SIDE_SLOPE_MIN_DISTANCE.
s32 GetSideSlope(void) {
    f32 direction[AXIS_COUNT];
    f32 distance;
    s32 sideSlope;
    s32 sampleIndex;
    s32 reachX;
    s32 reachZ;
    s32 sampleX;
    s32 sampleZ;
    s32 reachHeight;
    s32 oppositeHeight;
    f32 along;

    direction[AXIS_Y] = 0;
    direction[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X] - f_Shot.ballPosition[AXIS_X];
    direction[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z] - f_Shot.ballPosition[AXIS_Z];
    distance = Length2D(direction[AXIS_X], direction[AXIS_Z]);
    sideSlope = 0;
    if (distance < SIDE_SLOPE_DISTANCE_MIN) {
        return 0;
    }

    guNormalize(&direction[AXIS_X], &direction[AXIS_Y], &direction[AXIS_Z]);
    reachX = 2.0f * (-direction[AXIS_Z] * SIDE_SLOPE_SAMPLE_REACH);
    reachZ = 2.0f * (direction[AXIS_X] * SIDE_SLOPE_SAMPLE_REACH);
    for (sampleIndex = 1; sampleIndex < (s32)(distance * (1.0f / SIDE_SLOPE_SAMPLE_SPACING));
        sampleIndex++) {
        // Setting along inside the x term is there to match: setting it first
        // gives 2 words differing (2 in shape) in either order of its product,
        // 4 to 30 in the other operand orders of the terms, 6 with z first, and
        // writing its product out in both terms 10 (8)
        sampleX = f_Shot.ballPosition[AXIS_X] +
            ((along = sampleIndex * SIDE_SLOPE_SAMPLE_SPACING) * direction[AXIS_X]);
        sampleZ = f_Shot.ballPosition[AXIS_Z] + (along * direction[AXIS_Z]);
        reachHeight = GetGroundHeight(sampleX + reachX, sampleZ + reachZ);
        oppositeHeight = GetGroundHeight(sampleX - reachX, sampleZ - reachZ);
        if (reachHeight > oppositeHeight) {
            sideSlope--;
        }
        else if (reachHeight < oppositeHeight) {
            sideSlope++;
        }
    }

    sideSlope *= SIDE_SLOPE_STEP;
    if (sideSlope < -SIDE_SLOPE_LIMIT) {
        sideSlope = -SIDE_SLOPE_LIMIT;
    }

    if (sideSlope > SIDE_SLOPE_LIMIT) {
        sideSlope = SIDE_SLOPE_LIMIT;
    }

    return sideSlope;
}

// f_Shot.powerGaugeFrame's range, which GetPowerGaugeFrame maps 0 to 256 onto
#define POWER_GAUGE_FRAME_MIN 3
// The levels that PlanShot always tries, tries while the best evaluation is
// poor, and tries at most
#define TRIAL_LEVEL_COUNT_MIN 4
#define POOR_TRIAL_LEVEL_COUNT 8
#define TRIAL_LEVEL_COUNT_MAX 30
#define POOR_EVALUATION 0x10000
// Whether PlanShot tries another level: until MIN_TRIAL_LEVEL_COUNT, then
// while the best evaluation is poor or there is none yet
#define IsTrialLevelLeft() \
    (f_TrialLevel < TRIAL_LEVEL_COUNT_MIN || \
        (f_BestEvaluation > POOR_EVALUATION && f_TrialLevel < POOR_TRIAL_LEVEL_COUNT) || \
        (!f_HasBestEvaluation && f_TrialLevel < TRIAL_LEVEL_COUNT_MAX))
// Whether a plan stage is part of a trial under way
#define IsTrialStage(stage) \
    ((stage) == PLAN_STAGE_TRIAL_TAKEN || \
        (stage) == PLAN_STAGE_UNUSED || \
        (stage) == PLAN_STAGE_TRIAL_RUNNING)
#define INITIAL_CARRY_SCALE 1.1f
#define CARRY_SCALE_STEP 0.9f
#define RAINY_CARRY_SCALE 0.95f
#define RAINY_DISTANCE_SCALE 1.1f
// A club is swapped for the next while its shortest reach is at least this and
// fits the distance fewer times
#define CLUB_DOWN_REACH_MIN 0x600
#define CLUB_DOWN_REACH_COUNT 21
// How far from the cup a golfer ahead of its target aims on purpose
#define MISS_DISTANCE_MAX 0x25800
#define LEVEL_MISS_DISTANCE 0x1E00
#define MISS_STRETCH 2.5f
// How many times PlanShot reports a deliberate miss
#define MISS_REPORT_COUNT 40
// The least f_Shot.powerGaugeFrame that PlanShot plans, and how much it scales
// it up by for some golfers
#define PLANNED_POWER_GAUGE_FRAME_MIN 2
#define POWER_GAUGE_FRAME_BOOST 1.2f
// The trials taken at each level, at a level that aims at the cup, and under
// GAME_FLAG_COMPUTER_PLAYS_BEST
#define LEVEL_TRIAL_COUNT 4
#define CUP_LEVEL_TRIAL_COUNT 5
#define BEST_PLAY_LEVEL_TRIAL_COUNT 20
// A mis-hit is a roll below MISHIT_ROLL_RANGE divided by a golfer's divisor,
// and the sign bit sets its direction
#define MISHIT_ROLL_RANGE 250
#define MISHIT_SIGN_BIT 0x2000
// The divisors by the golfer's consistency, and for a shot two strokes short
// of the hole's target, which mis-hits by a frame at most
#define LOW_CONSISTENCY_MISHIT_DIVISOR 62
#define MIDDLE_CONSISTENCY_MISHIT_DIVISOR 83
#define HIGH_CONSISTENCY_MISHIT_DIVISOR 125
#define NEAR_TARGET_MISHIT_DIVISOR 208
#define TARGET_TEXT_COLUMN 3
#define TARGET_TEXT_ROW 12

// Plans the computer golfer's shot from g_CurrentShot a step at a time, and
// returns PLAN_UNFINISHED until the plan is made. The first call chooses the
// usable clubs, the route and the club and target. Each level then picks a
// landing target for the club's carry and an aim and power for it, and tries
// shots at it with StepTrialShot, changing the club or the carry when the
// trials ask for it. Once a trial is taken or the levels run out, the best
// trial may get a random mis-hit, and it is copied to plannedShot.
s32 PlanShot(CurrentShot *plannedShot) {
    char text[DEBUG_TEXT_BUFFER_SIZE];
    s32 percentRange[2];
    ClubType clubType;
    s32 targetX;
    s32 targetZ;
    ShotMode shotMode;
    s32 index;
    s32 strokesOver;
    s32 distance;
    s32 reach;
    ClubSlot nextSlot;
    s32 shortestReach;
    s32 targetDistance;
    s32 missDistance;
    Radians missAngle;
    Radians aimAngle;
    f32 missX;
    f32 missZ;
    f32 along;
    s32 savedPowerGaugeFrame;
    s32 savedImpactGaugeFrame;
    s32 par;
    s32 strokeCount;
    s32 mishit;
    s32 mishitMax;
    s32 mishitDivisor;
    SurfaceKind surfaceKind;

    if (f_PlanStage == PLAN_STAGE_UNSTARTED) {
        osSyncPrintf("\n------------------AI Start---------------------\n");
        f_Shot = g_CurrentShot;
        f_TrialShot = f_Shot;
        f_BestEvaluation = LOST_BALL_EVALUATION;
        f_NeedsNewTarget = TRUE;
        f_HasBestEvaluation = FALSE;
        f_IsTrialLost = FALSE;
        f_LostTrialCount = 0;
        f_UnusedPlanStartValue = 0;
        f_IsTrialAccepted = FALSE;
        f_AcceptableEvaluation = 0;
        f_AimChangeCount = 0;
        f_ShotOffset = SHOT_OFFSET_NONE;
        f_TrialIndex = 0;
        f_TrialLevel = 0;
        f_OtherUnusedPlanStartValue = 0;
        f_AimChangeCount = 0;
        f_HasTrialShot = FALSE;
        f_TrialLaunch = &f_TrialLaunchValues;
        f_CarryScale = INITIAL_CARRY_SCALE;
        for (index = 0; index < DIRECTION_SECTOR_COUNT; index++) {
            f_IsDirectionExcluded[index] = FALSE;
        }

        f_ClubCount = MarkUsableClubs(
            (s32)GetSurfaceTraits(g_CurrentShot.surface),
            g_Players[g_CurrentShot.playerIndex].clubs,
            f_IsClubUsable
        );
        osSyncPrintf("canuseflags = ");
        for (index = 0; index < f_ClubCount; index++) {
            osSyncPrintf("%d", f_IsClubUsable[index]);
        }

        osSyncPrintf("\n");
        f_RouteOrderCount = PlanRoute(f_RouteOrder);
        osSyncPrintf("TargetNum = %d\n", f_RouteOrderCount);
        for (index = 0; index < f_RouteOrderCount; index++) {
            osSyncPrintf("%d ", f_RouteOrder[index]);
        }

        osSyncPrintf("\n");
        PlanClubAndTarget(f_PlanTarget);
        surfaceKind = GetSurfaceTraits(f_Shot.surface)->kind;
        if (surfaceKind == SURFACE_KIND_GREEN || surfaceKind == SURFACE_KIND_GREEN_EDGE) {
            f_TrialLevel = 0;
        }

        osSyncPrintf("wnmTargetScoreOffset = %d\n", f_HoleTargetToPar);
        osSyncPrintf("ClubIndex = %d\n", f_Shot.clubSlot);
        sprintf(text, "Target %d   ", (int)f_HoleTargetToPar);
        PrintDebugText(text, TARGET_TEXT_COLUMN, TARGET_TEXT_ROW);
        f_PlanStage = PLAN_STAGE_STARTED;
        return PLAN_UNFINISHED;
    }
    while (IsTrialLevelLeft()) {
        if (IsTrialStage(f_PlanStage)) {
            // A trial is under way
        }
        else if (f_NeedsNewTarget == TRUE) {
            if (f_PlanStage != PLAN_STAGE_TARGET_CHOSEN) {
                osSyncPrintf("Level %d\n", f_TrialLevel);
                f_NeedsShorterClub = FALSE;
                f_NeedsLongerClub = FALSE;
                if (f_Shot.clubSlot != FindClubSlot(f_Shot.playerIndex, CLUB_TYPE_PUTTER)) {
                    f_TargetCarry = GetShotWorldDistance(
                        GetClub(g_Players[f_Shot.playerIndex].clubs[f_Shot.clubSlot]),
                        f_Shot.shotMode
                    );
                    if (g_HoleSetup.conditions.isRaining == TRUE) {
                        f_TargetCarry = f_TargetCarry * RAINY_CARRY_SCALE;
                    }

                    f_TargetCarry = f_TargetCarry * f_CarryScale;
                    ChooseLandingTarget(
                        f_Shot.ballPosition[AXIS_X],
                        f_Shot.ballPosition[AXIS_Z],
                        f_TargetCarry,
                        &f_PlanTarget[AXIS_X],
                        &f_PlanTarget[AXIS_Z]
                    );
                }
                else {
                    f_PlanTarget[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X];
                    f_PlanTarget[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z];
                }

                if (f_Shot.shotMode != SHOT_MODE_APPROACH &&
                    !(GetClub(g_Players[f_Shot.playerIndex].clubs[f_Shot.clubSlot])->surfaceMask &
                        CLUB_SURFACE_GREEN)) {
                    targetDistance = IntLength2D(
                        f_PlanTarget[AXIS_X] - f_Shot.ballPosition[AXIS_X],
                        f_PlanTarget[AXIS_Z] - f_Shot.ballPosition[AXIS_Z]
                    );
                    if (g_HoleSetup.conditions.isRaining == TRUE) {
                        targetDistance = (s32)(targetDistance * RAINY_DISTANCE_SCALE);
                    }
                    while (TRUE) {
                        shortestReach = GetShotWorldDistance(
                            GetClub(g_Players[f_Shot.playerIndex].clubs[f_Shot.clubSlot]),
                            f_Shot.shotMode
                        );
                        GetSurfacePercentRange(
                            GetSurfaceTraits(f_Shot.surface),
                            GetClub(g_Players[f_Shot.playerIndex].clubs[f_Shot.clubSlot]),
                            percentRange
                        );
                        // The carry per unit of f_Shot.powerGaugeFrame at the least percent
                        shortestReach = shortestReach * percentRange[0] /
                            (PERCENT_PER_WHOLE * GAUGE_FULL_FRAME);
                        if (shortestReach < CLUB_DOWN_REACH_MIN ||
                            targetDistance / shortestReach >= CLUB_DOWN_REACH_COUNT) {
                            break;
                        }

                        nextSlot = f_Shot.clubSlot;
                        if ((GetClub(g_Players[f_Shot.playerIndex].clubs[nextSlot])->surfaceMask &
                                CLUB_SURFACE_GREEN) ||
                            (GetClub(g_Players[f_Shot.playerIndex].clubs[++nextSlot])->surfaceMask &
                                CLUB_SURFACE_GREEN)) {
                            break;
                        }

                        f_Shot.clubSlot = nextSlot;
                        osSyncPrintf("ChangeDefault----------------------ClubDown\n");
                    }
                }

                f_PlanStage = PLAN_STAGE_TARGET_CHOSEN;
                if (f_PlanTarget[AXIS_X] == g_HoleSetup.cupPosition[AXIS_X] &&
                    f_PlanTarget[AXIS_Z] == g_HoleSetup.cupPosition[AXIS_Z] &&
                    GetSurfaceTraits(f_Shot.surface)->kind != SURFACE_KIND_GREEN) {
                    strokesOver = g_Players[f_Shot.playerIndex].strokeCount -
                        (g_HoleSetup.par + f_HoleTargetToPar - 2);
                    if (strokesOver <= 0) {
                        missDistance =
                            IntLength2D(
                                g_HoleSetup.cupPosition[AXIS_X] - f_Shot.ballPosition[AXIS_X],
                                g_HoleSetup.cupPosition[AXIS_Z] - f_Shot.ballPosition[AXIS_Z]
                            ) >>
                            3;
                        missAngle = ((rand() * 4) & BINARY_ANGLE_MASK) * BINARY_ANGLE_TO_RADIANS;
                        aimAngle = (u16)f_Shot.heading * BINARY_ANGLE_TO_RADIANS;
                        if (missDistance >= MISS_DISTANCE_MAX) {
                            missDistance = MISS_DISTANCE_MAX;
                        }

                        if (strokesOver < -1) {
                            missDistance *= 2;
                        }

                        if (strokesOver == 0) {
                            missDistance = LEVEL_MISS_DISTANCE;
                        }

                        missDistance = missDistance * 0.5f;
                        missX = cosf(missAngle) * missDistance;
                        missZ = sinf(missAngle) * missDistance;
                        along =
                            ((missX * cosf(aimAngle)) + (missZ * sinf(aimAngle))) * MISS_STRETCH;
                        missX += cosf(aimAngle) * along;
                        missZ += sinf(aimAngle) * along;
                        f_PlanTarget[AXIS_X] = f_PlanTarget[AXIS_X] + missX;
                        f_PlanTarget[AXIS_Z] = f_PlanTarget[AXIS_Z] + missZ;
                        for (index = 0; index < MISS_REPORT_COUNT; index++) {
                            osSyncPrintf("Iziwaru %d\n", strokesOver);
                        }
                    }
                }

                return PLAN_UNFINISHED;
            }

            f_PlanTarget[AXIS_Y] = GetGroundHeight(f_PlanTarget[AXIS_X], f_PlanTarget[AXIS_Z]);
            f_Shot.heading = atan2(
                                 f_PlanTarget[AXIS_Z] - f_Shot.ballPosition[AXIS_Z],
                                 f_PlanTarget[AXIS_X] - f_Shot.ballPosition[AXIS_X]
                             ) *
                RADIANS_TO_BINARY_ANGLE;
            if (GetClub(g_Players[f_Shot.playerIndex].clubs[f_Shot.clubSlot])->surfaceMask &
                CLUB_SURFACE_GREEN) {
                f_Shot.heading += GetSideSlope();
            }

            f_Shot.impactGaugeFrame = GAUGE_IMPACT_FRAME;
            f_Shot.powerGaugeFrame = GAUGE_FULL_FRAME;
            reach = GetShotWorldDistance(
                GetClub(g_Players[f_Shot.playerIndex].clubs[f_Shot.clubSlot]),
                f_Shot.shotMode
            );
            distance = IntLength2D(
                f_Shot.ballPosition[AXIS_X] - f_PlanTarget[AXIS_X],
                f_Shot.ballPosition[AXIS_Z] - f_PlanTarget[AXIS_Z]
            );
            if (g_HoleSetup.conditions.isRaining == TRUE) {
                distance = (s32)(distance * RAINY_DISTANCE_SCALE);
            }

            GetSurfacePercentRange(
                GetSurfaceTraits(f_Shot.surface),
                GetClub(g_Players[f_Shot.playerIndex].clubs[f_Shot.clubSlot]),
                percentRange
            );
            // The carry per unit of f_Shot.powerGaugeFrame at the mean percent
            reach = reach * (percentRange[1] + percentRange[0]) /
                (2 * PERCENT_PER_WHOLE * GAUGE_FULL_FRAME);
            // Holding f_Shot.powerGaugeFrame in distance is there to match: a
            // powerFrame local of its own gives 26 words differing, none in shape
            distance = (rand() & 3) + (distance / reach);
            if (GetClubBoldness() >= BOLD_CLUB_CHOICE) {
                distance = distance * POWER_GAUGE_FRAME_BOOST;
            }

            if (distance < PLANNED_POWER_GAUGE_FRAME_MIN) {
                distance = PLANNED_POWER_GAUGE_FRAME_MIN;
            }
            else if (distance > GAUGE_FULL_FRAME) {
                distance = GAUGE_FULL_FRAME;
            }

            f_Shot.powerGaugeFrame = distance;
            if (f_ShotOffset == SHOT_OFFSET_NONE) {
                f_Shot.offsetX = 0;
                f_Shot.offsetY = 0;
                f_ShotOffset = ChooseShotOffset();
            }

            f_TrialIndex = 0;
            f_LevelTrialCount = LEVEL_TRIAL_COUNT;
            if (!f_HasTrialShot) {
                f_TrialShot = f_Shot;
                f_HasTrialShot = TRUE;
            }

            if (f_PlanTarget[AXIS_X] == g_HoleSetup.cupPosition[AXIS_X] &&
                f_PlanTarget[AXIS_Z] == g_HoleSetup.cupPosition[AXIS_Z]) {
                f_LevelTrialCount = CUP_LEVEL_TRIAL_COUNT;
            }

            if (IsGameFlagSet(GAME_FLAG_COMPUTER_PLAYS_BEST)) {
                f_LevelTrialCount = BEST_PLAY_LEVEL_TRIAL_COUNT;
            }

            f_NeedsNewTarget = FALSE;
        }
        else {
            f_TrialIndex = 0;
        }

        if (f_TrialIndex < f_LevelTrialCount) {
            if (!StepTrialShot()) {
                return PLAN_UNFINISHED;
            }

            if (!f_IsTrialAccepted && !f_NeedsShorterClub && !f_NeedsLongerClub) {
                f_PlanStage = PLAN_STAGE_TRIAL_TAKEN;
                f_TrialIndex++;
                return PLAN_UNFINISHED;
            }
        }

        f_PlanStage = PLAN_STAGE_LEVEL_DONE;
        if (f_IsTrialLost == TRUE) {
            f_LostTrialCount++;
        }

        osSyncPrintf("Besteval = %d\n", f_BestEvaluation);
        if (f_IsTrialAccepted) {
            break;
        }

        if (f_NeedsShorterClub) {
            if (f_Shot.clubSlot == FindClubSlot(f_Shot.playerIndex, CLUB_TYPE_PUTTER)) {
                break;
            }

            if (f_Shot.shotMode == SHOT_MODE_APPROACH) {
                if (ShotGolferTraits().windPlay < WIND_PLAY_FOLLOWING_WINDS &&
                    f_Shot.clubSlot == FindClubSlot(f_Shot.playerIndex, CLUB_TYPE_PITCHING_WEDGE)) {
                    f_Shot.clubSlot = FindClubSlot(f_Shot.playerIndex, CLUB_TYPE_SAND_WEDGE);
                }
                else {
                    osSyncPrintf("Approach cancel\n");
                    ChooseLongShot(f_HoleStrokeTarget, &clubType, &targetX, &targetZ, &shotMode);
                    f_Shot.shotMode = shotMode;
                    f_Shot.clubSlot = FindClubSlot(f_Shot.playerIndex, clubType);
                }

                f_NeedsNewTarget = TRUE;
                f_TrialLevel++;
                continue;
            }

            f_Shot.clubSlot++;
            if (f_Shot.clubSlot >= FindClubSlot(f_Shot.playerIndex, CLUB_TYPE_PITCHING_WEDGE)) {
                f_Shot.clubSlot = FindClubSlot(f_Shot.playerIndex, CLUB_TYPE_PITCHING_WEDGE);
            }

            f_NeedsNewTarget = TRUE;
        }
        else if (f_NeedsLongerClub == TRUE) {
            osSyncPrintf("num_hit = %d\n", g_Players[f_Shot.playerIndex].strokeCount);
            if (f_Shot.clubSlot == FindClubSlot(f_Shot.playerIndex, CLUB_TYPE_PUTTER)) {
                break;
            }

            if (f_Shot.clubSlot > 0) {
                f_Shot.clubSlot--;
                if (!f_IsClubUsable[f_Shot.clubSlot] ||
                    (GetClubBoldness() < BOLD_CLUB_CHOICE &&
                        GetLieRating(
                            GetClub(g_Players[f_Shot.playerIndex].clubs[f_Shot.clubSlot]),
                            f_Shot.surface,
                            f_Shot.shotMode
                        ) <= LIE_RATING_MIN)) {
                    f_Shot.clubSlot++;
                    f_CarryScale *= CARRY_SCALE_STEP;
                }
            }
            else {
                f_CarryScale *= CARRY_SCALE_STEP;
            }

            f_NeedsNewTarget = TRUE;
        }

        f_TrialLevel++;
    }

    osSyncPrintf(
        "Finish Eval %d tlevel%d bestevalappear %d\n",
        f_BestEvaluation,
        f_TrialLevel,
        f_HasBestEvaluation
    );
    savedImpactGaugeFrame = f_TrialShot.impactGaugeFrame;
    savedPowerGaugeFrame = f_TrialShot.powerGaugeFrame;
    if ((s32)IntLength2D(
            g_HoleSetup.cupPosition[AXIS_X] - f_TrialShot.ballPosition[AXIS_X],
            g_HoleSetup.cupPosition[AXIS_Z] - f_TrialShot.ballPosition[AXIS_Z]
        ) <
        (s32)IntLength2D(
            g_HoleSetup.cupPosition[AXIS_X] - f_PlanTarget[AXIS_X],
            g_HoleSetup.cupPosition[AXIS_Z] - f_PlanTarget[AXIS_Z]
        )) {
        strokeCount = g_Players[f_Shot.playerIndex].strokeCount;
        par = g_HoleSetup.par;
        mishit = 0;
        if (GetSurfaceTraits(f_TrialShot.surface)->kind != SURFACE_KIND_GREEN &&
            strokeCount < par + f_HoleTargetToPar - 1) {
            mishitMax = GetLieRating(
                GetClub(g_Players[f_TrialShot.playerIndex].clubs[f_TrialShot.clubSlot]),
                f_TrialShot.surface,
                f_TrialShot.shotMode
            );
            mishitMax -= mishitMax != 0;
            if (strokeCount < par + f_HoleTargetToPar - 2) {
                switch (
                    f_GolferTraits[g_Players[f_TrialShot.playerIndex].characterIndex].consistency) {
                    case CONSISTENCY_LOW:
                        mishitDivisor = LOW_CONSISTENCY_MISHIT_DIVISOR;
                        break;
                    case CONSISTENCY_MIDDLE:
                        mishitDivisor = MIDDLE_CONSISTENCY_MISHIT_DIVISOR;
                        break;
                    case CONSISTENCY_HIGH:
                    default:
                        mishitDivisor = HIGH_CONSISTENCY_MISHIT_DIVISOR;
                        break;
                }
            }
            else {
                mishitDivisor = NEAR_TARGET_MISHIT_DIVISOR;
            }

            mishit = rand() % MISHIT_ROLL_RANGE / mishitDivisor;
            if (mishitMax < Abs(mishit) &&
                (f_HoleTargetToPar < 0 || strokeCount >= par + f_HoleTargetToPar - 3)) {
                mishit = mishitMax;
            }

            if (mishit != 0 && (rand() & MISHIT_SIGN_BIT)) {
                mishit = -mishit;
            }

            f_TrialShot.impactGaugeFrame += mishit;

            // Never printed; the original has it, and its string is in the ROM
            if (FALSE) {
                osSyncPrintf("Dahuri = %d\n", mishit);
            }
        }

        if ((f_HoleTargetToPar <= 0) & (mishit != 0) && !IsTrialShotSafe()) {
            f_TrialShot.impactGaugeFrame = savedImpactGaugeFrame;
            f_TrialShot.powerGaugeFrame = savedPowerGaugeFrame;
        }
    }

    osSyncPrintf("clubidx = %d\n", f_TrialShot.clubSlot);
    osSyncPrintf("pgaugeresult = %d\n", f_TrialShot.powerGaugeFrame);
    osSyncPrintf("pgaugeinpact = %d\n", f_TrialShot.impactGaugeFrame);
    *plannedShot = f_TrialShot;
    osSyncPrintf("AI End\n");
    return 1;
}

s32 GetBallCupDistance(void) {
    return IntLength2D(
        f_Shot.ballPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
        f_Shot.ballPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]
    );
}

// The cup distance from which PlanCupDistance allows for the wind
#define WIND_ALLOWANCE_DISTANCE_MIN 153600.0f
// windSpeed's units in a unit of the wind's push
#define WIND_PUSH_UNIT 0x10000
#define WIND_SPEED_SCALE (1.0f / WIND_PUSH_UNIT)
// The fraction of the distance that each unit of push along the line takes off
#define WIND_FOLLOW_FRACTION 0.01f
#define WIND_DISTANCE_RATIO_MIN 0.8f
#define WIND_DISTANCE_RATIO_MAX 1.2f
// The distance that each unit of the ball's height above the cup adds
#define HEIGHT_DISTANCE_FRACTION 0.1f

// Returns the distance to plan a shot to the cup for: the flat distance,
// shortened by a following wind and lengthened by a head wind from
// WIND_ALLOWANCE_MIN_DISTANCE out, plus a part of the ball's height above the
// cup.
s32 PlanCupDistance(ShotMode shotMode UNUSED) {
    s32 distance;
    f32 cupX;
    f32 cupZ;
    f32 length;
    Radians windAngle;
    f32 inverseLength;
    f32 windFollow;
    f32 windX;
    f32 windZ;

    distance = IntLength2D(
        f_Shot.ballPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
        f_Shot.ballPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]
    );
    cupX = g_HoleSetup.cupPosition[AXIS_X] - f_Shot.ballPosition[AXIS_X];
    cupZ = g_HoleSetup.cupPosition[AXIS_Z] - f_Shot.ballPosition[AXIS_Z];
    length = sqrtf((cupX * cupX) + (cupZ * cupZ));
    windAngle = g_HoleSetup.conditions.windDirection * WIND_DIRECTION_TO_RADIANS;
    if (length >= WIND_ALLOWANCE_DISTANCE_MIN) {
        inverseLength = 1.0f / length;
        cupX *= inverseLength;
        cupZ *= inverseLength;
        windX = cosf(windAngle) * g_HoleSetup.conditions.windSpeed * WIND_SPEED_SCALE;
        windZ = sinf(windAngle) * g_HoleSetup.conditions.windSpeed * WIND_SPEED_SCALE;
        windFollow = (cupX * windX) + (cupZ * windZ);
        osSyncPrintf("WindFollow%f\n", windFollow);

        // Turning windFollow into the distance ratio is there to match: a ratio
        // local of its own gives 8 words differing, none in shape
        windFollow = (-windFollow * WIND_FOLLOW_FRACTION) + 1.0f;
        osSyncPrintf("WindCareRatio%f\n", windFollow);
        if (windFollow < WIND_DISTANCE_RATIO_MIN) {
            windFollow = WIND_DISTANCE_RATIO_MIN;
        }

        if (windFollow >= WIND_DISTANCE_RATIO_MAX) {
            windFollow = WIND_DISTANCE_RATIO_MAX;
        }

        distance = distance * windFollow;
    }

    return distance +
        ((f_Shot.ballPosition[AXIS_Y] - g_HoleSetup.cupPosition[AXIS_Y]) *
            HEIGHT_DISTANCE_FRACTION);
}

ClubType ChooseApproachClub(s32 *targetX, s32 *targetZ);

// Shots from farther than this are long shots
#define APPROACH_RANGE (60 * UNITS_PER_YARD)
// How much farther than the cup distance a putt is played in the rain
#define RAINY_PUTT_DISTANCE_SCALE 1.2f

// Chooses f_Shot's club and shot mode for the lie and sets target to where the
// shot aims. From the bunker lies other than SURFACE_KIND_BUNKER it plays the
// sand wedge; on the green it takes the last usable club that can play from the
// green, in the weakest shot mode that reaches the cup; elsewhere it plays a
// long shot or an approach, by the cup distance.
void PlanClubAndTarget(CoursePoint target) {
    s32 targetX;
    s32 targetZ;
    ClubType clubType;
    ShotMode shotMode;
    SurfaceKind surfaceKind;
    s32 cupDistance;
    ClubSlot clubSlot;
    Club *club;

    clubType = CLUB_TYPE_DRIVER;
    surfaceKind = GetSurfaceTraits(f_Shot.surface)->kind;
    f_HoleStrokeTarget = ChooseHoleStrokeTarget();
    f_HoleTargetToPar = f_HoleStrokeTarget - g_HoleSetup.par;
    switch ((u32)surfaceKind) {
        case SURFACE_KIND_GREEN:
        case SURFACE_KIND_GREEN_EDGE:
            cupDistance = GetBallCupDistance();
            osSyncPrintf("rest = %d\n", cupDistance);
            if (g_HoleSetup.conditions.isRaining == TRUE) {
                cupDistance = cupDistance * RAINY_PUTT_DISTANCE_SCALE;
            }

            for (clubSlot = 0; clubSlot < f_ClubCount; clubSlot++) {
                if (!f_IsClubUsable[clubSlot]) {
                    continue;
                }

                club = GetClub(g_Players[f_Shot.playerIndex].clubs[clubSlot]);
                if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
                    continue;
                }

                if (GetShotWorldDistance(club, SHOT_MODE_APPROACH) >= cupDistance) {
                    f_Shot.shotMode = SHOT_MODE_APPROACH;
                }
                else if (GetShotWorldDistance(club, SHOT_MODE_NORMAL) >= cupDistance) {
                    f_Shot.shotMode = SHOT_MODE_NORMAL;
                }
                else {
                    f_Shot.shotMode = SHOT_MODE_POWER;
                }

                osSyncPrintf("skSCAI.powermode = %d\n", f_Shot.shotMode);
                f_Shot.clubSlot = clubSlot;
            }

            break;
        case SURFACE_KIND_BUNKER_FAIR_LIE:
        case SURFACE_KIND_BUNKER_POOR_LIE:
        case SURFACE_KIND_BUNKER_UNUSED_LIE:
            f_Shot.shotMode = SHOT_MODE_NORMAL;
            f_Shot.clubSlot = FindClubSlot(f_Shot.playerIndex, CLUB_TYPE_SAND_WEDGE);
            ChooseLandingTarget(
                f_Shot.ballPosition[AXIS_X],
                f_Shot.ballPosition[AXIS_Z],
                GetShotWorldDistance(
                    GetClub(g_Players[f_Shot.playerIndex].clubs[f_Shot.clubSlot]),
                    f_Shot.shotMode
                ),
                &targetX,
                &targetZ
            );
            target[AXIS_X] = targetX;
            target[AXIS_Z] = targetZ;
            target[AXIS_Y] = GetGroundHeight(targetX, targetZ);
            break;
        default:
            if (GetBallCupDistance() > APPROACH_RANGE) {
                ChooseLongShot(f_HoleStrokeTarget, &clubType, &targetX, &targetZ, &shotMode);
                target[AXIS_X] = targetX;
                target[AXIS_Z] = targetZ;
                target[AXIS_Y] = GetGroundHeight(targetX, targetZ);
                f_Shot.shotMode = shotMode;
                f_Shot.clubSlot = FindClubSlot(f_Shot.playerIndex, clubType);
            }
            else {
                clubType = ChooseApproachClub(&targetX, &targetZ);
                target[AXIS_X] = targetX;
                target[AXIS_Z] = targetZ;
                target[AXIS_Y] = GetGroundHeight(targetX, targetZ);
                f_Shot.shotMode = SHOT_MODE_APPROACH;
                f_Shot.clubSlot = FindClubSlot(f_Shot.playerIndex, clubType);
            }

            break;
    }
}

// SortByKeys's orders
#define SORT_ASCENDING 0

// Selection sorts count keys in the given order, swapping the entries of the
// values and otherValues arrays along with them unless they are NULL.
void SortByKeys(s32 count, s32 *values, s32 *otherValues, s32 *keys, s32 order) {
    s32 index;
    s32 otherIndex;
    s32 chosenIndex;
    s32 swapped;

    for (index = 0; index < count - 1; index++) {
        chosenIndex = index;
        for (otherIndex = index + 1; otherIndex < count; otherIndex++) {
            if (order == SORT_DESCENDING) {
                if (keys[otherIndex] > keys[chosenIndex]) {
                    chosenIndex = otherIndex;
                }
            }
            else if (keys[otherIndex] < keys[chosenIndex]) {
                chosenIndex = otherIndex;
            }
        }

        if (values != NULL) {
            swapped = values[index];
            values[index] = values[chosenIndex];
            values[chosenIndex] = swapped;
        }

        if (otherValues != NULL) {
            swapped = otherValues[index];
            otherValues[index] = otherValues[chosenIndex];
            otherValues[chosenIndex] = swapped;
        }

        swapped = keys[index];
        keys[index] = keys[chosenIndex];
        keys[chosenIndex] = swapped;
    }
}

// Returns the player's strokes over par on the holes before this one,
// counting only holes with strokes. A hole with more than twice its par plus
// one strokes counts as one under par.
s32 GetScoreToParSoFar(s32 playerIndex) {
    s32 strokeTotal = 0;
    s32 parTotal = 0;
    s32 holeIndex;
    s32 par;
    s32 strokeCount;

    if (g_GameState.holeIndex == 0) {
        return 0;
    }

    for (holeIndex = 0; holeIndex < g_GameState.holeIndex; holeIndex++) {
        strokeCount = g_Players[playerIndex].holes[holeIndex].strokeCount;
        par = g_CourseRecords[g_GameState.courseIndex].holes[holeIndex].par;
        if ((par * 2) + 1 < strokeCount) {
            strokeCount = par - 1;
        }

        if (strokeCount > 0) {
            strokeTotal += strokeCount;
            parTotal += par;
        }
    }

    return strokeTotal - parTotal;
}

// The par range of the course's holes
#define LOWEST_PAR 3
#define HIGHEST_PAR 5
// A hole's planned strokes are lowered only from this many or more
#define LOWEST_LOWERED_STROKES 3

// Spreads strokeCount strokes, or takes -strokeCount away, one a hole at a
// time in the order of the entries, over the holes that isSpreadHole picks,
// until they run out or no hole can take any more. It is a macro, as
// isSpreadHole is an expression on the caller's index.
#define SpreadStrokes(strokes, strokeCount, isSpreadHole) \
    { \
        s32 changeCount_; \
\
        while ((strokeCount) != 0) { \
            changeCount_ = 0; \
            for (index = 0; index < COURSE_HOLE_COUNT; index++) { \
                if (!(isSpreadHole)) { \
                    continue; \
                } \
                if ((strokeCount) < 0) { \
                    if ((strokes)[index] >= LOWEST_LOWERED_STROKES) { \
                        changeCount_++; \
                        (strokes)[index]--; \
                        (strokeCount)++; \
                        if ((strokeCount) >= 0) { \
                            break; \
                        } \
                    } \
                } \
                else { \
                    changeCount_++; \
                    (strokes)[index]++; \
                    (strokeCount)--; \
                    if ((strokeCount) <= 0) { \
                        break; \
                    } \
                } \
            } \
            if (changeCount_ == 0) { \
                break; \
            } \
        } \
    }

// Chooses how many strokes the golfer aims to take on this hole. It takes the
// character's base target score to par, plus the course's adjustment to it,
// plus a lucky bonus of 0 to 3 strokes in Get Character when its record against
// the player's character has more losses than wins. It spreads that score over
// the holes by par and handicap, then spreads what the score so far leaves over
// the holes still to play.
s32 ChooseHoleStrokeTarget(void) {
    s32 strokes[COURSE_HOLE_COUNT];
    s32 handicaps[COURSE_HOLE_COUNT];
    s32 holeIndices[COURSE_HOLE_COUNT];
    s32 groupHandicaps[COURSE_HOLE_COUNT];
    s32 groupStrokes[COURSE_HOLE_COUNT];
    s32 groupHoleIndices[COURSE_HOLE_COUNT];
    s32 luckyBonus;
    s32 lossCount;
    s32 winBits;
    s32 targetScore;
    StrongNine strongNine;
    s32 index;
    s32 groupCount;
    s32 frontStrokes;
    s32 backStrokes;
    s32 negativeCount;
    s32 targetLeftToPar;
    s32 plannedLeftToPar;
    s32 entry;
    s32 strokesLeft;

    luckyBonus = 0;
    if (IsGameFlagSet(GAME_FLAG_COMPUTER_PLAYS_BEST)) {
        return 0;
    }

    if (g_GameState.gameMode == GAME_MODE_TRAINING) {
        return g_HoleSetup.par;
    }

    if (g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
        lossCount =
            GetSaveData()
                ->records.awards
                .matchups[g_Players[0].characterIndex][g_Players[f_Shot.playerIndex].characterIndex]
                .lossCount;
        winBits =
            GetSaveData()
                ->records.awards
                .matchups[g_Players[0].characterIndex][g_Players[f_Shot.playerIndex].characterIndex]
                .winBits;
        if ((winBits & WIN_COUNT_MASK) < (lossCount & WIN_COUNT_MASK)) {
            luckyBonus = g_GameState.roundSeed & 3;
        }

        osSyncPrintf("win%d lose%d lucky%d\n", winBits, lossCount, luckyBonus);
    }

    targetScore = luckyBonus + ShotGolferTraits().baseTargetScore +
        ShotGolferTraits().courseTargetOffsets[g_GameState.courseIndex];
    osSyncPrintf("Char Index = %d\n", g_Players[f_Shot.playerIndex].characterIndex);
    osSyncPrintf("TargetScore = %d\n", targetScore);
    strongNine = ShotGolferTraits().strongNine;
    for (index = 0; index < COURSE_HOLE_COUNT; index++) {
        strokes[index] = 0;
        handicaps[index] = 0;
    }

    for (index = 0; index < COURSE_HOLE_COUNT; index++) {
        handicaps[index] = g_CourseRecords[g_GameState.courseIndex].holes[index].handicap;
        strokes[index] = g_CourseRecords[g_GameState.courseIndex].holes[index].par;
        holeIndices[index] = index;
    }

    if (targetScore < 0) {
        SortByKeys(COURSE_HOLE_COUNT, holeIndices, handicaps, strokes, SORT_DESCENDING);
    }
    else {
        SortByKeys(COURSE_HOLE_COUNT, holeIndices, handicaps, strokes, SORT_ASCENDING);
    }

    for (index = HIGHEST_PAR; index >= LOWEST_PAR; index--) {
        groupCount = 0;
        for (entry = 0; entry < COURSE_HOLE_COUNT; entry++) {
            if (strokes[entry] == index) {
                groupHoleIndices[groupCount] = holeIndices[entry];
                groupStrokes[groupCount] = strokes[entry];
                groupHandicaps[groupCount] = handicaps[entry];
                groupCount++;
            }
        }

        if (targetScore < 0) {
            SortByKeys(groupCount, groupHoleIndices, groupStrokes, groupHandicaps, SORT_DESCENDING);
        }
        else {
            SortByKeys(groupCount, groupHoleIndices, groupStrokes, groupHandicaps, SORT_ASCENDING);
        }

        groupCount = 0;
        for (entry = 0; entry < COURSE_HOLE_COUNT; entry++) {
            if (strokes[entry] == index) {
                holeIndices[entry] = groupHoleIndices[groupCount];
                strokes[entry] = groupStrokes[groupCount];

                // Reads handicaps rather than groupHandicaps; the original has it
                handicaps[entry] = handicaps[groupCount];
                groupCount++;
            }
        }
    }

    backStrokes = targetScore / 2;
    frontStrokes = backStrokes;
    if (strongNine == STRONG_NINE_NEITHER) {
        if (targetScore & 1) {
            frontStrokes = targetScore - backStrokes;
        }
    }
    else if (strongNine == STRONG_NINE_FRONT) {
        if (targetScore & 1) {
            frontStrokes = targetScore - backStrokes;
        }
    }
    else if (strongNine == STRONG_NINE_BACK) {
        if (targetScore & 1) {
            backStrokes = targetScore - backStrokes;
        }
    }

    strokesLeft = frontStrokes;
    SpreadStrokes(strokes, strokesLeft, holeIndices[index] < HOLES_PER_NINE);
    strokesLeft = backStrokes;
    SpreadStrokes(strokes, strokesLeft, holeIndices[index] >= HOLES_PER_NINE);
    if (strongNine == STRONG_NINE_FRONT || strongNine == STRONG_NINE_BACK) {
        negativeCount = 0;
        backStrokes = 0;
        frontStrokes = 0;
        for (index = 0; index < COURSE_HOLE_COUNT; index++) {
            if (strongNine == STRONG_NINE_FRONT && holeIndices[index] >= HOLES_PER_NINE) {
                continue;
            }

            if (strongNine == STRONG_NINE_BACK && holeIndices[index] < HOLES_PER_NINE) {
                continue;
            }

            if (strokes[index] < 0) {
                negativeCount++;
            }
        }

        if (targetScore > 0) {
            if (negativeCount == 0) {
                if (strongNine == STRONG_NINE_FRONT) {
                    frontStrokes--;
                    backStrokes++;
                }
                else if (strongNine == STRONG_NINE_BACK) {
                    backStrokes--;
                    frontStrokes++;
                }
            }
        }
        else if (targetScore == 0 || targetScore == -1) {
            if (negativeCount < 2) {
                if (strongNine == STRONG_NINE_FRONT) {
                    frontStrokes += negativeCount - 2;
                    backStrokes -= negativeCount - 2;
                }
                else if (strongNine == STRONG_NINE_BACK) {
                    backStrokes += negativeCount - 2;
                    frontStrokes -= negativeCount - 2;
                }
            }
        }
        else if (negativeCount < 3) {
            if (strongNine == STRONG_NINE_FRONT) {
                frontStrokes += negativeCount - 3;
                backStrokes -= negativeCount - 3;
            }
            else if (strongNine == STRONG_NINE_BACK) {
                backStrokes += negativeCount - 3;
                frontStrokes -= negativeCount - 3;
            }
        }

        for (index = 0; index < COURSE_HOLE_COUNT; index++) {
            if (holeIndices[index] < HOLES_PER_NINE) {
                if (frontStrokes < 0) {
                    if (strokes[index] ==
                            g_CourseRecords[g_GameState.courseIndex]
                                .holes[holeIndices[index]]
                                .par &&
                        strokes[index] != LOWEST_LOWERED_STROKES) {
                        strokes[index]--;
                        frontStrokes++;
                    }
                }
                else if (frontStrokes > 0) {
                    if (strokes[index] ==
                        g_CourseRecords[g_GameState.courseIndex].holes[holeIndices[index]].par) {
                        strokes[index]++;
                        frontStrokes--;
                    }
                }
            }
            else if (backStrokes < 0) {
                if (strokes[index] ==
                        g_CourseRecords[g_GameState.courseIndex].holes[holeIndices[index]].par &&
                    strokes[index] != LOWEST_LOWERED_STROKES) {
                    strokes[index]--;
                    backStrokes++;
                }
            }
            else if (backStrokes > 0) {
                if (strokes[index] ==
                    g_CourseRecords[g_GameState.courseIndex].holes[holeIndices[index]].par) {
                    strokes[index]++;
                    backStrokes--;
                }
            }
        }
    }

    plannedLeftToPar = 0;
    targetLeftToPar = targetScore - GetScoreToParSoFar(f_Shot.playerIndex);
    for (index = 0; index < COURSE_HOLE_COUNT; index++) {
        if (holeIndices[index] >= g_GameState.holeIndex) {
            plannedLeftToPar += strokes[index] -
                g_CourseRecords[g_GameState.courseIndex].holes[holeIndices[index]].par;
        }
    }

    if (g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE) {
        targetLeftToPar = targetScore - targetLeftToPar;
        if (g_GameState.holeIndex >= HOLES_PER_NINE) {
            targetLeftToPar = 0;
            for (index = 0; index < COURSE_HOLE_COUNT; index++) {
                if (holeIndices[index] < HOLES_PER_NINE) {
                    targetLeftToPar += strokes[index] -
                        g_CourseRecords[g_GameState.courseIndex].holes[holeIndices[index]].par;
                }
            }

            for (index = 0; index < COURSE_HOLE_COUNT; index++) {
                if (holeIndices[index] >= HOLES_PER_NINE &&
                    holeIndices[index] < g_GameState.holeIndex) {
                    targetLeftToPar +=
                        g_Players[f_Shot.playerIndex].holes[holeIndices[index]].strokeCount -
                        g_CourseRecords[g_GameState.courseIndex].holes[holeIndices[index]].par;
                }
            }

            targetLeftToPar = targetScore - targetLeftToPar;
        }
    }

    if (g_GameState.betRates[BET_SKINS] > 0 ||
        g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
        g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
        targetLeftToPar = 0;
        for (index = 0; index < COURSE_HOLE_COUNT; index++) {
            if (holeIndices[index] < g_GameState.holeIndex) {
                targetLeftToPar += strokes[index] -
                    g_CourseRecords[g_GameState.courseIndex].holes[holeIndices[index]].par;
            }
        }

        targetLeftToPar = targetScore - targetLeftToPar;
    }

    SortByKeys(COURSE_HOLE_COUNT, strokes, handicaps, holeIndices, SORT_ASCENDING);
    osSyncPrintf("OrgOffset = %d\n", targetLeftToPar);
    osSyncPrintf("Work = %d\n", plannedLeftToPar);
    strokesLeft = targetLeftToPar - plannedLeftToPar;
    SpreadStrokes(strokes, strokesLeft, g_GameState.holeIndex <= holeIndices[index]);
    osSyncPrintf("TS => ");
    for (index = 0; index < COURSE_HOLE_COUNT; index++) {
        for (entry = 0; entry < COURSE_HOLE_COUNT; entry++) {
            if (holeIndices[entry] == index) {
                osSyncPrintf(" %d", strokes[entry]);
            }
        }
    }

    osSyncPrintf("\n");
    osSyncPrintf("OS => ");
    for (index = 0; index < COURSE_HOLE_COUNT; index++) {
        osSyncPrintf(" %d", g_CourseRecords[g_GameState.courseIndex].holes[index].par);
    }

    osSyncPrintf("\n");
    for (index = 0; index < COURSE_HOLE_COUNT; index++) {
        if (holeIndices[index] == g_GameState.holeIndex) {
            // The comma is in the original's string
            osSyncPrintf("++ %d\n,", strokes[index]);
            return strokes[index];
        }
    }

// The original reads past the holes, into the next course record's name
#ifdef AVOID_UB
    return g_HoleSetup.par;
#else
    return g_CourseRecords[g_GameState.courseIndex].holes[index].par;
#endif
}

#define NO_ROUTE_POINT (-1)
// Beyond any distance on a hole
#define FARTHEST_DISTANCE 0x0FFFFFFF

// Stores in route the indices into f_Route of the hole's route
// points to play through, from the tee toward the cup, and returns their count.
// Starting at the cup, it walks to the nearest point not yet visited until it
// reaches the tee.
s32 PlanRoute(s8 *route) {
    Boolean8 isVisited[ROUTE_POINT_COUNT + 1];
    s8 order[ROUTE_POINT_COUNT];
    s32 index;
    s32 pointCount;
    s8 remainingCount;
    s32 currentX;
    s32 currentZ;
    s32 orderCount;
    s32 nearestDistance;
    s32 nearest;
    s32 distance;
    s32 orderIndex;

    for (index = ROUTE_POINT_COUNT - 1; index >= 0; index--) {
        isVisited[index] = FALSE;
    }

    currentX = g_HoleSetup.cupPosition[AXIS_X];
    currentZ = g_HoleSetup.cupPosition[AXIS_Z];
    pointCount = 0;
    for (index = 0; index < ROUTE_POINT_COUNT; index++) {
        if (g_HolePoints.routePoints[index][AXIS_X] == 0 &&
            g_HolePoints.routePoints[index][AXIS_Z] == 0) {
            break;
        }

        f_Route.x[pointCount] = g_HolePoints.routePoints[index][AXIS_X];
        f_Route.z[pointCount] = g_HolePoints.routePoints[index][AXIS_Z];
        pointCount++;
    }

    f_Route.x[pointCount] = g_HoleSetup.teePosition[AXIS_X];
    f_Route.z[pointCount] = g_HoleSetup.teePosition[AXIS_Z];
    pointCount++;
    remainingCount = pointCount;
    orderCount = 0;
    while (remainingCount > 0) {
        nearest = NO_ROUTE_POINT;
        nearestDistance = FARTHEST_DISTANCE;
        for (index = 0; index < pointCount; index++) {
            if (isVisited[index]) {
                continue;
            }

            distance = IntLength2D(currentX - f_Route.x[index], currentZ - f_Route.z[index]);
            // Unused; it is there to match
            IntLength2D(
                f_Route.x[index] - g_HoleSetup.teePosition[AXIS_X],
                f_Route.z[index] - g_HoleSetup.teePosition[AXIS_Z]
            );
            if (distance < nearestDistance) {
                nearest = index;
                nearestDistance = distance;
            }
        }

        if (nearest != NO_ROUTE_POINT) {
            remainingCount--;
            order[orderCount] = nearest;
            isVisited[nearest] = TRUE;
            currentX = f_Route.x[nearest];
            currentZ = f_Route.z[nearest];
            orderCount++;
            if (currentX == g_HoleSetup.teePosition[AXIS_X] &&
                currentZ == g_HoleSetup.teePosition[AXIS_Z]) {
                break;
            }
        }
    }

    // Counting the route in index is there to match: a routeLength local gives
    // 64 words differing, none in shape
    index = 0;
    for (orderIndex = orderCount - 1; orderIndex >= 0; orderIndex--) {
        if (order[orderIndex] != pointCount - 1) {
            route[index++] = order[orderIndex];
        }
    }

    return index;
}

// How good a place to land each surface kind is, from LANDING_NONE to
// LANDING_BEST
enum LandingRating {
    LANDING_NONE,
    LANDING_POOR = 5,
    LANDING_FAIR = 7,
    LANDING_BEST
};

typedef s32 LandingRating;

LandingRating GetLandingRating(SurfaceKind surfaceKind) {
    LandingRating rating;

    if (g_GameState.courseIndex == COURSE_MARIOS_STAR) {
        if (surfaceKind == SURFACE_KIND_ROCK) {
            return LANDING_FAIR;
        }

        if (surfaceKind == SURFACE_KIND_BUNKER) {
            return LANDING_FAIR;
        }
    }

    switch (surfaceKind) {
        case SURFACE_KIND_FAIRWAY:
        case SURFACE_KIND_FAIRWAY_FAIR_LIE:
        case SURFACE_KIND_FAIRWAY_POOR_LIE:
        case SURFACE_KIND_GREEN:
        case SURFACE_KIND_GREEN_EDGE:
        case SURFACE_KIND_TEE_GROUND:
        case SURFACE_KIND_SEMI_ROUGH:
            rating = LANDING_BEST;
            break;
        case SURFACE_KIND_ROUGH:
        case SURFACE_KIND_ROUGH_POOR_LIE:
        case SURFACE_KIND_ROUGH_UNUSED_LIE:
        case SURFACE_KIND_CART_WAY:
        case SURFACE_KIND_ROCK:
        case SURFACE_KIND_BARE_GROUND:
        case SURFACE_KIND_DEEP_ROUGH:
        case SURFACE_KIND_NO_DROP_GROUND:
        case SURFACE_KIND_WASTE_AREA:
            rating = LANDING_FAIR;
            break;
        case SURFACE_KIND_BUNKER:
        case SURFACE_KIND_BUNKER_FAIR_LIE:
        case SURFACE_KIND_BUNKER_POOR_LIE:
            rating = LANDING_POOR;
            break;
        case SURFACE_KIND_BUNKER_UNUSED_LIE:
        case SURFACE_KIND_WATER_HAZARD:
        case SURFACE_KIND_OUT_OF_BOUNDS:
        case SURFACE_KIND_OUT_OF_COURSE:
        case SURFACE_KIND_TEE_UP:
        default:
            rating = LANDING_NONE;
            break;
    }

    return rating;
}

#define END_ROUTE_POINT 0xFF
// The landing search walks back from the cup this many yards a step
#define TARGET_STEP_YARDS 2
// The search takes up to reach * TARGET_STEPS_PER_UNIT +
// EXTRA_TARGET_STEP_COUNT steps
#define TARGET_STEPS_PER_UNIT (4 * YARDS_PER_UNIT)
#define EXTRA_TARGET_STEP_COUNT 50.0f
#define TARGET_SIDE_COUNT 100
#define TARGET_SIDE_SPACING (4 * UNITS_PER_YARD)
// The neighbors a target's lie is tested at, in TARGET_NEIGHBOR_SPACINGs
#define TARGET_NEIGHBOR_REACH 2
#define TARGET_NEIGHBOR_SPACING 4
// Rolls shorter than this are not tested for ground in the way
#define TESTED_ROLL_MIN (2 * UNITS_PER_YARD)

// Chooses where a shot of distance from (x, z) lands. Walking from the cup back
// toward the ball a step at a time, and out to either side at each step, it
// takes the first point within reach that is outside the excluded directions,
// lands as well as the cup, has no ground in the way of the flight or the roll
// to the cup, and has no much worse lie around it. It aims at the ball if none
// qualifies, and at the cup if no route point is nearer the cup than the ball.
// Points that lie farther below the ball than they are across from it are
// skipped.
void ChooseLandingTarget(s32 x, s32 z, s32 distance, s32 *targetX, s32 *targetZ) {
    u8 order[ROUTE_POINT_COUNT + 1];
    s32 pointX[ROUTE_POINT_COUNT + 1];
    s32 pointZ[ROUTE_POINT_COUNT + 1];
    s32 nearIndices[ROUTE_POINT_COUNT + 1];
    PlaneTriangle hit;
    f32 hitFraction;
    s32 pointCount;
    s32 index;
    s32 nearCount;
    s32 ballCupDistance;
    s32 answerIndex;
    s32 reach;
    s32 alongX;
    s32 alongZ;
    s32 endX;
    s32 endZ;
    LandingRating endRating;
    s32 sideCount;
    Boolean isFound;
    s32 startX;
    s32 startZ;
    Radians angle;
    f32 cosAngle;
    f32 sinAngle;
    f32 along;
    s32 side;
    s32 offsetX;
    s32 offsetZ;
    s32 candidateX;
    s32 candidateZ;
    s32 height;
    s32 flatDistance;
    s32 cupDistance;
    Boolean isPoorlySurrounded;
    s32 column;
    s32 row;

    osSyncPrintf("Distance = %dYd\n", (s32)(distance * YARDS_PER_UNIT));
    pointCount = f_RouteOrderCount;
    isFound = FALSE;
    if (pointCount == 0) {
        sideCount = TARGET_SIDE_COUNT;
        endX = g_HoleSetup.cupPosition[AXIS_X];
        endZ = g_HoleSetup.cupPosition[AXIS_Z];
        startX = x;
        startZ = z;
    }
    else {
        for (index = 0; index < f_RouteOrderCount; index++) {
            order[index] = f_RouteOrder[index];
        }

        for (index = 0; index < pointCount; index++) {
            pointX[index] = f_Route.x[order[index]];
            pointZ[index] = f_Route.z[order[index]];
            osSyncPrintf("PointList[%d] = (%d, %d)\n", index, pointX[index], pointZ[index]);
        }

        // Reading the cup's position through g_HoleSetup here and below is there
        // to match: a cupZ local read once gives 35 words differing (11 in
        // shape), and cupX and cupZ locals 47 (19)
        pointX[pointCount] = g_HoleSetup.cupPosition[AXIS_X];
        pointZ[pointCount] = g_HoleSetup.cupPosition[AXIS_Z];
        order[pointCount] = END_ROUTE_POINT;
        pointCount++;
        nearCount = 0;
        ballCupDistance =
            IntLength2D(g_HoleSetup.cupPosition[AXIS_X] - x, g_HoleSetup.cupPosition[AXIS_Z] - z);
        for (index = 0; index < pointCount; index++) {
            if ((s32)IntLength2D(
                    g_HoleSetup.cupPosition[AXIS_X] - pointX[index],
                    g_HoleSetup.cupPosition[AXIS_Z] - pointZ[index]
                ) < ballCupDistance) {
                nearIndices[nearCount++] = index;
            }
        }

        for (index = 0; index < nearCount; index++) {
            order[index] = order[nearIndices[index]];
            pointX[index] = pointX[nearIndices[index]];
            pointZ[index] = pointZ[nearIndices[index]];
        }

        pointCount = nearCount;
        answerIndex = -2;
        if (pointCount == 0) {
            *targetX = g_HoleSetup.cupPosition[AXIS_X];
            *targetZ = g_HoleSetup.cupPosition[AXIS_Z];
            return;
        }

        for (index = 0; index < pointCount; index++) {
            if (distance < (s32)IntLength2D(pointX[index] - x, pointZ[index] - z)) {
                answerIndex = index - 1;
                break;
            }
        }

        osSyncPrintf("AnsIndex = %d\n", answerIndex);
        endX = g_HoleSetup.cupPosition[AXIS_X];
        endZ = g_HoleSetup.cupPosition[AXIS_Z];
        startX = x;
        startZ = z;
        sideCount = TARGET_SIDE_COUNT;
    }

    osSyncPrintf("StartX = %d, StartY = %d\n", startX, startZ);
    osSyncPrintf("EndX = %d, EndY = %d\n", endX, endZ);
    angle = atan2((f32)(startZ - endZ), (f32)(startX - endX));
    reach = IntLength2D(endX - startX, endZ - startZ);
    osSyncPrintf("r = %d\n", reach);
    cosAngle = cosf(angle);
    sinAngle = sinf(angle);
    *targetX = startX;
    *targetZ = startZ;
    endRating = GetLandingRating(GetSurfaceTraitsAt(endX, endZ)->kind);
    osSyncPrintf("TALevel %d\n", endRating);

    // Counting the steps in index is there to match: a step local gives 289
    // words differing, 27 in shape
    for (index = 0; index < (reach * TARGET_STEPS_PER_UNIT) + EXTRA_TARGET_STEP_COUNT; index++) {
        along = TARGET_STEP_YARDS * (index * UNITS_PER_YARD);
        alongX = (along * cosAngle) + endX;
        alongZ = (along * sinAngle) + endZ;
        for (side = 0; side < sideCount; side++) {
            offsetX = -sinAngle * side * TARGET_SIDE_SPACING;
            offsetZ = cosAngle * side * TARGET_SIDE_SPACING;
            if (side & 1) {
                candidateX = alongX + offsetX;
                candidateZ = alongZ + offsetZ;
            }
            else {
                candidateX = alongX - offsetX;
                candidateZ = alongZ - offsetZ;
            }

            if (distance < (s32)IntLength2D(candidateX - x, candidateZ - z)) {
                break;
            }

            if (GetLandingRating(GetSurfaceTraitsAt(candidateX, candidateZ)->kind) < endRating) {
                continue;
            }

            if (f_IsDirectionExcluded
                    [((u32)(s32)(atan2f(
                                     candidateZ - f_Shot.ballPosition[AXIS_Z],
                                     candidateX - f_Shot.ballPosition[AXIS_X]
                                 ) *
                          RADIANS_TO_BINARY_ANGLE) &
                         BINARY_ANGLE_MASK) >>
                        DIRECTION_SECTOR_SHIFT]) {
                continue;
            }

            height = GetGroundHeight(candidateX, candidateZ);
            flatDistance = IntLength2D(
                candidateZ - f_Shot.ballPosition[AXIS_Z],
                candidateX - f_Shot.ballPosition[AXIS_X]
            );
            if (flatDistance < f_Shot.ballPosition[AXIS_Y] - height) {
                continue;
            }

            cupDistance = IntLength2D(
                candidateX - g_HoleSetup.cupPosition[AXIS_X],
                candidateZ - g_HoleSetup.cupPosition[AXIS_Z]
            );
            if (FindGroundHit(
                    f_Shot.ballPosition[AXIS_X],
                    f_Shot.ballPosition[AXIS_Y] - PATH_GROUND_CLEARANCE,
                    f_Shot.ballPosition[AXIS_Z],
                    candidateX,
                    f_Shot.ballPosition[AXIS_Y] - flatDistance,
                    candidateZ,
                    &hit,
                    &hitFraction
                )) {
                continue;
            }

            if (cupDistance >= TESTED_ROLL_MIN) {
                if (FindGroundHit(
                        candidateX,
                        height - PATH_GROUND_CLEARANCE,
                        candidateZ,
                        g_HoleSetup.cupPosition[AXIS_X],
                        height - (cupDistance / 2),
                        g_HoleSetup.cupPosition[AXIS_Z],
                        &hit,
                        &hitFraction
                    )) {
                    continue;
                }
            }

            isPoorlySurrounded = FALSE;
            if (candidateX != g_HoleSetup.cupPosition[AXIS_X] ||
                candidateZ != g_HoleSetup.cupPosition[AXIS_Z]) {
                for (column = -TARGET_NEIGHBOR_REACH; column <= TARGET_NEIGHBOR_REACH; column++) {
                    for (row = -TARGET_NEIGHBOR_REACH; row <= TARGET_NEIGHBOR_REACH; row++) {
                        if (column == 0 && row == 0) {
                            continue;
                        }

                        if (GetLandingRating(GetSurfaceTraitsAt(
                                candidateX + (column * TARGET_NEIGHBOR_SPACING * UNITS_PER_YARD),
                                candidateZ + (row * TARGET_NEIGHBOR_SPACING * UNITS_PER_YARD)
                            )
                                    ->kind) < endRating - 1) {
                            isPoorlySurrounded = TRUE;
                            break;
                        }
                    }

                    if (isPoorlySurrounded == TRUE) {
                        break;
                    }
                }
            }

            if (!isPoorlySurrounded) {
                isFound = TRUE;
                *targetX = candidateX;
                *targetZ = candidateZ;
                break;
            }
        }

        if (isFound) {
            break;
        }
    }

    osSyncPrintf("ResultX = %d, ResultY = %d\n", *targetX, *targetZ);
    osSyncPrintf(
        "Result TALevel %d\n",
        GetLandingRating(GetSurfaceTraitsAt(*targetX, *targetZ)->kind)
    );
}

// The shot modes for each stroke count; all of them are SHOT_MODE_NORMAL
#define STROKE_SHOT_MODE_COUNT 16

// Plans a shot from beyond approach range: aims at the cup with the club whose
// carry covers PlanCupDistance, in the shot mode for the player's stroke count
void ChooseLongShot(
    s32 unused UNUSED,
    ClubType *clubType,
    s32 *targetX,
    s32 *targetZ,
    ShotMode *shotMode
) {
    // The original frame has room for this; without it 34 words differ, none in
    // shape
    s32 pad[44] UNUSED;
    ShotMode shotModes[STROKE_SHOT_MODE_COUNT];
    s32 index;
    // normalMode is there to match: the constant in its place as the argument
    // gives 3 words differing (3 in shape), and as the stored mode or in both
    // places 5 (3)
    ShotMode normalMode;
    s32 strokeCount;

    normalMode = SHOT_MODE_NORMAL;
    for (index = STROKE_SHOT_MODE_COUNT - 1; index >= 0; index--) {
        shotModes[index] = normalMode;
    }

    strokeCount = g_Players[f_Shot.playerIndex].strokeCount;
    *clubType = ChooseClubForDistance(PlanCupDistance(normalMode));
    *targetX = g_HoleSetup.cupPosition[AXIS_X];
    *targetZ = g_HoleSetup.cupPosition[AXIS_Z];
    *shotMode = shotModes[strokeCount];
}

// It is a macro, as an inline function compiles differently from the original
#define IsOffFairway(surfaceKind) \
    ((surfaceKind) != SURFACE_KIND_FAIRWAY && \
        (surfaceKind) != SURFACE_KIND_FAIRWAY_FAIR_LIE && \
        (surfaceKind) != SURFACE_KIND_FAIRWAY_POOR_LIE && \
        (surfaceKind) != SURFACE_KIND_SEMI_ROUGH && \
        (surfaceKind) != SURFACE_KIND_GREEN && \
        (surfaceKind) != SURFACE_KIND_GREEN_EDGE)

// Returns whether any point a yard apart on the line from from to to, from
// from itself, lies off the fairway, semi-rough, green and green edge.
Boolean IsLineOffFairway(CoursePoint from, CoursePoint to) {
    s32 distance;
    Radians angle;
    s32 step;
    f32 stepLength;
    s32 x;
    s32 z;
    SurfaceKind surfaceKind;

    distance = IntLength2D(to[AXIS_X] - from[AXIS_X], to[AXIS_Z] - from[AXIS_Z]);
    angle = atan2((f32)(to[AXIS_Z] - from[AXIS_Z]), (f32)(to[AXIS_X] - from[AXIS_X]));
    for (step = 0; step < distance * YARDS_PER_UNIT; step++) {
        // Setting stepLength in the loop is there to match: setting it before
        // the loop gives 7 words differing (7 in shape), and the constant in
        // its place 26
        stepLength = UNITS_PER_YARD;
        x = (step * (cosf(angle) * stepLength)) + from[AXIS_X];
        z = (step * (sinf(angle) * stepLength)) + from[AXIS_Z];
        surfaceKind = GetSurfaceTraitsAt(x, z)->kind;
        if (IsOffFairway(surfaceKind)) {
            return TRUE;
        }
    }

    return FALSE;
}

#define GREEN_EDGE_NOT_FOUND (-1)

// Steps a yard at a time along the line from from to to, and stores the
// first point on a green edge in edge. Returns GREEN_EDGE_FOUND if it found
// one, otherwise GREEN_EDGE_NOT_FOUND.
s32 FindGreenEdgeOnLine(CoursePoint from, CoursePoint to, CoursePoint edge) {
    s32 distance;
    Radians angle;
    s32 step;
    f32 stepLength;
    s32 x;
    s32 z;

    distance = IntLength2D(to[AXIS_X] - from[AXIS_X], to[AXIS_Z] - from[AXIS_Z]);
    angle = atan2((f32)(to[AXIS_Z] - from[AXIS_Z]), (f32)(to[AXIS_X] - from[AXIS_X]));
    osSyncPrintf("StartPoint = (%d, %d)\n", from[AXIS_X], from[AXIS_Z]);
    osSyncPrintf("EndPoint = (%d, %d)\n", to[AXIS_X], to[AXIS_Z]);
    for (step = 1; step < (distance * YARDS_PER_UNIT) + 1; step++) {
        // Setting stepLength in the loop is there to match: setting it before
        // the loop gives 7 words differing (7 in shape), and the constant in
        // its place 26
        stepLength = UNITS_PER_YARD;
        x = (step * (cosf(angle) * stepLength)) + from[AXIS_X];
        z = (step * (sinf(angle) * stepLength)) + from[AXIS_Z];
        if (GetSurfaceTraitsAt(x, z)->kind == SURFACE_KIND_GREEN_EDGE) {
            edge[AXIS_X] = x;
            edge[AXIS_Z] = z;
            return GREEN_EDGE_FOUND;
        }
    }

    return GREEN_EDGE_NOT_FOUND;
}

// Returns the distance from from to the first green edge on the line to to,
// or to to itself if the line crosses no green edge.
s32 GetGreenEdgeDistance(CoursePoint from, CoursePoint to) {
    CoursePoint edge;
    s32 distance;

    if (FindGreenEdgeOnLine(from, to, edge) != GREEN_EDGE_FOUND) {
        distance = IntLength2D(to[AXIS_X] - from[AXIS_X], to[AXIS_Z] - from[AXIS_Z]);
    }
    else {
        distance = IntLength2D(edge[AXIS_X] - from[AXIS_X], edge[AXIS_Z] - from[AXIS_Z]);
    }

    return distance;
}

// Stores in peak the point a yard apart on the line from from to to whose
// ground height is farthest from 0, starting with from's x and z. It sets
// peak's height only for a point past from.
void FindPeakOnLine(CoursePoint from, CoursePoint to, CoursePoint peak) {
    s32 distance;
    Radians angle;
    s32 peakHeight;
    s32 step;
    f32 stepLength;
    s32 x;
    s32 z;
    s32 height;

    distance = IntLength2D(to[AXIS_X] - from[AXIS_X], to[AXIS_Z] - from[AXIS_Z]);
    angle = atan2((f32)(to[AXIS_Z] - from[AXIS_Z]), (f32)(to[AXIS_X] - from[AXIS_X]));
    peakHeight = GetGroundHeight(from[AXIS_X], from[AXIS_Z]);
    peakHeight = Abs(peakHeight);
    peak[AXIS_X] = from[AXIS_X];
    peak[AXIS_Z] = from[AXIS_Z];
    for (step = 1; step < distance * YARDS_PER_UNIT; step++) {
        // Setting stepLength in the loop is there to match: setting it before
        // the loop gives 26 words differing (6 in shape), and the constant in
        // its place 46
        stepLength = UNITS_PER_YARD;
        x = (step * (cosf(angle) * stepLength)) + from[AXIS_X];
        z = (step * (sinf(angle) * stepLength)) + from[AXIS_Z];
        height = GetGroundHeight(x, z);
        height = Abs(height);
        if (peakHeight < height) {
            peak[AXIS_X] = x;
            peak[AXIS_Y] = GetGroundHeight(x, z);
            peakHeight = height;
            peak[AXIS_Z] = z;
        }
    }
}

// Aims at the cup and returns the type of club to approach it with. The club
// depends on how much of the line to the cup lies short of the green, where the
// line's peak counts when it is on the green, and on the golfer's consistency.
// A line that crosses rough or hazards takes a wedge.
ClubType ChooseApproachClub(s32 *targetX, s32 *targetZ) {
    CoursePoint peak;
    s32 cupDistance;
    s32 edgeDistance;
    f32 edgeShare;
    SurfaceKind peakKind;
    s32 band;
    Consistency consistency;

    *targetX = g_HoleSetup.cupPosition[AXIS_X];
    *targetZ = g_HoleSetup.cupPosition[AXIS_Z];
    cupDistance = IntLength2D(
        g_HoleSetup.cupPosition[AXIS_X] - f_Shot.ballPosition[AXIS_X],
        g_HoleSetup.cupPosition[AXIS_Z] - f_Shot.ballPosition[AXIS_Z]
    );
    // Unused; it is there to match
    GetSurfaceTraits(f_Shot.surface);
    if (IsLineOffFairway(f_Shot.ballPosition, g_HoleSetup.cupPosition) == TRUE) {
        osSyncPrintf("Appr60 Return point 1\n");
        if (ShotGolferTraits().windPlay < WIND_PLAY_FOLLOWING_WINDS) {
            return CLUB_TYPE_PITCHING_WEDGE;
        }

        return CLUB_TYPE_SAND_WEDGE;
    }

    edgeDistance = GetGreenEdgeDistance(f_Shot.ballPosition, g_HoleSetup.cupPosition);
    edgeShare = (f32)edgeDistance / (f32)cupDistance;
    osSyncPrintf("Approach : DistForEdge = %d\n", edgeDistance);
    osSyncPrintf("Approach : Distance = %d\n", cupDistance);
    osSyncPrintf("Approach : EdgePercent = %f\n", edgeShare);
    FindPeakOnLine(f_Shot.ballPosition, g_HoleSetup.cupPosition, peak);
    peakKind = GetSurfaceTraitsAt(peak[AXIS_X], peak[AXIS_Z])->kind;
    if (peakKind == SURFACE_KIND_GREEN || peakKind == SURFACE_KIND_GREEN_EDGE) {
        edgeDistance = IntLength2D(
            peak[AXIS_X] - f_Shot.ballPosition[AXIS_X],
            peak[AXIS_Z] - f_Shot.ballPosition[AXIS_Z]
        );
        edgeShare = (f32)edgeDistance / (f32)cupDistance;
    }

    consistency = ShotGolferTraits().consistency;
    for (band = 0; band < APPROACH_BAND_COUNT; band++) {
        if (edgeShare < f_ApproachBandLimits[band][consistency]) {
            if (band == 0) {
                return CLUB_TYPE_PITCHING_WEDGE;
            }
            else if (band == 1) {
                if (consistency == CONSISTENCY_HIGH) {
                    return CLUB_TYPE_DRIVER;
                }

                if (consistency == CONSISTENCY_MIDDLE) {
                    return CLUB_TYPE_THREE_WOOD;
                }

                return CLUB_TYPE_FOUR_WOOD;
            }
            else if (band == 2) {
                if (consistency == CONSISTENCY_HIGH) {
                    return CLUB_TYPE_FIVE_IRON;
                }

                if (consistency == CONSISTENCY_MIDDLE) {
                    return CLUB_TYPE_FOUR_IRON;
                }

                return CLUB_TYPE_THREE_IRON;
            }
            else if (band == 3) {
                if (consistency == CONSISTENCY_HIGH) {
                    return CLUB_TYPE_SAND_WEDGE;
                }

                if (consistency == CONSISTENCY_MIDDLE) {
                    return CLUB_TYPE_PITCHING_WEDGE;
                }

                return CLUB_TYPE_NINE_IRON;
            }
        }
    }

    return CLUB_TYPE_PITCHING_WEDGE;
}

#define POWER_TO_GAUGE_FRAME_SCALE (GAUGE_FULL_FRAME / 256.0f)

// What the trial costs for each step on which a putt lips out; one at the
// stroke limit costs POOR_LANDING_EVALUATION more
#define LIP_OUT_EVALUATION 0x10
#define POOR_LANDING_EVALUATION 0x8000
#define BLOCKED_CUP_EVALUATION 0x80000
#define FAR_FROM_TARGET 0x40000000
#define MISS_EVALUATION_SCALE 0.006535948f
#define APPROACH_TOLERANCE_SCALE 0.0065104165f
#define LONG_PUTT_DISTANCE_MIN 0x7800
#define PUTT_DRIFT_LIMIT 0xF000
#define AIM_CHANGE_STEP 0x400
#define BLOCKED_TRIAL_AIM_CHANGES_MAX 17
#define UNSTOPPED_TRIAL_AIM_CHANGES_MAX 3
#define BLOCKED_POWER_RATIO 10.0f
#define NEAR_TARGET_EVALUATION_SLACK 500
#define BEHIND_TARGET_EVALUATION_SLACK 0x8000
#define OFF_GREEN_EVALUATION_SLACK 8000
#define NEAR_TARGET_PUTT_SLACK_SHARE 0.3f
#define BEHIND_TARGET_PUTT_SLACK_SHARE 0.4f
#define EXCLUDED_SECTOR_COUNT 9

// Advances the trial of f_Shot by one step of the ball simulation, setting the
// trial up first unless it is under way. It returns 0 while the trial is still
// better than the best so far, and 1 once it has evaluated the trial, or
// abandoned it, and adjusted f_Shot's power, mode or aim for the next one.
//
// GetPowerGaugeFrame is nested in the block that calls it, a GNU C extension,
// as in the original: StepTrialShot passes it a stack address in $v0 as the
// static chain, which it stores. GCC compiles a nested function at its
// definition, and rest_of_compilation sets cse_not_expected, which isn't saved
// across it, so the code after the definition is expanded with constant
// addresses inside its memory references, as the original's is there, and the
// code before it with the addresses in registers. Nested at the start of the
// routine, 185 words differ (123 in shape); at the start of the evaluation
// block, 148 (106); at the start of the ratio's test, 56 (32); at the start
// of the test for too much power, 57 (33); and as a routine of its own, with
// or without a store standing in for the static chain's, 71 (27).
//
// A poor landing's evaluation sits in a block of its own, as a macro's
// do-while would make it, to match. The original loads
// POOR_LANDING_EVALUATION into $v0 and sums it into $v1. local-alloc ranks a
// block's quantities by floor_log2(refs) * refs / life, and outside a loop
// the sum's 4 references over a life of 8 tie with the constant's 2 over a
// life of 2; the tie goes to the lower quantity number, the sum's, which
// takes $v0. flow counts a reference twice in a loop, even one that runs
// once, and the logarithm then ranks the constant's 4 references above the
// sum's 8, so the constant takes $v0.
//
// Without the block, or with plain braces, 12 words differ (4 in shape). A
// block around the whole if, one that ends after the out-of-bounds test, and
// one around the whole else arm from the EVALUATE label match too. One around
// the sum alone, the out-of-bounds test alone or both tests gives 24 (6), and
// one around the water hazard's test 12 (4). The kind read into a local
// inside the block, before or after the sum, matches; read before the block
// it gives 12 (6), and with only its read and the sum in the block 4 (4).
//
// The heading's correction ends the arm for a finished trial, and the next
// try's aim change is the else arm, both reaching the one return: with an
// early return in the arm instead, 21 words differ (7 in shape), and with a
// do-while (0) around the correction 14 (4). The aim change's step as an
// if/else subtracting or adding gives 23 (13); adding the step or its
// negation, chosen by ?:, 15 (7); and choosing by ?: between the heading
// minus and plus the step 23 (9).
s32 StepTrialShot(void) {
    PlaneTriangle hit;
    f32 fraction;
    StepFlags stepFlags;
    BallState ballState;
    // A copy of flags, to match: reading flags gives 19 words differing, 3 in
    // shape
    StepFlags stepFlagsCopy;
    s32 velocityY;
    s32 bounceCount;
    s32 heightMiss;
    SurfaceTraits *surfaceTraits;
    LandingRating targetRating;
    s32 strokeCount;
    s32 par;
    s32 strokesOver;
    s32 approachDistance;
    s32 flyDistance;
    s32 targetDistance;
    f32 aimCos;
    f32 aimSin;
    f32 ratio;
    s32 launchPower;
    s32 index;

    stepFlags = 0;
    if (f_PlanStage != PLAN_STAGE_TRIAL_RUNNING) {
        f_IsTrialLost = FALSE;
        f_TrialEvaluation = 0;
        f_IsPutting = FALSE;
        f_HasLanded = FALSE;
        f_Shot.offsetX = f_ShotOffsets[f_ShotOffset].offsetX + (rand() & 7) - 3;
        f_Shot.offsetY = f_ShotOffsets[f_ShotOffset].offsetY + (rand() & 7) - 3;
        f_TrialLaunch->heading = f_Shot.heading;
        f_TrialLaunch->clubSlot = f_Shot.clubSlot;
        f_ClosestTargetDistance = FAR_FROM_TARGET;
        f_TrialLaunch->power = GetGaugeFramePower(f_Shot.powerGaugeFrame);
        f_TrialLaunch->impactTiming = 0;
        f_TrialLaunch->offsetX = f_Shot.offsetX;
        f_TrialLaunch->offsetY = f_Shot.offsetY;
        f_TrialLaunch->playerIndex = f_Shot.playerIndex;
        f_TrialLaunch->shotMode = f_Shot.shotMode;
        f_IsPutting = GetClub(f_Shot.clubSlot)->surfaceMask & CLUB_SURFACE_GREEN;
        if (!IsGameFlagSet(GAME_FLAG_COMPUTER_PLAYS_BEST)) {
            if (f_IsPutting) {
                f_AcceptableEvaluation = (f_AcceptableEvaluation * 1.1f) + 0.2f;
            }
            else {
                f_AcceptableEvaluation = (f_AcceptableEvaluation * 1.1f) + 20.0f;
            }
        }

        f_TrialSimulation.position[AXIS_X] = f_Shot.ballPosition[AXIS_X];
        f_TrialSimulation.position[AXIS_Y] = f_Shot.ballPosition[AXIS_Y];
        f_TrialSimulation.position[AXIS_Z] = f_Shot.ballPosition[AXIS_Z];
        f_TrialSimulation.surface = f_Shot.surface;
        LaunchBallSimulation(&f_TrialSimulation, LAUNCH_PURPOSE_TRIAL, f_TrialLaunch);
        f_TrialStepCount = 0;
        f_HasFirstFallHit = FALSE;
        f_TrialEndPoint[AXIS_X] = f_TrialSimulation.position[AXIS_X];
        f_TrialEndPoint[AXIS_Y] = f_TrialSimulation.position[AXIS_Y];
        f_TrialEndPoint[AXIS_Z] = f_TrialSimulation.position[AXIS_Z];
        f_LandingPosition[AXIS_X] = f_TrialSimulation.position[AXIS_X];
        f_LandingPosition[AXIS_Y] = f_TrialSimulation.position[AXIS_Y];
        f_LandingPosition[AXIS_Z] = f_TrialSimulation.position[AXIS_Z];
    }

    ballState = f_TrialSimulation.state;
    if (ballState != BALL_STATE_STOPPED) {
        // A loop that runs once, so that the step's checks can break out to the
        // end point's update; the original has no loop here, and jumping to a
        // label instead gives 38 words differing, 20 in shape
        do {
            velocityY = f_TrialSimulation.velocity[AXIS_Y];
            bounceCount = f_TrialSimulation.bounceCount;
            f_TrialStepCount++;
            stepFlags = StepBallSimulation(&f_TrialSimulation);
            stepFlagsCopy = stepFlags;
            if (f_TrialStepCount >= STEP_COUNT_MAX) {
                SetDebugTextHighlight(TRUE);
                PrintDebugText(
                    "AI Sim Fault!",
                    SIMULATION_FAULT_TEXT_COLUMN,
                    SIMULATION_FAULT_TEXT_ROW
                );
                break;
            }

            if (stepFlags & STEP_FLAG_OUTSIDE_COURSE) {
                break;
            }

            if (bounceCount == 0 &&
                (stepFlags & (STEP_FLAG_GROUND_HIT | STEP_FLAG_BILLBOARD_HIT)) &&
                velocityY < 0 &&
                ballState == BALL_STATE_FLYING) {
                if (f_HasBestEvaluation || f_TrialLevel < 2) {
                    break;
                }

                f_HasFirstFallHit = ballState;
            }

            if (f_IsPutting) {
                s32 rollingMiss;

                rollingMiss = IntLength2D(
                    f_TrialSimulation.position[AXIS_X] - f_PlanTarget[AXIS_X],
                    f_TrialSimulation.position[AXIS_Z] - f_PlanTarget[AXIS_Z]
                );
                if (rollingMiss < f_ClosestTargetDistance) {
                    f_ClosestTargetDistance = rollingMiss;
                    f_TrialEndPoint[AXIS_X] = f_TrialSimulation.position[AXIS_X];
                    f_TrialEndPoint[AXIS_Y] = f_TrialSimulation.position[AXIS_Y];
                    f_TrialEndPoint[AXIS_Z] = f_TrialSimulation.position[AXIS_Z];
                }

                if (stepFlagsCopy & STEP_FLAG_LIPPED_OUT) {
                    f_TrialEvaluation += LIP_OUT_EVALUATION;
                    if (IsStrokeLimitReached()) {
                        f_TrialEvaluation += POOR_LANDING_EVALUATION;
                    }
                }
            }
            else if ((stepFlagsCopy & STEP_FLAG_GROUND_HIT) &&
                f_TrialSimulation.state == BALL_STATE_FLYING &&
                !f_HasLanded) {
                f_HasLanded = f_TrialSimulation.state;
                f_LandingPosition[AXIS_X] = f_TrialSimulation.position[AXIS_X];
                f_LandingPosition[AXIS_Y] = f_TrialSimulation.position[AXIS_Y];
                f_LandingPosition[AXIS_Z] = f_TrialSimulation.position[AXIS_Z];
            }

            if (f_BestEvaluation > f_TrialEvaluation) {
                f_PlanStage = PLAN_STAGE_TRIAL_RUNNING;
                return 0;
            }
        }
        while (FALSE);
    }

    if (!f_IsPutting) {
        f_TrialEndPoint[AXIS_X] = f_TrialSimulation.position[AXIS_X];
        f_TrialEndPoint[AXIS_Y] = f_TrialSimulation.position[AXIS_Y];
        f_TrialEndPoint[AXIS_Z] = f_TrialSimulation.position[AXIS_Z];
    }

    if (f_TrialSimulation.state == BALL_STATE_STOPPED) {
        s32 targetMiss;

        IntLength2D(
            g_HoleSetup.cupPosition[AXIS_X] - f_TrialEndPoint[AXIS_X],
            g_HoleSetup.cupPosition[AXIS_Z] - f_TrialEndPoint[AXIS_Z]
        );
        targetMiss = IntLength2D(
            f_PlanTarget[AXIS_X] - f_TrialEndPoint[AXIS_X],
            f_PlanTarget[AXIS_Z] - f_TrialEndPoint[AXIS_Z]
        );
        if (f_IsPutting) {
            targetMiss = IntLength2D(
                g_HoleSetup.cupPosition[AXIS_X] - f_TrialSimulation.position[AXIS_X],
                g_HoleSetup.cupPosition[AXIS_Z] - f_TrialSimulation.position[AXIS_Z]
            );
            if (IntLength2D(
                    f_TrialSimulation.position[AXIS_X] - f_TrialEndPoint[AXIS_X],
                    f_TrialSimulation.position[AXIS_Z] - f_TrialEndPoint[AXIS_Z]
                ) > PUTT_DRIFT_LIMIT) {
                f_TrialEvaluation = POOR_LANDING_EVALUATION;
            }

            // A long putt is evaluated even if it holed out. The goto into the
            // evaluation, to match: a flag set by an if here and tested with the
            // holed-out one gives 14 words differing, 8 in shape; the flag
            // assigned this test 21, 7; and this test moved into the holed-out
            // one's 35, 17.
            if (g_Players[f_Shot.playerIndex].strokeCount + 1 <
                    g_HoleSetup.par + f_HoleTargetToPar &&
                f_HoleTargetToPar > 0 &&
                IntLength2D(
                    f_Shot.ballPosition[AXIS_X] - f_TrialEndPoint[AXIS_X],
                    f_Shot.ballPosition[AXIS_Z] - f_TrialEndPoint[AXIS_Z]
                ) > LONG_PUTT_DISTANCE_MIN) {
                goto EVALUATE;
            }
        }

        if (f_TrialSimulation.isHoledOut) {
            f_TrialEvaluation = 0;
        }
        else {
EVALUATE:
            surfaceTraits = GetSurfaceTraits(f_TrialSimulation.surface);
            if (FindGroundHit(
                    f_TrialSimulation.position[AXIS_X],
                    f_TrialSimulation.position[AXIS_Y] - PATH_GROUND_CLEARANCE,
                    f_TrialSimulation.position[AXIS_Z],
                    g_HoleSetup.cupPosition[AXIS_X],
                    f_TrialSimulation.position[AXIS_Y] -
                        (IntLength2D(
                             f_TrialSimulation.position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
                             f_TrialSimulation.position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]
                         ) >>
                            1),
                    g_HoleSetup.cupPosition[AXIS_Z],
                    &hit,
                    &fraction
                )) {
                f_TrialEvaluation += BLOCKED_CUP_EVALUATION;
            }

            targetRating = GetLandingRating(
                GetSurfaceTraitsAt(f_PlanTarget[AXIS_X], f_PlanTarget[AXIS_Z])->kind
            );
            if (GetLandingRating(surfaceTraits->kind) < targetRating) {
                // In a block of its own to match, as the header comment explains
                IN_OWN_BLOCK({
                    f_TrialEvaluation += POOR_LANDING_EVALUATION;
                    if (IsOutOfBoundsKind(surfaceTraits->kind)) {
                        f_TrialEvaluation = LOST_BALL_EVALUATION;
                        f_IsTrialLost = TRUE;
                    }

                    if (surfaceTraits->kind == SURFACE_KIND_WATER_HAZARD) {
                        osSyncPrintf("Water Hazzad\n");
                        f_TrialEvaluation = LOST_BALL_EVALUATION;
                        f_IsTrialLost = TRUE;
                    }
                });
            }

            heightMiss = f_PlanTarget[AXIS_Y] - f_TrialSimulation.position[AXIS_Y];
            f_TrialEvaluation += (targetMiss + (Abs(heightMiss) / 2)) * MISS_EVALUATION_SCALE;
        }

        osSyncPrintf("BestEval = %d\n", f_BestEvaluation);
        if (f_BestEvaluation > f_TrialEvaluation) {
            f_HasBestEvaluation = TRUE;
            f_BestEvaluation = f_TrialEvaluation;
            f_TrialShot = f_Shot;
        }

        if (f_TrialIndex != 0) {
            strokeCount = g_Players[f_Shot.playerIndex].strokeCount;
            par = g_HoleSetup.par;
            if (GetSurfaceTraitsAt(f_PlanTarget[AXIS_X], f_PlanTarget[AXIS_Z])->kind ==
                SURFACE_KIND_GREEN) {
                // The stroke this shot would be. Counting it in strokes with ++
                // gives 4 words differing (none in shape), and adding it in the
                // subtraction 17 (3)
                s32 shotStroke = strokeCount + 1;

                strokesOver = shotStroke - (par + f_HoleTargetToPar);
                if (strokesOver >= 0) {
                    if (f_BestEvaluation <= f_AcceptableEvaluation) {
                        f_IsTrialAccepted = TRUE;
                        return 1;
                    }
                }
                else if (!f_IsPutting) {
                    if (strokesOver >= -1) {
                        if (f_BestEvaluation <=
                            f_AcceptableEvaluation + NEAR_TARGET_EVALUATION_SLACK) {
                            f_IsTrialAccepted = TRUE;
                            return 1;
                        }
                    }
                    else if (f_BestEvaluation <
                        f_AcceptableEvaluation + BEHIND_TARGET_EVALUATION_SLACK) {
                        f_IsTrialAccepted = TRUE;
                        return 1;
                    }
                }
                else {
                    approachDistance = (s32)IntLength2D(
                                           f_Shot.ballPosition[AXIS_X] - f_PlanTarget[AXIS_X],
                                           f_Shot.ballPosition[AXIS_Z] - f_PlanTarget[AXIS_Z]
                                       ) *
                        APPROACH_TOLERANCE_SCALE;
                    if (strokesOver >= -1) {
                        if (f_BestEvaluation <= (approachDistance * NEAR_TARGET_PUTT_SLACK_SHARE) +
                                f_AcceptableEvaluation) {
                            f_IsTrialAccepted = TRUE;
                            return 1;
                        }
                    }
                    else if (f_BestEvaluation <=
                        (approachDistance * BEHIND_TARGET_PUTT_SLACK_SHARE) +
                            f_AcceptableEvaluation) {
                        f_IsTrialAccepted = TRUE;
                        return 1;
                    }
                }
            }
            else if (f_BestEvaluation < f_AcceptableEvaluation + OFF_GREEN_EVALUATION_SLACK) {
                f_IsTrialAccepted = TRUE;
                return 1;
            }
        }

        if (!f_IsPutting) {
            flyDistance = IntLength2D(
                ((f_TrialSimulation.position[AXIS_X] + f_TrialEndPoint[AXIS_X]) / 2) -
                    f_Shot.ballPosition[AXIS_X],
                ((f_TrialSimulation.position[AXIS_Z] + f_TrialEndPoint[AXIS_Z]) / 2) -
                    f_Shot.ballPosition[AXIS_Z]
            );
            targetDistance = IntLength2D(
                f_PlanTarget[AXIS_X] - f_Shot.ballPosition[AXIS_X],
                f_PlanTarget[AXIS_Z] - f_Shot.ballPosition[AXIS_Z]
            );
        }
        else {
            aimCos = cosf((u16)f_Shot.heading * BINARY_ANGLE_TO_RADIANS);
            aimSin = sinf((u16)f_Shot.heading * BINARY_ANGLE_TO_RADIANS);
            flyDistance =
                (aimCos *
                    ((((f_TrialSimulation.position[AXIS_X] * 3) + f_TrialEndPoint[AXIS_X]) / 4) -
                        f_Shot.ballPosition[AXIS_X])) +
                (aimSin *
                    ((((f_TrialSimulation.position[AXIS_Z] * 3) + f_TrialEndPoint[AXIS_Z]) / 4) -
                        f_Shot.ballPosition[AXIS_Z]));
            targetDistance = (aimCos * (f_PlanTarget[AXIS_X] - f_Shot.ballPosition[AXIS_X])) +
                (aimSin * (f_PlanTarget[AXIS_Z] - f_Shot.ballPosition[AXIS_Z]));
            if (flyDistance <= 0) {
                flyDistance = 1;
            }

            if (targetDistance <= 0) {
                targetDistance = 1;
            }
        }

        osSyncPrintf("flydist = %d\n", flyDistance);
        osSyncPrintf("targetdist = %d\n", targetDistance);
        osSyncPrintf("powermode = %d\n", f_Shot.shotMode);
        osSyncPrintf("powermodeleft = %d\n", f_Shot.powerShotCount);
        osSyncPrintf("ClubSelectType = %d\n", ShotGolferTraits().clubBoldness);
        if (flyDistance != 0) {
            ratio = (f32)targetDistance / flyDistance;
            launchPower = f_TrialLaunch->power * ratio;
            if (launchPower > LAUNCH_POWER_MAX) {
                if (f_Shot.shotMode == SHOT_MODE_NORMAL &&
                    f_Shot.powerShotCount > 0 &&
                    ((ShotGolferTraits().powerShotEagerness != 0 &&
                         ShotGolferTraits().clubBoldness < BOLD_CLUB_CHOICE) ||
                        (rand() & 1)) &&
                    GetSurfaceTraitsAt(f_PlanTarget[AXIS_X], f_PlanTarget[AXIS_Z])->kind ==
                        SURFACE_KIND_GREEN) {
                    f_Shot.shotMode = SHOT_MODE_POWER;
                    osSyncPrintf("Power mode\n ");
                }
                else if (f_Shot.shotMode == SHOT_MODE_NORMAL &&
                    f_Shot.powerShotCount > 0 &&
                    ShotGolferTraits().clubBoldness == BOLD_CLUB_CHOICE) {
                    f_Shot.shotMode = SHOT_MODE_POWER;
                    osSyncPrintf("Power mode\n ");
                }
                else if (f_Shot.shotMode == SHOT_MODE_APPROACH) {
                    f_Shot.shotMode = SHOT_MODE_NORMAL;
                }
                else if (targetDistance < flyDistance) {
                    osSyncPrintf("Level Down\n");
                    f_NeedsShorterClub = TRUE;
                    return 1;
                }
                else if (f_HasFirstFallHit || ratio >= BLOCKED_POWER_RATIO) {
                    f_AimChangeCount++;
                    f_Shot.heading +=
                        (!(f_AimChangeCount & 1) ? -1 : 1) * (f_AimChangeCount * AIM_CHANGE_STEP);
                    osSyncPrintf("Angle Change3\n");
                    for (index = 0; index < EXCLUDED_SECTOR_COUNT; index++) {
                        f_IsDirectionExcluded[(u8)((f_Shot.heading >> DIRECTION_SECTOR_SHIFT) +
                            index - (EXCLUDED_SECTOR_COUNT / 2))] = TRUE;
                    }

                    if (f_AimChangeCount >= BLOCKED_TRIAL_AIM_CHANGES_MAX) {
                        osSyncPrintf("Level Down\n");
                        f_AimChangeCount = 0;
                        f_NeedsShorterClub = TRUE;
                    }

                    return 1;
                }
                else {
                    f_NeedsLongerClub = TRUE;
                    osSyncPrintf("Level Up1\n");
                    return 1;
                }
            }
            else {
                // The power gauge frame for a launch power out of 256
                s32 GetPowerGaugeFrame(s32 amount) {
                    s32 gaugeFrame = (amount * POWER_TO_GAUGE_FRAME_SCALE) + 0.5f;

                    if (gaugeFrame < POWER_GAUGE_FRAME_MIN) {
                        gaugeFrame = POWER_GAUGE_FRAME_MIN;
                    }

                    if (gaugeFrame > GAUGE_FULL_FRAME - 1) {
                        gaugeFrame = GAUGE_FULL_FRAME;
                    }

                    return gaugeFrame;
                }

                f_Shot.powerGaugeFrame = GetPowerGaugeFrame(launchPower);
                if (f_IsPutting) {
                    f_Shot.powerGaugeFrame = f_Shot.powerGaugeFrame + ((rand() & 1) + 1);
                }

                if (f_Shot.powerGaugeFrame > GAUGE_FULL_FRAME) {
                    f_Shot.powerGaugeFrame = GAUGE_FULL_FRAME;
                }

                if (f_Shot.powerGaugeFrame < 2) {
                    f_Shot.powerGaugeFrame = 2;
                }

                if (f_TrialLaunch->power < launchPower) {
                    if (GetGaugeFramePower(f_Shot.powerGaugeFrame) <= f_TrialLaunch->power &&
                        f_Shot.powerGaugeFrame < GAUGE_FULL_FRAME) {
                        f_Shot.powerGaugeFrame++;
                    }
                }
                else if (launchPower < f_TrialLaunch->power) {
                    if (GetGaugeFramePower(f_Shot.powerGaugeFrame) >= f_TrialLaunch->power &&
                        f_Shot.powerGaugeFrame >= 4) {
                        f_Shot.powerGaugeFrame--;
                    }
                }
            }
        }

        f_Shot.heading += (s32)(atan2(
                                    f_PlanTarget[AXIS_Z] - f_Shot.ballPosition[AXIS_Z],
                                    f_PlanTarget[AXIS_X] - f_Shot.ballPosition[AXIS_X]
                                ) *
                              RADIANS_TO_BINARY_ANGLE) -
            (s32)(atan2(
                      f_TrialEndPoint[AXIS_Z] - f_Shot.ballPosition[AXIS_Z],
                      f_TrialEndPoint[AXIS_X] - f_Shot.ballPosition[AXIS_X]
                  ) *
                RADIANS_TO_BINARY_ANGLE);
    }
    else {
        if (stepFlags & STEP_FLAG_GROUND_HIT) {
            if (!(GetClub(g_Players[f_Shot.playerIndex].clubs[f_Shot.clubSlot])->surfaceMask &
                    CLUB_SURFACE_GREEN)) {
                f_Shot.clubSlot++;
                if (GetClub(g_Players[f_Shot.playerIndex].clubs[f_Shot.clubSlot])->surfaceMask &
                    CLUB_SURFACE_GREEN) {
                    f_Shot.clubSlot--;
                }
            }
        }

        f_AimChangeCount++;
        for (index = 0; index < EXCLUDED_SECTOR_COUNT; index++) {
            f_IsDirectionExcluded[(u8)((f_Shot.heading >> DIRECTION_SECTOR_SHIFT) + index -
                (EXCLUDED_SECTOR_COUNT / 2))] = TRUE;
        }

        f_ShotOffset = SHOT_OFFSET_MINUS_LARGE;
        f_Shot.heading += (!(f_AimChangeCount & 1) ? -1 : 1) * (f_AimChangeCount * AIM_CHANGE_STEP);

        osSyncPrintf("Angle Change4\n");
        if (f_AimChangeCount >= UNSTOPPED_TRIAL_AIM_CHANGES_MAX) {
            osSyncPrintf("Level Down\n");
            f_AimChangeCount = 0;
            f_NeedsShorterClub = TRUE;
        }
    }

    return 1;
}

// Returns the slot of the golfer's club of type clubType, or 0 if there is none
ClubSlot FindClubSlot(s32 unused UNUSED, ClubType clubType) {
    ClubSlot clubSlot;

    for (clubSlot = 0; clubSlot < CLUB_SLOT_COUNT; clubSlot++) {
        if (GetClub(g_Players[f_Shot.playerIndex].clubs[clubSlot])->type == clubType) {
            return clubSlot;
        }
    }

    return 0;
}

// What rain leaves of a shot's distance, for golfers it shortens
#define RAINY_DISTANCE_FRACTION 0.95f

// Returns the type of the shortest usable club, from the sand wedge up, whose
// normal shot carries beyond distance from the ball's lie. A club the golfer
// would hit from the worst lie only counts if the golfer's club boldness is
// BOLD_CLUB_CHOICE. If no club carries that far, it returns the slot of the
// longest club that counted, or CLUB_TYPE_SAND_WEDGE if none did.
ClubType ChooseClubForDistance(s32 distance) {
    ClubType fallback = CLUB_TYPE_SAND_WEDGE;
    ClubType clubType;
    ClubSlot clubSlot;
    Club *club;
    s32 carry;
    LieRating rating;

    for (clubType = CLUB_TYPE_SAND_WEDGE; clubType >= 0; clubType--) {
        clubSlot = FindClubSlot(f_Shot.playerIndex, clubType);
        if (!f_IsClubUsable[clubSlot]) {
            continue;
        }

        club = GetClub(g_Players[f_Shot.playerIndex].clubs[clubSlot]);
        carry = GetShotWorldDistance(club, SHOT_MODE_NORMAL);
        if (g_HoleSetup.conditions.isRaining == TRUE &&
            ShotGolferTraits().consistency < CONSISTENCY_HIGH) {
            carry = carry * RAINY_DISTANCE_FRACTION;
        }

        carry = carry * GetSurfaceTraits(f_Shot.surface)->percentMin / PERCENT_PER_WHOLE;
        rating = GetLieRating(club, f_Shot.surface, SHOT_MODE_NORMAL);
        if (ShotGolferTraits().clubBoldness < BOLD_CLUB_CHOICE) {
            if (rating == LIE_RATING_MIN) {
                continue;
            }
        }

        fallback = clubSlot;
        if (distance < carry) {
            return clubType;
        }
    }

    return fallback;
}

// Returns how far apart two binary angles are, up to half a turn
WideBinaryAngle GetAngleDifference(WideBinaryAngle angle, WideBinaryAngle otherAngle) {
    WideBinaryAngle difference = (s16)(angle - otherAngle);

    if (difference < 0) {
        difference = -difference;
    }

    return difference;
}

// How far below the ball a target must be for the largest minus offset
#define DOWNHILL_TARGET_DROP 0xB400
// The angle within which the wind counts as following the shot
#define FOLLOWING_WIND_ANGLE 10000

// It is a macro, as an inline function compiles differently from the original
#define IsWindSpeedInRange(lowest, limit) \
    (g_HoleSetup.conditions.windSpeed >= (lowest) && g_HoleSetup.conditions.windSpeed < (limit))

Boolean RollClubCategoryChance(ClubChanceSet chanceSet);

// Chooses the offset to give the planned shot, from the lie, the golfer's
// windPlay trait, whether the shot aims at the cup, how far below the ball
// the target is, and the wind's speed and direction against the shot's yaw.
// Most offsets also need RollClubKindChance to succeed.
ShotOffsetIndex ChooseShotOffset(void) {
    WindPlay windPlay;
    WideBinaryAngle aimYaw;
    WideBinaryAngle windDirection;

    switch (GetSurfaceTraits(f_Shot.surface)->kind) {
        case SURFACE_KIND_BUNKER:
        case SURFACE_KIND_BUNKER_FAIR_LIE:
        case SURFACE_KIND_BUNKER_POOR_LIE:
        case SURFACE_KIND_BUNKER_UNUSED_LIE:
        case SURFACE_KIND_GREEN:
        case SURFACE_KIND_GREEN_EDGE:
        case SURFACE_KIND_BARE_GROUND:
        case SURFACE_KIND_DEEP_ROUGH:
        case SURFACE_KIND_NO_DROP_GROUND:
        case SURFACE_KIND_WASTE_AREA:
            return SHOT_OFFSET_NONE;
    }

    windPlay = ShotGolferTraits().windPlay;
    if (f_PlanTarget[AXIS_X] == g_HoleSetup.cupPosition[AXIS_X] &&
        f_PlanTarget[AXIS_Z] == g_HoleSetup.cupPosition[AXIS_Z]) {
        switch (windPlay) {
            case WIND_PLAY_HEAD_WINDS:
                if (rand() % 2 == 1) {
                    return SHOT_OFFSET_PLUS_MEDIUM;
                }

                break;
            case WIND_PLAY_STRONG_WINDS:
                if (rand() % 3 == 0) {
                    return SHOT_OFFSET_PLUS_MEDIUM;
                }

                break;
        }
    }

    if (f_PlanTarget[AXIS_Y] < f_Shot.ballPosition[AXIS_Y] - DOWNHILL_TARGET_DROP) {
        return SHOT_OFFSET_MINUS_LARGE;
    }

    aimYaw = (u16)f_Shot.heading;
    windDirection = g_HoleSetup.conditions.windDirection;
    osSyncPrintf("skSCAI.angy = %ld\n", aimYaw);
    osSyncPrintf("wind_ang = %ld\n", windDirection);
    if (GetAngleDifference(windDirection, aimYaw) < FOLLOWING_WIND_ANGLE) {
        osSyncPrintf("---- Follow\n");
        if (windPlay == WIND_PLAY_FOLLOWING_WINDS) {
            if (IsWindSpeedInRange(4 * WIND_PUSH_UNIT, 5 * WIND_PUSH_UNIT)) {
                if (RollClubCategoryChance(CLUB_CHANCE_SET_FOLLOWING_WIND) == TRUE) {
                    return SHOT_OFFSET_MINUS_SMALL;
                }
            }
            else if (IsWindSpeedInRange(5 * WIND_PUSH_UNIT, 7 * WIND_PUSH_UNIT)) {
                if (RollClubCategoryChance(CLUB_CHANCE_SET_FOLLOWING_WIND) == TRUE) {
                    return SHOT_OFFSET_MINUS_MEDIUM;
                }
            }
            else if (g_HoleSetup.conditions.windSpeed >= 9 * WIND_PUSH_UNIT) {
                if (RollClubCategoryChance(CLUB_CHANCE_SET_FOLLOWING_WIND) == TRUE) {
                    return SHOT_OFFSET_MINUS_LARGE;
                }
            }
        }
        else if (windPlay == WIND_PLAY_STRONG_WINDS) {
            if (IsWindSpeedInRange(6 * WIND_PUSH_UNIT, 7 * WIND_PUSH_UNIT)) {
                if (RollClubCategoryChance(CLUB_CHANCE_SET_FOLLOWING_WIND) == TRUE) {
                    return SHOT_OFFSET_MINUS_SMALL;
                }
            }
            else if (g_HoleSetup.conditions.windSpeed >= 7 * WIND_PUSH_UNIT) {
                if (RollClubCategoryChance(CLUB_CHANCE_SET_FOLLOWING_WIND) == TRUE) {
                    return SHOT_OFFSET_MINUS_MEDIUM;
                }
            }
        }
    }
    else {
        osSyncPrintf("---- back\n");
        if (windPlay == WIND_PLAY_HEAD_WINDS) {
            if (IsWindSpeedInRange(4 * WIND_PUSH_UNIT, 5 * WIND_PUSH_UNIT)) {
                if (RollClubCategoryChance(CLUB_CHANCE_SET_HEAD_WIND) == TRUE) {
                    return SHOT_OFFSET_PLUS_SMALL;
                }
            }
            else if (IsWindSpeedInRange(5 * WIND_PUSH_UNIT, 7 * WIND_PUSH_UNIT)) {
                if (RollClubCategoryChance(CLUB_CHANCE_SET_HEAD_WIND) == TRUE) {
                    return SHOT_OFFSET_PLUS_MEDIUM;
                }
            }
            else if (g_HoleSetup.conditions.windSpeed >= 9 * WIND_PUSH_UNIT) {
                if (RollClubCategoryChance(CLUB_CHANCE_SET_HEAD_WIND) == TRUE) {
                    return SHOT_OFFSET_PLUS_LARGE;
                }
            }
        }
        else if (windPlay == WIND_PLAY_STRONG_WINDS) {
            if (IsWindSpeedInRange(6 * WIND_PUSH_UNIT, 7 * WIND_PUSH_UNIT)) {
                if (RollClubCategoryChance(CLUB_CHANCE_SET_HEAD_WIND) == TRUE) {
                    return SHOT_OFFSET_PLUS_SMALL;
                }
            }
            else if (g_HoleSetup.conditions.windSpeed >= 7 * WIND_PUSH_UNIT) {
                if (RollClubCategoryChance(CLUB_CHANCE_SET_HEAD_WIND) == TRUE) {
                    return SHOT_OFFSET_PLUS_MEDIUM;
                }
            }
        }
    }

    return SHOT_OFFSET_NONE;
}

#define CLUB_TYPE_LAST_WOOD 2
#define CLUB_TYPE_FIRST_IRON 3
#define CLUB_TYPE_LAST_IRON 10

// Whether the shot's club lies between the slots of the two club types
// It is a macro, as an inline function compiles differently from the original
#define IsShotClubOfTypes(firstType, lastType) \
    (f_Shot.clubSlot >= FindClubSlot(f_Shot.playerIndex, (firstType)) && \
        FindClubSlot(f_Shot.playerIndex, (lastType)) >= f_Shot.clubSlot)

// Returns TRUE on the golfer's first stroke of the hole. Otherwise it returns
// TRUE with the percent chance that row chanceSetIndex of f_ClubKindChances
// gives for the kind of the shot's club.
Boolean RollClubCategoryChance(ClubChanceSet chanceSet) {
    if (g_Players[f_Shot.playerIndex].strokeCount == 0) {
        return TRUE;
    }

    if (IsShotClubOfTypes(CLUB_TYPE_DRIVER, CLUB_TYPE_LAST_WOOD)) {
        if (rand() % PERCENT_PER_WHOLE < f_ClubCategoryChances[chanceSet][CLUB_CHANCE_WOOD]) {
            return TRUE;
        }
    }
    else if (IsShotClubOfTypes(CLUB_TYPE_FIRST_IRON, CLUB_TYPE_LAST_IRON)) {
        if (rand() % PERCENT_PER_WHOLE < f_ClubCategoryChances[chanceSet][CLUB_CHANCE_IRON]) {
            return TRUE;
        }
    }
    else {
        if (rand() % PERCENT_PER_WHOLE < f_ClubCategoryChances[chanceSet][CLUB_CHANCE_OTHER]) {
            return TRUE;
        }
    }

    return FALSE;
}

// Tens of yards in a unit of distance
#define TENS_OF_YARDS_PER_UNIT 0.000007119878f
// The fraction of a line's points past which the line counts as lost
#define LOST_LINE_FRACTION 0.5f

// Returns whether more than half of the points a yard apart on the first tenth
// of the line from from to to lie in water, out of bounds or out of the course.
Boolean IsLineMostlyLost(CoursePoint from, CoursePoint to) {
    s32 distance;
    Radians angle;
    s32 sampleCount;
    s32 step;
    s32 lostCount;
    f32 stepLength;
    s32 x;
    s32 z;
    SurfaceKind surfaceKind;

    distance = IntLength2D(to[AXIS_X] - from[AXIS_X], to[AXIS_Z] - from[AXIS_Z]);
    angle = atan2((f32)(to[AXIS_Z] - from[AXIS_Z]), (f32)(to[AXIS_X] - from[AXIS_X]));
    sampleCount = 0;
    lostCount = 0;
    for (step = 1; step < distance * TENS_OF_YARDS_PER_UNIT; step++) {
        // Setting stepLength in the loop is there to match: setting it before
        // the loop gives 8 words differing (8 in shape), and the constant in
        // its place 26
        stepLength = UNITS_PER_YARD;
        x = (step * (cosf(angle) * stepLength)) + from[AXIS_X];
        z = (step * (sinf(angle) * stepLength)) + from[AXIS_Z];
        surfaceKind = GetSurfaceTraitsAt(x, z)->kind;
        if (IsLostBallKind(surfaceKind)) {
            lostCount++;
        }

        sampleCount++;
    }

    if ((f32)lostCount / sampleCount > LOST_LINE_FRACTION) {
        osSyncPrintf("Many OB!!\n");
        return TRUE;
    }

    return FALSE;
}

// Simulates f_TrialShot and returns TRUE if the ball comes to rest, and not out
// of bounds, off the course or in a water hazard
//
// Its loop is a cut-down copy of StepTrialShot's step, and it keeps that step's
// read of the fall speed, which nothing here uses, to match. The original holds
// &f_TrialSimulation.velocity[AXIS_Y] in $s0 through the loop, passes
// StepBallSimulation that address less 0x28 and reads the loop's state 0x64
// past it, and nothing in the loop reads the fall speed itself. GCC expands
// the read as the address into a register and a load through it; jump deletes
// the load, whose value is unused, and leaves the address, and cse's
// use_related_value then derives the simulation's address and the state's
// from it. That leaves the simulation's address with one use, the call's
// argument, and in a loop with calls loop.c substitutes such a register's
// value into its use rather than move it out of the loop.
//
// Without the read 6 words differ, 4 in shape: the loop keeps the simulation's
// own address and reads the state from its constant address. Reading the
// bounce count as well, as StepTrialShot does, matches too, as do the read
// after the count, the read declared in the loop's body and an empty test of
// the fall speed. The read after the step or before the loop gives the same 6
// words (4), and through a simulation pointer 14 (6); a read of the bounce
// count alone, the state or another axis's speed gives 6 (0), biased by that
// field's offset instead.
//
// The step limit's report is written inside the loop and the original has it
// after the result's checks, jumping back to them: loop.c moves a block that
// ends in a jump out of its loop to a barrier outside it, here the one that
// ends the first FALSE report. The original's rodata has the FALSE text before
// the TRUE one, though the TRUE report's code comes first, so the FALSE report
// is written twice, before and after the TRUE one, and jump's cross-jumping
// then merges the first into the last. An else arm for the TRUE report matches
// too. Testing the out-of-bounds kinds and the water hazard in separate ifs,
// each with its own FALSE report, gives 8 words (6 in shape); the TRUE report
// first, with one FALSE report after it, 24 (18); a result flag tested for the
// reports 31 (25); a FALSE report for a ball that hasn't stopped before the
// kind's tests 44 (34); and one condition for all three tests 18 (12). The
// outside-course test before the step limit's gives 9 (5).
Boolean IsTrialShotSafe(void) {
    SurfaceKind surfaceKind;
    StepFlags stepFlags;
    // Read and never used, to match
    s32 velocityY UNUSED;

    f_TrialLaunch->heading = f_TrialShot.heading;
    f_TrialLaunch->clubSlot = f_TrialShot.clubSlot;
    f_TrialLaunch->power = GetGaugeFramePower(f_TrialShot.powerGaugeFrame);
    f_TrialLaunch->impactTiming = f_TrialShot.impactGaugeFrame - 60;
    f_TrialLaunch->offsetX = f_TrialShot.offsetX;
    f_TrialLaunch->offsetY = f_TrialShot.offsetY;
    f_TrialLaunch->playerIndex = f_TrialShot.playerIndex;
    f_TrialLaunch->shotMode = f_TrialShot.shotMode;
    f_TrialSimulation.position[AXIS_X] = f_TrialShot.ballPosition[AXIS_X];
    f_TrialSimulation.position[AXIS_Y] = f_TrialShot.ballPosition[AXIS_Y];
    f_TrialSimulation.position[AXIS_Z] = f_TrialShot.ballPosition[AXIS_Z];
    f_TrialSimulation.surface = f_TrialShot.surface;
    LaunchBallSimulation(&f_TrialSimulation, LAUNCH_PURPOSE_TRIAL, f_TrialLaunch);
    f_TrialStepCount = 0;
    while (f_TrialSimulation.state != BALL_STATE_STOPPED) {
        velocityY = f_TrialSimulation.velocity[AXIS_Y];
        f_TrialStepCount++;
        stepFlags = StepBallSimulation(&f_TrialSimulation);
        if (f_TrialStepCount >= STEP_COUNT_MAX) {
            SetDebugTextHighlight(TRUE);
            PrintDebugText(
                "AI Sim Fault!",
                SIMULATION_FAULT_TEXT_COLUMN,
                SIMULATION_FAULT_TEXT_ROW
            );
            break;
        }

        if (stepFlags & STEP_FLAG_OUTSIDE_COURSE) {
            break;
        }
    }

    if (f_TrialSimulation.state == BALL_STATE_STOPPED) {
        surfaceKind = GetSurfaceTraits(f_TrialSimulation.surface)->kind;
        if (IsOutOfBoundsKind(surfaceKind) || surfaceKind == SURFACE_KIND_WATER_HAZARD) {
            osSyncPrintf("nmResultCheck: FALSE\n");
            return FALSE;
        }

        osSyncPrintf("nmResultCheck: TRUE\n");
        return TRUE;
    }

    osSyncPrintf("nmResultCheck: FALSE\n");
    return FALSE;
}
