#include "common.h"

extern u32 g_bgm_current_id;
extern u32 g_bgm_pending_action;
extern u32 g_bgm_action_delay;
extern s32 D_800C0EAC[];
extern u16 D_800C0FF8[];
extern s32 D_800C10F4[];

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", play_sound_effect);

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", func_80050DA0);

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect",
            update_object_group_by_id);

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", func_80050FC0);

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect",
            set_field_0x9E_for_object_group_if_type_matches);

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect",
            update_indexed_slot_at_0xCB_atomic);

void func_800510EC(s32 idx, s32 val) { D_800C0EAC[idx] = val; }

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", func_80051100);

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", func_80051164);

void func_800511D8(s32 col, s32 row) {
  u16* arr = D_800C0FF8;
  u16* row_p = arr + row * 18;
  D_800C10F4[col] = row_p[col];
}

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", func_80051210);

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect",
            update_indexed_slot_at_0x60_atomic);

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", bgm_tick);

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", func_80051A08);

void bgm_request_stop(void) {
  g_bgm_pending_action = 2;
  g_bgm_action_delay = 1;
  g_bgm_current_id = -1;
}

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", func_80051A88);

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", func_80051AE4);

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", func_80051B40);

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", func_80051B9C);

s32 func_80051BFC(void) { return g_bgm_current_id; }

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", play_bgm_by_id);

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", func_80051D8C);

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", alSynNew);

INCLUDE_ASM("asm/nonmatchings/main/play_sound_effect", alSynDelete);

void __freeParam(void) {}

void _freePVoice(void) {}
