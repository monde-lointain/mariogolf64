#ifndef SHOT_SIMULATION_H
#define SHOT_SIMULATION_H

#include <ultra64.h>

#include "ball_simulation.h"
#include "language.h"

#define PATH_STEP_COUNT 900

#define NO_STEP (-1)

typedef struct SimulatedShot {
    s32 startPosition[3];
    s32 endPosition[3];
    s32 path[PATH_STEP_COUNT][3];
    StepFlags8 pathFlags[PATH_STEP_COUNT];
    s32 stepCount;
    s32 positionMax[3];
    s32 positionStepMax[3];
    s32 positionMin[3];
    s32 positionStepMin[3];
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
