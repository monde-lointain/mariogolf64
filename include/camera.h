#ifndef CAMERA_H
#define CAMERA_H

#include <ultra64.h>

/* The camera mode that frames the shot from behind the golfer */
#define CAMERA_MODE_BEHIND_GOLFER 1
#define CAMERA_MODE_UNK2 2

/* Camera drawFlags bits */
#define CAMERA_DRAW_SCENE (1 << 0)
#define CAMERA_DRAW_SCREEN_EFFECT (1 << 1)

/* What a camera sees and where on the screen it draws */
typedef struct CameraView {
    /* 0x00 */ Vec3 eye;
    /* 0x0C */ Vec3 lookAt;
    /* 0x18 */ f32 heading; /* In radians; the map camera turns by it */
    /* 0x1C */ s32 scissorX;
    /* 0x20 */ s32 scissorY;
    /* 0x24 */ s32 scissorWidth;
    /* 0x28 */ s32 scissorHeight;
    /* 0x2C */ s32 unreadSettings[2]; /* Set by StartGame, never read */
    /* 0x34 */ s32 viewportCenterX;   /* In half pixels */
    /* 0x38 */ s32 viewportCenterY;
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
    /* 0x000 */ Mtx perspective;
    /* 0x040 */ Mtx viewing;
    /* 0x080 */ Mtx placement;     /* Between the projection and the view */
    /* 0x0C0 */ Mtx billboard;     /* Turns billboards to face the camera */
    /* 0x100 */ Mtx antiBillboard; /* The opposite turn */
    /* 0x140 */ Mat4 perspectiveF;
    /* 0x180 */ Mat4 viewingF;
    /* 0x1C0 */ Mtx projection; /* perspectiveF and viewingF combined */
    /* 0x200 */ LookAt lookAt;
    /* 0x220 */ u8 unk220[0x10];
} CameraMatrices; /* size = 0x230 */

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

/*
 * Set up by StartGame; no code in the main segment draws with it. Only its
 * fields up to zoom are used, and libc's errno sits where its mode would be
 */
extern Camera g_SpareCamera;

#endif
