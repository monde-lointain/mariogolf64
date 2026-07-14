#include "common.h"

INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain", raycast_terrain);

INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain", raycast_terrain_floor);

/* get_surface_type: near-match carry (score 450, #local-alloc-qty-permutation).
   See docs/wip/get_surface_type.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain", get_surface_type);

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

/* func_8003DE80: near-match carry (score 350, #local-alloc-qty-permutation,
   constant-15 reg). Sibling of get_surface_type; S233 compiler-source dive
   confirmed no-lever (mips.md:153-155 R4000 load-latency scheduler coin).
   See docs/wip/func_8003DE80.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain", func_8003DE80);
