# init_sky_pool_and_world_state near-match (S256 carry, rebuilt S258)

Main-segment sky-dome vertex-pool builder + world-state reset in
`src/main/func_80080220.c` (0x284 / 644B / 161 instrs).

| sprint | build instrs | divergence |
| ------ | ------------ | ---------- |
| S256   | (short)      | 93 asm-differ rows: bounds hoisted into callee-saved regs, pointer-giv shape mismatch |
| S258   | **161 (exact)** | instruction shapes align 1:1; residual is a systematic register permutation over 55 of the 161 instructions plus one 2-instruction init swap |

## Behaviour (fully RE'd)

1. `D_800C5478 = 0`.
2. **Vtx-grid build** over `sky_dome_vertex_pool` (`Vtx`, 0x10 stride): 4 blocks x
   5 rows x 6 cols, one continuous walking vertex pointer per block. Per vertex
   `ob[0]=x`, `ob[1]=y`, `ob[2]=0`, `tc[0]=s`, `tc[1]=t`, `cn[0..3]=0xB7`; `x`
   steps +0xA0 and `s` steps +0x266 per column (both reset per row); `y` steps
   -0xA0 and `t` steps +0x2A0 per row (both reset per block); block base steps
   +0x1E0.
3. **s-texcoord overwrite** for blocks 2 and 3 (2 x 5 x 6): `tc[0] = s`, `s`
   starting at 0xC00 and stepping -0x266 per column, reset per row.
4. **tint fill**: `sky_tint_color_array[0..0x1D] = -0x49`.
5. `guOrtho(&D_801B5638, -160, 160, -120, 120, 1, 1000, 1)`.
6. `guPerspective(&D_80105278, &sky_panel_persp_norm, 10, 1.3333334, 10, 30000, 1)`.
7. `guPerspective(&D_800FE2F0, &sky_panel_persp_norm, 45, 1.3333334, 10, 8000, 1)`.
8. Fourteen world-state globals zeroed, in the order below.

## Best reconstruction (S258, 161/161 instrs, shapes 1:1)

```c
void init_sky_pool_and_world_state(void) {
  s32 block = 0;
  Vtx* pool = sky_dome_vertex_pool;
  s32 color = 0xB7;
  s32 cols = 6;
  s32 block_off = 0;
  Vtx* pool2;
  s32 cols2;
  s32 s_init;
  s32 tint_count;
  s32 row;
  s32 col;
  s32 x;
  s32 y;
  s32 s;
  s32 t;
  s32 row_y;
  s32 row_t;
  s32 i;
  Vtx* v;

  D_800C5478 = 0;

block_loop:
  v = (Vtx*)(block_off + (u32)pool);
  row = 0;
  t = 0;
  y = 0;
row_loop:
  col = 0;
  row_y = y;
  row_t = t;
  s = 0;
  x = 0;
  do {
    v->v.ob[0] = x;
    v->v.tc[0] = s;
    s += 0x266;
    x += 0xA0;
    col++;
    v->v.ob[1] = row_y;
    v->v.ob[2] = 0;
    v->v.tc[1] = row_t;
    v->v.cn[0] = color;
    v->v.cn[1] = color;
    v->v.cn[2] = color;
    v->v.cn[3] = color;
    v++;
  } while (col != cols);
  t += 0x2A0;
  row++;
  y -= 0xA0;
  if (row != 5) {
    goto row_loop;
  }
  block++;
  block_off += 0x1E0;
  if (block != 4) {
    goto block_loop;
  }

  block = 2;
  pool2 = sky_dome_vertex_pool;
  s_init = 0xC00;
  cols2 = 6;
  block_off = 0x3C0;
overwrite_block_loop:
  v = (Vtx*)(block_off + (u32)pool2);
  row = 0;
overwrite_row_loop:
  col = 0;
  s = s_init;
overwrite_col_loop:
  v->v.tc[0] = s;
  s -= 0x266;
  col++;
  v++;
  if (col != cols2) {
    goto overwrite_col_loop;
  }
  row++;
  if (row != 5) {
    goto overwrite_row_loop;
  }
  block++;
  block_off += 0x1E0;
  if (block != 4) {
    goto overwrite_block_loop;
  }

  i = 0;
  tint_count = 0x1E;
tint_loop:
  sky_tint_color_array[i] = -0x49;
  i++;
  if (i != tint_count) {
    goto tint_loop;
  }

  guOrtho(&D_801B5638, -160.0f, 160.0f, -120.0f, 120.0f, 1.0f, 1000.0f, 1.0f);
  guPerspective(&D_80105278, &sky_panel_persp_norm, 10.0f, 1.3333334f, 10.0f,
                30000.0f, 1.0f);
  guPerspective(&D_800FE2F0, &sky_panel_persp_norm, 45.0f, 1.3333334f, 10.0f,
                8000.0f, 1.0f);

  wind_dir_angle_rad = 0.0f;
  sky_view_elev_deg = 0.0f;
  D_801B7F48 = 0.0f;
  sky_view_elev_rad = 0.0f;
  sky_view_azimuth_rad = 0.0f;
  D_801B7F40 = 0.0f;
  D_801B7F6C = 0.0f;
  D_801B7F3C = 0.0f;
  D_801B7F4C = 0.0f;
  D_801B7F54 = 0.0f;
  D_801B7F5C = 0.0f;
  D_801B7F44 = 0.0f;
  D_801B7F70 = 0.0f;
  putting_meter_level = 0.0f;
}
```

Declarations needed: `extern Vtx sky_dome_vertex_pool[];`,
`extern s8 sky_tint_color_array[];`, `extern Mtx D_801B5638/D_80105278/D_800FE2F0;`,
`extern s32 D_800C5478;`, and the fourteen `f32` world-state globals (plus the
pre-existing `extern s32 D_801B7F70;` further down the file — declare it before
this function or the build errors on a conflicting type).

## The S258 rebuild: selective loop discovery

Which loops loop.c is allowed to SEE is the whole game here, and the ROM wants a
different answer per loop nest level. gcc-2.7.2's loop.c only processes loops it
discovers through `NOTE_INSN_LOOP_BEG`, which the front end emits for
`for`/`while`/`do-while` and never for a goto loop.

- **The two OUTER grid loops must be gotos.** The ROM re-materialises `li v0,5`
  and `li v0,4` inline at each exit test and does NOT strength-reduce the block
  base (`addu a3,t6,s0` recomputed per block iteration). Any structured spelling
  hoists both bounds into callee-saved registers and folds the block base into a
  walking pointer — the S256 failure mode.
- **The INNER col loop must be structured.** The ROM has TWO pointer givs walking
  the vertex (`a3` with `0(a3)` for `ob[0]`, plus `v0 = a3 + 0xF` with negative
  displacements for the rest), which only loop.c's strength reduction produces.
  As a goto loop the body collapses onto one pointer and comes out 4 instrs short.
- **Every constant the ROM holds in a register across a goto loop must be a local
  variable**, because the goto loop de-hoists literals: `color` (0xB7), `cols`
  (6), `s_init` (0xC00), `tint_count` (0x1E), and the pool base. Each must be
  assigned where the ROM materialises it (grid 1's in the prologue, grid 2's just
  before its block loop, the tint bound just before the tint loop), and grid 2
  needs its OWN `cols2`/`pool2` because the ROM materialises `6` and the pool base
  a second time into different registers.
- `v = (Vtx*)(block_off + (u32)pool)` — casting the pointer to `u32` puts the
  integer operand FIRST, matching the ROM's `addu a3,t6,s0`; `(u8*)pool +
  block_off` canonicalises the pointer to operand 0 and emits `addu a3,s0,t6`.

## Residual

A systematic register permutation of the grid-1 locals, with the same set of hard
registers used on both sides and the same quantity creation ORDER:

| variable | ROM | build |
| -------- | --- | ----- |
| block | t4 | t1 |
| block_off | t6 | t0 |
| vertex ptr | a3 | a1 |
| row | a1 | a3 |
| t | t1 | t4 |
| y | t0 | t3 |
| col | v1 | a0 |
| row_y | t5 | t6 |
| row_t | t3 | t5 |
| s | a2 | v1 |
| x | a0 | a2 |

(`pool`->s0, `color`->t2, `cols`->t7 and the giv->v0 all match.) This is
local-alloc quantity PRIORITY, not order: `qty_compare_1` ranks by
`floor_log2(n_refs) * n_refs * size / live_length`, so the permutation moves as a
block and no single-variable source tweak shifts it (row-top init order swaps
tried: `x`/`s` order, `col` first vs last — both stay at ~110 diff rows).

Plus one 2-instruction swap: the ROM emits the giv init `addiu v0,a3,0xF` BEFORE
the `s = 0` / `x = 0` inits at the row-loop top; loop.c places it at the end of
the preheader, so my build emits it after.

Permuter plateaued at 545 from a base of 615 in 60k+ iterations
(`nonmatchings/init_sky_pool_and_world_state/`, imported with
`permuter_settings_main.toml`). Escalation: `#pervasive-regalloc-classical-main` /
`#local-alloc-qty-permutation` — a local-alloc.c priority dive, or corpus-mining a
matched grid-builder sibling. Kin to the S241 grid-builder carry.
