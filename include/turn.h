#ifndef TURN_H
#define TURN_H

#include <ultra64.h>

#include "ball_simulation.h"
#include "camera.h"
#include "player.h"

extern BallSimulation g_TurnBallSimulation;

void CountNotableShot(s32 playerIndex, NotableShot notableShot);
void StartFreeLook(void);
void PlaceBallAtRest(s32 playerIndex, BallSimulation *simulation);

void DrawShotPreview(CameraMatrices *matrices);
void PlaceAimCamera(void);
s32 TallyCoursePoints(void);
void UpdateTurns(void);

#endif
