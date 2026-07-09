#include "common.h"

extern s32 calculate_hypotenuse_safe(s32, s32);

s32 func_8003DFD0(s32 x, s32 z) {
  s32 distance;

  distance = calculate_hypotenuse_safe(x, z);
  if ((u32)distance < 0x699U) {
    distance = 0;
  } else {
    distance -= 0x699;
  }
  return distance;
}

/* func_8003E004: wind-indicator vertex generator. Fills the 18-entry Vtx array
 * D_800BA888 (drawn by func_8003E314) with a fan of positions computed from the
 * scenery wind angle via sinf/cosf, projected onto terrain height. Structure and
 * math are fully solved (see nonmatchings/func_8003E004/base.c) but the body is
 * carried as INCLUDE_ASM pending a permuter run: a pervasive-regalloc divergence
 * (the (f64)base double and the scratch-buffer base pointer spill where the ROM
 * keeps them in callee-saved regs). All rodata/data refs (D_800CA8E0/E8/F0 pi/2
 * pool, D_800BA888, g_scenery_wind_angle_a) stay extern in the shared blob. */
INCLUDE_ASM("asm/nonmatchings/main/func_8003DFD0", func_8003E004);

extern Gfx* glistp;
extern Vtx D_800BA888[];

void func_8003E314(void) {
  s32 i;

  gDPSetCombineLERP(glistp++, 0, 0, 0, SHADE, SHADE, 0, TEXEL0, 0, 0, 0, 0, SHADE,
                    SHADE, 0, TEXEL0, 0);
  gDPPipeSync(glistp++);
  gSPVertex(glistp++, D_800BA888, 18, 0);
  for (i = 0; i != 8; i++) {
    gSP2Triangles(glistp++, i * 2, i * 2 + 2, i * 2 + 1, 0, i * 2 + 1, i * 2 + 2,
                  i * 2 + 3, 0);
  }
}
