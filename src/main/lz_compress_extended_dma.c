#include "common.h"

/* Terrain/hole-loader module state. */
extern s32 g_terrain_enable_gate;
extern s8 g_terrain_vtx_xform_mode;
extern s32 scenario_mode_id;
extern s32 D_801B6098;
extern const char D_800D1380[]; /* "putter%d:course%d:hole%d\n" */

/* Hole-terrain loader thread + its stack (overlay 4 data). */
extern OSThread D_800E1A50;
extern u8 D_8025D350;

/* Overlay swap + game callees referenced by this TU. */
extern s32 func_80025D78(s32);
extern void unload_overlay(s32);
extern void load_overlay(s32);
extern void load_hole_terrain_assets(s32, s32);
extern s32 func_80051FCC(void);
extern s32 func_80052070(void);
extern void func_8004D148(void);

/* In-TU siblings still carried as INCLUDE_ASM (forward decls for the call
 * sites). */
extern void func_80068F98(void);
extern void func_800695F8(void);
extern void func_80069BCC(void);
extern void func_80069D08(void*);

INCLUDE_ASM("asm/nonmatchings/main/lz_compress_extended_dma",
            lz_compress_extended_dma);

INCLUDE_ASM("asm/nonmatchings/main/lz_compress_extended_dma", func_80068F00);

INCLUDE_ASM("asm/nonmatchings/main/lz_compress_extended_dma", func_80068F18);

INCLUDE_ASM("asm/nonmatchings/main/lz_compress_extended_dma", func_80068F4C);

INCLUDE_ASM("asm/nonmatchings/main/lz_compress_extended_dma", func_80068F98);

void func_800690C0(void) {
  s32 temp_s0;

  osSyncPrintf(D_800D1380, g_terrain_vtx_xform_mode, scenario_mode_id,
               D_801B6098);
  temp_s0 = func_80051FCC();
  load_hole_terrain_assets(temp_s0, func_80052070());
  func_80068F98();
}

INCLUDE_ASM("asm/nonmatchings/main/lz_compress_extended_dma", func_80069124);

INCLUDE_ASM("asm/nonmatchings/main/lz_compress_extended_dma", func_8006955C);

INCLUDE_ASM("asm/nonmatchings/main/lz_compress_extended_dma", func_800695F8);

INCLUDE_ASM("asm/nonmatchings/main/lz_compress_extended_dma", func_80069BCC);

void func_80069CE4(void) {
  func_800695F8();
  func_80069BCC();
}

INCLUDE_ASM("asm/nonmatchings/main/lz_compress_extended_dma", func_80069D08);

void func_80069F28(void) { g_terrain_enable_gate = 1; }

void func_80069F38(void) {
  unload_overlay(func_80025D78(8));
  load_overlay(func_80025D78(6));
  osCreateThread(&D_800E1A50, 0xA, func_80069D08, NULL, &D_8025D350, 0x14);
  osStartThread(&D_800E1A50);
  g_terrain_enable_gate = 0;
  func_8004D148();
}

void func_80069FBC(void) {
  if (g_terrain_enable_gate == 0) {
    osDestroyThread(&D_800E1A50);
    g_terrain_enable_gate = 1;
  }
}
