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

INCLUDE_ASM("asm/nonmatchings/main/func_8003DFD0", func_8003E314);
