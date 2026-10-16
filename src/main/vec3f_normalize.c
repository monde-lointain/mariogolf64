#include "common.h"
#include "controller.h"
#include <nusys.h>

extern u8 D_801052F8[];
extern u16* D_800B67D8[];
extern void initialize_color_framebuffers(u16** framebuffers, u32 buffer_count);
extern void func_8006F4F0(void);
extern u8 g_mus_audio_paused;

extern char D_800CA1A8[];
extern f32 sqrtf(f32);

void vec3f_normalize(f32* x, f32* y, f32* z) {
  f32 vx = *x;
  f32 vy = *y;
  f32 vz = *z;
  f32 length = vx * vx + vy * vy + vz * vz;
  if (length < 0.0f) {
    osSyncPrintf(D_800CA1A8);
    *x = 0.0f;
    *y = 1.0f;
    *z = 0.0f;
  } else {
    length = 1.0f / sqrtf(length);
    *x = vx * length;
    *y = vy * length;
    *z = vz * length;
  }
}

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


__asm__(
    ".section .text\n"
    ".set noat\n"
    ".set noreorder\n"
    ".globl clear_fpc_csr\n"
    "clear_fpc_csr:\n"
    "cfc1 $4, $31\n"
    "nop\n"
    "lui $5, 0x0FFF\n"
    "ori $5, $5, 0xE0FF\n"
    "and $4, $4, $5\n"
    "addiu $5, $0, -0xF81\n"
    "and $4, $4, $5\n"
    "addiu $5, $0, 0\n"
    "or $4, $4, $5\n"
    "ctc1 $4, $31\n"
    "nop\n"
    "jr $31\n"
    "nop\n"
    ".set reorder\n"
    ".set at\n"
);

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
/* The direction and button symbols alias fields in each six-byte slot. */

extern ControllerRepeatState D_800FF518[];

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

  /* Field-relative walks retain the retail scalar aliasing and address roots. */
  s32 offset = index * sizeof(ControllerRepeatState);
  s32 repeat = *(s16*)((u8*)&D_800FF518[0].repeat_count + offset);
  NUContData* output = &D_800FED18[index];
  s32 repeat_offset;
  s32 buttons;

  output->stick_x = clamp_stick(input->stick_x);
  output->stick_y = clamp_stick(input->stick_y);
  *((s8*)&D_800FF518[0].horizontal_direction + offset) =
      func_80029EEC(input->stick_x, *((s8*)&D_800FF518[0].horizontal_direction + offset), 56, 20);
  *((s8*)&D_800FF518[0].vertical_direction + offset) =
      func_80029EEC(input->stick_y, *((s8*)&D_800FF518[0].vertical_direction + offset), 56, 20);
  output->button = input->button;
  if (*((s8*)&D_800FF518[0].horizontal_direction + offset) == 1) {
    output->button |= 0x100;
  } else if (*((s8*)&D_800FF518[0].horizontal_direction + offset) == -1) {
    output->button |= 0x200;
  }
  if (*((s8*)&D_800FF518[0].vertical_direction + index * sizeof(ControllerRepeatState)) == 1) {
    output->button |= 0x800;
  } else if (*((s8*)&D_800FF518[0].vertical_direction + index * sizeof(ControllerRepeatState)) == -1) {
    output->button |= 0x400;
  }
  /* Keep this offset's lifetime ahead of the trigger copy for register
   * allocation. */
  repeat_offset = index * sizeof(ControllerRepeatState);
  output->trigger = input->trigger;
  buttons = output->button;
  if (buttons == *(u16*)((u8*)&D_800FF518[0].previous_buttons + repeat_offset)) {
    *(u16*)((u8*)&D_800FF518[0].previous_buttons + repeat_offset) = buttons;
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
    *(u16*)((u8*)&D_800FF518[0].previous_buttons + index * sizeof(ControllerRepeatState)) = buttons;
  }
  *(s16*)((u8*)&D_800FF518[0].repeat_count + index * sizeof(ControllerRepeatState)) = repeat;
}


extern u8 D_801052B8;
extern NUContData debug_menu_pad_buttons_a[];
extern NUContData D_800FEDA8[];
extern NUContData D_801060E0[];
extern ControllerState D_800FBDC0[];
extern void func_8002A144(NUContData* input, ControllerState* state);

void func_8002A144(NUContData* input, ControllerState* state) {
  s32 repeat = state->repeat_count;
  state->buttons = input->button;
  state->stick_x = input->stick_x;
  state->stick_y = input->stick_y;
  if (state->stick_x < 0) {
    state->stick_x += 16;
    if (state->stick_x > 0) {
      state->stick_x = 0;
    } else {
      if (state->stick_x < -44) {
        state->stick_x = -44;
      }
      if (state->stick_x < -30) {
        state->buttons |= 0x200;
      }
    }
  } else {
    state->stick_x -= 16;
    if (state->stick_x < 0) {
      state->stick_x = 0;
    } else {
      if (state->stick_x > 44) {
        state->stick_x = 44;
      }
      if (state->stick_x > 30) {
        state->buttons |= 0x100;
      }
    }
  }
  if (state->stick_y < 0) {
    state->stick_y += 16;
    if (state->stick_y > 0) {
      state->stick_y = 0;
    } else {
      if (state->stick_y < -44) {
        state->stick_y = -44;
      }
      if (state->stick_y < -30) {
        state->buttons |= 0x400;
      }
    }
  } else {
    state->stick_y -= 16;
    if (state->stick_y < 0) {
      state->stick_y = 0;
    } else {
      if (state->stick_y > 44) {
        state->stick_y = 44;
      }
      if (state->stick_y > 30) {
        state->buttons |= 0x800;
      }
    }
  }
  state->pressed = (state->previous ^ input->button) & input->button;
  state->repeated = state->buttons;
  if (state->buttons == state->previous) {
    if (repeat >= 16) {
      repeat = 15;
    }
    if (repeat == 0) {
      repeat = 3;
    } else {
      repeat--;
      state->repeated = 0;
    }
  } else {
    repeat = 15;
  }
  state->repeat_count = repeat;
  state->previous = state->buttons;
  state->held = state->buttons;
  state->buttons = input->button;
}

void func_8002A310(void) {
  s32 count = 0;
  s32 port = 0;
  ControllerState* state = D_800FBDC0;
  NUContData* previous = D_801060E0;
  NUContData* processed = D_800FED18;
  NUContData* raw_previous = D_800FEDA8;
  NUContData* raw = debug_menu_pad_buttons_a;
  for (; port < 4; port++) {
    if ((D_801052B8 >> port) & 1) {
      *raw_previous = *raw;
      *previous = *processed;
      nuContDataGetEx(raw, port);
      func_80029F6C(raw, (s32)previous, count);
      func_8002A144(raw, state);
      state++;
      previous++;
      processed++;
      raw_previous++;
      raw++;
      count++;
    }
  }
}

extern s32 D_800B67F4;
extern s8 D_800B6800;
extern s8 D_800B67EC;
extern volatile u8 D_800B67C8;
extern volatile s32 g_frame_counter;
extern s32 D_800B67E8;
extern s32 D_800B67E4;
extern s32 D_800B67D0;
extern void func_800318A8(void);
extern void bgm_tick(void);
extern void func_80031AF4(void);
extern void render_frame(void);
extern void func_80032520(void);

void game_draw_callback(u32 task_count) {
  s32 frames;
  if (nuScPreNMIFlag & 1) {
    D_800B67F4 ^= 1;
    if (D_800B67F4 == 0) {
      func_800318A8();
    }
  } else {
    D_800B67F4 ^= 1;
    if (D_800B67F4 == 0) {
      /* Keep the two admission paths distinct before their shared work. */
      if (D_800B6800) {
        if ((s32)task_count < 4) {
          goto process_frame;
        }
        goto defer_frame;
      }
      if ((s32)task_count >= 2) {
        goto defer_frame;
      }
    process_frame:
      bgm_tick();
      if (D_800B67EC < 2) {
        func_8002A310();
        if (D_800B67C8 != 2) {
          func_80031AF4();
          render_frame();
          func_80032520();
          if (D_800B67C8 == 1 && (D_800FED18[0].button & 0xC030) == 0xC030) {
            D_800B67C8 = 2;
          }
        }
        g_frame_counter++;
        D_800B67E8++;
        if (D_800B67EC == 1) {
          D_800B67EC = 2;
        }
      } else {
        func_8002A310();
        if (D_800FED18[1].trigger & 0x200) {
          D_800B67EC = 1;
        }
        if (D_800B67EC != 1 && (D_800FED18[1].trigger & 0x100)) {
          D_800B67EC = 0;
        }
      }
      goto count_frame;
    defer_frame:
      D_800B67F4 ^= 1;
    }
  count_frame:
    D_800B67E4++;
    if (D_800B67E4 >= 60) {
      frames = D_800B67E8;
      D_800B67E4 = 0;
      D_800B67E8 = 0;
      D_800B67D0 = frames;
    }
  }
}
