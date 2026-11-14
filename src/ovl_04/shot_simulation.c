#include "shot_simulation.h"

#include <ultra64.h>

#include "ball_simulation.h"
#include "club.h"
#include "compiler.h"
#include "current_shot.h"
#include "debug_text.h"
#include "game_state.h"
#include "geometry.h"
#include "language.h"
#include "player.h"
#include "surface.h"

static u8 f_UnusedBytesBeforeSimulatedShot[0x10] UNUSED;
SimulatedShot g_SimulatedShot;

void SimulateShot(ShotLaunch *shot) {
    BallSimulation simulationState;
    BallSimulation *simulation;
    s32 step;
    Boolean hasGroundHit;
    Boolean hasHoleTriangleBounce;
    StepFlags stepFlags;
    SurfaceKind firstBounceKind;
    UBoolean8 isWaterCut;

    simulation = &simulationState;
    g_SimulatedShot.shot = *shot;
    g_SimulatedShot.stepCount = 0;
    g_SimulatedShot.positionStepMax[AXIS_X] = 0;
    g_SimulatedShot.positionStepMax[AXIS_Y] = 0;
    g_SimulatedShot.positionStepMax[AXIS_Z] = 0;
    g_SimulatedShot.positionStepMin[AXIS_X] = 0;
    g_SimulatedShot.positionStepMin[AXIS_Y] = 0;
    g_SimulatedShot.positionStepMin[AXIS_Z] = 0;
    g_SimulatedShot.flagHitStep = NO_STEP;

    g_SimulatedShot.positionMax[AXIS_X] = g_SimulatedShot.positionMin[AXIS_X] =
        g_SimulatedShot.startPosition[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
    g_SimulatedShot.positionMax[AXIS_Y] = g_SimulatedShot.positionMin[AXIS_Y] =
        g_SimulatedShot.startPosition[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
    g_SimulatedShot.positionMax[AXIS_Z] = g_SimulatedShot.positionMin[AXIS_Z] =
        g_SimulatedShot.startPosition[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
    simulation->position[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
    simulation->position[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
    simulation->position[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
    simulation->surface = g_CurrentShot.surface;
    hasGroundHit = FALSE;
    hasHoleTriangleBounce = FALSE;
    g_SimulatedShot.usesPutter = GetClub(shot->clubSlot)->surfaceMask & CLUB_SURFACE_GREEN;
    g_SimulatedShot.isPerfectSwing = shot->impactTiming == 0 &&
        shot->power >= LAUNCH_POWER_MAX &&
        !(GetClub(shot->clubSlot)->surfaceMask & CLUB_SURFACE_GREEN);
    g_SimulatedShot.impactRumbles = GetImpactTimingEntry(
        GetClub(g_Players[g_CurrentShot.playerIndex].clubs[g_CurrentShot.clubSlot]),
        g_CurrentShot.surface,
        g_CurrentShot.shotMode,
        shot->impactTiming
    )
                                        ->rumbles;
    LaunchBallSimulation(simulation, LAUNCH_PURPOSE_TRIAL, shot);
    g_SimulatedShot.startsOnGreen = g_GameState.isMiniGolf ||
        GetSurfaceTraits(g_CurrentShot.surface)->kind == SURFACE_KIND_GREEN;
    step = 0;
    while (simulation->state != BALL_STATE_STOPPED) {
        stepFlags = StepBallSimulation(simulation);
        if (step < PATH_STEP_COUNT) {
            g_SimulatedShot.pathFlags[step] = stepFlags;
            g_SimulatedShot.path[step][0] = simulation->position[AXIS_X];
            g_SimulatedShot.path[step][1] = simulation->position[AXIS_Y];
            g_SimulatedShot.path[step][2] = simulation->position[AXIS_Z];
        }

        if (simulation->hasHitFlag && g_SimulatedShot.flagHitStep == NO_STEP) {
            g_SimulatedShot.flagHitStep = step;
        }

        if (stepFlags & STEP_FLAG_GROUND_HIT & !hasGroundHit) {
            hasGroundHit = TRUE;
        }

        if (stepFlags & STEP_FLAG_HOLE_TRIANGLE_BOUNCE) {
            hasHoleTriangleBounce = TRUE;
        }

        if (step < PATH_STEP_COUNT) {
            if (simulation->position[AXIS_X] > g_SimulatedShot.positionMax[AXIS_X]) {
                g_SimulatedShot.positionMax[AXIS_X] = simulation->position[AXIS_X];
                g_SimulatedShot.positionStepMax[AXIS_X] = step;
            }

            if (simulation->position[AXIS_X] < g_SimulatedShot.positionMin[AXIS_X]) {
                g_SimulatedShot.positionMin[AXIS_X] = simulation->position[AXIS_X];
                g_SimulatedShot.positionStepMin[AXIS_X] = step;
            }

            if (simulation->position[AXIS_Y] > g_SimulatedShot.positionMax[AXIS_Y]) {
                g_SimulatedShot.positionMax[AXIS_Y] = simulation->position[AXIS_Y];
                g_SimulatedShot.positionStepMax[AXIS_Y] = step;
            }

            if (simulation->position[AXIS_Y] < g_SimulatedShot.positionMin[AXIS_Y]) {
                g_SimulatedShot.positionMin[AXIS_Y] = simulation->position[AXIS_Y];
                g_SimulatedShot.positionStepMin[AXIS_Y] = step;
            }

            if (simulation->position[AXIS_Z] > g_SimulatedShot.positionMax[AXIS_Z]) {
                g_SimulatedShot.positionMax[AXIS_Z] = simulation->position[AXIS_Z];
                g_SimulatedShot.positionStepMax[AXIS_Z] = step;
            }

            if (simulation->position[AXIS_Z] < g_SimulatedShot.positionMin[AXIS_Z]) {
                g_SimulatedShot.positionMin[AXIS_Z] = simulation->position[AXIS_Z];
                g_SimulatedShot.positionStepMin[AXIS_Z] = step;
            }
        }

        step++;
        if (step >= STEP_COUNT_MAX) {
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
    g_SimulatedShot.endsOnGreen = GetSurfaceTraits(simulation->surface)->kind == SURFACE_KIND_GREEN;
    isWaterCut = FALSE;
    if (hasHoleTriangleBounce) {
        isWaterCut = GetSurfaceTraits(simulation->surface)->kind != SURFACE_KIND_WATER_HAZARD;
    }

    g_SimulatedShot.isWaterCut = isWaterCut;
    osSyncPrintf("\nWaterCut%d\n", isWaterCut);
    firstBounceKind = GetSurfaceTraits(simulation->firstBounceSurface)->kind;
    if ((firstBounceKind == SURFACE_KIND_OUT_OF_BOUNDS) |
        (firstBounceKind == SURFACE_KIND_OUT_OF_COURSE)) {
        g_SimulatedShot.isFirstBounceOut = TRUE;
    }
    else {
        g_SimulatedShot.isFirstBounceOut = FALSE;
    }

    g_SimulatedShot.firstBounceSurface = simulation->firstBounceSurface;
}
