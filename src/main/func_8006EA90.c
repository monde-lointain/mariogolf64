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
extern Vtx D_800C4160[];

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

/* Builds a fixed screen-space two-texture filter (fog-blended, scrolling second
 * layer) display list into *glistp, splicing emit_per_phase_fog_state() in
 * after the render-state setup. */
void func_8006ED34(Gfx** glistp) {
  Gfx* gfx = *glistp;
  u32 uls;
  u32 lrs;

  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_MAGICSQ);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_2CYCLE);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_XLU_SURF2);
  gDPSetCombineLERP(gfx++, TEXEL0, TEXEL1, PRIMITIVE_ALPHA, TEXEL1, 0, 0, 0,
                    PRIMITIVE, PRIMITIVE, 0, COMBINED, 0, 0, 0, 0, COMBINED);
  gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0x7F, 0x7F, 0xBF);
  gDPSetEnvColor(gfx++, 0x3F, 0x7F, 0x9F, 0xFF);
  emit_per_phase_fog_state(&gfx);
  gSPLoadGeometryMode(gfx++, 0);
  gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_FOG | G_SHADING_SMOOTH);
  gDPLoadTextureBlock(gfx++, (D_800FC89C + 8) & ~7, G_IM_FMT_RGBA, G_IM_SIZ_16b,
                      32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP,
                      G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
  gDPLoadMultiBlock(gfx++, (D_80132CEC + 8) & ~7, 0x0100, 1, G_IM_FMT_RGBA,
                    G_IM_SIZ_16b, 32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
  gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
  gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 0x40, 0x40);
  uls = D_800C42C4 + 1;
  lrs = D_800C42C4 + 0x41;
  D_800C42C4 = uls;
  gDPSetTileSize(gfx++, 1, uls, 0, lrs, 0x40);
  gDPPipeSync(gfx++);
  gSPVertex(gfx++, D_800C4160, 6, 0);
  gSP2Triangles(gfx++, 1, 0, 2, 0, 1, 2, 3, 0);
  gSP2Triangles(gfx++, 3, 2, 4, 0, 3, 4, 5, 0);
  gDPPipeSync(gfx++);
  *glistp = gfx;
}
