/*
 * The cameras: the main camera and the inset view, how each is placed, what it
 * sees and where on the screen it draws.
 */

#ifndef CAMERA_H
#define CAMERA_H

#include <ultra64.h>

#include "math/geometry.h"
#include "math/math_util.h"
#include "system/c_language.h"

// How a camera is placed
enum CameraMode {
    // Frames the shot from behind the golfer
    CAMERA_MODE_BEHIND_GOLFER = 1,
    // Set while the pause menu's free look moves the camera
    CAMERA_MODE_FREE_LOOK,
    // Set while the camera circles the hole during its intro
    CAMERA_MODE_HOLE_INTRO = 8
};

typedef s32 CameraMode;

// Whether a camera mode can come close to the ground; such cameras clip
// nearer, draw the ground with a finer depth scale and show no rain
#define IsCloseUpCameraMode(mode) \
    ((mode) == CAMERA_MODE_FREE_LOOK || (mode) == CAMERA_MODE_HOLE_INTRO)

// Camera drawFlags bits
#define CAMERA_DRAW_SCENE (1 << 0)
#define CAMERA_DRAW_SCREEN_EFFECT (1 << 1)

// What a camera sees and where on the screen it draws
typedef struct CameraView {
    Vec3 eye;
    Vec3 lookAt;
    Radians heading; // The map camera turns by it
    s32 scissorX;
    s32 scissorY;
    s32 scissorWidth;
    s32 scissorHeight;
    s32 unreadSettings[2]; // Set by StartGame, never read
    s32 viewportCenterX; // In half pixels
    s32 viewportCenterY;
    f32 zoom;
    s32 drawFlags; // CAMERA_DRAW_* bits
    f32 fieldOfView; // In degrees
} CameraView;

// The viewport center, in half pixels, of a scissor box from start that spans
// size
#define ViewportCenter(start, size) (((start) * 2) + (size))

typedef struct Camera {
    s32 unusedBeforeView;
    CameraView view;
    CameraMode mode; // A CAMERA_MODE_* value
} Camera;

#define MAIN_CAMERA 0
// The camera of the inset view, a window over the main view
#define INSET_CAMERA 1

// The main view's camera and INSET_CAMERA
#define CAMERA_COUNT 2
// The camera index that the opening draws its characters with; it has no g_Cameras entry, and
// DrawCharacterAndShadow always draws for it
#define OPENING_CAMERA CAMERA_COUNT

extern Camera g_Cameras[CAMERA_COUNT];

// The matrices that DrawGame builds for one camera in one frame
typedef struct CameraMatrices {
    Mtx perspective;
    Mtx viewing;
    Mtx placement; // Between the projection and the view
    Mtx billboard; // Turns billboards to face the camera
    Mtx antiBillboard; // The opposite turn
    Mat4 perspectiveF;
    Mat4 viewingF;
    Mtx projection; // perspectiveF and viewingF combined
    LookAt lookAt;
    u8 unusedAfterLookAt[0x10];
} CameraMatrices;

// Where the camera's points were last eased to, and whether they have been
typedef struct EasedView {
    f32 eye[AXIS_COUNT];
    f32 lookAt[AXIS_COUNT];
    Boolean8 isEyeEased;
    Boolean8 isLookAtEased;
} EasedView;

// The easedParts bits of EaseReactionView and EaseStaffRollView
#define EASE_LOOK_AT (1 << 0)
#define EASE_EYE (1 << 1)

// InsetView.animation values
enum InsetViewState {
    INSET_VIEW_STILL,
    INSET_VIEW_OPENING,
    INSET_VIEW_CLOSING
};

typedef s32 InsetViewState;

// How the inset view opens and closes
typedef struct InsetView {
    InsetViewState animation; // INSET_VIEW_*
    s32 step;
    // Two codes nothing reads. The source code is the same for the landing
    // reaction's insets and differs for the flyover's; the size code is the
    // same for the two 128-pixel insets and differs for the small landing one.
    s32 ignoredSourceCode;
    s32 ignoredSizeCode;
} InsetView;

extern InsetView g_InsetView;

#define RING_OUTER_RADIUS 62
void SetInsetViewRectangle(s32 centerX, s32 centerY, s32 width, s32 height);
void SetLandingWaitFrameCount(s32 frameCount);
void BuildRingTexels(u8 *texels, s32 width, s32 height, s32 outerRadius);
void UpdateInsetView(void);

// Looks down on the course for the map views
extern Camera g_MapCamera;

#endif
