#include "common.h"

extern u32 g_bgm_current_id;
extern u32 g_bgm_pending_action;
extern u32 g_bgm_action_delay;
extern u32 g_bgm_active_handle;
extern u32 object_id_table[];
extern s32 MusHandleStop(u32 handle, s32 speed);
extern s32 MusHandleSetVolume(u32 handle, s32 volume);
extern s32 MusHandleSetPan(u32 handle, s32 pan);
extern s32 MusHandleSetReverb(u32 handle, s32 reverb);
extern s32 MusHandleSetTempo(u32 handle, s32 tempo);
extern s32 MusHandleSetFreqOffset(u32 handle, f32 offset);
extern const char D_800CCC14[];
extern const char D_800CCC20[];
extern s32 D_800C0EAC[];
extern u16 D_800C0FF8[];
extern s32 D_800C10F4[];
extern s32 D_800C113C[];
extern s32 D_800C1140[];
extern s32 D_800C1144[];
extern s32 D_800C1148[];
extern s32 D_800C114C[];

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", play_sound_effect);

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", func_80050DA0);

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

void func_800510EC(s32 idx, s32 val) { D_800C0EAC[idx] = val; }

void func_80051100(s32 index, s32 tempo) {
  OSIntMask mask = osSetIntMask(OS_IM_NONE);
  if (object_id_table[index] != -1) {
    MusHandleSetTempo(object_id_table[index], tempo);
  }
  osSetIntMask(mask);
}

void func_80051164(s32 a0, s32 a1, s32 a2, s32 a3, s32 arg5) {
  s32 idx = a1 * 5;
  u16* arr = D_800C0FF8;
  u16* r = arr + a2 * 18;
  s32 val = r[a0];
  D_800C113C[idx] = a0;
  D_800C1144[idx] = a2;
  D_800C1148[idx] = a3;
  D_800C1140[idx] = val;
  D_800C114C[idx] = arg5;
}

void func_800511D8(s32 col, s32 row) {
  u16* arr = D_800C0FF8;
  u16* row_p = arr + row * 18;
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

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", play_bgm_by_id);

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", func_80051D8C);

void alSynNew(ALSynth* s, ALSynConfig* config) { osSyncPrintf(D_800CCC14); }

void alSynDelete(ALSynth* s) { osSyncPrintf(D_800CCC20); }

void __freeParam(void) {}

void _freePVoice(void) {}
