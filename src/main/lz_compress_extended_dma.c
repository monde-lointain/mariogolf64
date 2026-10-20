#include "common.h"
#include "session.h"

/* Terrain/hole-loader module state. */
extern volatile s32 g_terrain_enable_gate;
extern s8 g_terrain_vtx_xform_mode;
extern s32 scenario_mode_id;
/*
 * Read as an array (offset 0) so its load carries MEM_IN_STRUCT_P. In
 * func_80069BCC the nonscalar store `D_80105B20[D_801B6098[0]] = 0` must
 * invalidate the cached value so the guard re-loads it (matches the ROM);
 * a bare scalar survives CSE and forwards.
 */
extern s32 round_hole_index[];
extern const char D_800D1380[]; /* "putter%d:course%d:hole%d\n" */
extern u32 D_800C3FA0;          /* LCG scratch-RNG state */

/* Hole-terrain loader thread + its stack (overlay 4 data). */
extern OSThread D_800E1A50;
extern u8 D_8025D350;

/* Overlay swap + game callees referenced by this TU. */
extern s32 overlay_id_from_segment(s32);
extern void unload_overlay(s32);
extern s32 load_overlay(s32);
extern void load_hole_terrain_assets(s32, s32);
extern s32 get_current_course_id(void);
extern s32 get_current_hole_index(void);
extern void func_8004D148(void);

/* In-TU siblings still carried as INCLUDE_ASM (forward decls for the call
 * sites). */
extern void func_80068F98(void);
extern void func_800695F8(void);
extern void func_80069124(void);
extern void func_80069BCC(void);
extern void func_80069D08(void*);

/* func_8006955C deps (mode-select dispatch). */
extern s32 D_800BB0E8;
extern void func_80253234(s32);
extern void func_80253270(void);
extern void build_scenery_light_sets(void);

/* func_80069BCC deps (camera + terrain-array reset). */
extern s32 get_interpolated_terrain_height_wrapper(s32, s32);
extern s32 flag_is_set(s32);
extern void func_8005F964(void);
extern void init_sky_panels(void);
extern void func_8005F1C8(void);
extern void func_80032E88(void);
extern const char D_800D139C[];
extern const char D_800D13A8[];
extern const char D_800D13B4[];
extern s32 active_terrain_index;
extern s32 D_801B60B0;
extern s32 D_80105B20[];
extern s32 D_80104E70[];
extern s32 D_8010CF60[];
extern s32 D_80106148[];

/* func_80069D08 deps (hole-terrain loader thread). */
extern volatile s32 g_frame_counter;
extern s32 D_80105318;
extern const char D_800D13C0[];
extern const char D_800D13CC[];
extern const char D_800D13E4[];
extern const char D_800D13EC[];
extern const char D_800D13F4[];
extern const char D_800D13FC[];
extern const char D_800D1404[];
extern void flag_set(s32 flag);
extern void flag_clear(s32 flag);
extern s32 func_80054310(void);
extern void func_802541E8(void);
extern void init_sky_pool_and_world_state(void);

typedef struct {
  u8* source_cursor;
  u8* output_start;
  u8* carry_buffer_start;
  u8* output_cursor;
  u8* source_end;
  u16 window_flags;
  u16 unknown_16;
  u16* control_ring;
  u16 ring_index;
  u16 run_remaining;
  u16 history_index;
  u16* history_buffer;
} LzDecompressState;

s32 lz_decompress_extended(LzDecompressState* state);
extern OSPiHandle* nuPiCartHandle;

s32 lz_compress_extended_dma(u32 rom_address, void* destination, u32 remaining_bytes) {
  OSIoMesg dma_messages[2];
  OSMesgQueue dma_queue;
  OSMesg message_buffer[2];
  LzDecompressState state;
  u8 first_buffer_storage[0x840];
  u8 second_buffer_storage[0x839];
  OSMesg* message_array;
  void* buffers[2];
  u16 control_ring[0x800];
  u16 history_buffer[0x100];

  s32 received_count = 0;
  u32 next_dma_buffer = 0;
  s32 in_flight_count = 0;
  u32 receive_index = 0;
  u8* first_buffer = (u8*)((((u32)first_buffer_storage + 0xF) & ~0xF) + 0x20);
  u8* second_buffer = (u8*)((((u32)second_buffer_storage + 0xF) & ~0xF) + 0x20);

  buffers[0] = first_buffer;
  buffers[1] = second_buffer;
  state.source_cursor = first_buffer;
  state.carry_buffer_start = second_buffer;
  state.window_flags = 1;
  state.control_ring = control_ring;
  state.history_buffer = history_buffer;
  state.output_start = (u8*)destination;
  osCreateMesgQueue(&dma_queue, message_buffer, 2);
  message_array = message_buffer;

  for (;;) {
    u32 other_buffer_index;

    if (remaining_bytes != 0) {
      u32 dma_size = remaining_bytes;
      OSIoMesg* dma_message;
      void* dram_address;

      if (dma_size > 0x800) {
        dma_size = 0x800;
      }
      dma_message = &dma_messages[next_dma_buffer];
      dma_message->hdr.pri = 0;
      dma_message->hdr.retQueue = &dma_queue;
      dram_address = buffers[next_dma_buffer];
      remaining_bytes -= dma_size;
      dma_message->devAddr = rom_address;
      rom_address += dma_size;
      dma_message->size = dma_size;
      dma_message->dramAddr = dram_address;
      in_flight_count++;
      osInvalDCache(buffers[next_dma_buffer], dma_size);
      next_dma_buffer ^= 1;
      osEPiStartDma(nuPiCartHandle, dma_message, OS_READ);
    }

    if (osRecvMesg(&dma_queue, message_array + receive_index,
                   OS_MESG_NOBLOCK) != -1) {
      receive_index ^= 1;
      received_count++;
      in_flight_count--;
    }

    if (received_count == 0) {
      if (in_flight_count == 2 || remaining_bytes == 0) {
        osRecvMesg(&dma_queue, message_array + receive_index, OS_MESG_BLOCK);
        receive_index ^= 1;
        received_count = 1;
        in_flight_count--;
      }
      if (received_count == 0) {
        continue;
      }
    }

    other_buffer_index = receive_index ^ 1;
    osInvalDCache(buffers[other_buffer_index],
                  dma_messages[other_buffer_index].size);
    state.carry_buffer_start = buffers[receive_index];
    state.source_end = (u8*)buffers[other_buffer_index] + 0x800;
    if (in_flight_count == 0 && remaining_bytes == 0) {
      state.window_flags |= 2;
    }
    {
      s32 result = lz_decompress_extended(&state);

      received_count--;
      if (result >= 0) {
        osWritebackDCache(destination, result);
        return result;
      }
      state.window_flags &= 0xFFFE;
    }
  }
}

/*
 * A small LCG scratch RNG (state D_800C3FA0). The two helpers are GCC nested
 * functions of func_80068F4C: only this parent calls them, and each call site
 * materializes the static chain ($v0 = &frame) before the jal, so they emit
 * with the chain prologue (dead `sw v0,0(sp)`) at 0x80068F00 / 0x80068F18 ahead
 * of the parent (0x80068F4C). The parent is called
 * externally by func_800695F8, so it stays top-level.
 */
void func_80068F4C(s32 arg0, u32 seed) {
  void rng_seed(u32 v) { D_800C3FA0 = v; } /* 0x80068F00 */
  u32 rng_next(void) {
    return D_800C3FA0 = D_800C3FA0 * 0x5D588B65 + 1;
  } /* 0x80068F18 */

  rng_seed(seed);
  arg0 += rng_next();
  rng_seed(arg0);
  rng_next();
}

extern s32 D_80105E10[][3];
extern f32 D_80254C88[];
extern f32 D_80254C8C;
extern f32 D_80254C90;

void func_80068F98(void) {
  s32 component;
  s32 i;
  if (g_terrain_vtx_xform_mode == 0) {
    return;
  }
  D_80254C88[0] = 0.25f;
  D_80254C8C = 0.25f;
  D_80254C90 = 0.125f;
  for (component = 0; component < 3; component++) {
    for (i = 0; i < 4; i++) {
      D_80105E10[i][component] =
          (f32)D_80105E10[i][component] * D_80254C88[component];
      D_80105E10[i + 4][component] =
          (f32)D_80105E10[i + 4][component] * D_80254C88[component];
    }
    for (i = 0; i < 8; i++) {
      D_80105E10[i + 8][component] =
          (f32)D_80105E10[i + 8][component] * D_80254C88[component];
      D_80105E10[i + 16][component] =
          (f32)D_80105E10[i + 16][component] * D_80254C88[component];
    }
  }
}

void func_800690C0(void) {
  s32 course_id;

  osSyncPrintf(D_800D1380, g_terrain_vtx_xform_mode, scenario_mode_id,
               round_hole_index[0]);
  course_id = get_current_course_id();
  load_hole_terrain_assets(course_id, get_current_hole_index());
  func_80068F98();
}

INCLUDE_ASM("asm/nonmatchings/main/lz_compress_extended_dma", func_80069124);

void func_8006955C(void) {
  s32 mode;

  D_800BB0E8 = 1;
  switch (hole_environment.unknown_24) {
    case 0:
      mode = 2;
      break;
    case 6:
      mode = 1;
      break;
    case 10:
      mode = 0;
      break;
  }
  if (!flag_is_set(0x11)) {
    func_80253234(mode);
  }
  D_800BB0E8 = 0;
  func_80253270();
  build_scenery_light_sets();
  func_80069124();
}

INCLUDE_ASM("asm/nonmatchings/main/lz_compress_extended_dma", func_800695F8);

void func_80069BCC(void) {
  s32 i;

  hole_environment.pin_position.y = get_interpolated_terrain_height_wrapper(
      hole_environment.pin_position.x, hole_environment.pin_position.z);
  if (flag_is_set(0x43)) {
    for (;;) {
    }
  }
  func_8005F964();
  active_terrain_index = 0;
  osSyncPrintf(D_800D139C);
  init_sky_panels();
  osSyncPrintf(D_800D13A8);
  func_8005F1C8();
  osSyncPrintf(D_800D13B4);
  func_80032E88();
  D_80105B20[round_hole_index[0]] = 0;
  if (round_hole_index[0] == 0) {
    D_801B60B0 = 0;
    for (i = 0; i != 0x12; i++) {
      D_80105B20[i] = 0;
      D_80104E70[i] = 0;
      D_8010CF60[i] = 0;
      D_80106148[i] = 0;
    }
  }
}

void func_80069CE4(void) {
  func_800695F8();
  func_80069BCC();
}

void func_80069D08(void *arg0) {
  s32 course_id;
  const char *msg;

  for (;;) {
    do {
    } while (g_terrain_enable_gate != 0);
    course_id = get_current_course_id();
    if ((u32)(g_frame_counter - D_80105318) < 3) {
      do {
      } while ((u32)(g_frame_counter - D_80105318) < 3);
    }
    msg = D_800D13C0;
    for (;;) {
      osSyncPrintf(msg);
      if (func_80054310() == 0) {
        break;
      }
      msg = D_800D13CC;
    }
    if (flag_is_set(0x51) != 0 && round_hole_index[0] == 0) {
      flag_set(0x12);
    } else {
      flag_clear(0x12);
    }
    flag_clear(7);
    if (course_id == 4) {
      flag_set(0x12);
      flag_set(0x60);
    } else {
      if ((u32)course_id < 2 || course_id == 3 || course_id == 5) {
        flag_set(0x12);
      } else {
        flag_clear(0x12);
      }
      flag_clear(0x60);
    }
    if (course_id == 1 && round_hole_index[0] == 7) {
      flag_set(0x69);
    } else {
      flag_clear(0x69);
    }
    osSyncPrintf(D_800D13E4);
    func_800690C0();
    osSyncPrintf(D_800D13EC);
    func_800695F8();
    osSyncPrintf(D_800D13F4);
    func_8006955C();
    func_80069BCC();
    osSyncPrintf(D_800D13FC);
    func_802541E8();
    osSyncPrintf(D_800D1404);
    init_sky_pool_and_world_state();
    flag_set(7);
    if (flag_is_set(0x43) != 0) {
      for (;;) {
      }
    }
    g_terrain_enable_gate = 1;
    osDestroyThread(&D_800E1A50);
  }
}

void func_80069F28(void) { g_terrain_enable_gate = 1; }

void func_80069F38(void) {
  unload_overlay(overlay_id_from_segment(8));
  load_overlay(overlay_id_from_segment(6));
  osCreateThread(&D_800E1A50, 0xA, func_80069D08, NULL, &D_8025D350, 0x14);
  osStartThread(&D_800E1A50);
  g_terrain_enable_gate = 0;
  func_8004D148();
}

void func_80069FBC(void) {
  if (g_terrain_enable_gate == 0) {
    osDestroyThread(&D_800E1A50);
    g_terrain_enable_gate = 1;
  }
}
