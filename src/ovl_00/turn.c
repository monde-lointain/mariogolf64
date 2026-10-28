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
#include "controller.h"
#include "course.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "debug_text.h"
#include "effects.h"
#include "functions.h"
#include "game.h"
#include "game_flags.h"
#include "golf_balls.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "libc.h"
#include "main.h"
#include "math_util.h"
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
#include "scoring.h"
#include "screen.h"
#include "shot_simulation.h"
#include "sound.h"
#include "surface.h"
#include "terrain.h"
#include "variables.h"

/* The loop played while the stick sets the impact point */
#define IMPACT_POINT_SOUND 0x26
#define IMPACT_POINT_SOUND_CHANNEL 8
#define IMPACT_POINT_SOUND_VOLUME 60
#define IMPACT_POINT_SOUND_REVERB 20
#define IMPACT_POINT_SOUND_PAN 230

/* The buttons that hold the impact point mode */
#define IMPACT_POINT_BUTTONS (Z_TRIG | L_TRIG)

#define SHOT_UNK50_SCALE (1.0f / 1536)
#define SHOT_UNK8_MAX_UNK50 600.0f

/* The kinds of notable shot that earn Course Points, as counted in unkAA */
#define SHOT_KIND_COUNT PLAYER_UNKAA_COUNT
#define SHOT_KIND_MAX_COUNT 0xFF
#define MAX_RING_SHOT_CLEAR_COUNT 99

/* The voice that reads out the hole-out banners */
#define HOLE_OUT_CALL_HOLE_IN_ONE 0x84
#define HOLE_OUT_CALL_ALBATROSS 0x85
#define HOLE_OUT_CALL_EAGLE 0x86
#define HOLE_OUT_CALL_BIRDIE 0x87
#define HOLE_OUT_CALL_PAR 0x88
#define HOLE_OUT_CALL_FINISH 0x90
#define HOLE_OUT_CALL_COMPLETE 0x96
#define HOLE_OUT_CALL_BOGEY 0x98
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
#define FREE_LOOK_HUD_FLAGS 0x47
#define FREE_LOOK_FRAMEBUFFER_COUNT 3
/* A shorter pulse than the shots' rumble in effects.c */
#define TURN_RUMBLE_FREQUENCY 0x100
#define TURN_RUMBLE_FRAME_COUNT 8
/* The character sound that each button plays; the D-pad and C buttons give
 * eight calls */
#define CALL_OUT_SOUND_UP 0
#define CALL_OUT_SOUND_DOWN 2
#define CALL_OUT_SOUND_LEFT 4
#define CALL_OUT_SOUND_RIGHT 6
#define CALL_OUT_SOUND_C_UP 8
#define CALL_OUT_SOUND_C_DOWN 10
#define CALL_OUT_SOUND_C_LEFT 12
#define CALL_OUT_SOUND_C_RIGHT 14
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
/* UnkB8.unk26: whether the player is still playing the hole */
#define HOLE_STATUS_HOLED_OUT 1
#define HOLE_STATUS_OUT 3
/* A hole given up outside a match counts as this many times its par */
#define GIVE_UP_PAR_MULTIPLE 3
#define GAME_FLAG_SNAP_REPLAY_AIM 0xF
/* A replay eases the aim by this much plus a 128th of what is left, until this
 * close */
#define REPLAY_AIM_MIN_STEP 0x80
#define REPLAY_AIM_STEP_DIVISOR 128
#define REPLAY_AIM_SETTLE_FRAMES 6
#define REPLAY_FRAMEBUFFER_COUNT 2
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
/* Swing meter phases that BuildShotPreview tells apart */
#define SWING_PHASE_UNK2 2
#define SWING_PHASE_UNK3 3
#define SWING_PHASE_UNK4 4
#define SWING_PHASE_UNK5 5
/* PickStartingClub settles for clubs with at least this lie rating */
#define STARTING_CLUB_MIN_LIE_RATING 2
/* Aims every shot straight down the driving range */
#define GAME_FLAG_AIM_DOWN_RANGE 0x24
/* Only hole-outs in fewer strokes than this enter the records */
#define MAX_RECORDED_STROKE_COUNT 4
/* The stopwatches stop at an hour */
#define STOPWATCH_MAX_FRAMES 108000
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
/* While putting, the crane's steps are this times a count that grows with the
 * putt's length */
#define AIM_CAMERA_PUTT_STEP_UNIT 426.700810546875
/* The crane swings the camera forward only while it is below this height, and
 * back only above the other */
#define AIM_CAMERA_CRANE_FORWARD_HEIGHT (-0x5A00)
#define AIM_CAMERA_CRANE_BACK_HEIGHT (-0x96000)
/* Shots count towards Course Points on their first flights only */
#define MAX_COUNTED_FLIGHT_COUNT 2

extern s32 D_801051F0; /* Set unless two players share a controller */
extern s8 D_800FBDB8; /* PlayContestMovies' state */
extern PlayRecord D_800FEDD8[];
extern RingShotRecord D_80105F40[];
extern CurrentShot D_80104F78; /* The replayed shot */
/* Set once the shot preview has been built */
static s32 f_IsShotPreviewBuilt = FALSE;
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
static u32 D_80223E58_ovl_00 = 10;
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
static f32 f_DebugFramingYaw = 0.0f;
static f32 f_DebugFramingPitch = 0.0f;
/* Read by no routine */
static s32 D_8022401C_ovl_00 = 0;

void SetCharacterTranslucent(u32 slotIndex, s32 value);
void PlaceAimCamera(void);
void PlaceShotCamera(void);
void SetFramebufferCount(s32 count);
s32 func_8005F30C(s32 x, s32 z);
void func_8006034C(s8 value);
s32 func_80060190(s32 strokes);
s32 HasNoRingsLeft(UnkB8* player);
void RecordRingShotClear(s32 playerIndex);

static Mtx D_80227110_ovl_00; /* An identity matrix for the putt preview */
/*
 * The launch of the shot being taken, which the shot preview and the ball
 * simulation start from. unkC is how far from the meter's impact mark the swing
 * was stopped, and clubIndex and shotMode are also the club slot and shot mode
 * that the golfer was last posed with.
 */
static Unk20 D_80227150_ovl_00;
static s32 D_80227170_ovl_00[MAX_PLAYER_COUNT];
static f32 D_80227180_ovl_00;
static f32 D_80227184_ovl_00;
static s32 D_80227188_ovl_00;
static s32 D_8022718C_ovl_00;
static s32 D_80227190_ovl_00;
static s32 D_80227194_ovl_00; /* Set while a recorded shot is replayed */
static s32 f_TurnTimerStart; /* Nothing reads it */
/* The shot preview's matrices, by draw buffer */
static Mtx D_802271A0_ovl_00[2][SHOT_PREVIEW_MAX_STEPS];
/* The shot preview's points */
static f32 D_80227BA0_ovl_00[SHOT_PREVIEW_MAX_STEPS][AXIS_COUNT];

/* Not static: the original left these to the linker's common block */
s32 D_8022B5C0_ovl_00;
CurrentShot g_SavedShots[MAX_PLAYER_COUNT];
UnkB8 g_SavedPlayers[MAX_PLAYER_COUNT];
Unk20 g_PreviousBallSurfaceInfo;

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
  f32 heading;
  f32 offset;
  f32 overshoot;

  if (g_GameState.isMiniGolf) {
    maxX = AIM_CAMERA_MINI_GOLF_MAX_X;
    maxZ = AIM_CAMERA_MINI_GOLF_MAX_Z;
  } else {
    maxX = AIM_CAMERA_COURSE_MAX_X;
    maxZ = AIM_CAMERA_COURSE_MAX_Z;
  }

  heading = (u16)g_CurrentShot.unk0 * BINARY_ANGLE_TO_RADIANS;
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
  f32 heading;

  if (IsGameFlagSet(GAME_FLAG_TEST_AIM_TILT_AND_MOVIES)) {
    if (g_ControllerStates[0].buttons & L_CBUTTONS) {
      f_DebugAimCameraTilt -= 1.0f;
    }

    if (g_ControllerStates[0].buttons & R_CBUTTONS) {
      f_DebugAimCameraTilt += 1.0f;
    }
  }

  if (g_PauseScreen == PAUSE_SCREEN_UNK4) {
    g_Cameras[0].view.mode = CAMERA_MODE_UNK2;
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
  } else if (g_LetterboxState == LETTERBOX_STATE_UNK1 ||
      g_LetterboxState == LETTERBOX_STATE_UNK2) {
    s32 lowestGround;
    s32 ground;
    s32 probe;
    s32 offset;
    s32 rise;
    f32 tilt;

    heading = (u16)g_CurrentShot.unk0 * BINARY_ANGLE_TO_RADIANS;
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
  f32 heading;

  GetSelectedClub();
  if (g_ShotFlags & SHOT_FLAG_ON_GREEN) {
    g_CurrentShot.aimCameraHeight = DOWN_RANGE_GREEN_OFFSET;
    g_CurrentShot.aimCameraDistance = DOWN_RANGE_GREEN_OFFSET;
  } else {
    g_CurrentShot.aimCameraHeight = DOWN_RANGE_HEIGHT;
    g_CurrentShot.aimCameraDistance = DOWN_RANGE_DISTANCE;
  }

  heading = (u16)g_CurrentShot.unk0 * BINARY_ANGLE_TO_RADIANS;
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
  f32 heading = (u16)g_CurrentShot.unk0 * BINARY_ANGLE_TO_RADIANS;

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
  g_LetterboxState = LETTERBOX_STATE_UNK1;
  D_800C3070 = 0;
  g_IsSettingImpactPoint = FALSE;
  g_RoundHud.flags = FREE_LOOK_HUD_FLAGS;
  g_FreeLookHeading = g_CurrentShot.unk0;
  SetMenuBackdrop(MENU_BACKDROP_PATTERN);
  g_PauseScreen = PAUSE_SCREEN_UNK4;
  g_RoundHud.isLetterboxShown = FALSE;
  g_RoundHud.unkC = 0;
  if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
    SetFramebufferCount(FREE_LOOK_FRAMEBUFFER_COUNT);
    SetScreenEffect(SCREEN_EFFECT_NONE, 0);
  }

  StopSound(IMPACT_POINT_SOUND_CHANNEL);
}

void UpdateImpactPointMode(void) {
  if ((g_TurnController.buttons & IMPACT_POINT_BUTTONS) &&
      !(g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT)) {
    if (!g_IsSettingImpactPoint && D_80227194_ovl_00 == 0) {
      PlaySoundWithEffects(IMPACT_POINT_SOUND, IMPACT_POINT_SOUND_CHANNEL,
          IMPACT_POINT_SOUND_VOLUME, IMPACT_POINT_SOUND_REVERB,
          IMPACT_POINT_SOUND_PAN);
    }

    g_IsSettingImpactPoint = TRUE;
  } else {
    StopSound(IMPACT_POINT_SOUND_CHANNEL);
    g_IsSettingImpactPoint = FALSE;
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
    targetDistance = g_CurrentShot.unk50 - AIM_CAMERA_APPROACH_BACKOFF;
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
      stepCount = 10;
    } else if (g_CurrentShot.shotMode == PUTT_MODE_MIDDLE) {
      stepCount = 13;
    } else {
      stepCount = 15;
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
      stepCount = 10;
    } else if (g_CurrentShot.shotMode == PUTT_MODE_MIDDLE) {
      stepCount = 15;
    } else {
      stepCount = 20;
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
  if (ABS(g_TurnController.stickY) > AIM_STICK_DEAD_ZONE) {
    g_CurrentShot.aimCameraHeight -=
        g_TurnController.stickY / AIM_STICK_STEP * AIM_CAMERA_HEIGHT_STEP;
  }
}

/* Moves the aim camera along the aim line with the stick's Y, in smaller steps
 * for a putt */
void MoveAimCameraDistance(void) {
  if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
    if (ABS(g_TurnController.stickY) > AIM_STICK_DEAD_ZONE) {
      g_CurrentShot.aimCameraDistance += g_TurnController.stickY /
          AIM_DISTANCE_STICK_STEP * AIM_CAMERA_PUTT_DISTANCE_STEP;
    }
  } else {
    if (ABS(g_TurnController.stickY) > AIM_STICK_DEAD_ZONE) {
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
  s16 heading = g_CurrentShot.unk0;
  s32 speed;
  s32 step;
  s32 dpad;
  s32 dpadSpeed;

  if (g_IsSettingImpactPoint) {
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

    g_CurrentShot.unk0 -= step;
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
  g_CurrentShot.unk0 +=
      dpad * ((dpadSpeed / AIM_DPAD_SPEED_DIVISOR) + AIM_DPAD_BASE_SPEED);
  if (!IsGameFlagSet(GAME_FLAG_RANDOM_FRAMEBUFFER_COUNT)) {
    if (g_LetterboxState == LETTERBOX_STATE_NONE ||
        g_LetterboxState == LETTERBOX_STATE_UNK5) {
      if (heading == (s16)g_CurrentShot.unk0) {
        if (g_ScreenEffect != SCREEN_EFFECT_SAVED_FRAME) {
          SetScreenEffect(SCREEN_EFFECT_SAVED_FRAME, 2);
          SetFramebufferCount(2);
        }
      } else if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
        SetFramebufferCount(3);
        SetScreenEffect(SCREEN_EFFECT_NONE, 0);
      }
    } else {
      SetFramebufferCount(3);
    }
  }

  if (IsGameFlagSet(GAME_FLAG_RANDOM_FRAMEBUFFER_COUNT)) {
    if (rand() & 1) {
      SetFramebufferCount(3);
    } else {
      SetFramebufferCount(2);
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

  if (D_80227194_ovl_00 != 0) {
    headingLeft = (s16)(D_80104F78.unk0 - g_CurrentShot.unk0);
    if (IsGameFlagSet(GAME_FLAG_SNAP_REPLAY_AIM) ||
        ABS(headingLeft) < REPLAY_AIM_MIN_STEP) {
      g_CurrentShot.unk0 = D_80104F78.unk0;
      if (g_ScreenEffect != SCREEN_EFFECT_SAVED_FRAME) {
        SetScreenEffect(SCREEN_EFFECT_SAVED_FRAME, 2);
        SetFramebufferCount(REPLAY_FRAMEBUFFER_COUNT);
      }

      return ++f_ReplayAimSettledFrameCount >= REPLAY_AIM_SETTLE_FRAMES;
    }

    step = headingLeft / REPLAY_AIM_STEP_DIVISOR;
    turn = ABS(step) + REPLAY_AIM_MIN_STEP;
    heading = g_CurrentShot.unk0;
    g_CurrentShot.unk0 = (headingLeft < 0) ? heading - turn : heading + turn;
    if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
      SetFramebufferCount(FREE_LOOK_FRAMEBUFFER_COUNT);
      SetScreenEffect(SCREEN_EFFECT_NONE, 0);
    }

    return FALSE;
  }

  g_HasPressedShotButton = 0;
  if (g_TurnController.buttons == AIM_RESET_BUTTONS) {
    g_CurrentShot.unk0 = atan2(g_HoleSetup.cupPosition[AXIS_Z] -
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
          g_HasPressedShotButton = 1;
          ResetAimCamera();
        } else if (g_TurnController.pressedButtons & R_TRIG) {
          PlaySound(
              SOUND_LOOK_DOWN_RANGE, g_NextSoundChannel, LOOK_SOUND_VOLUME);
          if (g_ShotFlags & SHOT_FLAG_UNK8) {
            g_LetterboxState = LETTERBOX_STATE_UNK3;
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
            g_LetterboxState = LETTERBOX_STATE_UNK2;
            D_800C3070 = 1;
          }
        } else if (g_TurnController.pressedButtons & B_BUTTON) {
          AimDownRange();
          g_LetterboxState = LETTERBOX_STATE_UNK1;
        }

        if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
          TurnAim(0);
        }

        break;
      case LETTERBOX_STATE_UNK1:
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
      case LETTERBOX_STATE_UNK2:
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
      case LETTERBOX_STATE_UNK3:
      case LETTERBOX_STATE_UNK4:
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
      g_IsSettingImpactPoint = TRUE;
    } else {
      g_IsSettingImpactPoint = FALSE;
    }
  } else {
    if (D_80227194_ovl_00 == 0) {
      if ((g_TurnController.buttons & IMPACT_POINT_BUTTONS) &&
          g_PauseScreen != PAUSE_SCREEN_UNK4 &&
          !(g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT)) {
        if (g_IsSettingImpactPoint != TRUE) {
          PlaySoundWithEffects(IMPACT_POINT_SOUND, IMPACT_POINT_SOUND_CHANNEL,
              IMPACT_POINT_SOUND_VOLUME, IMPACT_POINT_SOUND_REVERB,
              IMPACT_POINT_SOUND_PAN);
        }

        g_IsSettingImpactPoint = TRUE;
        g_ImpactPointLingerFrameCount = 0;
      } else {
        StopSound(IMPACT_POINT_SOUND_CHANNEL);
        if (g_IsSettingImpactPoint != FALSE) {
          g_IsSettingImpactPoint = -1;
        }

        if (g_ImpactPointLingerFrameCount < IMPACT_POINT_LINGER_FRAMES) {
          g_ImpactPointLingerFrameCount++;
        } else {
          g_IsSettingImpactPoint = FALSE;
        }
      }
    }

    switch (g_LetterboxState) {
      case LETTERBOX_STATE_NONE:
      case LETTERBOX_STATE_UNK5:
        if ((g_TurnController.pressedButtons & A_BUTTON) &&
            IsShotModeChangeDone()) {
          g_HasPressedShotButton = 1;
          ResetAimCamera();
        } else if (g_TurnController.pressedButtons & R_TRIG) {
          PlaySound(SOUND_LOOK_AT_CUP, g_NextSoundChannel, LOOK_SOUND_VOLUME);
          if ((g_ShotFlags & (SHOT_FLAG_UNK8 | SHOT_FLAG_NO_IMPACT_POINT)) ==
              (SHOT_FLAG_UNK8 | SHOT_FLAG_NO_IMPACT_POINT)) {
            toCup[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X] -
                g_CurrentShot.ballPosition[AXIS_X];
            toCup[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z] -
                g_CurrentShot.ballPosition[AXIS_Z];
            g_LetterboxState = LETTERBOX_STATE_UNK3;
            g_CurrentShot.aimCameraHeight = LOOK_BACK_START_HEIGHT;
            g_AltHeading = g_CurrentShot.unk0;
            g_CurrentShot.aimCameraDistance =
                -sqrtf((toCup[AXIS_X] * toCup[AXIS_X]) +
                    (toCup[AXIS_Z] * toCup[AXIS_Z])) -
                LOOK_BACK_EXTRA_DISTANCE;
          } else {
            g_LetterboxState = LETTERBOX_STATE_UNK2;
            D_800C3070 = 1;
          }
        }

        if (g_LetterboxState == LETTERBOX_STATE_NONE) {
          TurnAim(0);
        }

        break;
      case LETTERBOX_STATE_UNK1:
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
      case LETTERBOX_STATE_UNK2:
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
      case LETTERBOX_STATE_UNK3:
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
        g_AltHeading = g_CurrentShot.unk0;
        break;
      case LETTERBOX_STATE_UNK4:
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
        g_AltHeading = g_CurrentShot.unk0;
        break;
    }
  }

  if (g_RoundHud.greenGridState >= AIM_VIEW_TOGGLE_COUNT) {
    g_RoundHud.greenGridState = 0;
  }

  if (g_HasPressedShotButton != 1) {
    if (g_TurnController.buttons & AIM_CAMERA_BUTTONS) {
      if (g_LetterboxState == LETTERBOX_STATE_NONE ||
          g_LetterboxState == LETTERBOX_STATE_UNK5 ||
          g_LetterboxState == LETTERBOX_STATE_UNK2) {
        g_LetterboxState = LETTERBOX_STATE_UNK1;
        D_800C3070 = 0;
      }

      if (g_LetterboxState == LETTERBOX_STATE_UNK3) {
        g_LetterboxState = LETTERBOX_STATE_UNK4;
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
      if (g_PauseScreen == PAUSE_SCREEN_UNK4) {
        TurnFreeLook();
      } else {
        CraneAimCamera();
      }

      MoveAimCameraDistanceByUpDown();
    }

    if (g_PauseScreen != PAUSE_SCREEN_UNK4 &&
        (g_TurnController.pressedButtons & IMPACT_POINT_BUTTONS) &&
        !(g_ShotFlags & SHOT_FLAG_ON_GREEN)) {
      SpawnRisingClumps(g_CurrentShot.ballPosition);
    }
  }

  if (g_LetterboxState == LETTERBOX_STATE_UNK3) {
    g_LetterboxState = LETTERBOX_STATE_UNK4;
  }

  if (g_LetterboxState == LETTERBOX_STATE_UNK4) {
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

  if (g_CurrentShot.unk50 * SHOT_UNK50_SCALE <= SHOT_UNK8_MAX_UNK50) {
    g_ShotFlags |= SHOT_FLAG_UNK8;
  }
}

void StartTurnTimer(void) {
  f_TurnTimerStart = osGetCount();
}

/* Always FALSE; maybe once a debug check */
s32 func_8020561C_ovl_00(s32 arg0) {
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
      if (ABS(gap) < bestGap && (!reaches || distance >= cupDistance)) {
        bestSlot = slot;
        bestGap = ABS(gap);
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
  simulation->unkA0 = 0;
  simulation->isRunning = FALSE;
  groundHeight = GetGroundHeight(
      simulation->position[AXIS_X], simulation->position[AXIS_Z]);
  if (groundHeight < simulation->position[AXIS_Y]) {
    simulation->position[AXIS_Y] = groundHeight;
  }

  simulation->surface = player->ballSurface;
  simulation->purpose = LAUNCH_PURPOSE_UNK3;
  simulation->unkA1 = 0;
  PrepareShotEffects(simulation);
  if (g_IsModuleAlive[MODULE_UNK1] || ReportModuleNotAlive(MODULE_UNK1)) {
    ResetBallAfterimage();
  }
}

/* Counts a notable shot towards the player's Course Points */
void CountNotableShot(s32 playerIndex, s32 shotKind) {
  u8* count;

  if ((s8)g_Players[playerIndex].unk1E[0] != 0 || g_GameState.unk31 ||
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

/* The course points that the player has earned with the shots of one kind */
s32 GetShotKindPoints(s32 playerIndex, s32 shotKind) {
  s32 count = (u8)g_Players[playerIndex].unkAA[shotKind];
  s32 points;

  switch (shotKind) {
    case SHOT_KIND_GREAT:
    case SHOT_KIND_EXCELLENT:
      points = 2;
      break;
    case SHOT_KIND_LONG_CHIP_IN:
      points = 8;
      break;
    case SHOT_KIND_PIN_SHOT:
    case SHOT_KIND_UNK5:
      points = 4;
      break;
    case SHOT_KIND_HOLE_IN_ONE:
      if (g_GameState.isMiniGolf) {
        points = 10;
      } else {
        points = 20;
      }

      break;
    case SHOT_KIND_FLAG_SHOT:
    case SHOT_KIND_EAGLE:
    case SHOT_KIND_WATER_CUT:
      points = 10;
      break;
    case SHOT_KIND_ALBATROSS:
      points = 16;
      break;
    case SHOT_KIND_CHIP_IN:
    case SHOT_KIND_BIRDIE:
      points = 6;
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
void BuildShotPreview(Unk20* swing, s32 swingPhase) {
  BallSimulation simulation;
  Unk20 shot;
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
  shot.unk0 = g_CurrentShot.unk0;
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

  if (swingPhase == SWING_PHASE_UNK2 || swingPhase == SWING_PHASE_UNK4 ||
      swingPhase == SWING_PHASE_UNK3) {
    shot.power = MAX_LAUNCH_POWER;
  } else {
    shot.power = swing->power;
  }

  shot.unkC = 0;
  if (swingPhase == SWING_PHASE_UNK2 || swingPhase == SWING_PHASE_UNK4 ||
      swingPhase == SWING_PHASE_UNK5 || swingPhase == SWING_PHASE_UNK3) {
    shot.unk10 = swing->unk10;
    shot.unk14 = swing->unk14;
  } else {
    shot.unk10 = 0;
    shot.unk14 = 0;
  }

  shot.unk18 = g_CurrentShot.playerIndex;
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
      TEXTURE_ADDRESS((u8*)g_ShotPreviewDashImage + IMAGE_HEADER_SIZE),
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
  if (g_PauseScreen == PAUSE_SCREEN_UNK4) {
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
s32 IsRecordableHoleOut(s32 playerIndex) {
  if (g_GameState.unk31) {
    return FALSE;
  }

  if (!IsGameFlagSet(GAME_FLAG_RECORD_ALL_HOLE_OUTS)) {
    if ((s8)g_Players[playerIndex].unk1E[0] != 0) {
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

s32 IsRingShotRound(void) {
  if (!g_GameState.unk31) {
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
    record->scoreKind = func_80060190(g_Players[playerIndex].strokeCount);
    record->characterIndex = g_Players[playerIndex].characterIndex;
    record->unk3C = g_Players[playerIndex].unk22;
    record->paletteIndex = g_Players[playerIndex].paletteIndex;
    record->strokeCount = g_Players[playerIndex].strokeCount;
    record->unk40 = g_Players[playerIndex].unk88;
    record->hole = g_HoleSetup;
    record->shots[shotIndex].heading = g_CurrentShot.unk0;
    record->shots[shotIndex].unk2 = g_CurrentShot.unk4;
    record->shots[shotIndex].unk3 = g_CurrentShot.unk8;
    record->shots[shotIndex].shotMode = g_CurrentShot.shotMode;
    record->shots[shotIndex].clubSlot = g_CurrentShot.clubSlot;
    record->shots[shotIndex].unk6 = g_CurrentShot.unk14;
    record->shots[shotIndex].unk7 = g_CurrentShot.unk18;
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
    record->scoreKind = func_80060190(g_Players[playerIndex].strokeCount);
    record->characterIndex = g_Players[playerIndex].characterIndex;
    record->unk38 = g_Players[playerIndex].unk22;
    record->paletteIndex = g_Players[playerIndex].paletteIndex;
    record->strokeCount = g_Players[playerIndex].strokeCount;
    record->unk3C = g_Players[playerIndex].unk88;
    record->hole = g_HoleSetup;
    record->shots[shotIndex].heading = g_CurrentShot.unk0;
    record->shots[shotIndex].unk2 = g_CurrentShot.unk4;
    record->shots[shotIndex].unk3 = g_CurrentShot.unk8;
    record->shots[shotIndex].shotMode = g_CurrentShot.shotMode;
    record->shots[shotIndex].clubSlot = g_CurrentShot.clubSlot;
    record->shots[shotIndex].unk6 = g_CurrentShot.unk14;
    record->shots[shotIndex].unk7 = g_CurrentShot.unk18;
  }
}

s32 HasNoRingsLeft(UnkB8* player) {
  s32 i;

  for (i = 0; i < RING_COUNT; i++) {
    if (player->ringStates[i] != RING_STATE_NONE) {
      return FALSE;
    }
  }

  return TRUE;
}

s32 AreAllRingsPassed(UnkB8* player) {
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
      (s8)g_Players[playerIndex].unk1E[0] == 0 &&
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
  g_CurrentShot.unk18 = 0;
  g_CurrentShot.unk14 = 0;
  g_CurrentShot.unk54 = player->unk28;
  g_CurrentShot.unk40 = 0;
  g_CurrentShot.unk50 = GetCupDistance(
      x - g_HoleSetup.cupPosition[AXIS_X], z - g_HoleSetup.cupPosition[AXIS_Z]);
  if (!g_IsTrainingHeadingKept || g_GameState.gameMode != GAME_MODE_TRAINING) {
    s32 surfaceKind = GetSurfaceInfo(g_CurrentShot.surface)->kind;

    if (g_HolePoints.unk60[0][AXIS_X] == 0 ||
        g_HolePoints.unk60[0][AXIS_Z] == 0) {
      g_HolePoints.unk60[0][AXIS_X] = g_HoleSetup.cupPosition[AXIS_X];
      g_HolePoints.unk60[0][AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y];
      g_HolePoints.unk60[0][AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z];
    }

    if (g_HoleSetup.par != 3 &&
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

    g_CurrentShot.unk0 = atan2(target[AXIS_Z] - player->ballPosition[AXIS_Z],
                             target[AXIS_X] - player->ballPosition[AXIS_X]) *
        RADIANS_TO_BINARY_ANGLE;
    PickStartingClub(&g_HoleSetup);
  }

  g_IsTrainingHeadingKept = FALSE;
  if (IsGameFlagSet(GAME_FLAG_AIM_DOWN_RANGE)) {
    AimDownRange();
    g_LetterboxState = LETTERBOX_STATE_UNK1;
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
    if (g_Players[finishedCount].unkC != PLAYER_STATE_UNK17 &&
        g_Players[finishedCount].unkC != PLAYER_STATE_UNK20) {
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
    if (g_Players[i].unk26 != 0) {
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
  g_Players[nextPlayer].unkC = 0;
  if (g_IsHudTurnAwayRequested) {
    StartHudAnimation(HUD_ANIMATION_TURN_AWAY);
    g_IsHudTurnAwayRequested = 0;
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
    if (g_SimulatedShot.unk2E10) {
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
    if ((u32)controllerIndex < MAXCONTROLLERS && !g_GameState.unk31) {
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
      if ((g_Players[i].unkC == PLAYER_STATE_UNK17) |
          (g_Players[i].unkC == PLAYER_STATE_UNK20)) {
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
              CALL_OUT_SOUND_UP, SOUND_FULL_SCALE, g_Players[i].soundPan);
        } else if (pressed & D_JPAD) {
          PlayCharacterSound((s8)g_Players[i].characterIndex, i,
              CALL_OUT_SOUND_DOWN, SOUND_FULL_SCALE, g_Players[i].soundPan);
        } else if (pressed & L_JPAD) {
          PlayCharacterSound((s8)g_Players[i].characterIndex, i,
              CALL_OUT_SOUND_LEFT, SOUND_FULL_SCALE, g_Players[i].soundPan);
        } else if (pressed & R_JPAD) {
          PlayCharacterSound((s8)g_Players[i].characterIndex, i,
              CALL_OUT_SOUND_RIGHT, SOUND_FULL_SCALE, g_Players[i].soundPan);
        } else if (pressed & U_CBUTTONS) {
          PlayCharacterSound((s8)g_Players[i].characterIndex, i,
              CALL_OUT_SOUND_C_UP, SOUND_FULL_SCALE, g_Players[i].soundPan);
        } else if (pressed & D_CBUTTONS) {
          PlayCharacterSound((s8)g_Players[i].characterIndex, i,
              CALL_OUT_SOUND_C_DOWN, SOUND_FULL_SCALE, g_Players[i].soundPan);
        } else if (pressed & L_CBUTTONS) {
          PlayCharacterSound((s8)g_Players[i].characterIndex, i,
              CALL_OUT_SOUND_C_LEFT, SOUND_FULL_SCALE, g_Players[i].soundPan);
        } else if (pressed & R_CBUTTONS) {
          PlayCharacterSound((s8)g_Players[i].characterIndex, i,
              CALL_OUT_SOUND_C_RIGHT, SOUND_FULL_SCALE, g_Players[i].soundPan);
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
      case -1:
        PlaySong(SONG_HOLE_OUT_BIRDIE, 0);
        break;
      case -2:
        PlaySong(SONG_HOLE_OUT_EAGLE, 0);
        break;
      case -3:
        PlaySong(SONG_HOLE_OUT_EAGLE, 0);
        break;
      case 0:
        PlaySong(SONG_HOLE_OUT_PAR, 0);
        break;
      case 1:
        PlaySong(SONG_HOLE_OUT_BOGEY, 0);
        break;
      case 2:
      case 3:
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
    PlaySound(HOLE_OUT_CALL_FINISH, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
  } else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
    if (AreAllRingsPassed(&g_Players[playerIndex])) {
      StartBanner(BANNER_COMPLETE);
      PlaySound(
          HOLE_OUT_CALL_COMPLETE, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
    }
  } else if (strokeCount == 1) {
    banner = BANNER_HOLE_IN_ONE;
    PlaySound(
        HOLE_OUT_CALL_HOLE_IN_ONE, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
  } else {
    switch (scoreToPar) {
      case -1:
        banner = BANNER_BIRDIE;
        PlaySound(
            HOLE_OUT_CALL_BIRDIE, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
        break;
      case -2:
        banner = BANNER_EAGLE;
        PlaySound(HOLE_OUT_CALL_EAGLE, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
        break;
      case -3:
        banner = BANNER_ALBATROSS;
        PlaySound(
            HOLE_OUT_CALL_ALBATROSS, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
        break;
      case 0:
        banner = BANNER_PAR;
        PlaySound(HOLE_OUT_CALL_PAR, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
        break;
      case 1:
        banner = BANNER_BOGEY;
        PlaySound(HOLE_OUT_CALL_BOGEY, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
        break;
      case 2:
        banner = BANNER_STROKES_OVER_PAR + 2;
        break;
      case 3:
        banner = BANNER_STROKES_OVER_PAR + 3;
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
extern u16 D_80104998;
extern u16 D_80104994;
extern u8 D_800FE330;
extern s32 g_IsSettingImpactPoint;

/* Debug flag 0x47: prints where the ball lies, and the second controller moves
 * it while Z is held */
static inline void DebugPlaceBall(s32 playerIndex) {
  char text[128];

  D_80227188_ovl_00 = g_GameState.courseIndex;
  D_8022718C_ovl_00 = g_GameState.holeIndex;
  SetGameFlag(0x9F);
  sprintf(text, "X:%X Y:%X Z:%X  Attr%d             ",
      g_CurrentShot.ballPosition[0], g_CurrentShot.ballPosition[1],
      g_CurrentShot.ballPosition[2],
      GetSurface(g_CurrentShot.ballPosition[0], g_CurrentShot.ballPosition[2]));
  PrintDebugText(text, 3, 6);
  ClearGameFlag(0x9F);
  if (g_ControllerData[1].button & Z_TRIG) {
    g_CurrentShot.ballPosition[0] += g_ControllerStates[1].stickX *
        ((ABS(g_ControllerStates[1].stickX) * 2) + 1);
    g_CurrentShot.ballPosition[2] += g_ControllerStates[1].stickY *
        ((ABS(g_ControllerStates[1].stickY) * 2) + 1);
    g_CurrentShot.ballPosition[1] = GetGroundHeight(
        g_CurrentShot.ballPosition[0], g_CurrentShot.ballPosition[2]);
    g_Players[playerIndex].ballPosition[0] = g_CurrentShot.ballPosition[0];
    g_Players[playerIndex].ballPosition[1] = g_CurrentShot.ballPosition[1];
    g_Players[playerIndex].ballPosition[2] = g_CurrentShot.ballPosition[2];
    PlaceBallAtRest(playerIndex, &g_TurnBallSimulation);
  }
}

/* Debug flag 0x3E: the D-pad picks a character and a reaction to load in its
 * place */
static inline void DebugTestGutsMotion(void) {
  char* motionName;
  char text[128];
  switch (f_GutsTestMotion) {
    case 12:
      motionName = "EAGLE ";
      break;
    case 13:
      motionName = "BIRDIE";
      break;
    case 14:
      motionName = "PAR   ";
      break;
    case 15:
      motionName = "BOGEY ";
      break;
    default:
      motionName = "ERROR  ";
      break;
  }

  f_HasGutsTestRun = 1;
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
  center[0] = g_BallPosition[0] * COURSE_TO_WORLD_SCALE;
  center[1] = g_BallPosition[1] * COURSE_TO_WORLD_SCALE;
  center[2] = g_BallPosition[2] * COURSE_TO_WORLD_SCALE;
  AddFramedSphere(center, 7.5f);
  center[0] = g_CurrentShot.ballPosition[0] * COURSE_TO_WORLD_SCALE;
  center[1] = g_CurrentShot.ballPosition[1] * COURSE_TO_WORLD_SCALE;
  center[2] = g_CurrentShot.ballPosition[2] * COURSE_TO_WORLD_SCALE;
  AddFramedSphere(center, 7.5f);
  FrameSpheres(0);
  GetFramedView(g_Cameras[0].view.eye, g_Cameras[0].view.lookAt);
}

void UpdateTurns(void) {
  s32 playerIndex;
  s32 state;
  s32 nextState;
  s32 areCallsAllowed;
  s32 i;

  /* Returns whether the B button is held */
  s32 IsBButtonHeld(void) {
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
        g_LetterboxState != LETTERBOX_STATE_UNK4) {
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

    if (!g_GameState.unk31 &&
        (g_GameState.gameMode == GAME_MODE_MATCH_GAME ||
            g_GameState.gameMode == GAME_MODE_GET_CHARACTER) &&
        g_Players[playerIndex ^ 1].unk26 == HOLE_STATUS_HOLED_OUT) {
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
      g_Players[playerIndex].unk26 = HOLE_STATUS_OUT;
    } else {
      g_Players[playerIndex].strokeCount =
          g_HoleSetup.par * GIVE_UP_PAR_MULTIPLE;
      g_Players[playerIndex]
          .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
          .strokes = g_HoleSetup.par * GIVE_UP_PAR_MULTIPLE;
      g_Players[playerIndex].unk26 = HOLE_STATUS_OUT;
    }

    SkipUnmeasuredDistances(playerIndex);
    UpdateContest(FALSE);
    if (!g_GameState.unk31) {
      opponentIndex = playerIndex ^ 1;
      if (g_GameState.gameMode == GAME_MODE_MATCH_GAME ||
          g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
        g_Players[opponentIndex].unk26 = HOLE_STATUS_HOLED_OUT;
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
  s32 DoesImpactRumble(void) {
    return GetUnkCEntry(GetSelectedClub(), g_CurrentShot.surface,
        g_CurrentShot.shotMode, D_80227150_ovl_00.unkC)
        ->unk5;
  }

  /* Plays the impact click: a mishit, a stop right on the mark, or one off it
   */
  void PlayImpactClickSound(void) {
    if (DoesImpactRumble()) {
      PlaySound(SOUND_MENU_BLOCKED, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
    } else if (D_80227150_ovl_00.unkC == 0) {
      PlaySound(SOUND_IMPACT_ON_MARK, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
    } else {
      PlaySound(SOUND_IMPACT_OFF_MARK, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
    }
  }

  areCallsAllowed = TRUE;
  if (IsGameFlagSet(0x68)) {
    PrintCoursePoints();
  }

  f_IsShotPreviewBuilt = 0;
  if (!(g_LetterboxState == 0 || g_LetterboxState == 5)) {
    SetScreenEffect(4, 0);
  }

  if (IsGameFlagSet(0x2C)) {
    for (i = 0; i < g_GameState.playerCount; i++) {
      PrintDebugPaddedHex(g_Players[i].unkC, 2, i + 10);
    }
  }

  PlayAmbientSound();
  PickNextPlayer();
  for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
    s32 controllerIndex = g_Players[playerIndex].controllerIndex;

    state = g_Players[playerIndex].unkC;
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
    D_80227150_ovl_00.unk18 = playerIndex;
    if ((state == 2 || state == 4 || state == 5 || state == 6 || state == 8 ||
            state == 9) &&
        (g_PauseScreen == 0 || g_PauseScreen == 4)) {
      TickStopwatches();
    }

    if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
      g_RoundHud.stopwatchFrameCount =
          g_Players[playerIndex].stopwatchFrameCount;
    }

    switch (state) {
      case 0:
        f_ShouldUpdateReaction = 0;
        UnloadOverlay(GetOverlayOfModule(8));
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
        ClearGameFlag(0xF);
        StartPlayerShot(playerIndex);
        D_80227150_ovl_00.unk0 = g_CurrentShot.unk0;
        g_RoundHud.clubHeadTilt =
            GetAimSlopeAngle(&g_Players[playerIndex], &D_80227150_ovl_00);
        g_RoundHud.lieTilt =
            GetCrossSlopeAngle(&g_Players[playerIndex], &D_80227150_ovl_00);
        UpdateShotFlags();
        PlaceBallAtRest(playerIndex, &g_TurnBallSimulation);
        g_CurrentShot.unk50 =
            GetCupDistance(g_BallPosition[0] - g_HoleSetup.cupPosition[0],
                g_BallPosition[2] - g_HoleSetup.cupPosition[AXIS_Z]);
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
        if (g_GameState.unk31) {
          if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
            func_80060128(g_Players[playerIndex].strokeCount);
          } else {
            func_800600C0(g_Players[playerIndex].strokeCount);
          }

          D_80227194_ovl_00 = 1;
        } else if (IsGameFlagSet(0x32) || g_Players[playerIndex].unk1E[0] ||
            IsGameFlagSet(0x9B)) {
          g_Players[playerIndex].unk1E[1] = -9;
          LoadOverlay(GetOverlayOfModule(7));
          ListOffsetsByDistance();
          D_800B7D9C = 0;
          D_80223E58_ovl_00 = 30;
          D_80227194_ovl_00 = 2;
        } else if (f_ShouldReplaySavedShot) {
          D_80227194_ovl_00 = 1;
          D_80104F78 = g_SavedShots[playerIndex];
          f_ShouldReplaySavedShot = 0;
        } else {
          D_80227194_ovl_00 = 0;
        }

        D_8022B5C0_ovl_00 = 0;
        f_ReplayAimSettledFrameCount = 0;
        if (!g_GameState.unk31 &&
            (g_GameState.gameMode == GAME_MODE_MATCH_GAME ||
                g_GameState.gameMode == GAME_MODE_GET_CHARACTER)) {
          s32 scoreToPar = GetNetScoreToPar(g_GameState.holeIndex, playerIndex);

          g_RoundHud.skinsTotals[playerIndex] = scoreToPar -
              GetNetScoreToPar(g_GameState.holeIndex, playerIndex ^ 1);
        } else {
          g_RoundHud.skinsTotals[playerIndex] =
              GetNetScoreToPar(g_GameState.holeIndex, playerIndex);
        }

        switch (GetHolingOutStakes(playerIndex)) {
          case 3:
            break;
          case 1:
            StartMovie(10);
            break;
          case 4:
            StartMovie(11);
            break;
          case 0:
          case 2:
            if (GetSurfaceInfo(g_CurrentShot.surface)->kind == 0xB) {
              StartMovie(9);
            }

            break;
        }
        {
          s32 song = 0x1F;

          if (g_GameState.unk31) {
            PlaySong(song, 0x11);
          } else if (GetSurfaceInfo(g_CurrentShot.surface)->kind == 0xB) {
            song = 0x2C;
            if (g_GameState.isMiniGolf) {
              PlaySong(song, 0x11);
            } else if (g_GameState.gameMode != GAME_MODE_SPEED_GOLF &&
                g_GameState.gameMode != GAME_MODE_RING_SHOT) {
              s32 scoreToPar =
                  g_Players[playerIndex].strokeCount - (g_HoleSetup.par - 1);

              if (scoreToPar > 0) {
                PlaySong(0xF, 0x11);
                osSyncPrintf("BOGEY\n");
              } else if (scoreToPar < -2) {
                PlaySong(0x10, 0x11);
                osSyncPrintf("EAGLE\n");
                if (GetHolingOutStakes(playerIndex) == 3 &&
                    !g_GameState.isMiniGolf) {
                  StartMovie(6);
                }
              } else if (scoreToPar < -1) {
                PlaySong(0x10, 0x11);
                osSyncPrintf("EAGLE\n");
                if (GetHolingOutStakes(playerIndex) == 3 &&
                    !g_GameState.isMiniGolf) {
                  StartMovie(7);
                }
              } else if (scoreToPar == 0) {
                PlaySong(0xE, 0x11);
                osSyncPrintf("PAR\n");
              } else {
                PlaySong(0x10, 0x11);
                osSyncPrintf("BIRDIE\n");
                if (GetHolingOutStakes(playerIndex) == 3 &&
                    !g_GameState.isMiniGolf) {
                  StartMovie(8);
                }
              }
            }
          } else {
            song = 0x2C;
            if (!g_GameState.isMiniGolf) {
              if (g_GameState.gameMode == GAME_MODE_TOURNAMENT) {
                switch (g_GameState.courseIndex) {
                  case 0:
                    song = 0x20;
                    break;
                  case 1:
                    song = 0x21;
                    break;
                  case 2:
                    song = 0x22;
                    break;
                  case 3:
                    song = 0x30;
                    break;
                  case 4:
                    song = 0x23;
                    break;
                  case 5:
                  default:
                    song = 0x24;
                    break;
                }
              } else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
                song = 0x15;
              } else if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
                song = 0x26;
              } else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
                song = 0x1E;
              } else if (GetLastHoleIndex() == g_GameState.holeIndex &&
                  g_GameState.playerCount != 1) {
                song = 0x14;
              } else if ((g_GameState.gameMode == GAME_MODE_MATCH_GAME ||
                             g_GameState.gameMode == GAME_MODE_GET_CHARACTER) &&
                  GetMatchStanding(0, 1) != 0) {
                song = 0x14;
              } else if (g_GameState.playerCount == 4 &&
                  (g_Players[0].characterIndex == 10 ||
                      g_Players[0].characterIndex == 11) &&
                  (g_Players[1].characterIndex == 10 ||
                      g_Players[1].characterIndex == 11) &&
                  (g_Players[2].characterIndex == 10 ||
                      g_Players[2].characterIndex == 11) &&
                  (g_Players[3].characterIndex == 10 ||
                      g_Players[3].characterIndex == 11)) {
                song = 0x28;
              } else {
                switch (g_GameState.courseIndex) {
                  case 0:
                    song = 0xC;
                    break;
                  case 1:
                    song = 0xD;
                    break;
                  case 2:
                    song = 0x11;
                    break;
                  case 3:
                    song = 0x13;
                    break;
                  case 4:
                    song = 0x12;
                    break;
                  case 5:
                  default:
                    song = 0x29;
                    break;
                }
              }
            }

            PlaySong(song, 0x11);
          }
        }

        StartShotPanel();
        nextState = 0x15;
        if (!D_800C3094) {
          nextState = 2;
          if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
            nextState = 1;
            StartHoleIntroCamera();
            func_8005F1C8();
          }
        }

        StartTurnRumble(g_GameState.currentPlayerIndex);
        break;
      case 0x15:
        D_80227150_ovl_00.clubIndex = g_CurrentShot.clubSlot;
        D_80227150_ovl_00.shotMode = g_CurrentShot.shotMode;
        D_80227150_ovl_00.unk0 = g_CurrentShot.unk0;
        if (D_80227194_ovl_00 == 2) {
          StartTurnTimer();
          for (i = 0; D_80227194_ovl_00 == 2 && i < 100; i++) {
            D_80227194_ovl_00 = (GetPlannedShot(&D_80104F78) != -1) ? 1 : 2;
            if (!func_8020561C_ovl_00(10)) {
              break;
            }
          }
        }

        nextState = 0x15;
        if (IsHudAnimationDone()) {
          nextState = 2;
        }

        break;
      case 1:
        if (g_GameState.holeIndex == GetFirstHoleIndex()) {
          UpdateHoleIntroCamera();
        }

        if (UpdateHoleIntroCamera()) {
          nextState = 2;
        }

        TurnAim(0);
        PlaceCharacterForShot(0);
        if (g_ScreenEffect == 3) {
          SetFramebufferCount(3);
          SetScreenEffect(4, 0);
        }

        break;
      case 2:
        D_80223FF8_ovl_00 = 0;
        UpdateShotFlags();
        D_8022B5C0_ovl_00++;
        if (g_PauseScreen == 0) {
          if (D_80227194_ovl_00 == 0 &&
              g_TurnController.pressedButtons == START_BUTTON &&
              (g_TurnController.buttons & START_BUTTON)) {
            ForceStopRumble();
            StopSound(8);
            PlaySound(0x66, 2, SOUND_FULL_SCALE);
            g_PauseScreen = 1;
            SetMenuBackdrop(2);
            RequestPauseMenu(g_Players[playerIndex].strokeCount != 0, 0);
            HideRoundHud();
          }
        } else if (g_PauseScreen == 1 && D_80227194_ovl_00 == 0 &&
            (g_TurnController.pressedButtons & (START_BUTTON | B_BUTTON))) {
          ForceStopRumble();
          PlaySound(0x66, 2, SOUND_FULL_SCALE);
          RequestPauseMenu(0, -1);
          g_PauseScreen = 0;
          SetMenuBackdrop(-1);
          ResumeRound();
          break;
        }

        if (g_PauseScreen == 1) {
          s32 choice = UpdatePauseMenu(&g_TurnController);

          if (choice == PAUSE_ACTION_RETURN_TO_MENU) {
            RequestPauseMenu(0, -1);
            func_80060358();
          } else if (choice == PAUSE_ACTION_START_OVER) {
            RequestPauseMenu(0, -1);
            ResumeRound();
            TakePinOut();
            ClearShotEffects(1);
            PlacePlayersOnTee();
            SetFramebufferCount(3);
            nextState = 0x11;
          } else if (choice == PAUSE_ACTION_HOLE_OUT) {
            RequestPauseMenu(0, -1);
            nextState = 0x14;
            EndFlightCamera();
            ClearShotEffects(1);
            FreeCharacterAndReactionLight(0);
          }
        } else if (g_PauseScreen == 7) {
          if (g_IsModuleAlive[5]) {
            g_PauseScreen = 2;
            OpenScorecard(g_GameState.courseIndex, g_GameState.holeIndex);
            if (g_GameState.gameMode == GAME_MODE_TOURNAMENT) {
              SetScorecardLayout(3);
            } else if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
              SetScorecardLayout(4);
            } else if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
              SetScorecardLayout(2);
            } else if (g_GameState.gameMode == GAME_MODE_MATCH_GAME ||
                g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
              SetScorecardLayout(1);
            } else {
              SetScorecardLayout(0);
            }
          }
        } else if (g_PauseScreen == 2) {
          UpdateScorecard(D_80104998, D_80104994);
        } else if (g_PauseScreen == 8) {
          if (g_IsModuleAlive[0x16]) {
            g_PauseScreen = 5;
            OpenSaveMenu(1);
          }
        } else if (g_PauseScreen == 9) {
          if (g_IsModuleAlive[0x16]) {
            g_PauseScreen = 10;
            OpenControlsScreen();
          }
        } else if (g_PauseScreen == 10) {
          UpdateControlsScreen(&g_TurnController);
        } else if (g_PauseScreen == 5) {
          UpdateGameSaveMenu(&g_TurnController);
        } else if (g_PauseScreen == 3) {
        } else if (g_PauseScreen == 6) {
          switch (UpdatePrompt(&g_TurnController)) {
            case 6:
              D_800C3094 = 1;
              SetMenuBackdrop(-1);
              ResumeRound();
              nextState = 0xB;
              GiveUpHole(playerIndex);
              break;
            case 5:
              g_PauseScreen = 0;
              SetMenuBackdrop(-1);
              ResumeRound();
              break;
            case 1:
              SaveReplayToSlot();
              CloseSaveMenu();
              g_PauseScreen = 0;
              func_80060358();
              break;
            case 0:
              SaveGameToSlot();
              g_PauseScreen = 5;
              break;
            case 3:
              LeaveGameSaveMenu();
            case 2:
              g_PauseScreen = 5;
              break;
            case 4:
              g_PauseScreen = 0;
              ResumeRound();
              func_80060358();
              break;
          }
        } else if (g_PauseScreen == 4) {
          if (g_TurnController.pressedButtons & (A_BUTTON | R_TRIG)) {
            g_PauseScreen = 0;
            SetMenuBackdrop(-1);
            SetFramebufferCount(3);
            SetScreenEffect(4, 0);
            g_LetterboxState = 0;
            PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
            ResetAimCamera();
            func_8005F1C8();
            g_RoundHud.unkC = 1;
            g_RoundHud.isLetterboxShown = 1;
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
            SetFramebufferCount(3);
            SetScreenEffect(4, 0);
          }

          D_80227150_ovl_00.unk10 = 0;
          D_80227150_ovl_00.unk14 = 0;
          D_80227150_ovl_00.unk0 = g_CurrentShot.unk0;
          BuildShotPreview(&D_80227150_ovl_00, state);
        } else if (g_PauseScreen == 0) {
          s32 isReady;
          s32 tempo;
          s32 replayKind;

          if (D_80227194_ovl_00 == 0 &&
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
          if (GetSurfaceInfo(g_CurrentShot.surface)->kind == 0xB &&
              !g_GameState.isMiniGolf) {
            D_80227184_ovl_00 += 0.04f;
            if (D_80227184_ovl_00 > 155.0f) {
              D_80227184_ovl_00 = 155.0f;
            }

            if (IsGameFlagSet(0x2A)) {
              sprintf(g_PrintBuffer, "tempo:%5.2f", D_80227184_ovl_00);
              PrintDebugText(g_PrintBuffer, 10, 10);
            }

            tempo = D_80227184_ovl_00;
          }

          SetSongTempo(tempo);
          replayKind = D_80227194_ovl_00;
          if (replayKind == 2) {
            if (g_ScreenEffect != 3) {
              SetScreenEffect(3, 2);
              SetFramebufferCount(2);
            }

            StartTurnTimer();
            for (i = 0; D_80227194_ovl_00 == 2 && i < 400; i++) {
              D_80227194_ovl_00 = (GetPlannedShot(&D_80104F78) != -1) ? 1 : 2;
              if (!func_8020561C_ovl_00(D_80223E58_ovl_00)) {
                break;
              }
            }

            D_80223E58_ovl_00 *= 1.05f;
            if (D_80223E58_ovl_00 > 0x800) {
              D_80223E58_ovl_00 = 0x800;
            }

            if (D_80227194_ovl_00 == 2) {
              break;
            }
          }

          if (replayKind != 0 && D_8022B5C0_ovl_00 < 30) {
            break;
          }

          UnloadOverlay(GetOverlayOfModule(7));
          if (D_80227194_ovl_00 == 1) {
            isReady = StepToClubChoice(&D_80104F78);
          } else {
            if (!g_IsSettingImpactPoint) {
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
          if (IsGameFlagSet(0x47)) {
            DebugPlaceBall(playerIndex);
          }

          if (isReady) {
            isReady = UpdateAimControls() && isReady;
          }

          D_80227150_ovl_00.unk0 = g_CurrentShot.unk0;
          PlaceCharacterForShot(0);
          UpdateGolferTranslucency();
          if (!(g_LetterboxState == 0 || g_LetterboxState == 5) &&
              g_ScreenEffect == 3) {
            SetScreenEffect(4, 0);
          }

          if (g_LetterboxState == 0 || g_LetterboxState == 5) {
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

          if ((D_80227194_ovl_00 != 0 && isReady) ||
              (D_80227194_ovl_00 == 0 && g_HasPressedShotButton == 1)) {
            g_ShowsGroundWaves = 0;
            if (g_CurrentShot.shotMode != SHOT_MODE_APPROACH) {
              SkipToAnimationEnd(0);
            }

            g_CurrentShot.unk40 = 0;
            g_CurrentShot.unk4 = 0;
            g_CurrentShot.unk8 = 0;
            PlaySound(
                SOUND_IMPACT_OFF_MARK, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
            SetFramebufferCount(2);
            nextState = 4;
            if (g_ScreenEffect != 3) {
              SetScreenEffect(3, 2);
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

          if (g_IsSettingImpactPoint == 1) {
            if (D_80227194_ovl_00 == 0) {
              g_CurrentShot.unk14 = GetTurnStickX();
              g_CurrentShot.unk18 = GetTurnStickY();
            } else {
              g_CurrentShot.unk14 = 0;
              g_CurrentShot.unk18 = 0;
            }
          } else if (g_IsSettingImpactPoint == 0) {
            g_CurrentShot.unk14 = 0;
            g_CurrentShot.unk18 = 0;
          }

          g_CurrentShot.unk40 = 0;
          if (IsGameFlagSet(0x32)) {
            g_CurrentShot.unk40 = D_80104F78.unk4;
          }

          SetShotOffsetMarker(g_CurrentShot.unk14, g_CurrentShot.unk18);
          D_80227150_ovl_00.unk10 = g_CurrentShot.unk14;
          D_80227150_ovl_00.unk14 = g_CurrentShot.unk18;
          PlaceShotCamera();
          if (g_ShotFlags & 1) {
            if (g_LetterboxState == 5 || g_LetterboxState == 0) {
              g_RoundHud.flags = 5;
            } else {
              g_RoundHud.flags = 0x45;
            }
          } else if (g_ShotFlags & 2) {
            if (g_LetterboxState == 5 || g_LetterboxState == 0) {
              g_RoundHud.flags = 7;
            } else {
              g_RoundHud.flags = 0x47;
            }
          } else {
            if (g_LetterboxState == 5) {
              g_RoundHud.flags = 9;
            } else if (g_LetterboxState == 0) {
              g_RoundHud.flags = 3;
            } else {
              g_RoundHud.flags = 0x47;
            }

            if (g_IsSettingImpactPoint == 1) {
              g_RoundHud.flags = (g_RoundHud.flags | 8) & ~2;
            }
          }

          BuildShotPreview(&D_80227150_ovl_00, state);
        }

        break;
      case 4:
        UpdateImpactPointMode();
        ReuseLastCharacterMatrices(0);
        BuildShotPreview(&D_80227150_ovl_00, state);
        areCallsAllowed = FALSE;
        if (g_RoundHud.greenGridState == 1) {
          g_RoundHud.greenGridState = 2;
        }

        g_RoundHud.unkF = 0;
        g_RoundHud.unk12 = 0;
        if (g_ShotFlags & 2) {
          g_RoundHud.flags = 0x10;
        } else {
          g_RoundHud.flags = 0x18;
        }

        if (D_80227194_ovl_00 == 0) {
          g_CurrentShot.unk14 = GetTurnStickX();
          g_CurrentShot.unk18 = GetTurnStickY();
        } else {
          g_CurrentShot.unk14 = 0;
          g_CurrentShot.unk18 = 0;
        }

        SetShotOffsetMarker(g_CurrentShot.unk14, g_CurrentShot.unk18);
        D_80227150_ovl_00.unk10 = g_CurrentShot.unk14;
        D_80227150_ovl_00.unk14 = g_CurrentShot.unk18;
        if ((D_80227194_ovl_00 != 0)
                ? (D_80104F78.unk4 == g_CurrentShot.unk40)
                : (g_TurnController.pressedButtons & A_BUTTON)) {
          s32 sound;

          g_CurrentShot.unk4 = g_CurrentShot.unk40;
          D_80227150_ovl_00.power = func_8005F248(g_CurrentShot.unk40);
          g_SavedPlayers[playerIndex] = g_Players[playerIndex];
          g_Players[playerIndex].strokeCount++;
          if (g_Players[playerIndex].strokeCount >= 100) {
            g_Players[playerIndex].strokeCount = 99;
          }

          if (GetSelectedClub()->surfaceMask & CLUB_SURFACE_GREEN) {
            if (GetSurfaceInfo(g_CurrentShot.surface)->kind == 0xB) {
              g_Players[playerIndex].unk24++;
              if (g_Players[playerIndex].unk24 >= 100) {
                g_Players[playerIndex].unk24 = 99;
              }
            }

            D_80227150_ovl_00.unkC = 0;
            D_80227150_ovl_00.unk10 = 0;
            D_80227150_ovl_00.unk14 = 0;
            nextState = 6;
            if (g_ScreenEffect == 3) {
              SetScreenEffect(4, 0);
            }

            sound = SOUND_IMPACT_OFF_MARK;
          } else {
            nextState = 5;
            if (D_80227150_ovl_00.power == MAX_LAUNCH_POWER) {
              StartShotEffect(1);
              sound = SOUND_IMPACT_ON_MARK;
            } else {
              sound = SOUND_IMPACT_OFF_MARK;
            }
          }

          PlaySound(sound, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
        } else if (g_CurrentShot.unk40 >= 59 ||
            (D_80227194_ovl_00 == 0 &&
                (g_TurnController.pressedButtons & B_BUTTON))) {
          UpdateGolferTranslucency();
          nextState = 2;
          ResetShotModeChange();
          PlaySound(
              SOUND_MENU_BLOCKED, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
          if (g_RoundHud.greenGridState != 0) {
            g_RoundHud.greenGridState = 1;
          }

          g_RoundHud.unkF = 1;
          g_RoundHud.unk12 = 1;
        }

        g_CurrentShot.unk40++;
        break;
      case 5:
        UpdateImpactPointMode();
        ReuseLastCharacterMatrices(0);
        BuildShotPreview(&D_80227150_ovl_00, state);
        areCallsAllowed = FALSE;
        if (g_ShotFlags & 2) {
          g_RoundHud.flags = 0x20;
        } else {
          g_RoundHud.flags = 0x28;
        }

        if (D_80227194_ovl_00 == 0) {
          g_CurrentShot.unk14 = GetTurnStickX();
          g_CurrentShot.unk18 = GetTurnStickY();
        } else {
          s32 framesLeft = D_80104F78.unk8 - g_CurrentShot.unk40 - 10;

          if (framesLeft > 10) {
            g_CurrentShot.unk14 = 0;
            g_CurrentShot.unk18 = 0;
          } else if (framesLeft <= 0) {
            g_CurrentShot.unk14 = D_80104F78.unk14;
            g_CurrentShot.unk18 = D_80104F78.unk18;
          } else {
            g_CurrentShot.unk14 = D_80104F78.unk14 * (10 - framesLeft) / 10;
            g_CurrentShot.unk18 = D_80104F78.unk18 * (10 - framesLeft) / 10;
          }
        }

        SetShotOffsetMarker(g_CurrentShot.unk14, g_CurrentShot.unk18);
        D_80227150_ovl_00.unk10 = g_CurrentShot.unk14;
        D_80227150_ovl_00.unk14 = g_CurrentShot.unk18;
        if (((D_80227194_ovl_00 != 0)
                    ? (g_CurrentShot.unk40 == D_80104F78.unk8)
                    : (g_TurnController.pressedButtons & A_BUTTON)) ||
            g_CurrentShot.unk40 >= 69) {
          g_CurrentShot.unk8 = g_CurrentShot.unk40;
          D_80227150_ovl_00.unkC = g_CurrentShot.unk40 - 60;
          if (D_80227150_ovl_00.unkC > 6) {
            D_80227150_ovl_00.unkC = 6;
          }

          if (D_80227150_ovl_00.unkC < -6) {
            D_80227150_ovl_00.unkC = -6;
          }

          PlayImpactClickSound();
          StopSound(8);
          nextState = 6;
          if (g_CurrentShot.shotMode == SHOT_MODE_POWER &&
              (D_80227150_ovl_00.unkC != 0 ||
                  D_80227150_ovl_00.power < MAX_LAUNCH_POWER)) {
            if (g_Players[playerIndex].unk28 > 0) {
              g_Players[playerIndex].unk28--;
            }
          }

          if (g_ScreenEffect == 3) {
            SetScreenEffect(4, 0);
          }
        } else {
          g_CurrentShot.unk40++;
          if (71 - g_CurrentShot.unk40 < D_80224000_ovl_00 &&
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
          g_CurrentShot.unk8 = g_CurrentShot.unk40;
          if (g_TurnController.pressedButtons & B_BUTTON) {
            D_80227150_ovl_00.power = MAX_LAUNCH_POWER;
          }

          D_80227150_ovl_00.unkC = 0;
          if (D_80227150_ovl_00.unkC > 6) {
            D_80227150_ovl_00.unkC = 6;
          }

          if (D_80227150_ovl_00.unkC < -6) {
            D_80227150_ovl_00.unkC = -6;
          }

          PlayImpactClickSound();
          StopSound(8);
          nextState = 6;
          if (g_ScreenEffect == 3) {
            SetScreenEffect(4, 0);
          }
        }

        break;
      case 6: {
        s32 shotMode;

        EndRumbleForceStop();
        ReuseLastCharacterMatrices(0);
        g_RoundHud.flags = 0x80;
        D_80223FF8_ovl_00++;
        g_SavedShots[playerIndex] = g_CurrentShot;
        SetCharacterTranslucent(0, FALSE);
        SetFramebufferCount(3);
        PlaceBallAtRest(playerIndex, &g_TurnBallSimulation);
        LoadOverlay(GetOverlayOfModule(8));
        SetPaintSeed(&D_80227150_ovl_00);
        if (IsRecordableHoleOut(playerIndex)) {
          RecordHoleOut(playerIndex);
        }

        if (IsRingShotRound()) {
          RecordRingShotHole(playerIndex);
        }

        if (IsGameFlagSet(0x32)) {
          SetGameFlag(0xF);
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
        nextState = 8;
        StartFlightCamera(f_ShotFlightCount++);
        D_80223FFC_ovl_00 = 0;
        break;
      }
      case 8:
        if (D_80227170_ovl_00[playerIndex] == D_80224000_ovl_00 - 3 &&
            GetClub(g_Players[playerIndex].clubs[g_CurrentShot.clubSlot])
                    ->kind == 0) {
          if (g_CurrentShot.shotMode == SHOT_MODE_NORMAL) {
            StartSwingTrail(2);
          } else if (g_CurrentShot.shotMode == SHOT_MODE_POWER) {
            StartSwingTrail(1);
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
            (IsGameFlagSet(0xF) && D_80227170_ovl_00[playerIndex] >= 4)) {
          if (IsBButtonHeld()) {
            D_80223FFC_ovl_00 = 1;
          }

          if (g_IsModuleAlive[1] || ReportModuleNotAlive(1)) {
            ResetBallRotation();
          }

          LaunchBallSimulation(&g_TurnBallSimulation, 0, &D_80227150_ovl_00);
          nextState = 9;
          if (D_80227150_ovl_00.unkC == 0 &&
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
      case 9: {
        s32 isCameraDone;
        s32 previousPosition[3];

        if (D_80224000_ovl_00 + 10 == D_80227170_ovl_00[playerIndex]) {
          StartSwingTrail(0);
        }

        ReuseLastCharacterMatrices(0);
        previousPosition[0] = g_BallPosition[0];
        previousPosition[1] = g_BallPosition[1];
        previousPosition[2] = g_BallPosition[2];
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
          nextState = 6;
        }

        if (g_MergedController.buttons & (Z_TRIG | L_TRIG)) {
          u32 previousDistance =
              IntLength2D(previousPosition[0] - g_HoleSetup.cupPosition[0],
                  previousPosition[2] - g_HoleSetup.cupPosition[AXIS_Z]);

          for (i = 0; i < 4; i++) {
            u32 distance =
                IntLength2D(g_BallPosition[0] - g_HoleSetup.cupPosition[0],
                    g_BallPosition[2] - g_HoleSetup.cupPosition[AXIS_Z]);

            if (g_TurnBallSimulation.isRunning != 2 || g_GameState.isMiniGolf ||
                (GetSurfaceInfo(g_TurnBallSimulation.unkA4)->kind == 0xB &&
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

            if (IsGameFlagSet(0xF) ||
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
                ToggleGameFlag(0xF);
              }
            }
          }

          if (g_MergedController.buttons & A_BUTTON) {
            while (g_TurnBallSimulation.isRunning) {
              StepBallSimulation(&g_TurnBallSimulation);
              TickStopwatches();
              UpdateFlightCamera(TRUE, g_TurnController.buttons);
            }

            ResetShotEffects();
            ClearShotEffects(2);
          }
        }

        if (!g_TurnBallSimulation.isRunning) {
          ClearShotEffects(0);
          nextState = 10;
          D_800C3094 = 1;
          D_80227170_ovl_00[playerIndex] = 0;
          ForceStopRumble();
        }

        break;
      }
      case 10: {
        s32 isCameraDone = UpdateFlightCamera(TRUE, g_TurnController.buttons);

        if (g_TurnBallSimulation.unkA0) {
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
          nextState = 6;
          StartLandingReaction(-1);
          StartBanner(-1);
        } else if (isCameraDone) {
          UnkB8* player;
          SurfaceInfo* surfaceInfo;

          MeasureTeeShot(playerIndex);
          UpdateContest(FALSE);
          StartLandingReaction(-1);
          player = &g_Players[playerIndex];
          surfaceInfo = GetSurfaceInfo(g_BallSurface);
          func_80207660_ovl_00(player);
          ClearPassedRings(player);
          if ((surfaceInfo->kind == 0xD || surfaceInfo->kind == 0x15) &&
              !IsGameFlagSet(0x45)) {
            g_Players[playerIndex].strokeCount++;
            if (g_GameState.gameMode == GAME_MODE_TRAINING) {
              if (g_Players[playerIndex].strokeCount >= 100) {
                g_Players[playerIndex].strokeCount = 99;
              }
            } else if (g_Players[playerIndex].strokeCount >
                g_HoleSetup.par * 3) {
              g_Players[playerIndex].strokeCount = g_HoleSetup.par * 3;
            }

            if ((s8)player->ballSurface != 0x17) {
              player->ballSurface =
                  GetSurface(player->ballPosition[0], player->ballPosition[2]);
            }
          } else if (surfaceInfo->kind == 0xA) {
            player->ballPosition[0] = g_TurnBallSimulation.unkC[0];
            player->ballPosition[2] = g_TurnBallSimulation.unkC[2];
            player->ballPosition[1] = GetGroundHeight(
                player->ballPosition[0], player->ballPosition[2]);
            player->ballSurface =
                GetSurface(player->ballPosition[0], player->ballPosition[2]);
            g_Players[playerIndex].strokeCount++;
            if (g_GameState.gameMode == GAME_MODE_TRAINING) {
              if (g_Players[playerIndex].strokeCount >= 100) {
                g_Players[playerIndex].strokeCount = 99;
              }
            } else if (g_Players[playerIndex].strokeCount >
                g_HoleSetup.par * 3) {
              g_Players[playerIndex].strokeCount = g_HoleSetup.par * 3;
            }
          } else {
            player->ballPosition[0] = g_BallPosition[0];
            player->ballPosition[1] = g_BallPosition[1];
            player->ballPosition[2] = g_BallPosition[2];
            player->ballSurface = g_BallSurface;
          }

          if ((surfaceInfo->kind == 6 || surfaceInfo->kind == 7) ||
              (surfaceInfo->kind == 8 || surfaceInfo->kind == 9)) {
            player->unk9C = 1;
          }

          D_80227170_ovl_00[playerIndex] = 0;
          if (g_TurnBallSimulation.unkA0) {
            g_Players[playerIndex].unk26 = HOLE_STATUS_HOLED_OUT;
            g_Players[playerIndex]
                .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                .strokes = g_Players[playerIndex].strokeCount;
            if (!g_GameState.unk31 &&
                ((g_GameState.gameMode == GAME_MODE_MATCH_GAME ||
                     g_GameState.gameMode == GAME_MODE_GET_CHARACTER) ||
                    (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                        g_GameState.gameMode == GAME_MODE_CLUB_SLOTS))) {
              g_Players[playerIndex]
                  .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                  .strokes +=
                  GetHandicapOffset(GetCourseRecordIndex(g_GameState.gameMode),
                      g_GameState.holeIndex, g_Players[playerIndex].handicap);
            }

            nextState = 0xB;
            g_Players[playerIndex]
                .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                .putts = g_Players[playerIndex].unk24;
            FreeCharacterAndReactionLight(0);
          } else {
            FreeCharacterAndReactionLight(0);
            StartBanner(-1);
            if (g_GameState.unk31 &&
                g_Players[playerIndex].strokeCount >= D_800FE330) {
              nextState = 0xB;
            } else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
              nextState = 0xB;
              if (g_Players[playerIndex].strokeCount < g_HoleSetup.par) {
                EndFlightCamera();
                StartHudAnimation(2);
                nextState = 0x10;
              }
            } else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
              nextState = 0xF;
              OpenChoiceBox(
                  g_GameState.courseIndex == DRIVING_RANGE_COURSE_INDEX
                      ? CHOICE_BOX_ID_DRIVING_RANGE
                      : CHOICE_BOX_ID_TRAINING_SHOT);
            } else if (!g_GameState.unk31 &&
                (g_GameState.gameMode == GAME_MODE_MATCH_GAME ||
                    g_GameState.gameMode == GAME_MODE_GET_CHARACTER)) {
              if (g_Players[playerIndex].strokeCount >= g_HoleSetup.par * 3 ||
                  GetMatchHoleResult(0, 1)) {
                nextState = 0xB;
                GiveUpHole(playerIndex);
              } else {
                EndFlightCamera();
                StartHudAnimation(2);
                nextState = 0x10;
              }
            } else if (!g_GameState.unk31 &&
                (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                    g_GameState.gameMode == GAME_MODE_CLUB_SLOTS)) {
              s8 results[MAX_PLAYER_COUNT];

              if (g_Players[playerIndex].strokeCount >= g_HoleSetup.par * 3) {
                g_Players[playerIndex]
                    .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                    .strokes = 0;
                g_Players[playerIndex].unk26 = HOLE_STATUS_OUT;
              }

              if (GetGroupHoleResult(results) || results[playerIndex] == 2) {
                g_Players[playerIndex].unk26 = HOLE_STATUS_OUT;
                g_Players[playerIndex]
                    .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                    .strokes = 0;
                g_Players[playerIndex].strokeCount = 0;
                nextState = 0xB;
              } else {
                EndFlightCamera();
                StartHudAnimation(2);
                nextState = 0x10;
              }
            } else if (g_Players[playerIndex].strokeCount >=
                g_HoleSetup.par * 3) {
              g_Players[playerIndex]
                  .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                  .strokes = g_HoleSetup.par * 3;
              g_Players[playerIndex].unk26 = HOLE_STATUS_OUT;
              nextState = 0xB;
              if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
                g_Players[playerIndex].stopwatchFrameCount += 18000;
                g_GameState.stopwatchFrameCount += 18000;
                g_HoleTimeFrames[g_GameState.holeIndex] += 18000;
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

                nextState = 0xB;
              }
            } else {
              EndFlightCamera();
              if (!IsGameFlagSet(0x32)) {
                StartHudAnimation(2);
              }

              nextState = 0x10;
            }
          }
        }

        break;
      }
      case 0xB: {
        s32 character;
        s32 palette;
        s32 reaction;

        if (IsCharacterSlotBusy(0)) {
          FreeCharacterAndReactionLight(0);
          break;
        }

        if (g_HoleSetup.conditions.sceneColorIndex == 0xB) {
          StartFireworks();
        }

        ClearReactionSpot();
        ConsiderReactionSpot(g_CurrentShot.ballPosition);
        ConsiderReactionSpot(g_HoleSetup.cupPosition);
        ConsiderReactionSpot(g_HoleSetup.teePosition);
        ClearShotEffects(0);
        EndFlightCamera();
        if (IsGameFlagSet(0x3E) && f_HasGutsTestRun) {
          character = f_GutsTestCharacter;
          palette = 0;
        } else {
          character = g_Players[playerIndex].characterIndex;
          palette = g_Players[playerIndex].paletteIndex;
        }

        LoadCharacter(0, character + CHARACTER_COUNT, palette);
        SetCharacterLighting(0, 1);
        memset(&g_RoundHud, 0, sizeof(g_RoundHud));
        if (!g_TurnBallSimulation.unkA0) {
          PlaySong(7, 0);
          reaction = 0xF;
        } else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
          reaction = 0xF;
          if (HasNoRingsLeft(&g_Players[playerIndex])) {
            func_8006034C(1);
            reaction = 0xC;
          }
        } else if (g_Players[playerIndex].strokeCount == 1) {
          reaction = 0xC;
        } else {
          switch (g_Players[playerIndex].strokeCount - g_HoleSetup.par) {
            case -3:
            case -2:
              reaction = 0xC;
              break;
            case -1:
              reaction = 0xD;
              break;
            case 0:
              reaction = 0xE;
              break;
            default:
              reaction = 0xF;
              break;
          }
        }

        StartReaction(0, g_Players[playerIndex].characterIndex, reaction);
        SetCharacterAnimation(0, reaction);
        if (IsGameFlagSet(0x3E) && f_HasGutsTestRun) {
          SetCharacterAnimation(0, f_GutsTestMotion);
          StartReaction(0, f_GutsTestCharacter, f_GutsTestMotion);
        }

        D_80224000_ovl_00 =
            GetCurrentAnimationEndFrame(0) - GetAnimationFrame(0) + 10;
        if (IsGameFlagSet(0x32)) {
          D_80224000_ovl_00 = 15;
        }

        PlaceAndViewCharacter(0);
        f_ShouldUpdateReaction = 1;
        if (g_Players[playerIndex].unk26 == HOLE_STATUS_HOLED_OUT) {
          if (g_Players[playerIndex].strokeCount == 1) {
            CountNotableShot(playerIndex, 7);
          } else {
            switch (g_Players[playerIndex].strokeCount - g_HoleSetup.par) {
              case -3:
                CountNotableShot(playerIndex, 8);
                break;
              case -2:
                CountNotableShot(playerIndex, 9);
                break;
              case -1:
                CountNotableShot(playerIndex, 10);
                break;
            }
          }
        }

        if (g_Players[playerIndex].strokeCount - g_HoleSetup.par < 0) {
          ReportBirdieBadgeCount();
        }

        nextState = 0xC;
        D_80227170_ovl_00[playerIndex] = 0;
        break;
      }
      case 0xC:
        PlaceAndViewCharacter(0);
        areCallsAllowed = FALSE;
        f_ShouldUpdateReaction = 1;
        if (D_80227170_ovl_00[playerIndex] == D_80224000_ovl_00 - 10) {
          StartBanner(-1);
        }

        if (g_HoleSetup.conditions.sceneColorIndex == 0xB) {
          D_80224000_ovl_00 = 160;
        }

        if (D_80227170_ovl_00[playerIndex] >= D_80224000_ovl_00) {
          StopFireworks();
          if (g_GameState.gameMode == GAME_MODE_TRAINING) {
            nextState = 0xF;
            if (g_GameState.courseIndex == DRIVING_RANGE_COURSE_INDEX) {
              OpenChoiceBox(CHOICE_BOX_ID_DRIVING_RANGE);
            } else {
              OpenChoiceBox(CHOICE_BOX_ID_TRAINING_HOLE_END);
            }
          } else if (g_GameState.gameMode == GAME_MODE_RING_SHOT &&
              (!g_TurnBallSimulation.unkA0 ||
                  !HasNoRingsLeft(&g_Players[playerIndex]))) {
            nextState = 0xF;
            OpenChoiceBox(CHOICE_BOX_ID_RING_SHOT);
          } else {
            nextState = 0x12;
          }

          D_80227170_ovl_00[playerIndex] = 0;
        }

        break;
      case 0xF:
        if (g_GameState.gameMode == GAME_MODE_TRAINING) {
          if (IsGameFlagSet(0x3E) && g_Players[playerIndex].unk26 != 0) {
            DebugTestGutsMotion();
          }

          switch (UpdateChoiceBox(&g_TurnController)) {
            case CHOICE_RESULT_CONTINUE:
              EndFlightCamera();
              StartHudAnimation(2);
              nextState = 0x11;
              ForceStopRumble();
              break;
            case CHOICE_RESULT_MULLIGAN:
              g_IsTrainingHeadingKept = 1;
              g_Players[playerIndex] = g_SavedPlayers[playerIndex];
              EndFlightCamera();
              StartHudAnimation(2);
              nextState = 0x11;
              ForceStopRumble();
              break;
            case CHOICE_RESULT_HOLE_OUT:
              if (g_Players[playerIndex].unk26 == HOLE_STATUS_HOLED_OUT &&
                  IsRecordableHoleOut(playerIndex)) {
                InsertRankedRecord(&D_800FEDD8[playerIndex]);
              }

              nextState = 0x14;
              EndFlightCamera();
              FreeCharacterAndReactionLight(0);
              if (IsGameFlagSet(0x3E)) {
                nextState = 0xB;
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
              PlaySound(0x8E, 3, SOUND_FULL_SCALE);
              SkipNextPinDraw();
              PlacePlayersOnTee();
              nextState = 0x11;
              ForceStopRumble();
              break;
            case CHOICE_RESULT_RING_SHOT_HOLE_OUT:
              nextState = 0x14;
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
      case 0x10:
        nextState = 0x11;
        break;
      case 0x11:
      case 0x14:
        break;
      case 0x12:
        if (IsRecordableHoleOut(playerIndex) &&
            g_Players[playerIndex].unk26 == HOLE_STATUS_HOLED_OUT) {
          InsertRankedRecord(&D_800FEDD8[playerIndex]);
        }

        CheckRingShotClear(playerIndex);
        if (D_80227170_ovl_00[playerIndex] == 1) {
          StartBanner(-1);
          g_IsHudTurnAwayRequested = 1;
          if (g_GameState.unk31) {
            nextState = 0x14;
          } else if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
              g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
            s8 results[MAX_PLAYER_COUNT];
            s32 isAnyOut = FALSE;
            s32 isHoleOver = GetGroupHoleResult(results);

            for (i = 0; i < g_GameState.playerCount; i++) {
              if (results[i] == 2 && g_Players[i].unk26 == 0) {
                g_Players[i].unk26 = 2;
                isAnyOut = TRUE;
              }
            }

            nextState = (!isHoleOver && isAnyOut) ? 0x16 : 0x14;
          } else {
            nextState = 0x14;
            if (g_GameState.gameMode == GAME_MODE_MATCH_GAME ||
                g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
              s32 result;

              GetHandicapOffset(g_GameState.courseIndex, g_GameState.holeIndex,
                  g_Players[playerIndex].handicap);
              GetHandicapOffset(g_GameState.courseIndex, g_GameState.holeIndex,
                  g_Players[playerIndex ^ 1].handicap);
              result = GetMatchHoleResult(playerIndex, playerIndex ^ 1);
              osSyncPrintf("CheckHoltOutMatch %d\n", result);
              if (result == 1) {
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
                g_Players[playerIndex ^ 1].unk26 = HOLE_STATUS_OUT;
              } else if (result == 2) {
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
              } else if (result == 3 && g_Players[playerIndex ^ 1].unk26 == 0) {
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
                g_Players[playerIndex ^ 1].unk26 = HOLE_STATUS_OUT;
              }
            }
          }
        }

        break;
      case 0x16:
        for (i = 0; i < g_GameState.playerCount; i++) {
          if (g_Players[i].unk26 == 2) {
            break;
          }
        }

        if (i == g_GameState.playerCount) {
          nextState = 0x14;
          break;
        }

        if (IsCharacterSlotBusy(0)) {
          FreeCharacterAndReactionLight(0);
          break;
        }

        for (i = 0; i < g_GameState.playerCount; i++) {
          if (g_Players[i].unk26 == 2) {
            g_Players[i].unk26 = HOLE_STATUS_OUT;
            break;
          }
        }

        nextState = 0x14;
        if (i != g_GameState.playerCount) {
          ClearReactionSpot();
          LoadCharacter(0, g_Players[i].characterIndex + CHARACTER_COUNT,
              g_Players[i].paletteIndex);
          ConsiderReactionSpot(g_Players[i].ballPosition);
          ConsiderReactionSpot(g_HoleSetup.cupPosition);
          ConsiderReactionSpot(g_HoleSetup.teePosition);
          SetCharacterLighting(0, 1);
          memset(&g_RoundHud, 0, sizeof(g_RoundHud));
          StartReaction(0, g_Players[i].characterIndex, 0xF);
          SetCharacterAnimation(0, 0xF);
          PlaySong(7, 0);
          PlaceAndViewCharacter(0);
          f_ShouldUpdateReaction = 1;
          ShowFlipMessage(0);
          nextState = 0x17;
          D_80227170_ovl_00[playerIndex] = 0;
        }

        break;
      case 0x17:
        PlaceAndViewCharacter(0);
        f_ShouldUpdateReaction = 1;
        areCallsAllowed = FALSE;
        if (D_80227170_ovl_00[playerIndex] == 80) {
          ShowFlipMessage(-1);
        }

        if (D_80227170_ovl_00[playerIndex] == 90) {
          StartBanner(-1);
          nextState = 0x16;
          ClearDebugText();
          D_80227170_ovl_00[playerIndex] = 0;
        }

        break;
      default:
        SetDebugTextHighlight(1);
        PrintDebugText("P-S-ERR", 20, 2);
        nextState = 2;
        break;
    }

    g_Players[playerIndex].unkC = nextState;
    g_BallSimulations[0] = g_TurnBallSimulation;
    g_PreviousBallSurfaceInfo = D_80227150_ovl_00;
  }

  MarkNearbyGroundTiles(
      g_CurrentShot.ballPosition[0], g_CurrentShot.ballPosition[2]);
  GetBallSurfaceInfo(
      &g_Players[g_GameState.currentPlayerIndex], &D_80227150_ovl_00);
  if (areCallsAllowed) {
    PlayWaitingPlayersCalls();
  }

  AnimateAllCharacters();
  if (f_ShouldUpdateReaction) {
    UpdateReaction(0);
  }

  if (IsGameFlagSet(0x4A)) {
    FrameBallAndShot();
  }

  if (g_RoundHud.unkF) {
    s32 distance = GetSelectedShotDistance();
    f32 angle = (u16)g_CurrentShot.unk0 * 0.0000958738f;
    f32 cosine;
    f32 scale;
    f32 length;

    if (GetSelectedClub()->surfaceMask & CLUB_SURFACE_GREEN) {
      cosine = cosf(angle);
      scale = 15360.0f;
    } else {
      cosine = cosf(angle);
      scale = 14045.184f;
    }

    length = distance * scale;
    g_CurrentShot.unk44[0] = (cosine * length) + g_CurrentShot.ballPosition[0];
    g_CurrentShot.unk44[2] =
        (sinf(angle) * length) + g_CurrentShot.ballPosition[2];
    g_CurrentShot.unk44[1] =
        GetGroundHeight(g_CurrentShot.unk44[0], g_CurrentShot.unk44[2]);
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
