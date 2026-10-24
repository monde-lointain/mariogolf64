#include "common.h"
#include "course.h"
#include "billboard.h"
#include "camera.h"
#include "club.h"
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

/* How far the camera must be from where it eases to before it moves, and how far it goes */
#define CAMERA_EASE_DEAD_ZONE 1.0f
#define CAMERA_EASE_FRACTION 0.2f

/*
 * The ring of BuildRingTexels' texture, by distance in texels from its
 * corner: clear inside RING_INNER_RADIUS, brightening to RING_PEAK_RADIUS,
 * then fading to clear at RING_OUTER_RADIUS
 */
#define RING_INNER_RADIUS 5
#define RING_PEAK_RADIUS 55
#define RING_OUTER_RADIUS 62
#define RING_INNER_INTENSITY 10
#define RING_RISE_PER_TEXEL 5
#define RING_PEAK_INTENSITY 255
#define RING_FALL_PER_TEXEL 36

/* RateShotLength's ratings */
#define SHOT_LENGTH_UNRATED 0
#define SHOT_LENGTH_FULL 1
#define SHOT_LENGTH_SHORT 2

/* The fractions of the selected shot distance a full and a short shot go beyond */
#define FULL_SHOT_FRACTION 0.8f
#define SHORT_SHOT_FRACTION 0.6f

/*
 * The flyover's frames: it swings around the cup by a half turn every
 * FLYOVER_HALF_TURN_FRAME_COUNT frames up to a full turn, opens the inset view
 * and closes it again, and ends on HOLE_FLYOVER_FRAME_COUNT
 */
#define FLYOVER_HALF_TURN_FRAME_COUNT 90
#define FLYOVER_MAX_HALF_TURNS 2.0f
#define FLYOVER_INSET_OPEN_FRAME 94
#define FLYOVER_INSET_CLOSE_FRAME 162
#define HOLE_FLYOVER_FRAME_COUNT 185
/* How the flyover's distance from the cup, and height above it, follow the hole's radius */
#define FLYOVER_RADIUS_APPROACH 15.0f
#define FLYOVER_DISTANCE_SCALE 1536.0f
#define FLYOVER_HEIGHT_APPROACH 200.0f
#define FLYOVER_HEIGHT_SCALE 1.5f
/* The inset camera looks at a point below the cup from this far and this much higher */
#define FLYOVER_INSET_LOOK_AT_DROP 15360.0f
#define FLYOVER_INSET_DISTANCE 184320.0f
#define FLYOVER_INSET_EYE_RISE 75.0f

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
extern s32 D_800BE640;
extern s32 D_800BE644;
extern s32 D_800BE648;
extern SurfaceInfo *sEndSurfaceInfo;
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
    g_InsetView.animation = INSET_VIEW_STILL;
    g_InsetView.step = 0;
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

/* It is a macro, as an inline function compiles differently from the original */
#define IsShotLengthRated(endKind)                                                 \
    ((endKind) != SURFACE_KIND_WATER_HAZARD && D_800BE644 >= sLandingCupDistance   \
     && D_800BE648 != 1 && g_SimulatedShot.unk2E14 && !g_SimulatedShot.unk2E00     \
     && g_SimulatedShot.shot.shotMode != SHOT_MODE_APPROACH)

/*
 * Rates how far the simulated shot goes, in yards, against the distance
 * selected for it. Shots that end out of play or in water, approach shots and
 * some others go unrated.
 */
s32 RateShotLength(void) {
    s32 endKind;

    endKind = sEndSurfaceInfo->kind;
    if (endKind == SURFACE_KIND_OUT_OF_COURSE) {
        return SHOT_LENGTH_UNRATED;
    }
    if (endKind == SURFACE_KIND_OUT_OF_BOUNDS) {
        return SHOT_LENGTH_UNRATED;
    }
    if (IsShotLengthRated(endKind)) {
        if (GetSelectedShotDistance() * FULL_SHOT_FRACTION < D_800BE640 * YARDS_PER_UNIT) {
            return SHOT_LENGTH_FULL;
        }
        if (GetSelectedShotDistance() * SHORT_SHOT_FRACTION < D_800BE640 * YARDS_PER_UNIT) {
            return SHOT_LENGTH_SHORT;
        }
    }
    return SHOT_LENGTH_UNRATED;
}

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

/* Eases each of the camera's coordinates toward target's, unless it is already close */
void EaseCameraPosition(Vec3 target) {
    f32 offset;

    offset = target[AXIS_X] - sCameraPosition[AXIS_X];
    /* The do-while is there to match */
    do {
        if (fabsf(offset) > CAMERA_EASE_DEAD_ZONE) {
            sCameraPosition[AXIS_X] += offset * CAMERA_EASE_FRACTION;
        }
    } while (0);
    offset = target[AXIS_Y] - sCameraPosition[AXIS_Y];
    if (fabsf(offset) > CAMERA_EASE_DEAD_ZONE) {
        sCameraPosition[AXIS_Y] += offset * CAMERA_EASE_FRACTION;
    }
    offset = target[AXIS_Z] - sCameraPosition[AXIS_Z];
    if (fabsf(offset) > CAMERA_EASE_DEAD_ZONE) {
        sCameraPosition[AXIS_Z] += offset * CAMERA_EASE_FRACTION;
    }
}

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_80047E9C);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_800484F8);

/* Starts the flyover of the hole, with the inset view on the cup */
void StartHoleFlyover(void) {
    g_HoleFlyoverFrame = 0;
    D_801061CE = 0;
    g_InsetView.animation = INSET_VIEW_STILL;
    g_InsetView.step = 0;
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
    g_InsetView.animation = INSET_VIEW_STILL;
    g_InsetView.step = 0;
    g_Cameras[INSET_CAMERA].view.drawFlags = 0;
    if (g_GameMode != GAME_MODE_UNK7 && !g_IsMiniGolf) {
        if (IsGameFlagSet(GAME_FLAG_UNK48) || g_HoleConditions.windSpeed / WIND_SPEED_UNIT >= STRONG_WIND_SPEED) {
            func_80216130(1);
        }
    }
}

/*
 * Moves the flyover's cameras on by a frame. The main camera swings around
 * the cup from the tee's side as it closes in; the inset camera circles the
 * cup twice as fast. It returns whether the flyover has reached its end.
 */
s32 StepHoleFlyover(void) {
    Vec3 offset;
    f32 halfTurns;
    f32 widening;
    f32 approach;
    f32 yaw;
    f32 distance;
    f32 height;
    s32 holeRadius;

    halfTurns = g_HoleFlyoverFrame * (1.0f / FLYOVER_HALF_TURN_FRAME_COUNT);
    if (g_HoleFlyoverFrame > FLYOVER_HALF_TURN_FRAME_COUNT) {
        widening = 0.0f;
        approach = 1.0f - sinf(DEGREES_TO_RADIANS(g_HoleFlyoverFrame));
    } else {
        approach = (1.0f - sinf(DEGREES_TO_RADIANS(g_HoleFlyoverFrame))) * 0.5f;
        widening = (FLYOVER_HALF_TURN_FRAME_COUNT - g_HoleFlyoverFrame) * (1.0f / FLYOVER_HALF_TURN_FRAME_COUNT);
    }
    if (halfTurns > FLYOVER_MAX_HALF_TURNS) {
        halfTurns = FLYOVER_MAX_HALF_TURNS;
    }
    if (!g_IsMiniGolf) {
        if (g_HoleFlyoverFrame == FLYOVER_INSET_OPEN_FRAME) {
            g_InsetView.animation = INSET_VIEW_OPENING;
        }
        if (g_HoleFlyoverFrame == FLYOVER_INSET_CLOSE_FRAME) {
            g_InsetView.animation = INSET_VIEW_CLOSING;
        }
    }
    offset[AXIS_X] = D_800BA9D8.teePosition[AXIS_X] - D_800BA9D8.cupPosition[AXIS_X];
    offset[AXIS_Y] = D_800BA9D8.teePosition[AXIS_Y] - g_CupPositionY;
    offset[AXIS_Z] = D_800BA9D8.teePosition[AXIS_Z] - g_CupPositionZ;
    g_InsetView.unk8 = 100;
    g_InsetView.unkC = 101;
    g_Cameras[0].view.lookAt[AXIS_X] =
        (D_800BA9D8.cupPosition[AXIS_X] + offset[AXIS_X] * approach) * COURSE_TO_WORLD_SCALE;
    g_Cameras[0].view.lookAt[AXIS_Y] = (g_CupPositionY + offset[AXIS_Y] * approach) * COURSE_TO_WORLD_SCALE;
    g_Cameras[0].view.lookAt[AXIS_Z] = (g_CupPositionZ + offset[AXIS_Z] * approach) * COURSE_TO_WORLD_SCALE;
    yaw = atan2(offset[AXIS_X], offset[AXIS_Z]) + halfTurns * M_PI;
    holeRadius = GetHoleRadius(GetCourseRecordIndex(), GetHoleRecordIndex());
    /* Storing the sine before computing distance is there to match */
    offset[AXIS_X] = sinf(yaw);
    distance = ((holeRadius - approach * FLYOVER_RADIUS_APPROACH) + holeRadius * widening) * FLYOVER_DISTANCE_SCALE;
    offset[AXIS_X] *= distance;
    offset[AXIS_Z] = cosf(yaw) * distance;
    g_Cameras[0].view.eye[AXIS_X] = g_Cameras[0].view.lookAt[AXIS_X] + offset[AXIS_X] * COURSE_TO_WORLD_SCALE;
    yaw += yaw;
    height = (holeRadius / 2 + approach * FLYOVER_HEIGHT_APPROACH) * FLYOVER_HEIGHT_SCALE;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] = D_800BA9D8.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
    g_Cameras[0].view.eye[AXIS_Y] = g_Cameras[0].view.lookAt[AXIS_Y] - height;
    g_Cameras[0].view.eye[AXIS_Z] = g_Cameras[0].view.lookAt[AXIS_Z] + offset[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] = (g_CupPositionY - FLYOVER_INSET_LOOK_AT_DROP) * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] = g_CupPositionZ * COURSE_TO_WORLD_SCALE;
    offset[AXIS_X] = sinf(yaw) * FLYOVER_INSET_DISTANCE;
    offset[AXIS_Z] = cosf(yaw) * FLYOVER_INSET_DISTANCE;
    g_Cameras[INSET_CAMERA].view.eye[AXIS_X] =
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] + offset[AXIS_X] * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.eye[AXIS_Y] = g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] - FLYOVER_INSET_EYE_RISE;
    g_Cameras[INSET_CAMERA].view.eye[AXIS_Z] =
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] + offset[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    return ++g_HoleFlyoverFrame == HOLE_FLYOVER_FRAME_COUNT;
}

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

/* Fills a width by height texture with a quarter of a ring around its far corner */
void BuildRingTexels(u8 *texels, s32 width, s32 height) {
    s32 row;
    s32 column;
    s32 texelIndex;
    s32 radius;
    f32 distance;
    s32 intensity;

    texelIndex = 0;
    for (row = 0; row != height; row++) {
        for (column = 0; column != width; column++) {
            distance = sqrtf((row - height) * (row - height) + (column - width) * (column - width));
            radius = distance;
            if (radius < RING_INNER_RADIUS) {
                intensity = 0;
            } else if (radius < RING_PEAK_RADIUS) {
                intensity = (s32)(distance - RING_INNER_RADIUS) * RING_RISE_PER_TEXEL + RING_INNER_INTENSITY;
            } else if (radius < RING_OUTER_RADIUS) {
                intensity = RING_PEAK_INTENSITY - (s32)(distance - RING_PEAK_RADIUS) * RING_FALL_PER_TEXEL;
            } else {
                intensity = 0;
            }
            texels[texelIndex++] = intensity;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004C958);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004CDA0);

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004D148);
