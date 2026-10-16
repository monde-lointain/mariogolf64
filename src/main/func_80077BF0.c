#include "common.h"
#include "rom_asset.h"

typedef struct {
  /* 0x00 */ u8 unk00[3];
  /* 0x03 */ u8 column_count;
  /* 0x04 */ u8 unk04;
  /* 0x05 */ u8 row_count;
  /* 0x06 */ u8 unk06[2];
  /* 0x08 */ u8 source_grid[1];
} SparkSrc;

typedef struct {
  /* 0x00 */ u8 unknown_00[0x80];
  /* 0x80 */ f32 position_x;
  /* 0x84 */ f32 position_y;
  /* 0x88 */ f32 velocity_x;
  /* 0x8C */ f32 velocity_y;
  /* 0x90 */ u8 unknown_90[4];
  /* 0x94 */ s16 frames_remaining;
  /* 0x96 */ u8 unknown_96[2];
} SparkParticle; /* 0x98 */

typedef struct {
  /* 0x00 */ SparkParticle* particles;
  /* 0x04 */ s32 particle_count;
  /* 0x08 */ s32 animation_timer;
  /* 0x0C */ s32 unk0C;
  /* 0x10 */ u8 unk10[0x14];
  /* 0x24 */ u8 unk24;
  /* 0x25 */ u8 animation_enabled;
  /* 0x26 */ u8 unk26[2];
  /* 0x28 */ SparkSrc* source;
} SparkGroup; /* 0x2C */

extern s32 D_800C4660;
extern s32 D_800C4664;
extern SparkSrc* D_800FE338;
extern SparkGroup D_80105140[];
extern u16 D_800C4698[];

extern void heap3_free(void** payload_ptr);
extern void* heap3_alloc(u32 need);

void func_80077BF0(void) {
  D_80105140[0].animation_timer = 150;
  D_80105140[0].animation_enabled = 1;
}

void func_80077C0C(void) { D_80105140[0].animation_enabled = 0; }

void func_80077C18(s32 asset_index) {
  RomAssetLoad asset;
  SparkGroup* group;
  SparkSrc* source;
  s32 group_count;
  s32 alpha;
  s32 animation_timer;
  s32 group_index;
  s32 row_count;
  s32 column_count;
  s32 row;
  s32 col;
  s32 grid_offset;
  s32 row_offset;

  D_800FE338 = heap3_alloc(get_rom_asset_allocation_size(D_800C4698[asset_index % 8], &asset));
  load_rom_asset_payload(D_800FE338, &asset);

  alpha = 0xFF;
  group_count = 1;
  animation_timer = 0x9F;
  for (group_index = 0; group_index != group_count; group_index++) {
    D_80105140[group_index].unk24 = 0;
    D_80105140[group_index].animation_enabled = 0;
    D_80105140[group_index].animation_timer = animation_timer;
    animation_timer -= 0x32;
    D_80105140[group_index].unk0C = alpha;
    D_80105140[group_index].particle_count = 0;
    D_80105140[group_index].source = D_800FE338;
  }
  D_800C4660 = group_count;

  for (group_index = 0; group_index != D_800C4660; group_index++) {
    group = &D_80105140[group_index];
    row_count = group->source->row_count;
    column_count = group->source->column_count;
    row = 0;
    if (row_count != 0) {
      grid_offset = 0;
      for (; row != row_count; row++) {
        col = 0;
        if (column_count != 0) {
          row_offset = grid_offset;
          source = group->source;
          for (; col != column_count; col++) {
            if (*(col + row_offset + source->source_grid) == 0) {
              group->particle_count++;
            }
          }
        }
        grid_offset += column_count;
      }
    }
    group->particles = heap3_alloc(group->particle_count * 0x98);
    row = 0;
    if (group->particle_count != 0) {
      for (; row != group->particle_count; row++) {
        group->particles[row].frames_remaining = 0;
      }
    }
  }
  D_800C4664 = 1;
}

void func_80077DEC(void) {
  s32 i;

  if (D_800C4664 == 1) {
    i = 0;
    heap3_free((void**)&D_800FE338);
    if (D_800C4660 != 0) {
      for (; i != D_800C4660; i++) {
        heap3_free((void**)&D_80105140[i].particles);
      }
    }
  }
  D_800C4664 = 0;
}

s32 func_80077E6C(void) {
  s32 state = D_800C4664;

  if (state > 0) {
    return 0;
  }
  state -= 1;
  D_800C4664 = state;
  return state < -2;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80077BF0", func_80077E94);

INCLUDE_ASM("asm/nonmatchings/main/func_80077BF0", render_spark_effects);
