/*
 * The camera editor that moves the opening's camera: its eye and look-at point
 * move, turn, tilt and zoom at set speeds.
 */
#include "common.h"

#include "camera_editor.h"
#include "math_util.h"

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
s32 AreEditorMovesDone(void) {
  if (g_CameraEditor.flags[0] == 0 && g_CameraEditor.flags[1] == 0) {
    return TRUE;
  }

  return FALSE;
}

/* Returns whether neither point has any of the EDITOR_FLAG_* bits in mask */
s32 AreEditorFlagsClear(s32 mask) {
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
  s32 hasLeader = FALSE;
  s32 hasMoved = FALSE;
  s32 index;
  s32 isDone;
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

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", EditOpeningCamera);

/* Moves the whole camera by offset */
void func_802065FC_ovl_05(Vec3 offset) {
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] += offset[0];
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] += offset[1];
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] += offset[2];
  g_CameraEditor.points[EDITOR_POINT_EYE][0] += offset[0];
  g_CameraEditor.points[EDITOR_POINT_EYE][1] += offset[1];
  g_CameraEditor.points[EDITOR_POINT_EYE][2] += offset[2];
}

/* Ground kind names in half-width katakana; the data block points at the last
 * four */
const char D_8020C190_ovl_05[] = "ﾚｯﾄﾞ";
const char D_8020C198_ovl_05[] = "ﾌﾞﾙｰ";
const char D_8020C1A0_ovl_05[] = "ｸﾞﾘｰﾝ";
const char D_8020C1A8_ovl_05[] = "TEE UP";
const char D_8020C1B0_ovl_05[] = "ｱｳﾄ ｵﾌﾞ ｺｰｽ";
const char D_8020C1BC_ovl_05[] = "            ";
const char D_8020C1CC_ovl_05[] = "ｳｴｽﾄｴﾘｱ";
const char D_8020C1D4_ovl_05[] = "ﾃﾞｨｰﾌﾟﾗﾌ";
const char D_8020C1E0_ovl_05[] = "ﾍﾞｱｸﾞﾗｳﾝﾄﾞ";
const char D_8020C1EC_ovl_05[] = "ｾﾐﾗﾌ";
const char D_8020C1F4_ovl_05[] = "ｸﾞﾘｰﾝ ｴｯｼﾞ";
const char D_8020C200_ovl_05[] = "ﾛｯｸ";
const char D_8020C204_ovl_05[] = "ﾃｨｰｸﾞﾗｳﾝﾄﾞ";
const char D_8020C210_ovl_05[] = "ｶｰﾄｳｪｲ";
const char D_8020C218_ovl_05[] = "ｳｫｰﾀｰﾊｻﾞｰﾄﾞ";
const char D_8020C224_ovl_05[] = "ｵﾝ ｸﾞﾘｰﾝ";
const char D_8020C230_ovl_05[] = "ﾊﾞﾝｶｰ";
const char D_8020C238_ovl_05[] = "ﾗﾌ";
const char D_8020C23C_ovl_05[] = "ﾌｪｱｳｪｲ";
const char D_8020C244_ovl_05[] = "O.B.";
