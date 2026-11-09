/*
 * Places the first camera so that its view takes in a set of spheres, looking
 * from a given direction.
 */
#include "framing.h"

#include <math.h>
#include <ultra64.h>

#include "camera.h"
#include "compiler.h"
#include "controller.h"
#include "geometry.h"
#include "include_asm.h"
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

/* Sets the direction, in radians, from which FrameSpheres looks */
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
 * Doesn't match: the original's frame is 8 bytes larger. It spills
 * horizontalEyeZ, which ours keeps in $fp, and gives $fp to eyeZ, which ours
 * keeps in $f26; global-alloc ranks eyeZ (9 uses across 147 insns) above
 * right and verticalEyeZ, so it takes a float register they would have
 * taken. Forcing eyeZ into $fp, and horizontalEyeZ and the 10.0 constant
 * onto the stack, leaves 46 words in the register oracle, where the
 * best forcing of hard registers alone leaves 204.
 *
 * The spill slots are given out in declaration order, and the original's
 * hold the four angles, minZ and then maxZ, so maxZ is declared after minZ.
 * With the forcing above, the following forms took the oracle from 126
 * words to 46. eyeZ starts as horizontalEyeZ and takes verticalEyeZ when
 * that is not at most it, as the original's c.le.s tests; with if and else
 * 62 words differ, with < or > 50, and as a conditional expression 68. The
 * margin is added to eyeZ itself, from which the edges' distances are
 * measured and depth worked out, as the original's $fp holds the eye
 * distance after the margin; with a depth variable for the sum and the
 * eye's depth worked out into it, 64 words differ, or 47 into eyeZ.
 *
 * The original negates 10.0 at run time for maxZ, so maxZ is set from minZ
 * after the block that ends at bottomAngle, where cse no longer knows
 * minZ's value; set in the first block, maxZ shares minZ's load of 10.0,
 * where the original loads it twice, and 55 words differ under the forcing.
 * Written -Z_MARGIN the negation folded (282 words, 108 in shape, when the
 * draft gave 279). The permuter's best (269 words, 89 in shape) set eyeZ
 * from horizontalEyeZ before working it out. The vertical distance to the
 * depth is written into the top and bottom edges' products, and each
 * sphere's radius is read from f_FramedSphereRadii at each of its twelve
 * uses, rather than through a radius local, each a few words closer.
 * Computing both centers after the four edges, keeping the horizontal
 * front distance as f32, and giving the right edge its own variable took
 * the draft from 265 words (113 in shape) to 228 (66); the original also
 * computes the centers after its fourth tan call. The left and right
 * edges' eye distances have their own leftDistance and rightDistance, as
 * the top and bottom edges' do, which took it to 204 (66).
 *
 * The eye's depth starts from the horizontal fit's once the vertical
 * angle is known, as a sweep moving each assignment found, where the draft
 * set it just before comparing the two fits. That takes the draft from 258
 * words (54 in shape) to 250 (56), and the register oracle's closest from
 * 258 to 250, with nothing forced; set right after the horizontal fit it
 * gives 255 (63).
 *
 * Without the spill, every offset into the frame differs. That left 250
 * words, 56 in shape.
 *
 * Copying verticalEyeZ into top before working the top edge out from it,
 * which a search splitting assignments into steps found, gives 157
 * words, 53 in shape, with the original's frame, and the register
 * oracle's closest falls from 250 words to 157, forcing nothing. The
 * copy gives top 5 references where it had 3, raising its global-alloc
 * priority from 1428 to 4347, and horizontalEyeZ then spills as in the
 * original; copying into bottom changes nothing. The copy stands for
 * nothing, and the natural forms tried don't bring it: a local for the
 * vertical distance, set beside frontDistance or before the top edge,
 * gives 250 words, the distance worked out into verticalEyeZ 258, the
 * top edge in two statements 275 to 486, the top angle's radians in
 * angle 190 (84 in shape), and every declaration order of the eye
 * distances and edges 250. 250 words differ, 56 in shape.
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
     * In its own block, as without it 259 words differ, 57 in shape, against 258
     * and 54, and 55 against 46 under the note's forcing
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

    angle = FramingDegreesToRadians(rightAngle - leftAngle) - FramingDegreesToRadians(90);
    leftDistance = -minX / cosf(angle);
    eyeX = sinf(FramingDegreesToRadians(leftAngle)) * leftDistance;
    horizontalEyeZ = cosf(FramingDegreesToRadians(leftAngle)) * leftDistance;
    rightDistance = maxX / cosf(angle);
    eyeX += sinf(FramingDegreesToRadians(rightAngle)) * rightDistance;
    horizontalEyeZ += cosf(FramingDegreesToRadians(rightAngle)) * rightDistance;

    angle = FramingDegreesToRadians(topAngle - bottomAngle) - FramingDegreesToRadians(90);
    eyeZ = horizontalEyeZ;
    bottomDistance = -minY / cosf(angle);
    eyeY = sinf(FramingDegreesToRadians(bottomAngle)) * bottomDistance;
    verticalEyeZ = cosf(FramingDegreesToRadians(bottomAngle)) * bottomDistance;
    topDistance = maxY / cosf(-angle);
    eyeY += sinf(FramingDegreesToRadians(topAngle)) * topDistance;
    verticalEyeZ += cosf(FramingDegreesToRadians(topAngle)) * topDistance;
    if (g_ControllerData[1].trigger & D_CBUTTONS) {
        osSyncPrintf("xd %f yd %f xzd %f yzd %f\n", eyeX, eyeY, horizontalEyeZ, verticalEyeZ);
    }
    if (!(verticalEyeZ <= horizontalEyeZ)) {
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

    eyeZ += Z_MARGIN - minZ;
    frontDistance = eyeZ - horizontalEyeZ;
    rightRadians = FramingDegreesToRadians(rightAngle);
    right = frontDistance * tan(rightRadians);
    left = frontDistance * tan(FramingDegreesToRadians(leftAngle));
    top = (eyeZ - verticalEyeZ) * tan(FramingDegreesToRadians(topAngle));
    bottom = (eyeZ - verticalEyeZ) * tan(FramingDegreesToRadians(bottomAngle));
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
        depth + LOOK_DISTANCE,
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

#else
INCLUDE_ASM("asm/nonmatchings/framing", FrameSpheres);
#endif
