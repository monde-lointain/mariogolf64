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
