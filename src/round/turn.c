/*
 * Each player's turn: the aim camera and the aim controls, setting up and
 * taking the shot, the shot preview, Course Points, records of holes and Ring
 * Shot, picking the next player, and the round's debug tools.
 */

#include "round/turn.h"

#include <framebuffer_count.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>

#include "audio/sound.h"
#include "camera/camera.h"
#include "camera/camera_framing.h"
#include "camera/shot_camera.h"
#include "camera/view_projection.h"
#include "characters/character.h"
#include "characters/character_model.h"
#include "characters/reaction.h"
#include "controllers/controller.h"
#include "controllers/transfer_pak.h"
#include "course/course.h"
#include "course/course_space.h"
#include "course/hole_conditions.h"
#include "course/hole_loader.h"
#include "course/hole_setup.h"
#include "course/pin.h"
#include "course/surface.h"
#include "course/terrain.h"
#include "debug/debug_text.h"
#include "debug/game_flags.h"
#include "game/game.h"
#include "game/scene.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "menus/menu.h"
#include "round/computer_golfer.h"
#include "round/fireworks.h"
#include "round/game_state.h"
#include "round/hole_intro.h"
#include "round/movie.h"
#include "round/pause_menu.h"
#include "round/play.h"
#include "round/player.h"
#include "round/round_events.h"
#include "round/round_hud.h"
#include "round/round_stats.h"
#include "round/scorecard.h"
#include "round/scoring.h"
#include "round/turn.h"
#include "save/awards.h"
#include "save/play_record.h"
#include "save/save.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "screen/screen_layers.h"
#include "screen/sky_and_hud_animations.h"
#include "shot/ball_simulation.h"
#include "shot/club.h"
#include "shot/club_panel.h"
#include "shot/current_shot.h"
#include "shot/golf_balls.h"
#include "shot/particle_effects.h"
#include "shot/shot_effects.h"
#include "shot/shot_simulation.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/frame_time.h"
#include "system/main.h"
#include "system/memory_address.h"
#include "system/overlay_manager.h"
#include "text/text.h"

#define IMPACT_POINT_SOUND_CHANNEL 8
#define IMPACT_POINT_SOUND_VOLUME 60
#define IMPACT_POINT_SOUND_REVERB 20
#define TEST_CONTROLLER 0
#define DEBUG_AIM_TILT_STEP 1.0f
#define DEBUG_FRAMING_TURN_PER_STICK 0.002f
#define BALL_MOVE_SPEED_PER_STICK 2
#define IMPACT_POINT_SOUND_PAN 230

// The buttons that hold the impact point mode
#define IMPACT_POINT_BUTTONS (Z_TRIG | L_TRIG)

// Course units to tenths of a meter, as METERS_PER_UNIT * 10
#define DECIMETERS_PER_UNIT (1.0f / 1536)
// Sixty meters
#define NEAR_CUP_DECIMETERS_MAX 600.0f

// The most of one kind of notable shot that a player's count holds
#define NOTABLE_SHOT_COUNT_MAX 0xFF
#define RING_SHOT_CLEAR_COUNT_MAX 99

#define HOLE_OUT_CALL_CHANNEL 3
// How far the control pad tilts the stick, as GetTurnStickX and GetTurnStickY
// read it
#define DPAD_STICK_TILT 40
// The aim camera stays inside the course's ground, in course units
#define AIM_CAMERA_X_MAX 4347904.0f
#define AIM_CAMERA_Z_MAX 8542208.0f
// How far above the ground under the ball the aim camera looks, in world units
#define AIM_LOOK_AT_RISE 17.25f
// The bounds that KeepAimCameraOnCourse keeps the aim camera within
#define AIM_CAMERA_COORDINATE_MIN (-0x25800)
#define AIM_CAMERA_COURSE_X_MAX 0x425800
#define AIM_CAMERA_COURSE_Z_MAX 0x825800
#define AIM_CAMERA_MINI_GOLF_X_MAX 0x109600
#define AIM_CAMERA_MINI_GOLF_Z_MAX 0x104B00
// How far the C buttons push the aim camera on, once it has been pulled in
#define AIM_CAMERA_PUSH_STEP 0x25800
#define AIM_CAMERA_MINI_GOLF_PUSH_STEP 0xB400
// Where AimDownRange puts the aim camera, on the green and elsewhere
#define DOWN_RANGE_GREEN_OFFSET (-0x10E00)
#define DOWN_RANGE_DISTANCE (-0x2D000)
#define DOWN_RANGE_HEIGHT (-0x12C00)
// How far beyond the camera the down-range look-at point lies
#define DOWN_RANGE_LOOK_AHEAD 0x280000
// Where the aim camera eases to, in course units
#define AIM_CAMERA_PUTT_DISTANCE (-0x1800C)
// Back from where the shot lands
#define AIM_CAMERA_APPROACH_BACKOFF 0x4B000
// Short of the shot's full distance
#define AIM_CAMERA_SHOT_BACKOFF_YARDS 50
#define AIM_CAMERA_PUTT_HEIGHT (-0xB400)
#define AIM_CAMERA_APPROACH_HEIGHT (-0x12C00)
#define AIM_CAMERA_SHOT_HEIGHT (-0x25800)
// The aim camera moves this fraction of the way each frame, up to the most
#define AIM_CAMERA_EASE_FRACTION 0.3f
#define AIM_CAMERA_EASE_STEP_MAX (10 * UNITS_PER_YARD)
// The free-look camera starts behind the ball, and C-left and C-right turn it
#define FREE_LOOK_START_DISTANCE (-0x12C00)
#define FREE_LOOK_TURN_STEP 200
// A shorter pulse than the shots' rumble in effects.c
#define TURN_RUMBLE_FRAME_COUNT 8
// Every AMBIENT_SOUND_INTERVAL frames, one of the course's ambient sounds may
// play
#define AMBIENT_SOUND_INTERVAL 240
#define AMBIENT_SOUND_CHOICES 7
#define NO_AMBIENT_SOUND (-1)
#define AMBIENT_SOUND_CHANNEL 10
#define AMBIENT_SOUND_VOLUME 0x5F
#define AMBIENT_SOUND_REVERB 15
#define SOUND_PAN_RANGE 255
// The most shots that each kind of replay record keeps
#define HOLE_RECORD_STROKES_MAX 5
#define RING_SHOT_RECORD_STROKES_MAX 6
#define IMPACT_SOUND_VOLUME 0x64
// The order in which players' lies come up, from the first: the tee, then off
// the green (1) and on it (0), which comparing the lie's surface kind with
// SURFACE_KIND_GREEN gives
#define LIE_PRIORITY_TEE 2
// Under GAME_FLAG_FEWEST_STROKES_FIRST, a priority that falls with each stroke
#define STROKE_PRIORITY_BASE 200
// The stick turns the aim by half its tilt, times a quarter (a sixth for
// g_AltHeading) of it
#define AIM_STICK_SPEED_DIVISOR 4
#define LOOK_BACK_STICK_SPEED_DIVISOR 6
// The D-pad's turn speeds up over this many frames
#define AIM_DPAD_FRAMES_MAX 60
#define AIM_DPAD_SPEED_DIVISOR 6
#define AIM_DPAD_BASE_SPEED 3
// The shot preview's step count, which a debug flag lets the D-pad tune
#define SHOT_PREVIEW_STEPS_MIN 2
#define SHOT_PREVIEW_STEPS_MAX 20
#define SHOT_PREVIEW_SCROLL_STEP 8
#define INITIAL_SHOT_PREVIEW_STEP_COUNT 10
#define AIM_CAMERA_DEBUG_TILT_SCALE 30.0f
// The pause screen's camera circles the ball at this distance, and rises this
// far above it
#define PAUSE_CAMERA_RADIUS 1536000.0f
#define PAUSE_CAMERA_RISE 4608000.0f
// In mini golf, it circles the middle of the course instead
#define MINI_GOLF_CENTER_X 543488.0f
#define MINI_GOLF_CENTER_Z 533888.0f
#define MINI_GOLF_PAUSE_CAMERA_RADIUS 15360.0f
#define MINI_GOLF_PAUSE_CAMERA_RISE 890880.0f
#define MINI_GOLF_PAUSE_CAMERA_DISTANCE 0x1A400
// The aim camera keeps above the lowest ground of this many points behind it
#define AIM_GROUND_PROBE_COUNT 4
#define AIM_GROUND_PROBE_STEP 0x12C00
// Without an impact point, the camera drops lower and rises less with its
// height
#define NO_IMPACT_POINT_DROP 5683.2f
#define NO_IMPACT_POINT_HEIGHT_SCALE 0.4f
// The camera rises at most the square of its distance in thousands, over this
#define AIM_CAMERA_RISE_DIVISOR 4
// On the green, the camera looks this far past the ball and this far above the
// ground
#define GREEN_LOOK_AHEAD 0x4B000
#define GREEN_LOOK_RISE 0x1200
#define GREEN_LOOK_HEIGHT_SCALE 0.7f
// Off the green, the camera tilts down by the yards the club falls short of
// this
#define AIM_TILT_FULL_DISTANCE 300
#define AIM_TILT_PER_YARD 0.005f
// Looking back from the far side, the camera looks this far up from the ball
#define LOOK_BACK_RISE 0x3C00
#define LOOK_BACK_HEIGHT_SCALE 0.5f
#define SHOT_CAMERA_FIELD_OF_VIEW 45.0f
// A hole given up outside a match counts as this many times its par
#define GIVE_UP_PAR_MULTIPLE 3
// A replay turns the aim at once, rather than easing it round
#define GAME_FLAG_SNAP_REPLAY_AIM 0xF
// A replay eases the aim by this much plus a 128th of what is left, until this
// close
#define REPLAY_AIM_STEP_MIN 0x80
#define REPLAY_AIM_STEP_DIVISOR 128
#define REPLAY_AIM_SETTLE_FRAMES 6
#define AIM_RESET_BUTTONS (L_CBUTTONS | R_CBUTTONS)
#define AIM_CAMERA_BUTTONS (U_CBUTTONS | D_CBUTTONS | L_CBUTTONS | R_CBUTTONS)
#define AIM_CAMERA_RAISE_BUTTONS (U_CBUTTONS | R_CBUTTONS)
#define AIM_CAMERA_LOWER_BUTTONS (D_CBUTTONS | L_CBUTTONS)
#define AIM_CAMERA_SOUND_VOLUME 0x50
#define LOOK_SOUND_VOLUME 0x3F
// Frames that the impact point stays shown after Z or L is let go
#define IMPACT_POINT_LINGER_FRAMES 3
// Looking back from past the ball starts this far beyond the cup, and this high
#define LOOK_BACK_EXTRA_DISTANCE 53760.0f
#define LOOK_BACK_START_HEIGHT (-0x1E00)
// The aim camera's height and distance limits; heights are negative upwards
#define GREEN_AIM_CAMERA_HEIGHT_MIN (-0x2A00)
#define AIM_CAMERA_HEIGHT_MIN (-0x3C00)
#define AIM_CAMERA_HEIGHT_MAX (-0x96000)
#define LOOK_BACK_DISTANCE_MIN (-0x177000)
#define LOOK_BACK_DISTANCE_MAX 0x177000
#define AIM_CAMERA_DISTANCE_MIN (-0xBB800)
#define AIM_CAMERA_DISTANCE_MAX 0x8CA000
#define AIM_VIEW_TOGGLE_COUNT (GREEN_GRID_SHOWN + 1)
// Half the putt preview line's width, in screen pixels
#define PUTT_PREVIEW_HALF_WIDTH 10.0f
// Hides the shot preview
#define GAME_FLAG_HIDE_SHOT_PREVIEW 0x4B
// Lets the D-pad change how far the shot preview reaches
#define GAME_FLAG_TUNE_SHOT_PREVIEW 0x71
// PickStartingClub settles for clubs with at least this lie rating
#define STARTING_CLUB_LIE_RATING_MIN 2
// Aims every shot straight down the driving range
#define GAME_FLAG_AIM_DOWN_RANGE 0x24
// Only hole-outs in fewer strokes than this enter the records
#define RECORDED_STROKE_COUNT_MAX 4
// The most strokes a player's count shows
#define STROKE_COUNT_MAX 99
// Outside training, a player who reaches this many times the hole's par
// is out of the hole
#define STROKE_LIMIT_PER_PAR 3
// Speed golf adds this to a player stopped by the stroke limit
#define STROKE_LIMIT_PENALTY_MINUTES 10
// Where the aim camera starts, in course units
#define AIM_CAMERA_START_DISTANCE (-0xF000)
#define AIM_CAMERA_START_HEIGHT (-0x4E00)
// The stick moves the aim camera's height by this much for every AIM_STICK_STEP
// of tilt
#define AIM_CAMERA_HEIGHT_STEP 0x600
#define AIM_STICK_STEP 16
#define AIM_STICK_DEAD_ZONE 8
// The stick moves the aim camera's distance for every AIM_DISTANCE_STICK_STEP
// of tilt
#define AIM_DISTANCE_STICK_STEP 4
#define AIM_CAMERA_DISTANCE_STEP 0xF00
#define AIM_CAMERA_PUTT_DISTANCE_STEP 0x180
// How far the C buttons move the aim camera each frame
#define AIM_CAMERA_BUTTON_DISTANCE_STEP 42670.0810546875
#define AIM_CAMERA_BUTTON_HEIGHT_STEP 28446.720703125
#define AIM_CAMERA_BUTTON_PUTT_STEP 4267.00810546875
// The last gauge frames at which power, and impact, can be set
#define LAST_POWER_GAUGE_FRAME (GAUGE_IMPACT_FRAME - 1)
#define LAST_IMPACT_GAUGE_FRAME (GAUGE_IMPACT_FRAME + 9)
// How far either side of the impact mark the impact counts
#define IMPACT_TIMING_MAX 6
// The swing animation starts once fewer gauge frames are left before this one
// than its event frame, f_StateEventFrame
#define SWING_TIMING_GAUGE_FRAME 71
// A shot that the player isn't taking tilts the stick toward its offset over
// STICK_TILT_FRAME_COUNT frames, then holds it for the last
// STICK_HOLD_FRAME_COUNT frames before impact
#define STICK_TILT_FRAME_COUNT 10
#define STICK_HOLD_FRAME_COUNT 10
// While putting, the crane's steps are this times a count that grows with the
// putt's length
#define AIM_CAMERA_PUTT_STEP_UNIT 426.700810546875
// The counts for each putt mode, when craning and when moving along the line
#define SHORT_PUTT_CRANE_STEP_COUNT 10
#define MIDDLE_PUTT_CRANE_STEP_COUNT 13
#define LONG_PUTT_CRANE_STEP_COUNT 15
#define SHORT_PUTT_DOLLY_STEP_COUNT 10
#define MIDDLE_PUTT_DOLLY_STEP_COUNT 15
#define LONG_PUTT_DOLLY_STEP_COUNT 20
// The crane swings the camera forward only while it is below this height, and
// back only above the other
#define AIM_CAMERA_CRANE_FORWARD_HEIGHT (-0x5A00)
#define AIM_CAMERA_CRANE_BACK_HEIGHT (-0x96000)
// Shots count towards Course Points on their first flights only
#define COUNTED_FLIGHT_COUNT_MAX 2
// How UpdateTurns polls the computer golfer's plan: at most this many
// times as the aim is prepared, then this many times a frame of aiming
#define PLAN_PREPARING_POLL_COUNT 100
#define PLAN_AIMING_POLL_COUNT 400
// The time budget of each poll while the aim is prepared, and of the
// first frame of aiming; the budget then grows by a factor each frame,
// up to the most
#define PLAN_PREPARING_TIME_BUDGET 10
#define PLAN_START_TIME_BUDGET 30
#define PLAN_TIME_BUDGET_GROWTH 1.05f
#define PLAN_TIME_BUDGET_MAX 0x800
// Replaced by PLAN_START_TIME_BUDGET before it is read
#define INITIAL_PLAN_TIME_BUDGET 10
// A prepared shot waits this many frames of aiming before it is played
#define PREPARED_SHOT_AIM_FRAMES_MIN 30
// The latest frame of a skipped swing animation at which its impact comes
#define SKIPPED_SWING_IMPACT_FRAME 15
// The f_SwingCount of a shot's first replay
#define FIRST_REPLAY_SWING_COUNT 2
// Whether the swing is a replay of the shot, which leaves the stopwatches
#define IsReplayingShot() (f_SwingCount >= FIRST_REPLAY_SWING_COUNT)
// A replay can itself be replayed in flight from this frame of its swing
#define REPLAY_SWING_FRAMES_MIN 40

// Frames from the start of a swing, through the flight that follows it
// A wood's swing trail starts this many frames before the impact
#define WOOD_SWING_TRAIL_LEAD_FRAMES 3
// With GAME_FLAG_FAST_FLIGHT, the ball is launched by this frame
#define FAST_FLIGHT_LAUNCH_FRAME 4
// The swing trail fades this many frames after the impact
#define SWING_TRAIL_FADE_DELAY 10
// The golfer's random texture animations, which the swing stops, start again
// at this frame
#define RANDOM_TEXTURE_RESUME_FRAME 30
// With Z or L held, a rolling ball takes up to this many more steps a frame,
// outside mini golf and until it rolls toward the cup on the green
#define ROLL_SKIP_STEPS_MAX 4
// In the debug mode, with fast flight or Z or L held, the ball takes this
// many more steps a frame
#define FAST_FLIGHT_EXTRA_STEPS 3

// A reaction lasts this many frames past its animation, whose end calls
// StartBanner(-1)
#define REACTION_HOLD_FRAMES 10
// How long a reaction lasts with GAME_FLAG_FAST_AUTOPLAY set
#define SHORT_REACTION_FRAMES 15
// How long a reaction lasts in sudden death
#define SUDDEN_DEATH_REACTION_FRAMES 160
// The frames at which PLAYER_STATE_SHOWING_OUT_PLAYER calls
// ShowFlipMessage(-1), and ends
#define OUT_PLAYER_FLIP_FRAME 80
#define OUT_PLAYER_SHOWN_FRAMES 90
// A putt's power, scaled to a short putt's by about how many times a short
// putt's reach the long and middle putts have, picks its swing: the middle
// putt's from MIDDLE_PUTT_SWING_POWER, the short putt's below it, slowed
// below SLOW_PUTT_SWING_POWER for PUTT_SLOWDOWN_LEAD_FRAMES before the impact
#define LONG_PUTT_REACH_SCALE 6
#define MIDDLE_PUTT_REACH_SCALE 3
#define MIDDLE_PUTT_SWING_POWER (LAUNCH_POWER_MAX / 2)
#define SLOW_PUTT_SWING_POWER (LAUNCH_POWER_MAX / 4)
#define PUTT_SLOWDOWN_LEAD_FRAMES 3

// The song's tempo while aiming, a step below NORMAL_SONG_TEMPO; on the green,
// f_GreenSongTempo rises from it by GREEN_SONG_TEMPO_STEP each frame
#define AIMING_SONG_TEMPO 0x7F
#define GREEN_SONG_TEMPO_STEP 0.04f
#define GREEN_SONG_TEMPO_MAX 155.0f
// While aiming, the golfer's alpha swings by AIMING_ALPHA_SWING about
// AIMING_MEAN_ALPHA, at AIMING_ALPHA_RATE radians a frame of the stopwatch
#define AIMING_ALPHA_RATE 0.20933335f
#define AIMING_ALPHA_SWING 32.0f
#define AIMING_MEAN_ALPHA 64.0f

// Where the current turn's shot comes from, as f_ShotSource records
enum ShotSource {
    // The player's controller
    SHOT_SOURCE_PLAYER,
    // g_ScriptedShot: a replayed shot, or the computer golfer's finished plan
    SHOT_SOURCE_PREPARED,
    // The computer golfer, which is still planning
    SHOT_SOURCE_PLANNING
};

typedef s32 ShotSource;

// Set once the shot preview has been built
static Boolean f_IsShotPreviewBuilt = FALSE;
// Frames that a replayed shot's aim has been settled
static s32 f_ReplayAimSettledFrameCount = 0;
// A debug tilt of the aim camera, which the C buttons tune
static f32 f_DebugAimCameraTilt = 0.0f;
static s32 f_UnusedWordAfterAimTilt UNUSED = 0;
// Frames that the D-pad has turned the aim, negative to the right
static s32 f_AimTurnFrameCount = 0;
static s32 f_UnusedWordAfterAimTurn UNUSED = 0;
static u32 f_PlanTimeBudget = INITIAL_PLAN_TIME_BUDGET;
// How many times the shot's flight has been shown; StartFlightCamera gets the
// earlier count
static s32 f_ShotFlightCount = 0;
// A dash of the shot preview
static Vtx f_ShotPreviewDashVertices[QUAD_VERTEX_COUNT] = {
    {{{-1, 0, 0}, 0, {0, 0}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{1, 0, 0}, 0, {0, 0x800}, {0x00, 0x00, 0x00, 0xFF}}},
    {{{1, 0, -1}, 0, {0x800, 0x800}, {0x00, 0x00, 0xFF, 0x00}}},
    {{{-1, 0, -1}, 0, {0x800, 0}, {0xFF, 0x00, 0x00, 0x00}}},
};
// How many steps the shot preview simulates
static s32 f_ShotPreviewStepCount = INITIAL_SHOT_PREVIEW_STEP_COUNT;
// How many of the shot preview's points are shown
static s32 f_ShownShotPreviewPointCount = INITIAL_SHOT_PREVIEW_STEP_COUNT;
// Whether the shot preview ran to its end
static Boolean8 f_HasShotPreviewEnded = FALSE;
// Scrolls the shot preview's dots
static s32 f_ShotPreviewDotScroll = 0;
// The putt preview's line, placed on screen
static Vtx f_PuttPreviewLineVertices[QUAD_VERTEX_COUNT] = {
    {{{0, 0, -100}, 0, {0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{0, -1, -100}, 0, {0, 0x7C0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{48, -1, -100}, 0, {0x17C0, 0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
    {{{48, 0, -100}, 0, {0x17C0, 0x7C0}, {0xFF, 0xFF, 0xFF, 0xFF}}},
};
// The ambient sounds that each course may play, or NO_AMBIENT_SOUND
static s32 f_AmbientSounds[COURSE_RECORD_COUNT][AMBIENT_SOUND_CHOICES] = {
    {0x16, 0x18, 0x19, 0x16, 0x18, 0x19, NO_AMBIENT_SOUND},
    {0x16, 0x18, 0x19, 0x16, 0x18, 0x19, NO_AMBIENT_SOUND},
    {
        NO_AMBIENT_SOUND,
        NO_AMBIENT_SOUND,
        NO_AMBIENT_SOUND,
        NO_AMBIENT_SOUND,
        NO_AMBIENT_SOUND,
        NO_AMBIENT_SOUND,
        NO_AMBIENT_SOUND,
    },
    {0x1E, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8},
    {
        NO_AMBIENT_SOUND,
        NO_AMBIENT_SOUND,
        NO_AMBIENT_SOUND,
        NO_AMBIENT_SOUND,
        NO_AMBIENT_SOUND,
        NO_AMBIENT_SOUND,
        NO_AMBIENT_SOUND,
    },
    {0x1A, 0x1B, 0x2B, 0x18, 0x1A, 0x1B, 0x2B},
    {0x16, 0x18, 0x19, 0x16, 0x18, 0x19, NO_AMBIENT_SOUND},
    {0x16, 0x18, 0x19, 0x16, 0x18, 0x19, NO_AMBIENT_SOUND},
    {0x16, 0x18, 0x19, 0x16, 0x18, 0x19, NO_AMBIENT_SOUND},
};
// Frames since the last ambient sound
static s32 f_AmbientSoundIdleFrameCount = 0;
// No routine sets it; while it is set, the next turn replays the player's
// saved shot
static Boolean8 f_ShouldReplaySavedShot = FALSE;
// Read by no routine; three objects, as an array is laid out differently
static s8 f_UnusedFirstByteAfterReplayFlag UNUSED = 0;
static s8 f_UnusedSecondByteAfterReplayFlag UNUSED = 0;
static s8 f_UnusedThirdByteAfterReplayFlag UNUSED = 0;
// The debug guts test's state, character and motion
static Boolean8 f_HasReactionTestRun = FALSE;
static CharacterIndex8 f_ReactionTestCharacter = 0;
static ReactionAnimation8 f_ReactionTestAnimation = 0;
static Boolean8 f_ShouldUpdateReaction = FALSE;
// The swings since aiming: the shot's, then one for each replay of it
static s32 f_SwingCount = 0;
// Whether B was held for a replay of the shot
static Boolean f_IsReplayRequested = FALSE;
// The f_StateFrameCounts count at which the state acts: a swing's impact, or
// a reaction's end
static s32 f_StateEventFrame = 0;
// Whether the swing animation has started while the impact is set
static Boolean f_IsSwingAnimationStarted = FALSE;
// How many frames before the impact a short putt's swing slows to
// f_PuttSwingSpeed, or 0
static s32 f_PuttSlowdownLead = 0;
// The frames by which the slowed swing puts off the impact
static s32 f_ImpactDelay = 0;
static s32 f_UnusedWordAfterImpactDelay UNUSED = 0;
// The angles of FrameBallAndShot's camera
static Radians f_DebugFramingYaw = 0.0f;
static Radians f_DebugFramingPitch = 0.0f;
static s32 f_UnusedWordAfterFramingAngles UNUSED = 0;

void PlaceShotCamera(void);
void TurnAim(Boolean turnsLookBackHeading);

static Mtx f_PuttPreviewIdentity; // An identity matrix for the putt preview
// The launch of the shot being taken, which the shot preview and the ball
// simulation start from. impactTiming is how far from the meter's impact mark
// the swing was stopped, and clubIndex and shotMode are also the club slot and
// shot mode that the golfer was last posed with.
static ShotLaunch f_ShotLaunch;
// Each player's frames since the start of a state or step of one
static s32 f_StateFrameCounts[PLAYER_COUNT_MAX];
// The speed of a short putt's slowed swing animation
static f32 f_PuttSwingSpeed;
// The song's tempo while aiming on the green, which rises the longer the
// player aims
static f32 f_GreenSongTempo;
// The course and hole that the debug ball placing last ran on; read by no
// routine
static CourseIndex f_UnusedDebugCourseIndex;
static s32 f_UnusedDebugHoleIndex;
// The frames that replays after the first skip of the swing animation, found on
// the second, so that the impact comes at most SKIPPED_SWING_IMPACT_FRAME
// frames in
static s32 f_SkippedSwingFrames;
static ShotSource f_ShotSource; // A SHOT_SOURCE_* value
static s32 f_TurnTimerStart; // Nothing reads it
// The shot preview's matrices, by draw buffer
static Mtx f_ShotPreviewMatrices[DRAW_BUFFER_COUNT][SHOT_PREVIEW_STEPS_MAX];
// The shot preview's points
static f32 f_ShotPreviewPoints[SHOT_PREVIEW_STEPS_MAX][AXIS_COUNT];

// Not static: the original left it to the linker's common block
s32 f_AimFrameCount;

// Lets the Rumble Paks run again after nuContRmbForceStop
void EndRumbleForceStop(void) {
    nuSiSendMesg(NU_CONT_RMB_FORCESTOPEND_MSG, NULL);
}

// Pulls the aim camera in along the heading, as far as it takes to keep it
// within the course. Once it has moved, a held C button pushes it on by a step.
void KeepAimCameraOnCourse(void) {
    Vec3 eye;
    s32 oldDistance = g_CurrentShot.aimCameraDistance;
    s32 coordinateMin;
    s32 xMax;
    s32 zMax;
    Radians heading;
    f32 offset;
    f32 overshoot;

    if (g_GameState.isMiniGolf) {
        xMax = AIM_CAMERA_MINI_GOLF_X_MAX;
        zMax = AIM_CAMERA_MINI_GOLF_Z_MAX;
    }
    else {
        xMax = AIM_CAMERA_COURSE_X_MAX;
        zMax = AIM_CAMERA_COURSE_Z_MAX;
    }

    heading = (u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS;
    overshoot = 0.0f;
    offset = cosf(heading) * g_CurrentShot.aimCameraDistance;
    eye[AXIS_X] = offset + g_CurrentShot.ballPosition[AXIS_X];
    coordinateMin = AIM_CAMERA_COORDINATE_MIN;
    if (eye[AXIS_X] < coordinateMin) {
        overshoot = coordinateMin - eye[AXIS_X];
    }
    else if (eye[AXIS_X] > xMax) {
        overshoot = eye[AXIS_X] - xMax;
    }

    if ((s32)offset != 0) {
        g_CurrentShot.aimCameraDistance *= (fabsf(offset) - overshoot) / fabsf(offset);
    }

    offset = sinf(heading) * g_CurrentShot.aimCameraDistance;
    eye[AXIS_Z] = offset + g_CurrentShot.ballPosition[AXIS_Z];
    overshoot = 0.0f;
    if (eye[AXIS_Z] < coordinateMin) {
        g_IsAimCameraEasing = FALSE;
        overshoot = coordinateMin - eye[AXIS_Z];
    }
    else if (eye[AXIS_Z] > zMax) {
        g_IsAimCameraEasing = FALSE;
        overshoot = eye[AXIS_Z] - zMax;
    }

    if ((s32)offset != 0) {
        g_CurrentShot.aimCameraDistance *= (fabsf(offset) - overshoot) / fabsf(offset);
    }

    if (g_GameState.isMiniGolf) {
        if (oldDistance < g_CurrentShot.aimCameraDistance) {
            if (g_TurnController.buttons & D_CBUTTONS) {
                g_CurrentShot.aimCameraDistance += AIM_CAMERA_MINI_GOLF_PUSH_STEP;
            }
        }
        else if (g_CurrentShot.aimCameraDistance < oldDistance &&
            (g_TurnController.buttons & U_CBUTTONS)) {
            g_CurrentShot.aimCameraDistance -= AIM_CAMERA_MINI_GOLF_PUSH_STEP;
        }
    }
    else {
        if (oldDistance < g_CurrentShot.aimCameraDistance) {
            if (g_TurnController.buttons & D_CBUTTONS) {
                g_CurrentShot.aimCameraDistance += AIM_CAMERA_PUSH_STEP;
            }
        }
        else if (g_CurrentShot.aimCameraDistance < oldDistance &&
            (g_TurnController.buttons & U_CBUTTONS)) {
            g_CurrentShot.aimCameraDistance -= AIM_CAMERA_PUSH_STEP;
        }
    }
}

// Places the camera for the shot being set up: circling the ball on the pause
// screen, behind the ball while aiming, or looking back at it from past the
// ball.
//
// Behind the ball, KeepAimCameraOnCourse is called in a block of its own. The
// original keeps g_CurrentShot's address in $s0 from the heading's read and
// reads aimCameraDistance through it for point's x and z, but reads the
// ball's position by its addresses. cse's first pass stops at the block's
// end and forgets the heading's register there: past it the distance's
// address gets a register of its own, and the ball's coordinates are read as
// offsets from that one. The second pass doesn't stop. It finds that the
// distance's address is g_CurrentShot's register plus 0x34 and reads the
// distance so, and it folds the ball's offsets back into addresses. Without
// the block, the first pass makes every read an offset from g_CurrentShot's
// register, the second folds them all into addresses, and the register is
// left with the heading's read alone.
//
// The camera's greatest rise above the ball is built up in maxRise: the
// distance in thousands, then its square, then that over the divisor. The
// original keeps all three in $v1. With the distance in thousands in a local
// of its own, it and its square trade $v0 and $v1.
//
// On the green, the look-at's height is worked out after its x and z; down
// range, the tilt in world units has a local of its own, set just before the
// look-at's height. On the mini golf course, point is set in axis order.
// Looking back at the ball, the look-at's height comes from the center's,
// which the original reloads from the stack.
//
// Measured against the match, as words differing and how many of them in
// shape:
//
//   KeepAimCameraOnCourse outside a block, or in plain braces 11 (3)
//   The first point's x in a block, in place of the call      7 (5)
//   The first point's x and z in one, in place of the call    25 (9)
//   The call before the heading is set, in a block or not     14 (6)
//   The first point's distance read through a pointer         14 (4)
//   The distance in thousands in a local, rise, with maxRise
//   set to its square over the divisor                        23 (3)
//   The same without the block (the old draft)                34 (6)
//   rise squared into maxRise, and maxRise divided in place   16 (0)
//   rise squared in place, and divided into maxRise           23 (3)
//   maxRise squared and divided in one statement              7 (3)
//   maxRise's square shifted right, not divided               7 (7)
//   maxRise as an f32                                         36 (22)
//   maxRise as an s16                                         29 (7)
//   maxRise as a u32                                          74 (32)
//   The ball's height added to maxRise before the clamp       4 (0)
//   The probes' loop testing < the count                      23 (9)
//   The eye's x and z stored before the first ground is read  45 (19)
//   On the green, the look-at's height before its z           35 (31)
//   The tilt scaled inside the look-at height's expression    13 (11)
//   The tilt in world units set before the look-at's x        21 (9)
//   Down range, point's y not set to the eye's                21 (1)
//   On the mini golf course, point's z set before its y       8 (0)
//   Looking back, the look-at's height from the ball's        3 (1)
//   Looking back, without the middle's stores                 55 (37)
//   Looking back, without point's first y                     10 (10)
//   The current player's address taken with &                 4 (0)
//
// Also matching: the heading's statement in the block in place of the call,
// or with it, or in a block of its own as well; the look-back's call in a
// block too; maxRise's steps spelled maxRise = maxRise * maxRise and maxRise
// = maxRise / AIM_CAMERA_RISE_DIVISOR; maxRise declared first in its branch,
// or in the function; and the probe's offset written out at each use, with
// no local.
void PlaceShotCamera(void) {
    Vec3 point;
    CoursePoint center;
    Radians heading;

    if (IsGameFlagSet(GAME_FLAG_TEST_AIM_TILT_AND_MOVIES)) {
        if (g_ControllerStates[TEST_CONTROLLER].buttons & L_CBUTTONS) {
            f_DebugAimCameraTilt -= DEBUG_AIM_TILT_STEP;
        }

        if (g_ControllerStates[TEST_CONTROLLER].buttons & R_CBUTTONS) {
            f_DebugAimCameraTilt += DEBUG_AIM_TILT_STEP;
        }
    }

    if (g_PauseScreen == PAUSE_SCREEN_FREE_LOOK) {
        g_Cameras[MAIN_CAMERA].mode = CAMERA_MODE_FREE_LOOK;
        point[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
        point[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
        point[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
        heading = (u16)g_FreeLookHeading * BINARY_ANGLE_TO_RADIANS;
        if (!g_GameState.isMiniGolf) {
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] =
                (point[AXIS_X] + (cosf(heading) * PAUSE_CAMERA_RADIUS)) * COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] =
                (point[AXIS_Z] - g_CurrentShot.aimCameraDistance +
                    (sinf(heading) * PAUSE_CAMERA_RADIUS)) *
                COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] =
                (point[AXIS_X] + (cosf(heading) * -PAUSE_CAMERA_RADIUS)) * COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] =
                (point[AXIS_Y] - PAUSE_CAMERA_RISE) * COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] =
                (point[AXIS_Z] - g_CurrentShot.aimCameraDistance +
                    (sinf(heading) * -PAUSE_CAMERA_RADIUS)) *
                COURSE_TO_WORLD_SCALE;
        }
        else {
            point[AXIS_X] = MINI_GOLF_CENTER_X;
            point[AXIS_Y] = g_HoleSetup.teePosition[AXIS_Y] +
                ((g_HoleSetup.cupPosition[AXIS_Y] - g_HoleSetup.teePosition[AXIS_Y]) * 0.5f);
            point[AXIS_Z] = MINI_GOLF_CENTER_Z;
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] =
                (point[AXIS_X] + (cosf(heading) * MINI_GOLF_PAUSE_CAMERA_RADIUS)) *
                COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] =
                (point[AXIS_Z] -
                    (g_CurrentShot.aimCameraDistance + MINI_GOLF_PAUSE_CAMERA_DISTANCE) +
                    (sinf(heading) * MINI_GOLF_PAUSE_CAMERA_RADIUS)) *
                COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] =
                (point[AXIS_X] + (cosf(heading) * -MINI_GOLF_PAUSE_CAMERA_RADIUS)) *
                COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] =
                (point[AXIS_Y] - MINI_GOLF_PAUSE_CAMERA_RISE) * COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] =
                (point[AXIS_Z] -
                    (g_CurrentShot.aimCameraDistance + MINI_GOLF_PAUSE_CAMERA_DISTANCE) +
                    (sinf(heading) * -MINI_GOLF_PAUSE_CAMERA_RADIUS)) *
                COURSE_TO_WORLD_SCALE;
        }
    }
    else if (g_LetterboxState == LETTERBOX_STATE_NONE ||
        g_LetterboxState == LETTERBOX_STATE_UNUSED_AIMING) {
        PlaceAimCamera();
    }
    else if (g_LetterboxState == LETTERBOX_STATE_MOVING_CAMERA ||
        g_LetterboxState == LETTERBOX_STATE_LOOKING_DOWN_RANGE) {
        s32 lowestGround;
        s32 ground;
        s32 probe;
        s32 offset;
        s32 riseMax;
        f32 tilt;
        // The tilt in world units
        f32 worldTilt;

        heading = (u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS;
        // In a block of its own, to match
        IN_OWN_BLOCK(KeepAimCameraOnCourse());
        point[AXIS_X] =
            (cosf(heading) * g_CurrentShot.aimCameraDistance) + g_CurrentShot.ballPosition[AXIS_X];
        point[AXIS_Z] =
            (sinf(heading) * g_CurrentShot.aimCameraDistance) + g_CurrentShot.ballPosition[AXIS_Z];
        if (point[AXIS_X] < AIM_CAMERA_COORDINATE_MIN) {
            point[AXIS_X] = AIM_CAMERA_COORDINATE_MIN;
        }
        else if (point[AXIS_X] > AIM_CAMERA_X_MAX) {
            point[AXIS_X] = AIM_CAMERA_X_MAX;
            g_IsAimCameraEasing = FALSE;
        }

        if (point[AXIS_Z] < AIM_CAMERA_COORDINATE_MIN) {
            point[AXIS_Z] = AIM_CAMERA_COORDINATE_MIN;
            g_IsAimCameraEasing = FALSE;
        }
        else if (point[AXIS_Z] > AIM_CAMERA_Z_MAX) {
            point[AXIS_Z] = AIM_CAMERA_Z_MAX;
            g_IsAimCameraEasing = FALSE;
        }

        lowestGround = GetAimCameraGround(point[AXIS_X], point[AXIS_Z]);
        g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
        g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        for (probe = 0; probe != AIM_GROUND_PROBE_COUNT; probe++) {
            offset = probe * AIM_GROUND_PROBE_STEP;
            point[AXIS_X] = (cosf(heading) * (g_CurrentShot.aimCameraDistance + offset)) +
                g_CurrentShot.ballPosition[AXIS_X];
            point[AXIS_Z] = (sinf(heading) * (g_CurrentShot.aimCameraDistance + offset)) +
                g_CurrentShot.ballPosition[AXIS_Z];
            ground = GetAimCameraGround(point[AXIS_X], point[AXIS_Z]);
            if (ground < lowestGround) {
                lowestGround = ground;
            }
        }

        if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
            point[AXIS_Y] = lowestGround - NO_IMPACT_POINT_DROP +
                (g_CurrentShot.aimCameraHeight * NO_IMPACT_POINT_HEIGHT_SCALE);
        }
        else {
            point[AXIS_Y] = lowestGround + g_CurrentShot.aimCameraHeight;
        }

        // Built up in place, to match
        riseMax = g_CurrentShot.aimCameraDistance / 1000;
        riseMax *= riseMax;
        riseMax /= AIM_CAMERA_RISE_DIVISOR;
        if (g_CurrentShot.ballPosition[AXIS_Y] + riseMax < point[AXIS_Y]) {
            point[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y] + riseMax;
        }

        g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        if (g_ShotFlags & SHOT_FLAG_ON_GREEN) {
            point[AXIS_X] = (cosf(heading) * (g_CurrentShot.aimCameraDistance + GREEN_LOOK_AHEAD)) +
                g_CurrentShot.ballPosition[AXIS_X];
            point[AXIS_Z] = (sinf(heading) * (g_CurrentShot.aimCameraDistance + GREEN_LOOK_AHEAD)) +
                g_CurrentShot.ballPosition[AXIS_Z];
            point[AXIS_Y] = (lowestGround - GREEN_LOOK_RISE) -
                (g_CurrentShot.aimCameraHeight * GREEN_LOOK_HEIGHT_SCALE);
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        }
        else {
            tilt = (AIM_TILT_FULL_DISTANCE -
                       GetClub(g_Players[g_CurrentShot.playerIndex].clubs[g_CurrentShot.clubSlot])
                           ->distance) *
                AIM_TILT_PER_YARD;
            if (tilt < 0.0f) {
                tilt = 0.0f;
            }

            point[AXIS_X] =
                (cosf(heading) * (g_CurrentShot.aimCameraDistance + DOWN_RANGE_LOOK_AHEAD)) +
                g_CurrentShot.ballPosition[AXIS_X];
            point[AXIS_Z] =
                (sinf(heading) * (g_CurrentShot.aimCameraDistance + DOWN_RANGE_LOOK_AHEAD)) +
                g_CurrentShot.ballPosition[AXIS_Z];
            point[AXIS_Y] = g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y];
            worldTilt = tilt * COURSE_TO_WORLD_SCALE;
            g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] =
                (g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] * COURSE_TO_WORLD_SCALE) -
                (g_CurrentShot.aimCameraHeight * worldTilt) +
                (f_DebugAimCameraTilt * AIM_CAMERA_DEBUG_TILT_SCALE);
        }

        g_Cameras[MAIN_CAMERA].view.heading = heading;
        g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
        g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    }
    else {
        KeepAimCameraOnCourse();

        // The middle of the ball and the cup, at the cup's height, is worked out,
        // but the ball is used. Setting the components in turn keeps flow from
        // deleting the middle's stores, which it does when each store directly
        // follows another to the same component.
        heading = ((u16)g_LookBackHeading * BINARY_ANGLE_TO_RADIANS) + M_PI;
        center[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X] +
            ((g_HoleSetup.cupPosition[AXIS_X] - g_CurrentShot.ballPosition[AXIS_X]) * 0.5f);
        center[AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y];
        center[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z] +
            ((g_HoleSetup.cupPosition[AXIS_Z] - g_CurrentShot.ballPosition[AXIS_Z]) * 0.5f);
        center[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
        center[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
        center[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
        point[AXIS_X] = (cosf(heading) * g_CurrentShot.aimCameraDistance) + center[AXIS_X];
        point[AXIS_Y] = g_CurrentShot.aimCameraHeight + center[AXIS_Y];
        point[AXIS_Z] = (sinf(heading) * g_CurrentShot.aimCameraDistance) + center[AXIS_Z];
        point[AXIS_Y] =
            (f32)GetAimCameraGround(point[AXIS_X], point[AXIS_Z]) + g_CurrentShot.aimCameraHeight;
        g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
        g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        point[AXIS_X] =
            (cosf(heading) * (g_CurrentShot.aimCameraDistance + GREEN_LOOK_AHEAD)) + center[AXIS_X];
        point[AXIS_Z] =
            (sinf(heading) * (g_CurrentShot.aimCameraDistance + GREEN_LOOK_AHEAD)) + center[AXIS_Z];
        point[AXIS_Y] = (center[AXIS_Y] - LOOK_BACK_RISE) -
            (g_CurrentShot.aimCameraHeight * LOOK_BACK_HEIGHT_SCALE);
        g_Cameras[MAIN_CAMERA].view.heading = heading;
        g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] = point[AXIS_X] * COURSE_TO_WORLD_SCALE;
        g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] = point[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] = point[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    }

    g_Cameras[MAIN_CAMERA].view.fieldOfView = SHOT_CAMERA_FIELD_OF_VIEW;
    OnShotCameraPlaced();
}

// Backs the aim camera up behind the ball, and points it down the shot's
// heading. The clamped point that it first works out goes unused, as does the
// ground height.
void AimDownRange(void) {
    Vec3 lookAt;
    Radians heading;

    GetSelectedClub();
    if (g_ShotFlags & SHOT_FLAG_ON_GREEN) {
        g_CurrentShot.aimCameraHeight = DOWN_RANGE_GREEN_OFFSET;
        g_CurrentShot.aimCameraDistance = DOWN_RANGE_GREEN_OFFSET;
    }
    else {
        g_CurrentShot.aimCameraHeight = DOWN_RANGE_HEIGHT;
        g_CurrentShot.aimCameraDistance = DOWN_RANGE_DISTANCE;
    }

    heading = (u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS;
    lookAt[AXIS_X] =
        (cosf(heading) * g_CurrentShot.aimCameraDistance) + g_CurrentShot.ballPosition[AXIS_X];
    if (lookAt[AXIS_X] < 0.0f) {
        lookAt[AXIS_X] = 0.0f;
    }
    else if (lookAt[AXIS_X] >= AIM_CAMERA_X_MAX) {
        lookAt[AXIS_X] = AIM_CAMERA_X_MAX - 1.0f;
    }

    lookAt[AXIS_Z] =
        (sinf(heading) * g_CurrentShot.aimCameraDistance) + g_CurrentShot.ballPosition[AXIS_Z];
    if (lookAt[AXIS_Z] < 0.0f) {
        lookAt[AXIS_Z] = 0.0f;
    }
    else if (lookAt[AXIS_Z] >= AIM_CAMERA_Z_MAX) {
        lookAt[AXIS_Z] = AIM_CAMERA_Z_MAX - 1.0f;
    }

    lookAt[AXIS_X] = (cosf(heading) * (g_CurrentShot.aimCameraDistance + DOWN_RANGE_LOOK_AHEAD)) +
        g_CurrentShot.ballPosition[AXIS_X];
    lookAt[AXIS_Z] = (sinf(heading) * (g_CurrentShot.aimCameraDistance + DOWN_RANGE_LOOK_AHEAD)) +
        g_CurrentShot.ballPosition[AXIS_Z];
    lookAt[AXIS_Y] = g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y];
    GetGroundHeight(lookAt[AXIS_X], lookAt[AXIS_Z]);
    g_Cameras[MAIN_CAMERA].view.heading = heading;
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] = lookAt[AXIS_X] * COURSE_TO_WORLD_SCALE;
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] = lookAt[AXIS_Z] * COURSE_TO_WORLD_SCALE;
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] = (lookAt[AXIS_Y] * COURSE_TO_WORLD_SCALE) -
        (g_CurrentShot.aimCameraHeight * COURSE_TO_WORLD_SCALE);
}

// Places the aim camera on the aim line, looking at the ball
void PlaceAimCamera(void) {
    Vec3 eye;
    f32 scale;
    Radians heading = (u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS;

    eye[AXIS_X] =
        (cosf(heading) * g_CurrentShot.aimCameraDistance) + g_CurrentShot.ballPosition[AXIS_X];
    if (eye[AXIS_X] < 0.0f) {
        eye[AXIS_X] = 0.0f;
    }
    else if (eye[AXIS_X] >= AIM_CAMERA_X_MAX) {
        eye[AXIS_X] = AIM_CAMERA_X_MAX - 1.0f;
    }

    eye[AXIS_Z] =
        (sinf(heading) * g_CurrentShot.aimCameraDistance) + g_CurrentShot.ballPosition[AXIS_Z];
    if (eye[AXIS_Z] < 0.0f) {
        eye[AXIS_Z] = 0.0f;
    }
    else if (eye[AXIS_Z] >= AIM_CAMERA_Z_MAX) {
        eye[AXIS_Z] = AIM_CAMERA_Z_MAX - 1.0f;
    }

    eye[AXIS_Y] = GetGroundHeight(eye[AXIS_X], eye[AXIS_Z]);
    if (g_CurrentShot.ballPosition[AXIS_Y] < eye[AXIS_Y]) {
        eye[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
    }

    // Holding the scale in a variable, and taking the camera's address for the
    // first store, both keep the original's register choices and load order
    scale = COURSE_TO_WORLD_SCALE;
    (&g_Cameras[MAIN_CAMERA])->view.lookAt[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
    eye[AXIS_X] *= scale;
    eye[AXIS_Y] = (eye[AXIS_Y] + g_CurrentShot.aimCameraHeight) * scale;
    eye[AXIS_Z] *= scale;
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] = eye[AXIS_X];
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] = eye[AXIS_Y];
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] = eye[AXIS_Z];
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] = (GetAimCameraGround(
                                                      g_CurrentShot.ballPosition[AXIS_X],
                                                      g_CurrentShot.ballPosition[AXIS_Z]
                                                  ) * scale) -
        AIM_LOOK_AT_RISE;
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] *= scale;
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] *= scale;
}

// Switches from the pause menu to the free-look camera, facing along the aim
// line
void StartFreeLook(void) {
    g_CurrentShot.aimCameraDistance = FREE_LOOK_START_DISTANCE;
    g_LetterboxState = LETTERBOX_STATE_MOVING_CAMERA;
    g_IsAimCameraEasing = FALSE;
    g_ImpactPointState = IMPACT_POINT_OFF;
    g_RoundHud.flags = HUD_FLAG_AIMING | HUD_FLAG_LIGHT_CLUB_HEAD | HUD_FLAG_NO_IMPACT_POINT |
        HUD_FLAG_TRANSLUCENT_GAUGE;
    g_FreeLookHeading = g_CurrentShot.heading;
    SetMenuBackdrop(MENU_BACKDROP_PATTERN);
    g_PauseScreen = PAUSE_SCREEN_FREE_LOOK;
    g_RoundHud.isLetterboxShown = FALSE;
    g_RoundHud.isClubPanelShown = FALSE;
    if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
        SetFramebufferCount(TRIPLE_BUFFERING);
        SetScreenEffect(SCREEN_EFFECT_NONE, 0);
    }

    StopSound(IMPACT_POINT_SOUND_CHANNEL);
}

void UpdateImpactPointMode(void) {
    if ((g_TurnController.buttons & IMPACT_POINT_BUTTONS) &&
        !(g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT)) {
        if (g_ImpactPointState == IMPACT_POINT_OFF && f_ShotSource == SHOT_SOURCE_PLAYER) {
            PlaySoundWithEffects(
                SOUND_IMPACT_POINT,
                IMPACT_POINT_SOUND_CHANNEL,
                IMPACT_POINT_SOUND_VOLUME,
                IMPACT_POINT_SOUND_REVERB,
                IMPACT_POINT_SOUND_PAN
            );
        }

        g_ImpactPointState = IMPACT_POINT_SETTING;
    }
    else {
        StopSound(IMPACT_POINT_SOUND_CHANNEL);
        g_ImpactPointState = IMPACT_POINT_OFF;
    }
}

void ResetAimCamera(void) {
    // Pads the frame to match; without it 8 words differ
    s32 unused[4] UNUSED;

    g_CurrentShot.aimCameraDistance = AIM_CAMERA_START_DISTANCE;
    g_CurrentShot.aimCameraHeight = AIM_CAMERA_START_HEIGHT;
    g_IsAimCameraEasing = FALSE;
    g_ImpactPointLingerFrameCount = 0;
    PlaceAimCamera();
}

// Eases the aim camera towards the place that suits the shot, and stops once it
// is close.
void EaseAimCamera(void) {
    s32 targetDistance;
    s32 targetHeight;

    if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
        targetDistance = AIM_CAMERA_PUTT_DISTANCE;
    }
    else if (g_ShotFlags & SHOT_FLAG_APPROACH) {
        targetDistance = g_CurrentShot.cupDistance - AIM_CAMERA_APPROACH_BACKOFF;
    }
    else {
        targetDistance =
            (GetSelectedShotDistance() - AIM_CAMERA_SHOT_BACKOFF_YARDS) * UNITS_PER_YARD;
    }

    if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
        targetHeight = AIM_CAMERA_PUTT_HEIGHT;
    }
    else if (g_ShotFlags & SHOT_FLAG_APPROACH) {
        targetHeight = AIM_CAMERA_APPROACH_HEIGHT;
    }
    else {
        targetHeight = AIM_CAMERA_SHOT_HEIGHT;
    }

    if (fabsf(g_CurrentShot.aimCameraDistance - targetDistance) > AIM_CAMERA_EASE_STEP_MAX) {
        if (g_CurrentShot.aimCameraDistance > targetDistance) {
            g_CurrentShot.aimCameraDistance -= AIM_CAMERA_EASE_STEP_MAX;
        }
        else {
            g_CurrentShot.aimCameraDistance += AIM_CAMERA_EASE_STEP_MAX;
        }
    }
    else {
        g_CurrentShot.aimCameraDistance +=
            (targetDistance - g_CurrentShot.aimCameraDistance) * AIM_CAMERA_EASE_FRACTION;
    }

    g_CurrentShot.aimCameraHeight +=
        (targetHeight - g_CurrentShot.aimCameraHeight) * AIM_CAMERA_EASE_FRACTION;
    if (fabsf(g_CurrentShot.aimCameraDistance - targetDistance) < UNITS_PER_YARD) {
        g_IsAimCameraEasing = FALSE;
    }
}

// Raises or lowers the aim camera with the C buttons, in smaller steps for a
// putt
void MoveAimCameraHeightByButtons(void) {
    if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
        if (g_TurnController.buttons & D_CBUTTONS) {
            g_CurrentShot.aimCameraHeight += AIM_CAMERA_BUTTON_PUTT_STEP;
        }
        else if (g_TurnController.buttons & U_CBUTTONS) {
            g_CurrentShot.aimCameraHeight -= AIM_CAMERA_BUTTON_PUTT_STEP;
        }
    }
    else {
        if (g_TurnController.buttons & D_CBUTTONS) {
            g_CurrentShot.aimCameraHeight += AIM_CAMERA_BUTTON_HEIGHT_STEP;
        }
        else if (g_TurnController.buttons & U_CBUTTONS) {
            g_CurrentShot.aimCameraHeight -= AIM_CAMERA_BUTTON_HEIGHT_STEP;
        }
    }
}

// Turns the free-look camera with C-left and C-right
void TurnFreeLook(void) {
    if (g_TurnController.buttons & L_CBUTTONS) {
        g_FreeLookHeading -= FREE_LOOK_TURN_STEP;
    }
    else if (g_TurnController.buttons & R_CBUTTONS) {
        g_FreeLookHeading += FREE_LOOK_TURN_STEP;
    }
}

// Swings the aim camera up and back, or down and forward, with C-right and
// C-left
void CraneAimCamera(void) {
    s32 stepCount;

    if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
        if (g_CurrentShot.shotMode == PUTT_MODE_SHORT) {
            stepCount = SHORT_PUTT_CRANE_STEP_COUNT;
        }
        else if (g_CurrentShot.shotMode == PUTT_MODE_MIDDLE) {
            stepCount = MIDDLE_PUTT_CRANE_STEP_COUNT;
        }
        else {
            stepCount = LONG_PUTT_CRANE_STEP_COUNT;
        }

        if (g_TurnController.buttons & L_CBUTTONS) {
            g_CurrentShot.aimCameraHeight += stepCount * AIM_CAMERA_PUTT_STEP_UNIT;
        }
        else if (g_TurnController.buttons & R_CBUTTONS) {
            g_CurrentShot.aimCameraHeight -= stepCount * AIM_CAMERA_PUTT_STEP_UNIT;
        }
    }
    else if (g_TurnController.buttons & L_CBUTTONS) {
        g_CurrentShot.aimCameraHeight += AIM_CAMERA_BUTTON_HEIGHT_STEP;
        if (g_CurrentShot.aimCameraHeight < AIM_CAMERA_CRANE_FORWARD_HEIGHT) {
            g_CurrentShot.aimCameraDistance += AIM_CAMERA_BUTTON_HEIGHT_STEP;
        }
    }
    else if (g_TurnController.buttons & R_CBUTTONS) {
        g_CurrentShot.aimCameraHeight -= AIM_CAMERA_BUTTON_HEIGHT_STEP;
        if (g_CurrentShot.aimCameraHeight > AIM_CAMERA_CRANE_BACK_HEIGHT) {
            g_CurrentShot.aimCameraDistance -= AIM_CAMERA_BUTTON_HEIGHT_STEP;
        }
    }
}

// Moves the aim camera along the aim line with the C buttons, in smaller steps
// for a putt
void MoveAimCameraDistanceByButtons(void) {
    if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
        if (g_TurnController.buttons & R_CBUTTONS) {
            g_CurrentShot.aimCameraDistance += AIM_CAMERA_BUTTON_PUTT_STEP;
        }
        else if (g_TurnController.buttons & L_CBUTTONS) {
            g_CurrentShot.aimCameraDistance -= AIM_CAMERA_BUTTON_PUTT_STEP;
        }
    }
    else {
        if (g_TurnController.buttons & R_CBUTTONS) {
            g_CurrentShot.aimCameraDistance += AIM_CAMERA_BUTTON_DISTANCE_STEP;
        }
        else if (g_TurnController.buttons & L_CBUTTONS) {
            g_CurrentShot.aimCameraDistance -= AIM_CAMERA_BUTTON_DISTANCE_STEP;
        }
    }
}

// Moves the aim camera along the aim line with C-up and C-down, by steps that
// suit a putt's length
void MoveAimCameraDistanceByUpDown(void) {
    s32 stepCount;

    if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
        if (g_CurrentShot.shotMode == PUTT_MODE_SHORT) {
            stepCount = SHORT_PUTT_DOLLY_STEP_COUNT;
        }
        else if (g_CurrentShot.shotMode == PUTT_MODE_MIDDLE) {
            stepCount = MIDDLE_PUTT_DOLLY_STEP_COUNT;
        }
        else {
            stepCount = LONG_PUTT_DOLLY_STEP_COUNT;
        }

        if (g_TurnController.buttons & U_CBUTTONS) {
            g_CurrentShot.aimCameraDistance += stepCount * AIM_CAMERA_PUTT_STEP_UNIT;
        }
        else if (g_TurnController.buttons & D_CBUTTONS) {
            g_CurrentShot.aimCameraDistance -= stepCount * AIM_CAMERA_PUTT_STEP_UNIT;
        }
    }
    else {
        if (g_TurnController.buttons & U_CBUTTONS) {
            g_CurrentShot.aimCameraDistance += AIM_CAMERA_BUTTON_DISTANCE_STEP;
        }
        else if (g_TurnController.buttons & D_CBUTTONS) {
            g_CurrentShot.aimCameraDistance -= AIM_CAMERA_BUTTON_DISTANCE_STEP;
        }
    }
}

// Raises or lowers the aim camera with the stick's Y
void MoveAimCameraHeight(void) {
    if (Abs(g_TurnController.stickY) > AIM_STICK_DEAD_ZONE) {
        g_CurrentShot.aimCameraHeight -=
            g_TurnController.stickY / AIM_STICK_STEP * AIM_CAMERA_HEIGHT_STEP;
    }
}

// Moves the aim camera along the aim line with the stick's Y, in smaller steps
// for a putt
void MoveAimCameraDistance(void) {
    if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
        if (Abs(g_TurnController.stickY) > AIM_STICK_DEAD_ZONE) {
            g_CurrentShot.aimCameraDistance +=
                g_TurnController.stickY / AIM_DISTANCE_STICK_STEP * AIM_CAMERA_PUTT_DISTANCE_STEP;
        }
    }
    else {
        if (Abs(g_TurnController.stickY) > AIM_STICK_DEAD_ZONE) {
            g_CurrentShot.aimCameraDistance +=
                g_TurnController.stickY / AIM_DISTANCE_STICK_STEP * AIM_CAMERA_DISTANCE_STEP;
        }
    }
}

// Turns the aim with the stick, or g_AltHeading instead when asked, and the
// shot's heading with the D-pad, faster the longer it is held. While the
// heading holds still, the scene is drawn from a saved frame, through two
// framebuffers.
//
// Each stick branch takes its turn from the heading in 32 bits: the heading
// before the turn is copied to an s32, after the stick's half is worked out and
// before the speed. The original sign-extends the saved heading there, from $s1
// into $v1, and loads g_AltHeading with lh while the speed is still being
// divided. With -= on the heading itself, the difference is narrowed to 16 bits
// and needs no extension: the aim's turn is subtracted from $s1 as it was
// loaded, and g_AltHeading is loaded after the speed's absolute value is taken.
//
// The aim's branch also stores the new heading through a pointer, from a
// variable of its own. The original copies g_CurrentShot's address from $a2,
// through which the heading was saved, to $a3, and stores the heading through
// $a3 once, after both arms of the speed's test. Without the pointer the
// heading is saved and stored by its address, with no register for it. Without
// the variable the pointer is the address's own register, with no copy, and
// each arm of the speed's test ends in a store of its own.
//
// The heading is saved as a SignedBinaryAngle, where the stick is read: the
// original loads it with lhu into $s1 and sign-extends it at each use, for the
// aim's turn and for the test of whether it held still.
//
// Each D-pad branch counts through a pointer to f_AimTurnFrameCount of its own,
// which the original keeps in $v1.
//
// Measured against the match, as words differing and how many of them in shape:
//   The aim's turn as -= on g_CurrentShot.heading, with the
//   step multiplied by the speed under an if                  33 (5)
//   The same, with g_AltHeading's turn as -= too (the old
//   draft)                                                    39 (9)
//   The aim's turn as -= of a conditional expression          29 (11)
//   No shotHeading, newHeading stored to
//   g_CurrentShot.heading                                     24 (12)
//   No newHeading, the difference stored through shotHeading,
//   or to g_CurrentShot.heading                               9 (3)
//   oldHeading turned in place and stored                     35 (7)
//   No oldHeading, the turn taken from heading                22 (12)
//   oldHeading as a SignedBinaryAngle                         22 (12)
//   oldHeading declared before step                           22 (6)
//   oldHeading declared after speed                           22 (12)
//   speed declared before step                                41 (15)
//   shotHeading declared after speed                          20 (10)
//   The step multiplied by the speed under an if, then taken
//   from oldHeading                                           37 (5)
//   The step as stickX >> 1                                   4 (2)
//   An inline function for the step times the speed           33 (5)
//   g_AltHeading's turn as -=, or taken from g_AltHeading
//   with no oldHeading                                        6 (4)
//   g_AltHeading's turn stored through a pointer              8 (2)
//   g_AltHeading's oldHeading as a SignedBinaryAngle          23 (15)
//   g_AltHeading's step multiplied by the speed under an if   7 (3)
//   g_AltHeading's speed declared before its step             12 (12)
//   The test as turnsAltHeading, with its branches swapped    54 (42)
//   The saved heading as a BinaryAngle                        31 (11)
//   The saved heading as an s32                               45 (13)
//   stickX as an s8                                           91 (21)
//   stickX as an s16                                          89 (23)
//   The heading saved before the stick is read                21 (5)
//   Both read after g_ImpactPointState's test                 26 (12)
//   f_AimTurnFrameCount named in both D-pad branches          41 (19)
//   f_AimTurnFrameCount named in one D-pad branch             20 (8)
//   One pointer to f_AimTurnFrameCount for the function       40 (8)
//
// Also matching: newHeading declared and then set, set in an if and an else, or
// as a conditional expression that subtracts in each arm; newHeading as a
// SignedBinaryAngle; oldHeading read through shotHeading, or from
// g_CurrentShot.heading; shotHeading as a pointer to g_CurrentShot; the aim's
// step as an s16, or read from g_TurnController.stickX; either speed's absolute
// value taken with an if; either speed tested == 0, with the arms swapped;
// g_AltHeading's turn through a newHeading too, or with its oldHeading declared
// before its step; the frame count copied to a local for the D-pad's turn; the
// D-pad's speed as a conditional expression; and the shot's heading cast to s16
// in the test of whether it held still.
void TurnAim(Boolean turnsLookBackHeading) {
    s32 stickX = g_TurnController.stickX;
    SignedBinaryAngle heading = g_CurrentShot.heading;
    s32 dpad;
    s32 dpadSpeed;

    if (g_ImpactPointState != IMPACT_POINT_OFF) {
        return;
    }

    if (!turnsLookBackHeading) {
        // Turned in 32 bits and stored through a pointer, to match
        SignedBinaryAngle *shotHeading = &g_CurrentShot.heading;
        s32 step = stickX / 2;
        WideBinaryAngle oldHeading = heading;
        s32 speed = Abs(stickX / AIM_STICK_SPEED_DIVISOR);
        WideBinaryAngle newHeading = oldHeading - ((speed != 0) ? step * speed : step);

        *shotHeading = newHeading;
    }
    else {
        // Turned in 32 bits as well, to match
        s32 step = stickX / 2;
        WideBinaryAngle oldHeading = g_LookBackHeading;
        s32 speed = Abs(stickX / LOOK_BACK_STICK_SPEED_DIVISOR);

        g_LookBackHeading = oldHeading - ((speed != 0) ? step * speed : step);
    }

    dpad = g_TurnController.buttons & (L_JPAD | R_JPAD);
    if (dpad == 0 || dpad == (L_JPAD | R_JPAD)) {
        f_AimTurnFrameCount = 0;
    }
    else if (dpad & L_JPAD) {
        // Counted through a pointer of this branch's own, to match
        s32 *dpadFrames = &f_AimTurnFrameCount;

        if (*dpadFrames < 0) {
            *dpadFrames = 0;
        }

        if (++*dpadFrames > AIM_DPAD_FRAMES_MAX) {
            *dpadFrames = AIM_DPAD_FRAMES_MAX;
        }
    }
    else {
        // As in the left branch, to match
        s32 *dpadFrames = &f_AimTurnFrameCount;

        if (*dpadFrames > 0) {
            *dpadFrames = 0;
        }

        if (--*dpadFrames < -AIM_DPAD_FRAMES_MAX) {
            *dpadFrames = -AIM_DPAD_FRAMES_MAX;
        }
    }

    dpadSpeed = Abs(f_AimTurnFrameCount);
    g_CurrentShot.heading +=
        f_AimTurnFrameCount * ((dpadSpeed / AIM_DPAD_SPEED_DIVISOR) + AIM_DPAD_BASE_SPEED);
    if (!IsGameFlagSet(GAME_FLAG_RANDOM_FRAMEBUFFER_COUNT)) {
        if (g_LetterboxState == LETTERBOX_STATE_NONE ||
            g_LetterboxState == LETTERBOX_STATE_UNUSED_AIMING) {
            if (heading == g_CurrentShot.heading) {
                if (g_ScreenEffect != SCREEN_EFFECT_SAVED_FRAME) {
                    SetScreenEffect(SCREEN_EFFECT_SAVED_FRAME, SAVED_FRAME_STEP_SAVING);
                    SetFramebufferCount(DOUBLE_BUFFERING);
                }
            }
            else if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
                SetFramebufferCount(TRIPLE_BUFFERING);
                SetScreenEffect(SCREEN_EFFECT_NONE, 0);
            }
        }
        else {
            SetFramebufferCount(TRIPLE_BUFFERING);
        }
    }

    if (IsGameFlagSet(GAME_FLAG_RANDOM_FRAMEBUFFER_COUNT)) {
        if ((rand() & 1) == 0) {
            SetFramebufferCount(DOUBLE_BUFFERING);
        }
        else {
            SetFramebufferCount(TRIPLE_BUFFERING);
        }
    }
}

// Clamps the aim camera's height between minHeight and AIM_CAMERA_MAX_HEIGHT,
// and its distance between minDistance and maxDistance. Each view clamps all
// four, as in the original, whose green and other views share their tails;
// clamping the common bounds once after the views gives 24 differing words.
#define ClampAimCamera(heightMin, distanceMin, distanceMax) \
    { \
        if (g_CurrentShot.aimCameraHeight > (heightMin)) { \
            g_CurrentShot.aimCameraHeight = (heightMin); \
        } \
        if (g_CurrentShot.aimCameraHeight < AIM_CAMERA_HEIGHT_MAX) { \
            g_CurrentShot.aimCameraHeight = AIM_CAMERA_HEIGHT_MAX; \
        } \
        if (g_CurrentShot.aimCameraDistance < (distanceMin)) { \
            g_CurrentShot.aimCameraDistance = (distanceMin); \
        } \
        if (g_CurrentShot.aimCameraDistance > (distanceMax)) { \
            g_CurrentShot.aimCameraDistance = (distanceMax); \
        } \
    }

// Handles the aim's buttons for a frame: switching between looking down the
// range, at the cup and back from past the ball, and moving the camera. A
// replay instead turns the aim to the recorded heading, and returns TRUE once
// it has settled there for a while.
Boolean UpdateAimControls(void) {
    Vec3 toCup;
    WideBinaryAngle headingLeft;
    WideBinaryAngle step;
    WideBinaryAngle turn;
    WideBinaryAngle heading;

    if (f_ShotSource != SHOT_SOURCE_PLAYER) {
        headingLeft = (s16)(g_ScriptedShot.heading - g_CurrentShot.heading);
        if (IsGameFlagSet(GAME_FLAG_SNAP_REPLAY_AIM) || Abs(headingLeft) < REPLAY_AIM_STEP_MIN) {
            g_CurrentShot.heading = g_ScriptedShot.heading;
            if (g_ScreenEffect != SCREEN_EFFECT_SAVED_FRAME) {
                SetScreenEffect(SCREEN_EFFECT_SAVED_FRAME, SAVED_FRAME_STEP_SAVING);
                SetFramebufferCount(DOUBLE_BUFFERING);
            }

            return ++f_ReplayAimSettledFrameCount >= REPLAY_AIM_SETTLE_FRAMES;
        }

        step = headingLeft / REPLAY_AIM_STEP_DIVISOR;
        turn = Abs(step) + REPLAY_AIM_STEP_MIN;
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
        g_CurrentShot.heading =
            atan2(
                g_HoleSetup.cupPosition[AXIS_Z] - g_CurrentShot.ballPosition[AXIS_Z],
                g_HoleSetup.cupPosition[AXIS_X] - g_CurrentShot.ballPosition[AXIS_X]
            ) *
            RADIANS_TO_BINARY_ANGLE;
    }

    if (IsGameFlagSet(GAME_FLAG_AIM_DOWN_RANGE)) {
        switch (g_LetterboxState) {
            case LETTERBOX_STATE_NONE:
                if ((g_TurnController.pressedButtons & A_BUTTON) && IsShotModeChangeDone()) {
                    g_HasPressedShotButton = TRUE;
                    ResetAimCamera();
                }
                else if (g_TurnController.pressedButtons & R_TRIG) {
                    PlaySound(SOUND_LOOK_DOWN_RANGE, g_NextSoundChannel, LOOK_SOUND_VOLUME);
                    if (g_ShotFlags & SHOT_FLAG_NEAR_CUP) {
                        g_LetterboxState = LETTERBOX_STATE_STARTING_LOOK_BACK;
                        toCup[AXIS_X] =
                            g_HoleSetup.cupPosition[AXIS_X] - g_CurrentShot.ballPosition[AXIS_X];
                        toCup[AXIS_Z] =
                            g_HoleSetup.cupPosition[AXIS_Z] - g_CurrentShot.ballPosition[AXIS_Z];
                        g_LookBackHeading =
                            (atan2(toCup[AXIS_Z], toCup[AXIS_X]) + M_PI) * RADIANS_TO_BINARY_ANGLE;
                        g_CurrentShot.aimCameraDistance =
                            -sqrtf(
                                (toCup[AXIS_X] * toCup[AXIS_X]) + (toCup[AXIS_Z] * toCup[AXIS_Z])
                            ) -
                            LOOK_BACK_EXTRA_DISTANCE;
                        g_CurrentShot.aimCameraHeight = LOOK_BACK_START_HEIGHT;
                    }
                    else {
                        g_LetterboxState = LETTERBOX_STATE_LOOKING_DOWN_RANGE;
                        g_IsAimCameraEasing = TRUE;
                    }
                }
                else if (g_TurnController.pressedButtons & B_BUTTON) {
                    AimDownRange();
                    g_LetterboxState = LETTERBOX_STATE_MOVING_CAMERA;
                }

                if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
                    TurnAim(FALSE);
                }

                break;
            case LETTERBOX_STATE_MOVING_CAMERA:
                if (g_TurnController.pressedButtons & A_BUTTON) {
                    g_LetterboxState = LETTERBOX_STATE_NONE;
                    ResetAimCamera();
                }
                else if (g_TurnController.pressedButtons & R_TRIG) {
                    g_LetterboxState = LETTERBOX_STATE_NONE;
                    ResetAimCamera();
                }
                else if (g_TurnController.pressedButtons & B_BUTTON) {
                    g_RoundHud.greenGridState++;
                }

                TurnAim(FALSE);
                break;
            case LETTERBOX_STATE_LOOKING_DOWN_RANGE:
                g_IsAimCameraEasing = TRUE;
                if (g_TurnController.pressedButtons & A_BUTTON) {
                    g_LetterboxState = LETTERBOX_STATE_NONE;
                    g_IsAimCameraEasing = FALSE;
                    ResetAimCamera();
                }
                else if (g_TurnController.pressedButtons & R_TRIG) {
                    PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
                    g_LetterboxState = LETTERBOX_STATE_NONE;
                    g_IsAimCameraEasing = FALSE;
                    ResetAimCamera();
                }
                else if (g_TurnController.pressedButtons & B_BUTTON) {
                    g_RoundHud.greenGridState++;
                }

                TurnAim(FALSE);
                break;
            case LETTERBOX_STATE_STARTING_LOOK_BACK:
            case LETTERBOX_STATE_LOOKING_BACK:
                if (g_TurnController.pressedButtons & A_BUTTON) {
                    g_LetterboxState = LETTERBOX_STATE_NONE;
                    ResetAimCamera();
                }
                else if (g_TurnController.pressedButtons & R_TRIG) {
                    PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
                    g_LetterboxState = LETTERBOX_STATE_NONE;
                    ResetAimCamera();
                }
                else if (g_TurnController.pressedButtons & B_BUTTON) {
                    g_RoundHud.greenGridState++;
                }

                TurnAim(FALSE);
                break;
        }

        if (g_LetterboxState == LETTERBOX_STATE_NONE) {
            g_ImpactPointState = IMPACT_POINT_SETTING;
        }
        else {
            g_ImpactPointState = IMPACT_POINT_OFF;
        }
    }
    else {
        if (f_ShotSource == SHOT_SOURCE_PLAYER) {
            if ((g_TurnController.buttons & IMPACT_POINT_BUTTONS) &&
                g_PauseScreen != PAUSE_SCREEN_FREE_LOOK &&
                !(g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT)) {
                if (g_ImpactPointState != IMPACT_POINT_SETTING) {
                    PlaySoundWithEffects(
                        SOUND_IMPACT_POINT,
                        IMPACT_POINT_SOUND_CHANNEL,
                        IMPACT_POINT_SOUND_VOLUME,
                        IMPACT_POINT_SOUND_REVERB,
                        IMPACT_POINT_SOUND_PAN
                    );
                }

                g_ImpactPointState = IMPACT_POINT_SETTING;
                g_ImpactPointLingerFrameCount = 0;
            }
            else {
                StopSound(IMPACT_POINT_SOUND_CHANNEL);
                if (g_ImpactPointState != IMPACT_POINT_OFF) {
                    g_ImpactPointState = IMPACT_POINT_LINGERING;
                }

                if (g_ImpactPointLingerFrameCount < IMPACT_POINT_LINGER_FRAMES) {
                    g_ImpactPointLingerFrameCount++;
                }
                else {
                    g_ImpactPointState = IMPACT_POINT_OFF;
                }
            }
        }

        switch (g_LetterboxState) {
            case LETTERBOX_STATE_NONE:
            case LETTERBOX_STATE_UNUSED_AIMING:
                if ((g_TurnController.pressedButtons & A_BUTTON) && IsShotModeChangeDone()) {
                    g_HasPressedShotButton = TRUE;
                    ResetAimCamera();
                }
                else if (g_TurnController.pressedButtons & R_TRIG) {
                    PlaySound(SOUND_LOOK_AT_CUP, g_NextSoundChannel, LOOK_SOUND_VOLUME);
                    if ((g_ShotFlags & (SHOT_FLAG_NEAR_CUP | SHOT_FLAG_NO_IMPACT_POINT)) ==
                        (SHOT_FLAG_NEAR_CUP | SHOT_FLAG_NO_IMPACT_POINT)) {
                        toCup[AXIS_X] =
                            g_HoleSetup.cupPosition[AXIS_X] - g_CurrentShot.ballPosition[AXIS_X];
                        toCup[AXIS_Z] =
                            g_HoleSetup.cupPosition[AXIS_Z] - g_CurrentShot.ballPosition[AXIS_Z];
                        g_LetterboxState = LETTERBOX_STATE_STARTING_LOOK_BACK;
                        g_CurrentShot.aimCameraHeight = LOOK_BACK_START_HEIGHT;
                        g_LookBackHeading = g_CurrentShot.heading;
                        g_CurrentShot.aimCameraDistance =
                            -sqrtf(
                                (toCup[AXIS_X] * toCup[AXIS_X]) + (toCup[AXIS_Z] * toCup[AXIS_Z])
                            ) -
                            LOOK_BACK_EXTRA_DISTANCE;
                    }
                    else {
                        g_LetterboxState = LETTERBOX_STATE_LOOKING_DOWN_RANGE;
                        g_IsAimCameraEasing = TRUE;
                    }
                }

                if (g_LetterboxState == LETTERBOX_STATE_NONE) {
                    TurnAim(FALSE);
                }

                break;
            case LETTERBOX_STATE_MOVING_CAMERA:
                if (g_TurnController.pressedButtons & A_BUTTON) {
                    g_LetterboxState = LETTERBOX_STATE_NONE;
                    PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
                    ResetAimCamera();
                    g_ShotSetupFrameCount = 0;
                }
                else if (g_TurnController.pressedButtons & R_TRIG) {
                    g_LetterboxState = LETTERBOX_STATE_NONE;
                    g_ShotSetupFrameCount = 0;
                    PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
                    ResetAimCamera();
                }

                TurnAim(FALSE);
                break;
            case LETTERBOX_STATE_LOOKING_DOWN_RANGE:
                g_IsAimCameraEasing = TRUE;
                if (g_TurnController.pressedButtons & A_BUTTON) {
                    g_LetterboxState = LETTERBOX_STATE_NONE;
                    g_ShotSetupFrameCount = 0;
                    g_IsAimCameraEasing = FALSE;
                    ResetAimCamera();
                    PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
                }
                else if (g_TurnController.pressedButtons & R_TRIG) {
                    PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
                    g_ShotSetupFrameCount = 0;
                    g_LetterboxState = LETTERBOX_STATE_NONE;
                    g_IsAimCameraEasing = FALSE;
                    ResetAimCamera();
                }

                TurnAim(FALSE);
                break;
            case LETTERBOX_STATE_STARTING_LOOK_BACK:
                if (g_TurnController.pressedButtons & A_BUTTON) {
                    g_LetterboxState = LETTERBOX_STATE_NONE;
                    g_ShotSetupFrameCount = 0;
                    ResetAimCamera();
                    PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
                }
                else if (g_TurnController.pressedButtons & R_TRIG) {
                    g_ShotSetupFrameCount = 0;
                    PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
                    g_LetterboxState = LETTERBOX_STATE_NONE;
                    ResetAimCamera();
                }

                TurnAim(FALSE);
                g_LookBackHeading = g_CurrentShot.heading;
                break;
            case LETTERBOX_STATE_LOOKING_BACK:
                if (g_TurnController.pressedButtons & A_BUTTON) {
                    g_LetterboxState = LETTERBOX_STATE_NONE;
                    g_ShotSetupFrameCount = 0;
                    ResetAimCamera();
                    PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
                }
                else if (g_TurnController.pressedButtons & R_TRIG) {
                    PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
                    g_LetterboxState = LETTERBOX_STATE_NONE;
                    ResetAimCamera();
                    g_ShotSetupFrameCount = 0;
                }

                TurnAim(FALSE);
                g_LookBackHeading = g_CurrentShot.heading;
                break;
        }
    }

    if (g_RoundHud.greenGridState >= AIM_VIEW_TOGGLE_COUNT) {
        g_RoundHud.greenGridState = GREEN_GRID_HIDDEN;
    }

    if (g_HasPressedShotButton != TRUE) {
        if (g_TurnController.buttons & AIM_CAMERA_BUTTONS) {
            if (g_LetterboxState == LETTERBOX_STATE_NONE ||
                g_LetterboxState == LETTERBOX_STATE_UNUSED_AIMING ||
                g_LetterboxState == LETTERBOX_STATE_LOOKING_DOWN_RANGE) {
                g_LetterboxState = LETTERBOX_STATE_MOVING_CAMERA;
                g_IsAimCameraEasing = FALSE;
            }

            if (g_LetterboxState == LETTERBOX_STATE_STARTING_LOOK_BACK) {
                g_LetterboxState = LETTERBOX_STATE_LOOKING_BACK;
                g_IsAimCameraEasing = FALSE;
            }
        }

        if (g_TurnController.pressedButtons & AIM_CAMERA_RAISE_BUTTONS) {
            PlaySound(SOUND_PROMPT_CLOSE, g_NextSoundChannel, AIM_CAMERA_SOUND_VOLUME);
        }

        if (g_TurnController.pressedButtons & AIM_CAMERA_LOWER_BUTTONS) {
            PlaySound(SOUND_PROMPT_OPEN, g_NextSoundChannel, AIM_CAMERA_SOUND_VOLUME);
        }

        if (g_IsAimCameraEasing == TRUE) {
            EaseAimCamera();
        }
        else {
            if (g_PauseScreen == PAUSE_SCREEN_FREE_LOOK) {
                TurnFreeLook();
            }
            else {
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
        ClampAimCamera(GREEN_AIM_CAMERA_HEIGHT_MIN, LOOK_BACK_DISTANCE_MIN, LOOK_BACK_DISTANCE_MAX);
    }
    else if (g_ShotFlags & SHOT_FLAG_ON_GREEN) {
        ClampAimCamera(
            GREEN_AIM_CAMERA_HEIGHT_MIN,
            AIM_CAMERA_DISTANCE_MIN,
            AIM_CAMERA_DISTANCE_MAX
        );
    }
    else {
        ClampAimCamera(AIM_CAMERA_HEIGHT_MIN, AIM_CAMERA_DISTANCE_MIN, AIM_CAMERA_DISTANCE_MAX);
    }

    g_Cameras[INSET_CAMERA].view.eye[AXIS_X] = g_Cameras[MAIN_CAMERA].view.eye[AXIS_X];
    g_Cameras[INSET_CAMERA].view.eye[AXIS_Y] = g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y];
    g_Cameras[INSET_CAMERA].view.eye[AXIS_Z] = g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z];
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] = g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X];
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] = g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y];
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] = g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z];
    return FALSE;
}

void UpdateShotFlags(void) {
    ClubSurfaceMask surfaceMask = GetSelectedClub()->surfaceMask;
    Boolean8 *isMiniGolf;

    g_ShotFlags = 0;
    if (surfaceMask & CLUB_SURFACE_GREEN) {
        g_ShotFlags = SHOT_FLAG_NO_IMPACT_POINT;
    }

    isMiniGolf = &g_GameState.isMiniGolf;

    if (*isMiniGolf) {
        g_ShotFlags |= SHOT_FLAG_NO_IMPACT_POINT;
    }

    if (GetSurfaceTraits(g_CurrentShot.surface)->kind == SURFACE_KIND_GREEN) {
        g_ShotFlags |= SHOT_FLAG_ON_GREEN;
    }

    if (*isMiniGolf) {
        g_ShotFlags |= SHOT_FLAG_ON_GREEN | SHOT_FLAG_NO_IMPACT_POINT;
    }

    if (g_CurrentShot.shotMode == SHOT_MODE_APPROACH) {
        g_ShotFlags |= SHOT_FLAG_APPROACH;
    }

    if (g_CurrentShot.cupDistance * DECIMETERS_PER_UNIT <= NEAR_CUP_DECIMETERS_MAX) {
        g_ShotFlags |= SHOT_FLAG_NEAR_CUP;
    }
}

void StartTurnTimer(void) {
    f_TurnTimerStart = osGetCount();
}

// Always FALSE, so UpdateTurns stops polling the computer golfer's plan
// after one try. It gets a time budget after StartTurnTimer, so it
// presumably once told whether the frame had time left for planning.
Boolean HasPlanTime(s32 timeBudget UNUSED) {
    return FALSE;
}

// Picks the club and the shot mode that the shot starts with. On a green, it is
// the putter, in the shortest putt mode that reaches the cup. Elsewhere, it is
// the club whose normal shot comes nearest the cup, out of those with a good
// enough lie, and preferring those that reach.
void ChooseStartingClub(HoleSetup *hole) {
    Boolean8 isUsable[CLUB_SLOT_COUNT];
    s32 clubCount;
    s32 cupDistance;
    ClubSlot clubSlot;
    LieRating rating;
    LieRating lieRating;
    Club *club;

    g_CurrentShot.clubSlot = 0;
    g_CurrentShot.shotMode = SHOT_MODE_NORMAL;
    clubCount = MarkUsableClubs(
        (s32)GetSurfaceTraits(g_CurrentShot.surface),
        g_Players[g_CurrentShot.playerIndex].clubs,
        isUsable
    );
    cupDistance = GetCupDistance(
        g_CurrentShot.ballPosition[AXIS_X] - hole->cupPosition[AXIS_X],
        g_CurrentShot.ballPosition[AXIS_Z] - hole->cupPosition[AXIS_Z]
    );
    if (g_GameState.isMiniGolf ||
        GetSurfaceTraits(g_CurrentShot.surface)->kind == SURFACE_KIND_GREEN) {
        ClubSlot putterSlot;

        for (putterSlot = 0; putterSlot < clubCount; putterSlot++) {
            if (!isUsable[putterSlot]) {
                continue;
            }

            club = GetClub(g_Players[g_CurrentShot.playerIndex].clubs[putterSlot]);
            if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
                continue;
            }

            if (GetShotWorldDistance(club, PUTT_MODE_SHORT) >= cupDistance) {
                g_CurrentShot.shotMode = PUTT_MODE_SHORT;
            }
            else if (g_CurrentShot.powerShotCount == 0 ||
                GetShotWorldDistance(club, PUTT_MODE_MIDDLE) >= cupDistance) {
                g_CurrentShot.shotMode = PUTT_MODE_MIDDLE;
            }
            else {
                g_CurrentShot.shotMode = PUTT_MODE_LONG;
            }

            g_CurrentShot.clubSlot = putterSlot;
            break;
        }
    }
    else {
        LieRating ratingMin = NO_LIE_RATING;
        s32 bestGap = S32_MAX;
        Boolean reaches = FALSE;
        ClubSlot bestSlot = 0;
        s32 distance;
        s32 gap;

        for (clubSlot = 0; clubSlot < clubCount; clubSlot++) {
            rating = GetLieRating(
                GetClub(g_Players[g_CurrentShot.playerIndex].clubs[clubSlot]),
                g_CurrentShot.surface,
                SHOT_MODE_NORMAL
            );
            if (ratingMin < rating) {
                ratingMin = rating;
            }

            if (rating >= STARTING_CLUB_LIE_RATING_MIN) {
                break;
            }
        }

        if (ratingMin > STARTING_CLUB_LIE_RATING_MIN) {
            ratingMin = STARTING_CLUB_LIE_RATING_MIN;
        }

        for (clubSlot = 0; clubSlot < clubCount; clubSlot++) {
            if (!isUsable[clubSlot]) {
                continue;
            }

            club = GetClub(g_Players[g_CurrentShot.playerIndex].clubs[clubSlot]);
            if (club->surfaceMask & CLUB_SURFACE_GREEN) {
                continue;
            }

            lieRating = GetLieRating(club, g_CurrentShot.surface, SHOT_MODE_NORMAL);
            distance = GetShotDistance(club, SHOT_MODE_NORMAL) * UNITS_PER_YARD;
            if (lieRating < ratingMin) {
                continue;
            }

            gap = distance - cupDistance;
            if (Abs(gap) < bestGap && (!reaches || distance >= cupDistance)) {
                bestSlot = clubSlot;
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

// Puts the simulated ball at rest on the player's ball, and shows the player's
// passed rings again
void PlaceBallAtRest(s32 playerIndex, BallSimulation *simulation) {
    Player *player = &g_Players[playerIndex];
    s32 ringIndex;
    s32 groundHeight;

    StopSound(IMPACT_POINT_SOUND_CHANNEL);
    for (ringIndex = 0; ringIndex < RING_COUNT; ringIndex++) {
        if (player->ringStates[ringIndex] != RING_STATE_NONE) {
            player->ringStates[ringIndex] = RING_STATE_SHOWN;
        }

        SetRingShotCleared(FALSE);
    }

    player->unusedRestValue = 0;
    simulation->position[AXIS_X] = player->ballPosition[AXIS_X];
    simulation->position[AXIS_Y] = player->ballPosition[AXIS_Y];
    simulation->position[AXIS_Z] = player->ballPosition[AXIS_Z];
    simulation->playerIndex = playerIndex;
    simulation->isHoledOut = FALSE;
    simulation->state = BALL_STATE_STOPPED;
    groundHeight = GetGroundHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
    if (groundHeight < simulation->position[AXIS_Y]) {
        simulation->position[AXIS_Y] = groundHeight;
    }

    simulation->surface = player->ballSurface;
    simulation->purpose = LAUNCH_PURPOSE_PLACED_AT_REST;
    simulation->hasEnteredWater = FALSE;
    PrepareShotEffects(simulation);
    if (g_IsModuleAlive[MODULE_GOLF_BALLS] || ReportModuleNotAlive(MODULE_GOLF_BALLS)) {
        ResetBallAfterimage();
    }
}

// Counts a notable shot towards the player's Course Points
void CountNotableShot(s32 playerIndex, NotableShot notableShot) {
    u8 *count;

    if (g_Players[playerIndex].isComputer ||
        g_GameState.replayKind != REPLAY_KIND_NONE ||
        g_GameState.gameMode == GAME_MODE_TRAINING) {
        return;
    }

    if (g_GameState.gameMode == GAME_MODE_RING_SHOT ||
        f_ShotFlightCount >= COUNTED_FLIGHT_COUNT_MAX) {
        return;
    }

    if (g_GameState.isMiniGolf && notableShot != NOTABLE_SHOT_HOLE_IN_ONE) {
        return;
    }

    count = (u8 *)&g_Players[playerIndex].notableShotCounts[notableShot];
    if (*count < NOTABLE_SHOT_COUNT_MAX) {
        osSyncPrintf("EXP GET type %d\n", notableShot);
        (*count)++;
    }
}

// The course points that each shot of a kind earns
// A great or an excellent shot
#define GREAT_SHOT_POINTS 2
#define PIN_SHOT_POINTS 4
#define CHIP_IN_POINTS 6
#define LONG_CHIP_IN_POINTS 8
// A flag shot, an eagle or a water cut
#define BIG_SHOT_POINTS 10
#define ALBATROSS_POINTS 16
#define HOLE_IN_ONE_POINTS 20
#define MINI_GOLF_HOLE_IN_ONE_POINTS 10

// The course points that the player has earned with the shots of one kind
s32 GetNotableShotPoints(s32 playerIndex, NotableShot notableShot) {
    s32 count = (u8)g_Players[playerIndex].notableShotCounts[notableShot];
    s32 points;

    switch (notableShot) {
        case NOTABLE_SHOT_GREAT:
        case NOTABLE_SHOT_EXCELLENT:
            points = GREAT_SHOT_POINTS;
            break;
        case NOTABLE_SHOT_LONG_CHIP_IN:
            points = LONG_CHIP_IN_POINTS;
            break;
        case NOTABLE_SHOT_PIN_SHOT:
        case NOTABLE_SHOT_UNUSED:
            points = PIN_SHOT_POINTS;
            break;
        case NOTABLE_SHOT_HOLE_IN_ONE:
            if (g_GameState.isMiniGolf) {
                points = MINI_GOLF_HOLE_IN_ONE_POINTS;
            }
            else {
                points = HOLE_IN_ONE_POINTS;
            }

            break;
        case NOTABLE_SHOT_FLAG_SHOT:
        case NOTABLE_SHOT_EAGLE:
        case NOTABLE_SHOT_WATER_CUT:
            points = BIG_SHOT_POINTS;
            break;
        case NOTABLE_SHOT_ALBATROSS:
            points = ALBATROSS_POINTS;
            break;
        case NOTABLE_SHOT_CHIP_IN:
        case NOTABLE_SHOT_BIRDIE:
            points = CHIP_IN_POINTS;
            break;
        default:
            osSyncPrintf("Unknown Shot Type%d\n", notableShot);
            points = 0;
            break;
    }

    return count * points;
}

// Turns each player's counts of notable shots into points, and returns
// everyone's total
s32 TallyCoursePoints(void) {
    s32 playerIndex;
    NotableShot notableShot;
    s32 points;
    s32 total = 0;

    for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
        points = 0;
        for (notableShot = 0; notableShot < NOTABLE_SHOT_COUNT; notableShot++) {
            points += GetNotableShotPoints(playerIndex, notableShot);
            g_Players[playerIndex].notableShotCounts[notableShot] = 0;
        }

        total += points;
        g_Players[playerIndex].points = points;
    }

    return total;
}

void PrintCoursePoints(void) {
    char text[DEBUG_TEXT_BUFFER_SIZE];
    s32 playerIndex;
    NotableShot notableShot;
    s32 points;

    for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
        points = 0;
        for (notableShot = 0; notableShot < NOTABLE_SHOT_COUNT; notableShot++) {
            points += GetNotableShotPoints(playerIndex, notableShot);
        }

        sprintf(text, "Player%d EXP %d\n", (int)playerIndex, (int)points);
        PrintDebugText(text, 2, playerIndex + 3);
    }
}

// Simulates the shot as the swing stands and builds the matrices for the dashes
// that preview its path. Until the swing sets the power, the preview shows a
// full swing.
// Whether the swing has yet to set the power in a player state
#define IsBeforePowerSet(state) \
    ((state) == PLAYER_STATE_AIMING || \
        (state) == PLAYER_STATE_SETTING_POWER || \
        (state) == PLAYER_STATE_UNUSED_AIMING)
// Whether the swing has yet to set the impact in a player state
#define IsBeforeImpactSet(state) \
    ((state) == PLAYER_STATE_AIMING || \
        (state) == PLAYER_STATE_SETTING_POWER || \
        (state) == PLAYER_STATE_SETTING_IMPACT || \
        (state) == PLAYER_STATE_UNUSED_AIMING)

#define PREVIEW_TUNING_TEXT_COLUMN 10
#define PREVIEW_TUNING_TEXT_ROW 10
#define PREVIEW_STEP_TEXT_COLUMN 10
#define PREVIEW_STEP_TEXT_ROW 3

void BuildShotPreview(ShotLaunch *shot, PlayerState playerState) {
    BallSimulation simulation;
    ShotLaunch previewShot;
    CoursePoint start;
    Mat4 dash;
    Mat4 align;
    s32 step;
    s32 length;
    Boolean8 *isMiniGolf;

    if ((GetSelectedClub()->surfaceMask & CLUB_SURFACE_GREEN) ||
        IsGameFlagSet(GAME_FLAG_HIDE_SHOT_PREVIEW)) {
        return;
    }

    f_IsShotPreviewBuilt = TRUE;
    previewShot.heading = g_CurrentShot.heading;
    previewShot.clubSlot = g_CurrentShot.clubSlot;
    if (IsGameFlagSet(GAME_FLAG_TUNE_SHOT_PREVIEW)) {
        PrintDebugText("               ", PREVIEW_TUNING_TEXT_COLUMN, PREVIEW_TUNING_TEXT_ROW);
        if ((g_ControllerStates[TEST_CONTROLLER].pressedButtons & U_JPAD) &&
            f_ShotPreviewStepCount < SHOT_PREVIEW_STEPS_MAX) {
            f_ShotPreviewStepCount++;
        }

        if ((g_ControllerStates[TEST_CONTROLLER].pressedButtons & D_JPAD) &&
            f_ShotPreviewStepCount > SHOT_PREVIEW_STEPS_MIN) {
            f_ShotPreviewStepCount--;
        }

        if ((g_ControllerStates[TEST_CONTROLLER].buttons & L_JPAD) &&
            f_ShotPreviewStepCount < SHOT_PREVIEW_STEPS_MAX) {
            f_ShotPreviewStepCount++;
        }

        if ((g_ControllerStates[TEST_CONTROLLER].buttons & R_JPAD) &&
            f_ShotPreviewStepCount > SHOT_PREVIEW_STEPS_MIN) {
            f_ShotPreviewStepCount--;
        }

        sprintf(g_PrintBuffer, "prec:%4d", (int)f_ShotPreviewStepCount);
        PrintDebugText(g_PrintBuffer, PREVIEW_STEP_TEXT_COLUMN, PREVIEW_STEP_TEXT_ROW);
    }

    if (IsBeforePowerSet(playerState)) {
        previewShot.power = LAUNCH_POWER_MAX;
    }
    else {
        previewShot.power = shot->power;
    }

    previewShot.impactTiming = 0;
    if (IsBeforeImpactSet(playerState)) {
        previewShot.offsetX = shot->offsetX;
        previewShot.offsetY = shot->offsetY;
    }
    else {
        previewShot.offsetX = 0;
        previewShot.offsetY = 0;
    }

    previewShot.playerIndex = g_CurrentShot.playerIndex;
    previewShot.shotMode = g_CurrentShot.shotMode;
    simulation.position[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
    simulation.position[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
    simulation.position[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
    simulation.surface = g_CurrentShot.surface;
    LaunchBallSimulation(&simulation, LAUNCH_PURPOSE_SHOT_PREVIEW, &previewShot);
    f_ShotPreviewDotScroll += SHOT_PREVIEW_SCROLL_STEP;
    for (step = 0; step < f_ShotPreviewStepCount; step++) {
        isMiniGolf = &g_GameState.isMiniGolf;
        if (*isMiniGolf && simulation.bounceCount == 0) {
            break;
        }

        start[AXIS_X] = simulation.position[AXIS_X];
        start[AXIS_Y] = simulation.position[AXIS_Y];
        start[AXIS_Z] = simulation.position[AXIS_Z];
        StepBallSimulation(&simulation);
        length = IntLength3D(
            simulation.position[AXIS_X] - start[AXIS_X],
            simulation.position[AXIS_Y] - start[AXIS_Y],
            simulation.position[AXIS_Z] - start[AXIS_Z]
        );
        guTranslateF(
            dash,
            start[AXIS_X] * COURSE_TO_WORLD_SCALE,
            (start[AXIS_Y] - SHOT_PREVIEW_DROP) * COURSE_TO_WORLD_SCALE,
            start[AXIS_Z] * COURSE_TO_WORLD_SCALE
        );
        f_ShotPreviewPoints[step][AXIS_X] = simulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE;
        f_ShotPreviewPoints[step][AXIS_Y] = simulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        f_ShotPreviewPoints[step][AXIS_Z] = simulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        if (length != 0 && length <= SHOT_PREVIEW_DASH_LENGTH_MAX) {
            guScaleF(dash, 1.0f, 1.0f, length * COURSE_TO_WORLD_SCALE);
            guAlignF(
                align,
                0.0f,
                simulation.position[AXIS_X] - start[AXIS_X],
                simulation.position[AXIS_Y] - start[AXIS_Y],
                simulation.position[AXIS_Z] - start[AXIS_Z]
            );
            guMtxCatF(dash, align, dash);
            guTranslateF(
                align,
                start[AXIS_X] * COURSE_TO_WORLD_SCALE,
                start[AXIS_Y] * COURSE_TO_WORLD_SCALE,
                start[AXIS_Z] * COURSE_TO_WORLD_SCALE
            );
            guMtxCatF(dash, align, dash);
        }

        guMtxF2L(dash, &f_ShotPreviewMatrices[g_DrawBufferIndex][step]);
        if (!*isMiniGolf && simulation.bounceCount != 0) {
            break;
        }
    }

    f_ShownShotPreviewPointCount = step - 2;
    if (f_ShownShotPreviewPointCount <= 0) {
        f_ShownShotPreviewPointCount = 1;
    }

    if (simulation.bounceCount != 0) {
        f_HasShotPreviewEnded = TRUE;
        f_ShownShotPreviewPointCount = step + 1;
    }
    else {
        f_HasShotPreviewEnded = FALSE;
    }
}

// Draws the shot preview that BuildShotPreview built. On the pause screen it is
// instead a straight line on screen, from the ball to the preview's last point.
//
// For that line the original scales dx and dy to the line's half width in
// place, in $f6 and $f4, and that both are set twice decides the order of the
// insns that first set them. sched.c fills a block from its end, and raises the
// priority of an insn that gives a register its only set (birthing_insn_p).
// Were dx and dy set once, their conversions from the integer differences would
// be such insns and would go last before fabsf's assembly, with the stores of
// the scaled coordinates ahead of the subtractions; each scaled end coordinate
// would die at its subtraction and local-alloc would give the difference its
// register. Set again by the scaling they are not, so the stores, which fabsf's
// operand does not wait on, go last: the four scaled coordinates outlive the
// subtractions in $a1, $v1, $a2 and $a0, and both differences take $v0.
//
// Measured against the match, as words differing and how many of them in shape:
//   The offsets as the products dy * scale and dx * scale, dx
//   and dy set once                                           60 (12)
//   The same with the squares summed dy's first (the old
//   draft)                                                    56 (12)
//   dy alone scaled in place, offsetY as dx * scale           42 (2)
//   dx alone scaled in place, offsetX as dy * scale           58 (4)
//   The squares summed dy's first                             6 (0)
//   dy worked out before dx                                   28 (0)
//   fabsf(dy) tested first                                    4 (0)
//   No scale: each scaled as itself times the half width,
//   divided by the length                                     41 (19)
//   The offsets as s16                                        29 (17)
//
// Also matching: dy scaled before dx; both scaled where the offsets are set,
// together or each before its offset; either scaling spelled out, with scale as
// either operand; the half width divided by a length local in each scaling; no
// offsets, with dy and dx cast where they are added; and the four coordinates
// scaled into locals and stored after the differences.
void DrawShotPreview(CameraMatrices *matrices) {
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
    gSPTexture(
        g_DisplayListHead++,
        TEXTURE_SCALE_HALF,
        TEXTURE_SCALE_HALF,
        0,
        G_TX_RENDERTILE,
        G_ON
    );
    gDPLoadTextureBlock(
        g_DisplayListHead++,
        TextureAddress(ImageBody(g_ShotPreviewDashImage)),
        G_IM_FMT_I,
        G_IM_SIZ_8b,
        SHOT_PREVIEW_IMAGE_SIZE,
        SHOT_PREVIEW_IMAGE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        TextureMask(SHOT_PREVIEW_IMAGE_SIZE),
        TextureMask(SHOT_PREVIEW_IMAGE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPSetTileSize(
        g_DisplayListHead++,
        G_TX_RENDERTILE,
        f_ShotPreviewDotScroll,
        0,
        f_ShotPreviewDotScroll + (SHOT_PREVIEW_IMAGE_SIZE << G_TEXTURE_IMAGE_FRAC),
        SHOT_PREVIEW_IMAGE_SIZE << G_TEXTURE_IMAGE_FRAC
    );
    gDPSetEnvColor(g_DisplayListHead++, 0xE0, 0xE0, 0x00, 0x40);
    if (f_HasShotPreviewEnded) {
        gDPSetPrimColor(g_DisplayListHead++, 0, 0, 0xFF, 0x00, 0x00, 0xC8);
    }
    else {
        gDPSetPrimColor(g_DisplayListHead++, 0, 0, 0xFF, 0xFF, 0xFF, 0x80);
    }

    gDPPipeSync(g_DisplayListHead++);
    if (g_PauseScreen == PAUSE_SCREEN_FREE_LOOK) {
        f32 deltaX;
        f32 deltaY;
        f32 scale;
        s32 offsetX;
        s32 offsetY;

        if (ProjectPoint(
                g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
                g_CurrentShot.ballPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE,
                g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE,
                &ball
            ) >= 0) {
            return;
        }

        if (ProjectPoint(
                f_ShotPreviewPoints[f_ShownShotPreviewPointCount - 1][AXIS_X],
                f_ShotPreviewPoints[f_ShownShotPreviewPointCount - 1][AXIS_Y],
                f_ShotPreviewPoints[f_ShownShotPreviewPointCount - 1][AXIS_Z],
                &end
            ) >= 0) {
            return;
        }

        guMtxIdent(&f_PuttPreviewIdentity);
        ball.x *= QUARTER_PIXELS_PER_PIXEL;
        ball.y *= QUARTER_PIXELS_PER_PIXEL;
        end.x *= QUARTER_PIXELS_PER_PIXEL;
        end.y *= QUARTER_PIXELS_PER_PIXEL;
        deltaX = end.x - ball.x;
        deltaY = end.y - ball.y;
        if (fabsf(deltaX) > 1.0f || fabsf(deltaY) > 1.0f) {
            scale = PUTT_PREVIEW_HALF_WIDTH / sqrtf((deltaX * deltaX) + (deltaY * deltaY));
            // Both scaled in place, to match
            deltaX *= scale;
            deltaY *= scale;
            gSPLoadGeometryMode(g_DisplayListHead++, 0);
            gSPSetGeometryMode(g_DisplayListHead++, G_SHADE | G_SHADING_SMOOTH);
            gDPSetCombineMode(
                g_DisplayListHead++,
                G_CC_MODULATEIDECALA_PRIM,
                G_CC_MODULATEIDECALA_PRIM
            );
            gDPPipeSync(g_DisplayListHead++);
            gDPSetRenderMode(g_DisplayListHead++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
            gDPPipeSync(g_DisplayListHead++);
            gSPMatrix(
                g_DisplayListHead++,
                PhysicalAddress(&g_ScreenOrthoMatrix),
                G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
            );
            gSPMatrix(
                g_DisplayListHead++,
                PhysicalAddress(&f_PuttPreviewIdentity),
                G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
            );
            gSPVertex(g_DisplayListHead++, f_PuttPreviewLineVertices, QUAD_VERTEX_COUNT, 0);
            offsetX = deltaY;
            offsetY = deltaX;
            gSPModifyVertex(
                g_DisplayListHead++,
                0,
                G_MWO_POINT_XYSCREEN,
                ScreenPointWord(ball.x + offsetX, ball.y)
            );
            gSPModifyVertex(
                g_DisplayListHead++,
                1,
                G_MWO_POINT_XYSCREEN,
                ScreenPointWord(ball.x, ball.y + offsetY)
            );
            gSPModifyVertex(
                g_DisplayListHead++,
                2,
                G_MWO_POINT_XYSCREEN,
                ScreenPointWord(end.x + offsetX, end.y)
            );
            gSPModifyVertex(
                g_DisplayListHead++,
                3,
                G_MWO_POINT_XYSCREEN,
                ScreenPointWord(end.x, end.y + offsetY)
            );
            gSP2Triangles(g_DisplayListHead++, 1, 0, 2, 0, 1, 2, 3, 0);
            gSPMatrix(
                g_DisplayListHead++,
                &matrices->projection,
                G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
            );
            gDPPipeSync(g_DisplayListHead++);
        }
    }
    else {
        s32 step;

        if (g_LetterboxState == LETTERBOX_STATE_NONE ||
            g_LetterboxState == LETTERBOX_STATE_UNUSED_AIMING) {
            gDPPipeSync(g_DisplayListHead++);
            gDPSetRenderMode(g_DisplayListHead++, G_RM_ZB_CLD_SURF, G_RM_ZB_CLD_SURF2);
        }
        else {
            gDPPipeSync(g_DisplayListHead++);
            gDPSetRenderMode(g_DisplayListHead++, G_RM_AA_ZB_XLU_SURF, G_RM_AA_ZB_XLU_SURF2);
        }

        gSPLoadGeometryMode(g_DisplayListHead++, 0);
        gSPSetGeometryMode(g_DisplayListHead++, G_ZBUFFER | G_SHADE | G_FOG | G_SHADING_SMOOTH);
        gSPFogFactor(g_DisplayListHead++, 320, -128);
        gDPSetCombineLERP(
            g_DisplayListHead++,
            TEXEL0,
            0,
            PRIMITIVE,
            0,
            TEXEL0,
            0,
            SHADE,
            0,
            TEXEL0,
            0,
            PRIMITIVE,
            0,
            TEXEL0,
            0,
            SHADE,
            0
        );
        for (step = 0; step < f_ShownShotPreviewPointCount; step++) {
            gSPMatrix(
                g_DisplayListHead++,
                &f_ShotPreviewMatrices[g_DrawBufferIndex][step],
                G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
            );
            gSPVertex(g_DisplayListHead++, f_ShotPreviewDashVertices, QUAD_VERTEX_COUNT, 0);
            gSP2Triangles(g_DisplayListHead++, 0, 1, 2, 0, 0, 2, 3, 0);
            gSPPopMatrix(g_DisplayListHead++, G_MTX_MODELVIEW);
        }

        gDPPipeSync(g_DisplayListHead++);
    }
}

// Returns whether the player's hole-out may enter the records: a short one,
// by one of the main characters, outside mini-golf, in a mode that keeps them
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

    if (g_Players[playerIndex].strokeCount > RECORDED_STROKE_COUNT_MAX) {
        return FALSE;
    }

    if (g_Players[playerIndex].characterIndex >= CHARACTER_KID) {
        return FALSE;
    }

    if (g_GameState.isMiniGolf) {
        return FALSE;
    }

    switch (g_GameState.gameMode) {
        case GAME_MODE_STROKE:
        case GAME_MODE_UNLISTED_STROKE:
        case GAME_MODE_TRAINING:
        case GAME_MODE_GET_CHARACTER:
        case GAME_MODE_TOURNAMENT:
        case GAME_MODE_SPEED_GOLF:
            return TRUE;
        case GAME_MODE_MATCH_GAME:
        case GAME_MODE_UNRECORDED_STROKE:
        case GAME_MODE_RECORD_REPLAY:
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

// Keeps the player's last shot, and the hole and how it was played, in their
// replay record
void RecordHoleOut(s32 playerIndex) {
    u32 shotIndex = g_Players[playerIndex].strokeCount - 1;

    if (shotIndex < HOLE_RECORD_STROKES_MAX) {
        PlayRecord *record = &g_PlayRecords[playerIndex];

        record->gameMode = g_GameState.gameMode;
        record->courseIndex = g_GameState.courseIndex;
        record->holeIndex = g_GameState.holeIndex;
        record->scoreKind = GetRecordedScoreKind(g_Players[playerIndex].strokeCount);
        record->characterIndex = g_Players[playerIndex].characterIndex;
        record->isLefty = g_Players[playerIndex].isLefty;
        record->paletteIndex = g_Players[playerIndex].paletteIndex;
        record->strokeCount = g_Players[playerIndex].strokeCount;
        record->noiseSeed = g_Players[playerIndex].noiseSeed;
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

// Keeps the player's last shot, and the hole and how it was played, in their
// Ring Shot record
void RecordRingShotHole(s32 playerIndex) {
    u32 shotIndex = g_Players[playerIndex].strokeCount - 1;

    if (shotIndex < RING_SHOT_RECORD_STROKES_MAX) {
        RingShotRecord *record = &g_RingShotRecords[playerIndex];

        record->courseIndex = g_GameState.courseIndex;
        record->holeIndex = g_GameState.holeIndex;
        record->scoreKind = GetRecordedScoreKind(g_Players[playerIndex].strokeCount);
        record->characterIndex = g_Players[playerIndex].characterIndex;
        record->isLefty = g_Players[playerIndex].isLefty;
        record->paletteIndex = g_Players[playerIndex].paletteIndex;
        record->strokeCount = g_Players[playerIndex].strokeCount;
        record->noiseSeed = g_Players[playerIndex].noiseSeed;
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

Boolean HasNoRingsLeft(Player *player) {
    s32 ringIndex;

    for (ringIndex = 0; ringIndex < RING_COUNT; ringIndex++) {
        if (player->ringStates[ringIndex] != RING_STATE_NONE) {
            return FALSE;
        }
    }

    return TRUE;
}

Boolean AreAllRingsPassed(Player *player) {
    s32 ringIndex;

    for (ringIndex = 0; ringIndex < RING_COUNT; ringIndex++) {
        if (player->ringStates[ringIndex] == RING_STATE_SHOWN) {
            return FALSE;
        }
    }

    return TRUE;
}

void RecordRingShotClear(s32 playerIndex) {
    s8 *count =
        &GetSaveData()->records.hasRingShotStar[g_GameState.courseIndex][g_GameState.holeIndex];

    (*count)++;
    if (*count < 0) {
        *count = 0;
    }
    else if (*count > RING_SHOT_CLEAR_COUNT_MAX) {
        *count = RING_SHOT_CLEAR_COUNT_MAX;
    }

    if (g_Players[playerIndex].characterIndex < CHARACTER_COUNT) {
        count = &GetSaveData()->ringShotClearCounts[g_GameState.courseIndex][g_GameState.holeIndex]
                                                   [g_Players[playerIndex].characterIndex];
        (*count)++;
        if (*count < 0) {
            *count = 0;
        }
        else if (*count > RING_SHOT_CLEAR_COUNT_MAX) {
            *count = RING_SHOT_CLEAR_COUNT_MAX;
        }
    }

    WriteSaveData();
}

// Saves a Ring Shot hole as cleared once it is holed within par with every ring
// passed
void CheckRingShotClear(s32 playerIndex) {
    if (g_GameState.gameMode == GAME_MODE_RING_SHOT &&
        g_Players[playerIndex].strokeCount <= g_HoleSetup.par &&
        !g_Players[playerIndex].isComputer &&
        HasNoRingsLeft(&g_Players[playerIndex])) {
        RecordRingShotClear(playerIndex);
    }
}

// The first of the club kind's animations; shotMode is unused. The staff
// roll overlay has its own copies of these four routines, with the same
// names, so these are static to link beside them.
static AnimationId GetStanceAnimation(
    u32 slotIndex,
    ClubIndex clubIndex,

    ShotMode shotMode UNUSED
) {
    AnimationId animationId;
    switch (GetCharacterClubCategory(slotIndex, clubIndex)) {
        case CLUB_CATEGORY_WOOD:
            animationId = WOOD_ANIMATIONS;
            break;
        case CLUB_CATEGORY_IRON:
            animationId = IRON_ANIMATIONS;
            break;
        case CLUB_CATEGORY_WEDGE:
            animationId = WEDGE_ANIMATIONS;
            break;
        case CLUB_CATEGORY_PUTTER:
            animationId = PUTTER_ANIMATIONS;
            break;
        default:
            animationId = WOOD_ANIMATIONS;
            break;
    }

    return animationId;
}

// The second of the club kind's animations, or the third for an approach shot
static AnimationId GetSwingAnimation(u32 slotIndex, ClubIndex clubIndex, ShotMode shotMode) {
    AnimationId animationId;

    switch (GetCharacterClubCategory(slotIndex, clubIndex)) {
        case CLUB_CATEGORY_WOOD:
            animationId = WOOD_ANIMATIONS + SWING_ANIMATION;
            break;
        case CLUB_CATEGORY_IRON:
            animationId = IRON_ANIMATIONS + SWING_ANIMATION;
            break;
        case CLUB_CATEGORY_WEDGE:
            animationId = WEDGE_ANIMATIONS + SWING_ANIMATION;
            break;
        case CLUB_CATEGORY_PUTTER:
            animationId = PUTTER_ANIMATIONS + SWING_ANIMATION;
            break;
        default:
            animationId = 0;
            break;
    }

    return animationId + (shotMode == SHOT_MODE_APPROACH);
}

static void SetStanceAnimation(u32 slotIndex, ClubIndex clubIndex, ShotMode shotMode) {
    SetCharacterAnimation(slotIndex, GetStanceAnimation(slotIndex, clubIndex, shotMode));
}

static void SetSwingAnimation(u32 slotIndex, ClubIndex clubIndex, ShotMode shotMode) {
    SetCharacterAnimation(slotIndex, GetSwingAnimation(slotIndex, clubIndex, shotMode));
}

// Sets the shot up for the player whose turn it is: where the ball lies and the
// heading. Off the tee of a par 4 or 5 the shot aims at the hole's first aim
// point, and otherwise at the cup. A hole without that point gets the cup's
// position for it.
void StartPlayerShot(s32 playerIndex) {
    Player *player = &g_Players[playerIndex];
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
    g_CurrentShot.surface = player->ballSurface;
    g_CurrentShot.offsetY = 0;
    g_CurrentShot.offsetX = 0;
    g_CurrentShot.powerShotCount = player->powerShotCount;
    g_CurrentShot.gaugeFrame = 0;
    g_CurrentShot.cupDistance =
        GetCupDistance(x - g_HoleSetup.cupPosition[AXIS_X], z - g_HoleSetup.cupPosition[AXIS_Z]);
    if (!g_IsTrainingHeadingKept || g_GameState.gameMode != GAME_MODE_TRAINING) {
        SurfaceKind surfaceKind = GetSurfaceTraits(g_CurrentShot.surface)->kind;

        if (g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_X] == 0 ||
            g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_Z] == 0) {
            g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_X] = g_HoleSetup.cupPosition[AXIS_X];
            g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y];
            g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z];
        }

        if (g_HoleSetup.par != PAR_THREE &&
            (surfaceKind == SURFACE_KIND_TEE_UP || surfaceKind == SURFACE_KIND_TEE_GROUND)) {
            target[AXIS_X] = g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_X];
            target[AXIS_Y] = g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_Y];
            target[AXIS_Z] = g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_Z];
        }
        else {
            target[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X];
            target[AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y];
            target[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z];
        }

        g_CurrentShot.heading = atan2(
                                    target[AXIS_Z] - player->ballPosition[AXIS_Z],
                                    target[AXIS_X] - player->ballPosition[AXIS_X]
                                ) *
            RADIANS_TO_BINARY_ANGLE;
        ChooseStartingClub(&g_HoleSetup);
    }

    g_IsTrainingHeadingKept = FALSE;
    if (IsGameFlagSet(GAME_FLAG_AIM_DOWN_RANGE)) {
        AimDownRange();
        g_LetterboxState = LETTERBOX_STATE_MOVING_CAMERA;
    }
    else {
        ResetAimCamera();
        g_LetterboxState = LETTERBOX_STATE_NONE;
    }

    g_RoundHud.scorecardPage = -1;
    g_RoundHud.isWindIndicatorShown = TRUE;
    g_RoundHud.isHoleInfoShown = TRUE;
    g_RoundHud.isLetterboxShown = TRUE;
    g_RoundHud.isLieIconShown = TRUE;
    g_RoundHud.isClubHeadIconShown = TRUE;
    g_RoundHud.isUnreadItemShown = TRUE;
    g_RoundHud.isHoleIntroTextShown = FALSE;
    g_RoundHud.isClubPanelShown = TRUE;
    g_RoundHud.isShotLengthShown = FALSE;
    g_RoundHud.isProgressShown = TRUE;
    g_RoundHud.isIgnoredTurnPartShown = TRUE;
    g_RoundHud.isPuttingClubChosen = FALSE;
    g_RoundHud.flags = 0;
    PlaceShotCamera();
    ResetShotOffsetMarker();
}

// Left empty in the original; called after each step of a flying ball
void OnBallStepped(Player *player UNUSED) {
}

// Once every player's shot has finished, picks who plays next. Players still on
// the tee go first, then those off the green, then those on it. Among those,
// the farthest from the cup goes first, and the earliest in the tee order
// breaks ties.
void ChooseNextPlayer(void) {
    s32 playerIndex;
    s32 finishedCount;
    s32 nextPlayer;
    s32 bestPriority;
    s32 bestDistance;
    s32 bestTeeOrder;
    s32 priority;
    s32 distance;

    if (g_ContestMovieState == CONTEST_MOVIES_PLAYING) {
        return;
    }

    for (finishedCount = 0; finishedCount < g_GameState.playerCount; finishedCount++) {
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
    bestTeeOrder = S32_MAX;
    nextPlayer = 0;
    for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
        priority = LIE_PRIORITY_TEE;
        if (g_Players[playerIndex].holeStatus != HOLE_STATUS_PLAYING) {
            continue;
        }

        if (IsGameFlagSet(GAME_FLAG_FEWEST_STROKES_FIRST)) {
            distance = 0;
            priority = STROKE_PRIORITY_BASE - g_Players[playerIndex].strokeCount;
        }
        else {
            if (g_Players[playerIndex].strokeCount != 0) {
                priority = GetSurfaceTraits(g_Players[playerIndex].ballSurface)->kind !=
                    SURFACE_KIND_GREEN;
            }

            distance = IntLength2D(
                g_HoleSetup.cupPosition[AXIS_X] - g_Players[playerIndex].ballPosition[AXIS_X],
                g_HoleSetup.cupPosition[AXIS_Z] - g_Players[playerIndex].ballPosition[AXIS_Z]
            );
        }

        if (priority > bestPriority) {
            bestPriority = priority;
            nextPlayer = playerIndex;
            bestDistance = distance;
            bestTeeOrder = g_Players[nextPlayer].teeOrder;
        }
        else if (priority == bestPriority) {
            if (distance > bestDistance) {
                nextPlayer = playerIndex;
                bestDistance = distance;
                bestTeeOrder = g_Players[nextPlayer].teeOrder;
            }
            else if (distance == bestDistance) {
                if (g_Players[playerIndex].teeOrder < bestTeeOrder) {
                    bestTeeOrder = g_Players[playerIndex].teeOrder;
                    bestDistance = distance;
                    nextPlayer = playerIndex;
                }
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

// Hides the rings the ball passed, so that a new shot can pass them again
void ClearPassedRings(Player *player) {
    s32 ringIndex;

    for (ringIndex = 0; ringIndex < RING_COUNT; ringIndex++) {
        if (player->ringStates[ringIndex] >= RING_STATE_PASSED) {
            player->ringStates[ringIndex] = RING_STATE_NONE;
        }
    }
}

// Now and then plays one of the course's ambient sounds, picked at random, from
// a random side
void PlayAmbientSound(void) {
    s32 choiceIndex;

    if (++f_AmbientSoundIdleFrameCount == AMBIENT_SOUND_INTERVAL) {
        choiceIndex = guRandom() % AMBIENT_SOUND_CHOICES;
        f_AmbientSoundIdleFrameCount = 0;
        if (f_AmbientSounds[GetCourseRecordIndex()][choiceIndex] != NO_AMBIENT_SOUND) {
            PlaySoundWithEffects(
                f_AmbientSounds[GetCourseRecordIndex()][choiceIndex],
                AMBIENT_SOUND_CHANNEL,
                AMBIENT_SOUND_VOLUME,
                AMBIENT_SOUND_REVERB,
                guRandom() % SOUND_PAN_RANGE
            );
        }
    }
}

// The slot to free is always the golfer's, whatever the caller passes
void FreeCharacterAndReactionLight(s32 slotIndex UNUSED) {
    FreeCharacterSlot(0);
    TurnOffReactionLight();
}

// Measures a tee shot for the records: the drive counts if it ends on the
// fairway or the green, and the distance to the pin if it ends on the green.
void MeasureTeeShot(s32 playerIndex) {
    s32 distance;
    SurfaceTraits *surfaceTraits;

    if (g_Players[playerIndex].strokeCount == 1) {
        distance = UNCOUNTED_DISTANCE;
        surfaceTraits = GetSurfaceTraits(g_SimulatedShot.endSurface);
        switch ((u32)surfaceTraits->kind) {
            case SURFACE_KIND_FAIRWAY:
            case SURFACE_KIND_FAIRWAY_FAIR_LIE:
            case SURFACE_KIND_FAIRWAY_POOR_LIE:
            case SURFACE_KIND_GREEN:
            case SURFACE_KIND_GREEN_EDGE:
                distance = IntLength2D(
                    g_SimulatedShot.startPosition[AXIS_X] - g_SimulatedShot.endPosition[AXIS_X],
                    g_SimulatedShot.startPosition[AXIS_Z] - g_SimulatedShot.endPosition[AXIS_Z]
                );
                break;
        }

        g_Players[playerIndex].driveDistance = distance;
        distance = UNCOUNTED_DISTANCE;
        if (g_SimulatedShot.endsInCup) {
            distance = 0;
        }
        else if (surfaceTraits->kind == SURFACE_KIND_GREEN) {
            distance = GetCupDistance(
                g_HoleSetup.cupPosition[AXIS_X] - g_SimulatedShot.endPosition[AXIS_X],
                g_HoleSetup.cupPosition[AXIS_Z] - g_SimulatedShot.endPosition[AXIS_Z]
            );
        }

        g_Players[playerIndex].pinDistance = distance;
    }
}

// Leaves the distances that the player's contests have not measured uncounted
void SkipUnmeasuredDistances(s32 playerIndex) {
    if (g_Players[playerIndex].driveDistance == UNMEASURED_DISTANCE) {
        g_Players[playerIndex].driveDistance = UNCOUNTED_DISTANCE;
    }

    if (g_Players[playerIndex].pinDistance == UNMEASURED_DISTANCE) {
        g_Players[playerIndex].pinDistance = UNCOUNTED_DISTANCE;
    }
}

// Rumbles the player's controller briefly in a game for several players,
// if every player has a controller of their own
void StartTurnRumble(s32 playerIndex) {
    if (g_HasOwnControllers == TRUE && g_GameState.playerCount >= 2) {
        s32 controllerIndex = g_Players[playerIndex].controllerIndex;

        EndRumbleForceStop();
        if ((u32)controllerIndex < MAXCONTROLLERS && g_GameState.replayKind == REPLAY_KIND_NONE) {
            switch (nuContRmbCheck(g_PlayerControllerPorts[controllerIndex])) {
                case 0:
                    nuContRmbStart(
                        g_PlayerControllerPorts[controllerIndex],
                        RUMBLE_FREQUENCY,
                        TURN_RUMBLE_FRAME_COUNT
                    );
                    break;
                // The empty cases are there to match: without them, with one or a
                // default, or with an if for the 0 case, 23 words differ, 9 in shape
                case PFS_ERR_NOPACK:
                    break;
                case PFS_ERR_CONTRFAIL:
                    break;
            }
        }
    }
}

// Lets the players who are waiting make their characters call out with the
// D-pad and the C buttons, if every player has a controller of their own
void PlayWaitingPlayersCalls(void) {
    s32 playerIndex;
    s32 controllerIndex;
    u16 pressedButtons;

    if (g_HasOwnControllers == TRUE) {
        for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
            if ((g_Players[playerIndex].state == PLAYER_STATE_WAITING) |
                (g_Players[playerIndex].state == PLAYER_STATE_HOLE_DONE)) {
                controllerIndex = g_Players[playerIndex].controllerIndex;
                if (controllerIndex == g_Players[g_GameState.currentPlayerIndex].controllerIndex) {
                    continue;
                }

                if (controllerIndex >= MAXCONTROLLERS) {
                    continue;
                }

                pressedButtons = g_ControllerStates[controllerIndex].pressedButtons;
                if (pressedButtons & U_JPAD) {
                    PlayCharacterSound(
                        g_Players[playerIndex].characterIndex,
                        playerIndex,
                        CHARACTER_SOUND_CALL_OUT_UP,
                        SOUND_FULL_SCALE,
                        g_Players[playerIndex].soundPan
                    );
                }
                else if (pressedButtons & D_JPAD) {
                    PlayCharacterSound(
                        g_Players[playerIndex].characterIndex,
                        playerIndex,
                        CHARACTER_SOUND_CALL_OUT_DOWN,
                        SOUND_FULL_SCALE,
                        g_Players[playerIndex].soundPan
                    );
                }
                else if (pressedButtons & L_JPAD) {
                    PlayCharacterSound(
                        g_Players[playerIndex].characterIndex,
                        playerIndex,
                        CHARACTER_SOUND_CALL_OUT_LEFT,
                        SOUND_FULL_SCALE,
                        g_Players[playerIndex].soundPan
                    );
                }
                else if (pressedButtons & R_JPAD) {
                    PlayCharacterSound(
                        g_Players[playerIndex].characterIndex,
                        playerIndex,
                        CHARACTER_SOUND_CALL_OUT_RIGHT,
                        SOUND_FULL_SCALE,
                        g_Players[playerIndex].soundPan
                    );
                }
                else if (pressedButtons & U_CBUTTONS) {
                    PlayCharacterSound(
                        g_Players[playerIndex].characterIndex,
                        playerIndex,
                        CHARACTER_SOUND_CALL_OUT_C_UP,
                        SOUND_FULL_SCALE,
                        g_Players[playerIndex].soundPan
                    );
                }
                else if (pressedButtons & D_CBUTTONS) {
                    PlayCharacterSound(
                        g_Players[playerIndex].characterIndex,
                        playerIndex,
                        CHARACTER_SOUND_CALL_OUT_C_DOWN,
                        SOUND_FULL_SCALE,
                        g_Players[playerIndex].soundPan
                    );
                }
                else if (pressedButtons & L_CBUTTONS) {
                    PlayCharacterSound(
                        g_Players[playerIndex].characterIndex,
                        playerIndex,
                        CHARACTER_SOUND_CALL_OUT_C_LEFT,
                        SOUND_FULL_SCALE,
                        g_Players[playerIndex].soundPan
                    );
                }
                else if (pressedButtons & R_CBUTTONS) {
                    PlayCharacterSound(
                        g_Players[playerIndex].characterIndex,
                        playerIndex,
                        CHARACTER_SOUND_CALL_OUT_C_RIGHT,
                        SOUND_FULL_SCALE,
                        g_Players[playerIndex].soundPan
                    );
                }
            }
        }
    }
}

// Plays the jingle for how the player finished the hole
void PlayHoleOutSong(s32 playerIndex) {
    s32 strokeCount = g_Players[playerIndex].strokeCount;
    ScoreToPar scoreToPar = strokeCount - g_HoleSetup.par;

    if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
        PlaySong(SONG_HOLE_WON, 0);
    }
    else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
        if (AreAllRingsPassed(&g_Players[playerIndex])) {
            PlaySong(SONG_HOLE_WON, 0);
        }
        else {
            PlaySong(SONG_HOLE_LOST, 0);
        }
    }
    else if (strokeCount == 1) {
        PlaySong(SONG_HOLE_OUT_BIRDIE, 0);
    }
    else {
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
    ScoreToPar scoreToPar = strokeCount - g_HoleSetup.par;
    s32 banner = BANNER_NONE;

    if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
        StartBanner(BANNER_FINISH);
        PlaySound(SOUND_CALL_FINISH, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
    }
    else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
        if (AreAllRingsPassed(&g_Players[playerIndex])) {
            StartBanner(BANNER_COMPLETE);
            PlaySound(SOUND_CALL_COMPLETE, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
        }
    }
    else if (strokeCount == 1) {
        banner = BANNER_HOLE_IN_ONE;
        PlaySound(SOUND_CALL_HOLE_IN_ONE, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
    }
    else {
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
                PlaySound(SOUND_CALL_ALBATROSS, HOLE_OUT_CALL_CHANNEL, SOUND_FULL_SCALE);
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

// UpdateTurns holds ten functions nested in it, a GNU C extension. GCC
// passes them the parent's frame in $v0, its static chain, and they read
// the parent's playerIndex through it.
//
// Its three debug blocks are written in place. The original's ball placement
// was expanded with the constant addresses inside its memory references: GCC
// compiles each nested function at its definition, and rest_of_compilation sets
// cse_not_expected, which push_function_context doesn't save, so the rest of
// UpdateTurns is expanded with it set. A separate inline helper is expanded
// first, under its own init_function_start, which clears it, so memory_address
// puts each constant address in a register; as a helper, the ball placement
// kept the ball's X address in a register and CSE reached Z through it with an
// offset, where the original loads and stores each through its own lui. Written
// in place, the ball placement gave 90 words (48 in shape) against 146 (78) as
// a helper.
//
// Many forms follow the original's code rather than the shortest C.
// Tests that the original branches on separately are nested, not joined
// with &&: the hole-out song's Speed Golf and Ring Shot tests, Speed
// Golf outside, and the debug step's trigger and camera tests. The
// letterbox test before the debug output is written "== FALSE", for
// which the original ors both comparisons. The cup distance goes through
// a float, as round_hud.c's does. The hole-out song switch lists its
// modes in the order the original places their bodies, and the score
// switch gives a bogey a case before the default so that GCC builds the
// original's jump table. The planning tests read f_ShotSource itself
// after each loop, and the poll counts are zeroed first in the planning
// block. A player out of the group game has the hole's strokes and the
// stroke count zeroed by one chained assignment.
//
// Where the original sets a value in a branch's delay slot, the draft
// sets it in an arm of that branch: the holed-out reaction and notable
// shot work out the score to par before the hole-in-one test; a Ring
// Shot's reaction is the bogey's in an else arm; the turn's start tests
// the shot's rest first, in one chain with the speed-golf hole intro and
// aiming; the stroke limit sets the reaction state once, after the
// speed-golf penalty, and the Ring Shot's par test sets it in its own
// arm, which keeps those tails from being cross-jumped together. Where
// the original shares code by cross-jumping, each arm has its own copy:
// the song tempo's calls, PlaceTargetPoint in each arm of the units
// test, and nextState set before ForceStopRumble in the Ring Shot
// hole-out. The next state is set after the one-on-one match's handicap
// scoring, which calls GetCourseRecordIndex with no argument, as the
// ambient sounds' lookups do. The debug ball move holds the Z trigger
// test in a flag, as the original shifts the bit down and masks it,
// which GCC does only for a bit test stored as a value. The training
// choice box's reaction test and the frame test at the end are written
// in place, as inline helpers reload their constants and addresses for
// each call.
//
// Preparing the aim, nextState is set to PLAYER_STATE_AIMING at the top
// of the case, though the HUD test at its end always overwrites it. This
// is the one form here with no other reason to exist: the original
// compares f_ShotSource with nextState's $s3, which cse does only when
// nextState already holds the 2 that the comparison needs, and without
// the store 3 words differ.
// Whether the stopwatches run in a player state: from aiming to the flight
#define IsTimedState(state) \
    ((state) == PLAYER_STATE_AIMING || \
        (state) == PLAYER_STATE_SETTING_POWER || \
        (state) == PLAYER_STATE_SETTING_IMPACT || \
        (state) == PLAYER_STATE_STARTING_SWING || \
        (state) == PLAYER_STATE_SWINGING || \
        (state) == PLAYER_STATE_BALL_IN_FLIGHT)
// Whether a pause screen lets play go on
#define IsPlayGoingOn(pauseScreen) \
    ((pauseScreen) == PAUSE_SCREEN_NONE || (pauseScreen) == PAUSE_SCREEN_FREE_LOOK)
// Whether a character is Yoshi or Donkey Kong, who share a round song
#define IsYoshiOrDonkeyKong(character) \
    ((character) == CHARACTER_YOSHI || (character) == CHARACTER_DONKEY_KONG)
// Whether four players are all Yoshi or Donkey Kong, which has its own song
#define IsYoshiAndDonkeyKongRound() \
    (g_GameState.playerCount == PLAYER_COUNT_MAX && \
        IsYoshiOrDonkeyKong(g_Players[0].characterIndex) && \
        IsYoshiOrDonkeyKong(g_Players[1].characterIndex) && \
        IsYoshiOrDonkeyKong(g_Players[2].characterIndex) && \
        IsYoshiOrDonkeyKong(g_Players[3].characterIndex))

// Places the target point DISTANCE units of SCALE from the ball at ANGLE
#define PlaceTargetPoint(angle, distance, scale) \
    do { \
        f32 cosine = cosf(angle); \
        f32 length = (distance) * (scale); \
\
        g_CurrentShot.targetPoint[AXIS_X] = \
            (cosine * length) + g_CurrentShot.ballPosition[AXIS_X]; \
        g_CurrentShot.targetPoint[AXIS_Z] = \
            (sinf(angle) * length) + g_CurrentShot.ballPosition[AXIS_Z]; \
    } \
    while (FALSE)

#define SWING_TEMPO_TEXT_COLUMN 10
#define SWING_TEMPO_TEXT_ROW 10
#define BALL_POSITION_TEXT_COLUMN 3
#define BALL_POSITION_TEXT_ROW 6
#define REACTION_TEST_TEXT_COLUMN 3
#define REACTION_TEST_TEXT_ROW 3
#define STATE_ERROR_TEXT_COLUMN 20
#define STATE_ERROR_TEXT_ROW 2
#define HOLE_OUT_SONG_FRAME 1
#define ONLOOKERS_BANNER_CLEAR_FRAME 29
#define ONLOOKERS_BANNER_FRAME 30
#define LONG_PUTT_BANNER_FRAME 29

void UpdateTurns(void) {
    s32 playerIndex;
    PlayerState state;
    PlayerState nextState;
    Boolean areCallsAllowed;
    s32 skipStep;
    s32 checkedPlayer;
    s32 outPlayer;
    s32 aimingPollIndex;
    s32 preparingPollIndex;
    s32 extraStepIndex;
    s32 debugPlayerIndex;
    s32 groupPlayerIndex;

    // Returns whether the B button is held
    Boolean IsBButtonHeld(void) {
        if (g_MergedController.buttons & B_BUTTON) {
            return TRUE;
        }

        return FALSE;
    }

    // The turn controller's stick X, which left and right on the control pad tilt
    // fully
    s32 GetTurnStickX(void) {
        s32 x = (g_TurnController.buttons & L_JPAD) ? -DPAD_STICK_TILT : 0;

        if (g_TurnController.buttons & R_JPAD) {
            x = DPAD_STICK_TILT;
        }

        if (x == 0) {
            x = g_TurnController.stickX;
        }

        return x;
    }

    // The turn controller's stick Y, which down and up on the control pad tilt
    // fully
    s32 GetTurnStickY(void) {
        s32 y = (g_TurnController.buttons & D_JPAD) ? -DPAD_STICK_TILT : 0;

        if (g_TurnController.buttons & U_JPAD) {
            y = DPAD_STICK_TILT;
        }

        if (y == 0) {
            y = g_TurnController.stickY;
        }

        return y;
    }

    // Makes the golfer translucent while a club for the green is held
    void UpdateGolferTranslucency(void) {
        if ((GetClub(g_Players[playerIndex].clubs[g_CurrentShot.clubSlot])->surfaceMask &
                CLUB_SURFACE_GREEN) &&
            g_LetterboxState != LETTERBOX_STATE_LOOKING_BACK) {
            SetCharacterTranslucent(0, TRUE);
        }
        else {
            SetCharacterTranslucent(0, FALSE);
        }
    }

    // Poses the golfer for the picked club and shot mode, when either has
    // changed. An approach shows the swing's first frame, held; the other modes
    // show the stance.
    void PoseGolferForShot(void) {
        s32 *lastClubSlot = &f_ShotLaunch.clubSlot;

        if (*lastClubSlot != g_CurrentShot.clubSlot ||
            f_ShotLaunch.shotMode != g_CurrentShot.shotMode) {
            *lastClubSlot = g_CurrentShot.clubSlot;
            f_ShotLaunch.shotMode = g_CurrentShot.shotMode;
            SetCharacterClub(0, g_Players[playerIndex].clubs[g_CurrentShot.clubSlot]);
            UpdateGolferTranslucency();
            if (g_CurrentShot.shotMode == SHOT_MODE_APPROACH) {
                SetSwingAnimation(
                    0,
                    g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
                    SHOT_MODE_APPROACH
                );
                SetAnimationSpeed(0, 0);
            }
            else {
                SetStanceAnimation(
                    0,
                    g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
                    g_CurrentShot.shotMode
                );
                SkipToAnimationEnd(0);
            }
        }
    }

    // Advances the player's, the round's and the hole's stopwatches by a frame,
    // up to their limit
    void TickStopwatches(void) {
        if (!IsReplayingShot()) {
            g_Players[playerIndex].stopwatchFrameCount++;
            g_GameState.stopwatchFrameCount++;
            g_HoleTimeFrames[g_GameState.holeIndex]++;
            if (g_Players[playerIndex].stopwatchFrameCount > STOPWATCH_FRAMES_MAX) {
                g_Players[playerIndex].stopwatchFrameCount = STOPWATCH_FRAMES_MAX;
            }

            if (g_GameState.stopwatchFrameCount > STOPWATCH_FRAMES_MAX) {
                g_GameState.stopwatchFrameCount = STOPWATCH_FRAMES_MAX;
            }

            if (g_HoleTimeFrames[g_GameState.holeIndex] > STOPWATCH_FRAMES_MAX) {
                g_HoleTimeFrames[g_GameState.holeIndex] = STOPWATCH_FRAMES_MAX;
            }
        }
    }

    // Takes the player out of the hole. In a match, a player whose opponent has
    // holed out loses the hole by a stroke; otherwise the hole counts as
    // GIVE_UP_PAR_MULTIPLE times its par, and an opponent still playing wins it
    // by a stroke.
    void GiveUpHole(s32 playerIndex) {
        s32 opponentIndex;

        if (g_GameState.replayKind == REPLAY_KIND_NONE &&
            IsOneOnOneMatchMode(g_GameState.gameMode) &&
            g_Players[playerIndex ^ 1].holeStatus == HOLE_STATUS_HOLED_OUT) {
            g_Players[playerIndex].holes[g_GameState.holeIndex % COURSE_HOLE_COUNT].strokeCount =
                g_Players[playerIndex ^ 1]
                    .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                    .strokeCount +
                1;
            g_Players[playerIndex].strokeCount =
                g_Players[playerIndex]
                    .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                    .strokeCount -
                GetHandicapOffset(
                    g_GameState.courseIndex,
                    g_GameState.holeIndex,
                    g_Players[playerIndex].handicap
                );
            g_Players[playerIndex].holeStatus = HOLE_STATUS_OUT;
        }
        else {
            g_Players[playerIndex].strokeCount = g_HoleSetup.par * GIVE_UP_PAR_MULTIPLE;
            g_Players[playerIndex].holes[g_GameState.holeIndex % COURSE_HOLE_COUNT].strokeCount =
                g_HoleSetup.par * GIVE_UP_PAR_MULTIPLE;
            g_Players[playerIndex].holeStatus = HOLE_STATUS_OUT;
        }

        SkipUnmeasuredDistances(playerIndex);
        UpdateContest(FALSE);
        if (g_GameState.replayKind == REPLAY_KIND_NONE) {
            if (IsOneOnOneMatchMode(g_GameState.gameMode)) {
                opponentIndex = playerIndex ^ 1;
                g_Players[opponentIndex].holeStatus = HOLE_STATUS_HOLED_OUT;
                g_Players[opponentIndex]
                    .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                    .strokeCount = g_Players[playerIndex]
                                       .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                                       .strokeCount -
                    1;
                g_Players[opponentIndex].strokeCount =
                    g_Players[opponentIndex]
                        .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                        .strokeCount -
                    GetHandicapOffset(
                        g_GameState.courseIndex,
                        g_GameState.holeIndex,
                        g_Players[opponentIndex].handicap
                    );
                SkipUnmeasuredDistances(opponentIndex);
            }
        }
    }

    // Shows or hides the HUD's parts, apart from the club panel and the letterbox
    void SetRoundHudPartsShown(Boolean8 isShown) {
        g_RoundHud.isWindIndicatorShown = isShown;
        g_RoundHud.isHoleInfoShown = isShown;
        g_RoundHud.isLieIconShown = isShown;
        g_RoundHud.isClubHeadIconShown = isShown;
        g_RoundHud.isUnreadItemShown = isShown;
        g_RoundHud.isProgressShown = isShown;
    }

    // Returns whether a shot stopped this far from the impact mark rumbles, as a
    // mishit
    Boolean DoesImpactRumble(void) {
        return GetImpactTimingEntry(
            GetSelectedClub(),
            g_CurrentShot.surface,
            g_CurrentShot.shotMode,
            f_ShotLaunch.impactTiming
        )
            ->rumbles;
    }

    // Plays the impact click: a mishit, a stop right on the mark, or one off it
    void PlayImpactClickSound(void) {
        if (DoesImpactRumble()) {
            PlaySound(SOUND_MENU_BLOCKED, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
        }
        else if (f_ShotLaunch.impactTiming == 0) {
            PlaySound(SOUND_IMPACT_ON_MARK, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
        }
        else {
            PlaySound(SOUND_IMPACT_OFF_MARK, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
        }
    }

    areCallsAllowed = TRUE;
    if (IsGameFlagSet(GAME_FLAG_SHOW_COURSE_POINTS)) {
        PrintCoursePoints();
    }

    f_IsShotPreviewBuilt = FALSE;
    if ((g_LetterboxState == LETTERBOX_STATE_NONE ||
            g_LetterboxState == LETTERBOX_STATE_UNUSED_AIMING) == FALSE) {
        SetScreenEffect(SCREEN_EFFECT_NONE, 0);
    }

    if (IsGameFlagSet(GAME_FLAG_SHOW_PLAYER_STATES)) {
        for (debugPlayerIndex = 0; debugPlayerIndex < g_GameState.playerCount; debugPlayerIndex++) {
            PrintDebugPaddedHex(g_Players[debugPlayerIndex].state, 2, debugPlayerIndex + 10);
        }
    }

    PlayAmbientSound();
    ChooseNextPlayer();
    for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
        s32 controllerIndex = g_Players[playerIndex].controllerIndex;

        state = g_Players[playerIndex].state;
        if (controllerIndex < MAXCONTROLLERS) {
            g_TurnController = g_ControllerStates[controllerIndex];
        }
        else {
            g_TurnController.stickX = 0;
            g_TurnController.stickY = 0;
            g_TurnController.buttons = 0;
            g_TurnController.repeatedButtons = 0;
            g_TurnController.pressedButtons = 0;
        }

        f_StateFrameCounts[playerIndex]++;
        nextState = state;
        f_ShotLaunch.playerIndex = playerIndex;
        if (IsTimedState(state) && IsPlayGoingOn(g_PauseScreen)) {
            TickStopwatches();
        }

        if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
            g_RoundHud.stopwatchFrameCount = g_Players[playerIndex].stopwatchFrameCount;
        }

        switch (state) {
            case PLAYER_STATE_STARTING_TURN:
                f_ShouldUpdateReaction = FALSE;
                UnloadOverlay(GetOverlayOfModule(MODULE_SHOT_SIMULATION));
                f_ShotFlightCount = 0;
                f_GreenSongTempo = AIMING_SONG_TEMPO;
                UpdateContest(FALSE);
                ResetShotSetup();
                g_RoundHud.flags = 0;
                g_RoundHud.isBallTrailShown = FALSE;
                g_RoundHud.shooterIndex = playerIndex;
                ResetShotOffsetMarker();
                if (IsCharacterSlotBusy(0)) {
                    FreeCharacterAndReactionLight(0);
                    break;
                }

                LoadCharacter(
                    0,
                    g_Players[playerIndex].characterIndex,
                    g_Players[playerIndex].paletteIndex
                );
                if (g_Players[playerIndex].isLefty) {
                    SetTextureAnimationFrame(0, 1, 1);
                    SetCharacterFrontCulling(0, TRUE);
                }

                SetCharacterLighting(0, 1);
                ClearGameFlag(GAME_FLAG_FAST_FLIGHT);
                StartPlayerShot(playerIndex);
                f_ShotLaunch.heading = g_CurrentShot.heading;
                g_RoundHud.clubHeadTilt = GetAimSlopeAngle(&g_Players[playerIndex], &f_ShotLaunch);
                g_RoundHud.lieTilt = GetCrossSlopeAngle(&g_Players[playerIndex], &f_ShotLaunch);
                UpdateShotFlags();
                PlaceBallAtRest(playerIndex, &g_TurnBallSimulation);
                g_CurrentShot.cupDistance = (f32)GetCupDistance(
                    g_TurnBallSimulation.position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
                    g_TurnBallSimulation.position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]
                );
                g_RoundHud.greenGridState = GREEN_GRID_SHOWN;
                g_RoundHud.isDistanceSignShown = TRUE;
                g_RoundHud.isPinFlagShown = TRUE;
                f_ShotLaunch.clubSlot = g_CurrentShot.clubSlot;
                f_ShotLaunch.shotMode = g_CurrentShot.shotMode;
                g_ShotSetupFrameCount = 0;
                SetCharacterClub(0, g_Players[playerIndex].clubs[g_CurrentShot.clubSlot]);
                if (g_CurrentShot.shotMode == SHOT_MODE_APPROACH) {
                    SetSwingAnimation(
                        0,
                        g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
                        SHOT_MODE_APPROACH
                    );
                    SetAnimationSpeed(0, 0.0f);
                }
                else {
                    SetStanceAnimation(
                        0,
                        g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
                        g_CurrentShot.shotMode
                    );
                }

                PlaceCharacterForShot(0);
                UpdateGolferTranslucency();
                if (g_GameState.replayKind != REPLAY_KIND_NONE) {
                    if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
                        LoadRingShotReplayInputs(g_Players[playerIndex].strokeCount);
                    }
                    else {
                        LoadBestShotReplayInputs(g_Players[playerIndex].strokeCount);
                    }

                    f_ShotSource = SHOT_SOURCE_PREPARED;
                }
                else if (IsGameFlagSet(GAME_FLAG_FAST_AUTOPLAY) ||
                    g_Players[playerIndex].isComputer ||
                    IsGameFlagSet(GAME_FLAG_AUTOPLAY)) {
                    g_Players[playerIndex].ignoredPlanMark = -9;
                    LoadOverlay(GetOverlayOfModule(MODULE_COMPUTER_GOLFER));
                    StartShotPlan();
                    g_UnusedLaunchCount = 0;
                    f_PlanTimeBudget = PLAN_START_TIME_BUDGET;
                    f_ShotSource = SHOT_SOURCE_PLANNING;
                }
                else if (f_ShouldReplaySavedShot) {
                    f_ShotSource = SHOT_SOURCE_PREPARED;
                    g_ScriptedShot = g_SavedShots[playerIndex];
                    f_ShouldReplaySavedShot = FALSE;
                }
                else {
                    f_ShotSource = SHOT_SOURCE_PLAYER;
                }

                f_AimFrameCount = 0;
                f_ReplayAimSettledFrameCount = 0;
                if (g_GameState.replayKind == REPLAY_KIND_NONE &&
                    IsOneOnOneMatchMode(g_GameState.gameMode)) {
                    ScoreToPar scoreToPar = GetNetScoreToPar(g_GameState.holeIndex, playerIndex);

                    g_RoundHud.skinsTotals[playerIndex] =
                        scoreToPar - GetNetScoreToPar(g_GameState.holeIndex, playerIndex ^ 1);
                }
                else {
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
                        if (GetSurfaceTraits(g_CurrentShot.surface)->kind == SURFACE_KIND_GREEN) {
                            StartMovie(MOVIE_SINK_TO_WIN);
                        }

                        break;
                }
                {
                    SongId songId = SONG_ROUND_REPLAY;

                    if (g_GameState.replayKind != REPLAY_KIND_NONE) {
                        PlaySong(songId, SONG_FLAG_FOLLOWS_MUSIC_OPTION | SONG_FLAG_UNREAD);
                    }
                    else if (GetSurfaceTraits(g_CurrentShot.surface)->kind == SURFACE_KIND_GREEN) {
                        songId = SONG_MINI_GOLF;
                        if (g_GameState.isMiniGolf) {
                            PlaySong(songId, SONG_FLAG_FOLLOWS_MUSIC_OPTION | SONG_FLAG_UNREAD);
                        }
                        else if (g_GameState.gameMode != GAME_MODE_SPEED_GOLF) {
                            if (g_GameState.gameMode != GAME_MODE_RING_SHOT) {
                                // The tests below take this as the hole's score to par, one
                                // stroke more than the count. Subtracting par less one from
                                // the count gives 83 words differing, 45 in shape, against
                                // 77 and 43
                                ScoreToPar scoreToPar =
                                    (g_Players[playerIndex].strokeCount + 1) - g_HoleSetup.par;

                                if (scoreToPar > SCORE_TO_PAR_PAR) {
                                    PlaySong(
                                        SONG_CUP_IN_OVER_PAR,
                                        SONG_FLAG_FOLLOWS_MUSIC_OPTION | SONG_FLAG_UNREAD
                                    );
                                    osSyncPrintf("BOGEY\n");
                                }
                                else if (scoreToPar < SCORE_TO_PAR_EAGLE) {
                                    PlaySong(
                                        SONG_CUP_IN_UNDER_PAR,
                                        SONG_FLAG_FOLLOWS_MUSIC_OPTION | SONG_FLAG_UNREAD
                                    );
                                    osSyncPrintf("EAGLE\n");
                                    if (GetHolingOutStakes(playerIndex) == HOLING_OUT_STAKES_NONE &&
                                        !g_GameState.isMiniGolf) {
                                        StartMovie(MOVIE_ALBATROSS_PUTT);
                                    }
                                }
                                else if (scoreToPar < SCORE_TO_PAR_BIRDIE) {
                                    PlaySong(
                                        SONG_CUP_IN_UNDER_PAR,
                                        SONG_FLAG_FOLLOWS_MUSIC_OPTION | SONG_FLAG_UNREAD
                                    );
                                    osSyncPrintf("EAGLE\n");
                                    if (GetHolingOutStakes(playerIndex) == HOLING_OUT_STAKES_NONE &&
                                        !g_GameState.isMiniGolf) {
                                        StartMovie(MOVIE_EAGLE_PUTT);
                                    }
                                }
                                else if (scoreToPar == SCORE_TO_PAR_PAR) {
                                    PlaySong(
                                        SONG_CUP_IN_PAR,
                                        SONG_FLAG_FOLLOWS_MUSIC_OPTION | SONG_FLAG_UNREAD
                                    );
                                    osSyncPrintf("PAR\n");
                                }
                                else {
                                    PlaySong(
                                        SONG_CUP_IN_UNDER_PAR,
                                        SONG_FLAG_FOLLOWS_MUSIC_OPTION | SONG_FLAG_UNREAD
                                    );
                                    osSyncPrintf("BIRDIE\n");
                                    if (GetHolingOutStakes(playerIndex) == HOLING_OUT_STAKES_NONE &&
                                        !g_GameState.isMiniGolf) {
                                        StartMovie(MOVIE_BIRDIE_PUTT);
                                    }
                                }
                            }
                        }
                    }
                    else {
                        songId = SONG_MINI_GOLF;
                        if (!g_GameState.isMiniGolf) {
                            switch (g_GameState.gameMode) {
                                case GAME_MODE_RING_SHOT:
                                    songId = SONG_RING_SHOT;
                                    break;
                                case GAME_MODE_SPEED_GOLF:
                                    songId = SONG_SPEED_GOLF;
                                    break;
                                case GAME_MODE_TOURNAMENT:
                                    switch (g_GameState.courseIndex) {
                                        case COURSE_TOAD_HIGHLANDS:
                                            songId = SONG_TOURNAMENT_TOAD_HIGHLANDS;
                                            break;
                                        case COURSE_KOOPA_PARK:
                                            songId = SONG_TOURNAMENT_KOOPA_PARK;
                                            break;
                                        case COURSE_SHY_GUY_DESERT:
                                            songId = SONG_TOURNAMENT_SHY_GUY_DESERT;
                                            break;
                                        case COURSE_YOSHIS_ISLAND:
                                            songId = SONG_TOURNAMENT_YOSHIS_ISLAND;
                                            break;
                                        case COURSE_BOO_VALLEY:
                                            songId = SONG_TOURNAMENT_BOO_VALLEY;
                                            break;
                                        case COURSE_MARIOS_STAR:
                                        default:
                                            songId = SONG_TOURNAMENT_MARIOS_STAR;
                                            break;
                                    }
                                    break;
                                case GAME_MODE_TRAINING:
                                    songId = SONG_TRAINING;
                                    break;
                                default:
                                    if (GetLastHoleIndex() == g_GameState.holeIndex &&
                                        g_GameState.playerCount != 1) {
                                        songId = SONG_ROUND_FINAL;
                                    }
                                    else if (IsOneOnOneMatchMode(g_GameState.gameMode) &&
                                        GetMatchStanding(0, 1) != MATCH_STANDING_OPEN) {
                                        songId = SONG_ROUND_FINAL;
                                    }
                                    else if (IsYoshiAndDonkeyKongRound()) {
                                        songId = SONG_YOSHI_AND_DONKEY_KONG;
                                    }
                                    else {
                                        switch (g_GameState.courseIndex) {
                                            case COURSE_TOAD_HIGHLANDS:
                                                songId = SONG_COURSE_TOAD_HIGHLANDS;
                                                break;
                                            case COURSE_KOOPA_PARK:
                                                songId = SONG_COURSE_KOOPA_PARK;
                                                break;
                                            case COURSE_SHY_GUY_DESERT:
                                                songId = SONG_COURSE_SHY_GUY_DESERT;
                                                break;
                                            case COURSE_YOSHIS_ISLAND:
                                                songId = SONG_COURSE_YOSHIS_ISLAND;
                                                break;
                                            case COURSE_BOO_VALLEY:
                                                songId = SONG_COURSE_BOO_VALLEY;
                                                break;
                                            case COURSE_MARIOS_STAR:
                                            default:
                                                songId = SONG_COURSE_MARIOS_STAR;
                                                break;
                                        }
                                    }
                                    break;
                            }
                        }

                        PlaySong(songId, SONG_FLAG_FOLLOWS_MUSIC_OPTION | SONG_FLAG_UNREAD);
                    }
                }

                StartShotPanel();
                if (g_HasShotComeToRest) {
                    nextState = PLAYER_STATE_PREPARING_AIM;
                }
                else if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
                    nextState = PLAYER_STATE_HOLE_INTRO;
                    StartHoleIntroCamera();
                    ResetShotSetup();
                }
                else {
                    nextState = PLAYER_STATE_AIMING;
                }

                StartTurnRumble(g_GameState.currentPlayerIndex);
                break;
            case PLAYER_STATE_PREPARING_AIM:
                nextState = PLAYER_STATE_AIMING;
                f_ShotLaunch.clubSlot = g_CurrentShot.clubSlot;
                f_ShotLaunch.shotMode = g_CurrentShot.shotMode;
                f_ShotLaunch.heading = g_CurrentShot.heading;
                if (f_ShotSource == SHOT_SOURCE_PLANNING) {
                    preparingPollIndex = 0;
                    StartTurnTimer();
                    while (f_ShotSource == SHOT_SOURCE_PLANNING &&
                        preparingPollIndex < PLAN_PREPARING_POLL_COUNT) {
                        f_ShotSource =
                            (GetPlannedShot(&g_ScriptedShot) != PLANNED_SHOT_UNFINISHED) ?
                            SHOT_SOURCE_PREPARED :
                            SHOT_SOURCE_PLANNING;
                        if (!HasPlanTime(PLAN_PREPARING_TIME_BUDGET)) {
                            break;
                        }

                        preparingPollIndex++;
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

                TurnAim(FALSE);
                PlaceCharacterForShot(0);
                if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
                    SetFramebufferCount(TRIPLE_BUFFERING);
                    SetScreenEffect(SCREEN_EFFECT_NONE, 0);
                }

                break;
            case PLAYER_STATE_AIMING:
                f_SwingCount = 0;
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
                        RequestPauseMenu(g_Players[playerIndex].strokeCount != 0, FALSE);
                        HideRoundHud();
                    }
                }
                else if (g_PauseScreen == PAUSE_SCREEN_PAUSE_MENU &&
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
                    PauseAction choice = UpdatePauseMenu(&g_TurnController);

                    if (choice == PAUSE_ACTION_RETURN_TO_MENU) {
                        RequestPauseMenu(0, -1);
                        QuitPlayToTitle();
                    }
                    else if (choice == PAUSE_ACTION_START_OVER) {
                        RequestPauseMenu(0, -1);
                        ResumeRound();
                        TakePinOut();
                        ClearShotEffects(CLEAR_KIND_ALL);
                        PlacePlayersOnTee();
                        SetFramebufferCount(TRIPLE_BUFFERING);
                        nextState = PLAYER_STATE_WAITING;
                    }
                    else if (choice == PAUSE_ACTION_HOLE_OUT) {
                        RequestPauseMenu(0, -1);
                        StopFlightCamera();
                        ClearShotEffects(CLEAR_KIND_ALL);
                        FreeCharacterAndReactionLight(0);
                        nextState = PLAYER_STATE_HOLE_DONE;
                    }
                }
                else if (g_PauseScreen == PAUSE_SCREEN_LOADING_SCORECARD) {
                    if (g_IsModuleAlive[MODULE_SCORECARD]) {
                        g_PauseScreen = PAUSE_SCREEN_SCORECARD;
                        OpenScorecard(g_GameState.courseIndex, g_GameState.holeIndex);
                        if (g_GameState.gameMode == GAME_MODE_TOURNAMENT) {
                            SetScorecardLayout(SCORECARD_LAYOUT_TOURNAMENT);
                        }
                        else if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
                            SetScorecardLayout(SCORECARD_LAYOUT_SPEED_GOLF);
                        }
                        else if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                            g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
                            SetScorecardLayout(SCORECARD_LAYOUT_SKINS);
                        }
                        else if (IsOneOnOneMatchMode(g_GameState.gameMode)) {
                            SetScorecardLayout(SCORECARD_LAYOUT_MATCH);
                        }
                        else {
                            SetScorecardLayout(SCORECARD_LAYOUT_STROKE);
                        }
                    }
                }
                else if (g_PauseScreen == PAUSE_SCREEN_SCORECARD) {
                    UpdateScorecard(
                        g_MergedController.repeatedButtons,
                        g_MergedController.pressedButtons
                    );
                }
                else if (g_PauseScreen == PAUSE_SCREEN_LOADING_SAVE_MENU) {
                    if (g_IsModuleAlive[MODULE_SAVE_MENU]) {
                        g_PauseScreen = PAUSE_SCREEN_SAVE_MENU;
                        OpenSaveMenu(SAVE_MENU_GAMES);
                    }
                }
                else if (g_PauseScreen == PAUSE_SCREEN_LOADING_CONTROLS) {
                    if (g_IsModuleAlive[MODULE_SAVE_MENU]) {
                        g_PauseScreen = PAUSE_SCREEN_CONTROLS;
                        OpenControlsScreen();
                    }
                }
                else if (g_PauseScreen == PAUSE_SCREEN_CONTROLS) {
                    UpdateControlsScreen(&g_TurnController);
                }
                else if (g_PauseScreen == PAUSE_SCREEN_SAVE_MENU) {
                    UpdateGameSaveMenu(&g_TurnController);
                }
                else if (g_PauseScreen == PAUSE_SCREEN_UNUSED) {
                }
                else if (g_PauseScreen == PAUSE_SCREEN_PROMPT) {
                    switch (UpdatePrompt(&g_TurnController)) {
                        case PROMPT_RESULT_GIVE_UP_HOLE:
                            g_HasShotComeToRest = TRUE;
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
                            QuitPlayToTitle();
                            break;
                        case PROMPT_RESULT_SAVE_GAME:
                            SaveGameToSlot();
                            g_PauseScreen = PAUSE_SCREEN_SAVE_MENU;
                            break;
                        case PROMPT_RESULT_END_GAME:
                            LeaveGameSaveMenu();
                        // Falls through
                        case PROMPT_RESULT_STAY_IN_SAVE_MENU:
                            g_PauseScreen = PAUSE_SCREEN_SAVE_MENU;
                            break;
                        case PROMPT_RESULT_QUIT_GAME:
                            g_PauseScreen = PAUSE_SCREEN_NONE;
                            ResumeRound();
                            QuitPlayToTitle();
                            break;
                    }
                }
                else if (g_PauseScreen == PAUSE_SCREEN_FREE_LOOK) {
                    if (g_TurnController.pressedButtons & (A_BUTTON | R_TRIG)) {
                        g_PauseScreen = PAUSE_SCREEN_NONE;
                        SetMenuBackdrop(MENU_BACKDROP_NONE);
                        SetFramebufferCount(TRIPLE_BUFFERING);
                        SetScreenEffect(SCREEN_EFFECT_NONE, 0);
                        g_LetterboxState = LETTERBOX_STATE_NONE;
                        PlaySound(SOUND_LOOK_BACK, g_NextSoundChannel, LOOK_SOUND_VOLUME);
                        ResetAimCamera();
                        ResetShotSetup();
                        g_RoundHud.isClubPanelShown = TRUE;
                        g_RoundHud.isLetterboxShown = TRUE;
                        SetRoundHudPartsShown(1);
                    }
                    else {
                        if (g_TurnController.buttons & START_BUTTON) {
                            SetRoundHudPartsShown(0);
                        }
                        else {
                            SetRoundHudPartsShown(1);
                        }
                        UpdateClubControls();
                        UpdateShotModeControls();
                        PoseGolferForShot();
                        f_ShotLaunch.shotMode = g_CurrentShot.shotMode;
                        UpdateAimControls();
                        PlaceShotCamera();
                        SetFramebufferCount(TRIPLE_BUFFERING);
                        SetScreenEffect(SCREEN_EFFECT_NONE, 0);
                    }

                    f_ShotLaunch.offsetX = 0;
                    f_ShotLaunch.offsetY = 0;
                    f_ShotLaunch.heading = g_CurrentShot.heading;
                    BuildShotPreview(&f_ShotLaunch, state);
                }
                else if (g_PauseScreen == PAUSE_SCREEN_NONE) {
                    Boolean isReady;

                    if (f_ShotSource == SHOT_SOURCE_PLAYER &&
                        (g_TurnController.buttons & (Z_TRIG | L_TRIG)) &&
                        (g_TurnController.pressedButtons & R_TRIG)) {
                        StartFreeLook();
                        BuildShotPreview(&f_ShotLaunch, state);
                        break;
                    }

                    SetCharacterSlotAlpha(
                        0,
                        (sinf(g_Players[playerIndex].stopwatchFrameCount * AIMING_ALPHA_RATE) *
                            AIMING_ALPHA_SWING) +
                            AIMING_MEAN_ALPHA
                    );
                    isReady = TRUE;
                    if (GetSurfaceTraits(g_CurrentShot.surface)->kind == SURFACE_KIND_GREEN &&
                        !g_GameState.isMiniGolf) {
                        f_GreenSongTempo += GREEN_SONG_TEMPO_STEP;
                        if (f_GreenSongTempo > GREEN_SONG_TEMPO_MAX) {
                            f_GreenSongTempo = GREEN_SONG_TEMPO_MAX;
                        }

                        if (IsGameFlagSet(GAME_FLAG_SHOW_SWING_TEMPO)) {
                            sprintf(g_PrintBuffer, "tempo:%5.2f", f_GreenSongTempo);
                            PrintDebugText(
                                g_PrintBuffer,
                                SWING_TEMPO_TEXT_COLUMN,
                                SWING_TEMPO_TEXT_ROW
                            );
                        }

                        SetSongTempo(f_GreenSongTempo);
                    }
                    else {
                        SetSongTempo(AIMING_SONG_TEMPO);
                    }

                    if (f_ShotSource == SHOT_SOURCE_PLANNING) {
                        aimingPollIndex = 0;
                        if (g_ScreenEffect != SCREEN_EFFECT_SAVED_FRAME) {
                            SetScreenEffect(SCREEN_EFFECT_SAVED_FRAME, SAVED_FRAME_STEP_SAVING);
                            SetFramebufferCount(DOUBLE_BUFFERING);
                        }

                        StartTurnTimer();
                        while (f_ShotSource == SHOT_SOURCE_PLANNING &&
                            aimingPollIndex < PLAN_AIMING_POLL_COUNT) {
                            f_ShotSource =
                                (GetPlannedShot(&g_ScriptedShot) != PLANNED_SHOT_UNFINISHED) ?
                                SHOT_SOURCE_PREPARED :
                                SHOT_SOURCE_PLANNING;
                            if (!HasPlanTime(f_PlanTimeBudget)) {
                                break;
                            }

                            aimingPollIndex++;
                        }

                        f_PlanTimeBudget *= PLAN_TIME_BUDGET_GROWTH;
                        if (f_PlanTimeBudget > PLAN_TIME_BUDGET_MAX) {
                            f_PlanTimeBudget = PLAN_TIME_BUDGET_MAX;
                        }

                        if (f_ShotSource == SHOT_SOURCE_PLANNING) {
                            break;
                        }
                    }

                    if (f_ShotSource != SHOT_SOURCE_PLAYER &&
                        f_AimFrameCount < PREPARED_SHOT_AIM_FRAMES_MIN) {
                        break;
                    }

                    UnloadOverlay(GetOverlayOfModule(MODULE_COMPUTER_GOLFER));
                    if (f_ShotSource == SHOT_SOURCE_PREPARED) {
                        isReady = StepToClubChoice(&g_ScriptedShot);
                    }
                    else {
                        if (g_ImpactPointState == IMPACT_POINT_OFF) {
                            UpdateClubControls();
                        }

                        UpdateShotModeControls();
                    }

                    PoseGolferForShot();
                    f_StateEventFrame = GetAnimationEventFrame(
                        0,
                        GetAnimationIndex(
                            0,
                            GetSwingAnimation(
                                0,
                                g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
                                g_CurrentShot.shotMode
                            )
                        ),
                        ANIMATION_EVENT_IMPACT
                    );
                    f_IsSwingAnimationStarted = FALSE;
                    if (IsGameFlagSet(GAME_FLAG_CHOOSE_CUP)) {
                        // Debug flag 0x47: prints where the ball lies, and the second
                        // controller moves it while Z is held. In its own block, which
                        // frees the buffer's slot when the flag's test ends, so it shares
                        // one slot with the other debug buffers, as in the original's
                        // frame; declared in the flag's block, the frame grows from the
                        // original's 0x118 bytes to 0x1A0 and 170 words differ (36 in
                        // shape), and in a do-while 82 (36), against 68 (36).
                        {
                            char text[DEBUG_TEXT_BUFFER_SIZE];
                            Boolean isMovingBall;

                            f_UnusedDebugCourseIndex = g_GameState.courseIndex;
                            f_UnusedDebugHoleIndex = g_GameState.holeIndex;
                            SetGameFlag(GAME_FLAG_RANDOM_FRAMEBUFFER_COUNT);
                            sprintf(
                                text,
                                "X:%X Y:%X Z:%X  Attr%d             ",
                                g_CurrentShot.ballPosition[AXIS_X],
                                g_CurrentShot.ballPosition[AXIS_Y],
                                g_CurrentShot.ballPosition[AXIS_Z],
                                GetSurface(
                                    g_CurrentShot.ballPosition[AXIS_X],
                                    g_CurrentShot.ballPosition[AXIS_Z]
                                )
                            );
                            PrintDebugText(text, BALL_POSITION_TEXT_COLUMN, BALL_POSITION_TEXT_ROW);
                            ClearGameFlag(GAME_FLAG_RANDOM_FRAMEBUFFER_COUNT);
                            isMovingBall =
                                (g_ControllerData[DEBUG_CONTROLLER].button & Z_TRIG) != 0;
                            if (isMovingBall) {
                                g_CurrentShot.ballPosition[AXIS_X] +=
                                    g_ControllerStates[DEBUG_CONTROLLER].stickX *
                                    ((Abs(g_ControllerStates[DEBUG_CONTROLLER].stickX) *
                                         BALL_MOVE_SPEED_PER_STICK) +
                                        1);
                                g_CurrentShot.ballPosition[AXIS_Z] +=
                                    g_ControllerStates[DEBUG_CONTROLLER].stickY *
                                    ((Abs(g_ControllerStates[DEBUG_CONTROLLER].stickY) *
                                         BALL_MOVE_SPEED_PER_STICK) +
                                        1);
                                g_CurrentShot.ballPosition[AXIS_Y] = GetGroundHeight(
                                    g_CurrentShot.ballPosition[AXIS_X],
                                    g_CurrentShot.ballPosition[AXIS_Z]
                                );
                                g_Players[playerIndex].ballPosition[AXIS_X] =
                                    g_CurrentShot.ballPosition[AXIS_X];
                                g_Players[playerIndex].ballPosition[AXIS_Y] =
                                    g_CurrentShot.ballPosition[AXIS_Y];
                                g_Players[playerIndex].ballPosition[AXIS_Z] =
                                    g_CurrentShot.ballPosition[AXIS_Z];
                                PlaceBallAtRest(playerIndex, &g_TurnBallSimulation);
                            }
                        }
                    }

                    if (isReady) {
                        isReady = UpdateAimControls() && isReady;
                    }

                    f_ShotLaunch.heading = g_CurrentShot.heading;
                    PlaceCharacterForShot(0);
                    UpdateGolferTranslucency();
                    if (!(g_LetterboxState == LETTERBOX_STATE_NONE ||
                            g_LetterboxState == LETTERBOX_STATE_UNUSED_AIMING) &&
                        g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
                        SetScreenEffect(SCREEN_EFFECT_NONE, 0);
                    }

                    if (g_LetterboxState == LETTERBOX_STATE_NONE ||
                        g_LetterboxState == LETTERBOX_STATE_UNUSED_AIMING) {
                        if (g_CurrentShot.shotMode == SHOT_MODE_APPROACH) {
                            SetSwingAnimation(
                                0,
                                g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
                                SHOT_MODE_APPROACH
                            );
                            SetAnimationSpeed(0, 0.0f);
                        }
                        else {
                            SetAnimationSpeed(0, 1.0f);
                        }
                    }
                    else {
                        SetStanceAnimation(
                            0,
                            g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
                            g_CurrentShot.shotMode
                        );
                        SetAnimationSpeed(0, 0.0f);
                    }

                    if ((f_ShotSource != SHOT_SOURCE_PLAYER && isReady) ||
                        (f_ShotSource == SHOT_SOURCE_PLAYER && g_HasPressedShotButton == TRUE)) {
                        g_ShowsGroundWaves = FALSE;
                        if (g_CurrentShot.shotMode != SHOT_MODE_APPROACH) {
                            SkipToAnimationEnd(0);
                        }

                        g_CurrentShot.gaugeFrame = 0;
                        g_CurrentShot.powerGaugeFrame = 0;
                        g_CurrentShot.impactGaugeFrame = 0;
                        PlaySound(SOUND_IMPACT_OFF_MARK, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
                        SetFramebufferCount(DOUBLE_BUFFERING);
                        nextState = PLAYER_STATE_SETTING_POWER;
                        if (g_ScreenEffect != SCREEN_EFFECT_SAVED_FRAME) {
                            SetScreenEffect(SCREEN_EFFECT_SAVED_FRAME, SAVED_FRAME_STEP_SAVING);
                        }
                    }

                    g_RoundHud.clubHeadTilt =
                        GetAimSlopeAngle(&g_Players[playerIndex], &f_ShotLaunch);
                    g_RoundHud.lieTilt = GetCrossSlopeAngle(&g_Players[playerIndex], &f_ShotLaunch);
                    if (GetClub(f_ShotLaunch.clubSlot)->surfaceMask & CLUB_SURFACE_GREEN) {
                        g_RoundHud.isPuttingClubChosen = TRUE;
                    }
                    else {
                        g_RoundHud.isPuttingClubChosen = FALSE;
                    }

                    if (g_ImpactPointState == IMPACT_POINT_SETTING) {
                        if (f_ShotSource == SHOT_SOURCE_PLAYER) {
                            g_CurrentShot.offsetX = GetTurnStickX();
                            g_CurrentShot.offsetY = GetTurnStickY();
                        }
                        else {
                            g_CurrentShot.offsetX = 0;
                            g_CurrentShot.offsetY = 0;
                        }
                    }
                    else if (g_ImpactPointState == IMPACT_POINT_OFF) {
                        g_CurrentShot.offsetX = 0;
                        g_CurrentShot.offsetY = 0;
                    }

                    g_CurrentShot.gaugeFrame = 0;
                    if (IsGameFlagSet(GAME_FLAG_FAST_AUTOPLAY)) {
                        g_CurrentShot.gaugeFrame = g_ScriptedShot.powerGaugeFrame;
                    }

                    SetShotOffsetMarker(g_CurrentShot.offsetX, g_CurrentShot.offsetY);
                    f_ShotLaunch.offsetX = g_CurrentShot.offsetX;
                    f_ShotLaunch.offsetY = g_CurrentShot.offsetY;
                    PlaceShotCamera();
                    if (g_ShotFlags & SHOT_FLAG_ON_GREEN) {
                        if (g_LetterboxState == LETTERBOX_STATE_UNUSED_AIMING ||
                            g_LetterboxState == LETTERBOX_STATE_NONE) {
                            g_RoundHud.flags = HUD_FLAG_AIMING | HUD_FLAG_NO_IMPACT_POINT;
                        }
                        else {
                            g_RoundHud.flags = HUD_FLAG_AIMING | HUD_FLAG_NO_IMPACT_POINT |
                                HUD_FLAG_TRANSLUCENT_GAUGE;
                        }
                    }
                    else if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
                        if (g_LetterboxState == LETTERBOX_STATE_UNUSED_AIMING ||
                            g_LetterboxState == LETTERBOX_STATE_NONE) {
                            g_RoundHud.flags = HUD_FLAG_AIMING | HUD_FLAG_LIGHT_CLUB_HEAD |
                                HUD_FLAG_NO_IMPACT_POINT;
                        }
                        else {
                            g_RoundHud.flags = HUD_FLAG_AIMING | HUD_FLAG_LIGHT_CLUB_HEAD |
                                HUD_FLAG_NO_IMPACT_POINT | HUD_FLAG_TRANSLUCENT_GAUGE;
                        }
                    }
                    else {
                        if (g_LetterboxState == LETTERBOX_STATE_UNUSED_AIMING) {
                            g_RoundHud.flags = HUD_FLAG_AIMING | HUD_FLAG_ANIMATE_OFFSET_MARKER;
                        }
                        else if (g_LetterboxState == LETTERBOX_STATE_NONE) {
                            g_RoundHud.flags = HUD_FLAG_AIMING | HUD_FLAG_LIGHT_CLUB_HEAD;
                        }
                        else {
                            g_RoundHud.flags = HUD_FLAG_AIMING | HUD_FLAG_LIGHT_CLUB_HEAD |
                                HUD_FLAG_NO_IMPACT_POINT | HUD_FLAG_TRANSLUCENT_GAUGE;
                        }

                        if (g_ImpactPointState == IMPACT_POINT_SETTING) {
                            g_RoundHud.flags = (g_RoundHud.flags | HUD_FLAG_ANIMATE_OFFSET_MARKER) &
                                ~HUD_FLAG_LIGHT_CLUB_HEAD;
                        }
                    }

                    BuildShotPreview(&f_ShotLaunch, state);
                }

                break;
            case PLAYER_STATE_SETTING_POWER:
                UpdateImpactPointMode();
                ReuseLastCharacterMatrices(0);
                BuildShotPreview(&f_ShotLaunch, state);
                areCallsAllowed = FALSE;
                if (g_RoundHud.greenGridState == GREEN_GRID_SHOWN) {
                    g_RoundHud.greenGridState = GREEN_GRID_FADING;
                }

                g_RoundHud.isDistanceSignShown = FALSE;
                g_RoundHud.isPinFlagShown = FALSE;
                if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
                    g_RoundHud.flags = HUD_FLAG_SETTING_POWER;
                }
                else {
                    g_RoundHud.flags = HUD_FLAG_SETTING_POWER | HUD_FLAG_ANIMATE_OFFSET_MARKER;
                }

                if (f_ShotSource == SHOT_SOURCE_PLAYER) {
                    g_CurrentShot.offsetX = GetTurnStickX();
                    g_CurrentShot.offsetY = GetTurnStickY();
                }
                else {
                    g_CurrentShot.offsetX = 0;
                    g_CurrentShot.offsetY = 0;
                }

                SetShotOffsetMarker(g_CurrentShot.offsetX, g_CurrentShot.offsetY);
                f_ShotLaunch.offsetX = g_CurrentShot.offsetX;
                f_ShotLaunch.offsetY = g_CurrentShot.offsetY;
                if ((f_ShotSource != SHOT_SOURCE_PLAYER) ?
                        (g_ScriptedShot.powerGaugeFrame == g_CurrentShot.gaugeFrame) :
                        (g_TurnController.pressedButtons & A_BUTTON)) {
                    SoundId soundId;

                    g_CurrentShot.powerGaugeFrame = g_CurrentShot.gaugeFrame;
                    f_ShotLaunch.power = GetGaugeFramePower(g_CurrentShot.gaugeFrame);
                    g_SavedPlayers[playerIndex] = g_Players[playerIndex];
                    g_Players[playerIndex].strokeCount++;
                    if (g_Players[playerIndex].strokeCount > STROKE_COUNT_MAX) {
                        g_Players[playerIndex].strokeCount = STROKE_COUNT_MAX;
                    }

                    if (GetSelectedClub()->surfaceMask & CLUB_SURFACE_GREEN) {
                        if (GetSurfaceTraits(g_CurrentShot.surface)->kind == SURFACE_KIND_GREEN) {
                            g_Players[playerIndex].puttCount++;
                            if (g_Players[playerIndex].puttCount > STROKE_COUNT_MAX) {
                                g_Players[playerIndex].puttCount = STROKE_COUNT_MAX;
                            }
                        }

                        f_ShotLaunch.impactTiming = 0;
                        f_ShotLaunch.offsetX = 0;
                        f_ShotLaunch.offsetY = 0;
                        nextState = PLAYER_STATE_STARTING_SWING;
                        if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
                            SetScreenEffect(SCREEN_EFFECT_NONE, 0);
                        }

                        soundId = SOUND_IMPACT_OFF_MARK;
                        PlaySound(soundId, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
                    }
                    else {
                        nextState = PLAYER_STATE_SETTING_IMPACT;
                        if (f_ShotLaunch.power == LAUNCH_POWER_MAX) {
                            StartShotEffect(SHOT_EFFECT_MAX_POWER);
                            soundId = SOUND_IMPACT_ON_MARK;
                            PlaySound(soundId, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
                        }
                        else {
                            soundId = SOUND_IMPACT_OFF_MARK;
                            PlaySound(soundId, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
                        }
                    }
                }
                else if (g_CurrentShot.gaugeFrame >= LAST_POWER_GAUGE_FRAME ||
                    (f_ShotSource == SHOT_SOURCE_PLAYER &&
                        (g_TurnController.pressedButtons & B_BUTTON))) {
                    UpdateGolferTranslucency();
                    nextState = PLAYER_STATE_AIMING;
                    ResetShotModeChange();
                    PlaySound(SOUND_MENU_BLOCKED, g_NextSoundChannel, IMPACT_SOUND_VOLUME);
                    if (g_RoundHud.greenGridState != GREEN_GRID_HIDDEN) {
                        g_RoundHud.greenGridState = GREEN_GRID_SHOWN;
                    }

                    g_RoundHud.isDistanceSignShown = TRUE;
                    g_RoundHud.isPinFlagShown = TRUE;
                }

                g_CurrentShot.gaugeFrame++;
                break;
            case PLAYER_STATE_SETTING_IMPACT:
                UpdateImpactPointMode();
                ReuseLastCharacterMatrices(0);
                BuildShotPreview(&f_ShotLaunch, state);
                areCallsAllowed = FALSE;
                if (g_ShotFlags & SHOT_FLAG_NO_IMPACT_POINT) {
                    g_RoundHud.flags = HUD_FLAG_SETTING_IMPACT;
                }
                else {
                    g_RoundHud.flags = HUD_FLAG_SETTING_IMPACT | HUD_FLAG_ANIMATE_OFFSET_MARKER;
                }

                if (f_ShotSource == SHOT_SOURCE_PLAYER) {
                    g_CurrentShot.offsetX = GetTurnStickX();
                    g_CurrentShot.offsetY = GetTurnStickY();
                }
                else {
                    s32 framesLeft = g_ScriptedShot.impactGaugeFrame - g_CurrentShot.gaugeFrame -
                        STICK_HOLD_FRAME_COUNT;

                    if (framesLeft > STICK_TILT_FRAME_COUNT) {
                        g_CurrentShot.offsetX = 0;
                        g_CurrentShot.offsetY = 0;
                    }
                    else if (framesLeft <= 0) {
                        g_CurrentShot.offsetX = g_ScriptedShot.offsetX;
                        g_CurrentShot.offsetY = g_ScriptedShot.offsetY;
                    }
                    else {
                        g_CurrentShot.offsetX = g_ScriptedShot.offsetX *
                            (STICK_TILT_FRAME_COUNT - framesLeft) / STICK_TILT_FRAME_COUNT;
                        g_CurrentShot.offsetY = g_ScriptedShot.offsetY *
                            (STICK_TILT_FRAME_COUNT - framesLeft) / STICK_TILT_FRAME_COUNT;
                    }
                }

                SetShotOffsetMarker(g_CurrentShot.offsetX, g_CurrentShot.offsetY);
                f_ShotLaunch.offsetX = g_CurrentShot.offsetX;
                f_ShotLaunch.offsetY = g_CurrentShot.offsetY;
                if (((f_ShotSource != SHOT_SOURCE_PLAYER) ?
                            (g_CurrentShot.gaugeFrame == g_ScriptedShot.impactGaugeFrame) :
                            (g_TurnController.pressedButtons & A_BUTTON)) ||
                    g_CurrentShot.gaugeFrame >= LAST_IMPACT_GAUGE_FRAME) {
                    g_CurrentShot.impactGaugeFrame = g_CurrentShot.gaugeFrame;
                    f_ShotLaunch.impactTiming = g_CurrentShot.gaugeFrame - GAUGE_IMPACT_FRAME;
                    if (f_ShotLaunch.impactTiming > IMPACT_TIMING_MAX) {
                        f_ShotLaunch.impactTiming = IMPACT_TIMING_MAX;
                    }

                    if (f_ShotLaunch.impactTiming < -IMPACT_TIMING_MAX) {
                        f_ShotLaunch.impactTiming = -IMPACT_TIMING_MAX;
                    }

                    PlayImpactClickSound();
                    StopSound(IMPACT_POINT_SOUND_CHANNEL);
                    nextState = PLAYER_STATE_STARTING_SWING;
                    if (g_CurrentShot.shotMode == SHOT_MODE_POWER &&
                        (f_ShotLaunch.impactTiming != 0 || f_ShotLaunch.power < LAUNCH_POWER_MAX)) {
                        if (g_Players[playerIndex].powerShotCount > 0) {
                            g_Players[playerIndex].powerShotCount--;
                        }
                    }

                    if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
                        SetScreenEffect(SCREEN_EFFECT_NONE, 0);
                    }
                }
                else {
                    g_CurrentShot.gaugeFrame++;
                    if (SWING_TIMING_GAUGE_FRAME - g_CurrentShot.gaugeFrame < f_StateEventFrame &&
                        !f_IsSwingAnimationStarted) {
                        f_StateFrameCounts[playerIndex] = 0;
                        f_IsSwingAnimationStarted = TRUE;
                        SetCharacterAnimation(
                            0,
                            GetSwingAnimation(
                                0,
                                g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
                                g_CurrentShot.shotMode
                            )
                        );
                        SetAnimationSpeed(0, 1.0f);
                        SetPlaysRandomTextureAnimation(0, FALSE);
                    }
                }

                if (g_IsDebugEnabled &&
                    (g_TurnController.pressedButtons & (B_BUTTON | D_CBUTTONS))) {
                    g_CurrentShot.impactGaugeFrame = g_CurrentShot.gaugeFrame;
                    // B gives full power and on-mark timing, C-down on-mark timing
                    // alone. With one store after the B test, the first CSE pass
                    // folds the clamp's upper test below and the second its lower
                    // test, where the original loads the timing back for both. A
                    // store in each arm keeps a join between the stores and the
                    // clamp until cross-jumping merges them: 77 words differ, 43 in
                    // shape, against 91 and 57, or 101 and 59 with the one store
                    // before the B test.
                    if (g_TurnController.pressedButtons & B_BUTTON) {
                        f_ShotLaunch.power = LAUNCH_POWER_MAX;
                        f_ShotLaunch.impactTiming = 0;
                    }
                    else {
                        f_ShotLaunch.impactTiming = 0;
                    }
                    if (f_ShotLaunch.impactTiming > IMPACT_TIMING_MAX) {
                        f_ShotLaunch.impactTiming = IMPACT_TIMING_MAX;
                    }

                    if (f_ShotLaunch.impactTiming < -IMPACT_TIMING_MAX) {
                        f_ShotLaunch.impactTiming = -IMPACT_TIMING_MAX;
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
                ShotMode shotMode;

                EndRumbleForceStop();
                ReuseLastCharacterMatrices(0);
                g_RoundHud.flags = HUD_FLAG_SWINGING;
                f_SwingCount++;
                g_SavedShots[playerIndex] = g_CurrentShot;
                SetCharacterTranslucent(0, FALSE);
                SetFramebufferCount(TRIPLE_BUFFERING);
                PlaceBallAtRest(playerIndex, &g_TurnBallSimulation);
                LoadOverlay(GetOverlayOfModule(MODULE_SHOT_SIMULATION));
                SimulateShot(&f_ShotLaunch);
                if (IsRecordableHoleOut(playerIndex)) {
                    RecordHoleOut(playerIndex);
                }

                if (IsRingShotRound()) {
                    RecordRingShotHole(playerIndex);
                }

                if (IsGameFlagSet(GAME_FLAG_FAST_AUTOPLAY)) {
                    SetGameFlag(GAME_FLAG_FAST_FLIGHT);
                }

                f_ImpactDelay = 0;
                f_PuttSlowdownLead = 0;
                if (!f_IsSwingAnimationStarted) {
                    f_StateFrameCounts[playerIndex] = 0;
                    if (GetSelectedClub()->surfaceMask & CLUB_SURFACE_GREEN) {
                        s32 power = f_ShotLaunch.power;

                        if (f_ShotLaunch.shotMode == PUTT_MODE_LONG) {
                            power *= LONG_PUTT_REACH_SCALE;
                        }

                        if (f_ShotLaunch.shotMode == PUTT_MODE_MIDDLE) {
                            power *= MIDDLE_PUTT_REACH_SCALE;
                        }

                        if (power >= MIDDLE_PUTT_SWING_POWER) {
                            shotMode = PUTT_MODE_MIDDLE;
                        }
                        else {
                            shotMode = PUTT_MODE_SHORT;
                            if (power < SLOW_PUTT_SWING_POWER) {
                                f32 speed = (f32)GetPuttPower(power, PUTT_MODE_SHORT) /
                                    GetPuttPower(SLOW_PUTT_SWING_POWER, PUTT_MODE_SHORT);

                                f_PuttSlowdownLead = PUTT_SLOWDOWN_LEAD_FRAMES;
                                f_PuttSwingSpeed = speed;
                                f_ImpactDelay =
                                    (PUTT_SLOWDOWN_LEAD_FRAMES / speed) - PUTT_SLOWDOWN_LEAD_FRAMES;
                            }
                        }

                        if (!IsReplayingShot()) {
                            f_StateEventFrame = GetAnimationEventFrame(
                                0,
                                GetAnimationIndex(
                                    0,
                                    GetSwingAnimation(
                                        0,
                                        g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
                                        shotMode
                                    )
                                ),
                                ANIMATION_EVENT_IMPACT
                            );
                        }
                    }
                    else {
                        shotMode = g_CurrentShot.shotMode;
                    }

                    SetCharacterAnimation(
                        0,
                        GetSwingAnimation(
                            0,
                            g_Players[playerIndex].clubs[g_CurrentShot.clubSlot],
                            shotMode
                        )
                    );
                    SetPlaysRandomTextureAnimation(0, FALSE);
                    if (f_SwingCount > FIRST_REPLAY_SWING_COUNT) {
                        if (f_SwingCount == FIRST_REPLAY_SWING_COUNT + 1) {
                            if (f_StateEventFrame > SKIPPED_SWING_IMPACT_FRAME) {
                                f_SkippedSwingFrames =
                                    f_StateEventFrame - SKIPPED_SWING_IMPACT_FRAME;
                                f_StateEventFrame -= f_SkippedSwingFrames;
                            }
                            else {
                                f_SkippedSwingFrames = 0;
                            }
                        }

                        AdvanceAnimation(0, f_SkippedSwingFrames);
                    }
                }

                f_IsSwingAnimationStarted = FALSE;
                SetAnimationSpeed(0, 1.0f);
                nextState = PLAYER_STATE_SWINGING;
                StartFlightCamera(f_ShotFlightCount++);
                f_IsReplayRequested = FALSE;
                break;
            }
            case PLAYER_STATE_SWINGING:
                if (f_StateEventFrame - WOOD_SWING_TRAIL_LEAD_FRAMES ==
                        f_StateFrameCounts[playerIndex] &&
                    GetClub(g_Players[playerIndex].clubs[g_CurrentShot.clubSlot])->category ==
                        CLUB_CATEGORY_WOOD) {
                    if (g_CurrentShot.shotMode == SHOT_MODE_NORMAL) {
                        StartSwingTrail(SWING_TRAIL_KIND_NORMAL_SHOT);
                    }
                    else if (g_CurrentShot.shotMode == SHOT_MODE_POWER) {
                        StartSwingTrail(SWING_TRAIL_KIND_POWER_SHOT);
                    }
                }

                ReuseLastCharacterMatrices(0);
                if (f_PuttSlowdownLead != 0 &&
                    f_StateFrameCounts[playerIndex] + f_PuttSlowdownLead == f_StateEventFrame) {
                    SetAnimationSpeed(0, f_PuttSwingSpeed);
                }

                if (f_StateFrameCounts[playerIndex] >= f_StateEventFrame + f_ImpactDelay ||
                    (IsGameFlagSet(GAME_FLAG_FAST_FLIGHT) &&
                        f_StateFrameCounts[playerIndex] >= FAST_FLIGHT_LAUNCH_FRAME)) {
                    if (IsBButtonHeld()) {
                        f_IsReplayRequested = TRUE;
                    }

                    if (g_IsModuleAlive[MODULE_GOLF_BALLS] ||
                        ReportModuleNotAlive(MODULE_GOLF_BALLS)) {
                        ResetBallRotation();
                    }

                    LaunchBallSimulation(&g_TurnBallSimulation, 0, &f_ShotLaunch);
                    nextState = PLAYER_STATE_BALL_IN_FLIGHT;
                    if (f_ShotLaunch.impactTiming == 0 &&
                        f_ShotLaunch.power >= LAUNCH_POWER_MAX &&
                        !(GetClub(f_ShotLaunch.clubSlot)->surfaceMask & CLUB_SURFACE_GREEN) &&
                        RateShotLength() == SHOT_LENGTH_FULL) {
                        StartBanner(BANNER_NICE_SHOT);
                    }
                }
                else {
                    UpdateFlightCamera(FALSE, g_TurnController.buttons);
                    break;
                }
            case PLAYER_STATE_BALL_IN_FLIGHT: {
                Boolean isCameraDone;
                s32 previousPosition[AXIS_COUNT];

                if (f_StateEventFrame + SWING_TRAIL_FADE_DELAY == f_StateFrameCounts[playerIndex]) {
                    StartSwingTrail(SWING_TRAIL_KIND_FADE);
                }

                ReuseLastCharacterMatrices(0);
                previousPosition[AXIS_X] = g_TurnBallSimulation.position[AXIS_X];
                previousPosition[AXIS_Y] = g_TurnBallSimulation.position[AXIS_Y];
                previousPosition[AXIS_Z] = g_TurnBallSimulation.position[AXIS_Z];
                StepBallSimulation(&g_TurnBallSimulation);
                OnBallStepped(&g_Players[playerIndex]);
                isCameraDone = UpdateFlightCamera(TRUE, g_TurnController.buttons);
                if (f_StateFrameCounts[playerIndex] == RANDOM_TEXTURE_RESUME_FRAME) {
                    SetPlaysRandomTextureAnimation(0, TRUE);
                }

                if (IsBButtonHeld()) {
                    f_IsReplayRequested = TRUE;
                }

                if (IsReplayingShot() &&
                    f_StateFrameCounts[playerIndex] >= REPLAY_SWING_FRAMES_MIN &&
                    f_IsReplayRequested) {
                    nextState = PLAYER_STATE_STARTING_SWING;
                }

                if (g_MergedController.buttons & (Z_TRIG | L_TRIG)) {
                    u32 previousDistance = IntLength2D(
                        previousPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
                        previousPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]
                    );

                    for (skipStep = 0; skipStep < ROLL_SKIP_STEPS_MAX; skipStep++) {
                        u32 distance = IntLength2D(
                            g_TurnBallSimulation.position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
                            g_TurnBallSimulation.position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]
                        );

                        if (g_TurnBallSimulation.state != BALL_STATE_ROLLING ||
                            g_GameState.isMiniGolf ||
                            (GetSurfaceTraits(g_TurnBallSimulation.surfaceBelow)->kind ==
                                    SURFACE_KIND_GREEN &&
                                (s32)distance < (s32)previousDistance)) {
                            break;
                        }

                        StepBallSimulation(&g_TurnBallSimulation);
                        TickStopwatches();
                        isCameraDone = UpdateFlightCamera(TRUE, g_TurnController.buttons);
                    }
                }

                if (IsReplayingShot() || g_IsDebugEnabled) {
                    if (g_IsDebugEnabled) {
                        if (g_MergedController.buttons & (Z_TRIG | L_TRIG)) {
                            if (!isCameraDone) {
                                StepBallSimulation(&g_TurnBallSimulation);
                                OnBallStepped(&g_Players[playerIndex]);
                                isCameraDone = UpdateFlightCamera(TRUE, g_TurnController.buttons);
                            }
                        }

                        if (IsGameFlagSet(GAME_FLAG_FAST_FLIGHT) ||
                            (g_TurnController.buttons & (Z_TRIG | L_TRIG))) {
                            for (extraStepIndex = 0; extraStepIndex < FAST_FLIGHT_EXTRA_STEPS;
                                extraStepIndex++) {
                                if (!isCameraDone) {
                                    StepBallSimulation(&g_TurnBallSimulation);
                                    TickStopwatches();
                                    OnBallStepped(&g_Players[playerIndex]);
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
                        while (g_TurnBallSimulation.state != BALL_STATE_STOPPED) {
                            StepBallSimulation(&g_TurnBallSimulation);
                            TickStopwatches();
                            UpdateFlightCamera(TRUE, g_TurnController.buttons);
                        }

                        ResetShotEffects();
                        ClearShotEffects(CLEAR_KIND_NICE_SHOT_BANNER);
                    }
                }

                if (g_TurnBallSimulation.state == BALL_STATE_STOPPED) {
                    ClearShotEffects(CLEAR_KIND_EFFECTS);
                    nextState = PLAYER_STATE_BALL_AT_REST;
                    g_HasShotComeToRest = TRUE;
                    f_StateFrameCounts[playerIndex] = 0;
                    ForceStopRumble();
                }

                break;
            }
            case PLAYER_STATE_BALL_AT_REST: {
                Boolean isCameraDone = UpdateFlightCamera(TRUE, g_TurnController.buttons);

                if (g_TurnBallSimulation.isHoledOut) {
                    if (GetEndReaction() == END_REACTION_ONLOOKERS) {
                        if (f_StateFrameCounts[playerIndex] == HOLE_OUT_SONG_FRAME) {
                            PlayHoleOutSong(playerIndex);
                        }

                        if (f_StateFrameCounts[playerIndex] == ONLOOKERS_BANNER_CLEAR_FRAME) {
                            StartBanner(BANNER_NONE);
                        }

                        if (f_StateFrameCounts[playerIndex] == ONLOOKERS_BANNER_FRAME) {
                            ShowHoleOutBanner(playerIndex);
                        }
                    }
                    else if (GetEndReaction() == END_REACTION_LONG_PUTT) {
                        if (f_StateFrameCounts[playerIndex] == HOLE_OUT_SONG_FRAME) {
                            PlayHoleOutSong(playerIndex);
                        }

                        if (f_StateFrameCounts[playerIndex] == LONG_PUTT_BANNER_FRAME) {
                            ShowHoleOutBanner(playerIndex);
                        }
                    }
                    else if (f_StateFrameCounts[playerIndex] == HOLE_OUT_SONG_FRAME) {
                        PlayHoleOutSong(playerIndex);
                        ShowHoleOutBanner(playerIndex);
                    }
                }

                if (IsBButtonHeld()) {
                    f_IsReplayRequested = TRUE;
                }

                if (f_IsReplayRequested) {
                    nextState = PLAYER_STATE_STARTING_SWING;
                    StartLandingReaction(-1);
                    StartBanner(BANNER_NONE);
                }
                else if (isCameraDone) {
                    Player *player;
                    SurfaceTraits *surfaceTraits;

                    MeasureTeeShot(playerIndex);
                    UpdateContest(FALSE);
                    StartLandingReaction(-1);
                    player = &g_Players[playerIndex];
                    surfaceTraits = GetSurfaceTraits(g_TurnBallSimulation.surface);
                    OnBallStepped(player);
                    ClearPassedRings(player);
                    if (IsOutOfBoundsKind(surfaceTraits->kind) &&
                        !IsGameFlagSet(GAME_FLAG_NO_OUT_OF_BOUNDS_PENALTY)) {
                        g_Players[playerIndex].strokeCount++;
                        if (g_GameState.gameMode == GAME_MODE_TRAINING) {
                            if (g_Players[playerIndex].strokeCount > STROKE_COUNT_MAX) {
                                g_Players[playerIndex].strokeCount = STROKE_COUNT_MAX;
                            }
                        }
                        else if (g_Players[playerIndex].strokeCount >
                            g_HoleSetup.par * STROKE_LIMIT_PER_PAR) {
                            g_Players[playerIndex].strokeCount =
                                g_HoleSetup.par * STROKE_LIMIT_PER_PAR;
                        }

                        if (player->ballSurface != SURFACE_TEE_UP) {
                            player->ballSurface = GetSurface(
                                player->ballPosition[AXIS_X],
                                player->ballPosition[AXIS_Z]
                            );
                        }
                    }
                    else if (surfaceTraits->kind == SURFACE_KIND_WATER_HAZARD) {
                        player->ballPosition[AXIS_X] = g_TurnBallSimulation.dropPosition[AXIS_X];
                        player->ballPosition[AXIS_Z] = g_TurnBallSimulation.dropPosition[AXIS_Z];
                        player->ballPosition[AXIS_Y] = GetGroundHeight(
                            player->ballPosition[AXIS_X],
                            player->ballPosition[AXIS_Z]
                        );
                        player->ballSurface =
                            GetSurface(player->ballPosition[AXIS_X], player->ballPosition[AXIS_Z]);
                        g_Players[playerIndex].strokeCount++;
                        if (g_GameState.gameMode == GAME_MODE_TRAINING) {
                            if (g_Players[playerIndex].strokeCount > STROKE_COUNT_MAX) {
                                g_Players[playerIndex].strokeCount = STROKE_COUNT_MAX;
                            }
                        }
                        else if (g_Players[playerIndex].strokeCount >
                            g_HoleSetup.par * STROKE_LIMIT_PER_PAR) {
                            g_Players[playerIndex].strokeCount =
                                g_HoleSetup.par * STROKE_LIMIT_PER_PAR;
                        }
                    }
                    else {
                        player->ballPosition[AXIS_X] = g_TurnBallSimulation.position[AXIS_X];
                        player->ballPosition[AXIS_Y] = g_TurnBallSimulation.position[AXIS_Y];
                        player->ballPosition[AXIS_Z] = g_TurnBallSimulation.position[AXIS_Z];
                        player->ballSurface = g_TurnBallSimulation.surface;
                    }

                    if (IsBunkerKind(surfaceTraits->kind) ||
                        surfaceTraits->kind == SURFACE_KIND_BUNKER_UNUSED_LIE) {
                        player->hasBeenInBunker = TRUE;
                    }

                    f_StateFrameCounts[playerIndex] = 0;
                    if (g_TurnBallSimulation.isHoledOut) {
                        g_Players[playerIndex].holeStatus = HOLE_STATUS_HOLED_OUT;
                        g_Players[playerIndex]
                            .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                            .strokeCount = g_Players[playerIndex].strokeCount;
                        if (g_GameState.replayKind == REPLAY_KIND_NONE &&
                            (IsOneOnOneMatchMode(g_GameState.gameMode) ||
                                (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                                    g_GameState.gameMode == GAME_MODE_CLUB_SLOTS))) {
                            g_Players[playerIndex]
                                .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                                .strokeCount += GetHandicapOffset(
                                GetCourseRecordIndex(),
                                g_GameState.holeIndex,
                                g_Players[playerIndex].handicap
                            );
                        }

                        nextState = PLAYER_STATE_STARTING_REACTION;
                        g_Players[playerIndex]
                            .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                            .puttCount = g_Players[playerIndex].puttCount;
                        FreeCharacterAndReactionLight(0);
                    }
                    else {
                        FreeCharacterAndReactionLight(0);
                        StartBanner(BANNER_NONE);
                        if (g_GameState.replayKind != REPLAY_KIND_NONE &&
                            g_Players[playerIndex].strokeCount >= g_ReplayStrokeCount) {
                            nextState = PLAYER_STATE_STARTING_REACTION;
                        }
                        else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
                            if (g_Players[playerIndex].strokeCount >= g_HoleSetup.par) {
                                nextState = PLAYER_STATE_STARTING_REACTION;
                            }
                            else {
                                StopFlightCamera();
                                StartHudAnimation(HUD_ANIMATION_TURN_AWAY);
                                nextState = PLAYER_STATE_ENDING_TURN;
                            }
                        }
                        else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
                            if (g_GameState.courseIndex == DRIVING_RANGE_COURSE_INDEX) {
                                OpenChoiceBox(CHOICE_BOX_ID_DRIVING_RANGE);
                            }
                            else {
                                OpenChoiceBox(CHOICE_BOX_ID_TRAINING_SHOT);
                            }

                            nextState = PLAYER_STATE_CHOOSING;
                        }
                        else if (g_GameState.replayKind == REPLAY_KIND_NONE &&
                            IsOneOnOneMatchMode(g_GameState.gameMode)) {
                            if (g_Players[playerIndex].strokeCount >=
                                    g_HoleSetup.par * STROKE_LIMIT_PER_PAR ||
                                GetMatchHoleResult(0, 1)) {
                                nextState = PLAYER_STATE_STARTING_REACTION;
                                GiveUpHole(playerIndex);
                            }
                            else {
                                StopFlightCamera();
                                StartHudAnimation(HUD_ANIMATION_TURN_AWAY);
                                nextState = PLAYER_STATE_ENDING_TURN;
                            }
                        }
                        else if (g_GameState.replayKind == REPLAY_KIND_NONE &&
                            (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                                g_GameState.gameMode == GAME_MODE_CLUB_SLOTS)) {
                            GroupPlayerMark results[PLAYER_COUNT_MAX];

                            if (g_Players[playerIndex].strokeCount >=
                                g_HoleSetup.par * STROKE_LIMIT_PER_PAR) {
                                g_Players[playerIndex]
                                    .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                                    .strokeCount = 0;
                                g_Players[playerIndex].holeStatus = HOLE_STATUS_OUT;
                            }

                            if (GetGroupHoleResult(results) ||
                                results[playerIndex] == GROUP_PLAYER_OUT) {
                                g_Players[playerIndex].holeStatus = HOLE_STATUS_OUT;
                                g_Players[playerIndex].strokeCount =
                                    g_Players[playerIndex]
                                        .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                                        .strokeCount = 0;
                                nextState = PLAYER_STATE_STARTING_REACTION;
                            }
                            else {
                                StopFlightCamera();
                                StartHudAnimation(HUD_ANIMATION_TURN_AWAY);
                                nextState = PLAYER_STATE_ENDING_TURN;
                            }
                        }
                        else if (g_Players[playerIndex].strokeCount >=
                            g_HoleSetup.par * STROKE_LIMIT_PER_PAR) {
                            g_Players[playerIndex]
                                .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                                .strokeCount = g_HoleSetup.par * STROKE_LIMIT_PER_PAR;
                            g_Players[playerIndex].holeStatus = HOLE_STATUS_OUT;
                            if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
                                g_Players[playerIndex].stopwatchFrameCount +=
                                    STROKE_LIMIT_PENALTY_MINUTES * FRAMES_PER_MINUTE;
                                g_GameState.stopwatchFrameCount +=
                                    STROKE_LIMIT_PENALTY_MINUTES * FRAMES_PER_MINUTE;
                                g_HoleTimeFrames[g_GameState.holeIndex] +=
                                    STROKE_LIMIT_PENALTY_MINUTES * FRAMES_PER_MINUTE;
                                if (g_Players[playerIndex].stopwatchFrameCount >
                                    STOPWATCH_FRAMES_MAX) {
                                    g_Players[playerIndex].stopwatchFrameCount =
                                        STOPWATCH_FRAMES_MAX;
                                }

                                if (g_GameState.stopwatchFrameCount > STOPWATCH_FRAMES_MAX) {
                                    g_GameState.stopwatchFrameCount = STOPWATCH_FRAMES_MAX;
                                }

                                if (g_HoleTimeFrames[g_GameState.holeIndex] >
                                    STOPWATCH_FRAMES_MAX) {
                                    g_HoleTimeFrames[g_GameState.holeIndex] = STOPWATCH_FRAMES_MAX;
                                }
                            }

                            nextState = PLAYER_STATE_STARTING_REACTION;
                        }
                        else {
                            StopFlightCamera();
                            if (!IsGameFlagSet(GAME_FLAG_FAST_AUTOPLAY)) {
                                StartHudAnimation(HUD_ANIMATION_TURN_AWAY);
                            }

                            nextState = PLAYER_STATE_ENDING_TURN;
                        }
                    }
                }

                break;
            }
            case PLAYER_STATE_STARTING_REACTION: {
                CharacterIndex characterIndex;
                CButtonPalette paletteIndex;
                ReactionAnimation reaction;

                if (IsCharacterSlotBusy(0)) {
                    FreeCharacterAndReactionLight(0);
                    break;
                }

                if (g_HoleSetup.conditions.sceneColorIndex == SCENE_COLOR_INDEX_SUDDEN_DEATH) {
                    StartFireworks();
                }

                ClearReactionSpot();
                ConsiderReactionSpot(g_CurrentShot.ballPosition);
                ConsiderReactionSpot(g_HoleSetup.cupPosition);
                ConsiderReactionSpot(g_HoleSetup.teePosition);
                ClearShotEffects(CLEAR_KIND_EFFECTS);
                StopFlightCamera();
                if (IsGameFlagSet(GAME_FLAG_SHOW_REACTION_FRAME) && f_HasReactionTestRun) {
                    paletteIndex = UP_C_PALETTE;
                    LoadCharacter(0, f_ReactionTestCharacter + CHARACTER_COUNT, paletteIndex);
                }
                else {
                    characterIndex = g_Players[playerIndex].characterIndex;
                    paletteIndex = g_Players[playerIndex].paletteIndex;
                    LoadCharacter(0, characterIndex + CHARACTER_COUNT, paletteIndex);
                }

                SetCharacterLighting(0, 1);
                memset(&g_RoundHud, 0, sizeof(g_RoundHud));
                if (!g_TurnBallSimulation.isHoledOut) {
                    PlaySong(SONG_HOLE_LOST, 0);
                    reaction = REACTION_ANIMATION_BOGEY;
                }
                else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
                    if (HasNoRingsLeft(&g_Players[playerIndex])) {
                        SetRingShotCleared(TRUE);
                        reaction = REACTION_ANIMATION_EAGLE;
                    }
                    else {
                        reaction = REACTION_ANIMATION_BOGEY;
                    }
                }
                else {
                    ScoreToPar scoreToPar = g_Players[playerIndex].strokeCount - g_HoleSetup.par;

                    if (g_Players[playerIndex].strokeCount == 1) {
                        reaction = REACTION_ANIMATION_EAGLE;
                    }
                    else {
                        switch (scoreToPar) {
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
                            case SCORE_TO_PAR_BOGEY:
                            default:
                                reaction = REACTION_ANIMATION_BOGEY;
                                break;
                        }
                    }
                }

                StartReaction(0, g_Players[playerIndex].characterIndex, reaction);
                SetCharacterAnimation(0, reaction);
                if (IsGameFlagSet(GAME_FLAG_SHOW_REACTION_FRAME) && f_HasReactionTestRun) {
                    SetCharacterAnimation(0, f_ReactionTestAnimation);
                    StartReaction(0, f_ReactionTestCharacter, f_ReactionTestAnimation);
                }

                f_StateEventFrame =
                    GetCurrentAnimationEndFrame(0) - GetAnimationFrame(0) + REACTION_HOLD_FRAMES;
                if (IsGameFlagSet(GAME_FLAG_FAST_AUTOPLAY)) {
                    f_StateEventFrame = SHORT_REACTION_FRAMES;
                }

                PlaceAndViewCharacter(0);
                f_ShouldUpdateReaction = TRUE;
                if (g_Players[playerIndex].holeStatus == HOLE_STATUS_HOLED_OUT) {
                    ScoreToPar scoreToPar = g_Players[playerIndex].strokeCount - g_HoleSetup.par;

                    if (g_Players[playerIndex].strokeCount == 1) {
                        CountNotableShot(playerIndex, NOTABLE_SHOT_HOLE_IN_ONE);
                    }
                    else {
                        switch (scoreToPar) {
                            case SCORE_TO_PAR_BIRDIE:
                                CountNotableShot(playerIndex, NOTABLE_SHOT_BIRDIE);
                                break;
                            case SCORE_TO_PAR_EAGLE:
                                CountNotableShot(playerIndex, NOTABLE_SHOT_EAGLE);
                                break;
                            case SCORE_TO_PAR_ALBATROSS:
                                CountNotableShot(playerIndex, NOTABLE_SHOT_ALBATROSS);
                                break;
                        }
                    }
                }

                if (g_Players[playerIndex].strokeCount - g_HoleSetup.par < SCORE_TO_PAR_PAR) {
                    ReportBirdieBadgeCount();
                }

                nextState = PLAYER_STATE_REACTING;
                f_StateFrameCounts[playerIndex] = 0;
                break;
            }
            case PLAYER_STATE_REACTING:
                PlaceAndViewCharacter(0);
                areCallsAllowed = FALSE;
                f_ShouldUpdateReaction = TRUE;
                if (f_StateFrameCounts[playerIndex] == f_StateEventFrame - REACTION_HOLD_FRAMES) {
                    StartBanner(BANNER_NONE);
                }

                if (g_HoleSetup.conditions.sceneColorIndex == SCENE_COLOR_INDEX_SUDDEN_DEATH) {
                    f_StateEventFrame = SUDDEN_DEATH_REACTION_FRAMES;
                }

                if (f_StateFrameCounts[playerIndex] >= f_StateEventFrame) {
                    StopFireworks();
                    if (g_GameState.gameMode == GAME_MODE_TRAINING) {
                        nextState = PLAYER_STATE_CHOOSING;
                        if (g_GameState.courseIndex == DRIVING_RANGE_COURSE_INDEX) {
                            OpenChoiceBox(CHOICE_BOX_ID_DRIVING_RANGE);
                        }
                        else {
                            OpenChoiceBox(CHOICE_BOX_ID_TRAINING_HOLE_END);
                        }
                    }
                    else if (g_GameState.gameMode == GAME_MODE_RING_SHOT &&
                        (!g_TurnBallSimulation.isHoledOut ||
                            !HasNoRingsLeft(&g_Players[playerIndex]))) {
                        nextState = PLAYER_STATE_CHOOSING;
                        OpenChoiceBox(CHOICE_BOX_ID_RING_SHOT);
                    }
                    else {
                        nextState = PLAYER_STATE_FINISHING_HOLE;
                    }

                    f_StateFrameCounts[playerIndex] = 0;
                }

                break;
            case PLAYER_STATE_CHOOSING:
                if (g_GameState.gameMode == GAME_MODE_TRAINING) {
                    if (IsGameFlagSet(GAME_FLAG_SHOW_REACTION_FRAME) &&
                        g_Players[playerIndex].holeStatus != HOLE_STATUS_PLAYING) {
                        // Debug flag 0x3E: the D-pad picks a character and a reaction to
                        // load in its place
                        char *motionName;
                        char text[DEBUG_TEXT_BUFFER_SIZE];

                        switch (f_ReactionTestAnimation) {
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

                        f_HasReactionTestRun = TRUE;
                        if (g_TurnController.repeatedButtons & L_JPAD) {
                            f_ReactionTestCharacter--;
                        }

                        if (g_TurnController.repeatedButtons & R_JPAD) {
                            f_ReactionTestCharacter++;
                        }

                        if (g_TurnController.repeatedButtons & U_JPAD) {
                            f_ReactionTestAnimation--;
                        }

                        if (g_TurnController.repeatedButtons & D_JPAD) {
                            f_ReactionTestAnimation++;
                        }

                        if (f_ReactionTestCharacter < 0) {
                            f_ReactionTestCharacter = CHARACTER_COUNT - 1;
                        }

                        if (f_ReactionTestCharacter >= CHARACTER_COUNT) {
                            f_ReactionTestCharacter = 0;
                        }

                        if (f_ReactionTestAnimation < REACTION_ANIMATION_EAGLE) {
                            f_ReactionTestAnimation = REACTION_ANIMATION_BOGEY;
                        }

                        if (f_ReactionTestAnimation > REACTION_ANIMATION_BOGEY) {
                            f_ReactionTestAnimation = REACTION_ANIMATION_EAGLE;
                        }

                        sprintf(
                            text,
                            "GUTS TEST CH[%2.2d] MOTION %s",
                            f_ReactionTestCharacter,
                            motionName
                        );
                        PrintDebugText(text, REACTION_TEST_TEXT_COLUMN, REACTION_TEST_TEXT_ROW);
                        if (!IsCharacterSlotBusy(0)) {
                            LoadCharacter(0, f_ReactionTestCharacter + CHARACTER_COUNT, 0);
                        }

                        if (g_TurnController.buttons != 0) {
                            FreeCharacterAndReactionLight(0);
                        }

                        g_TurnController.repeatedButtons &= ~(U_JPAD | D_JPAD);
                    }

                    switch (UpdateChoiceBox(&g_TurnController)) {
                        case CHOICE_RESULT_CONTINUE:
                            StopFlightCamera();
                            StartHudAnimation(HUD_ANIMATION_TURN_AWAY);
                            ForceStopRumble();
                            nextState = PLAYER_STATE_WAITING;
                            break;
                        case CHOICE_RESULT_MULLIGAN:
                            g_IsTrainingHeadingKept = TRUE;
                            g_Players[playerIndex] = g_SavedPlayers[playerIndex];
                            StopFlightCamera();
                            StartHudAnimation(HUD_ANIMATION_TURN_AWAY);
                            ForceStopRumble();
                            nextState = PLAYER_STATE_WAITING;
                            break;
                        case CHOICE_RESULT_HOLE_OUT:
                            if (g_Players[playerIndex].holeStatus == HOLE_STATUS_HOLED_OUT &&
                                IsRecordableHoleOut(playerIndex)) {
                                InsertRankedRecord(&g_PlayRecords[playerIndex]);
                            }

                            nextState = PLAYER_STATE_HOLE_DONE;
                            StopFlightCamera();
                            FreeCharacterAndReactionLight(0);
                            if (IsGameFlagSet(GAME_FLAG_SHOW_REACTION_FRAME)) {
                                nextState = PLAYER_STATE_STARTING_REACTION;
                            }

                            ForceStopRumble();
                            break;
                        case CHOICE_RESULT_RETURN_TO_MENU:
                            QuitPlayToTitle();
                            ForceStopRumble();
                            break;
                    }
                }
                else {
                    switch (UpdateChoiceBox(&g_TurnController)) {
                        case CHOICE_RESULT_START_OVER:
                            StopFlightCamera();
                            PlaySound(
                                SOUND_RING_SHOT_RETRY,
                                RING_SHOT_RETRY_SOUND_CHANNEL,
                                SOUND_FULL_SCALE
                            );
                            SkipNextPinDraw();
                            PlacePlayersOnTee();
                            ForceStopRumble();
                            nextState = PLAYER_STATE_WAITING;
                            break;
                        case CHOICE_RESULT_RING_SHOT_HOLE_OUT:
                            nextState = PLAYER_STATE_HOLE_DONE;
                            StopFlightCamera();
                            FreeCharacterAndReactionLight(0);
                            ForceStopRumble();
                            break;
                        case CHOICE_RESULT_RETURN_TO_MENU:
                            QuitPlayToTitle();
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
                    InsertRankedRecord(&g_PlayRecords[playerIndex]);
                }

                CheckRingShotClear(playerIndex);
                if (f_StateFrameCounts[playerIndex] == 1) {
                    StartBanner(BANNER_NONE);
                    g_IsHudTurnAwayRequested = TRUE;
                    if (g_GameState.replayKind != REPLAY_KIND_NONE) {
                        nextState = PLAYER_STATE_HOLE_DONE;
                    }
                    else if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                        g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
                        GroupPlayerMark results[PLAYER_COUNT_MAX];
                        Boolean isAnyOut = FALSE;
                        GroupHoleResult holeResult = GetGroupHoleResult(results);

                        for (groupPlayerIndex = 0; groupPlayerIndex < g_GameState.playerCount;
                            groupPlayerIndex++) {
                            if (results[groupPlayerIndex] == GROUP_PLAYER_OUT &&
                                g_Players[groupPlayerIndex].holeStatus == HOLE_STATUS_PLAYING) {
                                g_Players[groupPlayerIndex].holeStatus = HOLE_STATUS_NEWLY_OUT;
                                isAnyOut = TRUE;
                            }
                        }

                        nextState = (holeResult == GROUP_HOLE_OPEN && isAnyOut) ?
                            PLAYER_STATE_FINDING_OUT_PLAYER :
                            PLAYER_STATE_HOLE_DONE;
                    }
                    else {
                        if (IsOneOnOneMatchMode(g_GameState.gameMode)) {
                            MatchSide matchHoleResult;

                            GetHandicapOffset(
                                g_GameState.courseIndex,
                                g_GameState.holeIndex,
                                g_Players[playerIndex].handicap
                            );
                            GetHandicapOffset(
                                g_GameState.courseIndex,
                                g_GameState.holeIndex,
                                g_Players[playerIndex ^ 1].handicap
                            );
                            matchHoleResult = GetMatchHoleResult(playerIndex, playerIndex ^ 1);
                            osSyncPrintf("CheckHoltOutMatch %d\n", matchHoleResult);
                            if (matchHoleResult == MATCH_SIDE_PLAYER) {
                                g_Players[playerIndex ^ 1]
                                    .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                                    .strokeCount =
                                    g_Players[playerIndex]
                                        .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                                        .strokeCount +
                                    1;
                                g_Players[playerIndex ^ 1].strokeCount =
                                    g_Players[playerIndex ^ 1]
                                        .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                                        .strokeCount -
                                    GetHandicapOffset(
                                        GetCourseRecordIndex(),
                                        g_GameState.holeIndex,
                                        g_Players[playerIndex ^ 1].handicap
                                    );
                                g_Players[playerIndex ^ 1].holeStatus = HOLE_STATUS_OUT;
                            }
                            else if (matchHoleResult == MATCH_SIDE_OPPONENT) {
                                g_Players[playerIndex ^ 1]
                                    .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                                    .strokeCount =
                                    g_Players[playerIndex]
                                        .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                                        .strokeCount -
                                    1;
                                g_Players[playerIndex ^ 1].strokeCount =
                                    g_Players[playerIndex ^ 1]
                                        .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                                        .strokeCount -
                                    GetHandicapOffset(
                                        GetCourseRecordIndex(),
                                        g_GameState.holeIndex,
                                        g_Players[playerIndex ^ 1].handicap
                                    );
                            }
                            else if (matchHoleResult == MATCH_HOLE_HALVED &&
                                g_Players[playerIndex ^ 1].holeStatus == HOLE_STATUS_PLAYING) {
                                g_Players[playerIndex ^ 1]
                                    .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                                    .strokeCount =
                                    g_Players[playerIndex]
                                        .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                                        .strokeCount;
                                g_Players[playerIndex ^ 1].strokeCount =
                                    g_Players[playerIndex ^ 1]
                                        .holes[g_GameState.holeIndex % COURSE_HOLE_COUNT]
                                        .strokeCount -
                                    GetHandicapOffset(
                                        GetCourseRecordIndex(),
                                        g_GameState.holeIndex,
                                        g_Players[playerIndex ^ 1].handicap
                                    );
                                g_Players[playerIndex ^ 1].holeStatus = HOLE_STATUS_OUT;
                            }
                        }

                        nextState = PLAYER_STATE_HOLE_DONE;
                    }
                }

                break;
            case PLAYER_STATE_FINDING_OUT_PLAYER:
                for (checkedPlayer = 0; checkedPlayer < g_GameState.playerCount; checkedPlayer++) {
                    if (g_Players[checkedPlayer].holeStatus == HOLE_STATUS_NEWLY_OUT) {
                        break;
                    }
                }

                if (checkedPlayer == g_GameState.playerCount) {
                    nextState = PLAYER_STATE_HOLE_DONE;
                    break;
                }

                if (IsCharacterSlotBusy(0)) {
                    FreeCharacterAndReactionLight(0);
                    break;
                }

                for (outPlayer = 0; outPlayer < g_GameState.playerCount; outPlayer++) {
                    if (g_Players[outPlayer].holeStatus == HOLE_STATUS_NEWLY_OUT) {
                        g_Players[outPlayer].holeStatus = HOLE_STATUS_OUT;
                        break;
                    }
                }

                nextState = PLAYER_STATE_HOLE_DONE;
                if (outPlayer != g_GameState.playerCount) {
                    ClearReactionSpot();
                    LoadCharacter(
                        0,
                        g_Players[outPlayer].characterIndex + CHARACTER_COUNT,
                        g_Players[outPlayer].paletteIndex
                    );
                    ConsiderReactionSpot(g_Players[outPlayer].ballPosition);
                    ConsiderReactionSpot(g_HoleSetup.cupPosition);
                    ConsiderReactionSpot(g_HoleSetup.teePosition);
                    SetCharacterLighting(0, 1);
                    memset(&g_RoundHud, 0, sizeof(g_RoundHud));
                    StartReaction(0, g_Players[outPlayer].characterIndex, REACTION_ANIMATION_BOGEY);
                    SetCharacterAnimation(0, REACTION_ANIMATION_BOGEY);
                    PlaySong(SONG_HOLE_LOST, 0);
                    PlaceAndViewCharacter(0);
                    f_ShouldUpdateReaction = TRUE;
                    ShowFlipMessage(0);
                    nextState = PLAYER_STATE_SHOWING_OUT_PLAYER;
                    f_StateFrameCounts[playerIndex] = 0;
                }

                break;
            case PLAYER_STATE_SHOWING_OUT_PLAYER:
                PlaceAndViewCharacter(0);
                f_ShouldUpdateReaction = TRUE;
                areCallsAllowed = FALSE;
                if (f_StateFrameCounts[playerIndex] == OUT_PLAYER_FLIP_FRAME) {
                    ShowFlipMessage(-1);
                }

                if (f_StateFrameCounts[playerIndex] == OUT_PLAYER_SHOWN_FRAMES) {
                    StartBanner(BANNER_NONE);
                    nextState = PLAYER_STATE_FINDING_OUT_PLAYER;
                    ClearDebugText();
                    f_StateFrameCounts[playerIndex] = 0;
                }

                break;
            default:
                SetDebugTextHighlight(TRUE);
                PrintDebugText("P-S-ERR", STATE_ERROR_TEXT_COLUMN, STATE_ERROR_TEXT_ROW);
                nextState = PLAYER_STATE_AIMING;
                break;
        }

        g_Players[playerIndex].state = nextState;
        g_BallSimulations[0] = g_TurnBallSimulation;
        g_LastShotLaunch = f_ShotLaunch;
    }

    MarkNearbyGroundTiles(g_CurrentShot.ballPosition[AXIS_X], g_CurrentShot.ballPosition[AXIS_Z]);
    GetBallSurfaceTraits(&g_Players[g_GameState.currentPlayerIndex], &f_ShotLaunch);
    if (areCallsAllowed) {
        PlayWaitingPlayersCalls();
    }

    AnimateAllCharacters();
    if (f_ShouldUpdateReaction) {
        UpdateReaction(0);
    }

    // Debug flag 0x4A: the second stick turns a camera that frames the ball and
    // the shot
    if (IsGameFlagSet(GAME_FLAG_FRAME_BALL_AND_SHOT)) {
        Vec3 center;

        f_DebugFramingYaw +=
            g_ControllerStates[DEBUG_CONTROLLER].stickX * DEBUG_FRAMING_TURN_PER_STICK;
        f_DebugFramingPitch +=
            g_ControllerStates[DEBUG_CONTROLLER].stickY * DEBUG_FRAMING_TURN_PER_STICK;
        ClearFramedSpheres();
        SetFramingAngles(f_DebugFramingPitch, f_DebugFramingYaw);
        center[AXIS_X] = g_TurnBallSimulation.position[AXIS_X] * COURSE_TO_WORLD_SCALE;
        center[AXIS_Y] = g_TurnBallSimulation.position[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        center[AXIS_Z] = g_TurnBallSimulation.position[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        AddFramedSphere(center, 7.5f);
        center[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
        center[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        center[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        AddFramedSphere(center, 7.5f);
        FrameSpheres(0);
        GetFramedView(g_Cameras[MAIN_CAMERA].view.eye, g_Cameras[MAIN_CAMERA].view.lookAt);
    }

    if (g_RoundHud.isDistanceSignShown) {
        s32 distance = GetSelectedShotDistance();
        Radians angle = (u16)g_CurrentShot.heading * BINARY_ANGLE_TO_RADIANS;
        if (GetSelectedClub()->surfaceMask & CLUB_SURFACE_GREEN) {
            PlaceTargetPoint(angle, distance, UNITS_PER_METER);
        }
        else {
            PlaceTargetPoint(angle, distance, UNITS_PER_YARD);
        }

        g_CurrentShot.targetPoint[AXIS_Y] =
            GetGroundHeight(g_CurrentShot.targetPoint[AXIS_X], g_CurrentShot.targetPoint[AXIS_Z]);
    }

    UpdateGolfBalls();
}
