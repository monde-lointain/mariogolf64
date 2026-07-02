#include "common.h"

extern s32 putter_mode_flag;
extern s32 D_800BA9FC;
extern u8 D_800C41CC[];
extern u8 D_800C6014[];
extern s32 D_800E1C10;
extern s32 D_800E1C14;
extern u8 D_800E1C18;
extern u8 D_800E1C19;
extern u8 D_800E1C1A;
extern u8 D_800E1C1B;
extern u8 D_800E1C1C;
extern u8 D_800E1C1D;

extern void emit_per_phase_fog_state(Gfx**);
extern u32 D_800FC89C;
extern u32 D_80132CEC;
extern u32 D_800C42C4;
extern u8 D_800C4160[];

void func_8006EA90(void) {
  s32 i;
  s32 j;
  s32 off;
  u8* rec;
  u8* rec2;
  u8* base;
  u8* base_hi;
  u8* dst_a;
  u8* dst_b;
  u8* r;
  f32 scale;

  D_800E1C10 = 0;
  D_800E1C14 = 0;
  rec = &D_800C6014[(putter_mode_flag * 3 + D_800BA9FC) * 0x100];
  for (i = 0; i != 4; i++) {
    off = 0x40 + i * 0x10;
    base = D_800C41CC;
    scale = 0.3f;
    base_hi = base + 0x80;
    j = 0;
    dst_b = base_hi + off;
    dst_a = base + off;
    do {
      rec2 = rec;
      r = rec2 + j;
      *dst_a = (s32)((f32)(r[4] + r[7]) * scale);
      j++;
      dst_a++;
      *dst_b = (s32)((f32)(r[4] + r[7]) * scale);
      dst_b++;
    } while (j != 3);
  }
  D_800E1C1B = (s32)((f32)(rec2[4] + rec2[7]) * 0.35f);
  D_800E1C1C = (s32)((f32)(rec2[5] + rec2[8]) * 0.35f);
  D_800E1C1D = (s32)((f32)(rec2[6] + rec2[9]) * 0.35f);
  D_800E1C18 = (u32)((f64)rec2[0xA] * 0.8);
  D_800E1C19 = (u32)((f64)rec2[0xB] * 0.8);
  D_800E1C1A = (u32)((f64)rec2[0xC] * 0.8);
}

void func_8006ED2C(void) {}

/* Builds a fixed screen-space filter/texture display list into *arg0, splicing
 * in emit_per_phase_fog_state() midway. Command words are emitted verbatim. */
#define GFX_CMD(cw0, cw1) (g = dl++, g->words.w0 = (cw0), g->words.w1 = (cw1))

void func_8006ED34(Gfx** arg0) {
  Gfx* dl;
  Gfx* g;
  u32 tileLo;
  u32 tileHi;

  dl = *arg0;
  GFX_CMD(0xE7000000, 0);
  GFX_CMD(0xE3001801, 0);
  GFX_CMD(0xE7000000, 0);
  GFX_CMD(0xE7000000, 0);
  GFX_CMD(0xE3000A01, 0x100000);
  GFX_CMD(0xE7000000, 0);
  GFX_CMD(0xE2001E01, 0);
  GFX_CMD(0xE7000000, 0);
  GFX_CMD(0xE3001201, 0x2000);
  GFX_CMD(0xE7000000, 0);
  GFX_CMD(0xE200001C, 0xC81049D8);
  GFX_CMD(0xFC157E60, 0x2FFD77F8);
  GFX_CMD(0xFA000000, 0x7F7F7FBF);
  GFX_CMD(0xFB000000, 0x3F7F9FFF);
  emit_per_phase_fog_state(&dl);
  GFX_CMD(0xD9000000, 0);
  GFX_CMD(0xD9FFFFFF, 0x210005);
  GFX_CMD(0xFD100000, (D_800FC89C + 8) & ~7);
  GFX_CMD(0xF5100000, 0x7014050);
  GFX_CMD(0xE6000000, 0);
  GFX_CMD(0xF3000000, 0x73FF100);
  GFX_CMD(0xE7000000, 0);
  GFX_CMD(0xF5101000, 0x14050);
  GFX_CMD(0xF2000000, 0x7C07C);
  GFX_CMD(0xFD100000, (D_80132CEC + 8) & ~7);
  GFX_CMD(0xF5100100, 0x7014050);
  GFX_CMD(0xE6000000, 0);
  GFX_CMD(0xF3000000, 0x73FF100);
  GFX_CMD(0xE7000000, 0);
  GFX_CMD(0xF5101100, 0x1014050);
  GFX_CMD(0xF2000000, 0x107C07C);
  GFX_CMD(0xD7000002, 0xFFFFFFFF);
  GFX_CMD(0xF2000000, 0x40040);
  tileLo = D_800C42C4 + 1;
  tileHi = D_800C42C4 + 0x41;
  D_800C42C4 = tileLo;
  GFX_CMD((tileLo & 0xFFF) << 0xC | 0xF2000000,
          (tileHi & 0xFFF) << 0xC | 0x1000040);
  GFX_CMD(0xE7000000, 0);
  GFX_CMD(0x100600C, (u32)&D_800C4160);
  GFX_CMD(0x6020004, 0x20406);
  GFX_CMD(0x6060408, 0x6080A);
  GFX_CMD(0xE7000000, 0);
  *arg0 = dl;
}

#undef GFX_CMD
