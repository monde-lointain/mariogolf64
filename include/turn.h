#ifndef TURN_H
#define TURN_H

#include <ultra64.h>

#include "ball_simulation.h"

/*
 * The kinds of notable shot that earn Course Points. Most are named after
 * the banner that the shot shows.
 */
/* Onto the green in three or more under par */
#define SHOT_KIND_GREAT 0
/* Close to the cup, on the green in two under par */
#define SHOT_KIND_EXCELLENT 1
#define SHOT_KIND_PIN_SHOT 2 /* Hits the pin */
#define SHOT_KIND_CHIP_IN 3 /* Holed from off the green */
#define SHOT_KIND_LONG_CHIP_IN 4 /* Holed from far off the green */
#define SHOT_KIND_UNK5 5
#define SHOT_KIND_FLAG_SHOT 6 /* Hits the flag */
#define SHOT_KIND_HOLE_IN_ONE 7
#define SHOT_KIND_ALBATROSS 8
#define SHOT_KIND_EAGLE 9
#define SHOT_KIND_BIRDIE 10
/* Crosses water without stopping in it, the debug text's "WaterCut" */
#define SHOT_KIND_WATER_CUT 11

/* The ball of the player whose turn it is, as PlaceBallAtRest sets it */
extern BallSimulation g_TurnBallSimulation;

void CountNotableShot(s32 playerIndex, s32 shotKind);
void StartFreeLook(void);
void PlaceBallAtRest(s32 playerIndex, BallSimulation* simulation);

#endif
