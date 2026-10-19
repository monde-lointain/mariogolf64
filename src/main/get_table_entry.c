#include "terrain.h"
#include "club.h"
#include "session.h"

extern TerrainAttrEntry D_800B790C[27];
extern u8 D_800B7C00[];
extern u8 D_800B7C08[];
extern u8 D_800B7C2E[];

extern s32 rand(void);
extern f64 D_800CA640;
extern f64 D_800CA648;
extern void play_sound_effect(s32 sfx, s32 channel, s32 volume);
extern s32 D_800C0E60;

TerrainAttrEntry* get_table_entry(u32 idx) {
  u32 i = (idx < 0x1BU) ? idx : 0;
  return &D_800B790C[i];
}

void func_80032E88(void) {
  s32 i;

  for (i = 0; i <= 0; i++) {
    D_800B7868[i].unknown_00 = 0x800;
    D_800B7868[i].unknown_04 = -1000;
    rand();
    D_800B7868[i].unknown_08 = 0x1000;
    D_800B7868[i].unknown_0C = rand() & 0x3FF;
    D_800B7868[i].unknown_10 = -100;
    D_800B7868[i].unknown_14 = rand() & 0x3FF;
    D_800B7868[i].unknown_18 = 0;
    D_800B7868[i].unknown_74 = 1;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry", play_sound_at_meter_ratio_gameplay);

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry", update_rotation_matrix);

/* Static-chain home of the nested-function original: $v0 on entry is the parent frame and is never read. The volatile local keeps its `sw $v0` spill in the frame. */
void play_sound_at_meter_ratio(s32 sfx, f32 arg1, f32 arg2) {
  s32 r;
  volatile s32 unused = r;
  s32 volume;

  volume = (s32) ((arg2 * 127.0f) / arg1);
  if (volume >= 0xA) {
    if (volume >= 0x80) {
      volume = 0x7F;
    }
    play_sound_effect(sfx, D_800C0E60, volume);
  }
}

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry", play_sound_for_meter_phase);

typedef struct {
  u8 pad00[0x18];
  s32 unk18;
  u8 pad1C[0x4];
  s32 unk20;
  u8 pad24[0x81];
  s8 unkA5;
} BallObject;

typedef struct {
  u8 pad00[0x40];
  BallObject **argSlot;
} MeterStickFrame;

/* Static-chain home of the nested-function original: $v0 on entry is the parent frame; its 0x40 slot holds &ball slot, and that slot holds the ball object pointer. The volatile frame access keeps `sw $v0,0(sp)` ahead of the chain loads. */
f32 read_meter_stick_normalized(void) {
  s32 r;
  volatile s32 unused = r;
  BallObject *obj;
  s32 sum;

  obj = *((volatile MeterStickFrame *) r)->argSlot;
  sum = (obj->unk18 + obj->unk20) & 0xFF;
  return ((f32) sum - 127.5f) * 0.007843138f;
}

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry", update_ball_physics);

u8* func_80037E50(ClubShot* club, s32 arg1, s32 arg2, s32 arg3) {
  s32 quality;
  u8* p;

  quality = resolve_shot_quality_table(club, arg1, arg2) - 1;
  quality = quality & ((s32)~quality >> 0x1F);
  if (quality >= 6) {
    quality = 5;
  }
  if (arg3 < -5) {
    return D_800B7C00;
  }
  if (arg3 < 6) {
    p = D_800B7C2E + quality * 0x42;
    return p + arg3 * 6;
  }
  return D_800B7C08;
}

/* Static-chain home of the nested-function original: $v0 on entry is the parent frame; its 0x8 slot holds &ball slot, and that slot holds the ball object pointer. The volatile frame access keeps `sw $v0,0(sp)` ahead of the chain loads. */
typedef struct {
  u8 pad00[0x8];
  BallObject **argSlot;
} StickNestFrame;

f32 calc_stick_offset_with_noise(void) {
  s32 r;
  volatile s32 unused = r;
  BallObject *obj;
  s32 temp_a0;
  s32 temp_a1;
  u32 temp_v1;
  s32 temp_v0_2;
  s32 temp_v0_3;
  f64 var_ft0;
  f64 var_fv1;
  f32 firstResult;

  obj = *((volatile StickNestFrame *) r)->argSlot;
  temp_a1 = obj->unk18;
  temp_a0 = obj->unk20;
  temp_v1 = player_records[obj->unkA5].unknown_88;
  temp_v0_2 = (temp_a1 + temp_a0 + temp_v1) & 0xFF;
  var_ft0 = (f64) temp_v0_2;
  if (temp_v0_2 < 0) {
    var_ft0 += D_800CA640;
  }
  firstResult = (f32) var_ft0 * 0.0019607844f;
  temp_v0_3 = ((temp_a1 - temp_a0) + temp_v1) & 0xFF;
  var_fv1 = (f64) temp_v0_3;
  if (temp_v0_3 < 0) {
    var_fv1 += D_800CA648;
  }
  return firstResult + ((f32) var_fv1 * 0.0019607844f);
}

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry", init_ball_for_shot);
