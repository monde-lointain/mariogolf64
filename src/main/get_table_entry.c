#include "terrain.h"
#include "club.h"

extern TerrainAttrEntry D_800B790C[27];
extern u8 D_800B7C00[];
extern u8 D_800B7C08[];
extern u8 D_800B7C2E[];

extern s32 rand(void);

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

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry", play_sound_at_meter_ratio);

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry", play_sound_for_meter_phase);

typedef struct {
  u8 pad00[0x18];
  s32 unk18;
  u8 pad1C[0x4];
  s32 unk20;
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

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry", calc_stick_offset_with_noise);

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry", init_ball_for_shot);
