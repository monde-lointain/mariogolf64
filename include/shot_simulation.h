#ifndef SHOT_SIMULATION_H
#define SHOT_SIMULATION_H

#include "common.h"

#include "ball_simulation.h"

/* Steps past this aren't kept, though the simulation goes on */
#define PATH_STEP_COUNT 900

#define NO_STEP (-1)

/* The shot as ovl_04 simulates it ahead of play, with the path it records */
typedef struct SimulatedShot {
  /* 0x0000 */ s32 startPosition[3];
  /* 0x000C */ s32 endPosition[3];
  /* 0x0018 */ s32 path[PATH_STEP_COUNT][3];
  /* The STEP_FLAG_* bits that StepBallSimulation gave each step */
  /* 0x2A48 */ s8 pathFlags[PATH_STEP_COUNT];
  /* 0x2DCC */ s32 stepCount;
  /* 0x2DD0 */ s32 maxPosition[3]; /* Over the kept steps */
  /* 0x2DDC */ s32 maxPositionStep[3];
  /* 0x2DE8 */ s32 minPosition[3];
  /* 0x2DF4 */ s32 minPositionStep[3];
  /* 0x2E00 */ s32 unk2E00;
  /* 0x2E04 */ s32 endsOnGreen;
  /* 0x2E08 */ Boolean startsOnGreen; /* Always, in mini-golf */
  /* 0x2E0C */ Boolean usesPutter;
  /* 0x2E10 */ Boolean endsInCup;
  /* A full-power shot on the impact mark, with any club but a putter */
  /* 0x2E14 */ Boolean isPerfectSwing;
  /* 0x2E18 */ s32 unk2E18;
  /* 0x2E1C */ ShotLaunch shot;
  /* 0x2E3C */ s32 flagHitStep; /* The step that hits the flag, or NO_STEP */
  /* 0x2E40 */ s8 endSurface; /* A SURFACE_* value */
  /* 0x2E41 */ s8 unk2E41;
  /* 0x2E42 */ s8 unk2E42;
} SimulatedShot; /* size = 0x2E44 */

extern SimulatedShot g_SimulatedShot;

void SimulateShot(ShotLaunch* shot);

#endif
