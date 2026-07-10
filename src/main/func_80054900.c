#include "common.h"

extern s32 flag_is_set(s32 flag);
extern s32 find_keyframe_offset_by_tag(void *base, s32 tag, s32 arg2);
extern void func_80054BC0(void);
extern u8 *get_character_state(s32 id);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80054900);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80054938);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80054B7C);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80054BC0);

void func_80054E20(void) {
    if (flag_is_set(0x36)) {
        func_80054BC0();
    }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80054E4C);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", collect_keyframe_events_at);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", find_keyframe_offset_by_tag);

s32 func_800550B8(void *arg0) {
    return find_keyframe_offset_by_tag(arg0, 2, 6);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", update_vertex_texture_coords);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", update_vertex_texture_coords_per_frame);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80055738);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80055788);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", activate_texture_anim_slot);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80055828);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", load_character_model);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", lookup_animation_by_id);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80056060);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", animation_play);

void func_80056238(s32 id, u8 value) {
    u8 *cs = get_character_state(id);
    if (cs != NULL) {
        cs[0x85] = value;
    }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80056264);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", seek_current_frame_by);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_800562F4);

void func_80056324(s32 id, u8 value) {
    u8 *cs = get_character_state(id);
    if (cs != NULL) {
        cs[0x83] = value;
    }
}

void func_80056350(s32 id, u8 value) {
    u8 *cs = get_character_state(id);
    if (cs != NULL) {
        cs[0x84] = value;
    }
}

void func_8005637C(s32 id, u8 value) {
    u8 *cs = get_character_state(id);
    if (cs != NULL) {
        cs[0x60] = value;
    }
}

void set_anim_flag_82(s32 id, u8 value) {
    u8 *cs = get_character_state(id);
    if (cs != NULL) {
        cs[0x82] = value;
    }
}

void set_anim_speed_byte(s32 id, u8 value) {
    u8 *cs = get_character_state(id);
    if (cs != NULL) {
        cs[0x61] = value;
    }
}

s32 func_80056400(s32 id) {
    u8 *cs = get_character_state(id);
    s32 result = -1;
    if (cs != NULL) {
        result = *(s32 *)(cs + 0x18);
    }
    return result;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_8005642C);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80056464);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80056494);

void set_extra_flag_0(s32 id, u8 value) {
    u8 *cs = get_character_state(id);
    if (cs != NULL) {
        cs[0x188] = value;
    }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_800564F0);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80056520);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", animation_step);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_800568CC);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", animation_step_no_rotate);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", animation_tick_all_players);

s32 func_800577D0(void) {
    return 0x23680;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_800577DC);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_800578AC);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80057914);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80057FFC);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80058ACC);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80058B34);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80058C58);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80058D04);
