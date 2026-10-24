#include "common.h"
#include "controller.h"
#include "course.h"
#include "ball_simulation.h"
#include "billboard.h"
#include "camera.h"
#include "character_slot.h"
#include "club.h"
#include "current_shot.h"
#include "functions.h"
#include "game_flags.h"
#include "hole_loader.h"
#include "libc.h"
#include "math_util.h"
#include "pin.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "shot_simulation.h"
#include "sound.h"
#include "surface.h"
#include "text.h"
#include "variables.h"

/* Unused; the original has it */
static const char sUnusedFloatFormat[] = "            %f";

/* The camera kind that the B button picks in debug builds */
#define FLIGHT_CAMERA_UNK4 4

/* A camera kind that keeps the putter's contact from calling func_80213D70 */
#define FLIGHT_CAMERA_UNK13 0x13

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

void func_8009226C(s32);
void func_80205AE4(s32, s32);
void func_80216130(s32);
void func_80216B74(s32);
/* menu.c returns an s8, but the original calls it without truncating, as if undeclared */
s32 func_8005DE40();
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
        if (sFlightCameraKind != FLIGHT_CAMERA_UNK13) {
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

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_800469EC);

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
 * It does not match: the original keeps the loads of the ball's and the
 * look-at point's coordinates in statement order, where this schedules the
 * ball's loads first.
 */
#ifdef NON_MATCHING
void EaseLookAtPoint(void) {
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

void func_800934CC(s32);

extern s8 D_801061B8;
extern s8 D_801061C0;
extern s8 D_801061C3;
extern s8 D_801B60B9;
extern s32 g_PlayerCount;

/* Resets the cameras for the start of a hole, and plays the game mode's song */
void StartHoleCamerasAndSong(void) {
    func_800934CC(-1);
    D_801061CE = 1;
    D_801061B8 = -1;
    D_801061C0 = 1;
    D_801061C3 = 1;
    g_InsetView.animation = INSET_VIEW_STILL;
    g_InsetView.step = 0;
    g_HoleFlyoverFrame = 0;
    g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
    g_Cameras[1].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
    if (D_801B60B9) {
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

#define FLIGHT_CAMERA_UNK3 3
#define FLIGHT_CAMERA_UNK5 5 /* The camera kind that the A button picks in debug builds */
#define FLIGHT_CAMERA_UNK6 6
#define FLIGHT_CAMERA_UNK7 7
#define FLIGHT_CAMERA_UNK9 9
#define FLIGHT_CAMERA_UNKA 0xA
#define FLIGHT_CAMERA_UNKB 0xB
#define FLIGHT_CAMERA_UNKC 0xC
#define FLIGHT_CAMERA_UNKD 0xD
#define FLIGHT_CAMERA_UNK12 0x12
#define NO_EFFECT -1 /* func_8009226C's argument that ends its effect */
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
 * and chance. A switch ends func_8009226C's effect and may start another one. In debug
 * builds, the second controller's B and A buttons pick FLIGHT_CAMERA_UNK4 and
 * FLIGHT_CAMERA_UNK5.
 * It does not match: cross-jumping sends the second switch's case 5 trouble-surface
 * branch through the identical "kind = FLIGHT_CAMERA_UNKC" tail of the first switch's
 * case 3, where the original branches straight to the end. The other instructions are
 * the same.
 */
#ifdef NON_MATCHING
void ChooseFlightCameraKind(void) {
    f32 unusedValues[14]; /* Unused; it is there to match */
    s32 effectIndex;
    s32 kind;
    s32 pick;

    effectIndex = NO_EFFECT;
    D_800BE630 = FALSE;
    kind = 0;
    if (g_SimulatedShot.unk2E08) {
        switch (D_800DAF50) {
            case 1:
                if (!(sFlightCameraFlags & FLIGHT_CAMERA_FLAG_UNK2) && RandomBelow(2) == 0 && !IsPinInCup()
                    && D_800BE644 >= CUP_LANDING_MIN_DISTANCE && D_800BE644 <= CUP_LANDING_MAX_DISTANCE
                    && (g_SimulatedShot.unk2E10 || sLandingCupDistance < NEAR_CUP_DISTANCE)) {
                    kind = FLIGHT_CAMERA_UNK13;
                } else if (g_IsMiniGolf) {
                    pick = RandomBelow(10);
                    switch (pick) {
                        case 0:
                        case 1:
                            kind = FLIGHT_CAMERA_UNK3;
                            break;
                        case 2:
                            kind = FLIGHT_CAMERA_UNKB;
                            break;
                        case 3:
                        case 4:
                            kind = FLIGHT_CAMERA_UNKC;
                            break;
                        default:
                            kind = FLIGHT_CAMERA_UNK12;
                            break;
                    }
                } else {
                    pick = RandomBelow(6);
                    if (pick < 3) {
                        kind = FLIGHT_CAMERA_UNK3;
                    } else if (pick == 3) {
                        if ((D_800BE658 & 2) || g_SimulatedShot.stepCount > 120) {
                            kind = FLIGHT_CAMERA_UNKB;
                        } else {
                            kind = FLIGHT_CAMERA_UNK4;
                        }
                    } else if (pick == 4) {
                        kind = FLIGHT_CAMERA_UNKB;
                    } else {
                        kind = FLIGHT_CAMERA_UNKC;
                    }
                }
                break;
            case 3:
                if (sFlightCameraKind != FLIGHT_CAMERA_UNK12 && sFlightCameraKind != FLIGHT_CAMERA_UNK13
                    && sFlightCameraKind != FLIGHT_CAMERA_UNKB) {
                    kind = FLIGHT_CAMERA_UNKC;
                }
                break;
            case 6:
                if (sFlightCameraKind != FLIGHT_CAMERA_UNK12 && sFlightCameraKind != FLIGHT_CAMERA_UNK13) {
                    kind = FLIGHT_CAMERA_UNKC;
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
                        kind = FLIGHT_CAMERA_UNK5;
                    }
                } else if (pick == 5) {
                    kind = FLIGHT_CAMERA_UNK3;
                    if (D_800BE690 == 1 && g_SimulatedShot.stepCount >= 61) {
                        kind = FLIGHT_CAMERA_UNK4;
                    }
                } else if (pick == 6) {
                    if (D_800BE640 <= SHORT_SHOT_MAX_LENGTH) {
                        kind = FLIGHT_CAMERA_UNKC;
                    } else {
                        kind = FLIGHT_CAMERA_UNKA;
                    }
                } else if (pick == 7) {
                    if (D_800BE640 <= SHORT_SHOT_MAX_LENGTH) {
                        kind = FLIGHT_CAMERA_UNKC;
                    } else {
                        kind = FLIGHT_CAMERA_UNK6;
                    }
                } else if (pick == 8) {
                    kind = FLIGHT_CAMERA_UNKA;
                } else if (pick == 9) {
                    kind = FLIGHT_CAMERA_UNK3;
                    if (D_800BE640 >= MID_SHOT_MIN_LENGTH && g_SimulatedShot.stepCount >= 16) {
                        kind = FLIGHT_CAMERA_UNK7;
                    }
                }
                break;
            case 2:
                if (sFlightCameraKind == FLIGHT_CAMERA_UNK7) {
                    effectIndex = 3;
                    kind = FLIGHT_CAMERA_UNKA;
                } else if (sFlightCameraKind == FLIGHT_CAMERA_UNK3) {
                    if (RandomBelow(3) == 0) {
                        kind = FLIGHT_CAMERA_UNKA;
                    }
                    if (D_800BE640 >= LONG_SHOT_MIN_LENGTH && RandomBelow(4) == 0) {
                        kind = FLIGHT_CAMERA_UNK9;
                    }
                } else if (sFlightCameraKind == FLIGHT_CAMERA_UNK4) {
                    if (g_SimulatedShot.stepCount < 120) {
                        kind = FLIGHT_CAMERA_UNKA;
                    } else if (g_SimulatedShot.unk2E0C) {
                        kind = FLIGHT_CAMERA_UNKB;
                    } else if (D_800BE640 < LONG_SHOT_MIN_LENGTH) {
                        kind = FLIGHT_CAMERA_UNKA;
                    } else if (RandomBelow(4) == 0) {
                        kind = FLIGHT_CAMERA_UNK9;
                    } else {
                        kind = FLIGHT_CAMERA_UNKA;
                    }
                }
                break;
            case 3:
                if (sFlightCameraKind == FLIGHT_CAMERA_UNK4) {
                    if (g_SimulatedShot.stepCount < 120) {
                        kind = FLIGHT_CAMERA_UNKA;
                    } else if (g_SimulatedShot.unk2E0C) {
                        kind = FLIGHT_CAMERA_UNKA;
                    }
                } else if (sFlightCameraKind != FLIGHT_CAMERA_UNK6) {
                    /* The nested ifs and the separate cases are there to match */
                    if (sFlightCameraKind != FLIGHT_CAMERA_UNK9) {
                        if (sFlightCameraKind != FLIGHT_CAMERA_UNKA) {
                            switch (RandomBelow(5)) {
                                case 0:
                                    kind = FLIGHT_CAMERA_UNKA;
                                    break;
                                case 1:
                                    kind = FLIGHT_CAMERA_UNKA;
                                    break;
                                case 2:
                                    kind = FLIGHT_CAMERA_UNKC;
                                    break;
                            }
                        }
                    }
                }
                break;
            case 4:
                if (D_800BE674) {
                    D_800BE630 = TRUE;
                } else if (sFlightCameraKind == FLIGHT_CAMERA_UNK6) {
                    /* The empty branches are there to match */
                } else if (sFlightCameraKind == FLIGHT_CAMERA_UNK9) {
                } else {
                    if (sLandingCupDistance <= SHORT_SHOT_MAX_LENGTH) {
                        if (RandomBelow(2) == 0) {
                            kind = FLIGHT_CAMERA_UNKC;
                        }
                    } else if (RandomBelow(8) == 0) {
                        kind = FLIGHT_CAMERA_UNKC;
                        effectIndex = 3;
                    } else if (RandomBelow(8) < 4) {
                        kind = FLIGHT_CAMERA_UNKC;
                    }
                }
                break;
            case 5:
                kind = FLIGHT_CAMERA_UNKC;
                /* The repeated store is there to match */
                if (IsTroubleSurface(sEndSurfaceInfo) || sFlightCameraKind == FLIGHT_CAMERA_UNK9) {
                    kind = FLIGHT_CAMERA_UNKC;
                }
                break;
            case 6:
                if (!D_800BE674) {
                    kind = FLIGHT_CAMERA_UNKC;
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
                               || sFlightCameraKind == FLIGHT_CAMERA_UNKC) {
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
                            kind = FLIGHT_CAMERA_UNKC;
                        } else {
                            kind = FLIGHT_CAMERA_UNK3;
                        }
                        break;
                    case 6:
                        kind = FLIGHT_CAMERA_UNK3;
                        if (D_800BE690 == 1 && g_SimulatedShot.stepCount >= 61) {
                            kind = FLIGHT_CAMERA_UNK4;
                        }
                        break;
                    case 7:
                        kind = FLIGHT_CAMERA_UNKA;
                        break;
                    case 8:
                        kind = FLIGHT_CAMERA_UNKC;
                        break;
                }
                D_800BE630 = TRUE;
                break;
            case 5:
                kind = FLIGHT_CAMERA_UNKC;
                D_800BE630 = TRUE;
                break;
            case 6:
                /* The empty case is there to match */
                break;
        }
    }
    if (sFlightCameraKind < kind) {
        D_800BE628 = TRUE;
        func_8009226C(NO_EFFECT);
        if (effectIndex == NO_EFFECT) {
            if (D_800DAF50 >= 4 && !g_SimulatedShot.unk2E08 && D_800BE640 > SHORT_SHOT_MAX_LENGTH) {
                if (kind == FLIGHT_CAMERA_UNKC) {
                    if (RandomBelow(16) == 0 && sFlightCameraKind == FLIGHT_CAMERA_UNKA) {
                        func_8009226C(3);
                    } else if (RandomBelow(8) == 0) {
                        func_8009226C(1);
                    } else if (RandomBelow(3) == 0) {
                        func_8009226C(9);
                    }
                }
            }
        } else if (D_800BE640 > SHORT_SHOT_MAX_LENGTH) {
            func_8009226C(effectIndex);
        }
        sFlightCameraKind = kind;
        D_800BE674 = CAMERA_CHANGE_HOLD_COUNT;
    }
    if (g_IsDebugEnabled) {
        if (g_ControllerStates[DEBUG_CAMERA_CONTROLLER].buttons & B_BUTTON) {
            sFlightCameraKind = FLIGHT_CAMERA_UNK4;
        }
        if (g_ControllerStates[DEBUG_CAMERA_CONTROLLER].buttons & A_BUTTON) {
            sFlightCameraKind = FLIGHT_CAMERA_UNK5;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/shot_camera", ChooseFlightCameraKind);
#endif

INCLUDE_ASM("asm/nonmatchings/shot_camera", func_8004B474);

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

#define NOISE_GRID_COUNT 2
#define NOISE_GRID_SIZE 64
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
/* An I8 texture of NOISE_GRID_SIZE x NOISE_GRID_SIZE texels */
extern u8 *g_NoiseTexels;

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
