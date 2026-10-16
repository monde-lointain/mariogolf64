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
extern void func_80028110(void* buf, s32 len);
s32 predict_shot_distance_variant(s32 idx, s32 category);
extern char D_800CC370[], D_800CC374[], D_800CC388[], D_800CC3EC[],
    D_800CC40C[];
extern char D_800CC454[], D_800CC474[], D_800CC478[], D_800CC480[],
    D_800CC484[], D_800CC488[];

s32 resolve_club_terrain_mask(s32 arg0, TerrainAttrEntry* arg1);
void predict_shot_distance(s32 index);
void func_80043C64(void);

// Debug club-data dump. func_80043C20 is a GCC nested string-appender (static
// chain = &line, with buf_pos/buf_end reached at chain+0x100/+0x104); GCC 2.7.2
// emits it immediately before this parent, landing it at the pack-lead vram
// 0x80043C20.
void func_80043C64(void) {
  char line[0x100];
  u8* buf_pos;
  u8* buf_end;
  u8* buf_start;
  ClubShot* cs;
  s32 club;
  s32 category;
  s32 power;

  void func_80043C20(u8 * s) {
    for (;;) {
      u8 c = *s++;
      if (c == 0) {
        break;
      }
      if (buf_pos < buf_end) {
        *buf_pos++ = c;
      }
    }
  }

  buf_start = heap3_alloc(0x8000);
  buf_pos = buf_start;
  buf_end = buf_start + 0x8000;
  if (buf_start == NULL) {
    __assert(D_800CC370, D_800CC374, 0x21F);
  }
  sprintf(line, D_800CC388);
  func_80043C20((u8*)line);
  for (club = 0; club < 0xFC; club++) {
    cs = &D_800BB258[club];
    sprintf(line, D_800CC3EC, cs->distances[0], cs->distances[1], cs->distances[2], cs->unknown_28,
            cs->unknown_2A, cs->unknown_2C);
    func_80043C20((u8*)line);
  }
  sprintf(line, D_800CC40C);
  func_80043C20((u8*)line);
  func_80043C20((u8*)D_800CC454);
  for (category = 0; category < 3; category++) {
    func_80043C20((u8*)D_800CC474);
    for (power = 1; power < 0x1F; power++) {
      sprintf(line, D_800CC478,
              predict_shot_distance_variant((power << 8) / 30, category));
      func_80043C20((u8*)line);
    }
    func_80043C20((u8*)D_800CC480);
  }
  func_80043C20((u8*)D_800CC484);
  line[0] = 0x1A;
  line[1] = 0;
  func_80043C20((u8*)line);
  sprintf(line, D_800CC488);
  func_80028110(line, buf_pos - buf_start);
  heap3_free(&buf_start);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", lerp_int_v2);

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", predict_shot_distance);

void apply_shot_input_to_club_parameters(s32 club_set_index, ShotInput* shot_input) {
  s32 club_index;
  s32 club_id;
  ClubShot* club_data;
  u16 target_distance;
  s32 launch_angle_scaled;
  s16 launch_angle_degrees;
  s32 percent_100;
  u8 unknown_05_value;
  u8 unknown_1E_value;
  u8 unknown_1D_value;

  club_index = 0;
  percent_100 = 100;
  club_id = club_set_index * 0xE;
  for (; club_index < 0xE; club_index += 1) {
    club_data = get_club_param(club_id);
    target_distance = shot_input->clubs[club_index].target_distance;
    club_data->target_distance = target_distance;
    if ((s16)target_distance >= 0x191) {
      club_data->target_distance = 0x190;
    }
    osSyncPrintf(D_800CC510, club_data->target_distance);
    launch_angle_scaled = shot_input->clubs[club_index].launch_angle_bam * 0x168;
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
    unknown_05_value = shot_input->clubs[club_index].unknown_05;
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
  s32 i;
  s8 func_80044470(s32 x) {
    s32 q = x / 14;
    if ((u32)q >= 0x12) {
      q = 0;
    }
    return D_800BE5E8[q];
  }
  if (!flag_is_set(0x1F)) {
    for (i = 0; i < 0xC4; i++) {
      ClubShot* dst = &D_800BB258[i];
      ClubDistancePreset* src = func_80043AF0(i);
      dst->distances[0] = src->distances[0];
      dst->distances[1] = src->distances[1];
      dst->distances[2] = src->distances[2];
      dst->unknown_28 = src->unknown_0C[0];
      dst->unknown_2A = src->unknown_0C[1];
      dst->unknown_2C = src->unknown_0C[2];
      dst->unknown_30 = func_80044470(i);
    }
    return;
  }
  if (flag_is_set(0x1F)) {
    for (i = 0; i < 0xFC; i++) {
      predict_shot_distance(i);
    }
    func_80043C64();
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", lerp_int);

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20",
            predict_shot_distance_variant);

s32 get_club_distance_slot(ClubShot* club, s32 distance_index) {
  return club->distances[distance_index];
}

ClubShot* get_club_param(u32 id) {
  ClubShot* result;
  if (id < 0xFC) {
    result = &D_800BB258[id];
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
// jump to the true epilogue for that fold). See
// docs/hazards.md#cross-jump-tail-merge (S185 compiler-source fan-out).
s32 resolve_club_terrain_mask(s32 arg0, TerrainAttrEntry* arg1) {
  s32 terrain = arg1->id;
  s32 r;

  if (flag_is_set(0x98)) {
    return -1;
  }
  if (g_terrain_vtx_xform_mode != 0) {
    return 1;
  }
  if (arg0 == 0) {
    return (terrain == 0xB) ? 1 : -1;
  }
  switch (terrain) {
    case 3:
    case 16:
    case 18:
    case 19:
    case 23:
      r = 2;
      break;
    case 11:
      r = 1;
      break;
    case 4:
    case 6:
    case 7:
    case 8:
      r = 4;
      break;
    case 15:
      r = 8;
      break;
    case 0:
    case 1:
    case 2:
      r = 0x10;
      break;
    case 17:
      r = 0x20;
      break;
    default:
      r = -1;
      break;
  }
  return r;
}

s32 resolve_active_terrain_club_mask(s32 arg0) {
  return resolve_club_terrain_mask(arg0, get_table_entry(active_terrain_index));
}

s32 resolve_shot_quality_table(ClubShot* club, s32 arg1, s32 arg2) {
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
  const char* p1;
  const char* p2;
  s32 idx = *(s32*)get_table_entry(arg1);
  s32 club_index;
  s32 v;

  switch (idx) {
    case 0x16:
      p1 = q00;
      p2 = q01;
      break;
    case 0:
    case 0xC:
    case 0xF:
      p1 = q02;
      p2 = q03;
      break;
    case 1:
    case 0x11:
      p1 = q04;
      p2 = q05;
      break;
    case 2:
      p1 = q06;
      p2 = q07;
      break;
    case 3:
    case 0x10:
    case 0x12:
      p1 = q08;
      p2 = q09;
      break;
    case 4:
      p1 = q10;
      p2 = q11;
      break;
    case 0x13:
    case 0x17:
      p1 = q12;
      p2 = q13;
      break;
    case 6:
      p1 = q14;
      p2 = q15;
      break;
    case 7:
      p1 = q16;
      p2 = q17;
      break;
    case 8:
      p1 = q18;
      p2 = q19;
      break;
    case 0xE:
      p1 = q20;
      p2 = q21;
      break;
    default:
      p1 = q22;
      p2 = p1;
      break;
  }

  club_index = club->club_index;
  if (club_index != 0xD) {
    const char* addr = (arg2 == 2) ? &p2[club_index] : &p1[club_index];
    s32 raw = (u8)*addr;
    v = raw - 0x30;
    /* Reuse raw for power so digit decoding precedes the power load. */
    raw = club->quality_scale_percent;
    v = (v * raw + 0x32) / 100;
    if (v < 1) {
      v = 1;
    }
    if (v < 6) {
      return v;
    }
    v = 5;
  } else {
    v = 0;
  }
  return v;
}

void func_80044C7C(ClubShot* club, s32 arg1, s32* out, s32 arg3) {
  s32 n = resolve_shot_quality_table(club, arg1, arg3);
  if (n > 0) {
    out[0] = n * 2 - 1;
    out[1] = n - 1;
  } else {
    out[0] = 0;
    out[1] = 0;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", func_80044CCC);

void func_80044FDC(void) {
  s32 club;
  u16 flags;

  if (func_8021EA48() != 0) {
    s32* alt =
        &active_player_idx_alt;  // 0x800FBE78; base=alt-1=&active_player_idx,
                                 // base[-7]=active_club_id
    flags = D_800FED18[*alt * 4];
    if (flags & 0x400) {
      s32* base = alt - 1;
      for (club = active_club_id + 1; club < 0x10; club++) {
        if (player_records[base[0]].club_ids[club] != -1) {
          if (D_800BB258[player_records[base[0]].club_ids[club]].flags & resolve_active_terrain_club_mask(0)) {
            base[-7] = club;
            play_sound_effect(0x5B, 0xF, 0x7F);
            return;
          }
        }
      }
    } else if (flags & 0x800) {
      s32* base = alt - 1;
      for (club = active_club_id - 1; club >= 0; club--) {
        if (player_records[base[0]].club_ids[club] != -1) {
          if (D_800BB258[player_records[base[0]].club_ids[club]].flags & resolve_active_terrain_club_mask(0)) {
            base[-7] = club;
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
  ClubShot* shot = get_shot_data();
  s32 distance =
      (s32)((f32)func_8003DFD0(D_800FBE64 - hole_environment.pin_position.x,
                               target_z - hole_environment.pin_position.z) *
            0.00007119878137f);
  if ((D_800FED18[active_player_idx_alt * 4] & 0x4000) && func_8021EA48()) {
    s32 far;
    if (shot->flags & 1) {
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
    } else if ((far = 60, far = far < distance)) {
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
  if (!(shot->flags & 1)) {
    s32* category = &active_meter_category;
    if ((*category == 0) & (distance <= 60)) {
      *category = 1;
    }
    if ((*category == 2) & (distance > 60)) {
      *category = 1;
    }
    if (*category == 0 && D_800FBE9C == 0) {
      *category = 1;
    }
  }
}
