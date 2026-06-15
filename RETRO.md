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
