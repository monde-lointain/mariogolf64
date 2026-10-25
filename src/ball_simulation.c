#include "common.h"
#include "ball_simulation.h"
#include "billboard.h"
#include "club.h"
#include "debug_text.h"
#include "functions.h"
#include "game_flags.h"
#include "libc.h"
#include "math_util.h"
#include "save.h"
#include "surface.h"
#include "variables.h"

BallSimulation g_BallSimulations[BALL_SIMULATION_COUNT] = { 0 };

s8 g_ClubTestMode = CLUB_TEST_NONE;

SurfaceInfo g_SurfaceInfo[SURFACE_COUNT] = {
    /* SURFACE_OUT_OF_BOUNDS */
    { SURFACE_KIND_OUT_OF_BOUNDS, "O.B.", 100, 100, 0, 0, 0, 0x500, 1, 7 },
    /* SURFACE_FAIRWAY */
    { SURFACE_KIND_FAIRWAY, "FAIRWAY", 98, 100, 5, 960, 500, 0x501, 0, 0 },
    /* SURFACE_ROUGH */
    { SURFACE_KIND_ROUGH, "ROUGH", 80, 90, 30, 600, 200, 0x508, 1, 1 },
    /* SURFACE_BUNKER */
    { SURFACE_KIND_BUNKER, "BUNKER", 70, 85, 30, 600, 0, 0x504, 0, 2 },
    /* SURFACE_GREEN */
    { SURFACE_KIND_GREEN, "ON GREEN", 100, 100, 0, 972, 600, 0x50A, 0, 6 },
    /* SURFACE_WATER_HAZARD */
    { SURFACE_KIND_WATER_HAZARD, "WATER HAZARD", 100, 100, 0, 900, 400, 0x50C, 0, 3 },
    /* SURFACE_CART_WAY */
    { SURFACE_KIND_CART_WAY, "CART WAY", 80, 100, 10, 975, 890, 0x50B, 0, 4 },
    /* SURFACE_TEE_GROUND */
    { SURFACE_KIND_TEE_GROUND, "TEE GROUND", 98, 100, 10, 930, 500, 0x501, 0, 8 },
    /* SURFACE_ROCK */
    { SURFACE_KIND_ROCK, "ROCK", 80, 90, 30, 930, 800, 0x50D, 0, 9 },
    /* SURFACE_GREEN_EDGE */
    { SURFACE_KIND_GREEN_EDGE, "GREEN EDGE", 98, 100, 10, 966, 550, 0x501, 0, 0 },
    /* SURFACE_SEMI_ROUGH */
    { SURFACE_KIND_SEMI_ROUGH, "SEMI ROUGH", 90, 98, 30, 930, 500, 0x507, 0, 0 },
    /* SURFACE_BARE_GROUND */
    { SURFACE_KIND_BARE_GROUND, "BARE GROUND", 80, 90, 30, 930, 500, 0x50C, 0, 1 },
    /* SURFACE_DEEP_ROUGH */
    { SURFACE_KIND_DEEP_ROUGH, "DEEP ROUGH", 50, 80, 30, 430, 100, 0x509, 2, 5 },
    /* SURFACE_BARE_GROUND_UNK13 */
    { SURFACE_KIND_BARE_GROUND_UNK20, "BARE GROUND", 80, 90, 30, 930, 500, 0x50C, 0, 1 },
    /* SURFACE_WASTE_AREA */
    { SURFACE_KIND_WASTE_AREA, "WASTE AREA", 50, 80, 30, 430, 100, 0x8F1, 1, 5 },
    /* SURFACE_UNK15 */
    { SURFACE_KIND_ROCK, "            ", 98, 100, 40, 930, 500, 0x500, 0, 9 },
    /* SURFACE_BUNKER_FAIR_LIE */
    { SURFACE_KIND_BUNKER_FAIR_LIE, "BUNKER", 50, 70, 30, 600, 0, 0x505, 1, 2 },
    /* SURFACE_BUNKER_POOR_LIE */
    { SURFACE_KIND_BUNKER_POOR_LIE, "BUNKER", 40, 60, 30, 600, 0, 0x506, 1, 2 },
    /* SURFACE_BUNKER_UNK18 */
    { SURFACE_KIND_BUNKER_UNK9, "BUNKER", 40, 60, 40, 600, 0, 0x506, 1, 2 },
    /* SURFACE_FAIRWAY_FAIR_LIE */
    { SURFACE_KIND_FAIRWAY_FAIR_LIE, "FAIRWAY", 94, 98, 7, 950, 380, 0x502, 0, 0 },
    /* SURFACE_FAIRWAY_POOR_LIE */
    { SURFACE_KIND_FAIRWAY_POOR_LIE, "FAIRWAY", 90, 94, 7, 950, 380, 0x503, 0, 0 },
    /* SURFACE_ROUGH_POOR_LIE */
    { SURFACE_KIND_ROUGH_POOR_LIE, "ROUGH", 70, 85, 30, 600, 200, 0x90C, 1, 5 },
    /* SURFACE_OUT_OF_COURSE */
    { SURFACE_KIND_OUT_OF_COURSE, "OUT OF COURSE", 100, 100, 0, 0, 0, 0x500, 0, 7 },
    /* SURFACE_TEE_UP */
    { SURFACE_KIND_TEE_UP, "TEE UP", 98, 100, 10, 930, 500, 0x500, 0, 8 },
    /* SURFACE_MINI_GOLF_GREEN */
    { SURFACE_KIND_GREEN, "GREEN", 100, 100, 0, 972, 500, 0x8F2, 0, 6 },
    /* SURFACE_MINI_GOLF_FAIRWAY */
    { SURFACE_KIND_FAIRWAY, "BLUE", 98, 100, 5, 960, 400, 0x8F3, 0, 0 },
    /* SURFACE_MINI_GOLF_CART_WAY */
    { SURFACE_KIND_CART_WAY, "RED", 80, 100, 10, 980, 800, 0x8F4, 0, 4 },
};

/* For unkC values below and above the entries of D_800B7C10 */
Unk6 D_800B7C00 = { -90, -64, -40, -10, 100, 1 };
Unk6 D_800B7C08 = { -80, 64, 30, 10, -5, 1 };
Unk6 D_800B7C10[MAX_LIE_RATING + 1][UNKC_ENTRY_COUNT] = {
    {
        { -90, -64, -40, -10, 100, 1 },
        { -90, -64, -40, -10, 100, 1 },
        { -90, -64, -40, -10, 100, 1 },
        { -90, -64, -40, -10, 100, 1 },
        { -90, -64, -40, -10, 100, 1 },
        { 0, 0, 0, 0, 0, 0 },
        { -80, 64, 30, 10, -5, 1 },
        { -80, 64, 30, 10, -5, 1 },
        { -80, 64, 30, 10, -5, 1 },
        { -80, 64, 30, 10, -5, 1 },
        { -80, 64, 30, 10, -5, 1 },
    },
    {
        { -90, -64, -40, -10, 100, 1 },
        { -90, -64, -40, -10, 100, 1 },
        { -90, -64, -40, -10, 100, 1 },
        { -90, -64, -40, -10, 100, 1 },
        { -8, -2, 2, -7, 15, 0 },
        { 0, 0, 0, 0, 0, 0 },
        { -12, 2, -2, 7, 4, 0 },
        { -80, 64, 30, 10, -5, 1 },
        { -80, 64, 30, 10, -5, 1 },
        { -80, 64, 30, 10, -5, 1 },
        { -80, 64, 30, 10, -5, 1 },
    },
    {
        { -90, -64, -40, -10, 100, 1 },
        { -90, -64, -40, -10, 100, 1 },
        { -90, -64, -40, -10, 100, 1 },
        { -8, -3, 2, -10, 15, 0 },
        { -4, -2, 1, -5, 5, 0 },
        { 0, 0, 0, 0, 0, 0 },
        { -8, 2, -1, 5, 4, 0 },
        { -12, 3, -2, 10, 4, 0 },
        { -80, 64, 30, 10, -5, 1 },
        { -80, 64, 30, 10, -5, 1 },
        { -80, 64, 30, 10, -5, 1 },
    },
    {
        { -90, -64, -40, -10, 100, 1 },
        { -90, -64, -40, -10, 100, 1 },
        { -8, -3, 2, -10, 15, 0 },
        { -4, -2, 1, -5, 5, 0 },
        { -2, -1, 0, -3, 3, 0 },
        { 0, 0, 0, 0, 0, 0 },
        { -4, 1, 0, 3, 2, 0 },
        { -8, 2, -1, 5, 4, 0 },
        { -12, 3, -2, 10, 4, 0 },
        { -80, 64, 30, 10, -5, 1 },
        { -80, 64, 30, 10, -5, 1 },
    },
    {
        { -90, -64, -40, -10, 100, 1 },
        { -8, -3, 2, -10, 15, 0 },
        { -4, -2, 1, -5, 5, 0 },
        { -2, -1, 0, -3, 3, 0 },
        { -1, 0, 0, -2, 1, 0 },
        { 0, 0, 0, 0, 0, 0 },
        { -2, 0, 0, 2, 1, 0 },
        { -4, 1, 0, 3, 2, 0 },
        { -8, 2, -1, 5, 4, 0 },
        { -12, 3, -2, 10, 4, 0 },
        { -80, 64, 30, 10, -5, 1 },
    },
    {
        { -8, -3, 2, -10, 15, 0 },
        { -4, -2, 1, -5, 5, 0 },
        { -2, -1, 0, -3, 3, 0 },
        { -1, 0, 0, -2, 1, 0 },
        { 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0 },
        { -2, 0, 0, 2, 1, 0 },
        { -4, 1, 0, 3, 2, 0 },
        { -8, 2, -1, 5, 4, 0 },
        { -12, 3, -2, 10, 4, 0 },
    },
};

s32 D_800B7D9C = 0;

SurfaceInfo *GetSurfaceInfo(s32 surface) {
    if ((u32)surface >= SURFACE_COUNT) {
        surface = SURFACE_OUT_OF_BOUNDS;
    }
    return &g_SurfaceInfo[surface];
}

void ResetBallSimulations(void) {
    s32 index;

    for (index = 0; index < BALL_SIMULATION_COUNT; index++) {
        g_BallSimulations[index].position[AXIS_X] = 0x800;
        g_BallSimulations[index].position[AXIS_Y] = -1000;
        rand(); /* The original has it */
        g_BallSimulations[index].position[AXIS_Z] = 0x1000;
        g_BallSimulations[index].velocity[AXIS_X] = rand() & 0x3FF;
        g_BallSimulations[index].velocity[AXIS_Y] = -100;
        g_BallSimulations[index].velocity[AXIS_Z] = rand() & 0x3FF;
        g_BallSimulations[index].unk30 = 0;
        g_BallSimulations[index].isRunning = TRUE;
    }
}

INCLUDE_ASM("asm/nonmatchings/ball_simulation", func_80032F70);

INCLUDE_ASM("asm/nonmatchings/ball_simulation", func_80032FE8);

INCLUDE_ASM("asm/nonmatchings/ball_simulation", func_800330D0);

INCLUDE_ASM("asm/nonmatchings/ball_simulation", func_80033134);

INCLUDE_ASM("asm/nonmatchings/ball_simulation", func_80033228);

INCLUDE_ASM("asm/nonmatchings/ball_simulation", func_8003327C);

/* Returns the Unk6 entry for the club's lie rating and unkC */
Unk6 *GetUnkCEntry(Club *club, s32 surface, s32 shotMode, s32 unkC) {
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

void SpawnShotDebris(CoursePoint point, f32 pitch, f32 yaw, s32 launchSpeed);
void StartPlayerRumble(UnkB8 *player);
s16 GetCrossSlopeAngle(UnkB8 *player, Unk20 *shot);
/* It returns s16; declaring it s32 here is there to match */
s32 GetAimSlopeAngle(UnkB8 *player, Unk20 *shot);
void GetSurfacePercentRange(SurfaceInfo *info, Club *club, s32 *range);
s32 GetPuttPower(s32 fraction, s32 shotMode);
s32 GetClubPower(Club *club, s32 shotMode);

/*
 * Sets the simulation's start, velocity and wind from the shot.
 * Only a stroke spawns debris, starts rumble and prints the launch values.
 */
/* Added to a putt's y velocity */
#define PUTT_VELOCITY_Y_OFFSET 0x106

s32 LaunchBallSimulation(BallSimulation *simulation, s32 purpose, Unk20 *shot) {
    s32 percentRange[2];
    Club *club;
    Unk6 *entry;
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
        return (u32)((simulation->position[AXIS_X] + simulation->position[AXIS_Z]
                      + g_Players[simulation->playerIndex].unk88) & 0xFF) / 510.0f
             + (u32)((simulation->position[AXIS_X] - simulation->position[AXIS_Z]
                      + g_Players[simulation->playerIndex].unk88) & 0xFF) / 510.0f;
    }

    launchScale = 1.0f;
    unkC = shot->unkC;
    simulation->purpose = purpose;
    sideSign = 1;
    D_800B7D9C++;
    slopeSpin = 0;
    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        groundHeight = GetGroundHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z]) - 1.536f;
        if (groundHeight < simulation->position[AXIS_Y]) {
            simulation->position[AXIS_Y] = groundHeight;
        }
    }
    surface = simulation->surface;
    simulation->startPosition[AXIS_X] = simulation->unkC[AXIS_X] = simulation->position[AXIS_X];
    simulation->startPosition[AXIS_Y] = simulation->unkC[AXIS_Y] = simulation->position[AXIS_Y];
    simulation->startPosition[AXIS_Z] = simulation->unkC[AXIS_Z] = simulation->position[AXIS_Z];
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
        s16 *clubs = g_Players[shot->unk18].clubs;

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
    if ((club->surfaceMask & CLUB_SURFACE_GREEN) && simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        speed = GetPuttPower(shot->unk8, shot->shotMode);
    } else {
        speed = GetClubPower(club, shot->shotMode);
        speed = speed * (shot->unk8 * (entry->unk0 * 2 + 0x100) / 256) / 256;
        if (speed <= 0) {
            speed = 10;
        }
    }
    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        SurfaceInfo *info = GetSurfaceInfo(simulation->unkA2);

        blend = PickLieBlend();
        GetSurfacePercentRange(info, club, percentRange);
        speed = (percentRange[0] * blend + percentRange[1] * (1.0f - blend)) * speed * 0.01f;
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
        along = pitch - club->unk10 * 0.017453294f;
        pitch += (f32)club->unk10 * shot->unk14 * 0.00054541545f;
        sinf(pitch); /* The original has it */
        launchScale = (cosf(along) - 0.86f) * shot->unk14 * 0.008300781f;
        if (club->kind == CLUB_KIND_WOOD) {
            launchScale *= 2.0f;
        }
        launchScale += 1.0f;
    }
    if (simulation->purpose != LAUNCH_PURPOSE_CLUB_TEST) {
        if ((club->surfaceMask & CLUB_SURFACE_GREEN) && !g_IsMiniGolf) {
            pitch += (u16)GetAimSlopeAngle(&g_Players[shot->unk18], shot) * BINARY_ANGLE_TO_RADIANS;
        } else {
            slopePitch = (u16)(GetAimSlopeAngle(&g_Players[shot->unk18], shot) / 4) * BINARY_ANGLE_TO_RADIANS;
            pitch += slopePitch;
            speed = speed * (1.0f - sinf(slopePitch));
        }
    }
    if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST || (club->surfaceMask & CLUB_SURFACE_GREEN)) {
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
        yaw = shot->unk0 + crossSlope / 2
            + sideSign * (club->deflection[shot->shotMode] * shot->unk8 / 256 + entry->unk1 * club->unk0);
        yawRadians = (u16)yaw * BINARY_ANGLE_TO_RADIANS;
    }
    if (simulation->purpose == LAUNCH_PURPOSE_CLUB_TEST) {
        simulation->unk38 = (sideSign * (club->unk26 * 2) * speed) * 0.00078125f;
        simulation->unk40 = 0.0f;
        spinBase = club->unk12 * 0.5f;
        simulation->unk34 = ((spinBase + 25.0f) * 0.006f * shot->unk14 - spinBase) * (speed / launchScale) * 0.0078125f;
        simulation->unk3C = yawRadians;
        if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
            speed = speed * launchScale;
        }
        if (speed < 0) {
            speed = 0;
        }
    } else {
        GetSurfaceInfo(simulation->unkA2); /* The original has it */
        simulation->unk38 = ((sideSign * entry->unk3 * club->unk0) / 64 + shot->unk10 * (club->unk0 * 0.0078125f)
                             + slopeSpin + sideSign * 2 * club->unk26)
                          * (speed * launchScale * launchScale) * 0.00078125f;
        if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
            simulation->unk34 =
                ((club->unk12 * 0.8f + 10.0f) * 0.004f * (entry->unk4 + shot->unk14) - club->unk12 * 0.5f) * speed
                * 0.0078125f;
            speed = speed * launchScale;
            if (g_HoleConditions.isRaining == TRUE) {
                speed = speed * 0.95f;
            }
        } else {
            simulation->unk34 = speed;
        }
        if (speed < 0) {
            speed = 0;
        }
        if (g_HoleConditions.isRaining == TRUE) {
            simulation->unk34 = simulation->unk34 * 0.6f;
            simulation->unk38 = simulation->unk38 * 0.95f;
        }
        simulation->unk40 = IntLength2D(simulation->unk34, simulation->unk38) * 0.4f;
        simulation->unk3C = yawRadians;
    }
    if (speed <= 0) {
        speed = 10;
    }
    simulation->velocity[AXIS_Y] = sinf(pitch) * speed;
    simulation->velocity[AXIS_X] = cosf(yawRadians) * speed;
    simulation->velocity[AXIS_Z] = sinf(yawRadians) * speed;
    if (club->surfaceMask & CLUB_SURFACE_GREEN) {
        simulation->isRunning = 2;
        simulation->velocity[AXIS_Y] += PUTT_VELOCITY_Y_OFFSET;
    } else {
        simulation->isRunning = 1;
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
        windAngle = g_HoleConditions.windDirection * BINARY_ANGLE_TO_RADIANS;
        simulation->windVelocityX = cosf(windAngle) * g_HoleConditions.windSpeed * windScale;
        simulation->windVelocityZ = sinf(windAngle) * g_HoleConditions.windSpeed * windScale;
        if (g_HoleConditions.isRaining != TRUE) {
            along =
                (simulation->windVelocityX * cosf(yawRadians) + simulation->windVelocityZ * sinf(yawRadians)) * 0.3f;
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
        if (simulation->purpose == LAUNCH_PURPOSE_STROKE && IsGameFlagSet(GAME_FLAG_SHOW_LAUNCH_VALUES)) {
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
            sprintf(text, "SpinAngle = %3.3d/360", (s32)(simulation->unk3C * 57.295776f));
            PrintDebugText(text, 3, 12);
            sprintf(text, "SpinVelX = %fkm/h      ", simulation->unk34 * 0.0070306873f);
            PrintDebugText(text, 3, 13);
            sprintf(text, "SpinVelY = %fkm/h      ", simulation->unk38 * 0.0070306873f);
            PrintDebugText(text, 3, 14);
        }
    }
    return 0;
}
