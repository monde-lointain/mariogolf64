# init_sky_pool_and_world_state near-match (S256 carry)

Main-segment sky-dome vertex-pool builder + world-state reset in
`src/main/func_80080220.c` (0x284 / 644B). Body 100% RE'd; CARRIED on a
pervasive grid-builder biv-regalloc + loop-bound-hoist divergence (93 asm-differ
rows). [[grid-builder-CE88-regalloc-levers]] / [[loop-invariant-hoist-order-preheader-regalloc]]
wall class (kin to S241 grid-builder, carried on the same two-level loop-hoist).

## Behavior (fully RE'd)
1. `D_800C5478 = 0`.
2. **Vtx-grid build** (`sky_dome_vertex_pool`, `Vtx` 0x10 stride): 4 blocks x 5
   rows x 6 cols. Per vertex: `ob[0]=x` (col: 0,+0xA0), `ob[1]=y` (row-const, 0
   then -0xA0/row), `ob[2]=0`, `tc[0]=s` (col: 0,+0x266), `tc[1]=t` (row-const, 0
   then +0x2A0/row), `cn[0..3]=0xB7`. `x`/`s` reset each row; `y`/`t` reset each
   block; block base = `&pool[block*30]` (byte +0x1E0).
3. **s-texcoord overwrite** for blocks 2,3 (2 x 5 x 6): `tc[0]=s`, s=0xC00 then
   -0x266/col, reset each row. Base `&pool[block*30]` (byte offset 0x3C0 +0x1E0).
4. **tint fill**: `sky_tint_color_array[0..0x1D] = -0x49` (30 bytes).
5. `guOrtho(&D_801B5638, -160,160,-120,120, 1, 1000, 1)`.
6. `guPerspective(&D_80105278, &sky_panel_persp_norm, 10, 1.3333334, 10, 30000, 1)`.
7. `guPerspective(&D_800FE2F0, &sky_panel_persp_norm, 45, 1.3333334, 10, 8000, 1)`.
8. Zero 14 world-state globals in this order: wind_dir_angle_rad,
   sky_view_elev_deg, D_801B7F48, sky_view_elev_rad, sky_view_azimuth_rad,
   D_801B7F40, D_801B7F6C, D_801B7F3C, D_801B7F4C, D_801B7F54, D_801B7F5C,
   D_801B7F44, D_801B7F70, putting_meter_level.

The full near-match C is in the S256 git history (the reverted diff on
func_80080220.c); the logic is byte-faithful, only the loop codegen diverges.

## Terminal divergence (grid-builder regalloc/hoist, not source-shape-leverable at first pass)
- **Loop-bound re-materialization.** ROM keeps `li v0,5` / `li v0,4` INLINE at
  each loop-exit test (`bne a1,v0` / `bne t4,v0`); my build HOISTS the bounds
  into callee-saved regs (`bne a1,t6` with t6=5 preloaded in the prologue). The
  `!= N` written bound is hoisted by gcc-2.7.2's loop invariant motion.
- **Biv base shape.** ROM materializes the block base as `a3 = t6 + s0` (t6 =
  byte offset += 0x1E0, s0 = pool base) re-added per outer iteration; my pointer
  strength-reduction assigns different regs (t7 base + t4 offset) and cascades the
  whole allocation (a0 vs a3 for the walking vtx ptr, etc.).
- 93 asm-differ rows: pervasive, not a one-line fix. Needs the grid-builder lever
  set (array-index-not-ptr base, block-scoped temps, precompute order) iterated;
  S241's analog carried after multi-lever work. Escalation = the
  [[grid-builder-CE88-regalloc-levers]] + [[loop-invariant-hoist-order-preheader-regalloc]]
  levers, or permuter once a spelling reaches >= 0.97.

Callees confirmed: guOrtho/guPerspective (gu.h). sky_panel_persp_norm is u16
(shared with the S256-banked func_8008658C). Vtx via gbi.h.
