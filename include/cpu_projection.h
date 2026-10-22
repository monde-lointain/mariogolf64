#ifndef CPU_PROJECTION_H
#define CPU_PROJECTION_H

#include "common.h"

/* SetTriangleNormal's targets */
#define NORMAL_ON_FIRST 0
#define NORMAL_ON_SECOND 1
#define NORMAL_ON_THIRD 2
#define NORMAL_ON_ALL 3
/* A vertex normal's largest component */
#define NORMAL_LENGTH 127.0f

typedef struct ScreenPoint {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 y;
    /* 0x08 */ s32 depth;
    /* 0x0C */ f32 scaleX;
    /* 0x10 */ f32 scaleY;
    /* 0x14 */ f32 depthStep;
} ScreenPoint; /* size = 0x18 */

void SetCpuProjection(Mtx *projection, Mtx *viewing, Vp *viewport);
void SetCpuProjectionF(Mat4 projection, Mat4 viewing, Vp *viewport);
s32 GetSphereVisibility(f32 x, f32 y, f32 z, f32 radius);
s32 GetViewDepth(f32 x, f32 y, f32 z);
s32 ProjectPoint(f32 x, f32 y, f32 z, ScreenPoint *point);
s32 ProjectPointToScreen(f32 x, f32 y, f32 z, ScreenPoint *point);
void BuildRotationMatrix(Mat4 matrix, Vec3 rotation, Vec3 translation);
void BuildTransformMatrix(Mat4 matrix, Vec3 rotation, Vec3 translation, Vec3 scale);
void MultiplyMatrices(Mat4 a, Mat4 b, Mat4 product);
void ConvertMatrixToFixed(Mat4 matrix, Mtx *fixedMatrix);
void SetTriangleNormal(Vtx *vertices, s32 firstIndex, s32 secondIndex, s32 thirdIndex, s32 target);

#endif
