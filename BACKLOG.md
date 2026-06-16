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
- **Near-free retry** — NOT blocked; a fully-scoped increment deferred only by the sprint cap (e.g. a
  coddog-mirror sibling, or the un-flipped head of a split subseg). Author it as a **completeness
  checklist** so the retry is a mechanical replay (S74→S75 `contquery` proof: all 4 addresses
  verbatim-correct, 0 rework): **(1)** the exact subseg flip/split line; **(2)** the placed-ref
  inventory (callees + data externs already resolved, with the sprint that placed them); **(3)** any
  NEW recover-extern / callee vrams to add, each WITH its confirmed address; **(4)** the include
  adaptation vs upstream; **(5)** the upstream pin (file + VERSION_J). A near-free retry missing any
  of these is a half-scoped spike — finish the scope before deferring.

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
- **io coddog-mirror trap — `motor.c` (`osMotorStop` pack:2fn, [0x89780, asm]).** A 99.99% coddog
  match but NOT an atomic verbatim cp — a **version-branch trap**. Upstream `motor.c`'s
  `#if BUILD_VERSION >= VERSION_J` branch defines `__osMotorAccess`+`__osMakeMotorData`+`osMotorInit`
  and does **NOT** define `osMotorStop` (or `osMotorStart`) at all — those live only in the `#else`
  (`< VERSION_J`) branch. So a verbatim VERSION_J mirror of `motor.c` yields the WRONG function set;
  the ROM's `osMotorStop` corresponds to the older `#else` source, conflicting with the project's
  VERSION_J pin. `pick_target`'s hazards mix both branches (`READFORMAT`/`SELECT_BANK` are J-only,
  `__osMotorinitialized` is else-only — cpreprocess does not version-gate motor.c here). Do NOT
  pursue `osMotorStop` as a routine drop-static/drop-def mirror; it needs the `#else`-branch source
  (a per-file build-version override or a vendored old-branch copy), a distinct un-priced enabler.
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
- **sched.c head — `osCreateScheduler` + ~13 sched fns, `[0x86A50, asm]` (0x800AB650..0x800AC060,
  the head left by S89's sched|sirawdma decompose).** Spike (heavy) — a GENUINE carve/recover spike,
  not the pimgr/vimgr over-frame: it is NOT a single drop-def. Coddog `src/sched/sched.c`@99.99 but a
  stacked-hazard mirror: **file-static** + **defines-data:count,firsttime** (apply the S87
  drop-static-mirror test at the gate — uninitialized→drop-to-extern, nonzero-init→carve; verify
  `count`/`firsttime` placement before assuming a carve) + a **`rodata-jtbl:0x800D25C0`** switch table
  (needs the `.rodata` sibling carve, `docs/hazards.md#rodata-sibling-yaml-pattern`, S76 devmgr
  pattern) + **5 calls-unplaced log callees** (`osCreateLog`/`osDpSetNextBuffer`/`osFlushLog`/
  `osLogEvent`/`osSpTaskYielded` — recover or place from the still-asm log/sp subsegs) +
  `jal-count-mismatch:12vs11` (verify at the gate — likely an indirect-call false-positive class).
  Header `PRinternal/siint.h` already vendored. Pursue when the data-sibling + log-callee enablers
  are the sprint goal; the sirawdma tail is already off it (S89). seed ~13 → the 8-gate applies
  (single-file once split, but the carve+jtbl+5-recover load is real work).
- **os/exceptasm.s asm-mirror — `__osExceptionPreamble` pack:8fn, `[0x8AF90, asm]`
  (0x800AFB90..0x800B0A20, 0x970).** SPIKE (S91 gate-investigated, then carried). The OS exception/
  thread-dispatch core: under VERSION_J+_FINALROM the 8 active fns are `__osExceptionPreamble`/
  `__osException`/`send_mesg`/`__osEnqueueAndYield`/`__osEnqueueThread`/`__osPopThread`/
  `__osDispatchThread`/`__osCleanupThread` (the `__pt*`/`handle_CpU`/`__osNop` gate out). The `.text`
  =0x970 EXACTLY matches the subseg (high source confidence; J/_FINALROM flags correct), and MOST of it
  strips clean — the `.data` tables `__osHwIntTable`=0x800C9480 (0x28) / `__osPiIntTable`=0x800C94A8
  (0x8) and the `.rodata` byte-table `__osIntOffTable`=0x800D25F0 (0x20) are plain S84 strip-and-rename;
  8 of 9 externs are already placed (only **`__osThreadSave`=0x800FC6A8** needs adding — it's the
  `la k0` at `__osException` start, splat-labelled `D_800FC6A8`). **Sole blocker = `__osIntTable` /
  `jtbl_800D2610`** — a switch jump table the active `__osException` does an indexed `jr` through.
  splat extracts it to the separate rodata blob `asm/data/AD9F0.rodata.s` SYMBOLICALLY
  (`.word .L800B00A0 … .L800AFDFC`, pointers to `.text`-internal labels). Vendoring the `.text` as
  hasm makes `asm/8AF90.s` vestigial → those `.L800Bxxxx` labels vanish → the blob's refs go UNDEFINED
  at link. A rodata-sibling carve ALSO fails — the `VENDOR_ASM` `.o` is `build/asm/8AF90.o` (no
  src-path subseg to address-place its rodata; a hasm `.o`'s rodata auto-links at the section END →
  wrong addr → SHA break). **Both dead-ends proven (S91).** NEEDS a novel placement mechanism (export
  the `.text` labels under names the jtbl blob references, OR make the jtbl literal via reloc config,
  OR a non-VENDOR_ASM way to carve a hasm `.o`'s rodata to a fixed addr) — a dedicated spike-sprint
  goal, NOT a routine asm-mirror. `pick_target.py` now flags it
  `intrinsic-likely:os/exceptasm.s(...)(asm-mirror-jtbl:__osIntTable)` (S91 #2) so it stops reading as
  an S84 has-rodata replay; the `has-rodata:__osCauseTable_pt` over-count is also fixed (S91 #1 — that
  symbol is `#ifndef _FINALROM`-gated, excluded here). Same heavy-spike class as the sched.c head
  (jtbl + multi-recover), distinct mechanism (the jtbl-vs-vendored-.text bind, not a `.data` carve).
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
