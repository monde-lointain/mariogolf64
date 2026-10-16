#ifndef VECTOR_H
#define VECTOR_H

#include "common.h"

typedef struct {
  /* 0x00 */ f32 x;
  /* 0x04 */ f32 y;
  /* 0x08 */ f32 z;
} Vec3f; /* 0x0C */

typedef struct {
  /* 0x00 */ s32 x;
  /* 0x04 */ s32 y;
  /* 0x08 */ s32 z;
} Vec3i; /* 0x0C */

/* project_point_to_screen writes this record only for points in front of it. */
typedef struct {
  /* 0x00 */ s32 x;
  /* 0x04 */ s32 y;
  /* 0x08 */ s32 depth;
  /* 0x0C */ f32 scale_x;
  /* 0x10 */ f32 scale_y;
  /* 0x14 */ f32 depth_step; /* absolute depth change for one camera-space unit */
} ScreenProjection; /* 0x18 */

s32 project_point_to_screen(f32 x, f32 y, f32 z, ScreenProjection* out);

#endif /* VECTOR_H */
