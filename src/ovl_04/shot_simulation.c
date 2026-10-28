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

/* Not static: the original left it to the linker's common block */
SimulatedShot g_SimulatedShot;

/* Simulates the shot from the ball's position and records its path */
void SimulateShot(Unk20* shot) {
  BallSimulation simulationState;
  BallSimulation* simulation;
  s32 step;
  Boolean hasGroundHit;
  Boolean hasUnk40Flag;
  s32 flags;
  s32 unkA7Kind;
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
  g_SimulatedShot.unkB0Step = NO_STEP;

  /* The chained assignment is there to match */
  g_SimulatedShot.maxPosition[0] = g_SimulatedShot.minPosition[0] =
      g_SimulatedShot.startPosition[0] = g_CurrentShot.ballPosition[0];
  g_SimulatedShot.startPosition[1] = g_CurrentShot.ballPosition[1];
  g_SimulatedShot.minPosition[1] = g_SimulatedShot.startPosition[1];
  g_SimulatedShot.maxPosition[1] = g_SimulatedShot.minPosition[1];
  g_SimulatedShot.startPosition[2] = g_CurrentShot.ballPosition[2];
  g_SimulatedShot.minPosition[2] = g_SimulatedShot.startPosition[2];
  g_SimulatedShot.maxPosition[2] = g_SimulatedShot.minPosition[2];
  simulation->position[0] = g_CurrentShot.ballPosition[0];
  simulation->position[1] = g_CurrentShot.ballPosition[1];
  simulation->position[2] = g_CurrentShot.ballPosition[2];
  simulation->surface = g_CurrentShot.surface;
  hasGroundHit = FALSE;
  hasUnk40Flag = FALSE;
  g_SimulatedShot.unk2E0C =
      GetClub(shot->clubIndex)->surfaceMask & CLUB_SURFACE_GREEN;
  g_SimulatedShot.unk2E14 = shot->unkC == 0 &&
      shot->power >= MAX_LAUNCH_POWER &&
      !(GetClub(shot->clubIndex)->surfaceMask & CLUB_SURFACE_GREEN);
  g_SimulatedShot.unk2E42 = GetUnkCEntry(
      GetClub(
          g_Players[g_CurrentShot.playerIndex].clubs[g_CurrentShot.clubSlot]),
      g_CurrentShot.surface, g_CurrentShot.shotMode, shot->unkC)
                                ->unk5;
  LaunchBallSimulation(simulation, LAUNCH_PURPOSE_TRIAL, shot);
  g_SimulatedShot.unk2E08 = g_GameState.isMiniGolf ||
      GetSurfaceInfo(g_CurrentShot.surface)->kind == SURFACE_KIND_GREEN;
  step = 0;
  while (simulation->isRunning) {
    flags = StepBallSimulation(simulation);
    if (step < PATH_STEP_COUNT) {
      g_SimulatedShot.pathFlags[step] = flags;
      g_SimulatedShot.path[step][0] = simulation->position[0];
      g_SimulatedShot.path[step][1] = simulation->position[1];
      g_SimulatedShot.path[step][2] = simulation->position[2];
    }

    if (simulation->unkB0 && g_SimulatedShot.unkB0Step == NO_STEP) {
      g_SimulatedShot.unkB0Step = step;
    }

    if (flags & STEP_FLAG_GROUND_HIT & !hasGroundHit) {
      hasGroundHit = TRUE;
    }

    if (flags & STEP_FLAG_HOLE_TRIANGLE_BOUNCE) {
      hasUnk40Flag = TRUE;
    }

    if (step < PATH_STEP_COUNT) {
      if (simulation->position[0] > g_SimulatedShot.maxPosition[0]) {
        g_SimulatedShot.maxPosition[0] = simulation->position[0];
        g_SimulatedShot.maxPositionStep[0] = step;
      }

      if (simulation->position[0] < g_SimulatedShot.minPosition[0]) {
        g_SimulatedShot.minPosition[0] = simulation->position[0];
        g_SimulatedShot.minPositionStep[0] = step;
      }

      if (simulation->position[1] > g_SimulatedShot.maxPosition[1]) {
        g_SimulatedShot.maxPosition[1] = simulation->position[1];
        g_SimulatedShot.maxPositionStep[1] = step;
      }

      if (simulation->position[1] < g_SimulatedShot.minPosition[1]) {
        g_SimulatedShot.minPosition[1] = simulation->position[1];
        g_SimulatedShot.minPositionStep[1] = step;
      }

      if (simulation->position[2] > g_SimulatedShot.maxPosition[2]) {
        g_SimulatedShot.maxPosition[2] = simulation->position[2];
        g_SimulatedShot.maxPositionStep[2] = step;
      }

      if (simulation->position[2] < g_SimulatedShot.minPosition[2]) {
        g_SimulatedShot.minPosition[2] = simulation->position[2];
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
  g_SimulatedShot.endPosition[0] = simulation->position[0];
  g_SimulatedShot.endPosition[1] = simulation->position[1];
  g_SimulatedShot.endPosition[2] = simulation->position[2];
  g_SimulatedShot.unk2E10 = simulation->unkA0;
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
  unkA7Kind = GetSurfaceInfo(simulation->unkA7)->kind;
  if ((unkA7Kind == SURFACE_KIND_OUT_OF_BOUNDS) |
      (unkA7Kind == SURFACE_KIND_OUT_OF_COURSE)) {
    g_SimulatedShot.unk2E00 = TRUE;
  } else {
    g_SimulatedShot.unk2E00 = FALSE;
  }

  g_SimulatedShot.unk2E18 = simulation->unkA7;
}
