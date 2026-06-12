# Mario Golf 64 decomp product backlog

The Product Owner (you) orders this. The live candidate list is **not** duplicated here —
target selection is `tools/pick_target.py` (smallest-first ranker, run at `/sprint-plan`).
This file holds only the *ordering rationale*, *enabler items* (gate actions), and
*carry-overs*. `/sprint-plan` re-refines the slice each sprint;
`/sprint-review` reprioritizes. See `CLAUDE.md ## Scrum operating model` for the cadence.

## Active phase / epic

**libultra/libkmc upstream-mirror band, smallest-first.** Reposition known SDK functions
verbatim (the upstream-mirror branch of the execution loop); reserve one no-upstream
classical-loop target per few sprints to validate the *classical* match-loop (still unproven —
every win below was an upstream mirror). Sprints capped small (1 upstream file, or ≤ ~3–4 functions) until a per-sprint
count emerges.

**PO ordering note (S7 #2 — RESOLVED at S9).** The standing "schedule one classical target"
recommendation was executed in **Sprint 9** (`func_80099490`, the first `--upstream none` match) —
the classical match-loop is now **proven**. But the spike matched first-pass-clean with **zero
residual variance**, so the v2 activation remains uncalibratable and was **deferred** at the S9
review. See the **`## PO ordering note (S9 retro …)`** below for the live guidance: the next
classical target must be **non-trivial** (real arithmetic/branches/locals) to generate the variance
v2 needs; clean mirror leaves remain the near-zero-risk default between classical spikes.

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
  re-price them next gate, likely another zero-enabler pair.**

## PO ordering note (S9 retro — defer v2, schedule a non-trivial classical next)

The S9 classical spike **proved the loop mechanically but produced zero residual variance**
(a clean verbatim-shaped wrapper), so the PO **deferred v2** at the S9 review — turning on the
realized-tier machinery against a point mass would measure nothing. To unlock v2, the next
classical target should be **non-trivial**: a small no-upstream fn with real arithmetic,
branches, or locals (NOT a thin wrapper or a register/FPU intrinsic — the new `intrinsic-likely`
hazard now filters the latter out of the smallest-first pick). Mining clean mirror leaves
(`__osDequeueThread`/`osYieldThread` in the now-warm thread band, or the cold convert/other
bands) remains the near-zero-risk default between classical spikes.

## Enabler items (gate-time, agent-performed since 2026-06-11)

These are the gate-time enabler actions a sprint may need before its execution middle can run
flip-free. `/sprint-plan` lists the ones a proposed sprint requires; since 2026-06-11 the
**agent** performs them at the plan gate (subseg flip/split, `symbol_addrs.txt` add) after PO
scope approval, and the gate validates with `make extract && make` (green ROM). (Pre-2026-06-11
these were USER actions performed by the PO.)

- [ ] _(none queued — `/sprint-plan` fills this per sprint)_

## Carry-overs (spiked files/clusters awaiting the next sprint)

Files with a timeboxed spike (a function that locked < 0.97 percent, needs permuter, or hit
a BSS-layout-conflict). `tools/pick_target.py` de-ranks BACKLOG carry-overs so they stop
resurfacing; `/sprint-plan` re-pulls them first when retrying.

- [ ] _(none)_
