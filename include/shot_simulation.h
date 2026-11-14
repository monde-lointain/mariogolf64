#ifndef SHOT_SIMULATION_H
#define SHOT_SIMULATION_H

#include <ultra64.h>

#include "ball_simulation.h"
#include "c_language.h"

#define PATH_STEP_COUNT 900

#define NO_STEP (-1)

typedef struct SimulatedShot {
    s32 startPosition[AXIS_COUNT];
    s32 endPosition[AXIS_COUNT];
    s32 path[PATH_STEP_COUNT][AXIS_COUNT];
    StepFlags8 pathFlags[PATH_STEP_COUNT];
    s32 stepCount;
    s32 positionMax[AXIS_COUNT];
    s32 positionStepMax[AXIS_COUNT];
    s32 positionMin[AXIS_COUNT];
    s32 positionStepMin[AXIS_COUNT];
    Boolean isFirstBounceOut;
    Boolean endsOnGreen;
    Boolean startsOnGreen;
    Boolean usesPutter;
    Boolean endsInCup;
    Boolean isPerfectSwing;
    Surface firstBounceSurface;
    ShotLaunch shot;
    s32 flagHitStep;
    Surface8 endSurface;
    Boolean8 isWaterCut;
    s8 impactRumbles;
} SimulatedShot;

extern SimulatedShot g_SimulatedShot;

void SimulateShot(ShotLaunch *shot);

#endif
