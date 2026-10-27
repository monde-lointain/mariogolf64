#ifndef CAMERA_EDITOR_H
#define CAMERA_EDITOR_H

#include "common.h"

/* The points of the camera that the editor moves */
#define EDITOR_POINT_EYE 0
#define EDITOR_POINT_LOOK_AT 1
#define EDITOR_POINT_COUNT 2

/* The axis along which MoveEditorPoint moves a point furthest */
#define EDITOR_MOVE_AXIS_X 1
#define EDITOR_MOVE_AXIS_Y 2
#define EDITOR_MOVE_AXIS_Z 3

/* The state of the opening's camera editor, starting with the camera it moves
 */
typedef struct CameraEditor {
  /* 0x00 */ Vec3
      points[EDITOR_POINT_COUNT]; /* The eye, then the look-at point */
  /* 0x18 */ f32
      heading; /* In radians, of the look direction across the ground */
  /* 0x1C */ f32 pitch; /* In radians, of the look direction above the ground */
  /* 0x20 */ f32 distance;    /* From the eye to the look-at point */
  /* 0x24 */ f32 fieldOfView; /* In degrees */
  /* 0x28 */ Vec3 targets[EDITOR_POINT_COUNT]; /* Where MoveEditorPoint moves
                                                  the points to */
  /* 0x40 */ Vec3 unk40[2];
  /* 0x58 */ f32 unk58;
  /* 0x5C */ f32 unk5C;
  /* 0x60 */ f32 unk60;
  /* 0x64 */ f32 unk64;
  /* 0x68 */ f32 unk68;
  /* 0x6C */ f32 unk6C;
  /* 0x70 */ f32 unk70[2];
  /* 0x78 */ f32 unk78[2];
  /* 0x80 */ f32 unk80;
  /* 0x84 */ f32 unk84;
  /* 0x88 */ f32 unk88;
  /* 0x8C */ f32 unk8C;
  /* 0x90 */ f32 unk90;
  /* 0x94 */ f32 unk94;
  /* 0x98 */ f32 unk98[2];
  /* 0xA0 */ f32 unkA0;
  /* 0xA4 */ f32 unkA4;
  /* 0xA8 */ f32 unkA8;
  /* 0xAC */ s32 flags[2]; /* EDITOR_FLAG_* bits */
  /* 0xB4 */ s32 unkB4[2];
} CameraEditor; /* size = 0xBC */

/* CameraEditor flags bits, kept for each point */
/* The point moves to its target at speeds set from a frame count */
#define EDITOR_FLAG_TIMED_MOVE 0x2
/* The point moves to its target at the speeds it already has */
#define EDITOR_FLAG_UNTIMED_MOVE 0x4
/* The other point turns, tilts or zooms around this one */
#define EDITOR_FLAG_PIVOT 0x8
/* The point turns around the pivot, changing the heading */
#define EDITOR_FLAG_TURNING 0x10
/* The point tilts around the pivot, changing the pitch */
#define EDITOR_FLAG_TILTING 0x20
/* The point moves toward or away from the pivot, changing the distance */
#define EDITOR_FLAG_ZOOMING 0x40
/* The point moves to its target and carries the other point with it */
#define EDITOR_FLAG_CARRYING 0x80

extern CameraEditor g_CameraEditor;
/* The size of the box that marks the camera's look-at point */
extern f32 g_LookAtMarkerScale;

void MeasureEditorCamera(void);
void MoveEditorPoint(s32 index, f32 x, f32 y, f32 z, s32 frameCount);

#endif
