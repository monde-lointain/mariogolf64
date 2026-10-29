/*
 * Each player's turn: the aim camera and the aim controls, setting up and
 * taking the shot, the shot preview, Course Points, records of holes and Ring
 * Shot, picking the next player, and the round's debug tools.
 */
#include "common.h"

#include "turn.h"

#include "asset.h"
#include "ball_simulation.h"
#include "billboard.h"
#include "camera.h"
#include "character.h"
#include "character_slot.h"
#include "club.h"
#include "computer_golfer.h"
#include "controller.h"
#include "course.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "debug_text.h"
#include "effects.h"
#include "framebuffer.h"
#include "functions.h"
#include "game.h"
#include "game_flags.h"
#include "golf_balls.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "libc.h"
#include "main.h"
#include "math_util.h"
#include "movie.h"
#include "overlay_manager.h"
#include "pause_menu.h"
#include "pin.h"
#include "play.h"
#include "reaction.h"
#include "round_events.h"
#include "round_hud.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "scorecard.h"
#include "scoring.h"
#include "screen.h"
#include "screen_layers.h"
#include "shot_effects.h"
#include "shot_simulation.h"
#include "sound.h"
#include "surface.h"
#include "terrain.h"
#include "variables.h"

#define IMPACT_POINT_SOUND_CHANNEL 8
#define IMPACT_POINT_SOUND_VOLUME 60
#define IMPACT_POINT_SOUND_REVERB 20
#define IMPACT_POINT_SOUND_PAN 230

/* The buttons that hold the impact point mode */
#define IMPACT_POINT_BUTTONS (Z_TRIG | L_TRIG)

/* Course units to tenths of a meter, as METERS_PER_UNIT * 10 */
#define DECIMETERS_PER_UNIT (1.0f / 1536)
/* Sixty meters */
#define NEAR_CUP_MAX_DECIMETERS 600.0f

/* The kinds of notable shot that earn Course Points, as counted in unkAA */
#define SHOT_KIND_COUNT PLAYER_UNKAA_COUNT
#define SHOT_KIND_MAX_COUNT 0xFF
#define MAX_RING_SHOT_CLEAR_COUNT 99

#define HOLE_OUT_CALL_CHANNEL 3
/* How far the control pad tilts the stick, as GetTurnStickX and GetTurnStickY
 * read it */
#define DPAD_STICK_TILT 40
/* The aim camera stays inside the course's ground, in course units */
#define AIM_CAMERA_MAX_X 4347904.0f
#define AIM_CAMERA_MAX_Z 8542208.0f
/* How far above the ground under the ball the aim camera looks, in world units
 */
#define AIM_LOOK_AT_RISE 17.25f
/* The bounds that KeepAimCameraOnCourse keeps the aim camera within */
#define AIM_CAMERA_MIN_COORD (-0x25800)
#define AIM_CAMERA_COURSE_MAX_X 0x425800
#define AIM_CAMERA_COURSE_MAX_Z 0x825800
#define AIM_CAMERA_MINI_GOLF_MAX_X 0x109600
#define AIM_CAMERA_MINI_GOLF_MAX_Z 0x104B00
/* How far the C buttons push the aim camera on, once it has been pulled in */
#define AIM_CAMERA_PUSH_STEP 0x25800
#define AIM_CAMERA_MINI_GOLF_PUSH_STEP 0xB400
/* Where AimDownRange puts the aim camera, on the green and elsewhere */
#define DOWN_RANGE_GREEN_OFFSET (-0x10E00)
#define DOWN_RANGE_DISTANCE (-0x2D000)
#define DOWN_RANGE_HEIGHT (-0x12C00)
/* How far beyond the camera the down-range look-at point lies */
#define DOWN_RANGE_LOOK_AHEAD 0x280000
/* Where the aim camera eases to, in course units */
#define AIM_CAMERA_PUTT_DISTANCE (-0x1800C)
/* Back from where the shot lands */
#define AIM_CAMERA_APPROACH_BACKOFF 0x4B000
/* Short of the shot's full distance */
#define AIM_CAMERA_SHOT_BACKOFF_YARDS 50
#define AIM_CAMERA_PUTT_HEIGHT (-0xB400)
#define AIM_CAMERA_APPROACH_HEIGHT (-0x12C00)
#define AIM_CAMERA_SHOT_HEIGHT (-0x25800)
/* The aim camera moves this fraction of the way each frame, up to the most */
#define AIM_CAMERA_EASE_FRACTION 0.3f
#define AIM_CAMERA_MAX_EASE_STEP (10 * UNITS_PER_YARD)
/* The free-look camera starts behind the ball, and C-left and C-right turn it
 */
#define FREE_LOOK_START_DISTANCE (-0x12C00)
#define FREE_LOOK_TURN_STEP 200
/* A shorter pulse than the shots' rumble in effects.c */
#define TURN_RUMBLE_FREQUENCY 0x100
#define TURN_RUMBLE_FRAME_COUNT 8
/* Every AMBIENT_SOUND_INTERVAL frames, one of the course's ambient sounds may
 * play */
#define AMBIENT_SOUND_INTERVAL 240
#define AMBIENT_SOUND_CHOICES 7
#define NO_AMBIENT_SOUND (-1)
#define AMBIENT_SOUND_CHANNEL 10
#define AMBIENT_SOUND_VOLUME 0x5F
#define AMBIENT_SOUND_REVERB 15
#define SOUND_PAN_RANGE 255
/* The most shots that each kind of replay record keeps */
#define HOLE_RECORD_MAX_STROKES 5
#define RING_SHOT_RECORD_MAX_STROKES 6
#define IMPACT_SOUND_VOLUME 0x64
/* D_800FBDB8 while the contest movies play */
#define CONTEST_MOVIES_PLAYING 60
/* The order in which players' lies come up, from the first */
#define LIE_PRIORITY_TEE 2
#define LIE_PRIORITY_OFF_GREEN 1
#define LIE_PRIORITY_ON_GREEN 0
/* Under GAME_FLAG_FEWEST_STROKES_FIRST, a priority that falls with each stroke
 */
#define STROKE_PRIORITY_BASE 200
/* The stick turns the aim by half its tilt, times a quarter (a sixth for
 * g_AltHeading) of it */
#define AIM_STICK_SPEED_DIVISOR 4
#define ALT_HEADING_STICK_SPEED_DIVISOR 6
/* The D-pad's turn speeds up over this many frames */
#define AIM_DPAD_MAX_FRAMES 60
#define AIM_DPAD_SPEED_DIVISOR 6
#define AIM_DPAD_BASE_SPEED 3
/* The shot preview's step count, which a debug flag lets the D-pad tune */
#define SHOT_PREVIEW_MIN_STEPS 2
#define SHOT_PREVIEW_MAX_STEPS 20
#define SHOT_PREVIEW_SCROLL_STEP 8
#define INITIAL_SHOT_PREVIEW_STEP_COUNT 10
#define AIM_CAMERA_DEBUG_TILT_SCALE 30.0f
/* The pause screen's camera circles the ball at this distance, and rises this
 * far above it */
#define PAUSE_CAMERA_RADIUS 1536000.0f
#define PAUSE_CAMERA_RISE 4608000.0f
/* In mini golf, it circles the middle of the course instead */
#define MINI_GOLF_CENTER_X 543488.0f
#define MINI_GOLF_CENTER_Z 533888.0f
#define MINI_GOLF_PAUSE_CAMERA_RADIUS 15360.0f
#define MINI_GOLF_PAUSE_CAMERA_RISE 890880.0f
#define MINI_GOLF_PAUSE_CAMERA_DISTANCE 0x1A400
/* The aim camera keeps above the lowest ground of this many points behind it */
#define AIM_GROUND_PROBE_COUNT 4
#define AIM_GROUND_PROBE_STEP 0x12C00
/* Without an impact point, the camera drops lower and rises less with its
 * height */
#define NO_IMPACT_POINT_DROP 5683.2f
#define NO_IMPACT_POINT_HEIGHT_SCALE 0.4f
/* The camera rises at most the square of its distance in thousands, over this
 */
#define AIM_CAMERA_RISE_DIVISOR 4
/* On the green, the camera looks this far past the ball and this far above the
 * ground */
#define GREEN_LOOK_AHEAD 0x4B000
#define GREEN_LOOK_RISE 0x1200
#define GREEN_LOOK_HEIGHT_SCALE 0.7f
/* Off the green, the camera tilts down by the yards the club falls short of
 * this */
#define AIM_TILT_FULL_DISTANCE 300
#define AIM_TILT_PER_YARD 0.005f
/* Looking back from the far side, the camera looks this far up from the ball */
#define LOOK_BACK_RISE 0x3C00
#define LOOK_BACK_HEIGHT_SCALE 0.5f
#define SHOT_CAMERA_FIELD_OF_VIEW 45.0f
/* A replay turns the aim at once, rather than easing it round */
/* A hole given up outside a match counts as this many times its par */
#define GIVE_UP_PAR_MULTIPLE 3
#define GAME_FLAG_SNAP_REPLAY_AIM 0xF
/* A replay eases the aim by this much plus a 128th of what is left, until this
 * close */
#define REPLAY_AIM_MIN_STEP 0x80
#define REPLAY_AIM_STEP_DIVISOR 128
#define REPLAY_AIM_SETTLE_FRAMES 6
#define AIM_RESET_BUTTONS (L_CBUTTONS | R_CBUTTONS)
#define AIM_CAMERA_BUTTONS (U_CBUTTONS | D_CBUTTONS | L_CBUTTONS | R_CBUTTONS)
#define AIM_CAMERA_RAISE_BUTTONS (U_CBUTTONS | R_CBUTTONS)
#define AIM_CAMERA_LOWER_BUTTONS (D_CBUTTONS | L_CBUTTONS)
#define AIM_CAMERA_SOUND_VOLUME 0x50
#define LOOK_SOUND_VOLUME 0x3F
/* Frames that the impact point stays shown after Z or L is let go */
#define IMPACT_POINT_LINGER_FRAMES 3
/* Looking back from past the ball starts this far beyond the cup, and this high
 */
#define LOOK_BACK_EXTRA_DISTANCE 53760.0f
#define LOOK_BACK_START_HEIGHT (-0x1E00)
/* The aim camera's height and distance limits; heights are negative upwards */
#define GREEN_AIM_CAMERA_MIN_HEIGHT (-0x2A00)
#define AIM_CAMERA_MIN_HEIGHT (-0x3C00)
#define AIM_CAMERA_MAX_HEIGHT (-0x96000)
#define LOOK_BACK_MIN_DISTANCE (-0x177000)
#define LOOK_BACK_MAX_DISTANCE 0x177000
#define AIM_CAMERA_MIN_DISTANCE (-0xBB800)
#define AIM_CAMERA_MAX_DISTANCE 0x8CA000
#define AIM_VIEW_TOGGLE_COUNT 2
/* Half the putt preview line's width, in screen pixels */
#define PUTT_PREVIEW_HALF_WIDTH 10.0f
/* Hides the shot preview */
#define GAME_FLAG_HIDE_SHOT_PREVIEW 0x4B
/* Lets the D-pad change how far the shot preview reaches */
#define GAME_FLAG_TUNE_SHOT_PREVIEW 0x71
/* PickStartingClub settles for clubs with at least this lie rating */
#define STARTING_CLUB_MIN_LIE_RATING 2
/* Aims every shot straight down the driving range */
#define GAME_FLAG_AIM_DOWN_RANGE 0x24
/* Only hole-outs in fewer strokes than this enter the records */
#define MAX_RECORDED_STROKE_COUNT 4
/* The most strokes a player's count shows */
#define STROKE_COUNT_MAX 99
/* Outside training, a player who reaches this many times the hole's par
 * is out of the hole */
#define STROKE_LIMIT_PER_PAR 3
/* Speed golf adds this to a player stopped by the stroke limit */
#define STROKE_LIMIT_PENALTY_MINUTES 10
/* Where the aim camera starts, in course units */
#define AIM_CAMERA_START_DISTANCE (-0xF000)
#define AIM_CAMERA_START_HEIGHT (-0x4E00)
/* The stick moves the aim camera's height by this much for every AIM_STICK_STEP
 * of tilt */
#define AIM_CAMERA_HEIGHT_STEP 0x600
#define AIM_STICK_STEP 16
#define AIM_STICK_DEAD_ZONE 8
/* The stick moves the aim camera's distance for every AIM_DISTANCE_STICK_STEP
 * of tilt */
#define AIM_DISTANCE_STICK_STEP 4
#define AIM_CAMERA_DISTANCE_STEP 0xF00
#define AIM_CAMERA_PUTT_DISTANCE_STEP 0x180
/* How far the C buttons move the aim camera each frame */
#define AIM_CAMERA_BUTTON_DISTANCE_STEP 42670.0810546875
#define AIM_CAMERA_BUTTON_HEIGHT_STEP 28446.720703125
#define AIM_CAMERA_BUTTON_PUTT_STEP 4267.00810546875
/* The last gauge frames at which power, and impact, can be set */
#define LAST_POWER_GAUGE_FRAME (GAUGE_IMPACT_FRAME - 1)
#define LAST_IMPACT_GAUGE_FRAME (GAUGE_IMPACT_FRAME + 9)
/* How far either side of the impact mark the impact counts */
#define MAX_IMPACT_TIMING 6
/*
 * The swing animation starts once fewer gauge frames are left before this one
 * than its event frame, D_80224000_ovl_00
 */
#define SWING_TIMING_GAUGE_FRAME 71
/*
 * A shot that the player isn't taking tilts the stick toward its offset over
 * STICK_TILT_FRAME_COUNT frames, then holds it for the last
 * STICK_HOLD_FRAME_COUNT frames before impact
 */
#define STICK_TILT_FRAME_COUNT 10
#define STICK_HOLD_FRAME_COUNT 10
/* While putting, the crane's steps are this times a count that grows with the
 * putt's length */
#define AIM_CAMERA_PUTT_STEP_UNIT 426.700810546875
/* The counts for each putt mode, when craning and when moving along the line */
#define SHORT_PUTT_CRANE_STEP_COUNT 10
#define MIDDLE_PUTT_CRANE_STEP_COUNT 13
#define LONG_PUTT_CRANE_STEP_COUNT 15
#define SHORT_PUTT_DOLLY_STEP_COUNT 10
#define MIDDLE_PUTT_DOLLY_STEP_COUNT 15
#define LONG_PUTT_DOLLY_STEP_COUNT 20
/* The crane swings the camera forward only while it is below this height, and
 * back only above the other */
#define AIM_CAMERA_CRANE_FORWARD_HEIGHT (-0x5A00)
#define AIM_CAMERA_CRANE_BACK_HEIGHT (-0x96000)
/* Shots count towards Course Points on their first flights only */
#define MAX_COUNTED_FLIGHT_COUNT 2
/* How UpdateTurns polls the computer golfer's plan: at most this many
 * times as the aim is prepared, then this many times a frame of aiming */
#define PLAN_PREPARING_POLL_COUNT 100
#define PLAN_AIMING_POLL_COUNT 400
/* The time budget of each poll while the aim is prepared, and of the
 * first frame of aiming; the budget then grows by a factor each frame,
 * up to the most */
#define PLAN_PREPARING_TIME_BUDGET 10
#define PLAN_START_TIME_BUDGET 30
#define PLAN_TIME_BUDGET_GROWTH 1.05f
#define PLAN_MAX_TIME_BUDGET 0x800
/* Replaced by PLAN_START_TIME_BUDGET before it is read */
#define INITIAL_PLAN_TIME_BUDGET 10
/* A prepared shot waits this many frames of aiming before it is played */
#define PREPARED_SHOT_MIN_AIM_FRAMES 30

/* Where the current turn's shot comes from, as f_ShotSource records */
enum ShotSource {
  /* The player's controller */
  SHOT_SOURCE_PLAYER,
  /* D_80104F78: a replayed shot, or the computer golfer's finished plan */
  SHOT_SOURCE_PREPARED,
  /* The computer golfer, which is still planning */
  SHOT_SOURCE_PLANNING
};

extern s32 D_801051F0; /* Set unless two players share a controller */
extern s8 D_800FBDB8; /* PlayContestMovies' state */
extern CurrentShot D_80104F78; /* The replayed shot */
/* Set once the shot preview has been built */
static Boolean f_IsShotPreviewBuilt = FALSE;
/* Frames that a replayed shot's aim has been settled */
static s32 f_ReplayAimSettledFrameCount = 0;
/* A debug tilt of the aim camera, which the C buttons tune */
static f32 f_DebugAimCameraTilt = 0.0f;
/* Read by no routine */
static s32 D_80223E4C_ovl_00 = 0;
/* Frames that the D-pad has turned the aim, negative to the right */
static s32 f_AimTurnFrameCount = 0;
/* Read by no routine */
static s32 D_80223E54_ovl_00 = 0;
static u32 f_PlanTimeBudget = INITIAL_PLAN_TIME_BUDGET;
/* How many times the shot's flight has been shown; StartFlightCamera gets the
 * earlier count */
static s32 f_ShotFlightCount = 0;
/* A dash of the shot preview */
static Vtx f_ShotPreviewDashVertices[4] = {
    {{{-1, 0, 0}, 0, {0, 0}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{1, 0, 0}, 0, {0, 0x800}, {0x00, 0x00, 0x00, 0xFF}}},
    {{{1, 0, -1}, 0, {0x800, 0x800}, {0x00, 0x00, 0xFF, 0x00}}},
    {{{-1, 0, -1}, 0, {0x800, 0}, {0xFF, 0x00, 0x00, 0x00}}}};
/* How many steps the shot preview simulates */
static s32 f_ShotPreviewStepCount = INITIAL_SHOT_PREVIEW_STEP_COUNT;
/* How many of the shot preview's points are shown */
static s32 f_ShownShotPreviewPointCount = INITIAL_SHOT_PREVIEW_STEP_COUNT;
/* Whether the shot preview ran to its end */
static s8 f_HasShotPreviewEnded = FALSE;
/* Scrolls the shot preview's dots */
static s32 f_ShotPreviewDotScroll = 0;
/* The putt preview's line, placed on screen */
static Vtx f_PuttPreviewLineVertices[4] = {
    {{{0, 0, -100}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{0, -1, -100}, 0, {0, 0x7C0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{48, -1, -100}, 0, {0x17C0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{48, 0, -100}, 0, {0x17C0, 0x7C0}, {0xFF, 0xFF, 0xFF, 0xFF}}}};
/* The ambient sounds that each course may play, or NO_AMBIENT_SOUND */
static s32 f_AmbientSounds[COURSE_RECORD_COUNT][AMBIENT_SOUND_CHOICES] = {
    {0x16, 0x18, 0x19, 0x16, 0x18, 0x19, NO_AMBIENT_SOUND},
    {0x16, 0x18, 0x19, 0x16, 0x18, 0x19, NO_AMBIENT_SOUND},
    {NO_AMBIENT_SOUND, NO_AMBIENT_SOUND, NO_AMBIENT_SOUND, NO_AMBIENT_SOUND,
        NO_AMBIENT_SOUND, NO_AMBIENT_SOUND, NO_AMBIENT_SOUND},
    {0x1E, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8},
    {NO_AMBIENT_SOUND, NO_AMBIENT_SOUND, NO_AMBIENT_SOUND, NO_AMBIENT_SOUND,
        NO_AMBIENT_SOUND, NO_AMBIENT_SOUND, NO_AMBIENT_SOUND},
    {0x1A, 0x1B, 0x2B, 0x18, 0x1A, 0x1B, 0x2B},
    {0x16, 0x18, 0x19, 0x16, 0x18, 0x19, NO_AMBIENT_SOUND},
    {0x16, 0x18, 0x19, 0x16, 0x18, 0x19, NO_AMBIENT_SOUND},
    {0x16, 0x18, 0x19, 0x16, 0x18, 0x19, NO_AMBIENT_SOUND}};
/* Frames since the last ambient sound */
static s32 f_AmbientSoundIdleFrameCount = 0;
/* No routine sets it; while it is set, the next turn replays the player's
 * saved shot */
static s8 f_ShouldReplaySavedShot = FALSE;
/* Read by no routine */
static s8 D_80223FF1_ovl_00 = 0;
static s8 D_80223FF2_ovl_00 = 0;
static s8 D_80223FF3_ovl_00 = 0;
/* The debug guts test's state, character and motion */
static s8 f_HasGutsTestRun = FALSE;
static s8 f_GutsTestCharacter = 0;
static s8 f_GutsTestMotion = 0;
static s8 f_ShouldUpdateReaction = FALSE;
static s32 D_80223FF8_ovl_00 = 0;
static s32 D_80223FFC_ovl_00 = 0;
static s32 D_80224000_ovl_00 = 0;
static s32 D_80224004_ovl_00 = 0;
static s32 D_80224008_ovl_00 = 0;
static s32 D_8022400C_ovl_00 = 0;
/* Read by no routine */
static s32 D_80224010_ovl_00 = 0;
/* The angles of FrameBallAndShot's camera */
static Radians f_DebugFramingYaw = 0.0f;
static Radians f_DebugFramingPitch = 0.0f;
/* Read by no routine */
static s32 D_8022401C_ovl_00 = 0;

void SetCharacterTranslucent(u32 slotIndex, s32 value);
void PlaceAimCamera(void);
void PlaceShotCamera(void);
s32 func_8005F30C(s32 x, s32 z);
void func_8006034C(s8 value);
s32 GetRecordedScoreKind(s32 strokes);
Boolean HasNoRingsLeft(UnkB8* player);
void RecordRingShotClear(s32 playerIndex);

static Mtx D_80227110_ovl_00; /* An identity matrix for the putt preview */
/*
 * The launch of the shot being taken, which the shot preview and the ball
 * simulation start from. impactTiming is how far from the meter's impact mark
 * the swing was stopped, and clubIndex and shotMode are also the club slot and
 * shot mode that the golfer was last posed with.
 */
static ShotLaunch D_80227150_ovl_00;
static s32 D_80227170_ovl_00[MAX_PLAYER_COUNT];
static f32 D_80227180_ovl_00;
static f32 D_80227184_ovl_00;
static s32 D_80227188_ovl_00;
static s32 D_8022718C_ovl_00;
static s32 D_80227190_ovl_00;
static s32 f_ShotSource; /* A SHOT_SOURCE_* value */
static s32 f_TurnTimerStart; /* Nothing reads it */
/* The shot preview's matrices, by draw buffer */
static Mtx D_802271A0_ovl_00[2][SHOT_PREVIEW_MAX_STEPS];
/* The shot preview's points */
static f32 D_80227BA0_ovl_00[SHOT_PREVIEW_MAX_STEPS][AXIS_COUNT];

/* Not static: the original left it to the linker's common block */
s32 f_AimFrameCount;

/* Defined in main_bss.c, at their addresses in the original */
extern CurrentShot g_SavedShots[MAX_PLAYER_COUNT];
extern UnkB8 g_SavedPlayers[MAX_PLAYER_COUNT];
extern ShotLaunch g_PreviousBallSurfaceInfo;

/* Lets the Rumble Paks run again after nuContRmbForceStop */
void EndRumbleForceStop(void) {
  nuSiSendMesg(NU_CONT_RMB_FORCESTOPEND_MSG, NULL);
}

/*
 * Pulls the aim camera in along the heading, as far as it takes to keep it
 * within the course. Once it has moved, a held C button pushes it on by a step.
 */
#ifdef NON_MATCHING
void KeepAimCameraOnCourse(void) {
  Vec3 eye;
  s32 oldDistance = g_CurrentShot.aimCameraDistance;
  s32 minCoord;
  s32 maxX;
  s32 maxZ;
  Radians heading;
  f32 offset;
  f32 overshoot;

  if (g_GameState.isMiniGolf) {
    maxX = AIM_CAMERA_MINI_GOLF_MAX_X;
    maxZ = AIM_CAMERA_MINI_GOLF_MAX_Z;
  } else {
    maxX = AIM_CAMERA_COURSE_MAX_X;
    maxZ = AIM_CAMERA_COURSE_MAX_Z;
  }

  heading = (u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS;
  offset = cosf(heading) * g_CurrentShot.aimCameraDistance;
  eye[AXIS_X] = offset + g_CurrentShot.ballPosition[AXIS_X];
  minCoord = AIM_CAMERA_MIN_COORD;
  overshoot = 0.0f;
  if (eye[AXIS_X] < minCoord) {
    overshoot = minCoord - eye[AXIS_X];
  } else if (eye[AXIS_X] > maxX) {
    overshoot = eye[AXIS_X] - maxX;
  }

  if ((s32)offset != 0) {
    g_CurrentShot.aimCameraDistance *=
        (fabsf(offset) - overshoot) / fabsf(offset);
  }

  offset = sinf(heading) * g_CurrentShot.aimCameraDistance;
  eye[AXIS_Z] = offset + g_CurrentShot.ballPosition[AXIS_Z];
  overshoot = 0.0f;
  if (eye[AXIS_Z] < minCoord) {
    D_800C3070 = FALSE;
    overshoot = minCoord - eye[AXIS_Z];
  } else if (eye[AXIS_Z] > maxZ) {
    D_800C3070 = FALSE;
    overshoot = eye[AXIS_Z] - maxZ;
  }

  if ((s32)offset != 0) {
    g_CurrentShot.aimCameraDistance *=
        (fabsf(offset) - overshoot) / fabsf(offset);
  }

  if (g_GameState.isMiniGolf) {
    if (oldDistance < g_CurrentShot.aimCameraDistance) {
      if (g_TurnController.buttons & D_CBUTTONS) {
        g_CurrentShot.aimCameraDistance += AIM_CAMERA_MINI_GOLF_PUSH_STEP;
      }
    } else if (g_CurrentShot.aimCameraDistance < oldDistance &&
        (g_TurnController.buttons & U_CBUTTONS)) {
      g_CurrentShot.aimCameraDistance -= AIM_CAMERA_MINI_GOLF_PUSH_STEP;
    }
  } else {
    if (oldDistance < g_CurrentShot.aimCameraDistance) {
      if (g_TurnController.buttons & D_CBUTTONS) {
        g_CurrentShot.aimCameraDistance += AIM_CAMERA_PUSH_STEP;
      }
    } else if (g_CurrentShot.aimCameraDistance < oldDistance &&
        (g_TurnController.buttons & U_CBUTTONS)) {
      g_CurrentShot.aimCameraDistance -= AIM_CAMERA_PUSH_STEP;
    }
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", KeepAimCameraOnCourse);
#endif

void func_8004C6A8(void);

/*
 * Places the camera for the shot being set up: circling the ball on the pause
 * screen, behind the ball while aiming, or looking back at it from past the
 * ball.
 */
#ifdef NON_MATCHING
void PlaceShotCamera(void) {
  Vec3 point;
  CoursePoint center;
  Radians heading;

  if (IsGameFlagSet(GAME_FLAG_TEST_AIM_TILT_AND_MOVIES)) {
    if (g_ControllerStates[0].buttons & L_CBUTTONS) {
      f_DebugAimCameraTilt -= 1.0f;
    }

    if (g_ControllerStates[0].buttons & R_CBUTTONS) {
      f_DebugAimCameraTilt += 1.0f;
    }
  }

  if (g_PauseScreen == PAUSE_SCREEN_FREE_LOOK) {
    g_Cameras[0].mode = CAMERA_MODE_FREE_LOOK;
    point[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
    point[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
    point[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
    heading = (u16)g_FreeLookHeading * BINARY_ANGLE_TO_RADIANS;
    if (!g_GameState.isMiniGolf) {
      g_Cameras[0].view.lookAt[AXIS_X] =
          (point[AXIS_X] + (cosf(heading) * PAUSE_CAMERA_RADIUS)) *
          COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.lookAt[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.lookAt[AXIS_Z] =
          (point[AXIS_Z] - g_CurrentShot.aimCameraDistance +
              (sinf(heading) * PAUSE_CAMERA_RADIUS)) *
          COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.eye[AXIS_X] =
          (point[AXIS_X] + (cosf(heading) * -PAUSE_CAMERA_RADIUS)) *
          COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.eye[AXIS_Y] =
          (point[AXIS_Y] - PAUSE_CAMERA_RISE) * COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.eye[AXIS_Z] =
          (point[AXIS_Z] - g_CurrentShot.aimCameraDistance +
              (sinf(heading) * -PAUSE_CAMERA_RADIUS)) *
          COURSE_TO_WORLD_SCALE;
    } else {
      point[AXIS_X] = MINI_GOLF_CENTER_X;
      point[AXIS_Z] = MINI_GOLF_CENTER_Z;
      point[AXIS_Y] = g_HoleSetup.teePosition[AXIS_Y] +
          ((g_HoleSetup.cupPosition[AXIS_Y] - g_HoleSetup.teePosition[AXIS_Y]) *
              0.5f);
      g_Cameras[0].view.lookAt[AXIS_X] =
          (point[AXIS_X] + (cosf(heading) * MINI_GOLF_PAUSE_CAMERA_RADIUS)) *
          COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.lookAt[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.lookAt[AXIS_Z] =
          (point[AXIS_Z] -
              (g_CurrentShot.aimCameraDistance +
                  MINI_GOLF_PAUSE_CAMERA_DISTANCE) +
              (sinf(heading) * MINI_GOLF_PAUSE_CAMERA_RADIUS)) *
          COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.eye[AXIS_X] =
          (point[AXIS_X] + (cosf(heading) * -MINI_GOLF_PAUSE_CAMERA_RADIUS)) *
          COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.eye[AXIS_Y] =
          (point[AXIS_Y] - MINI_GOLF_PAUSE_CAMERA_RISE) * COURSE_TO_WORLD_SCALE;
      g_Cameras[0].view.eye[AXIS_Z] =
          (point[AXIS_Z] -
              (g_CurrentShot.aimCameraDistance +
                  MINI_GOLF_PAUSE_CAMERA_DISTANCE) +
              (sinf(heading) * -MINI_GOLF_PAUSE_CAMERA_RADIUS)) *
          COURSE_TO_WORLD_SCALE;
    }
  } else if (g_LetterboxState == LETTERBOX_STATE_NONE ||
      g_LetterboxState == LETTERBOX_STATE_UNK5) {
    PlaceAimCamera();
  } else if (g_LetterboxState == LETTERBOX_STATE_MOVING_CAMERA ||
      g_LetterboxState == LETTERBOX_STATE_LOOKING_DOWN_RANGE) {
    s32 lowestGround;
    s32 ground;
    s32 probe;
    s32 offset;
    s32 rise;
    f32 tilt;

    heading = (u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS;
    KeepAimCameraOnCourse();
    point[AXIS_X] = (cosf(heading) * g_CurrentShot.aimCameraDistance) +
        g_CurrentShot.ballPosition[AXIS_X];
    point[AXIS_Z] = (sinf(heading) * g_CurrentShot.aimCameraDistance) +
        g_CurrentShot.ballPosition[AXIS_Z];
    if (point[AXIS_X] < AIM_CAMERA_MIN_COORD) {
      point[AXIS_X] = AIM_CAMERA_MIN_COORD;
    } else if (point[AXIS_X] > AIM_CAMERA_MAX_X) {
      point[AXIS_X] = AIM_CAMERA_MAX_X;
      D_800C3070 = 0;
    }

    if (point[AXIS_Z] < AIM_CAMERA_MIN_COORD) {
      point[AXIS_Z] = AIM_CAMERA_MIN_COORD;
      D_800C3070 = 0;
    } else if (point[AXIS_Z] > AIM_CAMERA_MAX_Z) {
      point[AXIS_Z] = AIM_CAMERA_MAX_Z;
      D_800C3070 = 0;
    }

    lowestGround = func_8005F30C(point[AXIS_X], point[AXIS_Z]);
    g_Cameras[0].view.eye[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
    g_Cameras[0].view.eye[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    for (probe = 0; probe < AIM_GROUND_PROBE_COUNT; probe++) {
      offset = probe * AIM_GROUND_PROBE_STEP;
      point[AXIS_X] =
          (cosf(heading) * (g_CurrentShot.aimCameraDistance + offset)) +
          g_CurrentShot.ballPosition[AXIS_X];
      point[AXIS_Z] =
          (sinf(heading) * (g_CurrentShot.aimCameraDistance + offset)) +
          g_CurrentShot.ballPosition[AXIS_Z];
      ground = func_8005F30C(point[AXIS_X], point[AXIS_Z]);
      if (ground < lowestGround) {
        lowestGround = ground;
      }
    }

    if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
      point[AXIS_Y] = lowestGround - NO_IMPACT_POINT_DROP +
          (g_CurrentShot.aimCameraHeight * NO_IMPACT_POINT_HEIGHT_SCALE);
    } else {
      point[AXIS_Y] = lowestGround + g_CurrentShot.aimCameraHeight;
    }

    rise = g_CurrentShot.aimCameraDistance / 1000;
    if (g_CurrentShot.ballPosition[AXIS_Y] +
            (rise * rise / AIM_CAMERA_RISE_DIVISOR) <
        point[AXIS_Y]) {
      point[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y] +
          (rise * rise / AIM_CAMERA_RISE_DIVISOR);
    }

    g_Cameras[0].view.eye[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    if (g_ShotFlags & SHOT_FLAG_ON_GREEN) {
      point[AXIS_X] =
          (cosf(heading) *
              (g_CurrentShot.aimCameraDistance + GREEN_LOOK_AHEAD)) +
          g_CurrentShot.ballPosition[AXIS_X];
      point[AXIS_Y] = (lowestGround - GREEN_LOOK_RISE) -
          (g_CurrentShot.aimCameraHeight * GREEN_LOOK_HEIGHT_SCALE);
      point[AXIS_Z] =
          (sinf(heading) *
              (g_CurrentShot.aimCameraDistance + GREEN_LOOK_AHEAD)) +
          g_CurrentShot.ballPosition[AXIS_Z];
      g_Cameras[0].view.lookAt[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    } else {
      tilt = (AIM_TILT_FULL_DISTANCE - GetSelectedClub()->distance) *
          AIM_TILT_PER_YARD;
      if (tilt < 0.0f) {
        tilt = 0.0f;
      }

      point[AXIS_X] =
          (cosf(heading) *
              (g_CurrentShot.aimCameraDistance + DOWN_RANGE_LOOK_AHEAD)) +
          g_CurrentShot.ballPosition[AXIS_X];
      point[AXIS_Z] =
          (sinf(heading) *
              (g_CurrentShot.aimCameraDistance + DOWN_RANGE_LOOK_AHEAD)) +
          g_CurrentShot.ballPosition[AXIS_Z];
      point[AXIS_Y] = g_Cameras[0].view.eye[AXIS_Y];
      g_Cameras[0].view.lookAt[AXIS_Y] =
          (g_Cameras[0].view.eye[AXIS_Y] * COURSE_TO_WORLD_SCALE) -
          (g_CurrentShot.aimCameraHeight * (tilt * COURSE_TO_WORLD_SCALE)) +
          (f_DebugAimCameraTilt * AIM_CAMERA_DEBUG_TILT_SCALE);
    }

    g_Cameras[0].view.heading = heading;
    g_Cameras[0].view.lookAt[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
    g_Cameras[0].view.lookAt[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
  } else {
    KeepAimCameraOnCourse();

    /* The middle of the ball and the cup is worked out, but the ball is used */
    center[AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y];
    center[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
    heading = ((u16)g_AltHeading * BINARY_ANGLE_TO_RADIANS) + M_PI;
    center[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X] +
        ((g_HoleSetup.cupPosition[AXIS_X] -
             g_CurrentShot.ballPosition[AXIS_X]) *
            0.5f);
    center[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
    center[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z] +
        ((g_HoleSetup.cupPosition[AXIS_Z] -
             g_CurrentShot.ballPosition[AXIS_Z]) *
            0.5f);
    center[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
    point[AXIS_X] =
        (cosf(heading) * g_CurrentShot.aimCameraDistance) + center[AXIS_X];
    point[AXIS_Y] = g_CurrentShot.aimCameraHeight + center[AXIS_Y];
    point[AXIS_Z] =
        (sinf(heading) * g_CurrentShot.aimCameraDistance) + center[AXIS_Z];
    point[AXIS_Y] = func_8005F30C(point[AXIS_X], point[AXIS_Z]) +
        g_CurrentShot.aimCameraHeight;
    g_Cameras[0].view.eye[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
    g_Cameras[0].view.eye[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    g_Cameras[0].view.eye[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    point[AXIS_X] =
        (cosf(heading) * (g_CurrentShot.aimCameraDistance + GREEN_LOOK_AHEAD)) +
        center[AXIS_X];
    point[AXIS_Y] = (g_CurrentShot.ballPosition[AXIS_Y] - LOOK_BACK_RISE) -
        (g_CurrentShot.aimCameraHeight * LOOK_BACK_HEIGHT_SCALE);
    point[AXIS_Z] =
        (sinf(heading) * (g_CurrentShot.aimCameraDistance + GREEN_LOOK_AHEAD)) +
        center[AXIS_Z];
    g_Cameras[0].view.heading = heading;
    g_Cameras[0].view.lookAt[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
    g_Cameras[0].view.lookAt[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    g_Cameras[0].view.lookAt[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
  }

  g_Cameras[0].view.fieldOfView = SHOT_CAMERA_FIELD_OF_VIEW;
  func_8004C6A8();
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", PlaceShotCamera);
#endif

/*
 * Backs the aim camera up behind the ball, and points it down the shot's
 * heading. The clamped point that it first works out goes unused, as does the
 * ground height.
 */
void AimDownRange(void) {
  Vec3 lookAt;
  Radians heading;

  GetSelectedClub();
  if (g_ShotFlags & SHOT_FLAG_ON_GREEN) {
    g_CurrentShot.aimCameraHeight = DOWN_RANGE_GREEN_OFFSET;
    g_CurrentShot.aimCameraDistance = DOWN_RANGE_GREEN_OFFSET;
  } else {
    g_CurrentShot.aimCameraHeight = DOWN_RANGE_HEIGHT;
    g_CurrentShot.aimCameraDistance = DOWN_RANGE_DISTANCE;
  }

  heading = (u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS;
  lookAt[AXIS_X] = (cosf(heading) * g_CurrentShot.aimCameraDistance) +
      g_CurrentShot.ballPosition[AXIS_X];
  if (lookAt[AXIS_X] < 0.0f) {
    lookAt[AXIS_X] = 0.0f;
  } else if (lookAt[AXIS_X] >= AIM_CAMERA_MAX_X) {
    lookAt[AXIS_X] = AIM_CAMERA_MAX_X - 1.0f;
  }

  lookAt[AXIS_Z] = (sinf(heading) * g_CurrentShot.aimCameraDistance) +
      g_CurrentShot.ballPosition[AXIS_Z];
  if (lookAt[AXIS_Z] < 0.0f) {
    lookAt[AXIS_Z] = 0.0f;
  } else if (lookAt[AXIS_Z] >= AIM_CAMERA_MAX_Z) {
    lookAt[AXIS_Z] = AIM_CAMERA_MAX_Z - 1.0f;
  }

  lookAt[AXIS_X] =
      (cosf(heading) *
          (g_CurrentShot.aimCameraDistance + DOWN_RANGE_LOOK_AHEAD)) +
      g_CurrentShot.ballPosition[AXIS_X];
  lookAt[AXIS_Z] =
      (sinf(heading) *
          (g_CurrentShot.aimCameraDistance + DOWN_RANGE_LOOK_AHEAD)) +
      g_CurrentShot.ballPosition[AXIS_Z];
  lookAt[AXIS_Y] = g_Cameras[0].view.eye[AXIS_Y];
  GetGroundHeight(lookAt[AXIS_X], lookAt[AXIS_Z]);
  g_Cameras[0].view.heading = heading;
  g_Cameras[0].view.lookAt[AXIS_X] = lookAt[AXIS_X] * COURSE_TO_WORLD_SCALE;
  g_Cameras[0].view.lookAt[AXIS_Z] = lookAt[AXIS_Z] * COURSE_TO_WORLD_SCALE;
  g_Cameras[0].view.lookAt[AXIS_Y] = (lookAt[AXIS_Y] * COURSE_TO_WORLD_SCALE) -
      (g_CurrentShot.aimCameraHeight * COURSE_TO_WORLD_SCALE);
}

/* Places the aim camera on the aim line, looking at the ball */
#ifdef NON_MATCHING
void PlaceAimCamera(void) {
  Vec3 eye;
  Radians heading = (u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS;

  eye[AXIS_X] = (cosf(heading) * g_CurrentShot.aimCameraDistance) +
      g_CurrentShot.ballPosition[AXIS_X];
  if (eye[AXIS_X] < 0.0f) {
    eye[AXIS_X] = 0.0f;
  } else if (eye[AXIS_X] >= AIM_CAMERA_MAX_X) {
    eye[AXIS_X] = AIM_CAMERA_MAX_X - 1.0f;
  }

  eye[AXIS_Z] = (sinf(heading) * g_CurrentShot.aimCameraDistance) +
      g_CurrentShot.ballPosition[AXIS_Z];
  if (eye[AXIS_Z] < 0.0f) {
    eye[AXIS_Z] = 0.0f;
  } else if (eye[AXIS_Z] >= AIM_CAMERA_MAX_Z) {
    eye[AXIS_Z] = AIM_CAMERA_MAX_Z - 1.0f;
  }

  eye[AXIS_Y] = GetGroundHeight(eye[AXIS_X], eye[AXIS_Z]);
  if (g_CurrentShot.ballPosition[AXIS_Y] < eye[AXIS_Y]) {
    eye[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
  }

  g_Cameras[0].view.lookAt[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
  g_Cameras[0].view.lookAt[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
  eye[AXIS_X] *= COURSE_TO_WORLD_SCALE;
  eye[AXIS_Y] =
      (eye[AXIS_Y] + g_CurrentShot.aimCameraHeight) * COURSE_TO_WORLD_SCALE;
  eye[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
  g_Cameras[0].view.eye[AXIS_X] = eye[AXIS_X];
  g_Cameras[0].view.eye[AXIS_Y] = eye[AXIS_Y];
  g_Cameras[0].view.eye[AXIS_Z] = eye[AXIS_Z];
  g_Cameras[0].view.lookAt[AXIS_Y] =
      (func_8005F30C(g_CurrentShot.ballPosition[AXIS_X],
           g_CurrentShot.ballPosition[AXIS_Z]) *
          COURSE_TO_WORLD_SCALE) -
      AIM_LOOK_AT_RISE;
  g_Cameras[0].view.lookAt[AXIS_X] *= COURSE_TO_WORLD_SCALE;
  g_Cameras[0].view.lookAt[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", PlaceAimCamera);
#endif

/* Switches from the pause menu to the free-look camera, facing along the aim
 * line */
void StartFreeLook(void) {
  g_CurrentShot.aimCameraDistance = FREE_LOOK_START_DISTANCE;
  g_LetterboxState = LETTERBOX_STATE_MOVING_CAMERA;
  D_800C3070 = 0;
  g_ImpactPointState = IMPACT_POINT_OFF;
  g_RoundHud.flags = HUD_FLAG_AIMING | HUD_FLAG_LIGHT_CLUB_HEAD |
      HUD_FLAG_UNK4 | HUD_FLAG_TRANSLUCENT_GAUGE;
  g_FreeLookHeading = g_CurrentShot.heading;
  SetMenuBackdrop(MENU_BACKDROP_PATTERN);
  g_PauseScreen = PAUSE_SCREEN_FREE_LOOK;
  g_RoundHud.isLetterboxShown = FALSE;
  g_RoundHud.unkC = 0;
  if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
    SetFramebufferCount(TRIPLE_BUFFERING);
    SetScreenEffect(SCREEN_EFFECT_NONE, 0);
  }

  StopSound(IMPACT_POINT_SOUND_CHANNEL);
}

void UpdateImpactPointMode(void) {
  if ((g_TurnController.buttons & IMPACT_POINT_BUTTONS) &&
      !(g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT)) {
    if (g_ImpactPointState == IMPACT_POINT_OFF &&
        f_ShotSource == SHOT_SOURCE_PLAYER) {
      PlaySoundWithEffects(SOUND_IMPACT_POINT, IMPACT_POINT_SOUND_CHANNEL,
          IMPACT_POINT_SOUND_VOLUME, IMPACT_POINT_SOUND_REVERB,
          IMPACT_POINT_SOUND_PAN);
    }

    g_ImpactPointState = IMPACT_POINT_SETTING;
  } else {
    StopSound(IMPACT_POINT_SOUND_CHANNEL);
    g_ImpactPointState = IMPACT_POINT_OFF;
  }
}

void ResetAimCamera(void) {
  s32 unused[4];

  g_CurrentShot.aimCameraDistance = AIM_CAMERA_START_DISTANCE;
  g_CurrentShot.aimCameraHeight = AIM_CAMERA_START_HEIGHT;
  D_800C3070 = 0;
  g_ImpactPointLingerFrameCount = 0;
  PlaceAimCamera();
}

/* Eases the aim camera towards the place that suits the shot, and stops once it
 * is close */
#ifdef NON_MATCHING
void EaseAimCamera(void) {
  s32 targetDistance;
  s32 targetHeight;

  if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
    targetDistance = AIM_CAMERA_PUTT_DISTANCE;
  } else if (g_ShotFlags & SHOT_FLAG_APPROACH) {
    targetDistance = g_CurrentShot.cupDistance - AIM_CAMERA_APPROACH_BACKOFF;
  } else {
    targetDistance =
        (GetSelectedShotDistance() - AIM_CAMERA_SHOT_BACKOFF_YARDS) *
        UNITS_PER_YARD;
  }

  if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
    targetHeight = AIM_CAMERA_PUTT_HEIGHT;
  } else if (g_ShotFlags & SHOT_FLAG_APPROACH) {
    targetHeight = AIM_CAMERA_APPROACH_HEIGHT;
  } else {
    targetHeight = AIM_CAMERA_SHOT_HEIGHT;
  }

  if (fabsf(g_CurrentShot.aimCameraDistance - targetDistance) >
      AIM_CAMERA_MAX_EASE_STEP) {
    if (g_CurrentShot.aimCameraDistance > targetDistance) {
      g_CurrentShot.aimCameraDistance -= AIM_CAMERA_MAX_EASE_STEP;
    } else {
      g_CurrentShot.aimCameraDistance += AIM_CAMERA_MAX_EASE_STEP;
    }
  } else {
    g_CurrentShot.aimCameraDistance +=
        (targetDistance - g_CurrentShot.aimCameraDistance) *
        AIM_CAMERA_EASE_FRACTION;
  }

  g_CurrentShot.aimCameraHeight +=
      (targetHeight - g_CurrentShot.aimCameraHeight) * AIM_CAMERA_EASE_FRACTION;
  if (fabsf(g_CurrentShot.aimCameraDistance - targetDistance) <
      UNITS_PER_YARD) {
    D_800C3070 = 0;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", EaseAimCamera);
#endif

/* Raises or lowers the aim camera with the C buttons, in smaller steps for a
 * putt */
void MoveAimCameraHeightByButtons(void) {
  if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
    if (g_TurnController.buttons & D_CBUTTONS) {
      g_CurrentShot.aimCameraHeight += AIM_CAMERA_BUTTON_PUTT_STEP;
    } else if (g_TurnController.buttons & U_CBUTTONS) {
      g_CurrentShot.aimCameraHeight -= AIM_CAMERA_BUTTON_PUTT_STEP;
    }
  } else {
    if (g_TurnController.buttons & D_CBUTTONS) {
      g_CurrentShot.aimCameraHeight += AIM_CAMERA_BUTTON_HEIGHT_STEP;
    } else if (g_TurnController.buttons & U_CBUTTONS) {
      g_CurrentShot.aimCameraHeight -= AIM_CAMERA_BUTTON_HEIGHT_STEP;
    }
  }
}

/* Turns the free-look camera with C-left and C-right */
void TurnFreeLook(void) {
  if (g_TurnController.buttons & L_CBUTTONS) {
    g_FreeLookHeading -= FREE_LOOK_TURN_STEP;
  } else if (g_TurnController.buttons & R_CBUTTONS) {
    g_FreeLookHeading += FREE_LOOK_TURN_STEP;
  }
}

/* Swings the aim camera up and back, or down and forward, with C-right and
 * C-left */
void CraneAimCamera(void) {
  s32 stepCount;

  if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
    if (g_CurrentShot.shotMode == PUTT_MODE_SHORT) {
      stepCount = SHORT_PUTT_CRANE_STEP_COUNT;
    } else if (g_CurrentShot.shotMode == PUTT_MODE_MIDDLE) {
      stepCount = MIDDLE_PUTT_CRANE_STEP_COUNT;
    } else {
      stepCount = LONG_PUTT_CRANE_STEP_COUNT;
    }

    if (g_TurnController.buttons & L_CBUTTONS) {
      g_CurrentShot.aimCameraHeight += stepCount * AIM_CAMERA_PUTT_STEP_UNIT;
    } else if (g_TurnController.buttons & R_CBUTTONS) {
      g_CurrentShot.aimCameraHeight -= stepCount * AIM_CAMERA_PUTT_STEP_UNIT;
    }
  } else if (g_TurnController.buttons & L_CBUTTONS) {
    g_CurrentShot.aimCameraHeight += AIM_CAMERA_BUTTON_HEIGHT_STEP;
    if (g_CurrentShot.aimCameraHeight < AIM_CAMERA_CRANE_FORWARD_HEIGHT) {
      g_CurrentShot.aimCameraDistance += AIM_CAMERA_BUTTON_HEIGHT_STEP;
    }
  } else if (g_TurnController.buttons & R_CBUTTONS) {
    g_CurrentShot.aimCameraHeight -= AIM_CAMERA_BUTTON_HEIGHT_STEP;
    if (g_CurrentShot.aimCameraHeight > AIM_CAMERA_CRANE_BACK_HEIGHT) {
      g_CurrentShot.aimCameraDistance -= AIM_CAMERA_BUTTON_HEIGHT_STEP;
    }
  }
}

/* Moves the aim camera along the aim line with the C buttons, in smaller steps
 * for a putt */
void MoveAimCameraDistanceByButtons(void) {
  if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
    if (g_TurnController.buttons & R_CBUTTONS) {
      g_CurrentShot.aimCameraDistance += AIM_CAMERA_BUTTON_PUTT_STEP;
    } else if (g_TurnController.buttons & L_CBUTTONS) {
      g_CurrentShot.aimCameraDistance -= AIM_CAMERA_BUTTON_PUTT_STEP;
    }
  } else {
    if (g_TurnController.buttons & R_CBUTTONS) {
      g_CurrentShot.aimCameraDistance += AIM_CAMERA_BUTTON_DISTANCE_STEP;
    } else if (g_TurnController.buttons & L_CBUTTONS) {
      g_CurrentShot.aimCameraDistance -= AIM_CAMERA_BUTTON_DISTANCE_STEP;
    }
  }
}

/* Moves the aim camera along the aim line with C-up and C-down, by steps that
 * suit a putt's length */
void MoveAimCameraDistanceByUpDown(void) {
  s32 stepCount;

  if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
    if (g_CurrentShot.shotMode == PUTT_MODE_SHORT) {
      stepCount = SHORT_PUTT_DOLLY_STEP_COUNT;
    } else if (g_CurrentShot.shotMode == PUTT_MODE_MIDDLE) {
      stepCount = MIDDLE_PUTT_DOLLY_STEP_COUNT;
    } else {
      stepCount = LONG_PUTT_DOLLY_STEP_COUNT;
    }

    if (g_TurnController.buttons & U_CBUTTONS) {
      g_CurrentShot.aimCameraDistance += stepCount * AIM_CAMERA_PUTT_STEP_UNIT;
    } else if (g_TurnController.buttons & D_CBUTTONS) {
      g_CurrentShot.aimCameraDistance -= stepCount * AIM_CAMERA_PUTT_STEP_UNIT;
    }
  } else {
    if (g_TurnController.buttons & U_CBUTTONS) {
      g_CurrentShot.aimCameraDistance += AIM_CAMERA_BUTTON_DISTANCE_STEP;
    } else if (g_TurnController.buttons & D_CBUTTONS) {
      g_CurrentShot.aimCameraDistance -= AIM_CAMERA_BUTTON_DISTANCE_STEP;
    }
  }
}

/* Raises or lowers the aim camera with the stick's Y */
void MoveAimCameraHeight(void) {
  if (Abs(g_TurnController.stickY) > AIM_STICK_DEAD_ZONE) {
    g_CurrentShot.aimCameraHeight -=
        g_TurnController.stickY / AIM_STICK_STEP * AIM_CAMERA_HEIGHT_STEP;
  }
}

/* Moves the aim camera along the aim line with the stick's Y, in smaller steps
 * for a putt */
void MoveAimCameraDistance(void) {
  if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
    if (Abs(g_TurnController.stickY) > AIM_STICK_DEAD_ZONE) {
      g_CurrentShot.aimCameraDistance += g_TurnController.stickY /
          AIM_DISTANCE_STICK_STEP * AIM_CAMERA_PUTT_DISTANCE_STEP;
    }
  } else {
    if (Abs(g_TurnController.stickY) > AIM_STICK_DEAD_ZONE) {
      g_CurrentShot.aimCameraDistance += g_TurnController.stickY /
          AIM_DISTANCE_STICK_STEP * AIM_CAMERA_DISTANCE_STEP;
    }
  }
}

/*
 * Turns the aim with the stick, or g_AltHeading instead when asked, and the
 * shot's heading with the D-pad, faster the longer it is held. While the
 * heading holds still, the scene is drawn from a saved frame, through two
 * framebuffers.
 */
#ifdef NON_MATCHING
void TurnAim(s32 turnsAltHeading) {
  s32 stickX = g_TurnController.stickX;
  SignedBinaryAngle heading = g_CurrentShot.heading;
  s32 speed;
  s32 step;
  s32 dpad;
  s32 dpadSpeed;

  if (g_ImpactPointState != IMPACT_POINT_OFF) {
    return;
  }

  if (!turnsAltHeading) {
    step = stickX / 2;
    speed = stickX / AIM_STICK_SPEED_DIVISOR;
    if (speed < 0) {
      speed = -speed;
    }

    if (speed != 0) {
      step *= speed;
    }

    g_CurrentShot.heading -= step;
  } else {
    step = stickX / 2;
    speed = stickX / ALT_HEADING_STICK_SPEED_DIVISOR;
    if (speed < 0) {
      speed = -speed;
    }

    if (speed != 0) {
      step *= speed;
    }

    g_AltHeading -= step;
  }

  dpad = g_TurnController.buttons & (L_JPAD | R_JPAD);
  if (dpad == 0 || dpad == (L_JPAD | R_JPAD)) {
    f_AimTurnFrameCount = 0;
  } else if (dpad & L_JPAD) {
    s32* dpadFrames = &f_AimTurnFrameCount;

    if (*dpadFrames < 0) {
      *dpadFrames = 0;
    }

    if (++*dpadFrames > AIM_DPAD_MAX_FRAMES) {
      *dpadFrames = AIM_DPAD_MAX_FRAMES;
    }
  } else {
    s32* dpadFrames = &f_AimTurnFrameCount;

    if (*dpadFrames > 0) {
      *dpadFrames = 0;
    }

    if (--*dpadFrames < -AIM_DPAD_MAX_FRAMES) {
      *dpadFrames = -AIM_DPAD_MAX_FRAMES;
    }
  }

  dpad = f_AimTurnFrameCount;
  dpadSpeed = (dpad < 0) ? -dpad : dpad;
  g_CurrentShot.heading +=
      dpad * ((dpadSpeed / AIM_DPAD_SPEED_DIVISOR) + AIM_DPAD_BASE_SPEED);
  if (!IsGameFlagSet(GAME_FLAG_RANDOM_FRAMEBUFFER_COUNT)) {
    if (g_LetterboxState == LETTERBOX_STATE_NONE ||
        g_LetterboxState == LETTERBOX_STATE_UNK5) {
      if (heading == (s16)g_CurrentShot.heading) {
        if (g_ScreenEffect != SCREEN_EFFECT_SAVED_FRAME) {
          SetScreenEffect(SCREEN_EFFECT_SAVED_FRAME, SAVED_FRAME_STEP_SAVING);
          SetFramebufferCount(DOUBLE_BUFFERING);
        }
      } else if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
        SetFramebufferCount(TRIPLE_BUFFERING);
        SetScreenEffect(SCREEN_EFFECT_NONE, 0);
      }
    } else {
      SetFramebufferCount(TRIPLE_BUFFERING);
    }
  }

  if (IsGameFlagSet(GAME_FLAG_RANDOM_FRAMEBUFFER_COUNT)) {
    if (rand() & 1) {
      SetFramebufferCount(TRIPLE_BUFFERING);
    } else {
      SetFramebufferCount(DOUBLE_BUFFERING);
    }
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", TurnAim);
#endif

/*
 * Handles the aim's buttons for a frame: switching between looking down the
 * range, at the cup and back from past the ball, and moving the camera. A
 * replay instead turns the aim to the recorded heading, and returns TRUE once
 * it has settled there for a while.
 */
#ifdef NON_MATCHING
s32 UpdateAimControls(void) {
  Vec3 toCup;
  s32 headingLeft;
  s32 step;
  s32 maxDistance;
  s32 turn;
  s32 heading;

  if (f_ShotSource != SHOT_SOURCE_PLAYER) {
    headingLeft = (s16)(D_80104F78.heading - g_CurrentShot.heading);
    if (IsGameFlagSet(GAME_FLAG_SNAP_REPLAY_AIM) ||
        Abs(headingLeft) < REPLAY_AIM_MIN_STEP) {
      g_CurrentShot.heading = D_80104F78.heading;
      if (g_ScreenEffect != SCREEN_EFFECT_SAVED_FRAME) {
        SetScreenEffect(SCREEN_EFFECT_SAVED_FRAME, SAVED_FRAME_STEP_SAVING);
        SetFramebufferCount(DOUBLE_BUFFERING);
      }

      return ++f_ReplayAimSettledFrameCount >= REPLAY_AIM_SETTLE_FRAMES;
    }

    step = headingLeft / REPLAY_AIM_STEP_DIVISOR;
    turn = Abs(step) + REPLAY_AIM_MIN_STEP;
    heading = g_CurrentShot.heading;
    g_CurrentShot.heading = (headingLeft < 0) ? heading - turn : heading + turn;
    if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
      SetFramebufferCount(TRIPLE_BUFFERING);
      SetScreenEffect(SCREEN_EFFECT_NONE, 0);
    }

    return FALSE;
  }

  g_HasPressedShotButton = FALSE;
  if (g_TurnController.buttons == AIM_RESET_BUTTONS) {
    g_CurrentShot.heading = atan2(g_HoleSetup.cupPosition[AXIS_Z] -
                                    g_CurrentShot.ballPosition[AXIS_Z],
                                g_HoleSetup.cupPosition[AXIS_X] -
                                    g_CurrentShot.ballPosition[AXIS_X]) *
        RADIANS_TO_BINARY_ANGLE;
  }

  if (IsGameFlagSet(GAME_FLAG_AIM_DOWN_RANGE)) {
    switch (g_LetterboxState) {
      case LETTERBOX_STATE_NONE:
        if ((g_TurnController.pressedButtons & A_BUTTON) &&
            IsShotModeChangeDone()) {
          g_HasPressedShotButton = TRUE;
          ResetAimCamera();
        } else if (g_TurnController.pressedButtons & R_TRIG) {
          PlaySound(
              SOUND_LOOK_DOWN_RANGE, g_NextSoundChannel, LOOK_SOUND_VOLUME);
          if (g_ShotFlags & SHOT_FLAG_NEAR_CUP) {
            g_LetterboxState = LETTERBOX_STATE_STARTING_LOOK_BACK;
            toCup[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X] -
                g_CurrentShot.ballPosition[AXIS_X];
            toCup[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z] -
                g_CurrentShot.ballPosition[AXIS_Z];
            g_AltHeading = (atan2(toCup[AXIS_Z], toCup[AXIS_X]) + M_PI) *
                RADIANS_TO_BINARY_ANGLE;
            g_CurrentShot.aimCameraDistance =
                -sqrtf((toCup[AXIS_X] * toCup[AXIS_X]) +
                    (toCup[AXIS_Z] * toCup[AXIS_Z])) -
                LOOK_BACK_EXTRA_DISTANCE;
            g_CurrentShot.aimCameraHeight = LOOK_BACK_START_HEIGHT;
          } else {
            g_LetterboxState = LETTERBOX_STATE_LOOKING_DOWN_RANGE;
            D_800C3070 = 1;
          }
        } else if (g_TurnController.pressedButtons & B_BUTTON) {
          AimDownRange();
          g_LetterboxState = LETTERBOX_STATE_MOVING_CAMERA;
        }

        if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
          TurnAim(0);
        }

        break;
      case LETTERBOX_STATE_MOVING_CAMERA:
        if (g_TurnController.pressedButtons & A_BUTTON) {
          g_LetterboxState = LETTERBOX_STATE_NONE;
          ResetAimCamera();
        } else if (g_TurnController.pressedButtons & R_TRIG) {
          g_LetterboxState = LETTERBOX_STATE_NONE;
          ResetAimCamera();
        } else if (g_TurnController.pressedButtons & B_BUTTON) {
          g_RoundHud.greenGridState++;
        }

        TurnAim(0);
        break;
      case LETTERBOX_STATE_LOOKING_DOWN_RANGE:
        D_800C3070 = 1;
        if (g_TurnController.pressedButtons & A_BUTTON) {
          g_LetterboxState = LETTERBOX_STATE_NONE;
          D_800C3070 = 0;
          ResetAimCamera();
        } else if (g_TurnController.pressedButtons & R_TRIG) {
          PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
          g_LetterboxState = LETTERBOX_STATE_NONE;
          D_800C3070 = 0;
          ResetAimCamera();
        } else if (g_TurnController.pressedButtons & B_BUTTON) {
          g_RoundHud.greenGridState++;
        }

        TurnAim(0);
        break;
      case LETTERBOX_STATE_STARTING_LOOK_BACK:
      case LETTERBOX_STATE_LOOKING_BACK:
        if (g_TurnController.pressedButtons & A_BUTTON) {
          g_LetterboxState = LETTERBOX_STATE_NONE;
          ResetAimCamera();
        } else if (g_TurnController.pressedButtons & R_TRIG) {
          PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
          g_LetterboxState = LETTERBOX_STATE_NONE;
          ResetAimCamera();
        } else if (g_TurnController.pressedButtons & B_BUTTON) {
          g_RoundHud.greenGridState++;
        }

        TurnAim(0);
        break;
    }

    if (g_LetterboxState == LETTERBOX_STATE_NONE) {
      g_ImpactPointState = IMPACT_POINT_SETTING;
    } else {
      g_ImpactPointState = IMPACT_POINT_OFF;
    }
  } else {
    if (f_ShotSource == SHOT_SOURCE_PLAYER) {
      if ((g_TurnController.buttons & IMPACT_POINT_BUTTONS) &&
          g_PauseScreen != PAUSE_SCREEN_FREE_LOOK &&
          !(g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT)) {
        if (g_ImpactPointState != IMPACT_POINT_SETTING) {
          PlaySoundWithEffects(SOUND_IMPACT_POINT, IMPACT_POINT_SOUND_CHANNEL,
              IMPACT_POINT_SOUND_VOLUME, IMPACT_POINT_SOUND_REVERB,
              IMPACT_POINT_SOUND_PAN);
        }

        g_ImpactPointState = IMPACT_POINT_SETTING;
        g_ImpactPointLingerFrameCount = 0;
      } else {
        StopSound(IMPACT_POINT_SOUND_CHANNEL);
        if (g_ImpactPointState != IMPACT_POINT_OFF) {
          g_ImpactPointState = IMPACT_POINT_LINGERING;
        }

        if (g_ImpactPointLingerFrameCount < IMPACT_POINT_LINGER_FRAMES) {
          g_ImpactPointLingerFrameCount++;
        } else {
          g_ImpactPointState = IMPACT_POINT_OFF;
        }
      }
    }

    switch (g_LetterboxState) {
      case LETTERBOX_STATE_NONE:
      case LETTERBOX_STATE_UNK5:
        if ((g_TurnController.pressedButtons & A_BUTTON) &&
            IsShotModeChangeDone()) {
          g_HasPressedShotButton = TRUE;
          ResetAimCamera();
        } else if (g_TurnController.pressedButtons & R_TRIG) {
          PlaySound(SOUND_LOOK_AT_CUP, g_NextSoundChannel, LOOK_SOUND_VOLUME);
          if ((g_ShotFlags &
                  (SHOT_FLAG_NEAR_CUP | SHOT_FLAG_NO_IMPACT_POINT)) ==
              (SHOT_FLAG_NEAR_CUP | SHOT_FLAG_NO_IMPACT_POINT)) {
            toCup[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X] -
                g_CurrentShot.ballPosition[AXIS_X];
            toCup[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z] -
                g_CurrentShot.ballPosition[AXIS_Z];
            g_LetterboxState = LETTERBOX_STATE_STARTING_LOOK_BACK;
            g_CurrentShot.aimCameraHeight = LOOK_BACK_START_HEIGHT;
            g_AltHeading = g_CurrentShot.heading;
            g_CurrentShot.aimCameraDistance =
                -sqrtf((toCup[AXIS_X] * toCup[AXIS_X]) +
                    (toCup[AXIS_Z] * toCup[AXIS_Z])) -
                LOOK_BACK_EXTRA_DISTANCE;
          } else {
            g_LetterboxState = LETTERBOX_STATE_LOOKING_DOWN_RANGE;
            D_800C3070 = 1;
          }
        }

        if (g_LetterboxState == LETTERBOX_STATE_NONE) {
          TurnAim(0);
        }

        break;
      case LETTERBOX_STATE_MOVING_CAMERA:
        if (g_TurnController.pressedButtons & A_BUTTON) {
          g_LetterboxState = LETTERBOX_STATE_NONE;
          PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
          ResetAimCamera();
          g_ShotSetupFrameCount = 0;
        } else if (g_TurnController.pressedButtons & R_TRIG) {
          g_LetterboxState = LETTERBOX_STATE_NONE;
          g_ShotSetupFrameCount = 0;
          PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
          ResetAimCamera();
        }

        TurnAim(0);
        break;
      case LETTERBOX_STATE_LOOKING_DOWN_RANGE:
        D_800C3070 = 1;
        if (g_TurnController.pressedButtons & A_BUTTON) {
          g_LetterboxState = LETTERBOX_STATE_NONE;
          g_ShotSetupFrameCount = 0;
          D_800C3070 = 0;
          ResetAimCamera();
          PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
        } else if (g_TurnController.pressedButtons & R_TRIG) {
          PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
          g_ShotSetupFrameCount = 0;
          g_LetterboxState = LETTERBOX_STATE_NONE;
          D_800C3070 = 0;
          ResetAimCamera();
        }

        TurnAim(0);
        break;
      case LETTERBOX_STATE_STARTING_LOOK_BACK:
        if (g_TurnController.pressedButtons & A_BUTTON) {
          g_LetterboxState = LETTERBOX_STATE_NONE;
          g_ShotSetupFrameCount = 0;
          ResetAimCamera();
          PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
        } else if (g_TurnController.pressedButtons & R_TRIG) {
          g_ShotSetupFrameCount = 0;
          PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
          g_LetterboxState = LETTERBOX_STATE_NONE;
          ResetAimCamera();
        }

        TurnAim(0);
        g_AltHeading = g_CurrentShot.heading;
        break;
      case LETTERBOX_STATE_LOOKING_BACK:
        if (g_TurnController.pressedButtons & A_BUTTON) {
          g_LetterboxState = LETTERBOX_STATE_NONE;
          g_ShotSetupFrameCount = 0;
          ResetAimCamera();
          PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
        } else if (g_TurnController.pressedButtons & R_TRIG) {
          PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
          g_LetterboxState = LETTERBOX_STATE_NONE;
          ResetAimCamera();
          g_ShotSetupFrameCount = 0;
        }

        TurnAim(0);
        g_AltHeading = g_CurrentShot.heading;
        break;
    }
  }

  if (g_RoundHud.greenGridState >= AIM_VIEW_TOGGLE_COUNT) {
    g_RoundHud.greenGridState = 0;
  }

  if (g_HasPressedShotButton != TRUE) {
    if (g_TurnController.buttons & AIM_CAMERA_BUTTONS) {
      if (g_LetterboxState == LETTERBOX_STATE_NONE ||
          g_LetterboxState == LETTERBOX_STATE_UNK5 ||
          g_LetterboxState == LETTERBOX_STATE_LOOKING_DOWN_RANGE) {
        g_LetterboxState = LETTERBOX_STATE_MOVING_CAMERA;
        D_800C3070 = 0;
      }

      if (g_LetterboxState == LETTERBOX_STATE_STARTING_LOOK_BACK) {
        g_LetterboxState = LETTERBOX_STATE_LOOKING_BACK;
        D_800C3070 = 0;
      }
    }

    if (g_TurnController.pressedButtons & AIM_CAMERA_RAISE_BUTTONS) {
      PlaySound(
          SOUND_PROMPT_CLOSE, g_NextSoundChannel, AIM_CAMERA_SOUND_VOLUME);
    }

    if (g_TurnController.pressedButtons & AIM_CAMERA_LOWER_BUTTONS) {
      PlaySound(SOUND_PROMPT_OPEN, g_NextSoundChannel, AIM_CAMERA_SOUND_VOLUME);
    }

    if (D_800C3070 == 1) {
      EaseAimCamera();
    } else {
      if (g_PauseScreen == PAUSE_SCREEN_FREE_LOOK) {
        TurnFreeLook();
      } else {
        CraneAimCamera();
      }

      MoveAimCameraDistanceByUpDown();
    }

    if (g_PauseScreen != PAUSE_SCREEN_FREE_LOOK &&
        (g_TurnController.pressedButtons & IMPACT_POINT_BUTTONS) &&
        !(g_ShotFlags & SHOT_FLAG_ON_GREEN)) {
      SpawnRisingClumps(g_CurrentShot.ballPosition);
    }
  }

  if (g_LetterboxState == LETTERBOX_STATE_STARTING_LOOK_BACK) {
    g_LetterboxState = LETTERBOX_STATE_LOOKING_BACK;
  }

  if (g_LetterboxState == LETTERBOX_STATE_LOOKING_BACK) {
    if (g_CurrentShot.aimCameraHeight > GREEN_AIM_CAMERA_MIN_HEIGHT) {
      g_CurrentShot.aimCameraHeight = GREEN_AIM_CAMERA_MIN_HEIGHT;
    }

    if (g_CurrentShot.aimCameraHeight < AIM_CAMERA_MAX_HEIGHT) {
      g_CurrentShot.aimCameraHeight = AIM_CAMERA_MAX_HEIGHT;
    }

    if (g_CurrentShot.aimCameraDistance < LOOK_BACK_MIN_DISTANCE) {
      g_CurrentShot.aimCameraDistance = LOOK_BACK_MIN_DISTANCE;
    }

    maxDistance = LOOK_BACK_MAX_DISTANCE;
    if (g_CurrentShot.aimCameraDistance > maxDistance) {
      g_CurrentShot.aimCameraDistance = maxDistance;
    }
  } else {
    if (g_ShotFlags & SHOT_FLAG_ON_GREEN) {
      if (g_CurrentShot.aimCameraHeight > GREEN_AIM_CAMERA_MIN_HEIGHT) {
        g_CurrentShot.aimCameraHeight = GREEN_AIM_CAMERA_MIN_HEIGHT;
      }
    } else if (g_CurrentShot.aimCameraHeight > AIM_CAMERA_MIN_HEIGHT) {
      g_CurrentShot.aimCameraHeight = AIM_CAMERA_MIN_HEIGHT;
    }

    if (g_CurrentShot.aimCameraHeight < AIM_CAMERA_MAX_HEIGHT) {
      g_CurrentShot.aimCameraHeight = AIM_CAMERA_MAX_HEIGHT;
    }

    if (g_CurrentShot.aimCameraDistance < AIM_CAMERA_MIN_DISTANCE) {
      g_CurrentShot.aimCameraDistance = AIM_CAMERA_MIN_DISTANCE;
    }

    maxDistance = AIM_CAMERA_MAX_DISTANCE;
    if (g_CurrentShot.aimCameraDistance > maxDistance) {
      g_CurrentShot.aimCameraDistance = maxDistance;
    }
  }

  g_Cameras[1].view.eye[AXIS_X] = g_Cameras[0].view.eye[AXIS_X];
  g_Cameras[1].view.eye[AXIS_Y] = g_Cameras[0].view.eye[AXIS_Y];
  g_Cameras[1].view.eye[AXIS_Z] = g_Cameras[0].view.eye[AXIS_Z];
  g_Cameras[1].view.lookAt[AXIS_X] = g_Cameras[0].view.lookAt[AXIS_X];
  g_Cameras[1].view.lookAt[AXIS_Y] = g_Cameras[0].view.lookAt[AXIS_Y];
  g_Cameras[1].view.lookAt[AXIS_Z] = g_Cameras[0].view.lookAt[AXIS_Z];
  return FALSE;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", UpdateAimControls);
#endif

void UpdateShotFlags(void) {
  s32 surfaceMask = GetSelectedClub()->surfaceMask;
  s8* isMiniGolf;

  g_ShotFlags = 0;
  if (surfaceMask & CLUB_SURFACE_GREEN) {
    g_ShotFlags = SHOT_FLAG_NO_IMPACT_POINT;
  }

  isMiniGolf = &g_GameState.isMiniGolf;

  if (*isMiniGolf) {
    g_ShotFlags |= SHOT_FLAG_NO_IMPACT_POINT;
  }

  if (GetSurfaceInfo(g_CurrentShot.surface)->kind == SURFACE_KIND_GREEN) {
    g_ShotFlags |= SHOT_FLAG_ON_GREEN;
  }

  if (*isMiniGolf) {
    g_ShotFlags |= SHOT_FLAG_ON_GREEN | SHOT_FLAG_NO_IMPACT_POINT;
  }

  if (g_CurrentShot.shotMode == SHOT_MODE_APPROACH) {
    g_ShotFlags |= SHOT_FLAG_APPROACH;
  }

  if (g_CurrentShot.cupDistance * DECIMETERS_PER_UNIT <=
      NEAR_CUP_MAX_DECIMETERS) {
    g_ShotFlags |= SHOT_FLAG_NEAR_CUP;
  }
}

void StartTurnTimer(void) {
  f_TurnTimerStart = osGetCount();
}

/*
 * Always FALSE, so UpdateTurns stops polling the computer golfer's plan
 * after one try. It gets a time budget after StartTurnTimer, so it
 * presumably once told whether the frame had time left for planning.
 */
Boolean HasPlanTime(s32 timeBudget) {
  return FALSE;
}

s32 MarkUsableClubs(s32 unused, s16* clubs, s8* isUsable);

/*
 * Picks the club and the shot mode that the shot starts with. On a green, it is
 * the putter, in the shortest putt mode that reaches the cup. Elsewhere, it is
 * the club whose normal shot comes nearest the cup, out of those with a good
 * enough lie, and preferring those that reach.
 */
#ifdef NON_MATCHING
void PickStartingClub(HoleSetup* hole) {
  s8 isUsable[CLUB_SLOT_COUNT];
  s32 clubCount;
  s32 cupDistance;
  s32 slot;
  s32 rating;
  s32 minRating = 0;
  Club* club;

  g_CurrentShot.clubSlot = 0;
  g_CurrentShot.shotMode = SHOT_MODE_NORMAL;
  clubCount = MarkUsableClubs((s32)GetSurfaceInfo(g_CurrentShot.surface),
      g_Players[g_CurrentShot.playerIndex].clubs, isUsable);
  cupDistance = GetCupDistance(
      g_CurrentShot.ballPosition[AXIS_X] - hole->cupPosition[AXIS_X],
      g_CurrentShot.ballPosition[AXIS_Z] - hole->cupPosition[AXIS_Z]);
  if (g_GameState.isMiniGolf ||
      GetSurfaceInfo(g_CurrentShot.surface)->kind == SURFACE_KIND_GREEN) {
    for (slot = 0; slot < clubCount; slot++) {
      if (!isUsable[slot]) {
        continue;
      }

      club = GetClub(g_Players[g_CurrentShot.playerIndex].clubs[slot]);
      if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
        continue;
      }

      if (GetShotWorldDistance(club, PUTT_MODE_SHORT) >= cupDistance) {
        g_CurrentShot.shotMode = PUTT_MODE_SHORT;
      } else if (g_CurrentShot.unk54 &&
          GetShotWorldDistance(club, PUTT_MODE_MIDDLE) < cupDistance) {
        g_CurrentShot.shotMode = PUTT_MODE_LONG;
      } else {
        g_CurrentShot.shotMode = PUTT_MODE_MIDDLE;
      }

      g_CurrentShot.clubSlot = slot;
      break;
    }
  } else {
    s32 bestGap = 0x7FFFFFFF;
    s32 reaches = FALSE;
    s32 bestSlot = 0;
    s32 distance;
    s32 gap;

    for (slot = 0; slot < clubCount; slot++) {
      rating = GetLieRating(
          GetClub(g_Players[g_CurrentShot.playerIndex].clubs[slot]),
          g_CurrentShot.surface, SHOT_MODE_NORMAL);
      if (minRating < rating) {
        minRating = rating;
      }

      if (rating >= STARTING_CLUB_MIN_LIE_RATING) {
        break;
      }
    }

    if (minRating > STARTING_CLUB_MIN_LIE_RATING) {
      minRating = STARTING_CLUB_MIN_LIE_RATING;
    }

    for (slot = 0; slot < clubCount; slot++) {
      if (!isUsable[slot]) {
        continue;
      }

      club = GetClub(g_Players[g_CurrentShot.playerIndex].clubs[slot]);
      if (club->surfaceMask & CLUB_SURFACE_GREEN) {
        continue;
      }

      rating = GetLieRating(club, g_CurrentShot.surface, SHOT_MODE_NORMAL);
      distance = GetShotDistance(club, SHOT_MODE_NORMAL) * UNITS_PER_YARD;
      if (rating < minRating) {
        continue;
      }

      gap = distance - cupDistance;
      if (Abs(gap) < bestGap && (!reaches || distance >= cupDistance)) {
        bestSlot = slot;
        bestGap = Abs(gap);
        if (distance >= cupDistance) {
          reaches = TRUE;
        }
      }
    }

    g_CurrentShot.shotMode = SHOT_MODE_NORMAL;
    g_CurrentShot.clubSlot = bestSlot;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", PickStartingClub);
#endif

/* Puts the simulated ball at rest on the player's ball, and shows the player's
 * passed rings again */
void PlaceBallAtRest(s32 playerIndex, BallSimulation* simulation) {
  UnkB8* player = &g_Players[playerIndex];
  s32 i;
  s32 groundHeight;

  StopSound(IMPACT_POINT_SOUND_CHANNEL);
  for (i = 0; i < RING_COUNT; i++) {
    if (player->ringStates[i] != RING_STATE_NONE) {
      player->ringStates[i] = RING_STATE_SHOWN;
    }

    func_8006034C(0);
  }

  player->unkA8 = 0;
  simulation->position[AXIS_X] = player->ballPosition[AXIS_X];
  simulation->position[AXIS_Y] = player->ballPosition[AXIS_Y];
  simulation->position[AXIS_Z] = player->ballPosition[AXIS_Z];
  simulation->playerIndex = playerIndex;
  simulation->isHoledOut = 0;
  simulation->state = FALSE;
  groundHeight = GetGroundHeight(
      simulation->position[AXIS_X], simulation->position[AXIS_Z]);
  if (groundHeight < simulation->position[AXIS_Y]) {
    simulation->position[AXIS_Y] = groundHeight;
  }

  simulation->surface = player->ballSurface;
  simulation->purpose = LAUNCH_PURPOSE_UNK3;
  simulation->unkA1 = 0;
  PrepareShotEffects(simulation);
  if (g_IsModuleAlive[MODULE_GOLF_BALLS] ||
      ReportModuleNotAlive(MODULE_GOLF_BALLS)) {
    ResetBallAfterimage();
  }
}

/* Counts a notable shot towards the player's Course Points */
void CountNotableShot(s32 playerIndex, s32 shotKind) {
  u8* count;

  if (g_Players[playerIndex].isComputer ||
      g_GameState.replayKind != REPLAY_KIND_NONE ||
      g_GameState.gameMode == GAME_MODE_TRAINING) {
    return;
  }

  if (g_GameState.gameMode == GAME_MODE_RING_SHOT ||
      f_ShotFlightCount >= MAX_COUNTED_FLIGHT_COUNT) {
    return;
  }

  if (g_GameState.isMiniGolf && shotKind != SHOT_KIND_HOLE_IN_ONE) {
    return;
  }

  count = (u8*)&g_Players[playerIndex].unkAA[shotKind];
  if (*count < SHOT_KIND_MAX_COUNT) {
    osSyncPrintf("EXP GET type %d\n", shotKind);
    (*count)++;
  }
}

/* The course points that each shot of a kind earns */
/* A great or an excellent shot */
#define GREAT_SHOT_POINTS 2
#define PIN_SHOT_POINTS 4
#define CHIP_IN_POINTS 6
#define LONG_CHIP_IN_POINTS 8
/* A flag shot, an eagle or a water cut */
#define BIG_SHOT_POINTS 10
#define ALBATROSS_POINTS 16
#define HOLE_IN_ONE_POINTS 20
#define MINI_GOLF_HOLE_IN_ONE_POINTS 10

/* The course points that the player has earned with the shots of one kind */
s32 GetShotKindPoints(s32 playerIndex, s32 shotKind) {
  s32 count = (u8)g_Players[playerIndex].unkAA[shotKind];
  s32 points;

  switch (shotKind) {
    case SHOT_KIND_GREAT:
    case SHOT_KIND_EXCELLENT:
      points = GREAT_SHOT_POINTS;
      break;
    case SHOT_KIND_LONG_CHIP_IN:
      points = LONG_CHIP_IN_POINTS;
      break;
    case SHOT_KIND_PIN_SHOT:
    case SHOT_KIND_UNK5:
      points = PIN_SHOT_POINTS;
      break;
    case SHOT_KIND_HOLE_IN_ONE:
      if (g_GameState.isMiniGolf) {
        points = MINI_GOLF_HOLE_IN_ONE_POINTS;
      } else {
        points = HOLE_IN_ONE_POINTS;
      }

      break;
    case SHOT_KIND_FLAG_SHOT:
    case SHOT_KIND_EAGLE:
    case SHOT_KIND_WATER_CUT:
      points = BIG_SHOT_POINTS;
      break;
    case SHOT_KIND_ALBATROSS:
      points = ALBATROSS_POINTS;
      break;
    case SHOT_KIND_CHIP_IN:
    case SHOT_KIND_BIRDIE:
      points = CHIP_IN_POINTS;
      break;
    default:
      osSyncPrintf("Unknown Shot Type%d\n", shotKind);
      points = 0;
      break;
  }

  return count * points;
}

/* Turns each player's counts of notable shots into points, and returns
 * everyone's total */
s32 TallyCoursePoints(void) {
  s32 playerIndex;
  s32 shotKind;
  s32 points;
  s32 total = 0;

  for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
    points = 0;
    for (shotKind = 0; shotKind < SHOT_KIND_COUNT; shotKind++) {
      points += GetShotKindPoints(playerIndex, shotKind);
      g_Players[playerIndex].unkAA[shotKind] = 0;
    }

    total += points;
    g_Players[playerIndex].points = points;
  }

  return total;
}

void PrintCoursePoints(void) {
  char text[128];
  s32 playerIndex;
  s32 shotKind;
  s32 points;

  for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
    points = 0;
    for (shotKind = 0; shotKind < SHOT_KIND_COUNT; shotKind++) {
      points += GetShotKindPoints(playerIndex, shotKind);
    }

    sprintf(text, "Player%d EXP %d\n", playerIndex, points);
    PrintDebugText(text, 2, playerIndex + 3);
  }
}

/*
 * Simulates the shot as the swing stands and builds the matrices for the dashes
 * that preview its path. Until the swing sets the power, the preview shows a
 * full swing.
 */
/* Whether the swing has yet to set the power in a player state */
#define IsBeforePowerSet(state) \
  ((state) == PLAYER_STATE_AIMING || (state) == PLAYER_STATE_SETTING_POWER || \
      (state) == PLAYER_STATE_UNK3)
/* Whether the swing has yet to set the impact in a player state */
#define IsBeforeImpactSet(state) \
  ((state) == PLAYER_STATE_AIMING || (state) == PLAYER_STATE_SETTING_POWER || \
      (state) == PLAYER_STATE_SETTING_IMPACT || (state) == PLAYER_STATE_UNK3)

void BuildShotPreview(ShotLaunch* swing, s32 playerState) {
  BallSimulation simulation;
  ShotLaunch shot;
  CoursePoint start;
  f32 dash[4][4];
  f32 align[4][4];
  s32 step;
  s32 length;
  s8* isMiniGolf;

  if ((GetSelectedClub()->surfaceMask & CLUB_SURFACE_GREEN) ||
      IsGameFlagSet(GAME_FLAG_HIDE_SHOT_PREVIEW)) {
    return;
  }

  f_IsShotPreviewBuilt = TRUE;
  shot.heading = g_CurrentShot.heading;
  shot.clubIndex = g_CurrentShot.clubSlot;
  if (IsGameFlagSet(GAME_FLAG_TUNE_SHOT_PREVIEW)) {
    PrintDebugText("               ", 10, 10);
    if ((g_ControllerStates[0].pressedButtons & U_JPAD) &&
        f_ShotPreviewStepCount < SHOT_PREVIEW_MAX_STEPS) {
      f_ShotPreviewStepCount++;
    }

    if ((g_ControllerStates[0].pressedButtons & D_JPAD) &&
        f_ShotPreviewStepCount > SHOT_PREVIEW_MIN_STEPS) {
      f_ShotPreviewStepCount--;
    }

    if ((g_ControllerStates[0].buttons & L_JPAD) &&
        f_ShotPreviewStepCount < SHOT_PREVIEW_MAX_STEPS) {
      f_ShotPreviewStepCount++;
    }

    if ((g_ControllerStates[0].buttons & R_JPAD) &&
        f_ShotPreviewStepCount > SHOT_PREVIEW_MIN_STEPS) {
      f_ShotPreviewStepCount--;
    }

    sprintf(g_PrintBuffer, "prec:%4d", f_ShotPreviewStepCount);
    PrintDebugText(g_PrintBuffer, 10, 3);
  }

  if (IsBeforePowerSet(playerState)) {
    shot.power = MAX_LAUNCH_POWER;
  } else {
    shot.power = swing->power;
  }

  shot.impactTiming = 0;
  if (IsBeforeImpactSet(playerState)) {
    shot.offsetX = swing->offsetX;
    shot.offsetY = swing->offsetY;
  } else {
    shot.offsetX = 0;
    shot.offsetY = 0;
  }

  shot.playerIndex = g_CurrentShot.playerIndex;
  shot.shotMode = g_CurrentShot.shotMode;
  simulation.position[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
  simulation.position[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
  simulation.position[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
  simulation.surface = g_CurrentShot.surface;
  LaunchBallSimulation(&simulation, LAUNCH_PURPOSE_UNK5, &shot);
  f_ShotPreviewDotScroll += SHOT_PREVIEW_SCROLL_STEP;
  for (step = 0; step < f_ShotPreviewStepCount; step++) {
    isMiniGolf = &g_GameState.isMiniGolf;
    if (*isMiniGolf && !simulation.unk94) {
      break;
    }

    start[AXIS_X] = simulation.position[AXIS_X];
    start[AXIS_Y] = simulation.position[AXIS_Y];
    start[AXIS_Z] = simulation.position[AXIS_Z];
    StepBallSimulation(&simulation);
    length = IntLength3D(simulation.position[AXIS_X] - start[AXIS_X],
        simulation.position[AXIS_Y] - start[AXIS_Y],
        simulation.position[AXIS_Z] - start[AXIS_Z]);
    guTranslateF(dash, start[AXIS_X] * COURSE_TO_WORLD_SCALE,
        (start[AXIS_Y] - SHOT_PREVIEW_DROP) * COURSE_TO_WORLD_SCALE,
        start[AXIS_Z] * COURSE_TO_WORLD_SCALE);
    D_80227BA0_ovl_00[step][AXIS_X] =
        simulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE;
    D_80227BA0_ovl_00[step][AXIS_Y] =
        simulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
    D_80227BA0_ovl_00[step][AXIS_Z] =
        simulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    if (length != 0 && length <= SHOT_PREVIEW_MAX_DASH_LENGTH) {
      guScaleF(dash, 1.0f, 1.0f, length * COURSE_TO_WORLD_SCALE);
      guAlignF(align, 0.0f, simulation.position[AXIS_X] - start[AXIS_X],
          simulation.position[AXIS_Y] - start[AXIS_Y],
          simulation.position[AXIS_Z] - start[AXIS_Z]);
      guMtxCatF(dash, align, dash);
      guTranslateF(align, start[AXIS_X] * COURSE_TO_WORLD_SCALE,
          start[AXIS_Y] * COURSE_TO_WORLD_SCALE,
          start[AXIS_Z] * COURSE_TO_WORLD_SCALE);
      guMtxCatF(dash, align, dash);
    }

    guMtxF2L(dash, &D_802271A0_ovl_00[g_DrawBufferIndex][step]);
    if (!*isMiniGolf && simulation.unk94) {
      break;
    }
  }

  f_ShownShotPreviewPointCount = step - 2;
  if (f_ShownShotPreviewPointCount <= 0) {
    f_ShownShotPreviewPointCount = 1;
  }

  if (simulation.unk94) {
    f_HasShotPreviewEnded = TRUE;
    f_ShownShotPreviewPointCount = step + 1;
  } else {
    f_HasShotPreviewEnded = FALSE;
  }
}

/*
 * Draws the shot preview that BuildShotPreview built. On the pause screen it is
 * instead a straight line on screen, from the ball to the preview's last point.
 */
#ifdef NON_MATCHING
void DrawShotPreview(CameraMatrices* matrices) {
  ScreenPoint ball;
  ScreenPoint end;

  if (!f_IsShotPreviewBuilt) {
    return;
  }

  g_ModuleRdpRequestFrames[MODULE_PLAY] = g_FrameCount;
  gDPPipeSync(g_DisplayListHead++);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTextureFilter(g_DisplayListHead++, G_TF_BILERP);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetAlphaCompare(g_DisplayListHead++, G_AC_NONE);
  gDPSetBlendColor(g_DisplayListHead++, 0xFF, 0xFF, 0xFF, 0x7F);
  gDPPipeSync(g_DisplayListHead++);
  gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
  gSPTexture(g_DisplayListHead++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPLoadTextureBlock(g_DisplayListHead++,
      TextureAddress((u8*)g_ShotPreviewDashImage + IMAGE_HEADER_SIZE),
      G_IM_FMT_I, G_IM_SIZ_8b, SHOT_PREVIEW_IMAGE_SIZE, SHOT_PREVIEW_IMAGE_SIZE,
      0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPSetTileSize(g_DisplayListHead++, G_TX_RENDERTILE, f_ShotPreviewDotScroll,
      0,
      f_ShotPreviewDotScroll +
          (SHOT_PREVIEW_IMAGE_SIZE << G_TEXTURE_IMAGE_FRAC),
      SHOT_PREVIEW_IMAGE_SIZE << G_TEXTURE_IMAGE_FRAC);
  gDPSetEnvColor(g_DisplayListHead++, 0xE0, 0xE0, 0x00, 0x40);
  if (f_HasShotPreviewEnded) {
    gDPSetPrimColor(g_DisplayListHead++, 0, 0, 0xFF, 0x00, 0x00, 0xC8);
  } else {
    gDPSetPrimColor(g_DisplayListHead++, 0, 0, 0xFF, 0xFF, 0xFF, 0x80);
  }

  gDPPipeSync(g_DisplayListHead++);
  if (g_PauseScreen == PAUSE_SCREEN_FREE_LOOK) {
    f32 dx;
    f32 dy;
    f32 scale;
    s32 offsetX;
    s32 offsetY;

    if (ProjectPoint(g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
            g_CurrentShot.ballPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE,
            g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE,
            &ball) >= 0) {
      return;
    }

    if (ProjectPoint(
            D_80227BA0_ovl_00[f_ShownShotPreviewPointCount - 1][AXIS_X],
            D_80227BA0_ovl_00[f_ShownShotPreviewPointCount - 1][AXIS_Y],
            D_80227BA0_ovl_00[f_ShownShotPreviewPointCount - 1][AXIS_Z],
            &end) >= 0) {
      return;
    }

    guMtxIdent(&D_80227110_ovl_00);
    ball.x *= 4;
    ball.y *= 4;
    end.x *= 4;
    end.y *= 4;
    dx = end.x - ball.x;
    dy = end.y - ball.y;
    if (fabsf(dx) > 1.0f || fabsf(dy) > 1.0f) {
      scale = PUTT_PREVIEW_HALF_WIDTH / sqrtf((dx * dx) + (dy * dy));
      gSPLoadGeometryMode(g_DisplayListHead++, 0);
      gSPSetGeometryMode(g_DisplayListHead++, G_SHADE | G_SHADING_SMOOTH);
      gDPSetCombineMode(g_DisplayListHead++, G_CC_MODULATEIDECALA_PRIM,
          G_CC_MODULATEIDECALA_PRIM);
      gDPPipeSync(g_DisplayListHead++);
      gDPSetRenderMode(g_DisplayListHead++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
      gDPPipeSync(g_DisplayListHead++);
      gSPMatrix(g_DisplayListHead++, OS_K0_TO_PHYSICAL(&g_ScreenOrthoMatrix),
          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
      gSPMatrix(g_DisplayListHead++, OS_K0_TO_PHYSICAL(&D_80227110_ovl_00),
          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
      gSPVertex(g_DisplayListHead++, f_PuttPreviewLineVertices, 4, 0);
      offsetX = dy * scale;
      offsetY = dx * scale;
      gSPModifyVertex(g_DisplayListHead++, 0, G_MWO_POINT_XYSCREEN,
          ((ball.x + offsetX) << 16) + ball.y);
      gSPModifyVertex(g_DisplayListHead++, 1, G_MWO_POINT_XYSCREEN,
          (ball.x << 16) + (ball.y + offsetY));
      gSPModifyVertex(g_DisplayListHead++, 2, G_MWO_POINT_XYSCREEN,
          ((end.x + offsetX) << 16) + end.y);
      gSPModifyVertex(g_DisplayListHead++, 3, G_MWO_POINT_XYSCREEN,
          (end.x << 16) + (end.y + offsetY));
      gSP2Triangles(g_DisplayListHead++, 1, 0, 2, 0, 1, 2, 3, 0);
      gSPMatrix(g_DisplayListHead++, &matrices->projection,
          G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
      gDPPipeSync(g_DisplayListHead++);
    }
  } else {
    s32 step;

    gDPPipeSync(g_DisplayListHead++);
    if (g_LetterboxState == LETTERBOX_STATE_NONE ||
        g_LetterboxState == LETTERBOX_STATE_UNK5) {
      gDPSetRenderMode(
          g_DisplayListHead++, G_RM_ZB_CLD_SURF, G_RM_ZB_CLD_SURF2);
    } else {
      gDPSetRenderMode(
          g_DisplayListHead++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
    }

    gSPLoadGeometryMode(g_DisplayListHead++, 0);
    gSPSetGeometryMode(
        g_DisplayListHead++, G_ZBUFFER | G_SHADE | G_FOG | G_SHADING_SMOOTH);
    gSPFogFactor(g_DisplayListHead++, 320, -128);
    gDPSetCombineLERP(g_DisplayListHead++, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0,
        SHADE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, SHADE, 0);
    for (step = 0; step < f_ShownShotPreviewPointCount; step++) {
      gSPMatrix(g_DisplayListHead++,
          &D_802271A0_ovl_00[g_DrawBufferIndex][step],
          G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
      gSPVertex(g_DisplayListHead++, f_ShotPreviewDashVertices, 4, 0);
      gSP2Triangles(g_DisplayListHead++, 0, 1, 2, 0, 0, 2, 3, 0);
      gSPPopMatrix(g_DisplayListHead++, G_MTX_MODELVIEW);
    }

    gDPPipeSync(g_DisplayListHead++);
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", DrawShotPreview);
#endif

/*
 * Returns whether the player's hole-out may enter the records: a short one,
 * by one of the main characters, outside mini-golf, in a mode that keeps them
 */
Boolean IsRecordableHoleOut(s32 playerIndex) {
  if (g_GameState.replayKind != REPLAY_KIND_NONE) {
    return FALSE;
  }

  if (!IsGameFlagSet(GAME_FLAG_RECORD_ALL_HOLE_OUTS)) {
    if (g_Players[playerIndex].isComputer) {
      return FALSE;
    }

    if (g_GameState.gameMode == GAME_MODE_TRAINING) {
      return FALSE;
    }
  }

  if (g_Players[playerIndex].strokeCount > MAX_RECORDED_STROKE_COUNT) {
    return FALSE;
  }

  if ((s8)g_Players[playerIndex].characterIndex >= CHARACTER_KID) {
    return FALSE;
  }

  if (g_GameState.isMiniGolf) {
    return FALSE;
  }

  switch (g_GameState.gameMode) {
    case GAME_MODE_STROKE:
    case GAME_MODE_UNK1:
    case GAME_MODE_TRAINING:
    case GAME_MODE_GET_CHARACTER:
    case GAME_MODE_TOURNAMENT:
    case GAME_MODE_SPEED_GOLF:
      return TRUE;
    case GAME_MODE_MATCH_GAME:
    case GAME_MODE_UNK6:
    case GAME_MODE_UNK8:
    case GAME_MODE_RING_SHOT:
    case GAME_MODE_SKINS_MATCH:
      return FALSE;
  }

  return FALSE;
}

Boolean IsRingShotRound(void) {
  if (g_GameState.replayKind == REPLAY_KIND_NONE) {
    return g_GameState.gameMode == GAME_MODE_RING_SHOT;
  }

  return FALSE;
}

/* Keeps the player's last shot, and the hole and how it was played, in their
 * replay record */
void RecordHoleOut(s32 playerIndex) {
  u32 shotIndex = g_Players[playerIndex].strokeCount - 1;

  if (shotIndex < HOLE_RECORD_MAX_STROKES) {
    PlayRecord* record = &D_800FEDD8[playerIndex];

    /* Read through g_GameState, which keeps GCC from loading the fields early,
     * to match */
    record->gameMode = g_GameState.gameMode;
    record->courseIndex = g_GameState.courseIndex;
    record->holeIndex = g_GameState.holeIndex;
    record->scoreKind =
        GetRecordedScoreKind(g_Players[playerIndex].strokeCount);
    record->characterIndex = g_Players[playerIndex].characterIndex;
    record->unk3C = g_Players[playerIndex].unk22;
    record->paletteIndex = g_Players[playerIndex].paletteIndex;
    record->strokeCount = g_Players[playerIndex].strokeCount;
    record->unk40 = g_Players[playerIndex].unk88;
    record->hole = g_HoleSetup;
    record->shots[shotIndex].heading = g_CurrentShot.heading;
    record->shots[shotIndex].powerGaugeFrame = g_CurrentShot.powerGaugeFrame;
    record->shots[shotIndex].impactGaugeFrame = g_CurrentShot.impactGaugeFrame;
    record->shots[shotIndex].shotMode = g_CurrentShot.shotMode;
    record->shots[shotIndex].clubSlot = g_CurrentShot.clubSlot;
    record->shots[shotIndex].offsetX = g_CurrentShot.offsetX;
    record->shots[shotIndex].offsetY = g_CurrentShot.offsetY;
  }
}

/* Keeps the player's last shot, and the hole and how it was played, in their
 * Ring Shot record */
void RecordRingShotHole(s32 playerIndex) {
  u32 shotIndex = g_Players[playerIndex].strokeCount - 1;

  if (shotIndex < RING_SHOT_RECORD_MAX_STROKES) {
    RingShotRecord* record = &D_80105F40[playerIndex];

    record->courseIndex = g_GameState.courseIndex;
    record->holeIndex = g_GameState.holeIndex;
    record->scoreKind =
        GetRecordedScoreKind(g_Players[playerIndex].strokeCount);
    record->characterIndex = g_Players[playerIndex].characterIndex;
    record->unk38 = g_Players[playerIndex].unk22;
    record->paletteIndex = g_Players[playerIndex].paletteIndex;
    record->strokeCount = g_Players[playerIndex].strokeCount;
    record->unk3C = g_Players[playerIndex].unk88;
    record->hole = g_HoleSetup;
    record->shots[shotIndex].heading = g_CurrentShot.heading;
    record->shots[shotIndex].powerGaugeFrame = g_CurrentShot.powerGaugeFrame;
    record->shots[shotIndex].impactGaugeFrame = g_CurrentShot.impactGaugeFrame;
    record->shots[shotIndex].shotMode = g_CurrentShot.shotMode;
    record->shots[shotIndex].clubSlot = g_CurrentShot.clubSlot;
    record->shots[shotIndex].offsetX = g_CurrentShot.offsetX;
    record->shots[shotIndex].offsetY = g_CurrentShot.offsetY;
  }
}

Boolean HasNoRingsLeft(UnkB8* player) {
  s32 i;

  for (i = 0; i < RING_COUNT; i++) {
    if (player->ringStates[i] != RING_STATE_NONE) {
      return FALSE;
    }
  }

  return TRUE;
}

Boolean AreAllRingsPassed(UnkB8* player) {
  s32 i;

  for (i = 0; i < RING_COUNT; i++) {
    if (player->ringStates[i] == RING_STATE_SHOWN) {
      return FALSE;
    }
  }

  return TRUE;
}

void RecordRingShotClear(s32 playerIndex) {
  s8* count =
      &GetSaveData()
           ->hasRingShotStar[g_GameState.courseIndex][g_GameState.holeIndex];

  (*count)++;
  if (*count < 0) {
    *count = 0;
  } else if (*count > MAX_RING_SHOT_CLEAR_COUNT) {
    *count = MAX_RING_SHOT_CLEAR_COUNT;
  }

  if ((s8)g_Players[playerIndex].characterIndex < SAVE_UNK1344_ENTRY_COUNT) {
    count =
        &GetSaveData()->unk1344[g_GameState.courseIndex][g_GameState.holeIndex]
                               [(s8)g_Players[playerIndex].characterIndex];
    (*count)++;
    if (*count < 0) {
      *count = 0;
    } else if (*count > MAX_RING_SHOT_CLEAR_COUNT) {
      *count = MAX_RING_SHOT_CLEAR_COUNT;
    }
  }

  WriteSaveData();
}

/* Saves a Ring Shot hole as cleared once it is holed within par with every ring
 * passed */
void CheckRingShotClear(s32 playerIndex) {
  if (g_GameState.gameMode == GAME_MODE_RING_SHOT &&
      g_Players[playerIndex].strokeCount <= g_HoleSetup.par &&
      !g_Players[playerIndex].isComputer &&
      HasNoRingsLeft(&g_Players[playerIndex])) {
    RecordRingShotClear(playerIndex);
  }
}

/*
 * The first of the club kind's animations; shotMode is unused. The staff
 * roll overlay has its own copies of these four routines, with the same
 * names, so these are static to link beside them.
 */
static s32 GetStanceAnimation(u32 slotIndex, s32 club, s32 shotMode) {
  s32 animation;

  switch (GetCharacterClubKind(slotIndex, club)) {
    case CLUB_KIND_WOOD:
      animation = WOOD_ANIMATIONS;
      break;
    case CLUB_KIND_IRON:
      animation = IRON_ANIMATIONS;
      break;
    case CLUB_KIND_WEDGE:
      animation = WEDGE_ANIMATIONS;
      break;
    case CLUB_KIND_PUTTER:
      animation = PUTTER_ANIMATIONS;
      break;
    default:
      animation = WOOD_ANIMATIONS;
      break;
  }

  return animation;
}

/* The second of the club kind's animations, or the third for an approach shot
 */
static s32 GetSwingAnimation(u32 slotIndex, s32 club, s32 shotMode) {
  s32 animation;

  switch (GetCharacterClubKind(slotIndex, club)) {
    case CLUB_KIND_WOOD:
      animation = WOOD_ANIMATIONS + SWING_ANIMATION;
      break;
    case CLUB_KIND_IRON:
      animation = IRON_ANIMATIONS + SWING_ANIMATION;
      break;
    case CLUB_KIND_WEDGE:
      animation = WEDGE_ANIMATIONS + SWING_ANIMATION;
      break;
    case CLUB_KIND_PUTTER:
      animation = PUTTER_ANIMATIONS + SWING_ANIMATION;
      break;
    default:
      animation = 0;
      break;
  }

  return animation + (shotMode == SHOT_MODE_APPROACH);
}

static void SetStanceAnimation(u32 slotIndex, s32 club, s32 shotMode) {
  SetCharacterAnimation(
      slotIndex, GetStanceAnimation(slotIndex, club, shotMode));
}

static void SetSwingAnimation(u32 slotIndex, s32 club, s32 shotMode) {
  SetCharacterAnimation(
      slotIndex, GetSwingAnimation(slotIndex, club, shotMode));
}

/*
 * Sets the shot up for the player whose turn it is: where the ball lies and the
 * heading. Off the tee of a par 4 or 5 the shot aims at the hole's first aim
 * point, and otherwise at the cup. A hole without that point gets the cup's
 * position for it.
 */
void StartPlayerShot(s32 playerIndex) {
  UnkB8* player = &g_Players[playerIndex];
  CoursePoint target;
  s32 x;
  s32 z;

  g_CurrentShot.playerIndex = playerIndex;
  g_CurrentShot.controllerIndex = player->controllerIndex;
  x = player->ballPosition[AXIS_X];
  g_CurrentShot.ballPosition[AXIS_X] = x;
  g_CurrentShot.ballPosition[AXIS_Y] = player->ballPosition[AXIS_Y];
  z = player->ballPosition[AXIS_Z];
  g_CurrentShot.ballPosition[AXIS_Z] = z;
  g_CurrentShot.surface = (s8)player->ballSurface;
  g_CurrentShot.offsetY = 0;
  g_CurrentShot.offsetX = 0;
  g_CurrentShot.unk54 = player->unk28;
  g_CurrentShot.gaugeFrame = 0;
  g_CurrentShot.cupDistance = GetCupDistance(
      x - g_HoleSetup.cupPosition[AXIS_X], z - g_HoleSetup.cupPosition[AXIS_Z]);
  if (!g_IsTrainingHeadingKept || g_GameState.gameMode != GAME_MODE_TRAINING) {
    SurfaceKind surfaceKind = GetSurfaceInfo(g_CurrentShot.surface)->kind;

    if (g_HolePoints.unk60[0][AXIS_X] == 0 ||
        g_HolePoints.unk60[0][AXIS_Z] == 0) {
      g_HolePoints.unk60[0][AXIS_X] = g_HoleSetup.cupPosition[AXIS_X];
      g_HolePoints.unk60[0][AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y];
      g_HolePoints.unk60[0][AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z];
    }

    if (g_HoleSetup.par != PAR_THREE &&
        (surfaceKind == SURFACE_KIND_TEE_UP ||
            surfaceKind == SURFACE_KIND_TEE_GROUND)) {
      target[AXIS_X] = g_HolePoints.unk60[0][AXIS_X];
      target[AXIS_Y] = g_HolePoints.unk60[0][AXIS_Y];
      target[AXIS_Z] = g_HolePoints.unk60[0][AXIS_Z];
    } else {
      target[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X];
      target[AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y];
      target[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z];
    }

    g_CurrentShot.heading = atan2(target[AXIS_Z] - player->ballPosition[AXIS_Z],
                                target[AXIS_X] - player->ballPosition[AXIS_X]) *
        RADIANS_TO_BINARY_ANGLE;
    PickStartingClub(&g_HoleSetup);
  }

  g_IsTrainingHeadingKept = FALSE;
  if (IsGameFlagSet(GAME_FLAG_AIM_DOWN_RANGE)) {
    AimDownRange();
    g_LetterboxState = LETTERBOX_STATE_MOVING_CAMERA;
  } else {
    ResetAimCamera();
    g_LetterboxState = LETTERBOX_STATE_NONE;
  }

  g_RoundHud.scorecardPage = -1;
  g_RoundHud.isWindIndicatorShown = TRUE;
  g_RoundHud.unk8 = TRUE;
  g_RoundHud.isLetterboxShown = TRUE;
  g_RoundHud.isLieIconShown = TRUE;
  g_RoundHud.isClubHeadIconShown = TRUE;
  g_RoundHud.unkB = TRUE;
  g_RoundHud.isHoleIntroTextShown = FALSE;
  g_RoundHud.unkC = TRUE;
  g_RoundHud.unk13 = FALSE;
  g_RoundHud.unkD = TRUE;
  g_RoundHud.unk84[0] = TRUE;
  g_RoundHud.unk84[1] = FALSE;
  g_RoundHud.flags = 0;
  PlaceShotCamera();
  ResetShotOffsetMarker();
}

void func_80207660_ovl_00(UnkB8* player) {}

/*
 * Once every player's shot has finished, picks who plays next. Players still on
 * the tee go first, then those off the green, then those on it. Among those,
 * the farthest from the cup goes first, and the earliest in the tee order
 * breaks ties.
 */
#ifdef NON_MATCHING
void PickNextPlayer(void) {
  s32 i;
  s32 finishedCount;
  s32 nextPlayer;
  s32 bestPriority;
  s32 bestDistance;
  s32 bestTeeOrder;
  s32 priority;
  s32 distance;

  if (D_800FBDB8 == CONTEST_MOVIES_PLAYING) {
    return;
  }

  for (finishedCount = 0; finishedCount < g_GameState.playerCount;
      finishedCount++) {
    if (g_Players[finishedCount].state != PLAYER_STATE_WAITING &&
        g_Players[finishedCount].state != PLAYER_STATE_HOLE_DONE) {
      break;
    }
  }

  bestDistance = -1;
  if (finishedCount != g_GameState.playerCount) {
    return;
  }

  bestPriority = 0;
  bestTeeOrder = 0x7FFFFFFF;
  nextPlayer = 0;
  for (i = 0; i < g_GameState.playerCount; i++) {
    priority = LIE_PRIORITY_TEE;
    if (g_Players[i].holeStatus != HOLE_STATUS_PLAYING) {
      continue;
    }

    if (IsGameFlagSet(GAME_FLAG_FEWEST_STROKES_FIRST)) {
      distance = 0;
      priority = STROKE_PRIORITY_BASE - g_Players[i].strokeCount;
    } else {
      if (g_Players[i].strokeCount != 0) {
        priority = GetSurfaceInfo((s8)g_Players[i].ballSurface)->kind !=
            SURFACE_KIND_GREEN;
      }

      distance = IntLength2D(
          g_HoleSetup.cupPosition[AXIS_X] - g_Players[i].ballPosition[AXIS_X],
          g_HoleSetup.cupPosition[AXIS_Z] - g_Players[i].ballPosition[AXIS_Z]);
    }

    if (priority > bestPriority) {
      bestPriority = priority;
      nextPlayer = i;
      bestTeeOrder = g_Players[nextPlayer].teeOrder;
      bestDistance = distance;
    } else if (priority == bestPriority) {
      if (distance > bestDistance) {
        nextPlayer = i;
        bestTeeOrder = g_Players[nextPlayer].teeOrder;
        bestDistance = distance;
      } else if (distance == bestDistance &&
          g_Players[i].teeOrder < bestTeeOrder) {
        bestTeeOrder = g_Players[i].teeOrder;
        bestDistance = distance;
        nextPlayer = i;
      }
    }
  }

  g_GameState.currentPlayerIndex = nextPlayer;
  g_Players[nextPlayer].state = PLAYER_STATE_STARTING_TURN;
  if (g_IsHudTurnAwayRequested) {
    StartHudAnimation(HUD_ANIMATION_TURN_AWAY);
    g_IsHudTurnAwayRequested = FALSE;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", PickNextPlayer);
#endif

/* Hides the rings the ball passed, so that a new shot can pass them again */
void ClearPassedRings(UnkB8* player) {
  s32 i;

  for (i = 0; i < RING_COUNT; i++) {
    if (player->ringStates[i] >= RING_STATE_PASSED) {
      player->ringStates[i] = RING_STATE_NONE;
    }
  }
}

/* Now and then plays one of the course's ambient sounds, picked at random, from
 * a random side */
void PlayAmbientSound(void) {
  s32 choice;

  if (++f_AmbientSoundIdleFrameCount == AMBIENT_SOUND_INTERVAL) {
    choice = guRandom() % AMBIENT_SOUND_CHOICES;
    f_AmbientSoundIdleFrameCount = 0;
    if (f_AmbientSounds[GetCourseRecordIndex()][choice] != NO_AMBIENT_SOUND) {
      PlaySoundWithEffects(f_AmbientSounds[GetCourseRecordIndex()][choice],
          AMBIENT_SOUND_CHANNEL, AMBIENT_SOUND_VOLUME, AMBIENT_SOUND_REVERB,
          guRandom() % SOUND_PAN_RANGE);
    }
  }
}

/* The slot to free is always the golfer's, whatever the caller passes */
void FreeCharacterAndReactionLight(s32 slotIndex) {
  FreeCharacterSlot(0);
  TurnOffReactionLight();
}

/*
 * Measures a tee shot for the records: the drive counts if it ends on the
 * fairway or the green, and the distance to the pin if it ends on the green.
 */
void MeasureTeeShot(s32 playerIndex) {
  s32 distance;
  SurfaceInfo* info;

  if (g_Players[playerIndex].strokeCount == 1) {
    distance = UNCOUNTED_DISTANCE;
    info = GetSurfaceInfo(g_SimulatedShot.endSurface);
    switch ((u32)info->kind) {
      case SURFACE_KIND_FAIRWAY:
      case SURFACE_KIND_FAIRWAY_FAIR_LIE:
      case SURFACE_KIND_FAIRWAY_POOR_LIE:
      case SURFACE_KIND_GREEN:
      case SURFACE_KIND_GREEN_EDGE:
        distance = IntLength2D(g_SimulatedShot.startPosition[AXIS_X] -
                g_SimulatedShot.endPosition[AXIS_X],
            g_SimulatedShot.startPosition[AXIS_Z] -
                g_SimulatedShot.endPosition[AXIS_Z]);
        break;
    }

    g_Players[playerIndex].driveDistance = distance;
    distance = UNCOUNTED_DISTANCE;
    if (g_SimulatedShot.endsInCup) {
      distance = 0;
    } else if (info->kind == SURFACE_KIND_GREEN) {
      distance = GetCupDistance(
          g_HoleSetup.cupPosition[AXIS_X] - g_SimulatedShot.endPosition[AXIS_X],
          g_HoleSetup.cupPosition[AXIS_Z] -
              g_SimulatedShot.endPosition[AXIS_Z]);
    }

    g_Players[playerIndex].pinDistance = distance;
  }
}

/* Leaves the distances that the player's contests have not measured uncounted
 */
void SkipUnmeasuredDistances(s32 playerIndex) {
  if (g_Players[playerIndex].driveDistance == UNMEASURED_DISTANCE) {
    g_Players[playerIndex].driveDistance = UNCOUNTED_DISTANCE;
  }

  if (g_Players[playerIndex].pinDistance == UNMEASURED_DISTANCE) {
    g_Players[playerIndex].pinDistance = UNCOUNTED_DISTANCE;
  }
}

/*
 * Rumbles the player's controller briefly in a game for several players,
 * if every player has a controller of their own
 */
void StartTurnRumble(s32 playerIndex) {
  if (D_801051F0 == TRUE && g_GameState.playerCount >= 2) {
    s32 controllerIndex = g_Players[playerIndex].controllerIndex;

    EndRumbleForceStop();
    if ((u32)controllerIndex < MAXCONTROLLERS &&
        g_GameState.replayKind == REPLAY_KIND_NONE) {
      switch (nuContRmbCheck(g_PlayerControllerPorts[controllerIndex])) {
        case 0:
          nuContRmbStart(g_PlayerControllerPorts[controllerIndex],
              TURN_RUMBLE_FREQUENCY, TURN_RUMBLE_FRAME_COUNT);
          break;
        /* The empty cases are there to match */
        case PFS_ERR_NOPACK:
          break;
        case PFS_ERR_CONTRFAIL:
          break;
      }
    }
  }
}

/*
 * Lets the players who are waiting make their characters call out with the
 * D-pad and the C buttons, if every player has a controller of their own
 */
void PlayWaitingPlayersCalls(void) {
  s32 i;
  s32 controllerIndex;
  u16 pressed;

  if (D_801051F0 == TRUE) {
    for (i = 0; i < g_GameState.playerCount; i++) {
      if ((g_Players[i].state == PLAYER_STATE_WAITING) |
          (g_Players[i].state == PLAYER_STATE_HOLE_DONE)) {
        controllerIndex = g_Players[i].controllerIndex;
        if (controllerIndex ==
            g_Players[g_GameState.currentPlayerIndex].controllerIndex) {
          continue;
        }

        if (controllerIndex >= MAXCONTROLLERS) {
          continue;
        }

        pressed = g_ControllerStates[controllerIndex].pressedButtons;
        if (pressed & U_JPAD) {
          PlayCharacterSound((s8)g_Players[i].characterIndex, i,
              CHARACTER_SOUND_CALL_OUT_UP, SOUND_FULL_SCALE,
              g_Players[i].soundPan);
        } else if (pressed & D_JPAD) {
          PlayCharacterSound((s8)g_Players[i].characterIndex, i,
              CHARACTER_SOUND_CALL_OUT_DOWN, SOUND_FULL_SCALE,
              g_Players[i].soundPan);
        } else if (pressed & L_JPAD) {
          PlayCharacterSound((s8)g_Players[i].characterIndex, i,
              CHARACTER_SOUND_CALL_OUT_LEFT, SOUND_FULL_SCALE,
              g_Players[i].soundPan);
        } else if (pressed & R_JPAD) {
          PlayCharacterSound((s8)g_Players[i].characterIndex, i,
              CHARACTER_SOUND_CALL_OUT_RIGHT, SOUND_FULL_SCALE,
              g_Players[i].soundPan);
        } else if (pressed & U_CBUTTONS) {
          PlayCharacterSound((s8)g_Players[i].characterIndex, i,
              CHARACTER_SOUND_CALL_OUT_C_UP, SOUND_FULL_SCALE,
              g_Players[i].soundPan);
        } else if (pressed & D_CBUTTONS) {
          PlayCharacterSound((s8)g_Players[i].characterIndex, i,
              CHARACTER_SOUND_CALL_OUT_C_DOWN, SOUND_FULL_SCALE,
              g_Players[i].soundPan);
        } else if (pressed & L_CBUTTONS) {
          PlayCharacterSound((s8)g_Players[i].characterIndex, i,
              CHARACTER_SOUND_CALL_OUT_C_LEFT, SOUND_FULL_SCALE,
              g_Players[i].soundPan);
        } else if (pressed & R_CBUTTONS) {
          PlayCharacterSound((s8)g_Players[i].characterIndex, i,
              CHARACTER_SOUND_CALL_OUT_C_RIGHT, SOUND_FULL_SCALE,
              g_Players[i].soundPan);
        }
      }
    }
  }
}

/* Plays the jingle for how the player finished the hole */
void PlayHoleOutSong(s32 playerIndex) {
  s32 strokeCount = g_Players[playerIndex].strokeCount;
  s32 scoreToPar = strokeCount - g_HoleSetup.par;

  if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
    PlaySong(SONG_HOLE_WON, 0);
  } else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
    if (AreAllRingsPassed(&g_Players[playerIndex])) {
      PlaySong(SONG_HOLE_WON, 0);
    } else {
      PlaySong(SONG_HOLE_LOST, 0);
    }
  } else if (strokeCount == 1) {
    PlaySong(SONG_HOLE_OUT_BIRDIE, 0);
  } else {
    switch (scoreToPar) {
      case SCORE_TO_PAR_BIRDIE:
        PlaySong(SONG_HOLE_OUT_BIRDIE, 0);
        break;
      case SCORE_TO_PAR_EAGLE:
        PlaySong(SONG_HOLE_OUT_EAGLE, 0);
        break;
      case SCORE_TO_PAR_ALBATROSS:
        PlaySong(SONG_HOLE_OUT_EAGLE, 0);
        break;
      case SCORE_TO_PAR_PAR:
        PlaySong(SONG_HOLE_OUT_PAR, 0);
        break;
      case SCORE_TO_PAR_BOGEY:
        PlaySong(SONG_HOLE_OUT_BOGEY, 0);
        break;
      case SCORE_TO_PAR_DOUBLE_BOGEY:
      case SCORE_TO_PAR_TRIPLE_BOGEY:
      default:
        PlaySong(SONG_HOLE_OUT_OVER_BOGEY, 0);
        break;
    }
  }
}

void ShowHoleOutBanner(s32 playerIndex) {
  s8 strokeCount = g_Players[playerIndex].strokeCount;
  s32 scoreToPar = strokeCount - g_HoleSetup.par;
  s32 banner = BANNER_NONE;

  if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
    StartBanner(BANNER_FINISH);
    PlaySound(SOUND_CALL_FINISH, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
  } else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
    if (AreAllRingsPassed(&g_Players[playerIndex])) {
      StartBanner(BANNER_COMPLETE);
      PlaySound(SOUND_CALL_COMPLETE, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
    }
  } else if (strokeCount == 1) {
    banner = BANNER_HOLE_IN_ONE;
    PlaySound(SOUND_CALL_HOLE_IN_ONE, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
  } else {
    switch (scoreToPar) {
      case SCORE_TO_PAR_BIRDIE:
        banner = BANNER_BIRDIE;
        PlaySound(SOUND_CALL_BIRDIE, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
        break;
      case SCORE_TO_PAR_EAGLE:
        banner = BANNER_EAGLE;
        PlaySound(SOUND_CALL_EAGLE, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
        break;
      case SCORE_TO_PAR_ALBATROSS:
        banner = BANNER_ALBATROSS;
        PlaySound(
            SOUND_CALL_ALBATROSS, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
        break;
      case SCORE_TO_PAR_PAR:
        banner = BANNER_PAR;
        PlaySound(SOUND_CALL_PAR, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
        break;
      case SCORE_TO_PAR_BOGEY:
        banner = BANNER_BOGEY;
        PlaySound(SOUND_CALL_BOGEY, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
        break;
      case SCORE_TO_PAR_DOUBLE_BOGEY:
        banner = BANNER_STROKES_OVER_PAR + SCORE_TO_PAR_DOUBLE_BOGEY;
        break;
      case SCORE_TO_PAR_TRIPLE_BOGEY:
        banner = BANNER_STROKES_OVER_PAR + SCORE_TO_PAR_TRIPLE_BOGEY;
        break;
      default:
        banner = BANNER_STROKES_OVER_PAR + scoreToPar;
        break;
    }
  }

  if (banner >= 0) {
    StartBanner(banner);
  }
}

/*
 * UpdateTurns holds ten functions nested in it, a GNU C extension. GCC
 * passes them the parent's frame in $v0, its static chain, and they read
 * the parent's playerIndex through it. The debug helpers before it are
 * inline: inlined, their buffers share one stack slot, as in the
 * original's frame.
 */
#ifdef NON_MATCHING
extern s32 D_800B7D9C;
extern u8 D_800FE330;

/* Debug flag 0x47: prints where the ball lies, and the second controller moves
 * it while Z is held */
static inline void DebugPlaceBall(s32 playerIndex) {
  char text[128];

  D_80227188_ovl_00 = g_GameState.courseIndex;
  D_8022718C_ovl_00 = g_GameState.holeIndex;
  SetGameFlag(GAME_FLAG_RANDOM_FRAMEBUFFER_COUNT);
  sprintf(text, "X:%X Y:%X Z:%X  Attr%d             ",
      g_CurrentShot.ballPosition[AXIS_X], g_CurrentShot.ballPosition[AXIS_Y],
      g_CurrentShot.ballPosition[AXIS_Z],
      GetSurface(g_CurrentShot.ballPosition[AXIS_X],
          g_CurrentShot.ballPosition[AXIS_Z]));
  PrintDebugText(text, 3, 6);
  ClearGameFlag(GAME_FLAG_RANDOM_FRAMEBUFFER_COUNT);
  if (g_ControllerData[1].button & Z_TRIG) {
    g_CurrentShot.ballPosition[AXIS_X] += g_ControllerStates[1].stickX *
        ((Abs(g_ControllerStates[1].stickX) * 2) + 1);
    g_CurrentShot.ballPosition[AXIS_Z] += g_ControllerStates[1].stickY *
        ((Abs(g_ControllerStates[1].stickY) * 2) + 1);
    g_CurrentShot.ballPosition[AXIS_Y] = GetGroundHeight(
        g_CurrentShot.ballPosition[AXIS_X], g_CurrentShot.ballPosition[AXIS_Z]);
    g_Players[playerIndex].ballPosition[AXIS_X] =
        g_CurrentShot.ballPosition[AXIS_X];
    g_Players[playerIndex].ballPosition[AXIS_Y] =
        g_CurrentShot.ballPosition[AXIS_Y];
    g_Players[playerIndex].ballPosition[AXIS_Z] =
        g_CurrentShot.ballPosition[AXIS_Z];
    PlaceBallAtRest(playerIndex, &g_TurnBallSimulation);
  }
}

/* Debug flag 0x3E: the D-pad picks a character and a reaction to load in its
 * place */
static inline void DebugTestGutsMotion(void) {
  char* motionName;
  char text[128];
  switch (f_GutsTestMotion) {
    case REACTION_ANIMATION_EAGLE:
      motionName = "EAGLE ";
      break;
    case REACTION_ANIMATION_BIRDIE:
      motionName = "BIRDIE";
      break;
    case REACTION_ANIMATION_PAR:
      motionName = "PAR   ";
      break;
    case REACTION_ANIMATION_BOGEY:
      motionName = "BOGEY ";
      break;
    default:
      motionName = "ERROR  ";
      break;
  }

  f_HasGutsTestRun = TRUE;
  if (g_TurnController.repeatedButtons & L_JPAD) {
    f_GutsTestCharacter--;
  }

  if (g_TurnController.repeatedButtons & R_JPAD) {
    f_GutsTestCharacter++;
  }

  if (g_TurnController.repeatedButtons & U_JPAD) {
    f_GutsTestMotion--;
  }

  if (g_TurnController.repeatedButtons & D_JPAD) {
    f_GutsTestMotion++;
  }

  if (f_GutsTestCharacter < 0) {
    f_GutsTestCharacter = CHARACTER_COUNT - 1;
  }

  if (f_GutsTestCharacter >= CHARACTER_COUNT) {
    f_GutsTestCharacter = 0;
  }

  if (f_GutsTestMotion < 12) {
    f_GutsTestMotion = 15;
  }

  if (f_GutsTestMotion >= 16) {
    f_GutsTestMotion = 12;
  }

  sprintf(
      text, "GUTS TEST CH[%2.2d] MOTION %s", f_GutsTestCharacter, motionName);
  PrintDebugText(text, 3, 3);
  if (!IsCharacterSlotBusy(0)) {
    LoadCharacter(0, f_GutsTestCharacter + CHARACTER_COUNT, 0);
  }

  if (g_TurnController.buttons) {
    FreeCharacterAndReactionLight(0);
  }

  g_TurnController.repeatedButtons &= ~(U_JPAD | D_JPAD);
}

/* Debug flag 0x4A: the second stick turns a camera that frames the ball and the
 * shot */
static inline void FrameBallAndShot(void) {
  Vec3 center;

  f_DebugFramingYaw += g_ControllerStates[1].stickX * 0.002f;
  f_DebugFramingPitch += g_ControllerStates[1].stickY * 0.002f;
  ClearFramedSpheres();
  SetFramingAngles(f_DebugFramingPitch, f_DebugFramingYaw);
  center[0] = g_TurnBallSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE;
  center[1] = g_TurnBallSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
  center[2] = g_TurnBallSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
  AddFramedSphere(center, 7.5f);
  center[0] = g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
  center[1] = g_CurrentShot.ballPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
  center[2] = g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
  AddFramedSphere(center, 7.5f);
  FrameSpheres(0);
  GetFramedView(g_Cameras[0].view.eye, g_Cameras[0].view.lookAt);
}

/* Whether the stopwatches run in a player state: from aiming to the flight */
#define IsTimedState(state) \
  ((state) == PLAYER_STATE_AIMING || (state) == PLAYER_STATE_SETTING_POWER || \
      (state) == PLAYER_STATE_SETTING_IMPACT || \
      (state) == PLAYER_STATE_STARTING_SWING || \
      (state) == PLAYER_STATE_SWINGING || \
      (state) == PLAYER_STATE_BALL_IN_FLIGHT)
/* Whether a pause screen lets play go on */
#define IsPlayGoingOn(pauseScreen) \
  ((pauseScreen) == PAUSE_SCREEN_NONE || \
      (pauseScreen) == PAUSE_SCREEN_FREE_LOOK)
/* Whether a character is Yoshi or Donkey Kong, who share a round song */
#define IsYoshiOrDonkeyKong(character) \
  ((character) == CHARACTER_YOSHI || (character) == CHARACTER_DONKEY_KONG)
/* Whether four players are all Yoshi or Donkey Kong, which has its own song */
#define IsYoshiAndDonkeyKongRound() \
  (g_GameState.playerCount == MAX_PLAYER_COUNT && \
      IsYoshiOrDonkeyKong(g_Players[0].characterIndex) && \
      IsYoshiOrDonkeyKong(g_Players[1].characterIndex) && \
      IsYoshiOrDonkeyKong(g_Players[2].characterIndex) && \
      IsYoshiOrDonkeyKong(g_Players[3].characterIndex))

void UpdateTurns(void) {
  s32 playerIndex;
  s32 state;
  s32 nextState;
  Boolean areCallsAllowed;
  s32 i;

  /* Returns whether the B button is held */
  Boolean IsBButtonHeld(void) {
    if (g_MergedController.buttons & B_BUTTON) {
      return TRUE;
    }

    return FALSE;
  }

  /* The turn controller's stick X, which left and right on the control pad tilt
   * fully */
  s8 GetTurnStickX(void) {
    s32 x = (g_TurnController.buttons & L_JPAD) ? -DPAD_STICK_TILT : 0;

    if (g_TurnController.buttons & R_JPAD) {
      x = DPAD_STICK_TILT;
    }

    if (x == 0) {
      x = g_TurnController.stickX;
    }

    return x;
  }

  /* The turn controller's stick Y, which down and up on the control pad tilt
   * fully */
  s8 GetTurnStickY(void) {
    s32 y = (g_TurnController.buttons & D_JPAD) ? -DPAD_STICK_TILT : 0;

    if (g_TurnController.buttons & U_JPAD) {
      y = DPAD_STICK_TILT;
    }

    if (y == 0) {
      y = g_TurnController.stickY;
    }

    return y;
  }

  /* Makes the golfer translucent while a club for the green is held */
  void UpdateGolferTranslucency(void) {
    if ((GetClub(g_Players[playerIndex].clubs[g_CurrentShot.clubSlot])
                ->surfaceMask &
            CLUB_SURFACE_GREEN) &&
        g_LetterboxState != LETTERBOX_STATE_LOOKING_BACK) {
      SetCharacterTranslucent(0, TRUE);
    } else {
      SetCharacterTranslucent(0, FALSE);
    }
  }

  /*
   * Poses the golfer for the picked club and shot mode, when either has
   * changed. An approach shows the swing's first frame, held; the other modes
   * show the stance.
   */
  void PoseGolferForShot(void) {
    s32* lastClubSlot = &D_80227150_ovl_00.clubIndex;

    if (*lastClubSlot != g_CurrentShot.clubSlot ||
        D_80227150_ovl_00.shotMode != g_CurrentShot.shotMode) {
      *lastClubSlot = g_CurrentShot.clubSlot;
      D_80227150_ovl_00.shotMode = g_CurrentShot.shotMode;
      SetCharacterClub(0, g_Players[playerIndex].clubs[g_CurrentShot.clubSlot]);
      UpdateGolferTranslucency();
      if (g_CurrentShot.shotMode == SHOT_MODE_APPROACH) {
        SetSwingAnimation(0,
            g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
            SHOT_MODE_APPROACH);
        SetAnimationSpeed(0, 0);
      } else {
        SetStanceAnimation(0,
            g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
            g_CurrentShot.shotMode);
        SkipToAnimationEnd(0);
      }
    }
  }

  /* Advances the player's, the round's and the hole's stopwatches by a frame,
   * up to their limit */
  void TickStopwatches(void) {
    if (D_80223FF8_ovl_00 < 2) {
      g_Players[playerIndex].stopwatchFrameCount++;
      g_GameState.stopwatchFrameCount++;
      g_HoleTimeFrames[g_GameState.holeIndex]++;
      if (g_Players[playerIndex].stopwatchFrameCount > STOPWATCH_MAX_FRAMES) {
        g_Players[playerIndex].stopwatchFrameCount = STOPWATCH_MAX_FRAMES;
      }

      if (g_GameState.stopwatchFrameCount > STOPWATCH_MAX_FRAMES) {
        g_GameState.stopwatchFrameCount = STOPWATCH_MAX_FRAMES;
      }

      if (g_HoleTimeFrames[g_GameState.holeIndex] > STOPWATCH_MAX_FRAMES) {
        g_HoleTimeFrames[g_GameState.holeIndex] = STOPWATCH_MAX_FRAMES;
      }
    }
  }

  /*
   * Takes the player out of the hole. In a match, a player whose opponent has
   * holed out loses the hole by a stroke; otherwise the hole counts as
   * GIVE_UP_PAR_MULTIPLE times its par, and an opponent still playing wins it
   * by a stroke.
   */
  void GiveUpHole(s32 playerIndex) {
    s32 opponentIndex;

    if (g_GameState.replayKind == REPLAY_KIND_NONE &&
        IsOneOnOneMatchMode(g_GameState.gameMode) &&
        g_Players[playerIndex ^ 1].holeStatus == HOLE_STATUS_HOLED_OUT) {
      g_Players[playerIndex]
          .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
          .strokes = g_Players[playerIndex ^ 1]
                         .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                         .strokes +
          1;
      g_Players[playerIndex].strokeCount =
          g_Players[playerIndex]
              .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
              .strokes -
          GetHandicapOffset(g_GameState.courseIndex, g_GameState.holeIndex,
              g_Players[playerIndex].handicap);
      g_Players[playerIndex].holeStatus = HOLE_STATUS_OUT;
    } else {
      g_Players[playerIndex].strokeCount =
          g_HoleSetup.par * GIVE_UP_PAR_MULTIPLE;
      g_Players[playerIndex]
          .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
          .strokes = g_HoleSetup.par * GIVE_UP_PAR_MULTIPLE;
      g_Players[playerIndex].holeStatus = HOLE_STATUS_OUT;
    }

    SkipUnmeasuredDistances(playerIndex);
    UpdateContest(FALSE);
    if (g_GameState.replayKind == REPLAY_KIND_NONE) {
      opponentIndex = playerIndex ^ 1;
      if (IsOneOnOneMatchMode(g_GameState.gameMode)) {
        g_Players[opponentIndex].holeStatus = HOLE_STATUS_HOLED_OUT;
        g_Players[opponentIndex]
            .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
            .strokes = g_Players[playerIndex]
                           .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                           .strokes -
            1;
        g_Players[opponentIndex].strokeCount =
            g_Players[opponentIndex]
                .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                .strokes -
            GetHandicapOffset(g_GameState.courseIndex, g_GameState.holeIndex,
                g_Players[opponentIndex].handicap);
        SkipUnmeasuredDistances(opponentIndex);
      }
    }
  }

  /* Shows or hides the HUD's parts, apart from the club panel and the letterbox
   */
  void SetRoundHudPartsShown(s8 isShown) {
    g_RoundHud.isWindIndicatorShown = isShown;
    g_RoundHud.unk8 = isShown;
    g_RoundHud.isLieIconShown = isShown;
    g_RoundHud.isClubHeadIconShown = isShown;
    g_RoundHud.unkB = isShown;
    g_RoundHud.unkD = isShown;
  }

  /* Returns whether a shot stopped this far from the impact mark rumbles, as a
   * mishit */
  Boolean DoesImpactRumble(void) {
    return GetUnkCEntry(GetSelectedClub(), g_CurrentShot.surface,
        g_CurrentShot.shotMode, D_80227150_ovl_00.impactTiming)
        ->unk5;
  }

  /* Plays the impact click: a mishit, a stop right on the mark, or one off it
   */
  void PlayImpactClickSound(void) {
    if (DoesImpactRumble()) {
      PlaySound(SOUND_MENU_BLOCKED, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
    } else if (D_80227150_ovl_00.impactTiming == 0) {
      PlaySound(SOUND_IMPACT_ON_MARK, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
    } else {
      PlaySound(SOUND_IMPACT_OFF_MARK, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
    }
  }

  areCallsAllowed = TRUE;
  if (IsGameFlagSet(GAME_FLAG_SHOW_COURSE_POINTS)) {
    PrintCoursePoints();
  }

  f_IsShotPreviewBuilt = FALSE;
  if (!(g_LetterboxState == LETTERBOX_STATE_NONE ||
          g_LetterboxState == LETTERBOX_STATE_UNK5)) {
    SetScreenEffect(SCREEN_EFFECT_NONE, 0);
  }

  if (IsGameFlagSet(GAME_FLAG_SHOW_PLAYER_UNKC)) {
    for (i = 0; i < g_GameState.playerCount; i++) {
      PrintDebugPaddedHex(g_Players[i].state, 2, i + 10);
    }
  }

  PlayAmbientSound();
  PickNextPlayer();
  for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
    s32 controllerIndex = g_Players[playerIndex].controllerIndex;

    state = g_Players[playerIndex].state;
    if (controllerIndex < MAXCONTROLLERS) {
      g_TurnController = g_ControllerStates[controllerIndex];
    } else {
      g_TurnController.stickX = 0;
      g_TurnController.stickY = 0;
      g_TurnController.buttons = 0;
      g_TurnController.repeatedButtons = 0;
      g_TurnController.pressedButtons = 0;
    }

    D_80227170_ovl_00[playerIndex]++;
    nextState = state;
    D_80227150_ovl_00.playerIndex = playerIndex;
    if (IsTimedState(state) && IsPlayGoingOn(g_PauseScreen)) {
      TickStopwatches();
    }

    if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
      g_RoundHud.stopwatchFrameCount =
          g_Players[playerIndex].stopwatchFrameCount;
    }

    switch (state) {
      case PLAYER_STATE_STARTING_TURN:
        f_ShouldUpdateReaction = 0;
        UnloadOverlay(GetOverlayOfModule(MODULE_SHOT_SIMULATION));
        f_ShotFlightCount = 0;
        D_80227184_ovl_00 = 127.0f;
        UpdateContest(FALSE);
        func_8005F1C8();
        g_RoundHud.flags = 0;
        g_RoundHud.unk17 = 0;
        g_RoundHud.unk87 = playerIndex;
        ResetShotOffsetMarker();
        if (IsCharacterSlotBusy(0)) {
          FreeCharacterAndReactionLight(0);
          break;
        }

        LoadCharacter(0, g_Players[playerIndex].characterIndex,
            g_Players[playerIndex].paletteIndex);
        if (g_Players[playerIndex].unk22) {
          SetTextureAnimationFrame(0, 1, 1);
          SetCharacterUnk188(0, 1);
        }

        SetCharacterLighting(0, 1);
        ClearGameFlag(GAME_FLAG_FAST_FLIGHT);
        StartPlayerShot(playerIndex);
        D_80227150_ovl_00.heading = g_CurrentShot.heading;
        g_RoundHud.clubHeadTilt =
            GetAimSlopeAngle(&g_Players[playerIndex], &D_80227150_ovl_00);
        g_RoundHud.lieTilt =
            GetCrossSlopeAngle(&g_Players[playerIndex], &D_80227150_ovl_00);
        UpdateShotFlags();
        PlaceBallAtRest(playerIndex, &g_TurnBallSimulation);
        g_CurrentShot.cupDistance =
            GetCupDistance(g_TurnBallSimulation.position[AXIS_X] -
                    g_HoleSetup.cupPosition[AXIS_X],
                g_TurnBallSimulation.position[AXIS_Z] -
                    g_HoleSetup.cupPosition[AXIS_Z]);
        g_RoundHud.greenGridState = 1;
        g_RoundHud.unkF = 1;
        g_RoundHud.unk12 = 1;
        D_80227150_ovl_00.clubIndex = g_CurrentShot.clubSlot;
        D_80227150_ovl_00.shotMode = g_CurrentShot.shotMode;
        g_ShotSetupFrameCount = 0;
        SetCharacterClub(
            0, g_Players[playerIndex].clubs[g_CurrentShot.clubSlot]);
        if (g_CurrentShot.shotMode == SHOT_MODE_APPROACH) {
          SetSwingAnimation(0,
              g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
              SHOT_MODE_APPROACH);
          SetAnimationSpeed(0, 0.0f);
        } else {
          SetStanceAnimation(0,
              g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
              g_CurrentShot.shotMode);
        }

        PlaceCharacterForShot(0);
        UpdateGolferTranslucency();
        if (g_GameState.replayKind != REPLAY_KIND_NONE) {
          if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
            func_80060128(g_Players[playerIndex].strokeCount);
          } else {
            func_800600C0(g_Players[playerIndex].strokeCount);
          }

          f_ShotSource = SHOT_SOURCE_PREPARED;
        } else if (IsGameFlagSet(GAME_FLAG_UNK32) ||
            g_Players[playerIndex].isComputer ||
            IsGameFlagSet(GAME_FLAG_UNK9B)) {
          g_Players[playerIndex].unk1F = -9;
          LoadOverlay(GetOverlayOfModule(MODULE_COMPUTER_GOLFER));
          StartShotPlan();
          D_800B7D9C = 0;
          f_PlanTimeBudget = PLAN_START_TIME_BUDGET;
          f_ShotSource = SHOT_SOURCE_PLANNING;
        } else if (f_ShouldReplaySavedShot) {
          f_ShotSource = SHOT_SOURCE_PREPARED;
          D_80104F78 = g_SavedShots[playerIndex];
          f_ShouldReplaySavedShot = 0;
        } else {
          f_ShotSource = SHOT_SOURCE_PLAYER;
        }

        f_AimFrameCount = 0;
        f_ReplayAimSettledFrameCount = 0;
        if (g_GameState.replayKind == REPLAY_KIND_NONE &&
            IsOneOnOneMatchMode(g_GameState.gameMode)) {
          s32 scoreToPar = GetNetScoreToPar(g_GameState.holeIndex, playerIndex);

          g_RoundHud.skinsTotals[playerIndex] = scoreToPar -
              GetNetScoreToPar(g_GameState.holeIndex, playerIndex ^ 1);
        } else {
          g_RoundHud.skinsTotals[playerIndex] =
              GetNetScoreToPar(g_GameState.holeIndex, playerIndex);
        }

        switch (GetHolingOutStakes(playerIndex)) {
          case HOLING_OUT_STAKES_NONE:
            break;
          case HOLING_OUT_TIES:
            StartMovie(MOVIE_SINK_TO_TIE);
            break;
          case HOLING_OUT_HALVES_HOLE:
            StartMovie(MOVIE_MISS_AND_LOSE);
            break;
          case HOLING_OUT_WINS_HOLE:
          case HOLING_OUT_WINS_MATCH:
            if (GetSurfaceInfo(g_CurrentShot.surface)->kind ==
                SURFACE_KIND_GREEN) {
              StartMovie(MOVIE_SINK_TO_WIN);
            }

            break;
        }
        {
          SongId song = SONG_ROUND_UNK31;

          if (g_GameState.replayKind != REPLAY_KIND_NONE) {
            PlaySong(song, SONG_FLAG_UNK10 | SONG_FLAG_UNREAD);
          } else if (GetSurfaceInfo(g_CurrentShot.surface)->kind ==
              SURFACE_KIND_GREEN) {
            song = SONG_MINI_GOLF;
            if (g_GameState.isMiniGolf) {
              PlaySong(song, SONG_FLAG_UNK10 | SONG_FLAG_UNREAD);
            } else if (g_GameState.gameMode != GAME_MODE_SPEED_GOLF &&
                g_GameState.gameMode != GAME_MODE_RING_SHOT) {
              s32 scoreToPar =
                  g_Players[playerIndex].strokeCount - (g_HoleSetup.par - 1);

              if (scoreToPar > SCORE_TO_PAR_PAR) {
                PlaySong(
                    SONG_CUP_IN_OVER_PAR, SONG_FLAG_UNK10 | SONG_FLAG_UNREAD);
                osSyncPrintf("BOGEY\n");
              } else if (scoreToPar < SCORE_TO_PAR_EAGLE) {
                PlaySong(
                    SONG_CUP_IN_UNDER_PAR, SONG_FLAG_UNK10 | SONG_FLAG_UNREAD);
                osSyncPrintf("EAGLE\n");
                if (GetHolingOutStakes(playerIndex) == HOLING_OUT_STAKES_NONE &&
                    !g_GameState.isMiniGolf) {
                  StartMovie(MOVIE_ALBATROSS_PUTT);
                }
              } else if (scoreToPar < SCORE_TO_PAR_BIRDIE) {
                PlaySong(
                    SONG_CUP_IN_UNDER_PAR, SONG_FLAG_UNK10 | SONG_FLAG_UNREAD);
                osSyncPrintf("EAGLE\n");
                if (GetHolingOutStakes(playerIndex) == HOLING_OUT_STAKES_NONE &&
                    !g_GameState.isMiniGolf) {
                  StartMovie(MOVIE_EAGLE_PUTT);
                }
              } else if (scoreToPar == SCORE_TO_PAR_PAR) {
                PlaySong(SONG_CUP_IN_PAR, SONG_FLAG_UNK10 | SONG_FLAG_UNREAD);
                osSyncPrintf("PAR\n");
              } else {
                PlaySong(
                    SONG_CUP_IN_UNDER_PAR, SONG_FLAG_UNK10 | SONG_FLAG_UNREAD);
                osSyncPrintf("BIRDIE\n");
                if (GetHolingOutStakes(playerIndex) == HOLING_OUT_STAKES_NONE &&
                    !g_GameState.isMiniGolf) {
                  StartMovie(MOVIE_BIRDIE_PUTT);
                }
              }
            }
          } else {
            song = SONG_MINI_GOLF;
            if (!g_GameState.isMiniGolf) {
              if (g_GameState.gameMode == GAME_MODE_TOURNAMENT) {
                switch (g_GameState.courseIndex) {
                  case COURSE_TOAD_HIGHLANDS:
                    song = SONG_TOURNAMENT_TOAD_HIGHLANDS;
                    break;
                  case COURSE_KOOPA_PARK:
                    song = SONG_TOURNAMENT_KOOPA_PARK;
                    break;
                  case COURSE_SHY_GUY_DESERT:
                    song = SONG_TOURNAMENT_SHY_GUY_DESERT;
                    break;
                  case COURSE_YOSHIS_ISLAND:
                    song = SONG_TOURNAMENT_YOSHIS_ISLAND;
                    break;
                  case COURSE_BOO_VALLEY:
                    song = SONG_TOURNAMENT_BOO_VALLEY;
                    break;
                  case COURSE_MARIOS_STAR:
                  default:
                    song = SONG_TOURNAMENT_MARIOS_STAR;
                    break;
                }
              } else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
                song = SONG_TRAINING;
              } else if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
                song = SONG_SPEED_GOLF;
              } else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
                song = SONG_RING_SHOT;
              } else if (GetLastHoleIndex() == g_GameState.holeIndex &&
                  g_GameState.playerCount != 1) {
                song = SONG_ROUND_FINAL;
              } else if (IsOneOnOneMatchMode(g_GameState.gameMode) &&
                  GetMatchStanding(0, 1) != MATCH_STANDING_OPEN) {
                song = SONG_ROUND_FINAL;
              } else if (IsYoshiAndDonkeyKongRound()) {
                song = SONG_YOSHI_AND_DONKEY_KONG;
              } else {
                switch (g_GameState.courseIndex) {
                  case COURSE_TOAD_HIGHLANDS:
                    song = SONG_COURSE_TOAD_HIGHLANDS;
                    break;
                  case COURSE_KOOPA_PARK:
                    song = SONG_COURSE_KOOPA_PARK;
                    break;
                  case COURSE_SHY_GUY_DESERT:
                    song = SONG_COURSE_SHY_GUY_DESERT;
                    break;
                  case COURSE_YOSHIS_ISLAND:
                    song = SONG_COURSE_YOSHIS_ISLAND;
                    break;
                  case COURSE_BOO_VALLEY:
                    song = SONG_COURSE_BOO_VALLEY;
                    break;
                  case COURSE_MARIOS_STAR:
                  default:
                    song = SONG_COURSE_MARIOS_STAR;
                    break;
                }
              }
            }

            PlaySong(song, SONG_FLAG_UNK10 | SONG_FLAG_UNREAD);
          }
        }

        StartShotPanel();
        nextState = PLAYER_STATE_PREPARING_AIM;
        if (!D_800C3094) {
          nextState = PLAYER_STATE_AIMING;
          if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
            nextState = PLAYER_STATE_HOLE_INTRO;
            StartHoleIntroCamera();
            func_8005F1C8();
          }
        }

        StartTurnRumble(g_GameState.currentPlayerIndex);
        break;
      case PLAYER_STATE_PREPARING_AIM:
        D_80227150_ovl_00.clubIndex = g_CurrentShot.clubSlot;
        D_80227150_ovl_00.shotMode = g_CurrentShot.shotMode;
        D_80227150_ovl_00.heading = g_CurrentShot.heading;
        if (f_ShotSource == SHOT_SOURCE_PLANNING) {
          StartTurnTimer();
          for (i = 0; f_ShotSource == SHOT_SOURCE_PLANNING &&
              i < PLAN_PREPARING_POLL_COUNT;
              i++) {
            f_ShotSource =
                (GetPlannedShot(&D_80104F78) != PLANNED_SHOT_UNFINISHED)
                ? SHOT_SOURCE_PREPARED
                : SHOT_SOURCE_PLANNING;
            if (!HasPlanTime(PLAN_PREPARING_TIME_BUDGET)) {
              break;
            }
          }
        }

        nextState = PLAYER_STATE_PREPARING_AIM;
        if (IsHudAnimationDone()) {
          nextState = PLAYER_STATE_AIMING;
        }

        break;
      case PLAYER_STATE_HOLE_INTRO:
        if (g_GameState.holeIndex == GetFirstHoleIndex()) {
          UpdateHoleIntroCamera();
        }

        if (UpdateHoleIntroCamera()) {
          nextState = PLAYER_STATE_AIMING;
        }

        TurnAim(0);
        PlaceCharacterForShot(0);
        if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
          SetFramebufferCount(TRIPLE_BUFFERING);
          SetScreenEffect(SCREEN_EFFECT_NONE, 0);
        }

        break;
      case PLAYER_STATE_AIMING:
        D_80223FF8_ovl_00 = 0;
        UpdateShotFlags();
        f_AimFrameCount++;
        if (g_PauseScreen == PAUSE_SCREEN_NONE) {
          if (f_ShotSource == SHOT_SOURCE_PLAYER &&
              g_TurnController.pressedButtons == START_BUTTON &&
              (g_TurnController.buttons & START_BUTTON)) {
            ForceStopRumble();
            StopSound(IMPACT_POINT_SOUND_CHANNEL);
            PlaySound(SOUND_PAUSE, PAUSE_SOUND_CHANNEL, SOUND_FULL_SCALE);
            g_PauseScreen = PAUSE_SCREEN_PAUSE_MENU;
            SetMenuBackdrop(MENU_BACKDROP_DIMMING);
            RequestPauseMenu(g_Players[playerIndex].strokeCount != 0, 0);
            HideRoundHud();
          }
        } else if (g_PauseScreen == PAUSE_SCREEN_PAUSE_MENU &&
            f_ShotSource == SHOT_SOURCE_PLAYER &&
            (g_TurnController.pressedButtons & (START_BUTTON | B_BUTTON))) {
          ForceStopRumble();
          PlaySound(SOUND_PAUSE, PAUSE_SOUND_CHANNEL, SOUND_FULL_SCALE);
          RequestPauseMenu(0, -1);
          g_PauseScreen = PAUSE_SCREEN_NONE;
          SetMenuBackdrop(MENU_BACKDROP_NONE);
          ResumeRound();
          break;
        }

        if (g_PauseScreen == PAUSE_SCREEN_PAUSE_MENU) {
          s32 choice = UpdatePauseMenu(&g_TurnController);

          if (choice == PAUSE_ACTION_RETURN_TO_MENU) {
            RequestPauseMenu(0, -1);
            func_80060358();
          } else if (choice == PAUSE_ACTION_START_OVER) {
            RequestPauseMenu(0, -1);
            ResumeRound();
            TakePinOut();
            ClearShotEffects(CLEAR_KIND_ALL);
            PlacePlayersOnTee();
            SetFramebufferCount(TRIPLE_BUFFERING);
            nextState = PLAYER_STATE_WAITING;
          } else if (choice == PAUSE_ACTION_HOLE_OUT) {
            RequestPauseMenu(0, -1);
            nextState = PLAYER_STATE_HOLE_DONE;
            EndFlightCamera();
            ClearShotEffects(CLEAR_KIND_ALL);
            FreeCharacterAndReactionLight(0);
          }
        } else if (g_PauseScreen == PAUSE_SCREEN_LOADING_SCORECARD) {
          if (g_IsModuleAlive[MODULE_SCORECARD]) {
            g_PauseScreen = PAUSE_SCREEN_SCORECARD;
            OpenScorecard(g_GameState.courseIndex, g_GameState.holeIndex);
            if (g_GameState.gameMode == GAME_MODE_TOURNAMENT) {
              SetScorecardLayout(SCORECARD_LAYOUT_TOURNAMENT);
            } else if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
              SetScorecardLayout(SCORECARD_LAYOUT_SPEED_GOLF);
            } else if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
              SetScorecardLayout(SCORECARD_LAYOUT_SKINS);
            } else if (IsOneOnOneMatchMode(g_GameState.gameMode)) {
              SetScorecardLayout(SCORECARD_LAYOUT_MATCH);
            } else {
              SetScorecardLayout(SCORECARD_LAYOUT_STROKE);
            }
          }
        } else if (g_PauseScreen == PAUSE_SCREEN_SCORECARD) {
          UpdateScorecard(g_MergedController.repeatedButtons,
              g_MergedController.pressedButtons);
        } else if (g_PauseScreen == PAUSE_SCREEN_LOADING_SAVE_MENU) {
          if (g_IsModuleAlive[MODULE_SAVE_MENU]) {
            g_PauseScreen = PAUSE_SCREEN_SAVE_MENU;
            OpenSaveMenu(1);
          }
        } else if (g_PauseScreen == PAUSE_SCREEN_LOADING_CONTROLS) {
          if (g_IsModuleAlive[MODULE_SAVE_MENU]) {
            g_PauseScreen = PAUSE_SCREEN_CONTROLS;
            OpenControlsScreen();
          }
        } else if (g_PauseScreen == PAUSE_SCREEN_CONTROLS) {
          UpdateControlsScreen(&g_TurnController);
        } else if (g_PauseScreen == PAUSE_SCREEN_SAVE_MENU) {
          UpdateGameSaveMenu(&g_TurnController);
        } else if (g_PauseScreen == PAUSE_SCREEN_UNK3) {
        } else if (g_PauseScreen == PAUSE_SCREEN_PROMPT) {
          switch (UpdatePrompt(&g_TurnController)) {
            case PROMPT_RESULT_GIVE_UP_HOLE:
              D_800C3094 = 1;
              SetMenuBackdrop(MENU_BACKDROP_NONE);
              ResumeRound();
              nextState = PLAYER_STATE_STARTING_REACTION;
              GiveUpHole(playerIndex);
              break;
            case PROMPT_RESULT_RESUME:
              g_PauseScreen = PAUSE_SCREEN_NONE;
              SetMenuBackdrop(MENU_BACKDROP_NONE);
              ResumeRound();
              break;
            case PROMPT_RESULT_SAVE_REPLAY:
              SaveReplayToSlot();
              CloseSaveMenu();
              g_PauseScreen = PAUSE_SCREEN_NONE;
              func_80060358();
              break;
            case PROMPT_RESULT_SAVE_GAME:
              SaveGameToSlot();
              g_PauseScreen = PAUSE_SCREEN_SAVE_MENU;
              break;
            case PROMPT_RESULT_END_GAME:
              LeaveGameSaveMenu();
            case PROMPT_RESULT_STAY_IN_SAVE_MENU:
              g_PauseScreen = PAUSE_SCREEN_SAVE_MENU;
              break;
            case PROMPT_RESULT_QUIT_GAME:
              g_PauseScreen = PAUSE_SCREEN_NONE;
              ResumeRound();
              func_80060358();
              break;
          }
        } else if (g_PauseScreen == PAUSE_SCREEN_FREE_LOOK) {
          if (g_TurnController.pressedButtons & (A_BUTTON | R_TRIG)) {
            g_PauseScreen = PAUSE_SCREEN_NONE;
            SetMenuBackdrop(MENU_BACKDROP_NONE);
            SetFramebufferCount(TRIPLE_BUFFERING);
            SetScreenEffect(SCREEN_EFFECT_NONE, 0);
            g_LetterboxState = LETTERBOX_STATE_NONE;
            PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
            ResetAimCamera();
            func_8005F1C8();
            g_RoundHud.unkC = 1;
            g_RoundHud.isLetterboxShown = TRUE;
            SetRoundHudPartsShown(1);
          } else {
            SetRoundHudPartsShown(
                (g_TurnController.buttons & START_BUTTON) ? 0 : 1);
            HandleClubInput();
            HandleShotModeInput();
            PoseGolferForShot();
            D_80227150_ovl_00.shotMode = g_CurrentShot.shotMode;
            UpdateAimControls();
            PlaceShotCamera();
            SetFramebufferCount(TRIPLE_BUFFERING);
            SetScreenEffect(SCREEN_EFFECT_NONE, 0);
          }

          D_80227150_ovl_00.offsetX = 0;
          D_80227150_ovl_00.offsetY = 0;
          D_80227150_ovl_00.heading = g_CurrentShot.heading;
          BuildShotPreview(&D_80227150_ovl_00, state);
        } else if (g_PauseScreen == PAUSE_SCREEN_NONE) {
          Boolean isReady;
          s32 tempo;
          s32 shotSource;

          if (f_ShotSource == SHOT_SOURCE_PLAYER &&
              (g_TurnController.buttons & (Z_TRIG | L_TRIG)) &&
              (g_TurnController.pressedButtons & R_TRIG)) {
            StartFreeLook();
            BuildShotPreview(&D_80227150_ovl_00, state);
            break;
          }

          SetCharacterSlotAlpha(0,
              (sinf(g_Players[playerIndex].stopwatchFrameCount * 0.20933335f) *
                  32.0f) +
                  64.0f);
          isReady = TRUE;
          tempo = 0x7F;
          if (GetSurfaceInfo(g_CurrentShot.surface)->kind ==
                  SURFACE_KIND_GREEN &&
              !g_GameState.isMiniGolf) {
            D_80227184_ovl_00 += 0.04f;
            if (D_80227184_ovl_00 > 155.0f) {
              D_80227184_ovl_00 = 155.0f;
            }

            if (IsGameFlagSet(GAME_FLAG_SHOW_SWING_TEMPO)) {
              sprintf(g_PrintBuffer, "tempo:%5.2f", D_80227184_ovl_00);
              PrintDebugText(g_PrintBuffer, 10, 10);
            }

            tempo = D_80227184_ovl_00;
          }

          SetSongTempo(tempo);
          shotSource = f_ShotSource;
          if (shotSource == SHOT_SOURCE_PLANNING) {
            if (g_ScreenEffect != SCREEN_EFFECT_SAVED_FRAME) {
              SetScreenEffect(
                  SCREEN_EFFECT_SAVED_FRAME, SAVED_FRAME_STEP_SAVING);
              SetFramebufferCount(DOUBLE_BUFFERING);
            }

            StartTurnTimer();
            for (i = 0; f_ShotSource == SHOT_SOURCE_PLANNING &&
                i < PLAN_AIMING_POLL_COUNT;
                i++) {
              f_ShotSource =
                  (GetPlannedShot(&D_80104F78) != PLANNED_SHOT_UNFINISHED)
                  ? SHOT_SOURCE_PREPARED
                  : SHOT_SOURCE_PLANNING;
              if (!HasPlanTime(f_PlanTimeBudget)) {
                break;
              }
            }

            f_PlanTimeBudget *= PLAN_TIME_BUDGET_GROWTH;
            if (f_PlanTimeBudget > PLAN_MAX_TIME_BUDGET) {
              f_PlanTimeBudget = PLAN_MAX_TIME_BUDGET;
            }

            if (f_ShotSource == SHOT_SOURCE_PLANNING) {
              break;
            }
          }

          if (shotSource != SHOT_SOURCE_PLAYER &&
              f_AimFrameCount < PREPARED_SHOT_MIN_AIM_FRAMES) {
            break;
          }

          UnloadOverlay(GetOverlayOfModule(MODULE_COMPUTER_GOLFER));
          if (f_ShotSource == SHOT_SOURCE_PREPARED) {
            isReady = StepToClubChoice(&D_80104F78);
          } else {
            if (g_ImpactPointState == IMPACT_POINT_OFF) {
              HandleClubInput();
            }

            HandleShotModeInput();
          }

          PoseGolferForShot();
          D_80224000_ovl_00 = GetAnimationEventFrame(0,
              GetAnimationIndex(0,
                  GetSwingAnimation(0,
                      g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
                      g_CurrentShot.shotMode)),
              6);
          D_80224004_ovl_00 = 0;
          if (IsGameFlagSet(GAME_FLAG_CHOOSE_CUP)) {
            DebugPlaceBall(playerIndex);
          }

          if (isReady) {
            isReady = UpdateAimControls() && isReady;
          }

          D_80227150_ovl_00.heading = g_CurrentShot.heading;
          PlaceCharacterForShot(0);
          UpdateGolferTranslucency();
          if (!(g_LetterboxState == LETTERBOX_STATE_NONE ||
                  g_LetterboxState == LETTERBOX_STATE_UNK5) &&
              g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
            SetScreenEffect(SCREEN_EFFECT_NONE, 0);
          }

          if (g_LetterboxState == LETTERBOX_STATE_NONE ||
              g_LetterboxState == LETTERBOX_STATE_UNK5) {
            if (g_CurrentShot.shotMode == SHOT_MODE_APPROACH) {
              SetSwingAnimation(0,
                  g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
                  SHOT_MODE_APPROACH);
              SetAnimationSpeed(0, 0.0f);
            } else {
              SetAnimationSpeed(0, 1.0f);
            }
          } else {
            SetStanceAnimation(0,
                g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
                g_CurrentShot.shotMode);
            SetAnimationSpeed(0, 0.0f);
          }

          if ((f_ShotSource != SHOT_SOURCE_PLAYER && isReady) ||
              (f_ShotSource == SHOT_SOURCE_PLAYER &&
                  g_HasPressedShotButton == TRUE)) {
            g_ShowsGroundWaves = 0;
            if (g_CurrentShot.shotMode != SHOT_MODE_APPROACH) {
              SkipToAnimationEnd(0);
            }

            g_CurrentShot.gaugeFrame = 0;
            g_CurrentShot.powerGaugeFrame = 0;
            g_CurrentShot.impactGaugeFrame = 0;
            PlaySound(
                SOUND_IMPACT_OFF_MARK, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
            SetFramebufferCount(DOUBLE_BUFFERING);
            nextState = PLAYER_STATE_SETTING_POWER;
            if (g_ScreenEffect != SCREEN_EFFECT_SAVED_FRAME) {
              SetScreenEffect(
                  SCREEN_EFFECT_SAVED_FRAME, SAVED_FRAME_STEP_SAVING);
            }
          }

          g_RoundHud.clubHeadTilt =
              GetAimSlopeAngle(&g_Players[playerIndex], &D_80227150_ovl_00);
          g_RoundHud.lieTilt =
              GetCrossSlopeAngle(&g_Players[playerIndex], &D_80227150_ovl_00);
          if (GetClub(D_80227150_ovl_00.clubIndex)->surfaceMask &
              CLUB_SURFACE_GREEN) {
            g_RoundHud.unk84[1] = 1;
          } else {
            g_RoundHud.unk84[1] = 0;
          }

          if (g_ImpactPointState == IMPACT_POINT_SETTING) {
            if (f_ShotSource == SHOT_SOURCE_PLAYER) {
              g_CurrentShot.offsetX = GetTurnStickX();
              g_CurrentShot.offsetY = GetTurnStickY();
            } else {
              g_CurrentShot.offsetX = 0;
              g_CurrentShot.offsetY = 0;
            }
          } else if (g_ImpactPointState == IMPACT_POINT_OFF) {
            g_CurrentShot.offsetX = 0;
            g_CurrentShot.offsetY = 0;
          }

          g_CurrentShot.gaugeFrame = 0;
          if (IsGameFlagSet(GAME_FLAG_UNK32)) {
            g_CurrentShot.gaugeFrame = D_80104F78.powerGaugeFrame;
          }

          SetShotOffsetMarker(g_CurrentShot.offsetX, g_CurrentShot.offsetY);
          D_80227150_ovl_00.offsetX = g_CurrentShot.offsetX;
          D_80227150_ovl_00.offsetY = g_CurrentShot.offsetY;
          PlaceShotCamera();
          if (g_ShotFlags & SHOT_FLAG_ON_GREEN) {
            if (g_LetterboxState == LETTERBOX_STATE_UNK5 ||
                g_LetterboxState == LETTERBOX_STATE_NONE) {
              g_RoundHud.flags = HUD_FLAG_AIMING | HUD_FLAG_UNK4;
            } else {
              g_RoundHud.flags =
                  HUD_FLAG_AIMING | HUD_FLAG_UNK4 | HUD_FLAG_TRANSLUCENT_GAUGE;
            }
          } else if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
            if (g_LetterboxState == LETTERBOX_STATE_UNK5 ||
                g_LetterboxState == LETTERBOX_STATE_NONE) {
              g_RoundHud.flags =
                  HUD_FLAG_AIMING | HUD_FLAG_LIGHT_CLUB_HEAD | HUD_FLAG_UNK4;
            } else {
              g_RoundHud.flags = HUD_FLAG_AIMING | HUD_FLAG_LIGHT_CLUB_HEAD |
                  HUD_FLAG_UNK4 | HUD_FLAG_TRANSLUCENT_GAUGE;
            }
          } else {
            if (g_LetterboxState == LETTERBOX_STATE_UNK5) {
              g_RoundHud.flags =
                  HUD_FLAG_AIMING | HUD_FLAG_ANIMATE_OFFSET_MARKER;
            } else if (g_LetterboxState == LETTERBOX_STATE_NONE) {
              g_RoundHud.flags = HUD_FLAG_AIMING | HUD_FLAG_LIGHT_CLUB_HEAD;
            } else {
              g_RoundHud.flags = HUD_FLAG_AIMING | HUD_FLAG_LIGHT_CLUB_HEAD |
                  HUD_FLAG_UNK4 | HUD_FLAG_TRANSLUCENT_GAUGE;
            }

            if (g_ImpactPointState == IMPACT_POINT_SETTING) {
              g_RoundHud.flags =
                  (g_RoundHud.flags | HUD_FLAG_ANIMATE_OFFSET_MARKER) &
                  ~HUD_FLAG_LIGHT_CLUB_HEAD;
            }
          }

          BuildShotPreview(&D_80227150_ovl_00, state);
        }

        break;
      case PLAYER_STATE_SETTING_POWER:
        UpdateImpactPointMode();
        ReuseLastCharacterMatrices(0);
        BuildShotPreview(&D_80227150_ovl_00, state);
        areCallsAllowed = FALSE;
        if (g_RoundHud.greenGridState == 1) {
          g_RoundHud.greenGridState = 2;
        }

        g_RoundHud.unkF = 0;
        g_RoundHud.unk12 = 0;
        if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
          g_RoundHud.flags = HUD_FLAG_SETTING_POWER;
        } else {
          g_RoundHud.flags =
              HUD_FLAG_SETTING_POWER | HUD_FLAG_ANIMATE_OFFSET_MARKER;
        }

        if (f_ShotSource == SHOT_SOURCE_PLAYER) {
          g_CurrentShot.offsetX = GetTurnStickX();
          g_CurrentShot.offsetY = GetTurnStickY();
        } else {
          g_CurrentShot.offsetX = 0;
          g_CurrentShot.offsetY = 0;
        }

        SetShotOffsetMarker(g_CurrentShot.offsetX, g_CurrentShot.offsetY);
        D_80227150_ovl_00.offsetX = g_CurrentShot.offsetX;
        D_80227150_ovl_00.offsetY = g_CurrentShot.offsetY;
        if ((f_ShotSource != SHOT_SOURCE_PLAYER)
                ? (D_80104F78.powerGaugeFrame == g_CurrentShot.gaugeFrame)
                : (g_TurnController.pressedButtons & A_BUTTON)) {
          SoundId sound;

          g_CurrentShot.powerGaugeFrame = g_CurrentShot.gaugeFrame;
          D_80227150_ovl_00.power =
              GetGaugeFramePower(g_CurrentShot.gaugeFrame);
          g_SavedPlayers[playerIndex] = g_Players[playerIndex];
          g_Players[playerIndex].strokeCount++;
          if (g_Players[playerIndex].strokeCount > STROKE_COUNT_MAX) {
            g_Players[playerIndex].strokeCount = STROKE_COUNT_MAX;
          }

          if (GetSelectedClub()->surfaceMask & CLUB_SURFACE_GREEN) {
            if (GetSurfaceInfo(g_CurrentShot.surface)->kind ==
                SURFACE_KIND_GREEN) {
              g_Players[playerIndex].puttCount++;
              if (g_Players[playerIndex].puttCount > STROKE_COUNT_MAX) {
                g_Players[playerIndex].puttCount = STROKE_COUNT_MAX;
              }
            }

            D_80227150_ovl_00.impactTiming = 0;
            D_80227150_ovl_00.offsetX = 0;
            D_80227150_ovl_00.offsetY = 0;
            nextState = PLAYER_STATE_STARTING_SWING;
            if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
              SetScreenEffect(SCREEN_EFFECT_NONE, 0);
            }

            sound = SOUND_IMPACT_OFF_MARK;
          } else {
            nextState = PLAYER_STATE_SETTING_IMPACT;
            if (D_80227150_ovl_00.power == MAX_LAUNCH_POWER) {
              StartShotEffect(1);
              sound = SOUND_IMPACT_ON_MARK;
            } else {
              sound = SOUND_IMPACT_OFF_MARK;
            }
          }

          PlaySound(sound, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
        } else if (g_CurrentShot.gaugeFrame >= LAST_POWER_GAUGE_FRAME ||
            (f_ShotSource == SHOT_SOURCE_PLAYER &&
                (g_TurnController.pressedButtons & B_BUTTON))) {
          UpdateGolferTranslucency();
          nextState = PLAYER_STATE_AIMING;
          ResetShotModeChange();
          PlaySound(
              SOUND_MENU_BLOCKED, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
          if (g_RoundHud.greenGridState != 0) {
            g_RoundHud.greenGridState = 1;
          }

          g_RoundHud.unkF = 1;
          g_RoundHud.unk12 = 1;
        }

        g_CurrentShot.gaugeFrame++;
        break;
      case PLAYER_STATE_SETTING_IMPACT:
        UpdateImpactPointMode();
        ReuseLastCharacterMatrices(0);
        BuildShotPreview(&D_80227150_ovl_00, state);
        areCallsAllowed = FALSE;
        if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
          g_RoundHud.flags = HUD_FLAG_SETTING_IMPACT;
        } else {
          g_RoundHud.flags =
              HUD_FLAG_SETTING_IMPACT | HUD_FLAG_ANIMATE_OFFSET_MARKER;
        }

        if (f_ShotSource == SHOT_SOURCE_PLAYER) {
          g_CurrentShot.offsetX = GetTurnStickX();
          g_CurrentShot.offsetY = GetTurnStickY();
        } else {
          s32 framesLeft = D_80104F78.impactGaugeFrame -
              g_CurrentShot.gaugeFrame - STICK_HOLD_FRAME_COUNT;

          if (framesLeft > STICK_TILT_FRAME_COUNT) {
            g_CurrentShot.offsetX = 0;
            g_CurrentShot.offsetY = 0;
          } else if (framesLeft <= 0) {
            g_CurrentShot.offsetX = D_80104F78.offsetX;
            g_CurrentShot.offsetY = D_80104F78.offsetY;
          } else {
            g_CurrentShot.offsetX = D_80104F78.offsetX *
                (STICK_TILT_FRAME_COUNT - framesLeft) / STICK_TILT_FRAME_COUNT;
            g_CurrentShot.offsetY = D_80104F78.offsetY *
                (STICK_TILT_FRAME_COUNT - framesLeft) / STICK_TILT_FRAME_COUNT;
          }
        }

        SetShotOffsetMarker(g_CurrentShot.offsetX, g_CurrentShot.offsetY);
        D_80227150_ovl_00.offsetX = g_CurrentShot.offsetX;
        D_80227150_ovl_00.offsetY = g_CurrentShot.offsetY;
        if (((f_ShotSource != SHOT_SOURCE_PLAYER)
                    ? (g_CurrentShot.gaugeFrame == D_80104F78.impactGaugeFrame)
                    : (g_TurnController.pressedButtons & A_BUTTON)) ||
            g_CurrentShot.gaugeFrame >= LAST_IMPACT_GAUGE_FRAME) {
          g_CurrentShot.impactGaugeFrame = g_CurrentShot.gaugeFrame;
          D_80227150_ovl_00.impactTiming =
              g_CurrentShot.gaugeFrame - GAUGE_IMPACT_FRAME;
          if (D_80227150_ovl_00.impactTiming > MAX_IMPACT_TIMING) {
            D_80227150_ovl_00.impactTiming = MAX_IMPACT_TIMING;
          }

          if (D_80227150_ovl_00.impactTiming < -MAX_IMPACT_TIMING) {
            D_80227150_ovl_00.impactTiming = -MAX_IMPACT_TIMING;
          }

          PlayImpactClickSound();
          StopSound(IMPACT_POINT_SOUND_CHANNEL);
          nextState = PLAYER_STATE_STARTING_SWING;
          if (g_CurrentShot.shotMode == SHOT_MODE_POWER &&
              (D_80227150_ovl_00.impactTiming != 0 ||
                  D_80227150_ovl_00.power < MAX_LAUNCH_POWER)) {
            if (g_Players[playerIndex].unk28 > 0) {
              g_Players[playerIndex].unk28--;
            }
          }

          if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
            SetScreenEffect(SCREEN_EFFECT_NONE, 0);
          }
        } else {
          g_CurrentShot.gaugeFrame++;
          if (SWING_TIMING_GAUGE_FRAME - g_CurrentShot.gaugeFrame <
                  D_80224000_ovl_00 &&
              !D_80224004_ovl_00) {
            D_80227170_ovl_00[playerIndex] = 0;
            D_80224004_ovl_00 = 1;
            SetCharacterAnimation(0,
                GetSwingAnimation(0,
                    g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
                    g_CurrentShot.shotMode));
            SetAnimationSpeed(0, 1.0f);
            SetPlaysRandomTextureAnimation(0, 0);
          }
        }

        if (g_IsDebugEnabled &&
            (g_TurnController.pressedButtons & (B_BUTTON | D_CBUTTONS))) {
          g_CurrentShot.impactGaugeFrame = g_CurrentShot.gaugeFrame;
          if (g_TurnController.pressedButtons & B_BUTTON) {
            D_80227150_ovl_00.power = MAX_LAUNCH_POWER;
          }

          D_80227150_ovl_00.impactTiming = 0;
          if (D_80227150_ovl_00.impactTiming > MAX_IMPACT_TIMING) {
            D_80227150_ovl_00.impactTiming = MAX_IMPACT_TIMING;
          }

          if (D_80227150_ovl_00.impactTiming < -MAX_IMPACT_TIMING) {
            D_80227150_ovl_00.impactTiming = -MAX_IMPACT_TIMING;
          }

          PlayImpactClickSound();
          StopSound(IMPACT_POINT_SOUND_CHANNEL);
          nextState = PLAYER_STATE_STARTING_SWING;
          if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
            SetScreenEffect(SCREEN_EFFECT_NONE, 0);
          }
        }

        break;
      case PLAYER_STATE_STARTING_SWING: {
        s32 shotMode;

        EndRumbleForceStop();
        ReuseLastCharacterMatrices(0);
        g_RoundHud.flags = HUD_FLAG_SWINGING;
        D_80223FF8_ovl_00++;
        g_SavedShots[playerIndex] = g_CurrentShot;
        SetCharacterTranslucent(0, FALSE);
        SetFramebufferCount(TRIPLE_BUFFERING);
        PlaceBallAtRest(playerIndex, &g_TurnBallSimulation);
        LoadOverlay(GetOverlayOfModule(MODULE_SHOT_SIMULATION));
        SimulateShot(&D_80227150_ovl_00);
        if (IsRecordableHoleOut(playerIndex)) {
          RecordHoleOut(playerIndex);
        }

        if (IsRingShotRound()) {
          RecordRingShotHole(playerIndex);
        }

        if (IsGameFlagSet(GAME_FLAG_UNK32)) {
          SetGameFlag(GAME_FLAG_FAST_FLIGHT);
        }

        D_8022400C_ovl_00 = 0;
        D_80224008_ovl_00 = 0;
        if (!D_80224004_ovl_00) {
          D_80227170_ovl_00[playerIndex] = 0;
          if (GetSelectedClub()->surfaceMask & CLUB_SURFACE_GREEN) {
            s32 power = D_80227150_ovl_00.power;

            if (D_80227150_ovl_00.shotMode == 0) {
              power *= 6;
            }

            if (D_80227150_ovl_00.shotMode == 1) {
              power *= 3;
            }

            if (power >= 0x80) {
              shotMode = 1;
            } else {
              shotMode = 2;
              if (power < 0x40) {
                f32 speed = (f32)GetPuttPower(power, 2) / GetPuttPower(0x40, 2);

                D_80224008_ovl_00 = 3;
                D_80227180_ovl_00 = speed;
                D_8022400C_ovl_00 = (3.0f / speed) - 3.0f;
              }
            }

            if (D_80223FF8_ovl_00 < 2) {
              D_80224000_ovl_00 = GetAnimationEventFrame(0,
                  GetAnimationIndex(0,
                      GetSwingAnimation(0,
                          g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
                          shotMode)),
                  6);
            }
          } else {
            shotMode = g_CurrentShot.shotMode;
          }

          SetCharacterAnimation(0,
              GetSwingAnimation(0,
                  g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
                  shotMode));
          SetPlaysRandomTextureAnimation(0, 0);
          if (D_80223FF8_ovl_00 >= 3) {
            if (D_80223FF8_ovl_00 == 3) {
              if (D_80224000_ovl_00 > 15) {
                D_80227190_ovl_00 = D_80224000_ovl_00 - 15;
                D_80224000_ovl_00 -= D_80227190_ovl_00;
              } else {
                D_80227190_ovl_00 = 0;
              }
            }

            AdvanceAnimation(0, D_80227190_ovl_00);
          }
        }

        D_80224004_ovl_00 = 0;
        SetAnimationSpeed(0, 1.0f);
        nextState = PLAYER_STATE_SWINGING;
        StartFlightCamera(f_ShotFlightCount++);
        D_80223FFC_ovl_00 = 0;
        break;
      }
      case PLAYER_STATE_SWINGING:
        if (D_80227170_ovl_00[playerIndex] == D_80224000_ovl_00 - 3 &&
            GetClub(g_Players[playerIndex].clubs[g_CurrentShot.clubSlot])
                    ->kind == CLUB_KIND_WOOD) {
          if (g_CurrentShot.shotMode == SHOT_MODE_NORMAL) {
            StartSwingTrail(SWING_TRAIL_KIND_NORMAL_SHOT);
          } else if (g_CurrentShot.shotMode == SHOT_MODE_POWER) {
            StartSwingTrail(SWING_TRAIL_KIND_POWER_SHOT);
          }
        }

        ReuseLastCharacterMatrices(0);
        if (D_80224008_ovl_00 != 0 &&
            D_80227170_ovl_00[playerIndex] + D_80224008_ovl_00 ==
                D_80224000_ovl_00) {
          SetAnimationSpeed(0, D_80227180_ovl_00);
        }

        if (D_80227170_ovl_00[playerIndex] >=
                D_80224000_ovl_00 + D_8022400C_ovl_00 ||
            (IsGameFlagSet(GAME_FLAG_FAST_FLIGHT) &&
                D_80227170_ovl_00[playerIndex] >= 4)) {
          if (IsBButtonHeld()) {
            D_80223FFC_ovl_00 = 1;
          }

          if (g_IsModuleAlive[MODULE_GOLF_BALLS] ||
              ReportModuleNotAlive(MODULE_GOLF_BALLS)) {
            ResetBallRotation();
          }

          LaunchBallSimulation(&g_TurnBallSimulation, 0, &D_80227150_ovl_00);
          nextState = PLAYER_STATE_BALL_IN_FLIGHT;
          if (D_80227150_ovl_00.impactTiming == 0 &&
              D_80227150_ovl_00.power >= MAX_LAUNCH_POWER &&
              !(GetClub(D_80227150_ovl_00.clubIndex)->surfaceMask &
                  CLUB_SURFACE_GREEN) &&
              RateShotLength() == 1) {
            StartBanner(0);
          }
        } else {
          UpdateFlightCamera(FALSE, g_TurnController.buttons);
          break;
        }
      case PLAYER_STATE_BALL_IN_FLIGHT: {
        Boolean isCameraDone;
        s32 previousPosition[3];

        if (D_80224000_ovl_00 + 10 == D_80227170_ovl_00[playerIndex]) {
          StartSwingTrail(SWING_TRAIL_KIND_FADE);
        }

        ReuseLastCharacterMatrices(0);
        previousPosition[AXIS_X] = g_TurnBallSimulation.position[AXIS_X];
        previousPosition[AXIS_Y] = g_TurnBallSimulation.position[AXIS_Y];
        previousPosition[AXIS_Z] = g_TurnBallSimulation.position[AXIS_Z];
        StepBallSimulation(&g_TurnBallSimulation);
        func_80207660_ovl_00(&g_Players[playerIndex]);
        isCameraDone = UpdateFlightCamera(TRUE, g_TurnController.buttons);
        if (D_80227170_ovl_00[playerIndex] == 30) {
          SetPlaysRandomTextureAnimation(0, 1);
        }

        if (IsBButtonHeld()) {
          D_80223FFC_ovl_00 = 1;
        }

        if (D_80223FF8_ovl_00 >= 2 && D_80227170_ovl_00[playerIndex] >= 40 &&
            D_80223FFC_ovl_00) {
          nextState = PLAYER_STATE_STARTING_SWING;
        }

        if (g_MergedController.buttons & (Z_TRIG | L_TRIG)) {
          u32 previousDistance = IntLength2D(
              previousPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
              previousPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]);

          for (i = 0; i < 4; i++) {
            u32 distance = IntLength2D(g_TurnBallSimulation.position[AXIS_X] -
                    g_HoleSetup.cupPosition[AXIS_X],
                g_TurnBallSimulation.position[AXIS_Z] -
                    g_HoleSetup.cupPosition[AXIS_Z]);

            if (g_TurnBallSimulation.state != BALL_STATE_ROLLING ||
                g_GameState.isMiniGolf ||
                (GetSurfaceInfo(g_TurnBallSimulation.unkA4)->kind ==
                        SURFACE_KIND_GREEN &&
                    (s32)distance < (s32)previousDistance)) {
              break;
            }

            StepBallSimulation(&g_TurnBallSimulation);
            TickStopwatches();
            isCameraDone = UpdateFlightCamera(TRUE, g_TurnController.buttons);
          }
        }

        if (D_80223FF8_ovl_00 >= 2 || g_IsDebugEnabled) {
          if (g_IsDebugEnabled) {
            if ((g_MergedController.buttons & (Z_TRIG | L_TRIG)) &&
                !isCameraDone) {
              StepBallSimulation(&g_TurnBallSimulation);
              func_80207660_ovl_00(&g_Players[playerIndex]);
              isCameraDone = UpdateFlightCamera(TRUE, g_TurnController.buttons);
            }

            if (IsGameFlagSet(GAME_FLAG_FAST_FLIGHT) ||
                (g_TurnController.buttons & (Z_TRIG | L_TRIG))) {
              for (i = 0; i < 3; i++) {
                if (!isCameraDone) {
                  StepBallSimulation(&g_TurnBallSimulation);
                  TickStopwatches();
                  func_80207660_ovl_00(&g_Players[playerIndex]);
                  isCameraDone =
                      UpdateFlightCamera(TRUE, g_TurnController.buttons);
                }
              }

              if (g_IsDebugEnabled &&
                  (g_TurnController.pressedButtons & (Z_TRIG | L_TRIG))) {
                ToggleGameFlag(GAME_FLAG_FAST_FLIGHT);
              }
            }
          }

          if (g_MergedController.buttons & A_BUTTON) {
            while (g_TurnBallSimulation.state) {
              StepBallSimulation(&g_TurnBallSimulation);
              TickStopwatches();
              UpdateFlightCamera(TRUE, g_TurnController.buttons);
            }

            ResetShotEffects();
            ClearShotEffects(CLEAR_KIND_NICE_SHOT_BANNER);
          }
        }

        if (!g_TurnBallSimulation.state) {
          ClearShotEffects(CLEAR_KIND_EFFECTS);
          nextState = PLAYER_STATE_BALL_AT_REST;
          D_800C3094 = 1;
          D_80227170_ovl_00[playerIndex] = 0;
          ForceStopRumble();
        }

        break;
      }
      case PLAYER_STATE_BALL_AT_REST: {
        Boolean isCameraDone =
            UpdateFlightCamera(TRUE, g_TurnController.buttons);

        if (g_TurnBallSimulation.isHoledOut) {
          if (GetEndReaction() == 1) {
            if (D_80227170_ovl_00[playerIndex] == 1) {
              PlayHoleOutSong(playerIndex);
            }

            if (D_80227170_ovl_00[playerIndex] == 29) {
              StartBanner(-1);
            }

            if (D_80227170_ovl_00[playerIndex] == 30) {
              ShowHoleOutBanner(playerIndex);
            }
          } else if (GetEndReaction() == 2) {
            if (D_80227170_ovl_00[playerIndex] == 1) {
              PlayHoleOutSong(playerIndex);
            }

            if (D_80227170_ovl_00[playerIndex] == 29) {
              ShowHoleOutBanner(playerIndex);
            }
          } else if (D_80227170_ovl_00[playerIndex] == 1) {
            PlayHoleOutSong(playerIndex);
            ShowHoleOutBanner(playerIndex);
          }
        }

        if (IsBButtonHeld()) {
          D_80223FFC_ovl_00 = 1;
        }

        if (D_80223FFC_ovl_00) {
          nextState = PLAYER_STATE_STARTING_SWING;
          StartLandingReaction(-1);
          StartBanner(-1);
        } else if (isCameraDone) {
          UnkB8* player;
          SurfaceInfo* surfaceInfo;

          MeasureTeeShot(playerIndex);
          UpdateContest(FALSE);
          StartLandingReaction(-1);
          player = &g_Players[playerIndex];
          surfaceInfo = GetSurfaceInfo(g_TurnBallSimulation.surface);
          func_80207660_ovl_00(player);
          ClearPassedRings(player);
          if (IsOutOfBoundsKind(surfaceInfo->kind) &&
              !IsGameFlagSet(GAME_FLAG_NO_OUT_OF_BOUNDS_PENALTY)) {
            g_Players[playerIndex].strokeCount++;
            if (g_GameState.gameMode == GAME_MODE_TRAINING) {
              if (g_Players[playerIndex].strokeCount > STROKE_COUNT_MAX) {
                g_Players[playerIndex].strokeCount = STROKE_COUNT_MAX;
              }
            } else if (g_Players[playerIndex].strokeCount >
                g_HoleSetup.par * STROKE_LIMIT_PER_PAR) {
              g_Players[playerIndex].strokeCount =
                  g_HoleSetup.par * STROKE_LIMIT_PER_PAR;
            }

            if ((s8)player->ballSurface != SURFACE_TEE_UP) {
              player->ballSurface = GetSurface(
                  player->ballPosition[AXIS_X], player->ballPosition[AXIS_Z]);
            }
          } else if (surfaceInfo->kind == SURFACE_KIND_WATER_HAZARD) {
            player->ballPosition[AXIS_X] =
                g_TurnBallSimulation.dropPosition[AXIS_X];
            player->ballPosition[AXIS_Z] =
                g_TurnBallSimulation.dropPosition[AXIS_Z];
            player->ballPosition[AXIS_Y] = GetGroundHeight(
                player->ballPosition[AXIS_X], player->ballPosition[AXIS_Z]);
            player->ballSurface = GetSurface(
                player->ballPosition[AXIS_X], player->ballPosition[AXIS_Z]);
            g_Players[playerIndex].strokeCount++;
            if (g_GameState.gameMode == GAME_MODE_TRAINING) {
              if (g_Players[playerIndex].strokeCount > STROKE_COUNT_MAX) {
                g_Players[playerIndex].strokeCount = STROKE_COUNT_MAX;
              }
            } else if (g_Players[playerIndex].strokeCount >
                g_HoleSetup.par * STROKE_LIMIT_PER_PAR) {
              g_Players[playerIndex].strokeCount =
                  g_HoleSetup.par * STROKE_LIMIT_PER_PAR;
            }
          } else {
            player->ballPosition[AXIS_X] =
                g_TurnBallSimulation.position[AXIS_X];
            player->ballPosition[AXIS_Y] =
                g_TurnBallSimulation.position[AXIS_Y];
            player->ballPosition[AXIS_Z] =
                g_TurnBallSimulation.position[AXIS_Z];
            player->ballSurface = g_TurnBallSimulation.surface;
          }

          if (IsBunkerKind(surfaceInfo->kind) ||
              surfaceInfo->kind == SURFACE_KIND_BUNKER_UNK9) {
            player->hasBeenInBunker = TRUE;
          }

          D_80227170_ovl_00[playerIndex] = 0;
          if (g_TurnBallSimulation.isHoledOut) {
            g_Players[playerIndex].holeStatus = HOLE_STATUS_HOLED_OUT;
            g_Players[playerIndex]
                .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                .strokes = g_Players[playerIndex].strokeCount;
            if (g_GameState.replayKind == REPLAY_KIND_NONE &&
                (IsOneOnOneMatchMode(g_GameState.gameMode) ||
                    (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                        g_GameState.gameMode == GAME_MODE_CLUB_SLOTS))) {
              g_Players[playerIndex]
                  .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                  .strokes +=
                  GetHandicapOffset(GetCourseRecordIndex(g_GameState.gameMode),
                      g_GameState.holeIndex, g_Players[playerIndex].handicap);
            }

            nextState = PLAYER_STATE_STARTING_REACTION;
            g_Players[playerIndex]
                .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                .putts = g_Players[playerIndex].puttCount;
            FreeCharacterAndReactionLight(0);
          } else {
            FreeCharacterAndReactionLight(0);
            StartBanner(-1);
            if (g_GameState.replayKind != REPLAY_KIND_NONE &&
                g_Players[playerIndex].strokeCount >= D_800FE330) {
              nextState = PLAYER_STATE_STARTING_REACTION;
            } else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
              nextState = PLAYER_STATE_STARTING_REACTION;
              if (g_Players[playerIndex].strokeCount < g_HoleSetup.par) {
                EndFlightCamera();
                StartHudAnimation(2);
                nextState = PLAYER_STATE_ENDING_TURN;
              }
            } else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
              nextState = PLAYER_STATE_CHOOSING;
              OpenChoiceBox(
                  g_GameState.courseIndex == DRIVING_RANGE_COURSE_INDEX
                      ? CHOICE_BOX_ID_DRIVING_RANGE
                      : CHOICE_BOX_ID_TRAINING_SHOT);
            } else if (g_GameState.replayKind == REPLAY_KIND_NONE &&
                IsOneOnOneMatchMode(g_GameState.gameMode)) {
              if (g_Players[playerIndex].strokeCount >=
                      g_HoleSetup.par * STROKE_LIMIT_PER_PAR ||
                  GetMatchHoleResult(0, 1)) {
                nextState = PLAYER_STATE_STARTING_REACTION;
                GiveUpHole(playerIndex);
              } else {
                EndFlightCamera();
                StartHudAnimation(2);
                nextState = PLAYER_STATE_ENDING_TURN;
              }
            } else if (g_GameState.replayKind == REPLAY_KIND_NONE &&
                (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                    g_GameState.gameMode == GAME_MODE_CLUB_SLOTS)) {
              s8 results[MAX_PLAYER_COUNT];

              if (g_Players[playerIndex].strokeCount >=
                  g_HoleSetup.par * STROKE_LIMIT_PER_PAR) {
                g_Players[playerIndex]
                    .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                    .strokes = 0;
                g_Players[playerIndex].holeStatus = HOLE_STATUS_OUT;
              }

              if (GetGroupHoleResult(results) ||
                  results[playerIndex] == GROUP_PLAYER_OUT) {
                g_Players[playerIndex].holeStatus = HOLE_STATUS_OUT;
                g_Players[playerIndex]
                    .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                    .strokes = 0;
                g_Players[playerIndex].strokeCount = 0;
                nextState = PLAYER_STATE_STARTING_REACTION;
              } else {
                EndFlightCamera();
                StartHudAnimation(2);
                nextState = PLAYER_STATE_ENDING_TURN;
              }
            } else if (g_Players[playerIndex].strokeCount >=
                g_HoleSetup.par * STROKE_LIMIT_PER_PAR) {
              g_Players[playerIndex]
                  .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                  .strokes = g_HoleSetup.par * STROKE_LIMIT_PER_PAR;
              g_Players[playerIndex].holeStatus = HOLE_STATUS_OUT;
              nextState = PLAYER_STATE_STARTING_REACTION;
              if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
                g_Players[playerIndex].stopwatchFrameCount +=
                    STROKE_LIMIT_PENALTY_MINUTES * FRAMES_PER_MINUTE;
                g_GameState.stopwatchFrameCount +=
                    STROKE_LIMIT_PENALTY_MINUTES * FRAMES_PER_MINUTE;
                g_HoleTimeFrames[g_GameState.holeIndex] +=
                    STROKE_LIMIT_PENALTY_MINUTES * FRAMES_PER_MINUTE;
                if (g_Players[playerIndex].stopwatchFrameCount >
                    STOPWATCH_MAX_FRAMES) {
                  g_Players[playerIndex].stopwatchFrameCount =
                      STOPWATCH_MAX_FRAMES;
                }

                if (g_GameState.stopwatchFrameCount > STOPWATCH_MAX_FRAMES) {
                  g_GameState.stopwatchFrameCount = STOPWATCH_MAX_FRAMES;
                }

                if (g_HoleTimeFrames[g_GameState.holeIndex] >
                    STOPWATCH_MAX_FRAMES) {
                  g_HoleTimeFrames[g_GameState.holeIndex] =
                      STOPWATCH_MAX_FRAMES;
                }

                nextState = PLAYER_STATE_STARTING_REACTION;
              }
            } else {
              EndFlightCamera();
              if (!IsGameFlagSet(GAME_FLAG_UNK32)) {
                StartHudAnimation(2);
              }

              nextState = PLAYER_STATE_ENDING_TURN;
            }
          }
        }

        break;
      }
      case PLAYER_STATE_STARTING_REACTION: {
        CharacterId character;
        s32 palette;
        s32 reaction;

        if (IsCharacterSlotBusy(0)) {
          FreeCharacterAndReactionLight(0);
          break;
        }

        if (g_HoleSetup.conditions.sceneColorIndex ==
            SCENE_COLOR_INDEX_SUDDEN_DEATH) {
          StartFireworks();
        }

        ClearReactionSpot();
        ConsiderReactionSpot(g_CurrentShot.ballPosition);
        ConsiderReactionSpot(g_HoleSetup.cupPosition);
        ConsiderReactionSpot(g_HoleSetup.teePosition);
        ClearShotEffects(CLEAR_KIND_EFFECTS);
        EndFlightCamera();
        if (IsGameFlagSet(GAME_FLAG_SHOW_REACTION_FRAME) && f_HasGutsTestRun) {
          character = f_GutsTestCharacter;
          palette = 0;
        } else {
          character = g_Players[playerIndex].characterIndex;
          palette = g_Players[playerIndex].paletteIndex;
        }

        LoadCharacter(0, character + CHARACTER_COUNT, palette);
        SetCharacterLighting(0, 1);
        memset(&g_RoundHud, 0, sizeof(g_RoundHud));
        if (!g_TurnBallSimulation.isHoledOut) {
          PlaySong(SONG_HOLE_LOST, 0);
          reaction = REACTION_ANIMATION_BOGEY;
        } else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
          reaction = REACTION_ANIMATION_BOGEY;
          if (HasNoRingsLeft(&g_Players[playerIndex])) {
            func_8006034C(1);
            reaction = REACTION_ANIMATION_EAGLE;
          }
        } else if (g_Players[playerIndex].strokeCount == 1) {
          reaction = REACTION_ANIMATION_EAGLE;
        } else {
          switch (g_Players[playerIndex].strokeCount - g_HoleSetup.par) {
            case SCORE_TO_PAR_ALBATROSS:
            case SCORE_TO_PAR_EAGLE:
              reaction = REACTION_ANIMATION_EAGLE;
              break;
            case SCORE_TO_PAR_BIRDIE:
              reaction = REACTION_ANIMATION_BIRDIE;
              break;
            case SCORE_TO_PAR_PAR:
              reaction = REACTION_ANIMATION_PAR;
              break;
            default:
              reaction = REACTION_ANIMATION_BOGEY;
              break;
          }
        }

        StartReaction(0, g_Players[playerIndex].characterIndex, reaction);
        SetCharacterAnimation(0, reaction);
        if (IsGameFlagSet(GAME_FLAG_SHOW_REACTION_FRAME) && f_HasGutsTestRun) {
          SetCharacterAnimation(0, f_GutsTestMotion);
          StartReaction(0, f_GutsTestCharacter, f_GutsTestMotion);
        }

        D_80224000_ovl_00 =
            GetCurrentAnimationEndFrame(0) - GetAnimationFrame(0) + 10;
        if (IsGameFlagSet(GAME_FLAG_UNK32)) {
          D_80224000_ovl_00 = 15;
        }

        PlaceAndViewCharacter(0);
        f_ShouldUpdateReaction = 1;
        if (g_Players[playerIndex].holeStatus == HOLE_STATUS_HOLED_OUT) {
          if (g_Players[playerIndex].strokeCount == 1) {
            CountNotableShot(playerIndex, SHOT_KIND_HOLE_IN_ONE);
          } else {
            switch (g_Players[playerIndex].strokeCount - g_HoleSetup.par) {
              case SCORE_TO_PAR_ALBATROSS:
                CountNotableShot(playerIndex, SHOT_KIND_ALBATROSS);
                break;
              case SCORE_TO_PAR_EAGLE:
                CountNotableShot(playerIndex, SHOT_KIND_EAGLE);
                break;
              case SCORE_TO_PAR_BIRDIE:
                CountNotableShot(playerIndex, SHOT_KIND_BIRDIE);
                break;
            }
          }
        }

        if (g_Players[playerIndex].strokeCount - g_HoleSetup.par <
            SCORE_TO_PAR_PAR) {
          ReportBirdieBadgeCount();
        }

        nextState = PLAYER_STATE_REACTING;
        D_80227170_ovl_00[playerIndex] = 0;
        break;
      }
      case PLAYER_STATE_REACTING:
        PlaceAndViewCharacter(0);
        areCallsAllowed = FALSE;
        f_ShouldUpdateReaction = 1;
        if (D_80227170_ovl_00[playerIndex] == D_80224000_ovl_00 - 10) {
          StartBanner(-1);
        }

        if (g_HoleSetup.conditions.sceneColorIndex ==
            SCENE_COLOR_INDEX_SUDDEN_DEATH) {
          D_80224000_ovl_00 = 160;
        }

        if (D_80227170_ovl_00[playerIndex] >= D_80224000_ovl_00) {
          StopFireworks();
          if (g_GameState.gameMode == GAME_MODE_TRAINING) {
            nextState = PLAYER_STATE_CHOOSING;
            if (g_GameState.courseIndex == DRIVING_RANGE_COURSE_INDEX) {
              OpenChoiceBox(CHOICE_BOX_ID_DRIVING_RANGE);
            } else {
              OpenChoiceBox(CHOICE_BOX_ID_TRAINING_HOLE_END);
            }
          } else if (g_GameState.gameMode == GAME_MODE_RING_SHOT &&
              (!g_TurnBallSimulation.isHoledOut ||
                  !HasNoRingsLeft(&g_Players[playerIndex]))) {
            nextState = PLAYER_STATE_CHOOSING;
            OpenChoiceBox(CHOICE_BOX_ID_RING_SHOT);
          } else {
            nextState = PLAYER_STATE_FINISHING_HOLE;
          }

          D_80227170_ovl_00[playerIndex] = 0;
        }

        break;
      case PLAYER_STATE_CHOOSING:
        if (g_GameState.gameMode == GAME_MODE_TRAINING) {
          if (IsGameFlagSet(GAME_FLAG_SHOW_REACTION_FRAME) &&
              g_Players[playerIndex].holeStatus != HOLE_STATUS_PLAYING) {
            DebugTestGutsMotion();
          }

          switch (UpdateChoiceBox(&g_TurnController)) {
            case CHOICE_RESULT_CONTINUE:
              EndFlightCamera();
              StartHudAnimation(2);
              nextState = PLAYER_STATE_WAITING;
              ForceStopRumble();
              break;
            case CHOICE_RESULT_MULLIGAN:
              g_IsTrainingHeadingKept = TRUE;
              g_Players[playerIndex] = g_SavedPlayers[playerIndex];
              EndFlightCamera();
              StartHudAnimation(2);
              nextState = PLAYER_STATE_WAITING;
              ForceStopRumble();
              break;
            case CHOICE_RESULT_HOLE_OUT:
              if (g_Players[playerIndex].holeStatus == HOLE_STATUS_HOLED_OUT &&
                  IsRecordableHoleOut(playerIndex)) {
                InsertRankedRecord(&D_800FEDD8[playerIndex]);
              }

              nextState = PLAYER_STATE_HOLE_DONE;
              EndFlightCamera();
              FreeCharacterAndReactionLight(0);
              if (IsGameFlagSet(GAME_FLAG_SHOW_REACTION_FRAME)) {
                nextState = PLAYER_STATE_STARTING_REACTION;
              }

              ForceStopRumble();
              break;
            case CHOICE_RESULT_RETURN_TO_MENU:
              func_80060358();
              ForceStopRumble();
              break;
          }
        } else {
          switch (UpdateChoiceBox(&g_TurnController)) {
            case CHOICE_RESULT_START_OVER:
              EndFlightCamera();
              PlaySound(SOUND_RING_SHOT_RETRY, RING_SHOT_RETRY_SOUND_CHANNEL,
                  SOUND_FULL_SCALE);
              SkipNextPinDraw();
              PlacePlayersOnTee();
              nextState = PLAYER_STATE_WAITING;
              ForceStopRumble();
              break;
            case CHOICE_RESULT_RING_SHOT_HOLE_OUT:
              nextState = PLAYER_STATE_HOLE_DONE;
              EndFlightCamera();
              FreeCharacterAndReactionLight(0);
              ForceStopRumble();
              break;
            case CHOICE_RESULT_RETURN_TO_MENU:
              func_80060358();
              ForceStopRumble();
              break;
          }
        }

        break;
      case PLAYER_STATE_ENDING_TURN:
        nextState = PLAYER_STATE_WAITING;
        break;
      case PLAYER_STATE_WAITING:
      case PLAYER_STATE_HOLE_DONE:
        break;
      case PLAYER_STATE_FINISHING_HOLE:
        if (IsRecordableHoleOut(playerIndex) &&
            g_Players[playerIndex].holeStatus == HOLE_STATUS_HOLED_OUT) {
          InsertRankedRecord(&D_800FEDD8[playerIndex]);
        }

        CheckRingShotClear(playerIndex);
        if (D_80227170_ovl_00[playerIndex] == 1) {
          StartBanner(-1);
          g_IsHudTurnAwayRequested = TRUE;
          if (g_GameState.replayKind != REPLAY_KIND_NONE) {
            nextState = PLAYER_STATE_HOLE_DONE;
          } else if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
              g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
            s8 results[MAX_PLAYER_COUNT];
            Boolean isAnyOut = FALSE;
            s32 holeResult = GetGroupHoleResult(results);

            for (i = 0; i < g_GameState.playerCount; i++) {
              if (results[i] == GROUP_PLAYER_OUT &&
                  g_Players[i].holeStatus == HOLE_STATUS_PLAYING) {
                g_Players[i].holeStatus = HOLE_STATUS_NEWLY_OUT;
                isAnyOut = TRUE;
              }
            }

            nextState = (holeResult == GROUP_HOLE_OPEN && isAnyOut)
                ? PLAYER_STATE_FINDING_OUT_PLAYER
                : PLAYER_STATE_HOLE_DONE;
          } else {
            nextState = PLAYER_STATE_HOLE_DONE;
            if (IsOneOnOneMatchMode(g_GameState.gameMode)) {
              s32 result;

              GetHandicapOffset(g_GameState.courseIndex, g_GameState.holeIndex,
                  g_Players[playerIndex].handicap);
              GetHandicapOffset(g_GameState.courseIndex, g_GameState.holeIndex,
                  g_Players[playerIndex ^ 1].handicap);
              result = GetMatchHoleResult(playerIndex, playerIndex ^ 1);
              osSyncPrintf("CheckHoltOutMatch %d\n", result);
              if (result == MATCH_SIDE_PLAYER) {
                g_Players[playerIndex ^ 1]
                    .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                    .strokes =
                    g_Players[playerIndex]
                        .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                        .strokes +
                    1;
                g_Players[playerIndex ^ 1].strokeCount =
                    g_Players[playerIndex ^ 1]
                        .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                        .strokes -
                    GetHandicapOffset(
                        GetCourseRecordIndex(g_GameState.gameMode),
                        g_GameState.holeIndex,
                        g_Players[playerIndex ^ 1].handicap);
                g_Players[playerIndex ^ 1].holeStatus = HOLE_STATUS_OUT;
              } else if (result == MATCH_SIDE_OPPONENT) {
                g_Players[playerIndex ^ 1]
                    .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                    .strokes =
                    g_Players[playerIndex]
                        .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                        .strokes -
                    1;
                g_Players[playerIndex ^ 1].strokeCount =
                    g_Players[playerIndex ^ 1]
                        .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                        .strokes -
                    GetHandicapOffset(
                        GetCourseRecordIndex(g_GameState.gameMode),
                        g_GameState.holeIndex,
                        g_Players[playerIndex ^ 1].handicap);
              } else if (result == MATCH_HOLE_HALVED &&
                  g_Players[playerIndex ^ 1].holeStatus ==
                      HOLE_STATUS_PLAYING) {
                g_Players[playerIndex ^ 1]
                    .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                    .strokes =
                    g_Players[playerIndex]
                        .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                        .strokes;
                g_Players[playerIndex ^ 1].strokeCount =
                    g_Players[playerIndex ^ 1]
                        .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                        .strokes -
                    GetHandicapOffset(
                        GetCourseRecordIndex(g_GameState.gameMode),
                        g_GameState.holeIndex,
                        g_Players[playerIndex ^ 1].handicap);
                g_Players[playerIndex ^ 1].holeStatus = HOLE_STATUS_OUT;
              }
            }
          }
        }

        break;
      case PLAYER_STATE_FINDING_OUT_PLAYER:
        for (i = 0; i < g_GameState.playerCount; i++) {
          if (g_Players[i].holeStatus == HOLE_STATUS_NEWLY_OUT) {
            break;
          }
        }

        if (i == g_GameState.playerCount) {
          nextState = PLAYER_STATE_HOLE_DONE;
          break;
        }

        if (IsCharacterSlotBusy(0)) {
          FreeCharacterAndReactionLight(0);
          break;
        }

        for (i = 0; i < g_GameState.playerCount; i++) {
          if (g_Players[i].holeStatus == HOLE_STATUS_NEWLY_OUT) {
            g_Players[i].holeStatus = HOLE_STATUS_OUT;
            break;
          }
        }

        nextState = PLAYER_STATE_HOLE_DONE;
        if (i != g_GameState.playerCount) {
          ClearReactionSpot();
          LoadCharacter(0, g_Players[i].characterIndex + CHARACTER_COUNT,
              g_Players[i].paletteIndex);
          ConsiderReactionSpot(g_Players[i].ballPosition);
          ConsiderReactionSpot(g_HoleSetup.cupPosition);
          ConsiderReactionSpot(g_HoleSetup.teePosition);
          SetCharacterLighting(0, 1);
          memset(&g_RoundHud, 0, sizeof(g_RoundHud));
          StartReaction(
              0, g_Players[i].characterIndex, REACTION_ANIMATION_BOGEY);
          SetCharacterAnimation(0, REACTION_ANIMATION_BOGEY);
          PlaySong(SONG_HOLE_LOST, 0);
          PlaceAndViewCharacter(0);
          f_ShouldUpdateReaction = 1;
          ShowFlipMessage(0);
          nextState = PLAYER_STATE_SHOWING_OUT_PLAYER;
          D_80227170_ovl_00[playerIndex] = 0;
        }

        break;
      case PLAYER_STATE_SHOWING_OUT_PLAYER:
        PlaceAndViewCharacter(0);
        f_ShouldUpdateReaction = 1;
        areCallsAllowed = FALSE;
        if (D_80227170_ovl_00[playerIndex] == 80) {
          ShowFlipMessage(-1);
        }

        if (D_80227170_ovl_00[playerIndex] == 90) {
          StartBanner(-1);
          nextState = PLAYER_STATE_FINDING_OUT_PLAYER;
          ClearDebugText();
          D_80227170_ovl_00[playerIndex] = 0;
        }

        break;
      default:
        SetDebugTextHighlight(1);
        PrintDebugText("P-S-ERR", 20, 2);
        nextState = PLAYER_STATE_AIMING;
        break;
    }

    g_Players[playerIndex].state = nextState;
    g_BallSimulations[0] = g_TurnBallSimulation;
    g_PreviousBallSurfaceInfo = D_80227150_ovl_00;
  }

  MarkNearbyGroundTiles(
      g_CurrentShot.ballPosition[AXIS_X], g_CurrentShot.ballPosition[AXIS_Z]);
  GetBallSurfaceInfo(
      &g_Players[g_GameState.currentPlayerIndex], &D_80227150_ovl_00);
  if (areCallsAllowed) {
    PlayWaitingPlayersCalls();
  }

  AnimateAllCharacters();
  if (f_ShouldUpdateReaction) {
    UpdateReaction(0);
  }

  if (IsGameFlagSet(GAME_FLAG_FRAME_BALL_AND_SHOT)) {
    FrameBallAndShot();
  }

  if (g_RoundHud.unkF) {
    s32 distance = GetSelectedShotDistance();
    Radians angle = (u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS;
    f32 cosine;
    f32 scale;
    f32 length;

    if (GetSelectedClub()->surfaceMask & CLUB_SURFACE_GREEN) {
      cosine = cosf(angle);
      scale = UNITS_PER_METER;
    } else {
      cosine = cosf(angle);
      scale = UNITS_PER_YARD;
    }

    length = distance * scale;
    g_CurrentShot.unk44[AXIS_X] =
        (cosine * length) + g_CurrentShot.ballPosition[AXIS_X];
    g_CurrentShot.unk44[AXIS_Z] =
        (sinf(angle) * length) + g_CurrentShot.ballPosition[AXIS_Z];
    g_CurrentShot.unk44[AXIS_Y] = GetGroundHeight(
        g_CurrentShot.unk44[AXIS_X], g_CurrentShot.unk44[AXIS_Z]);
  }

  UpdateGolfBalls();
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", IsBButtonHeld);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", GetTurnStickX);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", GetTurnStickY);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", UpdateGolferTranslucency);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", PoseGolferForShot);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", TickStopwatches);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", GiveUpHole);

/* Shows or hides the HUD's parts, apart from the club panel and the letterbox
 */
void SetRoundHudPartsShown(s8 isShown) {
  /* Stands in for the static chain that the original, nested in UpdateTurns,
   * saves */
  volatile s32 unused;
  s32 garbage;

  unused = garbage;
  g_RoundHud.isWindIndicatorShown = isShown;
  g_RoundHud.unk8 = isShown;
  g_RoundHud.isLieIconShown = isShown;
  g_RoundHud.isClubHeadIconShown = isShown;
  g_RoundHud.unkB = isShown;
  g_RoundHud.unkD = isShown;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", DoesImpactRumble);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", PlayImpactClickSound);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", UpdateTurns);
#endif
