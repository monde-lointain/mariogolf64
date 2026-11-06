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

#define NOISE_GRID_SIZE 64
/* An I8 texture of NOISE_GRID_SIZE x NOISE_GRID_SIZE texels */
extern u8* g_NoiseTexels;

/*
 * GetEndReaction's values: what greeted the ball as it neared its end, which
 * sets when UpdateTurns shows the hole-out banner
 */
enum EndReaction {
  END_REACTION_NONE,
  /* A flag shot's or chip-in's call, which queues the onlookers' sounds */
  END_REACTION_ONLOOKERS,
  /* SOUND_LONG_PUTT_HOLED, with no onlookers' sounds */
  END_REACTION_LONG_PUTT
};

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
/* Whether the view mode that the B button picks in debug builds is on */
Boolean IsDebugBButtonView(void);
void BuildNoiseTexture(void);
void StepNoiseTexture(void);
/* Sets a character's position, rotation and scale for a shot */
void PlaceGolferAtBall(Vec3 position, Vec3 rotation, Vec3 scale);
/* Called once PlaceShotCamera has placed the camera, and does nothing */
void OnShotCameraPlaced(void);

#endif
