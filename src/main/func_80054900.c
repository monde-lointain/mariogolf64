#include "common.h"

extern s32 flag_is_set(s32 flag);
extern s32 find_keyframe_offset_by_tag(void *base, s32 tag, s32 arg2);
extern void func_80054BC0(void);
extern u8 *get_character_state(s32 id);
extern void *func_80056060(s32 id);
extern void animation_step(s32 index);
extern void func_800543A4(void);
extern void func_800577DC(s32 index, void *arg1);

typedef struct {
    s32 unk_00;
    u8 unk_04[0x188];
} AnimSlot;
extern AnimSlot D_801F4424[];
extern s16 D_800FBE48;
extern void func_800453E0(void *a, void *b, void *c);
extern s32 get_interpolated_terrain_height_wrapper(s32 x, s32 z);
extern s32 func_800544B4(s32 arg0, s32 arg1);
extern void func_800543DC(s32 arg0, s32 arg1);
extern s32 func_8005342C(s32 a, s32 b, s32 c, s32 d, u32 phys);
extern s32 D_800C1DEC;

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80054900);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80054938);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80054B7C);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80054BC0);

void func_80054E20(void) {
    if (flag_is_set(0x36)) {
        func_80054BC0();
    }
}

void func_80054E4C(s32 arg0, s32 arg1) {
    u8 *cs = get_character_state(arg0);
    if (cs != NULL) {
        s32 mode;
        D_800C1DEC = *(s32 *)(cs + 8);
        switch (func_800544B4(arg0, arg1)) {
            case 0:
                mode = 0;
                break;
            case 1:
                mode = 1;
                break;
            case 2:
                mode = 2;
                break;
            case 3:
                mode = 3;
                break;
            default:
                mode = -1;
                break;
        }
        func_8005342C(0, 0, 0x20, 0x2000000, osVirtualToPhysical(*(void **)(cs + 4)));
        func_800543DC(arg0, mode);
    }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", collect_keyframe_events_at);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", find_keyframe_offset_by_tag);

s32 func_800550B8(void *arg0) {
    return find_keyframe_offset_by_tag(arg0, 2, 6);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", update_vertex_texture_coords);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", update_vertex_texture_coords_per_frame);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80055738);

void func_80055788(s32 id) {
    u8 *base = get_character_state(id);
    s32 i = 0x1F;
    u8 *p = base + 0xF8;
    for (; i >= 0; i--) {
        p[0x8C] = 0;
        p -= 8;
    }
}

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

void func_80056264(s32 id) {
    u8 *cs = get_character_state(id);
    if (cs != NULL) {
        *(f32 *)(cs + 0x10) = (f32)*(s32 *)(cs + 0x18);
    }
}

void seek_current_frame_by(s32 id, s32 delta) {
    u8 *cs = get_character_state(id);
    if (cs != NULL) {
        f32 newval = *(f32 *)(cs + 0x10) + (f32)delta;
        f32 limit = (f32)*(s32 *)(cs + 0x18);
        *(f32 *)(cs + 0x10) = newval;
        if (limit <= newval) {
            *(f32 *)(cs + 0x10) = limit;
        }
    }
}

void func_800562F4(s32 id, f32 value) {
    u8 *cs = get_character_state(id);
    if (cs != NULL) {
        *(f32 *)(cs + 0x5C) = value;
    }
}

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

s32 func_8005642C(s32 id) {
    void *p = func_80056060(id);
    s32 result;
    if (p == NULL) {
        result = -1;
    } else {
        result = *(s16 *)((u8 *)p + 4) + *(s16 *)((u8 *)p + 6);
    }
    return result;
}

s32 func_80056464(s32 id) {
    void *p = func_80056060(id);
    s32 result;
    if (p == NULL) {
        result = -1;
    } else {
        result = *(s16 *)((u8 *)p + 4);
    }
    return result;
}

s32 func_80056494(s32 id) {
    void *p = func_80056060(id);
    s32 result;
    if (p == NULL) {
        result = -1;
    } else {
        result = *(s16 *)((u8 *)p + 6);
    }
    return result;
}

void set_extra_flag_0(s32 id, u8 value) {
    u8 *cs = get_character_state(id);
    if (cs != NULL) {
        cs[0x188] = value;
    }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_800564F0);

s32 func_80056520(s32 id) {
    u8 *cs = get_character_state(id);
    s32 result = -1;
    if (cs != NULL) {
        result = (s32)*(f32 *)(cs + 0x10);
    }
    return result;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", animation_step);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_800568CC);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", animation_step_no_rotate);

void animation_tick_all_players(void) {
    s32 i;
    for (i = 0; i < 4; i++) {
        animation_step(i);
    }
}

s32 func_800577D0(void) {
    return 0x23680;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_800577DC);

void func_800578AC(void) {
    s32 i;
    func_800543A4();
    for (i = 0; i < 4; i++) {
        D_801F4424[i].unk_00 = -4;
        func_800577DC(i, (void *)0x8030F800);
    }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80057914);

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80057FFC);

void func_80058ACC(s32 id) {
    u8 *cs = get_character_state(id);
    if (cs != NULL) {
        s32 angle;
        func_800453E0(cs + 0x38, cs + 0x44, cs + 0x50);
        angle = -D_800FBE48;
        *(f32 *)(cs + 0x48) = (f32)(u16)angle * 9.5873802e-05f;
    }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80058B34);

void func_80058C58(s32 id, s32 *arg1, f32 arg2) {
    u8 *cs = get_character_state(id);
    if (cs != NULL) {
        s32 h;
        f32 t38;
        f32 t2;
        *(f32 *)(cs + 0x38) = (f32)arg1[0];
        t38 = *(volatile f32 *)(cs + 0x38);
        t2 = (f32)arg1[2];
        *(f32 *)(cs + 0x40) = t2;
        h = get_interpolated_terrain_height_wrapper((s32)t38, (s32)t2);
        *(f32 *)(cs + 0x3C) = (f32)h;
        *(f32 *)(cs + 0x44) = 0.0f;
        *(f32 *)(cs + 0x4C) = 0.0f;
        *(f32 *)(cs + 0x48) = -arg2;
        *(f32 *)(cs + 0x50) = 1.0f;
        *(f32 *)(cs + 0x54) = 1.0f;
        *(f32 *)(cs + 0x58) = 1.0f;
    }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80054900", func_80058D04);
