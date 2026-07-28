# emit_sky_dome_dl — S297 carry (299/299 count, register permutation; was 297/299, 272/299 at S296)

**Still not a wall.** No permuter import, no terminal verdict. S297 took it from 272 to 297 to
**299/299 with the ROM's -0x50 frame and all three region counts exact**. The residual is now a
whole-body register permutation with one identified root cause (see "The register residual" below).
Re-open it on size order.

> **Read the second half of this doc first.** The section "The residual: net 2, but it is two
> effects of 5" describes the 297/299 state and is kept for its mechanism write-up, but its
> conclusions about which alternatives are "worse overall" were measured *before* the
> `combine_movables` fix and no longer hold.

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

**The `+5` and the `-5` are the same five instructions, and they are constants hoisted out of the
strip loop.** A mnemonic histogram of the strip-loop body alone (ROM 217, build 212, splitting at
`.L8002AF1C` and at the build's matching back-edge target) isolates them exactly: after normalising
`li`/`move` to `addiu`/`addu`, the build is short 3 `addiu rX,$zero,-8` and 2 standalone `lui`
command-word loads, and nothing else — every `lui+lw`, `lui+ori`, `addu`, and branch count matches. So
the ROM re-materialises the `& ~7` mask at each of the three band sites and two DL command words at
each of their sites, while the build loads each once in the strip loop's preheader. Nothing is
missing; five instructions sit one region too high.

`-dL` names the pass. In the build's dump, the strip loop is `Loop from 141 to 752: 225 real insns`,
and `move_movables` unifies duplicate constant loads before hoisting them — the dump prints the
duplicates as `done move-insn matches <first insn>` against a first occurrence that was
`moved to <insn>` with `savings 2` or `3` (the savings count is the number of unified uses). The
constants the ROM keeps in place appear in the build as exactly this pattern. The two `-8` loads that
did **not** get unified show as `move-insn savings 1 not desirable` and stay in the loop, which is
what all three do in the ROM.

**Correction, from a sibling crack in the same sprint: the `threshold -= 3` decay IS a live lever
here.** An earlier revision of this doc ruled it out arithmetically — "blocking a `savings 1,
lifetime 23` move needs the threshold under 10, and it starts near 120" — but those savings and
lifetime figures were assumed, not read out of the dump. `func_8009548C` (S297, `f1d149d`) was cracked
by exactly this decay: its inner loop hoists twelve display-list constants, the ROM moves **9**, and
landing on 9 rather than 12 needed exactly **one** invariant insn ahead of the constants at an
inner-loop `insn_count` of 93 — achieved by splitting `col * 4` into its own statement, since the
natural two-insn `col * 5` prefix costs three more threshold and strands one more constant. Getting
the count wrong also spilled a local for three further instructions.

So there are two candidate levers, and the dump distinguishes them: change how many invariant insns
precede the constants (the decay), or change whether the duplicate loads in the arms and at the join
are recognised as the same movable at all (`combine_movables`, the `done move-insn matches` lines).
Read `savings` and `lifetime` per movable off the dump and compute loop.c:1631 rather than estimating
it — that is what the earlier revision got wrong.

The remaining `-2` is in the prologue (48 against 46) and has not been characterised.

Both sides carry the same seven induction variables (band words `+0x3C0`, band bytes `+0xF00`, three
doubled-vertex-index givs `+4`, `strip +1`, `pass +1`). Every DL command word is confirmed correct;
this is not a missing command.

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

## The four fixes that took 297 to 299/299 (exact count, exact frame, exact regions)

Best form is `nonmatchings/emit_sky_dome_dl/base.c` (= `best.c`); the full measurement log is
`nonmatchings/emit_sky_dome_dl/STATUS`. Regions are now 48/34/217 against the ROM's 48/34/217, and
the frame is `-0x50`.

1. **`combine_movables` is the lever for the `-5`, and a multiply-set local defeats it.** The three
   `& ~7` band sites expand to three identical `(set (reg) (const_int -8))` movables. `loop.c`
   `combine_movables` (loop.c:1245-1287) matches them on `rtx_equal_for_loop_p (m->set_src,
   m1->set_src)` — a `const_int` always compares equal — and folds them into one movable with
   `savings 3, lifetime 3`. `move_movables` then hoists it because loop.c:1631
   `threshold * savings * lifetime >= insn_count` holds at `120 * 3 * 3` against `insn_count` 225.
   Uncombined, each is `savings 1, lifetime 1`, needs `threshold >= 225`, and stays in place — which
   is the ROM. None of the combine gates (`!m1->global`, mode compatibility, `dependencies`) is
   reachable from source for identical constants; the reachable one is
   **`n_times_used[regno] == 1`**, which in `loop.c` is a copy of `n_times_set` (loop.c:597), i.e.
   the number of *sets*. Routing all three masks through one local — `& (align = ~7)` — makes
   `n_times_set == 3`, so `scan_loop`'s `n_times_set == 1 || consec_sets_invariant_p` gate fails and
   no movable is built at all. Put the assignment on the **RHS of the `&`**: expand evaluates op0
   first, so the `li` lands after the address arithmetic where the ROM has it. The comma-first
   spelling `((align = ~7), x & align)` hoists the `li` to the top of the block and scores worse.
   This also un-hoisted the two `lui 0x0700` (reload rematerialises them at each use from their
   `REG_EQUIV`), so the whole `-5` closes at once.
   **The `threshold -= 3` decay is not the lever here, and this time the arithmetic is computed
   rather than assumed.** `threshold` starts at `2 * (1 + n_non_fixed_regs)` = 122 and decays 3 per
   successful move (loop.c:1719/1904); `insn_count` for the strip loop is 225. Off the pre-fix
   dump: the `-8` is insn 192, `savings 3`, `life 3`, and the **3rd** move, so `threshold` is
   `122 - 6 = 116` and `116 * 3 * 3 = 1044 >= 225` — failing it needs `threshold < 25`, i.e. 33
   prior moves. The `0x07000000` is insn 301, `savings 2`, `life 2`, the **8th** move, `threshold`
   `122 - 21 = 101`, `101 * 2 * 2 = 404 >= 225` — failing it needs `threshold < 56.25`, i.e. 22
   prior moves. The strip loop makes 12 successful moves in total. The reason the decay reaches on
   `func_8009548C` and not here is structural: there the stranded constants are `savings 1`, so the
   left-hand side is `1 * lifetime` and one extra move is decisive; here `combine_movables` has
   already multiplied it by 9 and by 4 by unifying 3 and 2 copies. Defeating the combine drops both
   to `savings 1, lifetime 1`, where the test needs `threshold >= 225` against a ceiling of 122 —
   a hard block rather than a marginal one. The pass-loop re-hoist below *is* decay- and
   doubling-sensitive; the strip-loop hoist is not.
2. **The `+5` closes by making the `pass == 2` arm hoistable again.** `&D_800DABA0[2 * 512] +
   frameOff` as a plain expression is hoisted by loop.c to the pass-loop preheader and reloaded in
   the arm with one `lw 0x14(sp)`, exactly as the ROM does; the else arm keeps the comma form that
   wins loop.c:3823. This also restores the fifth stack slot and the `-0x50` frame. The earlier
   revision measured this as "worse overall" — that was true only before fix 1.
3. **The arms are the wrong way round in the carry.** The ROM's `bnez $s5` branches *away* on
   `tiled != 0`, so the seven-command `gDPLoadTextureBlock` arm is the fall-through:
   `if (tiled == 0) { tiled = 1; gDPLoadTextureBlock(...); } else { four commands }`.
4. `tiled = 0` moved off the declarator to just before the pass loop (allocno length 252 -> 247).

## The register residual, and the one lever that reaches it

`tiled` and the hoisted `0x0700001A` compete for the ninth callee-saved register and the build loses
the coin. `tools/allocno_report.py` (a `compile.sh` is now checked into the seed dir so it can read
this seed directly):

| allocno | value | refs | len | priority | build | ROM |
| --- | --- | --- | --- | --- | --- | --- |
| 78 | `tiled` | 7 | 247 | 566 | spilled 0xC(sp) | `$s5` |
| 142 | `0x0700001A` | 7 | 241 | 580 | `$fp` | spilled 0x1C(sp), reloaded into `$t0` |

`refs` is the sum of `loop_depth` over each reference, so both read `1 + 3 + 3`; priority is
`floor_log2(refs) * refs * 10000 / len` (global.c:587). The ROM's `tiled` sits sixth, between
`0xE7000000` (815) and `0x27000000` (621), so its priority window is **(621, 815)**.

**Moving `tiled = 1` to the end of the untiled arm reaches the window**: length 247 -> 205, priority
682, and global-alloc does hand it `$s4`. It still does not stick — `0x0700001A` is spilled in that
variant and its two reloads need a scratch, so reload takes the hard register back and `tiled` ends
up on the stack anyway, one instruction short at 298/299. Hand-expanding the composite and placing
`tiled = 1` mid-arm (length 221, priority 633) behaves the same. This is **not** caused by the
`align` local: with the plain `& ~7` and `tiled = 1` at the arm end, allocno 78 reads priority 686
and still gets no register. So the two reachable states are "142 in a register + `tiled` spilled"
(299/299, the current best) and "`tiled` in a register + 142 spilled" (the ROM), and the second
costs an instruction here. That is the open question for the next pass.

Two further count-neutral divergences, both measured:

- **`0xE7000000` is hoisted one loop too far.** The build puts it in the function prologue, the ROM
  keeps it in the strip-loop preheader. The `-dL` dump names the mechanism: when the pass loop
  re-scans the strip preheader's constants they all print `halved since already moved`, i.e.
  `insn_count *= 2` per already-moved movable (loop.c:1611). `insn_count` runs
  282, 564, 1128, 2256, 4512, 9024, 18048, 36096; the first six pass loop.c:1631 and the seventh
  fails. The list order is first-appearance order in the strip body — `0xFD100000`, `0x07000000`,
  `0xE6000000`, `0xF3000000`, `0x0700001A`, `0xE7000000`, then `0xF5109800`, `0x02140000`,
  `0x01000000`, `0x27000000` which all stay — and `0xE7000000` is sixth. The ROM's cutoff is five.
  Making the sixth fail through the decay needs `threshold < 62.9`, i.e. 20 prior moves against the
  7 that exist, so the decay cannot do it; it needs one more already-moved movable ahead of
  `0xE7000000` in the list.
- **The vertex if/else emits two copies of `gfx`** (`move a0,t0` plus `move v1,t0`) where the ROM
  shares one `addu $v1,$t2,$zero` in the `bne` delay slot and still increments in both arms.
  Hoisting the copy (`vtx = gfx;` with `gfx++` inside each arm) collapses the two increments into
  one and costs 3: 296/299.

## `alt_298_bodyexact.c`: one instruction short, but the body is structurally exact

`nonmatchings/emit_sky_dome_dl/alt_298_bodyexact.c` differs from `base.c` by one line — `tiled = 1;`
sits *after* the `gDPLoadTextureBlock` call instead of before it. It reads 298/299 with the correct
`-0x50` frame and regions **47/34/217**, and in it the strip body's mnemonic histogram against the
ROM is **empty**: only register naming differs across all 217 instructions, and region 1 matches
too. LCS-aligned mnemonic diff 68 against `base.c`'s 71. This is the form that puts allocno 78
(`tiled`) at priority 682, inside the ROM's (621, 815) window, where global-alloc does hand it `$s4`
before reload takes it back.

Its entire deficit is one instruction in the prologue. The ROM materialises the symbol and then adds
the offset —

```
lui   $t0, %hi(D_800DABA0)
addiu $t0, $t0, %lo(D_800DABA0)
addiu $v0, $t0, 0x400
```

— where the build folds the offset into the relocation (`lui %hi`, `addiu %lo+1024`, two insns).
**The fold is not reachable by re-associating the address**: `&D_800DABA0[frameOff + 2 * 512]`,
`&D_800DABA0[512] + frameOff + 512` and `&D_800DABA0[frameOff] + 2 * 512` all canonicalise in `fold`
before RTL and emit byte-identical code. The ROM's three-insn form implies the offset was not a
compile-time constant at expand time and was folded afterwards — i.e. `pass * 512` with cse
propagating `pass == 2` into the arm — but that spelling loses the hoist, reading 300/299 with frame
`-0x58`.

`base.c` is kept as the deliverable because it holds the exact count; `alt_298_bodyexact.c` is where
the next pass should start if the count gate is allowed to move.
