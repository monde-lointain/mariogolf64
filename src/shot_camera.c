/*
 * The cameras of a hole: the flyover, and the flight camera that follows the
 * shot with the sounds and reactions that go with it. The file also measures
 * the ground's slope around the ball and builds the noise textures.
 */
#include "common.h"

#include "shot_camera.h"

#include "ball_simulation.h"
#include "billboard.h"
#include "camera.h"
#include "character_slot.h"
#include "club.h"
#include "controller.h"
#include "course.h"
#include "current_shot.h"
#include "debug_text.h"
#include "framing.h"
#include "game_flags.h"
#include "golf_balls.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "hole_objects.h"
#include "libc.h"
#include "main.h"
#include "math_util.h"
#include "movie.h"
#include "overlay_manager.h"
#include "pin.h"
#include "round_events.h"
#include "round_hud.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "shot_effects.h"
#include "shot_simulation.h"
#include "sound.h"
#include "surface.h"
#include "terrain.h"
#include "text.h"
#include "turn.h"

#define RANDOM_YAW_TO_RADIANS (M_PI / 16384)

/*
 * The stages of the shot that UpdateFlightCamera moves f_FlightPhase through,
 * each giving ChooseFlightCameraKind its own choice of camera kinds. Shots
 * from the green skip EARLY_CLIMB, FALL and NEARING_CONTACT, and the simple
 * camera goes from CLIMB to NEARING_CONTACT at the highest step.
 */
enum FlightPhase {
  FLIGHT_PHASE_NONE,
  FLIGHT_PHASE_LAUNCH,
  FLIGHT_PHASE_EARLY_CLIMB, /* From the tenth step to the fifteenth */
  FLIGHT_PHASE_CLIMB,
  FLIGHT_PHASE_FALL, /* Past the path's highest step */
  FLIGHT_PHASE_NEARING_CONTACT, /* Forty steps before the first step flag */
  FLIGHT_PHASE_CONTACT, /* From the first step flag, or step 40 from the green
                         */
  FLIGHT_PHASE_END /* The last ten steps */
};

/* An enum FlightPhase value */
typedef s32 FlightPhase;

/* The flight camera kinds, which f_FlightCameraKind holds */
enum FlightCameraKind {
  /* None yet, as set when the flight camera starts and ends */
  FLIGHT_CAMERA_NONE = -1,
  FLIGHT_CAMERA_UNK0,
  FLIGHT_CAMERA_UNK1,
  /* Beside the tee, looking at it */
  FLIGHT_CAMERA_BESIDE_TEE,
  /* The aim camera, placed by PlaceAimCamera */
  FLIGHT_CAMERA_AIM_VIEW,
  /* Picked by the B button in debug builds */
  FLIGHT_CAMERA_BESIDE_GOLFER,
  /* Picked by the A button in debug builds */
  FLIGHT_CAMERA_CIRCLING_BALL,
  FLIGHT_CAMERA_TRAILING_BALL,
  /* At an early step of the path */
  FLIGHT_CAMERA_NEAR_LAUNCH,
  FLIGHT_CAMERA_CHASING_BALL,
  /* At a step of the path behind the ball */
  FLIGHT_CAMERA_FOLLOWING_PATH,
  FLIGHT_CAMERA_SIDE_OF_FLIGHT,
  FLIGHT_CAMERA_BEHIND_START,
  /* Placed by PlaceCameraAroundShotEnd */
  FLIGHT_CAMERA_AROUND_END,
  /* Beyond the ball on the line from the cup, looking at the ball and the cup
   */
  FLIGHT_CAMERA_FACING_CUP,
  FLIGHT_CAMERA_UNKE,
  FLIGHT_CAMERA_UNKF,
  FLIGHT_CAMERA_UNK10,
  FLIGHT_CAMERA_UNK11,
  /* Frames the path's extreme points on x and z */
  FLIGHT_CAMERA_FRAMING_PATH,
  /* Frames the cup and the ball; it keeps the putter's contact from calling
   * StartContactEffect */
  FLIGHT_CAMERA_FRAMING_CUP,
  /* Looks along the path at f_BillboardHitStep */
  FLIGHT_CAMERA_BILLBOARD_HIT
};

/* An enum FlightCameraKind value */
typedef s32 FlightCameraKind;

/*
 * Whether a holed shot is a chip-in: from off the green, after the first
 * stroke and without hitting the flag
 */
#define IsChipIn() \
  (!g_SimulatedShot.startsOnGreen && \
      g_Players[g_CurrentShot.playerIndex].strokeCount > 1 && \
      g_SimulatedShot.flagHitStep == NO_STEP)
/* Whether the pin is hit at the step, is about to be, or the monkey shows */
#define IsPinHitViewed(step) \
  ((g_SimulatedShot.pathFlags[step] & STEP_FLAG_PIN_HIT) || IsMonkeyShown() || \
      f_IsPinHitComing == TRUE)

/* Bits of f_ComingFlightEvents */
#define FLIGHT_EVENT_BILLBOARD_HIT (1 << 0)
#define FLIGHT_EVENT_CUP_PASS (1 << 1)
#define RANDOM_YAW_MASK 0x7FFF
#define SHORT_SHOT_MAX_LENGTH 0xBB7FF
#define CAMERA_CHANGE_HOLD_COUNT 20
#define NOISE_GRID_MASK (NOISE_GRID_SIZE - 1)

/* It is a macro, as an inline function compiles differently from the original
 */
#define IsBallLanding() \
  (g_SimulatedShot.stepCount < g_BallFlightStep + LANDING_STEP_COUNT && \
      (f32)f_ShotEndPosition[AXIS_X] < \
          (f32)(f_FlightBallPosition[AXIS_X] + LANDING_RADIUS) && \
      (f32)(f_FlightBallPosition[AXIS_X] - LANDING_RADIUS) < \
          (f32)f_ShotEndPosition[AXIS_X] && \
      (f32)f_ShotEndPosition[AXIS_Z] < \
          (f32)(f_FlightBallPosition[AXIS_Z] + LANDING_RADIUS) && \
      (f32)(f_FlightBallPosition[AXIS_Z] - LANDING_RADIUS) < \
          (f32)f_ShotEndPosition[AXIS_Z])

u8* g_NoiseTexels;

/* The grid that DisplaceNoiseSquare builds */
FILE_STATIC s16 (*f_NoiseGrid)[NOISE_GRID_SIZE];
/* The step of the simulated path that the ball's flight has reached */
FILE_STATIC s32 g_BallFlightStep;
/* Two grids of NOISE_GRID_SIZE x NOISE_GRID_SIZE heights, which the noise
 * texture blends */
FILE_STATIC s16
    f_NoiseGrids[NOISE_GRID_COUNT][NOISE_GRID_SIZE][NOISE_GRID_SIZE];

/* Frames waited since the ball's path ended */
static s32 f_LandingWaitFrame = 0;
/* StartFlightCamera's count of the flights before this one */
static s32 f_EarlierFlightCount = 0;
/* Set as the camera kind changes, for PlaceFlightCamera to set it up */
static Boolean f_IsCameraKindNew = FALSE;
static FlightCameraKind f_FlightCameraKind = 0;
/* Set as f_FlightPhase changes, and cleared as a camera kind is chosen */
static Boolean f_HasFlightPhaseChanged = FALSE;
static s32 f_BallCupDistance = 0;
static s32 f_BallCupGroundDistance = 0;
static s32 f_LandingCupDistance = 0;
/* How far the simulated shot goes, at least MIN_SHOT_LENGTH */
static s32 f_ShotLength = 0;
/* How far the shot starts from the cup */
static s32 f_StartCupDistance = 0;
/* Whether a billboard is hit in the first SCANNED_STEP_COUNT steps */
static Boolean f_HitsBillboardEarly = FALSE;
/* The first step of the path with STEP_FLAG_BILLBOARD_HIT set, or NO_STEP */
static s32 f_BillboardHitStep = 0;
/* Counted down from a billboard hit's discovery, but read by no routine */
static s32 f_BillboardHitCountdown = 0;
static Radians f_CameraYaw = 0.0f;
/* FLIGHT_EVENT_* bits for what the path comes to ahead of the ball */
static s32 f_ComingFlightEvents = 0;
/* A random angle about the ball for the trailing view */
static Radians f_TrailViewAngle = 0.0f;
static s32 f_CupViewHoldCount = 0;
static s32 f_FlightCameraFlags = 0;
/* Read by no routine */
static u8 f_UnusedBytesAfterFlightFlags[8] UNUSED = {0};
static s8 f_PendingResultSound = 0;
/* Frames left before ChooseFlightCameraKind may change the camera again */
static s32 f_CameraChangeHoldCount = 0;
/* Read by no routine: 1 for a shot from the green, 3 for one into water */
static s32 f_UnusedFlightStartKind = 0;
/* Whether the HUD shows during the flight, chosen at random after the first */
static Boolean f_IsFlightHudShown = FALSE;
/* Set from five steps ahead of a pin hit until the hit */
static Boolean f_IsPinHitComing = FALSE;
/* A random yaw about the cup for the close cup view and the monkey drop */
static Radians f_CupViewYaw = 0.0f;
static s32 f_EndReaction = 0;
/* How many frames the flight camera waits after the ball's path ends */
static s32 f_LandingWaitFrameCount = 0;
/* Whether the ground around the ball and the lie are level */
static Boolean f_IsBallGroundLevel = TRUE;
s32 g_HoleFlyoverFrame = 0;
/* Cleared while the cup view hides the characters from the main camera */
Boolean g_AreCharactersDrawn = TRUE;
Boolean g_IsCupViewShown = FALSE;
static s32 f_NextCameraPickIndex = 0;
static Boolean f_IsBehindViewFar = FALSE;
static s32 f_SideViewCount = 0;
static Boolean f_IsApexViewFar = FALSE;
static Boolean f_IsTrailViewClose = FALSE;
static Radians f_CircleAngle = 0.0f;
/* Read by no routine */
static u8 f_UnusedBytesAfterCircleAngle[8] UNUSED = {0};
/* The displacement scale at each midpoint displacement level */
static s16 f_NoiseRoughness[] = {0x80, 0x60, 0x40, 0x20, 8, 4, 4, 2};
static s16 f_NoiseGridIndex = 0;
static s32 f_NoiseBlendStep = 0;
static s8 f_IsNoiseResetPending = TRUE;
static s8 f_NoiseRow = 0;

/* menu.c returns an s8, but the original calls it without truncating, as if
 * undeclared */
#ifdef AVOID_UB
s8 IsSimpleCameraChosen(void);
#else
s32 IsSimpleCameraChosen();
#endif
void ChooseFlightCameraKind(void);
void PlaceFlightCamera(s32 step);
s32 StepHoleFlyover(void);
void EaseLookAtBelowBall(f32 drop);
void EaseLookAtPoint(Radians yaw);

/* Unused; the original has it */
static const char f_UnusedFloatFormat[] = "            %f";

/* The values 0 to CAMERA_PICK_COUNT - 1, shuffled; NextCameraPick deals them in
 * turn */
#define CAMERA_PICK_COUNT 10
/* How many swaps a shuffle makes */
#define CAMERA_PICK_SWAP_COUNT 5
#define NO_CAMERA_PICK (-1)

/* The field of view, in degrees, of the main camera between shots */
#define DEFAULT_FIELD_OF_VIEW 45.0f

/* The sound channels of the flight's sounds, which fade over
 * SOUND_FADE_FRAME_COUNT */
#define FLIGHT_SOUND_CHANNEL_UNKB 0xB
#define FLIGHT_SOUND_CHANNEL_UNKF 0xF
#define SOUND_FADE_FRAME_COUNT 120

/* f_EndReaction values: how the others reacted as the ball neared its end */
enum EndReaction {
  END_REACTION_NONE,
  END_REACTION_ONLOOKERS,
  END_REACTION_UNK2
};

/* Values of f_PendingResultSound, which plays when f_LandingWaitFrame reaches
 * RESULT_SOUND_STAGE */
enum ResultSound {
  RESULT_SOUND_NONE,
  RESULT_SOUND_UNK1, /* Plays SOUND_UNK23 */
  RESULT_SOUND_UNK2, /* Plays SOUND_THREE_STARS */
  RESULT_SOUND_STAGE = 15
};

/* The flyover's field of view, in degrees, and the inset view's rectangle */
#define FLYOVER_FIELD_OF_VIEW 80.0f
#define FLYOVER_INSET_CENTER_X 160
#define FLYOVER_INSET_CENTER_Y 112
#define FLYOVER_INSET_SIZE 128
/* The text slots that the hole's introduction shows */
#define INTRO_TEXT_SLOT_UNK0 0
#define INTRO_TEXT_SLOT_UNK1 1

/* How far the camera must be from where it eases to before it moves, and how
 * far it goes */
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

/* The fractions of the selected shot distance a full and a short shot go beyond
 */
#define FULL_SHOT_FRACTION 0.8f
#define SHORT_SHOT_FRACTION 0.6f
/* Whether the shot goes beyond a fraction of the distance selected for it */
#define IsShotBeyond(fraction) \
  (GetSelectedShotDistance() * (fraction) < f_ShotLength * YARDS_PER_UNIT)
/* Whether a stroke count on the hole is an albatross or better */
#define IsAlbatrossOrBetter(strokes) \
  (g_HoleSetup.par + SCORE_TO_PAR_ALBATROSS >= (strokes))

/*
 * The flyover's frames: it swings around the cup by a half turn every
 * FLYOVER_HALF_TURN_FRAME_COUNT frames up to a full turn, opens the inset view
 * and closes it again, and ends on HOLE_FLYOVER_FRAME_COUNT
 */
#define FLYOVER_HALF_TURN_FRAME_COUNT 90
#define FLYOVER_MAX_HALF_TURNS 2.0f
#define FLYOVER_INSET_OPEN_FRAME 94
#define FLYOVER_INSET_CLOSE_FRAME 162
/* How the flyover's distance from the cup, and height above it, follow the
 * hole's radius */
#define FLYOVER_RADIUS_APPROACH 15.0f
#define FLYOVER_DISTANCE_SCALE 1536.0f
#define FLYOVER_HEIGHT_APPROACH 200.0f
#define FLYOVER_HEIGHT_SCALE 1.5f
/* The inset camera looks at a point below the cup from this far and this much
 * higher */
#define FLYOVER_INSET_LOOK_AT_DROP 15360.0f
#define FLYOVER_INSET_DISTANCE 184320.0f
#define FLYOVER_INSET_EYE_RISE 75.0f

#define STRONG_WIND_SPEED 6

/* How far below the ball the look-at point settles, in world units */
#define LOOK_AT_DROP 19.650002f
/* The fractions of the way to the ball that the look-at point eases each frame
 */
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

static s8 f_CameraPicks[CAMERA_PICK_COUNT];
static CoursePoint f_FlightBallPosition;
static CoursePoint f_ShotEndPosition;
static CoursePoint f_ShotStartPosition;
static Radians f_ShotFlightYaw;
static Radians f_BillboardHitYaw;
static Radians f_EndYawFromCup;
static Radians f_StartYawFromCup;
/* Whether the shot's flight has been shown before, so that the HUD may hide */
static s32 f_IsLaterFlight;
static Vec3 f_CameraPosition;
static Vec3 f_LookAtPoint;
static s32 f_GreenSoundCount;
static Boolean f_IsLandingSoundPlayed;
/* Cleared by StartFlightCamera, and read by no routine */
static s32 f_UnusedClearedFlightWord;
static s8 f_PassedPathFlags;
static SurfaceInfo* f_EndSurfaceInfo;
/* How far the flight camera has followed the shot */
static FlightPhase f_FlightPhase;

/* How far the golfer stands along the shot's yaw from the ball */
#define STANCE_AIM_OFFSET (-921.60004f)
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
 * there. The golfer stands on the other side, mirrored, when player->unk22 is
 * set. Each pass measures the slope again at the point the last slope gives.
 */
void PlaceGolferAtBall(Vec3 position, Vec3 rotation, Vec3 scale) {
  Vec3 groundPoint;
  /* Pads the frame to match; without it 56 words differ */
  Mat4 unusedMatrices[2] UNUSED;
  Player* player;
  SignedBinaryAngle aimYaw;
  f32 distance;
  Radians angle;
  f32 reach;
  f32 slope;
  s32 ballHeight;
  s32 passIndex;

  player = &g_Players[g_CurrentShot.playerIndex];
  aimYaw = g_CurrentShot.heading;
  distance = GetBallDistance(
      0, player->clubs[g_CurrentShot.clubSlot], FALSE, g_CurrentShot.shotMode);
  angle = ((aimYaw + (BINARY_ANGLE_TURN / 4)) & BINARY_ANGLE_MASK) *
      BINARY_ANGLE_TO_RADIANS;
  if (player->unk22) {
    distance = -distance;
  }

  groundPoint[AXIS_X] = (cosf(angle) * distance) +
      (sinf(angle) * STANCE_AIM_OFFSET) + player->ballPosition[AXIS_X];
  /* The offset's sign on z differs from the loop's; the original has it */
  groundPoint[AXIS_Z] = (sinf(angle) * distance) +
      (cosf(angle) * STANCE_AIM_OFFSET) + player->ballPosition[AXIS_Z];
  groundPoint[AXIS_Y] =
      GetGroundHeight(groundPoint[AXIS_X], groundPoint[AXIS_Z]);
  ballHeight = player->ballPosition[AXIS_Y];
  slope = atan2(groundPoint[AXIS_Y] - ballHeight, distance);
  for (passIndex = 0; passIndex < STANCE_SLOPE_PASS_COUNT; passIndex++) {
    reach = cosf(slope) * distance;
    position[AXIS_X] = (cosf(angle) * reach) +
        (sinf(angle) * STANCE_AIM_OFFSET) + player->ballPosition[AXIS_X];
    position[AXIS_Z] = (sinf(angle) * reach) -
        (cosf(angle) * STANCE_AIM_OFFSET) + player->ballPosition[AXIS_Z];
    position[AXIS_Y] = GetGroundHeight(position[AXIS_X], position[AXIS_Z]);
    if (position[AXIS_Y] - ballHeight > AbsoluteF32(distance)) {
      position[AXIS_Y] = AbsoluteF32(distance) + ballHeight;
    }

    rotation[AXIS_X] = atan2(position[AXIS_Y] - ballHeight, reach);
    slope = rotation[AXIS_X];
  }

  scale[AXIS_Z] = 1.0f;
  scale[AXIS_Y] = scale[AXIS_Z];
  scale[AXIS_X] = scale[AXIS_Y];
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
SignedBinaryAngle GetCrossSlopeAngle(Player* player, ShotLaunch* shot) {
  SurfaceInfo* surfaceInfo;
  s32 sampleDistance;
  SignedBinaryAngle aimYaw;
  s32 yaw;
  Radians angle;
  s32 height;
  s32 rise;

  aimYaw = shot->heading;

  surfaceInfo = GetSurfaceInfo(player->ballSurface);
  sampleDistance = CROSS_SLOPE_DISTANCE;
  yaw = aimYaw + (BINARY_ANGLE_TURN / 4);
  if (player->unk22) {
    angle = BINARY_ANGLE_TO_RADIANS *
        ((yaw + (BINARY_ANGLE_TURN / 2)) & BINARY_ANGLE_MASK);
  } else {
    angle = (yaw & BINARY_ANGLE_MASK) * BINARY_ANGLE_TO_RADIANS;
  }

  height = GetGroundHeight(
      (s32)(cos(angle) * sampleDistance) + player->ballPosition[AXIS_X],
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
 * points half an AIM_SLOPE_STEP ahead of the ball and an AIM_SLOPE_STEP behind
 * it.
 */
SignedBinaryAngle GetAimSlopeAngle(Player* player, ShotLaunch* shot) {
  Radians angle;
  f32 step[2]; /* On x, then z */
  s32 aheadHeight;
  s32 behindHeight;

  angle = (u16)shot->heading * BINARY_ANGLE_TO_RADIANS;
  step[0] = cosf(angle) * AIM_SLOPE_STEP;
  step[1] = sinf(angle) * AIM_SLOPE_STEP;
  aheadHeight =
      GetGroundHeight(((s32)step[0] / 2) + player->ballPosition[AXIS_X],
          ((s32)step[1] / 2) + player->ballPosition[AXIS_Z]);
  if (aheadHeight < player->ballPosition[AXIS_Y] - AIM_SLOPE_MAX_RISE) {
    aheadHeight = player->ballPosition[AXIS_Y] - AIM_SLOPE_MAX_RISE;
  }

  if (aheadHeight > player->ballPosition[AXIS_Y] + AIM_SLOPE_MAX_RISE) {
    aheadHeight = player->ballPosition[AXIS_Y] + AIM_SLOPE_MAX_RISE;
  }

  behindHeight = GetGroundHeight(player->ballPosition[AXIS_X] - (s32)step[0],
      player->ballPosition[AXIS_Z] - (s32)step[1]);
  if (behindHeight < player->ballPosition[AXIS_Y] - AIM_SLOPE_MAX_RISE) {
    behindHeight = player->ballPosition[AXIS_Y] - AIM_SLOPE_MAX_RISE;
  }

  if (behindHeight > player->ballPosition[AXIS_Y] + AIM_SLOPE_MAX_RISE) {
    behindHeight = player->ballPosition[AXIS_Y] + AIM_SLOPE_MAX_RISE;
  }

  return atan2(aheadHeight - behindHeight, AIM_SLOPE_STEP * 1.5) *
      RADIANS_TO_BINARY_ANGLE;
}

/* Unreferenced; the original has it, and its zero is in the ROM */
static const f64 f_UnusedZero = 0.0;

/* The surface under the player's ball. Its one caller passes the shot as the
 * slope angles' routines take it, though the shot is not read. */
SurfaceInfo* GetBallSurfaceInfo(Player* player, ShotLaunch* shot UNUSED) {
  return GetSurfaceInfoAt(
      player->ballPosition[AXIS_X], player->ballPosition[AXIS_Z]);
}

Boolean IsDebugBButtonView(void) {
  return f_FlightCameraKind == FLIGHT_CAMERA_BESIDE_GOLFER;
}

/*
 * Deals the next camera pick, shuffling them again after the last. When
 * isDealing is false, it shuffles them and returns NO_CAMERA_PICK.
 *
 * SwapCameraPicks is a nested function, a GNU C extension.
 */
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

    /*
     * The != tests are there to match: < gives 24 words differing, 14 in shape
     */
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

/*
 * Ends the flight camera: it restores the main camera's field of view, stops
 * drawing the inset view, fades the flight's sounds, and reports a ball that
 * has finished its path with unk2E41 set.
 */
void EndFlightCamera(void) {
  g_AreCharactersDrawn = TRUE;
  g_IsCupViewShown = FALSE;
  f_CupViewHoldCount = 0;
  g_BallTrailState = BALL_TRAIL_OFF;
  g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
  StartHudAnimation(HUD_ANIMATION_NONE);
  g_InsetView.animation = INSET_VIEW_STILL;
  g_InsetView.step = 0;
  g_Cameras[INSET_CAMERA].view.drawFlags = 0;
  FadeOutSound(FLIGHT_SOUND_CHANNEL_UNKB, SOUND_FADE_FRAME_COUNT);
  FadeOutSound(FLIGHT_SOUND_CHANNEL_UNKF, SOUND_FADE_FRAME_COUNT);
  ClearShotEffects(CLEAR_KIND_EFFECTS);
  f_FlightCameraKind = FLIGHT_CAMERA_NONE;
  if (g_BallFlightStep == g_SimulatedShot.stepCount) {
    osSyncPrintf("\nCheckWater1\n");
    if (g_SimulatedShot.unk2E41) {
      osSyncPrintf("\nCheckWater2\n");
      CountNotableShot(g_CurrentShot.playerIndex, SHOT_KIND_WATER_CUT);
    }
  }
}

Boolean IsCupPassPoint(s32 cupDistance, s32 height);
s32 RandomBelow(s32 limit);

/* Random yaws come in steps of a 32768th of a turn */
#define RANDOM_YAW_RANGE 0x7FFF
/* The shortest shot length that the cameras work with */
#define MIN_SHOT_LENGTH 0x7800
/* The path steps searched for cup passes and for path flags */
#define SCANNED_STEP_COUNT 30
/* f_BillboardHitCountdown's start for a hit found early, or found ahead */
#define EARLY_BILLBOARD_HIT_COUNTDOWN 40
#define AHEAD_BILLBOARD_HIT_COUNTDOWN 70
/* The ground is probed at five points, 45 degrees apart, around the ball */
#define GROUND_PROBE_COUNT 5
#define GROUND_PROBE_ANGLE_STEP (M_PI / 4)
#define GROUND_PROBE_DISTANCE 30720.0f
#define GROUND_PROBE_MAX_RISE 4608.0f

/*
 * Starts the flight camera for the simulated shot. It resets the flight state,
 * records the shot's start and end points and their yaws from the cup, looks
 * for cup passes and flagged steps at the start of the path, and probes the
 * ground on one side of the ball. Then it picks the first camera kind and moves
 * the main camera to it. earlierFlightCount is ovl_00's count of the earlier
 * calls; on the first, the camera picks are shuffled again.
 */
void StartFlightCamera(s32 earlierFlightCount) {
  CoursePoint probe;
  s32 i;
  Radians yaw;
  f32 degrees;

  f_EarlierFlightCount = earlierFlightCount;
  ClearShotEffects(CLEAR_KIND_ALL);
  ResetBallTrail();
  f_CameraChangeHoldCount = 0;
  f_EndSurfaceInfo = GetSurfaceInfo(g_SimulatedShot.endSurface);
  if (earlierFlightCount == 0) {
    NextCameraPick(FALSE);
  }

  StartHudAnimation(HUD_ANIMATION_NONE);
  StartBanner(BANNER_NONE);
  f_FlightCameraFlags = 0;

  /*
   * The separate tests are there to match: one || test gives 3 words differing,
   * 3 in shape
   */
  if (earlierFlightCount == 0) {
    f_FlightCameraFlags |= FLIGHT_CAMERA_FLAG_UNK2;
  } else if (RandomBelow(2) == 0) {
    f_FlightCameraFlags |= FLIGHT_CAMERA_FLAG_UNK2;
  }

  if (earlierFlightCount == 0) {
    g_CapturedSliceCount = 0;
  }

  g_InsetView.animation = INSET_VIEW_STILL;
  g_InsetView.step = 0;
  g_Cameras[INSET_CAMERA].view.drawFlags = 0;

  /*
   * The nested ifs are there to match: one && chain gives 10 words differing, 4
   * in shape
   */
  if (!g_SimulatedShot.startsOnGreen) {
    if (earlierFlightCount != 0 && g_ContactEffect == CONTACT_EFFECT_NONE) {
      g_RoundHud.unk17 = TRUE;
    } else {
      g_RoundHud.unk17 = FALSE;
    }
  } else {
    g_RoundHud.unk17 = FALSE;
  }

  g_BallFlightStep = 0;
  f_LandingWaitFrame = 0;
  f_IsCameraKindNew = FALSE;
  f_CupViewHoldCount = 0;
  f_GreenSoundCount = 0;
  f_IsLandingSoundPlayed = FALSE;
  f_UnusedClearedFlightWord = 0;
  f_EndReaction = END_REACTION_NONE;
  f_CameraYaw = (f32)RandomBelow(RANDOM_YAW_RANGE) * RANDOM_YAW_TO_RADIANS;
  f_TrailViewAngle = (f32)(rand() & RANDOM_YAW_RANGE) * RANDOM_YAW_TO_RADIANS;
  f_CupViewYaw = (f32)(rand() & RANDOM_YAW_RANGE) * RANDOM_YAW_TO_RADIANS;
  if (earlierFlightCount == 0) {
    f_IsLaterFlight = FALSE;
  } else {
    f_IsLaterFlight = TRUE;
  }

  f_UnusedFlightStartKind = 0;
  if (g_SimulatedShot.startsOnGreen) {
    f_UnusedFlightStartKind = 1;
  } else if (f_EndSurfaceInfo->kind == SURFACE_KIND_WATER_HAZARD) {
    f_UnusedFlightStartKind = 3;
  }

  f_PassedPathFlags = 0;
  f_PendingResultSound = RESULT_SOUND_NONE;
  f_IsFlightHudShown = 1;
  if (f_IsLaterFlight) {
    f_IsFlightHudShown = RandomBelow(2);
    g_RoundHud.unkC = f_IsFlightHudShown;
    g_RoundHud.isWindIndicatorShown = f_IsFlightHudShown;
    g_RoundHud.unk8 = f_IsFlightHudShown;
    g_RoundHud.isLieIconShown = f_IsFlightHudShown;
    g_RoundHud.isClubHeadIconShown = f_IsFlightHudShown;
    g_RoundHud.unkB = f_IsFlightHudShown;
    g_RoundHud.unkD = f_IsFlightHudShown;
  }

  /* Converting through f32, and writing the start point first, are there to
   * match */
  f_ShotStartPosition[AXIS_X] = (f32)g_SimulatedShot.startPosition[AXIS_X];
  f_ShotStartPosition[AXIS_Y] = (f32)g_SimulatedShot.startPosition[AXIS_Y];
  f_ShotStartPosition[AXIS_Z] = (f32)g_SimulatedShot.startPosition[AXIS_Z];
  f_ShotEndPosition[AXIS_X] = (f32)g_SimulatedShot.endPosition[AXIS_X];
  f_ShotEndPosition[AXIS_Y] = (f32)g_SimulatedShot.endPosition[AXIS_Y];
  f_ShotEndPosition[AXIS_Z] = (f32)g_SimulatedShot.endPosition[AXIS_Z];
  f_ShotFlightYaw =
      -atan2((f32)(f_ShotEndPosition[AXIS_X] - f_ShotStartPosition[AXIS_X]),
          (f32)(f_ShotEndPosition[AXIS_Z] - f_ShotStartPosition[AXIS_Z]));
  f_EndYawFromCup =
      -atan2((f32)(f_ShotEndPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X]),
          (f32)(f_ShotEndPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]));
  f_StartYawFromCup = -atan2(
      (f32)(f_ShotStartPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X]),
      (f32)(f_ShotStartPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]));
  f_StartCupDistance = GetCupDistance(
      f_ShotStartPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
      f_ShotStartPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]);
  f_ShotLength =
      IntLength3D(f_ShotStartPosition[AXIS_X] - f_ShotEndPosition[AXIS_X],
          f_ShotStartPosition[AXIS_Y] - f_ShotEndPosition[AXIS_Y],
          f_ShotStartPosition[AXIS_Z] - f_ShotEndPosition[AXIS_Z]);
  if (f_ShotLength < MIN_SHOT_LENGTH) {
    f_ShotLength = MIN_SHOT_LENGTH;
  }

  f_LandingCupDistance = GetCupDistance(
      f_ShotEndPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
      f_ShotEndPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]);
  f_ComingFlightEvents = 0;

  /*
   * The != tests are there to match: < in the first loop gives 34 words
   * differing (6 in shape), and in both 50 (10)
   */
  for (i = 0; i != SCANNED_STEP_COUNT; i++) {
    if (i < g_SimulatedShot.stepCount &&
        IsCupPassPoint(IntLength2D(g_SimulatedShot.path[i][AXIS_X] -
                               g_HoleSetup.cupPosition[AXIS_X],
                           g_SimulatedShot.path[i][AXIS_Z] -
                               g_HoleSetup.cupPosition[AXIS_Z]),
            g_SimulatedShot.path[i][AXIS_Y])) {
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
    if (g_Players[g_CurrentShot.playerIndex].unk22 == 1) {
      yaw = ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) +
          ((f32)i * GROUND_PROBE_ANGLE_STEP);
    } else {
      yaw = ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) -
          ((f32)i * GROUND_PROBE_ANGLE_STEP);
    }

    probe[AXIS_X] = (cosf(yaw) * GROUND_PROBE_DISTANCE) +
        g_CurrentShot.ballPosition[AXIS_X];
    probe[AXIS_Z] = (sinf(yaw) * GROUND_PROBE_DISTANCE) +
        g_CurrentShot.ballPosition[AXIS_Z];
    probe[AXIS_Y] = GetGroundHeight(probe[AXIS_X], probe[AXIS_Z]);
    if (probe[AXIS_Y] <
        g_CurrentShot.ballPosition[AXIS_Y] - GROUND_PROBE_MAX_RISE) {
      f_IsBallGroundLevel = FALSE;
    }

    if (probe[AXIS_Y] >
        g_CurrentShot.ballPosition[AXIS_Y] + GROUND_PROBE_MAX_RISE) {
      f_IsBallGroundLevel = FALSE;
    }
  }

  degrees = g_RoundHud.lieTilt * (360.0f * BINARY_ANGLE_TO_RADIANS) *
      (1.0 / (2 * M_PI));
  if (degrees < 350.0f && degrees > 10.0f) {
    f_IsBallGroundLevel = FALSE;
  }

  f_IsCameraKindNew = FALSE;
  f_HasFlightPhaseChanged = FALSE;
  f_FlightPhase = FLIGHT_PHASE_LAUNCH;
  f_FlightCameraKind = FLIGHT_CAMERA_NONE;
  f_IsPinHitComing = FALSE;
  g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
  f_FlightBallPosition[AXIS_X] = f_ShotStartPosition[AXIS_X];
  f_FlightBallPosition[AXIS_Y] = f_ShotStartPosition[AXIS_Y];
  f_FlightBallPosition[AXIS_Z] = f_ShotStartPosition[AXIS_Z];
  ChooseFlightCameraKind();
  f_IsCameraKindNew = TRUE;
  PlaceFlightCamera(0);
  g_Cameras[0].view.eye[AXIS_X] = f_CameraPosition[AXIS_X];
  g_Cameras[0].view.eye[AXIS_Y] = f_CameraPosition[AXIS_Y];
  g_Cameras[0].view.eye[AXIS_Z] = f_CameraPosition[AXIS_Z];
  g_Cameras[0].view.lookAt[AXIS_X] = f_LookAtPoint[AXIS_X];
  g_Cameras[0].view.lookAt[AXIS_Y] = f_LookAtPoint[AXIS_Y];
  g_Cameras[0].view.lookAt[AXIS_Z] = f_LookAtPoint[AXIS_Z];
}

/* The clubs in slots below it get CONTACT_EFFECT_POWER_IRON on some power shots
 */
#define LEADING_CLUB_SLOT_COUNT 6

/* These are macros, as inline functions compile differently from the original
 */
#define IsUnk2E14PowerShot() \
  (g_SimulatedShot.shot.shotMode == SHOT_MODE_POWER && g_SimulatedShot.unk2E14)

/* Plays the sound of the simulated shot's club meeting the ball */
void PlayContactSound(void) {
  Club* club;
  SurfaceInfo* surfaceInfo;

  club = GetClub(g_Players[g_SimulatedShot.shot.playerIndex]
          .clubs[g_SimulatedShot.shot.clubIndex]);
  surfaceInfo = GetSurfaceInfo(g_CurrentShot.surface);
  if (club->kind == CLUB_KIND_WOOD) {
    if (IsUnk2E14PowerShot()) {
      PlaySound(SOUND_MENU_START, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
      StartContactEffect(CONTACT_EFFECT_POWER_WOOD);
    } else if (surfaceInfo->kind == SURFACE_KIND_TEE_UP) {
      StartContactEffect(CONTACT_EFFECT_SHOT);
      PlaySound(SOUND_MENU_START, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
    } else {
      StartContactEffect(CONTACT_EFFECT_SHOT);
      PlaySound(SOUND_CONTACT_WOOD, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
    }

    StartImpactSparks();
  } else if (club->kind == CLUB_KIND_IRON || club->kind == CLUB_KIND_WEDGE) {
    if (IsUnk2E14PowerShot()) {
      if (g_CurrentShot.clubSlot < LEADING_CLUB_SLOT_COUNT) {
        StartContactEffect(CONTACT_EFFECT_POWER_IRON);
      } else {
        StartContactEffect(CONTACT_EFFECT_SHOT);
      }
    } else {
      StartContactEffect(CONTACT_EFFECT_SHOT);
    }

    if (surfaceInfo->kind == SURFACE_KIND_TEE_UP) {
      PlaySound(
          SOUND_CONTACT_IRON_TEE, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
    } else if (IsFairwayKind(surfaceInfo->kind)) {
      PlaySound(
          SOUND_ENTER_TOURNAMENT, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
    } else if (IsBunkerKind(surfaceInfo->kind)) {
      PlaySound(
          SOUND_CONTACT_IRON_BUNKER, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
    } else {
      PlaySound(
          SOUND_ENTER_TOURNAMENT, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
    }
  } else {
    PlaySound(SOUND_CONTACT_PUTTER, CONTACT_SOUND_CHANNEL, SOUND_FULL_SCALE);
    if (f_FlightCameraKind != FLIGHT_CAMERA_FRAMING_CUP) {
      StartContactEffect(CONTACT_EFFECT_PUTT);
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
Boolean IsCupPassPoint(s32 cupDistance, s32 height) {
  if (cupDistance < CUP_PASS_RADIUS &&
      height > g_HoleSetup.cupPosition[AXIS_Y] - CUP_PASS_MAX_DEPTH &&
      f_LandingCupDistance < CUP_PASS_MAX_LANDING_DISTANCE &&
      (f_FlightCameraFlags & FLIGHT_CAMERA_FLAG_UNK2)) {
    return TRUE;
  }

  return FALSE;
}

/* It is a macro, as an inline function compiles differently from the original
 */
#define IsShotLengthRated(endKind) \
  ((endKind) != SURFACE_KIND_WATER_HAZARD && \
      f_StartCupDistance >= f_LandingCupDistance && \
      f_HitsBillboardEarly != TRUE && g_SimulatedShot.unk2E14 && \
      !g_SimulatedShot.unk2E00 && \
      g_SimulatedShot.shot.shotMode != SHOT_MODE_APPROACH)

/*
 * Rates how far the simulated shot goes, in yards, against the distance
 * selected for it. Shots that end out of play or in water, approach shots and
 * some others go unrated.
 */
s32 RateShotLength(void) {
  s32 endKind;

  endKind = f_EndSurfaceInfo->kind;
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

#define FLIGHT_SOUND_CHANNEL_UNK3 3
#define FLIGHT_SOUND_CHANNEL_UNKE 0xE
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
/* The green landing sounds play this many steps after the ball lands */
#define GREEN_SOUND_DELAY_STEP_COUNT 2

/* Whether the ball hit the ground the given number of steps back */
#define HasHitGroundBefore(step, stepsBack) \
  (g_SimulatedShot.pathFlags[(step) - (stepsBack)] & STEP_FLAG_GROUND_HIT)
/* Sounds that play this many steps before the end of the flight */
#define LANDING_SOUND_STEP_COUNT 3
#define CHIP_IN_CALL_STEP_COUNT 10
#define FLAG_SHOT_CALL_STEP_COUNT 15
/* f_IsPinHitComing is set this many steps before a pin hit */
#define PIN_HIT_LOOKAHEAD_STEP_COUNT 5

/* Distances that PlayFlightSounds tests, in course units */
#define CLOSE_LANDING_DISTANCE 0x3C00
#define NEAR_LANDING_DISTANCE 0x5A00
#define GREEN_SOUND_MAX_DISTANCE 0x7800
#define LONG_PUTT_MIN_DISTANCE 0x25800
#define LONG_SHOT_SOUND_MIN_DISTANCE 0x4B000
#define LONG_LANDING_MIN_DISTANCE (60.0f * UNITS_PER_YARD)

/* Queues a character sound of the given kind for each player but the one who
 * shot */
static inline void QueueOnlookerSounds(s32 kind) {
  s32 i;

  for (i = 0; i != g_GameState.playerCount; i++) {
    if (i != g_CurrentShot.playerIndex) {
      QueueCharacterSound(g_Players[i].characterIndex, i, kind,
          SOUND_FULL_SCALE, g_Players[i].soundPan);
    }
  }
}

/* Whether the shot counts as an approach: in that mode, or with the putter */
#define IsApproachShot() \
  (g_SimulatedShot.shot.shotMode == SHOT_MODE_APPROACH || \
      g_SimulatedShot.usesPutter)

/*
 * Whether a putt earns the Nice Touch call: from far, stopping close, with no
 * lip-out or pin hit passed, and before par
 */
#define IsNiceTouchPutt() \
  (f_LandingCupDistance < CLOSE_LANDING_DISTANCE && \
      f_StartCupDistance > LONG_PUTT_MIN_DISTANCE && \
      !(f_PassedPathFlags & (STEP_FLAG_LIPPED_OUT | STEP_FLAG_PIN_HIT)) && \
      g_HoleSetup.par > g_Players[g_CurrentShot.playerIndex].strokeCount)

/* Whether the shot reaches the green from off it, without the putter */
#define IsShotOntoGreen() \
  (!g_SimulatedShot.usesPutter && !g_SimulatedShot.startsOnGreen && \
      g_SimulatedShot.endsOnGreen && \
      f_EndSurfaceInfo->kind == SURFACE_KIND_GREEN)

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
  SurfaceInfo* info;
  Player* player;
  /*
   * The pointers are there to match: reading the green sound count
   * directly gives 11 words differing (3 in shape), either landing sound
   * flag 8 (2), and all three 27 (7)
   */
  s32* greenSoundCount;
  s32* isPlayed;
  s32* isStillPlayed;

  scoreToPar =
      g_Players[g_CurrentShot.playerIndex].strokeCount - g_HoleSetup.par;
  shownRingCount = 0;
  if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
    player = &g_Players[g_RoundHud.unk87];
    for (i = 0; i != RING_COUNT; i++) {
      shownRingCount += player->ringStates[i] == RING_STATE_SHOWN;
    }
  }

  f_PassedPathFlags |= g_SimulatedShot.pathFlags[step];
  if (step == FULL_SHOT_REACTION_STEP && RateShotLength() == SHOT_LENGTH_FULL) {
    QueueOnlookerSounds(CHARACTER_SOUND_KIND_FULL_SHOT);
  }

  if (step == SHOT_RESULT_STEP) {
    if (f_EndSurfaceInfo->kind == SURFACE_KIND_OUT_OF_COURSE &&
        !g_GameState.isMiniGolf) {
      PlaySound(
          SOUND_OUT_OF_COURSE, FLIGHT_SOUND_CHANNEL_UNK3, SOUND_FULL_SCALE);
      QueueOnlookerSounds(CHARACTER_SOUND_KIND_OUT_OF_COURSE);
    } else if (RateShotLength() == SHOT_LENGTH_FULL) {
      PlaySoundWithEffects(SOUND_FULL_SHOT, FLIGHT_SOUND_CHANNEL_UNK3,
          SOUND_FULL_SCALE, FULL_SHOT_REVERB,
          g_Players[g_CurrentShot.playerIndex].soundPan);
    }
  }

  if (step == UNK2E42_SOUND_STEP && g_SimulatedShot.unk2E42) {
    /* Plays the sound again for each player; the original has it */
    for (i = 0; i != g_GameState.playerCount; i++) {
      PlaySound(SOUND_UNK151, FLIGHT_SOUND_CHANNEL_UNK3, SOUND_FULL_SCALE);
      if (i != g_CurrentShot.playerIndex) {
        QueueCharacterSound(g_Players[i].characterIndex, i,
            CHARACTER_SOUND_KIND_TROUBLE, SOUND_FULL_SCALE,
            g_Players[i].soundPan);
      }
    }
  }

  if (step > EARLY_STEP_COUNT) {
    greenSoundCount = &f_GreenSoundCount;
    if (!*greenSoundCount && !shownRingCount &&
        HasHitGroundBefore(step, GREEN_SOUND_DELAY_STEP_COUNT) &&
        IsShotOntoGreen()) {
      if (g_HoleSetup.par >
          g_Players[g_CurrentShot.playerIndex].strokeCount + 1) {
        (*greenSoundCount)++;
        PlaySound(SOUND_UNK117, FLIGHT_SOUND_CHANNEL_UNKB, GREEN_SOUND_VOLUME);
      } else if (g_HoleSetup.par >
              g_Players[g_CurrentShot.playerIndex].strokeCount + 2 &&
          f_LandingCupDistance < GREEN_SOUND_MAX_DISTANCE) {
        (*greenSoundCount)++;
        PlaySound(SOUND_UNK118, FLIGHT_SOUND_CHANNEL_UNKB, GREEN_SOUND_VOLUME);
      }
    }
  }

  if (f_LandingWaitFrame == RESULT_SOUND_STAGE) {
    if (f_PendingResultSound == RESULT_SOUND_UNK1) {
      PlaySound(SOUND_UNK23, FLIGHT_SOUND_CHANNEL_UNKE, SOUND_FULL_SCALE);
    } else if (f_PendingResultSound == RESULT_SOUND_UNK2) {
      PlaySound(SOUND_THREE_STARS, FLIGHT_SOUND_CHANNEL_UNKE, SOUND_FULL_SCALE);
    }
  }

  if (!g_GameState.isMiniGolf) {
    if ((g_SimulatedShot.pathFlags[step + PIN_HIT_LOOKAHEAD_STEP_COUNT] &
            STEP_FLAG_PIN_HIT) &&
        g_SimulatedShot.flagHitStep == NO_STEP) {
      f_IsPinHitComing = TRUE;
    }

    if ((g_SimulatedShot.pathFlags[step] & STEP_FLAG_PIN_HIT) &&
        !g_SimulatedShot.endsInCup && g_SimulatedShot.flagHitStep == NO_STEP) {
      f_IsPinHitComing = FALSE;
      StartMonkeyDrop(f_CupViewYaw);
    }
  }

  if (!shownRingCount && g_BallFlightStep == g_SimulatedShot.stepCount - 1 &&
      !g_SimulatedShot.endsInCup) {
    if (g_SimulatedShot.startsOnGreen) {
      if (IsNiceTouchPutt()) {
        StartBanner(BANNER_NICE_TOUCH);
        PlaySound(
            SOUND_CALL_NICE_TOUCH, FLIGHT_SOUND_CHANNEL_UNK3, SOUND_FULL_SCALE);
      }
    } else if (g_SimulatedShot.flagHitStep != NO_STEP) {
      StartBanner(BANNER_FLAG_SHOT);
      PlaySound(
          SOUND_CALL_FLAG_SHOT, FLIGHT_SOUND_CHANNEL_UNK3, SOUND_FULL_SCALE);
      CountNotableShot(g_CurrentShot.playerIndex, SHOT_KIND_FLAG_SHOT);
      f_PendingResultSound = RESULT_SOUND_UNK2;
    } else if (f_PassedPathFlags & STEP_FLAG_PIN_HIT) {
      CountNotableShot(g_CurrentShot.playerIndex, SHOT_KIND_PIN_SHOT);
      StartBanner(BANNER_PIN_SHOT);
      PlaySound(
          SOUND_CALL_PIN_SHOT, FLIGHT_SOUND_CHANNEL_UNK3, SOUND_FULL_SCALE);
      f_PendingResultSound = RESULT_SOUND_UNK2;
    } else if (g_SimulatedShot.endsOnGreen) {
      if (IsAlbatrossOrBetter(
              g_Players[g_CurrentShot.playerIndex].strokeCount)) {
        CountNotableShot(g_CurrentShot.playerIndex, SHOT_KIND_GREAT);
        StartBanner(BANNER_GREAT);
        PlaySound(
            SOUND_CALL_GREAT, FLIGHT_SOUND_CHANNEL_UNK3, SOUND_FULL_SCALE);
        QueueOnlookerSounds(CHARACTER_SOUND_KIND_GREAT);
        f_PendingResultSound = RESULT_SOUND_UNK2;
      } else if (g_HoleSetup.par + SCORE_TO_PAR_EAGLE ==
          g_Players[g_CurrentShot.playerIndex].strokeCount) {
        if (f_LandingCupDistance <= CLOSE_LANDING_DISTANCE) {
          CountNotableShot(g_CurrentShot.playerIndex, SHOT_KIND_EXCELLENT);
          StartBanner(BANNER_EXCELLENT);
          PlaySound(SOUND_CALL_EXCELLENT, FLIGHT_SOUND_CHANNEL_UNK3,
              SOUND_FULL_SCALE);
          QueueOnlookerSounds(CHARACTER_SOUND_KIND_EXCELLENT);
          f_PendingResultSound = RESULT_SOUND_UNK1;
        } else if (IsApproachShot()) {
          if (f_LandingCupDistance <= NEAR_LANDING_DISTANCE) {
            StartBanner(BANNER_NICE_APPROACH);
            PlaySound(SOUND_CALL_NICE_APPROACH, FLIGHT_SOUND_CHANNEL_UNK3,
                SOUND_FULL_SCALE);
            f_PendingResultSound = RESULT_SOUND_UNK1;
          } else {
            StartBanner(BANNER_NICE_ON);
            PlaySound(SOUND_CALL_NICE_ON, FLIGHT_SOUND_CHANNEL_UNK3,
                SOUND_FULL_SCALE);
          }
        } else {
          StartBanner(BANNER_NICE_ON);
          PlaySound(
              SOUND_CALL_NICE_ON, FLIGHT_SOUND_CHANNEL_UNK3, SOUND_FULL_SCALE);
        }
      } else if (g_HoleSetup.par + SCORE_TO_PAR_BIRDIE ==
              g_Players[g_CurrentShot.playerIndex].strokeCount &&
          IsApproachShot() && f_LandingCupDistance <= NEAR_LANDING_DISTANCE) {
        StartBanner(BANNER_NICE_APPROACH);
        PlaySound(SOUND_CALL_NICE_APPROACH, FLIGHT_SOUND_CHANNEL_UNK3,
            SOUND_FULL_SCALE);
      }
    }
  }

  if (g_SimulatedShot.endsInCup) {
    if (g_BallFlightStep >=
            g_SimulatedShot.stepCount - FLAG_SHOT_CALL_STEP_COUNT &&
        f_EndReaction == END_REACTION_NONE &&
        g_SimulatedShot.flagHitStep != NO_STEP) {
      StartBanner(BANNER_FLAG_SHOT);
      PlaySound(
          SOUND_CALL_FLAG_SHOT, FLIGHT_SOUND_CHANNEL_UNK3, SOUND_FULL_SCALE);
      QueueOnlookerSounds(CHARACTER_SOUND_KIND_EXCELLENT);
      CountNotableShot(g_CurrentShot.playerIndex, SHOT_KIND_FLAG_SHOT);
      f_EndReaction = END_REACTION_ONLOOKERS;
      f_PendingResultSound = RESULT_SOUND_UNK2;
    }

    if (g_BallFlightStep >=
            g_SimulatedShot.stepCount - CHIP_IN_CALL_STEP_COUNT &&
        f_EndReaction == END_REACTION_NONE && IsChipIn()) {
      StartBanner(BANNER_CHIP_IN);
      PlaySound(
          SOUND_CALL_CHIP_IN, FLIGHT_SOUND_CHANNEL_UNK3, SOUND_FULL_SCALE);
      QueueOnlookerSounds(CHARACTER_SOUND_KIND_GREAT);
      f_EndReaction = END_REACTION_ONLOOKERS;
    }
  }

  isPlayed = &f_IsLandingSoundPlayed;
  if (!*isPlayed) {
    if (g_BallFlightStep == g_SimulatedShot.stepCount && !shownRingCount &&
        g_SimulatedShot.endsInCup) {
      *isPlayed = TRUE;
      if (IsGameFlagSet(GAME_FLAG_UNK48)) {
        if (g_IsCupViewShown == TRUE &&
            IsScoreToParWithin(
                scoreToPar, SCORE_TO_PAR_EAGLE, SCORE_TO_PAR_BOGEY) &&
            !IsPinInCup()) {
          StartMovie(MOVIE_GOOMBA);
        }
      } else if (IsScoreToParWithin(
                     scoreToPar, SCORE_TO_PAR_EAGLE, SCORE_TO_PAR_PAR) &&
          !IsPinInCup() && g_IsCupViewShown == TRUE) {
        if (g_SimulatedShot.shot.shotMode == SHOT_MODE_NORMAL) {
          StartMovie(MOVIE_MONTY_MOLE);
        } else if (g_SimulatedShot.shot.shotMode == SHOT_MODE_POWER) {
          StartMovie(MOVIE_GOOMBA);
        }
      }

      if (g_SimulatedShot.startsOnGreen) {
        if (f_StartCupDistance > LONG_SHOT_SOUND_MIN_DISTANCE ||
            IsGameFlagSet(GAME_FLAG_UNK48)) {
          PlaySound(SOUND_UNK155, FLIGHT_SOUND_CHANNEL_UNK3, SOUND_FULL_SCALE);
          f_EndReaction = END_REACTION_UNK2;
        }
      }

      if (!g_SimulatedShot.startsOnGreen) {
        if (f_StartCupDistance > LONG_LANDING_MIN_DISTANCE) {
          CountNotableShot(g_CurrentShot.playerIndex, SHOT_KIND_LONG_CHIP_IN);
          f_PendingResultSound = RESULT_SOUND_UNK2;
        } else {
          CountNotableShot(g_CurrentShot.playerIndex, SHOT_KIND_CHIP_IN);
          f_PendingResultSound = RESULT_SOUND_UNK1;
        }
      }

      if (g_Players[g_CurrentShot.playerIndex].strokeCount == 1) {
        PlaySound(SOUND_UNK118, FLIGHT_SOUND_CHANNEL_UNKF, SOUND_FULL_SCALE);
      } else if (g_HoleSetup.par + SCORE_TO_PAR_ALBATROSS ==
          g_Players[g_CurrentShot.playerIndex].strokeCount) {
        PlaySound(
            SOUND_UNK118, FLIGHT_SOUND_CHANNEL_UNKF, FLIGHT_SOUND_VOLUME_UNK64);
      } else if (g_HoleSetup.par + SCORE_TO_PAR_EAGLE ==
          g_Players[g_CurrentShot.playerIndex].strokeCount) {
        PlaySound(
            SOUND_UNK118, FLIGHT_SOUND_CHANNEL_UNKF, FLIGHT_SOUND_VOLUME_UNK50);
      } else if (g_HoleSetup.par + SCORE_TO_PAR_BIRDIE ==
          g_Players[g_CurrentShot.playerIndex].strokeCount) {
        PlaySound(
            SOUND_UNK116, FLIGHT_SOUND_CHANNEL_UNKF, FLIGHT_SOUND_VOLUME_UNK60);
      }
    }

    isStillPlayed = &f_IsLandingSoundPlayed;
    if (!*isStillPlayed && g_BallFlightStep > EARLY_STEP_COUNT &&
        HasHitGroundBefore(step, EARLY_STEP_COUNT) &&
        IsShotBeyond(FULL_SHOT_FRACTION) &&
        /* The surface is looked up only when the tests before pass, and
         * IsFairwayKind reads its kind three times; calling inside it
         * differs */
        (info = GetSurfaceInfo(g_TurnBallSimulation.surface),
            IsFairwayKind(info->kind)) &&
        !g_GameState.isMiniGolf && f_StartCupDistance > f_LandingCupDistance &&
        IsAlbatrossOrBetter(g_Players[g_CurrentShot.playerIndex].strokeCount)) {
      *isStillPlayed = TRUE;
      PlaySound(SOUND_UNK117, FLIGHT_SOUND_CHANNEL_UNKB, FAIRWAY_SOUND_VOLUME);
    }
  }

  if (g_BallFlightStep > EARLY_STEP_COUNT &&
      g_BallFlightStep ==
          g_SimulatedShot.stepCount - LANDING_SOUND_STEP_COUNT) {
    if (IsBunkerKind(f_EndSurfaceInfo->kind)) {
      f_IsLandingSoundPlayed = TRUE;
      PlaySound(
          SOUND_TROUBLE_LANDING, FLIGHT_SOUND_CHANNEL_UNKB, SOUND_FULL_SCALE);
      QueueOnlookerSounds(CHARACTER_SOUND_KIND_BUNKER);
    } else if (f_EndSurfaceInfo->kind == SURFACE_KIND_DEEP_ROUGH) {
      PlaySound(SOUND_DEEP_ROUGH_LANDING, FLIGHT_SOUND_CHANNEL_UNKB,
          SOUND_FULL_SCALE);
      QueueOnlookerSounds(CHARACTER_SOUND_KIND_BUNKER);
    } else if (f_EndSurfaceInfo->kind == SURFACE_KIND_WASTE_AREA) {
      PlaySound(SOUND_WASTE_AREA_LANDING, FLIGHT_SOUND_CHANNEL_UNKB,
          SOUND_FULL_SCALE);
    } else if (IsLostBallKind(f_EndSurfaceInfo->kind)) {
      f_IsLandingSoundPlayed = TRUE;
      PlaySound(
          SOUND_TROUBLE_LANDING, FLIGHT_SOUND_CHANNEL_UNKB, SOUND_FULL_SCALE);
      QueueOnlookerSounds(CHARACTER_SOUND_KIND_TROUBLE);
    }
  }
}

/* Eases the look-at point toward the ball, unless it is landing. */
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

/*
 * Eases the look-at point toward a point drop below the ball, unless it is
 * landing.
 */
void EaseLookAtBelowBall(f32 drop) {
  f32 offsetX;
  f32 offsetY;
  f32 offsetZ;
  f32 targetCoordinate;

  if (!IsBallLanding()) {
    targetCoordinate = f_FlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
    offsetX = targetCoordinate - f_LookAtPoint[AXIS_X];
    targetCoordinate =
        (f_FlightBallPosition[AXIS_Y] - drop) * COURSE_TO_WORLD_SCALE;
    offsetY = targetCoordinate - f_LookAtPoint[AXIS_Y];
    targetCoordinate = f_FlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    offsetZ = targetCoordinate - f_LookAtPoint[AXIS_Z];

    f_LookAtPoint[AXIS_X] += offsetX * LOOK_AT_EASE_FRACTION;
    f_LookAtPoint[AXIS_Y] += offsetY * LOOK_AT_HEIGHT_EASE_FRACTION;
    f_LookAtPoint[AXIS_Z] += offsetZ * LOOK_AT_EASE_FRACTION;
  }
}

/*
 * Eases the look-at point toward the ball.
 * It ignores yaw, which PlaceFlightCamera passes.
 */
void EaseLookAtPoint(Radians yaw UNUSED) {
  f32 offsetX;
  f32 offsetY;
  f32 offsetZ;
  f32 targetCoordinate;

  targetCoordinate = f_FlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
  offsetX = targetCoordinate - f_LookAtPoint[AXIS_X];
  targetCoordinate =
      (f_FlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE) - LOOK_AT_DROP;
  offsetY = targetCoordinate - f_LookAtPoint[AXIS_Y];
  targetCoordinate = f_FlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
  offsetZ = targetCoordinate - f_LookAtPoint[AXIS_Z];

  f_LookAtPoint[AXIS_X] += offsetX * LOOK_AT_EASE_FRACTION;
  f_LookAtPoint[AXIS_Y] += offsetY * LOOK_AT_HEIGHT_EASE_FRACTION;
  f_LookAtPoint[AXIS_Z] += offsetZ * LOOK_AT_EASE_FRACTION;
}

/* Puts the look-at point on the ball */
void SnapLookAtPoint(void) {
  f_LookAtPoint[AXIS_X] = f_FlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
  f_LookAtPoint[AXIS_Y] = f_FlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
  f_LookAtPoint[AXIS_Z] = f_FlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
}

/* Eases each of the camera's coordinates toward target's, unless it is already
 * close */
void EaseCameraPosition(Vec3 target) {
  f32 offset;

  offset = target[AXIS_X] - f_CameraPosition[AXIS_X];

  /* The x test in a block of its own is there to match; without it 3 words
   * differ, all in shape, as with a local for each axis's offset. A loop
   * over the axes gives 66, 54 in shape, fabs 66 and the comparison spelled
   * as two tests 27 */
  IN_OWN_BLOCK(if (fabsf(offset) > CAMERA_EASE_DEAD_ZONE) {
    f_CameraPosition[AXIS_X] += offset * CAMERA_EASE_FRACTION;
  });
  offset = target[AXIS_Y] - f_CameraPosition[AXIS_Y];
  if (fabsf(offset) > CAMERA_EASE_DEAD_ZONE) {
    f_CameraPosition[AXIS_Y] += offset * CAMERA_EASE_FRACTION;
  }

  offset = target[AXIS_Z] - f_CameraPosition[AXIS_Z];
  if (fabsf(offset) > CAMERA_EASE_DEAD_ZONE) {
    f_CameraPosition[AXIS_Z] += offset * CAMERA_EASE_FRACTION;
  }
}

#define UNK2E08_HEIGHT_SPREAD_FRACTION 0.2
#define DISTANCE_SPREAD_FRACTION 0.2f
#define DEFAULT_HEIGHT_SPREAD_FRACTION 0.1f
#define OPTION_UNK3_HEIGHT_SPREAD_FRACTION 0.2f
#define UNK2E08_MIN_HEIGHT 0x6600
#define RANDOM_DISTANCE_STEP_COUNT 1536
#define UNK2E08_DISTANCE_STEP 15
#define UNK2E08_MIN_DISTANCE 0xB400
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
 * Places the camera at a random yaw around the shot's end, a random distance
 * away and a random height above the ground there, then snaps the look-at
 * point. The ground point is kept at least MIN_RISE_ABOVE_SHOT_END above the
 * shot's end. Course y points down, so heights are subtracted.
 */
void PlaceCameraAroundShotEnd(void) {
  CoursePoint point;
  f32 height;
  /*
   * Holding the scale in yawScale is there to match: the constant in its
   * place, on either side, gives 2 words differing (2 in shape), and an f32
   * local 25
   */
  f64 yawScale;
  f32 distance;
  Radians yaw;

  if (g_SimulatedShot.startsOnGreen) {
    height = RandomBelow(f_ShotLength * UNK2E08_HEIGHT_SPREAD_FRACTION) +
        UNK2E08_MIN_HEIGHT;
    distance =
        ((guRandom() % RANDOM_DISTANCE_STEP_COUNT) * UNK2E08_DISTANCE_STEP) +
        UNK2E08_MIN_DISTANCE;
    RandomBelow(RANDOM_YAW_MASK);
    yawScale = M_PI / 65536;
    yaw = f_ShotFlightYaw - ((f32)(rand() & RANDOM_YAW_MASK) * yawScale);
  } else if (!IsSimpleCameraChosen()) {
    if (f_ShotLength <= SHORT_SHOT_MAX_LENGTH) {
      distance = SHORT_SHOT_DISTANCE;
      height = 0.0f;
      yaw = f_ShotFlightYaw + (M_PI / 8) -
          ((f32)(rand() & RANDOM_YAW_MASK) * (M_PI / 131072));
    } else {
      if (g_SimulatedShot.endsOnGreen) {
        distance =
            ((guRandom() % RANDOM_DISTANCE_STEP_COUNT) * GREEN_DISTANCE_STEP) +
            GREEN_MIN_DISTANCE;
      } else if (f_ShotLength * DISTANCE_SPREAD_FRACTION != 0.0f) {
        distance = ((guRandom() % f_ShotLength) * DISTANCE_SPREAD_FRACTION) +
            MIN_CAMERA_DISTANCE;
      } else {
        distance = MIN_CAMERA_DISTANCE;
      }

      if (f_ShotLength * DEFAULT_HEIGHT_SPREAD_FRACTION != 0.0f) {
        height =
            ((guRandom() % f_ShotLength) * DEFAULT_HEIGHT_SPREAD_FRACTION) +
            DEFAULT_MIN_HEIGHT;
      } else {
        height = DEFAULT_MIN_HEIGHT;
      }

      if (height > DEFAULT_MAX_HEIGHT) {
        height = DEFAULT_MAX_HEIGHT;
      }

      if (distance > MAX_CAMERA_DISTANCE) {
        distance = MAX_CAMERA_DISTANCE;
      }

      yaw = f_ShotFlightYaw + M_PI + (M_PI * 5 / 8) +
          ((f32)(rand() & RANDOM_YAW_MASK) * (M_PI / 32768 * 1.225f));
    }
  } else {
    if (f_ShotLength * DISTANCE_SPREAD_FRACTION != 0.0f) {
      distance = ((guRandom() % f_ShotLength) * DISTANCE_SPREAD_FRACTION) +
          MIN_CAMERA_DISTANCE;
    } else {
      distance = MIN_CAMERA_DISTANCE;
    }

    if (f_ShotLength * OPTION_UNK3_HEIGHT_SPREAD_FRACTION != 0.0f) {
      height =
          ((guRandom() % f_ShotLength) * OPTION_UNK3_HEIGHT_SPREAD_FRACTION) +
          OPTION_UNK3_MIN_HEIGHT;
    } else {
      height = OPTION_UNK3_MIN_HEIGHT;
    }

    if (height > OPTION_UNK3_MAX_HEIGHT) {
      height = OPTION_UNK3_MAX_HEIGHT;
    }

    if (distance > MAX_CAMERA_DISTANCE) {
      distance = MAX_CAMERA_DISTANCE;
    }

    yaw = f_ShotFlightYaw + M_PI + (M_PI / 2) +
        ((f32)(rand() & RANDOM_YAW_MASK) * (M_PI / 32768));
  }

  point[AXIS_X] = (-sinf(yaw) * distance) + f_ShotEndPosition[AXIS_X];
  point[AXIS_Z] = (cosf(yaw) * distance) + f_ShotEndPosition[AXIS_Z];
  point[AXIS_Y] = GetGroundHeight(point[AXIS_X], point[AXIS_Z]);
  if (point[AXIS_Y] > f_ShotEndPosition[AXIS_Y] - MIN_RISE_ABOVE_SHOT_END) {
    point[AXIS_Y] = f_ShotEndPosition[AXIS_Y] - MIN_RISE_ABOVE_SHOT_END;
  }

  point[AXIS_Y] = point[AXIS_Y] - height;
  f_CameraPosition[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
  f_CameraPosition[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
  f_CameraPosition[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
  SnapLookAtPoint();
}

/* Resets the cameras for the start of a hole, and plays the game mode's song */
void StartHoleCamerasAndSong(void) {
  SetMenuBackdrop(MENU_BACKDROP_NONE);
  g_RoundHud.isHoleIntroTextShown = TRUE;
  g_RoundHud.scorecardPage = NO_SCORECARD_PAGE;
  g_RoundHud.unk8 = 1;
  g_RoundHud.unkB = 1;
  g_InsetView.animation = INSET_VIEW_STILL;
  g_InsetView.step = 0;
  g_HoleFlyoverFrame = 0;
  g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
  g_Cameras[1].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
  if (g_GameState.replayKind != REPLAY_KIND_NONE) {
    PlaySong(SONG_ROUND_UNK31, SONG_FLAG_UNK10 | SONG_FLAG_UNREAD);
    return;
  }

  switch (g_GameState.gameMode) {
    case GAME_MODE_TRAINING:
      PlaySong(SONG_ROUND_UNK26, SONG_FLAG_UNREAD);
      break;
    case GAME_MODE_MATCH_GAME:
    case GAME_MODE_GET_CHARACTER:
      if (GetMatchStanding(0, 1) != MATCH_STANDING_OPEN) {
        PlaySong(SONG_ROUND_FINAL, SONG_FLAG_UNREAD);
      } else {
        PlaySong(SONG_ROUND_MATCH, SONG_FLAG_UNREAD);
      }

      break;
    case GAME_MODE_SKINS_MATCH:
      if (GetLastHoleIndex() == g_GameState.holeIndex) {
        PlaySong(SONG_ROUND_FINAL, SONG_FLAG_UNREAD);
      } else {
        PlaySong(SONG_ROUND_SKINS_MATCH, SONG_FLAG_UNREAD);
      }

      break;
    case GAME_MODE_TOURNAMENT:
      PlaySong(SONG_ROUND_TOURNAMENT, SONG_FLAG_UNREAD);
      break;
    case GAME_MODE_SPEED_GOLF:
      PlaySong(SONG_ROUND_UNK24, SONG_FLAG_UNREAD);
      break;
    case GAME_MODE_RING_SHOT:
      PlaySong(SONG_ROUND_RING_SHOT, SONG_FLAG_UNREAD);
      g_RoundHud.unk8 = 0;
      g_RoundHud.unkB = 0;
      break;
    case GAME_MODE_CLUB_SLOTS:
      if (GetLastHoleIndex() == g_GameState.holeIndex) {
        PlaySong(SONG_ROUND_FINAL, SONG_FLAG_UNREAD);
      } else {
        PlaySong(SONG_ROUND_CLUB_SLOTS, SONG_FLAG_UNREAD);
      }

      break;
    case GAME_MODE_STROKE:
    default:
      if (g_GameState.holeIndex == GetLastHoleIndex() &&
          g_GameState.playerCount != 1) {
        PlaySong(SONG_ROUND_FINAL, SONG_FLAG_UNREAD);
      } else if (g_GameState.isMiniGolf) {
        PlaySong(SONG_ROUND_MINI_GOLF, SONG_FLAG_UNREAD);
      } else {
        PlaySong(SONG_ROUND_COURSE, SONG_FLAG_UNREAD);
      }

      break;
  }
}

/* Starts the flyover of the hole, with the inset view on the cup */
void StartHoleFlyover(void) {
  g_HoleFlyoverFrame = 0;
  g_RoundHud.isHoleIntroTextShown = FALSE;
  g_InsetView.animation = INSET_VIEW_STILL;
  g_InsetView.step = 0;
  g_Cameras[INSET_CAMERA].view.drawFlags = 0;
  g_Cameras[0].mode = CAMERA_MODE_BEHIND_GOLFER;
  g_Cameras[INSET_CAMERA].mode = CAMERA_MODE_BEHIND_GOLFER;
  g_Cameras[0].view.fieldOfView = FLYOVER_FIELD_OF_VIEW;
  SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, INTRO_TEXT_SLOT_UNK0, 0,
      0, 0, 0, 0);
  SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, INTRO_TEXT_SLOT_UNK1, 0,
      0, 0, 0, 0);
  SetInsetViewRect(FLYOVER_INSET_CENTER_X, FLYOVER_INSET_CENTER_Y,
      FLYOVER_INSET_SIZE, FLYOVER_INSET_SIZE);
  StepHoleFlyover();
}

void ClearHoleIntroText(void) {
  g_RoundHud.isHoleIntroTextShown = FALSE;
  SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, INTRO_TEXT_SLOT_UNK0, 0,
      0, 0, 0, 0);
  SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, INTRO_TEXT_SLOT_UNK1, 0,
      0, 0, 0, 0);
}

/* Ends the hole flyover; the screen fades back, and a strong wind brings the
 * Fly Guy that warns of it */
void EndHoleFlyover(void) {
  g_HoleFlyoverFrame = HOLE_FLYOVER_ENDED;
  g_InsetView.animation = INSET_VIEW_STILL;
  g_InsetView.step = 0;
  g_Cameras[INSET_CAMERA].view.drawFlags = 0;
  if (g_GameState.gameMode != GAME_MODE_SPEED_GOLF && !g_GameState.isMiniGolf) {
    if (IsGameFlagSet(GAME_FLAG_UNK48) ||
        g_HoleSetup.conditions.windSpeed / WIND_SPEED_UNIT >=
            STRONG_WIND_SPEED) {
      ShowWindFlyGuy(TRUE);
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
  Radians yaw;
  f32 distance;
  f32 height;
  s32 holeRadius;

  halfTurns = g_HoleFlyoverFrame * (1.0f / FLYOVER_HALF_TURN_FRAME_COUNT);
  if (g_HoleFlyoverFrame > FLYOVER_HALF_TURN_FRAME_COUNT) {
    widening = 0.0f;
    approach = 1.0f - sinf(DegreesToRadians(g_HoleFlyoverFrame));
  } else {
    approach = (1.0f - sinf(DegreesToRadians(g_HoleFlyoverFrame))) * 0.5f;
    widening = (FLYOVER_HALF_TURN_FRAME_COUNT - g_HoleFlyoverFrame) *
        (1.0f / FLYOVER_HALF_TURN_FRAME_COUNT);
  }

  if (halfTurns > FLYOVER_MAX_HALF_TURNS) {
    halfTurns = FLYOVER_MAX_HALF_TURNS;
  }

  if (!g_GameState.isMiniGolf) {
    if (g_HoleFlyoverFrame == FLYOVER_INSET_OPEN_FRAME) {
      g_InsetView.animation = INSET_VIEW_OPENING;
    }

    if (g_HoleFlyoverFrame == FLYOVER_INSET_CLOSE_FRAME) {
      g_InsetView.animation = INSET_VIEW_CLOSING;
    }
  }

  offset[AXIS_X] =
      g_HoleSetup.teePosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X];
  offset[AXIS_Y] =
      g_HoleSetup.teePosition[AXIS_Y] - g_HoleSetup.cupPosition[AXIS_Y];
  offset[AXIS_Z] =
      g_HoleSetup.teePosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z];
  g_InsetView.unk8 = 100;
  g_InsetView.unkC = 101;
  g_Cameras[0].view.lookAt[AXIS_X] =
      (g_HoleSetup.cupPosition[AXIS_X] + (offset[AXIS_X] * approach)) *
      COURSE_TO_WORLD_SCALE;
  g_Cameras[0].view.lookAt[AXIS_Y] =
      (g_HoleSetup.cupPosition[AXIS_Y] + (offset[AXIS_Y] * approach)) *
      COURSE_TO_WORLD_SCALE;
  g_Cameras[0].view.lookAt[AXIS_Z] =
      (g_HoleSetup.cupPosition[AXIS_Z] + (offset[AXIS_Z] * approach)) *
      COURSE_TO_WORLD_SCALE;
  yaw = atan2(offset[AXIS_X], offset[AXIS_Z]) + (halfTurns * M_PI);
  holeRadius = GetHoleRadius(GetCourseRecordIndex(), GetHoleRecordIndex());

  /*
   * Storing the sine before computing distance is there to match: multiplying
   * it by distance afterwards gives 22 words differing, 6 in shape
   */
  offset[AXIS_X] = sinf(yaw);
  distance = ((holeRadius - (approach * FLYOVER_RADIUS_APPROACH)) +
                 (holeRadius * widening)) *
      FLYOVER_DISTANCE_SCALE;
  offset[AXIS_X] *= distance;
  offset[AXIS_Z] = cosf(yaw) * distance;
  g_Cameras[0].view.eye[AXIS_X] = g_Cameras[0].view.lookAt[AXIS_X] +
      (offset[AXIS_X] * COURSE_TO_WORLD_SCALE);
  yaw += yaw;
  height = ((holeRadius / 2) + (approach * FLYOVER_HEIGHT_APPROACH)) *
      FLYOVER_HEIGHT_SCALE;
  g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] =
      g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
  g_Cameras[0].view.eye[AXIS_Y] = g_Cameras[0].view.lookAt[AXIS_Y] - height;
  g_Cameras[0].view.eye[AXIS_Z] = g_Cameras[0].view.lookAt[AXIS_Z] +
      (offset[AXIS_Z] * COURSE_TO_WORLD_SCALE);
  g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] =
      (g_HoleSetup.cupPosition[AXIS_Y] - FLYOVER_INSET_LOOK_AT_DROP) *
      COURSE_TO_WORLD_SCALE;
  g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] =
      g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
  offset[AXIS_X] = sinf(yaw) * FLYOVER_INSET_DISTANCE;
  offset[AXIS_Z] = cosf(yaw) * FLYOVER_INSET_DISTANCE;
  g_Cameras[INSET_CAMERA].view.eye[AXIS_X] =
      g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] +
      (offset[AXIS_X] * COURSE_TO_WORLD_SCALE);
  g_Cameras[INSET_CAMERA].view.eye[AXIS_Y] =
      g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] - FLYOVER_INSET_EYE_RISE;
  g_Cameras[INSET_CAMERA].view.eye[AXIS_Z] =
      g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] +
      (offset[AXIS_Z] * COURSE_TO_WORLD_SCALE);
  return ++g_HoleFlyoverFrame == HOLE_FLYOVER_FRAME_COUNT;
}

/* Turns the camera's yaw to face the look-at point, unless the camera is over
 * it */
void AimCameraYaw(void) {
  f32 offsetX;
  f32 offsetZ;

  offsetX = f_LookAtPoint[AXIS_X] - f_CameraPosition[AXIS_X];
  offsetZ = f_LookAtPoint[AXIS_Z] - f_CameraPosition[AXIS_Z];
  if ((s32)offsetX != 0 || (s32)offsetZ != 0) {
    f_CameraYaw = M_PI - atan2f(offsetX, offsetZ);
  }
}

void PlaceAimCamera(void);

/* PlaceFlightCamera's distances are in course units unless they say otherwise;
 * y points down */
#define CUP_FRAMING_PITCH (-0.14f)
/* Above the cup, of both framed spheres */
#define CUP_FRAMING_RISE 0xC00
#define CUP_FRAMING_RADIUS 3.0f
#define CUP_FRAMING_DEPTH_OFFSET (-1.5f)
#define PATH_FRAMING_PITCH (-0.7853982f)
#define PATH_FRAMING_RADIUS (-15.0f)
#define PATH_FRAMING_DEPTH_OFFSET (-150.0f)
#define LAUNCH_VIEW_STEP 11
#define LAUNCH_VIEW_MIN_RISE 0x6600
/* In world units */
#define CHASE_DISTANCE 15.0f
#define CHASE_FIELD_OF_VIEW 80.0f
/* How many path steps behind the ball the camera follows */
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
/* In world units */
#define BEHIND_LOOK_AT_RISE 4.5f
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
#define FACING_CUP_VIEW_DISTANCE 76800.0f
#define FACING_CUP_VIEW_RISE 0x5A00
#define FACING_CUP_EASE_DEAD_ZONE 0.5f
#define FACING_CUP_EASE_FRACTION 0.1f
#define BILLBOARD_HIT_VIEW_DISTANCE 460800.0f
#define BILLBOARD_HIT_VIEW_RISE 0x4B000
/* While f_EarlierFlightCount is below this, the circle starts at angle 0 rather
 * than at a side */
#define CIRCLE_START_FLIGHT_COUNT 2
#define CIRCLE_ANGLE_STEP DegreesToRadians(2)
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
/* In world units */
#define BESIDE_GOLFER_CLEARANCE 22.5f
#define BESIDE_GOLFER_LOOK_AT_RISE 12288.0f
#define BESIDE_GOLFER_FLIGHT_FIELD_OF_VIEW 80.0f
#define MAX_EYE_DEPTH_BELOW_GROUND 0x21C0000

/*
 * Places the flight camera's eye and look-at point for f_FlightCameraKind at a
 * step of the ball's flight, then keeps the eye above the ground and over the
 * course. The kinds that test f_IsCameraKindNew do their setup once when it is
 * set, then clear it. Doesn't match: the original allocates some registers
 * differently, and orders the stores of point for FLIGHT_CAMERA_FACING_CUP
 * differently. The register oracle forced 13 pseudos and still left 22 words
 * differing when the draft gave 98. A permuter output with 96 words (2 in
 * shape) got both from a temp that it sets to point's Z only in unreachable
 * code before a case label, and then reads instead of point's Z in
 * FLIGHT_CAMERA_FACING_CUP's Z ease, so the value it reads is never set.
 * Keeping the scaled Z in a local for that ease changed nothing, and scaling Z
 * after the Y ease gave 148 (12). A focused run's best (65 words, 3 in shape)
 * reads point's Z before it is set again, but the original subtracts the
 * camera's Z from the newly scaled Z, so that changes behaviour.
 * FLIGHT_CAMERA_CIRCLING_BALL sets the field of view after the look-at point,
 * as that run also did, which took the draft from 98 words differing to 66, 4
 * in shape either way; set after the eye's height, as the draft had it, or
 * anywhere before the look-at point's Y, it gives 98 to 138. 66 words differ, 4
 * in shape.
 */
#ifdef NON_MATCHING
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
  f32 delta;
  f32 riseDistance;
  Radians angle;
  s32 index;
  s32 lowestHeight;

  switch (f_FlightCameraKind) {
    case FLIGHT_CAMERA_FRAMING_CUP:
      ClearFramedSpheres();
      SetFramingAngles(CUP_FRAMING_PITCH, -f_StartYawFromCup - (M_PI / 6));
      cupSphere[AXIS_X] =
          (f32)g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
      cupSphere[AXIS_Z] =
          (f32)g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
      cupSphere[AXIS_Y] =
          (f32)(g_HoleSetup.cupPosition[AXIS_Y] - CUP_FRAMING_RISE) *
          COURSE_TO_WORLD_SCALE;
      AddFramedSphere(cupSphere, CUP_FRAMING_RADIUS);
      cupSphere[AXIS_X] =
          (f32)g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
      cupSphere[AXIS_Z] =
          (f32)g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
      cupSphere[AXIS_Y] =
          (f32)(g_HoleSetup.cupPosition[AXIS_Y] - CUP_FRAMING_RISE) *
          COURSE_TO_WORLD_SCALE;
      AddFramedSphere(cupSphere, CUP_FRAMING_RADIUS);
      FrameSpheres(CUP_FRAMING_DEPTH_OFFSET);
      GetFramedView(f_CameraPosition, f_LookAtPoint);
      break;
    case FLIGHT_CAMERA_FRAMING_PATH:
      ClearFramedSpheres();
      SetFramingAngles(PATH_FRAMING_PITCH, 0.0f);
      pathSphere[AXIS_X] =
          (f32)g_SimulatedShot
              .path[g_SimulatedShot.minPositionStep[AXIS_X]][AXIS_X] *
          COURSE_TO_WORLD_SCALE;
      pathSphere[AXIS_Y] =
          (f32)g_SimulatedShot
              .path[g_SimulatedShot.minPositionStep[AXIS_X]][AXIS_Y] *
          COURSE_TO_WORLD_SCALE;
      pathSphere[AXIS_Z] =
          (f32)g_SimulatedShot
              .path[g_SimulatedShot.minPositionStep[AXIS_X]][AXIS_Z] *
          COURSE_TO_WORLD_SCALE;
      AddFramedSphere(pathSphere, PATH_FRAMING_RADIUS);
      pathSphere[AXIS_X] =
          (f32)g_SimulatedShot
              .path[g_SimulatedShot.minPositionStep[AXIS_Z]][AXIS_X] *
          COURSE_TO_WORLD_SCALE;
      pathSphere[AXIS_Y] =
          (f32)g_SimulatedShot
              .path[g_SimulatedShot.minPositionStep[AXIS_Z]][AXIS_Y] *
          COURSE_TO_WORLD_SCALE;
      pathSphere[AXIS_Z] =
          (f32)g_SimulatedShot
              .path[g_SimulatedShot.minPositionStep[AXIS_Z]][AXIS_Z] *
          COURSE_TO_WORLD_SCALE;
      AddFramedSphere(pathSphere, PATH_FRAMING_RADIUS);
      pathSphere[AXIS_X] =
          (f32)g_SimulatedShot
              .path[g_SimulatedShot.maxPositionStep[AXIS_X]][AXIS_X] *
          COURSE_TO_WORLD_SCALE;
      pathSphere[AXIS_Y] =
          (f32)g_SimulatedShot
              .path[g_SimulatedShot.maxPositionStep[AXIS_X]][AXIS_Y] *
          COURSE_TO_WORLD_SCALE;
      pathSphere[AXIS_Z] =
          (f32)g_SimulatedShot
              .path[g_SimulatedShot.maxPositionStep[AXIS_X]][AXIS_Z] *
          COURSE_TO_WORLD_SCALE;
      AddFramedSphere(pathSphere, PATH_FRAMING_RADIUS);
      pathSphere[AXIS_X] =
          (f32)g_SimulatedShot
              .path[g_SimulatedShot.maxPositionStep[AXIS_Z]][AXIS_X] *
          COURSE_TO_WORLD_SCALE;
      pathSphere[AXIS_Y] =
          (f32)g_SimulatedShot
              .path[g_SimulatedShot.maxPositionStep[AXIS_Z]][AXIS_Y] *
          COURSE_TO_WORLD_SCALE;
      pathSphere[AXIS_Z] =
          (f32)g_SimulatedShot
              .path[g_SimulatedShot.maxPositionStep[AXIS_Z]][AXIS_Z] *
          COURSE_TO_WORLD_SCALE;
      AddFramedSphere(pathSphere, PATH_FRAMING_RADIUS);
      FrameSpheres(PATH_FRAMING_DEPTH_OFFSET);
      GetFramedView(f_CameraPosition, f_LookAtPoint);
      break;
    case FLIGHT_CAMERA_NEAR_LAUNCH:
      if (f_IsCameraKindNew == TRUE) {
        f_IsCameraKindNew = FALSE;
        f_CameraPosition[AXIS_X] =
            g_SimulatedShot.path[LAUNCH_VIEW_STEP][AXIS_X];
        f_CameraPosition[AXIS_Y] =
            g_SimulatedShot.path[LAUNCH_VIEW_STEP][AXIS_Y];
        f_CameraPosition[AXIS_Z] =
            g_SimulatedShot.path[LAUNCH_VIEW_STEP][AXIS_Z];
        height = GetGroundHeight(
                     f_CameraPosition[AXIS_X], f_CameraPosition[AXIS_Z]) -
            LAUNCH_VIEW_MIN_RISE;
        if (height < f_CameraPosition[AXIS_Y]) {
          f_CameraPosition[AXIS_Y] = height;
        }

        f_CameraPosition[AXIS_X] *= COURSE_TO_WORLD_SCALE;
        f_CameraPosition[AXIS_Y] *= COURSE_TO_WORLD_SCALE;
        f_CameraPosition[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
        f_LookAtPoint[AXIS_X] =
            (f32)f_FlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
        f_LookAtPoint[AXIS_Y] =
            (f32)f_FlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        f_LookAtPoint[AXIS_Z] =
            (f32)f_FlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
      }

      break;
    case FLIGHT_CAMERA_CHASING_BALL: {
      /* Its own, as the original keeps it in a temporary register rather
       * than distance's saved one; sharing distance gives 184 differing
       * words, 8 in shape */
      f32 scale;

      /* Back along the path, level with the ball */
      position[AXIS_X] = g_SimulatedShot.path[step - 1][AXIS_X] -
          (f32)f_FlightBallPosition[AXIS_X];
      position[AXIS_Y] = 0.0f;
      position[AXIS_Z] = g_SimulatedShot.path[step - 1][AXIS_Z] -
          (f32)f_FlightBallPosition[AXIS_Z];
      scale = CHASE_DISTANCE /
          sqrtf((position[AXIS_X] * position[AXIS_X]) +
              (position[AXIS_Y] * position[AXIS_Y]) +
              (position[AXIS_Z] * position[AXIS_Z]));
      position[AXIS_X] *= scale;
      position[AXIS_Y] *= scale;
      position[AXIS_Z] *= scale;
      f_LookAtPoint[AXIS_X] =
          (f_FlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE) -
          position[AXIS_X];
      f_LookAtPoint[AXIS_Y] =
          f_FlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
      f_LookAtPoint[AXIS_Z] =
          (f_FlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE) -
          position[AXIS_Z];
      f_CameraPosition[AXIS_X] =
          (f_FlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE) +
          position[AXIS_X];
      f_CameraPosition[AXIS_Y] =
          f_FlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
      f_CameraPosition[AXIS_Z] =
          (f_FlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE) +
          position[AXIS_Z];
      g_Cameras[0].view.fieldOfView = CHASE_FIELD_OF_VIEW;
      break;
    }
    case FLIGHT_CAMERA_FOLLOWING_PATH:
      f_CameraPosition[AXIS_X] =
          (f32)g_SimulatedShot.path[step - FOLLOW_STEP_COUNT][AXIS_X] *
          COURSE_TO_WORLD_SCALE;
      f_CameraPosition[AXIS_Y] =
          (f32)(g_SimulatedShot.path[step - FOLLOW_STEP_COUNT][AXIS_Y] -
              FOLLOW_RISE) *
          COURSE_TO_WORLD_SCALE;
      f_CameraPosition[AXIS_Z] =
          (f32)g_SimulatedShot.path[step - FOLLOW_STEP_COUNT][AXIS_Z] *
          COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.fieldOfView = FOLLOW_FIELD_OF_VIEW;
      EaseLookAtFlyingBall();
      break;
    case FLIGHT_CAMERA_UNK10:
    case FLIGHT_CAMERA_UNK11:
      EaseLookAtFlyingBall();
      break;
    case FLIGHT_CAMERA_BESIDE_TEE:
      if (f_IsCameraKindNew == TRUE) {
        f_IsCameraKindNew = FALSE;
        yaw = ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) -
            TEE_VIEW_YAW_OFFSET;
        point[AXIS_X] =
            (cosf(yaw) * TEE_VIEW_DISTANCE) + f_ShotStartPosition[AXIS_X];
        point[AXIS_Z] =
            (sinf(yaw) * TEE_VIEW_DISTANCE) + f_ShotStartPosition[AXIS_Z];
        point[AXIS_Y] =
            GetGroundHeight(point[AXIS_X], point[AXIS_Z]) - TEE_VIEW_RISE;
        point[AXIS_X] *= COURSE_TO_WORLD_SCALE;
        point[AXIS_Y] *= COURSE_TO_WORLD_SCALE;
        point[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
        f_CameraPosition[AXIS_X] = point[AXIS_X];
        f_CameraPosition[AXIS_Y] = point[AXIS_Y];
        f_CameraPosition[AXIS_Z] = point[AXIS_Z];
        f_LookAtPoint[AXIS_X] =
            (f32)f_ShotStartPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
        f_LookAtPoint[AXIS_Y] =
            (f32)(f_ShotStartPosition[AXIS_Y] - TEE_LOOK_AT_RISE) *
            COURSE_TO_WORLD_SCALE;
        f_LookAtPoint[AXIS_Z] =
            (f32)f_ShotStartPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        AimCameraYaw();
      }

      g_Cameras[0].view.fieldOfView = TEE_VIEW_FIELD_OF_VIEW;
      break;
    case FLIGHT_CAMERA_BEHIND_START:
      if (f_IsCameraKindNew == TRUE) {
        /* Its own, as the original allocates these within the branch */
        Radians sideYaw;
        f32 sideDistance;

        f_IsCameraKindNew = FALSE;
        f_IsBehindViewFar ^= TRUE;
        yaw = f_ShotFlightYaw + M_PI;
        g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
        if (f_IsBehindViewFar == FALSE) {
          sideYaw = f_ShotFlightYaw + (M_PI / 4) +
              ((f32)(rand() & RANDOM_YAW_MASK) * (M_PI / 65536));
          distance = (f_ShotLength * NEAR_BEHIND_VIEW_LENGTH_FRACTION) +
              NEAR_BEHIND_VIEW_DISTANCE -
              RandomBelow(f_ShotLength * BEHIND_VIEW_SPREAD_FRACTION);
        } else {
          sideYaw = f_ShotFlightYaw - (M_PI / 4) -
              ((f32)(rand() & RANDOM_YAW_MASK) * (M_PI / 65536));
          distance = (f_ShotLength * FAR_BEHIND_VIEW_LENGTH_FRACTION) +
              FAR_BEHIND_VIEW_DISTANCE -
              RandomBelow(f_ShotLength * BEHIND_VIEW_SPREAD_FRACTION);
        }

        sideDistance =
            RandomBelow(f_ShotLength * BEHIND_VIEW_SIDE_SPREAD_FRACTION) +
            BEHIND_VIEW_SIDE_DISTANCE;
        point[AXIS_X] = f_ShotStartPosition[AXIS_X] + (sinf(yaw) * distance) +
            (sinf(sideYaw) * sideDistance);
        point[AXIS_Z] = f_ShotStartPosition[AXIS_Z] - (cosf(yaw) * distance) -
            (cosf(sideYaw) * sideDistance);
        point[AXIS_Y] =
            GetGroundHeight(point[AXIS_X], point[AXIS_Z]) - BEHIND_VIEW_RISE;
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
          /* Beside the path's highest point */
          sideYaw = f_ShotFlightYaw - (M_PI / 2) +
              ((f32)(rand() & RANDOM_YAW_MASK) * RANDOM_YAW_TO_RADIANS);
          f_IsApexViewFar ^= TRUE;
          if (f_IsApexViewFar == FALSE) {
            /* RandomBelow(1) is always 0; the original has it */
            point[AXIS_X] =
                g_SimulatedShot
                    .path[g_SimulatedShot.minPositionStep[AXIS_Y]][AXIS_X] +
                (2.0f * (sinf(sideYaw) * APEX_VIEW_NEAR_HALF_DISTANCE));
            point[AXIS_Y] =
                g_SimulatedShot
                    .path[g_SimulatedShot.minPositionStep[AXIS_Y]][AXIS_Y] +
                (RandomBelow(1) * APEX_VIEW_RISE_STEP);
            point[AXIS_Z] =
                g_SimulatedShot
                    .path[g_SimulatedShot.minPositionStep[AXIS_Y]][AXIS_Z] -
                (2.0f * (cosf(sideYaw) * APEX_VIEW_NEAR_HALF_DISTANCE));
          } else {
            point[AXIS_X] =
                g_SimulatedShot
                    .path[g_SimulatedShot.minPositionStep[AXIS_Y]][AXIS_X] +
                (sinf(sideYaw) * APEX_VIEW_FAR_DISTANCE);
            point[AXIS_Y] =
                g_SimulatedShot
                    .path[g_SimulatedShot.minPositionStep[AXIS_Y]][AXIS_Y] +
                (RandomBelow(APEX_VIEW_FAR_RISE_STEP_COUNT) *
                    APEX_VIEW_RISE_STEP);
            point[AXIS_Z] =
                g_SimulatedShot
                    .path[g_SimulatedShot.minPositionStep[AXIS_Y]][AXIS_Z] -
                (cosf(sideYaw) * APEX_VIEW_FAR_DISTANCE);
          }

          height = GetGroundHeight(point[AXIS_X], point[AXIS_Z]) -
              APEX_VIEW_MIN_RISE;
          if (height < point[AXIS_Y]) {
            point[AXIS_Y] = height;
          }
        } else {
          f32 sideDistance;

          /* Off to the side of the flight, ahead of the start */
          sideDistance = f_ShotLength * SIDE_VIEW_LENGTH_FRACTION;
          distance = f_ShotLength * SIDE_VIEW_AHEAD_FRACTION;

          sideYaw = f_ShotFlightYaw + (M_PI / 2) +
              ((f32)(rand() & RANDOM_YAW_MASK) * RANDOM_YAW_TO_RADIANS);
          g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
          yaw = f_ShotFlightYaw + M_PI;
          sideDistance -=
              RandomBelow(SIDE_VIEW_SPREAD_STEP_COUNT) * SIDE_VIEW_SPREAD_STEP;
          if (sideDistance > SIDE_VIEW_MAX_DISTANCE) {
            sideDistance = SIDE_VIEW_MAX_DISTANCE;
          }

          if (sideDistance < SIDE_VIEW_MIN_DISTANCE) {
            sideDistance = SIDE_VIEW_MIN_DISTANCE;
          }

          point[AXIS_X] = f_ShotStartPosition[AXIS_X] + (sinf(yaw) * distance) +
              (sinf(sideYaw) * sideDistance);
          point[AXIS_Z] = f_ShotStartPosition[AXIS_Z] - (cosf(yaw) * distance) -
              (cosf(sideYaw) * sideDistance);
          /*
           * Computing extraRise before GetGroundHeight is there to match: after
           * it gives 123 words differing (21 in shape), and in the branch 127
           * (15), against the draft's 98 (4)
           */
          extraRise = f_ShotLength * SIDE_VIEW_RISE_FRACTION;
          point[AXIS_Y] = GetGroundHeight(point[AXIS_X], point[AXIS_Z]);
          if (!g_SimulatedShot.startsOnGreen) {
            point[AXIS_Y] -= extraRise + SIDE_VIEW_RISE;
          }

          height = g_SimulatedShot
                       .path[g_SimulatedShot.minPositionStep[AXIS_Y]][AXIS_Y] +
              SIDE_VIEW_MAX_DROP_BELOW_APEX;
          if (height < point[AXIS_Y]) {
            point[AXIS_Y] = height;
          }

          height = f_ShotEndPosition[AXIS_Y] - SIDE_VIEW_MIN_RISE_ABOVE_END;
          if (height < point[AXIS_Y]) {
            point[AXIS_Y] = height;
          }

          height = GetGroundHeight(point[AXIS_X], point[AXIS_Z]) -
              SIDE_VIEW_MIN_RISE;
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

      g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
      EaseLookAtFlyingBall();
      break;
    case FLIGHT_CAMERA_TRAILING_BALL:
      if (f_IsCameraKindNew == TRUE) {
        SnapLookAtPoint();
        f_IsCameraKindNew = FALSE;
        AimCameraYaw();
        f_IsTrailViewClose ^= TRUE;
      }

      g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
      if (f_IsTrailViewClose == FALSE) {
        if (f_ShotLength <= SHORT_SHOT_MAX_LENGTH) {
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

      /*
       * Converting step through easeFraction is there to match: multiplying
       * step itself, with or without a cast, gives 104 words differing (4 in
       * shape) against the draft's 98 (4)
       */
      easeFraction = step;
      easeFraction =
          TRAIL_EASE_START_PERCENT - (TRAIL_EASE_PERCENT_STEP * easeFraction);
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

      if (angle > (2 * M_PI) - (M_PI / 6)) {
        angle = (2 * M_PI) - (M_PI / 6);
      }

      if (f_ShotLength <= SHORT_SHOT_MAX_LENGTH) {
        angle = ((angle - M_PI) * TRAIL_ANGLE_SCALE) - M_PI;
      } else {
        angle = (angle - M_PI) * TRAIL_ANGLE_SCALE;
      }

      position[AXIS_X] = ((-sinf(f_ShotFlightYaw + angle) * sideDistance) +
                             f_FlightBallPosition[AXIS_X]) *
          COURSE_TO_WORLD_SCALE;
      position[AXIS_Z] = ((cosf(f_ShotFlightYaw + angle) * sideDistance) +
                             f_FlightBallPosition[AXIS_Z]) *
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
      g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
      point[AXIS_X] = (-sinf(yaw) * FACING_CUP_VIEW_DISTANCE) +
          f_FlightBallPosition[AXIS_X];
      point[AXIS_Z] =
          (cosf(yaw) * FACING_CUP_VIEW_DISTANCE) + f_FlightBallPosition[AXIS_Z];
      point[AXIS_Y] =
          GetGroundHeight(point[AXIS_X], point[AXIS_Z]) - FACING_CUP_VIEW_RISE;
      height = f_ShotEndPosition[AXIS_Y] - FACING_CUP_VIEW_RISE;
      if (height < point[AXIS_Y]) {
        point[AXIS_Y] = height;
      }

      point[AXIS_X] *= COURSE_TO_WORLD_SCALE;
      point[AXIS_Y] *= COURSE_TO_WORLD_SCALE;
      point[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
      delta = point[AXIS_X] - f_CameraPosition[AXIS_X];
      /* The x test in a block of its own is there to match; without it 138
       * words differ, 16 in shape, and with only the block ended before it
       * 104, 6 in shape. Each axis's test in its own block gives the same as
       * this, the delta step in such a block too 97, 11 in shape, AbsFloat
       * 93, 11 in shape, fabs 141 and the comparison spelled as two tests
       * 151 */
      IN_OWN_BLOCK(if (fabsf(delta) > FACING_CUP_EASE_DEAD_ZONE) {
        f_CameraPosition[AXIS_X] += delta * FACING_CUP_EASE_FRACTION;
      });
      delta = point[AXIS_Y] - f_CameraPosition[AXIS_Y];
      if (fabsf(delta) > FACING_CUP_EASE_DEAD_ZONE) {
        f_CameraPosition[AXIS_Y] += delta * FACING_CUP_EASE_FRACTION;
      }

      delta = point[AXIS_Z] - f_CameraPosition[AXIS_Z];
      if (fabsf(delta) > FACING_CUP_EASE_DEAD_ZONE) {
        f_CameraPosition[AXIS_Z] += delta * FACING_CUP_EASE_FRACTION;
      }

      EaseLookAtPoint(f_EndYawFromCup);
      break;
    case FLIGHT_CAMERA_BILLBOARD_HIT:
      if (f_BillboardHitStep == 0) {
        f_BillboardHitStep = 1;
      }

      f_BillboardHitYaw =
          -atan2((f32)(g_SimulatedShot.path[f_BillboardHitStep][AXIS_X] -
                     g_SimulatedShot.path[f_BillboardHitStep - 1][AXIS_X]),
              (f32)(g_SimulatedShot.path[f_BillboardHitStep][AXIS_Z] -
                  g_SimulatedShot.path[f_BillboardHitStep - 1][AXIS_Z]));
      point[AXIS_X] = (-sinf(f_BillboardHitYaw) * BILLBOARD_HIT_VIEW_DISTANCE) +
          g_SimulatedShot.path[f_BillboardHitStep][AXIS_X];
      point[AXIS_Z] = (cosf(f_BillboardHitYaw) * BILLBOARD_HIT_VIEW_DISTANCE) +
          g_SimulatedShot.path[f_BillboardHitStep][AXIS_Z];
      point[AXIS_Y] = GetGroundHeight(point[AXIS_X], point[AXIS_Z]) -
          BILLBOARD_HIT_VIEW_RISE;
      f_CameraPosition[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
      f_CameraPosition[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
      f_CameraPosition[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
      EaseLookAtFlyingBall();
      g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
      break;
    case FLIGHT_CAMERA_AIM_VIEW:
      if (f_IsCameraKindNew == TRUE) {
        f_IsCameraKindNew = FALSE;
        PlaceAimCamera();
        f_CameraPosition[AXIS_X] = g_Cameras[0].view.eye[AXIS_X];
        f_CameraPosition[AXIS_Y] = g_Cameras[0].view.eye[AXIS_Y];
        f_CameraPosition[AXIS_Z] = g_Cameras[0].view.eye[AXIS_Z];
        f_LookAtPoint[AXIS_X] = g_Cameras[0].view.lookAt[AXIS_X];
        f_LookAtPoint[AXIS_Y] = g_Cameras[0].view.lookAt[AXIS_Y];
        f_LookAtPoint[AXIS_Z] = g_Cameras[0].view.lookAt[AXIS_Z];
        AimCameraYaw();
      }

      break;
    case FLIGHT_CAMERA_CIRCLING_BALL:
      if (f_IsCameraKindNew == TRUE) {
        f_IsCameraKindNew = FALSE;
        if (f_EarlierFlightCount < CIRCLE_START_FLIGHT_COUNT) {
          f_CircleAngle = 0.0f;
        } else if (g_Players[g_CurrentShot.playerIndex].unk22 == TRUE) {
          f_CircleAngle = M_PI / 2;
        } else {
          f_CircleAngle = -M_PI / 2;
        }
      }

      if (g_Players[g_CurrentShot.playerIndex].unk22 == 1) {
        f_CircleAngle += CIRCLE_ANGLE_STEP;
        if (f_CircleAngle > M_PI) {
          f_CircleAngle = M_PI;
        }
      } else {
        f_CircleAngle -= CIRCLE_ANGLE_STEP;
        if (f_CircleAngle < -M_PI) {
          f_CircleAngle = -M_PI;
        }
      }

      circleYaw = ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) +
          f_CircleAngle;
      f_CameraPosition[AXIS_X] = (cosf(circleYaw) * CIRCLE_DISTANCE) +
          g_CurrentShot.ballPosition[AXIS_X];
      f_CameraPosition[AXIS_Z] = (sinf(circleYaw) * CIRCLE_DISTANCE) +
          g_CurrentShot.ballPosition[AXIS_Z];
      f_CameraPosition[AXIS_Y] =
          GetGroundHeight(f_CameraPosition[AXIS_X], f_CameraPosition[AXIS_Z]) -
          CIRCLE_RISE;
      f_CameraPosition[AXIS_X] *= COURSE_TO_WORLD_SCALE;
      f_CameraPosition[AXIS_Y] *= COURSE_TO_WORLD_SCALE;
      f_CameraPosition[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
      f_LookAtPoint[AXIS_X] =
          (f32)g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
      f_LookAtPoint[AXIS_Y] =
          ((f32)g_CurrentShot.ballPosition[AXIS_Y] - CIRCLE_LOOK_AT_RISE) *
          COURSE_TO_WORLD_SCALE;
      f_LookAtPoint[AXIS_Z] =
          (f32)g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
      break;
    case FLIGHT_CAMERA_BESIDE_GOLFER:
      if (f_IsCameraKindNew == TRUE) {
        g_Cameras[0].view.fieldOfView = BESIDE_GOLFER_FIELD_OF_VIEW;
        GetCharacterPointPosition(0, 1, position);
        f_IsCameraKindNew = FALSE;
        if (g_Players[g_CurrentShot.playerIndex].unk22 == TRUE) {
          yaw = ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) +
              (M_PI / 2);
        } else {
          yaw = ((u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS) -
              (M_PI / 2);
        }

        for (index = BESIDE_GOLFER_FIRST_STEP; index < BESIDE_GOLFER_STEP_LIMIT;
            index++) {
          /* Setting distance inside the x term comes closer to the original:
           * a statement of its own before it gives 101 differing words, not
           * 98, and 9 in shape, not 4 */
          f_CameraPosition[AXIS_X] =
              (cosf(yaw) * (distance = index * BESIDE_GOLFER_STEP)) +
              g_CurrentShot.ballPosition[AXIS_X];
          f_CameraPosition[AXIS_Z] =
              (sinf(yaw) * distance) + g_CurrentShot.ballPosition[AXIS_Z];
          f_CameraPosition[AXIS_Y] = GetGroundHeight(f_CameraPosition[AXIS_X],
                                         f_CameraPosition[AXIS_Z]) -
              BESIDE_GOLFER_RISE;
          if (g_CurrentShot.ballPosition[AXIS_Y] - BESIDE_GOLFER_MIN_RISE <
              f_CameraPosition[AXIS_Y]) {
            f_CameraPosition[AXIS_Y] =
                g_CurrentShot.ballPosition[AXIS_Y] - BESIDE_GOLFER_MIN_RISE;
          }

          f_CameraPosition[AXIS_X] *= COURSE_TO_WORLD_SCALE;
          f_CameraPosition[AXIS_Y] *= COURSE_TO_WORLD_SCALE;
          f_CameraPosition[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
          if (Length2D(f_CameraPosition[AXIS_X] - position[AXIS_X],
                  f_CameraPosition[AXIS_Z] - position[AXIS_Z]) >
              BESIDE_GOLFER_CLEARANCE) {
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

      g_Cameras[0].view.fieldOfView = BESIDE_GOLFER_FLIGHT_FIELD_OF_VIEW;
      break;
  }

  position[AXIS_X] = f_CameraPosition[AXIS_X] * WORLD_TO_COURSE_SCALE;
  position[AXIS_Y] = f_CameraPosition[AXIS_Y] * WORLD_TO_COURSE_SCALE;
  position[AXIS_Z] = f_CameraPosition[AXIS_Z] * WORLD_TO_COURSE_SCALE;
  lowestHeight = GetGroundHeight(position[AXIS_X], position[AXIS_Z]) +
      MAX_EYE_DEPTH_BELOW_GROUND;
  if (f_FlightCameraKind == FLIGHT_CAMERA_CIRCLING_BALL) {
    if (lowestHeight < position[AXIS_Y]) {
      position[AXIS_Y] = lowestHeight;
    }

    height = f_ShotStartPosition[AXIS_Y] - CIRCLE_MIN_RISE_ABOVE_START;
    if (height < position[AXIS_Y]) {
      position[AXIS_Y] = height;
    }
  } else if (f_FlightCameraKind != FLIGHT_CAMERA_BESIDE_GOLFER &&
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

  f_CameraPosition[AXIS_X] = position[AXIS_X] * COURSE_TO_WORLD_SCALE;
  f_CameraPosition[AXIS_Y] = position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
  f_CameraPosition[AXIS_Z] = position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
}
#else
INCLUDE_ASM("asm/nonmatchings/shot_camera", PlaceFlightCamera);
#endif

#define LONG_SHOT_MIN_LENGTH 0x1C2000
#define MID_SHOT_MIN_LENGTH 0x4B001
#define NEAR_CUP_DISTANCE 0x1E00
#define CUP_LANDING_MIN_DISTANCE 0x1201
#define CUP_LANDING_MAX_DISTANCE 0x4AFFF
#define DEBUG_CAMERA_CONTROLLER 1

/* It is a macro, as an inline function compiles differently from the original
 */
#define IsTroubleSurface(info) \
  (IsBunkerKind((info)->kind) || (info)->kind == SURFACE_KIND_WATER_HAZARD || \
      (info)->kind == SURFACE_KIND_OUT_OF_BOUNDS)

/*
 * Picks a flight camera kind for the value of f_FlightPhase, and switches to it
 * if it is greater than the current kind. The picks depend on the simulated
 * shot, save option 3 and chance. A switch ends StartHudAnimation's effect and
 * may start another one. In debug builds, the second controller's B and A
 * buttons pick FLIGHT_CAMERA_BESIDE_GOLFER and FLIGHT_CAMERA_CIRCLING_BALL.
 *
 * Doesn't match: cross-jumping sends the second switch's case 5 trouble-surface
 * branch through the identical "kind = FLIGHT_CAMERA_AROUND_END" tail of the
 * first switch's case 3, where the original branches straight to the end. The
 * other instructions are the same. The register oracle's closest result is
 * these 2 words with no register forced, so allocation isn't the cause. 2 words
 * differ.
 *
 * The original cross-jumps the same way: its jump table sends the mini-golf
 * pick's cases 3 and 4 to the shared store and jump. The difference is in
 * reorg. relax_delay_slots moves a branch past a redundant store at its target.
 * For the later tests, redundant_insn finds the store in the first test's delay
 * slot, but for the first test it starts scanning before the branch, which
 * leaves out its own delay slot, and stops at the case label. Breaking out of
 * the case on a trouble surface, alone or after its store, testing the
 * following-path kind in a second if, an if/else storing in each arm, a ?: of
 * the two equal kinds, and storing after the if give 2 to 35 words.
 */
#ifdef NON_MATCHING
void ChooseFlightCameraKind(void) {
  /* Pads the frame to match; without it 18 words differ, against 2 */
  f32 unusedValues[14] UNUSED;
  s32 animation;
  s32 kind;
  s32 pick;

  animation = HUD_ANIMATION_NONE;
  f_HasFlightPhaseChanged = FALSE;
  kind = 0;
  if (g_SimulatedShot.startsOnGreen) {
    switch (f_FlightPhase) {
      case FLIGHT_PHASE_LAUNCH:
        if (!(f_FlightCameraFlags & FLIGHT_CAMERA_FLAG_UNK2) &&
            RandomBelow(2) == 0 && !IsPinInCup() &&
            f_StartCupDistance >= CUP_LANDING_MIN_DISTANCE &&
            f_StartCupDistance <= CUP_LANDING_MAX_DISTANCE &&
            (g_SimulatedShot.endsInCup ||
                f_LandingCupDistance < NEAR_CUP_DISTANCE)) {
          kind = FLIGHT_CAMERA_FRAMING_CUP;
        } else if (g_GameState.isMiniGolf) {
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
        } else {
          pick = RandomBelow(6);
          if (pick < 3) {
            kind = FLIGHT_CAMERA_AIM_VIEW;
          } else if (pick == 3) {
            if ((f_ComingFlightEvents & FLIGHT_EVENT_CUP_PASS) ||
                g_SimulatedShot.stepCount > 120) {
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
  } else if (!IsSimpleCameraChosen()) {
    switch (f_FlightPhase) {
      case FLIGHT_PHASE_LAUNCH:
        pick = NextCameraPick(1);
        if (f_BillboardHitStep != NO_STEP) {
          kind = FLIGHT_CAMERA_AIM_VIEW;
        } else if (pick < 4) {
          kind = FLIGHT_CAMERA_AIM_VIEW;
        } else if (pick == 4) {
          if (f_ShotLength > SHORT_SHOT_MAX_LENGTH) {
            kind = FLIGHT_CAMERA_AIM_VIEW;
          } else {
            kind = FLIGHT_CAMERA_CIRCLING_BALL;
          }
        } else if (pick == 5) {
          kind = FLIGHT_CAMERA_AIM_VIEW;
          if (f_IsBallGroundLevel == TRUE && g_SimulatedShot.stepCount >= 61) {
            kind = FLIGHT_CAMERA_BESIDE_GOLFER;
          }
        } else if (pick == 6) {
          if (f_ShotLength <= SHORT_SHOT_MAX_LENGTH) {
            kind = FLIGHT_CAMERA_AROUND_END;
          } else {
            kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
          }
        } else if (pick == 7) {
          if (f_ShotLength <= SHORT_SHOT_MAX_LENGTH) {
            kind = FLIGHT_CAMERA_AROUND_END;
          } else {
            kind = FLIGHT_CAMERA_TRAILING_BALL;
          }
        } else if (pick == 8) {
          kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
        } else if (pick == 9) {
          kind = FLIGHT_CAMERA_AIM_VIEW;
          if (f_ShotLength >= MID_SHOT_MIN_LENGTH &&
              g_SimulatedShot.stepCount >= 16) {
            kind = FLIGHT_CAMERA_NEAR_LAUNCH;
          }
        }

        break;
      case FLIGHT_PHASE_EARLY_CLIMB:
        if (f_FlightCameraKind == FLIGHT_CAMERA_NEAR_LAUNCH) {
          animation = HUD_ANIMATION_TILES;
          kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
        } else if (f_FlightCameraKind == FLIGHT_CAMERA_AIM_VIEW) {
          if (RandomBelow(3) == 0) {
            kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
          }

          if (f_ShotLength >= LONG_SHOT_MIN_LENGTH && RandomBelow(4) == 0) {
            kind = FLIGHT_CAMERA_FOLLOWING_PATH;
          }
        } else if (f_FlightCameraKind == FLIGHT_CAMERA_BESIDE_GOLFER) {
          if (g_SimulatedShot.stepCount < 120) {
            kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
          } else if (g_SimulatedShot.usesPutter) {
            kind = FLIGHT_CAMERA_BEHIND_START;
          } else if (f_ShotLength < LONG_SHOT_MIN_LENGTH) {
            kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
          } else if (RandomBelow(4) == 0) {
            kind = FLIGHT_CAMERA_FOLLOWING_PATH;
          } else {
            kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
          }
        }

        break;
      case FLIGHT_PHASE_CLIMB:
        if (f_FlightCameraKind == FLIGHT_CAMERA_BESIDE_GOLFER) {
          if (g_SimulatedShot.stepCount < 120) {
            kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
          } else if (g_SimulatedShot.usesPutter) {
            kind = FLIGHT_CAMERA_SIDE_OF_FLIGHT;
          }
        } else if (f_FlightCameraKind != FLIGHT_CAMERA_TRAILING_BALL) {
          /*
           * The nested ifs and the separate cases are there to match: one &&
           * test gives 9 words differing (5 in shape), and shared cases 12
           * (10), against the draft's 2 (0)
           */
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
        if (f_CameraChangeHoldCount) {
          f_HasFlightPhaseChanged = TRUE;
        } else if (f_FlightCameraKind == FLIGHT_CAMERA_TRAILING_BALL) {
          /*
           * The empty branches are there to match: one && test of the other
           * kinds gives 16 words differing (10 in shape) against the draft's 2
           * (0)
           */
        } else if (f_FlightCameraKind == FLIGHT_CAMERA_FOLLOWING_PATH) {
        } else {
          if (f_LandingCupDistance <= SHORT_SHOT_MAX_LENGTH) {
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
      case FLIGHT_PHASE_NEARING_CONTACT:
        kind = FLIGHT_CAMERA_AROUND_END;

        /*
         * The repeated store is there to match: dropping the test, keeping
         * only its call, or leaving its branch empty gives 26 words
         * differing (22 in shape), and storing in both arms 19
         */
        if (IsTroubleSurface(f_EndSurfaceInfo) ||
            f_FlightCameraKind == FLIGHT_CAMERA_FOLLOWING_PATH) {
          kind = FLIGHT_CAMERA_AROUND_END;
        }

        break;
      case FLIGHT_PHASE_CONTACT:
        if (!f_CameraChangeHoldCount) {
          kind = FLIGHT_CAMERA_AROUND_END;
        } else {
          f_HasFlightPhaseChanged = TRUE;
        }

        break;
      case FLIGHT_PHASE_END:
        if (IsTroubleSurface(f_EndSurfaceInfo) || g_SimulatedShot.endsInCup) {
          /*
           * The empty branch is there to match: the inverted test gives 10
           * words differing (8 in shape) against the draft's 2 (0)
           */
        } else {
          if (f_CameraChangeHoldCount || g_SimulatedShot.stepCount < 21) {
            f_HasFlightPhaseChanged = TRUE;
          } else if (f_LandingCupDistance <= SHORT_SHOT_MAX_LENGTH ||
              f_FlightCameraKind == FLIGHT_CAMERA_AROUND_END) {
            kind = FLIGHT_CAMERA_FACING_CUP;
          }
        }

        break;
    }
  } else {
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
            if (f_ShotLength <= SHORT_SHOT_MAX_LENGTH) {
              kind = FLIGHT_CAMERA_AROUND_END;
            } else {
              kind = FLIGHT_CAMERA_AIM_VIEW;
            }

            break;
          case 6:
            kind = FLIGHT_CAMERA_AIM_VIEW;
            if (f_IsBallGroundLevel == TRUE &&
                g_SimulatedShot.stepCount >= 61) {
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
        /*
         * The empty case is there to match: without it 17 words differ (9 in
         * shape) against the draft's 2 (0)
         */
        break;
    }
  }

  if (f_FlightCameraKind < kind) {
    f_IsCameraKindNew = TRUE;
    StartHudAnimation(HUD_ANIMATION_NONE);
    if (animation == HUD_ANIMATION_NONE) {
      if (f_FlightPhase >= FLIGHT_PHASE_FALL &&
          !g_SimulatedShot.startsOnGreen &&
          f_ShotLength > SHORT_SHOT_MAX_LENGTH) {
        if (kind == FLIGHT_CAMERA_AROUND_END) {
          if (RandomBelow(16) == 0 &&
              f_FlightCameraKind == FLIGHT_CAMERA_SIDE_OF_FLIGHT) {
            StartHudAnimation(HUD_ANIMATION_TILES);
          } else if (RandomBelow(8) == 0) {
            StartHudAnimation(HUD_ANIMATION_FADE_OUT);
          } else if (RandomBelow(3) == 0) {
            StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
          }
        }
      }
    } else if (f_ShotLength > SHORT_SHOT_MAX_LENGTH) {
      StartHudAnimation(animation);
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
#else
INCLUDE_ASM("asm/nonmatchings/shot_camera", ChooseFlightCameraKind);
#endif

#define CUP_PASS_LOOKAHEAD_STEP_COUNT 30
#define PATH_FLAG_LOOKAHEAD_STEP_COUNT 60
/* The billboard hit view starts no sooner, and lasts this far past the hit */
#define BILLBOARD_HIT_VIEW_MIN_STEP 60
#define BILLBOARD_HIT_VIEW_STEP_COUNT 10
/* Past the billboard hit view, a ball this far from the end is viewed there */
#define END_VIEW_MIN_DISTANCE 0x25800
/* Fewer steps left than this leave no time for a new camera kind */
#define CAMERA_CHOICE_MIN_STEP_COUNT 40
/* The steps at which the launch phase, and the early climb, end */
#define LAUNCH_END_STEP 10
#define EARLY_CLIMB_END_STEP 15
/* A shot from the green climbs until this step */
#define GREEN_CLIMB_END_STEP 40
/* The fall nears contact once a step flag is this many steps ahead */
#define CONTACT_LOOKAHEAD_STEP_COUNT 40
/* The end phase covers the path's last steps */
#define END_PHASE_STEP_COUNT 10

/* Whether the ball has passed the billboard hit and its view */
#define IsPastBillboardHitView(step) \
  (f_BillboardHitStep != NO_STEP && \
      f_BillboardHitStep + BILLBOARD_HIT_VIEW_STEP_COUNT < (step))
/* Whether the flying ball is still far from where the shot ends */
#define IsFarFromShotEnd() \
  ((s32)IntLength3D(f_FlightBallPosition[AXIS_X] - f_ShotEndPosition[AXIS_X], \
       f_FlightBallPosition[AXIS_Y] - f_ShotEndPosition[AXIS_Y], \
       f_FlightBallPosition[AXIS_Z] - f_ShotEndPosition[AXIS_Z]) > \
      END_VIEW_MIN_DISTANCE)
/* Whether a new camera kind would be seen long enough, or the end is shown */
#define HasTimeForNewCamera() \
  (g_BallFlightStep + CAMERA_CHOICE_MIN_STEP_COUNT < \
          g_SimulatedShot.stepCount || \
      f_FlightPhase == FLIGHT_PHASE_END)
/* Whether neither the cup view nor a coming event holds the camera */
#define IsFlightCameraFree() (!g_IsCupViewShown && f_ComingFlightEvents == 0)
#define VERY_LONG_SHOT_MIN_LENGTH 0x2A3000
#define BALL_SOUND_MIN_SHOT_LENGTH 0x25800
/* Shot length per step of volume */
#define BALL_SOUND_VOLUME_LENGTH 0x7800
#define BALL_SOUND_MIN_VOLUME 10
#define BALL_SOUND_MAX_VOLUME 64
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
/* Keeps the eye from sitting straight above the point it looks at */
#define LOOK_AT_NUDGE 0.1f

/*
 * Updates the flight camera, the inset view and the ball's sounds for one frame
 * of the ball's flight, and advances g_BallFlightStep when isAdvancing is 1.
 * Once the flight has ended, it counts the landing wait and returns TRUE when
 * it is over. The START button clears g_RoundHud.isWindIndicatorShown through
 * g_RoundHud.unkD and g_RoundHud.unk13.
 */
s32 UpdateFlightCamera(Boolean isAdvancing, s32 buttons) {
  f32 point[3];
  s32 step;
  SoundVolume volume;
  s32 aheadStep;
  s32 waitFrame;
  s32* flightStep;

  if (!isAdvancing) {
    g_RoundHud.unk13 = 0;
  } else {
    g_RoundHud.unk13 = 1;
  }

  if (buttons & START_BUTTON) {
    g_RoundHud.unkC = 0;
    g_RoundHud.isWindIndicatorShown = FALSE;
    g_RoundHud.unk8 = 0;
    g_RoundHud.isLieIconShown = FALSE;
    g_RoundHud.isClubHeadIconShown = FALSE;
    g_RoundHud.unkB = 0;
    g_RoundHud.unkD = 0;
    g_RoundHud.unk13 = 0;
  } else {
    g_RoundHud.unkC = f_IsFlightHudShown;
    g_RoundHud.isWindIndicatorShown = f_IsFlightHudShown;
    g_RoundHud.unk8 = f_IsFlightHudShown;
    g_RoundHud.isLieIconShown = f_IsFlightHudShown;
    g_RoundHud.isClubHeadIconShown = f_IsFlightHudShown;
    g_RoundHud.unkB = f_IsFlightHudShown;
    g_RoundHud.unkD = f_IsFlightHudShown;
  }

  if (g_BallFlightStep >= PATH_STEP_COUNT) {
    step = PATH_STEP_COUNT - 1;
  } else {
    step = g_BallFlightStep;
  }

  if (step >= g_SimulatedShot.stepCount - 1) {
    step = g_SimulatedShot.stepCount - 1;
  }

  if (!isAdvancing) {
    f_FlightBallPosition[AXIS_X] = f_ShotStartPosition[AXIS_X];
    f_FlightBallPosition[AXIS_Y] = f_ShotStartPosition[AXIS_Y];
    f_FlightBallPosition[AXIS_Z] = f_ShotStartPosition[AXIS_Z];
  } else if (g_BallFlightStep == g_SimulatedShot.stepCount) {
    f_FlightBallPosition[AXIS_X] = g_SimulatedShot.endPosition[AXIS_X];
    f_FlightBallPosition[AXIS_Y] = g_SimulatedShot.endPosition[AXIS_Y];
    f_FlightBallPosition[AXIS_Z] = g_SimulatedShot.endPosition[AXIS_Z];
  } else {
    f_FlightBallPosition[AXIS_X] = g_SimulatedShot.path[step][AXIS_X];
    f_FlightBallPosition[AXIS_Y] = g_SimulatedShot.path[step][AXIS_Y];
    f_FlightBallPosition[AXIS_Z] = g_SimulatedShot.path[step][AXIS_Z];
  }

  f_BallCupDistance = IntLength3D(
      f_FlightBallPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
      f_FlightBallPosition[AXIS_Y] - g_HoleSetup.cupPosition[AXIS_Y],
      f_FlightBallPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]);
  f_BallCupGroundDistance = IntLength2D(
      f_FlightBallPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
      f_FlightBallPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]);
  if (g_SimulatedShot.startsOnGreen) {
    switch (f_FlightPhase) {
      case FLIGHT_PHASE_LAUNCH:
        if (isAdvancing == TRUE && step == LAUNCH_END_STEP &&
            g_BallFlightStep <
                g_SimulatedShot.stepCount - END_PHASE_STEP_COUNT) {
          f_FlightPhase = FLIGHT_PHASE_CLIMB;
          f_HasFlightPhaseChanged = TRUE;
        }

        break;
      case FLIGHT_PHASE_CLIMB:
        if (step == GREEN_CLIMB_END_STEP &&
            g_BallFlightStep <
                g_SimulatedShot.stepCount - END_PHASE_STEP_COUNT) {
          f_FlightPhase = FLIGHT_PHASE_CONTACT;
          f_HasFlightPhaseChanged = TRUE;
        }

        break;
      case FLIGHT_PHASE_CONTACT:
        if (g_SimulatedShot.stepCount - END_PHASE_STEP_COUNT <
            g_BallFlightStep) {
          f_FlightPhase = FLIGHT_PHASE_END;
          f_HasFlightPhaseChanged = TRUE;
        }

        break;
    }
  } else if (!IsSimpleCameraChosen()) {
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
        if (g_BallFlightStep > g_SimulatedShot.minPositionStep[AXIS_Y]) {
          f_FlightPhase = FLIGHT_PHASE_FALL;
          f_HasFlightPhaseChanged = TRUE;
        }

        break;
      case FLIGHT_PHASE_FALL:
        if (g_SimulatedShot.pathFlags[step + CONTACT_LOOKAHEAD_STEP_COUNT]) {
          f_FlightPhase = FLIGHT_PHASE_NEARING_CONTACT;
          f_HasFlightPhaseChanged = TRUE;
        }

        break;
      case FLIGHT_PHASE_NEARING_CONTACT:
        if (g_SimulatedShot.pathFlags[step]) {
          f_FlightPhase = FLIGHT_PHASE_CONTACT;
          f_HasFlightPhaseChanged = TRUE;
        }

        break;
      case FLIGHT_PHASE_CONTACT:
        if (g_SimulatedShot.stepCount - END_PHASE_STEP_COUNT <
            g_BallFlightStep) {
          f_FlightPhase = FLIGHT_PHASE_END;
          f_HasFlightPhaseChanged = TRUE;
        }

        break;
    }
  } else {
    switch (f_FlightPhase) {
      case FLIGHT_PHASE_LAUNCH:
        f_FlightPhase = FLIGHT_PHASE_CLIMB;
        break;
      case FLIGHT_PHASE_CLIMB:
        if (g_BallFlightStep > g_SimulatedShot.minPositionStep[AXIS_Y]) {
          f_FlightPhase = FLIGHT_PHASE_NEARING_CONTACT;
          f_HasFlightPhaseChanged = TRUE;
        }

        break;
      case FLIGHT_PHASE_NEARING_CONTACT:
        /*
         * The empty case is there to match: without it 66 words differ, 32 in
         * shape
         */
        break;
    }
  }

  /*
   * Holding both look-ahead steps in aheadStep is there to match: a flagStep
   * local for the second gives 8 words differing, none in shape
   */
  aheadStep = step + CUP_PASS_LOOKAHEAD_STEP_COUNT;
  if (aheadStep <= g_SimulatedShot.stepCount &&
      IsCupPassPoint(IntLength2D(g_SimulatedShot.path[aheadStep][AXIS_X] -
                             g_HoleSetup.cupPosition[AXIS_X],
                         g_SimulatedShot.path[aheadStep][AXIS_Z] -
                             g_HoleSetup.cupPosition[AXIS_Z]),
          g_SimulatedShot.path[aheadStep][AXIS_Y])) {
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

  if (step > BILLBOARD_HIT_VIEW_MIN_STEP && step < f_BillboardHitStep &&
      !f_CameraChangeHoldCount) {
    f_FlightCameraKind = FLIGHT_CAMERA_BILLBOARD_HIT;
  }

  if (IsPastBillboardHitView(step) &&
      f_FlightCameraKind != FLIGHT_CAMERA_AROUND_END && IsFarFromShotEnd()) {
    f_IsCameraKindNew = TRUE;
    f_FlightCameraKind = FLIGHT_CAMERA_AROUND_END;
  }

  if (f_BillboardHitStep < step && HasTimeForNewCamera() &&
      f_HasFlightPhaseChanged == TRUE && IsFlightCameraFree()) {
    if (!f_CameraChangeHoldCount) {
      ChooseFlightCameraKind();
    }
  }

  if (f_CameraChangeHoldCount > 0) {
    f_CameraChangeHoldCount--;
  }

  if (g_IsDebugEnabled && IsGameFlagSet(GAME_FLAG_UNK5F)) {
    sprintf(g_PrintBuffer, "C:%d F:%2d %3d %3d ", (int)f_HasFlightPhaseChanged,
        (int)f_CameraChangeHoldCount, (int)step, (int)g_BallFlightStep);
    PrintDebugText(g_PrintBuffer, 8, 2);
    sprintf(g_PrintBuffer, "TYPE:%2d TIME:%2d", (int)f_FlightCameraKind,
        (int)f_FlightPhase);
    PrintDebugText(g_PrintBuffer, 10, 3);
  }

  if (f_ShotLength > VERY_LONG_SHOT_MIN_LENGTH && g_BallFlightStep == 40 &&
      g_SimulatedShot.stepCount > 80) {
    StartShotEffect(SHOT_EFFECT_LONG_SHOT);
  }

  if (f_ShotLength > BALL_SOUND_MIN_SHOT_LENGTH &&
      !g_SimulatedShot.usesPutter) {
    volume = (f_ShotLength / BALL_SOUND_VOLUME_LENGTH) + BALL_SOUND_MIN_VOLUME;
    if (volume > BALL_SOUND_MAX_VOLUME) {
      volume = BALL_SOUND_MAX_VOLUME;
    }

    if (step == BALL_SOUND_EARLY_STEP) {
      osSyncPrintf("ball:%d\n", volume);
      switch (f_FlightCameraKind) {
        case FLIGHT_CAMERA_UNK0:
        case FLIGHT_CAMERA_UNK1:
        case FLIGHT_CAMERA_BESIDE_TEE:
        case FLIGHT_CAMERA_AIM_VIEW:
        case FLIGHT_CAMERA_BESIDE_GOLFER:
        case FLIGHT_CAMERA_CIRCLING_BALL:
        case FLIGHT_CAMERA_CHASING_BALL:
        case FLIGHT_CAMERA_FOLLOWING_PATH:
          PlaySoundWithEffects(SOUND_SHOT_SPARKS, BALL_SOUND_CHANNEL, volume,
              BALL_SOUND_REVERB, SOUND_FULL_SCALE);
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
        case FLIGHT_CAMERA_UNKE:
        case FLIGHT_CAMERA_UNKF:
        case FLIGHT_CAMERA_UNK10:
        case FLIGHT_CAMERA_UNK11:
          PlaySoundWithEffects(SOUND_UNK90, BALL_SOUND_CHANNEL, volume,
              BALL_SOUND_REVERB, SOUND_FULL_SCALE);
          break;
      }
    }
  }

  PlayFlightSounds(step);
  g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
  PlaceFlightCamera(step);
  g_AreCharactersDrawn = TRUE;
  g_IsCupViewShown = FALSE;
  if (g_SimulatedShot.usesPutter) {
    if (g_IsModuleAlive[MODULE_GOLF_BALLS] ||
        ReportModuleNotAlive(MODULE_GOLF_BALLS)) {
      StartFastBallAfterimage();
    }

    /* pathFlags is an s8, so testing 0x100 tests its sign; the original has it
     */
    if ((g_SimulatedShot.pathFlags[step] & 0x100) ||
        (g_SimulatedShot.pathFlags[step] & STEP_FLAG_LIPPED_OUT)) {
      StartContactEffect(CONTACT_EFFECT_NONE);
    }
  } else if (g_SimulatedShot.pathFlags[step]) {
    g_BallTrailState = BALL_TRAIL_HELD;
    StartContactEffect(CONTACT_EFFECT_FADE);
    if (g_IsModuleAlive[MODULE_GOLF_BALLS] ||
        ReportModuleNotAlive(MODULE_GOLF_BALLS)) {
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

    if (!g_GameState.isMiniGolf && !g_SimulatedShot.endsInCup &&
        IsPinHitViewed(step)) {
      g_Cameras[0].view.fieldOfView = DEFAULT_FIELD_OF_VIEW;
      StartHudAnimation(HUD_ANIMATION_NONE);
      g_InsetView.animation = INSET_VIEW_STILL;
      g_InsetView.step = 0;
      g_Cameras[INSET_CAMERA].view.drawFlags = 0;
      g_Cameras[0].view.lookAt[AXIS_X] =
          (f32)g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.lookAt[AXIS_Y] =
          ((f32)g_HoleSetup.cupPosition[AXIS_Y] - CUP_LOOK_AT_DEPTH) *
          COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.lookAt[AXIS_Z] =
          (f32)g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.eye[AXIS_X] = g_Cameras[0].view.lookAt[AXIS_X] +
          (sinf(f_CupViewYaw) * CUP_CLOSE_VIEW_DISTANCE);
      g_Cameras[0].view.eye[AXIS_Y] =
          g_Cameras[0].view.lookAt[AXIS_Y] - CUP_CLOSE_VIEW_HEIGHT;
      g_Cameras[0].view.eye[AXIS_Z] = g_Cameras[0].view.lookAt[AXIS_Z] -
          (cosf(f_CupViewYaw) * CUP_CLOSE_VIEW_DISTANCE);
    } else if (IsCupPassPoint(
                   f_BallCupGroundDistance, f_FlightBallPosition[AXIS_Y]) ||
        f_CupViewHoldCount > 0) {
      g_Cameras[0].view.fieldOfView = CUP_VIEW_FIELD_OF_VIEW;
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
      g_Cameras[0].view.lookAt[AXIS_X] =
          (f32)g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.lookAt[AXIS_Y] =
          (f32)g_HoleSetup.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.lookAt[AXIS_Z] =
          (f32)g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
      point[AXIS_X] = (-sinf(f_CameraYaw) * CUP_VIEW_DISTANCE) +
          g_HoleSetup.cupPosition[AXIS_X];
      point[AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y] - CUP_VIEW_HEIGHT;
      point[AXIS_Z] = (cosf(f_CameraYaw) * CUP_VIEW_DISTANCE) +
          g_HoleSetup.cupPosition[AXIS_Z];
      g_Cameras[0].view.eye[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.eye[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.eye[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    } else {
      g_Cameras[0].view.eye[AXIS_X] = f_CameraPosition[AXIS_X];
      g_Cameras[0].view.eye[AXIS_Y] = f_CameraPosition[AXIS_Y];
      g_Cameras[0].view.eye[AXIS_Z] = f_CameraPosition[AXIS_Z];
      g_Cameras[0].view.lookAt[AXIS_X] = f_LookAtPoint[AXIS_X];
      g_Cameras[0].view.lookAt[AXIS_Y] = f_LookAtPoint[AXIS_Y];
      g_Cameras[0].view.lookAt[AXIS_Z] = f_LookAtPoint[AXIS_Z];
    }
  } else {
    g_Cameras[0].view.eye[AXIS_X] = f_CameraPosition[AXIS_X];
    g_Cameras[0].view.eye[AXIS_Y] = f_CameraPosition[AXIS_Y];
    g_Cameras[0].view.eye[AXIS_Z] = f_CameraPosition[AXIS_Z];
    g_Cameras[0].view.lookAt[AXIS_X] = f_LookAtPoint[AXIS_X];
    g_Cameras[0].view.lookAt[AXIS_Y] = f_LookAtPoint[AXIS_Y];
    g_Cameras[0].view.lookAt[AXIS_Z] = f_LookAtPoint[AXIS_Z];
  }

  if (!g_IsCupViewShown) {
    AimCameraYaw();
  }

  g_Cameras[INSET_CAMERA].view.eye[AXIS_X] =
      (f32)(f_ShotEndPosition[AXIS_X] - INSET_VIEW_SIDE_OFFSET) *
      COURSE_TO_WORLD_SCALE;
  g_Cameras[INSET_CAMERA].view.eye[AXIS_Y] =
      (f32)(f_ShotEndPosition[AXIS_Y] - INSET_VIEW_HEIGHT) *
      COURSE_TO_WORLD_SCALE;
  g_Cameras[INSET_CAMERA].view.eye[AXIS_Z] =
      (f32)f_ShotEndPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
  g_Cameras[INSET_CAMERA].view.fieldOfView = INSET_FIELD_OF_VIEW;
  if (f_EndSurfaceInfo->kind == SURFACE_KIND_OUT_OF_BOUNDS) {
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] =
        (f32)f_ShotEndPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] =
        (f32)f_ShotEndPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] =
        (f32)f_ShotEndPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
  } else {
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] =
        (f32)f_FlightBallPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] =
        (f32)f_FlightBallPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] =
        (f32)f_FlightBallPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
  }

  if (g_Cameras[0].view.eye[AXIS_X] == g_Cameras[0].view.lookAt[AXIS_X] &&
      g_Cameras[0].view.eye[AXIS_Z] == g_Cameras[0].view.lookAt[AXIS_Z]) {
    g_Cameras[0].view.lookAt[AXIS_X] += LOOK_AT_NUDGE;
    g_Cameras[0].view.lookAt[AXIS_Z] += LOOK_AT_NUDGE;
  }

  if (g_Cameras[INSET_CAMERA].view.eye[AXIS_X] ==
          g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] &&
      g_Cameras[INSET_CAMERA].view.eye[AXIS_Z] ==
          g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z]) {
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] += LOOK_AT_NUDGE;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] += LOOK_AT_NUDGE;
  }

  if (g_SimulatedShot.stepCount < g_BallFlightStep + 40 &&
      IsHudAnimationDone() == 1) {
    StartHudAnimation(HUD_ANIMATION_NONE);
  }

  if (isAdvancing == TRUE && g_BallFlightStep == 0) {
    PlayContactSound();
  }

  /* Reading f_LandingWaitFrame and g_BallFlightStep through locals is there to
   * match */
  waitFrame = f_LandingWaitFrame;
  if (isAdvancing == TRUE) {
    flightStep = &g_BallFlightStep;
    if (*flightStep == g_SimulatedShot.stepCount) {
      if (waitFrame == 0 && !g_TurnBallSimulation.isHoledOut) {
        StartLandingReaction(LANDING_REACTION_REST);
      }

      if (++f_LandingWaitFrame >= f_LandingWaitFrameCount) {
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
  f_LandingWaitFrameCount = frameCount;
}

#define VIEW_SIDE_DISTANCE 153600.0f

/*
 * Finds the points VIEW_SIDE_DISTANCE from center on x and z, a quarter turn
 * either way from the main camera's yaw.
 */
void GetViewSidePoints(CoursePoint center, CoursePoint plusQuarterPoint,
    CoursePoint minusQuarterPoint) {
  f32 offsetX;
  f32 offsetZ;
  Radians yaw;
  Radians plusQuarterAngle;
  Radians minusQuarterAngle;

  offsetX = g_Cameras[0].view.lookAt[AXIS_X] - g_Cameras[0].view.eye[AXIS_X];
  offsetZ = g_Cameras[0].view.lookAt[AXIS_Z] - g_Cameras[0].view.eye[AXIS_Z];
  if ((s32)offsetX != 0 || (s32)offsetZ != 0) {
    yaw = M_PI - atan2f(offsetX, offsetZ);
  } else {
    yaw = 0.0f;
  }

  plusQuarterAngle = yaw + (M_PI / 2);
  plusQuarterPoint[AXIS_X] =
      (s32)(-sinf(plusQuarterAngle) * VIEW_SIDE_DISTANCE) + center[AXIS_X];
  plusQuarterPoint[AXIS_Z] =
      (s32)(cosf(plusQuarterAngle) * VIEW_SIDE_DISTANCE) + center[AXIS_Z];
  minusQuarterAngle = yaw - (M_PI / 2);
  minusQuarterPoint[AXIS_X] =
      (s32)(-sinf(minusQuarterAngle) * VIEW_SIDE_DISTANCE) + center[AXIS_X];
  minusQuarterPoint[AXIS_Z] =
      (s32)(cosf(minusQuarterAngle) * VIEW_SIDE_DISTANCE) + center[AXIS_Z];
}

/* Left empty in the original, as are the two routines after it */
void OnShotCameraPlaced(void) {}

void FirstUnusedCameraStub(void) {}

void SecondUnusedCameraStub(void) {}

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
 * Doesn't match: the original converts the soft spot's squared distance into a
 * temporary, takes its root there and copies the root to distance, which radius
 * then reads. Here cse2 replaces distance with the root's register, so the copy
 * goes. Per cse.c, the copy survives only if make_regs_eqv makes distance the
 * class's canonical register, which needs distance to outlive the root beyond
 * the extended block or to be used before it, and if cse_insn's swap, which
 * makes the root's insn set distance itself, then fails, which needs the copy's
 * previous insn to set something other than the root. Getting the distance from
 * an inline helper, with the squared distance and the root in locals of their
 * own, gives these 3 words (1 in shape), and the register oracle leaves them
 * with no register forced, so allocation isn't the cause. Before the helper,
 * the draft converted into distance and took the root there, leaving 6 words;
 * taking sqrtf of the sum directly gave 10, setting distance before the loops
 * or copying a root temporary to it 3, writing it in the first loop as well 9,
 * and a squaredDistance local, wherever declared, 10 (4 in shape). 3 words
 * differ, 1 in shape.
 */
#ifdef NON_MATCHING
static inline f32 GetSpotDistance(s32 rowOffset, s32 columnOffset) {
  f32 squaredDistance = (rowOffset * rowOffset) + (columnOffset * columnOffset);
  f32 distance = sqrtf(squaredDistance);

  return distance;
}

void BuildSpotTexels(u8* texels, Boolean isSoft) {
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
        radius = GetSpotDistance(row - height, column - width);
        if (radius <= SPOT_DISC_RADIUS) {
          intensity = SPOT_PEAK_INTENSITY;
        } else if (radius <= SPOT_GAP_RADIUS) {
          intensity = SPOT_PEAK_INTENSITY -
              ((radius - SPOT_DISC_RADIUS) * SPOT_GAP_FALL_PER_TEXEL);
        } else if (radius < SPOT_QUARTER_SIZE) {
          intensity = (radius - SPOT_GAP_RADIUS) * SPOT_RING_RISE_PER_TEXEL;
        } else {
          intensity = 0;
        }

        texels[texelIndex++] = intensity;
      }
    }
  } else {
    for (row = 0, softDiscRadius = SPOT_SOFT_DISC_RADIUS; row != height;
        row++) {
      for (column = 0; column != width; column++) {
        distance = GetSpotDistance(row - height, column - width);
        radius = distance;
        if (radius <= softDiscRadius) {
          intensity = SPOT_PEAK_INTENSITY;
        } else if (radius <= lastRadius) {
          intensity = SPOT_PEAK_INTENSITY -
              ((s32)(distance - softDiscRadius) * SPOT_SOFT_FALL_PER_TEXEL);
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
void BuildRingTexels(
    u8* texels, s32 width, s32 height, s32 outerRadius UNUSED) {
  s32 row;
  s32 column;
  s32 texelIndex;
  s32 radius;
  f32 distance;
  s32 intensity;

  texelIndex = 0;
  for (row = 0; row != height; row++) {
    for (column = 0; column != width; column++) {
      distance = sqrtf(((row - height) * (row - height)) +
          ((column - width) * (column - width)));
      radius = distance;
      if (radius < RING_INNER_RADIUS) {
        intensity = 0;
      } else if (radius < RING_PEAK_RADIUS) {
        intensity =
            ((s32)(distance - RING_INNER_RADIUS) * RING_RISE_PER_TEXEL) +
            RING_INNER_INTENSITY;
      } else if (radius < RING_OUTER_RADIUS) {
        intensity = RING_PEAK_INTENSITY -
            ((s32)(distance - RING_PEAK_RADIUS) * RING_FALL_PER_TEXEL);
      } else {
        intensity = 0;
      }

      texels[texelIndex++] = intensity;
    }
  }
}

#define MIN_UNDIVIDED_NOISE_SIZE 2
#define NOISE_DISPLACEMENT_MASK 0xFF
#define NOISE_DISPLACEMENT_BIAS 0x80
#define NOISE_CORNER_SEED (-0xFE0)
#define NOISE_CENTER_SEED 0x3F80
#define NOISE_EDGE_SEED 1
#define NOISE_MAX_BLEND_WEIGHT 0x3F
#define NOISE_BLEND_STEP_MASK 0x7F
#define NOISE_INTENSITY_SHIFT 13
#define NOISE_MAX_INTENSITY 0xFF

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
  roughness = f_NoiseRoughness[level];
  half = size >> 1;
  topLeft = f_NoiseGrid[row][column];
  topRight = f_NoiseGrid[row][(column + size) & NOISE_GRID_MASK];
  bottomLeft = f_NoiseGrid[(row + size) & NOISE_GRID_MASK][column];
  bottomRight = f_NoiseGrid[(row + size) & NOISE_GRID_MASK]
                           [(column + size) & NOISE_GRID_MASK];
  if (f_NoiseGrid[row + half][column + half] == 0) {
    do {
      f_NoiseGrid[row + half][column + half] =
          ((topLeft + topRight + bottomLeft + bottomRight) >> 2) +
          (RandomNoiseDisplacement() * roughness);
    } while (f_NoiseGrid[row + half][column + half] == 0);
  }

  if (f_NoiseGrid[row][column + half] == 0) {
    do {
      f_NoiseGrid[row][column + half] =
          ((topLeft + topRight) >> 1) + (RandomNoiseDisplacement() * roughness);
    } while (f_NoiseGrid[row][column + half] == 0);
  }

  if (f_NoiseGrid[row + half][column] == 0) {
    do {
      f_NoiseGrid[row + half][column] = ((topLeft + bottomLeft) >> 1) +
          (RandomNoiseDisplacement() * roughness);
    } while (f_NoiseGrid[row + half][column] == 0);
  }

  if (f_NoiseGrid[row + half][(column + size) & NOISE_GRID_MASK] == 0) {
    do {
      f_NoiseGrid[row + half][(column + size) & NOISE_GRID_MASK] =
          ((topRight + bottomRight) >> 1) +
          (RandomNoiseDisplacement() * roughness);
    } while (f_NoiseGrid[row + half][(column + size) & NOISE_GRID_MASK] == 0);
  }

  if (f_NoiseGrid[(row + size) & NOISE_GRID_MASK][column + half] == 0) {
    do {
      f_NoiseGrid[(row + size) & NOISE_GRID_MASK][column + half] =
          ((bottomLeft + bottomRight) >> 1) +
          (RandomNoiseDisplacement() * roughness);
    } while (f_NoiseGrid[(row + size) & NOISE_GRID_MASK][column + half] == 0);
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
  f_NoiseGrid = f_NoiseGrids[gridIndex];
}

/*
 * Draws one row of the noise texture as a blend of the two noise grids. After
 * every row has been drawn, it moves the blend a step toward the other grid.
 * Once the blend has gone fully over, it rebuilds the faded-out grid and swaps
 * the grids, so the new one fades in.
 */
void StepNoiseTexture(void) {
  /*
   * index counts the grids, then the texels. Sharing it is there to match: an
   * s16 texel local gives 28 words differing (none in shape), and an s32 one 45
   * (9)
   */
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

      /* Seeds the same grid again, leaving the other one unseeded; the original
       * has it */
      f_NoiseGrid[0][0] = NOISE_CORNER_SEED;
      f_NoiseGrid[NOISE_GRID_SIZE / 2][NOISE_GRID_SIZE / 2] = NOISE_CENTER_SEED;
      SelectNoiseGrid(f_NoiseGridIndex ^ 1);
      DisplaceNoiseSquare(0, 0, 0);
    }

    index = f_NoiseRow * NOISE_GRID_SIZE;
    weight = f_NoiseBlendStep & NOISE_MAX_BLEND_WEIGHT;
    for (row = f_NoiseRow; row < f_NoiseRow + 1; row++) {
      for (column = 0; column < NOISE_GRID_SIZE; column++) {
        blend =
            ((f_NoiseGrids[f_NoiseGridIndex][row][column] *
                 (NOISE_MAX_BLEND_WEIGHT - weight)) +
                (f_NoiseGrids[f_NoiseGridIndex ^ 1][row][column] * weight)) >>
            NOISE_INTENSITY_SHIFT;
        intensity = Abs(blend);
        if (intensity > NOISE_MAX_INTENSITY) {
          intensity = NOISE_MAX_INTENSITY;
        }

        g_NoiseTexels[index++] = intensity;
      }
    }

    f_NoiseRow = (f_NoiseRow + 1) & NOISE_GRID_MASK;
    if (f_NoiseRow == 0) {
      f_NoiseBlendStep = (f_NoiseBlendStep + 1) & NOISE_BLEND_STEP_MASK;
      if ((f_NoiseBlendStep & NOISE_MAX_BLEND_WEIGHT) == 0) {
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

/* Resets the noise grids and draws every row of the noise texture */
void BuildNoiseTexture(void) {
  s32 row;

  f_IsNoiseResetPending = TRUE;
  for (row = 0; row < NOISE_GRID_SIZE; row++) {
    StepNoiseTexture();
  }
}
