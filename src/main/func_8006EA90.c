#include "common.h"
#include "fog.h"

extern s32 putter_mode_flag;
extern s32 D_800BA9FC;
extern u8 D_800C41CC[];
extern s32 D_800E1C10;
extern s32 D_800E1C14;
extern u8 D_800E1C18;
extern u8 D_800E1C19;
extern u8 D_800E1C1A;
extern u8 D_800E1C1B;
extern u8 D_800E1C1C;
extern u8 D_800E1C1D;
extern void emit_per_phase_fog_state(Gfx**);
extern u32 fog_filter_texture_primary;
extern u32 fog_filter_texture_secondary;
extern u32 fog_texture_scroll_offset;
extern Vtx fog_filter_vertices[];

void update_putter_color_tables(void) {
  s32 table_index;
  FogPreset* record;
  FogPreset* final_record;

  D_800E1C10 = 0;
  D_800E1C14 = 0;
  record = &D_800C6014[putter_mode_flag * 3 + D_800BA9FC][0];
  for (table_index = 0; table_index != 4; table_index++) {
    s32 record_index;
    s32 record_offset;
    u8* color_table;
    u8* upper_color_table;
    u8* destination;
    u8* upper_destination;
    f32 color_scale;

    record_offset = 0x40 + table_index * 0x10;
    color_table = D_800C41CC;
    color_scale = 0.3f;
    upper_color_table = color_table + 0x80;
    record_index = 0;
    upper_destination = upper_color_table + record_offset;
    destination = color_table + record_offset;
    for (; record_index != 3; record_index++) {
      final_record = record;
      *destination =
          (s32)((f32)(final_record->color_set_t[record_index] +
                       final_record->color_set_b[record_index]) * color_scale);
      destination++;
      *upper_destination =
          (s32)((f32)(final_record->color_set_t[record_index] +
                       final_record->color_set_b[record_index]) * color_scale);
      upper_destination++;
    }
  }
  D_800E1C1B = (s32)((f32)(final_record->color_set_t[0] + final_record->color_set_b[0]) * 0.35f);
  D_800E1C1C = (s32)((f32)(final_record->color_set_t[1] + final_record->color_set_b[1]) * 0.35f);
  D_800E1C1D = (s32)((f32)(final_record->color_set_t[2] + final_record->color_set_b[2]) * 0.35f);
  D_800E1C18 = (u32)((f64)final_record->color_set_c[0] * 0.8);
  D_800E1C19 = (u32)((f64)final_record->color_set_c[1] * 0.8);
  D_800E1C1A = (u32)((f64)final_record->color_set_c[2] * 0.8);
}

void fog_filter_noop(void) {}

/* Keep exact packet order/arguments, including duplicate syncs and the
 * per-phase fog splice; textures stay 8-byte aligned, and the scrolling tile
 * advances by 1 with its lower-right coordinate at offset 0x40. */
void append_fogged_texture_filter(Gfx** glistp) {
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
  gDPLoadTextureBlock(gfx++, (fog_filter_texture_primary + 8) & ~7,
                      G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, 0,
                      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5,
                      5, G_TX_NOLOD, G_TX_NOLOD);
  gDPLoadMultiBlock(gfx++, (fog_filter_texture_secondary + 8) & ~7, 0x0100, 1,
                    G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, 32, 0,
                    G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 5, 5,
                    G_TX_NOLOD, G_TX_NOLOD);
  gSPTexture(gfx++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
  gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 0x40, 0x40);
  uls = fog_texture_scroll_offset + 1;
  lrs = fog_texture_scroll_offset + 0x41;
  fog_texture_scroll_offset = uls;
  gDPSetTileSize(gfx++, 1, uls, 0, lrs, 0x40);
  gDPPipeSync(gfx++);
  gSPVertex(gfx++, fog_filter_vertices, 6, 0);
  gSP2Triangles(gfx++, 1, 0, 2, 0, 1, 2, 3, 0);
  gSP2Triangles(gfx++, 3, 2, 4, 0, 3, 4, 5, 0);
  gDPPipeSync(gfx++);
  *glistp = gfx;
}
