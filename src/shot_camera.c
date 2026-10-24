#include "common.h"
#include "functions.h"
#include "libc.h"
#include "math_util.h"
#include "save.h"
#include "surface.h"

/* Unused; the original has it */
static const char sUnusedFloatFormat[] = "            %f";

/* The camera kind that the B button picks in debug builds */
#define FLIGHT_CAMERA_UNK4 4

/* The values 0 to CAMERA_PICK_COUNT - 1, shuffled; NextCameraPick deals them in turn */
#define CAMERA_PICK_COUNT 10
/* How many swaps a shuffle makes */
#define CAMERA_PICK_SWAP_COUNT 5
#define NO_CAMERA_PICK (-1)

#define COURSE_TO_WORLD_SCALE (1.0f / 1024.0f)

/* How far below the ball the look-at point settles, in world units */
#define LOOK_AT_DROP 19.650002f
/* The fractions of the way to the ball that the look-at point eases each frame */
#define LOOK_AT_EASE_FRACTION 0.2f
#define LOOK_AT_HEIGHT_EASE_FRACTION 0.5f

#define FLIGHT_CAMERA_FLAG_UNK2 (1 << 1)

/*
 * How near IsCupPassPoint's points are to the cup, how far below it they
 * may be, and how near the cup the landing is
 */
#define CUP_PASS_RADIUS 0x1E00
#define CUP_PASS_MAX_DEPTH 0x1A400
#define CUP_PASS_MAX_LANDING_DISTANCE 0x4B000

extern s32 sLandingCupDistance;
extern s32 sFlightCameraFlags;
extern f32 sCameraYaw;
extern s32 sFlightCameraKind;
extern s32 sNextCameraPickIndex;
extern s8 sCameraPicks[CAMERA_PICK_COUNT];
extern CoursePoint sFlightBallPosition;
extern Vec3 sCameraPosition;
extern Vec3 sLookAtPoint;

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_800453E0);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_80045734);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_80045910);

/* The surface under the player's ball */
SurfaceInfo *GetBallSurfaceInfo(UnkB8 *player) {
    return GetSurfaceInfoAt(player->ballPosition[AXIS_X], player->ballPosition[AXIS_Z]);
}

s32 IsDebugBButtonView(void) {
    return sFlightCameraKind == FLIGHT_CAMERA_UNK4;
}

/*
 * Deals the next camera pick, shuffling them again after the last. When
 * isDealing is false, it shuffles them and returns NO_CAMERA_PICK.
 *
 * SwapCameraPicks is a nested function, a GNU C extension.
 */
s32 NextCameraPick(s32 isDealing) {
    s32 index;
    s32 pick;

    void SwapCameraPicks(s32 index, s32 otherIndex) {
        s32 pick;

        pick = sCameraPicks[index];
        sCameraPicks[index] = sCameraPicks[otherIndex];
        sCameraPicks[otherIndex] = pick;
    }

    if (!isDealing) {
        sNextCameraPickIndex = 0;
        /* The != tests are there to match */
        for (index = 0; index != CAMERA_PICK_COUNT; index++) {
            sCameraPicks[index] = index;
        }
        for (index = 0; index != CAMERA_PICK_SWAP_COUNT; index++) {
            SwapCameraPicks(index, rand() % CAMERA_PICK_COUNT);
        }
        return NO_CAMERA_PICK;
    }
    pick = sCameraPicks[sNextCameraPickIndex];
    if (++sNextCameraPickIndex == CAMERA_PICK_COUNT) {
        NextCameraPick(FALSE);
    }
    return pick;
}

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_80045C00);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_80045CE0);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_80046604);

/* A random value from 0 to limit - 1, or 0 if limit is 0 */
s32 RandomBelow(s32 limit) {
    if (limit == 0) {
        limit = 1;
    }
    return (guRandom() >> 2) % limit;
}

/*
 * Whether a point of the ball's path passes close by the cup, for a shot
 * that lands near it. Distances are in course units.
 */
s32 IsCupPassPoint(s32 cupDistance, s32 height) {
    if (cupDistance < CUP_PASS_RADIUS && height > g_CupPositionY - CUP_PASS_MAX_DEPTH &&
        sLandingCupDistance < CUP_PASS_MAX_LANDING_DISTANCE && (sFlightCameraFlags & FLIGHT_CAMERA_FLAG_UNK2)) {
        return TRUE;
    }
    return FALSE;
}

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_80046898);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_800469E0);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_800469EC);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_800479C0);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_80047B34);

/* Eases the look-at point toward the ball */
INCLUDE_ASM("asm/nonmatchings/shot_camera", EaseLookAtPoint);

/* Puts the look-at point on the ball */
void SnapLookAtPoint(void) {
    sLookAtPoint[AXIS_X] = sFlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
    sLookAtPoint[AXIS_Y] = sFlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    sLookAtPoint[AXIS_Z] = sFlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
}

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_80047DBC);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_80047E9C);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_800484F8);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_80048690);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004876C);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_800487E4);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004887C);

/* Turns the camera's yaw to face the look-at point, unless the camera is over it */
void AimCameraYaw(void) {
    f32 offsetX;
    f32 offsetZ;

    offsetX = sLookAtPoint[AXIS_X] - sCameraPosition[AXIS_X];
    offsetZ = sLookAtPoint[AXIS_Z] - sCameraPosition[AXIS_Z];
    if ((s32)offsetX != 0 || (s32)offsetZ != 0) {
        sCameraYaw = M_PI - atan2f(offsetX, offsetZ);
    }
}

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_80048D7C);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004AC04);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004B474);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004C510);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004C51C);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004C6A8);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004C6B0);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004C6B8);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004C6C0);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004C860);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004C958);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004CDA0);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004D148);
