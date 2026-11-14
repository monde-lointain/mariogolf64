#ifndef SHOT_CAMERA_H
#define SHOT_CAMERA_H

#include <ultra64.h>

#include "ball_simulation.h"
#include "geometry.h"
#include "language.h"
#include "math_util.h"
#include "player.h"
#include "surface.h"

#define HOLE_FLYOVER_FRAME_COUNT 185
#define HOLE_FLYOVER_ENDED (-1)

#define NOISE_GRID_COUNT 2

extern s32 g_HoleFlyoverFrame;
extern Boolean g_AreCharactersDrawn;
extern Boolean g_IsCupViewShown;

#define NOISE_GRID_SIZE 64
extern u8 *g_NoiseTexels;

enum EndReaction {
    END_REACTION_NONE,
    END_REACTION_ONLOOKERS,
    END_REACTION_LONG_PUTT
};

void ClearHoleIntroText(void);
void EndFlightCamera(void);
void EndHoleFlyover(void);
SurfaceInfo *GetBallSurfaceInfo(Player *player, ShotLaunch *shot);
SignedBinaryAngle GetAimSlopeAngle(Player *player, ShotLaunch *shot);
SignedBinaryAngle GetCrossSlopeAngle(Player *player, ShotLaunch *shot);
s32 GetEndReaction(void);
s32 RateShotLength(void);
void StartFlightCamera(s32 earlierFlightCount);
void StartHoleCamerasAndSong(void);
void StartHoleFlyover(void);
s32 StepHoleFlyover(void);
s32 UpdateFlightCamera(Boolean isAdvancing, s32 buttons);
Boolean IsDebugBButtonView(void);
void BuildNoiseTexture(void);
void StepNoiseTexture(void);
void PlaceGolferAtBall(Vec3 position, Vec3 rotation, Vec3 scale);
void OnShotCameraPlaced(void);

#endif
