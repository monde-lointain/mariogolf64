# emit_sky_dome_dl — S297 carry (297/299, was 272/299 at S296)

**Still not a wall.** No permuter import, no terminal verdict. S297 took it from 272 to 297 with four
source fixes, three of them semantic or structural rather than compiler-coin. The residual is 2
instructions in the loop epilogue region. Re-open it on size order.

`src/main/func_8002A640.c`. The body is committed in the tree as C (the function is banked as source,
not as `INCLUDE_ASM`) only if the ROM is green; if it is carried, the stub is restored. Attempt source
kept at `nonmatchings/emit_sky_dome_dl/attempt.c`; the S297 297/299 form is the one in git history for
this doc's commit.

## What S296's reading got wrong, and it was a body bug, not a compiler wall

The band a strip samples is **not** recomputed per pass. The ROM's `$s3` is initialised once at
function entry to `0x500` and incremented by `0x3C0` inside the strip loop, never reset at a pass
boundary, so the bands run continuously 8, 14, 20 … across all 38 strips of all three passes. The S296
attempt recomputed the band from `strip` alone, which draws passes 1 and 2 from the wrong texel rows.
That was worth 9 instructions and, more importantly, it was wrong output.

Three further findings, each measured:

1. **The band counter is in 32-bit words, not texel rows.** `$s3` init `0x500` = 1280 and step
   `0x3C0` = 960, with the strip-loop preheader deriving the byte offset as the single instruction
   `sll $t5, $s3, 2`. A row-unit counter (init 8, step 6) forces a three-instruction
   `sll/addu/sll` preheader instead. `SKY_DOME_FIRST_WORD` = `320 * 8 / 2`, `SKY_DOME_BAND_WORDS` =
   `320 * 6 / 2`, band address = `((u32)D_800B67A0 + word * 4) & ~7`. Worth 2.
2. **The band address and the row count are re-evaluated at the if/else join, not cached.** The ROM
   computes the `(last ? 2 : 6)` chain twice — once before `bnez $s5` (the `tiled` test, feeding both
   arms and the `gDPLoadTile` `lrt`) and again at the join, where its only consumer is the `rows * 32`
   of the four `gSPModifyVertex` ST words. Likewise `D_800B67A0` is loaded three times: once per arm
   and once at the join. This is the cse-table-reset-at-a-multi-predecessor-join shape
   (`docs/hazards.md`, and the memory `ifelse-not-ternary-cse-reset`). Reproduced by a
   `SKY_DOME_BAND(word)` macro at each use site plus a second `last`/`rows` assignment placed
   **after** `gDPSetTileSize` and before the first `gSPModifyVertex` — placing it earlier splits
   `(rows - 1)` into two computations and costs 3. Worth 12.
3. **`loop.c:3823` decides the vertex-bank address, and a comma expression wins it.** The vertex bank
   address `&D_800DABA0[pass * 512] + frameOff` is a giv of the `pass` biv. The ROM does **not**
   strength-reduce it: it recomputes `sll $v0, $t9, 9` plus a fresh `lui/addiu` of `D_800DABA0` inside
   the else arm, and its outer back edge is therefore a `bnel` with `addiu $v0, $zero, 2` in the delay
   slot rather than a `+512` giv increment. `strength_reduce` ignores a giv when
   `v->lifetime * threshold * benefit < insn_count` (loop.c:3823), and `v->lifetime` is measured from
   the giv insn to the dest register's last use. Writing the address as a separate statement
   (`u8* bank = &D_800DABA0[pass * 512];` then `bank + frameOff`) puts the def *before* the macro's w0
   `lui/ori/sw`, which lengthens the lifetime past the threshold and the giv is reduced. Writing it as
   a comma expression **inside** the argument —
   `gSPVertex(gfx++, (bank = &D_800DABA0[pass * 512], bank + frameOff), 32, 0)` — evaluates the def
   after the w0 store, shortens the lifetime, and the giv is ignored. That single change also fixed
   the frame size (`-0x58` to the ROM's `-0x50`). Both arms need the comma form; a plain expression in
   the `pass == 2` arm re-introduces a hoisted `base + frameOff` invariant that the else arm then
   reuses, which is 3 instructions short of the ROM's inline `lui/addiu`.

**The gcc loop dump is the oracle for this class.** Compile the file with `-dL` added to the normal
flags and read `<file>.c.loop`: it prints `Loop from A to B: N real insns`, every `Biv N initialized
at insn M: initial value V`, and, for each giv, either `giv at N reduced to (reg:SI R)` or
`giv of insn N not worth while, X vs Y` with the two sides of the loop.c:3823 comparison. The band
biv's `initial value 1280` and the vertex-bank giv's reduce/ignore decision both read straight off it.
This is to `loop.c` what `tools/allocno_report.py` is to `global.c`.

## The residual: net 2, but it is two effects of 5, not one of 2

Do not read the net -2 as a single small divergence, and do not trust `cmpfn`'s hunk alignment here
(its hunks read 1:1 in size, which is an artifact of the whole-body register permutation). Count by
region instead, splitting at the two loop heads — the ROM's are `.L8002AE94` (pass) and `.L8002AF1C`
(strip):

| region | ROM | build | delta |
| --- | --- | --- | --- |
| entry to pass-loop head | 48 | 46 | **-2** |
| pass-loop head to strip-loop head (the vertex if/else and the per-pass constants) | 34 | 39 | **+5** |
| strip-loop body plus function epilogue | 217 | 212 | **-5** |

The `+5` is a known cost of the loop.c:3823 fix and is understood: the ROM hoists the whole `pass == 2`
vertex address to the prologue and reloads it with one `lw $t0, 0x14($sp)`, but the comma expression
that shortens the giv's lifetime is an assignment, so gcc cannot hoist that arm and the build
recomputes `lui/addiu/addiu/lw/addu` in place. Measured alternatives, all worse overall: a plain
expression in the `pass == 2` arm with the comma form kept in the else arm reads 300 with frame
`-0x60` (`pass * 512`) or 297 with frame `-0x58` (a literal `2 * 512`), because a hoistable then-arm
re-introduces the `base + frameOff` invariant that the else arm then reuses.

So the next attempt has two separable jobs: get the `pass == 2` arm hoisted **without** lengthening
the else arm's giv lifetime (the two are currently coupled through the shared invariant), and find the
5 instructions the strip body is short. Both sides carry the same seven induction variables (band
words `+0x3C0`, band bytes `+0xF00`, three doubled-vertex-index givs `+4`, `strip +1`, `pass +1`), and
the ROM emits the two band increments in the loop epilogue where the build hoists them earlier, so
look at the `Gfx*` bookkeeping first: the ROM keeps two pointers, `$t1` (the store base, used at `-4`
and `0` displacements) and `$t2` (the `gfx` value written back through `gfxp`), and merges `$t2`'s
bumps as `0x8/0x8/0x18/0x10/0x8/0x68`. Every DL command word is confirmed correct; this is not a
missing command.

## Constants already decoded (do not re-derive)

- `0x0700001A` in the `gDPLoadBlock` word is `tile 7 | dxt 0x1A`, and `0x1A` is
  `CALC_DXT(320, G_IM_SIZ_16b_BYTES)`.
- The `slti 0x800` / `0x7FF` clamp in the `.s` is **not** source. It is
  `MIN(lrs, G_TX_LDBLK_MAX_TXL)` inside `gDPLoadBlock` itself.
- `D_800DABB0 == D_800DABA0 + 0x10`; both are referenced with their own `%hi`/`%lo`, so they stay two
  externs, not one symbol.
- `$s2` = `0x02140000` is the `gSPModifyVertex` header with `G_MWO_POINT_ST` = `0x14`.
- The second bind's two `gDPSetTile`s share one w0 (`0xF5109800`, line 76) and differ only in w1
  (`0x07000000` for `G_TX_LOADTILE`, `0` for `G_TX_RENDERTILE`).
