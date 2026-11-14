#ifndef CPU_PROJECTION_H
#define CPU_PROJECTION_H

#include <ultra64.h>

#include "geometry.h"

enum NormalPlacement {
    NORMAL_ON_FIRST,
    NORMAL_ON_SECOND,
    NORMAL_ON_THIRD,
    NORMAL_ON_ALL
};

#define NORMAL_LENGTH 127.0f

// A screen position with its depth; ScreenPoint adds the projection's
// scaling.
typedef struct ScreenPosition {
    s32 x;
    s32 y;
    s32 depth;
} ScreenPosition;

typedef struct ScreenPoint {
    s32 x;
    s32 y;
    s32 depth;
    f32 scaleX;
    f32 scaleY;
    f32 depthStep;
} ScreenPoint;

void SetCpuProjection(Mtx *projection, Mtx *viewing, Vp *viewport);
void SetCpuProjectionF(Mat4 projection, Mat4 viewing, Vp *viewport);

enum SphereVisibility {
    SPHERE_BEHIND_CAMERA,
    SPHERE_PAST_SIDE,
    SPHERE_PAST_TOP_OR_BOTTOM
};

// Returns a SphereVisibility when the sphere is out of view, or else its
// negative view depth.
s32 GetSphereVisibility(f32 x, f32 y, f32 z, f32 radius);
s32 GetViewDepth(f32 x, f32 y, f32 z);
s32 ProjectPoint(f32 x, f32 y, f32 z, ScreenPoint *point);
s32 ProjectPointToScreen(f32 x, f32 y, f32 z, ScreenPoint *point);
void BuildRotationMatrix(Mat4 matrix, Vec3 rotation, Vec3 translation);
void BuildTransformMatrix(Mat4 matrix, Vec3 rotation, Vec3 translation, Vec3 scale);
void MultiplyMatrices(Mat4 a, Mat4 b, Mat4 product);
void ConvertMatrixToFixed(Mat4 matrix, Mtx *fixedMatrix);
void SetTriangleNormal(
    Vtx *vertices,
    s32 firstIndex,
    s32 secondIndex,
    s32 thirdIndex,
    s32 placement
);

#endif
