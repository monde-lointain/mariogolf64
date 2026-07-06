#include "common.h"

/* Terrain/hole-loader module state. */
extern s32 g_terrain_enable_gate;
extern s8 g_terrain_vtx_xform_mode;
extern s32 scenario_mode_id;
extern s32 D_801B6098;
extern const char D_800D1380[]; /* "putter%d:course%d:hole%d\n" */
extern u32 D_800C3FA0;          /* LCG scratch-RNG state */

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
extern void func_80069124(void);
extern void func_80069BCC(void);
extern void func_80069D08(void*);

/* func_8006955C deps (mode-select dispatch). */
extern u32 D_800BA9FC;
extern s32 D_800BB0E8;
extern void func_80253234(s32);
extern void func_80253270(void);
extern void func_80057914(void);

/* func_80069BCC deps (camera + terrain-array reset). */
extern s32 camera_position_x;
extern s32 camera_position_y;
extern s32 camera_position_z;
extern s32 get_interpolated_terrain_height_wrapper(s32, s32);
extern s32 flag_is_set(s32);
extern void func_8005F964(void);
extern void func_80080220(void);
extern void func_8005F1C8(void);
extern void func_80032E88(void);
extern const char D_800D139C[];
extern const char D_800D13A8[];
extern const char D_800D13B4[];
extern s32 D_800FBE70;
extern s32 D_801B60B0;
extern s32 D_80105B20[];
extern s32 D_80104E70[];
extern s32 D_8010CF60[];
extern s32 D_80106148[];

INCLUDE_ASM("asm/nonmatchings/main/lz_compress_extended_dma",
            lz_compress_extended_dma);

/*
 * A small LCG scratch RNG (state D_800C3FA0). The two helpers are GCC nested
 * functions of func_80068F4C: only this parent calls them, and each call site
 * materializes the static chain ($v0 = &frame) before the jal, so they emit
 * with the chain prologue (dead `sw v0,0(sp)`) at 0x80068F00 / 0x80068F18 ahead
 * of the parent (0x80068F4C). See
 * docs/hazards.md#nested-function-static-chain-spill. The parent is called
 * externally by func_800695F8, so it stays top-level.
 */
void func_80068F4C(s32 arg0, u32 seed) {
  void rng_seed(u32 v) { D_800C3FA0 = v; } /* 0x80068F00 */
  u32 rng_next(void) {
    return D_800C3FA0 = D_800C3FA0 * 0x5D588B65 + 1;
  } /* 0x80068F18 */

  rng_seed(seed);
  arg0 += rng_next();
  rng_seed(arg0);
  rng_next();
}

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

void func_8006955C(void) {
  s32 mode;

  D_800BB0E8 = 1;
  switch (D_800BA9FC) {
    case 0:
      mode = 2;
      break;
    case 6:
      mode = 1;
      break;
    case 10:
      mode = 0;
      break;
  }
  if (!flag_is_set(0x11)) {
    func_80253234(mode);
  }
  D_800BB0E8 = 0;
  func_80253270();
  func_80057914();
  func_80069124();
}

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
