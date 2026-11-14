#ifndef CAMERA_H
#define CAMERA_H

#include <ultra64.h>

#include "geometry.h"
#include "language.h"
#include "math_util.h"

enum CameraMode {
    CAMERA_MODE_BEHIND_GOLFER = 1,
    CAMERA_MODE_FREE_LOOK,
    CAMERA_MODE_HOLE_INTRO = 8
};

#define IsCloseUpCameraMode(mode) \
    ((mode) == CAMERA_MODE_FREE_LOOK || (mode) == CAMERA_MODE_HOLE_INTRO)

#define CAMERA_DRAW_SCENE (1 << 0)
#define CAMERA_DRAW_SCREEN_EFFECT (1 << 1)

typedef struct CameraView {
    Vec3 eye;
    Vec3 lookAt;
    Radians heading;
    s32 scissorX;
    s32 scissorY;
    s32 scissorWidth;
    s32 scissorHeight;
    s32 unreadSettings[2];
    s32 viewportCenterX;
    s32 viewportCenterY;
    f32 zoom;
    s32 drawFlags;
    f32 fieldOfView;
} CameraView;

#define ViewportCenter(start, size) (((start) * 2) + (size))

typedef struct Camera {
    s32 unusedBeforeView;
    CameraView view;
    s32 mode;
} Camera;

#define CAMERA_COUNT 2

extern Camera g_Cameras[CAMERA_COUNT];

typedef struct CameraMatrices {
    Mtx perspective;
    Mtx viewing;
    Mtx placement;
    Mtx billboard;
    Mtx antiBillboard;
    Mat4 perspectiveF;
    Mat4 viewingF;
    Mtx projection;
    LookAt lookAt;
    u8 unusedAfterLookAt[0x10];
} CameraMatrices;

// A camera eye and look-at point that each start easing toward their
// target once set.
typedef struct EasedView {
    f32 eye[AXIS_COUNT];
    f32 lookAt[AXIS_COUNT];
    Boolean8 isEyeEased;
    Boolean8 isLookAtEased;
} EasedView;

// Parts of an EasedView to ease, combined.
#define EASE_LOOK_AT (1 << 0)
#define EASE_EYE (1 << 1)

#define INSET_CAMERA 1
#define OPENING_CAMERA 2

enum InsetViewState {
    INSET_VIEW_STILL,
    INSET_VIEW_OPENING,
    INSET_VIEW_CLOSING
};

typedef s32 InsetViewState;

typedef struct InsetView {
    InsetViewState animation;
    s32 step;
    s32 ignoredSourceCode;
    s32 ignoredSizeCode;
} InsetView;

extern InsetView g_InsetView;

void SetInsetViewRectangle(s32 centerX, s32 centerY, s32 width, s32 height);
void SetLandingWaitFrameCount(s32 frameCount);
void BuildRingTexels(u8 *texels, s32 width, s32 height, s32 outerRadius);
void UpdateInsetView(void);

extern Camera g_MapCamera;

#endif
