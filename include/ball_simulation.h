#ifndef BALL_SIMULATION_H
#define BALL_SIMULATION_H

#include <ultra64.h>

#include "club.h"
#include "math_util.h"

/* A simulation still going after this many steps is given up on */
#define MAX_STEP_COUNT 1801

/* Bits of the flags StepBallSimulation returns for each step */
#define STEP_FLAG_GROUND_HIT 0x1
#define STEP_FLAG_BILLBOARD_HIT 0x2
#define STEP_FLAG_LIPPED_OUT 0x4
#define STEP_FLAG_PIN_HIT 0x8
#define STEP_FLAG_OUTSIDE_COURSE 0x10
#define STEP_FLAG_HOLE_TRIANGLE_HIT 0x20
#define STEP_FLAG_HOLE_TRIANGLE_BOUNCE 0x40
#define STEP_FLAG_CUP_PULL 0x80

/* Values of a BallSimulation's state */
enum BallState {
  BALL_STATE_STOPPED,
  BALL_STATE_FLYING,
  BALL_STATE_ROLLING,
  BALL_STATE_IN_CUP,
  BALL_STATE_IN_WATER
};

#define BALL_SIMULATION_COUNT 1

/* The place that ResetBallSimulations starts each ball from, and the speeds
 * it gives it: a random one up to the mask across, and a fall */
#define RESET_BALL_X 0x800
#define RESET_BALL_Y (-1000)
#define RESET_BALL_Z 0x1000
#define RESET_BALL_SPEED_MASK 0x3FF
#define RESET_BALL_VELOCITY_Y (-100)

/* ShotLaunch impactTiming values from -IMPACT_TIMING_ENTRY_LIMIT to
 * IMPACT_TIMING_ENTRY_LIMIT have an Unk6 per lie rating */
#define IMPACT_TIMING_ENTRY_LIMIT 5
#define IMPACT_TIMING_ENTRY_COUNT ((IMPACT_TIMING_ENTRY_LIMIT * 2) + 1)

/*
 * The steepest ground that a ball is dropped on, as the square of its
 * slope: a rise of less than one in two
 */
#define DROP_MAX_SLOPE_SQUARED 0.25f
/* Whether dropPosition may be on a surface kind */
#define IsDropKind(kind) \
  ((kind) != SURFACE_KIND_WATER_HAZARD && \
      (kind) != SURFACE_KIND_OUT_OF_BOUNDS && \
      (kind) != SURFACE_KIND_BARE_GROUND_UNK20 && \
      (kind) != SURFACE_KIND_OUT_OF_COURSE)
/* Whether a ground plane is gentle enough for dropPosition */
#define IsDropSlope(plane) \
  (((plane).normal[AXIS_X] * (plane).normal[AXIS_X]) + \
          ((plane).normal[AXIS_Z] * (plane).normal[AXIS_Z]) < \
      (plane).normal[AXIS_Y] * (plane).normal[AXIS_Y] * \
          DROP_MAX_SLOPE_SQUARED)

typedef struct Unk6 {
  /* 0x0 */ s8 unk0;
  /* 0x1 */ s8 unk1;
  /* 0x2 */ s8 unk2;
  /* 0x3 */ s8 unk3;
  /* 0x4 */ s8 unk4;
  /* 0x5 */ s8 unk5;
} Unk6; /* size = 0x6 */

/* The largest launch power a club can give */
#define MAX_LAUNCH_POWER 256

/* How a shot is launched: the swing that the ball simulation starts from */
typedef struct ShotLaunch {
  /* 0x00 */ SignedBinaryAngle heading;
  /* 0x04 */ s32 clubIndex;
  /* 0x08 */ s32 power; /* Out of MAX_LAUNCH_POWER */
  /* 0x0C */ s32 impactTiming; /* Gauge frames past the impact mark */
  /* 0x10 */ s32 offsetX; /* CurrentShot's offsetX and offsetY */
  /* 0x14 */ s32 offsetY;
  /* 0x18 */ s32 playerIndex;
  /* 0x1C */ s32 shotMode;
} ShotLaunch; /* size = 0x20 */

#define UNKA8_COUNT 8

/* Added to a putt's y velocity as it launches */
#define PUTT_VELOCITY_Y_OFFSET 0x106

/* What LaunchBallSimulation is called for */
enum LaunchPurpose {
  LAUNCH_PURPOSE_STROKE,
  LAUNCH_PURPOSE_CLUB_TEST,
  LAUNCH_PURPOSE_TRIAL,
  LAUNCH_PURPOSE_UNK3,
  LAUNCH_PURPOSE_UNK5 = 5
};

typedef struct BallSimulation {
  /* 0x00 */ s32 startPosition[3];
  /*
   * The last point the ball crossed over gently sloping ground that is not
   * lost (IsLostBallKind) or SURFACE_KIND_BARE_GROUND_UNK20; a ball in a
   * water hazard is dropped there. Launching sets it to the start.
   */
  /* 0x0C */ s32 dropPosition[3];
  /* 0x18 */ s32 position[3]; /* Where the ball is, in course units */
  /* 0x24 */ s32 velocity[3];
  /* 0x30 */ s32 unk30;
  /* 0x34 */ s32 unk34;
  /* 0x38 */ s32 unk38;
  /* 0x3C */ f32 unk3C;
  /* 0x40 */ f32 unk40;
  /* 0x44 */ u8 unk44[0x84 - 0x44];
  /* 0x84 */ f32 windVelocityX;
  /* 0x88 */ f32 windVelocityZ;
  /* 0x8C */ s32 state; /* A BALL_STATE_* value */
  /* 0x90 */ s32 purpose;
  /* 0x94 */ s32 unk94;
  /* 0x98 */ s32 unk98;
  /* 0x9C */ s32 unk9C;
  /* 0xA0 */ s8 isHoledOut; /* Set as the ball stops in the cup */
  /* 0xA1 */ s8 unkA1;
  /* 0xA2 */ s8 unkA2; /* A SURFACE_* value */
  /* 0xA3 */ s8 surface; /* The SURFACE_* that the ball is on */
  /* 0xA4 */ u8 unkA4; /* A SURFACE_* value */
  /* 0xA5 */ s8 playerIndex;
  /* 0xA6 */ s8 unkA6;
  /* 0xA7 */ s8 unkA7;
  /* 0xA8 */ s8 unkA8[UNKA8_COUNT];
  /* Keeps the flag from being hit twice */
  /* 0xB0 */ s8 hasHitFlag;
  /* 0xB1 */ u8 unkB1;
  /* 0xB2 */ s16 unkB2;
  /* 0xB4 */ s8 unkB4;
  /* 0xB5 */ u8 unkB5[0xB8 - 0xB5];
} BallSimulation; /* size = 0xB8 */

/*
 * A shot's preview is drawn as a dash for each step of its simulation;
 * steps longer than this are not stretched into a dash
 */
#define SHOT_PREVIEW_MAX_DASH_LENGTH 0x4AFFF
/* How far below the ball's center the preview is drawn */
#define SHOT_PREVIEW_DROP 0x133

extern BallSimulation g_BallSimulations[BALL_SIMULATION_COUNT];

void ResetBallSimulations(void);
Unk6* GetImpactTimingEntry(
    Club* club, Surface surface, s32 shotMode, s32 impactTiming);
s32 StepBallSimulation(BallSimulation* simulation);
s32 LaunchBallSimulation(
    BallSimulation* simulation, s32 purpose, ShotLaunch* shot);

#endif
