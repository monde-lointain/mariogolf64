#ifndef CAMERA_FRAMING_H
#define CAMERA_FRAMING_H

#include <ultra64.h>

#include "math/geometry.h"
#include "math/math_util.h"

void GetFramedView(Vec3 eye, Vec3 lookAt);
void ClearFramedSpheres(void);
void AddFramedSphere(Vec3 center, f32 radius);
void SetFramingAngles(Radians pitch, Radians yaw);
void FrameSpheres(f32 depthOffset);

#endif
