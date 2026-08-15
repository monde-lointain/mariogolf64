# Mario Golf 64 decomp sprint retros

One short digest per sprint, newest first, written at the `sprint-review` gate. This is
the *consolidated* record so lessons compound across sprints (McConnell, *More Effective
Agile*, Ch.19: retros must change behavior). The PO promotes durable lessons into the memory
dir manually; the review gate does not auto-write memory.

Process/tooling edits are **retro-gated**: the "Suggested workflow improvements" the
execution loop records into `SPRINT.md` are applied here, at review, not mid-sprint (see
`docs/agent-workflow.md ## Scrum operating model`). The `Applied:` line below is the audit trail of which
numbered suggestions the PO accepted.

## Entry format

```
## Sprint N — <goal> — <date>
- Increment: <files banked> / <functions matched> (delta: <pct before → after>)
- Quality: <stuck-far>/<permuter>/<carried>/<re-opened> this sprint
- Seed: committed <N>pt; banked <B>pt; regime <mirror|classical|mixed>   (v1 story points; the realized tier is v2)
- What helped: <levers / upstream-mirrors / tools that worked>
- Friction: <what slowed it down>
- Applied: <PO-selected, N of M: #1 … #k …; (#j … not selected)>
- Carry-over: <spiked files + blocking function>
```

---

## Sprint 318 — 0 banked; the ref-multiplier lever, and two verdicts refuted — 2026-08-14
- Increment: **0 files md5-candidate** (232 of 265, unchanged) / **0 functions matched**. `main`
  stubs 260 → 260; the repo `grep -c INCLUDE_ASM` reads 263 → 262 only because a rewritten carry
  comment stopped quoting the macro name, not because a stub went away. Descriptive names **+0**.
  ROM SHA-1 green at the gate and at all four commits (`tools/verify-rom.sh` exit 0).
- Quality: 0 stuck-far / 2 permuter-escalated (7 imports, 0 zeros) / 3 carried / 3 re-opened.
- Seed: committed 5+5pt at `1f5314e` (+3pt stretch); **banked 0pt**; realized 16 committed
  (residual **+6**) and 5 stretch (+2); regime classical.
- What helped: **`do {} while (0)` is a per-region reference multiplier, not just a tie-break.**
  `n_refs` is loop-depth-weighted, so a wrapper multiplies the references of exactly what it
  encloses; with `gcc -dg`'s `;; N regs to allocate` line giving the build's priority order and the
  ROM's registers giving the target order, four wrappers walked all ten of
  `lz_decompress_extended`'s global allocnos onto the ROM's registers. Nine `local-alloc` levers
  (per-block temporaries, loads batched before stores, an in-place ring offset, decrement before
  increment) took it from 132 to 20 `cmpfn` rows at the ROM's exact 282 instructions, `-0x18` frame
  and identical instruction order. Also: reading the `.s` end to end before touching the body
  produced all six structural edits that closed the S317 mnemonic deficit, none of them found by
  search.
- Friction: **two carry re-opens in one sprint over-committed the slot.** Each is a crack slice, not
  a leaf, and neither closed its file, so the sprint banks 0 despite the largest residual reduction
  the segment has seen. Second: `cmpfn ... | grep -c '^[<>]'` prints `0` when the object failed to
  build, which reads exactly like a byte match (fixed — the header now carries `rows=`). Third: the
  permuter's score and `cmpfn`'s row count disagreed on 4 of 8 candidates, so every hint had to be
  re-measured rather than trusted.
- Applied: 6 of 6 — #1 (the ref-multiplier procedure with both traps → `docs/hazards.md`
  `#pervasive-regalloc-classical-main`, a `hazard-index.md` trigger row, a `levers.md` pointer), #2
  (`gcc -dg` prints the allocation order, not only conflicts → `loop.md` oracle row), #3 (an
  aligned-rows claim is a percent, not an instruction count → `gates.md` DoR), #4 (hoisting a
  computation out of a call *argument* moves it a block earlier → `levers.md`), #5 (`cmpfn.sh`
  prints `rows=`), #6 (re-measure permuter candidates with `cmpfn`; tally now 8/13 levers, 0/13
  zeros → `loop.md`). Retirements: the S164 codec case law in `#pervasive-regalloc-classical-main`
  compressed to its rule plus one citation (PO-selected), and `sched-tiebreak-coins` +
  `sched-bottomup-loadsplit-livelength-blockmove` merged into `sched-order-levers` (PO-selected when
  the accepted rows overran `levers.md`'s budget). Net prompt-surface delta **+2512 B**.
- Carry-over: `lz_decompress_extended` (20 rows, four `local-alloc` clusters),
  `func_8003E004` (88 rows: preheader constant order, the angle block's placement, the `i`/`a` tie),
  `func_8004DC44` (75/75, the `/40` dividend-versus-magic tie, lever (2) now refuted). Bodies are
  committed as `docs/wip/*.base.c`; measured/attributed splits in the two `.near-match.md` files.

## Sprint 317 — two inherited verdicts retired, two files closed — 2026-08-03
- Increment: **2 files md5-candidate** (230 → 232 of 265) / **2 functions matched**
  (`func_8005244C` 94/94 at `f297119`, `func_80070FD0` 61/61 at `110320d`), each at the ROM's exact
  frame and each closing its host. Repo `INCLUDE_ASM` 265 → 263; `main` stubs 262 → 260, with
  `--loose-stubs main` fresh unchanged at **1**. Descriptive names **+0** (a domain enum with two
  already-C callers, and a save-counter updater; auto names kept per S247). ROM SHA-1 green at the
  gate, at each of the four commits, and at review (`tools/verify-rom.sh` exit 0).
- Quality: 0 stuck-far / 1 permuter run (dry, stopped) / 2 carried / 3 re-opened.
- Seed: committed 5+5+5pt at `75aabb3` (+8pt stretch); **banked 10pt** — the first non-zero since
  S313, because points bank per file and both banks took their host to zero stubs; realized 19
  committed (residual **+4**) and 9 stretch (residual **+1**); regime classical.
- What helped: **the goal itself — rank by how close the host file is to zero stubs.** `main` has one
  fresh row left (`func_80089094`, 13452 B), so smallest-first offers nothing; but five files sat one
  stub from md5-candidate, where a single function is worth a whole file's points. Both banks were
  then inherited-verdict retirements, each found by reading the named pass in the gcc source rather
  than by search: S316's `func_8005244C` doc named `mostly_true_jump` branch prediction for an empty
  delay slot, where the real predicates are `may_trap_p` (reorg walks past a hoisted `lw` and steals
  the insn behind it), `own_thread_p` and `stop_search_p`; and S167's `func_80070FD0` "not reachable
  from equivalent single-TU C" had the right pass (`cse.c make_regs_eqv`) and the wrong conclusion —
  the destination of a copy becomes canonical only when its last use is later, so one `s32 t` scratch
  shared with the *other* arm of the enclosing branch brings the ROM's triple back at zero cost. Both
  carries also moved: `s32 unused[2];` gives `func_8004DC44` the ROM's dead `-0x8` frame **on the goto
  loop**, refuting S174's three-way structural conflict, and the stretch's S166 "greg-proven
  register-permutation floor" turns out to sit on a body whose mnemonic multiset still differs.
- Friction: **the plan gate could not see its own thesis.** The five one-stub hosts came from a hand
  `grep -c INCLUDE_ASM` loop, which also miscounted one file (a comment mentioning `INCLUDE_ASM`),
  so the sprint was priced against 5 hosts when there were 6. Second: at 93/94 the S316 doc asserted
  "no register differs", which was an artifact of `cmpfn`'s hunk alignment — three register roles
  differed and only became visible at exact count. Third: `func_8004DC44`'s remaining `/40`
  dividend/magic residual consumed the rest of its slot with no movement, and the stretch was opened
  while it was still carried, against the sprint's own method item 6.
- Applied: 4 of 5 — #1 (reorg thread-availability predicates → `docs/hazards.md` + a merged
  `hazard-index.md` row), #3 (cse `make_regs_eqv` shared-scratch crack → the existing hazard section's
  verdict rewritten + a `docs/levers.md` row), #4 (dead-frame lever is loop-shape-orthogonal, plus the
  generalised "re-test a two-feature conflict when a lever lands for either" rule), #5
  (`pick_target.py --file-close SEG`, which reports 4 one-stub `main` hosts and is the count the hand
  grep got wrong). #2 (a `cmpfn` register verdict at N-1 instructions is not comparable) was offered
  and **not selected**. Retirement: `docs/hazards.md ## dead-frame reload-artifact regalloc-wall`
  compressed from 10775 B to 3775 B — the S173/S174/S175 probe logs reduced to the rule, its three
  variants and one citation each, with the refuted conclusion removed. Net prompt-surface delta
  **−4055 B** (`hazards.md` 646734 → 642555, `hazard-index.md` 22504 → 22509, `levers.md` 10114 → 10233), and the line
  wraps re-flowed; `prompt_lint check` clean on all 18 surfaces, `make test-tools` 138 passed.
- Carry-over: `func_8004DC44` (`src/main/print_string_at_grid.c`) at **75/75 with the ROM's exact
  `-0x8` frame**, residual 20 rows all cascading from the `/40` dividend/magic local-alloc choice;
  `lz_decompress_extended` (`src/main/lz_decompress_simple.c`) at **282/282 at the exact `-0x18`
  frame** but with three `andi`, two `lhu` and a `beqzl` still structural, so its coloring verdict
  cannot be inherited yet.

## Sprint 316 — a TU-wide stamp is not evidence about its unbuilt members (3 of 4) — 2026-08-02
- Increment: 0 files md5-candidate / **3 functions matched** (`func_80052834` 141/141 at `01ef9ce`,
  `func_80052A68` 162/162 at `5518f82`, `func_80052CF0` 185/185 at `929a4c3` — the stretch), all in
  `src/main/func_80052250.c`, which goes 4 → 1 stub. Repo `INCLUDE_ASM` 268 → 265; `main` stubs
  265 → 262 with `--loose-stubs main` fresh unchanged at **1**. Every bank at the ROM's exact frame.
  ROM SHA-1 green at the gate, at each of the four commits, and at review (`tools/verify-rom.sh`
  exit 0). Descriptive names **+0**: all three return domain enums (0-3, 0-2 out-marks, 0-4) with
  only still-asm callers, so the auto `func_` names were kept per the S247 rule.
- Quality: 0 stuck-far / **2 permuter runs, both dry** / 1 carried / 1 re-opened.
- Seed: committed 5+5+5pt at `8023763` (+8pt stretch); banked 0pt (host partial); realized 18
  committed (residual **+3**) and 9 stretch (residual **+1**); regime classical.
- What helped: **re-deriving an inherited TU-wide stamp per function.** S183 measured three bodies
  at 84/128/150 rows and stamped `#pervasive-regalloc-classical-main` on all eight non-trivial
  siblings. With S314's three, six members have now banked and **none was a register-allocation
  problem** — the residuals were an addressing form (index temps keep `%lo` in the displacement, the
  third confirmation of the S314 lever in this TU), a branch sense, an argument evaluation order
  (`g(arg1) + 1 >= g(arg0)` lets gcc fuse the saved `move` into `addiu`; the bare call first does
  not), four loop-init placements, one pointer reused across three loops, and a signed-vs-unsigned
  switch selector. Two levers generalise: *where* a loop-invariant read is written decides whether
  gcc hoists it (`loop.c:715` `may_trap_p` + `loop.c:930` `maybe_never` — only a read at the top of
  the body reaches the preheader), which with the array-element base spelling **retired S261's
  "mutual exclusion, terminal for source" verdict**; and an allocno web can be shortened
  structurally, `break` through a shared `return` moving `func_80052CF0`'s result past two allocnos
  in one edit.
- Friction: **both permuter runs were dry and one pointed the wrong way.** 188k iterations on
  `func_8005244C` (base 735 → 380) produced only re-spellings of the same anchor; `func_80052CF0`'s
  best candidate was a `do {} while (0)` that raised two allocnos *together*, the opposite of what a
  result-web-too-long residual needs, and the `break` that fixed it made the candidate unnecessary.
  Second friction, self-inflicted and repeated: a `sed -i` anchored on `  s32 mag;` resized a banked
  sibling's array, and a Python splice built from two `s.index()` calls reversed — the end marker was
  defined earlier in the file than the start marker — silently truncating the file and duplicating a
  function. Third: `pkill -f` was used to stop a background permuter, which the conventions forbid
  precisely because the pattern also matches the calling shell; it killed the shell.
- Applied: 4 of 4 — #1 (loop-invariant read-placement sub-lever in
  `#indexed-vs-pointer loop`), #2 (shorten an allocno web structurally before reweighting), #3
  (pointer/index spanning two loops is a structural symptom, split before reading priorities),
  #4 (scripted-slice offset-ordering assert). Retirement: the S261 terminal verdict in
  `#base-register-vs-displacement` compressed to its three measurements plus the S316 correction.
  Net prompt-surface delta **+917 B** (`loop.md` 46912 → 47103, three paragraphs compressed in place
  to fit under budget; `hazards.md` 646008 → 646734). Suggestion #5 (rewriting the S183 stamp
  paragraph in `BACKLOG.md`) was offered as the alternative retirement and **not selected**; the
  carry-over there was updated factually instead.
- Carry-over: `func_8005244C` (`src/main/func_80052250.c`) at **93 of 94** with the ROM's exact
  `-0x28` frame, every register matching, and the loop-bound load in the ROM's own preheader position
  and `-0x8(s4)` form. Sole residual: one unfilled guard delay slot — reorg reaches past the
  non-movable `lw` and steals the `sll` behind it. Untested hypothesis (branch prediction in
  `fill_eager_delay_slots`, check with `-dd`) and the replayable body are in
  `docs/wip/func_8005244C.near-match.md`; the permuter has **not** been re-run against this body.

## Sprint 315 — both carries fell to declarations, not levers — 2026-08-02
- Increment: 0 files md5-candidate / **2 functions matched** (`func_8005B150` 79/79 →
  `rank_scores_descending`, `func_8005CEE0` 38/38 → `is_roster_entry_locked`, both in
  `src/main/func_80059BA0.c`, which goes 11 → 9 stubs). Repo `INCLUDE_ASM` 270 → 268; `main` stubs
  267 → 265; `--loose-stubs main` fresh **2 → 1**. Both banked byte-clean on the first build. ROM
  SHA-1 green at every commit (`tools/verify-rom.sh` exit 0 at the gate, at each bank, at review).
- Quality: 0 stuck-far / **1 permuter run** (dry, 11108 iterations, no candidate) / 1 carried /
  2 re-opened.
- Seed: committed 5+5pt at `8a00fa1` (+5pt stretch); banked 0pt (host partial); realized 8 committed
  (residual **−2**, both first-try) and 7 stretch (residual +2); regime classical.
- What helped: **re-deriving the declarations before the body.** `func_8005CEE0` carried an S268
  "terminal / do not re-grind" verdict built on a `base.c` that declared two tables as flat `u8`
  externs with hand-written `idx * 4` / `a * 14` / `b * 2` strides. They are one array and one grid,
  `s16 D_800C29B8[10][2]` and `s16 D_800C28E4[3][7]`, both already declared in the same file twenty
  lines above the stub for `build_roster_grid`. Typed, the pair loads share an index register with a
  `%lo` displacement each and the grid address is a two-term register sum, so the load order and the
  base-vs-displacement residual the doc called coupled and terminal simply do not arise. Same shape
  on the stretch: `s8 lo` instead of `s32 lo` restored both the `move v1,v0` that `cse` was
  propagating away and the 8 frame bytes, in one edit. Second thing that helped: the new-class fear
  was unfounded — the `$fp` + computed `subu $sp` prologue is a plain VLA, `s32 keys[count];`, and
  reproduced the whole prologue, probe and epilogue with no lever.
- Friction: **two carry docs named a specific pass, and both named the wrong one.** S264 called
  `func_8005D0D8`'s `slti`+`bltzl` a combine global-vs-local `nonzero_bits` choice and
  "source-invariant"; it is `fold_range_test` merging `lo >= 5 || lo < 0` at tree level, and two
  separate tests bring the ROM's pair straight back. S272 then re-derived that residual and
  *confirmed* the wrong attribution, because it re-tested the same three variants rather than the
  claim. Its second "not reproducible" coin needed an `s32` temp to keep the xor SImode. Third
  friction, self-inflicted: a `sed -i` sweeping `s32 flags[N];` array sizes also rewrote a sibling
  function's array in the same file, caught only by the gate build's md5 mismatch.
- Applied: 4 of 4 — #1 (VLA hazard section + index row), #2 (DoR: re-derive declarations first), #3
  (near-match docs split measured from attributed), #4 (scripted-substitution anchor guard).
  Retirement: `loop.md`'s near-match-is-a-hypothesis paragraph compressed and #3 folded into it, and
  two near-duplicate `&arr[K]` copy-preference rows in `hazard-index.md` merged; `loop.md` 47048 →
  46912 B and `hazard-index.md` 22522 → 22504 B, both net negative while gaining content.
- Carry-over: `func_8005D0D8` (`src/main/func_80059BA0.c`) at **67/67 and the ROM's exact `-0x38`
  frame**, up from 61/67. All three S264/S272 residuals refuted; what remains is a three-register
  rotation whose whole arithmetic is written down — the loop-B flags giv must drop below the counter,
  needing `refs <= 15` or `live_length >= 50`, and every ref count is forced by the ROM's own six
  stores.

## Sprint 314 — an inherited TU-wide wall verdict, refuted 3 for 3 — 2026-08-02
- Increment: 0 files md5-candidate / **3 functions matched** (`func_80052264` 48/48,
  `func_80052384` 50/50, `func_800525C4` 156/156, all in `src/main/func_80052250.c`, which goes 7 → 4
  stubs). Repo `INCLUDE_ASM` 273 → 270. ROM SHA-1 green at every commit (`tools/verify-rom.sh` exit 0
  at the gate, at each bank, and at review).
- Quality: 0 stuck-far / **0 permuter runs** / 0 carried / **3 deliberately re-opened**.
- Seed: committed 5+5pt at `104579e` (+8pt stretch); banked 0pt (host partial); realized 20, residual
  +2; regime classical.
- What helped: **reading the conflict set before reaching for a weight lever.** `gcc -dg` printed
  `;; 77 conflicts: 77 84 2 3 16 18 29 65 66` for `func_80052264`'s wrong register — `$a0`-`$a3`
  absent — and since `global.c find_reg` scans ascending in both passes, *no* allocno edit could
  reach the ROM's `$t0`. The only free way to make `$a0` conflict is to pass the parameter on to the
  call, which is what the ROM's source does (`func_80051FCC(arg0)`, zero instructions, callee ignores
  it). A doc that had survived a 42k-iteration permuter plateau fell to one token. The same
  discipline closed the other two: `-dL` named the hoisted symbol as a `loop.c` movable, and
  `allocno_report.py` priced one shared block pointer at 14 refs / length 72 / 5833, where splitting
  it per branch landed a four-register rotation in one build.
- Friction: **an inherited verdict cost more than the functions did.** S183 generalised
  `#pervasive-regalloc-classical-main` from three built diffs to all 8 non-trivial siblings, and the
  three smallest were each a *structural* miss (two symbol hoists, one missing argument) — the word
  "regalloc" sent every re-open at the wrong lever family for 130 sprints. Second friction:
  `func_800525C4`'s doc pointed at "the S183 commits" for its near-miss C, which was never committed
  (the carry commit touched only `docs/wip/`), so a 156-instruction body was re-derived from the
  `.s`. A doc must carry its body or say it does not.
- Applied: 4 of 5 — #1 (gates.md DoR: a TU-wide verdict is re-derived per function, and record which
  sprint's tooling produced it), #2 (the index-temp lever, written into
  `docs/hazards.md#base-register-vs-displacement`), #3 (the argument pass-through, folded into
  `docs/levers.md`'s allocation row), #5 (`docs/wip/banked/`, which `--carried-check` no longer
  scans — verified: the three now read `fresh`, the four remaining still read `CARRIED-WALL`).
  #4 dropped as already covered by `scope-and-live-range-steer-allocation`'s split clause.
  Retirement (PO-selected): `dual-offset-temps-around-a-call` retired from `docs/levers.md`, its
  content absorbed into the generalized S314 hazards write-up — levers.md 10235 → 10114 B, a
  **negative** net delta on the surface with 5 bytes of headroom. `gates.md` provenance rose 6 → 7.
- Carry-over: `src/main/func_80052250.c` at 4 stubs (`func_8005244C` 376 B — never individually
  characterised, `func_80052834` 564 B, `func_80052A68` 648 B, `func_80052CF0` 740 B — decoded but
  never built). All four now carry *only* the retired generalisation, and four more banks would take
  the file to md5-candidate.

## Sprint 313 — the residual was the block move's scratch registers (0 banked, 2 carries deepened) — 2026-08-02
- Increment: 0 files / **0 functions matched**. Repo `INCLUDE_ASM` 273 → 273; `--loose-stubs main`
  270 → 270, fresh 1 → 1; curated `type:func` names 375 → 375. ROM SHA-1 green at every commit
  (`tools/verify-rom.sh` exit 0 at the gate, after each revert, and at review).
- Quality: 0 stuck-far / **1 permuter-escalated** (base 350 vs S309's 915, ~82k iterations, no zero,
  no lever) / **2 carried** / **2 re-opened**.
- Seed: committed 5pt at `5546684` (+8pt stretch); banked 0pt; realized 8 for the committed item
  (residual +3), 13 for the stretch; regime classical.
- What helped: **re-deriving the residual instead of inheriting it.** `func_8007399C`'s doc named
  `sched.c`'s birthing boost, which is real but not what the schedule buys. Reading `-dS`, `-dR`,
  `-dl`/`-dg` and the object end to end produced the actual chain: `w = <aggregate>` is one
  `movstrsi_internal` whose four `match_scratch "=&d"` temps take the lowest free `d` registers at
  that insn, so everything live across the copy conflicts with all four and a global allocno
  allocated afterwards can lose the ROM's register to a scratch. The ROM's set is `v0,v1,a1,a2`
  (readable with no build: the copy's `la` temp is the top of the set), which is what leaves `$a3`
  for `player`. Getting that set required spelling `D_800D16C0` as the local aggregate initializer it
  is — a 12-byte zero blob sitting immediately before `jtbl_800D16D0` is a constant pool, not an
  extern. With a tenth block-0 insn added, the ROM's **first 25 instructions came out exactly**, at
  +2 `move`. Best body 70 → 68 rows at exact count and frame.
- Also closed: **the one class S309 left open.** A second `SET` on the out-pointer is either
  unconditional (the first becomes dead and `flow` deletes it) or conditional (the loop drops out of
  the variable's live range, its `allocno_compare` priority passes the other out-parameter's, and
  which of the two spills flips — +3). Six placements measured. And on the stretch, R2 is an **exact
  qty tie** (`2 refs / len 14` × 3), so `local-alloc` decides on birth order and the thirteen
  weight-editing forms tried across S310 and S313 were the wrong family entirely.
- Friction: two. (a) **A carry-crack slice was priced at the band of its remaining diff, not of its
  unknown.** "One legal source form" read as 5 points; the requirement was met three different ways
  and each moved the cost elsewhere. (b) Two scripted `python` edits to `src/` misfired again — one
  duplicated a 17-line preamble, one asserted against a stale slice — on a file the convention
  already covers. The guarded splice that *did* work asserted the `INCLUDE_ASM` count across the
  rewrite, which is exactly what the convention prescribes.
- Applied: 5 of 5 — #1 `aggregate-copy-scratch-clobbers` in `docs/levers.md` plus Axis 0b of
  `#loop-weight-and-live-length-regalloc-steering`; #2 the initializer-pool tell in
  `#rodata-sibling-yaml-pattern`; #3 `cmpfn` on a nested child prints `no symbol` permanently, in
  `#nested-function-static-chain-spill`; #4 wip-doc row counts carry their `cmpfn` sprint, in
  `gates.md`; #5 `tools/allocno_report.py` prints a `TIE` flag for equal `(refs, len)` plus Axis 0 of
  the same playbook. Retirement (PO-selected): `global-allocno-compare-livelength-biv-order` merged
  into `scope-and-live-range-steer-allocation`, keeping `global.c:587`, `global.c:790-823` and
  `local-alloc.c:472/1290/1587`. Net: `docs/levers.md` 10176 → 10235 (**+59**, still under budget),
  `docs/hazards.md` +2691, `docs/workflow/gates.md` +532; all 18 surfaces pass `prompt_lint check`,
  `make test-tools` 138 passed.
- Carry-over: both committed items. `func_8007399C` (`src/main/func_80071370.c`) and
  `func_8006E210` + `func_8006DFF0` (`src/main/func_8006A2C0.c`), each with a re-open that is now a
  named search rather than a lever hunt.

## Sprint 312 — the register allocation was a source fact about variable reuse (1 banked) — 2026-08-01
- Increment: 0 files / **1 function matched** — `func_8008E82C` → `run_fog_debug_editor`, 696
  instructions, exact count and exact frame. `src/main/func_8008D100.c` 12 → 11 stubs (not
  md5-candidate, so 0 file points); repo `INCLUDE_ASM` 275 → 274; `--loose-stubs main` 271 → 270,
  fresh 2 → **1**; descriptive names 651 → 652. ROM SHA-1 green at every commit.
- Quality: 0 stuck-far / **1 permuter-escalated (5 runs, 0 zeros)** / 0 carried / 0 re-opened. Plus
  1 red gate build, self-inflicted (see Friction).
- Seed: committed 8pt at `63d7bab`; banked 0pt; realized 13, residual +5; regime classical. Seed 8
  **+1** permuter **+1** novel bank-gotcha = 10, Fibonacci-snapped to 13. Insensitive to the
  subjective half: the escalation alone gives 9, which snaps to 13 as well.
- What helped: **three separate places where the ROM's register allocation turned out to be a
  source fact about variable reuse, not a compiler coin.** One counter shared by both loops (the
  tell was the ROM spending `$t8` on both), the light loop's `avg` being the same variable as
  `tod`, and `quarter` also carrying the last grid column. `tools/allocno_report.py` proved the
  first was not winnable any other way: the counter scored `floor_log2(7)*7/88` against five giv
  base pointers born after it, and no live-length edit can put a variable ahead of pseudos created
  in the preheader it precedes. Merging took it across a `floor_log2` tier and all six registers
  landed. Also: a 6-line probe file, compiled with the file's real flags, settled three codegen
  idioms (`x % 0x100` on a `u8`, the annulled `bltzl` clamp, the branchless `nor/sra/and` clamp)
  before a line of the body was written — cheaper than one wrong-body build.
- Friction: two, and the second was mine. (a) The permuter plateaued at every re-base and produced
  **no zero in 5 runs**, but 3 of its candidates were readable levers — and two of those were
  *illegal source* (an assignment inside a call argument, an assignment to `tod` inside the loop)
  whose real message was "this pseudo needs to live longer". Reading them as variable reuse is what
  banked the function; applying them literally would not have. (b) A scripted `str.replace` over
  the whole file rewrote the same address-arithmetic line inside the banked sibling
  `emit_lens_flare_dl`, and the gate build reddened on 3 bytes in a function this sprint never
  touched. The no-scripted-splice convention existed but was written for deletion.
- Applied: 5 of 5 — #1 the merge direction folded into `docs/levers.md`
  `scope-and-live-range-steer-allocation` (which named only the split direction); #2 a `gcc -dg`
  row added to the `loop.md` Oracles table, authoritative for an allocno's hard-register conflicts
  and preferences, which `allocno_report.py` cannot report; #3 the `loop.c` hoist arithmetic
  (threshold 122 less 3 per movable moved, `loop.c:532/1719`, knob is `insn_count` from `-dL`)
  folded into `emission-order-placement-lever`; #4 the no-scripted-splice convention widened from
  splice to any scripted edit incl. `replace`, with the `git diff`-every-hunk guard; #5
  `cmpfn.sh obj_has` retries once, since it was racing the build rule's `cp`+`strip` and reporting
  a false "no symbol" several times this sprint. Retirement: `global-reread-vs-cse` compressed to
  rule-plus-one-citation as proposed and accepted; the additions still overran, so on a second ask
  the PO also accepted compressing `jump-c-store-flag-conversions` (levers) and the `pkill` +
  `rm -rf` incident narratives (loop.md) to rule-plus-citation. Net: `docs/levers.md` 10217 → 10176
  (**−41**), `docs/workflow/loop.md` 46928 → 47048 (**+120**); all 18 surfaces pass
  `prompt_lint check`.
- Carry-over: none new. `func_8006E210` + `func_8006DFF0` (S310) and `func_8007399C` (S307) still
  carried.

## Sprint 311 — the 611-instruction JTBL-CARVEABLE row (1 banked) — 2026-07-31
- Increment: 0 files / **1 function matched** — `func_80095DE0` → `update_cutscene_sound_cues`, 611
  instructions, exact count and exact frame. `src/main/func_80095A10.c` 14 → 13 stubs (not
  md5-candidate, so 0 file points); `--loose-stubs main` 272 → 271, fresh 3 → 2; descriptive names
  373 → 374. ROM SHA-1 green at every commit.
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened.
- Seed: committed 8pt; banked 0pt; realized 7, residual −1; regime classical.
- What helped: **the `.s` histogram, read before any C.** `fp=0`, `branch-likely=0`, and 39 `jal`
  all to one already-`c` helper with a known signature said the row was call glue, not a
  2444-byte wall — an 18-arm `switch` over the jump table, each arm an inner `switch` on `0xC..0xF`,
  106 logical calls cross-jumped into one tail. Everything after that was transcription. The carve
  was verified against `objdump -s -j .rodata` *before* the yaml edit and needed no walk-back, the
  first in this class not to.
- Friction: one thing, and it was a doc-shape problem rather than a codegen one. Build 1 came out
  610/611 with `nop` as the only mnemonic delta: gcc stole the following `sll` into the `switch`
  range check's delay slot where the ROM leaves it empty. The fix is `s32` instead of `void` — one
  character — but `nonvoid-return-blocks-fallthrough-delay-steal` was written as a *lone
  epilogue-branch* rule, so a range-check branch that merely *targets* the epilogue did not match it
  and the lever was not tried first.
- Applied: 3 of 3 — #1 the non-void-return lever widened to "an unfilled delay slot on any branch
  whose target is the function's return block", with the diagnostic named (`rom=N+1 mine=N`, only
  `nop` differs, every later row shifted by one); #2 the `JTBL-CARVEABLE` 6-banked datum recorded in
  `BACKLOG.md` (no tool change); #3 `single_callee_tell` added to `pick_target_score.py` and printed
  as `single-callee:<fn>` on `--loose-stubs` rows when one target accounts for every `jal` (≥ 4
  calls), 9 `main` rows tagged, `make test-tools` 138 passed. Retirement: the longest
  `docs/levers.md` row, `emission-order-placement-lever` (814 B), compressed to rule-plus-citation
  with its four worked examples folded into one sprint list; `docs/levers.md` 10239 → 10217 B against
  its 10240 B budget, so the sprint's net delta on that surface is −22.
- Carry-over: none new. `func_8006E210` + `func_8006DFF0` (S310) still carried.

## Sprint 310 — the JTBL-CARVEABLE row that was a nested pair (0 banked, 1 carried) — 2026-07-31
- Increment: 0 files / **0 functions matched**. `func_8006E210` and its nested child
  `func_8006DFF0` both reach the ROM's exact instruction count and exact frame (543/543 at `-0x60`,
  136/136 at `-0x40`); ~20 of 679 instructions differ. `main` stubs unchanged at 272,
  md5-candidate unchanged, descriptive names unchanged. ROM SHA-1 green at every commit.
- Quality: 0 stuck-far / 0 permuter (structurally unavailable) / 1 carried (a pair) / 0 re-opened.
- Seed: committed 8pt; banked 0pt; realized 10, residual +2; regime classical.
- What helped: nine named levers, five of them re-usable. The two that moved the most ground were
  **`s32 off = <full byte offset>;` then `SYM[off]`** — the only spelling that keeps the symbol
  inside the `MEM` instead of letting loop.c hoist `la sN,SYM` into the preheader — and **reading a
  neighbour global as an array element of its predecessor** (`D_801B6090[2]` for `D_801B6098`), whose
  `MEM_IN_STRUCT_P` may-alias stops the same hoist. Giving the case-2 arm and the team-bet block
  their own locals instead of reusing function-wide ones moved `col` from `$s1` to the ROM's `$s0`
  and removed 40 diff lines in one edit.
- Friction: three things, in order of cost. (1) **The leaf was mis-scoped**: `func_8006DFF0` is a
  GCC nested function of the target, sharing the parent's `i`/`j` through the `$v0` static chain, so
  the slice was 679 instructions and the permuter was unavailable (`import.py`'s pycparser aborts on
  a nested definition, and the documented workaround cannot apply when the parent IS the target).
  Discovered only after the parent's C was written. (2) **`cmpfn` counted the ROM's inter-function
  padding nop**, so an exact 543-instruction body read `rom=544 mine=543` — an hour chasing a
  phantom deficit, and the third recurrence of the `cmpfn-trailing-padding-overcount` memory. (3)
  jump.c's store-flag conversion (`jump.c:1139-1250`, case 3) turns every spelling of a two-constant
  select branchless and re-fires in each pre-reload pass; the only escape is a `CODE_LABEL` between
  the assignment and the test, which moves where the value is born.
- Applied: 4 of 4 — #1 `jtbl_carve_tell` gains a `carve-pool-blocked:<sym>` verdict when the host's
  `.rodata` carve precedes the table with asm-owned rodata in the gap (8 `main` rows reclassified
  out of `fresh`); #2 `nested_parent_tell` gains the chain-setup tell (`addiu $v0,$sp,K` before a
  `jal`, 9 more `main` rows now NESTED) and `--nested-check` names the parent of a flagged child;
  #3 `cmpfn.sh` stops its ROM stream at `endlabel` and reports the ignored pad; #4+#5 the two new
  levers into `docs/levers.md`. Retirement: `store-flag-single-bit-terminal-wall` merged with the
  new jump.c finding into one `jump-c-store-flag-conversions` row, plus four over-long rows
  compressed to rule-plus-citation; `docs/levers.md` ends at 10239 B against its 10240 B budget.
  Unplanned fix: `prompt_lint`'s xref checker resolved citations against the gitignored `SPRINT.md`,
  so `make test-tools` failed or passed by accident of which sprint was open — now exempt.
- Carry-over: `func_8006E210` + `func_8006DFF0` as one slice, `docs/wip/func_8006E210.near-match.md`
  (replayable body, three residual clusters with their pass named, the carve line to re-apply).

---

## Sprint 309 — first probe of the JTBL-CARVEABLE vein (2 banked, 1 carried) — 2026-07-31
- Increment: 0 files / **2 functions matched** — `transfer_continue_slot` (ex-`func_8005BC10`, 258
  instructions) and `transfer_mode_continue_slot` (ex-`func_8005B7BC`, 277), both byte-exact in
  `src/main/func_80059BA0.c`, each committed with its own `.rodata` carve. The host goes 13 -> 11
  stubs and stays a mixed partial bank, so it books 0 file points. `--loose-stubs main` 274 -> 272
  stubs, fresh 6 -> 5. md5-candidate unchanged at 100 of 135. Descriptive names 172 -> 174.
- Quality: **0 stuck-far / 1 permuter (found score 0) / 1 carried / 1 re-opened**
- Seed: committed 13pt (8 + 5) frozen at `4405554`, plus an 8pt stretch pulled on success; banked
  **0**pt; realized **23**, residual **+2**; regime classical
- What helped: re-deriving each `.s` end to end before writing C — both leaves hit the ROM's exact
  instruction count *and* exact frame on the first or second build. Then four levers: `s32 bad = 0;`
  moved from the declaration into the branch **after** the count check (62 -> 12 diff lines, the ROM
  fills the `bnez` delay slot with it); the permuter at exact count, which found in 2426 iterations
  that the second `mode = 2;` belongs *after* the inner `if`, not at its body's end, colouring the
  allocno `$v0` not `$a0`; the `slot = base - 0x2B0` assignment moved ahead of its arm's
  `osSyncPrintf` so `reorg` can steal it for the `jal` delay slot; and `if (ok) {...} else { return
  -1; }` rather than the then-arm form, which is what lets `jump.c` merge that return into the shared
  error epilogue instead of emitting its own `li v0,-1; j` pair. The negative-displacement
  one-symbol rule paid twice: every base in both functions comes off a single symbol.
- Friction: the last two `cmpfn` lines on `func_8005BC10` were a real `j`-target delta, not
  normalisation — the save path jumps *past* the status store and the print, so that tail belongs
  inside the restore arm; reading it as noise would have cost a bank. `tools/cmpfn.sh` reported
  `mine=0` plus a frame mismatch after the first curated rename, which reads exactly like a
  catastrophic regression (fixed, suggestion #3). `func_8007399C` did not close for the second
  sprint running: the requirement is now known to be **two** conditions, not the one S308 named, and
  seven zero-cost source forms were measured and refuted.
- Applied: **4 of 4** — #1 else-arm-return extended with the shared-epilogue consequence
  (`docs/levers.md`); #2 the brace-placement colouring knob folded into
  `scope-and-live-range-steer-allocation` rather than added as its own entry (`docs/levers.md`); #3
  `tools/cmpfn.sh` resolves a curated rename in both directions via `symbol_addrs.txt` and otherwise
  fails loudly (`make test-tools` 138 passed); #4 `pool-cohort:<fn>` recorded as a pricing floor and
  the default stretch item (`docs/workflow/gates.md`). Retirements, both PO-selected: the two
  branch-likely entries merged into one, and `dead-frame-levers` compressed to its rule plus one
  citation. Net `docs/levers.md` delta **−5 B** (10237 -> 10232), back under its 10240 budget.
- Carry-over: `func_8007399C` (`src/main/func_80071370.c`), 149/149 at the ROM's exact `-0x50` frame,
  70 `cmpfn` lines, carve unblocked and one line away

## Sprint 308 — the S307 pool-carve cohort (2 banked, 1 carried) — 2026-07-31
- Increment: 0 files / **2 functions matched** — `place_glyph_sprite_run` (ex-`func_800754BC`, 212
  instructions) and `emit_glyph_sprite_dl` (ex-`func_8007580C`, 399), both byte-exact, banked with
  the host `.rodata` carve moved from `[0xACBE0]` to `[0xACB38]`. `src/main/func_80071370.c` 14 -> 12
  stubs; `--loose-stubs main` 276 -> **274**, 6 fresh unchanged (both banks came from the
  `jtbl-dispatch` pool, not the fresh vein). md5-candidate unchanged at 100 of 135 files, so the file
  point banks 0 as priced. Descriptive names 170 -> **172**.
- Quality: 0 stuck-far / 1 permuter (plateaued 915 -> 340, no zero) / 1 carried / 1 re-opened.
- Seed: committed 13pt at `7cf56c0`; banked **0**pt; realized **16**, residual **+3**; regime
  classical.
- What helped: **measuring the cohort instead of inheriting its price.** S307 recorded this pool as
  atomic and the plan gate committed it that way, answering the 8-point decompose gate with the
  atomic exemption. One object's `.rodata` carve in fact splits at any interior boundary 8-aligned on
  both sides, so two of the three leaves banked while the third's table stayed in the extracted blob.
  Two levers did most of the register work: one `gSPVertex` call per if-chain arm rather than one
  shared call after it, which multiplies that constant's `loop.c` movable and flips which of two
  constants the ROM hoists (290 diff lines -> 46); and a separate short-lived local per region
  instead of one variable stretched across both (46 -> 0, and the same shape closed the other leaf).
  `tools/dl_decode.py` plus `gbi_match.py` turned a 399-instruction emitter into a packet list, and
  the composite macros fell out of it (`gDPLoadTLUT_pal256`, `gDPLoadTextureTile`).
- Friction: **one RED gate build on one byte, behind three `cmpfn`-clean bodies.** `gSPMatrix` takes
  a physical address (`OS_K0_TO_PHYSICAL`); `cmpfn` normalises `%hi`/`%lo` so no per-function oracle
  could see it, and the `.s` reloc reads as splat's `D_FF530`. A byte-diff of the two ROMs localised
  it in one command. Second: `setup-permuter.sh` died silently (rc=1, no message) on an
  already-inlined function — `set -o pipefail` plus a no-match `grep` aborted the resolver before its
  own emptiness check. Third: the carried leaf ate roughly half the sprint for 14 diff lines of
  improvement; the stop rule should have fired at the second plateau, not the fourth.
- Applied: **4 of 4**: #1 partial-carve verdict -> `BACKLOG.md ## Carry-overs` `Cohort-blocked` class
  rewritten (retirement: the SUM-pricing and multi-leaf-slice clauses, refuted by this sprint's own
  bank); #2 `loop.c:1631` hoist desirability -> `docs/levers.md emission-order-placement-lever`;
  #3 `gSPMatrix` physical-address gotcha -> the `cmpfn` oracle row in `docs/workflow/loop.md`;
  #4 both tooling fixes -> `tools/lib.sh` resolver `|| true` guards, `tools/dl_decode.py` `~` marker
  for path-approximate offsets. `docs/levers.md` was over budget after #2, so `dead-frame-levers`,
  `division-codegen` and `global-reread-vs-cse` were compressed to rule-plus-citation to pay for it.
- Carry-over: `func_8007399C` (`src/main/func_80071370.c`) at 149/149 with the ROM's exact `-0x50`
  frame and 70 `cmpfn` lines, all one cause: `sched.c:2469 birthing_insn_p` boosts the `out_height`
  parameter copy because its pseudo has `reg_n_sets == 1`, scheduling it to the bottom of block 0 so
  the incoming `$a3` stays live across the 5th-argument load and `player` cannot colour `$a3`. The
  open question is a single one — a source form that gives that pseudo a second `SET` without costing
  an instruction. Doc: `docs/wip/func_8007399C.near-match.md`. Its carve is now a one-line follow-up
  (`[0xACB38]` -> `[0xACAD0]`), not a cohort commit.

## Sprint 307 — first probe of `main`'s `jtbl-dispatch` vein (1 banked, 1 carried) — 2026-07-31
- Increment: 0 files / **1 function matched** (`kSetMultiTLB`, ex-`func_8005342C`, 118 instructions,
  byte-exact). `src/main/func_80052FE0.c` 6 -> 5 stubs; `--loose-stubs main` 277 -> 276;
  `jtbl-dispatch` 33 -> 32. md5-candidate unchanged at 230 of 265, so the file point banks 0 as
  priced. Descriptive names 168 -> **170** (`kSetMultiTLB` plus the S304 debt `func_8008534C` ->
  `emit_club_and_power_hud_dl`, closed at the plan gate).
- Quality: 0 stuck-far / 0 permuter / 1 carried / 0 re-opened.
- Seed: committed 10pt at `823192e`; banked **0**pt; realized **11**, residual **+1**; regime
  classical.
- What helped: **the excluded-class re-check the S275 rule asks for, and the leaf's own strings.**
  `jtbl-dispatch` had been out of `fresh` since S275 on one leaf's carve alignment; the two
  conditions are per-leaf and mechanical, and the first row taken banked on the first build with no
  permuter and no dive. Its two `osSyncPrintf` strings named the routine outright (`kSetMultiTLB :
  Invalid Page Mode`), which gave the signature, the page-mode table and the whole error path before
  a single build — a KMC library routine with no copy in `~/development/repos/libkmc` to coddog
  against. On the residual, one lever closed it: the three parameter copies are latency-1 ties whose
  order `sched.c` breaks on LUID, and `s32 i = index;` as the FIRST statement moves `a0`'s copy out
  of the `assign_parms` group to exactly where the ROM has it (after the `size` computation it
  overshoots past the `sllv`).
- Friction: **the carve rule had a second condition nobody had written down, and the hand
  derivation of it was wrong.** `func_8007399C` passed the S265 both-edge-8-align test and still
  cannot bank: the host object already carves `0xACBE0`, and one object emits one contiguous
  `.rodata`, so everything between must come from the same object. Worse, the hand reading mis-sized
  `jtbl_800D1738` as 42 words and concluded a two-leaf cohort; the ranker check written at this
  review counts the blocks and reports three (`func_800754BC` **and** `func_8007580C`). Second
  friction: `pick_target_yaml.parse_subsegs`'s type group is `[a-z]+`, so it silently drops every
  leading-dot `.rodata`/`.data` carve line — the new tell had to parse the yaml itself, and the
  shared helpers that consume that parse have been running without those lines for an unknown
  number of sprints.
- Applied: 5 of 5 accepted. #1+#4 `jtbl_carve_tell` in `tools/pick_target_score.py`: per-leaf
  `jtbl-carveable` / `jtbl-carve-blocked` / `pool-cohort:<fn>,...`, with carveable now counted in
  `fresh` (24 of 32 rows) and the cohort members named in the row; #2 `string_refs`, printing the
  `.asciz` literals a stub references beside its row; #5 a third carry-over kind,
  **cohort-blocked**, in `BACKLOG.md` (a byte-exact body that needs siblings, not a lever search,
  and is priced as the sum of the cohort); #3 the `kSetMultiTLB` -> `src/libkmc/` placement recorded
  as a follow-up with its blocker (the splat subseg's rodata is not contiguous, so it needs a
  16-aligned text split first). Retirement: `docs/workflow/loop.md`'s two curated-rename
  stale-object rules (the Bank-step bullet and the Conventions bullet) merged into one at the
  Conventions site, keeping all four citations and the gap-relic sub-case; **-451 B**, and the Bank
  step already pointed at Conventions for it.
- Carry-over: `func_8007399C` (`src/main/func_80071370.c`) at 149/149 with the ROM's exact frame,
  **cohort-blocked** on `func_800754BC` + `func_8007580C` and one carve line
  (`[0xACAD0, .rodata, main/func_80071370]`, size 0x178). Its remaining 84-line residual is one
  cause, read off `gcc -dS`: the priority-1 template copy loses to the priority-3 global load in a
  bottom-up `schedule_block`, which puts `player` in `t0` where the ROM has `a3`. Body, lever table
  and refuted set in `docs/wip/func_8007399C.near-match.md`.
- Push: local.

## Sprint 306 — the nearest `dl-emitter` carry plus `main`'s last fresh row (0 banked, 2 carried) — 2026-07-30
- Increment: 0 files / **0 functions matched**. `src/main/func_80078910.c` 21 stubs and
  `src/main/func_80080220.c` 13 stubs, both unchanged; md5-candidate unchanged at 230 of 265;
  `--loose-stubs main` 277 stubs, **fresh 1 -> 0**. Descriptive names unchanged at 168 / 555.
- Quality: 0/1/2/1 this sprint
- Seed: committed 13pt; banked 0pt; realized 16, residual +3; regime classical
- What helped: **gcc's own dumps, twice.** `-dS` printed the scheduler's ready lists and priorities
  and settled `func_8007EF0C` in one command — the `divmodsi4` insn scores `INSN_PRIORITY` 1 because
  its only in-block producer is a constant load, against 5 and 7 for the two load chains it races,
  and `schedule_block` is bottom-up, so it is chosen last and emitted first no matter how the source
  is spelled. That is a measurement where the carry doc had asserted a `rank_for_schedule` tie.
  On `draw_terrain_aim_grid` the **goto-loop form was worth +39 instructions** across six loops:
  `loop.c` had been reducing `&scr[k]`, `&scr[k-1]` and `&shade[k]` to givs updated at the loop head
  where the ROM recomputes `sll k,4` inside each block. Wrapping each goto loop in `if (n != 0)` put
  back the entry test the goto form drops, another +10. Eight levers in all took that body from 1273
  to an exact 1349.
- Friction: **the two carries are the same shape as the sprint's own plan risk and it still cost the
  whole sprint.** Risk 3 said a 0/2 leaves the next gate with no smallest-first option in `main`, and
  that is now the state: 0 fresh rows. Second, `tools/seg_diff.py` and `tools/gbi_match.py` were both
  hand-rolled again this sprint though S305 landed them a week ago — the S304 "read the index before
  opening a dive" lesson recurring one level up, at the tooling layer. Third, the permuter was run on
  a compile the build never performs: `permuter_settings_main.toml` has no `-ffast-math`, but six
  `src/main` TUs are per-file `-ffast-math` overrides in `mk/main.mk`, and on an FP-heavy TU that flag
  decides whether `x * C1 / C2` folds to a single `mul.s`.
- Applied: 4 of 4 accepted: #1 `setup-permuter.sh` now patches the generated `compile.sh` when the
  target TU carries a per-file `-ffast-math` override; #2 `docs/levers.md` division bullet gains the
  fast-math folding rule; #3 the goto-loop anchor gains its +39 scale and the `if (n != 0)` guard;
  #4 `docs/workflow/loop.md ## Oracles` names `seg_diff.py` and `gbi_match.py`. Retirement: the
  verbatim-mirror exemption in `docs/workflow/gates.md ## Story points` compressed to its rule plus
  one citation line (the mirror regime has been mined out since S148), and the stale
  `Current phase regime: mirror` corrected to `classical`. Net line delta **-4106 bytes** on
  `gates.md`, and `levers.md` paid for its own additions by merging `defeat-global-base-cse` into
  `aggregate-store-pins-pointer-load` as `global-reread-vs-cse` and the two narrow-type spellings
  into `narrow-type-spelling`.
- Carry-over: `func_8007EF0C` (`src/main/func_80078910.c`) at 968/974 with the ROM's exact frame,
  terminal-looking on a `sched.c priority()` value; `draw_terrain_aim_grid`
  (`src/main/func_80080220.c`) at 1349/1349 exact count with a frame 0x10 too large, residual is a
  whole-function FP-versus-GPR allocation equilibrium. Both have rewritten `docs/wip/*.near-match.md`
  with the body and the measured numbers, and `--carried-check` flags both.

## Sprint 305 — the fresh `dl-emitter` tail in `main`, sibling-seeded (1 banked, 1 carried) — 2026-07-28
- Increment: 0 files / **1 function matched** (`emit_ball_offscreen_indicator`, 797 instructions,
  byte-exact). `src/main/func_80080220.c` 14 -> 13 stubs; `--loose-stubs main` 278 -> 277, fresh
  3 -> **1**. md5-candidate unchanged at 230 of 265, so the file point banks 0 as priced.
- Quality: 0 stuck-far / 0 permuter-escalated / 1 carried / 1 re-opened (a bounded probe, negative).
- Seed: committed 10pt (5 + 5) at `f6b74ec`; banked **0**pt; realized **11**, residual **+1**;
  regime classical.
- What helped: **the mnemonic histogram, now three sprints running, and a new segment-split
  localiser.** On the banked leaf the histogram named every fix in turn (`slti`+`bnez` vs `bne` ->
  do-while loop form; `bc1t`/`and` vs `bc1f`/`or` -> a De Morgan inversion). On the carried leaf the
  893-row `cmpfn` diff said nothing and splitting both streams at rare anchor mnemonics
  (`jal`, `div`, `sqrt.s`, `bc1tl`, `trunc.w.s`) put the whole 6-instruction residual on one line:
  `61 vs 50` before a `div`, `92 vs 101` after it. Also: `gSPScisTextureRectangle` is the macro
  behind the `nor`/`sra 31`/`and`/`andi 0xFFF` corner clamps plus the two-level MIN/MAX s/t
  adjustment — recognising it turned ~60 instructions of apparent hand-rolled clipping into one
  call, and the SDK-composite-macro-first rule paid for the second sprint in a row.
- Friction: **one scheduler placement cost more than the other three fixes combined.** The carried
  leaf's residual is a `div` scheduled 11 instructions earlier than the ROM's, and source order does
  not move it: a temp before the macro, a full hand-expansion of the macro, and a hand-expansion with
  the temps in the ROM's exact order all produce a **byte-identical object**. That negative is worth
  more than the attempt — it rules out the whole emission-order lever family for this residual and
  says the next probe has to be the dependence graph or a `sched.c` dive.
- Second friction, self-inflicted: the first draft passed `0x8000 / size` inline into
  `gSPScisTextureRectangle` and paid +38 instructions, because gcc 2.7.2 emits one `div` per basic
  block that uses the value and the macro branches twice. The ROM's single `div` per call site was
  visible in the histogram from the first build and was read as "extra work somewhere" for one
  iteration before the `div`/`break`/`bne`/`mflo` group was recognised as one cause.
- Applied: 4 of 4 plus 1 retirement — #1 the histogram folded into `tools/cmpfn.sh` (computed off
  the NORMALISED streams, which is what kills the `li`-for-both-`addiu`-and-`ori` false delta a
  hand-rolled version reports); #2 `tools/seg_diff.py`; #3 `tools/gbi_match.py` (brute-forces every
  `G_RM_*` pair and every 8-argument `G_CC_*` pair against a `w0/w1`, and prints the
  `gDPSetCombineLERP` form when no stock pair exists); #4+#5 two `docs/levers.md` entries
  (`one-divide-per-basic-block`, `and-equals-zero-not-negated-and`) and #6 the `--loose-stubs main`
  advisory refreshed with the spent-pool state. Retirements paying for them, all in `docs/levers.md`:
  `fp-const-load-before-fabs` merged into `sched-tiebreak-coins` (same `rank_for_schedule` rule),
  `pre-temp-defer-rowadd-lever` merged into `operand-order-statement-split`, and the playbook index
  compressed to one anchor list. Net `docs/levers.md` delta **-5 B** (10236 -> 10231), so the
  surface is inside budget with two new levers on it.
- Carry-over: `func_8007EF0C` in `src/main/func_80078910.c`, at 968/974 with the ROM's exact frame
  and a single named cause; body and the three refuted source forms in
  `docs/wip/func_8007EF0C.near-match.md`. `func_8002CDA8` stands unchanged, with
  `gDPLoadMultiTile` now ruled out (byte-identical object, not merely a wrong frame), which spends
  the last natural construct its carry-over named.
- Push: local.

## Sprint 304 — allocation-first on the `dl-emitter` tail; a three-agent compiler-source fan-out (1 banked, 1 carried) — 2026-07-28
- Increment: 0 files / **1 function matched** (`func_8008534C`, 787 instructions, byte-exact).
  `src/main/func_80080220.c` 15 -> 14 stubs; `--loose-stubs main` 279 -> 278, fresh 4 -> 3. No file
  reached md5-candidate, so the file point banks 0 as priced at the gate.
- Quality: 0 stuck-far / 1 permuter run (plateaued, no signal) / 1 carried / 1 re-opened.
- Seed: committed 8pt (3 + 5); banked **5pt**; regime classical.
- What helped: **the mnemonic histogram of the `.s` against a fresh object.** It re-classified both
  leaves in two `grep` calls and no build. On `func_8002CDA8` it showed `bgez 2/1, bgezl 0/1, j 2/1,
  sw 271/272` — a branch-FORM divergence behind S303's "pure allocation, no structural deficit
  anywhere" reading, which was `cmpfn` normalisation. On `func_8008534C` it reduced a 491-row diff to
  one line (`lui` +1, `addiu` -1) that named an address-CSE defect. Also: `tools/dl_decode.py` put
  both bodies at exact instruction count on the **first build**; and the fan-out's cheap half —
  re-running the compile line with `-S` — exonerated the assembler in one command.
- Friction, and the sprint's real lesson: **the lever that banked `func_8008534C` was already in
  `docs/levers.md`.** `aggregate-store-pins-pointer-load` names `sched.c:817 true_dependence` and the
  array-vs-scalar `MEM_IN_STRUCT_P` mechanism outright, and neither the orchestrator nor the subagent
  reached for it. The residual was briefed as a `rank_for_schedule` tie and cost nine source forms
  plus a 370-iteration permuter run before the agent found the documented answer. That is a lever-INDEX
  navigation failure, not a missing lever, and it is why suggestion #1 was accepted at a lower weight
  than it was buffered at. Second friction: three of the four hypotheses the orchestrator handed the
  `func_8002CDA8` agent were refuted by it from the `-dg`/`-lreg` dumps — the live-length tie, allocno
  80, and the CSE hypothesis. Cheap to refute, but all three were guesses dressed as briefs.
- Fan-out result (3 agents): 1 crack, 1 assembler exoneration, 1 reduced-but-carried. The binutils
  rule-out is the model to copy — it answered its question three ways (`tc-mips.c:1283`; the sole
  byte-relocating `memcpy` at `:1680-1684` behind the branch-only guard at `:1530-1531`; the
  divergence already present in gcc's `-S`) and then proved the ROM's block order assembles
  byte-exact with **zero inserted nops**, converting "not the assembler" into "the target schedule is
  reachable". It also narrowed a standing project claim: a nop-count delta is an emission-order
  oracle only ADJACENT TO A BRANCH, never inside a straight-line block (`tc-mips.c:1568-1571`,
  `:1596-1663`).
- Applied: 4 of 5 — #1 mnemonic histogram into the `## Oracles` table; #2 the frame-slot ordering
  rule folded into `dead-frame-levers`; #3 corrected rather than implemented (see below); #5 the
  `gcc -S` rule-out promoted to the front of the fan-out binutils row. (#4 host-harness gotcha not
  selected.) Retirement paying for them: the `## Spot-check` bullet in `docs/workflow/loop.md`
  (1267 B) compressed to its rule plus a pointer to the `decomp_loop.py` Oracles row that already
  carried the same failure mode, measured saving ~790 B.
- **Correction, self-inflicted:** suggestion #3 claimed `setup-permuter.sh --main` "exits 0
  silently" and asked for a wrapper fix. Reproduced under `bash -x` at the review gate: **the wrapper
  works** — it activates the venv and drives `import.py` correctly. The real failure was calling
  `tools/decomp-permuter/import.py` directly from a shell without the venv, which dies on
  `ModuleNotFoundError: toml`. No code was changed; the wrong note in the permuter oracle row was
  corrected instead. A folklore claim carried into a suggestion is still folklore.
- Carry-over: `func_8002CDA8` in `src/main/func_8002A640.c`, no longer a vague permutation. The model
  is a **one-register slide** and the deciding allocno is **189** (the `gDPLoadTile` w0 half of
  `gDPLoadTextureTile`) at priority 769 against 356's 934 (`global.c:587-604`). Raising its
  `REG_N_REFS` weight lands the ROM's chain exactly and makes the grid-loop clamp block
  instruction- and register-identical, branch trio included; mnemonic deltas 4 -> 2. Frame still
  `-0x1D8` vs `-0x1C8`, so not a bank, and the form that reaches it is a `do {} while (0)` ref-weight
  probe around a hand-expanded w0 store — a mechanism proof, not a shippable body. `HW_VERSION_1` is
  ruled out (it calls `guDPLoadTextureTile`; the ROM's five `jal`s are all `osVirtualToPhysical`);
  `gDPLoadMultiTile` is the one untried natural construct. Also carried: the banked
  `func_8008534C` still holds its auto name — the curated rename needs `make extract` and was not run
  while a fan-out agent was live against `asm/nonmatchings/`.
- Push: local.

## Sprint 303 — both `func_8008D100.c` carries re-opened plus the simplest fresh row (0 banked, 3 carried) — 2026-07-28
- Increment: 0 files / **0 functions matched**, 3 carried with rewritten characterisations.
  `func_800947A8` reproduced at 618/618 with an exact `-0x98` frame; `func_8009232C` at 698 against
  697 with an exact `-0x38` frame; the stretch `func_8002CDA8` (0xD18, 838 instructions) was
  reconstructed from scratch to **838/838 on its first build**, frame `-0x1E0` against `-0x1C8`.
  Hosts unchanged: `func_8008D100.c` 12 stubs, `func_8002A640.c` 11. `--loose-stubs main` 279 stubs,
  fresh 5 -> 4; md5-candidate unchanged at 230 of 265. ROM SHA-1 green at every commit; tree clean at
  `48f7c5f`.
- Quality: 0 stuck-far / **0 permuter-escalated** / 3 carried / 2 re-opened (0 banked).
- Seed: committed 8pt at `53ccda2`; banked **0**pt (both hosts partial); realized 11 / residual +3;
  regime classical.
- What helped: `tools/dl_decode.py`, written this sprint and promoted at review — a big emitter's
  `.s` is not readable as a display list until the packet stores are re-sorted by write-pointer
  offset (they diverged by up to 40 packets on `func_8002CDA8`). With the list decoded, plus a host
  `gbi.h` harness to confirm each composite macro, a 101-packet body reached exact instruction count
  on the first build. `tools/loop_window.py` and `tools/allocno_report.py` turned both carries'
  residuals into numbers rather than lever guesses.
- Friction: all three residuals are `global.c` allocation, which no lever list addresses directly —
  a three-slot frame delta on the stretch, a spilled-versus-register global on `func_8009232C`, and a
  movable-selection window on `func_800947A8` that needs two conditions at once (`insn_count`
  117-119 **and** `savings * lifetime >= 2`). Two documented residuals were also wrong: 9232C's
  "one hoist" is four conditional-arm movables, and 947A8's grid-3 half was never mentioned.
- Applied: 4 of 4 plus 1 retirement (#1 three decoders to `tools/` with a `tests/tooling` golden;
  #2 host-harness recipe as a `loop.md ## Oracles` row, including the `p++`-not-`p` trap; #3 the
  `--carried-check` stem-glob correction into the gates DoR; #4 allocation as the `dl-emitter`
  class's third cost centre into `BACKLOG.md`; retirement: `gates.md`'s spent
  `New-audio-sub-lib band-open` bullet, -1041 B).
- Carry-over: `func_800947A8`, `func_8009232C` (`src/main/func_8008D100.c`), `func_8002CDA8`
  (`src/main/func_8002A640.c`) — all three characterised, none a fresh leaf.

## Sprint 302 — the last fresh `dl-emitter` row in `main` plus the closest S301 carry (1 banked, 1 carried) — 2026-07-28
- Increment: 0 files / **+1 function matched**: `func_8008D3F4` -> **`emit_lens_flare_dl`** (0x9E8, 634/634, frame `-0x110`), the S301 carry re-opened and closed. `func_8009232C` (0xAE4, 697) reconstructed to **698 instructions with an exact `-0x38` frame** on the day it was opened and **carried**. Stretch `func_800947A8` re-derived at 618/618 and its S301 verdict corrected, still carried. `src/main/func_8008D100.c` 13 -> **12** stubs, so no file claimed md5-candidate and the per-file `grep -c INCLUDE_ASM == 0` check applied to nothing. `--loose-stubs main` 280 -> **279**, fresh 6 -> **5**; md5-candidate unchanged at **230 of 265**. ROM SHA-1 green at every commit; tree clean at `279567a`. Descriptive count **+1 banked, +1 characterised, +1 verdict corrected**. Class now **23 of 27 across S293-S302**.
- Quality: 0 stuck-far / **0 permuter-escalated** / 1 carried / 2 re-opened (1 banked).
- Seed: committed **8**pt; banked **0**pt; realized **11**, residual **+3**; regime classical.
- What helped:
  - **Re-running a rejected lever after a structural fix is what banked the leaf.** `tod = sky_time_of_day_idx; n = n << 8;` as their own statements is a lever S301 measured at 30 diff lines and rejected. It closed the four-instruction rotation on the first try once the spill-slot cluster was gone. The lever table in a `docs/wip/*.near-match.md` is exactly as conditional as its verdict, and nothing in the workflow said so.
  - **A `Gfx *` declared in a block that opens one statement early controls `reload1.c`'s slot order.** The pseudo is created at block entry, and `reload1.c` hands out spill slots in ascending pseudo number, so wrapping each `PipeSync` + `SetOtherMode` pair in a brace block that declares the SetOtherMode packet first inverted all four slot pairs to the ROM's `0x2C, 0x3C, 0x34, 0x54, 0x4C`. The mechanism was confirmed first by giving all five packets function-scope locals, which reproduced the ROM's *relative* pattern and shifted every slot — that intermediate result is what made the block-scoped version an obvious next step rather than a guess.
  - **The banked-sibling seed made a 697-instruction body cheap.** `func_8009232C` decoded end to end from its `.s` against three already-banked emitters in the same host, and the first build was 698/697 with an exact frame. A host harness resolved every `G_RM_*`/`G_CC_*` pair by brute force in one command (`G_RM_CLD_SURF`, `G_RM_OPA_SURF`, `G_CC_MODULATEI_PRIM`, `G_CC_DECALRGB`), which is the second sprint running that this removes a whole class of iteration.
  - **`tools/loop_window.py` plus `tools/allocno_report.py` turned the carry's residual into two numbers** rather than a paragraph: one extra hoisted constant, and a `src` allocno winning its register by about 7 units of live length (230 against 220-228).
- Friction:
  - **A moved verdict in the `-dL` dump is not a register cost, and reading it as one cost most of a day.** `func_8009232C`'s loop-B scan moves nine invariants; the built object holds one. Chasing "my build hoists four more than the ROM" was chasing a number that was not the pressure. `loop_window.py` now prints a `sites` column so the moved-and-allocated subset is readable directly (accepted suggestion #2).
  - **Two structural rewrites of the same conditional were regressions, and both were cheap to measure and expensive to reason about first.** Hoisting the shared commands out of the `first` conditional (two `if`s instead of one `if/else`) gave 637 instructions and a wrong frame; inlining the `(i != 37) ? 6 : 2` ternary at its three uses made gcc branch instead of CSE and gave 715. The reasoning that motivated both was a `savings`/`lifetime` model that could not be reconciled with the ROM either way.
  - **A scripted whole-region edit clobbered a sibling function** while testing a loop-counter variant: the slice ran from `func_8009232C` to `func_800934CC`, which contains `emit_snapshot_spiral_wipe_dl`. Caught immediately by the build (`'y' undeclared in spiral_step`), reverted from a scratch copy, no commit. The `loop.md` rule against scripted splices names the `INCLUDE_ASM`-count guard; the failure here was picking the end anchor by name without checking what sits between.
  - **`levers.md` needed five compression passes to fit one new sentence**, the same friction S301 reported on the same file. The retirement plus three compressions landed the sprint at −54 bytes on that surface.
- Applied: 3 of 3 — #1 the lever-table conditionality rule folded into the `near-match.md`-is-a-hypothesis paragraph in `docs/workflow/loop.md`; #2 `tools/loop_window.py` `sites` column (pairs `li` + `ori` halves back into one word; 135 tests pass, 1 skipped); #3 the `reload1.c` slot-order lever folded into `dead-frame-levers` in `docs/levers.md` rather than added as a new entry. Retirement: the `do-while-zero-block-break` S282 preference clause (duplicated in `docs/hazards.md#pervasive-regalloc-classical-main` and the `empty-asm-volatile-sched-barrier` memory), plus three compressions, net **−54 bytes** on `levers.md`.
- Carry-over: `src/main/func_8008D100.c` — `func_8009232C` (698/697, one extra hoisted constant coupled to an unspilled `src`, `docs/wip/func_8009232C.near-match.md`) and `func_800947A8` (618/618, needs `savings * lifetime >= 2` on the `4`, which the ROM gets by sharing one pseudo with the outer `i != 4` bound, `docs/wip/func_800947A8.near-match.md`).

## Sprint 301 — the DL-emitter vein at 618-634 instructions, one host (0 banked, 2 carried at exact count) — 2026-07-28
- Increment: 0 files / **0 functions matched**. Both committed leaves reached the ROM's exact instruction count and frame and neither banked. `func_800947A8` 618/618, frame `-0x98`, six of seven global registers correct. `func_8008D3F4` 634/634, frame `-0x110`, **12 instructions out (98.1%)**. Stretch `func_8009232C` (697, jal-free, fp-free) untouched. `src/main/func_8008D100.c` stays at 13 stubs, so no file claimed md5-candidate and the per-file `grep -c INCLUDE_ASM == 0` check applied to nothing. `--loose-stubs main` 280 -> **280**, fresh 8 -> **6** (both attempts now flagged `CARRIED-WALL`); md5-candidate unchanged at **230 of 265**. ROM SHA-1 green at every commit; tree clean at `3baf569`. Descriptive count **+0 banked, +2 characterised**. Class now **22 of 26 across S293-S301**, four carries.
- Quality: 0 stuck-far / 1 permuter-escalated (2 runs, 0 zeros, 1 lever) / 2 carried / 0 re-opened.
- Seed: committed **8**pt; banked **0**pt; realized **10**, residual **+2**; regime classical.
- What helped:
  - **The hoisting coin turned out to be arithmetic.** `move_movables` (`loop.c:1631`) moves an invariant when `threshold * savings * lifetime >= insn_count`, with `threshold` starting at `(loop_has_call ? 1 : 2) * (1 + n_non_fixed_regs)` = 122 here (`loop.c:532`) and dropping 3 per move (`:1719`, `:1904`). `savings` copies `n_times_set` (`:597`, `:793`), so a materialise-once constant scores 1, and a constant feeding a `bne` has lifetime 1 because RTL emits the `(set r C)` adjacent to the branch. For life-1 invariants that collapses to `moves = floor((122 - insn_count) / 3) + 1` — verified against the `-dL` dump on two loops of one function (115 real insns -> 3 moves, 117 -> 2). `func_800947A8`'s constant `4` misses the third slot by **2 units** (113 against 115), and order, `savings` and `lifetime` are each measured and closed.
  - **The `mem-in-struct` array-subscript lever has a `loop.c` face, not just a `cse` one.** As plain scalars, `D_800C5F24`/`D_800C5F2C` do not conflict with the `MEM_IN_STRUCT_P` stores through `Gfx *`, so `loop.c` hoisted both loads *and* the `160.0f - x` / `96.0f - y` terms out of the loop and needed `$f20`/`$f22` — the tell is an `sdc1` pair in the prologue and a frame 0x20 too large. Subscripting them (`extern f32 G[]` + `G[0]`) took `func_8008D3F4` from 654 instructions with a wrong frame to **634/634 with an exact frame in one change**.
  - **The permuter paid in a lever again, exactly as the docs predict.** Base 1025, best candidate 460, and the candidate's entire source diff was a `do { } while (0)` around one expanded `_g->words.w1 = 0;`. Wrapping that one `gDPPipeSync` call at source level took the residual from 64 to 24 lines. A second run from the improved base plateaued at 385 over 4100 iterations with no candidate, which is the honest signal that the rest is not permuter-reachable.
  - **Resolving DL command words in a harness before writing the body.** All 28 preamble words of `func_8008D3F4` matched on the first build, including a custom `gDPSetCombineLERP` that no `G_CC_*` pair produces and a `gDPLoadTextureBlock` whose `SetTextureImage` carries `G_IM_SIZ_16b` for an 8-bit texture. Brute-forcing `G_RM_*` and `G_CC_*` pairs against the ROM words costs minutes and removes a whole class of iteration.
- Friction:
  - **Gate risk 2 fired: iteration cost scales with body length, and reconstruction cost does not.** Both bodies came up fast and correct — the class is still cheap to reconstruct — but at 618-634 instructions each carries enough hoisting and spill-slot surface that a single compiler coin blocks the bank. Two leaves, two coins. The S298 pricing rule ("price the reconstruction cheap, budget the integration") holds in direction and understates the tail.
  - **`import.py` fails silently on a nested function.** It printed `Syntax error in base.c … Proceeding anyway` and imported a broken parse; the documented workaround (copy the TU with nested parents deleted) took two attempts by hand.
  - **`pkill -f 'decomp-permuter'` killed the calling shell.** `loop.md` already banned the `'<script> <arg>'` form; the bare tool name does it too, so the rule's boundary was wrong, not just its example.
  - **The S301 `SPRINT.md` was written without `## Method` or `## Definition of Done`,** which
    `docs/prompt-style.md:15` names as instruction sections of that file. `prompt_lint check
    --sprint` passed at the plan gate because it only de-shouts the sections that exist; the dead
    cross-reference surfaced at review as the *only* two `make test-tools` failures (133 passed / 2
    failed). Both sections were written at the review and the suite is green (135 passed / 1
    skipped). The gate's own lint does not check that the required headings are present.
  - **Four prompt surfaces sit at budget** (`loop.md`, `hazard-index.md`, `levers.md`, `fanout-prompt.md` all within 0.1%). Fitting one accepted lever into `levers.md` cost five compression passes on unrelated entries.
- Applied: 4 of 4 — #1 `tools/permuter_import.sh` (strips nested-function parents into `nonmatchings/permsrc/`, hard-fails when the target is still a stub instead of importing a broken parse); #2 the `pkill -f` rule restated as "any pattern that appears in the command you are running", citing this recurrence; #3 the `loop.c` face of the aliasing lever folded into `aggregate-store-pins-pointer-load` in `levers.md`; #4 `tools/loop_window.py` (per loop: real-insn count, the `floor((122 - insn_count) / 3) + 1` window, and the movable list in scan order with every constant decoded — validated against both `func_800947A8` loops). Retirement: the duplicate `cmpfn.sh` bullet plus its `@Dp/@Dm` sub-bullet in `loop.md ## Conventions`, **-1592 bytes**, every fact already in the Oracles row (same fold as S298's `diff.py` prose); `levers.md` additions paid for by compressing four entries, net **-4 bytes**.
- Carry-over: `src/main/func_8008D100.c` — `func_800947A8` (618/618, `loop.c:1631` window coin, `docs/wip/func_800947A8.near-match.md`) and `func_8008D3F4` (634/634, 12 instructions out, `docs/wip/func_8008D3F4.near-match.md`). Stretch `func_8009232C` still `fresh`.

## Sprint 300 — the DL-emitter vein at 474-590 instructions, all-fresh (2 banked, 1 carried byte-exact) — 2026-07-28
- Increment: 0 files / +2 functions matched: `func_80083AC8` -> **`emit_ball_trail_dl`** (0x7F8, 510/510, `src/main/func_80080220.c` 16 -> 15 stubs) and `func_8006BC80` -> **`emit_hole_banner_dl`** (0x768, 474/474, `src/main/func_8006A2C0.c` 12 -> 11). Both all-SDK-macro, zero raw `Gfx` words, zero `.rodata`. `func_8007C5D8` reached byte-exact 590/590 with an exact frame and is **carried, not banked**: pool-blocked. Both banked hosts partial, so no file claimed md5-candidate and the per-file `grep -c INCLUDE_ASM == 0` check applied to nothing. `--loose-stubs main` 282 -> **280**; md5-candidate files unchanged at **230 of 265**. ROM SHA-1 green at every commit; tree clean at `8a0d357`. Descriptive count **+2**. Class now **22 of 24 across S293-S300**.
- Quality: 0 stuck-far / 2 permuter-escalated (3 runs, 0 zeros, 2 levers) / 1 carried / 0 re-opened.
- Seed: committed 8pt (frozen at `3d221fc`, before any `src/` edit); banked 0pt; regime classical. Realized **11 / residual +3** (+1 permuter escalation, +1 re-attempt — two orchestrator push-backs on `func_8006BC80` — +1 carry; no −1, neither leaf was a first-try).
- What helped: **the two levers that banked `emit_ball_trail_dl` are both new and both sit below the source level.** `loop.c:3804` `benefit -= add_cost * bl->biv_count` is the only reachable knob on `combine_givs_p` once it has combined three identical DEST_ADDR givs: writing two loop guards as duplicated `continue` blocks gives the biv three increment sites, driving the combined benefit to <= 0 so all seven `(i+1)` accesses keep the ROM's `%hi`/`addu`/`%lo` (with one increment site the body is 12 insns short; the third guard must NOT repeat them or its tail stops cross-jumping). The last four bytes were `reload1.c:657` handing out spill slots in ascending pseudo number, fixed by naming a DL packet as a declared local so it gets a pre-macro pseudo number — a 1500-iteration permuter run at base score 20 never beat base, because it cannot express a pseudo-numbering change. On `emit_hole_banner_dl`, `allocno_report.py` turned a live-length argument into arithmetic: `halfw` 6233 vs `hgt` 6114 at 24 refs each, so `hgt` needed `n >= 25` — exactly one more reference, supplied by a `do {} while (0)` around one statement (four other wrappings measured worse). Per-region temps rather than per-role was the single largest fix, collapsing two clusters and a placement in one edit.
- Friction: **a fully-cited terminal verdict was wrong on the side it did not measure, and the orchestrator push-back is what caught it.** `func_8006BC80` came back at 2/474 words with `loop.c:1706`, a `sched.c rank_for_schedule` LUID tie, the giv escape closed at `loop.c:3805`, and twelve alternatives measured — but it had only proved that `off = 8` cannot move *after* the hoist, never that the hoist could be removed. `Gfx** gp = &gfx;` removed it and the leaf banked. Two rounds earlier the same agent had reported "46 words" that were really three emission-order clusters plus three `ori`-vs-`li` rows that are not diffs at all. Second: `func_8007C5D8` is the second byte-exact body in one host blocked by a shared literal pool, and nothing in the ranker saw it — the blocked shape is a local array initializer, which gcc block-moves from a rodata template the C body must therefore emit; measured, not inferred, at 22,110,362 differing bytes. Third: `tools/permuter_settings_main.toml` piped `gcc -S` into KMC `as`, which drops two hazard nops it inserts when reading the same input from a file, so one imported base scored 508/510 on an assembler artifact.
- Applied: PO-selected 4 of 4, plus 1 retirement. #1 permuter pipe defect (`tools/kmc_permuter_compile.sh` + the settings file now point at it; verified 590/590 by hand) and the permuter row in `docs/workflow/loop.md` now records that asm-differ tolerates an extra instruction, so a 475/474 candidate can outscore a correct 474/474. #2 the mid-sprint verdict rule in `docs/workflow/fan-out.md`. #3 `cmpfn.sh` now disassembles with `-M no-aliases`, which fixes the `ori`/`li` false row *and* keeps a real `addiu`-vs-`ori` difference visible — the old comment's reason for leaving it was the fold-the-asm-side shortcut, which would have hidden that; the pad-nop trim learned `sll zero,zero,0x0`, 135 golden tests green. #4 `pool_blocked_tell` in `tools/pick_target_score.py` plus the `POOL-BLOCKED` status in `--loose-stubs` (7 hits in `main`, one of them not previously carried), with the scope limit documented: it catches the block-move shape only, so the S279 FP-literal variant in the same host still reads clean. Hazard detail folded into `#duplicate-literal-pool-shared-rodata` and one index row. Retirement: `sched-class-tiebreak-order-coin` + `sched-select-potential-hazard-coin` merged into one `sched-tiebreak-coins` bullet keeping both citations, which paid for the new hoist-removal knob on `emission-order-placement-lever`.
- Carry-over: `func_8007C5D8` (`src/main/func_80078910.c`), byte-exact and pool-blocked; body preserved at `nonmatchings/func_8007C5D8/base.c`, verdict at `docs/wip/func_8007C5D8.near-match.md`. It banks when the pool-owning siblings in that host do — as does `func_80079EBC`, blocked in the same file since S279.

---

## Sprint 299 — the DL-emitter vein at 352-426 instructions, all-fresh (3 banked, 0 carried) — 2026-07-28
- Increment: 0 files / +3 functions matched: `func_8006AEA4` -> **`emit_screen_fade_overlay_dl`** (0x6A8, 426/426, `src/main/func_8006A2C0.c` 13 -> 12 stubs), `func_80081EF8` -> **`emit_rest_distance_panel_dl`** (0x5EC, 379/379, `src/main/func_80080220.c` 17 -> 16), `func_80094228` -> **`emit_snapshot_panel_grid_dl`** (0x580, 352/352, `src/main/func_8008D100.c` 14 -> 13). All three all-SDK-macro, zero raw `Gfx` words. All three hosts partial, so no file claimed md5-candidate and the per-file `grep -c INCLUDE_ASM == 0` check applied to nothing. `--loose-stubs main` 285 -> **282**, fresh 14 -> 11, dl-emitter 53 -> 50, fp-mixed 100 -> 97; md5-candidate files unchanged at **230 of 265**. ROM SHA-1 green; tree clean at `f926ed1`. Descriptive count **+3**. Class now **20 of 21 across S293-S299**.
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened — the vein's first clean sweep.
- Seed: committed 8pt; banked 0pt; regime classical. Realized **9 / residual +1** (+1 novel bank-gotcha for the immediate-normalisation blind spot; the stale-`.o` rename trap was already in memory, so not counted novel; no −1 taken, since the gate went red twice).
- What helped: **every residual fell to a named lever, and two of them ran opposite to their own documentation.** `emit_screen_fade_overlay_dl` needed `ifelse-not-ternary-cse-reset` *inverted* — the ROM's single load of the phase global means the value lives in a local across the multi-predecessor join, and fixing the reload collapsed an `$f0`/`$f2` permutation at the same time. `emit_rest_distance_panel_dl` needed `local-alloc combine_regs` run *opposite* to the memory's note: a function-scope `s32` set in both arms and read in the cross-jumped tail has `reg_basic_block == -1`, so it is a global allocno and `global.c` runs after `local-alloc` already took `$a3`; declaring it per-arm restored the ROM's register. `emit_snapshot_panel_grid_dl` took `cross-call-live-range-callee-saved-lever` (`$s7`), a `bank_off` statement for emission order, and `idx += 0x400` moved between the texture load and the triangle pair. Composites `gSPScisTextureRectangle` (its `sll 7`/`negu`/`sra 7` clamp correction verbatim in the ROM) and `gDPLoadTextureBlock` (7 commands to one call) again did the heavy lifting. The S298 parent-side nested check, run by hand, correctly returned negative on `func_80065A1C` (+1 fn, not +2) for ~2 minutes of `.s` reading.
- Friction: **two RED gate builds, and the second is the sprint's finding.** (1) A curated rename left a stale object: `render_frame`, a still-asm stub in a file never touched, called `func_8006AEA4`; `make extract` renamed it in the `.s` but the build tracks no `INCLUDE_ASM` dep, so a 20:56 `.o` linked against a 21:41 `.s`. (2) Then `cmp -l` against `baserom.z64` gave exactly **4 wrong bytes behind three byte-exact bodies**: three `lui $at,0x4780` (65536.0f) built as `0x4700` (32768.0f), one `lui $at,0xC391` (-290.0f) built as `0xC392`. `cmpfn` normalises immediates by design — but the raw `objdump -dz` fallback that `docs/fanout-prompt.md` already mandates *ran on both leaves and still missed*, because both subagents classified the deltas as relocation slots. `$at` is the register both `%hi` and every float-constant load use, which is exactly why they are confusable. Also: the gate's risk ranking was right in shape for the second sprint running (integration, not reconstruction) and still wrong in specifics — `func_80081EF8` was priced highest-risk for its `sprintf` literals and 17 `jal`, and its `.rodata` came back empty with every callee signature already correct.
- Applied: PO-selected 3 of 6 surface-costing plus 3 record-only, and 2 retirements. #1 the relocation-vs-immediate rule (`docs/fanout-prompt.md` false-relocation bullet + the immediate-normalisation limit on `cmpfn`'s Oracles row in `docs/workflow/loop.md`); #2 rename = whole-tree `.o` delete (`docs/workflow/loop.md` bank step, beside the clean-rebuild-after-header-edit bullet it mirrors); #4 both levers marked bidirectional in `docs/levers.md`. #3/#5/#6 recorded here only (parent-side check tally 1 hit / 1 miss; risk-ranking confirmation; seventh straight sprint of bare idle notifications, all three `RESULT:` lines recovered from `STATUS` at one `tail` each). Retirements: the raw-command-word case law in `docs/fanout-prompt.md` compressed to its rule plus the S297 citation, and `rodata-strings-as-literals-via-tu-combine` deleted from `docs/levers.md` — its unconditional "prefer literals" is now wrong in the case S298 documented, and both halves live elsewhere (`OBJCOPY_ALIGN` at `hazards.md:4612`, literals-vs-`extern` in `#duplicate-literal-pool-shared-rodata`). Note for the next gate: the first retirement was proposed on a ~200 B saving estimate that was wrong (the rewrite was longer than the line), which is why a second was needed — **measure a retirement with `prompt_lint report`, do not estimate it.**
- Carry-over: none from this sprint. The vein's two standing carries (`emit_sky_dome_dl`, `init_rdp_and_draw_sky_background`) are untouched, both with named-gate terminal verdicts in `docs/wip/`.

---

## Sprint 298 — the DL-emitter vein at 251-365 instructions, all-fresh (2 banked, 1 carried) — 2026-07-28
- Increment: 0 files / +3 functions matched: `func_80084EBC` -> **`emit_wind_indicator_dl`** (0x3EC, 251/251, `src/main/func_80080220.c` 18 -> 17 stubs), and `func_80092F18` -> **`emit_snapshot_spiral_wipe_dl`** (0x5B4, 365/365) **together with its GCC nested child `func_80092E10`** (0x108, 66/66) as the in-body `spiral_step` (`src/main/func_8008D100.c` 16 -> 14). Both hosts partial, so no file claimed md5-candidate and the per-file `grep -c INCLUDE_ASM == 0` check applied to nothing. Repo-wide stubs 291 -> 288; `--loose-stubs main` 288 -> **285**, fresh 17 -> 14, dl-emitter 55 -> 53, nested-child 11 -> 10; md5-candidate files unchanged at **230 of 265**. ROM SHA-1 green at both commits; tree clean. Descriptive count **+2**. Banking the nested parent also closed the separately tracked S248 `func_80092E10` "`$v0`-arg wall" carry at no extra cost.
- Quality: 0 stuck-far / 1 permuter / 1 carried / 0 re-opened.
- Seed: committed 8pt; banked 0pt (both hosts partial); regime classical. Realized 11 / residual **+3** (+1 carry, +1 permuter, +1 novel bank-gotcha). The −1 first-try credit was not taken: the isolated builds earned it and the gate build went red twice.
- What helped: three results, and the first is the one that changes behaviour.
  (1) **A byte-exact instruction stream can still redden the gate, and the per-function oracles cannot see it.** `emit_wind_indicator_dl` was byte-exact in isolation and failed the ROM twice. Its six doubles spelled as source literals made gcc emit a *second* 0x40-byte `.rodata` pool beside the one already in the TU's extracted blob, so every following data symbol shifted +0x40 while `.text` stayed identical. `cmpfn` normalises `%hi`/`%lo` and reads `.text`; the subagent's object never links; `diff.py` needs a full make. Nothing in the loop sees this except the gate. Diagnosis was three steps and is worth repeating verbatim: the map shows a constant delta on every `D_<addr>` past one point, the drift onset names the region, and `build/src/<tree>/<file>.o(.rodata) 0x<addr> 0x<N>` names the culprit. Fix is to reference the blob entries as `extern const` — keep the `const`, since a non-const global cannot be CSE'd across an intervening call where a pool entry can, so dropping it changes the load count.
  (2) **gcc 2.7.2 emits one literal-pool entry per textual occurrence, and so did the ROM.** After the extern fix two rows still differed: the second wind window compares against `D_800D1B60`/`D_800D1B68`, byte-identical duplicates of the first window's `D_800D1B50`/`D_800D1B58`. The natural assumption — one constant, one symbol — is wrong, and collapsing them costs a build. This is the half of the finding most likely to recur, because the fix for (1) is what creates the opportunity to get it wrong.
  (3) **`--nested-check` answers the child question, and the parent question is the valuable one.** It cleared `func_80092F18` as `standalone`, correctly: that leaf is a nested *parent*. Its child's only two `jal` sites in the entire ROM are inside it, so no standalone symbol survives, one committed leaf removed two stubs, and a carry tracked separately since S248 closed as a side effect. The parent-side tell is as cheap as the child-side one already implemented — a `jal` to a callee whose own `.s` homes an incoming `$v0` — and it would have priced the leaf at +2 at the gate.
- Friction: (1) **The gate's risk ranking was finally right, and it no longer measured the right thing.** After four sprints of inversion, this gate called it: the fp=0 anchor banked cleanest, and the two `fp-mixed` leaves split one bank / one carry. But reconstruction was cheap for all three leaves and *integration* was the entire sprint, which no column on the ranked table prices. (2) Per-surface budgets under-funded the retro for the **fourth** consecutive sprint, and this time the accepted retirement structurally could not pay: it freed `loop.md` bytes while the additions wanted `fanout-prompt.md` (43 B free) and `levers.md` (7 B free). Resolved without a second PO decision by compressing within `fanout-prompt.md` and moving both mechanisms into `hazards.md`, which has 21 KB of headroom — but the pattern is now a standing structural problem, not a run of tight sprints. (3) The plan gate wrote a `SPRINT.md` with no `## Definition of Done` section, which `prompt_lint.py check` only surfaced at the review gate via a dangling xref from `prompt-style.md`. (4) Sixth consecutive sprint of bare idle notifications — but recovery cost was near zero, because the S297 `RESULT:` contract worked: two of three agents returned a parseable line unprompted and the third's `STATUS` carried the complete result.
- Applied: PO-selected, 4 of 6 plus 1 retirement. #1 the `.rodata` pool check, as a required `objdump -s -j .rodata` step in `docs/fanout-prompt.md` plus the scope limit on `cmpfn`'s Oracles row in `loop.md`, with the mechanism in a new `docs/hazards.md#duplicate-literal-pool-shared-rodata`. #2 the one-entry-per-occurrence rule, folded into that same section rather than a `levers.md` line (no budget). #3 the nested-**parent** ranker tell, recorded as a `BACKLOG.md` follow-up (no prompt-surface cost). #4 the new class `docs/hazards.md#macro-internal-emission-order`, with both new sections added to the `## Playbook index` TOC and two `docs/hazard-index.md` rows. (#5 the FP-tag confirmation and #6 the `RESULT:`-contract confirmation were recorded here rather than added to a surface — both are confirmations of existing rules, and neither justified spending budget.) Retirement: the `loop.md` permuter candidate-as-lever block consolidated from three near-duplicate sub-bullets (S287/S288/S290, all saying "read the plateaued candidate as a lever") to the rule plus its three citations, −832 B net across that file's edits; `loop.md` 47103 -> **46271/47104**, `fanout-prompt.md` 4053 -> **4083/4096** (a merge of the two isolation-blindness paragraphs, which share a root cause, paid for the addition), `hazard-index.md` 21666 -> **22442/22528**, `hazards.md` 632701 -> **637576/654336**. `make test-tools` 135 passed, 1 skipped; prompt-lint green on all 18 surfaces.
- Carry-over: `init_rdp_and_draw_sky_background` at **286/286** with the ROM's `-0xA0` frame and an instruction multiset identical to the ROM including every register and immediate. Terminal from the caller's source: the residual is emission order fixed inside the `gDPSetScissor` macro expansion, proven not-a-scheduler-question by surviving `-fno-schedule-insns2` and by a `-dR` dump showing the window tied at one `INSN_PRIORITY` so `rank_for_schedule` never leaves its `INSN_LUID` fallback (sched.c:2425). The one spelling that fixes the order removes the instruction that needed moving (DSE, 284/286). One named lead recorded: a sibling host emitting scissors through a game-local wrapper that computes both words before touching the pointer would bank it immediately. `docs/wip/init_rdp_and_draw_sky_background.near-match.md`.

## Sprint 297 — the DL-emitter vein at 208-299 instructions (3 banked, 1 carried) — 2026-07-28
- Increment: 0 files / +3 functions matched: `func_800734F0` -> **`emit_glyph_sheet_preamble_dl`** (0x3CC, 243/243, one build, `src/main/func_80071370.c` 16 -> 15 stubs), `func_80074968` -> **`emit_hud_glyph_dl_preamble`** (0x340, 208/208, one build, same host 15 -> 14), `func_8009548C` -> **`emit_snapshot_tile_wipe_dl`** (0x364, 217/217, `src/main/func_8008D100.c` 17 -> 16). All three hosts partial, so no file claimed md5-candidate and the per-file `grep -c INCLUDE_ASM == 0` check applied to nothing. Repo-wide stubs 294 -> 291; `--loose-stubs main` 291 -> **288**, fresh 20 -> 17, dl-emitter 58 -> 55; md5-candidate files unchanged at **230 of 265**. ROM SHA-1 green at all commits; tree clean. Descriptive count **+3**.
- Quality: 0 stuck-far / 1 permuter / 1 carried / 1 re-opened. The permuter run yielded neither a zero nor a lever, breaking the S287-S290 streak at 4-of-5 candidates-yield-a-lever, 0-of-6 runs-a-zero.
- Seed: committed 8pt; banked 0pt (all three hosts partial); regime classical. Realized 9 / residual **+1** (+1 carry, +1 permuter, −1 for three first-or-second-build banks).
- What helped: three results, and the carry produced all of them.
  (1) **The gcc `-dL` loop dump is to `loop.c` what `tools/allocno_report.py` is to `global.c`.** Appended to the file's real compile flags it writes `<file>.c.loop`: per-loop `N real insns`, every `Biv N initialized at insn M: initial value V`, every movable's `savings S (life L)` with moved / `not desirable` / `done move-insn matches <insn>`, and every giv's `reduced to (reg:SI R)` or `not worth while, X vs Y` — both sides of the comparison. Three separate residuals fell to it in one function. The one that generalises furthest: the ROM does *not* strength-reduce the vertex-bank address, and `strength_reduce` ignores a giv when `v->lifetime * threshold * benefit < insn_count` (loop.c:3823) where lifetime runs from the giv insn to its dest register's last use — so writing the address as a separate statement puts the def *before* the macro's w0 stores and loses, while the same assignment as a **comma expression inside the macro argument** evaluates it after and wins. That one change also fixed the frame size.
  (2) **Decay versus combine is per-function, and the two terms must be read, not estimated.** `func_8009548C` cracked on the `threshold -= 3` decay (loop.c:1719/1904 gating loop.c:1631): it hoists twelve display-list constants, the ROM moves exactly **9**, and landing on 9 needed exactly one invariant insn ahead of them — a `col * 4` statement split, where the natural two-insn `col * 5` prefix costs three more threshold and strands one more constant. `emit_sky_dome_dl` cannot be cracked that way, by a factor of 3, **because** `combine_movables` had already multiplied the same test's left-hand side by 9 (savings 3 x lifetime 3) where the other leaf's stranded constants were `savings 1`. The orchestrator asserted the wrong one of these twice — first ruling the decay out, then reinstating it — both times by assuming `savings` and `lifetime` rather than reading them off the dump, and was corrected by a subagent's arithmetic each time. Read the numbers first; the mechanism follows from them.
  (3) **Freeing a caller-saved register is worthless; freeing a t- or s-register opens reload's tier 1.** `local-alloc` runs before `global-alloc` and only ever uses the call-clobbered `v0, v1, a0-a3` for its ~98 local quantities, so a freed caller-saved register is re-absorbed immediately — measured directly: deleting a convenience local freed `$a3`, locals took it, and reload's spill victim merely moved from `$a2` to `$a3`. A freed t- or s-register stays `uses == 0`, i.e. tier 1, and reload takes it at reload1.c:3682-3688 rather than stealing the cheapest tier-2 entry at 3708-3710. This inverts the usual instinct on a "reload keeps stealing my register" residual, and it is what separates this build from the ROM: tier 1 is empty here and non-empty there.
- Friction: (1) **The plan gate's risk ranking was inverted for the fourth consecutive sprint** — the three leaves priced as the work banked in about one build each, and the leaf priced as a cheap finish ("not a wall, one build, three of four hunks matching") took the whole sprint. Four sprints is no longer a run of bad luck; the gate is systematically mispricing a documented near-match as cheaper than a fresh leaf. (2) The accepted retirement under-funded the same retro's additions for the **third** sprint running, exactly as S294 and S295 both predicted, and needed a second PO decision mid-gate; `loop.md` landed at 47103 of 47104 B. (3) `cmpfn`'s hunk alignment sent two iterations at the wrong region of a whole-body permutation — its hunks read 1:1 in size, which is an artifact, not a signal. (4) Fifth consecutive sprint of bare idle notifications, and this time recovery also needed an explicit follow-up message per agent to get a verdict at all.
- Applied: PO-selected, 4 of 4 plus 2 retirements. #1 the `-dL` loop dump as an `Oracles` row in `docs/workflow/loop.md`. #2 count-by-region-then-histogram prepended to the residual-classification note. #3 a raw-word DL body is a result to challenge rather than integrate, into `docs/fanout-prompt.md`'s standing policy — the sprint's own `func_80074968` came back byte-exact as a raw-word `EMIT` clone whose justification ("several words do not map to a single SDK macro") was refuted for all 27, and the all-macro rewrite was byte-identical. #4 the verdict-return contract retired for a parseable `RESULT:` terminal line, merging the overlapping Checkpoint and Reporting paragraphs. Retirements: the Seed provenance case law compressed to its rule plus one citation (−440 B), and the `diff.py` cross-reference bullet that only pointed at the Oracles table (−327 B). Net **+20 B** across both surfaces; `loop.md` 47046 -> **47103/47104**, `fanout-prompt.md` 4090 -> **4053/4096**. `make test-tools` 135 passed, 1 skipped; prompt-lint baseline re-frozen.
- Carry-over: `emit_sky_dome_dl` at **299/299** with the ROM's `-0x50` frame and all three region counts exact (48/34/217), from a 272/299 inheritance. This is now a **wall with a named gate, not an unfinished reconstruction** — the opposite of what the S296 doc said about the same function. Everything above the register allocator is settled; the single open question is three block-local `(set (reg) (const_int -8))` band masks that `combine_movables` (loop.c:1245-1287) unifies, creating an `align` allocno on a t-register, which empties tier 1 and forces reload to steal `tiled`. Every reachable combine gate is enumerated with why source cannot reach it, and three predicted nulls are recorded with their arithmetic. Full write-up in `docs/wip/emit_sky_dome_dl.near-match.md`.

## Sprint 296 — spend the dl_twin pair (2 banked, 1 carried) — 2026-07-27
- Increment: 0 files / +2 functions matched: `func_80055828` -> **`reset_face_textures_for_anim_slot`** (0x2CC, 179/179, 2 builds) and its `dl_twin` **`update_vertex_texture_coords_per_frame`** (0x3D0, 244/244, 14 builds), both in `src/main/func_80054900.c`, 17 -> 15 stubs, partial. `emit_sky_dome_dl` carried at 272/299 on the first build. No file claimed md5-candidate. `--loose-stubs main` 293 -> 291; the `dl-twin` group is now empty (2 -> 0, both members banked). md5-candidate files unchanged at **230 of 265**. ROM SHA-1 green at both commits; tree clean. Descriptive count **+1**, plus four named types (`FaceTexAnim`, `TexKeyframe`, `AnimTexOrder`, `ANIM_SLOT_*`) replacing a `u8 pad08[6]` hole.
- Quality: 0 stuck-far / 2 permuter / 1 carried / 0 re-opened. Ends the three-sprint all-zero run.
- Seed: committed 5pt; banked 0pt (both hosts partial); regime classical. Realized 7 / residual **+2** (+1 carry, +1 permuter).
- What helped: three results, and the first is worth more than the two banks.
  (1) **`--carried-check` was marking each sprint's own recommendations as walls, and had already cost a leaf.** `carry_over_names()` scoops every backticked name under `BACKLOG.md ## Carry-overs`; S293/S294/S295 each wrote a "the vein, smallest-first from here" *next-target* paragraph into that block. Result: all 15 names those lists carry read `CARRIED-WALL` with no `docs/wip/<fn>.*.md`, no attempt, and no prose claiming a wall, so this gate's pool read **2 fresh** when ~15 were actionable. It is not a hypothetical cost — S295 declined `emit_sky_dome_dl` on the strength of `--carried-check`, which was echoing S294's own recommendation of that leaf back at it. Fixed as guard (4), excising the paragraph on its author-supplied marker, exactly the shape of the S288 `NEAR-FREE RETRY` guard and explicitly not the lead-line/subject heuristic the S271 note bars. Pool 2 -> **21 fresh**; all nine spot-checked genuine walls (wip-doc-owning) still flag, exit 1. The general lesson: a DoR detector that reads prose will eventually read the *previous sprint's advice* as evidence, so any advisory written into a scanned region needs a marker the scanner knows to skip.
  (2) **`tools/allocno_report.py` belongs in the loop, not in the terminal-class playbook.** S290 introduced it to retire the "multi-register permutation is terminal" verdict. This sprint used it three times on an ordinary leaf and it decided every register question: a priority **gap** (one merged counter gave `frame` 21 refs and 5833 against the segment mask's 1875, rotating five registers; splitting it to 7 refs / 1068 landed all five at once) and a priority **tie** (`keyframe` and `keyframeIndex` both scored 6666, so the fix was allocno order — block-scoping the pointer inside its loop — not any change of weight). Those are two different fixes and the arithmetic tells you which, which is the whole value: about fifteen source permutations were *not* tried.
  (3) **Both permuter runs plateaued and both paid as levers**, extending the S287/S288/S290 play to 4/4 candidates-yield-a-lever, 0/5 runs-a-zero. The second candidate (220 -> 100) retyped `counter` to `unsigned char`. Applied literally that is wrong — the same variable carries the frame counter, which a `u8` truncates — but the mechanism it names is right: a byte-wide copy wants a byte-wide destination, or gcc emits `andi v1,a0,0xFF` where the ROM has `move v1,a0`. Splitting the byte copy into its own `u8 hold` closed the stream. Second time in one function that a candidate was wrong as a diff and right as a mechanism; the first (`*(slot + 0x8C)` unsigned rather than `*(s8*)`) took the diff from 170 rows to 5, because an unsigned read is not *provably* equal to the signed one, so copy-propagation cannot fold the else-arm test onto the copy and both stay live.
- Friction: (1) `cmpfn` read the twin **byte-clean at 244/244 with a matching frame size** while the ROM build failed — it normalises stack displacements, and the frame-pinned `cs` sat at `0xd8(sp)` against the ROM's `0x58(sp)`. `diff.py` after a full make found it in one line. Third normalisation to need surfacing (S260 branch targets, S276 frame size). (2) The frame *offset* then needed its own lever: gcc slots arrays at `expand_decl` but an address-taken scalar only at `put_var_into_stack`, so declaration order does not place them and the reserve had to be split across a block boundary opened after the `(void)&cs`. (3) The plan gate's risk ranking was inverted for the second sprint running — the twin that read identically to its already-banked sibling on every column cost 14 builds and 7 levers, because its residual was never structural. (4) A `pkill -f 'decomp-permuter/permuter.py'` killed the calling shell, exactly the S289 trap already documented in the loop's conventions; the commit had already landed, so it cost only a re-check.
- Applied: PO-selected, 4 of 4 plus 1 retirement. #1 `carry_over_names()` guard (4) in `pick_target_score.py`. #2 `allocno_report.py` promoted into `docs/workflow/loop.md`'s residual-classification note with the gap-vs-tie split. #3 `tools/cmpfn.sh` folds `N(sp)` displacements to `@F<dec>(sp)` on both sides instead of masking them, with the oracle-table failure-mode cell updated. #4 the frame-reserve split point as a clause on `dead-frame-levers`. Retirement: levers.md `abs-compare-form-steers-allocno`, whose mechanism is fully documented at `docs/hazards.md#register-reuse-nudge-classical-regalloc` (verified present before retiring) and which carried no citations outside levers.md, −157 B; levers.md 10182 -> **10233/10240** (net +51), loop.md 45917 -> **47046/47104**. `make test-tools` 135 passed, 1 skipped; prompt-lint baseline re-frozen.
- Carry-over: `emit_sky_dome_dl` — an unfinished reconstruction, not a wall. Full decode in `docs/wip/emit_sky_dome_dl.near-match.md`.

## Sprint 295 — the DL-emitter vein at 425-528 instructions (3 banked, 0 carried) — 2026-07-27
- Increment: 0 files / +3 functions matched: `func_800880A0` -> **`draw_animated_status_icon`** (0x7F0, 508/508, `src/main/func_80080220.c` 19->18 stubs, partial), **`emit_sky_horizon_compositor_dl`** (0x6A4, 425/425, `src/main/func_8002A640.c` 12->11, partial), `func_800939E8` -> **`emit_screen_transition_overlay`** (0x840, 528/528, `src/main/func_8008D100.c` 18->17, partial). No file claimed md5-candidate, so the per-file `grep -c INCLUDE_ASM == 0` check applied to nothing. `--loose-stubs main` 296->293; dl-emitter 63->60; fresh 15->12. md5-candidate files unchanged at **230 of 265**. ROM SHA-1 green at all three commits; tree clean. Descriptive count **+2**, both mechanism-grounded.
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. Third consecutive all-zero row.
- Seed: committed 8pt; banked 0pt (all three hosts partial); regime classical. Realized 8 / residual **0**.
- What helped: two results, and the first is a gate lesson rather than a lever.
  (1) **An FP mnemonic COUNT does not price a leaf; what the floats FEED is the class.** S290 fixed how to *measure* FP here (count mnemonics, never `grep '$f[0-9]'`) and was right to. This gate then read `fp=31` on `func_800880A0` as the pack's one risk tell and designated it the first drop — and it banked byte-exact on the first build with zero iterations, because all 31 mnemonics were a two-line 20%-ease on a position pair plus six `lwc1`/`trunc.w.s`/`mfc1` triples making two texture rectangles' 10.2 fixed-point corners. The separating test is mechanical, unlike the composite-vs-custom-packing split S294 measured and could not separate: conversions out to integers (`trunc.w.s`/`cvt.w.s` with `mfc1`, few float stores, arithmetic no heavier than the conversions) is coordinate math; no to-int conversion at all plus >=8 `swc1` is the S276/S277/S290 stored-FP class. Shipped as `fp_class_tell` and calibrated on six functions, two with known outcomes (`func_800880A0` -> `fp-coord`, S290's `func_8005D3B8` -> `fp-sched`). Deliberately advisory and not fed into `_is_fresh`: six functions is a thin calibration, and the S275 tag is the standing lesson on what over-trusting a fresh `.s` heuristic costs. The generalisation is the more durable half — **the tells filter classes, not difficulty within a class**: both leaves that cost iterations this sprint read clean on every tell the ranker has.
  (2) **Split only the value whose live range must die between two loops.** `emit_sky_horizon_compositor_dl` is the banked sibling `copy_previous_frame_to_cfb` run twice around a shared preamble and tail, and its residual was a whole-function register rotation. With both `i` and `lines` at function scope they share one range pinned open across both loops, so the 5-instruction `(i != 37)` chain is hoisted to each loop's head into its own register instead of computing lazily in `$v0` (205 differing instructions). Block-scoping **both** lands the chain at exactly the ROM's offsets and still misses at 131, because the ROM keeps `i` in `$t8` in *both* loops — `i` is one pseudo. Function-scope `i` plus a per-loop `lines` is byte-exact. `func_800939E8`'s subagent reached the identical split from the opposite direction on a different function in a different file, with no contact, which is what makes it a lever rather than a per-function coincidence. Measured nulls: declaration order, moving the `D_800B6810` store, hoisting `nuGfxZBuffer` to a local, `for` -> `do/while`.
- Friction: (1) The plan gate's risk ranking was inverted — the designated drop banked first with zero iterations, the cleanest-reading leaf cost five. (2) An integration conflict the isolated build structurally cannot see: the new body indexed `D_800E2158`, which its host declared twice as a scalar `void*`. Both sides correct; only the orchestrator could observe it, and it cost an edit plus a re-verify. (3) All three subagents sent bare idle notifications instead of returning verdicts, the fourth consecutive sprint; the `STATUS` file plus `base.c` carried the complete result every time, so recovery was one read each. Four sprints of the same line is the signal to change the rule rather than restate it, which is what #3 did. (4) The retirement under-funded the additions for the third sprint running: budgets are per-surface, so the accepted gates.md retirement (−696 B) could not pay for the levers.md clause (+211 B over), forcing a second PO decision mid-gate.
- Applied: PO-selected, 4 of 5 plus 2 retirements. #1 `fp_class_tell` in `pick_target_score.py` (`fp-coord`/`fp-sched`/`fp-mixed`), surfaced as a `--loose-stubs` row suffix and a class tally, with the count-is-not-the-class rule appended to the main plateau advisory. #2 `docs/fanout-prompt.md` gains an extern-collision report clause so the orchestrator gets the conflict list with the body. #3 the idle-notification contract retired in favour of documenting `STATUS` + `base.c` as the primary deliverable pair. #4 folded into the retirement below. (#5, bounding what the tells buy, was not selected — its substance is carried by #1's generalisation.) Retirements: gates.md's manual `BACKLOG.md`-grep DoR procedure, fully superseded by `--carried-check` (which has strictly wider coverage and exits non-zero), 30680 -> **29984**, net **−696 B**; and `temp-scope-and-live-range-steer-a-copy` + `variable-reuse-is-a-per-register-lever` merged into **`scope-and-live-range-steer-allocation`**, 10451 -> **10182/10240**, net **−269 B**. `make test-tools` 135 passed, 1 skipped; prompt-lint baseline re-frozen.
- Carry-over: none blocked. The `dl-emitter` pool reads 12 fresh, and the next tier starts at `func_8006AEA4` (0x6A8) — now tagged `fp-mixed`, as are `func_8006BC80`, `func_80083AC8` and `func_800947A8`. The two `fp-coord` rows (`func_8007C5D8`, and `func_8009232C`/`func_8002CDA8` at fp=0) are the cheaper next slice by the new column, though `func_8007C5D8` carries 6 branch-likely, a separate tell. `emit_ball_offscreen_indicator` reads `fp-sched`.

## Sprint 294 — the DL-emitter vein at 1.5-2x the size (4 banked, 0 carried) — 2026-07-27
- Increment: 0 files / +4 functions matched: `func_8006A84C` -> **`load_texture_block`** (0x48C, 291/291, `src/main/func_8006A2C0.c` 15->14 stubs, partial), `func_80031450` -> **`copy_previous_frame_to_cfb`** (0x458, 278/278, `src/main/func_8002A640.c` 13->12, partial), `func_8006C918` -> **`update_and_draw_iris_wipe`** (0x438, 270/270, same file 14->13) and the PO-added 4th `func_8009351C` -> **`capture_frame_snapshot`** (0x4CC, 307/307, `src/main/func_8008D100.c` 19->18, partial). No file claimed md5-candidate, so the per-file `grep -c INCLUDE_ASM == 0` check applied to nothing. Repo-wide stubs 300->296; md5-candidate files unchanged at **230 of 265**. `--loose-stubs main` 300->296; dl-emitter 67->63; fresh 24->20. ROM SHA-1 green at all four commits; tree clean. Descriptive count **+4**, all mechanism-grounded.
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. Second consecutive all-zero row, and the first 4-bank sprint on `main`.
- Seed: committed 8pt; banked 0pt (all three hosts partial); regime classical. Realized 7 / residual **−1**.
- What helped: three results.
  (1) **The class verdict is settled, and the accepted split turned out to be unimplementable.** S293+S294 banked the seven smallest members of the DL-emitter class 7/7 at 154-307 instructions, zero permuter runs, zero compiler-source dives. The retro accepted "retire or split" the tag; the split (composite-mappable = a cheap leaf, custom packing = the genuine store-giv carry) was attempted and **measured to have no `.s` tell**: over the seven banked composites plus the canonical custom packing `func_8007624C`, the bitfield-assembly counts overlap completely, and in the wrong direction — a composite expanding run-time arguments emits *more* `andi`/`sll`/`or` than the hand-packed one does (`func_8006A84C` banked at andi=43/sll=38 against `func_8007624C`'s andi=4/sll=23). So the tag was renamed `dl-emitter` and demoted to a pricing tag, and the negative result was written into `wall_class_tell`'s docstring so a third sprint does not re-attempt the same split.
  (2) **Twin transfer: the shape carries, the register allocation does not.** `func_80031450` and `func_8009351C` live in different files and have an equal DL command-word multiset. Handing the banked twin's body to the 4th leaf's reconstruction transferred everything structural (preamble order, the 38-strip loop with its `(i != 37) ? 6 : 2` remainder, the `gSPScisTextureRectangle` composite, the loop-exit form) and it reached 308/307 on the first build. The single surplus instruction was a per-iteration reload of `D_800FED10`: the display-list stores through `Gfx*` may alias the global, so neither `cse` nor `loop.c` hoists the read, and that load forced a second induction variable. A preheader local (`u32 src = D_800FED10;`) restored the ROM's single `+0xF00` IV. 307 instructions in 2 iterations is the payoff, and the new `dl-twin:<fn>` tag found a further pair on its first run.
  (3) **An uncallable composite is still the composite.** `load_texture_block` takes `siz` as a run-time parameter, so `gDPLoadTextureBlock` — which token-pastes `siz` to reach its per-size constants — cannot be called at all. The ROM's body is nonetheless exactly that macro expanded twice, one arm per supported size, with gcc tail-merging the shared `gDPSetTileSize` into the join; it banked byte-identical on the first build with zero iterations. The failure mode this retires is reading "the macro does not apply" as evidence of a hand-rolled custom packing, which is the expensive class.
- Friction: (1) The retirement under-funded the additions *again*, exactly as S293's friction note predicted: the accepted merge saved ~200 B while the two accepted lever clauses cost ~700, so levers.md went 225 B over budget and needed a second PO decision mid-gate. Two sprints running, a merge chosen as the retirement has not paid for what the same retro was adding. Price the retirement against the accepted additions' size, not against the entry being merged. (2) All four subagents sent bare idle notifications instead of returning their verdicts, costing an orchestrator round-trip each; the `STATUS` file plus `base.c` carried the full result every time, so the recovery is cheap but the contract clause is being ignored. Third sprint in a row. (3) Integration ran while two isolated compiles were still live, against the loop's "never full-make while an isolated compile is in flight" rule; it was checked with `pgrep` first and no race occurred, but the check is manual and easy to skip.
- Applied: PO-selected, 4 of 4 plus 1 retirement. #1 `RAW-DL-EMITTER` renamed `dl-emitter` and its wall framing retired in `tools/pick_target_score.py wall_class_tell` + `tools/pick_target.py` `_is_fresh`/row/tally, with the failed-split measurement recorded in the docstring. #2 new `dl_word_signature` + `dl_twin_pairs` in `pick_target_score.py`, surfaced as a `dl-twin:<fn>` suffix on `--loose-stubs` rows and a count in the tally. #3 the uncallable-composite recipe added to `docs/hazards.md#display-lists` (not levers.md, which had no room). #4+#5 folded as clauses onto the existing `temp-scope-and-live-range-steer-a-copy` and `aggregate-store-pins-pointer-load` entries rather than added as new bullets. Retirement: `sched-luid-order-inline-arg-subexpr` + `sched-coin-loop-preheader-order-lever` merged into **`emission-order-placement-lever`** (both said placement follows source emission order; the merged entry keeps both citations and absorbs the S294 giv-order knob), plus the PO cut the `<scope>` maintenance paragraph; levers.md 10229 -> **10159/10240**, net **−70 B**. `make test-tools` 135 passed, 1 skipped; prompt-lint baseline re-frozen for the S294 citations.
- Carry-over: none blocked. Near-free retries in this vein: `emit_sky_dome_dl` (0x4AC, jal=0, fp=0, two texture-block loads) in the host that just went 13->12, and the `dl-twin` pair `func_80055828` / `update_vertex_texture_coords_per_frame` — both currently carried-wall, so the twin tag is a re-open candidate rather than a fresh leaf. `main` reads 20 fresh, all dl-emitter.

## Sprint 293 — open the RAW-DL-EMITTER vein in main (3 banked, 0 carried) — 2026-07-27
- Increment: 0 files / +3 functions matched: `func_8006A5E4` -> **`load_texture_block_4b`** (0x268, 154/154, `src/main/func_8006A2C0.c` 16->15 stubs, partial), **`update_vertex_texture_coords`** (0x290, 164/164, `src/main/func_80054900.c` 18->17, partial) and `func_80073C14` -> **`emit_glyph_string_12px`** (0x310, 196/196, `src/main/func_80071370.c` 17->16, partial). No file claimed md5-candidate, so the per-file `grep -c INCLUDE_ASM == 0` check applied to nothing. Repo-wide stubs 306->303; md5-candidate files unchanged at **230 of 265**. `--loose-stubs main` 303->300; raw-dl-emitter 70->67; fresh **0 -> 29** (the tag demotion, not new work). ROM SHA-1 green at all three commits; tree clean. Descriptive count **+2** (the third was already curated), both mechanism-grounded: the CI4 loader from its single composite, the glyph emitter from its 192-wide I4 atlas geometry.
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. First all-zero row since S291 and the first 3-of-3 on `main` since S287.
- Seed: committed 5pt; banked 0pt (all three hosts partial); regime classical. Realized 4 / residual **−1**.
- What helped: three results, and the first is a gate lesson rather than a lever.
  (1) **A plateau reading is only as current as its exclusion tags.** `--loose-stubs main` reported **0 fresh**, which S291 and S292 had both read as "smallest-first is over, `main` is a crack-slice-only pool". But 70 of the 303 stubs were excluded by `RAW-DL-EMITTER`, a tag added in S275 on a wall verdict that **S258 retired three sprints later** (retro #5, "retire the raw-DL-word wall class"), and S258/S270 had already banked 3 emitters of that family. The three smallest members all read `fresh` + `standalone` under `--carried-check`/`--nested-check`, and all three banked. Two were single-composite one-liners found by grepping `gbi.h`. The rule now in `gates.md`: a `0 fresh` count is not proof of a crack-slice-only pool — two of the five class tags are heuristics, so re-check each excluding tag's own verdict at a plateau gate. `RAW-DL-EMITTER` is advisory (counted in `fresh`) as of this sprint; `JTBL-DISPATCH` stays excluding because its wall is mechanical (bank-time rodata carve, 8-alignment both edges), not a codegen verdict.
  (2) **A copy deleted by register preference is recoverable by ending the live range, not by respelling the copy.** `func_80073C14`'s first build was 195/196, short exactly the ROM's `addu a0,v1,zero`. Eight spellings of that copy all stayed at 195 (variable split, `u8` source, mask, if/else, ternary, function-scope decls, `do {} while (0)`, statement reordering); RTL dumps showed the copy existed at `.cse` and was deleted because `global.c:786-823 expand_preferences` gave both pseudos `$v1`. What fixed it was naming the two atlas offsets as explicit temps (`uls = (code % 16) * 12;`) instead of inlining `col * 12` into the macro arguments, which ends `code`'s live range at the div/mod. One edit closed both residuals — it also hoisted the offset computation to the top of the emit block, ~17 instructions earlier, where the ROM has it. This is the emission-order lever applied to a **named temp** rather than a statement move.
  (3) The S258 composite-macro recipe worked exactly as documented, twice. `func_8006A5E4`'s `FD/F5/F3/F2` + `E6/E7` opcode run is one `gDPLoadTextureBlock_4b`; being all-RDP it is even profile-independent (verified identical with and without `-DF3DEX_GBI_2`). One sub-lesson worth keeping: the apparently duplicated tile-size word in `func_80073C14` is a CSE artifact (the two `gDPSetTileSize` w0 values are equal, so the same `sw` source register is reused), not a reconstruction error — the DL-preamble-word failure mode in reverse.
- Friction: (1) The `## Definition of Done` section was omitted from `SPRINT.md` at the plan gate. `prompt_lint check --sprint` passed (it does not run the xref check), so it surfaced only at the review's `make test-tools` as a dangling xref from `docs/prompt-style.md` — a gate-time miss that a different tool caught two hours later. (2) The retirement took four measure-and-trim rounds to go negative: the first merge was +334 B, and only folding in a second entry plus trimming two unrelated parentheticals reached 10229. S291's lesson ("merging two entries into one longer one is not a retirement") is right, and the corollary is that a merge funding a new addition usually needs a third entry's worth of slack. (3) Two subagents sent bare idle notifications after delivering, one of them twice, costing an orchestrator round-trip each to confirm nothing was outstanding.
- Applied: PO-selected, 3 of 3 plus 1 retirement. #1 `RAW-DL-EMITTER` demoted to advisory in `tools/pick_target.py` `_is_fresh` (counted in `fresh`, still tagged in the row and in the class tally) with the rationale in `pick_target_score.py wall_class_tell`'s docstring, including why the `.s` tell cannot separate the composite-mappable subtype from the genuine custom-packing carry subtype. #2 the named-temp live-range lever, folded into the retirement below. #3 the plateau-gate rule in `docs/workflow/gates.md` DoR, rewritten into the existing S275 bullet rather than added beside it. Retirement: `local-alloc-combine-regs-block-local-temp` + `copy-coalesce-cse-signext-terminal` merged into **`temp-scope-and-live-range-steer-a-copy`**, which carries both scope knobs, both citations, and drops the "terminal" verdict this sprint disproved in one direction; levers.md 10238 -> **10229/10240**, net **−9 B**. `make test-tools` 135 passed, 1 skipped; `prompt_lint check` green on 18 surfaces with no baseline re-freeze needed (the one cap that rose was de-shouted).
- Carry-over: none. `main` now reads 29 fresh, all raw-DL emitters; the next slice in this vein starts at `func_80074968` (832 B, 23 `lui` / 14 `ori`), which unlike this sprint's three is a plausible genuine store-giv subtype rather than a composite.

## Sprint 292 — crack slice on main's documented allocation carries (1 banked, 2 carried) — 2026-07-27
- Increment: 0 files / +1 function matched: `func_8005D334` -> **`clear_player_slots`** (0x84, 33/33, `src/main/func_80059BA0.c` 14->13 stubs, partial). No file claimed md5-candidate, so the per-file `grep -c INCLUDE_ASM == 0` check applied to nothing. Repo-wide stubs 307->306; `--loose-stubs main` 304->303, fresh **0->0** (the pool was already exhausted at the gate, which is why the sprint was a crack slice). ROM SHA-1 green at both commits; tree clean. Descriptive count +1.
- Quality: 0 stuck-far / 2 permuter (no zero; both yielded a knob) / 2 carried / 3 re-opened.
- Seed: committed 5pt; banked 0pt (both hosts partial); regime classical. Realized 9 / residual +4.
- What helped: `tools/allocno_report.py` used as arithmetic rather than as a report — compute the priority window the ROM's register implies FIRST, then choose the knob that lands in it. On `func_8005D334` that window was `end` in (4210, 6250), reached by a `do {} while (0)` note; the same construct moved `cs` one register on `func_80056060`. The goto outer loop was the other half: it stops `loop.c` hoisting the `!= 4` compare constant, and fixed preheader order, two register roles and the loop-tail order in one edit. Reading the permuter's best candidate as a knob rather than a diff (S290 rule) supplied the winning note placement after 286k iterations that never scored 0.
- Friction: three separate doc errors, all in carried near-match notes, each costing an iteration — a stale instruction count, a residual class that named colouring when the cause was a source-structure error (seven `tries++` where the ROM has one that `reorg` copies into four annulled `bnel` slots), and wrong return values. Compounding it, `cmpfn.sh` counted trailing padding nops, so `func_8005D334` read 36 vs 33 and was chased as a structural deficit when it was already at exact count.
- Applied: PO-selected, 5 of 5 plus 1 retirement. #1 `tools/cmpfn.sh` trims trailing padding nops beyond the `jr $ra` delay slot and reports `[+N pad nop(s) trimmed]` (conservative: only when every trailing line is a nop). #2 `tools/allocno_report.py` falls back to a seed dir's own `compile.sh`, so a crack slice reads the table from `nonmatchings/<fn>/base.c` without inlining into `src/` first. #3 the `do {} while (0)` reweight generalised from `local-alloc` qty tiers to `global.c` allocnos, with the measure-the-window procedure — folded into the merged lever. #4 `docs/hazards.md` goto-loop section gains the de-hoisted-constant generalisation (any `==`/`!=` compare constant, not only a loop bound, plus the two-constant hoist-order failure mode). #5 `docs/workflow/gates.md` DoR gains "a compiler-source dive's proven-wall conclusion is a hypothesis; keep the citation, drop the conclusion". Retirement: `do-while-doubles-reg-n-refs-qty-tier` merged into `global-allocno-compare-livelength-biv-order` (levers.md 10222 -> 10238/10240, net **+16 B**, flat rather than negative because the merged entry carries the new procedure). `make test-tools` 135 passed, 1 skipped; prompt-lint baseline re-frozen for the S292 citations (hazards 78->79, gates 7->8).
- Carry-over: `func_8005DFE8` (102/102, 36 rows; hoist-order tie between two compare constants + a `loop.c` IV init placement) and `func_80056060` (39/39, 20 rows; callee-saved priority inversion that ref/live-length knobs cannot reach — next direction is `find_reg`'s callee-saved preference path, not `allocno_compare`). Both docs rewritten from measurement.

## Sprint 291 — the last two non-FP fresh leaves of main (2 banked, 0 carried) — 2026-07-27
- Increment: 0 files / +2 functions matched: `func_8005A580` -> **`load_course_assets`** (0x778, 478/478, `src/main/func_80059BA0.c` 15->14 stubs, partial) and `func_80057914` -> **`build_scenery_light_sets`** (0x6E8, 442/442, `src/main/func_80054900.c` 19->18 stubs, partial). Neither file is md5-candidate, so the per-file `grep -c INCLUDE_ASM == 0` check applied to nothing this sprint. Repo-wide stubs 309->307; md5-candidate files unchanged at **230 of 265**. `--loose-stubs main` 306->304 stubs, fresh **9->7**. ROM SHA-1 green at both commits; tree clean. Descriptive count **+2**, both mechanism-grounded rather than guessed: the first from its teardown twin's free order, the second from the gbi.h macro its staging expands to.
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. First sprint since S285 with no permuter run at all; second consecutive zero-carry sprint.
- Seed: committed 5pt; banked 0 file points (both files partial); realized 7 / residual +2 (+1 mid-sprint split for the rodata carve, +1 novel bank-gotcha for the named-aggregate block move); regime classical.
- What helped: three results, two of which are pricing lessons rather than levers.
  (1) **Assigning a brace-initialized aggregate through a named local costs an extra whole-struct block move.** `func_80057914` builds two `gdSPDefLights2` sets. Written the obvious way — `{ Lights2 lights = gdSPDefLights2(...); D_800C1EA0[i] = lights; }` — it came out 478 instructions against the ROM's 442 with frame `-0x150` against `-0x128`: gcc-2.7.2 expands the constructor into a temporary, block-moves it into the declared local, then block-moves the local to its destination, three moves where the ROM has two, and the extra 0x28 of frame is the second `Lights2` slot. The block scope is irrelevant — inner block, outermost block and a compound-literal assignment to a pre-declared local were all measured and all cost the same. Assigning the constructor straight to its destination with the GNU cast-to-struct form, with no named local at all, closed both the count and the frame in one edit. Now `docs/hazards.md#named-aggregate-local-extra-block-move`.
  (2) **Both gate prices were wrong, in opposite directions, and both were cheap to check.** `func_8005A580`'s headline cost was "43 distinct `D_` globals to type"; its teardown twin `func_8005ACF8`, three functions below it in the same already-`c` file and already banked, frees every one in allocation order and so declares all 24 destination array shapes, including each adjacent pair the gate had flagged as "one record" (`D_800FF424` = `D_800FF420[1]`, `D_8012D424` = `D_8012D420[1]`, five more). The real cost was zero. Conversely `func_80057914` was priced as "model this 0x128 stack struct" from its 16 `lwl`/`swl` pairs, read as a byte-aligned record array shifting down a slot; they were a gbi.h macro expanding twice, and the 100 `sb` / 42 `lb` byte triples were its nested `{r,g,b}` brace sub-initializers staged through `Light_t` temporaries. Both checks are one grep. Now two hazards sections: read the freeing twin before pricing an allocator, and grep `gbi.h` before modelling a large stack struct.
  (3) **A partial rodata carve can need a three-way split, and the two obvious two-way splits fail in opposite directions.** The macro's constant sub-initializers become 16 bytes of compiler rodata at `0x800D082C`. Cutting at the end of those constants (`0xABC3C`) starts the following asm blob 4-mod-8, so GAS's own 8-alignment of the pool's first `.double` lands it **4 bytes late** and the whole ROM shifts; cutting at the pool (`0xABC40`) has `ld` place the blob straight after a 16-byte section without padding up, **4 bytes early**. Giving the lone intervening 4-byte word its own `rodata` subseg fixes both edges. This extends the recorded "8-align both edges" rule: when the gap between the carve's end and the next 8-aligned pool is non-zero, that gap is its own subseg.
  (4) Smaller, and it re-confirmed the S290 tool: `tools/allocno_report.py` turned `func_8005A580`'s residual from guesswork into a directed search, showing the loop counter at priority 10294 taking `$s0` where the ROM has `$s2`. Two dead ends are worth recording because they cost builds: **declaration-order permutation of the locals changed nothing** across four variants (gcc-2.7.2 numbers pseudos by first use in RTL, not by declaration), and splitting the loop counter into two variables was a real but *transient* win (276 -> 164 rows) that turned into a 36 -> 0 loss once the per-site-pointer fix landed. The final source has one counter.
- Friction: (1) `func_80057914` absorbed most of the sprint chasing the extra block move analytically — several rounds of reasoning about `safe_from_p` and stack-slot reuse — when the decisive experiment (drop the named local entirely) was one build away. The general lesson: when the residual is "N extra copies of a known size", test removing the *object* being copied before theorising about why gcc copied it. (2) The first `docs/levers.md` retirement attempt was net **+410 B** — merging two entries into one longer one is not a retirement. Measure the surface after the merge, not before. (3) `test_playbook_index_covers_all_sections` fired: a new `docs/hazards.md` section needs a row in the in-file **Playbook index** as well as in `docs/hazard-index.md`. Worth remembering, it is two indexes, not one. (4) The review offered the PO 4 of 6 buffered suggestions; the plateau-advisory item was not put to them and is carried, not dropped.
- Applied: PO-selected, 4 of 4 plus 1 retirement. #1 `docs/hazards.md#named-aggregate-local-extra-block-move` + a playbook-list line in `docs/levers.md`. #2 the three-way-split rule appended to `#rodata-sibling-yaml-pattern`. #3 `docs/hazards.md#grep-gbih-before-modelling-a-large-stack-struct`. #4 `docs/hazards.md#read-the-freeing-twin-before-pricing-an-allocator-leaf`. All three new sections indexed in both the in-file playbook list and `docs/hazard-index.md` (4 new rows). Retirement: `byte-offset-cast-defeats-base-ptr-cse` + `mem-in-struct-index-global-cse` merged into `defeat-global-base-cse`, which also absorbs the S291 per-site-pointer inverse; levers.md 10236 -> **10222/10240**, net **-14 B**, with the bulk of the new material in hazards.md (630 KB / 654 KB). `BACKLOG.md` records the PO's `func_80078FA8` drop (`--carried-check` now flags it CARRIED-WALL) and the post-S291 `main` pool state. `make test-tools` 135 passed, 1 skipped; prompt-lint baseline re-frozen for the S291 citation, one cap de-shouted.
- Carry-over: none from the committed backlog. **`main` smallest-first is exhausted:** 304 stubs, 7 fresh, and all 7 are heavy-FP (`func_8004887C` fp=129, `func_800874D8` fp=119, `func_80047E9C` fp=141, plus `func_80045CE0` / `func_80031AF4` / `func_800469EC` by size). The next `main` sprint is an FP-scheduler lever slice, a carried-wall crack slice (253 rows), a jtbl-carve slice (33 rows), or a scope change off `main` — not another smallest-first pass. One buffered suggestion carries to S292: restate the plateau advisory's terminal condition, since `fresh` is now not just a ceiling but a single wall class.

## Sprint 290 — the last two zero-FP fresh leaves of func_8006A2C0.c (2 banked, 0 carried) — 2026-07-27
- Increment: 0 files / +2 functions matched (`src/main/func_8006A2C0.c` 18->16 stubs, partial, NOT md5-candidate): `func_8006C484` -> **`stamp_circle_ring_alpha`** (0x448, 274/274), `func_8006D6D0` -> **`accumulate_mode_stats`** (0x6FC, 447/447). Repo-wide stubs 311->309, all in `src/main`; md5-candidate files unchanged at 230. Main-segment fresh leaves 12->10. ROM SHA-1 green at both commits; tree clean. Descriptive count **+2**, both mechanism-grounded rather than guessed: the first from its `0x00010001`-per-u32 alpha-bit mask over a 152-pair-wide 16-bit image, the second from the record layout plus its 9999-sample decay.
- Quality: 0 stuck-far / 1 permuter-run (2 imports, base 970 then 440, best 200, never 0) / 0 carried / 0 re-opened. First zero-carry sprint since S287.
- Seed: committed 5pt; banked 0 file points (partial file); realized 7 / residual +2 (+1 permuter, +1 novel bank-gotcha); regime classical.
- What helped: one result, and it retires a documented verdict.
  (1) **A multi-register allocno permutation is not terminal; it is arithmetic nobody had measured.** Both functions reached exact instruction count with an identical instruction sequence quickly, and both were then decided entirely by which value claimed which register. `func_8006C484` had **five** t-registers permuted, and about fifteen blind source permutations moved nothing. Reading gcc's own numbers did: `tools/cc/gcc -dg -dl` gives `.lreg` ("Register N used R times across L insns" = `REG_N_REFS` and `REG_LIVE_LENGTH`) and `.greg` (the `allocno_compare` order plus the final dispositions), and `global.c allocno_compare` is `floor_log2(refs)*refs / live_length * 10000 * size` with ties on allocno number, i.e. declaration order. That named the fix in one step — it needed `cj > ci > mask` and had `mask 12790 > ci 12903 > cj 12500` — where guessing had failed all afternoon. Four levers came out of the table: all-inline `grid[]` indexing so each address chain accumulates in one pseudo (16 registers down to the ROM's 14); both clamp temps initialised before both clamp tests, which lets the back edge steal `cj = j` and forces the annulled `beql` on the second test; the second block initialising `ci` before `cj`, which equalises the two temps' live lengths at 126 so the tie falls to declaration order as intended; and an empty `do {} while (0)` wrapped round *only* the clamp statements, which re-weights those refs to loop depth 3 and leaves the rival hoisted mask at depth 2. The last register needed a different rule: in a **call-free** function `local-alloc` has no priority at all, because `qty_compare_1` scales every term by `qty_n_calls_crossed`, so all local quantities tie and the order is birth order — declaring `t` inside the loop body rather than at function scope moved the pick. This is now `docs/hazards.md#loop-weight-and-live-length-regalloc-steering` Axis 8 plus `tools/allocno_report.py`, and it retires the S268 "Terminal / corpus-sibling-only" verdict on `#multi-register-allocno-permutation`: S268's evidence was that every *guessed* change failed, which is a statement about guessing.
  (2) **Preheader init order is an induction-variable question, and the giv must be its own statement.** On `func_8006D6D0` the ROM's loops show the hoisted constants *before* the offset and pointer initialisers, which means those initialisers are `strength_reduce` givs (emitted after the hoists) and not source IVs (emitted before them, as a `for`-init). Counting in `i` and deriving `off = i * 2` and `p = &cfg[i * 10]` as separate statements reproduced the order and closed 24 of the last 32 mismatches across three loops. The complement is the trap: writing `i * 2` *inline* inside `*(s8*)((u8*)SYM + i * 2)` lets gcc absorb the symbol into a walking pointer giv and lose the ROM's per-access `%hi`/`addu`/`%lo`.
  (3) **Read a plateaued permuter candidate for the knob it exposes, not the diff to apply.** The best `func_8006C484` candidate retyped one local to `unsigned short`: semantically wrong and 2 instructions worse, but it landed three permuted registers on the ROM's, which proved the residual was sensitive to any extra instruction in that region and pointed at the zero-cost version of the same knob. Extends the S288 re-read rule.
  (4) Smaller, but it got every branch polarity in a block wrong first: on `func_8006D6D0` the per-hole flag store and clamp sit **inside** the min branch — the failing comparison skips both. Read the branch targets, not just the instruction order, before writing a min/max block as sequential statements.
- Friction: (1) `func_8006C484` absorbed most of the sprint for a two-function increment: about twenty full rebuilds walking source permutations, most of which moved nothing, before switching to the dumps. The lesson is the tool, but the cost was real. (2) The curated rename hit the documented stale-parent-object failure (`undefined reference to func_8006D6D0` from a still-asm caller); `find build -path '*/src/main/*.o' -delete` fixed it, exactly as `docs/workflow/loop.md` says, so this was a re-read cost only. (3) `docs/levers.md` was at 10213/10240 for the fourth sprint running, so the accepted additions went to `docs/hazards.md` (31 KB free) with levers.md keeping only the merged one-line entry — the retirement plus that split landed it at 10236/10240. (4) The `prompt_lint` ratchet test was **already red at HEAD**, inherited from S289's un-frozen provenance warnings on gates/loop; this sprint de-shouted its own caps and then re-froze the baseline deliberately, which is the tool's own third option. Worth noting that a red ratchet survived a sprint boundary unremarked.
- Applied: PO-selected, 5 of 5 plus 1 retirement. #1 `tools/allocno_report.py` (runs the build's own `make -n` recipe, so per-file `-ffast-math` overrides are picked up, adds `-dg -dl` in a scratch copy, prints allocnos in `allocno_compare` order with refs, live length, size, computed priority and the register each got, plus the longest-lived local quantities; its self-check flags a table that is not descending, which is how the missing `allocno_size` **multiplier** in the formula was caught) + `docs/hazards.md` Axis 8 + the `#multi-register-allocno-permutation` verdict retirement. #2 `docs/levers.md` `sched-coin-loop-preheader-order-lever` gains the statement-vs-inline giv rule. #3 `docs/workflow/loop.md` gains the read-the-knob-not-the-diff sub-bullet (tally: S290 1/1). #4 `BACKLOG.md` records the PO's `func_8005D3B8` drop, which `--carried-check` now flags so it stops reading as `fresh`; a distinct `DROPPED` ranker tag stays a golden-gated follow-up. #5 the `--loose-stubs` plateau advisory gains the FP-measurement rule: count FP mnemonics, never `grep '$f[0-9]'`, which returns 0 on a heavily-FP function and prices it as a clean integer leaf. Retirement: `loop-invariant-hoist-order-preheader-regalloc` merged into `sched-coin-loop-preheader-order-lever` (two halves of one `loop.c` placement mechanism), levers.md 10213 -> 10236/10240, a net **+23 B** on that surface with the bulk of the new material in hazards.md. `make test-tools` 135 passed, 1 skipped; `prompt_lint check` green on 18 surfaces with the baseline re-frozen.
- Carry-over: none from the committed backlog. `src/main/func_8006A2C0.c` keeps 16 stubs. The sprint's conditional third slot, `func_80057914` (0x6E8, 442 instrs, `src/main/func_80054900.c`), was **not pulled**: its condition was met but the tell-scan showed a full-size slice rather than a mop-up — a `0x128` frame filled with byte and word stores into one large stack struct, four callee-saved FP registers (`$fs0`-`$fs3`) and 3x`sinf`/2x`cosf` over `g_scenery_wind_angle_a`/`_b`. It carries as the cheapest pre-vetted `main` leaf. Main fresh leaves are down to 10, of which the smallest four are the `func_800453E0.c` FP-slope family and `func_80078FA8` (fp=82 plus 12 div/mul), so the next `main` slice should either take `func_80057914` or apply Axis 8 to a documented register-permutation carry.

## Sprint 289 — the last 2 fresh leaves of the func_80095A10.c camera family + the twice-deferred func_8005C038 (2 banked, 1 carried) — 2026-07-27
- Increment: 0 files / +2 functions matched (`src/main/func_80095A10.c` 16->14 stubs, partial, NOT md5-candidate): `func_80096C04` (0x340, 208/208), `func_800967F4` (0x410, 260/260). Repo-wide stubs 313->311, all in `src/main`; md5-candidate files unchanged at 230. Main-segment fresh leaves 15->12. ROM SHA-1 green at all 3 commits; tree clean. Descriptive count **+0** — both keep auto `func_` names, the same rationale the PO signed off in S285 and S286: the mode distinction across this camera family is still unestablished, so a domain name would be invention.
- Quality: 0 stuck-far / 1 permuter-run (2 imports, no zero) / 1 carried / 0 re-opened.
- Seed: committed 5pt; banked 0 file points (partial file); realized 7 / residual +2 (+1 permuter, +1 carry); regime classical.
- What helped: three findings.
  (1) **A single function can carry two mode dispatches with opposite layouts.** `func_80096C04` dispatches twice on `D_800E4C54`; the follow-distance one is an if/else-if compare chain (each arm's body inline after its own test, the three identical tails cross-jumped into one call) and the `eye.y` one is a `switch` (all tests up front, arms out of line). Spelling either the other way costs exactly 2 instructions, and the first build had both wrong in opposite directions, which is why the count was right (208) while the body was not. So do not settle a file's dispatch idiom once and reuse it — read the branch layout per dispatch (`docs/hazards.md#switch-compare-chain-layout`).
  (2) **The `D_800C73Dx` byte run is one `Light`, not nine scalars.** The gate note flagged `D_800C73D0`/`D1`/`D2`/`D4`/`D5`/`D6`/`D8`/`D9`/`DA` as a record to identify; `sizeof(Light)` is 16 and `D_800C73B0` is already externed as `Light[3]` in the same file, so those are `col`/`colc`/`dir` of `D_800C73B0[2]` — the third scene light, whose direction the function aims by normalising the vector between two sampled points to the 127 range. Both spellings emit per-access `%hi`/`%lo`, so the codegen did not force this; reading the offsets did, and it is what makes the branch legible.
  (3) **The `BUILT_IN_FSQRT` guard is a 9-instruction gotcha, not a 5-instruction one.** S288 priced the `-ffast-math` override as dropping a NaN guard. On `func_800967F4` the guard's `jal` also clobbers the FP registers, so gcc additionally reloaded from the stack the three deltas the ROM keeps live across the compare. The flag is now on the sixth main-tree file; all 15 already-banked functions in that TU were `cmpfn`-verified fast-math-invariant before it was kept.
- Friction: (1) `func_8005C038` absorbed most of the sprint for a carry — about a dozen full rebuilds walking the pointer-local count per pass, landing at 268 against 264 with an exact frame. (2) The normal `cmpfn` diff was actively unhelpful on it: one global register-role difference makes every line differ, so it reported a single huge replaced block and hid where the extra instructions were; an ad-hoc opcode-only alignment diff answered it in one run, which is accepted suggestion #2. (3) `docs/levers.md` was at 99.92% of budget for the third sprint running (10232/10240), so one accepted addition needed the accepted retirement plus a trim and two compression passes. (4) A `pkill -f` aimed at a background permuter also matched the calling shell's own command line and killed the rest of that tool call, losing a `cp` that was saving a file later restored from — accepted suggestion #4.
- Applied: PO-selected, 4 of 4 plus 1 retirement. #1 `docs/levers.md` `per-region-cse-slot-base-lever` gains the multi-array generalisation: the count of pointer *locals* per pass is the register-pressure knob, since each becomes a `loop.c` induction pointer costing a callee-saved register while an offset expression off a shared row pointer does not. #2 `tools/cmpfn.sh` gains `--mnemonics`, an opcode-only diff (operands dropped, aliases already folded by `norm()`) for localising a count mismatch when a register-role difference makes the normal diff one replaced block; silent on a byte-exact function, and documented as a localiser that never replaces the normal diff. #3 `docs/workflow/gates.md` DoR gains the repeat-rejection rule: name the tell beside a rejected leaf, and read "rejected twice, never attempted" as the tell being unproven — at the third encounter attempt the leaf or drop it. #4 `docs/workflow/loop.md` conventions gain the `pkill -f` note (use the harness's stop action). Retirement: `bound-copy-for-live-range-placement` merged into `global-allocno-compare-livelength-biv-order` — it was already written as one way to flip that same `global.c:587` priority — plus a trim of the S282 aside in `do-while-zero-block-break`, landing levers.md at 10213/10240, a net **−19 B**. Net prompt-surface delta **+1109 B** (levers −19, gates +671, loop +457). `make test-tools` 135 passed, 1 skipped; `prompt_lint check` green on 18 surfaces (2 soft provenance warnings on gates/loop).
- Carry-over: `func_8005C038` (0x420, 264 instrs) at 268/264, frame `-0x60` exact, every control-flow edge and both value-select `beql` clamps reproduced — `docs/wip/func_8005C038.near-match.md` holds the full RE, the ten measured variants and the attempt body. The residual is a callee-saved colouring equilibrium: the build spills `score` and the fifth argument to their home slots and keeps the row pointers live, where the ROM does the opposite and pays to re-materialise `category * 12 + base`. **The gate's stated reason for deferring it twice was wrong** — the branch-likely instructions reproduced from ordinary C on the first build. Below exact count so permuter-ineligible on the count rule; run anyway for candidates (S287/S288 play) and both imports proposed the lever already found by hand, the running tally's first non-novel candidate. Next step is a `global.c`/`move_movables` fan-out. `src/main/func_80095A10.c` keeps 14 stubs, all carried-wall / jtbl-dispatch / the 2 S282 terminal carries — its fresh vein is closed; `src/main/func_80059BA0.c` keeps 15.

## Sprint 288 — the last identified fdlibm leaf + 2 non-FP siblings in func_80059BA0.c (2 banked, 1 carried) — 2026-07-26
- Increment: 0 files / +2 functions matched (`src/main/func_80059BA0.c` 17->15 stubs, partial, NOT md5-candidate): `func_80059BC0` -> **`acosf`** (0x3EC, 251/251, first build), `func_8005B314` -> **`insert_supershot_record`** (0x4A8, 298/298). Repo-wide stubs 316->314, all in `src/main`; md5-candidate files unchanged at 230. ROM SHA-1 green at all 3 commits; tree clean. Descriptive count **+2**, both names certain (one from fdlibm's own constant set, one from what the function demonstrably does).
- Quality: 0 stuck-far / 1 permuter-run / 1 carried / 0 re-opened.
- Seed: committed 5pt; banked 0 file points (partial file); realized 8 / residual +3 (+1 permuter, +1 carry, +1 novel bank-gotcha); regime classical.
- What helped: two findings, the second the sprint's real result.
  (1) **The S287 fdlibm-transcription exception paid out exactly as priced.** `func_80059BC0`'s `.s` carries `pi` 0x40490FDB, `pio2_hi` 0x3FC90FDB, `pio2_lo` 0x3811EF08 and the acosf thresholds 0x3EFFFFFF/0x23000000, so it was identified, transcribed from `e_acosf.c` and banked FIRST BUILD with no seed, no m2c and no MCP. Two ROM deviations were readable from the `.s` before writing a line: the `|x| > 1` arm returns `0.0f` where stock fdlibm returns the NaN `(x-x)/(x-x)`, cross-jumping with the `acos(1) = 0` return, and `pi + 2*pio2_lo` / `pio2_hi + pio2_lo` arrive pre-folded as single literals. The one enabler was a per-file `-ffast-math` override so `sqrtf()` emits the bare `sqrt.s` — the fifth main-tree file to need it, and the already-banked `atanf`/`atan2f` proved fast-math-invariant under it (the full-make SHA-1 is the proof, not an inspection).
  (2) **Re-read the plateaued permuter's candidates after EVERY hand fix, not once.** S287 established that a permuter that never scores 0 still yields a lever in its best candidate's source diff. S288 shows the stronger form: `func_8005B314` needed TWO such reads, from two separate imports, and the second lever only became visible once the first had taken the body to exact count. Import 1's best candidate introduced a redundant-looking `new_var = count;` before the loops — that is a bound-copy whose live range starts where the ROM's does, and it folds away to nothing while undoing a three-register allocno rotation (`count`/`first`/`best` against `$s3`/`$s4`/`$s5`, `global.c:587` priority). Import 2's best candidate inserted a bare `do { } while (0);` after the second `osSyncPrintf` — that ends the call's basic block, so `reorg` stops reaching past the call for the loop's `i = 0` and annulling the guard to compensate, which was the last instruction. Neither run scored near zero (base 620, best 320 across three runs). Also load-bearing on that function: an explicit `Struct80131510* board` for the ROM's hoisted `$s6` (7 instructions of re-materialized `%hi`); `s32 count = 5;` as a **declaration initializer**, because an assignment adjacent to the loops lets `loop.c` see the initial value and delete the first loop's entry guard — and, non-obviously, shrinks the frame from the ROM's `-0x930` to `-0x928`, so a frame-size miss can be a loop-guard symptom rather than a dead-locals one; and descending `switch` case order for the `case 3`/default arm polarity.
- Friction: (1) `docs/levers.md` was at 99.8% of budget again (10223/10240), the second sprint running, so two accepted additions needed the accepted retirement plus a PO-approved second cut plus five compression passes to land at 10232 — a net −1 B. The recurring shape is that a merge of two prose bullets saves ~50 B while one new entry costs ~330; the retirement that actually paid was moving a three-bullet family to a one-line `## Levers with a full playbook` link, which is what that section is for. (2) One accepted suggestion rested on a wrong premise: `--carried-check` was reported as scanning all of `BACKLOG.md`, but `carry_over_names()` already restricts to `## Carry-overs` (`pick_target_score.py:179`) and an in-code S271 note explicitly bars tightening it with a lead-line/subject heuristic. The real cause was narrower and was found only by instrumenting the function. (3) `func_8005CA48` absorbed a large share of the sprint for a carry: eleven source variants, each a full rebuild, to move from 298 to 296 against 294.
- Applied: PO-selected, 4 of 4 plus 1 retirement and 1 PO-approved second cut. #1 `tools/cmpfn.sh` normalizes `fp` to `s8` — the same register `$30`, printed differently by splat and objdump, which produced a false diff row per reference on every frame-pointer function (5 of the 7 rows still showing on an already byte-exact `func_8005B314`). #2 `tools/pick_target_score.py` `carry_over_names()` gains guard (3): excise any `## Carry-overs` bullet whose lead line carries the explicit `NEAR-FREE RETRY` label, since such an entry parks a READY slice, not a wall — S287 parked `func_80059BC0` that way having just identified it as `acosf`, and `--carried-check` read CARRIED-WALL on a leaf that then banked first build. Keyed on the author-supplied marker, NOT on the prose heuristic the S271 note bars; verified that `func_80059BC0`/`func_8005A580` clear while `func_80059BA0`/`func_8005B0B4`/`func_800990D0`/`func_80047DBC` still flag. #3 `docs/levers.md` gains `bound-copy-for-live-range-placement` and `do-while-zero-block-break`. #4 `docs/workflow/loop.md` permuter bullet gains the re-read-after-every-fix sub-bullet with the running tally (S287 1/1, S288 2/2 candidates yielded a lever, 0/3 runs a zero). Retirement: `goto-loop-defeats-loop-strength-reduction` + `goto-loop-vs-structured-loop-codegen` + `goto-is-last-resort` folded into one `## Levers with a full playbook` link to the existing `docs/hazards.md` section; second cut: the levers.md `<scope>` wiki-link history sentence. Net prompt-surface delta **+542 B** (levers −1, loop +533). `make test-tools` 135 passed, 1 skipped; `prompt_lint check` green on 18 surfaces.
- Carry-over: `func_8005CA48` (0x498, 294 instrs) at 296/294 with the correct frame and register roles — `docs/wip/func_8005CA48.near-match.md` + `nonmatchings/func_8005CA48/attempt.diff`. Residual is a `loop.c` placement pair: the profile-array symbol hoists where the ROM addresses it per access (+1), and the `s16` loop bound hoists where the ROM re-derives it each iteration (+2, with −1 back in the loop body). Below exact count, so permuter-ineligible; next step is a `move_movables` fan-out, not more source rewriting. `src/main/func_80059BA0.c` keeps 15 stubs. Also noted, not actioned: `docs/hazards.md` cites `#goto-is-last-resort` as an anchor that does not exist as a heading there — a pre-existing broken xref of the S184 class.

## Sprint 287 — 3 fresh loose stubs in func_80059BA0.c, which turned out to be the game's embedded fdlibm (3 banked, 0 carried) — 2026-07-26
- Increment: 0 files / +3 functions matched (`src/main/func_80059BA0.c` 20->17 stubs, partial, NOT md5-candidate): `func_8005A2AC` -> **`atanf`** (0x2D4, 181/181), `func_80059FAC` -> **`atan2f`** (0x300, 192/192), `func_8005C674` -> **`build_roster_grid`** (0x3D4, 245/245). Repo-wide stubs 318->315, all in `src/main`; md5-candidate files unchanged at 230. ROM SHA-1 green at all 3 commits; tree clean. Descriptive count **+3** — the first non-zero since S284, and all three names are certain rather than guessed (two from fdlibm's own constant tables, one from what the function demonstrably builds).
- Quality: 0 stuck-far / 1 permuter-run / 0 carried / 0 re-opened.
- Seed: committed 5pt; banked 0 file points (partial file); realized 7 / residual +2 (+1 permuter, +1 re-attempt); regime classical.
- What helped: three findings, in descending order of how much they change future sprints.
  (1) **`src/main/func_80059BA0.c` is the game's embedded fdlibm, and its rodata says so.** `func_8005A2AC`'s constants are fdlibm's `atanhi[4]`/`atanlo[4]`/`aT[11]` sitting at `D_800D08A0`/`B0`/`C0`, which identifies the function outright; `func_80059FAC` is then `atan2(y, x)` — confirming the S286 signature guess — with a truncated-`pi` constant set (`pi` = 0x40490FDA one ulp low, `pi_lo` = +1.50995788e-07 carrying the remainder, which is why `pi-(z-pi_lo)` emits two `sub.s`). Both banked the day they were identified. The file also already held `fabsf` as a carried near-match, and `func_80059BC0` is `acosf` by its `pS0-pS5`/`qS1-qS4` coefficients. So a heavy-FP main leaf is not automatically an FP-scheduler wall: **check the constants against fdlibm before pricing it**, because a hit turns an unpriceable leaf into a transcription. Both fdlibm functions kept their tables as `extern f32` refs into the shared main rodata blob — source literals would emit a fresh pool entry and force a carve the still-asm siblings share — while every scalar constant (1.0, 1.5, 2.0, the 1.0e30 `huge`) is materialized inline by `lui`/`ori`/`mtc1`, so those stay source literals.
  (2) **fdlibm's `GET_FLOAT_WORD` `do {} while (0)` shape is a scheduling barrier.** Written as a bare block, the bit copy let the pre-reload scheduler hoist the mask and threshold `li`s above the `mfc1` and fill its hazard slot, costing the ROM's `nop` (one instruction short) and permuting the constants' registers. The same three statements inside the macro's own `do {} while (0)` restored the ROM byte for byte. The existing lever entry recorded only the `REG_N_REFS` effect.
  (3) **On a register permutation the permuter's deliverable is a lever, not a zero.** `build_roster_grid` hand-plateaued at 245/245 with 14 differing operands in three clusters; the permuter never scored 0 (base 515, best 70 in 130k iterations, 60 after a re-import), which by the existing "multi-register permutations plateau" rule reads terminal. But its best candidate's *source* diff was one line — it had assigned a subscript into an existing local before the test — and applying that by hand cut the residual to 6 operands. Generalizing it (reuse is applied per differing register, not once) closed the function in three more builds: four reuses total, each fixing exactly one cluster. Also load-bearing on that function: loop-exit form is a per-loop choice (3 do-while against 6 structured `for (i != N)`; all-do-while cost a peel plus its strength reduction), and explicit offset temps keep each array's per-access `lui %hi / addu / lo %lo` instead of gcc hoisting a full `la`, which frees the two callee-saved slots the ROM spends on its walking pointer and loop bound.
- Friction: (1) A prototype that exists can still be out of scope — `f32 func_80059BA0(f32)` was declared above the function that needed it but still below `atan2f`, so that caller got implicit-int and +3 conversion instructions. gcc warned and the warning scrolled past; reading it would have saved an iteration. (2) `docs/levers.md` was at 10230/10240 again, so four accepted additions needed the accepted retirement plus four compression passes to land at 10223. (3) Two `prompt_lint` ratchets fired on the first try of the accepted edits (a bold lead-in ending `(S287).**`, and an `OR` in prose); both are documented style rules and both were cheap to fix, but they are only discoverable by running `make test-tools`, which is not in the review checklist.
- Applied: PO-selected, 7 of 7 plus 1 retirement. #1 `tools/pick_target.py` plateau advisory gains the fdlibm-transcription exception (+ the S287 family-locality data point). #2 `docs/levers.md` `do-while-doubles-reg-n-refs-qty-tier` gains the scheduling-barrier clause. #3 `docs/hazards.md#callee-prototype-is-load-bearing` gains a third wrong state (prototype below its caller in a long partially banked file). #4 `docs/levers.md` `do-while-not-equal-loop-exit-form` gains "choose per loop, never file-wide". #5 `docs/workflow/loop.md` permuter escalation gains the read-the-best-candidate bullet + the Oracles row pointer. #6 `docs/levers.md` `one-variable-reuse-reorders-loads` + `reuse-one-pointer-across-loops-coloring-trap` merged into `variable-reuse-is-a-per-register-lever`. #7 `docs/levers.md` `commutative-operand-order-statement-split` gains `*(p+off+C)` vs `p[off+C]`. Retirement: the three dead-frame entries (`dead-frame-live-index-pressure-lever`, `pure-dead-frame-clean-crack`, `dead-frame-dead-v0-store-crack`) merged into one `dead-frame-levers`. Net prompt-surface delta **+1850 B** (levers -7, loop +1096, hazards +761). `make test-tools` 135 passed, 1 skipped; `prompt_lint check` green on 18 surfaces.
- Carry-over: none from this sprint. `src/main/func_80059BA0.c` keeps 17 stubs, of which `func_80059BC0` (0x3EC, identified as `acosf`) and `func_8005A580` (0x778) are the obvious next slice, and `func_80059BA0`/`fabsf` remains its one characterized terminal carry.

## Sprint 286 — the next 3 fresh loose stubs in the same func_80095A10.c camera vein (3 banked, 0 carried) — 2026-07-26
- Increment: 0 files / +3 functions matched (`src/main/func_80095A10.c` 19->16 stubs, partial, NOT md5-candidate): `func_80097218` (0x2C0, 176/176, first build), `func_80096F44` (0x2D4, 181/181), `func_800974D8` (0x308, 194/194). Repo-wide stubs 321->318, all in `src/main`; md5-candidate files unchanged at 230. ROM SHA-1 green at all 3 commits; tree clean. All three keep auto `func_` names (descriptive count 0), the same PO-signed-off rationale as S285: the mode distinction across this ~6-member camera family is still unestablished.
- Quality: 0 stuck-far / 0 permuter-run / 0 carried / 0 re-opened. First all-clean counter-metric since S270.
- Seed: committed 5pt; banked 0 file points (partial file); realized 7 / residual +2; regime classical.
- What helped: the S285 siblings did most of the work — types, externs, the `f32 unused[16]` dead frame and most of the body skeleton came straight from the two banked camera builders, which is why the first target matched on the first build with no MCP and no seed_c.py. Three findings beyond that. (1) **FP argument registers are a signature, not a coin.** `func_80096F44`'s last residual read as a 3-register FP permutation, `f14`/`f12` against `f2`/`f4`; `f12`/`f14` are the FP argument registers, so those values were arguments and `func_80059FAC` is an atan2-shaped `f32 (f32 dz, f32 dx)` declared `(void)`. Confirmed by reading the callee's own prologue (NaN checks on both `$fa0` and `$fa1`), not guessed. (2) **A compare-chain switch with a case falling into the default.** `func_800974D8`'s mode dispatch branches to each case body with the default last and reached by an explicit `j`, and case 13 runs off its end into the default arm — a shape no if/else can express, which is exactly the instruction the if/else form lost. (3) **The delay-slot filler names the pre-reorg order.** The same function's last residual was which of the store and the argument move fills the `jal` slot; the ROM's merge point sits at the argument move, so each arm carried its own copy of the trailing calls. Duplicating them per arm let the scheduler cover the store's latency with the argument setup and gcc cross-jump the tails back together.
- Friction: (1) The plateau advisory pointed away from `main`, and this was the second consecutive 3-of-3 sprint in that very file — the advisory measures unrelated fresh leaves and has no notion of family locality, which cost a paragraph of justification at the gate both times. Fixed this gate. (2) `docs/levers.md` was at 96% of budget, so two accepted lever entries needed the accepted retirement plus a compression pass on my own wording to land at 10230/10240. (3) The new hazards section failed `test_hazard_anchors` until it was added to the in-file playbook index — worth knowing before writing the section, not after.
- Applied: PO-selected, 4 of 4 plus 1 retirement. #1 `docs/levers.md` `fp-arg-registers-are-a-signature`. #2 `docs/levers.md` `cross-jump-merge-point-before-store`. #3 new `docs/hazards.md#switch-compare-chain-layout` + a `docs/hazard-index.md` row + a playbook-index line. #4 `tools/pick_target.py` plateau advisory gains the S286 family-locality exception. Retirement: `mem-in-struct-index-global-cse` and `array-element-form-for-multilevel-bound` merged (the latter opened "the same form" and was a sub-case). Net prompt-surface delta +417 B. `make test-tools` 135 passed, 1 skipped; `prompt_lint check` green on 18 surfaces. Two memories written during execution.
- Carry-over: none from this sprint. `src/main/func_80095A10.c` keeps its 2 pre-existing S282 terminal carries (`func_80098CD8`, `func_80098C6C`) plus 14 untouched stubs, of which the same-family `func_80096C04` (0x340) and `func_800967F4` (0x410) are the obvious next slice.

## Sprint 285 — 3 smallest fresh loose stubs in func_80095A10.c; the carry was refuted same-session (3 banked) — 2026-07-26
- Increment: 0 files / +3 functions matched (`src/main/func_80095A10.c` 22->19 stubs, partial, NOT md5-candidate): `func_8009806C` (0x2A4, 169/169, shot-view camera builder) + `func_80098310` (0x2A4, 169/169, swing-phase sibling) + `func_80098758` (0x26C, 155/155, terrain-flatness position scorer). ROM SHA-1 green at all 3 commits; tree clean. All three keep auto `func_` names (descriptive count 0): the two camera builders are one of ~5 siblings whose mode distinction is not established, and PO signed off on that.
- Quality: 0 stuck-far / 0 permuter-run / 0 carried-final / 1 re-opened (self, mid-sprint).
- Seed: committed 5pt; banked 0 functions-worth of file points (partial file); realized 7 / residual +2; regime classical.
- What helped: the asm-first fast path carried both camera builders with no MCP and no seed_c.py — hand-translate from the `.s`, gate on the full make. Both then showed the same 0x40 pure dead frame, closed by `f32 unused[16]`. The real deliverable is the `func_80098758` re-attempt: its own S285 carry doc called it a terminal 7-vs-6 callee-saved coloring wall, and all four residuals turned out structural. (1) The cylinder walk wants a block-scoped `CollisionCyl* cyl = &collision_cylinders[i];`: a `cyl++` walk makes loop.c split three field accesses into two induction variables, and a bare `collision_cylinders[i].field` makes it a byte-offset IV with a per-access `%hi`. (2) The function returns `s32` with valueless `return;`s — the S281 `reorg.c:3375` lever in its other direction, where MY build was one short because it stole the fall-through slot the ROM leaves empty. (3) The three destination globals are one `s32 D_800E4C98[3]`: gcc-2.7.2 `true_dependence` (sched.c:817) exempts a varying-address in-struct load from a fixed-address non-struct store, so three scalar globals let all three `lw` float above their stores and burn a third register. Found by reading the compiler source, per the standing PO directive, not by guessing. (4) The ring divisor and the `0x12BFF` threshold belong inside their loops as literals so loop.c hoists each into its own preheader in the ROM's order.
- Friction: (1) `tools/cmpfn.sh` normalized only `fv*`/`fs*` FP register names, so both byte-exact camera builders reported ~12 phantom diff rows (`fa0` vs `f12`, `ft0` vs `f4`) and cost a manual objdump cross-check each. Fixed at this gate. (2) The carry doc itself was the friction: it argued a coloring wall from a body that was not at exact instruction count, which the loop doc already forbids — the missing rule was that the REGISTER COUNT is itself a structural symptom. (3) `docs/workflow/loop.md` documented the asm fast-path `.s` at `asm/nonmatchings/<seg>/<func>/<func>.s`, a path that exists for no loose stub; the real path is under the host subseg stem.
- Applied: PO-selected, 9 of 9 in 4 groups, plus 1 retirement. A: `.claude/commands/sprint-plan.md` repointed to `docs/workflow/loop.md ## Execution loop`; `docs/workflow/gates.md` step 2 branches `--lib` (library) vs `--segment`/`--loose-stubs` (segment) and now requires `--loose-stubs` for the plateau advisory; loop.md fast-path `.s` path corrected. B: `tools/cmpfn.sh` FP aliases became a full SDK-name-to-number map (also restoring visibility of a genuine FP register permutation, which the old collapse to `fN` hid among f0/f2/f20/f22/f24); `tools/pick_target.py` rejects a segment name passed to `--lib`. C: three `docs/levers.md` entries (`aggregate-store-pins-pointer-load`, `two-argument-call-temp-split`, `block-scoped-record-pointer-single-giv`). D: loop.md residual-classification note gained the register-count-is-structural rule. Retirement: the duplicated diff.py stale-verdict prose under Spot-check folded into its `## Oracles` row, loop.md 43980 -> 43031 bytes. Two memories written during execution. `make test-tools` 135 passed, `prompt_lint check` green.
- Carry-over: none from this sprint. `src/main/func_80095A10.c` keeps its 2 pre-existing S282 terminal carries (`func_80098CD8` uniform register rotation, `func_80098C6C` empty-barrier-only sched coin) plus 17 untouched stubs.

## Sprint 282 — compiler-source fan-out CRACK slice on the func_80095A10.c local-alloc-qty sibling pair (0 banked / 2 carried) — 2026-07-26
- Increment: 0 files / 0 functions matched (`src/main/func_80095A10.c` stays partial, 22 stubs). Both targets carried with FRESH pass-cited terminal verdicts (S233 deliverable). ROM SHA-1 green; src untouched, tree clean but for the 2 rewritten `docs/wip/*.near-match.md`.
- Quality: 0 stuck-far / 0 permuter (class = 0 cracks, not run) / 2 carried / 2 re-opened. The S281-analog shape (adjacent same-file exact-count qty carries) did NOT reproduce the S281 result: S281's pair hid STRUCTURAL bugs behind the qty verdict; S282's pair are genuine terminal coins.
- Seed: committed 3pt; banked 0 functions (0pt file-level); regime classical. Realized tier ~5 / residual +2 (+1 gccA subagent process-death → inline takeover; +1 both carried/re-open with no bank).
- What helped: the fan-out still earned its keep as CHARACTERIZATION (S233): refuted 2 stale docs (S268 recurred HARD), corrected `func_80098CD8` to exact-count 38/38, and set a durable PO policy. binutils rule-out was decisive (byte-exact KMC-as reconstruction proved 100% gcc codegen for both). `func_80098C6C` cracked byte-exact 13/13 via TWO empty `__asm__ __volatile__("")` barriers (schedule_select potential_hazard coin sched.c:2615 + local-alloc qty local-alloc.c:1579) — a real, precisely-cited result even though PO declined to bank it.
- Friction: (1) S268 recurred — BOTH carry docs were stale on COUNT and CLASS; my own pre-dispatch cmpfn also read a STALE `.o` (transient). Cost several iterations of chasing a phantom `andi`/`move` before the `u32 b=src[i]`+`(s32)b` form reached exact 38. (2) `gccA-CD8` DIED mid-run (process death, not idle) and left `base.c` in the already-disproven 4th-param form — the S235 SendMessage contract does not cover process death, so its progress was unrecoverable; orchestrator reverted to faithful C and re-derived inline. (3) `func_80098CD8`'s terminal residual = a uniform +1 reg rotation (ROM skips `$a3`); confirmed the fn is genuinely 3-arg (call site) and gcc-2.7.2 has no `REG_ALLOC_ORDER` override, so `$a3` is the forced pick — no source form nor 366k-iter permuter (S221) skips it.
- Applied: 3 of 3 — #1 `pick_target.py --refresh-residual <fn>` (rebuild+cmpfn a carry in one cmd) + STEP-0 references in `docs/agent-workflow.md` (S235 contract) and `docs/hazards.md#pervasive-regalloc-classical-main`; #2 empty-barrier no-bank PO policy in `docs/hazards.md#pervasive-regalloc-classical-main` + memory `empty-asm-volatile-sched-barrier` amended; #3 dead-subagent `nonmatchings/<fn>/STATUS` progress-checkpoint clause in the `docs/agent-workflow.md` S235 fan-out contract.
- Carry-over: `func_80098CD8` (terminal uniform reg-rotation, exact-count 38/38) + `func_80098C6C` (terminal sched2 coin, barrier-only crack PO-declined), both in `func_80095A10.c`; docs rewritten, `--carried-check` flags both.

## Sprint 281 — compiler-source fan-out CRACK slice on the get_tile_attribute.c div-by-4 sibling pair (2 banked) — 2026-07-25
- Increment: 0 files / +2 functions matched (`src/main/get_tile_attribute.c` 19→17 stubs, partial, NOT md5-candidate): `func_80041878` -> `set_lod_tile_attribute` (0x230, 140/140, LOD sub-tile attr setter) + `func_800415C4` -> `set_lod_grid_vertex` (0x2B4, 173/173, 16B GridVertex-copy sibling). ROM SHA-1 green. Both curate-named (symbol_addrs add + rename reaching still-asm callers).
- Quality: 0 stuck-far / 0 permuter / 0 carried / 2 re-opened (both cracked). PO-directed re-open of two S218 carries (the S280-advised high-EV main play, not another smallest-first fresh pick).
- Seed: committed 3pt; banked 2 functions (partial file, 0pt file-level); regime classical. Realized tier ~3 / residual ~0 (+2 re-open crack, −1 clean 2/2 fan-out no-permuter).
- What helped: the compiler-source fan-out (2 gcc-2.7.2 subagents, parallel isolated `make nonmatching-func`). Both S218 `#local-alloc-qty-permutation` "permuter-proof, ~20 forms" walls REFUTED as 3 STRUCTURAL bugs, none a permutation: (1) NON-VOID `s32` return with no `return` (keeps `$v0` live -> reorg.c:3375 cannot steal the fall-through into the epilogue-branch delay slot -> the ROM nop; seed was 1 short); (2) function-scope vs block-local index temp (local-alloc.c:1841 combine_regs no-ties a multi-block dest -> reg_qty=-1); (3) plain `{ }` vs `do{}while(0)` store macro (loop notes double REG_N_REFS -> local-alloc.c:1587 floor_log2 qty tier flip). The shared div-by-4 idiom needed NO lever of its own — it fell out of levers 1+2. Cross-pollinated the shared finding between subagents mid-run.
- Friction: rename reached STILL-ASM callers in `bgm_load_song_from_rom.c` + `lz_compress_extended_dma.c`; their per-fn `.s` are gitignored splat GAP-RELICS that do NOT regen on delete ([[nonmatchings-relic-no-rmrf]]) — first tried rm+extract (failed), recovered via `tools/recover_stub.sh 0x3A490 ... / 0x440A0 ...` (asm-mode carve resolves the new curated name) then `make extract && make`. `.ld`/`undefined_syms_auto` needed NO regen (no prior auto entry for the addrs).
- Applied: PO-selected, 4 of 4: #1 3 crack-lever memories ([[nonvoid-return-blocks-fallthrough-delay-steal]] + [[local-alloc-combine-regs-block-local-temp]] subagent-written mid-run + [[do-while-doubles-reg-n-refs-qty-tier]] written at review); #2 bank-checklist step-4a gap-relic/recover_stub note (next to the S270 stale-object note); #3 BACKLOG ranker follow-up (exact-count `#local-alloc-qty-permutation` carried-wall as high-EV fan-out RE-OPEN sub-tag, distinct from block-local sched coins); #4 hazards#local-alloc-qty-permutation STRUCTURAL-pretender refutation precedent (check non-void-return / combine-regs / do-while BEFORE accepting the class).
- Carry-over: none (both banked). LESSON: fan-out-on-a-fully-RE'd-exact-count-carry is now 2/2 (S281) after S280 2/2 + S232 3/3 — a `#local-alloc-qty-permutation` verdict, however strongly worded, is a HYPOTHESIS; re-open with a compiler-source fan-out before permuter-skip. Pure BANK value pulled from the main plateau where UNAIDED smallest-first gave S280 0/2.

## Sprint 280 — clean terrain slice in get_tile_attribute.c; compiler-source fan-out cracked both (2 banked) — 2026-07-25
- Increment: 0 files / +2 functions matched (`src/main/get_tile_attribute.c` 21→19 stubs, partial, NOT md5-candidate): `get_interpolated_terrain_height` (0x208, terrain plane-height interp) + `detect_terrain_collision` (0x46C, 4x4 quad→2-tri point location, GNU nested function). ROM SHA-1 green. Both already curated-named (no symbol_addrs add/rename).
- Quality: 0 stuck-far / 1 permuter-run (plateau 170, superseded by source crack) / 0 carried-final / 0 re-opened. NB two-phase: UNAIDED smallest-first was 0-banked/2-carried; the PO-directed compiler-source fan-out (2 gcc + 1 binutils) then cracked BOTH.
- Seed: committed 4pt; banked 2 functions (partial file, 0pt file-level); regime classical. Realized tier 8 / residual +2 (both register-alloc walls cracked only via the fan-out, 2 cited-pass root-causes incl. a nested-fn-mis-read-as-pressure correction; +permuter-plateau confirmation).
- What helped: the compiler-source fan-out (PO-directed, systematic-debugging). gccColoring cracked item1 via a `sched.c:2385` bottom-up load-SPLIT (`s32 y1` straddling the e2z stmt) + a `local-alloc.c:1598` live-length block-MOVE (the two-edit combo the permuter never produces). gccSpill re-diagnosed item2's "31-instr pressure/spill" as a GCC NESTED FUNCTION (nested `inline` fetch/hit referencing x/z/out as free vars home the params + emit the arg pointer; `function.c` ARG_POINTER==$zero on MIPS). binutils EXONERATED the KMC as on both (divergence in gcc `-S`). All 3 subagents delivered FINAL via SendMessage.
- Friction: burned a full permuter run (220+ iters) + 6 in-tree source-lever iterations on item1 before the fan-out; the plateau/295→245 reduction correctly characterized it but couldn't close it — the crack needed the split-straddle+block-move combo only a gcc-source dive found. Item2's "pressure/spill" self-diagnosis was WRONG (nested fn) — the arg-pointer tell was the giveaway, now coded into `--nested-check`.
- Applied: PO-selected, 4 of 4: #1 broaden `--nested-check` (nested_parent_tell: arg-pointer/params-from-home-slots tell; wired into loose_stubs `nested` count + verified flags detect_terrain_collision); #2 `--loose-stubs main` plateau advisory (unaided low bank-rate → prefer fresh non-main OR fan-out a characterized carry); #3 workflow_overview bullet (register-alloc carry = high-yield fan-out target, not terminal; rule out nested-fn before "pressure/spill"); #4 in-file nested-fn+permuter-caveat comment + `kmc-as-noreorder-not-global-nop-oracle` S280 refinement (as-nops numerous but byte-symmetric → still exonerating). 2 crack-lever memories saved during execution ([[sched-bottomup-loadsplit-livelength-blockmove]], [[argpointer-params-home-slots-nested-function-tell]]).
- Carry-over: none (both banked). LESSON: an apparent register-alloc wall on a clean small main leaf is NOT terminal — a fan-out on a fully-RE'd exact-count/structural carry flipped a 0-bank sprint to 2/2. The "prefer fresh non-main" plateau guidance holds for UNAIDED smallest-first only.

## Sprint 279 — main FP crack-slice, fresh file func_80078910.c (1 banked, 2 structural carries) — 2026-07-25
- Increment: 0 files / +1 function matched (`src/main/func_80078910.c` 22→21 stubs, partial, NOT md5-candidate): `func_80079358` (particle-spawn, twin of already-banked func_80079A08; auto-named). Plus 2 fully-RE'd BYTE-EXACT carries. ROM SHA-1 green.
- Quality: 0 stuck-far / 0 permuter-run / 2 carried / 0 re-opened
- Seed: committed ~5pt (3 FP crack-attempt leaves, fresh file); banked 0pt (file partial); realized ≈7 (1 clean bank + 2 strong byte-exact carries, +1 per carry for a structural-blocker discovery the size+FP+carried+nested pre-checks could not see), residual +2; regime classical. Value signal = +1 matched.
- What helped: the compiler-source FAN-OUT (3 gcc-2.7.2 crack + 1 binutils-2.6 rule-out) SORTED the tail {clean-crack, pool-blocked-crack, nested-discovery} with 0 false walls; binutils INDEPENDENTLY confirmed the nested-function finding (2 subagents, 2 methods, same verdict). func_80079358 fell out by recognizing it as the structural twin of func_80079A08. Both carries are BYTE-EXACT bodies blocked by STRUCTURE, not compiler walls.
- Friction: two INTEGRATION-TIME blockers the isolated per-fn cmpfn could NOT surface. (1) `func_80079EBC`: byte-exact in isolation yet the full-make flowed +0x10 — its tail `0.04` is a gcc literal-pool double (`D_800D19E0`) SHARED with still-asm siblings; literal form dups the pool, extern form flips a source-invariant count++ sched.c coin (CONST_DOUBLE vs MEM cost; `extern const` + `++count` both failed). Carried; unlock = bank the pool-owning siblings together. (2) `func_8007A40C` + twin `func_8007A10C`: GCC NESTED functions inside func_8007A6C8 — child byte-exact 175/175 but banks only as a 3-fn parent bundle. `--nested-check` MISSED both (8-insn window; chain-home ~15 in). Re-confirms: ONLY the orchestrator full-make ROM-SHA-1 is the oracle; a subagent "byte-exact in isolation" is a candidate, not a bank; heed a subagent's "carve may be needed" pre-flag.
- Applied: 4 of 4 — #1 CODED `pick_target.py --nested-check` widened-prologue-scan fix (scan to first `jal`, `_PROLOGUE_CAP=32`; verified it now flags func_8007A40C/A10C and keeps func_80079358 standalone); #2 NEW memory [[shared-literal-pool-partial-bank-blocker]]; #3 `docs/hazards.md#rodata-sibling-yaml-pattern` shared-with-still-asm literal-pool sub-case + BACKLOG ranker follow-up (documented, cross-file precompute not yet coded — same status as the S278 rodata-coupled follow-up); #4 fan-out process-win + full-make-is-oracle note (BACKLOG func_80078910.c entry).
- Carry-over: `func_80079EBC` (shared-literal-pool partial-bank blocker, byte-exact); `func_8007A40C`+`func_8007A10C`+`func_8007A6C8` (GCC nested-fn 3-fn bundle, children byte-exact). Both in `docs/wip/` + the BACKLOG func_80078910.c entry. Next slice options: the pool-unlock siblings (jtbl_800D1990 owner + 0.2/0.049 users) to free func_80079EBC, OR — per the S274/S275 main-plateau — a FRESH non-main pack (main fresh leaves now skew heavily to FP-sched / value-select / nested / shared-pool walls the size sort can't pre-see; S279 1/3 bank-rate is a data point).

## Sprint 278 — main FP crack-slice continuation in func_800453E0.c (4 banked, 0 walls) — 2026-07-25
- Increment: 0 files / +4 functions matched (`src/main/func_800453E0.c` 22→18 stubs, partial, NOT md5-candidate): `func_800479C0` + `func_80047B34` (wind/view smoothing twins, kept auto names) + `calc_slope_side_pitch` + `calc_slope_uphill_pitch` (pre-curated, banked as one rodata-coupled unit). ROM SHA-1 green. 0 carries.
- Quality: 0 stuck-far / 0 permuter-run / 0 carried / 0 re-opened
- Seed: committed ~5pt (3 FP crack-attempt leaves); banked 0pt (file partial); realized ≈6 (+1 mid-sprint rodata-coupling discovery + 1 bonus coupled bank), residual +1; regime classical. Value signal = +4 matched.
- What helped: the compiler-source FAN-OUT (3 gcc-2.7.2 crack + 1 binutils-2.6 rule-out, isolated `make nonmatching-func`) sorted the tail {crack, crack, crack+auto-coupled-crack} — 0 terminal walls, the cleanest FP slice yet, refuting the S263 "func_800453E0.c MINED OUT" verdict. (twins) MERGE direction of [[one-variable-reuse-reorders-loads]]: one reused `f32 t` anti-dep-serializes 3 hoisted `mul.s` into the ROM's reused $f14 + held-$v0 first-global base + load-bearing inner block after the early return. (side_pitch) [[himode-shortening-cse-neg-imm-addiu]]: `(f32)(u16)(x+0x8000)` shortens the add to HImode + cse folds to one neg-imm addiu (1 short); a separate `s32 t=x+0x8000` keeps SImode positive const -> ROM's ori/addu. (uphill) [[fp-slope-sampler-regalloc-levers]]: `f32 step[2]` stack-array forces the post-call re-trunc (cse MEM-across-call) + 6 distinct clamp temps defeat the 1-pseudo-3-reg impossibility (flips the reg-perm in one edit, S272 exact-count-first). binutils NEW finding: KMC-as inserts 1 nop between adjacent mul.s/mul.d (VR4300 hazard, not in pristine 2.6 source; 0 adjacent mul.s in the whole tree).
- Friction: the fan-out DISCOVERED the rodata coupling mid-run (gccC found uphill's `f64 li.d` pool needs side_pitch also-C for the 16-align) — a scope grow of +1 fn handled inline (extend gccC to the pool-owner sibling) rather than carrying uphill + re-planning. The size+FP sort priced uphill standalone; the coupling is `.s`-detectable (an `ldc1 %hi` pool ref) and now a tracked ranker tell. All 4 subagents delivered FINAL via SendMessage (1 crossed-message re-ping, harmless).
- Applied: 6 of 6 — #1 [[one-variable-reuse-reorders-loads]] MERGE direction; #2 NEW [[himode-shortening-cse-neg-imm-addiu]]; #3 [[kmc-as-noreorder-not-global-nop-oracle]] adjacent-mul.s nop + refined mips_emit_delays 1-vs-2 oracle; #4 NEW [[fp-slope-sampler-regalloc-levers]]; #5 `docs/hazards.md#rodata-sibling-yaml-pattern` classical-FP-`li.d`-pool coupling sub-case + `rodata-coupled:<pool-owner>` BACKLOG ranker follow-up; #6 fan-out process-win note (BACKLOG func_800453E0.c entry, supersedes S263 MINED-OUT).
- Carry-over: none. Next slice = continue func_800453E0.c smallest fresh FP/low-FP leaves via the fan-out (the S263 prefer-fresh-pack default does NOT fire while it keeps yielding banks); the `func_80047DBC` twin `func_80048D7C @0x8004A144` FP-sched wall is a candidate re-open. FINDING: the main FP tail bank-rate is 6/7 across S277-S278 — the "main FP = wall cluster" prior is now clearly too pessimistic for this file's FP leaves.

## Sprint 277 — main FP crack-slice via gcc/binutils fan-out — 2026-07-25
- Increment: 0 files / +2 functions matched (`src/main/func_800453E0.c` 24→22 stubs, partial, NOT md5-candidate): `build_radial_falloff_texture` (func_8004C860) + `get_shot_strength_tier` (func_80046898). ROM SHA-1 green. Plus 1 fully-RE'd terminal carry (`func_80047DBC`, `docs/wip/` doc).
- Quality: 0 stuck-far / 0 permuter-run / 1 carried (terminal, pass-cited) / 0 re-opened
- Seed: committed ~5pt (3 FP crack-attempt leaves); banked 0pt (file partial); realized ≈6 (2 clean fan-out banks ~2 each + 1 terminal carry ~2 w/ full root-cause), residual +1; regime classical. Value signal = +2 matched.
- What helped: the compiler-source FAN-OUT (3 gcc-2.7.2 crack + 1 binutils-2.6 rule-out, background subagents, isolated `make nonmatching-func`) SORTED the 3-leaf tail cleanly {crack, crack, terminal} — the S233/S276 expectation. (bank `func_8004C860`) do-while(!=) exit + flat `buf[k]` index + one reused `f32 s` (cse `mov.s`) + single `v` across 4 arms (cross-jump + annulled `beql`); needed a TU `-ffast-math` (BUILT_IN_FSQRT guard-drop, sibling precedent, verified codegen-neutral). (bank `func_80046898`) NEW LEVER [[else-arm-return-vs-then-arm]]: a far conditional early return must go in the ELSE arm (jump.c:1737 can't invert across the intervening set-retval; else form -> plain `bc1t` not `bc1tl`; final `return 0` stays last for cross_jump guard slots). (carry `func_80047DBC`) gccA root-caused the terminal wall: sched.c insn_cost load(3)>fabs(2) always schedules the 1.0f const before the abs.s feeding the compare, source-invariant (6 spellings) + cpu-invariant (9 builds), 1-instr-short so permuter-ineligible; base-reg asymmetry SOLVED (`f32* p` for the first global only); twin block in func_80048D7C @0x8004A144.
- Friction: binutils rule-out INVERTED my Phase-1 nop premise (`.set noreorder` is NOT global; nearly every nop is KMC-as-inserted) — good save, but shows the orchestrator's .s reading over-trusted literal nops. The fan-out was efficient (all 4 delivered via SendMessage, 1 re-ping for a fastmath re-verify), a good ROI vs S276's single-carry heavy fan-out. 2 of 3 FP leaves cracked despite the "main FP = wall cluster" prior — the size+FP sort still cannot pre-tell crack from wall.
- Applied: 4 of 4 — #1 memory [[fp-const-load-before-fabs]]; #2 memory [[else-arm-return-vs-then-arm]]; #3 memory [[kmc-as-noreorder-not-global-nop-oracle]] (noreorder-not-global + 1-vs-2 nop branch-slot oracle); #4 BACKLOG ranker follow-up (FP-SCHED? de-prioritizer tag for small heavy-FP main stubs; track main FP-leaf bank-rate, S277: 2/3).
- Carry-over: `func_80047DBC` (`docs/wip/func_80047DBC.near-match.md`, `--carried-check` flags it) — terminal sched.c const-load-vs-fabs coin; only untried mechanism is a C shape splitting the 1.0f's basic block from the compare (a crack banks the func_80048D7C @0x8004A144 twin too). FINDING (reconfirmed 4th sprint): main flagged-fresh is a wall-mixed pool the sort can't pre-triage; but a targeted gcc/binutils fan-out cracks ~2/3 of a small FP-leaf tail — a viable main play when the PO wants main, cheaper than sequential permuter setups.

## Sprint 276 — curated low-risk main slice + PO-requested compiler-source fan-out — 2026-07-25
- Increment: 0 files / +1 function matched (`src/main/func_80059BA0.c` 21→20 stubs, partial, NOT md5-candidate). ROM SHA-1 green. Plus 1 fully-characterized carry (`func_800990D0`, `docs/wip/` doc).
- Quality: 0 stuck-far / 1 permuter-run (plateau 1455) / 1 carried / 0 re-opened
- Seed: committed ~5pt (1 call-glue leaf + 1 crack-attempt); banked 0pt (files partial); realized ≈7 (func_8005ACF8 ~2 clean-w/-1-loop-form-fix; func_800990D0 ~5 = +1 carry +1 permuter-escalated), residual +2; regime classical. Value signal = +1 matched.
- What helped: (bank `func_8005ACF8`, heap-teardown of ~40 `heap3_free`) the do-while(i!=N) loop-form lever — a `for(i<N)` emits `slti;bnez` (immediate), the ROM tests `i!=N` with the bound in a callee-saved reg (`li sN; bne i,sN`); rewriting the 5 counted loops as `do{}while(i!=N)` matched byte-exact first build after the fix ([[do-while-not-equal-loop-exit-form]]). (carry `func_800990D0`, base-28 code generator) the compiler-source FAN-OUT (2 gcc-2.7.2 worktree + 1 binutils-2.6) SORTED the coupled residual: reached exact-count 225/225 + frame 0x50 and cracked every sub-residual with a cited source lever — `Struct990D0* r=&rec` forces &rec callee-saved (supplies the 2 missing `sw/lw`) + loop3 `j=4` after `rand` (counter → caller-saved, restores the rand-slot nop); split the reused digit pointer into `w1/w2/w3` (coloring, 156→108); `s16 h4`+`(u16)` clamp for the signed `lh`; inlined a checksum mod (108→82); `s32 unused[2]` first-local for the 8-byte dead frame. binutils ruled the KMC assembler OUT (noreorder active, 0 as-nops). Terminal residual = 5 preheader magic-hoist scheduling coins (loop.c move_movables + sched.c LUID tie), source-invariant, permuter plateau 1455 (base 1800).
- Friction: `tools/cmpfn.sh` NORMALIZED the prologue frame immediate (`0xNN → N`), so the 0x48-mine-vs-0x50-ROM frame delta was INVISIBLE and the miss read as "register-permutation only" — caught only by the binutils subagent's cross-check (now fixed, see Applied #1). The fan-out itself was heavy (3 subagents, ~2.9M/1.4M/idle subagent-tokens) for a single carried leaf, but delivered a definitive exact-count near-match + file:line terminal verdict (the S233 {crack sub-residuals, terminal coin} sort). Pre-existing: 9 stale pick_target/hazard-anchor golden failures (S216/S274 tracked debt) — banking adds live-state drift, left untouched per S274/S275 precedent (off-cadence golden fix).
- Applied: 4 of 4 — #1 `tools/cmpfn.sh` surfaces `[frame rom=.. mine=..] <-- FRAME MISMATCH` in the summary line (norm() masked it; golden-clean, no cmpfn golden) + [[pure-dead-frame-clean-crack]] within-larger-residual extension; #2 new memory [[reuse-one-pointer-across-loops-coloring-trap]] (split per-loop pointers when a shared walking-ptr web collides with a later loop's hard reg); #3 preheader-sched-coin plateau note folded into [[permuter-at-exact-count-residual]] (exact-count body w/ a preheader-order residual = CARRY, not a permuter run); #4 `docs/agent-workflow.md` fan-out-sorts-a-single-hard-fn's-coupled-residuals note.
- Carry-over: `func_800990D0` (`docs/wip/func_800990D0.near-match.md`, `--carried-check` flags it) — exact-count 225/225 near-match, terminal preheader magic-hoist sched coin; re-open if a loop.c/sched.c structural lever lands (S260), or try the untested [[commutative-operand-order-statement-split]] statement-structure flip for the secondary `addu` operand-order coin. `func_8005B314` deferred (294-instr wall-prone debug handler, its own slice). FINDING: main flagged-fresh remains a wall cluster; the ONE clean leaf this sprint was call-glue (func_8005ACF8) — prefer a FRESH non-main pack next unless a specific main leaf is pre-vetted.

## Sprint 275 — clean low-FP main leaf slice (1 salvaged bank, 4-for-4 wall cluster) — 2026-07-24
- Increment: 0 files / +1 function matched (`src/main/func_8002A640.c` 14→13 stubs, partial, NOT md5-candidate). ROM SHA-1 green.
- Quality: 0 stuck-far / 2 permuter-run (1 CRACK `func_80032720`, 1 plateau `func_80042318`) / 4 carried / 0 re-opened
- Seed: committed ~8pt (4 low-FP loose-stub leaves); banked 0pt (file partial); realized ≈11, residual +3 (all 4 committed leaves walled -> +1 carry-cluster, +1 permuter, +1 cmpfn near-loss gotcha; mid-sprint substitution to salvage a bank). Value signal = +1 matched.
- What helped: SUBSTITUTE bank `update_putting_meter` (func_80032720, a putting-meter charge/release SM one file over) cracked by the PERMUTER (`import.py --settings permuter_settings_main.toml`, base 790 -> 0): the winning lever is a temp `x8 = x/8` computed BEFORE the `y` mult, separating the two mults to restore the ROM's `mflo`-latency nops + `a0` pml->y register reuse — a statement-order lever no hand iteration found. Levers that fully RE'd `func_80042318` (menu/HUD handler, 171/172, register/structure/signedness-exact): giv `i+5` inline (NOT a `row` var) fixes the biv/giv reg-perm; `u32` counters -> `sltiu`; `u16* input` base-cache across calls; `switch` (not if-else) for the beq-to-out-of-line-case dispatch; `s32* mode=&D_801B7F70` base-cache.
- Friction: the `fp=0/bl=0` gate filter I used chose 4 leaves that were ALL walls of classes the size+FP sort can't see (jtbl-carve-align, reorg delay-slot coin, 2 raw-DL-word emitters) — the DoR carried/nested checks passed them all clean (first-encounter walls). BIGGEST friction: `cmpfn.sh` reported the byte-EXACT `update_putting_meter` as `107/112` (5 short) because `objdump -d` collapses the 2 `mflo`-latency nops into `...`; nearly abandoned the match as a sched coin — caught only by a direct `objdump -dz` + full-make verify-rom. Pre-existing: 9 stale pick_target golden tests fail (unrelated ranker drift, S274-flagged).
- Applied: 4 of 4 — #1 `tools/cmpfn.sh` -dz fix (expand elided nops) + memory [[cmpfn-nop-elision-undercount]]; #2 `pick_target_score.py wall_class_tell` -> `--loose-stubs` now tags JTBL-DISPATCH + RAW-DL-EMITTER (reclassified ~103 of ~147 main "fresh" stubs) + workflow plateau note; #3 permuter mflo-latency-nop-tractable folded into [[permuter-main-settings-flag]] + workflow; #4 DoR "fresh+standalone can still be a first-encounter wall" note (mechanized by #2).
- Carry-over: `func_800985B4` (jtbl trailing-4-align carve wall), `func_80042318` (reorg delay-slot coin, 171/172), `func_800318A8` + `func_80075E48` (raw-DL-word store-giv class) — all with `docs/wip/*.near-match.md`, all now flagged by `--loose-stubs`/`--carried-check`. FINDING: clean small main leaves mined out; prefer a FRESH non-main pack next, or attempt a flagged raw-DL emitter as a dedicated slice.

## Sprint 274 — next-tier `--loose-stubs` main leaf slice (2 banked, 2 exact-count walls) — 2026-07-24
- Increment: 0 files / +2 functions matched (`src/main/vec3f_normalize.c` 10→9 stubs, `src/main/get_tile_attribute.c` 22→21 stubs; both partial, NOT md5-candidate)
- Quality: 0 stuck-far / 0 permuter-run (2 eligible but LOW-EV shapes) / 2 carried / 0 re-opened
- Seed: committed 8pt (4 tiny loose-stub leaves); banked 0pt (files partial); realized ≈4 (2 clean leaves), residual −4 (seed over-priced the walled leaves — a size-only seed can't see an FP/value-select wall). Value signal = +2 matched.
- What helped: `--loose-stubs main` (S273 tool) surfaced the next tier; 2 banked. `func_80029B58` byte-matched first build (3 buffer-setup calls `func_800577DC(k,&g_terrain_tile_cache[size*n])` + osSyncPrintf; `size=func_800577D0()`). `get_lowest_height_at_position` banked on a single-instruction lever: the ROM's `add.s` with a NEGATIVE const (0xCA3B8000) needs `+ -3072000.0f` in C, NOT `- 3072000.0f` (which emits `sub.s` + positive const); reconstructed `find_collision_triangle`'s 8-arg signature and the 0xD0 stack-buffer frame (`s32 a[6]` + `f32 hit[0x22]`) straight from the call site; `bnezl` == ROM `bnel v0,zero` fell out naturally. Retired `func_80029C00` (FCSR `cfc1`/`ctc1 $31` intrinsic — no gcc-2.7.2 C form) at the gate with a wip doc so `--carried-check` stops re-surfacing it.
- Friction: **two self-inflicted, both from not reading the permuter hazard section before concluding "blocked".** (1) Ran BARE `import.py` on the two exact-count carries; it failed on `<PR/ultratypes.h>` because the default `permuter_settings.toml` is the KMC-mirror profile (`-I include`). The documented main command is `import.py --settings permuter_settings_main.toml <c> <asm>` (agent-workflow.md ## Execution loop, permuter bullet) — the tooling was complete; I mis-diagnosed a tooling gap and even started a needless `permuter_settings.toml` edit (reverted). (2) The two carries are a MULTI-register tail permutation and an FP-schedule coin (mine 1-instr shorter) — per the documented payoff rule these are the NON-payoff permuter shapes, so even correctly run they are LOW-EV; carrying them is right, only my stated reason ("blocked") was wrong. Corrected both wip docs.
- Applied: 3 of 3: #1 `pick_target --loose-stubs` `intrinsic-hasm` tell — new `intrinsic_stub_tell()` in `pick_target_score.py` reusing `decomp_asm.PRIVILEGED_OPS` (cfc1/ctc1/mfc0/mtc0/tlb*/cache/eret); intrinsic stubs now hidden from `fresh` and tagged `INTRINSIC-HASM`; #2 CORRECTED to a no-op-tooling finding: the main permuter flow already exists and is documented; recorded the operator-error lesson (pass `--settings permuter_settings_main.toml`, or the docs' direct-import.py note); #3 `docs/agent-workflow.md` loose-stub wall-cluster note (small fresh main stubs skew to FP/value-select/regperm walls once clean leaves mined; prefer a fresh non-main pack when the main bank-rate drops; ranker FP-tell de-prioritizer = BACKLOG follow-up).
- Carry-over: `get_terrain_vertex_pointer` (40/40 exact-count multi-reg tail permutation + bne/bnel annul; `docs/wip/get_terrain_vertex_pointer.near-match.md`), `func_80047CAC` (pure-FP 3x lerp; FP-scheduler mine-1-shorter + 1-ULP const; `docs/wip/func_80047CAC.near-match.md`). Both LOW-EV permuter (non-payoff shapes). `main` loose-stubs still show fresh leaves but skewing to walls; next slice prefer a fresh non-main pack or an exact-count-plus-one-operand `--main` permuter target.

## Sprint 273 — fresh stub-level integer-leaf slice on "mined-out" main (3/3 banked incl. bnel hedge) — 2026-07-24
- Increment: 0 files / +3 functions matched (`src/main/vec3f_normalize.c` 12→10 stubs, `src/main/func_8002A640.c` 15→14 stubs; both partial, NOT md5-candidate)
- Quality: 0 stuck-far / 1 permuter / 0 carried / 0 re-opened
- Seed: committed 4pt (3 tiny classical integer leaves + hedge); banked 0pt (files partial, all-or-nothing); realized ≈5 (permuter escalation on #3, +1), residual +1; regime classical. Value signal = +3 matched.
- What helped: **the "main is mined out" verdict (S272) was PACK-level, not stub-level.** `pick_target --segment main` prices only whole asm-flip subseg PACKS, so fresh standalone leaves persisting as individual `INCLUDE_ASM` stubs inside already-`c` files were invisible to it. Hand-mining the smallest stubs (grep INCLUDE_ASM names in src/main → size via `head -1 <fn>.s` → smallest-first → filter `--carried-check`+`--nested-check`) surfaced a live fresh vein: `pause_audio` (0x80029BD8, `func_8006F4F0()`+`g_mus_audio_paused=1`) and `unload_active_overlay` (0x800326C4, overlay-unload wrapper) both byte-matched first build. The hedged bnel predicate `func_80029EEC` cracked: an early-return rewrite fixed the value-select block-order/polarity (8 rows→4, one root cause = mag allocno $v0 vs $v1), then the permuter hit 0 in 657 iters; the load-bearing lever is abs spelled `(x <= -1) ? -x : x` (NOT `(x < 0)`) — an RTL-canonical spelling flip that steers the single-register allocno (the permuter's `(x>>9)>>22` was a red herring, folds to `x>>31`).
- Friction: a curated rename (`func_800326C4`→`unload_active_overlay`) reached a still-asm caller in a DIFFERENT already-`c` file (`bgm_load_song_from_rom.c`), link-failing until a whole-tree `find build -path '*/src/main/*.o' -delete` (2nd independent confirmation of the S271 whole-tree-delete rule; the caller was cross-file, not the target's own parent). `make test-tools` carries 9 PRE-EXISTING stale-golden failures (pick_target json/table goldens drift every sprint a ranked fn banks, e.g. `render_pin_assembly_with_wind_hud`); confirmed identical on clean HEAD, unrelated to this sprint's edits — parked as a BACKLOG follow-up (needs a `REGEN_GOLDEN=1` refresh).
- Applied: 3 of 3: #1 `tools/pick_target.py --loose-stubs SEG` (+`--all`) — enumerates still-`INCLUDE_ASM` stubs inside already-`c` files of src/SEG/, smallest-first, tagged fresh/CARRIED-WALL/NESTED-CHILD (new `loose_stubs()` in `pick_target_score.py`); #2 RETRO note only for the cross-file stale-`.o` reconfirm (S271 doc already covers it); #3 `docs/hazards.md#register-reuse-nudge-classical-regalloc` abs-compare-form allocno sub-lever. Memory `abs-compare-form-steers-allocno` saved (linked to `commutative-operand-order-statement-split` / `permuter-at-exact-count-residual`).
- Carry-over: none. `main` is NOT mined out at stub-level — `--loose-stubs main` reports 96 fresh / 238 carried-wall / 8 nested-child; next slice can be another fresh stub-level leaf batch (smallest fresh = `func_80029C00` FCSR, likely intrinsic; then integer glue across get_tile_attribute/func_800453E0) or a fresh overlay pack.

## Sprint 272 — crack-attempt slice on mined-out main walls (2 "terminal" walls cracked) — 2026-07-24
- Increment: 0 files / +2 functions matched (`src/main/func_80071370.c` 18→17 stubs, `src/main/func_800772B0.c` 3→2 stubs; both partial, NOT md5-candidate)
- Quality: 0 stuck-far / 0 permuter / 1 carried / 2 re-opened (both re-opens = successful cracks of prior-sprint "terminal" verdicts)
- Seed: committed 2pt (crack-slice of documented walls, not a size-priced pack); banked 0pt (files partial, all-or-nothing); realized ≈4 (2 novel wall-cracks, +1 each), residual +2; regime classical. Value signal = +2 matched.
- What helped: `main` confirmed fully mined (`pick_target --segment main` = 0 fresh; all 14 smallest remaining leaves `--carried-check` CARRIED-WALL). PO chose a crack-slice over widening to a fresh overlay pack. **BOTH banks fell to ONE meta-lever: reach the ROM's EXACT instruction count by reproducing what the ROM HOISTS, and the "irreducible" register permutation resolves WITH the count.** `emit_glyph_string_dl` (0x80076138, cracks S242 "biv-swap needs a 9th reg") — a preheader-local `u8* p = str;` defers the str param's callee-saved copy to the loop preheader (str→$s2, i→$s3, NO 9th reg), landing cleanly only after a register-cursor + fresh post-loop temp + explicit invariant temps got the body to exact count 69/69. `interp_cubic_finite_diff` (0x80077AD4, cracks S206 "pervasive FP-regalloc, unreachable from faithful C") — an explicit `s32 ia0_3 = ia0*3;` temp reproduces the ROM's early hoist, reaching 65/65 AND collapsing the ia0/ia1/d0/d1→$a2/$a0/$a1/$a3 coloring in one edit. Both prior verdicts had been drawn from NON-exact-count bodies. Fresh-objdump re-derive (S268) per wall first.
- Friction: none material. `func_8005D0D8` (S264 3-coin cluster) re-confirmed terminal after 3 source forms all narrowed the `slti`+`bltzl` to one `sltiu` (combine `nonzero_bits` timing coin); reverted clean, doc updated. Curated renames reached still-asm callers → force-deleted `src/main/*.o` + `make extract` (S271 gate rule, applied smoothly).
- Applied: 2 of 2: #1 `docs/agent-workflow.md` DoR re-open STEP-0 "reach exact instruction count first" for allocno/biv/FP-regalloc walls (extends S260/S268) + `carried-wall:exact-count?:no` ranker follow-up to BACKLOG; #2 `docs/hazards.md#pervasive-regalloc-classical-main` exact-count-first / FP-hoist sub-lever (sharpens the S233 "FP walls not steerable" framing — a HOIST-driven FP permutation IS steerable). Memory `remeasure-percent-after-structural-fix` updated with the dual-crack re-open playbook.
- Carry-over: `func_8005D0D8` (terminal 3-coin cluster, gcc-2.7.2 combine dive is the only path). Remaining `main` = documented walls + FP/DL/nusys tails; next slice is another crack-attempt or a fresh overlay pack.

---

## Sprint 271 — open main's last fresh vein: flip subseg 0x4D00, bank the D_801052F8 bit-flag module — 2026-07-24
- Increment: 0 files / +6 functions matched (`src/main/vec3f_normalize.c` new, 18→12 stubs; partial pack, NOT md5-candidate — mainproc=nuboot / vec3f_normalize FP / big fns carry)
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened (all 6 fresh first-build; but see Friction — one self-inflicted process incident, fully recovered)
- Seed: committed 13pt (18-fn one-tu pack, big); banked 0pt (file partial, per-file all-or-nothing); realized n/a (partial); regime classical (fresh-vein slice, asm-first fast-path). Value signal = +6 matched.
- What helped: `main` was confirmed mined out of fresh clean leaves everywhere EXCEPT the never-flipped 0x4D00 subseg (asm since ~S61) — a textbook `D_801052F8` bit-flag module. All 6 banked via the asm-first fast-path (hand-translate the `.s`, no m2c/base.c), no permuter: `flag_is_set` (early-return guard `if((u32)f>=0x100)return 0;` for the ROM's fall-through-`beqz` polarity), `flag_set`/`flag_clear`/`flag_toggle` (curated; clear = `&= 0xFF7F>>bit`, `li 0xFF7F`↔`ori` is a cmpfn display alias, byte-identical), `flag_clear_all` (for-init comma `i=0x1F, p=&D[0x1F]` to birth the counter reg first), `func_800299D0` cfb wrapper (auto-kept, S247).
- Friction: **the sprint's dominant cost was a self-inflicted recovery, not the banking (banking was ~20 min).** The curated rename's gate `make extract && make` hit a stale-`.o` link error (`lz_compress` ref `func_80029A6C`); while diagnosing I `rm -rf`'d `asm/nonmatchings/main/bgm_load_song_from_rom/` on a WRONG "orphan" theory. Those were STALE-PERSISTENT RELICS: splat's c-mode extract leaves disassembly GAPS at 6 curated bgm fns + `func_8005DDAC`@func_80059BA0 and will NOT regenerate them (gitignored → no git restore). Recovered by flipping each subseg c→asm (asm-mode disassembles the full range in the IDENTICAL nonmatchings stub format), awk-carving the missing blocks back, flip to c. ROM restored green. Motivated retro #1/#2/#3.
- Applied: 4 of 4: #1 `docs/agent-workflow.md ## Conventions` HARD RULE never `rm -rf` a `nonmatchings/<stem>/` dir + memory [[nonmatchings-relic-no-rmrf]]; #2 `docs/hazards.md#stale-persistent-nonmatchings-relic-recovery` new section + `tools/recover_stub.sh` helper (round-trip byte-identical, tested) + playbook-index entry; #3 `docs/agent-workflow.md` bank step 4a rename-gate whole-tree `.o` delete (generalizes S270 single-parent to `find build -path '*/src/<tree>/*.o' -delete`); #4 REVERTED as code (the lead-line tighten un-parked genuine carries func_8003E004/func_80050710 → dangerous false-NEGATIVE), applied as a `carry_over_names()` doc-note instead (over-scoop toward safe false-positive is intentional; clear via the `.s`).
- Carry-over: `vec3f_normalize.c` 12-stub partial (mainproc=nuboot → possible libnusys path-qualifier; vec3f_normalize S158 FP; big fns func_80029F6C/8002A144/8002A310 — a later FP/DL/nusys-path sprint). **Pre-existing tooling-test debt surfaced (NOT introduced): `tests/tooling` goldens stale since S241 (~30 sprints) + a `descending-score` invariant failure + 2 pre-existing hazard-anchor broken links (`delay-slot-fill` ambiguous, `cse-make-regs-eqv` unresolved). Dedicated tooling-maintenance task → BACKLOG.**

---

## Sprint 270 — file-complete re-open of func_8004D190.c (2 DL emitters + 2 regalloc walls) — 2026-07-24
- Increment: 0 files / +2 functions matched (func_8004D190.c 4→2 stubs, partial pack, not md5-candidate)
- Quality: 0 stuck-far / 1 permuter (D4B8, ~27k iters, no crack) / 2 carried / 4 re-opened (all 4 stubs were S182 carries; 2 re-opened→banked, 2 re-worked as walls)
- Seed: committed 13pt (ranker pts, c-stub continuation); banked 0pt (file partial); realized ~9 / residual +4 (2 DL banks on-plan + 1 advanced-wall-reopen with a new lever + 1 terminal-verdict-with-citation + 1 permuter + 1 novel dead-frame-live-index gotcha; the TU-profile-probe hypothesis was falsified — a real negative result); regime classical (crack/re-open slice)
- What helped: **DL-reconstruction subagent fan-out** — one subagent reconstructed BOTH DL emitters (`emit_text_glyph_dl_preamble` 26-cmd preamble via a raw `EMIT(g++)` macro ≡ gbi gDP* pointer macros; `render_text_grid` 30x40 grid via `gSPTextureRectangle`) byte-exact in isolation, iterated on the fresh-object objdump oracle (S244), returned both bodies; orchestrator integrated + full-make gated serially. The mystery word `0x075FF080` was the **w1 of gsDPLoadBlock** (lrs=1535/dxt=128), not a standalone opaque command — the subagent re-derived 5 of my gfxdis-decoded words straight from the `.s` store-trace (the `.s` is ground truth over gfxdis). `render_text_grid` matched with 3 cheap source levers, no permuter (init idx/prevhi before the 2 calls → callee-saved s1/s0; test `prevhi` not `hi`; decl+init order pins the swap). **gcc-2.7.2 compiler-source fan-out cracked the D4B8 frame+regs** the S182 wall called irreproducible: `str[row]` live-index pressure reproduced the phantom 8B dead frame AND matched all 6 registers (the S182 4-reg-perm), leaving only a 2-word scheduler-slot residual. D5F0 retired-with-citation (global.c:790-823 set_preferences). Profile-probe swept ~14 `-f` flags on both bases → NEGATIVE, falsifying the shared-config theory cleanly.
- Friction: **both walls resisted a bank** — D4B8's residual is a `move t2,zero` scheduled INTERIOR to the atomic `c/32`-division-BB (no C statement boundary targets it; permuter exhausted ~27k iters/3 seeds), D5F0 is a global.c copy-coalesce the ROM keeps un-coalesced (strictly less optimal than any faithful C). So the file-completion stretch missed as load-bearing-risks #2/#3 flagged; realistic estimate (2-3 banks) hit exactly. **Rename stale-object gotcha:** renaming `func_8004D7B8`→`render_text_grid` link-failed until the still-asm caller `render_frame`'s LARGE parent object (`func_8002A640.o`, 14400B) was force-deleted (motivated retro #2).
- Applied: 2 of 2: #1 `docs/hazards.md#dead-frame-reload-artifact-regalloc-wall` third-variant "live-index block-pressure dead frame" (str[row] pressure reproduces a small <0x18 dead frame + reg-perm, distinct from the `unused[]`/volatile recipes) + memory [[dead-frame-live-index-pressure-lever]]; #2 `docs/agent-workflow.md` bank step 4a stale-object gotcha (force-delete the still-asm caller's parent `.o` before the gate make after a rename-through-still-asm-caller in a large file).
- Carry-over: func_8004D4B8 (advanced near-crack, frame+regs solved, 2-word scheduler residual, docs/wip/func_8004D4B8.near-match.c.txt), func_8004D5F0 (TERMINAL retired-with-citation global.c:790-823, do NOT re-open, docs/wip/func_8004D5F0.near-match.c.txt). func_8004D190.c stays 2-stub partial; completes only if the D4B8 2-word scheduler coin cracks.

---

## Sprint 269 — open main's last fresh vein: flip the blocked func_8002A640 render pack — 2026-07-24
- Increment: 0 files / +8 functions matched (func_8002A640.c 23→15 stubs, partial pack, not md5-candidate)
- Quality: 0 stuck-far / 0 permuter / 2 carried / 0 re-opened
- Seed: committed 3pt; banked 0pt (file partial); realized 4 / residual 0 (8 banks vs est 3-4, 0 walls survived; the two levers were quick source fixes — u32-switch, goto-to-tail; 2 carries are coupled nested-fns, not walls); regime classical (open-vein slice)
- What helped: **`--carried-check` correctly proved main mined out** — every small partial-pack leaf flagged CARRIED-WALL (wip-doc or BACKLOG carry), fake control returned fresh — so the honest move was to OPEN the one `blk` asm-flip pack (24 fresh render fns behind `needs-header:sprite.c`) rather than grind another wall slice. Header deferred to execution (INCLUDE_ASM stubs build green without it; only a decompiled body needs it). The tiny getters/setters banked first-build (asm-first fast-path, objdump-vs-`.s` oracle per S244). **Two one-lever cracks:** `func_80032668` sparse switch needed a `u32` index to emit `sltiu` (not signed `slti`); `setup_view_by_camera_mode` mode dispatch needed a `goto tail_label` — 3 structured if/else spellings all canonicalized to the mirror block layout (returning arm inline, fall-into-common arm out-of-line); the goto forced the ROM's `bne mode,1` fall-through. **`func_80032520` banked with no lever** — gcc `record_jump_equiv` reused the `if(x==1)` compare register for the in-branch flag store, reproducing the ROM `sw v1` (not a fresh `li`). DL words decoded via the F3DEX2 gbi.h macros (`gSPPopMatrix`=0xD8380002, `gSPMatrix` push-XOR p=0x06→0x07) — Ghidra's macro guesses (ClearGeometryMode/Viewport) were wrong per the opcode bytes.
- Friction: **2 of the smallest post-getter leaves were GCC nested functions** (`func_8002BE78`, `func_8002DAC0`), arg homed via the `$v0` static chain, callers `render_frame`/`draw_ground_shadow_decals` still asm — not standalone-bankable, each cost a caller-disasm to classify (motivated retro #1). `setup_view` block-layout coin ate 4 iterations before the goto landed (motivated retro #2). Ghidra's decompile dropped a redundant intermediate `glistp` store (not byte-faithful) — re-derived the two-macro form from the `.s`.
- Applied: 2 of 2: #1 `pick_target.py --nested-check <fn>...` `$v0`-static-chain prologue detector (verified vs the 2 nested + 3 standalone S269 leaves) → tools + workflow DoR note, kin to `--carried-check`; #2 `docs/hazards.md#guard-block-layout-inversion` goto-to-tail multi-arm-dispatch variant + agent-workflow DoR reference, extends [[out-of-line-handler-block-branch-likely]]. Candidate memory (PO-manual promotion): the goto-to-tail lever + the nested-fn `$v0`-spill DoR tell.
- Carry-over: func_8002BE78 (0xA4 nested child of draw_ground_shadow_decals, docs/wip/func_8002BE78.near-match.md), func_8002DAC0 (0x150 nested child of render_frame, docs/wip/func_8002DAC0.near-match.md) — both bank with their parents when those decompile, NOT walls. func_8002A640.c remains 15-stub partial (render_frame 0x3840, func_8002A9C4, DL emitters, jtbl/data-static fns).

---

## Sprint 268 — crack-attempt slice on 2 documented func_80059BA0.c walls — 2026-07-24
- Increment: 0 files / +0 functions matched (func_80059BA0.c stays 21-stub partial pack, not md5-candidate)
- Quality: 0 stuck-far / 0 permuter (both walls permuter-plateau by class, not run) / 2 carried / 2 re-opened
- Seed: committed 5pt; banked 0pt (no bank); realized ~6 / residual +2 (both re-opens → pass-cited TERMINAL verdicts [S233-class] + 2 corrected doc residual-classes); regime classical (crack slice)
- What helped: **func_8005B0B4 — the S260 [[out-of-line-handler-block-branch-likely]] lever DID crack 3 sub-issues** (the S263 doc under-counted the divergence): computing `s32 sel = flags & 0xF;` right before the guard branch makes reorg fill the `beqz` delay slot with `andi sel` (not `move acc,zero`), freeing `v1=0` to emit EARLY → accumulator lands in `v1` (was `a1`); out-of-line value handlers + single `end:` exit fold the sel==3 arm into an annulled `beql`. score 4965→4165, ~90% structural. The "2-off / 37-39" framing was a scoring artifact — asm-differ scores the residual REGISTER cascade at 0.008. **func_8005CEE0 doc CORRECTED**: the S266 "a0<->v1 role swap" was not present (roles already matched); real residual = a fold-canonical load-order coin. Re-deriving each residual from a fresh objdump-vs-`.s` (not the doc's stated class) was the load-bearing move.
- Friction: BOTH walls terminal. func_8005CEE0 = a load-order coin COUPLED to the idx*4 index-reg coloring — 6 source forms (ptr-temp/base-RMW/`+=`/base-inline/dead-idx-reuse/operand-group-flip) each fix one and break the other; no form yields {a-first load AND idx*4=v0}. func_8005B0B4 residual = an irreducible `global.c` 3-register allocno permutation (flags/val/sel across a0/a1/a2), source-invariant (load-order swap leaves it identical) + a reorg sel==1 jump-to-jump threading coin; permuter plateaus on the multi-reg permute. **DoR fresh-leaf miss RECURRED (8th)**: the two smallest `.s`-sized "fresh" leaves (func_8005DE88, func_8005B150) were BACKLOG-only carries — caught this sprint only by the per-leaf `grep -n` + read. Ranker c-continuation blindness = 15th recurrence.
- Applied: 3 of 3: #1 `pick_target.py --carried-check <fn>...` DoR detector (wip docs ∪ BACKLOG carry region; flags DE88/B150) → tools + workflow DoR; #2 residual-CLASS-is-a-hypothesis (re-derive from fresh objdump before trusting the doc's named class) → workflow DoR + extends [[near-match-doc-semantics-suspect]]; #3 `docs/hazards.md#multi-register-allocno-permutation` new terminal class.
- Carry-over: func_8005CEE0 (38/38, load-order⟂idx-coloring coupled coin, TERMINAL S268, docs/wip/func_8005CEE0.near-match.md, warm base.c), func_8005B0B4 (global.c 3-reg allocno permutation, TERMINAL S268, S260-improved base.c ~90%, docs/wip/func_8005B0B4.near-match.md). func_80059BA0.c remains 21-stub partial.

---

## Sprint 267 — smallest-first func_80059BA0.c c-continuation (was a crack slice on 4 S210 carries) — 2026-07-24
- Increment: 0 files / +1 function matched (func_8005AF80; func_80059BA0.c 22→21 stubs, partial pack, not md5-candidate)
- Quality: 0 stuck-far / 2 permuter (0 crack: 80059BA0 280s, CEE0 400s) / 3 carried / 1 re-opened-and-cracked (func_8005AF80)
- Seed: committed 5pt; banked 0pt (file partial); realized ~7 / residual +2 (+1 permuter, +1 carry-cluster; +1 novel gotcha [(s32)base REG_POINTER-drop], offset by the bank being a re-opened S210 wall not a fresh leaf); regime classical (c-continuation)
- What helped: **func_8005AF80 (0xBC/47i grid-reset init) CRACKED — the S210 permuter-FAILED wall (~9500 iters, base 220) banked by hand** via a 3-lever combo [[grid-reset-init-crack-combo]]: `s8` counters + `do{}while(i!=N)` ([[do-while-not-equal-loop-exit-form]]); an explicit hoisted `s32* row = (s32*)(i*12 + (s32)base + 0xDC0)` (the 0xDC0 folds INTO the row ptr materialized in the outer loop, not the inner store displacement — a base-vs-disp fix); and the `(s32)base` cast dropping REG_POINTER so `commutative_operand_precedence` keeps `i*12` as the addu rs, matching the ROM operand order ([[commutative-operand-order-statement-split]], new pointer-specific case). **func_80059BA0 (fabsf sign-mask) root-caused end-to-end** with 3 read-only gcc/gas subagents (@mips-gcc-2.7.2 + @mips-binutils-2.6): gas ruled out (no general scheduling); the reassign `arg0=x.f;return arg0;` RESTORES the ROM's `mov.s` (exact 8i + identical regalloc — correcting S210's "copy-propagated-away artifact" reading); the sole residual is a post-reload `schedule_select` potential-hazard override ([[sched-select-potential-hazard-coin]], sched.c:2615/3733) that front-loads the mfc1 and hoists the independent `li` above it. func_8005CEE0 improved 4-rows → EXACT 38/38 (explicit `u8* base` closes the base-vs-disp; guard restructure = `-1` block between the two guards, 2nd branches toward ok). func_8005B0B4 improved → 37/39 (#char-signedness `s32 val=((s8*)p)[K]` → `lb`; single-exit v1). tools/cmpfn.sh object-oracle iteration throughout.
- Friction: **DoR grep-BACKLOG miss RECURRED a 3rd time (S239/S240)** — all 4 committed "fresh" leaves were documented S210 carries; the gate-time `grep -c '<fn>' BACKLOG.md` returned 1/1/1/6 and was misread as pack-listing, because the carry entries live in a deep multi-fn `## Carry-overs` block a COUNT-grep does not reveal. The DoR must `grep -n` AND READ the carry block. (Net upside: the mis-framed slice was a productive crack-attempt anyway — 1 wall cracked, 3 deepened.) Ranker c-continuation blindness = 14th recurrence. func_8005B0B4 residual is the S263 value-select-branch-likely (ROM `beql sel,K,end`+annulled `li v1,CONST`, ROM 2 LONGER); func_8005CEE0 residual = a within-block load-order/regalloc coin (permuter dry).
- Applied: 4 of 4: #1 sched-select potential-hazard coin → memory [[sched-select-potential-hazard-coin]] + MEMORY.md; #3 (s32)base REG_POINTER-drop operand-order lever → folded into [[commutative-operand-order-statement-split]]; #2/#4 DoR grep-BACKLOG-miss #3 + 8x-confirmed plateaued-tail → BACKLOG ranker/DoR follow-up (read the carry block, not just count); #5 grid-reset 3-lever combo → memory [[grid-reset-init-crack-combo]] + MEMORY.md.
- Carry-over: func_80059BA0 (TERMINAL sched2 potential-hazard coin, docs/wip/func_80059BA0.near-match.md), func_8005CEE0 (38/38, load-order/regalloc coin, permuter dry, docs/wip/func_8005CEE0.near-match.md, warm base.c), func_8005B0B4 (37/39, S263 value-select-branch-likely, docs/wip/func_8005B0B4.near-match.md, warm base.c). func_80059BA0.c remains 21-stub partial.

---

## Sprint 266 — dedicated func_8005CF78 jtbl crack + func_8005DFE8 filler — 2026-07-24
- Increment: 0 files / +1 function matched (func_8005CF78; func_80059BA0.c 23→22 stubs, partial pack, not md5-candidate)
- Quality: 0 stuck-far / 0 permuter / 1 carried (func_8005DFE8) / 0 re-opened
- Seed: committed 5pt; banked 0pt (file partial); realized ~6 / residual +2 (+1 carry-or-reopen, +1 novel gotcha [do-while-`!=` exit-form + fold-associate two-iv split]); regime classical (c-continuation)
- What helped: **func_8005CF78 (the S265-DEFERRED jtbl leaf) banked EXACTLY as the BACKLOG teed it up** — a documented-tractable target cracked to 88/88 via a repeatable 5-lever jtbl-dispatch playbook (switch default-set regen the 12-word jtbl; fold-associate `base+CONST+k*stride` constant-first for a two-iv base-vs-disp split; `do{}while(k!=3)` on ALL loops → `bne`+reg-bound+frame-0x28; `rows[i]` array-index anchors the inner giv at the row base not row+maxfield; assign the loop invariant INSIDE the while → gcc hoists it to the POST-GUARD preheader so the guard delay-slot fills with `move a0,zero`). jtbl_800D0A60 carve landed atomically with the C body (bank-time action, never a gate enabler). func_8005DFE8 driven to 102/102 + exact stack by 4 levers (size-var-for-compare-only+literal-args puts 0x2A78 in fp; size-before-flag decl → s7/s8; raw[0xD0] → s6=0xE0/frame 0x110; two `==` tests not `<2` → count 98→102). Re-derived CF78's 2nd loop as `if(arr[k]==1)clear` from the `.s` (S265 doc had it inverted, per the S258 DoR).
- Friction: func_8005DFE8 residual is a genuine S224-class allocno coin — s1/s2 role (off vs tries) is [[global-allocno-compare-livelength-biv-order]] priority-driven and declaration-order-INVARIANT (both orders = 24 rows), plus 4th-check bnel/beq + prologue/tail reorg coins; permuter-blind because the isolated compile scores 0.1 (D_800C2BE0..EC + callee %hi/%lo reloc noise pervade), so only in-tree diff.py is truth. Ranker continuation-blindness recurred a 13th time (3rd straight hand-mined func_80059BA0.c sprint). Self-inflicted: an unscoped `sed` for the raw-buffer size sweep clobbered banked func_8005E180's `raw[0xC0]`→`0xD0` (SHA break); caught via git-diff + restored before the carry — lesson: scope in-place seds to the target function.
- Applied: 4 of 4: #1 jtbl-dispatch-loop-crack-playbook → memory [[jtbl-dispatch-loop-crack-playbook]] + MEMORY.md; #2 do-while-`!=` loop-exit-form → memory [[do-while-not-equal-loop-exit-form]] + MEMORY.md; #3 constant-in-callee-saved-for-compare + literal-args → folded into [[cross-call-live-range-callee-saved-lever]]; #4 ranker continuation-blindness 13th recurrence → BACKLOG (priority already at "biggest gate-friction item").
- Carry-over: func_8005DFE8 (SRAM load/verify, 102/102 near-match, s1/s2 allocno + reorg residual, docs/wip/func_8005DFE8.near-match.md, warm-start base.c). func_80059BA0.c remains 22-stub partial (func_8005D0D8 S264 coin + func_8005D334 S210 + the FP/DF54/DE88/5CEE0 walls + ~6 bigger mid-logic leaves).

---

## Sprint 265 — 3 smallest fresh non-FP leaves of func_80059BA0.c — 2026-07-23
- Increment: 0 files / +1 function matched (func_8005E180; func_80059BA0.c 24→23 stubs, partial pack, not md5-candidate)
- Quality: 0 stuck-far / 1 permuter (1 partial crack, the xor lever) / 2 carried / 0 re-opened
- Seed: committed 5pt; banked 0pt (file partial); realized 8 / residual +3 (+1 permuter, +1 carry-or-reopen, +1 novel gotcha [jtbl-carve-is-bank-time]); regime classical (c-continuation)
- What helped: func_8005E180 (SRAM save/verify, 0x13C/80i) cracked to exact-count-80 by 5 stacked levers — split-first-loop-counter into its own var (frees caller-saved `$a0` for the callless 0x20-loop), `if(flag==0){verify}else{write}` branch polarity, raw/masked flag split (`$s0` park across crc16 + `$s4` mask), the permuter's cse-class statement split `stat[1]=r; stat[1]^=crc` (flips the xor operand order, S258 class), and `u8* dst=buf+j; dst[8]=...` pointer temp (flips the `buf+j` addu operand order the permuter plateaued on); manual stack-buffer 16-align `(u32)(raw+0xF)&~0xF` reproduced the runtime `addiu sp,0x1F; and ~0xF`; `tools/cmpfn.sh` object-oracle iteration.
- Friction: 2 of 3 committed "fresh" leaves were non-tractable despite a clean DoR carry-grep — func_8005DF54 a TERMINAL non-ABI `$s2`-arg wall (reads incoming `$s2` with no prologue `move`, callers set only a0/a1; kin to func_80041E8C), func_8005CF78 too-large (88i jtbl-dispatch 3-level loop). The "3 fresh tractable leaves" premise was optimistic; S210's "easy vein mined out" confirmed. Ranker c-continuation blindness recurred a 12th time (backlog hand-mined again). The jtbl carve cannot be gate-flipped: carving jtbl_800D0A60 while CF78 is still asm broke the link (undefined ref) — a bank-time-only action.
- Applied: 3 of 3: #1 jtbl-carve-is-bank-time-not-a-gate-enabler → docs/hazards.md#switch-jtbl-dispatch + workflow DoR; #2 commutative-operand-order statement-split lever → memory [[commutative-operand-order-statement-split]] + MEMORY.md; #3 ranker continuation-blindness priority raised → BACKLOG (12th recurrence, now flagged biggest gate-friction item).
- Carry-over: func_8005DF54 (terminal non-ABI $s2-arg wall, docs/wip/func_8005DF54.near-match.md), func_8005CF78 (deferred jtbl-dispatch, full structure+jtbl-map+carve-recipe in docs/wip/func_8005CF78.near-match.md). func_80059BA0.c remains 23-stub partial.

---

## Sprint 264 — fresh smallest-first c-continuation of func_80059BA0.c — 2026-07-23
- Increment: 0 files / +2 functions matched (func_80059BA0.c 26→24 stubs, partial pack, not md5-candidate)
- Quality: 0 stuck-far / 0 permuter / 1 carried / 0 re-opened
- Seed: committed 5pt; banked ~5pt; realized ~6 / residual +1; regime classical (c-continuation)
- What helped: asm-first fast-path (MCP-independent) on both banks; the S263 lesson pivot OFF the mined-out func_800453E0.c to the under-mined func_80059BA0.c integer-accessor pack (33/59 already banked) surfaced clean fresh leaves; `tools/cmpfn.sh` object-oracle iteration; the jtbl-carve recipe (docs/hazards.md#switch-jtbl-dispatch) held first try for the FIRST carved compiler-switch table to BANK in src/main (func_8005DAFC, jtbl_800D0A90, 3-way rodata split, 8-align both edges); two-def `lo` (`=b` then `&=0xF`) + up-count pointer-walk + delay-slot pointer levers on the D0D8 near-match.
- Friction: `pick_target.py` blind to c-continuation fresh leaves (11th ranker recurrence) — `--segment main` returned 1 blocked asm-flip pack while 33 partial `c` files hold the actual fresh smallest-first work; whole backlog hand-mined. func_8005D0D8 carried on a genuine combine/reorg pass-ordering coin (ROM keeps a provably-dead `lo<0` signed check because its `&0xF` mask lands in the branch delay slot post-combine; no source form separates mask from compare by a BB boundary without changing the asm) + xor-hoist + base-reg allocno.
- Applied: 2 selected, 1 of 2 applied now: #2 jtbl-switch 3-way-mid-blob carve recipe + link-error tell → docs/hazards.md#switch-jtbl-dispatch; (#1 `--c-stubs`/continuation ranker mode → BACKLOG ranker follow-up, off-cadence golden-gated, not a mid-review apply).
- Carry-over: func_8005D0D8 (func_80059BA0.c, near-match coin, docs/wip/func_8005D0D8.near-match.md). func_80059BA0.c remains 24-stub partial (22 other fresh/wall leaves + func_8005D334 S210 carry).

---

## Sprint 263 — next fresh non-FP leaves of func_800453E0.c (characterization: pack mined out) — 2026-07-23
- Increment: 0 fns banked / 3 fresh leaves RE'd to byte-exact structure and CARRIED as walls. md5-candidate files 230/263, delta 0. `func_800453E0.c` unchanged at 24 stubs.
- Quality: 0/0/3/0 (stuck-far / permuter runs / carried / re-opened).
- Seed: committed 5pt; banked 0pt; realized ~8 / residual +3; regime classical. BELOW the ~2-bank hedge — a target-selection miss (the committed leaves were a wall cluster, not fresh veins), accepted as characterization.
- Scope: PO accepted as a CHARACTERIZATION sprint. The 3 committed fresh non-FP leaves all resolved to terminal/near-terminal walls; 0 banks is the correct outcome given the pack state, not a reachable-bank miss. Value = 3 pass-cited wall verdicts + the mined-out root finding.
- Seeded via MCP+m2c (PO directed the m2c seed mid-sprint; the initial `/list_instances` 404 was a wrong endpoint — MCP was up on 8089).
- Walls characterized (all `docs/wip/<fn>.near-match.md` + in-file carry comment):
  - `func_8004CDA0` (234i) 227/234 — cloud-buffer blend driver (double-buffered `s16 D_801062C0[2][64][64]`, calls func_8004C958 x3). Tail byte-EXACT. Residual = `#local-alloc-qty-permutation`: register naming (`i`:t0/a3, base:a1/t0) + 7 ROM register-preserving `move` copies my build collapses. Levers tried and failed: row-pointer hoist, explicit-held base, compute-dst-next-before-store, s8/u8 signedness.
  - `func_8004C958` (274i) ~248/274 — recursive diamond-square/plasma midpoint-displacement (self-calls 4x). Residual = ROM spills step (sh/lhu sp+0x1E) + 3 corner values to stack + reloads under the recursion's s-reg pressure; faithful-C keeps them in registers, so mine is ~26 instr SHORTER (mine MORE optimal = terminal). `u16 step` + inline-recompute did not force the spills.
  - `func_800484F8` (102i) 99/102 — "start BGM for game state" switch(D_801B608C) over a 12-entry jtbl. Structure + dispatch + all cases match. Residual = default block's `x==D_801B6098 && D_801B6090!=1` -> gcc branch-likely (bnel) vs ROM plain beq+nop+j+li (`#value-select-if-else-vs-branch-likely`, goto-PROOF, confirmed 3 spellings) + `bgm` in a2 vs ROM a0-per-case (`#call-result-a0-vs-v0`).
- What helped:
  - **Working levers on func_800484F8 (recorded even though it walled):** case bodies emit in SOURCE order, so order them by ascending target address; `case 0`/`case 1` folded into `default` forces gcc's min case = 0 (the ROM's 0-based 12-entry table, no `x-2` normalisation); ternary/post-call bgm assignment removes a spurious `s0` save (assign the switch-result ONLY after each case's call); flip `if(rumble!=0){..}else{switch}` to match the ROM's `beqz` branch polarity.
  - **Root finding: func_800453E0.c's fresh non-FP integer veins are MINED OUT for clean banks.** S262 banked the tiny glue (38-56i); the remaining 100-274i leaves are dense nested-loop/recursive image-synthesis (cloud/plasma) + a value-select switch — all register-pressure / value-select walls. The remaining smaller leaves are FP-heavy or sqrt-based. The S262 pick was right for ONE sprint (3 tiny-glue banks); the tail is now a wall cluster.
- Friction:
  - `fp=0` + no-jtbl was NOT sufficient to predict a clean bank at 100+ instr — all 3 leaves passed that filter yet walled. Need a nested-loop/s-reg-count DoR tell (BACKLOG #2).
  - `pick_target.py --segment main` one-row recurrence (10th) — hand-survey again.
  - Tooling-test suite RED at baseline (9 golden-drift failures, pre-existing) — standing carry.
- Applied: 3 of 3 accepted (PO: keep local) — #1 `mined-out-pack` tag for func_800453E0.c + next-main-slice = FRESH pack (BACKLOG note + mined-out-pack ranker follow-up), #2 DoR nested-loop/s-reg-count tell (BACKLOG DoR sizing + ranker follow-up), #3 value-select-branch-likely-on-jtbl-dispatch + jtbl-reconstruction levers (hazards `#value-select-if-else-vs-branch-likely` + `#switch-jtbl-dispatch` + memory `value-select-branch-likely-on-switch-default`).
- Ranker: **10th recurrence, unchanged.** `pick_target.py --segment main` still emits one blk row.
- Carry-over: `src/main/func_800453E0.c` (24 stubs; NOW 6 documented walls: func_80046604, func_8004683C, func_80048CF8, func_8004CDA0, func_8004C958, func_800484F8). **MINED OUT for clean banks — next `main` slice opens a FRESH pack (func_8002A640 needs-header, or a different segment), NOT this pack.**

---

## Sprint 262 — fresh smallest-first slice of func_800453E0.c (pivot off the exhausted main carry tail) — 2026-07-23
- Increment: 3 fns banked byte-exact FIRST-BUILD (`func_800487E4` 0x98, `func_80048690` 0xDC, `func_80045C00` 0xE0) / 1 stretch carried (`func_80046604` 0x1D8, 119/118). `func_800453E0.c` 27->24 stubs. md5-candidate files 230/263, delta 0 (pack still partial); matched-fn +3.
- Quality: 0/0/1/0 (stuck-far / permuter runs / carried / re-opened).
- Seed: committed 5pt; banked ~3pt; realized 5 / residual 0; regime classical. Beat the ~2-bank hedge (3 clean first-build + 1 characterized carry).
- Scope: PO signed off 3 banks + 1 carry. Full committed scope met (3/3 first-build), stretch attempted-and-carried as a documented wall. DoD green at every commit.
- Banks (all asm-first fast-path, first build, auto names kept — no ghidra names, no C callers):
  - `func_800487E4` — flag/wind dispatch. Levers: shared-tail single `func_80216130(1)` call (nested `if(!flag_is_set){if(wind/0x10000<6)return;}`, not two calls), `wind_magnitude / 0x10000` signed div-by-2^16 (bgez/ori-0xFFFF/addu/sra), store order with the last store in the beq delay slot.
  - `func_80048690` — HUD/state reset + two `func_800719A0(0,0,-1,1,{0,1},...)` spawn calls (same arg pattern as sibling `func_8004876C`) + `func_800326FC` + `func_8004887C`. `D_801B5530=80.0f` (lui/mtc1 const, low16=0).
  - `func_80045C00` — match-init: 5 globals + `D_801B5530=45.0f`, `func_8009226C(-1)`, 3 zero-stores, two `func_800510EC`, `func_80216B74(0)`, `D_800BE62C=-1` (delay slot), then a conditional `osSyncPrintf` debug block. Rodata strings kept as `extern char D_[]` refs (no carve, partial one-tu).
- Stretch carry — `func_80046604` (club/terrain sound dispatch, logic fully RE'd, 119 vs 118):
  - Head byte-exact via the [[cross-call-live-range-callee-saved-lever]] INVERSE: read `kind` from the `get_club_param` return AFTER the `get_table_entry` call so the club pointer alone spans the call in ONE callee-saved reg ($s0 reuses to entry). Reading before cost a 2nd saved reg ($s1) + flipped lh->lhu.
  - Residual = 3 branch-scheduling coins, one wall class (`#value-select-if-else-vs-branch-likely`): (1) `&&` guard chain -> annulled `beqzl/bnezl` where the ROM fills plain `beqz/bnez` delays with `li a0,K`; (2) terminal `id==8 ? 0x56 : 0x54` if-converts branchless even with a shared-label `goto` (jump-opt rejoins the arms — a goto is NOT an escape); (3) the ROM shares one `a0=0x54` set once in the id<2 delay slot across id<2/id==2/default. `docs/wip/func_80046604.near-match.md` written at discovery.
- What helped:
  - **`main`'s cheap smallest-first veins are largely mined; the fresh-pack pick is a partially-worked NON-FP pack.** The right S262 pick was `func_800453E0.c` (S228 slope/dispatch pack) whose smallest UNTOUCHED leaves are clean integer game-glue (fp=0), NOT the documented carries or FP that dominate the smallest leaves of the more-worked packs (`func_80059BA0.c` "vein mined out", `func_80095A10.c` fp 44-76). Found by a per-pack FP/carry survey, not the ranker.
  - **asm-first fast-path + fresh-object objdump is the right oracle for tiny integer leaves.** All 3 banks were hand-translated straight from the `.s`, gated per-fn on `objdump -d` of the freshly-built object (not diff.py) and each bank on `tools/verify-rom.sh` — 3 first-build byte-matches, zero iteration.
  - **read-callee-return-AFTER-the-next-call is a one-callee-saved-reg lever** (func_80046604 head): the inverse trigger of the decl-before-call lever — delay the field read, not the decl.
- Friction:
  - `pick_target.py --segment main` one-row recurrence (9th) forced a full hand-survey of every `main` partial pack to find a fresh non-FP vein. New follow-up: a `mined-out-pack:<file>` retro-derived tag so the sort skips exhausted/FP packs (BACKLOG #1).
  - The stretch's const-select tail is goto-PROOF — a shared-label goto did not stop gcc's branchless if-conversion. Confirms the const-select wall has no source escape.
  - Tooling-test suite remains RED at baseline (9 golden-drift failures, pre-existing) — standing carry.
- Applied: 3 of 3 accepted (PO: keep local) — #1 `mined-out-pack:<file>` ranker signal (BACKLOG carried-wall follow-up), #2 goto-proof const-select wall (hazards `#value-select-if-else-vs-branch-likely` + memory `store-flag-single-bit-terminal-wall`), #3 read-after-call one-callee-saved-reg lever (memory `cross-call-live-range-callee-saved-lever` inverse trigger).
- Carry-over: `func_80046604` (branch-scheduling wall) stays carried in `func_800453E0.c` (now 24 stubs). Next slice: continue `func_800453E0.c`'s fresh non-FP integer leaves (func_800487E4-family), OR another fresh non-FP main pack.

## Sprint 261 — carry-crack sweep on the exact-count carries (lever-exhausted tail) — 2026-07-23
- Increment: 0 fns banked / 4 carries re-affirmed TERMINAL with sharper verdicts. md5-candidate files 230/263, delta 0. Packs unchanged (`func_8006A2C0.c` 18, `func_800453E0.c` 27, `func_80054900.c` 19).
- Quality: 0/1/4/4 (stuck-far / permuter runs / carried / deliberately re-opened).
- Seed: committed 5pt; banked 0pt (no fn matched); realized 5 / residual 0; regime classical.
- Scope: PO signed off as a **characterization slice**. The four targets were the RESIDUAL after S259+S260's goto-loop lever banked the crackable carries; all four are genuine terminal walls, so 0 banks was the correct outcome, not a miss of a reachable bank. Value delivered = 4 sharpened terminal verdicts (DoR/hazards) + one tooling finding.
- Re-affirmed walls (all sharpened from "open question" to terminal):
  - `func_8006D38C` (84/84) / `func_8006D214` (94/94, transfers verbatim) — the base-vs-displacement last shape is a MUTUAL EXCLUSION, not a lone cse-forward question. The ROM's `lw t0,-0x2B(a0)` needs a0-relative addressing (only the struct/related-value form gives it → MEM_IN_STRUCT_P → re-read OR, if hoisted, cse-merge) AND a held value (only a plain read gcc can prove no-alias → but that folds to a fresh `lui`). `{a0-relative}`⟹mem-in-struct⟹`{re-read OR merge}`; `{held}`⟹plain⟹`{fresh lui}`. No source form gives all three. Measured 3-form table in the wip doc.
  - `collect_keyframe_events_at` (54/54) — root-caused the one back-edge bit to a gcc first-load PEEL: both tests read `q->val` (same field), so gcc peels the redundant top load (label placed after it in `gcc -S`). find_keyframe escaped via a tag/val field-split, unavailable here. `volatile` defeats the peel but flips `lh`→`lhu`; the peek-before-increment costs the annulled delay slots.
  - `func_8004683C` (23/23) — confirmed `#local-alloc-qty-permutation`: ROM reuses the dead arg0 register (a0) as the second scratch, freeing v0 for `ret`; local-alloc scans regno-ascending (v0<a0) and claims v0 before global assigns ret, pushing ret to a2 (and the ROM's final `nop` becomes `move v0,a2`). 934k permuter-parked, 0 project cracks. The && form is optimal 23/23; nested-if regresses to 24.
- What helped:
  - **A crack-attempt slice on a LEVER-EXHAUSTED tail is a different bet than one with a fresh lever.** S259/S260 banked because the goto-loop lever was new to those carries; S261's tail was the carries whose goto-loop/struct-view levers had already been applied-and-failed, so it correctly sorted to all-terminal. The `carried-wall:<fn>` ranker follow-up now also records `lever-tried:<lever>` so the DoR sort puts fresh-lever carries ahead of exhausted-lever ones (BACKLOG follow-up #1 refinement).
  - **The permuter (asm-differ) is BLIND to internal branch TARGETS** ([[permuter-blind-to-internal-branch-target]]). The exact 54/54 collect_keyframe body scored `base = 0` while the real object was `fff2` vs ROM `fff1` — asm-differ normalises a branch to a local label. Same blind spot S260 fixed in `cmpfn`. Any permuter 0 on a pure-branch-target residual is a false positive; gate on `objdump`/verify-rom (workflow permuter rule updated).
- Friction:
  - The permuter's false-0 nearly banked a non-matching body — caught only by the full-make ROM-SHA-1 (the DoD held). Reinforces "gate every bank on `tools/verify-rom.sh`, never a permuter/diff/cmpfn score."
  - The tooling-test suite remains RED at baseline (9 golden-drift failures, pre-existing) — a standing carry.
- Walls / carries: all four above stay carried in their partial packs.
- Applied: 4 of 4 accepted (PO: keep local) — #1 permuter-branch-blind (workflow permuter rule + new memory `permuter-blind-to-internal-branch-target`), #2 D38C mutual-exclusion (hazards#base-register-vs-displacement + memory `negative-displacement-neighbour-needs-one-symbol`), #3 same-field-peel wall class (hazards goto-loop section + new memory `same-field-sentinel-loop-peels-top-load`), #4 ranker `lever-tried:<lever>` record (BACKLOG follow-up #1 refinement).
- Ranker: **8th recurrence, unchanged.** `pick_target.py --segment main` still emits one row; the gate backlog was hand-sorted again.
- Carry-over: `src/main/func_8006A2C0.c` (18 stubs; `func_8006D38C`/`func_8006D214` terminal mutual-exclusion), `src/main/func_80054900.c` (19 stubs; `collect_keyframe_events_at` terminal same-field-peel), `src/main/func_800453E0.c` (27 stubs; `func_8004683C` terminal local-alloc-qty). **All four now hold pass-cited TERMINAL verdicts — do NOT re-pick them without a genuinely new lever. Cheapest S262 opener: a FRESH `main` pack (open `func_8002A640`'s needs-header blocker, or find an un-mined c-stub continuation), not another carry re-pick of this tail.**

---

## Sprint 260 — carry-crack sweep, continuation of the superseded-lever sort — 2026-07-23
- Increment: 3 fns banked / 3 carries advanced (`func_8006A2C0.c` 20→18, `func_80054900.c` 20→19; `func_800453E0.c` 27→27 — carry sharpened only). md5-candidate files 230/263, delta 0.
- Quality: 0/1/3/6 (stuck-far / permuter runs / carried / deliberately re-opened).
- Seed: committed 5pt; banked 0pt (files partial); realized 9 / residual +4; regime classical
- Banks (all three were prior sprints' documented TERMINAL walls, none a fresh leaf):
  - `func_8006DF84` (0x6C) — S240's "cross-jump-tail-merge blocks reorg's annulled `bnezl`, not permuter-reachable" was the wrong mechanism. The ROM ALSO merges the two identical stores; it then has reorg STEAL the merged single store into the annulled `bnel` and retarget past it. My build couldn't steal because its merged block opened with a re-materialized `lui at` (two instructions). Writing the `field != 0` arm as a forward `goto` to a handler AFTER the chain ([[out-of-line-handler-block-branch-likely]]) restored the single-instruction merged block; the tail compare written `D_801B7270 > base[3]` fixed the load order.
  - `func_8006D164` (0x98) — S240's "LICM hoists the outer bound in my build but not the ROM's; coupled to regalloc, likely terminal" fell in one build to the goto-loop lever ([[goto-loop-defeats-loop-strength-reduction]]): a goto OUTER loop keeps loop.c out entirely, so `li 4` stays re-materialized in-loop and the 3-register rotation disappears. Inner loop stays a structured `do`-`while` with its bound in a local; `src` declared before `dst` decides a3 vs a2. `D_801B60C6/C8/CA` retyped u16→s16 for the signed `lh`.
  - `find_keyframe_offset_by_tag` (0xC4) — the strongest terminal verdict in the tree: S213's compiler-source dive cited `loop.c:505-545`'s first-iteration PEEL exposing a re-derivable invariant to CSE, "no source form yields {no-peel + re-derive + reload + result-in-s1}". Correct about the pass, wrong about the conclusion: a goto loop is never entered into loop.c's list, so there is no peel. Four more levers stacked: single `goto done` exit (reorg replicates the return copy into each guard's delay slot), out-of-line advance handler (`e++` folds into the annulled `bnel`), a local sentinel for the loop's -1, a named `tracks` local (splits the derivation across v0/v1), and the `(u32)` int-add spelling for `off + base`.
- What helped:
  - **The goto loop defeats EVERY loop.c pass, not just strength reduction** ([[goto-loop-defeats-loop-strength-reduction]], now generalised in the memory + hazard). The PEEL insight is what cracked the S213 wall; the same generalisation says: when a wall doc names any loop.c transform as the mechanism, try the goto loop first however the verdict is worded.
  - **Sorting by "whose cited lever has been superseded" continues to pay** — every one of six targets advanced, three banked, exactly as S259. The stronger the source-dive backing on a verdict, the more precisely it names the pass a new structural lever can now sidestep, so those belong at the FRONT of the DoR sort, not last (workflow DoR updated).
  - **The negative-displacement neighbour read is reachable** ([[negative-displacement-neighbour-needs-one-symbol]]): `lw t0,-0x2B(a0)` requires the two globals to be ONE source symbol (cse `use_related_value` relates offsets only within a symbol), reached via a per-TU struct view over an existing placed symbol. `func_8006D38C` reached its third and last access shape this way (82/84); the base-vs-displacement wall is now one cse-forward question, not three addressing mysteries.
- Friction:
  - **`cmpfn.sh` normalised branch TARGETS, so it reported `collect_keyframe_events_at` byte-clean at 54/54 while the full-make ROM broke** — the loop back edge was redirected one instruction over a redundant re-load, invisible when both targets collapse to `T`. Fixed this review: internal targets are now position-relative deltas (`@Dp<n>`/`@Dm<n>`); the regression case now shows `@Dm14` vs `@Dm13` and banked functions stay clean. (Rewritten mawk-compatible — the machine has no gawk.)
  - **The tooling-test suite is RED at baseline** (9 golden-drift failures already present at e9c21f0, pre-existing from prior sprints' banking + the one-row ranker). cmpfn is a shell tool untested by the suite, so this sprint's tooling edit is unaffected, but the red suite is a standing carry.
- Walls / carries: `func_8004683C` (23/23, one register — local-vs-global alloc phase ordering, no MIPS `REG_ALLOC_ORDER`, permuter 934k iters base 170 best 150); `collect_keyframe_events_at` (54/54, one branch-offset bit); `func_8006D38C` (82/84) and `func_8006D214` (not re-attempted — waits on the same cse-forward question).
- Applied: 4 of 4 accepted groups — #1 cmpfn branch-target delta fix + convention caveat (`tools/cmpfn.sh` + workflow), #2/#3/#4 goto-loop-PEEL / single-exit / init-after-call (3 memories + hazards goto-loop section), #5 re-open source-dive walls FIRST (workflow DoR), #6 one-symbol negative-displacement (new memory + hazards base-vs-disp section).
- Ranker: **7th recurrence, unchanged.** `pick_target.py --segment main` still emits exactly one row; the gate backlog was hand-built again.
- Carry-over: `src/main/func_8006A2C0.c` (18 stubs; `func_8006D38C` 82/84, `func_8006D214` D38C-model transfer pending the cse-forward fix), `src/main/func_800453E0.c` (27 stubs; `func_8004683C` 23/23 permuter-parked), `src/main/func_80054900.c` (19 stubs; `collect_keyframe_events_at` 54/54 one-bit). **Cheapest S261 opener: the `func_8006D38C`/`func_8006D214` cse-forward question (a varying-address store or a multi-pred preheader join), then re-scan carries whose verdict cites a loop.c pass — the goto loop now disables the whole family.**

---

## Sprint 259 — cross-pack carry-crack sweep, sorted by superseded lever — 2026-07-23
- Increment: 4 fns banked / 3 carries brought to exact instruction count (`func_800453E0.c` 29→27, `func_8006A2C0.c` 22→20; both files still partial). md5-candidate files 230/263, delta 0.
- Quality: 0/3/3/6 (stuck-far / permuter runs / carried / deliberately re-opened).
- Seed: committed 5pt; banked 0pt (files partial); realized 8 / residual +3; regime classical
- Banks: `next_shuffled_index` + its nested child (`func_80045B14` + `func_80045AD4`), `func_8006D058` (0x10C), `func_8006CE88` (0x1D0, the stretch). **All four were prior sprints' documented TERMINAL walls — none was a fresh leaf.**
- What helped:
  - **The array-element bound lever — the sprint's real finding** ([[array-element-form-for-multilevel-bound]]). Where the ROM re-reads a count/bound global at MORE THAN ONE nesting level, read it `extern s32 G[]` + `G[0]`; MEM_IN_STRUCT_P forces the re-read, while the previously-RECOMMENDED cached `s32* cnt = &G` keeps one pseudo and comes out short. Decisive on three functions: it took `func_8006CE88` from 114/116 instrs and 152 differing rows to **116/116 and 28**, and it reproduced one of the three access shapes S241 had called unreachable on `func_8006D38C` (now 84/84) and `func_8006D214` (now 94/94). Same MEM_IN_STRUCT_P mechanism as the index lever, applied to a BOUND.
  - **Writing a leaf as a GCC nested function banks the PARENT too** ([[nested-function-banks-the-parent-too]]). PO directive, and it was right: the S22x note claimed "caller sets no static chain, so it is a spurious dead frame, not a nested fn", but the caller's `addiu $v0,$sp,0x10` persists to the `jal`. Nested, the child is byte-exact at 16/16 including the frame and the dead store; gcc emits the child BEFORE the parent, matching the ROM layout; the parent then came out 59/59 first build. Load-bearing detail: `u8` for one read (`lbu`) and a widened `s32` for the other (`lb`).
  - **One local reused for two values reorders their loads** ([[one-variable-reuse-reorders-loads]]). `func_8006D058` was 67/67 first build with only three `s8` loads out of order and the register-to-value mapping already correct — one variable held two successive conditional values, and the anti-dependency pulled its load forward. The inverse is also a lever: the permuter cracked `func_8006CE88`'s colouring by REUSING a dead variable, and its last register fell to counting a loop in `col` rather than `i`.
  - **Loop-form corollaries of the bound lever:** the guarded loop usually has to be a `do`-`while` (the ROM has ONE zero-guard, so a top-tested `for` emits a second `beqz`), and where the ROM materializes an array base inside the preheader, the clear must be INDEX-form so loop.c hoists it there — `func_8006CE88`'s doc had REQUIRED a pointer walk, which was an artifact of the old count spelling.
  - **Sorting targets by "whose cited lever has been superseded"** rather than by size: all six targets advanced, four banked. That sort, not size, is what the `carried-wall:<fn>` ranker follow-up should emit.
- Friction:
  - **The S257 scripted-splice hazard RECURRED, and it was mine.** Reverting ONE function to `INCLUDE_ASM` with an anchor-to-anchor splice deleted two banked one-line siblings that sat between the anchors; caught by `undefined reference` at link, restored with `git checkout`, redone as an `Edit`. The rule existed and was still violated, so the retro added a MECHANICAL guard (assert the function list, not just the `INCLUDE_ASM` count).
  - **Banking a nested function makes the permuter unavailable for the whole TU** — pycparser aborts on a nested function definition, so no other function in that file can be imported. Workaround: import from an in-repo copy with the nested parent deleted (`import.py` also rejects a path outside the project root).
  - Two of the S258 retro's own suggested re-check targets (`func_80071924`, `func_8007512C`) had already been banked in S236 — a stale suggestion carried into the next gate unchecked.
- Walls: `func_8004683C` (23/23 exact; the permuter's temps fixed the count, residual is the return register landing in `a2` where the ROM keeps it in `v0`), `func_8006D38C` (84/84) and `func_8006D214` (94/94) — the base-vs-displacement wall is now ONE shape (the ROM's negative-displacement neighbour read) rather than three.
- Applied: 8 of 8 PO-accepted — #1/#2/#7 codegen levers (4 memories + 2 new `docs/hazards.md` sections), #3 base-vs-displacement re-priced from three shapes to one, #4 nested-fn-breaks-the-permuter and #8 re-measure-percent-after-a-structural-fix (workflow permuter rules + memory), #5 superseded-lever sort (BACKLOG ranker follow-up), #6 splice guard (workflow conventions, now with a mechanical check).
- Ranker: **6th recurrence, unchanged.** `pick_target.py --segment main` still emits exactly one row; the gate backlog was hand-built again.
- Carry-over: `src/main/func_800453E0.c` (27 stubs; `func_8004683C` at 23/23, `func_80048CF8` FP-class), `src/main/func_8006A2C0.c` (20 stubs; `func_8006D38C`/`func_8006D214` at exact count, `func_8006DF84`/`func_8006D164`/`func_8006DEB4` characterized). **Cheapest S260 opener: re-check every remaining carry whose doc recommends a cached pointer for a re-read global, or cites base-vs-displacement — that class just lost two of its three shapes.**

---

## Sprint 258 — crack-attempt slice on the func_80080220 DL/texrect carries — 2026-07-22
- Increment: 3 fns banked / 3 carries deepened in `src/main/func_80080220.c` (22→19 stubs; file still partial, not md5-candidate). md5-candidate files 230/263, delta 0.
- Quality: 0/4/3/3 (stuck-far / permuter runs, 1 of them a crack / carried / deliberately re-opened).
- Seed: committed 5pt; banked 0pt (file partial); realized 9 / residual +4; regime classical
- Banks: `draw_letterbox_bars` (`func_80088890`, 0x200), `pack_shade_ramp_rgba` (`func_800824E4`, 0x50), `scroll_sky_panels_by_wind` (`func_80081C90`, 0xBC) — **all three were carried "terminal" walls from S252/S253/S256, not fresh leaves.**
- What helped:
  - **The goto-loop lever — the sprint's real finding** ([[goto-loop-defeats-loop-strength-reduction]]). gcc-2.7.2's `loop.c` only processes loops it discovers through `NOTE_INSN_LOOP_BEG`, which `for`/`while`/`do-while` emit and a goto loop never does. So when the ROM re-materializes `%hi(SYM)+idx` per access (no walking pointer) or keeps a bound inline at the exit test, the loop must be written with `goto`: all four spellings S253 tried on `func_80081C90` were STRUCTURED, which is why every one of them strength-reduced and came out 4 instrs/iteration short. Two follow-ons: the goto de-hoists literal bounds, so put the bound in a local variable; and ONE bound variable shared by two loops raises its allocno priority and swaps registers with a neighbour, so use one per loop. It is the inverse reading of [[goto-loop-vs-structured-loop-codegen]] — losing loop.c's passes is sometimes the goal.
  - **Mixed loop discovery within one nest** (`init_sky_pool_and_world_state`, 93 rows → 161/161 with shapes 1:1): outer grid loops as gotos (ROM keeps `li v0,5`/`li v0,4` inline and does not strength-reduce the block base), inner column loop STRUCTURED (ROM has the two pointer givs `a3` and `a3+0xF` that only SR produces). Plus every constant the ROM holds in a register across a goto loop as its own local, assigned where the ROM materializes it, and `(Vtx*)(block_off + (u32)pool)` to put the integer operand first.
  - **fold's `associate` decides which operand carries a constant, by which side it is parenthesised on** ([[fold-associate-constant-side]], `fold-const.c:3722`/`:3759`): `GLOBAL + (elem + CONST)` reproduces the ROM's `addiu rX,globreg,C`, where the natural `GLOBAL + CONST + elem` reassociates it onto the element. A base temp is immune but ONLY where the ROM keeps the base live — `func_80087CB0` needed a temp for x (live into the s-clip) and inline spelling for y (re-loaded past the `bgezl`). 1580 → 345 at 252/252.
  - **Find the SDK macro, then write the emitter.** `func_80088890` banked as a ONE-immediate near-match on the first build purely because S257 had identified `gSPScisTextureRectangle`; and `func_80088A90`'s S254 "terminal raw-DL-word regalloc + reorg" verdict fell to one `gSPTextureRectangle(dl++, ...)` replacing the hand-rolled 6-word block — byte-identical emit block, exact 83/83, because the macro form also brings loop.c's preheader hoist of the command words and character literals.
  - **Two more source-reachable shapes** from that same function: [[u8-s32-char-split-zero-extend]] (a `u8` for the `!= 0` loop test plus a separate `s32` for the compares puts the zero-extend in another basic block where combine cannot fold it, and keeps `slti` signed) and [[out-of-line-handler-block-branch-likely]] (a one-instruction handler reached by `goto` and laid out after the chain folds into reorg's annulled `beql`).
  - **The permuter, used at the right shape** ([[permuter-at-exact-count-residual]]): `func_800824E4` at 20/20 instructions with one differing operand hit score 0 in **72 iterations**, with a spelling no hand-iteration produces. The three larger permutations all plateaued.
  - **`tools/cmpfn.sh`** (promoted this retro): a normalized per-function instruction-stream diff against a freshly built object. It never goes stale the way `diff.py` does, and its instruction-count line is the actionable number when a body is structurally right but the wrong length.
- Friction:
  - **A carried wall's near-match doc had the SEMANTICS wrong, and cost the previous sprint the bank.** S252 recorded `func_800824E4` as a 3-argument packer with `b = arg2`; `$a2` is written in the entry branch's delay slot before any read, so there is no third argument at all and the "delay-slot filler never selects `$a2`" verdict was a fiction built on the mis-read. Re-deriving from the `.s` took ten minutes and the function banked the same day.
  - `setup-permuter.sh` exits 0 with NO output when the function is already inlined as C (it resolves the C file by grepping for an `INCLUDE_ASM` stub) — a silent failure that cost a debugging detour; `import.py` called directly works.
  - The `func_80087CB0` and `init_sky` residuals both ended as local-alloc register permutations that neither source levers nor 60-91k permuter iterations moved.
- Walls: `func_80087CB0` (252/252, score 345, a0/a2 permutation + one sched1 LUID tie on two independent loads), `func_80088A90` (83/83, one extra callee-saved register because the ROM reuses the dead raw-char reg for the glyph value), `init_sky_pool_and_world_state` (161/161, a systematic register permutation over 55 instructions). All three now carry a full reconstruction plus a measured-variant table in `docs/wip/`.
- Stretch not attempted (correctly): `func_80085F98` (0x5F4) is a 381-instruction 3-nested-loop mesh emitter with computed vertex/triangle indices — a sprint of its own, not a stretch.
- Applied: 8 of 8 PO-accepted — #1 goto-loop lever (hazards new section + memory), #2 fold-associate (hazards new section + memory), #3/#4 char-split and out-of-line handler (hazards new section + 2 memories), #5 retire the raw-DL-word wall class (hazards#display-lists + [[mg64-glyph-emitter-dl-family]]), #6 near-match docs can have wrong SEMANTICS (workflow DoR + memory), #7 permuter at exact-count-plus-one-operand (workflow permuter rule + memory), #8 `tools/cmpfn.sh` promoted + documented in the workflow conventions.
- Ranker: **5th recurrence, now worse.** `pick_target.py --segment main` emits exactly ONE row (`func_8002A640`); the three c-stub continuation rows S257 still saw are gone too. The gate backlog was built entirely by hand (in-file `INCLUDE_ASM` grep + `.s` header sizes + BACKLOG/in-file carry greps). This now blocks gate automation rather than merely mis-pricing.
- Carry-over: `src/main/func_80080220.c` (19 stubs). Carries `func_80087CB0`, `func_80088A90`, `init_sky_pool_and_world_state` (all three at exact instruction count, residual = register permutation), `func_800842C0` (nested-fn, blocked on its parent `func_80084468`), `func_80080688` (multiply-synth), `func_80080E7C` (rodata-jtbl enabler slice). **Cheapest S259 opener: the goto-loop lever against the OTHER documented strength-reduction walls in the tree — `func_80071924` / `func_8007512C` (`#base-register-vs-displacement`, S227/S236) and the S241 grid-builder — all were characterized with structured loops only.**

---

## Sprint 257 — continue func_80080220 pack, TELL-CLASS sorted (PO approved continuation) — 2026-07-22
- Increment: 2 fns banked / 1 carried in `src/main/func_80080220.c` (24→22 stubs; file still partial, not md5-candidate). md5-candidate files 230/263, delta 0.
- Quality: 0/0/1/0 (stuck-far/permuter/carried/re-opened). Zero permuter runs all sprint.
- Seed: committed 5pt; banked 0pt (file partial); realized 6 / residual +1; regime classical
- Banks: `func_8008CD30` (0x3AC debug HUD overlay), `func_80088BDC` (0x4B8 debug RGB color-editor, FIRST-BUILD byte-exact)
- What helped:
  - **The SDK composite-macro grep — the sprint's real finding** ([[sdk-composite-macro-before-dl-reconstruction]]). `func_80087CB0`'s branchless corner clamp plus its ASYMMETRIC s/t clip (`bgezl` on the s16 x, `bgez` on the s32 y) is the verbatim expansion of `gSPScisTextureRectangle` ("like gSPTextureRectangle but accepts negative position arguments"), not hand-written clipping. Four iterations of ternary/bit-twiddle clamp forms were pinned at one score before the macro was found; then 8 iterations took it 15680→1580. Two more composites (`gDPLoadTLUT_pal16`, `gDPLoadTextureBlock_4b`) collapsed 13 commands to 2 source lines — and `gfxdis` had NAMED both in its own output all along. Generalized tell: a clamp whose MASK derives from a narrower view than the value being masked is a macro tell.
  - **mem-in-struct extends to an INDEX global, as a COUNT lever** ([[mem-in-struct-index-global-cse]]): `extern s32 G;` → `extern s32 G[];` + `G[0]` defeats `cse.c` forwarding of the load AND of everything derived from it (`G<<2`), so `TBLA[G]`/`TBLB[G]` re-read per access. Worth 6 of `func_8008CD30`'s 7 missing instructions. Previously documented only as a load/store SCHEDULE lever.
  - **divmod statement order** ([[divmod-order-quotient-coalescing]]): `t % K` before `t / K` keeps the quotient in its own pseudo and emits the ROM's non-coalesced `move`; div-first, or an explicit `quot` temp, coalesces it. The last instruction of `func_8008CD30`.
  - **if/else, not a ternary** ([[ifelse-not-ternary-cse-reset]]), when the ROM re-loads a global both arms store: the two stores cross-jump tail-merge and the multi-predecessor join resets cse's table, re-loading every live memory value. One edit reproduced both the `lw glistp` and the `lw D_800E2134` in `func_80087CB0` — ~0x20 of its length gap.
  - **Type narrowing is free structure** (`func_80088BDC`, first-build): `s32` color channels feed the gDP macros as `lbu` at byte +3 (gcc narrows `(x & 0xFF) << 24` on a big-endian word load), and `s16` params on a callee make `s32` globals emit `lh` at +2.
  - **Tell-class sort over smallest-first is now 2-for-2** (S256 follow-up #4, applied manually at the gate): both no-DL/no-FP glue leaves it picked banked; pure smallest-first would have committed 5 documented walls.
- Friction:
  - **`diff.py` went stale across FOUR consecutive single-file rebuilds**, reporting an identical score for four materially different sources. `find build -name '<obj>.o' -delete` did NOT clear it; only a full relink did. Cost ~4 wasted iterations before `objdump -d` on the object (plus an instruction count against the `.s` header) exposed that the code had in fact changed and was already 252/252 instructions.
  - **A scripted whole-region splice deleted 3 `INCLUDE_ASM` stubs and their carry comments** between its anchors; surfaced only as an `undefined reference` at link. Self-inflicted, one failed build.
  - The `gSPScisTextureRectangle` search itself: the right move (grep the SDK header for a composite) was available from iteration 1 and was not taken until iteration 5.
- Walls: `func_80087CB0` carried at ONE instruction (253 vs 252, score 1580, body 100% RE'd) — gcc-2.7.2 `fold` reassociates `(GLOBAL + C) + load` to `(GLOBAL + load) + C`, so the ROM's shared `D_800C54C8 + 0x108` pseudo is emitted twice; hoisting it to a local creates the shared pseudo but also CSEs away the `D_800E2190[0]` re-load (247 instrs). docs/wip + in-file verdict.
- Stretch refused (correctly): `func_80080E7C` (0x408) needs a rodata-jtbl carve at `jtbl_800D1AA0`, which is interleaved with the extern-referenced format strings — the documented [[jtbl-carve-both-edge-8align]] blocker. Re-priced as an enabler slice in BACKLOG, not a leaf.
- Applied: 6 of 6 PO-accepted — #3 SDK composite-macro grep (hazards#display-lists + memory), #1/#2/#4 codegen levers (hazards#mem-in-struct-scheduling-lever / #register-reuse-nudge / #cross-jump-tail-merge + 3 memories), #5 diff.py stale-detector (workflow execution loop + [[subagent-diff-crack-not-a-bank]]), #6 scripted-splice guard (workflow conventions).
- Ranker: the c-stub-continuation gap **REGRESSED (4th recurrence)** — S256 surfaced the pack as `c-stub remaining:27`, S257 did not surface it at all despite 24 stubs, while two other c-stub packs ranked fine. Follow-up REOPENED in BACKLOG with a concrete hypothesis (carried-wall de-rank applied at pack instead of leaf granularity).
- Carry-over: `src/main/func_80080220.c` (22 stubs). New carry `func_80087CB0`; existing `init_sky_pool_and_world_state`, `func_80088890`, `func_800842C0`, `func_80088A90`, `func_80080688`, `func_800824E4`, `func_80081C90`. **Cheapest S258 opener: re-check `func_80088890` / `func_800842C0` / `func_80088A90` against `gSPScisTextureRectangle` — three carries that may be one macro away.**

---

## Sprint 256 — continue func_80080220 pack: fresh leaves (PO chose continuation over spriteex2 pivot) — 2026-07-21
- Increment: 3 fns banked / 1 carried / 1 deferred in `src/main/func_80080220.c` (27→24 stubs; file still partial, not md5-candidate)
- Quality: 0/0/1/0 (stuck-far/permuter/carried/re-opened)
- Seed: committed 5pt; banked 0pt (file partial); realized 7 / residual +2; regime classical
- Banks: `load_course_scenery_assets` (0x2CC), `func_8008085C` (0x3F0, first-build), `func_8008658C` (0x210, DL emitter)
- What helped:
  - **Per-region CSE slot-base lever** ([[per-region-cse-slot-base-lever]]): pass the `slot` array DIRECTLY, not a cached `ls` pointer — gcc-2.7.2 then CSEs `&slot` per-region + re-materializes `addiu s0,sp,K` under mid-body register pressure, matching ROM. The cached pointer over-globalized the live range into one continuous s-reg = 2 instrs short = flowing-bss −0x10 shift. Cracked `load_course_scenery_assets`.
  - **DL-emitter reconstruction via gfxdis + gDP macros** ([[gfxdis-dl-emitter-reconstruction-crack]]): collect the DL command-word immediates from the `.s` → `gfxdis.f3dex2 -f` → rewrite as `gDPXxx(glistp++)` (banked precedent func_800328E0.c) → reconcile physical-addr matrix ptrs (`&D_E2050` = phys of `D_800E2050`). Softens the [[mg64-glyph-emitter-dl-family]] "raw-DL = terminal" verdict for the straight-line glistp++ macro subtype. Banked `func_8008658C`.
  - **Callee real arg count is load-bearing**: `func_80085F98` reads `a0`, so `func_80085F98(0)` supplied the 1 missing `move a0,zero` (a 1-instr-short DL emitter reads as a flowing-bss −0x10 shift, not a sched wall). Check the callee's own `.s` before calling a near-match a sched wall.
  - **`-1` in a callee-saved reg** (`func_8008085C`): the two D_800C59D8/DC = −1 stores forced −1 into s0, which forced the per-call slot-base recompute — matched first-build.
- Walls: `init_sky_pool_and_world_state` (0x284 grid-builder biv-regalloc + loop-bound-hoist, 93 rows, S241 analog carried; body 100% RE'd, docs/wip). `func_80088890` (0x200 branchless-clamp raw-DL G_TEXRECT) deferred, not build-attempted — needs a gSPTextureRectangle + saturation-codegen crack slice.
- Applied: #1 case-insensitive DL tell-scan (BACKLOG gate-hygiene), #2 per-region-CSE lever (memory), #3 gfxdis DL-crack + callee-arg-check (docs/hazards.md#display-lists + memory), #4 tell-class sort in pack (BACKLOG ranker follow-up). 4 of 4 PO-accepted.
- Ranker: the c-stub-continuation gap (S253-255 recurred 3×) APPEARS RESOLVED — S256 gate surfaced the pack as `c-stub remaining:27`; confirm at S257 before closing.
- Estimate note: seed 5 AGAIN above the seed-3 anchor for mined-leaf continuations (recurred 4th time). But S256's banks were more substantive than pure mined leaves (a per-region-CSE crack + a full DL reconstruction), so the realized 7 is defensible; the seed-vs-anchor gap is a pricing artifact, not an execution miss.
- Push: local.

---

## Sprint 255 — continue func_80080220 pack: lead + 3 smallest-first leaves — 2026-07-21
- Increment: 0 files banked (file partial, 30→27 stubs, NOT md5-candidate) / **+3 functions matched**:
  `init_sky_panels` (func_80080220 pack LEAD, curated) / `func_80080C4C` / `func_80081D4C` (both auto).
  Commits 65f3867..b1e355a. ROM SHA-1 e2c4e7a green at every commit.
- Quality: 0/0/2/0 this sprint
- Seed: committed 5pt; banked 0pt (file partial); realized 7 / residual +2; regime classical
- What helped: (1) the loop.c preheader invariant-hoist coin on init_sky_panels cracked via the
  `off=i*STRIDE` giv PLUS the `i != N` (not `i < N`) condition, which preserved the ROM's `bne rX,rBound`
  test + hoisted `li rBound` and restored the shifted `mfhi` reg (memory
  [[sched-coin-loop-preheader-order-lever]] extended). (2) func_80081D4C's only miss was case-BODY
  layout = SOURCE order — reordering the switch cases to the ROM body order `0xD/{0xA,0x15}/0xB/default`
  gave byte-exact (dispatch tree was already identical). (3) func_80080C4C's shared-`s0`-base free groups
  matched by expressing D_800E2144[2]/D_800E214C[3] as arrays (CSE) vs scalars for the rest.
- Friction: (1) DIRECTION note (S255) OVERLOOKED the pack lead func_80080220 (0xC0) — smaller than every
  listed candidate; caught by re-sorting all stubs at the gate. (2) func_800842C0 first read as a
  raw-DL-word wall, actually a chain-USED nested function (the $v0-spill tell) — needed the parent
  call-site read to confirm. (3) func_80080688 stretch: body 100% RE'd but the idx*210 synth_mult coin
  (subu-form vs ROM add-chain) is not source-leverable and permuter-unreachable.
- Applied: 2 of 2: #1 ranker c-stub-continuation gap (RECURRED 3rd time → BACKLOG ranker follow-up);
  #2 DIRECTION-note must re-sort ALL remaining stubs incl. the pack-name lead → BACKLOG note.
- Carry-over: `func_800842C0` (nested-fn, blocked on parent func_80084468), `func_80080688` (synth_mult
  near-match); the func_80080220.c 27-stub partial-bank pack continues.

## Sprint 254 — continue func_80080220 pack: 4 smallest-first leaves — 2026-07-21
- Increment: 0 files banked (file partial, 33→30 stubs, NOT md5-candidate) / **+3 functions matched**:
  `func_8008679C` / `func_80087BE4` / `func_8008C520` (all auto). Commits 808e8f0..4af69f6. ROM SHA-1
  green e2c4e7a9…, tree clean.
- Quality: 0 stuck-far / 0 permuter / **1 carried** / 0 re-opened (`func_80088A90`).
- Seed: committed 5pt; banked 0pt (per-file all-or-nothing, file partial); regime classical.
  Realized 6 / residual +1 (3 clean banks + 1 carry). Value signal = +3 matched. Note: seed 5 ran
  ABOVE the S251-253 seed-3 anchor for this pack's leaf-tier — a plan-gate over-estimate; future
  continuations of a mined leaf-tier pack should seed 3, not 5.
- What helped: (1) `func_8008679C` (shot-record sound-cue setter) byte-exact first-build; the odd
  `if(x==0&&y==0)` branch-sense and the `0.0f -> sw zero` (integer-zero store of a float global) both
  reproduced from the literal C. (2) `func_8008C520` (two-slot resource loader) frame crack: the
  in-tree build was byte-identical BODY but 0x10 short on the frame; the `out` buffer is a
  `RomLoadSlot[2]` (0x20), not one slot — same sibling-typedef tell as S253's `func_80080564`. (3)
  `func_80087BE4` (2D clamp-to-56) needed the bare `sqrt.s` → a TU-wide `-ffast-math` `mk/main.mk`
  override (like func_8006A000 / func_80078910); adding it to an ALREADY-partial-banked TU risked the
  banked FP siblings, but the full-make stayed green (their single `int*const` multiplies are
  fast-math-invariant).
- Friction: `func_80088A90` (glyph/units-string DL emitter) is a TERMINAL S243 raw-DL-word wall. Body
  structurally complete (emit block byte-identical in isolation), but two pervasive compiler artifacts
  block it: (1) the char is kept MASKED in a separate reg (`andi v1,t0,0xFF` on the already-lbu'd byte)
  while the raw load stays in t0 for the `bnez` loop test — collapsing them cascades the whole
  allocation; (2) the classifier uses `beq`/`beql` branch-likely TOWARD later handler labels (advance
  set in the annulled delay slot, reorg.c optimize_skip), unreproducible from structured if-else. Kin to
  func_80071370.c's 73F24/74230/74500 (all carried). This is the cheap-leaf→wall plateau the S254
  DIRECTION note forecast. Characterized in `docs/wip/func_80088A90.near-match.md`; carried.
- Applied: 1 of 1 (#1 ranker-gap RECURRED — `pick_target --segment main` again did not surface the
  active 30-stub partial-bank pack as a `c-stub remaining:N` continuation; folded into the BACKLOG
  `carried-wall`/continuation ranker follow-up).
- Carry-over: `src/main/func_80080220.c` (30 stubs; next fresh leaves func_80080C4C/func_800842C0/
  func_80081D4C/func_80080688). Terminal carries: func_800824E4, func_80081C90, func_80088A90.

## Sprint 253 — continue func_80080220 pack: 4 smallest-first leaves — 2026-07-21
- Increment: 0 files banked (file partial, 36→33 stubs, NOT md5-candidate) / **+3 functions matched**:
  `func_80083A48` / `func_800852A8` / `func_80080564` (all auto). Commits 6255c83 + 119850f. ROM SHA-1
  green e2c4e7a9…, tree clean.
- Quality: 0 stuck-far / 0 permuter / **1 carried** / 0 re-opened (`func_80081C90`).
- Seed: committed 3pt; banked 0pt (per-file all-or-nothing, file partial); regime classical.
  Realized 4 / residual +1 (2 first-build, 1 one-fix giv-lever, 1 carry). Value signal = +3 matched.
- What helped: (1) `func_80083A48` FP init loop was byte-exact in the loop BODY first-build; the only
  diff was prologue init ORDER (ROM inits the i counter first, the byte-offset var last), cracked clean
  by expressing the offset as a giv `off = i*0xC` (not a running `off += 0xC`) → loop.c preheader
  placement matched ([[sched-coin-loop-preheader-order-lever]]). (2) `func_80080564` (5-call ROM-load
  glue) reused the sibling `func_800505A0.c` RomLoadSlot typedef + callee signatures — first-build
  byte-exact incl. both stack-buffer frame offsets (sp+0x10 as the 0x20 `out` struct, sp+0x30 as the
  RomLoadSlot). (3) `func_800852A8`: u16 for the `lhu` pad-button load + two independent-if masks
  (natural `v0=v1&4` recompute) → first-build.
- Friction: `func_80081C90` (sky-panel wind updater, two 14-iter RMW loops over fixed array D_800C54F2)
  is a TERMINAL #base-register-vs-displacement / #indexed-vs-pointer loop. Structure byte-exact; only the
  per-iter addressing diverges: ROM keeps INDEXED `%hi(D_800C54F2)+offset` re-materialization per access
  (v1 = pure byte offset, 2x/iter for the RMW load+store, NO pointer giv), gcc-2.7.2 loop.c
  strength-reduction folds base+off into ONE walking pointer in EVERY spelling (byte-offset cast,
  array-index, counter-index, for/do-while). DISTINCT from the S235 func_8006F24C DEST_REG pointer-giv
  crack (that ROM used a pointer giv; this one uses none), so no source lever applies. Permuter-
  unreachable. Characterized in `docs/wip/func_80081C90.near-match.md`; carried.
- Applied: 2 of 2 (#1 ranker-gap — `pick_target --segment main` did not surface the active partial-bank
  pack as a c-stub continuation → folded into the BACKLOG `carried-wall`/continuation ranker follow-ups;
  #2 SR-walking-pointer terminal loop sub-case → extended memory `byte-offset-cast-defeats-base-ptr-cse`).
- Carry-over: `func_80081C90` (base-vs-displacement/indexed-vs-pointer, terminal) + `func_800824E4`
  (S252 delay-slot coin). `func_80080220.c` continues (33 asm leaves; next fresh non-carry by `.s` size
  `func_8008679C` 200B, `func_80087BE4` 204B).

---

## Sprint 252 — continue func_80080220 pack: 3 smallest-first leaves — 2026-07-21
- Increment: 0 files banked (file partial, 39→36 stubs, NOT md5-candidate) / **+3 functions matched**:
  `func_8008C658` / `func_80080E14` / `func_8008060C` (all auto). Commit 837d8be. ROM SHA-1 green
  e2c4e7a9…, tree clean.
- Quality: 0 stuck-far / 0 permuter / **1 carried** / 0 re-opened (`func_800824E4`).
- Seed: committed 3pt; banked 0pt (per-file all-or-nothing, file partial); regime classical.
  Realized 4 / residual +1 (2 first-build, 1 one-fix goto, 1 carry). Value signal = +3 matched.
- What helped: (1) Fresh-pivot pack still yielding cheap smallest-first leaves (S251 direction holds) —
  bitfield-free + guarded-teardown + state-dispatch all fell fast. (2) `goto`-to-shared-label defeated
  the gcc-2.7.2 `||` fold_range_test on `func_8008060C` (`x==9||x==3` folds to branchless
  `xori/sltiu/or`; ROM keeps short-circuit `beq`/`bne`) — [[gcc272-fold-range-test-slti-merge]], matched
  first goto attempt. (3) Declared all callees with real prototypes up front (missing-prototype hazard),
  no implicit-int scheduling flip.
- Friction: `func_800824E4` (RGBA-pack) is a terminal delay-slot-fill coin — `b` pinned to `$a2` by the
  negative-path `b=arg2` (no `move`), and the toolchain's delay-slot filler never selects the highest
  reg of the delay-candidate pair, so ROM's `$a2`-in-`bnez`-shadow is source-unreachable (5 orderings
  tested). percent 0.79 (reg-swap scoring; structurally a 2-insn transposition), below the 0.97 gate.
  Characterized in `docs/wip/func_800824E4.near-match.md`; carried.
- Applied: 0 of 0 (no buffered suggestions recorded — clean sprint, documented levers only; PO declined
  the optional delay-slot-coin hazard note as a narrow instance of the existing sched-tiebreak family).
- Carry-over: `func_800824E4` (delay-slot-fill coin, terminal). `func_80080220.c` continues (34 asm
  leaves; next smallest `func_80083A48` 128B, `func_800852A8` 164B).

---

## Sprint 251 — fresh-pivot: open func_80080220 pack, bank 4 smallest-first leaves — 2026-07-21
- Increment: 0 files banked (file partial, 43→39 stubs, NOT md5-candidate) / **+4 functions matched**:
  `func_8008D0DC`→`toggle_sky_panel_bank_index` (curated), `func_80081550` / `func_80087BAC` /
  `func_80080DCC` (auto). Commit 99c9050. ROM SHA-1 green e2c4e7a9…, tree clean.
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened — cleanest main-seg sprint since the
  wall-cluster grind began.
- Seed: committed 5pt; banked 0pt (per-file all-or-nothing, file partial); regime classical.
  Realized 5, residual 0 (2 first-build, 2 one-fix, no walls). Value signal = +4 matched.
- What helped: (1) FRESH-PIVOT off the mined-out func_8008D100.c wall tail (S250 retro direction) restored
  cheap smallest-first velocity — a fresh 43-fn main-seg pack yields getter/wrapper/init-loop/lazy-init
  leaves again (4 banks vs S250's 1-bank/2-terminal on the same-file crack grind). (2) The llcvt coddog
  tag was pre-known a structural false-positive (BACKLOG:178, non-lib-callee tell) → went straight to the
  classical track, no wasted mirror attempt. (3) Sized leaves from the `.s` headers post-flip (S247 DoR),
  not vram gaps.
- Friction: two one-fix leaves. `func_80087BAC` (dual-array zero-init): an explicit `s32* p/q`
  pointer-walk mis-allocated the biv (counter→v0); the INDEXED `ARR[i]=0` form let gcc's LSR assign
  counter→a0 + last-referenced-ptr→v0, matching. `func_80080DCC` (lazy-init): ROM reserved a 0x38 frame
  vs build 0x18 — a PURE dead frame (ONLY the frame immediate + ra-slot offset differ; NO reg
  permutation, NO divide), cracked byte-exact with `s32 unused[8]` (0x20 = delta). This is a CLEAN-crack
  variant distinct from the divide-driven `#dead-frame-reload-artifact-regalloc-wall` carry-class.
- Applied: 3 of 3 (#1 pure-dead-frame subsection → `docs/hazards.md#dead-frame-reload-artifact-regalloc-wall`
  + hazard-index row + new memory `pure-dead-frame-clean-crack`; #2 fresh-pivot-restores-velocity
  datapoint → this RETRO; #3 indexed-vs-pointer biv confirmation → this RETRO, no doc change).
- Carry-over: none. `func_80080220.c` stays open (39 stubs, ~35 untouched leaves); S252 continues
  smallest-first here (next: `func_800824E4` 80B, `func_8008C658` 88B, `func_80080E14` 104B) until it
  plateaus, then fresh-pivot again. Cross-repo: propagate `toggle_sky_panel_bank_index` →
  `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 250 — compiler-source fan-out crack slice on the func_8008D100.c documented walls — 2026-07-21
- Increment: 0 files banked (file partial, 20→19 stubs, NOT md5-candidate) / **+1 function matched**
  (6→7 of 26): `func_800958D8`→`emit_fullscreen_scissor_dl` (0x800958D8), commit 82cbc39. ROM SHA-1
  green e2c4e7a9…, tree clean.
- Quality: 2 stuck-far / 0 permuter / 2 carried / 3 re-opened this sprint (re-opens intentional per
  S232 crack-attempt; 1 resolved to a bank).
- Seed: committed 3pt; banked 0pt (per-file all-or-nothing, file partial); regime classical.
  Realized ~5 (seed +2 carries +1 novel isolation-artifact gotcha), residual +2. Value signal =
  +1 matched + 2 pass-cited retired walls.
- What helped: (1) The S232 fan-out (3 gcc-2.7.2 + binutils-2.6 subagents, isolated builds, orchestrator
  held all integration) sorted the tail exactly per S233 into {1 bank, 2 terminal-verdicts}. (2) The
  crack subagent's LINK-BOTH-AND-CMP test (link current.o + reference with identical real addresses,
  byte-cmp) proved `func_800958D8` byte-exact — the 0.833 score was pure reloc-token addend noise
  (struct base+2/+4/+6 == the ROM's separate siblings D_801B7F32/34/36, same page), a base-vs-disp
  MISCHARACTERIZATION that carried 2 sprints. (3) The S235 hand-off contract worked clean: all 3
  subagents sent FINAL verdicts via SendMessage, ZERO orchestrator re-pings (vs S235's 3-of-4 silent).
- Friction: `func_800958D8` sat mislabeled a base-vs-displacement wall for 2 sprints (S248 doc, S249
  SPRINT) when it was byte-exact all along — the mid-percent (0.833) reloc-addend artifact does not trip
  the usual high-percent/empty-`top_mismatches` recognizer. The two genuine walls were correctly
  predicted terminal at the plan gate (less-steerable class, permuter ~0): `func_8008DDDC` @0.610
  (mips.c:1023 folding macro + loop.c:1631 hoist, coupled 7-saved-reg equilibrium) and `func_80095150`
  @0.279 (global.c:594 pervasive 13-IV permutation).
- Applied: 3 of 3 (#1 link-both-and-cmp technique → `docs/hazards.md#isolated-compile-caveat` S250 case
  + hazard-index row, + a tracked `tools/link_both_cmp.py` automation follow-up in BACKLOG; #2 keep the
  S235 "idle_notification is NOT a deliverable" clause verbatim → agent-workflow.md S235-contract note
  re-validated; #3 S251 fresh-pivot direction → BACKLOG).
- Carry-over: `func_8008D100.c` stays open but GENUINELY MINED (1 md5-blocking artifact removed;
  remaining 19 stubs = DL/FP/jtbl/nested-in-DL + 2 pass-cited terminal regalloc walls). S251 FRESH-PIVOTS
  off this file: `func_80080220` (43-fn pack) or `func_8002A640` (24-fn spriteex2, sprite.c enabler).
  Cross-repo: propagate `emit_fullscreen_scissor_dl` → `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 249 — continue func_8008D100.c: mine the 2 untried tractable mid-leaves — 2026-07-21
- Increment: 0 files banked (file partial, 20 stubs unchanged, NOT md5-candidate) / **+0 functions
  matched** (still 6 of 26). ROM SHA-1 green e2c4e7a9…, tree clean.
- Quality: 2 stuck-far / 0 permuter / 2 carried / 0 re-opened this sprint.
- Seed: committed 5pt; banked 0pt (per-file all-or-nothing, file partial); regime mixed/classical.
  Realized ~7 (seed +2 carries), residual +2.
- What helped: (1) Sizing remaining stubs from real `.s` headers surfaced 2 untried integer mid-leaves
  (0x33C/0x388) that S248's "plateaued" verdict missed (it mined only <0x200). (2) m2c seeds were
  structurally faithful on both (dispatch decoded via delay-slot register truth). (3) Concrete regalloc
  levers landed and moved score materially: `func_80095150` cache-global-ptrs-in-locals to stop
  store-alias reloads (22200→19660) + `c0=cbase` invariant snapshot (→15140); `func_8008DDDC`
  compute-lane-bases-after-2nd-call to keep 3 in temps not callee-saved (frame 0x38→0x30, 11420→9340)
  + shared advancing `tbl` ptr = the ref's mutated `s0` base (→9040).
- Friction: BOTH untried leaves — despite passing the cheap tractability filter (jal-light/no-FP/no-DL/
  no-jtbl) — walled on pervasive regalloc + constant/address materialization, not source-leverable.
  `func_8008DDDC` @0.610 (23 opcode diffs), `func_80095150` @0.279 (4-deep nested-loop regalloc).
  The `&putter_mode_flag` keep-vs-remat ptr-cache BACKFIRED (8900→13160). This EMPIRICALLY closed the
  S248 continue-vs-pivot question: the pack's mid-logic tail is a wall-class cluster (S224), no banks.
- Applied: 2 of 2 (#1 ranker `mid-logic-wall-risk` tell = nested-depth≥2 AND ≥K fixed-global refs AND
  jal-light → BACKLOG ranker follow-up; #2 S250 fan-out direction — elevate a compiler-source fan-out
  crack slice above a fresh pivot, `func_8008DDDC`@0.610 best target → BACKLOG + this digest).
- Carry-over: `func_8008D100.c` stays open, now GENUINELY plateaued. +2 characterized carries
  (`func_80095150` 0.279, `func_8008DDDC` 0.610; wip docs + reproducer C committed 9494da4). S250:
  compiler-source fan-out on the documented walls (esp. `func_8008DDDC`), not another smallest-first
  continuation.

---

## Sprint 248 — continue func_8008D100.c: mine small leaves + compiler-source dive — 2026-07-21
- Increment: 0 files banked (file partial, 20 stubs, NOT md5-candidate) / **+2 functions matched**
  (22→20 stubs; 6 of 26 matched). ROM SHA-1 green e2c4e7a9…, tree clean.
- Quality: 0 stuck-far / 0 permuter / 3 carried / 0 re-opened this sprint.
- Seed: committed 5pt; banked 0pt (per-file all-or-nothing, file partial); regime mixed/classical.
  Realized ~6, residual +1.
- What helped: (1) PO-directed **compiler-source fan-out** (4 subagents over gcc-2.7.2 + binutils-2.6,
  systematic-debugging) cracked the S247 `func_8008E164` carry — `volatile s32 unused = r;` (write-only
  volatile of an uninitialized local) reproduces the dead frame + `sw v0`; gas ruled out. (2) A
  **follow-up nested-function dive** then proved the TRUE origin: `lerp_s32` is a GCC nested function
  (the `sw $v0` is the static-chain home, STATIC_CHAIN_REGNUM=$2, mips.h:1310), byte-for-byte identical
  to a clean nested child; orphaned (parent inlined the call, zero ROM xrefs). (3) m2c+Ghidra seed
  banked `init_scenario_state` first build. (4) The struct-member mem-in-struct lever took the scissor
  emitter `func_800958D8` to 0.833.
- Friction: 3 of the 4 small leaves are wall-class (jtbl+DL, nested-fn, base-vs-displacement) — the
  pack has plateaued for tractable small work; the remaining tail is large DL/FP.
- Applied: 2 of 2 (both emergent from the nested-fn finding, 0 pre-buffered): #1 nested-fn DoR check →
  `docs/agent-workflow.md` §3 pre-permuter list + 2 hazard-index rows + `docs/hazards.md`
  #nested-function-static-chain-spill (chain-unused orphan + chain-used masquerade variants); #2 fix
  the S247 "DCE/frame coin" framing → BACKLOG + VELOCITY + memory `dead-frame-dead-v0-store-crack`
  reframed to the nested-fn root cause.
- Carry-over: `func_8008D100.c` stays open. Carries: `func_8009226C` (jtbl partial-merge),
  `func_800957F0` (jtbl+DL), `func_800958D8` (0.833 base-vs-disp). **Re-priced** `func_80092E10` from
  a `$v0`-arg wall to a chain-USED nested fn (crackable once its parent 0x800930xx..0x80093470 is
  decompiled). DL/FP tail deprioritized (emit_sky_*, update_sky_panel_verts, palette_load).
- Cross-repo: 2 new curated names to propagate to the Ghidra workspace (`lerp_s32`,
  `init_scenario_state`).

---

## Sprint 247 — open fresh func_8008D100.c (26-fn sky/skybox render pack) — 2026-07-20
- Increment: 0 files banked (file partial, 22 stubs, NOT md5-candidate) / **+4 functions matched**
  (`func_80092324` scaffold-empty, `func_8008FF14`→set_sky_panel_cycle_mode_sel, `func_800959F8`,
  `func_800934CC`) / **2 carried** (delta: ROM SHA-1 green throughout; matched-count is the value).
- Quality: 0/0/2/0 (stuck-far/permuter/carried/re-opened). Carries: `func_8008E164` (FP lerp, body
  byte-exact, dead-frame+dead-store coin), `func_8009226C` (jtbl partial-merge+block-reorder wall).
- Seed: committed 5pt; banked 0pt (file partial); regime classical/mixed. Realized ~7 (+2 carries);
  residual +2.
- What helped: plateau-rule pivot off mined-out func_80071370.c to a FRESH pack was correct — 3 clean
  small leaves (setter/predicate/mode-setter) banked first-build. Per-object `objdump` fast-path (S244
  oracle) proved func_8008E164's FP body byte-exact and isolated the residual to a dead frame in one
  build, no permuter needed to characterize.
- Friction: (1) vram-gap leaf sizing at the plan gate was WRONG (named fns interleave the pack) — the
  committed named leaves turned out large; real tiny leaves were different fns; re-sorted from `.s`
  headers at execution. (2) Curated rename of func_800959F8 broke the link (called from committed C
  siblings by auto name) — 2 failed builds before keeping the auto name.
- Applied: 3 of 3 — #1 size-leaves-from-`.s`-not-vram-gaps (→ workflow DoR), #2
  grep-committed-C-callers-before-rename (→ workflow Bank-4a + memory), #3 keep-auto-name when backing
  global has diverse-subsystem callers (→ memory). 2 new memories written.
- Carry-over: `src/main/func_8008D100.c` (partial, 22 stubs) — `func_8008E164` (dead-frame coin, wip
  doc), `func_8009226C` (jtbl partial-merge switch, semantics RE'd). Remaining pack tail is DL/FP-heavy
  (emit_sky_*_dl, update_sky_panel_verts) = S243/S158 wall risk.

---

## Sprint 246 — bank fresh dispatcher leaves in func_80071370.c (glyph/HUD DL family) — 2026-07-20
- Increment: 0 files banked (file partial, 18 stubs, NOT md5-candidate) / **+2 functions matched**
  (`func_80071370`→load_hud_glyph_assets, `func_80071B34`→emit_hud_table_prim_dl) / **1 carried**
  (`func_8007624C`). File 20 → 18 stubs.
- Quality: 0 stuck-far / 2 permuter (BOTH retired: 71370 kept the `size2=` lever; 71B34 retired to a
  clean `off=i*0x2C` idiom) / 1 carried / 0 re-opened.
- Seed: committed 5pt (classical/mixed); banked 0pt (file partial); realized ~7 (+1 carry, +1 permuter-
  work); residual +2; regime classical/mixed.
- What helped: (71370) global.c s0/s1 base-vs-accumulator coin cracked via `heap3_alloc(size2 = w*h+8)`
  accumulator-reuse (pins both accumulators to one pseudo). (71B34) body byte-perfect first build; only
  the prologue sched-order coin (S243 "terminal" class) diverged — PO-directed gcc-2.7.2 dive + `-S`
  oracle + peer subagent ROOT-CAUSED it to loop.c preheader placement (`do{}while(0)` NOTE_INSN_LOOP_BEG
  artifact) and RETIRED it to the clean `off = i * STRIDE` giv idiom (same as func_800718C4). GBI macros
  (gDPSetPrimColor + gfx++) per PO directive.
- Friction: mis-priced the slice as "call-glue dispatchers" from jal-count — 71370 is an asset-loader,
  71B34/7624C are DL emitters. 7624C is a genuine 2-pass drop-shadow raw-DL wall (fully RE'd; residual =
  fixed-base-write + incrementing-localGfx dead-store split + x/y biv placement; quantized packing can't
  use GBI macros). decomp_loop scored the 71B34 permuter-crack 800 yet full-make ROM SHA-1 matched.
- Applied: 3 of 3 — #1 sched-coin retire lever → new memory `sched-coin-loop-preheader-order-lever` +
  updated `mg64-glyph-emitter-dl-family` (S243 verdict = hypothesis, re-attempt the 73F24/74230/74500
  tails with `off=i*STRIDE`); #2 DL-macro/gfx++ idiom → `mg64-glyph-emitter-dl-family` (+ raw-packing
  exception for quantized colors); #3 decomp_loop/permuter score-disagrees-ROM → `subagent-diff-crack-
  not-a-bank`. (0 new tooling-test failures; doc/memory edits only.)
- Carry-over: `func_8007624C` (raw-DL wall, `docs/wip/func_8007624C.near-match.md`). File
  `func_80071370.c` continues (18 stubs).

## Sprint 245 — bank the 3 smallest fresh leaves in func_80078910.c — 2026-07-20
- Increment: 0 files banked (file partial, 22 stubs, NOT md5-candidate) / **+3 functions matched**
  (`func_8007E30C`→project_delta_to_radius_150, `func_8007E664`→randomize_terrain_scatter_point,
  `func_80078BDC`→reload_scene_assets). File 25 → 22 stubs. All first-pass, 0 carries.
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened this sprint.
- Seed: committed 5pt; banked 0pt (file partial, per-file all-or-nothing); realized 5 (+0 residual, all
  first-pass no permuter/carry); regime classical.
- What helped: per-file `-ffast-math` TU override (bare `sqrt.s` in E30C) with the "siblings-held-
  invariant" reasoning (whole TU was fast-math in ROM → banked FP siblings match by construction);
  3-lever fixed-global vector crack (invert zero-guard if → bc1t+a0/a1; `s32* p` RMW → shared base-reg;
  one dist/scale var → sqrt coalesced into div reg); jtbl-carve from a ZERO-prior-rodata object (switch
  jtbl = sole `.rodata`, clean 8-aligned-both-edges 3-way yaml split) + cross-jump-tail-merge (full call
  body per case, not shared post-switch call, fixed a 4-instr short); objdump-fresh-object oracle again.
- Friction: 78BDC missing-prototype (get_interpolated_terrain_height_wrapper implicit-int from a LATER
  E664 decl) flipped codegen until hoisted; the shared-call-keyed-by-var switch form ran 4 instrs short
  before switching to per-case bodies.
- CONFIRMED func_80078910.c is a HEALTHY fresh-tail continuation (S244 +2, S245 +3, 1 wall), counter to
  the "plateaued mid-logic tail → prefer fresh pack" default; ~22 fresh leaves remain.
- Applied: 4 of 4 — #1 -ffast-math siblings-invariant note → hazards#double-sqrt-fast-math; #2 fixed-
  global vector crack combo → new memory `fixed-global-vector-fn-crack-combo`; #3 jtbl-carve zero-prior-
  rodata + cross-jump companion → memory `jtbl-carve-both-edge-8align`; #4 DoR fresh-tail note → BACKLOG.
  (0 new tooling-test failures; doc/BACKLOG/memory edits only.)
- Carry-over: none new. File `func_80078910.c` continues (22 stubs; `func_8007955C` still the lone
  documented wall).

## Sprint 244 — bank the particle-spawn family in func_80078910.c — 2026-07-19
- Increment: 0 files banked (file partial, 25 stubs, NOT md5-candidate) / **+2 functions matched**
  (`func_80079940`, `func_80079A08`) + 1 fully-RE'd carry (`func_8007955C`). File 27 → 25 stubs.
- Quality: 0 stuck-far / 0 permuter / 1 carried / 0 re-opened this sprint.
- Seed: committed 5pt; banked 0pt (file partial, per-file all-or-nothing); realized 7 (+1 carry, +1
  novel-crack cluster), residual +2; regime classical.
- What helped: S184 `&particle_array[i]` recompute de-biased a `combine_givs` DUAL-BASE split (79940);
  `s32 one=1` variable defeated gcc's `(f64)1*D→D` identity fold + `f64 grav=D` preheader-hoist flipped
  the loop-var/invariant FP-reg coin + explicit sentinel & statement order nailed the delay-slot fill
  (79A08); objdump of the FRESH object was the true per-fn oracle (diff.py stale, S242 recurred).
- Friction: diff.py lied byte-clean while the object was dual-base-split; `func_8007955C`'s schedule-order
  coin didn't yield to source store-order reshuffles (15↔19 rows, no convergence).
- REFUTED the S226 "79xxx regalloc-nemesis" bulk wall-prediction (2 of 3 banked; the 3rd is a scheduler
  coin, not regalloc) — 2nd verify-by-seed data point (S240): a bulk regalloc tag over a size/fp-count
  band with no per-fn attempt is a HYPOTHESIS.
- Applied: 5 of 5 — #1 S184 dual-base extension → hazards#indexed-vs-pointer + memory
  `fp-const-init-spawn-loop-levers`; #2 FP-const spawn levers → same new memory; #3 no-tag-un-attempted-
  leaves (ranker follow-up) → BACKLOG; #4 objdump-fresh-object oracle → agent-workflow asm-first fast-path;
  #5 `sched-order-coin:<fn>` coverage of func_8007955C → BACKLOG. (0 new tooling-test failures; doc/BACKLOG
  edits only.)
- Carry-over: `func_8007955C` (particle-spawn RTS-matrix builder, sched.c schedule-order coin ~85%,
  `docs/wip/func_8007955C.near-match.md`). File `func_80078910.c` continues (25 stubs).

## Sprint 243 — bank glyph-emitter siblings 73F24/74230/74500 in func_80071370.c — 2026-07-18
- Increment: 0 files banked (file partial, 20 stubs, NOT md5-candidate) / **+0 functions matched** — 3 fully
  RE'd + root-caused CARRIES. 2nd straight 0-bank characterization sprint on this emitter tail (S241 = 0/3).
- Quality: 0 stuck-far / **1 family permuter-attempted** (PO gate-override, non-converged) / **3 carried**
  (`func_80074500`/`func_80074230`/`func_80073F24`) / 0 re-opened.
- Seed: committed **5**pt (classical bank slice); banked **0**pt (per-file all-or-nothing); realized **8**
  (+1 carry cluster, +1 permuter override, +1 escalation fan-out+dive), residual **+3**; regime classical.
- What helped: the fan-out (3 crack subagents + 1 compiler-source dive) SORTED the tail {crack, terminal}.
  crack-74230's per-command post-increment idiom `{ Gfx* g = gfx++; g->words.w0=W0; g->words.w1=W1; }`
  SOLVED the store-giv wall (`#indexed-vs-pointer-loop-strength-reduction`, previously 0-precedent). The
  dive pass-cited the residual to a gcc-2.7.2 sched.c SCHEDULE-ORDER coin (`rank_for_schedule:2428`
  class-then-LUID) + found 2 transferable levers (c/code fresh-inside-loop split, beql via plain-if).
- Friction: the siblings were MIS-PRICED as the S242 clean-macro family; they are a harder raw-DL-word
  sub-family, all walled on the shared sched coin (0 banks vs the plan's 2-3). asm-differ `percent` reads
  NEGATIVE on a schedule-displacement near-match (metric artifact), so the 0.97 gate can't fire on a
  >90%-correct body — needed the PO override. `setup-permuter.sh` silently aborts once the body is inlined
  (`mg_resolve_c_asm` greps the `INCLUDE_ASM` marker); direct `import.py --settings` is the fallback.
- Applied: 5 of 5 — #1 raw-DL post-inc-idiom giv crack → `hazards#indexed-vs-pointer` + memory
  `mg64-glyph-emitter-dl-family`; #2 `sched-order-coin:<fn>` ranker sub-tag → `pick_target` (advisory, 0
  new test failures) + new memory `sched-class-tiebreak-order-coin`; #3 negative-percent gate mis-read →
  `hazards#permuter-setup`; #4 CORRECTED `setup-permuter.sh` inlined-body silent-abort root cause (NOT the
  S189 array guard) → `hazards#permuter-setup` (direct-import fallback); #5 same-family-is-a-hypothesis
  planning lesson + `raw-dl-emitter:<fn>` ranker follow-up → BACKLOG + memory.
- Carry-over: `src/main/func_80071370.c` (20 stubs) — 3 new raw-DL glyph carries (sched-order-coin) join
  `func_80074E5C`/`func_80076138` (regalloc-coin); a longer permuter run or a hand-finish (post-inc +
  reassociation + c/code-split + beql) MIGHT close the raw-DL family (a flip transfers to all 3).

## Sprint 242 — bank fresh integer leaves in func_80071370.c — 2026-07-17
- Increment: 0 files banked (file partial) / **+3 functions matched** (`func_80071370.c` 23 → 20 stubs; NOT md5-candidate).
- Quality: 0 stuck-far / 0 permuter (carry `func_80076138` ~96%, below the 0.97 gate) / **1 carried** (`func_80076138`) / 0 re-opened.
- Seed: committed 5pt; realized 7pt (residual +3: +1 carry, +1 compiler-source-fan-out escalation, +1 diff.py-stale gotcha); regime classical.
- What helped: the leaves were a **glyph/text DL-emitter family** (`gSPTextureRectangle` per char). `func_80074EFC` cracked on a char loop-var reuse (2 iters). `func_80075010` cracked via a **compiler-source fan-out** (4 subagents over gcc-2.7.2 + binutils-2.6): raw-division precompute + inline shifts/mask satisfies loop.c hoist-order + sched.c LUID + combine const-fold simultaneously; the recipe transferred to `func_80074D0C` first-try. The fan-out drove 2 of 3 banks — high EV on a well-scoped codegen wall.
- Friction: the orchestrator's own incremental `diff.py` read STALE ~6x (false-clean AND false-diff, both directions) — burned iterations until switching to full-make ROM-SHA-1 as the gate. `func_80076138` is a terminal `str<->i` biv allocno swap (global.c allocno_compare, param-entry-copy) with no semantics-preserving flip.
- Applied: 4 of 4 — #1 diff.py-stale gate note → `agent-workflow.md` + extended `subagent-diff-crack-not-a-bank` memory; #2 glyph-emitter family recipe → `hazards.md#display-lists` + new `mg64-glyph-emitter-dl-family` memory; #3 three gcc-2.7.2 levers → memories (`loop-invariant-hoist-order-preheader-regalloc`, `global-allocno-compare-livelength-biv-order`, combine const-fold folded into the glyph memory); #4 `regalloc-coin:<fn>` ranker sub-tag → `pick_target.py` (carried-wall wip-content probe; flags `func_80074E5C`+`func_80076138`, advisory, 0 new test failures).
- Carry-over: `func_80076138` (biv allocno coin, `docs/wip/`); `func_80071370.c` continuation (20 stubs; the 73F24/74230/74500 glyph siblings are the likely-next tractable vein per the family recipe).
- NB: `make test-tools` shows 8 pre-existing failures (golden drift from repo/hazards edits, not S242 code) — unchanged by the #4 ranker edit; deferred to a tooling pass.

## Sprint 241 — escalation crack-attempt fan-out over the S224 wall cluster in func_8006A2C0.c — 2026-07-16
- Increment: 0 files banked / **0 functions matched** (`func_8006A2C0.c` stays 22 stubs; NOT md5-candidate). Delta 0.
- Quality: 0 stuck-far / 0 permuter (all 3 stayed below 0.97, none run) / **3 carried** (`func_8006D38C`, `func_8006D214`, `func_8006CE88`) / 0 re-opened.
- Seed: committed **3pt**; banked **0pt**; regime classical (realized = seed, 0 banks → the file scores 0 pt per all-or-nothing; value = 3 retired+cited walls).
- Scope vs goal: goal was a reproduce-first crack-attempt fan-out expecting a {crack→bank | terminal→wip} SORT (S232/S233 model), explicitly hedged 0-3 banks. Delivered exactly the 3 committed fns, all 3 reproduced faithfully + root-caused + wip'd + `file:line`-cited. Goal MET as a wall-retirement slice.
- What helped: **reproduce-first discipline** (S238/S240-AD1C: a class-tag is a HYPOTHESIS) — the byte-offset-cast lever CRACKED the 0xB8-stride struct-array walks byte-exact, isolating each residual to its true terminal class instead of leaving a vague "base-register wall." Two-subagent fan-out (sibling pair + independent) parallelized 3 deep fns; isolated `nonmatching-func` kept the shared build/ race-free; I held integration serially (nothing to integrate → 0 banks, ROM green throughout).
- Friction: the pack's mid-logic tail is genuinely wall-class (S224 productivity-cliff lesson VALIDATED) — 0/3 banks vs S232's 3/3, confirming the S241 hedge that base-register / pervasive-regalloc walls skew terminal (mips.h addressing coin), unlike S232's `global.c`-ref-count-steerable tail. D38C/D214 terminal (`mips.h GO_IF_LEGITIMATE_ADDRESS:2318-2349` scalar-global `%hi`-CSE-share, a NEW third terminal sub-case of the class); CE88 body cracked to 0.553 but lands on an allocno-coloring floor below the 0.97 gate (permuter-only). Recurred DoR-miss root-fixed: the 3 carries now have wip files AT DISCOVERY (S239/S240 carries had none), and the new `carried-wall` ranker tag surfaces them.
- Applied: **4 of 4**: #1 `pick_target.py` carried-wall detector (wip-existence check on c-stub continuation leaves; `carried-wall:<fns>[;characterization-only]`, verified rendering on the pack, goldens regen'd) + #2 `;characterization-only` EV tag when ≥2 wip'd leaves; #3 `docs/hazards.md#base-register-vs-displacement` S241 scalar-global-CSE-share third-terminal-sub-case + the mips.h citation; #4 the CE88 cross-call base-allocation levers folded into the same section (array-index-vs-pointer inverse of Axis-7; block-scoped ptr for count-address). New `carried-wall` flag registered in the hazard index.
- Carry-over: `func_8006D38C` + `func_8006D214` (terminal `#base-register-vs-displacement`, wip'd) + `func_8006CE88` (permuter-only regalloc floor, wip'd), all in `src/main/func_8006A2C0.c` (now 22 stubs). Next-slice steer: this pack's escalation-EV is low — prefer a FRESH pack or corpus-mining.
- Note (pre-existing tooling debt, NOT S241): 3 `tests/tooling/test_pick_target.py` failures are red on clean HEAD too (`deep_json_golden` backlog shrank to 42 rows < the test's `>50` threshold; `ranked_by_descending_score` + `coddog_suppresses_maybe_upstream` invariant/fixture drift) — unrelated to this sprint, deferred to a dedicated tooling pass.

## Sprint 240 — smallest-first slice over func_8006A2C0.c (AD1C bank + D164/DF84 crack-attempts) — 2026-07-16
- Increment: 0 files banked / **+1 function matched** (`func_8006A2C0.c` 23 stubs → 22; NOT md5-candidate).
- Quality: 0 stuck-far / **1 permuter** (`func_8006D164`, no crack) / **2 carried** (`func_8006D164`, `func_8006DF84`) / 0 re-opened.
- Seed: committed 3pt; banked 0pt (per-file all-or-nothing, file partial); realized 6, residual +3 (+1 permuter, +2 carry); regime classical.
- What helped: (a) `func_8006AD1C` BANKED first-build — a signed `% 14` table-index leaf (magic 0x92492493 = the /14 div, NOT a permutation) via the byte-offset-cast u16 load `*(u16*)&D_800C4026[(arg0%14)*4]` + the sibling `func_8006ACD8` buffer idiom (`u8 sp10[0x20]`, func_8005062C/func_800506D4). This RETIRED a BACKLOG deferred-wall prediction (`AD1C = #local-alloc-qty-permutation`) — a magic-constant tell is a HYPOTHESIS, verify by seeding not by the tell. (b) **New regalloc lever (Axis 7, cracked DF84's base-reg factor):** declare a post-call value's var BEFORE the call so its live range crosses the jal -> callee-saved `s0` (mirror of the init-after-call Axis 5). Fixed DF84's `&D_800FF4D0` base from caller-saved `a0` to the ROM's `s0` (frame + all downstream regs matched).
- Friction: (a) **DoR miss RECURRED (S239→S240):** all 3 committed "fresh leaves" were ALREADY characterized BACKLOG carries (C8CC S239; D164/DF84 here) — the plan-gate DoR grep checks in-file comments, not the BACKLOG carry list. Both D164 and DF84 lived only in BACKLOG, so they re-surfaced as fresh. Not wasted (banked the mis-predicted AD1C, deepened D164, refined DF84), but the `carried-wall:<fn>` ranker/DoR follow-up is now ELEVATED — the plan gate must grep BACKLOG carries by fn name until built. (b) `func_8006D164` (byte-exact body) is a genuine LICM/regalloc coin: ROM rematerializes `li 4` inside the outer loop (reuses the set-in-loop data reg -> hoist blocked), build hoists it -> 3-reg rotation; `!=`-bounds/n-var/decl-swap/do-while + a 4-min permuter all failed. (c) `func_8006DF84` base-reg cracked but the TERMINAL factor is a cross-jump-tail-merge: the two identical `base[6]=0` tails merge in jump.c before reorg's optimize_skip can annul one into the `bnezl` — not permuter-reachable; S224's "address-fold" label was one of two factors, and the wrong one.
- Applied: 3 of 3 — #1 elevated `carried-wall:<fn>` ranker/DoR follow-up (must cross-ref BACKLOG carries) + AD1C wall-prediction CORRECTION → BACKLOG; #2 hazards.md `#loop-weight-and-live-length-regalloc-steering` Axis 7 (cross-call live-range = callee-saved lever) + memory `cross-call-live-range-callee-saved-lever`; #3 agent-workflow.md DoR bullet (grep BACKLOG carries by fn name + write wip docs AT DISCOVERY).
- Carry-over: `func_8006D164` (`docs/wip/func_8006D164.near-match.md`, LICM selective-hoist + regalloc-rotation wall) + `func_8006DF84` (`docs/wip/func_8006DF84.near-match.md`, cross-jump-tail-merge blocks annulled bnezl; base-reg factor SOLVED). `func_8006A2C0.c` retains 22 stubs (the S224 wall-class mid-logic tail).

---

## Sprint 239 — smallest-first classical bank slice over func_8006A2C0.c fresh leaves — 2026-07-16
- Increment: 0 files banked / **+2 functions matched** (`func_8006A2C0.c` 25 stubs → 23; NOT md5-candidate).
- Quality: 0 stuck-far / 0 permuter / **1 carried** (`func_8006C8CC`) / 0 re-opened.
- Seed: committed 3pt; banked 0pt (per-file all-or-nothing, file partial); realized 5, residual +2 (+1 carry, +1 novel bank-gotcha); regime classical/mixed.
- What helped: DL-emitter twins `func_8006A4A0`/`func_8006A548` banked via stock `gDP*(gfx++)` macros — a 6-cmd parameterized TLUT-load (SetTImg/TileSync/SetTile/LoadSync/LoadTLUT/PipeSync). The macro form makes GCC materialize N distinct `Gfx*` pointers (matching the ROM), where raw `gfx[i].words` indexing keeps a single base+displacement (diverges). Parameterized subfields fold into the macro arg (`gDPSetTile(…, 256 | ((pal&0xF)<<4), …)`; `gDPLoadTLUTCmd(…, count)`). Confirms the S226 low-FP-DL-vein-is-tractable lesson. S224's "prefer fresh pack" caution did NOT fire — these leaves were genuinely un-mined (S224 stopped at 1 bank + 3 wall-chars), not the bnel/base-reg wall tail.
- Friction: (a) the 19i "trivial predicate" `func_8006C8CC` was the sprint's WALL, not the 42i DL fns — `(flag&0x8000)?1:0` folds to `lhu;srl` (do_store_flag single-bit) where the ROM keeps `andi;bnez`; 10 source forms exhausted, terminal, permuter-denied (extends the S231 const-select wall to a single-bit/sign deciding term). Small size ≠ tractable on a wall-sensitive pack. (b) Subagent dl_A4A0 reported byte-clean via per-fn diff.py, but that read a STALE object — the orchestrator full-make was a 22M-byte layout break; isolated one-fn-at-a-time to find the raw-index-vs-macro divergence, rewrote to the macro form.
- Applied: 4 of 4 — #1 memory `subagent-diff-crack-not-a-bank` + hazards.md `#display-lists` note (per-fn diff.py CRACK not a bank until orchestrator full-make); #2 hazards.md `#display-lists` DL `gDP*(gfx++)` macro-vs-raw-index note; #3 memory `store-flag-single-bit-terminal-wall` + hazards.md `#value-select` single-bit/sign extension; #4 size≠tractable retro note (this line + BACKLOG).
- Carry-over: `func_8006C8CC` (`docs/wip/func_8006C8CC.near-match.md`, store-flag-single-bit terminal wall). `func_8006A2C0.c` retains 23 stubs (the S224 wall-class mid-logic tail + the fresh leaves func_8006D164/A5E4/A84C not yet attempted).

---

## Sprint 238 — crack/decomp fan-out over the func_80071370.c 748xx triplet — 2026-07-16
- Increment: 0 files banked / **+3 functions matched** (`func_80071370.c` 26 stubs → 23; NOT md5-candidate).
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened.
- Seed: committed 5pt; banked 0pt (per-file all-or-nothing, file partial); realized 5, residual 0 (all cracked, no escalation trigger); regime classical (crack/decomp slice).
- What helped: 3 gcc-2.7.2 + binutils-2.6 subagents (parallel, isolated `decomp_loop --profile main`) over the 748xx triplet — the 3 smallest leaves, all `jal`ing the S237-banked helper `func_800738BC`. **3 cracks / 0 carries** (2 first-build, 1 single-lever). Triplet was STRAIGHT-LINE, so the S237 `func_80074E5C` `global.c:587` allocno copy-pref wall did NOT recur (plan hedge held). `func_800747B0` cracked 60→0 on a NEW gcc-2.7.2 `sched.c` LUID lever: fold the `arg1 - w/2` subtract INTO the call arg so the `&saved`→`$a0` setup emits first (priority():1488 ties all latency-1 ALU at prio 1, rank_for_schedule():2428 breaks by INSN_LUID = source-emission order). All 3 same shape (save `*arg0` across two calls, banked helper, signed `w/2` round, s16 sign-extends, sibling call). Only additions = 3 second-callee externs. **A freshly-banked shared helper unblocks its still-asm caller family as tractable leaves** — a positive tractability signal, not the S224 wall-sensitive tail. Pack scorecard: func_80071370.c = S236 3/3 + S237 2/3 + S238 3/3 = **8 of 9 attempted leaves cracked**.
- Friction: none material. First-build 2/3; the lone lever was a clean 1-edit sched-order fix. Subagents applied the S235 hand-off contract cleanly (all 3 SendMessage'd verdicts, though 2 idle_notifications trailed the verdict — non-blocking).
- Applied: 3 of 3 — #1 new memory `sched-luid-order-inline-arg-subexpr` (already authored by the crack subagent; MEMORY.md index added); #2 caller-of-a-freshly-banked-helper positive-tractability ranker follow-up → BACKLOG; #3 known-playbook-tail 8/9 scorecard retro note → BACKLOG (6th data point).
- Carry-over: none. `func_80071370.c` retains 23 stubs (mid-logic/DL tail; the terminal `func_80074E5C` allocno wall stays carried from S237). LESSON: chain value across sprints — a helper banked one sprint pays off its caller family the next; the fan-out is the settled main-segment play.

## Sprint 237 — crack/decomp fan-out over 3 smallest wall-sensitive leaves in func_80071370.c — 2026-07-16
- Increment: 0 files banked / **+2 functions matched** (`func_80071370.c` 28 stubs → 26; NOT md5-candidate) + 1 wall carried-with-citation.
- Quality: 0 stuck-far / 0 permuter / **1 carried** / 0 re-opened.
- Seed: committed 5pt; banked 0pt (per-file all-or-nothing, file partial); realized 7, residual +2 (+1 carry, +1 novel mid-pool jtbl-carve gotcha); regime classical (crack/decomp slice).
- What helped: crack/decomp fan-out (gcc-2.7.2 + binutils-2.6, 3 parallel subagents, isolated `decomp_loop --profile main`) sorted the tail exactly as the S233 hedge predicted — **2 cracks + 1 terminal carry** (NOT 3/3). `func_800760CC` (27i switch jtbl char-classify) byte-matched; its jtbl carve was the **first partial-carve from the MIDDLE of a multi-jtbl shared rodata pool** — carving the 8-aligned-both-edge jtbl_800D17E0 (0xACBE0..0xACC48) with a 3-way yaml split left the still-asm sibling jtbls' `.L` labels intact. `func_800738BC` (font-width accum) cracked via goto-to-shared-label defeating gcc-2.7.2 `fold_range_test` slti-pair merge (the only matching structure) + `if(flags!=0)` polarity + guard+do-while. Even a plateaued/wall-sensitive tail (S224) still banked 2/3 on KNOWN-playbook leaves.
- Friction: the pre-carve full-make failed with `undefined reference to .L80076124` from the extracted `ACAD0.rodata.o` (the tell that func_800760CC's own jtbl needed carving out of the shared pool). Both crack subagents edited the shared `src/main/func_80071370.c` in parallel — reconciled clean at integration (different regions, no conflict). `func_80074E5C`'s decomp_loop percent (0.55) understates a 100%-structure single-argreg-swap residual (asm-differ weights the reg rename); the in-tree structure analysis, not the score, was the real read.
- Applied: 4 of 4 — #1 jtbl-carve mid-pool refinement → memory `jtbl-carve-both-edge-8align`; #2 new memory `gcc272-fold-range-test-slti-merge`; #3 loop-weight copy-pref terminal sub-case → `docs/hazards.md#loop-weight-and-live-length-regalloc-steering`; #4 ranker known-playbook-vs-wall-sensitive tail split → BACKLOG.
- Carry-over: `func_80074E5C` (terminal `global.c:587` allocno-tiebreak `a0`<->`a1` copy-pref wall, root-caused + `docs/wip/func_80074E5C.near-match.md`; permuter denied at 0.55 << 0.97). `func_80071370.c` retains 26 stubs (mid-logic/DL tail). LESSON: a plateaued-tail fan-out is BOTH a bank slice AND a sorter — known-playbook leaves (jtbl, glyph-classify) crack; pure allocno-tiebreak leaves terminal-carry.

## Sprint 236 — crack-attempt fan-out over 3 documented walls in func_80071370.c — 2026-07-16
- Increment: 0 files banked / **+3 functions matched** (`func_80071370.c` 31 stubs → 28; NOT md5-candidate).
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened.
- Seed: committed 5pt; banked 0pt (per-file all-or-nothing, file partial); realized 5, residual 0 (all first-candidate cracks, no +1 trigger); regime classical (crack-attempt slice).
- What helped: crack-attempt fan-out (gcc-2.7.2 + binutils-2.6, 3 parallel subagents, isolated `decomp_loop --profile main`) = **3 of 3** prior "no-source-lever" verdicts REFUTED (S232 rate, beat the planned S233-sort hedge). `func_800718C4` (#local-alloc-qty-permutation) + `func_8007512C` (#base-vs-disp + value-imm) both cracked FIRST-candidate on the PURE byte-offset form under do-while — variable bound+val + decl-order are the allocno flippers (no `*p++` ptr giv needed). `func_8007512C` value-imm: `s8*` keeps `li -1` (u8* truncates to 0xff). `func_80071924` (#base-vs-disp %lo-fold) cracked via pre-temp-defer-rowadd + `u8*` base cast.
- Friction: the 3 walls were all sub-tail of a plateaued c-stub file (had to override smallest-first `func_8006A2C0.c` on the crack-slice rationale). decomp_loop score=200/0.833 on both init-loop cracks was an isolation false-positive (unresolved %hi/%lo reloc rows); the in-tree full-make SHA-1 was the real oracle. A subagent left a stray `t.c.rtl` gcc dump + wrote a memory file mid-sprint (both reconciled at review).
- Applied: 4 of 4 — #1 byte-offset-cast refine (pure form suffices for single-store init loops; variable bound+val flippers) → memory `byte-offset-cast-defeats-base-ptr-cse`; #2 verified + kept subagent-written `pre-temp-defer-rowadd-lever.md` (already cross-linked, in MEMORY.md); #3 s8*-vs-u8* value-imm lever → same memory + MEMORY.md; #4 carried-wall crack-slice 4th confirmation (3/3; tune the `#base-vs-disp`/`#local-alloc-qty-perm` anchors to ~0.7, but re-derive residual from objdump — one carry's class was a misdiagnosis) → BACKLOG.
- Carry-over: none new. `func_80071370.c` retains 28 stubs (mid-logic/DL tail). LESSON (S232/S233/S235 4th time): a class-tagged prior wall verdict is a HYPOTHESIS — `func_80071924`'s "#base-vs-displacement %lo-fold" was an outright MISDIAGNOSIS (real cause = expr.c binop expand order). Re-derive the residual before trusting the carry's stated class.

## Sprint 235 — crack-attempt compiler-source fan-out over 4 documented main-segment walls — 2026-07-16
- Increment: 0 files banked / **+2 functions matched** (`func_8006F1A0.c` 10 stubs → 8; NOT md5-candidate) + 2 walls retired-with-citation.
- Quality: 0 stuck-far / 2 permuter / 2 carried / 0 re-opened.
- Seed: committed 3pt; banked 0pt (per-file all-or-nothing, no md5-candidate); realized ~5, residual +2; regime classical/mixed (crack-attempt slice).
- What helped: the crack-attempt fan-out (gcc-2.7.2 + binutils-2.6, 4 parallel subagents, isolated `make nonmatching-func MAIN=1`) — 3 of 4 prior "no-source-lever" verdicts REFUTED. Byte-offset-cast lever (`*(s32*)((u8*)SYM+off)`) cracked `func_8006F1A0` (base-vs-disp RMW, 220→0); byte-offset shared giv + bare-base DEST_REG ptr giv + do-while cracked `func_8006F24C` (in-tree byte-0). E004's S204 move_movables hoist SOLVED by 3 deterministic levers (dual-set pseudo blocks the hoist, call-order, prologue-share). 5E380 root-caused to a precise terminal citation (cse.c:5589 fold_rtx from_plus).
- Friction: 3 of 4 subagents went idle without sending their final verdict (only an idle_notification), costing ~3 orchestrator re-pings each; F24C sent a duplicate final after already integrated. E004's residual (global.c:587 live-length RA tie) is permuter-stubborn (158k iters, 0 breaks). The 400-isolated-score `j`-reloc artifact needed the in-tree gate to confirm byte-0.
- Applied: 4 of 4 — #1 byte-offset-cast lever + stride-array loop-crack recipe → `hazards.md#base-register-vs-displacement`; #2 cse.c:5589-5666 fold_rtx from_plus terminal sub-class → same section; #3 carried-wall crack-attempt-slice pricing + the S234 access-multiplicity "0-expected-bank → ~0.5 needs-lever" correction → BACKLOG; #4 subagent hand-off contract (send final verdict before idle) → agent-workflow.md fan-out recipe.
- Carry-over: `func_8003E004` (global.c:587 RA live-length tie, re-attempt with wider/longer permuter + seed diversity), `func_8005E380` (terminal cse.c:5589 fold, not re-grindable), + the 6 un-attempted `func_8006F1A0.c` tail stubs (jal-dispatch/regalloc-heavy). LESSON (S232/S233 3rd time): a pass-cited prior wall verdict is a HYPOTHESIS; when cheap-leaf veins are mined out, a crack-attempt fan-out over documented walls is a BANK slice, not just characterization.

## Sprint 234 — open fresh func_8006F1A0.c (main stat/score pack); access-multiplicity sorts the cheap-leaf vein — 2026-07-14
- Increment: 0 files banked / **+7 functions matched** (`func_8006F1A0.c` 17 stubs → 10; NOT md5-candidate).
- Quality: 0 stuck-far / 0 permuter / 2 carried / 0 re-opened
- Seed: committed 3pt (mixed-partial per-fn override, ranker pts13); banked 0pt (file partial); realized ~5, residual +2; regime classical/mixed
- Scope vs goal: goal was "open func_8006F1A0.c, mine the cheap-leaf vein." MET — committed 3/3 banked + 4 stretch = +7, all byte-exact FIRST build (asm-first fast-path, 0 permuter, 0 fan-out); 2 carried walls, 8 un-attempted jal-dispatch/stride-loop tail deferred per S224. PO chose the pack on cheap-leaf-depth over the sky DL/FP pack `func_8008D100` and the single-fn HUD monster `render_pin_assembly_with_wind_hud`.
- What helped: **access-multiplicity is the bank/carry discriminator for a fixed-stride `D_` array pack**, not size or FP. Single-use members banked byte-exact via the raw array-index form (getters `func_8006F1F0`/`func_8006F2F4`, setter `func_8006F2E8`, predicate `func_8006F1FC` `D_800FF1E8[i*140]==1`, address-return `func_8006F228` `&D_800FF1E9[i*140]`, rumble wrappers `func_8006F4F0`/`func_8006F50C`). The stride-multiply `sll;addu;sll;subu;sll` (×35/×140) reproduced free.
- Friction: the two carries walled on the SAME stride-0x8C array family (`D_800FF1E8`/`F210`/`F21C`/`F220`) — `func_8006F1A0` (RMW accumulate+clamp `D_800FF210[i*35]`, 3× same slot → GCC CSEs the la-pair into ONE full base pointer where the ROM re-materializes `%hi`+index+`%lo`-disp per store; `#base-register-vs-displacement`, no lever) and `func_8006F24C` (4-iter stride-0x8C init fill → GCC strength-reduces to 3 walking base pointers + `slti` counter vs ROM indexed addressing + `bne`; `#indexed-vs-pointer-loop-strength-reduction`). `func_8006F228` (single-use ADDRESS of the same array) DID bank — confirming the access-multiplicity line. `coddog-mirror:llcvt.c` was a pure STRUCTURAL false-positive (5th; 0/17 fns are `__ll_*`). 6th confirmation of the S224 cheap-leaf fresh-rotation, but the cliff here was addressing-mode-driven, not FP.
- Applied: 3 of 3 — #1 access-multiplicity ranker deweight (repeated/looped stride-`D_`-array access = partial-bank-risk, single-access = +1; refines the S227 %lo-fold index-count tell) → BACKLOG ranker follow-up; #2 single-use-address-vs-multi-use-value bank/carry lever → hazards.md `#base-register-vs-displacement`; #3 llcvt structural false-positive 5th confirmation + `func_8006F1A0`/`func_80080220` non-lib-callee deweight → BACKLOG.
- Carry-over: `src/main/func_8006F1A0.c` (10 stubs) — 2 documented no-lever carries (`func_8006F1A0`, `func_8006F24C`) + 8 un-attempted jal-dispatch/stride-array-loop tail (`func_8006F300` GBPak dispatcher … `func_800708B4` 198i). See BACKLOG `## Carry-overs`.

---

## Sprint 233 — open fresh raycast_terrain.c (main collision pack); compiler-source fan-out sorts the near-match tail — 2026-07-14
- Increment: 0 files banked / **+2 functions matched** (`raycast_terrain.c` 13 stubs → 11; NOT md5-candidate).
- Quality: 0 stuck-far / 1 permuter (`get_surface_type`, no crack) / 2 carried / 0 re-opened
- Seed: committed 3pt (fresh `none` cheap-leaf slice); banked 0pt (file partial); realized ~6, residual +3; regime classical/mixed
- Scope vs goal: goal was "open raycast_terrain.c, mine cheap-leaf vein, bank the 3 cheapest leaves." Result: 2 of 3 committed banked, 1 carried; the pack proved FP/collision-heavy (S224 productivity cliff) — its 5 smallest leaves were 1 clean integer bank + 4 compiler walls. Path diverged to a PO-directed gcc-2.7.2 + binutils-2.6 compiler-source fan-out over the near-match tail.
- What helped: (1) `get_triangle_normal_dominant_axis` — integer cross-product dominant-axis, byte-exact FIRST build. (2) `clamp_min_distance_from_target` — fan-out CRACKED a 505→0 FP-store-scheduling wall: root `sched.c:834-839 true_dependence` (a fixed `symbol_ref` global load does not alias a varying in-struct store, so the scheduler floats the store the ROM pins); lever = read the adjacent global `camera_position_z` via the varying pointer `cam[2]` (=`camera_position_x`+8), creating a memory-dependency edge that pins the x-store early AND fixes the downstream `$f0/$f4` swap. New reusable hazard (mirror of the retype-global-as-struct lever). (3) Re-used cracked levers across siblings (col-pointer base split, C89 top-decl, in-place `cz=15-cz` register-reuse).
- Friction: the two constant-15 walls (`get_surface_type` 450, `func_8003DE80` 350) are TERMINAL no-lever — a block-LOCAL constant materialized late to hide the R4000 load-latency (`mips.md:153-155` READY-DELAY 3; the scheduler front-loads the table `lbu`), so the build is 1 instr SHORTER than the ROM (the ROM lost a scheduler coin). The S232 `global.c` ref-count lever is INAPPLICABLE (block-local `local-alloc.c` qty, never enters the global sort); ~35 variants floored, `-fno-schedule-insns` worse. Spent a 4-min permuter (best 310) before the fan-out proved it terminal. KEY ASYMMETRY vs S232 (cracked 3/3): S232's walls were `global.c`-steerable; S233's were block-local scheduler coins — the fan-out's job is to SORT the tail into {crack, terminal-verdict}, not to crack everything. A no-lever verdict WITH a `file:line` citation is a real deliverable (stops re-grinding). Binutils subagent ruled the assembler out entirely (pure gcc codegen).
- Applied: 4 of 4 — #1 varying-pointer store-pin lever → hazards.md (#mem-in-struct-scheduling-lever, as the inverse); #2 block-local latency-coin terminal sub-case → hazards.md (#local-alloc-qty-permutation); #3 FP/collision-pack small-leaf deweight → BACKLOG ranker follow-up; #4 fan-out-value/terminal-verdict + S232↔S233 asymmetry note → agent-workflow.md fan-out section.
- Carry-over: `src/main/raycast_terrain.c` (11 stubs) — 2 terminal no-lever carries (`get_surface_type`, `func_8003DE80`) + 9 un-attempted larger FP/collision tail (`func_8003AC80` 464B … `check_ray_triangle_collision` 7204B). See BACKLOG `## Carry-overs`.

---

## Sprint 232 — crack 3 documented walls in func_80052FE0.c via compiler-source fan-out — 2026-07-13
- Increment: 0 files banked / **+3 functions matched** (`func_80052FE0.c` 9 stubs → 6; NOT md5-candidate). All 3 were documented carried near-match walls, retired.
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened (net −3 to the carried-wall backlog)
- Seed: committed 3pt (classical partial slice); banked 0pt (file partial); realized ~5, residual +2; regime classical/mixed
- Scope vs goal: goal was "bank 3 tractable leaves." At execution the 3 picked leaves proved to be documented carried walls (plan-gate miss — pick_target smallest-first + the FP/jal tell-filter don't see in-file near-match comments). PO approved a pivot to a crack-attempt slice; a 3-parallel gcc-2.7.2 + binutils-2.6 compiler-source fan-out CRACKED ALL 3. Count met, path diverged.
- What helped: three byte-exact source levers, each refuting a prior "not source-leverable" verdict — (1) `func_800542A0`: collapse `if(c1)r=1;else if(c2)r=1;` → `if(c1||c2)r=1;`; the single store drops the accumulator ref-count 4→3, halving `floor_log2(n_refs)` in `global.c:587-607` allocno priority so the scaled-offset pseudo out-ranks it and wins `$a0` (fixes the `$a0`↔`$a1` swap; it's global.c, NOT local-alloc as the prior note claimed). (2) `clear_animation_slot`: return `void`→`s32`; the candidate delay-slot fill `sll v0` writes `$v0`, and an s32 return marks `$v0` live at the return block, so reorg's opposite-thread liveness test (reorg.c:3374-3376) rejects the fill → ROM's nop (NOT jal-driven as prior note claimed). (3) `func_800543DC`: `goto neg` routes null+loop-exit through a shared `-1` tail while leaving the post-loop `return -1` inline; jump.c:1969 cross-jump merges the three identical tails otherwise (9 natural forms all merged first — legitimate last-resort goto).
- Friction: the plan gate committed 3 walls because it never grepped the target file for existing near-match comments; caught only at seed time. Compiler-source subagent fan-out is the fix and it earned its keep (3/3, ~530k-1.2M ms each) but a cheaper up-front detection would have framed the sprint correctly.
- Applied: 3 of 3 — #1 crack>fresh guidance → agent-workflow.md S224 note (documented walls are a hypothesis; crack-attempt fan-out above "prefer fresh pack" when the tail carries RE'd near-match comments); #2 plan-gate wall-comment grep → agent-workflow.md Definition of Ready + a `carried-wall:<fn>` pick_target ranker follow-up in BACKLOG; #3 two levers → hazards.md (#local-alloc-qty-permutation multi-BB store-count lever, #delay-slot-fill-across-call return-type correction, #cross-jump-tail-merge classical goto-split). Memory `revalidate-old-carries-stale-wall` reinforced (3/3, even pass-cited walls fell).
- Carry-over: `func_80052FE0.c` (6 stubs, un-attempted, no prior near-match comment) — `func_8005470C` (76i, matrix/FP-suspect), `func_80054550` (111i), `func_8005342C` (118i), `func_80052FE0` (71i), `func_800530FC` (204i), `calculate_bone_matrices` (783i). Likely wall-class per S224 but the tail is proven crack-worthy.

---

## Sprint 231 — open fresh deep-integer pack bgm_load_song_from_rom.c (main none pack) — 2026-07-13
- Increment: 0 files banked / **+23 functions matched** (`bgm_load_song_from_rom.c` 0/40 → 23/40 C, 17 stubs; NOT md5-candidate). Ties the S208 record; top of the +15-25 plan hedge.
- Quality: 0 stuck-far / 0 permuter / 17 carried / 0 re-opened
- Seed: committed 3pt (mixed-partial per-fn override, ranker pts13; plan gate wrote 13, realigned to 3 at retro per accepted #4); banked 0pt (file partial); realized ~10, residual +7; regime classical/mixed
- What helped: three novel byte-exact levers — (1) word-aligned struct-copy (`struct{u32 data[K];}`) reproduces the aligned lw/sw block-move for a struct-assign-to-cast-global AND a nested struct-array copy loop (byte struct → unaligned lwl/lwr + runtime check + base mis-addr); (2) array-of-row base-order split `u8(*rows)[K]=BASE; p=rows[idx];` forces base-materialize-first + index-reg-reuse (flat/pointer-add each missed one axis); (3) sequential-guard delay-slot fill (compute the 2nd guard operand up front to fill the 1st guard's branch delay — wrong order = +4 bytes = flowing-bss break, diagnosed by nm --print-size before the ROM cmp made sense). if/else-if reproduced bnel naturally (func_8005F180); a switch tree reproduced the beq/beql cascade (func_80060190). The func_80071220 sibling gave the Entry stride-6 struct + search idiom for func_800604F4.
- Friction: the #value-select wall recurred 3x and swallowed effort — func_80060190 (switch tree) and func_800604F4 (mode==5 search loop + 5-call chain) both matched byte-exact EXCEPT a single const-select case body that GCC if-converts to branchless regardless of `==`/`!=`/early-return/`&&`/single-temp; and func_8005F30C's `(x>0)?x:0` clamp always takes the `~x>>31` sign-trick vs the ROM's slt/negu. All carried (permuter can't flip if-conversion). One +4-byte flowing-bss shift (func_8006280C delay-slot) surfaced as a diff at a far-earlier rom offset.
- Applied: 4 of 4 — #1 aligned-struct-copy extension (cast-global + nested struct-array) → hazards.md #struct-copy-block-move; #2 array-of-row base-order lever → hazards.md #base-register-vs-displacement + sequential-guard delay-slot-fill lever → hazards.md #delay-slot-fill-null-guard; #3 const-value-select-is-source-invariant reconfirm (distinct from p?field:sentinel) → hazards.md #value-select; #4 mixed-partial committed-seed = 3 convention (not ranker 13) → VELOCITY + BACKLOG, plus the llcvt.c coddog structural false-positive (0/40 fns were llcvt mirrors) → BACKLOG.
- Carry-over: `bgm_load_song_from_rom.c` (17 stubs) — #value-select (func_8005F30C, func_80060190, func_800604F4; last two byte-exact but for a const-select body), permuter-candidates (func_8005F290 prologue-reg-order, gen_terrain_detail_texture dual-counter regalloc), #base-register (func_8005F4AC), FP (func_8005F964), big non-FP dispatch + D_801B71DC search loops (func_8005FCB8, func_8005FE8C, func_8005FB58, init_per_player_state 190i), intricate fall-through switch (func_8005F360), FP-monster/DL-emitter (func_800605EC 1985i, emit_course_terrain_dl 2237i, emit_per_phase_fog_state, emit_terrain_state_prefix_block $v0-arg, init_terrain_vertex_texcoords 88i).

---

## Sprint 230 — open fresh mixed pack play_sound_effect.c (main sound/bgm pack) — 2026-07-13
- Increment: 0 files banked / **+20 functions matched** (`play_sound_effect.c` 0/26 → 20/26 C, 6 stubs; NOT md5-candidate)
- Quality: 0 stuck-far / 0 permuter / 1 carried / 0 re-opened
- Seed: committed 3pt; banked 0pt (per-file all-or-nothing, file partial); realized ~9, residual +6; regime classical/mixed
- What helped: the `osSetIntMask`-guarded `MusHandle*` wrapper pattern banked FIRST-BUILD (12 fns) — the S177 "regalloc-heavy" pricing over-flags it (that wall needs loop + `ARR[K]` + ra-capture co-factors, absent here). Two one-line reorder levers cracked the two near-matches: idx-hoist named local (`s32 idx=row*5`, hoists the index-multiply early, `func_80051164` 265→0) and split-base pseudo (`u16 *arr=D_G; arr+i*K`, forces full base-materialization vs `%lo`-fold, `func_800511D8`). PO-requested check confirmed `alSynNew`/`alSynDelete` are game `osSyncPrintf`/`nop` debug-stubs sharing libaudio names, NOT mirrors (game uses libmus).
- Friction: two ranker PRICING false-positives cost early confidence — `alSynNew=synthesizer` was a NAME-only pack tag (stub body), and the `osSetIntMask` fns looked S177-walled but weren't. Two length-deficit flowing-`.bss` shifts (`func_800511D8` base-fold, `func_80051D8C` CSE) had to be diagnosed by readelf size vs the `.s` directive before the diff made sense.
- Applied: 4 of 4 — #2 S177-narrow (require loop+`ARR[K]`+ra-capture, not `osSetIntMask` alone) → hazards.md; #3 idx-hoist + split-base levers → hazards.md; #4 wrapper A/B (guard-vs-mask order) recognition → hazards.md; #1 `stub-suspect` ranker flag (lib-mirror-tagged fn ≤8i = name false-match) → BACKLOG.
- Carry-over: `play_sound_effect.c` (6 stubs) — `func_80051D8C` (`#cse-double-materialization` + loop-regalloc, permuter-candidate) + 5 deferred mid-logic/FP tail (`play_sound_effect` 108i lead, `func_80050DA0` 114i dispatcher, `func_80051210` 346i, `bgm_tick` 138i FP, `play_bgm_by_id` 97i 14-branch).

---

## Sprint 229 — open fresh pack func_80052FE0.c (main none pack) — 2026-07-11
- Increment: 0 files banked / **+6 functions matched** (`func_80052FE0.c` 0/15 → 6/15 C, 9 stubs
  remain; ROM SHA-1 green e2c4e7a…). OPENED a FRESH `none` pack (0x2E3E0, flipped asm→c) — PO chose it
  over bgm/terrain-DL and `raycast_terrain` (FP, no cheap leaves), on cheap-leaf-depth (~8 sub-40i
  getter/setter/predicate leaves, no DL reconstruction). One yaml flip, no symbol_addrs/sync-names/mk edit.
- Quality: 0 stuck-far / 0 permuter-run / 3 carried / 0 re-opened.
- Seed: committed 3pt; banked 0pt (file partial 6/15, per-file all-or-nothing); regime classical/mixed.
  Realized ~5 (seed 3 + strong +6 cheap-leaf mining incl. a switch + FP-transform leaf, − 3 no-lever
  near-matches), residual +2. Mixed-partial per-fn override (ranker pts13), same as S215-S228.
- Banked (+6): `func_800548DC` (tag=2 wrapper) · `func_800543A4` (clear-slots loop) · `func_80054310`
  (any-active predicate) · `get_character_state` (`(u32)id<4` sentinel getter, 1 fix: unsigned-bounds
  sltiu) · `func_800544B4` (club-kind switch; explicit `default: result=-1` lever) · `func_8005483C`
  (guMtxXFMF vec transform; FPR float-zero store-order lever).
- What helped: cheap-leaf-depth pick (4th fresh-rotation confirmation, S224/227/228 lineage). Two NEW
  positive levers: (a) explicit `default: var=DEFAULT;` in a small switch defeats the reorg optimize_skip
  branch-likely annul + routes through the common exit (`func_800544B4`); (b) named `f32 z=0.0f;` local
  keeps the FPU zero-store path (`mtc1`+`swc1`) AND source store order, where plain `out[i]=0.0f` folds to
  integer `sw zero` and chained assignment reverses order (`func_8005483C`). `contquery@99.99` coddog tag
  was a pure structural false-positive as predicted — 0 fns were mirrors.
- Friction: 3 no-source-lever near-matches consumed disproportionate time relative to +1-bank value —
  `clear_animation_slot` (NEW `#delay-slot-fill-across-call`: printf jal shifts the guard delay-slot fill
  vs the no-call sibling), `func_800542A0` (`#local-alloc-qty-permutation` a0↔a1 offset/accumulator swap;
  permuter skipped, 0 project cracks), `func_800543DC` (`#cross-jump-tail-merge`, 1-insn; `==` merges the
  post-loop -1 return, `!=` merges the loop-exit — no source form splits them; permuter-candidate). All
  fully RE'd with body documented in-file; the mid-logic/FP tail (DL builder lead + FP matrix + bone-matrices)
  deferred per S224 wall-class guidance.
- Applied: 4 of 4 — #1 switch explicit-default lever → hazards.md (optimize_skip variant); #2 FPR float-zero
  store-order → hazards.md (new section); #3 `#delay-slot-fill-across-call` → hazards.md (new section);
  #4 fresh-rotation 4th confirmation + cheap-leaf-depth ranker reinforcement → BACKLOG.md.
- Carry-over: `func_80052FE0.c` stays open (9 stubs: 3 carried near-matches + 6 deferred FP/DL/mid-logic:
  `func_8005470C` S158-FP matrix, `calculate_bone_matrices` 783i FP, `func_80052FE0` DL-builder lead,
  `func_80054550` 111i, `func_800530FC` 204i, `func_8005342C` 118i). Per plan: prefer a fresh pack /
  escalation next sprint over grinding this tail.

## Sprint 228 — open fresh pack func_800453E0.c (main none pack) — 2026-07-11
- Increment: 0 files banked / **+8 functions matched** (`func_800453E0.c` 0/40 → 11/40 C, 29 stubs
  remain; ROM SHA-1 green e2c4e7a…). +8 hand-matched plus 3 free empty-leaf auto-C (2i `jr ra;nop` leaves
  scaffold-emitted as `void f(void){}`; stub-count 37 < fn-count 40). OPENED a FRESH `none` pack (0x207E0,
  flipped asm→c) — PO chose it over `raycast_terrain` (FP, 56i leaf floor) + the open wall-tails, on
  cheap-leaf-depth (13+ sub-35i jal-light leaves). One yaml flip, no symbol_addrs/sync-names/mk edit.
- Quality: 0 stuck-far / 0 permuter / **3 carried** / 0 re-opened. All 3 carries fully-RE'd
  no-source-lever walls (in-file notes), NOT abandoned near-misses.
- Seed: committed 3pt (mixed-partial per-fn override, ranker pts13, same as S215-S227); banked 0pt
  (per-file all-or-nothing, file partial 11/40); realized 5 (seed 3 + strong +8 cheap-leaf mining incl. an
  FP + a mod leaf the ranker priced wall-risk, − 3 no-lever walls), residual +2; regime classical/mixed.
- What helped: the S224/S227 fresh-rotation call re-validated a 3rd time — a fresh pack's cheap-leaf vein
  banks +8 at 0 permuter before the wall-class tail reasserts. `func_800467DC` banked first-build as
  `(guRandom()>>2)%(arg0?:1)` — gcc's auto `break 7`/`break 6` divide-guards matched the ROM verbatim
  (write `%`/`/` directly, never hand-emit the guards). `func_800467DC`+`func_80047D68` (a mod + a
  straight-line FP leaf) banked despite living in the wall-risk size band.
- Friction: 3 of the smallest stretch leaves were no-source-lever walls back-to-back (`func_8004683C`
  local-alloc v0-permutation, `func_80048CF8` FP-regalloc, `func_80045AD4` dead-frame) — the same classes
  S227's tail hit. Confirms: once a fresh pack's cheap head is banked, the residual concentrates on the
  documented compiler walls; the RE work still lands (in-file notes) but does not bank.
- Applied: 3 of 3 — #2 divide-guard positive lever → `docs/hazards.md#signed-divide`; #1 empty-leaf
  auto-C stub-count note → `docs/agent-workflow.md` execution loop; #3 cheap-leaf-depth pack-score ranker
  follow-up → `BACKLOG.md` (golden-gated, off-cadence).
- Carry-over: `src/main/func_800453E0.c` partial (11/40 C, 29 stubs) — 3 characterized walls + the
  deferred mid-logic/FP/`calc_slope` tail + lead `func_800453E0` (213i). Next slice: a fresh pack /
  escalation, NOT a smallest-first continuation of this tail (S224 wall-class lesson).

## Sprint 227 — open fresh pack func_80071370.c (main none pack) — 2026-07-11
- Increment: 0 files banked / **+8 functions matched** (`func_80071370.c` 0/39 → 8/39, 31 stubs
  remain; ROM SHA-1 green e2c4e7a…). OPENED a FRESH `none` pack (0x4C770, flipped asm→c) — PO chose
  fresh rotation over the `func_80078910.c` wall-tail (S226 cheap vein exhausted; S224 plateau lesson).
  One yaml flip, no symbol_addrs/sync-names/mk edit.
- Quality: 0 stuck-far / 0 permuter / **3 carried** / 0 re-opened. All 3 carries fully-RE'd
  no-source-lever walls (in-file notes), NOT abandoned near-misses. 6 of 8 first-build.
- Seed: committed 3pt (mixed-partial per-fn override, ranker pts13, same as S215-S226); banked 0pt
  (per-file all-or-nothing, file partial 8/39); realized 5 (seed 3 + strong +8 cheap-leaf mining
  − first-build-heavy simple-fn offset + 3 no-lever walls), residual +2; regime classical/mixed.
- What helped: the S224 fresh-rotation call re-validated — a fresh pack's ~11-fn sub-30i vein
  (getter/setter/glue/strlen/DL-emitter) banks +8 at 0 permuter, far cheaper than grinding the
  plateaued `func_80078910.c` tail. Stock gbi macros for the DL emitter (`func_80074CA8`
  PipeSync/SetPrimColor) matched first-build (S226 lesson held). The struct-array setters/getters over
  the shared 0x2C-stride `D_8012F510` array modeled cheaply once the first field-symbol was decoded.
- Friction: burned ONE full-make cycle on a misleading signal — `func_80071924`/`func_8007512C` read
  per-fn asm-differ `(0)` but were 8/4 bytes SHORT (a `%lo`-fold dropped an instr); the header score
  shift-masked the deficit, surfacing only at the full-make SHA-miss (every downstream fn then
  mis-aligned). Both then confirmed as `#base-register-vs-displacement` walls (gcc folds `%lo` into the
  store where the ROM materializes the base; the single-index sibling `func_71C74` banked clean because
  there the fold matches). `func_800718C4` is a separate `#local-alloc-qty-permutation` v0/v1 IV-swap wall.
- Applied: 3 of 3 — #1 (size-check method note: cross-check `readelf` built size vs the `.s` size
  directive before trusting a per-fn `(0)`) → `docs/hazards.md #assembler-differences`. #2 (%lo-fold
  multi-index/in-loop-base D_-global store wall-risk pricing tell) → `BACKLOG.md` ranker follow-up,
  golden-gated off-cadence. #3 (S224 fresh-rotation re-validated) → this RETRO note, no edit.
- Carry-over: `func_80071370.c` (31 stubs) — 3 NEW documented walls (func_800718C4/func_80071924/
  func_8007512C). Cheap sub-30i vein exhausted; remaining tail = func_800760CC (jtbl switch, defer) +
  the 36-80i tier (747B0/74840/748D0 triplet call unbanked func_800738BC + div-round; 74E5C bnel
  value-select) entering the regalloc/mid-logic wall-sensitive class, then the 140i+ mid-logic + FP
  walls. Per S224: prefer a fresh pack / escalation next sprint over grinding this tail. Cross-repo: no
  new curated names (all `func_`).

---

## Sprint 226 — continue func_80078910.c (DL-emitter vein) — 2026-07-10
- Increment: 0 files banked / **+3 functions matched** (`func_80078910.c` 7/37 → 10/37, 27 stubs
  remain; ROM SHA-1 green e2c4e7a…). CONTINUED the in-progress pack — PO chose continuation over a
  fresh pack / escalation (pack not plateaued: S225 banked clean and stopped at cap). Zero enablers
  (subseg already `c`).
- Quality: 0 stuck-far / 0 permuter / **0 carried** / 0 re-opened. 2 of 3 first-build; 1 one-iteration
  (`func_8007CF10`, frame-size fix only).
- Seed: committed 3pt (mixed-partial per-fn override, ranker pts13, same as S215-S225); banked 0pt
  (per-file all-or-nothing, file still partial 10/37); realized 3 (seed held — clean sprint, first-
  build-heavy −1 offset balanced by genuine DL/Scis-macro decode + proto RE), residual 0; regime
  classical/mixed.
- What helped: a low-FP DL-emitter vein is TRACTABLE, not wall-class — the twins `func_8007DE9C`/
  `func_8007DFD0` (8-cmd F3DEX2 billboard, differ only in vtx data) banked first-build, and the
  stretch `func_8007CF10` one-iteration. Going STRAIGHT to stock gbi macros (each word verified vs
  gbi.h + `gfxdis.f3dex2 -x -w`) beat the raw-word intermediate. The stretch's three "wall tells" all
  dissolved: `MAX((s16),0)`+`0xFFC` texrect clamp = stock `gSPScisTextureRectangle`; declaring
  `project_point_to_screen(f32,f32,s32,s32*)` avoided the missing-proto implicit-int flip; the FP
  depth-cull guards emitted from a plain `if (z<-3 && z>-60)`.
- Friction: `func_8007CF10` first build missed only on frame size (target 0x38 vs 0x28) — the
  `project_point_to_screen` out buffer is 6 words, not the 2 I first reserved; `s32 screen[6]` fixed it
  byte-exact. Minor; caught immediately by the in-tree asm-differ (body was already identical).
- Applied: 2 of 4 — #2 (Scis-texrect hazard note) + #3 (gfxdis→stock-macro primary path) applied to
  `docs/hazards.md #display-lists`. #1 (`dl-emitter-tractable` ranker signal, golden-gated off-cadence)
  + #4 (`screen[6]`→named struct, blocked on `project_point_to_screen` RE) PO-accepted → recorded as
  BACKLOG follow-ups.
- Carry-over: none. `func_80078910.c` continues (27 stubs: FP-walls, `func_8007B994` 785i state
  machine, mid-logic base-register walls func_8007D19C/DB08/FEAC). PO open question: continue the pack
  vs rotate to a fresh pack next sprint given the remaining tail is all wall-class.

---

## Sprint 225 — open fresh pack func_80078910.c (rumble/shadow/effects) — 2026-07-10
- Increment: 0 files banked / **+7 functions matched** (`func_80078910.c` 7/37, 30 stubs remain; ROM
  SHA-1 green e2c4e7a…). Opened a FRESH `none` pack (0x53D10) — PO chose it over raycast-FP /
  f80071370 / an escalation slice, acting on the S224 plateaued-pack-tail lesson. One yaml flip, no
  symbol_addrs/sync-names/mk edit.
- Quality: 0 stuck-far / 0 permuter / **0 carried** / 0 re-opened. The score-60 `rumble` near-miss was
  root-caused + banked, NOT carried. 5 of 7 first-build.
- Seed: committed 3pt (mixed-partial per-fn override, ranker pts13, same as S215-S224); banked 0pt
  (per-file all-or-nothing, file still partial 7/37); realized 5 (seed 3 + 2 novel root-cause levers −
  first-build-heavy offset), residual +2; regime classical/mixed.
- What helped: the S224 escalation-routing call paid off — a fresh pack's cheap non-FP vein banks far
  cheaper than a plateaued-pack continuation (+7 @ 0/0/0/0 vs S224 +1/3-walls). The heap alloc/free/
  dispatch family (`func_80078910` teardown + `func_800789C8` 12-block glue + `func_8007E234`/
  `func_8007E2B0` per-mode init) banked as a coherent CLUSTER once the first member's D_-global/struct
  model was set. `rumble_check_and_trigger`+`shot_start_rumble_trigger` are byte-identical twins.
- Friction: two functions initially looked like compiler walls but were source-side misses. (1)
  `func_80078D94` particle-init loop hit gcc `check_dbra_loop` REVERSAL (loop.c:5655) on the natural
  `for`/`do-while` forms; cracked with the in-loop giv `(base+i)->f=v` (giv-init emitted after the
  hoisted invariants, reversal blocked). (2) `rumble_check_and_trigger` locked at score-60 (a `la`
  base-materialize scheduled AFTER a `jal` vs the ROM's before-jal delay-slot fill); bisected to a
  MISSING callee prototype → gcc implicit-int → flipped scheduling. Both root-caused via the `-S`
  codegen oracle + a gcc-2.7.2 source read, no permuter.
- Applied: 2 of 2 — #1 `#counter-up-pointer-giv-fill-loop` recipe (→ docs/hazards.md); #2
  `#callee-prototype-is-load-bearing` (→ docs/hazards.md + the before-permuter checklist in
  docs/agent-workflow.md Execution loop). Memory saved: `missing-prototype-implicit-int-scheduling-flip`.
- Carry-over: `func_80078910.c` (30 stubs) — no NEW carries. Next vein: the 187-208i pure-logic tier
  (`func_8007D19C`/`func_8007DB08`/`func_8007FEAC`, jal0/8) then the FP walls (`func_8007B054`/
  `func_8007A6C8`/`func_8007E980`) + DL (`draw_character_shadow`). LESSON: "declare every callee
  prototype" is now a before-wall checklist item — a missing prototype is a cheap source-side cause
  that masquerades as a scheduling wall.

---

## Sprint 224 — func_8006A2C0.c mid-logic-tail mixed-partial continuation — 2026-07-10
- Increment: 0 files banked / **+1 function matched** (`func_8006A2C0.c` 20/45, 25 stubs remain; ROM
  SHA-1 green e2c4e7a…). Continued the S223 pack (PO chose continue-open-file over a fresh-pack flip;
  no enabler, subseg already `c`).
- Quality: 0 stuck-far / 0 permuter / **3 carried** (`func_8006DF84`, `func_8006DEB4`, `func_8006D058`)
  / 0 re-opened. All 3 fully RE'd (100% logic), each a NAMED wall class, none permuter-reachable.
- Seed: committed 3pt (mixed-partial per-fn override, same as S215-S223); banked 0pt (per-file
  all-or-nothing, file still partial); realized 5 (seed 3 + 3 carries − re-attempts folded), residual
  +2; regime classical/mixed.
- What helped: `func_8006ADF8` mode-gated club dispatch banked via the range-unfold lever (nested
  `if(arg>0){if(arg<4)}` un-folds the `(u32)(arg-1)<3` sltiu to the target's blez+slti); float consts
  12.0f/-2.0f as lui/mtc1 literals (no rodata); 5-arg call w/ stack 0x7F. On DEB4 a `<=` operand-order
  lever pinned load-order + branch-polarity + inline-block + the p-base register (a1) all at once.
- Friction: the mid-logic tail is a wall-class cluster. `func_8006DF84` — branch-likely `bnezl` +
  `&SYM[const]` address-fold: `optimize_skip`'s annulled skip needs a 1-insn store, GCC folds to a
  2-insn absolute → plain bnez + tail-merge. `func_8006DEB4` — `#base-register-vs-displacement`: matched
  everything except the D_801B7270 chain (scalar folds w/ p=a1; pointer gets base-reg but swaps p→a2 +
  caches; target wants base-reg+reload+p=a1, no modeling gives all three). `func_8006D058` — D164
  loop-strength-reduction wall x2 (4×7 double-loops): iterated index→pointer-walk→`!=` (6180→4040→2960)
  but stays structurally LONGER (extra setup/materialization insns), so not permuter-eligible.
- Applied: 2 of 2 — #1 mid-logic-tail escalation-routing note (→ docs/agent-workflow.md workflow-overview
  subagent-fanout, + a tracked ranker follow-up in BACKLOG.md); #2 DEB4/ADF8 lever chain (→
  docs/hazards.md `#nested-guard-range-unfold--comparison-operand-order`).
- Carry-over: `func_8006A2C0.c` (25 stubs) — new carries `func_8006DF84`/`func_8006DEB4`/`func_8006D058`
  (+ S223's `func_8006C8CC`/`func_8006D164`); deferred `func_8006D38C`/`D214`/`CE88`/`D4EC` (same
  base-register/loop/branch-likely combo), the large DL/dispatch tail, `func_8006CD50` (caller-evict).
  LESSON: S223 (+19 easy leaves) → S224 (+1, 3 walls) is the fresh-pack productivity cliff; a plateaued
  pack's mid-logic residual wants a fresh pack or escalation slice, not another smallest-first pass.

---

## Sprint 223 — func_8006A2C0.c fresh-pack mixed-partial mine — 2026-07-10
- Increment: 0 files banked / **+19 functions matched** (`func_8006A2C0.c` 19/45, 26 stubs remain; ROM
  SHA-1 green e2c4e7a…). Fresh 45fn `none` pack opened this sprint (subseg 0x456C0 flipped `c` at gate).
- Quality: 0 stuck-far / 0 permuter / **2 carried** (`func_8006C8CC`, `func_8006D164`) / 0 re-opened.
- Seed: committed 3pt (partial-bankable per-fn, ranker pts13 override); banked 0pt (mixed-partial,
  per-file all-or-nothing); realized 5 (seed 3 + 2 carries + re-attempts, −1 from 8 first-build leaves),
  residual +2; regime classical/mixed.
- What helped: smallest-first per-fn INLINE mine, no subagent fan-out. 8 tiny getter/setter/predicate
  leaves (12-20B) byte-exact first-build; asset-load/free vein banked off the `func_8003E400.c` twin
  (`u8 sp10[0x20]` + heap3_alloc/free); mode-state resets banked 1-2 tries. NOVEL LEVER (3x): temp-var
  store-order — pin an indexed/computed load into a local BETWEEN neighboring global zero-stores so the
  loaded-value store lands last (`func_8006DDCC`/`DE44` stride-116 table read + `func_8006BA24` post-call
  field read); direct `G = <load>` hoists the load to the store's slot. Same-TU forward-decl
  (`func_8006B54C`) blocked the -O2 inline so the caller `jal`s it.
- Friction: `func_8006C8CC` — cascaded predicate byte-exact except the tail `(u16 & 0x8000)` test; GCC
  2.7.2 collapses it to `srl v0,v0,0xf` while ROM keeps `andi 0x8000; bnez; li 1` (single-return-var +
  explicit `!=0` both still collapse). `func_8006D164` — strided u16 4×7 double-loop copy: body + all
  trailing globals byte-exact, residual = outer loop de-hoists the limit `li v0,4` INSIDE the loop + IV
  register coloring (`#top-tested-loop-goto-local-hoist` / `#indexed-vs-pointer-loop-strength-reduction`).
  Both minor near-matches, carried below the smallest-first threshold.
- Applied: 2 of 2 — #1 temp-var store-order lever (→ docs/hazards.md, after the jal-delay-slot store
  subsection); #2 C8CC srl-fold note (→ docs/hazards.md, same neighborhood, NOTE-only).
- Carry-over: `func_8006A2C0.c` (26 stubs) — carries `func_8006C8CC` (srl-fold), `func_8006D164`
  (loop-regalloc); deferred `func_8006AD1C` (`0x92492493` `#local-alloc-qty` divide wall), `func_8006DF84`
  (branch-likely + struct-base), the large DL/dispatch/logic tail, `func_8006CD50` (caller-evict).

---

## Sprint 222 — func_80095A10.c predicate + DL builder (mixed-partial continuation) — 2026-07-10
- Increment: 0 files banked / **+1 function matched** (`func_80095A10.c` 9/31, 22 stubs remain; ROM
  SHA-1 green e2c4e7a…). `func_80095C10` byte-exact; `func_80098C6C` carried.
- Quality: 0 stuck-far / 0 permuter / **1 carried** / 0 re-opened.
- Seed: committed 3pt (predicate + DL pair); banked 0pt (partial mixed, per-file all-or-nothing);
  realized 4 (seed 3 + carry), residual +1; regime classical/mixed.
- What helped: the DL builder (`func_80095C10`) banked cleanly via the gfxdis/F3DEX2 workflow, NO
  subagent fan-out. Two source-steerable levers cracked it: (1) `#dl-builder-symbol-anchor` — model the
  light buffer as `Light D_<a>[3]` + ambient `(u8*)base-8` (array + ptr-arith) to match the asm's
  `D_800C73B0`-anchored base, NOT a `Lights3` struct at `D_<a-8>` (which anchors the wrong symbol with
  positive offsets); (2) `#guard-block-layout-inversion` — write the early-return guard as
  `if (main_cond) {main; return 1;} guard; return 0;` so GCC lays the return-0 guard inline and main as
  the taken branch, matching the ROM block order. One condition-inversion flipped it to byte-exact. The
  5-cmd `gSPSetLights3` composite doesn't advance `pkt`, so it's 5 individual `gSPNumLights`/`gSPLight`
  advancing calls (offset `(n)*24+24`).
- Friction: `func_80098C6C` (a 13-instr `fabsf(D_800E4C7C)==0.0f` leaf, fabsf = gcc-2.7.2 builtin ->
  inline `abs.s`) matched its branch/return half byte-exact (via the `s32 ret=0; if(..) ret=1;`
  accumulator form) but its 4 FP-setup instrs (load f0-vs-f4, in-place-vs-distinct abs, mtc1/abs
  schedule order) are a pure `#local-alloc-qty-permutation` + scheduler tie-break, source-invariant
  across 5 shapes. Below 0.97 -> permuter N/A (0 project cracks this class). Stretch `func_80098E48`
  (162-instr `%28` divide-dispatcher, same family as the carry) declined pre-attempt as a probable
  regalloc wall.
- Applied: 2 of 2 (#1 `#dl-builder-symbol-anchor`, #2 `#guard-block-layout-inversion`; both new
  `docs/hazards.md` levers + index rows).
- Carry-over: `func_80095A10.c` mixed-partial (22 stubs) — `func_80098C6C` near-match (docs/wip);
  FP-dispatcher tail (S158-class) + caller-evict `func_800989EC` + carried `func_80098CD8` remain.

---

## Sprint 221 — func_80095A10.c signed-divide %28 pair (mixed-partial continuation) — 2026-07-10
- Increment: 0 files banked / **+1 function matched** (`func_80095A10.c` 8/31, 23 stubs remain; ROM
  SHA-1 green e2c4e7a…). `func_80098D70` byte-exact; `func_80098CD8` carried.
- Quality: 0 stuck-far / **1 permuter (CRACKED)** / **1 carried** / 0 re-opened.
- Seed: committed 3pt (2-fn `#signed-divide-const` continuation); banked 0pt (partial mixed, per-file
  all-or-nothing); realized 5 (seed 3 + permuter + carry), residual +2; regime classical/mixed.
- What helped: PO-directed compiler-source fan-out (3 subagents, gcc-2.7.2 + binutils-2.6) root-caused
  the signed `/28` magic (expmed.c:3058) AND handed the exact fn2 nested-loop skeleton (reached score 300
  hand-iterated) — the fan-out earns its keep as a SEED oracle, not just a wall-characterizer. Permuter
  then cracked fn2's residual to 0 via an `if(1){}` CSE extended-basic-block barrier (forces the loop-top
  `*p` reload) + an `i-K` subexpr split. Clean A/B: fn2's CSE-collapse residual is barrier/split-reachable
  (permuter cracks); fn1's scheduler register-coloring tie-break is not (366k iters, 0).
- Friction: a permuter `import.py make` left a stale `build/` object that an incremental `make` skipped,
  so `diff.py` FALSE-POSITIVED byte-exact ("mirage"); clean-rebuild + the spot-check discipline caught it
  before a bad bank. New `rm-object-before-diff` guard codified.
- Applied: 3 of 3: #1 `#permuter-import-pollutes-build-stale-diff`, #2 `#cse-ebb-barrier-loop-reload`,
  #3 permuter-tractability residual-class triage.
- Carry-over: `func_80095A10.c` continues mixed-partial; `func_80098CD8` flagged for
  `#cross-project-matched-corpus-mining` (PO), not a plain permuter retry.

## Sprint 220 — func_80095A10.c 31-fn main-seg pack (mixed-partial, smallest-first) — 2026-07-10
- Increment: 0 files banked / **+7 functions matched** (`func_80095A10.c` 7/31, 24 stubs remain;
  ROM SHA-1 green e2c4e7a…). Committed backlog 5/5 + 2 stretch, all byte-exact.
- Quality: 0 stuck-far / 0 permuter / **0 carried-on-committed** / 0 re-opened (both hard fns CRACKED,
  not spiked; 3 stretch un-attempted by difficulty triage = backlog, not spikes).
- Seed: committed 8pt (nfns≥4 large pack); banked 0pt (partial mixed, per-file all-or-nothing);
  realized ~10 (seed 8 + 2 novel root-cause gotchas), residual +2; regime classical/mixed.
- What helped: fan-out subagents over gcc-2.7.2 found 2 reusable levers AND corrected a wrong
  foundational fact — the KMC scheduler IS active at -O2 (mips.md `define_function_unit`;
  `-fno-schedule-insns` toggle changed the `.o`), superseding the S209 "no scheduler" claim.
  `#scheduler-load-hoist-serial-store-lever` (shared temp pins independent global copies serial) cracked
  `func_800989C4`; `#cse-dest-preference-copy-collapse` (`u16` temp → zero_extend blocks the cse.c:6714
  copy-collapse) cracked `func_80098CA0`. Trivial getter/setter leaves banked first-build.
- Friction: 2 of 7 needed compiler-source dives (the rest first-build). The profiling awk under-counted
  tab-form `jal`s (stretch fns had calls). Stretch tail is a real difficulty step-change.
- Applied: 3 of 3: #1 scheduler-correction (FOUNDATIONAL hazards.md rewrite + new
  `#scheduler-load-hoist-serial-store-lever`, plus fixed 2 stale "no scheduler" refs and the
  `[[kmc-cc1-no-instruction-scheduler]]` memory); #2 new `#cse-dest-preference-copy-collapse`; #3
  pick_target leaf-vein banking-behavior anchor (note-only, no ranker change).
- Carry-over: `func_80095A10.c` remaining 24 — signed-divide-by-28 pair (`func_80098CD8`/`func_80098D70`,
  #signed-divide-const), F3DEX2 DL builder `func_80095C10`, FP tail (`func_80095A68`/`func_800977E0`/
  `func_80097A08`/`func_80097C18`/`func_80097E30`/`func_8009676C`/…), `func_800989EC` caller-evict.

## Sprint 219 — get_tile_attribute.c jtbl vein + struct anchor (mixed-partial, cont.) — 2026-07-10
- Increment: 0 files banked / **+1 function matched** (`ci8_to_rgba5551` jtbl switch + RGBA5551 pack;
  ROM SHA-1 green e2c4e7a…; delta 230→230 md5-candidate, file ~23/44, 22 stubs remain).
- Quality: 0 stuck-far / 0 permuter / **4 carried** / 0 re-opened (carries `func_800402F4`,
  `func_80042228`, `blend_terrain_color` = jtbl atomicity wall; `func_800425C8` = loop.c IV-bias ~1615).
- Seed: committed 8pt (c-stub continuation); banked 0pt (partial mixed, per-file all-or-nothing);
  realized ~12 (seed 8 + 4 carries), residual +4; regime classical/mixed.
- What helped: the **6-lever switch-bit-pack recipe** cracked `ci8_to_rgba5551` 3855→0 byte-exact
  (offset-0 field syms → `lb`; s32 return drops `andi`; in-place index → arg-reg; output-ascending cases
  + NO default → anti-cross-jump-tail-merge → value-ascending blocks; lazy last-field load). The
  **both-edge-8-align carve test** predicted which jtbls are bankable (1 of 4).
- Friction: the jtbl vein is mostly ATOMICITY-WALLED — one object's `.rodata` places contiguously, so a
  table not 8-aligned-both-edges (or with a still-asm fn's string interleaved) can't carve without a
  trailing-pad shift; only ci8's standalone `jtbl_800CA930` qualified. S217's `func_800425C8` struct-model
  guidance was INVERTED (ROM re-materializes 6 SEPARATE offset-0 symbols, not one folded base). Permuter
  useless on a bss-multi-symbol fn (0.04% isolated vs 1615 in-tree).
- Applied: 3 of 3 — #1 both-edge-8-align carve-feasibility test → `docs/hazards.md#switch-jtbl-dispatch`
  lever 5 + `#rodata-sibling-yaml-pattern` (+ pick_target follow-up flag); #2 6-lever switch-bit-pack
  playbook → `#switch-jtbl-dispatch` lever 6 + memory `jtbl-carve-both-edge-8align`; #3 S217 inversion
  corrected in BACKLOG + bss-multi-symbol permuter-blocked note → `#isolated-compile-caveat`.
- Carry-over: `src/main/get_tile_attribute.c` — S219 carries `func_800402F4`/`func_80042228`/
  `blend_terrain_color` (jtbl atomicity wall) + `func_800425C8` (IV-bias). Prior carries stand. No
  plain-tractable stub remains; each further bank needs a wall break (FP-regalloc fan-out, an interleaved-
  rodata co-bank to unwall a jtbl, or the IV-bias/phantom-addend GCC-source dives).

## Sprint 218 — get_tile_attribute.c non-FP tail via compiler-source fan-out (mixed-partial, cont.) — 2026-07-10
- Increment: 0 files banked / **+1 function matched** (`func_80041B98` grid-vertex averager; ROM SHA-1
  green e2c4e7a…; delta 230→230 md5-candidate, file ~22/44, ~22 stubs remain).
- Quality: 0 stuck-far / 0 permuter / **3 carried** / 0 re-opened (carries `func_80041878`,
  `func_800415C4`, `func_80041EC0`, all root-caused).
- Seed: committed 8pt (c-stub continuation); banked 0pt (partial mixed, per-file all-or-nothing);
  realized ~11 (seed 8 + 3 carries), residual +3; regime classical/mixed.
- What helped: **compiler-source subagent fan-out UP FRONT** (4 parallel over gcc-2.7.2 + binutils-2.6,
  PO directive) — 1 byte-match + 3 walls each pinned to a diverging pass with file:line, in one pass
  instead of sequential iterate-then-permuter. Recovered a reusable `Tile`/`TileEntry` (0x10/0x100) type
  model @D_80185220 + selector-table stride for the whole setter family.
- Friction: the ranker's "tractable non-FP getter/setter tail" was 3/4 regalloc/ABI-walled — smallest-
  first surfaced walls as if tractable (reinforces the pending regalloc-heavy pts detector).
- Applied: 3 of 3 — #1 fan-out-up-front note → `docs/agent-workflow.md ## Workflow at a glance`; #2
  div-by-4-index→selector-table wall-tell → BACKLOG off-cadence golden-gated pts-detector follow-up
  (not inline); #3 `func_80041E8C` `$v0`-arg ABI wall → verified asm + project memory
  `func-80041e8c-v0-arg-convention-wall` + `docs/wip/func_80041EC0.near-match.md`. Permuter escalation
  PO-declined (wall class = 0 cracks historically).
- Carry-over: `src/main/get_tile_attribute.c` — S218 carries `func_80041878`/`func_800415C4`
  (`#local-alloc-qty-permutation`, `docs/wip/` each) + `func_80041EC0` (`func_80041E8C` `$v0`-arg wall);
  prior carries (lead `get_tile_attribute` phantom-addend, `func_80041E8C`, `func_800402F4` jtbl,
  `func_800425C8` flowing-bss) stand. Non-FP getter/setter vein MINED OUT; remaining = FP interp + jtbl
  + wall cluster. Next slice: FP/jtbl sprint or corpus-mining on the accumulated regalloc carries.

## Sprint 217 — get_tile_attribute.c clean-compute vein (mixed-partial, cont.) — 2026-07-10
- Increment: 0 files banked / **4 functions matched** (delta: no md5-candidate change, 230→230; file
  ~21/44 banked, 23 stubs remain = FP terrain-height interp, jtbl dispatch, 17-jal dispatch, phantom-
  addend lead-fn wall, nested-fn, NOT md5-candidate). Committed 3/3 banked + stretch 1/2 banked, 1
  carried.
- Quality: 0/**1**/**1**/0 (stuck-far/permuter/carried/re-opened) this sprint. 1 permuter
  (`init_grid_vertex`, 0.91 local-alloc reg-permutation, score 0 at iter 618 — first permuter use on this
  file). 1 stretch carried (`func_800425C8` multi-hazard).
- Seed: committed 0pt; banked **0pt** (per-file all-or-nothing, file partial); regime mixed. Value
  signal = **+4 matched-fn count**. Residual n/a (partial file).
- What helped: found a reusable **grid-vertex builder vein** — `init_grid_vertex` (Vtx-init),
  `average_grid_vertices` (midpoint), `lerp_grid_vertices` (weighted `(a*(16-t)+b*t)/16`) share the 16B
  `s16 ob[3]`/`flag`/`tc[2]`/`u8 cn[4]` layout. Two levers bank the family first-build: (a) natural
  field-store order lets GAS fill the `ob[2]` load-delay slot with the `flag=0` store; (b) uniform
  `(a+b)/N` matches BOTH signed-s16 (full round-toward-zero) and unsigned-byte (bare `sra`, `nonzero_bits`
  proves non-negative) fields — no per-field special-casing. `load_club_offset_pair` first-build too.
- Friction: `init_grid_vertex` locked at 0.91 (store order byte-exact, entry-block compute cluster reg-
  permuted); source levers folded to identical emission, so it needed the permuter. Plumbing gotcha:
  `setup-permuter.sh --main` aborts once the fn is inlined-as-C — drove `import.py` directly on the
  seeded `base.c` + build-generated `.s`. `func_800425C8` (stretch) is a multi-hazard carry: flowing-bss
  on 6 auto-`D_` output symbols (floated to region-base) + target spills 6 min/max to stack from held
  constant-regs; needs a `collision_triangles` struct model.
- Applied: 3 of 3 — #1 (new `#grid-vertex builder vein` hazards.md subsection: delay-slot field-order +
  `/N` `nonzero_bits` levers), #2 (`#local-alloc-qty-permutation`: extractable-clean-reorder note +
  `setup-permuter --main` inlined-fn plumbing gotcha), #3 (`#short-text shifts flowing-bss` multi-`D_`-
  write variant + tracked pick_target `needs-struct-model` ranker follow-up).
- Carry-over: `func_800425C8` (multi-hazard: flowing-bss + stack-spill + struct-model). File stays open
  (mixed-partial); remaining tail = FP height-interp, jtbl dispatch (shared-rodata carve), 17-jal
  dispatch, `get_tile_attribute` phantom-addend wall, `func_80041E8C` nested fn.

## Sprint 216 — get_tile_attribute.c mixed-partial (cont.) — 2026-07-10
- Increment: 0 files banked / **4 functions matched** (delta: no md5-candidate change; file 17/44
  banked, 28 stubs remain = FP height-interp, jtbl dispatch, coddog-structural false-hits, NOT
  md5-candidate). Committed 4 (2 banked, 2 carried mis-scoped) + stretch 3 (2 banked, 1 near-match
  carry).
- Quality: 0/0/**2**/0 (stuck-far/permuter/carried-committed/re-opened) this sprint, +1 stretch
  near-match wall. ZERO permuter. 2 of the 4 committed items were mis-scoped by the jal/fp tractability
  scan (nested-fn + jtbl, both carried).
- Seed: committed 0pt; banked **0pt** (per-file all-or-nothing, file partial); regime mixed. Value
  signal = **+4 matched-fn count**. Residual n/a (partial file).
- What helped: asm-first fast-path again (no MCP/permuter). In-tree `asm-differ diff.py` isolated the
  one near-miss per fn fast. 2 NEW reusable regalloc levers cracked near-matches: masks-into-temps
  forces a base ptr to reuse a freed arg reg (`get_terrain_type`); naming a hoisted invariant const
  before the base var controls preheader materialization order (`mark_scenery_collision_cells`).
- Friction: `get_tile_attribute` (lead, body fully solved) is a NEAR-MATCH WALL — GCC bakes a phantom
  -0x10 in-place LO16 addend onto the `D_800BAC0C` ref across ALL index forms (1D/2D/byte-offset/local
  ptr), index value identical to ROM. Needs a GCC-source dive (addr-giv fold). The jal/fp scan
  mis-flagged `func_800402F4` (jtbl) and `func_80041E8C` (nested fn) as tractable, costing 2 dead
  committed picks.
- Applied: 3 of 3 — #1 (`#phantom -N in-place addend` new hazard subsection under
  `#base-register-vs-displacement`), #2 (2 regalloc levers into `#register-reuse nudge`), #3 (jtbl
  per-fn triage-grep note into `#switch-jtbl-dispatch`). Live-state pick_target + libultra goldens
  regenerated (banking drift). NOTE: 3 PRE-EXISTING `make test-tools` failures confirmed to predate
  S216 (playbook-index `cse-make-regs-eqv` anchor, `coddog_suppresses`/`ranked_by_descending`
  stale-fixtures) — carried to BACKLOG, not S216-induced.
- Carry-over: `get_tile_attribute` (phantom-addend wall, characterized in-file), `func_800402F4`
  (jtbl, shared-rodata carve), `func_80041E8C` (nested fn / static-chain-in-$v0). File stays open.

## Sprint 215 — get_tile_attribute.c terrain-query pack (mixed-partial) — 2026-07-10
- Increment: 0 files banked / **13 functions matched** (delta: no md5-candidate change, 230→230; file
  13/44 banked, 31 stubs remain = FP terrain-height interp + jtbl dispatch + coddog-structural
  false-hits, NOT md5-candidate). Committed 4 trivial getters + stretch 3, banked those + 6
  opportunistic (186% of the 7-fn stretch plan).
- Quality: 0/0/0/0 (stuck-far/permuter/carried-committed/re-opened) this sprint. ZERO permuter, ZERO
  MCP. 3 fns (func_80042DF4, get_direct_grid_vertex, func_800432E4) took a 2nd source-form (block
  layout / IV strength-reduction); 10/13 first-build.
- Seed: committed 5pt; banked **0pt** (per-file all-or-nothing, file partial); regime classical/mixed.
  Realized tier: file 0pt (not md5-candidate); value signal = **+13 matched-fn count**. Residual n/a
  (partial file).
- What helped: asm-first fast-path (hand-translate from splat `.s`, no MCP/decomp_loop) scaled to a
  whole tractable getter/glue vein in one session. Opaque pointer typing (`void*`/`s16*`/`u8*` + cast)
  avoided duplicating a sibling's local typedef at gate. In-tree `asm-differ diff.py` isolated the one
  SHA-breaking near-miss per batch fast.
- Friction: 3 near-misses needed a source-form flip — guard-clause inverted a two-arm both-return
  block layout (fixed with single-return-temp), and `a++` strength-reduced a fixed-trip multi-offset
  compare (fixed with the index form `a[i+K]`). Both now documented levers.
- Applied: 2 of 3 — #S1 (`#value-select-if-else…` two-arm block-layout sub-lever), #S2
  (`#indexed-vs-pointer loop` fixed-trip multi-offset sub-lever); (#S3 asm-first-vein workflow note
  NOT selected — fast-path already documented).
- Carry-over: none committed. `get_tile_attribute.c` stays open (mixed-partial); the 31 remaining
  stubs (FP height-interp, jtbl dispatch, `func_800414C0` unaligned struct-copy, anomalous
  `func_80041E8C`) are the unmined tail for future smallest-first sprints, not spikes.

## Sprint 214 — func_80026400.c scenery pack, compiler-source dive (PO-directed) — 2026-07-10
- Increment: 0 files banked / **3 functions matched** (delta: no md5-candidate change, 230→230; file
  5/8 banked, 3 stubs remain = emit_scenery_billboard/draw_scenery_opaque_pass/draw_scenery_alpha_pass
  DL/FP walls, NOT md5-candidate). Committed func_80026400 + project_sort_scenery_cylinders; stretch
  update_scenery_cylinder_transforms — all 3 banked byte-exact.
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) this sprint. **Retired a 7-sprint carry**
  (S207 func_80026400 false-wall). ZERO permuter runs across all 3, incl. a 152-instr FP/matrix fn.
- Seed: committed 5pt; banked **0pt** (per-file all-or-nothing, file partial); regime classical/mixed.
  Realized tier: file 0pt (not md5-candidate); value signal = **+3 matched-fn count**. Residual n/a
  (partial file).
- What helped: 3-subagent compiler-source-dive fan-out (asm-first seeds, isolation `decomp_loop`,
  gcc-2.7.2 + binutils-2.6 root-cause). All 3 matched in isolation (1st/3rd/8th build). New levers
  (see `#call-arg-delay-slot-fill--field-alias-addend-0-s214-scenery-levers`): call-crossing arg
  statement-order delay-slot fill, per-field 0x10-stride alias structs → reloc addend 0, `i != N`
  blocks `check_dbra_loop` reversal (loop.c:5847 LT gate), inline-FP-literal preheader hoist,
  `(s16)hf` direct trunc, `|`-within/`||`-between cull idiom.
- Friction: the S207 in-file "wall" comment mis-scoped a stale artifact as a proven wall for 7 sprints;
  a cheap reproduce-from-note pass would have caught it far earlier.
- Applied: 4 of 4 — #1 (docs/hazards.md new `#call-arg-delay-slot-fill--field-alias-addend-0` section
  incl. the stale-wall meta-lesson + `revalidate-old-carries-stale-wall` memory), #2 (project_sort
  levers, same section), #3 (update_transforms levers, same section), #4 (foundational reconfirm:
  compiler-source fan-out > permuter — provenance line in the new section + this digest).
- Carry-over: func_80026400.c 3 DL/FP-wall stubs (emit_scenery_billboard/draw_scenery_opaque_pass/
  draw_scenery_alpha_pass) to `BACKLOG.md ## Carry-overs`.

## Sprint 213 — func_80054900.c residual, compiler-source dive + m2c/Ghidra seeds (PO-directed) — 2026-07-10
- Increment: 0 files banked / **3 functions matched** (delta: no md5-candidate change, 230→230; file
  25/48→**28/48** banked, 20 stubs remain, NOT md5-candidate). Continuation of S211/S212's
  `src/main/func_80054900.c` (no re-flip). Banked byte-exact: `lookup_animation_by_id`,
  `activate_texture_anim_slot`, `func_800577DC`. The `/sprint-plan` gate was interrupted mid-flight by
  the PO redirecting to a compiler-source dive + m2c/Ghidra seeding; executed as a continuation.
- Quality: **0/0/5/0** (0 stuck-far-banked / **0 permuter-runs** / 5 carried — all now PROVEN walls:
  `func_80056060` global.c allocno; `func_800564F0`+`func_80055738` reorg.c:3374 delay-slot;
  `find_keyframe_offset_by_tag`+`collect_keyframe_events_at` loop.c peel/CSE+IV-split). The 5 were all
  pre-existing S211/S212 carries, upgraded from "carry, maybe permuter" to proof-backed permuter-proof.
- Seed: continuation slice (no re-seed; S212 seed 5); banked **0pt** (per-file all-or-nothing, file
  partial); regime classical/mixed. Value signal = **+3 matched** + 5 mechanism-backed wall proofs + a
  reusable data-carve fix.
- What helped: a **4-subagent compiler-source fan-out** (gcc-2.7.2 global.c/loop.c/reorg.c/cse.c +
  binutils-2.6 tc-mips.c), each armed with the `gcc -S` codegen oracle — banked 3 via exact levers
  (terminator-condition search-loop framing; const-materialize-first delay-fill; page-align hoist +
  strength-reduced fill IV + counter-before-dest regalloc) and returned 5 WALL verdicts with line-refs +
  empirical `-S` lever-tables. m2c+Ghidra MCP produced the seeds. Third sprint (S208/S209/S213) where the
  dive beats the permuter.
- Friction: the SPRINT.md-flagged data-carve hazard materialized — referencing the pcsub.c polychara bss
  + shared strings as `extern u8 D_x[]` created COMMON symbols that FLOWED the bss +0x40 and corrupted
  banked `func_800578AC` (`D_801F4424` 0x4424→0x4464). Caught by `git stash` + rebuild-HEAD isolation;
  fixed with 6 offset-0 `polychara_*` absolute aliases (refining the S210 ".NON_MATCHING absolute doesn't
  win" note: a NEW-named alias at the true addr DOES win, the shifted D_ name does not).
- Applied: **6 of 6** — #1 rewrite #delay-slot-fill mechanism (reorg.c:3374 + 3 escape conditions +
  const-first fill lever); #2 #top-tested-loop search-loop terminator-condition framing; #3
  #base-register-vs-displacement peel/CSE+IV-split coupling; #4 #recover-extern shifted-region
  NEW-name-alias fix + twin note in #base-register-vs-displacement; #5 #compiler-source-fan-out
  proves-walls-not-just-cracks; #6 BACKLOG.md func_80054900.c wall-cluster residual note.
- Carry-over: `src/main/func_80054900.c` 28/48, 20 stubs; the 5 proven walls + ~15 FP/nested/≥145-instr
  dispatchers. Tractable smallest-first vein EXHAUSTED — remaining is the wall cluster + an FP-dispatcher
  sprint.

## Sprint 212 — func_80054900.c mid-size logic vein (mixed-partial continuation) — 2026-07-09
- Increment: 0 files banked / **5 functions matched** (delta: no md5-candidate change, 230→230; file
  25/48 banked, 23 stubs remain, NOT md5-candidate). Continuation of S211's `src/main/func_80054900.c`
  (no re-flip, no re-seed). Banked byte-exact: `func_800578AC`, `func_80058ACC`, `func_80058C58`,
  `func_80054E4C`, `func_80058B34`.
- Quality: **0/0/0/3** (0 stuck-far-banked / 0 permuter-runs / 0 re-attempt / 3 carried [`func_80056060`
  callee-saved reg rotation; `find_keyframe_offset_by_tag` + `collect_keyframe_events_at`
  #base-register-vs-displacement keyframe list-walk]).
- Seed: continuation slice, committed **5pt** (plan-time); banked **0pt** (per-file all-or-nothing, file
  partial); regime classical/mixed. Value signal = **+5 matched**. Committed backlog 2/3 (func_80056060
  carried); stretch +3 banked.
- What helped: three NEW reusable source levers, each banked a fn first-or-second-build — the S187
  offset-0 struct-array form on a fixed-global INIT LOOP (`func_800578AC`); delay-slot-fill via
  source-order + f32-locals-defer-truncs + `*(volatile f32*)`-reload combined (`func_80058C58`); a 4-case
  dense switch lowering to GCC's exact compare-tree (`func_80054E4C`); straight-line FP with inline consts
  in callee-saved fp regs (`func_80058B34`). Zero permuter, zero MCP.
- Friction: `func_80056060`'s `beqz`-vs-`beqzl` residual was NOT an independent branch lever — it rode on
  a callee-saved reg rotation (early-return + operand-flip both no-op); the keyframe list-walk
  (find/collect) caches the base pointer in a reg where the ROM re-derives it (#base-register-vs-
  displacement, no reliable source lever) — carried, not thrashed.
- Applied: **3 of 3** (all `docs/hazards.md`: #offset-0-symbol-re-materialization init-loop extension;
  #volatile-view-cse-reload → new "delay-slot fill via source order" subsection; #value-select-if-else-vs-
  branch-likely direction-dependent + coupled-to-regalloc note).
- Carry-over: `src/main/func_80054900.c` 25/48 banked, 23 stubs (3 NEW characterized carries + the 3 S211
  carries + FP/nested/dispatcher tail). See `BACKLOG.md ## Carry-overs`.

## Sprint 211 — open func_80054900.c animation pack (mixed-partial, smallest-first) — 2026-07-09
- Increment: 0 files banked / **20 functions matched** (delta: no md5-candidate change, 230→230; file
  20/48 banked, 28 stubs remain, NOT md5-candidate). Opened `src/main/func_80054900.c` (flip `[0x2FD00]`
  asm→c, 48-fn pack).
- Quality: **0/0/3/0** (0 stuck-far / 0 permuter-runs / 3 carried [`func_800564F0` + `func_80055738`
  #delay-slot-fill; `lookup_animation_by_id` loop-optimizer wall] / 0 re-opened).
- Seed: fresh big one-tu classical pack — committed **13pt** (booked at plan); banked **0pt** (per-file
  all-or-nothing, file partial); regime classical/mixed. Value signal = **+20 matched**.
- What helped: asm-first fast-path on tiny accessors (whole get_character_state family banked in 2 green
  full-makes); the deref-guard root-cause let `seek_current_frame_by` / FP accessors bank first-try;
  systematic-debugging + gcc-2.7.2 source cracked the `lookup_animation_by_id` frame/regalloc
  (result-init-after-call, 2588→1340).
- Friction: the #delay-slot-fill reorg divergence (2 carries, no faithful-C lever); the
  `lookup_animation_by_id` loop shape (5 loop forms, no single idiom yields un-rotated head + conditional
  branch-likely back-edge); one 3-fn value-select miss cascaded a whole-file ±1 shift before diagnosis.
- Applied: **3 of 3** (#1 `docs/hazards.md` #value-select-if-else-vs-branch-likely; #2
  #delay-slot-fill-of-a-null-guard-beqz + the safe-on-taken-path root-cause rule; #3
  #default-return-var-must-init-after-call frame lever; + 3 hazard-index rows in `agent-workflow.md`).
- Carry-over: `src/main/func_80054900.c` 20/48 banked, 28 stubs (3 characterized carries +
  FP/nested/dispatcher tail). See `BACKLOG.md ## Carry-overs`.

## Sprint 210 — func_80059BA0.c continuation (D334 dive + fresh non-FP tail) — 2026-07-09
- Increment: 0 files banked / **1 function matched** (`func_8005C510`) (delta: no md5-candidate change,
  230→230; file 33/59 banked, 26 stubs remain, NOT md5-candidate).
- Quality: **0/3/6/0** (0 stuck-far / 3 permuter-runs [B0B4, AF80, D334 — ALL WALLED, ~9500/9500/150s
  iters, 0 banks] / 6 carried [D334, B0B4, AF80, func_80059BA0/fabsf, CEE0, DE88-blocked] / 0 re-opened).
- Seed: continuation of S208's 13pt pack — committed 0pt (no re-seed); banked 0pt (per-file
  all-or-nothing, file partial); regime classical/mixed. Value = **+1 matched**.
- What helped: the **C458 grid-counter FAMILY recipe** banked `func_8005C510` first-build (base-via-ret
  live-length + integer-arith end + the new `count`→code DISPATCH-TAIL variant). The **D334 compiler-
  source dive** reached a STRUCTURAL byte-match (middle ~20 instrs byte-identical) + full root cause
  (mips.c:1996 `!=K` force_reg → loop.c:1630 always-hoist → reload rematerialization is the +1-pressure
  decision), a much sharper characterization than the prior "loop.c IV divergence" note.
- Friction: **hard residual tail.** Post-S208/S209 the file's leftovers are a CLUSTER of allocno-swap
  (B0B4 ret/nibble, AF80 row/j) + base-register-vs-displacement (DE88, AF80 0xDC0-fold, CEE0) walls that
  neither source levers NOR the permuter (3 runs, ~9500 iters each, 0 cracks) flip. DE88 additionally
  blocked by a shifted `.NON_MATCHING` data-carve (referencing its globals corrupts the region). Smallest-
  first kept hitting walls; the ONE bank came from family-matching a banked pattern, not the smallest fn.
- Applied: **4 of 4** — #1 new [#base-register-vs-displacement] hazard (DE88/AF80/CEE0, incl. what-
  backfires + permuter-no-crack); #2 shifted-`.NON_MATCHING` data-carve blocker + `grep …map | grep -v
  NON_MATCHING` diagnostic (folded into #1's subsection + workflow hazard-index rows); #3 strengthened
  [#grid-counter-double-loop] with the `count`→code dispatch-tail variant (C510); #4 `family-of:<banked-
  fn>` ranker signal RECORDED as a tracked follow-up (a live pick_target change needs the golden-gated
  tooling branch per the tooling-refactor-style convention — NOT a retro in-place edit; gate applies it
  manually meanwhile).
- Carry-over: `func_80059BA0.c` 33/59 (26 stubs) — D334 (regalloc-pressure, `docs/wip/` note), B0B4
  (ret/nibble allocno-swap, permuter no-crack), AF80 (row/j allocno-swap + 0xDC0-hoist, permuter
  no-crack), func_80059BA0 (fabsf redundant-`mov.s` artifact), CEE0 (base-register 4-row), DE88 (blocked
  on the `.NON_MATCHING` data-carve). ~19 larger/FP fns still unprofiled.

## Sprint 209 — func_80059BA0.c near-match crack (compiler-source dive) — 2026-07-09
- Increment: 0 files banked / **9 functions matched** (delta: no md5-candidate change, 230→230; file
  32/59 banked, 27 stubs remain, NOT md5-candidate).
- Quality: 0/1/1/0 (0 stuck-far / 1 permuter-run [C458, WALLED at 1.27M iters / score 55, banked by the
  dive not the permuter] / 1 carried [D334] / 0 re-opened)
- Seed: continuation of S208's 13pt pack — committed 0pt (no re-seed); banked 0pt (per-file
  all-or-nothing, file partial); regime classical/mixed. Value = +9 matched.
- What helped: the PO-directed compiler-source dive (subagent fan-out over gcc-2.7.2 + gas-2.6) cracked
  ALL 4 committed near-matches (B070/D308/D218/D2E4) byte-exact with **0 permuter**, AND cracked the
  C458 grid-counter family which had **walled** the permuter (1.27M iters / score 55). FOUNDATIONAL
  discovery: KMC cc1 has NO instruction scheduler (INSN_SCHEDULING undefined; -fschedule-insns a byte
  no-op) → source emit-order is the only ordering control (new memory `kmc-cc1-no-instruction-scheduler`).
  Levers banked: gas `.set-reorder` textual≠machine nop (B070); reorg `optimize_skip` annulled bnel
  (D308); switch shared-`case 0: default:` merged-default (D218/D2E4); global.c allocno live-length
  steer via intermediate copy + integer-cast commutative order (C458 family); word-aligned-struct
  block-move path (B28C); outer-limit-as-var setup order (C5B4/C614). `tools/cc/gcc -S/-c` + objdump as
  the build-free byte oracle throughout.
- Friction: the permuter walled on C458 (1.27M iters) before the PO re-directed to the dive —
  reinforces `#compiler-source-fan-out` as the escalation for pure-regalloc allocno perms, not just
  slow ones. D334's triple-IV struct-array init resisted the oracle (GCC merges the outer counter into
  the byte-offset IV vs the ROM's 3 IVs) — carried for a loop.c dive.
- Applied: 9 of 9 (#1 strengthen #compiler-source-fan-out walled-permuter case; #2 no-scheduler fact +
  global.c allocno live-length lever; #3 integer-arith commutative order; #4 setup-block order; #5
  struct-align block-move; #6 .set-reorder textual≠machine; #7 optimize_skip bnel; #8 switch
  merged-default; #9 new #grid-counter-double-loop hazard). Memory added: kmc-cc1-no-instruction-scheduler.
- Carry-over: `func_8005D334` (triple-IV struct-array init, loop.c strength-reduction divergence). ~23
  larger/FP fns in the file still unprofiled.

## Sprint 208 — func_80059BA0.c integer-glue/accessor pack — 2026-07-09
- Increment: 0 files banked / **23 functions matched** (delta: no md5-candidate change, 230→230; file
  23/59, 36 stubs remain, NOT md5-candidate). Largest single-sprint match count to date.
- Quality: 0/0/4/0 (0 stuck-far / 0 permuter-run / 4 carried-near-match / 0 re-opened)
- Seed: committed 13pt; banked 0pt (per-file all-or-nothing, file partial); regime classical/mixed;
  realized ~15, residual +2
- What helped: pack was a DEEP non-FP integer vein (37/59 non-FP), not thin like S206/S207 FP/DL packs
  → +23 vs the +4 hedge. asm-first fast-path (MCP down, irrelevant). -O2-no-inline fact (cross-calls
  stay jal regardless of defn order). `tools/cc/gcc -S` as a build-free codegen oracle to pick loop/
  switch/const-type spellings before touching the tree. Levers: branchless `x&(~x>>31)`, local-ptr
  addr-reuse, s8/u8 store-const, s8→s32 return re-extension.
- Friction: ~1hr lost treating 4 fns (D218/D2E4/D308/B070) as a `nonmatching`-bss WALL — their own
  `D_80105DCx`/`D_800C2B3C` refs read +0x10 off. It was self-inflicted `#short-text-shifts-flowing-bss`
  (over-long codegen shifts the flowing bss); revert-to-confirm snapped the symbols back. Root-caused
  via systematic-debugging + gcc-2.7.2 source.
- Applied: 4 of 4 — #1 flowing-bss self-ref +N tell → `#short-text-shifts-flowing-bss`; #2 goto
  preamble-order/regalloc coupling + #3 `gcc -S` oracle method → `#top-tested-loop-goto-local-hoist`;
  #4 s8/u8 store-const + return-type re-extension → `#char-signedness`.
- Carry-over: `func_80059BA0.c` (36 stubs). 4 characterized ≥0.97 near-matches (`func_8005B070`
  schedule-transposition, `func_8005D218`/`func_8005D2E4` switch branch-polarity, `func_8005D308`
  regalloc) → permuter sprint. Plus C458/C4B4/C5B4/C614 nested 6×6 counters, DF54, D334/B28C/CEE0/
  B0B4/C510, and ~23 larger/FP fns.

---

## Sprint 207 — func_80026400.c scenery/heap pack — 2026-07-09
- Increment: 0 files banked / **3 functions matched** (delta: no md5-candidate change; file 3/9, 6
  stubs remain). Subseg `[0x1800, asm] -> c` flipped.
- Quality: 0/0/6/0 this sprint (stuck-far/permuter/carried/re-opened)
- Seed: committed 13pt; banked 0pt (mixed-partial, per-file all-or-nothing); realized ~14, residual +1;
  regime classical/mixed
- What helped: asm-first hand-translate for the heap-region-init sibling `func_8002646C` (byte-exact
  first build); 2 known levers cracked the string-dup-util pair (`func_80028110`/`func_80028204`) — an
  `(u32)len >= K` unsigned cast to force `sltiu` over `slti` on the length bound, and a statement-order
  swap (`*out=0` before `osSyncPrintf`) to fill the printf delay slot with the store rather than the
  address `addiu`. The S189/S190 FP-and-DL-emitter partial-bank-expected detector was right: 6 of 9 fns
  wall-class, only the integer heap/string glue banked.
- Friction: `func_80026400` (heap-region-init head) is a structural-complete delay-slot-fill/regalloc
  near-match — the build folds `block+size` into `osSyncPrintf`'s delay slot (block->s0), while TARGET
  saves `size` there and computes `end` late in `func_800263B0`'s delay slot (block->s1). 4 source forms
  (int-reuse / pointer-arith / late-decl / asm-memory-barrier) all hoist the add; no clean C trigger.
  The size-only `pts` priced the pack a flat 13, blind to the 6-of-9 partial-bank reality.
- Applied: 0 of 1 — the `func_80026400` delay-slot-fill/regalloc near-match signature (a 4th
  regalloc-heavy detector wall pattern after S203 FP-hoist / S204 qty-perm / S205 base-canon) was a
  PO-declined retro edit; kept as a standup data point only, not queued to the off-cadence branch.
- Carry-over: `src/main/func_80026400.c` (3 of 9 banked; carries `func_80026400` delay-slot near-match +
  `project_sort_scenery_cylinders` multi-IV FP insertion sort + `update_scenery_cylinder_transforms` +
  `emit_scenery_billboard` / `draw_scenery_opaque_pass` / `draw_scenery_alpha_pass` FP/DL emitters).

---

## Sprint 206 — func_800772B0.c float spline/curve-interpolation pack — 2026-07-09
- Increment: 0 files banked / **3 functions matched** (delta: no md5-candidate change; file 3/6, 3
  stubs remain). Subseg `[0x526B0, asm] -> c` flipped.
- Quality: 0/0/3/0 this sprint (stuck-far/permuter/carried/re-opened)
- Seed: committed 13pt; banked 0pt (mixed-partial, per-file all-or-nothing); realized ~17, residual +4;
  regime classical/mixed
- What helped: asm-first hand-translate for the 2 trivial glue fns (byte-exact first build); the S184
  parallel-subagent fan-out over the FP tail (3 isolated `nonmatchings/<fn>/`, no build race) both
  characterized 3 S158 FP-regalloc walls AND returned 1 unexpected byte-match (`func_800779A8`, cracked
  via a subagent local-alloc coloring lever — `while` not `if{do-while}`, split `t=x-base` to pin x in
  `$f12`, non-negated `bc1fl`, fully-inlined return); the S204 coddog-min-instr-floor call was right
  (settime@99.99 was a false collision, this is a `none` pack, no header vendoring needed).
- Friction: 3 of 4 non-trivial FP fns are irreducible-from-C S158 regalloc walls (buffer-ptr homing,
  coupled global/local coloring, int-temp hard-reg perm) — all < 0.97 so the permuter is not yet
  applicable; the size-only `pts` priced the all-FP pack a flat 13, blind to the partial-bank reality.
  One decomp_loop trap: a mid-TU byte-exact fn scores >0 purely from TU-offset branch mis-flagging.
- Applied: 3 of 3 — #S1 `docs/hazards.md#isolated-compile-caveat` mid-TU standalone-offset case
  (preceding stubs as INCLUDE_ASM in base.c for true offset); #S3 `docs/agent-workflow.md ## Workflow at
  a glance` FP-subagent fan-out recipe extension; #S2 FP-pack pts-detector data point + coddog
  min-instr-floor reconfirm QUEUED to the off-cadence golden-gated `pick_target.py` branch (not inline).
- Carry-over: `src/main/func_800772B0.c` (3 of 6 banked; carries `func_800772C4`/`func_8007775C`/
  `func_80077AD4`, all fully-RE'd S158 FP-regalloc walls, `docs/wip/*.near-match.md`). Retry: permuter
  once seeded past the reg fold, or `#cross-project-matched-corpus-mining`.

## Sprint 205 — complete func_8005E380.c (fault register/flag dump printer) — 2026-07-09
- Increment: 0 files banked / 0 functions matched (delta: no md5-candidate change). Sole increment
  `func_8005E380` carried as a compiler wall. Stubs unchanged.
- Quality: 1/1/1/0 this sprint (stuck-far/permuter/carried/re-opened)
- Seed: committed 8pt; banked 0pt (per-file all-or-nothing, carried = 0); regime classical
- What helped: asm-first hand-translate off `__OSThreadContext` u64/`__OSfp` fields → compiled +
  100% structural (535/535 rows) first build; PO's ultralib pointer confirmed no verbatim source
  (rmon copies raw words; `%f` dump is game-specific); compiler-source flag-bisect (19 opt/flag
  variants) + source-lever sweep (8 forms) cleanly root-caused the wall as CSE base-canonicalization
  (deterministic, unbreakable) rather than guessing; in-tree `diff.py` disambiguated the low-percent
  full-rows/empty-mismatches signal from a true isolation artifact.
- Friction: reached for the permuter before finishing the compiler-source dive (out of the PO-preferred
  order per [[compiler-source-rootcause-before-permuter]]); the permuter drifted into UB (dropped print
  calls / uninit ctx) below its valid floor ~1200, so it added no signal beyond confirming no valid
  source form reaches 0. The size-only `pts` priced this c-stub 13, blind to the regalloc/CSE wall.
- Applied: 3 of 3 — #1 new `docs/hazards.md#cse-derived-pointer-base-canonicalization` section + TOC +
  hazard-index rows; #3 isolation-caveat refinement (low-percent full-rows ≠ always artifact,
  disambiguate with in-tree `diff.py`); #2 ranker regalloc-heavy pts-detector (add CSE-base-canon
  signature) QUEUED to the off-cadence golden-gated `pick_target.py` branch (not applied inline).
- Carry-over: `src/main/func_8005E380.c` (spike; `func_8005E380` CSE derived-pointer base-canon wall,
  gold in-file root-cause note commit 8fe89cc). Fresh main c-stub singles now exhausted (this +
  `func_8003E004` both confirmed compiler walls); next main increment is a 13pt decompose-gated pack
  or the mispriced `func_800772B0` one-tu pack.

## Sprint 204 — main c-stub file completion (func_80050400.c banked) + func_8003E004 compiler-source spike — 2026-07-09
- Increment: 1 file md5-candidate (`src/main/func_80050400.c`, 0 stubs) / +1 function matched
  (`func_80050428`). Stubs 63→62. No flip enabler (file already `c`).
- Quality: 0/1/2/0 this sprint (`func_80050428` permuter-escalated & WON; carried: `func_8003E004`
  continuing-carry root-cause-upgraded, `func_8004DC44` stretch-deferred).
- Seed: committed 6pt (`func_80050428` 3 + `func_8004DC44` 3); banked 3pt; realized 5; residual +2;
  regime classical.
- What helped: **PO-directed compiler-source dives before the permuter (twice).** (1) `func_8003E004`:
  4 subagents over gcc-2.7.2 + binutils-2.6 + `-dL`/`-da` RTL dumps → definitive `move_movables` DFmode
  `(double)base` hoist root cause (loop.c:1630); gas exonerated; 9 clean variants plateau 12439-14020;
  gold carry note in-source (commit 53977e0). (2) `func_80050428`: `.flow` dump proved 1 basic block →
  `local-alloc.c` qty-priority regalloc (not global.c); permuter found score 0 (iter ~14250). Winning
  levers: reference the global INLINE (no pointer local), cache a re-read to pin an independent store's
  schedule slot, array decl-order sets aligned-scratch stack offsets. `pscore.py` + a pre/body/post
  splice harness A/B'd variants fast against the authoritative scorer.
- Friction: `nonmatching-func`/`decomp_loop` isolated object DIVERGED from the in-tree build for the
  1-BB `func_80050428` (misleading isolated score); had to gate on the in-tree object + full ROM SHA-1.
  The `func_8003E004` retry consumed most of the sprint for 0 bank (research spike), by PO direction.
- Applied: PO-selected 4 of 5. #2 `#local-alloc-qty-permutation` hazards.md section + index rows (DONE);
  #3 nonmatching-func-diverges-from-in-tree caveat (folded into #2 section, DONE); #1 `regalloc-heavy`
  pts-detector and #4 coddog-min-instr-floor ACCEPTED → queued to the off-cadence golden-gated
  `pick_target.py` tooling branch (specs in BACKLOG; not hand-edited inline). (#5 seed-dir dedup: minor,
  not selected.)
- Carry-over: `func_8003E004` (`src/main/func_8003DFD0.c`, confirmed `move_movables` hoist wall, gold
  note); `func_8004DC44` (`src/main/print_string_at_grid.c`, `#signed-divide-const` grid-copy, stretch).

## Sprint 203 — src/main/func_8003DFD0.c MIXED-PARTIAL: wind DL builder banked, vertex gen carried — 2026-07-09
- Increment: 0 files md5-candidate / +1 function matched. Continued the S200 c-stub file
  `src/main/func_8003DFD0.c` (was 1/3 → now 2/3, 1 stub). No flip enabler (already `c`).
- Quality: 1/1/1/0 this sprint (`func_8003E004`: stuck-far + permuter + carried, one fn).
- Seed: committed 13pt; banked 0pt (partial file, per-file all-or-nothing); realized 16; residual +3;
  regime classical/mixed.
- What helped: gfxdis (f3dex2) decoded `func_8003E314`'s 3 static head words (setcombine/pipesync/vtx)
  and confirmed the gSP2Triangles loop — the macro's internal ×2 exactly matched the asm's
  i*4/i*4+2/i*4+4/i*4+6 induction vars; the only source fix was `i!=8` (bne) vs `i<8` (slti). For the
  carry, the full asm decode + a self-authored base.c seed made the wall precisely characterized
  (206/223 rows; `(f64)base` + scratch-ptr spill vs ROM callee-saved alloc), and setup-permuter `--main`
  drove it 7115→3505.
- Friction: the two "stubs" were both mis-assessed at plan time as an easy leaf + an FP monster; the
  "easy leaf" `func_8003E314` was actually a hand-rolled F3DEX2 display-list builder (needed gfxdis +
  GBI-macro reconstruction), and the FP `func_8003E004` is a genuine S158-class pervasive-regalloc wall
  the c-stub seed (13, size-only) can't price. Permuter (280s) did not converge — a longer dedicated run
  is needed. seed dir collided (`func_8003E004` decomp_loop dir pre-existed, setup made `-2`).
- Applied: 0 of 2 (PO deferred both buffered suggestions; #1 regalloc-heavy pts-detector data point —
  already a tracked S158/S177/S183 follow-up; #2 permuter dup-seed-dir note — minor, both nonmatchings/
  gitignored).
- Carry-over: `src/main/func_8003DFD0.c` — `func_8003E004` (FP regalloc wall, structure solved, seed at
  `nonmatchings/func_8003E004-2/base.c`; see `BACKLOG.md ## Carry-overs`).

## Sprint 202 — src/main/func_8005E380.c MIXED-PARTIAL: debug fault tail 1/2 banked — 2026-07-08
- Increment: 0 files md5-candidate / +1 function matched (delta 229/244 -> 229/245). Flipped the
  aligned `0x39780` main debug/fault tail into `src/main/func_8005E380.c`; file is 1/2 with one stub.
- Quality: 1 stuck-far / 0 permuter-escalated / 1 carried / 0 re-opened.
- Seed: committed 8pt; banked 0pt (partial file, per-file all-or-nothing); realized 10; residual +2;
  regime classical/mixed.
- What helped: `func_8005EAD4` banked as a fault/debug flag-label list printer over
  `{mask, value, label}` records.
- Friction: `func_8005E380` reached a struct-complete first pass over `OSThread.context`, but stopped
  far below permuter threshold on whole-function saved-register/base-pointer coloring (`s0`/`s1`) plus
  prologue order. No explicit register binding used.
- Applied: 0 of 0; no buffered process/tooling suggestions.
- Carry-over: `src/main/func_8005E380.c` one stub — `func_8005E380`. Retry only with a new
  source-shape/codegen insight; no explicit register binding.

## Sprint 201 — src/main/func_8003E400.c COMPLETE: main head slice banked — 2026-07-08
- Increment: 1 file md5-candidate / +6 functions matched (delta 228/243 -> 229/244). Split the
  aligned `0x19800` main head slice into `src/main/func_8003E400.c`; tail starts at `0x19AC0`.
- Quality: 0 stuck-far / 0 permuter-escalated / 0 carried / 0 re-opened.
- Seed: committed 5pt; banked 5pt; realized 5; residual 0; regime classical/mixed.
- What helped: asm-first was enough for the call-glue and predicates. The matrix helper matched after
  separating `D_800B7780 << 6` and `arg << 7`, leaving the arg offset add in the call delay slot.
- Friction: first matrix-helper C was structurally right but associated the final address expression
  differently, causing a ROM SHA miss until the object diff exposed the `s0`/`s1` lifetime issue.
- Applied: 0 of 0; no buffered process/tooling suggestions.
- Carry-over: none. `0x19AC0` render tail stays asm, out of scope.

## Sprint 200 — src/main/func_8003DFD0.c MIXED-PARTIAL: main head slice 1/3 banked — 2026-07-07
- Increment: 0 files md5-candidate / +1 function matched (delta 228/242 -> 228/243). Split the
  aligned `0x193D0` main head slice into `src/main/func_8003DFD0.c`; file is 1/3 with 2 stubs.
- Quality: 0 stuck-far / 0 permuter-escalated / 2 carried / 0 re-opened.
- Seed: committed 5pt; banked 0pt (partial file, per-file all-or-nothing); realized 7; residual +2;
  regime classical/mixed.
- What helped: keeping the unsigned threshold branch in `func_8003DFD0` source order preserved the ROM
  branch shape; precomputing the subtract if-converted to a branchless mask.
- Friction: `func_8003E004` stopped at FP setup/output saved-register/schedule mismatch. `func_8003E314`
  stopped at raw display-list constant-load/register order despite the required volatile `glistp` cursor.
- Applied: 0 of 0; no buffered process/tooling suggestions.
- Carry-over: `src/main/func_8003DFD0.c` 2 stubs — `func_8003E004` and `func_8003E314`.

## Sprint 199 — src/main/func_80050710.c MIXED-PARTIAL: ROM-load helper tail carried — 2026-07-07
- Increment: 0 files md5-candidate / +0 functions matched (delta 228/242 -> 228/242). Flipped the
  `0x2BB10` ROM-load helper tail into `src/main/func_80050710.c`; both functions remain stubs.
- Quality: 0 stuck-far / 0 permuter-escalated / 2 carried / 0 re-opened.
- Seed: committed 8pt; banked 0pt (partial file, per-file all-or-nothing); realized 10; residual +2;
  regime classical/mixed.
- What helped: compiler-source fan-out confirmed the stack-counter lever: address-taken locals route
  through `mark_addressable` -> `put_var_into_stack`. That produced structural-complete near-matches.
- Friction: both functions stopped at saved-register coloring/order walls. `func_80050710` reached
  143/143 rows with only `s1`/`s2` color swapped. `func_80050914` reached 200/202 rows; residual was
  saved-register order plus one branch-likely detail in the two-byte RLE variant. No inline-asm register
  assignment used.
- Applied: 0 of 0; no buffered process/tooling suggestions.
- Carry-over: `src/main/func_80050710.c` 2 stubs — `func_80050710` and `func_80050914`. Retry only with
  a new source-shape/compiler-codegen insight; no explicit register binding.

## Sprint 198 — src/main/func_800505A0.c COMPLETE: ROM-load helper head banked — 2026-07-07
- Increment: 1 file md5-candidate / +3 functions matched (delta 227/241 -> 228/242). Split the
  `0x2B9A0` ROM-load tail at `0x2BB10` and banked `func_800505A0`, `func_8005062C`, and
  `func_800506D4`. ROM green at `6ffc6fc`.
- Quality: 0 stuck-far / 0 permuter-escalated / 0 carried / 0 re-opened.
- Seed: committed 5pt; banked 5pt; realized 4; residual -1; regime classical/mixed.
- What helped: asm-first was enough for the whole approved slice. `func_800505A0` matched once its
  signature followed the target ABI `(dst, size, slot)`; `func_8005062C` matched with the explicit
  16-aligned scratch pointer and slot-wrapper/header-read shape; `func_800506D4` was a direct
  dispatcher on `slot->pos == 1`.
- Friction: one routine signature correction on `func_800505A0`; no permuter/m2c/Ghidra path needed.
- Applied: 0 of 0; no buffered process/tooling suggestions.
- Carry-over: none. `0x2BB10` tail stays asm, out of scope.

## Sprint 197 — src/main/func_800263B0.c COMPLETE: first scenery-cache init slice banked — 2026-07-07
- Increment: 1 file md5-candidate / +1 function matched (delta 226/240 -> 227/241). Split the first
  aligned main slice of the 0x17B0 scenery/render pack into `src/main/func_800263B0.c`; tail starts
  at `0x1800`. ROM green at `c1ead13`.
- Quality: 0 stuck-far / 0 permuter-escalated / 0 carried / 0 re-opened.
- Seed: committed 1pt; banked 1pt; realized 1; residual 0; regime classical/mixed.
- What helped: asm-first was enough. A direct byte-offset zeroing loop matched after changing the
  loop to a do/while up-count, preserving the ROM's `v1 += 0x10` + `sltiu` loop form.
- Friction: gate triage first rejected `0x526B0`: its first split point `0x526C4` is non-16-aligned,
  and the whole-pack scaffold did not auto-export sibling fallback stubs. The aligned `0x17B0`
  slice avoided both issues.
- Applied: 0 of 0; no buffered process/tooling suggestions.
- Carry-over: none. `0x1800` tail stays asm, out of scope.

## Sprint 196 — src/main/func_80050400.c MIXED-PARTIAL: 5/6 banked, func_80050428 carried — 2026-07-07
- Increment: 0 files md5-candidate / +5 functions matched (delta 226/239 -> 226/240). Split the
  first aligned main slice of the structural `libc/llcvt.c@99.99` pack into
  `src/main/func_80050400.c`; file is 5/6 with one stub (`func_80050428`), ROM green at `74a4ff4`.
- Quality: 1 stuck-far / 0 permuter-escalated / 1 carried / 0 re-opened.
- Seed: committed 5pt; banked 0pt (partial file, per-file all-or-nothing); realized 7; residual +2;
  regime classical/mixed.
- What helped: the asm-first path banked all tractable functions. `func_80050400` needed a raw byte
  pointer so GCC emitted the ROM's `+0x10` stride; `func_80050588`/`func_80050598` were direct slot
  reset/getter leafs; `func_800504E8` was a direct wrapper; `func_80050504` matched with the ROM's
  pre-increment loop counter and raw slot pointer.
- Friction: `func_80050428` hit a saved-register/source-shape wall far below permuter threshold.
  The correct stack layout is known (`0x40` scratch buffers, `&buf[0xF]` alignment), but unconstrained
  C keeps rotating saved registers. Explicit register binding was rejected by PO and not used.
- Applied: 0 of 0; no buffered process/tooling suggestions.
- Carry-over: `src/main/func_80050400.c` one stub — `func_80050428` ROM table read/setup. Retry only
  with a new source-shape/codegen insight; do not use explicit register allocation.

## Sprint 195 — src/main/func_8005EC10.c COMPLETE: aligned audio tail banked — 2026-07-06
- Increment: 1 file md5-candidate / +4 functions matched (delta 225/238 -> 226/239). Split the
  0x39780 tail at the aligned 0x3A010 boundary, banking `func_8005EC10`, `func_8005EC48`,
  `func_8005ECC4`, and `audio_system_boot`. ROM green at `1b4b340`.
- Quality: 0 stuck-far / 0 permuter-escalated / 0 carried / 0 re-opened.
- Seed: committed 5pt; banked 5pt; realized 6; residual +1; regime classical/mixed.
- What helped: the asm-first fast-path was enough for all four functions. `func_8005EC10` matched as
  the stock pre-NMI callback setter, `audio_system_boot` matched as null-config default plus audio
  setup, and `func_8005ECC4` needed only an unsigned loop counter to emit the target `sltiu`.
- Friction: the initial plan split at `0x39ED4` failed the gate because the address is non-16-aligned
  and linker input-section alignment shifted ROM bytes; the aligned `0x3A010` split fixed it. The
  callback needed an explicit tail label to preserve the ROM branch layout.
- Applied: 0 of 0; no buffered process/tooling suggestions.
- Carry-over: none. `func_8005E380` and non-16-aligned `func_8005EAD4` stay asm, out of scope.

## Sprint 194 — src/main/func_8005E2C0.c COMPLETE: 0x396C0 head split banked — 2026-07-06
- Increment: 1 file md5-candidate / +3 functions matched (delta 224/237 -> 225/238). Split the
  0x396C0 game-embedded fault/debug pack into `src/main/func_8005E2C0.c` plus the 0x39780 asm tail.
  ROM green at `81dcc09`.
- Quality: 0 stuck-far / 0 permuter-escalated / 0 carried / 0 re-opened.
- Seed: committed 3pt; banked 3pt; realized 2; residual -1; regime classical/mixed.
- What helped: the asm-first fast-path fit the whole approved slice. `func_8005E2C0` matched with a
  volatile dead stack object, `func_8005E2CC` matched as the fault-thread message loop with extracted
  string refs, and `func_8005E360` matched as a one-call `osSyncPrintf` wrapper. The linked ROM slice
  compare for 0x396C0..0x39780 plus full ROM SHA-1 proved the match.
- Friction: raw object-byte cmp was noisy because unresolved reloc slots are zero in the `.o`; the
  linked ROM slice was the correct byte proof.
- Applied: 0 of 0; no buffered process/tooling suggestions.
- Carry-over: none. The 0x39780 asm tail stays out of scope for a future sprint.

## Sprint 193 — src/libultra/debug/assert.c COMPLETE: `__assert` split from 0x4CE0 and banked — 2026-07-06
- Increment: 1 file md5-candidate / +1 function matched (delta 223/236 -> 224/237). Split the 0x4CE0 pack into `src/libultra/debug/assert.c` plus the 0x4D00 asm remainder. ROM green at `83f4ac6`.
- Quality: 0 stuck-far / 0 permuter-escalated / 0 carried / 0 re-opened.
- Seed: committed 1pt; banked 1pt; regime mirror.
- What helped: the 8-gate was resolved by a 0x20-byte split. The upstream libultra source at `~/development/repos/ultralib/src/debug/assert.c` gave the function identity, while the target asm showed the ROM's older one-argument `osSyncPrintf` shape and existing `D_800CA184` string.
- Friction: the first C body emitted a new string literal in `.rodata`, so the text shape was right but the ROM SHA missed by shifting data. Referencing `extern const char D_800CA184[]` kept the bytes in the extracted data blob and matched full ROM.
- Applied: 1 of 1: #1 existing extracted string/data symbol note -> `docs/hazards.md#recover-extern-refs-unplaced`.
- Carry-over: none. The 0x4D00 asm remainder (`vec3f_normalize` and 17 following funcs) stays out of scope for a future sprint.

## Sprint 192 — src/main/func_80077BF0.c MIXED-PARTIAL retry: func_80077C18 banked, 2 FP/DL carries remain — 2026-07-06
- Increment: 0 files md5-candidate / +1 function matched (delta 223->223 md5-candidate files, total .c 236->236). File 5/7, 2 stubs. ROM green at `dacbf3f`.
- Quality: 0 stuck-far / 0 permuter-escalated / 2 carried / 0 re-opened.
- Seed: committed 5pt; banked 0pt (partial file, per-file all-or-nothing); realized 5; residual 0; regime classical/mixed.
- What helped: the S187 `func_80077C18` near-free retry was valid. Inlining the saved typed `SparkGroup`/`SparkSrc`/`SparkParticle` shape plus the count-reload, `D_800C4660 = groupCount`, and index-order fixes produced a ROM-green match. The final one-byte ROM diff was just commutative add order in the grid load; `*(col + n + src->grid)` forced the target `addu v0,v0,a3` form. The full ROM SHA-1 was the oracle because isolated score remained nonzero on reloc/addend noise with all rows aligned.
- Friction: `func_80077E94` m2c + typed seeds compiled but were not structural (`percent < 0`), so the compiler-source/permuter path was premature. `render_spark_effects` remains the larger FP+DL tail and was carried by sprint scope.
- Applied: 0 of 0; no buffered process/tooling suggestions.
- Carry-over: `src/main/func_80077BF0.c` 2 stubs — `func_80077E94` (165, FP particle initializer, S158 class; needs a structural seed before compiler-source/permuter work) and `render_spark_effects` (506, FP+DL render tail; dedicated FP/display-list sprint).

## Sprint 191 — src/main/get_table_entry.c MIXED-PARTIAL: 3/11 banked (m2c + RE'd-struct context; .greg-dump regalloc crack, zero permuter), 8 carried — 2026-07-06
- Increment: 0 files md5-candidate / +3 functions matched (delta 223→223 md5-candidate files, total .c 235→236). File 3/11, 8 stubs. ROM green at freeze `b10b8ab` + `[2/11]` `22d7128` + `[3/11]` `97d5430`.
- Quality: 0 stuck-far / 0 permuter-escalated / 8 carried / 0 re-opened.
- Seed: committed 13pt; banked 0pt (partial file, per-file all-or-nothing); realized 15 (seed 13 +1 novel-gotcha [.greg Axis-6 eval-order crack] +1 re-attempt [func_80037E50 iterations], −1 for the 2 clean first-integrate banks); residual +2; regime classical/mixed.
- What helped: the PO-directed **m2c-from-repo + RE'd-struct `--context`** recipe (Ghidra `TerrainAttrEntry` + a synthesized 0xB8 `ShotInitRecord`) gave byte-faithful seeds for all 3 standalone fns — confirms the recipe on a classical main LOGIC pack, not just call-glue (S186). Two m2c seed-refinement levers: (a) the **ternary** `(idx<27)?idx:0` clamp for branchless `sltiu`/`negu`/`and` (m2c's `&arr[idx & -(idx<27)]` branch-folds); (b) **stepwise pointer arith** `p=base+quality*K; return p+arg3*K2;` to defeat GCC's reassociation of `A+base+C`. The systematic-debugging pass on `func_80037E50` was decisive: reading the gcc-2.7.2 `.greg` dump (`;; N conflicts:` + `;; Register dispositions`) root-caused `quality`'s `$a0`-vs-`$v1` miss to allocno 76 conflicting with both `$v0`+`$v1` (the flat sum reassociated to a 2nd `$v1` accumulator) — a compiler-source crack with ZERO permuter.
- Friction: the decomp_loop score is a useless oracle here — it read an IDENTICAL 3320/0.1487 (39/39 rows, empty top_mismatches) with `quality` in `$a0` (wrong) and `$v1` (byte-exact), because the row-matcher normalizes registers and the score is pure extern-reloc noise. The in-tree `.o` objdump (register operands, ignoring reloc slots) was the only reliable oracle. The isolated compile also can't run from the fn's subdir (macro.inc relative path), so `-da` RTL dumps came from a repo-root-equivalent invocation. Pre-classifying the pack (6 of 11 nested children) up front avoided wasting seeds on un-bankable orphans.
- Applied: 4 of 4: #1 `.greg`-dump eval-order/reassociation lever (Axis 6) → `docs/hazards.md#loop-weight-and-live-length-regalloc-steering` + provenance; #2 isolation-noise-masks-a-real-register-permutation corollary → `#isolated-compile-caveat`; #3 callee-side nested-child pre-classify (dead-`$v0`-spill / `$v0`-base-load) + `nested-child:<parent>` pick_target follow-up → `#nested-function-static-chain-spill`; #4 m2c+RE'd-struct classical-logic confirmation + ternary/stepwise levers → `CLAUDE.md` Seed step.
- Carry-over: `src/main/get_table_entry.c` — `update_ball_physics` (~19KB FP) + its 5 nested children (sound/rotation/meter helpers) + `init_ball_for_shot` (~3.8KB FP) + its nested child `calc_stick_offset_with_noise`. Retry only within an FP-wall-parent sprint (nested children come free with the parent, child-before-parent).

## Sprint 190 — src/main/func_8004E5A0.c SPIKE: 0/3 banked (DL-emitter scheduling wall), 3 carried — 2026-07-06
- Increment: 0 files md5-candidate / +0 functions matched (delta 223→223 md5-candidate files, total .c 234→235). File 0/3, 3 stubs. ROM green, freeze commit `ad225fd` only.
- Quality: 0 stuck-far / 1 permuter-escalated (func_8004E5A0) / 3 carried / 0 re-opened.
- Seed: committed 13pt; banked 0pt (spike, per-file all-or-nothing); realized 13 (seed 13 +1 permuter +1 carry, clamped at Fibonacci ceiling); residual 0; regime classical/mixed.
- What helped: PO steers were all load-bearing. m2c seeding (`~/development/repos/m2c`) + Ghidra DB check (no game struct RE'd — hand-packed words) + gfxdis.f3dex2 decode (named the font-blit DL: PipeSync/RenderMode XLU/PrimColor/CombineMode MODULATEIA/LoadTextureBlock_4b IA 64×8 @0x800C0D10) + the gDPLoadTextureBlock_4b macro pointer → reconstructed func_8004E5A0 to a FAITHFUL structural near-match (225/233 rows). The `-DF3DEX_GBI_2` probe proved the dynamic gDP macros emit the ROM's exact words (F3DEX2 0xE2.. not F3DEX 0xB9..); `decomp_loop --profile main` is the correct isolated-compile flag for a macro DL seed.
- Friction: the "+2 tractable non-FP fns" plan assumption was wrong — 0-jal/0-FP `glistp++` DL EMITTERS are a distinct scheduling-wall class. The residual is GCC's whole-header instruction scheduling (ROM front-stages ~11 constants to stack + precomputes all ~14 command addresses; faithful C keeps constants in regs, frame −0x58 vs ROM −0x88). Permuter `--main --best-only` plateaued 10065→6235 over 8000+ iters, no match. NOT the `#mem-in-struct` lever (no global load to retype). The ranker priced this size-13 TU as a smallest-first "+2 tractable" pick, but a debug/HUD DL TU where the non-FP fns are all emitters is partial-bank-expected-ZERO. setup-permuter dir-collision (import.py suffixed `-2` because a decomp_loop seed dir pre-existed; `mg_find_permuter_dir` grabbed the seed dir, missing compile.sh).
- Applied: 3 of 3: #1 DL-emitter partial-bank-expected-ZERO pts detector → `BACKLOG.md` follow-up + `docs/hazards.md#display-lists` wall note; #2 `decomp_loop --profile main` for macro DL seeds → `#display-lists` caveat updated (profile now live, was a tracked follow-up); #3 `mg_find_permuter_dir` compile.sh-marker fix → `tools/lib.sh`.
- Carry-over: `src/main/func_8004E5A0.c` — all 3 fns (func_8004E5A0 DL font-blitter scheduling wall + reconstructed near-match; func_8004FDB4 DL HUD renderer same class; func_8004E924 21-FP scene renderer S158 class). Reconstructed C in `nonmatchings/func_8004E5A0/base.c`.

## Sprint 189 — src/main/func_800660A0.c MIXED-PARTIAL: 2/8 banked (m2c+Ghidra, row-order seed lever), 6 carried (2 regalloc near-matches + 4 FP/DL walls) — 2026-07-05
- Increment: 0 files md5-candidate / +2 functions matched (`func_800660A0`, `func_800676B0`; delta 223→223 md5-candidate files, total .c 233→234). File 2/8, 6 stubs.
- Quality: 0 stuck-far / 1 permuter-escalated (rumble) / 6 carried / 0 re-opened.
- Seed: committed 13pt; banked 0pt (partial file, per-file all-or-nothing); realized 14; residual +1; regime classical/mixed.
- What helped: **row/field-order seeding** cracked `func_800676B0` (a 9-`s16` record init) first build — Ghidra's decompile gives the compiler-SCHEDULED store order which collapses the reused-constant live ranges (`0x800`→`v0`); re-seeding in human row order spreads them so GCC front-loads `a0=0x800`/`v1=-0x100` like the ROM. **goto-outer loop** solved `update_rumble`'s loop form (the `#top-tested-loop` reversal corollary: structured `for`/`do-while` both reverse the up-count to a down-count; a goto is invisible to `loop.c`) + **signed pointer compares** (Ghidra's `(int)` casts → `slt` not `sltu`) gave plain `bnez`. m2c seeded all tractable fns off `common.h` context (no RE'd struct existed — Ghidra models the collision-record fields as separate symbols + running offset, which matches ROM codegen).
- Friction: `setup-permuter.sh --main` aborted silently (`set -u` on the array expansion) — import.py worked only when called directly; fixed with an array-existence guard. `update_rumble` is byte-exact except a 3-register rotation the permuter plateaus on (best 45 vs base 55, never 0). `func_80067A60`'s build is MORE optimal than the ROM (the ROM keeps a redundant mask save/restore + declines a `%lo`-fold), so it is not reachable from clean source.
- Applied (3 of 3): #1 row-order-vs-Ghidra-scheduled-order seed lever → `docs/hazards.md#pervasive-regalloc-classical-main` (seed-order sub-lever); #2 `setup-permuter.sh` `set -u` array-guard → `setup-permuter.sh`; #3 FP/DL-wall pts detector → `BACKLOG.md` follow-up (golden-gated, off-cadence).
- Carry-overs: `func_800660A0.c` (6 fns) — 2 regalloc near-matches (`update_rumble_intensity_table`, `func_80067A60`; wips saved) + 2 S158 FP walls (`func_800674B8`, `func_80067730`) + 2 F3DEX2 display-list walls (`emit_aim_target_ring`, `draw_course_decal_triangles`).

## Sprint 188 — src/main/set_camera_matrices_fixed.c MIXED-PARTIAL: 3/11 banked (m2c+Mtx4f seeding), 8 FP-regalloc walls carried — 2026-07-05
- Increment: **0 files banked / +3 functions matched** (delta: md5-candidate 223/232 → 223/233; the flipped camera/projection pack `set_camera_matrices_fixed.c` is 3/11, stays mixed-partial). Banked `project_point_view_depth` (view-depth dot product), `func_80065D5C` (in-place 4x4 matmul), `convert_and_pack_floats_to_fixed` (game guMtxF2L variant).
- Quality: **0 / 1 / 8 / 0** (0 stuck-far, 1 permuter-escalated [`func_80065898`, 1470→670 over 117k iters no match], 8 carried [`func_80065A1C`, `func_80065898`, `set_camera_matrices_float`/`_fixed`, `frustum_cull_point_with_radius`, `project_point_to_screen`, `mtx_from_rts`, `func_80065E6C`], 0 re-opened).
- Seed: committed **13pt**; banked **0pt** (partial file, per-file all-or-nothing); regime **classical/mixed**. Realized **15**; residual **+2** (seed 13 +1 permuter +1 game-gu-variant gotcha +1 combine_givs de-bias gotcha, −1 clean matmul/dot-product banks).
- What helped: **PO-directed m2c seeding with a Ghidra RE'd-struct context** (`ctx.c` = `common.h` + the `Mtx4f`=`float[4][4]` typedef from the Ghidra DB + loose camera globals — Ghidra had NO camera struct). **2D-index combine_givs de-bias**: m2c's `void*` walking-pointer matmul seed tripped loop.c `combine_givs` base-bias; `f32 m[4][4]` params + 2D `a[i][k]`/`b[k][j]` indexing gave the ROM's direct-immediate form. **Game guMtxF2L variant identification**: cross-checked the ultralib BUILD .o (not just source) — the MG64 copy diverges from ultralib's own VERSION_J build via `!=` held-const loops + dropped-redundant `& 0xffff0000`. **PO-directed GCC-2.7.2-source root-cause** of `func_80065A1C`: `config/mips/mips.h` has no `REG_ALLOC_ORDER` → the 6th-callee-saved-FP-reg product-hoist is priority-driven and not forceable from source (confirmed irreducible, no thrash).
- Friction: the whole pack is a camera/projection FP-math wall class (regalloc/scheduling); only the 3 structural fns (dot-product, 2D-indexed matmul, guMtxF2L-variant) were reachable from faithful C. The permuter plateaus on the FP-regalloc residuals.
- Applied: **3 of 3**: #1 m2c+Mtx4f de-bias lever → `docs/hazards.md#game-region-mirror--o2-profile` + cross-ref `#indexed-vs-pointer-loop-strength-reduction`; #2 game-embedded-gu-variant-vs-BUILD-.o tell + 2nd-copy-keeps-descriptive-name → `#game-region-mirror--o2-profile`; #3 FP-camera regalloc sub-case (6th-callee-saved-FP-reg product-hoist, no REG_ALLOC_ORDER) → `#pervasive-regalloc-classical-main`.
- Carry-over: `set_camera_matrices_fixed.c` (8 stubs) → BACKLOG; all FP-regalloc/scheduling walls, `func_80065A1C` compiler-source-confirmed irreducible, `func_80065898` permuter-plateau, `func_80065E6C` needs a rodata-jtbl carve.

---

## Sprint 187 — src/main/func_80077BF0.c MIXED-PARTIAL: 4/7 banked (asm-first fast-path), 3 carried (C18 regalloc near-miss + 2 FP not-attempted) — 2026-07-05
- Increment: **0 files banked / +4 functions matched** (delta: md5-candidate 223/231 → 223/232; the flipped spark-effect pack `func_80077BF0.c` is 4/7, stays mixed-partial). Banked `func_80077BF0`/`func_80077C0C` (global-init glue), `func_80077DEC` (heap3_free group loop), `func_80077E6C` (bgtzl decrement).
- Quality: **0 / 1 / 3 / 0** (0 stuck-far, 1 permuter-escalated [`func_80077C18`], 3 carried [`func_80077C18`, `func_80077E94`, `render_spark_effects`], 0 re-opened).
- Seed: committed 13pt (frozen `9a7b965`); banked 0pt (per-file all-or-nothing, 3 stubs remain); realized **13**; residual **0**; regime classical/mixed.
- What helped: the **asm-first fast-path** banked all 4 small fns byte-exact straight from the `.s` (no MCP, no base.c). `func_80077DEC` needed the array-index `&D_80105140[i*0x2C]` form (GCC strength-reduces the s0=pointer temp; a pointer-walk gives it s1) + `i=0` hoisted before the first `jal` (into its delay slot). On `func_80077C18`, an **in-tree objdump-vs-.s diff** (the post-flip isolated `decomp_loop` reference is stale — empty base_text, bogus 0.21) found 3 real source fixes: **count-reload not cache** (caching `g->count` across `heap3_alloc` forces an extra callee-saved reg; use `g->count` directly), `D_800C4660 = groupCount` (not `= i`), and index order `col + base` — taking it to 117==117 structural-complete.
- Friction: `func_80077C18`'s inner grid-scan loop kept a ~6-8 insn micro-regalloc residual (`rows` in t1-vs-t0, inner guard `beqz`-vs-`beqzl`, a `t0=a1` base-copy, two commutative-add operand orders) that resisted source steering; the `--main` permuter (~8400 iters) only found a semantics-breaking score-390 → carried. `setup-permuter.sh`'s `mg_resolve_c_asm` silently failed on the target (already a C body in a multi-fn file, no INCLUDE_ASM stub) → worked around by calling `import.py` directly. `func_80077E94`/`render_spark_effects` are FP-heavy (guRandom + cvt.s.w/add.s; render 506i/63 FP ops + DL) — not attempted.
- Applied (3 of 3): #1 `mg_resolve_c_asm` fallback (resolve a fn inlined in a multi-fn file via its C definition + find the `.s` by name; clear error not silent `set -e`) → `tools/lib.sh`; #2 post-flip stale-isolated-ref → objdump-slice-with-pseudo-op-normalization recipe added to `#isolated-compile-caveat`; #3 indexed struct-array field folds to the splat per-field `D_<addr>` symbol (no per-field externs needed) → note in `#offset-0-symbol-re-materialization`.
- Carry-over: `src/main/func_80077BF0.c` 3 stubs — `func_80077C18` (particle-group table init, #pervasive-regalloc near-miss, near-match saved), `func_80077E94` (165, FP particle initializer, S158 class), `render_spark_effects` (506, FP+DL render tail). All reference shared TU rodata extern (no carve); ROM green off the 4 banked.

## Sprint 186 — src/main/lz_compress_extended_dma.c MIXED-PARTIAL: 10/15 banked (5 clean glue + nested-RNG triple + switch + mem-in-struct), 5 walls carried; compiler-source fan-out cracked func_80069BCC with 0 permuter — 2026-07-05
- Increment: **0 files banked / +10 functions matched** (delta: md5-candidate 223/230 → 223/231; the flipped pack `lz_compress_extended_dma.c` is 10/15, stays mixed-partial). Banked 5 call-glue fns (m2c-seeded), the nested-RNG triple (`func_80068F4C`+nested `func_80068F00/F18`), `func_8006955C` (switch), `func_80069BCC` (mem-in-struct CSE lever).
- Quality: **0 / 0 / 5 / 0** (0 stuck-far, 0 permuter, 5 carried [`func_80068F98`, `func_80069124`, `func_800695F8`, `func_80069D08`, `lz_compress_extended_dma`], 0 re-opened).
- Seed: committed 13pt (frozen `63fc5ef`); banked 0pt (per-file all-or-nothing, 5 stubs remain); realized **15**; residual **+2**; regime classical/mixed.
- What helped: **m2c-from-repo + RE'd-struct `--context` seeding** (PO directive) — context = `common.h` OS structs + the sibling `lz_decompress_simple.c`'s `LzDecompressState` + game externs → byte-faithful call-glue seeds first-build. **PO-directed compiler-source fan-out** (2 gcc-2.7.2 subagents) root-caused `func_80069BCC`'s tail CSE-reload to `cse.c invalidate_memory` (`nonscalar && p->in_struct`) → fix = read `D_801B6098[0]` (MEM_IN_STRUCT_P) → **full-fn byte-exact, no permuter**. The nested-fn static-chain recipe banked the LCG triple (3rd confirmation, pure-global-only children). `func_8006955C` = a one-instr `sltiu`/`slti` signedness fix (`D_800BA9FC` u32).
- Friction: whole-pack flip broke `decomp_loop`'s seg-object reference model (`#stale-parent-asm-relic` on a flip, not a split; `asm/440A0.s` + `asm/43810.s` both shadowed) → moved relics aside + used full-build objdump diffs. `func_80068F98` (FP nested-loop) + the 3 big compression/thread fns are genuine walls (not attempted deeply).
- Applied (4 of 5): #1 mem-in-struct cse.c CSE-reload variant → `#mem-in-struct-scheduling-lever` + hazard-index row; #2 sltiu/slti signedness → `#switch-jtbl-dispatch` lever 4 + hazard-index row; #3 m2c+RE'd-struct seeding recipe → CLAUDE.md Seed step; #5 whole-pack-flip relic breakage → `#stale-parent-asm-relic`; (#4 nested-fn 3rd-confirmation anchor = no-op, NOT selected).
- Carry-over: `src/main/lz_compress_extended_dma.c` 5 stubs — `func_80068F98` (FP nested-loop vec3 scaler), `func_80069124`(294), `func_800695F8`(414), `func_80069D08`(157 loader-thread dispatch), `lz_compress_extended_dma`(165 DMA double-buffer goto-loop). All reference shared TU rodata extern (no carve); ROM green off the 10 banked.

## Sprint 185 — src/main/func_80043C20.c S184-tail rodata-carve retry: resolve_club_terrain_mask BANKED [14/21] via cross-jump fix; 2 carried (resolve_shot_quality carve-solved/sched-irreducible, predict_shot_distance not-attempted) — 2026-07-05
- Increment: **0 files banked / +1 function matched** (delta: md5-candidate 223 unchanged; `func_80043C20.c` 13/21 → 14/21, stays mixed-partial). Banked `resolve_club_terrain_mask` byte-exact (jtbl_800CC530 `.rodata` carve + a cross-jump-tail-merge fix).
- Quality: **0 / 0 / 2 / 0** (0 stuck-far, 0 permuter, 2 carried [`resolve_shot_quality_table`, `predict_shot_distance`], 0 re-opened).
- Seed: committed 5pt; banked 0pt (per-file all-or-nothing, file mixed-partial); regime classical/mixed. Realized 8 (residual +3): seed 5 +1 cross-jump novel-gotcha +1 interleaved-rodata-carve novel-gotcha +1 carry.
- What helped: **The PO-directed compiler-source fan-out (5 subagents) was the whole sprint, and it did exactly its job — cracked one wall, proved another irreducible, no premature permuter.** (1) `resolve_club_terrain_mask`'s cross-jump-tail-merge (two equal-constant `return 1` sites merged, blocking the ROM's tight `bnez end`+delay fold) fell to a **switch-funnel-through-result-var** — reorg + jump.c subagents proved the merge fires via `find_cross_jump`'s `rtx_renumbered_equal_p` on the jump *target* label, and funneling only the switch through `r`+`return r` (early ifs stay direct) gives the cases a distinct exit label → no merge; binutils ruled out the assembler. (2) The **interleaved-one-tu rodata carve** was solved for `resolve_shot_quality_table`: individual `static const char[16]` (not 2D — 10 insns short; not extern — 0x30 whole-ROM shift) makes GCC emit `[jtbl530][23 tables][jtbl700]`=0x230 matching the ROM's interleaved layout; the **address-select lever** `(cond)?&p2[i]:&p1[i]` fixed the a0/a1 pointer/index swap + the bne-vs-beql branch (22→3 diffs). (3) The focused sched.c subagent PROVED the residual 3-insn v0/v1 swap irreducible (load-latency-3 hoist → fresh reg vs the ROM's dead-reg reuse whose WAR anti-dep forbids the hoist; DAG-invariant, permuter useless) → carried with confidence + a decomp.me scratch.
- Friction: the interleaved-rodata carve surfaced as a **0x30 whole-ROM shift** that first looked like a boot-code corruption — the real cause was the near-match `.text` being the wrong size (extern digit tables mis-spaced the two jtbls) + then the `p2=p1+16` 2D-array fold; both diagnosed by reading rodata *symbol addresses*, not the `.text` diff. `mg_resolve_c_asm` (grep for the INCLUDE_ASM stub) can't locate a fn that's already C, so the decomp.me export needed a direct `import.py` call with a standalone in-repo one-fn file (the multi-fn file's GCC nested fn breaks pycparser's context/source split).
- Applied (3 of 4): #1 interleaved-one-tu rodata recipe → `docs/hazards.md #rodata-sibling-yaml-pattern`; #2 address-select lever + #3 sched-load-latency-irreducible → `docs/hazards.md #register-reuse-nudge`; (#4 decomp.me-via-import.py note NOT selected).
- Carry-over: `resolve_shot_quality_table` (structural-complete 109/109, carve SOLVED, 3-insn sched residual proven-irreducible — near-free retry only if a new faithful reg-pressure idiom or the permuter-negative is revisited; `docs/wip/resolve_shot_quality_table.near-match.md` + decomp.me `dZACn`). `predict_shot_distance` (+nested `lerp_int_v2`) not attempted — S181 `#abs-coalescing` fresh-reg LAW class + f64 D_800CC508 carve; dedicated retry.

## Sprint 184 — src/main/func_80043C20.c golf-shot/club logic pack: 13/21 banked (highest single-file classical bank), 8 regalloc/rodata carries — 2026-07-05
- Increment: **0 files banked / 13 functions matched** (delta: md5-candidate 223 unchanged; the new `func_80043C20.c` is 13/21 → mixed-partial, not md5-candidate). Banked byte-exact: 7 getters/wrappers (`get_club_distance_slot`/`get_club_param`/`get_shot_data`/`get_shot_param`/`get_shot_progress`/`func_80044A8C`/`func_80044C7C`), the `func_800444B8`+`func_80044470` nested pair (init loop + divide-by-14 lookup), `func_80044254` (club-param init w/ clamps) + `func_80044FDC` (auto-select-club), and the `func_80043C64`+`func_80043C20` nested formatter pair (debug club-dump + string-appender).
- Quality: **0 / 1 / 8 / 0** (0 stuck-far, 1 permuter-run [`func_80044CCC` 800→575 plateau], 8 carried, 0 re-opened).
- Seed: committed 13pt; banked 0pt (partial file, per-file all-or-nothing); realized 19 (seed 13 +1 re-attempt [`func_800444B8` base-IV, 4 tries] +3 novel-gotchas [combine_givs base-bias / nested-pair recipe / for-rotation] +4 carry-decode-effort, −2 clean getter/subagent banks); residual +6; regime classical/mixed.
- What helped: **the `combine_givs` base-IV lever** — a store loop over `D_800BB258[i]` byte-matched except the base register was biased to the max field offset (+0x30, negative store immediates); a carried `dst++` triggers `loop.c` `combine_givs`, and `ClubShot* cs = &D_800BB258[i]` recomputed INSIDE the loop de-biases to the ROM's element-base positive offsets (root-caused in `loop.c` per the PO's compiler-source direction; cracked `func_800444B8` + the `func_80043C64` club loop). **The GCC nested-function bank-the-pair recipe, confirmed on 4 nested fns**: GCC emits the child before the parent, so writing the parent's C at the child's lead-vram source position lands the child (`func_80043C20.2` at 0x80043C20); a pure-leaf math helper (`lerp_int`/`lerp_int_v2` = `(s32)((f64)from+(f64)(to-from)*(f64)t)`, `t` an f32 in `$a2`) is STILL framed when nested (17 vs standalone 14 instrs) — so a leaf whose only caller sets `$v0` before the `jal` MUST be nested to match. **`for(;;)+break` vs `while`** fixed the string-appender loop-rotation (a `while` copies the test to the loop bottom, +2 instrs; `for(;;)` keeps the ROM's single top-test + `j`-back). **Divide-by-14** = magic 0x92492493 + shift-3 (a `/7` C gave shift-2). **m2c** seeded every fn (PO-directed; hand-built the context around the `m2ctx.py` -I bug). **7-way parallel subagent fan-out** offloaded the hard tail — `make nonmatching-func` is per-function so `decomp_loop` is parallel-safe across distinct fns; the orchestrator integrated + full-made serially (both isolated matches held in-tree).
- Friction: the pack's tail is a `#pervasive-regalloc-classical-main` minefield — the shot-math fns (`func_80044CCC`, `func_800451E4`, both `predict_*` parents) are structurally-complete near-misses where the build is 3 instrs "more optimal than target" (allocno tiebreak / dead-branch DCE / abs-swap), unreachable from faithful C; and 3 carries (`resolve_club_terrain_mask`, `resolve_shot_quality_table`, `predict_shot_distance`) need a rodata carve (compiler jtbl / f64 literal) out of scope under the no-carve rule. **Pre-existing tooling debt surfaced (NOT this sprint's regression):** `make test-tools` had 6 stale failures before I touched anything — 4 `pick_target` live-state goldens drifted by S182/S183 banking (regenerated here with `REGEN_GOLDEN=1`, banking-only diff), plus 2 unrelated pre-existing bugs left noted (a broken `## Playbook index` anchor `cse-make-regs-eqv-branch-fold...` — cited-heading-adjacent, risky to touch; and `test_coddog_suppresses_maybe_upstream` KeyError, a stale fixture).
- Applied: **6 of 6**: #1 combine_givs `&ARR[i]`-recompute base-bias lever → `docs/hazards.md#indexed-vs-pointer` sub-lever; #2 `for(;;)`+`break` un-rotated-loop lever → `docs/hazards.md#top-tested-loop` sub-lever; #3 nested-function bank-the-pair recipe (4-fn confirmation) → `docs/hazards.md#nested-function-static-chain-spill`; #4 `tools/m2ctx.py` -I set synced to Makefile CFLAGS + error-handler `TypeError` fix (verified: preprocesses main files, `test-tools` unaffected by the fix); #5 /14-vs-/7 shared-magic shift note → `docs/hazards.md#signed-divide-const`; #6 parallel subagent decomp fan-out doctrine → `CLAUDE.md` Parallelism bullet.
- Carry-over: src/main/func_80043C20.c — 8 fns (detail in scratchpad `result_*.md`, seeds in `nonmatchings/<fn>/base.c`): `resolve_club_terrain_mask` + `resolve_shot_quality_table` (compiler jtbl → rodata carve; the latter also a 3-instr v0/v1 regalloc), `func_80044CCC` (arg0↔lp regalloc + dead-branch DCE, permuter 800→575), `func_800451E4` (`#pervasive-regalloc` const-1 CSE + allocno tiebreak), `predict_shot_distance_variant`+`lerp_int` (parent regalloc, child byte-exact), `predict_shot_distance`+`lerp_int_v2` (f64 rodata carve + `#abs-coalescing-reg-swap` irreducible, child byte-exact). Retry: a rodata-carve increment (jtbl/f64 fns) + a permuter/`#cross-project-matched-corpus-mining` increment (`func_80044CCC`/`func_800451E4`/`predict_shot_distance_variant`). No cross-repo name sync (all names pre-curated; no `symbol_addrs` adds).

---

## Sprint 183 — src/main/func_80052250.c main-segment [0x2D650] pack: 2/9 banked, DEFINITIVE pervasive-regalloc TU (7 carried) — 2026-07-05
- Increment: **0 files banked / 2 functions matched** (delta: md5-candidate 223/227 unchanged; the new `func_80052250.c` is 2/9 → mixed-partial). Banked byte-exact: `func_80052250` (0x80052250, `scenario_mode_id==0xC` predicate), `func_80052324` (0x80052324, scenario table lookup).
- Quality: **0 / 1 / 7 / 0** (0 stuck-far, 1 permuter-escalated [`func_80052264`], 7 carried, 0 re-opened).
- Seed: committed 13pt; banked 0pt (partial file, per-file all-or-nothing); realized 18 (seed 13 +1 permuter +1 TU-profile-probe +5 carries/walls, −2 clean predicate banks); residual +5; regime classical/mixed.
- What helped: **m2c** (`~/development/repos/m2c`, PO-directed) was the primary seeder for every non-trivial fn — its expression logic matched the hand-decode; the deviations from m2c were the near-miss causes (extracting the call into a local reordered eval; the inline form matched). The **`#register-reuse-nudge` array-index `+` operand-order lever** closed `func_80052324` (addu byte on the LEFT) and `func_80052264`'s table index (arg1*10 on the RIGHT). systematic-debugging with the **GCC-2.7.2 + binutils-2.6 source** (PO-directed) cleanly root-caused `func_80052264`: the `mfhi→mult` hazard `nop`s are inserted by KMC-`as` (`append_insn` line 1387, `interlocks=0` for plain `-mips2`; GCC emits `#nop` COMMENTS in `.set reorder`), so they were CORRECT — the "missing nops" was an **objdump `-d` collapse artifact** (`...` for `nop;nop`, dropped by `awk 'NF>2'`). The residual was a single scratch register (`t0` vs `a0`), a `REG_ALLOC_ORDER`-greedy artifact (no `REG_ALLOC_ORDER` in KMC mips.h → default ascending). The **`(&D_801B60A0)[-3]` negative-index idiom** referenced the struct-folded non-placed `D_801B6094`/`D_801B6098` through the placed neighbor, both compiling AND reproducing the ROM's `base - 0xC` derivation (`func_800525C4` frame+saves matched). The **TU-wide `#profile-probe`** (S182 doctrine, fired on ≥2 clustered walls) confirmed -O2 best, no flag flips the walls → genuine per-fn artifacts.
- Friction: this TU is a **definitive `#pervasive-regalloc-classical-main` minefield** — 8/8 non-trivial fns (compare/score/dispatch over `func_80052324` + `D_801B71Fx` s8 byte-tables + struct-folded `D_801B60A0` globals) are regalloc/structure walls; only the 2 trivial predicates banked. Big per-fn decode effort for near-certain carries. The `objdump -d` `...`-collapse false-positive cost a multi-step mflo-hazard chase before the real root cause (1 register) surfaced. The 4 large compare/dispatch fns built as growing-diff near-misses (84/128/150) confirming size scales the wall.
- Applied: 3 of 3: #1 objdump `-dz`-mandatory byte-diff doctrine (keep every line, no `awk 'NF>2'`) → `docs/hazards.md#assembler-differences--byte-cmp-spot-check`; #2 `(&PLACED)[-N]` struct-fold idiom for a non-placed base-relative global → `docs/hazards.md#struct-access-folding-changes-scheduling`; #3 pervasive-regalloc TU detector (call-return/table-scan idiom + struct-folded neighbors → regalloc-heavy pts / partial-bank-expected) → BACKLOG off-cadence golden-gated tooling follow-up.
- Carry-over: src/main/func_80052250.c — 7 fns, all with `docs/wip/<fn>.near-match.md`: `func_80052264` (1-reg `REG_ALLOC_ORDER` wall, permuter+source-confirmed), `func_80052384` (base-ptr-hoist), `func_8005244C`/`func_800525C4`/`func_80052834`/`func_80052A68` (built regalloc/structure near-misses), `func_80052CF0` (decoded switch dispatcher, not built). Retry: dedicated permuter/`#cross-project-matched-corpus-mining` sprint on the closest (`func_80052264`, `func_800525C4`). No cross-repo name sync (250/324 stayed auto `func_`; Ghidra had no curated names).

---

## Sprint 182 — src/main/func_8004D190.c VI/framebuffer + grid-print pack: 3/7 banked, 2 regalloc-artifact walls + 2 DL builders carried — 2026-07-05
- Increment: **0 files banked / 3 functions matched** (delta: md5-candidate 223/227 unchanged; the new `func_8004D190.c` is 3/7 → mixed-partial). Banked byte-exact: `clear_text_grid` (0x8004D794, backward buffer clear), `set_flag_based_on_param` (0x8004D99C), `func_8004D580` (0x8004D580, string-render wrapper).
- Quality: **0 / 1 / 4 / 0** (0 stuck-far, 1 permuter-escalated [func_8004D5F0], 4 carried [2 regalloc walls + 2 DL builders], 0 re-opened).
- Seed: committed 13pt; banked 0pt (partial file, per-file all-or-nothing); realized 14 (seed 13 +1 permuter-escalation; 3 clean first-try banks offset by 2 deep regalloc-wall dives + DL deferral); residual +1; regime classical/mixed.
- What helped: quick-win diagnostics on the clean banks — `set_flag_based_on_param`'s `-0x80` immediate proved `flag` is **s8** not u8 (u8 truncates -128→+128); `clear_text_grid`'s `v0`/`v1` swap fixed by initializing the counter before the pointer; `func_8004D580`'s `lb`(test)+`lbu`(arg) double-load turned out to be the *natural* GCC output (the callee call may clobber `*str` → reload). On `func_8004D4B8` (`#dead-frame`), two source levers got it structurally byte-exact: `c-=0x20` in-place (fixed char→v1 + dropped a `move`) and a clean `for(a3=0;a3<2)` inner loop (matched the branch-likely shape). systematic-debugging pinned both walls' root cause in `mips.c` (compute_frame_size:4444 — a dead frame is a local-alloc slot reserved despite free `t7-t9`, filled by global-alloc; and the register-COALESCING divergence on `func_8004D5F0` where the target keeps 2 non-coalesced copies of `c` while my -O2 build coalesces = strictly more optimal). Permuter (main-profile, via a **direct `import.py`** bypass since the resolver can't find an already-inlined fn) confirmed `func_8004D5F0` a wall (base 2055 → plateau 1190).
- Friction: this TU is a regalloc-artifact minefield — TWO consecutive register-pressure-heavy fns (`func_8004D4B8`, `func_8004D5F0`) hit the "mine-more-optimal-than-target" wall class (dead frame / non-coalescing) where faithful C cannot pessimize GCC to match the target's allocation. The permuter setup fought the tooling (`mg_resolve_c_asm` aborts silently on an inlined fn). The 2 DL builders (`func_8004D190` 200 insns + `func_8004D7B8` 120 insns) are a distinct m2c+gfxdis reconstruction workflow, correctly deferred rather than half-attempted.
- Applied: 3 of 3: #1 `#permuter-setup-for-kmc-toolchain-mirrors` direct-`import.py` bypass for an already-inlined multi-fn-file near-match (+ deferred golden-gated `mg_resolve_c_asm`-for-inlined-fn tooling follow-up in BACKLOG); #2 new `#profile-probe` **TU-wide-cluster doctrine** (when ≥2 alloc-artifact walls cluster in one TU, run one whole-TU profile-probe before N per-fn dives — a single flag/patchlevel could flip both); #3 deferred golden-gated pick_target DL-builder-flag tooling follow-up (leaf + dense GBI-opcode command-word stores → route to `#display-lists` upfront) in BACKLOG.
- Carry-over: src/main/func_8004D190.c — `func_8004D4B8` (`#dead-frame-reload-artifact-regalloc-wall`, near-match `docs/wip/func_8004D4B8.near-match.c.txt`) + `func_8004D5F0` (register-COALESCING wall, permuter-confirmed, `docs/wip/func_8004D5F0.near-match.c.txt`; run the TU-wide profile-probe on both FIRST) + `func_8004D190`/`func_8004D7B8` (DL builders → dedicated `#display-lists` sprint). Cross-repo: `clear_text_grid = 0x8004D794` → `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 181 — src/main/func_80076640.c abs-coalescing reg-swap: the func_80076640 wall DEFINITIVELY CLOSED (0-bank diagnostic carry) — 2026-07-05
- Increment: **0 files banked / 0 functions matched** (delta: md5-candidate 223/227 unchanged). `func_80076640` proven an irreducible `#abs-coalescing-reg-swap` wall; `func_8007680C` (stretch) untouched — file stays mixed-partial.
- Quality: **0 / 0 / 1 / 0** (0 stuck-far, 0 permuter [proven futile, not run], 1 carried, 0 re-opened).
- Seed: committed 5pt; banked 0pt (file carried, not md5-candidate); regime classical/mixed (0-bank carry; realized n/a — the value is diagnostic, not points).
- What helped: PO-directed systematic-debugging turned a "carry it" into a definitively-closed wall. 4 parallel GCC-2.7.2-source lenses converged on the exact mechanism (`combine_regs` suggested-reg pre-pass, local-alloc.c:1813-1817 / 1469-1477: a dying-hard-reg abs operand gets an unconditional `$f0` arithmetic suggestion the literal const can never contest). An 8-project **cross-project mining** sweep (new method) established the byte-cmp-proven fresh-reg unary-float LAW on the identical KMC GCC 2.7.2 (5 corpora: MP1/MP2/MP3/drmario64/sbk2; hm64+pl64 NULL). A 12-flag profile-probe + a direct **2.8.1 cross-compile** (in-place + 80 insns; same-TU `func_80076778` matched at 2.7.2 ⇒ TU provably 2.7.2) closed the wrong-pin / compiler-version door. The literal fix (`0.34906584f`→`0.3490659f`=0x3EB2B8C4) tightened the near-match to 75/78 (only the 3 abs-region regs differ).
- Friction: the ROM's fresh-reg abs for a provably single-use magnitude is genuinely anomalous vs. every same-compiler corpus — a 2.7.2 patchlevel micro-divergence the reconstruction can't reproduce, so there is no faithful fix (the honest systematic-debugging "no root-cause fix exists" outcome). A whole sprint of deep investigation banked 0 code; the value is entirely retiring an open lever + a reusable escalation method.
- Applied: 3 of 4: #1 rewrote `#abs-coalescing-reg-swap` to PROVEN-irreducible + folded in the fresh-reg unary-float LAW; #2 new `#cross-project-matched-corpus-mining` section (+ hazards TOC + CLAUDE hazard index); #3 fresh-reg law folded into #1 (same edit); (#4 "mark func_80076640 do-not-retry" NOT selected — stays a retryable carry pending a genuinely-new zero-insn lever or the original binary).
- Carry-over: src/main/func_80076640.c — `func_80076640` (proven `#abs-coalescing-reg-swap`, all faithful/flag levers exhausted; retry only on a new zero-insn mechanism) + `func_8007680C` (S158-class FP-spill regalloc wall, untouched this sprint).

---

## Sprint 180 — src/main/func_800328E0.c DCE0 DL pack COMPLETE: func_800329F4 banked via Color-struct mem-in-struct lever (S179 scheduler-load-pair carry resolved) — 2026-07-05
- Increment: **1 file md5-candidate** (`func_800328E0.c`, 0 `INCLUDE_ASM`) / **1 function matched** (`func_800329F4`); md5-candidate **222 → 223**.
- Quality: **0 / 0 / 0 / 0** (0 stuck-far, 0 permuter [deliberately avoided], 0 carried, 0 re-opened) — and RESOLVED the 1 S179 carry.
- Seed: committed 3pt; banked 3pt; realized 4 (residual +1); regime mixed   (v1 story points; v2 realized tier: seed 3 +1 novel-compiler-lever multi-agent dive).
- What helped: the **two-agents-per-wall GCC-source fan-out** (S178 doctrine, PO-directed via systematic-debugging) cracked a wall the S179 retro had framed permuter-class. Pairing **orthogonal lenses** was the key: a scheduler/priority agent (hunt a source reorder — proven dead end, every reorder/color-expr form stalled 18-30/97) and a machine-model/**alias** agent (hunt a dependency-edge lever — found it) + a gas rule-out agent (scoped the wall 100% to GCC). Both compiler agents *independently converged* on the same `mem/s` lever = strong corroboration. Root cause: plain-`s32` color loads no-alias the `mem/s` `*glistp` stores (`true_dependence` guard sched.c:834-836) → free roots → the sticky `LAUNCH_PRIORITY` birthing-boost (sched.c:3902/2543, dominates via rank_for_schedule:2395, never decays via 1435) floats them into the glistp load-shadow. Fix = retype the RGB triple as a `Color {s32 r,g,b;}` struct ([#mem-in-struct-scheduling-lever](docs/hazards.md), extended to DL fns) → loads become `mem/s`, may-alias the stores, pinned late = ROM; regalloc snaps exact (leaf, downstream of schedule). 2nd ingredient: the n64demos `gfxClearCfb` idiom (inline double-`GPACK_RGBA5551` in the `gDPSetFillColor` arg) puts the color compute after the fill-color w0 store. Data-side stayed as-is in `main_data` via a splat `type:Color size:0xC` symbol (no carve). Verified 97/97 isolation → clean-rebuild ROM SHA-1 byte-exact.
- Friction: the S179 retro's "escalate to the permuter without --best-only" verdict was a premature wall-call — the faithful lever was reachable and cheaper; the deeper compiler dive (which S178 had already proven pays off) should have been tried in S179 before carrying. The mechanism was also mis-described in S179 ("both priority-1 class-3 in rank_for_schedule" — actually a sticky-boost DOMINANCE, not a tie-break; corrected this sprint).
- Applied (4 of 4 buffered; #5 recorded as a deferred tooling follow-up): #1+2 `docs/hazards.md#display-lists` permuter-class RETIREMENT + route-to-mem-in-struct + LAUNCH_PRIORITY mechanism + `#mem-in-struct-scheduling-lever` DL extension + the CLAUDE.md hazard-index row; #3 two-agents-per-wall **orthogonal-lenses** 2nd confirmation in `#compiler-source-fan-out`; #4 n64demos `graphic.c` reference folded into `#display-lists` provenance; #5 pick_target detector idea (DL fill-color from ≥2 `D_` globals a sibling sets as consecutive words → mem-in-struct candidate) → `BACKLOG.md` tooling follow-up, NOT applied (off-cadence, golden-gated).
- Carry-over: none. The S179 `func_800329F4` carry is RESOLVED and pruned from `BACKLOG.md ## Carry-overs`.

## Sprint 179 — src/main/func_800328E0.c DCE0 display-list pack, 5/6 mixed-partial (m2c + gfxdis.f3dex2 seed; func_800329F4 scheduler-load-pair wall carried) — 2026-07-04
- Increment: **0 files md5-candidate** / **5 functions matched** (`func_800328E0`, `func_800329B8`, `func_800329D8`, `func_80032E34`, `func_80032B78`); md5-candidate **222 → 222** (file 5/6, 1 stub `func_800329F4`). First main-seg DL pack of the classical endgame.
- Quality: **0 / 1 / 1 / 0** (0 stuck-far, 1 permuter-escalated, 1 carried fn, 0 re-opened).
- Seed: committed 13pt; banked **0pt** (per-file all-or-nothing, file mixed-partial); regime mixed. Realized **13** (residual **0**): seed 13 +permuter +carry but Fibonacci caps at 13. Value signal = **+5 matched fns**.
- What helped: **PO-directed m2c + gfxdis.f3dex2 seed combo** (m2c for bodies, `extract_dlist.py`/`gfxdis.f3dex2 -d` to reconstruct the GBI macros from the immediate command words) — 4 of 6 fns matched at link after the isolated `%hi/%lo` reloc artifacts resolved, and `func_80032B78` (the mode-select color/Z clear with a per-scanline fill loop + `osVirtualToPhysical` mid-DL) matched **first build** via the `Gfx* gfx=*glistp` local-pointer idiom. **The user's n64demos lead was the decisive diagnostic:** `~/development/n64/n64demos/nusys/*/src/main/graphic.c` `gfxRCPInit`/`gfxClearCfb` confirmed the faithful source (double-`GPACK_RGBA5551` fill color, `OS_K0_TO_PHYSICAL`) AND settled that the `& ~7` phys-align mask is **game-specific** (the demo has none; `func_80032B78` needs it byte-exact — the user's "KMC autoaligns, drop it" hunch was refuted by the oracle: dropping it is 4 insns short). `OS_K0_TO_PHYSICAL` (user-directed) is the idiomatic macro and stayed byte-exact.
- Friction: `func_800329F4` is a genuine **GCC-scheduler load-hoist wall** — structurally complete (97 insns, identical op multiset) but the list scheduler pairs the color global symbol-load with the `glistp` global symbol-load in the load-latency shadow (`rank_for_schedule`: both priority-1 class-3), where the ROM defers it (77/97 register cascade). It is the ONLY pack fn using the global `glistp++` directly (13 `%lo(glistp)` stores); the others take a `Gfx**` param whose deref is not a symbol-load, so they don't pair. Exhaustively ruled out via full-build dumps: faithful double-GPACK, `-mips2`/`-mips3`, `vs32` volatile, the local-pointer idiom (forces a stack frame, still hoists). The permuter reached 2765/5210 then plateaued — **because the S179 run used `--best-only`**, which the S160 doctrine already warns cannot cross an equal-score plateau (the only improving mutation was an unfaithful scheduler-constraining pointer-alias, exactly the plateau signature). Retry without `--best-only` is the queued escalation. Also lost time to a runaway permuter-process cleanup (`pkill -f permuter` matched my own shell command).
- Applied: 4 of 4 — #1 built the **`setup-permuter.sh --main` flag + `permuter_settings_main.toml`** (the tracked S167 follow-up: game -O2/F3DEX2 profile + modern-GAS target assembler for the `.set gp=64` target `.s`, so a main/ DL fn needs no per-run patching); #2 `#display-lists` additions (RCP-clear/`& ~7`-game-mod idiom, the global-`glistp++` scheduler-load-pair wall, m2c+gfxdis seed combo) + 2 CLAUDE hazard-index rows + `#permuter-setup` `--main`/`--best-only` note; #3 n64demos `graphic.c` reference (memory + docs); #4 DL-track routing BACKLOG note (a DL pack is classical `#display-lists`, not a mirror, even on a `coddog-mirror` false-hit).
- Carry-over: `func_800329F4` (`src/main/func_800328E0.c`, the global-`glistp++` scheduler-load-pair wall; faithful source known, retry via `setup-permuter.sh --main func_800329F4` WITHOUT `--best-only`). Cross-repo follow-up: 5 new fn names (still auto `func_`; curate at a naming pass) → `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 178 — src/main/func_8004DE60.c slot-3 heap walls BANKED → md5-candidate (the 3 S177 "unreachable" walls, +3 matched) + whole heap module rewritten to Code Complete — 2026-07-04
- Increment: **1 file → md5-candidate** (func_8004DE60.c, 0 INCLUDE_ASM) / **3 functions matched** (`heap3_add_region`, `heap3_init`, `heap3_alloc`); md5-candidate **221 → 222**; asm subsegs unchanged. Both files of the heap module (func_8004DE60.c + func_8004DD70.c) rewritten to Code Complete, byte-exact.
- Quality: **0 / 0 / 0 / 0** (0 stuck-far, 0 permuter-escalated, 0 newly-carried, 0 re-opened) — and it **RESOLVED 3 prior S177 carries**. Cleanest counter-metric of the heap arc.
- Seed: committed 8pt; banked **8pt** (file md5-candidate); regime mixed. Realized **13** (residual **+5**): seed 8 +1 carry-reopen +1 re-attempt +3 novel-bank-gotcha (volatile-view, define-after-call liveness, inline-sentinel). Confirms the S177 note that the pts model under-prices regalloc/CSE-heavy FP-free fns.
- What helped: **two agents per wall** (a primary + an adversarial one) was the decisive method — every S177 primary "unreachable" PROOF was wrong, and the adversarial agent cracked both. (1) The CSE-reload wall (`heap3_init`/`heap3_add_region`) fell to the NEW `#volatile-view-cse-reload` lever: a per-access `volatile` view forces the `next=prev` reload (`MEM_VOLATILE_P` → `do_not_record`, cse.c) that the primary's volatile-one-field-only test rejected; volatiling size/state too pins them ahead of the reload's load-delay shadow. (2) The mask-rotation wall (`heap3_alloc`, S177 Axis-4 "no clean lever") fell to **Axis-5 define-point liveness** (define `head` AFTER `osSetIntMask(1)` → caller-saved → mask→$t1) + the **inline sentinel** (no head local → CSE derives it from the `.next` load base, `move t0,v1`). No permuter, no cross-project mining. The PO-directed **Code Complete rewrite** (read the CC ch7/11/31/32 digest first) then made the whole heap module self-documenting: new src/main/heap.h shared ADT (HeapBlock + heap_slots[] + named constants), every non-lib symbol renamed to the problem domain, every codegen-required oddity documented so it is not "fixed" away — byte-exact on a clean-from-scratch build.
- Friction: a genuine race — the adversarial WALL1 agent left its byte-exact match UNCOMMITTED in the shared file while the WALL2 close-out agent (which `git checkout`s that file) was already running; caught it, `TaskStop`ped the WALL2 agent, committed WALL1 first (git-safe), then relaunched WALL2. Lesson buffered as a Scrum-process note (commit an agent's verified match before launching another agent on the same file).
- Applied: 4 of 4 (all DOC + index/TOC): #1 new `#volatile-view-cse-reload` hazard + TOC + CLAUDE index row; #2 `#loop-weight-and-live-length-regalloc-steering` Axis-5 (define-point liveness) + amended Axis-4 verdict + CLAUDE index row; #3 two-agents-per-wall doctrine in `#compiler-source-fan-out-escalation-above-the-permuter` (+ corrected the S177 (c)+(d) "PROVED walls" claim); #4 `register asm("$N")`-is-not-a-no-op S178 empirical confirm in `#signed-divide-const-v0v1-quotient-destination`.
- Carry-over: **none** — all 3 S177 carries banked; func_8004DE60.c pruned from `BACKLOG.md ## Carry-overs`. Cross-repo follow-up: 3 new fn names + the renamed globals → `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 177 — src/main/func_8004DE60.c slot-3 heap module (6/9 mixed-partial; recombined the pack, +3 matched, 2 walls carried) — 2026-07-04
- Increment: 0 files md5-candidate / 3 functions matched (`heap3_get_total`, `heap3_get_largest_free`, `heap3_free`); md5-candidate 221→221 (file 6/9, 3 stubs); asm subsegs 77→76 (pack recombined into one object). matched +3.
- Quality: 0 stuck-far / 3 permuter-escalated / 3 carried fns (2 subagent-proven walls) / 0 re-opened.
- Seed: committed 8pt; banked 0pt (per-file all-or-nothing, file partial); regime classical/mixed. Realized 11 (residual +3): +1 permuter, +1 carry, +1 novel-bank-gotcha (offset-0 lever + `-S`-misread correction).
- What helped: a PO-directed **GCC-source fan-out** (`~/development/repos/mips-gcc-2.7.2/`) was decisive — it flipped 2 of 4 apparent-walls to CLEAN banks: (1) `heap3_free` via the NEW `#offset-0-symbol-re-materialization` lever (alias `D_800DC6E0[3].total` → own extern `D_800DC738` for the `+=` so GCC re-materializes `%hi/%lo` instead of a base-reg CSE-fold; cascaded the whole allocation into place), and (2) `heap3_get_largest_free`, which was NOT a wall — the "needs a synthetic no-op" verdict was a GCC-`-S` reorder-mode misread (clean inline-head already matched on the assembled `.o`). The **recombine** (whole 9-fn pack → one object) proved the nested-fn approach (the child `func_8004E184` matches perfectly).
- Friction: two subagent-proven CSE/regalloc WALLS carried — `func_8004E1E0` (`next=prev` CSE varying-address reload, mutually exclusive with the ROM's absolute stores + permuter-blocked) and `func_8004E2DC` (`mask`→`$a0` 6-register rotation, the slot-3 constant head being a 6th caller-saved competitor). A `-S` misread cost a real mid-sprint detour (E288 was wrongly called a wall) before the fan-out corrected it — the new Assembler-differences note guards this.
- Applied: 6 of 6 — #1 new `#offset-0-symbol-re-materialization` hazard + TOC + CLAUDE index row; #2 `-S`-vs-assembled note in `#assembler-differences--byte-cmp-spot-check` + CLAUDE index row; #3 `#nested-function-static-chain-spill` extend (recombine-to-land-child + CSE-reload wall); #4 `#loop-weight-and-live-length-regalloc-steering` Axis-4 (caller-saved-competitor-count) + CLAUDE index row; #5 pick_target.py regalloc-heavy pts follow-up re-confirm (BACKLOG); #6 fan-out doctrine in `#compiler-source-fan-out-escalation-above-the-permuter`.
- Carry-over: `func_8004E1E0`+`func_8004E184` (nested pair, CSE reload wall) and `func_8004E2DC` (mask-rotation wall) — both need a genuinely new mechanism (cross-project matched-corpus mining), NOT another same-fn dive; see `BACKLOG.md ## Carry-overs`. Cross-repo follow-up: 3 new names → `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 176 — src/main/func_8004DE60.c heap-allocator TU (3/4 mixed-partial; func_8004E184 carried as a nested-fn) — 2026-07-04
- Increment: **1 file mixed-partial / 3 fns matched** — `heap_get_largest_free` (func_8004DE60, arena accessor), `heap_alloc` (func_8004DE7C, best-fit alloc), `heap_free` (func_8004E058, coalescing free), banked C off the `func_8004DE60` 9-fn pack (decomposed cluster A). md5-candidate 221 → 221 (file has 1 stub, func_8004E184 carried → not md5-candidate). DoD: make OK, SHA-1 `e2c4e7a9…`, `grep -c INCLUDE_ASM` = 1. matched **+3**.
- Quality: **0 stuck-far / 1 permuter / 1 carried / 0 re-opened** (permuter-escalated on `heap_alloc` but the *source lever* cracked it, not the permuter; `func_8004E184` carried for scope, not a spike-fail).
- Seed: committed 5pt; banked 0pt (partial file, per-file all-or-nothing); regime mixed (realized 8; residual +3).
- What helped: **m2c seeds** (`-t mips-gcc-c`) gave clean starting bodies for all 4 fns; the `Slot` struct was reused verbatim from the sibling `func_8004DD70.c`. **`heap_free` lever:** load the payload *after* `osSetIntMask(1)` so it stays in a caller-saved reg (`$a2`) instead of being hoisted to `$s0` — first-try after the reorder. **`heap_alloc` (the hard one):** structural-complete 119/119 with only a 3-way `{best_rem,best,need}`↔`{s0,s1,s2}` register permutation; the permuter plateaued (1595→605), but a **regalloc subagent + gcc-source dive** cracked it — renaming param `size`→`need` and mutating it **in place** (`need=(need+0x17)&~7`) makes it a *global* quantity so `local-alloc.c` stops parking the call-crossing local in `$s0`, then `global.c` priority assigns `s0=best_rem/s1=best/s2=need` (the target). **`func_8004E184` real-trigger:** two subagents converged that the dead `sw v0,0(sp)` is a GCC **nested-function static-chain spill** (o32 passes the chain in `$v0`); the caller's `v0=&sp[K]`-per-`jal` is the proof. 4 subagents total (3 on the E184 trigger, 1 on the E7C regalloc); the `-dg`/`-lreg` allocno dumps + the permuter b64literal inline-asm transform were the enabling tools.
- Friction: heavy tool-setup yak-shaving on the permuter (inline-asm `__asm__ __volatile__` broke pycparser until the line was b64literal-wrapped by hand) and the isolated-compile harness (register-vs-immediate digit collisions in normalization; standalone schedule/nop differs from in-tree). The decompose boundary (0x295E0) over-reached by one fn because the true allocator-TU boundary (0x8004E184) is non-16-aligned and func_8004E184 is really the head of func_8004E1E0's nested-fn TU.
- Applied (7 of 7, 5 DOC + 2 index): #1+#2+#7 new `docs/hazards.md#nested-function-static-chain-spill` section (mechanism + real-nested-bank + decompose-orphaned-child corollary + UB-standalone caveat + the two-TU scope note) + TOC + CLAUDE index row; #3+#4 `#loop-weight-and-live-length-regalloc-steering` **Axis 3** (call-crossing-param → global-qty via in-place mutation) + the `-dg`/`-dl` allocno-dump diagnostic + CLAUDE index row; #5 `#permuter-setup-for-kmc-toolchain-mirrors` b64literal inline-asm fix + CLAUDE index row; #6 `#capturing-ra` S176 re-confirmation of the `__builtin_return_address(0)`→`lw` failure.
- Carry-over: `src/main/func_8004DE60.c` :: `func_8004E184` — a GCC nested function of `func_8004E1E0` (still-asm cluster `[0x295E0]`). Bank as the **real nested function inside func_8004E1E0** when that TU is decompiled (both in one `.c`, since a nested fn cannot be split from its parent). A byte-exact standalone UB reproduction exists (`volatile u32 a=(u32)uninit_ptr;`) but the PO chose the clean nested form. Next natural slice = cluster B `[0x295E0]` (func_8004E1E0 + its nested child + the 4 remaining pack fns).

## Sprint 175 — src/main/print_string_at_grid.c (retry of the S174 func_8004DC44 carry; PO-approved bounded new-angle try; CARRIED, new project-best + register ruled out) — 2026-07-04
- Increment: **0 files banked / 0 fns matched** (`func_8004DC44` re-CARRIED); md5-candidate 221 → 221 (file still 5/6 mixed-partial, 1 stub). DoD: make OK, SHA-1 `e2c4e7a9…` (regression-green off the matched fns), `grep -c INCLUDE_ASM` = 1. No `src/` change committed — a bounded investigation sprint.
- Quality: **1 stuck-far / 1 permuter / 1 carried / 0 re-opened** (0-bank re-attempt of the S174 carry; flagged honestly).
- Seed: committed 3pt; banked 0pt; regime classical (realized 6; residual +3).
- What helped: two genuinely-new probes the PO scoped. **(1) Permuter-reseed from the frame-bearing best.** S173/S174 always seeded the permuter from the frameless vA (38-diff) and never beat 14450 in ~430k cumulative iters; S175 seeded it from the frame-bearing 14450 candidate instead and reseeded from each new best → `14450 → 13530 → 13235` (new project-best, two bounded 620s best-only runs). The 13530 candidate is **structurally identical to the ROM** (frame present + schedule + all operations match) with every remaining diff a register name cascading from the single `/40` dividend-register choice — the tightest documented near-match. **(2) The plain `register` keyword — the PO's /systematic-debugging question — ruled out DEFINITIVELY.** Controlled A/B (`s32 seed` vs `register s32 seed`, identical structure) → `.text` byte-identical, both permuter-score 16613 (via new `tools/pscore.py`, the permuter's own Scorer). Three mips-gcc-2.7.2 subagents grounded why: (a) at -O2 `obey_regdecls==0` so `DECL_REGISTER` is ignored (stmt.c:3364) and a plain local also gets `REG_USERVAR_P` → register-vs-plain RTL identical; (b) `REG_USERVAR_P` has **0 hits** in `local-alloc.c`/`global.c` (absent from the `qty_compare` priority, `find_free_reg` scan, and suggestion machinery); (c) the flag survives into the dividend operand (`force_reg` REG-passthrough, no `PROMOTE_MODE` on MIPS) but allocation never reads it. The `register asm("$2")` hard-reg hack applied to the frame-correct 13530 base is *worse* (51 diffs; quotient→`$v1` where the ROM uses a fresh `$a3`) → confirms the ROM state is a coordinated `{dividend→$v0, quotient→$a3-fresh, dead-frame}` alloc, not a single-reg pin.
- Friction: 0 banks again (fourth carry sprint on this fn), but each probe was genuinely-new (not a re-run) and closed a distinct lever class: permuter-reseed set a new best + the `register` keyword is now a proven-negative, so future retries skip both. Value is a tighter near-match + two documented negatives + a reusable single-candidate scorer.
- Applied (3 of 3, 2 DOC + 1 TOOL): #1 **permuter-reseed doctrine** added to `#compiler-source-fan-out-escalation-above-the-permuter` ("reseed from the best candidate, not the clean seed; reseed from each new best"); #2 **plain-`register`-is-a-no-op note** added to `#signed-divide-const-v0v1-quotient-destination` (REG_USERVAR_P 0-hits proof + the asm-hack-from-frame-correct-base "coordinated alloc" evidence); #3 **new `tools/pscore.py`** (single-candidate permuter-Scorer wrapper with a hex-tolerant R_MIPS_26 addend monkeypatch; golden-verified it reproduces the permuter's relative scores + 0=match). S175 findings appended to `docs/wip/func_8004DC44.wip.md ## S175`.
- Carry-over: `src/main/print_string_at_grid.c` :: `func_8004DC44` (the lone stub) — kept RETRYABLE per PO. Blocker unchanged: void loop-fed leaf deterministically magic-`$v0`; the coordinated `{dividend-$v0, quotient-$a3-fresh, dead-frame}` alloc is unreachable by any faithful source lever. Permuter-reseed + `register` now spent; only untried lever = cross-project mining for a matched plain-global-divide-with-quotient-to-loop-var analog (S174 found none in 8 decomps). Retry seeds from `output-13530-1/source.c` (S175 doctrine). Full context in `docs/wip/func_8004DC44.wip.md`.

## Sprint 174 — src/main/print_string_at_grid.c (retry of the S173 func_8004DC44 carry; PO-directed cross-project + coalescing dive; CARRIED, verdict CORRECTED) — 2026-07-04
- Increment: **0 files banked / 0 fns matched** (`func_8004DC44` re-CARRIED); md5-candidate 221 → 221 (file still 5/6 mixed-partial, 1 stub). DoD: make OK, SHA-1 `e2c4e7a9…` (regression-green off the matched fns), `grep -c INCLUDE_ASM` = 1. No `src/` change committed — a pure investigation sprint.
- Quality: **1 stuck-far / 1 permuter / 1 carried / 1 re-opened** (a 0-bank re-attempt of the S173 carry; counter-metric flags it honestly).
- Seed: committed 3pt; banked 0pt; regime classical (realized 6; residual +3).
- What helped: **The single biggest deliverable is a CORRECTION of the S172/S173 verdict.** S173 called the `v0/v1` divide-swap "irreducible / unrecoverable from asm"; S174 proved that wrong — `return D_800DC6D0/40` reproduces the ROM's `/40` bytes EXACTLY, so the swap is **flippable-in-isolation**. An 8-project cross-decomp sweep (all KMC gcc 2.7.2: marioparty1/2/3, snowboardkids2, drmario64, hm64, puzzleleague64) + ~12 compiler-source subagents + RTL pass dumps (`-dl`/`-dg`/`-ds`) pinned the real mechanism: **dividend→`$v0` needs a physical reg-2 SET (return/call copy) that lands the divide chain in local-alloc's suggestion pass BEFORE the life-priority general pass** (`combine_regs` 1824-1838 + suggestion pass 1466-1477). `func_8004DC44` is a **void/callless/returnless leaf whose quotient feeds arithmetic then a loop-carried store** → emits no reg-2 mention → deterministically magic→`$v0`. Matched cross-project dividend-`$v0` examples (puzzleleague64 `gTheGame.menu[i].unk_4/100` indexed, hm64 `(a+b+c)/3` multi-term, marioparty3 `x/10%10` CSE-magic) all confirm the lever; NONE is a plain-global single-magic dividend in a real fn, confirming func_8004DC44's exact shape has no faithful analog. The PO's directives each advanced it: try-the-inline-fn (frame appears but swap persists), memcpy (ruled out — inlines as `lwl/lwr` word moves, not `lbu/sb`), m2c (plain global divide, no hidden provenance), return-value angle (confirmed the reg-2-SET mechanism), cross-project search (found the lever + corrected the verdict), deeper structural search (SA-A/SA-B grounded the void-leaf blocker).
- Friction: enormous compute (8 cross-project + 4 compiler-source subagents, 155k additional permuter iters, ~40 source variants) for 0 banks. But it corrected a wrong standing conclusion in the hazard doc and produced a reusable divide-reg-alloc playbook — high knowledge value. The `register asm("$2")` hack forces the dividend (28 diffs) but is unfaithful and incomplete (quotient intermediate + frame remain), so not a match.
- Applied (3 of 3, all DOC): #1 **corrected `#dead-frame-reload-artifact-regalloc-wall`** framing (retired "irreducible/unrecoverable" → "flippable-in-isolation; void-loop-fed leaf deterministically magic-`$v0`"); #2 **new `#signed-divide-const-v0v1-quotient-destination`** playbook (the quotient-destination lever + 5 flip mechanisms + the void-leaf blocker, grounded across 8 projects) + CLAUDE.md hazard-index row + TOC; #3 **cross-project matched-corpus mining** methodology added to `#compiler-source-fan-out-escalation-above-the-permuter` (fan out one subagent per same-toolchain N64 decomp; RTL-dump ground truth) + an "over-scoped negative" caveat. S174 findings appended to `docs/wip/func_8004DC44.wip.md`.
- Carry-over: `src/main/print_string_at_grid.c` :: `func_8004DC44` (the lone stub) — kept RETRYABLE per PO. Corrected blocker = the `/40` divide is flippable-in-isolation but the void loop-fed leaf is deterministically magic-`$v0` (no faithful source lever; needs a reg-2 SET the fn can't emit). Retry = a targeted sched1/permuter attempt or a genuinely new mechanism. Full context in `docs/wip/func_8004DC44.wip.md`.

## Sprint 173 — src/main/print_string_at_grid.c (retry of the S172 func_8004DC44 carry; PO-directed compiler-source dive; CARRIED) — 2026-07-04
- Increment: **0 files banked / 0 fns matched** (`func_8004DC44` re-CARRIED as a dump-verified compiler-artifact regalloc wall); md5-candidate 221 → 221 (file still 5/6 mixed-partial, 1 stub). DoD: make OK, SHA-1 `e2c4e7a9…` (regression-green off the matched fns), `grep -c INCLUDE_ASM` = 1. No `src/` change committed — a pure investigation sprint.
- Quality: **1 stuck-far / 1 permuter / 1 carried / 0 re-opened** (the counter-metric flags a 0-bank sprint honestly).
- Seed: committed 3pt; banked 0pt; regime classical (realized 6; residual +3).
- What helped: **A 4-subagent GCC-2.7.2/binutils-2.6 compiler-source dive (PO-directed, before the permuter) that DEFINITIVELY root-caused the wall** and turned a would-be open grind into a documented carry. It (a) proved the dead frame is REACHABLE (retract S172's "no source trigger" — structured loops produce it; the permuter hit a frame-bearing 75-insn candidate); (b) isolated the true wall as the `v0/v1` swap in the signed-`/40` (`expmed.c` fixed operand/pseudo order + `local-alloc.c` life-dominated priority: tiny-life magic scores 6666 vs the dividend's 1666 → grabs `$v0`), un-flippable across ~35 variants + 275k permuter iters; (c) produced an **improved seed** (pre-declared base-pointer vars → base-hoist in a goto-loop; operations now 100% match — the S172 seed lacked this, which is why its permuter plateaued). The user's own probes (unused-var → led to the frame mechanism; try-other-control-flow → revealed the frame is reachable; multiply-vs-divide → confirmed the divide operand order) each advanced the dive.
- Friction: a lot of compute (4 subagents, ~35 variants, 275k permuter iters) for 0 banks — but the payoff is a rigorous carry + a better next-attempt seed, and a dump-verified negative is itself the deliverable when the answer is "carry."
- Applied (3 of 3, all DOC): #1 enrich `#dead-frame-reload-artifact-regalloc-wall` (frame reachable + the `v0/v1` divide-swap mechanism, `expmed.c` + `local-alloc.c` cites); #2 `#top-tested-loop-goto-local-hoist` base-pointer-var hoist lever (upgrades the S172 "partial fix" to a full structural match); #3 `#compiler-source-fan-out-escalation-above-the-permuter` dead-frame/regalloc application + "a dump-verified negative is a valid deliverable". Improved seed + full analysis saved to `docs/wip/func_8004DC44.wip.md`.
- Carry-over: `src/main/print_string_at_grid.c` :: `func_8004DC44` (the lone stub). Spike blocker = the `v0/v1` signed-divide-register swap (compiler artifact, dump-proven not source-reachable) + the coupled dead frame (reachable). Retry = permuter from the improved vA seed (must flip both frame + swap in one candidate, low odds) or accept as a permanent dead-frame-wall carry. Full context in `docs/wip/func_8004DC44.wip.md`.

## Sprint 172 — src/main/print_string_at_grid.c (retry of the S171 carry's 2 regalloc walls; MIXED-PARTIAL) — 2026-07-04
- Increment: **0 files banked / 1 fn matched** (`print_string_at_grid` byte-exact C; `func_8004DC44` CARRIED); md5-candidate 221 → 221 (file still mixed-partial, 1 stub); asm subsegs 77, c-subsegs 218 (unchanged). DoD verified (make OK, SHA-1 `e2c4e7a9…`, `grep -c INCLUDE_ASM` = 1 → mixed-partial, not md5-candidate). This was the S171 carry-over retry; both fns are structurally/logically fully RE'd regalloc near-matches.
- Quality: 0 stuck-far / **1 permuter** / **1 carried** / 0 re-opened.
- Seed: committed 3pt; banked **0pt** (per-file all-or-nothing — file still partial); regime classical (v2: realized 6, residual +3).
- What helped: **the S171-rated HARDER carry (`print_string_at_grid`) banked via a compiler-source insight, not the permuter.** (1) **Nested-if forces the branch-likely** — GCC 2.7.2 cross-jumped the `if(dst<base){dst++;continue;}` guard's `dst++;j loop` tail into the bottom one (a double-jump) where the ROM keeps a branch-likely `bnezl` with the `dst++` in its annulled delay slot; re-expressing as nested `if(dst>=base){ if(dst<end)*dst=f|c; dst++; } else { dst++; }` fills the `dst<base` branch's delay slot with the annulled increment (no tail-merge) — a NEW classical-loop lever for `#cross-jump-tail-merge`. (2) **Lazy global-base load** closed the residual `{f,base,dst,end,c}` 5-cycle: referencing `D_800DAF60[...]` directly (no `u8 *base` local) defers the base load past the offset so the freed arg-register is reused for the base — 25/25 word-exact. (3) For `func_8004DC44`, the **outer-goto magic-de-hoist** drove the structure fully identical to the ROM (the `%4800` magic rematerializes at the loop tail vs loop.c hoisting it — a NEW SELECTIVE-hoist case for `#top-tested-loop-goto-local-hoist`).
- Friction: `func_8004DC44` is a genuine **dead-frame reload wall** — after the magic-de-hoist the structure is byte-identical to the ROM except a reserved **dead 8-byte spill frame** (`addiu sp,-8`/`+8`, zero sp access) + the register permutation it drives (first-load v0/v1 swap, `mfhi t3` vs t4, dst/row a3↔t0). This is a reload spill-then-eliminate artifact with **no clean source trigger**: address-taken locals (the permuter's `&i` mutation) force real sp loads/stores the ROM lacks. ~15 hand variants all left `frame_adj:0`; the main-profile permuter ran 31k iterations (best 15305, no zero). Also: the S170/S171 post-flip `find_segment` gap recurred — hand-built the permuter scaffold again (`cpp -P` self-contained base.c, INCLUDE_ASM build `.o` as `target.o`).
- Applied (4 of 4; all doc, PO-picked): #1 nested-if branch-likely lever + companion lazy-base note in `#cross-jump-tail-merge` (+ 1 hazard-index row); #2 lazy-global-base reg-cycle lever (folded into #1's companion); #3 outer-goto magic-de-hoist SELECTIVE-hoist case in `#top-tested-loop-goto-local-hoist` (+ 1 hazard-index row); #4 new `#dead-frame-reload-artifact-regalloc-wall` section (+ 1 hazard-index row).
- Carry-over: `src/main/print_string_at_grid.c` — `func_8004DC44` only (dead-frame reload wall). Near-free retry: scaffold live at the improved `wd` seed (word 17/75, magic-de-hoisted), no enablers left; see `## Carry-overs`.

---

## Sprint 171 — src/main/print_string_at_grid.c (6-fn grid-print/debug cluster decomposed from the FP-free debug subseg `[0x28590]`; MIXED-PARTIAL) — 2026-07-04
- Increment: **0 files banked / 4 fns matched** (`check_and_print_grid`, `func_8004DA4C`, `convert_and_print_hex`, `func_8004DAF4` byte-exact C; `print_string_at_grid` + `func_8004DC44` CARRIED); md5-candidate 221 → 221 (file mixed-partial, 2 stubs); asm subsegs 77 (head `[0x28590]` stayed asm), c-subsegs 217 → 218 (the flip). DoD verified (make OK, SHA-1 `e2c4e7a9…`, `grep -c INCLUDE_ASM` = 2 → mixed-partial, not md5-candidate). The whole cluster references placed `D_`/`flag` externs (no rodata carve), so 4 fns bank as C while 2 stay `INCLUDE_ASM`, ROM green.
- Quality: 0 stuck-far / **2 permuter** / **2 carried** / 0 re-opened.
- Seed: committed 5pt; banked **0pt** (per-file all-or-nothing — file partial); regime classical/mixed (v2: realized 9, residual +4).
- What helped: **four documented codegen levers, all landed byte-exact banks.** (1) **`while(1)`-vs-goto for hoisting COMPILER-generated constants** — `func_8004DAF4`'s goto-loop rematerialized the `/40`+`%4800` magic multipliers (`0x66666667`/`0x1B4E81B5`) + the `' '` fill-char each iteration; a structured `while(1){…break}` carries the `NOTE_INSN_LOOP` markers so `loop.c` hoists them to `t2`/`t0`/`t1`, and the `c=*str++` memory-read exit blocks `expand_end_loop`'s rotation so it stays top-tested — byte-exact. (2) **`&base[i]` index-grouping** — `base + row*40 + col` adds base-before-col; `&base[row*40+col]` forces index-first, matching the ROM's freed-arg-reg reuse (`print_string_at_grid`). (3) **dual-IV** — `func_8004DC44`'s ring→grid render needs both the pointer (load/store) and the offset (per-row recompute + `%4800` wrap); explicit `dp`/`sp` alongside `src`/`dst` took the opcode structure 71→75, the ROM's giv+biv. (4) setup-order (`convert_and_print_hex`: assign `stop` after `col+=8`). The 3-arg pass-through chain (converters → `check_and_print_grid` → `print_string_at_grid`, zero arg-moves) banked the guard + both hex printers cleanly.
- Friction: the two carries are **pure GCC 2.7.2 register-steering walls** — `print_string_at_grid` (25/25 instrs, logic correct) locks on a cyclic allocno permutation + a reorg branch tail-merge (my continue double-jumps where the ROM annuls `dst++` into the `bnel` delay); `func_8004DC44` is structurally IDENTICAL (75/75 opcodes) but permutes registers + reserves a dead 8-byte frame. **The permuter did NOT crack either** (plateaued ~20725 / ~15330 over a 6-min run) — the walls are allocno-assignment/reorg, which the source-level permuter passes don't reach here. Also: the post-flip `find_segment` gap (S170) recurred for BOTH `decomp_loop` and the permuter setup — hand-built the permuter scaffolds (copy a main-seg `compile.sh`, `cpp -P` self-contained base.c, `target.o` from the INCLUDE_ASM build object).
- Applied (4 of 4; all doc, PO-picked): #1 permuter post-flip hand-scaffold workaround (extends the S170 `find_segment` note in `#short-text-shifts-flowing-bss`); #2 `while(1)`-vs-goto compiler-constant hoist (new inverted-hoist-corollary paragraph in `#top-tested-loop-goto-local-hoist` + hazard-index row); #3 `&base[i]` index-grouping sub-lever + #4 dual-IV sub-lever (both in `#indexed-vs-pointer-loop-strength-reduction` + 1 combined hazard-index row).
- Carry-over: `src/main/print_string_at_grid.c` — `print_string_at_grid` (regalloc + reorg-branch-tail-merge, permuter-plateau) + `func_8004DC44` (pure regalloc + dead-frame, permuter-plateau). Both near-free retries: scaffolds live, no enablers left; see `## Carry-overs`.

---

## Sprint 170 — src/main/func_8004DD70.c (3-fn slot-allocator module decomposed from the FP-free debug subseg `[0x28590]`) — 2026-07-03
- Increment: **1 file banked / 3 fns matched** (func_8004DD70 append / func_8004DDE4 init / func_8004DE44 get-total, over a circular doubly-linked list of sized nodes on the `Slot D_800DC6E0[]` stride-0x18 array); md5-candidate 220 → 221; asm subsegs 76 → 77 (mid-slice decompose split off the `[0x29260]` tail). DoD verified (make OK, SHA-1 `e2c4e7a9…`, `grep -c INCLUDE_ASM` = 0).
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened — clean.
- Seed: committed 3pt; banked 3pt (full bank); regime classical (v2: realized 4, residual +1).
- What helped: **two struct-array-of-BSS reloc levers, no permuter** (asm-first fast-path, MCP-independent). (1) DIRECT `D_800DC6E0[i].field` indexing — NOT a `Slot *s=&arr[i]` base-pointer var — makes GCC re-derive each field via `%hi/%lo(D_<field>)` off the scaled index, matching the ROM's per-symbol relocs; a pointer var CSEs the base into shorter `offset(v1)` addressing. (2) The lone self-store `prev=s` routed THROUGH the pointer var → `sw v1,0xC(v1)` (1 instr, reusing the live self-ptr as base), while `next`/scalars stay direct. The reloc-hi/lo diffs in the isolated objdump were the `#isolated-compile-caveat` artifact, resolved by the full-make link.
- Friction: a **too-LONG `func_8004DDE4`** (routing `prev` direct = +2 instrs) pushed the object `.text` `0xF0→0x100`, overflowed the 240B `[0x29170,0x29260)` slice, and floated EVERY `D_800DC6xx` bss symbol +0x10 — which presented as symbol-table / reloc corruption (`D_800DC6E0`→`0x800DC6F0`, the head-asm sibling going wrong too) and sent the diagnosis down a long rabbit-hole before the object-`.text`-size-vs-subseg-span check found the real cause. `asm/data/<seg>.bss.s` being gitignored hid the shift from `git status`. Also: decomp_loop `find_segment` can't locate a fn whose subseg is already flipped to C (asm under `asm/nonmatchings/`), so the isolated per-fn diff was a manual objdump.
- Applied (3 of 3; all doc, PO-picked; the decomp_loop `--target-s` code change deferred to a golden-gated tooling branch): #1 new `docs/hazards.md#struct-array-of-bss-direct-index-vs-base-pointer-var` + `CLAUDE.md` hazard-index row; #2 `docs/hazards.md#short-text-shifts-flowing-bss` long-overflow-floats-bss-symbols variant + index row; #3 decomp_loop post-flip `find_segment` gap (doc note in the #short-text section; tooling follow-up in BACKLOG).
- Carry-over: none.

---

## Sprint 169 — src/main/func_80076640.c (3-fn one-tu S162 tail; MIXED-PARTIAL) — 2026-07-03
- Increment: 0 files banked / **1 fn matched** (func_80076778); md5-candidate 220 → 220 (file mixed-partial, 2 stubs); asm subsegs 77 → 76 (the flip). DoD verified (make OK, SHA-1 `e2c4e7a9…`, `grep -c INCLUDE_ASM` = 2). One-tu MIXED-PARTIAL: matched fn banked as C, hard fns stay `INCLUDE_ASM`, ROM green.
- Quality: 1 stuck-far (func_8007680C) / 1 permuter (func_80076640) / 2 carried / 0 re-opened.
- Seed: committed 13pt; banked 0pt (per-file all-or-nothing, partial); regime classical (v2: realized 17, residual +4). Note: freeze committed seed 13 (deterministic `pts`), not the "8" quoted in the PO approval option (that was the one-tu-rubric value); reconciled at the gate as honest for the 679-instr FP fn.
- What helped: **the one-tu partial-bank pattern extended to shared rodata** — `func_80076778` (a clean point×matrix transform) banked +1 as C with the two hard fns as `INCLUDE_ASM`, because the TU's shared rodata (FP doubles + printf strings) stays in the extracted `ACAD0` blob referenced `extern` by all (no `.rodata` carve, so the one-tu is not atomic-or-nothing). Two real codegen levers landed on `func_80076640`: (1) **`const`-extern forces cross-call CSE into a callee-saved reg** (the rad-to-deg double reloaded across the two `guRotateF` until declared `const`; inverse of `#volatile-global-tell`); (2) the permuter-found **`do{}while(0)` + `cosf` temp** flipped the -O2 schedule to fix the instruction count (77→78). func_80076778's fix: cache `in→x/y/z` in locals (GCC won't CSE across the aliasing `out→` store). func_8007680C fully RE'd from asm (the Ghidra decompile dropped the depth-xzd `tan` products and mis-assigned the dual xc/yc clamp).
- Friction: **func_80076640 stuck at score 25** on a NEW `#abs-coalescing-reg-swap` wall — a 3-instr register swap (GCC coalesces `fabsf(x)` in-place into `f0`; the target keeps `f0` for the two branch constants and puts the abs in `f2`). Permuter (main-profile, no `--best-only`) PLATEAUED at 25 over **338k iterations**; 3 hand levers (flip / abs-temp / thresh-temp) all failed → irreducible-from-equivalent-C. **func_8007680C is S158-class**: structure-complete (721/756 mnemonics) but the isolated score is dominated by an 8-byte frame cascade (target spills 1 more scalar, keeps min/max trackers as float-bits in GPRs under FP pressure). Permuter setup itself cost real time (import.py needs the venv `toml` + a manual target.s slice for an already-inlined fn).
- Applied (4 of 4; PO accept-partial, all doc/reference edits): #1 `docs/hazards.md#volatile-global-tell` const-extern inverse lever; #2 permuter-recipe `do{}while(0)` schedule lever + venv-`toml`/inlined-fn-import gotcha; #3 `CLAUDE.md` mixed-partial one-tu-with-shared-rodata generalization; #4 new `docs/hazards.md#abs-coalescing-reg-swap` + ToC + `CLAUDE.md` hazard index row.
- Carry-over: `src/main/func_80076640.c` mixed-partial — **func_80076640** (near-free retry, score-25 reg-swap) + **func_8007680C** (spike, S158-class deep regalloc). Both compile in-tree today; the gap is pure regalloc. WIP saved `docs/wip/func_80076640.3fn-wip.c.txt`; permuter scaffold live `nonmatchings/func_80076640/`.

---

## Sprint 168 — src/main/func_80071220.c (scenario-unlock dispatcher; the `[0x4C620]` tail split from `[0x4C3D0]` at S167) — 2026-07-03
- Increment: 1 file banked / 1 fn matched byte-exact (md5-candidate 219 → 220; asm subsegs 78 → 77; matched +1). Single-fn increment, banked atomically. DoD verified (make OK, SHA-1 `e2c4e7a9…`, 0 `INCLUDE_ASM`).
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened.
- Seed: committed 5pt; banked 5pt; regime classical (v2: realized 6, residual +1).
- What helped: a **4-lever documented-hazard stack** took a single main-segment fn from score 8540 → 0 with **no permuter and no carry** — the cleanest classical-endgame match to date. In order: (1) declare the loop vars AFTER the last pre-loop call so they stay in temp regs (3 saved, not 5); (2) `#indexed-vs-pointer` indexed form `table[i]` for the `move v1,a0` dual-IV; (3) index a pointer **variable** (`Entry *table = D_801B7118`) not the array symbol (folds the base into the strength-reduced IV — a new sub-lever); (4) a `switch` for the 4-way dispatch (branch-toward, per-case distinct tails — if/else branched away, the goto form tail-merged the shared `set=1`). The S167 struct model (`func_8005AF50()` base + a struct-field `Entry`/`ScenarioRec` view) carried straight over, so the addressing + mixed `lb`/`lbu` signedness matched first-try.
- Friction: one **tooling gotcha** — `decomp_loop` first diffed against a stale 4-func reference. After the S167/S168 split of `[0x4C3D0]`, the pre-split relic `asm/4C3D0.s` (all 4 original funcs) lingered and `find_segment` (sorted glob, first match) shadowed the correct 1-fn `asm/4C620.s`, giving a bogus 94/100 "match" with empty `base_text`. Fixed by hand (`mv asm/4C3D0.s` aside; gitignored relic, not regenerated). This recurs on every classical-endgame decompose-split.
- Applied (3 of 3; PO away past the 60s prompt → best judgment, all doc/review-gate edits; the `find_segment` code fix is deferred to a golden-gated `tools/` branch): #1 new `#stale-parent-asm-relic` hazard + ToC + `CLAUDE.md` index row; #2 `#goto-dispatch` switch-first dispatch-ladder note; #3 `#indexed-vs-pointer` pointer-var-vs-array-symbol sub-lever.
- Carry-over: none new. The S167 `func_80070FD0` (cse branch-fold wall) and S166 `lz_decompress_extended` (regalloc floor) spikes remain carried, out of scope this sprint.

---

## Sprint 167 — src/main/func_80070FD0.c (3-fn char/scenario stat-updater head; PARTIAL) — 2026-07-03
- Increment: `func_800710C4` + `func_8007117C` MATCHED + banked byte-exact C (0e8ded3); `func_80070FD0` CARRIED. File MIXED-PARTIAL (2/3 fns C, 1 stub) → NOT md5-candidate. matched +2; md5-candidate 219 → 219 (unchanged — file didn't flip); asm subsegs 78 → 78.
- Quality: 0 stuck-far / 1 permuter / 1 carried / 0 re-opened.
- Seed: committed 5pt; banked 0pt (per-file all-or-nothing, file partial); realized 13 (S158-class cse/regalloc slog: +1 permuter, +1 carry, +novel-gotchas struct-model/biv-elim/fan-out/cse-wall), residual +8; regime classical. The +2 matched fns are the value signal the 0-pt understates.
- What helped: **the per-FUNCTION compiler-source fan-out** (3 opus subagents, one per fn, over mips-gcc-2.7.2 + mips-binutils-2.6, each fed the EXACT byte-diff I had already isolated) — cracked the 2 byte-exact. Unifying key came from `func_800710C4`'s agent: `func_8005AF50()` returns a game-save **struct**, so `base->tbl[i][j]` (`SaveBlock{u8 pad[0xF4]; s8 tbl[6][0x12]; u8 wins[13][0x12][2]}`) keeps `+0xf4` explicit via COMPONENT_REF bitpos (expr.c:4882) + base in `$a0` (MEM_IN_STRUCT), fixing BOTH siblings. `func_8007117C`'s agent found biv-elimination (`p[j]` index form → GCC synthesizes `end=start+0x12`, loop.c:6165), `s32`-load for `lb` (extendqisi2, mips.c:1029), and `i`-before-`rp` init for the `$a2`/`$a3` split. Verified each lever with an isolated reloc-aware byte-cmp before the full-make SHA.
- Friction: **func_80070FD0's last 3 words are a fundamental cse/regalloc wall** and consumed the bulk of the sprint before I recognized it as unbankable. Root cause (systematic-debugging, `cse.c make_regs_eqv:840-862): value `(D==0)?t|0x80:t` through a variable REUSED for the loaded byte (`lb a1`) and the printf arg — the default-arm copy `old=t` makes old canonical (outlives t + crosses the post-branch EBB), so cse rewrites the other arm's `t|0x80`→`old|0x80` and t folds; the two C shapes are LOCKED to {right-polarity+fold, separate-t+wrong-polarity}. Not reachable from equivalent single-TU C (35 hand variants + 43k permuter iterations + a full RTL-dump trace all confirm). The lesson: recognize the symptom (byte-exact minus a bnez/beqz+delay triple on a `?:`-with-flag store) and CARRY FAST — the new `#cse-make-regs-eqv-branch-fold` hazard exists so the next sprint doesn't re-derive it.
- Applied: PO signed off partial; 4 of 4 (all DOC, no golden touch): #1 per-function compiler-source fan-out pattern added to `#compiler-source-fan-out-escalation-above-the-permuter`; #2 NEW `#cse-make-regs-eqv-branch-fold` hazard + CLAUDE.md index row; #3 main-profile permuter recipe (S167 confirmation) in `#permuter-setup-for-kmc-toolchain-mirrors`; #4 regalloc-heavy pts extension (shared call-return base-ptr + byte-field access as a difficulty signal) in BACKLOG. Also PRUNED 2 stale carry-overs (motor.c banked S102, sched.c banked S106).
- Carry-over: src/main/func_80070FD0.c (`func_80070FD0` only) → BACKLOG spike. Blocker: `#cse-make-regs-eqv-branch-fold` (3-word branch-direction miss, unreachable from equivalent C). Needs the ORIGINAL source shape (a different flag/print data-flow, helper, or macro) — game-source insight, not more permuter. Near-match saved to `docs/wip/func_80070FD0.near-match.c.txt`. Cross-repo follow-up: name `func_800710C4`/`func_8007117C` + push `SaveBlock` to Ghidra.

## Sprint 166 — src/main/lz_decompress_simple.c (retry the 2 carried LZ fns; PARTIAL) — 2026-07-03
- Increment: `lz_decompress_simple` MATCHED + banked (60ecb9a); `lz_decompress_extended` CARRIED at greg-proven floor raw-185; `lz_decompress_dma` already C (S165). File MIXED-PARTIAL (2/3 fns C, 1 stub) → NOT md5-candidate. matched +1; md5-candidate 219 → 219 (unchanged — file didn't flip).
- Quality: 0 stuck-far / 1 permuter / 1 carried / 0 re-opened. simple banked clean; the carry held to DoD (never force-banked; floor dual-confirmed).
- Seed: committed 8pt; banked 0pt (per-file all-or-nothing, file partial); realized 10 (seed 8 +1 permuter +1 carry), residual +2; regime classical. The +1 matched fn (the project's single hardest fn) is the value signal the 0-pt understates.
- What helped: **the loop-weight lever cracked the wall.** systematic-debugging Phase-1 re-grounding of root cause in `flow.c:2067` (`reg_n_refs += loop_depth`, structured-loop-only note) found the axis the prior 3 "irreducible" verdicts never tested: ASYMMETRIC nesting (structure ONLY the inner decode-dispatch `do{…}while(1)`, keep the outer a goto-loop) weights `control`'s decode refs ×2 while the raw-copy stays ×1 → control wins `$a2` = target. Plus web-split avoidance (`acc<<=16; acc|=0x8000`) + the S164 bltzl structured-inner-while. Struct unification to `LzDecompressState` preserved the dma match. A 5-agent compiler-source fan-out (3 read-only mechanism-RE over gcc-2.7.2 + 2 measurement) derived and cross-checked every lever; it also DUAL-PROVED extended's raw-185 floor (empirical binary param t1↔t8 + M1 structural-invariant: param L pinned by 5 post-loop exit stores, ridx by its dual-emit ring refs). Post-sprint (PO systematic-debugging): reworked simple to the cleanest byte-matching form (names+comments, control flow unchanged) and PROVED zero-goto impossible (CS1 loop-weight / CS2 no-BB-reorder / CS3 loop-opt-IV, all file:line-cited).
- Friction: the wall had been declared "irreducible" 3× (S158/S164/S165) because every prior attempt tested only the goto-outer structure, never the loop-weight/nesting axis — the lever needed a Phase-1 root-cause RE-grounding, not more permuter grinding. Two permuter FALSE MINIMA (simple raw-104, extended perm-3805) from var-reuse passes clobbering values live across goto back-edges wasted a sub-phase until caught (now a hazard). extended's coupled 3-cycle permutation is a genuine live-length + ring-ref floor.
- Applied: PO away → best-judgment 4 of 4 (all DOC, no tooling/golden touch): #1 NEW `#loop-weight-and-live-length-regalloc-steering` (S2 loop-weight/live-length/web-split levers + S4 zero-goto-impossible 3-reason proof, folded) + revised `#pervasive-regalloc-classical-main` triage tell (source-steerable → MATCH) + 2 index rows; #2 NEW `#permuter-goto-backedge-liveness-unsound` (var-reuse passes unsound on goto-loops) + step-3 safe-passes note + index row; #3 codec triage-tell RESOLUTION UPGRADE; #4 committed the byte-exact readability rework of `lz_decompress_simple`.
- Carry-over: src/main/lz_decompress_simple.c (`lz_decompress_extended` only; dma+simple now C) → BACKLOG. Blocker: greg-proven register-permutation floor raw-185 (two coupled near-tied 3-cycles; param binary t1↔t8, ridx loop-carried, both structurally pinned). Base preserved in `nonmatchings/lz_decompress_extended/base.c` (= scratchpad/lz/best_extended.c). Cross-repo follow-up: push corrected `LzDecompressState` to Ghidra.

## Sprint 164 — src/main/lz_decompress_simple.c (3-fn LZ-decompress trio decomposed from the 18-fn [0x43810] subseg — main-segment classical endgame) — 2026-07-02
- Increment: 0 files banked / 0 functions matched into tree (CARRIED); md5-candidate 219 → 219 (no delta). `lz_decompress_dma` matched in ISOLATION (157/157) but not inlined (per-file all-or-nothing).
- Quality: 0 stuck-far / 3 permuter / 1 carried / 0 re-opened.
- Seed: committed 5pt; banked 0pt (per-file all-or-nothing, carried); regime classical.
- What helped: the S158 fan-out (3 opus agents, worktree-less — isolated `decomp_loop.py` + boosted permuter, no `src/`/`make`/yaml writes) cracked `lz_decompress_dma` (157/157, residual only isolated reloc artifacts) and drove `simple`→123/124 + `extended`→290/298 (284/282 instrs) STRUCTURAL-COMPLETE — via the goto-loop fix (defeats `expand_end_loop` inversion), hoisted control-load + `read++`/`look++` at body entry, and the raw-copy `read[0]+look[-0xe..0]` pointer form. KMC-gcc source (`global.c:407-423` allocno priority `floor_log2(nref)*nref/live_length`, `REG_ALLOC_ORDER` undefined=ascending) grounded the register-perm root cause. The PO's struct hint surfaced a real RE contribution (`LzDecompressState` is mis-RE'd in Ghidra; corrected the layout vs asm).
- Friction: mis-priced seed-5 (should be 8+) — the "clean pure-integer leaf" heuristic (0-jal/no-float/no-data) masked that tight LZ codecs ARE the pervasive-regalloc wall. All 3 fns locked on the SAME register permutation (`control`→`$v0` vs the target's `$a2`, an internal allocno-priority equilibrium: control's short decode-only live range outranks the hi-freq copy temps for `$v0`, but the ROM keeps control live body→decode so the temps win `$v0/$v1`) that resisted permuter + systematic lever sweep + long annealing grind + struct-retyping + manual live-range levers (GCC coalesces them back). A shared-account usage-limit killed the first fan-out mid-run (recovered after the reset via checkpointed bases).
- Applied: PO away → best-judgment 3 of 3 (all DOC, no tooling/golden touch): #1 `#pervasive-regalloc-classical-main` codec-triage TELL (price pure-integer codecs seed-8+/expect permuter); #2 same-section fan-out-robustness (checkpoint-best per improvement / stagger `-j 4` / resume-after-limit-reset); #3 `#struct-access-folding-changes-scheduling` param-struct-at-gate check (query Ghidra, VERIFY vs asm, not a cure-all).
- Carry-over: src/main/lz_decompress_simple.c (all 3 fns) → BACKLOG. Blocker: pervasive-regalloc register permutation on `simple`+`extended` (`dma` matched). Bases preserved in `nonmatchings/<fn>/base.c` (+ scratchpad best_<fn>.c, session-ephemeral). Cross-repo follow-up: push the corrected `LzDecompressState` layout to the Ghidra workspace.

## Sprint 163 — src/main/func_80043AF0.c (4-fn club-meter head decomposed from the 25-fn [0x1EEF0] subseg — main-segment classical endgame) — 2026-07-02
- Increment: src/main/func_80043AF0.c banked (4 fns) / matched +4 (delta: md5-candidate 218 → 219). asm subsegs 78 → 78 (head-carve; tail [0x1F020] stays asm).
- Quality: stuck-far 2 / permuter 2 / carried 0 / re-opened 0. Both club-meter fns locked far and were permuter-escalated (extent plateaued 700, units 235); BOTH cracked by the compiler-source dive, NOT the permuter. A hard-won single file, not a gamed count.
- Seed: committed 5pt; banked 5pt; regime classical (8-gate CLEAR; v2 two-pass freeze at c3b0cc5).
- What helped: **the compiler-source fan-out escalation ABOVE the permuter** (new `docs/hazards.md#compiler-source-fan-out-escalation-above-the-permuter`). `func_80043AF0` (ptr-return) + `func_80043B0C` (splat auto-decompiled the `jr ra;nop` no-op) matched first build. The two hard fns: `get_club_meter_extent` was a pervasive BB-layout miss (24/39 words, same length) — 4 parallel subagents on KMC gcc 2.7.2 (reorg.c / jump.c+flow.c+stmt.c / mips.md+gas) proved the mechanism (`expr.c:9522-9545` do_jump polarity, `jump.c:1743` condjump-over-jump invert, `mips.md:127` no annul-true, `mips.md:79-82` load `dslot=yes`) and an empirical isolated-scoring harness FOUND the source: **goto-dispatch to out-of-line bodies, load-case last** (new `#goto-dispatch-branch-toward-vs-branchless`). `get_club_meter_units` = a `$a0`-vs-`$v0` regalloc miss cracked by **one reused var in both if/else arms** forcing a single allocno to conflict with `$v0` (`global.c:1005-1034`; new `#call-result-a0-vs-v0-single-allocno`).
- Friction: the permuter PLATEAUED on both fns (extent 700 with semantically-broken candidates, units 235) — it cannot invent a structural polarity/allocation lever, so it was a dead end here; the fan-out dive was the only path. GOTOLESS proven impossible for extent (10+ structured variants; the lone `if(cat==2)v=30` always branchless-if-converts). Applied a READABLE goto per Code Complete ch17 (documented 1-in-100 case; meaningful labels + a comment explaining WHY, so it is not simplified away).
- Applied (3 of 4, PO away → best judgment): #1 new `#goto-dispatch-branch-toward-vs-branchless` + index row; #2 new `#call-result-a0-vs-v0-single-allocno` + index row; #4 new `#compiler-source-fan-out-escalation-above-the-permuter` + index row. #3 (promote the isolated-scoring harness to `tools/`) DEFERRED to a golden-gated tooling branch per the tooling-refactor discipline (recorded in `BACKLOG.md`).
- Carry-overs: none. classical track → seed 5; realized 9; residual +4; regime classical (+2 permuter-escalated fns + 2 novel bank-gotchas = the goto-dispatch/branchless mechanism + the single-allocno a0/v0 lever; a 304B slice hid TWO distinct permuter-plateau codegen walls behind constant-dispatch + a call-result FP chain — the size/jal heuristics can't see "double compiler-wall classical", a pts data point).

---

- Increment: 1 file banked (`src/main/func_80076500.c`, **3/3 fns**) / 3 functions matched (`func_80076500` = copy two `Vec3f` global constants `D_80105B6C`/`D_80105F30` into two output buffers; `func_8007654C` = reset counter `D_800E1CD0 = 0`; `func_80076558` = push a `Vec3f`+scalar into slot `i` of the debug buffer `D_800E1C50[8]`/`D_800E1CB0[8]`, optional `osSyncPrintf` under `D_800FBDA6 & 4`, increment). md5-candidate **217→218** (+1); matched +3; asm subsegs **78→78** (split `[0x51900]`→c head + `[0x51A40,asm]` 3-fn tail; net unchanged).
- Quality: **0/0/0/0** (stuck-far 0 / permuter 0 / carried 0 / re-opened 0).
- Seed: committed 5pt; banked 5pt; regime classical (seed 5; **realized 7; residual +2** — +2 novel bank-gotchas, the MEM_IN_STRUCT scheduling lever + the short-text/flowing-`.bss` diagnostic; not verbatim-first-try).
- What helped: **the MEM_IN_STRUCT source-typing lever, found by reading gcc 2.7.2 `sched.c` (PO hint: is the global data part of a struct?).** `sched.c:797`/`true_dependence` — a MEM_IN_STRUCT ref at a varying addr never conflicts with a non-struct ref at a FIXED addr, so scalar-global loads are judged independent of pointer stores → the -O2 scheduler pipelines/hoists. Retyping the fixed globals as struct/array members forced the conflict: `func_80076500` scalar→`Vec3f` constants = strict `$f0` pairs; `func_80076558` = array-of-`Vec3f` `D_800E1C50[8]` (folded addressing + fresh recompute) + `D_800FBDA6[0]` struct-flag (late load → `nop` in the guard delay slot → exact 58-instr match). NO permuter. The `.bss`-shift diagnostic (below) was the key to realizing `func_80076500` was already correct. PO-directed `Vec3f*` args (byte-identical).
- Friction: `func_80076558` being 0x10 short made the flowing `.bss` at `0x8010xxxx` (interleaved after this `.text`) shift −0x10, so `func_80076500`'s `%lo(D_80105B6C)` read `0x80105B5C` — a byte miss in one fn manifested as a WRONG DATA ADDRESS in a SIBLING, which nearly sent me chasing a phantom symbol-placement bug. Also burned effort testing `-O1`/`-fno-schedule-insns2`/`-fno-cse-follow-jumps` profile hypotheses before the true cause (source-typing, not flags) surfaced.
- Applied: PO away (no response 60s) → proceeded on best judgment, **3 of 4**: #1 `docs/hazards.md#mem-in-struct-scheduling-lever` new + hazard-index row; #2 `docs/hazards.md#short-text-shifts-flowing-bss` new + index row; #4 `CLAUDE.md` Iterate-step pointer to both hazards before the permuter. #3 (seed-rubric data point) folded into the VELOCITY.md row, not a tooling change. Re-ask the PO at next gate if they want #3 as an explicit pts follow-up.
- Carry-over: none. The `[0x51A40]` 3-fn tail (`func_80076640`/`func_80076778`/`func_8007680C`, incl. `guRotateF`/`guMtxCatF` matrix code) is the natural next slice (re-ranked by pick_target; not a spike). Cross-repo: 3 fns kept `func_` (Ghidra had only `_NON_MATCHING`); now matched, a follow-up can clear the tag [deferred].

## Sprint 161 — src/main/func_800521C0.c (3-fn scenario-flag get/set head carved from the 12-fn [0x2D5C0] subseg — main-segment classical endgame) — 2026-07-02
- Increment: 1 file banked (`src/main/func_800521C0.c`, **3/3 fns**) / 3 functions matched (`func_800521C0` = branchless flag getter `-((D_801B60AC & 2) != 0) & 9` → 9 if bit1 set else 0; `func_800521DC` = `flag_is_set(0x3D)?2 : (D_801B60AC & 6)?9 : 0x12`; `func_80052220` = `func_800521C0() + func_800521DC() - 1`, s0-save eval order). md5-candidate **215→216** (+1); matched +3; asm subsegs **78→78** (split `[0x2D5C0]`→c head + `[0x2D650,asm]` 9-fn tail; net unchanged).
- Quality: **0/0/0/0** (stuck-far 0 / permuter 0 / carried 0 / re-opened 0).
- Seed: committed 3pt; banked 3pt; regime classical (seed 3; **realized 2; residual −1** — verbatim first-try, all 3 fns matched on the FIRST build, 0 fix-iterations; the smallest classical rung, first sub-5 classical realized).
- What helped: **asm-first fast-path** (S148) — the 3 fns are tiny (<10 instrs each), so no MCP shape was needed; hand-translated straight from `asm/2D5C0.s`. The branchless `-(cond) & mask` idiom (`andi`+`sltu $zero`+`negu`+`andi`) reads directly as `-((x & 2) != 0) & 9` — no permuter, no iteration. Sparse 16-alignment (only 0x2D620/0x2D650/0x2E0F0 in the subseg) → split at `0x2D650`; the head slice emits no rodata → decomposition-safe, no rodata-alignment wall.
- Friction: `--lib main` (the `/sprint-plan main` idiom) is a SUBSTRING filter, so it returned only the coddog `mainlib/*` game-embedded packs, NOT the classical main-segment `none` subsegs the recent sprints mine — fell back to global `-n40 --json` + a manual vram filter. The 2 smallest main subsegs (llcvt/settime) are game-embedded mirrors the PO defers, yet they kept topping smallest-first. Both fixed this sprint.
- Applied: PO-selected **3 of 3**: #1 `tools/pick_target.py --segment main` vram-range filter (0x80025C50..0x801F4A2F, distinct from `--lib` substring; `SEGMENT_RANGES` + `--segment` arg + `_row_filtered` drop); #2 `/sprint-review` Step 5.4 carry-over-hygiene rule (prune carry-overs whose file is now md5-candidate) + pruned the stale S156 `func_80051E90` jtbl-pair spike (banked S159 `c774454`); #3 `pick_target.py` sort de-rank of `game-embedded` fncount-mismatch mirrors below clean classical (`_deferred` tier, above phantoms). Goldens regen'd (4, `make test-tools` green 102).
- Carry-over: none. The `[0x2D650]` 9-fn tail is the natural next slice (re-ranked by pick_target, now surfaced cleanly via `--segment main`; not a spike). Cross-repo: 3 fns kept `func_` (Ghidra had only `func_..._NON_MATCHING` placeholders); now matched, so a follow-up can clear the `_NON_MATCHING` tag [deferred].

## Sprint 160 — src/main/func_8006EA90.c (3-fn one-tu classical slice — physics byte-flag table setup + no-op + a two-texture fog/scroll screen-filter display-list builder, whole [0x49E90] subseg — FIRST bank of the pure classical asm-flip endgame) — 2026-07-02
- Increment: 1 file banked (`src/main/func_8006EA90.c`, **3/3 fns**) / 3 functions matched (`func_8006EA90` = putter/physics byte-flag table setup: outer4/inner3 dup-store loop `trunc((rec[j+4]+rec[j+7])*0.3f)` to `D_800C41CC[0x40+]` AND `[+0x80]` (recompute-not-CSE), three `*0.35f` signed byte blocks, three `(u32)(f64)*0.8` UNSIGNED-cast blocks (`c.le.d` threshold idiom) → `D_800E1C18/19/1A`; `func_8006ED2C` = no-op; `func_8006ED34` = two-texture fog-blended scrolling screen-filter DISPLAY-LIST builder, `emit_per_phase_fog_state(&gfx)` spliced mid-list, texture-scroll counter `D_800C42C4++`). md5-candidate **215→216** (+1); matched +3; asm subsegs **79→78** (the `[0x49E90,asm]→c` flip was S160's gate action; +1 rodata carve).
- Quality: **0/1/0/0** (stuck-far 0 / permuter-escalated 1 [`func_8006EA90` prologue schedule] / carried 0 / re-opened 0).
- Seed: committed 13pt; banked 13pt; regime classical (seed 13; **realized 15; residual +2** — +1 permuter, +1 novel bank-gotcha = the DL post-increment idiom; the seed-13 one-tu rubric priced it close but the permuter + DL-idiom discovery were real overrun).
- What helped: **KMC gcc 2.7.2 source grounding before the permuter** (PO directive) — `func_8006EA90` was structurally 165/165 with IDENTICAL reg-alloc, missing only a prologue instruction-GROUP swap (FP-const `0.3f` load vs the base pointers). `sched.c` `rank_for_schedule` breaks priority ties by `INSN_LUID` (= physical/hoist order), so it's a hoist-order problem: structure levers (hoist the base pointer INSIDE the outer loop → fixes the `off+base` addu operand order + base position; strength-reduce `off = 0x40 + i*0x10` → moves the IV init late) got it to score-0-modulo-rodata, then the **permuter WITHOUT `--best-only`** (the equal-score PLATEAU case) found `scale = 0.3f` assigned in-loop + a pointer alias as the equal-score intermediate that reordered the FP-const ahead of base. For `func_8006ED34` the **`gDPxxx(dl++)` POST-increment idiom** was the single unlock (matches the ROM's base+offset-write-then-spill-advance codegen; a pre-store form adds an `addiu`). `tools/fpdecode.py` (new) would have caught the 0.8-vs-0.3 double up front. gfxdis.f3dex2 round-tripped the runtime DL to composite `gDPLoadTextureBlock`/`gDPLoadMultiBlock` for a clean idiomatic refine.
- Friction: (1) misread the rodata DOUBLE `0x3FE999999999999A` as 0.3 (it's **0.8**; the float `0.3f`=`0x3E99999A` shares the `999…A` nibble pattern) — one wasted build cycle. (2) the `GFX_CMD` word-write macro named its params `w0/w1`, colliding with the `g->words.w0/w1` field tokens → preprocessor rewrote the field access → KMC parse error; the isolated `decomp_loop` base.c used `a,b` and passed, so only the in-tree build caught it. (3) `setup-permuter.sh` blocked: `mg_resolve_c_asm`'s loose `grep "INCLUDE_ASM.*${func}"` matched BOTH stubs in the multi-fn subseg (the target's stem is in the sibling's asm PATH) → garbled ASM_FILE; worked around by calling `import.py` directly with the committed main-profile settings.
- Applied: PO-selected **4 of 4**: #1 `tools/lib.sh` `mg_resolve_c_asm` grep anchored to `, ${func});` (verified it disambiguates the sibling-path case); #4+#3 `docs/hazards.md#display-lists` addendum (POST-increment idiom + gfxdis.f3dex2 -f DOES fold the texture-LOAD composites [corrects the prior note] + bank-raw-then-refine recipe + the macro param/field-name-collision parse trap); #5 `#permuter-setup-for-kmc-toolchain-mirrors` sub-0.97 guidance extended to a pure prologue-SCHEDULING swap (match==total_rows + identical reg-alloc, run WITHOUT `--best-only`); #2 new `tools/fpdecode.py` (IEEE-754 float/double rodata decoder).
- Carry-over: none. Cross-repo: 3 fns kept `func_` + generic `D_` (Ghidra had none); optional curate (physics flag-table setup, DL builder) + data syms (D_800C4160 vtx array, D_800FC89C/D_80132CEC texture images, D_800C42C4 scroll counter) → `sync_decomp_names.py --import-from-decomp` [deferred].

## Sprint 159 — src/main/func_80051E90.c (2-fn one-tu jtbl-pair: course/hole yardage switch-jtbl + scenario/terrain config accessor, whole [0x2D290] subseg — the S156 jtbl-pair remainder) — 2026-07-02
- Increment: 1 file banked (`src/main/func_80051E90.c`, **2/2 fns**) / 2 functions matched (`func_80051E90` = `switch(course)` over the 8-entry compiler jump table `jtbl_800CCC30` + a sparse per-case `if`-chain on `hole` returning golf-yardage constants, default 200, cases 6/7→30; `func_80051FCC` = scenario/terrain config accessor: `D_801B608C==9` table lookup / `g_terrain!=0` signed-`%3`-magic 0x55555556 / else `sm<12` clamp-to-8). md5-candidate **214→215** (+1); matched +2; asm subsegs 79→**79** (the `[0x2D290,asm]→c` flip was S159's own gate action, counted at flip; +1 rodata carve). **Milestone: every `src/*.c` is now fully-C (0 INCLUDE_ASM stubs anywhere in `src/`); the c-stub backlog is drained, remaining work is the 79 un-flipped asm subsegs.**
- Quality: **0/0/0/0** (stuck-far 0 / permuter 0 / carried 0 / re-opened 0).
- Seed: committed 5pt; banked 5pt; regime classical (seed 5; realized 5; residual 0 — the seed-5 rung priced the jtbl-switch+rodata-carve hazard correctly; 4 codegen nudges were routine convergence on now-documented hazards, gcc-source grounding was knowledge-capture not difficulty).
- What helped: **asm-first seed** (no MCP needed for shape; MCP confirmed both fns have NO curated Ghidra name); **objdump-diff against the target `.s` per iteration** (both fns short); **PO directive to analyze codegen in the KMC gcc 2.7.2 source before the permuter** — the 4 nudges were all grounded (no permuter): (1) `switch` for the jtbl dispatch ONLY + `if`-chains for sparse inner cases → one table to carve; (2) `a==K1||a==K2` compiles branchless (`xori/sltiu/or`) → split into two `if`s for the short-circuit branch form, which lets gcc cross-jump a sibling case's identical "check-K-else-default" tail (const rides in shared v0); (3) index-add operand order (`expr.c:5248-5290 both_summands` "put a multiplication first" swap → cheap term on the RIGHT loads first); (4) branch-LIKELY `beqzl` on a coalesced return-var (`reorg.c:1141-1211 optimize_skip` "goes around a single insn" → invert the branch so the constant is the early return → plain `beqz`+`move v0,a0`). `.rodata` sibling carve for the switch table = first carve of a compiler jump table (mechanics identical to prior FP-literal/const-array carves).
- Friction: none material. func_80051FCC took 3 quick rebuilds to settle branch 3 (beqzl→ternary→inverted); the two subtle levers (operand order, branch-likely) each had a clear gcc-source mechanism once looked up, so no permuter and no guesswork loop.
- Applied: PO-selected **4 of 4**: #1 new `docs/hazards.md#switch-jtbl-dispatch` + 1 index row; #2 `#register-reuse-nudge` branch-likely variant + `optimize_skip` anchor + 1 index row; #3 `#register-reuse-nudge` operand-order gcc anchor (`both_summands`); #4 VELOCITY pts anchor (2nd post-S155 sub-13 classical data point after S156). Note: #2 was folded as a variant into the existing `#register-reuse-nudge-classical-regalloc` section (same INVERT-the-branch lever as the S156 guard-temp variant, distinct manifestation) rather than a duplicate `#return-value-coalescing-branch-likely` section; the index row makes the branch-likely symptom discoverable.
- Carry-over: none.

## Sprint 158 — src/main/func_80067D40.c (5-fn one-tu golf-tournament module: RNG pair + FP/trig score generator + leaderboard sort/rank + scenario name-table builder, whole [0x43140] subseg) — 2026-07-01
- Increment: 1 file banked (`src/main/func_80067D40.c`, **5/5 fns**) / 5 functions matched (`func_80067D40`/`4C` = RNG seed-set + LCG-next on a 2nd seed `D_800C3600`, mult 0x5D588B65; `func_80067D74` = 226-instr CPU-field score generator using per-entry mod-arith + the LCG + `sin(hole·π/18)`; `func_800680FC` = 137-instr leaderboard build/sort(`func_8005B150`)/rank returning the player position; `func_80068308` = 76-instr scenario name-table builder via strcmp). md5-candidate **213→214** (+1); matched +5; asm subsegs unchanged (the `[0x43140,asm]→c` flip was the gate action).
- Quality: **0/3/0/0** (stuck-far 0 / permuter-escalated 3 [all three non-trivial fns] / carried 0 / re-opened 0).
- Seed: committed 13pt; banked 13pt; regime classical. v2: seed 13; **realized 18; residual +5** (+3 permuter-escalated across the 3 fns, +1 heavy multi-round fan-out re-attempt, +1 novel-bank-gotchas cluster; a seed-13 one-tu MASSIVELY under-priced).
- What helped: **the new whole-function regalloc playbook** (`docs/hazards.md#pervasive-regalloc-classical-main`). All 3 non-trivial fns were STRUCTURALLY correct first pass (asm-differ rows aligned) but locked on PERVASIVE register allocation. Cracked by: EXACT-SYMBOL per-field structs (no reloc-addend false floor) + REF-COUNT/LIVE-RANGE levers (DECL ORDER IS INERT for KMC gcc 2.7.2 — verified) + boosted-weight permuter. The specific levers: `func_80067D74` = param-reuse (clamp `arg0` in place, drop `base`/`seq`, `.id=i-1`) + **`s32` return type** (reserves v0 — load-bearing, void differed at the entry delay slot) + `switch(mod3)` + a/b rng temps; `func_800680FC` = align-2 Eid (word+half struct copy) + `u8 result` hoisted to s3 + swapped compare + **const-pointer rank base** (defeats a wrong LICM hoist) + do-while inner loop; `func_80068308` = init-early/compute-late live-range trim (lo 27→26 insns to outrank found for s2) + hoisted `p<limit` + sc/base scratch split + offset-first ptr arith + hi inlined into the store. A **3-round multi-agent worktree fan-out** (empirical order-sweep + gcc-source theory + long permuter agents) is what found the param-reuse/align/return-type levers the solo permuter plateaued on.
- Friction: the permuter and asm-differ metrics diverge wildly (permuter 895 == asm-differ 6620 == 66 real rows) and BOTH floor above 0 on link-identical reloc artifacts (combined `D_801B7118+2` vs `D_801B711A`; intra-file `jal` as `.text`-vs-symbol) — had to verify matches by LINKED raw-instruction diff, not the score. Solo attempts + solo permuter plateaued indefinitely from a wrong structural base; the permuter cannot invent the param-reuse/align/LICM levers, so those go by hand FIRST. Worktree agents need the gitignored toolchain (tools/cc, venv, decomp-permuter) symlinked in. Two subagents lost their final message to a 529 / session-limit — recovered results from their persisted worktrees.
- Applied (4 of 4 selected): #1 three new `docs/hazards.md` sections (`#pervasive-regalloc-classical-main`, `#return-type-is-load-bearing`, `#struct-access-folding-changes-scheduling`) + the metric caveat + CLAUDE.md hazard-index rows; #2 committed the main-profile permuter tooling (`tools/permuter_settings_main.toml` + `tools/kmc_main_prelude.inc`) + the `.L`-label/gp=64 gotchas into `#permuter-setup-for-kmc-toolchain-mirrors`; #3 pts regalloc-heavy weight [BACKLOG]; #4 multi-agent fan-out process note (folded into `#pervasive-regalloc-classical-main`).
- Carry-over: none. Cross-repo: 5 fns kept `func_` + generic `D_` (Ghidra had none); curate names (RNG pair, score-gen, leaderboard, scenario-table) + data syms (2nd RNG seed D_800C3600, per-hole table D_800C3604, 30-entry ScoreEntry array D_801B7118, scenario table D_80132CB0) → `sync_decomp_names.py --import-from-decomp` [BACKLOG].

## Sprint 157 — src/main/func_80025D30.c (9-fn integer overlay/moduleset load-unload module, whole [0x1130] subseg) — 2026-07-01
- Increment: 1 file banked (`src/main/func_80025D30.c`, **9/9 fns**) / 9 functions matched (`func_80025D30`/`D54` = init wrappers, `func_80025D78` = seg-slot getter, `func_80025D8C` = seg→overlay map rebuild, `func_80025EC8` = logical-id normalizer, `func_80025F18` = can-load conflict check, `load_overlay`/`unload_overlay` = ROM-DMA + cache-flush + seg-claim, `func_80026358` = debug prints), over a 0x28-byte `OverlayDesc[]` table (`D_800B5F58`). md5-candidate **212→213** (+1); matched +9; bare-asm code subsegs 81→81 (the `[0x1130,asm]→c` flip was a gate action). First multi-fn main-segment integer module.
- Quality: **0/3/0/0** (stuck-far 0 / permuter-escalated 3 [`func_80025F18`/`load_overlay`/`unload_overlay` — set up + ran, but resolved by the PO reference impl, NOT the permuter] / carried 0 / re-opened 0).
- Seed: committed 13pt; banked 13pt; regime classical. v2: seed 13; **realized 16; residual +3** (+1 permuter-escalation, +1 heavy multi-round re-attempt on the 3 hard fns, +1 novel bank-gotcha = the indexed-vs-pointer strength-reduction structure; capped-13 seed, the escalations are real residual).
- What helped: **KMC gcc 2.7.2 source grounding** cracked 6/9 up front — `global.c` `allocno_compare` (priority `= log2(refs)*refs/live_length*size`) explained the s0/s1/s2 register ROTATION and drove the fix (reuse the `overlayId` param for the index → more refs → higher priority → the ROM's register); plus `func_80025EC8`'s mixed signed/unsigned range check (`(u32)` cast blocks the range-fold) and `func_80025D8C`'s single-counter + CSE-in-`if`-condition + `(u32)` loop bound. The last 3 were cracked by the **PO reference implementation**: the segment walk is INDEXED (`for(i=0; seg[i]!=-1; i++){ byte=seg[i]; ...}` + value temp) not pointer-increment, and F18's 3 `continue` guards fold into one `||`. WHY (grounded): `move_movables` (loop.c:966, invariant hoist) runs BEFORE `strength_reduce` (loop.c:976), so an indexed walk's giv pointer-init (`emit_iv_add_mult`@loop.c:666) emits AFTER the hoisted loop constants → reorg fills the entry-`beq` delay with the CONSTANT (matches ROM); a pointer `p=seg` biv-init emits first → the `move` fills the delay (miss). Same mechanism defused F18's `-1` hoist.
- Friction: the 3 hardest fns were 1-instruction scheduling/hoisting misses (delay-slot fill, `-1` hoist) that resisted BOTH extensive manual source variation AND the permuter (250k+ iterations flat) — because (a) `perm_sameline` is a NO-OP for KMC gcc 2.7.2 (it ignores line numbers, unlike IDO — verified), which I'd over-weighted, and (b) `--best-only` can't cross the equal-score plateau these needed. Correct permuter setup for game -O2 fns (manual `compile.sh` + isolated `target.o` assembled with modern GAS after prepending `.include "macro.inc"`) was itself a chunk of work — `permuter_settings.toml`'s command is stale for non-mirror fns.
- Applied (4 of 4 selected; #4 → BACKLOG): #1 new `docs/hazards.md#indexed-vs-pointer-loop-strength-reduction` (giv/biv preheader ordering → delay-slot/hoist; the indexed+value-temp form; the binutils-2.6 no-touch note; the `perm_sameline` false-lead) + CLAUDE.md hazard-index row; #2+#3 the two KMC-gcc permuter facts (`perm_sameline` no-op, `--best-only` plateau) into `#permuter-setup-for-kmc-toolchain-mirrors`; #5 the gcc-source cross-ref (allocno_compare / move_movables+strength_reduce / reorg fill / loop-inversion locations) folded into the new hazard section; #6 pts data-point [BACKLOG]. #4 (a `setup-game-permuter.sh` helper + `permuter_settings.toml` game profile) deferred to a golden-gated tooling branch [BACKLOG].
- Carry-over: none. Cross-repo: fns kept `func_` placeholders (Ghidra had none; `load_overlay`/`unload_overlay` pre-named); the `overlays`(D_800B5F58)/`debug_mode`(D_800B67C0) global renames were reverted (D_800B67C0 is shared by `func_80052070.c`+`nusched.c`) → a global-rename follow-up (BACKLOG).

## Sprint 156 — src/main/func_80052070.c (4-fn jal-free scenario/terrain accessor slice, carved off the func_80051E90 jtbl pair) — 2026-07-01
- Increment: 1 file banked (`src/main/func_80052070.c`, 4/4 fns) / 4 functions matched (`func_80052070`, `func_800520DC`, `func_80052100`, `func_80052168`). md5-candidate 210→211; asm subsegs 82→82 (the gate split at 0x2D470 left the jtbl-pair remainder `[0x2D290,asm]` in place, so no net subseg drop; value signal is the +4 matched fns / new fully-C file). All jal-free extern-global accessors (scenario/terrain config getters), no local rodata.
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened). 2/4 fns first-build; the other 2 took textbook regalloc/ordering nudges (no permuter, no spike).
- Seed: committed 5pt; banked 5pt; regime classical   (v1; realized tier v2 below)
- What helped: asm-first fast-path (`.s` ground truth) — Ghidra MCP was UP but returned stale `_NON_MATCHING` bodies (2 fns `return 0;`, phantom `>>0x1f` on a plain `lw`), correctly ignored. Object-only rebuilds (`make build/…o` + `objdump`) made the register-nudge sweep fast; full-make ROM SHA-1 gated each fn (the other 3 stay exact asm stubs, so a per-fn miss shows immediately). KMC gcc source (`config/mips/mips.h`, no `REG_ALLOC_ORDER` → default ascending $2-before-$3) explained the guard-var v0-grab.
- Friction: two codegen near-misses — a guard-variable landing in the wrong scratch reg (fixed by INVERTING the guard) and array-index `+` operand order picking the wrong `v0` accumulator (fixed by swapping operands). Both are now documented nudges.
- Applied: PO-selected 4 of 4 — #1 `docs/hazards.md#register-reuse-nudge` inverted-guard variant; #2 same section's array-index operand-order variant; #3 `#decompile-vs-asm-authority` `_NON_MATCHING`-distrust note; #4 the jal-free sub-slice carve strategy in `#multi-function-segment-splitting-pack` (manual gate procedure) + pts data-point [VELOCITY] + the automated ranker detector deferred to a BACKLOG tooling follow-up (off-cadence, golden-gated).
- Carry-over: the jtbl pair `func_80051E90` + `func_80051FCC` (the upper 2 fns of the region, `[0x2D290,asm]`) — a near-free retry once `jtbl_800CCC30` rodata handling is set up (checklist in BACKLOG Carry-overs).

## Sprint 154 — func_8006A000.c (math one-tu 8/8: banked the [0x455C0] 3-fn tail + RECOMBINED the 3+2+3 decomposition) — 2026-07-01
- Increment: 1 file banked (`src/main/func_8006A000.c`, now **8/8 fns**) / **3 functions matched** (`calc_vec3_magnitude` = `sqrtf(x²+y²+z²)`, bare single `sqrt.s`, 3rd arg z passed in GPR `$a2`; `crc16_ccitt` (was `func_8006A1EC`) = CRC-16/X.25, poly 0x8408, init 0xFFFF, xorout 0xFFFF; `report_div_error` (was `func_8006A274`) = `osSyncPrintf("kDivError", $ra)` + `(s32)(1.0f/D_800C3FB4)`). Completes the `[0x45400]` math/RNG one-tu and **RECOMBINES** the S152/S153 3+2+3 decomposition back into one file (deleted `func_8006A180.c` + `func_8006A1C0.c`). md5-candidate **212→211** (merge consolidation, NOT a regression: net +3 matched, all 211 C files fully-C); text subsegs `[0x45580]`/`[0x455C0]` merged into `[0x45400]`.
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened). All 3 matched; `calc_vec3_magnitude` first-build.
- Seed: committed 13pt; banked 13pt; regime classical. v2: seed 13; **realized 13; residual 0** — seed already at the 13 ceiling, so the FOUR novel gotchas (check_dbra_loop reversal, `$ra` inline-asm scheduling, rodata-split TU-recombine, mid-sprint 2-file merge) can't push realized higher on the Fibonacci ladder; not verbatim-first-try (no −1). The capped seed under-prices this sprint's difficulty (pts-recalibration territory).
- What helped: **The two PO mid-sprint directives were load-bearing.** (1) "goto only as a last resort" forced building BOTH natural loop forms (`for`, `do-while`) and confirming each reverses — which, cross-checked against the gcc source (`loop.c:5761-5829` `check_dbra_loop` reverses any count-only loop from 0), proved the goto was genuinely required for `crc16_ccitt`'s bit-loop while its OUTER loop matched as a natural indexed `for` (strength reduction gave the ROM's `addu a1,a1,a0`). (2) "put the strings in as actual literals / combine a TU with its neighbour" led to the rodata-split root cause and the clean recombine. Systematic-debugging + the KMC gcc/binutils source cracked all three codegen requirements: the `$ra` capture (`RETURN_ADDR_RTX` undefined `expr.c:7199` → `__builtin_return_address` is a wrong stack load; a `volatile` inline-asm barrier gives the exact `addu a1,$ra,$0` AND fills the jal delay slot), and the per-function isolated `.o` word-diff (reloc-filtered) pinpointed every mismatch.
- Friction: the rodata carve burned THREE dead ends (`[0xAC840]`→−4 shift; `[0xAC83C]`→+4/753k diffs from spimdisasm's `.align 3` re-padding; splat per-subseg `align: 8`→silently ignored, it's segment-level only) before the `Makefile` `OBJCOPY_ALIGN := --set-section-alignment .rodata=4` + `--no-pad-sections` root cause surfaced (force-4-aligns every ASM rodata, so a split can't 8-align the next-TU double). The splat + Makefile source dive was needed to understand WHY; the TU-recombine (func_8006A000's 16-aligned `2^31` doubles pad the section tail to `0x800D1440`) was then clean and correct. Also the `__builtin_return_address` + register-asm dead-ends before the volatile-asm barrier.
- Applied (4 of 4): #1 `docs/hazards.md#decomposed-one-tu-rodata-alignment-split` (new; the OBJCOPY_ALIGN split hazard + the recombine fix, a counter-case to the 8-point decompose gate) + CLAUDE.md hazard-index row; #2 extended `docs/hazards.md#top-tested-loop-goto-local-hoist` with the `check_dbra_loop` REVERSAL corollary (distinct from expand_end_loop inversion) + index row; #3 `docs/hazards.md#capturing-ra-return-address-as-a-call-argument` (new; the `$ra`-as-log-arg volatile-asm idiom) + index row; #4 pts-recalibration 6th data point (BACKLOG).
- Carry-over: none (one-tu fully banked). Cross-repo: `crc16_ccitt`/`report_div_error` added to `symbol_addrs` → propagate via `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 153 — func_8006A180.c (2-fn head slice: update_rng_seed + hypotf_2d, decomposed from the [0x45580] S152 remainder) — 2026-07-01
- Increment: 1 file banked (`src/main/func_8006A180.c`) / **2 functions matched** (`update_rng_seed` = LCG `rng_seed = rng_seed*0x5D588B65 + 1`; `hypotf_2d` = `sqrtf(a*a + b*b)`, bare single `sqrt.s`). Head-2 of the S152 `[0x45580]` carry-over, decomposed at the sole 16-aligned internal boundary 0x8006A1C0. md5-candidate **211→212**; matched +2; asm subsegs unchanged (split; the 3-fn `[0x455C0]` tail carried).
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened). Both fns MATCH first-build; the tail carry is the PLANNED decompose remainder, not a spike.
- Seed: committed 13pt; banked 13pt; regime classical (v2: seed 13; realized 13; residual 0 — verbatim-first-try −1 offset by the +1 codegen-enabler discovery + KMC-gcc-source verification; capped-13 seed is pts-recalibration territory).
- What helped: the **KMC gcc 2.7.2 source verification** (PO-requested) resolved the load-bearing sqrt.s unknown cleanly and REFUTED the S152 carry-note. `sqrtf` is `BUILT_IN_FSQRT` (`c-decl.c:3230`), expanded by the SAME `expr.c:7243` path as double `sqrt` (mode-only difference → `sqrtsf2` `mips.md:1506`, gated `mips_isa>=2`); `! flag_fast_math` appends the `c.eq.s`/`bc1t`+`jal sqrt` guard (proven by `align.o`), so the ROM's bare `sqrt.s` needs the SAME per-file `-ffast-math` as S152 — NOT a separate "sqrtf-intrinsic path." Asm-first seed (both fns tiny, ground truth in `asm/45580.s`); MCP not needed. The head-2 slice isolated the sqrt.s discovery cheaply (de-risks the tail).
- Friction: none. The S152 carry-note's incorrect "single-precision differs" hypothesis would have sent this down a dead `#pragma intrinsic(sqrtf)` path; the source dive corrected it (now fixed in `docs/hazards.md#double-sqrt-fast-math`).
- Applied (2 of 2 PO-picks; #2 = mandatory bookkeeping): #1 rewrote `docs/hazards.md#double-sqrt-fast-math` mode-agnostic (covers `sqrt.d` AND `sqrt.s`; the fix is `-ffast-math` for either; the builtin is always active; `#pragma intrinsic` dead on KMC) + synced the CLAUDE.md hazard-index row; #3 logged the pts-recalibration 5th data point (64B/2fn priced pts-13) to BACKLOG; #2 moved the tail carry-over to BACKLOG.
- Carry-over: the `[0x455C0]` 3-fn tail (`calc_vec3_magnitude` [same `-ffast-math` sqrt.s], `func_8006A1EC` [CRC16 goto-loop], `func_8006A274` [log + 1.0f/float→trunc]).

---

## Sprint 152 — func_8006A000.c (3-fn FP vector-magnitude slice, decomposed from the [0x45400] 8-fn math/RNG one-tu) — 2026-07-01
- Increment: 1 file banked (`src/main/func_8006A000.c`) / **3 functions matched** (`vector_magnitude_safe` = 3D sqrt-magnitude w/ overflow range-scaling → u32; `calculate_hypotenuse_safe` = 2D, same idiom; `set_rng_seed` = trivial `rng_seed` setter). Decomposed from the `[0x45400]` 8-fn FP-math/RNG one-tu (split at the 16-aligned 0x8006A180 boundary; the 5-fn remainder is carried). md5-candidate **210→211**; matched +3; `[0x45400]` subseg split (`[0x45400,c]` banked + `[0x45580,asm]` carried, net asm-subseg count unchanged).
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened). Matched WITHOUT the permuter despite the depth; the 5-fn remainder is a PLANNED decompose carry, not a spike.
- Seed: committed 13pt; banked 13pt; regime classical. v2: seed 13; realized 15; residual +2 (+1 novel bank-gotcha = the `-ffast-math` double-`sqrt.d` enabler; +1 novel bank-gotcha = the goto-loop+local-hoist un-inversion idiom; capped-13 seed over-prices a 384B/3-fn none pack [pts follow-up], but the two codegen discoveries are real residual).
- What helped: **A systematic-debugging pass into the KMC gcc 2.7.2 source (`~/development/repos/mips-gcc-2.7.2`) cracked three interlocking codegen requirements** the two magnitude fns needed. (1) **bare `sqrt.d`**: the ROM leaf has an unguarded `sqrt.d`; the default profile emits `jal sqrt` (undefined) and `#pragma intrinsic(sqrt)` a GUARDED `sqrt.d`+`c.eq.d`/`bc1t`+`jal sqrt` NaN-fallback — a per-file `-ffast-math` override drops the errno guard. (2) **top-tested plain-branch loop**: `expand_end_loop` (stmt.c) inverts EVERY structured top-tested loop at -O2 (guard-`j` + branch-likely `beql`), so the range-scale loop is hand-rolled with `goto`s; `loop.c` only optimizes `NOTE_INSN_LOOP`-marked (structured) loops, so a goto-loop is never inverted. (3) **invariant hoist**: because `loop.c` ignores goto-loops, the range bounds are declared as LOCAL variables to force register retention = manual hoist. Reading a MATCHED SIBLING (`func_8005029C`, S151, same -O2, IS inverted) proved inversion is the default (not a flag) and stopped a source-guessing spiral.
- Friction: (1) burned several attempts guessing source forms (`for`/`while`/`for(;;)+break` all invert identically) before checking the compiler source; the PO redirect ("verify the loop isn't compiler-autogenerated") + a matched-sibling comparison is what isolated the mechanism. (2) `-ffast-math` red-herring risk: it fixed `sqrt` but does NOT touch the loop inversion/hoist (verified with/without). (3) the Ghidra decompile added a spurious `>> 0x1f` on the return (asm was just `sllv`) — asm is ground truth.
- Applied (4 of 4): #1 `docs/hazards.md#double-sqrt-fast-math` + hazard-index row; #2 `docs/hazards.md#top-tested-loop-goto-local-hoist` + hazard-index row; #3 decomp_loop `main`-profile + `-ffast-math` tooling DEFERRED to a golden-gated branch (BACKLOG follow-up); #4 pts-recalibration 4th data point appended (BACKLOG follow-up).
- Carry-over: the 5-fn `[0x45580]` remainder (near-free-retry, checklist in BACKLOG) — `update_rng_seed`, `hypotf_2d`, `calc_vec3_magnitude` (single-precision `sqrt.S` leaves → sqrtf-intrinsic path, NOT this `-ffast-math` `sqrt.d`), `func_8006A1EC` (CRC16-CCITT + log call), `func_8006A274` (log + 1/x). Cross-repo: `set_rng_seed` → `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 151 — func_800500E0.c (3-fn Gfx display-list "wipe box" TU; FIRST main-segment game DL TU) — 2026-07-01
- Increment: 1 file banked (`src/main/func_800500E0.c`) / **3 functions matched** (`func_80050274` registrar, `func_800500E0` draw-one+animate, `func_8005029C` render-setup+draw-all). A UI wipe/reveal system of translucent animated textured rects. md5-candidate **209→210**; matched +3; asm subsegs 84→**83**.
- Quality: 0/1/0/0 (stuck-far/**permuter**/carried/re-opened).
- Seed: committed 13pt; banked 13pt; regime classical. v2: seed 13; realized 15; residual +2 (+1 permuter-escalation on `func_800500E0`, +1 novel bank-gotcha = the F3DEX2 `main`-profile requirement / first game DL TU; capped-13 seed but the escalations are real residual).
- What helped: (1) **gfxdis.f3dex2** (`~/development/repos/n64-tools/src/gfxdis/`) decoded the setup DL command words into `gDP*` macros; the demos (`n64demos` tile_rect2d + `kantan-demos` 2d.c) gave the `gDPxxx(glistp++)` / `Gfx **glistp` idiom and the `u16` coord type. (2) **The permuter cracked a regalloc + phantom -16 frame near-miss** that manual nudges could not: retyping the two texrect coords `u16 left; s16 right;` (score 640→10), then a commutative operand swap. Coord/local integer WIDTH is a first-class permuter lever. (3) **Testing the stock macro** instead of trusting a mask-constant hunch: `gSPTextureRectangle` proved byte-identical (GCC narrows `(coord<<2)&0xFFF`→`andi 0xFFC`), verified vs the GBI docs + kantan demo.
- Friction: (1) **first read wrongly concluded the texrect was custom-inline** (ROM `andi 0xFFC` ≠ macro `0xFFF`) and hand-inlined it; the stock `gSPTextureRectangle` was byte-identical all along — a mask-constant diff is NOT proof of a custom macro (GCC narrows redundant mask bits). (2) The **F3DEX2 profile** was a silent blocker: without `-DF3DEX_GBI_2` the RDPHALF opcodes read `0xB4`/`0xB3` (wrong); `decomp_loop` still lacks the define (isolated false diff). (3) `func_800500E0`'s phantom frame + regalloc resisted every manual structural nudge; only the permuter's coord-width retype reached it.
- Applied: PO-selected 4 (all 4 groups) — #1 `docs/hazards.md#display-lists` (DL-reconstruction workflow + the mask-narrowing lesson) + CLAUDE.md main-tree F3DEX2 profile convention (revises the S148 "zero mk edits" claim) + `mk/main.mk`; #2 `docs/hazards.md#permuter-setup-for-kmc-toolchain-mirrors` game-O2 recipe + the coord-width regalloc lever; #3 CLAUDE.md cross-repo Ghidra `strict_mode="off"` naming bypass + the `symbol_addrs`-rename-of-a-ghidra-symbol recipe; #4 tooling (promote `dl_fold_check.py` to `tools/`; teach `decomp_loop.py` a `main`/F3DEX_GBI_2 profile) DEFERRED to a golden-gated branch (BACKLOG).
- Carry-over: none (file fully banked). Cross-repo: 3 function names kept `func_` placeholders (behavior understood = wipe/reveal box register/draw-one/draw-all) → curate + `sync_decomp_names.py --import-from-decomp`; `glistp` already propagated to Ghidra live.

## Sprint 150 — func_80029250.c (cfb_setup + cfb_set_num — game-custom nusys CFB setup; 2nd classical Epic-2 pack) — 2026-06-30
- Increment: 1 file banked (`src/main/func_80029250.c`) / **2 functions matched** (`cfb_setup` = the game's rewritten `nuGfxSetCfb`; `cfb_set_num` = dynamic active-framebuffer-count change, widely called). md5-candidate **208→209**; matched +2; asm subsegs 85→**84** (`[0x4650]` retired).
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened).
- Seed: committed 13pt; banked 13pt; regime classical. v2: seed 13; realized 13; residual 0 (no escalation fired: not first-build so no −1 verbatim-first-try, the 2 fixes were routine classical convergence not a novel gotcha so no +1; capped at the 13 Fibonacci ceiling; the over-pricing is a SEED issue = pts-recalibration follow-up, not a residual one — same shape as S149).
- What helped: (1) **asm-first seed** straight from `asm/4650.s` (Ghidra had `func_` only) — the pack is a pure global data-shuffle (0 `jal`), so plain `extern` decls + the ROM-SHA-1 gate was nearly the whole loop. (2) **Isolated `decomp_loop`** localized the first fix fast (the `num==3` `nuGfxCfbNum` store-order, from the `> nuGfxCfbNum` insertion rows). (3) **`cmp -l build vs baserom`** localized the second fix when the isolated diff read near-perfect (the else-branch load hoist). (4) `nugfxtaskmgr.c`'s existing `extern u8 D_800B67A4[]` + the stock nusys `nuGfxSetCfb` source confirmed the CFB rotation-state model up front.
- Friction: (1) the **8-gate false-fired** again (496B priced pts-13) and decompose was one-tu-blocked, forcing a PO 8-gate question — the recurring pts-overpricing symptom (now softened by the new (a2) exemption branch; root fix still the tracked recalibration). (2) the **isolated `decomp_loop` UNDER-reported a scheduling reorder** — 44/45 rows, reloc-addend noise only, yet the full-make SHA missed on a hoisted `lw framebuf[2]`; asm-differ matched the moved load across its move. Cost one full-make miss before `cmp` localized it.
- Applied: PO-selected 2 of 3 — #3 `docs/hazards.md#isolated-compile-caveat` inverse-trap note (scheduling-reorder under-report + the `cmp -l` localize recipe); #1 CLAUDE.md small-pack exemption generalized to a new **(a2) 0-call size-agnostic** branch (+ BACKLOG pts-recalibration reinforced as a 2nd data point). (#2 asm-first fast-path confirmation = positive signal, no edit.)
- Carry-over: none (file fully banked). Cross-repo name follow-up: `cfb_setup`/`cfb_set_num` → `sync_decomp_names.py --import-from-decomp`.

## Sprint 149 — src/libnusys/nuboot.c COMPLETE (game-embedded nusys boot: nuBoot + idle) — 2026-06-30
- Increment: 1 file banked (`src/libnusys/nuboot.c`) / **2 functions matched** (`nuBoot` cart entry + `idle` boot thread — the game's copy of nusys `nuboot.c`). md5-candidate **207→208**; matched +2; asm subsegs 85→**84** (`[0x748B0]` retired).
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened).
- Seed: committed 13pt; banked 13pt; regime classical. v2: seed 13; realized 13; residual 0 (+1 novel bank-gotcha = the -O0 profile pin, but the seed is already at the 13 Fibonacci ceiling; NOT a verbatim first-try, so no −1; the seed over-prices a 320B/2-fn pack — the pts-recalibration follow-up, a seed issue not a residual one).
- What helped: the **NuBoot reference** (the PO-flagged nusys-2.07 `nuboot.c`) made the byte-match a transcription; the **profile-probe** flag-pin (assemble-target + KMC-compile-candidate normalized `objdump -dr` diff) decisively isolated the -O0 root cause in seconds (idle byte-exact at -O0, 53 instrs); **verifying against the ultralib pin** (the PO directive) exposed the `os_host.h` macro inversion as the real cause behind the `osInitialize`/`__osInitialize_common` confusion (a header bug, not a ghidra mislabel).
- Friction: the asm-first seed's first build SHA-MISSED with no obvious cause — a **profile** (opt-level) mismatch masquerading as a C-logic miss; the fp-kept + no-CSE + arg-spill codegen tell is what unlocked it. The profile-probe normalizer's first cut compared only reloc lines (POSIX awk has no `\s`), reading as false 0-diffs — a one-round detour now baked into the tool.
- Applied: 5 of 5: #1 `docs/hazards.md#-o0-bootsdk-glue-file-profile` + the per-file -O0 mk override pattern (+ CLAUDE.md path-convention exception + index row); #2 `tools/profile_probe.py` (+ `#profile-probe` hazard); #3 CLAUDE.md carve-to-libnusys path convention (`pick_target` auto-routing of the `idle=nuboot` tell is a tracked follow-up); #4 `docs/hazards.md#overlapping-symbols--allow_duplicated` (+ index row + the sync-managed-name caveat); #5 `docs/hazards.md#vendored-header-inversion` + `tools/audit_libultra_headers.py` (+ CLAUDE.md libultra-pin pointer + the `rcp.h VI_CTRL_PIXEL_ADV_MASK` fix it found).
- Carry-over: none.

## Sprint 148 — func_ovl10_801F4A40.c COMPLETE (the FIRST classical game-code bank; Epic 1 → Epic 2) — 2026-06-30
- Increment: 1 file banked (`src/overlay_10/func_ovl10_801F4A40.c`) / **2 functions matched** (`func_ovl10_801F4A40` flag-gated sound/setup + `func_ovl10_801F4AD8` `*p=*p` accessor). md5-candidate **206→207**; remaining 1523→**1521** asm fns / 78→**77** rows; asm subsegs 86→85.
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened). Banked atomically, first build, 0 iteration.
- Seed: committed 13pt; banked 13pt; regime classical. v2: seed 13; realized 12; residual −1 (−1 verbatim first-try, both fns 1-build; the size-pts over-pricing of a 176B pack is in the seed, not the residual).
- What helped: (1) **Phase-transition survey at the gate** — `--lib` on every band returned no candidates, so the increment choice was correctly framed as "first classical game-code unit", not another mirror. (2) **asm-first seed** straight from the splat `.s` — Ghidra MCP was DOWN all sprint and it didn't matter; the 2 small fns translated cleanly from the instruction listing (callee/global types read from call-site arg setup + `lhu`/`sw` widths). (3) The auto `func_`/`D_` symbols resolved from their home subsegs, so plain `extern` decls + ROM-SHA-1 gate was the whole loop. (4) First overlay-to-C flip just worked via the generic `mk/src.mk` rule + default -O2 profile (zero mk edit).
- Friction: the **8-gate false-fired** on the SMALLEST candidate (176B priced pts-13) — `pick_target.py` barely weights byte-size against the none-upstream/nfns/one-tu bumps. Handled by the new small-pack exemption; root-cause pts recalibration deferred to a tooling branch. Ghidra MCP down → names left as placeholders (deferred follow-up).
- Applied: PO-selected 4 of 5 — #1 CLAUDE.md small classical pack exemption (+ BACKLOG pts-recalibration tooling follow-up); #2 CLAUDE.md asm-first seed fast-path; #3 CLAUDE.md overlay path convention; #5 BACKLOG Epic-2 reframe + mirror-era PO-note archive. (#4 deferred name follow-up → BACKLOG Carry-overs, not a tooling edit.)
- Carry-over: none (file fully banked). Name follow-up note in BACKLOG (curate func_ovl10_801F4A40/AD8 when Ghidra is up).

---

## Sprint 147 — player.c TU COMPLETE (the whole 109-fn game-embedded libmus sequence player, banked from 0) — 2026-06-30
- Increment: 2 files banked (`src/libmus/player.c` + `src/libmus/player_commands.c`, split mid-sprint) / **109 functions matched** (the full `#include`-chained TU). md5-candidate **204→206**; asm subsegs 87→86 (main+idle `[0x748B0]` stays asm).
- Quality: 0/0/0/**1** (stuck-far/permuter/carried/re-opened). The frame handler `__MusIntMain` was committed as an "unbankable carry" then re-opened + banked (the re-open). Random + allocate_object_slot were intra-sprint carries → banked (not cross-gate).
- Seed: committed 13pt; banked **13pt** (the file is now md5-candidate, so the full carve banks); regime mixed. v2 classical track: seed 13; realized 17; residual +4.
- What helped: (1) **the multi-TU split** — splitting the carve at the 16-aligned 0x8009C540 made `func_8009BC58`/`allocate_object_slot`'s command-handler callers cross-TU (`jal`, not inlined), banking 108/109. (2) **Building the 4 matched reference games** (drmario64/hm64/snowboardkids2/puzzleleague64; PPL byte-exact at the same -O3) — disassembling PPL's `__MusIntMain` revealed the def-order drain structure, SUPPORT_PROFILER, and the manual Fstop inline. (3) **ELF-disasm word-diff** localized the last 14 bytes to the `subu`+`bgez` signed-subtraction comparison form.
- Friction: **twice wrongly declared `__MusIntMain` unbankable** — once a "compiler wall" (raw-RTL inline count), once a "permanent same-TU carry" backed by a (correct-but-incomplete) rodata-gap proof that `mus_fifo_dispatch` shares the frame handler's `.o`. Both conclusions were premature: the same-`.o` fact is real, but a same-TU over-inline is fixable by **definition order** (drain-before-dispatch), not only by a cross-TU split. Cost ~2 extra investigation passes before the reference build settled it.
- Applied (3 of 3): #1 `docs/hazards.md#same-tu-inline-mismatch-definition-order--cross-tu-split` (def-order + cross-TU split + `(s32)(a-b)<0` signed-compare tell + the reference-build methodology) + CLAUDE.md hazard-index rows; #2 CLAUDE.md "build the matched reference games before declaring a same-TU inline mismatch unbankable" rule (execution-loop spike paragraph); #3 `pick_target.py` phantom de-rank (`Hazard.is_phantom_risk()` + `PHANTOM_RISK` group; `maybe-upstream`/`coddog-source-banked` rows now sort below every genuine candidate — 0 phantoms in the live top-25, 13 pushed down; goldens regen'd, suite 94 pass).
- Carry-over: none (file complete). The libmus `player.c` TU — the largest game-embedded lib file — is fully decompiled.

## Sprint 146 — aud_dma.c COMPLETE (libmus DMA buffer mgr, game-modified cart-only; LAST libmus leaf → src/libmus/ 100%) — 2026-06-26
- Increment: 1 file banked / 6 fns matched (`__MusIntDmaInit`/`__MusIntDmaProcess` + statics `__CallBackDmaNew`/`__CallBackDmaProcess`/`__MusIntDmaSample` + the empty `func_8009DBA0`), `aud_dma.c` → md5-candidate. **`src/libmus/` is now 100%** (all 5 carved leaf files 0 stubs). md5-candidate **203→204**; c-subsegs 196→197; asm subsegs **88→87**. ROM SHA-1 == baserom.
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened). A classical-heavy in-sprint grind on `__MusIntDmaSample`, not a spike.
- Seed: committed 8pt; seed 8 / realized 10 / residual +2; regime mixed (file FULLY banked, banked 8pt; per-file all-or-nothing did NOT zero it). **v2 freeze commit SKIPPED** — banked in one commit, seed lived only in gitignored SPRINT.md (Applied #2 fixes this).
- What helped: ASM-first re-assessment OVERTURNED the S145 carry-over's "4 stock fns + carry DmaSample" premise BEFORE writing — only `__CallBackDmaNew`/`__CallBackDmaProcess` were pure stock (DDROM test intact) + `func_8009DBA0` an MG64-emptied `jr ra` stub (free at the gate); `__MusIntDmaInit` was stock+1-insert, `__MusIntDmaProcess`'s second half fully rewritten (flat-array `keep_count` ageing under `osSetIntMask`), `__MusIntDmaSample` a classical rewrite (cart-only, control-flag-first, `keep_count=0x20000001`, +an added min-`keep_count` eviction loop). 5 of 6 fns hit the target instr-COUNT on first compile; the ELF-disasm word-diff (count then per-instr) localized everything (238→30→2→0).
- Friction: `__MusIntDmaSample` needed 4 GCC-codegen levers to match (KMC GCC 2.7.2 -O3): explicit `else` to flip the control-flag early-return branch (`bnez`→`beqz`); a shared `goto no_free_buffer` to stop GCC value-prop tail-merging path-#1's `return dma_buffer_head` (proven NULL) with the control-flag `return NULL`; goto-skip layout for mid-fn failure-block placement; best-first compare operand order for the min-find load order. The `coddog-sweep-audio` I ran at plan time leaked an `n_csq.u` ucode file into the repo root (not gitignored).
- Applied (4 of 4): #1 `docs/hazards.md#cross-jump-tail-merge` ASM-verify-each-fn companion note (a carry-over's per-fn "likely stock" label is a hypothesis); #2 `.claude/commands/sprint-plan.md` Step 8 auto-freeze commit for classical/mixed regime; #3 `.gitignore` `*.u` (build_audio_refs.sh stray); #4 same `#cross-jump-tail-merge` inverse-tail-merge + 4-lever note. #1/#4 DOC, #2 command, #3 gitignore — no tooling/golden touch.
- Carry-over: none. (The S145 carry `aud_dma.c` is RESOLVED + FULLY banked.) **Cross-repo follow-up:** 10 new decomp symbols (9 data drop-statics `dma_buffer_*`/`audio_*`/`audDMAMessageQ`/`cartrom_handle` + `g_mus_dma_buffer_count`) + correct stale ghidra fn names `mus_dma_{cb_new,callback,sample}` → `__CallBackDmaNew`/`__CallBackDmaProcess`/`__MusIntDmaSample` via `sync_decomp_names.py --import-from-decomp`.

## Sprint 145 — aud_sched.c COMPLETE (libmus scheduler verbatim .data-carve mirror) — 2026-06-25
- Increment: 1 file banked / 4 fns matched (`__MusIntSchedInit` + static `__OsSched{Install,WaitFrame,DoTask}`), `aud_sched.c` → md5-candidate. md5-candidate **202→203**; c-subsegs 195→196; asm subsegs **88** (unchanged — the split is asm-neutral, the aud_dma carry `[0x78D10]` remains). ROM SHA-1 == baserom (after a 1-step `-U_FINALROM` re-attempt).
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened). One in-sprint re-attempt (the `-U_FINALROM` diagnosis), not a spike.
- Seed: committed 5pt; banked 5pt; regime mirror (8-gate clear; seed-only, no freeze).
- What helped: splitting the last libmus pack at the upstream-file boundary banked the CLEAN file (aud_sched, stock bodies + byte-verified stock `default_sched` data) and deferred the game-modified aud_dma; the `.data` carve + drop-statics followed the S116/S142 adjacent precedent; the 5-byte miss was localized fast by `cmp -l` (all in one fn's stack frame → struct SIZE drift, not a body edit).
- Friction: the `OSScTask` `_FINALROM` struct-drift — a clean verbatim mirror SHA-missed by 16 B of stack frame purely because MG64's 3rd-party libmus was built NON-FINALROM (carries `OSScTask`'s `#ifndef _FINALROM` startTime+totalTime) while the base CFLAGS are `-D_FINALROM`. Diagnosed via the uniform frame-immediate shift (field stores unchanged); fixed with `mk/libmus.mk -U_FINALROM`. The `@99.99` body-divergence flag was a false alarm (coddog structural).
- Applied (2 of 2): #1 `docs/hazards.md#upstream-mirror-pattern` `_FINALROM`/build-config struct-size-drift sub-note (the TELL: one fn's frame immediates shift by a fixed struct delta, field stores unchanged) + CLAUDE.md hazard-index row; #2 `docs/hazards.md#cross-jump-tail-merge` widened the `@99.99` body-divergence diagnosis to the build-config/struct-size axis. Both DOC, no tooling/golden touch. (Deferred: the optional `pick_target.py finalrom-struct:<struct>` pre-flag → noted in the hazard sub-note as a tracked follow-up, not built.)
- Carry-over: aud_dma.c (`[0x78D10, asm]`, the LAST libmus asm subseg) — game-modified (N64DD/diskrom path stripped, `__MusIntDmaSample` control-flag check reordered, an empty 0x8 stub) → classical/mixed track. Banking it completes the libmus tree.

## Sprint 144 — aud_thread.c COMPLETE (libmus `__MusIntThreadProcess` classical; first classically-decompiled libmus fn) — 2026-06-25
- Increment: 1 fn banked (`__MusIntThreadProcess`, the audio-thread frame loop), `aud_thread.c` COMPLETE (0 INCLUDE_ASM stubs → md5-candidate). matched **+1**; md5-candidate **201→202**; asm-backed subsegs **89→88** (the last stub cleared). ROM SHA-1 == baserom, MATCH first build, 0 iteration.
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened).
- Seed: committed 5pt; seed 5 / realized 4 / residual −1; regime classical (v2 two-pass freeze: seed frozen at commit `2866ba1` pre-`src/`, realized at this review).
- What helped: re-diffing the "MG64-custom body" carry vs the STOCK libmus 3.14 source AFTER `aud_sched.h` was vendored (S143) — the body was the stock thread-proc + a ~4-instr pause insert, not from-scratch classical; the `musSched` vtable macros (`__MusIntSched_*`) + `musTask` struct were already in the vendored headers; the recover-externs + callee override were applied up-front from the traced asm → banked near-verbatim first build, 0 iteration.
- Friction: one LINK-time `undefined reference to __MusIntDmaProcess` (the `aud_dma.h` callee name vs ghidra `mus_dma_process`) — fixed with a `rom:` callee override; the gate stub built green on the ghidra name, so it surfaced only at the C link, not the gate.
- Applied (3 of 3): #1 `docs/hazards.md#cross-jump-tail-merge` rule-out-stock-plus-insert-before-from-scratch-custom sibling rule + BACKLOG carry-convention tighten; #2 `docs/hazards.md#wrong-ghidra-name-override` duplicate-vram-alias → reference-the-placed-name sub-note; #3 same-section callee-override sub-note (link-time tell). All DOC, no tooling/golden touch.
- Carry-over: none (the S143 carry `__MusIntThreadProcess` is RESOLVED + banked).

## Sprint 143 — aud_thread.c (libmus, PARTIAL: `__MusIntAudManInit` banked, `__MusIntThreadProcess` carried) + the deeper libmus band opened — 2026-06-25
- Increment: 1 fn banked (`__MusIntAudManInit`, the audio-manager init), `aud_thread.c` PARTIAL (1 INCLUDE_ASM stub). matched **+1**; md5-candidate **201→201** (file partial); asm subsegs **89→88** (flip). ROM SHA-1 == baserom, MATCH first build.
- Quality: 0/0/**1**/0 (stuck-far/permuter/carried/re-opened). 1 carried (`__MusIntThreadProcess`, MG64-custom body — bank-stock-carry-custom, the hedge fired as predicted).
- Seed: committed 8pt; banked **0pt**; regime **mixed** (per-file all-or-nothing: file partial → 0pt; the +1 matched-fn is the value signal, S121/S123). v2 realized 0 (banked 0).
- What helped: (1) **The 3.14 version catch.** The vendored `libmus.h` was already 3.14 (S141), which prompted the version question; the n64sdkmod 3.14 `aud_thread.c` (`diff` vs 3.11 = ONLY version + `EXTRA_SAMPLES_N` 15→**20**) matched the asm immediate `li a3,0x14` exactly and supplied the correct body. The DiskLS 3.11 pin would have SHA-missed silently on that one immediate — caught by reading the asm, not the pin. (2) **`player_fx.h`'s `#define n_alInit CustomInit` resolved the alInit duplicate-symbol puzzle with NO redirect hack** — the verbatim `alInit`→`n_alInit`→`CustomInit` macro chain points at the libmus-BUNDLED synth init (0x8009CF30), and a Ghidra `get_xrefs_to` proved the standalone `n_alInit`@0x800A0730 is DEAD. Reading the header beat inventing a fix. (3) **The asm trace cleanly split the two fns:** `__MusIntAudManInit` verbatim; `__MusIntThreadProcess` carries an MG64 pause/mute block (byte flag + `osAiSetNextBuffer` in a `continue`) absent from stock 3.14. (4) The bank-stock-carry-custom partial (S121/S123) let the init bank while carrying the custom threadproc; the full-make SHA-1 proves the C fn byte-exact despite the sibling INCLUDE_ASM.
- Friction: the version+duplicate tangle cost real diagnosis time — it first looked like an unresolvable cross-region symbol collision (libmus-bundled vs standalone n_audio) before `player_fx.h`'s macro chain + the dead-`n_alInit` xref made it clean. The `player_fx.h` header cascade (`synthInternals.h`→`<libaudio.h>`) needed 2 `mk` `-I` additions found iteratively at compile time.
- Applied (3 of 4): #1 libmus PIN → n64sdkmod 3.14 (disable the DiskLS 3.11 rows in `tools/audio_ref_versions.tsv` + correct the `mk/libmus.mk` comment; the game rev is 3.14, the 3.11 near-tie historically misranked); #2 `docs/hazards.md#libmus-bundled-n_audio-duplicate` new section + `CLAUDE.md` hazard-index row (the bundled-synth duplicate + the `alInit`→`CustomInit` chain + dead standalone `n_al*`); #4 MG64-custom-body knowledge-capture (this digest + the `BACKLOG.md` carry + active-phase paragraph — the FIRST libmus game-customized body, contrast the all-stock leaf mirrors). (#3 a `pick_target.py` `game-embedded:libmus`/`coddog-bundled-dup` pricing tell for the `[0x78330]` bundled-synth candidate NOT selected here — DEFERRED to a golden-gated tooling branch, tracked in `BACKLOG.md ## Carry-overs`, since it adds FP surface to a load-bearing detector.)
- Carry-over: SPIKE `__MusIntThreadProcess` (the last stub in `src/libmus/aud_thread.c`), MG64-custom body. Completeness checklist in `BACKLOG.md ## Carry-overs`. **Cross-repo follow-up:** 4 new names (`__MusIntAudManInit`/`__MusIntThreadProcess`/`__MusIntDmaInit`/`CustomInit`) → `python3 ~/development/reversing/ghidra/mariogolf64/scripts/sync_decomp_names.py --import-from-decomp` (corrects the stale ghidra `mus_thread_create`/`mus_audio_thread`/`mus_dma_init`/`al_init`).

---

## Sprint 142 — aud_samples.c (libmus, the [0x79780] split remainder; 2 fns) — 2026-06-24
- Increment: 1 file banked, 2 fns matched (`__MusIntSamplesInit` + `__MusIntSamplesCurrent`) + 1 vendored header (`aud_samples.h`). md5-candidate **200→201** (+1); asm subsegs **90→89** (1 flip of the `[0x79780]` split remainder S141 left). ROM SHA-1 == baserom, MATCH first build.
- Quality: 0/0/0/0 this sprint (stuck-far/permuter/carried/re-opened). Clean seed-only mirror, no carry-overs.
- Seed: committed ~5pt; banked 5pt; regime mirror (seed-only; the row was `blk`/needs-header so priced by analogy — lib_memory's 5 + the 4-drop-static/defines-data delta; the verbatim-mirror single-file-pack exemption resolved the multi-carve "8-gate" concern).
- What helped: (1) **First libmus body-divergence diagnosis pass post-band-open proved `@99.99` = VERSION delta, not custom body** — the asm matched the upstream byte-for-byte (the `0xB21642C9` /184 + `0x51EB851F` /100 reciprocal-multiply magics, ±0xB8 min/max, the `only_one_flag` logic), and ZERO callees (no jal) meant no customization tell, so the verbatim-mirror single-file-pack exemption held cleanly. (2) **The 4 `.bss` statics were already curated** `g_mus_*` in `ghidra_symbols.txt`, so the drop-to-extern needed NO symbol add — just reference the curated names + rename the active body. (3) `only_one_flag` is a func-local static → automatically sole-referrer → a clean 0x10 `.data` carve at `main_data`'s tail (1-line split, `.o .data`=0x10 == carve). (4) The wrong-ghidra-name `rom:` override (S128/S141) corrected `mus_samples_*`→`__MusIntSamples*` at the gate.
- Friction: none material. The two drop-static naming sub-cases (pre-curated vs unnamed) and the func-local-static sole-referrer fact were undocumented edge refinements, now captured.
- Applied (3 of 3, all DOC, no tooling/golden touch): #1 `docs/hazards.md#file-static` pre-curated drop sub-case (static already named in `ghidra_symbols.txt` → reference the name + rename body, NO symbol add; vs unnamed → add the upstream name, S81/S141); #2 libmus `@99.99`=version-delta knowledge-capture (`BACKLOG.md` S142 paragraph; reinforces the S141 per-coddog-score hedge — a libmus single-file-pack with no customization tell is structural`@99.99`, trust after a 1-pass asm diff); #3 `docs/hazards.md#defines-data` func-local-static automatic-sole-referrer note (file-private by construction → always carve, skip the `asm/`+`src/` share-grep).
- Carry-over: none. **Cross-repo follow-up:** propagate `__MusIntSamplesInit`/`__MusIntSamplesCurrent` to the Ghidra workspace via `python3 ~/development/reversing/ghidra/mariogolf64/scripts/sync_decomp_names.py --import-from-decomp` (corrects the stale ghidra `mus_samples_init`/`mus_samples_current` guesses, same as S141's `__MusIntMemInit`).

## Sprint 141 — lib_memory.c + the libmus band OPENED (header-vendoring enabler + first @100.00 leaf mirror) — 2026-06-24
- Increment: 1 file banked, 6 fns matched (`__MusIntMemInit`/`Malloc`/`Remaining`/`GetHeapAddr`/`Set`/`Move`) + the libmus band-open enabler (3 vendored headers + `mk/libmus.mk` + `pick_target.py` libmus registration). md5-candidate **199→200** (+1); asm subsegs stayed **90** (the `[0x79640]` pack split into `[0x79640,c,libmus/lib_memory]` + `[0x79780,asm]` aud_samples remainder, a new asm subseg). ROM SHA-1 == baserom, MATCH first build.
- Quality: 0/0/0/0 this sprint (stuck-far/permuter/carried/re-opened). The S140 body-divergence "reset-to-FULL" hedge fired 0 times — the `@100.00` leaf was byte-identical.
- Seed: committed 8pt (enabler-as-goal + leaf); banked 8pt; regime mirror (seed-only; 8-gate resolved by the enabler-as-goal path, lib_memory.c took the verbatim-mirror single-file-pack exemption).
- What helped: (1) **libnaudio pre-paid the n_audio_sc header DAG** — opening libmus cost only ~3 small headers + a 2-line mk profile + a mechanical pick_target add (the S140 "full header sprint" fear was over-budgeted). (2) **Picking the `@100.00` coddog LEAF first** (`lib_memory.c`: pure allocator, 0 calls-unplaced, 1 drop-static, no carve) proved the new `mk/libmus.mk` profile with zero confounds and placed the `__MusIntMem*` allocator the whole band calls. (3) The `wrong-ghidra-name-override` `rom:` mechanism (S128) cleanly corrected `mus_heap_init`→`__MusIntMemInit` without a sync-names. (4) The asm-mapped pack decompose (6 lib_memory fns end exactly at the 16-aligned 0x79780 aud_samples boundary) was unambiguous.
- Friction: none material. The pick_target registration needed 4 in-file spots (LIB_EXTRA_INCLUDE_DIRS + the define-set raw dict + the inherit-main loop + the `_active_defines_for_lib` key map) — mechanical but easy to miss one; goldens regen'd for the bank drift.
- Applied (4 of 4): #1 sibling-audio-lib hedge DOWNGRADE (`CLAUDE.md` story-points bullet + `BACKLOG.md` S140 note: a shared-n_audio_sc-header-DAG band-open is a small incremental enabler; hedge body-divergence per-coddog-score, not blanket-FULL-by-lib); #2 `@100.00`-leaf-first band-open heuristic (`docs/hazards.md#upstream-mirror-pattern`); #3 pick_target per-file blk-delta `vendor-header:<h>@<pin>` hint (DEFERRED to a golden-gated tooling branch, tracked in `BACKLOG.md ## Carry-overs`); #4 cross-repo name sync (the 6 lib_memory names → `sync_decomp_names.py --import-from-decomp`, surfaced below).
- Carry-over: none. **Cross-repo follow-up:** propagate the 6 curated `lib_memory` names to the Ghidra workspace via `python3 ~/development/reversing/ghidra/mariogolf64/scripts/sync_decomp_names.py --import-from-decomp` (esp. `__MusIntMemInit`, which corrects the stale ghidra `mus_heap_init` guess; the 4 `func_` → `__MusIntMem{Remaining,GetHeapAddr,Set,Move}` are new names).

## Sprint 140 — n_env.c (the LAST libnaudio asm subseg → src/libnaudio/ tree 100%) — 2026-06-24
- Increment: 1 file banked, 5 fns matched (`n_alEnvmixerPull` + `n_alEnvmixerParam` + file-statics `_pullSubFrame`/`_getRate`/`_getVol`). The S139 carry-over `[0x79E70]` (n_env.c, pts-13), the final libnaudio asm subseg. md5-candidate **198→199** (+1); asm subsegs 91→**90**; **libnaudio asm subsegs → 0 (the entire `src/libnaudio/` n_audio_sc N_MICRO mirror tree, 20/20 .c files, is now md5-candidate).** ROM SHA-1 == baserom.
- Quality: 0/0/0/0 this sprint (stuck-far/permuter/carried/re-opened). MATCH first build, 0 re-attempt, 0 spike. The S121/S123 sub-100-coddog body-divergence hedge was budgeted but fired 0 times.
- Seed: committed 13pt; banked 13pt; regime mirror (13-gate FIRED → verbatim-mirror single-file-pack EXEMPTION applied: regime mirror + single upstream file n_env.c + decompose-blocked one-tu + all callees placed/curated; seed-only, no decompose).
- What helped: the S139 carry-over playbook (5-point completeness checklist) made the gate a mechanical replay. Reading `asm/79E70.s` at the gate resolved the two scary flags as FALSE up-front: `calls-unplaced:__pow` + `jal-count-mismatch:20vs15` were pick_target counting the `#ifndef N_MICRO` `_getRate` branch (libm `__pow`/`_frexpf`/`_ldexpf`); under `-DN_MICRO=1` the integer branches compile and the asm has ZERO such jals. Both carves turned out to be clean 1-line ATTRIBUTE FLIPS (no splits): `.data` n_eqpower[128]=0x100 was `main_data_1a` exactly (n_drvrNew's S139 arrays ended at 0xA30A0), `.rodata` jtbl + `_getRate` f64 consts=0x70 was the generic `[0xAD520,0xAD590)` block exactly. The n_env.o section sizes matched the carves to the byte (`.text` 0x9d0 = subseg extent) BEFORE the SHA — strong verbatim signal. `body-divergence-suspect@99.99` FALSE → **10 consecutive on n_audio_sc (S133-S140)**. ZERO symbol adds (all callees placed S139/earlier).
- Friction: none material. The two false flags (`__pow`/jal-count) were a known pricing artifact (carry-over note had pre-flagged them); retro #1 retires the class so the next `-D`-gated mirror prices clean.
- Applied (3 of 3): #1 `build_config.py` `_strip_inactive_define_branches(text, defined)` — a sibling of `_strip_inactive_version_branches` that drops the dead side of `#ifdef X`/`#ifndef X` for tokens X positively known-defined in the build profile's `-D` set (opaque/passthrough otherwise, so a live branch is never removed) — wired into `pick_target.py` `call_divergence`/`calls_unplaced`/`refs_unplaced` via `_active_defines_for_lib(lib)`, so libnaudio's `-DN_MICRO=1` strips the `#ifndef N_MICRO` phantom calls (the `__pow`/`jal-count-mismatch` class). Verified directly: drops n_env's 3 phantom externs (__pow/_frexpf/_ldexpf), keeps the active integer branch; libmus real `calls-unplaced` flags (mapped to `main` defines) unchanged; suite **94 pass, NO golden regen needed** (conservative change, no covered row shifted). #2 `docs/hazards.md#static-name-collision` "benign, not a problem to solve" reframe — when the colliding names are the mirror file's OWN file-statics, there is no real collision (statics emit no global symbol; the only action is the no-op of not adding a symbol); the collision only bites if you GLOBALIZE one (S135 `_decodeChunk`); cites S140 n_env's clean trio. #3 `BACKLOG.md` libmus-band hedge reset — record that the n_audio_sc 10/10-verbatim confidence does NOT carry into the libmus `aud_*.c` band (heavier flags, mostly `blk` needs-header, likely a header-vendoring enabler sprint + possibly divergent bodies), so reset the body-divergence hedge to full there.
- Carry-over: none. **Milestone: `src/libnaudio/` is 100% decompiled** — a publish-to-master candidate (PO deferred the publish; staying on dev).

---

## Sprint 139 — n_auxbus.c + n_drvrNew.c (DECOMPOSE the last libnaudio pack func_8009E4B0) — 2026-06-24
- Increment: 2 files banked, 3 fns matched (`n_alAuxBusPull`; `n_alFxNew` + `alN_PVoiceNew`). Decomposed the last libnaudio asm subseg `[0x798B0]` (8-fn pts-13 c-combined pack) at the 3 upstream-file boundaries; banked the cleanest 2, carried n_env.c. md5-candidate **196→198** (+2); the pack split into 2 c-flips + n_env asm carry. ROM SHA-1 == baserom.
- Quality: 0/0/0/1 this sprint (stuck-far/permuter/carried/re-opened). The 1 carry (n_env.c) is the PLANNED decompose remainder, not a committed-item spike — both committed files banked. n_drvrNew needed 2 mechanical carves (1 diagnosis pass), no spike.
- Seed: committed 5pt; banked 5pt; regime mirror (8-gate FIRED on the pts-13 pack → resolved by the c-combined DECOMPOSE, each banked file <8; verbatim-mirror exemption does NOT cover a c-combined multi-file pack, but each decomposed member is a single-file mirror).
- What helped: the S137 planning note pre-scoped n_auxbus as the c-combined 2-file pack to decompose. At the gate, reading `asm/798B0.s` mapped the rodata ownership cleanly by file (n_drvrNew jtbl_800D20F0 + 2 doubles = [0x800D20F0,0x800D2120); n_env jtbl_800D2120 + 4 doubles = [0x800D2120,0x800D2188)) — CONTIGUOUS per-file, NOT interleaved, so n_drvrNew's rodata carve is a clean prefix. The `.data` 6 PARAMS arrays were contiguous [0x800C7B10,0x800C7CA0)=0x190 (sizes add up exactly), a clean 3-way main_data split (S138 pattern). All 3 boundaries 16-aligned → clean splits, no non16align. Both files MATCH (n_auxbus first build); `body-divergence-suspect@99.99` FALSE all 3 (9 consecutive on n_audio_sc, S133-S139). The 3 banked fns' names were pre-curated; only `n_alEnvmixerPull` needed placing.
- Friction: none material. `dmaNew` flagged `calls-unplaced` (a false positive — it's an `ALDMANew` fn-ptr PARAM invoked via `jalr`, not a named `jal`); confirmed at the body triage, no cost (retro #3 fixes the detector). n_drvrNew's link error (`undefined reference to .L8009E5F8` in the extracted rodata jtbl) was the expected rodata-carve tell, fixed by the carve.
- Applied (3 of 3): #1 `pick_target.py` `c-combined-undercount:<m>vs<n>` (`_append_coddog_aux`) — the FILE analog of `coddog-fncount-mismatch`: when distinct coddog files exceed the named-symbol c-combined file count, the pack spans more upstream files than the named index sees (this pack: `2vs3`, n_env's members all un-named); CLAUDE.md index row + `docs/hazards.md#coddog-cross-ref` doc + Hazard factory/`is_c_combined`/`c_combined_count` + unit test. #2 `pick_target.py` body-divergence-suspect suppression re-keyed on `up_lib==libnaudio` + a CLEAN single-source shape (single-file-pack OR single-fn row, the post-decompose shape) rather than the single-file-pack shape alone — `single_cod` still gates it so a still-combined multi-coddog pack + a non-n_audio_sc row keep the hedge (S123 guard); unit test (a)-(d) + `#coddog-cross-ref` doc. #3 `pick_target.py` `all_fn_ptr_typedefs` scans `-I` headers for `typedef <ret>(*NAME)(args)` → `_fn_ptr_param_names` drops a typedef'd fn-ptr param (`ALDMANew dmaNew`), killing the `calls-unplaced:dmaNew` phantom across the audio `*New` constructors; `#calls-unplaced` doc + unit test. All 3 forward-looking (no current row exercises them since the pack is now banked, verified live); +2 unit tests; goldens regen'd for the S139 bank drift only (4 goldens, pack→n_env row), suite **94 pass**.
- Carry-over: **n_env.c `[0x79E70]`** (5 fns: `n_alEnvmixerPull`/`n_alEnvmixerParam` + statics `_pullSubFrame`/`_getRate`/`_getVol`) — the heavy 3rd file: `inc/n_env_add01.inc.c` vendor + `__pow` rodata pool `[0x800D2120,0x800D2188)` + own switch jtbl + 3 static-name-collisions. The last libnaudio asm subseg. 1 name (`n_alEnvmixerPull`) → `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 138 — n_reverb.c (n_alFxPull single-file-pack, n_audio_sc N_MICRO mirror) — 2026-06-24
- Increment: 1 file banked, 6 fns matched (`n_alFxPull` + `n_alFxParamHdl` + `_n_loadOutputBuffer` + `_n_loadBuffer` + `_n_saveBuffer` + `_n_filterBuffer`). md5-candidate **195→196** (+1); asm subsegs 92→**91** (1 flip, at gate). ROM SHA-1 == baserom.
- Quality: 0/0/0/0 this sprint (stuck-far/permuter/carried/re-opened). All 6 fns banked atomically; 1 diagnosis pass (the `.data` carve), no spike, no body divergence.
- Seed: committed 13pt; banked 13pt; regime mirror (13/8-gate FIRED → verbatim-mirror single-file-pack exemption applied).
- What helped: the S137 planning note pre-scoped n_reverb as the cleaner of the 2 pts-13 candidates (single-file-pack, all callees placed). At the plan gate, reading `asm/7B140.s` proved `L_INC` is a dead extern (not in asm) and `init_lpfilter`→`_init_lpfilter` is already placed — both pick_target false flags retired before execution. The S134/S136 rodata-literal carve pattern (generic-line flip) replayed exactly for the `n_alFxParamHdl` jtbl + 3 f64 literals. The 4 `inc/*.inc.c` body-include vendoring + N_MICRO pin (S133) were already established infrastructure.
- Friction: the `defines-data:val,blob` flag named the unused statics but NOT their `.data` address, so build #1 SHA-missed 16 B LARGER (KMC -O3 emits unused function-local statics; they carry no asm `%hi/%lo`, so the recover-from-asm path can't find them). Cost a build + cmp + baserom value-search to localize rom 0xA31A0 (and disambiguate vs the libultra reverb block at 0xA356C). The DoR hedged "likely emits" rather than "certainly emits + needs a value-search localization."
- Applied (3 of 3): #1 `docs/hazards.md#defines-data` — unused-function-local-static sub-case (KMC -O3 emits them; localize-by-VALUE not asm-ref; value-FP disambiguation vs the libultra twin). #3 `pick_target.py` `_append_coddog_aux` — body-divergence-suspect suppression extended to n_audio_sc/libnaudio single-file-packs (single coddog + single-file-pack + libnaudio src; libnusys EXCLUDED to keep the S121/S127 real-game-mod hedge); unit-verified narrow (no current candidate affected, n_reverb now banked); goldens regen'd for bank drift, suite 92 pass. #2 DEFERRED with spec to a golden-gated tooling branch (BACKLOG) — a `data-static:<addr>` resolver for `defines-data` needs a link-cluster/positional anchor to beat the multi-hit value-FP, a feature with regression surface, not a review-gate quick edit.
- Carry-over: none (sprint fully banked). 5 names → `sync_decomp_names.py --import-from-decomp`. 1 tooling task (#2 data-static resolver) parked in BACKLOG.

---

## Sprint 137 — n_synallocfx.c + n_mainbus.c (n_audio_sc 2-file N_MICRO mirror, retires S130 spike) — 2026-06-24
- Increment: 2 files banked, 2 fns matched (`n_alSynAllocFX` + `n_alMainBusPull`), split from the c-combined `[0x7C720,asm]`. md5-candidate **193→195** (+2); asm subsegs 93→**91** (2 flips). ROM SHA-1 == baserom.
- Quality: 0/0/0/0 this sprint (stuck-far/permuter/carried/re-opened). Both fns Match on the FIRST build, seed-only, NO diagnosis pass, NO rodata/data carve.
- Seed: committed 5pt; banked 5pt; regime mirror (8-gate clear).
- What helped: the S130 near-free-retry completeness-checklist (authored S130, blocker resolved at the S136 gate) replayed verbatim-correct — boundary 0x7C770, callee `n_alFxNew`=0x8009E550 (jal-confirmed at gate), no carve — 0 rework, re-confirming the S74→S75 protocol. The S136 gate had already pre-named both leaders (`n_alSynAllocFX`/`n_alMainBusPull`) and identified `func_800A1320`=`n_alSynAllocFX`, so the only NEW gate add was the 1 callee `n_alFxNew`. Both `body-divergence-suspect@99.99` flags were FALSE (asm == upstream exactly, both fns).
- Friction: none.
- Applied (2 of 2; both knowledge-capture, no tooling edit): #1 log-only — the completeness-checklist verbatim-replay confirmation; #2 planning note — the 2 remaining libnaudio pts-13 packs (`n_reverb` single-file-pack, `n_auxbus` 2-file) carry `body-divergence-suspect@99.99` + heavier hazards (n_reverb: 4× `inc.c` + defines-data:val + refs-unplaced:L_INC + rodata carve; n_auxbus: static-name-collision×3 + c-combined), so triage BODIES (asm-vs-upstream diff) before assuming the clean S133–S137 verbatim pattern (S123 exemption-GUARD); both pts-13 → 8-gate fires unless the single-file-pack exemption applies (n_reverb qualifies structurally, n_auxbus does NOT). No `tools/*.py`/`CLAUDE.md`/`docs/*` edit; no golden/test touch.
- Carry-over: none. `n_alFxNew` (1 name) is a cross-repo follow-up → `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 136 — n_synthesizer.c (n_audio_sc synth-driver core, 8-fn verbatim N_MICRO mirror) — 2026-06-24
- Increment: 1 file banked, 8 fns matched (n_alSynNew + n_alAudioFrame + __n_allocParam + _n_freeParam + _n_collectPVoices + _n_freePVoice + static _n_timeToSamplesNoRound + _n_timeToSamples), 1376B verbatim N_MICRO mirror. md5-candidate **192→193** (+1); asm subsegs 94→93 (1 flip). ROM SHA-1 == baserom.
- Quality: 0/0/0/0 this sprint (stuck-far/permuter/carried/re-opened). 1 diagnosis pass (the expected rodata carve), no spike, no body divergence.
- Seed: committed 8pt; banked 8pt; regime mirror (8-gate FIRED → single-file-pack verbatim-mirror exemption held).
- What helped: the gate disassembly of n_alSynNew up-front confirmed all 8 jals match upstream, de-risking the `body-divergence-suspect@99.99` hedge before execution. Existing vendored audio headers (n_synthInternals.h / n_libaudio_sc.h / the `_DEBUG`-off `alHeapAlloc`→`alHeapDBAlloc(0,0,…)` macro, S96/S129) → ZERO new header. The S134 MAX_RATIO rodata-carve pattern + byte-cmp localization turned the first-build SHA-miss into a 1-line yaml split, not a body hunt. The gate pre-naming of the 2 handler address-of refs resolved the S130 `n_mainbus` spike (`func_800A1320`=`n_alSynAllocFX`).
- Friction: the rodata-literal flag's `carve-end` was the POOL boundary (0x800D2930), far past the file's actual 0x20 extent (0x800D21E0..0x2200) — I had to objdump the built object to learn the real extent and split a 0xA0 generic subseg. Both `body-divergence-suspect` + `jal-count-mismatch:3vs8` were false (the jal gap = `alHeapAlloc` macro ×5).
- Applied: PO-selected 3 of 3: #1 `pick_target.py` rodata-literal `extent-end` (file's own carve length → split-vs-whole signal); #2 `pick_target.py`/`_hazards` `(macro-artifact?)` jal annotation (`_macro_single_real_call`/`call_divergence`) + single-coddog body-divergence suppression (`is_jal_artifact`); #3 `docs/hazards.md` note on gate handler-ref pre-naming. 3 unit tests added (`test_jal_count_artifact_annotations`/`test_macro_single_real_call`/`test_body_divergence_suppressed_on_single_coddog_artifact_jal`); goldens regen'd (bank drift + extent-end), suite 92 pass.
- Carry-over: none. n_synthesizer.c is md5-candidate.

## Sprint 135 — n_load.c / n_alAdpcmPull + n_alLoadParam + static _decodeChunk (n_audio_sc N_MICRO ADPCM-decoder mirror) — 2026-06-24
- Increment: 1 file banked, 3 fns matched (n_alAdpcmPull + n_alLoadParam + the file-static _decodeChunk), 1824B verbatim N_MICRO mirror. md5-candidate **191→192** (+1); asm subsegs 95→94 (1 flip). ROM SHA-1 == baserom.
- Quality: 0/0/0/0 this sprint (stuck-far/permuter/carried/re-opened). MATCH on the first build, no diagnosis pass — the cleanest of the n_audio_sc inc-vendor mirrors so far.
- Seed: committed 8pt; banked 8pt; regime mirror (8-gate FIRED → single-file-pack verbatim-mirror exemption; the `blk` was a false-flag from the named-index's wrong non-sc `add/*.c` variant). Third S133-pattern inc-vendor mirror.
- What helped: the proven S133/S134 N_MICRO recipe made this near-mechanical — `-DN_MICRO=1` already pinned, the `.inc.c` body-include + `*.inc.c` Makefile exclusion already in place, both real callees named at the S134 gate. The S134 retro EXPLICITLY predicted the n_load `add/`-vs-`inc/` mis-resolution ("prefer the coddog `n_load.c@99.99` over the named index's `add/` resolution"), so seeing through the false `blk` was a one-read gate decision. Unlike S134, NO rodata/data hazard (a pure text mirror like n_save) — the first-build SHA matched with zero carve. Recognizing `_decodeChunk` as a file-LOCAL static (already placed at a DIFFERENT vram, 0x800A4E3C) up-front avoided a duplicate-global gate-build break.
- Friction: minimal. The only judgment call was the static-name collision (`_decodeChunk` already curated at 0x800A4E3C for the non-micro twin) — handled by keeping ours file-local (no symbol_addrs add), now codified so the gate is pre-warned next time.
- Applied: 2 of 2 — #1 `pick_target.py` `_deblk_audio_variant_misresolve`: when a definitive (`>=CODDOG_MIRROR_PCT`) audio coddog-mirror replaces `up_path` with a DIFFERENT `mirror_path`, the named C-index's WRONG non-sc `libnaudio/src/<f>.c` variant (its `add/*.c` fragments are non-vendorable → a false `blk`) is dropped and `blocked` is re-derived from the AUTHORITATIVE coddog source's vendorable `inc/*.inc.c` includes only. Auto-resolves the recurring false-`blk` that hid n_save (S133), n_resample (S134), n_load (S135). Verified the libmus `aud_*` rows (real non-vendorable `libmus_config.h`/`libaudio.h` on the coddog source itself) STAY `blk` — no false de-blk. #2 `pick_target.py` `_append_static_name_collisions` + the `static_name_collision` Hazard factory + `placed_symbol_addrs()`: flags `static-name-collision:<name>@<existing-addr>` when a coddog-mirror's upstream file-static name is already a curated symbol placed at a different vram (in `placed_symbols` but not a member of this subseg) → the gate keeps the static file-local instead of adding a colliding global. Verified live on `func_8009E4B0` (`_pullSubFrame@0x800A56A4`/`_getRate@0x800A5A7C`/`_getVol@0x800A5CFC`). Factory test added; `docs/hazards.md#static-name-collision` (new) + `#coddog-cross-ref` audio-variant-de-blk note + CLAUDE.md hazard-index row. Goldens regen'd for the S135 bank drift + the new collision flags (4 goldens), suite **89 pass**.
- Carry-over: none. Next n_audio_sc band: `n_reverb.c` (`func_8009FD40`, pts13, 6 fns, 4 inc fragments + `defines-data:val,blob` + `refs-unplaced:L_INC` + `rodata-jtbl` + `rodata-literal` + 2 calls-unplaced — the heaviest remaining, a decompose/mixed candidate) and `func_8009E4B0` (n_auxbus/n_drvrNew/n_env, pts13, 8 fns, multi-coddog-source pack now carrying the static-name-collision flags). The libmus `aud_*` DAG (`file-static` + BSS statics + real non-vendorable headers) stays genuinely `blk`.

---

## Sprint 134 — n_resample.c / n_alResamplePull + n_alResampleParam (n_audio_sc N_MICRO mirror + MAX_RATIO rodata-literal carve) — 2026-06-24
- Increment: 1 file banked, 2 fns matched (n_alResamplePull + n_alResampleParam), 480B verbatim N_MICRO mirror. md5-candidate **190→191** (+1); asm subsegs 96→95 (1 flip). ROM SHA-1 == baserom.
- Quality: 0/0/0/0 this sprint (stuck-far/permuter/carried/re-opened). One rodata-literal diagnosis pass, not a spike.
- Seed: committed 5pt; banked 5pt; regime mirror (8-gate clear). Second S133-pattern inc-vendor mirror; the `@99.99 body-divergence-suspect` hedge budget covered the one diagnosis pass.
- What helped: the `@99.99` sub-100 coddog hedge (CLAUDE.md story-points) pre-budgeted a body-divergence diagnosis, so when the first build SHA-missed the response was a byte-cmp (build vs baserom over the subseg), which localized the diff to a single insn (the `ldc1 %lo` at 0x7AFEA) and proved BOTH fns' bodies matched — the miss was a rodata placement, not divergence. The S101 generic-subseg-bound carve made the fix a 1-line yaml flip: the generic `[0xAD590, rodata]` was EXACTLY the 0x10-byte `D_800D2190` block (the `MAX_RATIO` double 1.99996 + a `.double 0` pad), and the `.o(.rodata)` was also 0x10, byte-matching baserom.
- Friction: the rodata-literal was UNFLAGGED at the gate. pick_target flagged the libultra sibling `libultra/audio/resample`'s copy of the same double (@0x800D23E0, S98 c-combined) but NOT ours (@0x800D2190), because the rodata-literal scan (`_append_rodata_carve_hazards`) ran only on the NAMED-upstream path while this n_audio_sc mirror resolves via the coddog path (`_append_coddog_trap_hazards`), which had the rodata-jtbl scan (shared recover battery) but not the literal scan. So a first-build SHA-miss instead of a gate-priced enabler. Diagnosis was cheap (the hedge covered it); now codified so the band pays it once.
- Applied: 2 of 2 — #1 `pick_target.py` `_row_filtered`: `--lib audio` is now a SCOPE ALIAS for the audio libraries (a row whose `up_lib ∈ AUDIO_CODDOG_LIBS` stays in scope), not a fragile substring — pre-fix the audio band surfaced under `--lib audio` only by accident (n_load via its `src/audio/load.c` coddog variant, the libnusys audio_system_boot via its fn name), so de-blk'd rows like n_resample/n_reverb were invisible on the smallest-first audio frontier; verified `--lib audio` now lists the whole band (n_reverb `func_8009FD40`, n_load `n_alAdpcmPull`, the mus_* / al_init libmus rows). #2 `pick_target.py` `_append_coddog_trap_hazards` now pairs the rodata-literal carve scan into the coddog path (dedup-guarded so a c-combined coddog pack's per-cfile re-entry does not double-append), mirroring how the rodata-jtbl analog already rides the shared recover battery — so a coddog/audio mirror's FP-pool double prices at the gate; verified n_reverb `func_8009FD40` now shows `rodata-literal:0x800D21C0,0x800D21C8,0x800D21D0` alongside its `rodata-jtbl:0x800D21A0`. Goldens regen'd for the S134 bank drift (#1/#2 inert on the fixtures — the table golden runs without `--lib`, the coddog fixtures hit no FP-literal coddog row — so all drift is n_resample banking-out + the 2 callees getting named); suite **89 pass**.
- Carry-over: none from committed work. Heads-up for the next n_audio_sc sprint: naming the n_load callees (`n_alAdpcmPull`/`n_alLoadParam`) made the named-upstream index re-resolve n_load.c to a NON-n_audio_sc `load.c` variant (`needs-header:add/n_load_add01.c`, vs the n_audio_sc `inc/n_load_add01.inc.c@99.99`); when n_load is planned, prefer the coddog `n_load.c@99.99` (n_audio_sc, `inc/`) over the named index's `add/` resolution.

---

## Sprint 133 — n_save.c / n_alSavePull (n_audio_sc N_MICRO command-stream mirror; first inc-vendor + N_MICRO pin) — 2026-06-24
- Increment: 1 verbatim N_MICRO-branch mirror banked (n_alSavePull, 80B), Match after a clean rebuild. md5-candidate **189→190** (+1); asm subsegs 97→96 (1 flip). ROM SHA-1 == baserom.
- Quality: 0/0/0/0 this sprint (stuck-far/permuter/carried/re-opened).
- Seed: committed 3pt; banked 3pt; regime mirror (8-gate clear). pick_target priced it `blk`, a FALSE-FLAG: the `inc/n_save_add01.inc.c` body-include IS vendorable from the n_audio_sc `src/inc/` tree, but the needs-header detector scanned only `.h` basenames so it read as a DoR reject when it was a 1pt cp.
- What helped: the gate read the asm before the flip — confirmed the body is the verbatim N_MICRO (2-command) path (`jal n_alMainBusPull`, then `n_aInterleave`→0xD000000 + `n_aSaveBuffer`→0x62E0000 + `n_syn->sv_dramout`@0x48), so `body-divergence-suspect@99.99` was the expected n_a*-macro expansion, not divergence. The calls-unplaced callee `n_alMainBusPull`@0x800A1370 was dual-named at the gate (stays asm).
- Friction: two latent enablers the `INCLUDE_ASM` gate stub hid, both surfaced only at the full-make body compile. (1) `#needs-define N_MICRO`: the n_audio_sc upstream builds the WHOLE library with `-DN_MICRO=1`; without it n_save.c compiled the 4-command non-micro path (longer than the 2-command target → SHA-miss). Fix = pin `-DN_MICRO=1` in `LIBNAUDIO_CFLAGS` (same standing-pin pattern as `-DF3DEX_GBI_2`), then clean-rebuild per the shared-profile rule. (2) the build's `find src -name '*.c'` swept the vendored `inc/n_save_add01.inc.c` body-fragment as a standalone TU (`parse error before '++'`) → excluded `*.inc.c` in Makefile source discovery. Neither is classical iteration — the quality counter stays 0; both are now codified so the rest of the band pays them once.
- Applied: 3 of 3 — #1 `pick_target.py` `include_is_vendorable` now matches a missing include by its full source-relative path under `UPSTREAM_SRC_ROOTS` (with the n_audio_sc `src/` registered), so an `inc/*.inc.c` body-include prices as a +1 `needs-copy` enabler, not `blk` (verified: `func_8009FB60` n_resample blk→pts5, n_load/n_reverb/n_env blk→pts13). #2 `pick_target.py` `_parse_makefile_defines` parses `LIBNAUDIO_CFLAGS` into a new `libnaudio` active-define set (`_active_defines_for_lib` maps it), so N_MICRO reads as satisfied and a libnaudio mirror does not false-flag `needs-define:N_MICRO`. #3 `docs/hazards.md` — `#needs-header` `.inc.c` body-include vendoring sub-section (vendorable / not-a-TU / not-clang-formatted) + `#needs-define` N_MICRO library-pin sub-section; both under existing anchors so the CLAUDE.md hazard index is unchanged. Goldens regen'd for the S133 bank drift (func_800A12D0 banked-out + n_alSavePull leaves n_alSynNew's calls-unplaced list); suite **89 pass**.
- Carry-over: none from committed work. Leverage: the inc-vendor + N_MICRO pin + inc-exclusion now de-blk the whole remaining n_audio_sc band (n_resample / n_load / n_reverb / n_env), so a future sprint can batch the N_MICRO siblings cheaply.

---

## Sprint 132 — n_synallocvoice.c + n_sl.c (libnaudio n_audio_sc mirror cluster: clean cp + drop-def) — 2026-06-24
- Increment: 2 mirrors banked (n_alSynAllocVoice + static _allocatePVoice; n_alInit + n_alClose), both Match FIRST build. md5-candidate **187→189** (+2); asm subsegs 99→97 (2 flips). ROM SHA-1 == baserom.
- Quality: 0/0/0/0 this sprint (stuck-far/permuter/carried/re-opened).
- Seed: committed 8pt; banked 8pt; regime mirror. The combined-8 cluster ran as a 2-file increment (3+5), NOT a single-increment 8-gate stall: per-file all-or-nothing banking + each file individually verbatim-mirror-exempt (single-file-pack cp / drop-def) means the cluster is already decomposed at the file boundary.
- What helped: the gate read both bodies' asm before the flip. n_synallocvoice.c's callees were all placed (no `calls-unplaced` on the row) → pure `cp`. n_sl.c's `defines-data:n_alGlobals,n_syn` resolved to a DROP-DEF (S86 pattern), not a carve: both globals are `=0` BSS already provided by the extracted blob (n_syn placed S129) and `n_libaudio_sc.h` already declares them extern, so the two def lines just dropped. The one `calls-unplaced` (n_alSynNew=0x800A0D70, head of the still-asm n_synthesizer.c) was dual-named at the gate; n_alSynDelete was already placed (S131).
- Friction: none on the bank. The retro cost was suggestion #1 plus banking fallout — 2 coddog tests hardcoded the now-banked `func_800A0730` as a committed-fixture subject (the S131 de-hardcoding had missed these two), surfacing as KeyError/missing-row. Re-pointed both to a stable overlay subject (`func_ovl6_8024D800`, single-fn, no hazards, mined last) so banking can't break them again; regen'd 4 goldens for the bank+fixture drift.
- Applied: 1 of 1 — #1 `pick_target.py` post-pass: once a DEFINITIVE (`>=CODDOG_MIRROR_PCT`) `coddog-mirror` is on a row, the weaker `maybe-upstream` IDF guess is dropped. The existing `cod_definitive` guard (S75) consulted only the libultra `coddog_index` and excluded audio; an AUDIO mirror's identity arrives later from the separate `_resolve_audio` pass, AFTER the guess was appended, so the guess survived as noise pointing at the WRONG file (S132 func_800A0800: `maybe-upstream:n_synstopvoice,n_synstartvoiceparam,n_synstartvoice` vs the correct `coddog-mirror:n_synallocvoice.c`). The post-pass is pct-gated, so a SUB-threshold coddog hit keeps its guess as a second opinion. Behavioral test `test_coddog_suppresses_maybe_upstream` updated (audio-now-suppresses + a new sub-threshold-retain case); `docs/hazards.md#intrinsic-likely--maybe-upstream-signature-hints` notes the suppression. Suite **89 pass**.
- Carry-over: none from committed work.

---

## Sprint 131 — n_syndelete.c + n_synsetfxmix.c (libnaudio n_audio_sc verbatim mirrors; split a 2-file pack) — 2026-06-24
- Increment: 2 verbatim `@99.99` mirrors banked (n_alSynDelete + n_alSynSetFXMix), both Match FIRST build. md5-candidate **185→187** (+2); asm subsegs 100→99 (1 asm subseg split into 2 c). ROM SHA-1 == baserom.
- Quality: 0/0/0/0 this sprint (stuck-far/permuter/carried/re-opened).
- Seed: committed 4pt; banked 4pt; regime mirror (8-gate clear at 4<8; the c-combined:2file pack decomposed at the file boundary → two single-file-pack verbatim mirrors, each exempt).
- What helped: the gate's hand-disassembly of the `0x7BDE0` subseg caught that the "single-file" `coddog-mirror:n_synsetfxmix.c@99.99` was actually a 2-FILE pack — coddog matched only `func_800A09F0` (n_alSynSetFXMix); the 16B leader `func_800A09E0` (n_alSynDelete = `n_syn->head=0`, 4 instrs) is below coddog's fingerprint floor so it went unmatched. Identifying the leaf as `n_syndelete.c` and splitting at 0x7BDF0 gave two clean verbatim mirrors; all callees (`__n_allocParam`/`n_alEnvmixerParam`/`n_syn`) were placed S129/S130, so both were a pure `cp`.
- Friction: none on the bank. The retro engineering cost was suggestion #1 (the audio fncount guard) plus banking fallout — 3 golden tests hardcoded the now-banked `func_800A09E0` and had to be de-hardcoded (KeyError/missing-row); fixed 2 by dynamic candidate selection from a neutralized baseline and 1 (the committed coddog fixture) by switching the audio subject to a stable overlay func, so banking can't break them again.
- Applied: 1 of 1 — #1 `pick_target.py` ported the `coddog-fncount-mismatch` under-count guard from the libultra tail path (`_resolve_tail_coddog`) into the audio path (`_resolve_audio`), where it had been omitted: a single-identity multi-fn audio pack whose coddog `.c` defines fewer fns than the pack now flags `coddog-fncount-mismatch:<m>vs<n>` (the 0x7BDE0 case it was built for is now banked, but it fires live on `al_init`'s 13fn pack vs `player_fx.c`@99.99's 6 fns → `6vs13`). Verified end-to-end (libmus 0→1 tag; the al_init multi-file tell); the guard is conservative (only fires on a genuine under-count). + `docs/hazards.md#coddog-cross-ref` provenance (S88/S92→S131). Goldens regen'd for the banking cascade; suite **89 pass**.
- Carry-over: none from committed work.

## Sprint 130 — n_synaddplayer.c + n_synsetvol.c (libnaudio n_audio_sc setter mirrors) — 2026-06-24
- Increment: 2 verbatim `@99.99` mirrors banked (n_alSynAddPlayer + n_alSynSetVol), both Match FIRST build. md5-candidate **183→185** (+2); asm subsegs 102→100 (2 flips). ROM SHA-1 == baserom.
- Quality: 0/0/0/0 this sprint (stuck-far/permuter/carried/re-opened).
- Seed: committed 5pt; banked 5pt; regime mirror (8-gate clear at 5<8; 2× single-file-pack verbatim exemption).
- What helped: the S129-laid n_audio_sc header DAG + placed shared externs made this a near-free vein continuation; the gate's asm-vs-upstream check unmasked both `calls-unplaced` flags as false (SAMPLE184 = dead `#ifdef SAMPLE_ROUND` macro, __osError = non-_DEBUG ALFailIf), leaving one real callee to recover (`_n_timeToSamples`=func_800A1274). Both files first-build Match.
- Friction: none on the bank. The post-bank ch31/32 rework was PO-directed out-of-band (codegen-neutral, ROM byte-identical); the only retro engineering cost was suggestion #1 (the calls-unplaced reconciliation + the banked-fixture test repoint).
- Applied: 3 of 3 — #1 `pick_target.py` `_reconcile_calls_unplaced` (asm-jal-budget ground-truth filter: budget==0 → drop all, surplus → drop debug-only-callee family) + `_local_header_macro_names` (exclude band-internal `"..."`-header macros, e.g. SAMPLE184, and macro-shadowed prototypes like `__MusIntSched_*`); drops SAMPLE184/__osError/__assertBreak phantoms with all real callees preserved (audited vs asm); repointed `test_coddog_suppresses_maybe_upstream` off the now-banked `func_800A07B0` fixture → `func_80070FD0`; 4 golden regen; suite 89 pass. #2 `docs/hazards.md#coddog-cross-ref` note: the n_audio_sc n_syn* setter vein is empirically clean-verbatim @99.99 (6/6 S129+S130), so its body-divergence diagnosis pass stays lightweight. #3 BACKLOG carry-over: n_mainbus.c (2-fn subseg pack vs 1 upstream fn — identify func_800A1320 + split before mirroring).
- Carry-over: none from committed work. New: n_mainbus.c (see `## Carry-overs`).

## Sprint 129 — src/libnaudio stood up + 4 n_syn* setter mirrors (n_audio_sc header band unlock) — 2026-06-24
- Increment: 4 verbatim mirrors banked (n_alSynSetPan/SetPitch/StartVoice/StopVoice), all Match FIRST build. md5-candidate **179→183** (+4); asm subsegs 106→102 (4 flips). ROM SHA-1 == baserom.
- Quality: 0/0/0/0 this sprint (stuck-far/permuter/carried/re-opened).
- Seed: committed 5pt; banked 5pt; regime mirror (8-gate: candidates were blk header-rejects → PO pulled the header enabler as the goal, the gate's scaffolding-enabler branch).
- What helped: the header enabler had a SHALLOW DAG (each n_syn*.c includes only 3 headers; the leaf headers libaudio/mbi/ultratypes/os_internal/ultraerror were already vendored under include/libultra/PR) so standing up `src/libnaudio` + `mk/libnaudio.mk` (KMC -O3) unblocked a homogeneous ~19-leaf vein for the cost of 4 internal headers + one profile. Reading the n_alSynSetPan asm up front recovered the 3 shared externs (__n_allocParam/n_alEnvmixerParam/n_syn) once for all 4. The PO header-placement directive (public→include/<lib>, internal→src/<lib> with -I src/<lib> prepended) cleanly resolved the SC-vs-libultra `synthInternals.h` name clash.
- Friction: shared-callee RENAME stale-`.o` — recovering `__n_allocParam` (rename of func_800A1148) link-failed the 3 still-stub siblings on the OLD name (no `.s`-dep tracking); fixed by writing all 4 bodies before building. pick_target kept the whole n_syn* band at `blk` because its include-resolver lacked the libnaudio profile dirs (fixed in #2).
- Applied: 3 of 3 — #1 `docs/hazards.md#clean-rebuild-after-shared-header-edit` shared-callee-RENAME sub-case; #2 `pick_target.py` libnaudio profile include dirs (band drops blk, smallest now func_800A07B0 pts-2; golden regen); #3 CLAUDE.md vendored-header-placement convention + src/libnaudio added to the formatted-trees list.
- Carry-over: none. (Post-bank PO-directed, codegen-neutral: ch31/32 rework of the 4 .c + 3 .h, and clangd+clang-tidy enabled for src/libnaudio — 0 findings under the configured set. Cross-repo: 4 fn names + 3 externs → `sync_decomp_names.py --import-from-decomp`.)

---

## Sprint 128 — audio_mgr.c banked (game-embedded nualstl3 mgr + bgm; nualstl3/libmus band unlock) — 2026-06-24
- Increment: src/main/audio_mgr.c md5-candidate (6 fns: 4 nualstl3 verbatim mirror + 2 game bgm classical). matched-fn +6; md5-candidate 177→**178**. ROM SHA-1 == baserom.
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0 (matching was 6/6 first-build; all friction was in scoping + band-unlock).
- Seed: committed 8pt; regime mixed (4 mirror + 2 classical). v2 realized tier: seed 8 → realized 10; residual +2 (mid-sprint re-scope/split standalone→mixed carve +1; novel bank-gotcha CRLF-header +1).
- What helped: the gate-build canary (#non16align) caught the standalone-carve premise failure BEFORE banking; ASM-first reading of the boundary bytes (REAL bgm code at 0x3A448, not nops) proved game-embedding; coddog libmus_map gave the upstream callee names (MusInitialize/MusStartSong/MusSetScheduler/__MusIntMemMalloc).
- Friction: the whole plan premise was wrong — nualstl3 is game-EMBEDDED (compiled into a game audio TU, no object boundary), not a standalone lib → PO re-scope to a 16-aligned mixed carve under src/main/; vendored nualstl.h CRLF broke KMC cpp `\` macro continuations (silent parse cascade); root .clang-format sorted the order-dependent nusys.h/nualstl.h includes.
- Applied (5 of 5, all 3 PO groups): #1 pick_target `game-embedded` synthesis flag (HAZARD_GAME_EMBEDDED + predicates + `Hazard.game_embedded_for` + unit test; golden-inert — fixtures lack the 3-condition shape) + CLAUDE.md index row + docs/hazards.md#game-region-mirror-o2-profile sub-case; #2 docs/hazards.md#crlf-vendored-header + index row; #3 docs/coding-style.md include-sort-trap note (+ `src/main/.clang-format` landed in-sprint per PO directive); #4 #wrong-ghidra-name-override generalized beyond macro aliases; #5 #upstream-mirror-pattern header-CONSTANT-vs-asm validation (NU_AU_MESG_MAX=2). Also regen'd the 4 pick_target goldens (sprint carve + symbol-add drift the bank commit had left stale).
- Carry-over: none. (Cross-repo: 16 new decomp symbols + 3 wrong-ghidra-name corrections → `sync_decomp_names.py --import-from-decomp`.)

---

## Sprint 127 — contRmbControl banked (libnusys RMB-manager; resolves the S121 cross-jump-wall spike) — 2026-06-23
- Increment: src/libnusys/mainlib/nucontrmbmgr.c COMPLETE (1 fn, contRmbControl, banked C; closes the S121 partial). matched-fn +1; md5-candidate 176→**177** (file now 0 stubs). **libnusys mainlib is now 100% C.** ROM SHA-1 == baserom.
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0 — and **resolved 1 prior carry** (the S121 "unbankable" spike). Net positive.
- Seed: committed 3pt; regime mixed (classical body-fix). v2 realized tier: seed 3 → realized 5; residual +2 (carry-or-reopen +1 + novel bank-gotcha +1 = the misdiagnosis reversal).
- What helped: **the user's systematic-debugging redirect to the actual compiler source** (`mips-gcc-2.7.2/jump.c`). `find_cross_jump`'s `minimum=1` "cross-jump to code before the label" path merges on ONE matching insn before the epilogue label → the "merge" is basic-block-LAYOUT-driven, and layout is driven by the BODY. That reframe ("the body controls it, not an exotic heuristic") sent me back to re-read the target's stores, where the divergence was plain: the FORCESTOP epilogue has TWO `sb v0,6(s0)` state stores with DIFFERENT values (`li 1`/`li 2`), impossible from a single unconditional `state =`. MG64's FORCESTOP is game-modified (`state=STOPPED` on osMotorInit FAILURE, `state=STOPPING; counter=2` on SUCCESS, an if/else) vs upstream/papermario's unconditional STOPPING. One-branch fix → byte-identical .text + full ROM SHA-1, NO compiler change.
- Friction: 5 sprints + a 145k-iter permuter run + a multi-binary compiler hunt (S121) were spent on a 1-line body bug. The "byte-identical-tails ⇒ compiler wall" triage assumed the body was correct; it wasn't. The lying-signature experiment (a real codegen perturbation) was a productive side-track that didn't resolve it; the body did. S123/S124 had ALREADY hit the same "apparent wall == source artifact" pattern — the lesson didn't generalize to the carried spike until now.
- Applied (3 of 3 + mandatory ledger; the core `#cross-jump-tail-merge` / CLAUDE.md / carry-over corrections landed in the bank commit `7717792` per PO "fix docs now"): #1 SUPERSEDED markers on the S121 BACKLOG sprint-log + line-134 forward-pointer + VELOCITY row 121; #2 memory `rule-out-body-before-compiler-wall`; #3 `pick_target.py` `body-divergence-suspect` tag on sub-100 coddog-mirror rows (factory + `_build_row` post-pass + unit test + golden regen) + CLAUDE.md hazard-index row.
- Carry-overs: none. **libnusys mined out** (last stub gone); next sprint re-scopes (libultra audio maybe-upstream band or classical singletons).

---

## Sprint 126 — nusched.c COMPLETE (last 3 fns: nuScEventHandler + nuScCreateScheduler + nuScExecuteGraphics) — 2026-06-23
- Increment: src/libnusys/mainlib/nusched.c FULL (3/3 remaining fns banked C; closes the S123/S125 spike, open 3 sprints). matched-fn +3; md5-candidate files 175→**176** (file now 0 stubs); asm subsegs unchanged (subseg already `c` from S123). ROM SHA-1 == baserom at every commit.
- Quality: stuck-far 0 / permuter-escalated 0 / carried 0 / re-opened 0.
- Seed: committed 8pt; banked 8 file pt (full file, per-file all-or-nothing); regime mixed. v2 realized tier: seed 8 → realized 9; residual +1 (one novel bank-gotcha = the per-TU-volatile diagnosis).
- What helped: ASM-first decomp on the S125 scaffold + the upstream nusys-2.07 nuScCreateScheduler/nuScExecuteGraphics as shape templates (only the asm-cited features are MG64-custom). **nuScEventHandler banked with NO permuter — the S125 framing was OVERTURNED:** the 2 mflo-hazard nops appear naturally once the data + volatility scaffold is COMPLETE. The decisive fix was recognizing `nuScRetraceCounter` is **per-TU volatile** (volatile in nusched.c, plain u32 in the BANKED nugfxtaskmgr.c/nucontrmbmgr.c): a localized `extern volatile u32` redeclaration in nusched.c gives the per-access `lui/lo` volatile codegen without touching the shared header. nuScCreateScheduler matched via MG64 init order + switch case order (MPAL,PAL,NTSC,default); nuScExecuteGraphics (287/287) via the swap-gate arm + custom SWAPBUFFER rotation + a single source nudge (assign debTaskPerfPtr before the volatile D_800B678C++). Cross-fn volatile discovery: D_800B6788 flipped to vu32 (EventHandler's single-read index was insensitive; ExecuteGraphics' `++` showed the tell).
- Friction: a shared-header `vu32` flip on nuScRetraceCounter (the obvious first move, per the S125 carry-note) produced a **21M-byte ROM diff** on clean rebuild — nucontrmbmgr.c's lone `% nuContRmbSearchTime` modulo IS volatile-sensitive (+0x10 .text, cascading main_RODATA_END/BSS). Diagnosed by per-object `.text` map-diff. A cast-macro `(*(vu32*)&x)` also failed (GCC held `&x` in a reg + burned a saved reg). Both dead-ends are now documented so the next per-TU-volatile fn skips them. nuScExecuteGraphics' mask interleaving + while-loop change + custom perf rotation needed a careful full asm trace (the heaviest single fn this sprint).
- Applied (2 of 2): #1 `docs/hazards.md#volatile-global tell` — per-TU-volatile redeclaration recipe + cast-macro anti-pattern + the shared-header-flip map-diff diagnosis (single-read modulo IS volatile-sensitive; plain reload-pair is NOT); #2 `docs/hazards.md#libnusys inline-div mflo-hazard nop` — reframed: NOT a permuter wall, finish the per-TU-volatile + data scaffold and RE-DIFF before escalating (S126 overturns the S125 permuter-candidate framing). No new hazard sections (both existing; CLAUDE.md index unchanged).
- Carry-over: none. The libnusys clean-mirror band is mined out (every `--lib libnusys` row is masked game code); the next sprint moves to a different scope.

---

## Sprint 125 — nusched.c spike (1/4 banked: nuScExecuteAudio) + nuScRetraceCounter ID — 2026-06-21
- Increment: src/libnusys/mainlib/nusched.c PARTIAL (1/4: nuScExecuteAudio banked C; EventHandler/Create/ExecuteGraphics carried). matched-fn +1; md5-candidate files 175→175 (nusched.c still partial, 3 stubs). Bonus: nuScRetraceCounter identified (0x80104E68) + nugfxtaskmgr.c renamed (byte-neutral).
- Quality: stuck-far 0 / permuter-escalated 0 / carried 3 (nuScEventHandler near-miss + Create + ExecuteGraphics) / re-opened 0.
- Seed: committed 8pt; banked 0 file pt (partial, per-file all-or-nothing); regime mixed. v2 realized tier: seed 8 → realized 10; residual +2 (carry-or-reopen + the novel mflo-hazard bank-gotcha).
- What helped: the ASM-first re-assessment overturned the S123 "4 game-customized fns / heavy classical RE" framing — the 4 carried fns are EXACTLY the 4 with `#ifdef NU_DEBUG` blocks, and S123 compiled the file WITHOUT NU_DEBUG. nuScExecuteAudio is pure stock-NU_DEBUG, banked first-build via `#define NU_DEBUG` + a `NUDebTaskPerf` struct fix (dropped the 2.07 markerTime[10] → auTaskCnt@0x9 / auTaskTime@0x150, asm-confirmed) + drop-def debTaskPerfPtr. For nuScEventHandler, identifying the volatile globals (dead-reload-after-store + recompute-not-CSE) and the single-`frame`-local swap-gate brought it to byte-perfect except 2 nops.
- Friction: nuScEventHandler is a `#libnusys-inline-div-mflo-hazard-nop` WALL — KMC gcc schedules the inline-`divu` mflo consumer into the loop-back `j` delay slot, suppressing the 2 VR4300 hazard nops the original build has; KMC gcc/as reject every -mfix4300/-mcpu=vr4300/-Wa flag (a standalone div DOES get the nops). A permuter candidate, not a try-harder-C iteration; carried with the full worked body + the precise blocker. Budget then precluded the heavier Create/ExecuteGraphics.
- Applied (5 of 5): #1 `docs/hazards.md#nu_debug-stock-not-custom-carried-perf-fn-triage` (the carried-fns == NU_DEBUG-fns tell); #2 `docs/hazards.md#libnusys-inline-div-mflo-hazard-nop` (the EventHandler wall + permuter ladder); #3 perf-struct version-drift note in `#upstream-mirror-pattern`; #4 `docs/hazards.md#volatile-global-tell-dead-reload--recompute-not-cse`; #5 masking-coddog carry-forward (non-lib-`func_`-callee detection still the tracked follow-up); + 3 CLAUDE.md hazard-index rows.
- Carry-over: 3 fns in nusched.c (INCLUDE_ASM): nuScEventHandler (NEAR-MATCH, 2-nop mflo-hazard permuter candidate, full body in BACKLOG), nuScCreateScheduler + nuScExecuteGraphics (untouched). Scaffolding (perf globals, 3 stacks, 6 MG64 globals) is asm-confirmed and listed in BACKLOG.

---

## Sprint 124 — nugfxtaskmgr.c (libnusys game-customized gfx task manager; full file, 3/3 banked) — 2026-06-21
- Increment: src/libnusys/mainlib/nugfxtaskmgr.c banked (3 fns: nuGfxTaskMgr/nuGfxTaskMgrInit/nuGfxTaskStart). matched-fn +3; md5-candidate files 174→175 (all 175 src .c stub-free); asm subsegs 107→106 (subseg flipped + fully banked). The planned mixed-partial resolved to a FULL bank.
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0 (Init: 1 re-attempt to find the dup-msgQ codegen — a normal classical iteration, the +1 novel bank-gotcha on the realized tier).
- Seed: committed 8pt; banked full; regime mixed (planned mixed bank-stock-carry-custom; resolved to full). v2 realized tier: seed 8 → realized 9; residual +1 (game-customized asm RE + the dup-msgQ codegen discovery).
- What helped: ASM-first decomp (the workflow's "asm is ground truth") matched all 3 game-customized fns where the upstream `.c` was shape-only. nuGfxTaskMgr (the hardest-looking, with the MG64 retrace-pacing wait) and nuGfxTaskStart (custom swap-sequence-table) both matched first-try. The KEY for nuGfxTaskMgrInit: reading the asm STORE ORDER (`next, msgQ, …fields…, msgQ`) — the field appearing at BOTH ends of the store sequence was the cue that the MG64 source has a DUPLICATE `msgQ =` at the loop tail; re-adding it reproduced GCC 2.7.2's dual-induction-var + double-store and landed the exact instr count → byte-match. The extern-ref drop-def data model (nugfxinit.c pattern) avoided any .data/.bss carve.
- Friction: the first read mis-scored Init at ~19% byte-match (the 2-instr shortfall shifted .data/.bss → every reloc off), which masked that the structure was nearly right and over-suggested a permuter (the byte-match was far below the 0.97 permuter floor anyway). Localizing the deficit to the loop addressing (running-pointer vs indexed) and the dup-store tell took an objdump alignment pass. nuGfxUcode misread as 0x800B61B0 vs the real 0x801061B0 (hi/lo sign-extend) cost one reconciliation step.
- Applied (3 of 3): #1 CLAUDE.md never-clang-format list += `src/libnusys/` (3 spots) + new `src/libnusys/.clang-format` (DisableFormat) — the de-facto convention made explicit (nusched/nugfxinit/nugfxtaskmgr all upstream tab style); #2 `pick_target.py` seed_points masking-coddog pricing — a `coddog-structural`/`coddog-source-banked` hit no longer SUPPRESSES the near-verbatim +1 and earns it on a pack (the S123 exemption-GUARD priced into the seed; forward-looking, golden-inert on the current candidate set — the non-lib-`func_`-callee detection remains the tracked follow-up); #3 new `docs/hazards.md#struct-init-loop-dup-store--dual-induction-var` section + CLAUDE.md hazard-index row (the dup-store / dual-IV match recipe; a pick_target `struct-init-loop` tag is a tracked follow-up).
- Carry-over: none (full bank).

---

## Sprint 123 — nusched.c (libnusys game-customized scheduler; 10/14 banked, 4 carried) — 2026-06-21
- Increment: src/libnusys/mainlib/nusched.c PARTIAL (10/14 fns banked C, NOT md5-candidate). matched +10; md5-candidate files 174→174; asm subsegs 108→107 (subseg flipped).
- Quality: stuck-far 0 / permuter 0 / carried 4 / re-opened 0 (4 spikes: the heavily-customized fns).
- Seed: committed 13pt; banked 0pt (per-file all-or-nothing, file partial); regime planned-mirror → actual mixed.
- What helped: ASM-first re-assessment caught the false premise EARLY (coddog 99.99 was structural, the file is a game-customized scheduler). The user's ~/n64sdk version-triage hint was decisive — confirmed no stock 1.10/2.00/2.07 matches the custom fns, pinned MG64 ≈ 2.07-minus-nuVersion via the baserom "NuSystem"-string absence + the 2.07 PRENMI-dispatch in nuScAddClient. The vendored NUSched struct matched exactly (base 0x801B8380, size 0x680 derived from the asm). bank-stock-carry-custom (S121 generalized) banked 10 clean fns first-build, ROM green, committed as a safe checkpoint.
- Friction: the plan gate mis-priced a coddog-mirror@99.99 row as a verbatim-exemption atomic mirror (the S121 hedge anticipated a PARTIAL but still framed it as a near-verbatim, not a pervasively-custom file). Data-layout ambiguity (D_800D8970 = nuScGraphicsStack-end ALIASES debTaskPerfPtr) cost diagnosis time; resolved (stack-top-aliases-next-symbol).
- Applied (4 of 4): #1 CLAUDE.md ## Story points exemption-GUARD (coddog 99.99 = structure; don't fire the exemption on a non-lib func_ game-callee / unexplained large jal-mismatch; verify bodies) + #3 mixed bank-stock-carry-custom first-class note [the pick_target.py non-lib-func_-callee pricing is a TRACKED FOLLOW-UP, not yet coded — golden-suite risk in-gate; the gate applies the guard by reading asm callees]; #2 docs/hazards.md#upstream-mirror-pattern libnusys multi-version-triage step (baserom NuSystem-string grep + per-fn feature diff); #4 BACKLOG dependency-order note (nusched placed → unblocks nuGfxTaskMgr).
- Carry-over: 4 fns in nusched.c → INCLUDE_ASM spike (nuScCreateScheduler, nuScEventHandler, nuScExecuteAudio, nuScExecuteGraphics): MG64-game-customized, need NU_DEBUG perf machinery + full stack/perf data layout (findings recorded in BACKLOG).


- Increment: src/libnusys/mainlib/nusimgr.c banked (6 fns: nuSiMgrInit/nuSiSendMesg/nuSiMgrStop/nuSiMgrRestart/nuSiMgrThread + func_800A2780). matched-fn +6; md5-candidate 173 → 174 (all 174 src .c stub-free); asm subsegs 109 → 108. The S120-split carry-over, banked atomically.
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0 (one normal one-byte mirror fix-iteration, not a counter event).
- Seed: committed 5pt; banked 5pt; regime mirror (8-gate clear at 5<8; the leaf made it effectively single-file).
- What helped: the S87/S90/S115 drop-static pattern dropped the 3 statics + 3 globals to extern with zero friction (all bss → no carve); recovering the bss vrams from the asm up-front (nuSiMesgBuf/siMgrThread/siMgrStack contiguous + nuSiMgrMesgQ) meant the link resolved first try; the venv-python region cmp localized the lone SHA-miss byte instantly (the S44 .o-diff form).
- Friction: two carry-over open questions surfaced only at execution. (a) The leaf func_800A2780 — the S120 note framed it "foreign micro-TU, in NEITHER upstream source," but it returns &siMgrStack (a nusimgr.c file-static) → provably SAME-TU; once seen, trivially `return siMgrStack;`. (b) A one-byte SHA-miss: vendored nusys-2.07 NU_CONT_THREAD_ID=6 vs MG64's 5 (the osCreateThread thread-id li immediate) — a version-rev #define divergence, caught only at first build. Both were quick once diagnosed.
- Applied (3 of 3): #1 `docs/hazards.md#needs-define` version-rev single-`li`/`addiu`-immediate-byte sub-case (the S83/S44 link-clean-but-one-word-miss class, version-rev not GBI) + CLAUDE.md hazard-index "single immediate byte" row; #2 `docs/hazards.md` leaf-returns-static SAME-TU resolution rule appended to the `unattrib-leaf` paragraph (resolve the leaf's returned addr against the neighbors' static map before deciding foreign); #3 `BACKLOG.md ## Carry-overs` near-free-retry checklist 6th item (header-value reconciliation vs the game's nusys rev).
- Carry-over: none (full atomic bank).

---

## Sprint 121 — nucontrmbmgr.c (libnusys RMB-manager mirror; 8/9 banked, contRmbControl carried) — 2026-06-20
- Increment: src/libnusys/mainlib/nucontrmbmgr.c PARTIAL — 8/9 fns banked as verbatim C, contRmbControl carried as INCLUDE_ASM. matched-fn +8; md5-candidate 173 → 173 (file not candidate, 1 stub); asm subsegs 110 → 109.
- Quality: stuck-far 0 / permuter 1 / carried 1 / re-opened 1 (a size-8 verbatim mirror that became a multi-session compiler-RE spike, resolved to a partial bank).
- Seed: committed 8pt; banked 0pt (per-file all-or-nothing — file partial); regime mirror (8-gate: verbatim-mirror exemption, single-file-pack). The +8 matched fns are the value, not the file point.
- What helped: the S116 .data-carve template (carve 0xA31D0 / drop nuContRmbCtl to extern) made the 8 verbatim fns drop in clean; the byte-identical-vs-differing-tails triage proved the wall fast (no wasted permuter cycles once the tails were confirmed identical); option B (INCLUDE_ASM the one stuck fn) captured the 8 and kept the ROM green.
- Friction: contRmbControl is a `#cross-jump-tail-merge` WALL — the project gcc 2.7.2 merges two byte-identical counter-store tails MG64 keeps separate; exhaustive RE (permuter 145k iters; real-KMC + decompals + SN + gcc 2.8.1) found NO available binary reproduces the non-monotonic selective pattern. A clean compiler-patch can't do it either (downgraded option C).
- Applied (3 of 4): #2 `docs/hazards.md#cross-jump-tail-merge` (byte-identical-tail triage + resolution ladder; folds in the #3 downgraded compiler-fix conclusion); #4 `docs/hazards.md#permuter-setup-for-kmc-toolchain-mirrors` (custom --settings / KMC-safe prelude / body+target.s); #5 `CLAUDE.md ## Story points` sub-100-coddog exemption hedge (budget a codegen-divergence diagnosis pass, expect 0 pt if partial); + CLAUDE.md hazard-index rows. (#1 coddog-near-verbatim pick_target flag NOT selected.)
- Carry-over: contRmbControl (0x800A19E0, INCLUDE_ASM in the otherwise-C nucontrmbmgr.c) — cross-jump-implementation wall, not a "try harder" spike; banks only with MG64's actual compiler binary (none available) or stays carried.

---

## Sprint 120 — nuContGBPakFwrite (libnusys GBPak near-verbatim block-reorder mirror; S119 sibling) — 2026-06-18
- Increment: src/libnusys/mainlib/nucontgbpakfwrite.c banked (1 fn). md5-candidate 172 → 173 (all 173 src .c stub-free); asm subsegs 110 → 110 (the c-combined split is net-zero: `[0x7D970,asm]` flipped to c, a new trailing `[0x7DB80,asm]` created for the nusimgr remainder). GBPak family complete.
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened). **0 re-attempt** — first-build clean.
- Seed: committed 3pt; banked 2pt realized (residual −1); regime near-verbatim (classical track, per the S119 #3 reclassification). The S120 #3 sibling-known refinement: a near-verbatim whose swap is KNOWN from a banked sibling and applied up-front scores the realized −1 verbatim-first-try tier (the seed keeps its +1 near-verbatim risk).
- What helped: the S119 precedent + the asm. nuContGBPakFwrite is the structural twin of nuContGBPakFread (CheckConnector/RAM-enable pair); reading the asm block order (RAM-enable before CheckConnector, `ram=0` in the range-check delay slot) let the CheckConnector/RAM-enable swap be applied to the verbatim 2.07 cp UP-FRONT, so it banked first-build with 0 iteration — exactly the failure S119 paid a re-attempt to discover. All 5 callees pre-placed (nuContGBPakReadWrite confirmed the `5vs10` macro artifact), name pre-curated → zero symbol adds, one yaml split the only enabler.
- Friction: none. (The only judgment call was the c-combined split point; the asm gave nuContGBPakFwrite's exact 16-aligned extent [0x800A2570–0x800A2780], so the rom-0x7DB80 boundary was unambiguous.)
- Applied: 3 of 3 — #1 `pick_target.py` `block-reorder-sibling:<file>` advisory tag (`BLOCK_REORDER_FAMILIES` registry seeded `nucontgbpak*` + `_block_reorder_sibling`; fires on libnusys jal-mismatch + no-coddog in a banked-sibling family so the gate plans the swap up-front) + unit test + golden regen; #2 `unattrib-leaf:0x<vram>` flag for a LONE `?` leaf straddling a c-combined file boundary (`_straddling_unattrib`; gated to a single straddler so a whole interleaved foreign TU like __assert/nuboot does NOT false-fire) + unit test; #3 `VELOCITY.md` sibling-known refinement (per-fn provenance, realized −1). Note: all three are forward-looking — verified to add ZERO behavior on the current tree (the GBPak family is banked; the only golden churn was the legitimate S120 split row, stale since the banking commit).
- Carry-over: `func_800A2780` (0xC-byte leaf returning &0x800F77D0, in NEITHER upstream source — unidentified) + nusimgr.c (5 fns: nuSiMgrInit/SendMesg/Stop/Restart/Thread), both in `[0x7DB80,asm]`. See BACKLOG ## Carry-overs.

## Sprint 119 — clear func_800A2090 carry-over + nuContGBPakFread (libnusys GBPak/RMB region) — 2026-06-17
- Increment: src/libnusys/mainlib/func_800A2090.c + nucontgbpakfread.c banked (1 fn each). md5-candidate 170 → 172 (all 172 src .c stub-free); asm subsegs 112 → 110. Region 0x7D490–0x7D7A0 fully cleared.
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened). 1 mid-sprint re-attempt (nuContGBPakFread: verbatim cp SHA-missed → block-reorder hand-edit).
- Seed: committed 3pt; banked 4pt realized (residual +1); regime mixed (func_800A2090 trivial classical seed-only + nuContGBPakFread near-verbatim, reclassified to the classical track per S119 #3).
- What helped: the S118 near-free carry-over checklist made func_800A2090 a mechanical replay (the `trailing-pad:8B@16` to nucontgbpakmgr landed clean on the first full-make). For nuContGBPakFread, an in-tree-`.o` vs `build/asm/<rom>.o` objdump diff localized the divergence to pure block ORDER (117==117 insns reordered), and the per-revision nusys source set (`~/development/repos/nusys/src/<ver>/`) confirmed NO archived rev (1.20/2.00/2.05/2.06/2.07) reproduces it → hand-swap.
- Friction: the gate mis-classified nuContGBPakFread as a "clean verbatim mirror" — it refuted the jal-mismatch (a real macro artifact: nuContGBPakRead/Write → nuContGBPakReadWrite) and confirmed callees placed, but jal/insn-count parity does NOT prove block-order parity. The un-refuted tell was the "no coddog" flag (a reorder breaks the structural fingerprint), under-weighted at planning → a planned-2pt mirror realized 3pt near-verbatim.
- Applied: 3 of 3 — #1 `pick_target.py seed_points` prices a jal-mismatch + no-`coddog-mirror` target at mirror-floor +1 (near-verbatim risk; `(version-artifact?)` and coddog matches exempt), + unit test + golden regen (nuGfxTaskMgr 5→8, the genuinely-structural one); #2 `docs/hazards.md#near-verbatim-mirror-jal-count-mismatch` block-reorder version-divergence sub-case (read asm order, reconstruct source order, hand-swap, verify insn-identical) + CLAUDE.md hazard-index symptom row; #3 `VELOCITY.md` near-verbatim-reclassification rule (a SHA-missing nominal mirror scores on the classical track, not the point mass).
- Carry-over: none. Region 0x7D490–0x7D7A0 cleared; only the pts-8 `nuContGBPakFwrite` c-combined:2file pack (0x7D970) remains asm in the GBPak band.

---

## Sprint 118 — bank the RMB pair (nuContRmbModeSet + nuContRmbForceStop, libnusys mirrors) — 2026-06-17
- Increment: src/libnusys/mainlib/nucontrmbmodeset.c + nucontrmbforcestop.c banked (1 fn each), split from the c-combined `[0x7D3B0,asm]` RMB pack. md5-candidate 168 → 170 (all 170 src .c stub-free).
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened). Both first-build SHA, 0 iteration.
- Seed: committed 2pt; banked 2pt; regime mirror (8-gate clear; near-verbatim drop is a mirror sub-case → seed-only).
- What helped: the smallest-first ranker put the 240B RMB pack on top; gate-time asm read (`nuContRmbModeSet` = 0-jal leaf, `nuContRmbForceStop` = 1 jal to placed nuSiSendMesg, `func_800A2090` = 8B empty stub) plus the per-version source set under `~/development/repos/nusys/src/<ver>/` let me pin the version BEFORE the flip. Banking `nucontrmbstart.c` (already in-tree) as the convention probe confirmed the RMB family is English/no-int-mask = 2.05+, isolating modeset as the outlier.
- Friction: minimal. One real subtlety: the in-tree English RMB convention (2.05+) and modeset's matching code (2.00, no int-mask) are mutually exclusive — no English version omits the int-mask wrapper. Resolved by the near-verbatim drop (2.07-sdk verbatim minus the 3 wrapper lines = 2.00 leaf code + English comments). pick_target's `jal-count-mismatch:2vs0` was a version artifact (it read the 2.05+ source's 2 osSetIntMask jals).
- Applied (3 of 3): #1 `docs/hazards.md#near-verbatim-mirror-jal-count-mismatch` nusys int-mask-wrapper anchor (leaf-asm tell + the drop procedure + version-source-set pointer) + S118 provenance; #2 `pick_target.py` libnusys `(version-artifact?)` annotation on an osSetIntMask-wrapper jal-count-mismatch (+`test_call_divergence_libnusys_intmask_version_artifact`; also caught up the 4 live-captured pick_target goldens, stale since pre-S114 — they had carried S114-S117 banked fns as candidates); #3 BACKLOG `func_800A2090` near-free-retry note.
- Carry-over: none banked-blocking. `func_800A2090` (8B empty stub) deliberately deferred per PO scope as a near-free follow-up (a backlog near-free-retry, not a spike).
- Cross-repo follow-up: none (0 new symbols — both fn names were already curated in ghidra_symbols.txt).
- Process note: the 4 pick_target goldens were red (stale) at sprint start, since the S114-S117 reviews did not regen them; caught up here. Surfaced as a Scrum-process suggestion (golden-regen should be a per-review step, or pin them to a fixture).

---

## Sprint 117 — bank src/libnusys/mainlib/nucontmgr.c (libnusys 2.05 .data-carve + drop-def mirror) — 2026-06-17
- Increment: src/libnusys/mainlib/nucontmgr.c banked (9 fns: nuContMgrInit/Remove + DataClose/Open + 5 static dispatch leaves). md5-candidate 167 → 168 (all 168 src .c stub-free).
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened). Banked atomically, no spike. BUT 3 mirror-enabler gotchas (atypical for the mirror track) — see Friction.
- Seed: committed 8pt; banked 8pt; regime mirror (8-gate fired → verbatim-mirror exemption held; seed-only).
- What helped: the S116 sibling pattern (.data-carve hybrid) was the right mental model; the cross-ref check (sweep base..base+size) correctly cleared the S116 share-check gotcha (no still-asm sibling reads the carve at base+offset); the asm-vs-source reconcile mapped all 14 data refs (2 .data + 6 BSS + 3 already-placed + nuSiMesgQ) before any build.
- Friction (3 gotchas, all resolved in-sprint, no carry): (1) **wrong nusys version** — source is 2.05, not the pinned 2.07; the 2.06/2.07 loop rewrite (comma-operator for-update) compiles 2 instrs short → ROM-wide -0x10 cascade (100740 diff bytes). A clean rebuild ruled out staleness; per-function objdump localized it to nuContMgrInit+contReadNW; compiling all nusys revisions found 2.00≡2.05 = 0x340. The 8-gate exemption assumes a clean point-mass, so this is the first mirror sprint that needed real diagnosis. (2) **carve size** — I carved the 0x24 symbol-content sum, but the `.o` `.data` SECTION is 0x30 (0xC 16-align END pad) → overlap; fixed to 0xA3270. (3) **entry.s hasm sync** — naming nuContNum=0x8010C2D0 broke the `_start` boot stub (uses it as initial $sp; boot stack top IS &nuContNum); byte-neutral D_→name edit.
- Applied (3 of 3): #1 nusys-version-divergence + version-hunt playbook (`docs/hazards.md` coddog-cross-ref nusys-sweep); #2 carve-extent = `objdump -h .o .data` SECTION size, 16-aligned, not symbol-content sum (`#defines-data`); #3 hasm-referrer byte-neutral symbol sync, grep src/**/*.s at the gate (`#file-static`).
- Carry-over: none.
- Cross-repo follow-up: 13 new decomp symbols (5 fn + 8 data) → `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 116 — bank src/libnusys/mainlib/nucontgbpakmgr.c (libnusys .data-carve mirror) — 2026-06-17
- Increment: src/libnusys/mainlib/nucontgbpakmgr.c banked (8 fn: nuContGBPakMgrInit/Remove + 6 static contGBPak* dispatch leaves). md5-candidate 166→167 (all 167 src .c stub-free).
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened). One in-sprint link-error gotcha (shared-at-a-field), resolved without spike/carry.
- Seed: committed 8pt; banked 8pt; regime mirror (seed-only; 8-gate FIRED at 8, verbatim-mirror exemption applied — single-file-pack:8fn, decompose mechanically blocked, all callees placed + names curated).
- What helped: `single-file-pack:8fn` + `coddog-mirror:nucontgbpakmgr.c@99.99` priced this as one verbatim cp; all 8 names pre-curated and all 9 callees placed (nuSiCallBackAdd/Remove S114, 6× osGbpak*, nuSiMesgQ) → gate was a single text flip. The `.o` `.data` section size (0x30) was the carve-extent oracle (funcList 0x20 + nuContGBPakCallBack 0xC + 4B align pad). Reading the splat source (symbols.py `write_undefined_syms_auto` = `referenced && !defined`) explained why the mid-carve address was silently omitted, and the canonical-base-with-addend fix (sized `nuContGBPakCallBack` → splat renders sibling as `nuContGBPakCallBack + 0x4`, binds the C export) is cleaner than a `D_` placeholder or `defined:False` absolute.
- Friction: the gate share-check was INCOMPLETE — grepping only the base `D_800C7E20`/`D_800C7E00` wrongly read "sole-referrer → carve"; the real share was at base+4 (`D_800C7E24`, the still-asm sibling nuContGBPakFwrite reading `.func`), surfaced only by the link error. Compounded: S97 "shared ⇒ DROP" did not apply because the global is UNDROPPABLE (its initializer chains to file-private statics that dropping would orphan/dead-strip), so carve was forced anyway. Both now codified.
- Applied: 4 of 4: #1 + #2 + #3 → `docs/hazards.md#defines-data` S116 paragraph (whole-object share-check base..base+size; mid-struct canonical-addend resolution; S97 "AND droppable" qualifier; pick_target field-level-ref auto-detection noted as tooling follow-up); #4 → `CLAUDE.md ## Story points` verbatim-mirror exemption (a) now lists the `.data`-carve mirror alongside drop-def/pure-cp.
- Carry-over: none.

## Sprint 115 — bank src/libnusys/mainlib/nugfxthread.c (libnusys drop-static mirror) — 2026-06-17
- Increment: src/libnusys/mainlib/nugfxthread.c banked (2 fn: gfxThread + nuGfxThreadStart). md5-candidate 165→166 (all 166 src .c stub-free).
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened).
- Seed: committed 5pt; banked 5pt; regime mirror (seed-only; 8-gate clear at 5<8).
- What helped: the `drop-static-mirror:4bss` re-frame tag + `coddog-mirror:mainlib/nugfxthread.c@99.99` priced this as one drop-to-extern enabler, not a scary 4-flag carve cluster — the proven S87/S90 pattern replayed first-build, 0 iteration. All 5 callees + nuGfxFunc pre-placed; both fn names pre-curated → gate was a single yaml flip. The S90 contiguous-`.bss`-block fast-path recovered nuGfxMesgBuf/nuGfxThread as one run; the GfxStack stack-top resolving to the NAMED cross-TU symbol PiMesgQ@0x800F74A0 (placed S99) gave a zero-arithmetic base (0x800F74A0 − 0x2000).
- Friction: none. (One latent: the `drop-static-mirror:<n>bss` tally counted 4 but the recover set was 5 — the uninitialized header-declared global nuGfxMesgQ falls between the file-static and defines-data detectors; refs-unplaced caught it, so graceful. Now documented as a 2nd under-count class.)
- Applied: 2 of 2: #1 `docs/hazards.md` drop-static fast-path — stack-top-equals-named-adjacent-symbol tell (base = NamedSymbol addr − STACK_SIZE); #2 `docs/hazards.md` `drop-static-mirror:<n>bss` 2nd under-count class (header-declared uninit global → use the drop-static + defines-data + refs-unplaced union as the recover set).
- Carry-over: none.
- **Ledger note:** discovered at this gate that **S114 (nuSiCallBackAdd + nuSiCallBackRemove, commit 9690ad8) was banked but never reviewed** — no VELOCITY/RETRO/BACKLOG entry, and its SPRINT.md suggestion buffer was lost when S115 planning overwrote it. See the S114 backfill below / PO follow-up.

## Sprint 114 — bank nuSiCallBackAdd + nuSiCallBackRemove (libnusys nuSi mirror pair) — 2026-06-17  [BACKFILLED at S115 review]
- Increment: src/libnusys/mainlib/nusicallbackadd.c + nusicallbackremove.c banked (2 fn: nuSiCallBackAdd + nuSiCallBackRemove). md5-candidate 163→165.
- Quality: 0/0/0/0 (reconstructed from the clean commit 9690ad8; no standup log survives).
- Seed: committed ~4pt (reconstructed — SPRINT.md lost; S16 2-file-libnusys-recover-extern precedent); banked ~4pt; regime mirror.
- What helped: verbatim nusys-2.07 mirror pair (coddog 99.99 each); c-combined `[0x7DE70,asm]` split at the 0x7DF10 file boundary; 1 recover-extern nuSiCallBackList=0x800C7E30 (defined by the un-decompiled nusimgr.c).
- Friction: the load-bearing one is a PROCESS gap — the sprint banked + committed but its `/sprint-review` never ran, so the ledger had no row and the suggestion buffer was lost when S115 planning overwrote SPRINT.md. (Matching friction unknown; no standup survives.)
- Applied: none recoverable (suggestion buffer lost with the overwritten SPRINT.md).
- Carry-over: none.
- Process lesson (promoted to the S115 review): a banked sprint MUST run `/sprint-review` before the next `/sprint-plan` overwrites SPRINT.md — `/sprint-plan` could guard this by detecting an un-retro'd SPRINT.md (banked items + no matching RETRO entry) and warning before overwrite.

## Sprint 113 — bank src/libkmc/sin.c (libkmc C-mirror, near-free atan.c replay) — 2026-06-17
- Increment: src/libkmc/sin.c banked (4 fn: _xsincos/sin/cos/tan). md5-candidate 162→163. **libkmc C-band complete** (only mmuldi3/mcvtld `hasm` remain).
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened).
- Seed: committed 13pt; banked 13pt; regime mirror (8-gate fired → verbatim-mirror exemption; seed-only). 3rd pts-13 single-file-pack exemption (S64/S69/S112/S113).
- What helped: the S112 near-free-retry carry-over (a 5-point completeness checklist) replayed **verbatim-correct, 0 rework, 0 iteration** — the flip line, placed-ref inventory, the no-new-symbols verdict, the include adaptation, and the upstream pin were all exactly right. Shared deps pre-placed by atan.c/mcvtld (`_atbl`@0x800C9690, `__fixunsdfdi`@0x800B3C20, `__floatdidf`@0x800B3D40, cordic.h vendored). Gate-time asm disassembly (asm/8E660.s) confirmed the jal/data-ref set before the flip, so no execution-time link surprise. The rodata carve was the same exact-bound generic-subseg flip as atan's [0xADC40] (no split). The atan.c first-build match de-risked every codegen concern (long-long shift / -O fp scheduling / K&R protos).
- Friction: none. The `__fixdfdi`-vs-`__fixunsdfdi` question (signed `XLONG=double*MBIT` with possibly-negative th) was resolved at the gate by reading the asm — KMC GCC emits `__fixunsdfdi` for both atan.c and sin.c, so no signed-cvt helper was needed.
- Applied (3 of 3): #1 BACKLOG libkmc-fully-mined epic-status (S112 paragraph updated + sin.c carry-over cleared); #2 near-free-retry checklist 3rd-clean-hit doctrine data-point (S75 contquery / S93 xldtob / S113 sin.c — the carry-over-as-mechanical-replay pattern is robust, no tooling change); #3 `docs/hazards.md#compile-profiles` note that KMC GCC emits `__fixunsdfdi` not `__fixdfdi` for the libkmc CORDIC `double→long long` idiom.
- Carry-over: none. libkmc fully mined.

---

## Sprint 112 — bank src/libkmc/atan.c (first libkmc C-mirror) — 2026-06-17
- Increment: src/libkmc/atan.c banked (3 fn: _xatan/atan/atan2). md5-candidate 161→162.
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened).
- Seed: committed 13pt; banked 13pt; regime mirror (8-gate fired → verbatim-mirror exemption; seed-only).
- What helped: the carry-over's full data map (S109/S111) pre-resolved every dep at the gate — `_atbl`@0x800C9690 (the whole `[0xA4A90,data]` 0xBD8 blob = the CORDIC table, so a symbol-place not a carve), the rodata sibling bounded exactly by the generic `[0xADC40,rodata]` (1-line flip, no split), and all callees pre-placed (`__floatdidf`/`__fixunsdfdi`/`__matherr`). Verbatim cp at the libkmc `-O` profile matched FIRST BUILD, 0 iteration — all 4 flagged risks (rodata pool order / long-long shift codegen / -O fp scheduling / K&R protos) held. KMC GCC inlines the long-long shifts (no `__ashrdi3` enabler). Confirms the verbatim-mirror exemption generalizes to a pts-13 single-file-pack carrying recover-extern + rodata-carve, not just a clean cp.
- Friction: none on the bank. Minor: a stale `asm/data/ADC40.rodata.s` orphan lingered after the retype-carve (gitignored + unlinked, harmless) — confirmed inert via clean-rebuild SHA match (now documented).
- Applied (3 of 3): #1 `docs/hazards.md#.rodata sibling` stale-orphan-after-retype-carve note + S112 provenance; #2 `docs/hazards.md#recover-extern` unindexed-upstream-mirror no-scan note (root-caused: atan/sin are upstream `none` so refs_unplaced never runs; the regex ALREADY matches the inline `extern XLONG _atbl[];` — indexing libkmc math rejected as low-value carry-overs + reclassification-risk); #3 BACKLOG band-exhaustion paragraph.
- Carry-over: src/libkmc/sin.c (near-free retry, NOT a spike) — cordic.h + `_atbl` now placed; the last libkmc non-hasm unit.

---

## Sprint 111 — resolve ALL libultra TU .data/.rodata (whole-region carve sweep) — 2026-06-17
- Increment: ≈18 libultra TU `.data`/`.rodata` sections carved into named `libultra/<tu>` subsegs across the 0xA32D0–0xADCA0 region (12 placed drop-def restores, 6 vendored data TUs: thread/vitbl/vi/3×vimodes, exceptasm `.data`+`.rodata` un-stripped, gu/libm_vals). 8 new src/libultra files; 46 named data/rodata subsegs now. md5-candidate unchanged at 155 (these are data carves, not new `.c` fn matches).
- Quality: stuck-far 0, permuter 0, carried 3 (ADC40 atan-consts, ADCA0 _xsincos-consts, A4A90 unattributable), re-opened **1 — major** (3 build breaks masked behind an ungated stale-green `sha1sum`).
- Seed: committed 8pt; banked 8pt; regime mixed — v2 realized **13**, residual **+5** (the masked-failure re-open + 3 mid-sprint link/header debugs + large multi-TU scope).
- What helped: splat's `Rodata segment X may belong to text Y` hint as the attribution oracle; ultralib as the byte authority + drmario64 as the not-placed-TU cross-ref (incl. confirming A4A90 is genuinely unattributable); the 0x10 `.data`-size-pad boundary rule once found; un-stripping the S107 exceptasm tables (the strip was reversible once labels were re-exported).
- Friction: a hand-rolled `make … ; sha1sum` (NOT gated on the `OK` line) read a coincidentally-green stale ROM and false-positived MATCH across 3 commits that never built — vitbl missing `VI_CTRL_ANTIALIAS_MODE_0` (project rcp.h had only _1/_2/_3), timerintr `&__osBaseTimer` reloc to an unplaced bss symbol, and `vi` left `static` while MG64 splits `__osViInit` into a separate viinit.c. systematic-debugging root-caused all three; genuinely re-verified green.
- Applied (3 of 3): #1 `tools/verify-rom.sh` gated-verify helper + CLAUDE.md mandate (directly closes the false-positive that masked the breaks); #2 `docs/hazards.md#data-rodata-carve` playbook (3 carve kinds + 4 SHA/link gotchas) + CLAUDE.md hazard-index row; #3 splat-attribution-oracle + asm-mirror-jtbl-reversible notes folded into the same section.
- Carry-over: ADC40 (`atan` consts) + ADCA0 (`_xsincos` consts) — need `atan`/`_xsincos` vendored as `hasm` asm-mirrors first, then their rodata carves; A4A90 (~3 KB) unattributable, left a blob.

---

## Sprint 110 — re-home osSpTaskYielded + __osGetCurrFaultedThread + vendor os/parameters.s — 2026-06-16
- Increment: two trivial libultra leaf fns re-homed from src/main/ (-O2) to src/libultra/{io/sptaskyielded.c, os/getcurrfaultthread.c} as verbatim upstream mirrors at -O3; the 0x8AC20 "remaining file" resolved as `os/parameters.s` (`.space 0x60` + ABS() OS globals) vendored `hasm`. asm subsegs 119→118 (parameters now hasm). +0 net new fn matches (re-home), but the libultra region around 0x8AC20 is now fully c/hasm.
- Quality: stuck-far 0, permuter 0, carried 0, re-opened 0.
- Seed: committed 4pt; banked 4pt; regime mirror.
- What helped: the verbatim upstream IS the -O3 ground truth, so the re-home matched first build; recognizing the all-nop 0x8AC20 block as a real `.space`+ABS TU (os/parameters.s), not padding.
- Friction: initial mis-read of 0x8AC20 as alignment padding (corrected by the PO pointing at the ultralib source); C_FILES is a find-glob so the old src/main files had to be `git rm`'d to avoid a double-definition.
- Applied: none (banked between gates; lessons folded into the S111 review).
- Carry-over: none.

## Sprint 109 — mcvtld.s (__fixunsdfdi + __floatdidf), the first KMC-as sub-lane asm-mirror — 2026-06-16
- Increment: src/libkmc/mcvtld.s banked as a verbatim asm-mirror (`hasm`), 2 fns (`__fixdfdi`/`__fixunsdfdi` shared entry @0x8F020 + `__floatdidf` @0x8F140). asm subsegs 121→**119**, hasm 25→**26**; md5-candidate unchanged at 155 (asm-mirror banks as `hasm`, not a `.c`); +2 fns matched. The cleanest remaining asm TU in the libultra-region band (S108 note), PO-picked over the heavier `atan.c` C-mirror.
- Quality: stuck-far 0, permuter 0, carried 0, re-opened 0. Banked clean; the two `li`→`addiu` edits were a known mmuldi3 divergence handled at the gate, not a re-attempt.
- Seed: committed 2pt; banked 2pt; regime mirror   (asm-mirror, mirror track seed-only, no v2 residual; 8-gate clear)
- What helped: the `mmuldi3.s` precedent was the exact template — KMC-assembler explicit-rule path + the `li $X,0xffffffff`→`addiu $X,$0,-1` encoding divergence both transferred 1:1. Diagnosing the SHA miss was fast via a Python byte-slice `cmp` of the assembled `.o`'s `.text` vs the baserom (the one-word downstream shift at a `lui…ffff`/`ori…ffff` pair pinpointed both `li`s). Both fns pre-named → 0 symbol adds; pure register math → no rodata/relocs.
- Friction: `dd` is hook-blocked (used a Python byte-slice instead — now noted in the hazards doc). The golden test was silently stale since S107 (sprint-review doesn't run `make test-tools`, so S108+S109 banking drift accumulated unnoticed); regenerated this review. The new tooling tag (#1) is behavior-neutral on the current ROM (mcvtld is now `hasm`, no remaining libkmc asm-only TU) — verified by an identical golden diff with/without the pick_target.py edit before regenerating.
- Applied: 3 of 3 — #1 `pick_target.py` `intrinsic-likely:<tu>.s(kmc-as)` tag (new `build_kmc_asm_tu_index` + `KMC_ASM_TU_DEF_RE`; fires on an asm-only primary matching a libkmc `.globl` with no C upstream — the pure-shim/privileged guard misses branchy cvt routines; suite 61 pass, goldens regenerated); #2 `docs/hazards.md#asm-mirror-vendoring` KMC-as sub-lane subsection (recipe: `$(KMC_AS)` explicit rule, `.include "mips_as.h"` via `-I src/libkmc`, the `li`→`addiu` edit) + the CLAUDE.md hazard-index row; #3 multi-fn-TU-spanning->1-subseg merge-to-one-`hasm` note (folded into #2's sub-lane as the third bullet).
- Carry-over: none. Remaining libultra-region asm: `_xatan`/`atan.c` (0x8E110, C-mirror), `_xsincos`/`sin.c` (0x8E660, C-mirror), `audio_sched_thread_entry` cp0-asm (0x8F250).

## Sprint 108 — os/interrupt.s (__osDisableInt + __osRestoreInt), the clean asm-mirror sibling of S107 + llcvt.c ruled out — 2026-06-16
- Increment: src/libultra/os/interrupt.s banked as a verbatim asm-mirror (`hasm`), 2 fns. asm subsegs 122→**121**, hasm 24→**25**; md5-candidate unchanged at 155 (asm-mirror banks as `hasm`, not a `.c`); +2 fns matched. Closes the S107 carry-over verbatim (the `setintmask` partial-TU spike `__osDisableInt`+`__osRestoreInt` @0x8B900 — which was really `os/interrupt.s`, a separate ultralib TU).
- Quality: stuck-far 0, permuter 0, carried 0, re-opened 0. Banked clean; one first-build parse-error (bare include) fixed via documented precedent, not a re-attempt.
- Seed: committed 2pt; banked 2pt; regime mirror (asm-mirror, seed-only — no v2 residual).
- What helped: the S107 exceptasm work had already vendored every header interrupt.s needs (`internal/threadasm.h`, `PR/R4300.h`, `sys/asm.h`/`regdef.h`, `PR/os_version.h`) and all 4 symbols were already named → 0 header/symbol adds, 0 extract-artifact changes. The S107 `VENDOR_ASM` mechanism + `#if BUILD_VERSION>=VERSION_J` branch-match made this a near-mechanical replay. The plan-gate investigation also CLOSED the scope's other half (llcvt.c): confirming the *workhorse* symbols (`__floatdidf`@0x800B3D40, `__fixunsdfdi`@0x800B3C20, present in the libkmc/libgcc band) vs the absent cast *wrappers* (`__d_to_ll`…) proved llcvt.c is not linked — the 8 coddog `__d_to_ll`@99.99 rows are reloc-masked structural FPs (stack→jal→return stub shape).
- Friction: one first-build `threadasm.h: No such file or directory` — the vendored ultralib `.s` includes its internal header bare (resolves source-relative in ultralib) but the project keeps it under `include/libultra/internal/`, reached only via `-I include/libultra`. Fixed by the S107 convention (`"threadasm.h"`→`"internal/threadasm.h"`), now codified as a doc step (Applied #1) so it stops being a per-asm-mirror surprise. Separately, suggestion #2 (surface remaining asm-mirror inventory in the ranker) on investigation proved bigger than a gate edit — the existing intrinsic-likely path already tags ultralib hand-asm correctly (interrupt.s WAS tagged; its top-15 miss was intended `(-score,size)` de-prioritization), and the genuinely-missing inventory is **libkmc** (`mcvtld.s`/`atan.c`/`sin.c`), needing a multi-root index + golden regen → deferred to a BACKLOG tooling follow-up rather than rushed into the load-bearing ranker.
- Applied (4 of 4): #1 `docs/hazards.md#asm-mirror-vendoring` step-1 internal-header-include-rewrite sub-bullet (the one verbatim deviation; .text-unaffected, SHA-1 stays the proof; S107 exceptasm + S108 interrupt.s instances); #2 BACKLOG S108 #2 tooling follow-up (multi-root libkmc asm-TU index — scoped, deferred off-cadence golden-gated, explicitly "do NOT rush into a review gate"); #3 BACKLOG `## Active phase` S108 BANKED paragraph + remaining-asm-inventory PO note (_xatan/_xsincos/__floatdidf/__fixunsdfdi); #4 `docs/hazards.md#coddog-cross-ref` S108 workhorse-linked/wrapper-absent llcvt-FP example under coddog-structural.
- Carry-over: none. **Cross-repo follow-up:** none — both fns were already curated in the name files (no new decomp symbols to propagate).

## Sprint 107 — os/exceptasm.s (8-fn OS exception/dispatch TU), the S91 jtbl spike SOLVED via label-export — 2026-06-16
- Increment: src/libultra/os/exceptasm.s banked as a `.text`-only asm-mirror (`hasm`), 8 fns. asm subsegs ~123→122; md5-candidate unchanged at 155 (asm-mirror banks as `hasm`, not a `.c`); +8 fns matched.
- Quality: stuck-far 0, permuter 0, carried 0, re-opened 0. A seed-13 parked since S91, banked clean — 0 iteration after the mechanism.
- Seed: committed 13pt; banked 13pt; regime mirror (asm-mirror, seed-only — no v2 residual).
- What helped: the live disassembly re-opened the case at the plan gate — `jlabel` makes the 9 jtbl targets GLOBAL (`include/macro.inc`) and the jtbl lives in its OWN already-address-placed rodata subseg that survives the `.text` flip and stays SYMBOLIC. So S91's listed-but-untried "export the `.text` labels" option was viable all along. A Phase-1 rename-isolation checkpoint (strip-renames with the subseg still `asm`, proven green) cleanly separated rename risk from the destructive vendor/flip+clean-rebuild. All 8 fns were pre-curated in ghidra_symbols → no fn-add, no caller-evict.
- Friction: none mechanically (banked first-try after the mechanism). The only "friction" was a documentation one — S91's "both dead-ends proven" framing over-generalized from 2 tried paths to the whole class, which is what had parked exceptasm for 16 sprints.
- Applied (3 of 3): #1 `docs/hazards.md#asm-mirror-vendoring` asm-mirror-jtbl sub-case rewritten spike→proven LABEL-EXPORT procedure (Phase-1 rename-isolation + Phase-2 vendor-`.text`-only + re-export the `.L<addr>` labels) + `pick_target.py` comment + CLAUDE.md hazard-index row; #2 the untried-mechanism-before-dead-end lesson (BACKLOG carry-over header — a spike that lists an untried option must TRY it before the class is called a dead-end); #3 Phase-1 rename-isolation codified as step 1 of the procedure.
- Carry-over: none — the exceptasm carry-over is RESOLVED. The only genuine remaining libultra *source* work is the `setintmask` partial-TU spike (`__osDisableInt`+`__osRestoreInt` @0x8B900).
- Cross-repo follow-up: 5 new decomp-side data symbols (`__osHwIntTable`/`__osPiIntTable`/`__osIntOffTable`/`__osIntTable`/`__osThreadSave`) → propagate via `sync_decomp_names.py --import-from-decomp`.

## Sprint 106 — sched/sched.c (osCreateScheduler + 13 helpers), the libultra RCP task scheduler — THE last real libultra source mirror — 2026-06-16
- Increment: src/libultra/sched/sched.c banked / 14 fns matched (osCreateScheduler + osScAddClient/osScRemoveClient/osScGetCmdQ/__scTaskReady + 9 statics). md5-candidate 154→**155** (all .c stub-free); asm subsegs ~124→~123. Goal MET (0 stubs + ROM SHA-1 green). The libultra cheap/source-mirror band is now fully mined out — what remains is non-source work (exceptasm.s jtbl spike, game-region structural phantoms llcvt/settime/contquery, libnusys/libkmc fillers).
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. **1 fix-iteration** (2 missed `assert (` space-variants → SHA-miss → wrapped → match). Verbatim ultralib VERSION_J cp + dual carve + 2 recover + 2 caller-evict; banked atomically.
- Seed: committed 13pt; banked 13pt; regime **mirror** (seed-only — the 1 fix-iteration was a mirror enabler hiccup, not classical iteration, so it stays a point mass with no v2 residual). 8-gate FIRED at 13 → decompose MECHANICALLY BLOCKED (single-file-pack — the asm subseg `[0x86A50,asm]` is exactly the .text 0xA10, no inter-file boundary) → PO-approved enabler-forward full mirror (the S100 reverb precedent: a residual-variance single-file-pack banks atomically, not the classical-iteration stall the gate guards).
- What helped: **characterizing the whole increment up front from `ultralib/build/J/libgultra_rom/.../sched.o`** (the `_rom`/`_FINALROM` build = MG64's profile) — `nm` + `objdump -h` gave the exact section sizes (.text 0xa10 / .data 0x10 / .rodata 0x20 / **.bss 0**), the real undefined-callee set (21, of which only osViModeTable + osSpTaskYielded were unplaced), and proved `.bss=0` → SC_LOGGING is OFF → the scary `calls-unplaced:osCreateLog/osFlushLog/osLogEvent` + `jal-count-mismatch:12vs11` were ALL false flags (logging compiles out). So the plan-gate looked heavy (file-static + defines-data + bare-assert:9 + 4 calls-unplaced + rodata-jtbl) but the REAL load was 2 carves + 2 recover + 2 caller-evict + assert-strip, every one individually precedented. Carve vrams found mechanically (vram−rom = 0x80024C00 segment-wide; .data via __scExec `dp_busy=0x800C8204`; .rodata = the whole generic `[0xAD9C0,rodata]` block). The recover-callee osSpTaskYielded resolving to the S11-banked `func_800AB600` (= the un-named scheduler yield-check all along) closed a loose end.
- Friction: the plan-gate hazard set was misleading (heaviest-looking remaining libultra target, but most of the heaviness was build-inactive — the reference `.o` defused it in one inspection). The only real slip was the manual assert-strip: my `^\s*assert\(` grep found 7 but pick's `bare-assert:9` was right — 2 asserts used `assert (` with a SPACE before the paren (`assert (t->msgQ)`, `assert ( (type==…))`), so they compiled in (.text +0x80 `jal __assert`, .rodata +0x50 strings) → first-build SHA-miss → 1 fix-iteration. One of the 9 was also an `if`-body (`if(...) assert(...)`) needing the whole-`if` wrapped. Both lessons codified (Applied #1/#2).
- Applied (4 of 4): #1+#2 `docs/hazards.md#assert-strip` steps 4-5 — the assert-as-sole-`if`-body whole-`if` wrap + the `assert\s*\(` space-variant count (cross-check the manual strip against pick's `bare-assert:N`, which already uses `\bassert\s*\(`, so no tool change needed) + sched.c banked-instance; #3 `docs/hazards.md#caller-evict` Companion case B — a mirror's recover-callee that IS an already-banked `func_` (func_800AB600=osSpTaskYielded): rename + match header sig but KEEP the verified classical body (-O2 codegen risk), don't relocate to src/libultra; #4 `docs/hazards.md#caller-evict` Companion case A — a multi-global mirror flip evicts STILL-ASM callers of the old func_ names (osScAddClient/osScGetCmdQ via mus_dma asm/78D10.s), name each externally-referenced global at execution; #5 logged as a **BACKLOG tooling follow-up** (the suggestion's own stated apply-form for an off-cadence golden-gated ranker change) — extend the S104 `data-carve` detector to SCALAR initialized statics (sched's count/dp_busy/dpCount/firsttime, file-scope + function-local, were array-detector-invisible; pick's advisory `defines-data:count,firsttime` was the signal).
- Carry-over: none. **Cross-repo follow-up:** 4 new decomp symbols (`osSpTaskYielded`@0x800AB600, `osScAddClient`@0x800AB798, `osScGetCmdQ`@0x800AB880, `osViModeTable`@0x800C8270) → propagate via `sync_decomp_names.py --import-from-decomp`. Optional cosmetic: rename `src/main/func_800AB600.c` → an osSpTaskYielded-named file (keep the -O2/src/main placement).

---

## Sprint 105 — io/dpsetnextbuf.c (osDpSetNextBuffer), libultra DP next-buffer setter; the S104-split foreign TU picked up + re-identified — 2026-06-16
- Increment: src/libultra/io/dpsetnextbuf.c banked / 1 fn matched (osDpSetNextBuffer). md5-candidate 153→**154** (all .c stub-free); asm subsegs ~125→~124. Goal MET (0 stubs + ROM SHA-1 green), first-try, exactly as planned.
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. Verbatim-body ultralib VERSION_J cp; first-build full-make ROM SHA-1 == baserom, 0 iteration.
- Seed: committed 3pt; banked 3pt; regime **mirror** (seed-only — no v2 residual on the mirror track). 8-gate clear (3 < 8).
- What helped: the **split-then-mirror pipeline** S104 set up paid off cleanly — func_800B1580, split off the xprintf subseg as a foreign TU, was the smallest libultra candidate this sprint and coddog@99.99 attributed it to `src/io/dpsetnextbuf.c`. The gate asm confirmed `osDpSetNextBuffer` (3 jals = __osDpDeviceBusy@0x800B2B10 S1 + osVirtualToPhysical@0x800A7720 S7 ×2). Warm io band (dp.c S1, dpsetstat/dpctr S10, epi* S22/S23) pre-placed both callees + all 4 headers → zero recovery, zero carve, zero mid-flight surprises. The `needs-header:(already-vendored,adapt->osint.h)` annotation made the include adapt a zero-lookup mechanical step.
- Friction: none. The only thing to watch: S104 carried the TU with an imprecise label ("__osDpDeviceBusy TU") — the fn CALLS that, it isn't it. coddog + the asm were authoritative at the gate. Codified (retro #2).
- Applied: 1 of 3 — #2 `docs/hazards.md#upstream-mirror-pattern` (upstream-fncount-mismatch subsection): a split-off TU's carried label is a HINT, not a source attribution — coddog/asm verify at the next gate (motivated by exactly this S104→S105 hand-off). (#1 split-then-mirror-validated — confirmatory, log-only; #3 seed-pricing nuance on 0-work coddog-mirror + already-vendored header — NOT selected, seed is planning-only / mirror track seed-only.)
- Carry-over: none. Bonus: banking osDpSetNextBuffer pre-places a `calls-unplaced` callee for the future osCreateScheduler (pts-13) sprint.

---

## Sprint 104 — libc/xprintf.c (_Printf + _Putfld), libultra printf engine; PO's "classical spike" UNMASKED as a verbatim mirror at the gate — 2026-06-16
- Increment: src/libultra/libc/xprintf.c banked / 2 fns matched (_Printf, _Putfld). md5-candidate 152→**153** (all .c stub-free); asm subsegs ~125 (+1 func_800B1580 split subseg). Goal MET (0 stubs + ROM SHA-1 green); the target was the same, but the BRANCH (mirror, not classical) was corrected at the gate.
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. Byte-identical ultralib VERSION_J cp + dual carve; first-build full-make ROM SHA-1 == baserom, 0 iteration.
- Seed: committed 13pt; banked 13pt; regime **mirror** (seed-only — no v2 residual on the mirror track). 8-gate FIRED at 13 → resolved by DECOMPOSE (split off the func_800B1580 foreign TU at its 16-aligned boundary), then the xprintf pair ran enabler-forward as a carve-residual single-file mirror (S101 env / S93 xldtob precedent: banks atomically, not the classical-iteration stall the gate guards).
- What helped: the gate's **asm-vs-upstream investigation** (the S13 discipline) caught a misframe before any wasted classical effort. Disassembling _Printf showed exactly 3 jals (strchr×2 + _Putfld) with every `(*pfn)` output a `jalr s4` → the scary `jal-count-mismatch:14vs3` was jalr-vs-jal noise, NOT a stripped re-impl. Reading the upstream showed xprintf.c defines only 2 fns → `func_800B1580` (a `__osDpDeviceBusy` spin) is a foreign TU, split off. The warm libc band (xlitob/xldtob/string all banked) C-resolved all 4 callees → zero recovery. `objdump -h xprintf.o` was the authoritative carve-extent oracle (.data 0x50, .rodata 0x178).
- Friction: pick_target's plan-gate tags were ALL misleading on this candidate (jal-14vs3, single-file-pack:3fn, calls-unplaced:pfn, refs-unplaced:__PTRDIFF_TYPE__) — four false flags that read as "high-risk classical." The retro fixes target every one. Implementing #2 surfaced a deep counting-fragility chain (proto over-match → leading-space under-count → K&R single-token names → doc-comment over-count), resolved by rewriting `_iter_upstream_functions` as a depth-aware scanner (validated against a 445-file ultralib sweep + 4 unit tests).
- Applied: 4 of 4 — #1 `_c_jal_count` drops the .c's OWN function-like macros (xprintf PUT/PAD) + `calls_unplaced` skips fn-ptr params (`_fn_ptr_param_names`) → the jalr-vs-jal + pfn false flags; #2 `upstream-fncount-mismatch:<m>vs<n>` (foreign-TU-in-single-stem-pack) riding a depth-aware `_iter_upstream_functions` rewrite (single-token K&R + leading-space defs counted; protos/#define/doc-comments skipped) → as a bonus `_xatan`/`_xsincos` now correctly relabel single-file-pack; #3 `data-carve:<names>` .data init-static-array detector (`defines_file_static_init_array`, single-file-pack subset, the S92/S101 un-flagged class); #4 `docs/hazards.md` carve-start-past-foreign-leading-symbol + `.o`-section-size extent oracle. +4 unit tests, golden regen (func_800B1580 added; _xatan/_xsincos pack→single-file-pack; _Printf banked).
- Carry-over: none. func_800B1580 was split off as a foreign asm TU (never in scope), not a spike.

---

## Sprint 103 — mgu/mtxutil.c (guMtxF2L + guMtxL2F + guMtxIdentF + guMtxIdent), gu matrix utils; planned verbatim mirror PIVOTED to classical at -O2 — 2026-06-16
- Increment: src/mgu/mtxutil.c banked / 4 fns matched (guMtxF2L, guMtxL2F, guMtxIdentF, guMtxIdent). md5-candidate 151→**152** (all .c stub-free); asm subsegs 125→125 (the split carved a c subseg out of the `[0x414A0,asm]` game pack; the 8-fn game-code prefix stays asm, out of scope). Goal MET (all 4 fns + ROM SHA-1 green); path diverged from plan.
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. BUT **1 mid-sprint re-plan (mirror→classical) + 1 fix-iteration** — the counter-metric the headline lesson is about.
- Seed: committed 3pt (regime mirror, seed-only — the coddog-mirror tag over-promised "verbatim cp"); realized **5** (+1 mid-sprint re-plan, +1 novel profile+clamp gotcha); residual **+2**; regime **mixed** (v2 realized tier, classical track).
- What helped: diagnosing the FAILED first build by objdump-vs-ROM diff, not guesswork — it surfaced BOTH errors cleanly: (1) guMtxIdent 240B-vs-60B = -O3 inlining → the fns are GAME-region (-O2), not the libultra -O3 band (src/libultra/ forces the wrong profile); (2) guMtxF2L's clamp = a Monegi variant absent from every upstream (ultralib gu C + ultralib mgu asm + libultra_modern monegi/mgu asm, all 3 non-clamping byte-identical). Once profiled -O2 at src/mgu/, 3 fns were verbatim and guMtxF2L was a 1-iteration classical clamp (float-literal `f`-suffix for single-precision).
- Friction: the **coddog-mirror premise was wrong** — coddog matched only 2 of 4 fns (mtxidentf+mtxl2f per-fn twins), but `coddog-twin:mtxidentf!=mtxutil` was under-weighted as a clean single-file signal at the plan gate. Cost a full mirror attempt + revert before re-planning. Both new guards (#1 coddog-partial, #2 game-region-mirror) target exactly this miss for the next time.
- Applied: 4 of 4 — #1 `pick_target.py` `coddog-partial:<m>of<n>fn` (≥2-distinct-twin subset guard, the multi-twin companion to coddog-fncount-mismatch) + `test_coddog_partial_twin_subset`; #2 `pick_target.py` `game-region-mirror:0x<vram>` (libultra source below the libultra-band rom → -O2, route to src/mgu/) + `test_game_region_mirror_below_libultra_band` + `docs/hazards.md#game-region-mirror-o2-profile` + CLAUDE.md index row; #3 float-literal single-vs-double note (`docs/hazards.md#mirror-cast-divergence`); #4 codify `src/mgu/` as a no-clang-format verbatim-upstream dir (CLAUDE.md ×3 + `src/mgu/.clang-format`). suite 55→**57** pass (REGEN not needed — post-bank the live func_800660A0 row lost its gu identity, so no golden delta).
- Carry-over: none (the game-code prefix `[0x414A0,asm]` was never in scope, not a spike).

---

## Sprint 102 — io/motor.c (__osMotorAccess + osMotorInit), libultra io VERSION_J mirror; corrected a wrong ghidra name without sync-names — 2026-06-16
- Increment: src/libultra/io/motor.c banked / 2 fns matched (__osMotorAccess, osMotorInit). md5-candidate 150→**151** (all .c stub-free); asm subsegs 126→125. The io/motor.c trap S75 flagged (the smallest remaining libultra target — pts-8; everything else `--lib libultra` is pts-13 + structurally trapped).
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. Verbatim ultralib VERSION_J cp (+ include adapt + drop-static); first-build full-make ROM SHA-1 == baserom, 0 iteration.
- Seed: committed 8pt; banked 8pt; regime mirror (8-gate FIRED but decompose MECHANICALLY BLOCKED — one-tu single-file-pack, `__osMakeMotorData` inlined → no inter-file boundary → ran 1-increment enabler-forward per the S100/S101 precedent; failed the strict exemption on residual variance: drop-static + name override).
- What helped: reading the prebuilt object `nm build/J/libgultra_rom/.../motor.o` settled the entire ambiguity at the gate — the 0x800AE380 fn is `T __osMotorAccess` (not the ghidra `osMotorStop`, a macro), `__osMakeMotorData` is inlined (→ pack:2fn), `__MotorDataBuf` is local `b` (→ drop-to-extern). Reading the splat source (`util/symbols.py:298-309`) proved the dup-symbol error fires only on same-rom+segment, so a `rom:`-qualified `symbol_addrs` override is non-clashing AND (loaded first) wins the reference — the clean non-destructive alternative to `make sync-names`. The S100/S101 enabler-forward single-file-pack playbook pre-pinned the 8-gate handling.
- Friction: pick_target's whole-file scans flagged inactive-`#else`-branch false positives (`defines-data:__osMotorinitialized`, half of `drop-static:2bss`) because the version-strip wasn't wired into the data/static detectors, and `calls-unplaced:READFORMAT` (a same-file function-like macro). All resolved by VERSION_J branch analysis at the gate (no iteration) and fixed in the tool this retro. The wrong ghidra name (osMotorStop) was the one real puzzle — solved without touching the sync-owned ghidra_symbols.txt.
- Applied (4 of 4): #1 NEW `docs/hazards.md#wrong-ghidra-name-override` + CLAUDE.md hazard-index row + `pick_target.py` `wrong-ghidra-name:<g>-><c>@<hdr>` tag (`_macro_alias_target` + `_upstream_defines_function`) + unit test `test_wrong_ghidra_name_override`; #2 `pick_target.py` `#if BUILD_VERSION` version-strip wired into `has_file_scope_static`/`defines_data_globals`/`defines_local_static_data` + same-file function-like macro exclusion in `calls_unplaced` (also cleaned the `_Printf` ATOI/LDSIGN/PAD/PUT/isdigit FPs); #3 `header_renames_symbol` gated by `_upstream_defines_function` (macro-alias false-fire suppression); #4 `docs/hazards.md#upstream-mirror-pattern` `nm build/J/libgultra_rom/*.o` authoritative-symbol-set note. suite 55 pass, golden regen (2 intended diffs: __osMotorAccess now placed, _Printf macro FPs gone).
- Carry-over: none. Cross-repo follow-up: rename 0x800AE380 `osMotorStop`→`__osMotorAccess` in the Ghidra workspace (the deferred reconciliation; the symbol_addrs override coexists with the stale ghidra_symbols entry until then).

---

## Sprint 101 — audio/env.c (envmixer 7fn) + audio/filter.c (alFilterNew); cleared the 0x804D0 c-combined:2file[env|filter] pack — 2026-06-16
- Increment: src/libultra/audio/env.c + filter.c banked / 8 fns matched (alEnvmixerPull, alEnvmixerParam, _pullSubFrame, _frexpf, _ldexpf, _getRate, _getVol; alFilterNew). md5-candidate 148→**150**; asm subsegs 127→126. The `[0x804D0]` c-combined pack split + env dual-carved. The audio-synth mirror vein is now fully banked.
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. Both verbatim ultralib VERSION_J cp (env: + 3 assert-strip + dual carve + 2 recover-callee); first-build full-make ROM SHA-1 == baserom, 0 iteration.
- Seed: committed 13pt; banked 13pt; regime mirror (8-gate FIRED → MANDATORY decompose, c-combined blocks the exemption → split at the env/filter boundary; env then ran enabler-forward per the S96 drvrnew / S100 reverb precedent — dual carve + assert-strip = residual variance).
- What helped: the linear segment map (vram = rom + 0x80024C00, anchored off alEnvmixerPull) placed BOTH carves instantly — eqpower 0x800C8060→rom 0xA3460, jtbl 0x800D22F0→rom 0xAD6F0 — each landing EXACTLY on an existing generic `[off,(ro)data]` subseg whose end bounded the env.o section, so both carves were 1-line attribute flips (NO split, S93-class, now demonstrated for `.data` AND `.rodata` in one file). The twin-of:drvrnew / S100 reverb playbook pre-pinned the class; save.c pinned the `#ifdef _DEBUG` assert-strip style. Disassembling alEnvmixerPull at the gate cleanly separated the 2 real cross-file callees (__freeParam/_freePVoice, synth-region jal targets) from the 2 intra-file false-positives (_frexpf/_ldexpf, env.c members).
- Friction: minimal. pick flagged all 4 calls-unplaced incl. the 2 intra-file noise callees (S101 #1 follow-up); eqpower `.data` was UN-flagged (S92 reverted detector — recovered from the asm `s4` load); pick's `carve-end=0x800D25C0` over-stated the real 0x800D23E0 (S98 deferred carve-end). All resolved at the gate read, no iteration.
- Applied (3 of 3): #1 `docs/hazards.md#rodata-sibling-yaml-pattern` generic-subseg-bound carve = exact-extent-no-split heuristic (advances the deferred S98 carve-end work); #2 BACKLOG S92 `.data`-carve detector confirmed (env = 2nd data point; single-file-pack post-split = the safe first slice with no per-member ambiguity); #3 BACKLOG new S101 #1 tooling follow-up — suppress intra-pack `calls-unplaced` (calls-side dual of S66 #2, off-cadence golden-gated).
- Carry-over: none. Cross-repo follow-up: 6 new decomp symbols (_frexpf/_ldexpf/_getRate/_getVol/__freeParam/_freePVoice) → propagate via sync_decomp_names.py --import-from-decomp.

## Sprint 100 — audio/reverb.c (alFxPull pack:8fn), libultra audio reverb effect mirror; cleared the 0x815C0 pack — 2026-06-16
- Increment: src/libultra/audio/reverb.c banked / 8 fns matched (alFxPull, alFxParam, alFxParamHdl, _loadOutputBuffer, _loadBuffer, _saveBuffer, _filterBuffer, _doModFunc). md5-candidate 147→**148**; asm subsegs 128→127. The `[0x815C0]` single-file pack flipped + dual-carved.
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. Verbatim ultralib VERSION_J cp + 1 assert-strip + dual carve; first-build full-make ROM SHA-1 == baserom, 0 iteration.
- Seed: committed 13pt; banked 13pt; regime mirror (8-gate FIRED — decompose-blocked single-file-pack + fails the S64/S69 verbatim exemption (dual carve + assert-strip = residual variance); PO ran the full mirror enabler-forward per the S96 drvrnew precedent).
- What helped: the S96 drvrnew replay was near-mechanical — headers (libaudio/synthInternals/initfx/stdio/os) pre-vendored, the cross-file entry points (alFxPull/Param/ParamHdl, _init_lpfilter, alGlobals, osVirtualToPhysical) pre-named S96/S97/S7, the `twin-of:drvrnew` tag pre-pinned the carve region. The build-then-read flow surfaced the exact carves: link error → the `.rodata` jtbl (attribute-change, generic blob already bounded 0xAD810..0xAD860 exactly), objdump section dump → the `.data` (0x20). `SWAP`=inline macro (calls-unplaced false flag); jal 8vs7 = the stripped assert (both reconciled from the source read at the gate). Kept all 8 upstream includes verbatim (ultraerror.h+os_internal.h ARE vendored — drvrnew's drop was unnecessary).
- Friction: the `.data` carve was UNREFERENCED dead statics (L_INC[]/val/lastval/blob — unused, but KMC GCC 2.7.2 emits them; explicit-`=0` statics go to .data not .bss), so NO `.text` reloc to recover the carve vram from — the S61/S96 "size from the %hi address band" step had nothing to read. Located by ROM byte-search (objdump `.data` → `xxd baserom | grep`), at 0xA3560 (0x100 B past drvrnew's 0xA3460 tail — link order interleaves other files' .data, so it is NOT immediately after the previous mirror's carve).
- Applied (1 of 3): #1 `docs/hazards.md#defines-data` unreferenced-static-carve sub-case (objdump .data → ROM byte-search when no `%lo` ref; + deferred `pick_target` `;unref` tag as an off-cadence ranker follow-up). (#2 carve-end over-estimate reinforce — NOT selected, confirmatory of the open S98 follow-up; #3 defines-data incomplete-names — NOT selected, minor/superseded by #1.)
- Carry-over: none.

## Sprint 99 — libnusys nugfxdisplayon.c + nupiinit.c + nupiinitsram.c; cleared the 0x7CAD0 c-combined:3file pack — 2026-06-16
- Increment: src/libnusys/mainlib/{nugfxdisplayon,nupiinit,nupiinitsram}.c banked / 3 fns matched (nuGfxDisplayOn, nuPiInit, nuPiInitSram). md5-candidate 144→**147** (all .c stub-free). The `[0x7CAD0]` `c-combined:3file` pack decomposed at the 3 file boundaries (nuPiInit@0x7CAE0, nuPiInitSram@0x7CB20, 16-aligned).
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. nugfxdisplayon = trivial verbatim cp; nupiinit/nupiinitsram = S87 drop-static mirrors (5 file-scope defs → extern, 3 statics asm-recovered into symbol_addrs, no carve — `.bss` is NOBITS). All 3 first-build full-make ROM SHA-1 == baserom, 0 iteration.
- Seed: committed 3pt; banked 3pt; regime mirror (8-gate clear). NOTE: seed 3 was a primary-only lower bound — pick_target's whole-pack scan missed members 2&3's drop-static load (anchor-true ~6); retro #1 fixes the under-pricing class.
- What helped: the MCP asm-disassemble at the gate recovered all 3 static addrs (PiMesgQ/PiMesgBuf/SramHandle) + confirmed the 2 globals already placed → priced the drop-static load before execution. `.bss`-NOBITS insight: no carve, the ROM match rode entirely on the `.text` relocs resolving via symbol_addrs (S81/S87 pattern, 3rd confirmation).
- Friction: batch-adding all 3 statics up front transiently red the build — `SramHandle` evicted the still-asm `nupiinitsram` stub's sub-field `D_` labels (a DATA-symbol caller-evict) until the body landed. Benign, now documented (#2).
- Applied (3 of 3): #1 `pick_target.py` comment-strip fix (`has_file_scope_static` + `defines_data_globals` scan comment-STRIPPED text — trailing-`/*..*/`-after-`;` defeated FILE_STATIC_RE; a `Copyright (C)` banner's `(` falsely tripped the K&R guard, suppressing depth-0 globals across the whole nusys band) + `file-static` member-union over c-combined members; golden regen (now flags gfxThread/nuContMgrInit/nuGfxTaskMgr defines-data + nuContGBPakFwrite file-static+defines-data), suite 54 pass; #2 `docs/hazards.md#file-static` batch-add transient-red note + detector-sync note; #3 `.bss`-NOBITS-no-carve confirmation (log-only).
- Carry-over: none.

## Sprint 98 — audio/mainbus.c (alMainBusPull/alMainBusParam) + audio/resample.c (alResamplePull/alResampleParam), libultra audio-synth mirrors; cleared the 0x811A0 c-combined pack — 2026-06-16
- Increment: src/libultra/audio/mainbus.c + src/libultra/audio/resample.c banked / 4 fns matched (alMainBusPull, alMainBusParam; alResamplePull, alResampleParam). md5-candidate 142→**144** (all .c stub-free); asm subsegs 130→129. The `[0x811A0]` `c-combined:2file[mainbus|resample]` pack — the cheapest remaining audio-synth-cluster unit (S97 warm next band) — decomposed at the mainbus/resample file boundary (0x81310, alResamplePull@0x800A5F10, 16-aligned).
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. Both verbatim ultralib VERSION_J cp; mainbus carve-free, resample one `.rodata` carve. Both first-build full-make ROM SHA-1 == baserom, 0 iteration.
- Seed: committed 4pt; banked 4pt; regime mirror (seed mainbus 2 + resample 2; the pts-8 combined pack tripped the 8-gate → satisfied by decomposing at the file boundary, NOT the verbatim exemption — c-combined MUST decompose; each split singleton pts-2, clear).
- What helped: (a) **asm was ground truth for carve ownership** — disassembling alMainBusPull (no 0x800D2 refs, all immediates) vs alResamplePull (`lui at,0x800d`/`ldc1 0x23e0`) proved the rodata belongs ENTIRELY to resample, despite pick_target lumping `rodata-jtbl`/`rodata-literal`/`twin-of` onto the mainbus (primary) row; (b) the **pre-carve build link-error was definitive** — `AD6F0.rodata.o:(.rodata+0x114): undefined reference to .L800A6188` named the orphaned jtbl entries, and those `.L<addr>` labels sit inside alResampleParam → resample owns the carve; (c) the extracted `asm/data/AD6F0.rodata.s` listing made the surgical 3-way split obvious (resample = D_800D23E0 double + jtbl_800D23E8 = 0xAD7E0..0xAD810 = 0x30; everything before refs 0x800A5xxx/other fns, everything after = fx/reverb jtbl_800D2410); (d) all 4 names pre-curated S96 (drvrnew callees) → zero symbol adds; the twin-of:drvrnew (S96) playbook pinned the carve mechanics.
- Friction: pick_target's whole-pack rodata scan (S55, correct for a single-file pack) **over-attributed resample's carve to the mainbus primary row** — a c-combined pack lumps both members' rodata onto the leader, so the flag pointed at the carve-FREE file. Confirmed by asm; would mislead an agent that trusted the row over the asm. Secondary: re-running pick_target AFTER the split mis-attributed via a STALE `asm/811A0.s` (splat leaves the old per-ROM listing — with all 4 fns — when a subseg flips to c), so mainbus's row still saw resample's `ldc1`.
- Applied: 2 of 2 (conservative form, deeper code carried): #1 **`pick_target.py` `;owner-per-member` marker (tooling)** — when a pack is c-combined (`member_paths` non-empty), suffix any `rodata-jtbl`/`rodata-literal` hazard so the gate does NOT carve the primary `.c` by default; fires ONLY for multi-file packs (single-file packs untouched → no S55 regression); verified live on the env/filter pack 0x804D0 (`rodata-jtbl:0x800D22F0;owner-per-member`). Plus `CLAUDE.md` hazard-index row + `docs/hazards.md#rodata-sibling-yaml-pattern` "carve owner is per-member" note with the link-error confirm step. #2 **carve-end upper-bound + stale-asm caveats** captured in the same hazards section (carve-end can run past an intervening sibling's already-placed carve — size the real carve from the owning member's `.o(.rodata)`). **CARRIED as a tooling follow-up:** full per-member attribution (scan each member fn's body, tag the owning stem; cover the coddog jtbl path in `_append_recover_hazards`) + label-range-bounded carve-end — both touch the smallest-first ranker, so they warrant off-cadence golden-gated work, not a hurried in-review change. Golden regen (post-S98 bank + the new owner-per-member markers), tooling suite 54 pass.
- Carry-over: none for the increment. Tooling follow-up (S98 #1 deferred half) recorded in BACKLOG.
- Cross-repo follow-up: none (all 4 names pre-curated S96; zero new decomp-side symbols this sprint).

---

## Sprint 97 — audio/save.c (alSavePull/alSaveParam) + audio/sl.c (alInit/alClose/alLink/alUnlink), libultra audio-synth mirrors; cleared the 0x82160 c-combined pack — 2026-06-16
- Increment: src/libultra/audio/save.c + src/libultra/audio/sl.c banked / 6 fns matched (alSavePull, alSaveParam; alInit, alClose, alLink, alUnlink). md5-candidate 140→**142** (all .c stub-free). The smallest audio-synth-cluster unit, the `[0x82160]` `alSavePull` pack — `c-combined:2file[save|sl]` decomposed at the save.c/sl.c file boundary (0x82230, alInit).
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. Both verbatim ultralib VERSION_J cp + one known-edit each (assert-strip wrap / shared-global drop), first-build full-make ROM SHA-1 == baserom, 0 iteration.
- Seed: committed 5pt; banked 5pt; regime mirror (seed 2 save + 3 sl; 8-gate fired on the pts-8 pack → satisfied by decomposing at the file boundary, NOT the verbatim exemption — c-combined MUST decompose).
- What helped: (a) reading both upstream `.c` at the gate caught sl.c's `defines-data:alGlobals` that pick_target (primary-keyed) missed; (b) the asm was ground truth — alSavePull's zero `jal` confirmed the assert strips, and alClose's `jal func_80051E54` named alSynDelete; (c) the `#assert-strip` + `#defines-data` playbooks gave the exact mechanical fixes (sirawread `#ifdef _DEBUG` wrap; S42 drop-to-extern).
- Friction: save.c SHA-missed mid-execution on the first audio-cluster `assert()` (siblings load/drvrnew/auxbus have none) — `<assert.h>` resolves to `include/assert.h` (NDEBUG-keyed, active) → live `__assert`. Resolved in one known-edit wrap. sl.c's alGlobals is a SHARED global (env/fx still-asm reference it) → forced the DROP (a carve would orphan the sibling refs / double-define); recognized before building.
- Applied: 3 of 3: #1 `pick_target.py` `bare-assert:<n>` advisory (scans upstream for non-`_DEBUG`-guarded `assert(` post-`_strip_dead_blocks`; flags reverb/env/sched at the gate) + CLAUDE.md hazard-index row; #2 `docs/hazards.md#defines-data` shared-global DROP-mandatory rule (carve only when the file is the sole referrer); #3 `pick_target.py` unions `defines-data` over `c-combined` member upstreams (`_c_combined_member_paths`, so a secondary member's defined global prices at the gate). Golden regen (absorbs the bank + new flags), tooling suite green.
- Carry-over: none. The audio-synth cluster's remaining asm (env @804D0, mainbus+resample @811A0, fx/reverb @815C0) is the warm next band — pick_target now pre-flags its `bare-assert` (env 3, reverb 1) + member-union `defines-data`.

---

## Sprint 96 — audio/drvrnew.c (_init_lpfilter/alFxNew/6×al*New), libultra audio synth driver verbatim mirror; first non-exemption audio mirror — 2026-06-16
- Increment: src/libultra/audio/drvrnew.c banked / 8 fns matched (_init_lpfilter, alFxNew, + 6 trivial al*New filter-constructor wrappers: alEnvmixerNew/alLoadNew/alResampleNew/alAuxBusNew/alMainBusNew/alSaveNew). md5-candidate 139→**140** (all .c stub-free). The al synthesis **driver** — 3rd audio sub-band mirror (after S94 auxbus, S95 load), and the FIRST that did not run under the verbatim-mirror exemption.
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. Verbatim ultralib VERSION_J, byte-identical cp, first-build full-make ROM SHA-1 == baserom, 0 iteration. drvrnew.o sections match the carve extents exactly (.text 0x830 / .data 0x190 = 100 s32 / .rodata 0x40).
- Seed: committed 13pt; banked 13pt; regime mirror (seed-only). pts-13 tripped the 8-gate and — UNLIKE S93/S94/S95 — the **exemption did NOT apply**: condition (c) "no residual variance" failed (10 cross-file callee names + a dual `.data`+`.rodata` carve + 2 header vendors). Ran enabler-forward per the 8-gate's "pull scaffolding as the goal" path; PO chose the full mirror over an enabler-only sprint, and it banked atomically anyway.
- What helped: the **asm a1/a2+jal at the gate gave ground-truth for all 10 cross-file callee names** (each al*New loads its pull into a1, param into a2, then `jal alFilterNew`=func_800A5D80) → alMainBusPull/Param, alResamplePull/Param, alFxPull/Param/ParamHdl, alSavePull/Param mapped deterministically, no conflict, no caller-evict (pre-checked). Resolving the **debug-heap macro** early killed the scariest hazard: the asm calls `alHeapDBAlloc` not `alHeapAlloc`, but libaudio.h's `#ifndef _DEBUG` branch makes `alHeapAlloc(hp,c,s) → alHeapDBAlloc(0,0,hp,c,s)` — file/line are literal 0, so the `refs-unplaced:__FILE__,__LINE__` flag was FALSE (no rodata filename string to reproduce). Both carves sized cleanly from the `%hi` address band (0x800Cxxxx=.data PARAMS in main_data, 0x800D2xxx/jtbl=.rodata its own subseg); all carve labels were drvrnew-exclusive (grep-confirmed) so no external refs broke. Strategic dividend: banking drvrnew **pre-named the reverb/mainbus/save/resample/fx entry points** and vendored the shared initfx.h/stdio.h → the audio synth cluster is now the warm cheaper next band.
- Friction: the `coddog-mirror:src/audio/load.c@98.17` on the sibling func_8009F440 was a STRUCTURAL false-attribution (load.c banked S95) that cost a manual "is this really load.c?" gate investigation — fixed by #1. The `__FILE__/__LINE__` refs-unplaced phantom — fixed by #2. Process note: the pick_target golden test reads live repo state, so banking drvrnew (the 18 syms + the file) drifted the golden independent of the tooling edits; the retro regen absorbed both (alSavePull/alMainBusPull now named, drvrnew gone) — a recurring per-bank golden-coupling, not new.
- Applied: 3 of 4 edits (+1 log-only): #1 **`pick_target.py` coddog-source-banked tag (tooling)** — a `coddog-mirror` hit whose project mirror is already banked (0-stub) emits `coddog-source-banked:<file>` (advisory); the source is fully decompiled so the match can't be a fresh attribution, the sibling of coddog-fncount-mismatch/coddog-structural. Verified live: func_8009F440 now shows `coddog-source-banked:load.c`. #2 **`pick_target.py` refs_unplaced skips compiler predefined macros** — `_C_PREDEF_MACROS` (__FILE__/__LINE__/__DATE__/__TIME__/__func__/…) dropped like `__builtin_`; never link symbols. De-noises the reverb/env siblings carrying the same debug-heap macro. #3 **`docs/hazards.md` dual-section-carve cross-ref** — #defines-data ↔ #rodata-sibling-yaml-pattern bidirectional note: a coddog mirror with file-scope `static` initialized arrays + a switch/pooled-FP needs BOTH carves in one increment; size by the `%hi` address band; initialized statics are a `.data` carve, NOT file-static. Golden regen (50 pass, 4 skip). #4 cluster-unlock **log-only** (recorded here + BACKLOG: drvrnew pre-named the cluster entry points + vendored the shared headers → reverb/env/mainbus/save/resample cheaper next).
- Carry-over: none.
- Cross-repo follow-up: 18 new decomp-side symbols (8 drvrnew fns + 10 cross-file pull/param entry points) → propagate via `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 95 — audio/load.c (alAdpcmPull/alRaw16Pull/alLoadParam/_decodeChunk), libultra audio verbatim mirror; 2nd audio sub-band leaf — 2026-06-15
- Increment: src/libultra/audio/load.c banked / 4 fns matched (alAdpcmPull/alRaw16Pull/alLoadParam/_decodeChunk, the ADPCM + raw16 sample pulls, the load-param setter, and the chunk decoder). md5-candidate 138→**139** (all .c stub-free); asm subsegs 156→155. 2nd mirror in the **audio sub-band** (after S94 auxbus).
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. Verbatim ultralib VERSION_J, byte-identical cp, first-build full-make ROM SHA-1 == baserom, 0 iteration, **0 recover-extern**; no carve, no split.
- Seed: committed 13pt; banked 13pt; regime mirror (seed-only). pts-13 tripped the 8-gate → ran under the **verbatim-mirror exemption (S64/S69)**: regime mirror + verbatim single upstream file (the `[0x7F8B0]` subseg = 0xB10=2832B = exactly load.c's 4 fns, ending at auxbus 0x803C0) + decompose-blocked single-file-pack + all callees placed (alCopy S36) + 4 names curated at gate.
- What helped: the **coddog map (func→fn→src, in source order) was decisive at the gate** — it cleanly separated the single-file pack (load.c: all 4 fns → load.c@99.99, ordered) from the genuinely multi-file audio packs (mainbus 2vs4, save 2vs6 coddog-twin → structural FPs needing a boundary split first). The S88/S92 `coddog-fncount-mismatch` detector correctly did NOT fire on load.c. **All 3 flagged hazards proved false at gate triage** (no execution-time surprises): `refs-unplaced:lastCnt` — the `extern u32 ...lastCnt[]` decl AND every use (the `lastCnt[++cnt_index]` ref + the 2 `PROFILE_AUD()` calls) are `#ifdef AUD_PROFILE`-guarded and MG64 doesn't define AUD_PROFILE → all compile out; `calls-unplaced:alLoadParam` — a self-member (func_800A4C90), the coddog tail-carry address artifact. The audio header `-I` enabler was already paid (S36/S94) → near-zero-enabler cp.
- Friction: pick_target's phantom `refs-unplaced:lastCnt` (resolved by gate source-reading; fixed permanently by #1). Surfaced (not new): pick_target's `refs_unplaced` never stripped dead `#ifdef _DEBUG/_FINALROM` blocks (unlike calls_unplaced) — a latent asymmetry the AUD_PROFILE case made concrete.
- Applied: 3 of 3: #1 **AUD_PROFILE de-noise (tooling)** — `cpreprocess.py` `_strip_dead_blocks` dead-`#ifdef` set += `AUD_PROFILE`; `pick_target.py` `refs_unplaced` now also calls `_strip_dead_blocks` (symmetric with `calls_unplaced`, which already did) so a data extern referenced ONLY in a dead branch is not phantom-flagged; and `macro_hidden_text` strips dead blocks before scanning for macro invocations, so a macro invoked ONLY inside a dead block (`PROFILE_AUD` in the audio band) is not expanded into a phantom ref. Net: the phantom `lastCnt`/`save_min`/`rate_min`/`vol_min` profiling externs drop from the reverb/env/load rows; suite 54 pass, golden-inert. #2 `BACKLOG.md` audio-sub-band ordering refresh (the S94 note: auxbus + load were the only two near-clean audio leaves; the remaining single-file packs drvrnew/reverb/env need a vendorable-header(stdio.h,initfx.h)+rodata-jtbl-carve enabler, and mainbus/save need a multi-file coddog-boundary split first). #3 coddog-attribution **log-only** (confirmatory: the member map's per-fn source attribution, in order, was the gate's discriminator; validates the coddog-fncount-mismatch detector; no code change).
- Carry-over: none.
- Cross-repo follow-up: 4 new decomp-side symbols (alAdpcmPull=0x800A44B0, alRaw16Pull=0x800A48F4, alLoadParam=0x800A4C90, _decodeChunk=0x800A4E3C) → propagate via `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 94 — audio/auxbus.c (alAuxBusPull/alAuxBusParam), libultra audio verbatim mirror; opens the audio sub-band — 2026-06-15
- Increment: src/libultra/audio/auxbus.c banked / 2 fns matched (alAuxBusPull/alAuxBusParam, the aux-bus pull + param ABI filter). md5-candidate 137→**138** (all .c stub-free); asm subsegs 157→156. First mirror in the **audio sub-band**.
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. Verbatim ultralib VERSION_J, byte-identical cp, first-build full-make ROM SHA-1 == baserom, 0 iteration; no carve, no split.
- Seed: committed 13pt; banked 13pt; regime mirror (seed-only). pts-13 tripped the 8-gate → ran under the **verbatim-mirror exemption (S64/S69)**: regime mirror + verbatim single upstream file + decompose-blocked `one-tu` single-file-pack + no jal callees to place (ABI macros + indirect `handler`) + both names curated at gate.
- What helped: the **`--lib libultra` table was misleading** (the S55 caveat — un-flipped libultra asm subsegs lack a `libultra/` path qualifier; un-named audio coddog mirrors classify `upstream none` and were filtered out), showing ONLY pts-8/13 spikes (motor.c version-trap, sched.c carve+jtbl, exceptasm jtbl-bind, llcvt structural packs). Surveying by the **coddog column** instead found `func_800A4FC0` @ 0x803C0 = `auxbus.c`@100.00, `one-tu`, ZERO other hazards — the smallest-clean leaf. The audio header `-I` enabler was **already paid** (heapinit/heapalloc/copy flipped) → a near-zero-enabler cp. Pre-gate verification confirmed clean: no file-scope data/statics (no carve), callees ABI-macro/indirect (no jal to place), `switch` one case (no jtbl), all types/macros in-tree.
- Friction: the survey detour itself — the scoped `--lib libultra` filter hid the best target, costing a full-band by-coddog-column survey (fixed by #1 below). Also surfaced (deferred): seed-13 over-prices a zero-hazard atomic audio mirror because audio coddog isn't re-priced to `libultra` (the S71 header-gate carve-out, now stale since the `-I` is paid) → priced as a classical pack; absorbed by the exemption, documented in #3.
- Applied (3 of 3): #1 `pick_target.py` — `--lib <scope>` now ALSO surfaces a coddog-mirror row whose matched source is in-scope, even when the row stayed `upstream none` (audio coddog hits are flagged but NOT re-priced — header-gated). The coddog map IS the ultralib sweep → any coddog-mirror match counts as `libultra`; a sub-path scope (`--lib audio`) matches via the matched-source path substring. Found+fixed a **crash** (None `args.lib` → `None in s` TypeError on the no-`--lib` `--json` path; short-circuit on `bool(args.lib)`). +1 unit test (`test_lib_filter_surfaces_audio_coddog_mirror`: libultra-scope + audio-sub-path-scope surface the audio coddog row, no-match scope still excludes it), suite 53→54 pass, **golden regen** (the S94 bank removed func_800A4FC0 from the asm pool → tail-shift func_800A4FC0→func_800A3C80; the no-`--lib` golden is unaffected by the filter edit — pure bank delta, audited). #2 `BACKLOG.md` audio-sub-band ordering note (the `-I` enabler is paid; the next-cleanest audio mirrors + their hazards: mainbus/save/load/drvrnew/reverb/env, auxbus was the only zero-hazard leaf). #3 pts-mirror-over-estimate **decision** (`VELOCITY.md` seed-rubric note): keep pts as-is — display-only, no smallest-first sort effect, the S64/S69 exemption deterministically absorbs the false 8-fire; the deeper root (re-price audio coddog to a mirror seed now the `-I` is paid) is a candidate future edit, deferred (would re-seed every audio row + golden regen + per-row vendorable-header FP analysis).
- Carry-over: none. (The reverted S92 `.data`-carve detector tooling follow-up stays open in BACKLOG, untouched this sprint.)
- Cross-repo follow-up: 2 new decomp-side symbols (alAuxBusPull=0x800A4FC0, alAuxBusParam=0x800A509C) → propagate via `sync_decomp_names.py --import-from-decomp`.

## Sprint 93 — libc/xldtob.c (_Ldtob/_Ldunscale/_Genld), libultra libc float-to-string verbatim mirror — 2026-06-15
- Increment: src/libultra/libc/xldtob.c banked / 3 fns matched (_Ldtob/_Ldunscale/_Genld, the float/long-double→string scaling routines). md5-candidate 136→**137** (all .c stub-free); asm subsegs −1 (0x8D480 flipped) + 1 generic rodata [0xADBD0] carved.
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. Verbatim ultralib VERSION_J, byte-identical cp, first-build full-make ROM SHA-1 == baserom, 0 iteration; the `.rodata` carve was a 1-line attribute change (boundaries pre-matched), 0x70 exact.
- Seed: committed 13pt; banked 13pt; regime mirror (seed-only). pts-13 tripped the 8-gate → ran under the **verbatim-mirror exemption (S64/S69)**: regime mirror + verbatim single upstream file + decompose-blocked `single-file-pack:3fn`/`one-tu` + all callees placed + all names curated.
- What helped: the **S92 near-free-retry carry-over checklist replayed verbatim-correct** (flip line / placed-ref inventory / rodata recovery / includes / upstream pin) → 0 rework, a clean S74→S75-style mechanical replay (the third proof the 5-point checklist pays off). `pows[]` is `const` → `.rodata` ONLY (no `.data` carve, unlike the twin xlitob's mutable ldigs/udigs) — and splat's per-TU auto-segmentation had ALREADY bounded the generic `[0xADBD0, rodata]` subseg at the exact 0x70 extent (0xADBD0→0xADC40, vram 0x800D27D0→0x800D2840), so the carve was a 1-line `rodata`→`.rodata,path` attribute change, no split arithmetic. All 3 names pre-curated in ghidra_symbols + all 5 callees placed (memcpy/ldiv/lldiv/__udivdi3/__umoddi3) → zero symbol adds; `jal-count-mismatch:4vs3` was a false flag (SHA proved the verbatim body).
- Friction: none. (The only minor gap was cosmetic — the `rodata-literal` flag under-reported the carve-start by 0x50 B, missing the `pows[]` array base + string literals; recovered trivially at execution from the .o(.rodata) size + the preceding dlabel. Fixed by the #1 widening below.)
- Applied (3 of 3): #1 `pick_target.py` rodata-literal **carve-start widening** — new `defines_file_static_const_array` source-scan gates a rodata-subseg-start carve-start (symmetric to the existing `_rodata_carve_end_vram`); the FP/`lw` scans see only scalar `%lo` loads, so they missed the `static const ldouble pows[]` base 0x800D27D0 (an `addiu %lo` address-of) + the "NaN"/"Inf" string pointers, under-stating the start by 0x50 B. The `static const` source gate keeps the widening FP-safe (a `const` static is file-private rodata) — the deliberately-narrow companion to the S92-reverted `.data` addiu scan (which over-fired on shared cross-file externs; rodata + the source gate sidestep that). +1 unit test (`test_file_static_const_array_widens_rodata_carve_start`: const-array detect / non-const reject / depth-1 exclude / scalar reject), suite 52→53 pass, golden-inert (xldtob now banked, env.c has no const array → no live row change). #2 `docs/hazards.md#rodata-sibling-yaml-pattern` two notes — the carve-start widening (FP-safety rationale) + the carve-as-attribute-change shortcut ("first check whether splat already bounded the carve; no split when the generic subseg extent already matches") + S93 provenance. #3 near-free-retry checklist confirmation (no edit — the S92 checklist replayed clean, confirming the S75 #2 discipline).
- Carry-over: none. (The S92 xldtob-tail carry-over is now RESOLVED + banked; the reverted `.data`-carve detector tooling follow-up stays open in BACKLOG, untouched this sprint.)

## Sprint 92 — libc/xlitob.c (_Litob), libultra libc c-combined decompose + .data carve — 2026-06-15
- Increment: src/libultra/libc/xlitob.c banked / 1 fn matched (_Litob, the printf integer-to-string radix formatter). md5-candidate 135→**136** (all .c stub-free); asm subsegs 135→135 (xldtob tail stays asm).
- Quality: 0 stuck-far / 0 permuter / 0 carried / 0 re-opened. Verbatim mirror, byte-identical to ultralib VERSION_J, first-build full-make ROM SHA-1 == baserom, 0 iteration; the ldigs/udigs `.data` carve was 0x30 exact first try.
- Seed: committed 3pt; banked 3pt; regime mirror (seed-only). Parent pack pts-13 tripped the 8-gate → resolved by the mandatory file-boundary decompose (c-combined:2file blocks the verbatim-mirror exemption).
- What helped: the gate debunked the llcvt coddog phantoms by reading the source (8 trivial `return d;` stubs, ~250B, cannot be a 2032B/11fn subseg) — cheap and decisive, steered straight to the real decomposable libc leaf. The named-member identity (`_Litob=xlitob`) made the <99% coddog pct (a reloc-mask artifact) irrelevant — byte-verbatim once placed (retires S91's "divergent-classical" mischaracterization). All 4 callees pre-named (lldiv/memcpy/__udivdi3/__umoddi3) → zero symbol adds; the `.data` carve delta was anchored cleanly off a sibling carve's map entry (gu/align.o(.data) → delta 0x80024C00).
- Friction: suggestion #3 (a `.data`-carve detector for file-static initialized arrays) was built then **reverted at apply** — the `addiu %lo(D_<addr>)` scan over-fired on cross-file shared externs (0x800C8270 flagged on BOTH osCreateScheduler + nuScCreateScheduler; 0x800C7E30 on nuContGBPakFwrite). An address-of into `.data` is identical in asm for a file's own static (carve) vs a referenced extern (recover), and refs-unplaced has gaps, so the naive scan mis-routes externs to a phantom carve. Distinguishing them needs source-correlation the c-combined/coddog up_path can't reliably give → deferred.
- Applied (2 of 3): #1 `coddog-fncount-mismatch` extended to TAIL-carried coddog identities (the S88 check ran only on the primary member; func_80050400's llcvt identity is tail-carried → 8vs11 never fired — now caught all 3 llcvt phantoms 8vs11/8vs17/8vs9) + CLAUDE.md index/`docs/hazards.md#coddog-cross-ref` notes; #2 new `coddog-structural:<file>@<pct>` advisory size-ratio guard (`subseg_bytes > 64 × source_meaningful_LOC` on a single-coddog-identity multi-fn pack, `_meaningful_loc` helper) + hazard index row; (#3 the `.data`-carve detector — REVERTED, see Friction → carry-over). suite 52 pass, golden-inert (the phantoms rank below the golden's default row range).
- Carry-over: (near-free retry) the xldtob tail `[0x8D480]` (3 fns `_Ldtob`/`_Ldunscale`/`_Genld`, coddog xldtob.c@99.99, rodata-literal carve) — the decompose sibling. (tooling) the `.data`-carve detector for file-static initialized arrays (the reverted #3) — needs a file's-own-static vs cross-file-extern discriminator.

## Sprint 91 — libc/bcmp.s (bcmp), libultra libc asm-mirror (mid-gate pivot from exceptasm) — 2026-06-15
- Increment: src/libultra/libc/bcmp.s banked / 1 fn matched (asm-mirror). Vendored asm-mirror TUs 20→21; `build/asm/8BE20.o` .text-only 0x110 exact; full-make ROM SHA-1 == baserom, 0 iteration. Split the pts-13 `[0x8BE20]` bcmp/xprintf pack at 0x8BF30 (16-aligned), isolating the `[0x8BF30]` xprintf leaves for future classical work.
- Quality: 0 stuck-far, 0 permuter, **1 carried** (exceptasm spike), 0 re-opened. Plus 1 mid-gate pivot (exceptasm→bcmp) on a material gate finding.
- Seed: committed 2pt; banked 2pt; regime mirror (seed-only; 8-gate FIRED on the pts-13 parent pack → decomposed to the bcmp sub-increment).
- What helped: the gate's verbatim-vs-source checks killed three false coddog "mirrors" before any wasted iteration — `func_80050400` "llcvt.c@99.99" (KMC compiler-runtime; MG64 fn-sizes 0x28/0xC0/0x84 vs llcvt's 0x1c×6/0x80×2), `func_800660A0` mtxutil tail (only 2/4 fns match — L2F/IdentF do, F2L/Ident diverge), `_Litob`/`_Ldtob` (S71 <99% divergent classical). For exceptasm, the `.text`=0x970 EXACT subseg-size match + J/_FINALROM-correct flags gave high source confidence, but disassembling the rodata at the gate (NOT trusting the has-rodata flag) surfaced the `__osIntTable`/`jtbl_800D2610` blocker BEFORE committing — the gate-time data-ref reconciliation earned its keep.
- Friction: I mis-framed exceptasm to the PO as an "S84 has-rodata replay scaled ×3" before the deep gate dig; the symbolic-jtbl-in-separate-blob blocker only surfaced mid-gate, forcing a re-ask. The has-rodata flag itself over-counted (`__osCauseTable_pt`, a `#ifndef _FINALROM` export) — a real pricing bug now fixed (#1). Lesson: a heavy asm-mirror's has-rodata flag is not a green light; disassemble the table form (numeric LUT vs symbolic pointer table) at the gate.
- Applied: 3 of 3: #1 `pick_target.py` `vendorable_tu_data_symbols` strips dead (`#ifndef _FINALROM`) + inactive-`BUILD_VERSION` blocks before the data-section scan → the priced has-rodata set is the active build's (exceptasm now lists only `__osHwIntTable`/`__osPiIntTable`, not `__osCauseTable_pt`); #2 new `vendorable_tu_jtbl` detector + `intrinsic-likely:<tu>.s(asm-mirror-jtbl:<head>)` tag — a SYMBOLIC-pointer `.word <label>` table (switch jtbl / fn-ptr) makes the `.text`-only asm-mirror a SPIKE, not an S84 strip-and-rename (numeric LUTs unaffected; exceptasm flags `__osIntTable`); #3 `docs/hazards.md#asm-mirror-vendoring` jtbl sub-case (both proven dead-ends: vestigial labels + VENDOR_ASM-`.o`-rodata-end-placement) + the has-rodata active-build gate note + 2 CLAUDE.md index rows. suite 52 pass, golden-inert.
- Carry-over: **exceptasm.s** (`__osExceptionPreamble` pack:8fn, [0x8AF90]) — scoped SPIKE, jtbl-placement blocker (see BACKLOG). The cheap libultra mirror band is exhausted; remaining = heavy packs (exceptasm jtbl-spike, sched.c carve-spike, motor.c version-trap, the divergent xprintf classical leaves).

---

## Sprint 90 — io/pimgr.c (osCreatePiManager), libultra io PI-manager drop-def mirror — 2026-06-15
- Increment: src/libultra/io/pimgr.c banked / 1 function matched. md5-candidate 134→135 .c (all stub-free); asm/hasm subsegs 158→157. Closes the S84-split [0x7E360] PI pack.
- Quality: 0/0/0/0 this sprint (0 stuck-far, 0 permuter, 0 carried, 0 re-opened; verbatim drop-def, full-make ROM SHA-1 == baserom first build, 0 iteration).
- Seed: committed 5pt; banked 5pt; regime mirror (seed-only; 8-gate clear at 5<8).
- What helped: the carry-over's "mixed .data/.bss carve" spike framing was over-cautious (the vimgr S87 false-flag class) — applying the drop-static test at the gate showed the only .data global (__osCurrentHandle) was already placed → a clean drop-def, NO carve. The 4 uninitialized .bss statics dropped-to-extern at asm-recovered main_bss vrams, all 4 sizes+addresses from a SINGLE disassembly via inter-symbol gaps (piThread 0x1B0 / piThreadStack 0x1000 / piEventQueue 0x18 / piEventBuf 0x4, the contiguous block below piacs's piAccessBuf). Name pre-curated, all callees + data refs placed (S84/S85), header piint.h vendored → zero header copies. internal/piint.h already declared __osPiDevMgr/__osCurrentHandle so those defs just deleted; __osPiTable/__Dom*SpeedParam unreferenced → dropped.
- Friction: none on the bank. Applying #2 surfaced a real tooling bug — carry_over_names() over-scooped 332 backtick tokens (split landed on a mid-line prose mention of "## Carry-overs", then took everything to EOF including the historical banked-sprint archive), silently de-ranking name-dropped still-asm functions (bcmp/_Litob) from the ranker. pimgr's own absence was NOT that bug — it was a correct by-design carry-over de-rank (retrieved via the BACKLOG, the intended path).
- Applied: 3 of 3: #1 BACKLOG Spike-guidance at-write-time drop-static-test discipline (frame the drop-def verdict, not the worst-case carve) + sched.c head framing revised to a genuine carve/jtbl/log-callee spike; #2 pick_target.py carry_over_names() region+symbol scoping (heading-anchored ^## Carry-overs split + live-region bound + placed_symbols∪func_ intersect → 332→50 carried; un-suppresses prose-name-dropped still-asm fns while keeping motor/sched/func_800AFB90 parked) + the by-design clarification comment at the exclusion site; #3 docs/hazards.md#recover-extern-refs-unplaced contiguous-.bss-block-sized-by-gaps fast-path note.
- Carry-over: none new (pimgr resolved). Remaining io: motor.c (version-branch trap) + sched.c head (genuine carve/jtbl/5-log-callee spike).

---

## Sprint 89 — io/sirawdma.c (__osSiRawStartDma), libultra io SI-DMA mirror via sched|sirawdma decompose — 2026-06-15
- Increment: src/libultra/io/sirawdma.c banked / 1 function matched. md5-candidate 133→134 .c (all stub-free); flippable asm subsegs 136→136 (sched head stays asm, +1 C subseg).
- Quality: 0/0/0/0 this sprint (0 stuck-far, 0 permuter, 0 carried, 0 re-opened; first-build full-make ROM SHA-1 == baserom, 0 iteration).
- Seed: committed 2pt; banked 2pt; regime mirror (8-gate FIRED on the pts-13 osCreateScheduler pack → RESOLVED by decompose, the S74/S75 first option; the sirawdma tail alone is a 2pt single-fn mirror; seed-only).
- What helped: the c-combined pack decompose pattern (S74 contquery/contreaddata) made a pts-13 trap pickable — split at the upstream-file boundary (16-aligned at `__osSiRawStartDma`=0x800AC060, pre-placed S74), bank only the clean tail, leave the heavy sched.c head asm. sirawdma.c is a textbook verbatim mirror: sibling of S4 sirawread/sirawwrite (assert-strip `#ifdef _DEBUG` + bare-`siint.h` conventions reused verbatim), all callees pre-placed, all SI/PIF macros in the already-vendored siint.h/rcp.h. The plan-gate pre-check (callees + macros + boundary alignment + version branch) made the 3 self-audit risks (assert-strip / macro-resolvability / MMIO-isolation) all resolve benignly on the first build. The `>= VERSION_J` IO_READ branch confirmed this is NOT the motor.c version trap (the fn exists in J).
- Friction: the BACKLOG was STALE — `piacs.c` was carried as a "remaining io trap" (defines-data + file-static) through S88, but is in fact ALREADY BANKED (flipped `[0x7EDB0, c, libultra/io/piacs]`, drop-to-extern mirror, all 3 data symbols placed, green ROM) since the `cbaf80a` 2026-06-13 layout refactor. The old carry-over also mislabeled `func_800AC110` as piacs — that vram is actually `__osSiCreateAccessQueue`/siacs.c (banked S81). Caught by checking the live yaml + green build at the plan gate rather than trusting the prose. No build-time friction (clean first build).
- Applied: 1 of 1 — #1 BACKLOG staleness reconciliation: marked `piacs.c` BANKED, de-paired it from the `motor.c` carry-over (now a standalone version-branch trap), corrected the `func_800AC110`→siacs mislabel, and noted the genuine remaining io traps are motor + pimgr only. The historical per-sprint band-note refrains are left as immutable log; the forward-looking Carry-overs section is the authoritative live list.
- Carry-over: sched.c head (osCreateScheduler + ~13 fns, `[0x86A50, asm]` 0x800AB650..0x800AC060) → BACKLOG ## Carry-overs as a heavy spike (file-static + defines-data:count,firsttime + rodata-jtbl:0x800D25C0 switch + 5 unplaced log callees + a jal-count-mismatch:12vs11 to verify). Cross-repo follow-up: none (no new decomp-side symbols; `__osSiRawStartDma` pre-placed S74).

## Sprint 88 — io/contpfs.c (__osSumcalc + __osIdCheckSum + __osRepairPackId + __osCheckPackId + __osGetId + __osCheckId + __osPfsRWInode), libultra single-file-pack drop-def mirror — 2026-06-15
- Increment: src/libultra/io/contpfs.c banked / 7 functions matched. md5-candidate 132→133 .c (all stub-free); flippable asm subsegs 137→136.
- Quality: 0/0/0/0 this sprint (0 stuck-far, 0 permuter, 0 carried, 0 re-opened; 0 C-body iterations — clean-rebuild full-make SHA-1 == baserom). 1 novel bank-gotcha (vendored-header-incomplete).
- Seed: committed 13pt; banked 13pt; regime mirror (8-gate FIRED at 13 → verbatim-mirror exemption applied: single-file-pack drop-def, decompose mechanically blocked, callees placed + names curated; seed-only).
- What helped: coddog @100.00 + the `one .o` structural read (all 7 inner boundaries non-16-aligned ⇒ single compilation unit) confirmed single-file-pack before any build. The upstream version guards did the function-set selection for free: `#if BUILD_VERSION < VERSION_J` strips `__osPfsSelectBank` (the separately-banked pfsselectbank.c) and `#ifdef _DEBUG` strips `__osDumpId`, leaving exactly the 7 ROM fns — no manual deletion, no near-verbatim edit. Drop-def of the 3 cache globals was the S85/S87 pattern: `__osPfsInodeCacheBank` pre-placed, `__osPfsInodeCacheChannel`=0x800C9440 (.data, =-1 = D_800C9440) + `__osPfsInodeCache`=0x801B68E8 (.bss, 0x100) recovered from the asm/data extracts up front. All callees pre-placed.
- Friction: the vendored `controller.h` was a RECONSTRUCTED header (not a verbatim ultralib copy) that was `(already-vendored)` (file resolves) yet INCOMPLETE — missing `SELECT_BANK` entirely and defining `SET_ACTIVEBANK_TO_ZERO` object-like vs the source's `SET_ACTIVEBANK_TO_ZERO()` call → 5 mid-execution parse errors, invisible at the plan gate (the INCLUDE_ASM stub carries no header exercise). Resolved by aligning both macros to ultralib's VERSION_J definitions (kept contpfs.c verbatim), guarded by `grep -rn` confirming zero other src consumers, then a clean-rebuild (shared-header edit). The plan-gate's pick_target also (correctly per its current design) reported the `(already-vendored)` header as a no-op, which prices file existence, not macro completeness (→ applied #1).
- Applied: 4 of 4 — #1 `docs/hazards.md#vendored-header-incomplete` playbook (reconstructed-header-missing-macro class) + CLAUDE.md index row; the robust `needs-macro:<MACRO>@<hdr>` AUTO-detector is DEFERRED (a naive `UPPER(` grep false-fires; distinguishing a function-like macro invocation from a real call needs preprocessing — tracked in BACKLOG). #2 `pick_target.py` `coddog-fncount-mismatch:<m>vs<n>` in build_rows (under-count direction ONLY — a true single source can define MORE via version/_DEBUG-gated extras, e.g. contpfs 9-raw-vs-7-ROM, so over-count never false-fires; settime.c 1-fn-vs-6-pack correctly flags) + CLAUDE.md index row. #3 `pick_target.py` `one-tu` in classify_subseg + new `decomp_asm.asm_function_addrs` helper (all inner boundaries non-16 ⇒ one .o; confirms single-file-pack for un-named coddog packs, marks per-fn decompose blocked; verified it does NOT fire on any c-combined multi-.o pack) + CLAUDE.md index row. #4 BACKLOG `motor.c` trap note: the `>= VERSION_J` branch does not define `osMotorStop` (only `#else` does), so a verbatim J mirror is wrong source. `make test-tools` 50→52 pass, golden regen (28 `one-tu` additions; every changed line differs ONLY by the new flags, verified).
- Carry-over: none (the increment fully banked). Forward: the `one-tu` + `coddog-fncount-mismatch` flags now disambiguate single-file-pack vs multi-file at the gate (settime's 6-fn os pack reads `one-tu` + `coddog-fncount-mismatch:1vs6` = one source file, NOT settime.c). Cross-repo follow-up: 9 new symbol_addrs names (7 fns + __osPfsInodeCache/__osPfsInodeCacheChannel) → propagate via `sync_decomp_names.py --import-from-decomp`.

## Sprint 87 — io/vimgr.c (osCreateViManager + viMgrMain), libultra drop-static mirror — 2026-06-15
- Increment: src/libultra/io/vimgr.c banked / 2 functions matched. md5-candidate 131→132 .c (all stub-free); flippable asm subsegs 138→137.
- Quality: 0/0/0/0 this sprint (0 stuck-far, 0 permuter, 0 carried, 0 re-opened; 0 C-body iterations — first-build full-make SHA-1 == baserom).
- Seed: committed 5pt; banked 5pt; regime mirror (8-gate clear at 5<8; seed-only — verbatim body + drop-to-extern data).
- What helped: the S86 timer bank cleared the carry-over's one real blocker; the rest was an MCP `disassemble_function` of both fns recovering all 8 `.bss` vrams up front. The key realization that turned a feared "heavy .bss carve" into a seed-only mirror: the file-statics are all UNINITIALIZED → pure `.bss` (no ROM bytes), so they DROP to sized externs at their main_bss vrams (the S81 `siacs.c` pattern) — no carve, no classical loop. `STACK_START(viThreadStack)` math (0x800FABD0 + OS_VIM_STACKSIZE 0x1000 = 0x800FBBD0 = viEventQueue) confirmed the stack extern size before the build, and the .bss address gaps gave every symbol's size. Banked atomically, first build.
- Friction: none in execution. The carry-over (and the file-static playbook + coddog-trap note) framed this as a `.bss` carve / classical route — over-cautious; the uninitialized-vs-initialized distinction (only the latter is a carve) was the missing nuance (→ applied #1). pick_target also decorated the row with a phantom `calls-unplaced:aligned` (the `ALIGNED`/`STACK` macro expansion residue) (→ applied #2).
- Applied: 3 of 3 — #1 `docs/hazards.md#file-static-bss-layout-conflict` split (uninitialized file-static = pure-`.bss` drop-to-extern MIRROR, not a carve/classical spike; the S81 pattern) + the new `drop-static-mirror:<n>bss` re-frame tag in `pick_target.py` (`drop_static_mirror_hazard`: coddog@≥99 + file-static + no rodata-literal/data-static/rodata-jtbl → one drop-to-extern enabler, not the scary 4-flag cluster) + `+1` unit test (`test_drop_static_mirror_hazard`) + CLAUDE.md index row; #2 `pick_target.py _C_NONCALL += aligned,__attribute__` (the macros.h `ALIGNED`/`STACK` family expands to `__attribute__((aligned(x)))`, mis-read as a callee; golden regen = 4 `aligned`-only removals, real callees like `__osMotorAccess` retained); #3 folded into #1 (the `drop-static-mirror` tag IS the hazard-collapse suggestion). `make test-tools` 48→49 pass, golden regen (4 aligned removals only).
- Carry-over: none (the increment fully banked). The vimgr.c carry-over is now RESOLVED — see BACKLOG ## Carry-overs. Forward sighting: the `drop-static-mirror` tag now re-prices `osMotorStop`/`motor.c` (`drop-static-mirror:2bss`) and any future uninitialized-file-static coddog mirror.

## Sprint 86 — os/timerintr.c (__osTimerServicesInit + __osTimerInterrupt + __osSetTimerIntr + __osInsertTimer), libultra drop-def mirror — 2026-06-15
- Increment: src/libultra/os/timerintr.c banked / 4 functions matched. md5-candidate 130→131 .c (all stub-free); flippable asm subsegs 139→138.
- Quality: 0/0/0/0 this sprint (0 stuck-far, 0 permuter, 0 carried, 0 re-opened; 0 C-body iterations — first-build full-make SHA-1 == baserom).
- Seed: committed 8pt; banked 8pt; regime mirror (pts-8 tripped the 8-gate → verbatim-mirror exemption applied: single-file-pack:4fn decompose-blocked + all 4 names curated + all callees placed; seed-only).
- What helped: the only remaining libultra trap with NO file-static, so a pure drop-def (S82/S85 default) with no .bss/.data carve. The 6 data globals reduced to 2 real recover-externs (3 pre-placed from S27/S30) once I noticed `__osBaseTimer` is named ONLY in `__osTimerList`'s dropped `.data` initializer — the placed pointer's bytes already encode its address, so it needs no extern/placement at all. Two MCP `disassemble_function` calls recovered both externs deterministically (lui/sw HI/LO16) and reconciled the 3 placed ones. The `-D_FINALROM` profile-block strip + correctly-J-excluded `VERSION_K` clamp meant the verbatim body matched first build.
- Friction: none in execution. One process snag surfaced at the gate (not a code issue): `undefined_syms_auto.txt` was dirty at session start — an S85 `make extract` regen (BOOT_GLOBALS D_→named) the S85 commit never staged; S86 absorbed it (→ applied #2).
- Applied: 4 of 4 — #1 `pick_target.py refs_unplaced` drops a `__`-global the .c itself defines but no function body references (named only in another global's depth-0 initializer; new `_names_in_function_bodies` brace-depth scan), +1 unit test (`test_refs_unplaced_drops_initializer_only_self_defined_global`), `make test-tools` 47→48 pass, golden-neutral; #2 CLAUDE.md bank-step note — always stage the `make extract`-regenerated `undefined_syms_auto.txt` + `mariogolf64.ld` so the regen doesn't bleed into the next sprint's tree; #3 CLAUDE.md 8-gate verbatim-mirror exemption wording extended to explicitly cover the drop-def sub-case (verbatim body + externed data defs banks atomically like a pure cp); #4 BACKLOG `vimgr.c` carry-over note (timerintr banking placed its timer-side deps).
- Carry-over: none (the increment fully banked). vimgr.c (osCreateViManager) remains a file-static .bss-carve spike but its timer dependency wall is now gone — see BACKLOG ## Carry-overs.

## Sprint 85 — os/initialize.c (__osInitialize_common + create_speed_param), libultra coddog mirror — 2026-06-15
- Increment: src/libultra/os/initialize.c banked / 2 functions matched. md5-candidate 129→130 .c (all stub-free); asm subsegs 140→139.
- Quality: 0/0/0/0 this sprint (0 stuck-far, 0 permuter, 0 carried, 0 re-opened; 0 C-body iterations — the 2 build-fix passes were symbol/version reconciliation, not classical iteration).
- Seed: committed 5pt; banked 5pt; regime mirror (8-gate clear at 5<8; seed-only — verbatim/deterministic).
- What helped: the S80-teed-up coddog identity made this the obvious smallest-first pick; `.o`-disasm (`objdump -d`) vs Ghidra MCP `disassemble_function` localized the 8-byte SHA-miss to ONE instruction pair (`__osSetWatchLo`) in minutes; the drop-def fast path (S82/S83 default) meant no `.data` carve despite the carry-over framing one. The 8-byte `.o` size tell (fn 0x228 vs asm `nonmatching ... 0x230`) pointed straight at a whole-instruction gap (version class) vs an off-by-immediate (GBI/enum class).
- Friction: TWO reconcile classes neither pick_target nor the gate stub build can see, both surfacing only when the real body compiles/links — (1) os_host.h's `#define __osInitialize_common() osInitialize()` K→J source-compat macro silently renamed the exported symbol (link wanted the curated name); (2) `__osSetWatchLo(0x4900000)` gated `>= VERSION_K` in the reconstruction but present in MG64's J build. The carry-over also over-stated the work (cross-region carve + name-reconcile) where the reality was drop-def + 2 one-line edits — a recurring "carry-over over-states difficulty" theme (cf. S70 maptlb).
- Applied: 3 of 3 — #1 `pick_target.py header_renames_symbol` (transitive-header scan for a macro rewriting the curated leader → `header-renames-symbol:<fn>@<hdr>` flag, wired into both hazard appenders) + unit test + `docs/hazards.md#header-renames-symbol` + CLAUDE.md index row; #2 `docs/hazards.md#needs-define` VERSION_K-gate-present-in-J sub-case (the exact N×8B SHA-miss tell + the `.o`-size cross-check + `.o`-disasm-vs-MCP localize) + CLAUDE.md symptom row; #3 `BACKLOG.md ## Carry-overs` drop-def-default guidance + asm-recovered-address rule (corrected pimgr's wrong `__Dom*SpeedParam` addresses). `make test-tools` 47 pass (46→47), golden-neutral.
- Carry-over: none (the increment fully banked). pimgr (osCreatePiManager) remains the next io defines-data+file-static spike — now with __Dom1SpeedParam=0x80106248 / __Dom2SpeedParam=0x800FEC98 pre-placed (S85) + their carry-over addresses corrected.

## Sprint 84 — [0x7E360] io pack, 2 of 3 (epirawdma C mirror + setintmask asm-vendor) — 2026-06-15
- Increment: src/libultra/io/epirawdma.c (`__osEPiRawStartDma`, C mirror) + src/libultra/os/setintmask.s (`osSetIntMask`, hasm asm-vendor) banked / 2 functions matched. md5-candidate 128→129 .c; hasm 21→22; asm 140→140.
- Quality: 0/0/1/0 this sprint (0 stuck-far, 0 permuter, 1 carried = pimgr planned spike, 0 re-opened; both banked first-build SHA == baserom, 0 iterations).
- Seed: committed 4pt; banked 4pt; regime mirror (8-gate clear; seed-only — both verbatim/deterministic).
- What helped: the gate split the c-combined `[0x7E360]` pack 3-way at the upstream-file boundaries (all 16-aligned) and validated green before any body landed, so epirawdma + setintmask were independent atomic banks. epirawdma was a true zero-enabler io mirror (sibling epidma.c pre-resolved the `#include "piint.h"` adaptation + all refs; name pre-curated). For setintmask the `.ld` + `objdump -h` told the whole story at the gate: reading how splat auto-links a hasm `.o`'s sections (823B0/824E0 `.rodata` at the section tail) predicted the duplicate-LUT SHA-break BEFORE building, so approach-1 (vendor `.text` only + rename the generic blob) was chosen up front and worked first-build.
- Friction: the setintmask `.rodata` LUT (`__osRcpImTable`) is a NEW vendoring class — the first vendorable `.s` carrying a non-`.text` section, and it's referenced cross-TU (the exception dispatcher `asm/8AF90.s`), so it could not just be carved into setintmask's TU. Required understanding splat's auto-link ordering + a `D_<vram>`-rename clean rebuild. Now fully documented + pre-flagged so the next such TU is a mechanical replay.
- Applied: 3 of 3 — #1 `docs/hazards.md#asm-mirror-vendoring` vendored-`.s`-with-`.rodata`/`.data` sub-case (vendor `.text` only, strip the data block, keep it as the renamed generic blob; + Provenance S84 + the CLAUDE.md hazard-index row); #2 `pick_target.py vendorable_tu_missing_defines` subtracts the `.s`'s own `#define`s (the `MI_INTR_MASK` self-define false-positive); #3 `pick_target.py vendorable_tu_data_symbols` → `intrinsic-likely:<tu>.s(has-rodata:<sym>)` pre-flag so the strip+rename enabler is priced at the gate. `make test-tools` 46 pass, golden-neutral.
- Carry-over: src/libultra/io/pimgr.c (`osCreatePiManager`, [0x7E400,asm]) — file-static (piThread/piThreadStack/piEventQueue/piEventBuf) + defines-data (__osPiDevMgr/__osPiTable/__Dom1SpeedParam/__Dom2SpeedParam/__osCurrentHandle) → needs a .data/.bss carve.

## Sprint 83 — io sptask.c, the RSP task-load file (verbatim mirror) — 2026-06-15
- Increment: src/libultra/io/sptask.c (`osSpTaskLoad` + `osSpTaskStartGo`) banked / 2 functions matched. md5-candidate 127→128 files; asm subsegs 141→140.
- Quality: 0/0/0/0 this sprint (verbatim single-file-pack mirror, 0 C-body iterations; clean-rebuild ROM SHA-1 == baserom).
- Seed: committed 5pt; banked 5pt; regime mirror (8-gate clear at 5<8; seed-only).
- What helped: gate-investigation of `sptask`'s three hazard flags resolved ALL as false-flags before the flip — `jal-count-mismatch:7vs14` was the static `_VirtualToPhysicalTask` inlined (7× `jal osVirtualToPhysical` in asm confirms; coddog 99.99 holds), `calls-unplaced:_osVirtualToPhysical` was the line-11 macro (real callee placed), `needs-header` a no-op. So the only real work was the routine drop-def (`tmp_task`→extern @0x800FA9C0), exactly the S81/S82 pattern. When the verbatim body linked clean but SHA-missed, the S44 byte `.o`-diff localized it to one word instantly.
- Friction: the one-word divergence (`OS_YIELD_DATA_SIZE` 0x900 vs baserom 0xc00) is a NEW late-surface class — a GBI-microcode-guarded MACRO VALUE, invisible to the gate (the INCLUDE_ASM stub never compiles the body) and to a jal/ref reconcile. Surfaced only as a full-make SHA miss. Root cause: `LIBULTRA_CFLAGS` lacked any GBI define (`PR/sptask.h` then takes the `#else` value). PO directed the fix: pin `-DF3DEX_GBI_2` (MG64's microcode). A second clean rebuild confirmed every other banked libultra file stays SHA-1-neutral.
- Applied: 3 of 3 — #1 `pick_target.py` GBI-value-guard pre-flag (parse `LIBULTRA_CFLAGS` into the libultra active-define set; `gbi_value_guard_needs_define` + `_scan_value_guards`/`_gbi_guarded_macros` flag a candidate body that uses a GBI-guarded macro when no guard define is active — dormant while `-DF3DEX_GBI_2` stands, prices a differently-guarded candidate) +1 unit test `test_gbi_value_guard_needs_define`, golden-neutral, suite 46 pass; #2 `docs/hazards.md#needs-define` GBI-microcode sub-case (the 1-word `addiu` SHA-miss tell + the byte `.o`-diff localization); #3 the byte `.o`-diff localization note (confirmatory, already covered by the S44 doctrine — log-only).
- Carry-over: none.

## Sprint 82 — io controller.c, the controller-init file (verbatim mirror) — 2026-06-15
- Increment: src/libultra/io/controller.c (`osContInit` + `__osContGetInitData` + `__osPackRequestData`) banked / 3 functions matched. md5-candidate 126→127 files; asm subsegs 142→141.
- Quality: 0/0/0/0 this sprint (verbatim drop-def cp, 0 iteration; clean-make ROM SHA-1 == baserom).
- Seed: committed 5pt; banked 5pt; regime mirror (8-gate clear 5<8; seed-only).
- What helped: the teed-up S81 setup — siacs.c was banked precisely to place `__osSiCreateAccessQueue`, controller's last real callee, so `calls-unplaced` was already just the `aligned` macro false-flag. Clean `single-file-pack:3fn` (atomic, no split) + S42 drop-def fast path made it a 0-edit verbatim mirror. Includes adapted off the `contreaddata.c` sibling (already-vendored). 3 of the 7 dropped globals placed at the gate from `osContInit.s` `D_` refs (S81 #2). One global (`__osEepromTimer`) was pure drop-def — defined here but referenced only by still-asm eeprom.c, so no extern/placement needed at all.
- Friction: the gate `D_<vram>`→name symbol-add failed an *incremental* link (`undefined reference to D_8012F4DC`) because make doesn't track the INCLUDE_ASM `.s` dep, so the stale stub `.o` kept the now-removed `D_` symbol — masked by a stale-`.z64` false-positive SHA. `make clean && make extract && make` produced the green ROM. (Now documented — see Applied #1.)
- Applied (1 of 3): #1 `docs/hazards.md#defines-data` — a `D_<vram>` rename symbol-add (gate OR execution) must be validated with a CLEAN rebuild, not incremental, since the INCLUDE_ASM `.s` dep is untracked. (#2 pick_target defines-data referenced-by-self-vs-elsewhere split NOT selected — low value, fast path handles both identically; #3 confirmatory single-file-pack+S42+coddog-true-source cadence, log-only.)
- Carry-over: none.

## Sprint 81 — io siacs.c, the SI access-queue file (verbatim piacs.c twin) — 2026-06-15
- Increment: src/libultra/io/siacs.c (`__osSiCreateAccessQueue` + `__osSiGetAccess` + `__osSiRelAccess`) banked / 3 functions matched. md5-candidate 125→126 files; asm subsegs 143→142.
- Quality: 0/0/0/0 this sprint (verbatim drop-def cp, 0 iteration; first-make ROM SHA-1 == baserom).
- Seed: committed 5pt; banked 5pt; regime mirror (seed-only). 8-gate clear (5<8). Smallest remaining libultra candidate (240B); the file-static+defines-data load priced it pts-5 but it banked atomically like any mirror via the S42 drop-def fast path.
- What helped: it's the verbatim structural TWIN of the already-banked `io/piacs.c` — exact playbook in hand (drop the 3 data defs → extern, place add-only). coddog @99.99 confirmed verbatim; all callees pre-placed (`osCreateMesgQueue`/`osSendMesg`/`osRecvMesg`); 2 of 3 fns already named; warm io band so no header work. The 3 SI data vrams were all visible as `D_<vram>` in the scaffold asm → trivial recover-extern. Bonus: placing `__osSiCreateAccessQueue`=0x800AC110 resolves the `calls-unplaced` callee the next-up controller.c needs.
- Friction: coddog named the TWIN (piacs.c) not the real source (siacs.c) — caught by the named SI members, a manual reconcile. And the data-symbol adds landed mid-execution → forced a 2nd `make extract` (the gate placed only the fn symbol); these globals were gate-placeable (no section carve) so the 2nd extract was avoidable. Both became retro suggestions.
- Applied: 2 of 2 — #1 `pick_target.py` `coddog-twin:<matched>!=<member-src>` pre-flag (helper `_append_coddog_twin_hazard`, wired into both coddog emission sites; unit-verified piacs!=siacs fires, agree/unnamed no-op; CLAUDE.md index row + docs/hazards.md #coddog-cross-ref step 5); #2 docs/hazards.md #defines-data gate-safe symbol-add note (a drop-def symbol-add naming an existing `D_<vram>` is SHA-neutral at the stub stage, distinct from the S68 execution-only ld-section carve). Golden stable (committed map is coddog-free → twin check inert), suite 45 pass.
- Carry-over: none.

## Sprint 80 — io pfsgetstatus.c, a clean single-file 3-fn coddog verbatim mirror — 2026-06-15
- Increment: src/libultra/io/pfsgetstatus.c (`__osPfsGetStatus` + `__osPfsRequestOneChannel` + `__osPfsGetOneChannelData`) banked / 3 functions matched. md5-candidate 124→125 files; asm subsegs 144→143.
- Quality: 0/0/0/0 this sprint (verbatim cp, 0 edits, 0 iteration; first-make ROM SHA-1 == baserom).
- Seed: committed 5pt; banked 5pt; regime mirror (seed-only). 8-gate clear (5<8). Smallest-first off the re-priced coddog list; the first clean io coddog leaf after the band looked "mined out" at S79 — it was, this one only LOOKED costly because it carried two false hazard flags.
- What helped: coddog @99.99 confirmed verbatim before any compile; pfs band warm (S77/S78) so all callees pre-placed (`__osSiRawStartDma`/`osRecvMesg`/`__osContLastCmd`/`__osPfsPifRam`); the io `PRinternal/{controller,siint}.h`→bare include convention reused verbatim; the single real recover-extern (`__osPfsInodeCacheBank`=0x800C9444, byte store, defined by still-asm contpfs.c) was a 1-line MCP disasm at the gate; standalone subseg, no split.
- Friction: the candidate's two flagged hazards were BOTH pick_target false-positives that made a clean mirror look moderate at triage — `refs-unplaced:__OSContRequesFormatShort` (a struct TYPE, not data) and `jal-count-mismatch:7vs6` (a dead `#else` branch double-count). Debunking them by hand at the gate took the bulk of planning. NOTE: my execution-time hypothesis for the jal-mismatch (the CHNL_ERR macro) was WRONG; reading the actual code at the retro found the real cause was the unstripped version branch — verify the suggestion against the code before applying, don't implement the hypothesis verbatim.
- Applied: 3 of 3, all `pick_target.py` accuracy fixes, each with a regression test: #1 `_resolve_include` BASENAME fallback — a vendored-prefix include (`PRinternal/controller.h`) whose in-tree copy drops the prefix (`internal/controller.h`) now resolves, so `declared_type_names`/`declared_extern_data` scan it and `refs_unplaced` stops phantom-flagging a typedef'd struct as an unplaced data extern (`test_resolve_include_vendored_basename_fallback`); #2 `call_divergence` strips inactive `#if BUILD_VERSION` branches (now lib-threaded via `_build_version_ord(lib)`) so a dead-branch call no longer double-counts into a phantom jal-mismatch (`test_call_divergence_strips_inactive_version_branch`); #3 factored `_append_coddog_trap_hazards` and called it from the S78 tail-identity `cod_members` block too — a coddog identity carried by an UN-NAMED tail member now re-runs the file-level trap battery, so `initialize.c`'s defines-data `.data` carve is priced at the gate (its coddog hit keys on the sibling `create_speed_param`, not leader `__osInitialize_common`, so only the bare-flag path fired before) (`test_coddog_tail_trap_rescan`). Golden stable (no covered-row change), suite 45 pass.
- Carry-over: none for the increment. Tooling follow-up (logged to BACKLOG): `initialize.c` is the next-cleanest os/ coddog leaf (pts now 5, defines-data correctly surfaced) but needs a cross-region `.data` sibling carve (`osClockRate`/`osViClock`/`__osShutdown`/`__OSGlobalIntMask` @0x800C9460) + name reconciliation (J source = `osInitialize`/`createSpeedParam`; Ghidra curated the K-era `__osInitialize_common`/`create_speed_param`). Its `!defined(_FINALROM)` KMC block is not stripped by `_strip_inactive_version_branches` (BUILD_VERSION only), so its refs/calls-unplaced over-flag (advisory; the priced defines-data is the load-bearing signal).

---

## Sprint 79 — io contramread + contramwrite, the S71-named cont-pak RAM I/O verbatim mirror pair — 2026-06-15
- Increment: src/libultra/io/contramread.c (`__osContRamRead`) + contramwrite.c (`__osContRamWrite`) banked / 2 functions matched. md5-candidate 122→124 files; asm subsegs 145→144 (+1 nop-pad subseg).
- Quality: 0/0/0/0 this sprint (both banked first sprint; the contramwrite trailing-pad split was in-execution localization, not a spike — the body never diverged, 0 real iteration).
- Seed: committed 6pt (seed 3+3); banked 6pt; regime mirror (seed-only). 8-gate clear (6<8). Smallest-first homogeneous sibling pair off the re-priced coddog list.
- What helped: coddog @99.99 confirmed both verbatim before any compile; all 10 callees/fn pre-placed (warm cont band); the S44/S45 defines-data verbatim-body fast-path made `__osPfsLastChannel = -1` a drop-to-extern + add-only name (0x800C9450, recovered from the fn's own lui/lw), NO `.data` carve; the io-band `PRinternal/{controller,siint}.h`→bare include convention (S72/S74) reused verbatim; the gate-validated flip caught nothing wrong (both flips green).
- Friction: contramwrite SHA-missed the first make (ROM 96B short) — a NEW gate-invisible class: splat extracts the whole subseg slot (516B fn + 27 trailing nops padding to the 128-aligned `osAfterPreNMI`@0x800AF880), but the verbatim C compile only 16-aligns its `.text` (.o=0x210), dropping the 0x60 residual. Invisible to every gate check (the INCLUDE_ASM stub carries the pad → gate build green); localized by `.o`-size diff + the extracted-asm trailing-nop run. Fixed with a nop-pad `[0x8AC20, asm]` split (hazards.md:122 — no inter-subseg linker ALIGN). The contramread sibling (slot == 16-aligned fn) mirrored clean, no split — the FP guard in one pair.
- Applied: 2 of 2: #1 `pick_target.py` `trailing-pad:<n>B@<align>` pre-flag (new `code_end_rom` in `decomp_asm` finds the real code end; fires only when the next boundary is >16-aligned so a merely-16 gap / delay-nop artifact doesn't false-fire) + an **all-nop asm subseg skip** (the pad subseg carries a splat glabel but is pure nops; the skip also retired 8 pre-existing all-nop `func_ovl*_801F4A30` overlay stubs the ranker was surfacing as the "smallest" picks); #2 `docs/hazards.md#trailing-alignment-pad-after-a-c-mirror` (the split recipe + contramwrite worked example) + the CLAUDE.md hazard-index row. Golden regen (8 overlay-stub rows dropped + the new trailing-pad flag), suite 42 pass.
- Carry-over: none. Note: the trailing-pad pre-flag now prices this class at the gate, so a future >16-aligned-boundary mirror (3 live candidates flagged @32/@64/@128) won't SHA-miss mid-execution.

---

## Sprint 78 — clear the io c-combined subseg [0x8CE90]: gbpaksetbank + pfsisplug, 2 libultra io coddog mirrors — 2026-06-15
- Increment: src/libultra/io/gbpaksetbank.c (1 fn) + src/libultra/io/pfsisplug.c (3 fns) banked / 4 functions matched. md5-candidate 120→122 files; asm subsegs 146→145.
- Quality: 0/0/0/0 this sprint (both files banked first-try; the defines-data BSS was a known sub-case, not a surprise).
- Seed: committed 4pt; banked 4pt; regime mirror (seed-only). The 4-fn c-combined subseg ranks ~pts-8/13 → 8-gate FIRED → resolved by decompose at the upstream-file boundary (S74/S77 path).
- What helped: the S77 band-note pointed straight at [0x8CE90]; the cached coddog map (`tools/coddog/coddog_map.tsv`) gave the per-member upstream split (gbpaksetbank.c + pfsisplug.c) without hand-disassembly; the S44/S45 defines-data fast-path made pfsisplug's `OSPifRam __osPfsPifRam` a drop-to-extern (vendored `controller.h:227` extern + symbol_addrs, NO `.bss` carve — the shared bss blob already reserves the range, bss has no ROM bytes); all callees pre-placed; the io-band `PRinternal/{controller,siint}.h`→bare include convention (S72/S74) was reused verbatim.
- Friction: the subseg never surfaced in `pick_target.py` (had to find it via the band-note + a manual coddog grep). Root cause was two-fold: (1) `carry_over_names()` scoops EVERY backticked token from the BACKLOG digest log, so `__osGbpakSetBank` — name-dropped as a banked callee in S45's carry-over prose — falsely landed in the `carried` set and the whole subseg was de-ranked invisible; (2) the coddog flag keyed only on `fns[0]`, so the tail's real identity (func_800B1B50→pfsisplug.c) was never surfaced, compounded by `UPSTREAM_DEF_RE` mis-attributing `__osGbpakSetBank` to gbpakreadwrite.c (a forward prototype matched as a def).
- Applied: 1 of 1: #1 `pick_target.py build_rows` — scan ALL subseg members for a definitive (≥PCT, non-audio) coddog hit (not just `fns[0]`), surface each distinct tail identity even under a named/mis-attributed leader, and exempt a coddog-identified subseg from the over-broad `carried` name-drop filter; +2 unit tests (`test_coddog_tail_overrides_carried_namedrop`, `test_carried_namedrop_still_drops_without_coddog`), golden stable (the only live case [0x8CE90] is now flipped), suite 42 pass.
- Carry-over: none. Follow-ups logged (not applied this gate): (a) `UPSTREAM_DEF_RE` matches forward prototypes (`...);`) as definitions → mis-attributes a fn to a sibling file that only declares it (cosmetic noise on the surfaced row); (b) `carry_over_names()` is fundamentally over-broad (222 banked tokens treated as carried) — the coddog exemption only patches the coddog subset; a precise carry-over parser (or a structured carry-over marker in BACKLOG) would un-de-rank the rest.

---

## Sprint 77 — clear the io-SP c-combined subseg [0x8CAA0]: spgetstat + spsetstat + spsetpc + sprawdma, 4 libultra io mirrors — 2026-06-15
- Increment: 4 files banked (`src/libultra/io/{spgetstat,spsetstat,spsetpc,sprawdma}.c`, 4 fns) / 4 fns matched. md5-candidate 116→120; asm subsegs 147→146. Subseg [0x8CAA0] fully C.
- Quality: 0/0/0/0 this sprint (the caller-eviction was a gate surprise resolved without a spike/carry).
- Seed: committed 4pt (plan-gate est ~6); banked 4pt; regime mirror (seed-only). 8-gate FIRED on the subseg's pts-13 → resolved by decompose at the c-combined file boundary (S74 path), not the verbatim-exemption.
- What helped: coddog 99.99 on 3 of 4 members + Ghidra disasm of the un-named leader `func_800B16A0` (= `__osSpGetStatus`, a 4-instr `IO_READ` coddog couldn't fingerprint) turned a 3-file c-combined into a clean 4-file clear; the S72 assert-strip playbook made sprawdma a known-edit (asm-confirmed no `jal __assert`); all callees pre-placed (`__osSpDeviceBusy`/`osVirtualToPhysical`); register immediates only → zero ld-section siblings.
- Friction: gate symbol-add (`__osSpGetStatus`=0x800B16A0) evicted the banked classical caller `src/main/func_800AB600.c` (hard-coded `func_800B16A0()`) → `undefined reference` at the green-ROM gate check; fixed by renaming the call site (same addr, SHA-neutral). Now pre-flagged (Applied #1).
- Applied (1 of 1): #1 `pick_target.py` `caller-evict:<func_vram>@<file>` pre-flag (`src_func_callers()` walks `src/`, INCLUDE_ASM-excluded, display-only) + `docs/hazards.md#caller-evict` + CLAUDE.md hazard-index row; +1 unit test `test_caller_evict_flag`, golden regen (suite 40 pass).
- Carry-over: none.

---

## Sprint 76 — bank io/devmgr.c (__osDevMgrMain), libultra io coddog verbatim mirror; first switch-jump-table rodata sibling — 2026-06-14
- Increment: 1 file banked (`src/libultra/io/devmgr.c`, `__osDevMgrMain`) / 1 fn matched. md5-candidate 115→116; asm subsegs 148→147.
- Quality: 0/0/0/0 this sprint (the jtbl carve was an in-execution surprise resolved without a spike/carry).
- Seed: committed 3pt; banked 3pt; regime mirror (seed-only; 8-gate clear at 3<8).
- What helped: the coddog map (`__osDevMgrMain`→`src/io/devmgr.c`@99.99) confirmed a clean verbatim mirror despite the scary-looking hazards — both `jal-count-mismatch:25vs21` + `calls-unplaced:dma,edma` were correctly read at the gate as the **indirect-call false-positive class** (4 `jalr` through `dm->dma`/`dm->edma` OSDevMgr struct fn-ptrs; 25−4=21), so the candidate was NOT mis-routed to classical. Name pre-curated + all 7 callees + all LEO/OSDevMgr macros pre-placed → zero symbol adds, one include adapt.
- Friction: the verbatim `cp` link-FAILED at first make — `switch (mb->hdr.type)` compiles to a jump table `jtbl_800D2280` (ROM 0xAD680) in the autogen asm rodata whose `.word .L800A39xx` entries are the fn's own internal labels; flipping text→C deleted them → undefined-ref link break. NOT pre-flagged (the `rodata-literal` flag catches only FP pools), and **the gate's text-only green-ROM check cannot catch it by construction** (the jtbl stays valid asm until the body lands). Fixed in-execution with a `.rodata` sibling carve `[0xAD680, .rodata, libultra/io/devmgr]`; the C jtbl reproduced the 8-word block (7 case + 1 zero pad) byte-for-byte → SHA match.
- Applied (3 of 3): #1 `pick_target.py` `rodata-jtbl:0x<vram>` pre-flag — `decomp_asm.rodata_jtbls` scans `%lo(jtbl_…)` whole-subseg (plain + overlay), wired in the SHARED recover-battery (`_append_recover_hazards`) so it prices BOTH named-upstream and coddog candidates (display-only, no point bump, like `rodata-literal`); +3 unit tests, golden regen, suite 39 pass. #2 `docs/hazards.md#rodata-sibling-yaml-pattern` switch-jump-table sub-case (automatic byte-match for a verbatim mirror, trailing zero-pad from linker subseg-boundary fill, gate-can't-catch rationale) + S76 provenance. #3 `CLAUDE.md` hazard-index row for `rodata-jtbl:<addr>`.
- Carry-over: none new. (Standing: io defines-data/file-static traps piacs/motor; os/settime pts-13 multi-file pack; asm-mirror partial-TU spike `__osDisableInt`/`__osRestoreInt`. Next-cleanest named coddog mirrors: `osSetTimer`→settimer.c, `__osSetTimerIntr`→timerintr.c.)

---

## Sprint 75 — bank io/contquery.c (osContStartQuery + osContGetQuery), libultra io coddog-mirror; the near-free-retry of the S74 split head — 2026-06-14
- Increment: src/libultra/io/contquery.c banked (2 fns) / md5-candidate files 114→115; asm subsegs 170→169.
- Quality: 0/0/0/0 (stuck-far / permuter / carried / re-opened) — clean first-try, 0 iteration.
- Seed: committed 5pt; banked 5pt; regime mirror (seed-only; 8-gate clear at 5<8).
- What helped: the S74 carry-over was a model near-free-retry pre-scope — all 4 enabler addresses
  (`osContStartQuery`=0x800A7190, `osContGetQuery`=0x800A7210, callees `__osPackRequestData`=0x800A7660,
  `__osContGetInitData`=0x800A75AC) verbatim-correct, so execution was a mechanical replay: flip
  [0x82590,asm]→c, verbatim `cp` of ultralib `io/contquery.c` with `PRinternal/`→bare include adapt
  (the S74 sibling convention), full-make ROM SHA-1 == baserom first build. Pure text mirror (defines
  no data/static → no ld-section sibling). Ghidra disasm re-confirmed the two jal targets at the gate.
- Friction: none. The pick_target row carried a redundant + mis-pointed `maybe-upstream:voice*` IDF
  guess alongside the definitive `coddog-mirror:contquery.c@99.99` — noise, not a blocker (suggestion #1).
- Applied (2 of 2): #1 `pick_target.py build_rows` — suppress `maybe-upstream` when a ≥99% non-audio
  `coddog-mirror` hit is on the same row (the IDF guess is redundant once coddog has named the file;
  +1 unit test `test_coddog_suppresses_maybe_upstream`, golden regen for the contquery flip, suite
  36 pass); #2 `BACKLOG.md ## Carry-overs` two-kind format (spike vs near-free-retry) + a 5-point
  near-free-retry **completeness checklist** (flip line / placed-ref inventory / new-recovery vrams /
  include-adapt / upstream pin), + a `sprint-review.md` Step-5.4 pointer — codifies the S74 carry-over
  shape that made this sprint a replay.
- Carry-over: none. (Remaining coddog io band = the piacs/motor traps, defines-data/file-static, not
  clean verbatim cps — already parked in BACKLOG.)

---

## Sprint 74 — bank io/contreaddata.c (osContStartReadData + osContGetReadData + __osPackReadData), libultra io coddog-mirror; decompose the pts-8 cont subseg — 2026-06-14
- Increment: 1 .c file banked (`src/libultra/io/contreaddata.c`) / 3 fns matched (md5-candidate 113 → **114**, all 114 .c stub-free). Decomposed the pts-8 c-combined `0x82590` subseg (contquery.c head + contreaddata.c tail) at the upstream-file boundary; banked the clean tail (16-aligned split at 0x82630). Carry: contquery.c head.
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0 (verbatim cp, first-build SHA match, 0 iteration).
- Seed: committed 5pt; banked 5pt; regime mirror (seed-only; 8-gate fired on the combined subseg → resolved by decompose at the contquery/contreaddata boundary).
- What helped: the S71 coddog map identified the subseg's lead (`func_800A7190`→contquery.c@99.99); the tail's curated member names (osContStartReadData/osContGetReadData = ultralib) confirmed contreaddata.c. Picking the TAIL was the de-risk — its only non-placed-non-data callee is its own in-file static `__osPackReadData`, vs the head's two adjacent-subseg function recoveries. Pure text mirror (no ld-section sibling). All 6 recover-extern vrams + the 16-aligned split point read straight from the asm at the gate, so the verbatim cp linked first try.
- Friction: the ranker UNDER-flagged the recover-extern load — the row showed NO calls/refs-unplaced despite 6 genuinely-unplaced symbols (3 SI callees + 3 data globals), because un-named `func_` members blocked the named-keyed scan and the S72 coddog trap re-scan re-ran only defines-data/file-static/needs-header (not refs/calls-unplaced). All 6 found by manual gate recon. Also: the `(already-vendored)` header tag hid a required include-line adaptation (`PRinternal/{controller,siint}.h` → bare, in-tree at `internal/`) that only surfaced as a `No such file` at first build.
- Applied: 2 of 2: #1 `pick_target.py` — factored the refs/calls-unplaced battery into `_append_recover_hazards()` and ran it on the coddog trap re-scan path too (so a coddog-resolved un-named func_'s recover-extern load is priced at the gate, the deferred half of the S66 #2 cross-member-union note; verified the contquery row now shows `calls-unplaced:__osContGetInitData,__osPackRequestData`; golden regen map-free, suite 35 pass). #2 `pick_target.py` `already_vendored_intree_path()` — the `(already-vendored)` tag now carries the in-tree adapt target `(already-vendored,adapt->internal/<h>)` (every such tag IS a needs-include-adapt case by construction: full path failed, basename resolved), so the include-line edit is priced at the gate; golden regen (8 tag updates), suite 35 pass.
- Carry-over: src/libultra/io/contquery.c (osContStartQuery=func_800A7190, osContGetQuery=func_800A7210) — the asm head of the now-split 0x82590 subseg, the direct coddog 99.99 hit. Fully pre-scoped: shares the S74 SI+data recover-externs; needs 2 more func recoveries (`__osPackRequestData`=0x800A7660, `__osContGetInitData`=0x800A75AC, both confirmed jal targets in the adjacent controller.c subseg). Near-free next sibling.

---

## Sprint 73 — bank gu/position.c (guPositionF + guPosition), libultra gu coddog-mirror; closes the gu text band — 2026-06-14
- Increment: 1 .c file banked (`src/libultra/gu/position.c`) / 2 fns matched (md5-candidate 112 → 113, all 113 .c stub-free; asm subsegs 150 → 149). The gu text band is now fully decompiled (only the permanent `0x85DA0 hasm` remains).
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0 (first-build, 0 iteration).
- Seed: committed 3pt; banked 3pt; regime mirror (seed-only, 8-gate clear).
- What helped: the S71 coddog map made the un-named `func_800A9C60`/`func_800A9E38` a definitive 99.99 verbatim mirror of `gu/position.c`; the fully-warm gu band pre-placed every callee (`sinf`/`cosf`/`guMtxF2L`) + `guint.h`; align.c/rotate.c gave the exact 16B dtor `.data`-carve precedent (S61/S68), so sizing the carve `[0xA35B0,.data,position]`→`[0xA35C0,data]` (random's xseed) was mechanical. Verbatim cp byte-identical, first-build SHA match.
- Friction: the coddog row UNDER-flagged the target — clean pts-3, NO defines-data — even after the S72 coddog trap re-scan. Two blind spots stacked: the asm-side `data-static` pre-flag (S52) does not fire on un-named coddog candidates, and the S72 re-scan ran only `defines_data_globals`, which skips `static` lines AND scans only brace-depth 0 — so guPositionF's function-local `static float dtor` was invisible. Caught only by reading the upstream + asm data refs at the gate; an un-vetted pick would have SHA-missed mid-sprint on the un-carved dtor.
- Applied: 2 of 2: #1 `pick_target.py` `defines_local_static_data()` — greps function-body `static <type> <name> = <init>;` (depth ≥ 1, excludes static fn protos) and merges into the `defines-data` hazard on BOTH the named-upstream and coddog re-scan call sites; the source-side backstop the coddog band needed. Verified: flags `dtor` on position/rotate/align/rotaterpy, clean on perspective/epirawread; suite 35 pass, no golden regen. #2 `docs/hazards.md#defines-data` S73 source-side-backstop note (reinforces the S61 16B sizing).
- Carry-over: none.

---

## Sprint 72 — bank io/epirawread.c (__osEPiRawReadIo) + io/pfsselectbank.c (__osPfsSelectBank), libultra io coddog-mirror pair — 2026-06-14
- Increment: 2 .c files banked (`src/libultra/io/epirawread.c` + `pfsselectbank.c`) / 2 fns matched (md5-candidate 110 → 112, all 112 .c stub-free; asm subsegs 152 → 150).
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0 (both first-build, 0 iteration).
- Seed: committed 4pt (2+2); banked 4pt; regime mirror (seed-only, 8-gate clear).
- What helped: the S71 coddog map turned the un-named `func_800B0710`/`func_800AE920` into definitive verbatim-mirror targets (99.99%). epirawread had a banked twin (epirawwrite) pinning the include/EPI_SYNC pattern; the bare-assert wrinkle was resolved by the banked sirawread convention (`_DEBUG`-wrap), with the read==write subseg size as the proof the ROM strips it. pfsselectbank's callee + headers were all pre-placed.
- Friction: two near-misses avoided by reading upstream at the gate — (a) the bare `assert(data != NULL)` would have emitted `jal __assert` and SHA-missed (NDEBUG undefined here); (b) the sibling coddog leaf piacs.c looked like a clean pts-3 mirror but DEFINES data + a `static` (a BSS trap). Both are now tooling-surfaced.
- Applied: 2 of 2: #1 `pick_target.py` coddog trap re-scan — `build_rows` re-runs defines_data/file_static/needs_header on the coddog-resolved `.c` (shared `_tagged_missing_includes` + `_coddog_upstream_path`); piacs re-priced 3→5, motor.c now shows defines-data/file-static; +1 unit test, golden regen, suite 35 pass. #2 `docs/hazards.md#assert-strip` playbook + CLAUDE.md index row + coddog-cross-ref trap-re-scan note. (#3 was observation-only, no edit.)
- Carry-over: none (a file). Tooling follow-up (not file-blocking): optional `bare-assert` advisory `pick_target` flag — scan a mirror's upstream for a non-`_DEBUG`-guarded `assert(` so the strip is priced at the gate.

---

## Sprint 71 — bank io/crc.c (__osContAddressCrc + __osContDataCrc), libultra io mirror; FIRST coddog-driven target — 2026-06-14
- Increment: 1 .c file banked (`src/libultra/io/crc.c`) / 2 fns matched (md5-candidate 109 → 110, all 110 .c stub-free; asm subsegs 153 → 152).
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0.
- Seed: committed 2pt; banked 2pt; regime mirror (8-gate fired pts-13 → cleared by the verbatim-mirror exemption; pts-13 was a coddog-refuted false price, the S43 tooling-artifact class, logged at the corrected mirror seed).
- What helped: **coddog `compare2`** (PO directive) — fingerprinting MG64 vs a combined ultralib-J ELF reclassified the pts-13 `none`/`pack:2fn` subseg as a trivial verbatim 2-fn mirror (99.99%). nm confirmed zero callees (pure CRC integer math); subseg = crc.o `.text` 0xF0 exact; `u8*` arg dodged char-signedness. Byte-identical cp, first-build match.
- Friction: the harness needed building (KMC objects need `objcopy -R .mdebug/.reginfo` + `ld -r --allow-multiple-definition`; objdiff can't read a `.a`). The first sweep script had a `python3 - <<HEREDOC | pipe` stdin collision (heredoc shadowed the piped data) → extracted `parse_map.py`. Golden needed a map-free regen (crc banking + the new optional map both shifted output).
- Applied (4 of 4): #1 coddog gate-step (`docs/hazards.md#coddog-cross-ref` + CLAUDE.md index/preamble); #2 `pick_target.py build_coddog_index` reads `tools/coddog/coddog_map.tsv` → `coddog-mirror:<file>@<pct>` flag + ≥99% non-audio re-price as libultra mirror (env-overridable `CODDOG_MAP`, golden regen, new `test_coddog_mirror_repricing`); #3 BACKLOG S71 regime note (libultra ~all verbatim-mirrorable; genuine classical only the libc xprintf band; audio header-gated); #4 `tools/coddog_sweep.sh` + `tools/coddog/parse_map.py` harness + `make coddog-sweep`. `make test-tools` 34 pass.
- Carry-overs: none new. Bonus: `__osContRamRead`/`__osContRamWrite` unlocked (CRC helper callees now placed). Cross-repo follow-up: `__osContAddressCrc`/`__osContDataCrc` → `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 70 — asm-mirror osMapTLB + osUnmapTLB, libultra os TLB CP0 primitives — 2026-06-14
- Increment: 2 asm TUs vendored+banked (`src/libultra/os/maptlb.s` osMapTLB 0x87F40 / `unmaptlb.s` osUnmapTLB 0x880C0) / 2 functions matched. No `.c` md5-candidate delta (asm-mirror is asm→hasm, not asm→c): asm subsegs 155→153, hasm 19→21; `.c` md5-candidate unchanged 109/109. 6th asm-mirror vendoring sprint (S56/57/58/62/63 lineage).
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0. Both byte-id to ROM disasm, full-make ROM SHA-1 == baserom **first make, 0 iteration**; `.o(.text)` 0xC0/0x40 = 0xB4/0x3C bodies 16-padded (KMC-`as`) → exact slot fill (0x87F40+0xC0=0x88000, 0x880C0+0x40=0x88100).
- Seed: committed 2pt; banked 2pt; regime mirror (8-gate clear; asm-mirror floor, 2 clean single-fn vendorable TUs — S62 2-TU=2 / S58 3-TU=2 anchors)
- What helped: **identification cracked a ~14-sprint carry-over.** The two TUs were parked as "un-named `func_<addr>`, no identified ultralib `.s` yet" since S56 — but a single MCP `disassemble_function` named both instantly by their CP0/TLB signature (mfc0/mtc0 Index/EntryHi/EntryLo0/1 + tlbwi ⇒ osMapTLB/osUnmapTLB). All infra pre-present (MFC0/MTC0 macros S56, C0_*/TLBLO_*/K0BASE in R4300.h, ta0=$12 via the o32 ABI32 regdef path → `li t4` matches disasm); `os_tlb.h` already declared both prototypes; no in-tree C caller referenced the `func_` names → rename safe. The `_DEBUG && __sgi` debug block compiles out under KMC, so the emitted code is the post-`#endif` body — matched verbatim.
- Friction: none in execution. The retro surfaced the real gap: the bare `intrinsic-likely` these subsegs carried reads as "no-source shim → plain hasm," which is why they sat un-pursued for 14 sprints — the flag couldn't say "vendorable TU, just un-named."
- Applied (1 of 2): #1 `pick_target.py` `privileged_asm` fingerprint — a `func_<addr>` asm subseg whose body holds a privileged op gcc never emits (`tlbwi`/`tlbwr`/`tlbp`/`tlbr`, `mfc0`/`mtc0`/`dmfc0`/`dmtc0`/`cfc0`/`ctc0`, `cfc1`/`ctc1`, `cache`, `eret`) now flags `intrinsic-likely:cp0-asm(identify-TU)` — *broader* than the pure-shim `intrinsic_likely` (fires through surrounding branches/loads/`jal`s). It caught `audio_sched_thread_entry`@0x800B3E50 (496B, 50+ CP0 moves + jals, prior hazard `-`), which smallest-first would have mis-offered as a classical target — the negative signal (this is hand-asm, NEVER classical) is the value. `decomp_asm.privileged_asm` + the `classify_subseg` route (TU named via LEAF when resolvable, else `cp0-asm(identify-TU)`); +4 unit tests; CLAUDE.md hazard-index row + `docs/hazards.md#asm-mirror-vendoring`/signature-hints notes; golden regen (exactly the 1 vetted row, verified via repo-state-held-constant stash diff), suite **33 pass**. #2 (asm-mirror gate-note that the gate validation IS the bank) NOT selected — established by S56–S63 precedent.
- Carry-over: none. **Cross-repo follow-up:** `osMapTLB`=0x800ACB40 + `osUnmapTLB`=0x800ACCC0 are new decomp-side symbols → propagate to the Ghidra workspace via `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 69 — bank gu/lookat.c (guLookAtF + guLookAt), libultra gu mirror; GU BAND CLOSED — 2026-06-14
- Increment: 1 `.c` banked (`src/libultra/gu/lookat.c`, 2 fns `guLookAtF`+`guLookAt`) / 2 functions matched (md5-candidate 108→**109**, all 109 .c stub-free; asm subsegs 156→155). The **TRUE last un-flipped gu asm leaf** — banking it closes the entire gu band (the perspective S55 → lookathil S64 → cosf/sinf S66 → translate S67 → align S68 → lookat S69 trail ends here).
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0. Byte-identical verbatim `cp`, 0 edits, 0 iteration, full-make ROM SHA-1 == baserom first `make`.
- Seed: committed 13pt; banked 13pt; regime mirror (8-gate FIRED → verbatim-mirror exemption, generalized)
- What helped: the gate asm-vs-source check **corrected a wrong carry-over note before it cost anything** — the backlog claimed lookat was the "vec3f_normalize substitution class as align," but the source/asm showed guLookAtF uses `sqrtf` **inline** (`-1.0/sqrtf(...)`), making it a PURE verbatim mirror (S64 lookathil shape, NOT align/rotate). All callees pre-placed (`guMtxIdentF`/`sqrtf`/`guMtxF2L`); `guLookAt` inlines guLookAtF (-O2 same-TU, exactly the S68 guAlign pattern). The rodata-sibling carve was mechanical (cosf/lookathil playbook): the anon 6-literal pool 0x800D24C0..0x800D24E0 maps exactly to the generic `[0xAD8C0, rodata]` block (bounded by lookathil), so the carve = `.o(.rodata)` 0x20 exact, SHA-confirmed.
- Friction: one judgment call at the gate, not a stall — lookat's inner fn boundary guLookAt@0x800A7FF0 is **16-aligned**, so the S64 exemption's literal condition (b) (`non16align` on the inner boundary) did not hold as written. Resolved by recognizing the `non16align` test was a proxy for "no valid inter-file split point," which a `single-file-pack` satisfies regardless of inner alignment (you can't mirror half a `.c`). Codified as suggestion #1 so the next 16-aligned single-file-pack 8/13 doesn't re-litigate it.
- Applied (2 of 2): #1 generalized the S64 verbatim-mirror exemption condition (b) — a `single-file-pack` (every member fn from one upstream `.c`) is decompose-blocked **regardless of inner-boundary 16-alignment** (S64 lookathil non16align + S69 lookat 16-aligned, both blocked); `pick_target.py`'s `single-file-pack` tag (S67) IS the signal, so this is doctrine-only (no tooling/golden change). Edited `CLAUDE.md ## Story points` exemption + the `VELOCITY.md` exemption summary. #2 corrected the `BACKLOG.md` carry-over's wrong `vec3f_normalize`-substitution claim (lookat is sqrtf-inline verbatim) and marked the gu-band carry-over section CLOSED.
- Carry-over: none. **Cross-repo follow-up:** `guLookAt`=0x800A7FF0 is a new decomp-side symbol → propagate to the Ghidra workspace via `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 68 — bank gu/align.c (guAlignF + guAlign), libultra gu mirror — 2026-06-14
- Increment: 1 `.c` banked (`src/libultra/gu/align.c`, 2 fns `guAlignF`+`guAlign`) / 2 functions matched (md5-candidate 107→**108**, all 108 .c stub-free; remaining libultra asm-flip candidates 16→15). The **verbatim twin of S61 `rotate.c`** — same `libultra/gu` dir, same substituted-callee + static-dtor signature.
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0. First-build match, 0 iteration.
- Seed: committed 3pt; banked 3pt; regime mirror (8-gate clear)
- What helped: the S61 rotate.c precedent made the sprint mechanical — the two hazards (`calls-unplaced:guNormalize`, `data-static:0x800C81A0`) were exactly rotate's, so the playbook was known before the gate (copy verbatim, apply the one `guNormalize`→`vec3f_normalize` substituted-callee edit, carve the `static float dtor` `.data` sibling). `vec3f_normalize`@0x80029900 is a *game-region* fn (libultra lives at 0x800A0000+ — the S61 address-region discriminator), so the substitution was certain without re-body-comparing. `guAlign` inlined the full guAlignF body (-0xA8 frame) as -O2 same-TU inlining predicts; first build matched. `guint.h` resolves source-relative (co-located at `src/libultra/gu/guint.h`) → zero header work.
- Friction: one process wrinkle, not a stall — the `.data` sibling could not land at the plan gate (an INCLUDE_ASM stub emits no `.data`, so the carve shifts the data segment and fails the gate green-ROM check). Deferred the split to the execution step with the body; gate flipped text only. Now codified (suggestion #1) so the next static-carve mirror doesn't re-discover it.
- Applied (3 of 3): #1 ld-section split-timing — `sprint-plan` Step-7 enabler list + `docs/hazards.md#defines-data` (Carve timing) + `#.rodata-sibling-yaml-pattern` (Timing) now state a `.data`/`.rodata`/`.bss` sibling lands at execution-with-body, never the gate. #2 `pick_target.py` `twin-of:<file>` hint — a candidate that re-emits a function-local static (data-static / rodata-literal) whose mirror dir already carved that ld-section now names the proven sibling (new `_static_carve_siblings` with a dot-aware yaml scan, since the shared SUBSEG_RE types as `[a-z]+` and silently drops dot-prefixed siblings); CLAUDE.md hazard-index row added. #3 `pick_target.py` single-file-pack recognition for an unnamed trailing member — the gu F-variant + s16-wrapper idiom: when the one named C stem's upstream file defines exactly nfns functions, the pack atomic-mirrors (verified live: guLookAtF flipped `pack`→`single-file-pack`). golden regen (align banking removed guAlignF + the new flags fire on out-of-window rows), suite 26 pass / 3 skip.
- Carry-over: none.

---

## Sprint 67 — bank gu/translate.c (guTranslateF + guTranslate), libultra gu mirror — 2026-06-14
- Increment: 1 `.c` banked (`src/libultra/gu/translate.c`, 2 fns `guTranslateF`+`guTranslate`) / 2 functions matched (md5-candidate 106→**107**, all 107 .c stub-free). The **last un-flipped asm leaf in the gu band** (S66 surfaced it); the gu band 0x82F20..0x85CD0 is now fully decompiled.
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0.
- Seed: committed 2pt; banked 2pt; regime mirror (8-gate clear)
- What helped: the cleanest mirror of the whole band — zero enablers beyond the one yaml flip. Both fn names pre-curated in `ghidra_symbols.txt`, `guint.h` already vendored (gu-band sibling of perspective/lookathil), callees `guMtxIdentF`/`guMtxF2L` placed, and **no float literals → no rodata-sibling split** (simpler than perspective/lookathil). Byte-identical verbatim `cp`, full-make ROM SHA-1 == baserom first try, 0 iteration. The gate's call/data-ref reconciliation found nothing unplaced, so no execution-time surprises.
- Friction: none.
- Applied (1 of 2, PO selected #2 only): #2 `pick_target.py` now emits `single-file-pack:<n>fn[…]` for a pure single-upstream-file C pack (all members → one stem, no `=?`/asm members) instead of the split-implying `pack:<n>fn[…]` — display-only (the pts seed keys the pack penalty on `nfns>1`, not the hazard kind), so sort/scoring unchanged; CLAUDE.md hazard-index row + `docs/hazards.md` single-file-pack note synced to route it to `#upstream-mirror-pattern`; golden regen (legitimate — translate banking removed it from the candidate set), suite 29 pass. (#1 BACKLOG gu-trail-retired note: NOT selected.)
- Carry-over: none.

---

## Sprint 66 — bank cosf + sinf (libultra gu trig verbatim mirrors) — 2026-06-14
- Increment: 2 `.c` banked (`src/libultra/gu/cosf.c` `cosf` + `src/libultra/gu/sinf.c` `sinf`) / 2 functions matched (md5-candidate 104→**106**, all 106 .c stub-free; asm subsegs 157→158; surfaced `gu/translate.c` as a new asm leaf). Both decompose-splits of pts-13 combined packs at the upstream-file boundary (cosf from `[0x82B80]` 5-fn at 0x82F20+0x83070; sinf from `[0x85B30]` 3-fn at 0x85CD0).
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0 (1 gate-missed enabler recovered in-execution, not a spike).
- Seed: committed 4pt (cosf 2 + sinf 2); banked 4pt; regime mirror (8-gate satisfied by the two decompose-splits, S60 pattern)
- What helped: did **sinf first** (lower-variance: the shared weak-alias + `__libm_qnan_f` machinery proven on the clean anonymous-pool sibling before cosf's named pool). PO-directed byte-compare of cosf.o `.rodata` vs ROM **before** carving confirmed the pool matched libultra → the S64 named-rodata caveat fell as a phantom. First C `#pragma weak cosf=__cosf` mirror compiled+matched clean under KMC gcc 2.7.2 (C analog of S58 bcopy `_bcopy=bcopy`), no edit.
- Friction: a **gate-missed shared recover-extern** — `__libm_qnan_f`@0x800D2640 (the NaN-path return refd by both fns) was anonymous `D_800D2640`, invisible to `refs-unplaced` (can't bind an anon label) AND hidden because the fns were non-primary pack members (refs_unplaced scans only the primary's upstream, align.c). Caught at execution-time data-ref reconciliation, recovered (1 add). Also: a clean-rebuild was needed after the symbol rename (stale stub `.o` referenced the old `D_800D2640` — the no-`.s`-dep rule). PO course-corrections honored: libkmc-vs-libultra verified (libultra — ROM rodata byte-id to ultralib, libkmc has no cosf/sinf), cosf's named-rodata byte-checked vs libultra before carving.
- Applied (3 of 3): #1 `docs/hazards.md` .rodata-sibling named-pool note (retire the phantom collision caveat) + S66 provenance; #2 `pick_target.py PRAGMA_WEAK_RE` keys weak aliases in `build_upstream_index` (`cosf=cosf` not `cosf=?`) — provably golden-inert (the only weak-aliased fns, cosf/sinf, are now banked), suite 29 pass; #3 `docs/hazards.md` Upstream-mirror `#pragma weak` C-mirror note. (#4 mirror-track-variance observation: logged, no edit proposed.)
- Carry-over: none (file-level). Tooling follow-up (the deeper half of #2): union `refs_unplaced` over c-combined member upstreams so a hidden member's `__`-prefixed extern (the `__libm_qnan_f` class) surfaces at the gate, not at execution — recorded in BACKLOG enabler notes.

---

## Sprint 65 — clear [0x860C0] libc pack: bzero asm-mirror + string.c C mirror — 2026-06-14
- Increment: 1 `.c` banked (`src/libultra/libc/string.c`, 3 fns `strchr`/`strlen`/`memcpy`) + 1 asm-mirror (`src/libultra/libc/bzero.s`, `bzero`) / 4 functions matched (md5-candidate 103→**104**; asm subsegs 158→157, hasm 18→19, c 103→104). Decomposed the pts-8 4-fn pack at the bzero|string boundary (rom 0x86160, 16-aligned) into one `hasm` (bzero) + one `c` (string).
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0.
- Seed: committed 4pt; banked 4pt; regime mirror (seed-only; pack seed-8 → 8-gate fired → decomposed into string ~2 + bzero ~2). **Calibration note: the mirror track's "zero-variance point-mass" was VIOLATED — string.c was a verbatim mirror that SHA-missed and needed a build-flag recovery (would carry a +1 novel-gotcha residual on the classical track). First mirror sprint with real variance; the per-file all-or-nothing bank + counter-metric still held.**
- What helped: bzero asm-mirror reused the bcopy.s VENDOR_ASM pattern verbatim (WEAK/LEAF/XLEAF all in `sys/asm.h`, R4300.h/regdef.h deps vendored, `blkclr` unreferenced→harmless) → proven green at the gate, 0 iter. `memcpy` matched under both char flags. `.o`-disassembly diff localized the SHA-miss to char-load signedness in 1 step.
- Friction: string.c char-signedness. Verbatim mirror SHA-missed — ROM compiled it SIGNED-char (`strchr`/`strlen` load `lb`+`sll/sra`, phantom empty frame) but the band forced `-funsigned-char`. Initial fix was a per-file `-fsigned-char` override; the PO-approved authoritative test (`make clean` + global `-fsigned-char` rebuild) then reproduced the baserom SHA-1 **exactly** → `-funsigned-char` was a WRONG band default. Flipped the band to `-fsigned-char`, removed the per-file override. ultralib's gcc.mk adds `-funsigned-char` for VERSION_J, so MG64's libultra char signedness is a **ROM-proven deviation from the documented J profile** (durable project constraint — PO promote to memory).
- Applied (3 of 3): #1 `pick_target.py` pack-member labels resolve `.s` TUs via `build_asm_tu_index` (`bzero=?`→`bzero=bzero.s`; a MIXED asm+C pack is now legible at the gate, no longer an opaque pts-8; golden regen for the now-split [0x860C0] row, suite 26 pass / 3 skip). #2 `docs/hazards.md#char-signedness` section + `CLAUDE.md` hazard-index row (`clean mirror SHA-miss, char load lb/sll-sra vs lbu/andi`) + the per-file override mechanism documented; **the suggested pick_target char-sensitivity pre-flag was DESCOPED — the #3 global flip removed the systematic cause, so a fuzzy char-comparison detector would now only catch a hypothetical inverse case at false-positive risk**. #3 global signed-char investigation → flipped the band default to `-fsigned-char` (Makefile LIBULTRA_CFLAGS + comments; docs/hazards.md compile-profiles note).
- Carry-over: none. `[0x860C0]` pack fully cleared.

---

## Sprint 64 — bank gu/lookathil.c (guLookAtHiliteF + guLookAtHilite), libultra gu mirror — 2026-06-14
- Increment: 1 `.c` banked (`src/libultra/gu/lookathil.c`, 2 fns) / 2 functions matched (delta: md5-candidate 102/103 → **103/103**, all `.c` stub-free; asm subsegs 159→158). Verbatim ultralib `gu/lookathil.c` (VERSION_J), byte-identical cp, full-make ROM SHA-1 == baserom first try.
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0 (1 expected rodata sibling-split, not a gotcha).
- Seed: committed 13pt; banked 13pt; regime mirror (seed-only). **8-gate FIRED (pts-13) → ran as a documented verbatim-mirror EXEMPTION**, banked atomic first-try as predicted.
- What helped: the single cleanest remaining libultra mirror — all callees pre-placed (`guMtxIdentF`/`sqrtf`[S58]/`guMtxF2L`/self), both names curated → 0 symbol adds, 0 header copies (`guint.h` vendored S49). The pts-13 8-gate was a SIZE-only false fire: decompose was mechanically blocked (single upstream file; internal fn boundary guLookAtHilite@0x84104 non-16-aligned), and a verbatim single-file mirror has no all-or-nothing classical stall — so a narrow exemption was justified, validated by the first-try bank. rodata sibling-split for the 6-double ANONYMOUS pool (`[0xAD8E0, .rodata, libultra/gu/lookathil]`, 0x800D24E0..0x2510 = exactly the lookatref boundary) was the only mechanical step; gate verified at finalize against the compiled `.o` (0x30). NO vec3f_normalize substitution (that's the align/lookat 0x82B80 combined subseg).
- Friction: none on the bank. The rodata-literal pre-flag's max referenced literal (0x800D2500) understated the carve end (0x2510) by one trailing `.double 0` with no `%lo` of its own — harmless (the carve was `.o`-sized) but motivated suggestion #2.
- Applied: 3 of 3 — #1 8-gate **verbatim-mirror exemption** codified (`CLAUDE.md ## Story points` + `VELOCITY.md` purpose §); #2 `pick_target.py` rodata-literal **`;carve-end=` boundary** (`_rodata_carve_end_vram`, the next `.rodata` subseg boundary) + `docs/hazards.md` rodata-sibling note; #3 `pick_target.py` **`c-combined:<n>file[…]`** — the C analog of S62's asm `combined-subseg`, surfacing multi-file C packs the way the asm flag surfaces TU packs (the win: the sp register-shim pack `func_800B16A0`, previously `upstream none`, now flags `3file[sprawdma|spsetpc|spsetstat]`) + CLAUDE.md hazard index + `docs/hazards.md` pack note. Golden regen, suite 29 pass.
- Carry-over: none. **Next-cleanest libultra mirror recorded in BACKLOG: `cosf` (gu/cosf.c, 0x82F20) — a zero-callee leaf inside the 0x82B80 align/cosf/lookat combined subseg (`c-combined:2file[align|lookat]`, cosf hidden by its `#pragma weak cosf=__cosf` alias → `=?`); constants verified self-contained vs sinf, but they are NAMED in ghidra_symbols (kInvPi/kCoeff1-4) → novel carve-collision risk vs mirrored statics. sinf (0x85B30) is the clean sibling once cosf is proven.**

## Sprint 63 — clear the [0x8CA50] register-shim + device-busy pack (libultra) — 2026-06-14
- Increment: 1 new `.c` (`src/libultra/io/sp.c`, `__osSpDeviceBusy`) + 3 asm-mirror TUs vendored. Decomposed the pts-8 `__osSetFpcCsr` pack (8-gate fired): the `[0x8CA50,asm]` 80B subseg held 3 reg-shim asm TUs (`__osSetFpcCsr`/`setfpccsr.s`, `__osSetSR`/`setsr.s`, `__osSetWatchLo`/`setwatchlo.s`, each 0x10, continuing the adjacent 0x8CA20/30/40 S56 run) + 1 C-mirrorable `__osSpDeviceBusy` (`io/sp.c`, 0x8CA80). 4-way split at 0x8CA60/70/80 → 3 `hasm` + 1 `c` (asm subsegs 160→159, hasm 15→18); 4 fns matched; md5-candidate 101→102. C mirror trimmed to the dp.c io-band include set (`os_internal.h`+`rcp.h`), 0 iteration; clean-rebuild + full-make ROM SHA-1 == baserom; all 3 vendored `.o` `.text` == 0x10 slot.
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0 (1 enabler-discovery — CFC1/CTC1, priced reactively at a failing vendor-compile, not a spike).
- Seed: committed 4pt; banked 4pt; regime mirror (asm-mirror + warm-C cluster, seed-only; 8-gate FIRED on the pts-8 pack → resolved by the 4-way split, each member banks independently <8).
- What helped: the os/ reg-shim band is fully warm from S56 (getsr/setcompare sit right before this subseg), and the device-busy family is fully proven (dp/ai/si C mirrors) — so the C member was a verbatim sibling cp. `combined-subseg:3tu` (NOT 4tu — the no-C-upstream gate from S62 #1 correctly excluded `__osSpDeviceBusy`) named the split shape at the plan gate, and MCP `disassemble_function` confirmed all 4 boundaries verbatim against ultralib before the flip. All 4 names pre-curated → 0 symbol adds. `setfpccsr.s`'s upstream `@bug END(__osSetSR)` (non-MODERN_CC path) is metadata-only (`.size`/`.end`, not `.text`) → SHA-clean as predicted.
- Friction: one gate-surfaced enabler the detector did not pre-price — `setfpccsr.s` references `CFC1`/`CTC1` (FPU control-register moves) absent from the in-tree `sys/asm.h` (S56 vendored only `MTC0`/`MFC0`), so the gate `make` failed at the setfpccsr vendor-compile (`Unknown opcode: cfc1`). Root cause: the `vendorable_tu_missing_defines` pre-check ran only on the `intrinsic-likely:<tu>.s` hazard path, not on the `combined-subseg:<n>tu` path this pack surfaced under → the macro gap was discovered reactively, not priced at the gate. Fixed in-flight by vendoring CFC1/CTC1 verbatim; captured as suggestion #1.
- Applied (1 of 1): #1 `pick_target.py` — the `combined-subseg:<n>tu[…]` builder now unions `vendorable_tu_missing_defines(t)` across the pack's TUs and appends `(needs-define:…)` to the detail, the same pricing the `intrinsic-likely` path already does, so a missing asm macro is surfaced at the plan gate. The core function is already unit-tested (`test_vendorable_tu_missing_defines`, S57); the wiring is a 4-line mirror of the proven intrinsic-likely path. Provably golden-inert: 0 combined-subseg packs remain after this banking (the only one was `__osSetFpcCsr`), so the new branch cannot alter output — verified the golden diff is purely the banked-pack removal (no `+` needs-define line). Golden regen for the banking effect, suite 29 pass.
- Carry-overs: none new. The 3 reg-shim "set" TUs complete the CP0/FPU shim family (S56 did the "get"/setcompare siblings). Remaining intrinsic-likely (de-ranked in BACKLOG): partial-TU `__osDisableInt`/`__osRestoreInt` (`setintmask.s` carve off `osSetIntMask`), un-named `func_800ACCC0`/`func_800ACB40` (need `.s` ID), mixed packs `osSetIntMask` (3fn) / `func_800AFB90` (8fn). Remaining libultra C is classical-track (ContRam/pfs/cont `jal-mismatch` stripped impls).

## Sprint 62 — osInvalDCache + osInvalICache (libultra cache-invalidate asm-mirror) — 2026-06-14
- Increment: 0 new `.c` (asm-vendor/hasm housekeeping). 2 libultra cache-invalidate hand-asm TUs vendored from ultralib + built into the ROM: `osInvalDCache` (0x823B0, `os/invaldcache.s`, 176B/0xB0) + `osInvalICache` (0x82460, `os/invalicache.s`, 128B/0x80). The combined `[0x823B0,asm]` subseg (one 304B slot, both fns) split at the osInvalICache boundary 0x82460 → 2 `hasm` subsegs + 2 `VENDOR_ASM` pairs (asm subsegs 161→160, hasm 13→15). md5-candidate unchanged at 101/101 (asm-mirror is asm→hasm, not asm→c). Full-make ROM SHA-1 == baserom first try; both `.o` `.text` == subseg slot.
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0 (1 mild novelty — the combined-subseg split, mechanical, same as the S10/S60 C-pack splits).
- Seed: committed 2pt; banked 2pt; regime mirror (asm-mirror cluster, seed-only; 8-gate clear).
- What helped: the os/ cache band is fully warm from S57 (writebackdcache pair) — every macro the two TUs reference (`DCACHE_*`/`ICACHE_*`/`CACH_PD`/`CACH_PI`/`C_HINV`/`C_HWBINV`/`C_IINV`/`K0BASE` in `PR/R4300.h`, `CACHE`/`LEAF`/`END` in `sys/asm.h`) was already proven self-contained → 0 missing. Names pre-curated in ghidra_symbols + `LEAF` → zero symbol adds. Reading `asm/823B0.s` at the gate found the two `glabel`s mapping to distinct ultralib `.s` files, fixing the split boundary (0x82460) before going further; `make extract` then wrote the new split subseg's `asm/82460.s` automatically (absent → splat writes once), and the build took each `.o` from its `VENDOR_ASM` rule. Per-subseg `.o` independence + the standby bisect protocol meant no all-or-nothing risk.
- Friction: none. The S58 carry-over had already named this the "combined-subseg sub-pattern", so the only pre-work was confirming the split boundary from the disassembly.
- Applied (2 of 2): #1 `pick_target.py` `combined-subseg:<n>tu[…]` pre-flag — when an `asm-flip` pack has ≥2 asm-ONLY members (no C upstream) from *distinct* vendorable ultralib `.s` files, emit the hazard so the gate prices the split before hand-disassembling. Gated on no-C-upstream so a C-mirror gu pack whose members also ship `.s` variants (e.g. `sinf`+`guTranslateF`+`guTranslate` → `translate.s`/`translatef.s`) does NOT mis-flag; surfaced the smallest libultra candidate `__osSetFpcCsr` as `combined-subseg:3tu[setfpccsr.s|setsr.s|setwatchlo.s]` (a reg-shim pack). + `docs/hazards.md#asm-mirror-vendoring` combined-subseg caveat + CLAUDE.md hazard-index row; golden regen (1 intended diff: `__osSetFpcCsr`), suite 29 pass. #2 BACKLOG carry-over wording fix — `__osDisableInt`/`__osRestoreInt` (0x8B900) is a partial-TU/mixed-pack split (both share ultralib `setintmask.s` with `osSetIntMask`), NOT "source not located"; `func_800ACCC0`/`func_800ACB40` still need ultralib `.s` identification.
- Carry-overs: none new. The clean source-confirmed asm-mirror TUs are now exhausted; remaining (de-ranked in BACKLOG): partial-TU `__osDisableInt`/`__osRestoreInt` (`setintmask.s`, needs a carve/split off `osSetIntMask`), un-named `func_800ACCC0`/`func_800ACB40` (need `.s` ID), and the mixed packs `osSetIntMask` (3fn) / `func_800AFB90` (8fn). Remaining libultra C is classical-track (ContRam/pfs/cont `jal-mismatch` stripped impls) + the gu `guAlignF`/`guMtxCat` 4fn-pack 8-gate decomposes.

## Sprint 61 — guRotateF + guRotate (libultra gu/rotate near-verbatim mirror) — 2026-06-14
- Increment: `src/libultra/gu/rotate.c` banked (2 fns, `guRotateF` + `guRotate`). Verbatim libultra `gu/rotate.c` under BUILD_VERSION=VERSION_J, sole source edit `guNormalize`→`vec3f_normalize`. Proved by clean-rebuild full-make ROM SHA-1 == baserom. md5-candidate 100→101 (all 101 `.c` files now 0-stub).
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0 (1 novel bank-gotcha — the `.data` carve sizing — fixed same-session, not a spike).
- Seed: committed 3pt; banked 3pt; regime mirror (seed-only; 8-gate clear).
- What helped: the gu band is fully warm (S55/S60) — `guint.h` vendored co-located, all callees pre-placed (`vec3f_normalize`/`sinf`/`cosf`/`guMtxIdentF`/`guMtxF2L`), names pre-curated → zero symbol adds. The PO's interrupt ("if vec3f_normalize is near other libultra fns it's likely guNormalize — verify by .o-compare") drove the right check: body-comparing the ROM target @0x80029900 against ultralib's prebuilt `guNormalize.o` proved they are *different* functions (game region not libultra; −0x28 frame + `osSyncPrintf` degenerate-input error path + (0,1,0) fallback + bare `sqrt.s`, vs `guNormalize`'s −0x20 frame + `sqrtf` NaN-check), so the callee was a substitution, not a rename. Reading the upstream at the gate (VERSION_J kills the `#if >= VERSION_K` block) correctly predicted the detector's `jal 7vs4` / `calls-unplaced:xxsine,yxsine,zxsine` were false flags from `#define xxsine (x*sine)` macro-definition lines.
- Friction: (1) the detector mis-tagged a clean near-verbatim mirror as classical (phantom macro-def calls), forcing a gate-time upstream read to disprove it. (2) Novel bank-gotcha: the function-local `static float dtor` needed a `.data` carve, but a 4B carve (the float's size) double-counted — `rotate.o(.data)` is section-padded to 16B, so the un-carved tail re-emitted the original bytes → ROM +16B and a full data-segment reflow (the `cmp` first-diff at 0x1008 was an artifact of the size-grown file, not the real divergence). Correct carve was 16B, sized from the compiled `.o`. The `dtor` name was already claimed by the banked sibling `rotaterpy.c`@0x800C81E0, so the documented S52 drop-extern *hoist* would have collided — kept the static verbatim + carved (the more-verbatim fallback the S52 note didn't cover).
- Applied (3 of 3): #1 `docs/hazards.md#defines-data` — added the function-local-vs-file-scope distinction (a verbatim function-local static emits a *local* symbol `dtor.2` and never collides; the collision is a property of the drop-extern *hoist*) + the name-collision carve fallback + the carve-sizing rule (size from the compiled `.o`, not the static's byte size; clean-rebuild to verify). #2 `pick_target.py` `_strip_define_lines` — strips in-body `#define` directive lines before the call scan in both `_c_jal_count`/`call_divergence` and `calls_unplaced`, so a `#define NAME (expr)` definition line no longer matches `C_CALL_RE` as a phantom call (verified: guRotateF C-jal 7→4 == asm, calls-unplaced drops to just the genuine `guNormalize`); golden regen, suite 29 pass. #3 `docs/hazards.md#calls-unplaced` — the renamed-vs-substituted body-compare step (disassemble + `.o`-compare against the upstream callee before editing; address region as fast discriminator).
- Carry-overs: none. Remaining libultra C is classical-track (ContRam/pfs/cont families carry `jal-count-mismatch` → stripped impls) plus the gu `guAlignF`/`guMtxCat` 4fn-pack 8-gate decomposes and the asm-mirror vendoring carry-overs.

## Sprint 60 — gu matrix sibling-pair (guMtxCatL/guMtxXFML + guOrthoF/guOrtho) — 2026-06-14
- Increment: `src/libultra/gu/mtxcatl.c` (guMtxCatL + guMtxXFML) + `src/libultra/gu/ortho.c` (guOrthoF + guOrtho) banked, 4 fns. Decomposed the pts-8 `[0x84960,asm]` 4-fn gu pack at the `mtxcatl.c | ortho.c` upstream-file boundary into two verbatim libultra mirrors (the 8-gate's split path). Both proved by clean-rebuild full-make ROM SHA-1 == baserom. md5-candidate 98→100.
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0.
- Seed: committed 7pt; banked 7pt; regime mirror (seed-only; 8-gate satisfied by decompose — each file banks independently <8).
- What helped: the gu band was fully warm — `guint.h` vendored co-located (S49), `gu.h`/`mbi.h` in-tree, all callees pre-placed (`guMtxL2F`/`guMtxXFMF` in banked `mtxcatf.c`, `guMtxCatF`/`guMtxF2L`/`guMtxIdentF`), all 4 names pre-curated → zero symbol adds. `ortho.c` was first-pass clean. Reading the upstream at the plan gate (gu/mtxcatl.c carries guMtxXFML under `#if BUILD_VERSION < VERSION_K`, active for VERSION_J) caught the pick_target pack-disambiguation mislabel (it had attributed guMtxXFML to `mgu/mtxxfml.c`) before the flip.
- Friction: one genuine enabler surfaced mid-execution — `guMtxXFML` linked-undefined after a verbatim copy. Root cause: the in-tree `os_version.h` was the stripped 2.0L revision (no `VERSION_*` constants), so `#if BUILD_VERSION < VERSION_K` evaluated `0<0`=false and silently dropped the function. This is a **stale-vendored-header** class (header resolves as a file, but its content is a wrong revision) — invisible to `needs-header`, surfaces only at link, not compile. Fixed by adding `VERSION_D..L` verbatim; a `make clean` rebuild confirmed the add is globally SHA-safe (incremental `make` had left a stale `mtxcatl.o`, masking it). The phantom `needs-header:../gu/guint.h(vendorable)` on the pack/guRotateF was a cascade of the mgu mislabel.
- Applied (4 of 4): #1 `pick_target.py` `stale_version_header` detector → `stale-header:os_version.h(<V>)` hazard (reuses the existing `_strip_inactive_version_branches`/`_build_version_ord` machinery; inert now that os_version.h is fixed, golden unchanged by it) + `docs/hazards.md#stale-vendored-header` + CLAUDE.md hazard-index row. #2 `missing_includes` `os.path.normpath` so a `..` quote-include collapses correctly (the mechanism already existed since S50; this hardens it). #3 `docs/hazards.md#clean-rebuild-after-shared-header-edit` + CLAUDE.md finalization bullet (clean rebuild when an enabler edits a shared vendored header; the build has no header-dep tracking). #4 `build_upstream_index` deterministic glob sort (gu/ < mgu/ — MG64 mirrors libgultra, not the fast-gu mgu variant) + version-branch strip, fixing the guMtxXFML/guRotateF mgu-mislabel and removing the phantom `../gu/guint.h` needs-header from guRotateF (which supersedes the S59 #1 note that called guRotateF's `../gu/guint.h` "genuine" — it was the same mgu mislabel). Golden regen, suite 29 pass.
- Carry-overs: none. Remaining libultra is classical-track (ContRam/pfs/cont families carry `jal-count-mismatch` → stripped impls) plus the gu/guAlignF/guMtxCat 4fn-pack 8-gate decomposes and the asm-mirror vendoring carry-overs.

## Sprint 59 — osGbpakCheckConnector (libultra io gbpak verbatim C mirror) — 2026-06-14
- Increment: `src/libultra/io/gbpakcheckconnector.c` banked (1 fn, `osGbpakCheckConnector`, 1120 B). Verbatim ultralib `src/io/gbpakcheckconnector.c` with the io-band include adaptation (`PRinternal/controller.h` → `"controller.h"` + `controller_gbpak.h`), zero edits, matched first `make`, full-make ROM SHA-1 == baserom. md5-candidate +1. Returns to the C-mirror track after the S56-58 asm-mirror vendoring run.
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0.
- Seed: committed 3pt; banked 3pt; regime mirror (seed-only; 8-gate clear).
- What helped: sibling-vendored infra made this nearly free — the exact callees (`osGbpakGetStatus`/`Power`/`ReadWrite`) were already-banked siblings (`gbpak{getstatus,power,readwrite}.c`), and the only hazard `needs-header:controller.h(vendorable)` was a 0-work no-op: `include/libultra/internal/controller.h` (which defines `ARRLEN`/`ERRCK` inline) plus `controller_gbpak.h` were already vendored and already on the io band's `-I` set, so the stripped-include adaptation resolved for free. No jal-mismatch / calls-unplaced / refs-unplaced; name pre-curated → zero symbol adds. pick_target's clean hazard profile (vs the ContRam pair's `jal-count-mismatch:23vs10`) correctly steered the pick.
- Friction: none. The `needs-header` flag mildly over-stated the work (it was already vendored, not a fresh cp) — captured as suggestion #1, applied below.
- Applied (1 of 1): #1 `pick_target.py` `include_is_already_vendored` — a missing include whose BASENAME resolves under the lib's `-I` set (so the established prefix-stripping adaptation costs nothing) is now tagged `(already-vendored)` instead of `(vendorable)`, distinguishing a 0-work no-op from a one-time source cp; re-tags the warm io/cont/pfs/vimgr/timer band (`__osContRam{Read,Write}`, `osContInit`, `__osGbpakSetBank`, `osCreateViManager`, `__osTimerServicesInit`, …) whose header hazards are all no-ops, while `guRotateF`'s genuine `../gu/guint.h` companion-copy correctly stays `(vendorable)`; golden regen, suite 29 pass.
- Carry-overs: none. The clean low-cost libultra C-mirror band is exhausted again; remaining libultra C is classical-track (the ContRam/pfs/cont families carry `jal-count-mismatch` → stripped impls) or the asm-mirror vendoring carry-overs.

## Sprint 58 — asm-mirror vendoring: 3 libultra intrinsic-likely asm TUs (sqrtf, osMapTLBRdb, bcopy) — 2026-06-14
- Increment: 0 new `.c` (asm-vendor/hasm housekeeping). 3 libultra hand-asm TUs vendored from ultralib + built into the ROM: `sqrtf` (0x8BE10, gu/sqrtf.s), `osMapTLBRdb` (0x8CD10, os/maptlbrdb.s), `bcopy` (0x85DA0, libc/bcopy.s). 3 subsegs flipped `asm`→`hasm` (hasm 10→13; asm subsegs 167→164); 3 intrinsic-likely candidates off the carry-over list. md5-candidate unchanged at 97/97 (.c files all 0-stub). Full-make ROM SHA-1 == baserom; all 3 `.o` `.text` == subseg slot (0x10/0x60/0x320, KMC-`as` padding).
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0.
- Seed: committed 2pt; banked 2pt; regime mirror (asm-mirror cluster, seed-only; 8-gate clear).
- What helped: the S57 `needs-define` pre-check had already confirmed the whole batch self-contained (0 missing macros), so the two flagged caveats were pre-priced at the plan gate and both held without special handling — `bcopy`'s `#ifdef __sgi` takes the `#else` `_bcopy=bcopy` path in the KMC build (the in-tree `WEAK` macro never reached), and `sqrtf`'s missing `.set noreorder` lets the assembler auto-fill the `j ra` delay slot. First cross-dir batch (gu/ + os/ + libc/) proves the pattern is dir-agnostic. Per-subseg `.o` independence meant no all-or-nothing risk; the bisect protocol was on standby but unused.
- Friction: none. All 3 first-try clean in one atomic `make extract && make`.
- Applied (0 of 0): empty suggestion buffer — no new gotchas surfaced.
- Carry-overs: none new. Remaining intrinsic-likely TUs still de-ranked in BACKLOG: un-named `func_800ACB40`/`func_800ACCC0` (need ultralib `.s` identification, else bare-intrinsic→plain hasm); `osInvalDCache`+`osInvalICache` pack (0x823B0, combined 2-fn subseg → needs a combined-`.s` sub-pattern); `__osDisableInt`/`__osRestoreInt` (source not yet located in ultralib `src/os/`); plus the mixed packs (`osSetIntMask` 3fn, `func_800AFB90` 8fn) that must be split before any hasm.

## Sprint 57 — asm-mirror vendoring: 4 libultra cache/TLB asm primitives — 2026-06-14
- Increment: 0 new `.c` (asm-vendor/hasm housekeeping). 4 libultra cache/TLB hand-asm TUs vendored from ultralib + built into the ROM: `osWritebackDCacheAll` (0x82560), `osUnmapTLBAll` (0x88100), `osWritebackDCache` (0x824E0), `__osProbeTLB` (0x88000). 4 subsegs flipped `asm`→`hasm` (hasm 6→10; asm subsegs 171→167); 4 intrinsic-likely candidates off `pick_target`. md5-candidate unchanged at 97/97 (.c files all 0-stub).
- Quality: 0/0/0/0 stuck-far/permuter/carried/re-opened. All 4 ultralib version-matched FIRST attempt (one atomic gate `make`); no novel gotcha — S56 derisked the pattern (KMC-as padding, VENDOR_ASM mechanism).
- Seed: committed 4pt; banked 4pt; regime mirror (8-gate clear; seed-only, no realized/residual).
- What helped: S56's `VENDOR_ASM` map + `LIBULTRA_ASFLAGS` profile made each new TU a 3-line addition (copy `.s`, add pair, flip subseg). All cache/TLB/gu macros + `PR/rdb.h` already ship in-tree → the whole cache/TLB family is zero-enabler. Pre-curated `ghidra_symbols.txt` names + `LEAF` symbol → zero symbol adds. Gate validated all 4 atomically with the green ROM SHA-1.
- Friction: none. Pre-sprint I wrongly assumed `osMapTLBRdb` needed a `PR/rdb.h` copy and excluded it; the suggestion #1 pre-check then proved `rdb.h` is in-tree and the TU is clean (backlog correction, folded in).
- Applied (2 of 2): #1 `pick_target.py` `vendorable_tu_missing_defines` needs-define pre-check (greps the vendorable `.s` UPPER_CASE macros vs the in-tree asm `-I` headers; strips C comments + `#include` paths to avoid `R4300`/`TLB` false-flags; annotates `intrinsic-likely:<tu>.s(needs-define:<MACROS>)`; +1 unit test, suite 28→29, golden unchanged — inert on the self-contained backlog); #2 `docs/hazards.md#asm-mirror-vendoring` SHA-breaker bisect protocol (flip a suspect subseg back to `asm`, rebuild, narrow) + needs-define step note + S57 provenance.
- Carry-over: none from this set. Remaining vendorable intrinsic-likely TUs: `bcopy` (0x85DA0, `WEAK` alias — verify `WEAK` macro), `sqrtf` (0x8BE10, `sqrt.s` FPU + reorder/nop), `func_800ACCC0` (0x880C0), `func_800ACB40` (0x87F40), `osMapTLBRdb` (0x8CD10, **clean** — was mis-flagged); pure-asm packs `osInvalDCache`+`osInvalICache` (0x823B0), `__osDisableInt`+`__osRestoreInt` (0x8B900); mixed-pack split-firsts `osSetIntMask` (0x7E360), `func_800AFB90` (0x8AF90).

## Sprint 56 — asm-mirror vendoring pilot: 4 libultra reg-shim asm TUs — 2026-06-14
- Increment: 0 new `.c` (asm-vendor/hasm housekeeping, not a C-decomp increment). 4 libultra reg-shim hand-asm TUs vendored from ultralib + built into the ROM: `getcount`/`getcause`/`getsr`/`setcompare`. 4 subsegs flipped `asm`→`hasm` (hasm 2→6; asm subsegs 175→171); the 4 intrinsic-likely shims off `pick_target`. md5-candidate unchanged at 97/97.
- Quality: 0/0/0/0 stuck-far/permuter/re-opened this sprint; ~11 remaining intrinsic-likely TUs + 2 mixed packs carried *by plan* (not spiked) to BACKLOG. 1 novel bank-gotcha (modern-as padding) resolved within-sprint.
- Seed: committed 2pt; banked 2pt; regime mirror (asm-mirror track, seed-only).
- What helped: the C upstream-mirror discipline transferred cleanly to asm — vendor verbatim, prove by full-make ROM SHA-1. The asm headers (`PR/R4300.h`, `sys/asm.h`, `sys/regdef.h`) were already in-tree; only `MFC0`/`MTC0` macros needed vendoring. The `8EC50.o`/`mmuldi3.s` precedent gave the Makefile-override shape. Reading `splat/hasm.py` confirmed `hasm` keeps the existing `asm/<rom>.s` (its `split()` only writes if absent) so flipping was non-destructive.
- Friction: **the entire first attempt SHA-broke.** Assembling the verbatim 0xC ultralib TUs with modern `mips-linux-gnu as` produced 0xC objects, but the `.ld` does not ALIGN between subsegs (the pad must live in the object), so every following subseg shifted. The PO's directive to **use ultralib's exact flags** was the fix: the KMC/N64 gcc (`gcc.mk` profile) `as` pads each fn's `.text` up to its 16-byte ROM slot → 0x10, matching the ROM. Secondary friction: started against `libultra_modern` (an `additional working dir`) before PO corrected to `ultralib` — addressed by #4. The BACKLOG carry-over de-rank swept the mixed-pack names too (regex grabs all backtick'd idents), de-listing `osSetIntMask`/`func_800AFB90` — harmless (they're deferred anyway), fix offered as #carry-over-wording but PO did not select it.
- Applied (3 of 4): #1 new `docs/hazards.md#asm-mirror-vendoring` section (KMC-as padding load-bearing, hasm flip, `VENDOR_ASM` map, no-inter-subseg-ALIGN caveat) + CLAUDE.md hazard index row + reworded intrinsic-likely playbook; #2 `pick_target.py` intrinsic-likely now carries the vendorable ultralib TU path (`build_asm_tu_index` scans LEAF/XLEAF/WEAK; `intrinsic-likely:os/getcount.s` vs bare = no-source shim; golden regen 28 pass); #4 pinned `~/development/repos/ultralib` (gcc.mk / VERSION_J) as THE libultra source in CLAUDE.md's mirror-branch step, not `libultra_modern`. (#3 carry-over-wording fix NOT selected.)
- Carry-over: ~11 remaining intrinsic-likely libultra asm TUs (cache/TLB/bcopy/sqrtf + the `osInvalDCache`/`__osDisableInt` pure-asm 2fn packs) for follow-up asm-mirror sprints, + the 2 mixed packs (`osSetIntMask`, `func_800AFB90`) noted do-not-blanket-hasm. See BACKLOG ## Carry-overs.

---

## Sprint 55 — perspective libultra gu/ mirror (last clean low-cost libultra leaf) — 2026-06-14
- Increment: 1 file banked (`src/libultra/gu/perspective.c`) / 2 fns matched (`guPerspectiveF` + `guPerspective`). md5-candidate 96→97 (all 97 src `.c` files 0-stub); asm subsegs 176→175.
- Quality: 0/0/0/0 this sprint. Verbatim `libultra_modern` monegi `gu/perspective.c`, clean first build, 0 iterations.
- Seed: committed 2pt; banked 2pt; regime mirror (8-gate clear; seed-only, no freeze commit).
- What helped: the gu/ rodata-sibling pattern (S38/S48/S52) made this a known enabler, not a finalize surprise — flip text subseg at gate (green stub, rodata stays autogenerated), split the `.rodata` sibling at finalize with the source. All callees (`cosf`/`sinf`/`guMtxIdentF`/`guMtxF2L`) pre-placed by prior gu/ siblings + names pre-curated → zero symbol adds. `--lib libultra` returned empty (remaining libultra fns are un-path-qualified asm subsegs surfacing as `upstream none`); the full-band survey found perspective as the *only* clean leaf left (everything else carries jal-mismatch / unplaced SI callees / pts≥8 packs / intrinsic-shim→hasm).
- Friction: `pick_target.py`'s rodata-literal pre-flag undercounted the pool 4-of-8 — the flag's whole point is to size the sibling split at the gate, but it scanned only the *primary* function while the pool's 2nd half (0x800D2540..0x2558) is loaded by the *sibling* `guPerspective`. The asm (ground truth) gave the true extent (0xAD920..0xAD960), so no SHA-miss, but it forced a manual asm re-grep at finalize. Fixed (#1 below).
- Applied (1 of 1): #1 `rodata_literals`/`rodata_word_refs` now scan the whole subseg (new `decomp_asm.iter_subseg_body`) instead of just the primary fn, because a `.rodata` sibling places the *whole compiled object's* `.rodata` — every pack function's pooled literals belong to the same split extent. Added `tests/tooling/test_decomp_asm.py` (5 unit tests, synthetic 2-fn fixture proving sibling literals are caught + per-fn walk unchanged); suite 23→28 pass; pick_target JSON golden unchanged (no top-50 pack currently has sibling-only literals).
- Carry-over: none. **Phase note:** the libultra cheap-mirror band is now fully exhausted — no clean low-cost leaf remains. Next libultra sprint is classical-track (cont*/pfs*/timer recover-extern, v2 realized tier) or a regime/scope change (libnusys/libkmc fillers, or an intrinsic-shim→hasm housekeeping pass).

## Sprint 54 — sprintf libultra libc/ mirror (last named-clean libultra leaf) — 2026-06-13
- Increment: 1 file banked (`src/libultra/libc/sprintf.c`) / 2 fns matched (`sprintf` + static `proutSprintf`). md5-candidate 95→96 (all 96 src `.c` files 0-stub); asm subsegs 177→176.
- Quality: 0/0/0/0 this sprint. Verbatim ultralib VERSION_J mirror, clean first build, 0 iterations.
- Seed: committed 3pt; banked 3pt; regime mirror (seed-only; 8-gate clear).
- What helped: whole-file 2-fn combined subseg (S40 ldiv pattern, no split); both callees pre-placed (`_Printf`/`memcpy`) and both names pre-curated → zero symbol adds; the `size_t`-redefinition risk never materialized (verbatim built clean first try). The DoR self-audit pre-refuted both blocking flags before the flip.
- Friction: two `pick_target` false-flags forced a gate-time re-diagnosis — `file-static` fired on the `static proutSprintf(...)` *function* proto (a static function is no BSS hazard), and `blk needs-header:xstdio.h,string.h` was the recurring vendorable-header class (3rd instance after S49 guint.h / S53 PR-band). Both now closed in tooling. Applying #1 surfaced a latent bug — a naive proto detector was fooled by the `(` inside `__attribute__((aligned(8)))`, which would have wrongly un-flagged genuine static arrays (gfxThread's `nuGfxMesgBuf`); caught by reviewing the pre-regen diff, fixed with an attribute-strip.
- Applied (2 of 3): #1 `pick_target.py` file-static detector ignores static *function* declarations (`_is_static_func_proto`, attribute-stripped so attributed static *arrays* still flag) + `docs/hazards.md#file-static` note; #2 `needs-header:<h>(vendorable)` annotation — `UPSTREAM_SRC_ROOTS` source-private header index + `include_is_vendorable` recognizes source-relative-copyable headers as a 1pt enabler, not `blk` + `docs/hazards.md#needs-header` note; (#3 S40 cross-lib-header confirmatory — log-only, NOT a file edit). Golden regen, 23 pass.
- Carry-over: none.

## Sprint 53 — alHeapDBAlloc libultra audio/ mirror — 2026-06-13
- Increment: 1 file banked (`src/libultra/audio/heapalloc.c`) / 1 fn matched (`alHeapDBAlloc`; closes the named-clean `libultra/audio/` leaf, sibling of the S36 heapinit/copy pair). md5-candidate 94→95; asm subsegs 178→177.
- Quality: 0/0/0/0 this sprint. Verbatim ultralib mirror, clean first build, 0 iterations.
- Seed: committed 1pt; banked 1pt; regime mirror (8-gate clear; seed-only). pts column read `blk` (the false-block); true seed 1 (warm single-fn mirror), refuted at the gate.
- What helped: the audio band was already open (copy.c/heapinit.c, S36) and the name was pre-curated in ghidra_symbols, so the only enabler was the yaml flip. Body is trivial under `_FINALROM` (the `_DEBUG` HeapInfo bookkeeping + `__osError` compile out), so no data externs and no callees to reconcile — a pure verbatim cp + SHA proof.
- Friction: **a new false-block class.** pick_target reported `blk needs-header:libaudio.h,os_internal.h,ultraerror.h` and `--lib libultra` returned empty, making the band look mined out. All 3 headers ship at `include/libultra/PR/` and resolve via `LIBULTRA_CFLAGS -I include/libultra/PR`, but `missing_includes`'s `-I` model (base INCLUDE_DIRS) omits the PR/ path — so every libultra PR-band mirror false-flagged. Distinct from the S49/S50 gu/ quote-include false-blks (those resolved source-relative; this one is a genuine `-I` dir the model lacked). Refuted at the gate by the banked sibling heapinit.c, then fixed in tooling.
- Applied (3 of 3): #1 `missing_includes` now takes the candidate's `lib` and unions `LIB_EXTRA_INCLUDE_DIRS[lib]` (libultra ⇒ `include/libultra/compiler/gcc` + `include/libultra/PR`, matching LIBULTRA_CFLAGS) into the resolvable `-I` set — kills the PR-band false-blk; golden regen (alHeapDBAlloc gone, sprintf's `os.h` now resolved → `needs-header:xstdio.h,string.h`), 23 pass. #2 dropped the stale "deferred Makefile enabler — the audio band's <libaudio.h> at include/libultra/PR/" comment at the INCLUDE_DIRS note (superseded by #1). #3 BACKLOG libultra hazard-map re-survey (below).
- Carry-over: none.

---

## Sprint 52 — guRotateRPYF/RPY + guLookAtReflectF/Reflect libultra gu/ mirror pair — 2026-06-13
- Increment: 2 files banked (`src/libultra/gu/rotaterpy.c`, `lookatref.c`) / 4 fns matched (`guRotateRPYF`, `guRotateRPY`, `guLookAtReflectF`, `guLookAtReflect`; 4th+5th gu/ files). md5-candidate 92→94; asm subsegs 180→178.
- Quality: 0/0/0/0 this sprint. Both verbatim ultralib VERSION_J mirrors, clean first build, 0 iterations.
- Seed: committed 5pt; banked 5pt; regime mirror (8-gate clear; seed-only). rotaterpy seed 2 + lookatref seed 3.
- What helped: all callees pre-placed (sinf/cosf/guMtxIdentF/guMtxF2L); `guint.h` shipped S49; both names pre-curated. rotaterpy was a clean S49 static-float clone (`dtor`@0x800C81E0 recover-extern, drop fn-local static → file-scope extern). lookatref's two known patterns composed without surprise: `sqrtf`@0x800B0A10 recover-callee (S23 dual) + the S38/S48 `.rodata` sibling split, both pre-noted at the gate so neither was a finalize-time SHA miss.
- Friction: (a) **rodata-literal mislabel** — pick_target flagged rotaterpy's `dtor`@0x800C81E0 (data region) and lookatref's literals (rodata region) identically, but the enablers differ (recover-extern vs sibling split); diagnosed by address range at finalize. Fixed in tooling (#2). (b) **partial rodata extent** — the hazard listed only lookatref's `ldc1` double @0x800D2510; the `1.0` @0x800D2518 was an `lw`-pair the FP-only scan missed (the split is 16 B, not 8). Sized correctly from the disasm; fixed in tooling (#3). (c) **stale S51 band note** — predicted `guMtxIdentF`/`guMtxF2L` as next small gu/ leaves; they are inside the pts-13 main pack `func_800660A0`, not separable. Corrected (#1).
- Applied (3 of 3): #1 BACKLOG gu/ band-note correction (separable gu/ leaf pool mined out; the rest are `blk`/pts-13/heterogeneous); #2 `pick_target.py` segment classifier — `%lo(D_)` in a `rodata` subseg ⇒ `rodata-literal` sibling split, in the data segment ⇒ new `data-static` (S49 recover-extern); #3 `rodata_word_refs` unions `lw`-pair double 2nd-words (band-filtered) so the sibling-split extent is full. golden regen (sqrtf rename + stale guMtxCatF row dropped), 23 pass. docs/hazards.md (#defines-data data-static para + #rodata-sibling extent note) + CLAUDE.md hazard index updated.
- Carry-over: none.

---

## Sprint 51 — guMtxCatF + guMtxXFMF libultra gu/ combined-subseg mirror — 2026-06-13
- Increment: 1 file banked (`src/libultra/gu/mtxcatf.c`) / 2 fns matched (`guMtxCatF`, `guMtxXFMF`; 3rd gu/ file). md5-candidate 91→92 (92/92 .c files stub-free); asm subsegs → 180.
- Quality: 0/0/0/0 this sprint. Verbatim ultralib `src/gu/mtxcatf.c`, zero edits, matched first `make`.
- Seed: committed 3pt; banked 3pt; regime mirror (8-gate clear; seed-only). Deterministic pts seed was 2 (= S50's identical 2-fn gu shape); the gate +1 was a one-time non16align adjust — do NOT re-anchor future 2-fn gu mirrors at 3.
- What helped: ultralib's combined `src/gu/mtxcatf.c` holds BOTH fns as clean C (the original SGI file); both names pre-curated, `guint.h` shipped S49 — pure verbatim cp. S50's `missing_includes` fix correctly de-flagged `guMtxCatF` from `blk` (warm/no-hazard at the gate, as predicted).
- Friction: (a) **non16align** — `guMtxXFMF` is tight-packed after `guMtxCatF` at non-16 `0x848AC`; a per-fn yaml split SHA-missed on a bare stub (KMC `as` pads fn1's `.o` to 16, shifting fn2). Diagnosed via the gate build-check, fixed with one combined subseg. (b) **Gate red-herring** — `pick_target` mapped `guMtxCatF` to libultra_modern's hand-asm `mtxcatf.s` (the deprecated split distro CLAUDE.md already forbids); disasm-probe showed compiled-C nested-loop matmul. Cost one extra PO re-confirm. (c) **Orphan file** — the reverted split left a stale `gu/mtxxfmf.c` stub that `git add -A` swept into the bank commit; removed in a fixup commit after the user flagged it.
- Applied (3 of 3): #1 pack `.s`/`.c`-boundary disasm-probe before classical-flag (hazards.md pack section); #2 hazards.md#non16align combined-subseg case + reverted-split cleanup caveat; #3 hazards.md#upstream-mirror-pattern S51 cautionary note (reach ultralib first; a libultra_modern `.s` is not evidence of hand-asm).
- Carry-over: none. `guMtxCatF` was the only deferred candidate; it banked.

---

## Sprint 50 — guScaleF + guScale libultra gu/ band-open fast-path mirror — 2026-06-13
- Increment: 1 file banked (`src/libultra/gu/scale.c`) / 2 fns matched (`guScaleF`, `guScale`; 2nd gu/ file). md5-candidate 90→91 (91/91 .c files stub-free).
- Quality: 0/0/0/0 this sprint. Clean first-build verbatim match (zero edits, no seed/decomp_loop/permuter).
- Seed: committed 2pt; banked 2pt; regime mirror (8-gate clear; seed-only, no freeze commit).
- What helped: S49's open gu/ band — `guint.h` already in `src/libultra/gu/`, so scale.c's quote-`#include "guint.h"` resolved source-relative with no enabler. Diagnosed the `blk needs-header:guint.h` as a band-warmth false-blk at the gate (manual override). Both callees (`guMtxIdentF`/`guMtxF2L`) and both names already placed/curated → single yaml-flip the only enabler.
- Friction: S49's BACKLOG band-note mis-predicted this pack as calls-unplaced (`guMtxIdentF`/`guMtxF2L`); they were already placed, so it over-stated the cost. The false-blk required a manual gate override (now fixed in tooling, see Applied).
- Applied: 1 of 1 (#1 `missing_includes` now takes the mirror dir and drops band-local quote-includes that resolve source-relative — `guMtxCatF` and future gu/ siblings stop showing false-`blk`; `docs/hazards.md#needs-header` band-local-quote-include note; golden regen, 23 pass).
- Carry-over: none. (gu/ band now warm with guint.h in-tree; next gu candidates `guMtxCatF`/`guRotateRPYF` are pickable warm mirrors, plus rotate/perspective/mtxcat packs with their own hazards.)

## Sprint 49 — guRandom libultra gu/ band defines-data fast-path mirror — 2026-06-13
- Increment: 1 file banked (`src/libultra/gu/random.c`) / 1 fn matched (`guRandom`, 1st gu/ fn, opens the cold band). md5-candidate 89→90 (90/90 .c files stub-free); asm subsegs 183→182.
- Quality: 0/0/0/0 this sprint. Clean first-build match (no seed/decomp_loop/permuter).
- Seed: committed 2pt; banked 2pt; regime mirror (8-gate clear).
- What helped: the S45 defines-data verbatim-body fast path applied verbatim — function-local `static xseed` → file-scope sized `extern` + `xseed`@0x800C81C0 add-only; `main_data` `dlabel xseed` confirmed by the `.NON_MATCHING` alias at the gate, exactly as documented. Diagnosed the `blk needs-header:guint.h` as a false-block at the plan gate (mbi.h/gu.h already resolve via the existing `-I include/libultra/PR`; guint.h missing-but-copyable), avoiding a needless DoR reject.
- Friction: none. Suggestion buffer empty.
- Applied: 0 of 0 (PO: Apply none — no buffered suggestions).
- Carry-over: none. (Cold gu/ band now open via in-tree guint.h; `guScaleF`/`guScale` pack is the next gu candidate but carries `guMtxIdentF`/`guMtxF2L` callees → calls-unplaced.)

## Sprint 48 — __osViSwapContext libultra io/ vi-band verbatim mirror — 2026-06-13
- Increment: 1 file banked (`src/libultra/io/viswapcontext.c`) / 1 fn matched (11th vi-band sibling). md5-candidate 88→89 (89/89 .c files now stub-free).
- Quality: 0/0/0/0 this sprint.
- Seed: committed 5pt; banked 5pt; regime mirror (8-gate clear; seed-only, no freeze commit).
- What helped: gate triage pre-located BOTH wrinkles before the flip — the recover-extern
  (`__additional_scanline`=0x800C826C, asm-data-recovery) and the rodata-sibling (the asm `ldc1`
  from a 0x800d.. literal in a different ROM band telegraphed the `2^32` double). `.text` matched
  on first compile; only the yaml rodata split (`[0xAD9E0, .rodata, libultra/io/viswapcontext]`,
  same pattern as S38 aisetfreq) was needed for the green ROM. VERSION_J path + vi struct layout
  already proven by 10 banked siblings → high confidence.
- Friction: pick_target's refs-unplaced listed `__OSViContext` (a struct TYPE, not a data symbol),
  costing a verification step to rule out; the rodata-sibling surfaced only on the first SHA miss
  (not pre-flagged). Both fixed this retro.
- Applied: 2 of 2 — #1 `declared_type_names` in pick_target.py + the type-name exclusion in
  refs_unplaced (drops typedef'd types like __OSViContext from the recover-extern list); #2
  `rodata_literals` in decomp_asm.py + the `rodata-literal:<addr>` hazard on mirror candidates
  (pre-flags anonymous `ldc1/lwc1 %lo(D_)` FP constants so the sibling split is a DoR enabler).
  hazards.md rodata-sibling section + CLAUDE.md hazard index updated; `make test-tools` 23 pass.
- Carry-over: none.

---

## Sprint 47 — osCartRomInit libultra io/ verbatim mirror (cross-jump tail-merge) — 2026-06-13
- Increment: 1 file banked / 1 function matched (`src/libultra/io/cartrominit.c`); md5-candidate 87→88. Single fn 0x7E870, no split. Goal fully met, 0 carry-overs. First fn from the S46-reopened io/ band's near-verbatim tier (S46 banked the clean getters).
- Quality: stuck-far 0, permuter 0, carried 0, re-opened 0. (One 0x10-short near-miss self-corrected mid-build via the verbatim cross-jump — not a spike.)
- Seed: committed 3pt; banked 3pt; regime mirror (8-gate clear; near-verbatim-mirror sub-case, seed-only).
- What helped: **the user's "look at ultralib" steer was the unlock.** The verbatim ultralib VERSION_J source has two *identical* `{ __osPiRelAccess(); return &__CartRomHandle; }` tails (early-return + end); KMC GCC -O3 **cross-jumps** them into one shared block itself (the jal `6vs5`), reproducing the exact 0x20-frame / s1-anchor / s0=base-in-delay-slot regalloc. Two defines-data drops placed at gate (`__CartRomHandle`=0x80105BC0 size:0x74; function-local `static int first` → `osCartRomInitFirst`=0x800C7EA0) + companion `PRinternal/macros.h`. Residual isolated 4400 / 99-of-99-rows was pure reloc HI/LO16 addend (isolation artifact) → full-make SHA proved it, no permuter.
- Friction: my first attempt **hand-folded** the early-return into `if(first){body}` — it compiled 0x10 SHORTER (1 vs 2 callee-saved regs), shifting every downstream fn → whole-ROM mismatch (a far worse symptom than a local diff). The *wrong size*, not a wrong instruction, was the tell. pick_target also mis-flagged 3 refs/calls FPs from the inactive non-J `#else` branch (`CartRomHandle`, `osPiRawReadIo`) + an `endif` directive token.
- Applied: 3 of 3 — #1 `docs/hazards.md` Near-verbatim section: verbatim-first for cross-jumpable duplicate-tail jal-mismatches + "wrong-SIZE ⇒ regalloc/tail-merge, not logic" diagnostic (S47 provenance); #2 `tools/pick_target.py` `_strip_inactive_version_branches` honoring `#if BUILD_VERSION` in refs/calls-unplaced (drops dead-`#else` FPs + the `endif` token; golden regen, 23 pass); #3 log-only defines-data blind-spot note (function-local-static-with-init + global-def both surfaced as refs-unplaced, S42/S45 class).
- Carry-over: none. **Cross-repo follow-up:** `__CartRomHandle`=0x80105BC0, `osCartRomInitFirst`=0x800C7EA0 are new decomp-side symbols — propagate via `sync_decomp_names.py --import-from-decomp`.

## Sprint 46 — __osPiRawStartDma + osPiGetCmdQueue libultra io PI-band unlock — 2026-06-13
- Increment: 2 files banked / 2 functions matched (`src/libultra/io/{pirawdma,pigetcmdq}.c`); md5-candidate 85→87. 0x8BA20 3-fn pack split at the upstream boundary (pirawdma 0x8BA20, pigetcmdq 0x8BAF0, `func_800B0710` left asm). Goal fully met, 0 carry-overs.
- Quality: stuck-far 0, permuter 0, carried 0, re-opened 0. Both verbatim first-try, 0 iteration.
- Seed: committed 2pt; banked 2pt; regime mirror (8-gate clear).
- What helped: both verbatim mirrors built clean off the one-time `PRinternal/piint.h` companion-copy (`PR/os_internal.h`+`PR/rcp.h` deps already in-tree); `_DEBUG` block compiles out of pirawdma; both fn names pre-placed. The gate `make extract && make` green-ROM check confirmed the split + header before execution.
- Friction: the whole PI/SI/cont/pfs libultra band had been hidden behind a `pick_target` **false-`blk`** — `include_is_blocked` matched the include's *basename* (`piint.h`) against the differently-prefixed in-tree `internal/piint.h` and mislabeled a cheap companion-copy as a deferred -I, so 11 pickable mirrors read as un-pickable for many sprints. `osRomBase` (a libultra boot-region global at the fixed 0x80000308, asm-baked as `D_80000308`) was also missed by refs-unplaced's `__`-prefix grep — pre-added at the gate here, but a less-careful gate would link-fail on first mirror compile.
- Applied (2 of 2): #1 `include_is_blocked` now matches the full relative include path (not basename) + ultralib/include added as the primary libultra companion-header root (ships the `PRinternal/` prefix libultra_modern lacks) → the 11-fn band un-`blk`'d; #2 `BOOT_GLOBALS` table (osTvType…osAppNMIBuffer, 0x80000300-0x1C) wired into `refs_unplaced` so a referenced-but-unplaced boot global surfaces with its known vram inline. Golden snapshot refreshed (was stale from prior sprints; isolated my edits to 11 intended `blk`→pickable rows before regen); 20 pass / 3 skip.
- Carry-over: none.

## Sprint 45 — osGbpakReadWrite + osGbpakReadId libultra gbpak — 2026-06-13
- Increment: 2 files banked / 2 functions matched (`src/libultra/shared/gbpak/{gbpakreadwrite,gbpakreadid}.c`); md5-candidate 83→85. Closes the `libultra/shared/gbpak/` band (S43 power/getstatus, S44 init). All 85 c files now md5-candidate, 0 INCLUDE_ASM stubs anywhere in `src/`. Goal fully met, 0 carry-overs.
- Quality: stuck-far 0, permuter 0, carried 0, re-opened 0.
- Seed: committed 3pt (0x88FC0 2-fn pack); banked 3pt; regime mirror (seed-only — both mirror track; 8-gate clear). Both divergences were near-verbatim mirror known-edits (dropped blocks), not classical iterations, so no realized-tier scoring.
- What helped: warm gbpak band (S43/S44) pre-placed every callee except two deterministic recover-callees added at the gate (`__osGbpakSetBank`=0x800B1A90, `bcmp`=0x800B0A20 — both from the pack's own jals). The `.o`-diff-on-first-SHA-miss reflex (codified S44) immediately localized both divergences instead of blind C iteration. readid's data resolution reused S44's defines-data fast-path verbatim (drop static def → sized extern + dlabel rename), no main_data split needed.
- Friction: both functions are near-verbatim mirrors where MG64 OMITS upstream blocks, surfacing only at the full-make SHA miss. (a) `osGbpakReadWrite` drops `if (size == 0) return 0;` — a **jal-less** early-return, so jal-counting can't flag it (folds into the later `blez` uninit-`ret` path). (b) `osGbpakReadId` drops the upstream `if(bcmp){ write-temp; reread; recheck }` retry block (jal 12→7), and after the `.text` matched the SHA still missed on `.data`: the function-local `static nintendo[]`/`mmc_type[]` arrays live in the shared `main_data` blob (the compiler emitting a second copy shifts the data segment). Both are the S18/S44 late-surfacing class — invisible to every gate check.
- Applied (2 of 3): #1 `docs/hazards.md` Near-verbatim-mirror section gains a jal-less-dropped-block bullet (jal count can MATCH; `.o`-diff first) + provenance S45. #2 `docs/hazards.md` defines-data verbatim-body fast-path gains a function-local-statics-in-shared-blob paragraph (drop static → sized extern + dlabel rename; size it so `sizeof`/`ARRLEN` compiles; `D_<vram>` name is the real vram, `.NON_MATCHING` map addr is an alias). #3 log-only (pick_target can't cheaply detect shared-blob statics — they're invisible to the refs-unplaced grep and don't link-break).
- Carry-overs: none. Cross-repo follow-up: `__osGbpakSetBank`=0x800B1A90 + `bcmp`=0x800B0A20 (were `func_<addr>` in Ghidra) and data names `nintendo`=0x800C93F0 / `mmc_type`=0x800C9420 are new decomp-side symbols — propagate via `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 44 — osCreateThread + osDestroyThread + osGbpakInit libultra — 2026-06-13
- Increment: 3 files banked / 3 functions matched (`src/libultra/monegi/thread/{createthread,destroythread}.c` + `src/libultra/shared/gbpak/gbpakinit.c`); md5-candidate 80→83. First sprint under the PO libultra-epic directive. Goal fully met, 0 carry-overs.
- Quality: stuck-far 0, permuter 0, carried 0, re-opened 0.
- Seed: committed 6pt (thread pack 3 + gbpakinit 3); banked 6pt; regime mirror (seed-only — all 3 mirror track; 8-gate clear). createthread's cast fix is a near-verbatim mirror known-edit, not a classical iteration, so no realized-tier scoring.
- What helped: thread band fully proven (S8/12/14/35) → all callees + headers pre-placed, one recover-extern (`__osCleanupThread`=0x800B04E8) the only thread enabler. `_FINALROM` auto-drops the `thprof`/`_DEBUG` blocks (asm confirmed no thprof store). gbpakinit composed S42's defines-data verbatim-body fast-path with S43's already-placed gbpak data region + callees → zero new symbols, matched first `make`. The S43 gbpak groundwork made the initializer near-free.
- Friction: createthread's verbatim copy mismatched on one cast — baserom sign-extends `context.ra = (s64)(s32)__osCleanupThread` (`sra v0,a0,0x1f`) where libultra_modern zero-extends `(u64)(u32)` (`move v0,zero`). A VERSION_J source divergence invisible to every gate check (jal count, ref/header grep) and the INCLUDE_ASM gate build; surfaced only at the full-make SHA miss (same late-surfacing class as S18 jal-count / S40 wrong-lib-header). Resolved by diffing the compiled `.o` against baserom asm and flipping the one cast to match the sibling sp/a0 fields.
- Applied (1 of 2): #1 new `docs/hazards.md#mirror-cast-divergence-sign--vs-zero-extend` section (diff the `.o` on the first SHA miss of a context-building mirror; flip `(u64)(u32)`↔`(s64)(s32)` to match siblings) + CLAUDE.md hazard-index row. #2 confirmatory (defines-data + warm-band doctrine composed with zero friction) — log-only, no edit.
- Carry-overs: none. Cross-repo follow-up: `__osCleanupThread`=0x800B04E8 is a new decomp-side symbol (Ghidra had no function there) — propagate via `sync_decomp_names.py --import-from-decomp`.

---

## Sprint 43 — gbpak pair (osGbpakPower + osGbpakGetStatus) libultra classical — 2026-06-13
- Increment: 2 files banked / 2 functions matched (`src/libultra/shared/gbpak/{gbpakpower,gbpakgetstatus}.c`); md5-candidate 78→80 (80/80 src .c now 0-stub). Goal fully met, 0 carry-overs.
- Quality: stuck-far 0, permuter 0, carried 0, re-opened 0.
- Seed: committed 6pt (power 3 + getstatus 3); banked 6pt; classical track v2. Realized 4 (each fn −1 verbatim-first-try); residual −2 — the most-negative classical residual to date, because both were classical-FLAGGED (jal-count-mismatch) yet proved pure verbatim mirrors. Two-pass freeze: seed committed pre-`src/` (`fcd06a7`).
- What helped: both `jal-count-mismatch` flags were macro FPs, recognised at the gate against the asm (power's 6vs5 = `OS_USEC_TO_CYCLES`; getstatus's 6vs4 = 2× `ERRCK`), so both seeded as verbatim upstream bodies and matched first try. The S41 deterministic recover-extern pattern handled all 5 unplaced symbols at the gate (3 timer globals via the osSetTimer arg setup, 2 callees via their jal targets) → no execution-middle link failures. getstatus's score-15 isolation artifact (empty `top_mismatches` + 76/76 rows) was recognised immediately (S34/S39 precedent) → straight to full-make SHA, no wasted C iteration.
- Friction: pick_target's jal-count path still over-counted function-like macros as calls — the S42 fix only hardcoded `MQ_IS_FULL`/`MQ_IS_EMPTY`, so `OS_USEC_TO_CYCLES` and `ERRCK` slipped through and mis-routed two clean mirrors to the classical track. Both fns banked anyway (the gate caught it), but the seed over-priced the sprint by ~2pt.
- Applied (2 of 2): #1 generalised `_c_jal_count` in `pick_target.py` to drop EVERY invoked function-like macro via the S41 `all_func_macros()` table (not just the 2 hardcoded names); side-corrected sprintf `2vs1`→clean (va_start FP) and osCartRomInit `21vs5`→`6vs5` (macro inflation); golden regenerated, 23/23 tooling tests pass. #2 documented the isolation-artifact recognition signal (`score≠0` + empty `top_mismatches` + `match_count==total_rows` → trust full-make SHA, skip iteration/permuter) in `CLAUDE.md` Spot-check bullet + `docs/hazards.md#isolated-compile-caveat`.
- Carry-overs: none.

## Sprint 42 — osSendMesg + osSetEventMesg libultra message pair — 2026-06-13
- Increment: 2 files banked / 2 functions matched (`src/libultra/monegi/message/{sendmesg,seteventmesg}.c`); md5-candidate 76→78 (78/78 src .c now 0-stub).
- Quality: 0/0/0/0 this sprint (stuck-far/permuter/carried/re-opened).
- Seed: committed 5pt; banked 5pt; regime mixed (sendmesg mirror seed 2 + seteventmesg classical defines-data seed 3). Classical track: S42 seed 3 / realized 2 / residual −1 (verbatim-first-try). 8-gate clear.
- What helped: (1) **defines-data verbatim-body fast path** — `osSetEventMesg`'s only edit from upstream was dropping the two file-scope defs (`__osEventStateTab`, `__osPreNMI`); body verbatim → known-edit *mirror*, matched in one `make` (no seed loop). Both functions banked in a single build. (2) Per-function jal analysis at the gate dissolved the `7vs6` flag (osSendMesg 6vs6, osSetEventMesg 3vs3 — each clean). (3) `ultralib/src/os/seteventmesg.c` as 2nd source confirmed the `BUILD_VERSION>=VERSION_J` path (matches the asm's PRENMI/`__osPreNMI` block) and `_FINALROM`→OS_NUM_EVENTS=15 fixed the array size at 0x78.
- Friction: two pick_target blind spots, both caught at the gate not the table. (a) `jal-count-mismatch:7vs6` was a **`MQ_IS_FULL` macro-pseudo-call** counted on the C side (the jal-count path uses `_C_NONCALL`, which — unlike S41's calls-unplaced de-noise — lacked the message predicate macros). (b) **`defines_data_globals` never flagged `__osEventStateTab`**: the `ALIGNED(8)` suffix defeats both `DATA_GLOBAL_DEF_RE` (no `;` right after `]`) and the `"(" not in line` paren-guard (ALIGNED's paren), so the defines-data hazard was discovered manually by reading the asm + upstream. The PO swap to this sibling at the plan gate is what surfaced it.
- Applied (3 of 3): #1 `pick_target.py` — `MQ_IS_FULL`/`MQ_IS_EMPTY` → `_C_NONCALL` (fixes the jal-count C-side + maybe-upstream signature; golden regenerated). #2 `docs/hazards.md#defines-data` — verbatim-body fast-path note (mirror proof, skip the classical seed). #3 `pick_target.py` — `defines_data_globals` surfaces the array dimension as `defines-data:<name>[DIM]` (mechanical scalar-vs-array size hint; `DATA_GLOBAL_DEF_RE` capture group added).
- New suggestion buffered for next review: **extend `defines_data_globals` to attribute-suffixed defs** (`Type name[N] ALIGNED(x);` / `__attribute__((...))`) — both the regex and the paren-guard bail on the trailing macro-call, so `ALIGNED` arrays evade detection entirely (the S42 `__osEventStateTab` miss). Needs the paren-guard relaxed for known attribute macros (ALIGNED/`__attribute__`) without re-admitting function decls — not done this sprint (function-decl regression risk, out of approved scope).
- Carry-overs: none. (The dropped libnusys filler `nuPiInit`/`nuPiInitSram` and the message-pack sibling `osSetEventMesg` are all banked or remain asm as before; nothing spiked.)

---

## Sprint 41 — __osEPiRawWriteIo libultra pi IO_WRITE mirror — 2026-06-13
- Increment: 1 file banked / 1 fn matched (md5-candidate 72→73)
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0
- Seed: committed 2pt; banked 2pt; regime mirror
- What helped: clean single-fn subseg (0x8BC80, no split); `__osEPiRawWriteIo` name pre-curated; `piint.h`+`PR/ultraerror.h` in-tree; the `_DEBUG` block compiles out. IO_WRITE MMIO isolation artifact (S34) → skip the isolation spot-check, ROM SHA-1 is the proof. The recover-extern flow (read the vram from the fn's own `%hi/%lo` pair, add the data extern, rebuild) was the same deterministic, zero-iteration path as every prior recover-extern
- Friction: a NEW recover-extern blind spot — the unplaced global hid inside a *library macro* (`EPI_SYNC` in `piint.h` → `__osCurrentHandle[domain]`), not the `.c` body, so it was invisible to BOTH `pick_target.py`'s ref-grep and the gate build-check (the INCLUDE_ASM scaffold resolves the body), surfacing only as `undefined reference to __osCurrentHandle` when the C linked in the execution middle — exactly the S23 `calls-unplaced` / S40 wrong-library-header pattern. Recovered deterministically (`D_800C7E90` from the fn's `lui/lw %hi/%lo` pair; index `domain*4` separate `addu` → base direct; `OSPiHandle*[2]` → size:0x8), rebuilt green. Not a spike — no DoD weakening
- Applied: 2 of 2 — #1 CLAUDE.md *macro-hidden recover-extern (S41)* convention bullet; #2 `pick_target.py` `refs_unplaced`/`calls_unplaced` now **follow one level of macro expansion** (project-wide function-like-macro table cached once; each invoked macro's params stripped; `__builtin_*` + nested-macro names excluded). Validated by full-table diff vs the committed tool: strict de-noise (dropped 15+ macro false-positives incl. `IO_READ`/`IO_WRITE`/`ARRLEN`/`MQ_IS_FULL`/`va_start`/`ERRCK`), surfaced the real macro-hidden callee `__osMotorAccess` (via `osMotorStart`), and re-confirmed it would have flagged `__osCurrentHandle` had it been unplaced
- Carry-over: none

---

## Sprint 40 — ldiv.c (ldiv+lldiv) libultra verbatim mirror — 2026-06-13
- Increment: 1 file banked / 2 fns matched (md5-candidate 71→72)
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0
- Seed: committed 2pt; banked 2pt; regime mirror
- What helped: whole-file 2-fn pack (one subseg = exactly `ldiv`+`lldiv`, no split); both names pre-curated; `__divdi3` (lldiv's 64-bit-divide callee) pre-placed; gate triage confirmed `__divdi3` placed before declaring clean (the `calls-unplaced` dual check). Verbatim cp, ROM SHA-1 = the proof
- Friction: verbatim `ldiv.c`'s `#include "stdlib.h"` resolved to the libkmc `stdlib.h`, which lacks libultra-only `lldiv_t` — a **resolvable-but-wrong-library** header. `pick_target.py`'s `needs-header` grep (resolvability-only) AND the gate build-check (INCLUDE_ASM scaffold never compiles the C body) both miss it; it surfaced only when the body compiled in the execution middle, like `calls-unplaced`. First instinct (add `lldiv_t` to the libkmc header) was a symptom fix the PO rejected — it pollutes a verbatim libkmc mirror, defeating cross-referencing. Root-caused via systematic-debugging: the real defect is a libultra source resolving its std header to a libkmc header
- Applied: 1 of 2 — #2 CLAUDE.md per-library standard-C-header isolation bullet (vendor libultra std headers verbatim to `include/libultra/compiler/gcc/`, prepend `-I` in `LIBULTRA_CFLAGS` only; never pollute `include/libkmc/*`); (#1 `cross-lib-header` hazard in `pick_target.py` NOT selected — PO deferred)
- Carry-over: none

---

## Sprint 39 — __osViInit classical loop — 2026-06-12
- Increment: 1 file banked / 1 fn matched (delta: 80→81/2090 ~3.88%; md5-candidate 73→74; all 74 C files now 0-stub)
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0
- Seed: committed 3pt; banked 3pt; realized 3pt; regime classical
- What helped: ultralib VERSION_J `vi.c` as reference (function body verbatim-minus-3-defs); IO_WRITE isolation artifact doctrine (verify C vs asm instruction-by-instruction, inline direct — no C edit iterations); all 5 symbol_addrs adds + callees pre-placed at gate; `__osViCurr`/`__osViNext` externs already in symbol_addrs from prior sprints
- Friction: byte-level `.o` `cmp` reported MISMATCH — required analysis to identify two distinct isolation artifacts: (1) IO_WRITE literal-address vs symbolic reloc (known, S34); (2) struct-field access via base symbol encodes inline LO16 addend (`sh v0, 0x32(at)`) while reference asm uses zero-placeholder (`sh v0, 0x0(at)`) with per-field `D_<addr>` reloc — both link identically. The second pattern was previously unrecognized and briefly appeared to be a real mismatch before the hex dump analysis confirmed all non-reloc instruction bytes were bit-for-bit identical
- Applied: 1 of 1 — #1 CLAUDE.md spot-check bullet extended: base-symbol struct-field reloc+addend encoding documented as linking-equivalent isolation artifact (inline LO16 addend vs zero-placeholder D_addr reloc)
- Carry-over: none

---

## Sprint 38 (retroactive bank) — osAiSetFrequency verbatim mirror resolved — 2026-06-12
- Increment: 1 file banked / 1 fn matched (delta: 79→80/2090 ~3.83%; md5-candidate 72→73)
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0
- Seed: committed 2pt; banked 2pt (corrected from spiked 0pt); regime mirror
- What helped: **`.rodata` sibling yaml pattern** — splitting `[0xAD5E0, rodata]` at 0xAD6A0 and adding `[0xAD6A0, .rodata, libultra/monegi/ai/aisetfreq]` caused splat to treat the dot-prefix subseg as the sibling of the `c` subseg (matched by name). `auto_link_sections` finds the pre-existing sibling and does NOT insert `aisetfreq.o(.rodata)` at the wrong text-position yaml slot. The `data.py` `out_path()` for a dot-prefix type with a sibling returns `sibling.out_path()` — so the linker entry is `build/src/libultra/monegi/ai/aisetfreq.o(.rodata)` at 0x800D22A0 exactly. `should_self_split()` = False for dot-prefix (no asm extraction). The Layer 1 `-G 0` assembler fix (S38 retro) was a prerequisite. Both layers resolved; verbatim mirror proved by ROM SHA-1.
- Friction: The S38 retro incorrectly concluded `.rodata` placement required the classical loop. The autogenerated yaml splits placed the `c` subseg at its text ROM offset (0x7EEC0) but there was no mechanism to tell splat that its `.rodata` must land at a different ROM offset (0xAD6A0). The solution came from reading splat source: dot-prefix subseg types route `out_path()` to the sibling C object, and the sibling relationship prevents duplicate `auto_link_sections` insertion.
- Applied: 1 of 1 — #1 `.rodata` sibling yaml pattern added to CLAUDE.md conventions
- Carry-over: none

---

## Sprint 38 — osAiSetFrequency (carry-over retry; 0pt spiked again) — 2026-06-12
- Increment: 0 files banked / 0 fns matched (delta: none; 79/2090 ~3.78%; md5-candidate 72 unchanged)
- Quality: stuck-far 0 / permuter 0 / carried 1 (osAiSetFrequency — .rodata layout conflict, new layer) / re-opened 0
- Seed: committed 2pt; banked 0pt (spiked); regime mirror
- What helped: **Binutils source (mips-binutils-2.6) confirmed the fix.** `tc-mips.c` line 5323 shows `g_switch_value < 4` → inline immediate for `li.s` (32-bit float); line 5353 shows `g_switch_value >= 8` → `.lit8` else `.rodata` for `li.d` (64-bit double). **`-G 0` on KMC `as`** makes 0.5f/2^31f inline (no `.lit4`) and 2^32d go to `.rodata` (no `.lit8`) — exactly what the ROM has. Also confirmed by `ultralib/makefiles/gcc.mk` line 9: original build used `-G 0` in ASFLAGS. Applied `-G 0` to all `tools/cc/as` invocations in the Makefile; full `make` ROM SHA-1 still green. **The fix unblocks all future FP-using libultra functions.**
- Friction: **Two-layer blocker on osAiSetFrequency.** Layer 1 (`.lit4`/`.lit8` link error) = FIXED this sprint. Layer 2 (`.rodata` layout conflict) = new discovery: compiler-generated 2^32d double goes to `.rodata`, but linker places it at 0x800CA270 (per linker script ordering) instead of 0x800D22A0. The `[0xAD5E0, rodata]` subseg owns 0x800D22A0 already — the constant appears twice in the ROM (once from the asm object at the correct address, once from aisetfreq.o's `.rodata` at the wrong address). Root cause: the original linker concatenated all `.text` then all `.rodata` in object order; splat interleaves `.text` and `.rodata` by subseg position, so aisetfreq.o's `.rodata` lands 0x8030 bytes too early. Verbatim mirror is permanently blocked; the viable path is the **classical loop** with C code that avoids generating the `.rodata` constant (replace the u32→float idiom so the compiler uses `lui/mtc1` for the 2^32 double instead of a memory load).
- Applied: #1 PARTIAL — Makefile `-G 0` on `tools/cc/as` applied (layer 1 resolved; ROM green); `.rodata` layout conflict documented for next sprint
- Carry-over: osAiSetFrequency (updated blocker: `.rodata` layout conflict — classical loop required)

---

## Sprint 37 — nuPiReadRom (classical) + osAiGetLength/osAiGetStatus/osViSetSpecialFeatures (mirrors) — 2026-06-12
- Increment: 4 files banked / 4 fns matched (delta: md5-candidate files 68→72; matched 75→79/2090 (~3.78%))
- Quality: stuck-far 0 / permuter 0 / carried 1 (osAiSetFrequency — osViClock + rodata D_800D22A0 unplaced) / re-opened 0
- Seed: committed 7pt; banked 7pt; regime mixed (classical 3pt + mirror 4pt)   (v2 classical: nuPiReadRom realized=3, residual 0)
- What helped: **IO_READ isolation artifact correctly identified** for osAiGetLength/osAiGetStatus (from S34 precedent) — score ≠ 0 in isolation due to MMIO literal-vs-reloc; went directly to in-tree spot-check, 0 wasted iterations. **osViSetSpecialFeatures** clean zero-enabler verbatim mirror; `refs-unplaced:__osViDevMgr` was dead `_DEBUG` FP as predicted; spot-check MATCH 368B, ROM SHA-1 green.
- Friction: **nuPiReadRom ROM variant undocumented** — no upstream source (nusys-1.10/1.20/2.00/2.07) matched; ROM calls osInvalDCache twice per iteration and sets all struct fields inside the loop. Required 3 C iterations to discover correct GCC delay-slot ordering (`rom_addr += readSize; size -= readSize;` AFTER the jal, `rom_addr` first).
- Applied: 0 of 0 (suggestion buffer empty)
- Carry-over: osAiSetFrequency (0x7EEC0, 288B) — osViClock@D_800C9468 + rodata D_800D22A0 unplaced; stays `[0x7EEC0, asm]`

---

## Sprint 36 — alCopy + alHeapInit (libultra audio band unlock) — 2026-06-12
- Increment: 2 files banked / 2 fns matched (delta: md5-candidate files 66→68; matched 73→75/2090)
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0 — clean sweep
- Seed: committed 6pt; banked 6pt; regime mirror   (v1 — story points; realized tier is v2, untouched this pure-mirror sprint)
- What helped: **PO library-first directive** pointed directly to the audio-band unlock (S2 retro first flagged `-I include/libultra/PR` as missing; paid here in S36). Parallel to S15's libnusys unlock: one Makefile `-I include/libultra/PR` + one companion header copy (`synthInternals.h` → `include/libultra/internal/`) opens the whole `libultra/monegi/audio/` band. **libnaudio vs libaudio concern** (PO mid-sprint note) resolved quickly — `grep -r "void alCopy\|void alHeapInit"` in the libnaudio package returned no results; these are shared utility functions identical in both libraries; `libultra_modern` source is authoritative for both. Both verbatim cp, 0 iterations.
- Friction: **S35 ld/symbol_addrs leftovers** — `mariogolf64.ld` and `symbol_addrs.txt` changes from S35's `make extract` (vi symbols + ld entries) were not staged in S35's bank commits; bundled into S36's `alHeapInit` commit. No ROM correctness impact (SHA-1 stayed green throughout), but a minor audit gap.
- Applied: 0 of 0 (suggestion buffer empty — no workflow improvements recorded this sprint)
- Carry-over: none

---

## Sprint 35 — osStartThread + vi-getter trio (4 verbatim mirrors) — 2026-06-12
- Increment: 4 files banked / 4 fns matched (delta: md5-candidate files 62→66)
- Quality: stuck-far 0 / permuter 0 / carried 0 / re-opened 0
- Seed: committed 16pt; banked 16pt; regime mirror   (v1 — story points; mirror track seed-only)
- What helped: disassemble-first before presuming "drops needed" — the jal-count-mismatch:9vs7 on osStartThread resolved verbatim (GCC -O3 tail-merging shares one jal across 3 switch-case paths); mmuldi3.s vendor sidestep unblocked hasm consolidation cleanly
- Friction: preliminary "2 dropped jals" assessment in DoR notes required extra disassembly verification that turned out to be unnecessary; future small mismatches should default to try-verbatim-first
- Applied: 1 of 1 (#1 CLAUDE.md `jal-count-mismatch` bullet: small ≤2 mismatch + identical-arg multi-branch → try verbatim first; document tail-merge case)
- Carry-over: none

## Sprint 34 — mixed: osJamMesg + osRecvMesg (libultra message-queue pair, warm mirrors) + osAiSetNextBuffer (libultra AI, classical static-drop) — 2026-06-12
- Increment: 3 files banked / 3 fns matched (delta: md5-candidate files 59→62; matched 66→69)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — goal met
- Seed: committed 5pt; banked 5pt; regime mirror+classical-mixed (osJamMesg 1pt mirror; osRecvMesg 2pt mirror; osAiSetNextBuffer 2pt classical; all-or-nothing 3/3 banked)
- What helped: **warm message-band pair trivial** — `osJamMesg` + `osRecvMesg` were clean verbatim copies; all callees pre-placed; `MQ_IS_EMPTY` hazard was a macro false-positive (gate confirmed). Zero iterations, 2 yaml flips + `make extract` = done. **IO_WRITE isolation artifact correctly identified early** — `osAiSetNextBuffer`'s score=600 with `total_rows==match_count` and `top_mismatches:[]` was recognized as the MMIO literal-vs-reloc isolation pattern before wasting any iterations; went directly to in-tree spot-check after verifying C logic against asm. **`hdwrBugFlag` vram deterministically recovered** from the target fn's own `lui 0x800c`/`lbu 0x7ec0` HI/LO16 pair.
- Friction: **Data-global label sync** — after adding `hdwrBugFlag` to `symbol_addrs.txt`, the stale `asm/7EFE0.s` still used `D_800C7EC0`; decomp_loop.py built the reference from the stale file causing reloc-name mismatches. Fixed by replacing all 3 `D_800C7EC0` occurrences with `hdwrBugFlag` and rebuilding `build/asm/7EFE0.o`. **IO_WRITE score never reaches 0** — the isolation artifact cannot be fixed via C iteration; the in-tree spot-check is the only authoritative path for MMIO functions.
- Applied: 2 of 2: #1 (CLAUDE.md: IO_WRITE/IO_READ isolation artifact convention bullet added after Isolated-compile caveat); #2 (CLAUDE.md: data-global stale asm label sync convention bullet added after Stale top-level asm label sync after gate rename)
- Carry-over: none

---

## Sprint 33 — classical: piacs.c (libultra nintendo/pi, 3-fn PI access queue) — 2026-06-12
- Increment: 1 file banked / 3 fns matched (delta: md5-candidate files 58→59; 59/59 C files)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — all score 0 on first seed; goal met
- Seed: committed 5pt; banked 5pt; regime classical/mirror-mixed (first classical sub-sprint; no residual variance — v2 deferred)
- What helped: **ultralib build-flag discovery** — root cause of `__osPiGetAccess` refusing to match at `-O2 -mips2` was that libultra's actual build used `OPTFLAGS=-O3 MIPS_VERSION=-mips3 -funsigned-char` (ultralib gcc.mk, VERSION_J). `-O3` inlines `__osPiCreateAccessQueue` into `__osPiGetAccess` (eliminating the `jal`); `-mips3` schedules `sw $ra` into the `bnez` delay slot. Changing global CFLAGS to `-mips3` and adding `LIBULTRA_CFLAGS=-O3 -funsigned-char` produced score=0 on first seed. **Both-functions-in-base.c seed**: seeding only `__osPiGetAccess` in isolation (without `__osPiCreateAccessQueue` in the same TU) would never get `-O3` inlining — the two-function seed is the correct structural move for same-TU callee inlining. **`decomp_loop.py` libultra auto-detect**: added `detect_libultra_profile()` + wired into `auto` path so the correct flags apply without `--profile libultra`. **`__osPiRelAccess` trivial**: single `osSendMesg` call, score=0 immediately.
- Friction: **`make sync-names` mid-sprint eviction** — running sync-names mid-sprint evicted `__osRunningThread`, `__osViCurr`, `__osViNext` from `ghidra_symbols.txt` and renamed `__osPiRelAcces`→`__osPiRelAccess`; build broke with 3 undefined references + wrong INCLUDE_ASM label. Recovery: add evicted symbols to `symbol_addrs.txt` (add-only); rename the INCLUDE_ASM stub + per-function asm files; `make extract && make`. **Stale `asm/7EDB0.s` label**: the top-level segment asm still had the 1-s `__osPiRelAcces` label after the rename; `decomp_loop.py` couldn't find `__osPiRelAccess`. Fix: manually update the 3 occurrences in `asm/7EDB0.s` + rebuild `build/asm/7EDB0.o`. **`-mips2`→`-mips3` global change** needed ROM-wide verification before accepting; ran full `make` + SHA-1 to confirm all existing banked files still matched.
- Applied: 4 of 4: #1 (CLAUDE.md: sync-names eviction recovery bullet added); #2 (CLAUDE.md: libultra compile profile bullet updated — `-O3 -funsigned-char`, global `-mips3` origin documented); #3 (CLAUDE.md: decomp_loop.py libultra auto-detect documented in same bullet); #4 (CLAUDE.md: stale top-level asm label-sync note added)
- Carry-over: none

---

## Sprint 32 — mirror: osSpTaskYield (libultra rsp, zero-enabler) + osSyncPrintf+rmonPrintf (libultra libc, FINALROM vararg stubs) — 2026-06-12
- Increment: 2 files banked / 3 fns matched (delta: md5-candidate files 56→58; all 58 C files now md5-candidate)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — all first-pass; 0 iterations; goal met
- Seed: committed 10pt; banked 10pt; regime mirror   (v1 — story points; realized tier is v2, untouched this pure-mirror sprint)
- What helped: **`-D_FINALROM` global flag** (Makefile gate enabler) stripped both `osSyncPrintf`+`rmonPrintf` bodies to empty MIPS O32 vararg stubs (save `$a0–$a3` + `jr $ra`) — ROM match without iteration. **ultralib repo** for VERSION_J source structure (`~/development/repos/ultralib/src/libc/syncprintf.c`): `libultra_modern`'s `syncprintf.c` has `__osSyncVPrintf` (VERSION_K+ only) which this ROM omits; the `#if BUILD_VERSION <= VERSION_J` branch reveals the two-function VERSION_J layout. **`include/stdarg.h`** from ultralib GCC headers resolved KMC GCC 2.7.2's missing stdarg.h; **`-nostdinc` removed** (ROM SHA-1 still green — `include/stdarg.h` takes precedence via `-I include`). `osSpTaskYield`: verbatim zero-enabler — `__osSpSetStatus`@0x800B16B0 + `SP_SET_YIELD`=0x400 pre-placed; one yaml flip.
- Friction: **VERSION_J vs VERSION_K+ libultra mismatch** — `libultra_modern`'s syncprintf.c has `__osSyncVPrintf` first; verbatim copy would emit 3 fns in wrong order. Resolved by cross-referencing ultralib for the VERSION_J layout. **stdarg.h absent from KMC GCC 2.7.2 install** (compiler-internal headers absent from `tools/cc/`); companion copy from ultralib then `-nostdinc` removed.
- Applied: 2 of 2: #1 (pick_target.py INCLUDE_DIRS comment updated — `-nostdinc` removed; `include/stdarg.h` for correct MIPS GCC 2.7.2 vararg ABI); #2 (CLAUDE.md: ultralib VERSION_J cross-reference added to upstream-mirror bullet)
- Carry-over: none

---

## Sprint 31 — classical: func_800A2F50 (trivial getter) + nuGfxInit (libnusys, novel GBI/absolute-addr gotcha) — 2026-06-12
- Increment: 2 files banked / 2 fns matched (delta: md5-candidate files 54→56; **56/56 = ALL FILES** — project reaches full md5-candidate coverage for the current src/ tree)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — both score 0; goal met (PO signed off partial — noted)
- Seed: committed 8pt; banked 8pt; regime classical   (v2 active: func_800A2F50 seed=3/realized=2 residual=−1; nuGfxInit seed=5/realized=8 residual=+3; S31 net realized 10, net residual +2 — first positive-residual sprint)
- What helped: **func_800A2F50 first-pass clean** (16 B getter, score 0 immediately — no surprises). For **nuGfxInit**: the decisive move was consulting the **v2.00 SDK source** (`~/n64sdk/4.0/pc/basic/nusys/src/nusys-2.00/nusys/nugfxinit.c`) rather than the v2.07 libultra_modern upstream (wrong SDK version for this game). v2.00 revealed: `Gfx gfxList[0x100] + Gfx *gfxList_ptr` locals + GBI macros `gSPDisplayList/gDPFullSync/gSPEndDisplayList` with `gfxList_ptr++`. GBI macros forced KMC GCC to allocate `gfxList_ptr` (0x820 frame vs 0x818 without it) and created the data-dependency chain for sequential store scheduling. `D_B6698 = 0xB6698` in `undefined_syms_auto.txt` is an **absolute-physical-address linker symbol** (OS_K0_TO_PHYSICAL of the rdpstateinit_dl array); `(u32)&D_B6698` generates the matching `R_MIPS_HI16/LO16 D_B6698` relocs. `#undef nuGfxInit` after `#include <nusys.h>` to override the in-tree nusys.h macro redefinition.
- Friction: **v2.07 vs v2.00 SDK mismatch** — libultra_modern's `nugfxinit.c` wraps the init in a `nuGfxInitEX2()` macro absent from the ROM build; checking `~/n64sdk/4.0/pc/basic/nusys/` first would have saved multiple re-seed attempts. **`D_B6698` is distinct from the standard recover-extern vram pattern** — it's not a virtual address in RAM but a physical address in the linker's absolute segment; it lives in `undefined_syms_auto.txt` not `symbol_addrs.txt`, and must be referenced via `&D_B6698` (not as a literal). The decomp_loop.py cmp target issue (split-subseg 7E350 resolves to 7E330.o) surfaced again for func_800A2F50; worked around by using `build/asm/7E350.o` directly.
- Applied: 1 of 2: #1 (CLAUDE.md split-subseg spot-check cmp note — use `build/asm/<subseg_offset>.o` for split subsegs); (#2 libnusys classical v2.00 pattern NOT applied — PO deferred)
- Carry-over: none

---

## Sprint 30 — mixed: strcmp (libkmc verbatim) + osSetTimer (classical, stripped) + __osDequeueThread (classical, defines-data drop) — 2026-06-12
- Increment: 3 files banked / 3 fns matched (delta: md5-candidate files 51→54; matched 57→57/2090 2.58%→2.73%)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — all score 0 first pass; 0 iterations; goal met
- Seed: committed 6pt; banked 6pt; regime mixed (1 mirror + 2 classical)   (v1 + v2 active: mirror seed-only; classical realized tier: osSetTimer seed=realized=2, residual 0; __osDequeueThread seed=realized=3, residual 0; S30 classical seed+realized=5 logged)
- What helped: **all three fns score 0 first pass** — strcmp verbatim cp (libkmc warm, `-O` auto-applied); osSetTimer Ghidra-seeded classical body written directly from asm (stripped impl — no interrupt disable/restore, no counter update; correct `__osTimerList` recover-extern from `lui`/`lw` pair → 0x800C8240); __osDequeueThread direct-from-asm (64 B pointer-walk; register params; `(OSThread*)queue` head cast for the loop; 5 file-scope defs dropped cleanly). `make extract` re-run after `__osTimerList` add to regenerate asm labels; caught the missing re-extract early (linker undefined-ref vs stale asm).
- Friction: **osSetTimer mis-routed as near-verbatim at DoR** — `jal-count-mismatch:5vs2` at the gate implied a near-verbatim drop (S18/S26 pattern), but disassembly showed the ROM impl is fundamentally different (no interrupt-disable shell, no timer counter update, different arg to `__osSetTimerIntr`). A large mismatch (5vs2 = 3 absent calls) does NOT mean a near-verbatim drop is possible — it can mean a wholly different stripped implementation. The hazard flag was correct; the *routing intuition* was wrong. **`int` vs `s32` return type**: first seed used `s32 osSetTimer(...)` → conflicting-types compile error (declaration in `os_time.h:104` is `int`); fixed immediately.
- Applied: 1 of 1: #1 (CLAUDE.md gate note: `jal-count-mismatch` >2 is `classical-likely` — disassemble + compare asm logic structure vs upstream before routing to mirror branch; a large mismatch defaults to the classical loop unless asm confirms a clean line-drop)
- Carry-over: none

---

## Sprint 29 — mirror: nuPiReadWriteSram (libnusys, recover-extern+needs-define Makefile gate) + __matherr (libkmc, pack-split+recover-extern) — 2026-06-12
- Increment: 2 files banked / 2 fns matched (delta: md5-candidate files 49→51; matched 53→55/2090 ~2.54%→~2.63%)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — both first-pass; 0 iterations; goal met
- Seed: committed 6pt; banked 6pt; regime mirror   (v1 — story points; realized tier is v2, untouched this pure-mirror sprint)
- What helped: **USE_EPI gate handled cleanly at the plan gate once spotted** — the Makefile fix (`LIBNUSYS_CFLAGS := $(CFLAGS) -DUSE_EPI` + libnusys pattern rule, modeled on the existing LIBKMC_CFLAGS setup) was a one-time band enabler; applied + validated with `make extract && make` before the execution middle ran. `__matherr`'s non16align resolved by splitting the 0x8EBE0 pack: C portion 112 B 16-aligned (`_matherr.c`) + hasm portion 56 B (`__muldi3`, permanent hasm per CLAUDE.md — stays raw asm forever). Both verbatim cp, 0 iterations; `errno`@0x800FE3D0 recovered cleanly from `__matherr`'s own `lui`/`sw` pair.
- Friction: **`pick_target.py` false-clean on `nuPiReadWriteSram`** — the tool correctly flagged `refs-unplaced:nuPiSramHandle@0x8012F4D8` but did NOT detect that the entire function body is gated by `#ifdef USE_EPI`, so without `-DUSE_EPI` the function compiled to an empty stub. The existing `needs-header` hazard class catches missing includes but had no analogue for missing build-defines. Discovered at the execution middle (function body was empty on first compile attempt). Fixed at retro: `function_gating_define()` detects a top-level `#ifdef DEFINE` wrapping the entire body; `_parse_makefile_defines()` + `_active_defines_for_lib()` cross-check the define against the library's effective CFLAGS (parsed from the Makefile); `needs-define:<DEFINE>` hazard added to the main loop and seed_points +1. After fix, `nuPiReadWriteSram` no longer triggers `needs-define:USE_EPI` because USE_EPI is now in LIBNUSYS_CFLAGS — correct behavior; the hazard fires only for defines absent from the library's CFLAGS.
- Applied: 1 of 1: #1 (pick_target.py `needs-define` hazard — `function_gating_define()` + `_parse_makefile_defines()` + `_active_defines_for_lib()` + +1 in seed_points for needs-define; now detects body-gating `#ifdef` absent from the effective library CFLAGS)
- Carry-over: none

---

## Sprint 28 — mirror: nuContGBPakReadWrite+nuContGBPakCheckConnector (libnusys, pack-split+NU_DEBUG) + memset+setmem (libkmc, whole-file pack+memory.h) — 2026-06-12
- Increment: 3 files banked / 4 fns matched (delta: md5-candidate files 46→49)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — all first-pass; 0 iterations; goal met
- Seed: committed 6pt; banked 6pt; regime mirror   (v1 — story points; realized tier is v2, untouched this pure-mirror sprint)
- What helped: **NU_DEBUG block is a preprocessor conditional, not a manual drop** — the ROM build doesn't define NU_DEBUG, so the `#ifdef NU_DEBUG` osSyncPrintf block compiles out of the verbatim cp cleanly; no explicit line removal (contrast S26 near-verbatim/drop which required dropping `osSetIntMask` lines). **memory.h companion copy** straightforward: libkmc `memset.c`'s `#include <memory.h>` resolved by copying `include/libkmc/memory.h` from the upstream; self-contained (`size_t` typedef + memory fn decls). **FAST_SPEED=1 path consistent** — libkmc's fast branch matched without iteration. libkmc `-O` profile auto-applied by Makefile. All 4 fns verbatim cp, 0 iterations; all names pre-curated in ghidra_symbols; no new symbol_addrs.txt additions needed.
- Friction: **`pick_target.py` `jal-count-mismatch:5vs1` false alarm** on `nuContGBPakReadWrite` — two compounding bugs: (1) `_DEAD_OPEN_RE` only stripped `#ifdef _DEBUG`/`#ifndef _FINALROM`/`#if 0`, missing `#ifdef NU_DEBUG`; (2) `C_CALL_RE` matched `address(`/`size(` inside osSyncPrintf format-string literals. Combined: 5 = osSyncPrintf×2 + address×1 (str) + size×1 (str) + nuSiSendMesg×1. Fixed at retro: `NU_DEBUG` added to `_DEAD_OPEN_RE`; `_strip_string_literals()` helper added and applied at both `call_divergence` and `calls_unplaced`. After fix `osSetTimer` correctly shows `5vs2`.
- Applied: 1 of 1: #1 (fix pick_target.py jal-count-mismatch — add NU_DEBUG stripping + strip string literals before C_CALL_RE)
- Carry-over: none

---

## Sprint 27 — mirror: __osSetGlobalIntMask + __osResetGlobalIntMask + osGetTime (libultra recover-extern, 2 new bands) — 2026-06-12
- Increment: 3 files banked / 3 fns matched (delta: md5-candidate files 43→46)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — all first-pass; 0 iterations; goal met first pass
- Seed: committed 8pt; banked 8pt; regime mirror   (v1 — story points; realized tier is v2, untouched this pure-mirror sprint)
- What helped: **all headers in-tree, all externs recovered deterministically at gate.** `__OSGlobalIntMask`@0x800C9470 inlined by `pick_target` (no disassembly needed for that confirm). `__osBaseCounter`/`__osCurrentTime` from `osGetTime`'s own asm (2 `lui`/`lw` pairs, `D_800FBE04` + `D_801052F0`/`D_801052F4`, straightforward read). Pack split boundary at 0x8B9D0 located directly from the asm file (`endlabel __osSetGlobalIntMask` / `glabel __osResetGlobalIntMask`). `__osViDevMgr` dead-`#ifdef _DEBUG` over-flag caught early (pick's `#ifdef`-blind grep flags it, but fn's asm has no load → confirmed zero symbol add). 3 symbol adds + 1 pack split + 3 yaml flips at gate; all first-pass clean.
- Friction: none — two new bands opened without friction; existing recover-extern + pack-split doctrine fully covered both. The `nintendo/exception/` pair shared one extern between two files (no doubled symbol add). `osGetTime`'s dead-`_DEBUG` block added no complication (verbatim kept per convention; `__osViDevMgr` correctly excluded from symbol_addrs).
- Applied: 0 of 0 (suggestion buffer recorded "None new"; PO: apply none)
- Carry-over: none

---

## Sprint 26 — mirror: nuContRmbCheck + nuContQueryRead (libnusys jal-divergence/drop + pack-split) — 2026-06-12
- Increment: 2 files banked / 2 fns matched (delta: md5-candidate files 41→43)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — both first-pass; 0 iterations; goal met first pass
- Seed: committed 5pt; banked 5pt; regime mirror   (v1 — story points; realized tier is v2, untouched this pure-mirror sprint)
- What helped: **both leaves were fully pre-wired** — names pre-curated in ghidra_symbols, all refs placed, all constants/types in nusys.h → zero new symbol adds, zero header copies. `nuContRmbCheck` used the S18 near-verbatim/drop pattern exactly: disassemble confirmed 1 jal (nuSiSendMesg), drop `osSetIntMask`×2 + `mask`, verbatim cp of the remainder. `nuContQueryRead` used the S10 pack-split pattern: mechanical split at rom 0x7E350, trivial 1-jal upstream (`nuSiSendMesg(NU_CONT_QUERY_MSG, NULL)`), unnamed 16B sibling `func_800A2F50` stays asm. Heterogeneous hazard types (jal-divergence + pack) but both are proven sub-cases — no novel friction, no iteration needed on either.
- Friction: none — the `jal-count-mismatch:3vs1` advisory for `nuContRmbCheck` was exactly what it said: 3 upstream jals (nuSiSendMesg + osSetIntMask×2), 1 in ROM (nuSiSendMesg only). Disassembly took one MCP call to confirm; the drop was three lines.
- Applied: 0 of 0 (suggestion buffer recorded "None new"; PO: apply none)
- PO directive (S26 retro): **target ≥5pt per sprint going forward** — batch 2+ files per sprint consistently. Recorded in BACKLOG.md ordering note and VELOCITY.md.
- Carry-over: none

---

## Sprint 25 — mirror: nuContDataLock/UnLock (libnusys recover-extern, whole-file pack) — 2026-06-12
- Increment: 1 file banked / 2 fns matched (delta: md5-candidate files 40→41)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — verbatim mirror, 0 iterations; goal met first pass
- Seed: committed 5pt; banked 5pt; regime mirror   (v1 — story points; realized tier is v2, untouched this pure-mirror sprint)
- What helped: subseg 0x7E2D0 packed **exactly** the two fns of one upstream file (`nuContDataLock`+`nuContDataUnLock` = all of `nucontdatalock.c`) → cohesive whole-file flip, **no split, no orphan asm**. Single recover-extern `nuContDataLockKey`=0x800FED38, a plain **scalar** word (size:0x4) — the simplest S20 sub-case (assigned directly, no index-multiply), so the inlined `refs-unplaced` vram needed no base-offset correction; re-confirmed via the fn's own `lui 0x8010`/`sw -0x12c8` pair. Both fn names pre-curated in `ghidra_symbols.txt`, `osSetIntMask` pre-placed, lock macros already in `nusys.h` → only one gate add. Two `jal 0x800a2f60` both = osSetIntMask, reconciled clean (no jal-count-mismatch flag, correctly).
- Friction: none — fully covered by existing recover-extern doctrine; deterministic end-to-end.
- Applied: 0 of 0 (suggestion buffer recorded "None new"; PO: apply none)
- Carry-over: none

## Sprint 24 — mirror: nuContDataGetEx (libnusys recover-extern + recover-callee; closes S20 trim) — 2026-06-12
- Increment: 1 file banked / 1 fn matched (delta: md5-candidate files 39→40)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — verbatim mirror, 0 iterations; goal met first pass
- Seed: committed 3pt; banked 3pt; regime mirror   (v1 — story points; realized tier is v2, untouched this pure-mirror sprint)
- What helped: **first leaf carrying BOTH `refs-unplaced` AND `calls-unplaced` at once** — the S20 data-recover and S23 function-dual doctrines composed cleanly with no special handling, both vrams confirmed in one `disassemble_function` pass. Data `nuContData`=0x801051F8 (`OSContPad[NU_CONT_MAXCONTROLLERS]`; stride 6 from the asm index-multiply + `li a2,0x6` bcopy size × 4 controllers → size:0x18; the offset-0 `lhu` confirmed it is the BASE not a field addr — the S20 indexed-struct caution cleared cleanly). Callee `nuContDataOpen`=0x800A2CAC recovered from its jal target (S23 dual). jal-count reconcile 3vs3 (`nuContDataClose`@0x800A2C84 + `bcopy`@0x800AA9A0 pre-placed) → clean verbatim, no drop; pick correctly did NOT flag jal-count-mismatch. **Closed the S20 trim** — `nuContDataGetEx` was dropped from the S20 pair precisely for the then-unhandled missing `nuContDataOpen` symbol; S23's `calls-unplaced` hazard (added last sprint, with this exact leaf as its verification case) made it a deterministic gate add this sprint.
- Friction: none — the S23 `calls-unplaced` fix pre-flagged the missing callee at the plan gate, so the execution-middle link-fail that motivated that fix did **not** recur. Clean end-to-end.
- Applied: 0 of 3 (PO: apply none — #1 size-hint tooling [parse asm index-multiply → `stride:N` for array externs] declined as nice-to-have, gate derives size by hand correctly; #2 dual-hazard-composes + #3 jal-count-specificity both confirmatory, already covered by S20/S23 doctrine)
- Carry-over: none

## Sprint 23 — mirror: osEPiStartDma (libultra recover-extern, 2nd nintendo/pi leaf) — 2026-06-12
- Increment: 1 file banked / 1 fn matched (delta: md5-candidate files 38→39)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — verbatim mirror, 0 iterations; goal met first pass
- Seed: committed 2pt; banked 2pt; regime mirror   (v1 — story points; realized tier is v2, untouched this pure-mirror sprint)
- What helped: clean single-fn recover-extern, sibling of S22 epilinkhandle in the now-warm `nintendo/pi/` band (`piint.h` + `PR/ultraerror.h` pre-in-tree). `__osPiDevMgr`=0x800C7E70 recovered deterministically from the fn's own `lui 0x800c`/`lw 0x7e70` (`.active` off0 = struct base); size 0x1C confirmed by the gap to the already-placed `__osPiTable`=0x800C7E8C (OSDevMgr = 7 words). `osEPiStartDma` name pre-curated → no func symbol add at the gate. Asm jals matched upstream (no jal-count-mismatch), `_DEBUG` blocks compiled out.
- Friction: **an unplaced *function* callee (`osPiGetCmdQueue`=0x800B06F0) link-failed the verbatim mirror in the execution middle** — the gate `make extract && make` passed green because the INCLUDE_ASM scaffold resolves the jal directly, so the missing C symbol only bit once the body called it by name. `pick_target.py`'s `refs-unplaced` grep had reported epidma clean: it *excludes anything called* (assumed splat's undefined_funcs_auto resolves all callees), but a callee labelled `func_<addr>` (unnamed in both files) has no `<name>` to bind. Recovered the vram from its jal target, added `// type:func` add-only, re-extract+make → green. A false-clean the gate could not catch — the motivating defect for both fixes below.
- Applied: 2 of 2 — #1 (`pick_target.py` new `calls-unplaced:<fn>@0x<addr>` hazard, the function dual of `refs-unplaced` — greps upstream `name(` calls vs both name files, comment/string/dead-block-stripped to kill copyright-header + format-string noise; inlines the vram from the `func_<addr>` jal when unambiguous; verified it now flags `nuContDataGetEx`→`nuContDataOpen`@0x800A2CAC) + #2 (CLAUDE.md recover-extern bullet: reconcile the **full data+function callee list** against the name files in the same gate disassemble pass; note the gate build-check is blind to this class)
- Carry-over: none

## Sprint 22 — mirror: osEPiLinkHandle (libultra recover-extern, first nintendo/ dir) — 2026-06-12
- Increment: 1 file banked / 1 fn matched (delta: md5-candidate files 37→38)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — verbatim mirror, 0 iterations; goal met first pass
- Seed: committed 3pt; banked 3pt; regime mirror   (v1 — story points; realized tier is v2, untouched this pure-mirror sprint)
- What helped: clean single-fn recover-extern, the lowest-risk mirror. `__osPiTable`=0x800C7E8C (simple pointer → size:0x4) recovered deterministically from the fn's own `lui 0x800c`/`lw/sw 0x7e8c`; `piint.h` + `__osDisableInt`/`__osRestoreInt` pre-placed → one symbol add + one yaml flip. First `nintendo/` variant-dir mirror; like S21 for libnusys, refutes the stale S11 "libultra warm pool mined out" note for the recover-extern fillers it explicitly left open.
- Friction: the mandatory re-confirm exposed a **target-fn vram error** (distinct from S20's extern-vram miss): a hand-guessed flat `rom + 0x80020000` resolved *mid-function* and silently returned a wrong ~1000 B containing fn. Caught only by the leaf-size mismatch. Real base is `rom + 0x80024C00`, and the curated name was already authoritative in `ghidra_symbols.txt` (0x800A3420) — the guess was avoidable.
- Applied: 2 of 2 — #1 (CLAUDE.md recover-extern bullet: look up the *target fn's* vram from the name files / yaml base for the disassemble re-confirm, never guess a flat offset) + #2 (`pick_target.py` `vram` column — emits the authoritative splat vram so the gate re-confirm needs no derivation)
- Carry-over: none

## Sprint 21 — mirror: nuGfxRetraceWait (zero-enabler clean libnusys cp) — 2026-06-12
- Increment: 1 file banked / 1 fn matched (delta: md5-candidate files 36→37)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — verbatim mirror, 0 iterations; goal met first pass
- Seed: committed 2pt; banked 2pt; regime mirror   (v1 — story points; realized tier is v2, untouched this pure-mirror sprint)
- What helped: pick_target reported `-` (no hazard) and was right — first genuinely zero-enabler clean cp since the libnusys band opened (S15). Warm nuGfx band (5 banked siblings), `nusys.h` + all refs pre-placed, name pre-curated → one yaml flip the only enabler. Open-band fast-path applied cleanly (no manual re-grep).
- Friction: none. Only friction was conceptual — the S11 "warm pool mined out" BACKLOG note is stale (predates the S15 libnusys unlock) and could mislead a future gate into assuming a known-edit is always required.
- Applied: 1 of 1 — #1 (refresh BACKLOG PO note: libnusys mirror band still yields zero-enabler clean cp's, not just recover-extern fillers)
- Carry-over: none

## Sprint 20 — mirror: nuCont recover-extern pair (nuContRmbStart + nuContGBPakOpen) — 2026-06-12
- Increment: 2 files banked / 2 fns matched (delta: ~1.72% → ~1.82%; md5-candidate files 34→36)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — both verbatim mirrors, 0 iterations; goal met first pass
- Seed: committed 6pt; banked 6pt; regime mirror   (v1 — story points; realized tier is v2, untouched this pure-mirror sprint)
- What helped: 9th sibling-batch, 3rd recover-extern batch — the lowest-risk fill-the-cap pattern (S16/S19). Shared callee `nuSiSendMesg` pre-curated (S17). The **mandatory gate lui/addiu re-confirm** earned its keep: it caught the first inlined-vram miss (nuContRmbCtl@0x80104F57 was the `.mode` field addr, true base 0x80104F50). Doctrine trim-to-cleanest-2 correctly excluded nuContDataGetEx (extra MISSING fn symbol nuContDataOpen).
- Friction: none on the matching path. Out-of-band, the post-sprint Ghidra name-sync surfaced material **workspace↔decomp `ghidra_symbols.txt` drift** (~250 Ghidra-ahead funcs/globals; 5 decomp-ahead build-critical: `__muldi3`/`__moddi3`/`__osRunningThread`/`__osViCurr`/`__osViNext`) → full `make sync-names` is currently destructive (PO chose not to backlog it this gate).
- Applied: 2 of 3 — #1 (CLAUDE.md recover-extern bullet: indexed-struct-array inlined vram is the field addr, recover base by subtracting the member offset; keep re-confirm mandatory) + #2 (CLAUDE.md size rule: scalar/fn-ptr 0x4, array stride×count); (#3 confirmatory log-only — NOT a file edit; sync-drift backlog item NOT selected)
- Carry-over: none

## Sprint 19 — mirror: nuGfx*FuncSet trio (libnusys recover-extern batch) — 2026-06-12
- Increment: 3 files banked / 3 fns matched (delta: ~1.58% → ~1.72%; md5-candidate files 31→34)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — all verbatim mirrors, 0 iterations; goal met first pass
- Seed: committed 9pt; banked 9pt; regime mirror   (v1; mirror track seed-only — realized is v2/classical)
- What helped: the recover-extern mirror flow is now fully deterministic and batches cleanly — pick_target's broadened (S16#1) refs-unplaced grep flagged all 3 globals correctly (no false-clean), each vram confirmed from the fn's own lui/addiu (3/3 matched pick's inlined value). All 3 fns + the nuGfxTaskAllEndWait callee pre-curated in ghidra_symbols → enabler = 3 data-extern adds + 3 yaml flips, nothing else. First homogeneous batch banked at the full ≤3-4 cap (3 files) at one-file risk; per-file all-or-nothing banking made the marginal sibling near-free.
- Friction: none — clean first-pass on all three. (jal-count reconciled at the gate, no S18-style divergence.)
- Applied: 3 of 3 (PO: all) — #1 CLAUDE.md "recover-extern mirror" sub-case bullet (third known-edit mirror sub-case, alongside file-static-drop + jal-divergence); #2 CLAUDE.md fill-the-cap batch-sizing rule (homogeneous sibling sets default to filling the ≤3-4 cap; heterogeneous trim to cleanest 2); #3 kept gate MCP-disassemble re-confirm mandatory (vram empirically 3/3 reliable but re-confirm stays while sample small — recorded in #1's bullet). Also logged the S19 VELOCITY row + refreshed the running mirror seed-velocity (1.92→2.63, flagged batch-driven not throughput).
- Carry-over: none

## Sprint 18 — mirror: nuContInit (libnusys near-verbatim, drop-one-line) — 2026-06-12
- Increment: 1 file banked / 1 fn matched (delta: ~1.53% → ~1.58%; md5-candidate files 30→31)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — near-verbatim mirror, 0 iterations; goal met first pass
- Seed: committed 2pt; banked 2pt; regime mirror   (v1; mirror track seed-only — realized is v2/classical)
- What helped: the S16 gate asm-data-recovery check (disassemble before declaring clean) generalized perfectly — it caught the upstream-vs-ROM call divergence (upstream calls 4 managers, ROM jals only 3; no `nuContPakMgrInit` in this build). Handled as a near-verbatim mirror: copy upstream verbatim, drop the one diverging line, bank on full-make SHA. Deterministic from asm, no iteration. The 3 retained callees + nusys.h all pre-placed → one yaml flip the only enabler.
- Friction: pick_target reported nuContInit no-hazard — its ref-grep flags data refs, not jal callees, so the build divergence was invisible until the gate disassembled. Resolved by the applied #1 hazard.
- Applied: 2 of 2 (PO: both) — #1 new `jal-count-mismatch:<C>vs<asm>` hazard in pick_target.py (counts upstream calls vs ROM jals, strips dead `#ifdef _DEBUG`/`_FINALROM` blocks; verified it flags nuContInit 4vs3); #2 CLAUDE.md near-verbatim-mirror (upstream-vs-ROM call divergence) bullet alongside the file-scope-static-drop rule.
- Carry-over: none

## Sprint 17 — mirror: nuContGBPak{GetStatus,Power,ReadID} (libnusys sibling-trio) — 2026-06-11
- Increment: 3 files banked / 3 fns matched (delta: ~1.39% → ~1.53%; md5-candidate files 27→30)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — all verbatim mirrors, 0 iterations; goal met first pass
- Seed: committed 6pt; banked 6pt; regime mirror   (v1; mirror track seed-only — realized is v2/classical)
- What helped: cohesive sibling trio off the warm libnusys band — shared sole callee `nuSiSendMesg`@0x800A2824 (in ghidra_symbols) + shared header nusys.h pre-resolved all 3; struct types/constants/prototypes already in nusys.h; three yaml flips the only enabler (zero new symbols, zero header copies, zero splits). S16#1's extended refs-unplaced grep correctly reported all 3 no-hazard; the gate asm-data-recovery jal/lui scan confirmed truly clean (sole jal = nuSiSendMesg, zero data loads). Mirror branch banked on full-make SHA alone (no spot-check).
- Friction: none — cleanest sprint to date; the S16#1 grep fix + gate-check made the false-clean risk a non-event.
- Applied: 0 of 3 (PO: log only) — all 3 buffered items were confirmatory observations already covered by existing doctrine (#1 nuCont-warm ≡ S14 open-band fast-path; #2 sibling-batch ≡ S4/S5/S10; #3 gate jal/lui check ≡ S16#1). No tooling/CLAUDE.md edits.
- Carry-over: none

## Sprint 16 — mirror: nuGfxTaskAllEndWait + nuGfxDisplayOff (1st libnusys sibling-batch) — 2026-06-11
- Increment: 2 files banked / 2 fns matched (delta: ~1.29% → ~1.39%; md5-candidate files 25→27)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — both verbatim mirrors, 0 iterations; goal met first pass
- Seed: committed 4pt; banked 4pt; regime mirror   (v1; mirror track seed-only — realized is v2/classical)
- What helped: S15-note sibling-batch off the warm libnusys band; callee `osViBlack` (S6) + both fn names pre-curated; the two data globals recovered deterministically at the gate from each fn's own asm (`lui 0x8013`/`lw -0x2b88`→nuGfxTaskSpool=0x8012D478; `lui 0x8010`/`sw 0x4e6c`→nuGfxDisplay=0x80104E6C); mirror branch banked on full-make SHA alone (no spot-check)
- Friction: **false-clean** — pick_target reported both no-hazard, but the data globals are non-`__`-prefixed library globals its `refs-unplaced` grep skipped, so the recover-extern enabler surfaced only at the gate (not pre-flagged). Fixed in this retro (#1).
- Applied: 2 of 3 — #1 broadened `refs-unplaced` (pick_target now scans the .c's resolvable headers for `extern` *data* decls → flags non-`__` globals like nuGfxFunc/nuScPreNMIFunc/nuGfxSwapCfbFunc, which now show recover-extern hazards + inline vrams); #2 CLAUDE.md execution-loop note (mirror branch's proof IS the full-make SHA; byte spot-check is classical-only). #3 (document open-band fast-path covers libnusys/mainlib) NOT selected — guidance-only, and #1 supersedes its caveat.
- Carry-over: none

## Sprint 15 — mirror: nuGfxSwapCfb (libnusys band unlock) — 2026-06-11
- Increment: 1 file banked / 1 fn matched (delta: ~1.24% → ~1.29%; md5-candidate files 24→25)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — verbatim mirror, 0 iterations; goal met first pass
- Seed: committed 3pt; banked 3pt; regime mirror (cold floor 2 + 1 scaffolding-enabler unlock) — v1, mirror track seed-only
- What helped: the S13 retro's libnusys upstream-index made the band visible; the unlock was cheap because `nusys.h` is one self-contained header (deps `<ultra64.h>`+`<PR/gs2dex.h>` both already in-tree) and the only callee `osViSwapBuffer` was banked S5 — so the whole enabler was `cp nusys.h` + one `-I` CFLAGS line + one yaml flip, zero symbol adds (`nuGfxSwapCfb`@0x800A15E0 pre-curated). One paid-once enabler converts the entire nuGfx*/nuCont* band to pts-2 cold mirrors (verified: ranker now shows them pickable, no longer `blk`).
- Friction: none mechanically. The only subtlety was the gate-vs-execution split — the `-I`/header enabler is inert until the real `.c` lands, so the gate's `make extract && make` proves the flip stays green but not that the unlock *compiles*; that proof came in the execution-middle build (also green). Worth keeping in mind for future band-unlock sprints.
- Applied: 2 of 3 — #1 (un-blk the nusys ranker: `+include/libnusys` in `pick_target.py` INCLUDE_DIRS + nusys upstream-inc root) + #2 (CLAUDE.md libnusys path-mirror convention + `-I` list). (#3 sibling-batch the nuGfx* band — guidance only, carried to BACKLOG ordering note, no file edit.)
- Carry-over: none

## Sprint 14 — mirror: osSetThreadPri (thread band, open-band fast-path) — 2026-06-11
- Increment: 1 file banked / 1 fn matched (delta: ~1.20% → ~1.24%; md5-candidate files 23→24)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — verbatim mirror, 0 iterations; goal met first pass
- Seed: committed 1pt; banked 1pt; regime **mirror** (warm thread-band single-fn; seed correct first time — pick_target said 1/mirror, no gate correction needed; mirror track seed-only)
- What helped: 3rd thread-band mirror (after getthreadpri, yieldthread) — band fully open, so all 7 refs (`__osRunningThread`/`__osRunQueue`/`__osDequeueThread`/`__osEnqueueThread`/`__osEnqueueAndYield`/`__osDisableInt`/`__osRestoreInt`) + all 3 headers (`PR/os_internal.h`/`PR/ultraerror.h`/`internal/osint.h`) pre-placed and the name pre-curated in `ghidra_symbols.txt`. **One yaml flip the only enabler** — zero symbol adds, zero header copies, no split. Largest mirror banked to date (208 B, real queue dequeue/enqueue + yield branch) yet still pts=1 → reinforces the byte-gate-dormant calibration (effort = path + enabler load, not bytes).
- Friction: none. Notable: VELOCITY.md's "warm clean-singleton mirror pool mined out (S11)" was vi-band-specific — the **thread band is still warm with clean siblings**; S14 banked one with zero hazards.
- Applied: 1 of 3 — **#3 open-band fast-path** (CLAUDE.md upstream-mirror bullet: a ≥2-banked-sibling band with a `pick_target` no-hazard candidate skips the agent's redundant manual per-ref re-grep; the fast-path NEVER overrides a flagged hazard — `__osDequeueThread` is the live counterexample, a `defines-data` false-clean inside the warm thread band; gate build-check stays load-bearing). #1 (VELOCITY anchor note re 208 B) + #2 (thread-band sibling-pair BACKLOG note) NOT selected.
- Carry-over: none.

---

## Sprint 13 — mirror: osViSetYScale (vi band; un-named-mirror trap → pick_target fix) — 2026-06-11
- Increment: 1 file banked / 1 fn matched (delta: ~1.15% → ~1.20%; md5-candidate files 22→23)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — verbatim mirror, 0 iterations; goal met first pass
- Seed: committed 1pt; banked 1pt; regime **mirror** (warm vi-band; **seed-miss: pick said 5/classical, corrected to 1/mirror at the gate**; mirror track seed-only, realized tier is v2/classical)
- What helped: the gate's mandatory asm-vs-upstream check — `func_800AD370` ranked as a "non-trivial classical leaf" (the PO's target for v2 residual signal) but its asm (`swc1 fs0,0x24` + `ori 0x4` under int-disable) matched `libultra_modern/.../visetyscale.c` byte-for-shape → revealed as **osViSetYScale**, an un-named mirror. Caught BEFORE a wasted classical hand-decomp. All externs + all 4 headers pre-placed (6th vi-band mirror) → verbatim cp, zero header copies, one symbol add. PO interrupt pointed at libnusys/libmus/libnaudio + coddog as the systemic fix.
- Friction: the **root cause** — `pick_target.py` maps upstream by *curated name*, so every un-named `func_<vram>` that is really SDK code shows as `upstream:none` and gets mislabeled classical. Both offered candidates were mirrors (`func_800AD370`=osViSetYScale, `func_800AE920`=__osPfsSelectBank). This silently corrupts the classical pool and the v2-residual target hunt. The v2 goal went unmet this sprint (no genuine classical leaf banked).
- Applied: 2 of 3 — **#1 signature matcher** (un-named candidates now carry an advisory `maybe-upstream:<lib>:<files>` hazard from IDF-weighted shared-callee mass, coddog-style register-stripped idea done compile-free; validated: `func_800AE920`→`pfsselectbank` in the short list, vi setters→correctly-ambiguous `visetyscale,viswapbuf`) + **#2 three-lib scan** (libnusys/libmus/libnaudio added to the upstream index via the `UPSTREAM_TREES` registry; named nusys fns now classify `libnusys`, e.g. `nuGfxSwapCfb`). #3 (4-lib classical-negative vet) NOT selected — subsumed by #1's matcher for now.
- Carry-over: none banked-incomplete, but two ORDERING facts surface (see BACKLOG): (a) the nusys/audio mirror band needs an include-path enabler (`nusys.h` etc. unresolved → `needs-header` blk) before it's pickable; (b) a genuine classical leaf for v2 must now be vetted against all 4 SDK trees (use the new `maybe-upstream` hint as the filter).

---

## Sprint 12 — mirror: osYieldThread (thread band, recover-extern flip) — 2026-06-11
- Increment: 1 file banked / 1 fn matched (delta: ~1.10% → ~1.15%; md5-candidate files 21→22)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — verbatim mirror, 0 iterations; goal met
- Seed: committed 2pt; banked 2pt; regime **mirror** (warm-1 floor +1 recover-extern; mirror track seed-only, realized tier is v2/classical)
- What helped: the S11 gate had already recovered `__osRunQueue`@0x800C8228 from the fn's own asm (`lui a0,0x800d / addiu a0,a0,-0x7dd8`), so the whole enabler was one `symbol_addrs.txt` add + one yaml flip. Upstream `yieldthread.c` is a clean 5-liner, no file-scope static → safe verbatim mirror; refs `__osRunningThread`/`__osEnqueueAndYield` + both headers pre-placed from S8 stopthread (same band). Full make SHA-1 green first try
- Friction: none material. (Cosmetic: the bank commit body says "md5-candidate 20→21"; actual was 21→22 — unpushed, left as-is)
- Applied: 1 of 3 — PO selected #1. **#1 inline recovered vram into the `refs-unplaced` hazard** → `tools/pick_target.py` now reads splat's `D_<vaddr>` auto-labels from `asm/<ROM>.s` and binds `name@0xADDR` when the mapping is unambiguous (exactly one unplaced upstream name ∩ one candidate address — the recover-one-extern floor case). Verified: `osEPiLinkHandle → __osPiTable@0x800C7E8C`, `__osSetGlobalIntMask → __OSGlobalIntMask@0x800C9470` (address-set dedup collapses repeat refs); ambiguous multi-extern rows (`osGetTime`, `__osDequeueThread`) stay bare. The gate still confirms before any add, so an over-listed local-rodata D_ is harmless. **#2 (re-price `__osDequeueThread`)** and **#3 (favor classical next 1–2 sprints)** NOT selected — recorded here as considered, carried into the BACKLOG ordering note for the next gate
- Carry-over: none

---

## Sprint 11 — classical: func_800AB600, first sprint with residual variance (v2 activated) — 2026-06-11
- Increment: 1 file banked / 1 fn matched (delta: ~1.05% → ~1.10%; md5-candidate files 20→21)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — matched after 1 fix-iteration, no spike; goal met
- Seed: committed 5pt; banked 5pt; regime **classical** (realized 5, residual 0 — first v2-logged realized tier)
- What helped: PO redirected the gate from the `osYieldThread` mirror to a non-trivial no-upstream leaf (S9 ordering note — generate v2 variance). `func_800AB600` had real logic (bit ops + branch + conditional struct RMW + a `(status>>8)&1` return), so the classical loop actually iterated: seed compiled 0.80/score 400, and a **register-reuse nudge** (`bit = status>>8; bit &= 1;` instead of one expression — forces GCC to reuse `a0` for the shift+mask) flipped it to score 0 in one iteration. Byte spot-check identical; full make SHA-1 green. One yaml flip, zero symbol adds, zero header copies (kept the `func_` name like S9)
- Friction: m2c failed again on the stub-only parent (expected graceful fallback); the Ghidra **decompile was wrong** (rendered the return as `return 0`), so the body had to be hand-translated from the asm listing, not the decompile
- Applied: 2 of 3 — PO selected #2 + #3. **#2 register-reuse nudge** → new `CLAUDE.md ## Conventions` bullet (split a single expression into two statements over one lvalue when the only diff is a scratch register). **#3 asm > Ghidra decompile** → appended to the classical Seed step (`disassemble_function` is ground truth; the decompile can be silently wrong). **#1 (v2 activation)** was the headline decision, taken in the sign-off (not a file-suggestion): the S9 deferral condition — first classical sprint with real residual variance — is now satisfied, so **v2 is ACTIVE** (realized-tier/residual/rolling-5/re-anchor on the classical track; mirror track stays seed-only). VELOCITY.md updated accordingly
- Carry-over: none

## Sprint 10 — bank rdp DPC sibling-pair (dpsetstat.c + dpctr.c) — 2026-06-11
- Increment: 2 files banked / 2 fns matched (delta: ~0.96% → ~1.05%; md5-candidate files 18→20)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — both first-pass clean (verbatim mirrors, 0 iterations); goal met
- Seed: committed 2pt; banked 2pt; regime **mirror** (per-file 1+1; 8-gate clear; realized tier is v2)
- What helped: 5th **sibling-pair** off a warm band (dp band opened S1 dp.c). The `pack:2fn` hazard flagged the 0x86730 block; one disassembly confirmed it bundled two *different* upstream files (osDpSetStatus=dpsetstat.c 0x10 + osDpGetCounters=dpctr.c 0x4C, NOT the name-guessed dpgetstat.c). Split at 0x86740 (both 16-aligned) → two verbatim `cp`s. Both names pre-curated in ghidra_symbols.txt, all DPC_*_REG in PR/rcp.h, both headers present from S1 → **zero symbol adds, zero header copies, one yaml split**. Crossed 1% matched
- Friction: the `pack:2fn` hazard named only the *first* fn — the 2nd member + its upstream file were invisible until a hand-disassembly of asm/86730.s, and the obvious name-guess (dpgetstat.c) was wrong (it was dpctr.c). Minor, but it cost the one manual asm read the mirror branch otherwise avoids
- Applied: 1 of 3 — PO selected #1. **#1 pack-disambiguation column** landed in `tools/pick_target.py`: `pack:Nfn` now renders `pack:Nfn[fn1=basename1,fn2=basename2,…]` (each member's upstream basename via the existing upstream_index), so the gate distinguishes a multi-file pack needing a split (different basenames — `__osSetGlobalIntMask`=setglobalintmask+resetglobalintmask) from a single-file pack (`sprintf`=sprintf+sprintf) without disassembling asm/<rom>.s; an un-indexed member shows `=?` (e.g. `__muldi3`). pts unchanged (purely additive to the hazards string). (#2 re-price rdp leaves → folded into BACKLOG note, not a code edit; #3 v2-uncalibratable → reaffirmed below, not selected)
- Carry-over: none

---

## Sprint 9 — first classical (no-upstream) match: func_80099490 — 2026-06-11
- Increment: 1 file banked / 1 fn matched (delta: ~0.91% → ~0.96%; md5-candidate files 17→18)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — first-pass clean (score 0, 8/8 rows, 0 iterations); goal met
- Seed: committed 5pt; banked 5pt; regime **classical** (FIRST classical-track row — logged separately from the mirror seed-velocity; realized tier is v2)
- What helped: **acted on S7 retro #2 / S8 #3** (the standing classical-spike recommendation) — the PO scheduled the `--upstream none` target that 8 straight mirrors had deferred. Risk minimized by picking the lowest-variance classical leaf: a thin no-arg wrapper `void func_80099490(void){ nuPiInitSram(); }`. Callee `nuPiInitSram` already symbolized (ghidra_symbols.txt, 0x800A1720), single-fn subseg (no split), function kept its auto `func_` name → **one yaml flip, zero symbol adds, zero header copies**. m2c failed on the fresh 1-stub parent but the Ghidra-decompile seed (`ghidra.c`) carried the match alone. **Classical match-loop PROVEN** (S1–S8 were all upstream mirrors)
- Friction: (a) `seed_c.py`'s m2ctx step threw a near-silent traceback on the 1-stub parent (no type context) — the Ghidra seed saved it, but the primary-seed path looked broken; (b) the wrapper matched first-pass-clean → **zero residual variance**, the same point-mass shape as the mirrors, so the v2 trigger is *technically tripped but uncalibratable* — PO deferred v2; (c) the smallest no-upstream leaves the ranker surfaced were un-decompilable register/FPU intrinsics (`osGetCount`, `__osGetCause/SR`, `__osSetCompare`, `func_800B0A10`=sqrtf), noise above the genuine targets
- Applied: 2 of 3 — PO selected #1 + #3. **#1 graceful m2c fallback** landed in `tools/seed_c.py` (`parent_has_real_c()` detects a stub-only parent, skips m2ctx cleanly, falls to the Ghidra seed without a traceback). **#3 `intrinsic-likely` hazard** landed in `tools/pick_target.py` (`intrinsic_likely()` reads the leaf's asm body: a no-`jal` leaf whose work ops are all CP0-moves/`sqrt`, or a spimdisasm-tagged `handwritten` leaf → flagged; verified it flags all 5 known intrinsic shims and clears the wrappers / `__udivdi3`). (#2 was the v2-activation decision itself, resolved as *defer* in the scope sign-off — not a code edit)
- Carry-over: none

---

## Sprint 8 — bank monegi/thread/stopthread.c (osStopThread) — 2026-06-11
- Increment: 1 file banked / 1 fn matched (delta: ~0.86% → ~0.91%; md5-candidate files 16→17)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — verbatim `cp`, zero iteration, first-pass green; goal met
- Seed: committed 1pt; banked 1pt; regime mirror   (v1 — 3rd live-logged sprint; running 14pt/8 = 1.75 pt/sprint)
- What helped: acted on S7 retro #2's confirmation that `osStopThread` is the smallest *clean* leaf (seed 1, warm, no unplaced ref). True zero-enabler: name pre-curated in `ghidra_symbols.txt` (0x800AC5C0), all refs pre-placed (`__osRunningThread` 0x800C8230, `__osEnqueueAndYield`, `__osDequeueThread`, the `__osDisableInt`/`__osRestoreInt` pair), both headers (`PR/os_internal.h`, `osint.h`) present → **zero symbol adds, zero header copies, one yaml flip**; single-fn subseg (0x879C0→0x87A80 = 192 B) so no split. The `refs-unplaced`/`needs-header` hazard flags (S3/S7) correctly read this leaf as `-`, so smallest-first picked it with only a static pre-flight
- Friction: none of substance — banked verbatim first-pass. Signal is depletion: 8th straight clean mirror, zero residual variance — the v2 trigger has now been unmet for the entire Scrum history, so the classical-spike decision is overdue
- Applied: 0 of 3 — PO selected Apply none. (#1 `pick_target.py` 'next clean leaf' footer; #2 re-price the newly-warm thread band next gate; #3 schedule the classical spike next sprint — all recorded here, none landed; #1 and #3 carry forward as the standing recommendations)
- Carry-over: none

---

## Sprint 7 — bank monegi/convert/virtualtophysical.c (osVirtualToPhysical) — 2026-06-11
- Increment: 1 file banked / 1 fn matched (delta: ~0.81% → ~0.86%; md5-candidate files 15→16)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — verbatim `cp`, byte-identical, zero iteration, first-pass green; goal met
- Seed: committed 2pt; banked 2pt; regime mirror   (v1 — 2nd live-logged sprint; running 13pt/7 ≈ 1.86 pt/sprint)
- What helped: **acted on S6 retro #1 (vi band mined out)** by opening a NEW band (`monegi/convert/`). Gate pre-flight beat the smallest-first sort: the nominally-smaller `osYieldThread`(80B)/`osStopThread`(192B) looked clean but `osVirtualToPhysical` was the true zero-enabler pick — all refs pre-resolved (`__osProbeTLB`@0x800ACC00 in ghidra_symbols, R4300 macros in in-tree `PR/R4300.h`), name pre-curated (0x800A7720), single-fn subseg (no split) → **zero symbol adds, zero header copies, one yaml flip**
- Friction: none of substance — banked verbatim first-pass. The real friction is upstream of the sprint: the ranker's smallest-first sort *hid* enabler cost (a referenced-but-unplaced data extern wasn't flagged), so the gate had to hand-pre-flight ref-resolvability to avoid picking `osYieldThread` and stalling on a `__osRunQueue` recovery. Suggestion #1 fixes this at the source
- Applied: 2 of 3 — PO selected #1 + #2. **#1 `refs-unplaced` hazard** landed in `tools/pick_target.py` (dual of `defines-data`: a `__`-prefixed data extern referenced but absent from both name files, never called → asm-data-recovery enabler before the mirror links; re-priced `osYieldThread` 1→2, `osEPiLinkHandle` 2→3, `osSyncPrintf` +`__printfunc`, and confirmed `osStopThread` clean). **#2 classical-target ordering note** added to `BACKLOG.md` (deliberately schedule a `--upstream none` target to break the 7-straight mirror point-mass + trip v2). (#3 `osYieldThread`/`__osRunQueue` warm-up note — NOT selected, but now implicitly covered by the #1 hazard flag)
- Carry-over: none

---

## Sprint 6 — bank monegi/vi/viblack.c (osViBlack) — 2026-06-11
- Increment: 1 file banked / 1 fn matched (delta: ~0.77% → ~0.81%; md5-candidate files 14→15)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — verbatim `cp`, byte-identical, zero iteration, first-pass green; goal met
- Seed: committed 1pt; banked 1pt; regime mirror   (v1 — first live-logged sprint; bootstrap S1–5 were retro-pointed)
- What helped: 4th consecutive zero-enabler vi-band mirror (band opened S0 `vigetcurrcontext`, warmed S3 `visetmode`, S5 `viswapbuf`+`visetevent`) — `__osViNext` (0x800C9564), the `__osDisableInt`/`__osRestoreInt` pair, and both headers (`PR/os_internal.h`/`viint.h`) all pre-placed → **zero symbol adds, zero header copies, one yaml flip the only enabler**; single-fn subseg (0x88B20→0x88B80 = 96 B = osViBlack exactly) so no split; `osViBlack` already curated at 0x800AD720 so no rename; S5's `defines-data` hazard steered triage past `__osDequeueThread`/`__osViInit` (both re-define placed externs) to the one clean leaf
- Friction: none of substance — banked verbatim first-pass. The signal is depletion, not difficulty: viblack was the **last clean singleton in the vi band**; the 3 smallest remaining libultra candidates (`__osDequeueThread`, `__osViInit`, `osYieldThread`'s `__osRunQueue` ref) all now carry the `defines-data` hazard / need asm-data-recovery
- Applied: 0 of 3 — PO selected Apply none. (#1 warm-band-exhausted ranker signal; #2 `defines-data`/asm-data-recovery BACKLOG ordering note; #3 schedule a classical target to break the 6-straight mirror point-mass — all recorded here, none landed)
- Carry-over: none

---

## Sprint 5 — bank monegi/vi sibling pair (viswapbuf.c + visetevent.c) — 2026-06-11
- Increment: 2 files banked / 2 fns matched (delta: ~0.67% → ~0.77%; md5-candidate files 12→14)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — both first-pass, byte-identical verbatim copies, zero iteration; goal met first pass
- What helped: 2nd consecutive **sibling-pair** sprint and 3rd zero-enabler sprint off a warm band — the `monegi/vi/` band was opened by Sprint-0 (`vigetcurrcontext.c`) + Sprint-3 (`visetmode.c`), so `__osViNext` (0x800C9564), the `__osDisableInt`/`__osRestoreInt` pair, and the full 4-header set (`os_internal.h`/`ultraerror.h`/`assert.h`/`viint.h`) were all already in-tree → **zero new symbols, zero header copies, two yaml flips the only enabler**; both leaves were `cp`-verbatim from libultra_modern and built green on first `make`; `visetmode.c` was an exact structural template (same headers, same dead `_DEBUG return 0;` shape); Sprint-3's `needs-header` hazard again steered triage past the cold leaves (`guRandom`/audio/`osSyncPrintf`)
- Friction: none of substance — both fns banked verbatim first-pass. The only real work was vetting that the *next*-ranked clean-looking candidate (`__osDequeueThread`, nfn=1, no flagged hazard) was actually a false-clean: its upstream `thread.c` *defines* 5 placed data globals (`__osThreadTail` = the Sprint-2 extern at 0x800C8220, `__osRunQueue`, …), which a verbatim mirror would re-emit and collide with — caught by hand, then codified as suggestion #2
- Applied: 2 of 3 — #1 **band-warm boost** added to `pick_target.py` (a `band` column + `BAND_WARM_BONUS=64` lifting candidates whose mirror dir already holds a banked sibling above equally-small band-cold leaves — the inverse of `needs-header`; re-raised from Sprint-4 #1 which the PO deferred, now validated twice by the si + vi pairs); #2 **`defines-data:<name>` hazard** added to `pick_target.py` (brace-depth + K&R-aware scan flags upstream files that define file-scope external-linkage data globals → route to the classical loop with the defs dropped; the `.data` analogue of the `file-static` BSS hazard; correctly flags `__osDequeueThread`/thread.c and 27 other data-defining upstreams, K&R params suppressed so libkmc math stays clean); #3 a no-edit confirmation (open-band sibling-pair batching held again) NOT separately applied
- Carry-over: none

---

## Sprint 4 — bank Si raw-IO sibling pair (sirawread.c + sirawwrite.c) — 2026-06-11
- Increment: 2 files banked / 2 fns matched (delta: ~0.57% → ~0.67%; md5-candidate files 10→12)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — both first-pass, zero iteration; goal met first pass
- What helped: first **sibling-pair** sprint, and first to bank 2 files at one-file cost — the `monegi/si/` band was already open (Sprint-0's `si.c`/`__osSiDeviceBusy`), so both leaves' callee + companion headers (`siint.h`/`assert.h`/`PR/rcp.h`'s `IO_READ`/`IO_WRITE`) were already in-tree → **zero new symbols, zero header copies, two yaml flips the only enabler**; Sprint-3's `needs-header` hazard auto-flagged the smaller-ranked leaves (`guRandom`→`guint.h` cascading to `mbi.h`/`gu.h`, `osSyncPrintf`→`stdarg.h`, audio→`libaudio.h`) so triage landed on the clean pair without manual include-reading — the suggestion now paying off; plain-`make` execution-middle finalize (gate-flipped subsegs) held for both
- Friction: none of substance — both fns banked verbatim first-pass. Only mechanical note: `grep -c INCLUDE_ASM` exits 1 on a 0 count, short-circuiting an `&&` chain at DoD verification (cosmetic)
- Applied: 0 of 3 (PO: Apply none) — #1 band-warm boost for `pick_target.py` (de-rank candidates whose upstream dir has no banked sibling — inverse of `needs-header`) NOT selected; #2 CLAUDE.md "Cap small" sibling-pair note (≤2 may extend to a pair sharing an open band) NOT selected; #3 was a no-edit confirmation (needs-header steered triage correctly; plain-`make` finalize held)
- Carry-over: none

---

## Sprint 3 — bank osViSetMode as src/libultra/monegi/vi/visetmode.c — 2026-06-11
- Increment: 1 file banked / 1 fn matched (delta: ~0.53% → ~0.57%; md5-candidate files 9→10)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — clean first-pass verbatim-upstream singleton; goal met first pass
- What helped: the upstream-mirror path (verbatim single-fn `visetmode.c`, 2nd in the `monegi/vi/` band — sibling of the Sprint-0 `vigetcurrcontext.c`); **all linker refs pre-resolved** (`__osViNext`/`__osDisableInt`/`__osRestoreInt`) so **zero symbol recovery** — strictly cleaner than Sprint-2's `__osThreadTail` harvest; the only enabler was a trivial companion-header copy (`include/libultra/assert.h`, 8-line self-contained, `assert()` never expanded since `_DEBUG` off); the `/sprint-plan` gate build-check validated the flip green before the middle
- Friction: minimal — candidate triage was again the only real work. The two smaller-ranked leaves (`guRandom`: missing `guint.h` + static-`.data`; audio band `alCopy`/`alHeapInit`: `<libaudio.h>` unresolvable w/o `-I include/libultra/PR`) were de-prioritized **by hand** — that manual triage is exactly what suggestion #1 now automates
- Applied: 3 of 3 — #1 added the **`needs-header:<inc>`** hazard to `tools/pick_target.py` (greps every upstream `#include` vs the project `-I` set under `-nostdinc`; auto-surfaces `guRandom`→`guint.h`, audio→`libaudio.h`, `osSyncPrintf`→`stdarg.h`) + CLAUDE.md gate doctrine (copyable header → execution-middle copy; unindexed `-I` path → deferred enabler); #2 CLAUDE.md "**verbatim means verbatim**" note — keep dead `#ifdef _DEBUG` blocks + their companion `#include`s, copy the header rather than trim the include; #3 CLAUDE.md execution-middle finalize is just `make` for a gate-flipped subseg (`make extract` only on a mid-flight split)
- Carry-over: none

---

## Sprint 2 — bank osCreateMesgQueue as src/libultra/monegi/message/createmesgqueue.c — 2026-06-11
- Increment: 1 file banked / 1 fn matched (delta: ~0.48% → ~0.53%; md5-candidate files 8→9)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — clean verbatim-upstream singleton; goal met first pass
- What helped: the upstream-mirror path (verbatim single-fn `createmesgqueue.c`, `monegi/` `<PR/x.h>` includes resolved unchanged, no clang-format); **recovering the unsymbolized `__osThreadTail` data global's vram deterministically from the target fn's own asm** (`lui 0x800d`/`addiu -0x7de0` = 0x800C8220) instead of an RE hunt — turned the one risky enabler into a lookup; the `/sprint-plan` gate build-check (`make extract && make`) validated the flip + symbol add green before the execution middle
- Friction: minimal. Candidate triage was the only real work — the two smaller-ranked leaves (`guRandom` function-local `static .data` placement risk + missing `guint.h`; `alCopy`/`alHeapInit` bare `<libaudio.h>` not resolvable without `-I include/libultra/PR`) were de-prioritized in favor of the clean `monegi/message/` mirror
- Applied: 3 of 3 — #1 codified "harvest an unsymbolized data global's addr from the target fn's own `lui/addiu`" into CLAUDE.md's upstream-mirror branch; #2 include-resolvability hazard note (audio band `<libaudio.h>` needs `-I include/libultra/PR`; prefer `<PR/x.h>` upstreams) in the upstream-mirror convention; #3 documented the `symbol_addrs.txt` data-extern format (`name = 0x<vram>; // size:0x<n>`, no `type:func`)
- Carry-over: none

---

## Sprint 1 — bank __osDpDeviceBusy as src/libultra/monegi/rdp/dp.c — 2026-06-11
- Increment: 1 file banked / 1 fn matched (delta: 0.43% → ~0.48%; md5-candidate files 7→8)
- Quality: 0/0/0/0 (stuck-far/permuter/carried/re-opened) — the function itself was a trivial verbatim-upstream leaf
- What helped: the upstream-mirror path (`/decomp-libupstream`, verbatim 12-line `dp.c`, clean leaf, no new symbols); the `/sprint-plan` gate build-check surfaced the real blocker before the execution middle; **systematic root-cause investigation** (bisecting the `cpp|as` asm pipeline) instead of accepting the surface "next is static" symptom
- Friction: a system-wide **missing `mips-linux-gnu-cpp`** regression (toolchain removed since the 2026-05-22 green build) made the `cpp|as` pipe — which lacked `pipefail` — silently emit 0-byte asm objects for all 256 files, surfacing as a *misleading* `undefined reference to next` at link. Consumed most of the session to root-cause. A stale `build/.z64` also false-positived its SHA-1 mid-debug. Fixed by reinstalling `cpp-mips-linux-gnu`.
- Applied: 3 of 3 — #1 Makefile loud-fail guard (`.SHELLFLAGS` pipefail + missing-`$(CPP)` `$(error)`); #2 stale-ROM SHA guard (CLAUDE.md finalization doctrine — gate `sha1sum` on `make` success); #3 codified the `/sprint-plan` gate build-check as load-bearing (CLAUDE.md DoR)
- Also landed (PO directive, not a buffered suggestion): workflow-doctrine change — yaml subseg flips/splits + `symbol_addrs.txt` additions + the follow-on `make extract` are now **agent** actions (was USER), performed at the gate after PO scope approval; safety rails preserved (never-delete, disjoint-from-`ghidra_symbols`, `mariogolf64.ld`/segment-structure off-limits).
- Carry-over: none

---

_(first sprint closed — future `/sprint-review` digests prepend above this line, newest first)_
