#ifndef SHOT_CAMERA_H
#define SHOT_CAMERA_H

#include <ultra64.h>

#include "course/surface.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "player.h"
#include "shot/ball_simulation.h"
#include "system/c_language.h"

enum ShotLengthRating {
    SHOT_LENGTH_UNRATED,
    SHOT_LENGTH_FULL,
    SHOT_LENGTH_SHORT
};

typedef s32 ShotLengthRating;

#define HOLE_FLYOVER_FRAME_COUNT 185
#define HOLE_FLYOVER_DONE (-1)

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

typedef s32 EndReaction;

void ClearHoleIntroText(void);
void StopFlightCamera(void);
void StopHoleFlyover(void);
SurfaceTraits *GetBallSurfaceTraits(Player *player, ShotLaunch *shot);
#if defined(AVOID_UB) || !defined(WIDENS_AIM_SLOPE_ANGLE)
SignedBinaryAngle GetAimSlopeAngle(Player *player, ShotLaunch *shot);
#else
s32 GetAimSlopeAngle(Player *player, ShotLaunch *shot);
#endif
SignedBinaryAngle GetCrossSlopeAngle(Player *player, ShotLaunch *shot);
s32 GetEndReaction(void);
ShotLengthRating RateShotLength(void);
void StartFlightCamera(s32 earlierFlightCount);
void StartHoleCamerasAndSong(void);
void StartHoleFlyover(void);
Boolean StepHoleFlyover(void);
Boolean UpdateFlightCamera(Boolean isAdvancing, s32 buttons);
Boolean IsDebugBButtonView(void);
void BuildNoiseTexture(void);
void StepNoiseTexture(void);
void PlaceGolferAtBall(Vec3 position, Vec3 rotation, Vec3 scale);
void OnShotCameraPlaced(void);

#endif
