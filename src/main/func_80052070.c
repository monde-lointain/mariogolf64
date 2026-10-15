#include "common.h"

extern s32 scenario_mode_id;
extern s8 g_terrain_vtx_xform_mode;
extern s32 game_mode;
extern s32 round_hole_index;
extern s8 g_debug_mode_enabled;
extern u8 scenario_hole_indices[];
extern u8 course_hole_flags[];

s32 get_current_hole_index(void) {
  if (game_mode == 9) {
    return scenario_hole_indices[scenario_mode_id * 12 + round_hole_index * 2];
  }
  if (scenario_mode_id >= 12) {
    return 0;
  }
  return round_hole_index;
}

s32 get_standard_course_hole_index(void) {
  if (scenario_mode_id >= 12) {
    return 0;
  }
  return round_hole_index;
}

s32 get_course_hole_primary_flag(s32 course_id, s32 hole_index) {
  if (g_terrain_vtx_xform_mode != 0) {
    return 0;
  }
  if (g_debug_mode_enabled != 0 && hole_index == 0) {
    return 1;
  }
  return course_hole_flags[course_id * 200 + hole_index * 10] & 1;
}

s32 get_course_hole_secondary_flag(s32 course_id, s32 hole_index) {
  if (g_terrain_vtx_xform_mode != 0) {
    return 0;
  }
  return course_hole_flags[course_id * 200 + hole_index * 10] & 2;
}
