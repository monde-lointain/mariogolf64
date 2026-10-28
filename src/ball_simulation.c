/*
 * The ball simulation. It launches the ball with a shot's velocity and wind,
 * then moves it a step at a time as it flies, bounces and rolls over the
 * course. It plays the sounds of the ball's impacts and spawns their effects.
 */
#include "common.h"

#include "ball_simulation.h"

#include "billboard.h"
#include "club.h"
#include "collision.h"
#include "debug_text.h"
#include "effects.h"
#include "functions.h"
#include "game_flags.h"
#include "hole_objects.h"
#include "libc.h"
#include "math_util.h"
#include "save.h"
#include "scene.h"
#include "shot_effects.h"
#include "sound.h"
#include "surface.h"
#include "terrain.h"
#include "variables.h"

BallSimulation g_BallSimulations[BALL_SIMULATION_COUNT] = {0};

s8 g_ClubTestMode = CLUB_TEST_NONE;

static SurfaceInfo f_SurfaceInfo[SURFACE_COUNT] = {
    /* SURFACE_OUT_OF_BOUNDS */
    {SURFACE_KIND_OUT_OF_BOUNDS, "O.B.", 100, 100, 0, 0, 0, 0x500, 1, 7},
    /* SURFACE_FAIRWAY */
    {SURFACE_KIND_FAIRWAY, "FAIRWAY", 98, 100, 5, 960, 500, 0x501, 0, 0},
    /* SURFACE_ROUGH */
    {SURFACE_KIND_ROUGH, "ROUGH", 80, 90, 30, 600, 200, 0x508, 1, 1},
    /* SURFACE_BUNKER */
    {SURFACE_KIND_BUNKER, "BUNKER", 70, 85, 30, 600, 0, 0x504, 0, 2},
    /* SURFACE_GREEN */
    {SURFACE_KIND_GREEN, "ON GREEN", 100, 100, 0, 972, 600, 0x50A, 0, 6},
    /* SURFACE_WATER_HAZARD */
    {SURFACE_KIND_WATER_HAZARD, "WATER HAZARD", 100, 100, 0, 900, 400, 0x50C, 0,
        3},
    /* SURFACE_CART_WAY */
    {SURFACE_KIND_CART_WAY, "CART WAY", 80, 100, 10, 975, 890, 0x50B, 0, 4},
    /* SURFACE_TEE_GROUND */
    {SURFACE_KIND_TEE_GROUND, "TEE GROUND", 98, 100, 10, 930, 500, 0x501, 0, 8},
    /* SURFACE_ROCK */
    {SURFACE_KIND_ROCK, "ROCK", 80, 90, 30, 930, 800, 0x50D, 0, 9},
    /* SURFACE_GREEN_EDGE */
    {SURFACE_KIND_GREEN_EDGE, "GREEN EDGE", 98, 100, 10, 966, 550, 0x501, 0, 0},
    /* SURFACE_SEMI_ROUGH */
    {SURFACE_KIND_SEMI_ROUGH, "SEMI ROUGH", 90, 98, 30, 930, 500, 0x507, 0, 0},
    /* SURFACE_BARE_GROUND */
    {SURFACE_KIND_BARE_GROUND, "BARE GROUND", 80, 90, 30, 930, 500, 0x50C, 0,
        1},
    /* SURFACE_DEEP_ROUGH */
    {SURFACE_KIND_DEEP_ROUGH, "DEEP ROUGH", 50, 80, 30, 430, 100, 0x509, 2, 5},
    /* SURFACE_BARE_GROUND_UNK13 */
    {SURFACE_KIND_BARE_GROUND_UNK20, "BARE GROUND", 80, 90, 30, 930, 500, 0x50C,
        0, 1},
    /* SURFACE_WASTE_AREA */
    {SURFACE_KIND_WASTE_AREA, "WASTE AREA", 50, 80, 30, 430, 100, 0x8F1, 1, 5},
    /* SURFACE_UNK15 */
    {SURFACE_KIND_ROCK, "            ", 98, 100, 40, 930, 500, 0x500, 0, 9},
    /* SURFACE_BUNKER_FAIR_LIE */
    {SURFACE_KIND_BUNKER_FAIR_LIE, "BUNKER", 50, 70, 30, 600, 0, 0x505, 1, 2},
    /* SURFACE_BUNKER_POOR_LIE */
    {SURFACE_KIND_BUNKER_POOR_LIE, "BUNKER", 40, 60, 30, 600, 0, 0x506, 1, 2},
    /* SURFACE_BUNKER_UNK18 */
    {SURFACE_KIND_BUNKER_UNK9, "BUNKER", 40, 60, 40, 600, 0, 0x506, 1, 2},
    /* SURFACE_FAIRWAY_FAIR_LIE */
    {SURFACE_KIND_FAIRWAY_FAIR_LIE, "FAIRWAY", 94, 98, 7, 950, 380, 0x502, 0,
        0},
    /* SURFACE_FAIRWAY_POOR_LIE */
    {SURFACE_KIND_FAIRWAY_POOR_LIE, "FAIRWAY", 90, 94, 7, 950, 380, 0x503, 0,
        0},
    /* SURFACE_ROUGH_POOR_LIE */
    {SURFACE_KIND_ROUGH_POOR_LIE, "ROUGH", 70, 85, 30, 600, 200, 0x90C, 1, 5},
    /* SURFACE_OUT_OF_COURSE */
    {SURFACE_KIND_OUT_OF_COURSE, "OUT OF COURSE", 100, 100, 0, 0, 0, 0x500, 0,
        7},
    /* SURFACE_TEE_UP */
    {SURFACE_KIND_TEE_UP, "TEE UP", 98, 100, 10, 930, 500, 0x500, 0, 8},
    /* SURFACE_MINI_GOLF_GREEN */
    {SURFACE_KIND_GREEN, "GREEN", 100, 100, 0, 972, 500, 0x8F2, 0, 6},
    /* SURFACE_MINI_GOLF_FAIRWAY */
    {SURFACE_KIND_FAIRWAY, "BLUE", 98, 100, 5, 960, 400, 0x8F3, 0, 0},
    /* SURFACE_MINI_GOLF_CART_WAY */
    {SURFACE_KIND_CART_WAY, "RED", 80, 100, 10, 980, 800, 0x8F4, 0, 4},
};

/* For unkC values below and above the entries of D_800B7C10 */
static Unk6 D_800B7C00 = {-90, -64, -40, -10, 100, 1};
static Unk6 D_800B7C08 = {-80, 64, 30, 10, -5, 1};
static Unk6 D_800B7C10[MAX_LIE_RATING + 1][UNKC_ENTRY_COUNT] = {
    {
        {-90, -64, -40, -10, 100, 1},
        {-90, -64, -40, -10, 100, 1},
        {-90, -64, -40, -10, 100, 1},
        {-90, -64, -40, -10, 100, 1},
        {-90, -64, -40, -10, 100, 1},
        {0, 0, 0, 0, 0, 0},
        {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1},
    },
    {
        {-90, -64, -40, -10, 100, 1},
        {-90, -64, -40, -10, 100, 1},
        {-90, -64, -40, -10, 100, 1},
        {-90, -64, -40, -10, 100, 1},
        {-8, -2, 2, -7, 15, 0},
        {0, 0, 0, 0, 0, 0},
        {-12, 2, -2, 7, 4, 0},
        {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1},
    },
    {
        {-90, -64, -40, -10, 100, 1},
        {-90, -64, -40, -10, 100, 1},
        {-90, -64, -40, -10, 100, 1},
        {-8, -3, 2, -10, 15, 0},
        {-4, -2, 1, -5, 5, 0},
        {0, 0, 0, 0, 0, 0},
        {-8, 2, -1, 5, 4, 0},
        {-12, 3, -2, 10, 4, 0},
        {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1},
    },
    {
        {-90, -64, -40, -10, 100, 1},
        {-90, -64, -40, -10, 100, 1},
        {-8, -3, 2, -10, 15, 0},
        {-4, -2, 1, -5, 5, 0},
        {-2, -1, 0, -3, 3, 0},
        {0, 0, 0, 0, 0, 0},
        {-4, 1, 0, 3, 2, 0},
        {-8, 2, -1, 5, 4, 0},
        {-12, 3, -2, 10, 4, 0},
        {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1},
    },
    {
        {-90, -64, -40, -10, 100, 1},
        {-8, -3, 2, -10, 15, 0},
        {-4, -2, 1, -5, 5, 0},
        {-2, -1, 0, -3, 3, 0},
        {-1, 0, 0, -2, 1, 0},
        {0, 0, 0, 0, 0, 0},
        {-2, 0, 0, 2, 1, 0},
        {-4, 1, 0, 3, 2, 0},
        {-8, 2, -1, 5, 4, 0},
        {-12, 3, -2, 10, 4, 0},
        {-80, 64, 30, 10, -5, 1},
    },
    {
        {-8, -3, 2, -10, 15, 0},
        {-4, -2, 1, -5, 5, 0},
        {-2, -1, 0, -3, 3, 0},
        {-1, 0, 0, -2, 1, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {-2, 0, 0, 2, 1, 0},
        {-4, 1, 0, 3, 2, 0},
        {-8, 2, -1, 5, 4, 0},
        {-12, 3, -2, 10, 4, 0},
    },
};

s32 D_800B7D9C = 0;
/* Read by no routine */
static s32 D_800B7DA0[4] = {0};

SurfaceInfo* GetSurfaceInfo(s32 surface) {
  if ((u32)surface >= SURFACE_COUNT) {
    surface = SURFACE_OUT_OF_BOUNDS;
  }

  return &f_SurfaceInfo[surface];
}

void ResetBallSimulations(void) {
  s32 index;

  for (index = 0; index < BALL_SIMULATION_COUNT; index++) {
    g_BallSimulations[index].position[AXIS_X] = RESET_BALL_X;
    g_BallSimulations[index].position[AXIS_Y] = RESET_BALL_Y;
    rand(); /* The original has it */
    g_BallSimulations[index].position[AXIS_Z] = RESET_BALL_Z;
    g_BallSimulations[index].velocity[AXIS_X] = rand() & RESET_BALL_SPEED_MASK;
    g_BallSimulations[index].velocity[AXIS_Y] = RESET_BALL_VELOCITY_Y;
    g_BallSimulations[index].velocity[AXIS_Z] = rand() & RESET_BALL_SPEED_MASK;
    g_BallSimulations[index].unk30 = 0;
    g_BallSimulations[index].state = BALL_STATE_FLYING;
  }
}

/*
 * The absolute value as a statement expression, which compiles to a compare and
 * branch. It is a macro because StepBallSimulation compiles further from the
 * original when these sites call an inline function instead, whether a static
 * one or the AbsoluteValue nested one.
 */
#define AbsoluteValueExpression(value) \
  ({ \
    f32 absValue = (value); \
    (absValue < 0.0f) ? -absValue : absValue; \
  })
#define ROLL_SOUND_CHANNEL 8

/*
 * Moves the simulated ball through one step, with a second substep when the
 * ball bounces within the first 40% of its path. Each substep tests the path
 * against rings, the ground, billboards, the pin, the cup, walls and hole
 * triangles, then bounces, rolls, sinks or stops the ball, and a flying ball
 * feels drag, curve, lift and the wind. Returns the STEP_FLAG_* bits for the
 * step. It does not match: the register allocation and instruction scheduling
 * differ in many places, and where the original recomputes abs.s for each use
 * of a float's absolute value, this code shares one abs.s or compiles a compare
 * and branch.
 */
#ifdef NON_MATCHING
s32 StepBallSimulation(BallSimulation* simulation) {
  PlaneTriangle hit;
  PlaneTriangle triangle;
  f32 velocity[3];
  f32 hitFraction;
  f32 fraction;
  s32 previousVelocityX;
  s32 previousVelocityZ;
  s32 flags = 0;
  s32 substep;
  s32 substepCount = 1;
  s32 printsDebugText = FALSE;
  s32 startX;
  s32 startY;
  s32 startZ;
  s32 contact;
  s32 hitsHoleTriangle;
  Boolean isNearCup;
  s32 rollSpeed;
  Boolean isSteep;

  void PlayStrokeImpactSound(s32 sound, f32 fullSpeed, f32 speed) {
    s32 volume = speed * 127.0f / fullSpeed;

    if (volume >= 10) {
      if (volume >= 128) {
        volume = 127;
      }

      if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
        PlaySound(sound, g_NextSoundChannel, volume);
      }
    }
  }

  void BuildUnk44Rotation(void) {
    f32 axisX;
    f32 axisZ;

    if (simulation->unk34 != 0) {
      axisZ = cosf(simulation->unk3C) * simulation->unk34;
      axisX = sinf(simulation->unk3C) * simulation->unk34;
      if (simulation->unk34 < 0) {
        simulation->unk3C += 3.1415927f;
        simulation->unk34 = -simulation->unk34;
      }
    } else {
      axisZ = 1.0f;
      axisX = 0.0f;
    }

    guRotateF(simulation->unk44, 88.0f, -axisX, 0.0f, axisZ);
  }

  void PlayBillboardHitSound(s32 cell, s32 speed) {
    void PlayImpactSound(s32 sound, f32 fullSpeed, f32 speed) {
      s32 volume = speed * 127.0f / fullSpeed;

      if (volume >= 10) {
        if (volume >= 128) {
          volume = 127;
        }

        PlaySound(sound, g_NextSoundChannel, volume);
      }
    }

    switch (cell) {
      case 1:
        PlayImpactSound(0xA, 2844.672f, speed);
        break;
      case 2:
        PlayImpactSound(0xB, 2844.672f, speed);
        break;
      case 5:
        PlayImpactSound(0x29, 2844.672f, speed);
        break;
      case 4:
      default:
        PlayImpactSound(0xC, 2844.672f, speed);
        break;
    }
  }

  f32 GetPositionNoise(void) {
    return (((simulation->position[AXIS_X] + simulation->position[AXIS_Z]) &
                0xFF) -
               127.5f) /
        127.5f;
  }

  inline f32 AbsoluteValue(f32 value) {
    return value < 0.0f ? -value : value;
  }

  inline f32 GetStepNoise(void) {
    return (u32)((simulation->position[AXIS_X] + simulation->position[AXIS_Z] +
                     g_Players[simulation->playerIndex].unk88) &
               0xFF) /
        510.0f +
        (u32)((simulation->position[AXIS_X] - simulation->position[AXIS_Z] +
                  g_Players[simulation->playerIndex].unk88) &
            0xFF) /
        510.0f;
  }

  velocity[AXIS_X] = simulation->velocity[AXIS_X];
  velocity[AXIS_Y] = simulation->velocity[AXIS_Y];
  velocity[AXIS_Z] = simulation->velocity[AXIS_Z];
  fraction = 1.0f;
  substep = 0;
  do {
    if (substep != 0 && simulation->purpose == LAUNCH_PURPOSE_UNK5) {
      goto NEXT;
    }

    startY = simulation->position[AXIS_Y];
    isNearCup = FALSE;
    startX = simulation->position[AXIS_X];
    startZ = simulation->position[AXIS_Z];
    simulation->unkB1 = simulation->unkA6;
    previousVelocityX = velocity[AXIS_X];
    previousVelocityZ = velocity[AXIS_Z];
    hitsHoleTriangle = FALSE;
    if (simulation->state == BALL_STATE_STOPPED) {
      goto SETTLE;
    }

    if (simulation->unk98 == 0) {
      BuildUnk44Rotation();
    }

    if (++simulation->unk98 >= MAX_STEP_COUNT) {
      simulation->position[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X] + 614.4f;
      simulation->velocity[AXIS_X] = 0;
      simulation->velocity[AXIS_Z] = 0;
      simulation->position[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z];
    }

    if (simulation->state == BALL_STATE_ROLLING) {
      simulation->unkB2++;
    }

    simulation->position[AXIS_X] =
        simulation->position[AXIS_X] + velocity[AXIS_X];
    simulation->position[AXIS_Y] =
        simulation->position[AXIS_Y] + velocity[AXIS_Y];
    simulation->position[AXIS_Z] =
        simulation->position[AXIS_Z] + velocity[AXIS_Z];
    if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
      if (simulation->position[AXIS_Y] >= 0) {
        triangle.corners[AXIS_X][0] = 0;
        triangle.corners[AXIS_X][1] = 0x100;
        triangle.corners[AXIS_X][2] = 0;
        triangle.corners[AXIS_Y][0] = 0;
        triangle.corners[AXIS_Y][1] = 0;
        triangle.corners[AXIS_Y][2] = 0;
        triangle.corners[AXIS_Z][0] = 0;
        triangle.corners[AXIS_Z][1] = 0;
        triangle.corners[AXIS_Z][2] = 0x100;
        contact = STEP_FLAG_GROUND_HIT;
        if (ABS(simulation->position[AXIS_Y]) + ABS(startY) != 0) {
          fraction = (f32)simulation->position[AXIS_Y] /
              (ABS(simulation->position[AXIS_Y]) + ABS(startY));
        } else {
          fraction = 0.0f;
        }
      } else {
        contact = 0;
      }
    } else {
      if (simulation->state == BALL_STATE_IN_CUP) {
        contact = 0;
      } else {
        s32 endY;
        s32 hitsBillboard;
        s32 holeHit;

        if (g_GameState.gameMode == GAME_MODE_RING_SHOT &&
            (simulation->purpose == LAUNCH_PURPOSE_STROKE ||
                simulation->purpose == LAUNCH_PURPOSE_TRIAL) &&
            FindRingHit(startX, startY, startZ, simulation->position[AXIS_X],
                simulation->position[AXIS_Y], simulation->position[AXIS_Z],
                &hit, &hitFraction) > 0 &&
            g_Players[simulation->playerIndex].ringStates[hit.unk0] ==
                RING_STATE_SHOWN &&
            simulation->purpose == LAUNCH_PURPOSE_STROKE) {
          PlaySound(0x62, 0xE, SOUND_FULL_SCALE);
          if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
            g_Players[simulation->playerIndex].ringStates[hit.unk0] =
                RING_STATE_PASSED;
            StartRingPassRumble(&g_Players[simulation->playerIndex]);
          }
        }

        if (simulation->state == BALL_STATE_ROLLING && simulation->unkB4) {
          s32 groundHeight = GetGroundHeight(startX, startZ);

          if (groundHeight < startY) {
            startY = groundHeight;
          }

          simulation->unkB4 = 0;
        }

        if (g_GameState.isMiniGolf) {
          contact = FindMiniGolfGroundHit(simulation, startX, startY, startZ,
              simulation->position[AXIS_X], simulation->position[AXIS_Y],
              simulation->position[AXIS_Z], &hit, &hitFraction);
          if (g_GameState.isMiniGolf &&
              simulation->state == BALL_STATE_ROLLING) {
            hitsBillboard = FindMiniGolfGroundHit(simulation, startX, startY,
                startZ, simulation->position[AXIS_X],
                simulation->position[AXIS_Y] - 0x20C,
                simulation->position[AXIS_Z], &triangle, &fraction);
            if (hitsBillboard && (contact == 0 || fraction < hitFraction)) {
              contact = hitsBillboard;
              osSyncPrintf("PutterSpecialGroundCollision\n");
              hit = triangle;
              hitFraction = fraction;
            }
          }
        } else {
          endY = simulation->position[AXIS_Y];
          if (simulation->state == BALL_STATE_ROLLING &&
              GetGroundHeight(simulation->position[AXIS_X],
                  simulation->position[AXIS_Z]) < startY) {
            endY = startY;
          }

          contact = FindGroundHit(startX, startY, startZ,
              simulation->position[AXIS_X], endY, simulation->position[AXIS_Z],
              &hit, &hitFraction);
        }

        hitsBillboard = FALSE;
        if (simulation->state == BALL_STATE_FLYING &&
            (simulation->unk9C < 0 ||
                simulation->unk9C + 2 < simulation->unk98)) {
          hitsBillboard = FindBillboardHit(startX, startY, startZ,
              simulation->position[AXIS_X], simulation->position[AXIS_Y],
              simulation->position[AXIS_Z], &triangle.unk0, &fraction);
        }

        if (hitsBillboard && contact == STEP_FLAG_GROUND_HIT) {
          if (hitFraction <= fraction) {
            triangle = hit;
            fraction = hitFraction;
            contact = STEP_FLAG_GROUND_HIT;
          } else {
            contact = STEP_FLAG_BILLBOARD_HIT;
          }
        } else if (hitsBillboard) {
          contact = STEP_FLAG_BILLBOARD_HIT;
        } else if (contact == STEP_FLAG_GROUND_HIT) {
          triangle = hit;
          fraction = hitFraction;
        } else {
          contact = 0;
        }

        holeHit = FALSE;
        if (!simulation->unkA1) {
          if (simulation->state == BALL_STATE_FLYING ||
              simulation->state == BALL_STATE_ROLLING) {
            holeHit = FindHoleTriangleHit(startX, startY, startZ,
                simulation->position[AXIS_X], simulation->position[AXIS_Y],
                simulation->position[AXIS_Z], &hit, &hitFraction);
          } else {
            holeHit = FALSE;
          }
        }

        if (holeHit && (contact == 0 || hitFraction <= fraction)) {
          triangle = hit;
          contact = STEP_FLAG_HOLE_TRIANGLE_HIT;
          fraction = hitFraction;
          hitsHoleTriangle = TRUE;
        }
      }

      flags |= contact;
      if (printsDebugText) {
        PrintDebugPaddedHex(contact, 2, 6);
        osSyncPrintf("col%d\n", contact);
      }
    }

    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
      CoursePoint pinVelocity;
      f32 pinFraction;
      s32 pinHit;

      if (g_GameState.isMiniGolf ||
          GetSurfaceInfo(simulation->unkA2)->kind != SURFACE_KIND_GREEN) {
        pinHit =
            FindPinHit(startX, startY, startZ, simulation->position[AXIS_X],
                simulation->position[AXIS_Y], simulation->position[AXIS_Z],
                pinVelocity, &pinFraction, simulation->unkB0, simulation);
        if (pinHit && !(simulation->unkB1 & 8)) {
          simulation->unk94++;
          simulation->position[AXIS_X] =
              startX + (pinFraction * velocity[AXIS_X]);
          simulation->position[AXIS_Y] =
              startY + (pinFraction * velocity[AXIS_Y]);
          simulation->position[AXIS_Z] =
              startZ + (pinFraction * velocity[AXIS_Z]);
          if (pinHit == PIN_HIT_FLAG) {
            if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
              PlaySound(0xF, g_NextSoundChannel, SOUND_FULL_SCALE);
            }

            simulation->unkB0 = 1;
            simulation->windVelocityX = 0.0f;
            simulation->windVelocityZ = 0.0f;
          }

          if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
            if (pinHit == PIN_HIT_FLAGSTICK) {
              StartFlagstickSparks(simulation->position);
            }

            if (pinHit == PIN_HIT_FLAG) {
              StartFlagSparks(simulation->position);
            }
          }

          if (pinHit == PIN_HIT_FLAGSTICK) {
            f32 speed = sqrtf((velocity[AXIS_X] * velocity[AXIS_X]) +
                (velocity[AXIS_Z] * velocity[AXIS_Z]));

            if (speed > 142.2336f) {
              s32 volume = speed * 8.858665f;

              if (volume >= 127) {
                volume = 126;
              }

              if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                PlaySound(0xE, g_NextSoundChannel, volume);
              }
            }
          }

          if (simulation->state == BALL_STATE_ROLLING) {
            simulation->state = BALL_STATE_FLYING;
            velocity[AXIS_Y] = -sqrtf((velocity[AXIS_X] * velocity[AXIS_X]) +
                                   (velocity[AXIS_Z] * velocity[AXIS_Z])) *
                0.5f;
            simulation->unk3C = atan2(pinVelocity[AXIS_Z], pinVelocity[AXIS_X]);
            contact = 0;
            simulation->unk34 =
                IntLength2D(pinVelocity[AXIS_X], pinVelocity[AXIS_Z]);
          } else {
            velocity[AXIS_Y] = pinVelocity[AXIS_Y];
            simulation->unk34 = simulation->unk34 * 0.3f;
          }

          velocity[AXIS_X] = pinVelocity[AXIS_X];
          velocity[AXIS_Z] = pinVelocity[AXIS_Z];
          if (simulation->purpose != LAUNCH_PURPOSE_UNK5) {
            f32 rest = 1.3f - pinFraction;

            simulation->position[AXIS_X] =
                simulation->position[AXIS_X] + (rest * velocity[AXIS_X]);
            simulation->position[AXIS_Y] =
                simulation->position[AXIS_Y] + (rest * velocity[AXIS_Y]);
            simulation->position[AXIS_Z] =
                simulation->position[AXIS_Z] + (rest * velocity[AXIS_Z]);
          }

          flags |= STEP_FLAG_PIN_HIT;
          MoveBallToCupEdge(simulation);
          BuildUnk44Rotation();
        }
      }

      if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        s32 cupOffsetX =
            simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X];

        if (ABS(cupOffsetX) < 0x2400) {
          s32 cupOffsetZ =
              simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z];

          if (ABS(cupOffsetZ) < 0x2400) {
            s32 cupDistance = IntLength2D(cupOffsetX, cupOffsetZ);

            if (simulation->state != BALL_STATE_IN_CUP &&
                simulation->position[AXIS_Y] -
                        g_HoleSetup.cupPosition[AXIS_Y] >=
                    -0x2FF &&
                (f32)cupDistance < 1228.8f) {
              s32 speed = IntLength2D(velocity[AXIS_X], velocity[AXIS_Z]);

              if (speed < 1075.2f && velocity[AXIS_Y] >= -711.168f) {
                if (printsDebugText) {
                  SetDebugTextHighlight(1);
                  PrintDebugText("CUPIN!", 0x14, 0xA);
                }

                simulation->state = BALL_STATE_IN_CUP;
                if (speed < 1612.7999f) {
                  simulation->unkA8[0] = 1;
                }
              }
            }

            if ((f32)cupDistance < 1996.7999f && (f32)cupDistance >= 1075.2f) {
              isNearCup = TRUE;
              if (simulation->state == BALL_STATE_ROLLING &&
                  (f32)ABS(simulation->position[AXIS_Y] -
                      g_HoleSetup.cupPosition[AXIS_Y]) < 614.4f) {
                f32 toCup[3];
                f32 direction[3];
                f32 alignment;
                f32 depth;
                f32 angle;
                f32 pull;

                if (printsDebugText) {
                  SetDebugTextHighlight(1);
                  PrintDebugText("KURU!", 0x14, 0xA);
                }

                toCup[AXIS_Y] = 0.0f;
                toCup[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X] -
                    simulation->position[AXIS_X];
                toCup[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z] -
                    simulation->position[AXIS_Z];
                guNormalize(&toCup[AXIS_X], &toCup[AXIS_Y], &toCup[AXIS_Z]);
                direction[AXIS_X] = velocity[AXIS_X];
                direction[AXIS_Y] = 2096.0f;
                direction[AXIS_Z] = velocity[AXIS_Z];
                guNormalize(
                    &direction[AXIS_X], &direction[AXIS_Y], &direction[AXIS_Z]);
                alignment = (toCup[AXIS_X] * direction[AXIS_X]) +
                    (toCup[AXIS_Z] * direction[AXIS_Z]);
                if (alignment < 0.0f) {
                  alignment = 0.0f;
                }

                depth = 1996.7999f - (f32)cupDistance;
                angle = atan2(toCup[AXIS_Z], toCup[AXIS_X]);
                alignment = 1.0f - alignment;
                if (depth > 614.4f) {
                  depth = 614.4f;
                }

                pull = ((sinf(depth * 0.005113269f) * 307.2f) + 46.079998f) *
                    alignment;
                velocity[AXIS_X] *= 0.93f;
                velocity[AXIS_Z] *= 0.93f;
                velocity[AXIS_X] += cosf(angle) * pull;
                velocity[AXIS_Z] += sinf(angle) * pull;
                flags |= STEP_FLAG_CUP_PULL;
              }
            }

            if ((f32)cupDistance >= 1996.7999f &&
                (f32)IntLength2D(startX - g_HoleSetup.cupPosition[AXIS_X],
                    startZ - g_HoleSetup.cupPosition[AXIS_Z]) < 1996.7999f &&
                (f32)ABS(startY - g_HoleSetup.cupPosition[AXIS_Y]) < 614.4f) {
              f32 fromCup[3];
              f32 direction[3];
              f32 speed;
              f32 outwardSpeed;
              f32 alignment;

              fromCup[AXIS_X] = simulation->position[AXIS_X] -
                  g_HoleSetup.cupPosition[AXIS_X];
              fromCup[AXIS_Y] = 0.0f;
              fromCup[AXIS_Z] = simulation->position[AXIS_Z] -
                  g_HoleSetup.cupPosition[AXIS_Z];
              direction[AXIS_X] = velocity[AXIS_X];
              direction[AXIS_Y] = 0.0f;
              direction[AXIS_Z] = velocity[AXIS_Z];
              speed = sqrtf((velocity[AXIS_X] * velocity[AXIS_X]) +
                  (velocity[AXIS_Z] * velocity[AXIS_Z]));
              guNormalize(&fromCup[AXIS_X], &fromCup[AXIS_Y], &fromCup[AXIS_Z]);
              guNormalize(
                  &direction[AXIS_X], &direction[AXIS_Y], &direction[AXIS_Z]);
              outwardSpeed = (velocity[AXIS_X] * fromCup[AXIS_X]) +
                  (velocity[AXIS_Z] * fromCup[AXIS_Z]);
              alignment = (direction[AXIS_X] * fromCup[AXIS_X]) +
                  (direction[AXIS_Z] * fromCup[AXIS_Z]);
              osSyncPrintf("InnerMult %f, %f, %d\n", alignment, outwardSpeed,
                  simulation->state);
              if (alignment >= 17203.2f) {
                osSyncPrintf("Through Cup\n");
              } else if (simulation->state == BALL_STATE_ROLLING &&
                  alignment >= 0.6f) {
                f32 keep = speed * 0.0004650298f;

                if (keep < 0.0f) {
                  keep = 0.0f;
                }

                if (keep > 1.0f) {
                  keep = 1.0f;
                }

                velocity[AXIS_Y] =
                    -speed * (sqrtf(1.0f - (keep * keep)) + 0.1f);
                velocity[AXIS_X] *= keep;
                velocity[AXIS_Z] *= keep;
                simulation->state = BALL_STATE_FLYING;
                simulation->unk3C = atan2(velocity[AXIS_Z], velocity[AXIS_X]);
                simulation->unk34 =
                    IntLength2D(velocity[AXIS_X], velocity[AXIS_Z]) * 0.5f;
                BuildUnk44Rotation();
                contact = 0;
                flags |= STEP_FLAG_LIPPED_OUT;
                if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                  StartFlagstickSparks(simulation->position);
                }

                PlayStrokeImpactSound(0x10, 711.168f, outwardSpeed);
              }
            }
          }
        }
      }
    }

    if (simulation->state == BALL_STATE_IN_WATER) {
      simulation->unkA1 = TRUE;
      velocity[AXIS_X] *= 0.7f;
      velocity[AXIS_Z] *= 0.7f;
      velocity[AXIS_Y] = (velocity[AXIS_Y] + 262.0f) * 0.7f;
      if (simulation->position[AXIS_Y] <=
          GetGroundOrWaterHeight(
              simulation->position[AXIS_X], simulation->position[AXIS_Z]) +
              0xB400) {
        if (simulation->position[AXIS_Y] >=
            GetGroundHeight(
                simulation->position[AXIS_X], simulation->position[AXIS_Z])) {
          simulation->state = BALL_STATE_ROLLING;
          simulation->unkB2 = 0;
          goto SETTLE;
        }
      } else {
        simulation->state = BALL_STATE_STOPPED;
      }
    }

    if (simulation->state == BALL_STATE_ROLLING && simulation->unkA1 &&
        simulation->position[AXIS_Y] >
            GetGroundOrWaterHeight(
                simulation->position[AXIS_X], simulation->position[AXIS_Z]) +
                0xB400) {
      simulation->state = BALL_STATE_STOPPED;
    }

    if (simulation->state == BALL_STATE_IN_CUP) {
      f32 speed;
      f32 cupDistance;

      simulation->surface = SURFACE_GREEN;
      speed = sqrtf((velocity[AXIS_X] * velocity[AXIS_X]) +
          (velocity[AXIS_Z] * velocity[AXIS_Z]));
      cupDistance = (s32)IntLength2D(
          simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
          simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]);
      if (cupDistance >= 1228.8f) {
        f32 toCupX;
        f32 toCupY;
        f32 toCupZ;
        f32 push;
        f32 scale;

        toCupY = 0.0f;
        toCupX = g_HoleSetup.cupPosition[AXIS_X] - simulation->position[AXIS_X];
        toCupZ = g_HoleSetup.cupPosition[AXIS_Z] - simulation->position[AXIS_Z];
        guNormalize(&toCupX, &toCupY, &toCupZ);
        push = ((toCupX * velocity[AXIS_X]) + (toCupY * velocity[AXIS_Y]) +
                   (toCupZ * velocity[AXIS_Z])) *
            1.5f;
        scale = 1075.2f / cupDistance;
        velocity[AXIS_X] -= toCupX * push;
        velocity[AXIS_Y] -= toCupY * push;
        velocity[AXIS_Z] -= toCupZ * push;
        simulation->position[AXIS_X] =
            ((simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X]) *
                scale) +
            g_HoleSetup.cupPosition[AXIS_X];
        simulation->position[AXIS_Z] =
            ((simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]) *
                scale) +
            g_HoleSetup.cupPosition[AXIS_Z];
        if (speed >= 711.168f) {
          s32 volume = speed * 0.88586664f;

          if (volume >= 127) {
            volume = 126;
          }

          if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
            PlaySound(0x10, g_NextSoundChannel, volume);
          }
        }
      }

      if (simulation->position[AXIS_Y] <
              g_HoleSetup.cupPosition[AXIS_Y] + 768.0f &&
          velocity[AXIS_Y] < -711.168f) {
        s32 toCupX =
            g_HoleSetup.cupPosition[AXIS_X] - simulation->position[AXIS_X];
        s32 toCupZ =
            g_HoleSetup.cupPosition[AXIS_Z] - simulation->position[AXIS_Z];

        if ((toCupX | toCupZ) != 0) {
          s32 scale = AbsoluteValueExpression(velocity[AXIS_Y]) /
              IntLength2D(toCupX, toCupZ);

          velocity[AXIS_X] = toCupX * scale;
          velocity[AXIS_Z] = toCupZ * scale;
          simulation->position[AXIS_Y] =
              g_HoleSetup.cupPosition[AXIS_Y] + 768.0f;
        }

        velocity[AXIS_Y] = AbsoluteValueExpression(velocity[AXIS_Y]);
      }

      if (simulation->position[AXIS_Y] >=
          g_HoleSetup.cupPosition[AXIS_Y] + 0xC00) {
        f32 fallSpeed = velocity[AXIS_Y];

        /* The chained assignment is there to match */
        velocity[AXIS_X] = velocity[AXIS_Z] = 0.0f;
        velocity[AXIS_Y] = (-fallSpeed * 0.5f) + 131.0f;
        if (velocity[AXIS_Y] > 0.0f) {
          simulation->unkA0 = 1;
          simulation->state = BALL_STATE_STOPPED;
        }

        if (simulation->unkA8[0] == 1) {
          if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
            PlaySound(0x12, g_NextSoundChannel, SOUND_FULL_SCALE);
          }

          simulation->unkA8[0] = 2;
        } else if (simulation->unkA8[0] == 0) {
          if (fallSpeed >= 284.4672f) {
            s32 volume = fallSpeed * 0.88586664f;

            if (volume >= 128) {
              volume = 127;
            }

            if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
              PlaySound(0x11, g_NextSoundChannel, volume);
            }
          }
        }

        simulation->position[AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y] + 0xC00;
      } else {
        velocity[AXIS_Y] += 131.0f;
      }
    }

    if (contact == STEP_FLAG_BILLBOARD_HIT &&
        simulation->state == BALL_STATE_FLYING) {
      simulation->unk94++;
      simulation->position[AXIS_X] = startX + (fraction * velocity[AXIS_X]);
      simulation->position[AXIS_Y] = startY + (fraction * velocity[AXIS_Y]);
      simulation->position[AXIS_Z] = startZ + (fraction * velocity[AXIS_Z]);
      simulation->unk9C = simulation->unk98;
      simulation->unk34 /= 2;
      simulation->unk38 /= 2;
      switch (triangle.unk0) {
        case 1:
        case 4:
        case 5:
          velocity[AXIS_X] = -velocity[AXIS_Z] * 0.5f;
          velocity[AXIS_Z] = -velocity[AXIS_X] * 0.5f;
          break;
        case 2:
        default:
          velocity[AXIS_X] *= 0.5f;
          velocity[AXIS_Z] *= 0.5f;
          break;
      }

      if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
        PlayBillboardHitSound(triangle.unk0,
            IntLength3D(velocity[AXIS_X], velocity[AXIS_Y], velocity[AXIS_Z]));
      }
    }

    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST &&
        (g_GameState.isMiniGolf || IsGameFlagSet(GAME_FLAG_WALL_HITS))) {
      CoursePoint hitPosition;
      f32 reflected[3];
      PlaneTriangle wall;
      f32 wallFraction;
      f32 normalX;
      f32 normalY;
      f32 normalZ;
      s32 endY;

      if (simulation->state == BALL_STATE_ROLLING && contact) {
        endY = GetGroundHeight(
            simulation->position[AXIS_X], simulation->position[AXIS_Z]);
      } else {
        endY = simulation->position[AXIS_Y];
      }

      if (FindWallHit(startX, startY, startZ, simulation->position[AXIS_X],
              endY, simulation->position[AXIS_Z], &wall,
              &wallFraction) == TRUE &&
          wall.unk0 == 3) {
        f32 bounce = 1.0f;
        f32 inwardSpeed;
        f32 slide;
        f32 along;
        f32 edgeX;
        f32 edgeY;
        f32 edgeZ;
        f32 squareX;
        f32 squareZ;

        normalX = wall.corners[AXIS_X][0];
        edgeY = 0.0f;
        normalY = edgeY;
        normalZ = wall.corners[AXIS_Z][0];
        if (wall.corners[AXIS_X][0] == wall.corners[AXIS_X][1] &&
            wall.corners[AXIS_Z][0] == wall.corners[AXIS_Z][1]) {
          edgeX = wall.corners[AXIS_X][2];
          edgeZ = wall.corners[AXIS_Z][2];
        } else {
          edgeX = wall.corners[AXIS_X][1];
          edgeZ = wall.corners[AXIS_Z][1];
        }

        normalX -= edgeX;
        normalY -= edgeY;
        normalZ -= edgeZ;
        guNormalize(&normalX, &normalY, &normalZ);
        inwardSpeed = (-normalZ * velocity[AXIS_X]) +
            (normalY * velocity[AXIS_Y]) + (normalX * velocity[AXIS_Z]);
        PlayStrokeImpactSound(
            0x15, 1422.336f, AbsoluteValueExpression(inwardSpeed));
        squareX = velocity[AXIS_X] * velocity[AXIS_X];
        squareZ = velocity[AXIS_Z] * velocity[AXIS_Z];
        along = wallFraction;
        if (along < 0.0f) {
          along = 0.0f;
        }

        hitPosition[AXIS_X] = startX + (along * (velocity[AXIS_X] * 0.9f));
        hitPosition[AXIS_Y] = startY + (along * (velocity[AXIS_Y] * 0.9f));
        hitPosition[AXIS_Z] = startZ + (along * (velocity[AXIS_Z] * 0.9f));
        if (wallFraction < 0.0f && fabsf(inwardSpeed) < 230.40001f) {
          f32 speed = sqrtf(squareX + squareZ);

          if (inwardSpeed < 0.0f) {
            inwardSpeed -= 230.40001f;
          } else {
            inwardSpeed += 230.40001f;
          }

          bounce = speed / (speed + 230.40001f);
          osSyncPrintf("SpecialWallRefrect\n");
        }

        simulation->position[AXIS_X] = hitPosition[AXIS_X];
        simulation->position[AXIS_Y] = hitPosition[AXIS_Y];
        simulation->position[AXIS_Z] = hitPosition[AXIS_Z];
        if (simulation->purpose == LAUNCH_PURPOSE_STROKE &&
            fabsf(inwardSpeed) > 153.6f) {
          StartFlagstickSparks(simulation->position);
        }

        reflected[AXIS_X] =
            velocity[AXIS_X] - (-normalZ * (2.0f * inwardSpeed));
        reflected[AXIS_Y] = velocity[AXIS_Y] - (normalY * (2.0f * inwardSpeed));
        reflected[AXIS_Z] = velocity[AXIS_Z] - (normalX * (2.0f * inwardSpeed));
        velocity[AXIS_X] = reflected[AXIS_X] * bounce;
        velocity[AXIS_Y] = reflected[AXIS_Y] * bounce;
        velocity[AXIS_Z] = reflected[AXIS_Z] * bounce;
        slide = 1.0f - bounce;
        simulation->position[AXIS_X] =
            simulation->position[AXIS_X] + (velocity[AXIS_X] * slide);
        simulation->position[AXIS_Y] =
            simulation->position[AXIS_Y] + (velocity[AXIS_Y] * slide);
        simulation->unk34 = 0;
        simulation->position[AXIS_Z] =
            simulation->position[AXIS_Z] + (velocity[AXIS_Z] * slide);
        BuildUnk44Rotation();
        PushBallOutOfWalls(simulation, FALSE);
        goto SETTLE;
      }

      PushBallOutOfWalls(simulation, FALSE);
    }

    if (!simulation->unkA1 &&
        ((contact == STEP_FLAG_HOLE_TRIANGLE_HIT &&
             simulation->state == BALL_STATE_FLYING) ||
            (simulation->state == BALL_STATE_ROLLING && hitsHoleTriangle))) {
      CoursePoint bounced;
      TrianglePlane plane;
      BallSimulation impact;
      f32 impactSpeed;
      f32 steepness;
      s32 speed;
      f32 softness;
      f32 bounce;
      f32 push;
      f32 keep;

      simulation->unk94++;
      GetTrianglePlane(&triangle, &plane);
      simulation->position[AXIS_X] =
          startX + (fraction * (velocity[AXIS_X] * 0.995f));
      simulation->position[AXIS_Y] =
          startY + (fraction * (velocity[AXIS_Y] * 0.995f));
      simulation->position[AXIS_Z] =
          startZ + (fraction * (velocity[AXIS_Z] * 0.995f));
      substepCount += (fraction < 0.4f) & (substepCount == 1);
      if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
        simulation->surface = SURFACE_FAIRWAY;
      } else {
        simulation->surface = SURFACE_WATER_HAZARD;
        if (simulation->unk94 == 1) {
          simulation->unkA7 = simulation->surface;
        }

        if (simulation->surface == SURFACE_GREEN ||
            simulation->surface == SURFACE_GREEN_EDGE) {
          plane.normal[AXIS_Y] = (plane.normal[AXIS_Y] * 0.5f) + 0.5f;
          guNormalize(&plane.normal[AXIS_X], &plane.normal[AXIS_Y],
              &plane.normal[AXIS_Z]);
        }
      }

      impact = *simulation;
      impact.position[AXIS_X] = startX + (fraction * velocity[AXIS_X]);
      impact.position[AXIS_Y] = startY + (fraction * velocity[AXIS_Y]);
      impact.position[AXIS_Z] = startZ + (fraction * velocity[AXIS_Z]);
      impactSpeed = (plane.normal[AXIS_X] * velocity[AXIS_X]) +
          (plane.normal[AXIS_Y] * velocity[AXIS_Y]) +
          (plane.normal[AXIS_Z] * velocity[AXIS_Z]);
      speed = IntLength3D(velocity[AXIS_X], velocity[AXIS_Y], velocity[AXIS_Z]);
      if (speed != 0) {
        steepness = fabsf(impactSpeed) / speed;
      } else {
        steepness = 1.0f;
      }

      switch (GetSurfaceInfo(simulation->surface)->kind) {
        case SURFACE_KIND_GREEN:
        case SURFACE_KIND_GREEN_EDGE:
          if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
            PlaySound(5, g_NextSoundChannel, SOUND_FULL_SCALE);
          }

          break;
        case SURFACE_KIND_FAIRWAY:
        case SURFACE_KIND_FAIRWAY_FAIR_LIE:
        case SURFACE_KIND_FAIRWAY_POOR_LIE:
        case SURFACE_KIND_SEMI_ROUGH:
          if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
            PlaySound(2, g_NextSoundChannel, SOUND_FULL_SCALE);
          }

          break;
        case SURFACE_KIND_ROUGH:
        case SURFACE_KIND_ROUGH_POOR_LIE:
        case 5:
        case SURFACE_KIND_DEEP_ROUGH:
          if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
            PlaySound(3, g_NextSoundChannel, SOUND_FULL_SCALE);
          }

          break;
        case SURFACE_KIND_BUNKER:
        case SURFACE_KIND_BUNKER_FAIR_LIE:
        case SURFACE_KIND_BUNKER_POOR_LIE:
        case SURFACE_KIND_BUNKER_UNK9:
        case SURFACE_KIND_BARE_GROUND:
          if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
            PlaySound(4, g_NextSoundChannel, SOUND_FULL_SCALE);
          }

          break;
        case SURFACE_KIND_WATER_HAZARD:
          if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
            PlaySound(9, g_NextSoundChannel, SOUND_FULL_SCALE);
          }

          break;
        case SURFACE_KIND_OUT_OF_BOUNDS:
        case SURFACE_KIND_CART_WAY:
        case SURFACE_KIND_TEE_GROUND:
        case SURFACE_KIND_ROCK:
          if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
            PlaySound(
                SOUND_CONTACT_PLAIN, g_NextSoundChannel, SOUND_FULL_SCALE);
          }

          break;
      }

      flags |= STEP_FLAG_HOLE_TRIANGLE_BOUNCE;
      if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
        SpawnImpactEffects(&impact);
        StartCheepCheepLeap(0);
      }

      if (simulation->surface == SURFACE_WATER_HAZARD &&
          (simulation->state == BALL_STATE_ROLLING ||
              fabsf(impactSpeed) >= 14223.36f ||
              fabsf(impactSpeed) <= 426.7008f ||
              sqrtf((velocity[AXIS_X] * velocity[AXIS_X]) +
                  (velocity[AXIS_Z] * velocity[AXIS_Z])) *
                      0.4f <=
                  fabsf(impactSpeed))) {
        simulation->state = BALL_STATE_IN_WATER;
        impactSpeed = 0.0f;
        velocity[AXIS_X] = velocity[AXIS_X] * 0.2f * 0.2f;
      }

      softness = 1000 - GetSurfaceInfo(simulation->surface)->unkE;
      bounce = 1.0f - (softness * steepness * 0.001f);
      push = (bounce + 1.0f) * impactSpeed;
      bounced[AXIS_X] = velocity[AXIS_X] - (plane.normal[AXIS_X] * push);
      bounced[AXIS_Y] = velocity[AXIS_Y] - (plane.normal[AXIS_Y] * push);
      bounced[AXIS_Z] = velocity[AXIS_Z] - (plane.normal[AXIS_Z] * push);
      keep = steepness * 0.1f;
      keep = softness * keep;
      keep = (1000.0f - (keep * steepness)) * 0.001f;
      if (simulation->state == BALL_STATE_IN_WATER) {
        keep = 0.0f;
      }

      velocity[AXIS_X] = bounced[AXIS_X] * keep;
      velocity[AXIS_Y] = bounced[AXIS_Y] * keep;
      velocity[AXIS_Z] = bounced[AXIS_Z] * keep;
      simulation->unk9C = simulation->unk98;
      if (simulation->state == BALL_STATE_FLYING) {
        s32 unk34X = simulation->unk34 * cosf(simulation->unk3C);
        s32 unk34Z = simulation->unk34 * sinf(simulation->unk3C);
        f32 grip = 1.0f - bounce;

        velocity[AXIS_X] += (unk34X - velocity[AXIS_X]) * grip *
            AbsoluteValueExpression(plane.normal[AXIS_Y]) * 0.25f;
        velocity[AXIS_Z] += (unk34Z - velocity[AXIS_Z]) * grip *
            AbsoluteValueExpression(plane.normal[AXIS_Y]) * 0.25f;
        unk34X *= 0.9f;
        unk34Z *= 0.9f;
        unk34X = unk34X + ((previousVelocityX - unk34X) * 0.25f);
        unk34Z = unk34Z + ((previousVelocityZ - unk34Z) * 0.25f);
        simulation->unk3C = atan2(unk34Z, unk34X);
        simulation->unk34 = IntLength2D(unk34X, unk34Z);
        BuildUnk44Rotation();
      }

      simulation->unk38 = 0;
      velocity[AXIS_Y] += 262.0f;
    }

    if (simulation->state == BALL_STATE_FLYING &&
        contact == STEP_FLAG_GROUND_HIT) {
      TrianglePlane plane;
      f32 speed;
      s32 cupDistance;
      f32 impactSpeed;
      f32 slide;
      SurfaceInfo* info;
      f32 friction;
      f32 spin;
      f32 restitution;

      simulation->unk94++;
      speed = sqrtf((velocity[AXIS_X] * velocity[AXIS_X]) +
          (velocity[AXIS_Y] * velocity[AXIS_Y]) +
          (velocity[AXIS_Z] * velocity[AXIS_Z]));
      simulation->position[AXIS_X] = startX + (fraction * velocity[AXIS_X]);
      simulation->position[AXIS_Y] = startY + (fraction * velocity[AXIS_Y]);
      simulation->position[AXIS_Z] = startZ + (fraction * velocity[AXIS_Z]);
      cupDistance = IntLength2D(
          g_HoleSetup.cupPosition[AXIS_X] - simulation->position[AXIS_X],
          g_HoleSetup.cupPosition[AXIS_Z] - simulation->position[AXIS_Z]);
      if ((f32)cupDistance < 1996.7999f && (f32)cupDistance >= 1075.2f) {
        f32 toCup[3];

        toCup[AXIS_Y] = 1.0f;
        toCup[AXIS_X] =
            g_HoleSetup.cupPosition[AXIS_X] - simulation->position[AXIS_X];
        toCup[AXIS_Z] =
            g_HoleSetup.cupPosition[AXIS_Z] - simulation->position[AXIS_Z];
        guNormalize(&toCup[AXIS_X], &toCup[AXIS_Y], &toCup[AXIS_Z]);
        plane.normal[AXIS_X] = toCup[AXIS_X];
        plane.normal[AXIS_Z] = toCup[AXIS_Z];
        plane.normal[AXIS_Y] = toCup[AXIS_Y] - 0.7f;
        guNormalize(&plane.normal[AXIS_X], &plane.normal[AXIS_Y],
            &plane.normal[AXIS_Z]);
        if ((plane.normal[AXIS_X] * velocity[AXIS_X]) +
                    (plane.normal[AXIS_Z] * velocity[AXIS_Z]) >=
                0.0f ||
            fabsf(velocity[AXIS_Y]) * 0.5f <
                sqrtf((velocity[AXIS_X] * velocity[AXIS_X]) +
                    (velocity[AXIS_Z] * velocity[AXIS_Z]))) {
          GetTrianglePlane(&triangle, &plane);
        }

        osSyncPrintf("Baki\n");
      } else {
        GetTrianglePlane(&triangle, &plane);
      }

      substepCount += (fraction < 0.4f) & (substepCount == 1);
      if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST &&
          speed >= 711.168f && !g_GameState.isMiniGolf) {
        f32 side[3];

        side[AXIS_Y] = 0.0f;
        side[AXIS_X] = velocity[AXIS_X];
        side[AXIS_Z] = velocity[AXIS_Z];
        if (fabsf(velocity[AXIS_X]) + fabsf(velocity[AXIS_Z]) >= 711.168f) {
          f32 tilt = (GetStepNoise() - 0.5f) * 0.1f;

          if (tilt < 0.0f) {
            tilt -= 0.02f;
          } else {
            tilt += 0.02f;
          }

          guNormalize(&side[AXIS_X], &side[AXIS_Y], &side[AXIS_Z]);
          side[AXIS_X] *= tilt;
          side[AXIS_Z] *= tilt;
          plane.normal[AXIS_X] -= side[AXIS_Z];
          plane.normal[AXIS_Z] += side[AXIS_X];
          guNormalize(&plane.normal[AXIS_X], &plane.normal[AXIS_Y],
              &plane.normal[AXIS_Z]);
        }
      }

      if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
        simulation->surface = SURFACE_FAIRWAY;
      } else {
        simulation->surface = GetSurface(
            simulation->position[AXIS_X], simulation->position[AXIS_Z]);
        if (simulation->unk94 == 1) {
          simulation->unkA7 = simulation->surface;
        }
      }

      impactSpeed = (plane.normal[AXIS_X] * velocity[AXIS_X]) +
          (plane.normal[AXIS_Y] * velocity[AXIS_Y]) +
          (plane.normal[AXIS_Z] * velocity[AXIS_Z]);
      if (speed >= 0.0001f) {
        f32 cosine = fabsf(impactSpeed) / speed;

        if (cosine < 1.0f) {
          slide = sqrtf(1.0f - (cosine * cosine));
        } else {
          slide = 0.0f;
        }
      } else {
        slide = 0.1f;
      }

      switch (GetSurfaceInfo(simulation->surface)->kind) {
        case SURFACE_KIND_GREEN:
        case SURFACE_KIND_GREEN_EDGE:
          PlayStrokeImpactSound(5, 2844.672f, impactSpeed);
          break;
        case SURFACE_KIND_FAIRWAY:
        case SURFACE_KIND_FAIRWAY_FAIR_LIE:
        case SURFACE_KIND_FAIRWAY_POOR_LIE:
        case SURFACE_KIND_SEMI_ROUGH:
          PlayStrokeImpactSound(2, 2844.672f, impactSpeed);
          break;
        case SURFACE_KIND_BUNKER:
        case SURFACE_KIND_BUNKER_FAIR_LIE:
        case SURFACE_KIND_BUNKER_POOR_LIE:
        case SURFACE_KIND_BUNKER_UNK9:
        case SURFACE_KIND_BARE_GROUND:
        case SURFACE_KIND_WASTE_AREA:
          PlayStrokeImpactSound(4, 2844.672f, impactSpeed);
          break;
        case SURFACE_KIND_ROUGH:
        case SURFACE_KIND_ROUGH_POOR_LIE:
        case 5:
        case SURFACE_KIND_OUT_OF_BOUNDS:
        case SURFACE_KIND_CART_WAY:
        case SURFACE_KIND_TEE_GROUND:
        case SURFACE_KIND_ROCK:
        case SURFACE_KIND_DEEP_ROUGH:
          PlayStrokeImpactSound(3, 2844.672f, impactSpeed);
          break;
      }

      if (simulation->purpose == LAUNCH_PURPOSE_STROKE && !simulation->unkA1) {
        SpawnImpactEffects(simulation);
      }

      info = GetSurfaceInfo(simulation->surface);
      friction = (1000 - info->unkE) * 0.001f;
      if (simulation->surface == SURFACE_BUNKER) {
        s32 burial = 0;

        if (impactSpeed >= 9956.353f) {
          burial = 2;
        } else if (impactSpeed >= 7111.68f) {
          burial = 1;
        }

        switch (burial) {
          case 1:
            simulation->surface = SURFACE_BUNKER_FAIR_LIE;
            simulation->position[AXIS_Y] = GetGroundHeight(
                simulation->position[AXIS_X], simulation->position[AXIS_Z]);
            simulation->state = BALL_STATE_STOPPED;
            break;
          case 2:
            simulation->surface = SURFACE_BUNKER_POOR_LIE;
            simulation->position[AXIS_Y] = GetGroundHeight(
                simulation->position[AXIS_X], simulation->position[AXIS_Z]);
            simulation->state = BALL_STATE_STOPPED;
            break;
        }
      } else if (info->kind == SURFACE_KIND_OUT_OF_COURSE ||
          ((info->kind == SURFACE_KIND_OUT_OF_BOUNDS ||
               info->kind == SURFACE_KIND_WATER_HAZARD) &&
              impactSpeed < 1422.336f)) {
        simulation->state = BALL_STATE_STOPPED;
      }

      switch (GetSurfaceInfo(simulation->surface)->kind) {
        case SURFACE_KIND_GREEN:
        case SURFACE_KIND_GREEN_EDGE:
          friction *=
              ((fabsf(impactSpeed + (impactSpeed * GetPositionNoise() * 0.1f)) *
                   0.0001687365f) +
                  0.6f);
          if (friction > 0.8f) {
            friction = 0.8f;
          }

          friction -= GetStepNoise() * 0.1f;
          if (friction < 0.05f) {
            friction = 0.05f;
          }

          spin = 1.0f -
              (sqrtf((speed * speed) -
                   (impactSpeed * (1.0f - friction) * impactSpeed *
                       (1.0f - friction))) /
                  (speed * slide));
          if (spin > 0.01f) {
            spin = 0.01f;
          }

          if (spin < -0.2f) {
            spin = -0.2f;
          }

          break;
        case SURFACE_KIND_FAIRWAY:
        case SURFACE_KIND_FAIRWAY_FAIR_LIE:
        case SURFACE_KIND_FAIRWAY_POOR_LIE:
        case SURFACE_KIND_SEMI_ROUGH:
          friction *=
              ((fabsf(impactSpeed + (impactSpeed * GetPositionNoise() * 0.1f)) *
                   0.000112491f) +
                  0.3f);
          if (friction > 1.0f) {
            friction = 1.0f;
          }

          spin = 1.0f -
              (sqrtf((speed * speed) -
                   (impactSpeed * (1.0f - friction) * impactSpeed *
                       (1.0f - friction))) /
                  (speed * slide));
          if (spin > 0.01f) {
            spin = 0.01f;
          }

          if (spin < -0.2f) {
            spin = -0.2f;
          }

          break;
        case SURFACE_KIND_BUNKER:
        case SURFACE_KIND_BUNKER_FAIR_LIE:
        case SURFACE_KIND_BUNKER_POOR_LIE:
          friction *= ((fabsf(impactSpeed) * 0.00007030687f) + 0.3f);
          spin = 0.7f;
          friction += friction * GetPositionNoise() * 0.1f;
          break;
        case SURFACE_KIND_ROUGH:
        case SURFACE_KIND_ROUGH_POOR_LIE:
        case 5:
          friction *= ((fabsf(impactSpeed) * 0.00007030687f) + 0.3f);
          spin = 0.4f;
          friction += friction * GetPositionNoise() * 0.1f;
          break;
        default:
          friction *= ((fabsf(impactSpeed) * 0.00007030687f) + 0.3f);
          spin = 0.2f;
          friction += friction * GetPositionNoise() * 0.1f;
          break;
      }

      if (g_GameState.isMiniGolf) {
        spin = 0.0f;
      }

      restitution = 1.0f - friction;
      if (restitution < 0.0f) {
        restitution = 0.0f;
      } else if (restitution > 1.5f) {
        restitution = 1.5f;
      }

      if (simulation->state != BALL_STATE_ROLLING && impactSpeed < 7111.68f &&
          simulation->unk98 - 1 == simulation->unk9C) {
        s32 unk34X;
        s32 unk34Z;

        simulation->state = BALL_STATE_ROLLING;
        simulation->unkB2 = 0;
        if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
          simulation->position[AXIS_Y] = 0;
        } else {
          simulation->position[AXIS_Y] = GetGroundHeight(
              simulation->position[AXIS_X], simulation->position[AXIS_Z]);
        }

        unk34X = simulation->unk34 * cosf(simulation->unk3C);
        unk34Z = simulation->unk34 * sinf(simulation->unk3C);
        velocity[AXIS_X] += (unk34X - velocity[AXIS_X]) *
            AbsoluteValue(plane.normal[AXIS_Y]) * 0.5f;
        velocity[AXIS_Z] += (unk34Z - velocity[AXIS_Z]) *
            AbsoluteValue(plane.normal[AXIS_Y]) * 0.5f;
        simulation->unk34 = 0;
      } else {
        f32 keep = 1.0f - spin;
        f32 normalPart[3];
        f32 tangent[3];
        s32 unk34X;
        s32 unk34Z;
        f32 grip;
        s32 driftX;
        s32 driftZ;
        f32 driftLimit;
        f32 drift;

        if (keep <= 0.1f) {
          keep = 0.1f;
        }

        if (simulation->unk98 < 2 && !g_GameState.isMiniGolf) {
          keep *= 0.25;
        }

        normalPart[AXIS_X] = -plane.normal[AXIS_X] * impactSpeed;
        normalPart[AXIS_Y] = -plane.normal[AXIS_Y] * impactSpeed;
        normalPart[AXIS_Z] = -plane.normal[AXIS_Z] * impactSpeed;
        tangent[AXIS_X] = velocity[AXIS_X] + normalPart[AXIS_X];
        tangent[AXIS_Y] = velocity[AXIS_Y] + normalPart[AXIS_Y];
        tangent[AXIS_Z] = velocity[AXIS_Z] + normalPart[AXIS_Z];
        velocity[AXIS_X] =
            (tangent[AXIS_X] * keep) + (normalPart[AXIS_X] * restitution);
        velocity[AXIS_Y] =
            (tangent[AXIS_Y] * keep) + (normalPart[AXIS_Y] * restitution);
        velocity[AXIS_Z] =
            (tangent[AXIS_Z] * keep) + (normalPart[AXIS_Z] * restitution);
        simulation->unk9C = simulation->unk98;
        unk34X = simulation->unk34 * cosf(simulation->unk3C);
        unk34Z = simulation->unk34 * sinf(simulation->unk3C);
        grip = 1.0f - restitution;
        driftX = (unk34X - velocity[AXIS_X]) *
            (grip * fabsf(plane.normal[AXIS_Y]) * 0.5f);
        driftZ = (unk34Z - velocity[AXIS_Z]) *
            (grip * fabsf(plane.normal[AXIS_Y]) * 0.5f);
        drift = (s32)IntLength2D(driftX, driftZ);
        driftLimit = impactSpeed * 0.4f;
        if (driftLimit < drift && impactSpeed > 1.0f) {
          f32 scale = driftLimit / drift;

          driftX = driftX * scale;
          driftZ = driftZ * scale;
        }

        unk34X *= 0.9f;
        unk34Z *= 0.9f;
        velocity[AXIS_X] += driftX;
        velocity[AXIS_Z] += driftZ;
        unk34X = unk34X + ((previousVelocityX - unk34X) * 0.2f);
        unk34Z = unk34Z + ((previousVelocityZ - unk34Z) * 0.2f);
        simulation->unk3C = atan2(unk34Z, unk34X);
        simulation->unk34 = IntLength2D(unk34X, unk34Z);
        BuildUnk44Rotation();
        simulation->unk38 = 0;
        velocity[AXIS_Y] += 262.0f;
      }
    }

    if (!simulation->unkA1 && simulation->state == BALL_STATE_ROLLING &&
        contact == 0 && !isNearCup) {
      simulation->state = BALL_STATE_FLYING;
      simulation->unk3C = atan2(velocity[AXIS_Z], velocity[AXIS_X]);
      simulation->unk34 = IntLength2D(velocity[AXIS_X], velocity[AXIS_Z]);
      BuildUnk44Rotation();
    }

    if (simulation->unkA1 && simulation->state == BALL_STATE_ROLLING &&
        contact == 0 &&
        FindGroundTriangle(simulation->position[AXIS_X],
            simulation->position[AXIS_Z], &triangle) == 0) {
      contact = STEP_FLAG_GROUND_HIT;
    }

    if (simulation->state == BALL_STATE_ROLLING && !isNearCup &&
        contact == STEP_FLAG_GROUND_HIT) {
      TrianglePlane plane;
      SurfaceInfo* info;
      s32 groundHeight;
      s32 slopeX;
      s32 slopeZ;
      f32 friction;
      f32 speed;

      speed = sqrtf((velocity[AXIS_X] * velocity[AXIS_X]) +
          (velocity[AXIS_Z] * velocity[AXIS_Z]));
      isSteep = FALSE;
      rollSpeed = speed;
      if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
        info = &f_SurfaceInfo[SURFACE_GREEN];
        triangle.corners[AXIS_X][0] = 0;
        triangle.corners[AXIS_X][1] = 0x100;
        triangle.corners[AXIS_X][2] = 0;
        triangle.corners[AXIS_Y][0] = 0;
        triangle.corners[AXIS_Y][1] = 0;
        triangle.corners[AXIS_Y][2] = 0;
        triangle.corners[AXIS_Z][0] = 0;
        triangle.corners[AXIS_Z][1] = 0;
        triangle.corners[AXIS_Z][2] = 0x100;
        if (g_ClubTestMode == CLUB_TEST_SHOT) {
          info = &f_SurfaceInfo[SURFACE_FAIRWAY];
        }

        groundHeight = 0;
        GetTrianglePlane(&triangle, &plane);
      } else {
        GetTrianglePlane(&triangle, &plane);
        if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST &&
            !simulation->unkA1) {
          f32 pressing = (plane.normal[AXIS_X] * velocity[AXIS_X]) +
              (plane.normal[AXIS_Y] * velocity[AXIS_Y]) +
              (plane.normal[AXIS_Z] * velocity[AXIS_Z]);

          if ((g_GameState.isMiniGolf && pressing >= 655.0f) ||
              pressing >= 7111.68f) {
            f32 along[3];
            f32 damping;

            osSyncPrintf("Jump\n");
            damping = 1.0f;
            if (!g_GameState.isMiniGolf) {
              damping = 0.1f;
            }

            along[AXIS_X] =
                velocity[AXIS_X] - (plane.normal[AXIS_X] * pressing);
            along[AXIS_Y] =
                velocity[AXIS_Y] - (plane.normal[AXIS_Y] * pressing);
            along[AXIS_Z] =
                velocity[AXIS_Z] - (plane.normal[AXIS_Z] * pressing);
            simulation->position[AXIS_X] =
                startX + (fraction * velocity[AXIS_X]);
            simulation->position[AXIS_Y] =
                startY + (fraction * velocity[AXIS_Y]);
            simulation->position[AXIS_Z] =
                startZ + (fraction * velocity[AXIS_Z]);
            velocity[AXIS_X] = along[AXIS_X] * damping;
            velocity[AXIS_Y] = along[AXIS_Y] * damping;
            velocity[AXIS_Z] = along[AXIS_Z] * damping;
          }
        }

        if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST &&
            !g_GameState.isMiniGolf &&
            fabsf(velocity[AXIS_X]) + fabsf(velocity[AXIS_Z]) >= 711.168f) {
          f32 tilt = (GetStepNoise() - 0.5f) * 0.5f;

          plane.normal[AXIS_X] -= velocity[AXIS_Z] * 0.00035153437f * tilt;
          plane.normal[AXIS_Z] += velocity[AXIS_X] * 0.00035153437f * tilt;
          guNormalize(&plane.normal[AXIS_X], &plane.normal[AXIS_Y],
              &plane.normal[AXIS_Z]);
        }

        simulation->surface = GetSurface(
            simulation->position[AXIS_X], simulation->position[AXIS_Z]);
        switch (simulation->surface) {
          case SURFACE_FAIRWAY:
            if (!g_GameState.isMiniGolf) {
              f32 lie = GetStepNoise();

              if (lie < 0.25f) {
                simulation->surface = SURFACE_FAIRWAY_POOR_LIE;
              } else if (lie < 0.5f) {
                simulation->surface = SURFACE_FAIRWAY_FAIR_LIE;
              }
            }

            break;
          case SURFACE_ROUGH:
            if (GetStepNoise() < 0.35f) {
              simulation->surface = SURFACE_ROUGH_POOR_LIE;
            }

            break;
        }

        info = GetSurfaceInfo(simulation->surface);
        if (info->kind == SURFACE_KIND_OUT_OF_COURSE) {
          simulation->state = BALL_STATE_STOPPED;
          goto ROLLED;
        }

        groundHeight = GetGroundHeight(
            simulation->position[AXIS_X], simulation->position[AXIS_Z]);
        if (!g_GameState.isMiniGolf &&
            groundHeight < simulation->position[AXIS_Y] - 0x1200) {
          simulation->position[AXIS_X] = startX;
          simulation->position[AXIS_Y] = startY;
          simulation->position[AXIS_Z] = startZ;
          velocity[AXIS_X] *= 0.1f;
          velocity[AXIS_Y] *= 0.1f;
          velocity[AXIS_Z] *= 0.1f;
          simulation->state = BALL_STATE_FLYING;
          goto TAKE_OFF;
        }

        if (simulation->position[AXIS_Y] + 307.2f < groundHeight) {
          simulation->state = BALL_STATE_FLYING;

TAKE_OFF:

          simulation->unk3C = atan2(velocity[AXIS_Z], velocity[AXIS_X]);
          contact = 0;
          simulation->unk34 = IntLength2D(velocity[AXIS_X], velocity[AXIS_Z]);
          BuildUnk44Rotation();
          goto ROLLED;
        }
      }

      slopeX = (-plane.normal[AXIS_X] * 196.5f) /
          AbsoluteValue(plane.normal[AXIS_Y]);
      slopeZ = (-plane.normal[AXIS_Z] * 196.5f) /
          AbsoluteValue(plane.normal[AXIS_Y]);
      if (fabsf((f32)slopeX) + fabsf((f32)slopeZ) >= 131.0f) {
        f32 scale =
            131.0f / sqrtf(((f32)slopeX * slopeX) + ((f32)slopeZ * slopeZ));

        slopeX = slopeX * scale;
        slopeZ = slopeZ * scale;
      }

      if (plane.normal[AXIS_Y] * plane.normal[AXIS_Y] <=
              (plane.normal[AXIS_X] * plane.normal[AXIS_X]) +
                  (plane.normal[AXIS_Z] * plane.normal[AXIS_Z]) &&
          simulation->unk98 < 800) {
        isSteep = TRUE;
      }

      friction = info->unkC * 0.001f;
      if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST &&
          g_HoleSetup.conditions.isRaining == TRUE) {
        friction *= 0.993f;
      }

      if (simulation->unkA1 && (u16)simulation->unkB2 < 30) {
        slopeX *= 3;
        slopeZ *= 3;
        isSteep = TRUE;
      } else if (isSteep) {
        slopeX *= 2;
        slopeZ *= 2;
        if (friction < 0.9f) {
          friction = 0.9f;
        }
      } else {
        if ((f32)rollSpeed < 307.2f) {
          f32 scale = (rollSpeed * 0.0028483071f) + 0.125f;

          slopeX = slopeX * scale;
          slopeZ = slopeZ * scale;
        }

        if (simulation->unk98 >= 600) {
          friction *= 0.5f;
        } else if (simulation->unk98 >= 400) {
          friction *= 0.9f;
        } else if (simulation->unk98 >= 150) {
          friction *= 0.99f;
        }
      }

      speed = sqrtf((velocity[AXIS_X] * velocity[AXIS_X]) +
          (velocity[AXIS_Z] * velocity[AXIS_Z]));
      if (speed > 15.36f) {
        f32 against = -((((f32)slopeX * velocity[AXIS_X]) +
                            ((f32)slopeZ * velocity[AXIS_Z])) /
            speed);

        slopeX = slopeX + (against * ((slopeX / speed) * 0.2f));
        slopeZ = slopeZ + (against * ((slopeZ / speed) * 0.2f));
      } else {
        slopeX = slopeX * 0.5f;
        slopeZ = slopeZ * 0.5f;
      }

      velocity[AXIS_X] =
          (slopeX * friction * 0.4f) + (velocity[AXIS_X] * friction);
      velocity[AXIS_Z] =
          (slopeZ * friction * 0.4f) + (velocity[AXIS_Z] * friction);
      if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST &&
          g_GameState.isMiniGolf) {
        if ((f32)(groundHeight - startY) < velocity[AXIS_Y]) {
          velocity[AXIS_Y] = groundHeight - startY + 0x20C;
        } else {
          velocity[AXIS_Y] += 524.0f;
        }
      } else {
        if ((f32)(groundHeight - startY) < velocity[AXIS_Y]) {
          velocity[AXIS_Y] = groundHeight - startY + 0x106;
        } else {
          velocity[AXIS_Y] += 262.0f;
        }
      }

      if (simulation->purpose == LAUNCH_PURPOSE_STROKE &&
          IsGameFlagSet(GAME_FLAG_LOG_VERTICAL_VELOCITY)) {
        osSyncPrintf("V[1] %d\n", velocity[AXIS_Y]);
      }

      if ((u16)simulation->unkB2 >= 16) {
        s32 speed = IntLength2D(velocity[AXIS_X], velocity[AXIS_Z]);

        if ((u16)simulation->unkB2 >= 61 && simulation->unkA1) {
          simulation->state = BALL_STATE_STOPPED;
        } else if (speed < 30) {
          if (!isSteep && IntLength2D(slopeX, slopeZ) * friction < 500.0f) {
            simulation->state = BALL_STATE_STOPPED;
          }
        }
      }

      if (!isSteep && info->kind != SURFACE_KIND_GREEN &&
          simulation->unk9C + 90 < simulation->unk98 &&
          sqrtf((velocity[AXIS_X] * velocity[AXIS_X]) +
              (velocity[AXIS_Y] * velocity[AXIS_Y]) +
              (velocity[AXIS_Z] * velocity[AXIS_Z])) < (f64)284.4672f) {
        simulation->state = BALL_STATE_STOPPED;
      }
    }

ROLLED:

    if ((simulation->state == BALL_STATE_FLYING ||
            simulation->state == BALL_STATE_ROLLING) &&
        contact == 0) {
      {
        f32 relative[3];
        f32 airSpeed;
        f32 drag;
        f32 unk38;
        f32 horizontalSpeed;
        f32 unk38Speed;
        f32 ratio;
        f32 weight;
        f32 curve;
        f32 lift;
        f32 height;
        f32 windX;
        f32 windZ;

        relative[AXIS_X] = velocity[AXIS_X] - simulation->windVelocityX;
        relative[AXIS_Y] = velocity[AXIS_Y];
        relative[AXIS_Z] = velocity[AXIS_Z] - simulation->windVelocityZ;
        airSpeed = sqrtf((relative[AXIS_X] * relative[AXIS_X]) +
            (relative[AXIS_Y] * relative[AXIS_Y]) +
            (relative[AXIS_Z] * relative[AXIS_Z]));
        drag = (sqrtf(airSpeed * 0.0140613755f) * 0.0011f) + 0.004f;
        unk38 = simulation->unk38;
        horizontalSpeed = sqrtf((relative[AXIS_X] * relative[AXIS_X]) +
            (relative[AXIS_Z] * relative[AXIS_Z]));
        unk38Speed = fabsf(unk38 * 8.0f);
        if (unk38Speed != 0.0f) {
          ratio = horizontalSpeed / unk38Speed;
        } else {
          ratio = 0.0f;
        }

        weight = 1.0f - (fabsf(ratio - 1.0f) * 0.8f);
        if (weight < 0.0f) {
          weight = 0.0f;
        }

        curve = unk38 * (weight * 0.038146973f);
        if (horizontalSpeed < fabsf(curve) * 100.0f) {
          curve = horizontalSpeed * 0.01f;
          if (simulation->unk38 < 0) {
            curve = -curve;
          }
        }

        if (horizontalSpeed > 1422.336f) {
          f32 scale = (1.0f / horizontalSpeed) * curve;

          velocity[AXIS_X] += -relative[AXIS_Z] * scale;
          velocity[AXIS_Z] += relative[AXIS_X] * scale;
        }

        simulation->unk38 = simulation->unk38 * 0.995f;
        lift = simulation->unk34 * 0.045776367f;
        if (simulation->unk34 != 0 && airSpeed > 142.2336f) {
          f32 turned[3];
          f32 scale;

          guMtxXFMF(simulation->unk44, relative[AXIS_X], relative[AXIS_Y],
              relative[AXIS_Z], &turned[AXIS_X], &turned[AXIS_Y],
              &turned[AXIS_Z]);
          if (airSpeed < fabsf(lift) * 80.0f) {
            lift = airSpeed * 0.0125f;
          }

          scale = lift /
              sqrtf((turned[AXIS_X] * turned[AXIS_X]) +
                  (turned[AXIS_Y] * turned[AXIS_Y]) +
                  (turned[AXIS_Z] * turned[AXIS_Z]));
          turned[AXIS_X] *= scale;
          turned[AXIS_Y] *= scale;
          turned[AXIS_Z] *= scale;
          velocity[AXIS_X] += turned[AXIS_X];
          velocity[AXIS_Y] += turned[AXIS_Y];
          velocity[AXIS_Z] += turned[AXIS_Z];
          simulation->unk34 = simulation->unk34 * 0.993f;
        }

        height =
            simulation->startPosition[AXIS_Y] - simulation->position[AXIS_Y];
        if (height <= 0.0f) {
          windX = simulation->windVelocityX * 0.5f;
          windZ = simulation->windVelocityZ * 0.5f;
        } else {
          f32 scale = (height * 0.00000088998473f) + 0.5f;

          windX = simulation->windVelocityX * scale;
          windZ = simulation->windVelocityZ * scale;
        }

        velocity[AXIS_X] += (windX - velocity[AXIS_X]) * drag;
        velocity[AXIS_Y] += (-velocity[AXIS_Y] * drag) + 262.0f;
        velocity[AXIS_Z] += (windZ - velocity[AXIS_Z]) * drag;
      }
    }

    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
      Boolean isOutside = simulation->position[AXIS_Z] > 0x7ED400;

      if (simulation->position[AXIS_Z] <= 0x12BFF) {
        isOutside = TRUE;
      }

      if (simulation->position[AXIS_X] > 0x3ED400) {
        isOutside = TRUE;
      }

      if (simulation->position[AXIS_X] <= 0x12BFF) {
        isOutside = TRUE;
      }

      if (isOutside) {
        flags |= STEP_FLAG_OUTSIDE_COURSE;
      }
    }

SETTLE:

    if (simulation->purpose != LAUNCH_PURPOSE_UNK5) {
      s32 floor;

      if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
        floor = 0;
      } else if (simulation->state == BALL_STATE_IN_WATER) {
        floor = GetGroundHeight(
            simulation->position[AXIS_X], simulation->position[AXIS_Z]);
      } else {
        s32 state = simulation->state;

        floor = GetGroundHeight(
            simulation->position[AXIS_X], simulation->position[AXIS_Z]);
        if (state == BALL_STATE_IN_CUP || simulation->unkA0) {
          floor += 0x1200;
        } else if (state == BALL_STATE_ROLLING) {
          s32 cupOffsetX =
              simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X];

          if (ABS(cupOffsetX) < 0xC00) {
            s32 cupOffsetZ =
                simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z];

            if (ABS(cupOffsetZ) < 0xC00) {
              f32 cupDistance = (s32)IntLength2D(cupOffsetX, cupOffsetZ);

              if (cupDistance < 614.4f) {
                floor += 0xC00;
                simulation->unkB4 = 1;
              } else if (cupDistance < 1843.2001f) {
                f32 rimDistance = 1843.2001f - cupDistance;
                f32 rest = 377487.38f - (rimDistance * rimDistance);

                if (rest > 0.0f) {
                  floor = floor + ((614.4f - sqrtf(rest)) * 0.5f);
                  simulation->unkB4 = 1;
                }
              }
            }
          }
        }
      }

      if (floor < simulation->position[AXIS_Y]) {
        simulation->position[AXIS_Y] = floor;
      }

      if (simulation->position[AXIS_Y] < -0x465000) {
        simulation->position[AXIS_Y] = -0x465000;
      }

      if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        s32 kind;
        PlaneTriangle ground;
        TrianglePlane plane;

        simulation->unkA4 = FindSurface(
            simulation->position[AXIS_X], simulation->position[AXIS_Z], TRUE);
        kind = GetSurfaceInfo(simulation->unkA4)->kind;
        if (kind != SURFACE_KIND_WATER_HAZARD &&
            kind != SURFACE_KIND_OUT_OF_BOUNDS &&
            kind != SURFACE_KIND_BARE_GROUND_UNK20 &&
            kind != SURFACE_KIND_OUT_OF_COURSE &&
            FindGroundTriangle(simulation->position[AXIS_X],
                simulation->position[AXIS_Z], &ground) == 0 &&
            GetTrianglePlane(&ground, &plane) == 0 &&
            (plane.normal[AXIS_X] * plane.normal[AXIS_X]) +
                    (plane.normal[AXIS_Z] * plane.normal[AXIS_Z]) <
                plane.normal[AXIS_Y] * plane.normal[AXIS_Y] * 0.25f) {
          simulation->unkC[AXIS_X] = simulation->position[AXIS_X];
          simulation->unkC[AXIS_Y] = simulation->position[AXIS_Y];
          simulation->unkC[AXIS_Z] = simulation->position[AXIS_Z];
        }
      }
    }

NEXT:

    simulation->unkA6 = flags;
  } while (++substep < substepCount);
  simulation->velocity[AXIS_X] = velocity[AXIS_X];
  simulation->velocity[AXIS_Y] = velocity[AXIS_Y];
  simulation->velocity[AXIS_Z] = velocity[AXIS_Z];
  if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
    if (simulation->state == BALL_STATE_ROLLING && !simulation->unkA8[1]) {
      PlaySound(0x14, ROLL_SOUND_CHANNEL, SOUND_FULL_SCALE);
      simulation->unkA8[1] = TRUE;
    } else if (simulation->unkA8[1]) {
      if (simulation->state != BALL_STATE_ROLLING) {
        StopSound(ROLL_SOUND_CHANNEL);
        simulation->unkA8[1] = FALSE;
      } else {
        s32 rollFrames = (u16)simulation->unkB2;
        s32 volume;

        if (rollFrames > 5) {
          rollFrames = 5;
        }

        volume = sqrtf((velocity[AXIS_X] * velocity[AXIS_X]) +
                     (velocity[AXIS_Z] * velocity[AXIS_Z])) *
            0.008928973f * (rollFrames + 1);
        if (volume > 0x37) {
          volume = 0x37;
        }

        SetSoundVolume(ROLL_SOUND_CHANNEL, volume & ~(volume >> 31));
      }
    }
  }

  return flags;
}
#else
INCLUDE_ASM("asm/nonmatchings/ball_simulation", PlayStrokeImpactSound);

INCLUDE_ASM("asm/nonmatchings/ball_simulation", BuildUnk44Rotation);

INCLUDE_ASM("asm/nonmatchings/ball_simulation", PlayImpactSound);

INCLUDE_ASM("asm/nonmatchings/ball_simulation", PlayBillboardHitSound);

INCLUDE_ASM("asm/nonmatchings/ball_simulation", GetPositionNoise);

INCLUDE_ASM("asm/nonmatchings/ball_simulation", StepBallSimulation);
#endif

/* Returns the Unk6 entry for the club's lie rating and unkC */
Unk6* GetUnkCEntry(Club* club, s32 surface, s32 shotMode, s32 unkC) {
  s32 row;

  row = GetLieRating(club, surface, shotMode) - 1;
  if (row < 0) {
    row = 0;
  }

  if (row > MAX_LIE_RATING) {
    row = MAX_LIE_RATING;
  }

  if (unkC < -UNKC_ENTRY_LIMIT) {
    return &D_800B7C00;
  }

  if (unkC <= UNKC_ENTRY_LIMIT) {
    return &D_800B7C10[row][unkC + UNKC_ENTRY_LIMIT];
  }

  return &D_800B7C08;
}

s16 GetCrossSlopeAngle(UnkB8* player, Unk20* shot);
/* It returns s16; declaring it s32 here is there to match */
s32 GetAimSlopeAngle(UnkB8* player, Unk20* shot);

/*
 * Sets the simulation's start, velocity and wind from the shot.
 * Only a stroke spawns debris, starts rumble and prints the launch values.
 */
s32 LaunchBallSimulation(BallSimulation* simulation, s32 purpose, Unk20* shot) {
  s32 percentRange[2];
  Club* club;
  Unk6* entry;
  s32 unkC;
  s32 groundHeight;
  s32 speed;
  s32 sideSign;
  s32 slopeSpin;
  s32 rumbles;
  s16 crossSlope;
  s32 crossSlopeSize;
  s32 yaw;
  f32 pitch;
  f32 yawRadians;
  f32 launchScale;
  f32 blend;
  f32 slopePitch;
  f32 curve;
  f32 spinBase;
  f32 windAngle;
  f32 windScale;
  f32 along; /* Reusing it for the pitch below is there to match */
  s32 index;
  s8 surface;

  /* Reading the player each time is there to match */
  f32 PickLieBlend(void) {
    return (u32)((simulation->position[AXIS_X] + simulation->position[AXIS_Z] +
                     g_Players[simulation->playerIndex].unk88) &
               0xFF) /
        510.0f +
        (u32)((simulation->position[AXIS_X] - simulation->position[AXIS_Z] +
                  g_Players[simulation->playerIndex].unk88) &
            0xFF) /
        510.0f;
  }

  launchScale = 1.0f;
  unkC = shot->unkC;
  simulation->purpose = purpose;
  sideSign = 1;
  D_800B7D9C++;
  slopeSpin = 0;
  if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    groundHeight = GetGroundHeight(simulation->position[AXIS_X],
                       simulation->position[AXIS_Z]) -
        1.536f;
    if (groundHeight < simulation->position[AXIS_Y]) {
      simulation->position[AXIS_Y] = groundHeight;
    }
  }

  surface = simulation->surface;
  simulation->unkC[AXIS_X] = simulation->position[AXIS_X];
  simulation->startPosition[AXIS_X] = simulation->unkC[AXIS_X];
  simulation->unkC[AXIS_Y] = simulation->position[AXIS_Y];
  simulation->startPosition[AXIS_Y] = simulation->unkC[AXIS_Y];
  simulation->unkC[AXIS_Z] = simulation->position[AXIS_Z];
  simulation->startPosition[AXIS_Z] = simulation->unkC[AXIS_Z];
  simulation->unkA2 = surface;
  simulation->unkA7 = surface;
  simulation->unkA1 = 0;
  simulation->unkA6 = 0;
  simulation->unkB4 = 0;

  /* The tests of purpose, and their senses, are there to match */
  if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    simulation->playerIndex = shot->unk18;
  } else {
    simulation->playerIndex = 0;
  }

  if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    s16* clubs = g_Players[shot->unk18].clubs;

    club = GetClub(clubs[shot->clubIndex]);
  } else {
    club = GetClub(shot->clubIndex);
  }

  if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
    entry = &D_800B7C10[0][UNKC_ENTRY_LIMIT];
    rumbles = D_800B7C10[0][UNKC_ENTRY_LIMIT].unk5;
  } else {
    /* The same as GetUnkCEntry; writing it out is there to match */
    s32 row = GetLieRating(club, simulation->surface, shot->shotMode) - 1;

    if (row < 0) {
      row = 0;
    }

    if (row > MAX_LIE_RATING) {
      row = MAX_LIE_RATING;
    }

    if (unkC < -UNKC_ENTRY_LIMIT) {
      entry = &D_800B7C00;
    } else if (unkC > UNKC_ENTRY_LIMIT) {
      entry = &D_800B7C08;
    } else {
      entry = &D_800B7C10[row][unkC + UNKC_ENTRY_LIMIT];
    }

    rumbles = entry->unk5;
  }

  if ((club->surfaceMask & CLUB_SURFACE_GREEN) &&
      simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    speed = GetPuttPower(shot->power, shot->shotMode);
  } else {
    speed = GetClubPower(club, shot->shotMode);
    speed = speed * (shot->power * ((entry->unk0 * 2) + 0x100) / 256) / 256;
    if (speed <= 0) {
      speed = 10;
    }
  }

  if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    SurfaceInfo* info = GetSurfaceInfo(simulation->unkA2);

    blend = PickLieBlend();
    GetSurfacePercentRange(info, club, percentRange);
    speed = ((percentRange[0] * blend) + (percentRange[1] * (1.0f - blend))) *
        speed * 0.01f;
  } else {
    s32 percent = 198; /* The local is there to match */

    speed = speed * percent / 200;
  }

  pitch = (club->pitch + entry->unk2) * 0.017453294f;
  if (pitch >= 0.0f) {
    pitch = 0.0f;
  }

  if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
    sinf(pitch); /* The original has it */
    along = pitch - (club->unk10 * 0.017453294f);
    pitch += (f32)club->unk10 * shot->unk14 * 0.00054541545f;
    sinf(pitch); /* The original has it */
    launchScale = (cosf(along) - 0.86f) * shot->unk14 * 0.008300781f;
    if (club->kind == CLUB_KIND_WOOD) {
      launchScale *= 2.0f;
    }

    launchScale += 1.0f;
  }

  if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    if ((club->surfaceMask & CLUB_SURFACE_GREEN) && !g_GameState.isMiniGolf) {
      pitch += (u16)GetAimSlopeAngle(&g_Players[shot->unk18], shot) *
          BINARY_ANGLE_TO_RADIANS;
    } else {
      slopePitch = (u16)(GetAimSlopeAngle(&g_Players[shot->unk18], shot) / 4) *
          BINARY_ANGLE_TO_RADIANS;
      pitch += slopePitch;
      speed = speed * (1.0f - sinf(slopePitch));
    }
  }

  if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST ||
      (club->surfaceMask & CLUB_SURFACE_GREEN)) {
    yawRadians = (u16)shot->unk0 * BINARY_ANGLE_TO_RADIANS;
  } else {
    crossSlope = GetCrossSlopeAngle(&g_Players[shot->unk18], shot);
    crossSlopeSize = crossSlope;
    if (crossSlope < 0) {
      crossSlopeSize = -crossSlope;
    }

    curve = sqrtf(crossSlopeSize * 0.25f);
    sideSign = g_Players[shot->unk18].unk22 ? -1 : 1;
    if (crossSlope < 0) {
      curve = -curve;
    }

    slopeSpin = curve;
    yaw = shot->unk0 + (crossSlope / 2) +
        (sideSign *
            ((club->deflection[shot->shotMode] * shot->power /
                 MAX_LAUNCH_POWER) +
                (entry->unk1 * club->unk0)));
    yawRadians = (u16)yaw * BINARY_ANGLE_TO_RADIANS;
  }

  if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
    simulation->unk38 = (sideSign * (club->sideSpin * 2) * speed) * 0.00078125f;
    simulation->unk40 = 0.0f;
    spinBase = club->spin * 0.5f;
    simulation->unk34 =
        (((spinBase + 25.0f) * 0.006f * shot->unk14) - spinBase) *
        (speed / launchScale) * 0.0078125f;
    simulation->unk3C = yawRadians;
    if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
      speed = speed * launchScale;
    }

    if (speed < 0) {
      speed = 0;
    }
  } else {
    GetSurfaceInfo(simulation->unkA2); /* The original has it */
    simulation->unk38 = (((sideSign * entry->unk3 * club->unk0) / 64) +
                            (shot->unk10 * (club->unk0 * 0.0078125f)) +
                            slopeSpin + (sideSign * 2 * club->sideSpin)) *
        (speed * launchScale * launchScale) * 0.00078125f;
    if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
      simulation->unk34 = ((((club->spin * 0.8f) + 10.0f) * 0.004f *
                               (entry->unk4 + shot->unk14)) -
                              (club->spin * 0.5f)) *
          speed * 0.0078125f;
      speed = speed * launchScale;
      if (g_HoleSetup.conditions.isRaining == TRUE) {
        speed = speed * 0.95f;
      }
    } else {
      simulation->unk34 = speed;
    }

    if (speed < 0) {
      speed = 0;
    }

    if (g_HoleSetup.conditions.isRaining == TRUE) {
      simulation->unk34 = simulation->unk34 * 0.6f;
      simulation->unk38 = simulation->unk38 * 0.95f;
    }

    simulation->unk40 =
        IntLength2D(simulation->unk34, simulation->unk38) * 0.4f;
    simulation->unk3C = yawRadians;
  }

  if (speed <= 0) {
    speed = 10;
  }

  simulation->velocity[AXIS_Y] = sinf(pitch) * speed;
  simulation->velocity[AXIS_X] = cosf(yawRadians) * speed;
  simulation->velocity[AXIS_Z] = sinf(yawRadians) * speed;
  if (club->surfaceMask & CLUB_SURFACE_GREEN) {
    simulation->state = BALL_STATE_ROLLING;
    simulation->velocity[AXIS_Y] += PUTT_VELOCITY_Y_OFFSET;
  } else {
    simulation->state = BALL_STATE_FLYING;
  }

  simulation->unk94 = 0;
  simulation->unk98 = 0;
  simulation->unk9C = -2;
  simulation->unkA0 = 0;
  simulation->unkB0 = 0;
  simulation->unkB2 = 0;
  for (index = 0; index < UNKA8_COUNT; index++) {
    simulation->unkA8[index] = 0;
  }

  windScale = 0.0078125f; /* The local is there to match */
  if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    windAngle = g_HoleSetup.conditions.windDirection * BINARY_ANGLE_TO_RADIANS;
    simulation->windVelocityX =
        cosf(windAngle) * g_HoleSetup.conditions.windSpeed * windScale;
    simulation->windVelocityZ =
        sinf(windAngle) * g_HoleSetup.conditions.windSpeed * windScale;
    if (g_HoleSetup.conditions.isRaining != TRUE) {
      along = ((simulation->windVelocityX * cosf(yawRadians)) +
                  (simulation->windVelocityZ * sinf(yawRadians))) *
          0.3f;
      simulation->windVelocityX += -cosf(yawRadians) * along;
      simulation->windVelocityZ += -sinf(yawRadians) * along;
    }
  } else {
    simulation->windVelocityX = 0.0f;
    simulation->windVelocityZ = 0.0f;
  }

  if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
    SpawnShotDebris(simulation->position, pitch, yawRadians, speed);
    if (rumbles) {
      StartPlayerRumble(&g_Players[simulation->playerIndex]);
    }

    /* The original tests the purpose again */
    if (simulation->purpose == LAUNCH_PURPOSE_STROKE &&
        IsGameFlagSet(GAME_FLAG_SHOW_LAUNCH_VALUES)) {
      char text[128]; /* Declaring it here is there to match */

      sprintf(text, "ANGLEY = %3.3d/360", (s32)(yawRadians * 57.295776f));
      PrintDebugText(text, 3, 6);
      sprintf(text, "ANGLEX = %3.3d/360", (s32)(pitch * 57.295776f));
      PrintDebugText(text, 3, 7);
      sprintf(text, "Velocity = %fm/s     ", speed * 0.001953125f);
      PrintDebugText(text, 3, 8);
      sprintf(text, "SpinY = %d", simulation->unk38);
      PrintDebugText(text, 3, 9);
      sprintf(text, "SpinX = %d", simulation->unk34);
      PrintDebugText(text, 3, 10);
      sprintf(text, "SpinVel = %fRPM      ", simulation->unk34 * 0.8741959f);
      PrintDebugText(text, 3, 11);
      sprintf(
          text, "SpinAngle = %3.3d/360", (s32)(simulation->unk3C * 57.295776f));
      PrintDebugText(text, 3, 12);
      sprintf(
          text, "SpinVelX = %fkm/h      ", simulation->unk34 * 0.0070306873f);
      PrintDebugText(text, 3, 13);
      sprintf(
          text, "SpinVelY = %fkm/h      ", simulation->unk38 * 0.0070306873f);
      PrintDebugText(text, 3, 14);
    }
  }

  return 0;
}
