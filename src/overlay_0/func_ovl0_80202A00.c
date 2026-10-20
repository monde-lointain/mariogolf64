#include "common.h"
#include "../main/rom_asset.h"
#include "../main/session.h"
#include "../main/club.h"
#include "../main/ball.h"
#include "../main/terrain.h"
#include "../main/controller.h"

typedef struct {
    u16 unk0;
    s32 unk4;
    s32 unk8;
} MeterDefaults;

typedef struct {
    f32 target_z;
    f32 angle;
} CameraLookTarget;

extern s32 D_800C3084;
extern s32 D_800FBE7C[];
extern MeterDefaults D_800FBE48;
extern s32 D_800FBE64[];
extern f32 D_801B54F0;
extern f32 D_801B54F8;
extern f32 D_801B54FC;
extern CameraLookTarget D_801B5500;
extern s32 get_interpolated_terrain_height_wrapper(s32 x, s32 z);

extern s32 nuSiSendMesg(s16 mesg, void* dataPtr);

void func_ovl0_80202A00(void) {
    nuSiSendMesg(0x305, 0);
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80202A20);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80202D04);

void func_ovl0_802038D0(void) {
    f32 point[3];
    f32 angle;
    f32 cosine;
    s32 *radius_ptr;

    get_shot_data();
    if (D_800C3084 & 1) {
        D_800FBE7C[2] = -0x10E00;
        D_800FBE7C[0] = -0x10E00;
    } else {
        D_800FBE7C[2] = -0x12C00;
        D_800FBE7C[0] = -0x2D000;
    }
    angle = (f32)D_800FBE48.unk0 * 0.0000958738f;
    point[0] = cosf(angle) * (f32)D_800FBE7C[0] + (f32)D_800FBE64[0];
    if (point[0] < 0.0f) {
        point[0] = 0.0f;
    } else if (point[0] >= 4347904.0f) {
        point[0] = 4347903.0f;
    }
    point[2] = sinf(angle) * (f32)D_800FBE7C[0] + (f32)D_800FBE64[2];
    if (point[2] < 0.0f) {
        point[2] = 0.0f;
    } else if (point[2] >= 8.542208e6f) {
        point[2] = 8.542207e6f;
    }
    cosine = cosf(angle);
    radius_ptr = &D_800FBE7C[0];
    point[0] = cosine * (f32)(*radius_ptr + 0x280000) + (f32)D_800FBE64[0];
    point[2] = sinf(angle) * (f32)(*radius_ptr + 0x280000) + (f32)D_800FBE64[2];
    point[1] = D_801B54F0;
    get_interpolated_terrain_height_wrapper((s32)point[0], (s32)point[2]);
    D_801B5500.angle = angle;
    D_801B54F8 = point[0] * 0.0009765625f;
    D_801B5500.target_z = point[2] * 0.0009765625f;
    D_801B54FC = (point[1] * 0.0009765625f) - ((f32)D_800FBE7C[2] * 0.0009765625f);
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80203B4C);

extern void func_800934CC(s32 arg0);
extern void set_color_framebuffer_count(u32 buffer_count);
extern void func_8002A97C(s32 arg0, s32 arg1);
extern void update_object_group_by_id(s32 index);
extern s32 D_800B680C;
extern s32 D_800C306C;
extern s32 D_800C3070;
extern s32 D_800C308C;
extern s8 D_801061BD;
extern s8 D_801061C4;
extern u8 D_80106240[];
extern u16 D_800C307A;
extern s32 D_800C3090;

void func_ovl0_80203DAC(void) {
    *D_800FBE7C = -0x12C00;
    D_800C308C = 1;
    D_800C3070 = 0;
    D_800C3090 = 0;
    D_80106240[0] = 0x47;
    D_800C307A = D_800FBE48.unk0;
    func_800934CC(3);
    D_800C306C = 4;
    D_801061BD = 0;
    D_801061C4 = 0;
    if (D_800B680C == 3) {
        set_color_framebuffer_count(3);
        func_8002A97C(4, 0);
    }
    update_object_group_by_id(8);
}

extern void func_80050DA0(s32 sfx, s32 channel, s32 volume, s32 reverb, s32 pan);
extern s32 D_ovl0_80227194;
extern u16 D_801B55CC;

void func_ovl0_80203E58(void) {
    if ((D_801B55CC & 0x2020) && !(D_800C3084 & 2)) {
        if ((D_800C3090 == 0) && (D_ovl0_80227194 == 0)) {
            func_80050DA0(0x26, 8, 0x3C, 0x14, 0xE6);
        }
        D_800C3090 = 1;
        return;
    }
    update_object_group_by_id(8);
    D_800C3090 = 0;
}

extern s32 D_800C3080;
extern void func_ovl0_80203B4C(void);

static inline void func_ovl0_80203EEC_set_radius(s32 index) {
    if (index != 0) {
        s32 radius[3];

        radius[0] = -0xF000;
        radius[2] = -0x4E00;
        D_800FBE7C[0] = radius[index];
    } else {
        D_800FBE7C[0] = -0xF000;
        D_800FBE7C[2] = -0x4E00;
    }
}

void func_ovl0_80203EEC(void) {
    func_ovl0_80203EEC_set_radius(0);
    D_800C3070 = 0;
    D_800C3080 = 0;
    func_ovl0_80203B4C();
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80203F30);

extern f64 D_ovl0_80224EF8;
extern f64 D_ovl0_80224F00;
extern f64 D_ovl0_80224F08;
extern f64 D_ovl0_80224F10;

void func_ovl0_80204104(void) {
    if (D_800C3084 & 2) {
        if (D_801B55CC & 4) {
            D_800FBE7C[2] = (s32) ((f64) D_800FBE7C[2] + D_ovl0_80224EF8);
        } else if (D_801B55CC & 8) {
            D_800FBE7C[2] = (s32) ((f64) D_800FBE7C[2] - D_ovl0_80224F00);
        }
    } else {
        if (D_801B55CC & 4) {
            D_800FBE7C[2] = (s32) ((f64) D_800FBE7C[2] + D_ovl0_80224F08);
        } else if (D_801B55CC & 8) {
            D_800FBE7C[2] = (s32) ((f64) D_800FBE7C[2] - D_ovl0_80224F10);
        }
    }
}

void func_ovl0_802041C8(void) {
    u16 var_v0;

    if (D_801B55CC & 2) {
        var_v0 = D_800C307A - 0xC8;
    } else if (D_801B55CC & 1) {
        var_v0 = D_800C307A + 0xC8;
    } else {
        return;
    }
    D_800C307A = var_v0;
}

extern f64 D_ovl0_80224F18;
extern f64 D_ovl0_80224F20;
extern f64 D_ovl0_80224F28;
extern f64 D_ovl0_80224F30;
extern s32 active_meter_category;

void func_ovl0_80204210(void) {
    s32 temp_fv1;
    s32 temp_fv1_2;
    s32 var_a0;

    if (D_800C3084 & 2) {
        if (active_meter_category == 2) {
            var_a0 = 0xA;
        } else {
            var_a0 = (-(active_meter_category != 1) & 0xF) | 0xD;
        }
        if (D_801B55CC & 2) {
            D_800FBE7C[2] = (s32) ((f64) D_800FBE7C[2] + ((f64) var_a0 * D_ovl0_80224F18));
            return;
        }
        if (D_801B55CC & 1) {
            D_800FBE7C[2] = (s32) ((f64) D_800FBE7C[2] - ((f64) var_a0 * D_ovl0_80224F20));
        }
    } else if (D_801B55CC & 2) {
        temp_fv1 = (s32) ((f64) D_800FBE7C[2] + D_ovl0_80224F28);
        D_800FBE7C[2] = temp_fv1;
        if (temp_fv1 < -0x5A00) {
            D_800FBE7C[0] = (s32) ((f64) D_800FBE7C[0] + D_ovl0_80224F28);
        }
    } else if (D_801B55CC & 1) {
        temp_fv1_2 = (s32) ((f64) D_800FBE7C[2] - D_ovl0_80224F30);
        D_800FBE7C[2] = temp_fv1_2;
        if (temp_fv1_2 > -0x96000) {
            D_800FBE7C[0] = (s32) ((f64) D_800FBE7C[0] - D_ovl0_80224F30);
        }
    }
}

extern f64 D_ovl0_80224F38;
extern f64 D_ovl0_80224F40;
extern f64 D_ovl0_80224F48;
extern f64 D_ovl0_80224F50;

void func_ovl0_8020439C(void) {
    if (D_800C3084 & 2) {
        if (D_801B55CC & 1) {
            D_800FBE7C[0] = (s32) ((f64) D_800FBE7C[0] + D_ovl0_80224F38);
        } else if (D_801B55CC & 2) {
            D_800FBE7C[0] = (s32) ((f64) D_800FBE7C[0] - D_ovl0_80224F40);
        }
    } else {
        if (D_801B55CC & 1) {
            D_800FBE7C[0] = (s32) ((f64) D_800FBE7C[0] + D_ovl0_80224F48);
        } else if (D_801B55CC & 2) {
            D_800FBE7C[0] = (s32) ((f64) D_800FBE7C[0] - D_ovl0_80224F50);
        }
    }
}

extern s8 D_801B55D9;
extern f64 D_ovl0_80224F58;
extern f64 D_ovl0_80224F60;
extern f64 D_ovl0_80224F68;
extern f64 D_ovl0_80224F70;

void func_ovl0_80204460(void) {
    s32 units;

    if (D_800C3084 & 2) {
        if (active_meter_category == 2) {
            units = 0xA;
        } else {
            units = 0x14;
            if (active_meter_category == 1) {
                units = 0xF;
            }
        }
        if (D_801B55CC & 8) {
            D_800FBE7C[0] = (s32)((f64)D_800FBE7C[0] + (f64)units * D_ovl0_80224F58);
        } else if (D_801B55CC & 4) {
            D_800FBE7C[0] = (s32)((f64)D_800FBE7C[0] - (f64)units * D_ovl0_80224F60);
        }
    } else {
        if (D_801B55CC & 8) {
            D_800FBE7C[0] = (s32)((f64)D_800FBE7C[0] + D_ovl0_80224F68);
        } else if (D_801B55CC & 4) {
            D_800FBE7C[0] = (s32)((f64)D_800FBE7C[0] - D_ovl0_80224F70);
        }
    }
}

void func_ovl0_80204568(void) {
    s8 input;
    s32 magnitude;
    s32 *value;

    input = D_801B55D9;
    magnitude = input;
    if (input < 0) {
        magnitude = -input;
    }
    if (magnitude >= 9) {
        value = &D_800FBE7C[2];
        *value -= (input / 16) * 0x600;
    }
}

void func_ovl0_802045BC(void) {
    if (D_800C3084 & 2) {
        s8 input;
        s32 magnitude;

        input = D_801B55D9;
        magnitude = input;
        if (input < 0) {
            magnitude = -input;
        }
        if (magnitude >= 9) {
            D_800FBE7C[0] += (input / 4) * 0x180;
        }
    } else {
        s8 input;
        s32 magnitude;

        input = D_801B55D9;
        magnitude = input;
        if (input < 0) {
            magnitude = -input;
        }
        if (magnitude >= 9) {
            D_800FBE7C[0] += (input / 4) * 0xF00;
        }
    }
}

extern s8 D_801B55D8;
extern s16 D_800C3078;
extern s32 D_ovl0_80223E50;
extern s32 flag_is_set(s32 flag);
extern s32 rand(void);

#define ABS(x) ((x) < 0 ? -(x) : (x))

/* Turns the meter angle by the stick input: half the input, scaled by |input / 4| when non-zero. */
static inline void func_ovl0_8020466C_turn_angle(MeterDefaults *meter, s16 old, s32 stick) {
    u16 *angle = &meter->unk0;
    s32 half = stick / 2;
    s32 scale;
    s32 result;

    result = old - (((scale = ABS(stick / 4)) != 0) ? half * scale : half);
    *angle = result;
}

void func_ovl0_8020466C(s32 arg0) {
    s32 stick;
    s32 half;
    s16 old;
    s32 scale;
    s32 buttons;
    s32 *hold;
    s32 speed;
    MeterDefaults *meter;

    stick = D_801B55D8;
    meter = &D_800FBE48;
    old = meter->unk0;
    if (D_800C3090 != 0) {
        return;
    }
    if (arg0 == 0) {
        func_ovl0_8020466C_turn_angle(meter, old, stick);
    } else {
        half = stick / 2;
        D_800C3078 -= ((scale = ABS(stick / 6)) != 0) ? half * scale : half;
    }
    buttons = D_801B55CC & 0x300;
    if (buttons == 0 || buttons == 0x300) {
        D_ovl0_80223E50 = 0;
    } else if (buttons & 0x200) {
        hold = &D_ovl0_80223E50;
        if (*hold < 0) {
            *hold = 0;
        }
        if (++*hold > 60) {
            *hold = 60;
        }
    } else {
        hold = &D_ovl0_80223E50;
        if (*hold > 0) {
            *hold = 0;
        }
        if (--*hold < -60) {
            *hold = -60;
        }
    }
    speed = D_ovl0_80223E50;
    scale = speed;
    if (speed < 0) {
        scale = -speed;
    }
    D_800FBE48.unk0 += speed * (scale / 6 + 3);
    if (!flag_is_set(0x9F)) {
        if (D_800C308C == 0 || D_800C308C == 5) {
            if (old == (s16)D_800FBE48.unk0) {
                if (D_800B680C != 3) {
                    func_8002A97C(3, 2);
                    set_color_framebuffer_count(2);
                }
            } else if (D_800B680C == 3) {
                set_color_framebuffer_count(3);
                func_8002A97C(4, 0);
            }
        } else {
            set_color_framebuffer_count(3);
        }
    }
    if (flag_is_set(0x9F)) {
        if (!(rand() & 1)) {
            set_color_framebuffer_count(2);
        } else {
            set_color_framebuffer_count(3);
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80204910);

extern s8 g_terrain_vtx_xform_mode;
extern s32 active_terrain_index;
extern s32 D_800FBE94[];

/* Recomputes the overlay_0 state word D_800C3084 from the current shot flags,
 * terrain entry, transform mode and meter category. Each condition ORs its
 * bit in after a fresh reset of the word. */
void func_ovl0_802054CC(void) {
    s32 temp_v0;
    s8* xform_mode;

    temp_v0 = get_shot_data()->flags;
    D_800C3084 = 0;
    if (temp_v0 & 1) {
        D_800C3084 = 2;
    }
    xform_mode = &g_terrain_vtx_xform_mode;
    if (*xform_mode != 0) {
        D_800C3084 |= 2;
    }
    if (get_table_entry((u32) active_terrain_index)->id == 0xBU) {
        D_800C3084 |= 1;
    }
    if (*xform_mode != 0) {
        D_800C3084 |= 3;
    }
    if (active_meter_category == 2) {
        D_800C3084 |= 4;
    }
    if (((f32) D_800FBE94[1] * 0.0006510417f) <= 600.0f) {
        D_800C3084 |= 8;
    }
}

extern u32 D_ovl0_80227198;

void func_ovl0_802055F8(void) {
    D_ovl0_80227198 = osGetCount();
}

s32 func_ovl0_8020561C(s32 arg0) {
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80205624);

extern s8 D_800FC859;
extern void func_8006034C(u8 value);
extern void func_ovl0_801FD8DC(void);
extern void reload_scene_assets(BallObject *arg0);
extern s32 report_inactive_overlay_module(s32 module_id);

void func_ovl0_802059C0(s32 arg0, BallObject *arg1) {
    PlayerState *player;
    s32 i;
    s32 terrain_height;

    player = &player_records[arg0];
    update_object_group_by_id(8);
    for (i = 0; i < 3; i++) {
        if ((s8) player->unknown_72[6 + i] != 0) {
            player->unknown_72[6 + i] = 1;
        }
        func_8006034C(0);
    }
    player->unknown_A8 = 0;
    arg1->unk18 = player->unknown_10;
    arg1->unk1C = player->unknown_14;
    arg1->unk20 = player->unknown_18;
    arg1->unkA5 = arg0;
    arg1->unkA0 = 0;
    arg1->unk8C = 0;
    terrain_height =
        get_interpolated_terrain_height_wrapper(arg1->unk18, arg1->unk20);
    if (terrain_height < arg1->unk1C) {
        arg1->unk1C = terrain_height;
    }
    arg1->unkA3 = player->unknown_1C;
    arg1->unk90 = 3;
    arg1->unkA1 = 0;
    reload_scene_assets(arg1);
    if (D_800FC859 != 0 || report_inactive_overlay_module(1) != 0) {
        func_ovl0_801FD8DC();
    }
}

extern s32 game_mode;
extern s8 rumble_disable_flag;
extern s8 D_801B71EE;
extern u8 D_801B727A;
extern s32 D_ovl0_80223E5C;
extern const char D_ovl0_80224FC0[];

void func_ovl0_80205AE4(s32 arg0, s32 arg1) {
    s32 temp_a0;
    u8 *temp_v0;
    u8 *temp_s0;

    temp_a0 = arg0 * 0xB8;
    if (*(&D_801B71EE + temp_a0) != 0) {
        return;
    }
    if (rumble_disable_flag != 0) {
        return;
    }
    if (game_mode == 3) {
        return;
    }
    if (game_mode == 9) {
        return;
    }
    if (D_ovl0_80223E5C >= 2) {
        return;
    }
    if ((g_terrain_vtx_xform_mode != 0) & (arg1 != 7)) {
        return;
    }
    temp_v0 = &D_801B727A + temp_a0;
    temp_s0 = temp_v0 + arg1;
    if ((u8) *temp_s0 >= 0xFFU) {
        return;
    }
    osSyncPrintf(D_ovl0_80224FC0, arg1);
    *temp_s0 += 1;
}

extern const char D_ovl0_80224FD4[];

s32 func_ovl0_80205BB8(s32 player_index, s32 event_index) {
    s32 count;
    s32 points;

    count = player_records[player_index].event_counts[event_index];
    switch (event_index) {
        case 0:
        case 1:
            points = 2;
            break;
        case 4:
            points = 8;
            break;
        case 2:
        case 5:
            points = 4;
            break;
        case 7:
            points = 20;
            if (g_terrain_vtx_xform_mode == 0) {
                break;
            }
        case 6:
        case 9:
        case 11:
            points = 10;
            break;
        case 8:
            points = 16;
            break;
        case 3:
        case 10:
            points = 6;
            break;
        default:
            osSyncPrintf(D_ovl0_80224FD4);
            points = 0;
            break;
    }
    return count * points;
}

s32 func_ovl0_80205C78(void) {
    s32 player_index;
    s32 event_index;
    s32 player_total;
    s32 grand_total;

    grand_total = 0;
    for (player_index = 0; player_index < scenario_state_id.active_player_count;
         player_index++) {
        player_total = 0;
        for (event_index = 0; event_index < 12; event_index++) {
            player_total += func_ovl0_80205BB8(player_index, event_index);
            player_records[player_index].event_counts[event_index] = 0;
        }
        player_records[player_index].event_total = player_total;
        grand_total += player_total;
    }
    return grand_total;
}

const char D_ovl0_80225020[0x14] = "Player%d EXP %d\n";
extern void check_and_print_grid(char* str, s32 col, s32 row);
extern s32 active_player_count;

void func_ovl0_80205D44(void) {
    char buf[0x88];
    s32 event_index;
    s32 player_total;
    s32 player_index;

    player_index = 0;
    if (active_player_count > 0) {
        player_total = 0;
        do {
            event_index = 0;
            do {
                player_total += func_ovl0_80205BB8(player_index, event_index);
                event_index += 1;
            } while (event_index < 12);
            sprintf(buf, D_ovl0_80225020, player_index, player_total);
            check_and_print_grid(buf, 2, player_index + 3);
            player_index += 1;
            player_total = 0;
        } while (player_index < active_player_count);
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80205DE8);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80206354);

s32 func_ovl0_80206BD0(s32 player_index) {
    if (rumble_disable_flag != 0) {
        return 0;
    }
    if (!flag_is_set(0x3B)) {
        if (*(&D_801B71EE + player_index * 0xB8) != 0) {
            return 0;
        }
        if (game_mode == 3) {
            return 0;
        }
    }
    if (player_records[player_index].base_score >= 5) {
        return 0;
    }
    if (player_records[player_index].character_index >= 14) {
        return 0;
    }
    if (g_terrain_vtx_xform_mode != 0) {
        return 0;
    }
    switch (game_mode) {
    case 0:
    case 1:
        return 1;
    case 2:
        return 0;
    case 3:
    case 4:
    case 5:
        return 1;
    case 6:
        return 0;
    case 7:
        return 1;
    case 8:
    case 9:
    case 10:
        return 0;
    default:
        return 0;
    }
}


s32 func_ovl0_80206CC8(void) {
    if (rumble_disable_flag == 0) {
        return game_mode == 9;
    }
    return 0;
}

extern GameModeRecord D_800FEDD8[];
extern CourseSelectionRecord D_80105F40[];
extern s32 active_club_id;
s32 get_hole_score_result_code(s32 hole_score);


typedef struct {
    s32 unk0;
    s32 unk4;
} MeterDefaults2;

extern MeterDefaults2 D_800FBE5C;
extern s32 round_hole_index;
extern s32 scenario_mode_id;

void func_ovl0_80206CF0(s32 player_index) {
    GameModeRecord *record;
    u32 score_index;

    score_index = player_records[player_index].base_score - 1;
    if (score_index < 5U) {
        record = &D_800FEDD8[player_index];
        record->game_mode = scenario_state_id.game_mode;
        record->scenario_mode_id = scenario_state_id.scenario_mode_id;
        record->round_hole_index = scenario_state_id.round_hole_index;
        record->unknown_3A[0] = get_hole_score_result_code(player_records[player_index].base_score);
        record->unknown_3A[1] = player_records[player_index].character_index;
        record->unknown_3A[2] = player_records[player_index].unknown_1E[4];
        record->unknown_3A[3] = player_records[player_index].asset_variant;
        record->unknown_3E = player_records[player_index].base_score;
        record->unknown_40 = player_records[player_index].unknown_88;
        record->environment = hole_environment;
        record->mode_aux_fields[score_index].unknown_00 = D_800FBE48.unk0;
        record->mode_aux_fields[score_index].unknown_02[0] = D_800FBE48.unk4;
        record->mode_aux_fields[score_index].unknown_02[1] = D_800FBE48.unk8;
        record->mode_aux_fields[score_index].unknown_02[2] = active_meter_category;
        record->mode_aux_fields[score_index].unknown_02[3] = active_club_id;
        record->mode_aux_fields[score_index].unknown_02[4] = D_800FBE5C.unk0;
        record->mode_aux_fields[score_index].unknown_02[5] = D_800FBE5C.unk4;
    }
}

void func_ovl0_80206E88(s32 player_index) {
    CourseSelectionRecord *record;
    u32 score_index;

    score_index = player_records[player_index].base_score - 1;
    if (score_index < 6U) {
        record = &D_80105F40[player_index];
        record->scenario_mode_id = scenario_mode_id;
        record->round_hole_index = round_hole_index;
        record->unknown_36[0] =
            get_hole_score_result_code(player_records[player_index].base_score);
        record->unknown_36[1] = player_records[player_index].character_index;
        record->unknown_36[2] = player_records[player_index].unknown_1E[4];
        record->unknown_36[3] = player_records[player_index].asset_variant;
        record->unknown_3A = player_records[player_index].base_score;
        record->unknown_3C = player_records[player_index].unknown_88;
        record->environment = hole_environment;
        record->mode_aux_fields[score_index].unknown_00 = D_800FBE48.unk0;
        record->mode_aux_fields[score_index].unknown_02[0] = D_800FBE48.unk4;
        record->mode_aux_fields[score_index].unknown_02[1] = D_800FBE48.unk8;
        record->mode_aux_fields[score_index].unknown_02[2] = active_meter_category;
        record->mode_aux_fields[score_index].unknown_02[3] = active_club_id;
        record->mode_aux_fields[score_index].unknown_02[4] = D_800FBE5C.unk0;
        record->mode_aux_fields[score_index].unknown_02[5] = D_800FBE5C.unk4;
    }
}

/* Returns 1 if PlayerState bytes 0x78..0x7A are all zero, else 0. */
s32 func_ovl0_80207014(PlayerState *player, s32 record_offset) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if ((s8) player->unknown_72[6 + i] != 0) {
            return 0;
        }
    }
    return 1;
}

/* Returns 0 if any of PlayerState bytes 0x78..0x7A equals 1, else 1. */
s32 func_ovl0_80207048(PlayerState *player) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if (*((s8 *)player + i + 0x78) == 1) {
            return 0;
        }
    }
    return 1;
}

u8* func_8005AF50(void);
void func_8005DE60(s32);

void func_ovl0_80207080(s32 player_index) {
    s32 var_a0;
    u8 *temp_v0;
    u8 *temp_v1;
    u8 *temp_v1_2;
    u8 temp_v0_2;
    u8 temp_v0_3;
    u8 *save;

    save = func_8005AF50();
    temp_v1 = save + ((scenario_mode_id * 6) + 0x1320) + round_hole_index;
    var_a0 = round_hole_index;
    temp_v0_2 = *temp_v1 + 1;
    *temp_v1 = temp_v0_2;
    if ((s8) temp_v0_2 < 0) {
        *temp_v1 = 0;
        goto block_4;
    }
    if ((s8) temp_v0_2 >= 0x64) {
        *temp_v1 = 0x63;
    }
block_4:
    if (player_records[player_index].character_index < 0x12) {
        temp_v0 = func_8005AF50() + ((scenario_mode_id * 0x6C) + 0x1344);
        temp_v1_2 = temp_v0 + (round_hole_index * 0x12) + player_records[player_index].character_index;
        var_a0 = player_records[player_index].character_index;
        temp_v0_3 = *temp_v1_2 + 1;
        *temp_v1_2 = temp_v0_3;
        if ((s8) temp_v0_3 < 0) {
            *temp_v1_2 = 0;
        } else if ((s8) temp_v0_3 >= 0x64) {
            *temp_v1_2 = 0x63;
        }
    }
    func_8005DE60(var_a0);
}

extern s8 D_800BAA04;

void func_ovl0_80207080(s32 player_index);

void func_ovl0_802071B8(s32 arg0) {
    s32 record_offset;

    if (game_mode == 9) {
        record_offset = arg0 * sizeof(PlayerState);
        if (player_records[arg0].base_score <= D_800BAA04 &&
            (s8)player_records[arg0].unknown_1E[0] == 0 &&
            func_ovl0_80207014(&player_records[arg0], record_offset) != 0) {
            func_ovl0_80207080(arg0);
        }
    }
}

extern s32 func_800544B4(s32 arg0, s32 arg1);

s32 func_ovl0_80207250(s32 arg0, s32 arg1) {
    s32 kind;
    s32 anim;

    kind = func_800544B4(arg0, arg1);
    switch (kind) {
    case 0:
        anim = 0;
        break;
    case 1:
        anim = 3;
        break;
    case 2:
        anim = 6;
        break;
    case 3:
        anim = 9;
        break;
    default:
        anim = 0;
        break;
    }
    return anim;
}

s32 func_ovl0_802072AC(s32 arg0, s32 arg1, s32 arg2) {
    s32 kind;
    s32 base;

    kind = func_800544B4(arg0, arg1);
    switch (kind) {
    case 0:
        base = 1;
        break;
    case 1:
        base = 4;
        break;
    case 2:
        base = 7;
        break;
    case 3:
        base = 10;
        break;
    default:
        base = 0;
        break;
    }
    return base + (arg2 == 2);
}

extern void animation_play(s32 id, s32 animation);
s32 func_ovl0_802072AC(s32 arg0, s32 arg1, s32 arg2);

void func_ovl0_80207328(s32 arg0, s32 arg1) {
    animation_play(arg0, func_ovl0_80207250(arg0, arg1));
}

void func_ovl0_80207358(s32 arg0, s32 arg1, s32 arg2) {
    animation_play(arg0, func_ovl0_802072AC(arg0, arg1, arg2));
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207388);

void func_ovl0_80207660(void) {
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207668);

/* Clamps the three signed flag bytes at PlayerState 0x78..0x7A: values >= 2 become 0. */
void func_ovl0_802078C4(PlayerState *player) {
    s32 i;

    for (i = 0; i < 3; i++) {
        if ((s8)player->unknown_72[i + 6] >= 2) {
            player->unknown_72[i + 6] = 0;
        }
    }
}

extern s32 get_current_course_id(void);
extern s32 D_ovl0_80223EF0[9][7];
extern s32 D_ovl0_80223FEC;

void func_ovl0_802078F4(void) {
    s32 *sfx_ptr;
    s32 *course_row;
    s32 frame_count;
    s32 sfx_index;

    frame_count = D_ovl0_80223FEC + 1;
    D_ovl0_80223FEC = frame_count;
    if (frame_count == 0xF0) {
        sfx_index = guRandom() % 7;
        D_ovl0_80223FEC = 0;
        if (D_ovl0_80223EF0[get_current_course_id()][sfx_index] != -1) {
            course_row = D_ovl0_80223EF0[get_current_course_id()];
            sfx_ptr = (s32 *) (sfx_index * 4 + (s32) course_row);
            func_80050DA0(*sfx_ptr, 0xA, 0x5F, 0xF, guRandom() % 255);
        }
    }
}

extern s32 clear_animation_slot(s32 index);
extern void func_80095D64(void);

void func_ovl0_80207A10(s32 arg0) {
    clear_animation_slot(0);
    func_80095D64();
}

extern u32 calculate_hypotenuse_safe(s32 x, s32 z);
extern s32 func_8003DFD0(s32 x, s32 z);
extern s8 D_ovl6_80250B60;
extern s32 D_ovl6_8024DD20;
extern s32 D_ovl6_8024DD28;
extern s32 D_ovl6_8024DD2C;
extern s32 D_ovl6_8024DD34;
extern s32 camera_position_x;
extern s32 camera_position_z;

typedef struct {
    s32 unknown_00;
} Overlay6Word;

/* Struct-typed: as a plain s32 global, GCC 2.7.2 hoists this load above the pin store. */
extern Overlay6Word D_ovl6_80250B30;

void func_ovl0_80207A34(s32 player_index) {
    s32 distance;
    TerrainAttrEntry* terrain;

    if (player_records[player_index].base_score == 1) {
        distance = -1;
        terrain = get_table_entry(D_ovl6_80250B60);
        switch (terrain->id) {
            case 0:
            case 1:
            case 2:
            case 0xB:
            case 0xC:
                distance = calculate_hypotenuse_safe(D_ovl6_8024DD20 - D_ovl6_8024DD2C,
                                                     D_ovl6_8024DD28 - D_ovl6_8024DD34);
                break;
        }
        player_records[player_index].unknown_80 = distance;

        distance = -1;
        if (D_ovl6_80250B30.unknown_00 != 0) {
            distance = 0;
        } else if (terrain->id == 0xBU) {
            distance = func_8003DFD0(camera_position_x - D_ovl6_8024DD2C,
                                     camera_position_z - D_ovl6_8024DD34);
        }
        player_records[player_index].unknown_84 = distance;
    }
}

void func_ovl0_80207B8C(s32 player_index) {
    if (player_records[player_index].unknown_80 == -2) {
        player_records[player_index].unknown_80 = -1;
    }
    if (player_records[player_index].unknown_84 == -2) {
        player_records[player_index].unknown_84 = -1;
    }
}

typedef struct {
    u8 pad_00[0x88];
    s32 unk_88;
} RumbleStateBlock;

extern RumbleStateBlock D_80105168;
extern s8 D_801B71F7;
extern u32 controller_handle_array[];
extern s32 nuContRmbCheck(u32 contNo);
extern void nuContRmbStart(u32 contNo, u16 freq, u16 frame);

void func_ovl0_80207BEC(s32 arg0) {
    s32 temp_s1;
    s32 temp_v0;
    s32 temp_s0;

    temp_s1 = D_80105168.unk_88;
    if ((temp_s1 == 1) && (active_player_count >= 2)) {
        temp_s0 = *(&D_801B71F7 + (arg0 * 0xB8));
        func_ovl0_80202A00();
        if (((u32) temp_s0 < 4U) && (rumble_disable_flag == 0)) {
            temp_v0 = nuContRmbCheck(controller_handle_array[temp_s0]);
            if (temp_v0 != temp_s1) {
                if (temp_v0 < 2) {
                    if (temp_v0 == 0) {
                        nuContRmbStart(controller_handle_array[temp_s0], 0x100, 8);
                    }
                }
            }
        }
    }
}

extern s32 D_801B60A8;
extern ControllerState D_800FBDC0[];
extern void func_80067A60(s32 bank, s32 channel, s32 entry, s32 volume, s32 pan);

void func_ovl0_80207CAC(void) {
    s32 i;
    s32 state;
    s8 port;
    u16 buttons;

    if (D_80105168.unk_88 == 1) {
        for (i = 0; i < active_player_count; i++) {
            state = player_records[i].state_code;
            if (((state == 0x11) | (state == 0x14)) != 0) {
                port = player_records[i].unknown_27[0];
                if (port != player_records[D_801B60A8].unknown_27[0]) {
                    if (port < 4) {
                        buttons = D_800FBDC0[port].pressed;
                        if (buttons & 0x800) {
                            func_80067A60(player_records[i].character_index, i, 0, 0x7F, player_records[i].unknown_2A);
                        } else if (buttons & 0x400) {
                            func_80067A60(player_records[i].character_index, i, 2, 0x7F, player_records[i].unknown_2A);
                        } else if (buttons & 0x200) {
                            func_80067A60(player_records[i].character_index, i, 4, 0x7F, player_records[i].unknown_2A);
                        } else if (buttons & 0x100) {
                            func_80067A60(player_records[i].character_index, i, 6, 0x7F, player_records[i].unknown_2A);
                        } else if (buttons & 0x8) {
                            func_80067A60(player_records[i].character_index, i, 8, 0x7F, player_records[i].unknown_2A);
                        } else if (buttons & 0x4) {
                            func_80067A60(player_records[i].character_index, i, 0xA, 0x7F, player_records[i].unknown_2A);
                        } else if (buttons & 0x2) {
                            func_80067A60(player_records[i].character_index, i, 0xC, 0x7F, player_records[i].unknown_2A);
                        } else if (buttons & 0x1) {
                            func_80067A60(player_records[i].character_index, i, 0xE, 0x7F, player_records[i].unknown_2A);
                        }
                    }
                }
            }
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207E88);
