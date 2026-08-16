# lz_decompress_extended — near-match (S166 -> S317 -> S318, CARRIED)

`src/main/lz_decompress_simple.c`, 0x468 / 282 instructions, the file's last `INCLUDE_ASM` stub.
Banking it takes the host to md5-candidate (`lz_decompress_dma` S165 `20ff76d`,
`lz_decompress_simple` S166 `60ecb9a`).

## Measured (rows as of S318)

Body in tree: the S317 replay retyped onto the host's `LzDecompressState` plus a `LzHistoryState`
for the frame-local window, with six structural edits (below).

- **282 of 282 instructions at the ROM's exact `-0x18` frame.**
- **The mnemonic multiset matches** (S317's 3 `andi` / 2 `lhu` / 3 branch-form deficit is closed).
- **The instruction *order* matches**: normalising every register name to `R` makes the two streams
  diff-identical. The entire residual is register assignment.
- **20 `cmpfn` rows (cmpfn as of S318)**, all register-name-only: 200 before the weight levers, 132
  after them, 20 after the `local-alloc` levers below. Body: `docs/wip/lz_decompress_extended.base.c`
  (committed; the `nonmatchings/lz_decompress_extended-*/` copies are gitignored).
- **Every global allocno now lands on the ROM's register** (`$t1` count, `$t2` bits, `$t3` src,
  `$t4` word, `$t5` ridx, `$t6` state, `$t7` end, `$t8` ring, `$t9` marker, `$s0` is_final). The
  whole remaining residual is `local-alloc` quantity assignment inside `$v0/$v1/$a0-$a2/$t0`.
- Register-occurrence histograms differ in a way a pure relabelling cannot explain: ROM
  `t6=21 t2=19 t3=16 t0=14 t7=10 t5=10 t1=6 t8=5 t4=5 s0=4 t9=3`, build
  `t1=21 t3=19 t2=14 t6=10 t5=10 t0=8 t4=6 t8=5 t7=5 s0=4 t9=3` (totals equal at 483). The ROM
  shares `$t0` across more local quantities (14 vs 8) and its `src` allocno covers 16 references
  against the build's 14, so both `local-alloc` sharing and the `global.c` order differ.
### How the global order was fixed: `do {} while (0)` as a per-region ref multiplier

`gcc -dg` prints the allocation order directly (`;; N regs to allocate: ...` plus
`;; Register dispositions`), and `global.c` hands out `$t1, $t2, ...` in exactly that order because
every one of these allocnos conflicts with all the others. So matching the ROM's register names is
matching the ROM's *priority order*, and priority is
`floor_log2(n_refs) * n_refs / live_length` (`global.c allocno_compare`, the same formula
`local-alloc.c qty_compare` uses for quantities).

`n_refs` is ref count weighted by loop depth, so **wrapping a region in `do {} while (0)` multiplies
the refs of everything inside it** without emitting an instruction. That makes the order editable
region by region. The build's order started as `state > bits > src > ridx > count > end > ring`; the
ROM's is `count > bits > src > word > ridx > state > end > ring`. Four wrappers reproduce it:

1. the whole back-reference block (`count`/`word`/`ridx`) — takes `count` from 1400 to 4455 and
   lands it on `$t1`, the ROM's register for it;
2. the literal-emit `if` + its `do`-loop — moves `bits` to `$t2` and `state` down to `$t5`;
3. `word = token; if (word == 0) goto done;` plus a **second, nested** wrapper around just the
   `if` — `word` needs exactly 12 refs (3495) to sit between `ridx` (3485) and `src` (3589), an
   11-unit window that 11 refs (3203) misses and 13 (3786) overshoots;
4. the flush loop's *body* only — lifts `end` above `ring`. Wrapping the loop including its
   `src < end` test instead also doubles `src`, which overtakes `bits` and breaks the order.

### Then `local-alloc`, four levers, 132 -> 36 rows

With the global order fixed, everything left was quantity assignment inside `$v0/$v1/$a0-$a2/$t0`.
Four edits, each verified by rebuilding one object:

1. **Give the flush epilogue its own temporaries.** Reusing `out0` / `run_left` / `hist_idx` for
   both the epilogue saves and the emit blocks' walking pointer merges quantities that the ROM keeps
   separate (132 -> 88).
2. **Inline the `state->dst_alt` load into `st.out = ...`** in the else arm — the one place the
   batching rule of edit 4 above does *not* apply (88 -> 76). The same inlining on the continuation
   arm's `dst_start`, or on `hist_base`/`run`, is worse; test each one.
3. **`q->out = q->out + 1;` before the run decrement** in the second `lz_expand` loop, keeping the
   decrement and its test adjacent (76 -> 44, the single biggest step).
4. **Add the ring offset in place** (`ro = ro + ring; *((u16*)ro) = ctrl;`) in both emit blocks, so
   the address `addu` overwrites the `sll` result the way the ROM's does (42 -> 36), plus a load
   reorder in the else arm's prologue (`hist_base`, `hist_idx`, `run`) worth 44 -> 42. Splitting the
   back-reference block's multiply out as well (`ro = ro * 2; ro = ro + ring;`) is worth another
   28 -> 24.
5. **One variable per block wherever the ROM's quantity is block-local.** `out0` used for both the
   continuation arm's `st.out` and the emit blocks' walking pointer makes it a cross-block pseudo,
   which `global.c` places in `$a0` where the ROM has a `local-alloc` quantity in `$v0`; a dedicated
   `init_out` for the arm is 30 -> 28. The same split of `run_left` for the *second* emit block is
   24 -> 20. But the split is not free to over-apply: doing it for `hist_base` too costs 2 rows, and
   splitting `ctrl`/`ho`/`tmp`/`out0`/`off`/`run`/`next_ridx`/`ro` per block is inert or worse.

The permuter, run from the fixed-order body, found a fifth weight lever of the same kind
(`off = ...; lz_expand(q, off, run);` inside each emit block's `if`), worth 168 -> 150 -> 132 rows
applied to both copies. Its deliverable here was the *mechanism*, exactly as the escalation note
predicts.

### The six structural edits that closed the multiset and the ordering

Each was read off the `.s`, not searched for:

1. **Flush epilogue loads first.** Read `st.out` / `st.run` / `st.hist_idx` into locals *before* the
   five `state->` stores. The build otherwise starts that block with a store, which `reorg` steals
   into an annulled `beqzl` (the S317 histogram's extra `beqzl` + extra `sw`); the ROM's block
   starts with a load, which `may_trap_p` refuses to steal.
2. **`done:` as an out-of-line block** placed physically between the `-1` return and `read_word:`,
   reached by `goto done;`. Restores the ROM's `beqz` polarity (the inline `return` gave `bnez`).
3. **`u16 token` / `u32 word` split at the token load.** `token = *src; word = token;` keeps the
   redundant `andi ...,0xFFFF`: the `lhu` cannot fold into the zero-extend while `token` still has
   its own use (`token & 0x1f`). A single `u32 word = *src` folds the mask away.
4. **Prologue loads batched.** Both entry arms read all `state->` fields into locals first and
   assign the `st` members afterwards; the interleaved form serialises load/store pairs onto one
   register, because a store through `state` blocks the next load from hoisting.
5. **`run_left = st.run;` hoisted above the ring store** in both emit blocks, which is where the ROM
   loads it.
6. **Second `lz_expand` loop decrements into a temp** (`next_run = q->run - 1; q->run = next_run;`
   then `while (next_run != 0)`), so the test uses the register (one `andi`) instead of reloading
   after the intervening `q->out` store.

Three further one-instruction order coins fell to statement order: `ridx + 1` computed before the
ring offset in the literal block, `ro = ridx & 0xffff` computed before `ridx + 1` in the
back-reference block, and `count = token & 0x1f; word = word >> 5; count = count + 1;` split so the
shift lands between the mask and the increment.

## Attributed

- The S166 "greg-proven raw-185 register-permutation floor" (two coupled near-tied 3-cycles, Cycle A
  `{ridx, src, dist}` and Cycle B `{param, end, ring}`) was measured on a body that still had six
  structural defects. The class it named — register permutation — is now the *whole* residual for
  the first time, but its specific cycles belong to that older body and are re-derivable, not
  inheritable.
- Permuter re-imported at exact count against this body (`nonmatchings/lz_decompress_extended-3`).

## Re-open checklist

1. Restore the body from `docs/wip/lz_decompress_extended.base.c` and confirm
   282/282 at `-0x18` with `tools/cmpfn.sh`, then confirm the register-normalised streams are still
   identical and `allocno_report.py` still gives the ROM's global order before touching anything.
2. What remains is `local-alloc`, not `global.c`, in four clusters: (a) the continuation arm's
   `hist_base`, which takes `$a1` where the ROM takes `$v1`; (b) the else arm's `hist_idx`/
   `hist_base` load order; (c) the token block, where
   the ROM holds `token` in `$t4` and the zero-extended `word` in `$v0` until the `srl` writes
   `$t4`, and the build has the two swapped. Spent on (a): splitting the `+ 4` into its own
   statement, in three placements (80-114 rows). Spent on (c): a separate zero-extend variable
   (102 rows, and it also breaks the global order by dropping `word` below 12 refs), taking the
   shift from `token` instead of `word`, and reordering the `count` mask against the shift (both
   inert). Merging `token` and `word` into one `u16` variable does give the ROM's *shape* for (c),
   but the merged pseudo has only 5 references at length 99 (1010) and no wrapper moves it past 6,
   so it drops out of the global set entirely and the order breaks (92 rows). (d) the second emit
   block's `run` load, `$a0` against the ROM's `$v0`.
3. `qty_compare` is `global.c`'s formula plus `qty_size`, over `death - birth`, so the levers have
   the same shape — but a quantity is block-local, so a wrapper only moves what is inside its block.
4. Enablers: none. Split already done, all callees placed, no rodata/data carve, name pre-curated.
5. Spent: the S166 safe-passes permuter and lever sweep; the six structural edits above are landed,
   not levers to retry.

## S319 re-open (carry held, restore path repaired)

**The restore path was broken and is now fixed.** `docs/wip/lz_decompress_extended.base.c` as S318
left it held only `lz_decompress_extended` (plus a stray copy of `lz_decompress_dma`): the
`LzHistoryState` typedef and the `static inline lz_expand` helper both lived only in the S318
working tree. Pasting it builds a **189-instruction function at `-0x50`** with an undefined
`lz_expand`, which reads like a collapsed body rather than a missing helper. The base.c in this
directory is now self-contained (typedef + helper + body) and reproduces **282/282 at `-0x18`,
20 rows (cmpfn as of S319)** on the first build. The helper matters: the `-3` tree's copy of
`lz_expand` (its second loop decrements before `q->out = q->out + 1`) gives **52 rows** with the
same decoder body; the `-9` tree's copy (increment first, per structural edit 3) gives 20.

**The two remaining clusters, re-derived from the object.**

1. **The two entry arms' `hist_base` / `hist_idx` quantities** (rows 13-14, 22, 27, 29). In the
   continuation arm the ROM holds `hist_base` in `$v1` and the build in `$a1` (the `sw ...,4($sp)`
   follows the register). In the else arm the ROM's load order is `0x1C, 0x20, 0x1E, 0x24`
   (ring_idx, hist_idx, run, hist_base) and the build's is `0x1C, 0x24, 0x1E, 0x20` — the same
   multiset with `hist_base` and `hist_idx` exchanged. The ROM's load order equals its `st.` store
   order; the build's does not.
2. **The token block** (rows 171-176). The ROM loads the token into `$t4` (the `word` global's
   register), zero-extends into `$v0`, tests `$v0`, masks `count` out of `$t4`, then writes
   `word = $v0 >> 5` back into `$t4`. The build loads into `$v0` and zero-extends into `$t4`, so the
   two quantities are exchanged for the whole block. The ROM's shape needs the zero-extend to be a
   *third* pseudo, distinct from both `token` and `word`.

**Spent this sprint (all re-measured against the 20-row body, none kept):**

- A separate zero-extend temp (`word0 = token; if (word0 == 0) ...; word = word0 >> 5;`) — the
  structurally correct shape for cluster 2 — costs the `word` global its references and breaks the
  whole global order: **88 rows**. S318 measured the same lever at 102 rows on a different body, so
  this is the second body on which it fails for the same reason.
- Splitting the *shift result* instead (`dist = word >> 5;` used by the ring index) to move the
  references onto the new pseudo rather than off `word`: also **88 rows**, and the order breaks at
  the first `move t6,a0`.
- All six permutations of the else arm's three prologue reads (`hist_base` / `hist_idx` /
  `run_left`): three give 20 (no change), three give 22. The emitted load order does not follow the
  source order here — the scheduler pins it — so cluster 1 is not a statement-order question.
- Swapping the `u16 token` / `u32 word` declaration order (birth-order tie-break): inert.

**Then the allocno arithmetic closed cluster 2: 20 -> 10 rows (cmpfn as of S319).** The rule the
sprint proved is that a structural split and its weight compensation are ONE edit, computed before
building, not a split followed by a search:

1. **The ROM's `$t4` is `word`, and the raw token shares it.** `$t4` appears exactly 5 times in the
   ROM (`lhu` token, `andi 0xFFFF`, `andi 0x1F`, `srl`, `subu`). The zero-extend is a *third* pseudo
   in `$v0`; the shift result goes back into `$t4`.
2. **Spell it with ONE variable for the load and the distance plus one zero-extend temp**, not with
   a token variable and a word variable:
   `word = *src; zx = (u16)word; if (zx == 0) goto done; count = word & 0x1f; word = zx >> 5;`
   `word` is `u32`, so the `lhu` lands directly in it, `(u16)word` is the `andi 0xFFFF`, and
   `count` reads the raw value out of the same register the ROM does. The separate `u16 token` is
   gone.
3. **Compensate the reference loss in the same edit.** That spelling leaves `word` at refs 10,
   live_length 105, priority 2857 -- below `ridx` at 3471, so the whole global order shifts and the
   body reads 68 rows. `global.c allocno_compare` is `floor_log2(refs) * refs / live_length`, so the
   requirement is `floor_log2(r) * r / 105 > 0.3471`, i.e. **r >= 13**. Three nested `do {} while (0)`
   wrappers around `word = zx >> 5;` take refs 10 -> 13 (each wrapper adds one loop-depth-weighted
   reference), priority 3714, `$t4`, and the body reads **10 rows**. Four wrappers overshoot (40
   rows); the earlier token/word0 split needed seven wrappers to reach the same register and still
   read 16.

**What is left is 5 diff pairs, all in the two entry arms (cluster 1).** The continuation arm holds
`hist_base` in `$a1` where the ROM has `$v1` (and loads it before `dst_start`, where the ROM loads
`dst_start` first); the else arm has the ROM's registers already (`hist_idx` `$v1`, `run` `$a0`,
`hist_base` `$a1`) but emits the `0x24` load before the `0x20` load. Re-measured and refuted against
*this* body: all six permutations of the three prologue reads (10 or 12 rows -- the order is
scheduler-pinned, not source-order), inlining each of `hist_idx` / `run` / `hist_base` into its `st.`
store (16 / 32 / 30 rows), and a per-arm split of `hist_base` (14 rows -- it *fixes* the continuation
arm's three rows and rotates the else arm's trio instead, which is the first evidence that the two
arms want different quantities and that the split is right but needs its own weight compensation).
`run`'s allocno sits at priority 97058 in `$v1`, so a pure priority edit cannot reorder the else
arm's trio; the next attempt should compensate the split arm rather than re-weight the trio.
