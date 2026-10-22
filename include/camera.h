#ifndef CAMERA_H
#define CAMERA_H

#include <ultra64.h>

/* The camera mode that frames the shot from behind the golfer */
#define CAMERA_MODE_BEHIND_GOLFER 1

/* Camera drawFlags bits */
#define CAMERA_DRAW_SCENE (1 << 0)
#define CAMERA_DRAW_SCREEN_EFFECT (1 << 1)

typedef struct Camera {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ f32 eye[3];
    /* 0x10 */ f32 lookAt[3];
    /* 0x1C */ s32 unreadValue;
    /* 0x20 */ s32 scissorX;
    /* 0x24 */ s32 scissorY;
    /* 0x28 */ s32 scissorWidth;
    /* 0x2C */ s32 scissorHeight;
    /* 0x30 */ s32 unreadSettings[4]; /* Set by StartGame, never read */
    /* 0x40 */ f32 zoom;
    /* 0x44 */ s32 drawFlags; /* CAMERA_DRAW_* bits */
    /* 0x48 */ f32 fieldOfView; /* In degrees */
    /* 0x4C */ s32 mode;
} Camera; /* size = 0x50 */

extern Camera g_Cameras[];

/* The camera of the inset view, a window over the main view */
#define INSET_CAMERA 1

/* g_InsetViewAnimation values */
#define INSET_VIEW_STILL 0
#define INSET_VIEW_OPENING 1
#define INSET_VIEW_CLOSING 2

extern s32 g_InsetViewAnimation;
extern s32 g_InsetViewStep;

void SetInsetViewRect(s32 centerX, s32 centerY, s32 width, s32 height);
void UpdateInsetView(void);

/* Looks down on the course for the map views */
extern Camera g_MapCamera;

/* Set up by StartGame; no code in the main segment draws with it */
extern Camera g_SpareCamera;

#endif
