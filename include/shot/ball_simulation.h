/*
 * The ball simulation, which moves the ball as it flies, bounces and rolls.
 */

#ifndef BALL_SIMULATION_H
#define BALL_SIMULATION_H

#include <ultra64.h>

#include "course/course_space.h"
#include "course/surface.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "shot/club.h"
#include "system/c_language.h"
#include "system/frame_time.h"

// The ball moves its speed in course units each frame
typedef f32 BallSpeed;

// A meter per second in speed units, as the debug display shows speeds
#define SPEED_UNITS_PER_METER_PER_SECOND (UNITS_PER_METER / FRAMES_PER_SECOND)
// The tuning speeds are whole kilometers per hour, with a kilometer per hour
// rounded to this many meters per second
#define METERS_PER_SECOND_PER_KILOMETER_PER_HOUR 0.2778
// A ball within 5 m of the ground's edges is outside the course
#define COURSE_EDGE_MARGIN 0x12C00

// In the cup the ball falls at half gravity
#define CUP_GRAVITY (GRAVITY / 2)

// The simulations draw noise from the low byte of the ball's coordinates:
// centered on the byte's midpoint for noise from -1 to 1, or as a pair whose
// sum, divided by its most, gives noise from 0 to 1
#define NOISE_BYTE_MASK 0xFF
#define NOISE_BYTE_MIDPOINT (NOISE_BYTE_MASK / 2.0f)
#define NOISE_BYTE_PAIR_SUM_MAX (NOISE_BYTE_MASK * 2.0f)
// Noise from 0 to 1 less this is centered on 0
#define NOISE_MIDDLE 0.5f
// Mini golf holds a rolling ball to the ground at twice gravity
#define MINI_GOLF_ROLL_GRAVITY (GRAVITY * 2)

#define KilometersPerHour(kmh) \
    ((BallSpeed)((kmh) * METERS_PER_SECOND_PER_KILOMETER_PER_HOUR * \
        SPEED_UNITS_PER_METER_PER_SECOND))

// The launch debug displays turn speeds into kilometers per hour with this:
// 1 / KilometersPerHour(1), written out to ten places, which rounds it to the
// float below
#define KILOMETERS_PER_HOUR_PER_SPEED_UNIT 0.0070306873f

// The smallest a ball may be across by the rules, as the spin's RPM takes it
#define BALL_DIAMETER_METERS 0.04267
// Turns a spin speed, the speed of the ball's surface, into revolutions per
// minute
#define RPM_PER_SPIN_SPEED_UNIT \
    ((f32)(SECONDS_PER_MINUTE / \
        (SPEED_UNITS_PER_METER_PER_SECOND * (f32)M_PI * BALL_DIAMETER_METERS)))

// A simulation still going after this many steps is given up on
#define STEP_COUNT_MAX 1801

// Bits of the flags StepBallSimulation returns for each step
#define STEP_FLAG_GROUND_HIT 0x1
#define STEP_FLAG_BILLBOARD_HIT 0x2
#define STEP_FLAG_LIPPED_OUT 0x4
#define STEP_FLAG_PIN_HIT 0x8
#define STEP_FLAG_OUTSIDE_COURSE 0x10
#define STEP_FLAG_HOLE_TRIANGLE_HIT 0x20
#define STEP_FLAG_HOLE_TRIANGLE_BOUNCE 0x40
#define STEP_FLAG_CUP_PULL 0x80
typedef s32 StepFlags;
typedef s8 StepFlags8;
typedef u8 UStepFlags8;

// Values of a BallSimulation's state
enum BallState {
    BALL_STATE_STOPPED,
    BALL_STATE_FLYING,
    BALL_STATE_ROLLING,
    BALL_STATE_IN_CUP,
    BALL_STATE_IN_WATER
};

typedef s32 BallState;

#define BALL_SIMULATION_COUNT 1

// The place that ResetBallSimulations starts each ball from, and the speeds
// it gives it: a random one up to the mask across, and a fall
#define RESET_BALL_X 0x800
#define RESET_BALL_Y (-1000)
#define RESET_BALL_Z 0x1000
#define RESET_BALL_SPEED_MASK 0x3FF
#define RESET_BALL_VELOCITY_Y (-100)

// ShotLaunch impactTiming values from -IMPACT_TIMING_ENTRY_LIMIT to
// IMPACT_TIMING_ENTRY_LIMIT have an ImpactEffect per lie rating
#define IMPACT_TIMING_ENTRY_LIMIT 5
#define IMPACT_TIMING_ENTRY_COUNT ((IMPACT_TIMING_ENTRY_LIMIT * 2) + 1)

// The steepest ground that a ball is dropped on, as the square of its
// slope: a rise of less than one in two
#define DROP_SLOPE_SQUARED_MAX 0.25f
// Whether dropPosition may be on a surface kind
#define IsDropKind(kind) \
    ((kind) != SURFACE_KIND_WATER_HAZARD && \
        (kind) != SURFACE_KIND_OUT_OF_BOUNDS && \
        (kind) != SURFACE_KIND_NO_DROP_GROUND && \
        (kind) != SURFACE_KIND_OUT_OF_COURSE)
// Whether a ground plane is gentle enough for dropPosition
#define IsDropSlope(plane) \
    (((plane).normal[AXIS_X] * (plane).normal[AXIS_X]) + \
            ((plane).normal[AXIS_Z] * (plane).normal[AXIS_Z]) < \
        (plane).normal[AXIS_Y] * (plane).normal[AXIS_Y] * DROP_SLOPE_SQUARED_MAX)

// How a lie rating and an impact timing change a launch
typedef struct ImpactEffect {
    s8 speedChange; // In 128ths of the speed
    s8 yawChange; // Times the club's mishitScale
    s8 pitchChange; // In degrees
    s8 sideSpinChange;
    s8 spinChange; // Added to the shot's offsetY
    s8 rumbles; // Whether the mishit rumbles
} ImpactEffect;

// The largest launch power a club can give
#define LAUNCH_POWER_MAX 256

// How a shot is launched: the swing that the ball simulation starts from
typedef struct ShotLaunch {
    SignedBinaryAngle heading;
    ClubSlot clubSlot;
    s32 power; // Out of MAX_LAUNCH_POWER
    s32 impactTiming; // Gauge frames past the impact mark
    s32 offsetX; // CurrentShot's offsetX and offsetY
    s32 offsetY;
    s32 playerIndex;
    ShotMode shotMode;
} ShotLaunch;

// BallSimulation's lastBounceStep before the ball's first bounce
#define NO_BOUNCE_STEP (-2)

// BallSimulation.soundStates' entries
#define BALL_SOUND_CUP_IN 0 // Holds a CupInSoundState
#define BALL_SOUND_ROLL 1 // Whether the roll sound is playing
#define BALL_SOUND_COUNT 8

// The cup-in sound's states. A ball that drops in slowly makes the sound due,
// and it plays as the ball reaches the cup's floor; any other ball knocks on
// the floor instead.
enum CupInSoundState {
    CUP_IN_SOUND_NONE,
    CUP_IN_SOUND_DUE,
    CUP_IN_SOUND_PLAYED
};

typedef s32 CupInSoundState;

// Added to a putt's y velocity as it launches
#define PUTT_VELOCITY_Y_OFFSET 0x106

// What LaunchBallSimulation is called for
enum LaunchPurpose {
    LAUNCH_PURPOSE_STROKE,
    LAUNCH_PURPOSE_CLUB_TEST,
    LAUNCH_PURPOSE_TRIAL,
    // Not launched: PlaceBallAtRest sets it on a ball it puts at rest, which
    // DrawGolfBalls then draws at the ball's own depth, with no near-cup bias,
    // unless the cup view is shown
    LAUNCH_PURPOSE_PLACED_AT_REST,
    // BuildShotPreview's path: StepBallSimulation takes one substep a step and
    // skips the settling
    LAUNCH_PURPOSE_SHOT_PREVIEW = 5
};

typedef s32 LaunchPurpose;
typedef s8 LaunchPurpose8;

// Whether a ball was launched for a stroke or for a trial of one
#define IsStrokeOrTrialLaunch(purpose) \
    ((purpose) == LAUNCH_PURPOSE_STROKE || (purpose) == LAUNCH_PURPOSE_TRIAL)

typedef struct BallSimulation {
    s32 startPosition[AXIS_COUNT];
    // The last point the ball crossed over gently sloping ground that is not
    // lost (IsLostBallKind) or SURFACE_KIND_NO_DROP_GROUND; a ball in a
    // water hazard is dropped there. Launching sets it to the start.
    s32 dropPosition[AXIS_COUNT];
    s32 position[AXIS_COUNT]; // Where the ball is, in course units
    s32 velocity[AXIS_COUNT];
    // Only resetting the simulations clears it; nothing reads it
    s32 unusedResetValue;
    // The spin about the level axis across spinHeading, which the debug display
    // calls SpinX; on the ground it is the speed the spin drives the ball to
    s32 spinSpeed;
    // Curves the ball in flight; the debug display calls it SpinY
    s32 sideSpin;
    Radians spinHeading; // Launching sets it to the yaw
    f32 spinSize; // Set at launch; read by no routine
    // Turns the ball's velocity through the air into its lift's direction
    Mat4 liftRotation;
    f32 windVelocityX;
    f32 windVelocityZ;
    BallState state;
    LaunchPurpose purpose;
    // Hits on the ground, hole triangles and billboards
    s32 bounceCount;
    s32 stepCount;
    // The step of the ball's last bounce, or NO_BOUNCE_STEP before any
    s32 lastBounceStep;
    Boolean8 isHoledOut; // Set as the ball stops in the cup
    Boolean8 hasEnteredWater; // Set once the ball is in the water
    Surface8 launchSurface; // The surface launched from
    Surface8 surface; // The surface that the ball is on
    Surface8 surfaceBelow; // The surface under the ball, in the air too
    s8 playerIndex;
    StepFlags8 stepFlags; // Of the last substep
    // The surface of the first bounce, if on the ground; at first, where
    // the ball started
    Surface8 firstBounceSurface;
    s8 soundStates[BALL_SOUND_COUNT];
    // Keeps the flag from being hit twice
    Boolean8 hasHitFlag;
    UStepFlags8 previousStepFlags; // stepFlags as the step started
    u16 rollFrameCount; // Frames since the ball began rolling
    Boolean8 isOverCup; // The floor under the ball dips into the cup
} BallSimulation;

// A shot's preview is drawn as a dash for each step of its simulation;
// steps longer than this are not stretched into a dash
#define SHOT_PREVIEW_DASH_LENGTH_MAX 0x4AFFF
// How far below the ball's center the preview is drawn
#define SHOT_PREVIEW_DROP 0x133

extern BallSimulation g_BallSimulations[BALL_SIMULATION_COUNT];
// The launches since the computer golfer started planning, which
// LaunchBallSimulation counts and UpdateTurns clears; read by no routine
extern s32 g_UnusedLaunchCount;

void ResetBallSimulations(void);
ImpactEffect *GetImpactTimingEntry(
    Club *club,
    Surface surface,
    ShotMode shotMode,
    s32 impactTiming
);
StepFlags StepBallSimulation(BallSimulation *simulation);
s32 LaunchBallSimulation(BallSimulation *simulation, LaunchPurpose purpose, ShotLaunch *shot);

#endif
