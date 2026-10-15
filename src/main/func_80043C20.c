#include "club.h"
#include "terrain.h"

extern ClubShot D_800BB258[];
extern ClubShot club_param_fallback;
extern s8 D_800BE5E8[];
extern s16 D_801B7222[][92];
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
s32 resolve_shot_quality_table(s32 arg0, s32 arg1, s32 arg2);
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
  club = 0;
  do {
    cs = &D_800BB258[club];
    sprintf(line, D_800CC3EC, cs->distances[0], cs->distances[1], cs->distances[2], cs->unknown_28,
            cs->unknown_2A, cs->unknown_2C);
    club++;
    func_80043C20((u8*)line);
  } while (club < 0xFC);
  sprintf(line, D_800CC40C);
  func_80043C20((u8*)line);
  func_80043C20((u8*)D_800CC454);
  category = 0;
  do {
    func_80043C20((u8*)D_800CC474);
    power = 1;
    do {
      sprintf(line, D_800CC478,
              predict_shot_distance_variant((power << 8) / 30, category));
      func_80043C20((u8*)line);
      power++;
    } while (power < 0x1F);
    func_80043C20((u8*)D_800CC480);
    category++;
  } while (category < 3);
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

void func_80044254(s32 arg0, ShotInput* arg1) {
  s32 i;
  s32 club;
  ClubShot* cs;
  u16 temp;
  s32 var_v1;
  s16 temp_v0_2;
  s32 hundred;
  u8 temp_v0_3;
  u8 temp_v1_2;
  u8 temp_v0_4;

  i = 0;
  hundred = 100;
  club = arg0 * 0xE;
  do {
    cs = get_club_param(club);
    temp = arg1->clubs[i].target_distance;
    cs->target_distance = temp;
    if ((s16)temp >= 0x191) {
      cs->target_distance = 0x190;
    }
    osSyncPrintf(D_800CC510, cs->target_distance);
    var_v1 = arg1->clubs[i].launch_angle_bam * 0x168;
    if (var_v1 < 0) {
      var_v1 += 0xFFFF;
    }
    temp_v0_2 = (s8)arg1->launch_angle_offset + (var_v1 >> 0x10);
    cs->launch_angle_degrees = -temp_v0_2;
    osSyncPrintf(D_800CC520, (s16)-temp_v0_2);
    if (cs->launch_angle_degrees >= -6) {
      cs->launch_angle_degrees = -7;
    }
    if (cs->launch_angle_degrees < -0x2D) {
      cs->launch_angle_degrees = -0x2D;
    }
    temp_v0_3 = arg1->clubs[i].unknown_05;
    cs->unknown_26 = (s8)temp_v0_3;
    if (cs->unknown_26 >= 0x65) {
      cs->unknown_26 = hundred;
    }
    if (cs->unknown_26 < -0x64) {
      cs->unknown_26 = -0x64;
    }
    temp_v1_2 = arg1->unknown_1E;
    if ((u32)((temp_v1_2 + 4) & 0xFF) >= 9U) {
      cs->unknown_14 = hundred;
    } else {
      cs->unknown_14 = D_800BE5E0[(s8)temp_v1_2];
    }
    cs->unknown_00 = 0x6E - (arg1->unknown_1F * 9);
    temp_v0_4 = arg1->unknown_1D;
    cs->unknown_30 = temp_v0_4;
    if ((s8)temp_v0_4 < -4) {
      cs->unknown_30 = -4;
    }
    if (cs->unknown_30 >= 5) {
      cs->unknown_30 = 4;
    }
    if (cs->unknown_00 >= 0x92) {
      cs->unknown_00 = 0x91;
    }
    if (cs->unknown_00 < 0x46) {
      cs->unknown_00 = 0x46;
    }
    g_terrain_vtx_xform_mode = 0;
    predict_shot_distance(club);
    club += 1;
    i += 1;
  } while (i < 0xE);
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

s32 get_club_distance_slot(ClubShot* c, s32 i) { return c->distances[i]; }

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
  return &D_800BB258[D_801B7222[active_player_idx][active_club_id]];
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

s32 func_80044A8C(s32 arg0) {
  return resolve_club_terrain_mask(arg0, get_table_entry(active_terrain_index));
}

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", resolve_shot_quality_table);

void func_80044C7C(s32 arg0, s32 arg1, s32* out, s32 arg3) {
  s32 n = resolve_shot_quality_table(arg0, arg1, arg3);
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
        if (D_801B7222[base[0]][club] != -1) {
          if (D_800BB258[D_801B7222[base[0]][club]].flags & func_80044A8C(0)) {
            base[-7] = club;
            play_sound_effect(0x5B, 0xF, 0x7F);
            return;
          }
        }
      }
    } else if (flags & 0x800) {
      s32* base = alt - 1;
      for (club = active_club_id - 1; club >= 0; club--) {
        if (D_801B7222[base[0]][club] != -1) {
          if (D_800BB258[D_801B7222[base[0]][club]].flags & func_80044A8C(0)) {
            base[-7] = club;
            play_sound_effect(0x5B, 0xF, 0x7F);
            return;
          }
        }
      }
    }
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", func_800451E4);
