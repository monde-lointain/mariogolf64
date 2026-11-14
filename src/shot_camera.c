#include "shot_camera.h"

#include <math.h>
#include <stdlib.h>
#include <ultra64.h>

// The original reads the chosen options here as s32; see save.h.
#define WIDENS_CHOSEN_OPTIONS

#include "ball_simulation.h"
#include "camera.h"
#include "character_slot.h"
#include "club.h"
#include "compiler.h"
#include "controller.h"
#include "course.h"
#include "course_space.h"
#include "current_shot.h"
#include "debug_text.h"
#include "framing.h"
#include "game_flags.h"
#include "game_state.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "golf_balls.h"
#include "hole_conditions.h"
#include "hole_display.h"
#include "hole_objects.h"
#include "language.h"
#include "main.h"
#include "math_util.h"
#include "movie.h"
#include "overlay_manager.h"
#include "pin.h"
#include "player.h"
#include "round_events.h"
#include "round_hud.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "screen_layers.h"
#include "shot_effects.h"
#include "shot_simulation.h"
#include "sound.h"
#include "surface.h"
#include "terrain.h"
#include "text.h"
#include "turn.h"

#define RANDOM_YAW_MASK 0x7FFF
#define RANDOM_YAW_TO_RADIANS (M_PI / 16384)

enum ResultSound {
    RESULT_SOUND_NONE,
    RESULT_SOUND_GOOD_SHOT,
    RESULT_SOUND_SUPERB_SHOT
};

typedef s8 ResultSound;

#define RESULT_SOUND_WAIT_FRAME 15

enum FlightPhase {
    FLIGHT_PHASE_NONE,
    FLIGHT_PHASE_LAUNCH,
    FLIGHT_PHASE_EARLY_CLIMB,
    FLIGHT_PHASE_CLIMB,
    FLIGHT_PHASE_FALL,
    FLIGHT_PHASE_NEARING_CONTACT,
    FLIGHT_PHASE_CONTACT,
    FLIGHT_PHASE_END
};

typedef s32 FlightPhase;

enum FlightCameraKind {
    FLIGHT_CAMERA_NONE = -1,
    FLIGHT_CAMERA_UNCHANGED,
    FLIGHT_CAMERA_UNUSED_A,
    FLIGHT_CAMERA_BESIDE_TEE,
    FLIGHT_CAMERA_AIM_VIEW,
    FLIGHT_CAMERA_BESIDE_GOLFER,
    FLIGHT_CAMERA_CIRCLING_BALL,
    FLIGHT_CAMERA_TRAILING_BALL,
    FLIGHT_CAMERA_NEAR_LAUNCH,
    FLIGHT_CAMERA_CHASING_BALL,
    FLIGHT_CAMERA_FOLLOWING_PATH,
    FLIGHT_CAMERA_SIDE_OF_FLIGHT,
    FLIGHT_CAMERA_BEHIND_START,
    FLIGHT_CAMERA_AROUND_END,
    FLIGHT_CAMERA_FACING_CUP,
    FLIGHT_CAMERA_UNUSED_B,
    FLIGHT_CAMERA_UNUSED_C,
    FLIGHT_CAMERA_UNUSED_D,
    FLIGHT_CAMERA_UNUSED_E,
    FLIGHT_CAMERA_FRAMING_PATH,
    FLIGHT_CAMERA_FRAMING_CUP,
    FLIGHT_CAMERA_BILLBOARD_HIT
};

typedef s32 FlightCameraKind;

#define IsChipIn() \
    (!g_SimulatedShot.startsOnGreen && \
        g_Players[g_CurrentShot.playerIndex].strokeCount > 1 && \
        g_SimulatedShot.flagHitStep == NO_STEP)
#define IsPinHitViewed(step) \
    ((g_SimulatedShot.pathFlags[step] & STEP_FLAG_PIN_HIT) || \
        IsMonkeyShown() || \
        f_IsPinHitComing == TRUE)

#define FLIGHT_EVENT_BILLBOARD_HIT (1 << 0)
#define FLIGHT_EVENT_CUP_PASS (1 << 1)
#define SHORT_SHOT_LENGTH_MAX 0xBB7FF
#define CAMERA_CHANGE_HOLD_COUNT 20
#define NOISE_GRID_MASK (NOISE_GRID_SIZE - 1)

#define IsBallLanding() \
    (g_SimulatedShot.stepCount < g_BallFlightStep + LANDING_STEP_COUNT && \
        (f32)f_ShotEndPosition[AXIS_X] < (f32)(f_FlightBallPosition[AXIS_X] + LANDING_RADIUS) && \
        (f32)(f_FlightBallPosition[AXIS_X] - LANDING_RADIUS) < (f32)f_ShotEndPosition[AXIS_X] && \
        (f32)f_ShotEndPosition[AXIS_Z] < (f32)(f_FlightBallPosition[AXIS_Z] + LANDING_RADIUS) && \
        (f32)(f_FlightBallPosition[AXIS_Z] - LANDING_RADIUS) < (f32)f_ShotEndPosition[AXIS_Z])

u8 *g_NoiseTexels;

FILE_STATIC s16 (*f_NoiseGrid)[NOISE_GRID_SIZE];
FILE_STATIC s32 g_BallFlightStep;
FILE_STATIC s16 f_NoiseGrids[NOISE_GRID_COUNT][NOISE_GRID_SIZE][NOISE_GRID_SIZE];

static s32 f_LandingWaitFrame = 0;
static s32 f_EarlierFlightCount = 0;
static Boolean f_IsCameraKindNew = FALSE;
static FlightCameraKind f_FlightCameraKind = FLIGHT_CAMERA_UNCHANGED;
static Boolean f_HasFlightPhaseChanged = FALSE;
static s32 f_BallCupDistance = 0;
static s32 f_BallCupGroundDistance = 0;
static s32 f_LandingCupDistance = 0;
static s32 f_ShotLength = 0;
static s32 f_StartCupDistance = 0;
static Boolean f_HitsBillboardEarly = FALSE;
static s32 f_BillboardHitStep = 0;
static s32 f_BillboardHitCountdown = 0;
static Radians f_CameraYaw = 0.0f;
static s32 f_ComingFlightEvents = 0;
static Radians f_TrailViewAngle = 0.0f;
static s32 f_CupViewHoldCount = 0;
static s32 f_FlightCameraFlags = 0;
static u8 f_UnusedBytesAfterFlightFlags[8] UNUSED = {0};
static ResultSound f_PendingResultSound = RESULT_SOUND_NONE;
static s32 f_CameraChangeHoldCount = 0;
static s32 f_UnusedFlightStartKind = 0;
static Boolean f_IsFlightHudShown = FALSE;
static Boolean f_IsPinHitComing = FALSE;
static Radians f_CupViewYaw = 0.0f;
static EndReaction f_EndReaction = END_REACTION_NONE;
static s32 f_LandingWaitFrameCount = 0;
static Boolean f_IsBallGroundLevel = TRUE;
s32 g_HoleFlyoverFrame = 0;
Boolean g_AreCharactersDrawn = TRUE;
Boolean g_IsCupViewShown = FALSE;
static s32 f_NextCameraPickIndex = 0;
static Boolean f_IsBehindViewFar = FALSE;
static s32 f_SideViewCount = 0;
static Boolean f_IsApexViewFar = FALSE;
static Boolean f_IsTrailViewClose = FALSE;
static Radians f_CircleAngle = 0.0f;
static u8 f_UnusedBytesAfterCircleAngle[8] UNUSED = {0};
static s16 f_NoiseRoughness[] = {0x80, 0x60, 0x40, 0x20, 8, 4, 4, 2};
static s16 f_NoiseGridIndex = 0;
static s32 f_NoiseBlendStep = 0;
static Boolean8 f_IsNoiseResetPending = TRUE;
static s8 f_NoiseRow = 0;

void ChooseFlightCameraKind(void);
void PlaceFlightCamera(s32 step);

static const char f_UnusedFloatFormat[] = "            %f";

#define CAMERA_PICK_COUNT 10
#define CAMERA_PICK_SWAP_COUNT 5
#define NO_CAMERA_PICK (-1)

#define DEFAULT_FIELD_OF_VIEW 45.0f

#define FLIGHT_SOUND_CHANNEL_LANDING 0xB
#define FLIGHT_SOUND_CHANNEL_CUP_IN 0xF
#define SOUND_FADE_FRAME_COUNT 120

#define FLYOVER_FIELD_OF_VIEW 80.0f
#define FLYOVER_INSET_CENTER_X (SCREEN_WIDTH / 2)
#define FLYOVER_INSET_CENTER_Y 112
#define FLYOVER_INSET_SIZE 128
#define FLYOVER_INSET_SOURCE_CODE 100
#define FLYOVER_INSET_SIZE_CODE 101
#define CHOICE_TEXT_SLOT 1

#define CAMERA_EASE_DEAD_ZONE 1.0f
#define CAMERA_EASE_FRACTION 0.2f

#define RING_INNER_RADIUS 5
#define RING_PEAK_RADIUS 55
#define RING_INNER_INTENSITY 10
#define RING_RISE_PER_TEXEL 5
#define RING_PEAK_INTENSITY 255
#define RING_FALL_PER_TEXEL 36

enum ShotLengthRating {
    SHOT_LENGTH_UNRATED,
    SHOT_LENGTH_FULL,
    SHOT_LENGTH_SHORT
};

typedef s32 ShotLengthRating;

#define FULL_SHOT_FRACTION 0.8f
#define SHORT_SHOT_FRACTION 0.6f
#define IsShotBeyond(fraction) \
    (GetSelectedShotDistance() * (fraction) < f_ShotLength * YARDS_PER_UNIT)
#define IsAlbatrossOrBetter(strokes) (g_HoleSetup.par + SCORE_TO_PAR_ALBATROSS >= (strokes))

#define FLYOVER_HALF_TURN_FRAME_COUNT 90
#define FLYOVER_HALF_TURNS_MAX 2.0f
#define FLYOVER_INSET_OPEN_FRAME 94
#define FLYOVER_INSET_CLOSE_FRAME 162
#define FLYOVER_RADIUS_APPROACH 15.0f
#define FLYOVER_DISTANCE_SCALE 1536.0f
#define FLYOVER_HEIGHT_APPROACH 200.0f
#define FLYOVER_HEIGHT_SCALE 1.5f
#define FLYOVER_INSET_LOOK_AT_DROP 15360.0f
#define FLYOVER_INSET_DISTANCE 184320.0f
#define FLYOVER_INSET_EYE_RISE 75.0f

#define STRONG_WIND_SPEED 6

#define LOOK_AT_DROP 19.650002f
#define LOOK_AT_EASE_FRACTION 0.2f
#define LOOK_AT_HEIGHT_EASE_FRACTION 0.5f
#define LANDING_STEP_COUNT 10
#define LANDING_RADIUS 0x1E00

#define FLIGHT_CAMERA_FLAG_CUP_VIEW_ALLOWED (1 << 1)

#define CUP_PASS_RADIUS 0x1E00
#define CUP_PASS_DEPTH_MAX 0x1A400
#define CUP_PASS_LANDING_DISTANCE_MAX 0x4B000

static s8 f_CameraPicks[CAMERA_PICK_COUNT];
static CoursePoint f_FlightBallPosition;
static CoursePoint f_ShotEndPosition;
static CoursePoint f_ShotStartPosition;
static Radians f_ShotFlightYaw;
static Radians f_BillboardHitYaw;
static Radians f_EndYawFromCup;
static Radians f_StartYawFromCup;
static Boolean f_IsLaterFlight;
static Vec3 f_CameraPosition;
static Vec3 f_LookAtPoint;
static s32 f_GreenSoundCount;
static Boolean f_IsLandingSoundPlayed;
static s32 f_UnusedClearedFlightWord;
static StepFlags8 f_PassedPathFlags;
static SurfaceTraits *f_EndSurfaceTraits;
static FlightPhase f_FlightPhase;

#define STANCE_AIM_OFFSET (-921.60004f)
#define STANCE_SLOPE_PASS_COUNT 2

void PlaceGolferAtBall(Vec3 position, Vec3 rotation, Vec3 scale) {
    Vec3 groundPoint;
    Mat4 unusedMatrices[2] UNUSED;
    Player *player;
    SignedBinaryAngle aimYaw;
    f32 distance;
    Radians angle;
    f32 reach;
    f32 slope;
    s32 ballHeight;
    s32 passIndex;

    player = &g_Players[g_CurrentShot.playerIndex];
    aimYaw = g_CurrentShot.heading;
    distance =
        GetBallDistance(0, player->clubs[g_CurrentShot.clubSlot], FALSE, g_CurrentShot.shotMode);
    angle = ((aimYaw + BINARY_ANGLE_QUARTER_TURN) & BINARY_ANGLE_MASK) * BINARY_ANGLE_TO_RADIANS;
    if (player->isLefty) {
        distance = -distance;
    }

    groundPoint[AXIS_X] =
        (cosf(angle) * distance) + (sinf(angle) * STANCE_AIM_OFFSET) + player->ballPosition[AXIS_X];
    groundPoint[AXIS_Z] =
        (sinf(angle) * distance) + (cosf(angle) * STANCE_AIM_OFFSET) + player->ballPosition[AXIS_Z];
    groundPoint[AXIS_Y] = GetGroundHeight(groundPoint[AXIS_X], groundPoint[AXIS_Z]);
    ballHeight = player->ballPosition[AXIS_Y];
    slope = atan2(groundPoint[AXIS_Y] - ballHeight, distance);
    for (passIndex = 0; passIndex < STANCE_SLOPE_PASS_COUNT; passIndex++) {
        reach = cosf(slope) * distance;
        position[AXIS_X] = (cosf(angle) * reach) + (sinf(angle) * STANCE_AIM_OFFSET) +
            player->ballPosition[AXIS_X];
        position[AXIS_Z] = (sinf(angle) * reach) - (cosf(angle) * STANCE_AIM_OFFSET) +
            player->ballPosition[AXIS_Z];
        position[AXIS_Y] = GetGroundHeight(position[AXIS_X], position[AXIS_Z]);
        if (position[AXIS_Y] - ballHeight > AbsFloat(distance)) {
            position[AXIS_Y] = AbsFloat(distance) + ballHeight;
        }

        rotation[AXIS_X] = atan2(position[AXIS_Y] - ballHeight, reach);
        slope = rotation[AXIS_X];
    }

    scale[AXIS_Z] = 1.0f;
    scale[AXIS_Y] = scale[AXIS_Z];
    scale[AXIS_X] = scale[AXIS_Y];
    if (player->isLefty) {
        scale[AXIS_Y] = -1.0f;
    }

    rotation[AXIS_Z] = 0.0f;
}

#define CROSS_SLOPE_DISTANCE 0x3C00
#define CROSS_SLOPE_CAPPED_RISE 0x1E00
#define AIM_SLOPE_STEP 6144.0f
#define AIM_SLOPE_RISE_MAX 0x1800

SignedBinaryAngle GetCrossSlopeAngle(Player *player, ShotLaunch *shot) {
    SurfaceTraits *surfaceTraits;
    s32 sampleDistance;
    SignedBinaryAngle aimYaw;
    WideBinaryAngle yaw;
    Radians angle;
    s32 height;
    s32 rise;

    aimYaw = shot->heading;

    surfaceTraits = GetSurfaceTraits(player->ballSurface);
    sampleDistance = CROSS_SLOPE_DISTANCE;
    yaw = aimYaw + BINARY_ANGLE_QUARTER_TURN;
    if (player->isLefty) {
        angle = BINARY_ANGLE_TO_RADIANS * ((yaw + BINARY_ANGLE_HALF_TURN) & BINARY_ANGLE_MASK);
    }
    else {
        angle = (yaw & BINARY_ANGLE_MASK) * BINARY_ANGLE_TO_RADIANS;
    }

    height = GetGroundHeight(
        (s32)(cos(angle) * sampleDistance) + player->ballPosition[AXIS_X],
        (s32)(sin(angle) * sampleDistance) + player->ballPosition[AXIS_Z]
    );
    if (surfaceTraits->kind == SURFACE_KIND_TEE_UP) {
        height -= TEE_HEIGHT;
    }

    if (height < player->ballPosition[AXIS_Y] - sampleDistance) {
        height = player->ballPosition[AXIS_Y] - sampleDistance;
    }

    if (height > player->ballPosition[AXIS_Y] + sampleDistance) {
        height = player->ballPosition[AXIS_Y] + sampleDistance;
    }

    rise = height - player->ballPosition[AXIS_Y];
    if (player->isLefty) {
        rise = -rise;
    }

    if (rise > CROSS_SLOPE_DISTANCE) {
        rise = CROSS_SLOPE_CAPPED_RISE;
    }
    else if (rise < -CROSS_SLOPE_DISTANCE) {
        rise = -CROSS_SLOPE_CAPPED_RISE;
    }

    return atan2(rise, sampleDistance) * RADIANS_TO_BINARY_ANGLE;
}

SignedBinaryAngle GetAimSlopeAngle(Player *player, ShotLaunch *shot) {
    Radians angle;
    f32 step[2];
    s32 aheadHeight;
    s32 behindHeight;

    angle = (u16)shot->heading * BINARY_ANGLE_TO_RADIANS;
    step[0] = cosf(angle) * AIM_SLOPE_STEP;
    step[1] = sinf(angle) * AIM_SLOPE_STEP;
    aheadHeight = GetGroundHeight(
        ((s32)step[0] / 2) + player->ballPosition[AXIS_X],
        ((s32)step[1] / 2) + player->ballPosition[AXIS_Z]
    );
    if (aheadHeight < player->ballPosition[AXIS_Y] - AIM_SLOPE_RISE_MAX) {
        aheadHeight = player->ballPosition[AXIS_Y] - AIM_SLOPE_RISE_MAX;
    }

    if (aheadHeight > player->ballPosition[AXIS_Y] + AIM_SLOPE_RISE_MAX) {
        aheadHeight = player->ballPosition[AXIS_Y] + AIM_SLOPE_RISE_MAX;
    }

    behindHeight = GetGroundHeight(
        player->ballPosition[AXIS_X] - (s32)step[0],
        player->ballPosition[AXIS_Z] - (s32)step[1]
    );
    if (behindHeight < player->ballPosition[AXIS_Y] - AIM_SLOPE_RISE_MAX) {
        behindHeight = player->ballPosition[AXIS_Y] - AIM_SLOPE_RISE_MAX;
    }

    if (behindHeight > player->ballPosition[AXIS_Y] + AIM_SLOPE_RISE_MAX) {
        behindHeight = player->ballPosition[AXIS_Y] + AIM_SLOPE_RISE_MAX;
    }

    return atan2(aheadHeight - behindHeight, AIM_SLOPE_STEP * 1.5) * RADIANS_TO_BINARY_ANGLE;
}

static const f64 f_UnusedZero = 0.0;

SurfaceTraits *GetBallSurfaceTraits(Player *player, ShotLaunch *shot UNUSED) {
    return GetSurfaceTraitsAt(player->ballPosition[AXIS_X], player->ballPosition[AXIS_Z]);
}

Boolean IsDebugBButtonView(void) {
    return f_FlightCameraKind == FLIGHT_CAMERA_BESIDE_GOLFER;
}

s32 NextCameraPick(Boolean isDealing) {
    s32 index;
    s32 pick;

    void SwapCameraPicks(s32 index, s32 otherIndex) {
        s32 pick;

        pick = f_CameraPicks[index];
        f_CameraPicks[index] = f_CameraPicks[otherIndex];
        f_CameraPicks[otherIndex] = pick;
    }

    if (!isDealing) {
        f_NextCameraPickIndex = 0;

        for (index = 0; index != CAMERA_PICK_COUNT; index++) {
            f_CameraPicks[index] = index;
        }

        for (index = 0; index != CAMERA_PICK_SWAP_COUNT; index++) {
            SwapCameraPicks(index, rand() % CAMERA_PICK_COUNT);
        }

        return NO_CAMERA_PICK;
    }

    pick = f_CameraPicks[f_NextCameraPickIndex];
    if (++f_NextCameraPickIndex == CAMERA_PICK_COUNT) {
        NextCameraPick(FALSE);
    }

    return pick;
}

void StopFlightCamera(void) {
    g_AreCharactersDrawn = TRUE;
    g_IsCupViewShown = FALSE;
    f_CupViewHoldCount = 0;
    g_BallTrailState = BALL_TRAIL_OFF;
    g_Cameras[MAIN_CAMERA].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
    StartHudAnimation(HUD_ANIMATION_NONE);
    g_InsetView.animation = INSET_VIEW_STILL;
    g_InsetView.step = 0;
    g_Cameras[INSET_CAMERA].view.drawFlags = 0;
    FadeOutSound(FLIGHT_SOUND_CHANNEL_LANDING, SOUND_FADE_FRAME_COUNT);
    FadeOutSound(FLIGHT_SOUND_CHANNEL_CUP_IN, SOUND_FADE_FRAME_COUNT);
    ClearShotEffects(CLEAR_KIND_EFFECTS);
    f_FlightCameraKind = FLIGHT_CAMERA_NONE;
    if (g_BallFlightStep == g_SimulatedShot.stepCount) {
        osSyncPrintf("\nCheckWater1\n");
        if (g_SimulatedShot.isWaterCut) {
            osSyncPrintf("\nCheckWater2\n");
            CountNotableShot(g_CurrentShot.playerIndex, NOTABLE_SHOT_WATER_CUT);
        }
    }
}

Boolean IsCupPassPoint(s32 cupDistance, s32 height);
s32 RandomBelow(s32 limit);

#define SHOT_LENGTH_MIN 0x7800
#define SCANNED_STEP_COUNT 30
#define EARLY_BILLBOARD_HIT_COUNTDOWN 40
#define AHEAD_BILLBOARD_HIT_COUNTDOWN 70
#define GROUND_PROBE_COUNT 5
#define GROUND_PROBE_ANGLE_STEP (M_PI / 4)
#define GROUND_PROBE_DISTANCE 30720.0f
#define GROUND_PROBE_RISE_MAX 4608.0f
#define LEVEL_LIE_TILT_DEGREES 10

void StartFlightCamera(s32 earlierFlightCount) {
    CoursePoint probe;
    s32 i;
    Radians yaw;
    f32 degrees;

    f_EarlierFlightCount = earlierFlightCount;
    ClearShotEffects(CLEAR_KIND_ALL);
    ResetBallTrail();
    f_CameraChangeHoldCount = 0;
    f_EndSurfaceTraits = GetSurfaceTraits(g_SimulatedShot.endSurface);
    if (earlierFlightCount == 0) {
        NextCameraPick(FALSE);
    }

    StartHudAnimation(HUD_ANIMATION_NONE);
    StartBanner(BANNER_NONE);
    f_FlightCameraFlags = 0;

    if (earlierFlightCount == 0) {
        f_FlightCameraFlags |= FLIGHT_CAMERA_FLAG_CUP_VIEW_ALLOWED;
    }
    else if (RandomBelow(2) == 0) {
        f_FlightCameraFlags |= FLIGHT_CAMERA_FLAG_CUP_VIEW_ALLOWED;
    }

    if (earlierFlightCount == 0) {
        g_CapturedSliceCount = 0;
    }

    g_InsetView.animation = INSET_VIEW_STILL;
    g_InsetView.step = 0;
    g_Cameras[INSET_CAMERA].view.drawFlags = 0;

    if (!g_SimulatedShot.startsOnGreen) {
        if (earlierFlightCount != 0 && g_ContactEffectAge == NO_CONTACT_EFFECT_AGE) {
            g_RoundHud.isBallTrailShown = TRUE;
        }
        else {
            g_RoundHud.isBallTrailShown = FALSE;
        }
    }
    else {
        g_RoundHud.isBallTrailShown = FALSE;
    }

    g_BallFlightStep = 0;
    f_LandingWaitFrame = 0;
    f_IsCameraKindNew = FALSE;
    f_CupViewHoldCount = 0;
    f_GreenSoundCount = 0;
    f_IsLandingSoundPlayed = FALSE;
    f_UnusedClearedFlightWord = 0;
    f_EndReaction = END_REACTION_NONE;
    f_CameraYaw = (f32)RandomBelow(RANDOM_YAW_MASK) * RANDOM_YAW_TO_RADIANS;
    f_TrailViewAngle = (f32)(rand() & RANDOM_YAW_MASK) * RANDOM_YAW_TO_RADIANS;
    f_CupViewYaw = (f32)(rand() & RANDOM_YAW_MASK) * RANDOM_YAW_TO_RADIANS;
    if (earlierFlightCount == 0) {
        f_IsLaterFlight = FALSE;
    }
    else {
        f_IsLaterFlight = TRUE;
    }

    f_UnusedFlightStartKind = 0;
    if (g_SimulatedShot.startsOnGreen) {
        f_UnusedFlightStartKind = 1;
    }
    else if (f_EndSurfaceTraits->kind == SURFACE_KIND_WATER_HAZARD) {
        f_UnusedFlightStartKind = 3;
    }

    f_PassedPathFlags = 0;
    f_PendingResultSound = RESULT_SOUND_NONE;
    f_IsFlightHudShown = TRUE;
    if (f_IsLaterFlight) {
        f_IsFlightHudShown = RandomBelow(2);
        g_RoundHud.isClubPanelShown = f_IsFlightHudShown;
        g_RoundHud.isWindIndicatorShown = f_IsFlightHudShown;
        g_RoundHud.isHoleInfoShown = f_IsFlightHudShown;
        g_RoundHud.isLieIconShown = f_IsFlightHudShown;
        g_RoundHud.isClubHeadIconShown = f_IsFlightHudShown;
        g_RoundHud.isUnreadItemShown = f_IsFlightHudShown;
        g_RoundHud.isProgressShown = f_IsFlightHudShown;
    }

    f_ShotStartPosition[AXIS_X] = (f32)g_SimulatedShot.startPosition[AXIS_X];
    f_ShotStartPosition[AXIS_Y] = (f32)g_SimulatedShot.startPosition[AXIS_Y];
    f_ShotStartPosition[AXIS_Z] = (f32)g_SimulatedShot.startPosition[AXIS_Z];
    f_ShotEndPosition[AXIS_X] = (f32)g_SimulatedShot.endPosition[AXIS_X];
    f_ShotEndPosition[AXIS_Y] = (f32)g_SimulatedShot.endPosition[AXIS_Y];
    f_ShotEndPosition[AXIS_Z] = (f32)g_SimulatedShot.endPosition[AXIS_Z];
    f_ShotFlightYaw = -atan2(
        (f32)(f_ShotEndPosition[AXIS_X] - f_ShotStartPosition[AXIS_X]),
        (f32)(f_ShotEndPosition[AXIS_Z] - f_ShotStartPosition[AXIS_Z])
    );
    f_EndYawFromCup = -atan2(
        (f32)(f_ShotEndPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X]),
        (f32)(f_ShotEndPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z])
    );
    f_StartYawFromCup = -atan2(
        (f32)(f_ShotStartPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X]),
        (f32)(f_ShotStartPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z])
    );
    f_StartCupDistance = GetCupDistance(
        f_ShotStartPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
        f_ShotStartPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]
    );
    f_ShotLength = IntLength3D(
        f_ShotStartPosition[AXIS_X] - f_ShotEndPosition[AXIS_X],
        f_ShotStartPosition[AXIS_Y] - f_ShotEndPosition[AXIS_Y],
        f_ShotStartPosition[AXIS_Z] - f_ShotEndPosition[AXIS_Z]
    );
    if (f_ShotLength < SHOT_LENGTH_MIN) {
        f_ShotLength = SHOT_LENGTH_MIN;
    }

    f_LandingCupDistance = GetCupDistance(
        f_ShotEndPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
        f_ShotEndPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]
    );
    f_ComingFlightEvents = 0;

    for (i = 0; i != SCANNED_STEP_COUNT; i++) {
        if (i < g_SimulatedShot.stepCount &&
            IsCupPassPoint(
                IntLength2D(
                    g_SimulatedShot.path[i][AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
                    g_SimulatedShot.path[i][AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]
                ),
                g_SimulatedShot.path[i][AXIS_Y]
            )) {
            f_ComingFlightEvents |= FLIGHT_EVENT_CUP_PASS;
        }
    }

    f_HitsBillboardEarly = FALSE;
    f_BillboardHitStep = NO_STEP;
    f_BillboardHitCountdown = -1;
    for (i = 0; i != SCANNED_STEP_COUNT; i++) {
        if (i < g_SimulatedShot.stepCount &&
            (g_SimulatedShot.pathFlags[i] & STEP_FLAG_BILLBOARD_HIT)) {
            f_HitsBillboardEarly = TRUE;
            f_ComingFlightEvents |= FLIGHT_EVENT_BILLBOARD_HIT;
            if (f_BillboardHitStep == NO_STEP) {
                f_BillboardHitStep = i;
                f_BillboardHitCountdown = EARLY_BILLBOARD_HIT_COUNTDOWN;
            }
        }
    }

    f_IsBallGroundLevel = TRUE;
    for (i = 0; i != GROUND_PROBE_COUNT; i++) {
        if (g_Players[g_CurrentShot.playerIndex].isLefty == TRUE) {
            yaw = ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) +
                ((f32)i * GROUND_PROBE_ANGLE_STEP);
        }
        else {
            yaw = ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) -
                ((f32)i * GROUND_PROBE_ANGLE_STEP);
        }

        probe[AXIS_X] = (cosf(yaw) * GROUND_PROBE_DISTANCE) + g_CurrentShot.ballPosition[AXIS_X];
        probe[AXIS_Z] = (sinf(yaw) * GROUND_PROBE_DISTANCE) + g_CurrentShot.ballPosition[AXIS_Z];
        probe[AXIS_Y] = GetGroundHeight(probe[AXIS_X], probe[AXIS_Z]);
        if (probe[AXIS_Y] < g_CurrentShot.ballPosition[AXIS_Y] - GROUND_PROBE_RISE_MAX) {
            f_IsBallGroundLevel = FALSE;
        }

        if (probe[AXIS_Y] > g_CurrentShot.ballPosition[AXIS_Y] + GROUND_PROBE_RISE_MAX) {
            f_IsBallGroundLevel = FALSE;
        }
    }

    degrees = g_RoundHud.lieTilt * (DEGREES_PER_TURN * BINARY_ANGLE_TO_RADIANS) *
        (1.0 / FULL_TURN_RADIANS);
    if (degrees < DEGREES_PER_TURN - LEVEL_LIE_TILT_DEGREES && degrees > LEVEL_LIE_TILT_DEGREES) {
        f_IsBallGroundLevel = FALSE;
    }

    f_IsCameraKindNew = FALSE;
    f_HasFlightPhaseChanged = FALSE;
    f_FlightPhase = FLIGHT_PHASE_LAUNCH;
    f_FlightCameraKind = FLIGHT_CAMERA_NONE;
    f_IsPinHitComing = FALSE;
    g_Cameras[MAIN_CAMERA].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
    f_FlightBallPosition[AXIS_X] = f_ShotStartPosition[AXIS_X];
    f_FlightBallPosition[AXIS_Y] = f_ShotStartPosition[AXIS_Y];
    f_FlightBallPosition[AXIS_Z] = f_ShotStartPosition[AXIS_Z];
    ChooseFlightCameraKind();
    f_IsCameraKindNew = TRUE;
    PlaceFlightCamera(0);
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] = f_CameraPosition[AXIS_X];
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] = f_CameraPosition[AXIS_Y];
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] = f_CameraPosition[AXIS_Z];
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] = f_LookAtPoint[AXIS_X];
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] = f_LookAtPoint[AXIS_Y];
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] = f_LookAtPoint[AXIS_Z];
}

#define LEADING_CLUB_SLOT_COUNT 6

#define IsPerfectPowerShot() \
    (g_SimulatedShot.shot.shotMode == SHOT_MODE_POWER && g_SimulatedShot.isPerfectSwing)

void PlayContactSound(void) {
    Club *club;
    SurfaceTraits *surfaceTraits;

    club =
        GetClub(g_Players[g_SimulatedShot.shot.playerIndex].clubs[g_SimulatedShot.shot.clubSlot]);
    surfaceTraits = GetSurfaceTraits(g_CurrentShot.surface);
    if (club->category == CLUB_CATEGORY_WOOD) {
        if (IsPerfectPowerShot()) {
            PlaySound(SOUND_MENU_START, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
            StartContactEffect(CONTACT_EFFECT_POWER_WOOD);
        }
        else if (surfaceTraits->kind == SURFACE_KIND_TEE_UP) {
            StartContactEffect(CONTACT_EFFECT_SHOT);
            PlaySound(SOUND_MENU_START, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
        else {
            StartContactEffect(CONTACT_EFFECT_SHOT);
            PlaySound(SOUND_CONTACT_WOOD, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }

        StartImpactSparks();
    }
    else if (club->category == CLUB_CATEGORY_IRON || club->category == CLUB_CATEGORY_WEDGE) {
        if (IsPerfectPowerShot()) {
            if (g_CurrentShot.clubSlot < LEADING_CLUB_SLOT_COUNT) {
                StartContactEffect(CONTACT_EFFECT_POWER_IRON);
            }
            else {
                StartContactEffect(CONTACT_EFFECT_SHOT);
            }
        }
        else {
            StartContactEffect(CONTACT_EFFECT_SHOT);
        }

        if (surfaceTraits->kind == SURFACE_KIND_TEE_UP) {
            PlaySound(SOUND_CONTACT_IRON_TEE, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
        else if (IsFairwayKind(surfaceTraits->kind)) {
            PlaySound(SOUND_ENTER_TOURNAMENT, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
        else if (IsBunkerKind(surfaceTraits->kind)) {
            PlaySound(SOUND_CONTACT_IRON_BUNKER, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
        else {
            PlaySound(SOUND_ENTER_TOURNAMENT, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
    }
    else {
        PlaySound(SOUND_CONTACT_PUTTER, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
        if (f_FlightCameraKind != FLIGHT_CAMERA_FRAMING_CUP) {
            StartContactEffect(CONTACT_EFFECT_PUTT);
        }
    }
}

s32 RandomBelow(s32 limit) {
    if (limit == 0) {
        limit = 1;
    }

    return (guRandom() >> 2) % limit;
}

Boolean IsCupPassPoint(s32 cupDistance, s32 height) {
    if (cupDistance < CUP_PASS_RADIUS &&
        height > g_HoleSetup.cupPosition[AXIS_Y] - CUP_PASS_DEPTH_MAX &&
        f_LandingCupDistance < CUP_PASS_LANDING_DISTANCE_MAX &&
        (f_FlightCameraFlags & FLIGHT_CAMERA_FLAG_CUP_VIEW_ALLOWED)) {
        return TRUE;
    }

    return FALSE;
}

#define IsShotLengthRated(endKind) \
    ((endKind) != SURFACE_KIND_WATER_HAZARD && \
        f_StartCupDistance >= f_LandingCupDistance && \
        f_HitsBillboardEarly != TRUE && \
        g_SimulatedShot.isPerfectSwing && \
        !g_SimulatedShot.isFirstBounceOut && \
        g_SimulatedShot.shot.shotMode != SHOT_MODE_APPROACH)

ShotLengthRating RateShotLength(void) {
    SurfaceKind endKind;

    endKind = f_EndSurfaceTraits->kind;
    if (endKind == SURFACE_KIND_OUT_OF_COURSE) {
        return SHOT_LENGTH_UNRATED;
    }

    if (endKind == SURFACE_KIND_OUT_OF_BOUNDS) {
        return SHOT_LENGTH_UNRATED;
    }

    if (IsShotLengthRated(endKind)) {
        if (IsShotBeyond(FULL_SHOT_FRACTION)) {
            return SHOT_LENGTH_FULL;
        }

        if (IsShotBeyond(SHORT_SHOT_FRACTION)) {
            return SHOT_LENGTH_SHORT;
        }
    }

    return SHOT_LENGTH_UNRATED;
}

s32 GetEndReaction(void) {
    return f_EndReaction;
}

#define FLIGHT_SOUND_CHANNEL_CALLS 3
#define FLIGHT_SOUND_CHANNEL_RESULT 0xE
#define EAGLE_CUP_IN_VOLUME 0x50
#define BIRDIE_CUP_IN_VOLUME 0x60
#define ALBATROSS_CUP_IN_VOLUME 0x64
#define GREEN_SOUND_VOLUME 0x5F
#define FAIRWAY_SOUND_VOLUME 0x3F
#define FULL_SHOT_REVERB 0x14

#define RUMBLE_REACTION_STEP 10
#define FULL_SHOT_REACTION_STEP 14
#define SHOT_RESULT_STEP 20
#define EARLY_STEP_COUNT 10
#define GREEN_SOUND_DELAY_STEP_COUNT 2

#define HasHitGroundBefore(step, stepsBack) \
    (g_SimulatedShot.pathFlags[(step) - (stepsBack)] & STEP_FLAG_GROUND_HIT)
#define LANDING_SOUND_STEP_COUNT 3
#define CHIP_IN_CALL_STEP_COUNT 10
#define FLAG_SHOT_CALL_STEP_COUNT 15
#define PIN_HIT_LOOKAHEAD_STEP_COUNT 5

#define CLOSE_LANDING_DISTANCE 0x3C00
#define NEAR_LANDING_DISTANCE 0x5A00
#define GREEN_SOUND_DISTANCE_MAX 0x7800
#define NICE_TOUCH_DISTANCE_MIN 0x25800
#define LONG_PUTT_SOUND_DISTANCE_MIN 0x4B000
#define LONG_LANDING_DISTANCE_MIN (60.0f * UNITS_PER_YARD)

static inline void QueueOnlookerSounds(CharacterSoundKind kind) {
    s32 playerIndex;

    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        if (playerIndex != g_CurrentShot.playerIndex) {
            QueueCharacterSound(
                g_Players[playerIndex].characterIndex,
                playerIndex,
                kind,
                SOUND_FULL_SCALE,
                g_Players[playerIndex].soundPan
            );
        }
    }
}

#define IsApproachShot() \
    (g_SimulatedShot.shot.shotMode == SHOT_MODE_APPROACH || g_SimulatedShot.usesPutter)

#define IsNiceTouchPutt() \
    (f_LandingCupDistance < CLOSE_LANDING_DISTANCE && \
        f_StartCupDistance > NICE_TOUCH_DISTANCE_MIN && \
        !(f_PassedPathFlags & (STEP_FLAG_LIPPED_OUT | STEP_FLAG_PIN_HIT)) && \
        g_HoleSetup.par > g_Players[g_CurrentShot.playerIndex].strokeCount)

#define IsShotOntoGreen() \
    (!g_SimulatedShot.usesPutter && \
        !g_SimulatedShot.startsOnGreen && \
        g_SimulatedShot.endsOnGreen && \
        f_EndSurfaceTraits->kind == SURFACE_KIND_GREEN)

void PlayFlightSounds(s32 step) {
    s32 shownRingCount;
    s32 scoreToPar;
    s32 i;
    SurfaceTraits *surfaceTraits;
    Player *player;
    s32 *greenSoundCount;
    Boolean *isPlayed;
    Boolean *isStillPlayed;

    scoreToPar = g_Players[g_CurrentShot.playerIndex].strokeCount - g_HoleSetup.par;
    shownRingCount = 0;
    if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
        player = &g_Players[g_RoundHud.shooterIndex];
        for (i = 0; i != RING_COUNT; i++) {
            shownRingCount += player->ringStates[i] == RING_STATE_SHOWN;
        }
    }

    f_PassedPathFlags |= g_SimulatedShot.pathFlags[step];
    if (step == FULL_SHOT_REACTION_STEP && RateShotLength() == SHOT_LENGTH_FULL) {
        QueueOnlookerSounds(CHARACTER_SOUND_KIND_FULL_SHOT);
    }

    if (step == SHOT_RESULT_STEP) {
        if (f_EndSurfaceTraits->kind == SURFACE_KIND_OUT_OF_COURSE && !g_GameState.isMiniGolf) {
            PlaySound(SOUND_OUT_OF_COURSE, FLIGHT_SOUND_CHANNEL_CALLS, SOUND_FULL_SCALE);
            QueueOnlookerSounds(CHARACTER_SOUND_KIND_OUT_OF_COURSE);
        }
        else if (RateShotLength() == SHOT_LENGTH_FULL) {
            PlaySoundWithEffects(
                SOUND_FULL_SHOT,
                FLIGHT_SOUND_CHANNEL_CALLS,
                SOUND_FULL_SCALE,
                FULL_SHOT_REVERB,
                g_Players[g_CurrentShot.playerIndex].soundPan
            );
        }
    }

    if (step == RUMBLE_REACTION_STEP && g_SimulatedShot.impactRumbles) {
        for (i = 0; i != g_GameState.playerCount; i++) {
            PlaySound(SOUND_RUMBLING_MISHIT, FLIGHT_SOUND_CHANNEL_CALLS, SOUND_FULL_SCALE);
            if (i != g_CurrentShot.playerIndex) {
                QueueCharacterSound(
                    g_Players[i].characterIndex,
                    i,
                    CHARACTER_SOUND_KIND_TROUBLE,
                    SOUND_FULL_SCALE,
                    g_Players[i].soundPan
                );
            }
        }
    }

    if (step > EARLY_STEP_COUNT) {
        greenSoundCount = &f_GreenSoundCount;
        if (!*greenSoundCount &&
            !shownRingCount &&
            HasHitGroundBefore(step, GREEN_SOUND_DELAY_STEP_COUNT) &&
            IsShotOntoGreen()) {
            if (g_HoleSetup.par > g_Players[g_CurrentShot.playerIndex].strokeCount + 1) {
                (*greenSoundCount)++;
                PlaySound(SOUND_UNDER_PAR_CHANCE, FLIGHT_SOUND_CHANNEL_LANDING, GREEN_SOUND_VOLUME);
            }
            else if (g_HoleSetup.par > g_Players[g_CurrentShot.playerIndex].strokeCount + 2 &&
                f_LandingCupDistance < GREEN_SOUND_DISTANCE_MAX) {
                (*greenSoundCount)++;
                PlaySound(SOUND_EAGLE_CUP_IN, FLIGHT_SOUND_CHANNEL_LANDING, GREEN_SOUND_VOLUME);
            }
        }
    }

    if (f_LandingWaitFrame == RESULT_SOUND_WAIT_FRAME) {
        if (f_PendingResultSound == RESULT_SOUND_GOOD_SHOT) {
            PlaySound(SOUND_GOOD_SHOT, FLIGHT_SOUND_CHANNEL_RESULT, SOUND_FULL_SCALE);
        }
        else if (f_PendingResultSound == RESULT_SOUND_SUPERB_SHOT) {
            PlaySound(SOUND_THREE_STARS, FLIGHT_SOUND_CHANNEL_RESULT, SOUND_FULL_SCALE);
        }
    }

    if (!g_GameState.isMiniGolf) {
        if ((g_SimulatedShot.pathFlags[step + PIN_HIT_LOOKAHEAD_STEP_COUNT] & STEP_FLAG_PIN_HIT) &&
            g_SimulatedShot.flagHitStep == NO_STEP) {
            f_IsPinHitComing = TRUE;
        }

        if ((g_SimulatedShot.pathFlags[step] & STEP_FLAG_PIN_HIT) &&
            !g_SimulatedShot.endsInCup &&
            g_SimulatedShot.flagHitStep == NO_STEP) {
            f_IsPinHitComing = FALSE;
            StartMonkeyDrop(f_CupViewYaw);
        }
    }

    if (shownRingCount == 0 &&
        g_BallFlightStep == g_SimulatedShot.stepCount - 1 &&
        !g_SimulatedShot.endsInCup) {
        if (g_SimulatedShot.startsOnGreen) {
            if (IsNiceTouchPutt()) {
                StartBanner(BANNER_NICE_TOUCH);
                PlaySound(SOUND_CALL_NICE_TOUCH, FLIGHT_SOUND_CHANNEL_CALLS, SOUND_FULL_SCALE);
            }
        }
        else if (g_SimulatedShot.flagHitStep != NO_STEP) {
            StartBanner(BANNER_FLAG_SHOT);
            PlaySound(SOUND_CALL_FLAG_SHOT, FLIGHT_SOUND_CHANNEL_CALLS, SOUND_FULL_SCALE);
            CountNotableShot(g_CurrentShot.playerIndex, NOTABLE_SHOT_FLAG_SHOT);
            f_PendingResultSound = RESULT_SOUND_SUPERB_SHOT;
        }
        else if (f_PassedPathFlags & STEP_FLAG_PIN_HIT) {
            CountNotableShot(g_CurrentShot.playerIndex, NOTABLE_SHOT_PIN_SHOT);
            StartBanner(BANNER_PIN_SHOT);
            PlaySound(SOUND_CALL_PIN_SHOT, FLIGHT_SOUND_CHANNEL_CALLS, SOUND_FULL_SCALE);
            f_PendingResultSound = RESULT_SOUND_SUPERB_SHOT;
        }
        else if (g_SimulatedShot.endsOnGreen) {
            if (IsAlbatrossOrBetter(g_Players[g_CurrentShot.playerIndex].strokeCount)) {
                CountNotableShot(g_CurrentShot.playerIndex, NOTABLE_SHOT_GREAT);
                StartBanner(BANNER_GREAT);
                PlaySound(SOUND_CALL_GREAT, FLIGHT_SOUND_CHANNEL_CALLS, SOUND_FULL_SCALE);
                QueueOnlookerSounds(CHARACTER_SOUND_KIND_GREAT);
                f_PendingResultSound = RESULT_SOUND_SUPERB_SHOT;
            }
            else if (g_HoleSetup.par + SCORE_TO_PAR_EAGLE ==
                g_Players[g_CurrentShot.playerIndex].strokeCount) {
                if (f_LandingCupDistance <= CLOSE_LANDING_DISTANCE) {
                    CountNotableShot(g_CurrentShot.playerIndex, NOTABLE_SHOT_EXCELLENT);
                    StartBanner(BANNER_EXCELLENT);
                    PlaySound(SOUND_CALL_EXCELLENT, FLIGHT_SOUND_CHANNEL_CALLS, SOUND_FULL_SCALE);
                    QueueOnlookerSounds(CHARACTER_SOUND_KIND_EXCELLENT);
                    f_PendingResultSound = RESULT_SOUND_GOOD_SHOT;
                }
                else if (IsApproachShot()) {
                    if (f_LandingCupDistance <= NEAR_LANDING_DISTANCE) {
                        StartBanner(BANNER_NICE_APPROACH);
                        PlaySound(
                            SOUND_CALL_NICE_APPROACH,
                            FLIGHT_SOUND_CHANNEL_CALLS,
                            SOUND_FULL_SCALE
                        );
                        f_PendingResultSound = RESULT_SOUND_GOOD_SHOT;
                    }
                    else {
                        StartBanner(BANNER_NICE_ON);
                        PlaySound(SOUND_CALL_NICE_ON, FLIGHT_SOUND_CHANNEL_CALLS, SOUND_FULL_SCALE);
                    }
                }
                else {
                    StartBanner(BANNER_NICE_ON);
                    PlaySound(SOUND_CALL_NICE_ON, FLIGHT_SOUND_CHANNEL_CALLS, SOUND_FULL_SCALE);
                }
            }
            else if (g_HoleSetup.par + SCORE_TO_PAR_BIRDIE ==
                    g_Players[g_CurrentShot.playerIndex].strokeCount &&
                IsApproachShot() &&
                f_LandingCupDistance <= NEAR_LANDING_DISTANCE) {
                StartBanner(BANNER_NICE_APPROACH);
                PlaySound(SOUND_CALL_NICE_APPROACH, FLIGHT_SOUND_CHANNEL_CALLS, SOUND_FULL_SCALE);
            }
        }
    }

    if (g_SimulatedShot.endsInCup) {
        if (g_BallFlightStep >= g_SimulatedShot.stepCount - FLAG_SHOT_CALL_STEP_COUNT &&
            f_EndReaction == END_REACTION_NONE &&
            g_SimulatedShot.flagHitStep != NO_STEP) {
            StartBanner(BANNER_FLAG_SHOT);
            PlaySound(SOUND_CALL_FLAG_SHOT, FLIGHT_SOUND_CHANNEL_CALLS, SOUND_FULL_SCALE);
            QueueOnlookerSounds(CHARACTER_SOUND_KIND_EXCELLENT);
            CountNotableShot(g_CurrentShot.playerIndex, NOTABLE_SHOT_FLAG_SHOT);
            f_EndReaction = END_REACTION_ONLOOKERS;
            f_PendingResultSound = RESULT_SOUND_SUPERB_SHOT;
        }

        if (g_BallFlightStep >= g_SimulatedShot.stepCount - CHIP_IN_CALL_STEP_COUNT &&
            f_EndReaction == END_REACTION_NONE &&
            IsChipIn()) {
            StartBanner(BANNER_CHIP_IN);
            PlaySound(SOUND_CALL_CHIP_IN, FLIGHT_SOUND_CHANNEL_CALLS, SOUND_FULL_SCALE);
            QueueOnlookerSounds(CHARACTER_SOUND_KIND_GREAT);
            f_EndReaction = END_REACTION_ONLOOKERS;
        }
    }

    isPlayed = &f_IsLandingSoundPlayed;
    if (!*isPlayed) {
        if (g_BallFlightStep == g_SimulatedShot.stepCount &&
            !shownRingCount &&
            g_SimulatedShot.endsInCup) {
            *isPlayed = TRUE;
            if (IsGameFlagSet(GAME_FLAG_FORCE_FLOURISHES)) {
                if (g_IsCupViewShown == TRUE &&
                    IsScoreToParWithin(scoreToPar, SCORE_TO_PAR_EAGLE, SCORE_TO_PAR_BOGEY) &&
                    !IsPinInCup()) {
                    StartMovie(MOVIE_GOOMBA);
                }
            }
            else if (IsScoreToParWithin(scoreToPar, SCORE_TO_PAR_EAGLE, SCORE_TO_PAR_PAR) &&
                !IsPinInCup() &&
                g_IsCupViewShown == TRUE) {
                if (g_SimulatedShot.shot.shotMode == SHOT_MODE_NORMAL) {
                    StartMovie(MOVIE_MONTY_MOLE);
                }
                else if (g_SimulatedShot.shot.shotMode == SHOT_MODE_POWER) {
                    StartMovie(MOVIE_GOOMBA);
                }
            }

            if (g_SimulatedShot.startsOnGreen) {
                if (f_StartCupDistance > LONG_PUTT_SOUND_DISTANCE_MIN ||
                    IsGameFlagSet(GAME_FLAG_FORCE_FLOURISHES)) {
                    PlaySound(SOUND_LONG_PUTT_HOLED, FLIGHT_SOUND_CHANNEL_CALLS, SOUND_FULL_SCALE);
                    f_EndReaction = END_REACTION_LONG_PUTT;
                }
            }

            if (!g_SimulatedShot.startsOnGreen) {
                if (f_StartCupDistance > LONG_LANDING_DISTANCE_MIN) {
                    CountNotableShot(g_CurrentShot.playerIndex, NOTABLE_SHOT_LONG_CHIP_IN);
                    f_PendingResultSound = RESULT_SOUND_SUPERB_SHOT;
                }
                else {
                    CountNotableShot(g_CurrentShot.playerIndex, NOTABLE_SHOT_CHIP_IN);
                    f_PendingResultSound = RESULT_SOUND_GOOD_SHOT;
                }
            }

            if (g_Players[g_CurrentShot.playerIndex].strokeCount == 1) {
                PlaySound(SOUND_EAGLE_CUP_IN, FLIGHT_SOUND_CHANNEL_CUP_IN, SOUND_FULL_SCALE);
            }
            else if (g_HoleSetup.par + SCORE_TO_PAR_ALBATROSS ==
                g_Players[g_CurrentShot.playerIndex].strokeCount) {
                PlaySound(SOUND_EAGLE_CUP_IN, FLIGHT_SOUND_CHANNEL_CUP_IN, ALBATROSS_CUP_IN_VOLUME);
            }
            else if (g_HoleSetup.par + SCORE_TO_PAR_EAGLE ==
                g_Players[g_CurrentShot.playerIndex].strokeCount) {
                PlaySound(SOUND_EAGLE_CUP_IN, FLIGHT_SOUND_CHANNEL_CUP_IN, EAGLE_CUP_IN_VOLUME);
            }
            else if (g_HoleSetup.par + SCORE_TO_PAR_BIRDIE ==
                g_Players[g_CurrentShot.playerIndex].strokeCount) {
                PlaySound(SOUND_BIRDIE_CUP_IN, FLIGHT_SOUND_CHANNEL_CUP_IN, BIRDIE_CUP_IN_VOLUME);
            }
        }

        isStillPlayed = &f_IsLandingSoundPlayed;
        if (!*isStillPlayed &&
            g_BallFlightStep > EARLY_STEP_COUNT &&
            HasHitGroundBefore(step, EARLY_STEP_COUNT) &&
            IsShotBeyond(FULL_SHOT_FRACTION)) {
            surfaceTraits = GetSurfaceTraits(g_TurnBallSimulation.surface);
            if (IsFairwayKind(surfaceTraits->kind) &&
                !g_GameState.isMiniGolf &&
                f_StartCupDistance > f_LandingCupDistance &&
                IsAlbatrossOrBetter(g_Players[g_CurrentShot.playerIndex].strokeCount)) {
                *isStillPlayed = TRUE;
                PlaySound(
                    SOUND_UNDER_PAR_CHANCE,
                    FLIGHT_SOUND_CHANNEL_LANDING,
                    FAIRWAY_SOUND_VOLUME
                );
            }
        }
    }

    if (g_BallFlightStep > EARLY_STEP_COUNT &&
        g_BallFlightStep == g_SimulatedShot.stepCount - LANDING_SOUND_STEP_COUNT) {
        if (IsBunkerKind(f_EndSurfaceTraits->kind)) {
            f_IsLandingSoundPlayed = TRUE;
            PlaySound(SOUND_TROUBLE_LANDING, FLIGHT_SOUND_CHANNEL_LANDING, SOUND_FULL_SCALE);
            QueueOnlookerSounds(CHARACTER_SOUND_KIND_BUNKER);
        }
        else if (f_EndSurfaceTraits->kind == SURFACE_KIND_DEEP_ROUGH) {
            PlaySound(SOUND_DEEP_ROUGH_LANDING, FLIGHT_SOUND_CHANNEL_LANDING, SOUND_FULL_SCALE);
            QueueOnlookerSounds(CHARACTER_SOUND_KIND_BUNKER);
        }
        else if (f_EndSurfaceTraits->kind == SURFACE_KIND_WASTE_AREA) {
            PlaySound(SOUND_WASTE_AREA_LANDING, FLIGHT_SOUND_CHANNEL_LANDING, SOUND_FULL_SCALE);
        }
        else if (IsLostBallKind(f_EndSurfaceTraits->kind)) {
            f_IsLandingSoundPlayed = TRUE;
            PlaySound(SOUND_TROUBLE_LANDING, FLIGHT_SOUND_CHANNEL_LANDING, SOUND_FULL_SCALE);
            QueueOnlookerSounds(CHARACTER_SOUND_KIND_TROUBLE);
        }
    }
}

void EaseLookAtFlyingBall(void) {
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 targetCoordinate;

    if (!IsBallLanding()) {
        targetCoordinate = f_FlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
        offsetX = targetCoordinate - f_LookAtPoint[AXIS_X];
        targetCoordinate = f_FlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        offsetY = targetCoordinate - f_LookAtPoint[AXIS_Y];
        targetCoordinate = f_FlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        offsetZ = targetCoordinate - f_LookAtPoint[AXIS_Z];

        f_LookAtPoint[AXIS_X] += offsetX * LOOK_AT_EASE_FRACTION;
        f_LookAtPoint[AXIS_Y] += offsetY * LOOK_AT_HEIGHT_EASE_FRACTION;
        f_LookAtPoint[AXIS_Z] += offsetZ * LOOK_AT_EASE_FRACTION;
    }
}

void EaseLookAtBelowBall(f32 drop) {
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 targetCoordinate;

    if (!IsBallLanding()) {
        targetCoordinate = f_FlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
        offsetX = targetCoordinate - f_LookAtPoint[AXIS_X];
        targetCoordinate = (f_FlightBallPosition[AXIS_Y] - drop) * COURSE_TO_WORLD_SCALE;
        offsetY = targetCoordinate - f_LookAtPoint[AXIS_Y];
        targetCoordinate = f_FlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        offsetZ = targetCoordinate - f_LookAtPoint[AXIS_Z];

        f_LookAtPoint[AXIS_X] += offsetX * LOOK_AT_EASE_FRACTION;
        f_LookAtPoint[AXIS_Y] += offsetY * LOOK_AT_HEIGHT_EASE_FRACTION;
        f_LookAtPoint[AXIS_Z] += offsetZ * LOOK_AT_EASE_FRACTION;
    }
}

void EaseLookAtPoint(Radians yaw UNUSED) {
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 targetCoordinate;

    targetCoordinate = f_FlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
    offsetX = targetCoordinate - f_LookAtPoint[AXIS_X];
    targetCoordinate = (f_FlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE) - LOOK_AT_DROP;
    offsetY = targetCoordinate - f_LookAtPoint[AXIS_Y];
    targetCoordinate = f_FlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    offsetZ = targetCoordinate - f_LookAtPoint[AXIS_Z];

    f_LookAtPoint[AXIS_X] += offsetX * LOOK_AT_EASE_FRACTION;
    f_LookAtPoint[AXIS_Y] += offsetY * LOOK_AT_HEIGHT_EASE_FRACTION;
    f_LookAtPoint[AXIS_Z] += offsetZ * LOOK_AT_EASE_FRACTION;
}

void SnapLookAtPoint(void) {
    f_LookAtPoint[AXIS_X] = f_FlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
    f_LookAtPoint[AXIS_Y] = f_FlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    f_LookAtPoint[AXIS_Z] = f_FlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
}

#define EaseCameraCoordinate(axis, offset) \
    do { \
        if (fabsf(offset) > CAMERA_EASE_DEAD_ZONE) { \
            f_CameraPosition[axis] += (offset) * CAMERA_EASE_FRACTION; \
        } \
    } \
    while (FALSE)

void EaseCameraPosition(Vec3 target) {
    f32 offset;

    offset = target[AXIS_X] - f_CameraPosition[AXIS_X];
    EaseCameraCoordinate(AXIS_X, offset);
    offset = target[AXIS_Y] - f_CameraPosition[AXIS_Y];
    EaseCameraCoordinate(AXIS_Y, offset);
    offset = target[AXIS_Z] - f_CameraPosition[AXIS_Z];
    EaseCameraCoordinate(AXIS_Z, offset);
}

#define PUTT_HEIGHT_SPREAD_FRACTION 0.2
#define DISTANCE_SPREAD_FRACTION 0.2f
#define DEFAULT_HEIGHT_SPREAD_FRACTION 0.1f
#define SIMPLE_CAMERA_HEIGHT_SPREAD_FRACTION 0.2f
#define PUTT_HEIGHT_MIN 0x6600
#define RANDOM_DISTANCE_STEP_COUNT 1536
#define PUTT_DISTANCE_STEP 15
#define PUTT_DISTANCE_MIN 0xB400
#define SHORT_SHOT_DISTANCE 76800.0f
#define GREEN_DISTANCE_STEP 50
#define GREEN_DISTANCE_MIN 0x12C00
#define CAMERA_DISTANCE_MIN 76800.0f
#define CAMERA_DISTANCE_MAX 460800.0f
#define DEFAULT_HEIGHT_MIN 46080.0f
#define DEFAULT_HEIGHT_MAX 92160.0f
#define SIMPLE_CAMERA_HEIGHT_MIN 92160.0f
#define SIMPLE_CAMERA_HEIGHT_MAX 153600.0f
#define RISE_ABOVE_SHOT_END_MIN 0x7800

void PlaceCameraAroundShotEnd(void) {
    CoursePoint point;
    f32 height;
    f64 yawScale;
    f32 distance;
    Radians yaw;

    if (g_SimulatedShot.startsOnGreen) {
        height = RandomBelow(f_ShotLength * PUTT_HEIGHT_SPREAD_FRACTION) + PUTT_HEIGHT_MIN;
        distance =
            ((guRandom() % RANDOM_DISTANCE_STEP_COUNT) * PUTT_DISTANCE_STEP) + PUTT_DISTANCE_MIN;
        RandomBelow(RANDOM_YAW_MASK);
        yawScale = RANDOM_YAW_TO_RADIANS / 4;
        yaw = f_ShotFlightYaw - ((f32)(rand() & RANDOM_YAW_MASK) * yawScale);
    }
    else if (!IsSimpleCameraChosen()) {
        if (f_ShotLength <= SHORT_SHOT_LENGTH_MAX) {
            distance = SHORT_SHOT_DISTANCE;
            height = 0.0f;
            yaw = f_ShotFlightYaw + (M_PI / 8) -
                ((f32)(rand() & RANDOM_YAW_MASK) * (RANDOM_YAW_TO_RADIANS / 8));
        }
        else {
            if (g_SimulatedShot.endsOnGreen) {
                distance = ((guRandom() % RANDOM_DISTANCE_STEP_COUNT) * GREEN_DISTANCE_STEP) +
                    GREEN_DISTANCE_MIN;
            }
            else if (f_ShotLength * DISTANCE_SPREAD_FRACTION != 0.0f) {
                distance =
                    ((guRandom() % f_ShotLength) * DISTANCE_SPREAD_FRACTION) + CAMERA_DISTANCE_MIN;
            }
            else {
                distance = CAMERA_DISTANCE_MIN;
            }

            if (f_ShotLength * DEFAULT_HEIGHT_SPREAD_FRACTION != 0.0f) {
                height = ((guRandom() % f_ShotLength) * DEFAULT_HEIGHT_SPREAD_FRACTION) +
                    DEFAULT_HEIGHT_MIN;
            }
            else {
                height = DEFAULT_HEIGHT_MIN;
            }

            if (height > DEFAULT_HEIGHT_MAX) {
                height = DEFAULT_HEIGHT_MAX;
            }

            if (distance > CAMERA_DISTANCE_MAX) {
                distance = CAMERA_DISTANCE_MAX;
            }

            yaw = f_ShotFlightYaw + M_PI + (M_PI * 5 / 8) +
                ((f32)(rand() & RANDOM_YAW_MASK) * (RANDOM_YAW_TO_RADIANS / 2 * 1.225f));
        }
    }
    else {
        if (f_ShotLength * DISTANCE_SPREAD_FRACTION != 0.0f) {
            distance =
                ((guRandom() % f_ShotLength) * DISTANCE_SPREAD_FRACTION) + CAMERA_DISTANCE_MIN;
        }
        else {
            distance = CAMERA_DISTANCE_MIN;
        }

        if (f_ShotLength * SIMPLE_CAMERA_HEIGHT_SPREAD_FRACTION != 0.0f) {
            height = ((guRandom() % f_ShotLength) * SIMPLE_CAMERA_HEIGHT_SPREAD_FRACTION) +
                SIMPLE_CAMERA_HEIGHT_MIN;
        }
        else {
            height = SIMPLE_CAMERA_HEIGHT_MIN;
        }

        if (height > SIMPLE_CAMERA_HEIGHT_MAX) {
            height = SIMPLE_CAMERA_HEIGHT_MAX;
        }

        if (distance > CAMERA_DISTANCE_MAX) {
            distance = CAMERA_DISTANCE_MAX;
        }

        yaw = f_ShotFlightYaw + M_PI + (M_PI / 2) +
            ((f32)(rand() & RANDOM_YAW_MASK) * (RANDOM_YAW_TO_RADIANS / 2));
    }

    point[AXIS_X] = (-sinf(yaw) * distance) + f_ShotEndPosition[AXIS_X];
    point[AXIS_Z] = (cosf(yaw) * distance) + f_ShotEndPosition[AXIS_Z];
    point[AXIS_Y] = GetGroundHeight(point[AXIS_X], point[AXIS_Z]);
    if (point[AXIS_Y] > f_ShotEndPosition[AXIS_Y] - RISE_ABOVE_SHOT_END_MIN) {
        point[AXIS_Y] = f_ShotEndPosition[AXIS_Y] - RISE_ABOVE_SHOT_END_MIN;
    }

    point[AXIS_Y] = point[AXIS_Y] - height;
    f_CameraPosition[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
    f_CameraPosition[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    f_CameraPosition[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    SnapLookAtPoint();
}

void StartHoleCamerasAndSong(void) {
    SetMenuBackdrop(MENU_BACKDROP_NONE);
    g_RoundHud.isHoleIntroTextShown = TRUE;
    g_RoundHud.scorecardPage = NO_SCORECARD_PAGE;
    g_RoundHud.isHoleInfoShown = TRUE;
    g_RoundHud.isUnreadItemShown = TRUE;
    g_InsetView.animation = INSET_VIEW_STILL;
    g_InsetView.step = 0;
    g_HoleFlyoverFrame = 0;
    g_Cameras[MAIN_CAMERA].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
    g_Cameras[INSET_CAMERA].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
    if (g_GameState.replayKind != REPLAY_KIND_NONE) {
        PlaySong(SONG_ROUND_REPLAY, SONG_FLAG_FOLLOWS_MUSIC_OPTION | SONG_FLAG_UNREAD);
        return;
    }

    switch (g_GameState.gameMode) {
        case GAME_MODE_TRAINING:
            PlaySong(SONG_ROUND_TRAINING, SONG_FLAG_UNREAD);
            break;
        case GAME_MODE_MATCH_GAME:
        case GAME_MODE_GET_CHARACTER:
            if (GetMatchStanding(0, 1) != MATCH_STANDING_OPEN) {
                PlaySong(SONG_ROUND_FINAL, SONG_FLAG_UNREAD);
            }
            else {
                PlaySong(SONG_ROUND_MATCH, SONG_FLAG_UNREAD);
            }

            break;
        case GAME_MODE_SKINS_MATCH:
            if (GetLastHoleIndex() == g_GameState.holeIndex) {
                PlaySong(SONG_ROUND_FINAL, SONG_FLAG_UNREAD);
            }
            else {
                PlaySong(SONG_ROUND_SKINS_MATCH, SONG_FLAG_UNREAD);
            }

            break;
        case GAME_MODE_TOURNAMENT:
            PlaySong(SONG_ROUND_TOURNAMENT, SONG_FLAG_UNREAD);
            break;
        case GAME_MODE_SPEED_GOLF:
            PlaySong(SONG_ROUND_SPEED_GOLF, SONG_FLAG_UNREAD);
            break;
        case GAME_MODE_RING_SHOT:
            PlaySong(SONG_ROUND_RING_SHOT, SONG_FLAG_UNREAD);
            g_RoundHud.isHoleInfoShown = FALSE;
            g_RoundHud.isUnreadItemShown = FALSE;
            break;
        case GAME_MODE_CLUB_SLOTS:
            if (GetLastHoleIndex() == g_GameState.holeIndex) {
                PlaySong(SONG_ROUND_FINAL, SONG_FLAG_UNREAD);
            }
            else {
                PlaySong(SONG_ROUND_CLUB_SLOTS, SONG_FLAG_UNREAD);
            }

            break;
        case GAME_MODE_STROKE:
        default:
            if (g_GameState.holeIndex == GetLastHoleIndex() && g_GameState.playerCount != 1) {
                PlaySong(SONG_ROUND_FINAL, SONG_FLAG_UNREAD);
            }
            else if (g_GameState.isMiniGolf) {
                PlaySong(SONG_ROUND_MINI_GOLF, SONG_FLAG_UNREAD);
            }
            else {
                PlaySong(SONG_ROUND_COURSE, SONG_FLAG_UNREAD);
            }

            break;
    }
}

void StartHoleFlyover(void) {
    g_HoleFlyoverFrame = 0;
    g_RoundHud.isHoleIntroTextShown = FALSE;
    g_InsetView.animation = INSET_VIEW_STILL;
    g_InsetView.step = 0;
    g_Cameras[INSET_CAMERA].view.drawFlags = 0;
    g_Cameras[MAIN_CAMERA].mode = CAMERA_MODE_BEHIND_GOLFER;
    g_Cameras[INSET_CAMERA].mode = CAMERA_MODE_BEHIND_GOLFER;
    g_Cameras[MAIN_CAMERA].view.fieldOfView = FLYOVER_FIELD_OF_VIEW;
    SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, PROMPT_TEXT_SLOT, 0, 0, 0, 0, 0);
    SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, CHOICE_TEXT_SLOT, 0, 0, 0, 0, 0);
    SetInsetViewRectangle(
        FLYOVER_INSET_CENTER_X,
        FLYOVER_INSET_CENTER_Y,
        FLYOVER_INSET_SIZE,
        FLYOVER_INSET_SIZE
    );
    StepHoleFlyover();
}

void ClearHoleIntroText(void) {
    g_RoundHud.isHoleIntroTextShown = FALSE;
    SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, PROMPT_TEXT_SLOT, 0, 0, 0, 0, 0);
    SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, CHOICE_TEXT_SLOT, 0, 0, 0, 0, 0);
}

void StopHoleFlyover(void) {
    g_HoleFlyoverFrame = HOLE_FLYOVER_DONE;
    g_InsetView.animation = INSET_VIEW_STILL;
    g_InsetView.step = 0;
    g_Cameras[INSET_CAMERA].view.drawFlags = 0;
    if (g_GameState.gameMode != GAME_MODE_SPEED_GOLF && !g_GameState.isMiniGolf) {
        if (IsGameFlagSet(GAME_FLAG_FORCE_FLOURISHES) ||
            g_HoleSetup.conditions.windSpeed / WIND_SPEED_UNIT >= STRONG_WIND_SPEED) {
            ShowWindFlyGuy(TRUE);
        }
    }
}

Boolean StepHoleFlyover(void) {
    Vec3 offset;
    f32 halfTurns;
    f32 widening;
    f32 approach;
    Radians yaw;
    f32 distance;
    f32 height;
    s32 holeRadius;

    halfTurns = g_HoleFlyoverFrame * (1.0f / FLYOVER_HALF_TURN_FRAME_COUNT);
    if (g_HoleFlyoverFrame > FLYOVER_HALF_TURN_FRAME_COUNT) {
        widening = 0.0f;
        approach = 1.0f - sinf(DegreesToRadians(g_HoleFlyoverFrame));
    }
    else {
        approach = (1.0f - sinf(DegreesToRadians(g_HoleFlyoverFrame))) * 0.5f;
        widening = (FLYOVER_HALF_TURN_FRAME_COUNT - g_HoleFlyoverFrame) *
            (1.0f / FLYOVER_HALF_TURN_FRAME_COUNT);
    }

    if (halfTurns > FLYOVER_HALF_TURNS_MAX) {
        halfTurns = FLYOVER_HALF_TURNS_MAX;
    }

    if (!g_GameState.isMiniGolf) {
        if (g_HoleFlyoverFrame == FLYOVER_INSET_OPEN_FRAME) {
            g_InsetView.animation = INSET_VIEW_OPENING;
        }

        if (g_HoleFlyoverFrame == FLYOVER_INSET_CLOSE_FRAME) {
            g_InsetView.animation = INSET_VIEW_CLOSING;
        }
    }

    offset[AXIS_X] = g_HoleSetup.teePosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X];
    offset[AXIS_Y] = g_HoleSetup.teePosition[AXIS_Y] - g_HoleSetup.cupPosition[AXIS_Y];
    offset[AXIS_Z] = g_HoleSetup.teePosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z];
    g_InsetView.ignoredSourceCode = FLYOVER_INSET_SOURCE_CODE;
    g_InsetView.ignoredSizeCode = FLYOVER_INSET_SIZE_CODE;
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] =
        (g_HoleSetup.cupPosition[AXIS_X] + (offset[AXIS_X] * approach)) * COURSE_TO_WORLD_SCALE;
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] =
        (g_HoleSetup.cupPosition[AXIS_Y] + (offset[AXIS_Y] * approach)) * COURSE_TO_WORLD_SCALE;
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] =
        (g_HoleSetup.cupPosition[AXIS_Z] + (offset[AXIS_Z] * approach)) * COURSE_TO_WORLD_SCALE;
    yaw = atan2(offset[AXIS_X], offset[AXIS_Z]) + (halfTurns * M_PI);
    holeRadius = GetHoleRadius(GetCourseRecordIndex(), GetHoleRecordIndex());

    offset[AXIS_X] = sinf(yaw);
    distance = ((holeRadius - (approach * FLYOVER_RADIUS_APPROACH)) + (holeRadius * widening)) *
        FLYOVER_DISTANCE_SCALE;
    offset[AXIS_X] *= distance;
    offset[AXIS_Z] = cosf(yaw) * distance;
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] =
        g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] + (offset[AXIS_X] * COURSE_TO_WORLD_SCALE);
    yaw += yaw;
    height = ((holeRadius / 2) + (approach * FLYOVER_HEIGHT_APPROACH)) * FLYOVER_HEIGHT_SCALE;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] =
        g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] = g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] - height;
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] =
        g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] + (offset[AXIS_Z] * COURSE_TO_WORLD_SCALE);
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] =
        (g_HoleSetup.cupPosition[AXIS_Y] - FLYOVER_INSET_LOOK_AT_DROP) * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] =
        g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    offset[AXIS_X] = sinf(yaw) * FLYOVER_INSET_DISTANCE;
    offset[AXIS_Z] = cosf(yaw) * FLYOVER_INSET_DISTANCE;
    g_Cameras[INSET_CAMERA].view.eye[AXIS_X] =
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] + (offset[AXIS_X] * COURSE_TO_WORLD_SCALE);
    g_Cameras[INSET_CAMERA].view.eye[AXIS_Y] =
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] - FLYOVER_INSET_EYE_RISE;
    g_Cameras[INSET_CAMERA].view.eye[AXIS_Z] =
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] + (offset[AXIS_Z] * COURSE_TO_WORLD_SCALE);
    return ++g_HoleFlyoverFrame == HOLE_FLYOVER_FRAME_COUNT;
}

void AimCameraYaw(void) {
    f32 offsetX;
    f32 offsetZ;

    offsetX = f_LookAtPoint[AXIS_X] - f_CameraPosition[AXIS_X];
    offsetZ = f_LookAtPoint[AXIS_Z] - f_CameraPosition[AXIS_Z];
    if ((s32)offsetX != 0 || (s32)offsetZ != 0) {
        f_CameraYaw = M_PI - atan2f(offsetX, offsetZ);
    }
}

#define CUP_FRAMING_PITCH (-0.14f)
#define CUP_FRAMING_RISE 0xC00
#define CUP_FRAMING_RADIUS 3.0f
#define CUP_FRAMING_DEPTH_OFFSET (-1.5f)
#define PATH_FRAMING_PITCH (-0.7853982f)
#define PATH_FRAMING_RADIUS (-15.0f)
#define PATH_FRAMING_DEPTH_OFFSET (-150.0f)
#define LAUNCH_VIEW_STEP 11
#define LAUNCH_VIEW_RISE_MIN 0x6600
#define CHASE_DISTANCE 15.0f
#define CHASE_FIELD_OF_VIEW 80.0f
#define FOLLOW_STEP_COUNT 10
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
#define BEHIND_LOOK_AT_RISE 4.5f
#define BEHIND_LOOK_AT_EASED_RISE 4608.0f
#define APEX_VIEW_NEAR_HALF_DISTANCE 1536.0f
#define APEX_VIEW_FAR_DISTANCE 30720.0f
#define APEX_VIEW_RISE_STEP 0x600
#define APEX_VIEW_FAR_RISE_STEP_COUNT 10
#define APEX_VIEW_RISE_MIN 0x6600
#define SIDE_VIEW_LENGTH_FRACTION 0.1f
#define SIDE_VIEW_AHEAD_FRACTION 0.5f
#define SIDE_VIEW_SPREAD_STEP 0x600
#define SIDE_VIEW_SPREAD_STEP_COUNT 200
#define SIDE_VIEW_DISTANCE_MAX 768000.0f
#define SIDE_VIEW_DISTANCE_MIN 76800.0f
#define SIDE_VIEW_RISE_FRACTION 0.1f
#define SIDE_VIEW_RISE 153600.0f
#define SIDE_VIEW_DROP_BELOW_APEX_MAX 0x3C00
#define SIDE_VIEW_RISE_ABOVE_END_MIN 0x6600
#define SIDE_VIEW_RISE_MIN 0x7800
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
#define FACING_CUP_VIEW_DISTANCE 76800.0f
#define FACING_CUP_VIEW_RISE 0x5A00
#define FACING_CUP_EASE_DEAD_ZONE 0.5f
#define FACING_CUP_EASE_FRACTION 0.1f
#define BILLBOARD_HIT_VIEW_DISTANCE 460800.0f
#define BILLBOARD_HIT_VIEW_RISE 0x4B000
#define CIRCLE_START_FLIGHT_COUNT 2
#define CIRCLE_ANGLE_STEP DegreesToRadians(2)
#define CIRCLE_DISTANCE 30720.0f
#define CIRCLE_RISE 23040.0f
#define CIRCLE_LOOK_AT_RISE 15360.0f
#define CIRCLE_RISE_ABOVE_START_MIN 23040.0f
#define BESIDE_GOLFER_FIELD_OF_VIEW 100.0f
#define BESIDE_GOLFER_FIRST_STEP 7
#define BESIDE_GOLFER_STEP_LIMIT 15
#define BESIDE_GOLFER_STEP 1536.0f
#define BESIDE_GOLFER_RISE 1536.0f
#define BESIDE_GOLFER_RISE_MIN 7680.0f
#define BESIDE_GOLFER_CLEARANCE 22.5f
#define BESIDE_GOLFER_LOOK_AT_RISE 12288.0f
#define BESIDE_GOLFER_FLIGHT_FIELD_OF_VIEW 80.0f
#define EYE_DEPTH_BELOW_GROUND_MAX 0x21C0000

void PlaceFlightCamera(s32 step) {
    Vec3 point;
    f32 extraRise;
    Vec3 cupSphere;
    Vec3 pathSphere;
    Vec3 position;
    Radians yaw;
    f32 distance;
    Radians sideYaw;
    Radians circleYaw;
    f32 height;
    f32 easeFraction;
    f32 sideDistance;
    f32 riseDistance;
    Radians angle;
    s32 index;
    s32 lowestHeight;

    switch (f_FlightCameraKind) {
        case FLIGHT_CAMERA_FRAMING_CUP:
            ClearFramedSpheres();
            SetFramingAngles(CUP_FRAMING_PITCH, -f_StartYawFromCup - (M_PI / 6));
            cupSphere[AXIS_X] = (f32)g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
            cupSphere[AXIS_Z] = (f32)g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            cupSphere[AXIS_Y] =
                (f32)(g_HoleSetup.cupPosition[AXIS_Y] - CUP_FRAMING_RISE) * COURSE_TO_WORLD_SCALE;
            AddFramedSphere(cupSphere, CUP_FRAMING_RADIUS);
            cupSphere[AXIS_X] = (f32)g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
            cupSphere[AXIS_Z] = (f32)g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            cupSphere[AXIS_Y] =
                (f32)(g_HoleSetup.cupPosition[AXIS_Y] - CUP_FRAMING_RISE) * COURSE_TO_WORLD_SCALE;
            AddFramedSphere(cupSphere, CUP_FRAMING_RADIUS);
            FrameSpheres(CUP_FRAMING_DEPTH_OFFSET);
            GetFramedView(f_CameraPosition, f_LookAtPoint);
            break;
        case FLIGHT_CAMERA_FRAMING_PATH:
            ClearFramedSpheres();
            SetFramingAngles(PATH_FRAMING_PITCH, 0.0f);
            pathSphere[AXIS_X] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.positionStepMin[AXIS_X]][AXIS_X] *
                COURSE_TO_WORLD_SCALE;
            pathSphere[AXIS_Y] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.positionStepMin[AXIS_X]][AXIS_Y] *
                COURSE_TO_WORLD_SCALE;
            pathSphere[AXIS_Z] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.positionStepMin[AXIS_X]][AXIS_Z] *
                COURSE_TO_WORLD_SCALE;
            AddFramedSphere(pathSphere, PATH_FRAMING_RADIUS);
            pathSphere[AXIS_X] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.positionStepMin[AXIS_Z]][AXIS_X] *
                COURSE_TO_WORLD_SCALE;
            pathSphere[AXIS_Y] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.positionStepMin[AXIS_Z]][AXIS_Y] *
                COURSE_TO_WORLD_SCALE;
            pathSphere[AXIS_Z] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.positionStepMin[AXIS_Z]][AXIS_Z] *
                COURSE_TO_WORLD_SCALE;
            AddFramedSphere(pathSphere, PATH_FRAMING_RADIUS);
            pathSphere[AXIS_X] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.positionStepMax[AXIS_X]][AXIS_X] *
                COURSE_TO_WORLD_SCALE;
            pathSphere[AXIS_Y] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.positionStepMax[AXIS_X]][AXIS_Y] *
                COURSE_TO_WORLD_SCALE;
            pathSphere[AXIS_Z] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.positionStepMax[AXIS_X]][AXIS_Z] *
                COURSE_TO_WORLD_SCALE;
            AddFramedSphere(pathSphere, PATH_FRAMING_RADIUS);
            pathSphere[AXIS_X] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.positionStepMax[AXIS_Z]][AXIS_X] *
                COURSE_TO_WORLD_SCALE;
            pathSphere[AXIS_Y] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.positionStepMax[AXIS_Z]][AXIS_Y] *
                COURSE_TO_WORLD_SCALE;
            pathSphere[AXIS_Z] =
                (f32)g_SimulatedShot.path[g_SimulatedShot.positionStepMax[AXIS_Z]][AXIS_Z] *
                COURSE_TO_WORLD_SCALE;
            AddFramedSphere(pathSphere, PATH_FRAMING_RADIUS);
            FrameSpheres(PATH_FRAMING_DEPTH_OFFSET);
            GetFramedView(f_CameraPosition, f_LookAtPoint);
            break;
        case FLIGHT_CAMERA_NEAR_LAUNCH:
            if (f_IsCameraKindNew == TRUE) {
                f_IsCameraKindNew = FALSE;
                f_CameraPosition[AXIS_X] = g_SimulatedShot.path[LAUNCH_VIEW_STEP][AXIS_X];
                f_CameraPosition[AXIS_Y] = g_SimulatedShot.path[LAUNCH_VIEW_STEP][AXIS_Y];
                f_CameraPosition[AXIS_Z] = g_SimulatedShot.path[LAUNCH_VIEW_STEP][AXIS_Z];
                height = GetGroundHeight(f_CameraPosition[AXIS_X], f_CameraPosition[AXIS_Z]) -
                    LAUNCH_VIEW_RISE_MIN;
                if (height < f_CameraPosition[AXIS_Y]) {
                    f_CameraPosition[AXIS_Y] = height;
                }

                f_CameraPosition[AXIS_X] *= COURSE_TO_WORLD_SCALE;
                f_CameraPosition[AXIS_Y] *= COURSE_TO_WORLD_SCALE;
                f_CameraPosition[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
                f_LookAtPoint[AXIS_X] = (f32)f_FlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
                f_LookAtPoint[AXIS_Y] = (f32)f_FlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                f_LookAtPoint[AXIS_Z] = (f32)f_FlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            }

            break;
        case FLIGHT_CAMERA_CHASING_BALL: {
            f32 scale;

            position[AXIS_X] =
                g_SimulatedShot.path[step - 1][AXIS_X] - (f32)f_FlightBallPosition[AXIS_X];
            position[AXIS_Y] = 0.0f;
            position[AXIS_Z] =
                g_SimulatedShot.path[step - 1][AXIS_Z] - (f32)f_FlightBallPosition[AXIS_Z];
            scale = CHASE_DISTANCE /
                sqrtf(
                    (position[AXIS_X] * position[AXIS_X]) + (position[AXIS_Y] * position[AXIS_Y]) +
                    (position[AXIS_Z] * position[AXIS_Z])
                );
            position[AXIS_X] *= scale;
            position[AXIS_Y] *= scale;
            position[AXIS_Z] *= scale;
            f_LookAtPoint[AXIS_X] =
                (f_FlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE) - position[AXIS_X];
            f_LookAtPoint[AXIS_Y] = f_FlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            f_LookAtPoint[AXIS_Z] =
                (f_FlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE) - position[AXIS_Z];
            f_CameraPosition[AXIS_X] =
                (f_FlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE) + position[AXIS_X];
            f_CameraPosition[AXIS_Y] = f_FlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            f_CameraPosition[AXIS_Z] =
                (f_FlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE) + position[AXIS_Z];
            g_Cameras[MAIN_CAMERA].view.fieldOfView = CHASE_FIELD_OF_VIEW;
            break;
        }
        case FLIGHT_CAMERA_FOLLOWING_PATH:
            f_CameraPosition[AXIS_X] =
                (f32)g_SimulatedShot.path[step - FOLLOW_STEP_COUNT][AXIS_X] * COURSE_TO_WORLD_SCALE;
            f_CameraPosition[AXIS_Y] =
                (f32)(g_SimulatedShot.path[step - FOLLOW_STEP_COUNT][AXIS_Y] - FOLLOW_RISE) *
                COURSE_TO_WORLD_SCALE;
            f_CameraPosition[AXIS_Z] =
                (f32)g_SimulatedShot.path[step - FOLLOW_STEP_COUNT][AXIS_Z] * COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.fieldOfView = FOLLOW_FIELD_OF_VIEW;
            EaseLookAtFlyingBall();
            break;
        case FLIGHT_CAMERA_UNUSED_D:
        case FLIGHT_CAMERA_UNUSED_E:
            EaseLookAtFlyingBall();
            break;
        case FLIGHT_CAMERA_BESIDE_TEE:
            if (f_IsCameraKindNew == TRUE) {
                f_IsCameraKindNew = FALSE;
                yaw = ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) - TEE_VIEW_YAW_OFFSET;
                point[AXIS_X] = (cosf(yaw) * TEE_VIEW_DISTANCE) + f_ShotStartPosition[AXIS_X];
                point[AXIS_Z] = (sinf(yaw) * TEE_VIEW_DISTANCE) + f_ShotStartPosition[AXIS_Z];
                point[AXIS_Y] = GetGroundHeight(point[AXIS_X], point[AXIS_Z]) - TEE_VIEW_RISE;
                point[AXIS_X] *= COURSE_TO_WORLD_SCALE;
                point[AXIS_Y] *= COURSE_TO_WORLD_SCALE;
                point[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
                f_CameraPosition[AXIS_X] = point[AXIS_X];
                f_CameraPosition[AXIS_Y] = point[AXIS_Y];
                f_CameraPosition[AXIS_Z] = point[AXIS_Z];
                f_LookAtPoint[AXIS_X] = (f32)f_ShotStartPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
                f_LookAtPoint[AXIS_Y] =
                    (f32)(f_ShotStartPosition[AXIS_Y] - TEE_LOOK_AT_RISE) * COURSE_TO_WORLD_SCALE;
                f_LookAtPoint[AXIS_Z] = (f32)f_ShotStartPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                AimCameraYaw();
            }

            g_Cameras[MAIN_CAMERA].view.fieldOfView = TEE_VIEW_FIELD_OF_VIEW;
            break;
        case FLIGHT_CAMERA_BEHIND_START:
            if (f_IsCameraKindNew == TRUE) {
                Radians sideYaw;
                f32 behindDistance;
                f32 sideDistance;

                f_IsCameraKindNew = FALSE;
                f_IsBehindViewFar ^= TRUE;
                yaw = f_ShotFlightYaw + M_PI;
                g_Cameras[MAIN_CAMERA].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
                if (f_IsBehindViewFar == FALSE) {
                    sideYaw = f_ShotFlightYaw + (M_PI / 4) +
                        ((f32)(rand() & RANDOM_YAW_MASK) * (RANDOM_YAW_TO_RADIANS / 4));
                    behindDistance = (f_ShotLength * NEAR_BEHIND_VIEW_LENGTH_FRACTION) +
                        NEAR_BEHIND_VIEW_DISTANCE -
                        RandomBelow(f_ShotLength * BEHIND_VIEW_SPREAD_FRACTION);
                }
                else {
                    sideYaw = f_ShotFlightYaw - (M_PI / 4) -
                        ((f32)(rand() & RANDOM_YAW_MASK) * (RANDOM_YAW_TO_RADIANS / 4));
                    behindDistance = (f_ShotLength * FAR_BEHIND_VIEW_LENGTH_FRACTION) +
                        FAR_BEHIND_VIEW_DISTANCE -
                        RandomBelow(f_ShotLength * BEHIND_VIEW_SPREAD_FRACTION);
                }

                sideDistance = RandomBelow(f_ShotLength * BEHIND_VIEW_SIDE_SPREAD_FRACTION) +
                    BEHIND_VIEW_SIDE_DISTANCE;
                point[AXIS_X] = f_ShotStartPosition[AXIS_X] + (sinf(yaw) * behindDistance) +
                    (sinf(sideYaw) * sideDistance);
                point[AXIS_Z] = f_ShotStartPosition[AXIS_Z] - (cosf(yaw) * behindDistance) -
                    (cosf(sideYaw) * sideDistance);
                point[AXIS_Y] = GetGroundHeight(point[AXIS_X], point[AXIS_Z]) - BEHIND_VIEW_RISE;
                f_CameraPosition[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
                f_CameraPosition[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                f_CameraPosition[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                SnapLookAtPoint();
                f_LookAtPoint[AXIS_Y] -= BEHIND_LOOK_AT_RISE;
            }

            EaseLookAtBelowBall(BEHIND_LOOK_AT_EASED_RISE);
            break;
        case FLIGHT_CAMERA_SIDE_OF_FLIGHT:
            if (f_IsCameraKindNew == TRUE) {
                f_IsCameraKindNew = FALSE;
                f_SideViewCount++;
                if (f_SideViewCount & 1) {
                    sideYaw = f_ShotFlightYaw - (M_PI / 2) +
                        ((f32)(rand() & RANDOM_YAW_MASK) * RANDOM_YAW_TO_RADIANS);
                    f_IsApexViewFar ^= TRUE;
                    if (f_IsApexViewFar == FALSE) {
                        point[AXIS_X] =
                            g_SimulatedShot.path[g_SimulatedShot.positionStepMin[AXIS_Y]][AXIS_X] +
                            (2.0f * (sinf(sideYaw) * APEX_VIEW_NEAR_HALF_DISTANCE));
                        point[AXIS_Y] =
                            g_SimulatedShot.path[g_SimulatedShot.positionStepMin[AXIS_Y]][AXIS_Y] +
                            (RandomBelow(1) * APEX_VIEW_RISE_STEP);
                        point[AXIS_Z] =
                            g_SimulatedShot.path[g_SimulatedShot.positionStepMin[AXIS_Y]][AXIS_Z] -
                            (2.0f * (cosf(sideYaw) * APEX_VIEW_NEAR_HALF_DISTANCE));
                    }
                    else {
                        point[AXIS_X] =
                            g_SimulatedShot.path[g_SimulatedShot.positionStepMin[AXIS_Y]][AXIS_X] +
                            (sinf(sideYaw) * APEX_VIEW_FAR_DISTANCE);
                        point[AXIS_Y] =
                            g_SimulatedShot.path[g_SimulatedShot.positionStepMin[AXIS_Y]][AXIS_Y] +
                            (RandomBelow(APEX_VIEW_FAR_RISE_STEP_COUNT) * APEX_VIEW_RISE_STEP);
                        point[AXIS_Z] =
                            g_SimulatedShot.path[g_SimulatedShot.positionStepMin[AXIS_Y]][AXIS_Z] -
                            (cosf(sideYaw) * APEX_VIEW_FAR_DISTANCE);
                    }

                    height = GetGroundHeight(point[AXIS_X], point[AXIS_Z]) - APEX_VIEW_RISE_MIN;
                    if (height < point[AXIS_Y]) {
                        point[AXIS_Y] = height;
                    }
                }
                else {
                    f32 sideDistance;

                    sideDistance = f_ShotLength * SIDE_VIEW_LENGTH_FRACTION;
                    distance = f_ShotLength * SIDE_VIEW_AHEAD_FRACTION;

                    sideYaw = f_ShotFlightYaw + (M_PI / 2) +
                        ((f32)(rand() & RANDOM_YAW_MASK) * RANDOM_YAW_TO_RADIANS);
                    g_Cameras[MAIN_CAMERA].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
                    yaw = f_ShotFlightYaw + M_PI;
                    sideDistance -=
                        RandomBelow(SIDE_VIEW_SPREAD_STEP_COUNT) * SIDE_VIEW_SPREAD_STEP;
                    if (sideDistance > SIDE_VIEW_DISTANCE_MAX) {
                        sideDistance = SIDE_VIEW_DISTANCE_MAX;
                    }

                    if (sideDistance < SIDE_VIEW_DISTANCE_MIN) {
                        sideDistance = SIDE_VIEW_DISTANCE_MIN;
                    }

                    point[AXIS_X] = f_ShotStartPosition[AXIS_X] + (sinf(yaw) * distance) +
                        (sinf(sideYaw) * sideDistance);
                    point[AXIS_Z] = f_ShotStartPosition[AXIS_Z] - (cosf(yaw) * distance) -
                        (cosf(sideYaw) * sideDistance);
                    extraRise = f_ShotLength * SIDE_VIEW_RISE_FRACTION;
                    point[AXIS_Y] = GetGroundHeight(point[AXIS_X], point[AXIS_Z]);
                    if (!g_SimulatedShot.startsOnGreen) {
                        point[AXIS_Y] -= extraRise + SIDE_VIEW_RISE;
                    }

                    height = g_SimulatedShot.path[g_SimulatedShot.positionStepMin[AXIS_Y]][AXIS_Y] +
                        SIDE_VIEW_DROP_BELOW_APEX_MAX;
                    if (height < point[AXIS_Y]) {
                        point[AXIS_Y] = height;
                    }

                    height = f_ShotEndPosition[AXIS_Y] - SIDE_VIEW_RISE_ABOVE_END_MIN;
                    if (height < point[AXIS_Y]) {
                        point[AXIS_Y] = height;
                    }

                    height = GetGroundHeight(point[AXIS_X], point[AXIS_Z]) - SIDE_VIEW_RISE_MIN;
                    if (height < point[AXIS_Y]) {
                        point[AXIS_Y] = height;
                    }
                }

                f_CameraPosition[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
                f_CameraPosition[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                f_CameraPosition[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                SnapLookAtPoint();
                AimCameraYaw();
            }

            EaseLookAtFlyingBall();
            break;
        case FLIGHT_CAMERA_AROUND_END:
            if (f_IsCameraKindNew == TRUE) {
                f_IsCameraKindNew = FALSE;
                PlaceCameraAroundShotEnd();
                AimCameraYaw();
            }

            g_Cameras[MAIN_CAMERA].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
            EaseLookAtFlyingBall();
            break;
        case FLIGHT_CAMERA_TRAILING_BALL:
            if (f_IsCameraKindNew == TRUE) {
                SnapLookAtPoint();
                f_IsCameraKindNew = FALSE;
                AimCameraYaw();
                f_IsTrailViewClose ^= TRUE;
            }

            g_Cameras[MAIN_CAMERA].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
            if (f_IsTrailViewClose == FALSE) {
                if (f_ShotLength <= SHORT_SHOT_LENGTH_MAX) {
                    riseDistance = TRAIL_SHORT_SHOT_RISE;
                    sideDistance = TRAIL_SHORT_SHOT_DISTANCE;
                }
                else {
                    riseDistance = TRAIL_LONG_SHOT_RISE;
                    sideDistance = TRAIL_LONG_SHOT_DISTANCE;
                }
            }
            else {
                riseDistance = TRAIL_CLOSE_RISE;
                sideDistance = TRAIL_CLOSE_DISTANCE;
            }

            rand();

            easeFraction = step;
            easeFraction = TRAIL_EASE_START_PERCENT - (TRAIL_EASE_PERCENT_STEP * easeFraction);
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

            angle = f_TrailViewAngle;
            if (angle < M_PI / 6) {
                angle = M_PI / 6;
            }

            if (angle > FULL_TURN_RADIANS - (M_PI / 6)) {
                angle = FULL_TURN_RADIANS - (M_PI / 6);
            }

            if (f_ShotLength <= SHORT_SHOT_LENGTH_MAX) {
                angle = ((angle - M_PI) * TRAIL_ANGLE_SCALE) - M_PI;
            }
            else {
                angle = (angle - M_PI) * TRAIL_ANGLE_SCALE;
            }

            position[AXIS_X] =
                ((-sinf(f_ShotFlightYaw + angle) * sideDistance) + f_FlightBallPosition[AXIS_X]) *
                COURSE_TO_WORLD_SCALE;
            position[AXIS_Z] =
                ((cosf(f_ShotFlightYaw + angle) * sideDistance) + f_FlightBallPosition[AXIS_Z]) *
                COURSE_TO_WORLD_SCALE;
            position[AXIS_Y] =
                (f_FlightBallPosition[AXIS_Y] - riseDistance) * COURSE_TO_WORLD_SCALE;
            f_CameraPosition[AXIS_X] +=
                (position[AXIS_X] - f_CameraPosition[AXIS_X]) * easeFraction;
            f_CameraPosition[AXIS_Y] +=
                (position[AXIS_Y] - f_CameraPosition[AXIS_Y]) * easeFraction;
            f_CameraPosition[AXIS_Z] +=
                (position[AXIS_Z] - f_CameraPosition[AXIS_Z]) * easeFraction;
            EaseLookAtFlyingBall();
            break;
        case FLIGHT_CAMERA_FACING_CUP:
            yaw = f_EndYawFromCup;
            g_Cameras[MAIN_CAMERA].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
            point[AXIS_X] = (-sinf(yaw) * FACING_CUP_VIEW_DISTANCE) + f_FlightBallPosition[AXIS_X];
            point[AXIS_Z] = (cosf(yaw) * FACING_CUP_VIEW_DISTANCE) + f_FlightBallPosition[AXIS_Z];
            point[AXIS_Y] = GetGroundHeight(point[AXIS_X], point[AXIS_Z]) - FACING_CUP_VIEW_RISE;
            height = f_ShotEndPosition[AXIS_Y] - FACING_CUP_VIEW_RISE;
            if (height < point[AXIS_Y]) {
                point[AXIS_Y] = height;
            }

            point[AXIS_X] *= COURSE_TO_WORLD_SCALE;
            point[AXIS_Y] *= COURSE_TO_WORLD_SCALE;
            point[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
            if (fabsf(point[AXIS_X] - f_CameraPosition[AXIS_X]) > FACING_CUP_EASE_DEAD_ZONE) {
                f_CameraPosition[AXIS_X] +=
                    (point[AXIS_X] - f_CameraPosition[AXIS_X]) * FACING_CUP_EASE_FRACTION;
            }

            if (fabsf(point[AXIS_Y] - f_CameraPosition[AXIS_Y]) > FACING_CUP_EASE_DEAD_ZONE) {
                f_CameraPosition[AXIS_Y] +=
                    (point[AXIS_Y] - f_CameraPosition[AXIS_Y]) * FACING_CUP_EASE_FRACTION;
            }

            if (fabsf(point[AXIS_Z] - f_CameraPosition[AXIS_Z]) > FACING_CUP_EASE_DEAD_ZONE) {
                f_CameraPosition[AXIS_Z] +=
                    (point[AXIS_Z] - f_CameraPosition[AXIS_Z]) * FACING_CUP_EASE_FRACTION;
            }

            EaseLookAtPoint(f_EndYawFromCup);
            break;
        case FLIGHT_CAMERA_BILLBOARD_HIT:
            if (f_BillboardHitStep == 0) {
                f_BillboardHitStep = 1;
            }

            f_BillboardHitYaw = -atan2(
                (f32)(g_SimulatedShot.path[f_BillboardHitStep][AXIS_X] -
                    g_SimulatedShot.path[f_BillboardHitStep - 1][AXIS_X]),
                (f32)(g_SimulatedShot.path[f_BillboardHitStep][AXIS_Z] -
                    g_SimulatedShot.path[f_BillboardHitStep - 1][AXIS_Z])
            );
            point[AXIS_X] = (-sinf(f_BillboardHitYaw) * BILLBOARD_HIT_VIEW_DISTANCE) +
                g_SimulatedShot.path[f_BillboardHitStep][AXIS_X];
            point[AXIS_Z] = (cosf(f_BillboardHitYaw) * BILLBOARD_HIT_VIEW_DISTANCE) +
                g_SimulatedShot.path[f_BillboardHitStep][AXIS_Z];
            point[AXIS_Y] = GetGroundHeight(point[AXIS_X], point[AXIS_Z]) - BILLBOARD_HIT_VIEW_RISE;
            f_CameraPosition[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
            f_CameraPosition[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            f_CameraPosition[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            EaseLookAtFlyingBall();
            g_Cameras[MAIN_CAMERA].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
            break;
        case FLIGHT_CAMERA_AIM_VIEW:
            if (f_IsCameraKindNew == TRUE) {
                f_IsCameraKindNew = FALSE;
                PlaceAimCamera();
                f_CameraPosition[AXIS_X] = g_Cameras[MAIN_CAMERA].view.eye[AXIS_X];
                f_CameraPosition[AXIS_Y] = g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y];
                f_CameraPosition[AXIS_Z] = g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z];
                f_LookAtPoint[AXIS_X] = g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X];
                f_LookAtPoint[AXIS_Y] = g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y];
                f_LookAtPoint[AXIS_Z] = g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z];
                AimCameraYaw();
            }

            break;
        case FLIGHT_CAMERA_CIRCLING_BALL:
            if (f_IsCameraKindNew == TRUE) {
                f_IsCameraKindNew = FALSE;
                if (f_EarlierFlightCount < CIRCLE_START_FLIGHT_COUNT) {
                    f_CircleAngle = 0.0f;
                }
                else if (g_Players[g_CurrentShot.playerIndex].isLefty == TRUE) {
                    f_CircleAngle = M_PI / 2;
                }
                else {
                    f_CircleAngle = -M_PI / 2;
                }
            }

            if (g_Players[g_CurrentShot.playerIndex].isLefty == TRUE) {
                f_CircleAngle += CIRCLE_ANGLE_STEP;
                if (f_CircleAngle > M_PI) {
                    f_CircleAngle = M_PI;
                }
            }
            else {
                f_CircleAngle -= CIRCLE_ANGLE_STEP;
                if (f_CircleAngle < -M_PI) {
                    f_CircleAngle = -M_PI;
                }
            }

            circleYaw = ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) + f_CircleAngle;
            f_CameraPosition[AXIS_X] =
                (cosf(circleYaw) * CIRCLE_DISTANCE) + g_CurrentShot.ballPosition[AXIS_X];
            f_CameraPosition[AXIS_Z] =
                (sinf(circleYaw) * CIRCLE_DISTANCE) + g_CurrentShot.ballPosition[AXIS_Z];
            f_CameraPosition[AXIS_Y] =
                GetGroundHeight(f_CameraPosition[AXIS_X], f_CameraPosition[AXIS_Z]) - CIRCLE_RISE;
            f_CameraPosition[AXIS_X] *= COURSE_TO_WORLD_SCALE;
            f_CameraPosition[AXIS_Y] *= COURSE_TO_WORLD_SCALE;
            f_CameraPosition[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
            f_LookAtPoint[AXIS_X] = (f32)g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
            f_LookAtPoint[AXIS_Y] =
                ((f32)g_CurrentShot.ballPosition[AXIS_Y] - CIRCLE_LOOK_AT_RISE) *
                COURSE_TO_WORLD_SCALE;
            f_LookAtPoint[AXIS_Z] = (f32)g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
            break;
        case FLIGHT_CAMERA_BESIDE_GOLFER:
            if (f_IsCameraKindNew == TRUE) {
                Radians besideYaw;
                f32 besideDistance;

                g_Cameras[MAIN_CAMERA].view.fieldOfView = BESIDE_GOLFER_FIELD_OF_VIEW;
                GetCharacterPointPosition(0, 1, position);
                f_IsCameraKindNew = FALSE;
                if (g_Players[g_CurrentShot.playerIndex].isLefty == TRUE) {
                    besideYaw = ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) + (M_PI / 2);
                }
                else {
                    besideYaw = ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) - (M_PI / 2);
                }

                for (index = BESIDE_GOLFER_FIRST_STEP; index < BESIDE_GOLFER_STEP_LIMIT; index++) {
                    f_CameraPosition[AXIS_X] =
                        (cosf(besideYaw) * (besideDistance = index * BESIDE_GOLFER_STEP)) +
                        g_CurrentShot.ballPosition[AXIS_X];
                    f_CameraPosition[AXIS_Z] =
                        (sinf(besideYaw) * besideDistance) + g_CurrentShot.ballPosition[AXIS_Z];
                    f_CameraPosition[AXIS_Y] =
                        GetGroundHeight(f_CameraPosition[AXIS_X], f_CameraPosition[AXIS_Z]) -
                        BESIDE_GOLFER_RISE;
                    if (g_CurrentShot.ballPosition[AXIS_Y] - BESIDE_GOLFER_RISE_MIN <
                        f_CameraPosition[AXIS_Y]) {
                        f_CameraPosition[AXIS_Y] =
                            g_CurrentShot.ballPosition[AXIS_Y] - BESIDE_GOLFER_RISE_MIN;
                    }

                    f_CameraPosition[AXIS_X] *= COURSE_TO_WORLD_SCALE;
                    f_CameraPosition[AXIS_Y] *= COURSE_TO_WORLD_SCALE;
                    f_CameraPosition[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
                    if (Length2D(
                            f_CameraPosition[AXIS_X] - position[AXIS_X],
                            f_CameraPosition[AXIS_Z] - position[AXIS_Z]
                        ) > BESIDE_GOLFER_CLEARANCE) {
                        break;
                    }
                }

                f_LookAtPoint[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
                f_LookAtPoint[AXIS_Y] =
                    g_CurrentShot.ballPosition[AXIS_Y] - BESIDE_GOLFER_LOOK_AT_RISE;
                f_LookAtPoint[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
                f_LookAtPoint[AXIS_X] *= COURSE_TO_WORLD_SCALE;
                f_LookAtPoint[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
                f_LookAtPoint[AXIS_Y] *= COURSE_TO_WORLD_SCALE;
                AimCameraYaw();
            }

            g_Cameras[MAIN_CAMERA].view.fieldOfView = BESIDE_GOLFER_FLIGHT_FIELD_OF_VIEW;
            break;
    }

    position[AXIS_X] = f_CameraPosition[AXIS_X] * WORLD_TO_COURSE_SCALE;
    position[AXIS_Y] = f_CameraPosition[AXIS_Y] * WORLD_TO_COURSE_SCALE;
    position[AXIS_Z] = f_CameraPosition[AXIS_Z] * WORLD_TO_COURSE_SCALE;
    lowestHeight = GetGroundHeight(position[AXIS_X], position[AXIS_Z]) + EYE_DEPTH_BELOW_GROUND_MAX;
    if (f_FlightCameraKind == FLIGHT_CAMERA_CIRCLING_BALL) {
        if (lowestHeight < position[AXIS_Y]) {
            position[AXIS_Y] = lowestHeight;
        }

        height = f_ShotStartPosition[AXIS_Y] - CIRCLE_RISE_ABOVE_START_MIN;
        if (height < position[AXIS_Y]) {
            position[AXIS_Y] = height;
        }
    }
    else if (f_FlightCameraKind != FLIGHT_CAMERA_BESIDE_GOLFER &&
        f_FlightCameraKind != FLIGHT_CAMERA_FRAMING_CUP) {
        height = lowestHeight;
        if (height < position[AXIS_Y]) {
            position[AXIS_Y] = height;
        }
    }

    if (g_GameState.isMiniGolf) {
        if (f_FlightCameraKind != FLIGHT_CAMERA_FRAMING_PATH) {
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
    }
    else {
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

    f_CameraPosition[AXIS_X] = position[AXIS_X] * COURSE_TO_WORLD_SCALE;
    f_CameraPosition[AXIS_Y] = position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    f_CameraPosition[AXIS_Z] = position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
}

#define LONG_SHOT_LENGTH_MIN 0x1C2000
#define MID_SHOT_LENGTH_MIN 0x4B001
#define NEAR_CUP_DISTANCE_MAX 0x1E00
#define CUP_LANDING_DISTANCE_MIN 0x1201
#define CUP_LANDING_DISTANCE_MAX 0x4AFFF
#define IsCupLandingDistance(distance) \
    ((distance) >= CUP_LANDING_DISTANCE_MIN && (distance) <= CUP_LANDING_DISTANCE_MAX)
#define IsEndingNearCup() \
    (g_SimulatedShot.endsInCup || f_LandingCupDistance < NEAR_CUP_DISTANCE_MAX)
#define DEBUG_CAMERA_CONTROLLER 1

#define IsTroubleSurface(traits) \
    (IsBunkerKind((traits)->kind) || \
        (traits)->kind == SURFACE_KIND_WATER_HAZARD || \
        (traits)->kind == SURFACE_KIND_OUT_OF_BOUNDS)

void ChooseFlightCameraKind(void) {
    HudAnimation animationId;
    FlightCameraKind kind;
    s32 pick;

    animationId = HUD_ANIMATION_NONE;
    f_HasFlightPhaseChanged = FALSE;
    kind = FLIGHT_CAMERA_UNCHANGED;
    if (g_SimulatedShot.startsOnGreen) {
        switch (f_FlightPhase) {
            case FLIGHT_PHASE_LAUNCH:
                if (!(f_FlightCameraFlags & FLIGHT_CAMERA_FLAG_CUP_VIEW_ALLOWED) &&
                    RandomBelow(2) == 0 &&
                    !IsPinInCup() &&
                    IsCupLandingDistance(f_StartCupDistance) &&
                    IsEndingNearCup()) {
                    kind = FLIGHT_CAMERA_FRAMING_CUP;
                }
                else if (g_GameState.isMiniGolf) {
                    pick = RandomBelow(10);
                    switch (pick) {
                        case 0:
                        case 1:
                            kind = FLIGHT_CAMERA_AIM_VIEW;
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
                }
                else {
                    pick = RandomBelow(6);
                    if (pick < 3) {
                        kind = FLIGHT_CAMERA_AIM_VIEW;
                    }
                    else if (pick == 3) {
                        if ((f_ComingFlightEvents & FLIGHT_EVENT_CUP_PASS) ||
                            g_SimulatedShot.stepCount > 120) {
                            kind = FLIGHT_CAMERA_BEHIND_START;
                        }
                        else {
                            kind = FLIGHT_CAMERA_BESIDE_GOLFER;
                        }
                    }
                    else if (pick == 4) {
                        kind = FLIGHT_CAMERA_BEHIND_START;
                    }
                    else {
                        kind = FLIGHT_CAMERA_AROUND_END;
                    }
                }

                break;
            case FLIGHT_PHASE_CLIMB:
                if (f_FlightCameraKind != FLIGHT_CAMERA_FRAMING_PATH &&
                    f_FlightCameraKind != FLIGHT_CAMERA_FRAMING_CUP &&
                    f_FlightCameraKind != FLIGHT_CAMERA_BEHIND_START) {
                    kind = FLIGHT_CAMERA_AROUND_END;
                }

                break;
            case FLIGHT_PHASE_CONTACT:
                if (f_FlightCameraKind != FLIGHT_CAMERA_FRAMING_PATH &&
                    f_FlightCameraKind != FLIGHT_CAMERA_FRAMING_CUP) {
                    kind = FLIGHT_CAMERA_AROUND_END;
                }

                break;
            case FLIGHT_PHASE_END:
                if (!g_SimulatedShot.endsInCup && RandomBelow(2) == 0) {
                    kind = FLIGHT_CAMERA_FACING_CUP;
                }

                break;
        }
    }
    else if (!IsSimpleCameraChosen()) {
        switch (f_FlightPhase) {
            case FLIGHT_PHASE_LAUNCH:
                pick = NextCameraPick(1);
                if (f_BillboardHitStep != NO_STEP) {
                    kind = FLIGHT_CAMERA_AIM_VIEW;
                }
                else if (pick < 4) {
                    kind = FLIGHT_CAMERA_AIM_VIEW;
                }
                else if (pick == 4) {
                    if (f_ShotLength > SHORT_SHOT_LENGTH_MAX) {
                        kind = FLIGHT_CAMERA_AIM_VIEW;
                    }
                    else {
                        kind = FLIGHT_CAMERA_CIRCLING_BALL;
                    }
                }
                else if (pick == 5) {
                    kind = FLIGHT_CAMERA_AIM_VIEW;
                    if (f_IsBallGroundLevel == TRUE && g_SimulatedShot.stepCount >= 61) {
                        kind = FLIGHT_CAMERA_BESIDE_GOLFER;
                    }
                }
                else if (pick == 6) {
                    if (f_ShotLength <= SHORT_SHOT_LENGTH_MAX) {
                        kind = FLIGHT_CAMERA_AROUND_END;
                    }
                    else {
                        kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                    }
                }
                else if (pick == 7) {
                    if (f_ShotLength <= SHORT_SHOT_LENGTH_MAX) {
                        kind = FLIGHT_CAMERA_AROUND_END;
                    }
                    else {
                        kind = FLIGHT_CAMERA_TRAILING_BALL;
                    }
                }
                else if (pick == 8) {
                    kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                }
                else if (pick == 9) {
                    kind = FLIGHT_CAMERA_AIM_VIEW;
                    if (f_ShotLength >= MID_SHOT_LENGTH_MIN && g_SimulatedShot.stepCount >= 16) {
                        kind = FLIGHT_CAMERA_NEAR_LAUNCH;
                    }
                }

                break;
            case FLIGHT_PHASE_EARLY_CLIMB:
                if (f_FlightCameraKind == FLIGHT_CAMERA_NEAR_LAUNCH) {
                    animationId = HUD_ANIMATION_TILES;
                    kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                }
                else if (f_FlightCameraKind == FLIGHT_CAMERA_AIM_VIEW) {
                    if (RandomBelow(3) == 0) {
                        kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                    }

                    if (f_ShotLength >= LONG_SHOT_LENGTH_MIN && RandomBelow(4) == 0) {
                        kind = FLIGHT_CAMERA_FOLLOWING_PATH;
                    }
                }
                else if (f_FlightCameraKind == FLIGHT_CAMERA_BESIDE_GOLFER) {
                    if (g_SimulatedShot.stepCount < 120) {
                        kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                    }
                    else if (g_SimulatedShot.usesPutter) {
                        kind = FLIGHT_CAMERA_BEHIND_START;
                    }
                    else if (f_ShotLength < LONG_SHOT_LENGTH_MIN) {
                        kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                    }
                    else if (RandomBelow(4) == 0) {
                        kind = FLIGHT_CAMERA_FOLLOWING_PATH;
                    }
                    else {
                        kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                    }
                }

                break;
            case FLIGHT_PHASE_CLIMB:
                if (f_FlightCameraKind == FLIGHT_CAMERA_BESIDE_GOLFER) {
                    if (g_SimulatedShot.stepCount < 120) {
                        kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                    }
                    else if (g_SimulatedShot.usesPutter) {
                        kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
                    }
                }
                else if (f_FlightCameraKind != FLIGHT_CAMERA_TRAILING_BALL) {
                    if (f_FlightCameraKind != FLIGHT_CAMERA_FOLLOWING_PATH) {
                        if (f_FlightCameraKind != FLIGHT_CAMERA_SIDE_OF_FLIGHT) {
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
            case FLIGHT_PHASE_FALL:
                if (f_CameraChangeHoldCount != 0) {
                    f_HasFlightPhaseChanged = TRUE;
                }
                else if (f_FlightCameraKind == FLIGHT_CAMERA_TRAILING_BALL) {
                }
                else if (f_FlightCameraKind == FLIGHT_CAMERA_FOLLOWING_PATH) {
                }
                else {
                    if (f_LandingCupDistance <= SHORT_SHOT_LENGTH_MAX) {
                        if (RandomBelow(2) == 0) {
                            kind = FLIGHT_CAMERA_AROUND_END;
                        }
                    }
                    else if (RandomBelow(8) == 0) {
                        kind = FLIGHT_CAMERA_AROUND_END;
                        animationId = HUD_ANIMATION_TILES;
                    }
                    else if (RandomBelow(8) < 4) {
                        kind = FLIGHT_CAMERA_AROUND_END;
                    }
                }

                break;
            case FLIGHT_PHASE_NEARING_CONTACT:
                if (IsTroubleSurface(f_EndSurfaceTraits)) {
                    kind = FLIGHT_CAMERA_AROUND_END;
                }
                else if (f_FlightCameraKind == FLIGHT_CAMERA_FOLLOWING_PATH) {
                    kind = FLIGHT_CAMERA_AROUND_END;
                }
                else if (f_FlightCameraKind == FLIGHT_CAMERA_SIDE_OF_FLIGHT ||
                    f_FlightCameraKind == FLIGHT_CAMERA_TRAILING_BALL) {
                    kind = FLIGHT_CAMERA_AROUND_END;
                }
                else {
                    kind = FLIGHT_CAMERA_AROUND_END;
                }
                break;
            case FLIGHT_PHASE_CONTACT:
                if (f_CameraChangeHoldCount == 0) {
                    kind = FLIGHT_CAMERA_AROUND_END;
                }
                else {
                    f_HasFlightPhaseChanged = TRUE;
                }

                break;
            case FLIGHT_PHASE_END:
                if (IsTroubleSurface(f_EndSurfaceTraits) || g_SimulatedShot.endsInCup) {
                }
                else {
                    if (f_CameraChangeHoldCount != 0 || g_SimulatedShot.stepCount < 21) {
                        f_HasFlightPhaseChanged = TRUE;
                    }
                    else if (f_LandingCupDistance <= SHORT_SHOT_LENGTH_MAX ||
                        f_FlightCameraKind == FLIGHT_CAMERA_AROUND_END) {
                        kind = FLIGHT_CAMERA_FACING_CUP;
                    }
                }

                break;
        }
    }
    else {
        switch (f_FlightPhase) {
            case FLIGHT_PHASE_LAUNCH:
                pick = RandomBelow(9);
                switch (pick) {
                    case 0:
                    case 1:
                    case 2:
                        kind = FLIGHT_CAMERA_AIM_VIEW;
                        break;
                    case 3:
                    case 4:
                    case 5:
                        if (f_ShotLength <= SHORT_SHOT_LENGTH_MAX) {
                            kind = FLIGHT_CAMERA_AROUND_END;
                        }
                        else {
                            kind = FLIGHT_CAMERA_AIM_VIEW;
                        }

                        break;
                    case 6:
                        kind = FLIGHT_CAMERA_AIM_VIEW;
                        if (f_IsBallGroundLevel == TRUE && g_SimulatedShot.stepCount >= 61) {
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

                f_HasFlightPhaseChanged = TRUE;
                break;
            case FLIGHT_PHASE_NEARING_CONTACT:
                kind = FLIGHT_CAMERA_AROUND_END;
                f_HasFlightPhaseChanged = TRUE;
                break;
            case FLIGHT_PHASE_CONTACT:
                break;
        }
    }

    if (f_FlightCameraKind < kind) {
        f_IsCameraKindNew = TRUE;
        StartHudAnimation(HUD_ANIMATION_NONE);
        if (animationId == HUD_ANIMATION_NONE) {
            if (f_FlightPhase >= FLIGHT_PHASE_FALL &&
                !g_SimulatedShot.startsOnGreen &&
                f_ShotLength > SHORT_SHOT_LENGTH_MAX) {
                if (kind == FLIGHT_CAMERA_AROUND_END) {
                    if (RandomBelow(16) == 0 &&
                        f_FlightCameraKind == FLIGHT_CAMERA_SIDE_OF_FLIGHT) {
                        StartHudAnimation(HUD_ANIMATION_TILES);
                    }
                    else if (RandomBelow(8) == 0) {
                        StartHudAnimation(HUD_ANIMATION_FADE_OUT);
                    }
                    else if (RandomBelow(3) == 0) {
                        StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
                    }
                }
            }
        }
        else if (f_ShotLength > SHORT_SHOT_LENGTH_MAX) {
            StartHudAnimation(animationId);
        }

        f_FlightCameraKind = kind;
        f_CameraChangeHoldCount = CAMERA_CHANGE_HOLD_COUNT;
    }

    if (g_IsDebugEnabled) {
        if (g_ControllerStates[DEBUG_CAMERA_CONTROLLER].buttons & B_BUTTON) {
            f_FlightCameraKind = FLIGHT_CAMERA_BESIDE_GOLFER;
        }

        if (g_ControllerStates[DEBUG_CAMERA_CONTROLLER].buttons & A_BUTTON) {
            f_FlightCameraKind = FLIGHT_CAMERA_CIRCLING_BALL;
        }
    }
}

#define CUP_PASS_LOOKAHEAD_STEP_COUNT 30
#define PATH_FLAG_LOOKAHEAD_STEP_COUNT 60
#define BILLBOARD_HIT_VIEW_STEP_MIN 60
#define BILLBOARD_HIT_VIEW_STEP_COUNT 10
#define END_VIEW_DISTANCE_MIN 0x25800
#define CAMERA_CHOICE_STEP_COUNT_MIN 40
#define LAUNCH_END_STEP 10
#define EARLY_CLIMB_END_STEP 15
#define GREEN_CLIMB_END_STEP 40
#define CONTACT_LOOKAHEAD_STEP_COUNT 40
#define END_PHASE_STEP_COUNT 10

#define IsPastBillboardHitView(step) \
    (f_BillboardHitStep != NO_STEP && f_BillboardHitStep + BILLBOARD_HIT_VIEW_STEP_COUNT < (step))
#define IsFarFromShotEnd() \
    ((s32)IntLength3D( \
         f_FlightBallPosition[AXIS_X] - f_ShotEndPosition[AXIS_X], \
         f_FlightBallPosition[AXIS_Y] - f_ShotEndPosition[AXIS_Y], \
         f_FlightBallPosition[AXIS_Z] - f_ShotEndPosition[AXIS_Z] \
     ) > END_VIEW_DISTANCE_MIN)
#define HasTimeForNewCamera() \
    (g_BallFlightStep + CAMERA_CHOICE_STEP_COUNT_MIN < g_SimulatedShot.stepCount || \
        f_FlightPhase == FLIGHT_PHASE_END)
#define IsFlightCameraFree() (!g_IsCupViewShown && f_ComingFlightEvents == 0)
#define VERY_LONG_SHOT_LENGTH_MIN 0x2A3000
#define BALL_SOUND_SHOT_LENGTH_MIN 0x25800
#define BALL_SOUND_VOLUME_LENGTH 0x7800
#define BALL_SOUND_VOLUME_MIN 10
#define BALL_SOUND_VOLUME_MAX 64
#define BALL_SOUND_EARLY_STEP 6
#define BALL_SOUND_LATE_STEP 10
#define BALL_SOUND_CHANNEL 8
#define BALL_SOUND_REVERB 0x1E
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
#define LOOK_AT_NUDGE 0.1f

Boolean UpdateFlightCamera(Boolean isAdvancing, s32 buttons) {
    Vec3 point;
    s32 step;
    SoundVolume volume;
    s32 aheadStep;
    s32 waitFrame;
    s32 *flightStep;

    if (!isAdvancing) {
        g_RoundHud.isShotLengthShown = FALSE;
    }
    else {
        g_RoundHud.isShotLengthShown = TRUE;
    }

    if (buttons & START_BUTTON) {
        g_RoundHud.isClubPanelShown = FALSE;
        g_RoundHud.isWindIndicatorShown = FALSE;
        g_RoundHud.isHoleInfoShown = FALSE;
        g_RoundHud.isLieIconShown = FALSE;
        g_RoundHud.isClubHeadIconShown = FALSE;
        g_RoundHud.isUnreadItemShown = FALSE;
        g_RoundHud.isProgressShown = FALSE;
        g_RoundHud.isShotLengthShown = FALSE;
    }
    else {
        g_RoundHud.isClubPanelShown = f_IsFlightHudShown;
        g_RoundHud.isWindIndicatorShown = f_IsFlightHudShown;
        g_RoundHud.isHoleInfoShown = f_IsFlightHudShown;
        g_RoundHud.isLieIconShown = f_IsFlightHudShown;
        g_RoundHud.isClubHeadIconShown = f_IsFlightHudShown;
        g_RoundHud.isUnreadItemShown = f_IsFlightHudShown;
        g_RoundHud.isProgressShown = f_IsFlightHudShown;
    }

    if (g_BallFlightStep >= PATH_STEP_COUNT) {
        step = PATH_STEP_COUNT - 1;
    }
    else {
        step = g_BallFlightStep;
    }

    if (step >= g_SimulatedShot.stepCount - 1) {
        step = g_SimulatedShot.stepCount - 1;
    }

    if (!isAdvancing) {
        f_FlightBallPosition[AXIS_X] = f_ShotStartPosition[AXIS_X];
        f_FlightBallPosition[AXIS_Y] = f_ShotStartPosition[AXIS_Y];
        f_FlightBallPosition[AXIS_Z] = f_ShotStartPosition[AXIS_Z];
    }
    else if (g_BallFlightStep == g_SimulatedShot.stepCount) {
        f_FlightBallPosition[AXIS_X] = g_SimulatedShot.endPosition[AXIS_X];
        f_FlightBallPosition[AXIS_Y] = g_SimulatedShot.endPosition[AXIS_Y];
        f_FlightBallPosition[AXIS_Z] = g_SimulatedShot.endPosition[AXIS_Z];
    }
    else {
        f_FlightBallPosition[AXIS_X] = g_SimulatedShot.path[step][AXIS_X];
        f_FlightBallPosition[AXIS_Y] = g_SimulatedShot.path[step][AXIS_Y];
        f_FlightBallPosition[AXIS_Z] = g_SimulatedShot.path[step][AXIS_Z];
    }

    f_BallCupDistance = IntLength3D(
        f_FlightBallPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
        f_FlightBallPosition[AXIS_Y] - g_HoleSetup.cupPosition[AXIS_Y],
        f_FlightBallPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]
    );
    f_BallCupGroundDistance = IntLength2D(
        f_FlightBallPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
        f_FlightBallPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]
    );
    if (g_SimulatedShot.startsOnGreen) {
        switch (f_FlightPhase) {
            case FLIGHT_PHASE_LAUNCH:
                if (isAdvancing == TRUE &&
                    step == LAUNCH_END_STEP &&
                    g_BallFlightStep < g_SimulatedShot.stepCount - END_PHASE_STEP_COUNT) {
                    f_FlightPhase = FLIGHT_PHASE_CLIMB;
                    f_HasFlightPhaseChanged = TRUE;
                }

                break;
            case FLIGHT_PHASE_CLIMB:
                if (step == GREEN_CLIMB_END_STEP &&
                    g_BallFlightStep < g_SimulatedShot.stepCount - END_PHASE_STEP_COUNT) {
                    f_FlightPhase = FLIGHT_PHASE_CONTACT;
                    f_HasFlightPhaseChanged = TRUE;
                }

                break;
            case FLIGHT_PHASE_CONTACT:
                if (g_SimulatedShot.stepCount - END_PHASE_STEP_COUNT < g_BallFlightStep) {
                    f_FlightPhase = FLIGHT_PHASE_END;
                    f_HasFlightPhaseChanged = TRUE;
                }

                break;
        }
    }
    else if (!IsSimpleCameraChosen()) {
        switch (f_FlightPhase) {
            case FLIGHT_PHASE_NONE:
                f_FlightPhase = FLIGHT_PHASE_LAUNCH;
                f_HasFlightPhaseChanged = TRUE;
                break;
            case FLIGHT_PHASE_LAUNCH:
                if (isAdvancing == TRUE && step == LAUNCH_END_STEP) {
                    f_FlightPhase = FLIGHT_PHASE_EARLY_CLIMB;
                    f_HasFlightPhaseChanged = TRUE;
                }

                break;
            case FLIGHT_PHASE_EARLY_CLIMB:
                if (step == EARLY_CLIMB_END_STEP) {
                    f_FlightPhase = FLIGHT_PHASE_CLIMB;
                    f_HasFlightPhaseChanged = TRUE;
                }

                break;
            case FLIGHT_PHASE_CLIMB:
                if (g_BallFlightStep > g_SimulatedShot.positionStepMin[AXIS_Y]) {
                    f_FlightPhase = FLIGHT_PHASE_FALL;
                    f_HasFlightPhaseChanged = TRUE;
                }

                break;
            case FLIGHT_PHASE_FALL:
                if (g_SimulatedShot.pathFlags[step + CONTACT_LOOKAHEAD_STEP_COUNT] != 0) {
                    f_FlightPhase = FLIGHT_PHASE_NEARING_CONTACT;
                    f_HasFlightPhaseChanged = TRUE;
                }

                break;
            case FLIGHT_PHASE_NEARING_CONTACT:
                if (g_SimulatedShot.pathFlags[step] != 0) {
                    f_FlightPhase = FLIGHT_PHASE_CONTACT;
                    f_HasFlightPhaseChanged = TRUE;
                }

                break;
            case FLIGHT_PHASE_CONTACT:
                if (g_SimulatedShot.stepCount - END_PHASE_STEP_COUNT < g_BallFlightStep) {
                    f_FlightPhase = FLIGHT_PHASE_END;
                    f_HasFlightPhaseChanged = TRUE;
                }

                break;
        }
    }
    else {
        switch (f_FlightPhase) {
            case FLIGHT_PHASE_LAUNCH:
                f_FlightPhase = FLIGHT_PHASE_CLIMB;
                break;
            case FLIGHT_PHASE_CLIMB:
                if (g_BallFlightStep > g_SimulatedShot.positionStepMin[AXIS_Y]) {
                    f_FlightPhase = FLIGHT_PHASE_NEARING_CONTACT;
                    f_HasFlightPhaseChanged = TRUE;
                }

                break;
            case FLIGHT_PHASE_NEARING_CONTACT:
                break;
        }
    }

    aheadStep = step + CUP_PASS_LOOKAHEAD_STEP_COUNT;
    if (aheadStep <= g_SimulatedShot.stepCount &&
        IsCupPassPoint(
            IntLength2D(
                g_SimulatedShot.path[aheadStep][AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
                g_SimulatedShot.path[aheadStep][AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]
            ),
            g_SimulatedShot.path[aheadStep][AXIS_Y]
        )) {
        f_ComingFlightEvents |= FLIGHT_EVENT_CUP_PASS;
    }

    aheadStep = step + PATH_FLAG_LOOKAHEAD_STEP_COUNT;
    if (g_SimulatedShot.stepCount >= aheadStep &&
        (g_SimulatedShot.pathFlags[aheadStep] & STEP_FLAG_BILLBOARD_HIT)) {
        f_ComingFlightEvents |= FLIGHT_EVENT_BILLBOARD_HIT;
        if (f_BillboardHitStep == NO_STEP) {
            f_BillboardHitStep = aheadStep;
            f_BillboardHitCountdown = AHEAD_BILLBOARD_HIT_COUNTDOWN;
        }
    }

    if (step > BILLBOARD_HIT_VIEW_STEP_MIN &&
        step < f_BillboardHitStep &&
        !f_CameraChangeHoldCount) {
        f_FlightCameraKind = FLIGHT_CAMERA_BILLBOARD_HIT;
    }

    if (IsPastBillboardHitView(step) &&
        f_FlightCameraKind != FLIGHT_CAMERA_AROUND_END &&
        IsFarFromShotEnd()) {
        f_IsCameraKindNew = TRUE;
        f_FlightCameraKind = FLIGHT_CAMERA_AROUND_END;
    }

    if (f_BillboardHitStep < step &&
        HasTimeForNewCamera() &&
        f_HasFlightPhaseChanged == TRUE &&
        IsFlightCameraFree()) {
        if (f_CameraChangeHoldCount == 0) {
            ChooseFlightCameraKind();
        }
    }

    if (f_CameraChangeHoldCount > 0) {
        f_CameraChangeHoldCount--;
    }

    if (g_IsDebugEnabled && IsGameFlagSet(GAME_FLAG_TIMING_DEBUG)) {
        sprintf(
            g_PrintBuffer,
            "C:%d F:%2d %3d %3d ",
            (int)f_HasFlightPhaseChanged,
            (int)f_CameraChangeHoldCount,
            (int)step,
            (int)g_BallFlightStep
        );
        PrintDebugText(g_PrintBuffer, 8, 2);
        sprintf(g_PrintBuffer, "TYPE:%2d TIME:%2d", (int)f_FlightCameraKind, (int)f_FlightPhase);
        PrintDebugText(g_PrintBuffer, 10, 3);
    }

    if (f_ShotLength > VERY_LONG_SHOT_LENGTH_MIN &&
        g_BallFlightStep == 40 &&
        g_SimulatedShot.stepCount > 80) {
        StartShotEffect(SHOT_EFFECT_LONG_SHOT);
    }

    if (f_ShotLength > BALL_SOUND_SHOT_LENGTH_MIN && !g_SimulatedShot.usesPutter) {
        volume = (f_ShotLength / BALL_SOUND_VOLUME_LENGTH) + BALL_SOUND_VOLUME_MIN;
        if (volume > BALL_SOUND_VOLUME_MAX) {
            volume = BALL_SOUND_VOLUME_MAX;
        }

        if (step == BALL_SOUND_EARLY_STEP) {
            osSyncPrintf("ball:%d\n", volume);
            switch (f_FlightCameraKind) {
                case FLIGHT_CAMERA_UNCHANGED:
                case FLIGHT_CAMERA_UNUSED_A:
                case FLIGHT_CAMERA_BESIDE_TEE:
                case FLIGHT_CAMERA_AIM_VIEW:
                case FLIGHT_CAMERA_BESIDE_GOLFER:
                case FLIGHT_CAMERA_CIRCLING_BALL:
                case FLIGHT_CAMERA_CHASING_BALL:
                case FLIGHT_CAMERA_FOLLOWING_PATH:
                    PlaySoundWithEffects(
                        SOUND_SHOT_SPARKS,
                        BALL_SOUND_CHANNEL,
                        volume,
                        BALL_SOUND_REVERB,
                        SOUND_FULL_SCALE
                    );
                    break;
            }
        }

        if (step == BALL_SOUND_LATE_STEP) {
            osSyncPrintf("ball:%d\n", volume);
            switch (f_FlightCameraKind) {
                case FLIGHT_CAMERA_TRAILING_BALL:
                case FLIGHT_CAMERA_NEAR_LAUNCH:
                case FLIGHT_CAMERA_SIDE_OF_FLIGHT:
                case FLIGHT_CAMERA_AROUND_END:
                case FLIGHT_CAMERA_FACING_CUP:
                case FLIGHT_CAMERA_UNUSED_B:
                case FLIGHT_CAMERA_UNUSED_C:
                case FLIGHT_CAMERA_UNUSED_D:
                case FLIGHT_CAMERA_UNUSED_E:
                    PlaySoundWithEffects(
                        SOUND_ECHOING_SHOT,
                        BALL_SOUND_CHANNEL,
                        volume,
                        BALL_SOUND_REVERB,
                        SOUND_FULL_SCALE
                    );
                    break;
            }
        }
    }

    PlayFlightSounds(step);
    g_Cameras[MAIN_CAMERA].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
    PlaceFlightCamera(step);
    g_AreCharactersDrawn = TRUE;
    g_IsCupViewShown = FALSE;
    if (g_SimulatedShot.usesPutter) {
        if (g_IsModuleAlive[MODULE_GOLF_BALLS] || ReportModuleNotAlive(MODULE_GOLF_BALLS)) {
            StartFastBallAfterimage();
        }

        if ((g_SimulatedShot.pathFlags[step] & 0x100) ||
            (g_SimulatedShot.pathFlags[step] & STEP_FLAG_LIPPED_OUT)) {
            StartContactEffect(CONTACT_EFFECT_NONE);
        }
    }
    else if (g_SimulatedShot.pathFlags[step] != 0) {
        g_BallTrailState = BALL_TRAIL_HELD;
        StartContactEffect(CONTACT_EFFECT_FADE);
        if (g_IsModuleAlive[MODULE_GOLF_BALLS] || ReportModuleNotAlive(MODULE_GOLF_BALLS)) {
            StartFastBallAfterimage();
        }
    }

    if (f_BallCupGroundDistance > CUP_PASS_RADIUS) {
        if (f_CupViewHoldCount > 0) {
            f_CupViewHoldCount--;
        }
    }

    if (isAdvancing == TRUE) {
        if (f_BillboardHitCountdown > 0) {
            f_BillboardHitCountdown--;
        }

        if (!g_GameState.isMiniGolf && !g_SimulatedShot.endsInCup && IsPinHitViewed(step)) {
            g_Cameras[MAIN_CAMERA].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
            StartHudAnimation(HUD_ANIMATION_NONE);
            g_InsetView.animation = INSET_VIEW_STILL;
            g_InsetView.step = 0;
            g_Cameras[INSET_CAMERA].view.drawFlags = 0;
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] =
                (f32)g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] =
                ((f32)g_HoleSetup.cupPosition[AXIS_Y] - CUP_LOOK_AT_DEPTH) * COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] =
                (f32)g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] = g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] +
                (sinf(f_CupViewYaw) * CUP_CLOSE_VIEW_DISTANCE);
            g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] =
                g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] - CUP_CLOSE_VIEW_HEIGHT;
            g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] = g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] -
                (cosf(f_CupViewYaw) * CUP_CLOSE_VIEW_DISTANCE);
        }
        else if (IsCupPassPoint(f_BallCupGroundDistance, f_FlightBallPosition[AXIS_Y]) ||
            f_CupViewHoldCount > 0) {
            g_Cameras[MAIN_CAMERA].view.fieldOfView = CUP_VIEW_FIELD_OF_VIEW;
            StartHudAnimation(HUD_ANIMATION_NONE);
            g_IsCupViewShown = TRUE;
            g_AreCharactersDrawn = FALSE;
            f_ComingFlightEvents &= ~FLIGHT_EVENT_CUP_PASS;
            if (f_CupViewHoldCount == 0) {
                f_CupViewHoldCount = CUP_VIEW_HOLD_COUNT;
            }

            f_CameraChangeHoldCount = CAMERA_CHANGE_HOLD_COUNT;
            g_InsetView.animation = INSET_VIEW_STILL;
            g_InsetView.step = 0;
            g_Cameras[INSET_CAMERA].view.drawFlags = 0;
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] =
                (f32)g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] =
                (f32)g_HoleSetup.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] =
                (f32)g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
            point[AXIS_X] =
                (-sinf(f_CameraYaw) * CUP_VIEW_DISTANCE) + g_HoleSetup.cupPosition[AXIS_X];
            point[AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y] - CUP_VIEW_HEIGHT;
            point[AXIS_Z] =
                (cosf(f_CameraYaw) * CUP_VIEW_DISTANCE) + g_HoleSetup.cupPosition[AXIS_Z];
            g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        }
        else {
            g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] = f_CameraPosition[AXIS_X];
            g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] = f_CameraPosition[AXIS_Y];
            g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] = f_CameraPosition[AXIS_Z];
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] = f_LookAtPoint[AXIS_X];
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] = f_LookAtPoint[AXIS_Y];
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] = f_LookAtPoint[AXIS_Z];
        }
    }
    else {
        g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] = f_CameraPosition[AXIS_X];
        g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] = f_CameraPosition[AXIS_Y];
        g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] = f_CameraPosition[AXIS_Z];
        g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] = f_LookAtPoint[AXIS_X];
        g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] = f_LookAtPoint[AXIS_Y];
        g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] = f_LookAtPoint[AXIS_Z];
    }

    if (!g_IsCupViewShown) {
        AimCameraYaw();
    }

    g_Cameras[INSET_CAMERA].view.eye[AXIS_X] =
        (f32)(f_ShotEndPosition[AXIS_X] - INSET_VIEW_SIDE_OFFSET) * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.eye[AXIS_Y] =
        (f32)(f_ShotEndPosition[AXIS_Y] - INSET_VIEW_HEIGHT) * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.eye[AXIS_Z] =
        (f32)f_ShotEndPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.fieldOfView = INSET_FIELD_OF_VIEW;
    if (f_EndSurfaceTraits->kind == SURFACE_KIND_OUT_OF_BOUNDS) {
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] =
            (f32)f_ShotEndPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] =
            (f32)f_ShotEndPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] =
            (f32)f_ShotEndPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    }
    else {
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] =
            (f32)f_FlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] =
            (f32)f_FlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] =
            (f32)f_FlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    }

    if (g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] == g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] &&
        g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] == g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z]) {
        g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] += LOOK_AT_NUDGE;
        g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] += LOOK_AT_NUDGE;
    }

    if (g_Cameras[INSET_CAMERA].view.eye[AXIS_X] == g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] &&
        g_Cameras[INSET_CAMERA].view.eye[AXIS_Z] == g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z]) {
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] += LOOK_AT_NUDGE;
        g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] += LOOK_AT_NUDGE;
    }

    if (g_SimulatedShot.stepCount < g_BallFlightStep + 40 && IsHudAnimationDone() == 1) {
        StartHudAnimation(HUD_ANIMATION_NONE);
    }

    if (isAdvancing == TRUE && g_BallFlightStep == 0) {
        PlayContactSound();
    }

    waitFrame = f_LandingWaitFrame;
    if (isAdvancing == TRUE) {
        flightStep = &g_BallFlightStep;
        if (*flightStep == g_SimulatedShot.stepCount) {
            if (waitFrame == 0 && !g_TurnBallSimulation.isHoledOut) {
                StartLandingReaction(LANDING_REACTION_REST);
            }

            if (++f_LandingWaitFrame >= f_LandingWaitFrameCount) {
                if (IsGameFlagSet(GAME_FLAG_BLANK_TEXT_AFTER_FLIGHT)) {
                    PrintDebugText("        ", 16, 4);
                    PrintDebugText("         ", 10, 2);
                }

                return TRUE;
            }
        }
        else {
            (*flightStep)++;
        }
    }

    return FALSE;
}

void SetLandingWaitFrameCount(s32 frameCount) {
    f_LandingWaitFrameCount = frameCount;
}

#define VIEW_SIDE_DISTANCE 153600.0f

void GetViewSidePoints(
    CoursePoint center,
    CoursePoint plusQuarterPoint,
    CoursePoint minusQuarterPoint
) {
    f32 offsetX;
    f32 offsetZ;
    Radians yaw;
    Radians plusQuarterAngle;
    Radians minusQuarterAngle;

    offsetX = g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] - g_Cameras[MAIN_CAMERA].view.eye[AXIS_X];
    offsetZ = g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] - g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z];
    if ((s32)offsetX != 0 || (s32)offsetZ != 0) {
        yaw = M_PI - atan2f(offsetX, offsetZ);
    }
    else {
        yaw = 0.0f;
    }

    plusQuarterAngle = yaw + (M_PI / 2);
    plusQuarterPoint[AXIS_X] = (s32)(-sinf(plusQuarterAngle) * VIEW_SIDE_DISTANCE) + center[AXIS_X];
    plusQuarterPoint[AXIS_Z] = (s32)(cosf(plusQuarterAngle) * VIEW_SIDE_DISTANCE) + center[AXIS_Z];
    minusQuarterAngle = yaw - (M_PI / 2);
    minusQuarterPoint[AXIS_X] =
        (s32)(-sinf(minusQuarterAngle) * VIEW_SIDE_DISTANCE) + center[AXIS_X];
    minusQuarterPoint[AXIS_Z] =
        (s32)(cosf(minusQuarterAngle) * VIEW_SIDE_DISTANCE) + center[AXIS_Z];
}

void OnShotCameraPlaced(void) {
}

void FirstUnusedCameraStub(void) {
}

void SecondUnusedCameraStub(void) {
}

#define SPOT_QUARTER_SIZE 32
#define SPOT_PEAK_INTENSITY 210
#define SPOT_DISC_RADIUS 16
#define SPOT_GAP_RADIUS 24
#define SPOT_GAP_FALL_PER_TEXEL 26
#define SPOT_RING_RISE_PER_TEXEL 16
#define SPOT_SOFT_DISC_RADIUS 15
#define SPOT_SOFT_FALL_PER_TEXEL 12

void BuildSpotTexels(u8 *texels, Boolean isSoft) {
    s32 row;
    s32 column;
    s32 texelIndex;
    s32 radius;
    s32 squaredDistance;
    f32 root;
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
                radius = sqrtf(
                    ((row - height) * (row - height)) + ((column - width) * (column - width))
                );
                if (radius <= SPOT_DISC_RADIUS) {
                    intensity = SPOT_PEAK_INTENSITY;
                }
                else if (radius <= SPOT_GAP_RADIUS) {
                    intensity = SPOT_PEAK_INTENSITY -
                        ((radius - SPOT_DISC_RADIUS) * SPOT_GAP_FALL_PER_TEXEL);
                }
                else if (radius < SPOT_QUARTER_SIZE) {
                    intensity = (radius - SPOT_GAP_RADIUS) * SPOT_RING_RISE_PER_TEXEL;
                }
                else {
                    intensity = 0;
                }

                texels[texelIndex++] = intensity;
            }
        }
    }
    else {
        for (row = 0, softDiscRadius = SPOT_SOFT_DISC_RADIUS; row != height; row++) {
            for (column = 0; column != width; column++) {
                squaredDistance =
                    ((row - height) * (row - height)) + ((column - width) * (column - width));
                root = sqrtf(squaredDistance);
                if (squaredDistance == 0) {
                    distance = 0.0f;
                }

                distance = root;
                radius = distance;
                if (radius <= softDiscRadius) {
                    intensity = SPOT_PEAK_INTENSITY;
                }
                else if (radius <= lastRadius) {
                    intensity = SPOT_PEAK_INTENSITY -
                        ((s32)(distance - softDiscRadius) * SPOT_SOFT_FALL_PER_TEXEL);
                }
                else {
                    intensity = 0;
                }

                texels[texelIndex++] = intensity;
            }
        }
    }
}

void BuildRingTexels(u8 *texels, s32 width, s32 height, s32 outerRadius UNUSED) {
    s32 row;
    s32 column;
    s32 texelIndex;
    s32 radius;
    f32 distance;
    s32 intensity;

    texelIndex = 0;
    for (row = 0; row != height; row++) {
        for (column = 0; column != width; column++) {
            distance =
                sqrtf(((row - height) * (row - height)) + ((column - width) * (column - width)));
            radius = distance;
            if (radius < RING_INNER_RADIUS) {
                intensity = 0;
            }
            else if (radius < RING_PEAK_RADIUS) {
                intensity = ((s32)(distance - RING_INNER_RADIUS) * RING_RISE_PER_TEXEL) +
                    RING_INNER_INTENSITY;
            }
            else if (radius < RING_OUTER_RADIUS) {
                intensity = RING_PEAK_INTENSITY -
                    ((s32)(distance - RING_PEAK_RADIUS) * RING_FALL_PER_TEXEL);
            }
            else {
                intensity = 0;
            }

            texels[texelIndex++] = intensity;
        }
    }
}

#define UNDIVIDED_NOISE_SIZE_MIN 2
#define NOISE_DISPLACEMENT_MASK 0xFF
#define NOISE_DISPLACEMENT_BIAS 0x80
#define NOISE_CORNER_SEED (-0xFE0)
#define NOISE_CENTER_SEED 0x3F80
#define NOISE_EDGE_SEED 1
#define NOISE_BLEND_WEIGHT_MAX 0x3F
#define NOISE_BLEND_STEP_MASK 0x7F
#define NOISE_INTENSITY_SHIFT 13

static inline s32 RandomNoiseDisplacement(void) {
    return (rand() & NOISE_DISPLACEMENT_MASK) - NOISE_DISPLACEMENT_BIAS;
}

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
    roughness = f_NoiseRoughness[level];
    half = size >> 1;
    topLeft = f_NoiseGrid[row][column];
    topRight = f_NoiseGrid[row][(column + size) & NOISE_GRID_MASK];
    bottomLeft = f_NoiseGrid[(row + size) & NOISE_GRID_MASK][column];
    bottomRight = f_NoiseGrid[(row + size) & NOISE_GRID_MASK][(column + size) & NOISE_GRID_MASK];
    while (f_NoiseGrid[row + half][column + half] == 0) {
        f_NoiseGrid[row + half][column + half] =
            ((topLeft + topRight + bottomLeft + bottomRight) >> 2) +
            (RandomNoiseDisplacement() * roughness);
    }

    while (f_NoiseGrid[row][column + half] == 0) {
        f_NoiseGrid[row][column + half] =
            ((topLeft + topRight) >> 1) + (RandomNoiseDisplacement() * roughness);
    }

    while (f_NoiseGrid[row + half][column] == 0) {
        f_NoiseGrid[row + half][column] =
            ((topLeft + bottomLeft) >> 1) + (RandomNoiseDisplacement() * roughness);
    }

    while (f_NoiseGrid[row + half][(column + size) & NOISE_GRID_MASK] == 0) {
        f_NoiseGrid[row + half][(column + size) & NOISE_GRID_MASK] =
            ((topRight + bottomRight) >> 1) + (RandomNoiseDisplacement() * roughness);
    }

    while (f_NoiseGrid[(row + size) & NOISE_GRID_MASK][column + half] == 0) {
        f_NoiseGrid[(row + size) & NOISE_GRID_MASK][column + half] =
            ((bottomLeft + bottomRight) >> 1) + (RandomNoiseDisplacement() * roughness);
    }

    if (size > UNDIVIDED_NOISE_SIZE_MIN) {
        nextLevel = level + 1;
        DisplaceNoiseSquare(nextLevel, column, row);
        DisplaceNoiseSquare(nextLevel, column + half, row);
        DisplaceNoiseSquare(nextLevel, column, row + half);
        DisplaceNoiseSquare(nextLevel, column + half, row + half);
    }
}

static inline void SelectNoiseGrid(s16 gridIndex) {
    f_NoiseGrid = f_NoiseGrids[gridIndex];
}

void StepNoiseTexture(void) {
    s16 index;
    s16 row;
    s16 column;
    s8 weight;
    s32 intensity;
    s32 blend;

    if (!IsGameFlagSet(GAME_FLAG_PAUSE_NOISE_TEXTURE)) {
        if (f_IsNoiseResetPending) {
            f_IsNoiseResetPending = FALSE;
            f_NoiseBlendStep = 0;
            f_NoiseRow = 0;
            f_NoiseGridIndex = 0;
            for (index = 0; index < NOISE_GRID_COUNT; index++) {
                SelectNoiseGrid(index);
                for (row = 0; row < NOISE_GRID_SIZE; row++) {
                    for (column = 0; column < NOISE_GRID_SIZE; column++) {
                        f_NoiseGrid[row][column] = 0;
                    }
                }
            }

            SelectNoiseGrid(f_NoiseGridIndex);
            f_NoiseGrid[0][0] = NOISE_CORNER_SEED;
            f_NoiseGrid[NOISE_GRID_SIZE / 2][NOISE_GRID_SIZE / 2] = NOISE_CENTER_SEED;
            DisplaceNoiseSquare(0, 0, 0);

            f_NoiseGrid[0][0] = NOISE_CORNER_SEED;
            f_NoiseGrid[NOISE_GRID_SIZE / 2][NOISE_GRID_SIZE / 2] = NOISE_CENTER_SEED;
            SelectNoiseGrid(f_NoiseGridIndex ^ 1);
            DisplaceNoiseSquare(0, 0, 0);
        }

        index = f_NoiseRow * NOISE_GRID_SIZE;
        weight = f_NoiseBlendStep & NOISE_BLEND_WEIGHT_MAX;
        for (row = f_NoiseRow; row < f_NoiseRow + 1; row++) {
            for (column = 0; column < NOISE_GRID_SIZE; column++) {
                blend = ((f_NoiseGrids[f_NoiseGridIndex][row][column] *
                             (NOISE_BLEND_WEIGHT_MAX - weight)) +
                            (f_NoiseGrids[f_NoiseGridIndex ^ 1][row][column] * weight)) >>
                    NOISE_INTENSITY_SHIFT;
                intensity = Abs(blend);
                if (intensity > COLOR_CHANNEL_MAX) {
                    intensity = COLOR_CHANNEL_MAX;
                }

                g_NoiseTexels[index++] = intensity;
            }
        }

        f_NoiseRow = (f_NoiseRow + 1) & NOISE_GRID_MASK;
        if (f_NoiseRow == 0) {
            f_NoiseBlendStep = (f_NoiseBlendStep + 1) & NOISE_BLEND_STEP_MASK;
            if ((f_NoiseBlendStep & NOISE_BLEND_WEIGHT_MAX) == 0) {
                SelectNoiseGrid(f_NoiseGridIndex);
                for (row = 0; row < NOISE_GRID_SIZE; row++) {
                    for (column = 0; column < NOISE_GRID_SIZE; column++) {
                        f_NoiseGrid[row][column] = 0;
                    }
                }

                f_NoiseGrid[0][0] = 0;
                f_NoiseGrid[NOISE_GRID_SIZE / 2][NOISE_GRID_SIZE / 2] = 0;
                f_NoiseGrid[0][NOISE_GRID_SIZE / 2] = NOISE_EDGE_SEED;
                f_NoiseGrid[NOISE_GRID_SIZE / 2][0] = NOISE_EDGE_SEED;
                DisplaceNoiseSquare(0, 0, 0);
                f_NoiseGridIndex ^= 1;
            }
        }
    }
}

void BuildNoiseTexture(void) {
    s32 row;

    f_IsNoiseResetPending = TRUE;
    for (row = 0; row < NOISE_GRID_SIZE; row++) {
        StepNoiseTexture();
    }
}
