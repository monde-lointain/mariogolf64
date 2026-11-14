#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <ultra64.h>

#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2
#define AXIS_COUNT 3
#define PLANE_AXIS_COUNT 2
#define GROUND_AXIS_X 0
#define GROUND_AXIS_Z 1

#define TRIANGLE_VERTEX_COUNT 3
typedef f32 Vec3[AXIS_COUNT];
typedef f32 Vec2[PLANE_AXIS_COUNT];

typedef f32 Mat4[4][4];

#endif
