/*
 * The camera editor that moves the opening's camera: its eye and look-at point
 * move, turn, tilt and zoom at set speeds.
 */
#include "common.h"

#include "camera_editor.h"
#include "controller.h"
#include "debug_text.h"
#include "host_file.h"
#include "libc.h"
#include "math_util.h"
#include "sound.h"
#include "variables.h"

#define SNAPSHOT_COUNT 32
/* A snapshot holds the look-at point first, unlike CameraEditor.points */
#define SNAPSHOT_LOOK_AT 0
#define SNAPSHOT_EYE 1
#define SNAPSHOT_POINT_COUNT 2

/* What the first controller's stick and C buttons change */
enum EditorChoice {
  EDITOR_CHOICE_SLIDE, /* Both points */
  EDITOR_CHOICE_TARGET, /* The look-at point */
  EDITOR_CHOICE_POSITION, /* The eye */
  EDITOR_CHOICE_FIELD_OF_VIEW, /* And the look-at marker's size */
  EDITOR_CHOICE_COUNT
};

static Boolean f_IsSnapshotSaved[SNAPSHOT_COUNT] = {0};
/* The editor's menu choice, an EditorChoice */
static s32 f_EditorChoice = EDITOR_CHOICE_SLIDE;
static s32 f_SnapshotIndex = 0;
/* Whether the second controller moves the eye rather than the look-at point */
static Boolean f_IsSecondPadOnEye = FALSE;

/* Not static: the original left it to the linker's common block */
f32 f_CameraSnapshots[SNAPSHOT_COUNT][SNAPSHOT_POINT_COUNT][3];

/* Starts the editor with its eye at (0, -100, 100) looking at the origin, and
 * its moves stopped */
void ResetCameraEditor(void) {
  s32 i;

  g_CameraEditor.points[EDITOR_POINT_EYE][0] = 0.0f;
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] = 0.0f;
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] = 0.0f;
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] = 0.0f;
  g_CameraEditor.points[EDITOR_POINT_EYE][1] = -100.0f;
  g_CameraEditor.points[EDITOR_POINT_EYE][2] = 100.0f;
  g_CameraEditor.fieldOfView = 45.0f;
  for (i = 0; i != EDITOR_POINT_COUNT; i++) {
    g_CameraEditor.targets[i][0] = 0.0f;
    g_CameraEditor.targets[i][1] = 0.0f;
    g_CameraEditor.targets[i][2] = 0.0f;
    g_CameraEditor.pointVelocities[i][0] = 0.0f;
    g_CameraEditor.pointVelocities[i][1] = 0.0f;
    g_CameraEditor.pointVelocities[i][2] = 0.0f;

    /* The game clears these twice */
    g_CameraEditor.pointVelocities[i][0] = 0.0f;
    g_CameraEditor.pointVelocities[i][1] = 0.0f;
    g_CameraEditor.pointVelocities[i][2] = 0.0f;
    g_CameraEditor.pointTopSpeeds[i] = 30.0f;
    g_CameraEditor.pointAccelerations[i] = 3.0f;
    g_CameraEditor.pointBrakingDistances[i] = 0.0f;
    g_CameraEditor.moveAxes[i] = 0;
    g_CameraEditor.flags[i] = 0;
  }

  g_CameraEditor.tiltBrakingAngle = 0.0f;
  g_CameraEditor.turnBrakingAngle = 0.0f;
  g_CameraEditor.zoomBrakingDistance = 0.0f;
  g_CameraEditor.turnTopSpeed = DEGREES_TO_RADIANS(6.0f);
  g_CameraEditor.turnAcceleration = DEGREES_TO_RADIANS(2.0f);
  g_CameraEditor.tiltTopSpeed = DEGREES_TO_RADIANS(3.0f);
  g_CameraEditor.tiltAcceleration = DEGREES_TO_RADIANS(1.0f);
  g_CameraEditor.zoomTopSpeed = 10.0f;
  g_CameraEditor.zoomAcceleration = 1.0f;
  MeasureEditorCamera();
  g_LookAtMarkerScale = 1.0f;
}

/* Puts the camera's eye and look-at point at the given positions */
void PlaceEditorCamera(
    f32 eyeX, f32 eyeY, f32 eyeZ, f32 lookAtX, f32 lookAtY, f32 lookAtZ) {
  g_CameraEditor.points[EDITOR_POINT_EYE][0] = eyeX;
  g_CameraEditor.points[EDITOR_POINT_EYE][1] = eyeY;
  g_CameraEditor.points[EDITOR_POINT_EYE][2] = eyeZ;
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] = lookAtX;
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] = lookAtY;
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] = lookAtZ;
  MeasureEditorCamera();
}

/* Sets the top speed and acceleration of point index's moves. Nothing calls
 * it */
void SetEditorPointSpeeds(s32 index, f32 topSpeed, f32 acceleration) {
  g_CameraEditor.pointTopSpeeds[index] = topSpeed;
  g_CameraEditor.pointAccelerations[index] = acceleration;
}

/* Sets the top speed and acceleration of turns. Nothing calls it */
void SetEditorTurnSpeeds(s32 unusedIndex, f32 topSpeed, f32 acceleration) {
  g_CameraEditor.turnTopSpeed = topSpeed;
  g_CameraEditor.turnAcceleration = acceleration;
}

/* Sets the top speed and acceleration of tilts. Nothing calls it */
void SetEditorTiltSpeeds(s32 unusedIndex, f32 topSpeed, f32 acceleration) {
  g_CameraEditor.tiltTopSpeed = topSpeed;
  g_CameraEditor.tiltAcceleration = acceleration;
}

/* Sets the top speed and acceleration of zooms. Nothing calls it */
void SetEditorZoomSpeeds(s32 unusedIndex, f32 topSpeed, f32 acceleration) {
  g_CameraEditor.zoomTopSpeed = topSpeed;
  g_CameraEditor.zoomAcceleration = acceleration;
}

/* Works out the camera's heading, pitch and distance from its eye and look-at
 * point */
void MeasureEditorCamera(void) {
  f32 offsetX;
  f32 offsetY;
  f32 offsetZ;

  offsetX = g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] -
      g_CameraEditor.points[EDITOR_POINT_EYE][0];
  g_CameraEditor.pitch = atan2f(g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] -
          g_CameraEditor.points[EDITOR_POINT_EYE][1],
      sqrtf((offsetX * offsetX) +
          ((g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] -
               g_CameraEditor.points[EDITOR_POINT_EYE][2]) *
              (g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] -
                  g_CameraEditor.points[EDITOR_POINT_EYE][2]))));
  g_CameraEditor.heading =
      atan2f(g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] -
              g_CameraEditor.points[EDITOR_POINT_EYE][2],
          g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] -
              g_CameraEditor.points[EDITOR_POINT_EYE][0]);
  offsetY = g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] -
      g_CameraEditor.points[EDITOR_POINT_EYE][1];
  offsetZ = g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] -
      g_CameraEditor.points[EDITOR_POINT_EYE][2];
  g_CameraEditor.distance =
      sqrtf(((g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] -
                 g_CameraEditor.points[EDITOR_POINT_EYE][0]) *
                (g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] -
                    g_CameraEditor.points[EDITOR_POINT_EYE][0])) +
          (offsetY * offsetY) + (offsetZ * offsetZ));
}

/* Starts moving point index to (x, y, z) over frameCount frames, or snaps it
 * there when it is close */
void MoveEditorPoint(s32 index, f32 x, f32 y, f32 z, s32 frameCount) {
  f32 distance;
  s32 easeFrameCount;
  f32 speed;

  distance = sqrtf(((x - g_CameraEditor.points[index][0]) *
                       (x - g_CameraEditor.points[index][0])) +
      ((y - g_CameraEditor.points[index][1]) *
          (y - g_CameraEditor.points[index][1])) +
      ((z - g_CameraEditor.points[index][2]) *
          (z - g_CameraEditor.points[index][2])));
  if (distance < 0.1f) {
    g_CameraEditor.points[index][0] = x;
    g_CameraEditor.points[index][1] = y;
    g_CameraEditor.points[index][2] = z;
    return;
  }

  if (frameCount != 0) {
    easeFrameCount = frameCount / 4;
    speed = distance / (frameCount - easeFrameCount);
    g_CameraEditor.pointTopSpeeds[index] = speed;
    g_CameraEditor.pointAccelerations[index] = speed / (easeFrameCount + 1);
    g_CameraEditor.pointBrakingDistances[index] = distance * 0.25;
    if (!(g_CameraEditor.flags[index] & EDITOR_FLAG_CARRYING)) {
      if (!(g_CameraEditor.flags[index] & EDITOR_FLAG_TIMED_MOVE)) {
        g_CameraEditor.flags[index] ^= EDITOR_FLAG_TIMED_MOVE;
      }

      if (g_CameraEditor.flags[index] & EDITOR_FLAG_UNTIMED_MOVE) {
        g_CameraEditor.flags[index] ^= EDITOR_FLAG_UNTIMED_MOVE;
      }
    }
  } else {
    if (!(g_CameraEditor.flags[index] & EDITOR_FLAG_UNTIMED_MOVE)) {
      g_CameraEditor.flags[index] ^= EDITOR_FLAG_UNTIMED_MOVE;
    }

    if (g_CameraEditor.flags[index] & EDITOR_FLAG_TIMED_MOVE) {
      g_CameraEditor.flags[index] ^= EDITOR_FLAG_TIMED_MOVE;
    }
  }

  g_CameraEditor.targets[index][0] = x;
  g_CameraEditor.targets[index][1] = y;
  g_CameraEditor.targets[index][2] = z;

  /* The arguments become the offsets, and x the larger of the ground ones */
  x -= g_CameraEditor.points[index][0];
  y -= g_CameraEditor.points[index][1];
  z -= g_CameraEditor.points[index][2];
  g_CameraEditor.moveAxes[index] = EDITOR_MOVE_AXIS_X;
  if (fabsf(x) < fabsf(z)) {
    g_CameraEditor.moveAxes[index] = EDITOR_MOVE_AXIS_Z;
    x = z;
  }

  if (fabsf(x) < fabsf(y)) {
    g_CameraEditor.moveAxes[index] = EDITOR_MOVE_AXIS_Y;
  }
}

/* Moves point index to (x, y, z) over frameCount frames, carrying the other
 * point with it so that the camera keeps its direction and distance */
void MoveEditorCamera(s32 index, f32 x, f32 y, f32 z, s32 frameCount) {
  if (!(g_CameraEditor.flags[index] & EDITOR_FLAG_CARRYING)) {
    g_CameraEditor.flags[index] ^= EDITOR_FLAG_CARRYING;
  }

  if (g_CameraEditor.flags[index ^ 1] & EDITOR_FLAG_CARRYING) {
    g_CameraEditor.flags[index ^ 1] ^= EDITOR_FLAG_CARRYING;
  }

  if (!(g_CameraEditor.flags[index] & EDITOR_FLAG_PIVOT)) {
    g_CameraEditor.flags[index] ^= EDITOR_FLAG_PIVOT;
  }

  if (g_CameraEditor.flags[index ^ 1] & EDITOR_FLAG_PIVOT) {
    g_CameraEditor.flags[index ^ 1] ^= EDITOR_FLAG_PIVOT;
  }

  MoveEditorPoint(index, x, y, z, frameCount);
}

/* Starts turning the camera's heading by angle over frameCount frames, with
 * the other point swinging around point index, or turns it at once when angle
 * is tiny */
/* Doesn't match: the original loads the threshold after taking the absolute
 * value; GCC schedules it first */
#ifdef NON_MATCHING
void TurnEditorCamera(s32 index, f32 angle, s32 frameCount) {
  s32 easeFrameCount;
  f32 speed;

  if (fabsf(angle) < 0.001f) {
    g_CameraEditor.heading += angle;
    return;
  }

  if (frameCount != 0) {
    easeFrameCount = frameCount / 4;
    speed = angle / (frameCount - easeFrameCount);
    g_CameraEditor.turnBrakingAngle = angle * 0.25;
    g_CameraEditor.turnTopSpeed = speed;
    g_CameraEditor.turnAcceleration = speed / (easeFrameCount + 1);
  }

  g_CameraEditor.turnLeft = angle;
  if (!(g_CameraEditor.flags[index] & EDITOR_FLAG_PIVOT)) {
    g_CameraEditor.flags[index] ^= EDITOR_FLAG_PIVOT;
  }

  if (g_CameraEditor.flags[index ^ 1] & EDITOR_FLAG_PIVOT) {
    g_CameraEditor.flags[index ^ 1] ^= EDITOR_FLAG_PIVOT;
  }

  if (g_CameraEditor.flags[index] & EDITOR_FLAG_TURNING) {
    g_CameraEditor.flags[index] ^= EDITOR_FLAG_TURNING;
  }

  if (!(g_CameraEditor.flags[index ^ 1] & EDITOR_FLAG_TURNING)) {
    g_CameraEditor.flags[index ^ 1] ^= EDITOR_FLAG_TURNING;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", TurnEditorCamera);
#endif

/* Starts tilting the camera's pitch by angle over frameCount frames, with the
 * other point swinging around point index, or tilts it at once when angle is
 * tiny */
/* Doesn't match: the original loads the threshold after taking the absolute
 * value; GCC schedules it first */
#ifdef NON_MATCHING
void TiltEditorCamera(s32 index, f32 angle, s32 frameCount) {
  s32 easeFrameCount;
  f32 speed;

  if (fabsf(angle) < 0.001f) {
    g_CameraEditor.pitch += angle;
    return;
  }

  if (frameCount != 0) {
    easeFrameCount = frameCount / 4;
    speed = angle / (frameCount - easeFrameCount);
    g_CameraEditor.tiltBrakingAngle = angle * 0.25;
    g_CameraEditor.tiltTopSpeed = speed;
    g_CameraEditor.tiltAcceleration = speed / (easeFrameCount + 1);
  }

  g_CameraEditor.tiltLeft = angle;
  if (!(g_CameraEditor.flags[index] & EDITOR_FLAG_PIVOT)) {
    g_CameraEditor.flags[index] ^= EDITOR_FLAG_PIVOT;
  }

  if (g_CameraEditor.flags[index ^ 1] & EDITOR_FLAG_PIVOT) {
    g_CameraEditor.flags[index ^ 1] ^= EDITOR_FLAG_PIVOT;
  }

  if (g_CameraEditor.flags[index] & EDITOR_FLAG_TILTING) {
    g_CameraEditor.flags[index] ^= EDITOR_FLAG_TILTING;
  }

  if (!(g_CameraEditor.flags[index ^ 1] & EDITOR_FLAG_TILTING)) {
    g_CameraEditor.flags[index ^ 1] ^= EDITOR_FLAG_TILTING;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", TiltEditorCamera);
#endif

/* Starts moving the other point to distance from point index over frameCount
 * frames, or moves it at once when it is close */
/* Doesn't match: the original loads the threshold after taking the absolute
 * value; GCC schedules it first */
#ifdef NON_MATCHING
void ZoomEditorCamera(s32 index, f32 distance, s32 frameCount) {
  s32 easeFrameCount;
  f32 change;
  f32 speed;

  change = distance - g_CameraEditor.distance;
  if (fabsf(change) < 0.1f) {
    g_CameraEditor.distance = distance;
    return;
  }

  if (frameCount != 0) {
    easeFrameCount = frameCount / 4;
    speed = change / (frameCount - easeFrameCount);
    g_CameraEditor.zoomTopSpeed = speed;
    g_CameraEditor.zoomAcceleration = speed / (easeFrameCount + 1);
    g_CameraEditor.zoomBrakingDistance = fabsf(change) * 0.25;
  }

  g_CameraEditor.unk6C = change;
  if (!(g_CameraEditor.flags[index] & EDITOR_FLAG_PIVOT)) {
    g_CameraEditor.flags[index] ^= EDITOR_FLAG_PIVOT;
  }

  if (g_CameraEditor.flags[index ^ 1] & EDITOR_FLAG_PIVOT) {
    g_CameraEditor.flags[index ^ 1] ^= EDITOR_FLAG_PIVOT;
  }

  if (g_CameraEditor.flags[index] & EDITOR_FLAG_ZOOMING) {
    g_CameraEditor.flags[index] ^= EDITOR_FLAG_ZOOMING;
  }

  if (!(g_CameraEditor.flags[index ^ 1] & EDITOR_FLAG_ZOOMING)) {
    g_CameraEditor.flags[index ^ 1] ^= EDITOR_FLAG_ZOOMING;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", ZoomEditorCamera);
#endif

/* Returns whether both points have stopped moving */
Boolean AreEditorMovesDone(void) {
  if (g_CameraEditor.flags[0] == 0 && g_CameraEditor.flags[1] == 0) {
    return TRUE;
  }

  return FALSE;
}

/* Returns whether neither point has any of the EDITOR_FLAG_* bits in mask */
Boolean AreEditorFlagsClear(s32 mask) {
  if (!(g_CameraEditor.flags[0] & mask) && !(g_CameraEditor.flags[1] & mask)) {
    return TRUE;
  }

  return FALSE;
}

/* Stops the editor's moves and puts its speeds back to their starting values */
void StopEditorMoves(void) {
  s32 i;

  for (i = 0; i != EDITOR_POINT_COUNT; i++) {
    g_CameraEditor.targets[i][0] = 0.0f;
    g_CameraEditor.targets[i][1] = 0.0f;
    g_CameraEditor.targets[i][2] = 0.0f;
    g_CameraEditor.pointVelocities[i][0] = 0.0f;
    g_CameraEditor.pointVelocities[i][1] = 0.0f;
    g_CameraEditor.pointVelocities[i][2] = 0.0f;

    /* The game clears these twice */
    g_CameraEditor.pointVelocities[i][0] = 0.0f;
    g_CameraEditor.pointVelocities[i][1] = 0.0f;
    g_CameraEditor.pointVelocities[i][2] = 0.0f;
    g_CameraEditor.pointTopSpeeds[i] = 30.0f;
    g_CameraEditor.pointAccelerations[i] = 3.0f;
    g_CameraEditor.pointBrakingDistances[i] = 0.0f;
    g_CameraEditor.moveAxes[i] = 0;
    g_CameraEditor.flags[i] = 0;
  }

  g_CameraEditor.tiltBrakingAngle = 0.0f;
  g_CameraEditor.turnBrakingAngle = 0.0f;
  g_CameraEditor.zoomBrakingDistance = 0.0f;
  g_CameraEditor.turnTopSpeed = DEGREES_TO_RADIANS(6.0f);
  g_CameraEditor.turnAcceleration = DEGREES_TO_RADIANS(2.0f);
  g_CameraEditor.tiltTopSpeed = DEGREES_TO_RADIANS(3.0f);
  g_CameraEditor.tiltAcceleration = DEGREES_TO_RADIANS(1.0f);
  g_CameraEditor.zoomTopSpeed = 10.0f;
  g_CameraEditor.zoomAcceleration = 1.0f;
  MeasureEditorCamera();
}

/* Within these, a move, turn, tilt or zoom has arrived */
#define EDITOR_ARRIVAL_DISTANCE 0.1f
#define EDITOR_ARRIVAL_ANGLE 0.001f
/* The share of the acceleration that braking takes off the top speed each
 * update */
#define EDITOR_BRAKING_RATE 0.63979524f
/* The pitch stays within this of level, in radians */
#define EDITOR_PITCH_LIMIT 1.5f
/* The closest a zoom brings the eye to the look-at point */
#define EDITOR_MIN_DISTANCE 1.0f

/* Braking lowers a top speed no further than the acceleration */
#define KEEP_TOP_SPEED(topSpeed, acceleration) \
  do { \
    if (fabsf(topSpeed) < fabsf(acceleration)) { \
      (topSpeed) = (acceleration); \
    } \
  } while (0)

/*
 * Moves the camera on by an update: the point being moved, or carried with
 * the other, speeds up, brakes near its target and stops on reaching it, and
 * then the turn, tilt and zoom around the pivot point do the same. The
 * point that moves around the pivot is then placed from the heading, pitch
 * and distance.
 */
/* Doesn't match: GCC folds the eight flag tests before letting go of the pivot
 * into one mask test, keeps the zoom's gap in a float register rather than
 * $s6, and allocates the fabsf operands to other registers */
#ifdef NON_MATCHING
void UpdateCameraEditor(void) {
  /* Whether a point carries the other, and later whether one is a pivot */
  Boolean hasLeader = FALSE;
  Boolean hasMoved = FALSE;
  s32 index;
  Boolean isDone;
  f32 x;
  f32 y;
  f32 z; /* Also the distance before a zoom */
  f32 offsetX;
  f32 offsetY;
  f32 offsetZ;
  f32 distance;
  f32 scale;
  f32 gap;
  f32 zoomChange;
  f32 left;
  f32 cosPitch;

  if (g_CameraEditor.flags[EDITOR_POINT_EYE] == 0 &&
      g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] == 0) {
    return;
  }

  for (index = 0; index != EDITOR_POINT_COUNT; index++) {
    if (g_CameraEditor.flags[EDITOR_POINT_EYE] & EDITOR_FLAG_CARRYING) {
      index = EDITOR_POINT_EYE;
      hasLeader = TRUE;
    }

    if (g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] & EDITOR_FLAG_CARRYING) {
      index = EDITOR_POINT_LOOK_AT;
      hasLeader = TRUE;
    }

    if (!hasLeader && !(g_CameraEditor.flags[index] & EDITOR_FLAG_TIMED_MOVE) &&
        !(g_CameraEditor.flags[index] & EDITOR_FLAG_UNTIMED_MOVE)) {
      continue;
    }

    if (g_CameraEditor.moveAxes[index] == 0) {
      if (hasLeader) {
        break;
      }

      continue;
    }

    x = g_CameraEditor.points[index][0];
    y = g_CameraEditor.points[index][1];
    z = g_CameraEditor.points[index][2];
    if (g_CameraEditor.targets[index][0] != 0.0f ||
        g_CameraEditor.targets[index][1] != 0.0f ||
        g_CameraEditor.targets[index][2] != 0.0f) {
      offsetX = g_CameraEditor.targets[index][0] - x;
      offsetY = g_CameraEditor.targets[index][1] - y;
      offsetZ = g_CameraEditor.targets[index][2] - z;
      distance = sqrtf(
          (offsetX * offsetX) + (offsetY * offsetY) + (offsetZ * offsetZ));
      if (distance < EDITOR_ARRIVAL_DISTANCE) {
        x = g_CameraEditor.targets[index][0];
        y = g_CameraEditor.targets[index][1];
        z = g_CameraEditor.targets[index][2];
      } else {
        scale = g_CameraEditor.pointAccelerations[index] / distance;
        g_CameraEditor.pointVelocities[index][0] += offsetX * scale;
        g_CameraEditor.pointVelocities[index][1] += offsetY * scale;
        g_CameraEditor.pointVelocities[index][2] += offsetZ * scale;
        if (distance < g_CameraEditor.pointBrakingDistances[index]) {
          g_CameraEditor.pointTopSpeeds[index] -=
              g_CameraEditor.pointAccelerations[index] * EDITOR_BRAKING_RATE;
          if (g_CameraEditor.pointTopSpeeds[index] <
              g_CameraEditor.pointAccelerations[index]) {
            g_CameraEditor.pointTopSpeeds[index] =
                g_CameraEditor.pointAccelerations[index];
          }
        }

        offsetX = g_CameraEditor.pointVelocities[index][0];
        offsetY = g_CameraEditor.pointVelocities[index][1];
        offsetZ = g_CameraEditor.pointVelocities[index][2];
        distance = sqrtf(
            (offsetX * offsetX) + (offsetY * offsetY) + (offsetZ * offsetZ));
        if (g_CameraEditor.pointTopSpeeds[index] < distance) {
          scale = g_CameraEditor.pointTopSpeeds[index] / distance;
          g_CameraEditor.pointVelocities[index][0] = offsetX * scale;
          g_CameraEditor.pointVelocities[index][1] = offsetY * scale;
          g_CameraEditor.pointVelocities[index][2] = offsetZ * scale;
        }
      }
    }

    x += g_CameraEditor.pointVelocities[index][0];
    y += g_CameraEditor.pointVelocities[index][1];
    z += g_CameraEditor.pointVelocities[index][2];

    /* The move ends on reaching the target along its axis, or passing it */
    isDone = FALSE;
    switch (g_CameraEditor.moveAxes[index]) {
      case EDITOR_MOVE_AXIS_X:
        offsetX = x - g_CameraEditor.targets[index][0];
        if (fabsf(offsetX) < EDITOR_ARRIVAL_DISTANCE ||
            (g_CameraEditor.points[index][0] -
                g_CameraEditor.targets[index][0]) *
                    offsetX <
                0.0f) {
          isDone = TRUE;
        }

        break;
      case EDITOR_MOVE_AXIS_Y:
        /* The game takes the passing test with x rather than y */
        if (fabsf(y - g_CameraEditor.targets[index][1]) <
                EDITOR_ARRIVAL_DISTANCE ||
            (g_CameraEditor.points[index][1] -
                g_CameraEditor.targets[index][1]) *
                    (x - g_CameraEditor.targets[index][1]) <
                0.0f) {
          isDone = TRUE;
        }

        break;
      case EDITOR_MOVE_AXIS_Z:
        offsetZ = z - g_CameraEditor.targets[index][2];
        if (fabsf(offsetZ) < EDITOR_ARRIVAL_DISTANCE ||
            (g_CameraEditor.points[index][2] -
                g_CameraEditor.targets[index][2]) *
                    offsetZ <
                0.0f) {
          isDone = TRUE;
        }

        break;
    }

    if (isDone) {
      x = g_CameraEditor.targets[index][0];
      y = g_CameraEditor.targets[index][1];
      z = g_CameraEditor.targets[index][2];
      g_CameraEditor.pointVelocities[index][0] = 0.0f;
      g_CameraEditor.pointVelocities[index][1] = 0.0f;
      g_CameraEditor.pointVelocities[index][2] = 0.0f;
      g_CameraEditor.targets[index][0] = 0.0f;
      g_CameraEditor.targets[index][1] = 0.0f;
      g_CameraEditor.targets[index][2] = 0.0f;
      g_CameraEditor.pointBrakingDistances[index] = 0.0f;
      if (hasLeader) {
        g_CameraEditor.flags[index] ^= EDITOR_FLAG_CARRYING;
      } else {
        g_CameraEditor.flags[index] = 0;
      }

      g_CameraEditor.moveAxes[index] = 0;
    }

    g_CameraEditor.points[index][0] = x;
    g_CameraEditor.points[index][1] = y;
    g_CameraEditor.points[index][2] = z;
    if (hasLeader) {
      hasMoved = TRUE;
      break;
    }
  }

  /* The carried point keeps its place from the leader */
  if (hasLeader) {
    cosPitch = cosf(g_CameraEditor.pitch);
    y = sinf(g_CameraEditor.pitch);
    z = sinf(g_CameraEditor.heading) * cosPitch;
    x = cosPitch * cosf(g_CameraEditor.heading);
    distance = sqrtf((x * x) + (y * y) + (z * z));
    x /= distance;
    y /= distance;
    z /= distance;
    if (index == EDITOR_POINT_EYE) {
      x = -x;
      y = -y;
      z = -z;
    }

    g_CameraEditor.points[index ^ 1][0] =
        g_CameraEditor.points[index][0] + (x * g_CameraEditor.distance);
    g_CameraEditor.points[index ^ 1][1] =
        g_CameraEditor.points[index][1] + (y * g_CameraEditor.distance);
    g_CameraEditor.points[index ^ 1][2] =
        g_CameraEditor.points[index][2] + (z * g_CameraEditor.distance);
  }

  /* From here index is the point that moves around the pivot */
  if (g_CameraEditor.flags[EDITOR_POINT_EYE] & EDITOR_FLAG_PIVOT) {
    index = EDITOR_POINT_LOOK_AT;
    hasLeader = TRUE;
  }

  if (g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] & EDITOR_FLAG_PIVOT) {
    index = EDITOR_POINT_EYE;
    hasLeader = TRUE;
  }

  if (g_CameraEditor.flags[index] & EDITOR_FLAG_ZOOMING) {
    /* ZoomEditorCamera stores the change in distance, but this zooms until
     * the distance equals it */
    z = g_CameraEditor.distance;
    zoomChange = g_CameraEditor.unk6C;
    if (g_CameraEditor.zoomBrakingDistance != 0.0f) {
      gap = fabsf(zoomChange - z);
      if (gap < EDITOR_ARRIVAL_DISTANCE) {
        g_CameraEditor.distance = zoomChange;
      } else {
        g_CameraEditor.zoomSpeed += g_CameraEditor.zoomAcceleration;
        if (gap < g_CameraEditor.zoomBrakingDistance) {
          g_CameraEditor.zoomTopSpeed -=
              g_CameraEditor.zoomAcceleration * EDITOR_BRAKING_RATE;
          KEEP_TOP_SPEED(
              g_CameraEditor.zoomTopSpeed, g_CameraEditor.zoomAcceleration);
        }

        if (fabsf(g_CameraEditor.zoomTopSpeed) <
            fabsf(g_CameraEditor.zoomSpeed)) {
          g_CameraEditor.zoomSpeed = g_CameraEditor.zoomTopSpeed;
        }
      }
    }

    /* As in the game, gap is unset when there is no braking distance */
    g_CameraEditor.distance += g_CameraEditor.zoomSpeed;
    left = zoomChange - g_CameraEditor.distance;
    isDone = FALSE;
    if (gap <= sqrtf(left * left)) {
      isDone = TRUE;
    }

    if (isDone) {
      g_CameraEditor.distance = g_CameraEditor.unk6C;
      g_CameraEditor.zoomSpeed = 0.0f;
      g_CameraEditor.unk6C = 0.0f;
      g_CameraEditor.zoomBrakingDistance = 0.0f;
      g_CameraEditor.flags[index] ^= EDITOR_FLAG_ZOOMING;
    }

    if (g_CameraEditor.distance < EDITOR_MIN_DISTANCE) {
      g_CameraEditor.distance = EDITOR_MIN_DISTANCE;
    }

    hasMoved = TRUE;
  }

  if (g_CameraEditor.flags[index] & EDITOR_FLAG_TURNING) {
    left = g_CameraEditor.turnLeft;
    if (left != 0.0f) {
      if (fabsf(left) < EDITOR_ARRIVAL_ANGLE) {
        g_CameraEditor.heading += left;
      } else {
        g_CameraEditor.turnSpeed += g_CameraEditor.turnAcceleration;
        if (fabsf(left) < fabsf(g_CameraEditor.turnBrakingAngle)) {
          g_CameraEditor.turnTopSpeed -=
              g_CameraEditor.turnAcceleration * EDITOR_BRAKING_RATE;
          KEEP_TOP_SPEED(
              g_CameraEditor.turnTopSpeed, g_CameraEditor.turnAcceleration);
        }

        if (fabsf(g_CameraEditor.turnTopSpeed) <
            fabsf(g_CameraEditor.turnSpeed)) {
          g_CameraEditor.turnSpeed = g_CameraEditor.turnTopSpeed;
        }
      }
    }

    left = g_CameraEditor.turnLeft - g_CameraEditor.turnSpeed;
    isDone = TRUE;
    if (!(fabsf(g_CameraEditor.turnLeft) < fabsf(left))) {
      isDone = FALSE;
      g_CameraEditor.turnLeft = left;
      g_CameraEditor.heading += g_CameraEditor.turnSpeed;
    }

    if (fabsf(g_CameraEditor.turnLeft) < EDITOR_ARRIVAL_ANGLE) {
      isDone = TRUE;
    }

    if (isDone) {
      g_CameraEditor.heading += g_CameraEditor.turnLeft;
      g_CameraEditor.turnSpeed = 0.0f;
      g_CameraEditor.turnLeft = 0.0f;
      g_CameraEditor.turnBrakingAngle = 0.0f;
      g_CameraEditor.flags[index] ^= EDITOR_FLAG_TURNING;
    }

    hasMoved = TRUE;
  }

  if (g_CameraEditor.flags[index] & EDITOR_FLAG_TILTING) {
    left = g_CameraEditor.tiltLeft;
    if (left != 0.0f) {
      if (fabsf(left) < EDITOR_ARRIVAL_ANGLE) {
        g_CameraEditor.pitch += left;
      } else {
        g_CameraEditor.tiltSpeed += g_CameraEditor.tiltAcceleration;
        if (fabsf(left) < fabsf(g_CameraEditor.tiltBrakingAngle)) {
          g_CameraEditor.tiltTopSpeed -=
              g_CameraEditor.tiltAcceleration * EDITOR_BRAKING_RATE;
          KEEP_TOP_SPEED(
              g_CameraEditor.tiltTopSpeed, g_CameraEditor.tiltAcceleration);
        }

        if (fabsf(g_CameraEditor.tiltTopSpeed) <
            fabsf(g_CameraEditor.tiltSpeed)) {
          g_CameraEditor.tiltSpeed = g_CameraEditor.tiltTopSpeed;
        }
      }
    }

    left = g_CameraEditor.tiltLeft - g_CameraEditor.tiltSpeed;
    isDone = TRUE;
    if (!(fabsf(g_CameraEditor.tiltLeft) < fabsf(left))) {
      isDone = FALSE;
      g_CameraEditor.tiltLeft = left;
      g_CameraEditor.pitch += g_CameraEditor.tiltSpeed;
      if (g_CameraEditor.pitch > EDITOR_PITCH_LIMIT) {
        g_CameraEditor.pitch = EDITOR_PITCH_LIMIT;
        isDone = TRUE;
      }

      if (g_CameraEditor.pitch < -EDITOR_PITCH_LIMIT) {
        g_CameraEditor.pitch = -EDITOR_PITCH_LIMIT;
        isDone = TRUE;
      }
    }

    if (isDone) {
      g_CameraEditor.pitch += g_CameraEditor.tiltLeft;
      g_CameraEditor.tiltSpeed = 0.0f;
      g_CameraEditor.tiltLeft = 0.0f;
      g_CameraEditor.tiltBrakingAngle = 0.0f;
      g_CameraEditor.flags[index] ^= EDITOR_FLAG_TILTING;
    }

    hasMoved = TRUE;
  }

  /* The pivot is let go once nothing turns, tilts, zooms or carries */
  if (hasLeader &&
      (!(g_CameraEditor.flags[EDITOR_POINT_EYE] & EDITOR_FLAG_TURNING) &
          !(g_CameraEditor.flags[EDITOR_POINT_EYE] & EDITOR_FLAG_TILTING) &
          !(g_CameraEditor.flags[EDITOR_POINT_EYE] & EDITOR_FLAG_ZOOMING) &
          !(g_CameraEditor.flags[EDITOR_POINT_EYE] & EDITOR_FLAG_CARRYING) &
          !(g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] & EDITOR_FLAG_TURNING) &
          !(g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] & EDITOR_FLAG_TILTING) &
          !(g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] & EDITOR_FLAG_ZOOMING) &
          !(g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] &
              EDITOR_FLAG_CARRYING))) {
    if (g_CameraEditor.flags[EDITOR_POINT_EYE] & EDITOR_FLAG_PIVOT) {
      g_CameraEditor.flags[EDITOR_POINT_EYE] = 0;
    }

    if (g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] & EDITOR_FLAG_PIVOT) {
      g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] = 0;
    }
  }

  if (hasMoved) {
    cosPitch = cosf(g_CameraEditor.pitch);
    y = sinf(g_CameraEditor.pitch);
    z = sinf(g_CameraEditor.heading) * cosPitch;
    x = cosPitch * cosf(g_CameraEditor.heading);
    distance = sqrtf((x * x) + (y * y) + (z * z));
    x /= distance;
    y /= distance;
    z /= distance;
    if (index == EDITOR_POINT_EYE) {
      x = -x;
      y = -y;
      z = -z;
    }

    g_CameraEditor.points[index][0] =
        g_CameraEditor.points[index ^ 1][0] + (x * g_CameraEditor.distance);
    g_CameraEditor.points[index][1] =
        g_CameraEditor.points[index ^ 1][1] + (y * g_CameraEditor.distance);
    g_CameraEditor.points[index][2] =
        g_CameraEditor.points[index ^ 1][2] + (z * g_CameraEditor.distance);
  }

  MeasureEditorCamera();
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", UpdateCameraEditor);
#endif

/* The controllers that EditOpeningCamera reads */
#define EDITOR_PAD 0
#define SECOND_EDITOR_PAD 2

#define EDITOR_SOUND_CHANNEL 15
#define EDITOR_CHOICE_SOUND 8
#define SNAPSHOT_CURSOR_SOUND 0x5B
#define SNAPSHOT_SAVE_SOUND 0x62
/* Also for deleting a snapshot */
#define SNAPSHOT_LOAD_SOUND 0x46

/* The snapshots are shown as two rows of this many */
#define SNAPSHOT_ROW_LENGTH 16

/* Where EditOpeningCamera prints, in debug text cells */
#define EDITOR_MENU_COLUMN 4
#define EDITOR_MENU_TITLE_ROW 2
#define EDITOR_MENU_FIRST_ROW 4
#define CAMERA_PANEL_COLUMN 24
#define CAMERA_PANEL_LABEL_COLUMN 26
#define CAMERA_PANEL_TITLE_ROW 6
/* The values follow each label, one to a row */
#define LOOK_AT_LABEL_ROW 7
#define EYE_LABEL_ROW 11
#define ANGLE_LABEL_ROW 15
#define DISTANCE_LABEL_ROW 18
#define FIELD_OF_VIEW_LABEL_ROW 20
#define SNAPSHOT_COLUMN 19
/* Each row of snapshots has the cursor's row under it */
#define FIRST_SNAPSHOT_ROW 2
#define SECOND_SNAPSHOT_ROW 4

/* The first controller's stick slides slowly unless Z is held */
#define SLOW_SLIDE_SCALE 0.2f
#define SLOW_HEIGHT_STEP 0.5f
#define FAST_HEIGHT_STEP 10.0f
#define SLOW_FIELD_OF_VIEW_STEP 0.1f
#define FAST_FIELD_OF_VIEW_STEP 1.0f
#define MIN_FIELD_OF_VIEW 10.0f
#define MAX_FIELD_OF_VIEW 170.0f
#define SLOW_MARKER_SCALE_STEP 0.05f
#define FAST_MARKER_SCALE_STEP 0.5f
#define MIN_MARKER_SCALE 0.05f
#define MAX_MARKER_SCALE 10.0f
/* R moves the eye away from the look-at point, L toward it */
#define ZOOM_OUT_SCALE 1.1f
#define ZOOM_IN_SCALE 0.9f
#define MAX_ZOOM_OUT_DISTANCE 5000.0f
#define MIN_ZOOM_IN_DISTANCE 1.0f
/* The second controller's steps, larger while Start is held */
#define SLOW_PAD_STEP 1.0f
#define FAST_PAD_STEP 20.0f

/* Moves the eye along its line to the look-at point, scaling their distance */
#define ZOOM_EDITOR_EYE(scale) \
  do { \
    g_CameraEditor.points[EDITOR_POINT_EYE][0] = \
        ((g_CameraEditor.points[EDITOR_POINT_EYE][0] - \
             g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0]) * \
            (scale)) + \
        g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0]; \
    g_CameraEditor.points[EDITOR_POINT_EYE][1] = \
        ((g_CameraEditor.points[EDITOR_POINT_EYE][1] - \
             g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1]) * \
            (scale)) + \
        g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1]; \
    g_CameraEditor.points[EDITOR_POINT_EYE][2] = \
        ((g_CameraEditor.points[EDITOR_POINT_EYE][2] - \
             g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2]) * \
            (scale)) + \
        g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2]; \
  } while (0)

/* Steps one coordinate of a point while the second controller holds a
 * button; step is += or -= */
#define STEP_EDITOR_COORDINATE(button, point, axis, step) \
  do { \
    if (g_ControllerStates[SECOND_EDITOR_PAD].buttons & (button)) { \
      if (g_ControllerStates[SECOND_EDITOR_PAD].buttons & START_BUTTON) { \
        g_CameraEditor.points[point][axis] step FAST_PAD_STEP; \
        /* The empty do-while is there to match */ \
        do { \
        } while (0); \
      } else { \
        g_CameraEditor.points[point][axis] step SLOW_PAD_STEP; \
      } \
    } \
  } while (0)

/* The control pad moves a point across the ground and the C buttons up and
 * down */
#define STEP_EDITOR_POINT(point) \
  do { \
    STEP_EDITOR_COORDINATE(R_JPAD, point, 0, +=); \
    STEP_EDITOR_COORDINATE(L_JPAD, point, 0, -=); \
    STEP_EDITOR_COORDINATE(U_JPAD, point, 2, +=); \
    STEP_EDITOR_COORDINATE(D_JPAD, point, 2, -=); \
    STEP_EDITOR_COORDINATE(U_CBUTTONS, point, 1, -=); \
    STEP_EDITOR_COORDINATE(D_CBUTTONS, point, 1, +=); \
  } while (0)

/*
 * The opening's camera editor screen. The first controller slides or moves
 * the camera's points, zooms, and keeps snapshots of the camera; the second
 * moves one point in steps and writes or reads the snapshots as host files.
 * The camera's points and angles are shown as it goes.
 */
void EditOpeningCamera(void) {
  Vec3 offset;
  u32 fileSize;
  s32 i;

  MeasureEditorCamera();
  PrintDebugText("<< ACTIVE >>", EDITOR_MENU_COLUMN, EDITOR_MENU_TITLE_ROW);
  PrintDebugText(" SLIDE", EDITOR_MENU_COLUMN,
      EDITOR_MENU_FIRST_ROW + EDITOR_CHOICE_SLIDE);
  PrintDebugText(" TARGET", EDITOR_MENU_COLUMN,
      EDITOR_MENU_FIRST_ROW + EDITOR_CHOICE_TARGET);
  PrintDebugText(" POSITION", EDITOR_MENU_COLUMN,
      EDITOR_MENU_FIRST_ROW + EDITOR_CHOICE_POSITION);
  PrintDebugText(" FROV", EDITOR_MENU_COLUMN,
      EDITOR_MENU_FIRST_ROW + EDITOR_CHOICE_FIELD_OF_VIEW);
  SetDebugTextHighlight(TRUE);
  PrintDebugText(
      ">", EDITOR_MENU_COLUMN, EDITOR_MENU_FIRST_ROW + f_EditorChoice);
  if (g_ControllerStates[EDITOR_PAD].pressedButtons & D_JPAD) {
    PlaySound(EDITOR_CHOICE_SOUND, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
    f_EditorChoice++;
    if (f_EditorChoice >= EDITOR_CHOICE_COUNT) {
      f_EditorChoice = 0;
    }
  }

  if (g_ControllerStates[EDITOR_PAD].pressedButtons & U_JPAD) {
    PlaySound(EDITOR_CHOICE_SOUND, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
    f_EditorChoice--;
    if (f_EditorChoice < 0) {
      f_EditorChoice = EDITOR_CHOICE_COUNT - 1;
    }
  }

  PrintDebugText("<< POSITION >>", CAMERA_PANEL_COLUMN, CAMERA_PANEL_TITLE_ROW);
  SetDebugTextHighlight(TRUE);
  PrintDebugText("[TRG]", CAMERA_PANEL_LABEL_COLUMN, LOOK_AT_LABEL_ROW);
  sprintf(
      g_PrintBuffer, "  %f", g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0]);
  PrintDebugText(g_PrintBuffer, CAMERA_PANEL_COLUMN, LOOK_AT_LABEL_ROW + 1);
  sprintf(
      g_PrintBuffer, "  %f", g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1]);
  PrintDebugText(g_PrintBuffer, CAMERA_PANEL_COLUMN, LOOK_AT_LABEL_ROW + 2);
  sprintf(
      g_PrintBuffer, "  %f", g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2]);
  PrintDebugText(g_PrintBuffer, CAMERA_PANEL_COLUMN, LOOK_AT_LABEL_ROW + 3);
  SetDebugTextHighlight(TRUE);
  PrintDebugText("[POS]", CAMERA_PANEL_LABEL_COLUMN, EYE_LABEL_ROW);
  sprintf(g_PrintBuffer, "  %f", g_CameraEditor.points[EDITOR_POINT_EYE][0]);
  PrintDebugText(g_PrintBuffer, CAMERA_PANEL_COLUMN, EYE_LABEL_ROW + 1);
  sprintf(g_PrintBuffer, "  %f", g_CameraEditor.points[EDITOR_POINT_EYE][1]);
  PrintDebugText(g_PrintBuffer, CAMERA_PANEL_COLUMN, EYE_LABEL_ROW + 2);
  sprintf(g_PrintBuffer, "  %f", g_CameraEditor.points[EDITOR_POINT_EYE][2]);
  PrintDebugText(g_PrintBuffer, CAMERA_PANEL_COLUMN, EYE_LABEL_ROW + 3);
  SetDebugTextHighlight(TRUE);
  PrintDebugText("[ANGLE] (X,Y)", CAMERA_PANEL_COLUMN, ANGLE_LABEL_ROW);
  sprintf(g_PrintBuffer, "  %f", g_CameraEditor.pitch);
  PrintDebugText(g_PrintBuffer, CAMERA_PANEL_COLUMN, ANGLE_LABEL_ROW + 1);
  sprintf(g_PrintBuffer, "  %f", g_CameraEditor.heading);
  PrintDebugText(g_PrintBuffer, CAMERA_PANEL_COLUMN, ANGLE_LABEL_ROW + 2);
  SetDebugTextHighlight(TRUE);
  PrintDebugText("[ZOOM]", CAMERA_PANEL_LABEL_COLUMN, DISTANCE_LABEL_ROW);
  sprintf(g_PrintBuffer, "  %f", g_CameraEditor.distance);
  PrintDebugText(g_PrintBuffer, CAMERA_PANEL_COLUMN, DISTANCE_LABEL_ROW + 1);
  SetDebugTextHighlight(TRUE);
  PrintDebugText("[FORV]", CAMERA_PANEL_LABEL_COLUMN, FIELD_OF_VIEW_LABEL_ROW);
  sprintf(g_PrintBuffer, "  %f", g_CameraEditor.fieldOfView);
  PrintDebugText(
      g_PrintBuffer, CAMERA_PANEL_COLUMN, FIELD_OF_VIEW_LABEL_ROW + 1);

  /* The stick moves across the ground as seen along the heading */
  offset[0] = 0.0f;
  offset[1] = 0.0f;
  offset[2] = 0.0f;
  if (f_EditorChoice == EDITOR_CHOICE_SLIDE) {
    if (*(u16*)&g_ControllerStates[EDITOR_PAD].stickX != 0) {
      offset[0] = (cos(g_CameraEditor.heading) *
                      g_ControllerStates[EDITOR_PAD].stickY) +
          (sin(g_CameraEditor.heading) * g_ControllerStates[EDITOR_PAD].stickX);
      offset[2] = (sin(g_CameraEditor.heading) *
                      g_ControllerStates[EDITOR_PAD].stickY) -
          (cos(g_CameraEditor.heading) * g_ControllerStates[EDITOR_PAD].stickX);
      if (!(g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG)) {
        offset[0] *= SLOW_SLIDE_SCALE;
        offset[2] *= SLOW_SLIDE_SCALE;
      }

      ShiftEditorCamera(offset);
    }

    if (g_ControllerStates[EDITOR_PAD].buttons & U_CBUTTONS) {
      if (g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG) {
        offset[1] = -FAST_HEIGHT_STEP;
      } else {
        offset[1] = -SLOW_HEIGHT_STEP;
      }

      ShiftEditorCamera(offset);
    }

    if (g_ControllerStates[EDITOR_PAD].buttons & D_CBUTTONS) {
      if (g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG) {
        offset[1] = FAST_HEIGHT_STEP;
      } else {
        offset[1] = SLOW_HEIGHT_STEP;
      }

      ShiftEditorCamera(offset);
    }
  }

  if (f_EditorChoice == EDITOR_CHOICE_TARGET) {
    if (*(u16*)&g_ControllerStates[EDITOR_PAD].stickX != 0) {
      offset[0] = (cos(g_CameraEditor.heading) *
                      g_ControllerStates[EDITOR_PAD].stickY) +
          (sin(g_CameraEditor.heading) * g_ControllerStates[EDITOR_PAD].stickX);
      offset[2] = (sin(g_CameraEditor.heading) *
                      g_ControllerStates[EDITOR_PAD].stickY) -
          (cos(g_CameraEditor.heading) * g_ControllerStates[EDITOR_PAD].stickX);
      if (!(g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG)) {
        offset[0] *= SLOW_SLIDE_SCALE;
        offset[2] *= SLOW_SLIDE_SCALE;
      }

      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] += offset[0];
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] += offset[2];
    }

    if (g_ControllerStates[EDITOR_PAD].buttons & U_CBUTTONS) {
      if (g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG) {
        offset[1] = -FAST_HEIGHT_STEP;
      } else {
        offset[1] = -SLOW_HEIGHT_STEP;
      }

      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] += offset[1];
    }

    if (g_ControllerStates[EDITOR_PAD].buttons & D_CBUTTONS) {
      if (g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG) {
        offset[1] = FAST_HEIGHT_STEP;
      } else {
        offset[1] = SLOW_HEIGHT_STEP;
      }

      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] += offset[1];
    }
  }

  if (f_EditorChoice == EDITOR_CHOICE_POSITION) {
    if (*(u16*)&g_ControllerStates[EDITOR_PAD].stickX != 0) {
      offset[0] = (cos(g_CameraEditor.heading) *
                      g_ControllerStates[EDITOR_PAD].stickY) +
          (sin(g_CameraEditor.heading) * g_ControllerStates[EDITOR_PAD].stickX);
      offset[2] = (sin(g_CameraEditor.heading) *
                      g_ControllerStates[EDITOR_PAD].stickY) -
          (cos(g_CameraEditor.heading) * g_ControllerStates[EDITOR_PAD].stickX);
      if (!(g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG)) {
        offset[0] *= SLOW_SLIDE_SCALE;
        offset[2] *= SLOW_SLIDE_SCALE;
      }

      g_CameraEditor.points[EDITOR_POINT_EYE][0] += offset[0];
      g_CameraEditor.points[EDITOR_POINT_EYE][2] += offset[2];
    }

    if (g_ControllerStates[EDITOR_PAD].buttons & U_CBUTTONS) {
      if (g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG) {
        offset[1] = -FAST_HEIGHT_STEP;
      } else {
        offset[1] = -SLOW_HEIGHT_STEP;
      }

      g_CameraEditor.points[EDITOR_POINT_EYE][1] += offset[1];
    }

    if (g_ControllerStates[EDITOR_PAD].buttons & D_CBUTTONS) {
      if (g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG) {
        offset[1] = FAST_HEIGHT_STEP;
      } else {
        offset[1] = SLOW_HEIGHT_STEP;
      }

      g_CameraEditor.points[EDITOR_POINT_EYE][1] += offset[1];
    }
  }

  if (f_EditorChoice == EDITOR_CHOICE_FIELD_OF_VIEW) {
    if (g_ControllerStates[EDITOR_PAD].buttons & U_CBUTTONS) {
      if (g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG) {
        g_CameraEditor.fieldOfView -= FAST_FIELD_OF_VIEW_STEP;
      } else {
        g_CameraEditor.fieldOfView -= SLOW_FIELD_OF_VIEW_STEP;
      }

      if (g_CameraEditor.fieldOfView < MIN_FIELD_OF_VIEW) {
        g_CameraEditor.fieldOfView = MIN_FIELD_OF_VIEW;
      }
    }

    if (g_ControllerStates[EDITOR_PAD].buttons & D_CBUTTONS) {
      if (g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG) {
        g_CameraEditor.fieldOfView += FAST_FIELD_OF_VIEW_STEP;
      } else {
        g_CameraEditor.fieldOfView += SLOW_FIELD_OF_VIEW_STEP;
      }

      if (g_CameraEditor.fieldOfView > MAX_FIELD_OF_VIEW) {
        g_CameraEditor.fieldOfView = MAX_FIELD_OF_VIEW;
      }
    }

    if (g_ControllerStates[EDITOR_PAD].buttons & L_CBUTTONS) {
      if (g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG) {
        g_LookAtMarkerScale -= FAST_MARKER_SCALE_STEP;
      } else {
        g_LookAtMarkerScale -= SLOW_MARKER_SCALE_STEP;
      }

      if (g_LookAtMarkerScale < MIN_MARKER_SCALE) {
        g_LookAtMarkerScale = MIN_MARKER_SCALE;
      }
    }

    if (g_ControllerStates[EDITOR_PAD].buttons & R_CBUTTONS) {
      if (g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG) {
        g_LookAtMarkerScale += FAST_MARKER_SCALE_STEP;
      } else {
        g_LookAtMarkerScale += SLOW_MARKER_SCALE_STEP;
      }

      if (g_LookAtMarkerScale > MAX_MARKER_SCALE) {
        g_LookAtMarkerScale = MAX_MARKER_SCALE;
      }
    }
  }

  /* R zooms out, and L or Z with R zooms in */
  if ((g_ControllerStates[EDITOR_PAD].buttons & (Z_TRIG | R_TRIG)) == R_TRIG) {
    if (g_CameraEditor.distance < MAX_ZOOM_OUT_DISTANCE) {
      ZOOM_EDITOR_EYE(ZOOM_OUT_SCALE);
    }
  }

  if ((g_ControllerStates[EDITOR_PAD].buttons & L_TRIG) ||
      (g_ControllerStates[EDITOR_PAD].buttons & (Z_TRIG | R_TRIG)) ==
          (Z_TRIG | R_TRIG)) {
    if (g_CameraEditor.distance > MIN_ZOOM_IN_DISTANCE) {
      ZOOM_EDITOR_EYE(ZOOM_IN_SCALE);
    }
  }

  /* A star for each snapshot, highlighted once saved */
  for (i = 0; i != SNAPSHOT_COUNT; i++) {
    if (i < SNAPSHOT_ROW_LENGTH) {
      if (f_IsSnapshotSaved[i]) {
        SetDebugTextHighlight(TRUE);
        PrintDebugText("*", SNAPSHOT_COLUMN + i, FIRST_SNAPSHOT_ROW);
      } else {
        PrintDebugText("*", SNAPSHOT_COLUMN + i, FIRST_SNAPSHOT_ROW);
      }
    } else {
      if (f_IsSnapshotSaved[i]) {
        SetDebugTextHighlight(TRUE);
        PrintDebugText("*", SNAPSHOT_COLUMN + i - SNAPSHOT_ROW_LENGTH,
            SECOND_SNAPSHOT_ROW);
      } else {
        PrintDebugText("*", SNAPSHOT_COLUMN + i - SNAPSHOT_ROW_LENGTH,
            SECOND_SNAPSHOT_ROW);
      }
    }
  }

  PrintDebugText("                ", SNAPSHOT_COLUMN, FIRST_SNAPSHOT_ROW + 1);
  PrintDebugText("                ", SNAPSHOT_COLUMN, SECOND_SNAPSHOT_ROW + 1);
  SetDebugTextHighlight(TRUE);
  if (f_SnapshotIndex < SNAPSHOT_ROW_LENGTH) {
    PrintDebugText(
        "^", SNAPSHOT_COLUMN + f_SnapshotIndex, FIRST_SNAPSHOT_ROW + 1);
  } else {
    PrintDebugText("^", SNAPSHOT_COLUMN + f_SnapshotIndex - SNAPSHOT_ROW_LENGTH,
        SECOND_SNAPSHOT_ROW + 1);
  }

  /* The control pad chooses a snapshot while the stick is at rest */
  if ((g_ControllerStates[EDITOR_PAD].repeatedButtons & R_JPAD) &&
      *(u16*)&g_ControllerStates[EDITOR_PAD].stickX == 0) {
    PlaySound(SNAPSHOT_CURSOR_SOUND, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
    f_SnapshotIndex++;
    if (f_SnapshotIndex >= SNAPSHOT_COUNT) {
      f_SnapshotIndex = 0;
    }
  }

  if ((g_ControllerStates[EDITOR_PAD].repeatedButtons & L_JPAD) &&
      *(u16*)&g_ControllerStates[EDITOR_PAD].stickX == 0) {
    PlaySound(SNAPSHOT_CURSOR_SOUND, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
    f_SnapshotIndex--;
    if (f_SnapshotIndex < 0) {
      f_SnapshotIndex = SNAPSHOT_COUNT - 1;
    }
  }

  /* A saves the camera, B loads it, and B with Start deletes the snapshot.
   * A saves over a snapshot only while Start is held. */
  if (g_ControllerStates[EDITOR_PAD].pressedButtons & A_BUTTON) {
    if (f_IsSnapshotSaved[f_SnapshotIndex] &&
        !(g_ControllerStates[EDITOR_PAD].buttons & START_BUTTON)) {
      PlaySound(SOUND_MENU_BLOCKED, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
    } else {
      PlaySound(SNAPSHOT_SAVE_SOUND, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
      f_IsSnapshotSaved[f_SnapshotIndex] = TRUE;
      f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_LOOK_AT][0] =
          g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0];
      f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_LOOK_AT][1] =
          g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1];
      f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_LOOK_AT][2] =
          g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2];
      f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_EYE][0] =
          g_CameraEditor.points[EDITOR_POINT_EYE][0];
      f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_EYE][1] =
          g_CameraEditor.points[EDITOR_POINT_EYE][1];
      f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_EYE][2] =
          g_CameraEditor.points[EDITOR_POINT_EYE][2];
    }
  }

  if ((g_ControllerStates[EDITOR_PAD].pressedButtons & B_BUTTON) &&
      !(g_ControllerStates[EDITOR_PAD].buttons & START_BUTTON)) {
    if (f_IsSnapshotSaved[f_SnapshotIndex]) {
      PlaySound(SNAPSHOT_LOAD_SOUND, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] =
          f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_LOOK_AT][0];
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] =
          f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_LOOK_AT][1];
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] =
          f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_LOOK_AT][2];
      g_CameraEditor.points[EDITOR_POINT_EYE][0] =
          f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_EYE][0];
      g_CameraEditor.points[EDITOR_POINT_EYE][1] =
          f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_EYE][1];
      g_CameraEditor.points[EDITOR_POINT_EYE][2] =
          f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_EYE][2];
    } else {
      PlaySound(SOUND_MENU_BLOCKED, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
    }
  }

  if ((g_ControllerStates[EDITOR_PAD].buttons & START_BUTTON) &&
      (g_ControllerStates[EDITOR_PAD].pressedButtons & B_BUTTON)) {
    if (f_IsSnapshotSaved[f_SnapshotIndex]) {
      PlaySound(SNAPSHOT_LOAD_SOUND, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
      f_IsSnapshotSaved[f_SnapshotIndex] = FALSE;
    } else {
      PlaySound(SOUND_MENU_BLOCKED, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
    }
  }

  /* On the second controller, with Z and Start held, L writes the snapshots
   * and R reads them */
  if ((g_ControllerStates[SECOND_EDITOR_PAD].buttons &
          (Z_TRIG | START_BUTTON)) == (Z_TRIG | START_BUTTON) &&
      (g_ControllerStates[SECOND_EDITOR_PAD].pressedButtons & L_TRIG)) {
    sprintf(g_PrintBuffer, "opening/snapshot.pos");
    WriteHostFile(g_PrintBuffer, sizeof(f_CameraSnapshots));
    sprintf(g_PrintBuffer, "opening/snapshot.flg");
    WriteHostFile(g_PrintBuffer, sizeof(f_IsSnapshotSaved));
  }

  if ((g_ControllerStates[SECOND_EDITOR_PAD].buttons &
          (Z_TRIG | START_BUTTON)) == (Z_TRIG | START_BUTTON) &&
      (g_ControllerStates[SECOND_EDITOR_PAD].pressedButtons & R_TRIG)) {
    sprintf(g_PrintBuffer, "opening/snapshot.pos");
    ReadHostFile(g_PrintBuffer, &fileSize);
    sprintf(g_PrintBuffer, "opening/snapshot.flg");
    ReadHostFile(g_PrintBuffer, &fileSize);
  }

  /* C left or right switches the point the second controller moves, marked
   * by ? beside its values */
  if ((g_ControllerStates[SECOND_EDITOR_PAD].pressedButtons & R_CBUTTONS) ||
      (g_ControllerStates[SECOND_EDITOR_PAD].pressedButtons & L_CBUTTONS)) {
    f_IsSecondPadOnEye ^= TRUE;
  }

  if (!f_IsSecondPadOnEye) {
    SetDebugTextHighlight(TRUE);
    PrintDebugText("?", CAMERA_PANEL_COLUMN, LOOK_AT_LABEL_ROW + 1);
    SetDebugTextHighlight(TRUE);
    PrintDebugText("?", CAMERA_PANEL_COLUMN, LOOK_AT_LABEL_ROW + 2);
    SetDebugTextHighlight(TRUE);
    PrintDebugText("?", CAMERA_PANEL_COLUMN, LOOK_AT_LABEL_ROW + 3);
  } else {
    SetDebugTextHighlight(TRUE);
    PrintDebugText("?", CAMERA_PANEL_COLUMN, EYE_LABEL_ROW + 1);
    SetDebugTextHighlight(TRUE);
    PrintDebugText("?", CAMERA_PANEL_COLUMN, EYE_LABEL_ROW + 2);
    SetDebugTextHighlight(TRUE);
    PrintDebugText("?", CAMERA_PANEL_COLUMN, EYE_LABEL_ROW + 3);
  }

  if (!f_IsSecondPadOnEye) {
    STEP_EDITOR_POINT(EDITOR_POINT_LOOK_AT);
  } else {
    STEP_EDITOR_POINT(EDITOR_POINT_EYE);
  }
}

/* Moves the whole camera by offset */
void ShiftEditorCamera(Vec3 offset) {
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] += offset[0];
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] += offset[1];
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] += offset[2];
  g_CameraEditor.points[EDITOR_POINT_EYE][0] += offset[0];
  g_CameraEditor.points[EDITOR_POINT_EYE][1] += offset[1];
  g_CameraEditor.points[EDITOR_POINT_EYE][2] += offset[2];
}

/* Nothing refers to this */
const char D_8020C18C_ovl_05[] = "";
