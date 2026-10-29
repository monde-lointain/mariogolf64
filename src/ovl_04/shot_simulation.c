/*
 * Simulates the shot from the ball's position and records its path.
 */
#include "common.h"

#include "shot_simulation.h"

#include "ball_simulation.h"
#include "billboard.h"
#include "club.h"
#include "current_shot.h"
#include "debug_text.h"
#include "functions.h"
#include "save.h"
#include "surface.h"
#include "variables.h"

/* Read by no routine */
static u8 D_8024DD10_ovl_04[0x10];
/* Not static: the original left it to the linker's common block */
SimulatedShot g_SimulatedShot;

/* Simulates the shot from the ball's position and records its path */
void SimulateShot(ShotLaunch* shot) {
  BallSimulation simulationState;
  BallSimulation* simulation;
  s32 step;
  Boolean hasGroundHit;
  Boolean hasUnk40Flag;
  s32 flags;
  s32 firstBounceKind;
  u8 isUnk2E41;

  simulation = &simulationState;
  g_SimulatedShot.shot = *shot;
  g_SimulatedShot.stepCount = 0;
  g_SimulatedShot.maxPositionStep[0] = 0;
  g_SimulatedShot.maxPositionStep[1] = 0;
  g_SimulatedShot.maxPositionStep[2] = 0;
  g_SimulatedShot.minPositionStep[0] = 0;
  g_SimulatedShot.minPositionStep[1] = 0;
  g_SimulatedShot.minPositionStep[2] = 0;
  g_SimulatedShot.flagHitStep = NO_STEP;

  /* The chained assignment is there to match */
  g_SimulatedShot.maxPosition[AXIS_X] = g_SimulatedShot.minPosition[AXIS_X] =
      g_SimulatedShot.startPosition[AXIS_X] =
          g_CurrentShot.ballPosition[AXIS_X];
  g_SimulatedShot.startPosition[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
  g_SimulatedShot.minPosition[AXIS_Y] = g_SimulatedShot.startPosition[AXIS_Y];
  g_SimulatedShot.maxPosition[AXIS_Y] = g_SimulatedShot.minPosition[AXIS_Y];
  g_SimulatedShot.startPosition[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
  g_SimulatedShot.minPosition[AXIS_Z] = g_SimulatedShot.startPosition[AXIS_Z];
  g_SimulatedShot.maxPosition[AXIS_Z] = g_SimulatedShot.minPosition[AXIS_Z];
  simulation->position[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
  simulation->position[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
  simulation->position[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
  simulation->surface = g_CurrentShot.surface;
  hasGroundHit = FALSE;
  hasUnk40Flag = FALSE;
  g_SimulatedShot.usesPutter =
      GetClub(shot->clubIndex)->surfaceMask & CLUB_SURFACE_GREEN;
  g_SimulatedShot.unk2E14 = shot->impactTiming == 0 &&
      shot->power >= MAX_LAUNCH_POWER &&
      !(GetClub(shot->clubIndex)->surfaceMask & CLUB_SURFACE_GREEN);
  g_SimulatedShot.unk2E42 = GetImpactTimingEntry(
      GetClub(
          g_Players[g_CurrentShot.playerIndex].clubs[g_CurrentShot.clubSlot]),
      g_CurrentShot.surface, g_CurrentShot.shotMode, shot->impactTiming)
                                ->rumbles;
  LaunchBallSimulation(simulation, LAUNCH_PURPOSE_TRIAL, shot);
  g_SimulatedShot.startsOnGreen = g_GameState.isMiniGolf ||
      GetSurfaceInfo(g_CurrentShot.surface)->kind == SURFACE_KIND_GREEN;
  step = 0;
  while (simulation->state) {
    flags = StepBallSimulation(simulation);
    if (step < PATH_STEP_COUNT) {
      g_SimulatedShot.pathFlags[step] = flags;
      g_SimulatedShot.path[step][0] = simulation->position[AXIS_X];
      g_SimulatedShot.path[step][1] = simulation->position[AXIS_Y];
      g_SimulatedShot.path[step][2] = simulation->position[AXIS_Z];
    }

    if (simulation->hasHitFlag && g_SimulatedShot.flagHitStep == NO_STEP) {
      g_SimulatedShot.flagHitStep = step;
    }

    if (flags & STEP_FLAG_GROUND_HIT & !hasGroundHit) {
      hasGroundHit = TRUE;
    }

    if (flags & STEP_FLAG_HOLE_TRIANGLE_BOUNCE) {
      hasUnk40Flag = TRUE;
    }

    if (step < PATH_STEP_COUNT) {
      if (simulation->position[AXIS_X] > g_SimulatedShot.maxPosition[AXIS_X]) {
        g_SimulatedShot.maxPosition[AXIS_X] = simulation->position[AXIS_X];
        g_SimulatedShot.maxPositionStep[0] = step;
      }

      if (simulation->position[AXIS_X] < g_SimulatedShot.minPosition[AXIS_X]) {
        g_SimulatedShot.minPosition[AXIS_X] = simulation->position[AXIS_X];
        g_SimulatedShot.minPositionStep[0] = step;
      }

      if (simulation->position[AXIS_Y] > g_SimulatedShot.maxPosition[AXIS_Y]) {
        g_SimulatedShot.maxPosition[AXIS_Y] = simulation->position[AXIS_Y];
        g_SimulatedShot.maxPositionStep[1] = step;
      }

      if (simulation->position[AXIS_Y] < g_SimulatedShot.minPosition[AXIS_Y]) {
        g_SimulatedShot.minPosition[AXIS_Y] = simulation->position[AXIS_Y];
        g_SimulatedShot.minPositionStep[1] = step;
      }

      if (simulation->position[AXIS_Z] > g_SimulatedShot.maxPosition[AXIS_Z]) {
        g_SimulatedShot.maxPosition[AXIS_Z] = simulation->position[AXIS_Z];
        g_SimulatedShot.maxPositionStep[2] = step;
      }

      if (simulation->position[AXIS_Z] < g_SimulatedShot.minPosition[AXIS_Z]) {
        g_SimulatedShot.minPosition[AXIS_Z] = simulation->position[AXIS_Z];
        g_SimulatedShot.minPositionStep[2] = step;
      }
    }

    step++;
    if (step >= MAX_STEP_COUNT) {
      SetDebugTextHighlight(1);
      PrintDebugText("Sim Fault!", 10, 12);
      break;
    }
  }

  g_SimulatedShot.stepCount = step;
  g_SimulatedShot.endPosition[AXIS_X] = simulation->position[AXIS_X];
  g_SimulatedShot.endPosition[AXIS_Y] = simulation->position[AXIS_Y];
  g_SimulatedShot.endPosition[AXIS_Z] = simulation->position[AXIS_Z];
  g_SimulatedShot.endsInCup = simulation->isHoledOut;
  g_SimulatedShot.endSurface = simulation->surface;
  g_SimulatedShot.endsOnGreen =
      GetSurfaceInfo(simulation->surface)->kind == SURFACE_KIND_GREEN;
  isUnk2E41 = FALSE;
  if (hasUnk40Flag) {
    isUnk2E41 =
        GetSurfaceInfo(simulation->surface)->kind != SURFACE_KIND_WATER_HAZARD;
  }

  g_SimulatedShot.unk2E41 = isUnk2E41;
  osSyncPrintf("\nWaterCut%d\n", isUnk2E41);
  firstBounceKind = GetSurfaceInfo(simulation->firstBounceSurface)->kind;
  if ((firstBounceKind == SURFACE_KIND_OUT_OF_BOUNDS) |
      (firstBounceKind == SURFACE_KIND_OUT_OF_COURSE)) {
    g_SimulatedShot.unk2E00 = TRUE;
  } else {
    g_SimulatedShot.unk2E00 = FALSE;
  }

  g_SimulatedShot.unk2E18 = simulation->firstBounceSurface;
}
