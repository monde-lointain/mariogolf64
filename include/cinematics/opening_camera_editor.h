#ifndef OPENING_CAMERA_EDITOR_H
#define OPENING_CAMERA_EDITOR_H

#include <ultra64.h>

#include "math/geometry.h"
#include "math/math_util.h"
#include "system/c_language.h"

#define EDITOR_POINT_EYE 0
#define EDITOR_POINT_LOOK_AT 1
#define EDITOR_POINT_COUNT 2

enum EditorMoveAxis {
    EDITOR_MOVE_AXIS_NONE,
    EDITOR_MOVE_AXIS_X,
    EDITOR_MOVE_AXIS_Y,
    EDITOR_MOVE_AXIS_Z
};

typedef s32 EditorMoveAxis;

#define EDITOR_FLAG_TIMED_MOVE 0x2
#define EDITOR_FLAG_UNTIMED_MOVE 0x4
#define EDITOR_FLAG_PIVOT 0x8
#define EDITOR_FLAG_TURNING 0x10
#define EDITOR_FLAG_TILTING 0x20
#define EDITOR_FLAG_ZOOMING 0x40
#define EDITOR_FLAG_CARRYING 0x80
typedef s32 EditorFlags;

typedef struct CameraEditor {
    Vec3 points[EDITOR_POINT_COUNT];
    Radians heading;
    Radians pitch;
    f32 distance;
    f32 fieldOfView;
    Vec3 targets[EDITOR_POINT_COUNT];
    Vec3 pointVelocities[EDITOR_POINT_COUNT];
    f32 tiltSpeed;
    f32 turnSpeed;
    f32 zoomSpeed;
    f32 tiltLeft;
    f32 turnLeft;
    f32 zoomTarget;
    f32 pointTopSpeeds[EDITOR_POINT_COUNT];
    f32 pointAccelerations[EDITOR_POINT_COUNT];
    f32 turnTopSpeed;
    f32 turnAcceleration;
    f32 tiltTopSpeed;
    f32 tiltAcceleration;
    f32 zoomTopSpeed;
    f32 zoomAcceleration;
    f32 pointBrakingDistances[EDITOR_POINT_COUNT];
    Radians tiltBrakingAngle;
    Radians turnBrakingAngle;
    f32 zoomBrakingDistance;
    EditorFlags flags[EDITOR_POINT_COUNT];
    EditorMoveAxis moveAxes[EDITOR_POINT_COUNT];
} CameraEditor;

extern CameraEditor g_CameraEditor;
extern f32 g_LookAtMarkerScale;

void PlaceEditorCamera(f32 eyeX, f32 eyeY, f32 eyeZ, f32 lookAtX, f32 lookAtY, f32 lookAtZ);
void MeasureEditorCamera(void);
void MoveEditorPoint(s32 pointIndex, f32 x, f32 y, f32 z, s32 frameCount);
void MoveEditorCamera(s32 pointIndex, f32 x, f32 y, f32 z, s32 frameCount);
void TurnEditorCamera(s32 pointIndex, Radians angle, s32 frameCount);
void TiltEditorCamera(s32 pointIndex, Radians angle, s32 frameCount);
void ZoomEditorCamera(s32 pointIndex, f32 distance, s32 frameCount);
Boolean AreEditorMovesDone(void);
void StopEditorMoves(void);
void ShiftEditorCamera(Vec3 offset);
void UpdateCameraEditor(void);
void EditOpeningCamera(void);
void ResetCameraEditor(void);

#endif
