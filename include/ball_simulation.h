#ifndef BALL_SIMULATION_H
#define BALL_SIMULATION_H

#include <ultra64.h>

/* A simulation still going after this many steps is given up on */
#define MAX_STEP_COUNT 1801

/* Bits of the flags func_8003327C returns for each step */
#define STEP_FLAG_UNK1 0x1
#define STEP_FLAG_UNK10 0x10
#define STEP_FLAG_UNK40 0x40

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

typedef struct BallSimulation {
    /* 0x00 */ s32 startPosition[3];
    /* 0x0C */ s32 unkC[3];
    /* 0x18 */ s32 position[3];
    /* 0x24 */ u8 unk24[0x8C - 0x24];
    /* 0x8C */ s32 isRunning;
    /* 0x90 */ u8 unk90[0xA0 - 0x90];
    /* 0xA0 */ s8 unkA0;
    /* 0xA1 */ u8 unkA1[2];
    /* 0xA3 */ s8 surface;
    /* 0xA4 */ u8 unkA4[3];
    /* 0xA7 */ s8 unkA7;
    /* 0xA8 */ u8 unkA8[0xB0 - 0xA8];
    /* 0xB0 */ s8 unkB0;
    /* 0xB1 */ u8 unkB1[0xB8 - 0xB1];
} BallSimulation; /* size = 0xB8 */

s32 func_8003327C(BallSimulation *simulation);
void func_80037F94(BallSimulation *simulation, s32, Unk20 *shot);

#endif
