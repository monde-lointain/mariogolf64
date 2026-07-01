#include "common.h"

extern s32 scenario_mode_id;
extern s8 g_terrain_vtx_xform_mode;
extern s32 D_801B608C;
extern s32 D_801B6098;
extern s8 D_800B67C0;
extern u8 D_800C1B29;
extern u8 D_800C143C;

s32 func_80052070(void) {
  if (D_801B608C == 9) {
    return (&D_800C1B29)[scenario_mode_id * 12 + D_801B6098 * 2];
  }
  if (scenario_mode_id >= 12) {
    return 0;
  }
  return D_801B6098;
}

s32 func_800520DC(void) {
  if (scenario_mode_id >= 12) {
    return 0;
  }
  return D_801B6098;
}

s32 func_80052100(s32 arg0, s32 arg1) {
  if (g_terrain_vtx_xform_mode != 0) {
    return 0;
  }
  if (D_800B67C0 != 0 && arg1 == 0) {
    return 1;
  }
  return (&D_800C143C)[arg0 * 200 + arg1 * 10] & 1;
}

s32 func_80052168(s32 arg0, s32 arg1) {
  if (g_terrain_vtx_xform_mode != 0) {
    return 0;
  }
  return (&D_800C143C)[arg0 * 200 + arg1 * 10] & 2;
}
