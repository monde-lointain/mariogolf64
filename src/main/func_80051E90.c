#include "common.h"

extern s32 game_mode;
extern s32 round_hole_index;
extern s32 scenario_mode_id;
extern u8 scenario_course_ids[];
extern s8 g_terrain_vtx_xform_mode;

s32 get_hole_camera_distance(s32 course_id, s32 hole_index) {
  switch (course_id) {
    case 0:
      if (hole_index == 10) {
        return 400;
      }
      if (hole_index == 16) {
        return 400;
      }
      break;
    case 1:
      if (hole_index == 1) {
        return 400;
      }
      if (hole_index == 5) {
        return 600;
      }
      if (hole_index == 7) {
        return 150;
      }
      if (hole_index == 16) {
        return 300;
      }
      break;
    case 2:
      if (hole_index == 2) {
        return 300;
      }
      if (hole_index == 12) {
        return 600;
      }
      if (hole_index == 16) {
        return 550;
      }
      break;
    case 3:
      if (hole_index == 2) {
        return 300;
      }
      if (hole_index == 4) {
        return 1000;
      }
      if (hole_index == 9) {
        return 700;
      }
      if (hole_index == 10) {
        return 400;
      }
      if (hole_index == 11) {
        return 450;
      }
      if (hole_index == 12) {
        return 400;
      }
      if (hole_index == 13) {
        return 600;
      }
      if (hole_index == 16) {
        return 900;
      }
      break;
    case 4:
      if (hole_index == 10) {
        return 400;
      }
      if (hole_index == 15) {
        return 300;
      }
      break;
    case 5:
      if (hole_index == 15) {
        return 400;
      }
      if (hole_index == 16) {
        return 700;
      }
      if (hole_index == 17) {
        return 400;
      }
      break;
    case 6:
    case 7:
      return 30;
  }
  return 200;
}

s32 get_current_course_id(void) {
  if (game_mode == 9) {
    return scenario_course_ids[scenario_mode_id * 12 + round_hole_index * 2];
  }
  if (g_terrain_vtx_xform_mode != 0) {
    return ((scenario_mode_id - 6) / 3 & 1) | 6;
  }
  if (scenario_mode_id >= 12) {
    return 8;
  }
  return scenario_mode_id;
}
