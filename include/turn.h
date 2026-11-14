#ifndef TURN_H
#define TURN_H

#include <ultra64.h>

#include "ball_simulation.h"
#include "camera.h"

enum ShotKind {
    SHOT_KIND_GREAT,
    SHOT_KIND_EXCELLENT,
    SHOT_KIND_PIN_SHOT,
    SHOT_KIND_CHIP_IN,
    SHOT_KIND_LONG_CHIP_IN,
    SHOT_KIND_UNUSED,
    SHOT_KIND_FLAG_SHOT,
    SHOT_KIND_HOLE_IN_ONE,
    SHOT_KIND_ALBATROSS,
    SHOT_KIND_EAGLE,
    SHOT_KIND_BIRDIE,
    SHOT_KIND_WATER_CUT,
    SHOT_KIND_COUNT
};

extern BallSimulation g_TurnBallSimulation;

void CountNotableShot(s32 playerIndex, s32 shotKind);
void StartFreeLook(void);
void PlaceBallAtRest(s32 playerIndex, BallSimulation *simulation);

void DrawShotPreview(CameraMatrices *matrices);
void PlaceAimCamera(void);
s32 TallyCoursePoints(void);
void UpdateTurns(void);

#endif
