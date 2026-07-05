#include "common.h"

extern s8 flag;
extern u8 D_800DAF60[];
extern u16 D_800BE6E0[];
extern u16* D_800BFEE0;

void print_string_at_grid(char* str, s32 col, s32 row);

INCLUDE_ASM("asm/nonmatchings/main/func_8004D190", func_8004D190);

/* func_8004D4B8: string -> glyph-tile blit. CARRIED (S182) as a
 * #dead-frame-reload-artifact-regalloc-wall: structurally byte-exact except the
 * target reserves a dead 8-byte frame + the reg permutation it drives.
 * Near-match + root cause in docs/wip/func_8004D4B8.near-match.c.txt. */
INCLUDE_ASM("asm/nonmatchings/main/func_8004D190", func_8004D4B8);

void func_8004D5F0(s32 c, s32 x, s32 y);

void func_8004D580(char* str, s32 x, s32 y) {
  print_string_at_grid(str, x, y);
  while (*str != 0) {
    func_8004D5F0(*str, x, y);
    str++;
    x++;
  }
}

/* func_8004D5F0: framebuffer glyph renderer (double-buffered). CARRIED (S182)
 * as a register-COALESCING regalloc-artifact wall (permuter-confirmed, base
 * 2055 -> plateau 1190): 99/105, target keeps 2 non-coalesced copies of c
 * (frame 0x30/s0-s5) that my more-optimal -O2 build coalesces (frame 0x28).
 * Near-match + root cause in docs/wip/func_8004D5F0.near-match.c.txt. */
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
