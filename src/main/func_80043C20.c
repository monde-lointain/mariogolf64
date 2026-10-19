#include "club.h"
#include "terrain.h"
#include "session.h"

extern ClubShot D_800BB258[];
extern ClubShot club_param_fallback;
extern s8 D_800BE5E8[];
extern s32 active_player_idx;
extern s32 active_club_id;
extern s32 active_meter_category;
extern s32 active_terrain_index;

extern s32 flag_is_set(s32 flag);

/* Per-character adjustments and the fourteen per-club setup records. */

typedef struct {
  /* 0x00 */ u16 target_distance;
  /* 0x02 */ u16 launch_angle_bam;
  /* 0x04 */ u8 unknown_04;
  /* 0x05 */ u8 unknown_05;
} ClubSetupParameters; /* 0x06 */

typedef struct {
  /* 0x00 */ u8 unknown_00[0x1C];
  /* 0x1C */ u8 launch_angle_offset;
  /* 0x1D */ u8 unknown_1D;
  /* 0x1E */ u8 unknown_1E;
  /* 0x1F */ s8 unknown_1F;
  /* 0x20 */ u8 unknown_20[0x1C];
  /* 0x3C */ ClubSetupParameters clubs[14];
} ShotInput;

extern s32 active_player_idx_alt;  // 0x800FBE78 (active_club_id/idx/idx_alt are
                                   // adjacent @0x800FBE58/74/78)
extern u16 D_800FED18[];
extern u8 D_800BE5E0[];
extern char D_800CC510[];
extern char D_800CC520[];
extern s8 g_terrain_vtx_xform_mode;
extern s32 func_8021EA48(void);
extern void play_sound_effect(s32 sfx, s32 arg1, s32 arg2);
extern void* heap3_alloc(s32 size);
extern void heap3_free(void* ptr);
extern void validate_heap_string(void* buf, s32 len);
s32 predict_shot_distance_variant(s32 idx, s32 category);
extern char D_800CC370[], D_800CC374[], D_800CC388[], D_800CC3EC[],
    D_800CC40C[];
extern char D_800CC454[], D_800CC474[], D_800CC478[], D_800CC480[],
    D_800CC484[], D_800CC488[];

s32 resolve_club_terrain_mask(s32 mapping_selector, TerrainAttrEntry* terrain_entry);
void predict_shot_distance(s32 index);
void func_80043C64(void);

// Debug club-data dump. func_80043C20 is a GCC nested string-appender (static
// chain = &line_buffer, with buffer_cursor/buffer_end at chain+0x100/+0x104.
// GCC 2.7.2 emits it immediately before this parent, landing it at 0x80043C20.
void func_80043C64(void) {
  char line_buffer[0x100];
  u8* buffer_cursor;
  u8* buffer_end;
  u8* buffer_start;
  ClubShot* club_data;
  s32 club_index;
  s32 meter_category;
  s32 power_level;

  void func_80043C20(u8* text) {
    for (;;) {
      u8 character = *text++;
      if (character == 0) {
        break;
      }
      if (buffer_cursor < buffer_end) {
        *buffer_cursor++ = character;
      }
    }
  }

  buffer_start = heap3_alloc(0x8000);
  buffer_cursor = buffer_start;
  buffer_end = buffer_start + 0x8000;
  if (buffer_start == NULL) {
    __assert(D_800CC370, D_800CC374, 0x21F);
  }
  sprintf(line_buffer, D_800CC388);
  func_80043C20((u8*)line_buffer);
  for (club_index = 0; club_index < 0xFC; club_index++) {
    club_data = &D_800BB258[club_index];
    sprintf(line_buffer, D_800CC3EC, club_data->distances[0], club_data->distances[1],
            club_data->distances[2], club_data->unknown_28, club_data->unknown_2A,
            club_data->unknown_2C);
    func_80043C20((u8*)line_buffer);
  }
  sprintf(line_buffer, D_800CC40C);
  func_80043C20((u8*)line_buffer);
  func_80043C20((u8*)D_800CC454);
  for (meter_category = 0; meter_category < 3; meter_category++) {
    func_80043C20((u8*)D_800CC474);
    for (power_level = 1; power_level < 0x1F; power_level++) {
      sprintf(line_buffer, D_800CC478,
              predict_shot_distance_variant((power_level << 8) / 30, meter_category));
      func_80043C20((u8*)line_buffer);
    }
    func_80043C20((u8*)D_800CC480);
  }
  func_80043C20((u8*)D_800CC484);
  line_buffer[0] = 0x1A;
  line_buffer[1] = 0;
  func_80043C20((u8*)line_buffer);
  sprintf(line_buffer, D_800CC488);
  validate_heap_string(line_buffer, buffer_cursor - buffer_start);
  heap3_free(&buffer_start);
}


/* Integer lerp, truncated: a + (b - a) * t, evaluated in double. The dead 8-byte frame and `sw $v0,0(sp)` are the static-chain home of the nested-function original (child of predict_shot_distance, which sets $v0 = sp+0x18 before its jal); `unused` reproduces them, as in lerp_int. */
s32 lerp_int_v2(s32 a, s32 b, f32 t) {
    s32 r;
    volatile s32 unused = r;
    return a + (b - a) * (f64)t;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", predict_shot_distance);

void apply_shot_input_to_club_parameters(s32 setup_index, ShotInput* shot_input) {
  s32 club_setup_index;
  s32 club_id;
  ClubShot* club_data;
  u16 target_distance;
  s32 launch_angle_scaled;
  s16 launch_angle_degrees;
  s32 percent_100;
  u8 unknown_05_value;
  u8 unknown_1E_value;
  u8 unknown_1D_value;

  club_setup_index = 0;
  percent_100 = 100;
  club_id = setup_index * 0xE;
  for (; club_setup_index < 0xE; club_setup_index += 1) {
    club_data = get_club_param(club_id);
    target_distance = shot_input->clubs[club_setup_index].target_distance;
    club_data->target_distance = target_distance;
    if ((s16)target_distance >= 0x191) {
      club_data->target_distance = 0x190;
    }
    osSyncPrintf(D_800CC510, club_data->target_distance);
    launch_angle_scaled = shot_input->clubs[club_setup_index].launch_angle_bam * 0x168;
    if (launch_angle_scaled < 0) {
      launch_angle_scaled += 0xFFFF;
    }
    launch_angle_degrees =
        (s8)shot_input->launch_angle_offset + (launch_angle_scaled >> 0x10);
    club_data->launch_angle_degrees = -launch_angle_degrees;
    osSyncPrintf(D_800CC520, (s16)-launch_angle_degrees);
    if (club_data->launch_angle_degrees >= -6) {
      club_data->launch_angle_degrees = -7;
    }
    if (club_data->launch_angle_degrees < -0x2D) {
      club_data->launch_angle_degrees = -0x2D;
    }
    unknown_05_value = shot_input->clubs[club_setup_index].unknown_05;
    club_data->unknown_26 = (s8)unknown_05_value;
    if (club_data->unknown_26 >= 0x65) {
      club_data->unknown_26 = percent_100;
    }
    if (club_data->unknown_26 < -0x64) {
      club_data->unknown_26 = -0x64;
    }
    unknown_1E_value = shot_input->unknown_1E;
    if ((u32)((unknown_1E_value + 4) & 0xFF) >= 9U) {
      club_data->quality_scale_percent = percent_100;
    } else {
      club_data->quality_scale_percent = D_800BE5E0[(s8)unknown_1E_value];
    }
    club_data->unknown_00 = 0x6E - (shot_input->unknown_1F * 9);
    unknown_1D_value = shot_input->unknown_1D;
    club_data->unknown_30 = unknown_1D_value;
    if ((s8)unknown_1D_value < -4) {
      club_data->unknown_30 = -4;
    }
    if (club_data->unknown_30 >= 5) {
      club_data->unknown_30 = 4;
    }
    if (club_data->unknown_00 >= 0x92) {
      club_data->unknown_00 = 0x91;
    }
    if (club_data->unknown_00 < 0x46) {
      club_data->unknown_00 = 0x46;
    }
    g_terrain_vtx_xform_mode = 0;
    predict_shot_distance(club_id);
    club_id += 1;
  }
}

// func_80044470 is a GCC nested function of func_800444B8 (static chain in
// $v0); GCC 2.7.2 emits it immediately before the parent. Defined inside
// func_800444B8 below.

void func_800444B8(void) {
  s32 club_data_index;
  s8 func_80044470(s32 club_id) {
    s32 club_group = club_id / 14;
    if ((u32)club_group >= 0x12) {
      club_group = 0;
    }
    return D_800BE5E8[club_group];
  }
  if (!flag_is_set(0x1F)) {
    for (club_data_index = 0; club_data_index < 0xC4; club_data_index++) {
      ClubShot* club_data = &D_800BB258[club_data_index];
      ClubDistancePreset* distance_preset = func_80043AF0(club_data_index);
      club_data->distances[0] = distance_preset->distances[0];
      club_data->distances[1] = distance_preset->distances[1];
      club_data->distances[2] = distance_preset->distances[2];
      club_data->unknown_28 = distance_preset->unknown_0C[0];
      club_data->unknown_2A = distance_preset->unknown_0C[1];
      club_data->unknown_2C = distance_preset->unknown_0C[2];
      club_data->unknown_30 = func_80044470(club_data_index);
    }
    return;
  }
  if (flag_is_set(0x1F)) {
    for (club_data_index = 0; club_data_index < 0xFC; club_data_index++) {
      predict_shot_distance(club_data_index);
    }
    func_80043C64();
  }
}

/* Integer lerp, truncated: a + (b - a) * t, evaluated in double. The dead 8-byte frame and `sw $v0,0(sp)` are the static-chain home of the nested-function original (child of predict_shot_distance_variant, which sets $v0 = sp+0x10 before its jal); `unused` reproduces them, as in lerp_s32 (func_8008D100.c). */
s32 lerp_int(s32 a, s32 b, f32 t) {
    s32 r;
    volatile s32 unused = r;
    return a + (b - a) * (f64)t;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", predict_shot_distance_variant);

s32 get_club_distance_slot(ClubShot* club_data, s32 distance_index) {
  return club_data->distances[distance_index];
}

ClubShot* get_club_param(u32 club_id) {
  ClubShot* result;
  if (club_id < 0xFC) {
    result = &D_800BB258[club_id];
  } else {
    result = &club_param_fallback;
  }
  return result;
}

ClubShot* get_shot_data(void) {
  return &D_800BB258[player_records[active_player_idx].club_ids[active_club_id]];
}

s32 get_shot_param(void) {
  return get_club_meter_extent(get_shot_data(), active_meter_category);
}

s32 get_shot_progress(void) {
  return get_club_meter_units(get_shot_data(), active_meter_category);
}

// The switch funnels through `r` + a single trailing `return r` (NOT per-case
// returns): this gives its cases a distinct switch-exit label from the early
// `if (g_terrain) return 1`, so GCC 2.7.2's jump.c cross-jump (find_cross_jump,
// rtx_renumbered_equal_p on the jump target) does not tail-merge case 11's
// `return 1` with the g_terrain `return 1`. Merging them would give g_terrain's
// return block 2 predecessors, blocking reorg from folding it to the ROM's
// tight `bnez end`+delay. The three early `if` returns stay direct (they must
// jump to the true epilogue for that fold.
s32 resolve_club_terrain_mask(s32 mapping_selector, TerrainAttrEntry* terrain_entry) {
  s32 terrain_id = terrain_entry->id;
  s32 result_mask;

  if (flag_is_set(0x98)) {
    return -1;
  }
  if (g_terrain_vtx_xform_mode != 0) {
    return 1;
  }
  if (mapping_selector == 0) {
    return (terrain_id == 0xB) ? 1 : -1;
  }
  switch (terrain_id) {
    case 3:
    case 16:
    case 18:
    case 19:
    case 23:
      result_mask = 2;
      break;
    case 11:
      result_mask = 1;
      break;
    case 4:
    case 6:
    case 7:
    case 8:
      result_mask = 4;
      break;
    case 15:
      result_mask = 8;
      break;
    case 0:
    case 1:
    case 2:
      result_mask = 0x10;
      break;
    case 17:
      result_mask = 0x20;
      break;
    default:
      result_mask = -1;
      break;
  }
  return result_mask;
}

s32 resolve_active_terrain_club_mask(s32 mapping_selector) {
  return resolve_club_terrain_mask(mapping_selector, get_table_entry(active_terrain_index));
}

s32 resolve_shot_quality_table(ClubShot* club_data, s32 terrain_index,
                               s32 quality_type) {
  /* Keep the 16-byte tables independent: merging rows changes address folding. */
  static const char q00[16] = "3444444555555";
  static const char q01[16] = "5555555555555";
  static const char q02[16] = "3334444445555";
  static const char q03[16] = "4444555555555";
  static const char q04[16] = "2333344444455";
  static const char q05[16] = "3344445555555";
  static const char q06[16] = "1222333344445";
  static const char q07[16] = "2333334444455";
  static const char q08[16] = "1223333444455";
  static const char q09[16] = "2233344445555";
  static const char q10[16] = "1112223334445";
  static const char q11[16] = "2222333444455";
  static const char q12[16] = "1111222233334";
  static const char q13[16] = "1112222333344";
  static const char q14[16] = "1112223333445";
  static const char q15[16] = "2223333444455";
  static const char q16[16] = "1111122222334";
  static const char q17[16] = "1112223333444";
  static const char q18[16] = "1111111222223";
  static const char q19[16] = "1111112223334";
  static const char q20[16] = "1112223333344";
  static const char q21[16] = "2223333333444";
  static const char q22[16] = "3333333333333";
  const char* quality_row;
  const char* alternate_quality_row;
  s32 terrain_code = *(s32*)get_table_entry(terrain_index);
  s32 club_index;
  s32 quality_value;

  switch (terrain_code) {
    case 0x16:
      quality_row = q00;
      alternate_quality_row = q01;
      break;
    case 0:
    case 0xC:
    case 0xF:
      quality_row = q02;
      alternate_quality_row = q03;
      break;
    case 1:
    case 0x11:
      quality_row = q04;
      alternate_quality_row = q05;
      break;
    case 2:
      quality_row = q06;
      alternate_quality_row = q07;
      break;
    case 3:
    case 0x10:
    case 0x12:
      quality_row = q08;
      alternate_quality_row = q09;
      break;
    case 4:
      quality_row = q10;
      alternate_quality_row = q11;
      break;
    case 0x13:
    case 0x17:
      quality_row = q12;
      alternate_quality_row = q13;
      break;
    case 6:
      quality_row = q14;
      alternate_quality_row = q15;
      break;
    case 7:
      quality_row = q16;
      alternate_quality_row = q17;
      break;
    case 8:
      quality_row = q18;
      alternate_quality_row = q19;
      break;
    case 0xE:
      quality_row = q20;
      alternate_quality_row = q21;
      break;
    default:
      quality_row = q22;
      alternate_quality_row = quality_row;
      break;
  }

  club_index = club_data->club_index;
  if (club_index != 0xD) {
    const char* quality_digit =
        (quality_type == 2) ? &alternate_quality_row[club_index] : &quality_row[club_index];
    s32 quality_component = (u8)*quality_digit;
    quality_value = quality_component - 0x30;
    /* Reuse the component variable for power so digit decoding precedes the power load. */
    quality_component = club_data->quality_scale_percent;
    quality_value = (quality_value * quality_component + 0x32) / 100;
    if (quality_value < 1) {
      quality_value = 1;
    }
    if (quality_value < 6) {
      return quality_value;
    }
    quality_value = 5;
  } else {
    quality_value = 0;
  }
  return quality_value;
}

void func_80044C7C(ClubShot* club_data, s32 terrain_index, s32* quality_bounds, s32 quality_type) {
  s32 quality_level = resolve_shot_quality_table(club_data, terrain_index, quality_type);
  if (quality_level > 0) {
    quality_bounds[0] = quality_level * 2 - 1;
    quality_bounds[1] = quality_level - 1;
  } else {
    quality_bounds[0] = 0;
    quality_bounds[1] = 0;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", func_80044CCC);

void func_80044FDC(void) {
  s32 club_index;
  u16 player_input_flags;

  if (func_8021EA48() != 0) {
    s32* active_player_index_ptr =
        &active_player_idx_alt;  // active_player_index_ptr[-8] is active_club_id.
    player_input_flags = D_800FED18[*active_player_index_ptr * 4];
    if (player_input_flags & 0x400) {
      s32* player_index_ptr = active_player_index_ptr - 1;
      for (club_index = active_club_id + 1; club_index < 0x10; club_index++) {
        if (player_records[player_index_ptr[0]].club_ids[club_index] != -1) {
          if (D_800BB258[player_records[player_index_ptr[0]].club_ids[club_index]].flags &
              resolve_active_terrain_club_mask(0)) {
            player_index_ptr[-7] = club_index;
            play_sound_effect(0x5B, 0xF, 0x7F);
            return;
          }
        }
      }
    } else if (player_input_flags & 0x800) {
      s32* player_index_ptr = active_player_index_ptr - 1;
      for (club_index = active_club_id - 1; club_index >= 0; club_index--) {
        if (player_records[player_index_ptr[0]].club_ids[club_index] != -1) {
          if (D_800BB258[player_records[player_index_ptr[0]].club_ids[club_index]].flags &
              resolve_active_terrain_club_mask(0)) {
            player_index_ptr[-7] = club_index;
            play_sound_effect(0x5B, 0xF, 0x7F);
            return;
          }
        }
      }
    }
  }
}

extern s32 D_800FBE64;
extern s32 target_z;
extern s8 D_800FBE9C;
extern s32 func_8003DFD0(s32 x, s32 z);
void func_800451E4(void) {
  ClubShot* club_data = get_shot_data();
  s32 target_distance =
      (s32)((f32)func_8003DFD0(D_800FBE64 - hole_environment.pin_position.x,
                               target_z - hole_environment.pin_position.z) *
            0.00007119878137f);
  if ((D_800FED18[active_player_idx_alt * 4] & 0x4000) && func_8021EA48()) {
    s32 distance_exceeds_threshold;
    if (club_data->flags & 1) {
      if (active_meter_category == 1) {
        active_meter_category = 0;
      } else if (active_meter_category == 0) {
        active_meter_category = 2;
      } else {
        active_meter_category = 1;
      }
      play_sound_effect(0x5B, 15, 127);
      /* Materialize the threshold before the comparison: a direct > test emits
       * slti/xori instead of the retail li/slt pair. */
    } else if ((distance_exceeds_threshold = 60,
                distance_exceeds_threshold = distance_exceeds_threshold < target_distance)) {
      if (active_meter_category == 1) {
        if (D_800FBE9C == 0) {
          play_sound_effect(0x5E, 15, 127);
        } else {
          active_meter_category = 0;
          play_sound_effect(0x5B, 15, 127);
        }
      } else {
        active_meter_category = 1;
        play_sound_effect(0x5B, 15, 127);
      }
    } else {
      play_sound_effect(0x5B, 15, 127);
      if (active_meter_category == 1) {
        active_meter_category = 2;
      } else {
        active_meter_category = 1;
      }
    }
  }
  if (!(club_data->flags & 1)) {
    s32* meter_category_ptr = &active_meter_category;
    if ((*meter_category_ptr == 0) & (target_distance <= 60)) {
      *meter_category_ptr = 1;
    }
    if ((*meter_category_ptr == 2) & (target_distance > 60)) {
      *meter_category_ptr = 1;
    }
    if (*meter_category_ptr == 0 && D_800FBE9C == 0) {
      *meter_category_ptr = 1;
    }
  }
}
