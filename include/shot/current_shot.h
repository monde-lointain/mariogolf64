/*
 * The shot that the player whose turn it is sets up, and the swing gauge.
 */

#ifndef CURRENT_SHOT_H
#define CURRENT_SHOT_H

#include <ultra64.h>

#include "course/course_space.h"
#include "course/surface.h"
#include "math/math_util.h"
#include "shot/club.h"

// The state of the shot being set up by the player whose turn it is
typedef struct CurrentShot {
    SignedBinaryAngle heading;
    s32 powerGaugeFrame; // The gaugeFrame at which power is set
    s32 impactGaugeFrame; // The gaugeFrame at which impact is set
    ShotMode shotMode;
    ClubSlot clubSlot; // Index into the player's clubs
    s32 offsetX; // The shot offset that the stick sets
    s32 offsetY;
    CoursePoint ballPosition;
    Surface surface;
    s32 playerIndex;
    s32 controllerIndex;
    s32 aimCameraDistance; // From the ball along the aim, so negative is behind it
    s32 unusedBeforeAimCameraHeight;
    s32 aimCameraHeight; // Added to the lower of the ground's and the ball's heights
    s32 gaugeFrame; // Frames since the swing gauge started
    CoursePoint targetPoint; // On the ground, the selected club's distance along the heading
    s32 cupDistance; // From GetCupDistance
    s8 powerShotCount; // The shooter's, as the shot began
} CurrentShot;

// Whether the impact point is being set. Holding Z or L sets it with the stick,
// rather than aiming; once they are let go, it lingers for a few frames.
enum ImpactPointState {
    IMPACT_POINT_LINGERING = -1,
    IMPACT_POINT_OFF,
    IMPACT_POINT_SETTING
};

typedef s32 ImpactPointState;

// The swing gauge fills until gaugeFrame reaches GAUGE_FULL_FRAME, then empties
// until it reaches the impact mark at GAUGE_IMPACT_FRAME
#define GAUGE_FULL_FRAME 30
#define GAUGE_IMPACT_FRAME (2 * GAUGE_FULL_FRAME)

#if defined(__IS_KMC__) && !defined(READS_CURRENT_SHOT_VOLATILE)
// current_shot.c defines g_CurrentShot volatile, but most files saw it
// without the qualifier. game.c defines READS_CURRENT_SHOT_VOLATILE to see the
// qualifier.
extern CurrentShot g_CurrentShot;
#else
extern volatile CurrentShot g_CurrentShot;
#endif
extern ImpactPointState g_ImpactPointState;

#endif
