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
 * scenery wind angle via sinf/cosf, projected onto terrain height.
 *
 * S318 carry at 196/196 instructions and the ROM's exact -0x140 frame, 88 cmpfn
 * rows. Body: docs/wip/func_8003E004.base.c; measured state, the two angle
 * hoists that closed the 2-instruction deficit, and the residual's three
 * clusters: docs/wip/func_8003E004.near-match.md.
 *
 * S204's move_movables DFmode-hoist wall stays REFUTED (S235). Two inherited
 * claims do not: the body S235 called "structurally 100%" is 194 of 196 (two
 * load-use nops after mtc1, gone once ang_lo/ang_hi are hoisted out of the
 * sinf() arguments), and the 158k-iteration permuter floor was measured on that
 * short body, so it belongs to it and not to the function (the S259 rule). The
 * global.c:587 allocno tie is real and now has numbers: a is 15 refs at live
 * length 80 (5625) against i's 11 at 88 (3750), so i needs 16 refs or a needs
 * 9. All rodata/data refs (D_800CA8E0/E8/F0 pi/2 pool, D_800BA888,
 * g_scenery_wind_angle_a) stay extern in the shared blob. */
INCLUDE_ASM("asm/nonmatchings/main/func_8003DFD0", func_8003E004);

extern Gfx* glistp;
extern Vtx D_800BA888[];

void func_8003E314(void) {
  s32 i;

  gDPSetCombineLERP(glistp++, 0, 0, 0, SHADE, SHADE, 0, TEXEL0, 0, 0, 0, 0,
                    SHADE, SHADE, 0, TEXEL0, 0);
  gDPPipeSync(glistp++);
  gSPVertex(glistp++, D_800BA888, 18, 0);
  for (i = 0; i != 8; i++) {
    gSP2Triangles(glistp++, i * 2, i * 2 + 2, i * 2 + 1, 0, i * 2 + 1,
                  i * 2 + 2, i * 2 + 3, 0);
  }
}
