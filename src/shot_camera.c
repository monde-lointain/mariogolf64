#include "common.h"
#include "controller.h"
#include "course.h"
#include "ball_simulation.h"
#include "billboard.h"
#include "camera.h"
#include "character_slot.h"
#include "club.h"
#include "current_shot.h"
#include "debug_text.h"
#include "framing.h"
#include "functions.h"
#include "game_flags.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "libc.h"
#include "math_util.h"
#include "movie.h"
#include "overlay_manager.h"
#include "pin.h"
#include "round_hud.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "shot_simulation.h"
#include "sound.h"
#include "surface.h"
#include "terrain.h"
#include "text.h"
#include "variables.h"

/* Unused; the original has it */
static const char sUnusedFloatFormat[] = "            %f";

/* The flight camera kinds, which sFlightCameraKind holds */
#define FLIGHT_CAMERA_UNK0 0
#define FLIGHT_CAMERA_UNK1 1
#define FLIGHT_CAMERA_BESIDE_TEE 2 /* Beside the tee, looking at it */
#define FLIGHT_CAMERA_UNK3 3 /* Placed by func_80203B4C */
#define FLIGHT_CAMERA_BESIDE_GOLFER 4 /* Picked by the B button in debug builds */
#define FLIGHT_CAMERA_CIRCLING_BALL 5 /* Picked by the A button in debug builds */
#define FLIGHT_CAMERA_TRAILING_BALL 6
#define FLIGHT_CAMERA_NEAR_LAUNCH 7 /* At an early step of the path */
#define FLIGHT_CAMERA_CHASING_BALL 8
#define FLIGHT_CAMERA_FOLLOWING_PATH 9 /* At a step of the path behind the ball */
#define FLIGHT_CAMERA_SIDE_OF_FLIGHT 0xA
#define FLIGHT_CAMERA_BEHIND_START 0xB
#define FLIGHT_CAMERA_AROUND_END 0xC /* Placed by PlaceCameraAroundShotEnd */
#define FLIGHT_CAMERA_UNKD 0xD
#define FLIGHT_CAMERA_UNKE 0xE
#define FLIGHT_CAMERA_UNKF 0xF
#define FLIGHT_CAMERA_UNK10 0x10
#define FLIGHT_CAMERA_UNK11 0x11
#define FLIGHT_CAMERA_FRAMING_PATH 0x12 /* Frames the path's extreme points on x and z */
/* Frames the cup and the ball; it keeps the putter's contact from calling func_80213D70 */
#define FLIGHT_CAMERA_FRAMING_CUP 0x13
#define FLIGHT_CAMERA_FLAGGED_STEP 0x14 /* Looks along the path at D_800BE64C */

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

/* Values of sPendingResultSound, which plays when D_800BE620 reaches RESULT_SOUND_STAGE */
#define RESULT_SOUND_NONE 0
#define RESULT_SOUND_UNK1 1 /* Plays FLIGHT_SOUND_UNK17 */
#define RESULT_SOUND_UNK2 2 /* Plays FLIGHT_SOUND_UNK67 */
#define RESULT_SOUND_STAGE 15

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
/*
 * The ball is landing once it is within LANDING_STEP_COUNT steps of the end of
 * the simulated path and within LANDING_RADIUS of its end on both x and z
 */
#define LANDING_STEP_COUNT 10
#define LANDING_RADIUS 0x1E00

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
extern CoursePoint sShotEndPosition;
extern f32 sShotFlightYaw;
extern s32 D_800DAF50;
extern s32 D_800BE628;
extern s32 D_800BE630;
extern s32 D_800BE64C;
extern s32 D_800BE658;
extern s32 D_800BE674;
extern s32 D_800BE690;
extern Vec3 sCameraPosition;
extern Vec3 sLookAtPoint;

/* How far the golfer stands along the shot's yaw from the ball */
#define STANCE_AIM_OFFSET -921.60004f
#define STANCE_SLOPE_PASS_COUNT 2

static inline f32 AbsoluteF32(f32 value) {
    if (value < 0.0f) {
        value = -value;
    }
    return value;
}

/*
 * Places a golfer at the current shot's ball, its club's ball distance away a
 * quarter turn from the shot's yaw, and tilts it to the slope of the ground
 * there. The golfer stands on the other side, mirrored, when player->unk22 is set.
 * Each pass measures the slope again at the point the last slope gives.
 */
void PlaceGolferAtBall(Vec3 position, Vec3 rotation, Vec3 scale) {
    Vec3 groundPoint;
    Mat4 unusedMatrices[2]; /* Unused; it is there to match */
    UnkB8 *player;
    s16 aimYaw;
    f32 distance;
    f32 angle;
    f32 reach;
    f32 slope;
    s32 ballHeight;
    s32 passIndex;

    player = &g_Players[g_CurrentShot.playerIndex];
    aimYaw = g_CurrentShot.unk0;
    distance = GetBallDistance(0, player->clubs[g_CurrentShot.clubSlot], FALSE, g_CurrentShot.shotMode);
    angle = ((aimYaw + BINARY_ANGLE_TURN / 4) & BINARY_ANGLE_MASK) * BINARY_ANGLE_TO_RADIANS;
    if (player->unk22) {
        distance = -distance;
    }
    groundPoint[AXIS_X] = cosf(angle) * distance + sinf(angle) * STANCE_AIM_OFFSET + player->ballPosition[AXIS_X];
    /* The offset's sign on z differs from the loop's; the original has it */
    groundPoint[AXIS_Z] = sinf(angle) * distance + cosf(angle) * STANCE_AIM_OFFSET + player->ballPosition[AXIS_Z];
    groundPoint[AXIS_Y] = GetGroundHeight(groundPoint[AXIS_X], groundPoint[AXIS_Z]);
    ballHeight = player->ballPosition[AXIS_Y];
    slope = atan2(groundPoint[AXIS_Y] - ballHeight, distance);
    for (passIndex = 0; passIndex < STANCE_SLOPE_PASS_COUNT; passIndex++) {
        reach = cosf(slope) * distance;
        position[AXIS_X] = cosf(angle) * reach + sinf(angle) * STANCE_AIM_OFFSET + player->ballPosition[AXIS_X];
        position[AXIS_Z] = sinf(angle) * reach - cosf(angle) * STANCE_AIM_OFFSET + player->ballPosition[AXIS_Z];
        position[AXIS_Y] = GetGroundHeight(position[AXIS_X], position[AXIS_Z]);
        if (position[AXIS_Y] - ballHeight > AbsoluteF32(distance)) {
            position[AXIS_Y] = AbsoluteF32(distance) + ballHeight;
        }
        slope = rotation[AXIS_X] = atan2(position[AXIS_Y] - ballHeight, reach);
    }
    scale[AXIS_X] = scale[AXIS_Y] = scale[AXIS_Z] = 1.0f;
    if (player->unk22) {
        scale[AXIS_Y] = -1.0f;
    }
    rotation[AXIS_Z] = 0.0f;
}

#define CROSS_SLOPE_DISTANCE 0x3C00
#define CROSS_SLOPE_CAPPED_RISE 0x1E00
#define TEE_HEIGHT 307.2f
#define AIM_SLOPE_STEP 6144.0f
#define AIM_SLOPE_MAX_RISE 0x1800

/*
 * Returns the binary angle of the ground's slope from the ball to a point
 * CROSS_SLOPE_DISTANCE away, a quarter turn from the shot's yaw. The point is
 * on the other side when player->unk22 is set, and the slope is then negated.
 */
s16 GetCrossSlopeAngle(UnkB8 *player, Unk20 *shot) {
    SurfaceInfo *surfaceInfo;
    s32 sampleDistance;
    s16 aimYaw;
    s32 yaw;
    f32 angle;
    s32 height;
    s32 rise;

    aimYaw = shot->unk0;
    /* The (s8) cast is there to match */
    surfaceInfo = GetSurfaceInfo((s8)player->ballSurface);
    sampleDistance = CROSS_SLOPE_DISTANCE;
    yaw = aimYaw + BINARY_ANGLE_TURN / 4;
    if (player->unk22) {
        angle = BINARY_ANGLE_TO_RADIANS * ((yaw + BINARY_ANGLE_TURN / 2) & BINARY_ANGLE_MASK);
    } else {
        angle = (yaw & BINARY_ANGLE_MASK) * BINARY_ANGLE_TO_RADIANS;
    }
    height = GetGroundHeight((s32)(cos(angle) * sampleDistance) + player->ballPosition[AXIS_X],
                             (s32)(sin(angle) * sampleDistance) + player->ballPosition[AXIS_Z]);
    if (surfaceInfo->kind == SURFACE_KIND_TEE_UP) {
        height -= TEE_HEIGHT;
    }
    if (height < player->ballPosition[AXIS_Y] - sampleDistance) {
        height = player->ballPosition[AXIS_Y] - sampleDistance;
    }
    if (height > player->ballPosition[AXIS_Y] + sampleDistance) {
        height = player->ballPosition[AXIS_Y] + sampleDistance;
    }
    rise = height - player->ballPosition[AXIS_Y];
    if (player->unk22) {
        rise = -rise;
    }
    if (rise > CROSS_SLOPE_DISTANCE) {
        rise = CROSS_SLOPE_CAPPED_RISE;
    } else if (rise < -CROSS_SLOPE_DISTANCE) {
        rise = -CROSS_SLOPE_CAPPED_RISE;
    }
    return atan2(rise, sampleDistance) * RADIANS_TO_BINARY_ANGLE;
}

/*
 * Returns the binary angle of the ground's slope along the shot's yaw, between
 * points half an AIM_SLOPE_STEP ahead of the ball and an AIM_SLOPE_STEP behind it.
 */
s16 GetAimSlopeAngle(UnkB8 *player, Unk20 *shot) {
    f32 angle;
    f32 step[2]; /* On x, then z */
    s32 aheadHeight;
    s32 behindHeight;

    angle = (u16)shot->unk0 * BINARY_ANGLE_TO_RADIANS;
    step[0] = cosf(angle) * AIM_SLOPE_STEP;
    step[1] = sinf(angle) * AIM_SLOPE_STEP;
    aheadHeight = GetGroundHeight((s32)step[0] / 2 + player->ballPosition[AXIS_X],
                                  (s32)step[1] / 2 + player->ballPosition[AXIS_Z]);
    if (aheadHeight < player->ballPosition[AXIS_Y] - AIM_SLOPE_MAX_RISE) {
        aheadHeight = player->ballPosition[AXIS_Y] - AIM_SLOPE_MAX_RISE;
    }
    if (aheadHeight > player->ballPosition[AXIS_Y] + AIM_SLOPE_MAX_RISE) {
        aheadHeight = player->ballPosition[AXIS_Y] + AIM_SLOPE_MAX_RISE;
    }
    behindHeight =
        GetGroundHeight(player->ballPosition[AXIS_X] - (s32)step[0], player->ballPosition[AXIS_Z] - (s32)step[1]);
    if (behindHeight < player->ballPosition[AXIS_Y] - AIM_SLOPE_MAX_RISE) {
        behindHeight = player->ballPosition[AXIS_Y] - AIM_SLOPE_MAX_RISE;
    }
    if (behindHeight > player->ballPosition[AXIS_Y] + AIM_SLOPE_MAX_RISE) {
        behindHeight = player->ballPosition[AXIS_Y] + AIM_SLOPE_MAX_RISE;
    }
    return atan2(aheadHeight - behindHeight, AIM_SLOPE_STEP * 1.5) * RADIANS_TO_BINARY_ANGLE;
}

/* Unreferenced; the original has it, and its zero is in the ROM */
static const f64 sUnusedZero = 0.0;

void func_80205AE4(s32, s32);
void func_80216130(s32);
void func_80216B74(s32);
/* menu.c returns an s8, but the original calls it without truncating, as if undeclared */
s32 func_8005DE40();
void ChooseFlightCameraKind(void);
void PlaceFlightCamera(s32 step);
s32 StepHoleFlyover(void);

/* The surface under the player's ball */
SurfaceInfo *GetBallSurfaceInfo(UnkB8 *player) {
    return GetSurfaceInfoAt(player->ballPosition[AXIS_X], player->ballPosition[AXIS_Z]);
}

s32 IsDebugBButtonView(void) {
    return sFlightCameraKind == FLIGHT_CAMERA_BESIDE_GOLFER;
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
    StartHudAnimation(HUD_ANIMATION_NONE);
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

/* Random yaws come in steps of a 32768th of a turn */
#define RANDOM_YAW_RANGE 0x7FFF
#define RANDOM_YAW_TO_RADIANS (M_PI / 16384)
/* The shortest shot length that the cameras work with */
#define MIN_SHOT_LENGTH 0x7800
/* The path steps searched for cup passes and for path flags */
#define SCANNED_STEP_COUNT 30
/* The ground is probed at five points, 45 degrees apart, around the ball */
#define GROUND_PROBE_COUNT 5
#define GROUND_PROBE_ANGLE_STEP (M_PI / 4)
#define GROUND_PROBE_DISTANCE 30720.0f
#define GROUND_PROBE_MAX_RISE 4608.0f

extern s32 D_800BE620;
extern s32 D_800BE624;
extern s32 D_800BE650;
extern f32 D_800BE65C;
extern s8 sPendingResultSound;
extern s32 D_800BE678;
extern s32 D_800BE67C;
extern s32 D_800BE680;
extern f32 D_800BE684;
extern CoursePoint sShotStartPosition;
extern f32 sEndYawFromCup;
extern f32 sStartYawFromCup;
extern s32 D_800DAF20;
extern s32 sGreenSoundCount;
extern s32 sIsLandingSoundPlayed;
extern s32 D_800DAF44;
extern s8 sPassedPathFlags;
extern s8 D_801061BF;
extern s8 D_801061C0;
extern s8 D_801061C1;
extern s8 D_801061C2;
extern s8 D_801061C3;
extern s8 D_801061C4;
extern s8 D_801061C5;
extern s8 D_801061CF;
extern u16 D_8010623A;
extern s32 D_802246F8;

void func_8020E7B4(s32);

/*
 * Starts the flight camera for the simulated shot. It resets the flight state, records the
 * shot's start and end points and their yaws from the cup, looks for cup passes and
 * flagged steps at the start of the path, and probes the ground on one side of the ball.
 * Then it picks the first camera kind and moves the main camera to it.
 * earlierFlightCount is ovl_00's count of the earlier calls; on the first, the camera picks
 * are shuffled again.
 */
void StartFlightCamera(s32 earlierFlightCount) {
    CoursePoint probe;
    s32 i;
    f32 yaw;
    f32 degrees;

    D_800BE624 = earlierFlightCount;
    func_80216B74(1);
    ResetBallTrail();
    D_800BE674 = 0;
    sEndSurfaceInfo = GetSurfaceInfo(g_SimulatedShot.endSurface);
    if (earlierFlightCount == 0) {
        NextCameraPick(FALSE);
    }
    StartHudAnimation(HUD_ANIMATION_NONE);
    func_8020E7B4(-1);
    sFlightCameraFlags = 0;
    /* The separate tests are there to match */
    if (earlierFlightCount == 0) {
        sFlightCameraFlags |= FLIGHT_CAMERA_FLAG_UNK2;
    } else if (RandomBelow(2) == 0) {
        sFlightCameraFlags |= FLIGHT_CAMERA_FLAG_UNK2;
    }
    if (earlierFlightCount == 0) {
        g_CapturedSliceCount = 0;
    }
    g_InsetView.animation = INSET_VIEW_STILL;
    g_InsetView.step = 0;
    g_Cameras[INSET_CAMERA].view.drawFlags = 0;
    /* The nested ifs are there to match */
    if (!g_SimulatedShot.unk2E08) {
        if (earlierFlightCount != 0 && D_802246F8 == -1) {
            D_801061CF = TRUE;
        } else {
            D_801061CF = FALSE;
        }
    } else {
        D_801061CF = FALSE;
    }
    g_BallFlightStep = 0;
    D_800BE620 = 0;
    D_800BE628 = FALSE;
    sCupViewHoldCount = 0;
    sGreenSoundCount = 0;
    sIsLandingSoundPlayed = FALSE;
    D_800DAF44 = 0;
    sEndReaction = END_REACTION_NONE;
    sCameraYaw = (f32)RandomBelow(RANDOM_YAW_RANGE) * RANDOM_YAW_TO_RADIANS;
    D_800BE65C = (f32)(rand() & RANDOM_YAW_RANGE) * RANDOM_YAW_TO_RADIANS;
    D_800BE684 = (f32)(rand() & RANDOM_YAW_RANGE) * RANDOM_YAW_TO_RADIANS;
    if (earlierFlightCount == 0) {
        D_800DAF20 = FALSE;
    } else {
        D_800DAF20 = TRUE;
    }
    D_800BE678 = 0;
    if (g_SimulatedShot.unk2E08) {
        D_800BE678 = 1;
    } else if (sEndSurfaceInfo->kind == SURFACE_KIND_WATER_HAZARD) {
        D_800BE678 = 3;
    }
    sPassedPathFlags = 0;
    sPendingResultSound = RESULT_SOUND_NONE;
    D_800BE67C = 1;
    if (D_800DAF20) {
        D_800BE67C = RandomBelow(2);
        D_801061C4 = D_800BE67C;
        D_801061BF = D_800BE67C;
        D_801061C0 = D_800BE67C;
        D_801061C1 = D_800BE67C;
        D_801061C2 = D_800BE67C;
        D_801061C3 = D_800BE67C;
        D_801061C5 = D_800BE67C;
    }
    /* Converting through f32, and writing the start point first, are there to match */
    sShotStartPosition[AXIS_X] = (f32)g_SimulatedShot.startPosition[AXIS_X];
    sShotStartPosition[AXIS_Y] = (f32)g_SimulatedShot.startPosition[AXIS_Y];
    sShotStartPosition[AXIS_Z] = (f32)g_SimulatedShot.startPosition[AXIS_Z];
    sShotEndPosition[AXIS_X] = (f32)g_SimulatedShot.endPosition[AXIS_X];
    sShotEndPosition[AXIS_Y] = (f32)g_SimulatedShot.endPosition[AXIS_Y];
    sShotEndPosition[AXIS_Z] = (f32)g_SimulatedShot.endPosition[AXIS_Z];
    sShotFlightYaw = -atan2((f32)(sShotEndPosition[AXIS_X] - sShotStartPosition[AXIS_X]),
                            (f32)(sShotEndPosition[AXIS_Z] - sShotStartPosition[AXIS_Z]));
    sEndYawFromCup = -atan2((f32)(sShotEndPosition[AXIS_X] - D_800BA9D8.cupPosition[AXIS_X]),
                            (f32)(sShotEndPosition[AXIS_Z] - D_800BA9D8.cupPosition[AXIS_Z]));
    sStartYawFromCup = -atan2((f32)(sShotStartPosition[AXIS_X] - D_800BA9D8.cupPosition[AXIS_X]),
                              (f32)(sShotStartPosition[AXIS_Z] - D_800BA9D8.cupPosition[AXIS_Z]));
    D_800BE644 = GetCupDistance(sShotStartPosition[AXIS_X] - D_800BA9D8.cupPosition[AXIS_X],
                                sShotStartPosition[AXIS_Z] - D_800BA9D8.cupPosition[AXIS_Z]);
    D_800BE640 = IntLength3D(sShotStartPosition[AXIS_X] - sShotEndPosition[AXIS_X],
                             sShotStartPosition[AXIS_Y] - sShotEndPosition[AXIS_Y],
                             sShotStartPosition[AXIS_Z] - sShotEndPosition[AXIS_Z]);
    if (D_800BE640 < MIN_SHOT_LENGTH) {
        D_800BE640 = MIN_SHOT_LENGTH;
    }
    sLandingCupDistance = GetCupDistance(sShotEndPosition[AXIS_X] - D_800BA9D8.cupPosition[AXIS_X],
                                         sShotEndPosition[AXIS_Z] - D_800BA9D8.cupPosition[AXIS_Z]);
    D_800BE658 = 0;
    /* The != tests are there to match */
    for (i = 0; i != SCANNED_STEP_COUNT; i++) {
        if (i < g_SimulatedShot.stepCount
            && IsCupPassPoint(IntLength2D(g_SimulatedShot.path[i][AXIS_X] - D_800BA9D8.cupPosition[AXIS_X],
                                          g_SimulatedShot.path[i][AXIS_Z] - D_800BA9D8.cupPosition[AXIS_Z]),
                              g_SimulatedShot.path[i][AXIS_Y])) {
            D_800BE658 |= 2;
        }
    }
    D_800BE648 = FALSE;
    D_800BE64C = -1;
    D_800BE650 = -1;
    for (i = 0; i != SCANNED_STEP_COUNT; i++) {
        if (i < g_SimulatedShot.stepCount && (g_SimulatedShot.pathFlags[i] & 2)) {
            D_800BE648 = TRUE;
            D_800BE658 |= 1;
            if (D_800BE64C == -1) {
                D_800BE64C = i;
                D_800BE650 = 40;
            }
        }
    }
    D_800BE690 = TRUE;
    for (i = 0; i != GROUND_PROBE_COUNT; i++) {
        if (g_Players[g_CurrentShot.playerIndex].unk22 == 1) {
            yaw = (u16)g_CurrentShot.unk0 * BINARY_ANGLE_TO_RADIANS + (f32)i * GROUND_PROBE_ANGLE_STEP;
        } else {
            yaw = (u16)g_CurrentShot.unk0 * BINARY_ANGLE_TO_RADIANS - (f32)i * GROUND_PROBE_ANGLE_STEP;
        }
        probe[AXIS_X] = cosf(yaw) * GROUND_PROBE_DISTANCE + g_CurrentShot.ballPosition[AXIS_X];
        probe[AXIS_Z] = sinf(yaw) * GROUND_PROBE_DISTANCE + g_CurrentShot.ballPosition[AXIS_Z];
        probe[AXIS_Y] = GetGroundHeight(probe[AXIS_X], probe[AXIS_Z]);
        if (probe[AXIS_Y] < g_CurrentShot.ballPosition[AXIS_Y] - GROUND_PROBE_MAX_RISE) {
            D_800BE690 = FALSE;
        }
        if (probe[AXIS_Y] > g_CurrentShot.ballPosition[AXIS_Y] + GROUND_PROBE_MAX_RISE) {
            D_800BE690 = FALSE;
        }
    }
    degrees = D_8010623A * (360.0f * BINARY_ANGLE_TO_RADIANS) * (1.0 / (2 * M_PI));
    if (degrees < 350.0f && degrees > 10.0f) {
        D_800BE690 = FALSE;
    }
    D_800BE628 = FALSE;
    D_800BE630 = FALSE;
    D_800DAF50 = 1;
    sFlightCameraKind = NO_FLIGHT_CAMERA;
    D_800BE680 = 0;
    g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
    sFlightBallPosition[AXIS_X] = sShotStartPosition[AXIS_X];
    sFlightBallPosition[AXIS_Y] = sShotStartPosition[AXIS_Y];
    sFlightBallPosition[AXIS_Z] = sShotStartPosition[AXIS_Z];
    ChooseFlightCameraKind();
    D_800BE628 = TRUE;
    PlaceFlightCamera(0);
    g_Cameras[0].view.eye[AXIS_X] = sCameraPosition[AXIS_X];
    g_Cameras[0].view.eye[AXIS_Y] = sCameraPosition[AXIS_Y];
    g_Cameras[0].view.eye[AXIS_Z] = sCameraPosition[AXIS_Z];
    g_Cameras[0].view.lookAt[AXIS_X] = sLookAtPoint[AXIS_X];
    g_Cameras[0].view.lookAt[AXIS_Y] = sLookAtPoint[AXIS_Y];
    g_Cameras[0].view.lookAt[AXIS_Z] = sLookAtPoint[AXIS_Z];
}

/* The sounds of the club meeting the ball */
#define CONTACT_SOUND_WOOD_UNK50 0x50 /* Off a tee, or on some power shots */
#define CONTACT_SOUND_WOOD 0x52
#define CONTACT_SOUND_IRON_TEE 0x53
#define CONTACT_SOUND_IRON 0x54
#define CONTACT_SOUND_IRON_BUNKER 0x56
#define CONTACT_SOUND_PUTTER 0x57
#define CONTACT_SOUND_CHANNEL 2
#define CONTACT_SOUND_VOLUME 0x7F

/* The clubs in slots below it get func_80213D70(1) on some power shots */
#define LEADING_CLUB_SLOT_COUNT 6

void func_80213D70(s32);
void func_80216CAC(void);

/* These are macros, as inline functions compile differently from the original */
#define IsUnk2E14PowerShot() (g_SimulatedShot.shot.shotMode == SHOT_MODE_POWER && g_SimulatedShot.unk2E14)
#define IsFairwayKind(kind)                                                        \
    ((kind) == SURFACE_KIND_FAIRWAY || (kind) == SURFACE_KIND_FAIRWAY_FAIR_LIE     \
     || (kind) == SURFACE_KIND_FAIRWAY_POOR_LIE)
#define IsBunkerKind(kind)                                                         \
    ((kind) == SURFACE_KIND_BUNKER || (kind) == SURFACE_KIND_BUNKER_FAIR_LIE       \
     || (kind) == SURFACE_KIND_BUNKER_POOR_LIE)

/* Plays the sound of the simulated shot's club meeting the ball */
void PlayContactSound(void) {
    Club *club;
    SurfaceInfo *surfaceInfo;

    club = GetClub(g_Players[g_SimulatedShot.shot.unk18].clubs[g_SimulatedShot.shot.clubIndex]);
    surfaceInfo = GetSurfaceInfo(g_CurrentShot.surface);
    if (club->kind == CLUB_KIND_WOOD) {
        if (IsUnk2E14PowerShot()) {
            PlaySound(CONTACT_SOUND_WOOD_UNK50, CONTACT_SOUND_CHANNEL, CONTACT_SOUND_VOLUME);
            func_80213D70(0);
        } else if (surfaceInfo->kind == SURFACE_KIND_TEE_UP) {
            func_80213D70(2);
            PlaySound(CONTACT_SOUND_WOOD_UNK50, CONTACT_SOUND_CHANNEL, CONTACT_SOUND_VOLUME);
        } else {
            func_80213D70(2);
            PlaySound(CONTACT_SOUND_WOOD, CONTACT_SOUND_CHANNEL, CONTACT_SOUND_VOLUME);
        }
        func_80216CAC();
    } else if (club->kind == CLUB_KIND_IRON || club->kind == CLUB_KIND_WEDGE) {
        if (IsUnk2E14PowerShot()) {
            if (g_CurrentShot.clubSlot < LEADING_CLUB_SLOT_COUNT) {
                func_80213D70(1);
            } else {
                func_80213D70(2);
            }
        } else {
            func_80213D70(2);
        }
        if (surfaceInfo->kind == SURFACE_KIND_TEE_UP) {
            PlaySound(CONTACT_SOUND_IRON_TEE, CONTACT_SOUND_CHANNEL, CONTACT_SOUND_VOLUME);
        } else if (IsFairwayKind(surfaceInfo->kind)) {
            PlaySound(CONTACT_SOUND_IRON, CONTACT_SOUND_CHANNEL, CONTACT_SOUND_VOLUME);
        } else if (IsBunkerKind(surfaceInfo->kind)) {
            PlaySound(CONTACT_SOUND_IRON_BUNKER, CONTACT_SOUND_CHANNEL, CONTACT_SOUND_VOLUME);
        } else {
            PlaySound(CONTACT_SOUND_IRON, CONTACT_SOUND_CHANNEL, CONTACT_SOUND_VOLUME);
        }
    } else {
        PlaySound(CONTACT_SOUND_PUTTER, CONTACT_SOUND_CHANNEL, CONTACT_SOUND_VOLUME);
        if (sFlightCameraKind != FLIGHT_CAMERA_FRAMING_CUP) {
            func_80213D70(3);
        }
    }
}

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

/* PlayFlightSounds' sounds, by what they follow where that is clear */
#define FLIGHT_SOUND_UNK17 0x17
#define FLIGHT_SOUND_UNK67 0x67
#define FLIGHT_SOUND_UNK74 0x74
#define FLIGHT_SOUND_UNK75 0x75
#define FLIGHT_SOUND_UNK76 0x76
#define FLIGHT_SOUND_FULL_SHOT 0x7B
#define FLIGHT_SOUND_TROUBLE_LANDING 0x7C /* In a bunker, water or out of play */
#define FLIGHT_SOUND_UNK7E 0x7E
#define FLIGHT_SOUND_UNK7F 0x7F
#define FLIGHT_SOUND_UNK80 0x80
#define FLIGHT_SOUND_UNK81 0x81
#define FLIGHT_SOUND_UNK82 0x82
#define FLIGHT_SOUND_OUT_OF_COURSE 0x83
#define FLIGHT_SOUND_UNK91 0x91
#define FLIGHT_SOUND_UNK92 0x92
#define FLIGHT_SOUND_UNK93 0x93
#define FLIGHT_SOUND_UNK97 0x97
#define FLIGHT_SOUND_DEEP_ROUGH_LANDING 0x99
#define FLIGHT_SOUND_WASTE_AREA_LANDING 0x9A
#define FLIGHT_SOUND_UNK9B 0x9B
#define FLIGHT_SOUND_CHANNEL_UNK3 3
#define FLIGHT_SOUND_CHANNEL_UNKE 0xE
#define FLIGHT_SOUND_VOLUME 0x7F
#define FLIGHT_SOUND_VOLUME_UNK50 0x50
#define FLIGHT_SOUND_VOLUME_UNK60 0x60
#define FLIGHT_SOUND_VOLUME_UNK64 0x64
#define GREEN_SOUND_VOLUME 0x5F
#define FAIRWAY_SOUND_VOLUME 0x3F
#define FULL_SHOT_REVERB 0x14

/* The steps of the flight at which PlayFlightSounds plays some sounds */
#define UNK2E42_SOUND_STEP 10
#define FULL_SHOT_REACTION_STEP 14
#define SHOT_RESULT_STEP 20
/* Most sounds wait for the flight to pass its first steps */
#define EARLY_STEP_COUNT 10
/* Sounds that play this many steps before the end of the flight */
#define LANDING_SOUND_STEP_COUNT 3
#define UNK92_SOUND_STEP_COUNT 10
#define UNKB0_SOUND_STEP_COUNT 15

/* Bits of g_SimulatedShot.pathFlags */
#define PATH_FLAG_UNK1 (1 << 0)
#define PATH_FLAG_UNK2 (1 << 1)
#define PATH_FLAG_UNK4 (1 << 2)
#define PATH_FLAG_UNK8 (1 << 3)

/* Distances that PlayFlightSounds tests, in course units */
#define CLOSE_LANDING_DISTANCE 0x3C00
#define NEAR_LANDING_DISTANCE 0x5A00
#define GREEN_SOUND_MAX_DISTANCE 0x7800
#define LONG_PUTT_MIN_DISTANCE 0x25800
#define LONG_SHOT_SOUND_MIN_DISTANCE 0x4B000
#define LONG_LANDING_MIN_DISTANCE (60.0f * UNITS_PER_YARD)

extern s8 D_8010623F;

void func_8020D874(f32);

/* Queues a character sound of the given kind for each player but the one who shot */
static inline void QueueOnlookerSounds(s32 kind) {
    s32 i;

    for (i = 0; i != g_PlayerCount; i++) {
        if (i != g_CurrentShot.playerIndex) {
            QueueCharacterSound((s8)g_Players[i].characterIndex, i, kind, FLIGHT_SOUND_VOLUME,
                                g_Players[i].soundPan);
        }
    }
}

/*
 * Plays the sounds and the other players' reactions that go with the ball's
 * flight: after a full shot, on landing in trouble and, at the end of the
 * flight, by the shooter's stroke count against par. step is
 * g_BallFlightStep, which the caller clamps to the recorded path.
 */
void PlayFlightSounds(s32 step) {
    s32 shownRingCount;
    s32 scoreToPar;
    s32 i;
    SurfaceInfo *info;
    UnkB8 *player;
    s32 *greenSoundCount; /* The pointers are there to match */
    s32 *isPlayed;
    s32 *isStillPlayed;

    scoreToPar = g_Players[g_CurrentShot.playerIndex].strokeCount - g_HolePar;
    shownRingCount = 0;
    if (g_GameMode == GAME_MODE_RING_SHOT) {
        player = &g_Players[D_8010623F];
        for (i = 0; i != RING_COUNT; i++) {
            shownRingCount += player->ringStates[i] == RING_STATE_SHOWN;
        }
    }
    sPassedPathFlags |= g_SimulatedShot.pathFlags[step];
    if (step == FULL_SHOT_REACTION_STEP && RateShotLength() == SHOT_LENGTH_FULL) {
        QueueOnlookerSounds(0);
    }
    if (step == SHOT_RESULT_STEP) {
        if (sEndSurfaceInfo->kind == SURFACE_KIND_OUT_OF_COURSE && !g_IsMiniGolf) {
            PlaySound(FLIGHT_SOUND_OUT_OF_COURSE, FLIGHT_SOUND_CHANNEL_UNK3, FLIGHT_SOUND_VOLUME);
            QueueOnlookerSounds(4);
        } else if (RateShotLength() == SHOT_LENGTH_FULL) {
            PlaySoundWithEffects(FLIGHT_SOUND_FULL_SHOT, FLIGHT_SOUND_CHANNEL_UNK3, FLIGHT_SOUND_VOLUME,
                                 FULL_SHOT_REVERB, g_Players[g_CurrentShot.playerIndex].soundPan);
        }
    }
    if (step == UNK2E42_SOUND_STEP && g_SimulatedShot.unk2E42) {
        /* Plays the sound again for each player; the original has it */
        for (i = 0; i != g_PlayerCount; i++) {
            PlaySound(FLIGHT_SOUND_UNK97, FLIGHT_SOUND_CHANNEL_UNK3, FLIGHT_SOUND_VOLUME);
            if (i != g_CurrentShot.playerIndex) {
                QueueCharacterSound((s8)g_Players[i].characterIndex, i, 5, FLIGHT_SOUND_VOLUME,
                                    g_Players[i].soundPan);
            }
        }
    }
    if (step > EARLY_STEP_COUNT) {
        greenSoundCount = &sGreenSoundCount;
        if (!*greenSoundCount && !shownRingCount && (g_SimulatedShot.pathFlags[step - 2] & PATH_FLAG_UNK1)
            && !g_SimulatedShot.unk2E0C && !g_SimulatedShot.unk2E08 && g_SimulatedShot.endsOnGreen
            && sEndSurfaceInfo->kind == SURFACE_KIND_GREEN) {
            if (g_HolePar > g_Players[g_CurrentShot.playerIndex].strokeCount + 1) {
                (*greenSoundCount)++;
                PlaySound(FLIGHT_SOUND_UNK75, FLIGHT_SOUND_CHANNEL_UNKB, GREEN_SOUND_VOLUME);
            } else if (g_HolePar > g_Players[g_CurrentShot.playerIndex].strokeCount + 2
                       && sLandingCupDistance < GREEN_SOUND_MAX_DISTANCE) {
                (*greenSoundCount)++;
                PlaySound(FLIGHT_SOUND_UNK76, FLIGHT_SOUND_CHANNEL_UNKB, GREEN_SOUND_VOLUME);
            }
        }
    }
    if (D_800BE620 == RESULT_SOUND_STAGE) {
        if (sPendingResultSound == RESULT_SOUND_UNK1) {
            PlaySound(FLIGHT_SOUND_UNK17, FLIGHT_SOUND_CHANNEL_UNKE, FLIGHT_SOUND_VOLUME);
        } else if (sPendingResultSound == RESULT_SOUND_UNK2) {
            PlaySound(FLIGHT_SOUND_UNK67, FLIGHT_SOUND_CHANNEL_UNKE, FLIGHT_SOUND_VOLUME);
        }
    }
    if (!g_IsMiniGolf) {
        if ((g_SimulatedShot.pathFlags[step + 5] & PATH_FLAG_UNK8) && g_SimulatedShot.unkB0Step == NO_STEP) {
            D_800BE680 = 1;
        }
        if ((g_SimulatedShot.pathFlags[step] & PATH_FLAG_UNK8) && !g_SimulatedShot.unk2E10
            && g_SimulatedShot.unkB0Step == NO_STEP) {
            D_800BE680 = 0;
            func_8020D874(D_800BE684);
        }
    }
    if (!shownRingCount && g_BallFlightStep == g_SimulatedShot.stepCount - 1 && !g_SimulatedShot.unk2E10) {
        if (g_SimulatedShot.unk2E08) {
            if (sLandingCupDistance < CLOSE_LANDING_DISTANCE && D_800BE644 > LONG_PUTT_MIN_DISTANCE
                && !(sPassedPathFlags & (PATH_FLAG_UNK4 | PATH_FLAG_UNK8))
                && g_HolePar > g_Players[g_CurrentShot.playerIndex].strokeCount) {
                func_8020E7B4(3);
                PlaySound(FLIGHT_SOUND_UNK82, FLIGHT_SOUND_CHANNEL_UNK3, FLIGHT_SOUND_VOLUME);
            }
        } else if (g_SimulatedShot.unkB0Step != NO_STEP) {
            func_8020E7B4(6);
            PlaySound(FLIGHT_SOUND_UNK91, FLIGHT_SOUND_CHANNEL_UNK3, FLIGHT_SOUND_VOLUME);
            func_80205AE4(g_CurrentShot.playerIndex, 6);
            sPendingResultSound = RESULT_SOUND_UNK2;
        } else if (sPassedPathFlags & PATH_FLAG_UNK8) {
            func_80205AE4(g_CurrentShot.playerIndex, 2);
            func_8020E7B4(5);
            PlaySound(FLIGHT_SOUND_UNK93, FLIGHT_SOUND_CHANNEL_UNK3, FLIGHT_SOUND_VOLUME);
            sPendingResultSound = RESULT_SOUND_UNK2;
        } else if (g_SimulatedShot.endsOnGreen) {
            if (g_HolePar - 3 >= g_Players[g_CurrentShot.playerIndex].strokeCount) {
                func_80205AE4(g_CurrentShot.playerIndex, 0);
                func_8020E7B4(8);
                PlaySound(FLIGHT_SOUND_UNK7F, FLIGHT_SOUND_CHANNEL_UNK3, FLIGHT_SOUND_VOLUME);
                QueueOnlookerSounds(2);
                sPendingResultSound = RESULT_SOUND_UNK2;
            } else if (g_HolePar - 2 == g_Players[g_CurrentShot.playerIndex].strokeCount) {
                if (sLandingCupDistance <= CLOSE_LANDING_DISTANCE) {
                    func_80205AE4(g_CurrentShot.playerIndex, 1);
                    func_8020E7B4(7);
                    PlaySound(FLIGHT_SOUND_UNK7E, FLIGHT_SOUND_CHANNEL_UNK3, FLIGHT_SOUND_VOLUME);
                    QueueOnlookerSounds(1);
                    sPendingResultSound = RESULT_SOUND_UNK1;
                } else if (g_SimulatedShot.shot.shotMode == SHOT_MODE_APPROACH || g_SimulatedShot.unk2E0C) {
                    if (sLandingCupDistance <= NEAR_LANDING_DISTANCE) {
                        func_8020E7B4(2);
                        PlaySound(FLIGHT_SOUND_UNK81, FLIGHT_SOUND_CHANNEL_UNK3, FLIGHT_SOUND_VOLUME);
                        sPendingResultSound = RESULT_SOUND_UNK1;
                    } else {
                        func_8020E7B4(1);
                        PlaySound(FLIGHT_SOUND_UNK80, FLIGHT_SOUND_CHANNEL_UNK3, FLIGHT_SOUND_VOLUME);
                    }
                } else {
                    func_8020E7B4(1);
                    PlaySound(FLIGHT_SOUND_UNK80, FLIGHT_SOUND_CHANNEL_UNK3, FLIGHT_SOUND_VOLUME);
                }
            } else if (g_HolePar - 1 == g_Players[g_CurrentShot.playerIndex].strokeCount
                       && (g_SimulatedShot.shot.shotMode == SHOT_MODE_APPROACH || g_SimulatedShot.unk2E0C)
                       && sLandingCupDistance <= NEAR_LANDING_DISTANCE) {
                func_8020E7B4(2);
                PlaySound(FLIGHT_SOUND_UNK81, FLIGHT_SOUND_CHANNEL_UNK3, FLIGHT_SOUND_VOLUME);
            }
        }
    }
    if (g_SimulatedShot.unk2E10) {
        if (g_BallFlightStep >= g_SimulatedShot.stepCount - UNKB0_SOUND_STEP_COUNT && sEndReaction == END_REACTION_NONE
            && g_SimulatedShot.unkB0Step != NO_STEP) {
            func_8020E7B4(6);
            PlaySound(FLIGHT_SOUND_UNK91, FLIGHT_SOUND_CHANNEL_UNK3, FLIGHT_SOUND_VOLUME);
            QueueOnlookerSounds(1);
            func_80205AE4(g_CurrentShot.playerIndex, 6);
            sEndReaction = END_REACTION_ONLOOKERS;
            sPendingResultSound = RESULT_SOUND_UNK2;
        }
        if (g_BallFlightStep >= g_SimulatedShot.stepCount - UNK92_SOUND_STEP_COUNT && sEndReaction == END_REACTION_NONE
            && !g_SimulatedShot.unk2E08
            && g_Players[g_CurrentShot.playerIndex].strokeCount >= 2 && g_SimulatedShot.unkB0Step == NO_STEP) {
            func_8020E7B4(4);
            PlaySound(FLIGHT_SOUND_UNK92, FLIGHT_SOUND_CHANNEL_UNK3, FLIGHT_SOUND_VOLUME);
            QueueOnlookerSounds(2);
            sEndReaction = END_REACTION_ONLOOKERS;
        }
    }
    isPlayed = &sIsLandingSoundPlayed;
    if (!*isPlayed) {
        if (g_BallFlightStep == g_SimulatedShot.stepCount && !shownRingCount && g_SimulatedShot.unk2E10) {
            *isPlayed = TRUE;
            if (IsGameFlagSet(GAME_FLAG_UNK48)) {
                if (D_800BE69C == 1 && (scoreToPar >= -2 && scoreToPar <= 1) && !IsPinInCup()) {
                    StartMovie(MOVIE_UNK5);
                }
            } else if (scoreToPar >= -2 && scoreToPar <= 0 && !IsPinInCup() && D_800BE69C == 1) {
                if (g_SimulatedShot.shot.shotMode == SHOT_MODE_NORMAL) {
                    StartMovie(MOVIE_UNK16);
                } else if (g_SimulatedShot.shot.shotMode == SHOT_MODE_POWER) {
                    StartMovie(MOVIE_UNK5);
                }
            }
            if (g_SimulatedShot.unk2E08) {
                if (D_800BE644 > LONG_SHOT_SOUND_MIN_DISTANCE || IsGameFlagSet(GAME_FLAG_UNK48)) {
                    PlaySound(FLIGHT_SOUND_UNK9B, FLIGHT_SOUND_CHANNEL_UNK3, FLIGHT_SOUND_VOLUME);
                    sEndReaction = END_REACTION_UNK2;
                }
            }
            if (!g_SimulatedShot.unk2E08) {
                if (D_800BE644 > LONG_LANDING_MIN_DISTANCE) {
                    func_80205AE4(g_CurrentShot.playerIndex, 4);
                    sPendingResultSound = RESULT_SOUND_UNK2;
                } else {
                    func_80205AE4(g_CurrentShot.playerIndex, 3);
                    sPendingResultSound = RESULT_SOUND_UNK1;
                }
            }
            if (g_Players[g_CurrentShot.playerIndex].strokeCount == 1) {
                PlaySound(FLIGHT_SOUND_UNK76, FLIGHT_SOUND_CHANNEL_UNKF, FLIGHT_SOUND_VOLUME);
            } else if (g_HolePar - 3 == g_Players[g_CurrentShot.playerIndex].strokeCount) {
                PlaySound(FLIGHT_SOUND_UNK76, FLIGHT_SOUND_CHANNEL_UNKF, FLIGHT_SOUND_VOLUME_UNK64);
            } else if (g_HolePar - 2 == g_Players[g_CurrentShot.playerIndex].strokeCount) {
                PlaySound(FLIGHT_SOUND_UNK76, FLIGHT_SOUND_CHANNEL_UNKF, FLIGHT_SOUND_VOLUME_UNK50);
            } else if (g_HolePar - 1 == g_Players[g_CurrentShot.playerIndex].strokeCount) {
                PlaySound(FLIGHT_SOUND_UNK74, FLIGHT_SOUND_CHANNEL_UNKF, FLIGHT_SOUND_VOLUME_UNK60);
            }
        }
        isStillPlayed = &sIsLandingSoundPlayed;
        if (!*isStillPlayed && g_BallFlightStep > EARLY_STEP_COUNT
            && (g_SimulatedShot.pathFlags[step - EARLY_STEP_COUNT] & PATH_FLAG_UNK1)
            && GetSelectedShotDistance() * FULL_SHOT_FRACTION < D_800BE640 * YARDS_PER_UNIT
            && (info = GetSurfaceInfo(g_BallSurface), IsFairwayKind(info->kind)) && !g_IsMiniGolf
            && D_800BE644 > sLandingCupDistance
            && g_HolePar - 3 >= g_Players[g_CurrentShot.playerIndex].strokeCount) {
            *isStillPlayed = TRUE;
            PlaySound(FLIGHT_SOUND_UNK75, FLIGHT_SOUND_CHANNEL_UNKB, FAIRWAY_SOUND_VOLUME);
        }
    }
    if (g_BallFlightStep > EARLY_STEP_COUNT
        && g_BallFlightStep == g_SimulatedShot.stepCount - LANDING_SOUND_STEP_COUNT) {
        if (IsBunkerKind(sEndSurfaceInfo->kind)) {
            sIsLandingSoundPlayed = TRUE;
            PlaySound(FLIGHT_SOUND_TROUBLE_LANDING, FLIGHT_SOUND_CHANNEL_UNKB, FLIGHT_SOUND_VOLUME);
            QueueOnlookerSounds(6);
        } else if (sEndSurfaceInfo->kind == SURFACE_KIND_DEEP_ROUGH) {
            PlaySound(FLIGHT_SOUND_DEEP_ROUGH_LANDING, FLIGHT_SOUND_CHANNEL_UNKB, FLIGHT_SOUND_VOLUME);
            QueueOnlookerSounds(6);
        } else if (sEndSurfaceInfo->kind == SURFACE_KIND_WASTE_AREA) {
            PlaySound(FLIGHT_SOUND_WASTE_AREA_LANDING, FLIGHT_SOUND_CHANNEL_UNKB, FLIGHT_SOUND_VOLUME);
        } else if (sEndSurfaceInfo->kind == SURFACE_KIND_WATER_HAZARD
                   || sEndSurfaceInfo->kind == SURFACE_KIND_OUT_OF_BOUNDS
                   || sEndSurfaceInfo->kind == SURFACE_KIND_OUT_OF_COURSE) {
            sIsLandingSoundPlayed = TRUE;
            PlaySound(FLIGHT_SOUND_TROUBLE_LANDING, FLIGHT_SOUND_CHANNEL_UNKB, FLIGHT_SOUND_VOLUME);
            QueueOnlookerSounds(5);
        }
    }
}

/* It is a macro, as an inline function compiles differently from the original */
#define IsBallLanding()                                                            \
    (g_SimulatedShot.stepCount < g_BallFlightStep + LANDING_STEP_COUNT             \
     && (f32)sShotEndPosition[AXIS_X]                                              \
            < (f32)(sFlightBallPosition[AXIS_X] + LANDING_RADIUS)                  \
     && (f32)(sFlightBallPosition[AXIS_X] - LANDING_RADIUS)                        \
            < (f32)sShotEndPosition[AXIS_X]                                        \
     && (f32)sShotEndPosition[AXIS_Z]                                              \
            < (f32)(sFlightBallPosition[AXIS_Z] + LANDING_RADIUS)                  \
     && (f32)(sFlightBallPosition[AXIS_Z] - LANDING_RADIUS)                        \
            < (f32)sShotEndPosition[AXIS_Z])

/*
 * Eases the look-at point toward the ball, unless it is landing.
 * It does not match: the original keeps the loads of the ball's and the
 * look-at point's coordinates in statement order, where this schedules the
 * ball's loads first.
 */
#ifdef NON_MATCHING
void EaseLookAtFlyingBall(void) {
    if (!IsBallLanding()) {
        sLookAtPoint[AXIS_X] +=
            (sFlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE - sLookAtPoint[AXIS_X]) * LOOK_AT_EASE_FRACTION;
        sLookAtPoint[AXIS_Y] +=
            (sFlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE - sLookAtPoint[AXIS_Y]) * LOOK_AT_HEIGHT_EASE_FRACTION;
        sLookAtPoint[AXIS_Z] +=
            (sFlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE - sLookAtPoint[AXIS_Z]) * LOOK_AT_EASE_FRACTION;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/shot_camera", EaseLookAtFlyingBall);
#endif

/*
 * Eases the look-at point toward a point drop below the ball, unless it is landing.
 * It does not match: the original keeps the loads of the ball's and the
 * look-at point's coordinates in statement order, where this schedules the
 * ball's loads first and copies drop to another register.
 */
#ifdef NON_MATCHING
void EaseLookAtBelowBall(f32 drop) {
    if (!IsBallLanding()) {
        sLookAtPoint[AXIS_X] +=
            (sFlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE - sLookAtPoint[AXIS_X]) * LOOK_AT_EASE_FRACTION;
        sLookAtPoint[AXIS_Y] += ((sFlightBallPosition[AXIS_Y] - drop) * COURSE_TO_WORLD_SCALE - sLookAtPoint[AXIS_Y])
                              * LOOK_AT_HEIGHT_EASE_FRACTION;
        sLookAtPoint[AXIS_Z] +=
            (sFlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE - sLookAtPoint[AXIS_Z]) * LOOK_AT_EASE_FRACTION;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/shot_camera", EaseLookAtBelowBall);
#endif

/*
 * Eases the look-at point toward the ball.
 * It ignores yaw, which PlaceFlightCamera passes.
 * It does not match: the original keeps the loads of the ball's and the
 * look-at point's coordinates in statement order, where this schedules the
 * ball's loads first.
 */
#ifdef NON_MATCHING
void EaseLookAtPoint(f32 yaw) {
    sLookAtPoint[AXIS_X] +=
        (sFlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE - sLookAtPoint[AXIS_X]) * LOOK_AT_EASE_FRACTION;
    sLookAtPoint[AXIS_Y] += (sFlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE - LOOK_AT_DROP - sLookAtPoint[AXIS_Y])
                          * LOOK_AT_HEIGHT_EASE_FRACTION;
    sLookAtPoint[AXIS_Z] +=
        (sFlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE - sLookAtPoint[AXIS_Z]) * LOOK_AT_EASE_FRACTION;
}
#else
INCLUDE_ASM("asm/nonmatchings/shot_camera", EaseLookAtPoint);
#endif

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

#define RANDOM_YAW_MASK 0x7FFF
#define UNK2E08_HEIGHT_SPREAD_FRACTION 0.2
#define DISTANCE_SPREAD_FRACTION 0.2f
#define DEFAULT_HEIGHT_SPREAD_FRACTION 0.1f
#define OPTION_UNK3_HEIGHT_SPREAD_FRACTION 0.2f
#define UNK2E08_MIN_HEIGHT 0x6600
#define RANDOM_DISTANCE_STEP_COUNT 1536
#define UNK2E08_DISTANCE_STEP 15
#define UNK2E08_MIN_DISTANCE 0xB400
#define SHORT_SHOT_MAX_LENGTH 0xBB7FF
#define SHORT_SHOT_DISTANCE 76800.0f
#define GREEN_DISTANCE_STEP 50
#define GREEN_MIN_DISTANCE 0x12C00
#define MIN_CAMERA_DISTANCE 76800.0f
#define MAX_CAMERA_DISTANCE 460800.0f
#define DEFAULT_MIN_HEIGHT 46080.0f
#define DEFAULT_MAX_HEIGHT 92160.0f
#define OPTION_UNK3_MIN_HEIGHT 92160.0f
#define OPTION_UNK3_MAX_HEIGHT 153600.0f
#define MIN_RISE_ABOVE_SHOT_END 0x7800

/*
 * Places the camera at a random yaw around the shot's end, a random distance away
 * and a random height above the ground there, then snaps the look-at point. The
 * ground point is kept at least MIN_RISE_ABOVE_SHOT_END above the shot's end.
 * Course y points down, so heights are subtracted.
 */
void PlaceCameraAroundShotEnd(void) {
    CoursePoint point;
    f32 height;
    f64 yawScale; /* Holding the scale in yawScale is there to match */
    f32 distance;
    f32 yaw;

    if (g_SimulatedShot.unk2E08) {
        height = RandomBelow(D_800BE640 * UNK2E08_HEIGHT_SPREAD_FRACTION) + UNK2E08_MIN_HEIGHT;
        distance =
            (guRandom() % RANDOM_DISTANCE_STEP_COUNT) * UNK2E08_DISTANCE_STEP + UNK2E08_MIN_DISTANCE;
        RandomBelow(RANDOM_YAW_MASK);
        yawScale = M_PI / 65536;
        yaw = sShotFlightYaw - (f32)(rand() & RANDOM_YAW_MASK) * yawScale;
    } else if (!func_8005DE40()) {
        if (D_800BE640 <= SHORT_SHOT_MAX_LENGTH) {
            distance = SHORT_SHOT_DISTANCE;
            height = 0.0f;
            yaw = sShotFlightYaw + M_PI / 8 - (f32)(rand() & RANDOM_YAW_MASK) * (M_PI / 131072);
        } else {
            if (g_SimulatedShot.endsOnGreen) {
                distance =
                    (guRandom() % RANDOM_DISTANCE_STEP_COUNT) * GREEN_DISTANCE_STEP + GREEN_MIN_DISTANCE;
            } else if (D_800BE640 * DISTANCE_SPREAD_FRACTION != 0.0f) {
                distance = (guRandom() % D_800BE640) * DISTANCE_SPREAD_FRACTION + MIN_CAMERA_DISTANCE;
            } else {
                distance = MIN_CAMERA_DISTANCE;
            }
            if (D_800BE640 * DEFAULT_HEIGHT_SPREAD_FRACTION != 0.0f) {
                height = (guRandom() % D_800BE640) * DEFAULT_HEIGHT_SPREAD_FRACTION + DEFAULT_MIN_HEIGHT;
            } else {
                height = DEFAULT_MIN_HEIGHT;
            }
            if (height > DEFAULT_MAX_HEIGHT) {
                height = DEFAULT_MAX_HEIGHT;
            }
            if (distance > MAX_CAMERA_DISTANCE) {
                distance = MAX_CAMERA_DISTANCE;
            }
            yaw = sShotFlightYaw + M_PI + M_PI * 5 / 8
                  + (f32)(rand() & RANDOM_YAW_MASK) * (M_PI / 32768 * 1.225f);
        }
    } else {
        if (D_800BE640 * DISTANCE_SPREAD_FRACTION != 0.0f) {
            distance = (guRandom() % D_800BE640) * DISTANCE_SPREAD_FRACTION + MIN_CAMERA_DISTANCE;
        } else {
            distance = MIN_CAMERA_DISTANCE;
        }
        if (D_800BE640 * OPTION_UNK3_HEIGHT_SPREAD_FRACTION != 0.0f) {
            height = (guRandom() % D_800BE640) * OPTION_UNK3_HEIGHT_SPREAD_FRACTION + OPTION_UNK3_MIN_HEIGHT;
        } else {
            height = OPTION_UNK3_MIN_HEIGHT;
        }
        if (height > OPTION_UNK3_MAX_HEIGHT) {
            height = OPTION_UNK3_MAX_HEIGHT;
        }
        if (distance > MAX_CAMERA_DISTANCE) {
            distance = MAX_CAMERA_DISTANCE;
        }
        yaw = sShotFlightYaw + M_PI + M_PI / 2 + (f32)(rand() & RANDOM_YAW_MASK) * (M_PI / 32768);
    }
    point[AXIS_X] = -sinf(yaw) * distance + sShotEndPosition[AXIS_X];
    point[AXIS_Z] = cosf(yaw) * distance + sShotEndPosition[AXIS_Z];
    point[AXIS_Y] = GetGroundHeight(point[AXIS_X], point[AXIS_Z]);
    if (point[AXIS_Y] > sShotEndPosition[AXIS_Y] - MIN_RISE_ABOVE_SHOT_END) {
        point[AXIS_Y] = sShotEndPosition[AXIS_Y] - MIN_RISE_ABOVE_SHOT_END;
    }
    point[AXIS_Y] = point[AXIS_Y] - height;
    sCameraPosition[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
    sCameraPosition[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    sCameraPosition[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    SnapLookAtPoint();
}

/* The songs of the holes */
#define HOLE_SONG_TOURNAMENT 0x0B
#define HOLE_SONG_FINAL 0x14 /* A hole that can decide the round */
#define HOLE_SONG_UNK18 0x18
#define HOLE_SONG_MATCH 0x19
#define HOLE_SONG_UNK1A 0x1A
#define HOLE_SONG_COURSE 0x1B
#define HOLE_SONG_RING_SHOT 0x1D
#define HOLE_SONG_UNK1F 0x1F
#define HOLE_SONG_CLUB_SLOTS 0x2B
#define HOLE_SONG_MINI_GOLF 0x2D
#define HOLE_SONG_SKINS_MATCH 0x2E


extern s8 D_801061B8;

/* Resets the cameras for the start of a hole, and plays the game mode's song */
void StartHoleCamerasAndSong(void) {
    SetMenuBackdrop(MENU_BACKDROP_NONE);
    g_IsHoleIntroTextShown = 1;
    D_801061B8 = -1;
    D_801061C0 = 1;
    D_801061C3 = 1;
    g_InsetView.animation = INSET_VIEW_STILL;
    g_InsetView.step = 0;
    g_HoleFlyoverFrame = 0;
    g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
    g_Cameras[1].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
    if (g_GameState.unk31) {
        PlaySong(HOLE_SONG_UNK1F, SONG_FLAG_UNK10 | SONG_FLAG_UNREAD);
        return;
    }
    switch (g_GameMode) {
        case GAME_MODE_UNK3:
            PlaySong(HOLE_SONG_UNK1A, SONG_FLAG_UNREAD);
            break;
        case GAME_MODE_MATCH_GAME:
        case GAME_MODE_GET_CHARACTER:
            if (GetMatchStanding(0, 1) != MATCH_STANDING_OPEN) {
                PlaySong(HOLE_SONG_FINAL, SONG_FLAG_UNREAD);
            } else {
                PlaySong(HOLE_SONG_MATCH, SONG_FLAG_UNREAD);
            }
            break;
        case GAME_MODE_SKINS_MATCH:
            if (GetLastHoleIndex() == g_HoleIndex) {
                PlaySong(HOLE_SONG_FINAL, SONG_FLAG_UNREAD);
            } else {
                PlaySong(HOLE_SONG_SKINS_MATCH, SONG_FLAG_UNREAD);
            }
            break;
        case GAME_MODE_TOURNAMENT:
            PlaySong(HOLE_SONG_TOURNAMENT, SONG_FLAG_UNREAD);
            break;
        case GAME_MODE_UNK7:
            PlaySong(HOLE_SONG_UNK18, SONG_FLAG_UNREAD);
            break;
        case GAME_MODE_RING_SHOT:
            PlaySong(HOLE_SONG_RING_SHOT, SONG_FLAG_UNREAD);
            D_801061C0 = 0;
            D_801061C3 = 0;
            break;
        case GAME_MODE_CLUB_SLOTS:
            if (GetLastHoleIndex() == g_HoleIndex) {
                PlaySong(HOLE_SONG_FINAL, SONG_FLAG_UNREAD);
            } else {
                PlaySong(HOLE_SONG_CLUB_SLOTS, SONG_FLAG_UNREAD);
            }
            break;
        case GAME_MODE_STROKE:
        default:
            if (g_HoleIndex == GetLastHoleIndex() && g_PlayerCount != 1) {
                PlaySong(HOLE_SONG_FINAL, SONG_FLAG_UNREAD);
            } else if (g_IsMiniGolf) {
                PlaySong(HOLE_SONG_MINI_GOLF, SONG_FLAG_UNREAD);
            } else {
                PlaySong(HOLE_SONG_COURSE, SONG_FLAG_UNREAD);
            }
            break;
    }
}

/* Starts the flyover of the hole, with the inset view on the cup */
void StartHoleFlyover(void) {
    g_HoleFlyoverFrame = 0;
    g_IsHoleIntroTextShown = 0;
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
    g_IsHoleIntroTextShown = 0;
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

void EaseLookAtBelowBall(f32 drop);
void EaseLookAtPoint(f32 yaw);

extern s32 sIsBehindViewFar;
extern s32 sSideViewCount;
extern s32 sIsApexViewFar;
extern s32 sIsTrailViewClose;
extern f32 sCircleAngle;
extern f32 sFlaggedStepYaw;

void func_80203B4C(void);

/* PlaceFlightCamera's distances are in course units unless they say otherwise; y points down */
#define CUP_FRAMING_PITCH -0.14f
#define CUP_FRAMING_RISE 0xC00 /* Above the cup, of both framed spheres */
#define CUP_FRAMING_RADIUS 3.0f
#define CUP_FRAMING_DEPTH_OFFSET -1.5f
#define PATH_FRAMING_PITCH -0.7853982f
#define PATH_FRAMING_RADIUS -15.0f
#define PATH_FRAMING_DEPTH_OFFSET -150.0f
#define LAUNCH_VIEW_STEP 11
#define LAUNCH_VIEW_MIN_RISE 0x6600
#define CHASE_DISTANCE 15.0f /* In world units */
#define CHASE_FIELD_OF_VIEW 80.0f
#define FOLLOW_STEP_COUNT 10 /* How many path steps behind the ball the camera follows */
#define FOLLOW_RISE 0x3C00
#define FOLLOW_FIELD_OF_VIEW 80.0f
#define TEE_VIEW_YAW_OFFSET 1.0f
#define TEE_VIEW_DISTANCE 15360.0f
#define TEE_VIEW_RISE 0x3600
#define TEE_LOOK_AT_RISE 0x3000
#define TEE_VIEW_FIELD_OF_VIEW 110.0f
#define NEAR_BEHIND_VIEW_LENGTH_FRACTION 0.2f
#define NEAR_BEHIND_VIEW_DISTANCE 15360.0f
#define FAR_BEHIND_VIEW_LENGTH_FRACTION 0.4f
#define FAR_BEHIND_VIEW_DISTANCE 30720.0f
#define BEHIND_VIEW_SPREAD_FRACTION 0.1
#define BEHIND_VIEW_SIDE_SPREAD_FRACTION 0.2
#define BEHIND_VIEW_SIDE_DISTANCE 23040.0f
#define BEHIND_VIEW_RISE 23040.0f
#define BEHIND_LOOK_AT_RISE 4.5f /* In world units */
#define BEHIND_LOOK_AT_EASED_RISE 4608.0f
#define APEX_VIEW_NEAR_HALF_DISTANCE 1536.0f
#define APEX_VIEW_FAR_DISTANCE 30720.0f
#define APEX_VIEW_RISE_STEP 0x600
#define APEX_VIEW_FAR_RISE_STEP_COUNT 10
#define APEX_VIEW_MIN_RISE 0x6600
#define SIDE_VIEW_LENGTH_FRACTION 0.1f
#define SIDE_VIEW_AHEAD_FRACTION 0.5f
#define SIDE_VIEW_SPREAD_STEP 0x600
#define SIDE_VIEW_SPREAD_STEP_COUNT 200
#define SIDE_VIEW_MAX_DISTANCE 768000.0f
#define SIDE_VIEW_MIN_DISTANCE 76800.0f
#define SIDE_VIEW_RISE_FRACTION 0.1f
#define SIDE_VIEW_RISE 153600.0f
#define SIDE_VIEW_MAX_DROP_BELOW_APEX 0x3C00
#define SIDE_VIEW_MIN_RISE_ABOVE_END 0x6600
#define SIDE_VIEW_MIN_RISE 0x7800
#define TRAIL_SHORT_SHOT_RISE 92160.0f
#define TRAIL_SHORT_SHOT_DISTANCE 153600.0f
#define TRAIL_LONG_SHOT_RISE 122880.0f
#define TRAIL_LONG_SHOT_DISTANCE 230400.0f
#define TRAIL_CLOSE_RISE 46080.0f
#define TRAIL_CLOSE_DISTANCE 61440.0f
#define TRAIL_EASE_START_PERCENT 100.0f
#define TRAIL_EASE_PERCENT_STEP 2.0f
#define TRAIL_EFFECT_END_FRACTION 0.1f
#define TRAIL_ANGLE_SCALE 0.25
#define UNKD_VIEW_DISTANCE 76800.0f
#define UNKD_VIEW_RISE 0x5A00
#define UNKD_EASE_DEAD_ZONE 0.5f
#define UNKD_EASE_FRACTION 0.1f
#define FLAGGED_VIEW_DISTANCE 460800.0f
#define FLAGGED_VIEW_RISE 0x4B000
/* While D_800BE624 is below this, the circle starts at angle 0 rather than at a side */
#define CIRCLE_START_FLIGHT_COUNT 2
#define CIRCLE_ANGLE_STEP DEGREES_TO_RADIANS(2)
#define CIRCLE_DISTANCE 30720.0f
#define CIRCLE_RISE 23040.0f
#define CIRCLE_LOOK_AT_RISE 15360.0f
#define CIRCLE_MIN_RISE_ABOVE_START 23040.0f
#define BESIDE_GOLFER_FIELD_OF_VIEW 100.0f
#define BESIDE_GOLFER_FIRST_STEP 7
#define BESIDE_GOLFER_STEP_LIMIT 15
#define BESIDE_GOLFER_STEP 1536.0f
#define BESIDE_GOLFER_RISE 1536.0f
#define BESIDE_GOLFER_MIN_RISE 7680.0f
#define BESIDE_GOLFER_CLEARANCE 22.5f /* In world units */
#define BESIDE_GOLFER_LOOK_AT_RISE 12288.0f
#define BESIDE_GOLFER_FLIGHT_FIELD_OF_VIEW 80.0f
#define MAX_EYE_DEPTH_BELOW_GROUND 0x21C0000
#define MINI_GOLF_GROUND_SIZE 1048576.0f

/*
 * Places the flight camera's eye and look-at point for sFlightCameraKind at a step of
 * the ball's flight, then keeps the eye above the ground and over the course.
 * The kinds that test D_800BE628 do their setup once when it is set, then clear it.
 * It does not match: the original allocates some registers differently, and orders
 * three spots differently: the load of CHASE_FIELD_OF_VIEW, the move of cosf's
 * argument for FLIGHT_CAMERA_BEHIND_START, and the stores of point for FLIGHT_CAMERA_UNKD.
 */
#ifdef NON_MATCHING
void PlaceFlightCamera(s32 step) {
    Vec3 point;
    f32 extraRise;
    Vec3 cupSphere;
    Vec3 pathSphere;
    Vec3 position;
    f32 yaw;
    f32 distance;
    f32 sideYaw;
    f32 circleYaw;
    f32 height;
    f32 easeFraction;
    f32 sideDistance;
    f32 delta;
    f32 riseDistance;
    f32 angle;
    s32 index;
    s32 lowestHeight;

    switch (sFlightCameraKind) {
        case FLIGHT_CAMERA_FRAMING_CUP:
            ClearFramedSpheres();
            SetFramingAngles(CUP_FRAMING_PITCH, -sStartYawFromCup - M_PI / 6);
            cupSphere[AXIS_X] = (f32)D_800BA9D8.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
            cupSphere[AXIS_Z] = (f32)D_800BA9D8.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            cupSphere[AXIS_Y] = (f32)(D_800BA9D8.cupPosition[AXIS_Y] - CUP_FRAMING_RISE) * COURSE_TO_WORLD_SCALE;
            AddFramedSphere(cupSphere, CUP_FRAMING_RADIUS);
            cupSphere[AXIS_X] = (f32)g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
            cupSphere[AXIS_Z] = (f32)g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            cupSphere[AXIS_Y] = (f32)(D_800BA9D8.cupPosition[AXIS_Y] - CUP_FRAMING_RISE) * COURSE_TO_WORLD_SCALE;
            AddFramedSphere(cupSphere, CUP_FRAMING_RADIUS);
            FrameSpheres(CUP_FRAMING_DEPTH_OFFSET);
            GetFramedView(sCameraPosition, sLookAtPoint);
            break;
        case FLIGHT_CAMERA_FRAMING_PATH:
            ClearFramedSpheres();
            SetFramingAngles(PATH_FRAMING_PITCH, 0.0f);
            pathSphere[AXIS_X] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.minPositionStep[AXIS_X]][AXIS_X] * COURSE_TO_WORLD_SCALE;
            pathSphere[AXIS_Y] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.minPositionStep[AXIS_X]][AXIS_Y] * COURSE_TO_WORLD_SCALE;
            pathSphere[AXIS_Z] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.minPositionStep[AXIS_X]][AXIS_Z] * COURSE_TO_WORLD_SCALE;
            AddFramedSphere(pathSphere, PATH_FRAMING_RADIUS);
            pathSphere[AXIS_X] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.minPositionStep[AXIS_Z]][AXIS_X] * COURSE_TO_WORLD_SCALE;
            pathSphere[AXIS_Y] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.minPositionStep[AXIS_Z]][AXIS_Y] * COURSE_TO_WORLD_SCALE;
            pathSphere[AXIS_Z] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.minPositionStep[AXIS_Z]][AXIS_Z] * COURSE_TO_WORLD_SCALE;
            AddFramedSphere(pathSphere, PATH_FRAMING_RADIUS);
            pathSphere[AXIS_X] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.maxPositionStep[AXIS_X]][AXIS_X] * COURSE_TO_WORLD_SCALE;
            pathSphere[AXIS_Y] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.maxPositionStep[AXIS_X]][AXIS_Y] * COURSE_TO_WORLD_SCALE;
            pathSphere[AXIS_Z] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.maxPositionStep[AXIS_X]][AXIS_Z] * COURSE_TO_WORLD_SCALE;
            AddFramedSphere(pathSphere, PATH_FRAMING_RADIUS);
            pathSphere[AXIS_X] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.maxPositionStep[AXIS_Z]][AXIS_X] * COURSE_TO_WORLD_SCALE;
            pathSphere[AXIS_Y] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.maxPositionStep[AXIS_Z]][AXIS_Y] * COURSE_TO_WORLD_SCALE;
            pathSphere[AXIS_Z] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.maxPositionStep[AXIS_Z]][AXIS_Z] * COURSE_TO_WORLD_SCALE;
            AddFramedSphere(pathSphere, PATH_FRAMING_RADIUS);
            FrameSpheres(PATH_FRAMING_DEPTH_OFFSET);
            GetFramedView(sCameraPosition, sLookAtPoint);
            break;
        case FLIGHT_CAMERA_NEAR_LAUNCH:
            if (D_800BE628 == TRUE) {
                D_800BE628 = FALSE;
                sCameraPosition[AXIS_X] = g_SimulatedShot.path[LAUNCH_VIEW_STEP][AXIS_X];
                sCameraPosition[AXIS_Y] = g_SimulatedShot.path[LAUNCH_VIEW_STEP][AXIS_Y];
                sCameraPosition[AXIS_Z] = g_SimulatedShot.path[LAUNCH_VIEW_STEP][AXIS_Z];
                height = GetGroundHeight(sCameraPosition[AXIS_X], sCameraPosition[AXIS_Z]) - LAUNCH_VIEW_MIN_RISE;
                if (height < sCameraPosition[AXIS_Y]) {
                    sCameraPosition[AXIS_Y] = height;
                }
                sCameraPosition[AXIS_X] *= COURSE_TO_WORLD_SCALE;
                sCameraPosition[AXIS_Y] *= COURSE_TO_WORLD_SCALE;
                sCameraPosition[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
                sLookAtPoint[AXIS_X] = (f32)sFlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
                sLookAtPoint[AXIS_Y] = (f32)sFlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                sLookAtPoint[AXIS_Z] = (f32)sFlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            }
            break;
        case FLIGHT_CAMERA_CHASING_BALL:
            /* Back along the path, level with the ball */
            position[AXIS_X] = g_SimulatedShot.path[step - 1][AXIS_X] - (f32)sFlightBallPosition[AXIS_X];
            position[AXIS_Y] = 0.0f;
            position[AXIS_Z] = g_SimulatedShot.path[step - 1][AXIS_Z] - (f32)sFlightBallPosition[AXIS_Z];
            distance = CHASE_DISTANCE / sqrtf(position[AXIS_X] * position[AXIS_X] + position[AXIS_Y] * position[AXIS_Y]
                                              + position[AXIS_Z] * position[AXIS_Z]);
            position[AXIS_X] *= distance;
            position[AXIS_Y] *= distance;
            position[AXIS_Z] *= distance;
            g_Cameras[0].view.fieldOfView = CHASE_FIELD_OF_VIEW;
            sLookAtPoint[AXIS_X] = sFlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE - position[AXIS_X];
            sLookAtPoint[AXIS_Y] = sFlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            sLookAtPoint[AXIS_Z] = sFlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE - position[AXIS_Z];
            sCameraPosition[AXIS_X] = sFlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE + position[AXIS_X];
            sCameraPosition[AXIS_Y] = sFlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            sCameraPosition[AXIS_Z] = sFlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE + position[AXIS_Z];
            break;
        case FLIGHT_CAMERA_FOLLOWING_PATH:
            sCameraPosition[AXIS_X] =
                (f32)g_SimulatedShot.path[step - FOLLOW_STEP_COUNT][AXIS_X] * COURSE_TO_WORLD_SCALE;
            sCameraPosition[AXIS_Y] =
                (f32)(g_SimulatedShot.path[step - FOLLOW_STEP_COUNT][AXIS_Y] - FOLLOW_RISE) * COURSE_TO_WORLD_SCALE;
            sCameraPosition[AXIS_Z] =
                (f32)g_SimulatedShot.path[step - FOLLOW_STEP_COUNT][AXIS_Z] * COURSE_TO_WORLD_SCALE;
            g_Cameras[0].view.fieldOfView = FOLLOW_FIELD_OF_VIEW;
            EaseLookAtFlyingBall();
            break;
        case FLIGHT_CAMERA_UNK10:
        case FLIGHT_CAMERA_UNK11:
            EaseLookAtFlyingBall();
            break;
        case FLIGHT_CAMERA_BESIDE_TEE:
            if (D_800BE628 == TRUE) {
                D_800BE628 = FALSE;
                yaw = (u16)g_CurrentShot.unk0 * BINARY_ANGLE_TO_RADIANS - TEE_VIEW_YAW_OFFSET;
                point[AXIS_X] = cosf(yaw) * TEE_VIEW_DISTANCE + sShotStartPosition[AXIS_X];
                point[AXIS_Z] = sinf(yaw) * TEE_VIEW_DISTANCE + sShotStartPosition[AXIS_Z];
                point[AXIS_Y] = GetGroundHeight(point[AXIS_X], point[AXIS_Z]) - TEE_VIEW_RISE;
                point[AXIS_X] *= COURSE_TO_WORLD_SCALE;
                point[AXIS_Y] *= COURSE_TO_WORLD_SCALE;
                point[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
                sCameraPosition[AXIS_X] = point[AXIS_X];
                sCameraPosition[AXIS_Y] = point[AXIS_Y];
                sCameraPosition[AXIS_Z] = point[AXIS_Z];
                sLookAtPoint[AXIS_X] = (f32)sShotStartPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
                sLookAtPoint[AXIS_Y] = (f32)(sShotStartPosition[AXIS_Y] - TEE_LOOK_AT_RISE) * COURSE_TO_WORLD_SCALE;
                sLookAtPoint[AXIS_Z] = (f32)sShotStartPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                AimCameraYaw();
            }
            g_Cameras[0].view.fieldOfView = TEE_VIEW_FIELD_OF_VIEW;
            break;
        case FLIGHT_CAMERA_BEHIND_START:
            if (D_800BE628 == TRUE) {
                D_800BE628 = FALSE;
                sIsBehindViewFar ^= TRUE;
                yaw = sShotFlightYaw + M_PI;
                g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
                if (sIsBehindViewFar == FALSE) {
                    sideYaw = sShotFlightYaw + M_PI / 4 + (f32)(rand() & RANDOM_YAW_MASK) * (M_PI / 65536);
                    distance = D_800BE640 * NEAR_BEHIND_VIEW_LENGTH_FRACTION + NEAR_BEHIND_VIEW_DISTANCE
                             - RandomBelow(D_800BE640 * BEHIND_VIEW_SPREAD_FRACTION);
                } else {
                    sideYaw = sShotFlightYaw - M_PI / 4 - (f32)(rand() & RANDOM_YAW_MASK) * (M_PI / 65536);
                    distance = D_800BE640 * FAR_BEHIND_VIEW_LENGTH_FRACTION + FAR_BEHIND_VIEW_DISTANCE
                             - RandomBelow(D_800BE640 * BEHIND_VIEW_SPREAD_FRACTION);
                }
                sideDistance = RandomBelow(D_800BE640 * BEHIND_VIEW_SIDE_SPREAD_FRACTION) + BEHIND_VIEW_SIDE_DISTANCE;
                point[AXIS_X] = sShotStartPosition[AXIS_X] + sinf(yaw) * distance + sinf(sideYaw) * sideDistance;
                point[AXIS_Z] = sShotStartPosition[AXIS_Z] - cosf(yaw) * distance - cosf(sideYaw) * sideDistance;
                point[AXIS_Y] = GetGroundHeight(point[AXIS_X], point[AXIS_Z]) - BEHIND_VIEW_RISE;
                sCameraPosition[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
                sCameraPosition[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                sCameraPosition[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                SnapLookAtPoint();
                sLookAtPoint[AXIS_Y] -= BEHIND_LOOK_AT_RISE;
            }
            EaseLookAtBelowBall(BEHIND_LOOK_AT_EASED_RISE);
            break;
        case FLIGHT_CAMERA_SIDE_OF_FLIGHT:
            if (D_800BE628 == TRUE) {
                D_800BE628 = FALSE;
                sSideViewCount++;
                if (sSideViewCount & 1) {
                    /* Beside the path's highest point */
                    sideYaw = sShotFlightYaw - M_PI / 2 + (f32)(rand() & RANDOM_YAW_MASK) * RANDOM_YAW_TO_RADIANS;
                    sIsApexViewFar ^= TRUE;
                    if (sIsApexViewFar == FALSE) {
                        /* RandomBelow(1) is always 0; the original has it */
                        point[AXIS_X] = g_SimulatedShot.path[g_SimulatedShot.minPositionStep[AXIS_Y]][AXIS_X]
                                        + 2.0f * (sinf(sideYaw) * APEX_VIEW_NEAR_HALF_DISTANCE);
                        point[AXIS_Y] = g_SimulatedShot.path[g_SimulatedShot.minPositionStep[AXIS_Y]][AXIS_Y]
                                        + RandomBelow(1) * APEX_VIEW_RISE_STEP;
                        point[AXIS_Z] = g_SimulatedShot.path[g_SimulatedShot.minPositionStep[AXIS_Y]][AXIS_Z]
                                        - 2.0f * (cosf(sideYaw) * APEX_VIEW_NEAR_HALF_DISTANCE);
                    } else {
                        point[AXIS_X] = g_SimulatedShot.path[g_SimulatedShot.minPositionStep[AXIS_Y]][AXIS_X]
                                        + sinf(sideYaw) * APEX_VIEW_FAR_DISTANCE;
                        point[AXIS_Y] = g_SimulatedShot.path[g_SimulatedShot.minPositionStep[AXIS_Y]][AXIS_Y]
                                        + RandomBelow(APEX_VIEW_FAR_RISE_STEP_COUNT) * APEX_VIEW_RISE_STEP;
                        point[AXIS_Z] = g_SimulatedShot.path[g_SimulatedShot.minPositionStep[AXIS_Y]][AXIS_Z]
                                        - cosf(sideYaw) * APEX_VIEW_FAR_DISTANCE;
                    }
                    height = GetGroundHeight(point[AXIS_X], point[AXIS_Z]) - APEX_VIEW_MIN_RISE;
                    if (height < point[AXIS_Y]) {
                        point[AXIS_Y] = height;
                    }
                } else {
                    /* Off to the side of the flight, ahead of the start */
                    sideDistance = D_800BE640 * SIDE_VIEW_LENGTH_FRACTION;
                    distance = D_800BE640 * SIDE_VIEW_AHEAD_FRACTION;
                    /* Holding this yaw in angle is there to match */
                    angle = sShotFlightYaw + M_PI / 2 + (f32)(rand() & RANDOM_YAW_MASK) * RANDOM_YAW_TO_RADIANS;
                    g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
                    yaw = sShotFlightYaw + M_PI;
                    sideDistance -= RandomBelow(SIDE_VIEW_SPREAD_STEP_COUNT) * SIDE_VIEW_SPREAD_STEP;
                    if (sideDistance > SIDE_VIEW_MAX_DISTANCE) {
                        sideDistance = SIDE_VIEW_MAX_DISTANCE;
                    }
                    if (sideDistance < SIDE_VIEW_MIN_DISTANCE) {
                        sideDistance = SIDE_VIEW_MIN_DISTANCE;
                    }
                    point[AXIS_X] = sShotStartPosition[AXIS_X] + sinf(yaw) * distance + sinf(angle) * sideDistance;
                    point[AXIS_Z] = sShotStartPosition[AXIS_Z] - cosf(yaw) * distance - cosf(angle) * sideDistance;
                    /* Computing extraRise before GetGroundHeight is there to match */
                    extraRise = D_800BE640 * SIDE_VIEW_RISE_FRACTION;
                    point[AXIS_Y] = GetGroundHeight(point[AXIS_X], point[AXIS_Z]);
                    if (!g_SimulatedShot.unk2E08) {
                        point[AXIS_Y] -= extraRise + SIDE_VIEW_RISE;
                    }
                    height = g_SimulatedShot.path[g_SimulatedShot.minPositionStep[AXIS_Y]][AXIS_Y]
                           + SIDE_VIEW_MAX_DROP_BELOW_APEX;
                    if (height < point[AXIS_Y]) {
                        point[AXIS_Y] = height;
                    }
                    height = sShotEndPosition[AXIS_Y] - SIDE_VIEW_MIN_RISE_ABOVE_END;
                    if (height < point[AXIS_Y]) {
                        point[AXIS_Y] = height;
                    }
                    height = GetGroundHeight(point[AXIS_X], point[AXIS_Z]) - SIDE_VIEW_MIN_RISE;
                    if (height < point[AXIS_Y]) {
                        point[AXIS_Y] = height;
                    }
                }
                sCameraPosition[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
                sCameraPosition[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                sCameraPosition[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                SnapLookAtPoint();
                AimCameraYaw();
            }
            EaseLookAtFlyingBall();
            break;
        case FLIGHT_CAMERA_AROUND_END:
            if (D_800BE628 == TRUE) {
                D_800BE628 = FALSE;
                PlaceCameraAroundShotEnd();
                AimCameraYaw();
            }
            g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
            EaseLookAtFlyingBall();
            break;
        case FLIGHT_CAMERA_TRAILING_BALL:
            if (D_800BE628 == TRUE) {
                SnapLookAtPoint();
                D_800BE628 = FALSE;
                AimCameraYaw();
                sIsTrailViewClose ^= TRUE;
            }
            g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
            if (sIsTrailViewClose == FALSE) {
                if (D_800BE640 <= SHORT_SHOT_MAX_LENGTH) {
                    riseDistance = TRAIL_SHORT_SHOT_RISE;
                    sideDistance = TRAIL_SHORT_SHOT_DISTANCE;
                } else {
                    riseDistance = TRAIL_LONG_SHOT_RISE;
                    sideDistance = TRAIL_LONG_SHOT_DISTANCE;
                }
            } else {
                riseDistance = TRAIL_CLOSE_RISE;
                sideDistance = TRAIL_CLOSE_DISTANCE;
            }
            /* Unused; the original has it */
            rand();
            /* Converting step through easeFraction is there to match */
            easeFraction = step;
            easeFraction = TRAIL_EASE_START_PERCENT - TRAIL_EASE_PERCENT_STEP * easeFraction;
            if (easeFraction < 0.0f) {
                easeFraction = 0.0f;
            }
            if (easeFraction > 100.0f) {
                easeFraction = 100.0f;
            }
            easeFraction *= 0.01f;
            if (easeFraction < TRAIL_EFFECT_END_FRACTION) {
                StartHudAnimation(HUD_ANIMATION_NONE);
            }
            angle = D_800BE65C;
            if (angle < M_PI / 6) {
                angle = M_PI / 6;
            }
            if (angle > 2 * M_PI - M_PI / 6) {
                angle = 2 * M_PI - M_PI / 6;
            }
            if (D_800BE640 <= SHORT_SHOT_MAX_LENGTH) {
                angle = (angle - M_PI) * TRAIL_ANGLE_SCALE - M_PI;
            } else {
                angle = (angle - M_PI) * TRAIL_ANGLE_SCALE;
            }
            position[AXIS_X] =
                (-sinf(sShotFlightYaw + angle) * sideDistance + sFlightBallPosition[AXIS_X]) * COURSE_TO_WORLD_SCALE;
            position[AXIS_Z] =
                (cosf(sShotFlightYaw + angle) * sideDistance + sFlightBallPosition[AXIS_Z]) * COURSE_TO_WORLD_SCALE;
            position[AXIS_Y] = (sFlightBallPosition[AXIS_Y] - riseDistance) * COURSE_TO_WORLD_SCALE;
            sCameraPosition[AXIS_X] += (position[AXIS_X] - sCameraPosition[AXIS_X]) * easeFraction;
            sCameraPosition[AXIS_Y] += (position[AXIS_Y] - sCameraPosition[AXIS_Y]) * easeFraction;
            sCameraPosition[AXIS_Z] += (position[AXIS_Z] - sCameraPosition[AXIS_Z]) * easeFraction;
            EaseLookAtFlyingBall();
            break;
        case FLIGHT_CAMERA_UNKD:
            yaw = sEndYawFromCup;
            g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
            point[AXIS_X] = -sinf(yaw) * UNKD_VIEW_DISTANCE + sFlightBallPosition[AXIS_X];
            point[AXIS_Z] = cosf(yaw) * UNKD_VIEW_DISTANCE + sFlightBallPosition[AXIS_Z];
            point[AXIS_Y] = GetGroundHeight(point[AXIS_X], point[AXIS_Z]) - UNKD_VIEW_RISE;
            height = sShotEndPosition[AXIS_Y] - UNKD_VIEW_RISE;
            if (height < point[AXIS_Y]) {
                point[AXIS_Y] = height;
            }
            point[AXIS_X] *= COURSE_TO_WORLD_SCALE;
            /* Computing delta before scaling y and z, and the do-while, are there to match */
            delta = point[AXIS_X] - sCameraPosition[AXIS_X];
            point[AXIS_Y] *= COURSE_TO_WORLD_SCALE;
            point[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
            do {
                if (fabsf(delta) > UNKD_EASE_DEAD_ZONE) {
                    sCameraPosition[AXIS_X] += delta * UNKD_EASE_FRACTION;
                }
            } while (0);
            delta = point[AXIS_Y] - sCameraPosition[AXIS_Y];
            if (fabsf(delta) > UNKD_EASE_DEAD_ZONE) {
                sCameraPosition[AXIS_Y] += delta * UNKD_EASE_FRACTION;
            }
            delta = point[AXIS_Z] - sCameraPosition[AXIS_Z];
            if (fabsf(delta) > UNKD_EASE_DEAD_ZONE) {
                sCameraPosition[AXIS_Z] += delta * UNKD_EASE_FRACTION;
            }
            EaseLookAtPoint(sEndYawFromCup);
            break;
        case FLIGHT_CAMERA_FLAGGED_STEP:
            if (D_800BE64C == 0) {
                D_800BE64C = 1;
            }
            sFlaggedStepYaw =
                -atan2((f32)(g_SimulatedShot.path[D_800BE64C][AXIS_X] - g_SimulatedShot.path[D_800BE64C - 1][AXIS_X]),
                       (f32)(g_SimulatedShot.path[D_800BE64C][AXIS_Z] - g_SimulatedShot.path[D_800BE64C - 1][AXIS_Z]));
            point[AXIS_X] = -sinf(sFlaggedStepYaw) * FLAGGED_VIEW_DISTANCE + g_SimulatedShot.path[D_800BE64C][AXIS_X];
            point[AXIS_Z] = cosf(sFlaggedStepYaw) * FLAGGED_VIEW_DISTANCE + g_SimulatedShot.path[D_800BE64C][AXIS_Z];
            point[AXIS_Y] = GetGroundHeight(point[AXIS_X], point[AXIS_Z]) - FLAGGED_VIEW_RISE;
            sCameraPosition[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
            sCameraPosition[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            sCameraPosition[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            EaseLookAtFlyingBall();
            g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
            break;
        case FLIGHT_CAMERA_UNK3:
            if (D_800BE628 == TRUE) {
                D_800BE628 = FALSE;
                func_80203B4C();
                sCameraPosition[AXIS_X] = g_Cameras[0].view.eye[AXIS_X];
                sCameraPosition[AXIS_Y] = g_Cameras[0].view.eye[AXIS_Y];
                sCameraPosition[AXIS_Z] = g_Cameras[0].view.eye[AXIS_Z];
                sLookAtPoint[AXIS_X] = g_Cameras[0].view.lookAt[AXIS_X];
                sLookAtPoint[AXIS_Y] = g_Cameras[0].view.lookAt[AXIS_Y];
                sLookAtPoint[AXIS_Z] = g_Cameras[0].view.lookAt[AXIS_Z];
                AimCameraYaw();
            }
            break;
        case FLIGHT_CAMERA_CIRCLING_BALL:
            if (D_800BE628 == TRUE) {
                D_800BE628 = FALSE;
                if (D_800BE624 < CIRCLE_START_FLIGHT_COUNT) {
                    sCircleAngle = 0.0f;
                } else if (g_Players[g_CurrentShot.playerIndex].unk22 == TRUE) {
                    sCircleAngle = M_PI / 2;
                } else {
                    sCircleAngle = -M_PI / 2;
                }
            }
            if (g_Players[g_CurrentShot.playerIndex].unk22 == 1) {
                sCircleAngle += CIRCLE_ANGLE_STEP;
                if (sCircleAngle > M_PI) {
                    sCircleAngle = M_PI;
                }
            } else {
                sCircleAngle -= CIRCLE_ANGLE_STEP;
                if (sCircleAngle < -M_PI) {
                    sCircleAngle = -M_PI;
                }
            }
            circleYaw = (u16)g_CurrentShot.unk0 * BINARY_ANGLE_TO_RADIANS + sCircleAngle;
            sCameraPosition[AXIS_X] = cosf(circleYaw) * CIRCLE_DISTANCE + g_CurrentShot.ballPosition[AXIS_X];
            sCameraPosition[AXIS_Z] = sinf(circleYaw) * CIRCLE_DISTANCE + g_CurrentShot.ballPosition[AXIS_Z];
            sCameraPosition[AXIS_Y] = GetGroundHeight(sCameraPosition[AXIS_X], sCameraPosition[AXIS_Z]) - CIRCLE_RISE;
            g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
            sCameraPosition[AXIS_X] *= COURSE_TO_WORLD_SCALE;
            sCameraPosition[AXIS_Y] *= COURSE_TO_WORLD_SCALE;
            sCameraPosition[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
            sLookAtPoint[AXIS_X] = (f32)g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
            sLookAtPoint[AXIS_Y] =
                ((f32)g_CurrentShot.ballPosition[AXIS_Y] - CIRCLE_LOOK_AT_RISE) * COURSE_TO_WORLD_SCALE;
            sLookAtPoint[AXIS_Z] = (f32)g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            break;
        case FLIGHT_CAMERA_BESIDE_GOLFER:
            if (D_800BE628 == TRUE) {
                g_Cameras[0].view.fieldOfView = BESIDE_GOLFER_FIELD_OF_VIEW;
                GetCharacterPointPosition(0, 1, position);
                D_800BE628 = FALSE;
                if (g_Players[g_CurrentShot.playerIndex].unk22 == TRUE) {
                    yaw = (u16)g_CurrentShot.unk0 * BINARY_ANGLE_TO_RADIANS + M_PI / 2;
                } else {
                    yaw = (u16)g_CurrentShot.unk0 * BINARY_ANGLE_TO_RADIANS - M_PI / 2;
                }
                for (index = BESIDE_GOLFER_FIRST_STEP; index < BESIDE_GOLFER_STEP_LIMIT; index++) {
                    sCameraPosition[AXIS_X] =
                        cosf(yaw) * (distance = index * BESIDE_GOLFER_STEP) + g_CurrentShot.ballPosition[AXIS_X];
                    sCameraPosition[AXIS_Z] = sinf(yaw) * distance + g_CurrentShot.ballPosition[AXIS_Z];
                    sCameraPosition[AXIS_Y] =
                        GetGroundHeight(sCameraPosition[AXIS_X], sCameraPosition[AXIS_Z]) - BESIDE_GOLFER_RISE;
                    if (g_CurrentShot.ballPosition[AXIS_Y] - BESIDE_GOLFER_MIN_RISE < sCameraPosition[AXIS_Y]) {
                        sCameraPosition[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y] - BESIDE_GOLFER_MIN_RISE;
                    }
                    sCameraPosition[AXIS_X] *= COURSE_TO_WORLD_SCALE;
                    sCameraPosition[AXIS_Y] *= COURSE_TO_WORLD_SCALE;
                    sCameraPosition[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
                    if (Length2D(sCameraPosition[AXIS_X] - position[AXIS_X], sCameraPosition[AXIS_Z] - position[AXIS_Z])
                        > BESIDE_GOLFER_CLEARANCE) {
                        break;
                    }
                }
                sLookAtPoint[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
                sLookAtPoint[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y] - BESIDE_GOLFER_LOOK_AT_RISE;
                sLookAtPoint[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
                sLookAtPoint[AXIS_X] *= COURSE_TO_WORLD_SCALE;
                sLookAtPoint[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
                sLookAtPoint[AXIS_Y] *= COURSE_TO_WORLD_SCALE;
                AimCameraYaw();
            }
            g_Cameras[0].view.fieldOfView = BESIDE_GOLFER_FLIGHT_FIELD_OF_VIEW;
            break;
    }
    position[AXIS_X] = sCameraPosition[AXIS_X] * WORLD_TO_COURSE_SCALE;
    position[AXIS_Y] = sCameraPosition[AXIS_Y] * WORLD_TO_COURSE_SCALE;
    position[AXIS_Z] = sCameraPosition[AXIS_Z] * WORLD_TO_COURSE_SCALE;
    lowestHeight = GetGroundHeight(position[AXIS_X], position[AXIS_Z]) + MAX_EYE_DEPTH_BELOW_GROUND;
    if (sFlightCameraKind == FLIGHT_CAMERA_CIRCLING_BALL) {
        if (lowestHeight < position[AXIS_Y]) {
            position[AXIS_Y] = lowestHeight;
        }
        height = sShotStartPosition[AXIS_Y] - CIRCLE_MIN_RISE_ABOVE_START;
        if (height < position[AXIS_Y]) {
            position[AXIS_Y] = height;
        }
    } else if (sFlightCameraKind != FLIGHT_CAMERA_BESIDE_GOLFER && sFlightCameraKind != FLIGHT_CAMERA_FRAMING_CUP) {
        height = lowestHeight;
        if (height < position[AXIS_Y]) {
            position[AXIS_Y] = height;
        }
    }
    if (g_IsMiniGolf) {
        if (sFlightCameraKind != FLIGHT_CAMERA_FRAMING_PATH) {
            if (position[AXIS_X] > MINI_GOLF_GROUND_SIZE) {
                position[AXIS_X] = MINI_GOLF_GROUND_SIZE;
            }
            if (position[AXIS_X] < 0.0f) {
                position[AXIS_X] = 0.0f;
            }
            if (position[AXIS_Z] > MINI_GOLF_GROUND_SIZE) {
                position[AXIS_Z] = MINI_GOLF_GROUND_SIZE;
            }
            if (position[AXIS_Z] < 0.0f) {
                position[AXIS_Z] = 0.0f;
            }
        }
    } else {
        if (position[AXIS_X] > GROUND_WIDTH) {
            position[AXIS_X] = GROUND_WIDTH;
        }
        if (position[AXIS_X] < 0.0f) {
            position[AXIS_X] = 0.0f;
        }
        if (position[AXIS_Z] > GROUND_LENGTH) {
            position[AXIS_Z] = GROUND_LENGTH;
        }
        if (position[AXIS_Z] < 0.0f) {
            position[AXIS_Z] = 0.0f;
        }
    }
    sCameraPosition[AXIS_X] = position[AXIS_X] * COURSE_TO_WORLD_SCALE;
    sCameraPosition[AXIS_Y] = position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    sCameraPosition[AXIS_Z] = position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
}
#else
INCLUDE_ASM("asm/nonmatchings/shot_camera", PlaceFlightCamera);
#endif

#define LONG_SHOT_MIN_LENGTH 0x1C2000
#define MID_SHOT_MIN_LENGTH 0x4B001
#define NEAR_CUP_DISTANCE 0x1E00
#define CUP_LANDING_MIN_DISTANCE 0x1201
#define CUP_LANDING_MAX_DISTANCE 0x4AFFF
#define CAMERA_CHANGE_HOLD_COUNT 20
#define DEBUG_CAMERA_CONTROLLER 1

/* It is a macro, as an inline function compiles differently from the original */
#define IsTroubleSurface(info)                                                                   \
    ((info)->kind == SURFACE_KIND_BUNKER || (info)->kind == SURFACE_KIND_BUNKER_FAIR_LIE           \
     || (info)->kind == SURFACE_KIND_BUNKER_POOR_LIE || (info)->kind == SURFACE_KIND_WATER_HAZARD \
     || (info)->kind == SURFACE_KIND_OUT_OF_BOUNDS)

/*
 * Picks a flight camera kind for the value of D_800DAF50, and switches to it if it is
 * greater than the current kind. The picks depend on the simulated shot, save option 3
 * and chance. A switch ends StartHudAnimation's effect and may start another one. In debug
 * builds, the second controller's B and A buttons pick FLIGHT_CAMERA_BESIDE_GOLFER and
 * FLIGHT_CAMERA_CIRCLING_BALL.
 * It does not match: cross-jumping sends the second switch's case 5 trouble-surface
 * branch through the identical "kind = FLIGHT_CAMERA_AROUND_END" tail of the first switch's
 * case 3, where the original branches straight to the end. The other instructions are
 * the same.
 */
#ifdef NON_MATCHING
void ChooseFlightCameraKind(void) {
    f32 unusedValues[14]; /* Unused; it is there to match */
    s32 animation;
    s32 kind;
    s32 pick;

    animation = HUD_ANIMATION_NONE;
    D_800BE630 = FALSE;
    kind = 0;
    if (g_SimulatedShot.unk2E08) {
        switch (D_800DAF50) {
            case 1:
                if (!(sFlightCameraFlags & FLIGHT_CAMERA_FLAG_UNK2) && RandomBelow(2) == 0 && !IsPinInCup()
                    && D_800BE644 >= CUP_LANDING_MIN_DISTANCE && D_800BE644 <= CUP_LANDING_MAX_DISTANCE
                    && (g_SimulatedShot.unk2E10 || sLandingCupDistance < NEAR_CUP_DISTANCE)) {
                    kind = FLIGHT_CAMERA_FRAMING_CUP;
                } else if (g_IsMiniGolf) {
                    pick = RandomBelow(10);
                    switch (pick) {
                        case 0:
                        case 1:
                            kind = FLIGHT_CAMERA_UNK3;
                            break;
                        case 2:
                            kind = FLIGHT_CAMERA_BEHIND_START;
                            break;
                        case 3:
                        case 4:
                            kind = FLIGHT_CAMERA_AROUND_END;
                            break;
                        default:
                            kind = FLIGHT_CAMERA_FRAMING_PATH;
                            break;
                    }
                } else {
                    pick = RandomBelow(6);
                    if (pick < 3) {
                        kind = FLIGHT_CAMERA_UNK3;
                    } else if (pick == 3) {
                        if ((D_800BE658 & 2) || g_SimulatedShot.stepCount > 120) {
                            kind = FLIGHT_CAMERA_BEHIND_START;
                        } else {
                            kind = FLIGHT_CAMERA_BESIDE_GOLFER;
                        }
                    } else if (pick == 4) {
                        kind = FLIGHT_CAMERA_BEHIND_START;
                    } else {
                        kind = FLIGHT_CAMERA_AROUND_END;
                    }
                }
                break;
            case 3:
                if (sFlightCameraKind != FLIGHT_CAMERA_FRAMING_PATH && sFlightCameraKind != FLIGHT_CAMERA_FRAMING_CUP
                    && sFlightCameraKind != FLIGHT_CAMERA_BEHIND_START) {
                    kind = FLIGHT_CAMERA_AROUND_END;
                }
                break;
            case 6:
                if (sFlightCameraKind != FLIGHT_CAMERA_FRAMING_PATH && sFlightCameraKind != FLIGHT_CAMERA_FRAMING_CUP) {
                    kind = FLIGHT_CAMERA_AROUND_END;
                }
                break;
            case 7:
                if (!g_SimulatedShot.unk2E10 && RandomBelow(2) == 0) {
                    kind = FLIGHT_CAMERA_UNKD;
                }
                break;
        }
    } else if (!func_8005DE40()) {
        switch (D_800DAF50) {
            case 1:
                pick = NextCameraPick(1);
                if (D_800BE64C != -1) {
                    kind = FLIGHT_CAMERA_UNK3;
                } else if (pick < 4) {
                    kind = FLIGHT_CAMERA_UNK3;
                } else if (pick == 4) {
                    if (D_800BE640 > SHORT_SHOT_MAX_LENGTH) {
                        kind = FLIGHT_CAMERA_UNK3;
                    } else {
                        kind = FLIGHT_CAMERA_CIRCLING_BALL;
                    }
                } else if (pick == 5) {
                    kind = FLIGHT_CAMERA_UNK3;
                    if (D_800BE690 == 1 && g_SimulatedShot.stepCount >= 61) {
                        kind = FLIGHT_CAMERA_BESIDE_GOLFER;
                    }
                } else if (pick == 6) {
                    if (D_800BE640 <= SHORT_SHOT_MAX_LENGTH) {
                        kind = FLIGHT_CAMERA_AROUND_END;
                    } else {
                        kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                    }
                } else if (pick == 7) {
                    if (D_800BE640 <= SHORT_SHOT_MAX_LENGTH) {
                        kind = FLIGHT_CAMERA_AROUND_END;
                    } else {
                        kind = FLIGHT_CAMERA_TRAILING_BALL;
                    }
                } else if (pick == 8) {
                    kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                } else if (pick == 9) {
                    kind = FLIGHT_CAMERA_UNK3;
                    if (D_800BE640 >= MID_SHOT_MIN_LENGTH && g_SimulatedShot.stepCount >= 16) {
                        kind = FLIGHT_CAMERA_NEAR_LAUNCH;
                    }
                }
                break;
            case 2:
                if (sFlightCameraKind == FLIGHT_CAMERA_NEAR_LAUNCH) {
                    animation = HUD_ANIMATION_TILES;
                    kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                } else if (sFlightCameraKind == FLIGHT_CAMERA_UNK3) {
                    if (RandomBelow(3) == 0) {
                        kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                    }
                    if (D_800BE640 >= LONG_SHOT_MIN_LENGTH && RandomBelow(4) == 0) {
                        kind = FLIGHT_CAMERA_FOLLOWING_PATH;
                    }
                } else if (sFlightCameraKind == FLIGHT_CAMERA_BESIDE_GOLFER) {
                    if (g_SimulatedShot.stepCount < 120) {
                        kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                    } else if (g_SimulatedShot.unk2E0C) {
                        kind = FLIGHT_CAMERA_BEHIND_START;
                    } else if (D_800BE640 < LONG_SHOT_MIN_LENGTH) {
                        kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                    } else if (RandomBelow(4) == 0) {
                        kind = FLIGHT_CAMERA_FOLLOWING_PATH;
                    } else {
                        kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                    }
                }
                break;
            case 3:
                if (sFlightCameraKind == FLIGHT_CAMERA_BESIDE_GOLFER) {
                    if (g_SimulatedShot.stepCount < 120) {
                        kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                    } else if (g_SimulatedShot.unk2E0C) {
                        kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                    }
                } else if (sFlightCameraKind != FLIGHT_CAMERA_TRAILING_BALL) {
                    /* The nested ifs and the separate cases are there to match */
                    if (sFlightCameraKind != FLIGHT_CAMERA_FOLLOWING_PATH) {
                        if (sFlightCameraKind != FLIGHT_CAMERA_SIDE_OF_FLIGHT) {
                            switch (RandomBelow(5)) {
                                case 0:
                                    kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                                    break;
                                case 1:
                                    kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                                    break;
                                case 2:
                                    kind = FLIGHT_CAMERA_AROUND_END;
                                    break;
                            }
                        }
                    }
                }
                break;
            case 4:
                if (D_800BE674) {
                    D_800BE630 = TRUE;
                } else if (sFlightCameraKind == FLIGHT_CAMERA_TRAILING_BALL) {
                    /* The empty branches are there to match */
                } else if (sFlightCameraKind == FLIGHT_CAMERA_FOLLOWING_PATH) {
                } else {
                    if (sLandingCupDistance <= SHORT_SHOT_MAX_LENGTH) {
                        if (RandomBelow(2) == 0) {
                            kind = FLIGHT_CAMERA_AROUND_END;
                        }
                    } else if (RandomBelow(8) == 0) {
                        kind = FLIGHT_CAMERA_AROUND_END;
                        animation = HUD_ANIMATION_TILES;
                    } else if (RandomBelow(8) < 4) {
                        kind = FLIGHT_CAMERA_AROUND_END;
                    }
                }
                break;
            case 5:
                kind = FLIGHT_CAMERA_AROUND_END;
                /* The repeated store is there to match */
                if (IsTroubleSurface(sEndSurfaceInfo) || sFlightCameraKind == FLIGHT_CAMERA_FOLLOWING_PATH) {
                    kind = FLIGHT_CAMERA_AROUND_END;
                }
                break;
            case 6:
                if (!D_800BE674) {
                    kind = FLIGHT_CAMERA_AROUND_END;
                } else {
                    D_800BE630 = TRUE;
                }
                break;
            case 7:
                if (IsTroubleSurface(sEndSurfaceInfo) || g_SimulatedShot.unk2E10) {
                    /* The empty branch is there to match */
                } else {
                    if (D_800BE674 || g_SimulatedShot.stepCount < 21) {
                        D_800BE630 = TRUE;
                    } else if (sLandingCupDistance <= SHORT_SHOT_MAX_LENGTH
                               || sFlightCameraKind == FLIGHT_CAMERA_AROUND_END) {
                        kind = FLIGHT_CAMERA_UNKD;
                    }
                }
                break;
        }
    } else {
        switch (D_800DAF50) {
            case 1:
                pick = RandomBelow(9);
                switch (pick) {
                    case 0:
                    case 1:
                    case 2:
                        kind = FLIGHT_CAMERA_UNK3;
                        break;
                    case 3:
                    case 4:
                    case 5:
                        if (D_800BE640 <= SHORT_SHOT_MAX_LENGTH) {
                            kind = FLIGHT_CAMERA_AROUND_END;
                        } else {
                            kind = FLIGHT_CAMERA_UNK3;
                        }
                        break;
                    case 6:
                        kind = FLIGHT_CAMERA_UNK3;
                        if (D_800BE690 == 1 && g_SimulatedShot.stepCount >= 61) {
                            kind = FLIGHT_CAMERA_BESIDE_GOLFER;
                        }
                        break;
                    case 7:
                        kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                        break;
                    case 8:
                        kind = FLIGHT_CAMERA_AROUND_END;
                        break;
                }
                D_800BE630 = TRUE;
                break;
            case 5:
                kind = FLIGHT_CAMERA_AROUND_END;
                D_800BE630 = TRUE;
                break;
            case 6:
                /* The empty case is there to match */
                break;
        }
    }
    if (sFlightCameraKind < kind) {
        D_800BE628 = TRUE;
        StartHudAnimation(HUD_ANIMATION_NONE);
        if (animation == HUD_ANIMATION_NONE) {
            if (D_800DAF50 >= 4 && !g_SimulatedShot.unk2E08 && D_800BE640 > SHORT_SHOT_MAX_LENGTH) {
                if (kind == FLIGHT_CAMERA_AROUND_END) {
                    if (RandomBelow(16) == 0 && sFlightCameraKind == FLIGHT_CAMERA_SIDE_OF_FLIGHT) {
                        StartHudAnimation(HUD_ANIMATION_TILES);
                    } else if (RandomBelow(8) == 0) {
                        StartHudAnimation(HUD_ANIMATION_UNK1);
                    } else if (RandomBelow(3) == 0) {
                        StartHudAnimation(HUD_ANIMATION_UNK9);
                    }
                }
            }
        } else if (D_800BE640 > SHORT_SHOT_MAX_LENGTH) {
            StartHudAnimation(animation);
        }
        sFlightCameraKind = kind;
        D_800BE674 = CAMERA_CHANGE_HOLD_COUNT;
    }
    if (g_IsDebugEnabled) {
        if (g_ControllerStates[DEBUG_CAMERA_CONTROLLER].buttons & B_BUTTON) {
            sFlightCameraKind = FLIGHT_CAMERA_BESIDE_GOLFER;
        }
        if (g_ControllerStates[DEBUG_CAMERA_CONTROLLER].buttons & A_BUTTON) {
            sFlightCameraKind = FLIGHT_CAMERA_CIRCLING_BALL;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/shot_camera", ChooseFlightCameraKind);
#endif

#define CUP_PASS_LOOKAHEAD_STEP_COUNT 30
#define PATH_FLAG_LOOKAHEAD_STEP_COUNT 60
#define VERY_LONG_SHOT_MIN_LENGTH 0x2A3000
#define BALL_SOUND_MIN_SHOT_LENGTH 0x25800
#define BALL_SOUND_VOLUME_LENGTH 0x7800 /* Shot length per step of volume */
#define BALL_SOUND_MIN_VOLUME 10
#define BALL_SOUND_MAX_VOLUME 64
#define BALL_SOUND_EARLY_STEP 6
#define BALL_SOUND_LATE_STEP 10
#define BALL_SOUND_UNK59 0x59
#define BALL_SOUND_UNK5A 0x5A
#define BALL_SOUND_CHANNEL 8
#define BALL_SOUND_REVERB 0x1E
#define BALL_SOUND_PAN 0x7F
#define MODULE_UNK1 1
#define CUP_LOOK_AT_DEPTH 11520.0f
#define CUP_CLOSE_VIEW_DISTANCE 60.0f
#define CUP_CLOSE_VIEW_HEIGHT 11.700001f
#define CUP_VIEW_FIELD_OF_VIEW 15.0f
#define CUP_VIEW_HOLD_COUNT 3
#define CUP_VIEW_DISTANCE 76800.0f
#define CUP_VIEW_HEIGHT 0x7800
#define INSET_VIEW_SIDE_OFFSET 0x3C00
#define INSET_VIEW_HEIGHT 0x7800
#define INSET_FIELD_OF_VIEW 30.0f
#define LOOK_AT_NUDGE 0.1f /* Keeps the eye from sitting straight above the point it looks at */

extern s8 D_801061CB;
extern s32 sBallCupDistance;
extern s32 sBallCupGroundDistance;
extern s8 D_80104B68;

void func_801FD9C0(void);
s32 func_8020D838(void);

/*
 * Updates the flight camera, the inset view and the ball's sounds for one frame of the
 * ball's flight, and advances g_BallFlightStep when isAdvancing is 1. Once the flight has
 * ended, it counts the landing wait and returns TRUE when it is over. The START button
 * clears D_801061BF through D_801061C5 and D_801061CB.
 */
s32 UpdateFlightCamera(s32 isAdvancing, s32 buttons) {
    f32 point[3];
    s32 step;
    s32 volume;
    s32 aheadStep;
    s32 waitFrame;
    s32 *flightStep;

    if (isAdvancing == 0) {
        D_801061CB = 0;
    } else {
        D_801061CB = 1;
    }
    if (buttons & START_BUTTON) {
        D_801061C4 = 0;
        D_801061BF = 0;
        D_801061C0 = 0;
        D_801061C1 = 0;
        D_801061C2 = 0;
        D_801061C3 = 0;
        D_801061C5 = 0;
        D_801061CB = 0;
    } else {
        D_801061C4 = D_800BE67C;
        D_801061BF = D_800BE67C;
        D_801061C0 = D_800BE67C;
        D_801061C1 = D_800BE67C;
        D_801061C2 = D_800BE67C;
        D_801061C3 = D_800BE67C;
        D_801061C5 = D_800BE67C;
    }
    if (g_BallFlightStep >= PATH_STEP_COUNT) {
        step = PATH_STEP_COUNT - 1;
    } else {
        step = g_BallFlightStep;
    }
    if (step >= g_SimulatedShot.stepCount - 1) {
        step = g_SimulatedShot.stepCount - 1;
    }
    if (isAdvancing == 0) {
        sFlightBallPosition[AXIS_X] = sShotStartPosition[AXIS_X];
        sFlightBallPosition[AXIS_Y] = sShotStartPosition[AXIS_Y];
        sFlightBallPosition[AXIS_Z] = sShotStartPosition[AXIS_Z];
    } else if (g_BallFlightStep == g_SimulatedShot.stepCount) {
        sFlightBallPosition[AXIS_X] = g_SimulatedShot.endPosition[AXIS_X];
        sFlightBallPosition[AXIS_Y] = g_SimulatedShot.endPosition[AXIS_Y];
        sFlightBallPosition[AXIS_Z] = g_SimulatedShot.endPosition[AXIS_Z];
    } else {
        sFlightBallPosition[AXIS_X] = g_SimulatedShot.path[step][AXIS_X];
        sFlightBallPosition[AXIS_Y] = g_SimulatedShot.path[step][AXIS_Y];
        sFlightBallPosition[AXIS_Z] = g_SimulatedShot.path[step][AXIS_Z];
    }
    sBallCupDistance = IntLength3D(sFlightBallPosition[AXIS_X] - D_800BA9D8.cupPosition[AXIS_X],
                             sFlightBallPosition[AXIS_Y] - D_800BA9D8.cupPosition[AXIS_Y],
                             sFlightBallPosition[AXIS_Z] - D_800BA9D8.cupPosition[AXIS_Z]);
    sBallCupGroundDistance = IntLength2D(sFlightBallPosition[AXIS_X] - D_800BA9D8.cupPosition[AXIS_X],
                             sFlightBallPosition[AXIS_Z] - D_800BA9D8.cupPosition[AXIS_Z]);
    if (g_SimulatedShot.unk2E08) {
        switch (D_800DAF50) {
            case 1:
                if (isAdvancing == 1 && step == 10 && g_BallFlightStep < g_SimulatedShot.stepCount - 10) {
                    D_800DAF50 = 3;
                    D_800BE630 = TRUE;
                }
                break;
            case 3:
                if (step == 40 && g_BallFlightStep < g_SimulatedShot.stepCount - 10) {
                    D_800DAF50 = 6;
                    D_800BE630 = TRUE;
                }
                break;
            case 6:
                if (g_SimulatedShot.stepCount - 10 < g_BallFlightStep) {
                    D_800DAF50 = 7;
                    D_800BE630 = TRUE;
                }
                break;
        }
    } else if (func_8005DE40() == 0) {
        switch (D_800DAF50) {
            case 0:
                D_800DAF50 = 1;
                D_800BE630 = TRUE;
                break;
            case 1:
                if (isAdvancing == 1 && step == 10) {
                    D_800DAF50 = 2;
                    D_800BE630 = TRUE;
                }
                break;
            case 2:
                if (step == 15) {
                    D_800DAF50 = 3;
                    D_800BE630 = TRUE;
                }
                break;
            case 3:
                if (g_BallFlightStep > g_SimulatedShot.minPositionStep[AXIS_Y]) {
                    D_800DAF50 = 4;
                    D_800BE630 = TRUE;
                }
                break;
            case 4:
                if (g_SimulatedShot.pathFlags[step + 40]) {
                    D_800DAF50 = 5;
                    D_800BE630 = TRUE;
                }
                break;
            case 5:
                if (g_SimulatedShot.pathFlags[step]) {
                    D_800DAF50 = 6;
                    D_800BE630 = TRUE;
                }
                break;
            case 6:
                if (g_SimulatedShot.stepCount - 10 < g_BallFlightStep) {
                    D_800DAF50 = 7;
                    D_800BE630 = TRUE;
                }
                break;
        }
    } else {
        switch (D_800DAF50) {
            case 1:
                D_800DAF50 = 3;
                break;
            case 3:
                if (g_BallFlightStep > g_SimulatedShot.minPositionStep[AXIS_Y]) {
                    D_800DAF50 = 5;
                    D_800BE630 = TRUE;
                }
                break;
            case 5:
                /* The empty case is there to match */
                break;
        }
    }
    /* Holding both look-ahead steps in aheadStep is there to match */
    aheadStep = step + CUP_PASS_LOOKAHEAD_STEP_COUNT;
    if (aheadStep <= g_SimulatedShot.stepCount
        && IsCupPassPoint(IntLength2D(g_SimulatedShot.path[aheadStep][AXIS_X] - D_800BA9D8.cupPosition[AXIS_X],
                                      g_SimulatedShot.path[aheadStep][AXIS_Z] - D_800BA9D8.cupPosition[AXIS_Z]),
                          g_SimulatedShot.path[aheadStep][AXIS_Y])) {
        D_800BE658 |= 2;
    }
    aheadStep = step + PATH_FLAG_LOOKAHEAD_STEP_COUNT;
    if (g_SimulatedShot.stepCount >= aheadStep && (g_SimulatedShot.pathFlags[aheadStep] & PATH_FLAG_UNK2)) {
        D_800BE658 |= 1;
        if (D_800BE64C == -1) {
            D_800BE64C = aheadStep;
            D_800BE650 = 70;
        }
    }
    if (step > 60 && step < D_800BE64C && !D_800BE674) {
        sFlightCameraKind = FLIGHT_CAMERA_FLAGGED_STEP;
    }
    if (D_800BE64C != -1 && D_800BE64C + 10 < step && sFlightCameraKind != FLIGHT_CAMERA_AROUND_END
        && (s32)IntLength3D(sFlightBallPosition[AXIS_X] - sShotEndPosition[AXIS_X],
                            sFlightBallPosition[AXIS_Y] - sShotEndPosition[AXIS_Y],
                            sFlightBallPosition[AXIS_Z] - sShotEndPosition[AXIS_Z]) > 0x25800) {
        D_800BE628 = TRUE;
        sFlightCameraKind = FLIGHT_CAMERA_AROUND_END;
    }
    if (D_800BE64C < step && (g_BallFlightStep + 40 < g_SimulatedShot.stepCount || D_800DAF50 == 7)
        && D_800BE630 == TRUE && !D_800BE69C && !D_800BE658) {
        if (!D_800BE674) {
            ChooseFlightCameraKind();
        }
    }
    if (D_800BE674 > 0) {
        D_800BE674--;
    }
    if (g_IsDebugEnabled && IsGameFlagSet(GAME_FLAG_UNK5F)) {
        sprintf(g_PrintBuffer, "C:%d F:%2d %3d %3d ", D_800BE630, D_800BE674, step, g_BallFlightStep);
        PrintDebugText(g_PrintBuffer, 8, 2);
        sprintf(g_PrintBuffer, "TYPE:%2d TIME:%2d", sFlightCameraKind, D_800DAF50);
        PrintDebugText(g_PrintBuffer, 10, 3);
    }
    if (D_800BE640 > VERY_LONG_SHOT_MIN_LENGTH && g_BallFlightStep == 40 && g_SimulatedShot.stepCount > 80) {
        StartShotEffect(SHOT_EFFECT_LONG_SHOT);
    }
    if (D_800BE640 > BALL_SOUND_MIN_SHOT_LENGTH && !g_SimulatedShot.unk2E0C) {
        volume = D_800BE640 / BALL_SOUND_VOLUME_LENGTH + BALL_SOUND_MIN_VOLUME;
        if (volume > BALL_SOUND_MAX_VOLUME) {
            volume = BALL_SOUND_MAX_VOLUME;
        }
        if (step == BALL_SOUND_EARLY_STEP) {
            osSyncPrintf("ball:%d\n", volume);
            switch (sFlightCameraKind) {
                case FLIGHT_CAMERA_UNK0:
                case FLIGHT_CAMERA_UNK1:
                case FLIGHT_CAMERA_BESIDE_TEE:
                case FLIGHT_CAMERA_UNK3:
                case FLIGHT_CAMERA_BESIDE_GOLFER:
                case FLIGHT_CAMERA_CIRCLING_BALL:
                case FLIGHT_CAMERA_CHASING_BALL:
                case FLIGHT_CAMERA_FOLLOWING_PATH:
                    PlaySoundWithEffects(BALL_SOUND_UNK59, BALL_SOUND_CHANNEL, volume,
                                         BALL_SOUND_REVERB, BALL_SOUND_PAN);
                    break;
            }
        }
        if (step == BALL_SOUND_LATE_STEP) {
            osSyncPrintf("ball:%d\n", volume);
            switch (sFlightCameraKind) {
                case FLIGHT_CAMERA_TRAILING_BALL:
                case FLIGHT_CAMERA_NEAR_LAUNCH:
                case FLIGHT_CAMERA_SIDE_OF_FLIGHT:
                case FLIGHT_CAMERA_AROUND_END:
                case FLIGHT_CAMERA_UNKD:
                case FLIGHT_CAMERA_UNKE:
                case FLIGHT_CAMERA_UNKF:
                case FLIGHT_CAMERA_UNK10:
                case FLIGHT_CAMERA_UNK11:
                    PlaySoundWithEffects(BALL_SOUND_UNK5A, BALL_SOUND_CHANNEL, volume,
                                         BALL_SOUND_REVERB, BALL_SOUND_PAN);
                    break;
            }
        }
    }
    PlayFlightSounds(step);
    g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
    PlaceFlightCamera(step);
    D_800BE698 = TRUE;
    D_800BE69C = FALSE;
    if (g_SimulatedShot.unk2E0C) {
        if (g_IsModuleAlive[MODULE_UNK1] || ReportModuleNotAlive(MODULE_UNK1)) {
            func_801FD9C0();
        }
        /* pathFlags is an s8, so testing 0x100 tests its sign; the original has it */
        if ((g_SimulatedShot.pathFlags[step] & 0x100) || (g_SimulatedShot.pathFlags[step] & PATH_FLAG_UNK4)) {
            func_80213D70(-1);
        }
    } else if (g_SimulatedShot.pathFlags[step]) {
        D_800C5AAC = 2;
        func_80213D70(10);
        if (g_IsModuleAlive[MODULE_UNK1] || ReportModuleNotAlive(MODULE_UNK1)) {
            func_801FD9C0();
        }
    }
    if (sBallCupGroundDistance > CUP_PASS_RADIUS) {
        if (sCupViewHoldCount > 0) {
            sCupViewHoldCount--;
        }
    }
    if (isAdvancing == 1) {
        if (D_800BE650 > 0) {
            D_800BE650--;
        }
        if (!g_IsMiniGolf && !g_SimulatedShot.unk2E10
            && ((g_SimulatedShot.pathFlags[step] & PATH_FLAG_UNK8) || func_8020D838() || D_800BE680 == TRUE)) {
            g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
            StartHudAnimation(HUD_ANIMATION_NONE);
            g_InsetView.animation = INSET_VIEW_STILL;
            g_InsetView.step = 0;
            g_Cameras[INSET_CAMERA].view.drawFlags = 0;
            g_Cameras[0].view.lookAt[AXIS_X] = (f32)D_800BA9D8.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
            g_Cameras[0].view.lookAt[AXIS_Y] =
                ((f32)D_800BA9D8.cupPosition[AXIS_Y] - CUP_LOOK_AT_DEPTH) * COURSE_TO_WORLD_SCALE;
            g_Cameras[0].view.lookAt[AXIS_Z] = (f32)D_800BA9D8.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            g_Cameras[0].view.eye[AXIS_X] =
                g_Cameras[0].view.lookAt[AXIS_X] + sinf(D_800BE684) * CUP_CLOSE_VIEW_DISTANCE;
            g_Cameras[0].view.eye[AXIS_Y] = g_Cameras[0].view.lookAt[AXIS_Y] - CUP_CLOSE_VIEW_HEIGHT;
            g_Cameras[0].view.eye[AXIS_Z] =
                g_Cameras[0].view.lookAt[AXIS_Z] - cosf(D_800BE684) * CUP_CLOSE_VIEW_DISTANCE;
        } else if (IsCupPassPoint(sBallCupGroundDistance, sFlightBallPosition[AXIS_Y]) || sCupViewHoldCount > 0) {
            g_Cameras[0].view.fieldOfView = CUP_VIEW_FIELD_OF_VIEW;
            StartHudAnimation(HUD_ANIMATION_NONE);
            D_800BE69C = TRUE;
            D_800BE698 = FALSE;
            D_800BE658 &= ~2;
            if (sCupViewHoldCount == 0) {
                sCupViewHoldCount = CUP_VIEW_HOLD_COUNT;
            }
            D_800BE674 = CAMERA_CHANGE_HOLD_COUNT;
            g_InsetView.animation = INSET_VIEW_STILL;
            g_InsetView.step = 0;
            g_Cameras[INSET_CAMERA].view.drawFlags = 0;
            g_Cameras[0].view.lookAt[AXIS_X] = (f32)D_800BA9D8.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
            g_Cameras[0].view.lookAt[AXIS_Y] = (f32)D_800BA9D8.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            g_Cameras[0].view.lookAt[AXIS_Z] = (f32)D_800BA9D8.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            point[AXIS_X] = -sinf(sCameraYaw) * CUP_VIEW_DISTANCE + D_800BA9D8.cupPosition[AXIS_X];
            point[AXIS_Y] = D_800BA9D8.cupPosition[AXIS_Y] - CUP_VIEW_HEIGHT;
            point[AXIS_Z] = cosf(sCameraYaw) * CUP_VIEW_DISTANCE + D_800BA9D8.cupPosition[AXIS_Z];
            g_Cameras[0].view.eye[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
            g_Cameras[0].view.eye[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            g_Cameras[0].view.eye[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        } else {
            g_Cameras[0].view.eye[AXIS_X] = sCameraPosition[AXIS_X];
            g_Cameras[0].view.eye[AXIS_Y] = sCameraPosition[AXIS_Y];
            g_Cameras[0].view.eye[AXIS_Z] = sCameraPosition[AXIS_Z];
            g_Cameras[0].view.lookAt[AXIS_X] = sLookAtPoint[AXIS_X];
            g_Cameras[0].view.lookAt[AXIS_Y] = sLookAtPoint[AXIS_Y];
            g_Cameras[0].view.lookAt[AXIS_Z] = sLookAtPoint[AXIS_Z];
        }
    } else {
        g_Cameras[0].view.eye[AXIS_X] = sCameraPosition[AXIS_X];
        g_Cameras[0].view.eye[AXIS_Y] = sCameraPosition[AXIS_Y];
        g_Cameras[0].view.eye[AXIS_Z] = sCameraPosition[AXIS_Z];
        g_Cameras[0].view.lookAt[AXIS_X] = sLookAtPoint[AXIS_X];
        g_Cameras[0].view.lookAt[AXIS_Y] = sLookAtPoint[AXIS_Y];
        g_Cameras[0].view.lookAt[AXIS_Z] = sLookAtPoint[AXIS_Z];
    }
    if (!D_800BE69C) {
        AimCameraYaw();
    }
    g_Cameras[INSET_CAMERA].view.eye[AXIS_X] =
        (f32)(sShotEndPosition[AXIS_X] - INSET_VIEW_SIDE_OFFSET) * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.eye[AXIS_Y] =
        (f32)(sShotEndPosition[AXIS_Y] - INSET_VIEW_HEIGHT) * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.eye[AXIS_Z] = (f32)sShotEndPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.fieldOfView = INSET_FIELD_OF_VIEW;
    if (sEndSurfaceInfo->kind == SURFACE_KIND_OUT_OF_BOUNDS) {
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] = (f32)sShotEndPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] = (f32)sShotEndPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] = (f32)sShotEndPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    } else {
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] = (f32)sFlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] = (f32)sFlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] = (f32)sFlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    }
    if (g_Cameras[0].view.eye[AXIS_X] == g_Cameras[0].view.lookAt[AXIS_X]
        && g_Cameras[0].view.eye[AXIS_Z] == g_Cameras[0].view.lookAt[AXIS_Z]) {
        g_Cameras[0].view.lookAt[AXIS_X] += LOOK_AT_NUDGE;
        g_Cameras[0].view.lookAt[AXIS_Z] += LOOK_AT_NUDGE;
    }
    if (g_Cameras[INSET_CAMERA].view.eye[AXIS_X] == g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X]
        && g_Cameras[INSET_CAMERA].view.eye[AXIS_Z] == g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z]) {
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] += LOOK_AT_NUDGE;
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] += LOOK_AT_NUDGE;
    }
    if (g_SimulatedShot.stepCount < g_BallFlightStep + 40 && IsHudAnimationDone() == 1) {
        StartHudAnimation(HUD_ANIMATION_NONE);
    }
    if (isAdvancing == 1 && g_BallFlightStep == 0) {
        PlayContactSound();
    }
    /* Reading D_800BE620 and g_BallFlightStep through locals is there to match */
    waitFrame = D_800BE620;
    if (isAdvancing == 1) {
        flightStep = &g_BallFlightStep;
        if (*flightStep == g_SimulatedShot.stepCount) {
            if (waitFrame == 0 && !D_80104B68) {
                StartLandingReaction(LANDING_REACTION_UNK1);
            }
            if (++D_800BE620 >= sLandingWaitFrameCount) {
                if (IsGameFlagSet(GAME_FLAG_UNK22)) {
                    PrintDebugText("        ", 16, 4);
                    PrintDebugText("         ", 10, 2);
                }
                return TRUE;
            }
        } else {
            (*flightStep)++;
        }
    }
    return FALSE;
}

void SetLandingWaitFrameCount(s32 frameCount) {
    sLandingWaitFrameCount = frameCount;
}

#define VIEW_SIDE_DISTANCE 153600.0f

/*
 * Finds the points VIEW_SIDE_DISTANCE from center on x and z, a quarter turn
 * either way from the main camera's yaw.
 */
void GetViewSidePoints(CoursePoint center, CoursePoint plusQuarterPoint, CoursePoint minusQuarterPoint) {
    f32 offsetX;
    f32 offsetZ;
    f32 yaw;
    f32 plusQuarterAngle;
    f32 minusQuarterAngle;

    offsetX = g_Cameras[0].view.lookAt[AXIS_X] - g_Cameras[0].view.eye[AXIS_X];
    offsetZ = g_Cameras[0].view.lookAt[AXIS_Z] - g_Cameras[0].view.eye[AXIS_Z];
    if ((s32)offsetX != 0 || (s32)offsetZ != 0) {
        yaw = M_PI - atan2f(offsetX, offsetZ);
    } else {
        yaw = 0.0f;
    }
    plusQuarterAngle = yaw + M_PI / 2;
    plusQuarterPoint[AXIS_X] = (s32)(-sinf(plusQuarterAngle) * VIEW_SIDE_DISTANCE) + center[AXIS_X];
    plusQuarterPoint[AXIS_Z] = (s32)(cosf(plusQuarterAngle) * VIEW_SIDE_DISTANCE) + center[AXIS_Z];
    minusQuarterAngle = yaw - M_PI / 2;
    minusQuarterPoint[AXIS_X] = (s32)(-sinf(minusQuarterAngle) * VIEW_SIDE_DISTANCE) + center[AXIS_X];
    minusQuarterPoint[AXIS_Z] = (s32)(cosf(minusQuarterAngle) * VIEW_SIDE_DISTANCE) + center[AXIS_Z];
}

void func_8004C6A8(void) {
}

void func_8004C6B0(void) {
}

void func_8004C6B8(void) {
}

#define SPOT_QUARTER_SIZE 32
#define SPOT_PEAK_INTENSITY 210
#define SPOT_DISC_RADIUS 16
#define SPOT_GAP_RADIUS 24
#define SPOT_GAP_FALL_PER_TEXEL 26
#define SPOT_RING_RISE_PER_TEXEL 16
#define SPOT_SOFT_DISC_RADIUS 15
#define SPOT_SOFT_FALL_PER_TEXEL 12

/*
 * Builds the top-left quarter of a spot, as SPOT_QUARTER_SIZE x
 * SPOT_QUARTER_SIZE I8 texels. A soft spot is a disc that fades out toward its
 * edge. Otherwise it is a disc, a dark gap and a ring that brightens outward.
 * It does not match: the soft spot converts its squared distance in $f2
 * rather than $f0.
 */
#ifdef NON_MATCHING
void BuildSpotTexels(u8 *texels, s32 isSoft) {
    s32 row;
    s32 column;
    s32 texelIndex;
    s32 radius;
    f32 distance;
    s32 intensity;
    s32 width;
    s32 height;
    s32 softDiscRadius;
    s32 lastRadius;

    texelIndex = 0;
    width = SPOT_QUARTER_SIZE;
    height = SPOT_QUARTER_SIZE;
    lastRadius = SPOT_QUARTER_SIZE - 1;
    if (!isSoft) {
        for (row = 0; row != height; row++) {
            for (column = 0; column != width; column++) {
                radius = sqrtf((row - height) * (row - height)
                               + (column - width) * (column - width));
                if (radius <= SPOT_DISC_RADIUS) {
                    intensity = SPOT_PEAK_INTENSITY;
                } else if (radius <= SPOT_GAP_RADIUS) {
                    intensity = SPOT_PEAK_INTENSITY - (radius - SPOT_DISC_RADIUS) * SPOT_GAP_FALL_PER_TEXEL;
                } else if (radius < SPOT_QUARTER_SIZE) {
                    intensity = (radius - SPOT_GAP_RADIUS) * SPOT_RING_RISE_PER_TEXEL;
                } else {
                    intensity = 0;
                }
                texels[texelIndex++] = intensity;
            }
        }
    } else {
        for (row = 0, softDiscRadius = SPOT_SOFT_DISC_RADIUS; row != height; row++) {
            for (column = 0; column != width; column++) {
                distance = (row - height) * (row - height) + (column - width) * (column - width);
                distance = sqrtf(distance);
                radius = distance;
                if (radius <= softDiscRadius) {
                    intensity = SPOT_PEAK_INTENSITY;
                } else if (radius <= lastRadius) {
                    intensity = SPOT_PEAK_INTENSITY - (s32)(distance - softDiscRadius) * SPOT_SOFT_FALL_PER_TEXEL;
                } else {
                    intensity = 0;
                }
                texels[texelIndex++] = intensity;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/shot_camera", BuildSpotTexels);
#endif

/*
 * Fills a width by height texture with a quarter of a ring around its far
 * corner. The outer radius is unused: the ring always ends at
 * RING_OUTER_RADIUS.
 */
void BuildRingTexels(u8 *texels, s32 width, s32 height, s32 outerRadius) {
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

#define NOISE_GRID_COUNT 2
#define NOISE_GRID_MASK (NOISE_GRID_SIZE - 1)
#define MIN_UNDIVIDED_NOISE_SIZE 2
#define NOISE_DISPLACEMENT_MASK 0xFF
#define NOISE_DISPLACEMENT_BIAS 0x80
#define NOISE_CORNER_SEED -0xFE0
#define NOISE_CENTER_SEED 0x3F80
#define NOISE_EDGE_SEED 1
#define NOISE_MAX_BLEND_WEIGHT 0x3F
#define NOISE_BLEND_STEP_MASK 0x7F
#define NOISE_INTENSITY_SHIFT 13
#define NOISE_MAX_INTENSITY 0xFF

/* Two grids of NOISE_GRID_SIZE x NOISE_GRID_SIZE heights, which the noise texture blends */
extern s16 sNoiseGrids[NOISE_GRID_COUNT][NOISE_GRID_SIZE][NOISE_GRID_SIZE];
/* The grid that DisplaceNoiseSquare builds */
extern s16 (*sNoiseGrid)[NOISE_GRID_SIZE];
/* The displacement scale at each midpoint displacement level */
extern s16 sNoiseRoughness[];
extern s16 sNoiseGridIndex;
extern s32 sNoiseBlendStep;
extern s8 sIsNoiseResetPending;
extern s8 sNoiseRow;

static inline s32 RandomNoiseDisplacement(void) {
    return (rand() & NOISE_DISPLACEMENT_MASK) - NOISE_DISPLACEMENT_BIAS;
}

/*
 * Midpoint displacement: sets each unset midpoint of the square of side
 * NOISE_GRID_SIZE >> level at column, row to the average of its corners plus
 * a random displacement, then does the same for the square's quarters.
 * A zero point is unset, so a displacement that gives zero is drawn again.
 */
void DisplaceNoiseSquare(s32 level, s32 column, s32 row) {
    u16 size;
    u16 half;
    s16 roughness;
    s16 topLeft;
    s16 topRight;
    s16 bottomLeft;
    s16 bottomRight;
    s32 nextLevel;

    size = NOISE_GRID_SIZE >> level;
    roughness = sNoiseRoughness[level];
    half = size >> 1;
    topLeft = sNoiseGrid[row][column];
    topRight = sNoiseGrid[row][(column + size) & NOISE_GRID_MASK];
    bottomLeft = sNoiseGrid[(row + size) & NOISE_GRID_MASK][column];
    bottomRight = sNoiseGrid[(row + size) & NOISE_GRID_MASK][(column + size) & NOISE_GRID_MASK];
    if (sNoiseGrid[row + half][column + half] == 0) {
        do {
            sNoiseGrid[row + half][column + half] =
                ((topLeft + topRight + bottomLeft + bottomRight) >> 2) + RandomNoiseDisplacement() * roughness;
        } while (sNoiseGrid[row + half][column + half] == 0);
    }
    if (sNoiseGrid[row][column + half] == 0) {
        do {
            sNoiseGrid[row][column + half] = ((topLeft + topRight) >> 1) + RandomNoiseDisplacement() * roughness;
        } while (sNoiseGrid[row][column + half] == 0);
    }
    if (sNoiseGrid[row + half][column] == 0) {
        do {
            sNoiseGrid[row + half][column] = ((topLeft + bottomLeft) >> 1) + RandomNoiseDisplacement() * roughness;
        } while (sNoiseGrid[row + half][column] == 0);
    }
    if (sNoiseGrid[row + half][(column + size) & NOISE_GRID_MASK] == 0) {
        do {
            sNoiseGrid[row + half][(column + size) & NOISE_GRID_MASK] =
                ((topRight + bottomRight) >> 1) + RandomNoiseDisplacement() * roughness;
        } while (sNoiseGrid[row + half][(column + size) & NOISE_GRID_MASK] == 0);
    }
    if (sNoiseGrid[(row + size) & NOISE_GRID_MASK][column + half] == 0) {
        do {
            sNoiseGrid[(row + size) & NOISE_GRID_MASK][column + half] =
                ((bottomLeft + bottomRight) >> 1) + RandomNoiseDisplacement() * roughness;
        } while (sNoiseGrid[(row + size) & NOISE_GRID_MASK][column + half] == 0);
    }
    if (size > MIN_UNDIVIDED_NOISE_SIZE) {
        nextLevel = level + 1;
        DisplaceNoiseSquare(nextLevel, column, row);
        DisplaceNoiseSquare(nextLevel, column + half, row);
        DisplaceNoiseSquare(nextLevel, column, row + half);
        DisplaceNoiseSquare(nextLevel, column + half, row + half);
    }
}

static inline void SelectNoiseGrid(s16 gridIndex) {
    sNoiseGrid = sNoiseGrids[gridIndex];
}

/*
 * Draws one row of the noise texture as a blend of the two noise grids. After
 * every row has been drawn, it moves the blend a step toward the other grid.
 * Once the blend has gone fully over, it rebuilds the faded-out grid and swaps
 * the grids, so the new one fades in.
 */
void StepNoiseTexture(void) {
    /* index counts the grids, then the texels; sharing it is there to match */
    s16 index;
    s16 row;
    s16 column;
    s8 weight;
    s32 intensity;
    s32 blend;

    if (!IsGameFlagSet(GAME_FLAG_PAUSE_NOISE_TEXTURE)) {
        if (sIsNoiseResetPending) {
            sIsNoiseResetPending = FALSE;
            sNoiseBlendStep = 0;
            sNoiseRow = 0;
            sNoiseGridIndex = 0;
            for (index = 0; index < NOISE_GRID_COUNT; index++) {
                SelectNoiseGrid(index);
                for (row = 0; row < NOISE_GRID_SIZE; row++) {
                    for (column = 0; column < NOISE_GRID_SIZE; column++) {
                        sNoiseGrid[row][column] = 0;
                    }
                }
            }
            SelectNoiseGrid(sNoiseGridIndex);
            sNoiseGrid[0][0] = NOISE_CORNER_SEED;
            sNoiseGrid[NOISE_GRID_SIZE / 2][NOISE_GRID_SIZE / 2] = NOISE_CENTER_SEED;
            DisplaceNoiseSquare(0, 0, 0);
            /* Seeds the same grid again, leaving the other one unseeded; the original has it */
            sNoiseGrid[0][0] = NOISE_CORNER_SEED;
            sNoiseGrid[NOISE_GRID_SIZE / 2][NOISE_GRID_SIZE / 2] = NOISE_CENTER_SEED;
            SelectNoiseGrid(sNoiseGridIndex ^ 1);
            DisplaceNoiseSquare(0, 0, 0);
        }
        index = sNoiseRow * NOISE_GRID_SIZE;
        weight = sNoiseBlendStep & NOISE_MAX_BLEND_WEIGHT;
        for (row = sNoiseRow; row < sNoiseRow + 1; row++) {
            for (column = 0; column < NOISE_GRID_SIZE; column++) {
                blend = (sNoiseGrids[sNoiseGridIndex][row][column] * (NOISE_MAX_BLEND_WEIGHT - weight)
                             + sNoiseGrids[sNoiseGridIndex ^ 1][row][column] * weight)
                         >> NOISE_INTENSITY_SHIFT;
                intensity = ABS(blend);
                if (intensity > NOISE_MAX_INTENSITY) {
                    intensity = NOISE_MAX_INTENSITY;
                }
                g_NoiseTexels[index++] = intensity;
            }
        }
        sNoiseRow = (sNoiseRow + 1) & NOISE_GRID_MASK;
        if (sNoiseRow == 0) {
            sNoiseBlendStep = (sNoiseBlendStep + 1) & NOISE_BLEND_STEP_MASK;
            if ((sNoiseBlendStep & NOISE_MAX_BLEND_WEIGHT) == 0) {
                SelectNoiseGrid(sNoiseGridIndex);
                for (row = 0; row < NOISE_GRID_SIZE; row++) {
                    for (column = 0; column < NOISE_GRID_SIZE; column++) {
                        sNoiseGrid[row][column] = 0;
                    }
                }
                sNoiseGrid[0][0] = 0;
                sNoiseGrid[NOISE_GRID_SIZE / 2][NOISE_GRID_SIZE / 2] = 0;
                sNoiseGrid[0][NOISE_GRID_SIZE / 2] = NOISE_EDGE_SEED;
                sNoiseGrid[NOISE_GRID_SIZE / 2][0] = NOISE_EDGE_SEED;
                DisplaceNoiseSquare(0, 0, 0);
                sNoiseGridIndex ^= 1;
            }
        }
    }
}

/* Resets the noise grids and draws every row of the noise texture */
void BuildNoiseTexture(void) {
    s32 row;

    sIsNoiseResetPending = TRUE;
    for (row = 0; row < NOISE_GRID_SIZE; row++) {
        StepNoiseTexture();
    }
}
