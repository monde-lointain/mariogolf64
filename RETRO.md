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
- What helped: <levers / upstream-mirrors / tools that worked>
- Friction: <what slowed it down>
- Applied: <PO-selected, N of M: #1 … #k …; (#j … NOT selected)>
- Carry-over: <spiked files + blocking function>
```

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
