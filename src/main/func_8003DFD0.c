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
