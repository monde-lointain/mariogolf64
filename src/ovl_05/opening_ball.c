/*
 * The opening's copies of the ball simulation's routines, with which it
 * simulates its shot, and a routine that loads one of the other characters with
 * its birdie animation.
 */
#include "common.h"

#include "ball_physics.h"
#include "ball_simulation.h"
#include "billboard.h"
#include "character_slot.h"
#include "club.h"
#include "collision.h"
#include "debug_text.h"
#include "effects.h"
#include "game_flags.h"
#include "hole_triangles.h"
#include "libc.h"
#include "math_util.h"
#include "reaction.h"
#include "save.h"
#include "shot_effects.h"
#include "surface.h"
#include "terrain.h"

/* LoadCharacter kinds from the kind count on load one of g_OtherCharacterModels
 */
#define OTHER_CHARACTER_MODEL_KIND CHARACTER_MODEL_KIND_COUNT

/* The clubs that the opening's golfers swing */
#define OPENING_CLUB_COUNT 2

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
/* The opening's copies of f_EarlyImpactEffect and f_LateImpactEffect, which its
 * copy of GetImpactTimingEntry doesn't read */
static ImpactEffect f_OpeningEarlyImpactEffect UNUSED = {
    -90, -64, -40, -10, 100, 1};
static ImpactEffect f_OpeningLateImpactEffect UNUSED = {-80, 64, 30, 10, -5, 1};
/* The opening's copy of f_ImpactEffects */
// clang-format off
static ImpactEffect
    f_OpeningImpactEffects[MAX_LIE_RATING + 1][IMPACT_TIMING_ENTRY_COUNT] = {
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

/* The opening's copy of GetSurfaceInfo, which reads its own surface table */
static SurfaceInfo* GetOpeningSurfaceInfo(Surface surface) {
  if ((u32)surface >= SURFACE_COUNT) {
    surface = SURFACE_OUT_OF_BOUNDS;
  }

  return &D_8020B564_ovl_05[surface];
}

/* An unused copy of ResetBallSimulations, which resets g_BallSimulations as
 * that does */
static UNUSED_FUNCTION void ResetOpeningBallSimulations(void) {
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
 * Where the debug text shows the ball's state, height and speeds; the speeds
 * scroll through SPEED_TEXT_ROW_COUNT rows, blanking the row after the newest
 */
#define STATE_TEXT_COLUMN 2
#define STATE_TEXT_ROW 0xF
#define HEIGHT_TEXT_COLUMN 1
#define HEIGHT_TEXT_ROW 0xF
#define SPEED_TEXT_COLUMN 3
#define SPEED_TEXT_CLEAR_COLUMN 7
#define SPEED_TEXT_ROW_COUNT 8
#define SPEED_TEXT_FIRST_ROW 8
/*
 * The speed display turns speeds into kilometers per hour with this: 3.6
 * kilometers per hour per meter per second over
 * SPEED_UNITS_PER_METER_PER_SECOND, written out to ten places, which rounds it
 * to the float above
 */
#define SPEED_TEXT_KILOMETERS_PER_HOUR_PER_SPEED_UNIT 0.0070312503f

/*
 * Where the opening's copy of StepBallSimulation differs from main's: its
 * lift turns a full quarter turn; it tests only within 20 cm of the cup; a
 * ball that drops in plays the cup-in sound only below 1.75 cm a frame and
 * falling no faster than 5 km/h; its pull's direction tilts four times as
 * much; a ball crossing the cup's edge within 2 cm of its height and slower
 * than 28 cm a frame lips out; a ball in the water stops 5 m down; and its
 * cup's floor bounce is an integer division
 */
#define OPENING_LIFT_ANGLE_DEGREES 90.0f
#define OPENING_CUP_SEARCH_SPAN 0xC00
#define OPENING_MAX_CUP_IN_SOUND_SPEED 268.8f
#define OPENING_MAX_CUP_IN_SOUND_FALL_SPEED KilometersPerHour(5)
#define OPENING_CUP_PULL_DIRECTION_TILT 8384.0f
#define OPENING_MAX_LIP_OUT_HEIGHT 307.2f
#define OPENING_MAX_LIP_OUT_SPEED 4300.8f
#define OPENING_WATER_STOP_DEPTH 0x12C00
/* Its cup floor halves the ball's falling speed in whole units */
#define OPENING_CUP_FLOOR_BOUNCE_DIVISOR 2
/*
 * A billboard halves the ball's speed in whole units, and foliage speeds up
 * a rising ball. A wall puts the ball 4 cm back along its path from the hit,
 * then this share of the way there.
 */
#define OPENING_BILLBOARD_HIT_SPEED_DIVISOR 2
#define OPENING_FOLIAGE_RISE_SCALE 1.2f
#define OPENING_WALL_BACK_OFF 614.4f
#define OPENING_WALL_HIT_STEP_SHARE 0.99f
/*
 * A ball skips off the water only when it hits it faster than this and
 * its speed along the ground, at this ratio, is more than its impact's; a
 * bounce in the first steps keeps a quarter of its speed
 */
#define OPENING_MIN_SKIP_IMPACT_SPEED KilometersPerHour(10)
#define OPENING_SKIP_SPEED_RATIO 0.5f
#define OPENING_EARLY_BOUNCE_STEP_COUNT 3
/*
 * Its ground landing: a ball that lands in the water this fast stops on the
 * ground below. The green's friction grows with the speed from its base; a
 * landing's spin grows from nothing at OPENING_SPINLESS_LANDING_SPEED by one
 * per OPENING_LANDING_SPIN_SPEED_RANGE of the speed into the ground. The spin
 * drifts the ball with less grip, and at most this share of its speed into
 * the ground.
 */
#define OPENING_WATER_STOP_IMPACT_SPEED KilometersPerHour(20)
#define OPENING_GREEN_LANDING_FRICTION_BASE 0.5f
#define OPENING_GREEN_LANDING_FRICTION_PER_SPEED (1.6f / KilometersPerHour(100))
#define OPENING_SPINLESS_LANDING_SPEED KilometersPerHour(10)
#define OPENING_LANDING_SPIN_SPEED_RANGE KilometersPerHour(200)
#define OPENING_GROUND_SPIN_DRIFT_GRIP 0.25f
#define OPENING_MAX_SPIN_DRIFT_SHARE 0.2f
/*
 * Its roll: only on mini golf, a ball pressing into the ground faster than
 * this, between main's two limits, loses its speed into the ground. A slow
 * ball's slope pull grows from a smaller least share than main's, at 1/327.68
 * per unit of speed.
 */
#define OPENING_ROLL_JUMP_SPEED KilometersPerHour(10)
#define OPENING_MIN_SLOW_ROLL_SLOPE_SCALE 0.0625f
#define OPENING_SLOW_ROLL_SLOPE_SCALE_PER_SPEED 0.0030517578f
/*
 * Its flight: the lift, like the curve, is weighted by how near the air
 * speed is to the spin's, at this scale, and falls off by this much per
 * ratio away from it; it is the spin times OPENING_LIFT_SPIN_SCALE times
 * OPENING_LIFT_PER_SPIN, at most 1/OPENING_MAX_LIFT_SPEED_RATIO of the air
 * speed. The spin decays more slowly than main's, and the wind grows with
 * the height half as fast.
 */
#define OPENING_SPIN_SPEED_SCALE 8.0f
#define OPENING_SPIN_MISMATCH_FALLOFF 0.8f
#define OPENING_LIFT_SPIN_SCALE 10
#define OPENING_LIFT_PER_SPIN 0.0061035156f
#define OPENING_MAX_LIFT_SPEED_RATIO 200.0f
#define OPENING_SPIN_DECAY 0.995f
#define OPENING_WIND_SCALE_PER_HEIGHT (1.0f / (160 * UNITS_PER_YARD))

/*
 * The opening calls FindBillboardHit with a ninth argument, the simulation,
 * which FindBillboardHit does not read
 */
typedef s32 (*BillboardHitFinder)(s32 startX, s32 startY, s32 startZ, s32 endX,
    s32 endY, s32 endZ, BillboardCell* cell, f32* fraction,
    BallSimulation* simulation);

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
                NOISE_BYTE_MASK) *
               (1.0f / MAX_NOISE_BYTE_PAIR_SUM)) +
        ((u32)((simulation->position[AXIS_X] - simulation->position[AXIS_Z] +
                   g_Players[simulation->playerIndex].unk88) &
             NOISE_BYTE_MASK) *
            (1.0f / MAX_NOISE_BYTE_PAIR_SUM));
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

  /*
   * Points the lift rotation about the ball\'s spin axis, the level axis across
   * spinHeading, turning a negative spin into a positive one the other way
   */
  void BuildLiftRotation(void) {
    f32 axisX;
    f32 axisZ;

    if (simulation->spinSpeed != 0) {
      axisZ = cosf(simulation->spinHeading) * simulation->spinSpeed;
      axisX = sinf(simulation->spinHeading) * simulation->spinSpeed;
      if (simulation->spinSpeed < 0) {
        simulation->spinHeading += (f32)M_PI;
        simulation->spinSpeed = -simulation->spinSpeed;
      }
    } else {
      axisZ = 1.0f;
      axisX = 0.0f;
    }

    guRotateF(simulation->liftRotation, OPENING_LIFT_ANGLE_DEGREES, -axisX,
        0.0f, axisZ);
  }

  f32 GetPositionNoise(void) {
    return (((simulation->position[AXIS_X] + simulation->position[AXIS_Z]) &
                NOISE_BYTE_MASK) -
               NOISE_BYTE_MIDPOINT) *
        (1.0f / NOISE_BYTE_MIDPOINT);
  }

  char speedText[DEBUG_TEXT_BUFFER_SIZE];
  char heightText[DEBUG_TEXT_BUFFER_SIZE];
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
    PrintDebugPaddedHex(simulation->state, STATE_TEXT_COLUMN, STATE_TEXT_ROW);
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
              SPEED_TEXT_KILOMETERS_PER_HOUR_PER_SPEED_UNIT);
      PrintDebugText(speedText, SPEED_TEXT_COLUMN,
          D_8020B858_ovl_05 + SPEED_TEXT_FIRST_ROW);
      D_8020B858_ovl_05 = (D_8020B858_ovl_05 + 1) % SPEED_TEXT_ROW_COUNT;
      PrintDebugText("                            ", SPEED_TEXT_CLEAR_COLUMN,
          D_8020B858_ovl_05 + SPEED_TEXT_FIRST_ROW);
    }

    if (IsGameFlagSet(GAME_FLAG_SHOW_OPENING_BALL_HEIGHT)) {
      if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
        sprintf(heightText, "Height %fYd   ",
            (simulation->startPosition[AXIS_Y] - simulation->position[AXIS_Y]) *
                YARDS_PER_UNIT);
        PrintDebugText(heightText, HEIGHT_TEXT_COLUMN, HEIGHT_TEXT_ROW);
      }
    }

    if (simulation->state == BALL_STATE_STOPPED) {
      goto SETTLE;
    }

    if (simulation->stepCount == 0) {
      BuildLiftRotation();
    }

    simulation->stepCount++;
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
                simulation->position[AXIS_Y] - MINI_GOLF_ROLL_PROBE_RISE,
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
          if (simulation->lastBounceStep + BILLBOARD_HIT_BOUNCE_GAP <
              simulation->stepCount) {
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
        PrintDebugPaddedHex(
            contact, CONTACT_DEBUG_TEXT_COLUMN, CONTACT_DEBUG_TEXT_ROW);
        osSyncPrintf("col%d\n", contact);
      }
    }

    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
      CoursePoint pinVelocity;
      f32 pinFraction;
      s32 pinHit;

      if (g_GameState.isMiniGolf
              ? g_Players[simulation->playerIndex].strokeCount == 1
              : GetOpeningSurfaceInfo(simulation->unkA2)->kind !=
                  SURFACE_KIND_GREEN) {
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
                PIN_HIT_RISE_SCALE;
            contact = 0;
            simulation->spinHeading =
                atan2(pinVelocity[AXIS_Z], pinVelocity[AXIS_X]);
            simulation->spinSpeed =
                IntLength2D(pinVelocity[AXIS_X], pinVelocity[AXIS_Z]);
          } else {
            simulation->velocity[AXIS_Y] = pinVelocity[AXIS_Y];
            simulation->spinSpeed = simulation->spinSpeed * PIN_HIT_SPIN_KEPT;
          }

          {
            f32 rest = PIN_HIT_STEP_SHARE - pinFraction;

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
          BuildLiftRotation();
        }
      }

      if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        s32 cupOffsetX =
            simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X];

        if (Abs(cupOffsetX) < OPENING_CUP_SEARCH_SPAN) {
          s32 cupOffsetZ =
              simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z];

          if (Abs(cupOffsetZ) < OPENING_CUP_SEARCH_SPAN) {
            s32 cupDistance = IntLength2D(cupOffsetX, cupOffsetZ);

            if (simulation->state != BALL_STATE_IN_CUP &&
                simulation->position[AXIS_Y] -
                        g_HoleSetup.cupPosition[AXIS_Y] >=
                    -MAX_CUP_ENTRY_HEIGHT &&
                (f32)cupDistance < CUP_RADIUS) {
              BallSpeed speed = (s32)IntLength2D(
                  simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]);

              if (speed < MAX_CUP_IN_SPEED &&
                  (f32)simulation->velocity[AXIS_Y] >= -MAX_CUP_IN_RISE_SPEED) {
                if (printsDebugText) {
                  SetDebugTextHighlight(1);
                  PrintDebugText(
                      "CUPIN!", CUP_DEBUG_TEXT_COLUMN, CUP_DEBUG_TEXT_ROW);
                }

                simulation->state = BALL_STATE_IN_CUP;
                if (speed < OPENING_MAX_CUP_IN_SOUND_SPEED &&
                    (f32)simulation->velocity[AXIS_Y] <=
                        OPENING_MAX_CUP_IN_SOUND_FALL_SPEED) {
                  simulation->unkA8[0] = 1;
                }
              }
            }

            if (simulation->state == BALL_STATE_ROLLING &&
                (f32)Abs(simulation->position[AXIS_Y] -
                    g_HoleSetup.cupPosition[AXIS_Y]) < CUP_PULL_MAX_HEIGHT) {
              if ((f32)cupDistance < CUP_PULL_OUTER_RADIUS &&
                  (f32)cupDistance >= CUP_PULL_INNER_RADIUS) {
                f32 toCup[3];
                f32 direction[3];
                f32 alignment;
                f32 depth;
                Radians angle;
                f32 pull;

                isNearCup = TRUE;
                if (printsDebugText) {
                  SetDebugTextHighlight(1);
                  PrintDebugText(
                      "KURU!", CUP_DEBUG_TEXT_COLUMN, CUP_DEBUG_TEXT_ROW);
                }

                toCup[AXIS_Y] = 0.0f;
                toCup[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X] -
                    simulation->position[AXIS_X];
                toCup[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z] -
                    simulation->position[AXIS_Z];
                guNormalize(&toCup[AXIS_X], &toCup[AXIS_Y], &toCup[AXIS_Z]);
                direction[AXIS_X] = simulation->velocity[AXIS_X];
                direction[AXIS_Y] = OPENING_CUP_PULL_DIRECTION_TILT;
                direction[AXIS_Z] = simulation->velocity[AXIS_Z];
                guNormalize(
                    &direction[AXIS_X], &direction[AXIS_Y], &direction[AXIS_Z]);
                alignment = (toCup[AXIS_X] * direction[AXIS_X]) +
                    (toCup[AXIS_Z] * direction[AXIS_Z]);
                if (alignment < 0.0f) {
                  alignment = 0.0f;
                }

                depth = CUP_PULL_OUTER_RADIUS - (f32)cupDistance;
                angle = atan2(toCup[AXIS_Z], toCup[AXIS_X]);
                if (depth > CUP_PULL_DEPTH_RANGE) {
                  depth = CUP_PULL_DEPTH_RANGE;
                }

                pull = ((sinf(depth * ((f32)M_PI / CUP_PULL_DEPTH_RANGE)) *
                            CUP_PULL_RISE) +
                           MIN_CUP_PULL) *
                    (1.0f - alignment);
                simulation->velocity[AXIS_X] =
                    simulation->velocity[AXIS_X] * CUP_PULL_DRAG;
                simulation->velocity[AXIS_Z] =
                    simulation->velocity[AXIS_Z] * CUP_PULL_DRAG;
                simulation->velocity[AXIS_X] =
                    simulation->velocity[AXIS_X] + (cosf(angle) * pull);
                simulation->velocity[AXIS_Z] =
                    simulation->velocity[AXIS_Z] + (sinf(angle) * pull);
              }

              if ((f32)cupDistance > CUP_RADIUS &&
                  (f32)IntLength2D(startX - g_HoleSetup.cupPosition[AXIS_X],
                      startZ - g_HoleSetup.cupPosition[AXIS_Z]) < CUP_RADIUS &&
                  (f32)Abs(startY - g_HoleSetup.cupPosition[AXIS_Y]) <
                      OPENING_MAX_LIP_OUT_HEIGHT) {
                s32 speed = IntLength2D(
                    simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]);

                if ((f32)speed < OPENING_MAX_LIP_OUT_SPEED &&
                    simulation->state == BALL_STATE_ROLLING) {
                  f32 keep = speed * (1.0f / LIP_OUT_FULL_SPEED);
                  BallSpeed fallSpeed;

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
                  simulation->spinHeading = atan2(simulation->velocity[AXIS_Z],
                      simulation->velocity[AXIS_X]);
                  simulation->spinSpeed =
                      IntLength2D(simulation->velocity[AXIS_X],
                          simulation->velocity[AXIS_Z]) *
                      LIP_OUT_SPIN_SCALE;
                  flags |= STEP_FLAG_LIPPED_OUT;
                  contact = 0;
                  BuildLiftRotation();
                }
              }
            }
          }
        }
      }
    }

    if (simulation->state == BALL_STATE_IN_WATER) {
      simulation->hasEnteredWater = TRUE;
      simulation->velocity[AXIS_X] = simulation->velocity[AXIS_X] * WATER_DRAG;
      simulation->velocity[AXIS_Z] = simulation->velocity[AXIS_Z] * WATER_DRAG;
      simulation->velocity[AXIS_Y] =
          (simulation->velocity[AXIS_Y] + (s32)GRAVITY) * WATER_DRAG;
      if (GetGroundOrWaterHeight(
              simulation->position[AXIS_X], simulation->position[AXIS_Z]) +
                  OPENING_WATER_STOP_DEPTH <
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
      if (cupDistance >= CUP_RADIUS) {
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
            CUP_WALL_REFLECTION;
        scale = CUP_WALL_REBOUND_RADIUS / cupDistance;
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
              g_HoleSetup.cupPosition[AXIS_Y] + CUP_CEILING_DEPTH &&
          simulation->velocity[AXIS_Y] < -MIN_CUP_CEILING_SPEED) {
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
              g_HoleSetup.cupPosition[AXIS_Y] + CUP_CEILING_DEPTH;
        }

        simulation->velocity[AXIS_Y] = Abs(simulation->velocity[AXIS_Y]);
      }

      if (simulation->position[AXIS_Y] >=
          g_HoleSetup.cupPosition[AXIS_Y] + CUP_DEPTH) {
        simulation->velocity[AXIS_Y] = (f32)(-simulation->velocity[AXIS_Y] /
                                           OPENING_CUP_FLOOR_BOUNCE_DIVISOR) +
            CUP_GRAVITY;
        /* The chained assignment is there to match */
        simulation->velocity[AXIS_X] = simulation->velocity[AXIS_Z] = 0;
        if (simulation->velocity[AXIS_Y] > 0) {
          simulation->isHoledOut = 1;
          simulation->state = BALL_STATE_STOPPED;
        }

        simulation->position[AXIS_Y] =
            g_HoleSetup.cupPosition[AXIS_Y] + CUP_DEPTH;
      } else {
        simulation->velocity[AXIS_Y] =
            simulation->velocity[AXIS_Y] + CUP_GRAVITY;
      }
    }

    if (contact == STEP_FLAG_BILLBOARD_HIT &&
        simulation->state == BALL_STATE_FLYING) {
      simulation->bounceCount++;
      simulation->spinSpeed /= BILLBOARD_HIT_SPIN_DIVISOR;
      simulation->lastBounceStep = simulation->stepCount;
      simulation->sideSpin /= BILLBOARD_HIT_SPIN_DIVISOR;
      simulation->position[AXIS_X] =
          startX + (fraction * simulation->velocity[AXIS_X]);
      simulation->position[AXIS_Y] =
          startY + (fraction * simulation->velocity[AXIS_Y]);
      simulation->position[AXIS_Z] =
          startZ + (fraction * simulation->velocity[AXIS_Z]);
      switch (triangle.unk0) {
        case BILLBOARD_CELL_TRUNK:
        case BILLBOARD_CELL_SOLID:
          simulation->velocity[AXIS_X] = -simulation->velocity[AXIS_Z] /
              OPENING_BILLBOARD_HIT_SPEED_DIVISOR;
          simulation->velocity[AXIS_Z] = -simulation->velocity[AXIS_X] /
              OPENING_BILLBOARD_HIT_SPEED_DIVISOR;
          break;
        case BILLBOARD_CELL_FOLIAGE:
        default:
          simulation->velocity[AXIS_X] /= OPENING_BILLBOARD_HIT_SPEED_DIVISOR;
          simulation->velocity[AXIS_Z] /= OPENING_BILLBOARD_HIT_SPEED_DIVISOR;
          if (simulation->velocity[AXIS_Y] < 0) {
            simulation->velocity[AXIS_Y] =
                simulation->velocity[AXIS_Y] * OPENING_FOLIAGE_RISE_SCALE;
          }

          break;
      }
    }

    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST &&
        (g_GameState.isMiniGolf || IsGameFlagSet(GAME_FLAG_WALL_HITS)) &&
        FindWallHit(startX, startY, startZ, simulation->position[AXIS_X],
            simulation->position[AXIS_Y], simulation->position[AXIS_Z],
            &D_8022BE10_ovl_05, &D_8022BE28_ovl_05) == TRUE &&
        D_8022BE10_ovl_05.unk0 == HOLE_TRIANGLE_KIND_SHADED) {
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
      D_8022BE28_ovl_05 -= OPENING_WALL_BACK_OFF /
          GetLength3D(simulation->velocity[AXIS_X],
              simulation->velocity[AXIS_Y], simulation->velocity[AXIS_Z]);
      if (D_8022BE28_ovl_05 < 0.0f) {
        D_8022BE28_ovl_05 = 0.0f;
      }

      hitPosition[AXIS_X] = startX +
          (D_8022BE28_ovl_05 *
              (simulation->velocity[AXIS_X] * OPENING_WALL_HIT_STEP_SHARE));
      hitPosition[AXIS_Y] = startY +
          (D_8022BE28_ovl_05 *
              (simulation->velocity[AXIS_Y] * OPENING_WALL_HIT_STEP_SHARE));
      hitPosition[AXIS_Z] = startZ +
          (D_8022BE28_ovl_05 *
              (simulation->velocity[AXIS_Z] * OPENING_WALL_HIT_STEP_SHARE));
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
        simulation->spinSpeed = 0;
        simulation->velocity[AXIS_Z] = reflected[AXIS_Z];
      }
      BuildLiftRotation();
      contact &= ~STEP_FLAG_HOLE_TRIANGLE_HIT;
    }

    if (contact == STEP_FLAG_HOLE_TRIANGLE_HIT &&
        (simulation->state == BALL_STATE_FLYING ||
            simulation->state == BALL_STATE_ROLLING)) {
      TrianglePlane plane;
      s32 bounced[3];
      BallSpeed impactSpeed;
      f32 steepness;
      s32 speed;
      f32 softness;
      f32 bounce;
      f32 push;
      f32 keep;

      simulation->bounceCount++;
      GetTrianglePlane(&triangle, &plane);
      simulation->position[AXIS_X] =
          startX + (fraction * simulation->velocity[AXIS_X]);
      simulation->position[AXIS_Y] =
          startY + (fraction * simulation->velocity[AXIS_Y]);
      simulation->position[AXIS_Z] =
          startZ + (fraction * simulation->velocity[AXIS_Z]);
      substepCount += (fraction < EARLY_HIT_FRACTION) & (substepCount == 1);
      if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
        simulation->surface = SURFACE_FAIRWAY;
      } else {
        simulation->surface = SURFACE_WATER_HAZARD;
        if (simulation->bounceCount == 1) {
          simulation->firstBounceSurface = simulation->surface;
        }

        if (simulation->surface == SURFACE_GREEN ||
            simulation->surface == SURFACE_GREEN_EDGE) {
          plane.normal[AXIS_Y] =
              (plane.normal[AXIS_Y] * (1.0f - GREEN_TRIANGLE_LEVELING)) +
              GREEN_TRIANGLE_LEVELING;
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
        BunkerBurial burial = BUNKER_BURIAL_NONE;

        if (impactSpeed >= POOR_LIE_BUNKER_SPEED) {
          burial = BUNKER_BURIAL_MOSTLY;
        } else if (impactSpeed >= FAIR_LIE_BUNKER_SPEED) {
          burial = BUNKER_BURIAL_HALF;
        }

        switch (burial) {
          case BUNKER_BURIAL_HALF:
            simulation->surface = SURFACE_BUNKER_FAIR_LIE;
            simulation->position[AXIS_Y] = GetGroundHeight(
                simulation->position[AXIS_X], simulation->position[AXIS_Z]);
            simulation->state = BALL_STATE_STOPPED;
            break;
          case BUNKER_BURIAL_MOSTLY:
            simulation->surface = SURFACE_BUNKER_POOR_LIE;
            simulation->position[AXIS_Y] = GetGroundHeight(
                simulation->position[AXIS_X], simulation->position[AXIS_Z]);
            simulation->state = BALL_STATE_STOPPED;
            break;
        }
      } else if (simulation->surface == SURFACE_WATER_HAZARD) {
        if (simulation->state == BALL_STATE_ROLLING ||
            impactSpeed <= OPENING_MIN_SKIP_IMPACT_SPEED ||
            GetLength2D(
                simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]) *
                    OPENING_SKIP_SPEED_RATIO <=
                impactSpeed) {
          simulation->state = BALL_STATE_IN_WATER;
          /* The chained assignment is there to match */
          simulation->velocity[AXIS_X] = simulation->velocity[AXIS_Z] = 0;
        }
      }

      softness = THOUSANDTHS_PER_ONE -
          GetOpeningSurfaceInfo(simulation->surface)->bounciness;
      keep = ((f32)THOUSANDTHS_PER_ONE -
                 (softness * (steepness * SOFTNESS_SPEED_LOSS_SCALE) *
                     steepness)) *
          ONE_PER_THOUSANDTH;
      if (simulation->stepCount < OPENING_EARLY_BOUNCE_STEP_COUNT) {
        keep *= EARLY_BOUNCE_SPEED_SCALE;
      }

      bounce = 1.0f - (softness * steepness * ONE_PER_THOUSANDTH);
      push = (bounce + 1.0f) * impactSpeed;
      bounced[AXIS_X] =
          simulation->velocity[AXIS_X] - (plane.normal[AXIS_X] * push);
      bounced[AXIS_Y] =
          simulation->velocity[AXIS_Y] - (plane.normal[AXIS_Y] * push);
      bounced[AXIS_Z] =
          simulation->velocity[AXIS_Z] - (plane.normal[AXIS_Z] * push);
      simulation->velocity[AXIS_X] = bounced[AXIS_X] * keep;
      simulation->velocity[AXIS_Y] = bounced[AXIS_Y] * keep;
      simulation->lastBounceStep = simulation->stepCount;
      simulation->velocity[AXIS_Z] = bounced[AXIS_Z] * keep;
      if (simulation->state == BALL_STATE_FLYING) {
        s32 spinVelocityX =
            simulation->spinSpeed * cosf(simulation->spinHeading);
        s32 spinVelocityZ =
            simulation->spinSpeed * sinf(simulation->spinHeading);
        f32 grip = 1.0f - bounce;

        simulation->velocity[AXIS_X] = simulation->velocity[AXIS_X] +
            ((spinVelocityX - simulation->velocity[AXIS_X]) * grip *
                AbsoluteValueExpression(plane.normal[AXIS_Y]) *
                TRIANGLE_SPIN_GRIP);
        simulation->velocity[AXIS_Z] = simulation->velocity[AXIS_Z] +
            ((spinVelocityZ - simulation->velocity[AXIS_Z]) * grip *
                AbsoluteValueExpression(plane.normal[AXIS_Y]) *
                TRIANGLE_SPIN_GRIP);
        spinVelocityX = spinVelocityX * BOUNCE_SPIN_DECAY;
        spinVelocityZ = spinVelocityZ * BOUNCE_SPIN_DECAY;
        spinVelocityX = spinVelocityX +
            ((previousVelocityX - spinVelocityX) * TRIANGLE_SPIN_FOLLOW);
        spinVelocityZ = spinVelocityZ +
            ((previousVelocityZ - spinVelocityZ) * TRIANGLE_SPIN_FOLLOW);
        simulation->spinHeading = atan2(spinVelocityZ, spinVelocityX);
        simulation->spinSpeed = IntLength2D(spinVelocityX, spinVelocityZ);
        BuildLiftRotation();
      }

      simulation->sideSpin = 0;
      simulation->velocity[AXIS_Y] += (s32)GRAVITY;
    }

    if (simulation->state == BALL_STATE_FLYING &&
        contact == STEP_FLAG_GROUND_HIT) {
      TrianglePlane plane;
      BallSpeed impactSpeed;
      f32 steepness;
      s32 speed;
      f32 friction;
      f32 spin;
      f32 restitution;

      simulation->bounceCount++;
      GetTrianglePlane(&triangle, &plane);
      GetLength3D(simulation->velocity[AXIS_X], simulation->velocity[AXIS_Y],
          simulation->velocity[AXIS_Z]);
      simulation->position[AXIS_X] =
          startX + (fraction * simulation->velocity[AXIS_X]);
      simulation->position[AXIS_Y] =
          startY + (fraction * simulation->velocity[AXIS_Y]);
      simulation->position[AXIS_Z] =
          startZ + (fraction * simulation->velocity[AXIS_Z]);
      substepCount += (fraction < EARLY_HIT_FRACTION) & (substepCount == 1);
      if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
        simulation->surface = SURFACE_FAIRWAY;
      } else {
        simulation->surface = GetSurface(
            simulation->position[AXIS_X], simulation->position[AXIS_Z]);
        if (simulation->bounceCount == 1) {
          simulation->firstBounceSurface = simulation->surface;
        }

        if (simulation->surface == SURFACE_GREEN ||
            simulation->surface == SURFACE_GREEN_EDGE) {
          plane.normal[AXIS_Y] =
              (plane.normal[AXIS_Y] * (1.0f - GREEN_TRIANGLE_LEVELING)) +
              GREEN_TRIANGLE_LEVELING;
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

      friction = (THOUSANDTHS_PER_ONE -
                     GetOpeningSurfaceInfo(simulation->surface)->bounciness) *
          ONE_PER_THOUSANDTH;
      if (simulation->surface == SURFACE_BUNKER) {
        BunkerBurial burial = BUNKER_BURIAL_NONE;

        if (impactSpeed >= POOR_LIE_BUNKER_SPEED) {
          burial = BUNKER_BURIAL_MOSTLY;
        } else if (impactSpeed >= FAIR_LIE_BUNKER_SPEED) {
          burial = BUNKER_BURIAL_HALF;
        }

        switch (burial) {
          case BUNKER_BURIAL_HALF:
            simulation->surface = SURFACE_BUNKER_FAIR_LIE;
            simulation->position[AXIS_Y] = GetGroundHeight(
                simulation->position[AXIS_X], simulation->position[AXIS_Z]);
            simulation->state = BALL_STATE_STOPPED;
            break;
          case BUNKER_BURIAL_MOSTLY:
            simulation->surface = SURFACE_BUNKER_POOR_LIE;
            simulation->position[AXIS_Y] = GetGroundHeight(
                simulation->position[AXIS_X], simulation->position[AXIS_Z]);
            simulation->state = BALL_STATE_STOPPED;
            break;
        }
      } else if (simulation->surface == SURFACE_WATER_HAZARD) {
        if (impactSpeed >= OPENING_WATER_STOP_IMPACT_SPEED) {
          simulation->state = BALL_STATE_STOPPED;
          simulation->position[AXIS_Y] = GetGroundHeight(
              simulation->position[AXIS_X], simulation->position[AXIS_Z]);
        }
      }

      switch (simulation->surface) {
        case SURFACE_GREEN:
        case SURFACE_GREEN_EDGE:
          friction *=
              ((impactSpeed +
                   (impactSpeed * GetPositionNoise() * LANDING_NOISE_SCALE)) *
                  OPENING_GREEN_LANDING_FRICTION_PER_SPEED) +
              OPENING_GREEN_LANDING_FRICTION_BASE;
          spin = ((impactSpeed * steepness) - OPENING_SPINLESS_LANDING_SPEED) *
              (1.0f / OPENING_LANDING_SPIN_SPEED_RANGE);
          break;
        default:
          friction *= (impactSpeed * OTHER_LANDING_FRICTION_PER_SPEED) +
              LANDING_FRICTION_BASE;
          friction += friction * GetPositionNoise() * LANDING_NOISE_SCALE;
          spin = ((impactSpeed * steepness) - OPENING_SPINLESS_LANDING_SPEED) *
              (1.0f / OPENING_LANDING_SPIN_SPEED_RANGE);
          break;
      }

      if (spin < 0.0f) {
        spin = 0.0f;
      }

      restitution = 1.0f - friction;
      if (restitution < 0.0f) {
        restitution = 0.0f;
      } else if (restitution > MAX_RESTITUTION) {
        restitution = MAX_RESTITUTION;
      }

      if (simulation->state != BALL_STATE_ROLLING &&
          impactSpeed < MAX_ROLL_START_SPEED &&
          simulation->stepCount - 1 == simulation->lastBounceStep) {
        s32 spinVelocityX;
        s32 spinVelocityZ;

        simulation->state = BALL_STATE_ROLLING;
        if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
          simulation->position[AXIS_Y] = 0;
        } else {
          simulation->position[AXIS_Y] = GetGroundHeight(
              simulation->position[AXIS_X], simulation->position[AXIS_Z]);
        }

        spinVelocityX = simulation->spinSpeed * cosf(simulation->spinHeading);
        spinVelocityZ = simulation->spinSpeed * sinf(simulation->spinHeading);
        simulation->velocity[AXIS_X] = simulation->velocity[AXIS_X] +
            ((spinVelocityX - simulation->velocity[AXIS_X]) *
                AbsoluteValueExpression(plane.normal[AXIS_Y]) *
                GROUND_SPIN_GRIP);
        simulation->spinSpeed = 0;
        simulation->velocity[AXIS_Z] = simulation->velocity[AXIS_Z] +
            ((spinVelocityZ - simulation->velocity[AXIS_Z]) *
                AbsoluteValueExpression(plane.normal[AXIS_Y]) *
                GROUND_SPIN_GRIP);
      } else {
        f32 keep = 1.0f - spin;
        s32 bounced[3];
        f32 push;
        s32 spinVelocityX;
        s32 spinVelocityZ;
        f32 grip;
        s32 driftX;
        s32 driftZ;
        f32 driftLimit;
        f32 drift;

        if (keep <= MIN_BOUNCE_SPEED_KEPT) {
          keep = MIN_BOUNCE_SPEED_KEPT;
        }

        if (keep > 1.0f) {
          keep = 1.0f;
        }

        if (simulation->stepCount < EARLY_BOUNCE_STEP_COUNT &&
            !g_GameState.isMiniGolf) {
          keep *= EARLY_BOUNCE_SPEED_SCALE;
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
        simulation->lastBounceStep = simulation->stepCount;
        simulation->velocity[AXIS_Z] = bounced[AXIS_Z] * keep;
        spinVelocityX = simulation->spinSpeed * cosf(simulation->spinHeading);
        spinVelocityZ = simulation->spinSpeed * sinf(simulation->spinHeading);
        grip = 1.0f - restitution;
        driftX = (spinVelocityX - simulation->velocity[AXIS_X]) * grip *
            AbsoluteValueExpression(plane.normal[AXIS_Y]) *
            OPENING_GROUND_SPIN_DRIFT_GRIP;
        driftZ = (spinVelocityZ - simulation->velocity[AXIS_Z]) * grip *
            AbsoluteValueExpression(plane.normal[AXIS_Y]) *
            OPENING_GROUND_SPIN_DRIFT_GRIP;
        driftLimit = impactSpeed * OPENING_MAX_SPIN_DRIFT_SHARE;
        drift = (s32)IntLength2D(driftX, driftZ);
        if (driftLimit < drift && impactSpeed > 1.0f) {
          f32 scale = driftLimit / drift;

          driftX = driftX * scale;
          driftZ = driftZ * scale;
        }

        spinVelocityX = spinVelocityX * BOUNCE_SPIN_DECAY;
        spinVelocityZ = spinVelocityZ * BOUNCE_SPIN_DECAY;
        simulation->velocity[AXIS_X] += driftX;
        simulation->velocity[AXIS_Z] += driftZ;
        spinVelocityX = spinVelocityX +
            ((previousVelocityX - spinVelocityX) * GROUND_SPIN_FOLLOW);
        spinVelocityZ = spinVelocityZ +
            ((previousVelocityZ - spinVelocityZ) * GROUND_SPIN_FOLLOW);
        simulation->spinHeading = atan2(spinVelocityZ, spinVelocityX);
        simulation->spinSpeed = IntLength2D(spinVelocityX, spinVelocityZ);
        BuildLiftRotation();
        simulation->sideSpin = 0;
        simulation->velocity[AXIS_Y] += (s32)GRAVITY;
      }
    }

    if (simulation->state == BALL_STATE_ROLLING && contact == 0) {
      simulation->state = BALL_STATE_FLYING;
      simulation->spinHeading =
          atan2(simulation->velocity[AXIS_Z], simulation->velocity[AXIS_X]);
      simulation->spinSpeed = IntLength2D(
          simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]);
      BuildLiftRotation();
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
          BallSpeed pressing =
              (plane.normal[AXIS_X] * simulation->velocity[AXIS_X]) +
              (plane.normal[AXIS_Y] * simulation->velocity[AXIS_Y]) +
              (plane.normal[AXIS_Z] * simulation->velocity[AXIS_Z]);

          if (pressing >= OPENING_ROLL_JUMP_SPEED) {
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

              if (lie < FAIRWAY_POOR_LIE_CHANCE) {
                simulation->surface = SURFACE_FAIRWAY_POOR_LIE;
              } else if (lie <
                  FAIRWAY_POOR_LIE_CHANCE + FAIRWAY_FAIR_LIE_CHANCE) {
                simulation->surface = SURFACE_FAIRWAY_FAIR_LIE;
              }
            }

            break;
          case SURFACE_ROUGH:
            if (GetStepNoise() < ROUGH_POOR_LIE_CHANCE) {
              simulation->surface = SURFACE_ROUGH_POOR_LIE;
            }

            break;
        }

        info = GetOpeningSurfaceInfo(simulation->surface);
        if (info->kind == SURFACE_KIND_OUT_OF_COURSE) {
          simulation->state = BALL_STATE_STOPPED;
          goto SETTLE;
        }

        groundHeight = GetGroundHeight(
            simulation->position[AXIS_X], simulation->position[AXIS_Z]);
        if (simulation->position[AXIS_Y] + TAKE_OFF_HEIGHT < groundHeight) {
          simulation->state = BALL_STATE_FLYING;
          simulation->spinHeading =
              atan2(simulation->velocity[AXIS_Z], simulation->velocity[AXIS_X]);
          contact = 0;
          simulation->spinSpeed = IntLength2D(
              simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]);
          BuildLiftRotation();
          goto ROLLED;
        }
      }

      slopeX = (-plane.normal[AXIS_X] * GRAVITY) /
          AbsoluteValueExpression(plane.normal[AXIS_Y]);
      slopeZ = (-plane.normal[AXIS_Z] * GRAVITY) /
          AbsoluteValueExpression(plane.normal[AXIS_Y]);
      if (plane.normal[AXIS_Y] * plane.normal[AXIS_Y] <=
              (plane.normal[AXIS_X] * plane.normal[AXIS_X]) +
                  (plane.normal[AXIS_Z] * plane.normal[AXIS_Z]) &&
          simulation->stepCount < STEEP_SLOPE_STEP_LIMIT) {
        isSteep = TRUE;
      }

      friction = info->rollSpeedKept * ONE_PER_THOUSANDTH;
      if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST &&
          g_HoleSetup.conditions.isRaining == TRUE) {
        friction *= RAIN_FRICTION_SCALE;
      }

      if (isSteep) {
        slopeX *= STEEP_SLOPE_SCALE;
        slopeZ *= STEEP_SLOPE_SCALE;
        if (friction < MIN_STEEP_FRICTION) {
          friction = MIN_STEEP_FRICTION;
        }
      } else {
        if ((f32)rollSpeed < SLOW_ROLL_SPEED) {
          f32 scale = (rollSpeed * OPENING_SLOW_ROLL_SLOPE_SCALE_PER_SPEED) +
              OPENING_MIN_SLOW_ROLL_SLOPE_SCALE;

          slopeX = slopeX * scale;
          slopeZ = slopeZ * scale;
        }

        if (simulation->stepCount >= LONGEST_ROLL_STEP_COUNT) {
          friction *= LONGEST_ROLL_FRICTION_SCALE;
        } else if (simulation->stepCount >= LONGER_ROLL_STEP_COUNT) {
          friction *= LONGER_ROLL_FRICTION_SCALE;
        } else if (simulation->stepCount >= LONG_ROLL_STEP_COUNT) {
          friction *= LONG_ROLL_FRICTION_SCALE;
        }
      }

      simulation->velocity[AXIS_X] = (slopeX * friction * SLOPE_PULL_SCALE) +
          (simulation->velocity[AXIS_X] * friction);
      simulation->velocity[AXIS_Z] = (slopeZ * friction * SLOPE_PULL_SCALE) +
          (simulation->velocity[AXIS_Z] * friction);
      if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST &&
          g_GameState.isMiniGolf) {
        if (groundHeight - startY < simulation->velocity[AXIS_Y]) {
          simulation->velocity[AXIS_Y] =
              groundHeight - startY + (s32)MINI_GOLF_ROLL_GRAVITY;
        } else {
          simulation->velocity[AXIS_Y] += (s32)MINI_GOLF_ROLL_GRAVITY;
        }
      } else {
        if (groundHeight - startY < simulation->velocity[AXIS_Y]) {
          simulation->velocity[AXIS_Y] = groundHeight - startY + (s32)GRAVITY;
        } else {
          simulation->velocity[AXIS_Y] += (s32)GRAVITY;
        }
      }

      if (simulation->purpose == LAUNCH_PURPOSE_STROKE &&
          IsGameFlagSet(GAME_FLAG_LOG_VERTICAL_VELOCITY)) {
        osSyncPrintf("V[1] %d\n", simulation->velocity[AXIS_Y]);
      }

      if (IntLength2D(simulation->velocity[AXIS_X],
              simulation->velocity[AXIS_Z]) < MAX_STOP_SPEED) {
        if (!isSteep &&
            IntLength2D(slopeX, slopeZ) * friction < MAX_STOP_SLOPE_PULL) {
          simulation->state = BALL_STATE_STOPPED;
        }
      }

      if (!isSteep && info->kind != SURFACE_KIND_GREEN &&
          simulation->lastBounceStep + OFF_GREEN_STOP_STEPS <
              simulation->stepCount &&
          GetLength3D(simulation->velocity[AXIS_X],
              simulation->velocity[AXIS_Y],
              simulation->velocity[AXIS_Z]) < (f64)MAX_OFF_GREEN_STOP_SPEED) {
        simulation->state = BALL_STATE_STOPPED;
      }
    }

ROLLED:

    if ((simulation->state == BALL_STATE_FLYING ||
            simulation->state == BALL_STATE_ROLLING) &&
        contact == 0) {
      f32 relative[3];
      f32 turned[3];
      BallSpeed airSpeed;
      f32 drag;
      f32 sideSpin;
      BallSpeed horizontalSpeed;
      f32 sideSpinMagnitude;
      f32 ratio;
      f32 weight;
      f32 curve;
      f32 spinMagnitude;
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
      drag =
          (sqrtf(airSpeed * AIR_DRAG_SPEED_SCALE) * AIR_DRAG_PER_ROOT_SPEED) +
          MIN_AIR_DRAG;
      sideSpin = simulation->sideSpin;
      sideSpinMagnitude =
          AbsoluteValueExpression(sideSpin * SIDE_SPIN_SPEED_SCALE);
      horizontalSpeed = GetLength2D(relative[AXIS_X], relative[AXIS_Z]);
      ratio = 0.0f;
      if (sideSpinMagnitude != 0.0f) {
        ratio = horizontalSpeed / sideSpinMagnitude;
      }

      weight = 1.0f -
          (AbsoluteValueExpression(ratio - 1.0f) * SIDE_SPIN_MISMATCH_FALLOFF);
      if (weight < 0.0f) {
        weight = 0.0f;
      }

      curve = sideSpin * (weight * CURVE_PER_SIDE_SPIN);
      if (horizontalSpeed <
          AbsoluteValueExpression(curve) * MAX_CURVE_SPEED_RATIO) {
        curve = horizontalSpeed * (1.0f / MAX_CURVE_SPEED_RATIO);
        if (simulation->sideSpin < 0) {
          curve = -curve;
        }
      }

      if (horizontalSpeed > MIN_CURVE_SPEED) {
        f32 scale = (1.0f / horizontalSpeed) * curve;

        simulation->velocity[AXIS_X] =
            simulation->velocity[AXIS_X] + (-relative[AXIS_Z] * scale);
        simulation->velocity[AXIS_Z] =
            simulation->velocity[AXIS_Z] + (relative[AXIS_X] * scale);
      }

      simulation->sideSpin = simulation->sideSpin * SIDE_SPIN_DECAY;
      spinMagnitude = AbsoluteValueExpression(
          simulation->spinSpeed * OPENING_SPIN_SPEED_SCALE);
      ratio = 0.0f;
      if (spinMagnitude != 0.0f) {
        ratio = airSpeed / spinMagnitude;
      }

      weight = 1.0f -
          (AbsoluteValueExpression(ratio - 1.0f) *
              OPENING_SPIN_MISMATCH_FALLOFF);
      if (weight < 0.0f) {
        weight = 0.0f;
      }

      lift = (simulation->spinSpeed * OPENING_LIFT_SPIN_SCALE) *
          (weight * OPENING_LIFT_PER_SPIN);
      if (simulation->spinSpeed != 0 && airSpeed > MIN_LIFT_AIR_SPEED) {
        guMtxXFMF(simulation->liftRotation, relative[AXIS_X], relative[AXIS_Y],
            relative[AXIS_Z], &turned[AXIS_X], &turned[AXIS_Y],
            &turned[AXIS_Z]);
        if (airSpeed <
            AbsoluteValueExpression(lift) * OPENING_MAX_LIFT_SPEED_RATIO) {
          lift = airSpeed * (1.0f / OPENING_MAX_LIFT_SPEED_RATIO);
        }

        ScaleToLength(lift, &turned[AXIS_X], &turned[AXIS_Y], &turned[AXIS_Z]);
        simulation->velocity[AXIS_X] =
            simulation->velocity[AXIS_X] + turned[AXIS_X];
        simulation->velocity[AXIS_Y] =
            simulation->velocity[AXIS_Y] + turned[AXIS_Y];
        simulation->spinSpeed = simulation->spinSpeed * OPENING_SPIN_DECAY;
        simulation->velocity[AXIS_Z] =
            simulation->velocity[AXIS_Z] + turned[AXIS_Z];
      }

      height = simulation->startPosition[AXIS_Y] - simulation->position[AXIS_Y];
      if (height <= 0.0f) {
        windX = simulation->windVelocityX * LOW_WIND_SCALE;
        windZ = simulation->windVelocityZ * LOW_WIND_SCALE;
      } else {
        f32 scale = (height * OPENING_WIND_SCALE_PER_HEIGHT) + LOW_WIND_SCALE;

        windX = simulation->windVelocityX * scale;
        windZ = simulation->windVelocityZ * scale;
      }

      simulation->velocity[AXIS_X] = simulation->velocity[AXIS_X] +
          ((windX - simulation->velocity[AXIS_X]) * drag);
      simulation->velocity[AXIS_Z] = simulation->velocity[AXIS_Z] +
          ((windZ - simulation->velocity[AXIS_Z]) * drag);
      simulation->velocity[AXIS_Y] = simulation->velocity[AXIS_Y] +
          ((-simulation->velocity[AXIS_Y] * drag) + GRAVITY);
    }

    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
      Boolean isOutside =
          simulation->position[AXIS_Z] > GROUND_LENGTH - COURSE_EDGE_MARGIN;

      if (simulation->position[AXIS_Z] < COURSE_EDGE_MARGIN) {
        isOutside = TRUE;
      }

      if (simulation->position[AXIS_X] > GROUND_WIDTH - COURSE_EDGE_MARGIN) {
        isOutside = TRUE;
      }

      if (simulation->position[AXIS_X] < COURSE_EDGE_MARGIN) {
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
          floor += IN_CUP_FLOOR_DEPTH;
        } else if (state == BALL_STATE_ROLLING) {
          s32 cupOffsetX =
              simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X];

          if (Abs(cupOffsetX) < CUP_FLOOR_SEARCH_SPAN) {
            s32 cupOffsetZ =
                simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z];

            if (Abs(cupOffsetZ) < CUP_FLOOR_SEARCH_SPAN) {
              f32 cupDistance = (s32)IntLength2D(cupOffsetX, cupOffsetZ);

              if (cupDistance < CUP_DROP_RADIUS) {
                floor += CUP_DEPTH;
              } else if (cupDistance < CUP_RIM_OUTER_RADIUS) {
                f32 rimDistance = CUP_RIM_OUTER_RADIUS - cupDistance;
                f32 rest = (CUP_RIM_CURVE_RADIUS * CUP_RIM_CURVE_RADIUS) -
                    (rimDistance * rimDistance);

                if (rest > 0.0f) {
                  floor = floor +
                      ((CUP_RIM_CURVE_RADIUS - sqrtf(rest)) *
                          CUP_RIM_SINK_SCALE);
                }
              }
            }
          }
        }
      }

      if (floor < simulation->position[AXIS_Y]) {
        simulation->position[AXIS_Y] = floor;
      }

      if (simulation->position[AXIS_Y] < -MAX_BALL_ALTITUDE) {
        simulation->position[AXIS_Y] = -MAX_BALL_ALTITUDE;
      }

      if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        SurfaceKind kind;
        PlaneTriangle ground;
        TrianglePlane plane;

        simulation->unkA4 = GetSurface(
            simulation->position[AXIS_X], simulation->position[AXIS_Z]);
        kind = GetOpeningSurfaceInfo((s8)simulation->unkA4)->kind;
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
 * The opening's launch doubles the impact's and the aim's side spin, as it
 * does the club's, and a ball hit by a club that can't putt loses this share
 * of its spin from its speed
 */
#define OPENING_OFFSET_SIDE_SPIN_SCALE 2
#define OPENING_LAUNCH_SPIN_SPEED_LOSS 0.2f

/*
 * A cut-down copy of LaunchBallSimulation. It always takes the club test's
 * entry, picks a lie blend of 0, leaves out the slopes, rain and wind, and
 * prints the launch velocity to the debug output.
 *
 * Doesn't match: GCC schedules the block that sets the spin differently, which
 * moves the unk40 and sideSpin stores and swaps two float registers there.
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
  s8 surface UNUSED;

  f32 PickLieBlend(void) {
    return 0.0f;
  }

  simulation->purpose = purpose;
  D_8020BA60_ovl_05++;
  sideSign = 1;
  if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    groundHeight = GetGroundHeight(simulation->position[AXIS_X],
                       simulation->position[AXIS_Z]) -
        LAUNCH_GROUND_CLEARANCE;
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
  simulation->firstBounceSurface = simulation->unkA2;
  simulation->hasEnteredWater = 0;
  if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    simulation->playerIndex = shot->playerIndex;
  } else {
    simulation->playerIndex = 0;
  }

  club = &D_8020B9F8_ovl_05[shot->clubIndex];
  entry = &f_OpeningImpactEffects[0][IMPACT_TIMING_ENTRY_LIMIT];
  rumbles = f_OpeningImpactEffects[0][IMPACT_TIMING_ENTRY_LIMIT].rumbles;
  if ((club->surfaceMask & CLUB_SURFACE_GREEN) &&
      simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    speed = GetPuttPower(shot->power, shot->shotMode);
  } else {
    speed = GetClubPower(club, shot->shotMode) *
        (shot->power +
            (entry->speedChange * (LAUNCH_SCALE_ONE / SPEED_CHANGE_UNITS))) /
        MAX_LAUNCH_POWER;
    if (speed <= 0) {
      speed = MIN_LAUNCH_SPEED;
    }
  }

  if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    SurfaceInfo* info = GetOpeningSurfaceInfo(simulation->unkA2);

    blend = PickLieBlend();
    GetSurfacePercentRange(info, club, percentRange);
    speed = ((percentRange[0] * blend) + (percentRange[1] * (1.0f - blend))) *
        speed * ONE_PER_PERCENT;
  } else {
    s32 percent =
        CLUB_TEST_SPEED_HALF_PERCENTS; /* The local is there to match */

    speed = speed * percent / HALF_PERCENTS_PER_ONE;
  }

  pitch = (club->pitch + entry->pitchChange) * DEGREES_TO_RADIANS;
  if (pitch >= 0.0f) {
    pitch = 0.0f;
  }

  simulation->sideSpin =
      (((((sideSign * entry->sideSpinChange) + shot->offsetX) *
            OPENING_OFFSET_SIDE_SPIN_SCALE) +
           ((club->sideSpin * CLUB_SIDE_SPIN_SCALE) * sideSign)) *
          speed) *
      SIDE_SPIN_PER_SPEED;
  pitch += (f32)club->offsetPitch * shot->offsetY *
      (DEGREES_TO_RADIANS / OFFSET_Y_PER_PITCH_STEP);
  yawRadians = (u16)shot->heading * BINARY_ANGLE_TO_RADIANS;
  spinBase = club->spin * CLUB_SPIN_BASE_SCALE;
  simulation->spinSpeed = (((spinBase + CLUB_TEST_SPIN_OFFSET) *
                               (shot->offsetY * CLUB_TEST_SPIN_PER_OFFSET)) -
                              spinBase) *
      speed * SPIN_PER_SPEED;
  simulation->unk40 = 0.0f;
  simulation->spinHeading = yawRadians;
  if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
    spinLoss = simulation->spinSpeed * OPENING_LAUNCH_SPIN_SPEED_LOSS;
    if (spinLoss < 0.0f) {
      spinLoss = -spinLoss;
    }

    speed = speed - spinLoss;
  }

  if (speed < 0) {
    speed = 0;
  }

  if (speed <= 0) {
    speed = MIN_LAUNCH_SPEED;
  }

  simulation->velocity[AXIS_Y] = sinf(pitch) * speed;
  simulation->velocity[AXIS_X] = cosf(yawRadians) * speed;
  simulation->velocity[AXIS_Z] = sinf(yawRadians) * speed;
  osSyncPrintf("SVX%f, SVY%f, SVZ%f\n",
      simulation->velocity[AXIS_X] * (1.0f / KilometersPerHour(100)),
      simulation->velocity[AXIS_Y] * (1.0f / KilometersPerHour(100)),
      simulation->velocity[AXIS_Z] * (1.0f / KilometersPerHour(100)));
  if (club->surfaceMask & CLUB_SURFACE_GREEN) {
    simulation->state = BALL_STATE_ROLLING;
    simulation->velocity[AXIS_Y] += PUTT_VELOCITY_Y_OFFSET;
  } else {
    simulation->state = BALL_STATE_FLYING;
  }

  simulation->bounceCount = 0;
  simulation->stepCount = 0;
  simulation->lastBounceStep = NO_BOUNCE_STEP;
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
      char text[DEBUG_TEXT_BUFFER_SIZE];

      sprintf(text, "ANGLEY = %3.3d/360",
          (s32)(yawRadians * (f32)RADIANS_TO_DEGREES));
      PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_YAW);
      sprintf(
          text, "ANGLEX = %3.3d/360", (s32)(pitch * (f32)RADIANS_TO_DEGREES));
      PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_PITCH);
      sprintf(text, "Velocity = %fm/s     ",
          speed * (1.0f / SPEED_UNITS_PER_METER_PER_SECOND));
      PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SPEED);
      sprintf(text, "SpinY = %d", simulation->sideSpin);
      PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SIDE_SPIN);
      sprintf(text, "SpinX = %d", simulation->spinSpeed);
      PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SPIN);
      sprintf(text, "SpinVel = %fRPM      ",
          simulation->spinSpeed * RPM_PER_SPIN_SPEED_UNIT);
      PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SPIN_RPM);
      sprintf(text, "SpinAngle = %3.3d/360",
          (s32)(simulation->spinHeading * (f32)RADIANS_TO_DEGREES));
      PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SPIN_HEADING);
      sprintf(text, "SpinVelX = %fkm/h      ",
          simulation->spinSpeed * KILOMETERS_PER_HOUR_PER_SPEED_UNIT);
      PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SPIN_SPEED);
      sprintf(text, "SpinVelY = %fkm/h      ",
          simulation->sideSpin * KILOMETERS_PER_HOUR_PER_SPEED_UNIT);
      PrintDebugText(
          text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SIDE_SPIN_SPEED);
    }
  }

  return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_8020A474_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", func_8020A488_ovl_05);
#endif
