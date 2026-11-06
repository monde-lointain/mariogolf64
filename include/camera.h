#ifndef CAMERA_H
#define CAMERA_H

#include <ultra64.h>

#include "math_util.h"

/* How a camera is placed */
enum CameraMode {
  /* Frames the shot from behind the golfer */
  CAMERA_MODE_BEHIND_GOLFER = 1,
  /* Set while the pause menu's free look moves the camera */
  CAMERA_MODE_FREE_LOOK,
  /* Set while the camera circles the hole during its intro */
  CAMERA_MODE_HOLE_INTRO = 8
};

/*
 * Whether a camera mode can come close to the ground; such cameras clip
 * nearer, draw the ground with a finer depth scale and show no rain
 */
#define IsCloseUpCameraMode(mode) \
  ((mode) == CAMERA_MODE_FREE_LOOK || (mode) == CAMERA_MODE_HOLE_INTRO)

/* Camera drawFlags bits */
#define CAMERA_DRAW_SCENE (1 << 0)
#define CAMERA_DRAW_SCREEN_EFFECT (1 << 1)

/* What a camera sees and where on the screen it draws */
typedef struct CameraView {
  /* 0x00 */ Vec3 eye;
  /* 0x0C */ Vec3 lookAt;
  /* 0x18 */ Radians heading; /* The map camera turns by it */
  /* 0x1C */ s32 scissorX;
  /* 0x20 */ s32 scissorY;
  /* 0x24 */ s32 scissorWidth;
  /* 0x28 */ s32 scissorHeight;
  /* 0x2C */ s32 unreadSettings[2]; /* Set by StartGame, never read */
  /* 0x34 */ s32 viewportCenterX; /* In half pixels */
  /* 0x38 */ s32 viewportCenterY;
  /* 0x3C */ f32 zoom;
  /* 0x40 */ s32 drawFlags; /* CAMERA_DRAW_* bits */
  /* 0x44 */ f32 fieldOfView; /* In degrees */
} CameraView; /* size = 0x48 */

typedef struct Camera {
  /* 0x00 */ s32 unk0;
  /* 0x04 */ CameraView view;
  /* 0x4C */ s32 mode; /* A CAMERA_MODE_* value */
} Camera; /* size = 0x50 */

/* The main view's camera and INSET_CAMERA */
#define CAMERA_COUNT 2

extern Camera g_Cameras[CAMERA_COUNT];

/* The matrices that DrawGame builds for one camera in one frame */
typedef struct CameraMatrices {
  /* 0x000 */ Mtx perspective;
  /* 0x040 */ Mtx viewing;
  /* 0x080 */ Mtx placement; /* Between the projection and the view */
  /* 0x0C0 */ Mtx billboard; /* Turns billboards to face the camera */
  /* 0x100 */ Mtx antiBillboard; /* The opposite turn */
  /* 0x140 */ Mat4 perspectiveF;
  /* 0x180 */ Mat4 viewingF;
  /* 0x1C0 */ Mtx projection; /* perspectiveF and viewingF combined */
  /* 0x200 */ LookAt lookAt;
  /* 0x220 */ u8 unk220[0x10];
} CameraMatrices; /* size = 0x230 */

/* The camera of the inset view, a window over the main view */
#define INSET_CAMERA 1
/*
 * The camera index that the opening draws its characters with; it has no
 * g_Cameras entry, and DrawCharacterAndShadow always draws for it
 */
#define OPENING_CAMERA 2

/* InsetView.animation values */
enum InsetViewState {
  INSET_VIEW_STILL,
  INSET_VIEW_OPENING,
  INSET_VIEW_CLOSING
};

/* How the inset view opens and closes */
typedef struct InsetView {
  /* 0x00 */ s32 animation; /* INSET_VIEW_* */
  /* 0x04 */ s32 step;
  /*
   * Two codes nothing reads. The source code is the same for the landing
   * reaction's insets and differs for the flyover's; the size code is the
   * same for the two 128-pixel insets and differs for the small landing one.
   */
  /* 0x08 */ s32 ignoredSourceCode;
  /* 0x0C */ s32 ignoredSizeCode;
} InsetView; /* size = 0x10 */

extern InsetView g_InsetView;

void SetInsetViewRect(s32 centerX, s32 centerY, s32 width, s32 height);
void SetLandingWaitFrameCount(s32 frameCount);
void BuildRingTexels(u8* texels, s32 width, s32 height, s32 outerRadius);
void UpdateInsetView(void);

/* Looks down on the course for the map views */
extern Camera g_MapCamera;

#endif
