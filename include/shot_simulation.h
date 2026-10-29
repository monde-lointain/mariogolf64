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
  /* 0x2A48 */ s8 pathFlags[PATH_STEP_COUNT];
  /* 0x2DCC */ s32 stepCount;
  /* 0x2DD0 */ s32 maxPosition[3]; /* Over the kept steps */
  /* 0x2DDC */ s32 maxPositionStep[3];
  /* 0x2DE8 */ s32 minPosition[3];
  /* 0x2DF4 */ s32 minPositionStep[3];
  /* 0x2E00 */ s32 unk2E00;
  /* 0x2E04 */ s32 endsOnGreen;
  /* 0x2E08 */ s32 unk2E08;
  /* 0x2E0C */ s32 unk2E0C;
  /* 0x2E10 */ s32 unk2E10;
  /* 0x2E14 */ s32 unk2E14;
  /* 0x2E18 */ s32 unk2E18;
  /* 0x2E1C */ Unk20 shot;
  /* 0x2E3C */ s32 unkB0Step; /* The first step with unkB0 set, or NO_STEP */
  /* 0x2E40 */ s8 endSurface;
  /* 0x2E41 */ s8 unk2E41;
  /* 0x2E42 */ s8 unk2E42;
} SimulatedShot; /* size = 0x2E44 */

extern SimulatedShot g_SimulatedShot;

void SimulateShot(Unk20* shot);

#endif
