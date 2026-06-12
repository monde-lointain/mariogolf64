# Mario Golf 64 decomp sprint retros

One short digest per sprint, newest first, written at the `/sprint-review` gate. This is
the *consolidated* record so lessons compound across sprints (McConnell, *More Effective
Agile*, Ch.19: retros must change behavior). The PO promotes durable lessons into the memory
dir manually — the review gate does not auto-write memory.

Process/tooling edits are **retro-gated**: the "Suggested workflow improvements" the
execution loop records into `SPRINT.md` are applied here, at review, not mid-sprint (see
`CLAUDE.md ## Scrum operating model`). The `Applied:` line below is the audit trail of which
numbered suggestions the PO accepted.

## Entry format

```
## Sprint N — <goal> — <date>
- Increment: <files banked> / <functions matched> (delta: <pct before → after>)
- Quality: <stuck-far>/<permuter>/<carried>/<re-opened> this sprint
- Seed: committed <N>pt; banked <B>pt; regime <mirror|classical|mixed>   (v1 — story points; realized tier is v2)
- What helped: <levers / upstream-mirrors / tools that worked>
- Friction: <what slowed it down>
- Applied: <PO-selected, N of M: #1 … #k …; (#j … NOT selected)>
- Carry-over: <spiked files + blocking function>
```

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
