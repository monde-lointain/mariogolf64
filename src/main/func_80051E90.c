#include "common.h"

extern s32 D_801B608C;
extern s32 D_801B6098;
extern s32 scenario_mode_id;
extern u8 D_800C1B28[];
extern s8 g_terrain_vtx_xform_mode;

s32 func_80051E90(s32 course, s32 hole) {
  switch (course) {
    case 0:
      if (hole == 10) {
        return 400;
      }
      if (hole == 16) {
        return 400;
      }
      break;
    case 1:
      if (hole == 1) {
        return 400;
      }
      if (hole == 5) {
        return 600;
      }
      if (hole == 7) {
        return 150;
      }
      if (hole == 16) {
        return 300;
      }
      break;
    case 2:
      if (hole == 2) {
        return 300;
      }
      if (hole == 12) {
        return 600;
      }
      if (hole == 16) {
        return 550;
      }
      break;
    case 3:
      if (hole == 2) {
        return 300;
      }
      if (hole == 4) {
        return 1000;
      }
      if (hole == 9) {
        return 700;
      }
      if (hole == 10) {
        return 400;
      }
      if (hole == 11) {
        return 450;
      }
      if (hole == 12) {
        return 400;
      }
      if (hole == 13) {
        return 600;
      }
      if (hole == 16) {
        return 900;
      }
      break;
    case 4:
      if (hole == 10) {
        return 400;
      }
      if (hole == 15) {
        return 300;
      }
      break;
    case 5:
      if (hole == 15) {
        return 400;
      }
      if (hole == 16) {
        return 700;
      }
      if (hole == 17) {
        return 400;
      }
      break;
    case 6:
    case 7:
      return 30;
  }
  return 200;
}

s32 func_80051FCC(void) {
  if (D_801B608C == 9) {
    return D_800C1B28[scenario_mode_id * 12 + D_801B6098 * 2];
  }
  if (g_terrain_vtx_xform_mode != 0) {
    return ((scenario_mode_id - 6) / 3 & 1) | 6;
  }
  if (scenario_mode_id >= 12) {
    return 8;
  }
  return scenario_mode_id;
}
