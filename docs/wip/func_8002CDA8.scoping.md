# `func_8002CDA8` reconstruction scoping (S303)

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

## Next actions

1. The residual is a pure allocation gap at exact instruction count: run the priority-window
   procedure (`tools/allocno_report.py src/main/func_8002A640.c func_8002CDA8`) and find where this
   build's `global.c allocno_compare` order diverges from the ROM's register assignment. The
   boundary here sits near priority 900-1000, with two allocnos already unallocated above it.
2. Three long-lived tail values are the concrete suspects: the `E4080080` / `04000400` pair shared by
   the three identical `gSPTextureRectangle` calls, and the `01020020` word shared by the two
   identical `gDPSetTileSize(gfx++, 1, 0, 0, 32, 32)` calls. Spelling one of each differently (for
   example `8 << 2` for `32`) shortens those live ranges; that was not tried.
3. Do not re-derive the display list: it is verified. Start from the body below.

Host risk when integrating (S300): `src/main/func_8002A640.c` is a partial file. This body emits no
FP constants and no strings, so its `.rodata` should be empty -- confirm with
`objdump -s -j .rodata` on the object before claiming a bank.

## Working body (838/838, frame -0x1E0, cmpfn 1478 rows)

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
  gDPPipeSync(gfx++);
  gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, 320, 240);
  gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, 32,
                   osVirtualToPhysical(D_80105320) & ~7);
  gDPPipeSync(gfx++);

  D_800B7730 = D_800B7730 + 1;

  for (i = 0; i != 32; i++) {
    for (j = 0; j != 4; j++) {
      gDPLoadTextureTile(gfx++, src, G_IM_FMT_RGBA, G_IM_SIZ_16b, 320, 0,
                         34 + j * 76, 48 + i * 3, 41 + j * 76, 51 + i * 3, 0,
                         G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                         G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gSPScisTextureRectangle(gfx++, j << 5, (31 - i) << 2, (j + 1) << 5,
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
