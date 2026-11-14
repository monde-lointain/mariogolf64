#include "framing.h"

#include <math.h>
#include <ultra64.h>

#include "camera.h"
#include "compiler.h"
#include "controller.h"
#include "geometry.h"
#include "math_util.h"

#define FRAMED_SPHERE_CAPACITY 8

#define STEEP_PITCH_COSINE 0.1f

#define FRAMING_PI 3.141593f
#define FramingDegreesToRadians(degrees) ((degrees) * (FRAMING_PI / 180))

#define FAR_EXTENT 30000.0f

#define Z_MARGIN 10.0f

static f32 f_FramedSphereCenters[FRAMED_SPHERE_CAPACITY][3];
static f32 f_FramedSphereRadii[FRAMED_SPHERE_CAPACITY];
static s32 f_FramedSphereCount;
static Radians f_FramingPitch;
static Radians f_FramingYaw;
static Mat4 f_FramingRotation;

FILE_STATIC Vec3 f_FramedEye;
FILE_STATIC Vec3 f_FramedLookAt;

void GetFramedView(Vec3 eye, Vec3 lookAt) {
    eye[AXIS_X] = f_FramedEye[0];
    eye[AXIS_Y] = f_FramedEye[1];
    eye[AXIS_Z] = f_FramedEye[2];
    lookAt[AXIS_X] = f_FramedLookAt[0];
    lookAt[AXIS_Y] = f_FramedLookAt[1];
    lookAt[AXIS_Z] = f_FramedLookAt[2];
}

void ClearFramedSpheres(void) {
    f_FramedSphereCount = 0;
}

void AddFramedSphere(Vec3 center, f32 radius) {
    if (f_FramedSphereCount < FRAMED_SPHERE_CAPACITY) {
        f_FramedSphereCenters[f_FramedSphereCount][0] = center[AXIS_X];
        f_FramedSphereCenters[f_FramedSphereCount][1] = center[AXIS_Y];
        f_FramedSphereCenters[f_FramedSphereCount][2] = center[AXIS_Z];
        f_FramedSphereRadii[f_FramedSphereCount] = radius;
        if (g_ControllerData[1].trigger & D_CBUTTONS) {
            osSyncPrintf(
                "ox %f ox %f oz %f\n",
                f_FramedSphereCenters[f_FramedSphereCount][0],
                f_FramedSphereCenters[f_FramedSphereCount][1],
                f_FramedSphereCenters[f_FramedSphereCount][2]
            );
        }

        f_FramedSphereCount++;
    }
}

void SetFramingAngles(Radians pitch, Radians yaw) {
    Mat4 pitchRotation;
    Mat4 yawRotation;

    if (fabsf(cosf(pitch)) < STEEP_PITCH_COSINE) {
        pitch += FramingDegreesToRadians(20);
    }

    f_FramingPitch = pitch;
    f_FramingYaw = yaw;
    guRotateF(pitchRotation, (s32)(pitch * RADIANS_TO_DEGREES), -1.0f, 0.0f, 0.0f);
    guRotateF(yawRotation, (s32)(yaw * RADIANS_TO_DEGREES), 0.0f, -1.0f, 0.0f);
    guMtxCatF(yawRotation, pitchRotation, f_FramingRotation);
}

void TransformPoint(Mat4 matrix, Vec3 point, Vec3 transformed) {
    f32 x = point[AXIS_X];
    f32 y = point[AXIS_Y];
    f32 z = point[AXIS_Z];

    transformed[AXIS_X] =
        (matrix[0][0] * x) + (matrix[1][0] * y) + (matrix[2][0] * z) + matrix[3][0];
    transformed[AXIS_Y] =
        (matrix[0][1] * x) + (matrix[1][1] * y) + (matrix[2][1] * z) + matrix[3][1];
    transformed[AXIS_Z] =
        (matrix[0][2] * x) + (matrix[1][2] * y) + (matrix[2][2] * z) + matrix[3][2];
}

void FrameSpheres(f32 depthOffset) {
    Vec3 point;
    Mat4 rotation;
    Mat4 pitchRotation;
    Mat4 yawRotation;
    Mat4 orientation;
    f32 horizontalEyeZ;
    Degrees rightAngle;
    Degrees leftAngle;
    Degrees topAngle;
    Degrees bottomAngle;
    f32 zMin;
    f32 zMax;
    f32 xMin;
    f32 xMax;
    f32 yMin;
    f32 yMax;
    Radians angle;
    f32 leftDistance;
    f32 rightDistance;
    f32 topDistance;
    f32 bottomDistance;
    f32 eyeX;
    f32 eyeY;
    f32 eyeZ;
    f32 verticalEyeZ;
    f32 depth;
    f32 right;
    f32 left;
    f32 top;
    f32 bottom;
    f32 centerX;
    f32 centerY;
    f32 clearEyeZ;
    f32 margin;
    s32 sphereIndex;

    margin = Z_MARGIN;
    yMin = FAR_EXTENT;
    xMin = yMin;
    yMax = -FAR_EXTENT;
    xMax = yMax;
    zMin = margin;
    zMax = -margin;
    rightAngle = g_Cameras[MAIN_CAMERA].view.fieldOfView * 0.5f;
    leftAngle = -rightAngle;
    topAngle = rightAngle * 0.75f;
    bottomAngle = leftAngle * 0.75f;
    if (g_ControllerData[1].trigger & D_CBUTTONS) {
        osSyncPrintf("forv %f\n", g_Cameras[MAIN_CAMERA].view.fieldOfView);
    }

    guRotateF(rotation, rightAngle, 0.0f, 1.0f, 0.0f);
    guMtxCatF(f_FramingRotation, rotation, rotation);
    for (sphereIndex = 0; sphereIndex < f_FramedSphereCount; sphereIndex++) {
        TransformPoint(rotation, f_FramedSphereCenters[sphereIndex], point);
        if (point[AXIS_X] - f_FramedSphereRadii[sphereIndex] < xMin) {
            xMin = point[AXIS_X] - f_FramedSphereRadii[sphereIndex];
        }
    }

    guRotateF(rotation, leftAngle, 0.0f, 1.0f, 0.0f);
    guMtxCatF(f_FramingRotation, rotation, rotation);
    for (sphereIndex = 0; sphereIndex < f_FramedSphereCount; sphereIndex++) {
        TransformPoint(rotation, f_FramedSphereCenters[sphereIndex], point);
        if (xMax < point[AXIS_X] + f_FramedSphereRadii[sphereIndex]) {
            xMax = point[AXIS_X] + f_FramedSphereRadii[sphereIndex];
        }
    }

    guRotateF(rotation, topAngle, -1.0f, 0.0f, 0.0f);
    guMtxCatF(f_FramingRotation, rotation, rotation);
    for (sphereIndex = 0; sphereIndex < f_FramedSphereCount; sphereIndex++) {
        TransformPoint(rotation, f_FramedSphereCenters[sphereIndex], point);
        if (point[AXIS_Y] - f_FramedSphereRadii[sphereIndex] < yMin) {
            yMin = point[AXIS_Y] - f_FramedSphereRadii[sphereIndex];
        }
    }

    guRotateF(rotation, bottomAngle, -1.0f, 0.0f, 0.0f);
    guMtxCatF(f_FramingRotation, rotation, rotation);
    for (sphereIndex = 0; sphereIndex < f_FramedSphereCount; sphereIndex++) {
        TransformPoint(rotation, f_FramedSphereCenters[sphereIndex], point);
        if (yMax < point[AXIS_Y] + f_FramedSphereRadii[sphereIndex]) {
            yMax = point[AXIS_Y] + f_FramedSphereRadii[sphereIndex];
        }
    }

    if (g_ControllerData[1].trigger & D_CBUTTONS) {
        osSyncPrintf("maxx %f minx %f maxy %f miny %f\n", xMax, xMin, yMax, yMin);
    }

    angle = FramingDegreesToRadians(rightAngle - leftAngle) - FramingDegreesToRadians(90);
    leftDistance = -xMin / cosf(angle);
    eyeX = sinf(FramingDegreesToRadians(leftAngle)) * leftDistance;
    horizontalEyeZ = cosf(FramingDegreesToRadians(leftAngle)) * leftDistance;
    rightDistance = xMax / cosf(angle);
    eyeX += sinf(FramingDegreesToRadians(rightAngle)) * rightDistance;
    horizontalEyeZ += cosf(FramingDegreesToRadians(rightAngle)) * rightDistance;

    angle = FramingDegreesToRadians(topAngle - bottomAngle) - FramingDegreesToRadians(90);
    bottomDistance = -yMin / cosf(angle);
    eyeY = sinf(FramingDegreesToRadians(bottomAngle)) * bottomDistance;
    verticalEyeZ = cosf(FramingDegreesToRadians(bottomAngle)) * bottomDistance;
    topDistance = yMax / cosf(-angle);
    eyeY += sinf(FramingDegreesToRadians(topAngle)) * topDistance;
    verticalEyeZ += cosf(FramingDegreesToRadians(topAngle)) * topDistance;
    if (g_ControllerData[1].trigger & D_CBUTTONS) {
        osSyncPrintf("xd %f yd %f xzd %f yzd %f\n", eyeX, eyeY, horizontalEyeZ, verticalEyeZ);
    }
    eyeZ = horizontalEyeZ;
    if (!(verticalEyeZ <= horizontalEyeZ)) {
        eyeZ = verticalEyeZ;
    }

    guTranslateF(rotation, -eyeX, -eyeY, eyeZ);
    yMin = FAR_EXTENT;
    xMin = yMin;
    yMax = -FAR_EXTENT;
    xMax = yMax;
    guMtxCatF(f_FramingRotation, rotation, rotation);
    for (sphereIndex = 0; sphereIndex < f_FramedSphereCount; sphereIndex++) {
        TransformPoint(rotation, f_FramedSphereCenters[sphereIndex], point);
        if (g_ControllerData[1].trigger & D_CBUTTONS) {
            osSyncPrintf("rZ %f\n", point[AXIS_Z]);
        }
        if (point[AXIS_Z] - f_FramedSphereRadii[sphereIndex] < zMin) {
            zMin = point[AXIS_Z] - f_FramedSphereRadii[sphereIndex];
        }

        if (zMax < point[AXIS_Z] + f_FramedSphereRadii[sphereIndex]) {
            zMax = point[AXIS_Z] + f_FramedSphereRadii[sphereIndex];
        }

        if (point[AXIS_X] - f_FramedSphereRadii[sphereIndex] < xMin) {
            xMin = point[AXIS_X] - f_FramedSphereRadii[sphereIndex];
        }

        if (xMax < point[AXIS_X] + f_FramedSphereRadii[sphereIndex]) {
            xMax = point[AXIS_X] + f_FramedSphereRadii[sphereIndex];
        }

        if (point[AXIS_Y] - f_FramedSphereRadii[sphereIndex] < yMin) {
            yMin = point[AXIS_Y] - f_FramedSphereRadii[sphereIndex];
        }

        if (yMax < point[AXIS_Y] + f_FramedSphereRadii[sphereIndex]) {
            yMax = point[AXIS_Y] + f_FramedSphereRadii[sphereIndex];
        }
    }

    clearEyeZ = margin - zMin + eyeZ;
    right = (clearEyeZ - horizontalEyeZ) * tan(FramingDegreesToRadians(rightAngle));
    left = (clearEyeZ - horizontalEyeZ) * tan(FramingDegreesToRadians(leftAngle));
    top = (clearEyeZ - verticalEyeZ) * tan(FramingDegreesToRadians(topAngle));
    bottom = (clearEyeZ - verticalEyeZ) * tan(FramingDegreesToRadians(bottomAngle));
    centerX = (xMin + xMax) * 0.5f;
    centerY = (yMin + yMax) * 0.5f;
    eyeZ = clearEyeZ;
    if (centerX < left) {
        centerX = left;
    }

    if (right < centerX) {
        centerX = right;
    }

    if (centerY < bottom) {
        centerY = bottom;
    }

    if (top < centerY) {
        centerY = top;
    }

    eyeX += centerX;
    eyeY += centerY;

    guRotateF(pitchRotation, (s32)(f_FramingPitch * RADIANS_TO_DEGREES), 1.0f, 0.0f, 0.0f);
    guRotateF(yawRotation, (s32)(f_FramingYaw * RADIANS_TO_DEGREES), 0.0f, 1.0f, 0.0f);
    guMtxCatF(pitchRotation, yawRotation, orientation);
    depth = depthOffset - eyeZ;
    guMtxXFMF(orientation, eyeX, eyeY, depth, &f_FramedEye[0], &f_FramedEye[1], &f_FramedEye[2]);
    guMtxXFMF(
        orientation,
        eyeX,
        eyeY,
        depth + margin,
        &f_FramedLookAt[0],
        &f_FramedLookAt[1],
        &f_FramedLookAt[2]
    );
    if (g_ControllerData[1].trigger & D_CBUTTONS) {
        osSyncPrintf("px %f py %f pz %f\n", f_FramedEye[0], f_FramedEye[1], f_FramedEye[2]);
        if (g_ControllerData[1].trigger & D_CBUTTONS) {
            osSyncPrintf(
                "tx %f ty %f tz %f\n",
                f_FramedLookAt[0],
                f_FramedLookAt[1],
                f_FramedLookAt[2]
            );
        }
    }
}
