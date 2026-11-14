/*
 * Simulating a shot from the ball's position and recording its path.
 */

#ifndef SHOT_SIMULATION_H
#define SHOT_SIMULATION_H

#include <ultra64.h>

#include "shot/ball_simulation.h"
#include "system/c_language.h"

// Steps past this aren't kept, though the simulation goes on
#define PATH_STEP_COUNT 900

#define NO_STEP (-1)

// The shot as ovl_04 simulates it ahead of play, with the path it records
typedef struct SimulatedShot {
    s32 startPosition[AXIS_COUNT];
    s32 endPosition[AXIS_COUNT];
    s32 path[PATH_STEP_COUNT][AXIS_COUNT];
    // The STEP_FLAG_* bits that StepBallSimulation gave each step
    StepFlags8 pathFlags[PATH_STEP_COUNT];
    s32 stepCount;
    s32 positionMax[AXIS_COUNT]; // Over the kept steps
    s32 positionStepMax[AXIS_COUNT];
    s32 positionMin[AXIS_COUNT];
    s32 positionStepMin[AXIS_COUNT];
    Boolean isFirstBounceOut; // Out of bounds or off the course
    Boolean endsOnGreen;
    Boolean startsOnGreen; // Always, in mini-golf
    Boolean usesPutter;
    Boolean endsInCup;
    // A full-power shot on the impact mark, with any club but a putter
    Boolean isPerfectSwing;
    Surface firstBounceSurface; // As the simulation's; never read
    ShotLaunch shot;
    s32 flagHitStep; // The step that hits the flag, or NO_STEP
    Surface8 endSurface; // A SURFACE_* value
    Boolean8 isWaterCut; // See SHOT_KIND_WATER_CUT
    s8 impactRumbles; // As its ImpactEffect's rumbles
} SimulatedShot;

extern SimulatedShot g_SimulatedShot;

void SimulateShot(ShotLaunch *shot);

#endif
