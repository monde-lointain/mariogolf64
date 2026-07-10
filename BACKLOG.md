# Mario Golf 64 decomp product backlog

The Product Owner (you) orders this. The live candidate list is **not** duplicated here;
target selection is `tools/pick_target.py` (smallest-first ranker, run at `/sprint-plan`).
This file holds only the *ordering rationale*, *enabler items* (gate actions), and
*carry-overs*. `sprint-plan` re-refines the slice each sprint;
`sprint-review` reprioritizes. See `docs/agent-workflow.md ## Scrum operating model` for the cadence.

## Active phase / epic

**Epic 1 — complete the vendored-library function set — DONE (S148).** Every vendored-library band is
banked: libultra (clean-mirror vein), libnusys, libkmc, libnaudio, libmus, libnualstl, mgu. `--lib`
on any of these reports no candidates. The only rows still TAGGED libultra/libnusys are coddog
*structural* false-hits (`body-divergence-suspect`, `game-region-mirror`, the "second libnusys
instance") — game code that fingerprints to lib source but is NOT a verbatim mirror, so they belong
to Epic 2.

**Epic 2 — decompile the game's own code, smallest-first classical (active since S148).** ~1521 fns
across 77 game TUs remain (post-S148): `none`-upstream packs plus the game-region/structural rows
above. All classical track (the mirror regime is mined out). Target selection stays
`tools/pick_target.py` (smallest-first); the 8-point decompose gate fires on any seed >=8, now tempered
by the **small classical pack exemption** (`docs/agent-workflow.md ## Story points`: <=2 fns AND <256B AND one-tu
runs seed-only). v2 classical realized tier scored at review (since S11). Seed asm-first for small fns
(MCP-independent; see the Seed fast-path in `docs/agent-workflow.md`). Path convention `overlay_<N>/<stem>` or
`main/<stem>`, default -O2 game profile (no mk edit). Note: `pick_target.py`'s size-pts over-prices
tiny none-upstream packs (S148 priced a 176B trivial pack at 13) — a calibration follow-up (below).
**pts follow-up — regalloc-heavy dimension (S158):** the S155 rubric prices a one-tu classical pack by
size/nfns, but S158's seed-13 5-fn one-tu was MASSIVELY under-priced (realized 18) because all 3
non-trivial fns needed the full pervasive-regalloc playbook + a 3-round multi-agent fan-out + permuter.
Difficulty is driven by FP/trig + many callee-saved regs + struct-array pressure, none of which the
size/nfns model sees. Consider a `regalloc-heavy` pts bump in `pick_target.py` (detect FP ops / high
callee-saved count / struct-array access) — off-cadence, golden-gated tooling branch.
**Extend it beyond FP/trig (S167):** a 3-fn head priced seed-5 (tiny, 0-FP, no-jtbl) was an S158-class
cse/regalloc slog because the fns shared a **call-return game-state base pointer**
(`func_8005AF50()+0x990`) with **dense multi-field byte access** + a printf. A shared call-return
base-ptr + many byte-field loads/stores is a difficulty signal too — not just FP. And one fn hit the
`#cse-make-regs-eqv-branch-fold` wall (unreachable-from-equivalent-C), which no pts model predicts;
the mitigation is fast-carry recognition (the new hazard), not a price bump.
**Re-confirmed (S177):** a seed-8 6-fn one-tu heap pack realized ~11 (2 subagent-proven CSE/regalloc
WALLS + a 2-subagent GCC-source fan-out + 3 permuter setups). The unpriced difficulty signal here is
**`osSetIntMask`-guarded + constant-index struct-array (`D_800DC6E0[3]`) + ra-capture** heap/pointer fns:
the interrupt-mask's whole-fn liveness + the constant (caller-saved) loop-invariant drive an unreachable
regalloc rotation the size/nfns model can't see. Candidate detector: `jal osSetIntMask` + a fixed
`ARR[K]` struct-array base + `__builtin_return_address`-style ra read. Still a golden-gated tooling
follow-up (off-cadence), not a mid-sprint edit.
**Pervasive-regalloc TU detector (S183, off-cadence golden-gated):** `src/main/func_80052250.c` is a
`none` pack whose 2 trivial predicates bank but EVERY non-trivial fn (8/8) is a regalloc/structure wall
— compare/score/dispatch fns over `func_80052324`/`func_8005244C` + `D_801B71Fx` s8 byte-tables
(`[x*0xB8]` row stride) + struct-folded adjacent `D_801B609x`/`D_801B60A0` globals (accessed via a
`base - 0xC` derivation). Candidate `pick_target.py` signal: a `none` pack where fns share a
call-return/table-scan idiom (repeated `jal` to 2-3 same in-pack callees) + a fixed-stride byte-table
index + struct-folded neighbor globals → price it `regalloc-heavy` (bump pts) OR flag it as a partial-
bank-expected TU. Kin to the S158 FP/trig and S177 `osSetIntMask` regalloc-heavy pts follow-ups above.
**Path-convention exception (S149):** nusys/SDK-template main-segment code (the `idle=nuboot` coddog
tell) is carved to its LIBRARY tree (`libnusys/<file>`), not `main/<stem>` — yaml path-qualifier only,
placement unchanged (see `CLAUDE.md` path convention). A per-FILE -O0 override is the one mk edit a
boot/SDK-glue TU may need.

**Test-tools hygiene (S184, deferred, off-cadence golden-gated):** `make test-tools` carried 2
PRE-EXISTING non-golden failures, surfaced (not introduced) at the S184 review: (1) a broken
`## Playbook index` anchor `cse-make-regs-eqv-branch-fold...` in `docs/hazards.md` — the heading uses
`make_regs_eqv` (underscores) but the index link + the `CLAUDE.md`/`pick_target` citations use
`make-regs-eqv` (hyphens); reconcile in LOCKSTEP per the prompt-surface-as-API rule (heading is cited, so
fixing it means updating every reference). (2) `test_coddog_suppresses_maybe_upstream` KeyError (stale
fixture — the fn it probes changed state). The 4 `pick_target` live-state goldens were regenerated at the
S184 review (`REGEN_GOLDEN=1`, banking-only drift from S182–S184); consider making them fixture-based so
they stop drifting each banking sprint.

**FP/DL-wall pack pts detector (S189, off-cadence golden-gated):** `src/main/func_800660A0.c` is an
8-fn `none` pack where only the 4 non-FP fns were tractable (2 banked, 2 regalloc near-match walls) and
the 4 FP/DL fns are S158/`#display-lists`-class walls: `func_800674B8` (cosf/sinf/vec3f_normalize loop),
`func_80067730` (guAlignF/guScaleF/guMtxCatF/guTranslateF/guMtxF2L matrix builder), `emit_aim_target_ring`
+ `draw_course_decal_triangles` (F3DEX2 display-list emitters, 602/680i). The size/nfns model priced it 13;
realized 14 but only +2 banked. Same "FP/DL-heavy TU is partial-bank-expected, not size-13-worth-of-bank"
follow-up as S158/S177/S183: a `pick_target.py` detector for `gu*`/`cosf`/`sinf`/`guMtxF2L` FP calls +
`glistp++`/DL-command-word stores (`0xE7000000`/`0xDB…`/`0xFA…` literal stores) → flag the pack
**partial-bank-expected** (price the FP/DL fns at 0 expected-bank, the glue/init fns at their size). Kin to
the S158 FP/trig, S177 `osSetIntMask`, and S183 pervasive-regalloc pts follow-ups above. Golden-gated,
off-cadence, not a mid-sprint edit.

**Extend the detector to DL EMITTERS, not just FP (S190):** `src/main/func_8004E5A0.c` was a 3-fn
one-tu the ranker surfaced smallest-first as a "+2 tractable" pick because 2 of 3 fns are 0-jal/0-FP —
but they are `glistp++` **display-list emitters**, which are their OWN scheduling-wall class (the
`#display-lists` header constant-staging wall the permuter can't crack, plateaued 10065→6235), NOT
tractable integer glue. The S189 detector prices only FP fns at 0-expected-bank; it must ALSO price a
non-FP fn that STORES DL command words (a `sw` of `0xE7000000`/`0xE2xxxxxx`/`0xFAxxxxxx`/`0xFCxxxxxx`
literals through a `*glistp`-loaded running `Gfx*` cursor) at 0-expected-bank. Candidate signal for a
`none` pack: `Gfx`/gbi types + DL-command-word literal stores through a cursor loaded from `*arg` and
written back (`*arg = cursor`), regardless of jal/FP count. So a debug/HUD DL-renderer TU (all fns are
emitters) prices partial-bank-expected-**ZERO** and stops topping the smallest-first sort. Same
golden-gated off-cadence tooling follow-up; kin to the S158/S177/S183/S189 rows above.
**Re-confirmed + reframed (S204, PO-accepted at retro, QUEUED to the off-cadence golden-gated
`pick_target.py` branch):** `func_8003E004` (c-stub, priced 13) is another under-priced FP/6-callee-double
regalloc wall — spec for the detector: on a `none`/c-stub fn, count FP ops + callee-saved-double pressure
+ (new) whether the residual is a straight-line `#local-alloc-qty-permutation`; price regalloc-heavy /
flag partial-bank / permuter-expected. **Also accepted at S204 retro (#4, same branch): coddog
min-instruction floor** — suppress a `coddog-mirror:<file>@<pct>` tag on a fn of <=8 instrs (tiny store
fns fingerprint-collide; S204 `func_800772B0` writes ZERO to two globals but tagged `settime.c@99.99`
because osSetTime is also a 2-store leaf). Both are golden-gated (`make test-tools`, then
`REGEN_GOLDEN=1` for the intended re-price), NOT hand-edited inline at retro.
**Data point (S206, same off-cadence branch):** `src/main/func_800772B0.c` is a 6-fn all-FP one-tu
pack (spline/interpolation over vec3f arrays) whose 2 trivial glue fns banked free, 1 FP fn fell
(`func_800779A8`, a precise local-alloc coloring lever), and 3 FP-math fns are S158 regalloc walls
(`func_800772C4` 0.53, `func_8007775C` 0.73, `func_80077AD4` 0.62). Confirms the FP-pack detector:
an all-FP one-tu should price partial-bank-expected (trivial/glue fns at size, FP-math fns at ~0
expected-bank) rather than a flat size-13. Also reconfirms the S204 coddog min-instr floor: this pack
was mis-tagged `coddog-mirror:src/os/settime.c@99.99` (false fingerprint collision, it is a `none`
classical pack, NOT a libultra mirror) — no header vendoring was needed at the gate.

**S208 MIXED-PARTIAL — `src/main/func_80059BA0.c` integer-glue/accessor pack [0x34FA0], 23/59 (+23
this sprint).** matched **+23** (largest single-sprint count to date, 5.75× the +4 hedge). md5-candidate
**230 -> 230** (file 23/59, 36 stubs). Subseg flip `[0x34FA0, asm] -> c`. Unlike the S206/S207 FP/DL
packs, this 59-fn `none` pack was a DEEP non-FP integer vein (37/59 non-FP) — accessors, game-mode
store-init setters, call-chain glue, small clamps/loops — all asm-first byte-exact. Levers: -O2-no-inline
(cross-calls stay `jal`); branchless `max(x,0)`=`x&(~x>>31)`; local-ptr addr-reuse + bnel delay-store;
s8/u8 store-const + s8→s32 return-type. Seed 13; banked 0pt (mixed-partial); realized ~15; residual +2;
regime classical/mixed. Quality **0/0/4/0**. 4 CARRIED, all characterized ≥0.97 near-matches
(permuter-class): `func_8005B070` (1-instr `li`/`la` schedule transposition w/ coupled regalloc),
`func_8005D218`/`func_8005D2E4` (2-case switch `beq`-fallthrough+delay-fill vs gcc `bne`+`j`),
`func_8005D308` (`$a0`-accumulator regalloc). **PO takeaway:** a big `none` pack with a DEEP non-FP head
(many <16-instr 0-FP fns) is a high-yield partial — the `partial-bank-expected` detector should carry a
non-FP-integer-vein DEPTH signal, not just the FP/DL wall signal; price the value not the atomic-13.
Retro applied **4 of 4** (flowing-bss self-ref tell, goto preamble-order/regalloc coupling, `gcc -S`
codegen oracle, s8/u8+return-type levers — all folded into `docs/hazards.md`). Cross-repo: no new
curated names (banked fns kept `func_`). **Next natural slice:** a permuter sprint on the 4 S208
near-matches + the C458/C4B4/C5B4/C614 nested-counter family (all ≥0.97 loop/switch/regalloc), OR a
fresh non-FP main pack.

**S207 MIXED-PARTIAL — `src/main/func_80026400.c` scenery/heap pack [0x1800], 3/9 (+3 this sprint).**
matched **+3**. md5-candidate **223 -> 223** (file 3/9, 6 stubs). Subseg flip `[0x1800, asm] -> c`. A
9-fn one-tu pack: 2 heap-region-init heads + 2 string-dup-util tails (integer glue) + 5 FP/DL
scenery-render walls. BANKED: `func_8002646C` (slot-1 heap re-register, byte-exact asm-first
first-build), `func_80028110` (string-dup-with-header util; levers `(u32)len>=0x100` -> `sltiu`),
`func_80028204` (sibling w/ header read-back; lever `*out=0` before `osSyncPrintf` to steer the printf
delay-slot fill). **6 CARRIED:** `func_80026400` (heap-region-init head; structural-complete
delay-slot-fill/regalloc near-match — build folds `block+size` into printf's delay slot block->s0, ROM
saves `size` there + adds `end` late in `func_800263B0`'s delay block->s1; 4 source forms all hoist,
in-file note); `project_sort_scenery_cylinders` (analyzed = multi-IV FP insertion sort, 0x88-stride
struct-array + `project_point_view_depth` FP + branch-likely, S158 wall, not attempted-to-floor);
`update_scenery_cylinder_transforms` (FP32 wall) + `emit_scenery_billboard` / `draw_scenery_opaque_pass`
(FP252+DL17) / `draw_scenery_alpha_pass` (S189/S190 DL-emitter walls, detector-predicted, not
attempted). Seed 13; banked 0pt (mixed-partial); realized ~14; residual +1; regime classical/mixed.
Quality **0/0/6/0**. Reconfirms the S189/S190 FP-and-DL-emitter `partial-bank-expected` pts detector
(6 of 9 wall-class). Retro applied **0 of 1** (PO-declined: the delay-slot-fill/regalloc near-match as a
4th regalloc-heavy detector signature — kept as a data point, NOT queued). Cross-repo: no new curated
names (banked fns kept `func_`). **Next natural slice:** a fresh non-FP/DL main pack, OR a
permuter/`#cross-project-matched-corpus-mining` sprint on the accumulating S206+S207 FP/regalloc carries.

**S206 MIXED-PARTIAL — `src/main/func_800772B0.c` float spline/curve-interpolation pack [0x526B0],
3/6 (+3 this sprint).** matched **+3**. md5-candidate **223 -> 223** (file 3/6, 3 stubs). Subseg
flip `[0x526B0, asm] -> c`. A 6-fn all-FP one-tu pack (vec3f slots stride 0xC, shared globals
`D_800C45D0` seg-index / `D_800C45D4` substep counter), NOT a settime.c mirror (the
`coddog-mirror:src/os/settime.c@99.99` tag is the S204-flagged false 2-store-leaf collision). BANKED
byte-exact: `func_800772B0` (reset both globals) + `func_80077BD8` (`*a0=*a0` word RMW), both asm-first
first-build; and `func_800779A8` (periodic cubic-Hermite spline lookup, 75/75, via a subagent's precise
local-alloc coloring lever: `while`-loops not `if{do-while}`, split `t=x-base` to pin x in `$f12`,
non-negated `bc1fl`, fully-inlined return). **3 CARRIED, all fully-RE'd S158 `#pervasive-regalloc-
classical-main` FP walls** (`docs/wip/*.near-match.md`): `func_800772C4` (spline-segment stepper,
switch-dispatch, 0.53 — buffer-ptr-in-`$t0` + 2-global address-CSE cascade), `func_8007775C` (cyclic
cubic-spline solver Thomas+Sherman-Morrison, 0.73 — coupled loop-bound/FP-coloring), `func_80077AD4`
(cubic finite-diff interp, 0.62 — int-temp hard-reg perm + `ia0*3` hoist). **Method:** 3 parallel
isolated subagents over the FP tail (S184 recipe, each in its own `nonmatchings/<fn>/`, no build race);
1 returned an unexpected byte-match. Seed 13; banked 0pt (mixed-partial); realized ~17; residual +4;
regime classical/mixed. Quality **0/0/3/0**. Retro applied **3 of 3** (S1 decomp_loop mid-TU standalone-
offset artifact -> `#isolated-compile-caveat`; S3 FP-subagent fan-out recipe -> `## Workflow at a glance`;
S2 FP-pack pts detector data point + coddog min-instr floor reconfirm -> queued off-cadence branch).
Cross-repo: no new curated names (all `func_`/`D_`). **Next natural slice:** a FRESH main pack that is
NOT FP-dominated, OR a dedicated permuter/`#cross-project-matched-corpus-mining` sprint on the 3 S206
FP-regalloc carries (all < 0.97, permuter N/A now).

**S205 SPIKE/CARRY — `src/main/func_8005E380.c` fault register/flag dump printer, 0 banks (0 pt).**
matched **+0**. Sole committed increment; carried. `func_8005E380` (469 instr / 66 jal / 56 FP-reg,
2192B) is a game reimpl of the SDK `__osDumpThreadContext` (output via the game text renderer
`func_8005E360`/`func_8004D580`, not `osSyncPrintf`). FULLY RE'd off `__OSThreadContext` u64 GPR fields +
`__OSfp` FP regs (single-as-`(f64)fpN.f.f_even` then `fpN.d` double), semantically exact, **100%
structural (535/535 asm rows, identical mnemonic stream)** — in-file gold note (commit 8fe89cc) has the
complete body. **NOVEL WALL: CSE derived-pointer base-canonicalization.** GCC 2.7.2 CSE always
canonicalizes context-pointer arithmetic to the base PARAMETER `$4` (`move $16,$4`, folds +0x20 into every
displacement, keeps `thread` in a callee reg); ROM keeps `ctx=thread+0x20` materialized as the base
(`addiu $17,$4,0x20`). Distinct from `func_8003E004` (`move_movables` FP-hoist) and `func_80050428`
(`#local-alloc-qty-permutation`, S204). Ruled out **8 source forms** (register kw / char*-cast / eager
temp / decl order / `ctx[-3]` id read — GCC re-folds it back to `thread+0x14` / ...) and **19 flag/opt
variants** (-O0/O1/O3, -g, -funroll, -fno-{gcse,cse-follow-jumps,expensive-optimizations,defer-pop,...});
permuter (best-only `--main`, ~45k iters) **valid-floor ~1200** (base 27760 / 0.48 decomp_loop; lower
scores are UB — dropped print calls, ctx used uninit). PO mid-sprint pointed to ultralib
(`~/development/repos/ultralib`): confirmed NO verbatim source (rmon `__rmonCopyWords` copies raw words;
the `%f` single/double dump is game-specific), field names match. **NOT blind-retryable:** needs a
from-scratch permuter seeded PAST the base fold, or a source form forcing `thread+0x20` materialization
(none found). Fresh main c-stub singles now exhausted (`func_8003E004` + `func_8005E380` both confirmed
compiler walls); next main increment is a 13pt decompose-gated pack or the mispriced `func_800772B0`
one-tu pack. **Retro applied 3/3 (S205):** #1 `docs/hazards.md#cse-derived-pointer-base-canonicalization`
section + TOC + hazard-index rows (DONE inline); #3 isolation-caveat refinement in
`docs/agent-workflow.md ## Execution loop` (low-percent full-rows + empty-mismatches ≠ always an
isolation artifact — disambiguate with in-tree `diff.py`) (DONE inline); #2 `pick_target.py`
regalloc-heavy pts-detector — add the CSE-base-canon signature (single pointer param + big-substruct-base
dump → price permuter/carry-expected) QUEUED to the off-cadence golden-gated branch (joins the S158/
S177/S183/S203/S204 detector, now 3 wall signatures: FP-hoist, qty-perm, base-canon).

**S204 MIXED-PARTIAL — `src/main/func_80050400.c` ROM-load-slot head [0x2B800] COMPLETE + `func_8003E004`
compiler-source spike.** matched **+1** (`func_80050428`); md5-candidate file `func_80050400.c` now 0
stubs. `func_80050428` (ROM-slot directory loader: two aligned `nuPiReadRom` DMAs from `D_E473F0[i*8]`,
fills `RomLoadSlot`) is a straight-line (1 basic block) fn that seeded to a near-match with only a
`local-alloc.c` s-register **permutation** + one independent-store schedule move. PO directed a
codegen dive before the permuter: proved 1-BB → local-alloc qty priority (not global.c); 5 source levers
didn't move it; **permuter found score 0 (iter ~14250)** → new `#local-alloc-qty-permutation` hazard.
Winning levers: reference `D_E473F0` inline (no pointer local), cache the size re-read before the
`D_8012D3A8` store, array decl-order sets the aligned-scratch stack offsets. Trap: the isolated
`nonmatching-func` object DIVERGED from the in-tree build (gate on in-tree + ROM SHA-1). **Also this
sprint:** re-investigated the S203 carry `func_8003E004` as a PO-directed compiler-source spike (0 bank)
→ definitive `move_movables` DFmode `(double)base` hoist root cause (loop.c:1630, 4 subagents + RTL
dumps, gas exonerated, 9 clean variants plateau 12439-14020); gold carry note in-source. Quality
**0/1(won)/2/0**. Seed 6; banked 3pt; realized 5; residual +2; regime classical. Retro applied **4 of 5**
(#2/#3 docs DONE; #1/#4 queued to tooling branch; #5 seed-dir dedup not selected). Cross-repo: no new
curated names (siblings all `func_`). **Carry-overs:** `func_8003E004` (confirmed `move_movables`
hoist wall, gold note); `func_8004DC44` (`#signed-divide-const` grid-copy, an already-multi-sprint carry
S172-S175, deferred as the S204 stretch — NOT a fresh candidate). **Next natural slice:** a fresh main
c-stub 1-stub file completion (`func_80070FD0.c`, or `lz_decompress_simple.c`'s `lz_decompress_extended`),
or a fresh main pack.

**S202 MIXED-PARTIAL — `src/main/func_8005E380.c` debug/fault tail [0x39780], 1/2 (+1 this sprint).**
matched **+1**. md5-candidate **229/244->229/245** (file 1/2, 1 stub). `func_8005EAD4`
banked byte-exact as a fault/debug flag-label list printer over `{mask, value, label}` records.
`func_8005E380` reached a struct-complete first pass over `OSThread.context`; raw context-pointer and
32-bit `{hi, lo}` GPR-pair views fixed offset drift, but the function stopped on a whole-function
`s0`/`s1` base-pointer color swap plus prologue order. Quality **1/0/1/0**. Seed 8; banked 0pt;
realized 10; residual +2; regime classical/mixed. Retro applied **0 of 0**. Cross-repo: no new curated
names. **Next natural slice:** continue a fresh main pack, or retry `func_8005E380` only with a new
source-shape/codegen insight; no explicit register binding.

**S201 BANKED — `src/main/func_8003E400.c` main head slice [0x19800] COMPLETE.** matched **+6**.
md5-candidate **228/243->229/244**. Split the S200 tail at the aligned `0x19AC0` boundary and banked
`func_8003E400` (asset read/heap alloc setup), `func_8003E4B4` (heap frees),
`build_pin_or_cup_matrix_for_dad10` (camera-relative matrix build), `func_8003E628`, `func_8003E638`,
and `func_8003E648` (render eligibility predicate). Matrix helper needed final address association
`(D_800B7780 << 6) + (arg << 7)` with the arg offset added in the call delay slot. Quality **0/0/0/0**.
Seed 5; banked 5pt; realized 5; residual 0; regime classical/mixed. Retro applied **0 of 0**.
Cross-repo: no new curated names. **Next natural slice:** continue a fresh main pack, or return to
the `0x19AC0` render tail only with a display-list scheduling plan.

**S203 MIXED-PARTIAL — `src/main/func_8003DFD0.c` main head slice [0x193D0], 2/3 (+1 this sprint; was
S200 1/3).** md5-candidate unchanged (file 2/3, 1 stub). S203 banked `func_8003E314` byte-exact: it is
the wind-indicator display-list builder — `gDPSetCombineLERP(SHADE/TEXEL0)` + `gDPPipeSync` +
`gSPVertex(D_800BA888,18,0)` + `for(i=0;i!=8;i++) gSP2Triangles(glistp++, i*2, i*2+2, i*2+1, 0,
i*2+1, i*2+2, i*2+3, 0)` (gfxdis decoded the head; the macro's ×2 matched the asm's i*4/+2/+4/+6
induction vars; one fix `i!=8` not `i<8`). **Remaining stub — `func_8003E004` (SPIKE, FP regalloc
wall):** the wind-vertex generator that fills the D_800BA888 Vtx array E314 draws. FULLY solved
structurally + math (base angle = `(f32)((f64)((f32)wind_angle*2pi/65536)+pi/2)`; per i 0..8 two records
`sin(base)*i*8533 + sin(base±pi/2)*(9-i)*46.079998` for x, the −cos analogue for z, y =
terrain_height(base+off)−base_y; then /4-store as s16 into `D_800BA888[k].v.ob`). Isolated diff
**206/223 rows**; only the `(f64)base` double + the scratch-buffer base pointer spill vs the ROM's
callee-saved allocation (8 extra spill instrs). Permuter (setup `--main`, 280s, 15 threads,
`--stop-on-zero`) drove base 7115 → **best 3505, no zero**. Candidate seed preserved at
`nonmatchings/func_8003E004-2/base.c` for a dedicated longer permuter run. Same regalloc-wall class as
the S202 sibling carry `func_8005E380`. Quality **1/1/1/0**. Seed 13 (c-stub); banked 0pt; realized 16;
residual +3; regime classical/mixed. Retro applied **0 of 0**. Cross-repo: no new curated names.
**Next natural slice:** longer permuter run on the preserved seed, or a fresh main pack.

**S199 MIXED-PARTIAL — `src/main/func_80050710.c` ROM-load helper tail [0x2BB10], 0/2 (+0).**
matched **+0**. md5-candidate **228/242→228/242** (file 0/2, 2 stubs). `func_80050710` reached a
structural-complete isolated near-match (143/143 rows; residual saved-register color swap `s1`/`s2`).
`func_80050914` reached 200/202 rows; residual saved-register order plus one branch-likely detail in the
two-byte RLE variant. Compiler-source fan-out confirmed the address-taken-local stack-counter lever
(`mark_addressable` -> `put_var_into_stack`) but no faithful lever for the final register coloring. Quality
**0/0/2/0**. Seed 8; banked 0pt; realized 10; residual +2; regime classical/mixed. Retro applied **0 of 0**.
Cross-repo: no new curated names. **Next natural slice:** continue a fresh main pack, or retry this tail
only with a new source-shape/compiler-codegen insight; no explicit register binding.

**S198 BANKED — `src/main/func_800505A0.c` ROM-load helper head slice COMPLETE.** matched **+3**.
md5-candidate **227/241→228/242**. Split the S196 tail at the 16-aligned `0x2BB10` boundary and banked
`func_800505A0` (bounded ROM read + cursor advance), `func_8005062C` (slot init + 4-byte header read),
and `func_800506D4` (slot mode dispatcher). Quality **0/0/0/0**. Seed 5; banked 5pt; realized 4;
residual -1; regime classical/mixed. Retro applied **0 of 0**. Cross-repo: no new curated names.
**Next natural slice:** continue a fresh main pack, or return to `0x2BB10` tail with a new aligned slice.

**S197 BANKED — `src/main/func_800263B0.c` first scenery-cache init slice COMPLETE.** matched **+1**.
md5-candidate **226/240→227/241**. Split the 10-fn main pack at the 16-aligned `0x1800` boundary and
banked `func_800263B0` (debug print, sprite decode cache clear, `heap_init(1)`, flag clear). Gate
triage rejected `0x526B0`: first split `0x526C4` was non-16-aligned and whole-pack scaffold lacked
sibling fallback exports. Quality **0/0/0/0**. Seed 1; banked 1pt; realized 1; residual 0; regime
classical/mixed. Retro applied **0 of 0**. Cross-repo: no new curated names. **Next natural slice:**
continue a fresh main pack, or return to the `0x1800` tail with a new aligned slice.

**S196 MIXED-PARTIAL — `src/main/func_80050400.c` main-slice ROM-load slot pack [0x2B800], 5/6
(+5 this sprint).** matched **+5**. md5-candidate **226/239→226/240** (file 5/6, 1 stub). Split the
structural `libc/llcvt.c@99.99` pack at `0x2B9A0`; banked `func_80050400` (slot-start clear),
`func_800504E8` (wrapper), `func_80050504` (slot allocator/debug print), `func_80050588` (slot reset),
and `func_80050598` (slot size getter). **1 CARRIED:** `func_80050428` ROM table read/setup; correct
stack layout found (`0x40` scratch buffers, `&buf[0xF]`) but unconstrained C remains a saved-register
rotation far below permuter threshold; explicit register binding rejected by PO. Quality **1/0/1/0**.
Seed 5; banked 0pt; realized 7; residual +2; regime classical/mixed. Retro applied **0 of 0**.
Cross-repo: no new curated names. **Next natural slice:** retry `func_80050428` only with a new
source-shape/codegen insight, or continue a fresh main pack.

**S195 BANKED — `src/main/func_8005EC10.c` aligned audio boot/pre-NMI tail COMPLETE.** matched **+4**.
md5-candidate **225/238→226/239**. Split the S194 tail at the 16-aligned `0x3A010` boundary and banked
`func_8005EC10` (pre-NMI callback setter), `func_8005EC48` (pre-NMI/retrace callback), `func_8005ECC4`
(pointer-bank load/debug dump), and `audio_system_boot` (audio boot glue). Initial `0x39ED4` split was
rejected by gate checksum: non-16-aligned C object padding shifted ROM bytes, so `func_8005EAD4` stays
asm. Quality **0/0/0/0**. Seed 5; banked 5pt; realized 6; residual +1; regime classical/mixed. Retro
applied **0 of 0**. Cross-repo: no new curated names.

**S194 BANKED — `src/main/func_8005E2C0.c` 0x396C0 fault/debug head split COMPLETE.** matched
**+3**. md5-candidate **224/237→225/238**. Split the 9-fn structural `libnusys@99.99` coddog pack at
0x39780 and banked only the 0xC0-byte head: `func_8005E2C0` (dead-stack leaf), `func_8005E2CC`
(fault-thread message loop), and `func_8005E360` (`osSyncPrintf` wrapper). Tail `func_8005E380` and
following funcs stay asm. Quality **0/0/0/0**. Seed 3; banked 3pt; realized 2; residual -1; regime
classical/mixed. Retro applied **0 of 0**. Cross-repo: no new curated names.

**S193 BANKED — `src/libultra/debug/assert.c` (`__assert`, 0x20-byte leaf split from 0x4CE0).** matched
**+1**. md5-candidate **223/236→224/237**. Split the 19-fn 0x4CE0 pack at 0x4D00, banked only
`__assert`, and left `vec3f_normalize` + 17 following funcs in asm. Upstream source:
`~/development/repos/ultralib/src/debug/assert.c`; ROM body is an older one-argument `osSyncPrintf`
shape that loads existing `D_800CA184`, so the C references `extern const char D_800CA184[]` instead of
emitting a literal. Quality **0/0/0/0**. Seed 1; banked 1pt; regime mirror. Retro applied **1 of 1**
(existing extracted string/data symbol note → `docs/hazards.md#recover-extern-refs-unplaced`). Cross-repo:
none (`__assert` already curated).

**S191 MIXED-PARTIAL — `src/main/get_table_entry.c` meter-stick/ball-physics/sound-trigger logic pack
[0xE260], 3/11 (+3 this sprint).** matched **+3**. md5-candidate **223→223** (file 3/11, 8 stubs; total
.c 235→236). Seeded via **m2c-from-repo + RE'd-struct `--context`** (Ghidra `TerrainAttrEntry` + a
synthesized 0xB8 `ShotInitRecord` for the struct-array init base) — confirms the recipe on a classical
main LOGIC pack, not just call-glue (S186). Banked byte-exact: `get_table_entry` (terrain-attr LUT
index; the **ternary** `(idx<27)?idx:0` clamp gives branchless `sltiu`/`negu`/`and`, where m2c's
`&arr[idx & -(idx<27)]` BRANCH-FOLDS), `func_80032E88` (0xB8 struct-array single-pass init, first
integrate), `func_80037E50` (club/terrain table-address select). **`func_80037E50` cracked a `$a0`-vs-
`$v1` regalloc with ZERO permuter** via the gcc-2.7.2 `.greg`-dump method (new Axis 6): `quality`
(allocno 76) conflicted with both `$v0`+`$v1` because the return `A+base+C` (base = symbol addr)
reassociated to a 2nd `$v1` accumulator → forced `$a0`; FAITHFUL fix = **stepwise pointer arith**
`p=base+quality*K; return p+arg3*K2;` (single-accumulator-`$v0` eval, frees `$v1`) + the
`if(arg3<6)return general; return edge;` block reorder. **Pre-classified 6 of 11 as nested children**
(dead-`$v0`-spill / `$v0`-chain tell) of the 2 FP-wall parents → focused seeding on the 3 standalone
fns. Quality **0/0/8/0** (0 stuck-far, 0 permuter, 8 carried, 0 re-opened). Seed 13; banked 0pt
(mixed-partial); realized 15; residual +2; regime classical/mixed. Retro applied **4 of 4** (`.greg`
Axis-6 eval-order lever → `#loop-weight-and-live-length-regalloc-steering`; isolation-noise-masks-reg-
permutation corollary → `#isolated-compile-caveat`; callee-side nested-child pre-classify + a
`nested-child:<parent>` pick_target follow-up → `#nested-function-static-chain-spill`; m2c-classical-
logic confirmation + ternary/stepwise levers → `CLAUDE.md` Seed step). **8 CARRIED** (see
`## Carry-overs`). Cross-repo: no new curated names (all `func_`/pre-curated). **Next natural slice:**
a FRESH main pack that is NOT rendering/FP-heavy (this pack's tail is 2 FP-wall parents + their nested
children), OR a dedicated FP-wall-parent sprint (`update_ball_physics` / `init_ball_for_shot`) where
the 6 nested children bank FREE alongside the parent (child-before-parent, one TU).

**S189 MIXED-PARTIAL — `src/main/func_800660A0.c` course-decal/aim-target/rumble rendering pack
[0x414A0], 2/8 (+2 this sprint).** matched **+2**. md5-candidate **223→223** (file 2/8, 6 stubs; total .c
233→234). Banked byte-exact via **m2c + Ghidra shape** (no RE'd struct — Ghidra models the collision-record
fields as separate symbols + running offset, matching ROM codegen): `func_800660A0` (D_800C3100 flag set)
+ `func_800676B0` (collision-record[0] `s16`-field init). **New seed-order sub-lever:** `func_800676B0`
missed in Ghidra's compiler-SCHEDULED store order (collapsed the reused-constant live ranges → `0x800`→`v0`)
and matched first build in human **row/field order** (spreads the live ranges → ROM's front-loaded
`a0=0x800`/`v1=-0x100`) → `#pervasive-regalloc-classical-main` seed-order sub-lever. **6 CARRIED:** 2
regalloc near-matches — `update_rumble_intensity_table` (loop FORM solved via goto-outer for the
`#top-tested-loop` reversal corollary + signed pointer compares → plain `bnez`; residual a 3-reg rotation
`i`/`row+n`/`row+3`, `#loop-weight`, permuter `--main` plateaus 45/55) and `func_80067A60` (structure
matched but build MORE optimal than ROM — ROM's redundant mask save/restore + no `%lo`-fold not
source-reachable); + 4 FP/DL walls (`func_800674B8`/`func_80067730` S158 FP, `emit_aim_target_ring`/
`draw_course_decal_triangles` F3DEX2 DL). Seed 13; banked 0pt (mixed-partial); realized 14; residual +1;
regime classical/mixed. Quality **0/1/6/0**. Retro applied **3 of 3** (row-order seed lever →
`#pervasive-regalloc`; `setup-permuter.sh` `set -u` guard; FP/DL-wall pts detector → follow-up above).
Cross-repo: no new curated names (all `func_`, Ghidra default-named). **Next natural slice:** a FRESH main
pack that is NOT FP/DL-dominated, OR a permuter/`#cross-project-matched-corpus-mining` increment on the 2
regalloc near-matches (rumble/A60).

**S188 MIXED-PARTIAL — `src/main/set_camera_matrices_fixed.c` camera/projection FP-math pack [0x40180],
3/11 (+3 this sprint).** matched **+3**. md5-candidate **223→223** (file 3/11, 8 stubs; total .c 232→233).
Banked byte-exact via **PO-directed m2c + Ghidra RE'd `Mtx4f` struct context**: `project_point_view_depth`
(dot-product), `func_80065D5C` (in-place 4x4 matmul — **2D `a[i][k]` indexing de-biases loop.c
`combine_givs`**, the walking-pointer m2c seed tripped it), `convert_and_pack_floats_to_fixed` (**game
guMtxF2L variant** — ultralib `gu/mtxutil.c` source but NOT byte-id to ultralib's own J build; diverges
via `!=` held-const loops + dropped-redundant `& 0xffff0000`; name kept, canonical `guMtxF2L` taken
@0x80067B00). **8 CARRIED, all FP regalloc/scheduling walls** (see `## Carry-overs`), `func_80065A1C`
GCC-source-confirmed IRREDUCIBLE (6th-callee-saved-FP-reg product-hoist, no `REG_ALLOC_ORDER`),
`func_80065898` permuter-plateau (1470→670/117k). Quality **0/1/8/0**. Retro applied **3 of 3** (m2c+Mtx4f
de-bias + game-gu-variant-vs-BUILD-.o tell → `#game-region-mirror--o2-profile`/`#indexed-vs-pointer`;
FP-camera regalloc sub-case → `#pervasive-regalloc-classical-main`). Cross-repo: no new curated names.
**Next natural slice:** a FRESH main pack that is NOT camera/projection FP-math (this pack is the FP-regalloc
wall boundary), OR a dedicated permuter/`#cross-project-matched-corpus-mining` sprint on `func_80065898`,
OR `func_80065E6C` as a rodata-jtbl-carve increment.

**S186 MIXED-PARTIAL — `src/main/lz_compress_extended_dma.c` terrain/hole-loader pack [0x440A0], 10/15
(+10 this sprint).** matched **+10** (5 call-glue getters/setters + nested-RNG triple `func_80068F4C`+
nested `func_80068F00/F18` + `func_8006955C` switch + `func_80069BCC` mem-in-struct). md5-candidate
**223 → 223** (file has 5 stubs; total .c 230→231). NOT pure LZ despite the lead-fn name (a hole/terrain-
loader TU: LCG RNG, overlay/thread mgmt, asset loading + the LZ/DMA core). Seeded via **m2c-from-repo +
RE'd `LzDecompressState`/OS-struct `--context`** (PO directive) — byte-faithful call-glue seeds first-
build. The **PO-directed compiler-source fan-out** (2 gcc-2.7.2 subagents) cracked `func_80069BCC`'s tail
CSE-reload: root-caused to `cse.c invalidate_memory` (`nonscalar && p->in_struct` purge), fixed by reading
`D_801B6098[0]` (MEM_IN_STRUCT_P) → byte-exact, **ZERO permuter, ZERO stuck-far**. Nested-RNG triple = 3rd
end-to-end nested-fn recipe confirmation (pure-global-only children still framed). `func_8006955C` = a
one-instr `sltiu`/`slti` signedness fix (`D_800BA9FC` u32). Seed 13; banked 0pt (mixed-partial); realized
15; residual +2; regime classical/mixed. Quality **0/0/5/0**. Retro applied **4 of 5** (mem-in-struct
cse.c CSE-reload variant + sltiu/slti lever + m2c seeding recipe + whole-pack-flip relic note; nested-fn
anchor = no-op). 5 walls carried (see `## Carry-overs`). Cross-repo: no new curated names (all `func_`).
**Next natural slice:** a FRESH main pack (this pack's remaining tail is FP-scaler + two big compression
cores + thread-dispatch + DMA-goto wall-class), OR a permuter/`#cross-project-matched-corpus-mining`
increment on the 5 carries.

**S185 MIXED-PARTIAL — `src/main/func_80043C20.c` S184-tail rodata-carve retry, 14/21 (+1 this sprint).**
matched **+1** (`resolve_club_terrain_mask`, the terrain-mask switch). md5-candidate **223 → 223** (file
has 7 stubs). PO chose the rodata-carve retry of the 3 S184 jtbl/f64 carries + directed a compiler-source
fan-out before the permuter (systematic-debugging, `@mips-gcc-2.7.2` + `@mips-binutils-2.6`). Banked
`resolve_club_terrain_mask` byte-exact: the jtbl_800CC530 `.rodata` carve + a **cross-jump-tail-merge**
fix (two equal-constant `return 1` sites merged, blocking the ROM's tight `bnez end`+delay fold →
**switch-funnel-through-result-var** gives the switch cases a distinct exit label so `find_cross_jump`
never merges; reorg + jump.c subagents root-caused it, binutils ruled out). **Solved the
interleaved-one-tu rodata carve** (`resolve_shot_quality_table`: individual `static const char[16]`, NOT
2D/extern → GCC emits `[jtbl530][23 tables][jtbl700]`=0x230 matching the ROM) + the **address-select
lever** (22→3 diffs), but the last 3-insn v0/v1 residual is a **proven-irreducible** sched artifact
(sched.c subagent, load-latency-3 fixed point) → carried structural-complete with a decomp.me scratch
(`dZACn`). `predict_shot_distance` not attempted (S181 abs-coalescing class). Seed 5; banked 0pt
(mixed-partial); realized 8; residual +3; regime classical/mixed. Quality **0/0/2/0**. Retro applied **3
of 4** (interleaved-rodata recipe + address-select lever + sched-irreducible note; decomp.me note NOT
selected). **The PO-directed fan-out (5 subagents) did its job: cracked one wall, proved one irreducible,
zero premature permuter.** Cross-repo: no new curated names (all pre-curated). See `## Carry-overs`.
**Next natural slice:** the `resolve_shot_quality_table` near-free replay (carve done, needs a NEW
`power`→`$v0` reg-pressure idiom), OR a FRESH main pack (the func_80043C20.c tail is now regalloc/abs/DL
wall-class), OR a dedicated permuter/`#cross-project-matched-corpus-mining` sprint on the 3 regalloc carries.

**S184 MIXED-PARTIAL — `src/main/func_80043C20.c` main-segment [0x1F020] golf-shot/club logic pack, 13/21
(+13 this sprint).** matched **+13** — the **highest single-file classical bank to date**. BANKED
byte-exact C: 7 getters/wrappers (`get_club_distance_slot`/`get_club_param`/`get_shot_data`/
`get_shot_param`/`get_shot_progress`/`func_80044A8C`/`func_80044C7C`) + `func_800444B8`/`func_80044470`
nested pair (init loop + /14 lookup) + `func_80044254` (club-param init) + `func_80044FDC`
(auto-select-club) + `func_80043C64`/`func_80043C20` nested formatter pair. NEW reusable levers (all
applied to docs this review): **`combine_givs` base-IV de-bias** (`&ARR[i]` recompute inside the loop vs
carried `p++`), the **nested-function bank-the-pair recipe** confirmed on 4 nested fns (child emits before
parent; pure-leaf still framed when nested), **`for(;;)`+`break`** for an un-rotated top-test loop, and the
**/14 shared-magic** shift. **7-way parallel subagent fan-out** over the hard tail (decomp_loop is
parallel-safe across fns; orchestrator integrates + full-makes serially). **8 CARRIED** (see
`## Carry-overs`). Quality **0/1/8/0**. Retro applied **6 of 6**. **Next natural slice:** a rodata-carve
increment for the 3 jtbl/f64 carries (`resolve_club_terrain_mask`, `resolve_shot_quality_table`,
`predict_shot_distance`), OR a permuter/`#cross-project-matched-corpus-mining` increment for the 3
pervasive-regalloc carries (`func_80044CCC`, `func_800451E4`, `predict_shot_distance_variant`), OR a FRESH
main pack whose tail is NOT shot-math/dispatch (this pack's non-getter tail is the same
`#pervasive-regalloc-classical-main` wall class as S182/S183). Cross-repo: no new curated names to sync
(all pre-curated; no `symbol_addrs` adds).

**S183 MIXED-PARTIAL — `src/main/func_80052250.c` main-segment [0x2D650] game-code pack, 2/9 (+2 this
sprint).** matched **+2** (`func_80052250` = `scenario_mode_id == 0xC` predicate; `func_80052324` =
scenario table lookup `D_801B71F3[a0*0xB8] + func_80052264(scenario_mode_id, D_801B6098,
D_801B71F9[a0*0xB8])`, fixed via the `#register-reuse-nudge` addu-operand-order lever). md5-candidate
**223 → 223** (file has 7 stubs). **DEFINITIVE `#pervasive-regalloc-classical-main` TU: 8/8 non-trivial
fns are regalloc/structure walls; only the 2 trivial predicates banked.** 7 CARRIED (see `## Carry-overs`),
all with `docs/wip/*.near-match.md`. Key: `func_80052264` root-caused to a genuine 1-scratch-register
`REG_ALLOC_ORDER` artifact (m2c + GCC-2.7.2/binutils-2.6 source proven — hazard `nop`s CORRECT, table
index matched via operand-order lever; permuter 42k-iter plateau; TU-wide `#profile-probe` confirmed -O2
best, no flag flips the walls). Seed 13; banked 0pt (mixed-partial); realized ~18; residual +5; regime
classical/mixed. Retro applied **3 of 3** (objdump `-dz` byte-diff doctrine → `#assembler-differences`;
`(&PLACED)[-N]` struct-fold idiom → `#struct-access-folding`; pervasive-regalloc TU detector → BACKLOG
pts follow-up above). PO chose "attempt remaining 4" → 3 built+confirmed walls (5C4/834/A68, growing
diffs 84/128/150), CF0 decoded. Quality 0/1/7/0. **Next natural slice:** a FRESH main pack that is NOT a
compare/dispatch cluster (this TU's wall class), OR a dedicated permuter/cross-project-mining sprint on
the closest S183 carries (`func_80052264` 1-reg, `func_800525C4` struct-fold cascade). Cross-repo: no new
curated names to sync (250/324 stayed auto `func_`; Ghidra had no names).

**S182 MIXED-PARTIAL — `src/main/func_8004D190.c` VI/framebuffer + grid-print debug-display pack, 3/7
(+3 this sprint).** matched **+3** (`clear_text_grid` [0x8004D794, backward buffer clear],
`set_flag_based_on_param` [`flag` is s8: the `-0x80` immediate, not u8's 0x80], `func_8004D580`
[string-render wrapper; the `lb`(test)+`lbu`(arg) double-load is natural — the callee may clobber
`*str`]); md5-candidate **223 → 223** (file has 4 stubs → not md5-candidate). Flipped the 7-fn `[0x28590]`
pack (NO rodata carve; all `D_` externs placed). **4 CARRIED, 2 distinct hard classes.** (1) **Two
regalloc-ARTIFACT walls where MINE is more optimal than the target** — `func_8004D4B8` (string→glyph-tile
blit) is a `#dead-frame-reload-artifact-regalloc-wall` (structurally byte-exact once `c-=0x20` in-place +
the clean `for` inner loop matched the branch-likely shape; only the target's DEAD 8-byte frame + the
reg-perm it drives differ; root cause confirmed in `mips.c` compute_frame_size — local-alloc reserves a
slot despite free `t7-t9`, global fills it → dead slot); `func_8004D5F0` (framebuffer glyph renderer,
double-buffered) is a register-COALESCING wall (99/105; target keeps 2 non-coalesced copies of `c`, mine
coalesces; permuter main-profile base 2055 → plateau 1190, no path to 0). Both are the documented
MG64 main-segment pattern (cf. `func_80076640` S181, `func_8004DC44` S172-S175). (2) **Two `#display-lists`
builders** deferred to a dedicated DL sprint: `func_8004D190` (~200-insn unrolled texture-load DL preamble,
leaf) + `func_8004D7B8` (~120-insn text-grid render loop). Near-matches + root causes in
`docs/wip/func_8004D4B8.near-match.c.txt` and `docs/wip/func_8004D5F0.near-match.c.txt`. Quality 0/1/4/0
(permuter 1, carried 4). Seed 13; banked 0pt (mixed-partial, per-file all-or-nothing); realized 14;
residual +1; regime classical/mixed. Retro applied **3 of 3** (new `#profile-probe` TU-wide-cluster
doctrine; `#permuter-setup` direct-import bypass for an inlined multi-fn-file near-match; 2 deferred
golden-gated tooling follow-ups below). Cross-repo follow-up: `clear_text_grid = 0x8004D794` →
`sync_decomp_names.py --import-from-decomp`. **Next natural slice:** a FRESH main pack (e.g. `func_80052250`
9fn, `func_800660A0` 8fn) for reliable banks, OR a dedicated DL sprint for the `func_8004D190` DL builders.

**S180 BANKED — `src/main/func_800328E0.c` DCE0 DL pack COMPLETE → md5-candidate (+1 fn, the S179 carry
RESOLVED).** matched **+1** (`func_800329F4`, fill-rect RCP clear); md5-candidate **222 → 223** (file now
0 `INCLUDE_ASM`). RETIRED the S179 "permuter-class" scheduler-load-pair wall via a FAITHFUL
`#mem-in-struct-scheduling-lever`: the 3 RGB fill globals (`D_800B7840/44/48`) as plain `s32` no-alias the
`mem/s` `*glistp` stores, so GCC 2.7.2 sched1's sticky `LAUNCH_PRIORITY` birthing-boost hoists the color
loads into the glistp load-shadow; retyping them as one `Color {s32 r,g,b;}` struct (symbol
`clear_color = 0x800B7840; // type:Color size:0xC`, data as-is in main_data, NO carve) makes the loads
`mem/s` → may-alias → pinned after the stores = the ROM's deferred schedule. Paired with the n64demos
`gfxClearCfb` idiom (inline double-`GPACK_RGBA5551` in the `gDPSetFillColor` arg, so the color compute
lands after the fill-color w0 store). `func_800329D8` setter now writes `clear_color.r/.g/.b`. Clean-rebuild
ROM SHA-1 byte-exact; no permuter. Found by the PO-directed systematic-debugging two-agents-per-wall
GCC-2.7.2+binutils-2.6 fan-out (3 subagents, ORTHOGONAL lenses: scheduler-priority + machine/alias + gas
rule-out; both compiler agents independently converged on the `mem/s` lever). Quality 0/0/0/0. Retro applied
4 of 4 (`#display-lists` permuter-class retirement + `#mem-in-struct-scheduling-lever` DL extension + CLAUDE
index row; two-agents ORTHOGONAL-LENSES 2nd confirmation; n64demos provenance). See RETRO S180.
**Tooling follow-up (S180 #5, DEFERRED, off-cadence golden-gated):** teach `pick_target.py` to flag a DL fill
fn that reads its color from ≥2 separate `D_` globals which a SIBLING fn writes as consecutive words →
`mem-in-struct` color-struct candidate (the consecutive-word setter is the mechanical tell); kin to the S158
regalloc-heavy / S177 `osSetIntMask` pts-detector follow-ups above.

**S179 MIXED-PARTIAL — `src/main/func_800328E0.c` DCE0 display-list pack, 5/6 (+5 this sprint).** matched
**+5** (`func_800328E0` viewport/segment/camera DL preamble, `func_800329B8`/`func_80032E34` &glistp
wrappers, `func_800329D8` set-fill-RGB, `func_80032B78` color/Z framebuffer clear); md5-candidate **222
→ 222** (file has 1 stub → not md5-candidate). The **first PO-directed m2c + gfxdis.f3dex2 DL seed**:
m2c for the bodies, `extract_dlist.py`/`gfxdis.f3dex2 -d` to reconstruct the GBI macros from the
immediate command words. Reference idiom confirmed via `~/development/n64/n64demos` `graphic.c`
(`gfxRCPInit`/`gfxClearCfb`): double-`GPACK_RGBA5551` fill color, `OS_K0_TO_PHYSICAL`/`osVirtualToPhysical`
addresses, and the `& ~7` phys-align mask is GAME-SPECIFIC (demo lacks it; `func_80032B78` needs it).
Carried: `func_800329F4` (global-`glistp++` scheduler-load-pair wall; see `## Carry-overs`). Quality
0/1/1/0. Retro applied **4 of 4**: permuter `--main` flag + `permuter_settings_main.toml`; `#display-lists`
RCP-clear/`& ~7`/scheduler-load-pair additions + CLAUDE index rows; n64demos `graphic.c` reference
memory; this DL-track routing note. **DL-track routing (S179):** a display-list pack (a `glistp`/`nuGfx*`/
raw-GBI-command-word `.text`, e.g. a `coddog-mirror` false-hit like the DCE0 pack's nucontgbpakmgr.c) is
CLASSICAL `#display-lists`, NOT a mirror — seed with m2c + gfxdis, F3DEX2 profile via `mk/main.mk`.

**S178 BANKED — `src/main/func_8004DE60.c` slot-3 heap walls → md5-candidate (the 3 S177 "unreachable"
walls) + whole heap module rewritten to Code Complete.** matched **+3** (`heap3_add_region`,
`heap3_init`, `heap3_alloc`); md5-candidate **221 → 222** (file now 0 `INCLUDE_ASM`); 0 carries.
**Refuted BOTH S177 subagent-"proved" walls via a PO-directed two-agents-per-wall GCC-source fan-out**
(the adversarial agent per wall found what the primary's candidate set structurally couldn't): (1) the
CSE-reload wall (`heap3_init`/`heap3_add_region`) fell to the NEW **`#volatile-view-cse-reload`** lever
(per-access `volatile` view forces the `next=prev` reload the primary's volatile-one-field test
rejected; volatile size/state pin them ahead of the load-delay shadow); (2) the mask-rotation wall
(`heap3_alloc`, S177 Axis-4 "no clean lever") fell to **Axis-5 define-point liveness** (define `head`
AFTER `osSetIntMask(1)` → caller-saved → mask→$t1) + the **inline sentinel** (no head local → CSE
`move t0,v1`). No permuter, no cross-project mining. Then a **Code Complete rewrite** of the whole 2-file
heap module (new `src/main/heap.h` ADT; `Slot`→`HeapBlock`, `D_800DC6E0`→`heap_slots`, `unk_04`→`state`,
magic→`HEAP_*` constants, offset-0 aliases documented) — byte-exact on a clean-from-scratch build.
Quality **0/0/0/0** (and cleared 3 prior carries). Retro applied **4 of 4** (new `#volatile-view-cse-reload`
+ `#loop-weight` Axis-5 + two-agents-per-wall doctrine + `register asm("$N")` S178 confirm). Cross-repo
follow-up: the 3 new fn names + renamed globals → `sync_decomp_names.py --import-from-decomp`.
**Next natural slice:** the fresh main-segment **display-list** packs (see the forward note in
`## Carry-overs`) — `#display-lists` classical track, F3DEX2 profile, not a mirror.

**S177 MIXED-PARTIAL — `src/main/func_8004DE60.c` slot-3 heap module, 6/9 (+3 this sprint).** matched
**+3**; md5-candidate **221 → 221** (file has 3 stubs → not md5-candidate); asm subsegs **77 → 76**
(recombined the 9-fn pack into one object, removing `[0x295E0, asm]`). Banked byte-exact C:
`heap3_get_total`, `heap3_get_largest_free` (list-scan+cache), `heap3_free` (coalescing + ra-capture).
**Two NEW clean levers from a PO-directed GCC-source fan-out:** (1) `heap3_free` closed via the
**offset-0-symbol re-materialization** lever (new `#offset-0-symbol-re-materialization`: alias
`D_800DC6E0[3].total` as its own extern `D_800DC738` for the `+=` so GCC re-materializes `%hi/%lo`
instead of CSE-folding a base register — cascaded block->prev→`$a3`, mask→`$t0` for free); (2)
`heap3_get_largest_free` was NOT a wall — the "needs a synthetic no-op" verdict was a GCC-`-S`
reorder-mode misread; the clean inline-head already matched on the assembled `.o`. **Two subagent-proven
WALLS carried** (see `## Carry-overs`): `func_8004E1E0`+`func_8004E184` (nested pair, CSE varying-address
reload wall + permuter-blocked) and `func_8004E2DC` (heap_alloc slot3, mask→`$a0` 6-register rotation
wall). The S176 nested-carry is NOT resolved but the recombine + nested-fn approach is proven (the child
matches perfectly) and both walls are pinned to exact GCC-2.7.2 mechanisms. Quality 0/3/3/0 (permuter-set
3, carried 3 fns). Retro applied **6 of 6** (new offset-0 hazard + `-S`-vs-assembled note + nested-fn
recombine/CSE-reload + loop-weight Axis-4 + fan-out doctrine + pts follow-up re-confirm). Cross-repo
follow-up: 3 new names (`heap3_get_total`/`heap3_get_largest_free`/`heap3_free`) →
`sync_decomp_names.py --import-from-decomp`. **Next natural slice:** a FRESH main pack (`func_8004D190`
7fn, `func_80052250` 9fn) — do NOT re-attempt the func_8004DE60 walls without a genuinely new lever.

**S176 BANKED — `src/main/func_8004DE60.c` heap-allocator TU, 3/4 mixed-partial.** matched **+3**;
md5-candidate **221 → 221** (file has 1 stub → not md5-candidate). Decomposed the `func_8004DE60` 9-fn
main pack (cluster A) and banked the per-slot best-fit heap allocator over `Slot D_800DC6E0[]` as clean
C: `heap_get_largest_free` (accessor → `.unk_14`), `heap_alloc` (best-fit, block-split, `0x12345678`
guard), `heap_free` (coalescing). **Two compiler-source cracks:** (1) `heap_alloc`'s 3-way
`{best_rem,best,need}`↔`{s0,s1,s2}` permutation — the permuter plateaued (1595→605), but the
`#loop-weight-and-live-length-regalloc-steering` **Axis-3** lever (rename param `size`→`need`, mutate
**in place** so local-alloc stops parking the call-crossing local in `$s0`) closed it 119/119; (2)
`func_8004E184`'s dead `sw v0,0(sp)` = a GCC **nested-function static-chain spill** (new
`#nested-function-static-chain-spill` hazard) → it belongs in `func_8004E1E0`'s TU, **carried** (see
`## Carry-overs`). Quality 0/1/1/0. Retro applied **7 of 7** (new nested-fn hazard + Axis-3 regalloc
lever + `-dg` diagnostic + permuter b64literal fix + `#capturing-ra` re-confirm + 2 CLAUDE index rows).
Cross-repo follow-up: 3 new names (`heap_get_largest_free`/`heap_alloc`/`heap_free`) →
`sync_decomp_names.py --import-from-decomp`. **Next natural slice:** cluster B `[0x295E0]`
(`func_8004E1E0` + its nested `func_8004E184` + the 4 remaining pack fns).

**S175 CARRIED (retry of the S174 `func_8004DC44` carry; PO-approved bounded new-angle try) — 0 banks;
new project-best + `register` keyword ruled out.** matched **+0**; md5-candidate **221 → 221** (file still
5/6 mixed-partial, 1 stub). Two genuinely-new probes, both advance the characterization, neither matches.
**(1) Permuter-reseed from the frame-bearing best** (S173/S174 always seeded from the frameless vA):
broke the 3-sprint-stuck 14450 floor → `13530 → 13235`. The 13530 candidate is structurally identical to
the ROM (frame + schedule + all ops match); every remaining diff is a register name cascading from the one
`/40` dividend-register choice (zero op/shape diffs) — the tightest documented near-match. **(2) Plain
`register` keyword ruled out DEFINITIVELY** (PO /systematic-debugging): controlled A/B `.text`
byte-identical + both permuter-score 16613 (via new `tools/pscore.py`); 3 mips-gcc-2.7.2 subagents proved
`REG_USERVAR_P` is absent from `local-alloc.c`/`global.c` priority and `DECL_REGISTER` ignored at -O2. The
`register asm("$2")` hack from the frame-correct base is *worse* (51 diffs; quotient→`$v1`, ROM wants fresh
`$a3`) → confirms a coordinated `{dividend-$v0, quotient-$a3-fresh, dead-frame}` alloc. seed 3 / banked 0pt
/ realized 6; regime classical. Quality **1/1/1/0**. Retro applied **3 of 3** (2 DOC + 1 TOOL): #1
permuter-reseed doctrine (`#compiler-source-fan-out-escalation-above-the-permuter`); #2 `register`-keyword
no-op note (`#signed-divide-const-v0v1-quotient-destination`); #3 new `tools/pscore.py` single-candidate
scorer. Carry kept RETRYABLE (needs a new mechanism; permuter-reseed + `register` now spent). **Next
natural slice:** a FRESH main-segment pack (e.g. `func_8004DE60` 9fn, `func_8004D190` 7fn) — stop
re-attempting func_8004DC44 without a genuinely new lever.

**S174 CARRIED (retry of the S173 `func_8004DC44` carry; PO-directed cross-project + coalescing dive) — 0
banks; the S172/S173 "irreducible" verdict was CORRECTED.** matched **+0**; md5-candidate **221 → 221**
(file still 5/6 mixed-partial, 1 stub). Another pure-investigation sprint, but the deliverable is a
verdict correction: **the `v0/v1` divide-swap is FLIPPABLE-IN-ISOLATION** (`return D_800DC6D0/40`
reproduces the ROM's `/40` bytes exactly), not "irreducible." An 8-project cross-decomp sweep (all KMC
gcc 2.7.2: marioparty1/2/3, snowboardkids2, drmario64, hm64, puzzleleague64) + ~12 compiler-source
subagents + RTL dumps pinned the mechanism: dividend→`$v0` needs a physical **reg-2 SET** (return/call
copy) that lands the chain in local-alloc's suggestion pass; `func_8004DC44`, a **void/callless/
returnless leaf whose quotient feeds arithmetic then a loop-carried store**, emits no reg-2 mention →
deterministically magic→`$v0`. Every faithful lever failed *in that context* (24 control-flow × src
combos, multi-term dividends, all associativity, sched1 lifetime, memcpy, m2c); `register asm("$2")`
forces it (28 diffs) but is unfaithful + incomplete. seed 3 / banked 0pt / realized 6; regime classical.
Quality **1/1/1/1**. Retro applied **3 of 3** (all DOC): #1 correct `#dead-frame-reload-artifact-regalloc-wall`
framing; #2 new `#signed-divide-const-v0v1-quotient-destination` playbook; #3 cross-project matched-corpus
mining in `#compiler-source-fan-out-escalation-above-the-permuter`. Carry kept RETRYABLE (needs a new
mechanism, not another same-fn dive — ~430k iters exhausted). **Next natural slice:** a FRESH main-segment
pack (e.g. `func_8004DE60` 9fn, `func_8004D190` 7fn) for reliable banks — stop re-attempting func_8004DC44
without a new lever.

**S173 CARRIED (retry of the S172 `func_8004DC44` carry; PO-directed compiler-source dive) — 0 banks.**
matched **+0**; md5-candidate **221 → 221** (file still 5/6 mixed-partial, 1 stub); asm/c-subsegs
unchanged. A pure-investigation sprint: a 4-subagent GCC-2.7.2/binutils-2.6 dive (before the permuter)
**dump-verified** that `func_8004DC44` is an un-source-reachable compiler-artifact regalloc wall. Closed
the two S172 open questions: (1) the dead frame IS reachable (structured loops + a frame-bearing permuter
candidate — retract "no source trigger for the frame"); (2) the true wall is the `v0/v1` swap in the
signed-`/40` (`expmed.c` fixed operand/pseudo order + `local-alloc.c` life-dominated priority: tiny-life
magic 6666 vs dividend 1666 → magic grabs `$v0`), un-flippable across ~35 variants + 275k permuter iters.
Produced an **improved seed** (pre-declared base-pointer vars → base-hoist in a goto-loop; ops now 100%
match — the S172 seed lacked this). seed 3 / banked 0pt / realized 6 (residual +3); regime classical.
Quality **1/1/1/0**. Retro applied **3 of 3** (all DOC): #1 enrich `#dead-frame-reload-artifact-regalloc-wall`
(frame reachable + v0/v1 divide-swap), #2 `#top-tested-loop-goto-local-hoist` base-pointer-var lever,
#3 `#compiler-source-fan-out-escalation-above-the-permuter` dead-frame application. Full analysis + seed in
`docs/wip/func_8004DC44.wip.md`. **Next natural slice:** a FRESH main-segment pack (e.g. `func_8004DE60`
9fn, `func_8004D190` 7fn) for reliable banks, OR the head `[0x28590]` (7 fns) / tail `[0x29260]` (9 debug fns).

**S172 MIXED-PARTIAL (retry of the S171 carry) — `src/main/print_string_at_grid.c`; `print_string_at_grid`
BANKED byte-exact C, `func_8004DC44` still CARRIED.** matched **+1**; md5-candidate **221 → 221** (file
still mixed-partial, 1 `INCLUDE_ASM` stub); asm subsegs **77**, c-subsegs **218** (unchanged). The
S171-rated HARDER carry fell to a **compiler-source insight**: the `if(dst<base){dst++;continue;}` guard
that GCC 2.7.2 cross-jumped into the bottom `dst++;j loop` tail matched the ROM's branch-likely `bnezl`
(annulled-delay `dst++`) once re-expressed as nested `if(dst>=base){ if(dst<end)*dst=f|c; dst++; } else
{ dst++; }` (NEW `#cross-jump-tail-merge` classical-loop lever), and the residual `{f,base,dst,end,c}`
5-cycle closed via **lazy global-base load** (reference `D_800DAF60[...]` directly, no `u8 *base` local).
25/25 word-exact, full-make ROM SHA-1 == baserom. `func_8004DC44` remains a **dead-frame reload wall**
(structure driven fully identical via the NEW outer-goto magic-de-hoist, but locked on a reserved dead
8-byte spill frame + its driven reg permutation; 31k-iter permuter no zero). seed 3 / banked 0pt
(per-file all-or-nothing, still partial) / realized 6 (residual +3); regime classical. Quality **0/1/1/0**.
Retro applied **4 of 4** (all DOC): nested-if branch-likely + lazy-base levers (`#cross-jump-tail-merge`),
outer-goto SELECTIVE-hoist case (`#top-tested-loop-goto-local-hoist`), new
`#dead-frame-reload-artifact-regalloc-wall` section (+3 hazard-index rows). **Cross-repo follow-up:** none.
See `## Carry-overs` for `func_8004DC44` (the lone remaining stub). **Next natural slice:** finish
`func_8004DC44` (permuter/game-source), or the head `[0x28590]` (7 fns) / tail `[0x29260]` (9 FP-free debug fns).

**S171 MIXED-PARTIAL — `src/main/print_string_at_grid.c` (6-fn grid-print/debug cluster `[0x28DC0]`,
decomposed from the 13-fn FP-free debug subseg `[0x28590]` at 16-aligned `0x8004D9C0`; 4 of 6 banked).**
matched **+4**; md5-candidate **221 → 221** (file mixed-partial, 2 `INCLUDE_ASM` stubs); asm subsegs **77**
(the split kept the 7-fn head `[0x28590]` asm); c-subsegs **217 → 218**. The whole cluster references
placed `D_`/`flag` externs (NO rodata carve), so the tractable fns bank as C while the hard fns stay
`INCLUDE_ASM`, ROM green. BANKED byte-exact: `check_and_print_grid` (guard), `func_8004DA4C` (var-width
hex), `convert_and_print_hex` (8-digit hex; 1 setup-order fix), `func_8004DAF4` (scrollback console-puts;
the **`while(1)`-hoists-compiler-div-magic** lever — a goto-loop rematerialized `0x66666667`/`0x1B4E81B5`
/`' '` each iter, structured `while(1){…break}` hoisted them + stayed top-tested via the `*str` exit).
CARRIED (regalloc walls, permuters plateaued): `print_string_at_grid` (allocno permutation +
reorg-branch-tail-merge; `&base[i]` index-group + base-hoist levers landed but not sufficient) +
`func_8004DC44` (structurally IDENTICAL 75/75 opcodes via the **dual-IV offset+pointer** lever; residual
is pure register permutation + a dead 8-byte frame). seed 5 / banked 0pt (per-file all-or-nothing,
partial) / realized 9 (residual +4); regime classical/mixed. Quality **0/2/2/0**. Names: 3 curated +
3 auto `func_`. Retro applied **4 of 4** (all DOC): permuter post-flip hand-scaffold workaround +
`while(1)`-vs-goto compiler-constant hoist + `&base[i]` index-group + dual-IV levers (+2 hazard-index
rows). **Cross-repo follow-up:** none (3 curated names came from Ghidra; the 3 `func_` helpers —
`func_8004DAF4`=scrollback console-puts, `func_8004DC44`=scrollback→grid renderer, `func_8004DA4C`=min-
width hex printer — are optional later naming-pass candidates). See `## Carry-overs` for the 2 carried
fns. **Next natural slice:** the head `[0x28590]` (7 fns incl `func_8004D190` 808B), or the tail
`[0x29260]` (9 FP-free debug fns).

**S170 BANKED — `src/main/func_8004DD70.c` (3-fn slot-allocator module; the 16-aligned mid-slice
`[0x29170,0x29260)` decomposed from the 25-fn FP-free debug subseg `[0x28590]`).** matched **+3**;
md5-candidate **220 → 221**; asm subsegs **76 → 77** (the mid-slice decompose split off the `[0x29260]`
tail). A cohesive circular-doubly-linked-list slot module (`func_8004DD70` append / `func_8004DDE4` init
/ `func_8004DE44` get-total) over `Slot D_800DC6E0[]` (stride 0x18) matched **byte-exact, no permuter,
no carry** via 2 documented struct-array-of-BSS reloc levers: (1) DIRECT `D_800DC6E0[i].field` indexing
(NOT a `Slot *s=&arr[i]` base-pointer var → per-field `%hi/%lo` re-derivation matching the ROM's
per-symbol relocs); (2) the lone `prev=s` self-store through a pointer var (`sw v1,0xC(v1)`), the rest
direct. Diagnosis rabbit-hole: a +2-instr overflow in `func_8004DDE4` pushed object `.text` `0xF0→0x100`,
overflowed the 240B slice, and floated every `D_800DC6xx` bss symbol +0x10 (the long-text mirror of
`#short-text-shifts-flowing-bss`; root cause was the `.text` length, not the symbols). seed 3 / **banked
3pt** / realized 4 (residual +1). Quality **0/0/0/0**. Names kept `func_`/`D_` (PO: auto). Retro applied
3 of 3 (all DOC; the decomp_loop `--target-s` code change deferred to a golden-gated tooling branch): new
`#struct-array-of-bss-direct-index-vs-base-pointer-var`, `#short-text-shifts-flowing-bss` long-overflow
variant, decomp_loop post-flip `find_segment` gap note. No carry-over. **Cross-repo follow-up:** none
(auto names). **Next natural slice:** the `[0x29260]` tail (16 more FP-free debug fns incl the
`print_string_at_grid` grid-print cluster + the `osSetIntMask`/`osSyncPrintf` logging helpers).

**S169 MIXED-PARTIAL — `src/main/func_80076640.c` (3-fn one-tu S162 tail `[0x51A40]`; `func_80076778`
BANKED C, 2 carried).** matched **+1** (`func_80076778`, a point×matrix transform); md5-candidate
**220 → 220** (file mixed-partial, 2 `INCLUDE_ASM` stubs); asm subsegs **77 → 76** (the flip). The
PO-picked main-segment one-tu tail was FP-monster-dominated, as the plan hedged. Banked +1 via the
**one-tu partial-bank pattern extended to shared rodata**: the matched fn is C, the two hard fns stay
`INCLUDE_ASM`, and the TU's shared `ACAD0` rodata (FP doubles + printf strings) is referenced `extern`
by all (no `.rodata` carve, so a one-tu is not strictly atomic-or-nothing). `func_80076640`
(2-rotation-matrix builder) CARRIED at score 25 on the new `#abs-coalescing-reg-swap` wall (permuter
PLATEAUED 338k iters); `func_8007680C` (679i FP camera-frustum-bound) CARRIED S158-class (structure
complete 721/756, but an 8-byte frame cascade + deep pressure-driven regalloc). Two real levers landed
on func_80076640: `const`-extern doubles → callee-saved-reg 1-load cross-call CSE, and the permuter's
`do{}while(0)`+`cosf`-temp fixed the count 77→78. seed 13 / banked 0pt (per-file all-or-nothing,
partial); realized 17 (residual +4); regime classical. Quality **1/1/2/0**. Retro applied **4 of 4**
(PO accept-partial; all DOC): const-extern inverse lever (`#volatile-global-tell`) + `do{}while(0)`
schedule lever + mixed-partial one-tu-shared-rodata generalization (`CLAUDE.md`) + new
`#abs-coalescing-reg-swap`. Both carried fns compile in-tree; WIP `docs/wip/func_80076640.3fn-wip.c.txt`,
permuter scaffold `nonmatchings/func_80076640/`. **Cross-repo follow-up:** none (auto `func_` name);
optional later: curate a Ghidra name for `func_80076778` (a clear transform-point-by-matrix helper).

**S168 BANKED — `src/main/func_80071220.c` (scenario-unlock dispatcher; the `[0x4C620]` 1-fn tail split
from `[0x4C3D0]` at S167, the "next natural slice").** matched **+1**; md5-candidate **219 → 220**; asm
subsegs **78 → 77**. A single classical game fn (scenario completion/unlock) matched **byte-exact via a
4-lever documented-hazard stack, no permuter, no carry** (score 8540 → 0): (1) loop vars declared after
the last pre-loop call keep 3 saved regs not 5; (2) `#indexed-vs-pointer` indexed form `table[i]` for the
`move v1,a0` dual-IV; (3) index a pointer **variable** (`Entry *table = D_801B7118`), not the array symbol,
to fold the base into the giv (new sub-lever); (4) a `switch(sel)` for the 4-way dispatch (branch-toward,
per-case distinct tails). The S167 struct model (`func_8005AF50()` base + `Entry{s8 tag@0; u8 flags@5}` /
`ScenarioRec` 0x74 @ base+0xE98) carried straight over. Seed 5 / **banked 5pt** / realized 6 (residual +1:
the one novel gotcha below). Quality **0/0/0/0**. Retro applied 3 of 3 (all doc/review-gate; PO away → best
judgment): new `#stale-parent-asm-relic` hazard, `#goto-dispatch` switch-ladder note, `#indexed-vs-pointer`
pointer-var sub-lever. No carry-over. **Cross-repo follow-up:** none (auto `func_` name; Ghidra has no
curated name at 0x80071220 — a later naming pass can add one once the surrounding callees are understood).

**S167 PARTIAL — `src/main/func_80070FD0.c` (3-fn char/scenario stat-updater head, decomposed from
`[0x4C3D0]`; 2 of 3 banked, func_80070FD0 CARRIED).** matched **+2** (`func_800710C4`, `func_8007117C`
byte-exact C); md5-candidate 219→**219** (file MIXED-PARTIAL — 1 stub — so no flip); asm subsegs 78→78.
The 2 fell to the **per-function compiler-source fan-out** (3 opus subagents over mips-gcc-2.7.2 +
mips-binutils-2.6, one per fn, each fed the exact byte-diff): the unifying key was the `func_8005AF50()`
return being a game-save **struct** (`SaveBlock{u8 pad[0xF4]; s8 tbl[6][0x12]; u8 wins[13][0x12][2]}` @
base990), so `base->tbl[i][j]` (MEM_IN_STRUCT COMPONENT_REF) keeps `+0xf4` explicit + base in `$a0`;
plus biv-elimination (`p[j]` index -> `end=start+0x12`), `s32`-load for `lb`, `i`-before-`rp` init.
`func_80070FD0` CARRIED at a **3-word branch-direction miss** on the new `#cse-make-regs-eqv-branch-fold`
wall (default `old=t` copy makes old canonical -> cse folds t; bnez+separate-t unreachable from
equivalent C; proven by 35 variants + 43k permuter iters + a full RTL-dump trace). Needs game-source
insight, not more permuter. Split `[0x4C3D0]` at 16-aligned `0x4C620`; 1-fn tail func_80071220 stays
asm. seed 5 / banked 0pt (per-file all-or-nothing, partial); realized ~13 (S158-class cse/regalloc
slog); regime classical. Quality 0/1/1/0. Retro applied **4 of 4**: fan-out escalation doc + new
cse-fold hazard (+ CLAUDE.md index) + main-profile permuter recipe + regalloc-heavy pts extension; also
PRUNED 2 stale carry-overs (motor.c S102, sched.c S106). **Cross-repo follow-up:** name
`func_800710C4`/`func_8007117C` + push the `SaveBlock` struct to the Ghidra workspace.

**S166 PARTIAL — `src/main/lz_decompress_simple.c` (retry the 2 carried LZ fns; `lz_decompress_simple`
MATCHED, `lz_decompress_extended` CARRIED).** matched **+1** (simple); md5-candidate 219→**219** (file
still MIXED-PARTIAL — 2/3 fns C, 1 stub — so no md5-candidate flip); asm subsegs 78→78. **The project's
single hardest fn CRACKED byte-exact** (`lz_decompress_simple`, 8600→0, carried S164/S165 and declared
"irreducible" 3×): the crack was the **loop-weight lever** (new `#loop-weight-and-live-length-regalloc-
steering`) — re-grounding root cause in `flow.c:2067` showed that structuring ONLY the inner decode-
dispatch `do{…}while(1)` (keeping the outer a goto-loop) weights `control`'s refs ×2 asymmetrically so
it wins `$a2` = target layout. Struct unified to `LzDecompressState` (0x28) preserving the S165 dma
match. `lz_decompress_extended` CARRIED at greg-proven floor raw-185 (two coupled near-tied 3-cycles,
dual-confirmed a genuine live-length+ring-ref conflict; see `## Carry-overs`). Post-sprint (PO
systematic-debugging): reworked simple to the cleanest byte-matching form (names+comments, control flow
unchanged) and proved zero-goto impossible (3-agent source fan-out, CS1/CS2/CS3 file:line-cited). seed
8 / banked 0pt (per-file all-or-nothing, partial); regime classical. Quality 0/1/1/0. Retro applied 4
of 4 (2 new hazards + triage upgrade + rework commit). **Cross-repo follow-up:** push corrected
`LzDecompressState` to Ghidra.

**S163 BANKED — `src/main/func_80043AF0.c` (4-fn club-meter head, DECOMPOSED from the 25-fn `[0x1EEF0]`
subseg).** md5-candidate 218→**219**; matched +4; asm subsegs 78→78. `func_80043AF0` (club-table ptr
`&D_800CABD0[i*24]`) + `func_80043B0C` (splat auto-decompiled the no-op) matched first build. The two
club-meter fns were permuter-PLATEAU classical walls cracked by the new **compiler-source fan-out
escalation ABOVE the permuter** (`docs/hazards.md#compiler-source-fan-out-escalation-above-the-permuter`):
`get_club_meter_extent` needed a **goto-dispatch to out-of-line bodies, load-case last**
(`#goto-dispatch-branch-toward-vs-branchless`; gotoless proven impossible), `get_club_meter_units`
needed **one reused var in both if/else arms** to force the `$a0` allocation
(`#call-result-a0-vs-v0-single-allocno`). Split `[0x1EEF0]` at 16-aligned `0x1F020`; 21-fn tail stays
asm. No carry-overs. **Tooling follow-up (S163, PO-pick #3, deferred to a golden-gated tooling branch,
NOT a review-gate edit):** promote the isolated-scoring harness pattern (scratchpad
`score_extent.py`/`score_units.py`: prepend typedefs, compile the exact `src/main` -O2 profile
standalone, objdump, normalize branch/jal target addresses to a token, print an aligned TGT-vs-CAND
diff + a layout-shift-insensitive diff count) to `tools/`. It made the empirical structural search
tractable for the double compiler-wall (fast, in-tree-equivalent, target-shift-insensitive) and pairs
with `decomp_loop` for classical BB-layout/regalloc/scheduling near-misses where asm-differ's score is
dominated by cascade/target-shift noise. **Why a branch:** a new measurement surface feeds the loop, so
golden-gate it (byte-identical harness output on a fixed corpus) with reassess checkpoints per the
tooling-refactor discipline.

**S162 BANKED — `src/main/func_80076500.c` (3-fn debug-Vec3f-buffer head, DECOMPOSED from the 6-fn
`[0x51900]` subseg — main-segment classical endgame).** `func_80076500` = copy two `Vec3f` global
constants (`D_80105B6C`/`D_80105F30`) into two output buffers; `func_8007654C` = reset counter
`D_800E1CD0 = 0`; `func_80076558` = push a `Vec3f`+scalar into slot `i` of the debug buffer
`D_800E1C50[8]`/`D_800E1CB0[8]`, optional `osSyncPrintf` under `D_800FBDA6 & 4`, increment. **All 3
ROM-byte-exact WITHOUT the permuter, cracked by ONE lever: MEM_IN_STRUCT source typing (gcc 2.7.2
`sched.c` `true_dependence`, PO struct hint).** Both hard fns diverged at -O2 as "compiler optimizes
more than the ROM": `func_80076500` pipelined 6 global-load/pointer-store pairs into `$f0/$f2/$f4`
(ROM strict `$f0`); `func_80076558` hoisted the flag load past the store + materialized/reused array
pointers (ROM: late load + `nop`, folded fresh recompute). Fix = retype the fixed globals as struct/
array members (`sched.c:797`: a MEM_IN_STRUCT ref at a varying addr never conflicts with a non-struct
ref at a fixed addr, so a scalar-global load is judged independent of a pointer store → serialize/
late-load once it becomes MEM_IN_STRUCT): two `Vec3f` constants → strict pairs; `D_800E1C50` array-of-
`Vec3f` (C54/C58 = `.y/.z` field addrs, stride 12) → folded addressing; `D_800FBDA6[0]` → late flag
load → exact 58-instr match. New hazards `#mem-in-struct-scheduling-lever` + `#short-text-shifts-
flowing-bss` (the length-miss-shifts-the-sibling's-.bss diagnostic that unlocked it). Split `[0x51900]`
at 16-aligned `0x51A40`; 3-fn tail (`func_80076640` `guRotateF`/`guMtxCatF` matrix code) stays asm.
`Vec3f*` args (PO-directed, byte-identical). md5-candidate **217→218** (+1); matched +3; asm subsegs
**78→78** (split adds the `[0x51A40]` tail; net unchanged). Quality **0/0/0/0**. seed 5 / realized 7 /
residual +2; regime classical. Retro applied **3 of 4 PO-picks** (PO away, best judgment): 2 new
hazards + a CLAUDE.md Iterate pointer; #3 pts data point folded into VELOCITY. **Cross-repo follow-up:**
3 fns kept `func_` (Ghidra had only `_NON_MATCHING`); now matched, a follow-up can clear the tag.
**Next natural slice:** the `[0x51A40]` 3-fn matrix/DL tail.

**S161 BANKED — `src/main/func_800521C0.c` (3-fn scenario-flag get/set head, DECOMPOSED from the 12-fn
`[0x2D5C0]` subseg — main-segment classical endgame).** `func_800521C0` = branchless flag getter
`-((D_801B60AC & 2) != 0) & 9`; `func_800521DC` = `flag_is_set(0x3D)?2 : (D_801B60AC & 6)?9 : 0x12`;
`func_80052220` = `func_800521C0() + func_800521DC() - 1`. All 3 tiny fns (<10 instrs) matched on the
FIRST build via the **asm-first fast-path** (S148, no MCP) — verbatim first-try, no permuter. Split
`[0x2D5C0]` at the 16-aligned `0x2D650` (head emits no rodata → decomposition-safe); the `[0x2D650]`
9-fn tail stays asm (natural next slice, surfaced via the new `--segment main`). md5-candidate 215→216;
asm subsegs 78→78 (split adds the tail). Retro applied **3 of 3**: `pick_target.py --segment main`
vram-range filter, `/sprint-review` carry-over-hygiene prune (+ pruned the stale S156 jtbl spike banked
S159), `game-embedded` mirror sort de-rank; goldens regen'd (4). Quality 0/0/0/0. No carry-over.
seed 3; realized 2; residual −1 (first sub-5 classical).

**S160 BANKED — `src/main/func_8006EA90.c` (3-fn one-tu classical slice, whole `[0x49E90]` subseg — FIRST
bank of the pure classical asm-flip endgame).** `func_8006EA90` = putter/physics byte-flag table setup
(0-jal, FP: outer4/inner3 dup-store `*0.3f` loop + three `*0.35f` signed byte blocks + three
`(u32)(f64)*0.8` unsigned-cast blocks); `func_8006ED2C` = no-op; `func_8006ED34` = two-texture
fog/scroll screen-filter DISPLAY-LIST builder (1 jal `emit_per_phase_fog_state`). md5-candidate 215→216;
asm subsegs 79→78. `func_8006EA90` needed the **permuter** for a prologue instruction-GROUP schedule swap
(structure was 165/165 with identical reg-alloc; grounded in `sched.c rank_for_schedule` LUID tiebreak,
run without `--best-only` — the equal-score plateau); FP-double pool (0.8/2^31 ×3, NOT 0.3) carved to a
`.rodata` sibling. `func_8006ED34` matched via the `gDPxxx(dl++)` **post-increment** idiom, then
PO-directed refine to idiomatic gbi macros (composite `gDPLoadTextureBlock`/`gDPLoadMultiBlock`) decoded
with gfxdis.f3dex2. Retro applied 4 of 4: `lib.sh` grep-bug fix, `#display-lists` addendum,
`#permuter-setup` sub-0.97 scheduling extension, new `tools/fpdecode.py`. Quality 0/1/0/0. No carry-over.

**S159 BANKED — `src/main/func_80051E90.c` (2-fn one-tu jtbl-pair, whole `[0x2D290]` subseg — the
S156 jtbl-pair remainder).** `func_80051E90` = `switch(course)` over the 8-entry compiler jump table
`jtbl_800CCC30` + a sparse per-case `if`-chain on `hole` returning golf-yardage constants (default
200, cases 6/7→30); `func_80051FCC` = scenario/terrain config accessor (`D_801B608C==9` table lookup
/ `g_terrain!=0` signed-`%3`-magic 0x55555556 / else `sm<12` clamp-to-8). Both jal-free. **MILESTONE:
this was the LAST c-stub file — every `src/*.c` is now fully-C (md5-candidate 215/215); the c-stub
backlog is drained and all remaining work is the 79 un-flipped asm subsegs.** Matched with **4
codegen nudges, no permuter**, all grounded in the KMC gcc 2.7.2 source per PO directive: new
`docs/hazards.md#switch-jtbl-dispatch` (switch-for-jtbl-only + if-chains for sparse inner cases;
`a==K1||a==K2` → two `if`s for the short-circuit + cross-jump; `.rodata` sibling carve of the switch
table — first carve of a compiler jump table) + two `#register-reuse-nudge` variants (index-add
operand order → `expr.c:5248-5290 both_summands`; branch-LIKELY `beqzl` on a coalesced return-var →
INVERT the branch, `reorg.c:1141-1211 optimize_skip`). md5-candidate 214→**215** (+1); +2 matched; asm
subsegs 79→79 (the flip was the gate action; +1 rodata carve). Quality 0/0/0/0; seed 5 / realized 5 /
residual 0 (2nd post-S155 sub-13 classical data point after S156, validating the recalibrated rubric).
Names kept `func_` (Ghidra had none). No carry-overs.

**S157 BANKED — `src/main/func_80025D30.c` (9-fn integer overlay/moduleset load-unload module,
whole `[0x1130]` subseg).** The overlay loader over a 0x28-byte `OverlayDesc[]` table (`D_800B5F58`):
`load_overlay`/`unload_overlay` DMA an overlay's text from ROM, zero its BSS, flush caches, run its
ctor/dtor, and claim/free its segment slots; `func_80025F18` rejects a load that would overlap a
resident overlay or reuse a live segment; `func_80025D8C` rebuilds the seg→overlay map; the rest are
id-normalize/getter/debug helpers. Pure INTEGER, no local rodata, all externs placed. **8-gate FIRED
(seed 13, 9-fn); PO approved run-WHOLE** — the sole 16-aligned inner boundary was 0x1630 (→7+2, no
clean 3-4 slice), and the module banks atomically. **6/9 fns cracked by grounding in the KMC gcc 2.7.2
source** (`global.c` `allocno_compare` → the s0/s1/s2 register rotation, fixed by reusing the
`overlayId` param for the index; mixed signed/unsigned range check; single-counter + CSE + `(u32)`
bound). **The 3 hardest (`func_80025F18`/`load_overlay`/`unload_overlay`) were 1-instruction
scheduling/hoisting misses that resisted manual variation AND the permuter (250k+ iters flat); cracked
by the ORIGINAL loop STRUCTURE (PO reference impl): INDEXED segment walk (`seg[i]` + a value temp) not
pointer-increment, and F18's 3 `continue` guards folded into one `||`.** WHY is grounded in
`docs/hazards.md#indexed-vs-pointer-loop-strength-reduction` (move_movables@loop.c:966 before
strength_reduce@976 → a giv pointer-init emits after the hoisted loop constants → reorg fills the
entry-`beq` delay with the constant; a pointer biv-init emits first → the move fills the delay, miss).
md5-candidate 212→213 (+1); +9 matched; asm subsegs 81→81 (flip was a gate action). Quality 0/3/0/0
(3 permuter-escalated, all resolved by the reference not the permuter); seed 13 / realized 16 /
residual +3. Names kept `func_` (Ghidra had none). **Global-rename follow-up:** `D_800B5F58`→`overlays`
and `D_800B67C0`→`debug_mode` are the reference's clear names, but `D_800B67C0` is SHARED by
`func_80052070.c` + `libnusys/nusched.c`, so the rename must update all consumers in lockstep
(symbol_addrs add-only + `make extract`) — deferred as a cross-repo naming pass, not a single-file edit.

**S158 BANKED — `src/main/func_80067D40.c` (5-fn one-tu golf-tournament module,
whole `[0x43140]` subseg).** RNG pair (`func_80067D40` set / `func_80067D4C` LCG-next on a 2nd seed,
mult 0x5D588B65) + a 226-instr CPU-field score generator (`func_80067D74`: per-entry mod-arith + the
LCG + `sin(hole·π/18)`) + a 137-instr leaderboard build/sort/rank (`func_800680FC`, sort via
`func_8005B150`) + a 76-instr scenario name-table builder (`func_80068308`). All 3 non-trivial fns
were STRUCTURALLY correct first-pass but locked on **PERVASIVE register allocation** — the hardest
classical class, cracked by a **3-round multi-agent worktree fan-out + decomp-permuter** and a new
playbook `docs/hazards.md#pervasive-regalloc-classical-main`: exact-symbol per-field structs (no
reloc-addend false floor) + ref-count/live-range levers (DECL ORDER IS INERT for KMC gcc 2.7.2) +
boosted permuter. Load-bearing levers: **param-reuse** (clamp `arg0` in place, no `base`/`seq`),
**`s32` return type** (reserves v0; `#return-type-is-load-bearing`), **per-field structs** (combined
offset-folding changes scheduling; `#struct-access-folding-changes-scheduling`), align-2 struct copy,
const-pointer LICM-alias defeat, live-range trim. md5-candidate 213→214 (+1); +5 matched; asm subsegs
unchanged (flip was a gate action). Quality 0/3/0/0 (3 permuter-escalated); seed 13 / realized 18 /
residual +5 (a seed-13 one-tu massively under-priced — pervasive-regalloc difficulty not captured by
size/nfns). Committed the main-profile permuter tooling (`tools/permuter_settings_main.toml` +
`tools/kmc_main_prelude.inc`). **Cross-repo naming follow-up:** curate the 5 fns (RNG pair /
score-gen / leaderboard / scenario-table) + data syms (2nd RNG seed `D_800C3600`, per-hole table
`D_800C3604`, 30-entry ScoreEntry array `D_801B7118`, scenario table `D_80132CB0`) → decomp names +
`sync_decomp_names.py --import-from-decomp`. Kept `func_`/generic `D_` (Ghidra had none).

**S156 BANKED — `src/main/func_80052070.c` (4-fn jal-free scenario/terrain config-accessor slice,
carved off the `func_80051E90` jtbl pair).** The smallest main-segment classical candidate
`func_80051E90` (6-fn pack, pts-13) carries a rodata jump table (`jtbl_800CCC30`) only in its first
2 fns; the LOWER 4 (`func_80052070`/`func_800520DC`/`func_80052100`/`func_80052168`) are jal-free with
no local rodata, so they carved cleanly at the 16-aligned `0x2D470` boundary as a seed-5 sub-slice
(the 8-gate DECOMPOSE via the new `docs/hazards.md#multi-function-segment-splitting-pack` jal-free
sub-slice strategy), leaving the jtbl pair asm. All 4 are terrain/scenario config getters over extern
game globals; seeded asm-first (Ghidra MCP returned stale `_NON_MATCHING` bodies). Two codegen nudges,
no permuter: invert-the-guard regalloc coloring + array-index `+` operand-order (both now documented
in `#register-reuse-nudge`). md5-candidate 210→211 (+1); +4 matched; asm subsegs 82→82 (the
`[0x2D290,asm]` jtbl-pair remainder stays). Names kept `func_` (Ghidra had none). **First post-S155
decomposed classical slice priced BELOW the old flat-13 (seed 5), validating the recalibrated rubric.**
**Tracked tooling follow-up (S156):** teach `pick_target.py` to auto-detect a pack's carve-able
sub-slice — a CONTIGUOUS run of members all (a) `jal`-free AND (b) referencing no TU-local rodata
(`rodata-jtbl`/`straddle`/`literal` absent on those members; an already-placed jtbl/table is an
extern, not a blocker), bounded by 16-aligned ROM offsets — and emit an advisory
`subslice-carve:<lo>-<hi>@<pts>` flag with the sub-slice's own seed. Data is available (`_asm_jal_count`
per member + member addrs); needs a new hazard kind + factory + render format + score-neutral
integration + hazard-index row + golden regen, so it runs OFF-CADENCE on a golden-gated branch
(`make test-tools`, `REGEN_GOLDEN=1`) per the tooling-refactor policy, not at a review gate. Until
then the gate applies the strategy MANUALLY by reading the pack's `.s`. Also a pts data point: a
jal-free accessor sub-slice realized 5 (the recalibrated ranker priced it 5, not the old 13).

**S154 BANKED — `src/main/func_8006A000.c` (math/RNG one-tu now **8/8 fns** — banked the `[0x455C0]`
3-fn tail AND RECOMBINED the S152/S153 3+2+3 decomposition into one file).** `calc_vec3_magnitude`
(`sqrtf(x²+y²+z²)`, bare single `sqrt.s`, 3rd arg z in GPR `$a2`), `crc16_ccitt` (was `func_8006A1EC`;
CRC-16/X.25, poly 0x8408), `report_div_error` (was `func_8006A274`; log `$ra` + `(s32)(1.0f/D_800C3FB4)`).
Deleted `func_8006A180.c` + `func_8006A1C0.c`; text `[0x45580]`/`[0x455C0]` merged into `[0x45400]`;
`-ffast-math` consolidated to `func_8006A000.o`. **Three codegen gotchas:** (1) `crc16_ccitt`'s bit-loop
needed a **goto** — `loop.c` `check_dbra_loop` REVERSES any count-only loop from 0, but the ROM up-counts
(both natural forms verified to reverse first, PO "goto = last resort"); the outer loop matched as a
natural indexed `for` (strength reduction → `addu a1,a1,a0`). (2) `report_div_error` captures `$ra` via a
`volatile` inline-asm barrier (`__builtin_return_address(0)` broken here). (3) **strings kept as ACTUAL
LITERALS via the TU-recombine** (PO directive): a rodata carve can't 8-align the next-TU double because
`OBJCOPY_ALIGN` force-4-aligns every ASM rodata — one `.c` lets func_8006A000's 16-aligned `2^31` doubles
pad the section tail to `0x800D1440` (new `docs/hazards.md#decomposed-one-tu-rodata-alignment-split`, a
counter-case to the 8-point decompose gate). md5-candidate **212→211** (merge consolidation, +3 matched,
all 211 C files fully-C). Quality **0/0/0/0**. seed 13 / realized 13 / residual 0; regime classical.
Retro applied 4 of 4 PO-picks (#1 rodata-split hazard + index; #2 `#top-tested-loop-goto-local-hoist`
`check_dbra_loop` corollary + index; #3 `#capturing-ra-return-address-as-a-call-argument` + index; #4
pts-recalibration 6th data point). **Cross-repo follow-up:** `crc16_ccitt`/`report_div_error` →
`sync_decomp_names.py --import-from-decomp`. No carry-over (one-tu fully banked). **6th data point for the
pts-recalibration follow-up** (3-fn 256B none-upstream priced pts-13; also flags rodata-adjacency pricing).

**S153 BANKED — `src/main/func_8006A180.c` (2-fn head slice: `update_rng_seed` + `hypotf_2d`,
decomposed from the S152 `[0x45580]` remainder at the 16-aligned 0x8006A1C0).** `update_rng_seed`
(LCG `rng_seed = rng_seed*0x5D588B65 + 1`) + `hypotf_2d` (`sqrtf(a*a + b*b)`, bare single `sqrt.s`).
Both MATCHED first-build. The load-bearing unknown (bare single-precision `sqrt.s`) resolved by
**verifying against the KMC gcc 2.7.2 source, which REFUTED the S152 carry-note's "sqrtf needs a
separate intrinsic path, NOT -ffast-math" hypothesis:** `sqrtf` is `BUILT_IN_FSQRT` (`c-decl.c:3230`),
expanded by the SAME `expr.c:7243` path as double `sqrt` (mode-only diff → `sqrtsf2` `mips.md:1506`,
gated `mips_isa>=2`); without `-ffast-math` gcc emits `sqrt.s` + a `c.eq.s`/`bc1t` NaN guard + `jal sqrt`
fallback (proven by `align.o`), WITH it the bare `sqrt.s` the ROM has. So single-precision uses the SAME
per-file `-ffast-math` as S152 (`func_8006A180.o` in `mk/main.mk`). **8-gate FIRED (seed 13)** → small
classical pack exemption BOTH branches (a1 `<256B`=64B AND a2 `0-call`; inner boundary 0x8006A1A8
non-16-aligned = mechanically non-decomposable). md5-candidate **211→212**; matched +2. Quality
**0/0/0/0**. seed 13 / realized 13 / residual 0; regime classical. Retro applied 2 of 2 PO-picks (#1
`#double-sqrt-fast-math` rewritten mode-agnostic + CLAUDE.md index sync; #3 pts-recalibration 5th data
point; #2 tail carry-over bookkeeping). **Cross-repo follow-up:** none (both fns pre-named in
ghidra_symbols). Carry-over: the `[0x455C0]` 3-fn tail (see `## Carry-overs`). **5th data point for the
pts-recalibration follow-up** (a 64B 2-fn head slice priced pts-13 — the strongest under-weighting case).

**S152 BANKED — `src/main/func_8006A000.c` (3-fn FP vector-magnitude slice, decomposed from the
`[0x45400]` 8-fn math/RNG one-tu).** `vector_magnitude_safe` (3D `sqrt(x^2+y^2+z^2)` with overflow
range-scaling → u32), `calculate_hypotenuse_safe` (2D, same idiom), `set_rng_seed` (trivial `rng_seed`
setter). The 8-fn `[0x45400]` one-tu was DECOMPOSED at the 16-aligned 0x8006A180 boundary (8-gate
resolved by the decompose; the 5-fn remainder `[0x45580]` is carried, see `## Carry-overs`). All 3
matched ROM-byte-exact **without the permuter**, but the two magnitude fns took a systematic-debug dive
into the KMC gcc 2.7.2 source for **three interlocking codegen requirements**: (1) bare `sqrt.d` needs a
per-file `-ffast-math` override (`mk/main.mk`) — default emits `jal sqrt`, `#pragma intrinsic(sqrt)` a
guarded `sqrt.d`+`jal sqrt` NaN-fallback (new `docs/hazards.md#double-sqrt-fast-math`); (2) the top-tested
plain-branch loop needs `goto`s because `expand_end_loop` inverts every structured top-tested loop at -O2;
(3) the loop-invariant range bounds need to be LOCAL variables to hoist (since `loop.c` ignores
goto-loops) — new `docs/hazards.md#top-tested-loop-goto-local-hoist`. Rodata carve `[0xAC810,.rodata]` =
2 per-fn 2^31 doubles (0x800D1410/0x1418, no cross-fn dedup). md5-candidate **210→211**; matched +3.
Quality **0/0/0/0**. seed 13 / realized 15 / residual +2; regime classical. Retro applied all 4 PO-picks
(2 new hazards + 2 BACKLOG follow-ups). **Cross-repo follow-up:** `set_rng_seed` →
`sync_decomp_names.py --import-from-decomp` (the 2 magnitude fns were pre-named in ghidra_symbols).
**4th data point for the pts-recalibration follow-up** (a decomposed 384B 3-fn none-upstream subseg
re-priced pts-13, unchanged from the 8-fn pack).

**S151 BANKED — `src/main/func_800500E0.c` (3-fn Gfx display-list "wipe box" TU; the FIRST main-segment
game DISPLAY-LIST TU).** The one-tu `[0x2B4E0]` pack is a UI wipe/reveal system: `func_80050274`
registers a box into `D_80132370[]`/`D_800C0E50`, `func_8005029C` emits the shared XLU render setup
(`gDPPipeSync/SetCycleType/SetAlphaCompare/SetRenderMode/SetCombine`) via `gDPxxx(glistp++)` then draws
every registered box (`for(i != count)`) and clears the count, `func_800500E0` advances one box's wipe
(state 0 center-out open → 1 hold → 2 close → 3 done, over a 4-frame sub-timer) and emits its
`gDPSetPrimColor` + `gSPTextureRectangle`. All 3 matched, ROM SHA-1 == baserom. **New standing enabler
`mk/main.mk`** (`MAIN_CFLAGS = $(CFLAGS) -DF3DEX_GBI_2`): MG64 is F3DEX2, so game DL code needs the
F3DEX2 GBI opcodes — this REVISES the S148 "main/ needs zero mk edits" convention (a DL TU needs the
F3DEX2 profile). `func_800500E0` matched via the **permuter** (regalloc + phantom -16 frame near-miss;
the coord retype `u16 left; s16 right;` cracked it). `gfx_dl_write_cursor` renamed to `glistp` (demo
idiom). md5-candidate **209→210**; matched +3; asm subsegs 84→**83**. Quality 0/1/0/0; seed 13 /
realized 15 / residual +2; regime classical. Retro applied 4 PO-picks (3 doc groups + 1 tooling defer):
`docs/hazards.md#display-lists` DL-reconstruction workflow + mask-narrowing lesson; CLAUDE.md main-tree
F3DEX2 convention + `mk/main.mk`; `#permuter-setup-for-kmc-toolchain-mirrors` game-O2 recipe + coord-width
lever; CLAUDE.md Ghidra `strict_mode` bypass + `symbol_addrs`-rename recipe. **Cross-repo follow-up:** 3
function names (behavior understood) → `sync_decomp_names.py --import-from-decomp`; `glistp` already
propagated to Ghidra live. No carry-overs. **3rd data point for the pts-recalibration follow-up** (a
800B 3-fn one-tu classical pack priced pts-13, capped at the ceiling).

**S150 BANKED — `src/main/func_80029250.c` (`cfb_setup` + `cfb_set_num`, game-custom nusys CFB setup).**
The 2-fn one-tu `[0x4650]` main-segment pack is the game's customized nusys color-framebuffer
management: `cfb_setup` is the rewritten `nuGfxSetCfb` (drives a custom `D_800B67A4[]` advance-table /
`D_800B67A0` held-ptr rotation instead of stock's `nuGfxRetraceWait`/`nuScSetFrameBufferNum`),
`cfb_set_num` dynamically changes the active framebuffer count (widely called from gameplay/course/
overlay code). Pure global data-shuffle, **0 `jal`**. Asm-first seed; compiled clean but full-make
SHA-MISSED, converged in 2 codegen fixes (no permuter): `num==3` `nuGfxCfbNum` store-order, and an
else-branch `framebuf[2]` load-hoist (early temp). The **8-gate false-fired again** (496B pts-13,
one-tu decompose-blocked); PO ran it as-is and the retro GENERALIZED the small-pack exemption to a new
**(a2) 0-call size-agnostic** branch (`docs/agent-workflow.md ## Story points`). md5-candidate **208→209**; matched
+2; asm subsegs 85→**84**. Quality 0/0/0/0; seed 13 / realized 13 / residual 0; regime classical.
Retro applied 2 of 3 (#3 `docs/hazards.md#isolated-compile-caveat` scheduling-reorder inverse-trap +
`cmp`-localize recipe; #1 the (a2) exemption branch; #2 asm-first confirmation = no edit). **Cross-repo
follow-up:** `cfb_setup`/`cfb_set_num` → `sync_decomp_names.py --import-from-decomp` (Ghidra had `func_`
only). No carry-overs. **2nd data point for the pts-recalibration follow-up** (a tiny none-upstream
one-tu pack priced pts-13; the (a2) branch handles the gate symptom by-hand, the pricing fix is the
root).

**S149 BANKED — `src/libnusys/nuboot.c` (the game-embedded nusys boot: `nuBoot` + `idle`).** The 2-fn
`[0x748B0]` main-segment pack is the game's nusys `nuboot.c` (cart entry + idle thread). Asm-first
seed built first-try but the full-make SHA-MISSED: **root cause was the PROFILE, not the C** — the boot
file shipped at **-O0** (fp kept, no CSE, unused-arg spill), fixed with a file-specific -O0 mk override
(`mk/libnusys.mk`). Then 4 PO-directed refinements, all ROM byte-exact: CARVED `main/main` ->
`libnusys/nuboot` (nuboot is libnusys); RENAMED to official nusys names (`nuBoot`/`IdleThread`/
`MainThread`/`IdleStack`/`nuIdleFunc`, with splat `allow_duplicated:True` for the 2nd nusys instance);
nusys DEFINES + the stock `IdleStack + NU_IDLE_STACK_SIZE/8` stack calc; `osInitialize()` not
`__osInitialize_common()` — which exposed MG64's `os_host.h` macro shipped BACKWARDS vs ultralib (fixed
header + reverted `initialize.c` to VERSION_J). Closed with a macro-audit of all 91 libultra headers
vs the pin (`tools/audit_libultra_headers.py`): one more latent/unused bug found + fixed
(`rcp.h VI_CTRL_PIXEL_ADV_MASK 0x01000->0x0F000`). md5-candidate **207→208**; matched +2; asm subsegs
85→**84**. Quality 0/0/0/0; seed 13 / realized 13 / residual 0; regime classical. Retro applied **5 of
5** (new hazards `#-o0-bootsdk-glue-file-profile`, `#profile-probe`, `#overlapping-symbols--allow_duplicated`,
`#vendored-header-inversion`; new tools `profile_probe.py` + `audit_libultra_headers.py`; CLAUDE.md
carve-to-libnusys convention). **Cross-repo follow-up:** `nuBoot`/`IdleThread`/`MainThread`/`nuIdleFunc`
-> `sync_decomp_names.py --import-from-decomp`. **Tracked tooling follow-up:** teach `pick_target.py`
to auto-route the `idle=nuboot`/nusys-template tell to the `libnusys/` path. No carry-overs.

**S148 BANKED — `overlay_10/func_ovl10_801F4A40.c` (the FIRST classical game-code bank).** The 2-fn
overlay_10 .text pack (`func_ovl10_801F4A40` flag-gated sound/setup + `func_ovl10_801F4AD8` `*p=*p`
accessor, 176B, one-tu) banked first-build seed-only, asm-first (Ghidra MCP was down all sprint), ROM
SHA-1 == baserom. md5-candidate **206→207**; remaining 1523→**1521** asm fns / 78→**77** rows. Quality
0/0/0/0; seed 13 / realized 12 / residual −1; regime classical (small-pack exemption applied). Names
kept as `func_ovl10_*` placeholders (MCP down → cross-repo name follow-up DEFERRED; curate +
`sync_decomp_names.py --import-from-decomp` when Ghidra is back). Retro applied 4 of 5 (#1 small-pack
classical exemption + pts-recalibration tooling follow-up; #2 asm-first seed fast-path; #3 overlay
path convention; #5 this Epic-2 reframe). #4 = the deferred name follow-up (carry-over note, below).

**S147 — libmus `player.c` TU COMPLETE (the whole 109-fn game-embedded sequence player) → md5-candidate.**
Planned a cap-small first batch; banked the ENTIRE 109-fn `#include`-chained TU (internals + Mus* API +
fifo + 44 `mus_cmd_*` handlers), split into `src/libmus/player.c` + `src/libmus/player_commands.c`. The
bulk banked verbatim vs libmus 3.14 (ASM-first per-fn; 6-7 game-divergences reconstructed classically).
**The 3 inline-bound tail fns fell to a multi-TU split + a cross-game reference build.** `func_8009BC58`
(__MusIntRandom) + `allocate_object_slot`: split the carve at 16-aligned 0x8009C540 (internals \|
command handlers) so their callers became cross-TU → `jal` (not inlined). **`__MusIntMain` was twice
wrongly declared unbankable** (a "compiler wall", then a "permanent same-TU carry" w/ a rodata-gap proof
that `mus_fifo_dispatch` shares its `.o`); both overturned by building the 4 matched reference games
(drmario64/hm64/snowboardkids2/puzzleleague64; PPL byte-exact at the same -O3). The fix was source
STRUCTURE, not a TU wall: `__MusIntFifoProcess` (drain) defined BEFORE `mus_fifo_dispatch` keeps the
dispatch forward-declared → out-of-line (`jal`) while the tiny drain inlines into `__MusIntMain`;
SUPPORT_PROFILER on (2× osGetCount); manual Fstop field-clear inline (Fstop cross-TU); and the 4
`*_frame < channel_frame` tests as the signed-subtraction `(s32)(a - channel_frame) < 0` (`subu`+`bgez`).
md5-candidate **204→206**; asm subsegs 87→**86** (main+idle `[0x748B0]` stays asm). Quality 0/0/0/**1**
(re-opened = the frame handler unbankable→banked); seed 13 / realized 17 / residual +4; regime mixed
(FULLY banked, the full 13pt). **Cross-repo follow-up:** ~109 names → `sync_decomp_names.py
--import-from-decomp`. No carry-overs. Retro applied 3 of 3 (#1
`docs/hazards.md#same-tu-inline-mismatch-definition-order--cross-tu-split` + index rows; #2 CLAUDE.md
"build references before unbankable" rule; #3 `pick_target.py` phantom de-rank). **`src/libmus/`
player.c — the largest game-embedded lib file — is DONE; next libmus unit is `[0x78330]` CustomInit/
player_fx bundled synth.**

**S146 — `aud_dma.c` COMPLETE (libmus DMA buffer mgr, game-modified cart-only) → md5-candidate; `src/libmus/` 100%.**
Banked the S145 carry (`[0x78D10]`, the LAST libmus leaf asm subseg) FULLY as C — all 6 fns, ROM SHA-1
== baserom. `src/libmus/` now 0 INCLUDE_ASM stubs (all 5 carved leaf files md5-candidate). **ASM-first
overturned the carry-over's "4 stock + carry DmaSample" premise:** only `__CallBackDmaNew`/
`__CallBackDmaProcess` pure stock (DDROM test intact) + `func_8009DBA0` an MG64-emptied `jr ra` stub
(free at the gate); THREE game-modified — `__MusIntDmaInit` stock+1-insert (persists `dma_buffer_count`
→ new `g_mus_dma_buffer_count`@0x800C7AC0), `__MusIntDmaProcess` 2nd-half rewritten (flat-array
`keep_count` ageing under `osSetIntMask`, not the upstream linked-list free-walk), `__MusIntDmaSample`
classical rewrite (cart-only, `g_mus_control_flag&1` first, `keep_count=0x20000001`, + an added
min-`keep_count` eviction loop). DmaSample matched with 4 GCC-codegen levers (`docs/hazards.md#cross-jump-tail-merge`
inverse-levers note): explicit `else` → branch flip; shared `goto` → block a false value-prop
tail-merge; goto-skip → mid-fn failure-block; best-first operand order → load order. Enablers: subseg
flip `[0x78D10]` + 10 symbol adds (9 drop-static bss @0x800E70A0 block + `g_mus_dma_buffer_count`
drop-def). md5-candidate **203→204**; asm subsegs **88→87**. Quality 0/0/0/0; seed 8 / realized 10 /
residual +2; regime mixed (FULLY banked). **Cross-repo follow-up:** 10 new symbols + correct stale
ghidra `mus_dma_{cb_new,callback,sample}` → `__CallBack{DmaNew,DmaProcess}`/`__MusIntDmaSample` via
`sync_decomp_names.py --import-from-decomp`. No carry-overs. Retro applied 4 of 4 (#1 ASM-verify-each-fn
caveat; #2 sprint-plan auto-freeze classical/mixed; #3 gitignore `*.u`; #4 inverse-tail-merge 4-lever
note). **The libmus carved-leaf tree is DONE; the next libmus units are the game-embedded player TUs
(`[0x748B0]` player_api/player/player_fifo, `[0x78330]` CustomInit/player_fx bundled synth) — a
separate src/main/ game-region-carve track, not carved leaves.**

**S145 — `aud_sched.c` COMPLETE (libmus scheduler verbatim `.data`-carve mirror) → md5-candidate.**
Split the last libmus asm pack `[0x78D10]` (aud_dma.c + aud_sched.c, the S144-flagged 2-file pack) at the
upstream-file boundary (rom 0x791C0 / vram 0x8009DDC0, 16-aligned); banked the cleaner **aud_sched.c** (4
STOCK libmus 3.14 scheduler fns: `__MusIntSchedInit` + static `__OsSched{Install,WaitFrame,DoTask}`), carried
game-modified aud_dma.c. Verbatim cp, bodies byte-stock. **`.data` carve** `[0xA2ED0,.data,libmus/aud_sched]`=0x10
(`default_sched` musSched vtable + `__libmus_current_sched`=&default_sched), 3-way `main_data` split w/
`main_data_1a` tail; `__libmus_current_sched` shared (MusSetScheduler asm) but undroppable → forced in-section
carve (S116). **drop-static:2bss** (`audio_sched`→curated `g_mus_sched_ptr`@0x800E70E0, `sched_mem`@0x800E70E4).
**GOTCHA (1 re-attempt): `OSScTask` `_FINALROM` struct-drift** — first build missed 5 bytes in `__OsSchedDoTask`'s
stack frame (0x90 vs ROM 0xA0); MG64's 3rd-party libmus was built NON-FINALROM (OSScTask carries its
`#ifndef _FINALROM` startTime+totalTime = 0x10) while base CFLAGS are `-D_FINALROM` → fixed `mk/libmus.mk`
`-U_FINALROM` (game/libultra stays FINALROM; 3 banked siblings re-matched). `@99.99` body-divergence was a FALSE
flag. md5-candidate **202→203**; asm subsegs **88** (the aud_dma carry `[0x78D10]` remains). Quality 0/0/0/0.
**Next libmus:** `[0x78D10]` (aud_dma.c, the LAST libmus asm subseg — game-modified classical, see Carry-overs)
and `[0x78330]` (player_fx.c / `al_init`, bundled-n_audio dup). **Cross-repo follow-up:** 1 name
(`__MusIntSchedInit`) → `sync_decomp_names.py --import-from-decomp`.

**S144 — `aud_thread.c` COMPLETE (libmus `__MusIntThreadProcess` classical) → md5-candidate; the libmus band's FIRST classical bank.**
The S143 carry-over banked. `__MusIntThreadProcess` (the audio-thread frame loop) was framed S143 as a
from-scratch MG64-custom body; once `aud_sched.h` was vendored it proved to be the STOCK libmus **3.14**
thread-proc (the `musSched` vtable via `__MusIntSched_{install,waitframe,dotask}` macros + the stock
`last_task` func-static), with only a ~4-instr MG64 pause/mute insert (`if (paused@0x800C7AE0) {
osAiSetNextBuffer(silence@0x800C7AE8, 0x10); continue; }`) genuinely custom. Seeded from the stock 3.14
source + the insert; banked near-verbatim **MATCH first build, 0 iteration**. 4 drop-static recover-externs
(`__libmus_current_sched`@0x800C7ADC + `g_mus_audio_{paused,last_task,silence_buffer}`@AE0/AE4/AE8) + 1
callee `rom:` override (`__MusIntDmaProcess`@0x8009DA8C vs ghidra `mus_dma_process`, surfaced as a LINK-time
`undefined reference`); `MICROCODE_CODE` referenced the placed `rspbootTextEnd` (== `n_aspMainTextStart`
@0x800B3F20) via a local `#define` (no dup-vram add). md5-candidate **201→202**; asm-backed subsegs
**89→88** (last `aud_thread` stub cleared). Quality 0/0/0/0; classical track seed 5 / realized 4 / residual −1.
**Next libmus:** `[0x78D10]` (aud_dma.c + aud_sched.c, a `blk` 10-fn 2-file pack, double
`body-divergence-suspect@99.99` → decompose at the file boundary + body-triage at the gate) and `[0x78330]`
(player_fx.c / `al_init`, the bundled-n_audio dup, `coddog-fncount-mismatch:6vs13`). **Cross-repo follow-up:**
5 names → `sync_decomp_names.py --import-from-decomp`.

**S143 — `aud_thread.c` PARTIAL (libmus integrator init banked; threadproc carried) + the deeper libmus band opened.**
The S142-directed smallest libmus follow-on. `aud_thread.c` (`[0x79370]`, 2 fns) flipped to `c`; **`__MusIntAudManInit`
(the audio-manager init) banked C** as a verbatim libmus **3.14** mirror, **`__MusIntThreadProcess` carried INCLUDE_ASM**
(MG64-custom) → bank-stock-carry-custom PARTIAL: matched **+1**, md5-candidate **201→201** (file 1 stub), asm subsegs
**89→88**. **VERSION CORRECTION: the game libmus = 3.14 (n64sdkmod), NOT the DiskLS 3.11 pin** — 3.14 `aud_thread.c`
has `EXTRA_SAMPLES_N=20` (asm `li a3,0x14`), 3.11=15 (the only body diff vs 3.11). Pin corrected in
`tools/audio_ref_versions.tsv` (DiskLS rows disabled) + `mk/libmus.mk` comment. **alInit duplicate-symbol chain RESOLVED:**
`alInit`→(`n_libaudio_sn_sc.h`)`n_alInit`→(`player_fx.h` under FXCHANGE)`CustomInit`@0x8009CF30 = the libmus-BUNDLED synth
init (ghidra mis-named `al_init`); standalone `n_alInit`@0x800A0730 DEAD (0 xrefs) → verbatim macro chain + a `CustomInit`
recover-extern, NO redirect hack (see `docs/hazards.md#libmus-bundled-n_audio-duplicate`). **Band-open enabler:**
`mk/libmus.mk` += `-I include/libmus/PR` + `-I include/libultra/PR`; 7 vendored `src/libmus` headers (aud_sched/aud_dma/
aud_thread/player_fx/synthInternals/n_synthInternals/n_abi) → **unblocks `aud_dma.c` + `player_fx.c`** (the remaining 2
libmus subsegs). drop-static:4bss (`thread`@0x800E70F0 +symbol add / `stack_addr` / `audio_tasks` / `audio_command_list`
→ extern `g_mus_audio_*`) + drop-def `__libmus_alglobals`@0x801B56A0 (`N_ALGlobals`=0x50). Quality 0/0/**1**/0 (1 carried),
seed 8 / banked **0pt** (partial; +1 matched-fn is the value signal). Retro applied 3 of 4 (#1 pin→3.14, #2 hazard doc +
index, #4 this capture; #3 pricing-tell DEFERRED to a golden-gated branch). **Cross-repo follow-up:** 4 names → 
`sync_decomp_names.py --import-from-decomp`. **Next:** `aud_dma.c` (10fn, 2-file aud_dma+aud_sched pack) / `player_fx.c`
(the `[0x78330]` bundled-synth, 13fn, fncount-mismatch 6vs13 → the duplicate-naming plan); both header-unblocked now.
Plus the `__MusIntThreadProcess` carry (MG64-custom, completeness checklist below).

**S142 — `aud_samples.c` BANKED (libmus, the `[0x79780]` split remainder; 2 fns).**
The S141-directed smallest libmus follow-on. `aud_samples.c` (`__MusIntSamplesInit` + `__MusIntSamplesCurrent`)
is the `[0x79780]` split remainder S141 left when it decomposed the `[0x79640]` pack. Verbatim libmus
n_audio_sc-path cp (active `#else SUPPORT_NAUDIO` branch, N_SAMPLES=184), **MATCH first build** →
**md5-candidate 200→201**; asm subsegs **90→89** (1 flip). **First libmus body-divergence diagnosis pass:
`@99.99` = a VERSION delta** (file header v3.11 vs the v3.12 pin), NOT a customized body — the asm == upstream
byte-for-byte (`0xB21642C9` /184 + `0x51EB851F` /100 magics, ±0xB8 min/max, the `only_one_flag` logic), and
ZERO callees (no jal) → no customization tell → the verbatim-mirror single-file-pack exemption held. This
reinforces the S141 per-coddog-score hedge: a libmus single-file-pack with NO customization tell (no non-lib
`func_` callee, no unexplained jal-count-mismatch) is structural`@99.99` → trust after a 1-pass asm diff — but
**re-confirm PER FILE** (do NOT yet blanket-trust the whole band; the remaining `aud_*.c` carry heavier flags).
**drop-static:4bss (pre-curated sub-case):** `frame_samples{,_min,_max}`/`extra_samples` → `extern g_mus_*`
@0x800E72C0+, already in `ghidra_symbols.txt` → NO symbol add, just rename the active body to the curated
names (`.o .bss`=0). **defines-data:** `only_one_flag` (func-local static=1, automatic sole-referrer) → `.data`
carve `[0xA2F00,0xA2F10)`=0x10 (4 B + 0xC pad), a 1-line split of `main_data`'s tail. **wrong-ghidra-name
override:** `__MusIntSamples{Init,Current}` override ghidra `mus_samples_{init,current}` via `rom:` qualifier
(gate). Quality 0/0/0/0, seed 5 / banked 5pt (mirror, seed-only). Retro applied 3 of 3 (all DOC, no
tooling/golden touch): #1 `docs/hazards.md#file-static` pre-curated drop sub-case (reference the ghidra name,
no symbol add); #2 this libmus `@99.99`=version-delta data point; #3 `docs/hazards.md#defines-data`
func-local-static auto-sole-referrer note. **Cross-repo follow-up:** `__MusIntSamplesInit`/`__MusIntSamplesCurrent`
→ `sync_decomp_names.py --import-from-decomp`. **Next:** libmus band continues; the smallest remaining is
`aud_thread.c` (`mus_thread_create`, 2fn, 720B) but heavier — 6 vendored headers (libmus.h/aud_sched.h/aud_dma.h/
aud_samples.h/player_fx.h/aud_thread.h), `refs-unplaced:__libmus_alglobals`, `drop-static-mirror:5bss`,
`defines-data:__libmus_alglobals,last_task` — then aud_dma.c (10fn, 2-file aud_dma+aud_sched pack) / player_fx.c
(`al_init`, 13fn, calls-unplaced + coddog-fncount-mismatch). No carry-overs.

**S141 — `lib_memory.c` BANKED + the libmus band OPENED (header-vendoring enabler + first leaf mirror).**
The S140-directed next audio vein. Because libnaudio is 100% and pre-paid the n_audio_sc header base,
opening libmus cost only ~3 vendored headers (`n_libaudio_sn_sc.h`→include/libnaudio; `libmus_config.h`
+ `lib_memory.h`→src/libmus) + a 2-line `mk/libmus.mk` (KMC -O3, `-DSUPPORT_NAUDIO`, `-I src/libmus`) +
a mechanical `pick_target.py` libmus registration. First mirror `lib_memory.c` (the `@100.00` coddog
LEAF, 6 `__MusIntMem*` fns) **MATCH first build** → **md5-candidate 199→200**; asm subsegs stayed 90
(the `[0x79640]` pack split into `[0x79640, c, libmus/lib_memory]` + `[0x79780, asm]` aud_samples
remainder, a new asm subseg). Verbatim n_audio_sc-path cp + drop-static (`audio_heap`→extern
@0x800E72B0, ALHeap=0x10) + wrong-ghidra-name override (`__MusIntMemInit` overrides ghidra
`mus_heap_init` via `rom:0x79640` qualifier, S128 mechanism). Only external calls `alHeapInit`/
`alHeapDBAlloc` (placed in libnaudio) → NO calls-unplaced; places the `__MusIntMem*` allocator the whole
band depends on. Quality 0/0/0/0, seed-only (mirror). Retro applied 4 of 4: #1 sibling-audio-lib hedge
DOWNGRADE (CLAUDE.md story-points + the S140 note below), #2 `@100.00`-leaf-first band-open heuristic
(`docs/hazards.md#upstream-mirror-pattern`), #3 pick_target per-file blk-delta hint (deferred to a
golden-gated tooling branch, tracked in Carry-overs), #4 cross-repo name sync. **Cross-repo follow-up:**
6 lib_memory names → `sync_decomp_names.py --import-from-decomp` (esp. `__MusIntMemInit`). **Next:**
libmus band OPEN; smallest follow-on `aud_samples.c` (`mus_samples_init`, the `[0x79780]` split
remainder, 2fn) then aud_thread / aud_dma / aud_sched / player_fx (each vendors its own `aud_*.h`;
`file-static` + `body-divergence-suspect` per row, hedge per-coddog-score). No carry-overs.

**S140 — `n_env.c` BANKED (the LAST libnaudio asm subseg → `src/libnaudio/` tree 100%; 5 fns).**
The S139 carry-over `[0x79E70]` (n_env.c, pts-13, the final libnaudio asm subseg). Verbatim n_audio_sc
N_MICRO mirror of `n_alEnvmixerPull` + `n_alEnvmixerParam` + 3 file-statics, MATCH first build, 0 re-attempt
→ **md5-candidate 198→199**; **libnaudio asm subsegs → 0 (the entire `src/libnaudio/` n_audio_sc N_MICRO
mirror tree, 20/20 .c, is decompiled).** Both carves were clean 1-line ATTRIBUTE FLIPS (no splits): `.data`
n_eqpower[128]=0x100 was `main_data_1a` exactly; `.rodata` jtbl_800D2120 + `_getRate` f64 consts=0x70 was the
generic `[0xAD520,0xAD590)` block exactly; n_env.o sections matched to the byte (`.text` 0x9d0). The scary gate
flags were FALSE: `calls-unplaced:__pow` + `jal-count-mismatch:20vs15` = pick_target counting the `#ifndef
N_MICRO` branch (asm/79E70.s has zero `__pow`/`_frexpf`/`_ldexpf` under `-DN_MICRO=1`); `static-name-collision`×3
benign (file-statics). `body-divergence-suspect@99.99` FALSE → **10 consecutive on n_audio_sc (S133-S140)**. ZERO
symbol adds. Quality 0/0/0/0. seed 13 / banked 13pt (mirror, seed-only; verbatim-mirror single-file-pack
exemption). Retro applied 3 of 3: #1 `build_config.py` `_strip_inactive_define_branches` wired into
`pick_target.py` call_divergence/calls_unplaced/refs_unplaced (profile `-D` set strips `#ifndef N_MICRO` phantom
calls, retires the `__pow`/jal-count class; suite 94 pass, no golden regen); #2 `docs/hazards.md#static-name-collision`
benign-reframe; #3 this libmus hedge reset (below). **Cross-repo follow-up:** none (all names pre-curated).
**Milestone: `src/libnaudio/` is 100% — a publish-to-master candidate (PO deferred; staying on dev).**
**Next band — libmus `aud_*.c` (HEDGE RESET):** the next audio sub-band is libmus (`mus_thread_create`/aud_thread.c,
`mus_heap_init`/aud_samples.c, `mus_dma_init`/aud_dma.c, `al_init`/player_fx.c, …). Do **NOT** carry the
n_audio_sc 10/10-verbatim confidence into it: those rows carry heavier flags (`file-static` + `drop-static-mirror:Nbss`
+ `body-divergence-suspect` + nearly all `blk` needs-header on `libmus_config.h`/`libaudio.h`/etc.), so the band
likely needs a **header-vendoring ENABLER sprint first** and may have genuinely divergent (game-customized) bodies.
Reset the body-divergence hedge to FULL for libmus (treat like the S123 libnusys class, not the n_audio_sc class).
**DOWNGRADED S141 (see S141 above):** the enabler was SMALL (libnaudio pre-paid the shared n_audio_sc header
DAG — ~3 headers + a 2-line mk profile + a mechanical pick_target add), and the `@100.00` leaf `lib_memory.c`
banked first-build seed-only (NOT a customized body). Hedge body-divergence **per-coddog-score** going forward —
`@100.00` = trust the verbatim mirror, `@99.99` = the S121/S123 diagnosis-pass / exemption-guard — not a
blanket-FULL-by-lib reset. Each remaining libmus `.c` vendors only its own private `aud_*.h` at bank time.

**S139 — `n_auxbus.c` + `n_drvrNew.c` BANKED (DECOMPOSE the last libnaudio pack func_8009E4B0; 3 fns).**
The S138-"Next" increment. The last libnaudio asm subseg `[0x798B0]` (func_8009E4B0, 8 fns, pts-13) was a
c-combined 3-file pack; the 8-gate fired so it was DECOMPOSED at the 3 upstream-file boundaries
(n_auxbus | n_drvrNew | n_env, all 16-aligned: 0x798B0/0x79950/0x79E70) and the cleanest 2 banked →
**md5-candidate 196→198**. `n_auxbus.c` (`n_alAuxBusPull`, 1 fn) = pure-text N_MICRO mirror, MATCH first
build, no carve (lone callee `n_alEnvmixerPull`=0x8009EA70 placed at gate). `n_drvrNew.c` (`n_alFxNew` +
`alN_PVoiceNew`, 2 fns) = verbatim mirror + `.data` carve [0xA2F10,0xA30A0)=0x190 (6 contiguous PARAMS
arrays, 3-way main_data split) + `.rodata` carve [0xAD4F0,0xAD520)=0x30 (fxType jtbl + 2 doubles, a clean
PREFIX of n_env's rodata). `body-divergence-suspect@99.99` FALSE all 3 (9 consecutive on n_audio_sc);
`dmaNew` = `ALDMANew` fn-ptr param via jalr (false calls-unplaced). Quality 0/0/0/1 (the 1 carry = n_env,
the planned decompose remainder). seed 5 / banked 5pt (mirror, seed-only; 8-gate resolved by decompose).
Retro applied 3 of 3: #1 `pick_target.py` `c-combined-undercount` (FILE analog of coddog-fncount-mismatch);
#2 body-divergence suppression re-keyed on `up_lib==libnaudio` + clean single-source shape (post-decompose);
#3 `all_fn_ptr_typedefs` drops the typedef'd-fn-ptr-param calls-unplaced phantom (ALDMANew). +2 unit tests,
goldens regen'd (bank drift), suite 94 pass. **Cross-repo follow-up:** `n_alEnvmixerPull` →
`sync_decomp_names.py --import-from-decomp`. **Next:** `n_env.c` (`[0x79E70]`, the carry — last libnaudio
asm subseg, 5 fns: inc-vendor + `__pow` rodata pool + own jtbl + 3 static-name-collisions; see Carry-overs
for the completeness checklist). Carry-over: n_env.c.

**S138 — `n_reverb.c` BANKED (n_alFxPull single-file-pack, n_audio_sc N_MICRO mirror; 6 fns).**
The S137-"Next" #1 increment, all 6 fns Match → **md5-candidate 195→196**; asm subsegs 92→91 (1 flip, at gate).
Verbatim cp of n_audio_sc `n_reverb.c` (`n_alFxPull` + `n_alFxParamHdl` + 4 static helpers `_n_loadOutputBuffer`/
`_n_loadBuffer`/`_n_saveBuffer`/`_n_filterBuffer`) + 4 verbatim `inc/n_reverb_add0{1..4}.inc.c` body-includes, on
the existing vendored `n_synthInternals.h` (NO new header). **Two carves:** (a) the expected rodata-literal — the
`n_alFxParamHdl` jtbl @0x800D21A0 + 3 f64 literals — a 1-line flip of generic `[0xAD5A0, rodata]` → `[0xAD5A0,
.rodata, libnaudio/n_reverb]` (0x40); (b) a **NEW `.data` carve** the gate priced only by NAME (`defines-data:val,
blob`): KMC -O3 emits the UNUSED function-local statics (`val/lastval/blob`, no asm `%hi/%lo`) → build #1 was 16 B
larger; localized by VALUE to rom 0xA31A0 (vs the libultra reverb twin @0xA356C), 3-way split `main_data | n_reverb
.data | main_data_1b`, then byte-exact. `body-divergence-suspect@99.99` (6th consecutive FALSE on n_audio_sc),
`refs-unplaced:L_INC` (dead extern), `calls-unplaced:init_lpfilter` (`_init_lpfilter` placed) all false. Gate
enablers: flip `[0x7B140]` + 5 symbol_addrs (n_alFxParamHdl + 4 `_n_`-helpers, disjoint from libultra reverb's
@0x800A67xx); n_alFxPull already placed S136. Quality 0/0/0/0 (1 diagnosis pass = the `.data` carve). seed 13 /
banked 13pt (mirror, seed-only, 13-gate fired → single-file-pack exemption). Retro applied 3 of 3: #1
`docs/hazards.md#defines-data` unused-static sub-case; #3 `pick_target.py` body-divergence suppression for
n_audio_sc single-file-packs (libnusys excluded); #2 DEFERRED with spec to a golden-gated tooling branch (see
Carry-overs). Goldens regen'd (bank drift; suite 92 pass). **Cross-repo follow-up:** 5 names →
`sync_decomp_names.py --import-from-decomp`. **Next:** libnaudio band down to 1 — `func_8009E4B0`/`n_alAuxBusPull`
(n_auxbus/n_drvrNew/n_env multi-coddog c-combined 2-file pack, static-name-collision×3; pts-13, 8-gate FIRES,
decompose at the file boundary; body-divergence-suspect KEPT — multi-file, not exempt). No carry-overs.

**S137 — `n_synallocfx.c` + `n_mainbus.c` BANKED (n_audio_sc 2-file N_MICRO mirror; retires S130 spike).**
The S136-"Next" #1 increment, both fns Match FIRST build → **md5-candidate 193→195**; asm subsegs 93→91. Split the
c-combined `[0x7C720, asm]` at vram 0x800A1370 → `n_synallocfx.c` (`n_alSynAllocFX`, 0x50: `n_alFxNew(&n_syn->
auxBus->fx_array[bus],c,hp)` + return — pure call+return) + `n_mainbus.c` (`n_alMainBusPull`, 0x80: N_MICRO
aClearBuffer + indirect `mainBus->filter.handler` + 2× aMix, 8B trailing-pad@16 absorbed by the cp). Both verbatim
n_audio_sc cps on the existing vendored `n_synthInternals.h` (NO new header, NO rodata/data carve). The S130
near-free-retry completeness checklist replayed verbatim-correct (boundary 0x7C770, callee `n_alFxNew`=0x8009E550
jal-confirmed) — 0 rework. Gate enablers: split `[0x7C720]`→2 subsegs + 1 symbol add (`n_alFxNew`=0x8009E550; the
lone calls-unplaced callee, stays asm in the n_auxbus pack — also pre-resolves one n_auxbus-pack callee).
`n_alSynAllocFX`/`n_alMainBusPull` already named S136/S133. `body-divergence-suspect@99.99` FALSE both fns (asm ==
upstream). Quality 0/0/0/0. seed 5 / banked 5pt (mirror, seed-only, 8-gate clear). Retro applied 2 of 2 (both
knowledge-capture, no tooling edit); no golden/test touch. **Cross-repo follow-up:** 1 name (`n_alFxNew`) →
`sync_decomp_names.py --import-from-decomp`. **Next:** the 2 remaining libnaudio candidates are both pts-13 with
`body-divergence-suspect@99.99` + heavier hazards — `n_alFxPull`/`n_reverb.c` (single-file-pack:6fn, 4 inc
fragments + `defines-data:val,blob` + `refs-unplaced:L_INC` + rodata-jtbl/literal; 8-gate exemption may apply but
TRIAGE BODIES first per the S123 guard) and `func_8009E4B0`/`n_alAuxBusPull` (n_auxbus/n_drvrNew/n_env multi-coddog
2-file pack, static-name-collision×3; 8-gate FIRES, decompose at the file boundary). No carry-overs.

**S136 — `n_synthesizer.c` BANKED (n_audio_sc synth-driver core; 8-fn verbatim N_MICRO mirror + rodata carve).**
The S135-"Next" #2-class file, all 8 fns Match → **md5-candidate 192→193**; asm subsegs 94→93. The
synthesis-driver core: `n_alSynNew` (the synth `new`) + `n_alAudioFrame` (the per-frame command build,
`ONLY_ONE_PLAYER`) + `__n_allocParam`/`_n_freeParam`/`_n_collectPVoices`/`_n_freePVoice` + the file-static
`_n_timeToSamplesNoRound` (func_800A1224, kept file-local) + `_n_timeToSamples`. A 1376B verbatim cp on the
existing vendored headers (no new header). First-build SHA-missed on the EXPECTED S134-class rodata-literal
(GCC pooled `1000000.0`/`0.5` doubles @0x800D21E0, 0x20); byte-cmp localized it (all 8 bodies byte-identical,
only the unplaced rodata) → SPLIT the generic `[0xAD5E0, rodata]` (0xA0) into `[0xAD5E0, .rodata,
libnaudio/n_synthesizer]` (0x20) + `[0xAD600, rodata]`. `body-divergence-suspect@99.99` + `jal-count-mismatch:3vs8`
were BOTH false (the jal gap = `alHeapAlloc` macro ×5). Gate enablers: flip `[0x7C170]` + 8 symbol_addrs (4
member fns + calls-unplaced `alN_PVoiceNew`/`n_alSynAllocFX` + handler refs `n_alFxPull`=0x8009FD40 /
`n_alAuxBusPull`=0x8009E4B0). **The handler pre-naming RESOLVED the S130 `n_mainbus` spike** (`func_800A1320` =
`n_alSynAllocFX`) and PRE-NAMED the next 2 candidates' leaders. Quality 0/0/0/0. seed 8 / banked 8pt (mirror,
seed-only; single-file-pack exemption). Retro applied 3 of 3 (rodata `extent-end`; `(macro-artifact?)` jal
annotation + body-divergence suppression; gate pre-naming docs note); +3 unit tests, suite 92 pass. **Cross-repo
follow-up:** 8 names → `sync_decomp_names.py --import-from-decomp`. **Next:** the smallest libnaudio candidate is
now `n_alSynAllocFX` (`[0x7C720]`, pts-5, `c-combined:2file[n_mainbus|n_synallocfx]` — the resolved S130 spike,
both callees placed; split at the file boundary). Then the heavier `n_alFxPull`/`n_reverb.c` (pts-13, 6 fns,
4 inc fragments + `defines-data:val,blob` + `refs-unplaced:L_INC` + rodata-jtbl/literal; decompose/mixed) and
`func_8009E4B0`/`n_alAuxBusPull` (n_auxbus/n_drvrNew/n_env multi-coddog pack, pts-13). No carry-overs.

**S135 — `n_load.c` BANKED (n_audio_sc N_MICRO ADPCM-decoder mirror; the cleanest inc-vendor yet).** The
S134-"Next" #2, all 3 fns Match on the FIRST build → **md5-candidate 191→192**; asm subsegs 95→94.
`n_alAdpcmPull` (the ADPCM pull iface) + `n_alLoadParam` (the AL_FILTER_SET_WAVETABLE/RESET setter) +
the file-static `_decodeChunk` (func_8009FA14, called 3x from n_alAdpcmPull), a 1824B verbatim N_MICRO
copy with the 2 `inc/n_load_add0{1,2}.inc.c` body-fragments vendored. **No rodata/data hazard** (unlike
S134's MAX_RATIO carve) — a pure text mirror like n_save, first-build SHA with zero carve. The `blk`
was a FALSE-FLAG from the named C-index resolving `up_path` to the WRONG non-sc `libnaudio/src/n_load.c`
variant (`add/*.c` fragments, a tree not in UPSTREAM_SRC_ROOTS); the authoritative coddog source is the
n_audio_sc `n_load.c` with vendorable `inc/*.inc.c` (exactly the mis-resolution the S134 retro
predicted). **Only gate enabler = the subseg flip** (`[0x7A840]→[c, libnaudio/n_load]`); both real
callees were named at the S134 gate. The static `_decodeChunk` stayed file-local (NO symbol_addrs add —
a global would collide with the placed `_decodeChunk = 0x800A4E3C`, the non-micro decoder; func_8009FA14
has no external refs). Quality 0/0/0/0 (first-build atomic, no spike). seed 8 / banked 8pt (mirror,
seed-only; single-file-pack exemption). Retro applied 2 of 2: #1 `pick_target.py`
`_deblk_audio_variant_misresolve` (drop the wrong-variant `add/*.c` block + re-derive `blocked` from the
authoritative coddog `inc/*.inc.c` when a definitive audio coddog-mirror replaced `up_path`; libmus
`aud_*` real-header rows stay `blk`); #2 `_append_static_name_collisions` + `static_name_collision`
Hazard + `placed_symbol_addrs` (flags `static-name-collision:<name>@<addr>` when a coddog upstream
file-static name is already placed at another vram; live on `func_8009E4B0`
`_pullSubFrame`/`_getRate`/`_getVol`); factory test + `docs/hazards.md#static-name-collision` +
`#coddog-cross-ref` de-blk note + CLAUDE.md index row; 4 goldens regen'd, suite 89 pass. **Cross-repo
follow-up:** none new (callees named S134; the static is file-local). **Next:** the heavier remaining
n_audio_sc band — `n_reverb.c` (`func_8009FD40`, pts13, 6 fns, 4 inc fragments + `defines-data:val,blob`
+ `refs-unplaced:L_INC` + `rodata-jtbl`/`rodata-literal` + 2 calls-unplaced; decompose/mixed) and
`func_8009E4B0` (n_auxbus/n_drvrNew/n_env, pts13, 8 fns, multi-coddog-source pack now carrying the
static-name-collision flags). The libmus `aud_*` DAG (`file-static` + BSS statics + real non-vendorable
headers) stays genuinely `blk`. No carry-overs.

**S134 — `n_resample.c` BANKED (n_audio_sc N_MICRO mirror + MAX_RATIO rodata-literal carve).** The
S133-"Next" #1, both fns Match → **md5-candidate 190→191**; asm subsegs 96→95. `n_alResamplePull` +
`n_alResampleParam` (the N_MICRO `switch` degenerates to `default:`→`n_alLoadParam`, no jtbl), a 480B
verbatim mirror with the 2nd `.inc.c` body-include vendored (`inc/n_resample_add01.inc.c`). **First
build SHA-missed on an UNFLAGGED rodata-literal:** the `MAX_RATIO` double 1.99996 @ `D_800D2190` (rom
0xAD590) that `n_alResamplePull`'s `ldc1 %lo(D_800D2190)` loads — GCC's own .rodata literal linked to
the wrong vram (%hi matched, %lo off). The `@99.99 body-divergence-suspect` hedge budget paid for the
diagnosis: byte-cmp localized it to 0x7AFEA (the `ldc1` %lo), NOT body divergence (both fns matched).
Fixed with the S101 generic-subseg-bound `.rodata` carve — the generic `[0xAD590, rodata]` was EXACTLY
the 0x10-byte block (double + `.double 0` pad) = a 1-line flip `[0xAD590, .rodata, libnaudio/n_resample]`;
`.o(.rodata)` byte-matches baserom. Gate enablers: 4 symbol_addrs (`n_alResamplePull`=0x8009FB60 +
`n_alResampleParam`=0x8009FD1C + jal-verified callees `n_alAdpcmPull`=0x8009F440 / `n_alLoadParam`=0x8009F888,
both stay asm in n_load.c); flip `[0x7AF60]`. Quality 0/0/0/0 (1 diagnosis pass, no spike). seed 5 /
banked 5pt (mirror, seed-only). Retro applied 2 of 2: #1 `pick_target.py` `--lib audio` SCOPE-ALIAS
(`_row_filtered`, up_lib ∈ AUDIO_CODDOG_LIBS) → the audio band surfaces uniformly (n_reverb/n_load were
invisible pre-fix); #2 `pick_target.py` `_append_coddog_trap_hazards` pairs the rodata-literal scan into
the coddog path (dedup-guarded) → coddog/audio mirrors price the FP-pool carve at the gate (live:
n_reverb `func_8009FD40` now shows `rodata-literal:0x800D21C0,…`). Goldens regen'd for the bank drift (4
goldens, suite 89 pass). **Cross-repo follow-up:** 4 symbols (`n_alResamplePull`/`n_alResampleParam` +
callees `n_alAdpcmPull`/`n_alLoadParam`) → `sync_decomp_names.py --import-from-decomp`. **Next:** the
remaining n_audio_sc band — `n_reverb.c` (`func_8009FD40`, pts13, 6 fns, 4 inc fragments, now-flagged
`rodata-literal:0x800D21C0` + `rodata-jtbl:0x800D21A0` + 2 unplaced callees) and `n_load.c`
(`func_8009F440`/`n_alAdpcmPull`, pts13, 3 fns, 2 inc fragments — prefer the coddog `n_load.c@99.99`
n_audio_sc `inc/` source over the named index's wrong `add/` resolution that the gate-naming surfaced).
Both trip the 8-gate (single-file-pack exemption applies). The libmus `aud_*` DAG (`mus_thread_create`,
`file-static` + 5 BSS statics + deep needs-header) remains the heavy classical/mixed unit. No carry-overs.

**S133 — `n_save.c` / `n_alSavePull` BANKED (n_audio_sc N_MICRO command-stream mirror; first inc-vendor
+ N_MICRO pin).** A verbatim N_MICRO-branch mirror (80B): `jal n_alMainBusPull`, then the
`inc/n_save_add01.inc.c` fragment → **md5-candidate 189→190**; asm subsegs 97→96. pick_target priced it
`blk`, a FALSE-FLAG (the `.inc.c` body-include is vendorable from the n_audio_sc `src/inc/` tree; the
needs-header detector scanned only `.h` basenames). Two latent enablers surfaced at the body compile
(gate stub hid both): (1) `#needs-define N_MICRO` — the upstream builds the WHOLE lib with `-DN_MICRO=1`;
without it n_save.c took the longer non-micro path → SHA-miss. Pinned `-DN_MICRO=1` in `LIBNAUDIO_CFLAGS`
(same class as the `-DF3DEX_GBI_2` libultra pin), clean-rebuild. (2) `find src -name '*.c'` swept the
`inc/*.inc.c` fragment as a TU (parse error) → Makefile `! -name '*.inc.c'` exclusion. Gate enablers:
`n_alSavePull`=0x800A12D0 + `n_alMainBusPull`=0x800A1370 (calls-unplaced callee, stays asm); flip
`[0x7C6D0]`. Quality 0/0/0/0. seed 3 / banked 3pt (mirror, seed-only). Retro applied 3 of 3: #1
`include_is_vendorable` full-source-relative-path match under `UPSTREAM_SRC_ROOTS` (n_audio_sc `src/`
registered) → `.inc.c` prices +1 not `blk`; #2 `_parse_makefile_defines` parses `LIBNAUDIO_CFLAGS` →
libnaudio define set carries N_MICRO; #3 `docs/hazards.md#needs-header` `.inc.c` body-include sub-section
+ `#needs-define` N_MICRO library-pin sub-section. Goldens regen'd for the bank drift; suite 89 pass.
**Cross-repo follow-up:** `n_alSavePull` + `n_alMainBusPull` → `sync_decomp_names.py --import-from-decomp`.
**Next:** the inc-vendor + N_MICRO pin **de-blk the rest of the n_audio_sc band** — `n_resample.c`
(`func_8009FB60`) blk→**pts5** (smallest, 2 fns, needs `inc/n_resample_add01.inc.c` + 2 callees placed);
`n_load.c`/`n_reverb.c`/`n_env.c` blk→pts13 (decompose or batch). `func_800A0D70`/`n_synthesizer.c`
(pts-8, 3 calls-unplaced) and `n_mainbus.c` (`[0x7C720]` split, Carry-overs) still pending.

**S132 — `n_synallocvoice.c` + `n_sl.c` BANKED (n_audio_sc mirror cluster: clean cp + drop-def).** The
S131-"Next" #1+#2, both Match FIRST build → **md5-candidate 187→189**; asm subsegs 99→97.
`n_synallocvoice.c` (`n_alSynAllocVoice` + static `_allocatePVoice`) was a pure verbatim cp (all callees
placed S129). `n_sl.c` (`n_alInit` + `n_alClose`) was a **drop-def** mirror: `n_alGlobals`=0x800C7DB0 /
`n_syn`=0x800C7DB4 (both `=0` BSS, already declared extern by `n_libaudio_sc.h`) dropped to the header
externs — storage from the extracted blob, NO carve. Gate enablers: 4 fn names + `n_alGlobals`
recover-extern + `n_alSynNew`=0x800A0D70 dual-name (the one `calls-unplaced`). Combined seed 8 (3+5) ran
as a 2-file cluster (decomposed at the file boundary, per-file all-or-nothing). Quality 0/0/0/0. seed 8 /
banked 8pt (mirror, seed-only). Retro applied 1 of 1: #1 `pick_target.py` suppresses `maybe-upstream`
when a definitive `coddog-mirror` is on the row, EXTENDED to audio (S75 was libultra-only) → drops the
`func_800A0800` wrong-file IDF noise; sub-threshold hits stay advisory; 2 coddog fixture subjects
de-hardcoded off the banked `func_800A0730` → stable overlay `func_ovl6_8024D800`; suite 89 pass.
**Cross-repo follow-up:** 5 fn names + `n_alGlobals` → `sync_decomp_names.py --import-from-decomp`.
**Next:** `func_800A0D70`/`n_synthesizer.c` (pts-8, 8 fns) is the next n_audio_sc file but trips the
8-gate AND has 3 `calls-unplaced` (alN_PVoiceNew, n_alSavePull, n_alSynAllocFX) — decompose or place
those callees first. `n_mainbus.c` (`[0x7C720]`) still needs its file-boundary split (Carry-overs). The
`blk` audio libs (`n_save`/`n_load`/`n_reverb` `.inc.c` headers; the libmus `aud_*` DAG) remain.

**S131 — `n_syndelete.c` + `n_synsetfxmix.c` BANKED (split a 2-file pack at `0x7BDE0`).** The
S130-"Next" `func_800A09E0` (`n_synsetfxmix`) turned out to be a **c-combined:2file** pack, not a
single file: coddog flagged only `coddog-mirror:n_synsetfxmix.c@99.99` (matching `func_800A09F0` =
`n_alSynSetFXMix`), while the 16B leader `func_800A09E0` (`n_alSynDelete` = `n_syn->head=0`, 4 instrs)
is below coddog's fingerprint floor and went unmatched. Hand-disassembly at the gate identified the
leaf as `n_syndelete.c`; decompose-split at the file boundary 0x7BDF0 → two verbatim n_audio_sc
mirrors, **both Match FIRST build** (all callees placed S129/S130). **md5-candidate 185→187**; asm
subsegs 100→99. Quality 0/0/0/0. seed 4 / banked 4pt (mirror, seed-only). Retro applied 1 of 1: #1
`pick_target.py` ported the `coddog-fncount-mismatch` under-count guard into `_resolve_audio` (was
libultra-tail-only) → audio single-identity multi-fn packs now surface as multi-file (live: `al_init`
13fn vs `player_fx.c`@99.99 6fn → `6vs13`); + `#coddog-cross-ref` provenance; 3 golden tests
de-hardcoded off the banked `func_800A09E0` (2 dynamic-select, 1 overlay fixture subject); suite 89
pass. **Cross-repo follow-up:** 2 fn names (n_alSynDelete/n_alSynSetFXMix) →
`sync_decomp_names.py --import-from-decomp`. **Next:** `func_800A0800` (`n_synallocvoice`, pts-3,
2 source fns) is the cleanest remaining n_audio_sc mirror; `func_800A0730` (`n_sl.c`, pts-5);
`func_800A1320`/`n_mainbus.c` still needs a split (Carry-overs); the `blk` audio libs
(`n_save`/`n_load`/`n_reverb` `.inc.c` headers; the libmus `aud_*` DAG) remain. Heads-up: `al_init`
(libmus `player_fx.c`) now correctly shows `coddog-fncount-mismatch:6vs13` — a multi-file pack, not a
clean mirror.

**S130 — `n_synaddplayer.c` + `n_synsetvol.c` BANKED (libnaudio n_audio_sc setter vein, cont.).** Two
verbatim `@99.99` single-fn mirrors → **md5-candidate 183→185**; asm subsegs 102→100. `n_alSynAddPlayer`
(0x800A07B0, interrupt-masked player list-prepend) was a pure cp; `n_alSynSetVol` (0x800A0BB0, the
set-pan sibling + a `_n_timeToSamples(t)` transition-time line) needed one callee recover
(`_n_timeToSamples`=0x800A1274). The flagged `calls-unplaced:SAMPLE184,__osError` were BOTH false
positives (asm-verified): SAMPLE184 = dead `#ifdef SAMPLE_ROUND` macro, __osError = non-_DEBUG ALFailIf.
Both Match FIRST build. Quality 0/0/0/0. seed 5 / banked 5pt (mirror, seed-only). Post-bank PO-directed
ch31/32 rework (codegen-neutral, ROM byte-identical). Retro applied 3 of 3: #1 `pick_target.py`
calls-unplaced asm-jal reconciliation + band-internal-macro exclusion (drops the SAMPLE184/__osError/
__assertBreak/`__MusIntSched_*` phantoms, real callees preserved; suite 89 pass); #2 `#coddog-cross-ref`
n_syn* @99.99 empirically-verbatim note (6/6); #3 `n_mainbus.c` carry-over. **Cross-repo follow-up:** 2
fn names + `_n_timeToSamples` → `sync_decomp_names.py --import-from-decomp`. **Next:** the de-phantomed
n_syn* setters are the cleanest pickable — `func_800A09E0` (n_synsetfxmix, pts-3), `func_800A0800`
(n_synallocvoice, pts-3), `func_800A1320`/`n_mainbus.c` needs a split first (see Carry-overs); the `blk`
audio libs (`n_save`/`n_load`/`n_reverb` `.inc.c` headers; the libmus `aud_*` DAG) remain.

**S129 — `src/libnaudio` STOOD UP + 4 n_syn* setter mirrors BANKED (n_audio_sc header band unlock).**
The `audio` scope's only pickable work was the game-fault grab-bag (classical) or the `blk` audio libs
(header-rejects); PO pulled the **header enabler** as the goal (the 8-gate's scaffolding branch). Stood
up a NEW `src/libnaudio` tree (`mk/libnaudio.mk`, KMC -O3 per the n_audio_sc coddog pin) + vendored the
`n_synthInternals.h` DAG (PUBLIC `n_libaudio_sc.h`→`include/libnaudio`; INTERNAL
`n_synthInternals/synthInternals/n_abi.h`→`src/libnaudio`, `-I src/libnaudio` prepended so the SC
`synthInternals.h` wins over the libultra-internal copy; the leaf headers were already under
`include/libultra/PR`). Banked 4 homogeneous `@99.99` verbatim setter mirrors —
n_alSynSetPan/SetPitch/StartVoice/StopVoice — all Match FIRST build. Shared externs recovered from the
n_alSynSetPan asm (serve all 4): `__n_allocParam`=0x800A1148, `n_alEnvmixerParam`=0x8009EFE8,
`n_syn`=0x800C7DB4. **md5-candidate 179→183**; asm subsegs 106→102. Quality 0/0/0/0. seed 5 / banked
5pt (mirror, seed-only). **Gotcha:** shared-callee RENAME → stale stub `.o` (`#clean-rebuild-after-shared-header-edit`
new sub-case). Post-bank PO-directed (codegen-neutral): ch31/32 rework of the 4 .c + 3 .h; clangd +
clang-tidy enabled for `src/libnaudio`. **Cross-repo follow-up:** 4 fn names + 3 externs →
`sync_decomp_names.py --import-from-decomp`. Retro applied 3 of 3: #1 hazards shared-callee-RENAME
sub-case; #2 `pick_target.py` libnaudio profile include dirs (the `n_syn*` band now drops `blk`); #3
CLAUDE.md vendored-header-placement convention. **Next:** the n_syn* vein is now NEAR-FREE — smallest
pickable is `func_800A07B0` (pts-2, 80B) then func_800A0BB0/func_800A1320 (pts-3), a homogeneous 3-4
cap fill; the libmus `aud_thread.c` enabler (deeper DAG: defines-data + 5 BSS statics) and the
`0x8005E2C0` game-fault grab-bag remain. No carry-overs.

**S128 — `src/main/audio_mgr.c` BANKED (first nualstl3; MG64 audio libs are GAME-EMBEDDED).** Banked a
6-fn mixed game-region carve `[0x3A1D0..0x3A490)` → **md5-candidate 177→178**: 4 nualstl3 verbatim
mirrors (nuAuStlMgrInit/SchedInstall/SchedWaitFrame/SchedDoTask, nusys-2.05 nuaustlmgr.c) + 2 game bgm
fns classical (bgm_alloc_song_buffer/bgm_start_current). **HEADLINE: nualstl3/libmus are NOT linked as
standalone libraries — they are compiled INTO game audio TUs**, tight-packed (no 16-byte object
boundary) against game fns. The planned standalone `nuaustlmgr.c` carve SHA-missed at the non-16 tail
(0x3A448 = real bgm code, not nops; the #non16align gate-build canary), re-scoped (PO) to a 16-aligned
mixed carve under `src/main/`. **Band-unlock:** vendored `include/libmus/PR/libmus.h` +
`include/libnualstl/nualstl.h` (CRLF-stripped, `NU_AU_MESG_MAX=2` asm-confirmed, libmus_data.h→libaudio
cascade dropped); `-I libmus/libnualstl/libnaudio`. **Upstream libmus names (PO):**
MusInitialize/MusStartSong rom: override the wrong ghidra mus_*; MusSetScheduler + __MusIntMemMalloc
added; audio_config_init→nuAuStlMgrInit rom: override. asm subsegs 105→106. Quality 0/0/0/0 (6/6
first-build). seed 8 / realized 10 / residual +2 (re-scope/split + CRLF-header gotcha; mixed track).
**Cross-repo follow-up:** 16 new decomp symbols + 3 wrong-ghidra-name corrections (audio_config_init→
nuAuStlMgrInit, mus_initialize→MusInitialize, mus_play_song_ptr→MusStartSong) →
`sync_decomp_names.py --import-from-decomp`. Retro applied 5 of 5: #1 pick_target `game-embedded`
synthesis flag + CLAUDE.md index + docs sub-case; #2 `docs/hazards.md#crlf-vendored-header`; #3
coding-style include-sort note + `src/main/.clang-format`; #4 #wrong-ghidra-name-override generalized;
#5 #upstream-mirror-pattern header-constant-vs-asm validation. **Next:** the rest of the nualstl3/audio
region (`pick_target --lib audio`) is more game-embedded fns inside the 27KB `0x396C0`/`0x8005E2C0`
grab-bag — `game-embedded`-flagged, plan as mixed 16-aligned carves, not seed-only mirrors. No carry-overs.

**S127 — `nucontrmbmgr.c` COMPLETE (libnusys RMB-manager; the S121 spike RESOLVED).** Banked the last
stub `contRmbControl` (0x800A19E0) as C → nucontrmbmgr.c now 0 stubs → **md5-candidate 176→177**;
**libnusys mainlib is now 100% C.** **The S121 "cross-jump compiler wall" was a MISDIAGNOSIS:** MG64's
FORCESTOP is game-modified (`state=STOPPED` on `osMotorInit` FAILURE, `state=STOPPING; counter=2` only
on SUCCESS, an `if/else`), where the nusys/papermario upstream sets `state=STOPPING` UNCONDITIONALLY.
The tell (two `sb v0,6(s0)` state stores with DIFFERENT values, `li 1`/`li 2`) was in the target asm
all along; the "cross-jump shorter-by-N" symptom was the wrong body's block LAYOUT (jump.c's `minimum=1`
cross-jump needs only ONE matching insn before the epilogue label → layout, driven by the body).
User-directed systematic-debugging on the actual `mips-gcc-2.7.2/jump.c` source supplied the reframe.
One-branch fix → byte-identical `.text` + full-make ROM SHA-1 == baserom, NO compiler change.
Subseg/symbols unchanged (already `c` + curated from S121); asm subsegs 109→109. Quality 0/0/0/0 +
resolved 1 prior carry. seed 3 / realized 5 / residual +2 (classical/mixed track). **Cross-repo
follow-up:** none (`contRmbControl` already in `ghidra_symbols.txt`). Retro applied 3 of 3 (the core
`#cross-jump-tail-merge` / CLAUDE.md / carry-over corrections landed in the bank commit per PO
"fix docs now"): #1 BACKLOG/VELOCITY S121-claim SUPERSEDED markers; #2 memory
`rule-out-body-before-compiler-wall`; #3 `pick_target.py` `body-divergence-suspect` tag on sub-100
coddog-mirror rows (+ unit test + golden regen) + CLAUDE.md index row. **Next:** libnusys mined out
(last stub gone) → re-scope to the libultra audio maybe-upstream band or classical singletons.

**S126 — `nusched.c` COMPLETE (libnusys NU_DEBUG scheduler; last 3 fns banked; closes the S123/S125 spike).**
Banked `nuScEventHandler` + `nuScCreateScheduler` + `nuScExecuteGraphics` as C; nusched.c now 0 stubs →
**md5-candidate 175→176**. **HEADLINE: the S125 "nuScEventHandler is a permuter candidate" framing was
OVERTURNED — NO permuter.** The 2 mflo-hazard nops appear NATURALLY once the data + volatility scaffold is
complete. The decisive fix: `nuScRetraceCounter` is **per-TU volatile** (volatile in nusched.c, plain `u32`
in the BANKED nugfxtaskmgr.c/nucontrmbmgr.c). A shared-header `vu32` flip BREAKS nucontrmbmgr.c — its lone
`% nuContRmbSearchTime` modulo grows .text +0x10 (21M-byte ROM diff, found via per-object `.text` map-diff);
a cast-macro `(*(vu32*)&x)` fails (held-pointer). FIX = keep nusys.h non-volatile + a localized
`extern volatile u32 nuScRetraceCounter;` redeclaration in nusched.c only. `nuScCreateScheduler` = stock +
MG64 osTvType hang-guard / videoMode override / TV-format switch (`D_800B6790`); match keys = MG64 init
order + switch case order MPAL,PAL,NTSC,default; drop-static 3 thread stacks (tops alias next, S117); placed
`nuDebTaskPerfPtr`=0x800FBE10. `nuScExecuteGraphics` (287/287) = stock + `while(curAudio)`, one big int-mask
span, swap-gate arm (`D_8012F4D4`/`D_800B6784`), osSpTaskLoad+StartGo, custom SWAPBUFFER perf rotation
(`D_800B6788`%3 + `D_800B678C`++), game hooks func_8008D1DC/func_80092324; cross-fn discovery `D_800B6788`→vu32;
1 nudge (assign debTaskPerfPtr before the volatile `D_800B678C++`). Quality 0/0/0/0. seed 8; banked 8 file pt;
realized 9 (residual +1); regime mixed. Retro applied **2 of 2**: docs/hazards.md #volatile-global tell
(per-TU recipe + cast-macro anti-pattern + map-diff) / #libnusys inline-div mflo-hazard nop (reframe: not a
permuter wall). **Cross-repo follow-up:** 6 new decomp symbols (nuDebTaskPerf/nuDebTaskPerfPtr/nuScStack/
nuScAudioStack/nuScGraphicsStack/D_800B6790) → `sync_decomp_names.py --import-from-decomp`. **Phase note: the
libnusys clean-mirror band is MINED OUT** (every `--lib libnusys` row is masked game code); next sprint moves
scope. Carry-over: none.

**S125 — `nusched.c` 1/4 BANKED (libnusys NU_DEBUG scheduler; nuScExecuteAudio) + nuScRetraceCounter ID.**
Banked `nuScExecuteAudio` as C; `nuScEventHandler`/`nuScCreateScheduler`/`nuScExecuteGraphics` carry
(nusched.c PARTIAL, 3 stubs, NOT md5-candidate). md5-candidate 175→175. **HEADLINE: the S123 "4
game-customized fns / heavy classical RE" framing was PARTLY WRONG — the 4 carried fns are EXACTLY the
4 with `#ifdef NU_DEBUG` blocks, and S123 compiled the file WITHOUT NU_DEBUG so the perf machinery was
absent.** nuScExecuteAudio is a pure stock-NU_DEBUG mirror, banked first-build via (a) `#define NU_DEBUG`,
(b) `NUDebTaskPerf` struct fix (dropped the 2.07 `markerTime[10]` → auTaskCnt@0x9 / auTaskTime@0x150,
size 0x1F0 asm-confirmed), (c) drop-def `debTaskPerfPtr`=0x800D8970. **`nuScRetraceCounter` IDENTIFIED**
0x80104E68 (was `D_80104E68` in S124 nugfxtaskmgr.c) + renamed (byte-neutral). **`nuScEventHandler`
NEAR-MATCH** — byte-perfect except 2 VR4300 mflo-hazard nops (KMC gcc schedules the inline-`divu` mflo
consumer into the loop-back `j` delay slot; `#libnusys-inline-div-mflo-hazard-nop`); **permuter
candidate**, full worked body in the carry-over. Diagnosed the volatile globals + single-`frame`-local
swap-gate. Create/ExecuteGraphics untouched (budget). Quality 0 stuck-far / 0 permuter-escalated / 3
carried / 0 re-opened. seed 8; banked 0 file pt (partial); realized 10 (residual +2); regime mixed.
Retro applied **5 of 5**: docs/hazards.md #nu_debug-stock-not-custom / #libnusys-inline-div-mflo-hazard-nop
/ #volatile-global-tell / perf-struct version note (#upstream-mirror-pattern) + masking-coddog
carry-forward + 3 CLAUDE.md hazard-index rows. **Cross-repo follow-up:** nuScExecuteAudio +
nuScRetraceCounter → `sync_decomp_names.py --import-from-decomp`. Carry-over: see `## Carry-overs`
(nusched.c 3 fns, EventHandler a permuter-candidate near-free retry).

**S124 — `nugfxtaskmgr.c` BANKED (libnusys game-customized gfx task manager; FULL file, 3/3).**
Banked all 3 fns (`nuGfxTaskMgr` / `nuGfxTaskMgrInit` / `nuGfxTaskStart`) as C, ROM SHA-1 == baserom,
md5-candidate. **HEADLINE: the S123 exemption-GUARD scenario at EXECUTION, but it banked FULLY — the
apparent "regalloc wall" was a SOURCE ARTIFACT, not a compiler wall.** All 3 fns are game-customized
(the plan's "near-verbatim mirror" premise was wrong, caught by ASM-first decomp): nuGfxTaskMgr has an
MG64 retrace-pacing wait (`nuGfxRetraceWait`, sched frame-counter D_80104E68 vs last-seen D_800D8980)
+ reordered spool/callback; nuGfxTaskStart has a custom frame-buffer swap via seq-table
`D_800B67A4[nuGfxCfbCounter] → nuGfxCfb[idx]`. **nuGfxTaskMgrInit was the match key (new
`#struct-init-loop` hazard):** a DUPLICATE `nuGfxTask[cnt].msgQ = &mesgQ;` at the loop TAIL (MG64 edit
artifact) reproduces GCC 2.7.2's dual-induction-var/double-store; plus `yield_data_size =
OS_YIELD_DATA_SIZE` (0xC00, drops the 2.07 +0x10). Extern-ref drop-def data model (nugfxinit.c
pattern, NO carve): +8 data externs to `symbol_addrs` (nuGfxCfb/_ptr/Num/Counter, nuGfxTaskEndFunc,
nuRDPOutputBuf, nuDramStack, nuYieldBuf). Subseg flip [0x4840,c]. md5-candidate 174→**175**; asm
subsegs 107→**106**. Quality 0/0/0/0 (Init 1 re-attempt). seed 8 / realized 9 (residual +1, regime
mixed→full). **Cross-repo follow-up:** 11 new decomp symbols (3 fns + 8 data) →
`sync_decomp_names.py --import-from-decomp`. Retro applied 3 of 3 (#1 CLAUDE.md never-clang-format +=
src/libnusys/ + .clang-format; #2 pick_target masking-coddog seed pricing [forward-looking,
golden-inert]; #3 `docs/hazards.md#struct-init-loop-dup-store--dual-induction-var` + index row). No
carry-overs. **Remaining libnusys (next-cleanest):** the 4 nusched carry-over fns (classical,
heavily-customized scheduler: nuScCreateScheduler/EventHandler/ExecuteAudio/ExecuteGraphics ~193/255/
152/321 instrs — a multi-session classical grind, completes nusched.c → md5-candidate), then the big
`func_800328E0`/`func_ovl*` packs (unidentified, coddog-source-banked noise needing version/identify-TU
triage). **libnusys's clean-mirror band is mined out — remaining units need a classical-track plan.**

**S123 — `nusched.c` 10/14 BANKED (libnusys game-customized scheduler; 4 fns carried as
`INCLUDE_ASM`).** Banked 10 stock/small-variant scheduler fns as C (the 4 accessors nuScGetAudioMQ/
GfxMQ/SetFrameBufferNum/GetFrameRate, nuScAddClient, nuScResetClientMesgType, nuScRemoveClient,
nuScEventBroadcast, nuScWaitTaskReady, func_80028D28); the 4 heavily-customized fns (nuScCreateScheduler,
nuScEventHandler, nuScExecuteAudio, nuScExecuteGraphics) carry as INCLUDE_ASM (see Carry-overs spike).
**HEADLINE: the plan premise was WRONG — nusched.c is a game-customized scheduler, not a verbatim
mirror.** coddog 99.99 was STRUCTURAL (shared nusys skeleton, divergent bodies). ASM re-assessment
caught it pre-write; the user-hinted ~/n64sdk multi-version triage confirmed no stock 1.10/2.00/2.07
matches the custom fns and pinned MG64 ≈ 2.07-minus-nuVersion[] (baserom has NO "NuSystem" string;
nuScAddClient carries the 2.06/2.07 PRENMI-dispatch). bank-stock-carry-custom (S121 generalized):
10 clean fns first-build SHA, ROM green. Enablers: subseg flip `[0x37B0,asm]`→`[0x37B0,c,libnusys/
mainlib/nusched]`; drop-def `nusched`=0x801B8380 size:0x680 + `nuScPreNMIFlag`=0x800FBD94 (bss, no
carve); 4 accessor names → symbol_addrs. md5-candidate 174→**174** (file partial, 4 stubs); asm
subsegs 108→**107**. Quality: spike 4 / carried 4 / 0 stuck-far/permuter/re-opened. seed 13 / banked
0pt (regime mixed, per-file all-or-nothing — file partial; +10 matched-fn count is the value signal).
**Dependency-order win (#4):** banking nusched PLACES `nusched`@0x801B8380, so the next-cleanest
libnusys unit `nuGfxTaskMgr` (classical, game-modified) loses its `nusched` `refs-unplaced` blocker —
realized even from a partial bank. **Cross-repo follow-up:** 6 new decomp symbols (4 accessor fns +
nusched/nuScPreNMIFlag data) → `sync_decomp_names.py --import-from-decomp`. Retro applied 4 of 4 (#1
CLAUDE.md exemption-GUARD on coddog-99.99-structural + non-lib-func_-callee tell [pick_target pricing
= tracked follow-up] + #3 mixed bank-stock-carry-custom note; #2 `docs/hazards.md#upstream-mirror-pattern`
libnusys multi-version-triage; #4 this dependency note). **Remaining libnusys (next-cleanest):** the 4
nusched carry-over fns (classical), then `nuGfxTaskMgr` (pts-8, classical/game-modified, now nusched-
unblocked), then the big `func_800328E0`/`func_ovl*` packs (unidentified, coddog-source-banked noise).
**Note: libnusys's clean-mirror band is mined out — remaining units need a classical-track plan, not
the mirror default.**

**S122 — `nusimgr.c` BANKED (libnusys SI-manager drop-static mirror + same-TU leaf; the S120-split
carry-over).** Banked the whole `[0x7DB80]` subseg (0x800A2780-0x800A2A70, 6 fns) into ONE
md5-candidate: `nuSiMgrInit`/`nuSiSendMesg`/`nuSiMgrStop`/`nuSiMgrRestart`/`nuSiMgrThread` (verbatim
nusys-2.07) + `func_800A2780`. **The S120 carry-over's two open questions both resolved at execution.**
(1) **The leaf is SAME-TU, not foreign:** `func_800A2780` is `return siMgrStack;` — its 0x800F77D0
target is the base of nusimgr.c's own file-static `siMgrStack` (siMgrThread 0x800F7620 + 0x1B0); a
static is file-local, so the leaf MUST be nusimgr.c (an MG64-added leading accessor the 2.07 upstream
lacks), banked with it. (2) **Version-rev `needs-define`:** the byte-verbatim mirror SHA-missed by
exactly one byte (vram 0x800A27E0 `li a1,6`→`5`, the `osCreateThread` thread-id) because vendored
nusys-2.07 `NU_CONT_THREAD_ID=6` but MG64's rev compacts the controller/SI thread to slot 5 — fixed in
`include/libnusys/nusys.h` (grep-confirmed nusimgr.c is the SOLE tree consumer → collateral-free;
clean-rebuild). Drop-static `nuSiMesgBuf`=0x800F7600 / `siMgrThread`=0x800F7620 / `siMgrStack`=0x800F77D0
+ drop-def `nuSiMgrMesgQ`=0x8012D408 (all bss → SHA-neutral, no carve; `nuSiMesgQ`/`nuSiCallBackList`
already placed). md5-candidate 173→**174** (all 174 src .c stub-free); asm subsegs 109→**108**. Quality
0/0/0/0. seed 5 / banked 5pt; regime mirror. **Cross-repo follow-up:** 7 new decomp-side symbols (3 fn
nuSiMgrStop/Restart/Thread + 4 data nuSiMesgBuf/siMgrThread/siMgrStack/nuSiMgrMesgQ) →
`sync_decomp_names.py --import-from-decomp`. Retro applied 3 of 3 (#1 `#needs-define` version-rev
single-immediate-byte sub-case + CLAUDE.md index row; #2 leaf-returns-static SAME-TU rule on the
`unattrib-leaf` paragraph; #3 near-free-retry checklist 6th item = nusys header-value reconciliation).
**Remaining libnusys (next-cleanest):** `nuGfxTaskMgr` (pts-8 `single-file-pack:3fn`, file-static +
defines-data + refs-unplaced on the still-asm `nusched`/`rspbootTextEnd` + `jal-count-mismatch:7vs11`),
then the `nuScCreateScheduler` / `func_800328E0` multi-fn packs (the latter's coddog is stale). The
`contRmbControl` cross-jump WALL stays carried (compiler-blocked). _(SUPERSEDED S127: NOT a wall — a
game-modified FORCESTOP body; banked, file md5-candidate.)_

**S121 — `nucontrmbmgr.c` 8/9 BANKED (libnusys RMB-manager verbatim mirror; `contRmbControl` carried
as `INCLUDE_ASM`).** _(SUPERSEDED S127: the `#cross-jump-tail-merge` "COMPILER WALL" framing below was a
MISDIAGNOSIS — `contRmbControl` was a game-modified FORCESTOP body (`state=STOPPED` on `osMotorInit`
error), banked S127 with a one-branch fix; the file is now a pure-C md5-candidate. The compiler-wall RE
below is preserved as the historical record. See the S127 entry.)_ Banked the RMB manager's 8 verbatim libnusys fns as C (`nuContRmbMgrInit`/`Remove`,
`contRmbRetrace`, `contRmbCheckMesg`, `contRmbForceStop`/`ForceStopEnd`/`Start`/`StopMesg`); the 9th,
`contRmbControl` (0x800A19E0), is carried as `INCLUDE_ASM`. **HEADLINE: the verbatim-mirror exemption's
first PARTIAL bank — a `#cross-jump-tail-merge` COMPILER WALL.** Planned a clean 9-fn pure-C
md5-candidate (S116 .data-carve template), but `contRmbControl` SHA-missed: the project gcc 2.7.2
merges two byte-identical `j .L_ret; sh v0,4(s0)` counter-store tails (STOPPING `counter--` + FORCESTOP
`counter=2`) that MG64 keeps UNMERGED (build 0x1EC < target 0x1F8). A multi-session user-directed RE
proved it unbankable with available tooling: byte-identical tails ⇒ permuter-immune (145k iters,
plateau 220); and NO available binary reproduces the divergence — real-KMC (drmario64) + decompals
FSF-2.7.2 (== `tools/cc/gcc`, byte-identical) BOTH merge, SN 2.7.2 + gcc 2.8.1 (papermario) mis-allocate
(5 regs/40B frame) AND merge, and the target's selective pattern (osMotor merged, counter+FORCESTOP not)
is NON-MONOTONIC in any cross-jump knob. **PO chose option B** — bank the 8, carry the 1, keep the ROM
green. Enablers: subseg `[0x7CD60,asm]`→`[0x7CD60,c,nucontrmbmgr]`; `.data` carve `[0xA31D0,.data,
nucontrmbmgr]` (0x30B: SearchTime/funcList/CallBack); `nuContRmbCtl` drop-to-extern (0x80104F50); 4
msg-handler names added to `symbol_addrs` (ForceStopMesg=0x800A1DA0, ForceStopEndMesg=0x800A1DE8,
StartMesg=0x800A1E18, StopMesg=0x800A1EA4); `contRmbControl` fwd-decl `extern` (asm `glabel` is
`.globl`). matched-fn **+8**; md5-candidate 173→**173** (file not candidate, 1 stub); asm subsegs
110→**109**. Quality: spike 1 / permuter 1 / carried 1 / re-opened 1. seed 8 / banked 0pt (per-file
all-or-nothing — file partial); regime mirror. **Cross-repo follow-up:** 4 new decomp-side symbols →
`sync_decomp_names.py --import-from-decomp`. Retro applied 3 of 4 (#2 `#cross-jump-tail-merge` hazard +
#4 `#permuter-setup-for-kmc-toolchain-mirrors` + CLAUDE.md index rows; #5 sub-100-coddog exemption hedge;
#1 NOT selected). **`contRmbControl` → Carry-overs (spike: cross-jump wall, banks only with a
cross-jump-correct 2.7.2 build). Remaining libnusys (next-cleanest):** the S120 carry `[0x7DB80,asm]`
(`func_800A2780` leaf + `nusimgr.c`), then `nuGfxTaskMgr` (pts-8 `single-file-pack:3fn`) and the
`nuScCreateScheduler` multi-file packs.

**S120 — `nucontgbpakfwrite.c` BANKED (libnusys GBPak near-verbatim block-reorder mirror; S119
sibling; GBPak family COMPLETE).** Banked `nuContGBPakFwrite` (0x800A2570), the last GBPak-family asm
leaf, decomposed out of the pts-8 c-combined:2file `[0x7D970]` pack at the rom-0x7DB80 file boundary.
**HEADLINE: sibling-replay banks the near-verbatim FIRST-BUILD.** nuContGBPakFwrite is the structural
twin of S119's `nuContGBPakFread` — same MG64-specific block reorder (the RAM-enable block
`bzero`/`data[31]=0xa`/`nuContGBPakWrite` runs BEFORE `nuContGBPakCheckConnector`, `ram=0` in the
range-check `beqz` delay slot, `jal CheckConnector` at skip-label 0x800a2610), and the same benign
`nuContGBPakRead`/`Write`→`ReadWrite` macro `jal-count-mismatch:5vs10`. Applying the CheckConnector ↔
RAM-enable swap UP-FRONT (from the asm + the S119 precedent) on the verbatim nusys-2.07 cp landed
insn-identical, full-make ROM SHA-1 == baserom, **0 iteration** (vs S119's discover-via-re-attempt).
Zero symbol adds (name curated; all 5 callees placed). md5-candidate 172→**173** (all 173 src .c
stub-free); asm subsegs 110→110 (split net-zero). Quality 0/0/0/0, **0 re-attempt**. seed 3 → realized
2 (residual −1, the first sibling-known verbatim-first-try). **Cross-repo follow-up:** none. Retro
applied 3 of 3 (#1 `pick_target.py block-reorder-sibling:<file>` tag + `BLOCK_REORDER_FAMILIES` + unit
test + golden regen; #2 `unattrib-leaf:0x<vram>` lone-straddler flag for a c-combined split + unit
test; #3 `VELOCITY.md` sibling-known per-fn-provenance refinement). All three forward-looking (zero
current behavior; the only golden churn was the legitimate S120 split row). **Remaining libnusys
(next-cleanest):** the S120 carry `[0x7DB80,asm]` = `func_800A2780` leaf (unidentified, returns
&0x800F77D0) + `nusimgr.c` (5 fns, drop-static + `nuSiCallBackList` drop-def, see Carry-overs); then
`nuGfxTaskMgr` (pts-8 `single-file-pack:3fn`, file-static + ~14 defines-data, `jal-count-mismatch:7vs11`
no-coddog — a genuine structural near-verbatim, version-check AND structural) and the `nuContRmb*`/
`nuScCreateScheduler` multi-file packs.

**S119 — `func_800A2090.c` + `nucontgbpakfread.c` BANKED (libnusys GBPak/RMB region cleared).** Banked
the S118 near-free carry-over `func_800A2090` (0x7D490, trivial classical 8B empty leaf
`void func_800A2090(void){}` → KMC `jr $ra;nop`; the 8B subseg tail is the `trailing-pad:8B@16` to
nucontgbpakmgr@0x7D4A0, landed clean) plus `nuContGBPakFread` (0x7D7A0). **HEADLINE: a "mirror" that was
really a near-verbatim BLOCK REORDER.** The verbatim nusys-2.07 cp built+linked clean but ROM SHA-MISSED;
the in-tree vs asm objdump showed the SAME 117 insns REORDERED — MG64's per-file nusys rev runs the
RAM-enable block (`bzero`/`data[31]=…`/`nuContGBPakWrite`) BEFORE `nuContGBPakCheckConnector` (`ram=0` in
the range-check `beqz` delay slot, `jal CheckConnector` at the skip-label), but EVERY archived
1.20/2.00/2.05/2.06/2.07 is CheckConnector-first. Hand-swapping the two source blocks → insn-identical,
full-make ROM SHA-1 == baserom. The `jal-count-mismatch:5vs9` was an UNRELATED macro artifact (nusys.h
`nuContGBPakRead`/`Write` → `nuContGBPakReadWrite`; 9 asm jals == 9 expanded call sites); the un-refuted
tell was the "no coddog" flag (a reorder breaks the structural fingerprint). Zero symbol adds (func_ name
kept; `nuContGBPakFread`@0x800A23A0 + all callees pre-curated). md5-candidate 170→**172** (all 172 src .c
stub-free); asm subsegs 112→110. Quality 0/0/0/0 (1 re-attempt). **Cross-repo follow-up:** none. Retro
applied 3 of 3 (#1 `pick_target.py` prices jal-mismatch + no-coddog at mirror-floor +1 [near-verbatim
risk] + unit test + golden regen, nuGfxTaskMgr 5→8; #2 `docs/hazards.md#near-verbatim-mirror-jal-count-mismatch`
block-reorder sub-case + CLAUDE.md index row; #3 `VELOCITY.md` near-verbatim-reclassification rule).
**Remaining libnusys (next-cleanest):** the GBPak band's last asm leaf is the pts-8 `nuContGBPakFwrite`
c-combined:2file pack (0x7D970, nucontgbpakfwrite|nusimgr); next is `nuGfxTaskMgr` (now pts-8 after the
S119 #1 bump, `single-file-pack:3fn`, file-static + ~14 defines-data, `jal-count-mismatch:7vs11` no-coddog
— a genuine structural near-verbatim, version-check AND structural) and the `nuContRmb*`/`nuScCreateScheduler`
multi-file packs.

**S118 — `nucontrmbmodeset.c` + `nucontrmbforcestop.c` BANKED (libnusys RMB mirror pair, per-file
version split).** Split the smallest remaining libnusys block, the c-combined `[0x7D3B0,asm]` RMB pack
(240B), 3-way at file boundaries and banked its 2 verbatim mirrors, both first-build ROM SHA-1 ==
baserom, 0 iteration. **HEADLINE (extends S117): nusys version is per-file even WITHIN the RMB
family.** `nuContRmbForceStop` is code-identical 2.00..2.07 → verbatim 2.07-sdk cp. `nuContRmbModeSet`
is NOT: nusys-2.05 wrapped the body in an `osSetIntMask(OS_IM_NONE)`/restore pair (2 jals, non-leaf)
kept through 2.07, but MG64's asm is a **0-jal leaf** = the pre-2.05 (2.00) variant. Mirror = 2.07-sdk
verbatim minus the 3-line int-mask wrapper (the near-verbatim drop, English comments + 2.00 leaf code);
resolved pick_target's `jal-count-mismatch:2vs0` (a version artifact: it read the 2.05+ source's 2
osSetIntMask jals vs the 0-jal asm). Zero symbol adds (both names pre-curated; `nuContRmbCtl`@0x80104F50
+ `nuSiSendMesg`@0x800A2824 placed). md5-candidate 168→**170** (all 170 src .c stub-free). Quality
0/0/0/0. Retro applied 3 of 3 (#1 `docs/hazards.md#near-verbatim-mirror-jal-count-mismatch` nusys
int-mask-wrapper anchor + provenance; #2 `pick_target.py` libnusys `(version-artifact?)` annotation +
unit test + the 4 stale pick_target goldens caught up [red since pre-S114]; #3 this carry note).
**Cross-repo follow-up:** none (0 new symbols). `func_800A2090` (8B empty stub) left as `[0x7D490,asm]`
per PO scope → see Carry-overs (near-free retry). **Remaining libnusys (next-cleanest):** `nuGfxTaskMgr`
(pts-5 `single-file-pack:3fn`, file-static + ~14 defines-data, `jal-count-mismatch:7vs11` — asm has
MORE than the C, the opposite of S118's version-artifact, so version-check AND structural; the wrapper
direction does NOT explain it); the messy ones are `nuContGBPakFread` (pts-2 1fn, `jal-count-mismatch:5vs9`,
no coddog) and the `nuContRmb*`/`nuContGBPakFwrite` multi-file packs (1280B+, unidentified `func_`s).

**S117 — `src/libnusys/mainlib/nucontmgr.c` BANKED (libnusys **2.05** .data-carve + drop-def hybrid
mirror).** The NuSYS Controller Manager (`nuContMgrInit`/`Remove` + `nuContDataClose`/`Open` + 5 static
dispatch leaves `contReadData`/`contQuery`/`contRetrace`/`contRead`/`contReadNW`) banked, the S116
sibling, full-make ROM SHA-1 == baserom. pts-8 8-gate fired → verbatim-mirror exemption. **HEADLINE
finding: MG64's libnusys is NOT uniformly nusys-2.07.** coddog-sweep-nusys (2.07 ref) matched @99.99
STRUCTURALLY, but the 2.06/2.07 rewrite changed `nuContMgrInit`'s loop from `for(...;cnt++){...;bitmask
<<=1;}` to the comma-operator `for(...; bitmask<<=1, cnt++)`, which KMC compiles 2 instrs shorter
(`.text` 0x330 vs 0x340), cascading a ROM-wide -0x10 shift. Diagnosed by compiling every nusys revision:
2.00≡2.05 (code-identical, JP vs EN comments) give 0x340; used English 2.05. **Hybrid mirror:** `.data`
carve (`nuContReadFunc`@0x800C7E40 + `funcList`@0x800C7E44 + `nuContCallBack`@0x800C7E58, `.o` `.data`
SECTION 0x30 w/ 0xC 16-align END pad → carve 0xA3240..0xA3270 [first try 0x24 content → overlap]) out of
`main_data` 3-way split; BSS drop-def the 6 scattered-COMMON globals → extern. **entry.s hasm sync:**
naming `nuContNum`=0x8010C2D0 evicted `D_8010C2D0` that `_start` uses as the boot stack top
(== &nuContNum, highest BSS) → byte-neutral D_→name edit. 3 gotchas resolved in-sprint, no carry; first
mirror-track sprint with real diagnostic friction. md5-candidate 167→**168** (all 168 src .c stub-free).
Retro applied 3 of 3 (nusys-version-divergence + version-hunt playbook `#coddog-cross-ref`; carve-extent =
`.o .data` section size `#defines-data`; hasm-referrer sync `#file-static`). **Cross-repo follow-up:** 13
new decomp symbols (5 fn + 8 data) → propagate via `sync_decomp_names.py --import-from-decomp`. **Remaining
libnusys (next-cleanest):** `nuGfxTaskMgr` (pts-5 `single-file-pack:3fn`, file-static + ~14 defines-data,
`jal-count-mismatch:7vs11` — version-check it too); the messy ones are `nuContGBPakFread` (pts-2 1fn,
`jal-count-mismatch:5vs9`, no coddog) and the `nuContRmb*` multi-file packs (240B+, unidentified `func_`s).

**S116 — `src/libnusys/mainlib/nucontgbpakmgr.c` BANKED (libnusys .data-carve mirror, FIRST libnusys
`.data` carve).** The 64GB Pak Manager (`nuContGBPakMgrInit`/`Remove` + 6 static `contGBPak*` dispatch
leaves) banked as a verbatim nusys-2.07 mirror (coddog 99.99, `single-file-pack:8fn`), first-build ROM
SHA-1 == baserom, 0 iteration. pts-8 8-gate fired → verbatim-mirror exemption (now extended to cover
`.data`-carve mirrors, not just drop-def). The file DEFINES initialized `.data` (`funcList[8]`
@0x800C7E00 0x20 + `nuContGBPakCallBack`@0x800C7E20 0xC, `.o` `.data` 0x30) carved **mid-`main_data`**
via a 3-way split (`main_data | [0xA3200,.data,nucontgbpakmgr] | main_data_2`). **Gotcha (resolved
in-sprint, no carry):** the gate base-only share-check wrongly read "sole-referrer"; the still-asm
sibling `nuContGBPakFwrite` (0x7D970) reads `nuContGBPakCallBack.func` at base+4 (`D_800C7E24`) —
caught by the link error. DROP impossible (callback→funcList→6-statics ref-chain), so carve forced;
resolved by naming the base canonical+sized so splat renders the sibling as `nuContGBPakCallBack + 0x4`
(see new `docs/hazards.md#defines-data` S116 paragraph). All 8 names pre-curated, all 9 callees placed.
md5-candidate 166→**167** (all 167 src .c stub-free). Retro applied 4 of 4 (3 `#defines-data` refinements
+ 1 CLAUDE.md exemption note). **Cross-repo follow-up:** 1 new decomp data symbol (`nuContGBPakCallBack`)
→ propagate via `sync_decomp_names.py --import-from-decomp`. **Remaining libnusys (next-cleanest):**
`nuContMgrInit` (pts-8 `pack:9fn` `coddog-mirror:nucontmgr.c@99.99`, `drop-static-mirror:5bss` — the
S115/S87/S90 drop-static pattern) and `nuGfxTaskMgr` (pts-5 `single-file-pack:3fn`, file-static +
many defines-data); the messy ones are `nuContGBPakFread` (pts-2 1fn but `jal-count-mismatch:5vs9`,
no coddog match) and the `nuContRmb*` pack (240B but multi-file + unidentified `func_800A2090`).

**S115 — `src/libnusys/mainlib/nugfxthread.c` BANKED (libnusys drop-static mirror).** `gfxThread` +
`nuGfxThreadStart` (the NuSYS graphics thread + its starter) banked as a verbatim drop-static mirror of
nusys-2.07 `nugfxthread.c` (coddog 99.99, `single-file-pack:2fn`), first-build ROM SHA-1 == baserom, 0
iteration — the proven S87/S90 drop-static pattern. Bodies byte-verbatim; the 4 header-declared globals
drop-def (nusys.h `extern`) + the 2 file-statics drop-static-to-extern, all placed at asm-recovered
`main_bss` vrams: `nuGfxPreNMIFunc`=0x800C7DC4, `nuGfxMesgBuf`=0x800E72D0/0x20, `nuGfxThread`=
0x800E72F0/0x1B0, `GfxStack`=0x800F54A0/0x2000, `nuGfxMesgQ`=0x801052D0/0x18 (`nuGfxFunc`@0x800C7DC0
pre-placed S19). All 5 callees pre-placed, both fn names pre-curated → gate = 1 yaml flip. No rodata
carve (2-case switch, no jtbl). md5-candidate 165→**166** (all 166 src .c stub-free). Retro applied 2
of 2 (2 `docs/hazards.md` drop-static notes: the stack-top-named-symbol tell + the 2nd `<n>bss`
under-count class). **Cross-repo follow-up:** 5 new decomp data symbols → propagate via
`sync_decomp_names.py --import-from-decomp`. **Remaining libnusys (next-cleanest):** `nuContGBPakMgrInit`
(pts-8 single-file-pack:8fn, 1 drop-def `nuContGBPakCallBack`) and the `nuContRmb*` pack (240B but
multi-file + unidentified `func_800A2090`).

**S114 — `nusicallbackadd.c` + `nusicallbackremove.c` BANKED (libnusys nuSi mirror pair).** _[BACKFILLED
at the S115 review — banked commit 9690ad8 but its `/sprint-review` never ran.]_ `nuSiCallBackAdd` +
`nuSiCallBackRemove` banked as a verbatim nusys-2.07 mirror pair (coddog 99.99 each): `[0x7DE70,asm]`
split at the 0x7DF10 file boundary into `nusicallbackadd` + `nusicallbackremove`; recovered
`nuSiCallBackList`=0x800C7E30 (defined by the un-decompiled `nusimgr.c`). Full-make ROM SHA-1 ==
baserom. md5-candidate 163→**165**. No carry-overs. (Ledger reconstructed from the commit at the S115
gate; the committed seed ~4pt is reconstructed and the suggestion buffer was lost.)

**S113 — `src/libkmc/sin.c` BANKED (libkmc C-band COMPLETE).** Verbatim mirror of `libkmc/src/sin.c`
(`_xsincos`+`sin`+`cos`+`tan`, CORDIC fixed-point, same XLONG/MBIT/CBIT idioms as atan.c) at the libkmc
`-O` profile. The near-free retry of S112 atan.c: the carry-over's 5-point completeness checklist
replayed **verbatim-correct, 0 rework, 0 iteration**. Gate enabler = ONE text flip `[0x8E660,asm]`→
`[0x8E660,c,libkmc/sin]`; execution = verbatim cp + carve `[0xADCA0,rodata]`→`[0xADCA0,.rodata,
libkmc/sin]` (whole generic subseg = 0x90 B = 18 doubles, exact-bound, the atan.c [0xADC40] precedent).
**ZERO new symbols** — all 4 names curated, `_atbl`=0x800C9690 / `__fixunsdfdi`=0x800B3C20 /
`__floatdidf`=0x800B3D40 placed (S112/S109); asm-confirmed (asm/8E660.s) the only jals are those 3 +
`_atbl` + the FP pool; no `__matherr` (unlike atan), no `__fixdfdi` (KMC GCC emits `__fixunsdfdi` for
the signed `XLONG=double*MBIT` idiom). md5-candidate 162→163. Full-make ROM SHA-1 == baserom (first
build). **libkmc is now FULLY MINED — only the `mmuldi3`/`mcvtld` `hasm` math modules remain (permanent
asm). The next work is the non-libultra-region coddog targets (`os/settime.c`, `sp/spriteex2.c`,
`audio/synthesizer.c`) or a classical-track sprint.**

**S112 — `src/libkmc/atan.c` BANKED (first libkmc C-mirror).** Verbatim mirror of `libkmc/src/atan.c`
(`_xatan`+`atan`+`atan2`, CORDIC fixed-point on `long long` XLONG + doubles) at the libkmc `-O` profile
(`LIBKMC_CFLAGS` = `-O2`→`-O`), first-build clean (0 iteration, all 4 flagged risks held). Enablers:
placed `_atbl`=0x800C9690 (shared atbl.c CORDIC table — the whole `[0xA4A90,data]` 0xBD8 blob = the
table, a symbol-place NOT a carve), vendored `cordic.h`, carved `[0xADC40,rodata]`→`[0xADC40,.rodata,
libkmc/atan]` (generic-subseg-bound 1-line flip, 0x60 B). All callees pre-placed (`__floatdidf`/
`__fixunsdfdi`/`__matherr`); KMC GCC inlines the long-long shifts (0 shift-helper jal). md5-candidate
161→162. Full-make ROM SHA-1 == baserom. **`sin.c` followed as the near-free sibling (S113) →
libkmc is now fully mined (only mmuldi3/mcvtld `hasm` remain).**

**S111 — libultra `.data`/`.rodata` resolution sweep BANKED (≈18 TUs).** Carved every attributable
anonymous block in the libultra `.data`/`.rodata` region (0xA32D0–0xADCA0) into named `libultra/<tu>`
subsegs: 12 placed drop-def restores (initialize/sl/controller/random/seteventmesg/siacs/timerintr/
contpfs/contramread/rotaterpy/vimgr/gbpakreadid), 6 vendored data TUs (thread.c & vi.c data-only;
vitbl.c 56-entry osViModeTable + 3 vimodes verbatim), the exceptasm tables UN-stripped + carved (the
S107 strip was reversible once `.text` re-exports the `.L<addr>` labels), and `gu/libm_vals.s`
(`__libm_qnan_f`). New `docs/hazards.md#data-rodata-carve` playbook + `tools/verify-rom.sh` gated-verify
helper (after an ungated `sha1sum` masked 3 build breaks behind a stale-green ROM). **Remaining libultra
data/rodata = the libkmc `atan.c`/`sin.c` C-mirror unit** (A4A90 `_atbl`/ADC40/ADCA0 carve when those
fns are decompiled — see Carry-overs). Full-make ROM SHA-1 == baserom.

**S110 — re-home + `os/parameters.s` BANKED (mirror).** Re-homed `osSpTaskYielded`
(`func_800AB600.c`→`io/sptaskyielded.c`) + `__osGetCurrFaultedThread.c`→`os/getcurrfaultthread.c` from
src/main/ (-O2) to src/libultra/ (-O3) as verbatim mirrors; resolved 0x8AC20 as `os/parameters.s`
(`.space 0x60` + ABS() N64 OS globals) vendored `hasm`. asm subsegs 119→118.

**S109 — `mcvtld.s` BANKED (asm-mirror hasm, first KMC-as sub-lane).** libkmc soft-float
double↔long-long cvt TU (`__fixdfdi`/`__fixunsdfdi` @0x8F020 + `__floatdidf` @0x8F140) vendored
verbatim via the KMC-assembler explicit-rule path (the `mmuldi3.s` precedent), the two adjacent asm
subsegs merged to one `[0x8F020,hasm]`. Two `li 0xffffffff`→`addiu $X,$0,-1` encoding edits (the
mmuldi3 divergence); `.include "mips_as.h"` vendored to `src/libkmc/mips_as.h` + `-I src/libkmc`;
0 symbol adds. Full-make ROM SHA-1 == baserom; asm subsegs 121→119, hasm 25→26. The S108 #2
deferred "multi-root libkmc asm-TU index" follow-up is now DONE (S109 #1: `intrinsic-likely:<tu>.s
(kmc-as)` tag + `build_kmc_asm_tu_index`). **Remaining un-mirrored libultra-region asm subsegs (PO
context for the next gate):** `0x8E110` `_xatan`+`atan`+`atan2` (libkmc `atan.c`, **C-mirror**,
3-fn single-file-pack, refs external rodata `_atbl[]`/`D_800C9690`); `0x8E660` `_xsincos`+`sin`+
`cos`+`tan` (libkmc `sin.c`, **C-mirror**, 4-fn single-file-pack); `0x8F250`
`audio_sched_thread_entry` (cp0-asm intrinsic, identify-TU). The libkmc soft-float asm band is now
exhausted (mmuldi3 + mcvtld done); the next libkmc work is the atan/sin **C-mirrors** (doubles +
external rodata carves + KMC `-O` profile, real classical/mirror risk), or the non-libultra-region
coddog targets (`os/settime.c`, `sp/spriteex2.c`, `audio/synthesizer.c`).

**S108 — `os/interrupt.s` BANKED (asm-mirror hasm) + remaining libultra-region asm inventory.**
`__osDisableInt`+`__osRestoreInt` (0x8B900) vendored verbatim from ultralib (VERSION_J branch), the
clean no-jtbl/no-rodata sibling of S107 exceptasm; asm subsegs 122→121. The scope's other half —
`llcvt.c` — is **closed as not-linked** (workhorse `__floatdidf`/`__fixunsdfdi` present in the
libkmc/libgcc band, wrapper TU absent; the `__d_to_ll @99.99` rows are reloc-masked FPs — see
`docs/hazards.md#coddog-cross-ref` S108 note). **Remaining un-mirrored libultra-region asm subsegs
(PO context for the next gate):** `0x8E110` `_xatan` (libc math, ~0x440 B); `0x8E660` + `0x8F020`
`__fixunsdfdi` + `0x8F140` `__floatdidf` (libkmc/libgcc soft-float, ~0x690 B total). These are the
next asm-mirror / classical candidates; the non-libultra-region coddog targets (`os/settime.c`,
`sp/spriteex2.c`, `audio/synthesizer.c`) remain the other open work.

**Remaining libultra hazard map (S38; corrected S53; sprintf closed S54).** ~~**blk** —
`alHeapDBAlloc`~~ **BANKED S53** (PR-band false-blk fixed via `LIB_EXTRA_INCLUDE_DIRS`, S53 #1).
~~`sprintf` (file-static + pack:2fn + needs-header:xstdio.h,string.h)~~ **BANKED S54** — all three
were false-flags: `file-static` was the `static proutSprintf(...)` *function* proto (no BSS hazard;
detector now skips static functions, S54 #1), `pack:2fn` was the whole upstream file (combined
subseg, S40 ldiv pattern), `needs-header` was the vendorable class (xstdio.h source-private +
string.h/memory.h compiler companions, vendored; detector now tags `(vendorable)`, S54 #2).
**`--lib libultra` filter is misleading (S55 correction).** `--lib` substring-matches subseg
*paths/names*, but un-flipped libultra asm subsegs lack a `libultra/` path qualifier → they surface
under `upstream none` and `--lib libultra` returns empty. Survey the full band by the `upstream`
column instead (plain `pick_target.py`, no `--lib`). That survey found one remaining clean leaf:
**`gu/perspective` (guPerspectiveF + guPerspective) — BANKED S55** (verbatim mirror, 8-double
rodata-sibling split; the S55 #1 fix made `pick_target` size the rodata extent whole-subseg so a
pack sibling's pooled literals are no longer undercounted). **The libultra cheap-mirror band is now
truly exhausted.** What remains: `intrinsic-likely` register/FPU shims (`osGetCount`, `sqrtf`,
`__osGetCause`, `__osGetSR`, `__osSetCompare`, `osWritebackDCacheAll`, `func_800ACCC0`) →
classical-or-hasm, NOT mirrors; plus the heavy recover-extern/classical leaves below. Next libultra
progress is a classical-track sprint (v2 realized tier) or a regime/scope change — an
intrinsic-shim→hasm housekeeping pass, or libnusys/libkmc fillers — decided at the next gate.
**Next-cleanest candidates (post-S59, by pts):** ~~`osGbpakCheckConnector`~~ **BANKED S59** — turned
out a clean *verbatim C mirror*, NOT classical: no jal-mismatch / unplaced refs, and its only hazard
`needs-header:controller.h` was a 0-work no-op (already vendored at `include/libultra/internal/` and
on the io band `-I` set; S59 #1 now tags this `(already-vendored)`). `__osContRamRead`/`__osContRamWrite`
(pts 3, jal-mismatch 22-23vs10 → classical, unplaced SI callees; header hazards now correctly show
`(already-vendored)`); `guRotateF` (pts 5, 2fn pack, data-static + calls-unplaced, genuine
`../gu/guint.h` companion-copy still `(vendorable)`). NOTE: S59 #1 re-tagged the warm io/cont/pfs/
vimgr/timer band — a `(already-vendored)` header is no longer an enabler, so these targets' true cost
is set by their jal-mismatch/unplaced-ref/file-static load, not the (no-op) header flag.
**defines-data (classical)** — `__osViInit` (3pt, defines placed BSS globals, static drops
needed). **jal-count-mismatch / classical-likely** — `osCartRomInit` (`21vs5`, stripped impl);
`__osEPiRawWriteIo` (`2vs0`, investigate at gate). **Packs (split at upstream boundary)** —
`osCreateThread` (2fn), `ldiv` (2fn), `osSendMesg` (2fn), `__osPiRawStartDma` (3fn),
`osSpTaskLoad` (2fn), all ≤8pt. **Large packs (8-gate: decompose)** — `__osTimerServicesInit`
(4fn), `_Litob` (4fn), `alEnvmixerPull` (8fn), `osCreateScheduler` (13pt, must decompose into
sub-sprints).

- **Sprint 0 (pre-Scrum): 7 files BANKED (8 C-body matches + 1 hasm).** Before this overlay
  existed: `src/libkmc/matherr.c` (`_matherr`), `src/libkmc/rand.c` (`rand`+`srand`,
  validated the libkmc `-O` profile carve), `src/libultra/shared/system/afterprenmi.c`
  (`osAfterPreNMI`), `src/libultra/monegi/thread/getthreadpri.c` (`osGetThreadPri`),
  `src/libultra/monegi/si/si.c` (`__osSiDeviceBusy`), `src/libultra/monegi/vi/vigetcurrcontext.c`
  (`__osViGetCurrentContext`), `src/libultra/monegi/ai/ai.c` (`__osAiDeviceBusy`). md5-candidate 0→7.

- **Sprint 1: 1 file BANKED — `src/libultra/monegi/rdp/dp.c` (`__osDpDeviceBusy`), libultra
  upstream-mirror.** md5-candidate 7→8; matched 9→10/2090 (~0.48%). First sprint under the
  Scrum overlay. The gate build-check caught a system-wide missing-`mips-linux-gnu-cpp`
  toolchain regression (was silently emptying every asm object); fixed by reinstalling
  `cpp-mips-linux-gnu`, now guarded in the Makefile (`pipefail` + missing-`$(CPP)` abort).
  No carry-overs.

- **Sprint 2: 1 file BANKED — `src/libultra/monegi/message/createmesgqueue.c`
  (`osCreateMesgQueue`), libultra upstream-mirror.** md5-candidate 8→9; matched 10→11/2090
  (~0.53%). 7th `monegi/` mirror. New data extern `__osThreadTail`=0x800C8220 recovered from
  the fn's own asm (`lui/addiu`) and added add-only to `symbol_addrs.txt`. Clean first-pass
  match (0/0/0/0). Retro codified the asm-data-recovery pattern + an include-resolvability
  hazard (audio band needs `-I include/libultra/PR`). No carry-overs.

- **Sprint 3: 1 file BANKED — `src/libultra/monegi/vi/visetmode.c` (`osViSetMode`), libultra
  upstream-mirror.** md5-candidate 9→10; matched 11→12/2090 (~0.57%). 8th `monegi/` mirror, 2nd
  in the `vi/` band (sibling of `vigetcurrcontext.c`). **Zero symbol recovery** — all linker refs
  pre-resolved; only enabler was a trivial companion-header copy (`include/libultra/assert.h`).
  Clean first-pass match (0/0/0/0). Retro landed the **`needs-header` hazard** in
  `tools/pick_target.py` (greps upstream `#include`s vs the `-I` set — auto-flags `guRandom`,
  the audio band, `osSyncPrintf`), automating the manual include-triage of the last two sprints.
  No carry-overs.

- **Sprint 4: 2 files BANKED — `src/libultra/monegi/si/sirawread.c` (`__osSiRawReadIo`) +
  `sirawwrite.c` (`__osSiRawWriteIo`), libultra upstream-mirror.** md5-candidate 10→12; matched
  12→14/2090 (~0.67%). First **sibling-pair** sprint (2 files at one-file cost): the `monegi/si/`
  band was already open (Sprint-0's `si.c`/`__osSiDeviceBusy`), so both leaves' callee +
  companion headers (`siint.h`/`assert.h`/`PR/rcp.h`) were pre-resolved — **zero new symbols,
  zero header copies**, two yaml flips the only enabler. Both first-pass clean (0 iteration).
  Sprint-3's `needs-header` hazard auto-steered triage past `guRandom`/audio/`osSyncPrintf` to
  the clean pair. Retro: 0 of 3 suggestions applied (PO: Apply none). No carry-overs.

- **Sprint 5: 2 files BANKED — `src/libultra/monegi/vi/viswapbuf.c` (`osViSwapBuffer`) +
  `visetevent.c` (`osViSetEvent`), libultra upstream-mirror.** md5-candidate 12→14; matched
  14→16/2090 (~0.77%). 2nd sibling-pair, 3rd zero-enabler sprint off a warm band: the `monegi/vi/`
  band (opened Sprint-0 `vigetcurrcontext`, Sprint-3 `visetmode`) pre-resolved `__osViNext` + the
  int-disable pair + all 4 headers — **zero new symbols, zero header copies**, two yaml flips.
  Both byte-identical verbatim copies, first-pass clean (0 iteration). Retro: **2 of 3 applied** —
  #1 **band-warm boost** (Sprint-4 #1, PO-deferred, now validated twice) + #2 **`defines-data`
  hazard** both landed in `tools/pick_target.py`; the latter caught `__osDequeueThread`/`thread.c`
  as a false-clean (re-defines the placed `__osThreadTail` extern + 4 siblings). No carry-overs.

- **Sprint 6: 1 file BANKED — `src/libultra/monegi/vi/viblack.c` (`osViBlack`), libultra
  upstream-mirror.** md5-candidate 14→15; matched 16→17/2090 (~0.81%). 4th consecutive
  zero-enabler vi-band mirror — `__osViNext` + the int-disable pair + both headers all
  pre-placed, single-fn subseg (no split), name pre-curated (0x800AD720) → **one yaml flip the
  only enabler**. Verbatim `cp`, byte-identical, first-pass clean (0/0/0/0). First **live-logged
  v1 story-point sprint** (seed 1, banked 1pt). Retro: **0 of 3 applied** (PO: Apply none).
  No carry-overs. **Note: viblack was the last clean singleton in the vi band** — the 3 smallest
  remaining libultra leaves (`__osDequeueThread`, `__osViInit`, `osYieldThread`) now all carry
  the `defines-data` hazard or need asm-data-recovery for a placed-by-undecompiled-file extern.

- **Sprint 7: 1 file BANKED — `src/libultra/monegi/convert/virtualtophysical.c`
  (`osVirtualToPhysical`), libultra upstream-mirror.** md5-candidate 15→16; matched 17→18/2090
  (~0.86%). **Opened a NEW band (`monegi/convert/`)** — acted on S6 retro #1 (vi band mined out of
  clean leaves). True zero-enabler mirror: all refs pre-resolved (`__osProbeTLB`@0x800ACC00 in
  ghidra_symbols, R4300 macros in in-tree `PR/R4300.h`), name pre-curated (0x800A7720), single-fn
  subseg (no split) → **zero symbol adds, zero header copies, one yaml flip**. Verbatim `cp`,
  byte-identical, first-pass clean (0/0/0/0). seed 2pt / banked 2pt (cold-mirror floor). Retro:
  **2 of 3 applied** — **#1 `refs-unplaced` hazard** landed in `tools/pick_target.py` (dual of
  `defines-data`: flags a `__`-prefixed data extern referenced but absent from both name files →
  asm-data-recovery enabler; re-priced `osYieldThread` 1→2, `osEPiLinkHandle` 2→3, confirmed
  `osStopThread` is the smallest *clean* remaining leaf) + **#2** the classical-target ordering
  note above. No carry-overs.

- **Sprint 8: 1 file BANKED — `src/libultra/monegi/thread/stopthread.c` (`osStopThread`),
  libultra upstream-mirror.** md5-candidate 16→17; matched 18→19/2090 (~0.91%). **Opened the
  `monegi/thread/` band.** Smallest *clean* leaf (S7 retro #2 confirmed): seed 1, warm, name
  pre-curated (0x800AC5C0), all refs pre-placed (`__osRunningThread`, `__osEnqueueAndYield`,
  `__osDequeueThread`, int-disable pair), both headers present → **zero symbol adds, zero header
  copies, one yaml flip**. Verbatim `cp`, first-pass clean (0/0/0/0). 8th straight clean mirror.
  Retro: **0 of 3 applied** (PO: Apply none). No carry-overs. **Standing recommendations carried
  forward: (a) schedule the classical spike — `--upstream none` search — to break the now-8-long
  mirror point-mass and trip the v2 trigger (S7 #2 / S8 #3); (b) thread band is now warm, so
  re-price its leaves (`__osDequeueThread`/`osYieldThread`) at the next gate.**

- **Sprint 9: 1 file BANKED — `src/main/func_80099490.c` (`func_80099490`), FIRST classical
  (no-upstream) match.** md5-candidate 17→18; matched 19→20/2090 (~0.96%). **Classical match-loop
  PROVEN** (S1–S8 were all upstream mirrors). Acted on the standing S7#2/S8#3 classical-spike
  recommendation: a thin no-arg wrapper `void func_80099490(void){ nuPiInitSram(); }`, the
  lowest-variance classical leaf. Callee pre-symbolized, single-fn subseg, kept its `func_` name
  → **one yaml flip, zero symbol adds, zero header copies**; m2c failed on the 1-stub parent,
  Ghidra-decompile seed carried it; first-pass clean (0/0/0/0). Retro: **2 of 3 applied** —
  **#1 graceful m2c fallback** (`seed_c.py parent_has_real_c()` skips m2ctx on a stub-only parent)
  + **#3 `intrinsic-likely` hazard** (`pick_target.py` flags CP0/`sqrt` register shims like
  `osGetCount`/`__osGetCause` so smallest-first stops surfacing un-decompilable leaves). No
  carry-overs.

- **Sprint 10: 2 files BANKED — `src/libultra/monegi/rdp/dpsetstat.c` (`osDpSetStatus`) +
  `dpctr.c` (`osDpGetCounters`), libultra upstream-mirror.** md5-candidate 18→20; matched
  20→22/2090 (~1.05%, **crossed 1%**). 5th **sibling-pair**, 9th straight clean mirror. Split the
  0x86730 `pack:2fn` block at 0x86740 (two *different* upstream files — dpsetstat.c 0x10 +
  dpctr.c 0x4C, both 16-aligned) → two verbatim `cp`s. dp band warm (S1 dp.c); both names
  pre-curated in ghidra_symbols.txt, all DPC_*_REG in PR/rcp.h, both headers present → **zero
  symbol adds, zero header copies, one yaml split**. Both first-pass clean (0/0/0/0). seed 2pt /
  banked 2pt. Retro: **1 of 3 applied** — **#1 pack-disambiguation column** landed in
  `pick_target.py` (`pack:Nfn[fn=basename,…]` shows each member's upstream file → the gate spots
  a multi-file pack needing a split without disassembling asm/<rom>.s). No carry-overs.
  **Note (#2 carried): the rdp band still has `dpsetnextbuf.c` + `dpgetstat.c` as asm leaves —
  re-price them next gate, likely another zero-enabler pair.** *(CORRECTED at S11 gate: these
  do NOT exist as discrete asm leaves in this ROM — 0x86790 is `osGetCount` (intrinsic-likely),
  0x867A0 is the `osSpTaskLoad` pack. The upstream `.c` files exist but MG64's build doesn't
  carry those functions here. Do not re-pursue them as a pair.)*

- **Sprint 11: 1 file BANKED — `src/main/func_800AB600.c` (`func_800AB600`), 2nd classical
  (no-upstream) match; FIRST sprint with residual variance → v2 ACTIVATED.** md5-candidate
  20→21; matched 22→23/2090 (~1.10%). PO redirected the gate from the `osYieldThread` mirror to
  a **non-trivial** classical leaf (S9 ordering note). The fn has real logic — bit ops + a
  branch + a conditional struct RMW + a `(status>>8)&1` return that Ghidra decompiled **wrong**
  (`return 0`) — so the classical loop iterated: seed compiled 0.80/score 400, matched after **1
  fix-iteration** via a **register-reuse nudge** (`bit = status>>8; bit &= 1;`). One yaml flip,
  zero symbol adds, zero header copies (kept `func_` name). seed 5 / banked 5 / realized 5 /
  residual 0. Retro: **2 of 3 applied** — **#2 register-reuse nudge** (CLAUDE.md Conventions
  bullet) + **#3 asm > Ghidra-decompile for classical seeds** (Seed step). The headline decision
  was **v2 activation** (sign-off, not a file-suggestion): the S9 deferral condition is met, so
  the realized-tier/residual/rolling-5/re-anchor machinery is now live on the classical track
  (VELOCITY.md updated). No carry-overs.

- **Sprint 12: 1 file BANKED — `src/libultra/monegi/thread/yieldthread.c` (`osYieldThread`),
  libultra upstream-mirror.** md5-candidate 21→22; matched 23→24/2090 (~1.15%). 10th straight
  clean mirror, 2nd in the `monegi/thread/` band (sibling of S8 stopthread). The S11-gate-recovered
  extern `__osRunQueue`@0x800C8228 was consumed: one `symbol_addrs.txt` add + one yaml flip, zero
  header copies (refs + headers pre-placed from S8), verbatim cp, first-pass clean (0 iteration).
  seed 2 (warm-1 +1 recover-extern) / banked 2pt. Retro: **1 of 3 applied** — **#1** inline the
  recovered vram into the `refs-unplaced` hazard (`pick_target.py` reads splat's `D_<vaddr>`
  labels → binds `name@0xADDR` for the unambiguous single-extern case; verified on `osEPiLinkHandle`
  + `__osSetGlobalIntMask`). No carry-overs.

- **Sprint 13: 1 file BANKED — `src/libultra/monegi/vi/visetyscale.c` (`osViSetYScale`),
  libultra upstream-mirror.** md5-candidate 22→23; matched 24→25/2090 (~1.20%). 6th vi-band
  mirror. **The increment was picked as a non-trivial classical leaf** (`func_800AD370`, the
  PO's v2-residual target) but the gate's asm-vs-upstream check unmasked it as `osViSetYScale`
  — an **un-named** libultra mirror (no curated name → `pick_target` mislabeled it `none`).
  Verbatim cp, zero header copies, one symbol add, first-pass clean. seed mis-priced 5
  (classical) → corrected 1 (warm mirror) / banked 1pt. Retro: **2 of 3 applied** — **#1**
  signature matcher (un-named candidates get an advisory `maybe-upstream:<lib>:<files>` hazard
  from IDF-weighted shared-callee mass; PO-steer: coddog) + **#2** libnusys/libmus/libnaudio
  added to the upstream index (named nusys fns now classify `libnusys`). No carry-overs.

- **Sprint 14: 1 file BANKED — `src/libultra/monegi/thread/setthreadpri.c` (`osSetThreadPri`),
  libultra upstream-mirror.** md5-candidate 23→24; matched 25→26/2090 (~1.24%). 3rd thread-band
  mirror (after S8 `stopthread`, S12 `yieldthread`). Band fully open → all 7 refs + 3 headers
  pre-placed, name pre-curated → **one yaml flip the only enabler** (zero symbol add, zero header
  copy, no split). Largest mirror banked to date (208 B, queue dequeue/enqueue + yield branch)
  yet still seed/banked **1pt** — confirms byte-gate-dormant calibration. Verbatim cp, first-pass
  clean (0/0/0/0). Retro: **1 of 3 applied** — **#3** open-band fast-path (≥2-banked-sibling band
  + `pick_target` no-hazard → skip the agent's redundant manual per-ref re-grep; never overrides a
  flagged hazard, e.g. `__osDequeueThread`'s `defines-data` false-clean still routes normally).
  No carry-overs. **Note:** corrects the "warm clean-singleton pool mined out" claim — that was
  vi-band-specific; the **thread band still yields zero-enabler clean leaves** (siblings remain).

- **Sprint 15: 1 file BANKED — `src/libnusys/mainlib/nugfxswapcfb.c` (`nuGfxSwapCfb`), FIRST
  libnusys upstream-mirror; the libnusys band is now UNLOCKED.** md5-candidate 24→25; matched
  26→27/2090 (~1.29%). Acted on the S13 retro #1 highest-throughput lever: the include-blocked
  libnusys band. Paid a one-time scaffolding enabler — copy the single self-contained `nusys.h`
  (deps `<ultra64.h>`+`<PR/gs2dex.h>`, both already in-tree) to `include/libnusys/nusys.h` + add
  `-I include/libnusys` to CFLAGS — then mirrored the smallest leaf to prove it. Callee
  `osViSwapBuffer` banked S5, name pre-curated (`nuGfxSwapCfb`@0x800A15E0 in ghidra_symbols →
  zero symbol add). Verbatim cp, first-pass clean (0/0/0/0). seed 3 (cold floor 2 + enabler 1) /
  banked 3pt. Retro: **2 of 3 applied** — **#1** un-blk the nusys ranker (`+include/libnusys` in
  `pick_target.py` INCLUDE_DIRS + nusys upstream-inc root; the whole nuGfx*/nuCont* band now
  ranks as pts-2 cold mirrors, no longer `blk`) + **#2** the libnusys path-mirror convention in
  CLAUDE.md. No carry-overs.

- **Sprint 16: 2 files BANKED — `src/libnusys/mainlib/nugfxtaskallendwait.c`
  (`nuGfxTaskAllEndWait`) + `nugfxdisplayoff.c` (`nuGfxDisplayOff`), libnusys upstream-mirror
  sibling-batch.** md5-candidate 25→27; matched 27→29/2090 (~1.39%). 6th sibling-pair, 1st
  libnusys batch (acted on the S15 sibling-batch note). Both verbatim cp, first-pass clean
  (0/0/0/0). Two recover-extern enablers — `nuGfxTaskSpool`=0x8012D478, `nuGfxDisplay`=0x80104E6C
  — both **non-`__`-prefixed library globals** that pick_target's `refs-unplaced` grep missed (a
  **false-clean**: reported no-hazard, recovered at the gate from each fn's own lui/lw|sw). seed
  4 (2+2 cold floor) / banked 4pt. Retro: **2 of 3 applied** — **#1** broadened `refs-unplaced`
  to scan the .c's resolvable headers for `extern` *data* decls (now flags non-`__` globals;
  `nuGfxFunc`/`nuScPreNMIFunc`/`nuGfxSwapCfbFunc` now surface recover-extern + inline vrams) +
  **#2** CLAUDE.md note (mirror branch's proof IS the full-make SHA; byte spot-check is
  classical-only). No carry-overs.

- **Sprint 17: 3 files BANKED — `src/libnusys/mainlib/nucontgbpakgetstatus.c`
  (`nuContGBPakGetStatus`) + `nucontgbpakpower.c` (`nuContGBPakPower`) + `nucontgbpakreadid.c`
  (`nuContGBPakReadID`), libnusys upstream-mirror sibling-trio.** md5-candidate 27→30; matched
  29→32/2090 (~1.53%). 7th sibling-batch (first **trio**), opens the **nuCont sub-band warm**.
  All 3 verbatim cp, first-pass clean (0/0/0/0). **First nuGfx/nuCont leaves with no data
  globals at all** — shared sole callee `nuSiSendMesg`@0x800A2824 (already in ghidra_symbols),
  struct types/constants/prototypes all in nusys.h → **zero new symbols, zero header copies,
  zero splits, three yaml flips the only enabler.** S16#1's broadened `refs-unplaced` grep
  correctly reported all 3 no-hazard (no false-clean recurrence); the gate asm-data-recovery
  jal/lui scan confirmed truly clean. seed 6 (2+2+2 cold floor) / banked 6pt. Retro: **0 of 3
  applied** (PO: log only — all 3 buffered items confirmatory, already covered by S14/S16
  doctrine). No carry-overs.

- **Sprint 18: 1 file BANKED — `src/libnusys/mainlib/nucontinit.c` (`nuContInit`), libnusys
  near-verbatim mirror (drop-one-line).** md5-candidate 30→31; matched 32→33/2090 (~1.58%).
  First **near-verbatim mirror sub-case**: upstream calls 4 managers but this ROM's asm has
  only **3 jals** — `nuContPakMgrInit` is absent from this build (an upstream-vs-ROM **build
  divergence**, not a missing symbol/def). Copied upstream verbatim, dropped the diverging line,
  banked on full-make SHA, 0 iteration. pick_target reported no-hazard (its ref-grep counts data
  refs, not jal callees) — caught at the gate by disassembling. seed 2 / banked 2pt. Retro:
  **2 of 2 applied** — #1 new `jal-count-mismatch` hazard in pick_target.py (counts upstream
  calls vs ROM jals; verified flags 4vs3), #2 CLAUDE.md near-verbatim-mirror bullet. No carry-overs.

- **Sprint 19: 3 files BANKED — `src/libnusys/mainlib/nuprenmifuncset.c` (`nuPreNMIFuncSet`) +
  `nugfxfuncset.c` (`nuGfxFuncSet`) + `nugfxswapcfbfuncset.c` (`nuGfxSwapCfbFuncSet`), libnusys
  recover-extern mirrors.** md5-candidate 31→34; matched 33→36/2090 (~1.72%). The S15-note
  `nuGfx*FuncSet` trio — each stores one callback global behind an `osSetIntMask` critical
  section. **8th sibling-batch (2nd trio); first homogeneous batch banked at the full ≤3-4 cap.**
  One recover-extern each (nuScPreNMIFunc=0x800B6780, nuGfxFunc=0x800C7DC0,
  nuGfxSwapCfbFunc=0x800B67B4), all flagged correctly by S16#1's broadened grep (no false-clean),
  vrams confirmed deterministically from each fn's own lui/addiu (3/3 matched pick). All verbatim
  cp, 0 iterations. seed 9 / banked 9pt. Retro: **3 of 3 applied** — #1 CLAUDE.md recover-extern
  mirror sub-case bullet, #2 CLAUDE.md fill-the-cap batch-sizing rule, #3 kept gate MCP re-confirm
  mandatory (3/3 logged). No carry-overs.

- **Sprint 20: 2 files BANKED — `src/libnusys/mainlib/nucontrmbstart.c` (`nuContRmbStart`) +
  `nucontgbpakopen.c` (`nuContGBPakOpen`), libnusys recover-extern mirrors.** md5-candidate
  34→36; matched 36→38/2090 (~1.82%). 9th sibling-batch, 3rd recover-extern batch, 2nd libnusys
  pair. Both single-fn leaves behind the S17-banked callee `nuSiSendMesg`; one recover-extern
  each (nuContRmbCtl=0x80104F50, nuContPfs=0x801B8A18). **First gate vram-miss:** pick's inlined
  nuContRmbCtl@0x80104F57 was the `.mode` field addr (offset 7) of an indexed-struct array — true
  base 0x80104F50, caught by the mandatory lui/addiu re-confirm (nuContPfs matched exactly).
  Homogeneous pair; genuinely-clean set was exactly 2 (trimmed nuContDataGetEx — extra MISSING fn
  symbol nuContDataOpen). Both verbatim cp, 0 iterations. seed 6 / banked 6pt. Retro: **2 of 3
  applied** — #1 CLAUDE.md indexed-struct-array field-addr-vs-base note (keep re-confirm
  mandatory), #2 CLAUDE.md array-extern size rule (scalar 0x4 / array stride×count). No carry-overs.
  **Note (out-of-band, not backlogged per PO): Ghidra-workspace ↔ decomp `ghidra_symbols.txt`
  drift (~250 Ghidra-ahead; 5 decomp-ahead build-critical) makes full `make sync-names`
  destructive — gate-time name refresh stays surgical until reconciled.**

- **Sprint 21: 1 file BANKED — `src/libnusys/mainlib/nugfxretracewait.c` (`nuGfxRetraceWait`),
  libnusys clean mirror.** md5-candidate 36→37. First genuinely **zero-enabler clean cp** since
  the libnusys band opened (S15): pick reported `-` (no hazard) and was right — warm nuGfx band
  (5 siblings), `nusys.h` + all refs pre-placed, name pre-curated → **one yaml flip the only
  enabler** (zero symbol add, zero header copy, zero known-edit). Verbatim cp, 0 iterations.
  seed 2 / banked 2pt. Retro: **1 of 1 applied** — #1 refreshed the stale S11 "warm pool mined
  out" note below. No carry-overs.

- **Sprint 22: 1 file BANKED — `src/libultra/nintendo/pi/epilinkhandle.c` (`osEPiLinkHandle`),
  libultra recover-extern mirror.** md5-candidate 37→38. **First `nintendo/` variant-dir mirror**
  (prior libultra were `monegi/`/`shared/`) and the first libultra recover-extern since the
  S11 mined-out note — refuting it for the recover-extern fillers it explicitly left open, as
  S21 did for libnusys. One recover-extern (`__osPiTable`=0x800C7E8C, simple pointer → size:0x4,
  confirmed from the fn's own `lui 0x800c`/`lw/sw 0x7e8c`); `piint.h` + the int-disable pair
  pre-placed → one symbol add + one yaml flip. Verbatim cp, 0 iterations. seed 3 / banked 3pt.
  Retro: **2 of 2 applied** — #1 (CLAUDE.md: look up the target fn's vram, never guess a flat
  rom offset, for the recover-extern re-confirm) + #2 (`pick_target.py` `vram` column). The
  re-confirm caught a target-fn vram guess error this sprint — see RETRO/VELOCITY. No carry-overs.

- **Sprint 23: 1 file BANKED — `src/libultra/nintendo/pi/epidma.c` (`osEPiStartDma`), libultra
  recover-extern mirror.** md5-candidate 38→39. 2nd `nintendo/pi/` leaf (sibling of S22
  epilinkhandle; band now warm). One recover-extern (`__osPiDevMgr`=0x800C7E70, OSDevMgr struct →
  size:0x1C, confirmed from the fn's own `lui 0x800c`/`lw 0x7e70` `.active` off0 + the gap to
  placed `__osPiTable`). Verbatim cp, 0 iterations. seed 2 / banked 2pt. **New false-clean class
  surfaced:** an unplaced *function* callee (`osPiGetCmdQueue`=0x800B06F0) link-failed in the
  execution middle — pick's `refs-unplaced` excludes anything called, and the INCLUDE_ASM
  gate-build resolves the jal directly so `make extract && make` stayed green. Recovered from its
  jal target add-only. Retro: **2 of 2 applied** — #1 (`pick_target.py` `calls-unplaced:<fn>@addr`
  hazard, the function dual of `refs-unplaced`) + #2 (CLAUDE.md: reconcile the full data+function
  callee list at the gate). No carry-overs.

- **Sprint 24: 1 file BANKED — `src/libnusys/mainlib/nucontdatagetex.c` (`nuContDataGetEx`),
  libnusys recover-extern + recover-callee mirror.** md5-candidate 39→40. **First leaf carrying
  BOTH `refs-unplaced` AND `calls-unplaced` simultaneously** — the S20 data-recover and S23
  function-dual doctrines composed cleanly, no special handling, both vrams confirmed in one
  disassemble pass. Data `nuContData`=0x801051F8 (`OSContPad[NU_CONT_MAXCONTROLLERS]`, stride 6
  ×4 → size:0x18; offset-0 `lhu` confirmed BASE not field, S20 caution cleared) + callee
  `nuContDataOpen`=0x800A2CAC (jal target, S23 dual). jal-count 3vs3 clean. **Closes the S20
  trim** (this fn was dropped from the S20 pair for the then-missing `nuContDataOpen`; S23's
  `calls-unplaced` hazard — verified on this exact leaf — made it a deterministic gate add).
  Two symbol adds + one yaml flip, verbatim cp, 0 iterations. seed 3 / banked 3pt. Retro:
  **0 of 3 applied** (PO: apply none — #1 size-hint tooling declined; #2/#3 confirmatory). No
  carry-overs.

- **Sprint 47: 1 file BANKED — `src/libultra/io/cartrominit.c` (`osCartRomInit`), libultra; first near-verbatim fn from the S46-reopened io/ band.** md5-candidate 87→88. Single fn 0x7E870 (no split); companion `PRinternal/macros.h` copied in. Verbatim ultralib VERSION_J source; two defines-data drops placed at gate (`__CartRomHandle`=0x80105BC0 size:0x74; function-local `static int first` → `osCartRomInitFirst`=0x800C7EA0). **Key lesson (the cross-jump/wrong-size trap):** hand-folding the upstream `if(!first){rel;return;}` early-return into `if(first){body}` compiled 0x10 SHORTER (1 vs 2 callee-saved regs) → shifted every downstream fn → whole-ROM mismatch. Fix = copy upstream VERBATIM; KMC GCC -O3 cross-jumps the two identical `{rel;return;}` tails itself (the jal `6vs5`), giving the exact baserom regalloc. Residual isolated 4400/99-of-99-rows = reloc HI/LO16 addend artifact → full-make SHA proved it, no permuter. seed 3 / banked 3pt; regime mirror (seed-only). All 0 stuck-far/permuter/carried/re-opened. Applied: 3 of 3 (#1 hazards.md verbatim-first/wrong-size-diagnostic in Near-verbatim section; #2 `pick_target.py` `#if BUILD_VERSION` branch-strip in refs/calls-unplaced — drops dead-`#else` FPs `CartRomHandle`/`osPiRawReadIo` + the `endif` token, golden regen 23 pass; #3 log-only defines-data blind-spot). No carry-overs. **Cross-repo follow-up:** `__CartRomHandle`=0x80105BC0, `osCartRomInitFirst`=0x800C7EA0 are new decomp-side symbols — propagate via `sync_decomp_names.py --import-from-decomp`. **Band note: the cont/pfs/vi/timer siblings remain (each needs `controller.h`/`siint.h`/`viint.h`/`osint.h` companion-copies + recover-externs) — the next io/ mirror pool.**

- **Sprint 49: 1 file BANKED — `src/libultra/gu/random.c` (`guRandom`), libultra; opens the cold `gu/` band.** md5-candidate 89→90 (all 90 .c files stub-free); asm subsegs 183→182. Single fn 0x85420 (12 insns). Verbatim ultralib VERSION_J `src/gu/random.c` except the **S45 defines-data verbatim-body fast path**: function-local `static unsigned int xseed = 174823885` emits a 2nd `main_data` copy (data-segment shift → SHA miss), so dropped to file-scope `extern unsigned int xseed;` + recover `xseed`=0x800C81C0 (size:0x4, was `D_800C81C0` with the tell-tale `.NON_MATCHING` alias). The `blk needs-header:guint.h` was a **false-block** diagnosed at the plan gate: `guint.h` is a missing-but-copyable source-private header, and its `mbi.h`/`gu.h` deps already resolve via the existing `-I include/libultra/PR` (the needs-header grep's `-I` set omits PR → over-flag). Copied `guint.h` verbatim next to `random.c`; `.text` `%hi/%lo(xseed)` identical either way → matched first `make`. `guRandom` name pre-curated in ghidra_symbols (no func add). seed 2 / banked 2pt; regime mirror (seed-only). All 0 stuck-far/permuter/carried/re-opened. Applied: 0 of 0 (clean first-build, empty suggestion buffer). No carry-overs. **Cross-repo follow-up:** `xseed`=0x800C81C0 is a new decomp-side data symbol — propagate via `sync_decomp_names.py --import-from-decomp`. **Band note: cold `gu/` band now open (`guint.h` in-tree); the next gu candidate is the `guScaleF`/`guScale` pack but it carries `guMtxIdentF`/`guMtxF2L` callees → calls-unplaced (heterogeneous, not a clean leaf).**

- **Sprint 50: 1 file BANKED — `src/libultra/gu/scale.c` (`guScaleF` + `guScale`), libultra; 2nd gu/ file, band-open fast path.** md5-candidate 90→91 (all 91 .c files stub-free). Single-file 2-fn pack 0x85A50 (`guScaleF`=0x800AA650 + `guScale`=0x800AA6B0, 224 B, no split). Verbatim ultralib VERSION_J `src/gu/scale.c`, **zero edits** — matched first `make`, full-make ROM SHA-1 == baserom. The `blk needs-header:guint.h` was a **band-warmth false-blk**: scale.c's quote-`#include "guint.h"` resolves source-relative to `src/libultra/gu/guint.h` (shipped alongside random.c in S49); `missing_includes` greps only the `-I` set so over-flagged it. S49's band-note predicted this pack as `guMtxIdentF`/`guMtxF2L` calls-unplaced — **wrong**: both callees (0x80067CB4 / 0x80067B00) were already placed. Names `guScaleF`/`guScale` pre-curated. Single yaml-flip the only enabler. seed 2 / banked 2pt; regime mirror (seed-only). All 0 stuck-far/permuter/carried/re-opened. Applied: 1 of 1 (#1 `missing_includes` takes the mirror dir + drops band-local source-relative quote-includes → `guMtxCatF`/future gu/ siblings no longer false-`blk`; `docs/hazards.md#needs-header` note; golden regen 23 pass). No carry-overs. **Cross-repo follow-up: none** (no new symbols — callees placed, names pre-curated). **Band note: gu/ band now warm (guint.h in-tree). Next pickable gu/ candidates: `guMtxCatF` (warm, no hazard now), `guRotateRPYF` (warm, rodata-literal); larger packs `guRotateF`/`guPerspective`/`guMtxCatL` carry their own calls-unplaced/rodata/needs-header hazards.**

- **Sprint 51: 1 file BANKED — `src/libultra/gu/mtxcatf.c` (`guMtxCatF` + `guMtxXFMF`), libultra; 3rd gu/ file, combined-subseg non16align.** md5-candidate 91→92 (all 92 .c files stub-free); asm subsegs → 180. Single 2-fn subseg 0x847D0 (`guMtxCatF` 0xDC + `guMtxXFMF` 0xAC at non-16 `0x848AC`). Verbatim ultralib `src/gu/mtxcatf.c` — the original SGI file holding BOTH fns as C — **zero edits**, matched first `make`, full-make ROM SHA-1 == baserom. **non16align combined case:** `guMtxXFMF` is tight-packed after `guMtxCatF` at a non-16 offset, so a per-fn yaml split SHA-misses on a bare stub (KMC `as` pads fn1's standalone `.o` to 16, shifting fn2 — caught by the gate build-check); fix = one combined `[0x847D0, c, libultra/gu/mtxcatf]` subseg, both fns in one `.o`. **Gate red-herring:** `pick_target` mapped `guMtxCatF` to libultra_modern's hand-asm `mtxcatf.s` (the deprecated split distro) → mis-warned hand-asm at the gate; disasm-probe showed a textbook compiled nested-loop matmul, and ultralib (the sole source) ships it as C. Both names `guMtxCatF`/`guMtxXFMF` pre-curated; `guint.h` shipped S49. **Fixup:** the reverted per-fn split left an orphan `gu/mtxxfmf.c` stub that `git add -A` swept into the bank commit (9870b4f) → removed in fixup 36ae734 (build stayed green; user-flagged). seed 2 (pts) / committed 3 (+1 one-time non16align gate adjust) / banked 3pt; regime mirror (seed-only). All 0 stuck-far/permuter/carried/re-opened. Applied: 3 of 3 (#1 pack `.s`/`.c`-boundary disasm-probe before classical-flag; #2 hazards.md#non16align combined-subseg + reverted-split cleanup caveat; #3 hazards.md#upstream-mirror-pattern S51 cautionary note — reach ultralib first, a libultra_modern `.s` is not evidence of hand-asm). No carry-overs. **Cross-repo follow-up: none** (no new symbols — names pre-curated, callees are pure float arithmetic). **Band note: gu/ band warm. Next pickable gu/ candidates: `guMtxIdentF`/`guMtxF2L` (the scale.c callees, already placed → likely small mirrors), `guRotateRPYF` (rodata-literal); larger packs `guRotateF`/`guPerspective`/`guMtxCatL` carry calls-unplaced/rodata/needs-header. The non16align combined-subseg pattern now applies to any tight-packed gu/ multi-fn file.**

- **Sprint 52: 2 files BANKED — `src/libultra/gu/rotaterpy.c` (`guRotateRPYF` + `guRotateRPY`) + `src/libultra/gu/lookatref.c` (`guLookAtReflectF` + `guLookAtReflect`), libultra; 4th+5th gu/ files, mirror pair.** md5-candidate 92→94; asm subsegs 180→178. Two verbatim ultralib VERSION_J mirrors, both clean first build (0 iter), full-make ROM SHA-1 == baserom. **rotaterpy** (seed 2): S49 static-float fast path — fn-local `static float dtor = 3.1415926/180.0` dropped to file-scope `extern float dtor;` + recover `dtor`=0x800C81E0 (size:0x4). pick_target flagged it `rodata-literal` but 0x800C81E0 is the gu **data region** (`lwc1 %lo`), a `data-static` recover-extern, not a rodata sibling. Callees sinf/cosf/guMtxIdentF/guMtxF2L all pre-placed. **lookatref** (seed 3): recover-callee `sqrtf`=0x800B0A10 (S23 dual; also resolved `guLookAtHiliteF`'s former calls-unplaced) + **rodata-sibling split** — two doubles `-1.0`@0x800D2510 (`ldc1`) + `1.0`@0x800D2518 (an `lw` pair, FP-scan-invisible) = 16 B at rom 0xAD910 → `[0xAD910, .rodata, libultra/gu/lookatref]` + `[0xAD920, rodata]` (vram→rom delta 0x80024C00; finalize-time, since a stub has no `.rodata`). seed 5 / committed 5 / banked 5pt; regime mirror (seed-only). All 0 stuck-far/permuter/carried/re-opened. Applied: 3 of 3 (#1 this band-note correction; #2 `pick_target.py` data-static-vs-rodata-literal segment classifier — `%lo(D_)` in a `rodata` subseg ⇒ sibling split, in the data segment ⇒ recover-extern; #3 `rodata_word_refs` unions `lw`-pair double 2nd-words so the sibling extent is sized in full; docs/hazards.md #defines-data + #rodata-sibling notes, CLAUDE.md index, golden regen 23 pass). No carry-overs. **Cross-repo follow-up:** `dtor`=0x800C81E0, `sqrtf`=0x800B0A10 are new decomp-side symbols — propagate via `sync_decomp_names.py --import-from-decomp`. **Band note (corrects S51): the clean separable gu/ leaf pool is mined out.** S51's prediction that `guMtxIdentF`/`guMtxF2L` are next small gu/ mirrors was WRONG — they live inside the 12-fn main-segment pack `func_800660A0` (0x414A0, pts 13), not separable subsegs. After S52, the remaining gu/ candidates are all `blk` (`guRotateF`/`guPerspectiveF`/`guMtxCatL` need `gu.h`/`ultratypes.h`/`os_version.h` companion-copies; `../gu/guint.h` over-flags resolve source-relative) or pts-13 8-gate packs (`sinf`, `guLookAtHiliteF`, `guAlignF`) or carry `calls-unplaced:guNormalize/xxsine`. `guRotateF`/`guAlignF` now correctly show `data-static` (their per-file gu static floats @0x800C81D0/0x800C81A0). Next libultra gu/ needs a needs-header companion-copy enabler; otherwise the libultra epic's cheap-mirror band is exhausted and the next gate should weigh a classical leaf or a needs-header unblock.

- **Sprint 53: 1 file BANKED — `src/libultra/audio/heapalloc.c` (`alHeapDBAlloc`), libultra; closes the named-clean `audio/` leaf.** md5-candidate 94→95 (all 95 .c files stub-free); asm subsegs 178→177. Single fn 0x82320, wedged between the banked S36 siblings `heapinit.c` (0x822E0) and `copy.c` (0x82370). Verbatim ultralib `src/audio/heapalloc.c`, **zero edits**, matched first `make`, full-make ROM SHA-1 == baserom. Body trivial under `_FINALROM` (the `_DEBUG` HeapInfo bookkeeping + `__osError` call compile out) → no data externs, no callees to reconcile. The `blk needs-header:libaudio.h,os_internal.h,ultraerror.h` was a **new false-block class**: all 3 ship at `include/libultra/PR/` and resolve via `LIBULTRA_CFLAGS -I include/libultra/PR`, but `missing_includes`'s base-`INCLUDE_DIRS` model omits the PR/ path → over-flags every libultra PR-band mirror (distinct from the S49/S50 gu/ quote-include false-blks, which resolved source-relative). Refuted at the gate by the banked sibling `heapinit.c`. `alHeapDBAlloc` name pre-curated in ghidra_symbols (no func add). Single yaml-flip the only enabler. seed 1 (pts read `blk`, refuted) / banked 1pt; regime mirror (seed-only). All 0 stuck-far/permuter/carried/re-opened. Applied: 3 of 3 (#1 `missing_includes` lib-aware via `LIB_EXTRA_INCLUDE_DIRS` — libultra unions `include/libultra/compiler/gcc`+`include/libultra/PR`, matching LIBULTRA_CFLAGS; golden regen — alHeapDBAlloc gone, sprintf `os.h` now resolves → `needs-header:xstdio.h,string.h` — 23 pass; #2 dropped the stale "deferred Makefile enabler … <libaudio.h> at include/libultra/PR/" comment at the INCLUDE_DIRS note; #3 this BACKLOG hazard-map re-survey). No carry-overs. **Cross-repo follow-up: none** (name pre-curated, no new symbols). **Band note: `audio/` named-clean leaf done; with the PR-band false-blk fixed, `--lib libultra` is genuinely empty — remaining libultra leaves carry real blockers (`sprintf` file-static + true needs-header; cache ops intrinsic-likely/none). Next libultra progress = a needs-header unblock (`sprintf`) or a classical leaf, not a hidden mirror.**

- **Sprint 54: 1 file BANKED — `src/libultra/libc/sprintf.c` (`sprintf` + static `proutSprintf`), libultra; closes the LAST named-clean libultra leaf.** md5-candidate 95→96 (all 96 .c files stub-free); asm subsegs 177→176. Whole-file 2-fn combined subseg 0x861F0 (`sprintf` 0x800AADF0 0x58 + static `proutSprintf` 0x800AAE48 0x34, 144 B, no split — S40 ldiv pattern). Verbatim ultralib VERSION_J `src/libc/sprintf.c`, **zero edits**, matched first `make`, full-make ROM SHA-1 == baserom. Callees both pre-placed (`_Printf`=0x800B0B30, `memcpy`=0x800AADC4); no data externs; names pre-curated → **zero symbol adds**. **Two false-flags refuted at the gate:** (a) `file-static` was the `static void* proutSprintf(...)` *function* proto (shares the TU, no BSS hazard — not a static variable); (b) `blk needs-header:xstdio.h,string.h` was the vendorable false-blk class (3rd instance, S49 guint.h / S53 PR-band) — vendored `xstdio.h`→`src/libultra/libc/` (source-private; deps stdlib.h/stdarg.h in-tree) + `string.h`+`memory.h`→`include/libultra/compiler/gcc/` (kept source-relative inside the libultra compiler dir per the S40 cross-lib lesson). `os.h` resolves via `-I include/libultra/PR`. seed 3 (mirror floor 2 + header-vendor enabler 1) / banked 3pt; regime mirror (seed-only). All 0 stuck-far/permuter/carried/re-opened. Applied: 2 of 3 (#1 `pick_target.py` file-static detector ignores static *function* decls — `_is_static_func_proto`, attribute-stripped so attributed static *arrays* still flag; caught + fixed a gfxThread/`nuGfxMesgBuf` un-flag regression pre-regen; #2 `needs-header:<h>(vendorable)` annotation via `UPSTREAM_SRC_ROOTS` source-private index + `include_is_vendorable` — copyable headers price as a 1pt enabler not `blk`; #3 S40 cross-lib confirmatory, log-only; golden regen 23 pass). No carry-overs. **Cross-repo follow-up: none** (names pre-curated, no new symbols). **Band note: `--lib libultra` is now genuinely EMPTY. The remaining libultra-range candidates are all `intrinsic-likely` register/FPU shims (`osGetCount`, `sqrtf`, `__osGetCause`, `__osGetSR`, `__osSetCompare`, `osWritebackDCacheAll`, `func_800ACCC0`) → classical-or-hasm, NOT mirrors. The libultra epic's cheap-mirror band is fully exhausted; next libultra progress is a classical/hasm decision on the intrinsic shims, or the project pivots to libnusys/libkmc fillers (pivot the scope at the next gate).**

- **Sprint 55: 1 file BANKED — `src/libultra/gu/perspective.c` (`guPerspectiveF` + `guPerspective`), libultra; 6th gu/ file, last clean low-cost libultra leaf.** md5-candidate 96→97 (all 97 .c files stub-free); asm subsegs 176→175. 2-fn pack 0x84CE0 (`guPerspectiveF` 0x800A98E0 + thin wrapper `guPerspective` 0x800A9A90, 896 B). Verbatim `libultra_modern` monegi `gu/perspective.c`, **zero edits**, matched first `make`, full-make ROM SHA-1 == baserom. Callees all pre-placed (`cosf`/`sinf`/`guMtxIdentF`/`guMtxF2L`); `guint.h` sibling in-tree; names pre-curated → **zero symbol adds**. **rodata-sibling split** for an 8-double FP pool (0x800D2520..0x2558 = rom 0xAD920..0xAD960): `[0xAD920, rodata]` → `[0xAD920, .rodata, libultra/gu/perspective]` + `[0xAD960, rodata]` (finalize-time, since a stub has no `.rodata`). **Found via the full-band survey, NOT `--lib libultra`** — the `--lib` substring filter returned empty because un-flipped libultra asm subsegs lack a `libultra/` path qualifier (surface as `upstream none`); survey by the `upstream` column instead. seed 2 / banked 2pt; regime mirror (seed-only). All 0 stuck-far/permuter/carried/re-opened. **Bank-gotcha (caught pre-finalize via the asm, not a spike):** `pick_target.py`'s rodata-literal pre-flag undercounted 4 of 8 — the pool's 2nd half (0x2540..0x2558) is loaded by the *sibling* `guPerspective` while the scan was per-primary. Applied: 1 of 1 (#1 `rodata_literals`/`rodata_word_refs` now scan the whole subseg via new `decomp_asm.iter_subseg_body` — a `.rodata` sibling places the whole object's `.rodata`, so every pack function's pooled literals are in the split extent; + `tests/tooling/test_decomp_asm.py` 5 unit tests, suite 23→28 pass, pick_target golden unchanged). No carry-overs. **Cross-repo follow-up: none** (names pre-curated, no new symbols). **Band note: the libultra cheap-mirror band is now TRULY exhausted. Next libultra = classical-track (next-cleanest: `osGbpakCheckConnector` pts 3 needs-header-only) or a regime/scope change (intrinsic-shim→hasm pass, or libnusys/libkmc fillers).**

- **Sprint 56: 0 files BANKED — asm-mirror vendoring PILOT (4 libultra reg-shim hand-asm TUs).** First use of the new asm-mirror pattern (asm analog of the C upstream-mirror). libultra C-mirror band exhausted (97/97 .c 0-stub) and scope `libultra` empty, so PO pivoted to vendoring the `intrinsic-likely` hand-asm shims. Vendored `getcount`/`getcause`/`getsr`/`setcompare` verbatim from ultralib `src/os/` → `src/libultra/os/`, flipped 4 subsegs `asm`→`hasm` (asm subsegs 175→171, hasm 2→6; md5-candidate unchanged 97/97). **Load-bearing discovery (PO directive: use ultralib's exact flags):** assemble vendored TUs with the KMC/N64 gcc (gcc.mk profile, new `LIBULTRA_ASFLAGS` + `VENDOR_ASM` map in the Makefile), NOT modern `mips-linux-gnu as` — KMC `as` pads each fn's `.text` up to its 16-byte ROM slot; modern `as` emits the bare 0xC and the `.ld` (no inter-subseg ALIGN) shifts every following subseg → SHA-1 break. Added `MFC0`/`MTC0` macros to `include/sys/asm.h`. seed 2 / banked 2pt; regime mirror (seed-only). 0 stuck-far/permuter/re-opened; ~11 TUs + 2 mixed packs carried *by plan*. Applied 3 of 4 (#1 `docs/hazards.md#asm-mirror-vendoring` + CLAUDE.md index; #2 `pick_target.py` intrinsic-likely carries the vendorable ultralib TU path via `build_asm_tu_index`, golden regen 28 pass; #4 pinned ultralib gcc.mk as the libultra source in CLAUDE.md; #3 carry-over-wording NOT selected). **Cross-repo follow-up: none** (no new symbols). **Band note: see the Carry-overs list for the remaining intrinsic-likely TUs — the asm-mirror pattern is now proven and extends by adding `VENDOR_ASM` pairs.**

- **Sprint 57: 0 files BANKED — asm-mirror vendoring, 4 libultra cache/TLB asm primitives.** 2nd asm-mirror sprint (S56 pilot → repeatable). Vendored `osWritebackDCacheAll` (0x82560), `osUnmapTLBAll` (0x88100), `osWritebackDCache` (0x824E0), `__osProbeTLB` (0x88000) verbatim from ultralib `src/os/` → `src/libultra/os/`, added 4 `VENDOR_ASM` pairs, flipped 4 subsegs `asm`→`hasm` (asm subsegs 171→167, hasm 6→10; md5-candidate unchanged 97/97). **All 4 version-matched first try** (one atomic `make extract && make`, SHA-1 == baserom). Zero enablers beyond the flip: names pre-curated (`LEAF` supplies the symbol), all cache/TLB macros in `include/libultra/PR/R4300.h`. seed 4 / banked 4pt; regime mirror (seed-only). 0 stuck-far/permuter/carried/re-opened. Applied 2 of 2 (#1 `pick_target.py` `vendorable_tu_missing_defines` pre-check — flags `intrinsic-likely:<tu>.s(needs-define:<MACROS>)` so a missing macro is priced at the gate, golden unchanged, suite 28→29; #2 `docs/hazards.md#asm-mirror-vendoring` SHA-breaker bisect protocol + needs-define note + S57 provenance). **Cross-repo follow-up: none. Backlog correction:** `osMapTLBRdb` is NOT enabler-blocked (`PR/rdb.h` in-tree; pre-check confirms 0 missing macros) — joins the clean vendorable pool.

- **Sprint 58: 0 files BANKED — asm-mirror vendoring, 3 libultra intrinsic-likely asm TUs (cross-dir batch).** 3rd asm-mirror sprint; first cross-dir batch (gu/ + os/ + libc/) and first to clear caveated TUs. Vendored `sqrtf` (0x8BE10, `gu/sqrtf.s`, 16 B), `osMapTLBRdb` (0x8CD10, `os/maptlbrdb.s`, 96 B), `bcopy` (0x85DA0, `libc/bcopy.s`, 800 B) verbatim from ultralib → `src/libultra/`, added 3 `VENDOR_ASM` pairs, flipped 3 subsegs `asm`→`hasm` (asm subsegs 167→164, hasm 10→13; md5-candidate unchanged 97/97). **All 3 version-matched first try** (one atomic `make extract && make`, SHA-1 == baserom; all 3 `.o` `.text` == slot 0x10/0x60/0x320). **Both flagged caveats resolved clean, no special handling:** `bcopy`'s `#ifdef __sgi` is undefined in the KMC build → `#else` `_bcopy=bcopy` path (the in-tree `WEAK` macro never reached); `sqrtf` has no `.set noreorder` → assembler auto-fills the `j ra` delay slot. Zero enablers beyond the flip (names from `LEAF`; macros self-contained: `C0_*`/`TLBLO_*`/`RDB_*` in `PR/R4300.h`+`PR/rdb.h`, FPU `sqrt.s` native). seed 2 / banked 2pt; regime mirror (seed-only). 0 stuck-far/permuter/carried/re-opened. Applied 0 of 0 (empty suggestion buffer — all first-try clean). **Cross-repo follow-up: none. Pattern note: asm-mirror now proven across 3 dirs + the WEAK-alias and FPU-op cases.** Remaining intrinsic-likely carry-overs: un-named `func_800ACB40`/`func_800ACCC0` (need `.s` identification), `osInvalDCache`+`osInvalICache` (combined-subseg sub-pattern), `__osDisableInt`/`__osRestoreInt` (source not located), plus the mixed packs needing a split.

- **Sprint 59: 1 file BANKED — `src/libultra/io/gbpakcheckconnector.c` (`osGbpakCheckConnector`), libultra; returns to the C-mirror track after S56-58 asm-vendoring.** md5-candidate +1. Single fn 0x89320 (1120 B), sibling of the already-banked `gbpak{getstatus,power,readwrite}.c` (its exact callees). Verbatim ultralib `src/io/gbpakcheckconnector.c` with the io-band include adaptation (`PRinternal/controller.h` → `"controller.h"` + `controller_gbpak.h`), **zero edits**, matched first `make`, full-make ROM SHA-1 == baserom. **The sole hazard `needs-header:controller.h(vendorable)` was a 0-work no-op** — already vendored at `include/libultra/internal/controller.h` (defines `ARRLEN`/`ERRCK` inline) + `controller_gbpak.h`, already on the io band `-I` set, so the stripped-include adaptation resolved free. No jal-mismatch / calls-unplaced / refs-unplaced; name pre-curated → **zero symbol adds**. The clean hazard profile (vs the ContRam pair's `jal-count-mismatch:23vs10`) correctly steered the pick to a mirror, not a hard classical. seed 3 / banked 3pt; regime mirror (seed-only). 0 stuck-far/permuter/carried/re-opened. Applied 1 of 1 (#1 `pick_target.py` `include_is_already_vendored` — a missing include whose BASENAME resolves under the `-I` set is tagged `(already-vendored)` not `(vendorable)`, pricing a 0-work no-op apart from a real cp; re-tags the warm io/cont/pfs/vimgr/timer band, `guRotateF`'s genuine `../gu/guint.h` correctly stays `(vendorable)`; golden regen, suite 29 pass). **Cross-repo follow-up: none** (name pre-curated, no new symbols). **Band note: the clean low-cost libultra C-mirror band is exhausted again; remaining libultra C is classical-track (ContRam/pfs/cont families carry jal-mismatch → stripped impls) or the asm-mirror vendoring carry-overs.**

- **Sprint 61: 1 file BANKED — `src/libultra/gu/rotate.c` (`guRotateF`+`guRotate`), libultra near-verbatim gu-band mirror.** md5-candidate 100→101 (all 101 .c files now 0-stub). 2-fn pack 0x85450 (`guRotateF` 0x800AA050 + thin wrapper `guRotate`). Verbatim ultralib `gu/rotate.c` under BUILD_VERSION=VERSION_J, **sole edit `guNormalize`→`vec3f_normalize`**: the ROM `jal`s `vec3f_normalize`@0x80029900, which a `.o`-body-compare proved is a *genuinely different* function (game code region not libultra; −0x28 frame + `osSyncPrintf` degenerate-input error path + (0,1,0) fallback + bare `sqrt.s`, vs ultralib `guNormalize`'s −0x20 frame + `sqrtf` NaN-check) — a callee **substitution**, not a name reconciliation (PO-prompted verification). The detector flags `jal-count-mismatch:7vs4` + `calls-unplaced:...,xxsine,yxsine,zxsine` were **false** — under VERSION_J the `#if >= VERSION_K` block is dead, so `xxsine/yxsine/zxsine` are `#define` macros whose definition lines `#define xxsine (x*sine)` matched the call regex. **Enabler: a 16B `main_data` `.data` carve** for the function-local `static float dtor` (`data-static:0x800C81D0` → rom 0xA35D0→0xA35E0); the `dtor` name was already claimed by banked sibling `rotaterpy.c`@0x800C81E0, so the S52 drop-extern hoist would collide → kept the static verbatim + carved. Callees pre-placed (`vec3f_normalize`/`sinf`/`cosf`/`guMtxIdentF`/`guMtxF2L`), `guint.h` co-located, names pre-curated → **zero symbol adds**. seed 3 / banked 3pt; regime mirror (seed-only). 0 stuck-far/permuter/carried/re-opened; **1 novel bank-gotcha** (a 4B carve double-counted — rotate.o(.data) is section-padded to 16B — growing the ROM +16B; correct carve sized from the compiled `.o`, fixed same-session). Applied 3 of 3 (#1 `docs/hazards.md#defines-data` function-local-vs-file-scope hoist/carve distinction + carve-sizing rule; #2 `pick_target.py` `_strip_define_lines` drops in-body `#define` lines before the call scan [golden regen, suite 29 pass]; #3 `docs/hazards.md#calls-unplaced` renamed-vs-substituted body-compare step). **Cross-repo follow-up: none** (names pre-curated, no new symbols). **Band note: the libultra gu band's clean low-cost mirrors are now exhausted; what remains is the `guAlignF`/`guMtxCat` 4fn-pack 8-gate decomposes and the classical-track ContRam/pfs/cont families (jal-mismatch → stripped impls).**

- **Sprint 60: 2 files BANKED — `src/libultra/gu/mtxcatl.c` (`guMtxCatL`+`guMtxXFML`) + `gu/ortho.c` (`guOrthoF`+`guOrtho`), libultra verbatim gu-band mirrors.** md5-candidate 98→100. Decomposed the pts-8 `[0x84960,asm]` 4-fn gu pack at the `mtxcatl.c | ortho.c` upstream-file boundary (the 8-gate split path). The lone hazard `needs-header:../gu/guint.h(vendorable)` was a **false-positive cascade**: pick mis-attributed `guMtxXFML` to `mgu/mtxxfml.c` (whose `../gu/guint.h` include produced the flag), but upstream truth is `gu/mtxcatl.c` carries `guMtxXFML` under `#if BUILD_VERSION < VERSION_K` (active for VERSION_J); `guint.h` is vendored co-located (S49). **One genuine enabler — a new `stale-vendored-header` class:** verbatim `mtxcatl.c` linked-undefined on `guMtxXFML` because the in-tree `include/libultra/PR/os_version.h` was the stripped 2.0L revision (no `VERSION_*` constants), so the `< VERSION_K` guard read `0<0`=false and silently dropped the fn — invisible to `needs-header` (header resolves; its *content* is the wrong revision), surfacing only at link, not compile. Fixed by adding `VERSION_D..L` verbatim (additive; clean-rebuild SHA-safe — every other guard compares `VERSION_J`=`BUILD_VERSION`). `ortho.c` first-pass clean. All callees pre-placed (`guMtxL2F`/`guMtxXFMF` in banked `mtxcatf.c`, `guMtxCatF`/`guMtxF2L`/`guMtxIdentF`), all 4 names pre-curated → **zero symbol adds**. seed 5(cold)+2(warm)=7 / banked 7pt; regime mirror (seed-only; 8-gate satisfied by decompose). 0 stuck-far/permuter/carried/re-opened. Applied 4 of 4 (#1 `pick_target.py` `stale_version_header` → `stale-header:os_version.h(<V>)` + `docs/hazards.md#stale-vendored-header` + CLAUDE.md index; #2 `missing_includes` normpath; #3 `docs/hazards.md#clean-rebuild-after-shared-header-edit` + CLAUDE.md finalization bullet; #4 `build_upstream_index` deterministic sort [gu<mgu] + version-strip — fixes the guMtxXFML/guRotateF mgu-mislabel, removes guRotateF's phantom `../gu/guint.h` needs-header [supersedes the S59 #1 "genuine" claim]; golden regen, suite 29 pass). **Cross-repo follow-up: none** (all names pre-curated). **Band note: remaining libultra gu is the `guAlignF`/`guMtxCat` 4fn-pack 8-gate decomposes; the rest of libultra C is classical-track (ContRam/pfs/cont families carry jal-mismatch → stripped impls).**

- **Sprint 62: 0 .c files BANKED — asm-mirror vendoring, 2 libultra cache-invalidate asm primitives (first combined-subseg split).** 4th asm-mirror sprint. Vendored `osInvalDCache` (0x823B0, `os/invaldcache.s`, 176B/0xB0) + `osInvalICache` (0x82460, `os/invalicache.s`, 128B/0x80) verbatim from ultralib → `src/libultra/os/`. The combined `[0x823B0,asm]` subseg held BOTH fns in one 304B slot → **split at the osInvalICache boundary 0x82460** into two `hasm` subsegs + 2 `VENDOR_ASM` pairs (asm subsegs 161→160, hasm 13→15; md5-candidate unchanged 101/101 — asm-mirror is asm→hasm, not asm→c). Full-make ROM SHA-1 == baserom first try; both `.o` `.text` == slot. Macros self-contained (`DCACHE_*`/`ICACHE_*`/`CACH_*`/`C_*`/`K0BASE` in `PR/R4300.h`, `CACHE`/`LEAF`/`END` in `sys/asm.h` — the S57-proven set); names pre-curated (ghidra_symbols + `LEAF`) → **zero symbol adds**. seed 2 / banked 2pt; regime mirror (seed-only; 8-gate clear). 0 stuck-far/permuter/carried/re-opened; 1 mild novelty (combined-subseg split, mechanical, same as the S10/S60 C-pack splits). Applied 2 of 2 (#1 `pick_target.py` `combined-subseg:<n>tu[…]` pre-flag — ≥2 asm-ONLY members from distinct ultralib `.s` files, gated on no-C-upstream so C-mirror gu packs like `sinf`+`translate` don't mis-flag; surfaced `__osSetFpcCsr` as a `3tu[setfpccsr.s|setsr.s|setwatchlo.s]` reg-shim pack; + `docs/hazards.md#asm-mirror-vendoring` combined-subseg caveat + CLAUDE.md index; golden regen [1 diff], suite 29 pass; #2 this BACKLOG carry-over wording fix). **Cross-repo follow-up: none** (names pre-curated). **Band note: the clean source-confirmed asm-mirror TUs are exhausted; what remains are spikes — the partial-TU `__osDisableInt`/`__osRestoreInt` carve off `setintmask.s`, the un-named `func_800ACCC0`/`func_800ACB40` (need `.s` ID), and the mixed packs. Next libultra progress is classical-track (ContRam/pfs/cont jal-mismatch stripped impls) or the gu `guAlignF`/`guMtxCat` 4fn-pack 8-gate decomposes — a scope/track call for the next gate.**

- **Sprint 63: 1 .c file BANKED + 3 asm-mirror TUs vendored — cleared the pts-8 `[0x8CA50]` register-shim + device-busy pack (libultra).** md5-candidate 101→102. 5th asm-mirror sprint; first mixed asm-mirror + C-mirror subseg-clear. Decomposed the pts-8 `__osSetFpcCsr` pack (8-gate FIRED): the `[0x8CA50,asm]` 80B subseg held 3 reg-shim asm TUs (`__osSetFpcCsr`/`setfpccsr.s`, `__osSetSR`/`setsr.s`, `__osSetWatchLo`/`setwatchlo.s`, each 0x10, continuing the adjacent 0x8CA20/30/40 S56 run) + 1 C-mirrorable `__osSpDeviceBusy` (`io/sp.c`, 0x8CA80, sibling of dp/ai/si device-busy). `combined-subseg:3tu` (NOT 4tu — the C member excluded by the S62 no-C-upstream gate) named the split shape; MCP confirmed all 4 boundaries verbatim. 4-way split at 0x8CA60/70/80 → 3 `hasm` + 1 `c` (asm 160→159, hasm 15→18); +3 `VENDOR_ASM` pairs, all 3 `.text` == 0x10 slot; C mirror trimmed to the dp.c io-band include set (`os_internal.h`+`rcp.h`), 0 iteration. **One gate-surfaced enabler:** `setfpccsr.s` references `CFC1`/`CTC1` (FPU control-reg moves) absent from in-tree `sys/asm.h` (S56 vendored only `MTC0`/`MFC0`) → vendored both verbatim; the `combined-subseg` flag did NOT carry the `needs-define` pre-check (only `intrinsic-likely` did), so the gap surfaced at a failing vendor-compile, not the gate. `setfpccsr.s`'s upstream `@bug END(__osSetSR)` is metadata-only, .text clean. All 4 names pre-curated → **zero symbol adds**. Clean-rebuild + full-make ROM SHA-1 == baserom. seed 4 / banked 4pt; regime mirror (seed-only; 8-gate resolved by the split). 0 stuck-far/permuter/carried/re-opened; 1 enabler-discovery (CFC1/CTC1, reactive). Applied 1 of 1 (#1 `pick_target.py` — the `combined-subseg:<n>tu` builder now unions `vendorable_tu_missing_defines` across the pack's TUs and appends `(needs-define:…)`, the same pricing the `intrinsic-likely` path already does; provably golden-inert [0 combined-subseg packs remain post-banking], golden regen for the banking, suite 29 pass). **Cross-repo follow-up: none** (names pre-curated). **Band note: the 3 "set" reg shims complete the CP0/FPU shim family (S56 did the "get"/setcompare siblings). Remaining intrinsic-likely are spikes (partial-TU `__osDisableInt`/`__osRestoreInt`, un-named `func_800ACCC0`/`func_800ACB40`, mixed packs); next libultra progress is classical-track (ContRam/pfs/cont jal-mismatch stripped impls) or the gu `guAlignF`/`guMtxCat` 4fn-pack 8-gate decomposes.**

- **Sprint 64: 1 .c file BANKED — `src/libultra/gu/lookathil.c` (`guLookAtHiliteF` + `guLookAtHilite`), libultra gu-band verbatim mirror; the single cleanest remaining libultra mirror.** md5-candidate 102→103 (**all 103 .c files now 0-stub**); asm subsegs 159→158. 2-fn 2656B subseg 0x83780. Verbatim ultralib `gu/lookathil.c` (VERSION_J), **byte-identical cp, 0 edits**, matched first `make`, full-make ROM SHA-1 == baserom. All callees pre-placed (`guMtxIdentF`/`sqrtf`[S58]/`guMtxF2L`/self), both names pre-curated → **zero symbol adds**, `guint.h` vendored (S49) → zero header copies. **rodata sibling-split** for the 6-double ANONYMOUS pool (`[0xAD8E0, .rodata, libultra/gu/lookathil]`, 0x800D24E0..0x2510 = exactly the lookatref boundary). NO vec3f_normalize substitution (that's the align/lookat 0x82B80 combined subseg, not this file). **8-gate FIRED (pts-13, SIZE-only) → ran as the first documented verbatim-mirror EXEMPTION** (S64 #1): decompose mechanically blocked (single upstream file; internal fn boundary guLookAtHilite@0x84104 non-16-aligned) + all callees placed + names curated = no all-or-nothing classical stall → banked atomic first-try. seed 13 / banked 13pt; regime mirror (seed-only). 0 stuck-far/permuter/carried/re-opened. Applied 3 of 3 (#1 8-gate verbatim-mirror exemption [CLAUDE.md+VELOCITY]; #2 `pick_target.py` rodata-literal `;carve-end=` boundary; #3 `pick_target.py` `c-combined:<n>file[…]` — C analog of S62 combined-subseg surfacing multi-file C packs, e.g. the previously-`upstream none` sp pack `func_800B16A0`→`3file[sprawdma|spsetpc|spsetstat]`; golden regen, suite 29 pass). **Cross-repo follow-up: none** (names pre-curated). **Band note: ALL low-cost libultra C mirrors are now exhausted (103/103 .c stub-free). Next-cleanest C mirror = `cosf` (gu/cosf.c, 0x82F20) — see Carry-overs. Otherwise classical-track (ContRam/pfs/cont jal-mismatch stripped impls) or asm-mirror spikes (partial-TU `__osDisableInt`/`__osRestoreInt`, un-named `func_800ACCC0`/`func_800ACB40`, mixed packs).**

- **Sprint 65: 1 .c file BANKED + 1 asm-mirror — clear the `[0x860C0]` libc pack: `src/libultra/libc/string.c` (`strchr`+`strlen`+`memcpy`) C mirror + `src/libultra/libc/bzero.s` (`bzero`) asm-mirror.** md5-candidate 103→**104**; asm subsegs 158→157, hasm 18→19. The pts-8 4-fn pack `[bzero=?,strchr/strlen/memcpy=string]` was a MIXED unit: `bzero` is ultralib hand-asm (`libc/bzero.s`), the rest are `libc/string.c`. Decomposed at the bzero|string boundary (rom 0x86160, 16-aligned) → `bzero` vendored `hasm` via VENDOR_ASM (bcopy.s pattern, .o .text=0xA0, proven at gate 0 iter) + `string.c` verbatim C mirror. **Project constraint discovered (durable — PO promote to memory): MG64's libultra is `-fsigned-char`, NOT ultralib-J's `-funsigned-char`.** string.c SHA-missed under `-funsigned-char` (`strchr`/`strlen` load `lb`+`sll/sra` sign-extend + phantom empty frame; `memcpy` matched either way); the authoritative `make clean` + global `-fsigned-char` rebuild reproduced the baserom SHA-1 EXACTLY, so the band default was flipped to `-fsigned-char` and the interim per-file override removed. ultralib's gcc.mk adds `-funsigned-char` for VERSION_J → this is a ROM-proven deviation from the documented J profile; all 104 current libultra C files match signed. seed 4 / banked 4pt; regime mirror (seed-only, **but the mirror-track point-mass was violated — first mirror sprint with real variance, a build-flag recovery**). 0 stuck-far/permuter/carried/re-opened. Applied 3 of 3 (#1 `pick_target.py` pack-member labels resolve `.s` TUs `bzero=?`→`bzero=bzero.s`, mixed asm+C packs now legible; #2 `docs/hazards.md#char-signedness` + CLAUDE.md index row [pre-flag descoped — #3 subsumed it]; #3 band flip to `-fsigned-char`; golden regen for the split [0x860C0] row, suite 26 pass). **Cross-repo follow-up: none** (names pre-curated). **Band note: the libc band's `bzero`/`string` pack is cleared. Next-cleanest libultra C mirror is still `cosf` (gu/cosf.c, 0x82F20) — see Carry-overs; new libultra mirrors now compile `-fsigned-char` by default.**

- **Sprint 66: 2 .c files BANKED — `src/libultra/gu/cosf.c` (`cosf`) + `src/libultra/gu/sinf.c` (`sinf`), libultra gu-band verbatim trig mirrors.** md5-candidate 104→**106** (all 106 .c stub-free); asm subsegs 157→158 (surfaced `gu/translate.c`). Banked the two gu polynomial-trig leaves, each carved out of a pts-13 combined pack via the 8-gate decompose-split (cosf out of `[0x82B80]` 5-fn align/cosf/lookat at 0x82F20+0x83070; sinf out of `[0x85B30]` 3-fn sinf/translate at 0x85CD0). Both verbatim ultralib VERSION_J, 0 iteration, full-make ROM SHA-1 == baserom. **First C `#pragma weak` mirror** (`#pragma weak cosf=__cosf` + `#define fcos __cosf`): KMC gcc 2.7.2 emits both symbols, the curated weak alias resolves clean, no edit (C analog of S58 bcopy `_bcopy=bcopy`). **Shared gate-missed recover-extern** `__libm_qnan_f`=0x800D2640 (size:0x4) — the NaN-path return refd by BOTH fns as anonymous `D_800D2640` (refs-unplaced can't bind an anon label, and the fns were non-primary pack members so refs_unplaced never scanned cosf.c/sinf.c); recovered in-execution. rodata sibling carves: sinf ANONYMOUS `[0xAD960,.rodata,libultra/gu/sinf]` (.o 0x60 exact); cosf NAMED `[0xAD860,.rodata,libultra/gu/cosf]` — **the S64 named-rodata collision CAVEAT was a PHANTOM** (splat carves cleanly with `ghidra_symbols` labels inside; PO-directed byte-compare of the `.o(.rodata)` vs ROM confirmed match-to-libultra before carving). seed 4 / banked 4pt; regime mirror (seed-only). 0 stuck-far/permuter/carried/re-opened (1 gate-missed enabler recovered in-execution, not a spike). Applied 3 of 3 (#1 `docs/hazards.md` .rodata-sibling named-pool note retires the caveat; #2 `pick_target.py PRAGMA_WEAK_RE` keys weak aliases in `build_upstream_index`, golden-inert, suite 29 pass; #3 `docs/hazards.md` `#pragma weak` C-mirror note). **PO course-corrections: verified libultra NOT libkmc** (ROM rodata byte-id to ultralib; libkmc has no cosf/sinf/__libm_qnan_f), and byte-checked cosf's named rodata vs libultra before carving. **Cross-repo follow-up:** `__libm_qnan_f`=0x800D2640 is a new decomp-side symbol — propagate via `sync_decomp_names.py --import-from-decomp` (cosf/sinf names pre-curated). **Band note: ALL low-cost libultra gu C mirrors now exhausted; next-cleanest is the newly-surfaced `gu/translate.c` — see Carry-overs. Tooling follow-up (cross-member refs_unplaced) carried.**

- **Sprint 67: 1 .c file BANKED — `src/libultra/gu/translate.c` (`guTranslateF` + `guTranslate`), libultra gu-band verbatim mirror; the last un-flipped gu asm leaf.** md5-candidate 106→**107** (all 107 .c stub-free). Single 208B 2-fn subseg 0x85CD0 (the leaf S66 surfaced; next boundary 0x85DA0 hasm). Verbatim ultralib `gu/translate.c` (VERSION_J), **byte-identical cp, 0 edits**, matched first `make`, full-make ROM SHA-1 == baserom. The cleanest mirror of the band: both fn names pre-curated (guTranslateF=0x800AA8D0, guTranslate=0x800AA924) → **zero symbol adds**; `guint.h` vendored + callees `guMtxIdentF`/`guMtxF2L` placed → zero header copies; **no float literals → no rodata-sibling split** (simpler than perspective/lookathil). `pack:2fn` was the single-upstream-file false-flag class (atomic mirror). seed 2 / banked 2pt; regime mirror (seed-only). 0 stuck-far/permuter/carried/re-opened. Applied 1 of 2 (PO selected #2 only): `pick_target.py` now tags a pure single-upstream-file C pack `single-file-pack:<n>fn[…]` (vs split-implying `pack`), display-only (pts unchanged), CLAUDE.md index + `docs/hazards.md` synced → `#upstream-mirror-pattern`, golden regen, suite 29 pass; #1 (BACKLOG gu-trail-retired note) NOT selected. **Cross-repo follow-up: none** (names pre-curated). **Band note: the gu band 0x82F20..0x85CD0 is now fully decompiled — no gu mirrors remain. Next libultra work is off the gu band: classical-track jal-mismatch/defines-data leaves or libnusys/libkmc fillers (see Carry-overs + the post-S59 candidate list above).**

- **Sprint 69: 1 .c file BANKED — `src/libultra/gu/lookat.c` (`guLookAtF` + `guLookAt`), libultra gu-band verbatim mirror; the TRUE last un-flipped gu asm leaf, GU BAND CLOSED.** md5-candidate 108→**109** (all 109 .c stub-free); asm subsegs 156→155. Single 1808B 2-fn subseg 0x83070 (the leaf cosf's S66 split surfaced; bounded by lookathil 0x83780). Verbatim ultralib `gu/lookat.c` (VERSION_J), **byte-identical cp, 0 edits, 0 iteration**, matched first `make`, full-make ROM SHA-1 == baserom. **Carry-over note CORRECTED at gate:** lookat is NOT the `vec3f_normalize` substitution class the carry-over claimed (that's align/rotate) — guLookAtF uses `sqrtf` **inline** (`-1.0/sqrtf(...)`), a PURE verbatim mirror like S64 lookathil. Callees `guMtxIdentF`/`sqrtf`×3/`guMtxF2L` all placed; `guLookAt` inlines guLookAtF (-O2 same-TU, the S68 guAlign pattern). **8-gate FIRED (pts-13, SIZE-only) → ran under the verbatim-mirror exemption, GENERALIZED at S69:** the inner boundary guLookAt@0x800A7FF0 is **16-aligned** (unlike S64 lookathil's `non16align`), but it's a `single-file-pack` (both fns from one upstream `.c`) so decompose-blocked regardless — you can't mirror half a source file. rodata sibling carve `[0xAD8C0, .rodata, libultra/gu/lookat]` = `.o(.rodata)` 0x20 exact (anon 6-literal pool 0x800D24C0..0x800D24E0, the whole generic block bounded by lookathil). One gate enabler: `guLookAt`=0x800A7FF0 added to `symbol_addrs.txt` (guLookAtF pre-curated in ghidra_symbols). seed 13 / banked 13pt; regime mirror (seed-only; exemption). 0 stuck-far/permuter/carried/re-opened. Applied 2 of 2 (#1 generalize the S64 exemption condition (b) to `single-file-pack ⇒ decompose-blocked regardless of inner 16-alignment` [CLAUDE.md + VELOCITY]; #2 BACKLOG carry-over `vec3f_normalize` correction). **Cross-repo follow-up:** `guLookAt`=0x800A7FF0 is a new decomp-side symbol → propagate via `sync_decomp_names.py --import-from-decomp`. **Band note: the entire gu band is now decompiled. Next libultra progress is classical-track (ContRam/pfs/cont jal-mismatch stripped impls) or asm-mirror spikes (partial-TU `__osDisableInt`/`__osRestoreInt`, un-named `func_800ACCC0`/`func_800ACB40`, mixed packs), a scope/track call for the next gate.**

- **Sprint 70: 0 .c files BANKED — asm-mirror vendoring, 2 libultra TLB CP0 primitives `osMapTLB` + `osUnmapTLB`.** 6th asm-mirror sprint; first to **identify an un-named (`func_<addr>`) TU at the gate**. The two carry-over "no identified ultralib `.s` yet" primitives were named instantly by their CP0/TLB signature — `func_800ACB40` (0x87F40) = `osMapTLB` (`os/maptlb.s`), `func_800ACCC0` (0x880C0) = `osUnmapTLB` (`os/unmaptlb.s`) — one MCP `disassemble_function` each (mfc0/mtc0 Index/EntryHi/EntryLo0/1 + tlbwi). Vendored verbatim → `src/libultra/os/`, +2 `VENDOR_ASM` pairs, +2 curated names, flipped 2 subsegs `asm`→`hasm` (asm subsegs 155→153, hasm 19→21; md5-candidate unchanged 109/109 — asm-mirror is asm→hasm). Both byte-id to ROM disasm (the `_DEBUG && __sgi` block compiles out under KMC), full-make ROM SHA-1 == baserom **first make, 0 iteration**; `.o(.text)` 0xC0/0x40 = 0xB4/0x3C 16-padded (KMC-`as`) → exact slot fill (0x88000 probetlb, 0x88100 unmaptlball follow). Zero enablers beyond the flip+vendor: all macro/header infra pre-present (MFC0/MTC0 S56, C0_*/TLBLO_*/K0BASE in `PR/R4300.h`, ta0=$12 o32); `os_tlb.h` already declared both prototypes; no in-tree C caller used the `func_` names. seed 2 / banked 2pt; regime mirror (seed-only; 8-gate clear). 0 stuck-far/permuter/carried/re-opened. Applied 1 of 2 (PO selected #1): `pick_target.py` `privileged_asm` fingerprint — an un-named asm subseg holding a privileged op gcc never emits (`tlbwi`/`mtc0`/`mfc0`/`eret`/`cfc1`/…) now flags `intrinsic-likely:cp0-asm(identify-TU)`, *broader* than the pure-shim `intrinsic_likely` (fires through branches/loads/`jal`s), so hand-asm stops being mis-surfaced as classical (caught the 496B CP0 routine `audio_sched_thread_entry`@0x800B3E50, prior `-`); +4 unit tests, CLAUDE.md index + `docs/hazards.md` synced, golden regen (1 vetted row, isolated via stash diff), suite 33 pass. #2 (asm-mirror gate-note) NOT selected. **Cross-repo follow-up:** `osMapTLB`=0x800ACB40 + `osUnmapTLB`=0x800ACCC0 are new decomp-side symbols → propagate via `sync_decomp_names.py --import-from-decomp`. **Band note: the source-confirmed single-fn asm-mirror TUs are now exhausted (the two un-named TLB primitives were the last). What remains is spikes — the partial-TU `__osDisableInt`/`__osRestoreInt` carve off `setintmask.s`, the mixed packs (`osSetIntMask`, `func_800AFB90`) — or the classical-track ContRam/pfs/cont jal-mismatch stripped impls. The `privileged_asm` flag (S70 #1) will now self-surface any remaining un-named hand-asm. A scope/track call for the next gate.**

- **Sprint 71: 1 .c file BANKED — `src/libultra/io/crc.c` (`__osContAddressCrc` + `__osContDataCrc`), libultra io verbatim mirror; FIRST coddog-driven target.** md5-candidate 109→**110** (all 110 .c stub-free); asm subsegs 153→152. Single 240B 2-fn subseg 0x8CC20 (= crc.o `.text` 0xF0 exact, bounded by sirawwrite 0x8CBD0 / `osMapTLBRdb` hasm 0x8CD10). Verbatim ultralib `src/io/crc.c` (VERSION_J branch), **byte-identical cp, 0 edits, 0 iteration**, matched first `make`, full-make ROM SHA-1 == baserom. **The coddog payoff (PO directive at the S71 gate):** `pick_target` mis-seeded the subseg **pts-13 classical** (`pack:2fn`, `upstream none`) ONLY because both fns were un-named; `coddog compare2` (MG64 ELF vs combined ultralib-J ELF) matched both at 99.99% → a trivial verbatim 2-fn mirror. **Zero callees** (nm: no `U` syms — pure CRC integer math), only `PR/os_internal.h` (in-tree), no float literals → no rodata sibling. `__osContDataCrc` takes `u8*` (unsigned) → no char-signedness. Two gate enablers: `__osContAddressCrc`=0x800B1820 + `__osContDataCrc`=0x800B188C added to `symbol_addrs.txt` (both un-named). **8-gate FIRED (pts-13) → cleared by the verbatim-mirror exemption** (regime mirror + single-file-pack decompose-blocked + zero callees + names curated at gate); the pts-13 was a coddog-refuted FALSE price (the S43 tooling-artifact class), so logged at the corrected mirror seed. seed 2 / banked 2pt; regime mirror (seed-only). 0 stuck-far/permuter/carried/re-opened. Applied 4 of 4: #1 coddog gate-step (`docs/hazards.md#coddog-cross-ref` + CLAUDE.md index/preamble), #2 `pick_target.py` reads `tools/coddog/coddog_map.tsv` → `coddog-mirror:<file>@<pct>` flag + ≥99% non-audio re-price as libultra mirror (+ env-overridable `CODDOG_MAP`, golden regen, new repricing test), #3 this regime note, #4 the `tools/coddog_sweep.sh`/`parse_map.py` harness + `make coddog-sweep` target. **Bonus: unlocks `__osContRamRead`/`__osContRamWrite`** — their `calls-unplaced` CRC helpers are now placed (visible in the regenerated golden). **Cross-repo follow-up:** `__osContAddressCrc`=0x800B1820 + `__osContDataCrc`=0x800B188C are new decomp-side symbols → propagate via `sync_decomp_names.py --import-from-decomp`.

- **Sprint 72: 2 .c files BANKED — `src/libultra/io/epirawread.c` (`__osEPiRawReadIo`) + `pfsselectbank.c` (`__osPfsSelectBank`), libultra io coddog-mirror pair.** md5-candidate 110→**112** (all 112 .c stub-free); asm subsegs 152→150. 2nd coddog cross-ref sprint — the S71 map revealed both as un-named 99.99% verbatim mirrors. **epirawread** (368B 0x8BB10, read-twin of banked epirawwrite): EPI_SYNC+IO_READ via vendored `piint.h`; its bare upstream `assert(data != NULL)` (outside `_DEBUG`) `_DEBUG`-wrapped per the banked **sirawread** convention — NDEBUG undefined here, so a bare assert emits `jal __assert`; read==write subseg size (both 0x170) confirms the ROM strips it (new `#assert-strip` hazard). **pfsselectbank** (112B 0x89D20): include adapted `PRinternal/controller.h`→`"controller.h"` (internal path), callee `__osContRamWrite` pre-placed @0x800AF610, BLOCKSIZE/CONT_BLOCK_DETECT/OSPfs in vendored controller.h. Both byte-clean first build, 0 edits, 0 iteration, full-make ROM SHA-1 == baserom. Two gate enablers: `__osEPiRawReadIo`=0x800B0710 + `__osPfsSelectBank`=0x800AE920 added to `symbol_addrs.txt`. seed 2+2=4 / banked 4pt; regime mirror (seed-only, 8-gate clear). 0 stuck-far/permuter/carried/re-opened. **The remaining coddog io leaves are traps, not clean (the S72 #1 fix now surfaces them at the gate):** piacs.c = defines-data (`__osPiAccessQueueEnabled`) + file-static (`static piAccessBuf`) → re-priced pts-3→5; motor.c = defines-data (`__osMotorinitialized[]`) + file-static; contquery = pts-8 c-combined/8-gate. Applied 2 of 2: #1 `pick_target.py build_rows` coddog trap re-scan (re-run defines_data/file_static/needs_header on the coddog-resolved `.c`; shared `_tagged_missing_includes` + `_coddog_upstream_path`; +1 unit test, golden regen map-free, suite 35 pass), #2 `docs/hazards.md#assert-strip` playbook + CLAUDE.md index row + coddog-cross-ref trap-re-scan note. **Cross-repo follow-up:** `__osEPiRawReadIo`=0x800B0710 + `__osPfsSelectBank`=0x800AE920 are new decomp-side symbols → propagate via `sync_decomp_names.py --import-from-decomp`. **Band note: the clean single-fn coddog io leaves are now exhausted — what remains in io is the defines-data/file-static traps (piacs/motor, need a `.data`/`.bss` sibling carve or classical) and the pts-8 contquery c-combined pack.**

- **Sprint 73: 1 .c file BANKED — `src/libultra/gu/position.c` (`guPositionF` + `guPosition`), libultra gu coddog-mirror; closes the gu text band.** md5-candidate 112→**113** (all 113 .c stub-free); asm subsegs 150→149. 3rd coddog cross-ref sprint. Single 960B 2-fn subseg 0x85060 (bounded perspective 0x84CE0 / random 0x85420), the **last flippable asm in the gu band** (only the permanent `0x85DA0 hasm` remains → gu text band now fully decompiled). Verbatim ultralib `src/gu/position.c` (coddog 99.99 → both fns), **byte-identical cp, 0 edits, 0 iteration**, full-make ROM SHA-1 == baserom. `guPositionF` does the trig (sinf×3/cosf×3); `guPosition` inlines guPositionF + calls `guMtxF2L` (both load the same `D_800C81B0` dtor). **defines-data .data carve:** the function-local `static float dtor = 3.1415926/180.0` re-emits a 16B `.data` (4B + 12B KMC pad) → carve `[0xA35B0, .data, libultra/gu/position]` + a continuation `[0xA35C0, data]` for random's `xseed` remainder; align.c/rotate.c are the identical 16B precedents (S61/S68). Callees `sinf`/`cosf`/`guMtxF2L` pre-placed; header `guint.h` same-dir; no `.rodata` (1.0/0.0 inline immediates). Two gate enablers: `guPositionF`=0x800A9C60 + `guPosition`=0x800A9E38 added to `symbol_addrs.txt`. seed 3 / banked 3pt; regime mirror (seed-only, 8-gate clear). 0 stuck-far/permuter/carried/re-opened. **Trap caught at the gate:** the coddog row UNDER-flagged it (clean pts-3, no defines-data) even after the S72 re-scan — the asm-side `data-static` pre-flag (S52) doesn't fire on un-named coddog candidates, and the S72 re-scan ran only `defines_data_globals` (skips `static`, scans only brace-depth 0), so guPositionF's fn-local static was doubly invisible; found by reading the upstream + asm data refs. Applied 2 of 2: #1 `pick_target.py` `defines_local_static_data()` — flags function-body `static <type> <name> = <init>;` as `defines-data` on BOTH the named + coddog re-scan paths (the source-side backstop; verified on position/rotate/align/rotaterpy, clean on perspective/epirawread; suite 35 pass, no golden regen), #2 `docs/hazards.md#defines-data` S73 source-side-backstop note. **Cross-repo follow-up:** `guPositionF`=0x800A9C60 + `guPosition`=0x800A9E38 are new decomp-side symbols → propagate via `sync_decomp_names.py --import-from-decomp`. **Band note: the gu band is text-complete; the remaining coddog mirror targets are `func_800A7190`→io/contquery (pts-8 pack), `func_800772B0`→os/settime (pts-13 pack), and the io defines-data/file-static traps (piacs/motor).**

- **Sprint 74: 1 .c file BANKED — `src/libultra/io/contreaddata.c` (`osContStartReadData` + `osContGetReadData` + `__osPackReadData` static), libultra io coddog-mirror; decomposed the pts-8 cont subseg.** md5-candidate 113→**114** (all 114 .c stub-free). 4th coddog cross-ref sprint. The `0x82590` subseg is a c-combined of TWO upstream files (contquery.c head + contreaddata.c tail); pts-8 tripped the 8-gate → **decomposed at the upstream-file boundary** (16-aligned split rom 0x82630 / vram 0x800A7230: keep `[0x82590, asm]` for the contquery head, add `[0x82630, c, libultra/io/contreaddata]` for the tail). Banked the clean tail — its only non-placed-non-data callee is its own in-file static `__osPackReadData`, vs the head's two adjacent-subseg fn recoveries. Verbatim ultralib `src/io/contreaddata.c` (VERSION_J), **byte-identical cp, 0 edits, 0 iteration**, full-make ROM SHA-1 == baserom. **Pure text mirror** (no ld-section sibling: static helper is text, constants are immediates, all touched data lives in still-asm controller.c). **Heaviest recover-extern load to date — 6 gate enablers, all found by MANUAL recon (the ranker under-flagged):** the un-named `func_` members blocked the named-keyed refs/calls-unplaced scan, so the row showed NONE despite 3 SI callees (`__osSiGetAccess`=0x800AC164, `__osSiRawStartDma`=0x800AC060, `__osSiRelAccess`=0x800AC1D0) + 3 data globals (`__osContPifRam`=0x800FE340 size:0x40, `__osContLastCmd`=0x8012F4CC size:0x1, `__osMaxControllers`=0x80105274 size:0x1) being unplaced; `D_800FE37C`=__osContPifRam+0x3C (pifstatus addend, auto-resolved). Include reconciliation: verbatim `PRinternal/{controller,siint}.h` → bare (in-tree at `internal/`, the pfsselectbank.c convention; SHA-neutral). seed 5 / banked 5pt; regime mirror (seed-only; 8-gate resolved by decompose). 0 stuck-far/permuter/carried/re-opened. Applied 2 of 2: #1 `pick_target.py` `_append_recover_hazards()` factored out + run on the coddog re-scan path (unions refs/calls-unplaced over the coddog-resolved upstream so an un-named func_'s recover-extern load is priced at the gate — the deferred S66 #2 cross-member-union half; golden regen, suite 35 pass), #2 `pick_target.py` `already_vendored_intree_path()` — the `(already-vendored)` header tag now shows the in-tree adapt target `(already-vendored,adapt->internal/<h>)` so the include-line edit is priced at the gate (golden regen, suite 35 pass). **Cross-repo follow-up:** 6 new decomp-side symbols (`__osSiGetAccess`/`__osSiRawStartDma`/`__osSiRelAccess`/`__osContPifRam`/`__osContLastCmd`/`__osMaxControllers`) → propagate via `sync_decomp_names.py --import-from-decomp`. **Band note: the contquery.c head (osContStartQuery/osContGetQuery) is the near-free next coddog sibling (carried below); remaining coddog targets are os/settime (pts-13) + the io defines-data/file-static traps (piacs/motor).**

- **Sprint 75: 1 .c file BANKED — `src/libultra/io/contquery.c` (`osContStartQuery` + `osContGetQuery`), libultra io coddog-mirror; banked the asm HEAD left by S74's split.** md5-candidate 114→**115** (all 115 .c stub-free); asm subsegs 170→169. 5th coddog cross-ref sprint, and a **mechanical replay of the S74 carry-over** (the model near-free-retry pre-scope). The `0x82590` subseg is now fully C (S74 took the contreaddata tail; S75 takes the contquery head). Verbatim ultralib `src/io/contquery.c` (VERSION_J, coddog 99.99), **byte-identical cp, 0 edits, 0 iteration**, full-make ROM SHA-1 == baserom; flip `[0x82590, asm]`→`[0x82590, c, libultra/io/contquery]`. **Pure text mirror** — defines no data/static (only refs the placed externs `__osContPifRam`=0x800FE340 / `__osContLastCmd`=0x8012F4CC), no ld-section sibling, no rodata (immediates only). Four gate enablers, all pre-listed verbatim-correct in the S74 carry-over (0 rework): 2 fn names `osContStartQuery`=0x800A7190 + `osContGetQuery`=0x800A7210, 2 `calls-unplaced` callees `__osPackRequestData`=0x800A7660 + `__osContGetInitData`=0x800A75AC (jal targets re-confirmed via Ghidra `disassemble_function`; both defined in the still-asm controller.c 0x82810 subseg). Include adaptation `PRinternal/{controller,siint}.h` → bare (the S74 sibling convention; SHA-neutral). seed 5 / banked 5pt; regime mirror (seed-only; 8-gate clear at 5<8). 0 stuck-far/permuter/carried/re-opened. Applied 2 of 2: #1 `pick_target.py build_rows` — suppress the weaker `maybe-upstream` IDF guess when a ≥99% non-audio `coddog-mirror` hit is on the SAME row (func_800A7190 carried both `coddog-mirror:src/io/contquery.c@99.99` AND a mis-pointed `maybe-upstream:voice*`; the definitive identity now stands alone), +1 unit test `test_coddog_suppresses_maybe_upstream`, golden regen for the contquery flip, suite 36 pass; #2 `BACKLOG.md ## Carry-overs` two-kind format (spike vs near-free-retry) + a 5-point near-free-retry **completeness checklist** (flip line / placed-ref inventory / new-recovery vrams / include-adapt / upstream pin), + a `sprint-review.md` Step-5.4 pointer — codifies the S74 carry-over that made this sprint a replay. **Cross-repo follow-up:** 4 new decomp-side symbols (`osContStartQuery`/`osContGetQuery`/`__osPackRequestData`/`__osContGetInitData`) → propagate via `sync_decomp_names.py --import-from-decomp`. **Band note: the clean coddog io leaves are now mined out — only the piacs/motor defines-data+file-static traps remain in io (need a `.data`/`.bss` sibling carve or classical), plus os/settime (pts-13 pack) elsewhere.**

- **Sprint 76: 1 .c file BANKED — `src/libultra/io/devmgr.c` (`__osDevMgrMain`), libultra io coddog verbatim mirror; first switch-jump-table `.rodata` sibling.** md5-candidate 115→**116** (all 116 .c stub-free); asm subsegs 148→147. The smallest clean libultra coddog mirror remaining — `__osDevMgrMain` is NAMED, so the coddog-mirror flag didn't surface (it fires only on un-named `func_`), but the coddog map confirms `src/io/devmgr.c`@99.99. Verbatim ultralib `src/io/devmgr.c` (VERSION_J), `cp` + 1 include adapt (`PRinternal/piint.h`→bare, S72 epirawread convention), **0 body edits, full-make ROM SHA-1 == baserom**; flip `[0x7E9F0, asm]`→`[0x7E9F0, c, libultra/io/devmgr]`. **Both flagged hazards were the indirect-call false-positive class** — `jal-count-mismatch:25vs21` + `calls-unplaced:dma,edma` = 4 `jalr` calls through `dm->dma`/`dm->edma` (OSDevMgr struct fn-ptr members; C-call-counted, not `jal`; 25−4=21) — so the candidate was correctly NOT routed to classical. Zero symbol adds (`__osDevMgrMain`=0x800A35F0 pre-curated), all 7 callees (osRecvMesg/osSendMesg/int-mask pair/__osEPiRawWriteIo/__osEPiRawReadIo[S72]/osYieldThread[S12]) + all LEO/OSDevMgr macros pre-placed. **In-execution surprise (resolved, NOT a spike):** the `switch (mb->hdr.type)` compiles to a jump table `jtbl_800D2280`@0x800D2280/ROM 0xAD680 whose 7 `.word .L800A39xx` entries are the fn's own internal labels → flipping text→C deleted them → undefined-ref link break the gate's text-only green-ROM check cannot catch by construction. Fixed with a `.rodata` sibling carve `[0xAD680, .rodata, libultra/io/devmgr]` (split AD5E0 at 0xAD680); the C-compiled jtbl reproduced the 8-word block (7 case + 1 zero pad to 0x20) byte-for-byte. seed 3 / banked 3pt; regime mirror (seed-only; 8-gate clear at 3<8). 0 stuck-far/permuter/carried/re-opened. Applied 3 of 3: #1 `pick_target.py` `rodata-jtbl:0x<vram>` pre-flag (jump-table analog of `rodata-literal`; `decomp_asm.rodata_jtbls` scans `%lo(jtbl_…)` whole-subseg, wired in the SHARED recover-battery `_append_recover_hazards` so it prices both named-upstream + coddog paths; display-only, +3 unit tests, golden regen, suite 39 pass); #2 `docs/hazards.md#rodata-sibling-yaml-pattern` switch-jtbl sub-case (automatic byte-match + zero-pad + gate-can't-catch) + S76 provenance; #3 `CLAUDE.md` hazard-index `rodata-jtbl` row. **Cross-repo follow-up: none** (name pre-curated; zero new decomp-side symbols). **Band note: the next-cleanest libultra coddog mirrors are the two NAMED timer fns `osSetTimer`→`src/os/settimer.c` + `__osSetTimerIntr`→`src/os/timerintr.c` (both @99.99, verify clean at the gate); the io defines-data/file-static traps (piacs/motor) + os/settime pts-13 multi-file pack remain.**

- **Sprint 77: 4 .c files BANKED — `src/libultra/io/{spgetstat,spsetstat,spsetpc,sprawdma}.c`
  (`__osSpGetStatus` + `__osSpSetStatus` + `__osSpSetPc` + `__osSpRawStartDma`), libultra io
  coddog-mirror cluster; cleared the c-combined subseg [0x8CAA0].** md5-candidate 116→**120** (all
  120 .c stub-free); asm subsegs 147→146. 6th coddog cross-ref sprint. The 4-fn `c-combined:3file`
  subseg 0x8CAA0 (size 224, pts-13) tripped the 8-gate → **decomposed at the upstream-file boundary**
  (4-way split, all 16-aligned: 0x8CAA0/AB0/AC0/AF0). Verbatim ultralib VERSION_J: spgetstat/spsetstat/
  spsetpc pure `cp` (IO_READ/IO_WRITE SP regs; only `PR/rcp.h`+`PR/os_internal.h`, both in-tree);
  sprawdma assert-strip (3 bare asserts `_DEBUG`-wrapped per the S72/sirawread convention; asm-confirmed
  no `jal __assert`; callees `__osSpDeviceBusy`=0x800B1680 + `osVirtualToPhysical`=0x800A7720 placed).
  **Pure text mirrors** (register immediates only → no data/rodata siblings). **The un-named leader
  `func_800B16A0` was confirmed `__osSpGetStatus` at the gate via Ghidra `disassemble_function`**
  (`lui 0xa404; ori 0x10; lw` = `IO_READ(SP_STATUS_REG)`; coddog couldn't fingerprint a 4-instr fn) →
  the 3-file c-combined became a clean 4-file clear. One symbol add at the gate (`__osSpGetStatus`=
  0x800B16A0; the other 3 pre-curated). **Gate surprise (resolved, NOT a spike):** the symbol add
  renamed `func_800B16A0` in the scaffold → the banked classical caller `src/main/func_800AB600.c`
  (`func_800B16A0()`) failed to link → renamed call site to `__osSpGetStatus` (same addr, SHA-neutral);
  caught by the green-ROM gate check. seed 4 / banked 4pt (4× warm io single-fn mirror seed-1); regime
  mirror (seed-only; 8-gate resolved by decompose). 0 stuck-far/permuter/carried/re-opened. Applied
  1 of 1: #1 `pick_target.py` `caller-evict:<func_vram>@<file>` pre-flag (`src_func_callers()` walks
  `src/` for un-named members a banked C file references, INCLUDE_ASM-excluded; display-only) +
  `docs/hazards.md#caller-evict` + CLAUDE.md hazard-index row; +1 unit test `test_caller_evict_flag`,
  golden regen (suite 40 pass). **Cross-repo follow-up:** `__osSpGetStatus`=0x800B16A0 is a new
  decomp-side symbol → propagate via `sync_decomp_names.py --import-from-decomp` (other 3 pre-curated).
  **Band-note correction (S76 note was stale): `osSetTimer`/`osGetTime` are ALREADY banked
  (`src/libultra/os/{settimer,gettime}.c`); the S76 "next = settimer/timerintr" recommendation is
  moot. The remaining os-timer asm is `timerintr.c` (the `[0x87C40]` 4fn pack: __osTimerServicesInit/
  __osTimerInterrupt/__osSetTimerIntr/__osInsertTimer — a defines-data pack, NOT a clean atomic
  mirror). Next-cleanest libultra coddog mirrors: the io `[0x8CE90]` pack (gbpaksetbank+pfsisplug,
  both callees placed) and os/settime (single fn buried in a 6fn pack at 0x526B0); then the
  defines-data/file-static traps (piacs/motor, contpfs, sched, timerintr).**

- **Sprint 78: 2 .c files BANKED — `src/libultra/io/gbpaksetbank.c` (`__osGbpakSetBank`) +
  `pfsisplug.c` (`osPfsIsPlug` + `__osPfsRequestData` + `__osPfsGetInitData`), libultra io coddog
  mirrors; cleared the c-combined subseg [0x8CE90].** md5-candidate 120→**122** (all 122 .c stub-free);
  asm subsegs 146→145. 7th coddog cross-ref sprint; the S77 band-note's top recommendation. The 4-fn
  c-combined subseg 0x8CE90 (size 928, ~pts-8/13) tripped the 8-gate → **decomposed at the
  upstream-file boundary** (split 0x8CF50/vram 0x800B1B50, 16-aligned): gbpaksetbank HEAD (1fn) +
  pfsisplug TAIL (3fn). Verbatim ultralib VERSION_J both, 0 iteration, full-make ROM SHA-1 == baserom.
  **gbpaksetbank pure `cp`** (callees `__osContRamWrite`[S43] + `osGbpakInit`[S44] placed; hdrs
  os_internal.h/controller.h vendored; no data/static/rodata). **pfsisplug defines-data fast-path** —
  defines `OSPifRam __osPfsPifRam`@0x801B7EF0 (64-byte BSS global in the shared main `bss` blob); per
  the S44/S45 fast-path, dropped the def → vendored `internal/controller.h:227` extern + `symbol_addrs
  __osPfsPifRam=0x801B7EF0 size:0x40`, **NO `.bss` carve** (the sized blob already reserves the range;
  bss has no ROM bytes → SHA-neutral). Include adapt `PRinternal/{controller,siint}.h`→bare (kept
  `PRinternal/macros.h`). 3 gate fn-name adds (osPfsIsPlug=0x800B1B50/__osPfsRequestData=0x800B1CCC/
  __osPfsGetInitData=0x800B1D70; __osGbpakSetBank pre-placed S45). coddog fuzzy-labelled func_800B1D70
  as controller.c's `__osContGetInitData` (a near-twin); asm call-structure + verbatim SHA confirm it
  is pfsisplug's `__osPfsGetInitData` (distinct from `__osContGetInitData`@0x800A75AC, S74). seed 4 /
  banked 4pt; regime mirror (seed-only). 0 stuck-far/permuter/carried/re-opened. Applied 1 of 1: #1
  `pick_target.py build_rows` — scan ALL subseg members for a definitive coddog hit (not just `fns[0]`)
  → surface the tail identity under a named/mis-attributed leader, AND exempt a coddog-identified
  subseg from the over-broad `carried` name-drop filter (`carry_over_names()` scoops every backticked
  token from the digest log; S45 name-dropped `__osGbpakSetBank` → [0x8CE90] was de-ranked invisible);
  +2 unit tests, golden stable, suite 42 pass. **Cross-repo follow-up:** `osPfsIsPlug`=0x800B1B50,
  `__osPfsRequestData`=0x800B1CCC, `__osPfsGetInitData`=0x800B1D70, `__osPfsPifRam`=0x801B7EF0 are new
  decomp-side symbols → propagate via `sync_decomp_names.py --import-from-decomp`. **Tooling follow-ups
  logged (NOT applied this gate):** (a) `UPSTREAM_DEF_RE` matches forward prototypes (`...);`) as defs
  → mis-attributes a fn to a sibling that only declares it (gbpaksetbank→gbpakreadwrite); (b)
  `carry_over_names()` is fundamentally over-broad (222 banked tokens treated as carried) — the coddog
  exemption patches only the coddog subset; a precise carry-over parser would un-de-rank the rest.

- **Sprint 79: 2 .c files BANKED — `src/libultra/io/contramread.c` (`__osContRamRead`) +
  `contramwrite.c` (`__osContRamWrite`), libultra io coddog cont-pak RAM I/O mirror pair; first
  trailing-128-align pad split.** md5-candidate 122→**124** (all 124 .c stub-free); asm subsegs
  145→144 (net: 2 asm flipped to c, +1 nop-pad subseg). 8th coddog cross-ref sprint; the **exact
  pair the S71 note named** as verbatim-mirrorable, smallest-first off the re-priced coddog list.
  Both verbatim ultralib VERSION_J (coddog @99.99), full-make ROM SHA-1 == baserom. Flips `[0x8A820,
  asm]`→`[..,c,libultra/io/contramread]` + `[0x8AA10, asm]`→`[..,c,libultra/io/contramwrite]`.
  **contramread defines-data fast-path** — defines `s32 __osPfsLastChannel = -1` → dropped to
  `extern` per the S44/S45 fast-path + `symbol_addrs __osPfsLastChannel=0x800C9450 size:0x4`
  (recovered from `lui 0x800d`/`lw -0x6bb0`, disjoint), **NO `.data` carve** (the byte already lives
  in the data blob; def→extern leaves the .text unchanged); contramwrite externs it. **contramwrite
  trailing-128-align pad (the one wrinkle, NOT a spike):** the 516B fn (129 instrs byte-identical) is
  followed by 27 trailing nops padding to the 128-aligned `osAfterPreNMI`@0x800AF880; the verbatim C
  compile only 16-aligns its `.text` (`.o`=0x210), dropping the 0x60 residual → ROM 96B short → SHA
  miss on the first `make`, invisible to every gate check (the INCLUDE_ASM stub carries the pad).
  Localized via `.o`-size diff + the extracted-asm trailing-nop run; fixed with a nop-pad split
  `[0x8AC20, asm]` (=0x8AA10+`.o` 0x210) between contramwrite and afterprenmi (`docs/hazards.md:122`
  — no inter-subseg linker ALIGN). The contramread sibling (slot == 16-aligned fn size) mirrored
  clean, no split — the FP guard within one pair. All 10 callees/fn pre-placed (`__osSiGetAccess`/
  `__osSiRawStartDma`/`__osSiRelAccess`/`osRecvMesg`/`bcopy`/`__osContAddressCrc`/`__osContDataCrc`/
  `__osPfsGetStatus`); include adapt `PRinternal/{controller,siint}.h`→bare (kept `PRinternal/macros.h`).
  seed 6 (3+3) / banked 6pt; regime mirror (seed-only; 8-gate clear at 6<8). 0 stuck-far/permuter/
  carried/re-opened. Applied 2 of 2: #1 `pick_target.py` `trailing-pad:<n>B@<align>` pre-flag (new
  `decomp_asm.code_end_rom`; fires only when the next boundary is >16-aligned — the merely-16 /
  delay-nop case is the FP guard) + an **all-nop asm subseg skip** (the pad subseg carries a splat
  glabel but is pure nops; the skip also retired 8 pre-existing all-nop `func_ovl*_801F4A30` overlay
  stubs the ranker surfaced as the "smallest" picks); #2 `docs/hazards.md#trailing-alignment-pad-after-a-c-mirror`
  + the CLAUDE.md hazard-index row; golden regen (8 overlay-stub rows dropped + the new trailing-pad
  flag), suite 42 pass. **Cross-repo follow-up:** `__osPfsLastChannel`=0x800C9450 is a new decomp-side
  data symbol → propagate via `sync_decomp_names.py --import-from-decomp`. **Band note: 3 live
  candidates now carry the `trailing-pad` flag (@32/@64/@128) — those mirrors are priced at the gate.
  The io clean-coddog leaves remain mined out (piacs/motor defines-data+file-static traps + os/settime
  pts-13 pack remain).**

- **Sprint 80: 1 .c file BANKED — `src/libultra/io/pfsgetstatus.c` (`__osPfsGetStatus` +
  `__osPfsRequestOneChannel` + `__osPfsGetOneChannelData`), libultra io coddog mirror; a clean
  single-file 3-fn cp.** md5-candidate 124→**125** (all 125 .c stub-free); asm subsegs 144→143.
  9th coddog cross-ref sprint. Verbatim ultralib VERSION_J `io/pfsgetstatus.c` (coddog @99.99),
  byte-identical body (only `PRinternal/{controller,siint}.h`→bare include adapt), 0 edits, 0
  iteration, first-make ROM SHA-1 == baserom. Flip `[0x89B10, asm]`→`[0x89B10, c,
  libultra/io/pfsgetstatus]`, standalone subseg (no split). Gate enablers: recover
  `__osPfsInodeCacheBank`=0x800C9444 size:0x1 (byte store `li 0xfa; lui 0x800d; sb -0x6bbc(at)`;
  defined by still-asm contpfs.c, extern for us) + 2 sibling names `__osPfsRequestOneChannel`=
  0x800AE800 / `__osPfsGetOneChannelData`=0x800AE894 (`__osPfsGetStatus` pre-placed). **Two flagged
  hazards were pick_target false-positives**, debunked at the gate + FIXED this retro:
  `refs-unplaced:__OSContRequesFormatShort` = a struct TYPE in the (unresolved) `PRinternal/controller.h`;
  `jal-count-mismatch:7vs6` = the non-J `#else` branch's `__osPfsRequestOneChannel(channel)`
  double-counted (NOT the CHNL_ERR macro — the execution-time hypothesis was refuted by reading the
  code at the retro). seed 5 / banked 5pt; regime mirror (seed-only; 8-gate clear at 5<8). 0
  stuck-far/permuter/carried/re-opened. Applied 3 of 3, all `pick_target.py` accuracy fixes (each
  with a regression test): #1 `_resolve_include` basename fallback (vendored-prefix `PRinternal/X.h`→
  `internal/X.h` now scanned → `declared_type_names` sees the typedef → `refs_unplaced` drops it);
  #2 `call_divergence` strips inactive `#if BUILD_VERSION` branches (lib-threaded) so a dead-branch
  call no longer double-counts; #3 factor `_append_coddog_trap_hazards`, called from the S78
  tail-identity block too (so `initialize.c`'s defines-data is priced — its coddog hit keys on the
  sibling `create_speed_param`, not the leader `__osInitialize_common`). Golden stable, suite 45
  pass. **Cross-repo follow-up:** 3 new decomp-side symbols (`__osPfsRequestOneChannel` /
  `__osPfsGetOneChannelData` / `__osPfsInodeCacheBank`) → propagate via
  `sync_decomp_names.py --import-from-decomp`. **Band note: `initialize.c` (os/, pts now 5) is the
  next-cleanest coddog leaf but is a cross-region `.data`-carve + name-reconcile job (see
  Carry-overs); the io clean-coddog leaves remain mined out (piacs/motor traps, os/settime pts-13).**

- **Sprint 81: 1 .c file BANKED — `src/libultra/io/siacs.c` (`__osSiCreateAccessQueue` +
  `__osSiGetAccess` + `__osSiRelAccess`), the SI access-queue file; a verbatim twin of banked
  `io/piacs.c`.** md5-candidate 125→**126** (all 126 .c stub-free); asm subsegs 143→142. 10th coddog
  cross-ref sprint, smallest remaining libultra candidate (240B). coddog matched the TWIN
  `piacs.c@99.99` but the named members (`__osSiGetAccess`/`__osSiRelAccess`) name the real source
  `siacs.c` — mirrored from siacs.c. Drop-def fast path (S42): the 3 file-defined data globals
  dropped → `extern`, placed add-only at asm-recovered vrams (`__osSiAccessQueueEnabled`=0x800C8210
  size:0x4, `__osSiAccessQueue`=0x801EFFB0 size:0x18, `siAccessBuf`=0x800FAA00 size:0x4 — all visible
  as `D_<vram>` in the scaffold asm). Gate enablers: `__osSiCreateAccessQueue`=0x800AC110 (func) +
  flip `[0x87510, asm]`→`[0x87510, c, libultra/io/siacs]` (standalone 240B subseg, no split). 0
  edits, 0 iteration, first-make ROM SHA-1 == baserom. **Bonus unlock:** placing
  `__osSiCreateAccessQueue` resolves the `calls-unplaced` callee the next-up `controller.c`
  (`osContInit` pack) needs. seed 5 / banked 5pt; regime mirror (seed-only; 8-gate clear at 5<8). 0
  stuck-far/permuter/carried/re-opened. Applied 2 of 2: #1 `pick_target.py`
  `coddog-twin:<matched>!=<member-src>` pre-flag (cross-checks coddog basename vs named-member
  upstream basenames; fires `piacs!=siacs`; helper `_append_coddog_twin_hazard` wired into both
  coddog emission sites + CLAUDE.md index row + `docs/hazards.md#coddog-cross-ref` step 5); #2
  `docs/hazards.md#defines-data` gate-safe symbol-add note (a drop-def symbol-add naming an existing
  `D_<vram>` is SHA-neutral at the stub stage — distinct from the S68 execution-only ld-section
  carve; would have avoided this sprint's 2nd `make extract`). Golden stable (committed map is
  coddog-free → twin check inert), suite 45 pass. **Cross-repo follow-up:** 4 new decomp-side symbols
  (`__osSiCreateAccessQueue` / `__osSiAccessQueueEnabled` / `__osSiAccessQueue` / `siAccessBuf`) →
  propagate via `sync_decomp_names.py --import-from-decomp`. **Band note: `siacs` unlocked
  `__osSiCreateAccessQueue` → `controller.c` (`osContInit`, pts-5, defines 6 globals + 4 refs-unplaced)
  is now the next coddog io candidate; still a heavier recover-extern/defines-data mirror, not a clean
  cp.**
  **Band note: next-cleanest libultra coddog mirrors — os/settime (single fn buried in the 6fn pack at
  0x526B0, needs decompose) and the io `[0x8CE90]` pack is now CLEARED; the defines-data/file-static
  traps remain (piacs/motor, contpfs [0x89D90, 7fn @100], sched, timerintr [0x87C40, 4fn]).**

- **Sprint 82: 1 .c file BANKED — `src/libultra/io/controller.c` (`osContInit` +
  `__osContGetInitData` + `__osPackRequestData`), the controller-init file; a clean
  `single-file-pack:3fn` verbatim mirror.** md5-candidate 126→**127** (all 127 .c stub-free); asm
  subsegs 142→141. 11th coddog cross-ref sprint, the **teed-up next-up after S81** (siacs.c was banked
  precisely to place `__osSiCreateAccessQueue`, controller's last real callee). Verbatim ultralib
  VERSION_J `src/io/controller.c`, single-file-pack (atomic, no split), **0 edits, 0 iteration**,
  clean-make ROM SHA-1 == baserom; flip `[0x82810, asm]`→`[0x82810, c, libultra/io/controller]`
  (single 784B 16-aligned block). Drop-def fast path (S42): 7 file-scope data globals dropped →
  `extern` — 3 pre-placed (`__osContPifRam`/`__osContLastCmd`/`__osMaxControllers`), **3 placed at the
  gate** from `osContInit.s` `D_` refs (`__osContinitialized`=0x800C8190 size:0x4,
  `__osEepromTimerQ`=0x801B8A00 size:0x18, `__osEepromTimerMsg`=0x8012F4DC size:0x4), and
  `__osEepromTimer` a **pure drop-def** (defined here but referenced only by still-asm `eeprom.c` → its
  own `D_` resolves it; no extern/placement). Include adaptation `PRinternal/{controller,siint}.h` →
  bare (the `contreaddata.c` sibling convention; all already-vendored). `calls-unplaced:aligned` =
  ALIGNED() macro false-flag. seed 5 / banked 5pt; regime mirror (seed-only; 8-gate clear at 5<8). 0
  stuck-far/permuter/carried/re-opened. Applied 1 of 3: #1 `docs/hazards.md#defines-data` — a
  `D_<vram>` rename symbol-add (gate OR execution) needs a CLEAN rebuild, not incremental, since the
  INCLUDE_ASM `.s` dep is untracked by make (this sprint: the incremental link failed with
  `undefined reference to D_8012F4DC`, masked by a stale-`.z64` SHA; `make clean` fixed it). (#2
  pick_target defines-data referenced-by-self-vs-elsewhere split NOT selected; #3 confirmatory.)
  **Cross-repo follow-up:** 3 new decomp-side data symbols (`__osContinitialized` / `__osEepromTimerQ`
  / `__osEepromTimerMsg`) → propagate via `sync_decomp_names.py --import-from-decomp`. **Band note: the
  io defines-data/file-static traps remain (piacs/motor, contpfs [0x89D90, 7fn @100], vimgr, timerintr
  [0x87C40, 4fn], sched); next-cleanest is sptask [0x867A0, 2fn] but it carries jal-count-mismatch:7vs14
  (asm > C calls) → gate-investigate/classical, plus file-static.**

- **Sprint 83: 1 .c file BANKED — `src/libultra/io/sptask.c` (`osSpTaskLoad` + `osSpTaskStartGo`),
  the RSP task-load file; a clean `single-file-pack:2fn` verbatim mirror.** md5-candidate 127→**128**
  (all 128 .c stub-free); asm subsegs 141→140. 12th coddog cross-ref sprint, the S82-teed-up
  "next-cleanest" io leaf. **Its three gate hazards ALL resolved to false-flags before the flip:**
  `jal-count-mismatch:7vs14` = the `static _VirtualToPhysicalTask` inlined into `osSpTaskLoad` (the 7×
  `jal osVirtualToPhysical` in the asm confirms it; coddog 99.99 holds, NOT a version divergence);
  `calls-unplaced:_osVirtualToPhysical` = the line-11 macro (real callee `osVirtualToPhysical`=0x800A7720
  placed); `needs-header:PRinternal/osint.h` = already-vendored no-op (→ bare `osint.h`). Only real
  work = **drop-def fast path** (S33/S81/S82): `static OSTask tmp_task` → `extern`, placed add-only at
  the asm-recovered `tmp_task`=0x800FA9C0 size:0x40 (.bss `ADD30.bss.s`, abuts S81 `siAccessBuf`).
  Gate flip `[0x867A0, asm]`→`[0x867A0, c, libultra/io/sptask]` (standalone 576B 16-aligned, no split).
  **One in-execution divergence (NEW class, invisible to the gate stub):** verbatim body LINKED clean
  but full-make SHA-1 missed by EXACTLY ONE WORD @0x800AB504 — `IO_READ(...+OS_YIELD_DATA_SIZE-4)`
  emitted `0x8FC` vs baserom `0xBFC`. Root cause: `PR/sptask.h` guards `OS_YIELD_DATA_SIZE` 0xc00
  (GBI-microcode defined) vs 0x900 (#else) and `LIBULTRA_CFLAGS` had no GBI define. Fix (PO directive):
  `+LIBULTRA_CFLAGS -DF3DEX_GBI_2` (MG64's actual microcode; same 0xc00 guard as ultralib's default
  -DF3DEX_GBI). Clean rebuild → ROM SHA-1 == baserom, every other banked libultra file SHA-1-neutral;
  0 C-body iterations. seed 5 / banked 5pt; regime mirror (seed-only; 8-gate clear at 5<8). 0
  stuck-far/permuter/carried/re-opened. Applied 3 of 3: #1 `pick_target.py` GBI-value-guard pre-flag
  (parse `LIBULTRA_CFLAGS` → libultra active-define set; `gbi_value_guard_needs_define` flags a
  candidate using a GBI-guarded macro when no guard define is active — dormant while -DF3DEX_GBI_2
  stands) +1 unit test, golden-neutral, suite 46 pass; #2 `docs/hazards.md#needs-define` GBI-microcode
  sub-case + the 1-word SHA-miss tell; #3 byte `.o`-diff localization (confirmatory, S44 doctrine).
  **Cross-repo follow-up:** `tmp_task`=0x800FA9C0 is a new decomp-side data symbol → propagate via
  `sync_decomp_names.py --import-from-decomp` (the 2 fn names were already in `ghidra_symbols.txt`).
  **Band note: the standing -DF3DEX_GBI_2 now pre-satisfies any libultra mirror using a GBI-guarded
  macro. The io defines-data/file-static traps remain (motor [pack:2fn, pts-8], contpfs [0x89D90, 7fn
  @100], vimgr [0x88210, 2fn, un-named member], timerintr [0x87C40, 4fn, pts-8, 9 defines-data], sched
  [0x86A50, 15fn, pts-13]); osSetIntMask [0x7E360] needs a 3-way TU split (setintmask.s hasm + pimgr +
  epirawdma).**

- **Sprint 84: 2 units BANKED — `src/libultra/io/epirawdma.c` (`__osEPiRawStartDma`, C mirror) +
  `src/libultra/os/setintmask.s` (`osSetIntMask`, hasm asm-vendor); cleared 2 of 3 members of the
  `[0x7E360]` c-combined io pack.** md5-candidate 128→**129** .c (all stub-free); hasm 21→**22**; asm
  140→140. The gate split `[0x7E360, asm]` 3-way at the upstream-file boundaries (all 16-aligned):
  `[0x7E360, asm]`(setintmask) + `[0x7E400, asm]`(pimgr) + `[0x7E590, c, libultra/io/epirawdma]`,
  validated green. **epirawdma** = clean verbatim ultralib VERSION_J io mirror: jal=1
  (`osVirtualToPhysical` S7), `__osCurrentHandle`=0x800C7E90 placed, include adapt = sibling epidma.c's
  `#include "piint.h"`, no rodata, name pre-curated → zero symbol adds, first-build SHA == baserom, 0
  iteration. **setintmask** = first VENDOR_ASM `.s` carrying a `.rodata` LUT (`__osRcpImTable`, 64-`.half`
  @0x800D2200/rom 0xAD600, ref'd cross-TU by exception dispatch `asm/8AF90.s`). **Novel:** splat
  auto-links a hasm `.o`'s `.rodata` at the section END (out of address order, see `.ld` 823B0/824E0)
  → vendoring the full `.s` would duplicate+misplace the 0x80B → SHA break. Resolved by vendoring
  `.text` only (strip the `.rdata` block) + keeping the LUT as the extracted generic blob renamed via
  `symbol_addrs += __osRcpImTable=0x800D2200 size:0x80` (D_<vram> rename → clean rebuild; `7E360.o`
  confirmed `.text`-only 0xA0); VENDOR_ASM += `7E360:src/libultra/os/setintmask.s`; flip
  `[0x7E360, asm]`→`[0x7E360, hasm]`. First-build SHA == baserom, 0 iteration. seed 4 / banked 4pt;
  regime mirror (seed-only; 8-gate clear). 0 stuck-far/permuter/re-opened; 1 carried (pimgr, planned).
  Applied 3 of 3: #1 `docs/hazards.md#asm-mirror-vendoring` vendored-`.s`-with-`.rodata` sub-case (+
  CLAUDE.md index row); #2 `pick_target.py` needs-define skips a `.s`'s own `#define`s (MI_INTR_MASK
  FP); #3 `pick_target.py` `has-rodata:<sym>` pre-flag; `make test-tools` 46 pass, golden-neutral.
  **Cross-repo follow-up:** `__osRcpImTable`=0x800D2200 is a new decomp-side data symbol → propagate
  via `sync_decomp_names.py --import-from-decomp` (the 2 fn names were already in `ghidra_symbols.txt`).
  **The `[0x7E360]` pack now has only `pimgr` (osCreatePiManager) left → carry-over below.**

- **Sprint 104: 1 .c file BANKED — `src/libultra/libc/xprintf.c` (`_Printf` + `_Putfld`), the
  libultra printf formatting engine; PO picked it as a "classical spike", the gate UNMASKED a verbatim
  MIRROR (S13 precedent).** md5-candidate 152→**153** (all .c stub-free); asm subsegs ~125 (+1
  func_800B1580 split subseg). The gate asm-vs-upstream check defused four misleading plan-gate tags:
  `jal-count-mismatch:14vs3` was pure jalr-vs-jal (every `(*pfn)` output is `jalr s4`; the 3 jals =
  strchr×2 + _Putfld match upstream `_Printf` exactly), `single-file-pack:3fn` was wrong (upstream
  xprintf.c defines ONLY 2 fns — `func_800B1580` is a separate `__osDpDeviceBusy` TU, split off at the
  gate `[0x8BF30,asm]`→`[0x8BF30,c,libultra/libc/xprintf]`+`[0x8C980,asm]`, 16-aligned), and
  `calls-unplaced:pfn` / `refs-unplaced:__PTRDIFF_TYPE__` were a fn-ptr param + a compiler typedef
  macro. Warm libc band (xlitob S92 / xldtob S93 / string.c all banked) C-resolved all 4 callees
  (`_Litob`/`_Ldtob`/`strchr`/`strlen`) → ZERO recovery, ZERO symbol adds (`_Printf`/`_Putfld`
  pre-curated in ghidra_symbols). Byte-identical cp of ultralib VERSION_J `src/libc/xprintf.c`; **dual
  carve** sized by `objdump -h xprintf.o`: `.data` `[0xA48B0,.data]` 0x50 (spaces[33]+zeroes[33], the
  S92/S101 un-flagged init-static-array class) + `.rodata` `[0xADA50,.rodata]` 0x178 (fchar/fbit/"hlL"/
  _Putfld switch jtbl; carve-start 0x10 past the FOREIGN leading `__libm_qnan_f`@0xADA40, bounded by
  xldtob's `[0xADBD0]`). 8-gate FIRED at pts-13 → resolved by DECOMPOSE (split off func_800B1580) +
  enabler-forward (S101 env / S93 xldtob carve-residual single-file mirror precedent; banks
  atomically). **First-build full-make ROM SHA-1 == baserom, 0 iteration.** regime mirror → seed-only,
  banked 13pt. 0 stuck-far/permuter/carried/re-opened. Applied **4 of 4**: #1 `_c_jal_count` drops the
  .c's own function-like macros (PUT/PAD) + `calls_unplaced` skips fn-ptr params (the jalr-vs-jal +
  pfn false flags) + unit tests; #2 `upstream-fncount-mismatch:<m>vs<n>` (foreign-TU-in-single-stem-
  pack) on a depth-aware `_iter_upstream_functions` rewrite (counts single-token K&R `_xatan` +
  leading-space defs, skips protos/#define/doc-comments → `_xatan`/`_xsincos` correctly relabel
  single-file-pack) + unit test; #3 `data-carve:<names>` .data init-static-array detector
  (`defines_file_static_init_array`, single-file-pack subset) + unit test; #4 `docs/hazards.md`
  carve-start-past-foreign-leading-symbol + `.o`-section-size extent oracle. suite +4 tests pass,
  golden regen (func_800B1580 added; _xatan/_xsincos pack→single-file-pack; _Printf banked). No
  carry-over (func_800B1580 is a foreign asm TU, never in scope). **Cross-repo follow-up: none — both
  fn names already in `ghidra_symbols.txt`.** **Band note: the libultra libc band's printf/number-
  format vein is now banked (xprintf/xlitob/xldtob/ldiv/sprintf/string); remaining libultra is the
  heavy non-audio structural packs (llcvt/settime game-region phantoms) + the sched.c-head & exceptasm.s
  spikes (carry-overs).**

- **Sprint 105: 1 .c file BANKED — `src/libultra/io/dpsetnextbuf.c` (`osDpSetNextBuffer`), libultra
  DP next-buffer setter; the S104-split foreign TU picked up the very next sprint.** md5-candidate
  153→**154** (all .c stub-free); asm subsegs ~125→~124. **Split-then-mirror pipeline validated
  end-to-end:** S104's `upstream-fncount-mismatch` split `func_800B1580` off the xprintf subseg as a
  foreign asm TU; it was the smallest libultra candidate this sprint, coddog@99.99 → `src/io/
  dpsetnextbuf.c`, and the gate asm confirmed `osDpSetNextBuffer` (3 jals = `__osDpDeviceBusy`@0x800B2B10
  S1 + `osVirtualToPhysical`@0x800A7720 S7 ×2; DPC_STATUS/START/END_REG IO_WRITE/IO_READ; no `_DEBUG`
  block). NOTE: the imprecise S104 carried label "`__osDpDeviceBusy` TU" was a hint, not an attribution —
  the fn *calls* __osDpDeviceBusy, it isn't it (corrected at the gate; retro #2 codified this). Warm io
  band (dp.c S1, dpsetstat/dpctr S10, epi* S22/S23) pre-placed both callees + all 4 headers → ZERO
  recovery, ZERO carve, ZERO mid-flight surprises. Verbatim body cp of ultralib VERSION_J `src/io/
  dpsetnextbuf.c`; only the include block adapted to in-tree io-band convention (dropped `#ident`,
  `PRinternal/osint.h`→`osint.h`); `_DEBUG` asserts kept verbatim (compile out under `_FINALROM`). 1
  symbol add at gate (`osDpSetNextBuffer`=0x800B1580). **First-build full-make ROM SHA-1 == baserom, 0
  iteration.** regime mirror → seed-only, committed/banked **3pt**. 0 stuck-far/permuter/carried/re-opened.
  Applied **1 of 3** (PO): #2 `docs/hazards.md#upstream-mirror-pattern` split-off-TU-label-is-a-hint note
  (#1 confirmatory split-then-mirror-validated, log-only; #3 seed-pricing nuance on 0-work coddog-mirror +
  already-vendored header, NOT selected). **Cross-repo follow-up:** propagate `osDpSetNextBuffer`@0x800B1580
  to the Ghidra workspace via `sync_decomp_names.py --import-from-decomp`. **Bonus:** pre-places the
  `osCreateScheduler` (pts-13) `calls-unplaced` callee. No carry-overs.

- **Sprint 107: 1 asm-mirror BANKED — `src/libultra/os/exceptasm.s` (`__osExceptionPreamble` + 7
  dispatch fns, the OS exception/thread-dispatch core), vendored `.text`-only `hasm`. THE S91 jtbl
  spike SOLVED via the label-export mechanism.** asm subsegs ~123→**122**; md5-candidate unchanged at
  155 (an asm-mirror banks as `hasm`, not a `.c`); 8 fns matched. **The "both dead-ends proven, needs
  a novel mechanism" framing (parked 16 sprints) was an over-generalization** — S91 proved 2 paths fail
  (strip-and-rename a symbolic table; carve a `hasm` `.o`'s rodata) but LISTED a 3rd untried option that
  works first-try. The `__osIntTable` switch jtbl lives in its OWN already-address-placed rodata blob
  (`asm/data/AD9F0.rodata.s`) that survives the `.text` flip and keeps SYMBOLIC `.word .L800Bxxxx` refs
  into the `.text`. **Phase 1** (subseg still `asm`): `symbol_addrs` strip-rename the 5 D_/jtbl_ tables
  (`__osHwIntTable`=0x800C9480/0x28, `__osPiIntTable`=0x800C94A8/0x8, `__osIntOffTable`=0x800D25F0/0x20,
  `__osIntTable`=0x800D2610/0x30, `__osThreadSave`=0x800FC6A8/0x1B0=sizeof(OSThread) bss) → green with
  the subseg still `asm` (isolates rename risk; the `__osIntTable` rename PRESERVED the symbolic jtbl).
  **Phase 2:** vendor ultralib `os/exceptasm.s` `.text`-only (strip `.rdata`/`.data`; adapt the 2
  source-private includes → `internal/exceptasm.h`+`internal/threadasm.h`, vendoring `exceptasm.h`→
  `include/libultra/internal/`; `.globl` the stripped tables) + **export the 9 jtbl-target labels under
  the `.L800Bxxxx` names the blob references** (mapped by instruction: `counter`→`.L800AFDFC`,
  `redispatch`→`.L800B00A0`, …). VENDOR_ASM pair 8AF90 + flip `[0x8AF90,asm]`→`hasm`, `make clean &&
  make extract && make`. Vendored `.o` = `.text`-only `0x970` exact (objdump: empty data sections →
  0-byte auto-link lines); blob stayed symbolic → the 9 exports are LOAD-BEARING (this IS the
  mechanism). All 8 fns pre-curated in ghidra_symbols → no fn-add, no caller-evict. 0 extract-artifact
  changes (`.ld` already named `build/asm/8AF90.o`; renamed syms are blob-defined not undefined).
  **First-build full-make ROM SHA-1 == baserom, 0 iteration.** 0 stuck-far/permuter/carried/re-opened.
  regime mirror → seed-only, banked 13pt. Applied **3 of 3**: #1 `docs/hazards.md#asm-mirror-vendoring`
  asm-mirror-jtbl sub-case rewritten spike→proven LABEL-EXPORT procedure (Phase-1 rename-isolation +
  Phase-2 vendor-`.text`-only + re-export) + `pick_target.py` comment + CLAUDE.md hazard-index row; #2
  untried-mechanism-before-dead-end lesson (carry-over header above); #3 Phase-1 rename-isolation
  codified as step 1. **Cross-repo follow-up:** 5 new decomp data symbols (`__osHwIntTable`/
  `__osPiIntTable`/`__osIntOffTable`/`__osIntTable`/`__osThreadSave`) → propagate via
  `sync_decomp_names.py --import-from-decomp`. **Band: exceptasm done → the ONLY genuine remaining
  libultra SOURCE work is the `setintmask` partial-TU spike (`__osDisableInt`+`__osRestoreInt`
  @0x8B900, 2 of 3 fns share setintmask.s); everything else `--lib libultra` is game-region structural
  phantoms or libnusys/libkmc fillers.**

- **Sprint 106: 1 .c file BANKED — `src/libultra/sched/sched.c` (`osCreateScheduler` + 13 helpers),
  the libultra RCP task scheduler — THE last real libultra source mirror.** md5-candidate 154→**155**
  (all .c stub-free); asm subsegs ~124→~123; 14 fns matched. **The libultra cheap/source-mirror band
  is now fully mined out** — remaining libultra is non-source work: the `exceptasm.s` jtbl spike, the
  game-region structural phantoms (llcvt/settime/contquery, `coddog-structural`), and libnusys/libkmc
  fillers. pts-13 tripped the 8-gate but decompose was MECHANICALLY BLOCKED (single-file-pack — the
  `[0x86A50,asm]` subseg is exactly the .text 0xA10, 0x86A50..0x87460) → PO-approved enabler-forward
  full mirror (S100 reverb precedent), banked atomically after 1 enabler fix-iteration. **Characterized
  up front from `ultralib/build/J/libgultra_rom/.../sched.o`** (the `_rom`/`_FINALROM` profile =
  MG64's): .text 0xa10 / .data 0x10 / .rodata 0x20 / **.bss 0** — SC_LOGGING is OFF, so the scLog/
  logArray statics are gone and `calls-unplaced:osCreateLog,osFlushLog,osLogEvent` + `jal-count-
  mismatch:12vs11` were ALL false flags. **Dual carve:** `.data` `[0xA3600,.data,libultra/sched/sched]`
  0x10 (count/dp_busy/dpCount/firsttime @vram 0x800C8200, firsttime=1 nonzero; vram via __scExec
  `dp_busy=0x800C8204`, segment-wide vram−rom=0x80024C00) + `.rodata` `[0xAD9C0,.rodata,libultra/sched/
  sched]` 0x20 (the __scExec switch jtbl @0x800D25C0 — the whole generic `[0xAD9C0,rodata]` block, exact
  fit). **2 recover (S22/S24):** `osViModeTable`=0x800C8270 size:0x1180 (56×0x50 OSViMode, src/io/vitbl,
  shared with nuScCreateScheduler) + `osSpTaskYielded`=0x800AB600 — which IS the S11-banked
  `func_800AB600` ("classical main" leaf = the un-named scheduler yield-check all along); renamed +
  signature matched to sptask.h (`OSYieldResult`/`OSTask*`) keeping the verified `(status>>8)&1` body
  (NOT the upstream ternary — -O2 codegen risk; the file stays src/main/, no -O3 relocation). **2
  caller-evict:** still-asm mus_dma (`asm/78D10.s`) called the sched globals by old func_ names → named
  `osScAddClient`=0x800AB798 + `osScGetCmdQ`=0x800AB880 (osScRemoveClient/__scTaskReady had no external
  caller → no add). **assert-strip: 9 bare asserts** `#ifdef _DEBUG`-wrapped — pick's `bare-assert:9`
  was authoritative, but a manual `^\s*assert\(` grep found only 7 (missed 2 `assert (` SPACE-variants
  `assert (t->msgQ)`/`assert ( (type==…))` → first-build .text 0xa90/.rodata 0x70 bloat → 1 fix-iteration
  → exact 0xa10/0x20); 1 of the 9 was an `if`-body needing the whole-`if` wrapped. Include adapt
  PRinternal/osint.h→osint.h, drop #ident. **First-build (after the 1 assert fix) full-make ROM SHA-1
  == baserom.** 0 stuck-far/permuter/carried/re-opened; 1 fix-iteration. regime mirror → **seed-only,
  banked 13pt**. Applied **4 of 4** (PO): #1+#2 `docs/hazards.md#assert-strip` steps 4-5 (assert-as-`if`-
  body whole-`if` wrap + `assert\s*\(` space-variant count cross-check vs pick's `bare-assert:N` — no
  tool change, the detector already uses `\bassert\s*\(`); #3 `#caller-evict` Companion B (recover-callee
  IS an already-banked func_ → rename+match-sig+keep-verified-body); #4 `#caller-evict` Companion A
  (multi-global flip evicts still-asm callers); #5 logged as the BACKLOG tooling follow-up below. No
  carry-overs. **Cross-repo follow-up:** 4 new decomp symbols (`osSpTaskYielded`@0x800AB600,
  `osScAddClient`@0x800AB798, `osScGetCmdQ`@0x800AB880, `osViModeTable`@0x800C8270) → propagate via
  `sync_decomp_names.py --import-from-decomp`. Optional cosmetic: rename `src/main/func_800AB600.c` to
  an osSpTaskYielded-named file (keep -O2/src/main).

- **Sprint 103: 1 .c file BANKED — `src/mgu/mtxutil.c` (`guMtxF2L` + `guMtxL2F` + `guMtxIdentF` +
  `guMtxIdent`), gu matrix utils; a planned verbatim libultra mirror that PIVOTED to classical at the
  game `-O2` profile.** md5-candidate 151→**152** (all .c stub-free); asm subsegs 125→125 (the split
  carved a c subseg out of the `[0x414A0,asm]` game pack; the 8-fn game-code prefix stays asm, never
  in scope). The 8-gate fired on the pts-13 `func_800660A0` pack → decompose at the mtxutil TU
  boundary `guMtxF2L`@0x80067B00 (rom 0x42F00, 16-aligned), splitting `[0x414A0,asm]` →
  `[0x414A0,asm]` + `[0x42F00,c,mgu/mtxutil]` + `[0x43140,asm]`. **The verbatim-mirror premise FAILED
  first build (two compounding errors):** (1) the 4 gu fns are GAME-region (0x80067B00, inside the
  game pack), compiled `-O2`, NOT the libultra `-O3` band — `src/libultra/gu` placement forced `-O3`
  → `-finline-functions` inlined `guMtxIdent` (240B vs ROM 60B); (2) `guMtxF2L` CLAMPS in place
  (`if(x<-32768.0f)…; if(x>32766.0f)…`, consts `0xc7000000`/`0x46fffc00`) — a Monegi overflow-guard
  variant absent from ultralib `gu/mtxutil.c`, ultralib `mgu/mtxf2l.s`, AND `libultra_modern
  monegi/mgu/mtxf2l.s` (all 3 non-clamping, byte-identical). PO chose push-through-classically →
  re-placed at `src/mgu/mtxutil.c` (`-O2`, include via public `<ultra64.h>`), 3 fns byte-verbatim +
  `guMtxF2L` = upstream body + an explicit clamp; **new verbatim-upstream dir `src/mgu/`** with a
  `.clang-format` (`DisableFormat: true`). 1 fix-iteration: float-literal `f`-suffix (bare doubles
  compiled `c.lt.d`+`cvt.d.s`+a rodata pair; ROM uses single `c.lt.s` inline). 3-leaf byte-cmp
  IDENTICAL + full-make ROM SHA-1 == baserom. 0 symbol adds (4 names pre-curated in `ghidra_symbols`).
  seed 3 (regime mirror — the coddog-mirror tag over-promised "verbatim cp") → **realized 5, residual
  +2, regime mixed** (v2: +1 mid-sprint re-plan, +1 novel profile+clamp gotcha). 0
  stuck-far/permuter/carried/re-opened; **1 mid-sprint re-plan + 1 fix-iteration**. Applied **4 of 4**:
  #1 `pick_target.py coddog-partial:<m>of<n>fn` (≥2-distinct-twin subset guard — the multi-twin
  companion to `coddog-fncount-mismatch`, the under-weighted `coddog-twin:mtxidentf!=mtxutil` signal)
  + `test_coddog_partial_twin_subset`; #2 `pick_target.py game-region-mirror:0x<vram>` (a libultra
  source below the libultra-band rom is `-O2`, route to `src/mgu/`) + `test_game_region_mirror_below_libultra_band`
  + `docs/hazards.md#game-region-mirror-o2-profile` + CLAUDE.md index row; #3 float-literal
  single-vs-double note (`docs/hazards.md#mirror-cast-divergence`); #4 codify `src/mgu/` no-clang-format
  (CLAUDE.md ×3 + `.clang-format`). suite 55→**57** pass (no golden regen — post-bank the live
  `func_800660A0` row lost its gu identity, so no golden delta). **Lesson: a `coddog-mirror` on a
  game-region multi-fn pack is NOT a clean verbatim cp signal — coddog matched only 2 of 4 fns; the
  two new guards target exactly this.** No carry-over.

- **Sprint 102: 1 .c file BANKED — `src/libultra/io/motor.c` (`__osMotorAccess` + `osMotorInit`),
  libultra io VERSION_J verbatim mirror; corrected a wrong ghidra name WITHOUT `make sync-names`.**
  md5-candidate 150→**151** (all .c stub-free); asm subsegs 126→125; 2 fns matched. The io/motor.c
  trap S75 flagged — the smallest remaining libultra target (pts-8; everything else under
  `--lib libultra` is pts-13 and structurally trapped: llcvt/settime `coddog-structural`, `_Printf`
  rodata-jtbl). The active `#if BUILD_VERSION >= VERSION_J` branch compiles `__osMotorAccess`@0x800AE380
  + `osMotorInit`@0x800AE4C4 (`__osMakeMotorData` inlined → `pack:2fn`/`one-tu`) + the file-static
  `__MotorDataBuf[4]` — all verified against `build/J/libgultra_rom/motor.o` (`T __osMotorAccess`, no
  `osMotorStop`; `b __MotorDataBuf`). pts-8 tripped the 8-gate but decompose was MECHANICALLY BLOCKED
  (one-tu single-file-pack, no inter-file boundary) → ran 1-increment enabler-forward (S100/S101
  precedent). **Headline — `wrong-ghidra-name` override (NO sync-names):** ghidra_symbols mislabels
  0x800AE380 `osMotorStop`, but os_motor.h `#define osMotorStop(x) __osMotorAccess(...)` makes that a
  macro; the VERSION_J fn IS `__osMotorAccess`. Corrected via a `symbol_addrs.txt` override
  `__osMotorAccess = 0x800AE380; // rom:0x89780 type:func` — the `rom:` qualifier dodges splat's
  same-rom+segment dup error (`util/symbols.py:298-309`), symbol_addrs is read first so it wins the
  reference (still-asm contRmbControl's 4 relocs → `jal __osMotorAccess`, gate-verified). NO `#undef`
  needed (body names the macro RHS). **drop-static:** `__MotorDataBuf`=0x800FBC30 size:0x100 (lui
  0x8010/addiu -0x43d0; vi/io bss after viCounterMsg) `static`→`extern`. Include adapt
  `PRinternal/{controller,siint}.h`→bare. pick false-flags resolved by VERSION_J analysis
  (`defines-data:__osMotorinitialized` + half `drop-static:2bss` = inactive `#else`;
  `calls-unplaced:READFORMAT` = function-like macro) — all fixed in the tool this retro. **First-build
  full-make ROM SHA-1 == baserom, 0 iteration.** 0 stuck-far/permuter/carried/re-opened. Applied 4 of 4:
  #1 new `docs/hazards.md#wrong-ghidra-name-override` + CLAUDE.md index + `pick_target.py`
  `wrong-ghidra-name` tag + unit test; #2 version-strip wired into the file-static/defines-data
  detectors + same-file function-like macro exclusion in `calls_unplaced`; #3 `header_renames_symbol`
  macro-alias false-fire suppression; #4 `nm build/J/libgultra_rom/*.o` authoritative-symbol-set note.
  suite 55 pass, golden regen. **Cross-repo follow-up:** rename 0x800AE380 `osMotorStop`→`__osMotorAccess`
  in the Ghidra workspace (deferred reconciliation; the override coexists meanwhile). **Band note: io is
  now down to the `piacs` defines-data+file-static trap as the last io leaf; remaining libultra is the
  heavy non-audio structural packs (llcvt/settime/contquery-region phantoms), the xprintf classical
  band, and the sched.c-head + exceptasm.s spikes (carry-overs).**

- **Sprint 101: 2 .c files BANKED — `src/libultra/audio/env.c` (`alEnvmixerPull` + `alEnvmixerParam`
  + `_pullSubFrame` + `_frexpf` + `_ldexpf` + `_getRate` + `_getVol`) + `src/libultra/audio/filter.c`
  (`alFilterNew`); cleared the `[0x804D0]` `c-combined:2file[env|filter]` pack — the LAST un-flipped
  audio-synth-cluster subseg.** md5-candidate 148→**150**; asm subsegs 127→126; 8 fns matched. The
  audio-synth mirror vein is now fully banked. pts-13 tripped the 8-gate → MANDATORY decompose
  (`c-combined` blocks the verbatim exemption) → split `[0x804D0,asm]`→`[0x804D0,c,libultra/audio/env]`
  + `[0x81180,c,libultra/audio/filter]` at the env/filter file boundary (`alFilterNew`@0x800A5D80,
  the 0x20 B tail). **filter.c (1fn):** trivial verbatim cp (6 struct-field writes, no data/rodata/
  calls), `alFilterNew` pre-curated. **env.c (7fn, the envmixer):** the drvrnew (S96) / reverb (S100)
  carve-mirror class — verbatim ultralib VERSION_J, failed the exemption on residual variance (dual
  carve + assert-strip), ran enabler-forward. **Dual whole-subseg carve, NO split (both sections,
  S93-class):** `.data` `eqpower[128]`@`[0xA3460,.data,libultra/audio/env]` (0x100 B, vram 0x800C8060,
  the file's ONLY `.data`; the S92 UN-flagged initialized-static-array class, asm-recovered from the
  `s4 = lui 0x800d/addiu -0x7fa0` load) + `.rodata` jtbl_800D22F0 + 13 FP literals@`[0xAD6F0,.rodata,
  libultra/audio/env]` (0xF0 B) — BOTH landed exactly on an existing generic subseg boundary whose end
  bounded the env.o section → 1-line attribute flips (pick's `carve-end=0x800D25C0` over-stated; real
  end 0x800D23E0 = the next named `.rodata` boundary 0xAD7E0). **assert-strip:** 3 asserts (105/106/370,
  the `#if BUILD_VERSION<J` block around 102-104 is only a `#line` directive, asserts are ACTIVE)
  wrapped `#ifdef _DEBUG` (NDEBUG+_DEBUG both unset → bare `assert()`→`__assert` SHA-break; save.c
  style). **2 calls-unplaced recovered:** `__freeParam`=0x80051E74, `_freePVoice`=0x80051E7C
  (alEnvmixerPull lines 292/267 jal targets, still-asm synth region); `_frexpf`/`_ldexpf` calls-unplaced
  were intra-file FALSE-positives (env.c members). 4 member symbol adds at gate (`_frexpf`/`_ldexpf`/
  `_getRate`/`_getVol`). Both **first-build full-make ROM SHA-1 == baserom, 0 iteration**. 0 stuck-far/
  permuter/carried/re-opened. Applied 3 of 3: #1 `docs/hazards.md#rodata-sibling-yaml-pattern`
  generic-subseg-bound carve = exact-extent-no-split heuristic (advances the deferred S98 carve-end
  work); #2 BACKLOG S92 `.data`-carve detector 2nd data point (env single-file-pack = the safe first
  slice, no per-member `up_path` ambiguity); #3 BACKLOG new S101 #1 tooling follow-up — suppress
  intra-pack `calls-unplaced` (calls-side dual of S66 #2). **Cross-repo follow-up:** 6 new decomp-side
  symbols (`_frexpf`/`_ldexpf`/`_getRate`/`_getVol`/`__freeParam`/`_freePVoice`) → propagate via
  `sync_decomp_names.py --import-from-decomp`. **Band note: the audio-synth cluster (auxbus/load/
  drvrnew/save/sl/mainbus/resample/reverb/env/filter) is now COMPLETELY mirrored. Remaining libultra
  is the heavy non-audio packs — sched.c head + exceptasm.s spikes (carry-overs), the xprintf classical
  band, and the structural-phantom packs (llcvt/settime/contquery-region).**

- **Sprint 100: 1 .c file BANKED — `src/libultra/audio/reverb.c` (`alFxPull` + `alFxParam` +
  `alFxParamHdl` + `_loadOutputBuffer` + `_loadBuffer` + `_saveBuffer` + `_filterBuffer` +
  `_doModFunc`), the libultra audio REVERB effect; cleared the `[0x815C0]` pack.** md5-candidate
  147→**148**; asm subsegs 128→127; 8 fns matched. The 4th audio-synth-cluster mirror and a
  **drvrnew-class replay (S96)**: a pts-13 single-file pack that tripped the 8-gate and FAILED the
  S64/S69 verbatim exemption (residual variance: dual carve + assert-strip) → ran enabler-forward
  (PO chose the full mirror; banked atomically first-try). Verbatim ultralib VERSION_J
  `src/audio/reverb.c`, **all 8 includes kept verbatim** (`ultraerror.h`+`os_internal.h` ARE vendored
  at `include/libultra/PR/` — drvrnew's drop was unnecessary) + **1 assert-strip** (`assert(source)`
  → `#ifdef _DEBUG`, S97 convention; reconciles `jal-count-mismatch:8vs7`). **Dual carve:** `.rodata`
  ATTRIBUTE-CHANGE `[0xAD810,.rodata,libultra/audio/reverb]` (jtbl_800D2410 8-entry + 6 FP doubles =
  0x50; the generic `[0xAD810,rodata]` already bounded the exact extent → NO split, S93-class; pick's
  `carve-end=0x800D25C0` was a 0x160 over-estimate) + `.data` 3-WAY SPLIT `[0xA3560,.data,
  libultra/audio/reverb]` (0x20: `L_INC[]`={0x10,0x10,0x20}, `val`=0.0, `lastval`=-10.0=`0xC1200000`,
  `blob`=0, pad). **Novel: the `.data` was UNREFERENCED dead statics** (no `.text` reloc → not
  asm-recoverable) so the carve offset was found by ROM byte-search (objdump `.data` → `xxd baserom |
  grep`), at 0xA3560 (0x100 B past drvrnew's 0xA3460 tail — link order interleaves other files'
  `.data`). `calls-unplaced:SWAP` FALSE (inline macro). 5 helper symbol adds at gate
  (`_loadOutputBuffer`=0x800A6738/`_loadBuffer`=0x800A6950/`_saveBuffer`=0x800A6AC0/`_filterBuffer`=
  0x800A6C30/`_doModFunc`=0x800A6CCC; alFxPull/Param/ParamHdl placed S96). First-build full-make ROM
  SHA-1 == baserom, 0 iteration. seed 13 / banked 13pt; regime mirror (seed-only). 0
  stuck-far/permuter/carried/re-opened. Applied 1 of 3: #1 `docs/hazards.md#defines-data`
  unreferenced-static-carve byte-search sub-case (+ deferred `pick_target` `;unref` tag, off-cadence
  ranker follow-up). **Cross-repo follow-up:** 5 new decomp-side fn symbols (`_loadOutputBuffer`/
  `_loadBuffer`/`_saveBuffer`/`_filterBuffer`/`_doModFunc`) → propagate via
  `sync_decomp_names.py --import-from-decomp`. **The audio-synth cluster's last asm leaf is env @0x804D0
  (`c-combined:2file[env|filter]`, MUST decompose; verify `_frexpf`/`_ldexpf` placed first); then the
  heavy carry-over spikes (sched head @0x86A50, exceptasm @0x8AF90) remain.**

- **Sprint 99: 3 .c files BANKED — `src/libnusys/mainlib/nugfxdisplayon.c` (`nuGfxDisplayOn`) +
  `nupiinit.c` (`nuPiInit`) + `nupiinitsram.c` (`nuPiInitSram`), libnusys mirrors; cleared the
  `[0x7CAD0]` c-combined:3file pack.** md5-candidate 144→**147**; 3 fns matched. A
  `c-combined:3file[nugfxdisplayon|nupiinit|nupiinitsram]` pack split at the 3 file boundaries
  (`nuPiInit`@0x7CAE0, `nuPiInitSram`@0x7CB20, all 16-aligned) into 3 single-file mirrors.
  **nugfxdisplayon** (16B) = trivial verbatim cp (1 store to `nuGfxDisplay`, placed S16), zero
  enabler. **nupiinit** (64B) + **nupiinitsram** (176B) = S87 drop-static mirrors — file-statics
  `PiMesgQ`/`PiMesgBuf`/`SramHandle` + globals `nuPiCartHandle`/`nuPiSramHandle` dropped to extern;
  the 3 statics asm-recovered (PiMesgQ=0x800F74A0/0x18, PiMesgBuf=0x800F74B8/0xC8,
  SramHandle=0x800F7580/0x74) + added to symbol_addrs (**no carve — `.bss` is NOBITS**; the ROM match
  rides only the `.text` relocs). All 4 callees placed (osCreatePiManager/osCartRomInit/bzero/
  osEPiLinkHandle). All 3 first-build full-make ROM SHA-1 == baserom, 0 iteration. seed 3 (a
  primary-only lower bound — pick_target's whole-pack scan missed members 2&3's drop-static load,
  anchor-true ~6; retro #1 fixed the class); regime mirror (seed-only). 0
  stuck-far/permuter/carried/re-opened. Applied 3 of 3: #1 `pick_target.py` comment-strip fix
  (`has_file_scope_static`+`defines_data_globals` scan comment-STRIPPED text — a trailing `/*..*/`
  after `;` and a `Copyright (C)` banner's `(` had suppressed file-static/defines-data across the
  whole nusys band) + `file-static` member-union over c-combined members; golden regen, suite 54
  pass; #2 `docs/hazards.md#file-static` batch-add transient-red note (DATA-symbol caller-evict) +
  detector-sync note; #3 `.bss`-NOBITS-no-carve confirmation (log-only). **Cross-repo follow-up:** 3
  new decomp-side data symbols (PiMesgQ/PiMesgBuf/SramHandle) → propagate via
  `sync_decomp_names.py --import-from-decomp` when convenient. **Remaining libnusys mainlib asm:** the
  nuCont/nuSi manager packs (nuContRmbModeSet, nuContMgrInit, gfxThread, etc.) — now correctly
  flagged with their members' file-static/defines-data load post-fix.

- **Sprint 98: 2 .c files BANKED — `src/libultra/audio/mainbus.c` (`alMainBusPull` +
  `alMainBusParam`) + `src/libultra/audio/resample.c` (`alResamplePull` + `alResampleParam`),
  libultra audio-synth mirrors; cleared the `[0x811A0]` c-combined pack.** md5-candidate 142→**144**;
  asm subsegs 130→129; 4 fns matched. The cheapest remaining audio-synth-cluster unit (S97 warm next
  band), a `c-combined:2file[mainbus|resample]` pack that tripped the 8-gate (NOT the verbatim
  exemption — c-combined MUST decompose) → split `[0x811A0,asm]` text at the mainbus/resample file
  boundary (0x81310, `alResamplePull`@0x800A5F10, 16-aligned) into two single-file mirrors. **mainbus.c**
  verbatim VERSION_J cp, **carve-FREE** (alMainBusPull all-immediate, alMainBusParam 1-case
  switch→branch — asm-confirmed no 0x800D2 refs). **resample.c** verbatim cp + a **`.rodata` carve**
  `[0xAD7E0,.rodata,libultra/audio/resample]` (MAX_RATIO double `D_800D23E0` 8B + `jtbl_800D23E8` 10w
  40B = 0x30, alResamplePull `ldc1 0x23e0` + alResampleParam 5-case switch); pre-carve build link-failed
  (`AD6F0.rodata.o` jtbl `.word .L800A6160/.L800A6188` vanish under C) → carved generic `[0xAD6F0]`
  3-way (twin-of:drvrnew S96 pinned the playbook). All 4 names pre-curated S96 (drvrnew callees) →
  **zero symbol adds**; osVirtualToPhysical placed S7; AUD_PROFILE dead S95. Both first-build full-make
  ROM SHA-1 == baserom, 0 iteration. seed 4 (2 mainbus + 2 resample); regime mirror (seed-only). 0
  stuck-far/permuter/carried/re-opened. Applied 2 of 2 (conservative form): #1 `pick_target.py`
  `;owner-per-member` marker on a c-combined pack's rodata-jtbl/literal (the whole-pack scan
  over-attributed resample's carve to the mainbus primary row; fires only when `member_paths` non-empty
  → no single-file regression; now flags the env/filter pack 0x804D0) + CLAUDE.md index row +
  `docs/hazards.md#rodata-sibling-yaml-pattern` owner-per-member note; #2 carve-end upper-bound +
  stale-`asm/<ROM>.s` caveats in the same hazards section; golden regen, suite 54 pass. **No cross-repo
  follow-up** (zero new decomp-side symbols). **The audio-synth cluster's remaining asm — env @804D0
  (`c-combined:2file[env|filter]`, now `;owner-per-member`-flagged) + fx/reverb @815C0 — is the warm
  next band.**

- **Sprint 97: 2 .c files BANKED — `src/libultra/audio/save.c` (`alSavePull` + `alSaveParam`) +
  `src/libultra/audio/sl.c` (`alInit` + `alClose` + `alLink` + `alUnlink`), libultra audio-synth
  mirrors; cleared the `[0x82160]` `alSavePull` pack.** md5-candidate 140→142; 6 fns matched. The
  smallest audio-synth-cluster unit, a `c-combined:2file[save|sl]` pack that tripped the 8-gate (NOT
  the verbatim exemption — c-combined MUST decompose) → split `[0x82160,asm]` at the save.c/sl.c file
  boundary (0x82230, alInit) into two single-file mirrors. **save.c** verbatim VERSION_J cp + the
  **assert-strip wrap** — `assert(f->filter.source)` is the first audio mirror to use assert();
  `<assert.h>`→`include/assert.h` (NDEBUG-keyed, undefined → live `__assert`) SHA-missed a verbatim cp
  → wrapped in `#ifdef _DEBUG` (sirawread convention); names pre-curated S96, no carve. **sl.c**
  verbatim cp + **shared-global defines-data DROP (S42 fast path, mandatory)** — `alGlobals`
  (D_800C8180, 16B = 4B ptr + 12B `.data` pad) is SHARED (env @804D0 + fx @815C0 still-asm reference
  it) so a carve would orphan those refs / double-define → dropped to the `libaudio.h:388` extern +
  `alGlobals=0x800C8180 // size:0x4` (storage stays splat-side). 1 gate callee-name
  `alSynDelete=0x80051E54` (alClose's, only asm callers → no caller-evict). Both first-build full-make
  SHA-1 == baserom, 0 iteration. seed 5 (2 save + 3 sl), regime mirror (seed-only). 0
  stuck-far/permuter/carried/re-opened. Applied 3 of 3: #1 `pick_target.py` `bare-assert:<n>` advisory
  + CLAUDE.md index row; #2 `docs/hazards.md#defines-data` shared-global DROP-mandatory rule; #3
  `pick_target.py` `defines-data` c-combined member-union (`_c_combined_member_paths`); golden regen,
  tooling suite green. **Cross-repo follow-up:** `alSynDelete`=0x80051E54 + `alGlobals`=0x800C8180 are
  new decomp-side symbols → propagate via `sync_decomp_names.py --import-from-decomp`. **The audio-synth
  cluster's remaining asm (env @804D0, mainbus+resample @811A0, fx/reverb @815C0) is the warm next band
  — pick_target now pre-flags its bare-assert (env 3, reverb 1) + member-union defines-data.**

- **Sprint 96: 1 .c file BANKED — `src/libultra/audio/drvrnew.c` (`_init_lpfilter` + `alFxNew` +
  6×`al*New`), the libultra audio synthesis DRIVER; the 3rd audio sub-band mirror and the FIRST that
  did NOT run under the verbatim-mirror exemption.** md5-candidate 139→**140**. pts-13 tripped the
  8-gate and the S64/S69 exemption FAILED condition (c) "no residual variance" (unlike S94/S95):
  drvrnew references **10 cross-file pull/param entry points by name** + needs a **dual carve**, so
  it ran enabler-forward (PO chose the full mirror over an enabler-only sprint; banked atomically
  first-try). Enablers: vendored `initfx.h`(→`include/libultra/internal/`) + `stdio.h`(→
  `include/libultra/compiler/gcc/`, same source as memory/stdlib/string.h); 18 symbol adds (8 drvrnew
  fns + 10 callees `alFilterNew`=0x800A5D80/alMainBusPull/Param/alResamplePull/Param/alFxPull/Param/
  ParamHdl/alSavePull/Param, all asm-confirmed from al*New a1/a2+jal, no conflict/caller-evict);
  **`.data` carve** `[0xA32D0,.data,libultra/audio/drvrnew]` (6 `static s32 *_PARAMS[]`=0x190, 3-way
  split of main_data) + **`.rodata` carve** `[0xAD6B0,.rodata,…]` (jtbl_800D22B8 switch + SCALE/
  CONVERT/2³² consts=0x40, attribute-change). `refs-unplaced:__FILE__,__LINE__` was FALSE (`_DEBUG`
  off → `alHeapAlloc→alHeapDBAlloc(0,0,…)`, no string). Verbatim ultralib VERSION_J, byte-identical
  cp, **first-build full-make ROM SHA-1 == baserom, 0 iteration**. 0 stuck-far/permuter/carried/
  re-opened. **Strategic: pre-named the reverb/mainbus/save/resample/fx entry points + vendored the
  shared headers → the audio synth cluster (reverb/env/mainbus/save/resample) is the warm cheaper
  next band.** Retro applied 3 of 4 (+1 log-only): #1 `pick_target.py` `coddog-source-banked:<file>`
  tag (a coddog match to an already-banked mirror is structural — func_8009F440→load.c@98.17 FP);
  #2 `refs_unplaced` skips compiler predefined macros (`__FILE__`/`__LINE__`/…); #3 `docs/hazards.md`
  dual-section-carve cross-ref (#defines-data ↔ #rodata-sibling); #4 cluster-unlock log-only. No
  carry-overs.
- **Sprint 95: 1 .c file BANKED — `src/libultra/audio/load.c` (`alAdpcmPull` + `alRaw16Pull` +
  `alLoadParam` + `_decodeChunk`), libultra audio verbatim mirror; the 2nd audio sub-band leaf.**
  md5-candidate 138→**139** (all 139 .c stub-free); asm subsegs 156→155 (0x7F8B0 flipped). The
  cleanest tractable audio pack once auxbus (S94) opened the band: the `[0x7F8B0]` subseg
  (0xB10=2832B, ending exactly at auxbus 0x803C0) coddog-maps ALL 4 fns to ultralib
  `src/audio/load.c`@99.99 in source order (alAdpcmPull/alRaw16Pull/alLoadParam/_decodeChunk) → a
  true single-file pack, vs mainbus (2vs4) / save (2vs6 coddog-twin) which are genuinely multi-file
  (structural FPs needing a boundary split). **All 3 flagged hazards were FALSE:**
  `refs-unplaced:lastCnt` (the `extern u32 ...lastCnt[]` decl + the `lastCnt[++cnt_index]=osGetCount()`
  use + the 2 `PROFILE_AUD()` timing calls are ALL `#ifdef AUD_PROFILE`-guarded; MG64 doesn't define
  it → compile out → zero refs); `calls-unplaced:alLoadParam@0x800A4E3C` (a self-member, the coddog
  tail-carry artifact). Only active external callee `alCopy` placed S36; abi.h microcode macros
  (a*/aLoad*/aSetBuffer) + libaudio.h/synthInternals.h/os.h/R4300.h all vendored/in-tree; no rodata
  literals/jtbl (the alLoadParam `switch(paramID)` compiled to a branch chain → no carve). Single
  text flip `[0x7F8B0,asm]`→`[0x7F8B0,c,libultra/audio/load]` (no split, no carve). Verbatim ultralib
  VERSION_J `src/audio/load.c`, byte-identical cp, **first-build full-make ROM SHA-1 == baserom, 0
  iteration, 0 recover-extern**. pts-13 tripped the 8-gate but ran under the **verbatim-mirror
  exemption (S64/S69)** (regime mirror + verbatim single upstream file + decompose-blocked
  single-file-pack + all callees placed + 4 names curated at gate). 4 symbol adds
  (alAdpcmPull=0x800A44B0, alRaw16Pull=0x800A48F4, alLoadParam=0x800A4C90, _decodeChunk=0x800A4E3C).
  seed 13 / banked 13pt; regime mirror (seed-only). 0 stuck-far/permuter/carried/re-opened. Retro
  applied **3 of 3**: #1 AUD_PROFILE de-noise (`cpreprocess.py` `_strip_dead_blocks` set += AUD_PROFILE;
  `pick_target.py` `refs_unplaced` now strips dead blocks symmetric w/ calls_unplaced + `macro_hidden_text`
  strips dead blocks before finding invocations so a macro invoked ONLY under a dead block —
  PROFILE_AUD — isn't phantom-expanded; drops the phantom lastCnt/save_min/rate_min/vol_min from
  reverb/env/load rows; suite 54 pass, golden-inert); #2 the audio-sub-band ordering refresh below; #3
  coddog-attribution log-only note (the member map cleanly separated single-file from multi-file).
  **Cross-repo follow-up:** 4 new decomp-side symbols → `sync_decomp_names.py --import-from-decomp`.
  **Band note: the audio sub-band now has 2 leaves banked (auxbus, load); the remaining audio packs
  are real work — see the refreshed S94 ordering note.**

- **Sprint 94: 1 .c file BANKED — `src/libultra/audio/auxbus.c` (`alAuxBusPull` + `alAuxBusParam`),
  libultra audio verbatim mirror; the audio sub-band's first mirror.** md5-candidate 137→**138**
  (all 138 .c stub-free); asm subsegs 157→156 (0x803C0 flipped). The smallest-clean remaining
  libultra leaf (272 B / 2 fn), found by surveying the **coddog column** after `--lib libultra`
  showed only pts-8/13 spikes (the S55 caveat): `func_800A4FC0` @ 0x803C0 = coddog
  `src/audio/auxbus.c`@100.00, `one-tu`, ZERO other hazards. The audio header `-I` enabler was
  **already paid** (heapinit/heapalloc/copy flipped) → near-zero-enabler cp. Single text flip
  `[0x803C0,asm]`→`[0x803C0,c,libultra/audio/auxbus]` (no split, no carve). Verbatim ultralib
  VERSION_J `src/audio/auxbus.c`, byte-identical cp, **first-build full-make ROM SHA-1 == baserom,
  0 iteration**. Verified clean before the cp: no file-scope data/statics, callees are ABI macros
  (`aClearBuffer`) + an indirect `sources[i]->handler` (no jal to place), `switch` one real case
  (no jtbl), types/macros (`ALAuxBus`/`AL_AUX_*`/`AL_FILTER_ADD_SOURCE`) all resolve in-tree.
  pts-13 tripped the 8-gate but ran under the **verbatim-mirror exemption (S64/S69)** (regime mirror
  + verbatim single upstream file + decompose-blocked `one-tu` + no jal callees + both names curated
  at gate). 2 symbol adds (`alAuxBusPull`=0x800A4FC0, `alAuxBusParam`=0x800A509C). seed 13 / banked
  13pt; regime mirror (seed-only). 0 stuck-far/permuter/carried/re-opened. Retro applied **3 of 3**:
  #1 `pick_target.py` `--lib <scope>` now surfaces coddog-mirror rows whose matched source is
  in-scope (so un-named audio mirrors appear under `--lib libultra`/`--lib audio`) + a crash-guard +
  1 unit test (suite 54 pass) + golden regen (absorbs the auxbus-bank tail-shift, not the filter
  edit); #2 the audio-sub-band ordering note below; #3 a pts-mirror-over-estimate decision
  (`VELOCITY.md` seed-rubric — keep pts as-is, exemption absorbs the false 8-fire; audio re-pricing
  deferred). **Cross-repo follow-up:** 2 new decomp-side symbols → `sync_decomp_names.py
  --import-from-decomp`. **Band note: the audio sub-band is now partially open (see the S94 ordering
  note); auxbus was the only zero-hazard audio leaf — the rest carry real hazards.**

- **Sprint 93: 1 .c file BANKED — `src/libultra/libc/xldtob.c` (`_Ldtob` + `_Ldunscale` + `_Genld`),
  libultra libc float-to-string verbatim mirror; the S92 xldtob-tail carry-over, banked first-try.**
  md5-candidate 136→**137** (all 137 .c stub-free); asm subsegs −1 (0x8D480 flipped) + 1 generic rodata
  [0xADBD0] carved. The clean twin of S92's xlitob (`_Litob`) — the float/long-double→string scaling
  routines. Single text flip `[0x8D480,asm]`→`[0x8D480,c,libultra/libc/xldtob]` (S92 already split the
  xlitob head off `[0x8D230]`, so no further split). pts-13 tripped the 8-gate but ran under the
  **verbatim-mirror exemption (S64/S69)**: regime mirror + verbatim single upstream file +
  decompose-blocked `single-file-pack:3fn`/`one-tu` + all callees placed + all 3 names pre-curated in
  ghidra_symbols. Verbatim ultralib VERSION_J `src/libc/xldtob.c`, byte-identical cp, **first-build
  full-make ROM SHA-1 == baserom, 0 iteration**. `jal-count-mismatch:4vs3` was a FALSE flag (SHA proves
  the verbatim body). **`.rodata`-ONLY data story** (`pows[]` is `const` → `.rodata`, NOT the mutable
  `.data` of xlitob's ldigs/udigs): the generic `[0xADBD0,rodata]` subseg ALREADY bounded the exact 0x70
  extent (vram 0x800D27D0→0x800D2840 = `static const ldouble pows[]` 0x48 + "NaN"/"Inf" strings +
  1.0/1e8/"0" literals) → **1-line attribute change** `rodata`→`.rodata,libultra/libc/xldtob`, NO split.
  Callees memcpy/ldiv/lldiv/__udivdi3/__umoddi3 all pre-placed; xstdio.h/stdlib.h/string.h resolve (S92)
  → **zero symbol adds**. The S92 near-free-retry carry-over checklist (flip line / placed-ref inventory
  / rodata recovery / includes / upstream pin) replayed verbatim-correct → 0 rework (3rd S74→S75-style
  proof). 0 stuck-far/permuter/carried/re-opened. seed 13 / banked 13pt; regime mirror (seed-only).
  Retro applied 3 of 3: #1 `pick_target.py` rodata-literal **carve-start widening**
  (`defines_file_static_const_array` source-gates a rodata-subseg-start carve-start — the FP scan missed
  the pows[] base 0x800D27D0 by 0x50 B; FP-safe via the `static const` file-private gate, unlike the
  S92-reverted .data addiu scan; +1 unit test, suite 53 pass, golden-inert); #2
  `docs/hazards.md#rodata-sibling-yaml-pattern` carve-start-widening + carve-as-attribute-change notes +
  S93 provenance; #3 near-free-retry checklist confirmation (no edit). **Cross-repo follow-up: NONE** (all
  3 names pre-curated; zero new decomp-side symbols). **Band note: the libc xstdio family is now
  xprintf-only — `_Printf`/`_Putfld` (the `[0x8BF30]` xprintf leaves S91 split off bcmp) remain as a
  pts-13 single-file-pack with `rodata-jtbl` + soft-float; the remaining clean libultra mirrors are mined
  out (what's left = the motor.c version trap, the sched/exceptasm heavy spikes, and the coddog-structural
  multi-file packs).**

- **Sprint 92: 1 .c file BANKED — `src/libultra/libc/xlitob.c` (`_Litob`), libultra libc c-combined
  decompose + `.data` carve.** md5-candidate 135→**136** (all .c stub-free); asm subsegs 135→135
  (xldtob tail stays asm). `_Litob` is the printf integer-to-string radix formatter (oct/dec/hex,
  signed/unsigned 64-bit). The pts-13 `[0x8D230]` `c-combined:2file[xldtob|xlitob]` pack tripped the
  8-gate → decomposed at the upstream-file boundary (xlitob HEAD / xldtob TAIL), since a 2-file
  c-combined blocks the verbatim-mirror exemption. Split `[0x8D230,asm]`→`[0x8D230,c,libultra/libc/
  xlitob]` (_Litob, 592B/0x250) + `[0x8D480,asm]` (xldtob tail `_Ldtob`/`_Ldunscale`/`_Genld`,
  16-aligned). Verbatim ultralib VERSION_J `src/libc/xlitob.c`, **byte-identical cp, first-build
  full-make ROM SHA-1 == baserom, 0 iteration.** `jal-count-mismatch:2vs4` was a FALSE flag (4 jals =
  lldiv+memcpy source calls + 2 compiler u64 div/mod intrinsics `__udivdi3`/`__umoddi3`). **`.data`
  carve** for the file-static digit tables `ldigs`="0123456789abcdef"@0x800C9660 + `udigs`@0x800C9674
  (asm-recovered lui/addiu): `[0xA4A60,.data,libultra/libc/xlitob]` size 0x30 exact first try (segment
  vram→rom delta 0x80024C00, anchored off `gu/align.o(.data)`@0x800C81A0=rom 0xA35A0). Headers all
  resolve (xstdio.h source-private same-dir S54, stdlib.h/string.h); all 4 callees + `_Litob`
  pre-named → **zero symbol adds**. seed 3 / banked 3pt; regime mirror (seed-only). 0 stuck-far/
  permuter/carried/re-opened. Retro applied **2 of 3**: #1 `coddog-fncount-mismatch` extended to
  TAIL-carried coddog identities (the S88 check ran only on the pack leader → func_80050400's
  llcvt.c identity, carried by a tail member, never fired 8vs11; now flags all 3 llcvt phantoms);
  #2 new `coddog-structural:<file>@<pct>` size-ratio guard (`>64 B/LOC`); #3 a `.data`-carve detector
  was **built then reverted** (over-fired — see carry-over). suite 52 pass, golden-inert.
  **Cross-repo follow-up:** none (no new symbols; `_Litob` already in `ghidra_symbols.txt`).
  **Band note: the next libc leaf is the xldtob tail `[0x8D480]` (near-free decompose sibling,
  carry-over below); the broader libultra band is the heavy sched/exceptasm spikes + the `_Printf`/
  xprintf pack (S91-isolated).**

- **Sprint 91: 1 asm-mirror BANKED — `src/libultra/libc/bcmp.s` (`bcmp`), libultra libc asm-mirror;
  split off the pts-13 `[0x8BE20]` bcmp/xprintf pack.** Vendored asm-mirror TUs 20→**21**; the cheap
  libultra mirror band is exhausted (every remaining candidate is a pts-8/13 pack) so the gate verified
  the top coddog "mirrors" are false/partial before picking: `func_80050400` "llcvt.c@99.99" is KMC
  compiler-runtime (fn-sizes 0x28/0xC0/0x84 ≠ llcvt's 0x1c×6/0x80×2, a coddog false fingerprint),
  `func_800660A0` mtxutil tail matches only 2/4 fns (L2F/IdentF; F2L/Ident diverge), `_Litob`/`_Ldtob`
  are the S71 <99% divergent classical leaves. **Mid-gate pivot:** PO first approved **exceptasm.s**
  (8 fns, `.text`=0x970 EXACTLY matches the subseg) but gate disassembly found `__osIntTable` /
  `jtbl_800D2610` — a switch jtbl the active `.text` `jr`s through, emitted SYMBOLICALLY
  (`.word .L800Bxxxx`) in a separate extracted rodata blob → breaks S84 strip-and-rename (vestigial
  `.text` labels) AND can't be carve-placed (VENDOR_ASM `.o` rodata auto-links at section end) → a
  genuine SPIKE → PO pivoted to bcmp + carried exceptasm. Banked `libc/bcmp.s`: nm-u empty, only
  R4300/asm/regdef includes, WEAK skipped non-__sgi, NO data/rodata → cleanest asm-mirror class. Split
  `[0x8BE20]` at 0x8BF30 (`_Printf`@0x800B0B30, 16-aligned; gate-validated green as two asm subsegs
  first), vendor ultralib `libc/bcmp.s` via `VENDOR_ASM` (`8BE20:…`), flip `[0x8BE20]` asm→hasm.
  `build/asm/8BE20.o` .text-only 0x110 exact, full-make ROM SHA-1 == baserom, 0 iteration. `bcmp`
  =0x800B0A20 pre-curated → zero symbol adds. **Bonus:** isolates the `[0x8BF30]` xprintf pack
  (`_Printf`/`_Putfld`/`func_800B1580`) for future classical work. seed 2 / banked 2pt; regime mirror
  (seed-only; 8-gate FIRED on the pts-13 parent → decomposed). 0 stuck-far/permuter/re-opened, **1
  carried** (exceptasm). Applied 3 of 3: #1 `pick_target.py` has-rodata gated to the ACTIVE build
  (`#ifndef _FINALROM`/inactive-`BUILD_VERSION` EXPORTs dropped — exceptasm's `__osCauseTable_pt` no
  longer over-counted); #2 new `vendorable_tu_jtbl` + `asm-mirror-jtbl:<head>` tag (a symbolic-pointer
  table → asm-mirror SPIKE, not S84 replay); #3 `docs/hazards.md#asm-mirror-vendoring` jtbl sub-case +
  has-rodata active-build note + 2 CLAUDE.md index rows. `make test-tools` 52 pass, golden-inert.
  **Cross-repo follow-up: none** (bcmp pre-curated, no new decomp-side symbols). **exceptasm carried →
  Carry-overs below.**

- **Sprint 90: 1 .c file BANKED — `src/libultra/io/pimgr.c` (`osCreatePiManager`), libultra io
  PI-manager drop-def mirror; closes the S84-split `[0x7E360]` PI pack.** md5-candidate 134→**135**;
  asm/hasm subsegs 158→157. The last member of the S84 3-way split (setintmask + epirawdma banked
  S84). Verbatim ultralib `src/io/pimgr.c` (VERSION_J/FINALROM): `#ifndef _FINALROM` ramrom block +
  `_DEBUG` `__osError` strip → ONE fn; every file-scope DEF drops to `extern` so the `.o` emits only
  `.text` (atomic drop-def). **The "mixed `.data`/`.bss` carve" spike framing was over-cautious** (the
  vimgr S87 class): the only `.data` global `__osCurrentHandle` was already placed (S84) → NO carve.
  4 asm-recovered `.bss` drop-to-externs added at the gate (contiguous main_bss block below piacs's
  `piAccessBuf`@0x800FA9B0): `piThread`=0x800F97E0 (0x1B0 OSThread), `piThreadStack`=0x800F9990 (0x1000
  OS_PIM_STACKSIZE), `piEventQueue`=0x800FA990 (0x18), `piEventBuf`=0x800FA9A8 (0x4); sizes inferred
  from inter-symbol gaps + types. Header `piint.h` provides `__osPiDevMgr`/`__osCurrentHandle`;
  `__osPiTable`/`__Dom*SpeedParam` unreferenced (dropped); bare-include `piint.h` per the epirawdma
  sibling. Name `osCreatePiManager`=0x800A3000 pre-curated; all callees + data refs placed (S84/S85).
  Byte-clean first build, 0 iteration, full-make ROM SHA-1 == baserom. seed 5 / banked 5pt; regime
  mirror (seed-only; 8-gate clear at 5<8). 0 stuck-far/permuter/carried/re-opened. Applied 3 of 3:
  #1 `BACKLOG.md` Spike-guidance at-write-time drop-static-test discipline (frame the drop-def verdict,
  not the worst-case carve) + sched.c head framing revised; #2 `pick_target.py` `carry_over_names()`
  region+symbol scoping (the heading-anchored split + live-region bound fixes a 332→50 over-scoop that
  silently dropped name-dropped still-asm functions like `bcmp`/`_Litob` from the ranker) + the
  by-design clarification at the exclusion site (a parked carry-over is retrieved via `--include-stuck`
  / the BACKLOG, NOT smallest-first — S90 pimgr was found that way, not a filter bug); #3
  `docs/hazards.md#recover-extern-refs-unplaced` contiguous-`.bss`-block-sized-by-gaps recover note.
  **Cross-repo follow-up:** 4 new decomp-side `.bss` symbols (`piThread`/`piThreadStack`/`piEventQueue`/
  `piEventBuf`) → propagate via `sync_decomp_names.py --import-from-decomp`. **Band note: the io
  defines-data/file-static mirror traps are now exhausted except `motor.c` (version-branch trap) +
  `sched.c` head (genuine carve/jtbl/log-callee spike); the pimgr drop-def confirms the io
  file-static class is drop-to-extern, never a carve.**

- **Sprint 88: 1 .c file BANKED — `src/libultra/io/contpfs.c` (`__osSumcalc` + `__osIdCheckSum` +
  `__osRepairPackId` + `__osCheckPackId` + `__osGetId` + `__osCheckId` + `__osPfsRWInode`), libultra
  io single-file-pack drop-def mirror; the pfs id/inode core (7 fns banked atomically).**
  md5-candidate 132→**133** (all 133 .c stub-free); flippable asm subsegs 137→136. Verbatim ultralib
  VERSION_J `src/io/contpfs.c`, single 0x2C0 subseg = exactly the 7 fns (one .o — all inner
  boundaries non-16-aligned; flip `[0x89D90, asm]`→`[0x89D90, c, libultra/io/contpfs]`). The upstream
  version guards selected the function set for free: `#if BUILD_VERSION < VERSION_J` strips
  `__osPfsSelectBank` (the separately-banked `pfsselectbank.c`), `#ifdef _DEBUG` strips `__osDumpId`.
  **Drop-def** the 3 cache globals → `extern` (S85/S87 pattern; bytes in extracted data/bss):
  `__osPfsInodeCacheBank`=0x800C9444 pre-placed; recovered `__osPfsInodeCacheChannel`=0x800C9440
  (.data, =-1 = D_800C9440) + `__osPfsInodeCache`=0x801B68E8 (.bss, 0x100 = D_801B68E8). All callees
  pre-placed (__osContRamRead/Write, __osSi*, osRecvMesg, __osContAddressCrc/DataCrc, bzero,
  osGetCount, the 2-arg __osPfsSelectBank). Include adapt `PRinternal/controller.h`→bare. pts-13
  single-file-pack ran under the **verbatim-mirror exemption** (decompose blocked — one .o). **Novel
  bank-gotcha (vendored-header-incomplete):** the reconstructed `controller.h` was `(already-vendored)`
  yet missing `SELECT_BANK` + had an object-like `SET_ACTIVEBANK_TO_ZERO` (vs the source's `()` call)
  → 5 parse errors; aligned both to VERSION_J (add `SELECT_BANK`, function-like macro; grep confirmed
  no other src consumer) + clean-rebuild. Full-make ROM SHA-1 == baserom, 0 C-body iteration. 0
  stuck-far/permuter/carried/re-opened; 1 novel bank-gotcha. seed 13 / banked 13pt; regime mirror
  (8-gate fired → exemption; seed-only). Applied 4 of 4: #1 `docs/hazards.md#vendored-header-incomplete`
  + CLAUDE.md row (the `needs-macro` auto-detector DEFERRED — FP risk needs preprocessing; tracked
  below); #2 `pick_target.py coddog-fncount-mismatch` (under-count only); #3 `pick_target.py one-tu`
  + `decomp_asm.asm_function_addrs`; #4 this `motor.c` version-branch trap note. `make test-tools`
  50→52 pass, golden regen (28 `one-tu` additions, diff is flag-only). **Cross-repo follow-up:** 9
  new decomp-side symbols (7 fns + `__osPfsInodeCache`/`__osPfsInodeCacheChannel`) → propagate via
  `sync_decomp_names.py --import-from-decomp`. **Band note: the clean libultra single-file-packs are
  now the next pool — the remaining packs are mostly multi-file `c-combined` (sched/__assert/_Litob,
  decompose at the file boundary) or hazardous (llcvt soft-float, contquery, the motor/pimgr traps).**

- **Sprint 89: 1 .c file BANKED — `src/libultra/io/sirawdma.c` (`__osSiRawStartDma`), libultra io
  SI-DMA verbatim mirror; decomposed the `osCreateScheduler` `c-combined:2file[sched|sirawdma]` pack.**
  md5-candidate 133→**134** (all 134 .c stub-free); flippable asm subsegs 136→136 (sched head stays
  asm, +1 C subseg). The `0x86A50` subseg is a c-combined of the heavy `sched.c` head + the clean
  `sirawdma.c` tail; pts-13 tripped the 8-gate → **decomposed at the upstream-file boundary** (the
  S74/S75 contquery/contreaddata pattern): keep `[0x86A50, asm]` for the sched head (0x800AB650..
  0x800AC060), add `[0x87460, c, libultra/io/sirawdma]` for the 176B tail (`__osSiRawStartDma`=
  0x800AC060, 16-aligned, pre-placed S74). Verbatim ultralib VERSION_J `src/io/sirawdma.c`, sibling
  of S4 sirawread/sirawwrite; **first-build full-make ROM SHA-1 == baserom, 0 iteration**. Uses the
  `>= VERSION_J` `IO_READ(SI_STATUS_REG)` busy-check branch (the function EXISTS in J — NOT the
  motor.c version trap). Two known edits: include adapt `PRinternal/siint.h`→bare `siint.h` (S74
  convention) + `#ifdef _DEBUG`-wrap the bare upstream `assert` (assert-strip; held — ROM strips it,
  the sirawread/sirawwrite/epirawread convention). All callees pre-placed (osWritebackDCache=
  0x800A70E0, osVirtualToPhysical=0x800A7720, osInvalDCache=0x800A6FB0); SI/PIF macros in vendored
  siint.h/rcp.h; no data/rodata (the row's file-static / defines-data:count,firsttime /
  rodata-jtbl:0x800D25C0 / log-fn hazards ALL belong to the still-asm sched head). Pure text mirror,
  zero new symbols (name pre-placed). seed 2 / banked 2pt; regime mirror (8-gate resolved by
  decompose; seed-only). 0 stuck-far/permuter/carried/re-opened. Applied 1 of 1: #1 BACKLOG staleness
  reconciliation — `piacs.c` marked BANKED (was a stale "remaining io trap" through S88; green ROM
  since the `cbaf80a` 2026-06-13 layout refactor), carry-over de-paired from `motor.c`. **Cross-repo
  follow-up: none** (no new decomp-side symbols; `__osSiRawStartDma` pre-placed). **Band note: the
  sched.c head (osCreateScheduler + ~13 fns) is now a standalone heavy spike (file-static +
  count/firsttime defines-data + rodata-jtbl switch + 5 log callees) → carry-over below; remaining
  multi-file packs `__assert`/`_Litob`/llcvt decompose or are soft-float-hazardous; io traps now
  motor + pimgr only.**

- **Sprint 87: 1 .c file BANKED — `src/libultra/io/vimgr.c` (`osCreateViManager` + `viMgrMain`),
  libultra io drop-STATIC mirror; the vimgr.c carry-over, banked once S86 cleared its timer-wall.**
  md5-candidate 131→**132** (all 132 .c stub-free); flippable asm subsegs 138→137. Verbatim ultralib
  VERSION_J `src/io/vimgr.c`, single 0x340 subseg = exactly the 2 fns (single-file-pack, no split;
  flip `[0x88210, asm]`→`[0x88210, c, libultra/io/vimgr]`). **The carry-over's "heavy file-static
  `.bss` carve" framing was the false-flag this sprint retires:** the 6 file-statics + 2 globals + 1
  func-local static are all UNINITIALIZED → pure `.bss` (no ROM bytes) → DROP to sized `extern`s
  placed at recovered `main_bss` vrams (`retrace`=0x800FAA10, `viThread`=0x800FAA18,
  `viThreadStack`=0x800FABD0 [STACK_START +0x1000 = `viEventQueue`], `viEventQueue`=0x800FBBD0,
  `viEventBuf`=0x800FBBE8, `viRetraceMsg`=0x800FBC00, `viCounterMsg`=0x800FBC18,
  `__osViDevMgr`=0x800C8250), **NO carve, NO classical loop** (the S81 `siacs.c` drop-to-extern
  pattern; `__additional_scanline`=0x800C826C pre-placed S48). `osCreateViManager` pre-curated;
  `viMgrMain` (static fn) name added at gate. All callees placed (timer-side by S86). Include adapt
  `PRinternal/{viint,osint}.h`→bare (sibling visetevent/visetmode). Full-make ROM SHA-1 == baserom,
  0 iteration. 0 stuck-far/permuter/carried/re-opened. seed 5 / banked 5pt; regime mirror (8-gate
  clear at 5<8; seed-only). Applied 3 of 3: #1 `docs/hazards.md#file-static-bss-layout-conflict`
  uninitialized-static = pure-`.bss` drop-to-extern-mirror split + new `drop-static-mirror:<n>bss`
  re-frame tag in `pick_target.py` (`drop_static_mirror_hazard`; +1 unit test, golden-neutral) +
  CLAUDE.md index row; #2 `pick_target.py _C_NONCALL += aligned,__attribute__` (the `ALIGNED`/`STACK`
  macro-expansion attribute residue mis-flagged `calls-unplaced:aligned`; golden regen = 4
  `aligned`-only removals); #3 folded into #1. `make test-tools` 48→49 pass. **Cross-repo
  follow-up:** 9 new decomp-side symbols (`viMgrMain` + the 8 `.bss` data symbols) → propagate via
  `sync_decomp_names.py --import-from-decomp` (`osCreateViManager` already in `ghidra_symbols.txt`).
  **Band note: the `drop-static-mirror` tag now re-prices `osMotorStop`/`motor.c` (drop-static-mirror:2bss)
  — the io file-static traps that looked like carve spikes are mostly drop-to-extern mirrors;
  remaining heavy ones are pimgr [0x7E400] (mixed `.data`/`.bss`, needs verify) + the sched/contpfs
  packs (carve signals / 8-gate).**

- **Sprint 86: 1 .c file BANKED — `src/libultra/os/timerintr.c` (`__osTimerServicesInit` +
  `__osTimerInterrupt` + `__osSetTimerIntr` + `__osInsertTimer`), libultra os/ drop-def mirror; the
  OS timer-service core (`vimgr.c`/`settimer.c` dependency root).** md5-candidate 130→**131** (all 131
  .c stub-free); flippable asm subsegs 139→138. Verbatim ultralib VERSION_J `src/os/timerintr.c`,
  single 0x300 subseg = exactly the 4 fns (flip `[0x87C40, asm]`→`[0x87C40, c, libultra/os/timerintr]`,
  no split). pts-8 single-file-pack tripped the 8-gate → ran under the **verbatim-mirror exemption,
  now extended to the drop-def sub-case** (S64/S69 class; regime mirror + single-file-pack
  decompose-blocked + all 4 names curated + all callees placed). **Pure drop-def, NO carve** (the only
  remaining libultra trap with no file-static): the 6 file-scope data globals → `extern`; `-D_FINALROM`
  strips the `#ifndef _FINALROM` profile block; the `VERSION_K` `tim<468` clamp correctly excluded for
  J. 3 pre-placed (`__osCurrentTime`/`__osBaseCounter`/`__osTimerList`, S27/S30), 2 recovered from asm
  (`__osViIntrCount`=0x800FF1E0, `__osTimerCounter`=0x80132364), and **`__osBaseTimer` needed NO
  extern/placement** — named only in `__osTimerList`'s dropped `.data` initializer
  (`OSTimer* __osTimerList = &__osBaseTimer;`), so the placed pointer's bytes encode its address (the
  S86 #1 refs_unplaced fix now drops this class). All callees placed (osSendMesg/osGetCount/
  __osDisableInt/__osRestoreInt/__osSetCompare); include adapt `PRinternal/osint.h`→bare (sibling
  settimer/gettime). Full-make ROM SHA-1 == baserom, 0 iteration. 0 stuck-far/permuter/carried/
  re-opened. seed 8 / banked 8pt; regime mirror (seed-only). Applied 4 of 4: #1 `pick_target.py
  refs_unplaced` drops a self-defined global named only in another global's depth-0 initializer
  (`_names_in_function_bodies`; +1 unit test, suite 48 pass, golden-neutral); #2 CLAUDE.md bank-step
  generated-artifact commit-hygiene note (`undefined_syms_auto.txt`+`mariogolf64.ld`); #3 CLAUDE.md
  8-gate exemption wording extended to drop-def; #4 the `vimgr.c` carry-over update below.
  **Cross-repo follow-up:** 2 new decomp-side symbols (`__osViIntrCount`, `__osTimerCounter`) →
  propagate via `sync_decomp_names.py --import-from-decomp` (the 4 fn names were already in
  `ghidra_symbols.txt`). **Band note: timerintr banking places vimgr.c's timer-side deps; the
  remaining libultra is the io/os file-static traps (pimgr [0x7E400], piacs/motor, vimgr, sched/
  contpfs packs) + the `__osDisableInt`/`__osRestoreInt` partial-TU asm-vendor split. Smaller
  libnusys leaves (nuContRmbModeSet pts-3, nuGfxDisplayOn pts-3) rank cheaper than the libultra
  traps.**

- **Sprint 85: 1 .c file BANKED — `src/libultra/os/initialize.c` (`__osInitialize_common` +
  `create_speed_param`), libultra os/ coddog mirror; the S80-teed-up next-cleanest coddog leaf.**
  md5-candidate 129→**130** (all 130 .c stub-free); asm subsegs 140→139. 13th coddog cross-ref sprint.
  Verbatim ultralib VERSION_J `src/os/initialize.c` (single 0x2F0 subseg = exactly the 2 fns; flip
  `[0x8ACA0, asm]`→`[0x8ACA0, c, libultra/os/initialize]`, no split). **The carry-over over-stated it**
  (recurring theme): framed as a cross-region `.data` carve + name-reconcile, but the actual path was
  the **drop-def fast path** (S82 default — extern the 4 `.data` globals + `__osFinalrom`; main_data
  provides the bytes, NO carve). **Two NEW reconcile classes surfaced in execution, both → tooling:**
  (1) **header-renames-symbol** — `os_host.h` `#define __osInitialize_common() osInitialize()` (K→J
  source-compat shim, transitively included) rewrote the C function def → the curated symbol was
  undefined at link (caller wants `__osInitialize_common`, C exported `osInitialize`); fixed with
  `#undef __osInitialize_common` (S31 `#undef nuGfxInit` class, the 2nd instance). (2) **VERSION_K-gate
  too aggressive for MG64-J** — `__osSetWatchLo(0x4900000)` gated `#if BUILD_VERSION >= VERSION_K` but
  present in MG64's J build → the EXACT 8-byte/2-instr SHA-miss (`.o` fn 0x228 vs asm 0x230), localized
  by `.o`-disasm vs Ghidra MCP, fixed by un-gating that one line to `>= VERSION_J` (createSpeedParam's
  body, by contrast, has a `#elif == VERSION_J` branch so it compiled as-is). 7 symbol_addrs adds: gate
  `osClockRate`=0x800C9460 / `__osFinalrom`=0x801B74D8 / `__osExceptionPreamble`=0x800AFB90 (was
  func_800AFB90) / `__Dom1SpeedParam`=0x80106248 / `__Dom2SpeedParam`=0x800FEC98; execution
  `osResetType`=0x8000030C / `osAppNMIBuffer`=0x8000031C (BOOT_GLOBALS class, S46). 0 C-body iterations.
  Clean rebuild ROM SHA-1 == baserom. seed 5 / banked 5pt; regime mirror (seed-only; 8-gate clear at
  5<8). 0 stuck-far/permuter/carried/re-opened. Applied 3 of 3: #1 `pick_target.py`
  `header_renames_symbol` detector (transitive header scan for a macro rewriting the curated leader) +
  `header-renames-symbol:<fn>@<hdr>` flag wired into both hazard appenders + unit test +
  `docs/hazards.md#header-renames-symbol` + CLAUDE.md index row; #2 `docs/hazards.md#needs-define`
  VERSION_K-gate-present-in-J sub-case (the N×8B SHA-miss tell + .o-disasm localize) + CLAUDE.md index
  symptom row; #3 `BACKLOG.md ## Carry-overs` drop-def-default guidance + asm-recovered-address rule
  (corrected pimgr's wrong `__Dom*SpeedParam` addrs). `make test-tools` 47 pass, golden-neutral.
  **Cross-repo follow-up:** 7 new decomp-side symbols → propagate via `sync_decomp_names.py
  --import-from-decomp` (the 2 fn names were already in `ghidra_symbols.txt`). **Band note: the os/
  band's clean coddog leaf is now banked; remaining libultra is the io/os defines-data+file-static
  traps (pimgr [0x7E400], piacs/motor, contpfs/vimgr/timerintr/sched packs) + the asm-vendoring TUs
  (__osDisableInt/__osRestoreInt partial-TU split). Smaller libnusys leaves (nuContRmbModeSet pts-3,
  nuGfxDisplayOn pts-3) now rank ahead — a cheaper next sprint than the libultra traps.**

- **Sprint 48: 1 file BANKED — `src/libultra/io/viswapcontext.c` (`__osViSwapContext`), libultra; 11th vi-band sibling.** md5-candidate 88→89 (all 89 .c files now stub-free). Single fn 0x88810. Verbatim ultralib VERSION_J `src/io/viswapcontext.c` (include `PRinternal/viint.h` → bare `viint.h`, sibling convention). One recover-extern at gate: `__additional_scanline`=0x800C826C (size:0x4, `extern u32` per viint.h, recovered from `lui 0x800d / lw -0x7d94`). `__osViNext`/`__osViCurr` already placed; `__OSViContext` from pick_target refs-unplaced was a **struct TYPE** (0x30B viint.h), not a data symbol — ruled out. `.text` matched first compile (0x310B); only the **rodata-sibling** for the `2^32` u32→float double needed a yaml split (`[0xAD9C0, rodata]` → insert `[0xAD9E0, .rodata, libultra/io/viswapcontext]`, 16B = double + 8 pad; same as S38 aisetfreq). seed 5 / banked 5pt; regime mirror (seed-only). All 0 stuck-far/permuter/carried/re-opened. Applied: 2 of 2 (#1 `pick_target.py`/`decomp_asm.py` `declared_type_names` — excludes typedef'd types from refs-unplaced; #2 `rodata-literal:<addr>` pre-flag for mirror candidates loading anonymous `ldc1/lwc1 %lo(D_)` FP constants; hazards.md + CLAUDE.md index updated, `make test-tools` 23 pass). No carry-overs. **Cross-repo follow-up:** `__additional_scanline`=0x800C826C is a new decomp-side symbol — propagate via `sync_decomp_names.py --import-from-decomp`. **Band note: cont/pfs/timer io siblings remain (companion-copies + recover-externs); the vi band has no smaller clean leaves left.**

- **Sprint 46: 2 files BANKED — `src/libultra/io/pirawdma.c` (`__osPiRawStartDma`) + `pigetcmdq.c` (`osPiGetCmdQueue`), libultra; reopens the PI/SI/cont/pfs mirror band.** md5-candidate 85→87. 0x8BA20 3-fn pack split at vram 0x800B06F0 (pigetcmdq) + 0x800B0710 (`func_800B0710` left asm). **Root unblock — a multi-sprint `pick_target` false-`blk`:** ultralib mirrors `#include "PRinternal/<h>"` but the project shipped those internal headers under `internal/`, so `include_is_blocked` matched the include *basename* (`piint.h`) against in-tree `internal/piint.h` and mislabeled a cheap companion-copy as a deferred-`-I` block → the whole PI/SI/cont/pfs/vi/timer band read `blk needs-header` and was un-pickable. Fixed at retro: full-relative-path match (not basename) + ultralib/include as the primary libultra companion-header root → 11 band fns un-`blk`'d (`osCartRomInit`, `__osContRam{Read,Write}`, `__osPfsGetStatus`, `osSpTaskLoad`, `osContInit`, `osCreateViManager`, `osMotorStop`, `__osViSwapContext`, `__osTimerServicesInit`, `__osGbpakSetBank`). Enabler: verbatim `cp ultralib/include/PRinternal/piint.h → include/libultra/PRinternal/` (deps `PR/os_internal.h`+`PR/rcp.h` in-tree). `__osPiRawStartDma` (224 B): `_DEBUG` block compiles out; recover-extern `osRomBase`=0x80000308 — a libultra **boot-region global** asm-baked as `D_80000308`, missed by refs-unplaced's `__`-prefix grep → 2nd retro fix: `BOOT_GLOBALS` table (0x80000300-0x1C) surfaces them with known vram. `osPiGetCmdQueue` (32 B): 2-line getter, only ref `__osPiDevMgr` placed. Both verbatim cp, names pre-placed, 0 iter. seed 2 / banked 2pt; regime mirror (seed-only). All 0 stuck-far/permuter/carried/re-opened. Applied: 2 of 2 (#1 `include_is_blocked` full-path + ultralib companion root; #2 `BOOT_GLOBALS` recover table; golden refreshed, 20 pass/3 skip). No carry-overs. **Cross-repo follow-up:** `osRomBase`=0x80000308 is a new decomp-side symbol — propagate via `sync_decomp_names.py --import-from-decomp`. **Band note: the cont/pfs/vi siblings now need only `controller.h`/`siint.h`/`macros.h`/`viint.h` companion-copies (cheap) — the next mirror pool.**
- **Sprint 45: 2 files BANKED — `src/libultra/shared/gbpak/gbpakreadwrite.c` (`osGbpakReadWrite`) + `gbpakreadid.c` (`osGbpakReadId`), libultra; closes the `shared/gbpak/` band.** md5-candidate 83→85 — **all 85 c files now md5-candidate, 0 INCLUDE_ASM stubs anywhere in `src/`.** 0x88FC0 2-fn pack split at vram 0x800ADD90. Both near-verbatim mirrors where MG64 OMITS upstream blocks, surfacing only at the full-make SHA miss (`.o`-diff localizes, S18/S44 class). `osGbpakReadWrite` (464 B): drops upstream `if (size == 0) return 0;` — a **jal-less** early-return invisible to jal-counting (folds into the later `blez` uninit-`ret` path); 1 edit. `osGbpakReadId` (400 B): drops the upstream `if(bcmp){ write-temp; reread; recheck }` retry block (jal 12→7, + `temp[32]` local) → `if (bcmp(...)) return 4;`; `.text` then matched 97/97 but SHA missed on `.data` — function-local `static nintendo[]`/`mmc_type[]` live in the shared `main_data` blob (0x800C93F0/0x800C9420; `D_<vram>` name == real vram, `.NON_MATCHING` map addr is an alias) → dropped static defs → sized `extern u8 nintendo[48]; mmc_type[20];` + 2 `symbol_addrs` adds renaming the dlabels (S44 defines-data fast-path, no main_data split). Two recover-callees added at gate (`__osGbpakSetBank`=0x800B1A90, `bcmp`=0x800B0A20). seed 3 / banked 3pt; regime mirror (seed-only). All 0 stuck-far/permuter/carried/re-opened. Applied: 2 of 3 (#1 hazards.md jal-less-dropped-block bullet in Near-verbatim section; #2 hazards.md defines-data function-local-statics-in-shared-blob paragraph; #3 log-only). No carry-overs. **Cross-repo follow-up:** `__osGbpakSetBank`=0x800B1A90, `bcmp`=0x800B0A20, `nintendo`=0x800C93F0, `mmc_type`=0x800C9420 are new decomp-side symbols — propagate via `sync_decomp_names.py --import-from-decomp`.

- **Sprint 44: 3 files BANKED — `src/libultra/monegi/thread/createthread.c` (`osCreateThread`) + `destroythread.c` (`osDestroyThread`) + `src/libultra/shared/gbpak/gbpakinit.c` (`osGbpakInit`), libultra; first sprint under the PO libultra-epic directive.** md5-candidate 80→83. Thread pack 0x87600 split at vram 0x800AC2D0 (createthread rom 0x87600 / destroythread rom 0x876D0); gbpakinit single subseg 0x88B80 flipped `c`. **createthread near-verbatim cast divergence:** verbatim copy matched every instruction except `context.ra` — baserom sign-extends `(s64)(s32)__osCleanupThread` (`sra v0,a0,0x1f`) where libultra_modern zero-extends `(u64)(u32)` (`move v0,zero`); VERSION_J sign-extends the whole OSContext block (sibling `sp`/`a0` already did) → one-token fix, caught by `.o`-diff on the first SHA miss (invisible to every gate check; same late-surfacing class as S18/S40). `osDestroyThread` clean verbatim, 0 edits. One recover-extern enabler `__osCleanupThread`=0x800B04E8 (refs-unplaced, createthread's context.ra); `_FINALROM` drops the `thprof` block; all other thread callees/headers pre-placed (band proven S8/12/14/35). `osGbpakInit` (512 B): **defines-data verbatim-body fast-path** (S42) — dropped file-scope `__osGbpakTimer`/`__osGbpakTimerMsg`/`__osGbpakTimerQ` defs → externs from `controller_gbpak.h`, storage from the S43-placed data region; all 6 callees pre-placed (incl. S43's `__osContRamWrite`/`__osContRamRead`/`__osPfsGetStatus`); matched first `make`, 0 iter. seed 6 / banked 6pt; regime mirror (seed-only). All 0 stuck-far/permuter/carried/re-opened. Applied: 1 of 2 (#1 new `docs/hazards.md#mirror-cast-divergence-sign--vs-zero-extend` section + CLAUDE.md index row; #2 confirmatory log-only). No carry-overs. **Cross-repo follow-up:** `__osCleanupThread`=0x800B04E8 is a new decomp-side symbol (Ghidra had no function there) — propagate via `sync_decomp_names.py --import-from-decomp`.

- **Sprint 43: 2 files BANKED — `src/libultra/shared/gbpak/gbpakpower.c` (`osGbpakPower`) + `gbpakgetstatus.c` (`osGbpakGetStatus`), libultra gbpak pair; classical-flagged but both proved verbatim mirrors.** md5-candidate 78→80. Both subsegs flipped `c` (0x88EA0 power, 0x88D80 getstatus; no split — separate subsegs). Both carried `jal-count-mismatch` that turned out to be macro FPs (power 6vs5 = `OS_USEC_TO_CYCLES`; getstatus 6vs4 = 2× `ERRCK`), so both seeded as verbatim upstream bodies (libultra_modern `src/shared/gbpak/`) and matched first try. **5 symbols recovered at the gate** (S41 deterministic pattern): refs-unplaced `__osGbpakTimer`=0x8012F490 size:0x20 / `__osGbpakTimerMsg`=0x800FF4AC size:0x4 / `__osGbpakTimerQ`=0x801B70F8 size:0x18 (from osSetTimer arg setup) for power; calls-unplaced-dual `__osContRamRead`=0x800AF420 / `__osPfsGetStatus`=0x800AE710 (both `func_<addr>` in Ghidra, their jal targets) for getstatus. getstatus hit the isolation artifact (score 15 / 99.8%, empty `top_mismatches`, 76/76 rows) → full-make SHA proved byte-match, no iteration. seed 6 / banked 6pt (realized 4, residual −2 — classical-flag was a tooling artifact). All 0 stuck-far/permuter/carried/re-opened. Applied: 2 of 2 (#1 generalised `_c_jal_count` to drop every invoked function-like macro via the S41 `all_func_macros()` table — also fixed sprintf `2vs1`→clean and osCartRomInit `21vs5`→`6vs5`; #2 isolation-artifact recognition-signal doc in CLAUDE.md + hazards). No carry-overs. **Cross-repo follow-up:** `__osContRamRead`/`__osPfsGetStatus` still `func_<addr>` in the Ghidra workspace — propagate via `sync_decomp_names.py --import-from-decomp`.

- **Sprint 42: 2 files BANKED — `src/libultra/monegi/message/sendmesg.c` (`osSendMesg`) + `seteventmesg.c` (`osSetEventMesg`), libultra message pair; verbatim mirror + first defines-data verbatim-body fast path.** md5-candidate 76→78. Split the 0x86550 2-fn message pack at the function boundary (sendmesg 0x86550 + seteventmesg 0x86680, both flipped `c`). `osSendMesg`: clean verbatim libultra mirror, warm message band (createmesgqueue S2, jammesg/recvmesg S34), all 6 refs pre-placed, 0 iter. `osSetEventMesg`: **defines-data drop** — body verbatim, dropped file-scope `__OSEventState __osEventStateTab[15] ALIGNED(8)` + `u32 __osPreNMI` defs, added one `extern u32 __osPreNMI;`; 3 data externs placed add-only at the gate (`__osEventStateTab`=0x801B7078 size:0x78 [`_FINALROM`→OS_NUM_EVENTS=15 ×8], `__osPreNMI`=0x800C81F0, `__osShutdown`=0x800C946C). PO swapped this defines-data sibling in over a libnusys filler at the plan gate. Both matched in one full `make`, ROM SHA-1 green. seed 5 / banked 5pt (sendmesg 2 mirror + seteventmesg 3 classical, realized 2, residual −1). **Two pick_target blind spots, both gate-caught:** (a) `jal-count-mismatch:7vs6` was a `MQ_IS_FULL` macro-pseudo-call FP (per-fn 6vs6/3vs3 clean); (b) `defines_data_globals` never flagged `__osEventStateTab` — `ALIGNED(8)` defeats the regex + paren-guard. Applied: 3 of 3 (#1 `MQ_IS_FULL`/`MQ_IS_EMPTY` → `_C_NONCALL`; #2 `docs/hazards.md` defines-data verbatim-body fast-path note; #3 `defines-data:<name>[DIM]` array-dim annotation). No carry-overs. **Carried suggestion: extend `defines_data_globals` to ALIGNED/attribute-suffixed defs** (would have auto-flagged `__osEventStateTab`; needs the paren-guard relaxed for known attribute macros without re-admitting function decls).

- **Sprint 41: 1 file BANKED — `src/libultra/nintendo/pi/epirawwrite.c` (`__osEPiRawWriteIo`), libultra pi IO_WRITE verbatim mirror; first macro-hidden recover-extern.** md5-candidate 72→73. Clean single-fn subseg (0x8BC80, no split); `__osEPiRawWriteIo` name pre-curated; `piint.h`+`PR/ultraerror.h` in-tree; `_DEBUG` block compiles out. **New recover-extern blind spot → tooling closed it:** the unplaced global hid inside the `EPI_SYNC` *library macro* (`piint.h` → `__osCurrentHandle[domain]`), not the `.c` body — invisible to BOTH `pick_target.py`'s ref-grep and the gate build-check (INCLUDE_ASM scaffold never compiles the body); surfaced as `undefined reference to __osCurrentHandle` at link in the execution middle (S23 `calls-unplaced` / S40 wrong-lib-header pattern). Recovered deterministically: `__osCurrentHandle`=0x800C7E90 (`OSPiHandle*[2]` → size:0x8) from the fn's own `lui $a3,%hi(D_800C7E90)`+`lw %lo(D_800C7E90)`; index `domain*4` separate `addu` → base direct, no field-offset. IO_WRITE isolation artifact (S34) → no isolation spot-check; ROM SHA-1 green, 0 iterations. seed 2 / banked 2pt. Applied: 2 of 2 (#1 CLAUDE.md macro-hidden recover-extern bullet; #2 `pick_target.py` refs/calls-unplaced follow one level of macro expansion — full-table diff confirmed strict de-noise + real `__osMotorAccess` surfaced). No carry-overs.

- **Sprint 40: 1 file BANKED — `src/libultra/monegi/libc/ldiv.c` (`ldiv`+`lldiv`), libultra verbatim mirror; first `monegi/libc/` mirror.** md5-candidate 71→72. Whole-file 2-fn pack (subseg 0x8DF50 = exactly the two fns, no split); both names pre-curated; `__divdi3` (lldiv's 64-bit-divide callee) pre-placed. **Build-system friction → root-caused, not symptom-patched:** verbatim `ldiv.c`'s `#include "stdlib.h"` resolved to libkmc's `stdlib.h`, which lacks libultra-only `lldiv_t` — a *resolvable-but-wrong-library* header that both `pick_target.py`'s `needs-header` grep and the gate build-check miss (scaffold never compiles the C body; surfaces in execution middle like `calls-unplaced`). Fixed by vendoring `ultralib/include/compiler/gcc/stdlib.h` verbatim → `include/libultra/compiler/gcc/stdlib.h` + prepending `-I` to `LIBULTRA_CFLAGS` only (libkmc/libnusys unaffected; verified per-file with `gcc -M`); libkmc header left verbatim. seed 2 / banked 2pt. Applied: 1 of 2 (#2 CLAUDE.md per-lib std-header isolation bullet; #1 `cross-lib-header` hazard deferred). No carry-overs.

- **Sprint 38 (retroactive bank): 1 file BANKED — `src/libultra/monegi/ai/aisetfreq.c` (`osAiSetFrequency`), libultra verbatim mirror; resolved in the same session as the S38 retro.** md5-candidate 72→73; matched 79→80/2090 (~3.83%). Both blockers resolved: Layer 1 (`-G 0` on `tools/cc/as` — binutils + ultralib confirmed); Layer 2 (`.rodata` placement — dot-prefix `[0xAD6A0, .rodata, libultra/monegi/ai/aisetfreq]` subseg becomes splat sibling of the `c` subseg, routing `aisetfreq.o(.rodata)` to 0x800D22A0 without asm extraction). `osViClock=0x800C9468` recover-extern; IO_WRITE isolation artifact (expected; verbatim mirror + ROM SHA-1 = proof). seed 2 / banked 2pt. Applied: 1 of 1 (#1 `.rodata` sibling yaml pattern → CLAUDE.md conventions).

- **Sprint 37: 4 files BANKED — `src/libnusys/mainlib/nupireadrom.c` (`nuPiReadRom`) + `src/libultra/monegi/ai/aigetlen.c` (`osAiGetLength`) + `src/libultra/monegi/ai/aigetstat.c` (`osAiGetStatus`) + `src/libultra/monegi/vi/visetspecial.c` (`osViSetSpecialFeatures`); first libnusys main-segment classical + AI pair + vi mirror.** md5-candidate 68→72; matched 75→79/2090 (~3.78%). `nuPiReadRom` (224 B, classical): no upstream match — ROM uses undocumented variant (osInvalDCache×2/iter, all struct setup inside loop); `nuPiCartHandle`=0x801B55C8 recover-extern; matched after 3 iterations on delay-slot ordering; seed 3 / realized 3 / residual 0. `osAiGetLength` + `osAiGetStatus` (16 B each, verbatim mirrors): IO_READ isolation artifact — .o spot-check inapplicable (MMIO literal-vs-reloc); ROM SHA-1 green. `osViSetSpecialFeatures` (368 B, verbatim mirror): `refs-unplaced:__osViDevMgr` = dead `_DEBUG` FP; spot-check MATCH 368B. **Carry-over: `osAiSetFrequency` (0x7EEC0, 288B)** — osViClock + rodata D_800D22A0 unplaced; stays `[0x7EEC0, asm]`. seed 7 (3+1+1+2) / banked 7pt. Applied: 0 of 0.

- **Sprint 36: 2 files BANKED — `src/libultra/monegi/audio/heapinit.c` (`alHeapInit`) + `src/libultra/monegi/audio/copy.c` (`alCopy`); libultra audio band unlock; PO library-first directive.** md5-candidate 66→68; matched 73→75/2090 (~3.59%). **Audio band unlocked** (parallel to S15's libnusys unlock): one Makefile enabler (`-I include/libultra/PR` added to `LIBULTRA_CFLAGS`) + one companion header copy (`synthInternals.h` → `include/libultra/internal/`) opens the whole `libultra/monegi/audio/` band. PO mid-sprint libnaudio concern resolved — MG64 uses libnaudio for playback but `alCopy`/`alHeapInit` are shared utility functions identical in both libraries; `libultra_modern` source is authoritative. Both verbatim cp, 0 iterations. seed 6 / banked 6pt. **Next audio targets remain `blk`**: `alHeapDBAlloc` needs `os_internal.h` + `ultraerror.h` companion copies (or `-I` path additions) to unlock. No carry-overs.

- **Sprint 35: 4 files BANKED — `src/libultra/monegi/thread/startthread.c` (`osStartThread`) + `src/libultra/monegi/vi/vigetcurrframebuf.c` (`osViGetCurrentFramebuffer`) + `src/libultra/monegi/vi/vigetnextframebuf.c` (`osViGetNextFramebuffer`) + `src/libultra/monegi/vi/vigetmode.c` (`osViGetCurrentMode`); thread-band + vi-band trio; side-win mmuldi3 hasm vendor.** md5-candidate 62→66. All 4 verbatim mirrors, 0 iterations. `osStartThread` had `jal-count-mismatch:9vs7` — **resolved verbatim** (GCC -O3 tail-merge shares one `jal __osEnqueueThread` across 3 switch-case paths; preliminary "2 dropped jals" DoR note was superseded; try-verbatim-first is the correct approach for small mismatches). VI trio: no hazards, warm band, all deps pre-placed. **Side-win:** vendor `src/libkmc/mmuldi3.s` (6× `li $r,0xffffffff` → `addiu $r,$0,-1` compat patch; Makefile explicit-rule override for `build/asm/8EC50.o`; consolidates 5 yaml hasm subsegs into 1). seed 16 / banked 16pt. Retro: **1 of 1 applied** (#1 CLAUDE.md jal-count-mismatch: small ≤2 + identical-arg multi-branch → try verbatim first; GCC -O3 tail-merge documented). No carry-overs.

- **Sprint 34: 3 files BANKED — `src/libultra/monegi/message/jammesg.c` (`osJamMesg`) + `src/libultra/monegi/message/recvmesg.c` (`osRecvMesg`) + `src/libultra/monegi/ai/aisetnextbuf.c` (`osAiSetNextBuffer`); warm message-band pair + classical AI set-buffer with IO_WRITE isolation artifact.** md5-candidate 59→62; matched 66→69. `osJamMesg` + `osRecvMesg`: verbatim mirrors, 0 iterations; `MQ_IS_EMPTY` hazard = macro FP (gate confirmed). `osAiSetNextBuffer`: classical static-drop — file-scope `static u8 hdwrBugFlag` dropped, extern recovered at 0x800C7EC0 (vram from fn's own `lui 0x800c`/`lbu 0x7ec0`); **IO_WRITE isolation artifact** — score never reaches 0 (MMIO literal-vs-reloc); C verified against asm + in-tree spot-check MATCH → ROM SHA-1 green; 0 real iterations. seed 5 (1+2+2) / banked 5pt. Retro: **2 of 2 applied** (#1 IO_WRITE/IO_READ isolation artifact convention bullet; #2 data-global stale asm label sync convention bullet). No carry-overs.

- **Sprint 33: 1 file BANKED — `src/libultra/nintendo/pi/piacs.c` (`__osPiCreateAccessQueue`+`__osPiGetAccess`+`__osPiRelAccess`); first sprint requiring per-lib CFLAGS discovery; libultra `-O3 -funsigned-char` and global `-mips3` established.** md5-candidate 58→59. Classical loop (file-scope `static OSMesg piAccessBuf[]` BSS-layout-conflict + `u32 __osPiAccessQueueEnabled` defines-data — both dropped, externs recovered via `symbol_addrs.txt`). **Compile-flag discovery:** ultralib gcc.mk reveals libultra was built with `-O3 -mips3 -funsigned-char` for VERSION_J; global `-mips2`→`-mips3` (ROM-wide SHA-1 verified); `LIBULTRA_CFLAGS := $(subst -O2,-O3,$(CFLAGS)) -funsigned-char -DBUILD_VERSION=VERSION_J` + pattern rule added to Makefile; `decomp_loop.py` libultra auto-detect added. `__osPiGetAccess` required both fns in same TU (so `-O3` inlines `__osPiCreateAccessQueue`) — both-functions seed is the structural insight. All 3 fns score 0 first seed. `sync-names` mid-sprint eviction caused 3 undefined refs + label mismatch (recovered via `symbol_addrs.txt` adds + stale asm label fix). seed 5 / banked 5pt / realized 5pt (residual 0; first-pass clean all 3; flag-discovery overhead not counted as classical iteration). Retro: **4 of 4 applied** (#1 sync-names eviction guard; #2 libultra CFLAGS bullet; #3 decomp_loop.py libultra doc; #4 stale asm label-rename note). No carry-overs.

- **Sprint 32: 2 files BANKED — `src/libultra/monegi/rsp/sptaskyield.c` (`osSpTaskYield`) + `src/libultra/monegi/libc/syncprintf.c` (`osSyncPrintf`+`rmonPrintf`); -D_FINALROM global Makefile enabler; VERSION_J source cross-reference; -nostdinc removed.** md5-candidate 56→58 (all 58 C files now md5-candidate). `osSpTaskYield`: verbatim zero-enabler mirror — `__osSpSetStatus`+`SP_SET_YIELD`=0x400 pre-placed; one yaml flip. `osSyncPrintf`+`rmonPrintf`: near-verbatim VERSION_J mirror — drop `__osSyncVPrintf` (VERSION_K+ only; cross-ref `~/development/repos/ultralib/src/libc/syncprintf.c` for VERSION_J layout); `-D_FINALROM` gates both bodies to empty MIPS O32 vararg stubs (save $a0–$a3 + jr $ra) → ROM match. `include/stdarg.h` from ultralib GCC headers; `-nostdinc` removed (ROM SHA-1 still green). Both 0 iterations. seed 10 / banked 10pt. Retro: **2 of 2 applied** (#1 pick_target.py INCLUDE_DIRS comment; #2 CLAUDE.md ultralib VERSION_J cross-reference). No carry-overs.

- **Sprint 31: 2 files BANKED — `src/main/func_800A2F50.c` (`func_800A2F50`) + `src/libnusys/mainlib/nugfxinit.c` (`nuGfxInit`); first pure classical sprint; reaches 56/56 md5-candidate (ALL FILES).** md5-candidate 54→56. `func_800A2F50`: 16 B trivial getter returning `D_800C8234`; score 0 first pass. `nuGfxInit`: 176 B, 6 jals, jal-count-mismatch 13vs6 → classical; v2.00 SDK (`~/n64sdk/4.0/pc/basic/nusys/`) template; game-specific drops (nuGfxSetCfb/nuGfxSetZBuffer/nuGfxSetUcode absent); GBI macros (`gSPDisplayList/gDPFullSync/gSPEndDisplayList`) + `Gfx gfxList[0x100]+Gfx *gfxList_ptr` locals force 0x820 frame; `D_B6698` absolute-physical-address linker symbol (`undefined_syms_auto.txt`) referenced as `(u32)&D_B6698`; `#undef nuGfxInit` needed for nusys.h macro conflict; score 0. All green SHA-1. seed 8 / banked 8pt / realized 10pt (regime classical; first positive residual sprint). Retro: **1 of 2 applied** (#1 CLAUDE.md split-subseg cmp note; #2 libnusys classical v2.00 pattern deferred). No carry-overs.

- **Sprint 30: 3 files BANKED — `src/libkmc/strcmp.c` (`strcmp`) + `src/libultra/monegi/time/settimer.c` (`osSetTimer`) + `src/libultra/monegi/thread/dequeuethread.c` (`__osDequeueThread`); first mixed sprint.** md5-candidate 51→54. `strcmp`: libkmc warm verbatim cp (single-fn subseg 0x8EAD0; `_kmclib.h`+`memory.h` already in-tree; libkmc `-O` auto-applied; 0 iter). `osSetTimer`: **classical** — `jal-count-mismatch:5vs2` hazard at DoR initially suggested near-verbatim, but asm showed fundamentally stripped impl (no interrupt disable/restore/counter update); one recover-extern `__osTimerList`=0x800C8240 (OSTimer*→size:0x4, from `lui 0x800d`/`lw -0x7dc0`); score 0 first pass; `make extract` re-run after symbol add. `__osDequeueThread`: classical — 5 file-scope defs from thread.c dropped (defines-data hazard, classical fallback); 64 B pointer-walk loop; register params; `(OSThread*)queue` head cast; score 0 first pass. All 3 green SHA-1 at every commit. seed 6 / banked 6pt (regime mixed: 1 mirror + 2 classical). Retro: **1 of 1 applied** (#1 CLAUDE.md gate note: large jal-count-mismatch >2 is `classical-likely`). No carry-overs.

- **Sprint 29: 2 files BANKED — `src/libnusys/mainlib/nupireadwritesram.c`
  (`nuPiReadWriteSram`) + `src/libkmc/_matherr.c` (`__matherr`), libnusys+libkmc mirrors;
  opens the nuPi SRAM sub-band.** md5-candidate 49→51. `nuPiReadWriteSram`: cold libnusys
  recover-extern mirror with a one-time Makefile enabler — entire body gated by `#ifdef
  USE_EPI`; added `LIBNUSYS_CFLAGS := $(CFLAGS) -DUSE_EPI` + libnusys pattern rule (modeled
  on LIBKMC_CFLAGS); recover-extern `nuPiSramHandle`=0x8012F4D8 (OSPiHandle*, size:0x4).
  `__matherr`: pack-split at 0x8EBE0 (168 B subseg) → C portion 112 B (`_matherr.c`,
  16-aligned) + hasm 56 B (`__muldi3`, permanent hasm per CLAUDE.md); recover-extern
  `errno`=0x800FE3D0 (int, size:0x4); libkmc `-O` profile auto-applied. Both verbatim cp,
  0 iterations; all names pre-curated; no header copies. seed 6 / banked 6pt. Retro: **1 of
  1 applied** (#1 `needs-define` hazard in pick_target.py — detects a top-level `#ifdef
  DEFINE` body gate absent from effective library CFLAGS). No carry-overs.

- **Sprint 28: 3 files BANKED — `src/libnusys/mainlib/nucontgbpakreadwrite.c` (`nuContGBPakReadWrite`) + `nucontgbpakcheck.c` (`nuContGBPakCheckConnector`) + `src/libkmc/memset.c` (`memset`+`setmem`), libnusys+libkmc mirrors; opens `libkmc/` memset/setmem.** md5-candidate 46→49. Pack split at 0x7D710/0x7D760 for the libnusys pair; whole-file flip at 0x8E550 for libkmc. `nuContGBPakReadWrite`: verbatim cp — `#ifdef NU_DEBUG` block compiles out (ROM build doesn't define NU_DEBUG). `nuContGBPakCheckConnector`: verbatim cp, trivial. `memset`+`setmem`: verbatim cp + companion `include/libkmc/memory.h` copied from upstream; libkmc `-O` profile auto-applied. No new symbol_addrs.txt additions; all names pre-curated. All first-pass clean (0 iter). seed 6 / banked 6pt. Retro: **1 of 1 applied** (#1 fix `pick_target.py` jal-count-mismatch — `NU_DEBUG` stripping + string literal masking). No carry-overs.

- **Sprint 27: 3 files BANKED — `src/libultra/nintendo/exception/setglobalintmask.c` (`__osSetGlobalIntMask`) + `resetglobalintmask.c` (`__osResetGlobalIntMask`) + `src/libultra/monegi/time/gettime.c` (`osGetTime`), libultra recover-extern mirrors; opens `nintendo/exception/` and `monegi/time/` bands.** md5-candidate 43→46. Pack split at 0x8B9D0 for the `setglobalintmask`/`resetglobalintmask` pair; single yaml flip for `osGetTime`. Shared recover-extern `__OSGlobalIntMask`=0x800C9470 (inlined, size:0x4) for the `exception/` pair; `__osBaseCounter`=0x800FBE04 (size:0x4) + `__osCurrentTime`=0x801052F0 (size:0x8 OSTime) for `gettime.c`, recovered from the fn's own asm. `__osViDevMgr` dead-`#ifdef _DEBUG` over-flag confirmed (no symbol add). All first-pass clean (0 iter). seed 8 / banked 8pt. Retro: **0 of 0** (suggestion buffer "None new"). No carry-overs.

- **Sprint 26: 2 files BANKED — `src/libnusys/mainlib/nucontrmbcheck.c` (`nuContRmbCheck`) +
  `nucontqueryread.c` (`nuContQueryRead`), libnusys heterogeneous pair (jal-divergence/drop +
  pack-split).** md5-candidate 41→43. **`nuContRmbCheck`**: near-verbatim/drop — upstream's
  `osSetIntMask` critical section is absent from this ROM build (3 upstream jals → 1 ROM jal);
  dropped `mask`/`osSetIntMask`×2, verbatim cp of remainder; disassembly confirmed 1 jal
  (`nuSiSendMesg`). **`nuContQueryRead`**: verbatim pack-split — trivial 1-jal fn split out of
  the 0x7E330 pack at rom 0x7E350; unnamed 16B sibling `func_800A2F50` stays asm. Both names
  pre-curated in ghidra_symbols, zero new symbol adds, zero header copies. 0 iterations each.
  seed 5 / banked 5pt. Retro: **0 of 0** (buffer "None new"). No carry-overs. **PO directive:
  target ≥5pt per sprint going forward** — batch 2+ files per sprint consistently.

- **Sprint 25: 1 file BANKED — `src/libnusys/mainlib/nucontdatalock.c` (`nuContDataLock` +
  `nuContDataUnLock`), libnusys recover-extern mirror.** md5-candidate 40→41. **Whole-file
  pack** — subseg 0x7E2D0 held exactly the two fns of one upstream file, so a single cohesive
  flip banked both with **no split and no orphan asm**. One recover-extern
  `nuContDataLockKey`=0x800FED38, the simplest S20 sub-case (plain scalar word, size:0x4, no
  index-multiply → inlined `refs-unplaced` vram needed no base-offset fix; re-confirmed via the
  fn's own `lui 0x8010`/`sw -0x12c8`). Both fn names pre-curated, `osSetIntMask` pre-placed,
  lock macros in `nusys.h` → one symbol add + one yaml flip, verbatim cp, 0 iterations. Two
  `jal` both = osSetIntMask, reconciled clean (no jal-count flag). seed 5 / banked 5pt. Retro:
  **0 of 0** (suggestion buffer "None new"). No carry-overs.

> **ARCHIVED (S148) — the mirror-era PO ordering notes below (S94…S11) are superseded by Epic 2.**
> They guided target selection while the libultra/libnusys/audio MIRROR bands were open; those bands
> are now fully banked (Epic 1 DONE). Kept for provenance only. Epic 2 ordering is plain
> smallest-first classical off `tools/pick_target.py` (no special mirror-band survey).

## PO ordering note (S94 retro — the audio sub-band is partially open; survey by the coddog column)

S94 banked the first audio mirror (`auxbus.c`); S95 banked the 2nd (`load.c`, 4fn). Live ordering
facts for the next gate:
- **The audio header `-I` enabler is already paid** (heapinit/heapalloc/copy were flipped pre-S94;
  `libaudio.h`/`synthInternals.h`/`abi.h` are vendored in-tree). So the S71-deferred audio unlock
  lever does NOT need re-paying — audio coddog mirrors are pickable now, gated only by their own
  per-file hazards.
- **`--lib libultra` now surfaces the audio coddog rows** (S94 #1 fix — `--lib <scope>` matches a
  coddog-mirror row whose matched source is in-scope; `--lib audio` works as a sub-path scope too).
  Before S94 these classified `upstream none` (un-named, header-gated → not re-priced) and the
  scoped filter hid them; the S55 "survey by the upstream/coddog column" workaround is no longer
  required for audio.
- **Next-cleanest audio leaves + their hazards (S96 refresh).** auxbus (S94) + load (S95) + drvrnew
  (S96) banked. ~~`drvrnew.c` (`func_800A3C80`, 8fn, needs-header + refs/calls-unplaced +
  rodata-jtbl)~~ **BANKED S96** — the audio synth DRIVER, first non-exemption audio mirror: vendored
  initfx.h+stdio.h, dual `.data`+`.rodata` carve, 18 syms (8 + 10 cross-file callees). `__FILE__/
  __LINE__` was FALSE (`_DEBUG` off → `alHeapDBAlloc(0,0,…)`; S96 #2 dropped builtins from
  refs_unplaced). The rest, NOW CHEAPER (drvrnew pre-named their entry points + vendored the shared
  initfx.h/stdio.h): `reverb.c` (`alFxPull`=0x800A61C0, 8fn — `_init_lpfilter` now placed (drvrnew),
  defines-data:val,blob + refs:alGlobals + calls-unplaced:SWAP,_init_lpfilter + rodata-jtbl);
  `env.c`/`alEnvmixerPull` (8fn, calls-unplaced:_frexpf,_ldexpf float intrinsics + rodata-jtbl + 13
  rodata-literal + jal-mismatch — verify the intrinsic callees are placed first); `mainbus.c`
  (`alMainBusPull`=0x800A5DA0, 4fn, rodata-jtbl:0x800D23E8 + twin-of:drvrnew + coddog-fncount 2vs4 →
  multi-file/jtbl); `save.c` (`alSavePull`=0x800A6D60, 6fn, coddog-twin save!=sl + fncount 2vs6 →
  multi-file). The synth-cluster single-file packs (reverb/env) are mirrorable with their own
  rodata-jtbl carve (headers + cluster entry points already paid by S96); mainbus/save need the
  multi-file coddog-boundary split first. **NOTE (S95):** the phantom `lastCnt`/`save_min`/
  `rate_min`/`vol_min` AUD_PROFILE externs are now de-noised off these rows — their remaining
  refs-unplaced flags (alGlobals etc.) are the real ones.
- **Open tooling question (deferred S94 #3):** audio coddog hits are still priced as classical packs
  (seed 13) because they aren't re-priced to `libultra` (the S71 header-gate carve-out, now stale).
  Re-pricing would drop their seeds to mirror values but needs per-row vendorable-header FP analysis
  + a golden regen — a candidate enabler-sprint item, not yet done.

## PO ordering note (S71 retro — coddog: the remaining libultra band is ~all verbatim-mirrorable)

The S71 coddog sweep (`make coddog-sweep`, 223 matches ≥95% MG64 vs ultralib-J) reclassifies the
"classical" libultra backlog. Live ordering facts for the next gate:
- **Most "classical/jal-mismatch/none"-flagged libultra leaves are verbatim mirrors.** Their
  "missing" calls are *separate sibling fns in the same ultralib `.c`*, not inlined. `__osContRamRead`
  /`__osContRamWrite` (now CRC-unlocked by S71), the cont/pfs band (`contpfs.c` 7fn, `controller.c`,
  `contquery.c`, `contreaddata.c`, `pfsgetstatus.c`, `pfsisplug.c`, `pfsselectbank.c`, `motor.c`,
  `piacs.c`), `devmgr.c`, `sched.c` (10fn), `timerintr.c`, `sptask*`, `vimgr.c`, `epirawread.c`,
  `sirawdma.c` — all match 99.99%. `pick_target` now flags these `coddog-mirror:<file>@<pct>` and
  re-prices ≥99% non-audio hits as `libultra` mirrors (run `make coddog-sweep` first; map is
  gitignored). Smallest-first off the re-priced list is the path; treat each as
  `docs/hazards.md#upstream-mirror-pattern`.
- **The classical/v2-residual track is owed almost nothing in libultra.** The only genuine
  divergence (coddog <99%) is the libc xprintf band: `_Printf` 95.7%, `_Putfld` 96.1%, `_Litob`
  97.3%, `_Ldtob` 95.8% — these are the real classical leaves if the v2 realized tier is wanted.
- **Audio (~40 fns, 14 files, all ≥98%) is a deep clean mirror vein gated only by a one-time
  audio-header/`-I` enabler** (the deferred lever, same shape as the S15 libnusys unlock). coddog
  flags audio `coddog-mirror` advisory-only (not re-priced — the header enabler isn't modeled). A PO
  call when the io/os mirror band thins.
- **Retire the "classical owed since S11" carry:** it predates the coddog sweep and is now moot for
  libultra (mirror, not classical, is the remaining work).

## PO ordering note (S26 retro — target ≥5pt per sprint; batch 2+ files consistently)

PO directive: **commit at least 5pt per sprint going forward**. The mirror band now consistently
yields 2–3pt leaves, so hitting 5pt means batching ≥2 files per sprint. Practical implications:
- At the next gate, default to a 2-file minimum (heterogeneous pairs count — S26 proved they
  work first-pass). Fill the 3–4 fn cap when the batch is homogeneous.
- For the remaining pickable libnusys/libultra leaves, the easiest 5pt combos are: two
  jal-mismatch or two recover-extern leaves (pts 2+3 or 3+3), or one recover-extern plus one
  pack-split (2+3 or 3+3). `osGetTime` (pts 3, 3 recover-extern) + `__matherr` (pts 3, pack +
  non16align + recover-extern) would combine at pts 6, but the `__matherr` non16align hazard
  needs investigation at the gate (the non16align means we need to split away the hasm `__muldi3`
  at the correct non-16-aligned boundary before flipping). The simpler path is `osGetTime` (pts
  3) + the `nuContGBPakReadWrite` split (pts 3) = 6pt, or any two ≥2-pt leaves.

## PO ordering note (S16 retro — the false-clean class is closed; deeper nusys leaves now priced)

The S16 grep fix re-priced the nuGfx*FuncSet leaves (now pts-3 recover-extern, was false-clean
pts-2). Three live ordering facts for the next gate:
- **The cheapest remaining clean nusys leaves are the `nuContGBPak*` band** (`nuContInit`,
  `nuContGBPakGetStatus`/`Power`/`ReadID`, all pts-2, no hazard after the S16 fix → genuinely
  clean). Sibling-batch 2–3 of them next — same warm-band zero-enabler pattern as S16, but now
  with the false-clean risk eliminated (pick_target's no-hazard is trustworthy again for non-`__`
  globals).
- **The `nuGfx*FuncSet` trio (`nuPreNMIFuncSet`, `nuGfxFuncSet`, `nuGfxSwapCfbFuncSet`) are
  pts-3 recover-extern mirrors** with their vrams already inlined by pick_target — a cheap
  follow-on batch (one symbol add each, then verbatim cp).
- **The classical / v2-residual track is still owed a non-trivial leaf** (carried S11/S13/S15).
  Mirror remains the low-risk default, but v2's realized tier needs a `maybe-upstream`-cleared
  classical leaf to grow.

## PO ordering note (S15 retro — the libnusys band is open; sibling-batch it; #3 carried)

The S15 enabler is paid once. Three live ordering facts for the next gate:
- **The nuGfx*/nuCont* band is now a deep pool of zero-enabler cold mirrors** (S15 retro #3,
  guidance-only). `pick_target` surfaces them pickable at pts 2: `nuGfxTaskAllEndWait` (0x7CA00,
  32B), `nuGfxDisplayOff` (0x7CAA0, 48B), `nuPreNMIFuncSet`, `nuGfxFuncSet`, `nuGfxSwapCfbFuncSet`,
  `nuContInit`, … all single-fn, no hazard. **Next sprint: sibling-batch 2 of them** (the S4/S5
  zero-enabler pair pattern) now that the `-I`/header enabler is amortized.
- **Watch for nusys multi-fn packs.** `nuContQueryRead` (0x7E330) is a `pack:2fn` with an
  un-named sibling (`func_800A2F50`) — needs a subseg split at the upstream-file boundary, like
  the S10 dp pack. Prefer the clean single-fn leaves first.
- **The classical / v2-residual track is still owed a non-trivial leaf** (carried from S11/S12/
  S13 notes). The mirror band reopening (libnusys) is the lower-risk default, but v2's residual
  signal still needs a genuine `maybe-upstream`-cleared classical leaf to grow the realized tier.

## PO ordering note (S13 retro — the mirror band is far bigger than `none` showed; 2 facts)

The S13 tooling fixes reclassified a large slice of the "classical" backlog as **un-named SDK
mirrors**. Two live ordering facts for the next gate:
- **The nusys / audio / mus mirror band is now visible but include-blocked.** `pick_target`
  surfaces nuGfx*/nuCont*/audio leaves as `libnusys`/`libnaudio`/`libmus` mirrors, but they
  carry `needs-header:nusys.h` (blk) — the nusys/audio include paths aren't in the project `-I`
  set. **Enabler to unlock the whole band:** add the nusys (+ naudio/mus) include dir(s) to
  CFLAGS / copy the companion headers, then these become cheap cold mirrors. This is the new
  highest-throughput lever — a PO call (a Makefile `-I` enabler, deferred like the audio
  `<libaudio.h>` path).
- **A genuine classical leaf for v2 must clear the `maybe-upstream` filter.** The v2-residual
  hunt can no longer trust `upstream:none`. Pick a candidate whose `maybe-upstream` hazard is
  **absent or refuted at the gate** (asm-vs-upstream-checked) — that is the only trustworthy
  classical signal now. The smallest such genuinely-game leaf grows the realized tier.

## PO ordering note (S12 retro — recover-extern is the mirror floor; #2/#3 carried)

S12 banked the cheapest *remaining* mirror (recover-one-extern). Two carried ordering facts
for the next gate (S12 retro #2/#3, considered-but-not-applied — guidance, not file edits):
- **Re-price `__osDequeueThread` next gate (carried #2).** It is the last thread-band leaf but
  carries `defines-data` (it *re-defines* `__osThreadTail` + 4 placed siblings) → routes to the
  classical loop with the data-defs dropped, NOT a clean verbatim cp. Decide at the gate whether
  the drop links cleanly or hits the BSS-layout-conflict wall before committing it.
- **Favor classical non-trivial leaves the next 1–2 sprints (carried #3).** The mirror track is
  a confirmed point mass (10 straight clean/near-clean: S1–S8, S10, S12); v2's residual signal
  lives only on the classical track (S9, S11, both residual 0). Pull non-trivial small classical
  leaves to grow the realized tier and watch for the first **non-zero residual**; keep a
  recover-extern mirror (`osEPiLinkHandle`@`__osPiTable`, `osGetTime`, `osSetThreadPri` warm-1
  clean) in reserve as a low-risk filler. `pick_target.py` now surfaces the recovered vram inline
  (`refs-unplaced:name@0xADDR`) for the single-extern cases, so those flips are gate-cheap.

## PO ordering note (S11 retro — v2 active; mirror warm pool mined out)

**v2 is ACTIVE** (since the S11 review). The realized-tier/residual machinery now runs on the
**classical track**; the mirror track stays seed-only (still a point mass). The classical loop
is proven both mechanically (S9) and with real variance (S11). Two live ordering facts for the
next gate:
- **The *libultra* warm clean-singleton mirror pool is mined out — but the libnusys band is
  not (UPDATED S21).** At the S11 gate every top *libultra* mirror carried a *blocking* hazard:
  `needs-header` (audio band, `guRandom`, `sprintf`), `file-static` (`sprintf`, `osSpTaskLoad`),
  `defines-data` (`__osDequeueThread`), or a `refs-unplaced` data extern needing asm-data-recovery
  (`osYieldThread`/`__osRunQueue`@0x800C8228 — recovered at the S11 gate, `osGetTime`,
  `osEPiLinkHandle`). **This note predates the S15 libnusys-band unlock.** The libnusys mirror band
  (nuGfx*/nuCont*) still yields **zero-enabler clean cp's** — S21 banked `nuGfxRetraceWait` with
  one yaml flip and no symbol add / header copy / known-edit (pick reported `-` no-hazard and was
  right). So don't assume the cheapest remaining mirror is always a recover-extern or known-edit
  flip: re-check `pick_target.py`'s hazard column each gate — a `-` libnusys leaf in a warm band
  is a true zero-enabler `cp`. The *libultra* recover-extern fillers (above) remain valid options.
- **Classical is now a first-class option, not just a spike.** With v2 calibrating, continue
  pulling **non-trivial** small classical leaves (real arithmetic/branches/locals; the
  `intrinsic-likely` hazard filters register/FPU shims) to grow the realized-tier signal and
  watch for the first **non-zero residual** (a stuck-far / permuter / re-attempt) — that is the
  data v2's residual loop actually needs. `osYieldThread` (recovered extern ready) remains the
  cheapest mirror fallback when a low-risk increment is wanted.

## Enabler items (gate-time, agent-performed since 2026-06-11)

These are the gate-time enabler actions a sprint may need before its execution middle can run
flip-free. `/sprint-plan` lists the ones a proposed sprint requires; since 2026-06-11 the
**agent** performs them at the plan gate (subseg flip/split, `symbol_addrs.txt` add) after PO
scope approval, and the gate validates with `make extract && make` (green ROM). (Pre-2026-06-11
these were USER actions performed by the PO.)

- [ ] _(none queued — `/sprint-plan` fills this per sprint)_

## Carry-overs (files/clusters awaiting the next sprint)

Two kinds, both de-ranked by `tools/pick_target.py` (so they stop resurfacing) and re-pulled first
by `/sprint-plan`:
- **Spike** — a function that BLOCKED its file's DoD (locked < 0.97 percent, needs permuter,
  BSS-layout / subseg-alignment conflict). The note records the blocker so the retry resolves it first.
  **For a defines-data spike, default the framing to drop-def** (extern the file's data globals; the
  bytes come from the existing extracted blob, usually `main_data`) — NOT a `.data`/`.bss` sibling
  carve. A carve is needed only when the data lives in the file's OWN extracted region; placed-sibling
  data (already resolved from `main_data`) is not, so drop-def is the S82/S83/S85 default and a carve
  the exception (S38/S48/S68). Bind every cited data address to an **asm-recovered** value, never a
  guess (S85: `initialize.c`'s carry-over framed a carve that was really drop-def, AND `pimgr`'s
  `__Dom*SpeedParam` addresses were wrong — both corrected by reading the asm at the gate).
  **For an io/os file-static MIRROR spike, run the drop-static test AT CARRY-OVER-WRITE TIME and
  frame the EXPECTED resolution** — uninitialized statics → drop-to-extern at recovered `main_bss`
  vrams (S81 siacs / S87 vimgr); `.data` globals that are already-placed → drop-def; reserve "carve"
  only for a NON-placed nonzero-init global living in the file's OWN extracted region. Every io
  file-static mirror that has come due was a clean drop-def/drop-to-extern, ZERO carves (vimgr S87,
  pimgr S90), yet both were first framed as a "mixed `.data`/`.bss` carve" spike and banked
  first-build seed-only once the test was applied — so frame the verdict, not the worst case.
  **Try every UNTRIED mechanism before declaring a dead-end CLASS (S107).** A spike note that proves N
  paths fail but LISTS an untried option must not generalize "both/all dead-ends proven" to the whole
  class — the listed-but-untried option may be the answer. S107 exceptasm: S91 proved strip-and-rename
  + carve fail and listed "export the `.text` labels" as untried; that 3rd option banked first-try and
  retired the 16-sprint spike. Enumerate the untried mechanisms explicitly in the carry-over and gate
  the "spike" verdict on all of them being tried, not on the hard-looking ones.
- **Near-free retry** — NOT blocked; a fully-scoped increment deferred only by the sprint cap (e.g. a
  coddog-mirror sibling, or the un-flipped head of a split subseg). Author it as a **completeness
  checklist** so the retry is a mechanical replay (S74→S75 `contquery` proof: all 4 addresses
  verbatim-correct, 0 rework): **(1)** the exact subseg flip/split line; **(2)** the placed-ref
  inventory (callees + data externs already resolved, with the sprint that placed them); **(3)** any
  NEW recover-extern / callee vrams to add, each WITH its confirmed address; **(4)** the include
  adaptation vs upstream; **(5)** the upstream pin (file + VERSION_J); **(6)** for a libnusys/version-
  pinned mirror, a **header-value reconciliation** vs the game's library rev (thread IDs, mesg-max,
  stack sizes, and other plain `#define` VALUEs the body emits as `li`/`addiu` immediates) — the
  vendored upstream version can diverge from the game's rev on a single immediate (S122 nusys-2.07
  `NU_CONT_THREAD_ID=6` vs MG64's 5), and that surfaces only at first build unless reconciled here.
  A near-free retry missing any of these is a half-scoped spike — finish the scope before deferring.

- **(S209 MIXED-PARTIAL — carried; 32 of 59 banked)** `src/main/func_80059BA0.c` (main-segment
  `[0x34FA0]` integer-glue/accessor pack). 32 banked (S208 +23 asm-first; S209 +9 via the PO-directed
  compiler-source dive), 27 stubs remain, ROM green off extracted asm. **S209 CLOSED all 4 S208
  near-matches byte-exact with 0 permuter** — `func_8005B070` (while-loop; the `.set-reorder`
  textual≠machine nop, not a schedule wall), `func_8005D218`/`func_8005D2E4` (shared `case 0: default:`
  merged-default denies jump2 cross-jump inversion), `func_8005D308` (nested-if with the skipped store
  in the ELSE → reorg `optimize_skip` annulled bnel) — AND the full **C458/C4B4/C5B4/C614 grid-counter
  family** + `func_8005B28C` (word-aligned struct block-move). C458 WALLED the permuter (1.27M
  iters/score 55) then fell to the dive (global.c allocno live-length steer via an intermediate copy +
  integer-cast commutative operand order). **1 carry: `func_8005D334`** (triple-IV struct-array init —
  GCC merges the outer counter into the byte-offset IV vs the ROM's 3 separate IVs + an up-pointer inner
  loop; a loop.c strength-reduction divergence, characterized, oracle-close). ~23 larger/FP fns still
  unprofiled. See the S209 hazards `#grid-counter-double-loop`, `#compiler-source-fan-out`
  (walled-permuter case), `#local-alloc-qty-permutation` (global.c analog + no-scheduler fact), and
  memory `kmc-cc1-no-instruction-scheduler`.

- **(S206 MIXED-PARTIAL — carried; 3 of 6 banked)** `src/main/func_800772B0.c` (main-segment
  `[0x526B0]` float spline/curve-interpolation pack; NOT a settime.c mirror — false coddog collision).
  Subseg `[0x526B0, c, main/func_800772B0]` flipped; banked `func_800772B0`/`func_80077BD8`/
  `func_800779A8`; 3 stubs remain, ROM green off extracted asm. **All 3 carries are fully-RE'd S158
  `#pervasive-regalloc-classical-main` FP walls** (`docs/wip/<fn>.near-match.md` each; base.c in
  `nonmatchings/<fn>/`), all < 0.97 so permuter N/A:
  - `func_800772C4` (0x800772C4, spline-segment stepper, ~294 instr, switch-dispatch, 0.53). Wall =
    ROM homes the buffer base in `$t0` (`addu t0,a0,zero`) + address-CSEs 2 scratch globals
    (`&D_800E1D30->$a1`, `&D_800E1D20->$v1`), flipping the post-reload load scheduler; local-ptr copy
    coalesces away. Semantics 100% (control flow + 9 cases + FP math all match).
  - `func_8007775C` (0x8007775C, cyclic cubic-spline solver Thomas+Sherman-Morrison, ~158 instr, 0.73).
    Wall = coupled global-vs-local register coloring (loop3 `slti` is a required local sub-optimum for
    the good loop5/6 `$a0/$a1` coloring; a `d+d` accumulator lands `$f0` vs target `$f2`). All 7 loops
    structurally correct.
  - `func_80077AD4` (0x80077AD4, cubic finite-diff interp, ~70 instr, 0.62, 65/66 mnemonic rows). Wall =
    int-temp hard-register permutation (ia0->$a2/ia1->$a0/d0->$a1/d1->$a3 in ROM) + an `ia0*3` hoist into
    the truncation phase. Needs the `f32 v[3]` stack-array form for the dead-frame stores.
  Retry: a from-scratch permuter (once seeded past the reg fold) or `#cross-project-matched-corpus-mining`
  for the KMC-2.7.2 allocno idiom. Do NOT use explicit register allocation.

- **(S199 MIXED-PARTIAL — carried; 0 of 2 banked)** `src/main/func_80050710.c` (main-segment
  `[0x2BB10]` ROM-load helper tail). Subseg `[0x2BB10, c, main/func_80050710]` flipped; both functions
  remain `INCLUDE_ASM`, ROM green off extracted asm. **Carries:** `func_80050710` (structural-complete
  near-match, 143/143 rows; residual saved-register color swap `s1`/`s2`) and `func_80050914`
  (structural near-match, 200/202 rows; residual saved-register order plus one branch-likely detail in
  the two-byte RLE path). Compiler-source fan-out found the address-taken-local stack-counter lever
  (`mark_addressable` -> `put_var_into_stack`) but no faithful final-coloring lever. Do not use explicit
  register allocation; retry only with a new source-shape/compiler-codegen insight.

- **(S196 MIXED-PARTIAL — carried; 5 of 6 banked)** `src/main/func_80050400.c` (main-segment
  `[0x2B800]` ROM-load slot pack). Subseg `[0x2B800, c, main/func_80050400]` flipped; tail
  `[0x2B9A0, asm]` holds the rest of the original pack. Banked `func_80050400`, `func_800504E8`,
  `func_80050504`, `func_80050588`, and `func_80050598`; one stub remains, ROM green off extracted
  asm. **Carry:** `func_80050428` (0x80050428, ROM table read/setup via `nuPiReadRom`). Correct stack
  layout found (`0x40` scratch buffers with `&buf[0xF]` alignment), but unconstrained C rotates saved
  registers and remains far below permuter threshold. Do not use explicit register allocation; retry
  only with a new source-shape/codegen insight.

- **(S191 MIXED-PARTIAL — carried; 3 of 11 banked)** `src/main/get_table_entry.c` (main-segment
  `[0xE260]` meter/ball/sound logic pack). Subseg `[0xE260, c, main/get_table_entry]` flipped; 3
  standalone fns banked C (`get_table_entry`/`func_80032E88`/`func_80037E50`); 8 stubs remain, ROM
  green off the extracted asm. **All 8 carries are FP-wall parents or their nested children** — bank
  each nested child FREE inside its parent's C TU (child emits before parent, one compilation unit,
  `#nested-function-static-chain-spill`), so the retry is an FP-wall-parent sprint, not per-fn.
  - `update_ball_physics` (0x8003327C, ~19KB, 21-FP) — main ball-physics FP wall (S158 class).
    **Parent TU** of the 5 nested children below (all sit at 0x80032F70-0x80033228, just ahead of it):
    `play_sound_at_meter_ratio_gameplay` (0x80032F70, uses `$v0`-chain, reads `BallObject` via +0x40),
    `update_rotation_matrix` (0x80032FE8, FP, `$v0`-chain), `play_sound_at_meter_ratio` (0x800330D0,
    dead-`$v0`-spill leaf), `play_sound_for_meter_phase` (0x80033134, dead-`$v0`-spill, phase beq-chain),
    `read_meter_stick_normalized` (0x80033228, `$v0`-chain, controller-axis normalize `1/127.5`).
  - `init_ball_for_shot` (0x80037F94, ~3.8KB, 21-FP + sprintf debug HUD) — shot-init FP wall (S158 class).
    **Parent TU** of `calc_stick_offset_with_noise` (0x80037EE0, nested child; the parent sets
    `$v0=sp+0x10` before its `jal 0x80037ee0`).
  - **Retry:** open one of the two FP-wall parents as its own increment (the nested children come free
    with a banked parent). Until then the ranker should DE-RANK this partial file. No cross-repo name
    sync (all `func_`). pick_target follow-up: emit `nested-child:<parent>` from the callee-side tell.
- **(S190 SPIKE — carried; 0 of 3 banked)** `src/main/func_8004E5A0.c` (main-segment `[0x299A0]`
  3-fn one-tu **debug/HUD display-list renderer**). Subseg `[0x299A0, c, main/func_8004E5A0]` flipped;
  all 3 fns auto `func_`, no `symbol_addrs` adds. ROM green off the extracted asm (0 banked). This is
  the S189 FP/DL-wall class taken to its limit: EVERY fn is a DL/FP wall, so partial-bank-expected-ZERO.
  - `func_8004E5A0` (0x8004E5A0, 225i, 0-jal 0-FP) — **DL font-glyph blitter; `#display-lists` header
    constant-staging scheduling wall.** FULLY reconstructed to a FAITHFUL structural near-match (225/233
    rows): dynamic `gDP*` macros header (gfxdis: PipeSync/RenderMode XLU/PrimColor(FF,C8,00,FF)/
    CombineMode MODULATEIA/LoadTextureBlock_4b IA 64×8 font @0x800C0D10) + per-char TEXRECT/RDPHALF loop,
    needs `-DF3DEX_GBI_2` (macro words match ROM exactly). Blocker = GCC whole-header scheduling: ROM
    front-stages ~11 constants to scattered stack slots + precomputes all ~14 command addresses then
    stores; faithful `gDPxxx(gfx++)` C keeps constants in regs (frame −0x58 vs ROM −0x88, 204 vs 227
    instrs). NOT `#mem-in-struct` (no global load). Permuter `--main --best-only` PLATEAUED 10065→6235
    over 8000+ iters, no match. Reconstructed C in `nonmatchings/func_8004E5A0/base.c`. **Retry:** a
    dedicated permuter (without `--best-only`, S160 plateau doctrine) OR `#cross-project-matched-corpus-mining`
    for the exact header schedule — but this is the new `#display-lists` header-scheduling carry class.
  - `func_8004FDB4` (0x8004FDB4, 203i, 10-jal 0-FP) — **DL HUD frame renderer, same class (NOT
    reconstructed).** Debug heap-stats HUD: static-DL branch (0xDE000000 → `D_C0C40`) + SETCIMG cfb
    (`osVirtualToPhysical(nuGfxCfb_ptr) & ~7`) + identical gDP-macro header + fillrect + sprintf
    (`nuGfxCfbNum`/`heap3_get_largest_free`/`heap3_get_total`)×3 through `func_8004E5A0` + RDPFullSync/
    EndDL + `nuGfxTaskStart`. Same header-scheduling wall as E5A0 + more complexity → carried on
    shared-idiom basis. **Retry:** reconstruct + score only after E5A0's schedule wall is cracked (the
    header idiom is shared).
  - `func_8004E924` (0x8004E924, 1316i, 25-jal 21-FP) — 21-FP scene renderer, S158 FP class (NOT attempted).
  **Retry:** a dedicated permuter/`#cross-project-matched-corpus-mining` sprint on the E5A0 header
  schedule, OR skip this TU entirely until the ranker de-ranks DL-emitter packs (the pts follow-up
  below). No cross-repo name sync (all `func_`).

- **(S189 MIXED-PARTIAL — carried; 2 of 8 banked)** `src/main/func_800660A0.c` (main-segment
  `[0x414A0]` course-decal/aim-target/rumble rendering pack). Subseg `[0x414A0, c, main/func_800660A0]`
  flipped; `draw_course_decal_triangles`/`emit_aim_target_ring`/`update_rumble_intensity_table` curated in
  `ghidra_symbols`, other 5 auto `func_`; no `symbol_addrs` adds. S189 banked 2 (`func_800660A0` flag set +
  `func_800676B0` record-field init). **6 CARRIED** (no rodata carve — shared TU rodata referenced extern;
  ROM green off the 2 banked). Two regalloc near-matches (retry via permuter/`#cross-project-matched-corpus-mining`):
  - `func_80067A60` (0x80067A60, 40i) — **STRUCTURAL-COMPLETE near-match, build MORE optimal than ROM.**
    Structure matched (early mask, dup addr calc per branch, signed `lh`, per-branch `arg1+=4`, shared
    `jal` + arg4-store in the jal delay). Residual +3 = the ROM has a REDUNDANT mask save/restore
    (`move t1,a3`/`move a3,t1` — `a3` is never clobbered) + does NOT `%lo`-fold the else-branch `lh`; the
    build reproduces neither (it is tighter). "mine more optimal" wall — not source-reachable; needs the
    permuter (to find the less-optimal form) or cross-corpus. wip `docs/wip/func_800660A0.func_80067A60.near-match.c.txt`.
  - `update_rumble_intensity_table` (0x800679DC, 33i) — **STRUCTURAL-COMPLETE near-match, 3-reg rotation.**
    Loop FORM fully solved: goto-outer for the `#top-tested-loop` reversal corollary (up-count + plain
    `bnez` + delay-slot recompute) + signed pointer compares (`(s32)p < (s32)(row+n)` → `slt` not `sltu`).
    Residual = `i`/`row+n`/`row+3` land in a1/a3/a2 vs ROM a3/a2/a1 (`#loop-weight`: `i` is the most-global
    allocno so GCC pins it low a1; decl-order lever no-op). Permuter `--main` plateaus at 45 (base 55) over
    500+ iters, never 0 → genuine regalloc wall. wip `docs/wip/func_800660A0.update_rumble.near-match.c.txt`.
  Four FP/DL walls (NOT attempted; `#pervasive-regalloc`/S158/`#display-lists`):
  - `func_800674B8` (0x800674B8, 126i) — FP loop (32-iter cosf/sinf, `fVar*fVar*16.0`/`*128.0`,
    vec3f_normalize, stride-0x10 `s16`/`s8` writes to `D_800E1850`). S158 FP class.
  - `func_80067730` (0x80067730, 171i) — FP matrix builder over the collision-record array
    (guMtxIdentF/guAlignF/guScaleF/guMtxCatF/guTranslateF/guMtxF2L → `D_80104A00[i*0x40]`). S158 FP class.
  - `emit_aim_target_ring` (0x80066B50, 602i) — F3DEX2 display-list emitter (guRotateRPY/guMtxL2F/
    guMtxXFMF/cosf/sinf, 8-segment ring, counter-gated fade via `D_801B7248`). DL+FP wall (`#display-lists`).
  - `draw_course_decal_triangles` (0x800660B0, 680i) — F3DEX2 display-list builder (per-record type
    dispatch, vec3f_normalize, `glistp++` DL-word stores). DL+FP wall (`#display-lists`).
  **Retry:** a permuter/`#cross-project-matched-corpus-mining` increment on the 2 regalloc near-matches, OR
  a dedicated FP/`#display-lists` sprint for the 4 walls. No cross-repo name sync (all `func_`/pre-curated).

- **(S188 MIXED-PARTIAL — carried; 3 of 11 banked)** `src/main/set_camera_matrices_fixed.c`
  (main-segment `[0x40180]` camera/projection FP-math pack). Subseg `[0x40180, c, main/set_camera_matrices_fixed]`
  flipped; named fns already curated in `ghidra_symbols` (`set_camera_matrices_fixed/_float`,
  `frustum_cull_point_with_radius`, `project_point_to_screen`, `mtx_from_rts`,
  `convert_and_pack_floats_to_fixed`); `func_80065898`/`func_80065A1C`/`func_80065E6C` auto-scaffold;
  no `symbol_addrs` adds. S188 banked 3 (`project_point_view_depth` dot-product + `func_80065D5C` matmul
  + `convert_and_pack_floats_to_fixed` guMtxF2L-variant). **8 CARRIED, all FP regalloc/scheduling walls
  (this is the FP-camera sub-case of `#pervasive-regalloc-classical-main`, S188):**
  - `func_80065A1C` (RPY rot+translate, 95i): **compiler-source-confirmed IRREDUCIBLE.** ROM hoists all
    10 products before any store → parks `cp*sy` in a 6th callee-saved FP reg (`$f30`, frame -0x50);
    faithful C (inline + 10-explicit-temp) gives 5 regs (-0x48) + cr/sp/cp permutation. No `REG_ALLOC_ORDER`
    in `config/mips/mips.h` → priority-driven, not forceable from source. Escalation: cross-project
    matched-corpus mining or accept as a permanent regalloc carry.
  - `func_80065898` (persp-project+clamp, 97i): +1 `mov.s $f6,$f12`; permuter `--main` 1470→670 over
    117k iters, no match. Struct `ProjectedPoint` + named globals (view_matrix/persp_scale_x/screen_center_y)
    correct. Permuter/cross-mining escalation.
  - `set_camera_matrices_float` (215i) / `set_camera_matrices_fixed` (188i): copy-loop (de-biasable via
    2D indexing) + a 16-field perspNorm normalize held in registers + stored in a heavily-scheduled block
    = severe FP-pressure wall.
  - `frustum_cull_point_with_radius` (141i), `project_point_to_screen` (146i), `mtx_from_rts` (113i):
    FP projection/matrix, same wall class (not deeply attempted).
  - `func_80065E6C` (141i): Vtx triangle-normal with a jump-table `switch(mode)` (cases 0-3, fall-through)
    → needs a rodata-jtbl carve (`#rodata-sibling-yaml-pattern` / `#switch-jtbl-dispatch`) + the FP normalize.
  Retry framing: a dedicated permuter + `#cross-project-matched-corpus-mining` sprint on the 2 close ones
  (A1C/898), OR `func_80065E6C` as a jtbl-carve increment. m2c context (`ctx.c` with the Ghidra `Mtx4f`
  typedef) is re-derivable. Do NOT re-attempt A1C without a new idiom — it is source-proven irreducible.

- **(S192 MIXED-PARTIAL — carried; 5 of 7 banked)** `src/main/func_80077BF0.c` (main-segment
  `[0x52FF0]` spark-effect one-tu pack). Subseg `[0x52FF0, c, main/func_80077BF0]` flipped; all fns
  `func_`/`render_spark_effects` (render_spark_effects@0x80078128 already curated; no `symbol_addrs`
  adds). S187 banked 4 (BF0/C0C global-init glue + DEC heap3_free group loop + E6C bgtzl decrement);
  S192 banked `func_80077C18` via the saved typed `SparkGroup` near-free retry plus a final
  commutative-add source-order nudge (`*(col + n + src->grid)`). **2 CARRIED** (no rodata carve —
  shared TU rodata referenced extern; ROM green off the 5 banked):
  - `func_80077E94` (0x80077E94, 165 insn) — **S158 FP class, non-structural seed so far.** FP particle
    initializer: `guRandom` ×N + `cvt.s.w`/`add.s`/`mul.s`, FP consts 64.0/0.1/0.0005 via `lui`+`mtc1`
    (no rodata carve), nested grid loop writing particle fields @0x80/0x84 of the 0x98 `SparkParticle`.
    S192 m2c + typed seeds compiled but stayed far below structural threshold (`percent < 0`), so
    compiler-source/permuter escalation is premature. Retry only with a better structural seed first.
  - `render_spark_effects` (0x80078128, 506 insn) — **NOT ATTEMPTED.** 63 FP ops + 6 jals, the DL/render
    tail. FP + display-list heavy; dedicated `#display-lists` sprint (F3DEX2 auto via `mk/main.mk`).
  **Retry:** dedicated FP/DL sprint for E94 + render. No cross-repo name sync (all `func_`).

- **(S186 MIXED-PARTIAL — carried; 10 of 15 banked)** `src/main/lz_compress_extended_dma.c`
  (main-segment `[0x440A0]` terrain/hole-loader pack — NOT pure LZ; the lead-fn name misleads). Subseg
  `[0x440A0, c, main/lz_compress_extended_dma]` flipped; all fns `func_` (no `symbol_addrs` adds). S186
  banked 10 (5 call-glue + nested-RNG triple `func_80068F4C`+`func_80068F00/F18` + `func_8006955C` switch
  + `func_80069BCC` via `#mem-in-struct-scheduling-lever`). **5 CARRIED, all WALL-class** (no rodata carve
  — the shared TU rodata is referenced extern; ROM green off the 10 banked; seeds via the S186 m2c+RE'd-
  struct recipe):
  - `func_80068F98` (0x80068F98, 83 insns) — FP nested-loop vec3-component scaler (3× outer × two inner
    loops, `cvt.s.w`/`mul.s`/`trunc.w.s`, 4 running pointers + scale-array [0.25/0.25/0.125] index; scale
    consts via `lui`+`mtc1`, no FP-pool rodata). `#pervasive-regalloc-classical-main` / S158 FP class; not
    attempted deeply. Called by the banked `func_800690C0`.
  - `func_80069124` (0x80069124, 294) + `func_800695F8` (0x800695F8, 414) — the big compression-core fns;
    not attempted (large). `func_800695F8` calls the banked `func_80068F4C`; `func_80069124` is called by
    the banked `func_8006955C`/`func_80069CE4`.
  - `func_80069D08` (0x80069D08, 157) — loader-thread main loop (`func_80069F38` starts it): `do{}while(gate)`
    + mode-dispatch on a loaded value vs 4/<2/3/5/1 + `D_801B6098==7` via `func_80029A30`/`func_80029A6C` +
    debug-string spam (MPSTART/MP1..MP5) + calls the banked sub-loaders. Complex branch dispatch.
  - `lz_compress_extended_dma` (0x80068CA0, 165) — DMA double-buffer orchestrator (`osEPiStartDma` +
    mesg-queue ping-pong, calls `lz_decompress_extended`); goto-loop.
  **Retry:** a permuter / `#cross-project-matched-corpus-mining` increment on the FP scaler + the big cores,
  OR a dedicated compression-core/thread-dispatch sprint. No cross-repo name sync (all `func_`).

- **(S184→S185 MIXED-PARTIAL — carried; 14 of 21 banked; highest single-file classical bank)**
  `src/main/func_80043C20.c` (main-segment `[0x1F020]` golf-shot/club logic pack). Subseg
  `[0x1F020, c, main/func_80043C20]` flipped; `ClubShot` (0x34) + `ShotInput` structs defined; all names
  pre-curated (no `symbol_addrs` adds). S184 banked 13 (getters/wrappers + 2 nested pairs). **S185 banked
  `resolve_club_terrain_mask`** (+1, jtbl_800CC530 `.rodata` carve at `[0xA7930, .rodata, main/func_80043C20]`
  0x60 + cross-jump switch-funnel fix; see RETRO S185). **7 CARRIED now, two DoD-blocker classes:**
  - **`resolve_shot_quality_table` — STRUCTURAL-COMPLETE near-free retry (carve SOLVED, sched-irreducible).**
    109/109 insns, only a 3-instr v0/v1 load-schedule residual proven irreducible by the S185 sched.c
    subagent (`#register-reuse-nudge` load-use-interlock variant; permuter useless). **Retry checklist
    (near-free):** (1) restore the saved near-match `docs/wip/func_80043C20.resolve_shot_quality.near-match.c.txt`
    + the 0x230 carve `[0xA7930, .rodata, main/func_80043C20]`(over jtbl530+23 tables+jtbl700), `[0xA7B60, rodata]`;
    (2) all refs placed (get_table_entry; digit tables are TU-local `static const char[16]`, NOT extern/2D —
    see `#rodata-sibling-yaml-pattern` interleaved-partial-TU); (3) no recover-externs/symbol adds; (4) classical;
    (5) `docs/wip/resolve_shot_quality_table.near-match.md` + decomp.me scratch `dZACn`. Only closes with a NEW
    faithful reg-pressure idiom that allocates `power`→`$v0`; else stays a documented irreducible carry.
  - **`predict_shot_distance` — NOT ATTEMPTED (S181 abs-coalescing class + f64 carve + nested child).** f64
    rodata literal `D_800CC508` via `ldc1` (carve, interleaved with strings — see the S185 layout notes) +
    fresh-reg `abs.s $fv1,$fs1` = the `#abs-coalescing-reg-swap` fresh-reg LAW single-use+dying-hard-reg
    irreducible class. Its nested `lerp_int_v2` child is byte-exact and banks ONLY with the parent. Dedicated retry.
  - **Pervasive-regalloc walls (3, structurally-complete near-misses)** — `#pervasive-regalloc-classical-main`,
    "mine more optimal than target", not source-reachable: `func_80044CCC` (arg0↔lp allocno swap by loop-weight
    priority + a folded dead guard via DCE; permuter 800→575 plateau, 195/196), `func_800451E4` (const-1 CSE +
    allocno tiebreak + reorg delay-slot, 136/139), `predict_shot_distance_variant` (pseudo82=`&D_800BB4FC` claims
    `$s0`, pushes category; mine 3 instrs SHORTER — its nested `lerp_int` child is byte-exact, banks with parent).
  - Near-match seeds in `nonmatchings/<fn>/base.c`. **Retry:** the `resolve_shot_quality_table` near-free replay
    (carve done) OR a permuter/`#cross-project-matched-corpus-mining` increment (the 3 regalloc fns + the FP-pair
    parents). No cross-repo name sync.
- **(S183 MIXED-PARTIAL — carried; 2 of 9 banked; DEFINITIVE pervasive-regalloc TU)**
  `src/main/func_80052250.c` (main-segment `[0x2D650]` game-code pack). BANKED byte-exact C:
  `func_80052250` (0x80052250, `scenario_mode_id==0xC`), `func_80052324` (0x80052324, scenario table
  lookup). SEVEN `INCLUDE_ASM` stubs remain → NOT md5-candidate. **No flip/data/symbol enablers left**
  (subseg `[0x2D650, c, main/func_80052250]`; refs all placed; NO rodata carve; auto `func_` names).
  All 7 carries have a `docs/wip/<fn>.near-match.md` with FULL decoded logic + residual + retry ideas.
  Two distinct carry classes:
  - **`func_80052264` (0x80052264, ~48 insns — SPIKE, 1-scratch-register `REG_ALLOC_ORDER` wall,
    permuter+source-CONFIRMED):** `-arg2/18 - (arg2%18 >= D_800C1435[func_80051FCC()*200 + arg1*10])`.
    Byte-exact EXCEPT the divide-1 `mfhi` transient scratch (`t0`/$8 ROM vs `a0`/$4 mine) + its 1 `sra`
    use. Table index matched via the `#register-reuse-nudge` array-index `+` operand-order lever (arg1*10
    on the RIGHT). `mfhi→mult` hazard `nop`s CORRECT (KMC-`as` inserts, interlocks=0). Permuter (--main,
    2 bases, 42k iters) plateaus at this exact register; TU `#profile-probe` -O2-best-no-flag. Retry:
    `#cross-jump-tail-merge`… no — `#cross-project-matched-corpus-mining` for the idiom that makes the
    post-jal `arg1*10` qty win `a0` before the `mfhi`-hi qty. `docs/wip/func_80052264.near-match.md`.
  - **`func_80052384` (0x80052384, 50 insns — SPIKE, base-pointer-hoist wall):** `void(void)` 8-row
    fill/sort(`func_8005B150`)/rank over `D_800C1435`. Inner1 load matches; inner2 store HOISTS
    `&D_800C1435` into callee-saved `s3` (+8 frame) where ROM copies row `s0→a3` + re-materializes. 52 vs
    50. Permuter 850→250. `docs/wip/func_80052384.near-match.md`.
  - **`func_8005244C`/`func_800525C4`/`func_80052834`/`func_80052A68` — 4 large compare/dispatch fns,
    BUILT as growing-diff `#pervasive-regalloc-classical-main` near-misses** (89v94/89-short;
    156v156/84-diff struct-fold cascade via `(&D_801B60A0)[-3]`; 136v141 shared-block structure; 165v162
    output-param 3-loop). Each `docs/wip/*.near-match.md`. Retry: permuter --main (big cascades) +
    control-flow shaping; these are the TU's dominant wall class.
  - **`func_80052CF0` (0x80052CF0, 188 insns — DECODED, not built).** `switch(D_801B608C)` game-mode
    dispatcher; carried without a build given the 8/8 definitive wall pattern. Retry: write from m2c +
    `#switch-jtbl-dispatch` + permuter, dedicated sprint. `docs/wip/func_80052CF0.near-match.md`.
  - **Retry checklist (near-free):** (1) subseg flip DONE (`[0x2D650, c, main/func_80052250]`); (2) all
    refs placed (`D_800C1435`, `D_801B71F6/F9/FB`, `D_801B6090/98`, `D_801B60A0`, `scenario_mode_id`,
    `func_80051FCC`/`80052264`/`800521C0`/`800521DC`/`8005B150`), NO carve; (3) NO recover-externs;
    (4) classical; (5) the 7 WIP near-match files above; (6) TU `#profile-probe` = -O2, no-flag-flip.

- **(S182 MIXED-PARTIAL — carried; 3 of 7 banked)** `src/main/func_8004D190.c` (the VI/framebuffer +
  grid-print debug-display pack `[0x28590]`). BANKED byte-exact C: `clear_text_grid` (0x8004D794),
  `set_flag_based_on_param` (0x8004D99C), `func_8004D580` (0x8004D580). FOUR `INCLUDE_ASM` stubs remain →
  NOT md5-candidate. **No flip/data/symbol enablers left** (subseg `[0x28590, c, main/func_8004D190]`; all
  refs placed `D_`/`flag`/`D_A0000000` externs, NO rodata carve; only `clear_text_grid` newly named).
  Two distinct carry classes:
  - **`func_8004D4B8` (0x8004D4B8, ~50 insns — SPIKE, `#dead-frame-reload-artifact-regalloc-wall`):**
    string→glyph-tile blit into a `u16* out` tilemap from the `D_800BE6E0` glyph LUT. STRUCTURALLY
    BYTE-EXACT (48 non-frame insns match in order+opcode) except the target reserves a DEAD 8-byte frame
    (no `($sp)` body access) + the `t1<->t2`/`t3<->t4` reg-perm it drives. Root cause (mips.c
    compute_frame_size): local-alloc reserves a slot despite free `t7-t9`, global-alloc fills it → dead
    slot; not reachable from faithful C. **Levers landed (KEEP):** `c-=0x20` in-place (fixed char→v1 +
    dropped an extra `move`); clean `for(a3=0;a3<2;a3++) out[t0*2+a3]=LUT[t1+a3]` (fixed the branch-likely
    inner loop). Tried-and-failed: explicit `base=t1` invariant copy (no frame). Near-match in
    `docs/wip/func_8004D4B8.near-match.c.txt`. **Retry:** a genuinely-new mechanism that makes local-alloc
    spill one inner-block pseudo, OR the TU-wide `#profile-probe` (see next bullet). Do NOT re-run the
    in-place/for-loop/base-copy levers.
  - **`func_8004D5F0` (0x8004D5F0, 105 insns — SPIKE, register-COALESCING regalloc wall, permuter-CONFIRMED):**
    framebuffer glyph renderer, double-buffered (writes both current+next VI fb, uncached K1). 99/105
    near-exact. Target keeps 2 NON-coalesced copies of `c` (s0 original for the first `(s8)c` compare, s2
    working; frame 0x30/s0-s5, defers `t3=next K1` into the beqz delay slot); mine COALESCES to one (frame
    0x28, s0-s4) = MORE optimal. Permuter (main profile, direct import.py — see
    `#permuter-setup-for-kmc-toolchain-mirrors`) base 2055 → plateau 1190, NO path to 0. Tried-and-failed:
    `c2=c` split, two explicit copies `raw`+`clamped` (all coalesced). `OS_PHYSICAL_TO_K1` + `D_A0000000`
    handling all confirmed correct. Near-match in `docs/wip/func_8004D5F0.near-match.c.txt`. **Retry:** the
    non-coalesced-2-copy global-alloc state, OR a compiler-config confirmation.
  - **TU-wide `#profile-probe` FIRST on the two above (S182 doctrine).** `func_8004D4B8` (dead-frame) and
    `func_8004D5F0` (coalescing) are BOTH the "mine-more-optimal-than-target" class clustered in ONE TU. A
    single subtle flag/patchlevel that makes GCC coalesce-less / spill-more could flip BOTH at once — run
    `tools/profile_probe.py` on the whole TU before another per-fn dive (see
    `docs/hazards.md#profile-probe`). Only if that finds nothing are they genuine per-fn 2.7.2 artifacts.
  - **`func_8004D190` (0x8004D190, ~200 insns) + `func_8004D7B8` (0x8004D7B8, ~120 insns) — DL BUILDERS,
    dedicated `#display-lists` sprint.** `func_8004D190` = leaf unrolled texture-load + render-mode DL
    preamble (GBI opcodes SETTIMG/SETTILE/LOADBLOCK/SETTILESIZE/SETCOMBINE/SETOTHERMODE/TEXTURE/sync);
    `func_8004D7B8` calls it + `flag_is_set(0x1D)` then a nested row/col loop over the `D_800DAF60` text
    grid emitting glyph DL commands. Reconstruct with m2c + `gfxdis.f3dex2` (F3DEX2 profile via
    `mk/main.mk`) — the DCE0 DL-pack workflow (S179/S180). Not attempted S182.
  - **Retry checklist (near-free):** (1) subseg flip DONE (`[0x28590, c, main/func_8004D190]`); (2) placed
    refs: `flag`(0x800BFEE4), `D_800DAF60`, `D_800BE6E0`, `D_800BFEE0`, `D_A0000000` all placed externs, NO
    carve; (3) NO recover-externs; (4) classical (no upstream); (5) permuter scaffold live
    `nonmatchings/func_8004D5F0/` (main settings; output-1190-1 best); (6) near-matches in the two WIP
    files above.

- **(S175 MIXED-PARTIAL — carried; 5 of 6 banked; new project-best 13530/13235, `register` ruled out)** `src/main/print_string_at_grid.c` (the grid-print
  debug cluster `[0x28DC0]`). BANKED byte-exact C (S171): `check_and_print_grid`, `func_8004DA4C`,
  `convert_and_print_hex`, `func_8004DAF4`; BANKED (S172): `print_string_at_grid` (the S171-rated harder
  carry — broke via the nested-if branch-likely + lazy-base levers, now `#cross-jump-tail-merge`). ONE
  `INCLUDE_ASM` stub remains: `func_8004DC44`. File mixed-partial (1 stub) → NOT md5-candidate. **No
  flip/data/symbol enablers left** (subseg `[0x28DC0, c, main/print_string_at_grid]`; all refs placed
  `D_`/`flag` externs, NO rodata carve; name auto `func_`).
  - **func_8004DC44 (0x8004DC44, 300B — DEAD-FRAME + v0/v1-DIVIDE-SWAP regalloc wall, dump-verified
    S173).** Renders `D_800BFEE8` rows × 40 chars from ring `D_800DB410` (start
    `((D_800DC6D0/40-nrows)*40+4800)%4800`, wrapping `%4800`) into grid `D_800DAF60` (start
    `1200-(nrows+3)*40`), dual-IV (offset + pointer). **S173 deep compiler-source dive (4 subagents on
    GCC 2.7.2 + binutils 2.6, ~35 variants, 275k permuter iters). Full analysis + IMPROVED SEED in
    `docs/wip/func_8004DC44.wip.md`.** OPERATIONS now match the ROM 100% (seed vA); residual = pure
    register allocation, two items:
    1. **Dead 8-byte frame** = ONE greedy reload spill (mips.c `MIPS_STACK_ALIGN` 4→8; frame emits only
       at `get_frame_size()>0` post-reload). **REACHABLE** (S172's "no clean source trigger" framing is
       RETIRED): a structured outer loop produces it (via magic-hoist pressure), and the permuter hit a
       frame-bearing 75-insn candidate (score 14450, frame at exact ROM position). Frame ⊥ control flow.
    2. **`v0<->v1` swap in the signed-divide-by-40** (THE true wall — **S174 CORRECTION: flippable-in-
       isolation, NOT "unrecoverable" as S173 claimed**). `local-alloc.c` life-dominated priority
       (`log2(refs)*refs*size/life`) gives the tiny-life magic 6666 vs the dividend's 1666 → magic grabs
       `$v0` (ROM: dividend→$v0, magic→$v1). **BUT `return D_800DC6D0/40` reproduces the ROM's `/40` bytes
       EXACTLY** — the divide flips when the quotient reaches `$v0` via a **reg-2 SET** (return/call copy)
       that lands the chain in local-alloc's **suggestion pass** before the general pass (grounded: SA-A
       `local-alloc.c` combine_regs/suggestion; SA-B `sched.c` life formula; 8-project cross-decomp
       sweep). The REAL blocker: `func_8004DC44` is a **void/callless/returnless leaf whose quotient
       feeds arithmetic then a loop-carried store** → it emits **no reg-2 mention** → deterministically
       magic→`$v0`. Every faithful lever fails *in that context* (24 control-flow × src combos, multi-term
       dividends, all associativity, the sched1 lifetime lever, memcpy [`lwl/lwr`], m2c [plain global
       divide]); `register asm("$2")` forces it (28 diffs) but is unfaithful AND incomplete (quotient→v1
       not `a3`, frame absent). See `#signed-divide-const-v0v1-quotient-destination`.
    - **S175 progress (permuter-reseed + `register` ruled out):** reseeding the permuter from the
      frame-bearing best (S173/S174 always seeded from the frameless vA) broke the 3-sprint-stuck floor:
      `14450 → 13530 → 13235` (new project-best). The **13530** candidate (`output-13530-1/`) is
      structurally identical to the ROM — frame present, schedule + ALL operations match — with *every*
      remaining diff a register name cascading from the one `/40` dividend-register choice (zero op/shape
      diffs). The **plain `register` keyword was ruled out DEFINITIVELY** (PO /systematic-debugging):
      controlled A/B (`s32 seed` vs `register s32 seed`) → `.text` byte-identical + both permuter-score
      16613 (via new `tools/pscore.py`); source-proven (3 mips-gcc-2.7.2 subagents) that REG_USERVAR_P is
      absent from `local-alloc.c`/`global.c` priority and `DECL_REGISTER` is ignored at -O2. The
      `register asm("$2")` hard-reg hack from the frame-correct base is *worse* (51 diffs: quotient→`$v1`,
      ROM wants fresh `$a3`) — confirms a *coordinated* `{dividend→$v0, quotient→$a3-fresh, dead-frame}`
      alloc. See `#signed-divide-const-v0v1-quotient-destination` + `docs/wip/func_8004DC44.wip.md ## S175`.
    - **Seed + scaffold:** `nonmatchings/func_8004DC44/` base.c restored to the vA seed; best candidates
      preserved in `output-13530-1/` (cleanest) + `output-13235-*/`. **Retry (kept retryable per S175 PO):**
      the divide is flippable-in-isolation but the void-loop-fed context is deterministically magic→`$v0`
      — no faithful source lever survives the loop (S174 control-flow/association/schedule/cross-project
      corpus; S175 permuter-reseed + `register`). A future retry needs a genuinely NEW mechanism (a
      sched1/pressure state reproducing the coordinated `{dividend-$v0, quotient-$a3-fresh, dead-frame}`),
      or accept as a permanent `#signed-divide-const-v0v1-quotient-destination` /
      `#dead-frame-reload-artifact-regalloc-wall` carry. Do NOT re-run the same permuter/single-fn dive
      or the `register`/associativity levers — S172–S175 exhausted them (~430k+ iters). Only untried lever
      left: cross-project mining for a matched plain-global-divide with quotient-to-loop-var analog
      (S174 found none in 8 decomps).
  - **Retry checklist (near-free):** (1) subseg flip DONE; (2) placed refs: `flag`=0x800BFEE4,
    `D_800BFEE8`/`D_800DAF60`/`D_800DB410`/`D_800DC6D0` all placed externs, NO carve; (3) NO
    recover-externs; (4) classical (no upstream); (5) permuter scaffold live — reseed from
    `output-13530-1/source.c` (the frame-bearing best), NOT vA (S175 doctrine: reseed from best);
    (6) full context + seed source in `docs/wip/func_8004DC44.wip.md`. Score variants with
    `venv/bin/python3 tools/pscore.py nonmatchings/func_8004DC44 <cand.c>`. Inline any `output-0-*`
    the permuter produces, clang-format, full-make SHA, done.

- **(S169 MIXED-PARTIAL — carried; 1 of 3 banked)** `src/main/func_80076640.c` — `func_80076778`
  BANKED byte-exact C; `func_80076640` + `func_8007680C` remain `INCLUDE_ASM`. File is mixed-partial
  (ROM green off the matched fn + the shared `ACAD0` rodata referenced extern, NO carve). Retry =
  decompile the 2 carried fns from the saved WIP `docs/wip/func_80076640.3fn-wip.c.txt` (both compile
  in-tree today, the gap is pure regalloc). **No flip/data enablers left:** subseg already `[0x51A40,
  c, main/func_80076640]`; rodata stays in the blob referenced extern; placed callees are all named/
  placed main-segment fns. Permuter scaffold live `nonmatchings/func_80076640/` (main settings).
  - **func_80076640 (S181: PROVEN `#abs-coalescing-reg-swap` wall — NOT near-free; retry ONLY on a NEW
    zero-insn mechanism):** 75/78 byte-exact (fixed the literal `0.34906584f`→`0.3490659f`=0x3EB2B8C4;
    ONLY the 3 abs-region regs differ: target `abs.s f2,f0`+const in `f0`, mine `abs.s f0,f0`
    in-place+const in `f2`). **S181 EXHAUSTED every faithful/flag lever — do NOT re-run:** (1) 4
    GCC-2.7.2-source lenses → root cause = `combine_regs` suggested-reg pre-pass (local-alloc.c:1813-1817
    / 1469-1477): a dying-`$f0` abs operand gets an unconditional `$f0` arithmetic suggestion the literal
    const can never contest → in-place; the `absSF2` MD (mips.md:1578 `=f`/`f`) merely permits. (2) An
    8-project cross-project mining sweep (`#cross-project-matched-corpus-mining`) proved the byte-cmp
    fresh-reg unary-float LAW on the identical KMC GCC 2.7.2 (5 corpora MP1/MP2/MP3/drmario64/sbk2;
    hm64+pl64 NULL): single-use + dying-hard-reg → ALWAYS in-place. (3) A 12-flag profile-probe (all
    in-place). (4) A direct 2.8.1 cross-compile (in-place + 80 insns, worse; and same-TU `func_80076778`
    matches at 2.7.2 ⇒ the TU is PROVABLY 2.7.2, NOT a wrong pin). MG64's exact signature (dying +
    hard-reg + single-use + single-precision) appears in NO same-compiler corpus at EITHER version → a
    2.7.2 patchlevel/build artifact the reconstruction cannot reproduce. Permuter PROVEN FUTILE
    (plateaued 338k; only mutates source shape). KEEP the landed levers (const-extern doubles;
    `do{ if(fabsf(cosPitch)<0.1f){...} }while(0)` + `cosPitch=cosf(pitch)` temp — load-bearing for the
    schedule). **Retry ONLY on a genuinely-NEW mechanism** (a faithful source keeping the abs
    result/operand live at ZERO added insns, or the exact original build binary); see
    `#abs-coalescing-reg-swap`. Do NOT re-run the source dive / permuter / flag probe / 2.8.1
    cross-compile.
  - **func_8007680C (SPIKE, S158-class deep regalloc):** structure complete (721/756 mnemonics; logic
    fully RE'd from asm). Blocked by pressure-driven regalloc: (1) frame -496 vs -504 (target spills 1
    more scalar, `zHi`@0x174, dead-after-loop5, that mine keeps in a reg); (2) min/max trackers as
    float-bits in GPRs s3/s4/s5 (`mtc1`/`mfc1`) under loop5 FP pressure, mine keeps more in FP; (3)
    target keeps the 30000/-30000 tracker-init int-bits in callee-saved GPRs s3/s5 (reused for the
    loop1+loop5 resets), mine rematerializes. **Untried:** permuter (scaffold-able via the same
    main-settings recipe) + compiler-source fan-out on the FP-vs-GPR spill/rematerialization.
- **(S167 SPIKE — carried, cse/regalloc branch-fold wall; 2 of 3 head fns BANKED)** `func_80070FD0`
  in `src/main/func_80070FD0.c` — ONLY `func_80070FD0` remains INCLUDE_ASM (func_800710C4 +
  func_8007117C banked byte-exact C). The fn is BYTE-EXACT except a **3-word branch-direction triple**
  in the osSyncPrintf tail (mine `beqz v1;ori a1,v0,0x80;move a1,v0` vs target `bnez v1;move a1,v0;ori
  a1,v0,0x80`). **Root cause (fully traced, cse.c `make_regs_eqv`:840-862):** the value is
  `nv=(D_801B60C5==0)?t|0x80:t` where `t=count|(old&0x80)`, `old` reused for the loaded byte (`lbu a1`)
  AND the printf arg. The two C forms are LOCKED to the fold: default `old=t` -> the copy makes `old`
  canonical (it outlives `t` + crosses the post-branch EBB) -> cse rewrites the other arm's `t|0x80`
  -> `old|0x80` -> `t` folds into $a1 (gives the target's **bnez** but 60 instrs, 1 short); default
  `old=t|0x80` -> `t` stays separate in $v0 but gives **beqz** (lever-2, 61/61, the 3-word miss).
  Branch-direction and the fold are inseparable. **35 hand source-variants + 43k permuter iterations**
  all plateau at these 3 words; NOT reachable from equivalent single-TU C (the load-in-$a1 requires
  reusing `old`, which forces the fold). **UNTRIED / needs the ORIGINAL source shape** — the target
  was compiled from a form that keeps `t` alive across the branch without the canonical-old fold
  (likely a different flag/print data-flow, a helper, or a macro). Retry needs game-source insight,
  not more permuter. Near-match C saved: `docs/wip/func_80070FD0.near-match.c.txt` (lever-2, byte-exact
  minus the 3 words). Struct model `SaveBlock{u8 pad[0xF4]; s8 tbl[6][0x12]; u8 wins[13][0x12][2]}` @
  base990 is the MEM_IN_STRUCT key (folded from the func_800710C4 subagent; nailed the 2 banked fns).
- **(S166 SPIKE — carried, pervasive-regalloc; 2 of 3 fns now BANKED)** `src/main/lz_decompress_simple.c`
  — ONLY `lz_decompress_extended` remains a stub. `lz_decompress_dma` banked S165 (`20ff76d`),
  `lz_decompress_simple` banked S166 (`60ecb9a`, the wall CRACKED byte-exact via the loop-weight lever;
  struct unified to `LzDecompressState` 0x28). File is MIXED-PARTIAL (2 C + 1 stub, ROM green) → NOT
  md5-candidate until `extended` lands. **Blocker:** `#pervasive-regalloc-classical-main` register-
  permutation floor at valid **raw-185** (from prior 208), GREG-PROVEN and DUAL-CONFIRMED: two coupled
  near-tied 3-cycles — Cycle A {ridx,src,dist} (ridx pinned highest by its dual-emit ring refs
  `ridx+1&0x7ff`/`ro=ridx*2`), Cycle B {param,end,ring} (param a binary t1↔t8 live-length switch,
  target t6 unreachable in between; param L pinned by 5 post-loop exit stores). Both value-distinct
  (safe permuter number-reorder can't flip) AND structurally pinned (no single manual nudge). **State:**
  base = `nonmatchings/lz_decompress_extended/base.c` (= `scratchpad/lz/best_extended.c`, decomp_loop
  21220, 290/292, ridx increment intact). **Retry checklist (mechanical replay):** (1) split ALREADY
  DONE (`[0x43810, c, main/lz_decompress_simple]`; 15-fn tail `[0x440A0]` stays asm); (2) placed-ref
  inventory: all callees placed, NO rodata/data carve, `extended` name pre-curated (NO symbol adds);
  (3) NO recover-externs; (4) no upstream (classical); (5) the honest verdict is that raw-185 is the
  floor — a genuine live-length+ring-ref conflict, not unfinished work. A further attempt should test
  a STRUCTURAL change that breaks the ridx/param live-range pins (e.g. re-express the ring index or the
  exit-store ordering), NOT another permuter grind (safe-passes-only already plateaued 230→208; see
  `#permuter-goto-backedge-liveness-unsound`) — apply the `#loop-weight-and-live-length-regalloc-
  steering` levers first. **Cross-repo follow-up:** push the corrected `LzDecompressState` layout to
  the Ghidra workspace (natural-aligned, size ≥0x28).

- **(S159 BANKED, removed from carry-overs)** The S156-spike `func_80051E90` + `func_80051FCC` jtbl
  pair (`[0x2D290, asm]`) banked S159 (`c774454`) as `src/main/func_80051E90.c` — the `rodata-jtbl`
  sibling carve for `jtbl_800CCC30` was the real work, resolved at the S159 gate. Pruned at S161 per
  the carry-over-hygiene rule (`/sprint-review` Step 5.4).

- **(S154 BANKED, removed from carry-overs)** The 3-fn `[0x455C0]` tail (`calc_vec3_magnitude`,
  `crc16_ccitt`, `report_div_error`) banked S154 by RECOMBINING the whole `func_8006A000` one-tu into one
  file. See the `## Active phase` S154 BANKED paragraph and `#decomposed-one-tu-rodata-alignment-split`.

- **(S155 DONE, branch `tooling/pts-recalibration`).** Recalibrated: `seed_points` now size-grades a
  one-tu classical pack of `nfns<4` (tiny <256 B → 3, mid → 5, big → 8; deweight to 3 if `jal-free`;
  +1 if `rodata-straddle`) + enabler bumps, instead of the flat `classical and pack → 13`. Phase 2a
  `HAZARD_JAL_FREE` (0-jal deweight, a2) and Phase 2b `HAZARD_RODATA_STRADDLE` + `rodata_double_literals`
  (the S154 alignment-wall detector) both landed. Golden gate held: formula diff was hazards-column
  ONLY (new tags on 5 live packs), zero pts/score/size/row-order change. Vein was mined out (0 live
  rows changed pts) → future-proofing; regression-guarded by the `test_seed_points_characterization`
  assertions on S148-S154. See VELOCITY.md ## Seed rubric (S155 re-anchor) + `docs/agent-workflow.md ## Story points`.
  The original follow-up rationale is retained below for the record.

- **Tooling follow-up (S148, PO-selected #1 companion; deferred to a golden-gated tooling branch, NOT
  a review-gate edit).** Recalibrate `pick_target.py`'s `pts` so it does not over-price tiny
  `none`-upstream classical packs. S148's increment was the SMALLEST candidate (176B, 2fn) yet priced
  `pts=13` — size is barely weighted against the none-upstream + nfns + one-tu bumps, so the 8-gate
  false-fired on a trivially-bankable pack. The `docs/agent-workflow.md` **small classical pack exemption** (S148)
  handles the gate symptom by-hand; this follow-up fixes the root pricing: give raw byte-size a larger
  weight (or a small-pack floor that caps pts for `<256B AND <=2fn AND one-tu`). **Why a branch:** pts
  feeds the displayed estimate and the 8-gate, a load-bearing surface, so re-weighting needs the
  golden-gated + reassess-checkpoint discipline (byte-identical goldens on the ranker output) to avoid
  silently reshuffling the smallest-first sort. Companion to the small-pack exemption text.
  **S150 2nd data point:** `main/func_80029250` (`cfb_setup`+`cfb_set_num`, 496B, 2fn, one-tu, 0 `jal`)
  also priced `pts=13` and false-fired the 8-gate. It exceeded the exemption's original `<256B` cap, so
  the retro added a `(a2) 0-call size-agnostic` exemption branch by-hand — but that is a 2nd by-hand
  patch on the same root mispricing. Two false-fires in three sprints; the recalibration should now
  weight raw byte-size AND deweight/floor a `0-jal` one-tu pack (a `jal`-free pack has no
  callee-resolution cost, so its true effort is far below the nfns/one-tu bumps that inflate its pts).
  **S152 4th data point:** the DECOMPOSED 3-fn 384B none-upstream subseg `main/func_8006A000` re-priced
  `pts=13` — unchanged from the full 8-fn pack, i.e. decomposing 8fn→3fn did NOT lower the pts (size is
  so weakly weighted that a 384B/3fn slice and an 800B/8fn pack both hit the ceiling). Here the 8-gate
  was resolved by a genuine decompose (so no by-hand exemption needed), but the mispricing means a
  decompose can't be *seen* in the pts. Reinforces: raw byte-size needs real weight so a decomposed
  slice prices below its parent pack.
  **S153 5th data point:** the 64B 2-fn `jal`-free head slice `main/func_8006A180` (`update_rng_seed` +
  `hypotf_2d`, decomposed from the S152 remainder) again priced `pts=13`; the small-pack exemption ran it
  seed-only via BOTH branches at once (a1 `<256B` AND a2 `0-call`). A 64B/2fn slice hitting the ceiling is
  the strongest under-weighting evidence yet — the recalibration's `<256B AND <=2fn AND one-tu` floor (or
  a raw-byte term) would drop this to a low single-digit pts and stop the 8-gate false-fire on the smallest
  possible packs.
  **S154 6th data point:** the 3-fn 256B none-upstream `[0x455C0]` tail priced `pts=13`; the small-pack
  exemption did NOT cover it (3 fns > the `<=2` cap), so it ran as a normal 1-increment classical sprint —
  and then, mid-sprint, MERGED back into the 8-fn `func_8006A000.c` for a rodata-alignment fix (see
  `docs/hazards.md#decomposed-one-tu-rodata-alignment-split`). So a 256B/3fn slice AND its 800B/8fn parent
  both price `pts=13`: another decompose invisible in the pts. Reinforces the raw-byte term AND flags that
  the pts should also weigh rodata-adjacency (a decompose the ranker prices as free can carry an
  unfixable rodata split).

- **Name follow-up (S148; near-free, do at the next gate with Ghidra up).** `func_ovl10_801F4A40` and
  `func_ovl10_801F4AD8` (`src/overlay_10/func_ovl10_801F4A40.c`, banked S148) kept `func_` placeholder
  names because Ghidra MCP was DOWN all sprint. When an instance is up: (1) check Ghidra for curated
  names at vram 0x801F4A40 / 0x801F4AD8 (overlay_10 / `exclusive_ram_id: ovl10`; vram is
  overlay-ambiguous, so use the overlay context); (2) if named, add to `symbol_addrs.txt` (add-only),
  rename in the body, `make` to re-confirm ROM SHA-1; (3) propagate via `sync_decomp_names.py
  --import-from-decomp`. Behaviour understood: `_801F4A40` is a flag-gated (`D_800FBDC4 & 0x9000`)
  sound/setup routine (sets 4 globals @0x800BB020/024/02C/030 then `play_sound_effect` +
  `func_800719A0`); `_801F4AD8` is a 3-insn `*p = *p` accessor.

- **Tooling follow-up (S151, PO-selected #4; deferred to a golden-gated tooling branch, NOT a
  review-gate edit).** Two DL-related tooling adds surfaced this sprint: (a) promote
  `dl_fold_check.py` (currently in the session scratchpad) to `tools/` — it scans a `gfxdis` decode for
  the texture-LOAD composite opcodes (SETTIMG/SETTILE/LOADBLOCK/LOADTILE/LOADTLUT/SETTILESIZE) so the
  gate knows whether a higher-level `gsDPLoadTextureBlock/Tile/TLUT` macro must be reconstructed (gfxdis
  does not fold those). (b) Teach `decomp_loop.py` a `main` (F3DEX_GBI_2) compile profile like its
  existing libkmc/libultra profile detectors, so isolated diffs of a `src/main/` DL fn use the same
  `-DF3DEX_GBI_2` as the real build (else the RSP opcodes read `0xB4`/`0xB3`, a false diff — S151
  worked around it with `#define F3DEX_GBI_2` atop `base.c`). **Why a branch:** both touch load-bearing
  detectors (decode-scan FP surface; a new `decomp_loop` profile changes isolated bytes), so run
  off-cadence golden-gated with reassess checkpoints (the tooling-refactor discipline).
  **S152 addition (PO-selected #3):** the same `main` profile detector should also add `-ffast-math`
  for a `needs-fast-math`-tagged fn (one whose asm has a bare `sqrt.d`/`sqrt.s` — see
  `docs/hazards.md#double-sqrt-fast-math`), else the isolated `decomp_loop` mis-compiles `sqrt` (`jal`
  or a guarded form) and reads a false diff. Bundle with the `main`/F3DEX_GBI_2 profile add above (one
  `main`-profile detector serving both `-DF3DEX_GBI_2` and, when tagged, `-ffast-math`).

- **Tooling follow-up (S157, PO-selected #4; deferred to a golden-gated tooling branch, NOT a
  review-gate edit).** Running the permuter on a game/-O2 `main/`+`overlay_*` fn currently needs a
  manual setup that the mirror-oriented tooling misses: (a) `permuter_settings.toml`'s
  `compiler_command` is stale for non-mirror fns (only `-I include`, `-mips2`; misses the libultra
  include DAG, `-mips3`, `-D_FINALROM`, `-DF3DEX_GBI_2`), so `import.py`'s preprocess fails on
  `PR/ultratypes.h` — add a `[game]` profile (or refresh the base command) matching `mk/main.mk`
  (template: `nonmatchings/func_800500E0-2/compile.sh`); (b) the isolated `target.o` must be assembled
  with MODERN GAS (`mips-linux-gnu-as -march=vr4300 -32 -I include`) after prepending
  `.include "macro.inc"` to the per-fn `.s` — KMC `as` can't parse splat's `glabel`/`nonmatching`
  macros, and it uses the correct explicit-`addu` encoding that KMC-`as`-compiled `base.o` also
  produces. Promote a `setup-game-permuter.sh` helper wrapping both. **Why a branch:** it touches
  `permuter_settings.toml` (load-bearing preprocess surface) and a new helper; golden-gate per the
  tooling-refactor policy. (Until then, S157's manual recipe in `docs/hazards.md#permuter-setup-for-
  kmc-toolchain-mirrors` applies by hand.)

- **Tooling follow-up (S182, PO-selected #1; deferred to a golden-gated tooling branch, NOT a
  review-gate edit).** `setup-permuter.sh` / `mg_resolve_c_asm` (tools/lib.sh) resolve the C file by
  grepping `INCLUDE_ASM(.*, <fn>);`, which is GONE once the fn is a C body (a near-match you want to
  permute in place). The resolver sets `C_FILE=` empty and `set -e` aborts SILENTLY. Fix: teach
  `mg_resolve_c_asm` to also resolve a fn defined as C — grep for the `<fn>(` definition in `src/**`
  + resolve the asm at `asm/nonmatchings/**/<fn>/<fn>.s`. **Why a branch:** `mg_resolve_c_asm` is a
  shared shell helper feeding the permuter setup; golden-gate per the tooling-refactor policy. Kin to
  the S170/S168 `find_segment` follow-ups (same "already-flipped-to-c" resolution gap, shell side).
  Until then the by-hand direct-`import.py` recipe applies (`docs/hazards.md#permuter-setup-for-kmc-
  toolchain-mirrors`, the S182 bullet).
- **Tooling follow-up (S182, PO-selected #3; deferred to a golden-gated tooling branch, NOT a
  review-gate edit).** Teach `pick_target.py` to flag a DL-builder fn — a **leaf** (no `jal`) with
  dense GBI-opcode command-word stores (top bytes E2/E3/FC/FD/F5/F3/F2/D7/E6/E7 written as `sw`
  immediates into a Gfx buffer) — so the gate routes it to the `#display-lists` track (m2c + gfxdis,
  F3DEX2) upfront instead of the classical track. **Why a branch:** touches the ranker's hazard
  detectors (FP surface feeding the displayed estimate + the 8-gate); golden-gate per policy. Kin to
  the S180 DL-fill-color `mem-in-struct` detector follow-up. Data points: `func_8004D190` +
  `func_8004D7B8` (S182 carries).
- **Tooling follow-up (S170, PO-selected #3; deferred to a golden-gated tooling branch, NOT a
  review-gate edit).** `tools/decomp_loop.py`'s `find_segment` resolves a placeholder by grepping
  top-level `asm/<off>.s` for its `glabel`, so it **fails on a fn whose subseg is already flipped to
  `c`** (the asm then lives under `asm/nonmatchings/<seg>/<fn>.s`, not `asm/*.s`) with `no glabel
  <fn> found in any asm/*.s file`. This breaks the asm-first fast-path miss-recovery: once you inline
  a C body and the first full-make misses, `decomp_loop` can't run the isolated per-fn diff. Add a
  `--target-s <path>` arg (or extend `find_segment`/`dc.find_segment` to also search
  `asm/nonmatchings/**`) so the isolated loop works post-flip. **Why a branch:** `find_segment` is in
  `tools/decomp_common.py` (shared, characterization-tested), so golden-gate per the tooling-refactor
  policy. (Until then, S170's manual recipe applies by hand: `mips-linux-gnu-objdump -d
  build/src/<seg>/<file>.o` vs the `asm/nonmatchings/<seg>/<fn>.s` hex, gating on the full-make SHA
  since the isolated reloc-hi/lo diffs are the `#isolated-compile-caveat` artifact; see
  `docs/hazards.md#short-text-shifts-flowing-bss` tooling note.)

- **Tooling follow-up (S168, PO away → best-judgment doc-only applied at review; the CODE fix deferred
  to a golden-gated tooling branch, NOT a review-gate edit).** `decomp_loop.py` / `dc.find_segment`
  mis-resolve a just-split classical fn to its STALE pre-split parent `asm/<A>.s` relic. After a
  decompose-split (`[0x<A>,asm]` → `[0x<A>,c] + [0x<B>,c]`), the pre-split top-level `asm/<A>.s` (the
  whole N-fn range) lingers on disk (make extract does not delete or regenerate it), so both it and the
  correct child `asm/<B>.s` declare `glabel <child_fn>`; `find_segment` globs `asm/*.s` SORTED and
  returns the FIRST, picking the parent (`4C3D0.s` < `4C620.s`) → `decomp_loop` builds an N-function
  reference and asm-differ reports a bogus near-match (empty `base_text`, `match_count==total_rows`,
  large score). **Fix (golden-gated):** for a c-flipped fn prefer the `asm/nonmatchings/<tree>/<fn>/<fn>.s`
  target as the reference source (always the exact 1-fn ground truth), OR make `find_segment` skip a seg
  file whose `glabel` set spans a now-`c` sibling (cross-check the yaml), OR have `make extract` delete
  stale top-level `asm/<off>.s` for a now-`c` subseg. **Why a branch:** touches `find_segment` /
  `ensure_reference_object` (load-bearing reference resolution feeding every isolated diff); golden-gate
  per the tooling-refactor policy. This RECURS on every classical-endgame decompose-split. Until then the
  by-hand workaround (`mv asm/<A>.s` aside) is documented at
  `docs/hazards.md#stale-parent-asm-relic-find_segment-mis-resolution-after-a-decompose-split`.

- **pts data point (S157).** A 9-fn 1664B none-upstream INTEGER pack priced `pts=13` and realized far
  above (16, +3 residual): register-alloc rotation + delay-slot scheduling + a loop-invariant hoist
  across 3 fns needed a KMC-gcc-source deep-dive AND a PO reference impl. A 9-fn integer module is NOT
  "trivial" despite no float/rodata — `nfns` alone does not predict effort, and the seed ceiling (13)
  under-prices a module whose difficulty is register-alloc/scheduling. Not a decompose target (only
  16-aligned inner boundary 0x1630 → 7+2, no clean 3-4 slice); running WHOLE was correct (banked
  atomically). Data for any future `pts` refinement that wants a scheduling/regalloc-difficulty term
  (hard to predict statically; a `coddog`-style structural signal would be needed).

- **Name follow-up (S151; near-free, do at the next gate).** `func_80050274` / `func_800500E0` /
  `func_8005029C` (`src/main/func_800500E0.c`, banked S151) kept `func_` placeholders — Ghidra had only
  `func_` at 0x80050274 / 0x800500E0 / 0x8005029C. Behavior understood (a UI wipe/reveal-box system):
  `func_80050274` REGISTERS a `WipeBox*` into `D_80132370[D_800C0E50++]`; `func_800500E0` DRAWS one box
  (advances its wipe state/timer, emits `gDPSetPrimColor`+`gSPTextureRectangle` into `*glistp`);
  `func_8005029C` emits the shared XLU render setup then DRAWS ALL registered boxes and clears the
  count. Suggested names (confirm the game term first): e.g. `wipebox_add` / `wipebox_draw` /
  `wipebox_draw_all`. When confirmed: add to `symbol_addrs.txt` (add-only; `type:func`), rename in the
  body + at all asm call sites via `make extract`, `make` to re-confirm ROM SHA-1, then propagate via
  `sync_decomp_names.py --import-from-decomp`. The `WipeBox` struct (TU-local) + `glistp` global are
  already named (`glistp` propagated to Ghidra live).

- **Tooling follow-up (S138, PO-selected #2; deferred to a golden-gated tooling branch, NOT a
  review-gate edit).** Make `pick_target.py`'s `defines-data` detector resolve + emit the static's
  `.data` rom address (`data-static:<addr>`), so the gate plans the `.data` carve instead of spending a
  build + cmp + value-search to localize it (S138 n_reverb cost exactly that). Algorithm: parse each
  `defines-data` static's initializer to its `.data` byte block (the contiguous `val/lastval/blob`
  pattern, here `00000000 C1200000 00000000`), search the baserom `.data` region for it, emit
  `data-static:<addr>` on a hit. **Hard requirement (why it's a branch, not a quick edit):** the
  value-search is multi-hit (n_reverb's block matched BOTH the n_audio_sc reverb @0xA31A0 AND the
  libultra `reverb.c` twin @0xA356C), so a value-only unique-hit is unsafe — it needs a link-cluster /
  positional anchor (the n_audio_sc `.data` cluster sits just below the libnusys `.data` carves) to
  disambiguate before emitting an address. Run it off-cadence on a branch, golden-gated + reassess
  checkpoints (the tooling-refactor discipline), since it adds FP/regression surface to a load-bearing
  detector. Spec is also inline at `docs/hazards.md#defines-data` (Tooling follow-up (S138)).

- **Tooling follow-up (S141, PO-selected #3; deferred to a golden-gated tooling branch, NOT a
  review-gate edit).** After a new lib is registered, make `pick_target.py` emit a per-file
  band-open hint: for each `needs-header:<h>` on a row whose lib is now registered, resolve `<h>`
  against the lib's coddog-pin srcdir (`audio_pins.tsv` for audio libs) and emit
  `vendor-header:<h>@<pin>/<h>` so the next gate's header enabler is a mechanical copy instead of a
  manual hunt (S141 read each libmus row's `needs-header:aud_*.h` by hand). **Why a branch:** it
  reaches into the pin-srcdir resolution + adds a new advisory column (FP/regression surface on a
  load-bearing detector), so run it off-cadence golden-gated with reassess checkpoints (the
  tooling-refactor discipline). Companion to the S140-deferred coddog-callee-tell pricing.

- **Tooling follow-up (S143, PO-deferred #3; golden-gated tooling branch, NOT a review-gate edit).**
  Price the libmus-bundled-n_audio DUPLICATE: emit a `game-embedded:libmus` / `coddog-bundled-dup` tell on the
  `[0x78330]` `al_init`/player_fx candidate (`coddog-fncount-mismatch:6vs13` = player_fx's 6 fns + the bundled n_audio
  synth ~7). The bundled copy duplicates the standalone n_audio_sc `n_al*` (libnaudio); a libmus mirror's `alInit`
  resolves through the `n_libaudio_sn_sc.h`→`player_fx.h` macro chain to the BUNDLED `CustomInit`, NOT the standalone
  `n_al*` (which may be DEAD, 0 xrefs). **Why a branch:** detecting the bundled-dup needs cross-region coddog reasoning
  (a fn whose coddog source is an n_audio_sc file but whose vram is in the libmus region) + a new advisory column on a
  load-bearing detector → run off-cadence golden-gated (the tooling-refactor discipline). Until then the gate applies
  the guard by reading `player_fx.h`'s `#define n_alInit CustomInit` + the asm callees
  (`docs/hazards.md#libmus-bundled-n_audio-duplicate`). Companion to the S140 coddog-callee-tell + S141 vendor-header pricing.

- _(CLASSICAL/MIXED carry (S145 planned decompose remainder) — `aud_dma.c` (`[0x78D10, asm]`, the LAST
  libmus asm subseg; banking it COMPLETES the libmus tree). **RESOLVED + FULLY banked S146** — NOT the
  planned mixed-partial: all 6 fns banked C, `src/libmus/` 100%. ASM-first overturned the "4 stock + carry
  DmaSample" premise below: 3 of the "4 stock" were game-modified (DmaInit stock+1-insert, DmaProcess
  2nd-half rewritten, DmaSample classical incl. an added eviction loop); DmaSample matched via 4
  GCC-codegen levers (`docs/hazards.md#cross-jump-tail-merge` inverse-levers). The original checklist
  below is retained for provenance. NOT a verbatim mirror — GAME-MODIFIED, so it
  was the heterogeneous-batch trim when S145 banked the clean aud_sched.c sibling. **Track: regime mixed
  (bank-stock-carry-custom likely).** **6 labels** (upstream n64sdkmod libmus 3.14 `aud_dma.c` has 5 source
  fns): `__MusIntDmaInit`@0x8009D910 (placed S143) + `__MusIntDmaProcess`@0x8009DA8C (placed S144) +
  `__CallBackDmaNew`/`__CallBackDmaProcess` (statics) + an **empty 0x8 `func_8009DBA0` stub** (`jr ra;nop` —
  a function MG64 emptied) + `__MusIntDmaSample` (`mus_dma_sample`@0x8009DBA8, static, the large one).
  **Body divergence is REAL (asm-confirmed at the S145 gate):** `__MusIntDmaSample` checks
  `g_mus_control_flag & 1` (=`__muscontrol_flag & MUSCONTROL_RAM`) FIRST and the N64DD/diskrom path is
  STRIPPED (MG64 is cart-only; upstream checks the DD-ROM `0xff000000` test first). The 4 stock fns
  (DmaInit/DmaProcess/CallBackDmaNew/CallBackDmaProcess) likely mirror verbatim → write them as C, carry
  `__MusIntDmaSample` (+ the empty stub) as `INCLUDE_ASM` until classically decompiled (per-file partial =
  not md5-candidate until the last stub clears). **Hazards/enablers:** recover-extern `__muscontrol_flag`
  (= ghidra `g_mus_control_flag`@0x80132368, a `refs-unplaced` — use the curated ghidra name or a coexisting
  override); BSS file-statics (dma_buffer_head/free/list, audio_IO_mess_buf, audio_mess_buf, audio_dma_size/
  count, audDMAMessageQ, cartrom_handle) → drop-static; `defines-data:diskrom_handle` (the non-`static`
  global, possibly dead under cart-only). **Profile:** `-U_FINALROM` (S145) is libmus-wide; aud_dma.c uses
  OSIoMesg/OSMesgQueue (verify any `_FINALROM`-sized struct, though OSMesgQueue is unconditional). Diff the 4
  stock fns vs upstream + classically decompile `__MusIntDmaSample`'s cart-only body before banking.)_

- _(SPIKE (S143) — `__MusIntThreadProcess` (`mus_audio_thread` @0x8009E0A8), the last INCLUDE_ASM stub in
  `src/libmus/aud_thread.c`. **RESOLVED + banked S144** — the "MG64-CUSTOM body → classical" framing was
  over-pessimistic. Once `aud_sched.h` (vendored S143) exposed the stock `musSched` vtable + the
  `__MusIntSched_{install,waitframe,dotask}` macros, the body was the STOCK libmus 3.14 thread-proc and
  `last_task`@0x800C7AE4 was the STOCK func-static (not a custom global) — only a ~4-instr MG64 pause/mute
  insert (`if (paused@0x800C7AE0) { osAiSetNextBuffer(silence@0x800C7AE8, 0x10); continue; }`) was genuinely
  custom. Seeded from the stock 3.14 source + the insert; 4 drop-static recover-externs
  (`__libmus_current_sched`@0x800C7ADC + `g_mus_audio_{paused,last_task,silence_buffer}`@AE0/AE4/AE8) + 1
  callee `rom:` override (`__MusIntDmaProcess`@0x8009DA8C vs ghidra `mus_dma_process`); `MICROCODE_CODE`
  referenced the placed `rspbootTextEnd` (== `n_aspMainTextStart`@0x800B3F20) via a local `#define`. MATCH
  first build, 0 iteration. Lesson: re-diff a "custom body" carry vs the stock source AFTER the headers are
  vendored before assuming from-scratch classical — see `docs/hazards.md#cross-jump-tail-merge` sibling rule.)_

- _(Near-free retry (S139 decompose remainder) — `n_env.c` (`[0x79E70, asm]`, the LAST libnaudio asm
  subseg) **RESOLVED + banked S140** — the completeness checklist replayed verbatim-correct, 0 rework,
  MATCH first build. Flipped `[0x79E70, asm]` → `[0x79E70, c, libnaudio/n_env]` (single file, 0x9D0);
  verbatim n_audio_sc cp of `n_alEnvmixerPull` + `n_alEnvmixerParam` + 3 file-statics + newly-vendored
  `inc/n_env_add01.inc.c`, on the existing `n_synthInternals.h`, NO new header. Both carves were clean
  1-line attribute flips (NO splits): `.data` n_eqpower[128]=0x100 was `main_data_1a` exactly; `.rodata`
  jtbl_800D2120 + `_getRate` f64 consts=0x70 was the generic `[0xAD520,0xAD590)` block exactly. The gate's
  `calls-unplaced:__pow` + `jal-count-mismatch:20vs15` were FALSE (pick_target read the `#ifndef N_MICRO`
  branch; under `-DN_MICRO=1` `_getRate`/`_getVol` are integer, asm has zero `__pow`/`_frexpf`/`_ldexpf`) —
  retro #1 retires that class (`_strip_inactive_define_branches`). The 3 statics stayed file-local
  (`static-name-collision` benign). `body-divergence-suspect@99.99` FALSE → 10 consecutive on n_audio_sc.
  ZERO symbol adds. Quality 0/0/0/0, seed-only 13pt. **Milestone: `src/libnaudio/` 100% (libnaudio asm
  subsegs → 0).** Cross-repo follow-up: none (all names pre-curated).)_

- _(Near-free retry (S130 spike, blocker RESOLVED S136) — `n_alSynAllocFX` + `n_mainbus.c` (`[0x7C720, asm]`,
  c-combined:2file) **RESOLVED + banked S137** — the completeness checklist replayed verbatim-correct, 0 rework.
  Split `[0x7C720, asm]` → `[0x7C720, c, libnaudio/n_synallocfx]` (n_alSynAllocFX @0x800A1320, +0x50) +
  `[0x7C770, c, libnaudio/n_mainbus]` (n_alMainBusPull @0x800A1370); the 0x7C770 boundary confirmed against the
  asm at the gate (n_alSynAllocFX ends 0x800A136C). The 1 NEW callee `n_alFxNew`=0x8009E550 was jal-confirmed
  (`jal 0x8009e550`) and added to symbol_addrs (stays asm in the n_auxbus pack — also pre-resolves one
  n_auxbus-pack callee). Both verbatim n_audio_sc cps on the existing vendored `n_synthInternals.h`, NO new
  header, NO rodata/data carve (n_synallocfx = pure call+return; n_mainbus = N_MICRO aClearBuffer + indirect
  handler + 2× aMix, the `alHeapAlloc`/`(macro-artifact?)`/rodata watches were FALSE for these two tiny fns).
  Both Match FIRST build, seed-only (5pt), Quality 0/0/0/0. The S130-spike `func_800A1320` block is fully
  cleared; `body-divergence-suspect@99.99` was FALSE both fns (asm == upstream).)_

- _(Spike (S121) — `contRmbControl` (0x800A19E0), the last INCLUDE_ASM stub in
  `src/libnusys/mainlib/nucontrmbmgr.c` **RESOLVED + banked S127** — the "compiler wall" was a
  MISDIAGNOSIS. S121 framed it as an unbankable `#cross-jump-tail-merge` (project gcc 2.7.2 merges two
  byte-identical counter-store tails MG64 keeps unmerged; "exhaustively proven unbankable without a new
  compiler" — 145k-iter permuter plateau, no available binary reproduces the selective merge). The real
  cause was a **game-modified body**: MG64's FORCESTOP case sets `state = STOPPED` on `osMotorInit`
  FAILURE and `state = STOPPING; counter = 2` only on SUCCESS (an `if/else`), where the
  nusys/papermario upstream sets `state = STOPPING` UNCONDITIONALLY. The tell — the target's FORCESTOP
  epilogue has TWO `sb v0,6(s0)` state stores with DIFFERENT values (`li v0,1` and `li v0,2`) — was in
  the asm all along; the "cross-jump shorter-by-N" symptom was the wrong body's block layout, not a
  merge wall (jump.c's `minimum=1` cross-jump needs only ONE matching insn before the epilogue label,
  so LAYOUT, driven by the body, controls it). Fixed the one branch → byte-identical `.text` + full ROM
  SHA-1 == baserom, NO compiler change. `nucontrmbmgr.c` is now a pure-C md5-candidate; **libnusys is
  100% C.** Lesson folded into `#cross-jump-tail-merge` (rule out a game-modified body before declaring
  a compiler wall: read the target's store SEQUENCE + VALUES) and the CLAUDE.md sub-100-coddog hedge.
  S121 investigation evidence remains at `nonmatchings/contRmbControl/sn_crossjump_investigation/`.)_

- _(Near-free retry (S118 deferral) — `func_800A2090` (0x7D490, 8B empty stub) **RESOLVED + banked
  S119** — the carry-over's completeness checklist replayed verbatim-correct, 0 rework: flip
  `[0x7D490,asm]`→`[0x7D490,c,libnusys/mainlib/func_800A2090]`, body `void func_800A2090(void){}` (KMC
  `-O` → `jr $ra; nop` = 8B), kept `func_` name (zero symbol adds), and the one flagged risk held — the
  8B tail IS the `trailing-pad:8B@16` to `nucontgbpakmgr`@0x7D4A0, landed clean on the first full-make
  ROM SHA-1. Confirmed NOT `nuContRmbForceStopEnd` (no `nuSiSendMesg` call); unidentified empty fn. Banked
  with its GBPak sibling `nuContGBPakFread`; the old `[0x7D3B0]` RMB block is fully cleared.)_

- _(S120 split remainder — `[0x7DB80, asm]` = `func_800A2780` leaf + `nusimgr.c` (5 fns) **RESOLVED +
  banked S122** — the whole subseg banked atomically as one md5-candidate `src/libnusys/mainlib/nusimgr.c`.
  Both open questions resolved at execution. The leaf `func_800A2780` returns `&siMgrStack` (0x800F77D0
  = siMgrThread 0x800F7620 + 0x1B0), a nusimgr.c file-static → provably SAME-TU (an MG64-added leading
  accessor the 2.07 upstream lacks), banked WITH nusimgr (`return siMgrStack;`, kept func_ name); the
  "foreign micro-TU" framing was wrong (now the `#multi-function-segment-splitting-pack` leaf-returns-
  static rule). The expected drop-static/drop-def resolution held (nuSiMesgBuf=0x800F7600 / siMgrThread=
  0x800F7620 / siMgrStack=0x800F77D0 drop-static; nuSiMgrMesgQ=0x8012D408 drop-def + symbol_addrs;
  nuSiMesgQ/nuSiCallBackList already placed; all bss → no carve). ONE wrinkle the frame missed: a
  version-rev `needs-define` — vendored nusys-2.07 `NU_CONT_THREAD_ID=6` vs MG64's 5 (the osCreateThread
  thread-id), a single `li` immediate byte, fixed in nusys.h (sole consumer → collateral-free). Full-make
  ROM SHA-1 == baserom, 1 fix-iteration.)_

- _(Near-free retry — **libkmc `sin.c` (0x8E660) C-mirror** (`_xsincos`+`sin`+`cos`+`tan`) **RESOLVED +
  banked S113** — the carry-over's 5-point completeness checklist replayed verbatim-correct, 0 rework,
  0 iteration: text flip `[0x8E660,asm]`→`[0x8E660,c,libkmc/sin]`, verbatim cp of
  `~/development/repos/libkmc/src/sin.c`, carve `[0xADCA0,rodata]`→`[0xADCA0,.rodata,libkmc/sin]` (whole
  generic subseg, 0x90 B = 18 doubles), ZERO new symbols (all deps placed S112/S109), full-make ROM SHA-1
  == baserom first build. The "confirm no un-named `D_<addr>` beyond `_atbl`" check held; KMC GCC emits
  `__fixunsdfdi` (not `__fixdfdi`) for the signed `XLONG=double*MBIT` idiom (now `#compile-profiles`).
  **libkmc is now fully mined — only mmuldi3/mcvtld `hasm` remain.** The 3rd clean near-free-retry-checklist
  replay (S75 contquery / S93 xldtob / S113 sin.c).)_

- _(Near-free retry — xldtob tail `[0x8D480]` (`_Ldtob` + `_Ldunscale` + `_Genld`) **RESOLVED + banked
  S93** — the carry-over's 5-point checklist replayed verbatim-correct (0 rework): single text flip
  `[0x8D480,asm]`→`[0x8D480,c,libultra/libc/xldtob]`, verbatim ultralib VERSION_J cp, 0 edits, 0
  iteration, full-make ROM SHA-1 == baserom first build. The `.rodata` story was simpler than the
  checklist hedged: `pows[]` is `const` → `.rodata`-ONLY (no `.data` carve — the "check for file-static
  .data" was a no-op), and the generic `[0xADBD0,rodata]` subseg ALREADY bounded the exact 0x70 extent
  (vram 0x800D27D0→0x800D2840), so the carve was a 1-line attribute change, NO split. The carry-over's
  `rodata-literal:0x800D2820` carve-start under-stated the real start (0x800D27D0, the pows[] dlabel) by
  0x50 B → S93 #1 added the `defines_file_static_const_array`-gated carve-start widening so a const-array
  mirror prices the full extent at the gate.)_

- _(S89 reconciliation — `piacs.c` (`__osPiCreateAccessQueue`/`__osPiGetAccess`/`__osPiRelAccess`,
  0x800A39B0) is **ALREADY BANKED**, NOT a remaining trap. It is flipped + matching at
  `[0x7EDB0, c, libultra/io/piacs]` (drop-to-extern mirror — all 3 data symbols placed:
  `__osPiAccessQueueEnabled`=0x800C7EB0, `__osPiAccessQueue`=0x80106198, `piAccessBuf`=0x800FA9B0),
  green ROM since the `cbaf80a` 2026-06-13 layout refactor. The historical "piacs/motor traps remain"
  band refrain is stale on piacs; NOTE `func_800AC110` in the old pairing was a mislabel — that vram
  is `__osSiCreateAccessQueue`/siacs.c (banked S81), a different file. The genuine remaining io traps
  are `motor.c` + `pimgr.c` below.)_
- _(io `motor.c` carry-over **RESOLVED + banked S102** — flipped `[0x89780, c, libultra/io/motor]`,
  0 stubs, md5-candidate. The "osMotorStop version-branch trap" framing rested on a MISLABEL: the
  `osMotorStop` vram 0x800AE380 is actually `__osMotorAccess` (os_motor.h `#define osMotorStop(x)
  __osMotorAccess(...)`), banked with `osMotorInit` as the J-branch pair — no `#else` source needed.)_
- _(io `pimgr.c` (`osCreatePiManager`) carry-over **RESOLVED + banked S90** — the "mixed `.data`/`.bss`
  carve" spike framing was over-cautious, the same false-frame the vimgr S87 carry-over got. Under
  `-D_FINALROM -DBUILD_VERSION=VERSION_J` it compiles ONE fn; the only `.data` global
  (`__osCurrentHandle`) was already placed (S84) → it's a clean **drop-def mirror**, NO carve: all
  file-scope DEFs drop to `extern` (header provides `__osPiDevMgr`/`__osCurrentHandle`;
  `__osPiTable`/`__Dom*SpeedParam` unreferenced), and the 4 uninitialized file-statics drop-to-extern
  at asm-recovered `main_bss` vrams — `piThread`=0x800F97E0 (0x1B0), `piThreadStack`=0x800F9990
  (0x1000), `piEventQueue`=0x800FA990 (0x18), `piEventBuf`=0x800FA9A8 (0x4), the contiguous block just
  below piacs's `piAccessBuf`@0x800FA9B0. Byte-clean first build, 0 iteration. The earlier
  0x800FA990/0x800FA9A8 framed here as `__Dom*SpeedParam` were WRONG (those are piEventQueue/piEventBuf);
  `__Dom1/2SpeedParam` are at 0x80106248/0x800FEC98 per S85.)_
- _(`sched.c` head carry-over **RESOLVED + banked S106** — flipped `[0x86A50, c, libultra/sched/sched]`,
  0 stubs, md5-candidate (`osCreateScheduler` + 13 helpers). The stacked-hazard spike (file-static +
  defines-data:count,firsttime + rodata-jtbl:0x800D25C0 + 5 log callees) was worked through at the S106
  gate.)_
- _(os/exceptasm.s asm-mirror (`__osExceptionPreamble` + 7 dispatch fns, `[0x8AF90]`) carry-over
  **RESOLVED + banked S107** — the "both dead-ends proven, needs a novel mechanism" spike framing was
  an over-generalization. S91 proved 2 paths fail (strip-and-rename a SYMBOLIC table; carve a `hasm`
  `.o`'s rodata) but LISTED a 3rd untried option ("export the `.text` labels under names the blob
  references") — which works FIRST-TRY. The `__osIntTable` jtbl lives in its OWN already-address-placed
  rodata blob (`asm/data/AD9F0.rodata.s`) that survives the `.text` flip and keeps SYMBOLIC
  `.word .L800Bxxxx` refs; vendor `.text`-only + strip-rename the 5 tables (`__osHwIntTable`/
  `__osPiIntTable`/`__osIntOffTable`/`__osIntTable`/`__osThreadSave`@0x800FC6A8 0x1B0 bss) + re-export
  the 9 jtbl-target `.L800Bxxxx` labels in the vendored `.text` (mapped by instruction). All 8 fns
  pre-curated in ghidra_symbols. Full-make ROM SHA-1 == baserom first build, 0 iteration. Codified as
  the LABEL-EXPORT procedure in `docs/hazards.md#asm-mirror-vendoring` (the asm-mirror-jtbl class is no
  longer a spike). NB: the sched.c-head "same heavy-spike class" pairing was stale — sched banked S106,
  exceptasm S107.)_
- _(io `vimgr.c` (`osCreateViManager` + `viMgrMain`) carry-over **RESOLVED + banked S87** — the
  "heavy file-static `.bss` carve" framing was over-cautious. The 6 file-statics + 2 globals + 1
  func-local static are all UNINITIALIZED → pure `.bss` (no ROM bytes), so they DROP to sized
  `extern`s placed at recovered `main_bss` vrams (the S81 `siacs.c` drop-to-extern pattern), **NO
  carve, NO classical loop**. Banked first-build seed-only once S86 placed the timer-side deps.
  Codified: `docs/hazards.md#file-static-bss-layout-conflict` now splits uninitialized (pure-`.bss`
  drop-to-extern mirror) from initialized-nonzero (real `.data` carve), and `pick_target.py` emits a
  `drop-static-mirror:<n>bss` re-frame tag (coddog@≥99 + file-static + no carve signal).)_
- _(os/ `initialize.c` (`__osInitialize_common` + `create_speed_param`) carry-over **RESOLVED + banked
  S85** — turned out a drop-def mirror (NOT the framed cross-region `.data` carve; main_data provides
  the bytes) + one VERSION_K-gate un-gate (`__osSetWatchLo`, `docs/hazards.md#needs-define`) + a
  `#undef __osInitialize_common` for the os_host.h K→J shim (`docs/hazards.md#header-renames-symbol`).)_
- **Tooling follow-up (S98 #1, deferred half) — full per-member rodata carve attribution +
  label-bounded carve-end.** S98 shipped the conservative `;owner-per-member` marker (suffixed on a
  c-combined pack's `rodata-jtbl`/`rodata-literal` so the gate does not carve the PRIMARY `.c` by
  default) + the `docs/hazards.md#rodata-sibling-yaml-pattern` owner-confirm playbook, but DEFERRED the
  precise attribution: (a) scan each c-combined member function's body for the `%lo(D_<addr>)` /
  `%lo(jtbl_<addr>)` ref and tag the OWNING member stem (e.g. `rodata-jtbl:0x800D23E8@resample`) instead
  of the all-or-nothing marker — needs the fn→member-stem map plumbed into `append_upstream_hazards`
  AND `_append_recover_hazards` (the jtbl is emitted in the latter, which today gets no member context;
  also covers the coddog jtbl path); (b) tighten `_rodata_carve_end_vram` to stop at the first
  following rodata block whose `.word .L<addr>` entries leave the owning member's `[fn_start, fn_end)`
  range (S98 env's `carve-end=0x800D2410` over-ran into resample's already-carved 0x800D23E0..0x2410).
  Also worth a guard: `pick_target` reads a STALE `asm/<ROM>.s` after a flip/split (splat leaves the old
  per-ROM listing when a subseg flips to `c`), so a post-split re-run mis-scopes the scan — bound the
  subseg scan to the candidate's own `[off, off+size)` extent. Not file-blocking (the marker +
  link-error confirm cover the gate; the carve is `.o`-sized at execution). Touches the smallest-first
  ranker → off-cadence golden-gated work (tooling-refactor-style). Reuse the existing `member_paths`
  resolution + `iter_function_body` + the rodata REs already in `decomp_asm.py`.
- **Tooling follow-up (S92 #3, REVERTED at apply) — `.data`-carve detector for file-static
  INITIALIZED arrays.** S92's `_Litob` needed a `.data` carve for its `ldigs`/`udigs` digit tables
  (`static char[]="0123…"`), which NO detector flagged at the gate (only xldtob's `rodata-literal`
  surfaced; the asm ref is `addiu %lo(D_<addr>)` address-of, caught by neither the FP-load nor
  `lw`-load scan). A built `data_addr_refs` scan (`addiu %lo(D_)` band-filtered to the `.data`
  range) was **reverted** because it could not separate a file's OWN static (→ `.data` carve) from a
  SHARED cross-file extern referenced via the identical instruction (→ recover-extern): it over-fired
  on `0x800C8270` (a global shared between two scheduler-create files, osCreateScheduler +
  nuScCreateScheduler) and `0x800C7E30` (nuSiCallBackList, referenced cross-file by nuContGBPakFwrite —
  names un-backticked here so carry_over_names does not de-rank these real candidates). Excluding
  `refs-unplaced` addresses is necessary but
  INSUFFICIENT (refs-unplaced has cross-file gaps). A correct detector needs a file's-own-static vs
  cross-file-extern discriminator — likely SOURCE-based (scan the mirror's resolved upstream `.c` for
  `static <type> <name>[…] = <init>` initialized file-scope statics → flag a `.data` carve, address
  recovered from asm at the gate), but the c-combined/coddog `up_path` resolution can point at the
  WRONG member file (xlitob vs xldtob), so it needs the right per-member upstream first. Not
  file-blocking (the carve is a mechanical S38/S68 gate step once the asm is read; S92 banked it
  manually first-try). Dedicated tooling sprint — see `docs/hazards.md#coddog-cross-ref` step 6.
  **S101 2nd data point (confirms worth doing):** env.c's `eqpower[128]` (`static s16 eqpower[…] =
  {…}`, the file's ONLY `.data`) was likewise UN-flagged, recovered manually at execution from the
  asm (`s4 = lui 0x800d/addiu -0x7fa0` → 0x800C8060) and bounded exactly by the existing generic
  `[0xA3460,data]` subseg. This is exactly the `static <type> <name>[…] = <init>` source-scan case;
  AND env is a clean **single-file-pack** (post env|filter split), so the per-member `up_path`
  ambiguity that blocked S92 does NOT arise here — the source-based detector would have fired cleanly.
  The single-file-pack subset is the safe first slice to ship the detector on.
  **SHIPPED (single-file-pack subset) S104** — the source-based `data-carve:<names>` detector
  (`defines_file_static_init_array`: file-scope NON-const `static T name[]=init;`) now fires on the
  single-file (non `c-combined`) subset, the exact safe slice this note identified. 3rd data point was
  xprintf's spaces/zeroes. **Still DEFERRED: the c-combined/multi-file case** — a c-combined pack's
  per-member `up_path` can mis-attribute which member owns the `.data` array, the same blocker as the
  jtbl owner-per-member work (S98 #1). Gate that on the per-member upstream resolution landing first.
  **S106 4th data point + the SCALAR extension (deferred) —** sched.c's `.data` (0x10) is FOUR SCALAR
  initialized statics, NOT an array: file-scope `static int dp_busy=0; static int dpCount=0;` (depth 0)
  + function-local `static int count=0;` (`__scMain`) / `static int firsttime=1;` (`__scTaskReady`,
  depth>=1). `defines_file_static_init_array` matches only `[…]` arrays at depth 0, so all 4 were
  invisible; the carve was recovered manually at execution (asm `__scExec` `dp_busy=lui 0x800d/sw
  -0x7dfc` → 0x800C8204, so the .data base count=0x800C8200). NB: under KMC GCC, an EXPLICITLY-init
  scalar (even `=0`) lands in `.data` not `.bss` — so the detector key is "has `= <init>`", not
  "nonzero init"; at least one nonzero in the group (firsttime=1) anchors the whole .data block. To
  ship: a `FILE_STATIC_INIT_SCALAR_RE` (`static <type> <name> = <init>;`, no `[`, exclude `const`)
  scanned at ALL depths (function-local statics carve too), fired on the single-file-pack subset like
  the array detector; +unit test + golden regen. pick's advisory `defines-data:count,firsttime` WAS the
  gate signal (not a miss, just un-upgraded to `data-carve`), and the carve is a mechanical execution
  step, so NOT file-blocking. Off-cadence golden-gated (touches the ranker) — `tooling-refactor-style`.

- **Tooling follow-up (S108 #2) — multi-root asm-TU index so libkmc math/soft-float TUs surface as
  asm-mirror candidates.** Investigated at the S108 review: the existing intrinsic-likely path
  already tags ultralib hand-asm correctly (`interrupt.s` WAS tagged `intrinsic-likely:os/interrupt.s`
  pre-flip; its top-15 miss was the INTENDED `(-score,size)` de-prioritization of hand-asm, not a
  bug). The real gap is the remaining libultra-region inventory is **libkmc**, which `build_asm_tu_index`
  (ultralib-only scan) misses → it shows as bare `upstream none`: `__floatdidf`/`__fixunsdfdi`
  (0x8F140/0x8F020 → `libkmc/src/mcvtld.s`, asm-mirror) and `_xatan`/`_xsincos` (0x8E110/0x8E660 →
  `libkmc/src/atan.c`/`sin.c`, **C** mirror, not asm). Scope: (1) extend `build_asm_tu_index` to a
  multi-root scan (ultralib + libkmc), tracking each TU's source root; (2) make the downstream
  consumers root-aware — `vendorable_tu_missing_defines`/`_data_symbols`/`_jtbl` all
  `os.path.join(LIBULTRA, rel)` today and would mis-join a libkmc path; (3) optionally extend the C
  upstream index for libkmc math so `_xatan`/`_xsincos` get a libkmc C attribution. NOT file-blocking
  (the inventory is in `## Active phase` for the PO). Off-cadence golden-gated (touches the ranker +
  several coupled functions) — `tooling-refactor-style`. Do NOT rush this into a review gate.

- **Tooling follow-up (S101 #1) — suppress intra-pack `calls-unplaced` (calls-side dual of S66 #2).**
  `pick_target` flagged all 4 `calls-unplaced:__freeParam,_freePVoice,_frexpf,_ldexpf` on env, but
  `_frexpf`/`_ldexpf` are DEFINED as pack members (env.c fns 0x800A5978/0x800A5A58) → they self-resolve
  on a verbatim mirror and are pure noise (only `__freeParam`/`_freePVoice` are real cross-file
  recover-callees). Suppress a `calls-unplaced` callee whose name matches a resolved pack-member
  function — the calls-side analog of the deferred S66 #2 cross-member `refs_unplaced` union. Cheap:
  the member basename set is already computed for the `pack:Nfn[fn=basename,…]` column, so the filter
  is a set-membership check in `_append_recover_hazards` (the same place the jtbl owner-attribution
  lives). Not file-blocking (the intra-file callees self-resolve on mirror; the noise just over-states
  the gate cost). Off-cadence golden-gated (touches the ranker) — `tooling-refactor-style`.

- **Tooling follow-up (S72 #2) — optional `bare-assert` advisory flag.** `pick_target` could scan a
  mirror candidate's upstream `.c` for a non-`_DEBUG`-guarded `assert(` and flag it, so the
  `#assert-strip` `_DEBUG`-wrap is priced at the gate rather than rediscovered by the read==write
  size tell. Not file-blocking (the playbook covers it; the strip is a cheap in-execution edit).

- **Tooling follow-up (S88 #1) — `needs-macro:<MACRO>@<hdr>` AUTO-detector (deferred half).** S88
  shipped the `docs/hazards.md#vendored-header-incomplete` playbook + the manual gate check, but
  DEFERRED the automated `pick_target` detector: for a mirror candidate, confirm every function-like
  helper macro the upstream `.c` invokes is `#define`d (with compatible arity) in a resolvable header.
  The blocker is FP-avoidance — a naive `\b[A-Z_]{3,}\s*\(` grep can't tell a function-like macro
  invocation from a real call or an enum-constant-in-a-macro, so it needs cpreprocess-grade macro
  extraction from the headers (object- vs function-like) + arity compare. Worthwhile because several
  remaining io/os mirrors use the same reconstructed `controller.h`/`siint.h`; a new macro one of them
  is the FIRST to use would re-trip the S88 mid-execution parse-error. Not file-blocking (the playbook
  + manual grep covers it; the macro is a cheap header-align edit). Reuse `cpreprocess` define-line
  parsing + the existing `C_CALL_RE` call-scan, both already imported by `pick_target`.

- **asm-mirror vendoring — remaining intrinsic-likely libultra TUs (S56 pilot + S57 cache/TLB + S58
  cross-dir + S62 combined-subseg split + S63 mixed combined-subseg+C-mirror clear (reg-shim "set"
  family done: setfpccsr/setsr/setwatchlo) proved the pattern repeatable).** S56 vendored the 4 single-instr reg shims
  (`getcount`/`getcause`/`getsr`/`setcompare`); S57 vendored the 4 cache/TLB primitives
  (`osWritebackDCacheAll`/`osWritebackDCache`/`osUnmapTLBAll`/`__osProbeTLB`, all first-try clean) via
  the KMC-gcc `VENDOR_ASM` Makefile mechanism (assemble ultralib `.s` with `LIBULTRA_ASFLAGS`; KMC `as`
  pads each fn's `.text` to its 16-byte ROM slot, which modern `as` does not). The pattern extends by
  adding `<rom>:src/libultra/<dir>/<file>.s` pairs to `VENDOR_ASM`, vendoring the ultralib TU verbatim,
  and flipping the subseg `asm`→`hasm`. Each new TU still needs per-TU ROM-SHA-1 verification (version
  mismatch → SHA break; bisect per `docs/hazards.md#asm-mirror-vendoring`). `pick_target.py` now
  pre-flags any `needs-define:<MACROS>` on the `intrinsic-likely:<tu>.s` hazard (S57 #1) so a
  missing-macro enabler is priced at the gate — the whole list below verified self-contained (0
  missing). Remaining, smallest-first:
  - ~~**single-fn primitives:** `func_800ACCC0` (0x880C0), `func_800ACB40` (0x87F40)~~ **BANKED S70** —
    both were identified at the gate by their CP0/TLB signature: `func_800ACB40` = **`osMapTLB`**
    (`os/maptlb.s`), `func_800ACCC0` = **`osUnmapTLB`** (`os/unmaptlb.s`). One MCP `disassemble_function`
    each (mfc0/mtc0 Index/EntryHi/EntryLo + tlbwi); the "no identified ultralib `.s` yet" was the only
    blocker and it was trivial — the carry-over over-stated the difficulty for 14 sprints. Vendored
    verbatim (the `_DEBUG && __sgi` block compiles out under KMC), 2 `VENDOR_ASM` pairs, 2 curated names,
    full-make ROM SHA-1 == baserom first try, 0 iteration. S70 #1 added `pick_target.py`'s `privileged_asm`
    fingerprint so a future un-named privileged-asm subseg self-flags `intrinsic-likely:cp0-asm(identify-TU)`
    instead of parking as a bare no-source shim.
    _(S58 banked the 3 source-confirmed single-fn TUs: `sqrtf` 0x8BE10, `osMapTLBRdb` 0x8CD10, `bcopy`
    0x85DA0 — all first-try clean; bcopy's `#ifdef __sgi`→`_bcopy=bcopy` path and sqrtf's auto-filled
    `j ra` delay slot both held without special handling.)_
  - **partial-TU split (asm members share ONE ultralib `.s`):** `__osDisableInt`+`__osRestoreInt`
    (0x8B900) — both live in ultralib `src/os/setintmask.s` *together with* `osSetIntMask`, so this is a
    partial-TU carve/split off that shared TU (the same shape as the mixed-pack item below), NOT a clean
    "source not located" case (the S58 wording was wrong — the source exists, the difficulty is isolating
    two of three fns from one `.s`). `pick_target`'s `combined-subseg:<n>tu[…]` flag (S62 #1) does NOT
    fire here (single distinct TU). Spike.
    _(S62 banked the one clean combined-subseg pack — `osInvalDCache`+`osInvalICache` (0x823B0): two
    asm-ONLY fns in distinct ultralib `.s` files (`os/invaldcache.s`+`invalicache.s`), split at 0x82460
    into two `hasm` subsegs + two `VENDOR_ASM` pairs, first-try clean. `combined-subseg:2tu[…]` is the
    pre-flag for this shape.)_
  - **DO NOT blanket-hasm (mixed packs with real C mirrors):** ~~`osSetIntMask` pack:3fn (0x7E360 —
    also holds `osCreatePiManager`/pimgr + `__osEPiRawStartDma`/epirawdma; split first, hasm only
    `osSetIntMask`)~~ **2 of 3 BANKED S84** — split 3-way at the gate; `osSetIntMask` vendored
    `.text`-only as `hasm` (the FIRST vendored `.s` carrying a `.rodata` LUT, `__osRcpImTable`; see the
    has-rodata sub-case in `docs/hazards.md#asm-mirror-vendoring`), `__osEPiRawStartDma` C-mirrored;
    only `pimgr` (osCreatePiManager) remains → carry-over below. `func_800AFB90` pack:8fn (0x8AF90 —
    exception/thread dispatch block; its own decision; note it loads the now-named `__osRcpImTable`).
    Stays an asm flip-candidate for now.
- **~~C-mirror next-cleanest leaves buried in gu combined subsegs (S64)~~ — GU BAND FULLY CLOSED at
  S69** (cosf+sinf S66, translate S67, align S68, lookat S69; no gu mirrors remain). History retained:
  - ~~**`cosf` (gu/cosf.c)** + **`sinf` (gu/sinf.c)**~~ **BANKED S66** — both verbatim ultralib
    VERSION_J mirrors. cosf split out of `[0x82B80]` at 0x82F20+0x83070, sinf out of `[0x85B30]` at
    0x85CD0. The first C `#pragma weak` mirrors (compiled clean, no edit; `pick_target` now resolves
    weak aliases so the `cosf=?` mislabel is gone). The **named-rodata CAVEAT was a phantom** (splat
    carves a `.rodata` subseg cleanly with `ghidra_symbols` labels inside — see
    `docs/hazards.md#rodata-sibling-yaml-pattern`). Shared gate-missed recover-extern
    `__libm_qnan_f`=0x800D2640 (the NaN-path return) recovered in-execution.
  - ~~**`gu/translate.c` (guTranslateF + guTranslate, 0x85CD0)**~~ **BANKED S67** — verbatim ultralib
    VERSION_J mirror, byte-identical cp, 0 iteration, full-make ROM SHA-1 == baserom first try. Zero
    enablers beyond the yaml flip (names pre-curated, `guint.h` vendored, callees placed, no float
    literals → no rodata split). The `pack:2fn` flag was the single-upstream-file false-flag class
    (now tagged `single-file-pack` by `pick_target.py`, S67 #2). (S67 said "gu band fully
    decompiled"; corrected at S68 — `gu/align.c` 0x82B80 banked S68, `gu/lookat.c` 0x83070 remains.)
  - ~~**`gu/align.c` (guAlignF + guAlign, 0x82B80)**~~ **BANKED S68** — a known-edit near-verbatim
    mirror, the **verbatim twin of S61 rotate.c**. The `calls-unplaced:guNormalize` →
    `vec3f_normalize` substitution (game-region fn @0x80029900) + the `static float dtor` `.data`
    sibling `[0xA35A0]` were exactly rotate's playbook → first-build match, 0 iteration. The
    substituted-mirror class is mechanical, not "harder," when a banked dir-sibling pins the playbook
    (now surfaced by `pick_target.py twin-of:<file>`, S68 #2). `guAlign` inlined guAlignF (-O2 same-TU).
  - ~~**`gu/lookat.c` (guLookAtF + guLookAt, 0x83070)**~~ **BANKED S69** — the last un-flipped gu
    leaf; banking it closes the gu band. **CORRECTION:** NOT the `vec3f_normalize` substitution class
    (this note was wrong) — guLookAtF uses `sqrtf` **inline** (`-1.0/sqrtf(...)`), so it's a PURE
    verbatim mirror like S64 lookathil, callees guMtxIdentF/sqrtf×3/guMtxF2L all placed, 0 edits, 0
    iteration. guLookAt inlines guLookAtF (-O2 same-TU). pts-13 (1808B) tripped the 8-gate → ran under
    the **verbatim-mirror exemption (S64, generalized S69)**: the inner boundary guLookAt@0x800A7FF0
    is 16-aligned (unlike lookathil), but it's a `single-file-pack` so decompose-blocked anyway. rodata
    sibling carve `[0xAD8C0, .rodata, libultra/gu/lookat]` = .o(.rodata) 0x20 exact (anon pool
    0x800D24C0..0x800D24E0). guLookAt=0x800A7FF0 added to symbol_addrs at gate.
- **Tooling follow-up (S66 #2, deeper half) — cross-member `refs_unplaced` scan.** `pick_target.py`
  now keys weak aliases (`PRAGMA_WEAK_RE`, so `cosf=cosf`), but `refs_unplaced` still scans only a
  pack's PRIMARY upstream — a hidden member's `__`-prefixed data extern (S66 `__libm_qnan_f`, refd by
  cosf/sinf but the pack primary is align.c) is missed at the gate. Union `refs_unplaced` over the
  resolved `c-combined` member upstreams so the recover-extern is priced at the gate, not discovered
  at execution-time data-ref reconciliation. Not file-blocking (recover-extern is cheap in-execution).
- _(osAiSetFrequency carry-over resolved and banked at S38 retroactive review)_
