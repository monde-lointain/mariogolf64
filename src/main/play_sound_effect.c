#include "common.h"
#include <nusys.h>
#include <nualstl.h>

typedef struct {
  u16 delay_frames[18];
} SoundDelayRow; /* 0x24 */

/* One deferred request per player; -1 frames_until_playback means no request. */
typedef struct {
  /* 0x00 */ s32 sound_bank_index;
  /* 0x04 */ s32 frames_until_playback;
  /* 0x08 */ s32 parameter_index;
  /* 0x0C */ s32 volume;
  /* 0x10 */ s32 pan;
} PendingPlayerSound; /* 0x14 */

extern u32 g_bgm_current_id;
extern u32 g_bgm_pending_action;
extern u32 g_bgm_action_delay;
extern u32 g_bgm_active_handle;
extern u32 object_id_table[];
extern const char D_800CCC14[];
extern const char D_800CCC20[];
extern s32 D_800C0EAC[];
extern SoundDelayRow D_800C0FF8[7];
extern s32 D_800C10F4[];
extern PendingPlayerSound D_800C113C[4];
extern u32 D_800C11B0[];
extern s32 g_bgm_pending_id;
extern s32 g_bgm_pending_flags;
extern const char D_800CCC08[];
extern s32 func_8005DE00(void);
extern void bgm_load_song_from_rom(u32 rom_addr, s32 size);
extern musHandle bgm_start_current_song(void);
extern u8 D_800C1FFC;
extern u32 D_800C0F2C;
extern void func_8009B0DC(int mode);
extern s32 flag_is_set(s32 flag);
extern u32 spawn_object_simple(int number);
extern s32 D_800C0E60;
extern f32 D_800C0EEC[];
extern u32 D_800C0F30[];
extern s32 D_800C0F70[];
void update_indexed_slot_at_0xCB_atomic(s32 index, s32 reverb);
void update_indexed_slot_at_0x60_atomic(s32 index, f32 offset);

void play_sound_effect(s32 sfx, s32 channel, s32 volume) {
  OSIntMask mask;
  s32 i;
  if (flag_is_set(0x9C)) {
    return;
  }
  mask = osSetIntMask(OS_IM_NONE);
  D_800C0EEC[channel] = (f32)volume;
  if (sfx < 0x215) {
    if (object_id_table[channel] != -1) {
      MusHandleStop(object_id_table[channel], 0);
      object_id_table[channel] = spawn_object_simple(sfx);
      D_800C0F30[channel] = object_id_table[channel];
      MusHandleSetVolume(object_id_table[channel], volume);
      MusHandleSetFreqOffset(object_id_table[channel], 0.0f);
      MusHandleSetPan(object_id_table[channel], 0x7F);
    }
    if (D_800C0E60 == channel) {
      D_800C0E60 = channel ^ 1;
    }
  }
  osSetIntMask(mask);
  if (sfx < 0x215) {
    for (i = 0; i != 0x22; i++) {
      if (D_800C0F70[i] == sfx) {
        break;
      }
    }
    if (i < 0x22) {
      if (D_800C0F70[i] == 0x9A) {
        update_indexed_slot_at_0xCB_atomic(channel, 0);
      } else {
        update_indexed_slot_at_0xCB_atomic(channel, 6);
      }
      update_indexed_slot_at_0x60_atomic(channel, 0.0f);
    } else {
      if (sfx == 0x5A) {
        update_indexed_slot_at_0xCB_atomic(channel, 6);
      } else {
        update_indexed_slot_at_0xCB_atomic(channel, 2);
      }
    }
  }
}

void func_80050DA0(s32 sfx, s32 channel, s32 volume, s32 reverb, s32 pan) {
  OSIntMask mask;
  s32 i;
  if (flag_is_set(0x9C)) {
    return;
  }
  mask = osSetIntMask(OS_IM_NONE);
  D_800C0EEC[channel] = (f32)volume;
  if (sfx < 0x215) {
    if (object_id_table[channel] != -1) {
      MusHandleStop(object_id_table[channel], 0);
      MusHandleSetReverb(object_id_table[channel], reverb);
    }
    object_id_table[channel] = spawn_object_simple(sfx);
    D_800C0F30[channel] = object_id_table[channel];
    MusHandleSetFreqOffset(object_id_table[channel], 0.0f);
    MusHandleSetVolume(object_id_table[channel], volume);
    MusHandleSetPan(object_id_table[channel], pan);
    if (D_800C0E60 == channel) {
      D_800C0E60 = channel ^ 1;
    }
  }
  osSetIntMask(mask);
  if (sfx < 0x215) {
    for (i = 0; i != 0x22; i++) {
      if (D_800C0F70[i] == sfx) {
        break;
      }
    }
    if (i < 0x22) {
      if (D_800C0F70[i] == 0x9A) {
        update_indexed_slot_at_0xCB_atomic(channel, 0);
      } else {
        update_indexed_slot_at_0xCB_atomic(channel, 6);
      }
      update_indexed_slot_at_0x60_atomic(channel, 0.0f);
    } else {
      if (sfx == 0x5A) {
        update_indexed_slot_at_0xCB_atomic(channel, 6);
      } else {
        update_indexed_slot_at_0xCB_atomic(channel, 2);
      }
    }
  }
}

void update_object_group_by_id(s32 index) {
  OSIntMask mask = osSetIntMask(OS_IM_NONE);
  if (object_id_table[index] != -1) {
    MusHandleStop(object_id_table[index], 0);
  }
  osSetIntMask(mask);
}

void func_80050FC0(s32 index, s32 pan) {
  OSIntMask mask = osSetIntMask(OS_IM_NONE);
  if (object_id_table[index] != -1) {
    MusHandleSetPan(object_id_table[index], pan);
  }
  osSetIntMask(mask);
}

void set_field_0x9E_for_object_group_if_type_matches(s32 index, s32 volume) {
  OSIntMask mask = osSetIntMask(OS_IM_NONE);
  if (object_id_table[index] != -1) {
    MusHandleSetVolume(object_id_table[index], volume);
  }
  osSetIntMask(mask);
}

void update_indexed_slot_at_0xCB_atomic(s32 index, s32 reverb) {
  OSIntMask mask = osSetIntMask(OS_IM_NONE);
  if (object_id_table[index] != -1) {
    MusHandleSetReverb(object_id_table[index], reverb);
  }
  osSetIntMask(mask);
}

void func_800510EC(s32 index, s32 value) { D_800C0EAC[index] = value; }

void func_80051100(s32 index, s32 tempo) {
  OSIntMask mask = osSetIntMask(OS_IM_NONE);
  if (object_id_table[index] != -1) {
    MusHandleSetTempo(object_id_table[index], tempo);
  }
  osSetIntMask(mask);
}

void func_80051164(s32 sound_bank_index, s32 player_index, s32 parameter_index,
                  s32 volume, s32 pan) {
  /* Separate member roots preserve retail scalar aliasing and address lowering. */
  s32 byte_offset = player_index * sizeof(PendingPlayerSound);
  u16* delays = D_800C0FF8[0].delay_frames;
  u16* delay_row = delays + parameter_index * (sizeof(SoundDelayRow) / sizeof(u16));
  s32 delay_frames = delay_row[sound_bank_index];
  *(s32*)(byte_offset + (u32)&D_800C113C[0].sound_bank_index) = sound_bank_index;
  *(s32*)(byte_offset + (u32)&D_800C113C[0].parameter_index) = parameter_index;
  *(s32*)(byte_offset + (u32)&D_800C113C[0].volume) = volume;
  *(s32*)(byte_offset + (u32)&D_800C113C[0].frames_until_playback) = delay_frames;
  *(s32*)(byte_offset + (u32)&D_800C113C[0].pan) = pan;
}

void func_800511D8(s32 col, s32 row) {
  u16* delays = D_800C0FF8[0].delay_frames;
  u16* row_p = delays + row * (sizeof(SoundDelayRow) / sizeof(u16));
  D_800C10F4[col] = row_p[col];
}

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", func_80051210);

void update_indexed_slot_at_0x60_atomic(s32 index, f32 offset) {
  OSIntMask mask = osSetIntMask(OS_IM_NONE);
  if (object_id_table[index] != -1) {
    MusHandleSetFreqOffset(object_id_table[index], offset);
  }
  osSetIntMask(mask);
}

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", bgm_tick);

void func_80051A08(s32 speed) {
  OSIntMask mask = osSetIntMask(OS_IM_NONE);
  if (g_bgm_active_handle != -1) {
    MusHandleStop(g_bgm_active_handle, speed);
  }
  osSetIntMask(mask);
}

void bgm_request_stop(void) {
  g_bgm_pending_action = 2;
  g_bgm_action_delay = 1;
  g_bgm_current_id = -1;
}

void func_80051A88(s32 volume) {
  if (g_bgm_active_handle != -1) {
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    MusHandleSetVolume(g_bgm_active_handle, volume);
    osSetIntMask(mask);
  }
}

void func_80051AE4(s32 reverb) {
  if (g_bgm_active_handle != -1) {
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    MusHandleSetReverb(g_bgm_active_handle, reverb);
    osSetIntMask(mask);
  }
}

void func_80051B40(s32 tempo) {
  if (g_bgm_active_handle != -1) {
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    MusHandleSetTempo(g_bgm_active_handle, tempo);
    osSetIntMask(mask);
  }
}

void func_80051B9C(f32 offset) {
  if (g_bgm_active_handle != -1) {
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    MusHandleSetFreqOffset(g_bgm_active_handle, offset);
    osSetIntMask(mask);
  }
}

s32 func_80051BFC(void) { return g_bgm_current_id; }

void play_bgm_by_id(s32 id, s32 flags) {
  if (flag_is_set(0x9D)) {
    return;
  }
  if (func_8005DE00() && (flags & 0x10)) {
    bgm_request_stop();
    return;
  }
  if (g_bgm_active_handle != -1) {
    if (!(flags & 0x80) && g_bgm_current_id == id) {
      return;
    }
    g_bgm_pending_action = 1;
    g_bgm_pending_id = id;
    g_bgm_pending_flags = flags;
    g_bgm_action_delay = 2;
    return;
  }
  if (g_bgm_pending_action == 1) {
    g_bgm_pending_id = id;
    g_bgm_pending_flags = flags;
    return;
  }
  osSyncPrintf(D_800CCC08, id);
  if (func_8005DE00() && (flags & 0x10)) {
    return;
  }
  g_bgm_current_id = id;
  if (id >= 51) {
    id = 50;
  }
  bgm_load_song_from_rom(D_800C11B0[id * 2],
                         D_800C11B0[id * 2 + 1] - D_800C11B0[id * 2]);
  MusSetMasterVolume(2, 12000);
  g_bgm_active_handle = bgm_start_current_song();
  D_800C0F2C = g_bgm_active_handle;
  if (D_800C1FFC & 2) {
    func_8009B0DC(0);
  } else {
    func_8009B0DC(1);
  }
}

extern u8 D_171790[];
extern u8 audio_wave_table_rom_start[];
extern u8 audio_wave_bank_rom_end[];
extern u8 D_D9B9C0[];
extern u8 D_D9F450[];
extern void audio_system_boot(musConfig* config);
extern void load_audio_wave_bank(u32 rom_address, s32 size, void* wave_bank);
extern void load_sound_effect_bank_from_rom(u32 rom_address, s32 size);
extern s32 ChangeCustomEffect(s32 index);

void func_80051D8C(void) {
  s32 i;
  u32* object;
  s32* effect;

  audio_system_boot(NULL);
  g_bgm_pending_action = 0;
  g_bgm_active_handle = -1;
  g_bgm_current_id = -1;
  /* The pointer-bank end and wave-table start are distinct linker roots,
   * despite sharing the same ROM address. */
  load_audio_wave_bank((u32)D_171790,
                      (u32)audio_wave_bank_rom_end - (u32)D_171790,
                      audio_wave_table_rom_start);
  load_sound_effect_bank_from_rom((u32)D_D9B9C0, (u32)D_D9F450 - (u32)D_D9B9C0);
  i = 0;
  effect = D_800C0EAC;
  object = object_id_table;
  do {
    *object = 0;
    *effect = 0;
    effect++;
    i++;
    object++;
  } while (i < 16);
  ChangeCustomEffect(5);
}

void alSynNew(ALSynth* s, ALSynConfig* config) { osSyncPrintf(D_800CCC14); }

void alSynDelete(ALSynth* s) { osSyncPrintf(D_800CCC20); }

void __freeParam(void) {}

void _freePVoice(void) {}
