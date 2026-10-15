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
  /* 0x00 */ u8 unk00[0x94];
  /* 0x94 */ s16 unknown_94;
  /* 0x96 */ u8 unk96[2];
} SparkParticle; /* 0x98 */

typedef struct {
  /* 0x00 */ SparkParticle* particles;
  /* 0x04 */ s32 particle_count;
  /* 0x08 */ s32 unk08;
  /* 0x0C */ s32 unk0C;
  /* 0x10 */ u8 unk10[0x14];
  /* 0x24 */ u8 unk24;
  /* 0x25 */ u8 unk25;
  /* 0x26 */ u8 unk26[2];
  /* 0x28 */ SparkSrc* source;
} SparkGroup; /* 0x2C */

extern s32 D_80105148;
extern u8 D_80105165;
extern s32 D_800C4660;
extern s32 D_800C4664;
extern SparkSrc* D_800FE338;
extern SparkGroup D_80105140[];
extern u16 D_800C4698[];

extern void heap3_free(void** payload_ptr);
extern void* heap3_alloc(u32 need);

void func_80077BF0(void) {
  D_80105148 = 150;
  D_80105165 = 1;
}

void func_80077C0C(void) { D_80105165 = 0; }

void func_80077C18(s32 arg0) {
  RomAssetLoad asset;
  SparkGroup* g;
  SparkSrc* src;
  s32 groupCount;
  s32 alpha;
  s32 val8;
  s32 i;
  s32 rows;
  s32 cols;
  s32 row;
  s32 col;
  s32 base;
  s32 n;

  D_800FE338 = heap3_alloc(get_rom_asset_allocation_size(D_800C4698[arg0 % 8], &asset));
  load_rom_asset_payload(D_800FE338, &asset);

  alpha = 0xFF;
  groupCount = 1;
  val8 = 0x9F;
  for (i = 0; i != groupCount; i++) {
    D_80105140[i].unk24 = 0;
    D_80105140[i].unk25 = 0;
    D_80105140[i].unk08 = val8;
    val8 -= 0x32;
    D_80105140[i].unk0C = alpha;
    D_80105140[i].particle_count = 0;
    D_80105140[i].source = D_800FE338;
  }
  D_800C4660 = groupCount;

  for (i = 0; i != D_800C4660; i++) {
    g = &D_80105140[i];
    rows = g->source->row_count;
    cols = g->source->column_count;
    row = 0;
    if (rows != 0) {
      base = 0;
      for (; row != rows; row++) {
        col = 0;
        if (cols != 0) {
          n = base;
          src = g->source;
          for (; col != cols; col++) {
            if (*(col + n + src->source_grid) == 0) {
              g->particle_count++;
            }
          }
        }
        base += cols;
      }
    }
    g->particles = heap3_alloc(g->particle_count * 0x98);
    row = 0;
    if (g->particle_count != 0) {
      for (; row != g->particle_count; row++) {
        g->particles[row].unknown_94 = 0;
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
  s32 v = D_800C4664;

  if (v > 0) {
    return 0;
  }
  v -= 1;
  D_800C4664 = v;
  return v < -2;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80077BF0", func_80077E94);

INCLUDE_ASM("asm/nonmatchings/main/func_80077BF0", render_spark_effects);
