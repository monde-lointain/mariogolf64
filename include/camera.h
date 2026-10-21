#ifndef CAMERA_H
#define CAMERA_H

#include <ultra64.h>

/* The camera mode that frames the shot from behind the golfer */
#define CAMERA_MODE_BEHIND_GOLFER 1

typedef struct Camera {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ f32 eye[3];
    /* 0x10 */ f32 lookAt[3];
    /* 0x1C */ u8 unk1C[0x4C - 0x1C];
    /* 0x4C */ s32 mode;
} Camera; /* size = 0x50 */

extern Camera g_Cameras[];

#endif
