#include "ball_simulation.h"
#include "club.h"
#include "common.h"
#include "course.h"
#include "current_shot.h"
#include "debug_text.h"
#include "functions.h"
#include "game_flags.h"
#include "hole_loader.h"
#include "libc.h"
#include "math_util.h"
#include "save.h"
#include "scene.h"
#include "surface.h"
#include "variables.h"

/* The shot planning thread's states, in sPlanThreadState */
#define PLAN_THREAD_RUNNING 0
#define PLAN_THREAD_COLLECTED 1
#define PLAN_THREAD_FINISHED 2
#define PLAN_THREAD_ID 11
#define PLAN_THREAD_PRIORITY 20
/* PlanShot's result while it has more to do */
#define PLAN_UNFINISHED (-1)

/* How a character plays when the computer controls it */
typedef struct GolferTraits {
    /* 0x0 */ s8 unk0;
    /* 0x1 */ s8 unk1;
    /* 0x2 */ s8 unk2;
    /* 0x3 */ s8 unk3;
    /* 0x4 */ s8 unk4;
    /* 0x5 */ s8 windPlay; /* WIND_PLAY_*: which winds the golfer offsets shots for */
    /* 0x6 */ s8 unk6;
    /* 0x7 */ s8 targetScores[COURSE_COUNT]; /* The score to par it aims at on each course */
    /* 0xD */ s8 unkD;                      /* Added to the target score */
} GolferTraits; /* size = 0xE */

/* GolferTraits.windPlay values */
#define WIND_PLAY_HEAD_WINDS 0      /* Offsets shots for head winds */
#define WIND_PLAY_STRONG_WINDS 1    /* Offsets shots for the strongest winds either way */
#define WIND_PLAY_FOLLOWING_WINDS 2 /* Offsets shots for following winds */

extern GolferTraits sGolferTraits[CHARACTER_COUNT];
extern s32 sPlanThreadState;
extern OSThread sPlanThread;
extern CurrentShot sPlannedShot;
/* The directions from the ball, in BINARY_ANGLE_TURN / DIRECTION_SECTOR_COUNT sectors */
#define DIRECTION_SECTOR_COUNT 256
#define DIRECTION_SECTOR_SHIFT 8
/* The direction sectors in which no target is chosen */
extern s8 sIsDirectionExcluded[DIRECTION_SECTOR_COUNT];
extern u8 sPlanThreadStackEnd[];
/* sHoleStrokeTarget less the hole's par */
extern s32 sHoleTargetToPar;
/* The strokes the golfer aims to take on the hole */
extern s32 sHoleStrokeTarget;
extern s32 D_801B60B4;
/* Where the planned shot aims */
extern CoursePoint sPlanTarget;
/* The route points and the tee, as PlanRoute gathers them */
typedef struct RouteCoordinates {
    /* 0x00 */ s32 x[ROUTE_POINT_COUNT + 1];
    /* 0x24 */ s32 z[ROUTE_POINT_COUNT + 1];
} RouteCoordinates; /* size = 0x48 */

extern RouteCoordinates sRoute;
/* The route points nearest the cup first, as PlanRoute orders them */
extern s8 sRouteOrder[ROUTE_POINT_COUNT];
extern s32 sRouteOrderCount;
/* How far PlanShot has got with the shot */
extern s8 sPlanStage;
#define PLAN_STAGE_UNSTARTED 0
#define PLAN_STAGE_STARTED 1
#define PLAN_STAGE_TARGET_CHOSEN 2
#define PLAN_STAGE_TRIAL_TAKEN 3
#define PLAN_STAGE_UNK4 4
#define PLAN_STAGE_LEVEL_FINISHED 5
#define PLAN_STAGE_TRIAL_RUNNING 6
extern s8 sIsClubUsable[CLUB_SLOT_COUNT];
/* The size of the golfer's club set */
extern s32 sClubCount;

/* The rows of sClubKindChances */
#define CLUB_CHANCE_SET_COUNT 2
/* Woods, irons and the other clubs */
#define CLUB_CHANCE_KIND_COUNT 3
extern s8 sClubKindChances[CLUB_CHANCE_SET_COUNT][CLUB_CHANCE_KIND_COUNT];
extern CurrentShot sShot;

s32 PlanShot(CurrentShot *plannedShot);
s32 PlanCupDistance(s32 arg0);
s32 FindGreenEdgeOnLine(CoursePoint from, CoursePoint to, CoursePoint edge);
s32 ChooseClubForDistance(s32 distance);

void RestartShotPlan(void) {
    sPlanStage = PLAN_STAGE_UNSTARTED;
}

/* Starts planning the shot that sets g_CurrentShot up, on its own thread */
void StartShotPlan(void) {
    void RunShotPlan(void *arg);

    RestartShotPlan();
    sPlanThreadState = PLAN_THREAD_RUNNING;
    osCreateThread(&sPlanThread, PLAN_THREAD_ID, RunShotPlan, NULL, sPlanThreadStackEnd, PLAN_THREAD_PRIORITY);
    osStartThread(&sPlanThread);
}

/* The planning thread, which waits to be destroyed once the plan is made */
void RunShotPlan(void *arg) {
    while (PlanShot(&sPlannedShot) == PLAN_UNFINISHED) {
    }
    sPlanThreadState = PLAN_THREAD_FINISHED;
    while (TRUE) {
    }
}

/*
 * Copies the planned shot to shot. It returns -1 while the plan is unfinished,
 * and 0 once it is made, when it ends the planning thread.
 */
s32 GetPlannedShot(CurrentShot *shot) {
    if (sPlanThreadState != PLAN_THREAD_RUNNING) {
        *shot = sPlannedShot;
        if (sPlanThreadState == PLAN_THREAD_FINISHED) {
            osDestroyThread(&sPlanThread);
            sPlanThreadState = PLAN_THREAD_COLLECTED;
        }
        return 0;
    }
    return -1;
}

s32 GetGolferTraitUnk2(void) {
    return sGolferTraits[(s8)g_Players[sShot.playerIndex].characterIndex].unk2;
}

s32 IsStrokeLimitReached(void) {
    return g_Players[sShot.playerIndex].unk23 >= g_HolePar + sHoleTargetToPar - 1;
}

/* Course units, of which there are 1024 to a world unit */
#define SIDE_SLOPE_MIN_DISTANCE 15360.0f
#define SIDE_SLOPE_SAMPLE_SPACING 7680.0f
#define SIDE_SLOPE_SAMPLE_REACH 1536.0f
#define SIDE_SLOPE_STEP 64
#define SIDE_SLOPE_LIMIT 512

/*
 * Returns how the ground slopes across the line from the ball to the cup: the
 * count of points along the line where the ground is lower toward (-z, x) of
 * the line than the other way, less the count where it is higher, times
 * SIDE_SLOPE_STEP. It returns 0 for a cup within SIDE_SLOPE_MIN_DISTANCE.
 */
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
    direction[AXIS_X] = D_800BA9D8.cupPosition[AXIS_X] - sShot.ballPosition[AXIS_X];
    direction[AXIS_Z] = g_CupPositionZ - sShot.ballPosition[AXIS_Z];
    distance = Length2D(direction[AXIS_X], direction[AXIS_Z]);
    sideSlope = 0;
    if (distance < SIDE_SLOPE_MIN_DISTANCE) {
        return 0;
    }
    guNormalize(&direction[AXIS_X], &direction[AXIS_Y], &direction[AXIS_Z]);
    reachX = 2.0f * (-direction[AXIS_Z] * SIDE_SLOPE_SAMPLE_REACH);
    reachZ = 2.0f * (direction[AXIS_X] * SIDE_SLOPE_SAMPLE_REACH);
    for (sampleIndex = 1; sampleIndex < (s32)(distance * (1.0f / SIDE_SLOPE_SAMPLE_SPACING)); sampleIndex++) {
        sampleX = sShot.ballPosition[AXIS_X] + (along = sampleIndex * SIDE_SLOPE_SAMPLE_SPACING) * direction[AXIS_X];
        sampleZ = sShot.ballPosition[AXIS_Z] + along * direction[AXIS_Z];
        reachHeight = GetGroundHeight(sampleX + reachX, sampleZ + reachZ);
        oppositeHeight = GetGroundHeight(sampleX - reachX, sampleZ - reachZ);
        if (reachHeight > oppositeHeight) {
            sideSlope--;
        } else if (reachHeight < oppositeHeight) {
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

/* Where the shot to try starts, as PlanShot sets it up */
extern CurrentShot sTrialShot;
extern Unk20 *sTrialLaunch;
extern BallSimulation sTrialSimulation;
extern s32 sTrialStepCount;

s32 func_8005F248(s32 value);

/* ChooseShotOffset's results, indices into sShotOffsets */
#define SHOT_OFFSET_NONE 0
#define SHOT_OFFSET_PLUS_SMALL 1
#define SHOT_OFFSET_PLUS_MEDIUM 2
#define SHOT_OFFSET_PLUS_LARGE 3
#define SHOT_OFFSET_MINUS_SMALL 4
#define SHOT_OFFSET_MINUS_MEDIUM 5
#define SHOT_OFFSET_MINUS_LARGE 6

/* The spin-like launch values that each SHOT_OFFSET_* gives a shot */
typedef struct ShotOffset {
    /* 0x0 */ s8 unk14; /* Becomes sShot.unk14 */
    /* 0x1 */ s8 unk18; /* Becomes sShot.unk18 */
} ShotOffset; /* size = 0x2 */

#define SHOT_OFFSET_COUNT 8

extern ShotOffset sShotOffsets[SHOT_OFFSET_COUNT];
/* The SHOT_OFFSET_* that PlanShot chose for the shot */
extern s32 sShotOffset;
/* Whether the trial's club is one for the green */
extern s32 sIsPutting;
/* How much worse than ideal an evaluation may be for the trial to be taken */
extern s32 sAcceptableEvaluation;
/* How many times PlanShot has changed the club or carry */
extern s32 sTrialLevel;
/* Whether the trial has reported STEP_FLAG_UNK1, and where it first did */
extern s32 sHasUnk1Position;
extern CoursePoint sUnk1Position;
/* Whether the trial ended out of bounds, off the course or in water */
extern s32 sIsTrialLost;
/* The nearest that a putt has come to the target */
extern s32 sClosestTargetDistance;
/* The count of trials at this level */
extern s32 sTrialIndex;
/* Set when the trial asks PlanShot for a longer or a shorter club */
extern s32 sNeedsLongerClub;
extern s32 sNeedsShorterClub;
/* The trial's evaluation; lower is better */
extern s32 sTrialEvaluation;
/* Where the trial's ball rests, or for a putt where it passes nearest the target */
extern CoursePoint sTrialEndPoint;
/* Set when the trial is good enough to take */
extern s32 sIsTrialAccepted;
/* The best evaluation so far, and whether there is one */
extern s32 sBestEvaluation;
extern s32 sHasBestEvaluation;
extern s32 sAimChangeCount;
extern s32 sHasUnk3Contact;
/* The evaluation of a lost ball, and the one that the best starts at */
#define LOST_BALL_EVALUATION 0x10000000
/* Whether sTrialShot holds a copy of sShot yet */
extern s32 sHasTrialShot;
/* Set when the level needs a new landing target */
extern s32 sNeedsNewTarget;
/* The carry that the landing target is chosen for, and what the club's carry is scaled by for it */
extern s32 sTargetCarry;
extern f32 sCarryScale;
/* The count of trials to take at the level */
extern s32 sLevelTrialCount;
/* The count of trials that ended with the ball lost */
extern s32 sLostTrialCount;
/* PlanShot clears these, and nothing in the overlay reads them */
extern s32 sUnk658;
extern s8 sUnk840;
extern Unk20 sTrialLaunchValues;

s32 StepTrialShot(void);
s32 IsTrialShotSafe(void);
s32 ChooseShotOffset(void);
s32 FindClubSlot(s32 unused, s32 clubType);
s32 PlanRoute(s8 *route);
void ChooseLandingTarget(s32 x, s32 z, s32 distance, s32 *targetX, s32 *targetZ);
void PlanClubAndTarget(CoursePoint target);
void ChooseLongShot(s32 unused, s32 *clubType, s32 *targetX, s32 *targetZ, s32 *shotMode);
s32 MarkUsableClubs(s32 unused, s16 *clubs, s8 *isUsable);
void GetSurfacePercentRange(SurfaceInfo *info, Club *club, s32 *range);

/* sShot.unk4's range, which ScaleToShotUnk4 maps 0 to 256 onto */
#define SHOT_UNK4_MIN 3
#define SHOT_UNK4_MAX 30
/* The levels that PlanShot always tries, tries while the best evaluation is poor, and tries at most */
#define MIN_TRIAL_LEVEL_COUNT 4
#define POOR_TRIAL_LEVEL_COUNT 8
#define MAX_TRIAL_LEVEL_COUNT 30
#define POOR_EVALUATION 0x10000
#define INITIAL_CARRY_SCALE 1.1f
#define CARRY_SCALE_STEP 0.9f
#define RAINY_CARRY_SCALE 0.95f
#define RAINY_DISTANCE_SCALE 1.1f
/* A club is swapped for the next while its shortest reach is at least this and fits the distance fewer times */
#define MIN_CLUB_DOWN_REACH 0x600
#define CLUB_DOWN_REACH_COUNT 21
/* How far from the cup a golfer ahead of its target aims on purpose */
#define MAX_MISS_DISTANCE 0x25800
#define LEVEL_MISS_DISTANCE 0x1E00
#define MISS_STRETCH 2.5f
/* How many times PlanShot reports a deliberate miss */
#define MISS_REPORT_COUNT 40
/* The whole of a surface's percent range */
#define FULL_PERCENT 100
#define DEFAULT_SHOT_UNK8 60
/* The least sShot.unk4 that PlanShot plans, and how much it scales it up by for some golfers */
#define MIN_PLANNED_SHOT_UNK4 2
#define SHOT_UNK4_BOOST 1.2f
/* The trials taken at each level, at a level that aims at the cup, and under GAME_FLAG_UNK3C */
#define LEVEL_TRIAL_COUNT 4
#define CUP_LEVEL_TRIAL_COUNT 5
#define UNK3C_LEVEL_TRIAL_COUNT 20
/* A mis-hit is a roll below MISHIT_ROLL_RANGE divided by a golfer's divisor, and the sign bit sets its direction */
#define MISHIT_ROLL_RANGE 250
#define MISHIT_SIGN_BIT 0x2000

/*
 * Plans the computer golfer's shot from g_CurrentShot a step at a time, and
 * returns PLAN_UNFINISHED until the plan is made. The first call chooses the
 * usable clubs, the route and the club and target. Each level then picks a
 * landing target for the club's carry and an aim and power for it, and tries
 * shots at it with StepTrialShot, changing the club or the carry when the
 * trials ask for it. Once a trial is taken or the levels run out, the best
 * trial may get a random mis-hit, and it is copied to plannedShot.
 */
s32 PlanShot(CurrentShot *plannedShot) {
    char text[0x80];
    s32 percentRange[2];
    s32 clubType;
    s32 targetX;
    s32 targetZ;
    s32 shotMode;
    s32 index;
    s32 strokesOver;
    s32 distance;
    s32 reach;
    s32 nextSlot;
    s32 shortestReach;
    s32 targetDistance;
    s32 missDistance;
    f32 missAngle;
    f32 aimAngle;
    f32 missX;
    f32 missZ;
    f32 along;
    s32 savedUnk4;
    s32 savedUnk8;
    s32 par;
    s32 strokes;
    s32 mishit;
    s32 maxMishit;
    s32 mishitDivisor;
    s32 *shotPower;
    u32 kind;

    if (sPlanStage == PLAN_STAGE_UNSTARTED) {
        osSyncPrintf("\n------------------AI Start---------------------\n");
        sShot = g_CurrentShot;
        sTrialShot = sShot;
        sBestEvaluation = LOST_BALL_EVALUATION;
        sNeedsNewTarget = TRUE;
        sHasBestEvaluation = FALSE;
        sIsTrialLost = FALSE;
        sLostTrialCount = 0;
        sUnk658 = 0;
        sIsTrialAccepted = FALSE;
        sAcceptableEvaluation = 0;
        sAimChangeCount = 0;
        sShotOffset = SHOT_OFFSET_NONE;
        sTrialIndex = 0;
        sTrialLevel = 0;
        sUnk840 = 0;
        sAimChangeCount = 0;
        sHasTrialShot = FALSE;
        sTrialLaunch = &sTrialLaunchValues;
        sCarryScale = INITIAL_CARRY_SCALE;
        for (index = 0; index < DIRECTION_SECTOR_COUNT; index++) {
            sIsDirectionExcluded[index] = FALSE;
        }
        sClubCount = MarkUsableClubs((s32)GetSurfaceInfo(g_CurrentShot.surface),
                                     g_Players[g_CurrentShot.playerIndex].clubs, sIsClubUsable);
        osSyncPrintf("canuseflags = ");
        for (index = 0; index < sClubCount; index++) {
            osSyncPrintf("%d", sIsClubUsable[index]);
        }
        osSyncPrintf("\n");
        sRouteOrderCount = PlanRoute(sRouteOrder);
        osSyncPrintf("TargetNum = %d\n", sRouteOrderCount);
        for (index = 0; index < sRouteOrderCount; index++) {
            osSyncPrintf("%d ", sRouteOrder[index]);
        }
        osSyncPrintf("\n");
        PlanClubAndTarget(sPlanTarget);
        kind = GetSurfaceInfo(sShot.surface)->kind;
        if (kind == SURFACE_KIND_GREEN || kind == SURFACE_KIND_GREEN_EDGE) {
            sTrialLevel = 0;
        }
        osSyncPrintf("wnmTargetScoreOffset = %d\n", sHoleTargetToPar);
        osSyncPrintf("ClubIndex = %d\n", sShot.clubSlot);
        sprintf(text, "Target %d   ", sHoleTargetToPar);
        PrintDebugText(text, 3, 12);
        sPlanStage = PLAN_STAGE_STARTED;
        return PLAN_UNFINISHED;
    }
    while (sTrialLevel < MIN_TRIAL_LEVEL_COUNT ||
           (sBestEvaluation > POOR_EVALUATION && sTrialLevel < POOR_TRIAL_LEVEL_COUNT) ||
           (!sHasBestEvaluation && sTrialLevel < MAX_TRIAL_LEVEL_COUNT)) {
        if (sPlanStage == PLAN_STAGE_TRIAL_TAKEN || sPlanStage == PLAN_STAGE_UNK4 ||
            sPlanStage == PLAN_STAGE_TRIAL_RUNNING) {
            /* A trial is under way */
        } else if (sNeedsNewTarget == TRUE) {
            if (sPlanStage != PLAN_STAGE_TARGET_CHOSEN) {
                osSyncPrintf("Level %d\n", sTrialLevel);
                sNeedsShorterClub = FALSE;
                sNeedsLongerClub = FALSE;
                if (sShot.clubSlot != FindClubSlot(sShot.playerIndex, CLUB_TYPE_PUTTER)) {
                    sTargetCarry = GetShotWorldDistance(GetClub(g_Players[sShot.playerIndex].clubs[sShot.clubSlot]),
                                                        sShot.shotMode);
                    if (g_HoleConditions.isRaining == TRUE) {
                        sTargetCarry = sTargetCarry * RAINY_CARRY_SCALE;
                    }
                    sTargetCarry = sTargetCarry * sCarryScale;
                    ChooseLandingTarget(sShot.ballPosition[AXIS_X], sShot.ballPosition[AXIS_Z], sTargetCarry,
                                        &sPlanTarget[AXIS_X], &sPlanTarget[AXIS_Z]);
                } else {
                    sPlanTarget[AXIS_X] = D_800BA9D8.cupPosition[AXIS_X];
                    sPlanTarget[AXIS_Z] = g_CupPositionZ;
                }
                if (sShot.shotMode != SHOT_MODE_APPROACH &&
                    !(GetClub(g_Players[sShot.playerIndex].clubs[sShot.clubSlot])->surfaceMask &
                      CLUB_SURFACE_GREEN)) {
                    targetDistance = IntLength2D(sPlanTarget[AXIS_X] - sShot.ballPosition[AXIS_X],
                                                 sPlanTarget[AXIS_Z] - sShot.ballPosition[AXIS_Z]);
                    if (g_HoleConditions.isRaining == TRUE) {
                        targetDistance = (s32)(targetDistance * RAINY_DISTANCE_SCALE);
                    }
                    while (TRUE) {
                        shortestReach =
                            GetShotWorldDistance(GetClub(g_Players[sShot.playerIndex].clubs[sShot.clubSlot]),
                                                 sShot.shotMode);
                        GetSurfacePercentRange(GetSurfaceInfo(sShot.surface),
                                               GetClub(g_Players[sShot.playerIndex].clubs[sShot.clubSlot]),
                                               percentRange);
                        /* The carry per unit of sShot.unk4 at the least percent */
                        shortestReach = shortestReach * percentRange[0] / (FULL_PERCENT * SHOT_UNK4_MAX);
                        if (shortestReach < MIN_CLUB_DOWN_REACH ||
                            targetDistance / shortestReach >= CLUB_DOWN_REACH_COUNT) {
                            break;
                        }
                        nextSlot = sShot.clubSlot;
                        if ((GetClub(g_Players[sShot.playerIndex].clubs[nextSlot])->surfaceMask &
                             CLUB_SURFACE_GREEN) ||
                            (GetClub(g_Players[sShot.playerIndex].clubs[++nextSlot])->surfaceMask &
                             CLUB_SURFACE_GREEN)) {
                            break;
                        }
                        sShot.clubSlot = nextSlot;
                        osSyncPrintf("ChangeDefault----------------------ClubDown\n");
                    }
                }
                sPlanStage = PLAN_STAGE_TARGET_CHOSEN;
                if (sPlanTarget[AXIS_X] == D_800BA9D8.cupPosition[AXIS_X] && sPlanTarget[AXIS_Z] == g_CupPositionZ &&
                    GetSurfaceInfo(sShot.surface)->kind != SURFACE_KIND_GREEN) {
                    strokesOver = g_Players[sShot.playerIndex].unk23 - (g_HolePar + sHoleTargetToPar - 2);
                    if (strokesOver <= 0) {
                        missDistance = IntLength2D(g_CupPositionX - sShot.ballPosition[AXIS_X],
                                                   g_CupPositionZ - sShot.ballPosition[AXIS_Z]) >>
                                       3;
                        missAngle = ((rand() * 4) & BINARY_ANGLE_MASK) * BINARY_ANGLE_TO_RADIANS;
                        aimAngle = (u16)sShot.unk0 * BINARY_ANGLE_TO_RADIANS;
                        if (missDistance >= MAX_MISS_DISTANCE) {
                            missDistance = MAX_MISS_DISTANCE;
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
                        along = (missX * cosf(aimAngle) + missZ * sinf(aimAngle)) * MISS_STRETCH;
                        missX += cosf(aimAngle) * along;
                        missZ += sinf(aimAngle) * along;
                        sPlanTarget[AXIS_X] = sPlanTarget[AXIS_X] + missX;
                        sPlanTarget[AXIS_Z] = sPlanTarget[AXIS_Z] + missZ;
                        for (index = 0; index < MISS_REPORT_COUNT; index++) {
                            osSyncPrintf("Iziwaru %d\n", strokesOver);
                        }
                    }
                }
                return PLAN_UNFINISHED;
            }
            sPlanTarget[AXIS_Y] = GetGroundHeight(sPlanTarget[AXIS_X], sPlanTarget[AXIS_Z]);
            sShot.unk0 = atan2(sPlanTarget[AXIS_Z] - sShot.ballPosition[AXIS_Z],
                               sPlanTarget[AXIS_X] - sShot.ballPosition[AXIS_X]) *
                         RADIANS_TO_BINARY_ANGLE;
            if (GetClub(g_Players[sShot.playerIndex].clubs[sShot.clubSlot])->surfaceMask & CLUB_SURFACE_GREEN) {
                sShot.unk0 += GetSideSlope();
            }
            sShot.unk8 = DEFAULT_SHOT_UNK8;
            sShot.unk4 = SHOT_UNK4_MAX;
            reach = GetShotWorldDistance(GetClub(g_Players[sShot.playerIndex].clubs[sShot.clubSlot]), sShot.shotMode);
            distance = IntLength2D(sShot.ballPosition[AXIS_X] - sPlanTarget[AXIS_X],
                                   sShot.ballPosition[AXIS_Z] - sPlanTarget[AXIS_Z]);
            if (g_HoleConditions.isRaining == TRUE) {
                distance = (s32)(distance * RAINY_DISTANCE_SCALE);
            }
            GetSurfacePercentRange(GetSurfaceInfo(sShot.surface),
                                   GetClub(g_Players[sShot.playerIndex].clubs[sShot.clubSlot]), percentRange);
            /* The carry per unit of sShot.unk4 at the mean percent */
            reach = reach * (percentRange[1] + percentRange[0]) / (2 * FULL_PERCENT * SHOT_UNK4_MAX);
            /* Holding sShot.unk4 in distance is there to match */
            distance = (rand() & 3) + distance / reach;
            if (GetGolferTraitUnk2() >= 2) {
                distance = distance * SHOT_UNK4_BOOST;
            }
            if (distance < MIN_PLANNED_SHOT_UNK4) {
                distance = MIN_PLANNED_SHOT_UNK4;
            } else if (distance > SHOT_UNK4_MAX) {
                distance = SHOT_UNK4_MAX;
            }
            /* Storing through shotPower is there to match */
            shotPower = &sShot.unk4;
            *shotPower = distance;
            if (sShotOffset == SHOT_OFFSET_NONE) {
                sShot.unk14 = 0;
                sShot.unk18 = 0;
                sShotOffset = ChooseShotOffset();
            }
            sTrialIndex = 0;
            sLevelTrialCount = LEVEL_TRIAL_COUNT;
            if (!sHasTrialShot) {
                sTrialShot = sShot;
                sHasTrialShot = TRUE;
            }
            if (sPlanTarget[AXIS_X] == D_800BA9D8.cupPosition[AXIS_X] && sPlanTarget[AXIS_Z] == g_CupPositionZ) {
                sLevelTrialCount = CUP_LEVEL_TRIAL_COUNT;
            }
            if (IsGameFlagSet(GAME_FLAG_UNK3C)) {
                sLevelTrialCount = UNK3C_LEVEL_TRIAL_COUNT;
            }
            sNeedsNewTarget = FALSE;
        } else {
            sTrialIndex = 0;
        }
        if (sTrialIndex < sLevelTrialCount) {
            if (!StepTrialShot()) {
                return PLAN_UNFINISHED;
            }
            if (!sIsTrialAccepted && !sNeedsShorterClub && !sNeedsLongerClub) {
                sPlanStage = PLAN_STAGE_TRIAL_TAKEN;
                sTrialIndex++;
                return PLAN_UNFINISHED;
            }
        }
        sPlanStage = PLAN_STAGE_LEVEL_FINISHED;
        if (sIsTrialLost == TRUE) {
            sLostTrialCount++;
        }
        osSyncPrintf("Besteval = %d\n", sBestEvaluation);
        if (sIsTrialAccepted) {
            break;
        }
        if (sNeedsShorterClub) {
            if (sShot.clubSlot == FindClubSlot(sShot.playerIndex, CLUB_TYPE_PUTTER)) {
                break;
            }
            if (sShot.shotMode == SHOT_MODE_APPROACH) {
                if (sGolferTraits[(s8)g_Players[sShot.playerIndex].characterIndex].windPlay < 2 &&
                    sShot.clubSlot == FindClubSlot(sShot.playerIndex, CLUB_TYPE_PITCHING_WEDGE)) {
                    sShot.clubSlot = FindClubSlot(sShot.playerIndex, CLUB_TYPE_SAND_WEDGE);
                } else {
                    osSyncPrintf("Approach cancel\n");
                    ChooseLongShot(sHoleStrokeTarget, &clubType, &targetX, &targetZ, &shotMode);
                    sShot.shotMode = shotMode;
                    sShot.clubSlot = FindClubSlot(sShot.playerIndex, clubType);
                }
                sNeedsNewTarget = TRUE;
                sTrialLevel++;
                continue;
            }
            sShot.clubSlot++;
            if (sShot.clubSlot >= FindClubSlot(sShot.playerIndex, CLUB_TYPE_PITCHING_WEDGE)) {
                sShot.clubSlot = FindClubSlot(sShot.playerIndex, CLUB_TYPE_PITCHING_WEDGE);
            }
            sNeedsNewTarget = TRUE;
        } else if (sNeedsLongerClub == TRUE) {
            osSyncPrintf("num_hit = %d\n", g_Players[sShot.playerIndex].unk23);
            if (sShot.clubSlot == FindClubSlot(sShot.playerIndex, CLUB_TYPE_PUTTER)) {
                break;
            }
            if (sShot.clubSlot > 0) {
                sShot.clubSlot--;
                if (!sIsClubUsable[sShot.clubSlot] ||
                    (GetGolferTraitUnk2() < 2 &&
                     GetLieRating(GetClub(g_Players[sShot.playerIndex].clubs[sShot.clubSlot]), sShot.surface,
                                  sShot.shotMode) < 2)) {
                    sShot.clubSlot++;
                    sCarryScale *= CARRY_SCALE_STEP;
                }
            } else {
                sCarryScale *= CARRY_SCALE_STEP;
            }
            sNeedsNewTarget = TRUE;
        }
        sTrialLevel++;
    }
    osSyncPrintf("Finish Eval %d tlevel%d bestevalappear %d\n", sBestEvaluation, sTrialLevel, sHasBestEvaluation);
    savedUnk8 = sTrialShot.unk8;
    savedUnk4 = sTrialShot.unk4;
    if ((s32)IntLength2D(D_800BA9D8.cupPosition[AXIS_X] - sTrialShot.ballPosition[AXIS_X],
                         g_CupPositionZ - sTrialShot.ballPosition[AXIS_Z]) <
        (s32)IntLength2D(D_800BA9D8.cupPosition[AXIS_X] - sPlanTarget[AXIS_X], g_CupPositionZ - sPlanTarget[AXIS_Z])) {
        strokes = g_Players[sShot.playerIndex].unk23;
        par = g_HolePar;
        mishit = 0;
        if (GetSurfaceInfo(sTrialShot.surface)->kind != SURFACE_KIND_GREEN && strokes < par + sHoleTargetToPar - 1) {
            maxMishit = GetLieRating(GetClub(g_Players[sTrialShot.playerIndex].clubs[sTrialShot.clubSlot]),
                                     sTrialShot.surface, sTrialShot.shotMode);
            maxMishit -= maxMishit != 0;
            if (strokes < par + sHoleTargetToPar - 2) {
                switch (sGolferTraits[(s8)g_Players[sTrialShot.playerIndex].characterIndex].unk1) {
                    case 0:
                        mishitDivisor = 62;
                        break;
                    case 1:
                        mishitDivisor = 83;
                        break;
                    case 2:
                    default:
                        mishitDivisor = 125;
                        break;
                }
            } else {
                mishitDivisor = 208;
            }
            mishit = rand() % MISHIT_ROLL_RANGE / mishitDivisor;
            if (maxMishit < ABS(mishit) && (sHoleTargetToPar < 0 || strokes >= par + sHoleTargetToPar - 3)) {
                mishit = maxMishit;
            }
            if (mishit != 0 && (rand() & MISHIT_SIGN_BIT)) {
                mishit = -mishit;
            }
            sTrialShot.unk8 += mishit;
            /* Never printed; the original has it, and its string is in the ROM */
            if (FALSE) {
                osSyncPrintf("Dahuri = %d\n", mishit);
            }
        }
        if ((sHoleTargetToPar <= 0) & (mishit != 0) && !IsTrialShotSafe()) {
            sTrialShot.unk8 = savedUnk8;
            sTrialShot.unk4 = savedUnk4;
        }
    }
    osSyncPrintf("clubidx = %d\n", sTrialShot.clubSlot);
    osSyncPrintf("pgaugeresult = %d\n", sTrialShot.unk4);
    osSyncPrintf("pgaugeinpact = %d\n", sTrialShot.unk8);
    *plannedShot = sTrialShot;
    osSyncPrintf("AI End\n");
    return 1;
}

s32 GetBallCupDistance(void) {
    return IntLength2D(sShot.ballPosition[AXIS_X] - D_800BA9D8.cupPosition[AXIS_X],
                       sShot.ballPosition[AXIS_Z] - g_CupPositionZ);
}

/* The cup distance from which PlanCupDistance allows for the wind */
#define WIND_ALLOWANCE_MIN_DISTANCE 153600.0f
/* windSpeed's units in a unit of the wind's push */
#define WIND_PUSH_UNIT 0x10000
#define WIND_SPEED_SCALE (1.0f / WIND_PUSH_UNIT)
/* The fraction of the distance that each unit of push along the line takes off */
#define WIND_FOLLOW_FRACTION 0.01f
#define WIND_DISTANCE_RATIO_MIN 0.8f
#define WIND_DISTANCE_RATIO_MAX 1.2f
/* The distance that each unit of the ball's height above the cup adds */
#define HEIGHT_DISTANCE_FRACTION 0.1f

/*
 * Returns the distance to plan a shot to the cup for: the flat distance,
 * shortened by a following wind and lengthened by a head wind from
 * WIND_ALLOWANCE_MIN_DISTANCE out, plus a part of the ball's height above the
 * cup.
 */
s32 PlanCupDistance(s32 unused) {
    s32 distance;
    f32 cupX;
    f32 cupZ;
    f32 length;
    f32 windAngle;
    f32 inverseLength;
    f32 windFollow;
    f32 windX;
    f32 windZ;

    distance = IntLength2D(sShot.ballPosition[AXIS_X] - D_800BA9D8.cupPosition[AXIS_X],
                           sShot.ballPosition[AXIS_Z] - g_CupPositionZ);
    cupX = D_800BA9D8.cupPosition[AXIS_X] - sShot.ballPosition[AXIS_X];
    cupZ = g_CupPositionZ - sShot.ballPosition[AXIS_Z];
    length = sqrtf(cupX * cupX + cupZ * cupZ);
    windAngle = g_HoleConditions.windDirection * WIND_DIRECTION_TO_RADIANS;
    if (length >= WIND_ALLOWANCE_MIN_DISTANCE) {
        inverseLength = 1.0f / length;
        cupX *= inverseLength;
        cupZ *= inverseLength;
        windX = cosf(windAngle) * g_HoleConditions.windSpeed * WIND_SPEED_SCALE;
        windZ = sinf(windAngle) * g_HoleConditions.windSpeed * WIND_SPEED_SCALE;
        windFollow = cupX * windX + cupZ * windZ;
        osSyncPrintf("WindFollow%f\n", windFollow);
        /* Turning windFollow into the distance ratio is there to match */
        windFollow = -windFollow * WIND_FOLLOW_FRACTION + 1.0f;
        osSyncPrintf("WindCareRatio%f\n", windFollow);
        if (windFollow < WIND_DISTANCE_RATIO_MIN) {
            windFollow = WIND_DISTANCE_RATIO_MIN;
        }
        if (windFollow >= WIND_DISTANCE_RATIO_MAX) {
            windFollow = WIND_DISTANCE_RATIO_MAX;
        }
        distance = distance * windFollow;
    }
    return distance + (sShot.ballPosition[AXIS_Y] - g_CupPositionY) * HEIGHT_DISTANCE_FRACTION;
}

/* Shots from farther than this are long shots */
#define APPROACH_RANGE (60 * UNITS_PER_YARD)
/* How much farther than the cup distance a putt is played in the rain */
#define RAINY_PUTT_DISTANCE_SCALE 1.2f

s32 ChooseHoleStrokeTarget(void);

/*
 * Chooses sShot's club and shot mode for the lie and sets target to where the
 * shot aims. From the bunker lies other than SURFACE_KIND_BUNKER it plays the
 * sand wedge; on the green it takes the last usable club that can play from the
 * green, in the weakest shot mode that reaches the cup; elsewhere it plays a long
 * shot or an approach, by the cup distance.
 */
void PlanClubAndTarget(CoursePoint target) {
    s32 targetX;
    s32 targetZ;
    s32 clubType;
    s32 shotMode;
    u32 kind;
    s32 rest;
    s32 slot;
    Club *club;

    clubType = CLUB_TYPE_DRIVER;
    kind = GetSurfaceInfo(sShot.surface)->kind;
    sHoleStrokeTarget = ChooseHoleStrokeTarget();
    sHoleTargetToPar = sHoleStrokeTarget - g_HolePar;
    switch (kind) {
        case SURFACE_KIND_GREEN:
        case SURFACE_KIND_GREEN_EDGE:
            rest = GetBallCupDistance();
            osSyncPrintf("rest = %d\n", rest);
            if (g_HoleConditions.isRaining == TRUE) {
                rest = rest * RAINY_PUTT_DISTANCE_SCALE;
            }
            for (slot = 0; slot < sClubCount; slot++) {
                if (!sIsClubUsable[slot]) {
                    continue;
                }
                club = GetClub(g_Players[sShot.playerIndex].clubs[slot]);
                if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
                    continue;
                }
                if (GetShotWorldDistance(club, SHOT_MODE_APPROACH) >= rest) {
                    sShot.shotMode = SHOT_MODE_APPROACH;
                } else if (GetShotWorldDistance(club, SHOT_MODE_NORMAL) >= rest) {
                    sShot.shotMode = SHOT_MODE_NORMAL;
                } else {
                    sShot.shotMode = SHOT_MODE_POWER;
                }
                osSyncPrintf("skSCAI.powermode = %d\n", sShot.shotMode);
                sShot.clubSlot = slot;
            }
            break;
        case SURFACE_KIND_BUNKER_FAIR_LIE:
        case SURFACE_KIND_BUNKER_POOR_LIE:
        case SURFACE_KIND_BUNKER_UNK9:
            sShot.shotMode = SHOT_MODE_NORMAL;
            sShot.clubSlot = FindClubSlot(sShot.playerIndex, CLUB_TYPE_SAND_WEDGE);
            ChooseLandingTarget(
                sShot.ballPosition[AXIS_X], sShot.ballPosition[AXIS_Z],
                GetShotWorldDistance(GetClub(g_Players[sShot.playerIndex].clubs[sShot.clubSlot]), sShot.shotMode),
                &targetX, &targetZ);
            target[AXIS_X] = targetX;
            target[AXIS_Z] = targetZ;
            target[AXIS_Y] = GetGroundHeight(targetX, targetZ);
            break;
        default:
            if (GetBallCupDistance() > APPROACH_RANGE) {
                ChooseLongShot(sHoleStrokeTarget, &clubType, &targetX, &targetZ, &shotMode);
                target[AXIS_X] = targetX;
                target[AXIS_Z] = targetZ;
                target[AXIS_Y] = GetGroundHeight(targetX, targetZ);
                sShot.shotMode = shotMode;
                sShot.clubSlot = FindClubSlot(sShot.playerIndex, clubType);
            } else {
                clubType = ChooseApproachClub(&targetX, &targetZ);
                target[AXIS_X] = targetX;
                target[AXIS_Z] = targetZ;
                target[AXIS_Y] = GetGroundHeight(targetX, targetZ);
                sShot.shotMode = SHOT_MODE_APPROACH;
                sShot.clubSlot = FindClubSlot(sShot.playerIndex, clubType);
            }
            break;
    }
}

/* SortByKeys's orders */
#define SORT_ASCENDING 0
#define SORT_DESCENDING 1

/*
 * Selection sorts count keys in the given order, swapping the entries of the
 * values and otherValues arrays along with them unless they are NULL.
 */
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
            } else if (keys[otherIndex] < keys[chosenIndex]) {
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

/*
 * Returns the player's strokes over par on the holes before this one,
 * counting only holes with strokes. A hole with more than twice its par plus
 * one strokes counts as one under par.
 */
s32 GetScoreToParSoFar(s32 playerIndex) {
    s32 strokeTotal = 0;
    s32 parTotal = 0;
    s32 hole;
    s32 par;
    s32 strokes;

    if (g_GameState.holeIndex == 0) {
        return 0;
    }
    for (hole = 0; hole < g_GameState.holeIndex; hole++) {
        strokes = g_Players[playerIndex].holes[hole].strokes;
        par = (s8)g_CourseRecords[g_GameState.courseIndex].holes[hole].par;
        if (par * 2 + 1 < strokes) {
            strokes = par - 1;
        }
        if (strokes > 0) {
            strokeTotal += strokes;
            parTotal += par;
        }
    }
    return strokeTotal - parTotal;
}

/* The par range of the course's holes */
#define LOWEST_PAR 3
#define HIGHEST_PAR 5
/* A hole's planned strokes are lowered only from this many or more */
#define LOWEST_LOWERED_STROKES 3

/*
 * Spreads strokeCount strokes, or takes -strokeCount away, one a hole at a
 * time in the order of the entries, over the holes that isSpreadHole picks,
 * until they run out or no hole can take any more. It is a macro, as
 * isSpreadHole is an expression on the caller's index.
 */
#define SpreadStrokes(strokes, strokeCount, isSpreadHole)                          \
    {                                                                              \
        s32 changeCount_;                                                          \
                                                                                   \
        while ((strokeCount) != 0) {                                               \
            changeCount_ = 0;                                                      \
            for (index = 0; index < COURSE_HOLE_COUNT; index++) {                  \
                if (!(isSpreadHole)) {                                             \
                    continue;                                                      \
                }                                                                  \
                if ((strokeCount) < 0) {                                           \
                    if ((strokes)[index] >= LOWEST_LOWERED_STROKES) {              \
                        changeCount_++;                                            \
                        (strokes)[index]--;                                        \
                        (strokeCount)++;                                           \
                        if ((strokeCount) >= 0) {                                  \
                            break;                                                 \
                        }                                                          \
                    }                                                              \
                } else {                                                           \
                    changeCount_++;                                                \
                    (strokes)[index]++;                                            \
                    (strokeCount)--;                                               \
                    if ((strokeCount) <= 0) {                                      \
                        break;                                                     \
                    }                                                              \
                }                                                                  \
            }                                                                      \
            if (changeCount_ == 0) {                                               \
                break;                                                             \
            }                                                                      \
        }                                                                          \
    }

/*
 * Chooses how many strokes the golfer aims to take on this hole. It takes the
 * character's target score to par for the course, plus a lucky bonus of 0 to 3
 * strokes in Get Character when its record against the player's character has
 * more losses than wins. It spreads that score over the holes by par and
 * handicap, then spreads what the score so far leaves over the holes still to
 * play.
 */
s32 ChooseHoleStrokeTarget(void) {
    s32 strokes[COURSE_HOLE_COUNT];
    s32 handicaps[COURSE_HOLE_COUNT];
    s32 holes[COURSE_HOLE_COUNT];
    s32 groupHandicaps[COURSE_HOLE_COUNT];
    s32 groupStrokes[COURSE_HOLE_COUNT];
    s32 groupHoles[COURSE_HOLE_COUNT];
    s32 luckyBonus;
    s32 lossCount;
    s32 winBits;
    s32 targetScore;
    s32 spreadStyle;
    s32 index;
    s32 par;
    s32 groupCount;
    s32 frontStrokes;
    s32 backStrokes;
    s32 negativeCount;
    s32 targetLeftToPar;
    s32 plannedLeftToPar;
    s32 entry;
    s32 strokesLeft;

    luckyBonus = 0;
    if (IsGameFlagSet(GAME_FLAG_UNK3C)) {
        return 0;
    }
    if (g_GameMode == GAME_MODE_UNK3) {
        return g_HolePar;
    }
    if (g_GameMode == GAME_MODE_GET_CHARACTER) {
        lossCount = GetSaveData()->unk990.unkAF0[(s8)g_Players[0].characterIndex]
                                                [(s8)g_Players[sShot.playerIndex].characterIndex].unk0;
        winBits = GetSaveData()->unk990.unkAF0[(s8)g_Players[0].characterIndex]
                                              [(s8)g_Players[sShot.playerIndex].characterIndex].unk1;
        if ((winBits & WIN_COUNT_MASK) < (lossCount & WIN_COUNT_MASK)) {
            luckyBonus = D_801B60B4 & 3;
        }
        osSyncPrintf("win%d lose%d lucky%d\n", winBits, lossCount, luckyBonus);
    }
    targetScore = luckyBonus + sGolferTraits[(s8)g_Players[sShot.playerIndex].characterIndex].unkD +
                  sGolferTraits[(s8)g_Players[sShot.playerIndex].characterIndex].targetScores[g_CourseIndex];
    osSyncPrintf("Char Index = %d\n", (s8)g_Players[sShot.playerIndex].characterIndex);
    osSyncPrintf("TargetScore = %d\n", targetScore);
    spreadStyle = sGolferTraits[(s8)g_Players[sShot.playerIndex].characterIndex].unk0;
    for (index = 0; index < COURSE_HOLE_COUNT; index++) {
        strokes[index] = 0;
        handicaps[index] = 0;
    }
    for (index = 0; index < COURSE_HOLE_COUNT; index++) {
        handicaps[index] = g_CourseRecords[g_GameState.courseIndex].holes[index].handicap;
        strokes[index] = (s8)g_CourseRecords[g_GameState.courseIndex].holes[index].par;
        holes[index] = index;
    }
    if (targetScore < 0) {
        SortByKeys(COURSE_HOLE_COUNT, holes, handicaps, strokes, SORT_DESCENDING);
    } else {
        SortByKeys(COURSE_HOLE_COUNT, holes, handicaps, strokes, SORT_ASCENDING);
    }
    for (index = HIGHEST_PAR; index >= LOWEST_PAR; index--) {
        groupCount = 0;
        for (entry = 0; entry < COURSE_HOLE_COUNT; entry++) {
            if (strokes[entry] == index) {
                groupHoles[groupCount] = holes[entry];
                groupStrokes[groupCount] = strokes[entry];
                groupHandicaps[groupCount] = handicaps[entry];
                groupCount++;
            }
        }
        if (targetScore < 0) {
            SortByKeys(groupCount, groupHoles, groupStrokes, groupHandicaps, SORT_DESCENDING);
        } else {
            SortByKeys(groupCount, groupHoles, groupStrokes, groupHandicaps, SORT_ASCENDING);
        }
        groupCount = 0;
        for (entry = 0; entry < COURSE_HOLE_COUNT; entry++) {
            if (strokes[entry] == index) {
                holes[entry] = groupHoles[groupCount];
                strokes[entry] = groupStrokes[groupCount];
                /* Reads handicaps rather than groupHandicaps; the original has it */
                handicaps[entry] = handicaps[groupCount];
                groupCount++;
            }
        }
    }
    backStrokes = targetScore / 2;
    frontStrokes = backStrokes;
    if (spreadStyle == 0) {
        if (targetScore & 1) {
            frontStrokes = targetScore - backStrokes;
        }
    } else if (spreadStyle == 1) {
        if (targetScore & 1) {
            frontStrokes = targetScore - backStrokes;
        }
    } else if (spreadStyle == 2) {
        if (targetScore & 1) {
            backStrokes = targetScore - backStrokes;
        }
    }
    strokesLeft = frontStrokes;
    SpreadStrokes(strokes, strokesLeft, holes[index] < HOLES_PER_NINE);
    strokesLeft = backStrokes;
    SpreadStrokes(strokes, strokesLeft, holes[index] >= HOLES_PER_NINE);
    if (spreadStyle == 1 || spreadStyle == 2) {
        negativeCount = 0;
        backStrokes = 0;
        frontStrokes = 0;
        for (index = 0; index < COURSE_HOLE_COUNT; index++) {
            if (spreadStyle == 1 && !(holes[index] < HOLES_PER_NINE)) {
                continue;
            }
            if (spreadStyle == 2 && holes[index] < HOLES_PER_NINE) {
                continue;
            }
            if (strokes[index] < 0) {
                negativeCount++;
            }
        }
        if (targetScore > 0) {
            if (negativeCount == 0) {
                if (spreadStyle == 1) {
                    frontStrokes--;
                    backStrokes++;
                } else if (spreadStyle == 2) {
                    backStrokes--;
                    frontStrokes++;
                }
            }
        } else if (targetScore == 0 || targetScore == -1) {
            if (negativeCount < 2) {
                if (spreadStyle == 1) {
                    frontStrokes += negativeCount - 2;
                    backStrokes -= negativeCount - 2;
                } else if (spreadStyle == 2) {
                    backStrokes += negativeCount - 2;
                    frontStrokes -= negativeCount - 2;
                }
            }
        } else if (negativeCount < 3) {
            if (spreadStyle == 1) {
                frontStrokes += negativeCount - 3;
                backStrokes -= negativeCount - 3;
            } else if (spreadStyle == 2) {
                backStrokes += negativeCount - 3;
                frontStrokes -= negativeCount - 3;
            }
        }
        for (index = 0; index < COURSE_HOLE_COUNT; index++) {
            if (holes[index] < HOLES_PER_NINE) {
                if (frontStrokes < 0) {
                    if (strokes[index] == (s8)g_CourseRecords[g_GameState.courseIndex].holes[holes[index]].par &&
                        strokes[index] != LOWEST_LOWERED_STROKES) {
                        strokes[index]--;
                        frontStrokes++;
                    }
                } else if (frontStrokes > 0) {
                    if (strokes[index] == (s8)g_CourseRecords[g_GameState.courseIndex].holes[holes[index]].par) {
                        strokes[index]++;
                        frontStrokes--;
                    }
                }
            } else if (backStrokes < 0) {
                if (strokes[index] == (s8)g_CourseRecords[g_GameState.courseIndex].holes[holes[index]].par &&
                    strokes[index] != LOWEST_LOWERED_STROKES) {
                    strokes[index]--;
                    backStrokes++;
                }
            } else if (backStrokes > 0) {
                if (strokes[index] == (s8)g_CourseRecords[g_GameState.courseIndex].holes[holes[index]].par) {
                    strokes[index]++;
                    backStrokes--;
                }
            }
        }
    }
    plannedLeftToPar = 0;
    targetLeftToPar = targetScore - GetScoreToParSoFar(sShot.playerIndex);
    for (index = 0; index < COURSE_HOLE_COUNT; index++) {
        if (holes[index] >= g_GameState.holeIndex) {
            plannedLeftToPar += strokes[index] - (s8)g_CourseRecords[g_GameState.courseIndex].holes[holes[index]].par;
        }
    }
    if (g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE) {
        targetLeftToPar = targetScore - targetLeftToPar;
        if (g_GameState.holeIndex >= HOLES_PER_NINE) {
            targetLeftToPar = 0;
            for (index = 0; index < COURSE_HOLE_COUNT; index++) {
                if (holes[index] < HOLES_PER_NINE) {
                    targetLeftToPar +=
                        strokes[index] - (s8)g_CourseRecords[g_GameState.courseIndex].holes[holes[index]].par;
                }
            }
            for (index = 0; index < COURSE_HOLE_COUNT; index++) {
                if (holes[index] >= HOLES_PER_NINE && holes[index] < g_GameState.holeIndex) {
                    targetLeftToPar += g_Players[sShot.playerIndex].holes[holes[index]].strokes -
                              (s8)g_CourseRecords[g_GameState.courseIndex].holes[holes[index]].par;
                }
            }
            targetLeftToPar = targetScore - targetLeftToPar;
        }
    }
    if (g_GameState.betRates[BET_UNK4] > 0 || g_GameMode == GAME_MODE_SKINS_MATCH ||
        g_GameMode == GAME_MODE_CLUB_SLOTS) {
        targetLeftToPar = 0;
        for (index = 0; index < COURSE_HOLE_COUNT; index++) {
            if (holes[index] < g_GameState.holeIndex) {
                targetLeftToPar +=
                    strokes[index] - (s8)g_CourseRecords[g_GameState.courseIndex].holes[holes[index]].par;
            }
        }
        targetLeftToPar = targetScore - targetLeftToPar;
    }
    SortByKeys(COURSE_HOLE_COUNT, strokes, handicaps, holes, SORT_ASCENDING);
    osSyncPrintf("OrgOffset = %d\n", targetLeftToPar);
    osSyncPrintf("Work = %d\n", plannedLeftToPar);
    strokesLeft = targetLeftToPar - plannedLeftToPar;
    SpreadStrokes(strokes, strokesLeft, g_GameState.holeIndex <= holes[index]);
    osSyncPrintf("TS => ");
    for (index = 0; index < COURSE_HOLE_COUNT; index++) {
        for (entry = 0; entry < COURSE_HOLE_COUNT; entry++) {
            if (holes[entry] == index) {
                osSyncPrintf(" %d", strokes[entry]);
            }
        }
    }
    osSyncPrintf("\n");
    osSyncPrintf("OS => ");
    for (index = 0; index < COURSE_HOLE_COUNT; index++) {
        osSyncPrintf(" %d", (s8)g_CourseRecords[g_CourseIndex].holes[index].par);
    }
    osSyncPrintf("\n");
    for (index = 0; index < COURSE_HOLE_COUNT; index++) {
        if (holes[index] == g_HoleIndex) {
            /* The comma is in the original's string */
            osSyncPrintf("++ %d\n,", strokes[index]);
            return strokes[index];
        }
    }
    /* Reads past the holes; the original has it */
    return (s8)g_CourseRecords[g_CourseIndex].holes[index].par;
}

#define NO_ROUTE_POINT (-1)
/* Beyond any distance on a hole */
#define FARTHEST_DISTANCE 0x0FFFFFFF

/*
 * Stores in route the indices into sRoute of the hole's route
 * points to play through, from the tee toward the cup, and returns their count.
 * Starting at the cup, it walks to the nearest point not yet visited until it
 * reaches the tee.
 */
s32 PlanRoute(s8 *route) {
    s8 isVisited[ROUTE_POINT_COUNT + 1];
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
    currentX = D_800BA9D8.cupPosition[AXIS_X];
    currentZ = g_CupPositionZ;
    pointCount = 0;
    for (index = 0; index < ROUTE_POINT_COUNT; index++) {
        if (g_HolePoints.routePoints[index][AXIS_X] == 0 && g_HolePoints.routePoints[index][AXIS_Z] == 0) {
            break;
        }
        sRoute.x[pointCount] = g_HolePoints.routePoints[index][AXIS_X];
        sRoute.z[pointCount] = g_HolePoints.routePoints[index][AXIS_Z];
        pointCount++;
    }
    sRoute.x[pointCount] = D_800BA9D8.teePosition[AXIS_X];
    sRoute.z[pointCount] = D_800BA9D8.teePosition[AXIS_Z];
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
            distance = IntLength2D(currentX - sRoute.x[index], currentZ - sRoute.z[index]);
            /* Unused; it is there to match */
            IntLength2D(sRoute.x[index] - D_800BA9D8.teePosition[AXIS_X],
                        sRoute.z[index] - D_800BA9D8.teePosition[AXIS_Z]);
            if (distance < nearestDistance) {
                nearest = index;
                nearestDistance = distance;
            }
        }
        if (nearest != NO_ROUTE_POINT) {
            remainingCount--;
            order[orderCount] = nearest;
            isVisited[nearest] = TRUE;
            currentX = sRoute.x[nearest];
            currentZ = sRoute.z[nearest];
            orderCount++;
            if (currentX == D_800BA9D8.teePosition[AXIS_X] &&
                currentZ == D_800BA9D8.teePosition[AXIS_Z]) {
                break;
            }
        }
    }
    /* Counting the route in index is there to match */
    index = 0;
    for (orderIndex = orderCount - 1; orderIndex >= 0; orderIndex--) {
        if (order[orderIndex] != pointCount - 1) {
            route[index++] = order[orderIndex];
        }
    }
    return index;
}

/* How good a place to land each surface kind is, from LANDING_NONE to LANDING_BEST */
#define LANDING_NONE 0
#define LANDING_POOR 5
#define LANDING_FAIR 7
#define LANDING_BEST 8

s32 GetLandingRating(u32 surfaceKind) {
    s32 rating;

    if (g_CourseIndex == COURSE_MARIOS_STAR) {
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
        case 5:
        case SURFACE_KIND_CART_WAY:
        case SURFACE_KIND_ROCK:
        case SURFACE_KIND_BARE_GROUND:
        case SURFACE_KIND_DEEP_ROUGH:
        case SURFACE_KIND_BARE_GROUND_UNK20:
        case SURFACE_KIND_WASTE_AREA:
            rating = LANDING_FAIR;
            break;
        case SURFACE_KIND_BUNKER:
        case SURFACE_KIND_BUNKER_FAIR_LIE:
        case SURFACE_KIND_BUNKER_POOR_LIE:
            rating = LANDING_POOR;
            break;
        case SURFACE_KIND_BUNKER_UNK9:
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
/* The landing search walks back from the cup this many yards a step */
#define TARGET_STEP_YARDS 2
/* The search takes up to reach * TARGET_STEPS_PER_UNIT + EXTRA_TARGET_STEP_COUNT steps */
#define TARGET_STEPS_PER_UNIT (4 * YARDS_PER_UNIT)
#define EXTRA_TARGET_STEP_COUNT 50.0f
#define TARGET_SIDE_COUNT 100
#define TARGET_SIDE_SPACING (4 * UNITS_PER_YARD)
/* The neighbours a target's lie is tested at, in TARGET_NEIGHBOUR_SPACINGs */
#define TARGET_NEIGHBOUR_REACH 2
#define TARGET_NEIGHBOUR_SPACING 4
/* How far below the paths the ground test starts */
#define PATH_GROUND_CLEARANCE 0x600
/* Rolls shorter than this are not tested for ground in the way */
#define MIN_TESTED_ROLL (2 * UNITS_PER_YARD)

/*
 * Chooses where a shot of distance from (x, z) lands. Walking from the cup back
 * toward the ball a step at a time, and out to either side at each step, it takes
 * the first point within reach that is outside the excluded directions, lands as
 * well as the cup, has no ground in the way of the flight or the roll to the cup,
 * and has no much worse lie around it. It aims at the ball if none qualifies, and
 * at the cup if no route point is nearer the cup than the ball. Points that lie
 * farther below the ball than they are across from it are skipped.
 */
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
    s32 endRating;
    s32 sideCount;
    s32 isFound;
    s32 startX;
    s32 startZ;
    f32 angle;
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
    s32 isPoorlySurrounded;
    s32 column;
    s32 row;

    osSyncPrintf("Distance = %dYd\n", (s32)(distance * YARDS_PER_UNIT));
    pointCount = sRouteOrderCount;
    isFound = FALSE;
    if (pointCount == 0) {
        sideCount = TARGET_SIDE_COUNT;
        endX = D_800BA9D8.cupPosition[AXIS_X];
        endZ = g_CupPositionZ;
        startX = x;
        startZ = z;
    } else {
        for (index = 0; index < sRouteOrderCount; index++) {
            order[index] = sRouteOrder[index];
        }
        for (index = 0; index < pointCount; index++) {
            pointX[index] = sRoute.x[order[index]];
            pointZ[index] = sRoute.z[order[index]];
            osSyncPrintf("PointList[%d] = (%d, %d)\n", index, pointX[index], pointZ[index]);
        }
        pointX[pointCount] = D_800BA9D8.cupPosition[AXIS_X];
        /* Reading the cup's z through D_800BA9D8 here and below is there to match */
        pointZ[pointCount] = D_800BA9D8.cupPosition[AXIS_Z];
        order[pointCount] = END_ROUTE_POINT;
        pointCount++;
        nearCount = 0;
        ballCupDistance = IntLength2D(D_800BA9D8.cupPosition[AXIS_X] - x, g_CupPositionZ - z);
        for (index = 0; index < pointCount; index++) {
            if ((s32)IntLength2D(D_800BA9D8.cupPosition[AXIS_X] - pointX[index],
                                 D_800BA9D8.cupPosition[AXIS_Z] - pointZ[index]) < ballCupDistance) {
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
            *targetX = D_800BA9D8.cupPosition[AXIS_X];
            *targetZ = g_CupPositionZ;
            return;
        }
        for (index = 0; index < pointCount; index++) {
            if (distance < (s32)IntLength2D(pointX[index] - x, pointZ[index] - z)) {
                answerIndex = index - 1;
                break;
            }
        }
        osSyncPrintf("AnsIndex = %d\n", answerIndex);
        endX = D_800BA9D8.cupPosition[AXIS_X];
        endZ = g_CupPositionZ;
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
    endRating = GetLandingRating(GetSurfaceInfoAt(endX, endZ)->kind);
    osSyncPrintf("TALevel %d\n", endRating);
    /* Counting the steps in index is there to match */
    for (index = 0; index < reach * TARGET_STEPS_PER_UNIT + EXTRA_TARGET_STEP_COUNT; index++) {
        along = TARGET_STEP_YARDS * (index * UNITS_PER_YARD);
        alongX = along * cosAngle + endX;
        alongZ = along * sinAngle + endZ;
        for (side = 0; side < sideCount; side++) {
            offsetX = -sinAngle * side * TARGET_SIDE_SPACING;
            offsetZ = cosAngle * side * TARGET_SIDE_SPACING;
            if (side & 1) {
                candidateX = alongX + offsetX;
                candidateZ = alongZ + offsetZ;
            } else {
                candidateX = alongX - offsetX;
                candidateZ = alongZ - offsetZ;
            }
            if (distance < (s32)IntLength2D(candidateX - x, candidateZ - z)) {
                break;
            }
            if (GetLandingRating(GetSurfaceInfoAt(candidateX, candidateZ)->kind) < endRating) {
                continue;
            }
            if (sIsDirectionExcluded[((u32)(s32)(atan2f(candidateZ - sShot.ballPosition[AXIS_Z],
                                                  candidateX - sShot.ballPosition[AXIS_X]) *
                                            RADIANS_TO_BINARY_ANGLE) &
                                      BINARY_ANGLE_MASK) >>
                                     DIRECTION_SECTOR_SHIFT]) {
                continue;
            }
            height = GetGroundHeight(candidateX, candidateZ);
            flatDistance =
                IntLength2D(candidateZ - sShot.ballPosition[AXIS_Z], candidateX - sShot.ballPosition[AXIS_X]);
            if (flatDistance < sShot.ballPosition[AXIS_Y] - height) {
                continue;
            }
            cupDistance = IntLength2D(candidateX - D_800BA9D8.cupPosition[AXIS_X],
                                      candidateZ - D_800BA9D8.cupPosition[AXIS_Z]);
            if (FindGroundHit(sShot.ballPosition[AXIS_X], sShot.ballPosition[AXIS_Y] - PATH_GROUND_CLEARANCE,
                              sShot.ballPosition[AXIS_Z], candidateX, sShot.ballPosition[AXIS_Y] - flatDistance,
                              candidateZ, &hit, &hitFraction)) {
                continue;
            }
            if (cupDistance >= MIN_TESTED_ROLL) {
                if (FindGroundHit(candidateX, height - PATH_GROUND_CLEARANCE, candidateZ,
                                  D_800BA9D8.cupPosition[AXIS_X], height - cupDistance / 2, g_CupPositionZ, &hit,
                                  &hitFraction)) {
                    continue;
                }
            }
            isPoorlySurrounded = FALSE;
            if (candidateX != D_800BA9D8.cupPosition[AXIS_X] || candidateZ != D_800BA9D8.cupPosition[AXIS_Z]) {
                for (column = -TARGET_NEIGHBOUR_REACH; column <= TARGET_NEIGHBOUR_REACH; column++) {
                    for (row = -TARGET_NEIGHBOUR_REACH; row <= TARGET_NEIGHBOUR_REACH; row++) {
                        if (column == 0 && row == 0) {
                            continue;
                        }
                        if (GetLandingRating(
                                GetSurfaceInfoAt(candidateX + column * TARGET_NEIGHBOUR_SPACING * UNITS_PER_YARD,
                                                 candidateZ + row * TARGET_NEIGHBOUR_SPACING * UNITS_PER_YARD)
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
    osSyncPrintf("Result TALevel %d\n", GetLandingRating(GetSurfaceInfoAt(*targetX, *targetZ)->kind));
}

/* The shot modes for each stroke count; all of them are SHOT_MODE_NORMAL */
#define STROKE_SHOT_MODE_COUNT 16

/*
 * Plans a shot from beyond approach range: aims at the cup with the club whose
 * carry covers PlanCupDistance, in the shot mode for the player's stroke count
 */
void ChooseLongShot(s32 unused, s32 *clubType, s32 *targetX, s32 *targetZ, s32 *shotMode) {
    s32 pad[44]; /* Unused; it is there to match */
    s32 shotModes[STROKE_SHOT_MODE_COUNT];
    s32 index;
    s32 one; /* one is there to match */
    s32 strokeCount;

    one = SHOT_MODE_NORMAL;
    for (index = STROKE_SHOT_MODE_COUNT - 1; index >= 0; index--) {
        shotModes[index] = one;
    }
    strokeCount = g_Players[sShot.playerIndex].unk23;
    *clubType = ChooseClubForDistance(PlanCupDistance(one));
    *targetX = D_800BA9D8.cupPosition[AXIS_X];
    *targetZ = g_CupPositionZ;
    *shotMode = shotModes[strokeCount];
}

/* It is a macro, as an inline function compiles differently from the original */
#define IsOffFairway(surfaceKind)                                                                       \
    ((surfaceKind) != SURFACE_KIND_FAIRWAY && (surfaceKind) != SURFACE_KIND_FAIRWAY_FAIR_LIE &&        \
     (surfaceKind) != SURFACE_KIND_FAIRWAY_POOR_LIE && (surfaceKind) != SURFACE_KIND_SEMI_ROUGH &&     \
     (surfaceKind) != SURFACE_KIND_GREEN && (surfaceKind) != SURFACE_KIND_GREEN_EDGE)

/*
 * Returns whether any point a yard apart on the line from from to to, from
 * from itself, lies off the fairway, semi-rough, green and green edge.
 */
s32 IsLineOffFairway(CoursePoint from, CoursePoint to) {
    s32 distance;
    f32 angle;
    s32 step;
    f32 stepLength;
    s32 x;
    s32 z;
    u32 kind;

    distance = IntLength2D(to[AXIS_X] - from[AXIS_X], to[AXIS_Z] - from[AXIS_Z]);
    angle = atan2((f32)(to[AXIS_Z] - from[AXIS_Z]), (f32)(to[AXIS_X] - from[AXIS_X]));
    for (step = 0; step < distance * YARDS_PER_UNIT; step++) {
        /* Setting stepLength in the loop is there to match */
        stepLength = UNITS_PER_YARD;
        x = step * (cosf(angle) * stepLength) + from[AXIS_X];
        z = step * (sinf(angle) * stepLength) + from[AXIS_Z];
        kind = GetSurfaceInfoAt(x, z)->kind;
        if (IsOffFairway(kind)) {
            return TRUE;
        }
    }
    return FALSE;
}

/* FindGreenEdgeOnLine's results */
#define GREEN_EDGE_FOUND 1
#define GREEN_EDGE_NOT_FOUND (-1)

/*
 * Steps a yard at a time along the line from from to to, and stores the
 * first point on a green edge in edge. Returns GREEN_EDGE_FOUND if it found
 * one, otherwise GREEN_EDGE_NOT_FOUND.
 */
s32 FindGreenEdgeOnLine(CoursePoint from, CoursePoint to, CoursePoint edge) {
    s32 distance;
    f32 angle;
    s32 step;
    f32 stepLength;
    s32 x;
    s32 z;

    distance = IntLength2D(to[AXIS_X] - from[AXIS_X], to[AXIS_Z] - from[AXIS_Z]);
    angle = atan2((f32)(to[AXIS_Z] - from[AXIS_Z]), (f32)(to[AXIS_X] - from[AXIS_X]));
    osSyncPrintf("StartPoint = (%d, %d)\n", from[AXIS_X], from[AXIS_Z]);
    osSyncPrintf("EndPoint = (%d, %d)\n", to[AXIS_X], to[AXIS_Z]);
    for (step = 1; step < distance * YARDS_PER_UNIT + 1; step++) {
        /* Setting stepLength in the loop is there to match */
        stepLength = UNITS_PER_YARD;
        x = step * (cosf(angle) * stepLength) + from[AXIS_X];
        z = step * (sinf(angle) * stepLength) + from[AXIS_Z];
        if (GetSurfaceInfoAt(x, z)->kind == SURFACE_KIND_GREEN_EDGE) {
            edge[AXIS_X] = x;
            edge[AXIS_Z] = z;
            return GREEN_EDGE_FOUND;
        }
    }
    return GREEN_EDGE_NOT_FOUND;
}

/*
 * Returns the distance from from to the first green edge on the line to to,
 * or to to itself if the line crosses no green edge.
 */
s32 GetGreenEdgeDistance(CoursePoint from, CoursePoint to) {
    CoursePoint edge;
    s32 distance;

    if (FindGreenEdgeOnLine(from, to, edge) != GREEN_EDGE_FOUND) {
        distance = IntLength2D(to[AXIS_X] - from[AXIS_X], to[AXIS_Z] - from[AXIS_Z]);
    } else {
        distance = IntLength2D(edge[AXIS_X] - from[AXIS_X], edge[AXIS_Z] - from[AXIS_Z]);
    }
    return distance;
}

/*
 * Stores in peak the point a yard apart on the line from from to to whose
 * ground height is farthest from 0, starting with from's x and z. It sets
 * peak's height only for a point past from.
 */
void FindPeakOnLine(CoursePoint from, CoursePoint to, CoursePoint peak) {
    s32 distance;
    f32 angle;
    s32 peakHeight;
    s32 step;
    f32 stepLength;
    s32 x;
    s32 z;
    s32 height;

    distance = IntLength2D(to[AXIS_X] - from[AXIS_X], to[AXIS_Z] - from[AXIS_Z]);
    angle = atan2((f32)(to[AXIS_Z] - from[AXIS_Z]), (f32)(to[AXIS_X] - from[AXIS_X]));
    peakHeight = GetGroundHeight(from[AXIS_X], from[AXIS_Z]);
    peakHeight = ABS(peakHeight);
    peak[AXIS_X] = from[AXIS_X];
    peak[AXIS_Z] = from[AXIS_Z];
    for (step = 1; step < distance * YARDS_PER_UNIT; step++) {
        /* Setting stepLength in the loop is there to match */
        stepLength = UNITS_PER_YARD;
        x = step * (cosf(angle) * stepLength) + from[AXIS_X];
        z = step * (sinf(angle) * stepLength) + from[AXIS_Z];
        height = GetGroundHeight(x, z);
        height = ABS(height);
        if (peakHeight < height) {
            peak[AXIS_X] = x;
            peak[AXIS_Y] = GetGroundHeight(x, z);
            peakHeight = height;
            peak[AXIS_Z] = z;
        }
    }
}

#define APPROACH_BAND_COUNT 4
#define APPROACH_STYLE_COUNT 3
#define APPROACH_STYLE_UNK1 1
#define APPROACH_STYLE_UNK2 2

/*
 * The upper limits of each approach band, as shares of the cup distance that lie
 * short of the green, for each value of the golfer's unk1 trait
 */
extern f32 sApproachBandLimits[APPROACH_BAND_COUNT][APPROACH_STYLE_COUNT];

/*
 * Aims at the cup and returns the type of club to approach it with. The club
 * depends on how much of the line to the cup lies short of the green, where the
 * line's peak counts when it is on the green, and on the golfer's unk1 trait.
 * A line that crosses rough or hazards takes a wedge.
 */
s32 ChooseApproachClub(s32 *targetX, s32 *targetZ) {
    CoursePoint peak;
    s32 cupDistance;
    s32 edgeDistance;
    f32 edgeShare;
    u32 peakKind;
    s32 band;
    s32 style;

    *targetX = D_800BA9D8.cupPosition[AXIS_X];
    *targetZ = g_CupPositionZ;
    cupDistance = IntLength2D(D_800BA9D8.cupPosition[AXIS_X] - sShot.ballPosition[AXIS_X],
                              g_CupPositionZ - sShot.ballPosition[AXIS_Z]);
    /* Unused; it is there to match */
    GetSurfaceInfo(sShot.surface);
    if (IsLineOffFairway(sShot.ballPosition, D_800BA9D8.cupPosition) == TRUE) {
        osSyncPrintf("Appr60 Return point 1\n");
        if (sGolferTraits[(s8)g_Players[sShot.playerIndex].characterIndex].windPlay < WIND_PLAY_FOLLOWING_WINDS) {
            return CLUB_TYPE_PITCHING_WEDGE;
        }
        return CLUB_TYPE_SAND_WEDGE;
    }
    edgeDistance = GetGreenEdgeDistance(sShot.ballPosition, D_800BA9D8.cupPosition);
    edgeShare = (f32)edgeDistance / (f32)cupDistance;
    osSyncPrintf("Approach : DistForEdge = %d\n", edgeDistance);
    osSyncPrintf("Approach : Distance = %d\n", cupDistance);
    osSyncPrintf("Approach : EdgePercent = %f\n", edgeShare);
    FindPeakOnLine(sShot.ballPosition, D_800BA9D8.cupPosition, peak);
    peakKind = GetSurfaceInfoAt(peak[AXIS_X], peak[AXIS_Z])->kind;
    if (peakKind == SURFACE_KIND_GREEN || peakKind == SURFACE_KIND_GREEN_EDGE) {
        edgeDistance =
            IntLength2D(peak[AXIS_X] - sShot.ballPosition[AXIS_X], peak[AXIS_Z] - sShot.ballPosition[AXIS_Z]);
        edgeShare = (f32)edgeDistance / (f32)cupDistance;
    }
    style = sGolferTraits[(s8)g_Players[sShot.playerIndex].characterIndex].unk1;
    for (band = 0; band < APPROACH_BAND_COUNT; band++) {
        if (edgeShare < sApproachBandLimits[band][style]) {
            if (band == 0) {
                return CLUB_TYPE_PITCHING_WEDGE;
            } else if (band == 1) {
                if (style == APPROACH_STYLE_UNK2) {
                    return CLUB_TYPE_DRIVER;
                }
                if (style == APPROACH_STYLE_UNK1) {
                    return CLUB_TYPE_THREE_WOOD;
                }
                return CLUB_TYPE_FOUR_WOOD;
            } else if (band == 2) {
                if (style == APPROACH_STYLE_UNK2) {
                    return CLUB_TYPE_FIVE_IRON;
                }
                if (style == APPROACH_STYLE_UNK1) {
                    return CLUB_TYPE_FOUR_IRON;
                }
                return CLUB_TYPE_THREE_IRON;
            } else if (band == 3) {
                if (style == APPROACH_STYLE_UNK2) {
                    return CLUB_TYPE_SAND_WEDGE;
                }
                if (style == APPROACH_STYLE_UNK1) {
                    return CLUB_TYPE_PITCHING_WEDGE;
                }
                return CLUB_TYPE_NINE_IRON;
            }
        }
    }
    return CLUB_TYPE_PITCHING_WEDGE;
}

#define SHOT_UNK4_SCALE (SHOT_UNK4_MAX / 256.0f)

s32 ScaleToShotUnk4(s32 amount) {
    s32 value;
    s32 unused;
    s32 uninitialized;

    /* The store of an uninitialized value is there to match */
    *(&unused) = uninitialized;

    value = amount * SHOT_UNK4_SCALE + 0.5f;
    if (value < SHOT_UNK4_MIN) {
        value = SHOT_UNK4_MIN;
    }
    if (value > SHOT_UNK4_MAX - 1) {
        value = SHOT_UNK4_MAX;
    }
    return value;
}

/* The fault report of StepTrialShot and IsTrialShotSafe, which share its one copy */
extern const char sSimFaultText[];

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", sSimFaultText);

/* What the trial costs for each step a putt takes, and for a putt at the stroke limit */
#define PUTT_STEP_EVALUATION 0x10
#define POOR_LANDING_EVALUATION 0x8000
#define BLOCKED_CUP_EVALUATION 0x80000
#define FAR_FROM_TARGET 0x40000000
#define MISS_EVALUATION_SCALE 0.006535948f
#define APPROACH_TOLERANCE_SCALE 0.0065104165f
#define LONG_PUTT_DISTANCE 0x7800
#define PUTT_DRIFT_LIMIT 0xF000
/* The largest launch power a club can give */
#define MAX_LAUNCH_POWER 256
#define AIM_CHANGE_STEP 0x400
#define EXCLUDED_SECTOR_COUNT 9

/*
 * Advances the trial of sShot by one step of the ball simulation, setting the
 * trial up first unless it is under way. It returns 0 while the trial is still
 * better than the best so far, and 1 once it has evaluated the trial, or
 * abandoned it, and adjusted sShot's power, mode or aim for the next one.
 *
 * It does not match: ours gives some values other saved registers than the
 * original does, and it keeps the address of sShot.ballPosition's z in a saved
 * register across the atan2 calls, where the original loads it again.
 */
#ifdef NON_MATCHING
s32 StepTrialShot(void) {
    PlaneTriangle hit;
    f32 fraction;
    s32 flags;
    s32 isRunning;
    s32 stepFlags;
    s32 unk24Y;
    s32 unk94;
    s32 targetMiss;
    s32 heightMiss;
    SurfaceInfo *surfaceInfo;
    s32 targetRating;
    s32 kind;
    s32 par;
    s32 strokes;
    s32 strokesOver;
    s32 approachDistance;
    s32 flyDistance;
    s32 targetDistance;
    f32 aimCos;
    f32 aimSin;
    f32 ratio;
    s32 launchPower;
    s32 index;

    flags = 0;
    if (sPlanStage != PLAN_STAGE_TRIAL_RUNNING) {
        sIsTrialLost = FALSE;
        sTrialEvaluation = 0;
        sIsPutting = FALSE;
        sHasUnk1Position = FALSE;
        sShot.unk14 = sShotOffsets[sShotOffset].unk14 + (rand() & 7) - 3;
        sShot.unk18 = sShotOffsets[sShotOffset].unk18 + (rand() & 7) - 3;
        sTrialLaunch->unk0 = sShot.unk0;
        sTrialLaunch->clubIndex = sShot.clubSlot;
        sClosestTargetDistance = FAR_FROM_TARGET;
        sTrialLaunch->unk8 = func_8005F248(sShot.unk4);
        sTrialLaunch->unkC = 0;
        sTrialLaunch->unk10 = sShot.unk14;
        sTrialLaunch->unk14 = sShot.unk18;
        sTrialLaunch->unk18 = sShot.playerIndex;
        sTrialLaunch->shotMode = sShot.shotMode;
        sIsPutting = GetClub(sShot.clubSlot)->surfaceMask & CLUB_SURFACE_GREEN;
        if (!IsGameFlagSet(GAME_FLAG_UNK3C)) {
            if (sIsPutting) {
                sAcceptableEvaluation = sAcceptableEvaluation * 1.1f + 0.2f;
            } else {
                sAcceptableEvaluation = sAcceptableEvaluation * 1.1f + 20.0f;
            }
        }
        sTrialSimulation.position[AXIS_X] = sShot.ballPosition[AXIS_X];
        sTrialSimulation.position[AXIS_Y] = sShot.ballPosition[AXIS_Y];
        sTrialSimulation.position[AXIS_Z] = sShot.ballPosition[AXIS_Z];
        sTrialSimulation.surface = sShot.surface;
        func_80037F94(&sTrialSimulation, 2, sTrialLaunch);
        sTrialStepCount = 0;
        sHasUnk3Contact = FALSE;
        sTrialEndPoint[AXIS_X] = sTrialSimulation.position[AXIS_X];
        sTrialEndPoint[AXIS_Y] = sTrialSimulation.position[AXIS_Y];
        sTrialEndPoint[AXIS_Z] = sTrialSimulation.position[AXIS_Z];
        sUnk1Position[AXIS_X] = sTrialSimulation.position[AXIS_X];
        sUnk1Position[AXIS_Y] = sTrialSimulation.position[AXIS_Y];
        sUnk1Position[AXIS_Z] = sTrialSimulation.position[AXIS_Z];
    }
    isRunning = sTrialSimulation.isRunning;
    if (isRunning) {
        do {
            unk24Y = sTrialSimulation.unk24[AXIS_Y];
            unk94 = sTrialSimulation.unk94;
            sTrialStepCount++;
            stepFlags = flags = func_8003327C(&sTrialSimulation);
            if (sTrialStepCount >= MAX_STEP_COUNT) {
                SetDebugTextHighlight(TRUE);
                PrintDebugText(sSimFaultText, 10, 12);
                break;
            }
            if (flags & STEP_FLAG_UNK10) {
                break;
            }
            if (unk94 == 0 && (flags & 3) && unk24Y < 0 && isRunning == 1) {
                if (sHasBestEvaluation || sTrialLevel < 2) {
                    break;
                }
                sHasUnk3Contact = isRunning;
            }
            if (sIsPutting) {
                targetMiss = IntLength2D(sTrialSimulation.position[AXIS_X] - sPlanTarget[AXIS_X],
                                         sTrialSimulation.position[AXIS_Z] - sPlanTarget[AXIS_Z]);
                if (targetMiss < sClosestTargetDistance) {
                    sClosestTargetDistance = targetMiss;
                    sTrialEndPoint[AXIS_X] = sTrialSimulation.position[AXIS_X];
                    sTrialEndPoint[AXIS_Y] = sTrialSimulation.position[AXIS_Y];
                    sTrialEndPoint[AXIS_Z] = sTrialSimulation.position[AXIS_Z];
                }
                if (stepFlags & 4) {
                    sTrialEvaluation += PUTT_STEP_EVALUATION;
                    if (IsStrokeLimitReached()) {
                        sTrialEvaluation += POOR_LANDING_EVALUATION;
                    }
                }
            } else if ((stepFlags & STEP_FLAG_UNK1) && sTrialSimulation.isRunning == 1 && !sHasUnk1Position) {
                sHasUnk1Position = sTrialSimulation.isRunning;
                sUnk1Position[AXIS_X] = sTrialSimulation.position[AXIS_X];
                sUnk1Position[AXIS_Y] = sTrialSimulation.position[AXIS_Y];
                sUnk1Position[AXIS_Z] = sTrialSimulation.position[AXIS_Z];
            }
            if (sTrialEvaluation < sBestEvaluation) {
                sPlanStage = PLAN_STAGE_TRIAL_RUNNING;
                return 0;
            }
        } while (0);
    }
    if (!sIsPutting) {
        sTrialEndPoint[AXIS_X] = sTrialSimulation.position[AXIS_X];
        sTrialEndPoint[AXIS_Y] = sTrialSimulation.position[AXIS_Y];
        sTrialEndPoint[AXIS_Z] = sTrialSimulation.position[AXIS_Z];
    }
    if (!sTrialSimulation.isRunning) {
        IntLength2D(D_800BA9D8.cupPosition[AXIS_X] - sTrialEndPoint[AXIS_X], g_CupPositionZ - sTrialEndPoint[AXIS_Z]);
        targetMiss = IntLength2D(sPlanTarget[AXIS_X] - sTrialEndPoint[AXIS_X],
                                 sPlanTarget[AXIS_Z] - sTrialEndPoint[AXIS_Z]);
        if (sIsPutting) {
            targetMiss = IntLength2D(D_800BA9D8.cupPosition[AXIS_X] - sTrialSimulation.position[AXIS_X],
                                     g_CupPositionZ - sTrialSimulation.position[AXIS_Z]);
            if (IntLength2D(sTrialSimulation.position[AXIS_X] - sTrialEndPoint[AXIS_X],
                            sTrialSimulation.position[AXIS_Z] - sTrialEndPoint[AXIS_Z]) > PUTT_DRIFT_LIMIT) {
                sTrialEvaluation = POOR_LANDING_EVALUATION;
            }
            if (g_Players[sShot.playerIndex].unk23 + 1 < g_HolePar + sHoleTargetToPar && sHoleTargetToPar > 0 &&
                IntLength2D(sShot.ballPosition[AXIS_X] - sTrialEndPoint[AXIS_X],
                            sShot.ballPosition[AXIS_Z] - sTrialEndPoint[AXIS_Z]) > LONG_PUTT_DISTANCE) {
                goto evaluate;
            }
        }
        if (sTrialSimulation.unkA0) {
            sTrialEvaluation = 0;
        } else {
        evaluate:
            surfaceInfo = GetSurfaceInfo(sTrialSimulation.surface);
            if (FindGroundHit(sTrialSimulation.position[AXIS_X],
                              sTrialSimulation.position[AXIS_Y] - PATH_GROUND_CLEARANCE,
                              sTrialSimulation.position[AXIS_Z], D_800BA9D8.cupPosition[AXIS_X],
                              sTrialSimulation.position[AXIS_Y] -
                                  (IntLength2D(sTrialSimulation.position[AXIS_X] - D_800BA9D8.cupPosition[AXIS_X],
                                               sTrialSimulation.position[AXIS_Z] - g_CupPositionZ) >>
                                   1),
                              g_CupPositionZ, &hit, &fraction)) {
                sTrialEvaluation += BLOCKED_CUP_EVALUATION;
            }
            targetRating = GetLandingRating(GetSurfaceInfoAt(sPlanTarget[AXIS_X], sPlanTarget[AXIS_Z])->kind);
            if (GetLandingRating(surfaceInfo->kind) < targetRating) {
                kind = surfaceInfo->kind;
                sTrialEvaluation += POOR_LANDING_EVALUATION;
                if (kind == SURFACE_KIND_OUT_OF_BOUNDS || kind == SURFACE_KIND_OUT_OF_COURSE) {
                    sTrialEvaluation = LOST_BALL_EVALUATION;
                    sIsTrialLost = TRUE;
                }
                if (surfaceInfo->kind == SURFACE_KIND_WATER_HAZARD) {
                    osSyncPrintf("Water Hazzad\n");
                    sTrialEvaluation = LOST_BALL_EVALUATION;
                    sIsTrialLost = TRUE;
                }
            }
            heightMiss = sPlanTarget[AXIS_Y] - sTrialSimulation.position[AXIS_Y];
            sTrialEvaluation += (targetMiss + ABS(heightMiss) / 2) * MISS_EVALUATION_SCALE;
        }
        osSyncPrintf("BestEval = %d\n", sBestEvaluation);
        if (sTrialEvaluation < sBestEvaluation) {
            sHasBestEvaluation = TRUE;
            sBestEvaluation = sTrialEvaluation;
            sTrialShot = sShot;
        }
        if (sTrialIndex != 0) {
            par = g_HolePar;
            strokes = g_Players[sShot.playerIndex].unk23;
            if (GetSurfaceInfoAt(sPlanTarget[AXIS_X], sPlanTarget[AXIS_Z])->kind == SURFACE_KIND_GREEN) {
                strokesOver = strokes + 1;
                strokesOver -= par + sHoleTargetToPar;
                if (strokesOver >= 0) {
                    if (sBestEvaluation <= sAcceptableEvaluation) {
                        sIsTrialAccepted = TRUE;
                        return 1;
                    }
                } else if (!sIsPutting) {
                    if (strokesOver >= -1) {
                        if (sBestEvaluation <= sAcceptableEvaluation + 500) {
                            sIsTrialAccepted = TRUE;
                            return 1;
                        }
                    } else if (sBestEvaluation < sAcceptableEvaluation + 0x8000) {
                        sIsTrialAccepted = TRUE;
                        return 1;
                    }
                } else {
                    approachDistance = (s32)IntLength2D(sShot.ballPosition[AXIS_X] - sPlanTarget[AXIS_X],
                                                        sShot.ballPosition[AXIS_Z] - sPlanTarget[AXIS_Z]) *
                                       APPROACH_TOLERANCE_SCALE;
                    if (strokesOver >= -1) {
                        if (sBestEvaluation <= approachDistance * 0.3f + sAcceptableEvaluation) {
                            sIsTrialAccepted = TRUE;
                            return 1;
                        }
                    } else if (sBestEvaluation <= approachDistance * 0.4f + sAcceptableEvaluation) {
                        sIsTrialAccepted = TRUE;
                        return 1;
                    }
                }
            } else if (sBestEvaluation < sAcceptableEvaluation + 8000) {
                sIsTrialAccepted = TRUE;
                return 1;
            }
        }
        if (!sIsPutting) {
            flyDistance = IntLength2D((sTrialSimulation.position[AXIS_X] + sTrialEndPoint[AXIS_X]) / 2 -
                                          sShot.ballPosition[AXIS_X],
                                      (sTrialSimulation.position[AXIS_Z] + sTrialEndPoint[AXIS_Z]) / 2 -
                                          sShot.ballPosition[AXIS_Z]);
            targetDistance = IntLength2D(sPlanTarget[AXIS_X] - sShot.ballPosition[AXIS_X],
                                         sPlanTarget[AXIS_Z] - sShot.ballPosition[AXIS_Z]);
        } else {
            aimCos = cosf((u16)sShot.unk0 * BINARY_ANGLE_TO_RADIANS);
            aimSin = sinf((u16)sShot.unk0 * BINARY_ANGLE_TO_RADIANS);
            flyDistance =
                aimCos * ((sTrialSimulation.position[AXIS_X] * 3 + sTrialEndPoint[AXIS_X]) / 4 -
                          sShot.ballPosition[AXIS_X]) +
                aimSin * ((sTrialSimulation.position[AXIS_Z] * 3 + sTrialEndPoint[AXIS_Z]) / 4 -
                          sShot.ballPosition[AXIS_Z]);
            targetDistance = aimCos * (sPlanTarget[AXIS_X] - sShot.ballPosition[AXIS_X]) +
                             aimSin * (sPlanTarget[AXIS_Z] - sShot.ballPosition[AXIS_Z]);
            if (flyDistance <= 0) {
                flyDistance = 1;
            }
            if (targetDistance <= 0) {
                targetDistance = 1;
            }
        }
        osSyncPrintf("flydist = %d\n", flyDistance);
        osSyncPrintf("targetdist = %d\n", targetDistance);
        osSyncPrintf("powermode = %d\n", sShot.shotMode);
        osSyncPrintf("powermodeleft = %d\n", sShot.unk54);
        osSyncPrintf("ClubSelectType = %d\n", sGolferTraits[(s8)g_Players[sShot.playerIndex].characterIndex].unk2);
        if (flyDistance != 0) {
            ratio = (f32)targetDistance / flyDistance;
            launchPower = sTrialLaunch->unk8 * ratio;
            if (launchPower > MAX_LAUNCH_POWER) {
                if (sShot.shotMode == SHOT_MODE_NORMAL && sShot.unk54 > 0 &&
                    ((sGolferTraits[(s8)g_Players[sShot.playerIndex].characterIndex].unk6 != 0 &&
                      sGolferTraits[(s8)g_Players[sShot.playerIndex].characterIndex].unk2 < 2) ||
                     (rand() & 1)) &&
                    GetSurfaceInfoAt(sPlanTarget[AXIS_X], sPlanTarget[AXIS_Z])->kind == SURFACE_KIND_GREEN) {
                    sShot.shotMode = SHOT_MODE_POWER;
                    osSyncPrintf("Power mode\n ");
                } else if (sShot.shotMode == SHOT_MODE_NORMAL && sShot.unk54 > 0 &&
                           sGolferTraits[(s8)g_Players[sShot.playerIndex].characterIndex].unk2 == 2) {
                    sShot.shotMode = SHOT_MODE_POWER;
                    osSyncPrintf("Power mode\n ");
                } else if (sShot.shotMode == SHOT_MODE_APPROACH) {
                    sShot.shotMode = SHOT_MODE_NORMAL;
                } else if (targetDistance < flyDistance) {
                    osSyncPrintf("Level Down\n");
                    sNeedsShorterClub = TRUE;
                    return 1;
                } else if (sHasUnk3Contact || ratio >= 10.0f) {
                    sAimChangeCount++;
                    sShot.unk0 = !(sAimChangeCount & 1) ? sShot.unk0 - sAimChangeCount * AIM_CHANGE_STEP
                                                        : sShot.unk0 + sAimChangeCount * AIM_CHANGE_STEP;
                    osSyncPrintf("Angle Change3\n");
                    for (index = 0; index < EXCLUDED_SECTOR_COUNT; index++) {
                        sIsDirectionExcluded[(u8)((sShot.unk0 >> DIRECTION_SECTOR_SHIFT) + index -
                                                  EXCLUDED_SECTOR_COUNT / 2)] = TRUE;
                    }
                    if (sAimChangeCount >= 17) {
                        osSyncPrintf("Level Down\n");
                        sAimChangeCount = 0;
                        sNeedsShorterClub = TRUE;
                    }
                    return 1;
                } else {
                    sNeedsLongerClub = TRUE;
                    osSyncPrintf("Level Up1\n");
                    return 1;
                }
            } else {
                sShot.unk4 = ScaleToShotUnk4(launchPower);
                if (sIsPutting) {
                    sShot.unk4 = sShot.unk4 + 1 + (rand() & 1);
                }
                if (sShot.unk4 > SHOT_UNK4_MAX) {
                    sShot.unk4 = SHOT_UNK4_MAX;
                }
                if (sShot.unk4 < 2) {
                    sShot.unk4 = 2;
                }
                if (sTrialLaunch->unk8 < launchPower) {
                    if (func_8005F248(sShot.unk4) <= sTrialLaunch->unk8 && sShot.unk4 < SHOT_UNK4_MAX) {
                        sShot.unk4++;
                    }
                } else if (launchPower < sTrialLaunch->unk8) {
                    if (func_8005F248(sShot.unk4) >= sTrialLaunch->unk8 && sShot.unk4 >= 4) {
                        sShot.unk4--;
                    }
                }
            }
        }
        sShot.unk0 += (s32)(atan2(sPlanTarget[AXIS_Z] - sShot.ballPosition[AXIS_Z],
                                  sPlanTarget[AXIS_X] - sShot.ballPosition[AXIS_X]) *
                            RADIANS_TO_BINARY_ANGLE) -
                      (s32)(atan2(sTrialEndPoint[AXIS_Z] - sShot.ballPosition[AXIS_Z],
                                  sTrialEndPoint[AXIS_X] - sShot.ballPosition[AXIS_X]) *
                            RADIANS_TO_BINARY_ANGLE);
        return 1;
    }
    if (flags & STEP_FLAG_UNK1) {
        if (!(GetClub(g_Players[sShot.playerIndex].clubs[sShot.clubSlot])->surfaceMask & CLUB_SURFACE_GREEN)) {
            sShot.clubSlot++;
            if (GetClub(g_Players[sShot.playerIndex].clubs[sShot.clubSlot])->surfaceMask & CLUB_SURFACE_GREEN) {
                sShot.clubSlot--;
            }
        }
    }
    sAimChangeCount++;
    for (index = 0; index < EXCLUDED_SECTOR_COUNT; index++) {
        sIsDirectionExcluded[(u8)((sShot.unk0 >> DIRECTION_SECTOR_SHIFT) + index - EXCLUDED_SECTOR_COUNT / 2)] = TRUE;
    }
    sShotOffset = SHOT_OFFSET_MINUS_LARGE;
    if (!(sAimChangeCount & 1)) {
        sShot.unk0 -= sAimChangeCount * AIM_CHANGE_STEP;
    } else {
        sShot.unk0 += sAimChangeCount * AIM_CHANGE_STEP;
    }
    osSyncPrintf("Angle Change4\n");
    if (sAimChangeCount >= 3) {
        osSyncPrintf("Level Down\n");
        sAimChangeCount = 0;
        sNeedsShorterClub = TRUE;
    }
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", StepTrialShot);
#endif

/* Returns the slot of the golfer's club of type clubType, or 0 if there is none */
s32 FindClubSlot(s32 unused, s32 clubType) {
    s32 slot;

    for (slot = 0; slot < CLUB_SLOT_COUNT; slot++) {
        if (GetClub(g_Players[sShot.playerIndex].clubs[slot])->type == clubType) {
            return slot;
        }
    }
    return 0;
}

/* GetLieRating's worst rating */
#define WORST_LIE_RATING 1
/* What rain leaves of a shot's distance, for golfers it shortens */
#define RAINY_DISTANCE_FRACTION 0.95f

/*
 * Returns the type of the shortest usable club, from the sand wedge up, whose
 * normal shot carries beyond distance from the ball's lie. A club the golfer
 * would hit from the worst lie only counts if the golfer's unk2 trait is 2 or
 * more. If no club carries that far, it returns the slot of the longest club
 * that counted, or CLUB_TYPE_SAND_WEDGE if none did.
 */
s32 ChooseClubForDistance(s32 distance) {
    s32 fallback = CLUB_TYPE_SAND_WEDGE;
    s32 type;
    s32 slot;
    Club *club;
    s32 carry;
    s32 rating;

    for (type = CLUB_TYPE_SAND_WEDGE; type >= 0; type--) {
        slot = FindClubSlot(sShot.playerIndex, type);
        if (!sIsClubUsable[slot]) {
            continue;
        }
        club = GetClub(g_Players[sShot.playerIndex].clubs[slot]);
        carry = GetShotWorldDistance(club, SHOT_MODE_NORMAL);
        if (g_HoleConditions.isRaining == TRUE &&
            sGolferTraits[(s8)g_Players[sShot.playerIndex].characterIndex].unk1 < 2) {
            carry = carry * RAINY_DISTANCE_FRACTION;
        }
        carry = carry * GetSurfaceInfo(sShot.surface)->minPercent / 100;
        rating = GetLieRating(club, sShot.surface, SHOT_MODE_NORMAL);
        if (sGolferTraits[(s8)g_Players[sShot.playerIndex].characterIndex].unk2 < 2) {
            if (rating == WORST_LIE_RATING) {
                continue;
            }
        }
        fallback = slot;
        if (distance < carry) {
            return type;
        }
    }
    return fallback;
}

/* Returns how far apart two binary angles are, up to half a turn */
s32 GetAngleDifference(s32 angle, s32 otherAngle) {
    s32 difference = (s16)(angle - otherAngle);

    if (difference < 0) {
        difference = -difference;
    }
    return difference;
}

/* How far below the ball a target must be for the largest minus offset */
#define DOWNHILL_TARGET_DROP 0xB400
/* The angle within which the wind counts as following the shot */
#define FOLLOWING_WIND_ANGLE 10000

/* It is a macro, as an inline function compiles differently from the original */
#define IsWindSpeedInRange(lowest, limit) \
    (g_HoleConditions.windSpeed >= (lowest) && g_HoleConditions.windSpeed < (limit))

/* sClubKindChances rows */
#define CLUB_CHANCE_SET_FOLLOWING_WIND 0
#define CLUB_CHANCE_SET_HEAD_WIND 1

/*
 * Chooses the offset to give the planned shot, from the lie, the golfer's
 * windPlay trait, whether the shot aims at the cup, how far below the ball
 * the target is, and the wind's speed and direction against the shot's yaw.
 * Most offsets also need RollClubKindChance to succeed.
 */
s32 ChooseShotOffset(void) {
    s32 windPlay;
    s32 aimYaw;
    s32 windDirection;

    switch (GetSurfaceInfo(sShot.surface)->kind) {
        case SURFACE_KIND_BUNKER:
        case SURFACE_KIND_BUNKER_FAIR_LIE:
        case SURFACE_KIND_BUNKER_POOR_LIE:
        case SURFACE_KIND_BUNKER_UNK9:
        case SURFACE_KIND_GREEN:
        case SURFACE_KIND_GREEN_EDGE:
        case SURFACE_KIND_BARE_GROUND:
        case SURFACE_KIND_DEEP_ROUGH:
        case SURFACE_KIND_BARE_GROUND_UNK20:
        case SURFACE_KIND_WASTE_AREA:
            return SHOT_OFFSET_NONE;
    }
    windPlay = sGolferTraits[(s8)g_Players[sShot.playerIndex].characterIndex].windPlay;
    if (sPlanTarget[AXIS_X] == D_800BA9D8.cupPosition[AXIS_X] && sPlanTarget[AXIS_Z] == g_CupPositionZ) {
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
    if (sPlanTarget[AXIS_Y] < sShot.ballPosition[AXIS_Y] - DOWNHILL_TARGET_DROP) {
        return SHOT_OFFSET_MINUS_LARGE;
    }
    aimYaw = (u16)sShot.unk0;
    windDirection = g_HoleConditions.windDirection;
    osSyncPrintf("skSCAI.angy = %ld\n", aimYaw);
    osSyncPrintf("wind_ang = %ld\n", windDirection);
    if (GetAngleDifference(windDirection, aimYaw) < FOLLOWING_WIND_ANGLE) {
        osSyncPrintf("---- Follow\n");
        if (windPlay == WIND_PLAY_FOLLOWING_WINDS) {
            if (IsWindSpeedInRange(4 * WIND_PUSH_UNIT, 5 * WIND_PUSH_UNIT)) {
                if (RollClubKindChance(CLUB_CHANCE_SET_FOLLOWING_WIND) == TRUE) {
                    return SHOT_OFFSET_MINUS_SMALL;
                }
            } else if (IsWindSpeedInRange(5 * WIND_PUSH_UNIT, 7 * WIND_PUSH_UNIT)) {
                if (RollClubKindChance(CLUB_CHANCE_SET_FOLLOWING_WIND) == TRUE) {
                    return SHOT_OFFSET_MINUS_MEDIUM;
                }
            } else if (g_HoleConditions.windSpeed >= 9 * WIND_PUSH_UNIT) {
                if (RollClubKindChance(CLUB_CHANCE_SET_FOLLOWING_WIND) == TRUE) {
                    return SHOT_OFFSET_MINUS_LARGE;
                }
            }
        } else if (windPlay == WIND_PLAY_STRONG_WINDS) {
            if (IsWindSpeedInRange(6 * WIND_PUSH_UNIT, 7 * WIND_PUSH_UNIT)) {
                if (RollClubKindChance(CLUB_CHANCE_SET_FOLLOWING_WIND) == TRUE) {
                    return SHOT_OFFSET_MINUS_SMALL;
                }
            } else if (g_HoleConditions.windSpeed >= 7 * WIND_PUSH_UNIT) {
                if (RollClubKindChance(CLUB_CHANCE_SET_FOLLOWING_WIND) == TRUE) {
                    return SHOT_OFFSET_MINUS_MEDIUM;
                }
            }
        }
    } else {
        osSyncPrintf("---- back\n");
        if (windPlay == WIND_PLAY_HEAD_WINDS) {
            if (IsWindSpeedInRange(4 * WIND_PUSH_UNIT, 5 * WIND_PUSH_UNIT)) {
                if (RollClubKindChance(CLUB_CHANCE_SET_HEAD_WIND) == TRUE) {
                    return SHOT_OFFSET_PLUS_SMALL;
                }
            } else if (IsWindSpeedInRange(5 * WIND_PUSH_UNIT, 7 * WIND_PUSH_UNIT)) {
                if (RollClubKindChance(CLUB_CHANCE_SET_HEAD_WIND) == TRUE) {
                    return SHOT_OFFSET_PLUS_MEDIUM;
                }
            } else if (g_HoleConditions.windSpeed >= 9 * WIND_PUSH_UNIT) {
                if (RollClubKindChance(CLUB_CHANCE_SET_HEAD_WIND) == TRUE) {
                    return SHOT_OFFSET_PLUS_LARGE;
                }
            }
        } else if (windPlay == WIND_PLAY_STRONG_WINDS) {
            if (IsWindSpeedInRange(6 * WIND_PUSH_UNIT, 7 * WIND_PUSH_UNIT)) {
                if (RollClubKindChance(CLUB_CHANCE_SET_HEAD_WIND) == TRUE) {
                    return SHOT_OFFSET_PLUS_SMALL;
                }
            } else if (g_HoleConditions.windSpeed >= 7 * WIND_PUSH_UNIT) {
                if (RollClubKindChance(CLUB_CHANCE_SET_HEAD_WIND) == TRUE) {
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
#define CLUB_CHANCE_WOOD 0
#define CLUB_CHANCE_IRON 1
#define CLUB_CHANCE_OTHER 2

/* Whether the shot's club lies between the slots of the two club types */
/* It is a macro, as an inline function compiles differently from the original */
#define IsShotClubOfTypes(firstType, lastType)                       \
    (sShot.clubSlot >= FindClubSlot(sShot.playerIndex, (firstType)) && \
     FindClubSlot(sShot.playerIndex, (lastType)) >= sShot.clubSlot)

/*
 * Returns TRUE on the golfer's first stroke of the hole. Otherwise it returns
 * TRUE with the percent chance that row chanceSetIndex of sClubKindChances
 * gives for the kind of the shot's club.
 */
s32 RollClubKindChance(s32 chanceSetIndex) {
    if (g_Players[sShot.playerIndex].unk23 == 0) {
        return TRUE;
    }
    if (IsShotClubOfTypes(CLUB_TYPE_DRIVER, CLUB_TYPE_LAST_WOOD)) {
        if (rand() % 100 < sClubKindChances[chanceSetIndex][CLUB_CHANCE_WOOD]) {
            return TRUE;
        }
    } else if (IsShotClubOfTypes(CLUB_TYPE_FIRST_IRON, CLUB_TYPE_LAST_IRON)) {
        if (rand() % 100 < sClubKindChances[chanceSetIndex][CLUB_CHANCE_IRON]) {
            return TRUE;
        }
    } else {
        if (rand() % 100 < sClubKindChances[chanceSetIndex][CLUB_CHANCE_OTHER]) {
            return TRUE;
        }
    }
    return FALSE;
}

/* Tens of yards in a unit of distance */
#define TENS_OF_YARDS_PER_UNIT 0.000007119878f
/* The fraction of a line's points past which the line counts as lost */
#define LOST_LINE_FRACTION 0.5f

/* It is a macro, as an inline function compiles differently from the original */
#define IsLostBallKind(surfaceKind)                                                               \
    ((surfaceKind) == SURFACE_KIND_WATER_HAZARD || (surfaceKind) == SURFACE_KIND_OUT_OF_BOUNDS || \
     (surfaceKind) == SURFACE_KIND_OUT_OF_COURSE)

/*
 * Returns whether more than half of the points a yard apart on the first tenth
 * of the line from from to to lie in water, out of bounds or out of the course.
 */
s32 IsLineMostlyLost(CoursePoint from, CoursePoint to) {
    s32 distance;
    f32 angle;
    s32 sampleCount;
    s32 step;
    s32 lostCount;
    f32 stepLength;
    s32 x;
    s32 z;
    u32 kind;

    distance = IntLength2D(to[AXIS_X] - from[AXIS_X], to[AXIS_Z] - from[AXIS_Z]);
    angle = atan2((f32)(to[AXIS_Z] - from[AXIS_Z]), (f32)(to[AXIS_X] - from[AXIS_X]));
    sampleCount = 0;
    lostCount = 0;
    for (step = 1; step < distance * TENS_OF_YARDS_PER_UNIT; step++) {
        /* Setting stepLength in the loop is there to match */
        stepLength = UNITS_PER_YARD;
        x = step * (cosf(angle) * stepLength) + from[AXIS_X];
        z = step * (sinf(angle) * stepLength) + from[AXIS_Z];
        kind = GetSurfaceInfoAt(x, z)->kind;
        if (IsLostBallKind(kind)) {
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

/*
 * Simulates sTrialShot and returns TRUE if the ball comes to rest, and not out of
 * bounds, off the course or in a water hazard
 *
 * It does not match: ours reads isRunning in the loop test from its address,
 * where the original reads it through the register that holds the simulation's
 * address, and the original places the fault report after the surface test.
 */
#ifdef NON_MATCHING
s32 IsTrialShotSafe(void) {
    u32 kind;
    s32 flags;

    sTrialLaunch->unk0 = sTrialShot.unk0;
    sTrialLaunch->clubIndex = sTrialShot.clubSlot;
    sTrialLaunch->unk8 = func_8005F248(sTrialShot.unk4);
    sTrialLaunch->unkC = sTrialShot.unk8 - 60;
    sTrialLaunch->unk10 = sTrialShot.unk14;
    sTrialLaunch->unk14 = sTrialShot.unk18;
    sTrialLaunch->unk18 = sTrialShot.playerIndex;
    sTrialLaunch->shotMode = sTrialShot.shotMode;
    sTrialSimulation.position[AXIS_X] = sTrialShot.ballPosition[AXIS_X];
    sTrialSimulation.position[AXIS_Y] = sTrialShot.ballPosition[AXIS_Y];
    sTrialSimulation.position[AXIS_Z] = sTrialShot.ballPosition[AXIS_Z];
    sTrialSimulation.surface = sTrialShot.surface;
    func_80037F94(&sTrialSimulation, 2, sTrialLaunch);
    sTrialStepCount = 0;
    while (sTrialSimulation.isRunning) {
        sTrialStepCount++;
        flags = func_8003327C(&sTrialSimulation);
        if (sTrialStepCount < MAX_STEP_COUNT) {
            if (flags & STEP_FLAG_UNK10) {
                break;
            }
        } else {
            SetDebugTextHighlight(TRUE);
            PrintDebugText(sSimFaultText, 10, 12);
            break;
        }
    }
    if (!sTrialSimulation.isRunning) {
        kind = GetSurfaceInfo(sTrialSimulation.surface)->kind;
        if (kind == SURFACE_KIND_OUT_OF_BOUNDS || kind == SURFACE_KIND_OUT_OF_COURSE) {
        } else if (kind != SURFACE_KIND_WATER_HAZARD) {
            osSyncPrintf("nmResultCheck: TRUE\n");
            return TRUE;
        }
    }
    osSyncPrintf("nmResultCheck: FALSE\n");
    return FALSE;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", IsTrialShotSafe);
#endif
