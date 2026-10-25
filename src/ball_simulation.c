#include "common.h"
#include "ball_simulation.h"
#include "club.h"
#include "libc.h"
#include "surface.h"

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
        g_BallSimulations[index].unk24[AXIS_X] = rand() & 0x3FF;
        g_BallSimulations[index].unk24[AXIS_Y] = -100;
        g_BallSimulations[index].unk24[AXIS_Z] = rand() & 0x3FF;
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

INCLUDE_ASM("asm/nonmatchings/ball_simulation", func_80037E50);

INCLUDE_ASM("asm/nonmatchings/ball_simulation", func_80037EE0);

INCLUDE_ASM("asm/nonmatchings/ball_simulation", func_80037F94);
