/*
 * Places the first camera so that its view takes in a set of spheres, looking
 * from a given direction.
 */
#include "common.h"

#include "framing.h"

#include "camera.h"
#include "controller.h"
#include "math_util.h"

/* How many spheres f_FramedSphereCenters has room for */
#define FRAMED_SPHERE_CAPACITY 8

/* A pitch whose cosine is closer to zero than this is turned further */
#define STEEP_PITCH_COSINE 0.1f

/* The pi of these angles' conversions, rounded to six places */
#define FRAMING_PI 3.141593f
#define FramingDegreesToRadians(degrees) ((degrees) * (FRAMING_PI / 180))

/* The starting x and y bounds */
#define FAR_EXTENT 30000.0f

/* How far along z the look-at point lies from the eye */
#define LOOK_DISTANCE 10.0f

/* The starting z bounds, also added to the eye's distance */
#define Z_MARGIN 10.0f

static f32 f_FramedSphereCenters[FRAMED_SPHERE_CAPACITY][3];
static f32 f_FramedSphereRadii[FRAMED_SPHERE_CAPACITY];
static s32 f_FramedSphereCount;
static Radians f_FramingPitch;
static Radians f_FramingYaw;
static Mat4 f_FramingRotation;

void TransformPoint(Mat4 mf, Vec3 point, Vec3 result);
float fabsf(float x);
double tan(double x);

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

/* Adds a sphere for FrameSpheres to keep in view; extra spheres are ignored */
void AddFramedSphere(Vec3 center, f32 radius) {
  if (f_FramedSphereCount < FRAMED_SPHERE_CAPACITY) {
    f_FramedSphereCenters[f_FramedSphereCount][0] = center[AXIS_X];
    f_FramedSphereCenters[f_FramedSphereCount][1] = center[AXIS_Y];
    f_FramedSphereCenters[f_FramedSphereCount][2] = center[AXIS_Z];
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

/*
 * Sets the direction, in radians, from which FrameSpheres looks.
 *
 * Doesn't match: GCC loads STEEP_PITCH_COSINE before taking the cosine's
 * magnitude, where the original takes it first. Swapping the comparison or
 * taking the cosine or its magnitude into a local changes nothing, and
 * fabs or a double constant makes it worse.
 */
#ifdef NON_MATCHING
void SetFramingAngles(Radians pitch, Radians yaw) {
  Mat4 pitchRotation;
  Mat4 yawRotation;

  if (fabsf(cosf(pitch)) < STEEP_PITCH_COSINE) {
    pitch += FramingDegreesToRadians(20);
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
  f32 x = point[AXIS_X];
  f32 y = point[AXIS_Y];
  f32 z = point[AXIS_Z];

  result[AXIS_X] = (mf[0][0] * x) + (mf[1][0] * y) + (mf[2][0] * z) + mf[3][0];
  result[AXIS_Y] = (mf[0][1] * x) + (mf[1][1] * y) + (mf[2][1] * z) + mf[3][1];
  result[AXIS_Z] = (mf[0][2] * x) + (mf[1][2] * y) + (mf[2][2] * z) + mf[3][2];
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
  Degrees rightAngle;
  Degrees leftAngle;
  Degrees topAngle;
  Degrees bottomAngle;
  f32 minZ;
  f32 maxZ;
  f32 minX;
  f32 maxX;
  f32 minY;
  f32 maxY;
  Radians angle;
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
  IN_OWN_BLOCK(bottomAngle = leftAngle * 0.75f);
  minZ = Z_MARGIN;
  if (g_ControllerData[1].trigger & D_CBUTTONS) {
    osSyncPrintf("forv %f\n", g_Cameras[0].view.fieldOfView);
  }

  guRotateF(rotation, rightAngle, 0.0f, 1.0f, 0.0f);
  guMtxCatF(f_FramingRotation, rotation, rotation);
  for (i = 0; i < f_FramedSphereCount; i++) {
    TransformPoint(rotation, f_FramedSphereCenters[i], point);
    if (point[AXIS_X] - f_FramedSphereRadii[i] < minX) {
      minX = point[AXIS_X] - f_FramedSphereRadii[i];
    }
  }

  guRotateF(rotation, leftAngle, 0.0f, 1.0f, 0.0f);
  guMtxCatF(f_FramingRotation, rotation, rotation);
  for (i = 0; i < f_FramedSphereCount; i++) {
    TransformPoint(rotation, f_FramedSphereCenters[i], point);
    if (maxX < point[AXIS_X] + f_FramedSphereRadii[i]) {
      maxX = point[AXIS_X] + f_FramedSphereRadii[i];
    }
  }

  guRotateF(rotation, topAngle, -1.0f, 0.0f, 0.0f);
  guMtxCatF(f_FramingRotation, rotation, rotation);
  for (i = 0; i < f_FramedSphereCount; i++) {
    TransformPoint(rotation, f_FramedSphereCenters[i], point);
    if (point[AXIS_Y] - f_FramedSphereRadii[i] < minY) {
      minY = point[AXIS_Y] - f_FramedSphereRadii[i];
    }
  }

  guRotateF(rotation, bottomAngle, -1.0f, 0.0f, 0.0f);
  guMtxCatF(f_FramingRotation, rotation, rotation);
  for (i = 0; i < f_FramedSphereCount; i++) {
    TransformPoint(rotation, f_FramedSphereCenters[i], point);
    if (maxY < point[AXIS_Y] + f_FramedSphereRadii[i]) {
      maxY = point[AXIS_Y] + f_FramedSphereRadii[i];
    }
  }

  if (g_ControllerData[1].trigger & D_CBUTTONS) {
    osSyncPrintf("maxx %f minx %f maxy %f miny %f\n", maxX, minX, maxY, minY);
  }

  angle = FramingDegreesToRadians(rightAngle - leftAngle) -
      FramingDegreesToRadians(90);
  distance = -minX / cosf(angle);
  eyeX = sinf(FramingDegreesToRadians(leftAngle)) * distance;
  horizontalEyeZ = cosf(FramingDegreesToRadians(leftAngle)) * distance;
  distance = maxX / cosf(angle);
  eyeX += sinf(FramingDegreesToRadians(rightAngle)) * distance;
  horizontalEyeZ += cosf(FramingDegreesToRadians(rightAngle)) * distance;

  angle = FramingDegreesToRadians(topAngle - bottomAngle) -
      FramingDegreesToRadians(90);
  distance = -minY / cosf(angle);
  eyeY = sinf(FramingDegreesToRadians(bottomAngle)) * distance;
  IN_OWN_BLOCK(
      verticalEyeZ = cosf(FramingDegreesToRadians(bottomAngle)) * distance);
  distance = maxY / cosf(-angle);
  eyeY += sinf(FramingDegreesToRadians(topAngle)) * distance;
  verticalEyeZ += cosf(FramingDegreesToRadians(topAngle)) * distance;
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
      osSyncPrintf("rZ %f\n", point[AXIS_Z]);
    }

    radius = f_FramedSphereRadii[i];
    if (point[AXIS_Z] - radius < minZ) {
      minZ = point[AXIS_Z] - radius;
    }

    if (maxZ < point[AXIS_Z] + radius) {
      maxZ = point[AXIS_Z] + radius;
    }

    if (point[AXIS_X] - radius < minX) {
      minX = point[AXIS_X] - radius;
    }

    if (maxX < point[AXIS_X] + radius) {
      maxX = point[AXIS_X] + radius;
    }

    if (point[AXIS_Y] - radius < minY) {
      minY = point[AXIS_Y] - radius;
    }

    if (maxY < point[AXIS_Y] + radius) {
      maxY = point[AXIS_Y] + radius;
    }
  }

  depth = (Z_MARGIN - minZ) + eyeZ;
  frontDistance = depth - horizontalEyeZ;
  IN_OWN_BLOCK(
      right = frontDistance * tan(FramingDegreesToRadians(rightAngle)));
  left = frontDistance * tan(FramingDegreesToRadians(leftAngle));
  frontDistance = depth - verticalEyeZ;
  top = frontDistance * tan(FramingDegreesToRadians(topAngle));
  centerX = (minX + maxX) * 0.5f;
  centerY = (minY + maxY) * 0.5f;
  bottom = frontDistance * tan(FramingDegreesToRadians(bottomAngle));
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
