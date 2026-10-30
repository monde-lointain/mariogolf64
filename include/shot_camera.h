#ifndef SHOT_CAMERA_H
#define SHOT_CAMERA_H

#include <ultra64.h>

#include "ball_simulation.h"
#include "math_util.h"
#include "save.h"
#include "surface.h"

/* The fly-over's last frame, which StepHoleFlyover ends on */
#define HOLE_FLYOVER_FRAME_COUNT 185
/* g_HoleFlyoverFrame's value once the flyover is over, while the screen fades
 * back */
#define HOLE_FLYOVER_ENDED (-1)

/* How many noise grids f_NoiseGrids holds for the noise texture to blend */
#define NOISE_GRID_COUNT 2

/* The frame the hole's fly-over has reached */
extern s32 g_HoleFlyoverFrame;
/* While set, characters are drawn for every camera but the inset */
extern Boolean g_AreCharactersDrawn;
/* Set while the camera shows the cup up close, at twice the distance from it */
extern Boolean g_IsCupViewShown;

void ClearHoleIntroText(void);
void EndFlightCamera(void);
void EndHoleFlyover(void);
SurfaceInfo* GetBallSurfaceInfo(Player* player, ShotLaunch* shot);
SignedBinaryAngle GetAimSlopeAngle(Player* player, ShotLaunch* shot);
SignedBinaryAngle GetCrossSlopeAngle(Player* player, ShotLaunch* shot);
s32 GetEndReaction(void);
s32 RateShotLength(void);
void StartFlightCamera(s32 earlierFlightCount);
void StartHoleCamerasAndSong(void);
void StartHoleFlyover(void);
s32 StepHoleFlyover(void);
s32 UpdateFlightCamera(Boolean isAdvancing, s32 buttons);
#endif
