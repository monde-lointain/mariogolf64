#include "common.h"

#include "framing.h"

#include "camera.h"
#include "controller.h"
#include "functions.h"
#include "math_util.h"

/* How many spheres f_FramedSphereCenters has room for */
#define FRAMED_SPHERE_CAPACITY 8

/* A pitch whose cosine is closer to zero than this is turned further */
#define STEEP_PITCH_COSINE 0.1f

/* The pi of these angles' conversions, rounded to six places */
#define FRAMING_PI 3.141593f
#define FRAMING_DEGREES_TO_RADIANS(degrees) ((degrees) * (FRAMING_PI / 180))

/* The starting x and y bounds */
#define FAR_EXTENT 30000.0f

/* How far along z the look-at point lies from the eye */
#define LOOK_DISTANCE 10.0f

/* The starting z bounds, also added to the eye's distance */
#define Z_MARGIN 10.0f

extern f32 f_FramedSphereCenters[FRAMED_SPHERE_CAPACITY][3];
extern f32 f_FramedSphereRadii[FRAMED_SPHERE_CAPACITY];
extern s32 f_FramedSphereCount;
extern f32 f_FramingPitch;
extern f32 f_FramingYaw;
extern Mat4 f_FramingRotation;
extern Vec3 f_FramedEye;
extern Vec3 f_FramedLookAt;

void TransformPoint(Mat4 mf, Vec3 point, Vec3 result);
float fabsf(float x);
double tan(double x);

void GetFramedView(Vec3 eye, Vec3 lookAt) {
  eye[0] = f_FramedEye[0];
  eye[1] = f_FramedEye[1];
  eye[2] = f_FramedEye[2];
  lookAt[0] = f_FramedLookAt[0];
  lookAt[1] = f_FramedLookAt[1];
  lookAt[2] = f_FramedLookAt[2];
}

void ClearFramedSpheres(void) {
  f_FramedSphereCount = 0;
}

/* Adds a sphere for FrameSpheres to keep in view; extra spheres are ignored */
void AddFramedSphere(Vec3 center, f32 radius) {
  if (f_FramedSphereCount < FRAMED_SPHERE_CAPACITY) {
    f_FramedSphereCenters[f_FramedSphereCount][0] = center[0];
    f_FramedSphereCenters[f_FramedSphereCount][1] = center[1];
    f_FramedSphereCenters[f_FramedSphereCount][2] = center[2];
    f_FramedSphereRadii[f_FramedSphereCount] = radius;
    if (g_ControllerData[1].trigger & D_CBUTTONS) {
      osSyncPrintf("ox %f ox %f oz %f\n",
          f_FramedSphereCenters[f_FramedSphereCount][0],
          f_FramedSphereCenters[f_FramedSphereCount][1],
          f_FramedSphereCenters[f_FramedSphereCount][2]);
    }
    f_FramedSphereCount++;
  }
}

/* Sets the direction, in radians, from which FrameSpheres looks */
#ifdef NON_MATCHING
void SetFramingAngles(f32 pitch, f32 yaw) {
  Mat4 pitchRotation;
  Mat4 yawRotation;

  if (fabsf(cosf(pitch)) < STEEP_PITCH_COSINE) {
    pitch += FRAMING_DEGREES_TO_RADIANS(20);
  }
  f_FramingPitch = pitch;
  f_FramingYaw = yaw;
  guRotateF(
      pitchRotation, (s32)(pitch * RADIANS_TO_DEGREES), -1.0f, 0.0f, 0.0f);
  guRotateF(yawRotation, (s32)(yaw * RADIANS_TO_DEGREES), 0.0f, -1.0f, 0.0f);
  guMtxCatF(yawRotation, pitchRotation, f_FramingRotation);
}

#else
INCLUDE_ASM("asm/nonmatchings/framing", SetFramingAngles);
#endif

void TransformPoint(Mat4 mf, Vec3 point, Vec3 result) {
  f32 x = point[0];
  f32 y = point[1];
  f32 z = point[2];

  result[0] = (mf[0][0] * x) + (mf[1][0] * y) + (mf[2][0] * z) + mf[3][0];
  result[1] = (mf[0][1] * x) + (mf[1][1] * y) + (mf[2][1] * z) + mf[3][1];
  result[2] = (mf[0][2] * x) + (mf[1][2] * y) + (mf[2][2] * z) + mf[3][2];
}

/*
 * Places the eye and look-at point so that the first camera's field of view
 * takes in every framed sphere, looking from the framing angles
 */
#ifdef NON_MATCHING
void FrameSpheres(f32 depthOffset) {
  Vec3 point;
  Mat4 rotation;
  Mat4 pitchRotation;
  Mat4 yawRotation;
  Mat4 orientation;
  f32 horizontalEyeZ;
  f32 rightAngle;
  f32 leftAngle;
  f32 topAngle;
  f32 bottomAngle;
  f32 minZ;
  f32 maxZ;
  f32 minX;
  f32 maxX;
  f32 minY;
  f32 maxY;
  f32 angle;
  f32 distance;
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
  f32 radius;
  f64 frontDistance;
  s32 i;

  minY = FAR_EXTENT;
  minX = minY;
  maxY = -FAR_EXTENT;
  maxX = maxY;
  maxZ = -Z_MARGIN;
  rightAngle = g_Cameras[0].view.fieldOfView * 0.5f;
  leftAngle = -rightAngle;
  topAngle = rightAngle * 0.75f;
  bottomAngle = leftAngle * 0.75f;
  minZ = Z_MARGIN;
  if (g_ControllerData[1].trigger & D_CBUTTONS) {
    osSyncPrintf("forv %f\n", g_Cameras[0].view.fieldOfView);
  }

  guRotateF(rotation, rightAngle, 0.0f, 1.0f, 0.0f);
  guMtxCatF(f_FramingRotation, rotation, rotation);
  for (i = 0; i < f_FramedSphereCount; i++) {
    TransformPoint(rotation, f_FramedSphereCenters[i], point);
    if (point[0] - f_FramedSphereRadii[i] < minX) {
      minX = point[0] - f_FramedSphereRadii[i];
    }
  }
  guRotateF(rotation, leftAngle, 0.0f, 1.0f, 0.0f);
  guMtxCatF(f_FramingRotation, rotation, rotation);
  for (i = 0; i < f_FramedSphereCount; i++) {
    TransformPoint(rotation, f_FramedSphereCenters[i], point);
    if (maxX < point[0] + f_FramedSphereRadii[i]) {
      maxX = point[0] + f_FramedSphereRadii[i];
    }
  }
  guRotateF(rotation, topAngle, -1.0f, 0.0f, 0.0f);
  guMtxCatF(f_FramingRotation, rotation, rotation);
  for (i = 0; i < f_FramedSphereCount; i++) {
    TransformPoint(rotation, f_FramedSphereCenters[i], point);
    if (point[1] - f_FramedSphereRadii[i] < minY) {
      minY = point[1] - f_FramedSphereRadii[i];
    }
  }
  guRotateF(rotation, bottomAngle, -1.0f, 0.0f, 0.0f);
  guMtxCatF(f_FramingRotation, rotation, rotation);
  for (i = 0; i < f_FramedSphereCount; i++) {
    TransformPoint(rotation, f_FramedSphereCenters[i], point);
    if (maxY < point[1] + f_FramedSphereRadii[i]) {
      maxY = point[1] + f_FramedSphereRadii[i];
    }
  }
  if (g_ControllerData[1].trigger & D_CBUTTONS) {
    osSyncPrintf("maxx %f minx %f maxy %f miny %f\n", maxX, minX, maxY, minY);
  }

  angle = FRAMING_DEGREES_TO_RADIANS(rightAngle - leftAngle) -
      FRAMING_DEGREES_TO_RADIANS(90);
  distance = -minX / cosf(angle);
  eyeX = sinf(FRAMING_DEGREES_TO_RADIANS(leftAngle)) * distance;
  horizontalEyeZ = cosf(FRAMING_DEGREES_TO_RADIANS(leftAngle)) * distance;
  distance = maxX / cosf(angle);
  eyeX += sinf(FRAMING_DEGREES_TO_RADIANS(rightAngle)) * distance;
  horizontalEyeZ += cosf(FRAMING_DEGREES_TO_RADIANS(rightAngle)) * distance;

  angle = FRAMING_DEGREES_TO_RADIANS(topAngle - bottomAngle) -
      FRAMING_DEGREES_TO_RADIANS(90);
  distance = -minY / cosf(angle);
  eyeY = sinf(FRAMING_DEGREES_TO_RADIANS(bottomAngle)) * distance;
  verticalEyeZ = cosf(FRAMING_DEGREES_TO_RADIANS(bottomAngle)) * distance;
  distance = maxY / cosf(-angle);
  eyeY += sinf(FRAMING_DEGREES_TO_RADIANS(topAngle)) * distance;
  verticalEyeZ += cosf(FRAMING_DEGREES_TO_RADIANS(topAngle)) * distance;
  if (g_ControllerData[1].trigger & D_CBUTTONS) {
    osSyncPrintf("xd %f yd %f xzd %f yzd %f\n", eyeX, eyeY, horizontalEyeZ,
        verticalEyeZ);
  }

  if (verticalEyeZ <= horizontalEyeZ) {
    eyeZ = horizontalEyeZ;
  } else {
    eyeZ = verticalEyeZ;
  }

  minY = FAR_EXTENT;
  minX = minY;
  maxY = -FAR_EXTENT;
  maxX = maxY;
  guTranslateF(rotation, -eyeX, -eyeY, eyeZ);
  guMtxCatF(f_FramingRotation, rotation, rotation);
  for (i = 0; i < f_FramedSphereCount; i++) {
    TransformPoint(rotation, f_FramedSphereCenters[i], point);
    if (g_ControllerData[1].trigger & D_CBUTTONS) {
      osSyncPrintf("rZ %f\n", point[2]);
    }
    radius = f_FramedSphereRadii[i];
    if (point[2] - radius < minZ) {
      minZ = point[2] - radius;
    }
    if (maxZ < point[2] + radius) {
      maxZ = point[2] + radius;
    }
    if (point[0] - radius < minX) {
      minX = point[0] - radius;
    }
    if (maxX < point[0] + radius) {
      maxX = point[0] + radius;
    }
    if (point[1] - radius < minY) {
      minY = point[1] - radius;
    }
    if (maxY < point[1] + radius) {
      maxY = point[1] + radius;
    }
  }

  depth = (Z_MARGIN - minZ) + eyeZ;
  frontDistance = depth - horizontalEyeZ;
  right = frontDistance * tan(FRAMING_DEGREES_TO_RADIANS(rightAngle));
  left = frontDistance * tan(FRAMING_DEGREES_TO_RADIANS(leftAngle));
  frontDistance = depth - verticalEyeZ;
  top = frontDistance * tan(FRAMING_DEGREES_TO_RADIANS(topAngle));
  centerX = (minX + maxX) * 0.5f;
  centerY = (minY + maxY) * 0.5f;
  bottom = frontDistance * tan(FRAMING_DEGREES_TO_RADIANS(bottomAngle));
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

  guRotateF(pitchRotation, (s32)(f_FramingPitch * RADIANS_TO_DEGREES), 1.0f,
      0.0f, 0.0f);
  guRotateF(
      yawRotation, (s32)(f_FramingYaw * RADIANS_TO_DEGREES), 0.0f, 1.0f, 0.0f);
  guMtxCatF(pitchRotation, yawRotation, orientation);
  depth = depthOffset - depth;
  guMtxXFMF(orientation, eyeX, eyeY, depth, &f_FramedEye[0], &f_FramedEye[1],
      &f_FramedEye[2]);
  guMtxXFMF(orientation, eyeX, eyeY, depth + LOOK_DISTANCE, &f_FramedLookAt[0],
      &f_FramedLookAt[1], &f_FramedLookAt[2]);
  if (g_ControllerData[1].trigger & D_CBUTTONS) {
    osSyncPrintf(
        "px %f py %f pz %f\n", f_FramedEye[0], f_FramedEye[1], f_FramedEye[2]);
    if (g_ControllerData[1].trigger & D_CBUTTONS) {
      osSyncPrintf("tx %f ty %f tz %f\n", f_FramedLookAt[0], f_FramedLookAt[1],
          f_FramedLookAt[2]);
    }
  }
}

#else
INCLUDE_ASM("asm/nonmatchings/framing", FrameSpheres);
#endif
