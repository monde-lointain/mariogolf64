#include "common.h"

extern s32 calculate_hypotenuse_safe(s32, s32);

s32 func_8003DFD0(s32 x_coordinate, s32 z_coordinate) {
  s32 distance;

  distance = calculate_hypotenuse_safe(x_coordinate, z_coordinate);
  if ((u32)distance < 0x699U) {
    distance = 0;
  } else {
    distance -= 0x699;
  }
  return distance;
}

/* func_8003E004: wind-indicator vertex generator. Fills the 18-entry Vtx array
 * D_800BA888 (drawn by func_8003E314) with a fan of positions computed from the
 * scenery wind angle via sinf/cosf, projected onto terrain height.
 *
 * The build matches the ROM's exact -0x140 frame and 196-instruction body once
 * ang_lo/ang_hi are hoisted out of the sinf() arguments; without that hoist the
 * body is 194 of 196, short by the two load-use nops after mtc1.
 *
 * The global.c:587 allocno tie is real: a is 15 refs at live length 80 (5625)
 * against i's 11 at 88 (3750), so i needs 16 refs or a needs 9. All rodata/data
 * refs (D_800CA8E0/E8/F0 pi/2 pool, D_800BA888, g_scenery_wind_angle_a) stay
 * extern in the shared blob. */
INCLUDE_ASM("asm/nonmatchings/main/func_8003DFD0", func_8003E004);

extern Gfx* glistp;
extern Vtx D_800BA888[];

void func_8003E314(void) {
  s32 triangle_index;

  gDPSetCombineLERP(glistp++, 0, 0, 0, SHADE, SHADE, 0, TEXEL0, 0, 0, 0, 0,
                    SHADE, SHADE, 0, TEXEL0, 0);
  gDPPipeSync(glistp++);
  gSPVertex(glistp++, D_800BA888, 18, 0);
  for (triangle_index = 0; triangle_index != 8; triangle_index++) {
    gSP2Triangles(glistp++, triangle_index * 2, triangle_index * 2 + 2, triangle_index * 2 + 1, 0,
                  triangle_index * 2 + 1, triangle_index * 2 + 2, triangle_index * 2 + 3, 0);
  }
}
