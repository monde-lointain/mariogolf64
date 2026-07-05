#include "common.h"

typedef struct {
  u8 pad00[0x18];
  s32 dist[3];  // 0x18, 0x1C, 0x20
  u8 pad24[4];  // 0x24
  s16 unk28;    // 0x28
  s16 unk2A;    // 0x2A
  s16 unk2C;    // 0x2C
  u8 pad2E[2];  // 0x2E
  s8 unk30;     // 0x30
  u8 pad31[3];  // 0x31
} ClubShot;     // 0x34

typedef struct {
  s32 w[6];  // 0x00..0x14
} ClubSrc;   // 0x18

extern ClubShot D_800BB258[];
extern ClubShot club_param_fallback;
extern s8 D_800BE5E8[];
extern s16 D_801B7222[][92];
extern s32 active_player_idx;
extern s32 active_club_id;
extern s32 active_meter_category;
extern void* D_800FBE70;

extern s32 flag_is_set(s32 flag);
extern s32 get_club_meter_extent(void* club, s32 category);
extern s32 get_club_meter_units(void* club, s32 category);
extern void* get_table_entry(void* table);
extern u8* func_80043AF0(s32 index);

s32 resolve_club_terrain_mask(s32 arg0, void* arg1);
s32 resolve_shot_quality_table(s32 arg0, s32 arg1, s32 arg2);
void predict_shot_distance(s32 index);
void func_80043C64(void);
ClubShot* get_shot_data(void);

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", func_80043C20);

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", func_80043C64);

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", lerp_int_v2);

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", predict_shot_distance);

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", func_80044254);

// func_80044470 is a GCC nested function of func_800444B8 (static chain in $v0);
// GCC 2.7.2 emits it immediately before the parent. Defined inside func_800444B8 below.

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
      ClubSrc* src = (ClubSrc*)func_80043AF0(i);
      dst->dist[0] = src->w[0];
      dst->dist[1] = src->w[1];
      dst->dist[2] = src->w[2];
      dst->unk28 = src->w[3];
      dst->unk2A = src->w[4];
      dst->unk2C = src->w[5];
      dst->unk30 = func_80044470(i);
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

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", predict_shot_distance_variant);

s32 get_club_distance_slot(ClubShot* c, s32 i) { return c->dist[i]; }

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

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", resolve_club_terrain_mask);

s32 func_80044A8C(s32 arg0) {
  return resolve_club_terrain_mask(arg0, get_table_entry(D_800FBE70));
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

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", func_80044FDC);

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", func_800451E4);
