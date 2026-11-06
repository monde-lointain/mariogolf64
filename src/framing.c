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
 * Doesn't match: the original takes the cosine's magnitude into $f2, where
 * local-alloc gives it the $f0 cosf returns it in. A cosine local with a block
 * ending after it gives the original's order of instructions, where they
 * took 7 words differing, 3 in shape. Taking the magnitude into the cosine
 * local, before or after the block's end, or swapping the comparison doesn't
 * help. The register oracle finds nothing closer: forcing the magnitude's
 * pseudo into $f2 gives 16 words, and an early-clobber output ("=&f") in
 * fabsf's assembly 20 words, none in shape. 6 words differ.
 *
 * Braces in place of the empty block, around the cosine and its test, with the
 * cosine declared in them or not, or around the test alone or with a magnitude
 * local, give 7 words (3 in shape), as without the block, since braces end no
 * basic block; so does testing cosf's result directly, with no cosine local.
 */
#ifdef NON_MATCHING
void SetFramingAngles(Radians pitch, Radians yaw) {
  Mat4 pitchRotation;
  Mat4 yawRotation;
  f32 cosine;

  cosine = cosf(pitch);
  /* Ended here, as without it 7 words differ, 3 in shape, against 6 and 0 */
  END_BASIC_BLOCK();
  if (fabsf(cosine) < STEEP_PITCH_COSINE) {
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
 *
 * Doesn't match: the original's frame is 8 bytes larger, it keeps
 * horizontalEyeZ on the stack where ours keeps it in $fp, and its float
 * registers differ from the first eye distance on. The register oracle, forcing
 * 7 pseudos, still leaves 221 words. The original negates 10.0 at run time for
 * maxZ, so maxZ is set from minZ after the block that ends at bottomAngle,
 * where cse no longer knows minZ's value. Written -Z_MARGIN the negation folded
 * (282 words, 108 in shape, when the draft gave 279), as it did from minZ in
 * the same block (291, 117); setting minZ just before the block's end gave 285.
 * The permuter's best (269 words, 89 in shape) set eyeZ from horizontalEyeZ
 * before working it out. Setting eyeZ to the horizontal distance first and
 * raising it to the vertical one gave 279 to 332. The vertical distance to the
 * depth is written into the top and bottom edges' products, where a double
 * verticalFrontDistance held it, which took the draft from 279 words differing
 * to 269, 109 in shape to 113, and the oracle from 227 words, forcing 12, to
 * 225; as a float variable it gave 281, and writing frontDistance into its two
 * uses as well 282. Each sphere's radius is read from f_FramedSphereRadii at
 * each of its twelve uses, rather than through a radius local, which took the
 * draft from 269 words differing to 265, 113 in shape either way, and the
 * oracle from 225 words to 221, 7 forced either way. Computing both centers
 * after the four edges, keeping the horizontal front distance as f32, and
 * giving the right edge its own variable reduces 265 words (113 in shape) to
 * 228 (66). The original also computes the centers after its fourth tan call.
 * The front-distance subtraction is single precision before promotion in
 * either form; a double local changes which value lives across the calls.
 * Inlining that subtraction at both uses gives the same 228 words.
 */
#ifdef NON_MATCHING
void FrameSpheres(f32 depthOffset) {
  f32 maxZ;
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
  f32 minX;
  f32 maxX;
  f32 minY;
  f32 maxY;
  Radians angle;
  f32 distance;
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
  f32 frontDistance;
  s32 i;
  f32 rightRadians;

  minZ = Z_MARGIN;
  minY = FAR_EXTENT;
  minX = minY;
  maxY = -FAR_EXTENT;
  maxX = maxY;
  rightAngle = g_Cameras[0].view.fieldOfView * 0.5f;
  leftAngle = -rightAngle;
  topAngle = rightAngle * 0.75f;
  /*
   * In its own block, as without it 241 words differ, 69 in shape, against 228
   * and 66
   */
  IN_OWN_BLOCK(bottomAngle = leftAngle * 0.75f);
  maxZ = -minZ;
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
  bottomDistance = -minY / cosf(angle);
  eyeY = sinf(FramingDegreesToRadians(bottomAngle)) * bottomDistance;
  verticalEyeZ = cosf(FramingDegreesToRadians(bottomAngle)) * bottomDistance;
  topDistance = maxY / cosf(-angle);
  eyeY += sinf(FramingDegreesToRadians(topAngle)) * topDistance;
  verticalEyeZ += cosf(FramingDegreesToRadians(topAngle)) * topDistance;
  if (g_ControllerData[1].trigger & D_CBUTTONS) {
    osSyncPrintf("xd %f yd %f xzd %f yzd %f\n", eyeX, eyeY, horizontalEyeZ,
        verticalEyeZ);
  }

  if (verticalEyeZ <= horizontalEyeZ) {
    eyeZ = horizontalEyeZ;
  } else {
    eyeZ = verticalEyeZ;
  }

  guTranslateF(rotation, -eyeX, -eyeY, eyeZ);
  minY = FAR_EXTENT;
  minX = minY;
  maxY = -FAR_EXTENT;
  maxX = maxY;
  guMtxCatF(f_FramingRotation, rotation, rotation);
  for (i = 0; i < f_FramedSphereCount; i++) {
    TransformPoint(rotation, f_FramedSphereCenters[i], point);
    if (g_ControllerData[1].trigger & D_CBUTTONS) {
      osSyncPrintf("rZ %f\n", point[AXIS_Z]);
    }
    if (point[AXIS_Z] - f_FramedSphereRadii[i] < minZ) {
      minZ = point[AXIS_Z] - f_FramedSphereRadii[i];
    }

    if (maxZ < point[AXIS_Z] + f_FramedSphereRadii[i]) {
      maxZ = point[AXIS_Z] + f_FramedSphereRadii[i];
    }

    if (point[AXIS_X] - f_FramedSphereRadii[i] < minX) {
      minX = point[AXIS_X] - f_FramedSphereRadii[i];
    }

    if (maxX < point[AXIS_X] + f_FramedSphereRadii[i]) {
      maxX = point[AXIS_X] + f_FramedSphereRadii[i];
    }

    if (point[AXIS_Y] - f_FramedSphereRadii[i] < minY) {
      minY = point[AXIS_Y] - f_FramedSphereRadii[i];
    }

    if (maxY < point[AXIS_Y] + f_FramedSphereRadii[i]) {
      maxY = point[AXIS_Y] + f_FramedSphereRadii[i];
    }
  }

  depth = (Z_MARGIN - minZ) + eyeZ;
  frontDistance = depth - horizontalEyeZ;
  rightRadians = FramingDegreesToRadians(rightAngle);
  right = frontDistance * tan(rightRadians);
  left = frontDistance * tan(FramingDegreesToRadians(leftAngle));
  top = (depth - verticalEyeZ) * tan(FramingDegreesToRadians(topAngle));
  bottom = (depth - verticalEyeZ) * tan(FramingDegreesToRadians(bottomAngle));
  centerX = (minX + maxX) * 0.5f;
  centerY = (minY + maxY) * 0.5f;
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

  eyeY += centerY;
  eyeX += centerX;

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
