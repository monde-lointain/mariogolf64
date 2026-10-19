#include "common.h"
#include "../main/rom_asset.h"
#include "../main/session.h"
#include "../main/club.h"
#include "../main/terrain.h"

extern s32 nuSiSendMesg(s16 mesg, void* dataPtr);

void func_ovl0_80202A00(void) {
    nuSiSendMesg(0x305, 0);
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80202A20);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80202D04);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802038D0);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80203B4C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80203DAC);

extern void func_80050DA0(s32 sfx, s32 channel, s32 volume, s32 reverb, s32 pan);
extern void update_object_group_by_id(s32 index);
extern s32 D_800C3084;
extern s32 D_800C3090;
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

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80203EEC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80203F30);

extern s32 D_800FBE7C[];
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

extern u16 D_800C307A;

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

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_8020439C);

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

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_8020466C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80204910);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802054CC);

extern u32 D_ovl0_80227198;

void func_ovl0_802055F8(void) {
    D_ovl0_80227198 = osGetCount();
}

s32 func_ovl0_8020561C(s32 arg0) {
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80205624);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802059C0);

extern s32 game_mode;
extern s8 rumble_disable_flag;
extern s8 D_801B71EE;
extern u8 D_801B727A;
extern s32 D_ovl0_80223E5C;
extern const char D_ovl0_80224FC0[];
extern s8 g_terrain_vtx_xform_mode;

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

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80205BB8);

s32 func_ovl0_80205BB8(s32 player_index, s32 event_index);

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

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80205D44);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80205DE8);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80206354);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80206BD0);


s32 func_ovl0_80206CC8(void) {
    if (rumble_disable_flag == 0) {
        return game_mode == 9;
    }
    return 0;
}

extern GameModeRecord D_800FEDD8[];
extern s32 active_club_id;
s32 get_hole_score_result_code(s32 hole_score);

typedef struct {
    u16 unk0;
    s32 unk4;
    s32 unk8;
} MeterDefaults;

typedef struct {
    s32 unk0;
    s32 unk4;
} MeterDefaults2;

extern MeterDefaults D_800FBE48;
extern MeterDefaults2 D_800FBE5C;

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

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80206E88);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207014);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207048);

u8* func_8005AF50(void);
void func_8005DE60(s32);
extern s32 round_hole_index;
extern s32 scenario_mode_id;

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

s32 func_ovl0_80207014(PlayerState *player, s32 record_offset);
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

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207250);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802072AC);

extern void animation_play(s32 id, s32 animation);
s32 func_ovl0_80207250(void);
s32 func_ovl0_802072AC(s32 arg0, s32 arg1, s32 arg2);

void func_ovl0_80207328(s32 arg0) {
    animation_play(arg0, func_ovl0_80207250());
}

void func_ovl0_80207358(s32 arg0, s32 arg1, s32 arg2) {
    animation_play(arg0, func_ovl0_802072AC(arg0, arg1, arg2));
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207388);

void func_ovl0_80207660(void) {
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207668);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802078C4);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802078F4);

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
extern s32 active_player_count;
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

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207CAC);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207E88);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80207F70);

/* Returns whether bit 0x4000 of D_80104990 is set. The dead 8-byte frame with
 * the never-reloaded `sw $v0,0(sp)` is the same nested-function stand-in as
 * lerp_s32 (func_8008D100.c); parent func_ovl0_802088B0 is not recovered. */
extern u16 D_80104990;

s32 func_ovl0_802080B4(void) {
  s32 r;
  volatile s32 unused = r;
  r = D_80104990;
  r &= 0x4000;
  return r != 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802080D4);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80208120);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_8020816C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802081F8);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80208354);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_8020845C);

extern s8 D_801061BF;
extern s8 D_801061C0;
extern s8 D_801061C1;
extern s8 D_801061C2;
extern s8 D_801061C3;
extern s8 D_801061C5;

/* Dead 8-byte frame and `sw $v0,0(sp)`: static-chain home of the nested-function original (parent func_ovl0_802088B0 is not recovered); `unused` reproduces them, as in func_ovl7_802068B4. */
void func_ovl0_802087D0(s8 arg0) {
    s32 r;
    volatile s32 unused = r;

    D_801061BF = arg0;
    D_801061C0 = arg0;
    D_801061C1 = arg0;
    D_801061C2 = arg0;
    D_801061C3 = arg0;
    D_801061C5 = arg0;
}

extern s32 active_terrain_index;
extern s32 D_ovl0_8022715C;
extern u8* func_80037E50(ClubShot* club, s32 arg1, s32 arg2, s32 arg3);

/* Dead `sw $v0,0x10($sp)` in the first jal's delay slot: the `unused` local is the stand-in, as in lerp_s32 (func_8008D100.c). */
s8 func_ovl0_80208810(void) {
    s32 r;
    volatile s32 unused = r;
    return ((s8 *)func_80037E50(get_shot_data(), active_terrain_index, active_meter_category, D_ovl0_8022715C))[5];
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_80208850);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_802088B0);

extern void* heap3_alloc(u32 size);
extern void* D_ovl0_80227C90[2];
extern void* D_ovl0_80227C98;
extern s32 D_ovl0_80224034;

void func_ovl0_8020D6F0(void) {
    RomAssetLoad asset;
    void* temp_v0;
    void* temp_v0_2;
    void* temp_v0_3;

    temp_v0 = heap3_alloc(get_rom_asset_allocation_size(0x6E0, &asset));
    D_ovl0_80227C90[0] = temp_v0;
    load_rom_asset_payload(temp_v0, &asset);
    temp_v0_2 = heap3_alloc(get_rom_asset_allocation_size(0x6E5, &asset));
    D_ovl0_80227C90[1] = temp_v0_2;
    load_rom_asset_payload(temp_v0_2, &asset);
    temp_v0_3 = heap3_alloc(get_rom_asset_allocation_size(0x6F0, &asset));
    D_ovl0_80227C98 = temp_v0_3;
    load_rom_asset_payload(temp_v0_3, &asset);
    D_ovl0_80224034 = 0;
}

extern void heap3_free(void** payload_ptr);

void func_ovl0_8020D784(void) {
    heap3_free(&D_ovl0_80227C90[0]);
    heap3_free(&D_ovl0_80227C90[1]);
    heap3_free(&D_ovl0_80227C98);
}

extern f32 atan2f(f32 y, f32 x);
extern f32 D_801B54F8;
extern f32 view_camera_eye_x;
extern f32 D_801B5500;
extern f32 view_camera_eye_z;

f32 func_ovl0_8020D7C4(void) {
    f32 dx = D_801B54F8 - view_camera_eye_x;
    f32 angle = 0.0f;
    f32 dz = D_801B5500 - view_camera_eye_z;

    if ((s32)dx != 0 || (s32)dz != 0) {
        angle = atan2f(dx, dz);
    }
    return angle;
}

typedef struct {
    u8 unk00[0xA7];
    u8 kind; /* 0xA7; 0 = unused slot */
} OverlaySlot; /* size 0xA8 */

extern OverlaySlot D_ovl0_8022B5D0[10];

s32 func_ovl0_8020D838(void) {
    s32 i;

    for (i = 0; i != 10; i++) {
        if ((D_ovl0_8022B5D0 + i)->kind == 2) {
            return 1;
        }
    }
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_8020D874);

void func_ovl0_8020D9F0(void) {
    s32 i;

    for (i = 0; i != 10; i++) {
        (D_ovl0_8022B5D0 + i)->kind = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_8020DA18);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_80202A00", func_ovl0_8020DCA4);
