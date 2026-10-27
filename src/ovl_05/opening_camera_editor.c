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
      sqrtf(offsetX * offsetX +
          (g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] -
              g_CameraEditor.points[EDITOR_POINT_EYE][2]) *
              (g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] -
                  g_CameraEditor.points[EDITOR_POINT_EYE][2])));
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
      sqrtf((g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] -
                g_CameraEditor.points[EDITOR_POINT_EYE][0]) *
              (g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] -
                  g_CameraEditor.points[EDITOR_POINT_EYE][0]) +
          offsetY * offsetY + offsetZ * offsetZ);
}

/* Starts moving point index to (x, y, z) over frameCount frames, or snaps it
 * there when it is close */
void MoveEditorPoint(s32 index, f32 x, f32 y, f32 z, s32 frameCount) {
  f32 distance;
  s32 easeFrameCount;
  f32 speed;

  distance = sqrtf((x - g_CameraEditor.points[index][0]) *
          (x - g_CameraEditor.points[index][0]) +
      (y - g_CameraEditor.points[index][1]) *
          (y - g_CameraEditor.points[index][1]) +
      (z - g_CameraEditor.points[index][2]) *
          (z - g_CameraEditor.points[index][2]));
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

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", UpdateCameraEditor);

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
