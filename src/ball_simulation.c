#include "ball_simulation.h"

#include <math.h>
#include <stdlib.h>
#include <ultra64.h>

#define WIDENS_AIM_SLOPE_ANGLE

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
#include "hole_creatures.h"
#include "hole_setup.h"
#include "hole_triangles.h"
#include "language.h"
#include "math_util.h"
#include "player.h"
#include "scene.h"
#include "shot_camera.h"
#include "shot_effects.h"
#include "sound.h"
#include "surface.h"
#include "terrain.h"
#include "turn.h"

BallSimulation g_TurnBallSimulation;

BallSimulation g_BallSimulations[BALL_SIMULATION_COUNT] = {0};

ClubTest g_ClubTestMode = CLUB_TEST_NONE;

static SurfaceTraits f_SurfaceTraits[SURFACE_COUNT] = {
    {SURFACE_KIND_OUT_OF_BOUNDS, "O.B.", 100, 100, 0, 0, 0, 0x500, SINK_DEPTH_HALF, 7},
    {SURFACE_KIND_FAIRWAY, "FAIRWAY", 98, 100, 5, 960, 500, 0x501, SINK_DEPTH_NONE, 0},
    {SURFACE_KIND_ROUGH, "ROUGH", 80, 90, 30, 600, 200, 0x508, SINK_DEPTH_HALF, 1},
    {SURFACE_KIND_BUNKER, "BUNKER", 70, 85, 30, 600, 0, 0x504, SINK_DEPTH_NONE, 2},
    {SURFACE_KIND_GREEN, "ON GREEN", 100, 100, 0, 972, 600, 0x50A, SINK_DEPTH_NONE, 6},
    {SURFACE_KIND_WATER_HAZARD, "WATER HAZARD", 100, 100, 0, 900, 400, 0x50C, SINK_DEPTH_NONE, 3},
    {SURFACE_KIND_CART_WAY, "CART WAY", 80, 100, 10, 975, 890, 0x50B, SINK_DEPTH_NONE, 4},
    {SURFACE_KIND_TEE_GROUND, "TEE GROUND", 98, 100, 10, 930, 500, 0x501, SINK_DEPTH_NONE, 8},
    {SURFACE_KIND_ROCK, "ROCK", 80, 90, 30, 930, 800, 0x50D, SINK_DEPTH_NONE, 9},
    {SURFACE_KIND_GREEN_EDGE, "GREEN EDGE", 98, 100, 10, 966, 550, 0x501, SINK_DEPTH_NONE, 0},
    {SURFACE_KIND_SEMI_ROUGH, "SEMI ROUGH", 90, 98, 30, 930, 500, 0x507, SINK_DEPTH_NONE, 0},
    {SURFACE_KIND_BARE_GROUND, "BARE GROUND", 80, 90, 30, 930, 500, 0x50C, SINK_DEPTH_NONE, 1},
    {SURFACE_KIND_DEEP_ROUGH, "DEEP ROUGH", 50, 80, 30, 430, 100, 0x509, SINK_DEPTH_BURIED, 5},
    {SURFACE_KIND_NO_DROP_GROUND, "BARE GROUND", 80, 90, 30, 930, 500, 0x50C, SINK_DEPTH_NONE, 1},
    {SURFACE_KIND_WASTE_AREA, "WASTE AREA", 50, 80, 30, 430, 100, 0x8F1, SINK_DEPTH_HALF, 5},
    {SURFACE_KIND_ROCK, "            ", 98, 100, 40, 930, 500, 0x500, SINK_DEPTH_NONE, 9},
    {SURFACE_KIND_BUNKER_FAIR_LIE, "BUNKER", 50, 70, 30, 600, 0, 0x505, SINK_DEPTH_HALF, 2},
    {SURFACE_KIND_BUNKER_POOR_LIE, "BUNKER", 40, 60, 30, 600, 0, 0x506, SINK_DEPTH_HALF, 2},
    {SURFACE_KIND_BUNKER_UNUSED_LIE, "BUNKER", 40, 60, 40, 600, 0, 0x506, SINK_DEPTH_HALF, 2},
    {SURFACE_KIND_FAIRWAY_FAIR_LIE, "FAIRWAY", 94, 98, 7, 950, 380, 0x502, SINK_DEPTH_NONE, 0},
    {SURFACE_KIND_FAIRWAY_POOR_LIE, "FAIRWAY", 90, 94, 7, 950, 380, 0x503, SINK_DEPTH_NONE, 0},
    {SURFACE_KIND_ROUGH_POOR_LIE, "ROUGH", 70, 85, 30, 600, 200, 0x90C, SINK_DEPTH_HALF, 5},
    {SURFACE_KIND_OUT_OF_COURSE, "OUT OF COURSE", 100, 100, 0, 0, 0, 0x500, SINK_DEPTH_NONE, 7},
    {SURFACE_KIND_TEE_UP, "TEE UP", 98, 100, 10, 930, 500, 0x500, SINK_DEPTH_NONE, 8},
    {SURFACE_KIND_GREEN, "GREEN", 100, 100, 0, 972, 500, 0x8F2, SINK_DEPTH_NONE, 6},
    {SURFACE_KIND_FAIRWAY, "BLUE", 98, 100, 5, 960, 400, 0x8F3, SINK_DEPTH_NONE, 0},
    {SURFACE_KIND_CART_WAY, "RED", 80, 100, 10, 980, 800, 0x8F4, SINK_DEPTH_NONE, 4},
};

static ImpactEffect f_EarlyImpactEffect = {-90, -64, -40, -10, 100, 1};
static ImpactEffect f_LateImpactEffect = {-80, 64, 30, 10, -5, 1};
static ImpactEffect f_ImpactEffects[LIE_RATING_MAX + 1][IMPACT_TIMING_ENTRY_COUNT] = {
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
static s32 f_UnusedWordsAfterLaunchCount[4] UNUSED = {0};

SurfaceTraits *GetSurfaceTraits(Surface surface) {
    if ((u32)surface >= SURFACE_COUNT) {
        surface = SURFACE_OUT_OF_BOUNDS;
    }

    return &f_SurfaceTraits[surface];
}

void ResetBallSimulations(void) {
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

StepFlags StepBallSimulation(BallSimulation *simulation) {
    PlaneTriangle hit;
    PlaneTriangle triangle;
    BallSpeed velocity[AXIS_COUNT];
    f32 hitFraction;
    f32 fraction;
    s32 previousVelocityX;
    s32 previousVelocityZ;
    StepFlags stepFlags = 0;
    s32 substep;
    s32 substepCount = 1;
    Boolean printsDebugText = FALSE;
    s32 startX;
    s32 startY;
    s32 startZ;
    StepFlags contact;
    Boolean hitsHoleTriangle;
    Boolean isNearCup;
    s32 rollSpeed;
    Boolean isSteep;

    void PlayStrokeImpactSound(SoundId soundId, BallSpeed fullSpeed, BallSpeed speed) {
        SoundVolume volume = speed * SOUND_FULL_SCALE / fullSpeed;

        if (volume >= HIT_SOUND_VOLUME_MIN) {
            if (volume > SOUND_FULL_SCALE) {
                volume = SOUND_FULL_SCALE;
            }

            if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                PlaySound(soundId, g_NextSoundChannel, volume);
            }
        }
    }

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
        void PlayImpactSound(SoundId soundId, BallSpeed fullSpeed, BallSpeed speed) {
            SoundVolume volume = speed * SOUND_FULL_SCALE / fullSpeed;

            if (volume >= HIT_SOUND_VOLUME_MIN) {
                if (volume > SOUND_FULL_SCALE) {
                    volume = SOUND_FULL_SCALE;
                }

                PlaySound(soundId, g_NextSoundChannel, volume);
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

    inline f32 GetStepNoise(void) {
        f32 noise = (u32)((simulation->position[AXIS_X] + simulation->position[AXIS_Z] +
                              g_Players[simulation->playerIndex].noiseSeed) &
                        NOISE_BYTE_MASK) /
            NOISE_BYTE_PAIR_SUM_MAX;

        noise += (u32)((simulation->position[AXIS_X] - simulation->position[AXIS_Z] +
                           g_Players[simulation->playerIndex].noiseSeed) &
                     NOISE_BYTE_MASK) /
            NOISE_BYTE_PAIR_SUM_MAX;
        return noise;
    }

    velocity[AXIS_X] = simulation->velocity[AXIS_X];
    velocity[AXIS_Y] = simulation->velocity[AXIS_Y];
    velocity[AXIS_Z] = simulation->velocity[AXIS_Z];
    fraction = 1.0f;
    for (substep = 0; substep < substepCount; substep++) {
        if (substep != 0 && simulation->purpose == LAUNCH_PURPOSE_SHOT_PREVIEW) {
            break;
        }

        startY = simulation->position[AXIS_Y];
        isNearCup = FALSE;
        startX = simulation->position[AXIS_X];
        startZ = simulation->position[AXIS_Z];
        previousVelocityX = velocity[AXIS_X];
        previousVelocityZ = velocity[AXIS_Z];
        simulation->previousStepFlags = simulation->stepFlags;
        hitsHoleTriangle = FALSE;
        if (simulation->state == BALL_STATE_STOPPED) {
            goto SETTLE;
        }

        if (simulation->stepCount == 0) {
            BuildLiftRotation();
        }

        if (++simulation->stepCount >= STEP_COUNT_MAX) {
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
                s32 endY;
                Boolean found;

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
                    ) > 0) {
                    s32 ringIndex = hit.tag;

                    if (g_Players[simulation->playerIndex].ringStates[ringIndex] ==
                            RING_STATE_SHOWN &&
                        simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                        PlaySound(SOUND_IMPACT_ON_MARK, RING_SOUND_CHANNEL, SOUND_FULL_SCALE);
                        if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                            g_Players[simulation->playerIndex].ringStates[ringIndex] =
                                RING_STATE_PASSED;
                            StartRingPassRumble(&g_Players[simulation->playerIndex]);
                        }
                    }
                }

                if (simulation->state == BALL_STATE_ROLLING && simulation->isOverCup) {
                    s32 groundHeight = GetGroundHeight(startX, startZ);

                    if (groundHeight < startY) {
                        startY = groundHeight;
                    }

                    simulation->isOverCup = FALSE;
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
                        if (found && (contact == 0 || fraction < hitFraction)) {
                            contact = found;
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

                found = FALSE;
                if (simulation->state == BALL_STATE_FLYING &&
                    (simulation->lastBounceStep < 0 ||
                        simulation->lastBounceStep + BILLBOARD_HIT_BOUNCE_GAP <
                            simulation->stepCount)) {
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

                if (found && contact == STEP_FLAG_GROUND_HIT) {
                    if (hitFraction <= fraction) {
                        triangle = hit;
                        fraction = hitFraction;
                        contact = STEP_FLAG_GROUND_HIT;
                    }
                    else {
                        contact = STEP_FLAG_BILLBOARD_HIT;
                    }
                }
                else if (found) {
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
                        found = FindHoleTriangleHit(
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
                        found = FALSE;
                    }
                }
                else {
                    found = FALSE;
                }

                if (found) {
                    if (contact == 0 || hitFraction <= fraction) {
                        triangle = hit;
                        contact = STEP_FLAG_HOLE_TRIANGLE_HIT;
                        fraction = hitFraction;
                    }

                    hitsHoleTriangle = TRUE;
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

            if (g_GameState.isMiniGolf ||
                GetSurfaceTraits(simulation->launchSurface)->kind != SURFACE_KIND_GREEN) {
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
                                volume = KNOCK_VOLUME_MAX;
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
                        f32 remainingShare = PIN_HIT_STEP_SHARE - pinFraction;

                        simulation->position[AXIS_X] =
                            simulation->position[AXIS_X] + (remainingShare * velocity[AXIS_X]);
                        simulation->position[AXIS_Y] =
                            simulation->position[AXIS_Y] + (remainingShare * velocity[AXIS_Y]);
                        simulation->position[AXIS_Z] =
                            simulation->position[AXIS_Z] + (remainingShare * velocity[AXIS_Z]);
                    }

                    stepFlags |= STEP_FLAG_PIN_HIT;
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
                                -CUP_ENTRY_HEIGHT_MAX &&
                            (f32)cupDistance < CUP_RADIUS) {
                            s32 speed = IntLength2D(velocity[AXIS_X], velocity[AXIS_Z]);

                            if (speed < CUP_IN_SPEED_MAX &&
                                velocity[AXIS_Y] >= -CUP_IN_RISE_SPEED_MAX) {
                                if (printsDebugText) {
                                    SetDebugTextHighlight(TRUE);
                                    PrintDebugText(
                                        "CUPIN!",
                                        CUP_DEBUG_TEXT_COLUMN,
                                        CUP_DEBUG_TEXT_ROW
                                    );
                                }

                                simulation->state = BALL_STATE_IN_CUP;
                                if (speed < CUP_IN_SOUND_SPEED_MAX) {
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
                                ) < CUP_PULL_HEIGHT_MAX) {
                                Vec3 toCup;
                                Vec3 direction;
                                f32 alignment;
                                Radians angle;
                                f32 pull;

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
                                velocity[AXIS_X] *= CUP_PULL_DRAG;
                                velocity[AXIS_Z] *= CUP_PULL_DRAG;
                                velocity[AXIS_X] += cosf(angle) * pull;
                                velocity[AXIS_Z] += sinf(angle) * pull;
                                stepFlags |= STEP_FLAG_CUP_PULL;
                            }
                        }

                        if ((f32)cupDistance >= CUP_PULL_OUTER_RADIUS &&
                            (f32)IntLength2D(
                                startX - g_HoleSetup.cupPosition[AXIS_X],
                                startZ - g_HoleSetup.cupPosition[AXIS_Z]
                            ) < CUP_PULL_OUTER_RADIUS &&
                            (f32)Abs(startY - g_HoleSetup.cupPosition[AXIS_Y]) <
                                CUP_PULL_HEIGHT_MAX) {
                            Vec3 fromCup;
                            Vec3 direction;
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
                            alignment = (direction[AXIS_X] * fromCup[AXIS_X]) +
                                (direction[AXIS_Z] * fromCup[AXIS_Z]);
                            outwardSpeed = (velocity[AXIS_X] * fromCup[AXIS_X]) +
                                (velocity[AXIS_Z] * fromCup[AXIS_Z]);
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
                                alignment >= LIP_OUT_ALIGNMENT_MIN) {
                                f32 keptFraction = speed * (1.0f / LIP_OUT_FULL_SPEED);

                                if (keptFraction < 0.0f) {
                                    keptFraction = 0.0f;
                                }

                                if (keptFraction > 1.0f) {
                                    keptFraction = 1.0f;
                                }

                                velocity[AXIS_Y] = -speed *
                                    (sqrtf(1.0f - (keptFraction * keptFraction)) +
                                        LIP_OUT_EXTRA_RISE);
                                velocity[AXIS_X] *= keptFraction;
                                velocity[AXIS_Z] *= keptFraction;
                                simulation->state = BALL_STATE_FLYING;
                                simulation->spinHeading = atan2(velocity[AXIS_Z], velocity[AXIS_X]);
                                simulation->spinSpeed =
                                    IntLength2D(velocity[AXIS_X], velocity[AXIS_Z]) *
                                    LIP_OUT_SPIN_SCALE;
                                BuildLiftRotation();
                                contact = 0;
                                stepFlags |= STEP_FLAG_LIPPED_OUT;
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
                if (speed >= CUP_WALL_SOUND_SPEED_MIN) {
                    SoundVolume volume = speed * CUP_SOUND_VOLUME_PER_SPEED;

                    if (volume >= SOUND_FULL_SCALE) {
                        volume = KNOCK_VOLUME_MAX;
                    }

                    if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                        PlaySound(SOUND_HIT_CUP_WALL, g_NextSoundChannel, volume);
                    }
                }
            }

            if (simulation->position[AXIS_Y] <
                    g_HoleSetup.cupPosition[AXIS_Y] + CUP_CEILING_DEPTH &&
                velocity[AXIS_Y] < -CUP_CEILING_SPEED_MIN) {
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

                velocity[AXIS_X] = velocity[AXIS_Z] = 0.0f;
                velocity[AXIS_Y] = (-fallSpeed * CUP_FLOOR_BOUNCE) + CUP_GRAVITY;
                if (velocity[AXIS_Y] > 0.0f) {
                    simulation->isHoledOut = TRUE;
                    simulation->state = BALL_STATE_STOPPED;
                }

                if (simulation->soundStates[BALL_SOUND_CUP_IN] == CUP_IN_SOUND_DUE) {
                    if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                        PlaySound(SOUND_CUP_IN, g_NextSoundChannel, SOUND_FULL_SCALE);
                    }

                    simulation->soundStates[BALL_SOUND_CUP_IN] = CUP_IN_SOUND_PLAYED;
                }
                else if (simulation->soundStates[BALL_SOUND_CUP_IN] == CUP_IN_SOUND_NONE) {
                    if (fallSpeed >= CUP_FLOOR_SOUND_SPEED_MIN) {
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
            Vec3 reflected;
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
                f32 raisedSpeed = 0.0f;
                BallSpeed inwardSpeed;
                f32 slide;
                f32 along;
                f32 edgeX;
                f32 edgeY;
                f32 edgeZ;
                f32 squareX;
                f32 squareY;
                f32 squareZ;
                f32 stepX;
                f32 stepY;
                f32 stepZ;

                normalX = wall.corners[AXIS_X][0];
                normalY = 0.0f;
                edgeY = 0.0f;
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
                squareY = velocity[AXIS_Y] * velocity[AXIS_Y];
                squareZ = velocity[AXIS_Z] * velocity[AXIS_Z];
                along = wallFraction;
                if (along < 0.0f) {
                    along = 0.0f;
                }

                stepX = velocity[AXIS_X] * WALL_HIT_STEP_SHARE;
                stepY = velocity[AXIS_Y] * WALL_HIT_STEP_SHARE;
                stepZ = velocity[AXIS_Z] * WALL_HIT_STEP_SHARE;
                hitPosition[AXIS_X] = startX + (along * stepX);
                hitPosition[AXIS_Y] = startY + (along * stepY);
                hitPosition[AXIS_Z] = startZ + (along * stepZ);
                if (wallFraction < 0.0f && fabsf(inwardSpeed) < WALL_ESCAPE_SPEED) {
                    along = sqrtf(squareX + squareZ);
                    if (inwardSpeed < 0.0f) {
                        inwardSpeed -= WALL_ESCAPE_SPEED;
                    }
                    else {
                        inwardSpeed += WALL_ESCAPE_SPEED;
                    }

                    raisedSpeed = WALL_ESCAPE_SPEED;
                    raisedSpeed += along;
                    bounce = along / raisedSpeed;
                    osSyncPrintf("SpecialWallRefrect\n");
                }

                simulation->position[AXIS_X] = hitPosition[AXIS_X];
                simulation->position[AXIS_Y] = hitPosition[AXIS_Y];
                simulation->position[AXIS_Z] = hitPosition[AXIS_Z];
                if (simulation->purpose == LAUNCH_PURPOSE_STROKE &&
                    fabsf(inwardSpeed) > WALL_SPARK_SPEED_MIN) {
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
            f32 keptFraction;

            simulation->bounceCount++;
            GetTrianglePlane(&triangle, &plane);
            simulation->position[AXIS_X] =
                startX + (fraction * TRIANGLE_HIT_STEP_SHARE * velocity[AXIS_X]);
            simulation->position[AXIS_Y] =
                startY + (fraction * TRIANGLE_HIT_STEP_SHARE * velocity[AXIS_Y]);
            simulation->position[AXIS_Z] =
                startZ + (fraction * TRIANGLE_HIT_STEP_SHARE * velocity[AXIS_Z]);
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

            switch (GetSurfaceTraits(simulation->surface)->kind) {
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

            stepFlags |= STEP_FLAG_HOLE_TRIANGLE_BOUNCE;
            if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
                SpawnImpactEffects(&impact);
                StartCheepCheepLeap(0);
            }

            if (simulation->surface == SURFACE_WATER_HAZARD &&
                (simulation->state == BALL_STATE_ROLLING ||
                    fabsf(impactSpeed) >= SKIP_IMPACT_SPEED_MAX ||
                    fabsf(impactSpeed) <= SKIP_IMPACT_SPEED_MIN ||
                    fabsf(impactSpeed) >=
                        sqrtf(
                            (velocity[AXIS_X] * velocity[AXIS_X]) +
                            (velocity[AXIS_Z] * velocity[AXIS_Z])
                        ) * SKIP_SPEED_RATIO)) {
                simulation->state = BALL_STATE_IN_WATER;
                impactSpeed = 0.0f;
                velocity[AXIS_X] *= WATER_ENTRY_DRAG;
                velocity[AXIS_X] *= WATER_ENTRY_DRAG;
            }

            softness = THOUSANDTHS_PER_ONE - GetSurfaceTraits(simulation->surface)->bounciness;
            keptFraction = ((f32)THOUSANDTHS_PER_ONE -
                               (SOFTNESS_SPEED_LOSS_SCALE * softness * steepness * steepness)) *
                ONE_PER_THOUSANDTH;
            bounce = 1.0f - (softness * steepness * ONE_PER_THOUSANDTH);
            push = (bounce + 1.0f) * impactSpeed;
            bounced[AXIS_X] = velocity[AXIS_X] - (plane.normal[AXIS_X] * push);
            bounced[AXIS_Y] = velocity[AXIS_Y] - (plane.normal[AXIS_Y] * push);
            bounced[AXIS_Z] = velocity[AXIS_Z] - (plane.normal[AXIS_Z] * push);
            if (simulation->state == BALL_STATE_IN_WATER) {
                keptFraction = 0.0f;
            }

            velocity[AXIS_X] = bounced[AXIS_X] * keptFraction;
            velocity[AXIS_Y] = bounced[AXIS_Y] * keptFraction;
            velocity[AXIS_Z] = bounced[AXIS_Z] * keptFraction;
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
            f32 slide = 0.0f;
            SurfaceTraits *surfaceTraits;
            f32 friction = 1.0f;
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
                Vec3 toCup;

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
                    ) > fabsf(velocity[AXIS_Y]) * CUP_PULL_SLIDE_RATIO_MAX) {
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
                f32 sideX = velocity[AXIS_X];
                f32 sideY = 0.0f;
                f32 sideZ = velocity[AXIS_Z];

                if (fabsf(sideX) + fabsf(sideZ) >= KilometersPerHour(5)) {
                    f32 tilt = (GetStepNoise() - NOISE_MIDDLE) * LANDING_TILT_NOISE_SCALE;

                    if (tilt < 0.0f) {
                        tilt -= LANDING_TILT_MIN;
                    }
                    else {
                        tilt += LANDING_TILT_MIN;
                    }

                    guNormalize(&sideX, &sideY, &sideZ);
                    sideX *= tilt;
                    sideZ *= tilt;
                    plane.normal[AXIS_X] -= sideZ;
                    plane.normal[AXIS_Z] += sideX;
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
            if (speed >= SLIDE_SPEED_MIN) {
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

            switch (GetSurfaceTraits(simulation->surface)->kind) {
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

            surfaceTraits = GetSurfaceTraits(simulation->surface);
            friction = (THOUSANDTHS_PER_ONE - surfaceTraits->bounciness) * ONE_PER_THOUSANDTH;
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
            else if (surfaceTraits->kind == SURFACE_KIND_OUT_OF_COURSE ||
                ((surfaceTraits->kind == SURFACE_KIND_OUT_OF_BOUNDS ||
                     surfaceTraits->kind == SURFACE_KIND_WATER_HAZARD) &&
                    impactSpeed < LOST_BALL_STOP_SPEED)) {
                simulation->state = BALL_STATE_STOPPED;
            }

            switch (GetSurfaceTraits(simulation->surface)->kind) {
                case SURFACE_KIND_GREEN:
                case SURFACE_KIND_GREEN_EDGE:
                    friction *=
                        ((fabsf(
                              impactSpeed + (impactSpeed * GetPositionNoise() * LANDING_NOISE_SCALE)
                          ) * GREEN_LANDING_FRICTION_PER_SPEED) +
                            GREEN_LANDING_FRICTION_BASE);
                    if (friction > GREEN_LANDING_FRICTION_MAX) {
                        friction = GREEN_LANDING_FRICTION_MAX;
                    }

                    friction -= GetStepNoise() * LANDING_NOISE_SCALE;
                    if (friction < GREEN_LANDING_FRICTION_MIN) {
                        friction = GREEN_LANDING_FRICTION_MIN;
                    }

                    spin = 1.0f -
                        (sqrtf(
                             (speed * speed) -
                             (impactSpeed * (1.0f - friction) * impactSpeed * (1.0f - friction))
                         ) / (speed * slide));
                    if (spin > GRASS_LANDING_SPIN_MAX) {
                        spin = GRASS_LANDING_SPIN_MAX;
                    }

                    if (spin < GRASS_LANDING_SPIN_MIN) {
                        spin = GRASS_LANDING_SPIN_MIN;
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
                    if (spin > GRASS_LANDING_SPIN_MAX) {
                        spin = GRASS_LANDING_SPIN_MAX;
                    }

                    if (spin < GRASS_LANDING_SPIN_MIN) {
                        spin = GRASS_LANDING_SPIN_MIN;
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
            else if (restitution > RESTITUTION_MAX) {
                restitution = RESTITUTION_MAX;
            }

            if (simulation->state != BALL_STATE_ROLLING &&
                impactSpeed < ROLL_START_SPEED_MAX &&
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
                f32 keptFraction = 1.0f - spin;
                Vec3 normalPart;
                Vec3 tangent;
                s32 spinVelocityX;
                s32 spinVelocityZ;
                s32 driftX;
                s32 driftZ;
                f32 driftLimit;
                f32 drift;

                if (keptFraction <= BOUNCE_SPEED_KEPT_MIN) {
                    keptFraction = BOUNCE_SPEED_KEPT_MIN;
                }

                if (simulation->stepCount < EARLY_BOUNCE_STEP_COUNT && !g_GameState.isMiniGolf) {
                    keptFraction *= EARLY_BOUNCE_SPEED_SCALE;
                }

                normalPart[AXIS_X] = -plane.normal[AXIS_X] * impactSpeed;
                normalPart[AXIS_Y] = -plane.normal[AXIS_Y] * impactSpeed;
                normalPart[AXIS_Z] = -plane.normal[AXIS_Z] * impactSpeed;
                tangent[AXIS_X] = velocity[AXIS_X] + normalPart[AXIS_X];
                tangent[AXIS_Y] = velocity[AXIS_Y] + normalPart[AXIS_Y];
                tangent[AXIS_Z] = velocity[AXIS_Z] + normalPart[AXIS_Z];
                velocity[AXIS_X] =
                    (tangent[AXIS_X] * keptFraction) + (normalPart[AXIS_X] * restitution);
                velocity[AXIS_Y] =
                    (tangent[AXIS_Y] * keptFraction) + (normalPart[AXIS_Y] * restitution);
                velocity[AXIS_Z] =
                    (tangent[AXIS_Z] * keptFraction) + (normalPart[AXIS_Z] * restitution);
                simulation->lastBounceStep = simulation->stepCount;
                spinVelocityX = simulation->spinSpeed * cosf(simulation->spinHeading);
                spinVelocityZ = simulation->spinSpeed * sinf(simulation->spinHeading);
                driftX = (spinVelocityX - velocity[AXIS_X]) * (1.0f - restitution) *
                    fabsf(plane.normal[AXIS_Y]) * GROUND_SPIN_GRIP;
                driftZ = (spinVelocityZ - velocity[AXIS_Z]) * (1.0f - restitution) *
                    fabsf(plane.normal[AXIS_Y]) * GROUND_SPIN_GRIP;
                drift = (s32)IntLength2D(driftX, driftZ);
                driftLimit = impactSpeed * SPIN_DRIFT_SHARE_MAX;
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
            SurfaceTraits *surfaceTraits;
            s32 groundHeight;
            s32 slopeX;
            s32 slopeZ;
            f32 friction;
            BallSpeed speed;
            f32 lie;

            isSteep = FALSE;
            rollSpeed = sqrtf(
                (velocity[AXIS_X] * velocity[AXIS_X]) + (velocity[AXIS_Z] * velocity[AXIS_Z])
            );
            if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
                surfaceTraits = &f_SurfaceTraits[SURFACE_GREEN];
                SetFlatTriangle(&triangle);
                if (g_ClubTestMode == CLUB_TEST_SHOT) {
                    surfaceTraits = &f_SurfaceTraits[SURFACE_FAIRWAY];
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
                        Vec3 along;
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

                if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST && !g_GameState.isMiniGolf) {
                    f32 sideX = velocity[AXIS_X];
                    f32 sideZ = velocity[AXIS_Z];

                    if (fabsf(sideX) + fabsf(sideZ) >= KilometersPerHour(5)) {
                        f32 tilt = (GetStepNoise() - NOISE_MIDDLE) * ROLL_TILT_NOISE_SCALE;

                        sideX *= (1.0f / ROLL_TILT_FULL_SPEED);
                        sideZ *= (1.0f / ROLL_TILT_FULL_SPEED);
                        sideX *= tilt;
                        sideZ *= tilt;
                        plane.normal[AXIS_X] -= sideZ;
                        plane.normal[AXIS_Z] += sideX;
                        guNormalize(
                            &plane.normal[AXIS_X],
                            &plane.normal[AXIS_Y],
                            &plane.normal[AXIS_Z]
                        );
                    }
                }

                simulation->surface =
                    GetSurface(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
                switch (simulation->surface) {
                    case SURFACE_FAIRWAY:
                        if (!g_GameState.isMiniGolf) {
                            lie = GetStepNoise();
                            if (lie < FAIRWAY_POOR_LIE_CHANCE) {
                                simulation->surface = SURFACE_FAIRWAY_POOR_LIE;
                            }
                            else if (lie < FAIRWAY_POOR_LIE_CHANCE + FAIRWAY_FAIR_LIE_CHANCE) {
                                simulation->surface = SURFACE_FAIRWAY_FAIR_LIE;
                            }
                        }

                        break;
                    case SURFACE_ROUGH:
                        lie = GetStepNoise();
                        if (lie < ROUGH_POOR_LIE_CHANCE) {
                            simulation->surface = SURFACE_ROUGH_POOR_LIE;
                        }

                        break;
                }

                surfaceTraits = GetSurfaceTraits(simulation->surface);
                if (surfaceTraits->kind == SURFACE_KIND_OUT_OF_COURSE) {
                    simulation->state = BALL_STATE_STOPPED;
                    goto SETTLE;
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

            slopeX = (-plane.normal[AXIS_X] * SLOPE_ACCELERATION) / fabsf(plane.normal[AXIS_Y]);
            slopeZ = (-plane.normal[AXIS_Z] * SLOPE_ACCELERATION) / fabsf(plane.normal[AXIS_Y]);
            if (fabsf((f32)slopeX) + fabsf((f32)slopeZ) >= SLOPE_ACCELERATION_MAX) {
                f32 scale =
                    SLOPE_ACCELERATION_MAX / sqrtf(((f32)slopeX * slopeX) + ((f32)slopeZ * slopeZ));

                slopeX = slopeX * scale;
                slopeZ = slopeZ * scale;
            }

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

            if (simulation->hasEnteredWater &&
                simulation->rollFrameCount < WATER_EXIT_ROLL_FRAMES) {
                slopeX *= WATER_EXIT_SLOPE_SCALE;
                slopeZ *= WATER_EXIT_SLOPE_SCALE;
                isSteep = TRUE;
            }
            else if (isSteep) {
                slopeX *= STEEP_SLOPE_SCALE;
                slopeZ *= STEEP_SLOPE_SCALE;
                if (friction < STEEP_FRICTION_MIN) {
                    friction = STEEP_FRICTION_MIN;
                }
            }
            else {
                if ((f32)rollSpeed < SLOW_ROLL_SPEED) {
                    f32 scale =
                        (rollSpeed * SLOW_ROLL_SLOPE_SCALE_PER_SPEED) + SLOW_ROLL_SLOPE_SCALE_MIN;

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
            if (speed > SLOPE_STEER_SPEED_MIN) {
                f32 against = -(
                    (((f32)slopeX * velocity[AXIS_X]) + ((f32)slopeZ * velocity[AXIS_Z])) / speed
                );

                slopeX = slopeX + (SLOPE_STEER_SCALE * against * (slopeX / speed));
                slopeZ = slopeZ + (SLOPE_STEER_SCALE * against * (slopeZ / speed));
            }
            else {
                slopeX = slopeX * STILL_BALL_SLOPE_SCALE;
                slopeZ = slopeZ * STILL_BALL_SLOPE_SCALE;
            }

            if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
            }

            velocity[AXIS_X] =
                (slopeX * friction * SLOPE_PULL_SCALE) + (velocity[AXIS_X] * friction);
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

            if (simulation->rollFrameCount >= STOP_ROLL_FRAMES_MIN) {
                s32 speed = IntLength2D(velocity[AXIS_X], velocity[AXIS_Z]);

                if (simulation->rollFrameCount >= WATER_STOP_ROLL_FRAMES &&
                    simulation->hasEnteredWater) {
                    simulation->state = BALL_STATE_STOPPED;
                }
                else if (speed < STOP_SPEED_MAX) {
                    if (!isSteep && IntLength2D(slopeX, slopeZ) * friction < STOP_SLOPE_PULL_MAX) {
                        simulation->state = BALL_STATE_STOPPED;
                    }
                }
            }

            if (!isSteep &&
                surfaceTraits->kind != SURFACE_KIND_GREEN &&
                simulation->lastBounceStep + OFF_GREEN_STOP_STEPS < simulation->stepCount &&
                sqrtf(
                    (velocity[AXIS_X] * velocity[AXIS_X]) + (velocity[AXIS_Y] * velocity[AXIS_Y]) +
                    (velocity[AXIS_Z] * velocity[AXIS_Z])
                ) < OFF_GREEN_STOP_SPEED_MAX) {
                simulation->state = BALL_STATE_STOPPED;
            }
        }

ROLLED:

        if ((simulation->state == BALL_STATE_FLYING || simulation->state == BALL_STATE_ROLLING) &&
            contact == 0) {
            {
                Vec3 relative;
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
                    AIR_DRAG_MIN;
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

                curve = sideSpin * CURVE_PER_SIDE_SPIN * weight;
                if (horizontalSpeed < fabsf(curve) * CURVE_SPEED_RATIO_MAX) {
                    curve = horizontalSpeed * (1.0f / CURVE_SPEED_RATIO_MAX);
                    if (simulation->sideSpin < 0) {
                        curve = -curve;
                    }
                }

                if (horizontalSpeed > CURVE_SPEED_MIN) {
                    f32 scale = (1.0f / horizontalSpeed) * curve;
                    f32 curveX = -relative[AXIS_Z] * scale;
                    f32 curveZ = relative[AXIS_X] * scale;

                    velocity[AXIS_X] += curveX;
                    velocity[AXIS_Z] += curveZ;
                }

                simulation->sideSpin = simulation->sideSpin * SIDE_SPIN_DECAY;
                lift = simulation->spinSpeed * LIFT_PER_SPIN;
                if (simulation->spinSpeed != 0 && airSpeed > LIFT_AIR_SPEED_MIN) {
                    Vec3 turned;
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
                    if (airSpeed < fabsf(lift) * LIFT_SPEED_RATIO_MAX) {
                        lift = airSpeed * (1.0f / LIFT_SPEED_RATIO_MAX);
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
                    height = (height * WIND_SCALE_PER_HEIGHT) + LOW_WIND_SCALE;
                    windX = simulation->windVelocityX * height;
                    windZ = simulation->windVelocityZ * height;
                }

                windX = (windX - velocity[AXIS_X]) * drag;
                windZ = (windZ - velocity[AXIS_Z]) * drag;
                velocity[AXIS_X] += windX;
                velocity[AXIS_Z] += windZ;
                velocity[AXIS_Y] += (-velocity[AXIS_Y] * drag) + GRAVITY;
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
                stepFlags |= STEP_FLAG_OUTSIDE_COURSE;
            }
        }

SETTLE:

        if (simulation->purpose != LAUNCH_PURPOSE_SHOT_PREVIEW) {
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
                                simulation->isOverCup = TRUE;
                            }
                            else if (cupDistance < CUP_RIM_OUTER_RADIUS) {
                                f32 rimDistance = CUP_RIM_OUTER_RADIUS - cupDistance;
                                f32 remainingShare = (CUP_RIM_CURVE_RADIUS * CUP_RIM_CURVE_RADIUS) -
                                    (rimDistance * rimDistance);

                                if (remainingShare > 0.0f) {
                                    simulation->isOverCup = TRUE;
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
                    FindSurface(simulation->position[AXIS_X], simulation->position[AXIS_Z], TRUE);
                surfaceKind = GetSurfaceTraits(simulation->surfaceBelow)->kind;
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

    simulation->velocity[AXIS_X] = velocity[AXIS_X];
    simulation->velocity[AXIS_Y] = velocity[AXIS_Y];
    simulation->velocity[AXIS_Z] = velocity[AXIS_Z];
    if (simulation->purpose == LAUNCH_PURPOSE_STROKE) {
        if (simulation->state == BALL_STATE_ROLLING && !simulation->soundStates[BALL_SOUND_ROLL]) {
            PlaySound(SOUND_ROLL, ROLL_SOUND_CHANNEL, SOUND_FULL_SCALE);
            simulation->soundStates[BALL_SOUND_ROLL] = TRUE;
        }

        if (simulation->soundStates[BALL_SOUND_ROLL]) {
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
                    (ROLL_VOLUME_PER_SPEED * (rollFrames + 1));
                if (volume > ROLL_VOLUME_MAX) {
                    volume = ROLL_VOLUME_MAX;
                }

                if (volume < 0) {
                    volume = 0;
                }

                SetSoundVolume(ROLL_SOUND_CHANNEL, volume);
            }
        }
    }

    return stepFlags;
}

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

    if (row > LIE_RATING_MAX) {
        row = LIE_RATING_MAX;
    }

    if (impactTiming < -IMPACT_TIMING_ENTRY_LIMIT) {
        return &f_EarlyImpactEffect;
    }

    if (impactTiming <= IMPACT_TIMING_ENTRY_LIMIT) {
        return &f_ImpactEffects[row][impactTiming + IMPACT_TIMING_ENTRY_LIMIT];
    }

    return &f_LateImpactEffect;
}

s32 LaunchBallSimulation(BallSimulation *simulation, LaunchPurpose purpose, ShotLaunch *shot) {
    s32 percentRange[2];
    Club *club;
    ImpactEffect *impactEffect;
    s32 impactTiming;
    s32 groundHeight;
    s32 speed;
    s32 sideSign;
    s32 slopeSpin;
    s32 rumbles;
    s16 crossSlope;
    s32 crossSlopeSize;
    WideBinaryAngle yaw;
    Radians pitch;
    Radians yawRadians;
    f32 launchScale;
    f32 blend;
    Radians slopePitch;
    f32 curve;
    f32 spinBase;
    Radians windAngle;
    f32 windScale;
    f32 along;
    s32 index;
    Surface8 surface;

    f32 ChooseLieBlend(void) {
        return (u32)((simulation->position[AXIS_X] + simulation->position[AXIS_Z] +
                         g_Players[simulation->playerIndex].noiseSeed) &
                   NOISE_BYTE_MASK) /
            NOISE_BYTE_PAIR_SUM_MAX +
            (u32)((simulation->position[AXIS_X] - simulation->position[AXIS_Z] +
                      g_Players[simulation->playerIndex].noiseSeed) &
                NOISE_BYTE_MASK) /
            NOISE_BYTE_PAIR_SUM_MAX;
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
    simulation->hasEnteredWater = FALSE;
    simulation->stepFlags = 0;
    simulation->isOverCup = FALSE;

    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        simulation->playerIndex = shot->playerIndex;
    }
    else {
        simulation->playerIndex = 0;
    }

    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        s16 *clubs = g_Players[shot->playerIndex].clubs;

        club = GetClub(clubs[shot->clubSlot]);
    }
    else {
        club = GetClub(shot->clubSlot);
    }

    if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
        impactEffect = &f_ImpactEffects[0][IMPACT_TIMING_ENTRY_LIMIT];
        rumbles = f_ImpactEffects[0][IMPACT_TIMING_ENTRY_LIMIT].rumbles;
    }
    else {
        s32 row = GetLieRating(club, simulation->surface, shot->shotMode) - 1;

        if (row < 0) {
            row = 0;
        }

        if (row > LIE_RATING_MAX) {
            row = LIE_RATING_MAX;
        }

        if (impactTiming < -IMPACT_TIMING_ENTRY_LIMIT) {
            impactEffect = &f_EarlyImpactEffect;
        }
        else if (impactTiming > IMPACT_TIMING_ENTRY_LIMIT) {
            impactEffect = &f_LateImpactEffect;
        }
        else {
            impactEffect = &f_ImpactEffects[row][impactTiming + IMPACT_TIMING_ENTRY_LIMIT];
        }

        rumbles = impactEffect->rumbles;
    }

    if ((club->surfaceMask & CLUB_SURFACE_GREEN) &&
        simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        speed = GetPuttPower(shot->power, shot->shotMode);
    }
    else {
        speed = GetClubPower(club, shot->shotMode);
        speed = speed *
            (shot->power *
                ((impactEffect->speedChange * (LAUNCH_SCALE_ONE / SPEED_CHANGE_UNITS)) +
                    LAUNCH_SCALE_ONE) /
                LAUNCH_SCALE_ONE) /
            LAUNCH_POWER_MAX;
        if (speed <= 0) {
            speed = LAUNCH_SPEED_MIN;
        }
    }

    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        SurfaceTraits *surfaceTraits = GetSurfaceTraits(simulation->launchSurface);

        blend = ChooseLieBlend();
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

    if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
        sinf(pitch);
        along = pitch - (club->offsetPitch * DEGREES_TO_RADIANS);
        pitch +=
            (f32)club->offsetPitch * shot->offsetY * (DEGREES_TO_RADIANS / OFFSET_Y_PER_PITCH_STEP);
        sinf(pitch);
        launchScale =
            (cosf(along) - LAUNCH_SCALE_COSINE_BASE) * shot->offsetY * LAUNCH_SCALE_PER_OFFSET;
        if (club->category == CLUB_CATEGORY_WOOD) {
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
                ((club->deflection[shot->shotMode] * shot->power / LAUNCH_POWER_MAX) +
                    (impactEffect->yawChange * club->mishitScale)));
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
        GetSurfaceTraits(simulation->launchSurface);
        simulation->sideSpin =
            (((sideSign * impactEffect->sideSpinChange * club->mishitScale) /
                 MISHIT_SIDE_SPIN_DIVISOR) +
                (shot->offsetX * (club->mishitScale * OFFSET_SIDE_SPIN_PER_MISHIT)) + slopeSpin +
                (sideSign * CLUB_SIDE_SPIN_SCALE * club->sideSpin)) *
            (speed * launchScale * launchScale) * SIDE_SPIN_PER_SPEED;
        if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
            simulation->spinSpeed =
                ((((club->spin * STROKE_SPIN_SCALE) + STROKE_SPIN_OFFSET) * STROKE_SPIN_PER_OFFSET *
                     (impactEffect->spinChange + shot->offsetY)) -
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
        speed = LAUNCH_SPEED_MIN;
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
    simulation->isHoledOut = FALSE;
    simulation->hasHitFlag = FALSE;
    simulation->rollFrameCount = 0;
    for (index = 0; index < BALL_SOUND_COUNT; index++) {
        simulation->soundStates[index] = 0;
    }

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

        if (simulation->purpose == LAUNCH_PURPOSE_STROKE &&
            IsGameFlagSet(GAME_FLAG_SHOW_LAUNCH_VALUES)) {
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
