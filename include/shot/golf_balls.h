/*
 * The players' golf balls and the afterimages of the ball in play.
 */

#ifndef GOLF_BALLS_H
#define GOLF_BALLS_H

#include <ultra64.h>

#include "camera/camera.h"
#include "system/c_language.h"

// The wind indicator draws it while MODULE_WIND_BOO is alive
extern Gfx g_BooDisplayList[];

void LoadGolfBallImages(void);
void ResetBallAfterimage(void);
void StartFastBallAfterimage(void);
void ResetBallRotation(void);
void ResetGolfBalls(void);
Boolean IsAnyBallInCup(void);
void UpdateGolfBalls(void);

void DrawGolfBalls(CameraMatrices *matrices);

#endif
