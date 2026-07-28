# func_8007EF0C — near-match (S305)

`src/main/func_80078910.c`, 0xF38 / **974 instructions**. Reconstructed from scratch this sprint.

## State

**968 / 974 instructions, frame `-0x160` exact (the ROM's), every structural feature reproduced.**
The whole residual is ONE scheduling placement, repeated in the two texture-rectangle loops:

- ROM order in the block after the size clamp: `E4 corner words` -> `(sprite / 2) << 9` ->
  `(s16)(sx - size)` -> `div $zero, 0x8000, size`.
- This build: `E4 corner words` -> `div` -> `(sprite / 2) << 9` -> `(s16)(sx - size)`.

The 11 instructions of the two chains sit on the other side of the divide. Cost: the ROM's
`mflo` then has nothing independent to fill its two hazard slots, so the ROM carries **2 nops per
loop that this build fills** (4 total), plus one extra spill `lw`/`sw` pair. 968 + 4 + 2 = 974.

## Proven, do not re-try

- **Source statement order does not move the divide.** Three source forms produce a
  byte-identical object: (a) `step = 0x8000 / size;` as a statement before
  `gSPScisTextureRectangle`; (b) the whole macro hand-expanded with the divide written between the
  `E4` words and the `RDPHALF_1` word; (c) the same hand-expansion with `t`/`xl` temps written in
  the ROM's exact order between the `RDPHALF_1` header store and its data word. All three: 968,
  same segment split. So this is not an `INSN_LUID` tie-break — the divide's position is fixed
  before scheduling.
- **Inline `0x8000 / size` in the macro arguments is wrong**: gcc 2.7.2 emits one divide per
  basic block the value is used in (4 in the first loop, 5 total), +38 instructions. The ROM has
  exactly one divide per loop, so `dsdx`/`dtdy` are one variable.
- `t = (seg->sprite / 2) << 9;` hoisted to a statement BEFORE the macro moves the divide earlier
  still (segment shrinks 50 -> 31), not later.
- Declaration order of `step` among the locals: no effect.

## Fixed this sprint (keep these)

1. `step = 0x8000 / size;` as one variable (see above) — the single largest fix, -44 instructions.
2. `if (((dx == 0.0f) & (dy == 0.0f)) == 0)` and not `if (!(...))`: the negated form applies
   De Morgan and emits `bc1f`/`or`/`beqz` where the ROM has `bc1t`/`and`/`bnez`.
3. `gSPScisTextureRectangle` is the right macro: its `MAX((s16)x,0)` corner clamps are the ROM's
   `nor`/`sra 31`/`and`/`andi 0xFFF`, and its s/t adjustment is the ROM's two-level
   `(s16)(xl) < 0` / `(s16)(dsdx) < 0` MIN-MAX pair verbatim.
4. Words settled on a host `gbi.h` harness: combine `FCFF97FF/FF2DFEFF` =
   `gDPSetCombineLERP(0,0,0,PRIMITIVE, TEXEL0,0,PRIMITIVE,0, ...)` (no stock `G_CC_` pair);
   `FCFFFFFF/FFFE793C` = `gDPSetCombineMode(G_CC_SHADE, G_CC_SHADE)`; rendermode `0x00504B50` =
   `G_RM_ZB_CLD_SURF/2`, `0x00504340` = `G_RM_CLD_SURF/2`; the texture is
   `gDPLoadTextureBlock_4b(timg, G_IM_FMT_I, 32, 64, 0, 0, 0, 6, 6, 0, 0)`.
5. The trailing matrix argument is `OS_K0_TO_PHYSICAL(&proj[7])` (`arg1 + 0x800001C0`), same as
   `emit_ball_trail_dl`.

## Next lever to try

The class is the `sched-class-tiebreak-order-coin` (`rank_for_schedule`, class-then-LUID). Since
source order provably does not move it, the next probe is the dependence graph: give the divide's
divisor a longer producer chain, or change what `size` is (it arrives from a `bc1tl`-annulled
`addiu`), so the divide is not ready when the scheduler first looks for it. A compiler-source
fan-out on `sched.c`'s ready-list order for a `div` with no in-block consumer is the honest next
step — every in-block consumer of the divide lives in a LATER block, so its priority should be 0,
the same as the two chains it is racing.

## The body

Insert in place of the `INCLUDE_ASM` line, plus the `TerrainScatterPoint` field split
(`pad_0C[0xA]; s8 unk_16; u8 pad_17; u8 unk_18;`).

```c
/* One recorded shot segment: the two world points it spans, their projected
 * screen positions, and the sprite the marker end is drawn with. */
typedef struct {
  /* 0x00 */ f32 tip_x;
  /* 0x04 */ f32 tip_y;
  /* 0x08 */ s32 tip_z;
  /* 0x0C */ f32 x;
  /* 0x10 */ f32 y;
  /* 0x14 */ s32 z;
  /* 0x18 */ u8 pad_18[0x4];
  /* 0x1C */ f32 scale;
  /* 0x20 */ s16 tip_sx;
  /* 0x22 */ s16 tip_sy;
  /* 0x24 */ s16 sx;
  /* 0x26 */ s16 sy;
  /* 0x28 */ s16 depth;
  /* 0x2A */ u16 flags;
  /* 0x2C */ s8 sprite;
  /* 0x2D */ u8 unk_2D;
  /* 0x2E */ u8 pad_2E[0x2];
} ShotTraceSegment; /* size 0x30 */

extern s32 putter_mode_flag;
extern s32 D_801B5534;
extern s32 D_800C5400;
extern s32 D_800C5404;
extern void func_8007E980(void);
extern s32 func_80065898(f32, f32, s32, s32*);
extern Vtx D_800C5408[];
extern Mtx D_800E1D60[];
extern Mtx D_E1D60[];
extern Mtx D_1B5638[];

/**
 * Emits the putting-mode shot trace: a z-sorted ribbon through the recorded
 * shot segments, with a scaled sprite at each segment end and at each terrain
 * scatter point.
 *
 * Runs only in putter mode with the overlay loaded and the view not in state 2
 * or 8. Every segment is projected first (both ends, unless the segment is
 * flagged as already having one); a projection that lands too close to the
 * camera marks the segment dead in `flags` and drops it from both later passes.
 *
 * The ribbon is drawn with the same gSPModifyVertex trick the ball trail uses:
 * four vertices re-pointed per segment, offset along the segment normal, with
 * depth supplied by G_ZS_PRIM. The sprites are scissored texture rectangles out
 * of one 32x64 4-bit texture, sized by the projected scale and clamped to
 * 4..16 (1..16 for the scatter points).
 */
void func_8007EF0C(Gfx** gfxp, Mtx* proj) {
  s32 screen[6];
  s32 point[6];
  Gfx* gfx;
  ShotTraceSegment* seg;
  TerrainScatterPoint* p;
  s32 i;
  s32 off;
  s32 size;
  s32 step;
  f32 dx;
  f32 dy;
  f32 scale;

  gfx = *gfxp;

  if (putter_mode_flag != 1) {
    return;
  }
  if (D_800C5448 != 2) {
    return;
  }
  if ((D_801B5534 == 2) | (D_801B5534 == 8)) {
    return;
  }

  func_8007E980();

  i = 0;
  off = 0;
  if (D_800C5400 != 0) {
    do {
      seg = (ShotTraceSegment*)((u8*)D_801EFFA8 + off);
      if ((f32)project_point_to_screen(seg->x, seg->y, seg->z, screen) <
          -15.0f) {
        seg->sx = screen[0] << 2;
        seg->sy = screen[1] << 2;
        seg->flags &= 0xFFFE;
        seg->depth = screen[2];
        seg->scale = *(f32*)&screen[3];
        if ((seg->flags & 0x10) == 0) {
          if ((f32)func_80065898(seg->tip_x, seg->tip_y, seg->tip_z, screen) <
              -15.0f) {
            seg->tip_sx = screen[0] << 2;
            seg->tip_sy = screen[1] << 2;
          } else {
            seg->flags |= 1;
          }
        }
      } else {
        seg->flags |= 1;
      }
      i++;
      off += 0x30;
    } while (i != D_800C5400);
  }

  guMtxIdent(D_800E1D60);

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
  gDPSetDepthSource(gfx++, G_ZS_PRIM);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_ZB_CLD_SURF, G_RM_ZB_CLD_SURF2);
  gDPPipeSync(gfx++);
  gSPMatrix(gfx++, D_1B5638, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPMatrix(gfx++, D_E1D60, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH);
  gSPVertex(gfx++, D_800C5408, 4, 0);

  i = 0;
  off = 0;
  if (D_800C5400 != 0) {
    do {
      seg = (ShotTraceSegment*)((u8*)D_801EFFA8 + off);
      if (seg->flags == 0) {
        dx = (f32)(seg->tip_sx - seg->sx);
        dy = (f32)(seg->tip_sy - seg->sy);
        if (((dx == 0.0f) & (dy == 0.0f)) == 0) {
          scale = 5.0f / sqrtf(dx * dx + dy * dy);

          gDPPipeSync(gfx++);
          gDPSetPrimDepth(gfx++, seg->depth, 0);
          gDPPipeSync(gfx++);

          dy = dy * scale;
          dx = dx * scale;
          gSPModifyVertex(gfx++, 0, G_MWO_POINT_XYSCREEN,
                          ((seg->tip_sx + (s32)dy) << 16) + seg->tip_sy);
          gSPModifyVertex(gfx++, 1, G_MWO_POINT_XYSCREEN,
                          (seg->tip_sx << 16) + (seg->tip_sy + (s32)dx));
          gSPModifyVertex(gfx++, 2, G_MWO_POINT_XYSCREEN,
                          ((seg->sx + (s32)dy) << 16) + seg->sy);
          gSPModifyVertex(gfx++, 3, G_MWO_POINT_XYSCREEN,
                          (seg->sx << 16) + (seg->sy + (s32)dx));
          gSP2Triangles(gfx++, 1, 0, 2, 0, 1, 2, 3, 0);
        }
      }
      i++;
      off += 0x30;
    } while (i != D_800C5400);
  }

  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0,
                    PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
  gDPSetBlendColor(gfx++, 255, 255, 255, 1);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPLoadTextureBlock_4b(gfx++, ((u32)D_800FF4CC + 8) & ~7, G_IM_FMT_I, 32, 64,
                         0, G_TX_NOMIRROR | G_TX_WRAP,
                         G_TX_NOMIRROR | G_TX_WRAP, 6, 6, G_TX_NOLOD,
                         G_TX_NOLOD);
  gDPSetPrimColor(gfx++, 0, 0, 191, 191, 191, 159);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);

  i = 0;
  off = 0;
  if (D_800C5400 != 0) {
    do {
      seg = (ShotTraceSegment*)((u8*)D_801EFFA8 + off);
      if (seg->sprite != -1 && (seg->flags & 1) == 0) {
        gDPPipeSync(gfx++);
        gDPSetPrimDepth(gfx++, seg->depth, 0);
        gDPPipeSync(gfx++);
        size = (s32)(seg->scale * 8.0f);
        if ((f32)size >= 4.0f) {
          if ((f32)size > 16.0f) {
            size = 16;
          }
          step = 0x8000 / size;
          gSPScisTextureRectangle(
              gfx++, seg->sx - size, seg->sy - size, seg->sx + size,
              seg->sy + size, G_TX_RENDERTILE, seg->unk_2D << 9,
              (seg->sprite / 2) << 9, step, step);
        }
      }
      i++;
      off += 0x30;
    } while (i != D_800C5400);
  }

  i = 0;
  off = 0;
  if (D_800C5404 != 0) {
    do {
      p = (TerrainScatterPoint*)((u8*)D_801B8BB8 + off);
      if ((f32)func_80065898(p->unk_00, p->unk_04, *(s32*)&p->unk_08, point) <
          -7.5f) {
        size = (s32)(*(f32*)&point[3] * 8.0f);
        if ((f32)size >= 1.0f) {
          if ((f32)size > 16.0f) {
            size = 16;
          }
          step = 0x8000 / size;
          gSPScisTextureRectangle(
              gfx++, (point[0] << 2) - size, (point[1] << 2) - size,
              (point[0] << 2) + size, (point[1] << 2) + size, G_TX_RENDERTILE,
              p->unk_18 << 9, (p->unk_16 / 2) << 9, step, step);
        }
      }
      i++;
      off += 0x1C;
    } while (i != D_800C5404);
  }

  gDPPipeSync(gfx++);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&proj[7]),
            G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
  gDPPipeSync(gfx++);
  gDPSetDepthSource(gfx++, G_ZS_PIXEL);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);

  *gfxp = gfx;
}

```
