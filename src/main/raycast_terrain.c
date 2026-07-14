#include "common.h"

INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain", raycast_terrain);

INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain", raycast_terrain_floor);

INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain", get_surface_type);

INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain",
            check_line_cylinder_collision);

INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain",
            clamp_min_distance_from_target);

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

INCLUDE_ASM("asm/nonmatchings/main/raycast_terrain", func_8003DE80);
