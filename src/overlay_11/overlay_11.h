#ifndef OVERLAY_11_H
#define OVERLAY_11_H

#include "common.h"

typedef struct Overlay11State {
    /* 0x00 */ u8 unk_00[0x38];
    /* 0x38 */ u32 unk38;
    /* 0x3C */ u8 unk_3C[0x60];
    /* 0x9C */ u32 unk9C[4];
    /* 0xAC */ u32 unkAC[4];
    /* 0xBC */ u8 unk_BC[0xC];
    /* 0xC8 */ s32 unkC8;
    /* 0xCC */ s32 unkCC;
    /* 0xD0 */ u8 unk_D0[0x4];
    /* 0xD4 */ s32 unkD4;
    /* 0xD8 */ u8 unk_D8[0x14];
} Overlay11State; /* 0xEC */

extern Overlay11State D_80104FD0;

#endif
