#include "club.h"
#include "common.h"
#include "course.h"
#include "current_shot.h"
#include "functions.h"
#include "libc.h"
#include "math_util.h"
#include "save.h"
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
    /* 0x6 */ s8 unk6[0xE - 0x6];
} GolferTraits; /* size = 0xE */

extern GolferTraits sGolferTraits[CHARACTER_COUNT];
extern s32 sPlanThreadState;
extern OSThread sPlanThread;
extern CurrentShot sPlannedShot;
extern u8 sPlanThreadStackEnd[];
extern s32 D_8025C640_ovl_03;
/* Where the planned shot aims */
extern CoursePoint sPlanTarget;
extern s8 sIsPlanStarted;
extern s8 sIsClubUsable[CLUB_SLOT_COUNT];

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
    *plannedClub = ChooseClubForDistance(PlanCupDistance(one));
    *targetX = D_800BA9D8.cupPosition[AXIS_X];
    *targetZ = g_CupPositionZ;
    *arg4 = values[strokeCount];
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

/* GolferTraits.windPlay values */
#define WIND_PLAY_HEAD_WINDS 0      /* Offsets shots for head winds */
#define WIND_PLAY_STRONG_WINDS 1    /* Offsets shots for the strongest winds either way */
#define WIND_PLAY_FOLLOWING_WINDS 2 /* Offsets shots for following winds */

/* ChooseShotOffset's results, indices into the shot offset table */
#define SHOT_OFFSET_NONE 0
#define SHOT_OFFSET_PLUS_SMALL 1
#define SHOT_OFFSET_PLUS_MEDIUM 2
#define SHOT_OFFSET_PLUS_LARGE 3
#define SHOT_OFFSET_MINUS_SMALL 4
#define SHOT_OFFSET_MINUS_MEDIUM 5
#define SHOT_OFFSET_MINUS_LARGE 6

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

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80253978_ovl_03);
