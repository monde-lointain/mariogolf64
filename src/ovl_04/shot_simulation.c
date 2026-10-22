#include "common.h"
#include "ball_simulation.h"
#include "billboard.h"
#include "club.h"
#include "current_shot.h"
#include "debug_text.h"
#include "functions.h"
#include "save.h"
#include "surface.h"
#include "variables.h"

/* Steps past this aren't kept, though the simulation goes on */
#define PATH_STEP_COUNT 900

/* A simulation still going after this many steps is given up on */
#define MAX_STEP_COUNT 1801

/* Bits of the flags func_8003327C returns for each step */
#define STEP_FLAG_UNK1 0x1
#define STEP_FLAG_UNK40 0x40

#define NO_STEP (-1)

typedef struct SimulatedShot {
    /* 0x0000 */ s32 startPosition[3];
    /* 0x000C */ s32 endPosition[3];
    /* 0x0018 */ s32 path[PATH_STEP_COUNT][3];
    /* 0x2A48 */ u8 pathFlags[PATH_STEP_COUNT];
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
    /* 0x2E41 */ u8 unk2E41;
    /* 0x2E42 */ u8 unk2E42;
} SimulatedShot; /* size = 0x2E44 */

extern SimulatedShot g_SimulatedShot;

Club *func_80037E50(Club *, s32, s32, s32);

/* Simulates the shot from the ball's position and records its path */
void SimulateShot(Unk20 *shot) {
    BallSimulation simulationState;
    BallSimulation *simulation;
    s32 step;
    s32 hasUnk1Flag;
    s32 hasUnk40Flag;
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
    g_SimulatedShot.maxPosition[0] = g_SimulatedShot.minPosition[0] = g_SimulatedShot.startPosition[0] =
        g_CurrentShot.ballPosition[0];
    g_SimulatedShot.maxPosition[1] = g_SimulatedShot.minPosition[1] = g_SimulatedShot.startPosition[1] =
        g_CurrentShot.ballPosition[1];
    g_SimulatedShot.maxPosition[2] = g_SimulatedShot.minPosition[2] = g_SimulatedShot.startPosition[2] =
        g_CurrentShot.ballPosition[2];
    simulation->position[0] = g_CurrentShot.ballPosition[0];
    simulation->position[1] = g_CurrentShot.ballPosition[1];
    simulation->position[2] = g_CurrentShot.ballPosition[2];
    simulation->surface = g_CurrentShot.surface;
    hasUnk1Flag = FALSE;
    hasUnk40Flag = FALSE;
    g_SimulatedShot.unk2E0C = GetClub(shot->clubIndex)->surfaceMask & CLUB_SURFACE_GREEN;
    g_SimulatedShot.unk2E14 =
        shot->unkC == 0 && shot->unk8 >= 0x100 && !(GetClub(shot->clubIndex)->surfaceMask & CLUB_SURFACE_GREEN);
    g_SimulatedShot.unk2E42 =
        func_80037E50(GetClub(g_Players[g_CurrentShot.playerIndex].clubs[g_CurrentShot.clubSlot]), g_CurrentShot.surface, g_CurrentShot.shotMode, shot->unkC)
            ->unk5;
    func_80037F94(simulation, 2, shot);
    g_SimulatedShot.unk2E08 = g_ShowsBillboardsAtEdge || GetSurfaceInfo(g_CurrentShot.surface)->kind == SURFACE_KIND_GREEN;
    step = 0;
    while (simulation->isRunning) {
        flags = func_8003327C(simulation);
        if (step < PATH_STEP_COUNT) {
            g_SimulatedShot.pathFlags[step] = flags;
            g_SimulatedShot.path[step][0] = simulation->position[0];
            g_SimulatedShot.path[step][1] = simulation->position[1];
            g_SimulatedShot.path[step][2] = simulation->position[2];
        }
        if (simulation->unkB0 && g_SimulatedShot.unkB0Step == NO_STEP) {
            g_SimulatedShot.unkB0Step = step;
        }
        if (flags & STEP_FLAG_UNK1 & !hasUnk1Flag) {
            hasUnk1Flag = TRUE;
        }
        if (flags & STEP_FLAG_UNK40) {
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
    g_SimulatedShot.endsOnGreen = GetSurfaceInfo(simulation->surface)->kind == SURFACE_KIND_GREEN;
    isUnk2E41 = FALSE;
    if (hasUnk40Flag) {
        isUnk2E41 = GetSurfaceInfo(simulation->surface)->kind != SURFACE_KIND_WATER_HAZARD;
    }
    g_SimulatedShot.unk2E41 = isUnk2E41;
    osSyncPrintf("\nWaterCut%d\n", isUnk2E41);
    unkA7Kind = GetSurfaceInfo(simulation->unkA7)->kind;
    if ((unkA7Kind == SURFACE_KIND_OUT_OF_BOUNDS) | (unkA7Kind == SURFACE_KIND_OUT_OF_COURSE)) {
        g_SimulatedShot.unk2E00 = TRUE;
    } else {
        g_SimulatedShot.unk2E00 = FALSE;
    }
    g_SimulatedShot.unk2E18 = simulation->unkA7;
}
