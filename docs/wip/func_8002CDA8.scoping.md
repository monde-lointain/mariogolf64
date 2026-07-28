# `func_8002CDA8` reconstruction scoping (S303)

**This is not a wall note.** The leaf was never attempted: it is `fresh` / `standalone` on every
ranker tell and this file only records what its `.s` decodes to, so the next attempt starts from a
resolved display list instead of 838 raw instructions. Deliberately *not* named
`.near-match.md`, because `pick_target.py --carried-check` reads that suffix and would report a wall
that does not exist.

`src/main/func_8002A640.c`, `0xD18` / **838 instructions**, 5 `jal` (all `osVirtualToPhysical`), **0
FP**, frame `-0x1C8`, 271 stores, 6 branches.

## What the function is

A render-to-texture pass. It points the RDP's colour image at a 32-pixel-wide off-screen buffer and
replays the live frame buffer (`nuGfxCfb_ptr`) into it as a **4-column x 32-row grid of tiles**, each
tile loaded with a `gDPLoadTextureTile`-shaped seven-packet block and drawn with one texture
rectangle whose extent shrinks with the row index. It then draws several fixed sprites from
`D_800B6B28` / `D_800B6F28` / `D_800FC89C` / `D_80132CEC` into two further off-screen targets
(`D_801B8BC0`, `D_801B60E0`), and finally restores the colour image to the real frame buffer, the
320x240-minus-8 scissor, `G_CYC_1CYCLE` and the colour dither.

The global `D_800B7730` is incremented by one at the top and its value is used as a tile coordinate
later in the body, so it is a frame/phase counter driving the effect.

Signature: `void func_8002CDA8(Gfx** gfxp)` -- `$a0` is stored to `0x14($sp)`, `lw $fp, 0($s4)`
loads `*gfxp` into the walking pointer, and the final packet chain writes the advanced pointer back.

## Method (reusable)

Three scratch decoders were written this sprint and are kept beside this note:

- `docs/wip/dl_order_decode.py <fn.s>` -- walks the `.s`, constant-propagates registers **and**
  `$sp` spill slots, models the DL write pointer symbolically (`DL+n`), and prints the store stream
  **re-sorted into display-list order**. Without the re-sort the stream is unreadable: this emitter
  precomputes ~40 packet pointers into registers and stack slots, so program order and DL order
  diverge by up to 40 packets.
- `docs/wip/rdp_word_decode.py <w0>/<w1> ...` -- decodes SetTile / SetTileSize / LoadTile /
  LoadBlock / SetTextureImage / SetColorImage / TextureRectangle / RDPHALF / SetScissor /
  SetPrimColor words into their fields.
- `docs/wip/combine_word_decode.py <w0>/<w1> ...` -- decodes a SETCOMBINE pair into
  `gDPSetCombineLERP` arguments (reads the `G_CCMUX_*` / `G_ACMUX_*` tables straight out of
  `include/libultra/PR/gbi.h`).

A fourth artefact, a host harness that emits candidate macros and prints their words
(`gcc -m32 -DF3DEX_GBI_2 -D_LANGUAGE_C -I include -I include/libultra -I include/libultra/PR`, with
local `_SHIFTL`/`_SHIFTR` definitions and `G_ON`/`G_OFF` from `mbi.h`), resolved the
`gDPSetOtherMode_H` family by brute force. Promoting these three scripts to `tools/` is a recorded
suggestion for the review gate.

## Resolved preamble (22 packets, DL order)

| words | macro |
| --- | --- |
| `E7000000 / 0` | `gDPPipeSync` (x2) |
| `E3000A01 / 0` | `gDPSetCycleType(G_CYC_1CYCLE)` |
| `D7000002 / 80008000` | `gSPTexture(0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON)` |
| `E3001801 / 000000C0` | `gDPSetColorDither(G_CD_DISABLE)` |
| `E3000C00 / 0` | `gDPSetTexturePersp(G_TP_NONE)` |
| `E3001201 / 00002000` | `gDPSetTextureFilter(G_TF_BILERP)` |
| `E2001E01 / 0` | `gDPSetAlphaCompare(G_AC_NONE)` |
| `E3001001 / 0` | `gDPSetTextureLUT(G_TT_NONE)` |
| `E200001C / 0F0A4000` | `gDPSetRenderMode(G_RM_OPA_SURF, G_RM_OPA_SURF2)` |
| `FC157E2A / 33FDFCFE` | `gDPSetCombineLERP(TEXEL0, PRIMITIVE, PRIMITIVE_ALPHA, PRIMITIVE, 0,0,0, PRIM_LOD_FRAC, ...same cycle 2)` |
| `FA000000 / FFFFFFC8` | `gDPSetPrimColor(0, 0, 255, 255, 255, 200)` |
| `ED000000 / 005003C0` | `gDPSetScissor(G_SC_NON_INTERLACE, 0, 0, 320, 240)` |
| `FF10001F / phys&~7` | `gDPSetColorImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 32, osVirtualToPhysical(D_80105320) & ~7)` |

with a `gDPPipeSync` between most of them (the stream is sync, cmd, sync, cmd ...).

## The grid loop

```
for (i = 0; i != 0x20; i++)          /* $t5, outer; 32 rows            */
  for (j = 0; j != 4; j++)           /* $a3, inner; 4 columns          */
```

Per-iteration accumulators, all `addiu`-stepped at the loop bottoms:

| reg | init | step | role |
| --- | --- | --- | --- |
| `$t1` | `0x88` | `+0x130` per `j` | LoadTile `uls` (10.2: 34 + 76 j) |
| `$t2` | `0xA4` | `+0x130` per `j` | LoadTile `lrs` (41 + 76 j) |
| `$s2` | `0xC0` | `+0xC` per `i` | LoadTile `ult` (48 + 3 i) |
| `$s3` | `0xCC` | `+0xC` per `i` | LoadTile `lrt` (51 + 3 i) |
| `$t0` | `0x440` | `+0x980` per `j` | rectangle S base |
| `$t6` | `0x600` | `+0x60` per `i` | rectangle T base |
| `$t3` | `0x20` | `+0x20` per `j` | rectangle X extent |
| `$t4` | `0x200000` | `+0x200000` per `j` | sign probe for the S clamp |

The body emits, in DL order:

1. `gDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 320, cfb_phys & ~7)` (`FD10013F`)
2. `gDPSetTile(RGBA, 16b, line = 0x17 >> 3, tmem 0, G_TX_LOADTILE, ...)` (`F5100000 | line << 9`)
3. `gDPLoadSync`
4. `gDPLoadTile(G_TX_LOADTILE, uls, ult, lrs, lrt)` from the four accumulators
5. `gDPPipeSync`
6. `gDPSetTile(...)` again, w1 = 0
7. `gDPSetTileSize(tile 0, ult, lrs, lrt)`
8. `gSPTextureRectangle`-family packet (`E4......`) + `G_RDPHALF_1` + `G_RDPHALF_2 (0x04000400)`

Packets 1-7 are exactly the `gDPLoadTextureTile` expansion, and the loop advances the write pointer
by `0x40` (eight packets) in one `addiu`, so the reconstruction should reach for the composite SDK
macro first rather than spelling seven packets by hand (see the memory
`sdk-composite-macro-before-dl-reconstruction`).

Coordinate clamps in the body are the standard `MAX(x, 0)` idiom
(`sll 18 / sra 16 / nor / sra 31 / and`) that `gSPScisTextureRectangle` expands to, applied to
`(0x20 - i) * 4` and `(0x1F - i) * 4`; the two `bgez` branches near `8002D234` / `8002D25C` select
between `$t0` / `$t6` and a `subu`-corrected value for the RDPHALF_1 `s` and `t`.

## What is left to do

1. Decode the second half's sprite draws (offsets `+256` to `+804` of the DL stream, all constant
   words already dumped and in this note's method output) into macros -- mechanical.
2. Pin the rectangle arithmetic (`$t0`/`$t6` bases and the two `bgez` arms) to a source expression.
3. Write the C, then iterate with `tools/cmpfn.sh func_8002CDA8 build/src/main/func_8002A640.o`.

Host risk to check before integrating (S300): `src/main/func_8002A640.c` is a partial file, so any
`.rodata` this body would emit must be checked against the host's carve first. This body has zero FP
constants and no strings, so the expected emission is empty `.rodata`, but confirm with
`objdump -s -j .rodata` on the object.
