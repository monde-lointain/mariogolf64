# emit_sky_dome_dl — S297 carry (299/299 count, register permutation; was 297/299, 272/299 at S296)

> **Verdict in one line.** Instruction count, frame and all three region counts are exact. The whole
> residual is one register: `tiled` loses the 9th callee-saved register, and every lever that wins it
> at global-alloc loses it again at reload. The chain is fully traced and terminates on a single
> unsolved question — *three block-local `(set (reg) (const_int -8))` pseudos that
> `combine_movables` does not unify, without a function-scope variable* — with every reachable gate
> enumerated below and shown unreachable from source. **Do not re-derive:** the two predicted nulls
> (lengthening allocno 142; uniform `refs` reweighting) and the 25-of-25 occupancy argument are
> written out with their arithmetic precisely so the next attempt does not spend on them again.

**This is a wall with a named gate, not an unfinished reconstruction.** That is the opposite of what
the S296 revision of this doc said about the same function, and the distinction is the point:
everything above the register allocator is *settled* — semantics, all 299 instructions, the `-0x50`
frame, all three region counts (48/34/217). What remains is a single `combine_movables` question
whose every reachable gate is enumerated and refuted below. **Do not re-open this on size order and
do not spend iterations on it.** If a future sprint gains a lever that reaches any of those gates,
it banks immediately; short of that, it does not.

S297 took it from 272 to 297 to 299/299, with a permuter import (base 7575 -> best 4870, no zero,
workspace at `nonmatchings/emit_sky_dome_dl-2`).

**The generalising finding — read this even if you never touch this function.** Reload's spill victim
is chosen from two tiers (reload1.c:3681-3711): tier 1 is hard regs with `uses == 0`, tier 2 is the
rest sorted ascending by `uses` = Σ`reg_n_refs` (reload1.c:3628-3638). `local_alloc` runs *before*
`global_alloc` and only ever uses the call-clobbered `v0`, `v1`, `a0`-`a3` for its local quantities.
So **freeing a caller-saved register achieves nothing — local-alloc re-absorbs it — while freeing a
t- or s-register opens tier 1, because local-alloc never reaches those.** Measured here: deleting a
convenience local freed `$a3`, locals took it, and reload's victim merely moved `$a2` -> `$a3`. Any
crack that needs reload to stop stealing a register must free a *callee-saved or temporary* register
specifically; counting allocnos is not enough.

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

## The reweight direction is arithmetically dead; `.greg` names the reload victim

**`refs` cannot reach the window, and this is a property of the formula, not a tuning failure.**
Priority is `floor_log2(refs) * refs * 10000 / len` (global.c:587) and `floor_log2` is a step
function, so 7 -> 8 crosses `floor_log2` 2 -> 3 and multiplies the weight by `24/14` = **1.71x**. The
ROM's window (621, 815) against `tiled`'s 566 needs between **1.10x and 1.44x**. There is no `refs`
value that lands inside:

| `tiled` refs | priority at len 247 | vs window (621, 815) |
| --- | --- | --- |
| 7 (now) | 566 | below |
| 8 | 971 | **over** |
| 9 | 1093 | **over** |

Both `do {} while (0)` wraps were measured anyway. Around `tiled = 0;`: refs 8, priority 1030,
`tiled` ranks *above* `0xE7000000` (874) at position four, 296/299 with frame `-0x48`. Around the
whole `if/else`: `tiled` refs 9 -> 1093, but `0x0700001A` also goes to refs 9 -> 1120 and still wins,
because the two have identical ref structure (`1 + 3 + 3`) and a uniform wrap scales both. Only `len`
has fine enough resolution, and that lever is the already-known `tiled = 1` at the arm end (len 205,
priority 682).

**`-dg` states the reload decision outright, and it is the same need in both variants:**

```
;; Need 1 reg of class GR_REGS (for insn 10).
;; Need 1 reg of class ALL_REGS (for insn 10).
```

`insn 10` is `(set (reg/v:SI 73) (mem:SI (reg/v:SI 72)))` — `Gfx* gfx = *gfxp;`, the first statement.
Only the victim differs. `base.c` prints `Spilling reg 6` (`$a2`) with pseudos 88 and 208 going to
the stack, and never touches `tiled`, which had already lost at global-alloc. `alt_298` prints
`Spilling reg 21` (`$s5`) and `Register 78 now on stack` — global-alloc *does* give `tiled` a
register once its priority is in the window, and reload takes it straight back.

The selection rule is reload1.c:3681-3711: `potential_reload_regs` is built in two tiers — tier 1 is
hard regs with `uses == 0` in `REG_ALLOC_ORDER`, tier 2 is the rest sorted ascending by `uses`, where
`uses` is the sum of `reg_n_refs` over the pseudos allocated to that hard reg (reload1.c:3628-3638).
`reg_n_refs` is the same loop-depth-weighted count the allocno table uses, so a register holding only
`tiled` scores 7 and is the cheapest thing in the function to steal. Defending it by weight needs
`refs >= 9`, and `refs 9` inside the global window needs `270000 / len` in (621, 815), i.e.
`len` in (331, 435) against `tiled`'s actual 205-252 — the two constraints are incompatible.

**So the remaining route is to make some register tier 1** (`uses == 0`), so reload never enters tier
2 at all. The ROM's `$t0` looks exactly like that: it holds no allocated pseudo, only rematerialised
constants and the two `0x0700001A` reloads. This build carries one global allocno the ROM does not —
`align` (refs 18, len 43, `$t1`), or, without it, the hoisted `-8` (refs 10). Eliminating that one
allocno *without* re-hoisting the masks is the open problem, and it is the same `combine_movables`
question, now with a concrete payoff attached.

## Permuter: a mechanism, not a zero, and the mechanism does not transfer

Imported at `nonmatchings/emit_sky_dome_dl-2`. Base score 7575, best **4870** after ~6300 iterations
at `-j 15 --best-only`; no zero, consistent with the 0-of-5 standing tally. The workspace is left in
place. Note that the seed dir now carries its own `compile.sh` for `allocno_report.py`, so
`mg_find_permuter_dir` matches the *seed* dir first — invoke `permuter.py` with the explicit
`nonmatchings/emit_sky_dome_dl-2` path.

All four top candidates (4870, 4995, 5575, 5745) name one mechanism: split a sub-term of a
`gSPModifyVertex` / `gSP2Triangles` word into a temporary computed **before** the store, or share one
sub-term temp across several words. That points straight at the largest remaining LCS chunk (ROM
198-232). **Tested and null:** the direct reading — `s32 st = rows * 32;` as its own statement after
the second `rows` assignment, with the ST words spelled `st + 0x01000000` / `st + 0x27000000` — is
**byte-identical** to `base.c`. gcc already holds one pseudo for `rows * 32`; the divergence is where
that `sll` is *scheduled* (ROM index 228, immediately after the `gDPSetTileSize` store; build index
251, at first use), not how the expression is spelled. The permuter's edit is a scheduling
perturbation, not a source form to adopt.

## Verdict on the register residual: two constraints, jointly unsatisfiable for `tiled`

The reload threshold is now bracketed from both sides by measurement rather than inferred:

| variant | `tiled` refs | global-alloc | reload (`-dg`) |
| --- | --- | --- | --- |
| `alt_298` | 7 | gets `$s5` | `Spilling reg 21` / `Register 78 now on stack` — **steals it** |
| do-while(0) around the `if/else` | 9 | gets `$s3` | `Spilling reg 6` (`$a2`, pseudos 88 + 208) — **leaves it** |

So Σ`reg_n_refs` over `$a2`'s occupants is strictly between 7 and 9, i.e. **exactly 8**, and `tiled`
survives reload iff its `refs >= 9` (reload1.c:3628-3638 computes `uses`; reload1.c:3681-3711 orders
tier 2 ascending by it). Against the global-alloc window (621, 815) from global.c:587:

| `tiled` refs | `len` required by the window | measured `len` range |
| --- | --- | --- |
| 8 | 295..386 (and 8 only *ties* `$a2`) | 205..252 |
| 9 | 332..434 | 205..252 |
| 10 | 369..483 | 205..252 |

No reachable pair satisfies both. Every variant either loses at global-alloc, or wins it at refs 7
and is stolen by reload, or survives reload at refs 8-9 and overshoots the window. **Terminal for any
lever acting on allocno 78.**

`len` is `REG_LIVE_LENGTH`, the def-to-last-use span, and `tiled`'s last use is the
`if (tiled == 0)` test at the *top* of the strip body — which is why it caps at ~252 while constants
consumed late in the body sit at 386-486, exactly the band refs 9 would need. But the ROM has only
three references to `$s5` (`addu s5,zero,zero`, `bnez s5`, `addiu s5,zero,1`), so the ROM's `tiled`
has the same shape as this build's. **The ROM does not win this by reweighting `tiled` at all.**

**It wins by having a cheaper spill victim.** Tier 1 (`uses == 0`) is *empty* here: every GPR
`v0`..`t9`, `s0`..`s7`, `fp` holds at least one pseudo (24 global allocnos plus 98 local quantities
spread over `v0`, `v1`, `a0`, `a1`, `a2`, `a3`). `$a2` is the only GPR occupied *solely* by local
quantities — 88 and 208, refs totalling 8 — which is why `Spilling reg 6` is what the good build
picks. The ROM's `$t0` holds **no allocated pseudo at all**: only rematerialised constants
(`0xFD100000`, `0x07000000`, `0xF3000000`, `0x004DC000`) and the two `0x0700001A` reloads. That is a
tier-1 register, so the ROM never enters tier 2, spills nothing, and `tiled` keeps `$s5`.

The single allocno this build carries that the ROM does not is `align` (86, refs 18, len 43), alone
in `$t1`. Remove it *without* re-hoisting the masks and `$t1` becomes `uses == 0`, reload takes it
from tier 1 for free, and the whole chain resolves. So the entire remaining residual reduces to one
question, unchanged since the `combine_movables` fix: **three block-local
`(set (reg) (const_int -8))` pseudos that loop.c does not unify, without a function-scope variable.**
Every reachable gate in `combine_movables` (loop.c:1245-1287) and why it fails from source:

- `n_times_used == 1` — a copy of `n_times_set` (loop.c:597); breaking it needs one variable set at
  all three sites, which *is* `align`, and that is what costs the allocno.
- `!m1->global` — needs each mask pseudo live outside the strip loop; not expressible in source.
- mode compatibility — SI vs DI separates at most **one** of the three; an increasing bit-size chain
  needs a third integer mode wider than DI, which MIPS o32 does not have.
- `rtx_equal_for_loop_p` on `set_src` — two `(const_int -8)` always compare equal.
- `m->dependencies` — 0 for every const load; only libcall blocks differ.

Partial forms keep the masks in place but still cost the allocno: two sites through `align`
(`n_times_set` 2, so not a movable) plus one plain `& ~7` temp (`savings 1, lifetime 1`, not
desirable). The two `align` sites are in different basic blocks, so `align` stays global. For it to
be a *local* quantity its sites would have to share one basic block, and the three mask sites are in
three — the untiled arm, the tiled arm, and the join.

**Predicted null, not built: lengthening allocno 142.** Its priority 580 falls below 78's 566 only at
`len >= 248`, and its length is fixed by its def position among the re-hoisted constants — it is 5th
of 6 with 4 insns ahead, so moving it to the front gains at most 4 (241 -> 245). Even if it were
reached, `tiled` would then hold a register at refs 7, `uses` 7 < 8, and reload would steal it
exactly as in `alt_298`. The lever is sound in principle and dead in arithmetic for the same reason
as the rest: refs 7 sits below the reload threshold.

## The local audit: `last` was the convenience local, and freeing it still does not reach tier 1

`Spilling reg N` in the `-dg` dump is a binary tier-1 indicator — present means tier 1 was empty and
reload had to enter tier 2. Auditing every local against it (all rows 299/299, frame `-0x50`):

| variant | allocnos | reload victim | verdict |
| --- | --- | --- | --- |
| baseline | 31 | `reg 6` (`$a2`) | — |
| inline `lastPass` | 31 | `reg 6` | **null** — byte-identical, not an allocno |
| inline `frameOff` | 31 | `reg 6` | **null** — byte-identical, not an allocno |
| inline both | 31 | `reg 6` | null |
| inline `last` | **30** | `reg 7` (`$a3`) | **frees one allocno** |
| no `align` (masks re-hoist) | 31 | `reg 6` | 297/299, no gain |

`lastPass` and `frameOff` are not holding registers — cse rebuilds the same pseudo without the
variable, so they are free either way and stay for readability. The convenience local that *was*
costing an allocno is **`last`**. Inlining it at both sites drops 31 → 30 allocnos, keeps 299/299 and
`-0x50`, keeps the mnemonic-order score at 196, improves the LCS mnemonic diff **71 → 65**, and moves
the store cursor from `$a3` to `$t0` and gfx from `$t0` to `$t1` — one step closer to the ROM's `$t1`
cursor and `$t2` gfx. This is now `base.c`.

**But freeing an allocno does not create a tier-1 register, and the reason is structural.**
`local_alloc` runs *before* `global_alloc` and fills `v0`, `v1`, `a0`, `a1`, `a2`, `a3` with local
quantities regardless; the global allocnos then take what is left. Occupancy is 25 of 25 GPRs either
way:

```
31 allocnos: globals on 23 distinct regs + locals hold v0, a2       = 25
30 allocnos: globals on 22 distinct regs + locals hold v0, a2, a3   = 25
```

Freeing a global allocno simply hands that register back to local-alloc, which is why reload's victim
moved from `$a2` to `$a3` rather than disappearing. So "delete a local to free a tier-1 register" does
not work on this function at any allocno count — the locals expand to fill.

The reload threshold is likewise confirmed independent of the allocno count: `nolast` plus
`tiled = 1` at the arm end gives 30 allocnos, `tiled` at priority 682 (inside the window), assigned
`$s5` — and reload still takes it (`Spilling reg 21`). That form reads 298/299 with mnemonic-order
mismatches **159** and LCS **66**, the best structural score of anything built, and is kept as
`alt_298_bodyexact.c`. While `tiled` holds a register at refs 7 it is the cheapest tier-2 entry in the
function and reload takes it; clearing that threshold needs refs ≥ 9, which overshoots the
global-alloc window. Both constraints are measured, and they do not intersect.

## Value reuse (time-boxed), and the exact shape of what would work

**Deliberate reuse of a disjoint local — null.** The permuter named this mechanism itself: its two
best candidates (4870, 5575) both reuse the *existing* `band` local as the donor for a
`gSPModifyVertex`/`gSP2Triangles` word sub-term rather than adding a temp. Applied correctly with the
one local whose range is genuinely disjoint there — `align` is dead from the join's band computation
until the next iteration, and the ModifyVertex block sits between — as `align = rows * 32;` with the
ST words spelled `align + 0x01000000` / `align + 0x27000000`: 299/299, frame `-0x50`, allocnos **30
(unchanged)**, LCS **65 (unchanged)**, still `Spilling reg 7`. Assigning a value to an existing local
only renames the pseudo; `rows * 32` has two uses so gcc needs a pseudo for it either way. **Reuse
does not reduce demand when the donated value itself needs a register.**

**Permuter harvest for pseudo count — no signal.** Re-read all four best candidates specifically for
merge / delete / re-scope of a temporary: one reuse signal (above, tested and null); the others *add*
a temp (`int new_var`, `volatile unsigned short new_var2`). No candidate deletes or re-scopes a local.

**What would actually work, stated so it is not re-derived.** `local_alloc` runs first and only ever
uses the call-clobbered `v0`, `v1`, `a0`, `a1`, `a2`, `a3` for its ~98 local quantities; the global
allocnos then occupy `t0`-`t9`, `s0`-`s7`, `fp`. So a freed *caller-saved* register is immediately
re-absorbed by local-alloc — that is exactly what happened when `last` went, `$a3` freed, locals took
it, and reload's victim merely moved from `$a2` to `$a3` — but a freed **t- or s-register would stay
free**, because locals never reach them. It would be `uses == 0`, i.e. tier 1, and reload would take
it at reload1.c:3682-3688 without spilling anything.

In the current 30-allocno build the t-registers hold: `t0` cursor, `t1` gfx, `t2` **`align`**, `t3`/`t4`
loop values, `t5`-`t8` the four loop.c givs, `t9` pass. The only one not required by the computation
is `align`, the mask variable. Deleting it frees a t-register, opens tier 1, stops the spill, and lets
`tiled` keep its register. The full chain:

```
three block-local (set (reg) (const_int -8)) pseudos that combine_movables does not unify,
without a function-scope variable
  -> no `align` allocno
  -> a t-register with uses == 0
  -> reload takes it from tier 1 (reload1.c:3682-3688) not tier 2 (3708-3710)
  -> nothing is spilled
  -> `tiled` keeps its hard register, as in the ROM
```

Every reachable gate of `combine_movables` (loop.c:1245-1287) is enumerated above with why it cannot
be reached from source. **That is the wall.**
