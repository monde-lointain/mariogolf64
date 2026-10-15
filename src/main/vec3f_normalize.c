#include "common.h"
#include <nusys.h>

extern u8 D_801052F8[];
extern u16* D_800B67D8[];
extern void initialize_color_framebuffers(u16** framebuffers, u32 buffer_count);
extern void func_8006F4F0(void);
extern u8 g_mus_audio_paused;

INCLUDE_ASM("asm/nonmatchings/main/vec3f_normalize", vec3f_normalize);

void func_800299D0(void) { initialize_color_framebuffers(D_800B67D8, 3); }

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

extern char D_800CA1B8[];

void func_80029B08(void) {
  s32 count;
  for (;;) {
    for (count = 0; count != 100000000; count++) {
    }
    osSyncPrintf(D_800CA1B8, 100000000);
  }
}

extern s32 func_800577D0(void);
extern void func_800577DC(s32 index, void* arg1);
extern u8 g_terrain_tile_cache[];
extern char D_800CA1C4[];

void func_80029B58(void) {
  s32 size = func_800577D0();
  func_800577DC(1, &g_terrain_tile_cache[0]);
  func_800577DC(3, &g_terrain_tile_cache[size]);
  func_800577DC(2, &g_terrain_tile_cache[size * 2]);
  osSyncPrintf(D_800CA1C4, 0x827E0 - size * 3);
}

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

extern NUContData D_800FED18[];
extern s16 D_800FF518[];
extern s8 D_800FF51A[];
extern s8 D_800FF51B[];
extern u16 D_800FF51C[];

void func_80029F6C(NUContData* input, s32 unused, s32 index) {
  /* The unused static chain and its spill belong to the nested helper at
   * 80029F28. */
  s32 clamp_stick(s32 value) {
    s32 result;
    s32 absolute = value;
    s32 magnitude;
    if (value < 0) {
      absolute = -value;
    }
    magnitude = absolute;
    if (magnitude < 16) {
      return 0;
    }
    if (magnitude > 64) {
      magnitude = 64;
    }
    result = magnitude;
    if (value < 0) {
      result = -result;
    }
    return result;
  }

  s32 offset = index * 6;
  s32 repeat = *(s16*)((u8*)D_800FF518 + offset);
  NUContData* output = &D_800FED18[index];
  s32 repeat_offset;
  s32 buttons;

  output->stick_x = clamp_stick(input->stick_x);
  output->stick_y = clamp_stick(input->stick_y);
  *(s8*)((u8*)D_800FF51A + offset) =
      func_80029EEC(input->stick_x, *(s8*)((u8*)D_800FF51A + offset), 56, 20);
  *(s8*)((u8*)D_800FF51B + offset) =
      func_80029EEC(input->stick_y, *(s8*)((u8*)D_800FF51B + offset), 56, 20);
  output->button = input->button;
  if (*(s8*)((u8*)D_800FF51A + offset) == 1) {
    output->button |= 0x100;
  } else if (*(s8*)((u8*)D_800FF51A + offset) == -1) {
    output->button |= 0x200;
  }
  if (*(s8*)((u8*)D_800FF51B + index * 6) == 1) {
    output->button |= 0x800;
  } else if (*(s8*)((u8*)D_800FF51B + index * 6) == -1) {
    output->button |= 0x400;
  }
  /* Keep this offset's lifetime ahead of the trigger copy for register
   * allocation. */
  repeat_offset = index * 6;
  output->trigger = input->trigger;
  buttons = output->button;
  if (buttons == *(u16*)((u8*)D_800FF51C + repeat_offset)) {
    *(u16*)((u8*)D_800FF51C + repeat_offset) = buttons;
    if (repeat >= 16) {
      repeat = 15;
    }
    if (repeat == 0) {
      repeat = 3;
    } else {
      repeat--;
      output->button = 0;
    }
  } else {
    repeat = 15;
    *(u16*)((u8*)D_800FF51C + index * 6) = buttons;
  }
  *(s16*)((u8*)D_800FF518 + index * 6) = repeat;
}

INCLUDE_ASM("asm/nonmatchings/main/vec3f_normalize", func_8002A144);

INCLUDE_ASM("asm/nonmatchings/main/vec3f_normalize", func_8002A310);

INCLUDE_ASM("asm/nonmatchings/main/vec3f_normalize", game_draw_callback);
