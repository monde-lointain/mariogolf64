#include "common.h"
#include "billboard.h"
#include "camera.h"
#include "current_shot.h"
#include "functions.h"
#include "game_flags.h"
#include "hole_loader.h"
#include "libc.h"
#include "math_util.h"
#include "save.h"
#include "scene.h"
#include "shot_simulation.h"
#include "sound.h"
#include "surface.h"
#include "text.h"
#include "variables.h"

/* Unused; the original has it */
static const char sUnusedFloatFormat[] = "            %f";

/* The camera kind that the B button picks in debug builds */
#define FLIGHT_CAMERA_UNK4 4

/* The values 0 to CAMERA_PICK_COUNT - 1, shuffled; NextCameraPick deals them in turn */
#define CAMERA_PICK_COUNT 10
/* How many swaps a shuffle makes */
#define CAMERA_PICK_SWAP_COUNT 5
#define NO_CAMERA_PICK (-1)

#define NO_FLIGHT_CAMERA (-1)

/* The field of view, in degrees, of the main camera between shots */
#define DEFAULT_FIELD_OF_VIEW 45.0f

/* The sound channels of the flight's sounds, which fade over SOUND_FADE_FRAME_COUNT */
#define FLIGHT_SOUND_CHANNEL_UNKB 0xB
#define FLIGHT_SOUND_CHANNEL_UNKF 0xF
#define SOUND_FADE_FRAME_COUNT 120

/* sEndReaction values: how the others reacted as the ball neared its end */
#define END_REACTION_NONE 0
#define END_REACTION_ONLOOKERS 1
#define END_REACTION_UNK2 2

#define COURSE_TO_WORLD_SCALE (1.0f / 1024.0f)

/* The flyover's field of view, in degrees, and the inset view's rectangle */
#define FLYOVER_FIELD_OF_VIEW 80.0f
#define FLYOVER_INSET_CENTER_X 160
#define FLYOVER_INSET_CENTER_Y 112
#define FLYOVER_INSET_SIZE 128
/* g_HoleFlyoverFrame's value once the flyover is over, while the screen fades back */
#define HOLE_FLYOVER_ENDED (-1)
/* The text slots that the hole's introduction shows */
#define INTRO_TEXT_SLOT_UNK0 0
#define INTRO_TEXT_SLOT_UNK1 1

/* windSpeed's units in a unit of wind, and the speed of a wind that EndHoleFlyover calls strong */
#define WIND_SPEED_UNIT 0x10000
#define STRONG_WIND_SPEED 6

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

extern s32 D_800BE698;
extern s32 D_800BE69C;
extern s32 D_800C5AAC;
/* The step of the simulated path that the ball's flight has reached */
extern s32 g_BallFlightStep;
extern SimulatedShot g_SimulatedShot;
extern s32 sCupViewHoldCount;
extern s32 g_HoleFlyoverFrame;
extern s8 D_801061CE;
extern s32 sEndReaction;
/* How many frames the flight camera waits after the ball's path ends */
extern s32 sLandingWaitFrameCount;
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

void func_8009226C(s32);
void func_80205AE4(s32, s32);
void func_80216130(s32);
void func_80216B74(s32);
s32 StepHoleFlyover(void);

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

/*
 * Ends the flight camera: it restores the main camera's field of view, stops
 * drawing the inset view, fades the flight's sounds, and reports a ball that
 * has finished its path with unk2E41 set.
 */
void EndFlightCamera(void) {
    D_800BE698 = 1;
    D_800BE69C = 0;
    sCupViewHoldCount = 0;
    D_800C5AAC = 0;
    g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
    func_8009226C(-1);
    g_InsetViewAnimation = INSET_VIEW_STILL;
    g_InsetViewStep = 0;
    g_Cameras[INSET_CAMERA].view.drawFlags = 0;
    FadeOutSound(FLIGHT_SOUND_CHANNEL_UNKB, SOUND_FADE_FRAME_COUNT);
    FadeOutSound(FLIGHT_SOUND_CHANNEL_UNKF, SOUND_FADE_FRAME_COUNT);
    func_80216B74(0);
    sFlightCameraKind = NO_FLIGHT_CAMERA;
    if (g_BallFlightStep == g_SimulatedShot.stepCount) {
        osSyncPrintf("\nCheckWater1\n");
        if (g_SimulatedShot.unk2E41) {
            osSyncPrintf("\nCheckWater2\n");
            func_80205AE4(g_CurrentShot.playerIndex, 0xB);
        }
    }
}

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

s32 GetEndReaction(void) {
    return sEndReaction;
}

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

/* Starts the flyover of the hole, with the inset view on the cup */
void StartHoleFlyover(void) {
    g_HoleFlyoverFrame = 0;
    D_801061CE = 0;
    g_InsetViewAnimation = INSET_VIEW_STILL;
    g_InsetViewStep = 0;
    g_Cameras[INSET_CAMERA].view.drawFlags = 0;
    g_Cameras[0].view.mode = CAMERA_MODE_BEHIND_GOLFER;
    g_Cameras[INSET_CAMERA].view.mode = CAMERA_MODE_BEHIND_GOLFER;
    g_Cameras[0].view.fieldOfView = FLYOVER_FIELD_OF_VIEW;
    SetTextSlot(0, 0, NO_MESSAGE, 1, INTRO_TEXT_SLOT_UNK0, 0, 0, 0, 0, 0);
    SetTextSlot(0, 0, NO_MESSAGE, 1, INTRO_TEXT_SLOT_UNK1, 0, 0, 0, 0, 0);
    SetInsetViewRect(FLYOVER_INSET_CENTER_X, FLYOVER_INSET_CENTER_Y, FLYOVER_INSET_SIZE, FLYOVER_INSET_SIZE);
    StepHoleFlyover();
}

void ClearHoleIntroText(void) {
    D_801061CE = 0;
    SetTextSlot(0, 0, NO_MESSAGE, 1, INTRO_TEXT_SLOT_UNK0, 0, 0, 0, 0, 0);
    SetTextSlot(0, 0, NO_MESSAGE, 1, INTRO_TEXT_SLOT_UNK1, 0, 0, 0, 0, 0);
}

/* Ends the hole flyover; the screen fades back, and a strong wind gets func_80216130 */
void EndHoleFlyover(void) {
    g_HoleFlyoverFrame = HOLE_FLYOVER_ENDED;
    g_InsetViewAnimation = INSET_VIEW_STILL;
    g_InsetViewStep = 0;
    g_Cameras[INSET_CAMERA].view.drawFlags = 0;
    if (g_GameMode != GAME_MODE_UNK7 && !g_IsMiniGolf) {
        if (IsGameFlagSet(GAME_FLAG_UNK48) || g_HoleConditions.windSpeed / WIND_SPEED_UNIT >= STRONG_WIND_SPEED) {
            func_80216130(1);
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/shot_camera", StepHoleFlyover);

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

void SetLandingWaitFrameCount(s32 frameCount) {
    sLandingWaitFrameCount = frameCount;
}

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004C51C);

void func_8004C6A8(void) {
}

void func_8004C6B0(void) {
}

void func_8004C6B8(void) {
}

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004C6C0);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004C860);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004C958);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004CDA0);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004D148);
