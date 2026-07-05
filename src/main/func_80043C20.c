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

extern ClubShot D_800BB258[];
extern ClubShot club_param_fallback;
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
ClubShot* get_shot_data(void);

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", func_80043C20);

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", func_80043C64);

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", lerp_int_v2);

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", predict_shot_distance);

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", func_80044254);

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", func_80044470);

INCLUDE_ASM("asm/nonmatchings/main/func_80043C20", func_800444B8);

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
