/*
 * The ball simulation. It launches the ball with a shot's velocity and wind,
 * then moves it a step at a time as it flies, bounces and rolls over the
 * course. It plays the sounds of the ball's impacts and spawns their effects.
 */
#include "ball_simulation.h"

#include <math.h>
#include <stdlib.h>
#include <ultra64.h>

#include "ball_physics.h"
#include "club.h"
#include "collision.h"
#include "compiler.h"
#include "course_space.h"
#include "debug_text.h"
#include "effects.h"
#include "game_flags.h"
#include "game_state.h"
#include "geometry.h"
#include "hole_objects.h"
#include "hole_triangles.h"
#include "include_asm.h"
#include "language.h"
#include "math_util.h"
#include "player.h"
#include "scene.h"
#include "shot_effects.h"
#include "sound.h"
#include "surface.h"
#include "terrain.h"
#include "turn.h"

BallSimulation g_TurnBallSimulation;

BallSimulation g_BallSimulations[BALL_SIMULATION_COUNT] = {0};

s8 g_ClubTestMode = CLUB_TEST_NONE;

static SurfaceInfo f_SurfaceInfo[SURFACE_COUNT] = {
    /* SURFACE_OUT_OF_BOUNDS */
    {SURFACE_KIND_OUT_OF_BOUNDS, "O.B.", 100, 100, 0, 0, 0, 0x500, SINK_DEPTH_HALF, 7},
    /* SURFACE_FAIRWAY */
    {SURFACE_KIND_FAIRWAY, "FAIRWAY", 98, 100, 5, 960, 500, 0x501, SINK_DEPTH_NONE, 0},
    /* SURFACE_ROUGH */
    {SURFACE_KIND_ROUGH, "ROUGH", 80, 90, 30, 600, 200, 0x508, SINK_DEPTH_HALF, 1},
    /* SURFACE_BUNKER */
    {SURFACE_KIND_BUNKER, "BUNKER", 70, 85, 30, 600, 0, 0x504, SINK_DEPTH_NONE, 2},
    /* SURFACE_GREEN */
    {SURFACE_KIND_GREEN, "ON GREEN", 100, 100, 0, 972, 600, 0x50A, SINK_DEPTH_NONE, 6},
    /* SURFACE_WATER_HAZARD */
    {SURFACE_KIND_WATER_HAZARD, "WATER HAZARD", 100, 100, 0, 900, 400, 0x50C, SINK_DEPTH_NONE, 3},
    /* SURFACE_CART_WAY */
    {SURFACE_KIND_CART_WAY, "CART WAY", 80, 100, 10, 975, 890, 0x50B, SINK_DEPTH_NONE, 4},
    /* SURFACE_TEE_GROUND */
    {SURFACE_KIND_TEE_GROUND, "TEE GROUND", 98, 100, 10, 930, 500, 0x501, SINK_DEPTH_NONE, 8},
    /* SURFACE_ROCK */
    {SURFACE_KIND_ROCK, "ROCK", 80, 90, 30, 930, 800, 0x50D, SINK_DEPTH_NONE, 9},
    /* SURFACE_GREEN_EDGE */
    {SURFACE_KIND_GREEN_EDGE, "GREEN EDGE", 98, 100, 10, 966, 550, 0x501, SINK_DEPTH_NONE, 0},
    /* SURFACE_SEMI_ROUGH */
    {SURFACE_KIND_SEMI_ROUGH, "SEMI ROUGH", 90, 98, 30, 930, 500, 0x507, SINK_DEPTH_NONE, 0},
    /* SURFACE_BARE_GROUND */
    {SURFACE_KIND_BARE_GROUND, "BARE GROUND", 80, 90, 30, 930, 500, 0x50C, SINK_DEPTH_NONE, 1},
    /* SURFACE_DEEP_ROUGH */
    {SURFACE_KIND_DEEP_ROUGH, "DEEP ROUGH", 50, 80, 30, 430, 100, 0x509, SINK_DEPTH_BURIED, 5},
    /* SURFACE_NO_DROP_GROUND */
    {SURFACE_KIND_NO_DROP_GROUND, "BARE GROUND", 80, 90, 30, 930, 500, 0x50C, SINK_DEPTH_NONE, 1},
    /* SURFACE_WASTE_AREA */
    {SURFACE_KIND_WASTE_AREA, "WASTE AREA", 50, 80, 30, 430, 100, 0x8F1, SINK_DEPTH_HALF, 5},
    /* SURFACE_UNNAMED_ROCK */
    {SURFACE_KIND_ROCK, "            ", 98, 100, 40, 930, 500, 0x500, SINK_DEPTH_NONE, 9},
    /* SURFACE_BUNKER_FAIR_LIE */
    {SURFACE_KIND_BUNKER_FAIR_LIE, "BUNKER", 50, 70, 30, 600, 0, 0x505, SINK_DEPTH_HALF, 2},
    /* SURFACE_BUNKER_POOR_LIE */
    {SURFACE_KIND_BUNKER_POOR_LIE, "BUNKER", 40, 60, 30, 600, 0, 0x506, SINK_DEPTH_HALF, 2},
    /* SURFACE_BUNKER_UNUSED_LIE */
    {SURFACE_KIND_BUNKER_UNUSED_LIE, "BUNKER", 40, 60, 40, 600, 0, 0x506, SINK_DEPTH_HALF, 2},
    /* SURFACE_FAIRWAY_FAIR_LIE */
    {SURFACE_KIND_FAIRWAY_FAIR_LIE, "FAIRWAY", 94, 98, 7, 950, 380, 0x502, SINK_DEPTH_NONE, 0},
    /* SURFACE_FAIRWAY_POOR_LIE */
    {SURFACE_KIND_FAIRWAY_POOR_LIE, "FAIRWAY", 90, 94, 7, 950, 380, 0x503, SINK_DEPTH_NONE, 0},
    /* SURFACE_ROUGH_POOR_LIE */
    {SURFACE_KIND_ROUGH_POOR_LIE, "ROUGH", 70, 85, 30, 600, 200, 0x90C, SINK_DEPTH_HALF, 5},
    /* SURFACE_OUT_OF_COURSE */
    {SURFACE_KIND_OUT_OF_COURSE, "OUT OF COURSE", 100, 100, 0, 0, 0, 0x500, SINK_DEPTH_NONE, 7},
    /* SURFACE_TEE_UP */
    {SURFACE_KIND_TEE_UP, "TEE UP", 98, 100, 10, 930, 500, 0x500, SINK_DEPTH_NONE, 8},
    /* SURFACE_MINI_GOLF_GREEN */
    {SURFACE_KIND_GREEN, "GREEN", 100, 100, 0, 972, 500, 0x8F2, SINK_DEPTH_NONE, 6},
    /* SURFACE_MINI_GOLF_FAIRWAY */
    {SURFACE_KIND_FAIRWAY, "BLUE", 98, 100, 5, 960, 400, 0x8F3, SINK_DEPTH_NONE, 0},
    /* SURFACE_MINI_GOLF_CART_WAY */
    {SURFACE_KIND_CART_WAY, "RED", 80, 100, 10, 980, 800, 0x8F4, SINK_DEPTH_NONE, 4},
};

/* For impactTiming values below and above the entries of f_ImpactEffects */
static ImpactEffect f_EarlyImpactEffect = {-90, -64, -40, -10, 100, 1};
static ImpactEffect f_LateImpactEffect = {-80, 64, 30, 10, -5, 1};
static ImpactEffect f_ImpactEffects[MAX_LIE_RATING + 1][IMPACT_TIMING_ENTRY_COUNT] = {
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

s32 g_UnusedLaunchCount = 0;
/* Read by no routine */
static s32 f_UnusedWordsAfterLaunchCount[4] UNUSED = {0};

SurfaceInfo *GetSurfaceInfo(Surface surface) {
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
        g_BallSimulations[index].unusedResetValue = 0;
        g_BallSimulations[index].state = BALL_STATE_FLYING;
    }
}

/*
 * Moves the simulated ball through one step, with a second substep when the
 * ball bounces within the first 40% of its path. Each substep tests the path
 * against rings, the ground, billboards, the pin, the cup, walls and hole
 * triangles, then bounces, rolls, sinks or stops the ball, and a flying ball
 * feels drag, curve, lift and the wind. Returns the STEP_FLAG_* bits for the
 * step.
 *
 * Doesn't match: the register allocation and instruction scheduling differ in
 * many places, and where the original recomputes abs.s for each use of a
 * float's absolute value, this code shares one abs.s or compiles a compare and
 * branch. Adding the height to its slide step, rather than the step to the
 * height, took 779 words differing to 771, 235 in shape; it later cost 8 words,
 * so the slide adds the step to the height, as it does in x and z. The slope's
 * two compare-and-branch magnitudes come from the nested AbsoluteValue
 * helper; with fabsf there 1194 words differ (326 in shape) and with
 * AbsFloat 482 (128). Mixing two forms, one per slope, gives 452 to
 * 482, but takes one magnitude two ways. The spin grip's two take fabsf, an
 * abs.s: with AbsoluteValue there 518 words differed (144). The pull toward the
 * cup sets toCup's X before its zero Y, so the zero store follows the cup's X
 * load as in the original rather than filling the debug text branch's delay
 * slot; with Y first, 769 words differ (233 in shape). A permuter output
 * reached 760 words (230 in shape) with Y first but the ball's X copied to a
 * fake s32 temp before it, from 766 (236) without the copy; reverting its fake
 * temp for push or its move of semi-rough's case label below the friction
 * scaling changes nothing. On the 769-word draft, setting side's X inside the
 * landing tilt test gave 766 (236), as did side's three stores, and the
 * gravity's nested if as an else-if changed nothing. On the 763-word draft, the
 * register oracle, forcing 126 pseudos' registers, left 425 words at best, so
 * the code differs too, and a permuter output gaining 1 word (5 in shape)
 * normalized the triangle's normal after both of the hit's surface branches,
 * but the original's club-test branch jumps past that call and the other skips
 * it unless the surface is green or green edge, so the output's call changes
 * what the code does. The second toCup, for a ball between
 * CUP_PULL_INNER_RADIUS and CUP_PULL_OUTER_RADIUS of the cup, is set in axis
 * order too, which took the draft from 749 words to 747 and from 225 in shape
 * to 221; with its Y of 1 first, as the draft had it, 749 differed. Past
 * MAX_STEP_COUNT, the ball's position is set before its velocity, both in axis
 * order, which took the draft from 747 words (221 in shape) to 743 (217). A
 * wall hit clears the spin after all three position updates, as the opening's
 * copy clears it after its velocities, which keeps 743 words and takes the
 * shape from 217 to 215; the register oracle then gets to 409 words forcing 124
 * pseudos, against 413 forcing 123. A triangle bounce's spin grip, 1.0f -
 * bounce, is written into both velocity updates, as their other factors are,
 * where a grip local gave 743 words (215 in shape) against 727 (209); in the
 * opening's copy the two forms give 288 words and the same oracle result. The
 * floor's in-cup and rolling tests read simulation->state after
 * GetGroundHeight, where a state local set before the call gave 727 words (209
 * in shape) against 680 (206). Of the six orders of the flight's drag updates,
 * only y, z and x gives fewer words, 668 (676 when the draft had 680), the
 * others 672, and it still stores z before y, where the original, having worked
 * out y's drag first, stores x, y and z in turn. An s16 spinVelocityZ where the
 * ball starts rolling gives 658 words (208 in shape), but adds two shifts or
 * masks to the ten that the draft and the original have; s16 for spinVelocityX
 * as well gives 662 (210) and adds two 16-bit shift pairs, spinVelocityX alone
 * 680, and in StepOpeningBallSimulation's roll start spinVelocityZ alone 259
 * and both 265 against its 247. In the triangle bounce an s16 spinVelocityZ
 * gives 743 words (215), both 771 and spinVelocityX alone 749, and
 * spinVelocityZ in StepOpeningBallSimulation's 277. Apart from the magnitudes',
 * this note's counts were measured with GCC's built-in fabsf, which left 672
 * words differing, 206 in shape.
 *
 * The curve's ratio is set to 0 in an else rather than before its test. The
 * code at the test is the same either way, but the change takes the draft
 * from 478 words differing (126 in shape) to 462 (112), and the register
 * oracle's closest result from 270 words to 254, forcing 76 pseudos for
 * each. The same form in StepOpeningBallSimulation's curve and lift ratios
 * leaves 237 words there.
 *
 * The bounce's drift is scaled x first and the water skip sums the square of
 * x's speed first, as the target converts x's drift and loads x's speed
 * first and as StepOpeningBallSimulation's copy already scales its drift; an
 * earlier sweep had chosen z first for both. Scaling x first takes the draft
 * from 462 words to 454, and the register oracle's closest result from 254
 * words to 246, forcing 76 pseudos for each; summing x first as well takes
 * them to 450 and 240, forcing 75 pseudos, with 112 in shape throughout.
 * That sweep's other choice, multiplying the wall and triangle hits' z steps
 * by the fraction last, still saves 2 words each.
 *
 * The original divides the slope's x and z by the roll speed before the
 * slope steering's dot product, where the draft divides each inside its
 * steering update. Dividing them first into direction locals gives 458
 * words (112 in shape), and the register oracle's closest result stays at
 * 240 words but forces 79 pseudos. 450 words differ, 112 in shape.
 *
 * The counts above were measured before the change below. The bounce's
 * speed loss, steepness times SOFTNESS_SPEED_LOSS_SCALE times softness,
 * is held in speedLoss, apart from keep, the share of the bounced
 * velocity kept. The register oracle had reported keep lining up with two
 * registers, $f10 for the loss and $f6 for the share; with its own
 * variable the report no longer lists it, and the draft goes from 450
 * words differing, 112 in shape, to 443 (113). The loss as one product in
 * speedLoss, or inside keep's expression, gives 457 (117). 443 words
 * differ, 113 in shape.
 *
 * At a hole triangle hit, the original recomputes the plane's address for
 * guNormalize where ours keeps it in $s0 from GetTrianglePlane, as cse
 * finds it equal. Moving the green leveling out of the else gives 435
 * words (105 in shape), but the club test's arm then jumps to the green
 * test, where the original's jumps past it. The original also leaves the
 * first-bounce test's delay slot empty, where ours fills it with the
 * surface load.
 *
 * The hole and billboard hit flags are cleared together, before the
 * billboard's test, as a sweep moving each assignment found. That takes
 * the draft from 443 words (113 in shape) to 440 (114), and the register
 * oracle's closest from 228 words to 222, forcing 123 pseudos either way.
 *
 * The water skip's test compares the impact speed with the scaled horizontal
 * speed, and the cup pull's the horizontal speed with the scaled vertical one,
 * written the other way round from before, as a sweep swapping each operator's
 * operands found. That takes the draft from 440 words (114 in shape) to 415
 * (107), and the register oracle's closest from 222 words, forcing 123 pseudos,
 * to 215, forcing 118. The cup pull's alone gives 419 (109), and the skip's
 * alone 436 (112). 415 words differ, 107 in shape.
 */
#ifdef NON_MATCHING
s32 StepBallSimulation(BallSimulation *simulation) {
    PlaneTriangle hit;
    PlaneTriangle triangle;
    BallSpeed velocity[AXIS_COUNT];
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

    void PlayStrokeImpactSound(SoundId sound, BallSpeed fullSpeed, BallSpeed speed) {
        SoundVolume volume = speed * SOUND_FULL_SCALE / fullSpeed;

        if (volume >= MIN_HIT_SOUND_VOLUME) {
            if (volume > SOUND_FULL_SCALE) {
                volume = SOUND_FULL_SCALE;
            }

            if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                PlaySound(sound, g_NextSoundChannel, volume);
            }
        }
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
        }
        else {
            axisZ = 1.0f;
            axisX = 0.0f;
        }

        guRotateF(simulation->liftRotation, LIFT_ANGLE_DEGREES, -axisX, 0.0f, axisZ);
    }

    void PlayBillboardHitSound(BillboardCell cell, s32 speed) {
        void PlayImpactSound(SoundId sound, BallSpeed fullSpeed, BallSpeed speed) {
            SoundVolume volume = speed * SOUND_FULL_SCALE / fullSpeed;

            if (volume >= MIN_HIT_SOUND_VOLUME) {
                if (volume > SOUND_FULL_SCALE) {
                    volume = SOUND_FULL_SCALE;
                }

                PlaySound(sound, g_NextSoundChannel, volume);
            }
        }

        switch (cell) {
            case BILLBOARD_CELL_TRUNK:
                PlayImpactSound(SOUND_HIT_TRUNK, FULL_VOLUME_HIT_SPEED, speed);
                break;
            case BILLBOARD_CELL_FOLIAGE:
                PlayImpactSound(SOUND_HIT_FOLIAGE, FULL_VOLUME_HIT_SPEED, speed);
                break;
            case BILLBOARD_CELL_SHELL:
                PlayImpactSound(SOUND_HIT_SHELL, FULL_VOLUME_HIT_SPEED, speed);
                break;
            case BILLBOARD_CELL_SOLID:
            default:
                PlayImpactSound(SOUND_HIT_SOLID, FULL_VOLUME_HIT_SPEED, speed);
                break;
        }
    }

    f32 GetPositionNoise(void) {
        return (((simulation->position[AXIS_X] + simulation->position[AXIS_Z]) & NOISE_BYTE_MASK) -
                   NOISE_BYTE_MIDPOINT) /
            NOISE_BYTE_MIDPOINT;
    }

    inline f32 AbsoluteValue(f32 value) {
        return value < 0.0f ? -value : value;
    }

    inline f32 GetStepNoise(void) {
        return (u32)((simulation->position[AXIS_X] + simulation->position[AXIS_Z] +
                         g_Players[simulation->playerIndex].noiseSeed) &
                   NOISE_BYTE_MASK) /
            MAX_NOISE_BYTE_PAIR_SUM +
            (u32)((simulation->position[AXIS_X] - simulation->position[AXIS_Z] +
                      g_Players[simulation->playerIndex].noiseSeed) &
                NOISE_BYTE_MASK) /
            MAX_NOISE_BYTE_PAIR_SUM;
    }

    velocity[AXIS_X] = simulation->velocity[AXIS_X];
    velocity[AXIS_Y] = simulation->velocity[AXIS_Y];
    velocity[AXIS_Z] = simulation->velocity[AXIS_Z];
    fraction = 1.0f;
    substep = 0;
    do {
        /*
         * The rest of the substep in an if instead of this goto gives the same
         * code. The goto stays like the one to SETTLE below, which matching needs.
         */
        if (substep != 0 && simulation->purpose == LAUNCH_PURPOSE_SHOT_PREVIEW) {
            goto NEXT;
        }

        startY = simulation->position[AXIS_Y];
        isNearCup = FALSE;
        startX = simulation->position[AXIS_X];
        startZ = simulation->position[AXIS_Z];
        previousVelocityX = velocity[AXIS_X];
        previousVelocityZ = velocity[AXIS_Z];
        simulation->previousStepFlags = simulation->stepFlags;
        hitsHoleTriangle = FALSE;
        /*
         * The goto, to match: the rest of the step in an if on the ball not having
         * stopped gives 978 words differing, 126 in shape, against 478 and 126.
         */
        if (simulation->state == BALL_STATE_STOPPED) {
            goto SETTLE;
        }

        if (simulation->stepCount == 0) {
            BuildLiftRotation();
        }

        if (++simulation->stepCount >= MAX_STEP_COUNT) {
            simulation->position[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X] + 614.4f;
            simulation->position[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z];
            simulation->velocity[AXIS_X] = 0;
            simulation->velocity[AXIS_Z] = 0;
        }

        if (simulation->state == BALL_STATE_ROLLING) {
            simulation->rollFrameCount++;
        }

        simulation->position[AXIS_X] = simulation->position[AXIS_X] + velocity[AXIS_X];
        simulation->position[AXIS_Y] = simulation->position[AXIS_Y] + velocity[AXIS_Y];
        simulation->position[AXIS_Z] = simulation->position[AXIS_Z] + velocity[AXIS_Z];
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
                }
                else {
                    fraction = 0.0f;
                }
            }
            else {
                contact = 0;
            }
        }
        else {
            if (simulation->state == BALL_STATE_IN_CUP) {
                contact = 0;
            }
            else {
                s32 endY;
                s32 hitsBillboard;
                s32 holeHit;

                if (g_GameState.gameMode == GAME_MODE_RING_SHOT &&
                    IsStrokeOrTrialLaunch(simulation->purpose) &&
                    FindRingHit(
                        startX,
                        startY,
                        startZ,
                        simulation->position[AXIS_X],
                        simulation->position[AXIS_Y],
                        simulation->position[AXIS_Z],
                        &hit,
                        &hitFraction
                    ) > 0 &&
                    g_Players[simulation->playerIndex].ringStates[hit.tag] == RING_STATE_SHOWN &&
                    simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                    PlaySound(SOUND_IMPACT_ON_MARK, RING_SOUND_CHANNEL, SOUND_FULL_SCALE);
                    if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                        g_Players[simulation->playerIndex].ringStates[hit.tag] = RING_STATE_PASSED;
                        StartRingPassRumble(&g_Players[simulation->playerIndex]);
                    }
                }

                if (simulation->state == BALL_STATE_ROLLING && simulation->isOverCup) {
                    s32 groundHeight = GetGroundHeight(startX, startZ);

                    if (groundHeight < startY) {
                        startY = groundHeight;
                    }

                    simulation->isOverCup = 0;
                }

                if (g_GameState.isMiniGolf) {
                    contact = FindMiniGolfGroundHit(
                        simulation,
                        startX,
                        startY,
                        startZ,
                        simulation->position[AXIS_X],
                        simulation->position[AXIS_Y],
                        simulation->position[AXIS_Z],
                        &hit,
                        &hitFraction
                    );
                    if (g_GameState.isMiniGolf && simulation->state == BALL_STATE_ROLLING) {
                        hitsBillboard = FindMiniGolfGroundHit(
                            simulation,
                            startX,
                            startY,
                            startZ,
                            simulation->position[AXIS_X],
                            simulation->position[AXIS_Y] - MINI_GOLF_ROLL_PROBE_RISE,
                            simulation->position[AXIS_Z],
                            &triangle,
                            &fraction
                        );
                        if (hitsBillboard && (contact == 0 || fraction < hitFraction)) {
                            contact = hitsBillboard;
                            osSyncPrintf("PutterSpecialGroundCollision\n");
                            hit = triangle;
                            hitFraction = fraction;
                        }
                    }
                }
                else {
                    endY = simulation->position[AXIS_Y];
                    if (simulation->state == BALL_STATE_ROLLING &&
                        GetGroundHeight(
                            simulation->position[AXIS_X],
                            simulation->position[AXIS_Z]
                        ) < startY) {
                        endY = startY;
                    }

                    contact = FindGroundHit(
                        startX,
                        startY,
                        startZ,
                        simulation->position[AXIS_X],
                        endY,
                        simulation->position[AXIS_Z],
                        &hit,
                        &hitFraction
                    );
                }

                holeHit = FALSE;
                hitsBillboard = FALSE;
                if (simulation->state == BALL_STATE_FLYING &&
                    (simulation->lastBounceStep < 0 ||
                        simulation->lastBounceStep + BILLBOARD_HIT_BOUNCE_GAP <
                            simulation->stepCount)) {
                    hitsBillboard = FindBillboardHit(
                        startX,
                        startY,
                        startZ,
                        simulation->position[AXIS_X],
                        simulation->position[AXIS_Y],
                        simulation->position[AXIS_Z],
                        &triangle.tag,
                        &fraction
                    );
                }

                if (hitsBillboard && contact == STEP_FLAG_GROUND_HIT) {
                    if (hitFraction <= fraction) {
                        triangle = hit;
                        fraction = hitFraction;
                        contact = STEP_FLAG_GROUND_HIT;
                    }
                    else {
                        contact = STEP_FLAG_BILLBOARD_HIT;
                    }
                }
                else if (hitsBillboard) {
                    contact = STEP_FLAG_BILLBOARD_HIT;
                }
                else if (contact == STEP_FLAG_GROUND_HIT) {
                    triangle = hit;
                    fraction = hitFraction;
                }
                else {
                    contact = 0;
                }

                if (!simulation->hasEnteredWater) {
                    if (simulation->state == BALL_STATE_FLYING ||
                        simulation->state == BALL_STATE_ROLLING) {
                        holeHit = FindHoleTriangleHit(
                            startX,
                            startY,
                            startZ,
                            simulation->position[AXIS_X],
                            simulation->position[AXIS_Y],
                            simulation->position[AXIS_Z],
                            &hit,
                            &hitFraction
                        );
                    }
                    else {
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
                PrintDebugPaddedHex(contact, CONTACT_DEBUG_TEXT_COLUMN, CONTACT_DEBUG_TEXT_ROW);
                osSyncPrintf("col%d\n", contact);
            }
        }

        if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
            CoursePoint pinVelocity;
            f32 pinFraction;
            s32 pinHit;

            if (g_GameState.isMiniGolf ||
                GetSurfaceInfo(simulation->launchSurface)->kind != SURFACE_KIND_GREEN) {
                pinHit = FindPinHit(
                    startX,
                    startY,
                    startZ,
                    simulation->position[AXIS_X],
                    simulation->position[AXIS_Y],
                    simulation->position[AXIS_Z],
                    pinVelocity,
                    &pinFraction,
                    simulation->hasHitFlag,
                    simulation
                );
                if (pinHit && !(simulation->previousStepFlags & STEP_FLAG_PIN_HIT)) {
                    simulation->bounceCount++;
                    simulation->position[AXIS_X] = startX + (pinFraction * velocity[AXIS_X]);
                    simulation->position[AXIS_Y] = startY + (pinFraction * velocity[AXIS_Y]);
                    simulation->position[AXIS_Z] = startZ + (pinFraction * velocity[AXIS_Z]);
                    if (pinHit == PIN_HIT_FLAG) {
                        if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                            PlaySound(SOUND_HIT_FLAG, g_NextSoundChannel, SOUND_FULL_SCALE);
                        }

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
                        BallSpeed speed = sqrtf(
                            (velocity[AXIS_X] * velocity[AXIS_X]) +
                            (velocity[AXIS_Z] * velocity[AXIS_Z])
                        );

                        if (speed > KilometersPerHour(1)) {
                            SoundVolume volume = speed * FLAGSTICK_SOUND_VOLUME_PER_SPEED;

                            if (volume >= SOUND_FULL_SCALE) {
                                volume = MAX_KNOCK_VOLUME;
                            }

                            if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                                PlaySound(SOUND_HIT_FLAGSTICK, g_NextSoundChannel, volume);
                            }
                        }
                    }

                    if (simulation->state == BALL_STATE_ROLLING) {
                        simulation->state = BALL_STATE_FLYING;
                        velocity[AXIS_Y] = -sqrtf(
                                               (velocity[AXIS_X] * velocity[AXIS_X]) +
                                               (velocity[AXIS_Z] * velocity[AXIS_Z])
                                           ) *
                            PIN_HIT_RISE_SCALE;
                        simulation->spinHeading = atan2(pinVelocity[AXIS_Z], pinVelocity[AXIS_X]);
                        contact = 0;
                        simulation->spinSpeed =
                            IntLength2D(pinVelocity[AXIS_X], pinVelocity[AXIS_Z]);
                    }
                    else {
                        velocity[AXIS_Y] = pinVelocity[AXIS_Y];
                        simulation->spinSpeed = simulation->spinSpeed * PIN_HIT_SPIN_KEPT;
                    }

                    velocity[AXIS_X] = pinVelocity[AXIS_X];
                    velocity[AXIS_Z] = pinVelocity[AXIS_Z];
                    if (simulation->purpose != LAUNCH_PURPOSE_SHOT_PREVIEW) {
                        f32 rest = PIN_HIT_STEP_SHARE - pinFraction;

                        simulation->position[AXIS_X] =
                            simulation->position[AXIS_X] + (rest * velocity[AXIS_X]);
                        simulation->position[AXIS_Y] =
                            simulation->position[AXIS_Y] + (rest * velocity[AXIS_Y]);
                        simulation->position[AXIS_Z] =
                            simulation->position[AXIS_Z] + (rest * velocity[AXIS_Z]);
                    }

                    flags |= STEP_FLAG_PIN_HIT;
                    MoveBallToCupEdge(simulation);
                    BuildLiftRotation();
                }
            }

            if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
                s32 cupOffsetX = simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X];

                if (Abs(cupOffsetX) < CUP_SEARCH_SPAN) {
                    s32 cupOffsetZ = simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z];

                    if (Abs(cupOffsetZ) < CUP_SEARCH_SPAN) {
                        s32 cupDistance = IntLength2D(cupOffsetX, cupOffsetZ);

                        if (simulation->state != BALL_STATE_IN_CUP &&
                            simulation->position[AXIS_Y] - g_HoleSetup.cupPosition[AXIS_Y] >=
                                -MAX_CUP_ENTRY_HEIGHT &&
                            (f32)cupDistance < CUP_RADIUS) {
                            s32 speed = IntLength2D(velocity[AXIS_X], velocity[AXIS_Z]);

                            if (speed < MAX_CUP_IN_SPEED &&
                                velocity[AXIS_Y] >= -MAX_CUP_IN_RISE_SPEED) {
                                if (printsDebugText) {
                                    SetDebugTextHighlight(1);
                                    PrintDebugText(
                                        "CUPIN!",
                                        CUP_DEBUG_TEXT_COLUMN,
                                        CUP_DEBUG_TEXT_ROW
                                    );
                                }

                                simulation->state = BALL_STATE_IN_CUP;
                                if (speed < MAX_CUP_IN_SOUND_SPEED) {
                                    simulation->soundStates[BALL_SOUND_CUP_IN] = CUP_IN_SOUND_DUE;
                                }
                            }
                        }

                        if ((f32)cupDistance < CUP_PULL_OUTER_RADIUS &&
                            (f32)cupDistance >= CUP_PULL_INNER_RADIUS) {
                            isNearCup = TRUE;
                            if (simulation->state == BALL_STATE_ROLLING &&
                                (f32)Abs(
                                    simulation->position[AXIS_Y] - g_HoleSetup.cupPosition[AXIS_Y]
                                ) < CUP_PULL_MAX_HEIGHT) {
                                f32 toCup[3];
                                f32 direction[3];
                                f32 alignment;
                                f32 depth;
                                Radians angle;
                                f32 pull;

                                if (printsDebugText) {
                                    SetDebugTextHighlight(1);
                                    PrintDebugText(
                                        "KURU!",
                                        CUP_DEBUG_TEXT_COLUMN,
                                        CUP_DEBUG_TEXT_ROW
                                    );
                                }

                                toCup[AXIS_X] =
                                    g_HoleSetup.cupPosition[AXIS_X] - simulation->position[AXIS_X];
                                toCup[AXIS_Y] = 0.0f;
                                toCup[AXIS_Z] =
                                    g_HoleSetup.cupPosition[AXIS_Z] - simulation->position[AXIS_Z];
                                guNormalize(&toCup[AXIS_X], &toCup[AXIS_Y], &toCup[AXIS_Z]);
                                direction[AXIS_X] = velocity[AXIS_X];
                                direction[AXIS_Y] = CUP_PULL_DIRECTION_TILT;
                                direction[AXIS_Z] = velocity[AXIS_Z];
                                guNormalize(
                                    &direction[AXIS_X],
                                    &direction[AXIS_Y],
                                    &direction[AXIS_Z]
                                );
                                alignment = (toCup[AXIS_X] * direction[AXIS_X]) +
                                    (toCup[AXIS_Z] * direction[AXIS_Z]);
                                if (alignment < 0.0f) {
                                    alignment = 0.0f;
                                }

                                depth = CUP_PULL_OUTER_RADIUS - (f32)cupDistance;
                                angle = atan2(toCup[AXIS_Z], toCup[AXIS_X]);
                                alignment = 1.0f - alignment;
                                if (depth > CUP_PULL_DEPTH_RANGE) {
                                    depth = CUP_PULL_DEPTH_RANGE;
                                }

                                pull = ((sinf(depth * ((f32)M_PI / CUP_PULL_DEPTH_RANGE)) *
                                            CUP_PULL_RISE) +
                                           MIN_CUP_PULL) *
                                    alignment;
                                velocity[AXIS_X] *= CUP_PULL_DRAG;
                                velocity[AXIS_Z] *= CUP_PULL_DRAG;
                                velocity[AXIS_X] += cosf(angle) * pull;
                                velocity[AXIS_Z] += sinf(angle) * pull;
                                flags |= STEP_FLAG_CUP_PULL;
                            }
                        }

                        if ((f32)cupDistance >= CUP_PULL_OUTER_RADIUS &&
                            (f32)IntLength2D(
                                startX - g_HoleSetup.cupPosition[AXIS_X],
                                startZ - g_HoleSetup.cupPosition[AXIS_Z]
                            ) < CUP_PULL_OUTER_RADIUS &&
                            (f32)Abs(startY - g_HoleSetup.cupPosition[AXIS_Y]) <
                                CUP_PULL_MAX_HEIGHT) {
                            f32 fromCup[3];
                            f32 direction[3];
                            BallSpeed speed;
                            BallSpeed outwardSpeed;
                            f32 alignment;

                            fromCup[AXIS_X] =
                                simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X];
                            fromCup[AXIS_Y] = 0.0f;
                            fromCup[AXIS_Z] =
                                simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z];
                            direction[AXIS_X] = velocity[AXIS_X];
                            direction[AXIS_Y] = 0.0f;
                            direction[AXIS_Z] = velocity[AXIS_Z];
                            speed = sqrtf(
                                (velocity[AXIS_X] * velocity[AXIS_X]) +
                                (velocity[AXIS_Z] * velocity[AXIS_Z])
                            );
                            guNormalize(&fromCup[AXIS_X], &fromCup[AXIS_Y], &fromCup[AXIS_Z]);
                            guNormalize(&direction[AXIS_X], &direction[AXIS_Y], &direction[AXIS_Z]);
                            outwardSpeed = (fromCup[AXIS_X] * velocity[AXIS_X]) +
                                (fromCup[AXIS_Z] * velocity[AXIS_Z]);
                            alignment = (direction[AXIS_X] * fromCup[AXIS_X]) +
                                (direction[AXIS_Z] * fromCup[AXIS_Z]);
                            osSyncPrintf(
                                "InnerMult %f, %f, %d\n",
                                alignment,
                                outwardSpeed,
                                simulation->state
                            );
                            if (alignment >= THROUGH_CUP_ALIGNMENT) {
                                osSyncPrintf("Through Cup\n");
                            }
                            else if (simulation->state == BALL_STATE_ROLLING &&
                                alignment >= MIN_LIP_OUT_ALIGNMENT) {
                                f32 keep = speed * (1.0f / LIP_OUT_FULL_SPEED);

                                if (keep < 0.0f) {
                                    keep = 0.0f;
                                }

                                if (keep > 1.0f) {
                                    keep = 1.0f;
                                }

                                velocity[AXIS_Y] =
                                    -speed * (sqrtf(1.0f - (keep * keep)) + LIP_OUT_EXTRA_RISE);
                                velocity[AXIS_X] *= keep;
                                velocity[AXIS_Z] *= keep;
                                simulation->state = BALL_STATE_FLYING;
                                simulation->spinHeading = atan2(velocity[AXIS_Z], velocity[AXIS_X]);
                                simulation->spinSpeed =
                                    IntLength2D(velocity[AXIS_X], velocity[AXIS_Z]) *
                                    LIP_OUT_SPIN_SCALE;
                                BuildLiftRotation();
                                contact = 0;
                                flags |= STEP_FLAG_LIPPED_OUT;
                                if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                                    StartFlagstickSparks(simulation->position);
                                }

                                PlayStrokeImpactSound(
                                    SOUND_HIT_CUP_WALL,
                                    FULL_VOLUME_LIP_OUT_SPEED,
                                    outwardSpeed
                                );
                            }
                        }
                    }
                }
            }
        }

        if (simulation->state == BALL_STATE_IN_WATER) {
            simulation->hasEnteredWater = TRUE;
            velocity[AXIS_X] *= WATER_DRAG;
            velocity[AXIS_Z] *= WATER_DRAG;
            velocity[AXIS_Y] = (velocity[AXIS_Y] + GRAVITY) * WATER_DRAG;
            if (simulation->position[AXIS_Y] <=
                GetGroundOrWaterHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z]) +
                    WATER_STOP_DEPTH) {
                if (simulation->position[AXIS_Y] >=
                    GetGroundHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z])) {
                    simulation->state = BALL_STATE_ROLLING;
                    simulation->rollFrameCount = 0;
                    goto SETTLE;
                }
            }
            else {
                simulation->state = BALL_STATE_STOPPED;
            }
        }

        if (simulation->state == BALL_STATE_ROLLING &&
            simulation->hasEnteredWater &&
            simulation->position[AXIS_Y] >
                GetGroundOrWaterHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z]) +
                    WATER_STOP_DEPTH) {
            simulation->state = BALL_STATE_STOPPED;
        }

        if (simulation->state == BALL_STATE_IN_CUP) {
            BallSpeed speed;
            f32 cupDistance;

            speed = sqrtf(
                (velocity[AXIS_X] * velocity[AXIS_X]) + (velocity[AXIS_Z] * velocity[AXIS_Z])
            );
            simulation->surface = SURFACE_GREEN;
            cupDistance = (s32)IntLength2D(
                simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
                simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]
            );
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
                push = ((toCupX * velocity[AXIS_X]) + (toCupY * velocity[AXIS_Y]) +
                           (toCupZ * velocity[AXIS_Z])) *
                    CUP_WALL_REFLECTION;
                scale = CUP_WALL_REBOUND_RADIUS / cupDistance;
                velocity[AXIS_X] -= toCupX * push;
                velocity[AXIS_Y] -= toCupY * push;
                velocity[AXIS_Z] -= toCupZ * push;
                simulation->position[AXIS_X] =
                    ((simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X]) * scale) +
                    g_HoleSetup.cupPosition[AXIS_X];
                simulation->position[AXIS_Z] =
                    ((simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]) * scale) +
                    g_HoleSetup.cupPosition[AXIS_Z];
                if (speed >= MIN_CUP_WALL_SOUND_SPEED) {
                    SoundVolume volume = speed * CUP_SOUND_VOLUME_PER_SPEED;

                    if (volume >= SOUND_FULL_SCALE) {
                        volume = MAX_KNOCK_VOLUME;
                    }

                    if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                        PlaySound(SOUND_HIT_CUP_WALL, g_NextSoundChannel, volume);
                    }
                }
            }

            if (simulation->position[AXIS_Y] <
                    g_HoleSetup.cupPosition[AXIS_Y] + CUP_CEILING_DEPTH &&
                velocity[AXIS_Y] < -MIN_CUP_CEILING_SPEED) {
                s32 toCupX = g_HoleSetup.cupPosition[AXIS_X] - simulation->position[AXIS_X];
                s32 toCupZ = g_HoleSetup.cupPosition[AXIS_Z] - simulation->position[AXIS_Z];

                if ((toCupX | toCupZ) != 0) {
                    s32 scale = AbsFloat(velocity[AXIS_Y]) / IntLength2D(toCupX, toCupZ);

                    velocity[AXIS_X] = toCupX * scale;
                    velocity[AXIS_Z] = toCupZ * scale;
                    simulation->position[AXIS_Y] =
                        g_HoleSetup.cupPosition[AXIS_Y] + CUP_CEILING_DEPTH;
                }

                velocity[AXIS_Y] = AbsFloat(velocity[AXIS_Y]);
            }

            if (simulation->position[AXIS_Y] >= g_HoleSetup.cupPosition[AXIS_Y] + CUP_DEPTH) {
                BallSpeed fallSpeed = velocity[AXIS_Y];

                /*
                 * The chained assignment is there to match: two statements, in
                 * either order, give 1122 words differing, 262 in shape, against 478
                 * and 126
                 */
                velocity[AXIS_X] = velocity[AXIS_Z] = 0.0f;
                velocity[AXIS_Y] = (-fallSpeed * CUP_FLOOR_BOUNCE) + CUP_GRAVITY;
                if (velocity[AXIS_Y] > 0.0f) {
                    simulation->isHoledOut = 1;
                    simulation->state = BALL_STATE_STOPPED;
                }

                if (simulation->soundStates[BALL_SOUND_CUP_IN] == CUP_IN_SOUND_DUE) {
                    if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                        PlaySound(SOUND_CUP_IN, g_NextSoundChannel, SOUND_FULL_SCALE);
                    }

                    simulation->soundStates[BALL_SOUND_CUP_IN] = CUP_IN_SOUND_PLAYED;
                }
                else if (simulation->soundStates[BALL_SOUND_CUP_IN] == CUP_IN_SOUND_NONE) {
                    if (fallSpeed >= MIN_CUP_FLOOR_SOUND_SPEED) {
                        SoundVolume volume = fallSpeed * CUP_SOUND_VOLUME_PER_SPEED;

                        if (volume > SOUND_FULL_SCALE) {
                            volume = SOUND_FULL_SCALE;
                        }

                        if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                            PlaySound(SOUND_HIT_CUP_BOTTOM, g_NextSoundChannel, volume);
                        }
                    }
                }

                simulation->position[AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y] + CUP_DEPTH;
            }
            else {
                velocity[AXIS_Y] += CUP_GRAVITY;
            }
        }

        if (contact == STEP_FLAG_BILLBOARD_HIT && simulation->state == BALL_STATE_FLYING) {
            simulation->bounceCount++;
            simulation->position[AXIS_X] = startX + (fraction * velocity[AXIS_X]);
            simulation->position[AXIS_Y] = startY + (fraction * velocity[AXIS_Y]);
            simulation->position[AXIS_Z] = startZ + (fraction * velocity[AXIS_Z]);
            simulation->lastBounceStep = simulation->stepCount;
            simulation->spinSpeed /= BILLBOARD_HIT_SPIN_DIVISOR;
            simulation->sideSpin /= BILLBOARD_HIT_SPIN_DIVISOR;
            switch (triangle.tag) {
                case BILLBOARD_CELL_TRUNK:
                case BILLBOARD_CELL_SOLID:
                case BILLBOARD_CELL_SHELL:
                    velocity[AXIS_X] = -velocity[AXIS_Z] * BILLBOARD_HIT_SPEED_KEPT;
                    velocity[AXIS_Z] = -velocity[AXIS_X] * BILLBOARD_HIT_SPEED_KEPT;
                    break;
                case BILLBOARD_CELL_FOLIAGE:
                default:
                    velocity[AXIS_X] *= BILLBOARD_HIT_SPEED_KEPT;
                    velocity[AXIS_Z] *= BILLBOARD_HIT_SPEED_KEPT;
                    break;
            }

            if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                PlayBillboardHitSound(
                    triangle.tag,
                    IntLength3D(velocity[AXIS_X], velocity[AXIS_Y], velocity[AXIS_Z])
                );
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
                endY = GetGroundHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
            }
            else {
                endY = simulation->position[AXIS_Y];
            }

            if (FindWallHit(
                    startX,
                    startY,
                    startZ,
                    simulation->position[AXIS_X],
                    endY,
                    simulation->position[AXIS_Z],
                    &wall,
                    &wallFraction
                ) == TRUE &&
                wall.tag == HOLE_TRIANGLE_KIND_SHADED) {
                f32 bounce = 1.0f;
                BallSpeed inwardSpeed;
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
                }
                else {
                    edgeX = wall.corners[AXIS_X][1];
                    edgeZ = wall.corners[AXIS_Z][1];
                }

                normalX -= edgeX;
                normalY -= edgeY;
                normalZ -= edgeZ;
                guNormalize(&normalX, &normalY, &normalZ);
                inwardSpeed = (-normalZ * velocity[AXIS_X]) + (normalY * velocity[AXIS_Y]) +
                    (normalX * velocity[AXIS_Z]);
                PlayStrokeImpactSound(
                    SOUND_HIT_WALL,
                    FULL_VOLUME_WALL_HIT_SPEED,
                    AbsFloat(inwardSpeed)
                );
                squareX = velocity[AXIS_X] * velocity[AXIS_X];
                squareZ = velocity[AXIS_Z] * velocity[AXIS_Z];
                along = wallFraction;
                if (along < 0.0f) {
                    along = 0.0f;
                }

                hitPosition[AXIS_X] = startX + (along * (velocity[AXIS_X] * WALL_HIT_STEP_SHARE));
                hitPosition[AXIS_Y] = startY + (along * (velocity[AXIS_Y] * WALL_HIT_STEP_SHARE));
                hitPosition[AXIS_Z] = startZ + ((velocity[AXIS_Z] * WALL_HIT_STEP_SHARE) * along);
                if (wallFraction < 0.0f && fabsf(inwardSpeed) < WALL_ESCAPE_SPEED) {
                    BallSpeed speed = sqrtf(squareX + squareZ);

                    if (inwardSpeed < 0.0f) {
                        inwardSpeed -= WALL_ESCAPE_SPEED;
                    }
                    else {
                        inwardSpeed += WALL_ESCAPE_SPEED;
                    }

                    bounce = speed / (speed + WALL_ESCAPE_SPEED);
                    osSyncPrintf("SpecialWallRefrect\n");
                }

                simulation->position[AXIS_X] = hitPosition[AXIS_X];
                simulation->position[AXIS_Y] = hitPosition[AXIS_Y];
                simulation->position[AXIS_Z] = hitPosition[AXIS_Z];
                if (simulation->purpose == LAUNCH_PURPOSE_STROKE &&
                    fabsf(inwardSpeed) > MIN_WALL_SPARK_SPEED) {
                    StartFlagstickSparks(simulation->position);
                }

                reflected[AXIS_X] = velocity[AXIS_X] - (-normalZ * (2.0f * inwardSpeed));
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
                simulation->position[AXIS_Z] =
                    simulation->position[AXIS_Z] + (velocity[AXIS_Z] * slide);
                simulation->spinSpeed = 0;
                BuildLiftRotation();
                PushBallOutOfWalls(simulation, FALSE);
                goto SETTLE;
            }

            PushBallOutOfWalls(simulation, FALSE);
        }

        if (!simulation->hasEnteredWater &&
            ((contact == STEP_FLAG_HOLE_TRIANGLE_HIT && simulation->state == BALL_STATE_FLYING) ||
                (simulation->state == BALL_STATE_ROLLING && hitsHoleTriangle))) {
            CoursePoint bounced;
            TrianglePlane plane;
            BallSimulation impact;
            BallSpeed impactSpeed;
            f32 steepness;
            s32 speed;
            f32 softness;
            f32 bounce;
            f32 push;
            f32 keep;
            f32 speedLoss;

            simulation->bounceCount++;
            GetTrianglePlane(&triangle, &plane);
            simulation->position[AXIS_X] =
                (fraction * (velocity[AXIS_X] * TRIANGLE_HIT_STEP_SHARE)) + startX;
            simulation->position[AXIS_Y] =
                startY + (fraction * (velocity[AXIS_Y] * TRIANGLE_HIT_STEP_SHARE));
            simulation->position[AXIS_Z] =
                startZ + ((velocity[AXIS_Z] * TRIANGLE_HIT_STEP_SHARE) * fraction);
            substepCount += (fraction < EARLY_HIT_FRACTION) & (substepCount == 1);
            if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
                simulation->surface = SURFACE_FAIRWAY;
            }
            else {
                simulation->surface = SURFACE_WATER_HAZARD;
                if (simulation->bounceCount == 1) {
                    simulation->firstBounceSurface = simulation->surface;
                }

                if (simulation->surface == SURFACE_GREEN ||
                    simulation->surface == SURFACE_GREEN_EDGE) {
                    plane.normal[AXIS_Y] =
                        (plane.normal[AXIS_Y] * (1.0f - GREEN_TRIANGLE_LEVELING)) +
                        GREEN_TRIANGLE_LEVELING;
                    guNormalize(
                        &plane.normal[AXIS_X],
                        &plane.normal[AXIS_Y],
                        &plane.normal[AXIS_Z]
                    );
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
            }
            else {
                steepness = 1.0f;
            }

            switch (GetSurfaceInfo(simulation->surface)->kind) {
                case SURFACE_KIND_GREEN:
                case SURFACE_KIND_GREEN_EDGE:
                    if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                        PlaySound(SOUND_LAND_GREEN, g_NextSoundChannel, SOUND_FULL_SCALE);
                    }

                    break;
                case SURFACE_KIND_FAIRWAY:
                case SURFACE_KIND_FAIRWAY_FAIR_LIE:
                case SURFACE_KIND_FAIRWAY_POOR_LIE:
                case SURFACE_KIND_SEMI_ROUGH:
                    if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                        PlaySound(SOUND_LAND_FAIRWAY, g_NextSoundChannel, SOUND_FULL_SCALE);
                    }

                    break;
                case SURFACE_KIND_ROUGH:
                case SURFACE_KIND_ROUGH_POOR_LIE:
                case SURFACE_KIND_ROUGH_UNUSED_LIE:
                case SURFACE_KIND_DEEP_ROUGH:
                    if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                        PlaySound(SOUND_LAND_ROUGH, g_NextSoundChannel, SOUND_FULL_SCALE);
                    }

                    break;
                case SURFACE_KIND_BUNKER:
                case SURFACE_KIND_BUNKER_FAIR_LIE:
                case SURFACE_KIND_BUNKER_POOR_LIE:
                case SURFACE_KIND_BUNKER_UNUSED_LIE:
                case SURFACE_KIND_BARE_GROUND:
                    if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                        PlaySound(SOUND_LAND_SAND, g_NextSoundChannel, SOUND_FULL_SCALE);
                    }

                    break;
                case SURFACE_KIND_WATER_HAZARD:
                    if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                        PlaySound(SOUND_LAND_WATER, g_NextSoundChannel, SOUND_FULL_SCALE);
                    }

                    break;
                case SURFACE_KIND_OUT_OF_BOUNDS:
                case SURFACE_KIND_CART_WAY:
                case SURFACE_KIND_TEE_GROUND:
                case SURFACE_KIND_ROCK:
                    if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                        PlaySound(SOUND_CONTACT_PLAIN, g_NextSoundChannel, SOUND_FULL_SCALE);
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
                    fabsf(impactSpeed) >= MAX_SKIP_IMPACT_SPEED ||
                    fabsf(impactSpeed) <= MIN_SKIP_IMPACT_SPEED ||
                    fabsf(impactSpeed) >=
                        sqrtf(
                            (velocity[AXIS_X] * velocity[AXIS_X]) +
                            (velocity[AXIS_Z] * velocity[AXIS_Z])
                        ) * SKIP_SPEED_RATIO)) {
                simulation->state = BALL_STATE_IN_WATER;
                impactSpeed = 0.0f;
                /*
                 * Twice, to match: under -ffast-math, fold-const reassociates one
                 * expression's two products into a product by 0.04f, where the
                 * original multiplies by 0.2f twice. As one expression, in any order
                 * or with the drags' product grouped, 492 words differ, 128 in shape,
                 * against 478 and 126.
                 */
                velocity[AXIS_X] *= WATER_ENTRY_DRAG;
                velocity[AXIS_X] *= WATER_ENTRY_DRAG;
            }

            softness = THOUSANDTHS_PER_ONE - GetSurfaceInfo(simulation->surface)->bounciness;
            bounce = 1.0f - (softness * steepness * ONE_PER_THOUSANDTH);
            push = (bounce + 1.0f) * impactSpeed;
            bounced[AXIS_X] = velocity[AXIS_X] - (plane.normal[AXIS_X] * push);
            bounced[AXIS_Y] = velocity[AXIS_Y] - (plane.normal[AXIS_Y] * push);
            bounced[AXIS_Z] = velocity[AXIS_Z] - (plane.normal[AXIS_Z] * push);
            speedLoss = steepness * SOFTNESS_SPEED_LOSS_SCALE;
            speedLoss = softness * speedLoss;
            keep = ((f32)THOUSANDTHS_PER_ONE - (speedLoss * steepness)) * ONE_PER_THOUSANDTH;
            if (simulation->state == BALL_STATE_IN_WATER) {
                keep = 0.0f;
            }

            velocity[AXIS_X] = bounced[AXIS_X] * keep;
            velocity[AXIS_Y] = bounced[AXIS_Y] * keep;
            velocity[AXIS_Z] = bounced[AXIS_Z] * keep;
            simulation->lastBounceStep = simulation->stepCount;
            if (simulation->state == BALL_STATE_FLYING) {
                s32 spinVelocityX = simulation->spinSpeed * cosf(simulation->spinHeading);
                s32 spinVelocityZ = simulation->spinSpeed * sinf(simulation->spinHeading);

                velocity[AXIS_X] += (spinVelocityX - velocity[AXIS_X]) * (1.0f - bounce) *
                    AbsFloat(plane.normal[AXIS_Y]) * TRIANGLE_SPIN_GRIP;
                velocity[AXIS_Z] += (spinVelocityZ - velocity[AXIS_Z]) * (1.0f - bounce) *
                    AbsFloat(plane.normal[AXIS_Y]) * TRIANGLE_SPIN_GRIP;
                spinVelocityX *= BOUNCE_SPIN_DECAY;
                spinVelocityZ *= BOUNCE_SPIN_DECAY;
                spinVelocityX =
                    spinVelocityX + ((previousVelocityX - spinVelocityX) * TRIANGLE_SPIN_FOLLOW);
                spinVelocityZ =
                    spinVelocityZ + ((previousVelocityZ - spinVelocityZ) * TRIANGLE_SPIN_FOLLOW);
                simulation->spinHeading = atan2(spinVelocityZ, spinVelocityX);
                simulation->spinSpeed = IntLength2D(spinVelocityX, spinVelocityZ);
                BuildLiftRotation();
            }

            simulation->sideSpin = 0;
            velocity[AXIS_Y] += GRAVITY;
        }

        if (simulation->state == BALL_STATE_FLYING && contact == STEP_FLAG_GROUND_HIT) {
            TrianglePlane plane;
            BallSpeed speed;
            s32 cupDistance;
            BallSpeed impactSpeed;
            f32 slide;
            SurfaceInfo *info;
            f32 friction;
            f32 spin;
            f32 restitution;

            simulation->bounceCount++;
            speed = sqrtf(
                (velocity[AXIS_X] * velocity[AXIS_X]) + (velocity[AXIS_Y] * velocity[AXIS_Y]) +
                (velocity[AXIS_Z] * velocity[AXIS_Z])
            );
            simulation->position[AXIS_X] = startX + (fraction * velocity[AXIS_X]);
            simulation->position[AXIS_Y] = startY + (fraction * velocity[AXIS_Y]);
            simulation->position[AXIS_Z] = startZ + (fraction * velocity[AXIS_Z]);
            cupDistance = IntLength2D(
                g_HoleSetup.cupPosition[AXIS_X] - simulation->position[AXIS_X],
                g_HoleSetup.cupPosition[AXIS_Z] - simulation->position[AXIS_Z]
            );
            if ((f32)cupDistance < CUP_PULL_OUTER_RADIUS &&
                (f32)cupDistance >= CUP_PULL_INNER_RADIUS) {
                f32 toCup[3];

                toCup[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X] - simulation->position[AXIS_X];
                toCup[AXIS_Y] = 1.0f;
                toCup[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z] - simulation->position[AXIS_Z];
                guNormalize(&toCup[AXIS_X], &toCup[AXIS_Y], &toCup[AXIS_Z]);
                plane.normal[AXIS_X] = toCup[AXIS_X];
                plane.normal[AXIS_Z] = toCup[AXIS_Z];
                plane.normal[AXIS_Y] = toCup[AXIS_Y] - CUP_PULL_TILT;
                guNormalize(&plane.normal[AXIS_X], &plane.normal[AXIS_Y], &plane.normal[AXIS_Z]);
                if ((plane.normal[AXIS_X] * velocity[AXIS_X]) +
                            (plane.normal[AXIS_Z] * velocity[AXIS_Z]) >=
                        0.0f ||
                    sqrtf(
                        (velocity[AXIS_X] * velocity[AXIS_X]) +
                        (velocity[AXIS_Z] * velocity[AXIS_Z])
                    ) > fabsf(velocity[AXIS_Y]) * MAX_CUP_PULL_SLIDE_RATIO) {
                    GetTrianglePlane(&triangle, &plane);
                }

                osSyncPrintf("Baki\n");
            }
            else {
                GetTrianglePlane(&triangle, &plane);
            }

            substepCount += (fraction < EARLY_HIT_FRACTION) & (substepCount == 1);
            if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST &&
                speed >= KilometersPerHour(5) &&
                !g_GameState.isMiniGolf) {
                f32 side[3];

                side[AXIS_Y] = 0.0f;
                side[AXIS_X] = velocity[AXIS_X];
                side[AXIS_Z] = velocity[AXIS_Z];
                if (fabsf(velocity[AXIS_X]) + fabsf(velocity[AXIS_Z]) >= KilometersPerHour(5)) {
                    f32 tilt = (GetStepNoise() - NOISE_MIDDLE) * LANDING_TILT_NOISE_SCALE;

                    if (tilt < 0.0f) {
                        tilt -= MIN_LANDING_TILT;
                    }
                    else {
                        tilt += MIN_LANDING_TILT;
                    }

                    guNormalize(&side[AXIS_X], &side[AXIS_Y], &side[AXIS_Z]);
                    side[AXIS_X] *= tilt;
                    side[AXIS_Z] *= tilt;
                    plane.normal[AXIS_X] -= side[AXIS_Z];
                    plane.normal[AXIS_Z] += side[AXIS_X];
                    guNormalize(
                        &plane.normal[AXIS_X],
                        &plane.normal[AXIS_Y],
                        &plane.normal[AXIS_Z]
                    );
                }
            }

            if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
                simulation->surface = SURFACE_FAIRWAY;
            }
            else {
                simulation->surface =
                    GetSurface(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
                if (simulation->bounceCount == 1) {
                    simulation->firstBounceSurface = simulation->surface;
                }
            }

            impactSpeed = (plane.normal[AXIS_X] * velocity[AXIS_X]) +
                (plane.normal[AXIS_Y] * velocity[AXIS_Y]) +
                (plane.normal[AXIS_Z] * velocity[AXIS_Z]);
            if (speed >= MIN_SLIDE_SPEED) {
                f32 cosine = fabsf(impactSpeed) / speed;

                if (cosine < 1.0f) {
                    slide = sqrtf(1.0f - (cosine * cosine));
                }
                else {
                    slide = 0.0f;
                }
            }
            else {
                slide = STILL_BALL_SLIDE;
            }

            switch (GetSurfaceInfo(simulation->surface)->kind) {
                case SURFACE_KIND_GREEN:
                case SURFACE_KIND_GREEN_EDGE:
                    PlayStrokeImpactSound(SOUND_LAND_GREEN, FULL_VOLUME_HIT_SPEED, impactSpeed);
                    break;
                case SURFACE_KIND_FAIRWAY:
                case SURFACE_KIND_FAIRWAY_FAIR_LIE:
                case SURFACE_KIND_FAIRWAY_POOR_LIE:
                case SURFACE_KIND_SEMI_ROUGH:
                    PlayStrokeImpactSound(SOUND_LAND_FAIRWAY, FULL_VOLUME_HIT_SPEED, impactSpeed);
                    break;
                case SURFACE_KIND_BUNKER:
                case SURFACE_KIND_BUNKER_FAIR_LIE:
                case SURFACE_KIND_BUNKER_POOR_LIE:
                case SURFACE_KIND_BUNKER_UNUSED_LIE:
                case SURFACE_KIND_BARE_GROUND:
                case SURFACE_KIND_WASTE_AREA:
                    PlayStrokeImpactSound(SOUND_LAND_SAND, FULL_VOLUME_HIT_SPEED, impactSpeed);
                    break;
                case SURFACE_KIND_ROUGH:
                case SURFACE_KIND_ROUGH_POOR_LIE:
                case SURFACE_KIND_ROUGH_UNUSED_LIE:
                case SURFACE_KIND_OUT_OF_BOUNDS:
                case SURFACE_KIND_CART_WAY:
                case SURFACE_KIND_TEE_GROUND:
                case SURFACE_KIND_ROCK:
                case SURFACE_KIND_DEEP_ROUGH:
                    PlayStrokeImpactSound(SOUND_LAND_ROUGH, FULL_VOLUME_HIT_SPEED, impactSpeed);
                    break;
            }

            if (simulation->purpose == LAUNCH_PURPOSE_STROKE && !simulation->hasEnteredWater) {
                SpawnImpactEffects(simulation);
            }

            info = GetSurfaceInfo(simulation->surface);
            friction = (THOUSANDTHS_PER_ONE - info->bounciness) * ONE_PER_THOUSANDTH;
            if (simulation->surface == SURFACE_BUNKER) {
                BunkerBurial burial = BUNKER_BURIAL_NONE;

                if (impactSpeed >= POOR_LIE_BUNKER_SPEED) {
                    burial = BUNKER_BURIAL_MOSTLY;
                }
                else if (impactSpeed >= FAIR_LIE_BUNKER_SPEED) {
                    burial = BUNKER_BURIAL_HALF;
                }

                switch (burial) {
                    case BUNKER_BURIAL_HALF:
                        simulation->surface = SURFACE_BUNKER_FAIR_LIE;
                        simulation->position[AXIS_Y] = GetGroundHeight(
                            simulation->position[AXIS_X],
                            simulation->position[AXIS_Z]
                        );
                        simulation->state = BALL_STATE_STOPPED;
                        break;
                    case BUNKER_BURIAL_MOSTLY:
                        simulation->surface = SURFACE_BUNKER_POOR_LIE;
                        simulation->position[AXIS_Y] = GetGroundHeight(
                            simulation->position[AXIS_X],
                            simulation->position[AXIS_Z]
                        );
                        simulation->state = BALL_STATE_STOPPED;
                        break;
                }
            }
            else if (info->kind == SURFACE_KIND_OUT_OF_COURSE ||
                ((info->kind == SURFACE_KIND_OUT_OF_BOUNDS ||
                     info->kind == SURFACE_KIND_WATER_HAZARD) &&
                    impactSpeed < LOST_BALL_STOP_SPEED)) {
                simulation->state = BALL_STATE_STOPPED;
            }

            switch (GetSurfaceInfo(simulation->surface)->kind) {
                case SURFACE_KIND_GREEN:
                case SURFACE_KIND_GREEN_EDGE:
                    friction *=
                        ((fabsf(
                              impactSpeed + (impactSpeed * GetPositionNoise() * LANDING_NOISE_SCALE)
                          ) * GREEN_LANDING_FRICTION_PER_SPEED) +
                            GREEN_LANDING_FRICTION_BASE);
                    if (friction > MAX_GREEN_LANDING_FRICTION) {
                        friction = MAX_GREEN_LANDING_FRICTION;
                    }

                    friction -= GetStepNoise() * LANDING_NOISE_SCALE;
                    if (friction < MIN_GREEN_LANDING_FRICTION) {
                        friction = MIN_GREEN_LANDING_FRICTION;
                    }

                    spin = 1.0f -
                        (sqrtf(
                             (speed * speed) -
                             (impactSpeed * (1.0f - friction) * impactSpeed * (1.0f - friction))
                         ) / (speed * slide));
                    if (spin > MAX_GRASS_LANDING_SPIN) {
                        spin = MAX_GRASS_LANDING_SPIN;
                    }

                    if (spin < MIN_GRASS_LANDING_SPIN) {
                        spin = MIN_GRASS_LANDING_SPIN;
                    }

                    break;
                case SURFACE_KIND_FAIRWAY:
                case SURFACE_KIND_FAIRWAY_FAIR_LIE:
                case SURFACE_KIND_FAIRWAY_POOR_LIE:
                case SURFACE_KIND_SEMI_ROUGH:
                    friction *=
                        ((fabsf(
                              impactSpeed + (impactSpeed * GetPositionNoise() * LANDING_NOISE_SCALE)
                          ) * FAIRWAY_LANDING_FRICTION_PER_SPEED) +
                            LANDING_FRICTION_BASE);
                    if (friction > 1.0f) {
                        friction = 1.0f;
                    }

                    spin = 1.0f -
                        (sqrtf(
                             (speed * speed) -
                             (impactSpeed * (1.0f - friction) * impactSpeed * (1.0f - friction))
                         ) / (speed * slide));
                    if (spin > MAX_GRASS_LANDING_SPIN) {
                        spin = MAX_GRASS_LANDING_SPIN;
                    }

                    if (spin < MIN_GRASS_LANDING_SPIN) {
                        spin = MIN_GRASS_LANDING_SPIN;
                    }

                    break;
                case SURFACE_KIND_BUNKER:
                case SURFACE_KIND_BUNKER_FAIR_LIE:
                case SURFACE_KIND_BUNKER_POOR_LIE:
                    friction *=
                        ((fabsf(impactSpeed) * OTHER_LANDING_FRICTION_PER_SPEED) +
                            LANDING_FRICTION_BASE);
                    spin = BUNKER_LANDING_SPIN;
                    friction += friction * GetPositionNoise() * LANDING_NOISE_SCALE;
                    break;
                case SURFACE_KIND_ROUGH:
                case SURFACE_KIND_ROUGH_POOR_LIE:
                case SURFACE_KIND_ROUGH_UNUSED_LIE:
                    friction *=
                        ((fabsf(impactSpeed) * OTHER_LANDING_FRICTION_PER_SPEED) +
                            LANDING_FRICTION_BASE);
                    spin = ROUGH_LANDING_SPIN;
                    friction += friction * GetPositionNoise() * LANDING_NOISE_SCALE;
                    break;
                default:
                    friction *=
                        ((fabsf(impactSpeed) * OTHER_LANDING_FRICTION_PER_SPEED) +
                            LANDING_FRICTION_BASE);
                    spin = OTHER_LANDING_SPIN;
                    friction += friction * GetPositionNoise() * LANDING_NOISE_SCALE;
                    break;
            }

            if (g_GameState.isMiniGolf) {
                spin = 0.0f;
            }

            restitution = 1.0f - friction;
            if (restitution < 0.0f) {
                restitution = 0.0f;
            }
            else if (restitution > MAX_RESTITUTION) {
                restitution = MAX_RESTITUTION;
            }

            if (simulation->state != BALL_STATE_ROLLING &&
                impactSpeed < MAX_ROLL_START_SPEED &&
                simulation->stepCount - 1 == simulation->lastBounceStep) {
                s32 spinVelocityX;
                s32 spinVelocityZ;

                simulation->state = BALL_STATE_ROLLING;
                simulation->rollFrameCount = 0;
                if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
                    simulation->position[AXIS_Y] = 0;
                }
                else {
                    simulation->position[AXIS_Y] =
                        GetGroundHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
                }

                spinVelocityX = simulation->spinSpeed * cosf(simulation->spinHeading);
                spinVelocityZ = simulation->spinSpeed * sinf(simulation->spinHeading);
                velocity[AXIS_X] += (spinVelocityX - velocity[AXIS_X]) *
                    fabsf(plane.normal[AXIS_Y]) * GROUND_SPIN_GRIP;
                velocity[AXIS_Z] += (spinVelocityZ - velocity[AXIS_Z]) *
                    fabsf(plane.normal[AXIS_Y]) * GROUND_SPIN_GRIP;
                simulation->spinSpeed = 0;
            }
            else {
                f32 keep = 1.0f - spin;
                f32 normalPart[3];
                f32 tangent[3];
                s32 spinVelocityX;
                s32 spinVelocityZ;
                s32 driftX;
                s32 driftZ;
                f32 driftLimit;
                f32 drift;

                if (keep <= MIN_BOUNCE_SPEED_KEPT) {
                    keep = MIN_BOUNCE_SPEED_KEPT;
                }

                if (simulation->stepCount < EARLY_BOUNCE_STEP_COUNT && !g_GameState.isMiniGolf) {
                    keep *= EARLY_BOUNCE_SPEED_SCALE;
                }

                normalPart[AXIS_X] = -plane.normal[AXIS_X] * impactSpeed;
                normalPart[AXIS_Y] = -plane.normal[AXIS_Y] * impactSpeed;
                normalPart[AXIS_Z] = -plane.normal[AXIS_Z] * impactSpeed;
                tangent[AXIS_X] = velocity[AXIS_X] + normalPart[AXIS_X];
                tangent[AXIS_Y] = velocity[AXIS_Y] + normalPart[AXIS_Y];
                tangent[AXIS_Z] = velocity[AXIS_Z] + normalPart[AXIS_Z];
                velocity[AXIS_X] = (tangent[AXIS_X] * keep) + (normalPart[AXIS_X] * restitution);
                velocity[AXIS_Y] = (tangent[AXIS_Y] * keep) + (normalPart[AXIS_Y] * restitution);
                velocity[AXIS_Z] = (tangent[AXIS_Z] * keep) + (normalPart[AXIS_Z] * restitution);
                simulation->lastBounceStep = simulation->stepCount;
                spinVelocityX = simulation->spinSpeed * cosf(simulation->spinHeading);
                spinVelocityZ = simulation->spinSpeed * sinf(simulation->spinHeading);
                driftX = (spinVelocityX - velocity[AXIS_X]) *
                    ((1.0f - restitution) * fabsf(plane.normal[AXIS_Y]) * GROUND_SPIN_GRIP);
                driftZ = (spinVelocityZ - velocity[AXIS_Z]) *
                    ((1.0f - restitution) * fabsf(plane.normal[AXIS_Y]) * GROUND_SPIN_GRIP);
                drift = (s32)IntLength2D(driftX, driftZ);
                driftLimit = impactSpeed * MAX_SPIN_DRIFT_SHARE;
                if (driftLimit < drift && impactSpeed > 1.0f) {
                    f32 scale = driftLimit / drift;

                    driftX = driftX * scale;
                    driftZ = driftZ * scale;
                }

                spinVelocityX *= BOUNCE_SPIN_DECAY;
                spinVelocityZ *= BOUNCE_SPIN_DECAY;
                velocity[AXIS_X] += driftX;
                velocity[AXIS_Z] += driftZ;
                spinVelocityX =
                    spinVelocityX + ((previousVelocityX - spinVelocityX) * GROUND_SPIN_FOLLOW);
                spinVelocityZ =
                    spinVelocityZ + ((previousVelocityZ - spinVelocityZ) * GROUND_SPIN_FOLLOW);
                simulation->spinHeading = atan2(spinVelocityZ, spinVelocityX);
                simulation->spinSpeed = IntLength2D(spinVelocityX, spinVelocityZ);
                BuildLiftRotation();
                simulation->sideSpin = 0;
                velocity[AXIS_Y] += GRAVITY;
            }
        }

        if (!simulation->hasEnteredWater &&
            simulation->state == BALL_STATE_ROLLING &&
            contact == 0 &&
            !isNearCup) {
            simulation->state = BALL_STATE_FLYING;
            simulation->spinHeading = atan2(velocity[AXIS_Z], velocity[AXIS_X]);
            simulation->spinSpeed = IntLength2D(velocity[AXIS_X], velocity[AXIS_Z]);
            BuildLiftRotation();
        }

        if (simulation->hasEnteredWater &&
            simulation->state == BALL_STATE_ROLLING &&
            contact == 0 &&
            FindGroundTriangle(
                simulation->position[AXIS_X],
                simulation->position[AXIS_Z],
                &triangle
            ) == 0) {
            contact = STEP_FLAG_GROUND_HIT;
        }

        if (simulation->state == BALL_STATE_ROLLING &&
            !isNearCup &&
            contact == STEP_FLAG_GROUND_HIT) {
            TrianglePlane plane;
            SurfaceInfo *info;
            s32 groundHeight;
            s32 slopeX;
            s32 slopeZ;
            f32 friction;
            BallSpeed speed;

            isSteep = FALSE;
            rollSpeed = sqrtf(
                (velocity[AXIS_X] * velocity[AXIS_X]) + (velocity[AXIS_Z] * velocity[AXIS_Z])
            );
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
            }
            else {
                GetTrianglePlane(&triangle, &plane);
                if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST &&
                    !simulation->hasEnteredWater) {
                    BallSpeed pressing = (plane.normal[AXIS_X] * velocity[AXIS_X]) +
                        (plane.normal[AXIS_Y] * velocity[AXIS_Y]) +
                        (plane.normal[AXIS_Z] * velocity[AXIS_Z]);

                    if ((g_GameState.isMiniGolf && pressing >= MINI_GOLF_ROLL_JUMP_SPEED) ||
                        pressing >= ROLL_JUMP_SPEED) {
                        f32 along[3];
                        f32 damping;

                        damping = 1.0f;
                        osSyncPrintf("Jump\n");
                        if (!g_GameState.isMiniGolf) {
                            damping = ROLL_JUMP_SPEED_KEPT;
                        }

                        along[AXIS_X] = velocity[AXIS_X] - (plane.normal[AXIS_X] * pressing);
                        along[AXIS_Y] = velocity[AXIS_Y] - (plane.normal[AXIS_Y] * pressing);
                        along[AXIS_Z] = velocity[AXIS_Z] - (plane.normal[AXIS_Z] * pressing);
                        simulation->position[AXIS_X] = startX + (fraction * velocity[AXIS_X]);
                        simulation->position[AXIS_Y] = startY + (fraction * velocity[AXIS_Y]);
                        simulation->position[AXIS_Z] = startZ + (fraction * velocity[AXIS_Z]);
                        velocity[AXIS_X] = along[AXIS_X] * damping;
                        velocity[AXIS_Y] = along[AXIS_Y] * damping;
                        velocity[AXIS_Z] = along[AXIS_Z] * damping;
                    }
                }

                if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST &&
                    !g_GameState.isMiniGolf &&
                    fabsf(velocity[AXIS_X]) + fabsf(velocity[AXIS_Z]) >= KilometersPerHour(5)) {
                    f32 tilt = (GetStepNoise() - NOISE_MIDDLE) * ROLL_TILT_NOISE_SCALE;

                    plane.normal[AXIS_X] -= velocity[AXIS_Z] * (1.0f / ROLL_TILT_FULL_SPEED) * tilt;
                    plane.normal[AXIS_Z] += velocity[AXIS_X] * (1.0f / ROLL_TILT_FULL_SPEED) * tilt;
                    guNormalize(
                        &plane.normal[AXIS_X],
                        &plane.normal[AXIS_Y],
                        &plane.normal[AXIS_Z]
                    );
                }

                simulation->surface =
                    GetSurface(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
                switch (simulation->surface) {
                    case SURFACE_FAIRWAY:
                        if (!g_GameState.isMiniGolf) {
                            f32 lie = GetStepNoise();

                            if (lie < FAIRWAY_POOR_LIE_CHANCE) {
                                simulation->surface = SURFACE_FAIRWAY_POOR_LIE;
                            }
                            else if (lie < FAIRWAY_POOR_LIE_CHANCE + FAIRWAY_FAIR_LIE_CHANCE) {
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

                info = GetSurfaceInfo(simulation->surface);
                if (info->kind == SURFACE_KIND_OUT_OF_COURSE) {
                    simulation->state = BALL_STATE_STOPPED;
                    goto ROLLED;
                }

                groundHeight =
                    GetGroundHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
                if (!g_GameState.isMiniGolf &&
                    groundHeight < simulation->position[AXIS_Y] - BURIED_BALL_DEPTH) {
                    simulation->position[AXIS_X] = startX;
                    simulation->position[AXIS_Y] = startY;
                    simulation->position[AXIS_Z] = startZ;
                    velocity[AXIS_X] *= BURIED_BALL_SPEED_SCALE;
                    velocity[AXIS_Y] *= BURIED_BALL_SPEED_SCALE;
                    velocity[AXIS_Z] *= BURIED_BALL_SPEED_SCALE;
                    simulation->state = BALL_STATE_FLYING;
                    goto TAKE_OFF;
                }

                if (simulation->position[AXIS_Y] + TAKE_OFF_HEIGHT < groundHeight) {
                    simulation->state = BALL_STATE_FLYING;

TAKE_OFF:

                    simulation->spinHeading = atan2(velocity[AXIS_Z], velocity[AXIS_X]);
                    contact = 0;
                    simulation->spinSpeed = IntLength2D(velocity[AXIS_X], velocity[AXIS_Z]);
                    BuildLiftRotation();
                    goto ROLLED;
                }
            }

            slopeX =
                (-plane.normal[AXIS_X] * SLOPE_ACCELERATION) / AbsoluteValue(plane.normal[AXIS_Y]);
            slopeZ =
                (-plane.normal[AXIS_Z] * SLOPE_ACCELERATION) / AbsoluteValue(plane.normal[AXIS_Y]);
            if (fabsf((f32)slopeX) + fabsf((f32)slopeZ) >= MAX_SLOPE_ACCELERATION) {
                f32 scale =
                    MAX_SLOPE_ACCELERATION / sqrtf(((f32)slopeX * slopeX) + ((f32)slopeZ * slopeZ));

                slopeX = slopeX * scale;
                slopeZ = slopeZ * scale;
            }

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

            if (simulation->hasEnteredWater &&
                simulation->rollFrameCount < WATER_EXIT_ROLL_FRAMES) {
                slopeX *= WATER_EXIT_SLOPE_SCALE;
                slopeZ *= WATER_EXIT_SLOPE_SCALE;
                isSteep = TRUE;
            }
            else if (isSteep) {
                slopeX *= STEEP_SLOPE_SCALE;
                slopeZ *= STEEP_SLOPE_SCALE;
                if (friction < MIN_STEEP_FRICTION) {
                    friction = MIN_STEEP_FRICTION;
                }
            }
            else {
                if ((f32)rollSpeed < SLOW_ROLL_SPEED) {
                    f32 scale =
                        (rollSpeed * SLOW_ROLL_SLOPE_SCALE_PER_SPEED) + MIN_SLOW_ROLL_SLOPE_SCALE;

                    slopeX = slopeX * scale;
                    slopeZ = slopeZ * scale;
                }

                if (simulation->stepCount >= LONGEST_ROLL_STEP_COUNT) {
                    friction *= LONGEST_ROLL_FRICTION_SCALE;
                }
                else if (simulation->stepCount >= LONGER_ROLL_STEP_COUNT) {
                    friction *= LONGER_ROLL_FRICTION_SCALE;
                }
                else if (simulation->stepCount >= LONG_ROLL_STEP_COUNT) {
                    friction *= LONG_ROLL_FRICTION_SCALE;
                }
            }

            speed = sqrtf(
                (velocity[AXIS_X] * velocity[AXIS_X]) + (velocity[AXIS_Z] * velocity[AXIS_Z])
            );
            if (speed > MIN_SLOPE_STEER_SPEED) {
                f32 against = -(
                    (((f32)slopeX * velocity[AXIS_X]) + ((f32)slopeZ * velocity[AXIS_Z])) / speed
                );

                slopeX = slopeX + (against * ((slopeX / speed) * SLOPE_STEER_SCALE));
                slopeZ = slopeZ + (against * ((slopeZ / speed) * SLOPE_STEER_SCALE));
            }
            else {
                slopeX = slopeX * STILL_BALL_SLOPE_SCALE;
                slopeZ = slopeZ * STILL_BALL_SLOPE_SCALE;
            }

            velocity[AXIS_X] =
                (velocity[AXIS_X] * friction) + (slopeX * friction * SLOPE_PULL_SCALE);
            velocity[AXIS_Z] =
                (slopeZ * friction * SLOPE_PULL_SCALE) + (velocity[AXIS_Z] * friction);
            if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST && g_GameState.isMiniGolf) {
                if ((f32)(groundHeight - startY) < velocity[AXIS_Y]) {
                    velocity[AXIS_Y] = groundHeight - startY + (s32)MINI_GOLF_ROLL_GRAVITY;
                }
                else {
                    velocity[AXIS_Y] += MINI_GOLF_ROLL_GRAVITY;
                }
            }
            else {
                if ((f32)(groundHeight - startY) < velocity[AXIS_Y]) {
                    velocity[AXIS_Y] = groundHeight - startY + (s32)GRAVITY;
                }
                else {
                    velocity[AXIS_Y] += GRAVITY;
                }
            }

            if (simulation->purpose == LAUNCH_PURPOSE_STROKE &&
                IsGameFlagSet(GAME_FLAG_LOG_VERTICAL_VELOCITY)) {
                osSyncPrintf("V[1] %d\n", velocity[AXIS_Y]);
            }

            if (simulation->rollFrameCount >= MIN_STOP_ROLL_FRAMES) {
                s32 speed = IntLength2D(velocity[AXIS_X], velocity[AXIS_Z]);

                if (simulation->rollFrameCount >= WATER_STOP_ROLL_FRAMES &&
                    simulation->hasEnteredWater) {
                    simulation->state = BALL_STATE_STOPPED;
                }
                else if (speed < MAX_STOP_SPEED) {
                    if (!isSteep && IntLength2D(slopeX, slopeZ) * friction < MAX_STOP_SLOPE_PULL) {
                        simulation->state = BALL_STATE_STOPPED;
                    }
                }
            }

            if (!isSteep &&
                info->kind != SURFACE_KIND_GREEN &&
                simulation->lastBounceStep + OFF_GREEN_STOP_STEPS < simulation->stepCount &&
                sqrtf(
                    (velocity[AXIS_X] * velocity[AXIS_X]) + (velocity[AXIS_Y] * velocity[AXIS_Y]) +
                    (velocity[AXIS_Z] * velocity[AXIS_Z])
                ) < (f64)MAX_OFF_GREEN_STOP_SPEED) {
                simulation->state = BALL_STATE_STOPPED;
            }
        }

ROLLED:

        if ((simulation->state == BALL_STATE_FLYING || simulation->state == BALL_STATE_ROLLING) &&
            contact == 0) {
            {
                f32 relative[3];
                BallSpeed airSpeed;
                f32 drag;
                f32 sideSpin;
                BallSpeed horizontalSpeed;
                f32 sideSpinMagnitude;
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
                airSpeed = sqrtf(
                    (relative[AXIS_X] * relative[AXIS_X]) + (relative[AXIS_Y] * relative[AXIS_Y]) +
                    (relative[AXIS_Z] * relative[AXIS_Z])
                );
                drag = (sqrtf(airSpeed * AIR_DRAG_SPEED_SCALE) * AIR_DRAG_PER_ROOT_SPEED) +
                    MIN_AIR_DRAG;
                sideSpin = simulation->sideSpin;
                horizontalSpeed = sqrtf(
                    (relative[AXIS_X] * relative[AXIS_X]) + (relative[AXIS_Z] * relative[AXIS_Z])
                );
                sideSpinMagnitude = fabsf(sideSpin * SIDE_SPIN_SPEED_SCALE);
                if (sideSpinMagnitude != 0.0f) {
                    ratio = horizontalSpeed / sideSpinMagnitude;
                }
                else {
                    ratio = 0.0f;
                }

                weight = 1.0f - (fabsf(ratio - 1.0f) * SIDE_SPIN_MISMATCH_FALLOFF);
                if (weight < 0.0f) {
                    weight = 0.0f;
                }

                curve = sideSpin * (weight * CURVE_PER_SIDE_SPIN);
                if (horizontalSpeed < fabsf(curve) * MAX_CURVE_SPEED_RATIO) {
                    curve = horizontalSpeed * (1.0f / MAX_CURVE_SPEED_RATIO);
                    if (simulation->sideSpin < 0) {
                        curve = -curve;
                    }
                }

                if (horizontalSpeed > MIN_CURVE_SPEED) {
                    f32 scale = (1.0f / horizontalSpeed) * curve;

                    velocity[AXIS_X] += -relative[AXIS_Z] * scale;
                    velocity[AXIS_Z] += relative[AXIS_X] * scale;
                }

                simulation->sideSpin = simulation->sideSpin * SIDE_SPIN_DECAY;
                lift = simulation->spinSpeed * LIFT_PER_SPIN;
                if (simulation->spinSpeed != 0 && airSpeed > MIN_LIFT_AIR_SPEED) {
                    f32 turned[3];
                    f32 scale;

                    guMtxXFMF(
                        simulation->liftRotation,
                        relative[AXIS_X],
                        relative[AXIS_Y],
                        relative[AXIS_Z],
                        &turned[AXIS_X],
                        &turned[AXIS_Y],
                        &turned[AXIS_Z]
                    );
                    if (airSpeed < fabsf(lift) * MAX_LIFT_SPEED_RATIO) {
                        lift = airSpeed * (1.0f / MAX_LIFT_SPEED_RATIO);
                    }

                    scale = lift /
                        sqrtf(
                            (turned[AXIS_X] * turned[AXIS_X]) + (turned[AXIS_Y] * turned[AXIS_Y]) +
                            (turned[AXIS_Z] * turned[AXIS_Z])
                        );
                    turned[AXIS_X] *= scale;
                    turned[AXIS_Y] *= scale;
                    turned[AXIS_Z] *= scale;
                    velocity[AXIS_X] += turned[AXIS_X];
                    velocity[AXIS_Y] += turned[AXIS_Y];
                    velocity[AXIS_Z] += turned[AXIS_Z];
                    simulation->spinSpeed = simulation->spinSpeed * SPIN_DECAY;
                }

                height = simulation->startPosition[AXIS_Y] - simulation->position[AXIS_Y];
                if (height <= 0.0f) {
                    windX = simulation->windVelocityX * LOW_WIND_SCALE;
                    windZ = simulation->windVelocityZ * LOW_WIND_SCALE;
                }
                else {
                    f32 scale = (height * WIND_SCALE_PER_HEIGHT) + LOW_WIND_SCALE;

                    windX = simulation->windVelocityX * scale;
                    windZ = simulation->windVelocityZ * scale;
                }

                velocity[AXIS_X] += (windX - velocity[AXIS_X]) * drag;
                velocity[AXIS_Y] += (-velocity[AXIS_Y] * drag) + GRAVITY;
                velocity[AXIS_Z] += (windZ - velocity[AXIS_Z]) * drag;
            }
        }

        if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
            Boolean isOutside = simulation->position[AXIS_Z] > GROUND_LENGTH - COURSE_EDGE_MARGIN;

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

        if (simulation->purpose != LAUNCH_PURPOSE_SHOT_PREVIEW) {
            s32 floor;

            if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
                floor = 0;
            }
            else if (simulation->state == BALL_STATE_IN_WATER) {
                floor = GetGroundHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
            }
            else {
                floor = GetGroundHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
                if (simulation->state == BALL_STATE_IN_CUP || simulation->isHoledOut) {
                    floor += IN_CUP_FLOOR_DEPTH;
                }
                else if (simulation->state == BALL_STATE_ROLLING) {
                    s32 cupOffsetX = simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X];

                    if (Abs(cupOffsetX) < CUP_FLOOR_SEARCH_SPAN) {
                        s32 cupOffsetZ =
                            simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z];

                        if (Abs(cupOffsetZ) < CUP_FLOOR_SEARCH_SPAN) {
                            f32 cupDistance = (s32)IntLength2D(cupOffsetX, cupOffsetZ);

                            if (cupDistance < CUP_DROP_RADIUS) {
                                floor += CUP_DEPTH;
                                simulation->isOverCup = 1;
                            }
                            else if (cupDistance < CUP_RIM_OUTER_RADIUS) {
                                f32 rimDistance = CUP_RIM_OUTER_RADIUS - cupDistance;
                                f32 rest = (CUP_RIM_CURVE_RADIUS * CUP_RIM_CURVE_RADIUS) -
                                    (rimDistance * rimDistance);

                                if (rest > 0.0f) {
                                    simulation->isOverCup = 1;
                                    floor = floor +
                                        ((CUP_RIM_CURVE_RADIUS - sqrtf(rest)) * CUP_RIM_SINK_SCALE);
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

                simulation->surfaceBelow =
                    FindSurface(simulation->position[AXIS_X], simulation->position[AXIS_Z], TRUE);
                kind = GetSurfaceInfo(simulation->surfaceBelow)->kind;
                if (IsDropKind(kind) &&
                    FindGroundTriangle(
                        simulation->position[AXIS_X],
                        simulation->position[AXIS_Z],
                        &ground
                    ) == 0 &&
                    GetTrianglePlane(&ground, &plane) == 0 &&
                    IsDropSlope(plane)) {
                    simulation->dropPosition[AXIS_X] = simulation->position[AXIS_X];
                    simulation->dropPosition[AXIS_Y] = simulation->position[AXIS_Y];
                    simulation->dropPosition[AXIS_Z] = simulation->position[AXIS_Z];
                }
            }
        }

NEXT:

        simulation->stepFlags = flags;
    }
    while (++substep < substepCount);
    simulation->velocity[AXIS_X] = velocity[AXIS_X];
    simulation->velocity[AXIS_Y] = velocity[AXIS_Y];
    simulation->velocity[AXIS_Z] = velocity[AXIS_Z];
    if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
        if (simulation->state == BALL_STATE_ROLLING && !simulation->soundStates[BALL_SOUND_ROLL]) {
            PlaySound(SOUND_ROLL, ROLL_SOUND_CHANNEL, SOUND_FULL_SCALE);
            simulation->soundStates[BALL_SOUND_ROLL] = TRUE;
        }
        else if (simulation->soundStates[BALL_SOUND_ROLL]) {
            if (simulation->state != BALL_STATE_ROLLING) {
                StopSound(ROLL_SOUND_CHANNEL);
                simulation->soundStates[BALL_SOUND_ROLL] = FALSE;
            }
            else {
                s32 rollFrames = simulation->rollFrameCount;
                SoundVolume volume;

                if (rollFrames > ROLL_VOLUME_RAMP_FRAMES) {
                    rollFrames = ROLL_VOLUME_RAMP_FRAMES;
                }

                volume = sqrtf(
                             (velocity[AXIS_X] * velocity[AXIS_X]) +
                             (velocity[AXIS_Z] * velocity[AXIS_Z])
                         ) *
                    ROLL_VOLUME_PER_SPEED * (rollFrames + 1);
                if (volume > MAX_ROLL_VOLUME) {
                    volume = MAX_ROLL_VOLUME;
                }

                SetSoundVolume(ROLL_SOUND_CHANNEL, volume & ~(volume >> 31));
            }
        }
    }

    return flags;
}
#else
INCLUDE_ASM("asm/nonmatchings/ball_simulation", PlayStrokeImpactSound);

INCLUDE_ASM("asm/nonmatchings/ball_simulation", BuildLiftRotation);

INCLUDE_ASM("asm/nonmatchings/ball_simulation", PlayImpactSound);

INCLUDE_ASM("asm/nonmatchings/ball_simulation", PlayBillboardHitSound);

INCLUDE_ASM("asm/nonmatchings/ball_simulation", GetPositionNoise);

INCLUDE_ASM("asm/nonmatchings/ball_simulation", StepBallSimulation);
#endif

/* Returns the ImpactEffect entry for the club's lie rating and impactTiming */
ImpactEffect *GetImpactTimingEntry(
    Club *club,
    Surface surface,
    ShotMode shotMode,
    s32 impactTiming
) {
    s32 row;

    row = GetLieRating(club, surface, shotMode) - 1;
    if (row < 0) {
        row = 0;
    }

    if (row > MAX_LIE_RATING) {
        row = MAX_LIE_RATING;
    }

    if (impactTiming < -IMPACT_TIMING_ENTRY_LIMIT) {
        return &f_EarlyImpactEffect;
    }

    if (impactTiming <= IMPACT_TIMING_ENTRY_LIMIT) {
        return &f_ImpactEffects[row][impactTiming + IMPACT_TIMING_ENTRY_LIMIT];
    }

    return &f_LateImpactEffect;
}

SignedBinaryAngle GetCrossSlopeAngle(Player *player, ShotLaunch *shot);
/*
 * It returns a SignedBinaryAngle; declaring it s32 here is there to match: as a
 * SignedBinaryAngle, LaunchBallSimulation has 2 words differing, 2 in shape
 */
#ifdef AVOID_UB
SignedBinaryAngle GetAimSlopeAngle(Player *player, ShotLaunch *shot);
#else
s32 GetAimSlopeAngle(Player *player, ShotLaunch *shot);
#endif

/*
 * Sets the simulation's start, velocity and wind from the shot.
 * Only a stroke spawns debris, starts rumble and prints the launch values.
 */
s32 LaunchBallSimulation(BallSimulation *simulation, s32 purpose, ShotLaunch *shot) {
    s32 percentRange[2];
    Club *club;
    ImpactEffect *entry;
    s32 impactTiming;
    s32 groundHeight;
    s32 speed;
    s32 sideSign;
    s32 slopeSpin;
    s32 rumbles;
    s16 crossSlope;
    s32 crossSlopeSize;
    s32 yaw;
    Radians pitch;
    Radians yawRadians;
    f32 launchScale;
    f32 blend;
    Radians slopePitch;
    f32 curve;
    f32 spinBase;
    Radians windAngle;
    f32 windScale;
    /*
     * Reusing it for the offset-free pitch below is there to match: a variable
     * of its own gives 4 differing words, though none in shape
     */
    f32 along;
    s32 index;
    s8 surface;

    /*
     * Reading the player each time is there to match: a pointer to the player
     * gives 22 differing words, and a local copy of the field 8
     */
    f32 PickLieBlend(void) {
        return (u32)((simulation->position[AXIS_X] + simulation->position[AXIS_Z] +
                         g_Players[simulation->playerIndex].noiseSeed) &
                   NOISE_BYTE_MASK) /
            MAX_NOISE_BYTE_PAIR_SUM +
            (u32)((simulation->position[AXIS_X] - simulation->position[AXIS_Z] +
                      g_Players[simulation->playerIndex].noiseSeed) &
                NOISE_BYTE_MASK) /
            MAX_NOISE_BYTE_PAIR_SUM;
    }

    launchScale = 1.0f;
    impactTiming = shot->impactTiming;
    simulation->purpose = purpose;
    sideSign = 1;
    g_UnusedLaunchCount++;
    slopeSpin = 0;
    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        groundHeight = GetGroundHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z]) -
            LAUNCH_GROUND_CLEARANCE;
        if (groundHeight < simulation->position[AXIS_Y]) {
            simulation->position[AXIS_Y] = groundHeight;
        }
    }

    surface = simulation->surface;
    simulation->dropPosition[AXIS_X] = simulation->position[AXIS_X];
    simulation->startPosition[AXIS_X] = simulation->dropPosition[AXIS_X];
    simulation->dropPosition[AXIS_Y] = simulation->position[AXIS_Y];
    simulation->startPosition[AXIS_Y] = simulation->dropPosition[AXIS_Y];
    simulation->dropPosition[AXIS_Z] = simulation->position[AXIS_Z];
    simulation->startPosition[AXIS_Z] = simulation->dropPosition[AXIS_Z];
    simulation->launchSurface = surface;
    simulation->firstBounceSurface = surface;
    simulation->hasEnteredWater = 0;
    simulation->stepFlags = 0;
    simulation->isOverCup = 0;

    /*
     * The tests of purpose, and their senses, are there to match: setting the
     * player index in the club's test gives 140 words differing (14 in shape),
     * testing for the club test first in the player index's test 4 (4), and in
     * the club's test 6 (6)
     */
    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        simulation->playerIndex = shot->playerIndex;
    }
    else {
        simulation->playerIndex = 0;
    }

    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        s16 *clubs = g_Players[shot->playerIndex].clubs;

        club = GetClub(clubs[shot->clubIndex]);
    }
    else {
        club = GetClub(shot->clubIndex);
    }

    if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
        entry = &f_ImpactEffects[0][IMPACT_TIMING_ENTRY_LIMIT];
        rumbles = f_ImpactEffects[0][IMPACT_TIMING_ENTRY_LIMIT].rumbles;
    }
    else {
        /*
         * The same as GetImpactTimingEntry; writing it out is there to match:
         * calling it gives 33 words differing, 27 in shape
         */
        s32 row = GetLieRating(club, simulation->surface, shot->shotMode) - 1;

        if (row < 0) {
            row = 0;
        }

        if (row > MAX_LIE_RATING) {
            row = MAX_LIE_RATING;
        }

        if (impactTiming < -IMPACT_TIMING_ENTRY_LIMIT) {
            entry = &f_EarlyImpactEffect;
        }
        else if (impactTiming > IMPACT_TIMING_ENTRY_LIMIT) {
            entry = &f_LateImpactEffect;
        }
        else {
            entry = &f_ImpactEffects[row][impactTiming + IMPACT_TIMING_ENTRY_LIMIT];
        }

        rumbles = entry->rumbles;
    }

    if ((club->surfaceMask & CLUB_SURFACE_GREEN) &&
        simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        speed = GetPuttPower(shot->power, shot->shotMode);
    }
    else {
        speed = GetClubPower(club, shot->shotMode);
        speed = speed *
            (shot->power *
                ((entry->speedChange * (LAUNCH_SCALE_ONE / SPEED_CHANGE_UNITS)) +
                    LAUNCH_SCALE_ONE) /
                LAUNCH_SCALE_ONE) /
            MAX_LAUNCH_POWER;
        if (speed <= 0) {
            speed = MIN_LAUNCH_SPEED;
        }
    }

    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        SurfaceInfo *info = GetSurfaceInfo(simulation->launchSurface);

        blend = PickLieBlend();
        GetSurfacePercentRange(info, club, percentRange);
        speed = ((percentRange[0] * blend) + (percentRange[1] * (1.0f - blend))) * speed *
            ONE_PER_PERCENT;
    }
    else {
        /*
         * The local is there to match: the constant gives 143 words differing, 13
         * in shape
         */
        s32 percent = CLUB_TEST_SPEED_HALF_PERCENTS;

        speed = speed * percent / HALF_PERCENTS_PER_ONE;
    }

    pitch = (club->pitch + entry->pitchChange) * DEGREES_TO_RADIANS;
    if (pitch >= 0.0f) {
        pitch = 0.0f;
    }

    if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
        sinf(pitch); /* The original has it */
        along = pitch - (club->offsetPitch * DEGREES_TO_RADIANS);
        pitch +=
            (f32)club->offsetPitch * shot->offsetY * (DEGREES_TO_RADIANS / OFFSET_Y_PER_PITCH_STEP);
        sinf(pitch); /* The original has it */
        launchScale =
            (cosf(along) - LAUNCH_SCALE_COSINE_BASE) * shot->offsetY * LAUNCH_SCALE_PER_OFFSET;
        if (club->kind == CLUB_KIND_WOOD) {
            launchScale *= WOOD_LAUNCH_SCALE_FACTOR;
        }

        launchScale += 1.0f;
    }

    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        if ((club->surfaceMask & CLUB_SURFACE_GREEN) && !g_GameState.isMiniGolf) {
            pitch += (u16)GetAimSlopeAngle(&g_Players[shot->playerIndex], shot) *
                BINARY_ANGLE_TO_RADIANS;
        }
        else {
            slopePitch = (u16)(GetAimSlopeAngle(&g_Players[shot->playerIndex], shot) /
                             AIM_SLOPE_PITCH_DIVISOR) *
                BINARY_ANGLE_TO_RADIANS;
            pitch += slopePitch;
            speed = speed * (1.0f - sinf(slopePitch));
        }
    }

    if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST ||
        (club->surfaceMask & CLUB_SURFACE_GREEN)) {
        yawRadians = (u16)shot->heading * BINARY_ANGLE_TO_RADIANS;
    }
    else {
        crossSlope = GetCrossSlopeAngle(&g_Players[shot->playerIndex], shot);
        crossSlopeSize = crossSlope;
        if (crossSlope < 0) {
            crossSlopeSize = -crossSlope;
        }

        curve = sqrtf(crossSlopeSize * CROSS_SLOPE_SPIN_SCALE);
        sideSign = g_Players[shot->playerIndex].isLefty ? -1 : 1;
        if (crossSlope < 0) {
            curve = -curve;
        }

        slopeSpin = curve;
        yaw = shot->heading + (crossSlope / CROSS_SLOPE_YAW_DIVISOR) +
            (sideSign *
                ((club->deflection[shot->shotMode] * shot->power / MAX_LAUNCH_POWER) +
                    (entry->yawChange * club->mishitScale)));
        yawRadians = (u16)yaw * BINARY_ANGLE_TO_RADIANS;
    }

    if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
        simulation->sideSpin =
            (sideSign * (club->sideSpin * CLUB_SIDE_SPIN_SCALE) * speed) * SIDE_SPIN_PER_SPEED;
        simulation->spinSize = 0.0f;
        spinBase = club->spin * CLUB_SPIN_BASE_SCALE;
        simulation->spinSpeed =
            (((spinBase + CLUB_TEST_SPIN_OFFSET) * CLUB_TEST_SPIN_PER_OFFSET * shot->offsetY) -
                spinBase) *
            (speed / launchScale) * SPIN_PER_SPEED;
        simulation->spinHeading = yawRadians;
        if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
            speed = speed * launchScale;
        }

        if (speed < 0) {
            speed = 0;
        }
    }
    else {
        GetSurfaceInfo(simulation->launchSurface); /* The original has it */
        simulation->sideSpin =
            (((sideSign * entry->sideSpinChange * club->mishitScale) / MISHIT_SIDE_SPIN_DIVISOR) +
                (shot->offsetX * (club->mishitScale * OFFSET_SIDE_SPIN_PER_MISHIT)) + slopeSpin +
                (sideSign * CLUB_SIDE_SPIN_SCALE * club->sideSpin)) *
            (speed * launchScale * launchScale) * SIDE_SPIN_PER_SPEED;
        if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
            simulation->spinSpeed =
                ((((club->spin * STROKE_SPIN_SCALE) + STROKE_SPIN_OFFSET) * STROKE_SPIN_PER_OFFSET *
                     (entry->spinChange + shot->offsetY)) -
                    (club->spin * CLUB_SPIN_BASE_SCALE)) *
                speed * SPIN_PER_SPEED;
            speed = speed * launchScale;
            if (g_HoleSetup.conditions.isRaining == TRUE) {
                speed = speed * RAIN_LAUNCH_SPEED_SCALE;
            }
        }
        else {
            simulation->spinSpeed = speed;
        }

        if (speed < 0) {
            speed = 0;
        }

        if (g_HoleSetup.conditions.isRaining == TRUE) {
            simulation->spinSpeed = simulation->spinSpeed * RAIN_SPIN_SCALE;
            simulation->sideSpin = simulation->sideSpin * RAIN_SIDE_SPIN_SCALE;
        }

        simulation->spinSize =
            IntLength2D(simulation->spinSpeed, simulation->sideSpin) * SPIN_SIZE_SCALE;
        simulation->spinHeading = yawRadians;
    }

    if (speed <= 0) {
        speed = MIN_LAUNCH_SPEED;
    }

    simulation->velocity[AXIS_Y] = sinf(pitch) * speed;
    simulation->velocity[AXIS_X] = cosf(yawRadians) * speed;
    simulation->velocity[AXIS_Z] = sinf(yawRadians) * speed;
    if (club->surfaceMask & CLUB_SURFACE_GREEN) {
        simulation->state = BALL_STATE_ROLLING;
        simulation->velocity[AXIS_Y] += PUTT_VELOCITY_Y_OFFSET;
    }
    else {
        simulation->state = BALL_STATE_FLYING;
    }

    simulation->bounceCount = 0;
    simulation->stepCount = 0;
    simulation->lastBounceStep = NO_BOUNCE_STEP;
    simulation->isHoledOut = 0;
    simulation->hasHitFlag = FALSE;
    simulation->rollFrameCount = 0;
    for (index = 0; index < BALL_SOUND_COUNT; index++) {
        simulation->soundStates[index] = 0;
    }

    /*
     * The local is there to match: the constant gives 75 words differing, 5 in
     * shape
     */
    windScale = WIND_SPEED_TO_VELOCITY;
    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        windAngle = g_HoleSetup.conditions.windDirection * BINARY_ANGLE_TO_RADIANS;
        simulation->windVelocityX = cosf(windAngle) * g_HoleSetup.conditions.windSpeed * windScale;
        simulation->windVelocityZ = sinf(windAngle) * g_HoleSetup.conditions.windSpeed * windScale;
        if (g_HoleSetup.conditions.isRaining != TRUE) {
            along = ((simulation->windVelocityX * cosf(yawRadians)) +
                        (simulation->windVelocityZ * sinf(yawRadians))) *
                WIND_ALONG_SHOT_CUT;
            simulation->windVelocityX += -cosf(yawRadians) * along;
            simulation->windVelocityZ += -sinf(yawRadians) * along;
        }
    }
    else {
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
            /*
             * Declaring it here is there to match: declaring it at the top gives 6
             * words differing, none in shape
             */
            char text[DEBUG_TEXT_BUFFER_SIZE];

            sprintf(text, "ANGLEY = %3.3d/360", (int)(yawRadians * (f32)RADIANS_TO_DEGREES));
            PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_YAW);
            sprintf(text, "ANGLEX = %3.3d/360", (int)(pitch * (f32)RADIANS_TO_DEGREES));
            PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_PITCH);
            sprintf(
                text,
                "Velocity = %fm/s     ",
                speed * (1.0f / SPEED_UNITS_PER_METER_PER_SECOND)
            );
            PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SPEED);
            sprintf(text, "SpinY = %d", (int)simulation->sideSpin);
            PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SIDE_SPIN);
            sprintf(text, "SpinX = %d", (int)simulation->spinSpeed);
            PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SPIN);
            sprintf(text, "SpinVel = %fRPM      ", simulation->spinSpeed * RPM_PER_SPIN_SPEED_UNIT);
            PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SPIN_RPM);
            sprintf(
                text,
                "SpinAngle = %3.3d/360",
                (int)(simulation->spinHeading * (f32)RADIANS_TO_DEGREES)
            );
            PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SPIN_HEADING);
            sprintf(
                text,
                "SpinVelX = %fkm/h      ",
                simulation->spinSpeed * KILOMETERS_PER_HOUR_PER_SPEED_UNIT
            );
            PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SPIN_SPEED);
            sprintf(
                text,
                "SpinVelY = %fkm/h      ",
                simulation->sideSpin * KILOMETERS_PER_HOUR_PER_SPEED_UNIT
            );
            PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SIDE_SPIN_SPEED);
        }
    }

    return 0;
}
