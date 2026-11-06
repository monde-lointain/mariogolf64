#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <ultra64.h>

/*
 * The axes, points, directions and matrices of 3D space, in floats
 */

/* Indices of a point's coordinates; y is vertical, and in the ball's
 * simulation it grows downward, the way GRAVITY pulls */
#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2
#define AXIS_COUNT 3
/* A point or direction in 3D space */
typedef f32 Vec3[AXIS_COUNT];
/* The axes of a point on a plane, x and y */
#define PLANE_AXIS_COUNT 2
/* A point or direction on a plane */
typedef f32 Vec2[PLANE_AXIS_COUNT];

/* A 4 by 4 matrix of floats, indexed by row and then column */
typedef f32 Mat4[4][4];

#endif
