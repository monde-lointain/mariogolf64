#include "common.h"
#include "course.h"
#include "current_shot.h"
#include "functions.h"
#include "math_util.h"
#include "save.h"

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

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_8024F070_ovl_03);

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

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_8024F688_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_8024F790_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_8024F844_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_802504A0_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80250768_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_802507D8_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_802510D0_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80251198_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80251378_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80251570_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_802515DC_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_802517F0_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80251AC0_ovl_03);

INCLUDE_RODATA("asm/nonmatchings/ovl_03/computer_golfer", D_80254154_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80251B20_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80252EE4_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80252F70_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80253180_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_8025319C_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80253574_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80253754_ovl_03);

INCLUDE_ASM("asm/nonmatchings/ovl_03/computer_golfer", func_80253978_ovl_03);
