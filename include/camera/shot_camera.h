/*
 * The cameras of a hole, the flyover and the flight camera, and the slope and
 * noise texture routines of shot_camera.c.
 */

#ifndef SHOT_CAMERA_H
#define SHOT_CAMERA_H

#include <ultra64.h>

#include "course/surface.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "round/player.h"
#include "shot/ball_simulation.h"
#include "system/c_language.h"

// RateShotLength's ratings
enum ShotLengthRating {
    SHOT_LENGTH_UNRATED,
    SHOT_LENGTH_FULL,
    SHOT_LENGTH_SHORT
};

typedef s32 ShotLengthRating;

// The fly-over's last frame, which StepHoleFlyover ends on
#define HOLE_FLYOVER_FRAME_COUNT 185
// g_HoleFlyoverFrame's value once the flyover is over, while the screen fades
// back
#define HOLE_FLYOVER_DONE (-1)

// How many noise grids f_NoiseGrids holds for the noise texture to blend
#define NOISE_GRID_COUNT 2

// The frame the hole's fly-over has reached
extern s32 g_HoleFlyoverFrame;
// While set, characters are drawn for every camera but the inset
extern Boolean g_AreCharactersDrawn;
// Set while the camera shows the cup up close, at twice the distance from it
extern Boolean g_IsCupViewShown;

#define NOISE_GRID_SIZE 64
// An I8 texture of NOISE_GRID_SIZE x NOISE_GRID_SIZE texels
extern u8 *g_NoiseTexels;

// GetEndReaction's values: what greeted the ball as it neared its end, which
// sets when UpdateTurns shows the hole-out banner
enum EndReaction {
    END_REACTION_NONE,
    // A flag shot's or chip-in's call, which queues the onlookers' sounds
    END_REACTION_ONLOOKERS,
    // SOUND_LONG_PUTT_HOLED, with no onlookers' sounds
    END_REACTION_LONG_PUTT
};

typedef s32 EndReaction;

void ClearHoleIntroText(void);
void StopFlightCamera(void);
void StopHoleFlyover(void);
SurfaceTraits *GetBallSurfaceTraits(Player *player, ShotLaunch *shot);
// ball_simulation.c defines WIDENS_AIM_SLOPE_ANGLE to declare the result as an
// s32, which LaunchBallSimulation's code needs to match; as a
// SignedBinaryAngle, 2 words differ, 2 in shape. AVOID_UB keeps the true type.
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
// Updates the flight camera, the inset view and the ball's sounds for one frame of the ball's
// flight, and advances f_BallFlightStep when isAdvancing is 1. Once the flight has ended, it counts
// the landing wait and returns TRUE when it is over. The START button clears
// g_RoundHud.isWindIndicatorShown through g_RoundHud.isProgressShown and
// g_RoundHud.isShotLengthShown.
Boolean UpdateFlightCamera(Boolean isAdvancing, s32 buttons);
// Whether the view mode that the B button picks in debug builds is on
Boolean IsDebugBButtonView(void);
void BuildNoiseTexture(void);
void StepNoiseTexture(void);
// Sets a character's position, rotation and scale for a shot
void PlaceGolferAtBall(Vec3 position, Vec3 rotation, Vec3 scale);
// Called once PlaceShotCamera has placed the camera, and does nothing
void OnShotCameraPlaced(void);

#endif
