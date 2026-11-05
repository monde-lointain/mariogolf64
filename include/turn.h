#ifndef TURN_H
#define TURN_H

#include <ultra64.h>

#include "ball_simulation.h"

/*
 * The kinds of notable shot that earn Course Points. Most are named after
 * the banner that the shot shows.
 */
/* Onto the green in three or more under par */
enum ShotKind {
  SHOT_KIND_GREAT,
  /* Close to the cup, on the green in two under par */
  SHOT_KIND_EXCELLENT,
  SHOT_KIND_PIN_SHOT, /* Hits the pin */
  SHOT_KIND_CHIP_IN, /* Holed from off the green */
  SHOT_KIND_LONG_CHIP_IN, /* Holed from far off the green */
  SHOT_KIND_UNK5,
  SHOT_KIND_FLAG_SHOT, /* Hits the flag */
  SHOT_KIND_HOLE_IN_ONE,
  SHOT_KIND_ALBATROSS,
  SHOT_KIND_EAGLE,
  SHOT_KIND_BIRDIE,
  /* Crosses water without stopping in it, the debug text's "WaterCut" */
  SHOT_KIND_WATER_CUT,
  SHOT_KIND_COUNT
};

/* The ball of the player whose turn it is, as PlaceBallAtRest sets it */
extern BallSimulation g_TurnBallSimulation;

void CountNotableShot(s32 playerIndex, s32 shotKind);
void StartFreeLook(void);
void PlaceBallAtRest(s32 playerIndex, BallSimulation* simulation);

#endif
