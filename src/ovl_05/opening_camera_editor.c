/*
 * The camera editor that moves the opening's camera: its eye and look-at point
 * move, turn, tilt and zoom at set speeds.
 */
#include <ultra64.h>

#include "camera_editor.h"
#include "compiler.h"
#include "controller.h"
#include "debug_text.h"
#include "geometry.h"
#include "host_file.h"
#include "include_asm.h"
#include "language.h"
#include "libc.h"
#include "math_util.h"
#include "sound.h"
#include "text.h"

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
  g_CameraEditor.turnTopSpeed = DegreesToRadians(6.0f);
  g_CameraEditor.turnAcceleration = DegreesToRadians(2.0f);
  g_CameraEditor.tiltTopSpeed = DegreesToRadians(3.0f);
  g_CameraEditor.tiltAcceleration = DegreesToRadians(1.0f);
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
void SetEditorTurnSpeeds(
    s32 unusedIndex UNUSED, f32 topSpeed, f32 acceleration) {
  g_CameraEditor.turnTopSpeed = topSpeed;
  g_CameraEditor.turnAcceleration = acceleration;
}

/* Sets the top speed and acceleration of tilts. Nothing calls it */
void SetEditorTiltSpeeds(
    s32 unusedIndex UNUSED, f32 topSpeed, f32 acceleration) {
  g_CameraEditor.tiltTopSpeed = topSpeed;
  g_CameraEditor.tiltAcceleration = acceleration;
}

/* Sets the top speed and acceleration of zooms. Nothing calls it */
void SetEditorZoomSpeeds(
    s32 unusedIndex UNUSED, f32 topSpeed, f32 acceleration) {
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

/* Within these, a move, turn, tilt or zoom has arrived */
#define EDITOR_ARRIVAL_DISTANCE 0.1f
#define EDITOR_ARRIVAL_ANGLE 0.001f
/* A timed move speeds up over this share of its frames and brakes over the
 * same share of its distance */
#define EDITOR_EASE_SHARE 4

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
  if (distance < EDITOR_ARRIVAL_DISTANCE) {
    g_CameraEditor.points[index][0] = x;
    g_CameraEditor.points[index][1] = y;
    g_CameraEditor.points[index][2] = z;
    return;
  }

  if (frameCount != 0) {
    easeFrameCount = frameCount / EDITOR_EASE_SHARE;
    speed = distance / (frameCount - easeFrameCount);
    g_CameraEditor.pointTopSpeeds[index] = speed;
    g_CameraEditor.pointAccelerations[index] = speed / (easeFrameCount + 1);
    g_CameraEditor.pointBrakingDistances[index] =
        distance * (1.0 / EDITOR_EASE_SHARE);
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

  /* The arguments become the offsets, and x the larger of the ground ones.
   * The magnitudes come from fabs in double precision, which GCC narrows to
   * abs.s: fabsf gives 28 differing words, and fabs without the casts 18 */
  x -= g_CameraEditor.points[index][0];
  y -= g_CameraEditor.points[index][1];
  z -= g_CameraEditor.points[index][2];
  g_CameraEditor.moveAxes[index] = EDITOR_MOVE_AXIS_X;
  if ((f32)fabs(x) < (f32)fabs(z)) {
    g_CameraEditor.moveAxes[index] = EDITOR_MOVE_AXIS_Z;
    x = z;
  }

  if ((f32)fabs(x) < (f32)fabs(y)) {
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
void TurnEditorCamera(s32 index, Radians angle, s32 frameCount) {
  s32 easeFrameCount;
  f32 speed;

  if (fabsf(angle) < EDITOR_ARRIVAL_ANGLE) {
    g_CameraEditor.heading += angle;
    return;
  }

  if (frameCount != 0) {
    easeFrameCount = frameCount / EDITOR_EASE_SHARE;
    speed = angle / (frameCount - easeFrameCount);
    g_CameraEditor.turnBrakingAngle = angle * (1.0 / EDITOR_EASE_SHARE);
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

/* Starts tilting the camera's pitch by angle over frameCount frames, with the
 * other point swinging around point index, or tilts it at once when angle is
 * tiny */
void TiltEditorCamera(s32 index, Radians angle, s32 frameCount) {
  s32 easeFrameCount;
  f32 speed;

  if (fabsf(angle) < EDITOR_ARRIVAL_ANGLE) {
    g_CameraEditor.pitch += angle;
    return;
  }

  if (frameCount != 0) {
    easeFrameCount = frameCount / EDITOR_EASE_SHARE;
    speed = angle / (frameCount - easeFrameCount);
    g_CameraEditor.tiltBrakingAngle = angle * (1.0 / EDITOR_EASE_SHARE);
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

/* Starts moving the other point to distance from point index over frameCount
 * frames, or moves it at once when it is close */
void ZoomEditorCamera(s32 index, f32 distance, s32 frameCount) {
  s32 easeFrameCount;
  f32 change;
  f32 speed;

  change = distance - g_CameraEditor.distance;
  if (fabsf(change) < EDITOR_ARRIVAL_DISTANCE) {
    g_CameraEditor.distance = distance;
    return;
  }

  if (frameCount != 0) {
    easeFrameCount = frameCount / EDITOR_EASE_SHARE;
    g_CameraEditor.zoomTopSpeed = change / (frameCount - easeFrameCount);
    speed = g_CameraEditor.zoomTopSpeed;
    g_CameraEditor.zoomAcceleration = speed / (easeFrameCount + 1);
    /* The braking distance in double precision, to match; a float quarter or
     * a division by 4 gives 9 differing words */
    g_CameraEditor.zoomBrakingDistance =
        fabsf(change) * (1.0 / EDITOR_EASE_SHARE);
  }

  g_CameraEditor.zoomTarget = distance;
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

/* Unreferenced; left by debug output that ZoomEditorCamera no longer has */
static const char f_UnusedIntegerFormat[] = "%d  ";
static const char f_UnusedFloatFormat[] = "%f";

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
  g_CameraEditor.turnTopSpeed = DegreesToRadians(6.0f);
  g_CameraEditor.turnAcceleration = DegreesToRadians(2.0f);
  g_CameraEditor.tiltTopSpeed = DegreesToRadians(3.0f);
  g_CameraEditor.tiltAcceleration = DegreesToRadians(1.0f);
  g_CameraEditor.zoomTopSpeed = 10.0f;
  g_CameraEditor.zoomAcceleration = 1.0f;
  MeasureEditorCamera();
}

/* The share of the acceleration that braking takes off the top speed each
 * update */
#define EDITOR_BRAKING_RATE 0.63979524f
/* The pitch stays within this of level, in radians */
#define EDITOR_PITCH_LIMIT 1.5f
/* The closest a zoom brings the eye to the look-at point */
#define EDITOR_MIN_DISTANCE 1.0f

/*
 * Braking lowers a top speed no further than the acceleration. A do-while,
 * as with a plain block UpdateCameraEditor gives 347 words differing, 127 in
 * shape, against 278 (80)
 */
#define KeepTopSpeed(topSpeed, acceleration) \
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
 *
 * Doesn't match: GCC keeps the zoom's gap in a float register rather than $s6,
 * and allocates the fabsf operands to other registers. It also reads turnLeft
 * and tiltLeft once before stepping them, where the original reads each twice.
 * The distance to a target sums the offsets' squares in x, y, z order, as the
 * original adds them; summing x, z, y gives 266 words, but the register oracle
 * then comes no closer than 173, against 161. A permuter run's best output,
 * 6723 against 7158, only clears turnLeft after toggling the turning flag,
 * giving 278 words differing, 74 in shape; the original clears it with the
 * turn's speed and braking angle, as here, so the gain is incidental. Setting a
 * reached target's z before its y also gives 266 words (80 in shape), but the
 * oracle then comes no closer than 165 words, forcing 48 pseudos, against 161
 * forcing 49, and the offsets and velocities around it go x, y, z. Those
 * counts were measured with GCC's built-in fabsf, when 278 words differed, 80
 * in shape. With the game's fabsf, an abs.s in volatile inline assembly, 188
 * words differ, 44 in shape.
 *
 * The zoom, the turn and the tilt each have their own remaining amount,
 * zoomLeft, turnLeft and tiltLeft, where one left variable served all three:
 * that takes the draft from 188 words to 154 (44 in shape), declared in
 * their blocks or at the top. Only the turn's own gives 172, only the zoom's
 * 186, and only the tilt's 394.
 *
 * The eight flag tests before letting go of the pivot each clear isDone:
 * jump.c turns each "if (test) isDone = FALSE" into an and with the negated
 * test, the original's branchless chain. Written as one expression, fold
 * merges the tests into two mask tests: with & or && between them 298 or
 * 309 words differ (102 and 111 in shape), and with * 322. A new flag
 * variable instead of isDone gives 314, in any declaration position.
 *
 * The point's velocity has its own velocityX, velocityY and velocityZ, where
 * the offsets to the target held it too, which keeps 154 words. distance
 * also holds the velocity's length, the distance moved in an update: a speed
 * variable for it gives 192 words (44 in shape), and 166 with the velocity's
 * variables.
 *
 * The arrival tests work out the point's offset from its target in each
 * case, as the y axis's test does, where offsetX and offsetZ, the offsets to
 * the target, held it too, reversed: that takes the draft from 154 words to
 * 138 (44 in shape to 40), and the register oracle's closest result from 154
 * to 138, with nothing forced. Only x's test gives 150, only z's 148, and a
 * variable of its own, shared or one for each axis, 159 (43 in shape).
 *
 * The zoom's, turn's and tilt's speed tests read the speed first and ask
 * whether it exceeds the top speed, as the original loads the speed before
 * the top speed and compares the top speed's magnitude below the speed's;
 * written top speed first, they load the top speed first. That takes the
 * draft from 138 words to 108, 40 in shape either way, and the register
 * oracle's closest result from 138 to 108, with nothing forced. Writing the
 * point's top speed test or its braking test the other way round gives 108
 * words too, and a speed variable for the velocity's length gives 120.
 *
 * The eight flag tests that set isDone come inside the test for a leader,
 * as the original branches on hasLeader before them. That takes the draft
 * from 108 words differing, 40 in shape, to 99 (37), and the register
 * oracle's closest result from 108 words to 99, with nothing forced.
 * Testing hasLeader && isDone after the flag tests as two nested ifs
 * gives 104 words (40 in shape), with isDone tested first 105 (41), and
 * as isDone && hasLeader 108 (40). 99 words differ, 37 in shape.
 */
#ifdef NON_MATCHING
void UpdateCameraEditor(void) {
  Boolean hasMoved = FALSE;
  /* Whether a point carries the other, and later whether one is a pivot */
  Boolean hasLeader = FALSE;
  s32 index;
  Boolean isDone;
  f32 y;
  f32 z; /* Also the distance before a zoom */
  f32 offsetX;
  f32 offsetY;
  f32 offsetZ;
  f32 distance;
  f32 scale;
  f32 gap;
  f32 zoomTarget;
  /*
   * The point's x, then the pitch's cosine, to match: a pitchCosine variable
   * in both direction blocks gives 186 words differing (44 in shape) against
   * the draft's 154 (44), and the register oracle's closest 164 words,
   * forcing 15 pseudos, against 154 with none forced
   */
  f32 x;

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
        f32 velocityX;
        f32 velocityY;
        f32 velocityZ;

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

        velocityX = g_CameraEditor.pointVelocities[index][0];
        velocityY = g_CameraEditor.pointVelocities[index][1];
        velocityZ = g_CameraEditor.pointVelocities[index][2];
        distance = sqrtf((velocityX * velocityX) + (velocityY * velocityY) +
            (velocityZ * velocityZ));
        if (g_CameraEditor.pointTopSpeeds[index] < distance) {
          scale = g_CameraEditor.pointTopSpeeds[index] / distance;
          g_CameraEditor.pointVelocities[index][0] = velocityX * scale;
          g_CameraEditor.pointVelocities[index][1] = velocityY * scale;
          g_CameraEditor.pointVelocities[index][2] = velocityZ * scale;
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
        if (fabsf(x - g_CameraEditor.targets[index][0]) <
                EDITOR_ARRIVAL_DISTANCE ||
            (g_CameraEditor.points[index][0] -
                g_CameraEditor.targets[index][0]) *
                    (x - g_CameraEditor.targets[index][0]) <
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
        if (fabsf(z - g_CameraEditor.targets[index][2]) <
                EDITOR_ARRIVAL_DISTANCE ||
            (g_CameraEditor.points[index][2] -
                g_CameraEditor.targets[index][2]) *
                    (z - g_CameraEditor.targets[index][2]) <
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
    x = cosf(g_CameraEditor.pitch);
    y = sinf(g_CameraEditor.pitch);
    z = sinf(g_CameraEditor.heading) * x;
    x = x * cosf(g_CameraEditor.heading);
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
    f32 zoomLeft;

    z = g_CameraEditor.distance;
    zoomTarget = g_CameraEditor.zoomTarget;
    /*
     * In the original, gap is unset when there is no braking distance; this
     * stops the zoom once a step brings it no closer
     */
#ifdef AVOID_UB
    gap = fabsf(zoomTarget - z);
#endif
    if (g_CameraEditor.zoomBrakingDistance != 0.0f) {
      gap = fabsf(zoomTarget - z);
      if (gap < EDITOR_ARRIVAL_DISTANCE) {
        g_CameraEditor.distance = zoomTarget;
      } else {
        g_CameraEditor.zoomSpeed += g_CameraEditor.zoomAcceleration;
        if (gap < g_CameraEditor.zoomBrakingDistance) {
          g_CameraEditor.zoomTopSpeed -=
              g_CameraEditor.zoomAcceleration * EDITOR_BRAKING_RATE;
          KeepTopSpeed(
              g_CameraEditor.zoomTopSpeed, g_CameraEditor.zoomAcceleration);
        }

        if (fabsf(g_CameraEditor.zoomSpeed) >
            fabsf(g_CameraEditor.zoomTopSpeed)) {
          g_CameraEditor.zoomSpeed = g_CameraEditor.zoomTopSpeed;
        }
      }
    }

    g_CameraEditor.distance += g_CameraEditor.zoomSpeed;
    zoomLeft = zoomTarget - g_CameraEditor.distance;
    isDone = FALSE;
    if (gap <= sqrtf(zoomLeft * zoomLeft)) {
      isDone = TRUE;
    }

    if (isDone) {
      g_CameraEditor.distance = g_CameraEditor.zoomTarget;
      g_CameraEditor.zoomSpeed = 0.0f;
      g_CameraEditor.zoomTarget = 0.0f;
      g_CameraEditor.zoomBrakingDistance = 0.0f;
      g_CameraEditor.flags[index] ^= EDITOR_FLAG_ZOOMING;
    }

    if (g_CameraEditor.distance < EDITOR_MIN_DISTANCE) {
      g_CameraEditor.distance = EDITOR_MIN_DISTANCE;
    }

    hasMoved = TRUE;
  }

  if (g_CameraEditor.flags[index] & EDITOR_FLAG_TURNING) {
    f32 turnLeft;

    turnLeft = g_CameraEditor.turnLeft;
    if (turnLeft != 0.0f) {
      if (fabsf(turnLeft) < EDITOR_ARRIVAL_ANGLE) {
        g_CameraEditor.heading += turnLeft;
      } else {
        g_CameraEditor.turnSpeed += g_CameraEditor.turnAcceleration;
        if (fabsf(turnLeft) < fabsf(g_CameraEditor.turnBrakingAngle)) {
          g_CameraEditor.turnTopSpeed -=
              g_CameraEditor.turnAcceleration * EDITOR_BRAKING_RATE;
          KeepTopSpeed(
              g_CameraEditor.turnTopSpeed, g_CameraEditor.turnAcceleration);
        }

        if (fabsf(g_CameraEditor.turnSpeed) >
            fabsf(g_CameraEditor.turnTopSpeed)) {
          g_CameraEditor.turnSpeed = g_CameraEditor.turnTopSpeed;
        }
      }
    }

    isDone = TRUE;
    if (!(fabsf(g_CameraEditor.turnLeft) <
            fabsf(g_CameraEditor.turnLeft - g_CameraEditor.turnSpeed))) {
      isDone = FALSE;
      g_CameraEditor.turnLeft -= g_CameraEditor.turnSpeed;
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
    f32 tiltLeft;

    tiltLeft = g_CameraEditor.tiltLeft;
    if (tiltLeft != 0.0f) {
      if (fabsf(tiltLeft) < EDITOR_ARRIVAL_ANGLE) {
        g_CameraEditor.pitch += tiltLeft;
      } else {
        g_CameraEditor.tiltSpeed += g_CameraEditor.tiltAcceleration;
        if (fabsf(tiltLeft) < fabsf(g_CameraEditor.tiltBrakingAngle)) {
          g_CameraEditor.tiltTopSpeed -=
              g_CameraEditor.tiltAcceleration * EDITOR_BRAKING_RATE;
          KeepTopSpeed(
              g_CameraEditor.tiltTopSpeed, g_CameraEditor.tiltAcceleration);
        }

        if (fabsf(g_CameraEditor.tiltSpeed) >
            fabsf(g_CameraEditor.tiltTopSpeed)) {
          g_CameraEditor.tiltSpeed = g_CameraEditor.tiltTopSpeed;
        }
      }
    }

    isDone = TRUE;
    if (!(fabsf(g_CameraEditor.tiltLeft) <
            fabsf(g_CameraEditor.tiltLeft - g_CameraEditor.tiltSpeed))) {
      isDone = FALSE;
      g_CameraEditor.tiltLeft -= g_CameraEditor.tiltSpeed;
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

  if (hasLeader) {
    /* The pivot is let go once nothing turns, tilts, zooms or carries */
    isDone = TRUE;
    if (g_CameraEditor.flags[EDITOR_POINT_EYE] & EDITOR_FLAG_TURNING) {
      isDone = FALSE;
    }
    if (g_CameraEditor.flags[EDITOR_POINT_EYE] & EDITOR_FLAG_TILTING) {
      isDone = FALSE;
    }
    if (g_CameraEditor.flags[EDITOR_POINT_EYE] & EDITOR_FLAG_ZOOMING) {
      isDone = FALSE;
    }
    if (g_CameraEditor.flags[EDITOR_POINT_EYE] & EDITOR_FLAG_CARRYING) {
      isDone = FALSE;
    }
    if (g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] & EDITOR_FLAG_TURNING) {
      isDone = FALSE;
    }
    if (g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] & EDITOR_FLAG_TILTING) {
      isDone = FALSE;
    }
    if (g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] & EDITOR_FLAG_ZOOMING) {
      isDone = FALSE;
    }
    if (g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] & EDITOR_FLAG_CARRYING) {
      isDone = FALSE;
    }
    if (isDone) {
      if (g_CameraEditor.flags[EDITOR_POINT_EYE] & EDITOR_FLAG_PIVOT) {
        g_CameraEditor.flags[EDITOR_POINT_EYE] = 0;
      }

      if (g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] & EDITOR_FLAG_PIVOT) {
        g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] = 0;
      }
    }
  }

  if (hasMoved) {
    x = cosf(g_CameraEditor.pitch);
    y = sinf(g_CameraEditor.pitch);
    z = sinf(g_CameraEditor.heading) * x;
    x = x * cosf(g_CameraEditor.heading);
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
#define ZoomEditorEye(scale) \
  { \
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
  }

/* Steps one coordinate of a point while the second controller holds a
 * button; step is += or -= */
#define StepEditorCoordinate(button, point, axis, step) \
  { \
    if (g_ControllerStates[SECOND_EDITOR_PAD].buttons & (button)) { \
      if (g_ControllerStates[SECOND_EDITOR_PAD].buttons & START_BUTTON) { \
        g_CameraEditor.points[point][axis] step FAST_PAD_STEP; \
      } else { \
        g_CameraEditor.points[point][axis] step SLOW_PAD_STEP; \
      } \
    } \
  }

/* The control pad moves a point across the ground and the C buttons up and
 * down */
#define StepEditorPoint(point) \
  { \
    StepEditorCoordinate(R_JPAD, point, 0, +=); \
    StepEditorCoordinate(L_JPAD, point, 0, -=); \
    StepEditorCoordinate(U_JPAD, point, 2, +=); \
    StepEditorCoordinate(D_JPAD, point, 2, -=); \
    StepEditorCoordinate(U_CBUTTONS, point, 1, -=); \
    StepEditorCoordinate(D_CBUTTONS, point, 1, +=); \
  }

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
    PlaySound(SOUND_SPLASH, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
    f_EditorChoice++;
    if (f_EditorChoice >= EDITOR_CHOICE_COUNT) {
      f_EditorChoice = 0;
    }
  }

  if (g_ControllerStates[EDITOR_PAD].pressedButtons & U_JPAD) {
    PlaySound(SOUND_SPLASH, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
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
  offset[AXIS_X] = 0.0f;
  offset[AXIS_Y] = 0.0f;
  offset[AXIS_Z] = 0.0f;
  if (f_EditorChoice == EDITOR_CHOICE_SLIDE) {
    if (*(u16*)&g_ControllerStates[EDITOR_PAD].stickX != 0) {
      offset[AXIS_X] = (cos(g_CameraEditor.heading) *
                           g_ControllerStates[EDITOR_PAD].stickY) +
          (sin(g_CameraEditor.heading) * g_ControllerStates[EDITOR_PAD].stickX);
      offset[AXIS_Z] = (sin(g_CameraEditor.heading) *
                           g_ControllerStates[EDITOR_PAD].stickY) -
          (cos(g_CameraEditor.heading) * g_ControllerStates[EDITOR_PAD].stickX);
      if (!(g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG)) {
        offset[AXIS_X] *= SLOW_SLIDE_SCALE;
        offset[AXIS_Z] *= SLOW_SLIDE_SCALE;
      }

      ShiftEditorCamera(offset);
    }

    if (g_ControllerStates[EDITOR_PAD].buttons & U_CBUTTONS) {
      if (g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG) {
        offset[AXIS_Y] = -FAST_HEIGHT_STEP;
      } else {
        offset[AXIS_Y] = -SLOW_HEIGHT_STEP;
      }

      ShiftEditorCamera(offset);
    }

    if (g_ControllerStates[EDITOR_PAD].buttons & D_CBUTTONS) {
      if (g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG) {
        offset[AXIS_Y] = FAST_HEIGHT_STEP;
      } else {
        offset[AXIS_Y] = SLOW_HEIGHT_STEP;
      }

      ShiftEditorCamera(offset);
    }
  }

  if (f_EditorChoice == EDITOR_CHOICE_TARGET) {
    if (*(u16*)&g_ControllerStates[EDITOR_PAD].stickX != 0) {
      offset[AXIS_X] = (cos(g_CameraEditor.heading) *
                           g_ControllerStates[EDITOR_PAD].stickY) +
          (sin(g_CameraEditor.heading) * g_ControllerStates[EDITOR_PAD].stickX);
      offset[AXIS_Z] = (sin(g_CameraEditor.heading) *
                           g_ControllerStates[EDITOR_PAD].stickY) -
          (cos(g_CameraEditor.heading) * g_ControllerStates[EDITOR_PAD].stickX);
      if (!(g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG)) {
        offset[AXIS_X] *= SLOW_SLIDE_SCALE;
        offset[AXIS_Z] *= SLOW_SLIDE_SCALE;
      }

      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] += offset[AXIS_X];
      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] += offset[AXIS_Z];
    }

    if (g_ControllerStates[EDITOR_PAD].buttons & U_CBUTTONS) {
      if (g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG) {
        offset[AXIS_Y] = -FAST_HEIGHT_STEP;
      } else {
        offset[AXIS_Y] = -SLOW_HEIGHT_STEP;
      }

      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] += offset[AXIS_Y];
    }

    if (g_ControllerStates[EDITOR_PAD].buttons & D_CBUTTONS) {
      if (g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG) {
        offset[AXIS_Y] = FAST_HEIGHT_STEP;
      } else {
        offset[AXIS_Y] = SLOW_HEIGHT_STEP;
      }

      g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] += offset[AXIS_Y];
    }
  }

  if (f_EditorChoice == EDITOR_CHOICE_POSITION) {
    if (*(u16*)&g_ControllerStates[EDITOR_PAD].stickX != 0) {
      offset[AXIS_X] = (cos(g_CameraEditor.heading) *
                           g_ControllerStates[EDITOR_PAD].stickY) +
          (sin(g_CameraEditor.heading) * g_ControllerStates[EDITOR_PAD].stickX);
      offset[AXIS_Z] = (sin(g_CameraEditor.heading) *
                           g_ControllerStates[EDITOR_PAD].stickY) -
          (cos(g_CameraEditor.heading) * g_ControllerStates[EDITOR_PAD].stickX);
      if (!(g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG)) {
        offset[AXIS_X] *= SLOW_SLIDE_SCALE;
        offset[AXIS_Z] *= SLOW_SLIDE_SCALE;
      }

      g_CameraEditor.points[EDITOR_POINT_EYE][0] += offset[AXIS_X];
      g_CameraEditor.points[EDITOR_POINT_EYE][2] += offset[AXIS_Z];
    }

    if (g_ControllerStates[EDITOR_PAD].buttons & U_CBUTTONS) {
      if (g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG) {
        offset[AXIS_Y] = -FAST_HEIGHT_STEP;
      } else {
        offset[AXIS_Y] = -SLOW_HEIGHT_STEP;
      }

      g_CameraEditor.points[EDITOR_POINT_EYE][1] += offset[AXIS_Y];
    }

    if (g_ControllerStates[EDITOR_PAD].buttons & D_CBUTTONS) {
      if (g_ControllerStates[EDITOR_PAD].buttons & Z_TRIG) {
        offset[AXIS_Y] = FAST_HEIGHT_STEP;
      } else {
        offset[AXIS_Y] = SLOW_HEIGHT_STEP;
      }

      g_CameraEditor.points[EDITOR_POINT_EYE][1] += offset[AXIS_Y];
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
      ZoomEditorEye(ZOOM_OUT_SCALE);
    }
  }

  if ((g_ControllerStates[EDITOR_PAD].buttons & L_TRIG) ||
      (g_ControllerStates[EDITOR_PAD].buttons & (Z_TRIG | R_TRIG)) ==
          (Z_TRIG | R_TRIG)) {
    if (g_CameraEditor.distance > MIN_ZOOM_IN_DISTANCE) {
      ZoomEditorEye(ZOOM_IN_SCALE);
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
    PlaySound(SOUND_LOOK_DOWN_RANGE, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
    f_SnapshotIndex++;
    if (f_SnapshotIndex >= SNAPSHOT_COUNT) {
      f_SnapshotIndex = 0;
    }
  }

  if ((g_ControllerStates[EDITOR_PAD].repeatedButtons & L_JPAD) &&
      *(u16*)&g_ControllerStates[EDITOR_PAD].stickX == 0) {
    PlaySound(SOUND_LOOK_DOWN_RANGE, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
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
      PlaySound(SOUND_IMPACT_ON_MARK, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
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
      PlaySound(SOUND_SNAPSHOT_LOAD, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
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
      PlaySound(SOUND_SNAPSHOT_LOAD, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
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
    StepEditorPoint(EDITOR_POINT_LOOK_AT);
  } else {
    StepEditorPoint(EDITOR_POINT_EYE);
  }
}

/* Moves the whole camera by offset */
void ShiftEditorCamera(Vec3 offset) {
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] += offset[AXIS_X];
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] += offset[AXIS_Y];
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] += offset[AXIS_Z];
  g_CameraEditor.points[EDITOR_POINT_EYE][0] += offset[AXIS_X];
  g_CameraEditor.points[EDITOR_POINT_EYE][1] += offset[AXIS_Y];
  g_CameraEditor.points[EDITOR_POINT_EYE][2] += offset[AXIS_Z];
}

/* Nothing refers to this */
const char f_UnusedCameraEditorEmptyText[] = "";
