#include "common.h"

INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain", raycast_terrain);

INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain", raycast_terrain_floor);

extern u8 D_800BA6B0[];
extern s8 D_800B7DB0[];
extern s32 flag_is_set(s32);

s32 get_surface_type(s32 tile, f32 x, f32 z) {
  s32 cx;
  s32 cz;
  s32 type;
  s8* col;
  s32 edge;
  cx = (s32)(x * 16.0f);
  cz = (s32)(z * 16.0f);
  if (cx < 0) {
    cx = 0;
  } else if (cx >= 16) {
    cx = 15;
  }
  if (cz < 0) {
    cz = 0;
  } else if (cz >= 16) {
    cz = 15;
  }
  /* Materialize the edge before the selector load; its live range preserves
   * the incoming tile copy and the target's load-delay scheduling. */
  do {
    edge = 15;
  } while (0);
  col = D_800B7DB0 + (D_800BA6B0[tile & 0x3F] << 8);
  cz = edge - cz;
  type = col[(cz << 4) | cx];
  type = type - 0x30;
  if (type < 0) {
    type = 0;
  }
  if (type >= 16) {
    type = 15;
  }
  if (type == 3) {
    type = 0;
  }
  if (flag_is_set(10)) {
    type = 1;
  }
  return type;
}

INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain",
            check_line_cylinder_collision);

typedef struct {
  u8 pad0[0x18];
  s32 x;     /* 0x18 */
  s32 unk1C; /* 0x1C */
  s32 z;     /* 0x20 */
} RaycastTarget;

extern s32 camera_position_x;
extern s32 camera_position_z;
extern s32 calculate_hypotenuse_safe(s32 dx, s32 dz);
extern void vec3f_normalize(f32* x, f32* y, f32* z);

void clamp_min_distance_from_target(RaycastTarget* target) {
  f32 dir_x;
  f32 dir_y;
  f32 dir_z;
  s32 dist;
  s32* cam = &camera_position_x;

  dist = calculate_hypotenuse_safe(cam[0] - target->x,
                                   camera_position_z - target->z);
  if ((f32)dist < 614.4f) {
    if (dist != 0) {
      dir_y = 0.0f;
      dir_x = (f32)(target->x - cam[0]);
      dir_z = (f32)(target->z - camera_position_z);
      vec3f_normalize(&dir_x, &dir_y, &dir_z);
      target->x = (s32)(dir_x * 614.4f + (f32)cam[0]);
      target->z = (s32)(dir_z * 614.4f + (f32)cam[2]);
    }
  }
}

INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain",
            calculate_camera_attraction_vector);

s32 get_triangle_normal_dominant_axis(s32 x0, s32 y0, s32 z0, s32 x1, s32 y1,
                                      s32 z1, s32 x2, s32 y2, s32 z2) {
  s32 nx;
  s32 ny;
  s32 nz;
  s32 max;
  s32 axis;

  nx = (x1 - x0) * (z2 - z0) - (z1 - z0) * (x2 - x0);
  max = 0;
  axis = -1;
  if (nx < 0) {
    nx = -nx;
  }
  if (max < nx) {
    axis = 0;
    max = nx;
  }

  ny = (y1 - y0) * (z2 - z0) - (z1 - z0) * (y2 - y0);
  if (ny < 0) {
    ny = -ny;
  }
  if (max < ny) {
    axis = 1;
    max = ny;
  }

  nz = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
  if (nz < 0) {
    nz = -nz;
  }
  if (max < nz) {
    axis = 2;
    max = nz;
  }

  return axis;
}

INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain", func_8003AC80);

INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain", find_collision_triangle);

INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain",
            check_ray_triangle_collision);

INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain", resolve_wall_collision);

INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain",
            raycast_find_closest_triangle);

s32 func_8003DE80(s32 tile, f32 x, f32 z) {
  s32 cx;
  s32 cz;
  s32 count;
  s8* col;
  s32 edge;
  cx = (s32)(x * 16.0f);
  cz = (s32)(z * 16.0f);
  if (cx < 0) {
    cx = 0;
  } else if (cx >= 16) {
    cx = 15;
  }
  if (cz < 0) {
    cz = 0;
  } else if (cz >= 16) {
    cz = 15;
  }
  /* As above, keep the edge load before the column selector and row flip. */
  do {
    edge = 15;
  } while (0);
  col = D_800B7DB0 + (D_800BA6B0[tile & 0x3F] << 8);
  cz = edge - cz;
  count = (col[(cz << 4) | cx] != 0x30) << 2;
  if (cz > 0) {
    count += col[((cz - 1) << 4) | cx] != 0x30;
  }
  if (cz < 15) {
    count += col[((cz + 1) << 4) | cx] != 0x30;
  }
  if (cx > 0) {
    count += col[(cz << 4) | (cx - 1)] != 0x30;
  }
  if (cx < 15) {
    count += col[(cz << 4) | (cx + 1)] != 0x30;
  }
  return count;
}
