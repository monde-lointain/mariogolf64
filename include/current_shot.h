#ifndef CURRENT_SHOT_H
#define CURRENT_SHOT_H

#include <ultra64.h>

/* The state of the shot being set up by the player whose turn it is */
typedef struct CurrentShot {
    /* 0x00 */ u16 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 shotMode;
    /* 0x10 */ s32 clubSlot; /* Index into the player's clubs */
    /* 0x14 */ s32 unk14;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 ballPosition[3]; /* In units of 1/1024 */
    /* 0x28 */ s32 surface;
    /* 0x2C */ s32 playerIndex;
    /* 0x30 */ s32 controllerIndex;
    /* 0x34 */ f32 unk34[2];
    /* 0x3C */ f32 unk3C;
    /* 0x40 */ s32 unk40;
    /* 0x44 */ f32 unk44;
    /* 0x48 */ s32 unk48;
    /* 0x4C */ f32 unk4C;
    /* 0x50 */ s32 unk50;
    /* 0x54 */ s8 unk54;
} CurrentShot; /* size = 0x58 */

extern CurrentShot g_CurrentShot;

#endif
