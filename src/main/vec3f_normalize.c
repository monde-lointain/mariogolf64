#include "common.h"

extern u8 D_801052F8[];
extern u32 D_800B67D8;
extern void cfb_setup(void* framebuf, s32 num);
extern void func_8006F4F0(void);
extern u8 g_mus_audio_paused;

INCLUDE_ASM("asm/nonmatchings/main/vec3f_normalize", vec3f_normalize);

void func_800299D0(void) { cfb_setup(&D_800B67D8, 3); }

s32 flag_is_set(s32 flag) {
  if ((u32)flag >= 0x100) {
    return 0;
  }
  return (D_801052F8[(u32)flag >> 3] & (0x80 >> (flag & 7))) != 0;
}

void flag_set(s32 flag) {
  if ((u32)flag < 0x100) {
    D_801052F8[(u32)flag >> 3] |= 0x80 >> (flag & 7);
  }
}

void flag_clear(s32 flag) {
  if ((u32)flag < 0x100) {
    D_801052F8[(u32)flag >> 3] &= 0xFF7F >> (flag & 7);
  }
}

void flag_toggle(s32 flag) {
  if ((u32)flag < 0x100) {
    D_801052F8[(u32)flag >> 3] ^= 0x80 >> (flag & 7);
  }
}

void flag_clear_all(void) {
  s32 i;
  u8* p;
  for (i = 0x1F, p = &D_801052F8[0x1F]; i >= 0; i--) {
    *p-- = 0;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/vec3f_normalize", func_80029B08);

INCLUDE_ASM("asm/nonmatchings/main/vec3f_normalize", func_80029B58);

void pause_audio(void) {
  func_8006F4F0();
  g_mus_audio_paused = 1;
}

INCLUDE_ASM("asm/nonmatchings/main/vec3f_normalize", func_80029C00);

INCLUDE_ASM("asm/nonmatchings/main/vec3f_normalize", mainproc);

s32 func_80029EEC(s32 arg0, s32 arg1, u32 arg2, u32 arg3) {
  u32 mag = (arg0 <= -1) ? (-arg0) : (arg0);
  if (mag <= arg3) {
    return 0;
  }
  if (mag < arg2) {
    arg0 = arg1;
  }
  if (arg0 != 0) {
    return (arg0 >> 31) | 1;
  }
  return 0;
}

INCLUDE_ASM("asm/nonmatchings/main/vec3f_normalize", func_80029F28);

INCLUDE_ASM("asm/nonmatchings/main/vec3f_normalize", func_80029F6C);

INCLUDE_ASM("asm/nonmatchings/main/vec3f_normalize", func_8002A144);

INCLUDE_ASM("asm/nonmatchings/main/vec3f_normalize", func_8002A310);

INCLUDE_ASM("asm/nonmatchings/main/vec3f_normalize", game_draw_callback);
