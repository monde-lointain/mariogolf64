#include "common.h"

extern s8 flag;
extern u8 D_800DAF60[];

INCLUDE_ASM("asm/nonmatchings/main/func_8004D190", func_8004D190);

INCLUDE_ASM("asm/nonmatchings/main/func_8004D190", func_8004D4B8);

INCLUDE_ASM("asm/nonmatchings/main/func_8004D190", func_8004D580);

INCLUDE_ASM("asm/nonmatchings/main/func_8004D190", func_8004D5F0);

void clear_text_grid(void) {
  s32 i = 0x4AF;
  u8* p = &D_800DAF60[0x4AF];

  for (; i >= 0; i--) {
    *p-- = 0;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_8004D190", func_8004D7B8);

void set_flag_based_on_param(s32 enable) {
  if (enable) {
    flag = -0x80;
  } else {
    flag = 0;
  }
}
