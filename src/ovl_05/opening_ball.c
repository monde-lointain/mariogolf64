/*
 * The opening's copies of the ball simulation's routines, with which it
 * simulates its shot, and a routine that loads one of the other characters with
 * its birdie animation.
 */
#include "opening_ball.h"

#include <math.h>
#include <stdlib.h>
#include <ultra64.h>

#include "asset.h"
#include "ball_physics.h"
#include "ball_simulation.h"
#include "character_slot.h"
#include "club.h"
#include "collision.h"
#include "compiler.h"
#include "course_space.h"
#include "debug_text.h"
#include "effects.h"
#include "game_flags.h"
#include "game_state.h"
#include "geometry.h"
#include "hole_triangles.h"
#include "include_asm.h"
#include "language.h"
#include "math_util.h"
#include "player.h"
#include "reaction.h"
#include "shot_effects.h"
#include "surface.h"
#include "terrain.h"

/* LoadCharacter kinds from the kind count on load one of g_OtherCharacterModels
 */
#define OTHER_CHARACTER_MODEL_KIND CHARACTER_MODEL_KIND_COUNT

/* The clubs that the opening's golfers swing */
#define OPENING_CLUB_COUNT 2

/* The opening's copy of g_ClubTestMode */
static s8 f_OpeningClubTestMode = CLUB_TEST_NONE;

/* The opening's own copy of f_SurfaceInfo, which names the surfaces in
 * half-width katakana and differs from it in eight entries' values */
static SurfaceInfo f_OpeningSurfaceInfo[SURFACE_COUNT] = {
    /* SURFACE_OUT_OF_BOUNDS */
    {SURFACE_KIND_OUT_OF_BOUNDS, "O.B.", 100, 100, 0, 0, 0, 0x500,
        SINK_DEPTH_HALF, 7},
    /* SURFACE_FAIRWAY */
    {SURFACE_KIND_FAIRWAY, "ﾌｪｱｳｪｲ", 98, 100, 5, 960, 400, 0x501,
        SINK_DEPTH_NONE, 0},
    /* SURFACE_ROUGH */
    {SURFACE_KIND_ROUGH, "ﾗﾌ", 80, 90, 30, 600, 200, 0x508, SINK_DEPTH_HALF, 1},
    /* SURFACE_BUNKER */
    {SURFACE_KIND_BUNKER, "ﾊﾞﾝｶｰ", 70, 85, 30, 600, 0, 0x504, SINK_DEPTH_NONE,
        2},
    /* SURFACE_GREEN */
    {SURFACE_KIND_GREEN, "ｵﾝ ｸﾞﾘｰﾝ", 100, 100, 0, 972, 500, 0x50A,
        SINK_DEPTH_NONE, 6},
    /* SURFACE_WATER_HAZARD */
    {SURFACE_KIND_WATER_HAZARD, "ｳｫｰﾀｰﾊｻﾞｰﾄﾞ", 100, 100, 0, 0, 0, 0x50C,
        SINK_DEPTH_BURIED, 3},
    /* SURFACE_CART_WAY */
    {SURFACE_KIND_CART_WAY, "ｶｰﾄｳｪｲ", 80, 100, 10, 975, 890, 0x50B,
        SINK_DEPTH_NONE, 4},
    /* SURFACE_TEE_GROUND */
    {SURFACE_KIND_TEE_GROUND, "ﾃｨｰｸﾞﾗｳﾝﾄﾞ", 98, 100, 10, 930, 500, 0x501,
        SINK_DEPTH_NONE, 8},
    /* SURFACE_ROCK */
    {SURFACE_KIND_ROCK, "ﾛｯｸ", 80, 90, 30, 930, 900, 0x50D, SINK_DEPTH_NONE, 9},
    /* SURFACE_GREEN_EDGE */
    {SURFACE_KIND_GREEN_EDGE, "ｸﾞﾘｰﾝ ｴｯｼﾞ", 98, 100, 10, 966, 400, 0x501,
        SINK_DEPTH_NONE, 0},
    /* SURFACE_SEMI_ROUGH */
    {SURFACE_KIND_SEMI_ROUGH, "ｾﾐﾗﾌ", 90, 98, 30, 930, 500, 0x507,
        SINK_DEPTH_NONE, 0},
    /* SURFACE_BARE_GROUND */
    {SURFACE_KIND_BARE_GROUND, "ﾍﾞｱｸﾞﾗｳﾝﾄﾞ", 80, 90, 30, 930, 500, 0x50C,
        SINK_DEPTH_NONE, 1},
    /* SURFACE_DEEP_ROUGH */
    {SURFACE_KIND_DEEP_ROUGH, "ﾃﾞｨｰﾌﾟﾗﾌ", 50, 80, 30, 430, 100, 0x509,
        SINK_DEPTH_BURIED, 5},
    /* SURFACE_NO_DROP_GROUND */
    {SURFACE_KIND_NO_DROP_GROUND, "ﾍﾞｱｸﾞﾗｳﾝﾄﾞ", 80, 90, 30, 930, 500, 0x50C,
        SINK_DEPTH_NONE, 1},
    /* SURFACE_WASTE_AREA */
    {SURFACE_KIND_WASTE_AREA, "ｳｴｽﾄｴﾘｱ", 50, 80, 30, 430, 100, 0x8F1,
        SINK_DEPTH_HALF, 5},
    /* SURFACE_UNNAMED_ROCK */
    {SURFACE_KIND_ROCK, "            ", 98, 100, 40, 930, 500, 0x500,
        SINK_DEPTH_NONE, 9},
    /* SURFACE_BUNKER_FAIR_LIE */
    {SURFACE_KIND_BUNKER_FAIR_LIE, "ﾊﾞﾝｶｰ", 50, 70, 30, 600, 0, 0x505,
        SINK_DEPTH_HALF, 2},
    /* SURFACE_BUNKER_POOR_LIE */
    {SURFACE_KIND_BUNKER_POOR_LIE, "ﾊﾞﾝｶｰ", 40, 60, 30, 600, 0, 0x506,
        SINK_DEPTH_HALF, 2},
    /* SURFACE_BUNKER_UNUSED_LIE */
    {SURFACE_KIND_BUNKER_UNUSED_LIE, "ﾊﾞﾝｶｰ", 40, 60, 40, 600, 0, 0x506,
        SINK_DEPTH_HALF, 2},
    /* SURFACE_FAIRWAY_FAIR_LIE */
    {SURFACE_KIND_FAIRWAY_FAIR_LIE, "ﾌｪｱｳｪｲ", 94, 98, 7, 950, 380, 0x502,
        SINK_DEPTH_NONE, 0},
    /* SURFACE_FAIRWAY_POOR_LIE */
    {SURFACE_KIND_FAIRWAY_POOR_LIE, "ﾌｪｱｳｪｲ", 90, 94, 7, 950, 380, 0x503,
        SINK_DEPTH_NONE, 0},
    /* SURFACE_ROUGH_POOR_LIE */
    {SURFACE_KIND_ROUGH_POOR_LIE, "ﾗﾌ", 70, 85, 30, 600, 200, 0x509,
        SINK_DEPTH_HALF, 5},
    /* SURFACE_OUT_OF_COURSE */
    {SURFACE_KIND_OUT_OF_COURSE, "ｱｳﾄ ｵﾌﾞ ｺｰｽ", 100, 100, 0, 0, 0, 0x500,
        SINK_DEPTH_BURIED, 7},
    /* SURFACE_TEE_UP */
    {SURFACE_KIND_TEE_UP, "TEE UP", 98, 100, 10, 930, 500, 0x500,
        SINK_DEPTH_NONE, 8},
    /* SURFACE_MINI_GOLF_GREEN */
    {SURFACE_KIND_GREEN, "ｸﾞﾘｰﾝ", 100, 100, 0, 972, 500, 0x8F2, SINK_DEPTH_NONE,
        6},
    /* SURFACE_MINI_GOLF_FAIRWAY */
    {SURFACE_KIND_FAIRWAY, "ﾌﾞﾙｰ", 98, 100, 5, 960, 400, 0x8F3, SINK_DEPTH_NONE,
        0},
    /* SURFACE_MINI_GOLF_CART_WAY */
    {SURFACE_KIND_CART_WAY, "ﾚｯﾄﾞ", 80, 100, 10, 980, 890, 0x8F4,
        SINK_DEPTH_NONE, 4}};

/* The debug text row, of SPEED_TEXT_ROW_COUNT, that the next ball speed goes in
 */
static s32 f_OpeningSpeedTextRow = 0;
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
static Club f_OpeningClubs[OPENING_CLUB_COUNT] = {
    {0, "1W", DRIVER_SURFACES, 208, -15, 0, 11, 120, CLUB_TYPE_DRIVER,
        {97608, 84651, 19335}, CLUB_KIND_WOOD, 0, {0, 0, 0},
        ASSET_DRIVER_HEAD_IMAGE, 0},
    {0, "1W", DRIVER_SURFACES, 208, -23, 0, 11, 120, CLUB_TYPE_DRIVER,
        {97608, 84651, 19335}, CLUB_KIND_WOOD, 0, {0, 0, 0},
        ASSET_DRIVER_HEAD_IMAGE, 0}};

/* LaunchOpeningBallSimulation counts each launch here, and nothing else reads
 * it */
static s32 f_UnusedOpeningLaunchCount = 0;

/* The opening's copies of the collision triangle and fraction that
 * StepBallSimulation keeps on its stack */
static PlaneTriangle f_OpeningContactTriangle;
static f32 f_OpeningContactFraction;

/* Loads one of the other characters into the slot with its birdie animation, or
 * frees a busy slot first */
s32 ToggleOpeningCharacter(u32 slotIndex, s32 character) {
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

  return &f_OpeningSurfaceInfo[surface];
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
    g_BallSimulations[index].unusedResetValue = 0;
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
 * The original calls FindBillboardHit with a ninth argument, the simulation,
 * which FindBillboardHit does not read. The cast goes through a function of
 * no arguments, which GCC takes as meant. Calling through it is undefined,
 * so otherwise the simulation is left out.
 */
#ifndef AVOID_UB
typedef s32 (*BillboardHitFinder)(s32 startX, s32 startY, s32 startZ, s32 endX,
    s32 endY, s32 endZ, BillboardCell* cell, f32* fraction,
    BallSimulation* simulation);
#define FindOpeningBillboardHit( \
    startX, startY, startZ, endX, endY, endZ, cell, fraction, simulation) \
  ((BillboardHitFinder)(void (*)(void))FindBillboardHit)( \
      startX, startY, startZ, endX, endY, endZ, cell, fraction, simulation)
#else
#define FindOpeningBillboardHit( \
    startX, startY, startZ, endX, endY, endZ, cell, fraction, simulation) \
  FindBillboardHit(startX, startY, startZ, endX, endY, endZ, cell, fraction)
#endif

/*
 * The opening's copy of StepBallSimulation, which moves the ball through one
 * step. Unlike main's copy it works on the simulation's integer velocity
 * rather than a float copy, plays no sounds, starts no rumble or Cheep Cheep
 * leap, and can print the ball's speed, height and collisions as debug text.
 * The sparks it starts on a stroke's pin and wall hits are called at ovl_00's
 * addresses, which lie in the opening's BSS while it is loaded. Returns the
 * STEP_FLAG_* bits for the step.
 *
 * Doesn't match: the nested helpers match. The substep loop's hoisted
 * 0.5f now sits in $f30 as in the original; of the 552 differing lines,
 * 216 use floating-point registers. On an older base, forcing the
 * original's registers on 61 pseudos left 632 words at best, so the code
 * differs too. Reading the side spin before the drag, and adding the
 * velocity to the spin's push rather than the other way round, took it
 * from 676 words differing to 662. The water skip's last two tests are
 * grouped, as the original branches on the rolling state and then the
 * impact speed: in one chain fold_truthop ors their flags (662). Working
 * out the horizontal speed before the side spin's magnitude, as
 * StepBallSimulation does, took it from 652 words, 248 in shape, to
 * 422 (182). Naming the lift per unit of spin, as a permuter run's float
 * temp suggested, takes it to 395 (167) wherever the local is declared;
 * scaling weight in place instead gives 412 (170). The register oracle,
 * forcing 53 pseudos' registers, leaves 188 words at best, so the code
 * differs too. The original's add.s for the x and z velocities takes the
 * velocity first and the wind's pull second, as velocity + pull does,
 * where the draft writes pull + velocity; written as the original orders
 * them, both lines gave 397 words, and the x line alone 393, with the
 * draft at 395. A billboard hit now updates the ball in
 * StepBallSimulation's order: the bounce count, the position, the last
 * bounce step, then both spins, which took the draft from 395 words (167
 * in shape) to 337 (149); of the 58 other orders of those five steps that
 * divide the spin speed before the side spin, the best gave 341 (153).
 * With that, writing the x line as velocity + pull gave 343 words, the z
 * line as pull + velocity 341, and both 345. With the draft at 247
 * words (111 in shape), velocity + pull gave 245 words in the x line
 * alone, 249 in the z line alone and 249 in both, and with both the
 * register oracle got no closer than 128 words forcing 32, against the
 * draft's 124 forcing 32; at 237 words (109), they give 235, 239 and 239.
 * Both bounces set the last bounce step after the three velocities, as
 * StepBallSimulation's do, which took the draft from 337 words (149 in
 * shape) to 325 (141); moving the first alone gave 329 (145), the second
 * alone 333 (145). Each substep copies stepFlags to previousStepFlags
 * after saving the start and the previous velocity, as StepBallSimulation
 * does, which took the draft from 325 words (141 in shape) to 314 (138);
 * with the start's y saved before its x as well, 316 (138). The lift
 * decays the spin after all three velocity updates, as StepBallSimulation
 * does, which took the draft from 314 words (138 in shape) to 296 (126).
 * The cup pull sets toCup in axis order, as StepBallSimulation does, and
 * a wall hit's reflection sets all three velocities before clearing the
 * spin, which took the draft from 296 words (126 in shape) to 290 (122);
 * alone, they gave 292 (124) and 294 (124). The ground spin's grip clears
 * the spin after both velocity updates, as StepBallSimulation's does,
 * which took the draft from 290 words (122 in shape) to 288 (120).
 * Holding the side spin in an s32, the field's type, and testing it for
 * the curve's sign keeps 288 words, takes the shape from 120 to 114 and
 * the register oracle from 129 words forcing 34 pseudos to 127 forcing
 * 35. Scaling the wall hit's x velocity into a local before its hit
 * position gives 278 words (106 in shape), but not for y and z: locals
 * for all three first give 328, one step local set for each axis in turn
 * 289 (101), and the contact fraction in a local 288. Writing the x line
 * as the sibling writes its z, or its triangle hit's x, gives 290 and
 * 298. The floor's in-cup and rolling tests read simulation->state after
 * GetGroundHeight, as StepBallSimulation's do, where a state local set
 * before the call gives 278 words (112 in shape) against 237 (109).
 *
 * The side spin's curve and the lift each weigh the ball's speed against
 * its spin, with their own ratio and weight; one ratio and one weight for
 * both gave 247 words differing (109 in shape), a weight for each takes
 * it to 237 (109), and a ratio for each as well keeps 237.
 *
 * Adding the wind's pull to the x velocity, rather than the velocity to the
 * pull, gives 235 words (109 in shape), but only two floating-point
 * registers change, and z's would then be written the other way round.
 *
 * Writing windX and windZ as scale times the wind's velocity, rather than
 * the velocity times scale, gives 235 words, 109 in shape, but the register
 * oracle's closest result rises from 118 words, forcing 31 pseudos, to 124,
 * forcing 29. 237 words differ, 109 in shape.
 */
#ifdef NON_MATCHING
s32 StepOpeningBallSimulation(BallSimulation* simulation) {
  PlaneTriangle triangle;

  f32 GetOpeningStepNoise(void) {
    return ((u32)((simulation->position[AXIS_X] + simulation->position[AXIS_Z] +
                      g_Players[simulation->playerIndex].noiseSeed) &
                NOISE_BYTE_MASK) *
               (1.0f / MAX_NOISE_BYTE_PAIR_SUM)) +
        ((u32)((simulation->position[AXIS_X] - simulation->position[AXIS_Z] +
                   g_Players[simulation->playerIndex].noiseSeed) &
             NOISE_BYTE_MASK) *
            (1.0f / MAX_NOISE_BYTE_PAIR_SUM));
  }

  f32 GetOpeningLength3D(f32 x, f32 y, f32 z) {
    return sqrtf((x * x) + (y * y) + (z * z));
  }

  f32 GetOpeningLength2D(f32 x, f32 z) {
    return sqrtf((x * x) + (z * z));
  }

  void ScaleToOpeningLength(f32 length, f32 * x, f32 * y, f32 * z) {
    f32 scale = length / sqrtf((*x * *x) + (*y * *y) + (*z * *z));

    *x *= scale;
    *y *= scale;
    *z *= scale;
  }

  /*
   * Points the lift rotation about the ball\'s spin axis, the level axis across
   * spinHeading, turning a negative spin into a positive one the other way
   */
  void BuildOpeningLiftRotation(void) {
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

  f32 GetOpeningPositionNoise(void) {
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
    startZ = simulation->position[AXIS_Z];
    previousVelocityX = simulation->velocity[AXIS_X];
    previousVelocityZ = simulation->velocity[AXIS_Z];
    simulation->previousStepFlags = simulation->stepFlags;
    if (IsGameFlagSet(GAME_FLAG_SHOW_OPENING_BALL_SPEED) &&
        simulation->purpose == LAUNCH_PURPOSE_STROKE) {
      sprintf(speedText, "%fkm/h   ",
          (s32)IntLength3D(simulation->velocity[AXIS_X],
              simulation->velocity[AXIS_Y], simulation->velocity[AXIS_Z]) *
              SPEED_TEXT_KILOMETERS_PER_HOUR_PER_SPEED_UNIT);
      PrintDebugText(speedText, SPEED_TEXT_COLUMN,
          f_OpeningSpeedTextRow + SPEED_TEXT_FIRST_ROW);
      f_OpeningSpeedTextRow =
          (f_OpeningSpeedTextRow + 1) % SPEED_TEXT_ROW_COUNT;
      PrintDebugText("                            ", SPEED_TEXT_CLEAR_COLUMN,
          f_OpeningSpeedTextRow + SPEED_TEXT_FIRST_ROW);
    }

    if (IsGameFlagSet(GAME_FLAG_SHOW_OPENING_BALL_HEIGHT)) {
      if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
        sprintf(heightText, "Height %fYd   ",
            (simulation->startPosition[AXIS_Y] - simulation->position[AXIS_Y]) *
                YARDS_PER_UNIT);
        PrintDebugText(heightText, HEIGHT_TEXT_COLUMN, HEIGHT_TEXT_ROW);
      }
    }

    /*
     * The rest of the step in an if instead of this goto gives the same code.
     * The goto stays as in StepBallSimulation, where the same test needs it to
     * match.
     */
    if (simulation->state == BALL_STATE_STOPPED) {
      goto SETTLE;
    }

    if (simulation->stepCount == 0) {
      BuildOpeningLiftRotation();
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

        if (IsStrokeOrTrialLaunch(simulation->purpose) &&
            FindRingHit(startX, startY, startZ, simulation->position[AXIS_X],
                simulation->position[AXIS_Y], simulation->position[AXIS_Z],
                &f_OpeningContactTriangle, &f_OpeningContactFraction) > 0) {
          s8* ringState = &g_Players[simulation->playerIndex]
                               .ringStates[f_OpeningContactTriangle.tag];

          if (*ringState == RING_STATE_SHOWN &&
              simulation->purpose == LAUNCH_PURPOSE_STROKE) {
            *ringState = RING_STATE_PASSED;
          }
        }

        if (g_GameState.isMiniGolf) {
          contact = FindMiniGolfGroundHit(simulation, startX, startY, startZ,
              simulation->position[AXIS_X], simulation->position[AXIS_Y],
              simulation->position[AXIS_Z], &f_OpeningContactTriangle,
              &f_OpeningContactFraction);
          if (g_GameState.isMiniGolf &&
              simulation->state == BALL_STATE_ROLLING) {
            s32 putterContact = FindMiniGolfGroundHit(simulation, startX,
                startY, startZ, simulation->position[AXIS_X],
                simulation->position[AXIS_Y] - MINI_GOLF_ROLL_PROBE_RISE,
                simulation->position[AXIS_Z], &triangle, &fraction);

            if (putterContact && fraction < f_OpeningContactFraction) {
              contact = putterContact;
              osSyncPrintf("PutterSpecialGroundCollision\n");
              f_OpeningContactTriangle = triangle;
              f_OpeningContactFraction = fraction;
            }
          }
          hitsBillboard = FALSE;
        } else {
          contact = FindGroundHit(startX, startY, startZ,
              simulation->position[AXIS_X], simulation->position[AXIS_Y],
              simulation->position[AXIS_Z], &f_OpeningContactTriangle,
              &f_OpeningContactFraction);
          hitsBillboard = FALSE;
        }

        if (simulation->state == BALL_STATE_FLYING) {
          if (simulation->lastBounceStep + BILLBOARD_HIT_BOUNCE_GAP <
              simulation->stepCount) {
            hitsBillboard = FindOpeningBillboardHit(startX, startY, startZ,
                simulation->position[AXIS_X], simulation->position[AXIS_Y],
                simulation->position[AXIS_Z], &triangle.tag, &fraction,
                simulation);
          } else {
            hitsBillboard = FALSE;
          }
        }

        if (hitsBillboard && contact == STEP_FLAG_GROUND_HIT) {
          if (f_OpeningContactFraction <= fraction) {
            contact = STEP_FLAG_GROUND_HIT;
            triangle = f_OpeningContactTriangle;
            fraction = f_OpeningContactFraction;
          } else {
            contact = STEP_FLAG_BILLBOARD_HIT;
          }
        } else if (hitsBillboard) {
          contact = STEP_FLAG_BILLBOARD_HIT;
        } else if (contact == STEP_FLAG_GROUND_HIT) {
          triangle = f_OpeningContactTriangle;
          fraction = f_OpeningContactFraction;
        } else {
          contact = 0;
        }

        if (simulation->state == BALL_STATE_FLYING ||
            simulation->state == BALL_STATE_ROLLING) {
          holeHit = FindHoleTriangleHit(startX, startY, startZ,
              simulation->position[AXIS_X], simulation->position[AXIS_Y],
              simulation->position[AXIS_Z], &f_OpeningContactTriangle,
              &f_OpeningContactFraction);
        } else {
          holeHit = FALSE;
        }

        if (holeHit && (contact == 0 || f_OpeningContactFraction <= fraction)) {
          contact = STEP_FLAG_HOLE_TRIANGLE_HIT;
          triangle = f_OpeningContactTriangle;
          fraction = f_OpeningContactFraction;
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
              : GetOpeningSurfaceInfo(simulation->launchSurface)->kind !=
                  SURFACE_KIND_GREEN) {
        pinHit =
            FindPinHit(startX, startY, startZ, simulation->position[AXIS_X],
                simulation->position[AXIS_Y], simulation->position[AXIS_Z],
                pinVelocity, &pinFraction, simulation->hasHitFlag, simulation);
        if (pinHit && !(simulation->previousStepFlags & STEP_FLAG_PIN_HIT)) {
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
            GetOpeningLength2D(
                simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]);
          }

          if (simulation->state == BALL_STATE_ROLLING) {
            simulation->state = BALL_STATE_FLYING;
            simulation->velocity[AXIS_Y] =
                -GetOpeningLength2D(simulation->velocity[AXIS_X],
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
          BuildOpeningLiftRotation();
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
                  simulation->soundStates[BALL_SOUND_CUP_IN] = CUP_IN_SOUND_DUE;
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

                toCup[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X] -
                    simulation->position[AXIS_X];
                toCup[AXIS_Y] = 0.0f;
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

                angle = atan2(toCup[AXIS_Z], toCup[AXIS_X]);
                depth = CUP_PULL_OUTER_RADIUS - (f32)cupDistance;
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
                  BuildOpeningLiftRotation();
                  contact = 0;
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

      GetOpeningLength3D(
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

        toCupX = g_HoleSetup.cupPosition[AXIS_X] - simulation->position[AXIS_X];
        toCupY = 0.0f;
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
        simulation->velocity[AXIS_Z] = 0;
        simulation->velocity[AXIS_X] = 0;
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
      simulation->position[AXIS_X] =
          startX + (fraction * simulation->velocity[AXIS_X]);
      simulation->position[AXIS_Y] =
          startY + (fraction * simulation->velocity[AXIS_Y]);
      simulation->position[AXIS_Z] =
          startZ + (fraction * simulation->velocity[AXIS_Z]);
      simulation->lastBounceStep = simulation->stepCount;
      simulation->spinSpeed /= BILLBOARD_HIT_SPIN_DIVISOR;
      simulation->sideSpin /= BILLBOARD_HIT_SPIN_DIVISOR;
      switch (triangle.tag) {
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
            &f_OpeningContactTriangle, &f_OpeningContactFraction) == TRUE &&
        f_OpeningContactTriangle.tag == HOLE_TRIANGLE_KIND_SHADED) {
      CoursePoint hitPosition;
      f32 normalX;
      f32 normalY;
      f32 normalZ;
      f32 edgeX;
      f32 edgeY;
      f32 edgeZ;
      f32 approach;

      normalX = f_OpeningContactTriangle.corners[AXIS_X][0];
      edgeY = 0.0f;
      normalY = edgeY;
      normalZ = f_OpeningContactTriangle.corners[AXIS_Z][0];
      if (f_OpeningContactTriangle.corners[AXIS_X][0] ==
              f_OpeningContactTriangle.corners[AXIS_X][1] &&
          f_OpeningContactTriangle.corners[AXIS_Z][0] ==
              f_OpeningContactTriangle.corners[AXIS_Z][1]) {
        edgeX = f_OpeningContactTriangle.corners[AXIS_X][2];
        edgeZ = f_OpeningContactTriangle.corners[AXIS_Z][2];
      } else {
        edgeX = f_OpeningContactTriangle.corners[AXIS_X][1];
        edgeZ = f_OpeningContactTriangle.corners[AXIS_Z][1];
      }

      normalX -= edgeX;
      normalY -= edgeY;
      normalZ -= edgeZ;
      guNormalize(&normalX, &normalY, &normalZ);
      approach = (-normalZ * simulation->velocity[AXIS_X]) +
          (normalY * simulation->velocity[AXIS_Y]) +
          (normalX * simulation->velocity[AXIS_Z]);
      f_OpeningContactFraction -= OPENING_WALL_BACK_OFF /
          GetOpeningLength3D(simulation->velocity[AXIS_X],
              simulation->velocity[AXIS_Y], simulation->velocity[AXIS_Z]);
      if (f_OpeningContactFraction < 0.0f) {
        f_OpeningContactFraction = 0.0f;
      }

      hitPosition[AXIS_X] = startX +
          (f_OpeningContactFraction *
              (simulation->velocity[AXIS_X] * OPENING_WALL_HIT_STEP_SHARE));
      hitPosition[AXIS_Y] = startY +
          (f_OpeningContactFraction *
              (simulation->velocity[AXIS_Y] * OPENING_WALL_HIT_STEP_SHARE));
      hitPosition[AXIS_Z] = startZ +
          (f_OpeningContactFraction *
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
        simulation->velocity[AXIS_Z] = reflected[AXIS_Z];
        simulation->spinSpeed = 0;
      }
      BuildOpeningLiftRotation();
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
        steepness = AbsFloat(impactSpeed) / speed;
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
            (impactSpeed <= OPENING_MIN_SKIP_IMPACT_SPEED ||
                GetOpeningLength2D(simulation->velocity[AXIS_X],
                    simulation->velocity[AXIS_Z]) *
                        OPENING_SKIP_SPEED_RATIO <=
                    impactSpeed)) {
          simulation->state = BALL_STATE_IN_WATER;
          simulation->velocity[AXIS_Z] = 0;
          simulation->velocity[AXIS_X] = 0;
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
      simulation->velocity[AXIS_Z] = bounced[AXIS_Z] * keep;
      simulation->lastBounceStep = simulation->stepCount;
      if (simulation->state == BALL_STATE_FLYING) {
        s32 spinVelocityX =
            simulation->spinSpeed * cosf(simulation->spinHeading);
        s32 spinVelocityZ =
            simulation->spinSpeed * sinf(simulation->spinHeading);
        f32 grip = 1.0f - bounce;

        simulation->velocity[AXIS_X] = simulation->velocity[AXIS_X] +
            ((spinVelocityX - simulation->velocity[AXIS_X]) * grip *
                AbsFloat(plane.normal[AXIS_Y]) * TRIANGLE_SPIN_GRIP);
        simulation->velocity[AXIS_Z] = simulation->velocity[AXIS_Z] +
            ((spinVelocityZ - simulation->velocity[AXIS_Z]) * grip *
                AbsFloat(plane.normal[AXIS_Y]) * TRIANGLE_SPIN_GRIP);
        spinVelocityX = spinVelocityX * BOUNCE_SPIN_DECAY;
        spinVelocityZ = spinVelocityZ * BOUNCE_SPIN_DECAY;
        spinVelocityX = spinVelocityX +
            ((previousVelocityX - spinVelocityX) * TRIANGLE_SPIN_FOLLOW);
        spinVelocityZ = spinVelocityZ +
            ((previousVelocityZ - spinVelocityZ) * TRIANGLE_SPIN_FOLLOW);
        simulation->spinHeading = atan2(spinVelocityZ, spinVelocityX);
        simulation->spinSpeed = IntLength2D(spinVelocityX, spinVelocityZ);
        BuildOpeningLiftRotation();
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
      GetOpeningLength3D(simulation->velocity[AXIS_X],
          simulation->velocity[AXIS_Y], simulation->velocity[AXIS_Z]);
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
        steepness = AbsFloat(impactSpeed) / speed;
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
          friction *= ((impactSpeed +
                           (impactSpeed * GetOpeningPositionNoise() *
                               LANDING_NOISE_SCALE)) *
                          OPENING_GREEN_LANDING_FRICTION_PER_SPEED) +
              OPENING_GREEN_LANDING_FRICTION_BASE;
          spin = ((impactSpeed * steepness) - OPENING_SPINLESS_LANDING_SPEED) *
              (1.0f / OPENING_LANDING_SPIN_SPEED_RANGE);
          break;
        default:
          friction *= (impactSpeed * OTHER_LANDING_FRICTION_PER_SPEED) +
              LANDING_FRICTION_BASE;
          friction +=
              friction * GetOpeningPositionNoise() * LANDING_NOISE_SCALE;
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
                AbsFloat(plane.normal[AXIS_Y]) * GROUND_SPIN_GRIP);
        simulation->velocity[AXIS_Z] = simulation->velocity[AXIS_Z] +
            ((spinVelocityZ - simulation->velocity[AXIS_Z]) *
                AbsFloat(plane.normal[AXIS_Y]) * GROUND_SPIN_GRIP);
        simulation->spinSpeed = 0;
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
        simulation->velocity[AXIS_Z] = bounced[AXIS_Z] * keep;
        simulation->lastBounceStep = simulation->stepCount;
        spinVelocityX = simulation->spinSpeed * cosf(simulation->spinHeading);
        spinVelocityZ = simulation->spinSpeed * sinf(simulation->spinHeading);
        grip = 1.0f - restitution;
        driftX = (spinVelocityX - simulation->velocity[AXIS_X]) * grip *
            AbsFloat(plane.normal[AXIS_Y]) * OPENING_GROUND_SPIN_DRIFT_GRIP;
        driftZ = (spinVelocityZ - simulation->velocity[AXIS_Z]) * grip *
            AbsFloat(plane.normal[AXIS_Y]) * OPENING_GROUND_SPIN_DRIFT_GRIP;
        drift = (s32)IntLength2D(driftX, driftZ);
        driftLimit = impactSpeed * OPENING_MAX_SPIN_DRIFT_SHARE;
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
        BuildOpeningLiftRotation();
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
      BuildOpeningLiftRotation();
    }

    if (simulation->state == BALL_STATE_ROLLING && !isNearCup) {
      TrianglePlane plane;
      SurfaceInfo* info;
      s32 groundHeight;
      s32 slopeX;
      s32 slopeZ;
      f32 friction;
      Boolean isSteep;

      rollSpeed = GetOpeningLength2D(
          simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]);
      isSteep = FALSE;
      if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
        contact = STEP_FLAG_GROUND_HIT;
        info = &f_OpeningSurfaceInfo[SURFACE_GREEN];
        triangle.corners[AXIS_X][0] = 0;
        triangle.corners[AXIS_X][1] = 0x100;
        triangle.corners[AXIS_X][2] = 0;
        triangle.corners[AXIS_Y][0] = 0;
        triangle.corners[AXIS_Y][1] = 0;
        triangle.corners[AXIS_Y][2] = 0;
        triangle.corners[AXIS_Z][0] = 0;
        triangle.corners[AXIS_Z][1] = 0;
        triangle.corners[AXIS_Z][2] = 0x100;
        if (f_OpeningClubTestMode == CLUB_TEST_SHOT) {
          info = &f_OpeningSurfaceInfo[SURFACE_FAIRWAY];
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
              f32 lie = GetOpeningStepNoise();

              if (lie < FAIRWAY_POOR_LIE_CHANCE) {
                simulation->surface = SURFACE_FAIRWAY_POOR_LIE;
              } else if (lie <
                  FAIRWAY_POOR_LIE_CHANCE + FAIRWAY_FAIR_LIE_CHANCE) {
                simulation->surface = SURFACE_FAIRWAY_FAIR_LIE;
              }
            }

            break;
          case SURFACE_ROUGH:
            if (GetOpeningStepNoise() < ROUGH_POOR_LIE_CHANCE) {
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
          BuildOpeningLiftRotation();
          goto ROLLED;
        }
      }

      slopeX =
          (-plane.normal[AXIS_X] * GRAVITY) / AbsFloat(plane.normal[AXIS_Y]);
      slopeZ =
          (-plane.normal[AXIS_Z] * GRAVITY) / AbsFloat(plane.normal[AXIS_Y]);
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

      if ((s32)IntLength2D(simulation->velocity[AXIS_X],
              simulation->velocity[AXIS_Z]) < MAX_STOP_SPEED) {
        if (!isSteep &&
            IntLength2D(slopeX, slopeZ) * friction < MAX_STOP_SLOPE_PULL) {
          simulation->state = BALL_STATE_STOPPED;
        }
      }

      if (!isSteep && info->kind != SURFACE_KIND_GREEN &&
          simulation->lastBounceStep + OFF_GREEN_STOP_STEPS <
              simulation->stepCount &&
          GetOpeningLength3D(simulation->velocity[AXIS_X],
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
      s32 sideSpin;
      BallSpeed horizontalSpeed;
      f32 sideSpinMagnitude;
      f32 curveRatio;
      f32 liftRatio;
      f32 curveWeight;
      f32 liftWeight;
      f32 liftPerSpin;
      f32 curve;
      f32 spinMagnitude;
      f32 lift;
      f32 height;
      f32 windX;
      f32 windZ;

      relative[AXIS_X] =
          simulation->velocity[AXIS_X] - simulation->windVelocityX;
      sideSpin = simulation->sideSpin;
      relative[AXIS_Y] = simulation->velocity[AXIS_Y];
      relative[AXIS_Z] =
          simulation->velocity[AXIS_Z] - simulation->windVelocityZ;
      airSpeed = GetOpeningLength3D(
          relative[AXIS_X], relative[AXIS_Y], relative[AXIS_Z]);
      drag =
          (sqrtf(airSpeed * AIR_DRAG_SPEED_SCALE) * AIR_DRAG_PER_ROOT_SPEED) +
          MIN_AIR_DRAG;
      horizontalSpeed = GetOpeningLength2D(relative[AXIS_X], relative[AXIS_Z]);
      sideSpinMagnitude = AbsFloat(sideSpin * SIDE_SPIN_SPEED_SCALE);
      curveRatio = 0.0f;
      if (sideSpinMagnitude != 0.0f) {
        curveRatio = horizontalSpeed / sideSpinMagnitude;
      }

      curveWeight =
          1.0f - (AbsFloat(curveRatio - 1.0f) * SIDE_SPIN_MISMATCH_FALLOFF);
      if (curveWeight < 0.0f) {
        curveWeight = 0.0f;
      }

      curve = sideSpin * (curveWeight * CURVE_PER_SIDE_SPIN);
      if (horizontalSpeed < AbsFloat(curve) * MAX_CURVE_SPEED_RATIO) {
        curve = horizontalSpeed * (1.0f / MAX_CURVE_SPEED_RATIO);
        if (sideSpin < 0) {
          curve = -curve;
        }
      }

      if (horizontalSpeed > MIN_CURVE_SPEED) {
        f32 scale = (1.0f / horizontalSpeed) * curve;

        simulation->velocity[AXIS_X] =
            (-relative[AXIS_Z] * scale) + simulation->velocity[AXIS_X];
        simulation->velocity[AXIS_Z] =
            simulation->velocity[AXIS_Z] + (relative[AXIS_X] * scale);
      }

      simulation->sideSpin = simulation->sideSpin * SIDE_SPIN_DECAY;
      spinMagnitude =
          AbsFloat(simulation->spinSpeed * OPENING_SPIN_SPEED_SCALE);
      liftRatio = 0.0f;
      if (spinMagnitude != 0.0f) {
        liftRatio = airSpeed / spinMagnitude;
      }

      liftWeight =
          1.0f - (AbsFloat(liftRatio - 1.0f) * OPENING_SPIN_MISMATCH_FALLOFF);
      if (liftWeight < 0.0f) {
        liftWeight = 0.0f;
      }

      liftPerSpin = liftWeight * OPENING_LIFT_PER_SPIN;
      lift = (simulation->spinSpeed * OPENING_LIFT_SPIN_SCALE) * liftPerSpin;
      if (simulation->spinSpeed != 0 && airSpeed > MIN_LIFT_AIR_SPEED) {
        guMtxXFMF(simulation->liftRotation, relative[AXIS_X], relative[AXIS_Y],
            relative[AXIS_Z], &turned[AXIS_X], &turned[AXIS_Y],
            &turned[AXIS_Z]);
        if (airSpeed < AbsFloat(lift) * OPENING_MAX_LIFT_SPEED_RATIO) {
          lift = airSpeed * (1.0f / OPENING_MAX_LIFT_SPEED_RATIO);
        }

        ScaleToOpeningLength(
            lift, &turned[AXIS_X], &turned[AXIS_Y], &turned[AXIS_Z]);
        simulation->velocity[AXIS_X] =
            simulation->velocity[AXIS_X] + turned[AXIS_X];
        simulation->velocity[AXIS_Y] =
            simulation->velocity[AXIS_Y] + turned[AXIS_Y];
        simulation->velocity[AXIS_Z] =
            simulation->velocity[AXIS_Z] + turned[AXIS_Z];
        simulation->spinSpeed = simulation->spinSpeed * OPENING_SPIN_DECAY;
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

      simulation->velocity[AXIS_X] =
          ((windX - simulation->velocity[AXIS_X]) * drag) +
          simulation->velocity[AXIS_X];
      simulation->velocity[AXIS_Z] =
          ((windZ - simulation->velocity[AXIS_Z]) * drag) +
          simulation->velocity[AXIS_Z];
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
        floor = GetGroundHeight(
            simulation->position[AXIS_X], simulation->position[AXIS_Z]);
        if (simulation->state == BALL_STATE_IN_CUP || simulation->isHoledOut) {
          floor += IN_CUP_FLOOR_DEPTH;
        } else if (simulation->state == BALL_STATE_ROLLING) {
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

        simulation->surfaceBelow = GetSurface(
            simulation->position[AXIS_X], simulation->position[AXIS_Z]);
        kind = GetOpeningSurfaceInfo(simulation->surfaceBelow)->kind;
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

    simulation->stepFlags = flags;
  } while (++substep < substepCount);

  return flags;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", GetOpeningStepNoise);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", GetOpeningLength3D);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", GetOpeningLength2D);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", ScaleToOpeningLength);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", BuildOpeningLiftRotation);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", GetOpeningPositionNoise);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", StepOpeningBallSimulation);
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
 * Doesn't match: fold moves CLUB_SIDE_SPIN_SCALE onto sideSign, where the
 * original doubles club->sideSpin before multiplying by sideSign. The two
 * products agree, but their shift and multiply operands differ (4 words).
 * A left shift of the club value prevents that reassociation, but leaves 14
 * words differing: local-alloc assigns the club contribution and shot->offsetX
 * the opposite registers. Keep the named scale, as in LaunchBallSimulation,
 * rather than shifting a potentially negative signed club value.
 *
 * Swapping the sum's operands gives 16 to 28 words with the shift form.
 * Computing the doubled club contribution in a local before yawRadians or
 * just before the sum leaves 14, unless staged multiplications let fold move
 * the scale onto sideSign again (4). No extra local is needed for that form.
 *
 * With the doubled club value in a local, the register oracle matches when 4
 * pseudos are forced, so only allocation differs: local-alloc ties the load to
 * its doubling and gives that quantity $v1 before shot->offsetX's load, where
 * the original has them the other way round. Setting the local just before the
 * pitch's offset term, the yaw or the sum, with either operand order at the yaw
 * or the sum, leaves 14 words, as do 48 orders of the operands of the sum and
 * of its product with speed with the local; set before the pitch or its sign
 * test, 11 differ (9 in shape), and earlier 20 to 30 (8 to 14), as the load
 * moves into earlier blocks. Staging the product in the local, or putting the
 * offset's part or the whole sum in locals of their own, leaves 14 too, or 18
 * when the sum starts with the club's part.
 *
 * fold's associate step rewrites (club->sideSpin * 2) * sideSign as
 * club->sideSpin * (sideSign * 2), and does so too with sideSign written
 * first, as LaunchBallSimulation's club test writes it, which gives 6 words;
 * other orders of the club's part give 14 to 26, multiplying each part by
 * speed 12 to 20, and doubling the offset's and club's parts together 16.
 * Doubling through a static inline helper, which GCC inlines after fold,
 * keeps the target's sll before the mult but leaves 14 words, as the local
 * does, and so does adding club->sideSpin to itself, with sideSign on either
 * side. Holding the scale in a local, as the club test holds its percent,
 * leaves 14 words too, none in shape, and setting it at the routine's start
 * 23 (15).
 */
#ifdef NON_MATCHING
s32 LaunchOpeningBallSimulation(
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

  f32 PickOpeningLieBlend(void) {
    return 0.0f;
  }

  simulation->purpose = purpose;
  f_UnusedOpeningLaunchCount++;
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
  simulation->launchSurface = simulation->surface;
  simulation->firstBounceSurface = simulation->launchSurface;
  simulation->hasEnteredWater = 0;
  if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
    simulation->playerIndex = shot->playerIndex;
  } else {
    simulation->playerIndex = 0;
  }

  club = &f_OpeningClubs[shot->clubIndex];
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
    SurfaceInfo* info = GetOpeningSurfaceInfo(simulation->launchSurface);

    blend = PickOpeningLieBlend();
    GetSurfacePercentRange(info, club, percentRange);
    speed = ((percentRange[0] * blend) + (percentRange[1] * (1.0f - blend))) *
        speed * ONE_PER_PERCENT;
  } else {
    /*
     * In a local, to match: scaling by the constant itself gives 19 words
     * differing (11 in shape), against 4 (0)
     */
    s32 percent = CLUB_TEST_SPEED_HALF_PERCENTS;

    speed = speed * percent / HALF_PERCENTS_PER_ONE;
  }

  pitch = (club->pitch + entry->pitchChange) * DEGREES_TO_RADIANS;
  if (pitch >= 0.0f) {
    pitch = 0.0f;
  }

  pitch += (f32)club->offsetPitch * shot->offsetY *
      (DEGREES_TO_RADIANS / OFFSET_Y_PER_PITCH_STEP);
  yawRadians = (u16)shot->heading * BINARY_ANGLE_TO_RADIANS;
  simulation->sideSpin =
      (((((sideSign * entry->sideSpinChange) + shot->offsetX) *
            OPENING_OFFSET_SIDE_SPIN_SCALE) +
           ((club->sideSpin * CLUB_SIDE_SPIN_SCALE) * sideSign)) *
          speed) *
      SIDE_SPIN_PER_SPEED;
  simulation->spinSize = 0.0f;
  spinBase = club->spin * CLUB_SPIN_BASE_SCALE;
  simulation->spinSpeed = (((spinBase + CLUB_TEST_SPIN_OFFSET) *
                               CLUB_TEST_SPIN_PER_OFFSET * shot->offsetY) -
                              spinBase) *
      speed * SPIN_PER_SPEED;
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
  for (index = 0; index < BALL_SOUND_COUNT; index++) {
    simulation->soundStates[index] = 0;
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
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", PickOpeningLieBlend);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_ball", LaunchOpeningBallSimulation);
#endif
