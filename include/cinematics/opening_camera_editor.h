/*
 * The debug editor that moves the opening's camera.
 */

#ifndef OPENING_CAMERA_EDITOR_H
#define OPENING_CAMERA_EDITOR_H

#include <ultra64.h>

#include "math/geometry.h"
#include "math/math_util.h"
#include "system/c_language.h"

// The points of the camera that the editor moves
#define EDITOR_POINT_EYE 0
#define EDITOR_POINT_LOOK_AT 1
#define EDITOR_POINT_COUNT 2

// The axis along which MoveEditorPoint moves a point furthest
enum EditorMoveAxis {
    EDITOR_MOVE_AXIS_NONE,
    EDITOR_MOVE_AXIS_X,
    EDITOR_MOVE_AXIS_Y,
    EDITOR_MOVE_AXIS_Z
};

typedef s32 EditorMoveAxis;

// CameraEditor flags bits, kept for each point
// The point moves to its target at speeds set from a frame count
#define EDITOR_FLAG_TIMED_MOVE 0x2
// The point moves to its target at the speeds it already has
#define EDITOR_FLAG_UNTIMED_MOVE 0x4
// The other point turns, tilts or zooms around this one
#define EDITOR_FLAG_PIVOT 0x8
// The point turns around the pivot, changing the heading
#define EDITOR_FLAG_TURNING 0x10
// The point tilts around the pivot, changing the pitch
#define EDITOR_FLAG_TILTING 0x20
// The point moves toward or away from the pivot, changing the distance
#define EDITOR_FLAG_ZOOMING 0x40
// The point moves to its target and carries the other point with it
#define EDITOR_FLAG_CARRYING 0x80

typedef s32 EditorFlags;

// The state of the opening's camera editor, starting with the camera it moves
typedef struct CameraEditor {
    Vec3 points[EDITOR_POINT_COUNT]; // The eye, then the look-at point
    Radians heading; // Of the look direction across the ground
    Radians pitch; // Of the look direction above the ground
    f32 distance; // From the eye to the look-at point
    f32 fieldOfView; // In degrees
    Vec3 targets[EDITOR_POINT_COUNT]; // Where MoveEditorPoint moves the points to
    Vec3 pointVelocities[EDITOR_POINT_COUNT]; // Added to each point every frame
    f32 tiltSpeed; // The pitch change this frame
    f32 turnSpeed; // The heading change this frame
    f32 zoomSpeed; // The distance change this frame
    f32 tiltLeft; // The pitch change still to make
    f32 turnLeft; // The heading change still to make
    f32 zoomTarget; // The distance a zoom ends at
    f32 pointTopSpeeds[EDITOR_POINT_COUNT]; // Lowered while braking
    f32 pointAccelerations[EDITOR_POINT_COUNT];
    f32 turnTopSpeed;
    f32 turnAcceleration;
    f32 tiltTopSpeed;
    f32 tiltAcceleration;
    f32 zoomTopSpeed;
    f32 zoomAcceleration;
    f32 pointBrakingDistances[EDITOR_POINT_COUNT]; // Within which a moving point starts braking
    Radians tiltBrakingAngle; // Within which a tilt starts braking
    Radians turnBrakingAngle; // Within which a turn starts braking
    f32 zoomBrakingDistance; // Within which a zoom starts braking
    EditorFlags flags[EDITOR_POINT_COUNT]; // EDITOR_FLAG_* bits
    EditorMoveAxis moveAxes[EDITOR_POINT_COUNT]; // EDITOR_MOVE_AXIS_*, along which a move ends
} CameraEditor;

extern CameraEditor g_CameraEditor;
// The size of the box that marks the camera's look-at point
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
