#include "common.h"

extern s32 D_80105148;
extern u8 D_80105165;
extern s32 D_800C4660;
extern s32 D_800C4664;
extern void* D_800FE338;
extern u8 D_80105140[];

extern void heap3_free(void** payload_ptr);

void func_80077BF0(void) {
  D_80105148 = 150;
  D_80105165 = 1;
}

void func_80077C0C(void) { D_80105165 = 0; }

INCLUDE_ASM("asm/nonmatchings/main/func_80077BF0", func_80077C18);

void func_80077DEC(void) {
  s32 i;

  if (D_800C4664 == 1) {
    i = 0;
    heap3_free((void**)&D_800FE338);
    if (D_800C4660 != 0) {
      do {
        heap3_free((void**)&D_80105140[i * 0x2C]);
        i++;
      } while (i != D_800C4660);
    }
  }
  D_800C4664 = 0;
}

s32 func_80077E6C(void) {
  s32 v = D_800C4664;

  if (v > 0) {
    return 0;
  }
  v -= 1;
  D_800C4664 = v;
  return v < -2;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80077BF0", func_80077E94);

INCLUDE_ASM("asm/nonmatchings/main/func_80077BF0", render_spark_effects);
