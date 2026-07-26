/* Scores how flat the terrain is around a candidate position, and keeps the
 * position when it beats the best seen so far. The score sums the absolute
 * height difference from the centre sample over four rings of sixteen samples,
 * weighting an outer ring less by dividing by the ring index. A position sitting
 * inside any scenery collision cylinder is forced to a large score so it loses. */
void func_80098758(s32* pos) {
  CollisionCyl* cyl;
  s32 score;
  s32 centre_height;
  s32 ring;
  s32 step;
  s32 divisor;
  s32 i;
  s32 height;
  s32 distance;
  f32 angle;
  f32 offset_x;
  f32 offset_z;

  score = 0;
  centre_height = get_interpolated_terrain_height_wrapper(pos[0], pos[2]);

  for (ring = 1; ring < 5; ring++) {
    divisor = ring + 1;
    for (step = 0; step < 16; step++) {
      angle = (f32)step * 0.39269909f;
      offset_x = cosf(angle) * (f32)ring * 15360.0f;
      offset_z = sinf(angle) * (f32)ring * 15360.0f;
      height = get_interpolated_terrain_height_wrapper(
          (s32)((f32)pos[0] + offset_x), (s32)((f32)pos[2] + offset_z));
      height -= centre_height;
      if (height < 0) {
        height = -height;
      }
      score += height / divisor;
    }
  }

  score <<= 1;

  if (D_800E4CC8 == 0) {
    /* Both the centre sample and the ring divisor are dead from here, and the
     * ROM reuses their two registers for the threshold and the blocked flag
     * rather than reserving two more. */
    divisor = 0;
    cyl = collision_cylinders;
    centre_height = 0x12BFF;
    for (i = 0; i < 0x80; i++) {
      distance = calculate_hypotenuse_safe((cyl->x << 10) - pos[0],
                                           (cyl->z << 10) - pos[2]);
      distance -= cyl->radius << 10;
      if (distance <= centre_height) {
        divisor = 1;
      }
      cyl++;
    }
    if (divisor != 0) {
      score = 0x465000;
    }
  }

  if (D_800E4CC8 != 0) {
    if (!(score * 2 < D_800E4CA4)) {
      return;
    }
    if (!(0xB400 < D_800E4CA4)) {
      return;
    }
  }

  D_800E4CA4 = score;
  D_800E4C98 = pos[0];
  D_800E4C9C = pos[1];
  D_800E4CC8 = 1;
  D_800E4CA0 = pos[2];
}
