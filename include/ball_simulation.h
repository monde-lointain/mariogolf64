#ifndef BALL_SIMULATION_H
#define BALL_SIMULATION_H

#include <ultra64.h>

#include "club.h"
#include "course_space.h"
#include "frame_time.h"
#include "geometry.h"
#include "language.h"
#include "math_util.h"
#include "surface.h"

typedef f32 BallSpeed;

#define SPEED_UNITS_PER_METER_PER_SECOND (UNITS_PER_METER / FRAMES_PER_SECOND)
#define METERS_PER_SECOND_PER_KILOMETER_PER_HOUR 0.2778
#define COURSE_EDGE_MARGIN 0x12C00

#define CUP_GRAVITY (GRAVITY / 2)

#define NOISE_BYTE_MASK 0xFF
#define NOISE_BYTE_MIDPOINT (NOISE_BYTE_MASK / 2.0f)
#define NOISE_BYTE_PAIR_SUM_MAX (NOISE_BYTE_MASK * 2.0f)
#define NOISE_MIDDLE 0.5f
#define MINI_GOLF_ROLL_GRAVITY (GRAVITY * 2)

#define KilometersPerHour(kmh) \
    ((BallSpeed)((kmh) * METERS_PER_SECOND_PER_KILOMETER_PER_HOUR * \
        SPEED_UNITS_PER_METER_PER_SECOND))

#define KILOMETERS_PER_HOUR_PER_SPEED_UNIT 0.0070306873f

#define BALL_DIAMETER_METERS 0.04267
#define RPM_PER_SPIN_SPEED_UNIT \
    ((f32)(SECONDS_PER_MINUTE / \
        (SPEED_UNITS_PER_METER_PER_SECOND * (f32)M_PI * BALL_DIAMETER_METERS)))

#define STEP_COUNT_MAX 1801

#define STEP_FLAG_GROUND_HIT 0x1
#define STEP_FLAG_BILLBOARD_HIT 0x2
#define STEP_FLAG_LIPPED_OUT 0x4
#define STEP_FLAG_PIN_HIT 0x8
#define STEP_FLAG_OUTSIDE_COURSE 0x10
#define STEP_FLAG_HOLE_TRIANGLE_HIT 0x20
#define STEP_FLAG_HOLE_TRIANGLE_BOUNCE 0x40
#define STEP_FLAG_CUP_PULL 0x80

enum BallState {
    BALL_STATE_STOPPED,
    BALL_STATE_FLYING,
    BALL_STATE_ROLLING,
    BALL_STATE_IN_CUP,
    BALL_STATE_IN_WATER
};

typedef s32 BallState;

#define BALL_SIMULATION_COUNT 1

#define RESET_BALL_X 0x800
#define RESET_BALL_Y (-1000)
#define RESET_BALL_Z 0x1000
#define RESET_BALL_SPEED_MASK 0x3FF
#define RESET_BALL_VELOCITY_Y (-100)

#define IMPACT_TIMING_ENTRY_LIMIT 5
#define IMPACT_TIMING_ENTRY_COUNT ((IMPACT_TIMING_ENTRY_LIMIT * 2) + 1)

#define DROP_SLOPE_SQUARED_MAX 0.25f
#define IsDropKind(kind) \
    ((kind) != SURFACE_KIND_WATER_HAZARD && \
        (kind) != SURFACE_KIND_OUT_OF_BOUNDS && \
        (kind) != SURFACE_KIND_NO_DROP_GROUND && \
        (kind) != SURFACE_KIND_OUT_OF_COURSE)
#define IsDropSlope(plane) \
    (((plane).normal[AXIS_X] * (plane).normal[AXIS_X]) + \
            ((plane).normal[AXIS_Z] * (plane).normal[AXIS_Z]) < \
        (plane).normal[AXIS_Y] * (plane).normal[AXIS_Y] * DROP_SLOPE_SQUARED_MAX)

typedef struct ImpactEffect {
    s8 speedChange;
    s8 yawChange;
    s8 pitchChange;
    s8 sideSpinChange;
    s8 spinChange;
    s8 rumbles;
} ImpactEffect;

#define LAUNCH_POWER_MAX 256

typedef struct ShotLaunch {
    SignedBinaryAngle heading;
    // A slot in the player's bag, except in club tests, which store the
    // CLUB_TYPE_* to test.
    s32 clubSlot;
    s32 power;
    s32 impactTiming;
    s32 offsetX;
    s32 offsetY;
    s32 playerIndex;
    ShotMode shotMode;
} ShotLaunch;

#define NO_BOUNCE_STEP (-2)

#define BALL_SOUND_CUP_IN 0
#define BALL_SOUND_ROLL 1
#define BALL_SOUND_COUNT 8

#define CUP_IN_SOUND_NONE 0
#define CUP_IN_SOUND_DUE 1
#define CUP_IN_SOUND_PLAYED 2

#define PUTT_VELOCITY_Y_OFFSET 0x106

enum LaunchPurpose {
    LAUNCH_PURPOSE_STROKE,
    LAUNCH_PURPOSE_CLUB_TEST,
    LAUNCH_PURPOSE_TRIAL,
    LAUNCH_PURPOSE_PLACED_AT_REST,
    LAUNCH_PURPOSE_SHOT_PREVIEW = 5
};

typedef s32 LaunchPurpose;
typedef s8 LaunchPurpose8;

#define IsStrokeOrTrialLaunch(purpose) \
    ((purpose) == LAUNCH_PURPOSE_STROKE || (purpose) == LAUNCH_PURPOSE_TRIAL)

typedef struct BallSimulation {
    s32 startPosition[3];
    s32 dropPosition[3];
    s32 position[3];
    s32 velocity[3];
    s32 unusedResetValue;
    s32 spinSpeed;
    s32 sideSpin;
    Radians spinHeading;
    f32 spinSize;
    Mat4 liftRotation;
    f32 windVelocityX;
    f32 windVelocityZ;
    BallState state;
    LaunchPurpose purpose;
    s32 bounceCount;
    s32 stepCount;
    s32 lastBounceStep;
    Boolean8 isHoledOut;
    Boolean8 hasEnteredWater;
    Surface8 launchSurface;
    Surface8 surface;
    Surface8 surfaceBelow;
    s8 playerIndex;
    s8 stepFlags;
    Surface8 firstBounceSurface;
    s8 soundStates[BALL_SOUND_COUNT];
    Boolean8 hasHitFlag;
    u8 previousStepFlags;
    u16 rollFrameCount;
    Boolean8 isOverCup;
} BallSimulation;

#define SHOT_PREVIEW_DASH_LENGTH_MAX 0x4AFFF
#define SHOT_PREVIEW_DROP 0x133

extern BallSimulation g_BallSimulations[BALL_SIMULATION_COUNT];
extern s32 g_UnusedLaunchCount;

void ResetBallSimulations(void);
ImpactEffect *GetImpactTimingEntry(
    Club *club,
    Surface surface,
    ShotMode shotMode,
    s32 impactTiming
);
s32 StepBallSimulation(BallSimulation *simulation);
s32 LaunchBallSimulation(BallSimulation *simulation, s32 purpose, ShotLaunch *shot);

#endif
