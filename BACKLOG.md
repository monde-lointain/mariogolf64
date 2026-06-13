# Mario Golf 64 decomp product backlog

The Product Owner (you) orders this. The live candidate list is **not** duplicated here —
target selection is `tools/pick_target.py` (smallest-first ranker, run at `/sprint-plan`).
This file holds only the *ordering rationale*, *enabler items* (gate actions), and
*carry-overs*. `/sprint-plan` re-refines the slice each sprint;
`/sprint-review` reprioritizes. See `CLAUDE.md ## Scrum operating model` for the cadence.

## Active phase / epic

**Epic 1 — complete the libultra function set (mirrors + classical), smallest-first.** All
remaining libultra functions are in scope: verbatim mirrors, known-edit sub-cases (near-verbatim
drop, file-static drop), recover-extern mirrors, and classical-loop targets. libkmc and libnusys
remain pickable fillers when needed to hit the ≥5pt sprint target (S26 directive), but are
subordinate to the libultra goal. Target selection is `tools/pick_target.py` (smallest-first);
the 8-point decompose gate fires on any seed ≥8. v2 classical track is active (since S11);
mirror is the default, classical is first-class when the asm warrants it.

**Remaining libultra hazard map (S38).** Clean warm mirrors still available in open bands
(`monegi/ai/`, `nintendo/pi/`, `monegi/thread/`). **blk** — `alHeapDBAlloc` (needs
`os_internal.h`/`ultraerror.h` companion copies to advance the `audio/` sub-band beyond the S36
pair). **defines-data (classical)** — `__osViInit` (3pt, defines placed BSS globals, static drops
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

## Carry-overs (spiked files/clusters awaiting the next sprint)

Files with a timeboxed spike (a function that locked < 0.97 percent, needs permuter, or hit
a BSS-layout-conflict). `tools/pick_target.py` de-ranks BACKLOG carry-overs so they stop
resurfacing; `/sprint-plan` re-pulls them first when retrying.

_(none — osAiSetFrequency carry-over resolved and banked at S38 retroactive review)_
