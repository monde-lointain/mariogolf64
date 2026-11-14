#ifndef CURRENT_SHOT_H
#define CURRENT_SHOT_H

#include <ultra64.h>

#include "club.h"
#include "course_space.h"
#include "math/math_util.h"
#include "surface.h"

typedef struct CurrentShot {
    SignedBinaryAngle heading;
    s32 powerGaugeFrame;
    s32 impactGaugeFrame;
    ShotMode shotMode;
    ClubSlot clubSlot;
    s32 offsetX;
    s32 offsetY;
    CoursePoint ballPosition;
    Surface surface;
    s32 playerIndex;
    s32 controllerIndex;
    s32 aimCameraDistance;
    s32 unusedBeforeAimCameraHeight;
    s32 aimCameraHeight;
    s32 gaugeFrame;
    CoursePoint targetPoint;
    s32 cupDistance;
    s8 powerShotCount;
} CurrentShot;

enum ImpactPointState {
    IMPACT_POINT_LINGERING = -1,
    IMPACT_POINT_OFF,
    IMPACT_POINT_SETTING
};

typedef s32 ImpactPointState;

#define GAUGE_FULL_FRAME 30
#define GAUGE_IMPACT_FRAME (2 * GAUGE_FULL_FRAME)

#if defined(__IS_KMC__) && !defined(READS_CURRENT_SHOT_VOLATILE)
extern CurrentShot g_CurrentShot;
#else
extern volatile CurrentShot g_CurrentShot;
#endif
extern ImpactPointState g_ImpactPointState;

#endif
