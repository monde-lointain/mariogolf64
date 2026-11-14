#include "cinematics/opening_ball.h"

#include <math.h>
#include <stdlib.h>
#include <ultra64.h>

#include "characters/character_model.h"
#include "characters/reaction.h"
#include "course/collision.h"
#include "course/course_space.h"
#include "course/hole_setup.h"
#include "course/hole_triangles.h"
#include "course/surface.h"
#include "course/terrain.h"
#include "debug/debug_text.h"
#include "debug/game_flags.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "round/game_state.h"
#include "round/player.h"
#include "shot/ball_physics.h"
#include "shot/ball_simulation.h"
#include "shot/club.h"
#include "shot/particle_effects.h"
#include "shot/shot_effects.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"

#define OPENING_CHARACTER_MODEL_KIND CHARACTER_MODEL_KIND_COUNT

#define OPENING_CLUB_COUNT 2

static ClubTest f_OpeningClubTestMode = CLUB_TEST_NONE;

static SurfaceTraits f_OpeningSurfaceTraits[SURFACE_COUNT] = {
    {SURFACE_KIND_OUT_OF_BOUNDS, "O.B.", 100, 100, 0, 0, 0, 0x500, SINK_DEPTH_HALF, 7},
    {SURFACE_KIND_FAIRWAY, "ﾌｪｱｳｪｲ", 98, 100, 5, 960, 400, 0x501, SINK_DEPTH_NONE, 0},
    {SURFACE_KIND_ROUGH, "ﾗﾌ", 80, 90, 30, 600, 200, 0x508, SINK_DEPTH_HALF, 1},
    {SURFACE_KIND_BUNKER, "ﾊﾞﾝｶｰ", 70, 85, 30, 600, 0, 0x504, SINK_DEPTH_NONE, 2},
    {SURFACE_KIND_GREEN, "ｵﾝ ｸﾞﾘｰﾝ", 100, 100, 0, 972, 500, 0x50A, SINK_DEPTH_NONE, 6},
    {SURFACE_KIND_WATER_HAZARD, "ｳｫｰﾀｰﾊｻﾞｰﾄﾞ", 100, 100, 0, 0, 0, 0x50C, SINK_DEPTH_BURIED, 3},
    {SURFACE_KIND_CART_WAY, "ｶｰﾄｳｪｲ", 80, 100, 10, 975, 890, 0x50B, SINK_DEPTH_NONE, 4},
    {SURFACE_KIND_TEE_GROUND, "ﾃｨｰｸﾞﾗｳﾝﾄﾞ", 98, 100, 10, 930, 500, 0x501, SINK_DEPTH_NONE, 8},
    {SURFACE_KIND_ROCK, "ﾛｯｸ", 80, 90, 30, 930, 900, 0x50D, SINK_DEPTH_NONE, 9},
    {SURFACE_KIND_GREEN_EDGE, "ｸﾞﾘｰﾝ ｴｯｼﾞ", 98, 100, 10, 966, 400, 0x501, SINK_DEPTH_NONE, 0},
    {SURFACE_KIND_SEMI_ROUGH, "ｾﾐﾗﾌ", 90, 98, 30, 930, 500, 0x507, SINK_DEPTH_NONE, 0},
    {SURFACE_KIND_BARE_GROUND, "ﾍﾞｱｸﾞﾗｳﾝﾄﾞ", 80, 90, 30, 930, 500, 0x50C, SINK_DEPTH_NONE, 1},
    {SURFACE_KIND_DEEP_ROUGH, "ﾃﾞｨｰﾌﾟﾗﾌ", 50, 80, 30, 430, 100, 0x509, SINK_DEPTH_BURIED, 5},
    {SURFACE_KIND_NO_DROP_GROUND, "ﾍﾞｱｸﾞﾗｳﾝﾄﾞ", 80, 90, 30, 930, 500, 0x50C, SINK_DEPTH_NONE, 1},
    {SURFACE_KIND_WASTE_AREA, "ｳｴｽﾄｴﾘｱ", 50, 80, 30, 430, 100, 0x8F1, SINK_DEPTH_HALF, 5},
    {SURFACE_KIND_ROCK, "            ", 98, 100, 40, 930, 500, 0x500, SINK_DEPTH_NONE, 9},
    {SURFACE_KIND_BUNKER_FAIR_LIE, "ﾊﾞﾝｶｰ", 50, 70, 30, 600, 0, 0x505, SINK_DEPTH_HALF, 2},
    {SURFACE_KIND_BUNKER_POOR_LIE, "ﾊﾞﾝｶｰ", 40, 60, 30, 600, 0, 0x506, SINK_DEPTH_HALF, 2},
    {SURFACE_KIND_BUNKER_UNUSED_LIE, "ﾊﾞﾝｶｰ", 40, 60, 40, 600, 0, 0x506, SINK_DEPTH_HALF, 2},
    {SURFACE_KIND_FAIRWAY_FAIR_LIE, "ﾌｪｱｳｪｲ", 94, 98, 7, 950, 380, 0x502, SINK_DEPTH_NONE, 0},
    {SURFACE_KIND_FAIRWAY_POOR_LIE, "ﾌｪｱｳｪｲ", 90, 94, 7, 950, 380, 0x503, SINK_DEPTH_NONE, 0},
    {SURFACE_KIND_ROUGH_POOR_LIE, "ﾗﾌ", 70, 85, 30, 600, 200, 0x509, SINK_DEPTH_HALF, 5},
    {SURFACE_KIND_OUT_OF_COURSE, "ｱｳﾄ ｵﾌﾞ ｺｰｽ", 100, 100, 0, 0, 0, 0x500, SINK_DEPTH_BURIED, 7},
    {SURFACE_KIND_TEE_UP, "TEE UP", 98, 100, 10, 930, 500, 0x500, SINK_DEPTH_NONE, 8},
    {SURFACE_KIND_GREEN, "ｸﾞﾘｰﾝ", 100, 100, 0, 972, 500, 0x8F2, SINK_DEPTH_NONE, 6},
    {SURFACE_KIND_FAIRWAY, "ﾌﾞﾙｰ", 98, 100, 5, 960, 400, 0x8F3, SINK_DEPTH_NONE, 0},
    {SURFACE_KIND_CART_WAY, "ﾚｯﾄﾞ", 80, 100, 10, 980, 890, 0x8F4, SINK_DEPTH_NONE, 4},
};

static s32 f_OpeningSpeedTextRow = 0;
static ImpactEffect f_OpeningEarlyImpactEffect UNUSED = {-90, -64, -40, -10, 100, 1};
static ImpactEffect f_OpeningLateImpactEffect UNUSED = {-80, 64, 30, 10, -5, 1};
static ImpactEffect f_OpeningImpactEffects[LIE_RATING_MAX + 1][IMPACT_TIMING_ENTRY_COUNT] = {
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
        {-4, -2, 1, -5, 5, 0},
        {0, 0, 0, 0, 0, 0},
        {-8, 2, -1, 5, 4, 0},
        {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1},
    },
    {
        {-90, -64, -40, -10, 100, 1},
        {-90, -64, -40, -10, 100, 1},
        {-90, -64, -40, -10, 100, 1},
        {-8, -8, 2, -10, 15, 0},
        {-4, -2, 1, -5, 5, 0},
        {0, 0, 0, 0, 0, 0},
        {-8, 2, -1, 5, 4, 0},
        {-12, 8, -2, 10, 4, 0},
        {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1},
    },
    {
        {-90, -64, -40, -10, 100, 1},
        {-90, -64, -40, -10, 100, 1},
        {-8, -8, 2, -10, 15, 0},
        {-4, -2, 1, -5, 5, 0},
        {-2, -1, 0, -3, 3, 0},
        {0, 0, 0, 0, 0, 0},
        {-4, 1, 0, 3, 2, 0},
        {-8, 2, -1, 5, 4, 0},
        {-12, 8, -2, 10, 4, 0},
        {-80, 64, 30, 10, -5, 1},
        {-80, 64, 30, 10, -5, 1},
    },
    {
        {-90, -64, -40, -10, 100, 1},
        {-8, -8, 2, -10, 15, 0},
        {-4, -2, 1, -5, 5, 0},
        {-2, -1, 0, -3, 3, 0},
        {-1, 0, 0, -2, 1, 0},
        {0, 0, 0, 0, 0, 0},
        {-2, 0, 0, 2, 1, 0},
        {-4, 1, 0, 3, 2, 0},
        {-8, 2, -1, 5, 4, 0},
        {-12, 8, -2, 10, 4, 0},
        {-80, 64, 30, 10, -5, 1},
    },
    {
        {-8, -8, 2, -10, 15, 0},
        {-4, -2, 1, -5, 5, 0},
        {-2, -1, 0, -3, 3, 0},
        {-1, 0, 0, -2, 1, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {-2, 0, 0, 2, 1, 0},
        {-4, 1, 0, 3, 2, 0},
        {-8, 2, -1, 5, 4, 0},
        {-12, 8, -2, 10, 4, 0},
    },
};

static Club f_OpeningClubs[OPENING_CLUB_COUNT] = {
    {
        0,
        "1W",
        DRIVER_SURFACES,
        208,
        -15,
        0,
        11,
        120,
        CLUB_TYPE_DRIVER,
        {97608, 84651, 19335},
        CLUB_CATEGORY_WOOD,
        0,
        {0, 0, 0},
        ASSET_DRIVER_HEAD_IMAGE,
        0,
    },
    {
        0,
        "1W",
        DRIVER_SURFACES,
        208,
        -23,
        0,
        11,
        120,
        CLUB_TYPE_DRIVER,
        {97608, 84651, 19335},
        CLUB_CATEGORY_WOOD,
        0,
        {0, 0, 0},
        ASSET_DRIVER_HEAD_IMAGE,
        0,
    },
};

static s32 f_UnusedOpeningLaunchCount = 0;

static PlaneTriangle f_OpeningContactTriangle;
static f32 f_OpeningContactFraction;

Boolean ToggleOpeningCharacter(u32 slotIndex, OpeningModel model) {
    if (IsCharacterSlotBusy(slotIndex)) {
        FreeCharacterSlot(slotIndex);
        return FALSE;
    }

    LoadCharacter(slotIndex, model, OPENING_CHARACTER_MODEL_KIND);
    SetCharacterAnimation(slotIndex, REACTION_ANIMATION_BIRDIE);
    SetAnimationSpeed(slotIndex, 1.0f);
    return TRUE;
}

static SurfaceTraits *GetOpeningSurfaceTraits(Surface surface) {
    if ((u32)surface >= SURFACE_COUNT) {
        surface = SURFACE_OUT_OF_BOUNDS;
    }

    return &f_OpeningSurfaceTraits[surface];
}

static UNUSED_FUNCTION void ResetOpeningBallSimulations(void) {
    s32 index;

    for (index = 0; index < BALL_SIMULATION_COUNT; index++) {
        g_BallSimulations[index].position[AXIS_X] = RESET_BALL_X;
        g_BallSimulations[index].position[AXIS_Y] = RESET_BALL_Y;
        rand();
        g_BallSimulations[index].position[AXIS_Z] = RESET_BALL_Z;
        g_BallSimulations[index].velocity[AXIS_X] = rand() & RESET_BALL_SPEED_MASK;
        g_BallSimulations[index].velocity[AXIS_Y] = RESET_BALL_VELOCITY_Y;
        g_BallSimulations[index].velocity[AXIS_Z] = rand() & RESET_BALL_SPEED_MASK;
        g_BallSimulations[index].unusedResetValue = 0;
        g_BallSimulations[index].state = BALL_STATE_FLYING;
    }
}

#define STATE_TEXT_COLUMN 2
#define STATE_TEXT_ROW 0xF
#define HEIGHT_TEXT_COLUMN 1
#define HEIGHT_TEXT_ROW 0xF
#define SPEED_TEXT_COLUMN 3
#define SPEED_TEXT_CLEAR_COLUMN 7
#define SPEED_TEXT_ROW_COUNT 8
#define SPEED_TEXT_FIRST_ROW 8
#define SPEED_TEXT_KILOMETERS_PER_HOUR_PER_SPEED_UNIT 0.0070312503f

#define OPENING_LIFT_ANGLE_DEGREES 90.0f
#define OPENING_CUP_SEARCH_SPAN 0xC00
#define OPENING_CUP_IN_SOUND_SPEED_MAX 268.8f
#define OPENING_CUP_IN_SOUND_FALL_SPEED_MAX KilometersPerHour(5)
#define OPENING_CUP_PULL_DIRECTION_TILT 8384.0f
#define OPENING_LIP_OUT_HEIGHT_MAX 307.2f
#define OPENING_LIP_OUT_SPEED_MAX 4300.8f
#define OPENING_WATER_STOP_DEPTH 0x12C00
#define OPENING_CUP_FLOOR_BOUNCE_DIVISOR 2
#define OPENING_BILLBOARD_HIT_SPEED_DIVISOR 2
#define OPENING_FOLIAGE_RISE_SCALE 1.2f
#define OPENING_WALL_BACK_OFF 614.4f
#define OPENING_WALL_HIT_STEP_SHARE 0.99f
#define OPENING_SKIP_IMPACT_SPEED_MIN KilometersPerHour(10)
#define OPENING_SKIP_SPEED_RATIO 0.5f
#define OPENING_EARLY_BOUNCE_STEP_COUNT 3
#define OPENING_WATER_STOP_IMPACT_SPEED KilometersPerHour(20)
#define OPENING_GREEN_LANDING_FRICTION_BASE 0.5f
#define OPENING_GREEN_LANDING_FRICTION_PER_SPEED (1.6f / KilometersPerHour(100))
#define OPENING_SPINLESS_LANDING_SPEED KilometersPerHour(10)
#define OPENING_LANDING_SPIN_SPEED_RANGE KilometersPerHour(200)
#define OPENING_GROUND_SPIN_DRIFT_GRIP 0.25f
#define OPENING_SPIN_DRIFT_SHARE_MAX 0.2f
#define OPENING_ROLL_JUMP_SPEED KilometersPerHour(10)
#define OPENING_SLOW_ROLL_SLOPE_SCALE_MIN 0.0625f
#define OPENING_SLOW_ROLL_SLOPE_SCALE_PER_SPEED 0.0030517578f
#define OPENING_SPIN_SPEED_SCALE 8.0f
#define OPENING_SPIN_MISMATCH_FALLOFF 0.8f
#define OPENING_LIFT_SPIN_SCALE 10
#define OPENING_LIFT_PER_SPIN 0.0061035156f
#define OPENING_LIFT_SPEED_RATIO_MAX 200.0f
#define OPENING_SPIN_DECAY 0.995f
#define OPENING_WIND_SCALE_PER_HEIGHT (1.0f / (160 * UNITS_PER_YARD))

StepFlags StepOpeningBallSimulation(BallSimulation *simulation) {
    PlaneTriangle triangle;

    f32 GetOpeningStepNoise(void) {
        return ((u32)((simulation->position[AXIS_X] + simulation->position[AXIS_Z] +
                          g_Players[simulation->playerIndex].noiseSeed) &
                    NOISE_BYTE_MASK) *
                   (1.0f / NOISE_BYTE_PAIR_SUM_MAX)) +
            ((u32)((simulation->position[AXIS_X] - simulation->position[AXIS_Z] +
                       g_Players[simulation->playerIndex].noiseSeed) &
                 NOISE_BYTE_MASK) *
                (1.0f / NOISE_BYTE_PAIR_SUM_MAX));
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
        }
        else {
            axisZ = 1.0f;
            axisX = 0.0f;
        }

        guRotateF(simulation->liftRotation, OPENING_LIFT_ANGLE_DEGREES, -axisX, 0.0f, axisZ);
    }

    f32 GetOpeningPositionNoise(void) {
        return (((simulation->position[AXIS_X] + simulation->position[AXIS_Z]) & NOISE_BYTE_MASK) -
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
    StepFlags contact;
    StepFlags stepFlags = 0;
    Boolean printsDebugText;
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
            sprintf(
                speedText,
                "%fkm/h   ",
                (s32)IntLength3D(
                    simulation->velocity[AXIS_X],
                    simulation->velocity[AXIS_Y],
                    simulation->velocity[AXIS_Z]
                ) * SPEED_TEXT_KILOMETERS_PER_HOUR_PER_SPEED_UNIT
            );
            PrintDebugText(
                speedText,
                SPEED_TEXT_COLUMN,
                f_OpeningSpeedTextRow + SPEED_TEXT_FIRST_ROW
            );
            f_OpeningSpeedTextRow = (f_OpeningSpeedTextRow + 1) % SPEED_TEXT_ROW_COUNT;
            PrintDebugText(
                "                            ",
                SPEED_TEXT_CLEAR_COLUMN,
                f_OpeningSpeedTextRow + SPEED_TEXT_FIRST_ROW
            );
        }

        if (IsGameFlagSet(GAME_FLAG_SHOW_OPENING_BALL_HEIGHT)) {
            if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                sprintf(
                    heightText,
                    "Height %fYd   ",
                    (simulation->startPosition[AXIS_Y] - simulation->position[AXIS_Y]) *
                        YARDS_PER_UNIT
                );
                PrintDebugText(heightText, HEIGHT_TEXT_COLUMN, HEIGHT_TEXT_ROW);
            }
        }

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
                SetFlatTriangle(&triangle);
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
                Boolean found;

                if (IsStrokeOrTrialLaunch(simulation->purpose) &&
                    FindRingHit(
                        startX,
                        startY,
                        startZ,
                        simulation->position[AXIS_X],
                        simulation->position[AXIS_Y],
                        simulation->position[AXIS_Z],
                        &f_OpeningContactTriangle,
                        &f_OpeningContactFraction
                    ) > 0) {
                    s8 *ringState = &g_Players[simulation->playerIndex]
                                         .ringStates[f_OpeningContactTriangle.tag];

                    if (*ringState == RING_STATE_SHOWN &&
                        simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                        *ringState = RING_STATE_PASSED;
                    }
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
                        &f_OpeningContactTriangle,
                        &f_OpeningContactFraction
                    );
                    if (g_GameState.isMiniGolf && simulation->state == BALL_STATE_ROLLING) {
                        found = FindMiniGolfGroundHit(
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

                        if (found && fraction < f_OpeningContactFraction) {
                            contact = found;
                            osSyncPrintf("PutterSpecialGroundCollision\n");
                            f_OpeningContactTriangle = triangle;
                            f_OpeningContactFraction = fraction;
                        }
                    }
                    found = FALSE;
                }
                else {
                    contact = FindGroundHit(
                        startX,
                        startY,
                        startZ,
                        simulation->position[AXIS_X],
                        simulation->position[AXIS_Y],
                        simulation->position[AXIS_Z],
                        &f_OpeningContactTriangle,
                        &f_OpeningContactFraction
                    );
                    found = FALSE;
                }

                if (simulation->state == BALL_STATE_FLYING) {
                    if (simulation->lastBounceStep + BILLBOARD_HIT_BOUNCE_GAP <
                        simulation->stepCount) {
                        found = FindSimulationBillboardHit(
                            startX,
                            startY,
                            startZ,
                            simulation->position[AXIS_X],
                            simulation->position[AXIS_Y],
                            simulation->position[AXIS_Z],
                            &triangle.tag,
                            &fraction,
                            simulation
                        );
                    }
                    else {
                        found = FALSE;
                    }
                }

                if (found && contact == STEP_FLAG_GROUND_HIT) {
                    if (f_OpeningContactFraction <= fraction) {
                        contact = STEP_FLAG_GROUND_HIT;
                        triangle = f_OpeningContactTriangle;
                        fraction = f_OpeningContactFraction;
                    }
                    else {
                        contact = STEP_FLAG_BILLBOARD_HIT;
                    }
                }
                else if (found) {
                    contact = STEP_FLAG_BILLBOARD_HIT;
                }
                else if (contact == STEP_FLAG_GROUND_HIT) {
                    triangle = f_OpeningContactTriangle;
                    fraction = f_OpeningContactFraction;
                }
                else {
                    contact = 0;
                }

                if (simulation->state == BALL_STATE_FLYING ||
                    simulation->state == BALL_STATE_ROLLING) {
                    found = FindHoleTriangleHit(
                        startX,
                        startY,
                        startZ,
                        simulation->position[AXIS_X],
                        simulation->position[AXIS_Y],
                        simulation->position[AXIS_Z],
                        &f_OpeningContactTriangle,
                        &f_OpeningContactFraction
                    );
                }
                else {
                    found = FALSE;
                }

                if (found && (contact == 0 || f_OpeningContactFraction <= fraction)) {
                    contact = STEP_FLAG_HOLE_TRIANGLE_HIT;
                    triangle = f_OpeningContactTriangle;
                    fraction = f_OpeningContactFraction;
                }
            }

            stepFlags |= contact;
            if (printsDebugText) {
                PrintDebugPaddedHex(contact, CONTACT_DEBUG_TEXT_COLUMN, CONTACT_DEBUG_TEXT_ROW);
                osSyncPrintf("col%d\n", contact);
            }
        }

        if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
            CoursePoint pinVelocity;
            f32 pinFraction;
            PinHit pinHit;

            if (g_GameState.isMiniGolf ? g_Players[simulation->playerIndex].strokeCount == 1 :
                                         GetOpeningSurfaceTraits(simulation->launchSurface)->kind !=
                        SURFACE_KIND_GREEN) {
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
                        BallSpeed speed = GetOpeningLength2D(
                            simulation->velocity[AXIS_X],
                            simulation->velocity[AXIS_Z]
                        );

                        if (speed > KilometersPerHour(1)) {
                            SoundVolume volume = speed * FLAGSTICK_SOUND_VOLUME_PER_SPEED;

                            if (volume >= SOUND_FULL_SCALE) {
                                volume = KNOCK_VOLUME_MAX;
                            }
                        }
                    }

                    if (simulation->state == BALL_STATE_ROLLING) {
                        simulation->state = BALL_STATE_FLYING;
                        simulation->velocity[AXIS_Y] = -GetOpeningLength2D(
                                                           simulation->velocity[AXIS_X],
                                                           simulation->velocity[AXIS_Z]
                                                       ) *
                            PIN_HIT_RISE_SCALE;
                        contact = 0;
                        simulation->spinHeading = atan2(pinVelocity[AXIS_Z], pinVelocity[AXIS_X]);
                        simulation->spinSpeed =
                            IntLength2D(pinVelocity[AXIS_X], pinVelocity[AXIS_Z]);
                    }
                    else {
                        simulation->velocity[AXIS_Y] = pinVelocity[AXIS_Y];
                        simulation->spinSpeed = simulation->spinSpeed * PIN_HIT_SPIN_KEPT;
                    }

                    {
                        f32 remainingShare = PIN_HIT_STEP_SHARE - pinFraction;

                        simulation->velocity[AXIS_X] = pinVelocity[AXIS_X];
                        simulation->velocity[AXIS_Z] = pinVelocity[AXIS_Z];
                        stepFlags |= STEP_FLAG_PIN_HIT;
                        simulation->position[AXIS_X] = simulation->position[AXIS_X] +
                            (remainingShare * simulation->velocity[AXIS_X]);
                        simulation->position[AXIS_Y] = simulation->position[AXIS_Y] +
                            (remainingShare * simulation->velocity[AXIS_Y]);
                        simulation->position[AXIS_Z] = simulation->position[AXIS_Z] +
                            (remainingShare * simulation->velocity[AXIS_Z]);
                    }

                    MoveBallToCupEdge(simulation);
                    BuildOpeningLiftRotation();
                }
            }

            if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
                s32 cupOffsetX = simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X];

                if (Abs(cupOffsetX) < OPENING_CUP_SEARCH_SPAN) {
                    s32 cupOffsetZ = simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z];

                    if (Abs(cupOffsetZ) < OPENING_CUP_SEARCH_SPAN) {
                        s32 cupDistance = IntLength2D(cupOffsetX, cupOffsetZ);

                        if (simulation->state != BALL_STATE_IN_CUP &&
                            simulation->position[AXIS_Y] - g_HoleSetup.cupPosition[AXIS_Y] >=
                                -CUP_ENTRY_HEIGHT_MAX &&
                            (f32)cupDistance < CUP_RADIUS) {
                            BallSpeed speed = (s32)IntLength2D(
                                simulation->velocity[AXIS_X],
                                simulation->velocity[AXIS_Z]
                            );

                            if (speed < CUP_IN_SPEED_MAX &&
                                (f32)simulation->velocity[AXIS_Y] >= -CUP_IN_RISE_SPEED_MAX) {
                                if (printsDebugText) {
                                    SetDebugTextHighlight(TRUE);
                                    PrintDebugText(
                                        "CUPIN!",
                                        CUP_DEBUG_TEXT_COLUMN,
                                        CUP_DEBUG_TEXT_ROW
                                    );
                                }

                                simulation->state = BALL_STATE_IN_CUP;
                                if (speed < OPENING_CUP_IN_SOUND_SPEED_MAX &&
                                    (f32)simulation->velocity[AXIS_Y] <=
                                        OPENING_CUP_IN_SOUND_FALL_SPEED_MAX) {
                                    simulation->soundStates[BALL_SOUND_CUP_IN] = CUP_IN_SOUND_DUE;
                                }
                            }
                        }

                        if (simulation->state == BALL_STATE_ROLLING &&
                            (f32)Abs(
                                simulation->position[AXIS_Y] - g_HoleSetup.cupPosition[AXIS_Y]
                            ) < CUP_PULL_HEIGHT_MAX) {
                            if ((f32)cupDistance < CUP_PULL_OUTER_RADIUS &&
                                (f32)cupDistance >= CUP_PULL_INNER_RADIUS) {
                                Vec3 toCup;
                                Vec3 direction;
                                f32 alignment;
                                Radians angle;
                                f32 pull;

                                isNearCup = TRUE;
                                if (printsDebugText) {
                                    SetDebugTextHighlight(TRUE);
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
                                direction[AXIS_X] = simulation->velocity[AXIS_X];
                                direction[AXIS_Y] = OPENING_CUP_PULL_DIRECTION_TILT;
                                direction[AXIS_Z] = simulation->velocity[AXIS_Z];
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

                                angle = atan2(toCup[AXIS_Z], toCup[AXIS_X]);
                                alignment = 1.0f - alignment;
                                pull = CUP_PULL_OUTER_RADIUS - (f32)cupDistance;
                                if (pull > CUP_PULL_DEPTH_RANGE) {
                                    pull = CUP_PULL_DEPTH_RANGE;
                                }

                                pull = (sinf(pull * ((f32)M_PI / CUP_PULL_DEPTH_RANGE)) *
                                           CUP_PULL_RISE) +
                                    CUP_PULL_MIN;
                                pull *= alignment;
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
                                (f32)IntLength2D(
                                    startX - g_HoleSetup.cupPosition[AXIS_X],
                                    startZ - g_HoleSetup.cupPosition[AXIS_Z]
                                ) < CUP_RADIUS &&
                                (f32)Abs(startY - g_HoleSetup.cupPosition[AXIS_Y]) <
                                    OPENING_LIP_OUT_HEIGHT_MAX) {
                                s32 speed = IntLength2D(
                                    simulation->velocity[AXIS_X],
                                    simulation->velocity[AXIS_Z]
                                );

                                if (!((f32)speed >= OPENING_LIP_OUT_SPEED_MAX) &&
                                    simulation->state == BALL_STATE_ROLLING) {
                                    f32 keptFraction = speed * (1.0f / LIP_OUT_FULL_SPEED);
                                    BallSpeed fallSpeed;

                                    if (keptFraction < 0.0f) {
                                        keptFraction = 0.0f;
                                    }

                                    if (keptFraction > 1.0f) {
                                        keptFraction = 1.0f;
                                    }

                                    fallSpeed =
                                        -speed * sqrtf(1.0f - (keptFraction * keptFraction));
                                    simulation->state = BALL_STATE_FLYING;
                                    simulation->velocity[AXIS_X] =
                                        simulation->velocity[AXIS_X] * keptFraction;
                                    simulation->velocity[AXIS_Z] =
                                        simulation->velocity[AXIS_Z] * keptFraction;
                                    simulation->velocity[AXIS_Y] = fallSpeed;
                                    simulation->spinHeading = atan2(
                                        simulation->velocity[AXIS_Z],
                                        simulation->velocity[AXIS_X]
                                    );
                                    simulation->spinSpeed = IntLength2D(
                                                                simulation->velocity[AXIS_X],
                                                                simulation->velocity[AXIS_Z]
                                                            ) *
                                        LIP_OUT_SPIN_SCALE;
                                    stepFlags |= STEP_FLAG_LIPPED_OUT;
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
            if (simulation->position[AXIS_Y] >
                    GetGroundOrWaterHeight(
                        simulation->position[AXIS_X],
                        simulation->position[AXIS_Z]
                    ) + OPENING_WATER_STOP_DEPTH ||
                simulation->position[AXIS_Y] >=
                    GetGroundHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z])) {
                simulation->state = BALL_STATE_STOPPED;
            }
        }

        if (simulation->state == BALL_STATE_IN_CUP) {
            f32 cupDistance;

            GetOpeningLength3D(simulation->velocity[AXIS_X], 0.0f, simulation->velocity[AXIS_Z]);
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
                push = ((toCupX * simulation->velocity[AXIS_X]) +
                           (toCupY * simulation->velocity[AXIS_Y]) +
                           (toCupZ * simulation->velocity[AXIS_Z])) *
                    CUP_WALL_REFLECTION;
                scale = CUP_WALL_REBOUND_RADIUS / cupDistance;
                simulation->velocity[AXIS_X] = simulation->velocity[AXIS_X] - (toCupX * push);
                simulation->velocity[AXIS_Y] = simulation->velocity[AXIS_Y] - (toCupY * push);
                simulation->velocity[AXIS_Z] = simulation->velocity[AXIS_Z] - (toCupZ * push);
                simulation->position[AXIS_X] =
                    ((simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X]) * scale) +
                    g_HoleSetup.cupPosition[AXIS_X];
                simulation->position[AXIS_Z] =
                    ((simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]) * scale) +
                    g_HoleSetup.cupPosition[AXIS_Z];
            }

            if (simulation->position[AXIS_Y] <
                    g_HoleSetup.cupPosition[AXIS_Y] + CUP_CEILING_DEPTH &&
                simulation->velocity[AXIS_Y] < -CUP_CEILING_SPEED_MIN) {
                s32 toCupX = g_HoleSetup.cupPosition[AXIS_X] - simulation->position[AXIS_X];
                s32 toCupZ = g_HoleSetup.cupPosition[AXIS_Z] - simulation->position[AXIS_Z];

                if ((toCupX | toCupZ) != 0) {
                    u32 scale =
                        (u32)Abs(simulation->velocity[AXIS_Y]) / IntLength2D(toCupX, toCupZ);

                    simulation->velocity[AXIS_X] = toCupX * scale;
                    simulation->velocity[AXIS_Z] = toCupZ * scale;
                    simulation->position[AXIS_Y] =
                        g_HoleSetup.cupPosition[AXIS_Y] + CUP_CEILING_DEPTH;
                }

                simulation->velocity[AXIS_Y] = Abs(simulation->velocity[AXIS_Y]);
            }

            if (simulation->position[AXIS_Y] >= g_HoleSetup.cupPosition[AXIS_Y] + CUP_DEPTH) {
                simulation->velocity[AXIS_Y] =
                    (f32)(-simulation->velocity[AXIS_Y] / OPENING_CUP_FLOOR_BOUNCE_DIVISOR) +
                    CUP_GRAVITY;
                simulation->velocity[AXIS_Z] = 0;
                simulation->velocity[AXIS_X] = 0;
                if (simulation->velocity[AXIS_Y] > 0) {
                    simulation->isHoledOut = TRUE;
                    simulation->state = BALL_STATE_STOPPED;
                }

                simulation->position[AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y] + CUP_DEPTH;
            }
            else {
                simulation->velocity[AXIS_Y] = simulation->velocity[AXIS_Y] + CUP_GRAVITY;
            }
        }

        if (contact == STEP_FLAG_BILLBOARD_HIT && simulation->state == BALL_STATE_FLYING) {
            simulation->bounceCount++;
            simulation->position[AXIS_X] = startX + (fraction * simulation->velocity[AXIS_X]);
            simulation->position[AXIS_Y] = startY + (fraction * simulation->velocity[AXIS_Y]);
            simulation->position[AXIS_Z] = startZ + (fraction * simulation->velocity[AXIS_Z]);
            simulation->lastBounceStep = simulation->stepCount;
            simulation->spinSpeed /= BILLBOARD_HIT_SPIN_DIVISOR;
            simulation->sideSpin /= BILLBOARD_HIT_SPIN_DIVISOR;
            switch (triangle.tag) {
                case BILLBOARD_CELL_TRUNK:
                case BILLBOARD_CELL_SOLID:
                    simulation->velocity[AXIS_X] =
                        -simulation->velocity[AXIS_Z] / OPENING_BILLBOARD_HIT_SPEED_DIVISOR;
                    simulation->velocity[AXIS_Z] =
                        -simulation->velocity[AXIS_X] / OPENING_BILLBOARD_HIT_SPEED_DIVISOR;
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
            FindWallHit(
                startX,
                startY,
                startZ,
                simulation->position[AXIS_X],
                simulation->position[AXIS_Y],
                simulation->position[AXIS_Z],
                &f_OpeningContactTriangle,
                &f_OpeningContactFraction
            ) == TRUE &&
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
            }
            else {
                edgeX = f_OpeningContactTriangle.corners[AXIS_X][1];
                edgeZ = f_OpeningContactTriangle.corners[AXIS_Z][1];
            }

            normalX -= edgeX;
            normalY -= edgeY;
            normalZ -= edgeZ;
            guNormalize(&normalX, &normalY, &normalZ);
            approach = (-normalZ * simulation->velocity[AXIS_X]) +
                (normalY * simulation->velocity[AXIS_Y]) + (normalX * simulation->velocity[AXIS_Z]);
            f_OpeningContactFraction -= OPENING_WALL_BACK_OFF /
                GetOpeningLength3D(
                    simulation->velocity[AXIS_X],
                    simulation->velocity[AXIS_Y],
                    simulation->velocity[AXIS_Z]
                );
            if (f_OpeningContactFraction < 0.0f) {
                f_OpeningContactFraction = 0.0f;
            }

            hitPosition[AXIS_X] = startX +
                (OPENING_WALL_HIT_STEP_SHARE * f_OpeningContactFraction *
                    simulation->velocity[AXIS_X]);
            hitPosition[AXIS_Y] = startY +
                (OPENING_WALL_HIT_STEP_SHARE * f_OpeningContactFraction *
                    simulation->velocity[AXIS_Y]);
            hitPosition[AXIS_Z] = startZ +
                (OPENING_WALL_HIT_STEP_SHARE * f_OpeningContactFraction *
                    simulation->velocity[AXIS_Z]);
            simulation->position[AXIS_X] = hitPosition[AXIS_X];
            simulation->position[AXIS_Y] = hitPosition[AXIS_Y];
            simulation->position[AXIS_Z] = hitPosition[AXIS_Z];
            if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                StartFlagstickSparks(simulation->position);
            }

            {
                Vec3 reflected;
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
            (simulation->state == BALL_STATE_FLYING || simulation->state == BALL_STATE_ROLLING)) {
            TrianglePlane plane;
            s32 bounced[AXIS_COUNT];
            BallSpeed impactSpeed;
            f32 steepness;
            s32 speed;
            f32 softness;
            f32 bounce;
            f32 push;
            f32 keptFraction;

            simulation->bounceCount++;
            GetTrianglePlane(&triangle, &plane);
            simulation->position[AXIS_X] = startX + (fraction * simulation->velocity[AXIS_X]);
            simulation->position[AXIS_Y] = startY + (fraction * simulation->velocity[AXIS_Y]);
            simulation->position[AXIS_Z] = startZ + (fraction * simulation->velocity[AXIS_Z]);
            substepCount += (fraction < EARLY_HIT_FRACTION) & (substepCount == 1);
            if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
                simulation->surface = SURFACE_FAIRWAY;
            }
            else {
                IN_OWN_BLOCK(simulation->surface = SURFACE_WATER_HAZARD);
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

            impactSpeed = (plane.normal[AXIS_X] * simulation->velocity[AXIS_X]) +
                (plane.normal[AXIS_Y] * simulation->velocity[AXIS_Y]) +
                (plane.normal[AXIS_Z] * simulation->velocity[AXIS_Z]);
            speed = IntLength3D(
                simulation->velocity[AXIS_X],
                simulation->velocity[AXIS_Y],
                simulation->velocity[AXIS_Z]
            );
            if (speed != 0) {
                steepness = AbsFloat(impactSpeed) / speed;
            }
            else {
                steepness = 1.0f;
            }

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
            else if (simulation->surface == SURFACE_WATER_HAZARD) {
                if (simulation->state == BALL_STATE_ROLLING ||
                    (impactSpeed <= OPENING_SKIP_IMPACT_SPEED_MIN ||
                        impactSpeed >=
                            GetOpeningLength2D(
                                simulation->velocity[AXIS_X],
                                simulation->velocity[AXIS_Z]
                            ) * OPENING_SKIP_SPEED_RATIO)) {
                    simulation->state = BALL_STATE_IN_WATER;
                    simulation->velocity[AXIS_Z] = 0;
                    simulation->velocity[AXIS_X] = 0;
                }
            }

            softness =
                THOUSANDTHS_PER_ONE - GetOpeningSurfaceTraits(simulation->surface)->bounciness;
            keptFraction = ((f32)THOUSANDTHS_PER_ONE -
                               (SOFTNESS_SPEED_LOSS_SCALE * softness * steepness * steepness)) *
                ONE_PER_THOUSANDTH;
            if (simulation->stepCount < OPENING_EARLY_BOUNCE_STEP_COUNT) {
                keptFraction *= EARLY_BOUNCE_SPEED_SCALE;
            }

            bounce = 1.0f - (softness * steepness * ONE_PER_THOUSANDTH);
            push = (bounce + 1.0f) * impactSpeed;
            bounced[AXIS_X] = simulation->velocity[AXIS_X] - (plane.normal[AXIS_X] * push);
            bounced[AXIS_Y] = simulation->velocity[AXIS_Y] - (plane.normal[AXIS_Y] * push);
            bounced[AXIS_Z] = simulation->velocity[AXIS_Z] - (plane.normal[AXIS_Z] * push);
            simulation->velocity[AXIS_X] = bounced[AXIS_X] * keptFraction;
            simulation->velocity[AXIS_Y] = bounced[AXIS_Y] * keptFraction;
            simulation->velocity[AXIS_Z] = bounced[AXIS_Z] * keptFraction;
            simulation->lastBounceStep = simulation->stepCount;
            if (simulation->state == BALL_STATE_FLYING) {
                s32 spinVelocityX = simulation->spinSpeed * cosf(simulation->spinHeading);
                s32 spinVelocityZ = simulation->spinSpeed * sinf(simulation->spinHeading);
                f32 grip = 1.0f - bounce;

                simulation->velocity[AXIS_X] = simulation->velocity[AXIS_X] +
                    ((spinVelocityX - simulation->velocity[AXIS_X]) * grip *
                        AbsFloat(plane.normal[AXIS_Y]) * TRIANGLE_SPIN_GRIP);
                simulation->velocity[AXIS_Z] = simulation->velocity[AXIS_Z] +
                    ((spinVelocityZ - simulation->velocity[AXIS_Z]) * grip *
                        AbsFloat(plane.normal[AXIS_Y]) * TRIANGLE_SPIN_GRIP);
                spinVelocityX = spinVelocityX * BOUNCE_SPIN_DECAY;
                spinVelocityZ = spinVelocityZ * BOUNCE_SPIN_DECAY;
                spinVelocityX =
                    spinVelocityX + ((previousVelocityX - spinVelocityX) * TRIANGLE_SPIN_FOLLOW);
                spinVelocityZ =
                    spinVelocityZ + ((previousVelocityZ - spinVelocityZ) * TRIANGLE_SPIN_FOLLOW);
                simulation->spinHeading = atan2(spinVelocityZ, spinVelocityX);
                simulation->spinSpeed = IntLength2D(spinVelocityX, spinVelocityZ);
                BuildOpeningLiftRotation();
            }

            simulation->sideSpin = 0;
            simulation->velocity[AXIS_Y] += (s32)GRAVITY;
        }

        if (simulation->state == BALL_STATE_FLYING && contact == STEP_FLAG_GROUND_HIT) {
            TrianglePlane plane;
            BallSpeed impactSpeed;
            f32 steepness;
            s32 speed;
            f32 friction;
            f32 spin;
            f32 restitution;

            simulation->bounceCount++;
            GetTrianglePlane(&triangle, &plane);
            GetOpeningLength3D(
                simulation->velocity[AXIS_X],
                simulation->velocity[AXIS_Y],
                simulation->velocity[AXIS_Z]
            );
            simulation->position[AXIS_X] = startX + (fraction * simulation->velocity[AXIS_X]);
            simulation->position[AXIS_Y] = startY + (fraction * simulation->velocity[AXIS_Y]);
            simulation->position[AXIS_Z] = startZ + (fraction * simulation->velocity[AXIS_Z]);
            substepCount += (fraction < EARLY_HIT_FRACTION) & (substepCount == 1);
            if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
                simulation->surface = SURFACE_FAIRWAY;
            }
            else {
                simulation->surface =
                    GetSurface(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
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

            impactSpeed = (plane.normal[AXIS_X] * simulation->velocity[AXIS_X]) +
                (plane.normal[AXIS_Y] * simulation->velocity[AXIS_Y]) +
                (plane.normal[AXIS_Z] * simulation->velocity[AXIS_Z]);
            speed = IntLength3D(
                simulation->velocity[AXIS_X],
                simulation->velocity[AXIS_Y],
                simulation->velocity[AXIS_Z]
            );
            if (speed != 0) {
                steepness = AbsFloat(impactSpeed) / speed;
            }
            else {
                steepness = 1.0f;
            }

            friction =
                (THOUSANDTHS_PER_ONE - GetOpeningSurfaceTraits(simulation->surface)->bounciness) *
                ONE_PER_THOUSANDTH;
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
            else if (simulation->surface == SURFACE_WATER_HAZARD) {
                if (impactSpeed >= OPENING_WATER_STOP_IMPACT_SPEED) {
                    simulation->state = BALL_STATE_STOPPED;
                    simulation->position[AXIS_Y] =
                        GetGroundHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
                }
            }

            switch (simulation->surface) {
                case SURFACE_GREEN:
                case SURFACE_GREEN_EDGE:
                    friction *=
                        ((impactSpeed +
                             (impactSpeed * GetOpeningPositionNoise() * LANDING_NOISE_SCALE)) *
                            OPENING_GREEN_LANDING_FRICTION_PER_SPEED) +
                        OPENING_GREEN_LANDING_FRICTION_BASE;
                    spin = ((impactSpeed * steepness) - OPENING_SPINLESS_LANDING_SPEED) *
                        (1.0f / OPENING_LANDING_SPIN_SPEED_RANGE);
                    if (spin < 0.0f) {
                        spin = 0.0f;
                    }

                    break;
                default:
                    friction *=
                        (impactSpeed * OTHER_LANDING_FRICTION_PER_SPEED) + LANDING_FRICTION_BASE;
                    friction += friction * GetOpeningPositionNoise() * LANDING_NOISE_SCALE;
                    spin = ((impactSpeed * steepness) - OPENING_SPINLESS_LANDING_SPEED) *
                        (1.0f / OPENING_LANDING_SPIN_SPEED_RANGE);
                    if (spin < 0.0f) {
                        spin = 0.0f;
                    }

                    break;
            }

            restitution = 1.0f - friction;
            if (restitution < 0.0f) {
                restitution = 0.0f;
            }
            else if (restitution > RESTITUTION_MAX) {
                restitution = RESTITUTION_MAX;
            }

            if (simulation->state != BALL_STATE_ROLLING &&
                impactSpeed < ROLL_START_SPEED_MAX &&
                simulation->stepCount - 1 == simulation->lastBounceStep) {
                s32 spinVelocityX;
                s32 spinVelocityZ;

                simulation->state = BALL_STATE_ROLLING;
                if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
                    simulation->position[AXIS_Y] = 0;
                }
                else {
                    simulation->position[AXIS_Y] =
                        GetGroundHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
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
            }
            else {
                f32 keptFraction = 1.0f - spin;
                s32 bounced[AXIS_COUNT];
                f32 push;
                s32 spinVelocityX;
                s32 spinVelocityZ;
                f32 grip;
                s32 driftX;
                s32 driftZ;
                f32 driftLimit;
                f32 drift;

                if (keptFraction <= BOUNCE_SPEED_KEPT_MIN) {
                    keptFraction = BOUNCE_SPEED_KEPT_MIN;
                }

                if (keptFraction > 1.0f) {
                    keptFraction = 1.0f;
                }

                if (simulation->stepCount < EARLY_BOUNCE_STEP_COUNT && !g_GameState.isMiniGolf) {
                    keptFraction *= EARLY_BOUNCE_SPEED_SCALE;
                    osSyncPrintf("Ouch! %d, %d\n", startY, simulation->position[AXIS_Y]);
                }

                push = (restitution + 1.0f) * impactSpeed;
                bounced[AXIS_X] = simulation->velocity[AXIS_X] - (plane.normal[AXIS_X] * push);
                bounced[AXIS_Y] = simulation->velocity[AXIS_Y] - (plane.normal[AXIS_Y] * push);
                bounced[AXIS_Z] = simulation->velocity[AXIS_Z] - (plane.normal[AXIS_Z] * push);
                simulation->velocity[AXIS_X] = bounced[AXIS_X] * keptFraction;
                simulation->velocity[AXIS_Y] = bounced[AXIS_Y] * keptFraction;
                simulation->velocity[AXIS_Z] = bounced[AXIS_Z] * keptFraction;
                simulation->lastBounceStep = simulation->stepCount;
                spinVelocityX = simulation->spinSpeed * cosf(simulation->spinHeading);
                spinVelocityZ = simulation->spinSpeed * sinf(simulation->spinHeading);
                grip = 1.0f - restitution;
                driftX = (spinVelocityX - simulation->velocity[AXIS_X]) * grip *
                    AbsFloat(plane.normal[AXIS_Y]) * OPENING_GROUND_SPIN_DRIFT_GRIP;
                driftZ = (spinVelocityZ - simulation->velocity[AXIS_Z]) * grip *
                    AbsFloat(plane.normal[AXIS_Y]) * OPENING_GROUND_SPIN_DRIFT_GRIP;
                drift = (s32)IntLength2D(driftX, driftZ);
                driftLimit = impactSpeed * OPENING_SPIN_DRIFT_SHARE_MAX;
                if (driftLimit < drift && impactSpeed > 1.0f) {
                    f32 scale = driftLimit / drift;

                    driftX = driftX * scale;
                    driftZ = driftZ * scale;
                }

                spinVelocityX = spinVelocityX * BOUNCE_SPIN_DECAY;
                spinVelocityZ = spinVelocityZ * BOUNCE_SPIN_DECAY;
                simulation->velocity[AXIS_X] += driftX;
                simulation->velocity[AXIS_Z] += driftZ;
                spinVelocityX =
                    spinVelocityX + ((previousVelocityX - spinVelocityX) * GROUND_SPIN_FOLLOW);
                spinVelocityZ =
                    spinVelocityZ + ((previousVelocityZ - spinVelocityZ) * GROUND_SPIN_FOLLOW);
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
            simulation->spinSpeed =
                IntLength2D(simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]);
            BuildOpeningLiftRotation();
        }

        if (simulation->state == BALL_STATE_ROLLING && !isNearCup) {
            TrianglePlane plane;
            SurfaceTraits *surfaceTraits;
            s32 groundHeight;
            s32 slopeX;
            s32 slopeZ;
            f32 friction;
            Boolean isSteep;

            rollSpeed =
                GetOpeningLength2D(simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]);
            isSteep = FALSE;
            if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
                contact = STEP_FLAG_GROUND_HIT;
                surfaceTraits = &f_OpeningSurfaceTraits[SURFACE_GREEN];
                SetFlatTriangle(&triangle);
                if (f_OpeningClubTestMode == CLUB_TEST_SHOT) {
                    surfaceTraits = &f_OpeningSurfaceTraits[SURFACE_FAIRWAY];
                }

                groundHeight = 0;
                GetTrianglePlane(&triangle, &plane);
            }
            else {
                GetTrianglePlane(&triangle, &plane);
                if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST && g_GameState.isMiniGolf) {
                    BallSpeed pressing = (plane.normal[AXIS_X] * simulation->velocity[AXIS_X]) +
                        (plane.normal[AXIS_Y] * simulation->velocity[AXIS_Y]) +
                        (plane.normal[AXIS_Z] * simulation->velocity[AXIS_Z]);

                    if (pressing >= OPENING_ROLL_JUMP_SPEED) {
                        Vec3 along;

                        along[AXIS_X] =
                            simulation->velocity[AXIS_X] - (plane.normal[AXIS_X] * pressing);
                        along[AXIS_Y] =
                            simulation->velocity[AXIS_Y] - (plane.normal[AXIS_Y] * pressing);
                        along[AXIS_Z] =
                            simulation->velocity[AXIS_Z] - (plane.normal[AXIS_Z] * pressing);
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

                simulation->surface =
                    GetSurface(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
                switch (simulation->surface) {
                    case SURFACE_FAIRWAY:
                        if (!g_GameState.isMiniGolf) {
                            f32 lie = GetOpeningStepNoise();

                            if (lie < FAIRWAY_POOR_LIE_CHANCE) {
                                simulation->surface = SURFACE_FAIRWAY_POOR_LIE;
                            }
                            else if (lie < FAIRWAY_POOR_LIE_CHANCE + FAIRWAY_FAIR_LIE_CHANCE) {
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

                surfaceTraits = GetOpeningSurfaceTraits(simulation->surface);
                if (surfaceTraits->kind == SURFACE_KIND_OUT_OF_COURSE) {
                    simulation->state = BALL_STATE_STOPPED;
                    goto SETTLE;
                }

                groundHeight =
                    GetGroundHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
                if (simulation->position[AXIS_Y] + TAKE_OFF_HEIGHT < groundHeight) {
                    simulation->state = BALL_STATE_FLYING;
                    simulation->spinHeading =
                        atan2(simulation->velocity[AXIS_Z], simulation->velocity[AXIS_X]);
                    contact = 0;
                    simulation->spinSpeed =
                        IntLength2D(simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]);
                    BuildOpeningLiftRotation();
                    goto ROLLED;
                }
            }

            slopeX = (-plane.normal[AXIS_X] * GRAVITY) / AbsFloat(plane.normal[AXIS_Y]);
            slopeZ = (-plane.normal[AXIS_Z] * GRAVITY) / AbsFloat(plane.normal[AXIS_Y]);
            if (plane.normal[AXIS_Y] * plane.normal[AXIS_Y] <=
                    (plane.normal[AXIS_X] * plane.normal[AXIS_X]) +
                        (plane.normal[AXIS_Z] * plane.normal[AXIS_Z]) &&
                simulation->stepCount < STEEP_SLOPE_STEP_LIMIT) {
                isSteep = TRUE;
            }

            friction = surfaceTraits->rollSpeedKept * ONE_PER_THOUSANDTH;
            if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST &&
                g_HoleSetup.conditions.isRaining == TRUE) {
                friction *= RAIN_FRICTION_SCALE;
            }

            if (isSteep) {
                slopeX *= STEEP_SLOPE_SCALE;
                slopeZ *= STEEP_SLOPE_SCALE;
                if (friction < STEEP_FRICTION_MIN) {
                    friction = STEEP_FRICTION_MIN;
                }
            }
            else {
                if ((f32)rollSpeed < SLOW_ROLL_SPEED) {
                    f32 scale = (rollSpeed * OPENING_SLOW_ROLL_SLOPE_SCALE_PER_SPEED) +
                        OPENING_SLOW_ROLL_SLOPE_SCALE_MIN;

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

            simulation->velocity[AXIS_X] =
                (slopeX * friction * SLOPE_PULL_SCALE) + (simulation->velocity[AXIS_X] * friction);
            simulation->velocity[AXIS_Z] =
                (slopeZ * friction * SLOPE_PULL_SCALE) + (simulation->velocity[AXIS_Z] * friction);
            if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST && g_GameState.isMiniGolf) {
                if (groundHeight - startY < simulation->velocity[AXIS_Y]) {
                    simulation->velocity[AXIS_Y] =
                        groundHeight - startY + (s32)MINI_GOLF_ROLL_GRAVITY;
                }
                else {
                    simulation->velocity[AXIS_Y] += (s32)MINI_GOLF_ROLL_GRAVITY;
                }
            }
            else {
                if (groundHeight - startY < simulation->velocity[AXIS_Y]) {
                    simulation->velocity[AXIS_Y] = groundHeight - startY + (s32)GRAVITY;
                }
                else {
                    simulation->velocity[AXIS_Y] += (s32)GRAVITY;
                }
            }

            {
                s32 groundGap = groundHeight - startY;

                if (groundGap < 0) {
                    groundGap = -groundGap;
                }
            }

            if (simulation->purpose == LAUNCH_PURPOSE_STROKE &&
                IsGameFlagSet(GAME_FLAG_LOG_VERTICAL_VELOCITY)) {
                osSyncPrintf("V[1] %d\n", simulation->velocity[AXIS_Y]);
            }

            {
                s32 speed = IntLength2D(simulation->velocity[AXIS_X], simulation->velocity[AXIS_Z]);

                if (speed < STOP_SPEED_MAX) {
                    if (!isSteep && IntLength2D(slopeX, slopeZ) * friction < STOP_SLOPE_PULL_MAX) {
                        simulation->state = BALL_STATE_STOPPED;
                    }
                }
                else {
                    SoundVolume volume = speed * ROLL_VOLUME_PER_SPEED;

                    if (volume > ROLL_VOLUME_MAX) {
                        volume = ROLL_VOLUME_MAX;
                    }
                }
            }

            if (!isSteep &&
                surfaceTraits->kind != SURFACE_KIND_GREEN &&
                simulation->lastBounceStep + OFF_GREEN_STOP_STEPS < simulation->stepCount &&
                GetOpeningLength3D(
                    simulation->velocity[AXIS_X],
                    simulation->velocity[AXIS_Y],
                    simulation->velocity[AXIS_Z]
                ) < OFF_GREEN_STOP_SPEED_MAX) {
                simulation->state = BALL_STATE_STOPPED;
            }
        }

ROLLED:

        if ((simulation->state == BALL_STATE_FLYING || simulation->state == BALL_STATE_ROLLING) &&
            contact == 0) {
            Vec3 relative;
            Vec3 turned;
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

            relative[AXIS_X] = simulation->velocity[AXIS_X] - simulation->windVelocityX;
            relative[AXIS_Y] = simulation->velocity[AXIS_Y];
            relative[AXIS_Z] = simulation->velocity[AXIS_Z] - simulation->windVelocityZ;
            airSpeed = GetOpeningLength3D(relative[AXIS_X], relative[AXIS_Y], relative[AXIS_Z]);
            drag =
                (sqrtf(airSpeed * AIR_DRAG_SPEED_SCALE) * AIR_DRAG_PER_ROOT_SPEED) + AIR_DRAG_MIN;
            horizontalSpeed = GetOpeningLength2D(relative[AXIS_X], relative[AXIS_Z]);
            sideSpin = simulation->sideSpin;
            sideSpinMagnitude = AbsFloat(sideSpin * SIDE_SPIN_SPEED_SCALE);
            curveRatio = 0.0f;
            if (sideSpinMagnitude != 0.0f) {
                curveRatio = horizontalSpeed / sideSpinMagnitude;
            }

            curveWeight = 1.0f - (AbsFloat(curveRatio - 1.0f) * SIDE_SPIN_MISMATCH_FALLOFF);
            if (curveWeight < 0.0f) {
                curveWeight = 0.0f;
            }

            curve = sideSpin * CURVE_PER_SIDE_SPIN * curveWeight;
            if (horizontalSpeed < AbsFloat(curve) * CURVE_SPEED_RATIO_MAX) {
                curve = horizontalSpeed * (1.0f / CURVE_SPEED_RATIO_MAX);
                if (sideSpin < 0) {
                    curve = -curve;
                }
            }

            if (horizontalSpeed > CURVE_SPEED_MIN) {
                f32 scale = (1.0f / horizontalSpeed) * curve;
                f32 curveX = -relative[AXIS_Z] * scale;
                f32 curveZ = relative[AXIS_X] * scale;

                simulation->velocity[AXIS_X] += curveX;
                simulation->velocity[AXIS_Z] += curveZ;
            }

            simulation->sideSpin = simulation->sideSpin * SIDE_SPIN_DECAY;
            spinMagnitude = AbsFloat(simulation->spinSpeed * OPENING_SPIN_SPEED_SCALE);
            liftRatio = 0.0f;
            if (spinMagnitude != 0.0f) {
                liftRatio = airSpeed / spinMagnitude;
            }

            liftWeight = 1.0f - (AbsFloat(liftRatio - 1.0f) * OPENING_SPIN_MISMATCH_FALLOFF);
            if (liftWeight < 0.0f) {
                liftWeight = 0.0f;
            }

            liftPerSpin = liftWeight * OPENING_LIFT_PER_SPIN;
            lift = (simulation->spinSpeed * OPENING_LIFT_SPIN_SCALE) * liftPerSpin;
            if (simulation->spinSpeed != 0 && airSpeed > LIFT_AIR_SPEED_MIN) {
                guMtxXFMF(
                    simulation->liftRotation,
                    relative[AXIS_X],
                    relative[AXIS_Y],
                    relative[AXIS_Z],
                    &turned[AXIS_X],
                    &turned[AXIS_Y],
                    &turned[AXIS_Z]
                );
                if (airSpeed < AbsFloat(lift) * OPENING_LIFT_SPEED_RATIO_MAX) {
                    lift = airSpeed * (1.0f / OPENING_LIFT_SPEED_RATIO_MAX);
                }

                ScaleToOpeningLength(lift, &turned[AXIS_X], &turned[AXIS_Y], &turned[AXIS_Z]);
                simulation->velocity[AXIS_X] = simulation->velocity[AXIS_X] + turned[AXIS_X];
                simulation->velocity[AXIS_Y] = simulation->velocity[AXIS_Y] + turned[AXIS_Y];
                simulation->velocity[AXIS_Z] = simulation->velocity[AXIS_Z] + turned[AXIS_Z];
                simulation->spinSpeed = simulation->spinSpeed * OPENING_SPIN_DECAY;
            }

            height = simulation->startPosition[AXIS_Y] - simulation->position[AXIS_Y];
            if (height <= 0.0f) {
                windX = simulation->windVelocityX * LOW_WIND_SCALE;
                windZ = simulation->windVelocityZ * LOW_WIND_SCALE;
            }
            else {
                height = (height * OPENING_WIND_SCALE_PER_HEIGHT) + LOW_WIND_SCALE;
                windX = simulation->windVelocityX * height;
                windZ = simulation->windVelocityZ * height;
            }

            windX = (windX - simulation->velocity[AXIS_X]) * drag;
            windZ = (windZ - simulation->velocity[AXIS_Z]) * drag;
            simulation->velocity[AXIS_X] = simulation->velocity[AXIS_X] + windX;
            simulation->velocity[AXIS_Z] = simulation->velocity[AXIS_Z] + windZ;
            simulation->velocity[AXIS_Y] =
                simulation->velocity[AXIS_Y] + ((-simulation->velocity[AXIS_Y] * drag) + GRAVITY);
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
                stepFlags |= STEP_FLAG_OUTSIDE_COURSE;
            }
        }

SETTLE:

        {
            s32 floorHeight;

            if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
                floorHeight = 0;
            }
            else if (simulation->state == BALL_STATE_IN_WATER) {
                floorHeight =
                    GetGroundHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
            }
            else {
                floorHeight =
                    GetGroundHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
                if (simulation->state == BALL_STATE_IN_CUP || simulation->isHoledOut) {
                    floorHeight += IN_CUP_FLOOR_DEPTH;
                }
                else if (simulation->state == BALL_STATE_ROLLING) {
                    s32 cupOffsetX = simulation->position[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X];

                    if (Abs(cupOffsetX) < CUP_FLOOR_SEARCH_SPAN) {
                        s32 cupOffsetZ =
                            simulation->position[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z];

                        if (Abs(cupOffsetZ) < CUP_FLOOR_SEARCH_SPAN) {
                            f32 cupDistance = (s32)IntLength2D(cupOffsetX, cupOffsetZ);

                            if (cupDistance < CUP_DROP_RADIUS) {
                                floorHeight += CUP_DEPTH;
                            }
                            else if (cupDistance < CUP_RIM_OUTER_RADIUS) {
                                f32 rimDistance = CUP_RIM_OUTER_RADIUS - cupDistance;
                                f32 remainingShare = (CUP_RIM_CURVE_RADIUS * CUP_RIM_CURVE_RADIUS) -
                                    (rimDistance * rimDistance);

                                if (remainingShare > 0.0f) {
                                    floorHeight = floorHeight +
                                        ((CUP_RIM_CURVE_RADIUS - sqrtf(remainingShare)) *
                                            CUP_RIM_SINK_SCALE);
                                }
                            }
                        }
                    }
                }
            }

            if (floorHeight < simulation->position[AXIS_Y]) {
                simulation->position[AXIS_Y] = floorHeight;
            }

            if (simulation->position[AXIS_Y] < -BALL_ALTITUDE_MAX) {
                simulation->position[AXIS_Y] = -BALL_ALTITUDE_MAX;
            }

            if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
                SurfaceKind surfaceKind;
                PlaneTriangle ground;
                TrianglePlane plane;

                simulation->surfaceBelow =
                    GetSurface(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
                surfaceKind = GetOpeningSurfaceTraits(simulation->surfaceBelow)->kind;
                if (IsDropKind(surfaceKind) &&
                    FindGroundTriangle(
                        simulation->position[AXIS_X],
                        simulation->position[AXIS_Z],
                        &ground
                    ) == 0 &&
                    GetTrianglePlane(&ground, &plane) == OUTCOME_OK &&
                    IsDropSlope(plane)) {
                    simulation->dropPosition[AXIS_X] = simulation->position[AXIS_X];
                    simulation->dropPosition[AXIS_Y] = simulation->position[AXIS_Y];
                    simulation->dropPosition[AXIS_Z] = simulation->position[AXIS_Z];
                }
            }
        }

        simulation->stepFlags = stepFlags;
    }
    while (++substep < substepCount);

    return stepFlags;
}

#define OPENING_OFFSET_SIDE_SPIN_SCALE 2
#define OPENING_LAUNCH_SPIN_SPEED_LOSS 0.2f

s32 LaunchOpeningBallSimulation(
    BallSimulation *simulation,
    LaunchPurpose purpose,
    ShotLaunch *shot
) {
    s32 percentRange[2];
    Club *club;
    ImpactEffect *impactEffect;
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

    f32 ChooseOpeningLieBlend(void) {
        return 0.0f;
    }

    simulation->purpose = purpose;
    f_UnusedOpeningLaunchCount++;
    sideSign = 1;
    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        groundHeight = GetGroundHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z]) -
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
    simulation->hasEnteredWater = FALSE;
    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        simulation->playerIndex = shot->playerIndex;
    }
    else {
        simulation->playerIndex = 0;
    }

    club = &f_OpeningClubs[shot->clubSlot];
    impactEffect = &f_OpeningImpactEffects[0][IMPACT_TIMING_ENTRY_LIMIT];
    rumbles = f_OpeningImpactEffects[0][IMPACT_TIMING_ENTRY_LIMIT].rumbles;
    if ((club->surfaceMask & CLUB_SURFACE_GREEN) &&
        simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        speed = GetPuttPower(shot->power, shot->shotMode);
    }
    else {
        speed = GetClubPower(club, shot->shotMode) *
            (shot->power + (impactEffect->speedChange * (LAUNCH_SCALE_ONE / SPEED_CHANGE_UNITS))) /
            LAUNCH_POWER_MAX;
        if (speed <= 0) {
            speed = LAUNCH_SPEED_MIN;
        }
    }

    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        SurfaceTraits *surfaceTraits = GetOpeningSurfaceTraits(simulation->launchSurface);

        blend = ChooseOpeningLieBlend();
        GetSurfacePercentRange(surfaceTraits, club, percentRange);
        speed = ((percentRange[0] * blend) + (percentRange[1] * (1.0f - blend))) * speed *
            ONE_PER_PERCENT;
    }
    else {
        s32 percent = CLUB_TEST_SPEED_HALF_PERCENTS;

        speed = speed * percent / HALF_PERCENTS_PER_ONE;
    }

    pitch = (club->pitch + impactEffect->pitchChange) * DEGREES_TO_RADIANS;
    if (pitch >= 0.0f) {
        pitch = 0.0f;
    }

    pitch +=
        (f32)club->offsetPitch * shot->offsetY * (DEGREES_TO_RADIANS / OFFSET_Y_PER_PITCH_STEP);
    yawRadians = (u16)shot->heading * BINARY_ANGLE_TO_RADIANS;
    groundHeight = club->sideSpin;
    groundHeight *= CLUB_SIDE_SPIN_SCALE;
    simulation->sideSpin = (((((sideSign * impactEffect->sideSpinChange) + shot->offsetX) *
                                 OPENING_OFFSET_SIDE_SPIN_SCALE) +
                                (sideSign * groundHeight)) *
                               speed) *
        SIDE_SPIN_PER_SPEED;
    simulation->spinSize = 0.0f;
    spinBase = club->spin * CLUB_SPIN_BASE_SCALE;
    simulation->spinSpeed =
        (((spinBase + CLUB_TEST_SPIN_OFFSET) * CLUB_TEST_SPIN_PER_OFFSET * shot->offsetY) -
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
        speed = LAUNCH_SPEED_MIN;
    }

    simulation->velocity[AXIS_Y] = sinf(pitch) * speed;
    simulation->velocity[AXIS_X] = cosf(yawRadians) * speed;
    simulation->velocity[AXIS_Z] = sinf(yawRadians) * speed;
    osSyncPrintf(
        "SVX%f, SVY%f, SVZ%f\n",
        simulation->velocity[AXIS_X] * (1.0f / KilometersPerHour(100)),
        simulation->velocity[AXIS_Y] * (1.0f / KilometersPerHour(100)),
        simulation->velocity[AXIS_Z] * (1.0f / KilometersPerHour(100))
    );
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
    simulation->isHoledOut = FALSE;
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

            sprintf(text, "ANGLEY = %3.3d/360", (s32)(yawRadians * (f32)RADIANS_TO_DEGREES));
            PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_YAW);
            sprintf(text, "ANGLEX = %3.3d/360", (s32)(pitch * (f32)RADIANS_TO_DEGREES));
            PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_PITCH);
            sprintf(
                text,
                "Velocity = %fm/s     ",
                speed * (1.0f / SPEED_UNITS_PER_METER_PER_SECOND)
            );
            PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SPEED);
            sprintf(text, "SpinY = %d", simulation->sideSpin);
            PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SIDE_SPIN);
            sprintf(text, "SpinX = %d", simulation->spinSpeed);
            PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SPIN);
            sprintf(text, "SpinVel = %fRPM      ", simulation->spinSpeed * RPM_PER_SPIN_SPEED_UNIT);
            PrintDebugText(text, LAUNCH_VALUE_COLUMN, LAUNCH_VALUE_ROW_SPIN_RPM);
            sprintf(
                text,
                "SpinAngle = %3.3d/360",
                (s32)(simulation->spinHeading * (f32)RADIANS_TO_DEGREES)
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
