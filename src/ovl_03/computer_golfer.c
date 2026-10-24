#include "club.h"
#include "common.h"
#include "course.h"
#include "current_shot.h"
#include "functions.h"
#include "math_util.h"
#include "save.h"
#include "surface.h"

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
    /* 0x3 */ s8 unk3[0xE - 0x3];
} GolferTraits; /* size = 0xE */

extern GolferTraits sGolferTraits[CHARACTER_COUNT];
extern s32 sPlanThreadState;
extern OSThread sPlanThread;
extern CurrentShot sPlannedShot;
extern u8 sPlanThreadStackEnd[];
extern s32 D_8025C640_ovl_03;
extern s8 sIsPlanStarted;
extern CurrentShot sShot;

s32 PlanShot(CurrentShot *plannedShot);
s32 func_8024F0B0_ovl_03(s32 arg0);
s32 FindGreenEdgeOnLine(CoursePoint from, CoursePoint to, CoursePoint edge);
s32 func_80252F70_ovl_03(s32 arg0);

void RestartShotPlan(void) {
    sIsPlanStarted = FALSE;
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

s8 GetGolferTraitUnk2(void) {
    return sGolferTraits[(s8)g_Players[sShot.playerIndex].characterIndex].unk2;
}

s32 IsStrokeLimitReached(void) {
    return g_Players[sShot.playerIndex].unk23 >= g_HolePar + D_8025C640_ovl_03 - 1;
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

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", PlanShot);

s32 GetBallCupDistance(void) {
    return IntLength2D(sShot.ballPosition[AXIS_X] - D_800BA9D8.cupPosition[AXIS_X],
                       sShot.ballPosition[AXIS_Z] - g_CupPositionZ);
}

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253D78_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253D7C_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253D8C_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253D90_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253DAC_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253DBC_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253DCC_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253DD8_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253E08_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253E14_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253E24_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253E38_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253E48_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253E84_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253E94_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253EA8_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253EBC_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80253EC8_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_8024F0B0_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_8024F320_ovl_03);

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

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_8024F844_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_802504A0_ovl_03);

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

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_802507D8_ovl_03);

#define UNK802510D0_VALUE_COUNT 16

void func_802510D0_ovl_03(s32 unused, s32 *plannedClub, s32 *targetX, s32 *targetZ, s32 *arg4) {
    s32 pad[44]; /* Unused; it is there to match */
    s32 values[UNK802510D0_VALUE_COUNT];
    s32 index;
    s32 one; /* one is there to match */
    s32 strokeCount;

    one = 1;
    for (index = UNK802510D0_VALUE_COUNT - 1; index >= 0; index--) {
        values[index] = one;
    }
    strokeCount = g_Players[sShot.playerIndex].unk23;
    *plannedClub = func_80252F70_ovl_03(func_8024F0B0_ovl_03(one));
    *targetX = D_800BA9D8.cupPosition[AXIS_X];
    *targetZ = g_CupPositionZ;
    *arg4 = values[strokeCount];
}

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80251198_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", FindGreenEdgeOnLine);

/*
 * Returns the distance from from to the first green edge on the line to to,
 * or to to itself if the line crosses no green edge.
 */
s32 GetGreenEdgeDistance(CoursePoint from, CoursePoint to) {
    CoursePoint edge;
    s32 distance;

    if (FindGreenEdgeOnLine(from, to, edge) != TRUE) {
        distance = IntLength2D(to[AXIS_X] - from[AXIS_X], to[AXIS_Z] - from[AXIS_Z]);
    } else {
        distance = IntLength2D(edge[AXIS_X] - from[AXIS_X], edge[AXIS_Z] - from[AXIS_Z]);
    }
    return distance;
}

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_802515DC_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_802517F0_ovl_03);

/* sShot.unk4's range, which ScaleToShotUnk4 maps 0 to 256 onto */
#define SHOT_UNK4_MIN 3
#define SHOT_UNK4_MAX 30
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

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80254154_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80251B20_ovl_03);

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

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80252F70_ovl_03);

/* Returns how far apart two binary angles are, up to half a turn */
s32 GetAngleDifference(s32 angle, s32 otherAngle) {
    s32 difference = (s16)(angle - otherAngle);

    if (difference < 0) {
        difference = -difference;
    }
    return difference;
}

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_8025319C_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80253574_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80253754_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80253978_ovl_03);
