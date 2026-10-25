#ifndef BALL_SIMULATION_H
#define BALL_SIMULATION_H

#include <ultra64.h>
#include "club.h"

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

/* Values of a BallSimulation's isRunning */
#define BALL_STATE_STOPPED 0
#define BALL_STATE_FLYING 1
#define BALL_STATE_ROLLING 2
#define BALL_STATE_IN_CUP 3
#define BALL_STATE_IN_WATER 4

#define BALL_SIMULATION_COUNT 1

/* Unk20 unkC values from -UNKC_ENTRY_LIMIT to UNKC_ENTRY_LIMIT have an Unk6 per lie rating */
#define UNKC_ENTRY_LIMIT 5
#define UNKC_ENTRY_COUNT (UNKC_ENTRY_LIMIT * 2 + 1)

typedef struct Unk6 {
    /* 0x0 */ s8 unk0;
    /* 0x1 */ s8 unk1;
    /* 0x2 */ s8 unk2;
    /* 0x3 */ s8 unk3;
    /* 0x4 */ s8 unk4;
    /* 0x5 */ s8 unk5;
} Unk6; /* size = 0x6 */

typedef struct Unk20 {
    /* 0x00 */ s16 unk0;
    /* 0x04 */ s32 clubIndex;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 shotMode;
} Unk20; /* size = 0x20 */

#define UNKA8_COUNT 8

/* What LaunchBallSimulation is called for */
#define LAUNCH_PURPOSE_STROKE 0
#define LAUNCH_PURPOSE_CLUB_TEST 1
#define LAUNCH_PURPOSE_TRIAL 2
#define LAUNCH_PURPOSE_UNK5 5

typedef struct BallSimulation {
    /* 0x00 */ s32 startPosition[3];
    /* 0x0C */ s32 unkC[3];
    /* 0x18 */ s32 position[3];
    /* 0x24 */ s32 velocity[3];
    /* 0x30 */ s32 unk30;
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 unk38;
    /* 0x3C */ f32 unk3C;
    /* 0x40 */ f32 unk40;
    /* 0x44 */ u8 unk44[0x84 - 0x44];
    /* 0x84 */ f32 windVelocityX;
    /* 0x88 */ f32 windVelocityZ;
    /* 0x8C */ s32 isRunning;
    /* 0x90 */ s32 purpose;
    /* 0x94 */ s32 unk94;
    /* 0x98 */ s32 unk98;
    /* 0x9C */ s32 unk9C;
    /* 0xA0 */ s8 unkA0;
    /* 0xA1 */ s8 unkA1;
    /* 0xA2 */ s8 unkA2;
    /* 0xA3 */ s8 surface;
    /* 0xA4 */ u8 unkA4;
    /* 0xA5 */ s8 playerIndex;
    /* 0xA6 */ s8 unkA6;
    /* 0xA7 */ s8 unkA7;
    /* 0xA8 */ s8 unkA8[UNKA8_COUNT];
    /* 0xB0 */ s8 unkB0;
    /* 0xB1 */ u8 unkB1;
    /* 0xB2 */ s16 unkB2;
    /* 0xB4 */ s8 unkB4;
    /* 0xB5 */ u8 unkB5[0xB8 - 0xB5];
} BallSimulation; /* size = 0xB8 */

extern BallSimulation g_BallSimulations[BALL_SIMULATION_COUNT];

void ResetBallSimulations(void);
Unk6 *GetUnkCEntry(Club *club, s32 surface, s32 shotMode, s32 unkC);
s32 StepBallSimulation(BallSimulation *simulation);
s32 LaunchBallSimulation(BallSimulation *simulation, s32 purpose, Unk20 *shot);

#endif
