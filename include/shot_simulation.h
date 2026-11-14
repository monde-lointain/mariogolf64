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
      s8 pathFlags[PATH_STEP_COUNT];
      s32 stepCount;
      s32 maxPosition[3];
      s32 maxPositionStep[3];
      s32 minPosition[3];
      s32 minPositionStep[3];
      Boolean isFirstBounceOut;
      s32 endsOnGreen;
      Boolean startsOnGreen;
      Boolean usesPutter;
      Boolean endsInCup;
      Boolean isPerfectSwing;
      s32 firstBounceSurface;
      ShotLaunch shot;
      s32 flagHitStep;
      s8 endSurface;
      s8 isWaterCut;
      s8 impactRumbles;
} SimulatedShot;

extern SimulatedShot g_SimulatedShot;

void SimulateShot(ShotLaunch *shot);

#endif
