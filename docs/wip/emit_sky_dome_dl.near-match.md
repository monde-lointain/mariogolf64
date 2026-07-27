# emit_sky_dome_dl — S296 carry (272/299, unfinished reconstruction)

**This is not a wall.** No compiler-source dive was run and no permuter was imported. The leaf was
committed third in S296, the first two took the sprint, and this one got one build. It reached
272/299 with the structure matching in three of the four diff hunks on that build. Re-open it on
size order like any fresh leaf; do not price it as a documented wall.

`src/main/func_8002A640.c`. Host has two already-banked emitters as shape references:
`copy_previous_frame_to_cfb` (S294) and `emit_sky_horizon_compositor_dl` (S295), so the S286
same-file-sibling exception applies.

Attempt source: `nonmatchings/emit_sky_dome_dl/attempt.c` (the S296 first build, verbatim).

## What it draws

Three passes of horizontal strips forming the sky dome. Each strip is a quad textured from the
matching band of `D_800B67A0` — the same image `emit_sky_horizon_compositor_dl` composites, at the
same `320 * 2 * 6` stride, but starting at texel row 8 rather than 0.

Preamble is two matrices: the shared projection `D_800FE2F0`
(`G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH`) and the per-frame modelview
`D_800DABB0[D_800B7780]` (`G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH`).

Each pass opens with `gSPVertex` from `D_800DABA0 + pass * 512 + D_800B7780 * 16`, 18 vertices on
pass 2 and 32 otherwise. The address expression is identical in both arms and only the count differs,
which is why gcc hoists the pass-2 form out of the outer loop (its `pass * 512` folds to `0x400`).

## Per strip

`rows = ((pass == 2) && (strip == 7)) ? 2 : 6`, emitted branchlessly
(`sltiu`/`negu`/`andi 6`/`ori 2`).

A flag (`s5`, `tiled` in the attempt) makes the **very first strip of the whole dome** emit the full
seven-command `gDPLoadTextureBlock`, and every later strip emit only the four that re-point and
re-fetch: `gDPSetTextureImage` width 1, `gDPLoadSync`, `gDPLoadBlock`, `gDPPipeSync`. The tile
descriptors are declared once for the dome, not per strip.

Then a **second** bind of the same band as a 320-wide tile: `gDPSetTextureImage` 320,
`gDPSetTile` line 76 `G_TX_LOADTILE`, `gDPLoadSync`, `gDPLoadTile` (`uls = 8 << 2`, `ult = 0`,
`lrs = 311 << 2`, `lrt = (rows - 1) << 2`), `gDPPipeSync`, `gDPSetTile` `G_TX_RENDERTILE`,
`gDPSetTileSize` (same rect).

Geometry: four `gSPModifyVertex` writing `G_MWO_POINT_ST` for vertices `2s`, `2s+1`, `2s+2`, `2s+3`
with ST `(8,0) (312,0) (8,rows) (312,rows)` — as the words `0x01000000`, `0x27000000`,
`rows * 32 + 0x01000000`, `rows * 32 + 0x27000000`. Then one
`gSP2Triangles(2s, 2s+2, 2s+1, 0, 2s+1, 2s+2, 2s+3, 0)` and a closing `gDPPipeSync`, after which the
pass-2 strip-7 case breaks out of the strip loop.

## Constants already decoded (do not re-derive)

- `0x0700001A` in the `gDPLoadBlock` word is `tile 7 | dxt 0x1A`, and `0x1A` is
  `CALC_DXT(320, G_IM_SIZ_16b_BYTES)`.
- The `slti 0x800` / `0x7FF` clamp in the `.s` is **not** source. It is
  `MIN(lrs, G_TX_LDBLK_MAX_TXL)` inside `gDPLoadBlock` itself — the same clamp both S296 banks
  reproduced for free.
- `D_800DABB0 == D_800DABA0 + 0x10`; both are referenced with their own `%hi`/`%lo`, so they stay two
  externs, not one symbol.

## Where the 27 instructions are missing

Entirely in the last hunk: ROM rows 246-299 are 54 instructions, the build's are 27. That is the
`gSPModifyVertex` / `gSP2Triangles` word-building block. The ROM keeps the doubled vertex indices as
three separate induction variables (`t6 = 2 + 4s`, `t7 = 4 + 4s`, `t8 = 6 + 4s`) and masks each with
`andi ..., 0xFFFF` per command; the build folds them into the strip counter instead. Start there.

Two smaller tells for the retry, both in the same region:

- The ROM's outer back edge is a branch-**likely** (`bnel t9, 3` with the next comparison constant in
  its delay slot) where the build emits a plain `bne`.
- The ROM spills two loop constants to the stack (`sw t0, N(sp)`, reloaded per use) where the build
  hoists them into callee-saved registers. The build sets up noticeably more `lui` in the prologue.
