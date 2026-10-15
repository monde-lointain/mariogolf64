#include "terrain.h"

/* Per-shot init scratch record, 0xB8 stride, base @ D_800B7868. */
typedef struct {
  /* 0x00 */ s32 unknown_00;
  /* 0x04 */ s32 unknown_04;
  /* 0x08 */ s32 unknown_08;
  /* 0x0C */ s32 unknown_0C;
  /* 0x10 */ s32 unknown_10;
  /* 0x14 */ s32 unknown_14;
  /* 0x18 */ s32 unknown_18;
  /* 0x1C */ u8 pad1C[0x58];
  /* 0x74 */ s32 unknown_74;
  /* 0x78 */ u8 pad78[0x40];
} ShotInitRecord;

extern TerrainAttrEntry D_800B790C[27];
extern ShotInitRecord D_800B7868[1];
extern u8 D_800B7C00[];
extern u8 D_800B7C08[];
extern u8 D_800B7C2E[];

extern s32 rand(void);
extern s32 resolve_shot_quality_table(s32, s32, s32);

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

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry",
            play_sound_at_meter_ratio_gameplay);

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry", update_rotation_matrix);

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry", play_sound_at_meter_ratio);

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry",
            play_sound_for_meter_phase);

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry",
            read_meter_stick_normalized);

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry", update_ball_physics);

u8* func_80037E50(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
  s32 quality;
  u8* p;

  quality = resolve_shot_quality_table(arg0, arg1, arg2) - 1;
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

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry",
            calc_stick_offset_with_noise);

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry", init_ball_for_shot);
