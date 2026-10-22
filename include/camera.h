#ifndef CAMERA_H
#define CAMERA_H

#include <ultra64.h>

/* The camera mode that frames the shot from behind the golfer */
#define CAMERA_MODE_BEHIND_GOLFER 1

/* Camera drawFlags bits */
#define CAMERA_DRAW_SCENE (1 << 0)
#define CAMERA_DRAW_SCREEN_EFFECT (1 << 1)

/* What a camera sees and where on the screen it draws */
typedef struct CameraView {
    /* 0x00 */ f32 eye[3];
    /* 0x0C */ f32 lookAt[3];
    /* 0x18 */ s32 unreadValue;
    /* 0x1C */ s32 scissorX;
    /* 0x20 */ s32 scissorY;
    /* 0x24 */ s32 scissorWidth;
    /* 0x28 */ s32 scissorHeight;
    /* 0x2C */ s32 unreadSettings[4]; /* Set by StartGame, never read */
    /* 0x3C */ f32 zoom;
    /* 0x40 */ s32 drawFlags; /* CAMERA_DRAW_* bits */
    /* 0x44 */ f32 fieldOfView; /* In degrees */
    /* 0x48 */ s32 mode;
} CameraView; /* size = 0x4C */

typedef struct Camera {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ CameraView view;
} Camera; /* size = 0x50 */

extern Camera g_Cameras[];

/* The matrices that DrawGame builds for one camera in one frame */
typedef struct CameraMatrices {
    /* 0x000 */ u8 unk0[0x1C0];
    /* 0x1C0 */ Mtx projection;
    /* 0x200 */ u8 unk200[0x50];
} CameraMatrices; /* size = 0x250 */

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
