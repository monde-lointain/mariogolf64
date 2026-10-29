/*
 * The opening's copies of the ball simulation's routines, with which it
 * simulates its shot, and a routine that loads one of the other characters with
 * its birdie animation.
 */
#include "common.h"

#include "ball_simulation.h"
#include "billboard.h"
#include "character_slot.h"
#include "club.h"
#include "collision.h"
#include "debug_text.h"
#include "effects.h"
#include "functions.h"
#include "game_flags.h"
#include "libc.h"
#include "math_util.h"
#include "reaction.h"
#include "save.h"
#include "shot_effects.h"
#include "surface.h"
#include "terrain.h"
#include "variables.h"

/* LoadCharacter kinds from the kind count on load one of g_OtherCharacterModels
 */
#define OTHER_CHARACTER_MODEL_KIND CHARACTER_MODEL_KIND_COUNT

/* The clubs that the opening's golfers swing */
#define OPENING_CLUB_COUNT 2

/*
 * The absolute value as a statement expression, as StepBallSimulation has it,
 * which compiles to a compare and branch
 */
#define AbsoluteValueExpression(value) \
  ({ \
    f32 absValue = (value); \
    (absValue < 0.0f) ? -absValue : absValue; \
  })

/* The opening's copy of g_ClubTestMode */
static s8 D_8020B560_ovl_05 = CLUB_TEST_NONE;

/* The opening's own copy of f_SurfaceInfo, which names the surfaces in
 * half-width katakana and differs from it in eight entries' values */
static SurfaceInfo D_8020B564_ovl_05[SURFACE_COUNT] = {
    /* SURFACE_OUT_OF_BOUNDS */
    {SURFACE_KIND_OUT_OF_BOUNDS, "O.B.", 100, 100, 0, 0, 0, 0x500, 1, 7},
    /* SURFACE_FAIRWAY */
    {SURFACE_KIND_FAIRWAY, "ﾌｪｱｳｪｲ", 98, 100, 5, 960, 400, 0x501, 0, 0},
    /* SURFACE_ROUGH */
    {SURFACE_KIND_ROUGH, "ﾗﾌ", 80, 90, 30, 600, 200, 0x508, 1, 1},
    /* SURFACE_BUNKER */
    {SURFACE_KIND_BUNKER, "ﾊﾞﾝｶｰ", 70, 85, 30, 600, 0, 0x504, 0, 2},
    /* SURFACE_GREEN */
    {SURFACE_KIND_GREEN, "ｵﾝ ｸﾞﾘｰﾝ", 100, 100, 0, 972, 500, 0x50A, 0, 6},
    /* SURFACE_WATER_HAZARD */
    {SURFACE_KIND_WATER_HAZARD, "ｳｫｰﾀｰﾊｻﾞｰﾄﾞ", 100, 100, 0, 0, 0, 0x50C, 2, 3},
    /* SURFACE_CART_WAY */
    {SURFACE_KIND_CART_WAY, "ｶｰﾄｳｪｲ", 80, 100, 10, 975, 890, 0x50B, 0, 4},
    /* SURFACE_TEE_GROUND */
    {SURFACE_KIND_TEE_GROUND, "ﾃｨｰｸﾞﾗｳﾝﾄﾞ", 98, 100, 10, 930, 500, 0x501, 0, 8},
    /* SURFACE_ROCK */
    {SURFACE_KIND_ROCK, "ﾛｯｸ", 80, 90, 30, 930, 900, 0x50D, 0, 9},
    /* SURFACE_GREEN_EDGE */
    {SURFACE_KIND_GREEN_EDGE, "ｸﾞﾘｰﾝ ｴｯｼﾞ", 98, 100, 10, 966, 400, 0x501, 0, 0},
    /* SURFACE_SEMI_ROUGH */
    {SURFACE_KIND_SEMI_ROUGH, "ｾﾐﾗﾌ", 90, 98, 30, 930, 500, 0x507, 0, 0},
    /* SURFACE_BARE_GROUND */
    {SURFACE_KIND_BARE_GROUND, "ﾍﾞｱｸﾞﾗｳﾝﾄﾞ", 80, 90, 30, 930, 500, 0x50C, 0, 1},
    /* SURFACE_DEEP_ROUGH */
    {SURFACE_KIND_DEEP_ROUGH, "ﾃﾞｨｰﾌﾟﾗﾌ", 50, 80, 30, 430, 100, 0x509, 2, 5},
    /* SURFACE_BARE_GROUND_UNK13 */
    {SURFACE_KIND_BARE_GROUND_UNK20, "ﾍﾞｱｸﾞﾗｳﾝﾄﾞ", 80, 90, 30, 930, 500, 0x50C,
        0, 1},
    /* SURFACE_WASTE_AREA */
    {SURFACE_KIND_WASTE_AREA, "ｳｴｽﾄｴﾘｱ", 50, 80, 30, 430, 100, 0x8F1, 1, 5},
    /* SURFACE_UNK15 */
    {SURFACE_KIND_ROCK, "            ", 98, 100, 40, 930, 500, 0x500, 0, 9},
    /* SURFACE_BUNKER_FAIR_LIE */
    {SURFACE_KIND_BUNKER_FAIR_LIE, "ﾊﾞﾝｶｰ", 50, 70, 30, 600, 0, 0x505, 1, 2},
    /* SURFACE_BUNKER_POOR_LIE */
    {SURFACE_KIND_BUNKER_POOR_LIE, "ﾊﾞﾝｶｰ", 40, 60, 30, 600, 0, 0x506, 1, 2},
    /* SURFACE_BUNKER_UNK18 */
    {SURFACE_KIND_BUNKER_UNK9, "ﾊﾞﾝｶｰ", 40, 60, 40, 600, 0, 0x506, 1, 2},
    /* SURFACE_FAIRWAY_FAIR_LIE */
    {SURFACE_KIND_FAIRWAY_FAIR_LIE, "ﾌｪｱｳｪｲ", 94, 98, 7, 950, 380, 0x502, 0, 0},
    /* SURFACE_FAIRWAY_POOR_LIE */
    {SURFACE_KIND_FAIRWAY_POOR_LIE, "ﾌｪｱｳｪｲ", 90, 94, 7, 950, 380, 0x503, 0, 0},
    /* SURFACE_ROUGH_POOR_LIE */
    {SURFACE_KIND_ROUGH_POOR_LIE, "ﾗﾌ", 70, 85, 30, 600, 200, 0x509, 1, 5},
    /* SURFACE_OUT_OF_COURSE */
    {SURFACE_KIND_OUT_OF_COURSE, "ｱｳﾄ ｵﾌﾞ ｺｰｽ", 100, 100, 0, 0, 0, 0x500, 2, 7},
    /* SURFACE_TEE_UP */
    {SURFACE_KIND_TEE_UP, "TEE UP", 98, 100, 10, 930, 500, 0x500, 0, 8},
    /* SURFACE_MINI_GOLF_GREEN */
    {SURFACE_KIND_GREEN, "ｸﾞﾘｰﾝ", 100, 100, 0, 972, 500, 0x8F2, 0, 6},
    /* SURFACE_MINI_GOLF_FAIRWAY */
    {SURFACE_KIND_FAIRWAY, "ﾌﾞﾙｰ", 98, 100, 5, 960, 400, 0x8F3, 0, 0},
    /* SURFACE_MINI_GOLF_CART_WAY */
    {SURFACE_KIND_CART_WAY, "ﾚｯﾄﾞ", 80, 100, 10, 980, 890, 0x8F4, 0, 4}};

/* The debug text row, of SPEED_TEXT_ROW_COUNT, that the next ball speed goes in
 */
static s32 D_8020B858_ovl_05 = 0;
/* The opening's copies of D_800B7C00 and D_800B7C08, which its copy of
 * GetImpactTimingEntry doesn't read */
static ImpactEffect D_8020B85C_ovl_05 = {-90, -64, -40, -10, 100, 1};
static ImpactEffect D_8020B864_ovl_05 = {-80, 64, 30, 10, -5, 1};
/* The opening's copy of D_800B7C10 */
// clang-format off
static ImpactEffect
    D_8020B86C_ovl_05[MAX_LIE_RATING + 1][IMPACT_TIMING_ENTRY_COUNT] = {
    {{-90, -64, -40, -10, 100, 1}, {-90, -64, -40, -10, 100, 1},
        {-90, -64, -40, -10, 100, 1}, {-90, -64, -40, -10, 100, 1},
        {-90, -64, -40, -10, 100, 1}, {0, 0, 0, 0, 0, 0},
        {-80, 64, 30, 10, -5, 1}, {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1}, {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1}},
    {{-90, -64, -40, -10, 100, 1}, {-90, -64, -40, -10, 100, 1},
        {-90, -64, -40, -10, 100, 1}, {-90, -64, -40, -10, 100, 1},
        {-4, -2, 1, -5, 5, 0}, {0, 0, 0, 0, 0, 0}, {-8, 2, -1, 5, 4, 0},
        {-80, 64, 30, 10, -5, 1}, {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1}, {-80, 64, 30, 10, -5, 1}},
    {{-90, -64, -40, -10, 100, 1}, {-90, -64, -40, -10, 100, 1},
        {-90, -64, -40, -10, 100, 1}, {-8, -8, 2, -10, 15, 0},
        {-4, -2, 1, -5, 5, 0}, {0, 0, 0, 0, 0, 0}, {-8, 2, -1, 5, 4, 0},
        {-12, 8, -2, 10, 4, 0}, {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1}, {-80, 64, 30, 10, -5, 1}},
    {{-90, -64, -40, -10, 100, 1}, {-90, -64, -40, -10, 100, 1},
        {-8, -8, 2, -10, 15, 0}, {-4, -2, 1, -5, 5, 0}, {-2, -1, 0, -3, 3, 0},
        {0, 0, 0, 0, 0, 0}, {-4, 1, 0, 3, 2, 0}, {-8, 2, -1, 5, 4, 0},
        {-12, 8, -2, 10, 4, 0}, {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1}},
    {{-90, -64, -40, -10, 100, 1}, {-8, -8, 2, -10, 15, 0},
        {-4, -2, 1, -5, 5, 0}, {-2, -1, 0, -3, 3, 0}, {-1, 0, 0, -2, 1, 0},
        {0, 0, 0, 0, 0, 0}, {-2, 0, 0, 2, 1, 0}, {-4, 1, 0, 3, 2, 0},
        {-8, 2, -1, 5, 4, 0}, {-12, 8, -2, 10, 4, 0}, {-80, 64, 30, 10, -5, 1}},
    {{-8, -8, 2, -10, 15, 0}, {-4, -2, 1, -5, 5, 0}, {-2, -1, 0, -3, 3, 0},
        {-1, 0, 0, -2, 1, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0}, {-2, 0, 0, 2, 1, 0}, {-4, 1, 0, 3, 2, 0},
        {-8, 2, -1, 5, 4, 0}, {-12, 8, -2, 10, 4, 0}}};
// clang-format on

/* The opening's clubs: two drivers that differ only in pitch */
static Club D_8020B9F8_ovl_05[OPENING_CLUB_COUNT] = {
    {0, "1W", 0, {0}, DRIVER_SURFACES, 208, -15, 0, 11, 120, CLUB_TYPE_DRIVER,
        {97608, 84651, 19335}, CLUB_KIND_WOOD, 0, {0, 0, 0},
        ASSET_DRIVER_HEAD_IMAGE, 0, {0}},
    {0, "1W", 0, {0}, DRIVER_SURFACES, 208, -23, 0, 11, 120, CLUB_TYPE_DRIVER,
        {97608, 84651, 19335}, CLUB_KIND_WOOD, 0, {0, 0, 0},
        ASSET_DRIVER_HEAD_IMAGE, 0, {0}}};

/* func_8020A488_ovl_05 counts each launch here, and nothing else reads it */
static s32 D_8020BA60_ovl_05 = 0;

/* The opening's copies of the collision triangle and fraction that
 * StepBallSimulation keeps on its stack */
static PlaneTriangle D_8022BE10_ovl_05;
static f32 D_8022BE28_ovl_05;

/* Loads one of the other characters into the slot with its birdie animation, or
 * frees a busy slot first */
s32 func_80206680_ovl_05(u32 slotIndex, s32 character) {
  if (IsCharacterSlotBusy(slotIndex)) {
    FreeCharacterSlot(slotIndex);
    return FALSE;
  }

  LoadCharacter(slotIndex, character, OTHER_CHARACTER_MODEL_KIND);
  SetCharacterAnimation(slotIndex, REACTION_ANIMATION_BIRDIE);
  SetAnimationSpeed(slotIndex, 1.0f);
  return TRUE;
}

/*
 * The opening's copies of the ball simulation's routines keep their names and
 * are static, so they don't clash with the originals. GCC 2.7 only warns when
 * a static definition follows the headers' prototypes.
 */

/* The opening's copy of GetSurfaceInfo, which reads its own surface table */
static SurfaceInfo* GetSurfaceInfo(Surface surface) {
  if ((u32)surface >= SURFACE_COUNT) {
    surface = SURFACE_OUT_OF_BOUNDS;
  }

  return &D_8020B564_ovl_05[surface];
}

/* An unused copy of ResetBallSimulations, which resets g_BallSimulations as
 * that does */
static void ResetBallSimulations(void) {
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

#define SPEED_TEXT_ROW_COUNT 8
#define SPEED_TEXT_FIRST_ROW 8

/*
 * The opening calls FindBillboardHit with a ninth argument, the simulation,
 * which FindBillboardHit does not read
 */
typedef s32 (*BillboardHitFinder)(s32 startX, s32 startY, s32 startZ, s32 endX,
    s32 endY, s32 endZ, s32* cell, f32* fraction, BallSimulation* simulation);

/*
 * The opening's copy of StepBallSimulation, which moves the ball through one
 * step. Unlike main's copy it works on the simulation's integer velocity
 * rather than a float copy, plays no sounds, starts no rumble or Cheep Cheep
 * leap, and can print the ball's speed, height and collisions as debug text.
 * The sparks it starts on a stroke's pin and wall hits are called at ovl_00's
 * addresses, which lie in the opening's BSS while it is loaded. Returns the
 * STEP_FLAG_* bits for the step.
 *
 * Doesn't match: the nested helpers match, but the routine keeps 614.4f and
 * 1996.8f in saved registers where the original reloads them, so 0.5f loses
 * the register it had and the floating-point registers are allocated
 * differently.
 */
#ifdef NON_MATCHING
s32 func_80206AA4_ovl_05(BallSimulation* simulation) {
  PlaneTriangle triangle;

  f32 GetStepNoise(void) {
    return ((u32)((simulation->position[AXIS_X] + simulation->position[AXIS_Z] +
                      g_Players[simulation->playerIndex].unk88) &
                0xFF) *
               0.0019607844f) +
        ((u32)((simulation->position[AXIS_X] - simulation->position[AXIS_Z] +
                   g_Players[simulation->playerIndex].unk88) &
             0xFF) *
            0.0019607844f);
  }

  f32 GetLength3D(f32 x, f32 y, f32 z) {
    return sqrtf((x * x) + (y * y) + (z * z));
  }

  f32 GetLength2D(f32 x, f32 z) {
    return sqrtf((x * x) + (z * z));
  }

  void ScaleToLength(f32 length, f32 * x, f32 * y, f32 * z) {
    f32 scale = length / sqrtf((*x * *x) + (*y * *y) + (*z * *z));

    *x *= scale;
    *y *= scale;
    *z *= scale;
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

    guRotateF(simulation->unk44, 90.0f, -axisX, 0.0f, axisZ);
  }

  f32 GetPositionNoise(void) {
    return (((simulation->position[AXIS_X] + simulation->position[AXIS_Z]) &
                0xFF) -
               127.5f) *
        0.007843138f;
  }

  char speedText[128];
  char heightText[128];
  f32 fraction;
  s32 startX;
  s32 startY;
  s32 startZ;
  s32 previousVelocityX;
  s32 previousVelocityZ;
  s32 contact;
  s32 flags = 0;
  s32 printsDebugText;
  s32 substep;
  s32 substepCount;
  Boolean isNearCup;
  s32 rollSpeed;

  printsDebugText = IsGameFlagSet(GAME_FLAG_LOG_VERTICAL_VELOCITY);
  substepCount = 1;
  isNearCup = FALSE;
  fraction = 1.0f;
  if (printsDebugText) {
    PrintDebugPaddedHex(simulation->state, 2, 0xF);
  }

  substep = 0;
  do {
    startX = simulation->position[AXIS_X];
    startY = simulation->position[AXIS_Y];
    simulation->unkB1 = simulation->unkA6;
    startZ = simulation->position[AXIS_Z];
    previousVelocityX = simulation->velocity[AXIS_X];
    previousVelocityZ = simulation->velocity[AXIS_Z];
    if (IsGameFlagSet(GAME_FLAG_SHOW_OPENING_BALL_SPEED) &&
        simulation->purpose == LAUNCH_PURPOSE_STROKE) {
      sprintf(speedText, "%fkm/h   ",
          (s32)IntLength3D(simulation->velocity[AXIS_X],
              simulation->velocity[AXIS_Y], simulation->velocity[AXIS_Z]) *
              0.0070312503f);
      PrintDebugText(speedText, 3, D_8020B858_ovl_05 + SPEED_TEXT_FIRST_ROW);
      D_8020B858_ovl_05 = (D_8020B858_ovl_05 + 1) % SPEED_TEXT_ROW_COUNT;
      PrintDebugText("                            ", 7,
          D_8020B858_ovl_05 + SPEED_TEXT_FIRST_ROW);
    }

    if (IsGameFlagSet(GAME_FLAG_SHOW_OPENING_BALL_HEIGHT)) {
      if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
        sprintf(heightText, "Height %fYd   ",
            (simulation->startPosition[AXIS_Y] - simulation->position[AXIS_Y]) *
                YARDS_PER_UNIT);
        PrintDebugText(heightText, 1, 0xF);
      }
    }

    if (simulation->state == BALL_STATE_STOPPED) {
      goto SETTLE;
    }

    if (simulation->unk98 == 0) {
      BuildUnk44Rotation();
    }

    simulation->unk98++;
    simulation->position[AXIS_X] += simulation->velocity[AXIS_X];
    simulation->position[AXIS_Y] += simulation->velocity[AXIS_Y];
    simulation->position[AXIS_Z] += simulation->velocity[AXIS_Z];
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
        if (Abs(simulation->position[AXIS_Y]) + Abs(startY) != 0) {
          fraction = (f32)simulation->position[AXIS_Y] /
              (Abs(simulation->position[AXIS_Y]) + Abs(startY));
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
        s32 hitsBillboard;
        s32 holeHit;

        if ((simulation->purpose == LAUNCH_PURPOSE_STROKE ||
                simulation->purpose == LAUNCH_PURPOSE_TRIAL) &&
            FindRingHit(startX, startY, startZ, simulation->position[AXIS_X],
                simulation->position[AXIS_Y], simulation->position[AXIS_Z],
                &D_8022BE10_ovl_05, &D_8022BE28_ovl_05) > 0) {
          s8* ringState = &g_Players[simulation->playerIndex]
                               .ringStates[D_8022BE10_ovl_05.unk0];

          if (*ringState == RING_STATE_SHOWN &&
              simulation->purpose == LAUNCH_PURPOSE_STROKE) {
            *ringState = RING_STATE_PASSED;
          }
        }

        if (g_GameState.isMiniGolf) {
          contact = FindMiniGolfGroundHit(simulation, startX, startY, startZ,
              simulation->position[AXIS_X], simulation->position[AXIS_Y],
              simulation->position[AXIS_Z], &D_8022BE10_ovl_05,
              &D_8022BE28_ovl_05);
          if (g_GameState.isMiniGolf &&
              simulation->state == BALL_STATE_ROLLING) {
            s32 putterContact = FindMiniGolfGroundHit(simulation, startX,
                startY, startZ, simulation->position[AXIS_X],
                simulation->position[AXIS_Y] - 0x20C,
                simulation->position[AXIS_Z], &triangle, &fraction);

            if (putterContact && fraction < D_8022BE28_ovl_05) {
              contact = putterContact;
              osSyncPrintf("PutterSpecialGroundCollision\n");
              D_8022BE10_ovl_05 = triangle;
              D_8022BE28_ovl_05 = fraction;
            }
          }
        } else {
          contact = FindGroundHit(startX, startY, startZ,
              simulation->position[AXIS_X], simulation->position[AXIS_Y],
              simulation->position[AXIS_Z], &D_8022BE10_ovl_05,
              &D_8022BE28_ovl_05);
        }

        hitsBillboard = FALSE;
        if (simulation->state == BALL_STATE_FLYING) {
          if (simulation->unk9C + 2 < simulation->unk98) {
            hitsBillboard = ((BillboardHitFinder)FindBillboardHit)(startX,
                startY, startZ, simulation->position[AXIS_X],
                simulation->position[AXIS_Y], simulation->position[AXIS_Z],
                &triangle.unk0, &fraction, simulation);
          } else {
            hitsBillboard = FALSE;
          }
        }

        if (hitsBillboard && contact == STEP_FLAG_GROUND_HIT) {
          if (D_8022BE28_ovl_05 <= fraction) {
            contact = STEP_FLAG_GROUND_HIT;
            triangle = D_8022BE10_ovl_05;
            fraction = D_8022BE28_ovl_05;
          } else {
            contact = STEP_FLAG_BILLBOARD_HIT;
          }
        } else if (hitsBillboard) {
          contact = STEP_FLAG_BILLBOARD_HIT;
        } else if (contact == STEP_FLAG_GROUND_HIT) {
          triangle = D_8022BE10_ovl_05;
          fraction = D_8022BE28_ovl_05;
        } else {
          contact = 0;
        }

        if (simulation->state == BALL_STATE_FLYING ||
            simulation->state == BALL_STATE_ROLLING) {
          holeHit = FindHoleTriangleHit(startX, startY, startZ,
              simulation->position[AXIS_X], simulation->position[AXIS_Y],
              simulation->position[AXIS_Z], &D_8022BE10_ovl_05,
              &D_8022BE28_ovl_05);
        } else {
          holeHit = FALSE;
        }

        if (holeHit && (contact == 0 || D_8022BE28_ovl_05 <= fraction)) {
          contact = STEP_FLAG_HOLE_TRIANGLE_HIT;
          triangle = D_8022BE10_ovl_05;
          fraction = D_8022BE28_ovl_05;
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

      if (g_GameState.isMiniGolf
              ? g_Players[simulation->playerIndex].strokeCount == 1
              : GetSurfaceInfo(simulation->unkA2)->kind != SURFACE_KIND_GREEN) {
        pinHit =
            FindPinHit(startX, startY, startZ, simulation->position[AXIS_X],
                simulation->position[AXIS_Y], simulation->position[AXIS_Z],
                pinVelocity, &pinFraction, simulation->hasHitFlag, simulation);
        if (pinHit && !(simulation->unkB1 & STEP_FLAG_PIN_HIT)) {
          simulation->position[AXIS_X] =
              startX + (pinFraction * simulation->velocity[AXIS_X]);
          simulation->position[AXIS_Y] =
              startY + (pinFraction * simulation->velocity[AXIS_Y]);
          simulation->position[AXIS_Z] =
              startZ + (pinFraction * simulation->velocity[AXIS_Z]);
          if (pinHit == PIN_HIT_FLAG) {
            simulation->hasHitFlag = TRUE;
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
            GetLength2D(
                simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]);
          }

          if (simulation->state == BALL_STATE_ROLLING) {
            simulation->state = BALL_STATE_FLYING;
            simulation->velocity[AXIS_Y] =
                -GetLength2D(simulation->velocity[AXIS_X],
                    simulation->velocity[AXIS_Z]) *
                0.5f;
            contact = 0;
            simulation->unk3C = atan2(pinVelocity[AXIS_Z], pinVelocity[AXIS_X]);
            simulation->unk34 =
                IntLength2D(pinVelocity[AXIS_X], pinVelocity[AXIS_Z]);
          } else {
            simulation->velocity[AXIS_Y] = pinVelocity[AXIS_Y];
            simulation->unk34 = simulation->unk34 * 0.3f;
          }

          {
            f32 rest = 1.3f - pinFraction;

            simulation->velocity[AXIS_X] = pinVelocity[AXIS_X];
            simulation->velocity[AXIS_Z] = pinVelocity[AXIS_Z];
            flags |= STEP_FLAG_PIN_HIT;
            simulation->position[AXIS_X] = simulation->position[AXIS_X] +
                (rest * simulation->velocity[AXIS_X]);
            simulation->position[AXIS_Y] = simulation->position[AXIS_Y] +
                (rest * simulation->velocity[AXIS_Y]);
            simulation->position[AXIS_Z] = simulation->position[AXIS_Z] +
                (rest * simulation->velocity[AXIS_Z]);
          }

          MoveBallToCupEdge(simulation);
          BuildUnk44Rotation();
        }
      }

      if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        s32 cupOffsetX =
            simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X];

        if (Abs(cupOffsetX) < 0xC00) {
          s32 cupOffsetZ =
              simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z];

          if (Abs(cupOffsetZ) < 0xC00) {
            s32 cupDistance = IntLength2D(cupOffsetX, cupOffsetZ);

            if (simulation->state != BALL_STATE_IN_CUP &&
                simulation->position[AXIS_Y] -
                        g_HoleSetup.cupPosition[AXIS_Y] >=
                    -0x2FF &&
                (f32)cupDistance < 1228.8f) {
              f32 speed = (s32)IntLength2D(
                  simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]);

              if (speed < 1075.2f &&
                  (f32)simulation->velocity[AXIS_Y] >= -711.168f) {
                if (printsDebugText) {
                  SetDebugTextHighlight(1);
                  PrintDebugText("CUPIN!", 0x14, 0xA);
                }

                simulation->state = BALL_STATE_IN_CUP;
                if (speed < 268.8f &&
                    (f32)simulation->velocity[AXIS_Y] <= 711.168f) {
                  simulation->unkA8[0] = 1;
                }
              }
            }

            if (simulation->state == BALL_STATE_ROLLING &&
                (f32)Abs(simulation->position[AXIS_Y] -
                    g_HoleSetup.cupPosition[AXIS_Y]) < 614.4f) {
              if ((f32)cupDistance < 1996.7999f &&
                  (f32)cupDistance >= 1075.2f) {
                f32 toCup[3];
                f32 direction[3];
                f32 alignment;
                f32 depth;
                Radians angle;
                f32 pull;

                isNearCup = TRUE;
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
                direction[AXIS_X] = simulation->velocity[AXIS_X];
                direction[AXIS_Y] = 8384.0f;
                direction[AXIS_Z] = simulation->velocity[AXIS_Z];
                guNormalize(
                    &direction[AXIS_X], &direction[AXIS_Y], &direction[AXIS_Z]);
                alignment = (toCup[AXIS_X] * direction[AXIS_X]) +
                    (toCup[AXIS_Z] * direction[AXIS_Z]);
                if (alignment < 0.0f) {
                  alignment = 0.0f;
                }

                depth = 1996.7999f - (f32)cupDistance;
                angle = atan2(toCup[AXIS_Z], toCup[AXIS_X]);
                if (depth > 614.4f) {
                  depth = 614.4f;
                }

                pull = ((sinf(depth * 0.005113269f) * 307.2f) + 46.079998f) *
                    (1.0f - alignment);
                simulation->velocity[AXIS_X] =
                    simulation->velocity[AXIS_X] * 0.93f;
                simulation->velocity[AXIS_Z] =
                    simulation->velocity[AXIS_Z] * 0.93f;
                simulation->velocity[AXIS_X] =
                    simulation->velocity[AXIS_X] + (cosf(angle) * pull);
                simulation->velocity[AXIS_Z] =
                    simulation->velocity[AXIS_Z] + (sinf(angle) * pull);
              }

              if ((f32)cupDistance > 1228.8f &&
                  (f32)IntLength2D(startX - g_HoleSetup.cupPosition[AXIS_X],
                      startZ - g_HoleSetup.cupPosition[AXIS_Z]) < 1228.8f &&
                  (f32)Abs(startY - g_HoleSetup.cupPosition[AXIS_Y]) < 307.2f) {
                s32 speed = IntLength2D(
                    simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]);

                if ((f32)speed < 4300.8f &&
                    simulation->state == BALL_STATE_ROLLING) {
                  f32 keep = speed * 0.0004650298f;
                  f32 fallSpeed;

                  if (keep < 0.0f) {
                    keep = 0.0f;
                  }

                  if (keep > 1.0f) {
                    keep = 1.0f;
                  }

                  fallSpeed = -speed * sqrtf(1.0f - (keep * keep));
                  simulation->state = BALL_STATE_FLYING;
                  simulation->velocity[AXIS_X] =
                      simulation->velocity[AXIS_X] * keep;
                  simulation->velocity[AXIS_Z] =
                      simulation->velocity[AXIS_Z] * keep;
                  simulation->velocity[AXIS_Y] = fallSpeed;
                  simulation->unk3C = atan2(simulation->velocity[AXIS_Z],
                      simulation->velocity[AXIS_X]);
                  simulation->unk34 = IntLength2D(simulation->velocity[AXIS_X],
                                          simulation->velocity[AXIS_Z]) *
                      0.5f;
                  flags |= STEP_FLAG_LIPPED_OUT;
                  contact = 0;
                  BuildUnk44Rotation();
                }
              }
            }
          }
        }
      }
    }

    if (simulation->state == BALL_STATE_IN_WATER) {
      simulation->unkA1 = TRUE;
      simulation->velocity[AXIS_X] = simulation->velocity[AXIS_X] * 0.7f;
      simulation->velocity[AXIS_Z] = simulation->velocity[AXIS_Z] * 0.7f;
      simulation->velocity[AXIS_Y] =
          (simulation->velocity[AXIS_Y] + 0x106) * 0.7f;
      if (GetGroundOrWaterHeight(
              simulation->position[AXIS_X], simulation->position[AXIS_Z]) +
                  0x12C00 <
              simulation->position[AXIS_Y] ||
          simulation->position[AXIS_Y] >=
              GetGroundHeight(
                  simulation->position[AXIS_X], simulation->position[AXIS_Z])) {
        simulation->state = BALL_STATE_STOPPED;
      }
    }

    if (simulation->state == BALL_STATE_IN_CUP) {
      f32 cupDistance;

      GetLength3D(
          simulation->velocity[AXIS_X], 0.0f, simulation->velocity[AXIS_Z]);
      simulation->surface = SURFACE_GREEN;
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
        push = ((toCupX * simulation->velocity[AXIS_X]) +
                   (toCupY * simulation->velocity[AXIS_Y]) +
                   (toCupZ * simulation->velocity[AXIS_Z])) *
            1.5f;
        scale = 1075.2f / cupDistance;
        simulation->velocity[AXIS_X] =
            simulation->velocity[AXIS_X] - (toCupX * push);
        simulation->velocity[AXIS_Y] =
            simulation->velocity[AXIS_Y] - (toCupY * push);
        simulation->velocity[AXIS_Z] =
            simulation->velocity[AXIS_Z] - (toCupZ * push);
        simulation->position[AXIS_X] =
            ((simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X]) *
                scale) +
            g_HoleSetup.cupPosition[AXIS_X];
        simulation->position[AXIS_Z] =
            ((simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]) *
                scale) +
            g_HoleSetup.cupPosition[AXIS_Z];
      }

      if (simulation->position[AXIS_Y] <
              g_HoleSetup.cupPosition[AXIS_Y] + 768.0f &&
          simulation->velocity[AXIS_Y] < -711.168f) {
        s32 toCupX =
            g_HoleSetup.cupPosition[AXIS_X] - simulation->position[AXIS_X];
        s32 toCupZ =
            g_HoleSetup.cupPosition[AXIS_Z] - simulation->position[AXIS_Z];

        if ((toCupX | toCupZ) != 0) {
          u32 scale = (u32)Abs(simulation->velocity[AXIS_Y]) /
              IntLength2D(toCupX, toCupZ);

          simulation->velocity[AXIS_X] = toCupX * scale;
          simulation->velocity[AXIS_Z] = toCupZ * scale;
          simulation->position[AXIS_Y] =
              g_HoleSetup.cupPosition[AXIS_Y] + 768.0f;
        }

        simulation->velocity[AXIS_Y] = Abs(simulation->velocity[AXIS_Y]);
      }

      if (simulation->position[AXIS_Y] >=
          g_HoleSetup.cupPosition[AXIS_Y] + 0xC00) {
        simulation->velocity[AXIS_Y] =
            (f32)(-simulation->velocity[AXIS_Y] / 2) + 131.0f;
        /* The chained assignment is there to match */
        simulation->velocity[AXIS_X] = simulation->velocity[AXIS_Z] = 0;
        if (simulation->velocity[AXIS_Y] > 0) {
          simulation->isHoledOut = 1;
          simulation->state = BALL_STATE_STOPPED;
        }

        simulation->position[AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y] + 0xC00;
      } else {
        simulation->velocity[AXIS_Y] = simulation->velocity[AXIS_Y] + 131.0f;
      }
    }

    if (contact == STEP_FLAG_BILLBOARD_HIT &&
        simulation->state == BALL_STATE_FLYING) {
      simulation->unk94++;
      simulation->unk34 /= 2;
      simulation->unk9C = simulation->unk98;
      simulation->unk38 /= 2;
      simulation->position[AXIS_X] =
          startX + (fraction * simulation->velocity[AXIS_X]);
      simulation->position[AXIS_Y] =
          startY + (fraction * simulation->velocity[AXIS_Y]);
      simulation->position[AXIS_Z] =
          startZ + (fraction * simulation->velocity[AXIS_Z]);
      switch (triangle.unk0) {
        case 1:
        case 4:
          simulation->velocity[AXIS_X] = -simulation->velocity[AXIS_Z] / 2;
          simulation->velocity[AXIS_Z] = -simulation->velocity[AXIS_X] / 2;
          break;
        case 2:
        default:
          simulation->velocity[AXIS_X] /= 2;
          simulation->velocity[AXIS_Z] /= 2;
          if (simulation->velocity[AXIS_Y] < 0) {
            simulation->velocity[AXIS_Y] = simulation->velocity[AXIS_Y] * 1.2f;
          }

          break;
      }
    }

    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST &&
        (g_GameState.isMiniGolf || IsGameFlagSet(GAME_FLAG_WALL_HITS)) &&
        FindWallHit(startX, startY, startZ, simulation->position[AXIS_X],
            simulation->position[AXIS_Y], simulation->position[AXIS_Z],
            &D_8022BE10_ovl_05, &D_8022BE28_ovl_05) == TRUE &&
        D_8022BE10_ovl_05.unk0 == 3) {
      CoursePoint hitPosition;
      f32 normalX;
      f32 normalY;
      f32 normalZ;
      f32 edgeX;
      f32 edgeY;
      f32 edgeZ;
      f32 approach;

      normalX = D_8022BE10_ovl_05.corners[AXIS_X][0];
      edgeY = 0.0f;
      normalY = edgeY;
      normalZ = D_8022BE10_ovl_05.corners[AXIS_Z][0];
      if (D_8022BE10_ovl_05.corners[AXIS_X][0] ==
              D_8022BE10_ovl_05.corners[AXIS_X][1] &&
          D_8022BE10_ovl_05.corners[AXIS_Z][0] ==
              D_8022BE10_ovl_05.corners[AXIS_Z][1]) {
        edgeX = D_8022BE10_ovl_05.corners[AXIS_X][2];
        edgeZ = D_8022BE10_ovl_05.corners[AXIS_Z][2];
      } else {
        edgeX = D_8022BE10_ovl_05.corners[AXIS_X][1];
        edgeZ = D_8022BE10_ovl_05.corners[AXIS_Z][1];
      }

      normalX -= edgeX;
      normalY -= edgeY;
      normalZ -= edgeZ;
      guNormalize(&normalX, &normalY, &normalZ);
      approach = (-normalZ * simulation->velocity[AXIS_X]) +
          (normalY * simulation->velocity[AXIS_Y]) +
          (normalX * simulation->velocity[AXIS_Z]);
      D_8022BE28_ovl_05 -= 614.4f /
          GetLength3D(simulation->velocity[AXIS_X],
              simulation->velocity[AXIS_Y], simulation->velocity[AXIS_Z]);
      if (D_8022BE28_ovl_05 < 0.0f) {
        D_8022BE28_ovl_05 = 0.0f;
      }

      hitPosition[AXIS_X] =
          startX + (D_8022BE28_ovl_05 * (simulation->velocity[AXIS_X] * 0.99f));
      hitPosition[AXIS_Y] =
          startY + (D_8022BE28_ovl_05 * (simulation->velocity[AXIS_Y] * 0.99f));
      hitPosition[AXIS_Z] =
          startZ + (D_8022BE28_ovl_05 * (simulation->velocity[AXIS_Z] * 0.99f));
      simulation->position[AXIS_X] = hitPosition[AXIS_X];
      simulation->position[AXIS_Y] = hitPosition[AXIS_Y];
      simulation->position[AXIS_Z] = hitPosition[AXIS_Z];
      if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
        StartFlagstickSparks(simulation->position);
      }

      {
        f32 reflected[3];
        f32 push = 2.0f * approach;

        reflected[AXIS_X] = simulation->velocity[AXIS_X] - (-normalZ * push);
        reflected[AXIS_Y] = simulation->velocity[AXIS_Y] - (normalY * push);
        reflected[AXIS_Z] = simulation->velocity[AXIS_Z] - (normalX * push);
        simulation->velocity[AXIS_X] = reflected[AXIS_X];
        simulation->velocity[AXIS_Y] = reflected[AXIS_Y];
        simulation->unk34 = 0;
        simulation->velocity[AXIS_Z] = reflected[AXIS_Z];
      }
      BuildUnk44Rotation();
      contact &= ~STEP_FLAG_HOLE_TRIANGLE_HIT;
    }

    if (contact == STEP_FLAG_HOLE_TRIANGLE_HIT &&
        (simulation->state == BALL_STATE_FLYING ||
            simulation->state == BALL_STATE_ROLLING)) {
      TrianglePlane plane;
      s32 bounced[3];
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
          startX + (fraction * simulation->velocity[AXIS_X]);
      simulation->position[AXIS_Y] =
          startY + (fraction * simulation->velocity[AXIS_Y]);
      simulation->position[AXIS_Z] =
          startZ + (fraction * simulation->velocity[AXIS_Z]);
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

      impactSpeed = (plane.normal[AXIS_X] * simulation->velocity[AXIS_X]) +
          (plane.normal[AXIS_Y] * simulation->velocity[AXIS_Y]) +
          (plane.normal[AXIS_Z] * simulation->velocity[AXIS_Z]);
      speed = IntLength3D(simulation->velocity[AXIS_X],
          simulation->velocity[AXIS_Y], simulation->velocity[AXIS_Z]);
      if (speed != 0) {
        steepness = AbsoluteValueExpression(impactSpeed) / speed;
      } else {
        steepness = 1.0f;
      }

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
      } else if (simulation->surface == SURFACE_WATER_HAZARD) {
        if (simulation->state == BALL_STATE_ROLLING ||
            impactSpeed <= 1422.336f ||
            GetLength2D(
                simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]) *
                    0.5f <=
                impactSpeed) {
          simulation->state = BALL_STATE_IN_WATER;
          /* The chained assignment is there to match */
          simulation->velocity[AXIS_X] = simulation->velocity[AXIS_Z] = 0;
        }
      }

      softness = 1000 - GetSurfaceInfo(simulation->surface)->unkE;
      keep = (1000.0f - (softness * (steepness * 0.1f) * steepness)) * 0.001f;
      if (simulation->unk98 < 3) {
        keep *= 0.25;
      }

      bounce = 1.0f - (softness * steepness * 0.001f);
      push = (bounce + 1.0f) * impactSpeed;
      bounced[AXIS_X] =
          simulation->velocity[AXIS_X] - (plane.normal[AXIS_X] * push);
      bounced[AXIS_Y] =
          simulation->velocity[AXIS_Y] - (plane.normal[AXIS_Y] * push);
      bounced[AXIS_Z] =
          simulation->velocity[AXIS_Z] - (plane.normal[AXIS_Z] * push);
      simulation->velocity[AXIS_X] = bounced[AXIS_X] * keep;
      simulation->velocity[AXIS_Y] = bounced[AXIS_Y] * keep;
      simulation->unk9C = simulation->unk98;
      simulation->velocity[AXIS_Z] = bounced[AXIS_Z] * keep;
      if (simulation->state == BALL_STATE_FLYING) {
        s32 unk34X = simulation->unk34 * cosf(simulation->unk3C);
        s32 unk34Z = simulation->unk34 * sinf(simulation->unk3C);
        f32 grip = 1.0f - bounce;

        simulation->velocity[AXIS_X] = simulation->velocity[AXIS_X] +
            ((unk34X - simulation->velocity[AXIS_X]) * grip *
                AbsoluteValueExpression(plane.normal[AXIS_Y]) * 0.25f);
        simulation->velocity[AXIS_Z] = simulation->velocity[AXIS_Z] +
            ((unk34Z - simulation->velocity[AXIS_Z]) * grip *
                AbsoluteValueExpression(plane.normal[AXIS_Y]) * 0.25f);
        unk34X = unk34X * 0.9f;
        unk34Z = unk34Z * 0.9f;
        unk34X = unk34X + ((previousVelocityX - unk34X) * 0.25f);
        unk34Z = unk34Z + ((previousVelocityZ - unk34Z) * 0.25f);
        simulation->unk3C = atan2(unk34Z, unk34X);
        simulation->unk34 = IntLength2D(unk34X, unk34Z);
        BuildUnk44Rotation();
      }

      simulation->unk38 = 0;
      simulation->velocity[AXIS_Y] += 0x106;
    }

    if (simulation->state == BALL_STATE_FLYING &&
        contact == STEP_FLAG_GROUND_HIT) {
      TrianglePlane plane;
      f32 impactSpeed;
      f32 steepness;
      s32 speed;
      f32 friction;
      f32 spin;
      f32 restitution;

      simulation->unk94++;
      GetTrianglePlane(&triangle, &plane);
      GetLength3D(simulation->velocity[AXIS_X], simulation->velocity[AXIS_Y],
          simulation->velocity[AXIS_Z]);
      simulation->position[AXIS_X] =
          startX + (fraction * simulation->velocity[AXIS_X]);
      simulation->position[AXIS_Y] =
          startY + (fraction * simulation->velocity[AXIS_Y]);
      simulation->position[AXIS_Z] =
          startZ + (fraction * simulation->velocity[AXIS_Z]);
      substepCount += (fraction < 0.4f) & (substepCount == 1);
      if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
        simulation->surface = SURFACE_FAIRWAY;
      } else {
        simulation->surface = GetSurface(
            simulation->position[AXIS_X], simulation->position[AXIS_Z]);
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

      impactSpeed = (plane.normal[AXIS_X] * simulation->velocity[AXIS_X]) +
          (plane.normal[AXIS_Y] * simulation->velocity[AXIS_Y]) +
          (plane.normal[AXIS_Z] * simulation->velocity[AXIS_Z]);
      speed = IntLength3D(simulation->velocity[AXIS_X],
          simulation->velocity[AXIS_Y], simulation->velocity[AXIS_Z]);
      if (speed != 0) {
        steepness = AbsoluteValueExpression(impactSpeed) / speed;
      } else {
        steepness = 1.0f;
      }

      friction = (1000 - GetSurfaceInfo(simulation->surface)->unkE) * 0.001f;
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
      } else if (simulation->surface == SURFACE_WATER_HAZARD) {
        if (impactSpeed >= 2844.672f) {
          simulation->state = BALL_STATE_STOPPED;
          simulation->position[AXIS_Y] = GetGroundHeight(
              simulation->position[AXIS_X], simulation->position[AXIS_Z]);
        }
      }

      switch (simulation->surface) {
        case SURFACE_GREEN:
        case SURFACE_GREEN_EDGE:
          friction *=
              ((impactSpeed + (impactSpeed * GetPositionNoise() * 0.1f)) *
                  0.000112491f) +
              0.5f;
          spin = ((impactSpeed * steepness) - 1422.336f) * 0.000035153436f;
          break;
        default:
          friction *= (impactSpeed * 0.00007030687f) + 0.3f;
          friction += friction * GetPositionNoise() * 0.1f;
          spin = ((impactSpeed * steepness) - 1422.336f) * 0.000035153436f;
          break;
      }

      if (spin < 0.0f) {
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
        if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
          simulation->position[AXIS_Y] = 0;
        } else {
          simulation->position[AXIS_Y] = GetGroundHeight(
              simulation->position[AXIS_X], simulation->position[AXIS_Z]);
        }

        unk34X = simulation->unk34 * cosf(simulation->unk3C);
        unk34Z = simulation->unk34 * sinf(simulation->unk3C);
        simulation->velocity[AXIS_X] = simulation->velocity[AXIS_X] +
            ((unk34X - simulation->velocity[AXIS_X]) *
                AbsoluteValueExpression(plane.normal[AXIS_Y]) * 0.5f);
        simulation->unk34 = 0;
        simulation->velocity[AXIS_Z] = simulation->velocity[AXIS_Z] +
            ((unk34Z - simulation->velocity[AXIS_Z]) *
                AbsoluteValueExpression(plane.normal[AXIS_Y]) * 0.5f);
      } else {
        f32 keep = 1.0f - spin;
        s32 bounced[3];
        f32 push;
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

        if (keep > 1.0f) {
          keep = 1.0f;
        }

        if (simulation->unk98 < 2 && !g_GameState.isMiniGolf) {
          keep *= 0.25;
          osSyncPrintf("Ouch! %d, %d\n", startY, simulation->position[AXIS_Y]);
        }

        push = (restitution + 1.0f) * impactSpeed;
        bounced[AXIS_X] =
            simulation->velocity[AXIS_X] - (plane.normal[AXIS_X] * push);
        bounced[AXIS_Y] =
            simulation->velocity[AXIS_Y] - (plane.normal[AXIS_Y] * push);
        bounced[AXIS_Z] =
            simulation->velocity[AXIS_Z] - (plane.normal[AXIS_Z] * push);
        simulation->velocity[AXIS_X] = bounced[AXIS_X] * keep;
        simulation->velocity[AXIS_Y] = bounced[AXIS_Y] * keep;
        simulation->unk9C = simulation->unk98;
        simulation->velocity[AXIS_Z] = bounced[AXIS_Z] * keep;
        unk34X = simulation->unk34 * cosf(simulation->unk3C);
        unk34Z = simulation->unk34 * sinf(simulation->unk3C);
        grip = 1.0f - restitution;
        driftX = (unk34X - simulation->velocity[AXIS_X]) * grip *
            AbsoluteValueExpression(plane.normal[AXIS_Y]) * 0.25f;
        driftZ = (unk34Z - simulation->velocity[AXIS_Z]) * grip *
            AbsoluteValueExpression(plane.normal[AXIS_Y]) * 0.25f;
        driftLimit = impactSpeed * 0.2f;
        drift = (s32)IntLength2D(driftX, driftZ);
        if (driftLimit < drift && impactSpeed > 1.0f) {
          f32 scale = driftLimit / drift;

          driftX = driftX * scale;
          driftZ = driftZ * scale;
        }

        unk34X = unk34X * 0.9f;
        unk34Z = unk34Z * 0.9f;
        simulation->velocity[AXIS_X] += driftX;
        simulation->velocity[AXIS_Z] += driftZ;
        unk34X = unk34X + ((previousVelocityX - unk34X) * 0.2f);
        unk34Z = unk34Z + ((previousVelocityZ - unk34Z) * 0.2f);
        simulation->unk3C = atan2(unk34Z, unk34X);
        simulation->unk34 = IntLength2D(unk34X, unk34Z);
        BuildUnk44Rotation();
        simulation->unk38 = 0;
        simulation->velocity[AXIS_Y] += 0x106;
      }
    }

    if (simulation->state == BALL_STATE_ROLLING && contact == 0) {
      simulation->state = BALL_STATE_FLYING;
      simulation->unk3C =
          atan2(simulation->velocity[AXIS_Z], simulation->velocity[AXIS_X]);
      simulation->unk34 = IntLength2D(
          simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]);
      BuildUnk44Rotation();
    }

    if (simulation->state == BALL_STATE_ROLLING && !isNearCup) {
      TrianglePlane plane;
      SurfaceInfo* info;
      s32 groundHeight;
      s32 slopeX;
      s32 slopeZ;
      f32 friction;
      Boolean isSteep;

      rollSpeed = GetLength2D(
          simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]);
      isSteep = FALSE;
      if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
        contact = STEP_FLAG_GROUND_HIT;
        info = &D_8020B564_ovl_05[SURFACE_GREEN];
        triangle.corners[AXIS_X][0] = 0;
        triangle.corners[AXIS_X][1] = 0x100;
        triangle.corners[AXIS_X][2] = 0;
        triangle.corners[AXIS_Y][0] = 0;
        triangle.corners[AXIS_Y][1] = 0;
        triangle.corners[AXIS_Y][2] = 0;
        triangle.corners[AXIS_Z][0] = 0;
        triangle.corners[AXIS_Z][1] = 0;
        triangle.corners[AXIS_Z][2] = 0x100;
        if (D_8020B560_ovl_05 == CLUB_TEST_SHOT) {
          info = &D_8020B564_ovl_05[SURFACE_FAIRWAY];
        }

        groundHeight = 0;
        GetTrianglePlane(&triangle, &plane);
      } else {
        GetTrianglePlane(&triangle, &plane);
        if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST &&
            g_GameState.isMiniGolf) {
          f32 pressing = (plane.normal[AXIS_X] * simulation->velocity[AXIS_X]) +
              (plane.normal[AXIS_Y] * simulation->velocity[AXIS_Y]) +
              (plane.normal[AXIS_Z] * simulation->velocity[AXIS_Z]);

          if (pressing >= 1422.336f) {
            f32 along[3];

            along[AXIS_X] = simulation->velocity[AXIS_X] -
                (plane.normal[AXIS_X] * pressing);
            along[AXIS_Y] = simulation->velocity[AXIS_Y] -
                (plane.normal[AXIS_Y] * pressing);
            along[AXIS_Z] = simulation->velocity[AXIS_Z] -
                (plane.normal[AXIS_Z] * pressing);
            simulation->position[AXIS_X] =
                startX + (fraction * simulation->velocity[AXIS_X]);
            simulation->position[AXIS_Y] =
                startY + (fraction * simulation->velocity[AXIS_Y]);
            simulation->position[AXIS_Z] =
                startZ + (fraction * simulation->velocity[AXIS_Z]);
            simulation->velocity[AXIS_X] = along[AXIS_X];
            simulation->velocity[AXIS_Y] = along[AXIS_Y];
            simulation->velocity[AXIS_Z] = along[AXIS_Z];
          }
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
          goto SETTLE;
        }

        groundHeight = GetGroundHeight(
            simulation->position[AXIS_X], simulation->position[AXIS_Z]);
        if (simulation->position[AXIS_Y] + 307.2f < groundHeight) {
          simulation->state = BALL_STATE_FLYING;
          simulation->unk3C =
              atan2(simulation->velocity[AXIS_Z], simulation->velocity[AXIS_X]);
          contact = 0;
          simulation->unk34 = IntLength2D(
              simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]);
          BuildUnk44Rotation();
          goto ROLLED;
        }
      }

      slopeX = (-plane.normal[AXIS_X] * 262.0f) /
          AbsoluteValueExpression(plane.normal[AXIS_Y]);
      slopeZ = (-plane.normal[AXIS_Z] * 262.0f) /
          AbsoluteValueExpression(plane.normal[AXIS_Y]);
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

      if (isSteep) {
        slopeX *= 2;
        slopeZ *= 2;
        if (friction < 0.9f) {
          friction = 0.9f;
        }
      } else {
        if ((f32)rollSpeed < 307.2f) {
          f32 scale = (rollSpeed * 0.0030517578f) + 0.0625f;

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

      simulation->velocity[AXIS_X] = (slopeX * friction * 0.4f) +
          (simulation->velocity[AXIS_X] * friction);
      simulation->velocity[AXIS_Z] = (slopeZ * friction * 0.4f) +
          (simulation->velocity[AXIS_Z] * friction);
      if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST &&
          g_GameState.isMiniGolf) {
        if (groundHeight - startY < simulation->velocity[AXIS_Y]) {
          simulation->velocity[AXIS_Y] = groundHeight - startY + 0x20C;
        } else {
          simulation->velocity[AXIS_Y] += 0x20C;
        }
      } else {
        if (groundHeight - startY < simulation->velocity[AXIS_Y]) {
          simulation->velocity[AXIS_Y] = groundHeight - startY + 0x106;
        } else {
          simulation->velocity[AXIS_Y] += 0x106;
        }
      }

      if (simulation->purpose == LAUNCH_PURPOSE_STROKE &&
          IsGameFlagSet(GAME_FLAG_LOG_VERTICAL_VELOCITY)) {
        osSyncPrintf("V[1] %d\n", simulation->velocity[AXIS_Y]);
      }

      if (IntLength2D(simulation->velocity[AXIS_X],
              simulation->velocity[AXIS_Z]) < 30) {
        if (!isSteep && IntLength2D(slopeX, slopeZ) * friction < 500.0f) {
          simulation->state = BALL_STATE_STOPPED;
        }
      }

      if (!isSteep && info->kind != SURFACE_KIND_GREEN &&
          simulation->unk9C + 90 < simulation->unk98 &&
          GetLength3D(simulation->velocity[AXIS_X],
              simulation->velocity[AXIS_Y],
              simulation->velocity[AXIS_Z]) < (f64)284.4672f) {
        simulation->state = BALL_STATE_STOPPED;
      }
    }

ROLLED:

    if ((simulation->state == BALL_STATE_FLYING ||
            simulation->state == BALL_STATE_ROLLING) &&
        contact == 0) {
      f32 relative[3];
      f32 turned[3];
      f32 airSpeed;
      f32 drag;
      f32 unk38;
      f32 horizontalSpeed;
      f32 unk38Speed;
      f32 ratio;
      f32 weight;
      f32 curve;
      f32 unk34Speed;
      f32 lift;
      f32 height;
      f32 windX;
      f32 windZ;

      relative[AXIS_X] =
          simulation->velocity[AXIS_X] - simulation->windVelocityX;
      relative[AXIS_Y] = simulation->velocity[AXIS_Y];
      relative[AXIS_Z] =
          simulation->velocity[AXIS_Z] - simulation->windVelocityZ;
      airSpeed =
          GetLength3D(relative[AXIS_X], relative[AXIS_Y], relative[AXIS_Z]);
      drag = (sqrtf(airSpeed * 0.0140613755f) * 0.0011f) + 0.004f;
      unk38 = simulation->unk38;
      unk38Speed = AbsoluteValueExpression(unk38 * 8.0f);
      horizontalSpeed = GetLength2D(relative[AXIS_X], relative[AXIS_Z]);
      ratio = 0.0f;
      if (unk38Speed != 0.0f) {
        ratio = horizontalSpeed / unk38Speed;
      }

      weight = 1.0f - (AbsoluteValueExpression(ratio - 1.0f) * 0.8f);
      if (weight < 0.0f) {
        weight = 0.0f;
      }

      curve = unk38 * (weight * 0.038146973f);
      if (horizontalSpeed < AbsoluteValueExpression(curve) * 100.0f) {
        curve = horizontalSpeed * 0.01f;
        if (simulation->unk38 < 0) {
          curve = -curve;
        }
      }

      if (horizontalSpeed > 1422.336f) {
        f32 scale = (1.0f / horizontalSpeed) * curve;

        simulation->velocity[AXIS_X] =
            simulation->velocity[AXIS_X] + (-relative[AXIS_Z] * scale);
        simulation->velocity[AXIS_Z] =
            simulation->velocity[AXIS_Z] + (relative[AXIS_X] * scale);
      }

      simulation->unk38 = simulation->unk38 * 0.995f;
      unk34Speed = AbsoluteValueExpression(simulation->unk34 * 8.0f);
      ratio = 0.0f;
      if (unk34Speed != 0.0f) {
        ratio = airSpeed / unk34Speed;
      }

      weight = 1.0f - (AbsoluteValueExpression(ratio - 1.0f) * 0.8f);
      if (weight < 0.0f) {
        weight = 0.0f;
      }

      lift = (simulation->unk34 * 10) * (weight * 0.0061035156f);
      if (simulation->unk34 != 0 && airSpeed > 142.2336f) {
        guMtxXFMF(simulation->unk44, relative[AXIS_X], relative[AXIS_Y],
            relative[AXIS_Z], &turned[AXIS_X], &turned[AXIS_Y],
            &turned[AXIS_Z]);
        if (airSpeed < AbsoluteValueExpression(lift) * 200.0f) {
          lift = airSpeed * 0.005f;
        }

        ScaleToLength(lift, &turned[AXIS_X], &turned[AXIS_Y], &turned[AXIS_Z]);
        simulation->velocity[AXIS_X] =
            simulation->velocity[AXIS_X] + turned[AXIS_X];
        simulation->velocity[AXIS_Y] =
            simulation->velocity[AXIS_Y] + turned[AXIS_Y];
        simulation->unk34 = simulation->unk34 * 0.995f;
        simulation->velocity[AXIS_Z] =
            simulation->velocity[AXIS_Z] + turned[AXIS_Z];
      }

      height = simulation->startPosition[AXIS_Y] - simulation->position[AXIS_Y];
      if (height <= 0.0f) {
        windX = simulation->windVelocityX * 0.5f;
        windZ = simulation->windVelocityZ * 0.5f;
      } else {
        f32 scale = (height * 0.00000044499237f) + 0.5f;

        windX = simulation->windVelocityX * scale;
        windZ = simulation->windVelocityZ * scale;
      }

      simulation->velocity[AXIS_X] = simulation->velocity[AXIS_X] +
          ((windX - simulation->velocity[AXIS_X]) * drag);
      simulation->velocity[AXIS_Z] = simulation->velocity[AXIS_Z] +
          ((windZ - simulation->velocity[AXIS_Z]) * drag);
      simulation->velocity[AXIS_Y] = simulation->velocity[AXIS_Y] +
          ((-simulation->velocity[AXIS_Y] * drag) + 262.0f);
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

    {
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
        if (state == BALL_STATE_IN_CUP || simulation->isHoledOut) {
          floor += 0x1200;
        } else if (state == BALL_STATE_ROLLING) {
          s32 cupOffsetX =
              simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X];

          if (Abs(cupOffsetX) < 0xC00) {
            s32 cupOffsetZ =
                simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z];

            if (Abs(cupOffsetZ) < 0xC00) {
              f32 cupDistance = (s32)IntLength2D(cupOffsetX, cupOffsetZ);

              if (cupDistance < 614.4f) {
                floor += 0xC00;
              } else if (cupDistance < 1843.2001f) {
                f32 rimDistance = 1843.2001f - cupDistance;
                f32 rest = 377487.38f - (rimDistance * rimDistance);

                if (rest > 0.0f) {
                  floor = floor + ((614.4f - sqrtf(rest)) * 0.5f);
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
        SurfaceKind kind;
        PlaneTriangle ground;
        TrianglePlane plane;

        simulation->unkA4 = GetSurface(
            simulation->position[AXIS_X], simulation->position[AXIS_Z]);
        kind = GetSurfaceInfo((s8)simulation->unkA4)->kind;
        if (IsDropKind(kind) &&
            FindGroundTriangle(simulation->position[AXIS_X],
                simulation->position[AXIS_Z], &ground) == 0 &&
            GetTrianglePlane(&ground, &plane) == 0 && IsDropSlope(plane)) {
          simulation->dropPosition[AXIS_X] = simulation->position[AXIS_X];
          simulation->dropPosition[AXIS_Y] = simulation->position[AXIS_Y];
          simulation->dropPosition[AXIS_Z] = simulation->position[AXIS_Z];
        }
      }
    }

    simulation->unkA6 = flags;
  } while (++substep < substepCount);

  return flags;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_80206800_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_802068B4_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_802068EC_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_80206910_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_80206968_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_80206A50_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_80206AA4_ovl_05);
#endif

/*
 * A cut-down copy of LaunchBallSimulation. It always takes the club test's
 * entry, picks a lie blend of 0, leaves out the slopes, rain and wind, and
 * prints the launch velocity to the debug output.
 *
 * Doesn't match: GCC schedules the block that sets the spin differently, which
 * moves the unk40 and unk38 stores and swaps two float registers there.
 */
#ifdef NON_MATCHING
s32 func_8020A488_ovl_05(
    BallSimulation* simulation, s32 purpose, ShotLaunch* shot) {
  s32 percentRange[2];
  Club* club;
  ImpactEffect* entry;
  s32 groundHeight;
  s32 speed;
  s32 sideSign;
  s32 rumbles;
  Radians pitch;
  Radians yawRadians;
  f32 blend;
  f32 spinBase;
  f32 spinLoss;
  s32 index;
  s8 surface;

  f32 PickLieBlend(void) {
    return 0.0f;
  }

  simulation->purpose = purpose;
  D_8020BA60_ovl_05++;
  sideSign = 1;
  if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    groundHeight = GetGroundHeight(simulation->position[AXIS_X],
                       simulation->position[AXIS_Z]) -
        1.536f;
    if (groundHeight < simulation->position[AXIS_Y]) {
      simulation->position[AXIS_Y] = groundHeight;
    }
  }

  simulation->dropPosition[AXIS_X] = simulation->position[AXIS_X];
  simulation->startPosition[AXIS_X] = simulation->dropPosition[AXIS_X];
  simulation->dropPosition[AXIS_Y] = simulation->position[AXIS_Y];
  simulation->startPosition[AXIS_Y] = simulation->dropPosition[AXIS_Y];
  simulation->dropPosition[AXIS_Z] = simulation->position[AXIS_Z];
  simulation->startPosition[AXIS_Z] = simulation->dropPosition[AXIS_Z];
  simulation->unkA2 = simulation->surface;
  simulation->unkA7 = simulation->unkA2;
  simulation->unkA1 = 0;
  if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    simulation->playerIndex = shot->playerIndex;
  } else {
    simulation->playerIndex = 0;
  }

  club = &D_8020B9F8_ovl_05[shot->clubIndex];
  entry = &D_8020B86C_ovl_05[0][IMPACT_TIMING_ENTRY_LIMIT];
  rumbles = D_8020B86C_ovl_05[0][IMPACT_TIMING_ENTRY_LIMIT].rumbles;
  if ((club->surfaceMask & CLUB_SURFACE_GREEN) &&
      simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    speed = GetPuttPower(shot->power, shot->shotMode);
  } else {
    speed = GetClubPower(club, shot->shotMode) *
        (shot->power + (entry->speedChange * 2)) / 256;
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

  pitch = (club->pitch + entry->pitchChange) * 0.017453294f;
  if (pitch >= 0.0f) {
    pitch = 0.0f;
  }

  simulation->unk38 =
      (((((sideSign * entry->sideSpinChange) + shot->offsetX) * 2) +
           ((club->sideSpin * 2) * sideSign)) *
          speed) *
      0.00078125f;
  pitch += (f32)club->unk10 * shot->offsetY * 0.00054541545f;
  yawRadians = (u16)shot->heading * BINARY_ANGLE_TO_RADIANS;
  spinBase = club->spin * 0.5f;
  simulation->unk34 =
      (((spinBase + 25.0f) * (shot->offsetY * 0.006f)) - spinBase) * speed *
      0.0078125f;
  simulation->unk40 = 0.0f;
  simulation->unk3C = yawRadians;
  if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
    spinLoss = simulation->unk34 * 0.2f;
    if (spinLoss < 0.0f) {
      spinLoss = -spinLoss;
    }

    speed = speed - spinLoss;
  }

  if (speed < 0) {
    speed = 0;
  }

  if (speed <= 0) {
    speed = 10;
  }

  simulation->velocity[AXIS_Y] = sinf(pitch) * speed;
  simulation->velocity[AXIS_X] = cosf(yawRadians) * speed;
  simulation->velocity[AXIS_Z] = sinf(yawRadians) * speed;
  osSyncPrintf("SVX%f, SVY%f, SVZ%f\n",
      simulation->velocity[AXIS_X] * 0.00007030687f,
      simulation->velocity[AXIS_Y] * 0.00007030687f,
      simulation->velocity[AXIS_Z] * 0.00007030687f);
  if (club->surfaceMask & CLUB_SURFACE_GREEN) {
    simulation->state = BALL_STATE_ROLLING;
    simulation->velocity[AXIS_Y] += PUTT_VELOCITY_Y_OFFSET;
  } else {
    simulation->state = BALL_STATE_FLYING;
  }

  simulation->unk94 = 0;
  simulation->unk98 = 0;
  simulation->unk9C = -2;
  simulation->isHoledOut = 0;
  simulation->hasHitFlag = FALSE;
  for (index = 0; index < UNKA8_COUNT; index++) {
    simulation->unkA8[index] = 0;
  }

  simulation->windVelocityX = 0.0f;
  simulation->windVelocityZ = 0.0f;
  if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
    if (rumbles) {
      StartPlayerRumble(&g_Players[simulation->playerIndex]);
    }

    if (simulation->purpose == LAUNCH_PURPOSE_STROKE &&
        IsGameFlagSet(GAME_FLAG_SHOW_LAUNCH_VALUES)) {
      char text[128];

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
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_8020A474_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_8020A488_ovl_05);
#endif
