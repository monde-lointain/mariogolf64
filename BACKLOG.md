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
  - **single-fn primitives:** `func_800ACCC0` (0x880C0), `func_800ACB40` (0x87F40) — both un-named
    `func_<addr>` with no identified ultralib `.s` yet; identify the TU (else a bare `intrinsic-likely`
    no-source shim → plain `hasm`) + curate the name before vendoring.
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
  - **DO NOT blanket-hasm (mixed packs with real C mirrors):** `osSetIntMask` pack:3fn (0x7E360 —
    also holds `osCreatePiManager`/pimgr + `__osEPiRawStartDma`/epirawdma; split first, hasm only
    `osSetIntMask`); `func_800AFB90` pack:8fn (0x8AF90 — exception/thread dispatch block; its own
    decision). These stay asm flip-candidates for now.
- **C-mirror next-cleanest leaves buried in gu combined subsegs (S64; cosf+sinf BANKED S66).**
  - ~~**`cosf` (gu/cosf.c)** + **`sinf` (gu/sinf.c)**~~ **BANKED S66** — both verbatim ultralib
    VERSION_J mirrors. cosf split out of `[0x82B80]` at 0x82F20+0x83070, sinf out of `[0x85B30]` at
    0x85CD0. The first C `#pragma weak` mirrors (compiled clean, no edit; `pick_target` now resolves
    weak aliases so the `cosf=?` mislabel is gone). The **named-rodata CAVEAT was a phantom** (splat
    carves a `.rodata` subseg cleanly with `ghidra_symbols` labels inside — see
    `docs/hazards.md#rodata-sibling-yaml-pattern`). Shared gate-missed recover-extern
    `__libm_qnan_f`=0x800D2640 (the NaN-path return) recovered in-execution.
  - **`gu/translate.c` (guTranslateF + guTranslate, 0x85CD0)** — NEWLY SURFACED by the sinf split
    (`[0x85CD0,asm]` 2-fn pack, the former sinf-subseg tail). The next-cleanest gu mirror candidate;
    re-price at the next gate.
  - NB: the align/lookat tails of the old 0x82B80 subseg (now `[0x82B80,asm]` guAlignF + `[0x83070,asm]`
    guLookAtF) are NOT clean mirrors — they carry the `calls-unplaced:guNormalize` →
    `vec3f_normalize` substitution (S61) + a `data-static`; classical or substituted-mirror, harder.
- **Tooling follow-up (S66 #2, deeper half) — cross-member `refs_unplaced` scan.** `pick_target.py`
  now keys weak aliases (`PRAGMA_WEAK_RE`, so `cosf=cosf`), but `refs_unplaced` still scans only a
  pack's PRIMARY upstream — a hidden member's `__`-prefixed data extern (S66 `__libm_qnan_f`, refd by
  cosf/sinf but the pack primary is align.c) is missed at the gate. Union `refs_unplaced` over the
  resolved `c-combined` member upstreams so the recover-extern is priced at the gate, not discovered
  at execution-time data-ref reconciliation. Not file-blocking (recover-extern is cheap in-execution).
- _(osAiSetFrequency carry-over resolved and banked at S38 retroactive review)_
