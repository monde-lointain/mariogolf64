# `func_8002CDA8` reconstruction scoping (S303, re-opened S304)

**This is not a wall note.** The body is reconstructed and sits at the ROM's exact instruction
count; only the register allocation differs. `pick_target.py --carried-check` nonetheless reports
`CARRIED-WALL` for this leaf: `carried_wall_names()` unions the BACKLOG carry names with every
`docs/wip/<fn>.*.md` **by stem**, so the `.scoping.md` suffix changes nothing (the review gate
corrected an earlier claim here that it would). Read this file before pricing the leaf; a
wall-vs-scoped state in the detector is a tracked ranker follow-up.

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

Three decoders were written this sprint and promoted to `tools/` at the S303 review:

- `venv/bin/python3 tools/dl_decode.py <fn.s>` -- walks the `.s`, constant-propagates registers **and**
  `$sp` spill slots, models the DL write pointer symbolically (`DL+n`), and prints the store stream
  **re-sorted into display-list order**. Without the re-sort the stream is unreadable: this emitter
  precomputes ~40 packet pointers into registers and stack slots, so program order and DL order
  diverge by up to 40 packets.
- `venv/bin/python3 tools/rdp_word.py <w0>/<w1> ...` -- decodes SetTile / SetTileSize / LoadTile /
  LoadBlock / SetTextureImage / SetColorImage / TextureRectangle / RDPHALF / SetScissor /
  SetPrimColor words into their fields.
- `venv/bin/python3 tools/combine_word.py <w0>/<w1> ...` -- decodes a SETCOMBINE pair into
  `gDPSetCombineLERP` arguments (reads the `G_CCMUX_*` / `G_ACMUX_*` tables straight out of
  `include/libultra/PR/gbi.h`).

A fourth artefact, a host harness that emits candidate macros and prints their words
(`gcc -m32 -DF3DEX_GBI_2 -D_LANGUAGE_C -I include -I include/libultra -I include/libultra/PR`, with
local `_SHIFTL`/`_SHIFTR` definitions and `G_ON`/`G_OFF` from `mbi.h`), resolved the
`gDPSetOtherMode_H` family by brute force; the recipe and its three gotchas are now the
`tools/dl_decode.py` row of `docs/workflow/loop.md ## Oracles`.

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

## Reconstruction result (S303): 838/838 on the first build, frame `+0x18`

The whole body was written from the decoded stream and compiled to **838 instructions against the
ROM's 838 on the first build**, every display-list word verified against `gbi.h` through the
harness. The residual is **entirely allocation**: frame `-0x1E0` against the ROM's `-0x1C8` and a
1478-row register permutation. `cmpfn` reports no structural deficit anywhere.

The frame delta is exactly three spill slots. Both sides use the same 50-entry, 8-stride constant
spill area at `20..412(sp)`; this build adds three more at `420`, `428`, `436` before the
callee-saved block, and they hold display-list *pointers* (`sw s8,436(sp)`) and two packet constants
(`FD10013F`, `07000000`) that the ROM keeps in registers. So the gap is three simultaneously-live
values in the straight-line sections, not a body difference.

Levers measured this sprint (instruction count / frame / cmpfn rows):

| form | result |
| --- | --- |
| one shared `n = D_800B7730;` temp, re-assigned before each of the two animated `gDPSetTileSize` sites | **838 / -0x1E0 / 1478** (best) |
| both sites reading `D_800B7730` directly | 835 / -0x1C0 / 1497 |
| two distinct temps `n`, `m` | 835 / -0x1C0 / 1497 |
| one temp + one direct, either way round | 835 / -0x1C0 |
| `D_800B7730++` moved above the preamble | 840 / -0x1E8 |
| `src = osVirtualToPhysical(nuGfxCfb_ptr) & ~7` moved down to just before the grid loop | 836 / -0x1D8 |
| explicit `uls`/`ult`/`lrs`/`lrt` block-scoped temps in the loops | identical to the inline form (838 / -0x1E0 / 1478) -- gcc folds them |

So the count is a knife edge on how `D_800B7730` is spelled: one variable re-assigned at both sites
is the only form that reaches 838, which is the `one-variable-reuse-reorders-loads` lever in its
*wanted* direction.

## S304 re-open: the residual is one two-pseudo allocation swap, and it is named

S303's suspects (item 2 below, the tail texrect constants) were wrong, and its "no structural
deficit anywhere" reading was `cmpfn` normalisation. The mnemonic histogram of a fresh object
against the `.s` is the measurement that finds it:

```
grep -oE '  [a-z0-9.]+ ' <fn>.s | tr -d ' ' | sed 's/^addu$/move/' | sort | uniq -c   # ROM
grep -oE $'\t[a-z0-9.]+\t' mine.txt | tr -d $'\t' | sed 's/^li$/addiu/' | sort | uniq -c
```

On the S303 body that prints `bgez 2/1, bgezl 0/1, j 2/1, sw 271/272` -- a branch-FORM divergence
in the grid loop that `cmpfn` never showed, not a pure permutation.

**Two levers moved it, both retained in the body below.**

| lever | result |
| --- | --- |
| S303 body (increment after the last preamble `gDPPipeSync`) | 838 / `-0x1E0` / 1557 rows / `sw` +1 |
| **P3**: `D_800B7730 = D_800B7730 + 1;` moved up to just after `gDPSetPrimColor(..., 200)` | 838 / `-0x1D8` / 1576 / `sw`,`lw` exact |
| **Q3**: `yl` as an `s32` assigned at the i-loop head, passed as the texrect's `yl` | **838 / `-0x1D8` / 1541 / only `move` +1 + the branch trio** |

Placement of the increment is a real axis, not a coin: P1/P2 (anywhere above `gDPSetRenderMode`)
give 840 / `-0x1E8`; P3 and P4 (`gDPSetPrimColor` or `gDPSetScissor`) both give the `-0x1D8` frame
and exact `sw`/`lw`. The ROM loads `D_800B7730` at instruction 57 and stores it at 147, ~90 apart,
so the load is scheduled far ahead of the add; only a mid-preamble source position reproduces that.

**The remaining residual is a single pair of hoisted pseudos.** With the body below,
`tools/allocno_report.py` prints them adjacent and 9 apart:

| pseudo | expression | refs | live_length | priority | this build | ROM |
| --- | --- | --- | --- | --- | --- | --- |
| 356 | `t & 0xFFFF`, i.e. `_SHIFTL((48 + i*3) << 5, 0, 16)` | 5 | 107 | 934 | `$s1` | spilled to `0x194($sp)` |
| 352 | `yl << 10`, i.e. `(s16)yl * (s16)dtdy` | 5 | 108 | 925 | spilled | `$s5` |

Both are `gSPScisTextureRectangle` internals hoisted to the i-loop preheader by `loop.c`, and their
defs are adjacent (`insn 1969`, `insn 1970` in the `.lreg` dump), which is exactly why the lengths
differ by one. That one swap explains **all three** remaining deltas at once: with `t & 0xFFFF`
spilled, the `yl >= 0` arm becomes `lw` + `or` (two instructions) instead of the single
`or $v0,$a0,$s1`, so `reorg` can no longer fold it into an annulled `bgezl` and the ROM's
`bgez` + `j` pair returns (+1 insn), and the surplus `move` goes with it.

The arithmetic says the fix is small: `priority = floor_log2(refs)*refs/live_length*10000`, so
equalising the two live lengths is enough (a tie falls through to the pseudo number, and 352 < 356).
Everything tried failed to move that one unit:

- `yl` spellings: `(31-i)<<2`, `(31-i)*4`, `124-i*4`, `31-i` with the shift at the call site,
  block-scoped, assigned in the j-loop instead of the i-loop -- all 838 / `-0x1D8` / 1541, identical.
- `t` spellings: hoisted to the i-loop head (835), assigned in the j-loop (838/1569), `1536+i*96`
  (1541), pre-masked `((48+i*3)<<5) & 0xFFFF` (840, adds an `andi`).
- explicit `yh`, explicit `ult`, `s` as a j-loop temp: no change or worse.
- `dtdy` as a variable: catastrophic (862, `mult`/`mflo` appear -- the `(s16)` cast stops folding).
- loop forms: i-loop and j-loop each as `do {} while`, `do {} while (0)` round the `yl` assignment
  or round the `gDPLoadTextureTile` call: no change or worse.
- `do {} while (0)` round the **`gSPScisTextureRectangle` call** does fix the branch (`bgez`/`j`
  match exactly, the `do-while-zero-block-break` lever) -- but it costs a duplicate
  `sw $v1,4($a1)` immediately before the real `sw $v0,4($a1)`, i.e. `lw` 99/101 and `sw` 273/271.
  Net worse, so it is not in the body below. It does confirm the branch is a block-boundary effect.

**Next action:** this is a `global.c` live-length question with a one-unit target, so the next
attempt is a compiler-source fan-out on `loop.c`'s preheader emission order (what fixes the order
of the two adjacent invariant defs at `insn 1969`/`1970`) rather than another source-spelling sweep;
that axis is exhausted. Reproduce the base first: the body below, `tools/allocno_report.py`, and the
histogram command above.

Superseded S303 leads, kept for the record: the tail `E4080080`/`04000400` and `01020020` constants
are **not** the residual, and the frame delta is not three tail spill slots -- at P3 it is two
reload spill slots (`420`, `428`) carrying grid-loop traffic.

Do not re-derive the display list: it is verified.

Host risk when integrating (S300): `src/main/func_8002A640.c` is a partial file. This body emits no
FP constants and no strings, so its `.rodata` should be empty -- confirm with
`objdump -s -j .rodata` on the object before claiming a bank.

## Working body (S304 best: 838/838, frame -0x1D8 against -0x1C8, cmpfn 1541 rows)

```c
extern s32 D_800B7730;
extern u8 D_800B6B28[];
extern u8 D_800B6F28[];
extern u8* D_800FC89C;
extern u8* D_80132CEC;
extern u16 D_80105320[];
extern u16 D_801B8BC0[];
extern u16 D_801B60E0[];

void func_8002CDA8(Gfx** gfxp) {
  Gfx* gfx = *gfxp;
  u32 src = osVirtualToPhysical(nuGfxCfb_ptr) & ~7;
  s32 i;
  s32 j;
  s32 n;
  s32 yl;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_DISABLE);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gDPSetCombineLERP(gfx++, TEXEL0, PRIMITIVE, PRIMITIVE_ALPHA, PRIMITIVE, 0, 0,
                    0, PRIM_LOD_FRAC, TEXEL0, PRIMITIVE, PRIMITIVE_ALPHA,
                    PRIMITIVE, 0, 0, 0, PRIM_LOD_FRAC);
  gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 200);
  D_800B7730 = D_800B7730 + 1;
  gDPPipeSync(gfx++);
  gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, 320, 240);
  gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32,
                   osVirtualToPhysical(D_80105320) & ~7);
  gDPPipeSync(gfx++);

  for (i = 0; i != 32; i++) {
    yl = (31 - i) << 2;
    for (j = 0; j != 4; j++) {
      gDPLoadTextureTile(gfx++, src, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320, 0,
                         34 + j * 76, 48 + i * 3, 41 + j * 76, 51 + i * 3, 0,
                         G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                         G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gSPScisTextureRectangle(gfx++, j << 5, yl, (j + 1) << 5,
                              (32 - i) << 2, G_TX_RENDERTILE, (34 + j * 76) << 5,
                              (48 + i * 3) << 5, 1 << 10, 1 << 10);
    }
  }

  gDPLoadTextureBlock(gfx++, (u32)D_800B6B28 & ~7, G_IM_FMT_I, G_IM_SIZ_8b, 32,
                      32, 0, G_TX_NOMIRROR | G_TX_WRAP,
                      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                      G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0,
                    0, 0, 0, PRIMITIVE);
  gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 159);
  gSPTextureRectangle(gfx++, 0, 0, 32 << 2, 32 << 2, G_TX_RENDERTILE, 0, 0,
                      1 << 10, 1 << 10);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_2CYCLE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
  gDPSetCombineLERP(gfx++, TEXEL0, TEXEL1, PRIMITIVE_ALPHA, TEXEL1, 0, 0, 0,
                    PRIMITIVE, PRIMITIVE, 0, COMBINED, COMBINED_ALPHA, 0, 0, 0,
                    COMBINED);
  gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32,
                   osVirtualToPhysical(D_801B8BC0) & ~7);
  gDPLoadTextureBlock(gfx++, (u32)D_800B6B28 & ~7, G_IM_FMT_I, G_IM_SIZ_8b, 32,
                      32, 0, G_TX_NOMIRROR | G_TX_WRAP,
                      G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
  gDPLoadMultiBlock(gfx++, (u32)D_800B6F28 & ~7, 256, 1, G_IM_FMT_RGBA,
                    G_IM_SIZ_16b, 32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
  gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 48);
  n = D_800B7730;
  gDPSetTileSize(gfx++, 0, 0, n, 32, n + 32);
  gDPSetTileSize(gfx++, 1, 0, 0, 32, 32);
  gSPTextureRectangle(gfx++, 0, 0, 32 << 2, 32 << 2, G_TX_RENDERTILE, 0, 0,
                      1 << 10, 1 << 10);
  gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32,
                   osVirtualToPhysical(D_801B60E0) & ~7);
  gDPLoadTextureBlock(gfx++, ((u32)D_800FC89C + 8) & ~7, G_IM_FMT_RGBA,
                      G_IM_SIZ_16b, 32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP,
                      G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
  gDPLoadMultiBlock(gfx++, ((u32)D_80132CEC + 8) & ~7, 256, 1, G_IM_FMT_RGBA,
                    G_IM_SIZ_16b, 32, 32, 0, G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
  gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 127);
  n = D_800B7730;
  gDPSetTileSize(gfx++, 0, n, n, n + 32, n + 32);
  gDPSetTileSize(gfx++, 1, 0, 0, 32, 32);
  gSPTextureRectangle(gfx++, 0, 0, 32 << 2, 32 << 2, G_TX_RENDERTILE, 0, 0,
                      1 << 10, 1 << 10);
  gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320,
                   osVirtualToPhysical(nuGfxCfb_ptr) & ~7);
  gDPPipeSync(gfx++);
  gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 8, 8, 312, 232);
  gDPPipeSync(gfx++);
  gDPSetColorDither(gfx++, G_CD_MAGICSQ);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  *gfxp = gfx;
}
```

## S304 crack agent: the residual is a one-register slide, not a live-length tie

The S304 re-open above models the residual as `allocno_compare` mis-ordering pseudos 352 and 356,
with 352 spilled by `global.c`. **That model is wrong**, and the `-dg` dump says so directly. Read
this section before spending another sprint on live lengths.

### What the dumps actually show

Compile the seed with `-dg -dl` (`nonmatchings/func_8002CDA8/compile.sh` is now present, so
`venv/bin/python3 tools/allocno_report.py nonmatchings/func_8002CDA8/base.c func_8002CDA8` works
without inlining into `src/`). Three facts:

1. **The three competing pseudos have identical conflict sets.** From `<file>.c.greg`:

   ```
   ;; 313 conflicts: 72 73 74 78 79 81 169 189 190 207 208 254 255 289 297 298 303 313 316 323 352 356 ... 2 3 4 5 29
   ;; 352 conflicts: <byte-identical>
   ;; 356 conflicts: <byte-identical>
   ```

   Same allocno conflicts, same hard-reg conflicts (`{v0, v1, a0, a1, sp}`). Any priority order over
   these three is therefore reachable -- but by the same token, priority order cannot by itself
   produce an asymmetric outcome between them.

2. **`global.c` gives all three a hard register.** In allocation order the seed's
   `Register dispositions` are

   | allocno | expression | this build | ROM |
   | --- | --- | --- | --- |
   | 207 | -- | `$s0` | `$s0` |
   | 637 | LoadTile `ult` | `$s2` | `$s2` |
   | 635 | LoadTile `lrt` | `$s3` | `$s3` |
   | 356 | `t & 0xFFFF` | **`$s1`** | **`$s4`** |
   | 352 | `yl << 10` | `$s4` | `$s5` |
   | 313 | `MAX(yl,0) & 0xFFC` | `$s5` | `$s6` |
   | 297 | -- | `$s6` | `$s7` |
   | 254 | -- | `$s7` | `$t8` |
   | 208 | -- | `$t8` | spilled |

   The ROM's chain is **this chain slid down exactly one register**. Nothing is reordered.

3. **The spill is `reload`'s, not `global`'s.** The `.greg` dump prints

   ```
   Spilling reg 20.
    Register 88 now on stack.
    Register 352 now on stack.
    Register 378 now on stack.
   ```

   `reload1.c:2232` (`new_spill_reg`) commandeers hard reg 20 = `$s4` as a reload register and
   `reload1.c:3503` evicts every pseudo `global.c` had parked there. Both builds spill `$s4`; because
   of the one-register slide, the ROM's evicted pseudo is **356** and ours is **352**.

Everything else in the residual is downstream of that single fact: with 356 in a register the
`yl >= 0` arm is one `or`, `reorg` folds it into an annulled `bgezl`, and the ROM's `bgez` + `j`
pair plus the compensating `move` disappear.

### So the question is: why is `$s1` free here and taken in the ROM?

Note that 637 and 635 (both ranked *above* 356) skip `$s1` and take `$s2`/`$s3`; `find_reg`'s pass 0
(`global.c:951-953`) excludes `regs_someone_prefers[allocno]`, and some later allocno prefers reg 17.
356 then takes `$s1` in pass 0 because that exclusion does not apply to it. In the ROM, reg 17 is
genuinely unavailable to 356. **The lever is therefore one unit of register pressure or one copy
preference in the i-loop preheader region, not a live length anywhere.**

### Proved not to move it

- `allocno_compare` (`global.c:587-604`) arithmetic is *correct as documented* -- refs 5/5,
  live lengths 108/107, priorities 925/934 -- but it is not the deciding pass, so equalising the
  lengths is not the fix. The lengths differ by one only because `move_movables`
  (`loop.c:1551`, `loop.c:1854`) emits the two hoisted invariants adjacently, in loop-body order.
- **The block order is not source-controllable.** `gSPScisTextureRectangle`'s
  `((yl) < 0) ? MIN(...) : 0` puts the `yl << 10` arm on the fall-through and the `t & 0xFFFF` arm
  behind the branch, which fixes the movables order. Expanding the macro locally with **both**
  ternaries inverted to `((yl) >= 0) ? 0 : MIN(...)` produces a **byte-identical object**:
  `fold`/`invert_truthvalue` canonicalises `c ? 0 : X` straight back. The ROM's compiler saw the same
  canonical form, which is consistent with the ROM's def order (`sll ...,10` before
  `andi ...,0xFFFF` in its preheader at `8002D100` / `8002D108`) being identical to ours.
- Loop-body respellings, all 838/838 and byte-identical to the seed: `s32 xl = j << 5` with
  `xh = xl + 32`; an `s32 yh` i-loop temp; every shift respelled as a multiply
  (`j * 32`, `(32 - i) * 4`, `1024`). Worse: `yh = yl + 4` (835, 1640 rows);
  `yl = 124 - i*4` with `yh = 128 - i*4` (840, frame `-0x1E8`).
- Goto-loops (the `goto-loop-defeats-loop-strength-reduction` lever) kill the hoist and therefore do
  reproduce the ROM branch trio exactly (`bgez` 2, `bgezl` 0, `j` 2) with the i-loop as a goto-loop,
  but at 868 instructions. j-loop only: 858. Both: 849. This is the clean proof that the branch form
  is a *consequence* of 356's allocation and carries no independent information.

### One further structural divergence, unexplained and probably related

The ROM keeps `lui $v1, 0xE4000000` **inside** the j-loop (`8002D1E0`) and computes
`or $v1,$s7,$v1` / `or $v0,$v0,$v1` there; this build hoists the whole `0xE4000000 | yhfield` into
the i-loop head (`or s6,v1,t9` with `lui t9,0xe400`), which costs one extra register (`$t9`) live
across the loop while saving two in-loop instructions -- static count unchanged, pressure +1 in
exactly the region that decides `$s1`. That hoist is gated by `loop.c:1631`,
`(threshold * savings * m->lifetime) >= insn_count`, with `threshold = 2 * (1 + n_non_fixed_regs)`
(`loop.c:532`) decaying by 3 per movable actually moved (`loop.c:1719`, `loop.c:1904`). Defeating
that one hoist -- without adding instructions -- is the most concrete remaining lead, and it is a
`move_movables` ordering/threshold question, not a spelling question.

### Verdict

Not terminal, but the S304 target was mis-named. Any further attempt must aim at
**hard-register availability at allocno 356's turn** (`global.c` `find_reg` pass 0 /
`regs_someone_prefers`, and the `loop.c:1631` hoist that inflates preheader pressure), and must be
measured with the `-dg` `Register dispositions` block, not with `allocno_report.py`'s priority
column alone -- the priority column is accurate and irrelevant here.

### `MEM_IN_STRUCT_P` / array-element spelling: clean negative (S304)

Tested after `func_8008534C` banked on that lever. Neither hoisted invariant reads memory --
`yl << 10` and `t & 0xFFFF` are pure functions of the loop index -- so `true_dependence`
(`sched.c:817`) and `loop.c`'s `invariant_p` have no aliasing surface in the grid loop; the only
memory traffic there is the `Gfx *` store stream and one spill reload of `src`. Measured anyway:

| form | result |
| --- | --- |
| `extern s32 D_800B7730[]` + `D_800B7730[0]` at all four sites | 826 / `-0x1A0` / 1619 rows |
| `extern u16* nuGfxCfb_ptr[]` + `nuGfxCfb_ptr[0]` | 837 / `-0x1D8` / 1600 rows |
| both | 824 / `-0x198` / 1617 rows |

The branch trio is unchanged in all three, confirming the lever does not reach the two competing
pseudos. The `D_800B7730` array form is a large *pressure reduction* (`lw` 92 vs 101, frame 0x38
smaller) -- the opposite of what this residual needs, which is **one more** unit of pressure so that
`$s1` is unavailable to allocno 356.

### The deciding allocno is 189, not 80 (S304)

Two measurements from the `-dg` conflict dump settle it.

**Allocno 80 cannot be the lever, for two independent reasons.**

1. *It does not conflict with 356.* `356 in conflicts(80)` is false and `80 in conflicts(356)` is
   false. Giving 80 a hard register cannot make any register unavailable to 356, so no slide is
   possible from that direction.
2. *It is structurally unallocatable.* `hard_reg_conflicts[80]` is
   `{2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,29}` -- `$v0,$v1,$a0-$a3,
   $t0-$t9,$s0-$s7,$sp`, i.e. **every allocatable GP register**. `find_reg` ORs that straight into
   `used1` (`global.c:936`) before either pass runs (`global.c:951-985`), so no candidate survives
   and the spill is unconditional. For contrast, 356's and 352's hard-reg block set is only
   `{$v0,$v1,$a0,$a1,$sp}`.

   Pseudo 80 is `n`, the shared `D_800B7730` temp: `(reg/v:SI 80)` with two defs (insns 1349, 1670)
   and uses under `and`(0xFFF) / `plus`(32) -- the four animated `gDPSetTileSize` arguments. Its two
   short ranges are merged into one 118-insn allocno spanning the densest part of the tail, where
   `local-alloc` has already claimed every register. Splitting it into two temps is the 835-instruction
   form already recorded above, so this is not source-reachable either.

**The register that actually differs is `$s1`, and the ROM holds a value in it that this build
spills.** An earlier `grep '\$s1[,)]'` missed it because the operand is line-final; a proper live-in
scan of the ROM's j-loop body (`.L8002D128` .. `bne $a3,$v0` at `8002D2C8`) reports
`read-before-write = s0 s1 s5 s6 s7 t6 t7 t9`. The `$s1` use is `or $v0,$a0,$s1` at `8002D18C`,
and its def is `or $s1,$v0,$s4` at `8002D0A0`, three insns into the i-loop head:

```
.L8002D094:                       |  mine (2a48):
  addu $a3,$zero,$zero            |    move a3,zero
  andi $v0,$s2,0xFFF              |    andi v0,s2,0xfff
  lui  $s4,(0xF4000000>>16)       |    lui  t9,0xf400
  or   $s1,$v0,$s4    <-- $s1     |    or   t9,v0,t9      <-- caller-saved, then spilled
...                               |  ...
  or   $v0,$a0,$s1    (8002D18C)  |    lw   t9,420(sp)    <-- reload from the extra slot
                                  |    or   v0,a0,t9
```

That value is **allocno 189** = `(ior (reg 188) (reg 190))` (insn 1937), the j-loop-invariant
`_SHIFTL(G_LOADTILE,24,8) | _SHIFTL(ult,0,12)` half of the `gDPLoadTile` packet inside
`gDPLoadTextureTile`. Its hard-reg block set is `{$v0,$v1,$a0,$a1,$sp}` -- the same as 356's, so
`$s1` **is** available to it -- and it conflicts with 356, 352, 313, 297, 254, 208, 637, 635 and 207.
It is simply ranked too low and never gets its turn before 356:

| allocno | expression | refs | len | priority | this build | ROM |
| --- | --- | --- | --- | --- | --- | --- |
| 189 | `G_LOADTILE_cmd \| ult` | 5 | 130 | **769** | spilled (reload slot `420`) | `$s1` |
| 356 | `t & 0xFFFF` | 5 | 107 | 934 | `$s1` | `$s4` (evicted by reload) |

Insert 189 above 356 and the ROM's entire chain falls out with no other change:
`207→s0, 637→s2, 635→s3, 189→s1, 356→s4, 352→s5, 313→s6, 297→s7, 254→t8, 208→spilled`.

**The exact arithmetic target.** `allocno_compare` (`global.c:587-604`) needs
`priority(189) > 934`. With `refs = 5` (`floor_log2(5)*5 = 10`) that means
`live_length(189) <= 107`, i.e. **23 insns shorter than the current 130**; alternatively
`refs >= 7` at the current length (`floor_log2(7)*7 = 14`, giving 1076). Raising the loop depth of
the *use* does not work: 189 and 356 are both used in the j-loop, so any `do {} while (0)` or extra
nesting scales both refs together and preserves the order.

189's 130 comes from where `move_movables` places its def: it is discovered at packet 4 of the
`gDPLoadTextureTile` expansion, so it is emitted near the **front** of the j-loop preheader, while
352/356 come from packet 8 (the texture rectangle) and land at the **back** (insns 1969/1970). The
~23-insn gap is exactly the preheader between them. The ROM's def sits at the same early position
(`8002D0A0`, three insns after the i-loop label), so the ROM's 189 is long-lived too -- which means
the ROM's advantage is *not* a shorter live range for 189, and the remaining unknown is narrow and
well-posed: what gives 189 a higher `allocno_compare` priority, or `$s1` specifically, in the ROM's
build. That is the one question a follow-up should answer.

**Verdict:** not terminal, and now reduced to a single named allocno with a numeric target.
`docs/hazard-index.md` should route this class to "one allocno's priority, not a register coin";
the working model is the one-register slide above, and the measurement is the `-dg`
`Register dispositions` block plus the `;; N conflicts:` lines, never `allocno_report.py`'s priority
column read in isolation.

### The refs axis is the lever, and it lands the ROM's allocation chain (S304)

The lead's arithmetic on the *product* was right and my "gap 23 or refs>=7" framing understated it.
`floor_log2` makes refs the cheap axis: at len 130, refs 7 gives `2*7/130*10000 = 1076`, clear of
356's 934, with the live range untouched. And refs are reachable, because `flow.c` accumulates
`REG_N_REFS (regno) += loop_depth` -- so a `do {} while (0)` placed around **one statement** raises
the ref weight of only the pseudos used in that statement.

**First: the CSE question is a non-issue.** 189 = `(ior (reg 188) (reg 190))` where 188 =
`(and (reg 637) 4095)` = `ult & 0xFFF` and 190 = the `0xF4000000` constant. 188 is *already* shared
with the `gDPSetTileSize` half in both builds -- the ROM shows the same single `andi $v0,$s2,0xFFF`
feeding both `or $s1,$v0,$s4` (LoadTile, `F4......`) and `or $s4,$v0,$s4` (SetTileSize, `F2......`).
Nothing to gain there. Explicit `s32 uls/ult/lrs/lrt` temps referenced from both call sites produce a
**byte-identical object and a byte-identical allocno table** (every refs / len / priority / reg equal,
only the pseudo numbering shifts) -- gcc folds them. That axis is closed.

**The depth probes, in order.** Each row is `do {} while (0)` nesting around a progressively smaller
piece, with the resulting 189 row and the s-register chain:

| form | 189 | chain | count |
| --- | --- | --- | --- |
| seed | refs 5, len 130, **769** | 207 s0, 637 s2, 635 s3, **356 s1**, 352 s4(evicted), 313 s5 | 838 |
| x1 round the whole `gDPLoadTextureTile` | refs 6, 923 | 254 s1, 208 s4, 356 s5 -- slides **two** | 836 |
| x2 round the whole call | refs 7, 1076 | 254 s1, 208 s2, 189 s3, 356 s5 | 836 |
| x1 round `gDPLoadTile` only | refs 6, 923 | 208 s1, **356 spilled**, 189 s5 | **838** |
| **x2 round the LoadTile `w0` store only** | **refs 7, 1076** | **207 s0, 189 s1, s2, s3, 356 out, 352 s5, 313 s6, s7, 254 t8** | **838** |

The last row is the ROM's chain exactly, as predicted. Wrapping the whole `gDPLoadTextureTile` always
lifts 254 and 208 with 189 (they are the other invariant halves of the same packets) and oversteps;
hand-expanding `gDPLoadTextureTile` and then `gDPLoadTileGeneric`, and wrapping **only the `w0`
assignment**, isolates 189 -- 208 stays at refs 5 / 781 and drops out of the chain onto `$t8`.

**Result: the grid loop's clamp block is now instruction-for-instruction and register-for-register
identical to the ROM**, including `sra $v0,$s5,7` (352 in `$s5`), the `lw` + `or` spilled arm for 356,
and `bgez` + `j` in place of the annulled `bgezl`:

```
        ROM  8002D25C                       mine  2c10
        bgez  $t7, .L8002D280               bgez  t7, 2c34
         sra  $v0, $s5, 7                    sra  v0, s5, 0x7
        ...  andi $v0,$v0,0xFFFF            ...  andi v0,v0,0xffff
        j    .L8002D288                     j    2c3c
         or   $v0, $a0, $v0                  or   v0, a0, v0
  .L8002D280:                          2c34:
        lw   $t8, 0x194($sp)                lw   t9, 428(sp)
        or   $v0, $a0, $t8                  or   v0, a0, t9
```

Mnemonic histogram against the ROM went from 4 deltas (`move` +1, `bgez` -1, `bgezl` +1, `j` -1) to
**2** (`lw` 100/101, `move` 80/79), at the same 838 instructions. Frame is still `-0x1D8`; the two
extra reload slots (`420`, `428`) and the preamble scheduling hunks are untouched and are what remain.

**A caveat on the source form.** `do { do { ... } while (0); } while (0);` round a hand-expanded
`w0` store is a *mechanism probe*, not a plausible original. It proves the residual is a ref-weight
question and names the pseudo, but the real source almost certainly reached the same weight through a
genuine construct. The obvious candidate to try next is the `HW_VERSION_1` arm of
`gDPLoadTextureTile` (`gbi.h:3908`), which wraps the whole expansion in a real
`for (_loadtile_i = 0; _loadtile_i < _loadtile_nw; _loadtile_i++) pkt;` -- a true loop, and therefore
a true `loop_depth` increment -- or `gDPLoadMultiTile`. Either would give the ref weight without the
probe's ugliness, if their emitted words match.

**Still open, and now the whole residual:** `lw` -1 / `move` +1 and the two extra reload slots. The
seed form is preserved at `/tmp` and reproduced by reverting the loop body to the plain
`gDPLoadTextureTile` call.

### Orchestrator addendum (S304): the `HW_VERSION_1` follow-up is ruled out

The crack agent's closing suggestion — reach allocno 189's ref weight naturally through the
`HW_VERSION_1` arm of `gDPLoadTextureTile` (`include/libultra/PR/gbi.h:3908`), whose expansion
wraps the packet advance in a real `for (_loadtile_i = 0; _loadtile_i < _loadtile_nw;
_loadtile_i++) pkt;` and would therefore give a genuine `loop_depth` increment — **does not apply
here.** That arm does not expand the packets inline at all; it calls the library function
`guDPLoadTextureTile(...)`, so any body using it emits a `jal`. The ROM's `func_8002CDA8` contains
exactly **5 `jal`s and every one of them is `osVirtualToPhysical`**:

```
$ grep -oE 'jal +[A-Za-z_0-9]+' asm/nonmatchings/main/func_8002A640/func_8002CDA8.s | sort | uniq -c
      5 jal        osVirtualToPhysical
```

So the ROM was built against the non-`HW_VERSION_1` (inline-expanding) `gDPLoadTextureTile`, and the
natural-construct search has to look elsewhere. `gDPLoadMultiTile` (`gbi.h`, same file) remains
untried: it expands inline to the same seven packets with `tmem` and `rtile` parameters, so at
`tmem = 0` / `rtile = G_TX_RENDERTILE` it should emit identical words — but it is a different macro
text and worth one `allocno_report` read to see whether 189's refs differ.

**Status of the winning form.** The `do { do { ... } while (0); } while (0);` wrapper around a
hand-expanded `gDPLoadTile` `w0` store is a *mechanism probe*, not a shippable body: it is the
measurement that proves the residual is `REG_N_REFS` weight on allocno 189 and nothing else. It is
kept in `base.c` because it is the best-measured form (mnemonic deltas 4 -> 2, grid-loop allocation
and branch form both matching the ROM), **not** because it is a bank candidate. Anyone resuming
must find the natural construct that reaches the same ref weight, or treat this as terminal.

## S305 probe: `gDPLoadMultiTile` is ruled out

One-construct probe, as scoped. `gDPLoadMultiTile(gfx++, src, 0, G_TX_RENDERTILE, G_IM_FMT_RGBA,
G_IM_SIZ_16b, 320, 0, ...)` emits **byte-identical words** to the `gDPLoadTextureTile` call it
replaces (checked packet-by-packet on a host `gbi.h` harness, all seven `w0`/`w1` pairs equal), and
it produces a **byte-identical object**: 838/838, frame `-0x1D8`, 1400 `cmpfn` rows, and the same
four-delta mnemonic histogram (`addu` +1, `bgez` -1, `bgezl` +1, `j` -1) as the plain
`gDPLoadTextureTile` body measured in the same session. So the two macros are indistinguishable to
`global.c`: allocno 189 keeps its low ref weight either way, and the grid-loop clamp block still
takes the annulled `bgezl` instead of the ROM's `bgez` + `j`.

Both natural constructs are now spent: `HW_VERSION_1` (S304, emits a `jal`) and `gDPLoadMultiTile`
(here). The only measured form that reaches the ROM's allocation chain remains the `do {} while (0)`
ref-weight probe, which is a mechanism proof rather than a shippable body. Re-opening this leaf
should start from `sched`/`global.c` source, not from another macro substitution.
