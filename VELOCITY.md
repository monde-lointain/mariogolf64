# Mario Golf 64 decomp sprint velocity (story points)

The story-point dashboard (activated 2026-06-11; PO directive at the Sprint-5 review — the
"phase 2" named trigger in `CLAUDE.md`, fired at the ≈5-sprint mark). Scale = **Fibonacci
1, 2, 3, 5, 8, 13**. Scope = the **current phase: the libultra/libkmc upstream-mirror band**
(`regime: mirror`). The classical/game-code regime gets a **separate track** when it starts —
never compare velocity across regimes (McConnell, *More Effective Agile*, Ch. 19).

**Ported from `../marioparty7/VELOCITY.md`** (23-sprint sibling system), but **shipped in two
phases** because MG64 differs from MP7 (its cost axis is deterministic and its functions are
tiny/uniform — see `CLAUDE.md ## Story points`):

- **v1 (now): deterministic seed + the 8-point decompose gate.** This is the part with signal
  today. The seed is a pure function of `tools/pick_target.py`'s columns, so it is
  **re-derivable** — it needs no anti-retrofit freeze commit, and the plan gate creates no
  commit. The committed seed is logged here in the normal review/retro commit.
- **v2 (ACTIVE since Sprint 11, 2026-06-11): the realized-tier + residual + rolling/re-anchor
  machinery.** Was dormant until the **first classical or mixed sprint** produced real residual
  variance to calibrate against (turning it on against the 5 identical verbatim mirror sprints
  would only measure a point mass). S11 (`func_800AB600`) was that sprint — a non-trivial
  classical leaf whose seed compiled at 0.80 and matched only after a fix-iteration — so the PO
  activated v2 at the S11 review. The realized tier is now scored per-sprint on the **classical
  track**; the mirror track stays seed-only (still a point mass there).

## Purpose — what the v1 number actually changes
1. **8-point decompose gate (primary).** A seed of **8 or 13 means do NOT commit it as a normal
   1-increment sprint** — decompose it (split the subseg at the upstream-file/function boundary)
   or pull a scaffolding enabler as the sprint goal. Catches an all-or-nothing bank stall
   *before* a sprint sinks into a too-big unit. (Most remaining classical units seed 8/13 — see
   Regime — so this gate will fire on nearly every sprint once the mirror band is mined out.)
2. **Regime indicator (primary).** A sustained seed/velocity shift flags the mirror→classical
   transition so the PO re-plans cadence and triggers v2.
3. **Forecasting — omitted in v1.** The backlog is overwhelmingly un-pointed classical work, so
   a remaining-points / velocity number would be a false deadline. Added with the classical
   track in v2, and even then "tertiary, noisy, not a deadline" (book Ch. 20).

## Seed rubric (deterministic; `tools/pick_target.py seed_points()`, the `pts` column)
Pure function of `size, upstream, band, nfns, hazards` + a copyable-vs-blocked header classify.
**Display-only — does NOT change the smallest-first sort.** A **cluster** seed = the **sum** of
its files' seeds; points bank **per file** (a spiked sibling scores 0 for that file only, not
the whole cluster — so sibling-pair batching is never punished).

```
blk  : a needs-header that can't be fixed by a companion-copy (unindexed -I / system header,
       e.g. <libaudio.h>, guint.h) → un-pickable, a DoR reject (not a cheap seed)
13   : size>=1536, or any classical multi-fn pack                  → must decompose
8    : nfns>=4 (large pack, any path), or a classical fn that is big/pack/drop/needs-copy
5    : a small single classical fn (no upstream — unproven, high variance)
mirror (has upstream): base = 1 if warm else 2;  +1 each for {static/data-drop, header-copy,
       needs-define, recover-extern, pack-or-big};  snap to Fibonacci (4→5, 6→8)
```
`drop` = `file-static`/`defines-data` (→ classical fallback, defs dropped). `warm` = the mirror
dir already holds a banked sibling (enabler-free). `big` = size≥768 B, `huge` = ≥1536 B — the
byte gates are **dormant** in the current <256 B regime and only bite for large/classical fns.

## Realized tier + residual loop — ACTIVE (v2, since Sprint 11)
Scored at review from the RETRO facts (start at the seed band; +1 per
{stuck-far / permuter / re-attempt / novel bank-gotcha / mid-sprint split / carry-or-reopen};
−1 verbatim first-try ≤1 fix-iteration; per-file then summed). With it come the two-pass
**freeze** (plan-time seed committed before `src/`; realized in a second commit). **Caveat
(do not over-credit the freeze):** the freeze locks only the deterministic *seed*; the realized
tier is **agent-scored and subjective**, so the real anti-gaming guards are the **per-file
all-or-nothing bank + the quality counter-metric** (stuck-far / permuter / carried / re-opened),
not the freeze. **Scope:** applied on the **classical track only** — the mirror track has no
residual variance to score (still a point mass), so it stays seed-only. **S11 worked example:**
seed 5, no escalations, single fix-iteration on a non-verbatim leaf → realized 5, residual 0.

## Bootstrap anchors (retro-pointed S1–5; the calibration loop, pre-run)
All `regime: mirror`, all tiny verbatim leaves (<256 B). `seed` = the rubric (a-priori, **live
in v1**). `(real)` is the v2 realized rule, shown *italic/illustrative* only — **not** logged
until v2. These rows double as the **reference stories** for the plan-time ±1 adjust.

| Sp | increment | nfns | path / band / hazard | seed | *(real)* | note |
|----|-----------|------|----------------------|------|----------|------|
| 1 | dp.c | 1 | mirror / cold / — | 2 | *2* | verbatim vs cpp-toolchain gotcha |
| 2 | createmesgqueue.c | 1 | mirror / cold / — | 2 | *2* | verbatim vs `__osThreadTail` asm-recovery |
| 3 | visetmode.c | 1 | mirror / warm / needs-copy | 2 | *1* | trivial `assert.h` copy |
| 4 | sirawread + sirawwrite | 1+1 | mirror / warm / — | 1+1=2 | *2* | both verbatim, warm floor 1 each |
| 5 | viswapbuf + visetevent | 1+1 | mirror / warm / — | 1+1=2 | *2* | both verbatim, warm floor 1 each |
| 6 | viblack | 1 | mirror / warm / — | 1 | *1* | first live-logged v1 sprint; verbatim, zero-enabler, banked 1pt |
| 7 | virtualtophysical | 1 | mirror / cold / — | 2 | *2* | opens convert band (vi exhausted); verbatim, zero-enabler, banked 2pt |
| 8 | stopthread | 1 | mirror / warm / — | 1 | *1* | opens thread band; verbatim, zero-enabler, banked 1pt |
| 9 | func_80099490 | 1 | **classical** / — / — | 5 | *—* | **first classical-track row** — no-upstream wrapper `nuPiInitSram()`; first-pass clean (score 0, 0 iter), banked 5pt; classical loop PROVEN. Kept OUT of the mirror seed-velocity (separate regime) |
| 10 | dpsetstat + dpctr | 1+1 | mirror / warm / — | 1+1=2 | *2* | 5th sibling-pair; split 0x86730 pack (osDpSetStatus+osDpGetCounters) at upstream-file boundary; both verbatim, zero-enabler, banked 2pt. 9th straight clean mirror |
| 11 | func_800AB600 | 1 | **classical** / — / — | 5 | **5** | **2nd classical-track row; v2 ACTIVATED here.** Non-trivial leaf (bit ops + branch + struct RMW). First classical sprint with **real residual variance**: seed compiled 0.80/score 400, matched after **1 fix-iteration** (register-reuse nudge). Realized=5 (seed band; no stuck-far/permuter/re-attempt/split/carry; not a verbatim mirror so the −1 first-try credit doesn't apply). Residual 0 — seed priced it right, but the loop *iterated*, so the realized tier finally has signal. `(real)` is now **logged, not illustrative** |
| 12 | yieldthread | 1 | mirror / warm / recover-extern | 2 | *2* | 2nd thread-band leaf (sibling of S8 stopthread); 10th straight clean mirror. seed 2 = warm-1 floor +1 for the `refs-unplaced` recover-extern enabler (`__osRunQueue`@0x800C8228, recovered at the S11 gate). Verbatim cp, 0 iterations, zero header copies, one symbol add. Mirror track stays seed-only — `(real)` illustrative |
| 13 | visetyscale | 1 | mirror / warm / — | 1 | *1* | 6th vi-band mirror (`osViSetYScale`). **Seed-miss event:** pick mis-seeded it **5** (mislabeled classical, un-named func_800AD370) — corrected to **1** at the gate after asm-vs-upstream confirmed the libultra mirror. Verbatim cp, 0 iterations, zero header copies, one symbol add. Motivated the S13 retro's pick_target signature-matcher + 3-lib scan. Mirror track stays seed-only — `(real)` illustrative |
| 14 | setthreadpri | 1 | mirror / warm / — | 1 | *1* | 3rd thread-band mirror (`osSetThreadPri`, after S8 stopthread + S12 yieldthread). Seed **correct first time** (pick said 1/mirror, no gate correction). Largest mirror banked to date (208 B, queue dequeue/enqueue + yield branch) yet still pts 1 → confirms byte-gate-dormant calibration. Verbatim cp, 0 iterations, name pre-curated, all 7 refs + 3 headers pre-placed — **one yaml flip the only enabler** (zero symbol add, zero header copy). Retro applied #3 open-band fast-path. Mirror track stays seed-only — `(real)` illustrative |
| 15 | nugfxswapcfb | 1 | mirror / cold / band-unlock | 3 | *3* | **1st libnusys row — opens the whole nuGfx*/nuCont* band.** seed 3 = cold-mirror floor 2 + 1 scaffolding-enabler (`-I include/libnusys` CFLAGS + verbatim `nusys.h`, deps `<ultra64.h>`+`<PR/gs2dex.h>` both in-tree; callee `osViSwapBuffer` banked S5). One paid-once enabler, then the band ranks as pts-2 cold mirrors. Verbatim cp, 0 iterations, name pre-curated (`nuGfxSwapCfb`@0x800A15E0 in ghidra_symbols → zero symbol add). Retro applied #1 (un-blk nusys ranker `-I`) + #2 (CLAUDE.md libnusys path table). Mirror track stays seed-only — `(real)` illustrative |
| 16 | nugfxtaskallendwait + nugfxdisplayoff | 1+1 | mirror / cold / recover-extern | 2+2=4 | *4* | **6th sibling-pair; 1st libnusys batch (S15-note).** Both verbatim cp, 0 iterations. Each needed a recover-extern (nuGfxTaskSpool=0x8012D478, nuGfxDisplay=0x80104E6C) — **non-`__` library globals pick_target's refs-unplaced grep missed → a false-clean** (seeded 2 each, no hazard, but actually recover-extern). Recovered deterministically at the gate (lui/lw + lui/sw). Callee `osViBlack` banked S6; names pre-curated in ghidra_symbols. Retro applied #1 (broaden refs-unplaced to header-declared non-`__` data externs — now flags nuGfxFunc/nuScPreNMIFunc etc.) + #2 (CLAUDE.md: mirror branch's proof IS the full-make SHA). Mirror track stays seed-only — `(real)` illustrative |
| 17 | nucontgbpak{getstatus,power,readid} | 1+1+1 | mirror / cold→warm / — | 2+2+2=6 | *6* | **7th sibling-batch (first trio); opens the nuCont sub-band warm.** All 3 verbatim cp, 0 iterations. **First nuGfx/nuCont leaves with NO data globals** — shared sole callee `nuSiSendMesg`@0x800A2824 (already in ghidra_symbols), struct types/constants/prototypes all in nusys.h → zero new symbols, zero header copies, zero splits, three yaml flips the only enabler. S16#1's broadened refs-unplaced grep correctly reported all 3 no-hazard (no false-clean this time); gate jal/lui scan confirmed truly clean. Retro applied 0 of 3 (all confirmatory observations). Mirror track stays seed-only — `(real)` illustrative |
| 18 | nucontinit | 1 | mirror / warm / jal-divergence | 2 | *2* | **near-verbatim mirror sub-case (1st):** upstream calls 4 managers, ROM's asm has only **3 jals** (no `nuContPakMgrInit` in this build). Not a missing symbol/def — an upstream-vs-ROM build **divergence**. Copied upstream verbatim, dropped the diverging line, banked on full-make SHA, 0 iteration. Caught at the gate by disassembling (pick reported no-hazard — its ref-grep counts data refs, not jal callees). Retro applied #1 (new `jal-count-mismatch` hazard in pick_target — verified it flags 4vs3) + #2 (CLAUDE.md near-verbatim-mirror bullet). Mirror track stays seed-only — `(real)` illustrative |
| 19 | nu{prenmi,gfx,gfxswapcfb}funcset | 1+1+1 | mirror / cold / recover-extern | 3+3+3=9 | *9* | **8th sibling-batch (2nd trio); the S15-note nuGfx\*FuncSet trio.** All 3 verbatim cp, 0 iterations. Each stores one callback global behind an `osSetIntMask` critical section; one recover-extern each (nuScPreNMIFunc=0x800B6780, nuGfxFunc=0x800C7DC0, nuGfxSwapCfbFunc=0x800B67B4) — S16#1's broadened refs-unplaced grep flagged all 3 correctly (no false-clean), vrams confirmed deterministically at the gate via each fn's own lui/addiu (3/3 matched pick's inlined vram). 3 fns + callee `nuGfxTaskAllEndWait` pre-curated in ghidra_symbols → only enabler = 3 symbol_addrs adds + 3 yaml flips. **First 3-file homogeneous batch banked at the ≤3-4 cap** (vs S4/S5's pairs). Retro applied #1 (CLAUDE.md recover-extern-mirror sub-case bullet) + #2 (CLAUDE.md fill-the-cap batch-sizing rule) + #3 (keep gate MCP re-confirm mandatory; 3/3 logged). Mirror track stays seed-only — `(real)` illustrative |
| 20 | nucont{rmbstart,gbpakopen} | 1+1 | mirror / cold / recover-extern | 3+3=6 | *6* | **9th sibling-batch (3rd recover-extern batch); 2nd libnusys pair.** Both verbatim cp, 0 iterations. One recover-extern each (nuContRmbCtl=0x80104F50, nuContPfs=0x801B8A18) behind shared callee `nuSiSendMesg` (banked S17). **First gate vram-miss on the inlined `refs-unplaced` vram:** pick reported nuContRmbCtl@0x80104F57 = the `.mode` field addr (offset 7) of an indexed-struct array — true array base 0x80104F50, recovered via the mandatory lui/addiu re-confirm (nuContPfs@0x801B8A18 matched exactly). Homogeneous pair; genuinely-clean set was exactly 2 (trimmed nuContDataGetEx — extra MISSING fn symbol nuContDataOpen). Retro applied #1 (CLAUDE.md field-addr-vs-base note + keep re-confirm mandatory) + #2 (CLAUDE.md array-extern size rule: scalar 0x4 / array stride×count). Mirror track stays seed-only — `(real)` illustrative |
| 21 | nugfxretracewait | 1 | mirror / warm / — | 2 | *2* | **First zero-hazard clean cp since the libnusys band opened (S15).** Verbatim cp, 0 iterations, name pre-curated, all refs + `nusys.h` pre-placed → **one yaml flip the only enabler** (zero symbol add, zero header copy, zero known-edit). Refutes the stale S11 "warm pool mined out" note — that predates the S15 libnusys unlock; this band still yields zero-enabler clean cp's, not just recover-extern fillers. Retro applied #1 (refresh BACKLOG PO note accordingly). Mirror track stays seed-only — `(real)` illustrative |
| 22 | epilinkhandle | 1 | mirror / warm / recover-extern | 3 | *3* | **First `nintendo/` variant-dir mirror** (prior libultra were `monegi/`/`shared/`); 1st libultra recover-extern since the band's mined-out note, refuting it like S21 did for libnusys. Verbatim cp, 0 iterations; one recover-extern (`__osPiTable`=0x800C7E8C, simple pointer global → size:0x4) behind `__osDisableInt`/`__osRestoreInt`, both pre-placed; `piint.h` in-tree → one symbol add + one yaml flip. **Process catch:** the mandatory re-confirm exposed a *target-fn* vram error — a hand-guessed flat `rom + 0x80020000` resolved mid-function and returned a wrong ~1000 B containing fn; real base is `rom + 0x80024C00` (= ghidra_symbols `osEPiLinkHandle`=0x800A3420). Retro applied #1 (CLAUDE.md: look up target vram, never guess a flat offset) + #2 (`pick_target.py` vram column). Mirror track stays seed-only — `(real)` illustrative |
| 23 | epidma | 1 | mirror / warm / recover-extern + calls-unplaced | 2 | *2* | **2nd `nintendo/pi/` mirror (sibling of S22 epilinkhandle).** Verbatim cp, 0 iterations. One recover-extern (`__osPiDevMgr`=0x800C7E70, OSDevMgr struct → size:0x1C, confirmed from the fn's own `lui 0x800c`/`lw 0x7e70` `.active` off0 + the 0x1C gap to placed `__osPiTable`). **New friction → the S23 fix:** the leaf ALSO calls an unplaced *function* (`osPiGetCmdQueue`=0x800B06F0) — pick's `refs-unplaced` *excludes anything called*, so this was a false-clean caught only at the **execution-middle link** (the INCLUDE_ASM gate-build resolves the jal directly → can't see the missing C symbol). Recovered from its jal target, add-only `// type:func`, re-extract+make → green SHA. Retro applied #1 (new `calls-unplaced:<fn>@0x<addr>` hazard — function dual of `refs-unplaced`, comment/string/dead-block-stripped; verified it flags `nuContDataGetEx`→`nuContDataOpen`@0x800A2CAC) + #2 (CLAUDE.md: reconcile the full data+function callee list at the gate). Mirror track stays seed-only — `(real)` illustrative |
| 24 | nucontdatagetex | 1 | mirror / cold / recover-extern + calls-unplaced | 3 | *3* | **First leaf carrying BOTH `refs-unplaced` AND `calls-unplaced` simultaneously** — the S20 data-recover and S23 function-dual doctrines composed cleanly, no special handling. Verbatim cp, 0 iterations. Two gate recoveries from one disassemble pass: data `nuContData`=0x801051F8 (`OSContPad[NU_CONT_MAXCONTROLLERS]`, stride 6 from the asm index-multiply + `li a2,0x6` bcopy size × 4 controllers → size:0x18; confirmed BASE not field via the offset-0 `lhu`, S20 caution cleared) + callee `nuContDataOpen`=0x800A2CAC (jal target, S23 dual). jal-count reconcile 3vs3 clean (`nuContDataClose`/`bcopy` pre-placed) — pick correctly did NOT flag jal-count-mismatch. **Closes the S20 trim** (this fn was dropped from the S20 pair for the then-unhandled missing `nuContDataOpen`). Retro applied 0 of 3 (PO: apply none — #1 size-hint tooling declined; #2/#3 confirmatory). Mirror track stays seed-only — `(real)` illustrative |
| 25 | nucontdatalock | 2 | mirror / cold / recover-extern | 5 | *5* | **Whole-file pack: one subseg (0x7E2D0) = exactly the 2 fns of `nucontdatalock.c` → no split, no orphan asm.** Both verbatim, 0 iterations. Single recover-extern `nuContDataLockKey`=0x800FED38 — the simplest S20 sub-case: a plain **scalar** word (size:0x4, assigned directly, no index-multiply) so the inlined `refs-unplaced` vram needed no base-offset correction; re-confirmed via the fn's own `lui 0x8010`/`sw -0x12c8`. Both fn names pre-curated, `osSetIntMask` pre-placed, lock macros in `nusys.h` → one gate add. Two `jal 0x800a2f60` both = osSetIntMask, reconciled clean (no jal-count flag, correctly). Retro applied 0 of 0 (buffer "None new"). Mirror track stays seed-only — `(real)` illustrative |
| 26 | nucontrmbcheck + nucontqueryread | 1+1 | mirror / cold / jal-divergence + pack | 2+3=5 | *5* | **Heterogeneous pair: `nuContRmbCheck` (near-verbatim/drop — `osSetIntMask` critical section absent from this ROM build; 3 upstream jals → 1 ROM jal confirmed via disassemble; dropped `mask` + `osSetIntMask`×2) + `nuContQueryRead` (verbatim pack-split; trivial 1-jal fn released by splitting the 0x7E330 pack at rom 0x7E350; unnamed 16B sibling `func_800A2F50` stays asm).** Both names pre-curated in ghidra_symbols; no new symbol adds, no header copies. Both 0 iterations. PO directive at S26 retro: target ≥5pt per sprint going forward → batch 2+ files consistently. Retro applied 0 of 0 (buffer "None new"). Mirror track stays seed-only — `(real)` illustrative |
| 27 | setglobalintmask + resetglobalintmask + gettime | 1+1+1 | mirror / cold / pack+recover-extern | 5+3=8 | *8* | **Opens `nintendo/exception/` (3rd `nintendo/` band after S22/S23 `pi/`) + `monegi/time/` (new band).** Pack split at 0x8B9D0; shared recover-extern `__OSGlobalIntMask`@0x800C9470 (inlined vram, size:0x4) for the `exception/` pair; `__osBaseCounter`@0x800FBE04 (size:0x4) + `__osCurrentTime`@0x801052F0 (size:0x8 OSTime) for `gettime.c` from asm disassembly. `__osViDevMgr` dead-`#ifdef _DEBUG` over-flag confirmed (no symbol add — pick's `#ifdef`-blind grep expected behavior). 3 symbol adds + 1 pack split + 3 yaml flips at gate. All verbatim cp, 0 iterations. seed 8pt (5+3); 8-gate clear (3-increment batch, not a single large item). Retro applied 0 of 0 (buffer "None new"). Mirror track stays seed-only — `(real)` illustrative |
| 28 | nucontgbpakreadwrite + nucontgbpakcheck + memset+setmem | 1+1+2 | mirror / cold / pack-split+NU_DEBUG+header | 3+3=6 | *6* | **Heterogeneous 3-file sprint: libnusys pack-split (0x7D710 → two single-fn leaves) + libkmc whole-file pack + companion header copy.** `nuContGBPakReadWrite`: verbatim cp — `#ifdef NU_DEBUG` block compiles out automatically (ROM build doesn't define NU_DEBUG; no manual drop needed, unlike S26's near-verbatim). `nuContGBPakCheckConnector`: verbatim cp, simple 1-jal leaf. `memset`+`setmem`: verbatim cp of upstream `libkmc/memset.c`; FAST_SPEED=1 path confirmed consistent; `memory.h` companion copied from `~/development/repos/libkmc/include/memory.h` → `include/libkmc/memory.h` (self-contained: `size_t` + memory fn decls); libkmc `-O` profile auto-applied by Makefile. All 4 fns 0 iterations, all names pre-curated. md5-candidate 46→49. **Retro fix:** `pick_target.py` `jal-count-mismatch:5vs1` was a tool bug — (1) `_DEAD_OPEN_RE` missing `#ifdef NU_DEBUG`; (2) `C_CALL_RE` matching `address(`/`size(` inside format-string literals. Fixed: `NU_DEBUG` added to dead-block pattern + `_strip_string_literals()` helper applied at both `call_divergence` and `calls_unplaced`. After fix `osSetTimer` correctly shows `5vs2`. Retro applied 1 of 1 (#1 fix jal-count-mismatch). Mirror track stays seed-only — `(real)` illustrative |
| 29 | nupireadwritesram + _matherr | 1+1 | mirror / cold+warm / recover-extern+pack+needs-define | 3+3=6 | *6* | **libnusys (cold): `nuPiReadWriteSram` — entire body behind `#ifdef USE_EPI`; compiles to empty stub without it. One-time Makefile enabler (`LIBNUSYS_CFLAGS := $(CFLAGS) -DUSE_EPI` + libnusys pattern rule) at gate; `nuPiSramHandle`=0x8012F4D8 recover-extern. `pick_target.py` false-clean (USE_EPI not detected as a hazard). libkmc (warm): `__matherr` — pack-split 0x8EBE0 → C 112 B (`_matherr.c`) + hasm 56 B (`__muldi3`, permanent); `errno`=0x800FE3D0 recover-extern; libkmc `-O` auto-applied. Both verbatim cp, 0 iter. md5-candidate 49→51. Retro applied 1 of 1 (#1 `needs-define` hazard in `pick_target.py` — `function_gating_define()` detects top-level `#ifdef` body gate; `_parse_makefile_defines()` cross-checks against effective library CFLAGS; +1 in seed_points). Mirror track stays seed-only — `(real)` illustrative |
| 30 | strcmp + osSetTimer + __osDequeueThread | 1+1+1 | mixed: mirror/warm/verbatim + classical/warm/jal-mismatch-stripped + classical/warm/defines-data-drop | 1+2+3=6 | **6** | **First mixed sprint; 3rd+4th classical-track fns.** `strcmp` (libkmc verbatim cp, 0 iter, warm mirror, seed 1; libkmc `-O` auto-applied). `osSetTimer` (**mis-routed as near-verbatim at DoR** — `jal-count-mismatch:5vs2` flagged; asm revealed fundamentally stripped impl — no interrupt disable/restore/counter update, different `__osSetTimerIntr` arg; routed to classical; `__osTimerList`@0x800C8240 recover-extern; score 0 first pass; realized=seed=2). `__osDequeueThread` (classical, defines-data drop — 5 file-scope defs from thread.c dropped; 64 B pointer-walk loop; register params + `(OSThread*)queue` head cast; score 0 first pass; realized=seed=3). All 3 green SHA first pass, 0 stuck-far/permuter/carried/re-opened. md5-candidate 51→54. Retro applied 1 of 1 (#1 CLAUDE.md gate note: large jal-count-mismatch >2 is `classical-likely` — disassemble before routing to mirror branch). Classical track adds 2 fns, both residual 0 — S30 classical seed+realized **5 logged** |

**Seed-velocity = 2.0 pt/sprint** (bootstrap anchors S1–5, sum seed 10 / 5 sprints). With the
live-logged mirror sprints S6 (seed 1) + S7 (seed 2) + S8 (seed 1) + S12 (seed 2) + S13 (seed 1)
+ S14 (seed 1) + S15 (seed 3) + S16 (seed 4) + S17 (seed 6) + S18 (seed 2) + S19 (seed 9)
+ S20 (seed 6) + S21 (seed 2) + S22 (seed 3) + S23 (seed 2) + S24 (seed 3) + S25 (seed 5)
+ S26 (seed 5) + S27 (seed 8) + S28 (seed 6) + S29 (seed 6) + S30-mirror (seed 1), the
running mirror-regime seed-velocity is 89 pt / 27 sprints = **3.30 pt/sprint** — the warm-pool
downward drift reverses as a *new* cold band opens (S6 last clean vi leaf; S7 opened the *cold*
convert band at the 2 pt floor; S8 the warm-1 thread leaf; S12 the warm-2 recover-extern thread
leaf; S13 6th vi mirror; S14 3rd thread mirror — note S14 was a **zero-enabler warm singleton**
(one yaml flip), so the "warm clean-singleton pool mined out" claim is **vi-band-specific**, not
project-wide: the thread band still yields zero-enabler clean leaves; S15 paid a one-time
+1 enabler to open the *cold* libnusys band — a fresh deep pool of pts-2 cold mirrors; S16
sibling-batched 2 of them, seed 4, each a recover-extern over a non-`__` library global that
pick_target had mis-reported clean — the S16 retro broadened the refs-unplaced grep to close
that false-clean); S17 trio-batched the nuContGBPak siblings (seed 6) and S19 the nuGfx\*FuncSet
trio (seed 9). **The post-S16 rise to 2.63 is sibling-batch-driven, not per-leaf throughput** —
S17/S19 banked 3 files each in one sprint, so points/sprint reflects how many homogeneous
siblings were committed (the "commitment-shaped, not pure throughput" caveat below), not faster
matching; per-leaf effort is still the flat cold-mirror 2–3 pt. **S9 is the first classical-track row** (seed 5,
banked 5, first-pass clean) — logged separately, NOT folded into the mirror 3.30 average
(never compare regimes). Classical track: **n=3** (S9 seed 5 / realized
illustrative; S11 seed 5 / **realized 5 logged**; S30 (mixed) seed 5 / **realized 5 logged**),
seed-velocity 5 pt/sprint. S9 proved the loop
mechanically but produced **zero residual variance** (clean wrapper); **S11 produced the first
real variance** (non-trivial leaf, seed compiled 0.80, matched after 1 fix-iteration), so the
PO **activated v2** at the S11 review. The realized tier is now live on this track; all three
data points land at seed (residual 0), so the seed rubric is so-far well-calibrated for
small classical leaves — watch for the first non-zero residual (a stuck-far / permuter / re-attempt).
Three honest caveats:
- **Fitted, not validated.** The bands were chosen so this history reads ~2 pt/sprint; the
  first out-of-sample sprint is the real test. The byte gates (768/1536) and the classical
  5/8/13 tiers have **zero** supporting data — provisional, corrected by v2's residual loop.
- **Non-stationary.** The recent ~2 came from *warm* sibling pairs, but of the ~56 remaining
  mirror candidates only ~18 are warm and ~38 are cold-band (the 2–3 pt tier) — expect velocity
  to **drift down within the mirror regime** as the warm pool depletes, then a discontinuity at
  the classical wall.
- **Commitment-shaped, not pure throughput.** Sprints are PO-scope-boxed, so points/sprint
  partly measures how much was committed, not raw output — read it as a **regime indicator**,
  not a productivity score.
- **S0 excluded.** Sprint 0 (7 files, pre-Scrum) had no DoD discipline and was batch-extracted
  before the overlay; its 7-files/sprint outlier would corrupt the per-sprint baseline. It
  stays in `BACKLOG.md` as historical record, out of this calibration.

## Velocity (headline — v1 mirror track + v2 classical track active)
- **Mirror track (v1): seed-velocity 2.0 pt/sprint** (n=5 anchors, all verbatim; seed-only —
  the mirror band is a point mass, no realized tier).
- **Classical track (v2 ACTIVE since S11): n=3, seed 5 pt/sprint, realized 5 pt/sprint, residual 0.**
  Realized-velocity / rolling-5 / re-anchor are now live here; all three data points land at seed
  so far, so the rubric is well-calibrated for small classical leaves pending the first non-zero residual.
- **Regime:** `mirror` is a depleting minority (~22 % of ranked candidates: 56 mirror vs 194
  classical; 18 warm / 38 cold). The warm clean-singleton mirror pool is now **mined out** (S11
  plan gate: every top mirror candidate carries a blocking hazard), pushing the project onto the
  classical track — which is exactly what tripped v2.
- **S7 note:** vi band exhausted of clean leaves (S6), so S7 opened the **cold convert band**
  (`virtualtophysical`, seed 2 = the cold-mirror floor). The S7 retro added a **`refs-unplaced`
  hazard** to `pick_target.py` (a referenced data extern absent from both name files — the dual
  of `defines-data`); it re-priced `osYieldThread` 1→2 and `osEPiLinkHandle` 2→3, and confirmed
  `osStopThread` is the smallest *clean* remaining leaf (seed 1, warm, no unplaced ref). Still
  7-straight first-pass-clean mirrors → zero residual variance → v2 trigger remains unmet.
