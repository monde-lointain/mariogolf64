/*
 * A copy of the camera's projection and viewing matrices kept on the CPU, so
 * the game can work out where points land on the screen without asking the RSP.
 * The file also builds and multiplies matrices and converts them to the RSP's
 * fixed point.
 */

#include "camera/view_projection.h"

#include <ultra64.h>

#include "math/geometry.h"
#include "math/math_util.h"
#include "screen/screen.h"
#include "system/compiler.h"
#include "system/memory_address.h"

// The screen depth of the far plane over two, in a viewport's depth units
#define SCREEN_DEPTH_SCALE 16352.0f
// Screen coordinates are clamped to this either side of the center
#define SCREEN_COORDINATE_LIMIT 0x400

static Mat4 f_CpuProjectionMatrix;
static Mat4 f_CpuViewingMatrix;
// What a unit at a view depth of one spans on the screen
static f32 f_CpuScreenScaleX;
static f32 f_CpuScreenScaleY;
static s32 f_CpuViewportCenterX;
static s32 f_CpuViewportCenterY;
// The viewport's size over the screen's
static f32 f_CpuViewportWidthRatio;
static f32 f_CpuViewportHeightRatio;
static u8 f_UnusedBufferAfterViewportRatio[0x80] UNUSED;

void SetCpuProjection(Mtx *projection, Mtx *viewing, Vp *viewport) {
    f32 reciprocal;

    if (projection != NULL) {
        guMtxL2F(f_CpuProjectionMatrix, projection);
    }

    if (viewing != NULL) {
        guMtxL2F(f_CpuViewingMatrix, viewing);
    }

    if (viewport != NULL) {
        f_CpuViewportWidthRatio = (viewport->vp.vscale[0] >> 1) * (1.0f / SCREEN_WIDTH);
        f_CpuViewportHeightRatio = (viewport->vp.vscale[1] >> 1) * (1.0f / SCREEN_HEIGHT);
        f_CpuViewportCenterX = viewport->vp.vtrans[0] >> 2;
        f_CpuViewportCenterY = viewport->vp.vtrans[1] >> 2;
    }

    if ((s32)f_CpuProjectionMatrix[3][0] != 0) {
        reciprocal = 1.0f / f_CpuProjectionMatrix[3][3];
        f_CpuProjectionMatrix[0][0] *= reciprocal;
        f_CpuProjectionMatrix[0][1] *= reciprocal;
        f_CpuProjectionMatrix[0][2] *= reciprocal;
        f_CpuProjectionMatrix[0][3] *= reciprocal;
        f_CpuProjectionMatrix[1][0] *= reciprocal;
        f_CpuProjectionMatrix[1][1] *= reciprocal;
        f_CpuProjectionMatrix[1][2] *= reciprocal;
        f_CpuProjectionMatrix[1][3] *= reciprocal;
        f_CpuProjectionMatrix[2][0] *= reciprocal;
        f_CpuProjectionMatrix[2][1] *= reciprocal;
        f_CpuProjectionMatrix[2][2] *= reciprocal;
        f_CpuProjectionMatrix[2][3] *= reciprocal;
        f_CpuProjectionMatrix[3][0] *= reciprocal;
        f_CpuProjectionMatrix[3][1] *= reciprocal;
        f_CpuProjectionMatrix[3][2] *= reciprocal;
        f_CpuProjectionMatrix[3][3] *= reciprocal;
        f_CpuScreenScaleX = 1.0f;
        f_CpuScreenScaleY = -1.0f;
    }
    else {
        f_CpuScreenScaleX = f_CpuProjectionMatrix[0][0] * -(SCREEN_WIDTH / 2.0f);
        f_CpuScreenScaleY = f_CpuProjectionMatrix[1][1] * (SCREEN_HEIGHT / 2.0f);
    }
}

void SetCpuProjectionF(Mat4 projection, Mat4 viewing, Vp *viewport) {
    s32 row;
    s32 column;
    f32 reciprocal;

    for (row = 0; row != 4; row++) {
        for (column = 0; column != 4; column++) {
            f_CpuProjectionMatrix[row][column] = projection[row][column];
            f_CpuViewingMatrix[row][column] = viewing[row][column];
        }
    }

    if (viewport != NULL) {
        f_CpuViewportWidthRatio = (viewport->vp.vscale[0] >> 1) * (1.0f / SCREEN_WIDTH);
        f_CpuViewportHeightRatio = (viewport->vp.vscale[1] >> 1) * (1.0f / SCREEN_HEIGHT);
        f_CpuViewportCenterX = viewport->vp.vtrans[0] >> 2;
        f_CpuViewportCenterY = viewport->vp.vtrans[1] >> 2;
    }

    if ((s32)f_CpuProjectionMatrix[3][0] != 0) {
        reciprocal = 1.0f / f_CpuProjectionMatrix[3][3];
        f_CpuProjectionMatrix[0][0] *= reciprocal;
        f_CpuProjectionMatrix[0][1] *= reciprocal;
        f_CpuProjectionMatrix[0][2] *= reciprocal;
        f_CpuProjectionMatrix[0][3] *= reciprocal;
        f_CpuProjectionMatrix[1][0] *= reciprocal;
        f_CpuProjectionMatrix[1][1] *= reciprocal;
        f_CpuProjectionMatrix[1][2] *= reciprocal;
        f_CpuProjectionMatrix[1][3] *= reciprocal;
        f_CpuProjectionMatrix[2][0] *= reciprocal;
        f_CpuProjectionMatrix[2][1] *= reciprocal;
        f_CpuProjectionMatrix[2][2] *= reciprocal;
        f_CpuProjectionMatrix[2][3] *= reciprocal;
        f_CpuProjectionMatrix[3][0] *= reciprocal;
        f_CpuProjectionMatrix[3][1] *= reciprocal;
        f_CpuProjectionMatrix[3][2] *= reciprocal;
        f_CpuProjectionMatrix[3][3] *= reciprocal;
        f_CpuScreenScaleX = 1.0f;
        f_CpuScreenScaleY = -1.0f;
    }
    else {
        // Overwritten at once
        f_CpuScreenScaleX = -f_CpuViewportCenterX * f_CpuProjectionMatrix[0][0];
        f_CpuScreenScaleY = f_CpuViewportCenterY * f_CpuProjectionMatrix[1][1];
        f_CpuScreenScaleX =
            f_CpuProjectionMatrix[0][0] * (SCREEN_WIDTH / 2.0f) / f_CpuProjectionMatrix[2][3];
        f_CpuScreenScaleY =
            f_CpuProjectionMatrix[1][1] * -(SCREEN_HEIGHT / 2.0f) / f_CpuProjectionMatrix[2][3];
    }
}

// Returns SPHERE_BEHIND_CAMERA if the sphere is not in front of the camera,
// SPHERE_PAST_SIDE if it is off the left or right of the screen,
// SPHERE_PAST_TOP_OR_BOTTOM if it is off the top or bottom, or else its view
// depth, which is negative. The side of the sphere nearest the screen's
// center is the one tested against each edge.
s32 GetSphereVisibility(f32 x, f32 y, f32 z, f32 radius) {
    f32 depth;
    f32 viewX;
    f32 viewY;
    f32 perspective;
    // The screen offset of the side tested
    f32 screenEdge;

    depth = (f_CpuViewingMatrix[0][2] * x) + (f_CpuViewingMatrix[1][2] * y) +
        (f_CpuViewingMatrix[2][2] * z) + f_CpuViewingMatrix[3][2];
    // The depth of the sphere's nearest point
    depth -= radius;
    if ((s32)depth >= 0) {
        return SPHERE_BEHIND_CAMERA;
    }

    viewX = (f_CpuViewingMatrix[0][0] * x) + (f_CpuViewingMatrix[1][0] * y) +
        (f_CpuViewingMatrix[2][0] * z) + f_CpuViewingMatrix[3][0];
    if ((s32)f_CpuProjectionMatrix[3][3] == 1) {
        perspective = 1.0f;
    }
    else {
        perspective = 1.0f / depth;
    }

    if (viewX > 0.0f) {
        viewX -= radius;
        screenEdge = f_CpuScreenScaleX * viewX * perspective;
        if (screenEdge > f_CpuViewportCenterX) {
            return SPHERE_PAST_SIDE;
        }
    }
    else {
        viewX += radius;
        screenEdge = f_CpuScreenScaleX * viewX * perspective;
        if (screenEdge < -f_CpuViewportCenterX) {
            return SPHERE_PAST_SIDE;
        }
    }

    // The screen's y runs down, opposite the view's
    viewY = (f_CpuViewingMatrix[0][1] * x) + (f_CpuViewingMatrix[1][1] * y) +
        (f_CpuViewingMatrix[2][1] * z) + f_CpuViewingMatrix[3][1];
    if (viewY > 0.0f) {
        viewY -= radius;
        screenEdge = f_CpuScreenScaleY * viewY * perspective;
        if (screenEdge < -f_CpuViewportCenterY) {
            return SPHERE_PAST_TOP_OR_BOTTOM;
        }
    }
    else {
        viewY += radius;
        screenEdge = f_CpuScreenScaleY * viewY * perspective;
        if (screenEdge > f_CpuViewportCenterY) {
            return SPHERE_PAST_TOP_OR_BOTTOM;
        }
    }

    return depth;
}

s32 GetViewDepth(f32 x, f32 y, f32 z) {
    return (f_CpuViewingMatrix[0][2] * x) + (f_CpuViewingMatrix[1][2] * y) +
        (f_CpuViewingMatrix[2][2] * z) + f_CpuViewingMatrix[3][2];
}

// Projects a point onto the screen, filling in its position, scale and depth.
// Returns the view depth, which is negative when the point is in front of the
// camera.
s32 ProjectPoint(f32 x, f32 y, f32 z, ScreenPoint *point) {
    f32 viewX;
    f32 viewY;
    f32 viewZ;
    f32 perspective;
    s32 depth;

    guMtxXFMF(f_CpuViewingMatrix, x, y, z, &viewX, &viewY, &viewZ);
    if ((s32)viewZ >= 0) {
        return viewZ;
    }

    perspective = 1.0f / viewZ;
    point->scaleX = f_CpuScreenScaleX * perspective;
    point->scaleY = f_CpuScreenScaleY * perspective;
    if ((s32)f_CpuProjectionMatrix[3][3] == 1) {
        point->x = (s32)viewX + f_CpuViewportCenterX;
        point->y = (s32)-viewY + f_CpuViewportCenterY;
    }
    else {
        point->x = (s32)(point->scaleX * viewX * f_CpuViewportWidthRatio) + f_CpuViewportCenterX;
        point->y = (s32)(point->scaleY * viewY * f_CpuViewportHeightRatio) + f_CpuViewportCenterY;
    }

    if (point->x < -SCREEN_COORDINATE_LIMIT) {
        point->x = -SCREEN_COORDINATE_LIMIT;
    }

    if (point->x > SCREEN_COORDINATE_LIMIT) {
        point->x = SCREEN_COORDINATE_LIMIT;
    }

    if (point->y < -SCREEN_COORDINATE_LIMIT) {
        point->y = -SCREEN_COORDINATE_LIMIT;
    }

    if (point->y > SCREEN_COORDINATE_LIMIT) {
        point->y = SCREEN_COORDINATE_LIMIT;
    }

    depth = ((-f_CpuProjectionMatrix[2][2] - (f_CpuProjectionMatrix[3][2] / viewZ)) *
                SCREEN_DEPTH_SCALE) +
        SCREEN_DEPTH_SCALE;
    point->depth = depth;
    point->depthStep = fabsf(
        ((((f_CpuProjectionMatrix[2][2] * (viewZ - 1.0f)) + f_CpuProjectionMatrix[3][2]) /
             (1.0f - viewZ)) *
            SCREEN_DEPTH_SCALE) +
        SCREEN_DEPTH_SCALE - depth
    );
    return viewZ;
}

// Like ProjectPoint, without the screen depth. The viewing matrix's [1][0]
// is left out of x, unlike in y.
s32 ProjectPointToScreen(f32 x, f32 y, f32 z, ScreenPoint *point) {
    f32 depth;
    f32 viewX;
    f32 viewY;
    f32 perspective;

    depth = viewY = (f_CpuViewingMatrix[0][2] * x) + (f_CpuViewingMatrix[1][2] * y) +
        (f_CpuViewingMatrix[2][2] * z) + f_CpuViewingMatrix[3][2];
    if ((s32)depth == 0) {
        return 0;
    }

    viewX =
        (f_CpuViewingMatrix[0][0] * x) + (f_CpuViewingMatrix[2][0] * z) + f_CpuViewingMatrix[3][0];
    viewY = (f_CpuViewingMatrix[0][1] * x) + (f_CpuViewingMatrix[1][1] * y) +
        (f_CpuViewingMatrix[2][1] * z) + f_CpuViewingMatrix[3][1];
    perspective = 1.0f / depth;
    point->scaleX = f_CpuScreenScaleX * perspective;
    point->scaleY = f_CpuScreenScaleY * perspective;
    point->x = (s32)(point->scaleX * viewX) + f_CpuViewportCenterX;
    point->y = (s32)(point->scaleY * viewY) + f_CpuViewportCenterY;
    if (point->x < -SCREEN_COORDINATE_LIMIT) {
        point->x = -SCREEN_COORDINATE_LIMIT;
    }

    if (point->x > SCREEN_COORDINATE_LIMIT) {
        point->x = SCREEN_COORDINATE_LIMIT;
    }

    if (point->y < -SCREEN_COORDINATE_LIMIT) {
        point->y = -SCREEN_COORDINATE_LIMIT;
    }

    if (point->y > SCREEN_COORDINATE_LIMIT) {
        point->y = SCREEN_COORDINATE_LIMIT;
    }

    return depth;
}

// Like BuildTransformMatrix, without a scale
void BuildRotationMatrix(Mat4 matrix, Vec3 rotation, Vec3 translation) {
    f32 sinX = sinf(rotation[AXIS_X]);
    f32 cosX = cosf(rotation[AXIS_X]);
    f32 sinY = sinf(rotation[AXIS_Y]);
    f32 cosY = cosf(rotation[AXIS_Y]);
    f32 sinZ = sinf(rotation[AXIS_Z]);
    f32 cosZ = cosf(rotation[AXIS_Z]);

    matrix[0][0] = cosY * cosZ;
    matrix[0][1] = cosY * sinZ;
    matrix[0][2] = -sinY;
    matrix[0][3] = 0;
    matrix[1][0] = (sinX * sinY * cosZ) - (cosX * sinZ);
    matrix[1][1] = (sinX * sinY * sinZ) + (cosX * cosZ);
    matrix[1][2] = sinX * cosY;
    matrix[1][3] = 0;
    matrix[2][0] = (cosX * sinY * cosZ) + (sinX * sinZ);
    matrix[2][1] = (cosX * sinY * sinZ) - (sinX * cosZ);
    matrix[2][2] = cosX * cosY;
    matrix[2][3] = 0;
    matrix[3][0] = translation[AXIS_X];
    matrix[3][1] = translation[AXIS_Y];
    matrix[3][2] = translation[AXIS_Z];
    matrix[3][3] = 1.0f;
}

void BuildTransformMatrix(Mat4 matrix, Vec3 rotation, Vec3 translation, Vec3 scale) {
    f32 sinX = sinf(rotation[0]);
    f32 cosX = cosf(rotation[0]);
    f32 sinY = sinf(rotation[1]);
    f32 cosY = cosf(rotation[1]);
    f32 sinZ = sinf(rotation[2]);
    f32 cosZ = cosf(rotation[2]);

    matrix[0][0] = cosY * cosZ * scale[0];
    matrix[0][1] = cosY * sinZ * scale[0];
    matrix[0][2] = -sinY * scale[0];
    matrix[0][3] = 0;
    matrix[1][0] = ((sinX * sinY * cosZ) - (cosX * sinZ)) * scale[1];
    matrix[1][1] = ((sinX * sinY * sinZ) + (cosX * cosZ)) * scale[1];
    matrix[1][2] = sinX * cosY * scale[1];
    matrix[1][3] = 0;
    matrix[2][0] = ((cosX * sinY * cosZ) + (sinX * sinZ)) * scale[2];
    matrix[2][1] = ((cosX * sinY * sinZ) - (sinX * cosZ)) * scale[2];
    matrix[2][2] = cosX * cosY * scale[2];
    matrix[2][3] = 0;
    matrix[3][0] = translation[0];
    matrix[3][1] = translation[1];
    matrix[3][2] = translation[2];
    matrix[3][3] = 1.0f;
}

void MultiplyMatrices(Mat4 a, Mat4 b, Mat4 product) {
    s32 row;
    s32 column;

    for (row = 0; row != 4; row++) {
        for (column = 0; column != 4; column++) {
            product[row][column] = (a[row][0] * b[0][column]) + (a[row][1] * b[1][column]) +
                (a[row][2] * b[2][column]) + (a[row][3] * b[3][column]);
        }
    }
}

// Mtx keeps the integer halves of all sixteen elements before the fractions
void ConvertMatrixToFixed(Mat4 matrix, Mtx *fixedMatrix) {
    s32 *integerPart = (s32 *)fixedMatrix;
    s32 *fractionPart = integerPart + 8;
    s32 row;
    s32 pairIndex;
    s32 first;
    s32 second;

    for (row = 0; row != 4; row++) {
        for (pairIndex = 0; pairIndex != 2; pairIndex++) {
            first = matrix[row][pairIndex * 2] * 65536.0f;
            second = matrix[row][(pairIndex * 2) + 1] * 65536.0f;
            *integerPart++ = (first & 0xFFFF0000) | ((u32)second >> 16);
#ifdef AVOID_UB
            // A fraction of a half or more, a whole part or a negative value
            // shifts past or into the sign bit, which C leaves undefined
            *fractionPart++ = ((u32)first << 16) | (second & 0xFFFF);
#else
            *fractionPart++ = (first << 16) | (second & 0xFFFF);
#endif
        }
    }
}

// No code calls this
void SetTriangleNormal(
    Vtx *vertices,
    s32 firstIndex,
    s32 secondIndex,
    s32 thirdIndex,
    s32 placement
) {
    Vtx_tn *first = (Vtx_tn *)((firstIndex * sizeof(Vtx)) + AddressNumber(vertices));
    Vtx_tn *second = (Vtx_tn *)((secondIndex * sizeof(Vtx)) + AddressNumber(vertices));
    Vtx_tn *third = (Vtx_tn *)((thirdIndex * sizeof(Vtx)) + AddressNumber(vertices));
    s32 firstEdgeX = second->ob[AXIS_X] - first->ob[AXIS_X];
    s32 firstEdgeY = -first->ob[AXIS_Y] + second->ob[AXIS_Y];
    s32 firstEdgeZ = second->ob[AXIS_Z] - first->ob[AXIS_Z];
    s32 secondEdgeX = third->ob[AXIS_X] - second->ob[AXIS_X];
    s32 secondEdgeY = third->ob[AXIS_Y] - second->ob[AXIS_Y];
    s32 secondEdgeZ = third->ob[AXIS_Z] - second->ob[AXIS_Z];
    f32 normalX = (firstEdgeY * secondEdgeZ) - (firstEdgeZ * secondEdgeY);
    f32 normalY = (firstEdgeZ * secondEdgeX) - (firstEdgeX * secondEdgeZ);
    f32 normalZ = (firstEdgeX * secondEdgeY) - (firstEdgeY * secondEdgeX);
    f32 scale =
        NORMAL_LENGTH / sqrtf((normalX * normalX) + (normalY * normalY) + (normalZ * normalZ));
    s8 scaledX;
    s8 scaledY;
    s8 scaledZ;

    switch (placement) {
        case NORMAL_ON_FIRST:
            first->n[0] = normalX * scale;
            first->n[1] = normalY * scale;
            first->n[2] = normalZ * scale;
            break;
        case NORMAL_ON_SECOND:
            second->n[0] = normalX * scale;
            second->n[1] = normalY * scale;
            second->n[2] = normalZ * scale;
            break;
        case NORMAL_ON_THIRD:
            third->n[0] = normalX * scale;
            third->n[1] = normalY * scale;
            third->n[2] = normalZ * scale;
            break;
        case NORMAL_ON_ALL:
            scaledX = normalX * scale;
            scaledY = normalY * scale;
            scaledZ = normalZ * scale;
            first->n[0] = scaledX;
            first->n[1] = scaledY;
            first->n[2] = scaledZ;
            second->n[0] = scaledX;
            second->n[1] = scaledY;
            second->n[2] = scaledZ;
            third->n[0] = scaledX;
            third->n[1] = scaledY;
            third->n[2] = scaledZ;
            break;
    }
}
