#ifndef TURN_H
#define TURN_H

#include <ultra64.h>

#include "ball_simulation.h"
#include "camera.h"

enum NotableShot {
    NOTABLE_SHOT_GREAT,
    NOTABLE_SHOT_EXCELLENT,
    NOTABLE_SHOT_PIN_SHOT,
    NOTABLE_SHOT_CHIP_IN,
    NOTABLE_SHOT_LONG_CHIP_IN,
    NOTABLE_SHOT_UNUSED,
    NOTABLE_SHOT_FLAG_SHOT,
    NOTABLE_SHOT_HOLE_IN_ONE,
    NOTABLE_SHOT_ALBATROSS,
    NOTABLE_SHOT_EAGLE,
    NOTABLE_SHOT_BIRDIE,
    NOTABLE_SHOT_WATER_CUT,
    NOTABLE_SHOT_COUNT
};

typedef s32 NotableShot;

extern BallSimulation g_TurnBallSimulation;

void CountNotableShot(s32 playerIndex, s32 notableShot);
void StartFreeLook(void);
void PlaceBallAtRest(s32 playerIndex, BallSimulation *simulation);

void DrawShotPreview(CameraMatrices *matrices);
void PlaceAimCamera(void);
s32 TallyCoursePoints(void);
void UpdateTurns(void);

#endif
