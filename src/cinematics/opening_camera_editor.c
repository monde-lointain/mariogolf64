#include "cinematics/opening_camera_editor.h"

#include <math.h>
#include <ultra64.h>

#include "audio/sound.h"
#include "controllers/controller.h"
#include "debug_text.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/host_file.h"
#include "text/text.h"

#define SNAPSHOT_COUNT 32
#define SNAPSHOT_LOOK_AT 0
#define SNAPSHOT_EYE 1
#define SNAPSHOT_POINT_COUNT 2

enum EditorChoice {
    EDITOR_CHOICE_SLIDE,
    EDITOR_CHOICE_TARGET,
    EDITOR_CHOICE_POSITION,
    EDITOR_CHOICE_FIELD_OF_VIEW,
    EDITOR_CHOICE_COUNT
};

typedef s32 EditorChoice;

static Boolean f_IsSnapshotSaved[SNAPSHOT_COUNT] = {0};
static EditorChoice f_EditorChoice = EDITOR_CHOICE_SLIDE;
static s32 f_SnapshotIndex = 0;
static Boolean f_IsSecondControllerOnEye = FALSE;

Vec3 f_CameraSnapshots[SNAPSHOT_COUNT][SNAPSHOT_POINT_COUNT];

void ResetCameraEditor(void) {
    s32 pointIndex;

    g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_X] = 0.0f;
    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X] = 0.0f;
    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y] = 0.0f;
    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z] = 0.0f;
    g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Y] = -100.0f;
    g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Z] = 100.0f;
    g_CameraEditor.fieldOfView = 45.0f;
    for (pointIndex = 0; pointIndex != EDITOR_POINT_COUNT; pointIndex++) {
        g_CameraEditor.targets[pointIndex][AXIS_X] = 0.0f;
        g_CameraEditor.targets[pointIndex][AXIS_Y] = 0.0f;
        g_CameraEditor.targets[pointIndex][AXIS_Z] = 0.0f;
        g_CameraEditor.pointVelocities[pointIndex][AXIS_X] = 0.0f;
        g_CameraEditor.pointVelocities[pointIndex][AXIS_Y] = 0.0f;
        g_CameraEditor.pointVelocities[pointIndex][AXIS_Z] = 0.0f;

        g_CameraEditor.pointVelocities[pointIndex][AXIS_X] = 0.0f;
        g_CameraEditor.pointVelocities[pointIndex][AXIS_Y] = 0.0f;
        g_CameraEditor.pointVelocities[pointIndex][AXIS_Z] = 0.0f;
        g_CameraEditor.pointTopSpeeds[pointIndex] = 30.0f;
        g_CameraEditor.pointAccelerations[pointIndex] = 3.0f;
        g_CameraEditor.pointBrakingDistances[pointIndex] = 0.0f;
        g_CameraEditor.moveAxes[pointIndex] = EDITOR_MOVE_AXIS_NONE;
        g_CameraEditor.flags[pointIndex] = 0;
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

void PlaceEditorCamera(f32 eyeX, f32 eyeY, f32 eyeZ, f32 lookAtX, f32 lookAtY, f32 lookAtZ) {
    g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_X] = eyeX;
    g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Y] = eyeY;
    g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Z] = eyeZ;
    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X] = lookAtX;
    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y] = lookAtY;
    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z] = lookAtZ;
    MeasureEditorCamera();
}

void SetEditorPointSpeeds(s32 pointIndex, f32 topSpeed, f32 acceleration) {
    g_CameraEditor.pointTopSpeeds[pointIndex] = topSpeed;
    g_CameraEditor.pointAccelerations[pointIndex] = acceleration;
}

void SetEditorTurnSpeeds(s32 unusedPointIndex UNUSED, f32 topSpeed, f32 acceleration) {
    g_CameraEditor.turnTopSpeed = topSpeed;
    g_CameraEditor.turnAcceleration = acceleration;
}

void SetEditorTiltSpeeds(s32 unusedPointIndex UNUSED, f32 topSpeed, f32 acceleration) {
    g_CameraEditor.tiltTopSpeed = topSpeed;
    g_CameraEditor.tiltAcceleration = acceleration;
}

void SetEditorZoomSpeeds(s32 unusedPointIndex UNUSED, f32 topSpeed, f32 acceleration) {
    g_CameraEditor.zoomTopSpeed = topSpeed;
    g_CameraEditor.zoomAcceleration = acceleration;
}

void MeasureEditorCamera(void) {
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;

    offsetX = g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X] -
        g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_X];
    g_CameraEditor.pitch = atan2f(
        g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y] -
            g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Y],
        sqrtf(
            (offsetX * offsetX) +
            ((g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z] -
                 g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Z]) *
                (g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z] -
                    g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Z]))
        )
    );
    g_CameraEditor.heading = atan2f(
        g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z] -
            g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Z],
        g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X] -
            g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_X]
    );
    offsetY = g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y] -
        g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Y];
    offsetZ = g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z] -
        g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Z];
    g_CameraEditor.distance = sqrtf(
        ((g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X] -
             g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_X]) *
            (g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X] -
                g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_X])) +
        (offsetY * offsetY) + (offsetZ * offsetZ)
    );
}

#define EDITOR_ARRIVAL_DISTANCE 0.1f
#define EDITOR_ARRIVAL_ANGLE 0.001f
#define EDITOR_EASE_SHARE 4

void MoveEditorPoint(s32 pointIndex, f32 x, f32 y, f32 z, s32 frameCount) {
    f32 distance;
    s32 easeFrameCount;
    f32 speed;

    distance = sqrtf(
        ((x - g_CameraEditor.points[pointIndex][AXIS_X]) *
            (x - g_CameraEditor.points[pointIndex][AXIS_X])) +
        ((y - g_CameraEditor.points[pointIndex][AXIS_Y]) *
            (y - g_CameraEditor.points[pointIndex][AXIS_Y])) +
        ((z - g_CameraEditor.points[pointIndex][AXIS_Z]) *
            (z - g_CameraEditor.points[pointIndex][AXIS_Z]))
    );
    if (distance < EDITOR_ARRIVAL_DISTANCE) {
        g_CameraEditor.points[pointIndex][AXIS_X] = x;
        g_CameraEditor.points[pointIndex][AXIS_Y] = y;
        g_CameraEditor.points[pointIndex][AXIS_Z] = z;
        return;
    }

    if (frameCount != 0) {
        easeFrameCount = frameCount / EDITOR_EASE_SHARE;
        speed = distance / (frameCount - easeFrameCount);
        g_CameraEditor.pointTopSpeeds[pointIndex] = speed;
        g_CameraEditor.pointAccelerations[pointIndex] = speed / (easeFrameCount + 1);
        g_CameraEditor.pointBrakingDistances[pointIndex] = distance * (1.0 / EDITOR_EASE_SHARE);
        if (!(g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_CARRYING)) {
            if (!(g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_TIMED_MOVE)) {
                g_CameraEditor.flags[pointIndex] ^= EDITOR_FLAG_TIMED_MOVE;
            }

            if (g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_UNTIMED_MOVE) {
                g_CameraEditor.flags[pointIndex] ^= EDITOR_FLAG_UNTIMED_MOVE;
            }
        }
    }
    else {
        if (!(g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_UNTIMED_MOVE)) {
            g_CameraEditor.flags[pointIndex] ^= EDITOR_FLAG_UNTIMED_MOVE;
        }

        if (g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_TIMED_MOVE) {
            g_CameraEditor.flags[pointIndex] ^= EDITOR_FLAG_TIMED_MOVE;
        }
    }

    g_CameraEditor.targets[pointIndex][AXIS_X] = x;
    g_CameraEditor.targets[pointIndex][AXIS_Y] = y;
    g_CameraEditor.targets[pointIndex][AXIS_Z] = z;

    x -= g_CameraEditor.points[pointIndex][AXIS_X];
    y -= g_CameraEditor.points[pointIndex][AXIS_Y];
    z -= g_CameraEditor.points[pointIndex][AXIS_Z];
    g_CameraEditor.moveAxes[pointIndex] = EDITOR_MOVE_AXIS_X;
    if ((f32)fabs(x) < (f32)fabs(z)) {
        g_CameraEditor.moveAxes[pointIndex] = EDITOR_MOVE_AXIS_Z;
        x = z;
    }

    if ((f32)fabs(x) < (f32)fabs(y)) {
        g_CameraEditor.moveAxes[pointIndex] = EDITOR_MOVE_AXIS_Y;
    }
}

void MoveEditorCamera(s32 pointIndex, f32 x, f32 y, f32 z, s32 frameCount) {
    if (!(g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_CARRYING)) {
        g_CameraEditor.flags[pointIndex] ^= EDITOR_FLAG_CARRYING;
    }

    if (g_CameraEditor.flags[pointIndex ^ 1] & EDITOR_FLAG_CARRYING) {
        g_CameraEditor.flags[pointIndex ^ 1] ^= EDITOR_FLAG_CARRYING;
    }

    if (!(g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_PIVOT)) {
        g_CameraEditor.flags[pointIndex] ^= EDITOR_FLAG_PIVOT;
    }

    if (g_CameraEditor.flags[pointIndex ^ 1] & EDITOR_FLAG_PIVOT) {
        g_CameraEditor.flags[pointIndex ^ 1] ^= EDITOR_FLAG_PIVOT;
    }

    MoveEditorPoint(pointIndex, x, y, z, frameCount);
}

void TurnEditorCamera(s32 pointIndex, Radians angle, s32 frameCount) {
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
    if (!(g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_PIVOT)) {
        g_CameraEditor.flags[pointIndex] ^= EDITOR_FLAG_PIVOT;
    }

    if (g_CameraEditor.flags[pointIndex ^ 1] & EDITOR_FLAG_PIVOT) {
        g_CameraEditor.flags[pointIndex ^ 1] ^= EDITOR_FLAG_PIVOT;
    }

    if (g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_TURNING) {
        g_CameraEditor.flags[pointIndex] ^= EDITOR_FLAG_TURNING;
    }

    if (!(g_CameraEditor.flags[pointIndex ^ 1] & EDITOR_FLAG_TURNING)) {
        g_CameraEditor.flags[pointIndex ^ 1] ^= EDITOR_FLAG_TURNING;
    }
}

void TiltEditorCamera(s32 pointIndex, Radians angle, s32 frameCount) {
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
    if (!(g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_PIVOT)) {
        g_CameraEditor.flags[pointIndex] ^= EDITOR_FLAG_PIVOT;
    }

    if (g_CameraEditor.flags[pointIndex ^ 1] & EDITOR_FLAG_PIVOT) {
        g_CameraEditor.flags[pointIndex ^ 1] ^= EDITOR_FLAG_PIVOT;
    }

    if (g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_TILTING) {
        g_CameraEditor.flags[pointIndex] ^= EDITOR_FLAG_TILTING;
    }

    if (!(g_CameraEditor.flags[pointIndex ^ 1] & EDITOR_FLAG_TILTING)) {
        g_CameraEditor.flags[pointIndex ^ 1] ^= EDITOR_FLAG_TILTING;
    }
}

void ZoomEditorCamera(s32 pointIndex, f32 distance, s32 frameCount) {
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
        g_CameraEditor.zoomBrakingDistance = fabsf(change) * (1.0 / EDITOR_EASE_SHARE);
    }

    g_CameraEditor.zoomTarget = distance;
    if (!(g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_PIVOT)) {
        g_CameraEditor.flags[pointIndex] ^= EDITOR_FLAG_PIVOT;
    }

    if (g_CameraEditor.flags[pointIndex ^ 1] & EDITOR_FLAG_PIVOT) {
        g_CameraEditor.flags[pointIndex ^ 1] ^= EDITOR_FLAG_PIVOT;
    }

    if (g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_ZOOMING) {
        g_CameraEditor.flags[pointIndex] ^= EDITOR_FLAG_ZOOMING;
    }

    if (!(g_CameraEditor.flags[pointIndex ^ 1] & EDITOR_FLAG_ZOOMING)) {
        g_CameraEditor.flags[pointIndex ^ 1] ^= EDITOR_FLAG_ZOOMING;
    }
}

static const char f_UnusedIntegerFormat[] = "%d  ";
static const char f_UnusedFloatFormat[] = "%f";

Boolean AreEditorMovesDone(void) {
    if (g_CameraEditor.flags[EDITOR_POINT_EYE] == 0 &&
        g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] == 0) {
        return TRUE;
    }

    return FALSE;
}

Boolean AreEditorFlagsClear(s32 mask) {
    if (!(g_CameraEditor.flags[EDITOR_POINT_EYE] & mask) &&
        !(g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] & mask)) {
        return TRUE;
    }

    return FALSE;
}

void StopEditorMoves(void) {
    s32 pointIndex;

    for (pointIndex = 0; pointIndex != EDITOR_POINT_COUNT; pointIndex++) {
        g_CameraEditor.targets[pointIndex][AXIS_X] = 0.0f;
        g_CameraEditor.targets[pointIndex][AXIS_Y] = 0.0f;
        g_CameraEditor.targets[pointIndex][AXIS_Z] = 0.0f;
        g_CameraEditor.pointVelocities[pointIndex][AXIS_X] = 0.0f;
        g_CameraEditor.pointVelocities[pointIndex][AXIS_Y] = 0.0f;
        g_CameraEditor.pointVelocities[pointIndex][AXIS_Z] = 0.0f;

        g_CameraEditor.pointVelocities[pointIndex][AXIS_X] = 0.0f;
        g_CameraEditor.pointVelocities[pointIndex][AXIS_Y] = 0.0f;
        g_CameraEditor.pointVelocities[pointIndex][AXIS_Z] = 0.0f;
        g_CameraEditor.pointTopSpeeds[pointIndex] = 30.0f;
        g_CameraEditor.pointAccelerations[pointIndex] = 3.0f;
        g_CameraEditor.pointBrakingDistances[pointIndex] = 0.0f;
        g_CameraEditor.moveAxes[pointIndex] = EDITOR_MOVE_AXIS_NONE;
        g_CameraEditor.flags[pointIndex] = 0;
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

#define EDITOR_BRAKING_RATE 0.63979524f
#define EDITOR_PITCH_LIMIT 1.5f
#define EDITOR_DISTANCE_MIN 1.0f

#define KeepTopSpeed(topSpeed, acceleration) \
    do { \
        if (fabsf(topSpeed) < fabsf(acceleration)) { \
            (topSpeed) = (acceleration); \
        } \
    } \
    while (FALSE)

void UpdateCameraEditor(void) {
    Boolean hasMoved = FALSE;
    Boolean hasLeader = FALSE;
    s32 pointIndex;
    Boolean isDone;
    f32 x;
    f32 y;
    f32 z;
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;
    f32 distance;
    f32 scale;
    f32 gap;

    if (g_CameraEditor.flags[EDITOR_POINT_EYE] == 0 &&
        g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] == 0) {
        return;
    }

    for (pointIndex = 0; pointIndex != EDITOR_POINT_COUNT; pointIndex++) {
        if (g_CameraEditor.flags[EDITOR_POINT_EYE] & EDITOR_FLAG_CARRYING) {
            pointIndex = EDITOR_POINT_EYE;
            hasLeader = TRUE;
        }

        if (g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] & EDITOR_FLAG_CARRYING) {
            pointIndex = EDITOR_POINT_LOOK_AT;
            hasLeader = TRUE;
        }

        if (!hasLeader &&
            !(g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_TIMED_MOVE) &&
            !(g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_UNTIMED_MOVE)) {
            continue;
        }

        if (g_CameraEditor.moveAxes[pointIndex] == EDITOR_MOVE_AXIS_NONE) {
            if (hasLeader) {
                break;
            }

            continue;
        }

        x = g_CameraEditor.points[pointIndex][AXIS_X];
        y = g_CameraEditor.points[pointIndex][AXIS_Y];
        z = g_CameraEditor.points[pointIndex][AXIS_Z];
        if (g_CameraEditor.targets[pointIndex][AXIS_X] != 0.0f ||
            g_CameraEditor.targets[pointIndex][AXIS_Y] != 0.0f ||
            g_CameraEditor.targets[pointIndex][AXIS_Z] != 0.0f) {
            offsetX = g_CameraEditor.targets[pointIndex][AXIS_X] - x;
            offsetY = g_CameraEditor.targets[pointIndex][AXIS_Y] - y;
            offsetZ = g_CameraEditor.targets[pointIndex][AXIS_Z] - z;
            distance = sqrtf((offsetX * offsetX) + (offsetY * offsetY) + (offsetZ * offsetZ));
            if (distance < EDITOR_ARRIVAL_DISTANCE) {
                x = g_CameraEditor.targets[pointIndex][AXIS_X];
                y = g_CameraEditor.targets[pointIndex][AXIS_Y];
                z = g_CameraEditor.targets[pointIndex][AXIS_Z];
            }
            else {
                f32 velocityX;
                f32 velocityY;
                f32 velocityZ;

                scale = g_CameraEditor.pointAccelerations[pointIndex] / distance;
                g_CameraEditor.pointVelocities[pointIndex][AXIS_X] += offsetX * scale;
                g_CameraEditor.pointVelocities[pointIndex][AXIS_Y] += offsetY * scale;
                g_CameraEditor.pointVelocities[pointIndex][AXIS_Z] += offsetZ * scale;
                if (distance < g_CameraEditor.pointBrakingDistances[pointIndex]) {
                    g_CameraEditor.pointTopSpeeds[pointIndex] -=
                        g_CameraEditor.pointAccelerations[pointIndex] * EDITOR_BRAKING_RATE;
                    if (g_CameraEditor.pointTopSpeeds[pointIndex] <
                        g_CameraEditor.pointAccelerations[pointIndex]) {
                        g_CameraEditor.pointTopSpeeds[pointIndex] =
                            g_CameraEditor.pointAccelerations[pointIndex];
                    }
                }

                velocityX = g_CameraEditor.pointVelocities[pointIndex][AXIS_X];
                velocityY = g_CameraEditor.pointVelocities[pointIndex][AXIS_Y];
                velocityZ = g_CameraEditor.pointVelocities[pointIndex][AXIS_Z];
                distance = sqrtf(
                    (velocityX * velocityX) + (velocityY * velocityY) + (velocityZ * velocityZ)
                );
                if (g_CameraEditor.pointTopSpeeds[pointIndex] < distance) {
                    scale = g_CameraEditor.pointTopSpeeds[pointIndex] / distance;
                    g_CameraEditor.pointVelocities[pointIndex][AXIS_X] = velocityX * scale;
                    g_CameraEditor.pointVelocities[pointIndex][AXIS_Y] = velocityY * scale;
                    g_CameraEditor.pointVelocities[pointIndex][AXIS_Z] = velocityZ * scale;
                }
            }
        }

        x += g_CameraEditor.pointVelocities[pointIndex][AXIS_X];
        y += g_CameraEditor.pointVelocities[pointIndex][AXIS_Y];
        z += g_CameraEditor.pointVelocities[pointIndex][AXIS_Z];

        isDone = FALSE;
        switch (g_CameraEditor.moveAxes[pointIndex]) {
            case EDITOR_MOVE_AXIS_X:
                if (fabsf(x - g_CameraEditor.targets[pointIndex][AXIS_X]) <
                        EDITOR_ARRIVAL_DISTANCE ||
                    (g_CameraEditor.points[pointIndex][AXIS_X] -
                        g_CameraEditor.targets[pointIndex][AXIS_X]) *
                            (x - g_CameraEditor.targets[pointIndex][AXIS_X]) <
                        0.0f) {
                    isDone = TRUE;
                }

                break;
            case EDITOR_MOVE_AXIS_Y:
                if (fabsf(y - g_CameraEditor.targets[pointIndex][AXIS_Y]) <
                        EDITOR_ARRIVAL_DISTANCE ||
                    (g_CameraEditor.points[pointIndex][AXIS_Y] -
                        g_CameraEditor.targets[pointIndex][AXIS_Y]) *
                            (x - g_CameraEditor.targets[pointIndex][AXIS_Y]) <
                        0.0f) {
                    isDone = TRUE;
                }

                break;
            case EDITOR_MOVE_AXIS_Z:
                if (fabsf(z - g_CameraEditor.targets[pointIndex][AXIS_Z]) <
                        EDITOR_ARRIVAL_DISTANCE ||
                    (g_CameraEditor.points[pointIndex][AXIS_Z] -
                        g_CameraEditor.targets[pointIndex][AXIS_Z]) *
                            (z - g_CameraEditor.targets[pointIndex][AXIS_Z]) <
                        0.0f) {
                    isDone = TRUE;
                }

                break;
        }

        if (isDone) {
            x = g_CameraEditor.targets[pointIndex][AXIS_X];
            y = g_CameraEditor.targets[pointIndex][AXIS_Y];
            z = g_CameraEditor.targets[pointIndex][AXIS_Z];
            g_CameraEditor.pointVelocities[pointIndex][AXIS_X] = 0.0f;
            g_CameraEditor.pointVelocities[pointIndex][AXIS_Y] = 0.0f;
            g_CameraEditor.pointVelocities[pointIndex][AXIS_Z] = 0.0f;
            g_CameraEditor.targets[pointIndex][AXIS_X] = 0.0f;
            g_CameraEditor.targets[pointIndex][AXIS_Y] = 0.0f;
            g_CameraEditor.targets[pointIndex][AXIS_Z] = 0.0f;
            g_CameraEditor.pointBrakingDistances[pointIndex] = 0.0f;
            if (hasLeader) {
                g_CameraEditor.flags[pointIndex] ^= EDITOR_FLAG_CARRYING;
            }
            else {
                g_CameraEditor.flags[pointIndex] = 0;
            }

            g_CameraEditor.moveAxes[pointIndex] = EDITOR_MOVE_AXIS_NONE;
        }

        g_CameraEditor.points[pointIndex][AXIS_X] = x;
        g_CameraEditor.points[pointIndex][AXIS_Y] = y;
        g_CameraEditor.points[pointIndex][AXIS_Z] = z;
        if (hasLeader) {
            hasMoved = TRUE;
            break;
        }
    }

    if (hasLeader) {
        f32 pitchCosine = cosf(g_CameraEditor.pitch);

        y = sinf(g_CameraEditor.pitch);
        z = sinf(g_CameraEditor.heading) * pitchCosine;
        x = pitchCosine * cosf(g_CameraEditor.heading);
        distance = sqrtf((x * x) + (y * y) + (z * z));
        x /= distance;
        y /= distance;
        z /= distance;
        if (pointIndex == EDITOR_POINT_EYE) {
            x = -x;
            y = -y;
            z = -z;
        }

        g_CameraEditor.points[pointIndex ^ 1][AXIS_X] =
            g_CameraEditor.points[pointIndex][AXIS_X] + (x * g_CameraEditor.distance);
        g_CameraEditor.points[pointIndex ^ 1][AXIS_Y] =
            g_CameraEditor.points[pointIndex][AXIS_Y] + (y * g_CameraEditor.distance);
        g_CameraEditor.points[pointIndex ^ 1][AXIS_Z] =
            g_CameraEditor.points[pointIndex][AXIS_Z] + (z * g_CameraEditor.distance);
    }

    if (g_CameraEditor.flags[EDITOR_POINT_EYE] & EDITOR_FLAG_PIVOT) {
        pointIndex = EDITOR_POINT_LOOK_AT;
        hasLeader = TRUE;
    }

    if (g_CameraEditor.flags[EDITOR_POINT_LOOK_AT] & EDITOR_FLAG_PIVOT) {
        pointIndex = EDITOR_POINT_EYE;
        hasLeader = TRUE;
    }

    if (g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_ZOOMING) {
        f32 zoomLeft;

        z = g_CameraEditor.distance;
        offsetZ = g_CameraEditor.zoomTarget;
#ifdef AVOID_UB
        gap = fabsf(offsetZ - z);
#endif
        if (g_CameraEditor.zoomBrakingDistance != 0.0f) {
            gap = fabsf(offsetZ - z);
            if (gap < EDITOR_ARRIVAL_DISTANCE) {
                g_CameraEditor.distance = offsetZ;
            }
            else {
                g_CameraEditor.zoomSpeed += g_CameraEditor.zoomAcceleration;
                if (gap < g_CameraEditor.zoomBrakingDistance) {
                    g_CameraEditor.zoomTopSpeed -=
                        g_CameraEditor.zoomAcceleration * EDITOR_BRAKING_RATE;
                    KeepTopSpeed(g_CameraEditor.zoomTopSpeed, g_CameraEditor.zoomAcceleration);
                }

                if (fabsf(g_CameraEditor.zoomSpeed) > fabsf(g_CameraEditor.zoomTopSpeed)) {
                    g_CameraEditor.zoomSpeed = g_CameraEditor.zoomTopSpeed;
                }
            }
        }

        g_CameraEditor.distance += g_CameraEditor.zoomSpeed;
        zoomLeft = offsetZ - g_CameraEditor.distance;
        isDone = FALSE;
        distance = sqrtf(zoomLeft * zoomLeft);
        if (gap <= distance) {
            isDone = TRUE;
        }

        if (isDone) {
            g_CameraEditor.distance = g_CameraEditor.zoomTarget;
            g_CameraEditor.zoomSpeed = 0.0f;
            g_CameraEditor.zoomTarget = 0.0f;
            g_CameraEditor.zoomBrakingDistance = 0.0f;
            g_CameraEditor.flags[pointIndex] ^= EDITOR_FLAG_ZOOMING;
        }

        if (g_CameraEditor.distance < EDITOR_DISTANCE_MIN) {
            g_CameraEditor.distance = EDITOR_DISTANCE_MIN;
        }

        hasMoved = TRUE;
    }

    if (g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_TURNING) {
        if (g_CameraEditor.turnLeft != 0.0f) {
            if (fabsf(g_CameraEditor.turnLeft) < EDITOR_ARRIVAL_ANGLE) {
                g_CameraEditor.heading += g_CameraEditor.turnLeft;
            }
            else {
                g_CameraEditor.turnSpeed += g_CameraEditor.turnAcceleration;
                if (fabsf(g_CameraEditor.turnLeft) < fabsf(g_CameraEditor.turnBrakingAngle)) {
                    g_CameraEditor.turnTopSpeed -=
                        g_CameraEditor.turnAcceleration * EDITOR_BRAKING_RATE;
                    KeepTopSpeed(g_CameraEditor.turnTopSpeed, g_CameraEditor.turnAcceleration);
                }

                if (fabsf(g_CameraEditor.turnSpeed) > fabsf(g_CameraEditor.turnTopSpeed)) {
                    g_CameraEditor.turnSpeed = g_CameraEditor.turnTopSpeed;
                }
            }
        }

        if (fabsf(g_CameraEditor.turnLeft) <
            fabsf(g_CameraEditor.turnLeft - g_CameraEditor.turnSpeed)) {
            isDone = TRUE;
        }
        else {
            isDone = FALSE;
            g_CameraEditor.turnLeft -= g_CameraEditor.turnSpeed;
            g_CameraEditor.heading += g_CameraEditor.turnSpeed;
        }

        if (!isDone && fabsf(g_CameraEditor.turnLeft) < EDITOR_ARRIVAL_ANGLE) {
            isDone = TRUE;
        }

        if (isDone) {
            g_CameraEditor.heading += g_CameraEditor.turnLeft;
            g_CameraEditor.turnSpeed = 0.0f;
            g_CameraEditor.turnLeft = 0.0f;
            g_CameraEditor.turnBrakingAngle = 0.0f;
            g_CameraEditor.flags[pointIndex] ^= EDITOR_FLAG_TURNING;
        }

        hasMoved = TRUE;
    }

    if (g_CameraEditor.flags[pointIndex] & EDITOR_FLAG_TILTING) {
        if (g_CameraEditor.tiltLeft != 0.0f) {
            if (fabsf(g_CameraEditor.tiltLeft) < EDITOR_ARRIVAL_ANGLE) {
                g_CameraEditor.pitch += g_CameraEditor.tiltLeft;
            }
            else {
                g_CameraEditor.tiltSpeed += g_CameraEditor.tiltAcceleration;
                if (fabsf(g_CameraEditor.tiltLeft) < fabsf(g_CameraEditor.tiltBrakingAngle)) {
                    g_CameraEditor.tiltTopSpeed -=
                        g_CameraEditor.tiltAcceleration * EDITOR_BRAKING_RATE;
                    KeepTopSpeed(g_CameraEditor.tiltTopSpeed, g_CameraEditor.tiltAcceleration);
                }

                if (fabsf(g_CameraEditor.tiltSpeed) > fabsf(g_CameraEditor.tiltTopSpeed)) {
                    g_CameraEditor.tiltSpeed = g_CameraEditor.tiltTopSpeed;
                }
            }
        }

        if (fabsf(g_CameraEditor.tiltLeft) <
            fabsf(g_CameraEditor.tiltLeft - g_CameraEditor.tiltSpeed)) {
            isDone = TRUE;
        }
        else {
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
            g_CameraEditor.flags[pointIndex] ^= EDITOR_FLAG_TILTING;
        }

        hasMoved = TRUE;
    }

    if (hasLeader) {
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
        f32 pitchCosine = cosf(g_CameraEditor.pitch);

        y = sinf(g_CameraEditor.pitch);
        z = sinf(g_CameraEditor.heading) * pitchCosine;
        x = pitchCosine * cosf(g_CameraEditor.heading);
        distance = sqrtf((x * x) + (y * y) + (z * z));
        x /= distance;
        y /= distance;
        z /= distance;
        if (pointIndex == EDITOR_POINT_EYE) {
            x = -x;
            y = -y;
            z = -z;
        }

        g_CameraEditor.points[pointIndex][AXIS_X] =
            g_CameraEditor.points[pointIndex ^ 1][AXIS_X] + (x * g_CameraEditor.distance);
        g_CameraEditor.points[pointIndex][AXIS_Y] =
            g_CameraEditor.points[pointIndex ^ 1][AXIS_Y] + (y * g_CameraEditor.distance);
        g_CameraEditor.points[pointIndex][AXIS_Z] =
            g_CameraEditor.points[pointIndex ^ 1][AXIS_Z] + (z * g_CameraEditor.distance);
    }

    MeasureEditorCamera();
}

#define SNAPSHOT_ROW_LENGTH 16

#define EDITOR_MENU_COLUMN 4
#define EDITOR_MENU_TITLE_ROW 2
#define EDITOR_MENU_FIRST_ROW 4
#define CAMERA_PANEL_COLUMN 24
#define CAMERA_PANEL_LABEL_COLUMN 26
#define CAMERA_PANEL_TITLE_ROW 6
#define LOOK_AT_LABEL_ROW 7
#define EYE_LABEL_ROW 11
#define ANGLE_LABEL_ROW 15
#define DISTANCE_LABEL_ROW 18
#define FIELD_OF_VIEW_LABEL_ROW 20
#define SNAPSHOT_COLUMN 19
#define FIRST_SNAPSHOT_ROW 2
#define SECOND_SNAPSHOT_ROW 4

#define SLOW_SLIDE_SCALE 0.2f
#define SLOW_HEIGHT_STEP 0.5f
#define FAST_HEIGHT_STEP 10.0f
#define SLOW_FIELD_OF_VIEW_STEP 0.1f
#define FAST_FIELD_OF_VIEW_STEP 1.0f
#define FIELD_OF_VIEW_MIN 10.0f
#define FIELD_OF_VIEW_MAX 170.0f
#define SLOW_MARKER_SCALE_STEP 0.05f
#define FAST_MARKER_SCALE_STEP 0.5f
#define MARKER_SCALE_MIN 0.05f
#define MARKER_SCALE_MAX 10.0f
#define ZOOM_OUT_SCALE 1.1f
#define ZOOM_IN_SCALE 0.9f
#define ZOOM_OUT_DISTANCE_MAX 5000.0f
#define ZOOM_IN_DISTANCE_MIN 1.0f
#define SLOW_POINT_STEP 1.0f
#define FAST_POINT_STEP 20.0f

#define ZoomEditorEye(scale) \
    { \
        g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_X] = \
            ((g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_X] - \
                 g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X]) * \
                (scale)) + \
            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X]; \
        g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Y] = \
            ((g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Y] - \
                 g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y]) * \
                (scale)) + \
            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y]; \
        g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Z] = \
            ((g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Z] - \
                 g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z]) * \
                (scale)) + \
            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z]; \
    }

#define StepEditorCoordinate(button, point, axis, step) \
    { \
        if (g_ControllerStates[SECOND_EDITOR_CONTROLLER].buttons & (button)) { \
            if (g_ControllerStates[SECOND_EDITOR_CONTROLLER].buttons & START_BUTTON) { \
                g_CameraEditor.points[point][axis] step FAST_POINT_STEP; \
            } \
            else { \
                g_CameraEditor.points[point][axis] step SLOW_POINT_STEP; \
            } \
        } \
    }

#define StepEditorPoint(point) \
    { \
        StepEditorCoordinate(R_JPAD, point, 0, +=); \
        StepEditorCoordinate(L_JPAD, point, 0, -=); \
        StepEditorCoordinate(U_JPAD, point, 2, +=); \
        StepEditorCoordinate(D_JPAD, point, 2, -=); \
        StepEditorCoordinate(U_CBUTTONS, point, 1, -=); \
        StepEditorCoordinate(D_CBUTTONS, point, 1, +=); \
    }

void EditOpeningCamera(void) {
    Vec3 offset;
    u32 fileSize;
    s32 i;

    MeasureEditorCamera();
    PrintDebugText("<< ACTIVE >>", EDITOR_MENU_COLUMN, EDITOR_MENU_TITLE_ROW);
    PrintDebugText(" SLIDE", EDITOR_MENU_COLUMN, EDITOR_MENU_FIRST_ROW + EDITOR_CHOICE_SLIDE);
    PrintDebugText(" TARGET", EDITOR_MENU_COLUMN, EDITOR_MENU_FIRST_ROW + EDITOR_CHOICE_TARGET);
    PrintDebugText(" POSITION", EDITOR_MENU_COLUMN, EDITOR_MENU_FIRST_ROW + EDITOR_CHOICE_POSITION);
    PrintDebugText(
        " FROV",
        EDITOR_MENU_COLUMN,
        EDITOR_MENU_FIRST_ROW + EDITOR_CHOICE_FIELD_OF_VIEW
    );
    SetDebugTextHighlight(TRUE);
    PrintDebugText(">", EDITOR_MENU_COLUMN, EDITOR_MENU_FIRST_ROW + f_EditorChoice);
    if (g_ControllerStates[EDITOR_CONTROLLER].pressedButtons & D_JPAD) {
        PlaySound(SOUND_SPLASH, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
        f_EditorChoice++;
        if (f_EditorChoice >= EDITOR_CHOICE_COUNT) {
            f_EditorChoice = EDITOR_CHOICE_SLIDE;
        }
    }

    if (g_ControllerStates[EDITOR_CONTROLLER].pressedButtons & U_JPAD) {
        PlaySound(SOUND_SPLASH, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
        f_EditorChoice--;
        if (f_EditorChoice < EDITOR_CHOICE_SLIDE) {
            f_EditorChoice = EDITOR_CHOICE_COUNT - 1;
        }
    }

    PrintDebugText("<< POSITION >>", CAMERA_PANEL_COLUMN, CAMERA_PANEL_TITLE_ROW);
    SetDebugTextHighlight(TRUE);
    PrintDebugText("[TRG]", CAMERA_PANEL_LABEL_COLUMN, LOOK_AT_LABEL_ROW);
    sprintf(g_PrintBuffer, "  %f", g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X]);
    PrintDebugText(g_PrintBuffer, CAMERA_PANEL_COLUMN, LOOK_AT_LABEL_ROW + 1);
    sprintf(g_PrintBuffer, "  %f", g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y]);
    PrintDebugText(g_PrintBuffer, CAMERA_PANEL_COLUMN, LOOK_AT_LABEL_ROW + 2);
    sprintf(g_PrintBuffer, "  %f", g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z]);
    PrintDebugText(g_PrintBuffer, CAMERA_PANEL_COLUMN, LOOK_AT_LABEL_ROW + 3);
    SetDebugTextHighlight(TRUE);
    PrintDebugText("[POS]", CAMERA_PANEL_LABEL_COLUMN, EYE_LABEL_ROW);
    sprintf(g_PrintBuffer, "  %f", g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_X]);
    PrintDebugText(g_PrintBuffer, CAMERA_PANEL_COLUMN, EYE_LABEL_ROW + 1);
    sprintf(g_PrintBuffer, "  %f", g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Y]);
    PrintDebugText(g_PrintBuffer, CAMERA_PANEL_COLUMN, EYE_LABEL_ROW + 2);
    sprintf(g_PrintBuffer, "  %f", g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Z]);
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
    PrintDebugText(g_PrintBuffer, CAMERA_PANEL_COLUMN, FIELD_OF_VIEW_LABEL_ROW + 1);

    offset[AXIS_X] = 0.0f;
    offset[AXIS_Y] = 0.0f;
    offset[AXIS_Z] = 0.0f;
    if (f_EditorChoice == EDITOR_CHOICE_SLIDE) {
        if (*(u16 *)&g_ControllerStates[EDITOR_CONTROLLER].stickX != 0) {
            offset[AXIS_X] =
                (cos(g_CameraEditor.heading) * g_ControllerStates[EDITOR_CONTROLLER].stickY) +
                (sin(g_CameraEditor.heading) * g_ControllerStates[EDITOR_CONTROLLER].stickX);
            offset[AXIS_Z] =
                (sin(g_CameraEditor.heading) * g_ControllerStates[EDITOR_CONTROLLER].stickY) -
                (cos(g_CameraEditor.heading) * g_ControllerStates[EDITOR_CONTROLLER].stickX);
            if (!(g_ControllerStates[EDITOR_CONTROLLER].buttons & Z_TRIG)) {
                offset[AXIS_X] *= SLOW_SLIDE_SCALE;
                offset[AXIS_Z] *= SLOW_SLIDE_SCALE;
            }

            ShiftEditorCamera(offset);
        }

        if (g_ControllerStates[EDITOR_CONTROLLER].buttons & U_CBUTTONS) {
            if (g_ControllerStates[EDITOR_CONTROLLER].buttons & Z_TRIG) {
                offset[AXIS_Y] = -FAST_HEIGHT_STEP;
            }
            else {
                offset[AXIS_Y] = -SLOW_HEIGHT_STEP;
            }

            ShiftEditorCamera(offset);
        }

        if (g_ControllerStates[EDITOR_CONTROLLER].buttons & D_CBUTTONS) {
            if (g_ControllerStates[EDITOR_CONTROLLER].buttons & Z_TRIG) {
                offset[AXIS_Y] = FAST_HEIGHT_STEP;
            }
            else {
                offset[AXIS_Y] = SLOW_HEIGHT_STEP;
            }

            ShiftEditorCamera(offset);
        }
    }

    if (f_EditorChoice == EDITOR_CHOICE_TARGET) {
        if (*(u16 *)&g_ControllerStates[EDITOR_CONTROLLER].stickX != 0) {
            offset[AXIS_X] =
                (cos(g_CameraEditor.heading) * g_ControllerStates[EDITOR_CONTROLLER].stickY) +
                (sin(g_CameraEditor.heading) * g_ControllerStates[EDITOR_CONTROLLER].stickX);
            offset[AXIS_Z] =
                (sin(g_CameraEditor.heading) * g_ControllerStates[EDITOR_CONTROLLER].stickY) -
                (cos(g_CameraEditor.heading) * g_ControllerStates[EDITOR_CONTROLLER].stickX);
            if (!(g_ControllerStates[EDITOR_CONTROLLER].buttons & Z_TRIG)) {
                offset[AXIS_X] *= SLOW_SLIDE_SCALE;
                offset[AXIS_Z] *= SLOW_SLIDE_SCALE;
            }

            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X] += offset[AXIS_X];
            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z] += offset[AXIS_Z];
        }

        if (g_ControllerStates[EDITOR_CONTROLLER].buttons & U_CBUTTONS) {
            if (g_ControllerStates[EDITOR_CONTROLLER].buttons & Z_TRIG) {
                offset[AXIS_Y] = -FAST_HEIGHT_STEP;
            }
            else {
                offset[AXIS_Y] = -SLOW_HEIGHT_STEP;
            }

            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y] += offset[AXIS_Y];
        }

        if (g_ControllerStates[EDITOR_CONTROLLER].buttons & D_CBUTTONS) {
            if (g_ControllerStates[EDITOR_CONTROLLER].buttons & Z_TRIG) {
                offset[AXIS_Y] = FAST_HEIGHT_STEP;
            }
            else {
                offset[AXIS_Y] = SLOW_HEIGHT_STEP;
            }

            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y] += offset[AXIS_Y];
        }
    }

    if (f_EditorChoice == EDITOR_CHOICE_POSITION) {
        if (*(u16 *)&g_ControllerStates[EDITOR_CONTROLLER].stickX != 0) {
            offset[AXIS_X] =
                (cos(g_CameraEditor.heading) * g_ControllerStates[EDITOR_CONTROLLER].stickY) +
                (sin(g_CameraEditor.heading) * g_ControllerStates[EDITOR_CONTROLLER].stickX);
            offset[AXIS_Z] =
                (sin(g_CameraEditor.heading) * g_ControllerStates[EDITOR_CONTROLLER].stickY) -
                (cos(g_CameraEditor.heading) * g_ControllerStates[EDITOR_CONTROLLER].stickX);
            if (!(g_ControllerStates[EDITOR_CONTROLLER].buttons & Z_TRIG)) {
                offset[AXIS_X] *= SLOW_SLIDE_SCALE;
                offset[AXIS_Z] *= SLOW_SLIDE_SCALE;
            }

            g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_X] += offset[AXIS_X];
            g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Z] += offset[AXIS_Z];
        }

        if (g_ControllerStates[EDITOR_CONTROLLER].buttons & U_CBUTTONS) {
            if (g_ControllerStates[EDITOR_CONTROLLER].buttons & Z_TRIG) {
                offset[AXIS_Y] = -FAST_HEIGHT_STEP;
            }
            else {
                offset[AXIS_Y] = -SLOW_HEIGHT_STEP;
            }

            g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Y] += offset[AXIS_Y];
        }

        if (g_ControllerStates[EDITOR_CONTROLLER].buttons & D_CBUTTONS) {
            if (g_ControllerStates[EDITOR_CONTROLLER].buttons & Z_TRIG) {
                offset[AXIS_Y] = FAST_HEIGHT_STEP;
            }
            else {
                offset[AXIS_Y] = SLOW_HEIGHT_STEP;
            }

            g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Y] += offset[AXIS_Y];
        }
    }

    if (f_EditorChoice == EDITOR_CHOICE_FIELD_OF_VIEW) {
        if (g_ControllerStates[EDITOR_CONTROLLER].buttons & U_CBUTTONS) {
            if (g_ControllerStates[EDITOR_CONTROLLER].buttons & Z_TRIG) {
                g_CameraEditor.fieldOfView -= FAST_FIELD_OF_VIEW_STEP;
            }
            else {
                g_CameraEditor.fieldOfView -= SLOW_FIELD_OF_VIEW_STEP;
            }

            if (g_CameraEditor.fieldOfView < FIELD_OF_VIEW_MIN) {
                g_CameraEditor.fieldOfView = FIELD_OF_VIEW_MIN;
            }
        }

        if (g_ControllerStates[EDITOR_CONTROLLER].buttons & D_CBUTTONS) {
            if (g_ControllerStates[EDITOR_CONTROLLER].buttons & Z_TRIG) {
                g_CameraEditor.fieldOfView += FAST_FIELD_OF_VIEW_STEP;
            }
            else {
                g_CameraEditor.fieldOfView += SLOW_FIELD_OF_VIEW_STEP;
            }

            if (g_CameraEditor.fieldOfView > FIELD_OF_VIEW_MAX) {
                g_CameraEditor.fieldOfView = FIELD_OF_VIEW_MAX;
            }
        }

        if (g_ControllerStates[EDITOR_CONTROLLER].buttons & L_CBUTTONS) {
            if (g_ControllerStates[EDITOR_CONTROLLER].buttons & Z_TRIG) {
                g_LookAtMarkerScale -= FAST_MARKER_SCALE_STEP;
            }
            else {
                g_LookAtMarkerScale -= SLOW_MARKER_SCALE_STEP;
            }

            if (g_LookAtMarkerScale < MARKER_SCALE_MIN) {
                g_LookAtMarkerScale = MARKER_SCALE_MIN;
            }
        }

        if (g_ControllerStates[EDITOR_CONTROLLER].buttons & R_CBUTTONS) {
            if (g_ControllerStates[EDITOR_CONTROLLER].buttons & Z_TRIG) {
                g_LookAtMarkerScale += FAST_MARKER_SCALE_STEP;
            }
            else {
                g_LookAtMarkerScale += SLOW_MARKER_SCALE_STEP;
            }

            if (g_LookAtMarkerScale > MARKER_SCALE_MAX) {
                g_LookAtMarkerScale = MARKER_SCALE_MAX;
            }
        }
    }

    if ((g_ControllerStates[EDITOR_CONTROLLER].buttons & (Z_TRIG | R_TRIG)) == R_TRIG) {
        if (g_CameraEditor.distance < ZOOM_OUT_DISTANCE_MAX) {
            ZoomEditorEye(ZOOM_OUT_SCALE);
        }
    }

    if ((g_ControllerStates[EDITOR_CONTROLLER].buttons & L_TRIG) ||
        (g_ControllerStates[EDITOR_CONTROLLER].buttons & (Z_TRIG | R_TRIG)) == (Z_TRIG | R_TRIG)) {
        if (g_CameraEditor.distance > ZOOM_IN_DISTANCE_MIN) {
            ZoomEditorEye(ZOOM_IN_SCALE);
        }
    }

    for (i = 0; i != SNAPSHOT_COUNT; i++) {
        if (i < SNAPSHOT_ROW_LENGTH) {
            if (f_IsSnapshotSaved[i]) {
                SetDebugTextHighlight(TRUE);
                PrintDebugText("*", SNAPSHOT_COLUMN + i, FIRST_SNAPSHOT_ROW);
            }
            else {
                PrintDebugText("*", SNAPSHOT_COLUMN + i, FIRST_SNAPSHOT_ROW);
            }
        }
        else {
            if (f_IsSnapshotSaved[i]) {
                SetDebugTextHighlight(TRUE);
                PrintDebugText("*", SNAPSHOT_COLUMN + i - SNAPSHOT_ROW_LENGTH, SECOND_SNAPSHOT_ROW);
            }
            else {
                PrintDebugText("*", SNAPSHOT_COLUMN + i - SNAPSHOT_ROW_LENGTH, SECOND_SNAPSHOT_ROW);
            }
        }
    }

    PrintDebugText("                ", SNAPSHOT_COLUMN, FIRST_SNAPSHOT_ROW + 1);
    PrintDebugText("                ", SNAPSHOT_COLUMN, SECOND_SNAPSHOT_ROW + 1);
    SetDebugTextHighlight(TRUE);
    if (f_SnapshotIndex < SNAPSHOT_ROW_LENGTH) {
        PrintDebugText("^", SNAPSHOT_COLUMN + f_SnapshotIndex, FIRST_SNAPSHOT_ROW + 1);
    }
    else {
        PrintDebugText(
            "^",
            SNAPSHOT_COLUMN + f_SnapshotIndex - SNAPSHOT_ROW_LENGTH,
            SECOND_SNAPSHOT_ROW + 1
        );
    }

    if ((g_ControllerStates[EDITOR_CONTROLLER].repeatedButtons & R_JPAD) &&
        *(u16 *)&g_ControllerStates[EDITOR_CONTROLLER].stickX == 0) {
        PlaySound(SOUND_LOOK_DOWN_RANGE, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
        f_SnapshotIndex++;
        if (f_SnapshotIndex >= SNAPSHOT_COUNT) {
            f_SnapshotIndex = 0;
        }
    }

    if ((g_ControllerStates[EDITOR_CONTROLLER].repeatedButtons & L_JPAD) &&
        *(u16 *)&g_ControllerStates[EDITOR_CONTROLLER].stickX == 0) {
        PlaySound(SOUND_LOOK_DOWN_RANGE, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
        f_SnapshotIndex--;
        if (f_SnapshotIndex < 0) {
            f_SnapshotIndex = SNAPSHOT_COUNT - 1;
        }
    }

    if (g_ControllerStates[EDITOR_CONTROLLER].pressedButtons & A_BUTTON) {
        if (f_IsSnapshotSaved[f_SnapshotIndex] &&
            !(g_ControllerStates[EDITOR_CONTROLLER].buttons & START_BUTTON)) {
            PlaySound(SOUND_MENU_BLOCKED, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
        else {
            PlaySound(SOUND_IMPACT_ON_MARK, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
            f_IsSnapshotSaved[f_SnapshotIndex] = TRUE;
            f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_LOOK_AT][0] =
                g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X];
            f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_LOOK_AT][1] =
                g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y];
            f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_LOOK_AT][2] =
                g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z];
            f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_EYE][0] =
                g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_X];
            f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_EYE][1] =
                g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Y];
            f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_EYE][2] =
                g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Z];
        }
    }

    if ((g_ControllerStates[EDITOR_CONTROLLER].pressedButtons & B_BUTTON) &&
        !(g_ControllerStates[EDITOR_CONTROLLER].buttons & START_BUTTON)) {
        if (f_IsSnapshotSaved[f_SnapshotIndex]) {
            PlaySound(SOUND_SNAPSHOT_LOAD, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X] =
                f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_LOOK_AT][0];
            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y] =
                f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_LOOK_AT][1];
            g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z] =
                f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_LOOK_AT][2];
            g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_X] =
                f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_EYE][0];
            g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Y] =
                f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_EYE][1];
            g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Z] =
                f_CameraSnapshots[f_SnapshotIndex][SNAPSHOT_EYE][2];
        }
        else {
            PlaySound(SOUND_MENU_BLOCKED, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
    }

    if ((g_ControllerStates[EDITOR_CONTROLLER].buttons & START_BUTTON) &&
        (g_ControllerStates[EDITOR_CONTROLLER].pressedButtons & B_BUTTON)) {
        if (f_IsSnapshotSaved[f_SnapshotIndex]) {
            PlaySound(SOUND_SNAPSHOT_LOAD, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
            f_IsSnapshotSaved[f_SnapshotIndex] = FALSE;
        }
        else {
            PlaySound(SOUND_MENU_BLOCKED, EDITOR_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
    }

    if ((g_ControllerStates[SECOND_EDITOR_CONTROLLER].buttons & (Z_TRIG | START_BUTTON)) ==
            (Z_TRIG | START_BUTTON) &&
        (g_ControllerStates[SECOND_EDITOR_CONTROLLER].pressedButtons & L_TRIG)) {
        sprintf(g_PrintBuffer, "opening/snapshot.pos");
        WriteHostFile(g_PrintBuffer, sizeof(f_CameraSnapshots));
        sprintf(g_PrintBuffer, "opening/snapshot.flg");
        WriteHostFile(g_PrintBuffer, sizeof(f_IsSnapshotSaved));
    }

    if ((g_ControllerStates[SECOND_EDITOR_CONTROLLER].buttons & (Z_TRIG | START_BUTTON)) ==
            (Z_TRIG | START_BUTTON) &&
        (g_ControllerStates[SECOND_EDITOR_CONTROLLER].pressedButtons & R_TRIG)) {
        sprintf(g_PrintBuffer, "opening/snapshot.pos");
        ReadHostFile(g_PrintBuffer, &fileSize);
        sprintf(g_PrintBuffer, "opening/snapshot.flg");
        ReadHostFile(g_PrintBuffer, &fileSize);
    }

    if ((g_ControllerStates[SECOND_EDITOR_CONTROLLER].pressedButtons & R_CBUTTONS) ||
        (g_ControllerStates[SECOND_EDITOR_CONTROLLER].pressedButtons & L_CBUTTONS)) {
        f_IsSecondControllerOnEye ^= TRUE;
    }

    if (!f_IsSecondControllerOnEye) {
        SetDebugTextHighlight(TRUE);
        PrintDebugText("?", CAMERA_PANEL_COLUMN, LOOK_AT_LABEL_ROW + 1);
        SetDebugTextHighlight(TRUE);
        PrintDebugText("?", CAMERA_PANEL_COLUMN, LOOK_AT_LABEL_ROW + 2);
        SetDebugTextHighlight(TRUE);
        PrintDebugText("?", CAMERA_PANEL_COLUMN, LOOK_AT_LABEL_ROW + 3);
    }
    else {
        SetDebugTextHighlight(TRUE);
        PrintDebugText("?", CAMERA_PANEL_COLUMN, EYE_LABEL_ROW + 1);
        SetDebugTextHighlight(TRUE);
        PrintDebugText("?", CAMERA_PANEL_COLUMN, EYE_LABEL_ROW + 2);
        SetDebugTextHighlight(TRUE);
        PrintDebugText("?", CAMERA_PANEL_COLUMN, EYE_LABEL_ROW + 3);
    }

    if (!f_IsSecondControllerOnEye) {
        StepEditorPoint(EDITOR_POINT_LOOK_AT);
    }
    else {
        StepEditorPoint(EDITOR_POINT_EYE);
    }
}

void ShiftEditorCamera(Vec3 offset) {
    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_X] += offset[AXIS_X];
    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Y] += offset[AXIS_Y];
    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][AXIS_Z] += offset[AXIS_Z];
    g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_X] += offset[AXIS_X];
    g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Y] += offset[AXIS_Y];
    g_CameraEditor.points[EDITOR_POINT_EYE][AXIS_Z] += offset[AXIS_Z];
}

const char f_UnusedCameraEditorEmptyText[] = "";
