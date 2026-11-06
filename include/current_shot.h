#ifndef CURRENT_SHOT_H
#define CURRENT_SHOT_H

#include <ultra64.h>

#include "club.h"
#include "course_space.h"
#include "math_util.h"
#include "surface.h"

/* The state of the shot being set up by the player whose turn it is */
typedef struct CurrentShot {
  /* 0x00 */ SignedBinaryAngle heading;
  /* 0x04 */ s32 powerGaugeFrame; /* The gaugeFrame at which power is set */
  /* 0x08 */ s32 impactGaugeFrame; /* The gaugeFrame at which impact is set */
  /* 0x0C */ ShotMode shotMode;
  /* 0x10 */ s32 clubSlot; /* Index into the player's clubs */
  /* 0x14 */ s32 offsetX; /* The shot offset that the stick sets */
  /* 0x18 */ s32 offsetY;
  /* 0x1C */ CoursePoint ballPosition;
  /* 0x28 */ Surface surface;
  /* 0x2C */ s32 playerIndex;
  /* 0x30 */ s32 controllerIndex;
  /* 0x34 */ s32 aimCameraDistance; /* From the ball along the aim, so negative
                                       is behind it */
  /* 0x38 */ s32 unusedBeforeAimCameraHeight;
  /* 0x3C */ s32 aimCameraHeight; /* Added to the lower of the ground's and the
                                     ball's heights */
  /* 0x40 */ s32 gaugeFrame; /* Frames since the swing gauge started */
  /* 0x44 */ CoursePoint targetPoint; /* On the ground, the selected club's
                                         distance along the heading */
  /* 0x50 */ s32 cupDistance; /* From GetCupDistance */
  /* 0x54 */ s8 powerShotCount; /* The shooter's, as the shot began */
} CurrentShot; /* size = 0x58 */

/*
 * Whether the impact point is being set. Holding Z or L sets it with the stick,
 * rather than aiming; once they are let go, it lingers for a few frames.
 */
enum ImpactPointState {
  IMPACT_POINT_LINGERING = -1,
  IMPACT_POINT_OFF,
  IMPACT_POINT_SETTING
};

/*
 * The swing gauge fills until gaugeFrame reaches GAUGE_FULL_FRAME, then empties
 * until it reaches the impact mark at GAUGE_IMPACT_FRAME
 */
#define GAUGE_FULL_FRAME 30
#define GAUGE_IMPACT_FRAME (2 * GAUGE_FULL_FRAME)

#ifdef __IS_KMC__
/* current_shot.c defines g_CurrentShot volatile, but most files saw it
 * without the qualifier. game.c declares it itself, with the qualifier. */
extern CurrentShot g_CurrentShot;
#else
extern volatile CurrentShot g_CurrentShot;
#endif
/* An IMPACT_POINT_* value */
extern s32 g_ImpactPointState;

#endif
