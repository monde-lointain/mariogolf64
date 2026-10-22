#ifndef COMMON_H
#define COMMON_H

#include <ultra64.h>
#include "include_asm.h"

/* A point or direction in 3D space */
typedef f32 Vec3[3];
/* A 4 by 4 matrix of floats, indexed by row and then column */
typedef f32 Mat4[4][4];

#define ARRAY_COUNT(array) (s32)(sizeof(array) / sizeof((array)[0]))

/* The magnitude of an integer. The game's code evaluates the value once,
   into a variable of its own, as a GNU statement expression does. */
#define ABS(value) ({ s32 absValue = (value); (absValue < 0) ? -absValue : absValue; })

#endif
