# draw_terrain_aim_grid — near-match (S306)

`src/main/func_80080220.c`, 0x1514 / **1349 instructions**. Reconstructed from scratch this sprint;
it was the last `fresh` row `--loose-stubs main` reported and the largest body the `dl-emitter`
class has attempted (1.4x S305's `func_8007EF0C`).

## State

**1349 / 1349 instructions — exact count.** Frame `-0x2D08` against the ROM's `-0x2CF8` (0x10, two
8-byte slots, too large). Every structural feature is reproduced: the four meter/mode arms, the trig
basis, the scroll-phase wrap, the grid build with its terrain-height callback, the half-density
interpolation pass, the projection + shade pass, the display-list preamble, both emit passes and the
epilogue.

The residual is an **allocation equilibrium**, not structure. At an identical instruction total the
mnemonic histogram reads `mtc1` +8, `nop` +5, `li` -4, `move` -4, `sw` -3. The ROM holds two of the
function's floats — `lead` (the 2.2/12.0 lead constant) and `colX` — in **general** registers as raw
bit patterns and a third (`baseX`) on the stack, converting with `mtc1` at each use, because eight
floats are live across the four `sinf`/`cosf` calls and only six callee-saved FP registers exist.
This build spends those six differently and therefore materialises the SFmode constants into FP
registers at each of the four arm assignments (`lui`/`ori`/`mtc1` per arm) where the ROM materialises
into one GPR (`lui`/`ori` per arm) and pays a single `mtc1` at the one use — exactly the +3
`mtc1 $at` in the delta.

## What each step of the reconstruction was worth

Measured, in order, from the first build at 1273:

| lever | delta |
| --- | --- |
| goto-loop form on all five inner loops + the interp outer loop | +39 |
| explicit `if (cols != 0)` entry guard on each goto loop | +10 |
| the `rgba` pack written inside each arm of the `j & 1` split | +7 |
| `rows` as a one-element array (stays memory-resident) | +6 |
| colour blocks in the ROM's arm order with `r = 0xFF; b = r - sh; g = b;` | +6 |
| `f32 fzero` variable in place of the `0.0f` literals | +5 |
| `k + 8` rather than `k + cols` for the loop-B row stride | +4 |
| arm-3 divisor `(f32)full` with `rows` assigned last in that arm | +3 |

The goto-loop result is the load-bearing one and it generalises: `loop.c` was reducing `&scr[k]`,
`&scr[k-1]` and `&shade[k]` to givs updated at the loop head, while the ROM recomputes `sll k,4` /
`addu` inside each basic block that needs it. See `docs/levers.md`
(goto-loop-defeats-loop-strength-reduction) — this is the largest single application of it recorded.

## Proven, do not re-try

- **`x * C1 / 30.0f` folds to one `mul.s`.** This TU is a per-file `-ffast-math` override
  (`mk/main.mk`), and `fold` reassociates `(x * C1) / C2` to `x * (C1/C2)`; splitting it into two
  statements does not help, because a division by any *literal* is turned into a reciprocal multiply
  as well. The ROM's `mul.s` + `div.s` pair is only reachable with a non-constant divisor:
  `/ (f32)full` where `full` is an `s32` set to 30. `cse` then folds the conversion back to a
  `lui`/`mtc1` constant in the arm where the integer 30 is not already live, which is exactly the
  ROM's asymmetry between the two meter arms.
- **Using `(f32)full` in both meter arms costs 22 instructions**: the arms become structurally
  identical and cross-jumping merges their tails. Assigning `rows` *last* in arm 3 breaks the tie
  and keeps both `div.s`.
- **`ox`/`oy`/`oz` as an `s32 org[3]` array costs 23 instructions** — same cross-jump merge, all four
  arms writing one object.
- **`baseX` as a one-element array** (to force the ROM's stack residency) is +3 and moves `lwc1`
  the wrong way.
- Declaration-order permutation of `lead` among the locals: no effect.
- Permuter, imported with `-ffast-math` added to the scratch `compile.sh` (note
  `permuter_settings_main.toml` omits it): base score 36925, ~600 iterations, best 36315. No signal,
  which is the expected shape for a whole-function allocation difference rather than a one-operand
  residual.

## Caveat on the exact count

The `f32 fzero` lever is worth +5 and 2 of those are `mtc1` rematerialisations plus 3 `nop`s, so it
pads the count as much as it fixes anything. Without it the body is 1344 with a *better* histogram
(`mtc1` +6, `nop` +2). Read 1349 as "at count, with one contrivance", and re-derive from the 1344
form if the next attempt wants the cleanest structural base.

## Next lever to try

Read the allocation, do not guess: `venv/bin/python3 tools/allocno_report.py src/main/func_80080220.c
draw_terrain_aim_grid` runs and prints ~90 allocnos. The question to answer from it is which allocno
should lose its FP register so `lead` (four defs, one use, only use is a `sub.s`) falls back to its
alternate class, a general register — that single change is worth the +3 `mtc1 $at` and probably the
frame delta with it. The ROM's own answer is visible in the `.s`: `lead` -> `$s1`, `colX` -> `$s6`,
`baseX` -> `0x2C54($sp)`, and `fs0`-`fs5` hold `colZ`, `span`/`baseZ`, `angle`, `pitch`, `rowZ`,
`rowX`.

## The body

Insert in place of the `INCLUDE_ASM` line. Needs no `symbol_addrs.txt` addition and no `.rodata`
carve: every FP constant is materialised inline by KMC gcc at `-G 0`, so there is no literal pool to
straddle.

```c
/* One projected grid vertex: its screen position, whether the projection
 * landed in front of the camera, and the primitive depth the quad it
 * belongs to is drawn at. */
typedef struct {
  /* 0x00 */ s32 sx;
  /* 0x04 */ s32 sy;
  /* 0x08 */ s32 valid;
  /* 0x0C */ s32 depth;
} AimGridPoint; /* size 0x10 */

typedef struct {
  u8 pad00[8];
  s32 unk_08;
} AimShot;

extern s8 D_801061C8;
extern s32 D_800BE69C;
extern s32 D_801B6088;
extern u16 D_800FBE48;
extern s32 D_800FBE64;
extern s32 D_800FBE68;
extern s32 target_z;
extern s32 D_800BB0E8;
extern s32 D_800D1B10;
extern f64 D_800D1B18;
extern f32 D_800C5AA4;
extern s32 D_800E2970[];
extern s32 D_800C306C;
extern s32 D_800C5A58;
extern s8 D_801B725C[];
extern s32 active_player_idx;
extern s32 project_point_to_screen(f32 x, f32 y, f32 z, s32* out);
extern s32 get_interpolated_terrain_height_wrapper(s32 x, s32 z);
extern Mtx D_1B5638[];
extern AimShot* get_shot_data(void);
extern s32 get_shot_param(void);
extern s32 get_shot_progress(void);
extern f32 sinf(f32 x);
extern f32 cosf(f32 x);
extern f32 sqrtf(f32 x);
extern Vtx D_800C5A60[];
extern Mtx D_800E1E50[];
extern Mtx D_E1E50[];

/**
 * Emits the terrain aiming grid: a lit wireframe mesh laid over the terrain
 * between the ball and the aim target, drawn as two passes of screen-space
 * quads (along each row, then along each column).
 *
 * The grid is 8 columns wide and 8, 16 or 32 rows deep depending on the shot
 * mode and the club's meter reading. Its origin and basis vectors come from
 * the aim angle; every vertex is dropped onto the terrain with
 * `get_interpolated_terrain_height_wrapper` (or read back from the cached
 * heights in `D_800E2970` when the grid is not being rebuilt), projected to
 * screen space, and shaded by the signed square root of its height above the
 * reference plane. A 32-row grid is built at half density and the odd rows are
 * filled in by midpoint interpolation.
 *
 * `D_800C5AA4` is the scroll phase that animates the interior rows toward the
 * target; it wraps by 1.0 or 2.0 depending on the grid depth.
 */
void draw_terrain_aim_grid(Gfx** gfxp, Mtx* proj) {
  f32 pts[256][3];
  AimGridPoint scr[256];
  s32 shade[1024];
  s32 ox;
  s32 oy;
  s32 oz;
  s32 screen[6];
  Gfx* gfx;
  AimShot* shot;
  f32 angle;
  f32 lead;
  f32 span;
  f32 pitch;
  f32 colX;
  f32 colZ;
  f32 rowX;
  f32 rowZ;
  f32 animX;
  f32 animZ;
  f32 baseX;
  f32 baseZ;
  f32 t;
  f32 half;
  f32 dx;
  f32 dy;
  f32 scale;
  s32 rowsv[1];
  f32 fzero;
  s32 cols;
  s32 last;
  s32 param;
  s32 dist;
  s32 i;
  s32 j;
  s32 k;
  s32 k2;
  s32 base;
  s32 r;
  s32 g;
  s32 b;
  s32 rgba;
  s32 alpha;
  s32 full;
  s32 sh;
  s32 edge;
  s32 row1;
  s32 ii;
  s32 n;

  cols = 8;
  fzero = 0.0f;
  gfx = *gfxp;
  if (D_801061C8 == 0) {
    return;
  }
  if (D_800BE69C == 1) {
    return;
  }
  if (flag_is_set(0x79) != 0) {
    return;
  }
  if ((D_801B6088 != 6) & (D_801B6088 != 0xB)) {
    return;
  }

  full = 30;
  shot = get_shot_data();
  angle = (f32)D_800FBE48 * 0.00009587380f;
  param = get_shot_param();
  dist = get_shot_progress();

  if ((shot->unk_08 & 1) != 0) {
    if (param == full) {
      lead = 2.2f;
      span = 18000.0f;
      pitch = 25600.0f;
      rowsv[0] = 8;
      ox = D_800FBE64;
      oy = D_800FBE68;
      oz = target_z;
    } else {
      lead = 2.2f;
      span = 18000.0f;
      rowsv[0] = 0x20;
      ox = D_800FBE64;
      oy = D_800FBE68;
      oz = target_z;
      pitch = (f32)param * 4681.728f / (f32)full;
    }
  } else if (param < 0x3D) {
    lead = 2.2f;
    span = 18000.0f;
    ox = D_800FBE64;
    oy = D_800FBE68;
    oz = target_z;
    pitch = (f32)param * 15360.0f / (f32)full;
    rowsv[0] = 0x20;
  } else {
    ox = (s32)(cosf(angle) * (f32)dist + (f32)D_800FBE64);
    oz = (s32)(sinf(angle) * (f32)dist + (f32)target_z);
    lead = 12.0f;
    pitch = 28800.0f;
    D_800BB0E8 = 1;
    oy = get_interpolated_terrain_height_wrapper(ox, oz);
    rowsv[0] = 0x10;
    span = 307200.0f / (f32)D_800D1B10;
  }

  rowX = -cosf(angle);
  rowZ = -sinf(angle);
  t = (f32)((f64)angle + D_800D1B18);
  colX = -cosf(t) * span;
  colZ = -sinf(t) * span;
  t = (lead - (f32)rowsv[0]) * pitch;
  half = 0.5f - (f32)D_800D1B10 * 0.5f;
  baseX = rowX * t + colX * half;
  baseZ = rowZ * t + colZ * half;
  rowX = -cosf(angle) * pitch;
  rowZ = -sinf(angle) * pitch;

  if (D_801061C8 == 1) {
    D_800C5AA4 -= 0.025f;
    if (rowsv[0] == 0x20) {
      if (D_800C5AA4 < -1.0f) {
        D_800C5AA4 += 2.0f;
      }
    } else {
      if (D_800C5AA4 < fzero) {
        D_800C5AA4 += 1.0f;
      }
    }
  }

  animX = rowX * D_800C5AA4;
  animZ = rowZ * D_800C5AA4;

  i = 0;
  base = 0;
  if (rowsv[0] != 0) {
    last = rowsv[0] - 1;
    do {
      j = 0;
      k = base;
      edge = (i == last) | (i == 0);
      row1 = (i == 1);
      if (cols != 0) {
      build_pt:
        ii = (i == 0) ? i : i - 1;
        if (edge || ((D_800C5AA4 < fzero) & row1)) {
          pts[k][0] =
              (f32)ox + baseX + rowX * (f32)ii + colX * (f32)j;
          pts[k][2] =
              (f32)oz + baseZ + rowZ * (f32)ii + colZ * (f32)j;
        } else {
          pts[k][0] =
              (f32)ox + baseX + rowX * (f32)ii + colX * (f32)j + animX;
          pts[k][2] =
              (f32)oz + baseZ + rowZ * (f32)ii + colZ * (f32)j + animZ;
        }
        if (D_801061C8 == 1) {
          D_800BB0E8 = 1;
          pts[k][1] = (f32)get_interpolated_terrain_height_wrapper(
              (s32)pts[k][0], (s32)pts[k][2]);
          D_800E2970[k] = (s32)pts[k][1];
        } else {
          pts[k][1] = (f32)D_800E2970[k];
        }
        j++;
        k++;
        if (j != cols) {
          goto build_pt;
        }
      }
      if (rowsv[0] == 0x20) {
        if (i < 0x1E) {
          base += cols * 2;
          i += 2;
        } else {
          base += cols;
          i++;
        }
      } else {
        base += cols;
        i++;
      }
    } while (i != rowsv[0]);
  }

  if (rowsv[0] == 0x20) {
    i = 1;
    k = cols * 2;
    k2 = cols;
  interp_row:
    {
      j = 0;
      if (cols != 0) {
        s32 a;
        s32 c;
        s32 d;
      interp_pt:
        a = (i - 1) * cols + j;
        c = k + j;
        d = k2 + j;

        pts[d][0] = (pts[a][0] + pts[c][0]) * 0.5f;
        pts[d][1] = (pts[a][1] + pts[c][1]) * 0.5f;
        j++;
        pts[d][2] = (pts[a][2] + pts[c][2]) * 0.5f;
        if (j != cols) {
          goto interp_pt;
        }
      }
      k += cols * 2;
      k2 += cols * 2;
      i += 2;
    }
    if (i != rowsv[0] - 1) {
      goto interp_row;
    }
  }

  i = 0;
  if (rowsv[0] != 0) {
    base = 0;
    do {
      j = 0;
      k = base;
      if (cols != 0) {
      project_pt:
        if (project_point_to_screen(pts[k][0] * (1.0f / 1024.0f),
                                    pts[k][1] * (1.0f / 1024.0f),
                                    pts[k][2] * (1.0f / 1024.0f),
                                    screen) < 0) {
          scr[k].sx = screen[0] << 2;
          scr[k].valid = 1;
          scr[k].sy = screen[1] << 2;
          scr[k].depth = (s32)((f32)screen[2] - *(f32*)&screen[5] * 15.0f);
        } else {
          scr[k].valid = 0;
        }
        dx = pts[k][1] - (f32)oy;
        if (dx < fzero) {
          shade[k] = (s32)(sqrtf(-dx) * 1.5f);
        } else if (fzero < dx) {
          shade[k] = (s32)(-sqrtf(dx) * 1.5f);
        } else {
          shade[k] = 0;
        }
        if (shade[k] >= 0x100) {
          shade[k] = 0xFF;
        }
        j++;
        if (shade[k] < -0xFF) {
          shade[k] = -0xFF;
        }
        k++;
        if (j != cols) {
          goto project_pt;
        }
      }
      i++;
      base += cols;
    } while (i != rowsv[0]);
  }

  guMtxIdent(D_800E1E50);

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gSPLoadGeometryMode(gfx++, 0);
  gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_ZB_CLD_SURF, G_RM_ZB_CLD_SURF2);
  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PRIM);
  gSPMatrix(gfx++, D_1B5638, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPMatrix(gfx++, D_E1E50, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH);
  gSPVertex(gfx++, D_800C5A60, 4, 0);

  if (D_800C306C == 4) {
    alpha = 0x3F;
  } else {
    alpha = 0x7F;
    if (D_801061C8 == 2) {
      alpha = 0x3F;
      if (D_800C5A58 > 0x47) {
        alpha = D_800C5A58 - 8;
      }
    }
  }
  D_800C5A58 = alpha;

  if (D_801061C8 != 0) {
    i = 0;
    if (rowsv[0] != 0) {
      base = 0;
      do {
        j = 0;
        k = base;
        if (cols != 0) {
        emit_row:
          if (scr[k].valid == 1) {
            if ((j & 1) == 0) {
              k2 = k + 1;
            } else {
              k2 = k - 1;
            }
            dx = (f32)(scr[k].sx - scr[k2].sx);
            dy = (f32)(scr[k].sy - scr[k2].sy);
            if (((dx == fzero) & (dy == fzero)) != 0) {
              scr[k].valid = 0;
            } else {
              scale = 4.5f / sqrtf(dx * dx + dy * dy);
              dx = dx * scale;
              dy = dy * scale;
              if (D_801B725C[active_player_idx * 0xB8] == 0) {
                sh = shade[k];
                if (sh == 0) {
                  b = 0xFF;
                  g = 0xFF;
                  r = 0xFF;
                } else if (sh > 0) {
                  r = 0xFF;
                  b = r - sh;
                  g = b;
                } else {
                  b = 0xFF;
                  g = sh + 0xFF;
                  r = g;
                }
              } else {
                sh = shade[k];
                b = 0;
                if (sh == 0) {
                  g = 0;
                  r = 0;
                } else if (sh > 0) {
                  r = sh;
                  g = 0;
                } else {
                  b = -sh;
                  g = 0;
                  r = 0;
                }
              }
              if ((j & 1) == 0) {
                gSPModifyVertex(gfx++, 0, G_MWO_POINT_XYSCREEN,
                                (scr[k].sx << 16) + scr[k].sy);
                gSPModifyVertex(gfx++, 1, G_MWO_POINT_XYSCREEN,
                                ((scr[k].sx + (s32)dy) << 16) +
                                    (scr[k].sy - (s32)dx));
                rgba = (r << 24) | (g << 16) | (b << 8) | D_800C5A58;
                gSPModifyVertex(gfx++, 0, G_MWO_POINT_RGBA, rgba);
                gSPModifyVertex(gfx++, 1, G_MWO_POINT_RGBA, rgba);
                if (j != 0 && scr[k - 1].valid == 1) {
                  gDPPipeSync(gfx++);
                  if (scr[k].depth < scr[k - 1].depth) {
                    gDPSetPrimDepth(gfx++, scr[k].depth, 0);
                  } else {
                    gDPSetPrimDepth(gfx++, scr[k - 1].depth, 0);
                  }
                  gDPPipeSync(gfx++);
                  gSP2Triangles(gfx++, 2, 1, 0, 0, 3, 2, 0, 0);
                }
              } else {
                gSPModifyVertex(gfx++, 2, G_MWO_POINT_XYSCREEN,
                                (scr[k].sx << 16) + scr[k].sy);
                gSPModifyVertex(gfx++, 3, G_MWO_POINT_XYSCREEN,
                                ((scr[k].sx + (s32)dy) << 16) +
                                    (scr[k].sy - (s32)dx));
                rgba = (r << 24) | (g << 16) | (b << 8) | D_800C5A58;
                gSPModifyVertex(gfx++, 2, G_MWO_POINT_RGBA, rgba);
                gSPModifyVertex(gfx++, 3, G_MWO_POINT_RGBA, rgba);
                if (scr[k - 1].valid == 1) {
                  gDPPipeSync(gfx++);
                  if (scr[k].depth < scr[k - 1].depth) {
                    gDPSetPrimDepth(gfx++, scr[k].depth, 0);
                  } else {
                    gDPSetPrimDepth(gfx++, scr[k - 1].depth, 0);
                  }
                  gDPPipeSync(gfx++);
                  gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
                }
              }
            }
          }
          j++;
          k++;
          if (j != cols) {
            goto emit_row;
          }
        }
        if (rowsv[0] == 0x20) {
          if (i < 0x1E) {
            base += cols * 2;
            i += 2;
          } else {
            base += cols;
            i++;
          }
        } else {
          base += cols;
          i++;
        }
      } while (i != rowsv[0]);
    }

    i = 0;
    n = rowsv[0] - 1;
    if (cols != 0) {
      do {
        j = 0;
        if (n != 0) {
          k = i;
        emit_col:
          if (scr[k].valid == 1 && scr[k + 8].valid == 1) {
            dx = (f32)(scr[k].sx - scr[k + 8].sx);
            dy = (f32)(scr[k].sy - scr[k + 8].sy);
            if (((dx == fzero) & (dy == fzero)) == 0) {
              scale = 4.5f / sqrtf(dx * dx + dy * dy);
              if (D_801B725C[active_player_idx * 0xB8] == 0) {
                sh = shade[k];
                if (sh == 0) {
                  b = 0xFF;
                  g = 0xFF;
                  r = 0xFF;
                } else if (sh > 0) {
                  r = 0xFF;
                  b = r - sh;
                  g = b;
                } else {
                  b = 0xFF;
                  g = sh + 0xFF;
                  r = g;
                }
              } else {
                sh = shade[k];
                b = 0;
                if (sh == 0) {
                  g = 0;
                  r = 0;
                } else if (sh > 0) {
                  r = sh;
                  g = 0;
                } else {
                  b = -sh;
                  g = 0;
                  r = 0;
                }
              }
              rgba = (r << 24) | (g << 16) | (b << 8) | D_800C5A58;
              gSPModifyVertex(gfx++, 0, G_MWO_POINT_RGBA, rgba);
              gSPModifyVertex(gfx++, 1, G_MWO_POINT_RGBA, rgba);
              if (D_801B725C[active_player_idx * 0xB8] == 0) {
                sh = shade[k + 8];
                if (sh == 0) {
                  b = 0xFF;
                  g = 0xFF;
                  r = 0xFF;
                } else if (sh > 0) {
                  r = 0xFF;
                  b = r - sh;
                  g = b;
                } else {
                  b = 0xFF;
                  g = sh + 0xFF;
                  r = g;
                }
              } else {
                sh = shade[k + 8];
                b = 0;
                if (sh == 0) {
                  g = 0;
                  r = 0;
                } else if (sh > 0) {
                  r = sh;
                  g = 0;
                } else {
                  b = -sh;
                  g = 0;
                  r = 0;
                }
              }
              rgba = (r << 24) | (g << 16) | (b << 8) | D_800C5A58;
              gSPModifyVertex(gfx++, 2, G_MWO_POINT_RGBA, rgba);
              gSPModifyVertex(gfx++, 3, G_MWO_POINT_RGBA, rgba);
              gSPModifyVertex(gfx++, 0, G_MWO_POINT_XYSCREEN,
                              (scr[k].sx << 16) + scr[k].sy);
              gSPModifyVertex(gfx++, 1, G_MWO_POINT_XYSCREEN,
                              ((scr[k].sx - (s32)(dy * scale)) << 16) +
                                  scr[k].sy + (s32)(dx * scale));
              gSPModifyVertex(gfx++, 2, G_MWO_POINT_XYSCREEN,
                              (scr[k + 8].sx << 16) + scr[k + 8].sy);
              gSPModifyVertex(gfx++, 3, G_MWO_POINT_XYSCREEN,
                              ((scr[k + 8].sx - (s32)(dy * scale)) << 16) +
                                  scr[k + 8].sy + (s32)(dx * scale));
              if (scr[k].depth < scr[k + 8].depth) {
                gDPSetPrimDepth(gfx++, scr[k].depth, 0);
              } else {
                gDPSetPrimDepth(gfx++, scr[k + 8].depth, 0);
              }
              gDPPipeSync(gfx++);
              gSP2Triangles(gfx++, 1, 0, 2, 0, 1, 2, 3, 0);
              gDPPipeSync(gfx++);
            }
          }
          j++;
          k += cols;
          if (j != n) {
            goto emit_col;
          }
        }
        i++;
      } while (i != cols);
    }
  }

  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PIXEL);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&proj[7]),
            G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPPopMatrix(gfx++, G_MTX_MODELVIEW);

  *gfxp = gfx;
}
```
