#ifndef FRAMING_H
#define FRAMING_H

#include "common.h"

void GetFramedView(Vec3 eye, Vec3 lookAt);
void ClearFramedSpheres(void);
void AddFramedSphere(Vec3 center, f32 radius);
void SetFramingAngles(f32 pitch, f32 yaw);
void FrameSpheres(f32 depthOffset);

#endif
