#ifndef MATH_UTIL_H
#define MATH_UTIL_H

#include <ultra64.h>

u32 IntLength3D(s32 x, s32 y, s32 z);
u32 IntLength2D(s32 x, s32 y);
void SetRandomSeed(s32 seed);
s32 NextRandom(void);
f32 Length2D(f32 x, f32 y);
f32 Length3D(f32 x, f32 y, f32 z);
u16 ComputeCrc16(u8 *data, u32 size);
s32 ReportDivisionError(void);

#endif
