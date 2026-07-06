#include "common.h"

/* Terrain attribute lookup table: 27 entries, 0x1C stride (RE'd:
 * TerrainAttrEntry). */
typedef struct {
  /* 0x00 */ u32 id;
  /* 0x04 */ char* name_ptr;
  /* 0x08 */ u8 traction_drag;
  /* 0x09 */ u8 traction_grip;
  /* 0x0A */ u8 slope_roughness;
  /* 0x0B */ u8 pad_0b;
  /* 0x0C */ s16 bounce_a;
  /* 0x0E */ s16 bounce_b;
  /* 0x10 */ u32 param;
  /* 0x14 */ u8 flag_a;
  /* 0x15 */ u8 flag_b;
  /* 0x16 */ u8 flag_c;
  /* 0x17 */ u8 penalty_class;
  /* 0x18 */ u32 sound_id;
} TerrainAttrEntry;

/* Per-shot init scratch record, 0xB8 stride, base @ D_800B7868. */
typedef struct {
  /* 0x00 */ s32 unk00;
  /* 0x04 */ s32 unk04;
  /* 0x08 */ s32 unk08;
  /* 0x0C */ s32 unk0C;
  /* 0x10 */ s32 unk10;
  /* 0x14 */ s32 unk14;
  /* 0x18 */ s32 unk18;
  /* 0x1C */ u8 pad1C[0x58];
  /* 0x74 */ s32 unk74;
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
    D_800B7868[i].unk00 = 0x800;
    D_800B7868[i].unk04 = -1000;
    rand();
    D_800B7868[i].unk08 = 0x1000;
    D_800B7868[i].unk0C = rand() & 0x3FF;
    D_800B7868[i].unk10 = -100;
    D_800B7868[i].unk14 = rand() & 0x3FF;
    D_800B7868[i].unk18 = 0;
    D_800B7868[i].unk74 = 1;
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

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry", func_80037E50);

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry",
            calc_stick_offset_with_noise);

INCLUDE_ASM("asm/nonmatchings/main/get_table_entry", init_ball_for_shot);
