# Hazard playbooks (on-demand reference)

This is the detail layer for the MG64 execution loop. `CLAUDE.md` holds the always-loaded core
and a hazard index; read the matching section here when `tools/pick_target.py` flags a hazard or a
match behaves as one of the symptoms below. Each entry follows the same shape where it fits:
**Rule** (the invariant), **Trigger** (the flag or symptom that sends you here), **Procedure** (the
steps to a green ROM SHA-1), then optional **Sub-cases / variants** and **Caveats**, and
**Provenance**. Short principle/note sections carry only the labels that apply. The S-numbers are
provenance tags pointing at the sprint that established the rule.

The deterministic-vram recovery and the byte-`cmp` rules are load-bearing: follow the procedures
exactly rather than re-deriving them.

---

## Playbook index

The hazard families below group the sections that follow. Each links to its existing anchor;
`CLAUDE.md`'s hazard index maps every `pick_target.py` flag to the same anchors.

**Upstream verbatim mirror & build profile**
- [Upstream-mirror pattern](#upstream-mirror-pattern)
- [asm-mirror vendoring](#asm-mirror-vendoring)
- [per-library standard-C-header isolation](#per-library-standard-c-header-isolation)
- [Compile profiles (libkmc -O, libultra -O3)](#compile-profiles-libkmc--o-libultra--o3)
- [isolated-compile caveat](#isolated-compile-caveat)
- [open-band fast-path](#open-band-fast-path)
- [intrinsic-likely / maybe-upstream (signature hints)](#intrinsic-likely--maybe-upstream-signature-hints)

**Coddog cross-ref & symbol collisions**
- [coddog cross-ref](#coddog-cross-ref)
- [static-name-collision (an upstream file-static reuses a placed global name)](#static-name-collision-an-upstream-file-static-reuses-a-placed-global-name)
- [overlapping symbols — allow_duplicated (two instances share one official static name)](#overlapping-symbols--allow_duplicated-two-instances-share-one-official-static-name)

**Recover-extern & symbol placement / sync**
- [recover-extern (refs-unplaced)](#recover-extern-refs-unplaced)
- [calls-unplaced (function-callee dual)](#calls-unplaced-function-callee-dual)
- [Macro-hidden recover-extern](#macro-hidden-recover-extern)
- [header-renames-symbol (vendored header rewrites the curated symbol)](#header-renames-symbol-vendored-header-rewrites-the-curated-symbol)
- [wrong-ghidra-name-override (correct a mislabeled symbol without sync-names)](#wrong-ghidra-name-override-correct-a-mislabeled-symbol-without-sync-names)
- [make sync-names eviction recovery](#make-sync-names-eviction-recovery)
- [stale top-level asm label sync](#stale-top-level-asm-label-sync)
- [stale parent asm relic (find_segment mis-resolution after a decompose-split)](#stale-parent-asm-relic-find_segment-mis-resolution-after-a-decompose-split)
- [caller-evict](#caller-evict)

**Vendored headers & preprocessor defines**
- [defines-data](#defines-data)
- [needs-header](#needs-header)
- [vendored-header-incomplete (a reconstructed header is `(already-vendored)` yet missing a macro)](#vendored-header-incomplete-a-reconstructed-header-is-already-vendored-yet-missing-a-macro)
- [crlf-vendored-header (a copied SDK header breaks KMC cpp's `\` continuations)](#crlf-vendored-header-a-copied-sdk-header-breaks-kmc-cpps--continuations)
- [stale-vendored-header](#stale-vendored-header)
- [clean-rebuild-after-shared-header-edit](#clean-rebuild-after-shared-header-edit)
- [shared-callee RENAME](#shared-callee-rename)
- [needs-define](#needs-define)
- [N_MICRO library-wide pin](#n_micro-library-wide-pin)
- [GBI-microcode define](#gbi-microcode-define)
- [Version-rev `#define` value divergence](#version-rev-define-value-divergence)
- [VERSION_K-gated statement present in MG64's J build](#version_k-gated-statement-present-in-mg64s-j-build)
- [vendored-header inversion (a curated libultra header diverges from the ultralib pin)](#vendored-header-inversion-a-curated-libultra-header-diverges-from-the-ultralib-pin)

**Data / rodata carve**
- [.rodata sibling-yaml pattern](#rodata-sibling-yaml-pattern)
- [data-rodata-carve](#data-rodata-carve)
- [decomposed-one-tu rodata alignment split (a counter-case to the 8-point decompose gate)](#decomposed-one-tu-rodata-alignment-split-a-counter-case-to-the-8-point-decompose-gate)

**Near-verbatim mirror byte-edits**
- [Near-verbatim mirror (jal-count-mismatch)](#near-verbatim-mirror-jal-count-mismatch)
- [Mirror cast divergence (sign- vs zero-extend)](#mirror-cast-divergence-sign--vs-zero-extend)
- [char-signedness (libultra is -fsigned-char)](#char-signedness-libultra-is--fsigned-char)
- [assert-strip (bare upstream assert vs NDEBUG)](#assert-strip-bare-upstream-assert-vs-ndebug)
- [same-TU inline mismatch (definition-order + cross-TU split)](#same-tu-inline-mismatch-definition-order--cross-tu-split)
- [cross-jump-tail-merge](#cross-jump-tail-merge)
- [double-sqrt fast-math (bare sqrt.d needs a per-file -ffast-math override)](#double-sqrt-fast-math-bare-sqrtd-needs-a-per-file--ffast-math-override)

**Segment splitting & bss layout**
- [file-static (BSS-layout-conflict)](#file-static-bss-layout-conflict)
- [Multi-function-segment splitting (pack)](#multi-function-segment-splitting-pack)
- [non16align](#non16align)
- [trailing-alignment pad after a C mirror](#trailing-alignment-pad-after-a-c-mirror)

**Classical register allocation (the wall)**
- [register-reuse nudge (classical regalloc)](#register-reuse-nudge-classical-regalloc)
- [permuter setup for KMC-toolchain mirrors](#permuter-setup-for-kmc-toolchain-mirrors)
- [pervasive-regalloc-classical-main](#pervasive-regalloc-classical-main)
- [loop-weight and live-length regalloc steering](#loop-weight-and-live-length-regalloc-steering)
- [permuter goto-backedge liveness unsound (var-reuse passes corrupt live-across-backedge values)](#permuter-goto-backedge-liveness-unsound-var-reuse-passes-corrupt-live-across-backedge-values)
- [return-type is load-bearing](#return-type-is-load-bearing)
- [struct-access-folding-changes-scheduling](#struct-access-folding-changes-scheduling)
- [offset-0-symbol re-materialization (fixed-global field RMW)](#offset-0-symbol-re-materialization-fixed-global-field-rmw)
- [volatile-view CSE reload (force a just-stored global to reload)](#volatile-view-cse-reload-force-a-just-stored-global-to-reload)
- [mem-in-struct scheduling lever (model a fixed global as a struct/array member)](#mem-in-struct-scheduling-lever-model-a-fixed-global-as-a-structarray-member)
- [call-result a0-vs-v0 single-allocno (force a scratch reg via both-arm reuse)](#call-result-a0-vs-v0-single-allocno-force-a-scratch-reg-via-both-arm-reuse)
- [compiler-source fan-out (escalation above the permuter)](#compiler-source-fan-out-escalation-above-the-permuter)
- [cross-project matched-corpus mining (sibling-decomp byte-exact escalation above the source dive)](#cross-project-matched-corpus-mining)
- [signed-divide-const v0/v1 quotient-destination](#signed-divide-const-v0v1-quotient-destination)
- [cse make_regs_eqv branch-fold (reused-var canonical fold on a `?:`-with-flag store)](#cse-make-regs-eqv-branch-fold-reused-var-canonical-fold-on-a--with-flag-store)
- [abs-coalescing reg-swap (fabsf in-place vs fresh reg on a const compare)](#abs-coalescing-reg-swap)

**Classical control-flow & scheduling**
- [struct-init-loop (dup-store / dual-induction-var)](#struct-init-loop-dup-store--dual-induction-var)
- [volatile-global tell (dead-reload + recompute-not-CSE)](#volatile-global-tell-dead-reload--recompute-not-cse)
- [top-tested-loop goto local-hoist (matching an un-inverted -O2 loop)](#top-tested-loop-goto-local-hoist-matching-an-un-inverted--o2-loop)
- [capturing $ra (return address) as a call argument](#capturing-ra-return-address-as-a-call-argument)
- [indexed-vs-pointer loop (strength-reduction preheader ordering)](#indexed-vs-pointer-loop-strength-reduction-preheader-ordering)
- [switch-jtbl-dispatch (compiler jump table + sparse inner cases)](#switch-jtbl-dispatch-compiler-jump-table--sparse-inner-cases)
- [short-text shifts flowing-bss (a length miss surfaces as a SIBLING's wrong data addr)](#short-text-shifts-flowing-bss-a-length-miss-surfaces-as-a-siblings-wrong-data-addr)
- [struct-array-of-BSS direct-index vs base-pointer var](#struct-array-of-bss-direct-index-vs-base-pointer-var)
- [goto-dispatch branch-toward vs branchless (constant dispatch through a shared return)](#goto-dispatch-branch-toward-vs-branchless-constant-dispatch-through-a-shared-return)
- [nested-function static-chain spill (leaf dead `sw v0,0(sp)` + caller sets `v0=&frame` per-call)](#nested-function-static-chain-spill)
- [local-alloc qty-permutation (1-basic-block reg swap, permuter-appropriate)](#local-alloc-qty-permutation)
- [cse-derived-pointer base-canonicalization (sub-struct dump fn keeps the param base, folds the offset)](#cse-derived-pointer-base-canonicalization)
- [integer-arith commutative operand order (pointer_int_sum pins pointer-first; int math keeps source order)](#integer-arith-commutative-operand-order)
- [setup-block instruction order (a pre-base loop-limit const must be a variable)](#setup-block-instruction-order)
- [struct-copy block-move path (alignment picks lwl/lwr vs aligned word-loop)](#struct-copy-block-move-path)
- [reorg optimize_skip annulled bnel (single-skipped-insn branch-likely)](#reorg-optimize_skip-annulled-bnel)
- [switch tight merged-default (shared case-0/default label + case-1 last)](#switch-tight-merged-default)
- [grid-counter double-loop (non-zero-cell counter idiom)](#grid-counter-double-loop)
- [base-register-vs-displacement (full base materialized vs %lo-in-displacement; + family-of ranker follow-up + shifted .NON_MATCHING data-carve blocker)](#base-register-vs-displacement-full-symbolbase-materialized-vs-lo-in-displacement)

**libnusys / audio-band specifics**
- [libmus-bundled-n_audio duplicate (a SUPPORT_NAUDIO libmus archive links its OWN n_audio synth copy)](#libmus-bundled-n_audio-duplicate-a-support_naudio-libmus-archive-links-its-own-n_audio-synth-copy)
- [NU_DEBUG-stock-not-custom (carried perf fn triage)](#nu_debug-stock-not-custom-carried-perf-fn-triage)
- [libnusys inline-div mflo-hazard nop](#libnusys-inline-div-mflo-hazard-nop)

**Build-profile probe & per-file opt exceptions**
- [game-region mirror (-O2 profile)](#game-region-mirror--o2-profile)
- [-O0 boot/SDK-glue file profile (a per-file opt-level exception)](#-o0-bootsdk-glue-file-profile-a-per-file-opt-level-exception)
- [profile-probe (pinning the flags of a SHA-missing build)](#profile-probe-pinning-the-flags-of-a-sha-missing-build)

**Toolchain oracles & spot-checks**
- [IO_WRITE/IO_READ isolation artifact](#io_writeio_read-isolation-artifact)
- [Assembler differences + byte-cmp spot-check](#assembler-differences--byte-cmp-spot-check)
- [gas .set-reorder delay-slot fill (textual layout != machine; disasm the .o)](#gas-set-reorder-delay-slot-fill)
- [Decompile-vs-asm authority](#decompile-vs-asm-authority)
- [Display lists](#display-lists)

---

## Upstream-mirror pattern

**Rule:** When a target maps to a known SDK upstream, copy the upstream `.c` verbatim instead of
hand-decompiling. The proof is the full-`make` ROM SHA-1 (verbatim copy + green ROM = match): no
iteration, no byte-`cmp` (that guard is classical-loop-only).

**Trigger:** `pick_target.py`'s `upstream` column is `libultra` / `libkmc` / `libnusys` / `libmus`
(not `none`). Three related flags also land here:
- `single-file-pack:<n>fn[…]`: every member fn comes from one upstream `.c`, so the pack is an atomic
  verbatim mirror with no split.
- `upstream-fncount-mismatch:<m>vs<n>`: a single-named-stem pack holds more fns than the
  upstream `.c` defines, so a foreign TU is bundled in the subseg. Split it off and mirror only the
  upstream's fns. This is the named-symbol analog of `coddog-fncount-mismatch` (xprintf's
  `func_800B1580` `__osDpDeviceBusy` TU). Advisory.
- `one-tu`: every inner fn boundary is non-16-aligned, so the subseg is one `.o`. This confirms
  a single-file-pack even for un-named coddog packs, and marks per-fn decompose as blocked (see also
  #non16align).

**Band-open: pick the `@100.00` leaf first to prove a new lib's profile.** When opening a
fresh upstream band (vendoring its first headers + a new `mk/<lib>.mk` profile), the lowest-risk first
pick is a `coddog-mirror:<file>@100.00` row that is a leaf (0 `calls-unplaced`, no rodata/data carve,
at most a drop-static). `@100.00` is byte-identical structure (not a customized body), and the leaf
has no cross-file callees to recover, so it validates the new profile with zero confounds: a SHA-miss
is then unambiguously the build profile (wrong `-O`/`-D`/`-I`), not a body or a missing extern. It
also tends to place the allocator/util the rest of the band calls (S141 `lib_memory.c`: the `@100.00`
`__MusIntMem*` leaf, banked first-build, placed the allocator the whole libmus band depends on).
Prefer it over a larger `@99.99` API file (which carries the diagnosis-pass + cross-file
recover-extern cost) as the band's first increment.

**Procedure:**
1. Upstream roots: libultra `~/development/repos/ultralib/src/`, libkmc
   `~/development/repos/libkmc/src/`, libnusys
   `~/development/repos/n64sdkmod/packages/libnusys/usr/src/PR/libsrc/nusys-2.07/nusys/src/`.
   ultralib is the sole libultra source. The project builds `-DBUILD_VERSION=VERSION_J`, so
   version-conditional upstream code selects itself via ultralib's own `#if BUILD_VERSION` guards;
   no manual version picking. Do not consult `libultra_modern` (deprecated): it is 2.0L-only and its
   casts diverge from this VERSION_J build (see the cast-divergence section).
2. Project path mirrors the upstream path exactly: take the upstream path relative to its `src/` root
   and prepend `src/lib<name>/`. E.g. `ultralib/src/io/vigetcurrcontext.c` becomes
   `src/libultra/io/vigetcurrcontext.c`. Mirror ultralib's functional subdirs verbatim (`io/`, `os/`,
   `libc/`, `audio/`, `gu/`, ...); keep each name and depth exactly as upstream. Companion headers mirror their
   upstream `include/` layout under `include/lib<name>/`.
3. Verbatim means verbatim: keep dead `#ifdef _DEBUG` blocks and their unconditional companion
   `#include`s (they compile out because `_DEBUG` is undefined). If such a block needs a header
   missing from the tree, copy that header rather than trimming the include.
4. `clang-format-22 -i` the copied files under `src/libultra/` and `src/libkmc/` (both dirs carry a
   local `.clang-format` = `BasedOnStyle: Google` + `SortIncludes: Never`; formatting does not change
   codegen).
5. `make`, and the ROM SHA-1 must equal the baserom.

Before declaring a clean verbatim mirror, reconcile the upstream's full call + data-ref list
(including one level of library-macro expansion) against the name files; see the recover-extern and
near-verbatim sections. Known-edit sub-cases: file-static drop, defines-data drop, near-verbatim
line-drop, recover-extern placement.

**Sub-cases / variants:**

**Authoritative symbol set via `nm`.** For a mirror that is `#if BUILD_VERSION`-branched or
inline-ambiguous (a `pack` where a static helper may or may not be inlined), read the matching
prebuilt object rather than reasoning from the source alone:
`mips-linux-gnu-nm ~/development/repos/ultralib/build/J/libgultra_rom/src/<dir>/<file>.o`. It gives
the definitive VERSION_J symbol set: which functions are global (`T`) vs absent (inlined), which data
is local/static (`b`/`d`) vs global, and the exact `.text` offsets. S102 motor.c: nm settled that the
0x800AE380 fn is `T __osMotorAccess` (not the ghidra `osMotorStop`; see #wrong-ghidra-name-override),
that `__osMakeMotorData` is inlined (so `pack:2fn`, not 3), and that `__MotorDataBuf` is a local `b`
(so drop-static-to-extern, not a defined global). `libgultra_rom` is the release/ROM profile that
matches MG64's build; `libgultra`/`libgultra_d` are the debug profiles.

**libnusys multi-version triage before concluding "custom".** The pinned libnusys source is
nusys-2.07 (`coddog-sweep-nusys` builds its ref), but MG64 forked an earlier rev with game edits, so a
2.07 mirror can SHA-miss or only structurally match. Before declaring a function MG64-custom, triage
across all on-disk nusys versions and pin the rev from the ROM:
- Sources: `~/n64sdk/.../nusys/src/nusys-{1.10,1.20,2.00}/nusys/*.c` (note CRLF + Shift-JIS comments;
  UTF-clean or strip comments when copying) plus the pinned `~/development/repos/n64sdkmod/.../nusys-2.07/`.
- Pin the rev with two ROM-side tells: (1) `strings baserom.z64 | grep -i NuSystem` — absent means the
  `nuVersion[]="NuSystem"NU_VERSION` marker was removed (so not a stock 2.07 with its `.data` string;
  `nuScRetraceCounter` is then uninitialized bss, no `.data` carve). (2) per-function feature diffs —
  e.g. the `nuScAddClient` PRENMI-dispatch block is a 1998/12 (2.06/2.07) addition; its presence in
  the asm dates the fork ≥2.06 even when the version string is gone (S123 nusched = ~2.07-minus-nuVersion).
- A function that matches no version (game-fn callees, added display/swap blocks, a tvtype hang-guard)
  is genuinely MG64-custom -> classical track. A function that matches a specific older version is a
  near-verbatim mirror of that version (the per-file/per-fn version rule, #near-verbatim-mirror-jal-count-mismatch).
This generalizes the libultra coddog cross-ref (`#coddog-cross-ref`) and the S117 nusys-version-hunt to
the per-function grain: in one subseg, stock fns and custom fns coexist (bank-stock-carry-custom).

**`#pragma weak` alias mirror (no special handling).** A libultra C file whose ROM/curated
symbol is a weak alias mirrors verbatim with zero edits. `gu/cosf.c` / `gu/sinf.c` carry
`#pragma weak cosf = __cosf` + `#define fcos __cosf`, so the defined function is `__cosf` and `cosf`
is a weak alias at the same address. KMC gcc 2.7.2 emits both symbols; the linker resolves the
curated `cosf` (from `ghidra_symbols.txt` / `symbol_addrs.txt`) to the subseg address and `__cosf`
rides along: compiles + matches clean, no `#pragma`/define edit. This is the C analog of the
asm-mirror `WEAK(bcopy, _bcopy)` alias (S58 bcopy `_bcopy=bcopy`). `pick_target.py` keys the upstream
index by the weak alias too (S66 `PRAGMA_WEAK_RE`), so a weak-aliased pack member resolves to its
`.c` (`cosf=cosf`) instead of the opaque `cosf=?` that buried it in a `c-combined` pack. Watch for a
shared `__`-prefixed data extern reached only through the weak fn (S66: `__libm_qnan_f`, the NaN-path
return refd by both cosf and sinf as an anonymous `D_<addr>`): a recover-extern the `refs-unplaced`
flag misses when the fn is a hidden pack member (see #recover-extern).

**Perf/debug struct version drift (S125).** A vendored header struct that is referenced only by
carried / `INCLUDE_ASM` fns is **unvalidated** — it was copied from the upstream version verbatim and
never checked against the asm. When the first consumer is finally decompiled, the layout can be a
different library rev: MG64's `nusys.h` `NUDebTaskPerf` carried the 2.07 `markerTime[10]` field
(a 1999/05/30 feature) that MG64's pre-1999 rev lacks, so `auTaskCnt` was at 0x59 / `auTaskTime` at
0x1A0 in the header but the asm needs 0x9 / 0x150 (struct size 0x1F0, asm-confirmed via the
`idx * 0x1F0` array stride and the `lbu …,0x9` count load). Validate perf/debug struct offsets against
the asm before banking the first perf-using fn; fix the vendored header to the game's rev (drop
markerTime). A header struct is not ground truth until a matching fn proves its offsets.

**Header constants reach codegen too — validate each against the asm (S128, generalizes the perf-struct
rule).** Not just struct offsets: any vendored-header `#define` that flows into an instruction
(array sizes, message counts, loop bounds, masks) must be reconciled with the asm before banking. MG64's
nualstl `NU_AU_MESG_MAX` is 2 (the `osCreateMesgQueue(&nuAuMesgQ, …, 2)` count in the asm), not the
stock 2.05 source's 4; the vendored `include/libnualstl/nualstl.h` was edited to 2. Read the literal in
the target instruction (the `li`/`addiu` immediate), do not trust the stock constant — a wrong constant
is a single-immediate SHA-miss the same class as a wrong struct offset.

**`_FINALROM` (build-config) struct-size drift — a clean mirror can SHA-miss with the body byte-stock
(S145).** Generalizes the perf-struct rule to the build-config axis: a vendored header struct whose
size depends on a `#ifndef _FINALROM` / `#ifdef _DEBUG` block can drift between the ROM's library and
the in-tree `-D_FINALROM` profile, with no body change. MG64's game/libultra is `-D_FINALROM` (base
CFLAGS), but the 3rd-party Software Creations **libmus** library object was built **non-FINALROM**, so
`OSScTask` (`sched.h`) carries its `#ifndef _FINALROM` trailing `startTime`+`totalTime` (2× `OSTime` =
0x10) that the FINALROM build drops. `aud_sched.c`'s `__OsSchedDoTask` declares an `OSScTask` on the
stack, so the 16 B size delta surfaced as a 5-byte SHA-miss. Fix: per-library profile undef in
`mk/libmus.mk` (`-U_FINALROM`); the game/libultra stays FINALROM (e.g. `src/libultra/sched/sched.c`
matched with the smaller `OSScTask`), and no other libmus file is `_FINALROM`-sensitive (the remaining
conditionals are `OS_NUM_EVENTS` macros + an `alParseAbiCL` proto, not codegen), so the banked siblings
re-match on a full rebuild (`rm build/src/<lib>/*.o` — the build tracks no flag deps).
- **Tell (distinguishes a struct size drift from a body edit).** The miss is confined to one function's
  **stack-frame immediates** — the `addiu sp,sp,-N` frame size, the `sw/lw ra,M(sp)` save/restore offset,
  and any trailing local's `sp`-relative offset all shift by the same struct delta, while every field
  store offset (`sw …,K(sp)` into the struct) is unchanged. Field-stores-shift = wrong field offset
  (perf-struct S125); frame-immediates-shift-uniformly = wrong struct size (this). Read which class of
  offset moved before assuming a game-modified body.
- **Pre-flag follow-up (tracked, not yet built).** `pick_target.py` could pre-flag a libmus/libultra fn
  that declares a `#ifndef _FINALROM`-sized struct (`OSScTask`; sweep `os_message.h`/`osint.h` too) as
  `finalrom-struct:<struct>`, the build-config analogue of the perf-struct pre-flag. Until then the gate
  applies this by reading the target fn's struct locals.

**Provenance:** S15 unlocked the libnusys band by vendoring one `include/libnusys/nusys.h` +
`-I include/libnusys`; `-I include/libultra/PR` is the precedent for unblocking a band. S51 is the
cautionary case for step 1's "do not consult `libultra_modern`": its split `gu/` layout (hand-asm
`mtxcatf.s` + C `mtxxfmf.c`) mis-suggested `guMtxCatF` was hand-asm at the gate, while ultralib's
`src/gu/mtxcatf.c` (the sole source) holds both fns as clean C, which the ROM matched verbatim. Reach
for ultralib first; a `libultra_modern` `.s` is not evidence the ROM fn is hand-asm. S88 named the
`one-tu` non-16-aligned single-`.o` flag; S104 named the `upstream-fncount-mismatch` bundled-foreign-TU
flag.

---

## asm-mirror vendoring

**Rule:** The asm analog of the upstream-mirror pattern, for a libultra function that is genuinely
hand-written assembly (a register/FPU/cache/TLB primitive, the `intrinsic-likely` shims). Instead of
leaving it as an anonymous `asm/<rom>.s` disassembly or a bare `hasm`, vendor the real ultralib `.s`
TU into `src/libultra/<dir>/` and build it. The proof is the full-`make` ROM SHA-1, same as the C
mirror.

**Trigger:** `pick_target.py` flags `intrinsic-likely:<tu>.s` (a vendorable ultralib TU exists) on an
`asm-flip` candidate. A bare `intrinsic-likely` (no `:<tu>`) is a no-source shim, so plain `hasm`.

**Sub-trigger, `intrinsic-likely:cp0-asm(identify-TU)`.** This is the same asm-mirror class
for an un-named `func_<addr>` subseg whose body holds a privileged op gcc never emits (`tlbwi`/`tlbwr`/`tlbp`/`tlbr`,
`mfc0`/`mtc0`/`dmfc0`/`dmtc0`/`cfc0`/`ctc0`, `cfc1`/`ctc1`, `cache`, `eret`). The op proves a
vendorable ultralib source exists, but the un-named primary cannot resolve the `.s`, so the gate
identifies the TU first: one MCP `disassemble_function` names it by its CP0/TLB signature (e.g.
`mfc0/mtc0 Index/EntryHi + tlbwi` is `osMapTLB`/`osUnmapTLB`, S70), then vendors it per the procedure
below. This is broader than the pure-shim `intrinsic_likely`: it fires even when the body has real
control flow or `jal`s around the privileged op (`osMapTLB`'s branch/lw logic; an exception/context
dispatcher's `mtc0`+`jal`, e.g. the mislabeled `audio_sched_thread_entry` 0x800B3E50). The point is
negative as much as positive: such a subseg is hand-asm, never a classical decomp target, so
smallest-first must not surface it as one. If no ultralib source turns up for the identified TU, it
falls back to plain `hasm`.

**Procedure:**
1. **Copy the ultralib `.s` verbatim** from `~/development/repos/ultralib/src/<dir>/<file>.s` to
   `src/libultra/<dir>/<file>.s` (mirror the subdir, same as the C pattern).
   - **Internal-header include rewrite (the one verbatim deviation).** ultralib `.s` TUs include
     their internal headers bare (`#include "threadasm.h"`, `"exceptasm.h"`) because in ultralib they
     sit next to the `.s` in `src/<dir>/`. The project keeps those headers under
     `include/libultra/internal/`, which the `LIBULTRA_ASFLAGS` `-I` set reaches only via
     `-I include/libultra`, so rewrite each bare internal-header include to `internal/<h>`
     (`"threadasm.h"` becomes `"internal/threadasm.h"`). The public-prefixed includes (`PR/…`,
     `sys/…`) already resolve and stay as-is. This is the only allowed edit to the vendored `.s`; it
     touches no `.text` byte, so the full-`make` SHA-1 stays the verbatim proof. Skipping it is a
     first-build `No such file or directory` parse error, not a SHA miss (S107 exceptasm rewrote
     `exceptasm.h`+`threadasm.h`; S108 interrupt.s rewrote `threadasm.h`).
2. **Vendor any missing asm macros** into the project headers (verbatim from ultralib). The Monegi
   `include/sys/asm.h` lacked `MFC0`/`MTC0` (S56 added them). The TU's `#include "PR/R4300.h"`
   resolves via `-I include/libultra`; `sys/asm.h`/`sys/regdef.h` via `-I include`.
   `pick_target.py` pre-flags this as `intrinsic-likely:<tu>.s(needs-define:<MACROS>)` when the
   vendorable `.s` references an `UPPER_CASE` macro that none of the in-tree asm `-I` headers
   (`include`, `include/libultra`, `include/libultra/PR`, `include/libultra/compiler/gcc`) define, so
   the enabler is priced at the gate, not at a failing vendor-compile. The whole
   current cache/TLB/gu backlog is self-contained (all `DCACHE_*`/`C_*`/`TLB*`/`C0_*` ship in
   `include/libultra/PR/R4300.h`, `RDB_*` in `PR/rdb.h`), so the pre-check reports nothing for it.
3. **Assemble with ultralib's exact toolchain and flags: KMC/N64 gcc, gcc.mk profile, not modern
   `mips-linux-gnu as`.** This is the load-bearing step: KMC `as` pads each function's `.text` up to
   its 16-byte ROM slot, so the verbatim 0xC TU lands as the 0x10 the ROM expects. Modern `as` emits
   the bare 0xC and every following subseg shifts, breaking the SHA-1 (the entire failure mode
   before the toolchain switch). `mk/libultra.mk` carries this as `LIBULTRA_ASFLAGS` plus a path-based
   pattern rule `build/src/libultra/%.o: src/libultra/%.s` that assembles every `.s` under
   `src/libultra/` with `$(CC) $(LIBULTRA_ASFLAGS)` plus the section-alignment `objcopy` (the
   more-specific pattern wins over the generic `src/%.s` modern-GAS rule; the project sets
   `hasm_in_src_path: True`, see step 4). There is nothing to add per TU: dropping the `.s` under
   `src/libultra/<dir>/` plus qualifying the yaml line (step 4) is the whole extension, with no
   build-file edit. (This mirrors how `LIBULTRA_CFLAGS` mirrors ultralib's C profile.)
4. **Flip the subseg `asm` to `hasm` AND add the `<dir>/<stem>` name qualifier** in `mariogolf64.yaml`
   (e.g. `[0x86790, hasm, libultra/os/getcount]`), so the project's `hasm_in_src_path: True` resolves
   the `.s` to `src/libultra/<dir>/<stem>.s` and its object to `build/src/libultra/<dir>/<stem>.o`.
   splat's `hasm` `split()` writes the `.s` only if absent, so the verbatim copy from step 1 is kept
   (it IS the canonical source now, not a vestigial `asm/<rom>.s`) and never regenerated;
   `pick_target.py` skips `hasm` (it classifies only `asm`/`c`). The build gets the `.o` from the
   path-based pattern rule.
5. `make extract && make`, and the ROM SHA-1 must equal the baserom. Verify the vendored `.o`'s
   `.text` size equals the subseg size (`mips-linux-gnu-size -A build/src/libultra/<dir>/<stem>.o`).

**Caveats:**
- The `.ld` does **not** `ALIGN` between subsegs (only segment-level `ALIGN(__romPos,16)`), so the
  pad must live in the object; there is no linker fallback. This is why step 3's KMC padding is
  mandatory, not cosmetic.
- Per-TU version-match is unproven beyond the S56 reg shims (S57 added 4 cache/TLB primitives, all
  first-try clean). Each new TU needs its own ROM-SHA-1 verify; a macro-expansion divergence (e.g.
  an aliased `WEAK(bcopy, _bcopy)` whose project `WEAK` macro is empty) can need extra handling.
- **SHA-breaker bisect (a batch of N flips, one bad TU).** The gate `make` validates all flipped
  subsegs at once, so a single version-mismatched TU breaks the whole-ROM SHA-1 without naming the
  culprit. To localize: flip the suspect subseg back to `asm` in `mariogolf64.yaml` and drop its name
  qualifier. splat then re-extracts `asm/<rom>.s` on the next `make extract` (the vendored copy stays
  at `src/libultra/<dir>/<stem>.s` for retry); rebuild, and check the SHA-1. SHA goes
  green → that TU was the breaker (it is a spike: leave it `asm`, carry it to `BACKLOG.md`); SHA
  still red → keep flipping suspects back until it greens. Bisect a large batch by reverting half
  the flips at a time. The reverted TU's vendored `.s` stays in `src/libultra/` (unused while `asm`)
  for a later retry against a different ultralib revision. Banked TUs in the same batch are unaffected:
  each subseg's `.o` is independent.
- For a mixed `pack:` (e.g. `osSetIntMask` shares its subseg with the C mirrors
  `osCreatePiManager`/`__osEPiRawStartDma`), split first and vendor only the asm member.

**Sub-cases / variants:**
- **Vendored `.s` carrying a non-`.text` section (`.rodata`/`.data`/`.bss`) → vendor `.text` only.**
  splat auto-links a `hasm` `.o`'s `.data`/`.rodata`/`.bss` at the **end** of each output
  section, not in address order. See the generated `mariogolf64.ld`: the
  `build/src/libultra/os/invaldcache.o(.rodata)` / `writebackdcache.o(.rodata)` lines sit at the
  `.rodata` section tail, after the address-ordered C/data-file
  siblings. For a TU whose data section is empty this is a harmless 0-byte line; for a TU with a real
  data section (S84 `setintmask.s`'s `.rdata __osRcpImTable`, a 0x80-byte / 64-`.half` LUT) the bytes
  would be emitted a **second** time at the wrong offset → every following rodata byte shifts → SHA-1
  break. **Fix:** copy the `.s` but **strip the data block** (the `.rdata`/`.data` directive through
  EOF), vendoring only `.text`; keep that data as the existing splat-extracted generic blob
  (`asm/data/<rom>.rodata.s`), renamed to the upstream symbol via a `symbol_addrs.txt` add
  (`__osRcpImTable = 0x800D2200; // size:0x80`). splat then renames `D_<vram>` → the symbol in both
  the generic blob (which now *defines* it) and every referencer, **including cross-TU ones** (S84:
  the exception dispatcher `asm/8AF90.s` also loads `__osRcpImTable`), and the vendored `.text`'s
  `%hi/%lo(<sym>)` resolves to it. It is a `D_<vram>` rename across asm, so it needs a clean rebuild
  (`make clean && make extract && make`; see `#defines-data`). The vendored `.s` keeps its
  `.globl <sym>` line (now a harmless external declaration). Afterward confirm
  `build/src/libultra/<dir>/<stem>.o` is `.text`-only
  (`mips-linux-gnu-objdump -h build/src/libultra/<dir>/<stem>.o`), so the auto-link line carries 0 bytes.
  `pick_target.py` pre-flags this as `intrinsic-likely:<tu>.s(has-rodata:<sym>)` so the
  strip+rename enabler is priced at the gate. (Keeping the table in the shared blob is arguably *more*
  correct than carving it into one TU when it is referenced cross-TU, as `__osRcpImTable` is.)
  - **The has-rodata pre-flag is gated to the active build.** `vendorable_tu_data_symbols`
    strips `#ifndef _FINALROM` + inactive `BUILD_VERSION` branches before scanning the data section, so
    an export only the debug/other-version build emits is not listed (S91: exceptasm.s's
    `__osCauseTable_pt` lives in `#ifndef _FINALROM` → excluded under the `-D_FINALROM` asm profile; the
    real active rodata is `__osIntOffTable`/`__osIntTable`/`__osHwIntTable`/`__osPiIntTable`). Price the
    strip set the vendored `.o` actually carries, not the all-branches union.
- **A symbolic-pointer table in the data section → the label-export procedure.** The S84
  strip-and-rename works for a *numeric* LUT (`.word 0,0` / `.half …`): the
  bytes are self-contained, so keeping the extracted blob + renaming `D_<vram>` resolves cleanly. A
  table of `.word <label>` entries needs one more step. Such a table is a switch jump table
  (`__osIntTable: .word redispatch, sw1, …`, whose active `__osException` `jr`s through it) or any
  function-pointer table.
  splat extracts such a table to a separate rodata blob (`asm/data/<rom>.rodata.s`) with the entries
  emitted symbolically (`.word .L800B00A0, …`) as references to `.text`-internal labels (these are
  global: `jlabel` = `.global`+label, `include/macro.inc`). Two paths fail:
  (a) vendoring `.text`-only and doing nothing makes `asm/<rom>.s` go vestigial, so the `.L…` labels
  the blob references are no longer defined → undefined at link;
  (b) carve-placing the table in the vendored `.o` fails because a `hasm` `.o`'s `.rodata` auto-links
  at the section end → wrong addr → SHA break. **The fix is neither: re-export the labels** (S91's
  listed-but-untried option, proven S107 exceptasm). The blob lives
  in its own data subseg (`asm/data/<rom>.rodata.s`), which is **already address-placed and survives
  the `.text` flip**, and after the flip it keeps emitting `.word .L<addr>` (symbolic, not literal).
  So make the vendored `.text` define those labels:
  1. **Phase 1: rename-isolation (do first, subseg still `asm`).** `symbol_addrs` add-only: rename the
     jtbl head (`jtbl_<addr>` → its upstream name, e.g. `__osIntTable`) and every byte/word/data table
     the `.text` references by upstream name (`D_<vram>` → `__osIntOffTable`/`__osHwIntTable`/…), plus
     any externalized scratch (`__osThreadSave`). `make extract && make` → must stay green. This proves
     the renames don't break the still-`asm` build and that the jtbl rename preserves the symbolic
     entries, isolating rename risk from the flip. (S107: a clean green checkpoint.)
  2. **Phase 2: vendor `.text`-only + export the labels.** Copy the ultralib `.s`, strip `.rdata`/
     `.data` (S84), `.globl`-declare the stripped tables (harmless external decls). Then **insert
     `.globl .L<addr>` + `.L<addr>:` at each jtbl-target instruction**, mapping the blob's `.L<addr>`
     name to the upstream label by its instruction (e.g. `.L800AFDFC`=`mfc0 t1,C0_COMPARE`=`counter`;
     `.L800B00A0`=`lw t1,THREAD_PRI(k0)`=`redispatch`). Flip `asm`→`hasm` + add the yaml name
     qualifier (`libultra/<dir>/<stem>` resolves both the `.s` and its `.o`),
     `make clean && make extract && make`. The blob's `.word .L<addr>` now resolve to the vendored-
     `.text` globals; `objdump -h build/src/libultra/<dir>/<stem>.o` confirms `.text`-only (empty
     `.data`/`.rodata`/`.bss` → 0-byte auto-link lines). SHA-1 == baserom.
  `pick_target.py` flags it `intrinsic-likely:<tu>.s(asm-mirror-jtbl:<head>)` (the `.word <label>`
  table's head symbol), now a routine label-export asm-mirror, not a spike. (S107 exceptasm is the
  worked example: 8-fn OS exception/dispatch TU, 9 jtbl targets, banked first-try after the mechanism.)
- **Combined-subseg sub-pattern (≥2 distinct asm TUs in one subseg).** When one `asm` subseg holds
  ≥2 asm-only functions (no C mirror) from *different* ultralib `.s` files, `pick_target.py` flags
  `combined-subseg:<n>tu[a.s|b.s]`. Split the subseg at each TU boundary first (one `hasm` subseg per
  `.s`, the asm analog of the multi-file C-pack split), qualify each new `hasm` line with its
  `src/libultra/<dir>/<stem>` path, then vendor each `.s` verbatim there. splat writes each split
  subseg's `.s` to `src/` only if absent; the build takes each `.o` from the path-based pattern rule. Like the
  `intrinsic-likely` path, the flag carries `(needs-define:<MACROS>)` (the union across the pack's
  TUs) when any pack `.s` references an `UPPER_CASE` asm macro the in-tree `-I` headers lack, so the
  define enabler is priced at the gate, not at a failing vendor-compile (S63: `setfpccsr.s` →
  `CFC1`/`CTC1`, which were absent because S56 vendored only `MFC0`/`MTC0`; vendor the missing
  macros into `include/sys/asm.h` verbatim from ultralib `sys/asm.h`). S62:
  `[0x823B0,asm]` held `osInvalDCache`+`osInvalICache` → split at 0x82460 → two `hasm` subsegs
  (qualified `libultra/os/invaldcache` + `libultra/os/invalicache`, slots 0xB0 + 0x80). A mixed pack
  with a C-mirrorable member splits the same way
  but vendors only the asm TUs and C-mirrors the rest (S63: `[0x8CA50,asm]` = 3 reg-shim TUs + the
  C-mirror `__osSpDeviceBusy` → 3 `hasm` + 1 `c, libultra/io/sp`). Distinct from the two cases above: (a) the mixed-pack caveat
  fires when ≥1 member has a C mirror (a member with a C upstream is excluded from the TU count, so
  the flag does not fire); (b) a *partial-TU* pack whose asm members share one `.s` (e.g.
  `__osDisableInt`/`__osRestoreInt` both in `setintmask.s`) is a single distinct TU → also does not
  fire, and needs a harder partial-TU carve (still a spike).

**KMC-as sub-lane (libkmc soft-float / 64-bit math TUs).** libkmc's hand-asm TUs (`mmuldi3.s`
`__muldi3`; `mcvtld.s` `__fixdfdi`/`__fixunsdfdi`+`__floatdidf`) are not ultralib `LEAF`/`XLEAF` TUs
and do not assemble under `LIBULTRA_ASFLAGS`. They use **KMC register conventions** (`move dst,zero`
→ `addu` encoding) and must be assembled with the **KMC assembler** `$(KMC_AS)`, the same toolchain
the in-tree disassembly-reassembly path uses. `mk/libkmc.mk` carries these under a **path-based pattern
rule** `build/src/libkmc/%.o: src/libkmc/%.s` (separate from the ultralib `src/libultra/%.o` rule,
which hard-codes the `LIBULTRA_ASFLAGS` profile): `$(KMC_AS) -EB -mips2 -I src/libkmc -o $@.tmp $<;
cp $@.tmp $@; rm $@.tmp` (no objcopy; the 16-byte-slot pad happens inside KMC `as`).
`pick_target.py` flags it `intrinsic-likely:<tu>.s(kmc-as)` so the
gate reaches for the KMC-as recipe, not the ultralib one: the libkmc analog of the ultralib
`intrinsic-likely:<tu>.s`. It fires when an asm-only primary the pure-shim + privileged tests
both miss (a branchy cvt routine, no CP0/FPU-ctrl op, no `handwritten` tag) matches a libkmc `.s`
`.globl` and has no C upstream; the `not in upstream_index` guard excludes the C-mirrorable libkmc
files (`memset`/`strcmp`/`rand` resolve via the C index), leaving only the asm-only math TUs. Three
KMC-as specifics:
- **`.include "mips_as.h"`.** A libkmc `.s` may `.include` its tiny KMC header (`mcvtld.s` →
  `mips_as.h`, one line `.equ FPU,1`). Vendor that header alongside the `.s` (`src/libkmc/mips_as.h`)
  and add `-I src/libkmc` to the KMC-as rule. (`mmuldi3.s` is self-contained and needs neither.)
- **`li $X,0xffffffff` → `addiu $X,$0,-1` (the documented encoding edit).** KMC `as` expands
  `li reg,0xffffffff` to a 2-insn `lui+ori`; the ROM uses the 1-insn `addiu reg,$0,-1`
  (`2403ffff`/`2407ffff`). Each extra word shifts the rest of the TU down → SHA-1 break (the
  `.align`/trailing-nop pad absorbs the size so the function boundaries hold, but the bytes diverge).
  Rewrite each such `li` to the explicit `addiu`, the **only** allowed edit to the vendored libkmc
  `.s`, touching no real `.text` semantics. Same divergence already applied to the vendored
  `mmuldi3.s` (six `li $9,0xffffffff`); S109 applied two in `mcvtld.s`. Diagnose by `cmp`-ing the
  assembled `.o`'s `.text` against the baserom bytes (Python byte-slice; `dd` is hook-blocked): a
  one-word downstream shift starting at a `lui …ffff` / `ori …ffff` pair is this exact case.
- **Multi-fn TU spanning >1 splat subseg → merge to one `hasm`.** A KMC-as TU defining several
  functions (`mcvtld.s` = `__fixunsdfdi`@0x8F020 + `__floatdidf`@0x8F140) extracts as several
  adjacent `asm` subsegs (one per fn). One source `.s` → one `.o`, so merge them into a single
  `[<first-rom>, hasm, libkmc/<stem>]` (delete the later subseg line(s)); the merged
  subseg's extent (to the next subseg) must equal the TU's assembled size. The asm analog of the C
  `single-file-pack` (one upstream file → one atomic mirror, no inter-fn split). 0 `symbol_addrs`
  adds when both fns are already named.

**Provenance:** S56 (pilot; 4 reg shims `getcount`/`getcause`/`getsr`/`setcompare`; KMC-as padding the
load-bearing discovery, per PO directive to use ultralib's exact flags). S57 (4 cache/TLB primitives
`osWritebackDCacheAll`/`osWritebackDCache`/`osUnmapTLBAll`/`__osProbeTLB`, all first-try clean; added
the `needs-define` pre-check + this bisect protocol). S58 (3 cross-dir TUs:
`sqrtf`/`osMapTLBRdb`/`bcopy`, clearing the `WEAK`-alias + FPU-op cases). S62
(`osInvalDCache`+`osInvalICache`: the combined-subseg split sub-pattern + the `combined-subseg`
pre-flag). S63 (the `[0x8CA50]` reg-shim "set" family `setfpccsr`/`setsr`/`setwatchlo` + the C-mirror
`__osSpDeviceBusy`: first mixed asm-mirror + C-mirror subseg-clear; extended the `needs-define`
pre-check to the `combined-subseg` path after `CFC1`/`CTC1` surfaced at a failing vendor-compile). S70
(`intrinsic-likely:cp0-asm(identify-TU)` sub-trigger for an un-named privileged-op `func_<addr>`, e.g.
`osMapTLB`/`osUnmapTLB`). S84 (`osSetIntMask` from `[0x7E360]` mixed pack: first vendored `.s` with a
`.rodata` LUT; added the `has-rodata` pre-flag + the vendor-`.text`-only / strip+rename-the-blob
sub-case above; `pimgr` C member carried, `epirawdma` C member banked same sprint). S91 (has-rodata
pre-flag gated to the active build; the symbolic-pointer-table label-export first listed as an untried
option, then mis-framed a spike). S107 (`exceptasm` label-export asm-mirror: the symbolic
`.word <label>` jtbl re-export procedure, 8-fn OS exception/dispatch TU). S109 (`mcvtld.s`
`__fixunsdfdi`+`__floatdidf`: first documented **KMC-as sub-lane** TU: the libkmc `$(KMC_AS)`
explicit-rule path + `.include "mips_as.h"` via `-I src/libkmc` + the `li 0xffffffff`→`addiu` encoding
edit + the multi-subseg merge-to-one-`hasm`; the `intrinsic-likely:<tu>.s(kmc-as)` flag).

---

## recover-extern (refs-unplaced)

**Rule:** A clean verbatim leaf often references a global the upstream declares `extern` (defined in
not-yet-decompiled lib code) that is absent from both name files. A *referenced* extern is safe to
place; recover its vram deterministically and add it to `symbol_addrs.txt` before the flip. This is
the lowest-risk mirror and batches well.

**Trigger:** `pick_target.py` flag `refs-unplaced:<g>@0x<ADDR>`. The vram is inlined when the binding
is unambiguous (one unplaced name ∩ one asm candidate); otherwise bare names.

**Procedure (zero iteration):**
1. **Confirm the vram from the target fn's own `lui/addiu` HI/LO16 pair** via MCP
   `disassemble_function`. This re-confirm is mandatory. To address the call, look up the *target
   fn's own* vram first: if its curated name is already in `ghidra_symbols.txt`/`symbol_addrs.txt`
   read it there (authoritative), else derive it from the yaml `main` segment base
   (`vram 0x80025C00 @ rom 0x1000` → `vram = rom + 0x80024C00`). Never guess a flat
   `rom + <round constant>`: S22 caught a flat guess that resolved mid-function and silently
   returned a different ~1000 B fn, flagged only by the size mismatch vs the expected leaf.
2. **Indexed-struct-array externs:** the inlined `refs-unplaced` vram is the *field-access* address,
   not the array base. Recover the base by subtracting the `lbu/lw` displacement's struct-member
   offset (S20: `nuContRmbCtl` inlined 0x80104F57 = base + offsetof(`.mode`)=7 → base 0x80104F50).
3. **Add a data extern** to `symbol_addrs.txt` (add-only, disjoint from `ghidra_symbols.txt`):
   `<g> = 0x<ADDR>; // size:0x<n>` (no `type:func`). Size rule: scalar / function-pointer / word =
   `size:0x4`; array = `size:0x<stride×count>`, where stride comes from the index multiply in the asm
   and count from the index bound / `MAXCONTROLLERS` (e.g. `nuContRmbCtl` 0x28 = 10×4, `nuContPfs`
   0x1A0 = 0x68×4).
4. Flip the subseg, verbatim `cp` the upstream, `make` → ROM SHA-1 is the proof.

**Sub-cases / variants:**

**Contiguous `.bss`-block fast-path:** a file-static drop-to-extern mirror often references a
whole run of adjacent `.bss` statics declared together in the upstream `.c` (e.g. pimgr.c's
`piThread` / `piThreadStack` / `piEventQueue` / `piEventBuf`). Recover the entire block from a single
`disassemble_function` of the one fn that touches them: each base is a `lui/addiu` HI/LO16 pair (or a
`STACK_START` top-of-stack value = base + sizeof), and each **size is the gap to the next symbol in
the run**, cross-checked against the C type (`OSThread`=0x1B0, `OSMesgQueue`=0x18, `OSMesg[1]`=0x4,
`STACK(_,N)`=`ALIGN8(N)`). The run is laid out in source-declaration order at consecutive `main_bss`
vrams (pimgr's block sat immediately below the prior io file's `piAccessBuf`), so one disassembly plus
the gap arithmetic yields every `symbol_addrs` size:-extern at once, with no per-symbol re-disassembly.
**Stack-top-equals-named-adjacent-symbol tell.** A thread-stack `static T Stack[N]` passes its
*top* (`Stack + N`, stacks grow down) as the `osCreateThread` sp arg, so its asm `%hi/%lo` resolves to
`Stack`'s **end**, not its base. When that end lands exactly on an already-placed symbol from a
different TU, the asm shows `%lo(<NamedSymbol>)` (a real name, not a `D_<addr>` auto-label): that named
symbol's addr is the stack's exact end, so `base = <NamedSymbol> addr - STACK_SIZE` with zero gap
arithmetic. (S115 `nugfxthread.c` `GfxStack`: sp arg `%lo(PiMesgQ)`=0x800F74A0, `PiMesgQ` placed
S99/nupiinit.c → `GfxStack` base 0x800F54A0, size 0x2000 = `NU_GFX_STACK_SIZE`.) A `D_<addr>` label
there means the next symbol is itself still un-named — fall back to `base = top - sizeof`.

**Unindexed-upstream mirror → no auto refs-unplaced.** A mirror candidate whose `upstream`
column is `none` (a coddog-match, a de-ranked carry-over, or any source not in the upstream index)
gets **no** refs-unplaced scan at all; the hazard is computed only for a candidate with an indexed
upstream `.c`. So an inline `extern <type> <name>[];` data dep declared in the upstream BODY (not a
header) won't be auto-flagged; recover it manually at the gate from the asm `%hi/%lo` and place it
add-only before the flip. NB the *detection* is not the gap: `EXTERN_DATA_DECL_RE` +
`declared_extern_data` already match the inline array / macro-type form (`extern XLONG _atbl[];` →
`_atbl`) once the upstream is indexed. S112 `atan.c`/`sin.c` (libkmc math, upstream `none`, de-ranked
carry-overs) needed `_atbl`@0x800C9690 placed by hand; indexing libkmc math was rejected as low-value
(carry-overs) plus reclassification-risk.

**Existing extracted data/string symbol:** when the target asm loads an existing extracted data symbol,
reference that symbol as an `extern` instead of emitting a C literal or definition. A text-identical
tiny mirror can still SHA-miss if the C body creates a new `.rodata` entry and shifts later data; the
target's own `%hi/%lo` pair tells whether the bytes should stay in the extracted blob.

**Provenance:** S12 (inline-vram idea), S19 (3/3 `nuGfx*FuncSet` trio banked at one-file risk), S20
(indexed-array base correction), S22 (flat-guess trap), S90 (contiguous `.bss`-block sized by
inter-symbol gaps), S115 (stack-top-equals-named-adjacent-symbol tell), S112 (unindexed-mirror
no-scan note), S193 (existing string extern).

---

## calls-unplaced (function-callee dual)

**Rule:** The same recovery applies to an unplaced *function* the leaf calls. A callee labelled
`func_<addr>` in the asm (absent from both name files) has no symbol for the verbatim C reference to
bind, so the mirror link-fails once the body calls it by name.

**Trigger:** `pick_target.py` flag `calls-unplaced:<fn>@0x<ADDR>` (vram inlined when one unplaced
call ∩ one `func_<addr>` jal). The gate build-check cannot catch this: the `INCLUDE_ASM` scaffold
resolves the jal directly, so the missing symbol only bites in the execution middle.

**Procedure:** The callee's vram **is** its jal target. Add `<fn> = 0x<ADDR>; // type:func` to
`symbol_addrs.txt` (add-only) at the gate, before the flip. Reconcile the full callee list (data
and functions) against the name files in the same disassemble pass that confirms a recover-extern
vram (the asm jal list is already in hand).

**Sub-cases / variants:**

**Renamed-vs-substituted callee, body-compare before editing.** When the upstream calls
`fooBar` but the jal target is already named *something else* (`calls-unplaced:fooBar` flagged, yet
the asm `jal`s a placed symbol with a different name), there are two cases needing opposite
fixes. **(1) renamed:** same function, decomp gave it a different name, so the mirror is still
verbatim; just confirm the name and leave the body (no edit; the placed name binds the jal).
**(2) substituted:** this ROM calls a *genuinely different* function for that slot, so edit the C body
to call the placed name (a known-edit near-verbatim mirror). **Do not assume (1).** Disassemble the
target and body-compare it against the upstream callee's prebuilt `.o`
(`mips-linux-gnu-objdump -d ~/development/repos/ultralib/build/J/.../<callee>.o`): stack frame, error
paths, sqrt/branch handling. A fast first discriminator is the address: a libultra callee lives in
the 0x800A_0000+ block; a target in the game code region (low 0x8002_xxxx) is almost certainly a game
substitution. (S61 `guRotateF`: upstream `guNormalize`, but the ROM `jal`s `vec3f_normalize`@0x80029900.
Body-compare proved a *different* fn (game region; −0x28 frame + `osSyncPrintf` degenerate-input
error path + (0,1,0) fallback + bare `sqrt.s`, vs `guNormalize`'s −0x20 frame + `sqrtf` NaN-check, no
error path), so the body edit `guNormalize`→`vec3f_normalize` was correct, not a name reconciliation.)

**Handler address-of ref, not a jal — pre-name the next candidate's leader.** A verbatim mirror
can reference a function *by address* rather than by call: a `(handler = (Cast)fn)` assignment compiles
to a `lui/addiu %hi/%lo(fn)` pair, not a `jal`. `pick_target.py`'s `calls-unplaced` scan keys on `jal`
targets, so an address-of ref to a `func_<vram>` is not flagged there (nor as `refs-unplaced`, which is
data) — it surfaces only when the body compiles. Recover it the same way: read the `lui/addiu` pair in
the disassemble pass that confirms the jal callees, and add `<fn> = 0x<vram>; // type:func` at the gate.
Frequently the target is the leader of another still-asm candidate subseg, so naming it correctly
**pre-names that future candidate** (its `pick_target` row shows the curated name, not `func_<vram>`) and
can resolve a sibling spike for free. (S136 n_synthesizer's `mainBus->filter.handler = n_alFxPull` /
`n_alAuxBusPull` placed `0x8009FD40`=n_alFxPull (n_reverb leader) + `0x8009E4B0`=n_alAuxBusPull (n_auxbus
leader), and the `n_alSynAllocFX`=0x800A1320 jal target resolved the S130 `n_mainbus` 2-fn-subseg spike's
`func_800A1320` identity.) Confirm the name by semantics (which handler slot / call site) before adding.

**Typedef'd function-pointer param is a `jalr`, not a callee.** A parameter typed by a
function-pointer typedef (`alN_PVoiceNew(N_PVoice *mv, ALDMANew dmaNew, ALHeap *hp)`) is invoked
`dmaNew(&mv->dc_dmaState)` via a `jalr` through the pointer, not a named `jal`, so it must not flag
`calls-unplaced`. `_fn_ptr_param_names` already drops the explicit `T (*name)(args)` and `T name(args)`
param forms, but a typedef'd param reads as a plain scalar (`ALDMANew dmaNew`), so it needs the project
typedef set: `all_fn_ptr_typedefs` scans the `-I` headers for `typedef <ret> (*NAME)(args)` (ALDMANew
lives in `include/libultra/PR/libaudio.h`) and `_fn_ptr_param_names` matches `<typedef> name`. Recurs
across the audio `*New` constructors. Without it, `calls-unplaced:dmaNew` is a phantom on n_drvrNew.c.
(The asm-jal-budget `_reconcile_calls_unplaced` already drops such phantoms for a single-fn row via the
unnamed-`jal` count, but the per-coddog-member c-combined path prices each member file's
`calls_unplaced` without that per-member reconcile, so the source-side typedef suppression is the
robust fix.)

**Provenance:** S23 (`osEPiStartDma` → `osPiGetCmdQueue`@0x800B06F0); S61 (`guRotateF` substituted
`vec3f_normalize`); S136 (handler address-of pre-naming the n_reverb/n_auxbus leaders); S139 (typedef'd
fn-ptr param `ALDMANew dmaNew` is a `jalr`, suppressed via `all_fn_ptr_typedefs`).

---

## Macro-hidden recover-extern

**Rule:** A recover-extern (data or callee) can hide inside a library macro the leaf invokes rather
than appearing in the `.c` body, so neither the `.c`-source ref-grep nor the gate build-check sees
it and the mirror link-fails mid-execution. When mirroring a leaf that invokes a library macro
(`EPI_SYNC`, `EPI_*`, `WAIT_ON_IOBUSY`, …), expand the macro and reconcile its referenced
globals/callees too.

**Trigger:** `pick_target.py`'s `refs-unplaced`/`calls-unplaced` now follow one level of macro
expansion, so a macro-hidden extern usually surfaces as a normal flag. Confirm against the asm.

**Procedure:** Identical to recover-extern: read the unplaced global's vram from the target fn's
own `lui/addiu` HI/LO16 pair in the extracted asm, add the `symbol_addrs.txt` entry, `make extract
&& make`. Note: when the index (e.g. `domain*4`) is a separate `addu`, the `%hi/%lo` base is direct,
with no field-offset subtraction.

**Provenance:** S41 (`__osEPiRawWriteIo` → `EPI_SYNC` references `__osCurrentHandle[domain]`;
`__osCurrentHandle` = D_800C7E90, an `OSPiHandle*[2]` array → `size:0x8`).

---

## Near-verbatim mirror (jal-count-mismatch)

**Rule:** This ROM's build can disagree with the upstream `.c` on which calls it makes. The fix
depends on the size of the difference.

**Trigger:** `pick_target.py` flag `jal-count-mismatch:<C>vs<asm>` (advisory: it strips dead
`#ifdef _DEBUG`/`#ifndef _FINALROM` first, but macro/inlined calls can skew the count). Confirm at
the gate by disassembling and comparing the jal list against the upstream call list.

**Procedure:**
- **Small (≤2): try verbatim first.** The mismatch may be a GCC -O3 tail-merge artifact (one shared
  `jal` for identical call+args across branches). Verbatim copy + `make`; only investigate
  line-drops if the SHA-1 doesn't match. (S35: `osStartThread` `9vs7` = three identical
  `__osEnqueueThread(&__osRunQueue, t)` sites merged.) **Do not hand-fold an early-return to force
  the count.** When the upstream has two *identical* tail blocks (an early-return
  `if (cond) { f(); return X; }` plus the same `f(); return X;` at the end), -O3 **cross-jumps**
  them into one shared tail itself, which **is** the `jal N-1`. Copy verbatim and let the compiler do
  it; rewriting to `if (!cond) { body }` changes register allocation and can emit a *different-sized*
  function. **Wrong-size diagnostic:** if a verbatim/near-verbatim mirror's built function differs
  from the baserom in *instruction count* (not just a local field), the cause is regalloc /
  tail-merge, not a logic bug, and every downstream function then shifts and the whole ROM mismatches
  (a far worse symptom than a one-row diff). Restore the verbatim structure before chasing it. (S47:
  `osCartRomInit` `6vs5`; hand-folding the `if(!first){rel;return;}` early-return compiled 0x10
  short (1 vs 2 callee-saved regs); the verbatim source cross-jumped to the exact baserom regalloc.)
- **Large (>2): default to classical.** The ROM may implement different logic (different branches,
  args, stripped control flow). Disassemble and compare structure vs the upstream body before
  routing; only a confirmed clean line-drop stays a mirror. (S30: `osSetTimer` `5vs2` was a
  stripped classical, not a 3-call drop.)
- **Confirmed clean drop:** stays a mirror. Copy verbatim, drop the diverging line(s)
  (deterministic from the asm jal list), `make` → ROM SHA-1 is the proof. (S18: `nuContInit` drops
  the absent `nuContPakMgrInit` call.)
- **Jal-less dropped block (jal count can match).** A dropped block need not contain a call, so the
  jal count can agree while the build still diverges and `pick_target.py` flags nothing. A verbatim
  mirror that links fine but misses the ROM SHA-1, with no flagged hazard, often means this ROM omits
  a jal-less upstream guard/early-return (or replaces a multi-call block with a one-liner). Don't
  iterate C blind: `.o`-diff against the baserom asm first (same first-SHA-miss remedy as the cast
  divergence below). The dropped lines are deterministic from the asm structure. (S45 `osGbpakReadWrite`
  drops `if (size == 0) return 0;`, a jal-less early-return that folds into the later `blez` size<=0
  path; `osGbpakReadId` replaces the upstream `if(bcmp){ write-temp; reread; recheck }` retry block
  (5 calls + a `temp[32]` local) with `if (bcmp(...)) return 4;`, where the jal count *did* diverge.)
- **nusys per-file version wrapper (libnusys int-mask drop).** A libnusys mirror can mismatch
  because MG64's build pins an older nusys revision *per file* than the in-tree default (the
  n64sdkmod nusys-2.07 tree); the version is not uniform. The
  concrete recurring delta in the `cont`/RMB family: nusys-2.05 wrapped the function body in an
  `osSetIntMask(OS_IM_NONE)` ... `osSetIntMask(mask)` pair (a `OSIntMask mask;` decl + the two
  calls), kept through 2.07; the pre-2.05 (2.00/1.x) source omits it. **Tell:** MG64's asm is a
  **leaf** (no `addiu $sp,-N` / `sw $ra`, 0 jals) but the 2.05+ upstream wraps the body in two
  `osSetIntMask` jals → a `jal-count-mismatch:2vs0`-style flag whose surplus is exactly the wrapper.
  **Confirmed clean drop:** stays a mirror — copy the 2.07-sdk file verbatim, drop the 3-line
  int-mask wrapper (keep the English comments), which **is** the 2.00 leaf code; ROM SHA-1 is the proof.
  A sibling whose code is version-identical (comments-only diff) copies straight from 2.07-sdk.
  Diagnose the version with the per-revision source set under `~/development/repos/nusys/src/<ver>/`
  (`grep -c osSetIntMask <ver>/nusys/<file>.c`). `pick_target.py` now annotates this case
  `jal-count-mismatch:<c>vs<asm>(version-artifact?)` for libnusys so smallest-first is not deterred.
  (S118 `nuContRmbModeSet` — leaf 0-jal = 2.00 variant, int-mask dropped; sibling
  `nuContRmbForceStop` was version-identical and copied straight from 2.07-sdk.)
- **nusys per-file version block reorder (no count signal; "no coddog" is the tell).** A per-file
  version divergence need not change the call/instruction count — it can be pure block order. MG64's
  per-file nusys revision can order two top-level blocks differently from every archived rev, and no
  available source reproduces it. **Nothing count-based fires:** the jal count agrees with the asm and
  the instruction count is identical; the only standing tell is the absence of a `coddog-mirror:`
  match (a reorder breaks coddog's structural fingerprint, so "no coddog" on a jal-flagged libnusys
  target is a divergence signal, not noise — `pick_target.py` now prices a jal-mismatch-without-coddog
  target one above the mirror floor). **Symptom:** the verbatim cp builds + links clean but
  the ROM SHA-1 misses; an in-tree-`.o` vs baserom-asm objdump diff shows the same instruction count,
  same ops, reordered (with cascading regalloc differences). **Procedure:** read the asm's jal/block
  order (which call/block executes first), reconstruct the true source order — a C compiler does not
  reorder across an early-return-gated side-effecting call, so the asm order **is** the source order —
  hand-swap the blocks in the closest English rev (2.07), and verify insn-identical (`objdump -d`
  diff of the in-tree `.o` vs `build/asm/<rom>.o`) plus full-make ROM SHA-1. (S119 `nuContGBPakFread`:
  the RAM-enable block — `bzero` / `data[31]=…` / `nuContGBPakWrite` — runs before
  `nuContGBPakCheckConnector` (`ram=0` in the range-check `beqz` delay slot, `jal CheckConnector` at
  the skip-label), but every archived 1.20/2.00/2.05/2.06/2.07 is CheckConnector-first; swapping the
  two source blocks gave the byte match. Its `jal-count-mismatch:5vs9` was an unrelated macro
  artifact: nusys.h `nuContGBPakRead`/`Write` expand to `nuContGBPakReadWrite`, so 9 asm jals == 9
  expanded call sites — confirm by expanding the macros before counting, do not read it as 4 dropped
  calls.) Generalizes the "nusys version is per-file" finding from wrapper-presence to
  block-order divergence.

**Sub-cases / variants:**

**Sibling-replay.** Once one fn in a family is confirmed a block-reorder mirror, its siblings
replay the same swap: `nuContGBPakFwrite` (S120) is the direct sibling of S119's `nuContGBPakFread`
(its asm runs the RAM-enable block before `nuContGBPakCheckConnector` identically, with the same
benign `nuContGBPakRead`/`Write`→`ReadWrite` macro `jal-count-mismatch:5vs10`). Applying the swap
up-front from the asm + the sibling precedent banked it first-build, 0 iteration (vs S119, which
discovered the reorder via a re-attempt). `pick_target.py` now surfaces
`block-reorder-sibling:<sibling.c>` on a libnusys candidate carrying the tell (unexplained
jal-mismatch + no `coddog-mirror`) whose upstream-file basename is in a known block-reorder family
(`BLOCK_REORDER_FAMILIES`, seeded `nucontgbpak*`), so the gate plans the swap instead of re-deriving
it. The flag is advisory: the seed keeps its +1 near-verbatim risk (the hand-edit is still real at
plan time), but a sibling-known swap that banks first-try scores the realized −1 verbatim-first-try
tier (VELOCITY S120 #3).

**Provenance:** S18, S30, S35, S45, S47, S117, S118, S119, S120.

---

## Mirror cast divergence (sign- vs zero-extend)

**Rule:** A verbatim mirror can match every instruction except the width-extension of one 64-bit
context/field assignment, when this ROM's source casts differently from the upstream `.c`. KMC GCC
emits `sra rd,rs,0x1f` for a sign-extending `(s64)(s32)x` but `move rd,zero` (writing the high word)
for a zero-extending `(u64)(u32)x`: same low word, different high word. Invisible to every gate
check (jal count, ref/header grep) and the `INCLUDE_ASM` gate build; surfaces only as a full-make
ROM SHA-1 miss in the execution middle (the same late-surfacing class as jal-count-mismatch /
wrong-lib-header).

**Trigger:** a clean-mirror flip (no hazard flagged) whose verbatim copy links fine but the ROM
SHA-1 doesn't match, and the byte diff is isolated to one field's high word.

**Procedure:** on the first SHA miss of a context-building / pointer-into-64-bit-field mirror, diff
the compiled `.o` (`mips-linux-gnu-objdump -d build/.../<file>.o`) against the baserom asm before
assuming a deeper problem. If the only delta is `sra rd,rs,0x1f` (baserom) vs `move rd,zero` (yours)
or vice-versa, flip that one cast: `(u64)(u32)x` ↔ `(s64)(s32)x`. Match the sibling fields:
VERSION_J commonly sign-extends the whole `OSContext` block, so align an outlier zero-extend to its
neighbors.

**Provenance:** S44 (`osCreateThread`: `context.ra = (s64)(s32)__osCleanupThread` sign-extend, vs
the old 2.0L upstream's `(u64)(u32)` zero-extend; sibling `sp`/`a0` already sign-extended). This
divergence is one reason the project standardized on ultralib VERSION_J as the sole upstream.
Late-surfacing analogues: jal-count-mismatch S18, wrong-lib-header S40.

**Sub-cases / variants:**

**Float-literal single-vs-double (classical FP reconstruction).** When a classical/mirror
reconstruction adds a float comparison or clamp against a literal (`if (x < -32768.0)`), a **bare
floating literal is `double`**: KMC GCC promotes the `f32` operand (`cvt.d.s`), compares as double
(`c.lt.d`), and **loads the double constant from `.rodata`** (`lui at,0; ldc1`). The ROM, built from
a `float`-literal source, compares **single** (`c.lt.s`) with the constant **inline** (`lui/mtc1`, no
rodata). Symptom: insn count matches but the compares are `.d`, plus a spurious `.rodata` double the
mirror would need to carve. **Fix:** suffix every FP literal in a compare/clamp/assignment with `f`
(`-32768.0f`, `32766.0f`) so it stays single-precision and inline. Provenance: S103 (`guMtxF2L`'s
Monegi clamp, `if (mf[i][j*2] < -32768.0f) … > 32766.0f`, consts `0xc7000000`/`0x46fffc00`; bare
doubles first compiled `c.lt.d`+`cvt.d.s`+a rodata pair before the `f` suffix fixed it in one
iteration). See #game-region-mirror--o2-profile for the same sprint's profile gotcha.

---

## file-static (BSS-layout-conflict)

**Rule:** A file-scope `static <type> <name>;` (BSS global) in the upstream blocks the verbatim
mirror: KMC GCC emits a section-relative `.bss` reloc, the linker can't place the local BSS at the
splat-side `next` slot, the 0x10-aligned chunk inserts ahead and shifts every downstream BSS symbol,
breaking every code reloc into that range (10,853 ROM bytes diff for `rand`). Function-local
`static` is fine; only file-scope is the problem.

**Trigger:** `pick_target.py` flag `file-static`. **Static *functions* do not count:** a
file-scope `static <type> <name>(...);` (proto/def) shares the mirror's TU and is no BSS hazard, so
`has_file_scope_static` skips it (sprintf's `static void* proutSprintf(...);` was a false flag
that mis-routed a clean 2-fn mirror toward the classical loop). The detector strips
`__attribute__((...))` before the declarator check so an attributed static *array*
(`static OSMesg buf[N] __attribute__((aligned(8)));`) still flags. **The scan runs on
comment-stripped text.** `FILE_STATIC_RE` anchors on `;\s*$`, so a trailing `/* ... */` after the
`;` (nupiinit.c's `static OSMesgQueue PiMesgQ __attribute__((aligned(8)));  /* PI message queue */`)
silently defeated the match pre-fix; the same comment-strip un-suppressed `defines-data` (a
`/* ... ( ... */` banner like `Copyright (C) 1997` falsely tripped the K&R-param guard, hiding every
subsequent depth-0 global across the whole nusys band). **Also `file-static` now unions over a
c-combined pack's members** (like `defines-data`): a secondary member's file-scope static
(`nuContGBPakFwrite`'s `nusimgr` member) is a pack-level drop-static enabler the primary-only scan
missed. Known gap: an *initialized*
static (`static T x[] = {...};`) carries `=` and is invisible to the regex, so it is not flagged
(caught at the gate / link if a sub-fn of a decompose pack is later mirrored).

**Procedure:** Fall to the classical loop with the `static` **dropped** in the C body. The linker
then resolves to the splat-side global at the correct vram.

**Sub-cases / variants:**

**Uninitialized file-static is a drop-to-extern mirror, not a carve/classical spike.** Be
precise about what "BSS-layout-conflict" costs. An *uninitialized* file-scope static (`static T x;`,
no `=`) is **pure `.bss`**: it occupies no ROM bytes (`.bss` is zero-filled at runtime, absent from
the image), so it cannot shift the ROM. The only requirement is that the `.text` `%hi/%lo` relocs
into it resolve to the right addresses, which a **drop-to-`extern`** delivers: drop each `static T x;`
to a sized `extern T x;` (size `viThreadStack`-style arrays via the same macro so `sizeof`/`STACK_START`
still compute), then place each at its `main_bss` vram (`x = 0x<vram>; // size:0x<n>`, recovered from
the fn's `lui/%lo` like any recover-extern). The compiled object then emits **no `.bss`** and the body
is byte-identical → it banks atomically on the full-make SHA, exactly like an S81 drop-def. This is the
**S81 `siacs.c` pattern generalized** (drop-to-extern, no carve, no classical seed loop), *not* the
`rand` carve. The tell is initialized-vs-uninitialized: `FILE_STATIC_RE` only matches the
uninitialized form (an `=`-bearing static is invisible to it), so a flagged `file-static` is already
the pure-`.bss` kind. A **func-local** uninitialized `static` (S87 `vimgr` `retrace`) is the same:
hoist it to a file-scope `extern` + place it (it would otherwise emit a local `.bss` symbol the linker
places at the wrong address). Escalate to a `.data` **carve** (the `rand`/S61 playbook) only when a
static carries a **nonzero** initializer (real `.data` bytes that do shift the image); `={0}` is still
`.bss`. (S87 `io/vimgr.c`: 6 file-statics + 2 globals + 1 func-local static, all dropped to externs,
0 carve, first-build SHA match; the carry-over's "heavy `.bss` carve" framing was the false-flag this
retires.)

**Pre-curated `.bss` static ⇒ reference the ghidra name, no symbol add.** The drop-to-extern
above has two naming sub-cases, set by whether the placed `.bss` symbol is already named: (a)
**unnamed** (only a splat `D_<vram>`, in neither name file) ⇒ drop to `extern <T> <upstream_name>;`
and add `<upstream_name> = 0x<vram>; // size:0x<n>` to `symbol_addrs.txt` (S81 `siacs.c`; S141
lib_memory `audio_heap`). (b) **pre-curated** (the vram already carries a curated data name in
`ghidra_symbols.txt`) ⇒ drop to `extern <T> <ghidra_name>;` and rename the active-branch body
references to that name, with **no `symbol_addrs` add** (a same-vram add would dup-clash the existing
ghidra entry, the disjoint-files rule). The rename is the forced verbatim divergence the `.text`
relocs require: the body's `lui/%lo` must resolve to the placed vram, so the C must spell the symbol
exactly as splat names it. S142 `aud_samples.c`: `frame_samples{,_min,_max}`/`extra_samples` were
pre-curated `g_mus_frame_samples`@0x800E72C0 / `_min`@72C4 / `_max`@72C8 / `g_mus_extra_samples`@72CC,
so the drop was 4 `extern u32 g_mus_*;` + renaming the `#else SUPPORT_NAUDIO` body, zero symbol adds,
`.o .bss`=0, first-build SHA match.

**hasm referrers need a byte-neutral symbol sync.** A BSS drop-to-extern (or recover-extern)
names a `D_<vram>` that `make extract` re-labels everywhere except `hasm` files (the entry stub,
vendored asm), which are never re-extracted. If a `hasm` referenced the old `D_<vram>`, naming the
symbol orphans that reference (`undefined reference to D_<vram>` at link). S117: naming
`nuContNum`=0x8010C2D0 broke `src/entry/entry.s`, which loads that address as the `_start` initial
`$sp` (the boot stack top **is** `&nuContNum`, the highest BSS global, so the boot stack grows down through
the lower BSS and abandons it before `nucontmgr` runs). Fix: a byte-neutral `D_<vram>`->`<name>` edit in
the `hasm` (same address => identical `%hi/%lo`, ROM SHA-1 unchanged). Anticipate it at the gate: grep
`src/**/*.s` for the `D_<vram>` of each to-be-named drop-def/recover-extern symbol before the build, so
the sync is planned, not a mid-build link error. (A [`caller-evict`](#caller-evict) sub-case for data
symbols.)

**The `drop-static-mirror:<n>bss` re-frame tag.** When a `coddog-mirror:<file>@≥99` (non-audio)
is on the row, a `file-static` is present, and there is **no** carve signal (`rodata-literal` /
`data-static` / `rodata-jtbl`), `pick_target.py` appends `drop-static-mirror:<n>bss`: the leading
verdict that the co-listed `file-static` + `defines-data` + `refs-unplaced` flags are **one
drop-to-extern enabler** (the pure-`.bss` case above), not the scary 4-flag carve cluster they read as.
`<n>` counts the detector-visible defined `.bss` symbols (file-scope static lines + `defines-data`
globals); a func-local static is a known under-count, recovered at the gate with the rest. **A second
under-count class: a file-scope uninitialized non-static global that the upstream header already
declares `extern`** (S115 `nugfxthread.c`'s `nuGfxMesgQ`) is a `.bss` drop-def but is not in the
`<n>bss` tally — it carries no `=` (so `defines-data`'s K&R-guarded init scan skips it) yet is not a
file-scope `static` either, so it falls between both detectors and surfaces only under `refs-unplaced`.
The gate's per-symbol `refs-unplaced` recovery still places it, so the tally undercount is graceful
(use the union of `drop-static-mirror` + `defines-data` + `refs-unplaced` as the recover set, not the
`<n>` alone). The cluster
flags stay (the gate's per-symbol recovery + `seed_points` read them); the tag tells the gate to price
a **seed-only N-symbol mirror**. A nonzero-initialized global that slips the gate degrades gracefully
to the carve playbook on the SHA miss, never a silent wrong bank.

**Batch-adding a pack's recovered statics transiently reds the build (data-symbol caller-evict).**
When drop-static-mirroring a multi-member pack one member at a time, add each member's recovered `.bss`
statics to `symbol_addrs.txt` **with (or just before) writing that member's body**, not all up front.
A static added while its consuming member is still an `INCLUDE_ASM` stub **evicts the asm stub's
sub-field auto-labels** (`SramHandle = 0x800F7580` killed the still-asm `nupiinitsram` stub's
`D_800F758C`/`D_800F7584`/… → undefined-reference link errors), exactly like the `#caller-evict` /
`make-sync-names` eviction but for a data symbol. It is benign and resolves the moment the consuming
body lands (the C references `SramHandle` directly, no `D_` labels), but a red build between the symbol
add and the last body is expected, not a regression. (S99 `nuPiInitSram`: added all 3 pack statics at
once, red until both pi bodies landed, then SHA-clean.)

**Provenance:** `rand` (file-scope-static pre-flight); `sprintf` (static-function false-flag, S54);
`io/vimgr.c` (S87: uninitialized file-static = pure-`.bss` drop-to-extern mirror, the
`drop-static-mirror` tag); `nupiinit`/`nupiinitsram` (S99: 3-file c-combined drop-static pack;
comment-strip + member-union detector fix; batch-add transient-red note); `defines-data`
member-union precedent (S97).

---

## defines-data

**Rule:** A leaf that *defines* a placed global (the `.data`/`.bss` analogue of file-static) hits
the same BSS-layout-conflict. Route to the classical loop with the definition dropped. (Distinct
from `refs-unplaced`, where a merely *referenced* extern is safe to place.)

**Trigger:** `pick_target.py` flag `defines-data:<g>,...`. Note `__osDequeueThread` is a
`defines-data` false-clean even inside the warm thread band; it does not fast-path.

**Sub-cases / variants:**

**Dual-section carve.** A verbatim coddog mirror can need both a `.data` carve (here) and a
`.rodata` carve ([`.rodata sibling-yaml pattern`](#rodata-sibling-yaml-pattern)) in the same
increment when the upstream file has file-scope `static` *initialized* arrays **and** a `switch` /
pooled FP constants. drvrnew.c (the al synth driver) had both: six `static s32 *_PARAMS[]` arrays
+ a `switch(fxType)` jtbl + SCALE/CONVERT/2³² f64 consts. Size each carve from the `%hi(D_<vram>)`
address band: a `0x800Cxxxx`-range ref is `.data` (carved out of `main_data` with a 3-way
split: `[start,data]` / `[carve,.data,<file>]` / `[end,data]`), a `0x800D2xxx`/`jtbl_` ref is
`.rodata` (its own subseg → attribute-change carve, or split). Both are execution-time (not gate)
enablers, landed with the real body; confirm the C-object section sizes match the carve extents
(drvrnew.o: `.data` 0x190 = 100 `s32`, `.rodata` 0x40). The `static` arrays do not trip
`file-static` (that's the BSS/uninitialized hazard); initialized statics are a `.data` carve, not a
classical reroute.

**`.data` init-static-array pre-flag.** A file-scope non-const initialized `static <type>
<name>[…] = <init>;` array (`.data` the verbatim mirror re-emits) is now pre-flagged
`data-carve:<names>` by `pick_target.py`, the `.data` analogue of the `static const` rodata-widening
signal. It is the recurring S92/S101 un-flagged class no other detector caught (`FILE_STATIC_RE`
excludes `=`-initialized lines, `defines_data_globals` skips `static`, `defines_local_static_data` is
depth≥1): xprintf's `spaces`/`zeroes`, env's `eqpower[128]`, _Litob's `ldigs`/`udigs`. `static` bars
cross-file linkage, so the array is file-private and the source-scan sidesteps the S92 own-vs-extern
blocker (the reason the asm `addiu %lo` scan was reverted). Fired only on the single-file (non
`c-combined`) subset (S101 safe slice; the multi-file per-member attribution stays a BACKLOG
follow-up). Advisory/display-only: recover the vram + exact extent from the asm at execution, then
carve as above (confirm against the `.o` `.data` section size, per the rodata extent-oracle note).

**Shared global ⇒ drop, never carve.** The carve-vs-drop choice is not free when the defined
global is *shared*, referenced by other still-asm subsegs via the splat `D_<vram>` name. Then the
**drop-to-extern is mandatory** and a carve cannot work: a carve makes the mirror `.o` define the C
*name* (e.g. `alGlobals`) while the un-flipped siblings still reference `D_<vram>`, so they go
unresolved; bridging them with a `symbol_addrs` entry then creates an absolute-symbol-vs-`.data`
double-definition. Drop is clean: one splat-side def (the `make extract` rename names the existing
`.data` dlabel), and every referrer (the mirror's `extern` + all sibling asm) resolves to it. Carve
only when this file is the **sole referrer** (a file-private initialized array/const with content the
compiler must emit, like drvrnew.c's `static s32 *_PARAMS[]`). Tell them apart by grepping the `D_<vram>`
referrers across `asm/` + `src/`: >1 distinct subseg ⇒ shared ⇒ drop. S97 `sl.c` `alGlobals`
(`D_800C8180`, refd by env @804D0 + fx @815C0 + sl @82160) dropped to the `libaudio.h` extern +
`alGlobals=0x800C8180 // size:0x4`; the 16 B `.data` (4 B ptr + 12 B section-align pad) stayed
splat-side. `pick_target.py` now unions `defines-data` over a `c-combined` pack's member upstreams,
so a secondary member's defined global is priced at the gate, not at execution.

**Function-local static ⇒ automatic sole-referrer carve.** A `static <T> <name> = <init>;`
declared inside a function is file-private by construction (no external linkage, no `D_<vram>` a
sibling can name), so it is always the sole referrer ⇒ carve, and the `asm/`+`src/` share-grep above
is unnecessary (the share-vs-drop question only arises for file-scope `defines-data`). Used-vs-unused
does not change this: both carve (KMC -O3 emits the initialized local to `.data`) — S138 n_reverb's
`val`/`lastval`/`blob` were unused, S142 aud_samples's `only_one_flag`=1 was used, both sole-referrer
carves. S142 `aud_samples.c`: `only_one_flag` (a `__MusIntSamplesCurrent` local) carved
`[0xA2F00,0xA2F10)`=0x10 (4 B + 0xC section-align pad), `.o .data`=0x10, a 1-line split of
`main_data`'s tail.

**Shared at a field, and undroppable ⇒ carve + canonical-addend.** Two refinements to the
S97 share-check. (1) Sweep the whole object range `base..base+size`, not just the base `D_<vram>`: a
sibling that reads a *field* references `D_<base+N>` (a different label), so a base-only grep misses
it and wrongly reads "sole-referrer." (2) "Shared ⇒ drop" needs an **"and droppable"** qualifier — a
shared global whose initializer chains to file-private `static`s cannot be dropped (dropping the def
orphans the statics → the compiler can dead-strip them → `.text` mismatch), so the **carve is forced
even though it is shared**. Resolve the sibling's field reference by naming the carved global
**canonical + sized** (`nuContGBPakCallBack = 0x800C7E20; // size:0xC`): splat then renders the
sibling's `D_<base+4>` as `nuContGBPakCallBack + 0x4` (identical `%hi/%lo` bytes) and it binds to the
C-exported symbol. This is the prefer-canonical-over-`D_<addr>`-placeholder rule, and it beats the two
alternatives: a `D_<addr>` placeholder hides the name, and a `defined:False` absolute risks the S97
double-def. No clash arises because the base stays C-defined (it is inside the carve, so splat marks
it defined and never emits it to `undefined_syms_auto`). S116 `nucontgbpakmgr.c` (first libnusys
`.data` carve: `funcList[8]`@0x800C7E00 + `nuContGBPakCallBack`@0x800C7E20, 0x30 B `.o` `.data`, 3-way
split `main_data | nucontgbpakmgr .data | main_data_2`): the still-asm sibling `nuContGBPakFwrite`
(0x7D970) reads `nuContGBPakCallBack.func` at base+4; the gate base-only share-check missed it and
only the link error (`undefined reference to D_800C7E24`) surfaced the field ref. Tooling follow-up:
`pick_target.py` could report a defines-data global's field-level `base+N` external refs (not just the
base) so the share-status is priced at the gate.

**Carve extent = the `.o` `.data` section size (16-aligned), not the symbol-content sum.** The
compiler pads `.data` up to the section's 16-byte alignment, so the carve runs to the next 16 boundary,
past the last placed symbol. `nucontmgr.c`'s `.data` content is 0x24 (`nuContReadFunc` 0x4 + `funcList`
0x14 + `nuContCallBack` 0xC) but the `.o` `.data` section is **0x30** (0xC of end padding); carving 0x24
left the next subseg 0xC inside the section, so the link placed the 0x30 section over it and shifted the
data segment. Always size the carve from `mips-linux-gnu-objdump -h build/.../<file>.o` `.data`, never the
sum of `symbol_addrs` sizes; the ROM zero-padding at `carve_end..section_end` is the mirror's, owned by the
carve (it matches the compiler's `.data` tail-padding bytes).

**Unused function-local statics still emit `.data`; localize by value, not asm ref.** KMC GCC
2.7.2 `-O3` does not dead-strip an unused function-local `static` — `static f32 val=0.0,
lastval=-10.0; static f32 blob=0;` that no code path reads still emits its `.data` (here 0x10:
`0x00000000 0xC1200000 0x00000000` + tail pad). So `defines-data` on an n_audio_sc / verbatim mirror
is a certain `.data` carve enabler, never a "maybe pure-cp": pre-scope the carve. The localization
twist: an unused static carries no `%hi/%lo(D_<vram>)` in the subseg asm (nothing references it), so
the recover-vram-from-asm path that finds rodata-literals does not find it. Localize by value instead
— compile the verbatim body, read the `.o` `.data` size (`objdump -h`), then search the baserom
`.data` region for the static block's initializer byte pattern; a clean SHA-miss that is exactly the
`.o` `.data` size larger than baserom is this hazard. `n_reverb.c`: build #1 was 16 B larger (the 0x10
`.data`, nothing carved), located at rom 0xA31A0 via the `00000000 C1200000 00000000` pattern, carved
(3-way `main_data | n_reverb .data | main_data_1b` split), then byte-exact. Beware value-FP: the same
block appears at the *libultra* `reverb.c` `.data` (rom 0xA356C, its own val/lastval/blob), so
disambiguate by the n_audio_sc `.data` link-cluster (it sits just below the libnusys `.data` carves,
before 0xA31D0), not value alone.

**Tooling follow-up.** `pick_target.py` reports `defines-data:<names>` but not the resolved
`.data` rom address (`data-static:<addr>`), so the gate spends a build + cmp + value-search to
localize. A detector could resolve it by parsing the static's initializer to bytes and searching the
baserom `.data`, but must disambiguate the multi-hit value-FP (n_reverb's block matched 2 sites) via
a link-cluster / positional anchor before emitting an address — a value-only unique-hit is not safe.
Deferred to a golden-gated tooling branch (see `BACKLOG.md ## Carry-overs`; a feature with FP /
regression surface, not a quick edit).

**Procedure:** Classical loop, drop the definition; the linker resolves to the splat-side global.

**Verbatim-body fast path.** When the *only* edit from upstream is dropping the file-scope
definitions (the function body is otherwise verbatim), this is a known-edit **mirror**, not a true
classical target: prove it by full-make ROM SHA-1 like any mirror, skip the seed / `decomp_loop` /
spot-check cycle. The dropped def changes only the `.data`/`.bss` allocation (moved splat-side); the
`.text` is byte-identical once the externs land at their placed vrams, so the body matches on the
first build. S42 `osSetEventMesg` (drop `__osEventStateTab` + `__osPreNMI`, add one `extern`) matched
in one `make`. Only reach for the classical seed loop when dropping the def forces a *body* change
(e.g. an initializer the asm computes inline). Place each dropped global add-only in
`symbol_addrs.txt` at its asm-recovered vram (scalar `// size:0x4`, array `// size:stride×count`);
`pick_target.py` surfaces the array dimension as `defines-data:<name>[DIM]`.

**Function-local statics in a shared data blob.** The fast path also covers a *function-local*
`static` array whose initialized bytes live in a shared data segment (`main_data`), not in this
object's `.data` and not in a `.rodata` sibling. `pick_target.py` won't flag it (a `static` is
invisible to the refs-unplaced extern grep, and the asm names it `D_<vram>` which resolves from
`main_data`, so no link break); it surfaces only as a full-make SHA miss whose `.o`-diff shows a
nonzero `.data` section in the compiled object (the compiler emitted a *second* copy that shifts the
data segment). Fix exactly as for a file-scope global: drop the `static` def → a **sized** `extern`
(`extern u8 nintendo[48];`, sized so `sizeof`/`ARRLEN` still compiles; an incomplete `[]` errors
on `sizeof applied to an incomplete type`), then add the symbol add-only at its real vram so
`make extract` renames the `main_data` dlabel. No `.data` is emitted, no segment shift, and the
`.text` reloc target is identical (`%hi/%lo` either way). The splat `D_<vram>` name is its real vram;
a `.NON_MATCHING`-suffixed map address is an alias, not the storage. (S45 `osGbpakReadId`:
`nintendo`@0x800C93F0 size:0x30, `mmc_type`@0x800C9420 size:0x14.)

**Function-local static FP constant: the `data-static` pre-flag.** A function-local
`static float`/`double` initializer (e.g. `static float dtor = 3.1415926/180.0`) is the FP analogue
of the S45 array case: the mirror re-emits its initialized bytes into the data segment, shifting it
→ full-make SHA miss. Unlike the array case it *is* visible in asm (the fn loads it via
`lwc1/ldc1 %lo(D_<addr>)`), so `pick_target.py` pre-flags it `data-static:0x<vram>`. Crucially this
is **distinct from `rodata-literal`**: pick_target classifies the `%lo(D_)` address by segment
(vram − the fn's code-segment delta → ROM offset; in a `rodata` subseg ⇒ rodata-literal sibling
split, else ⇒ `data-static`). The two enablers differ: `data-static` is the S49 fast path here (drop
the fn-local `static` → a file-scope `extern <type> <name>;` + add the symbol add-only at its real
vram so `make extract` renames the `D_<vram>` dlabel; no `.data` emitted, `.text` reloc identical),
**not** a `.rodata` sibling split. (S49 `xseed`@0x800C81C0; S52 `dtor`@0x800C81E0 for `guRotateRPYF`;
the same gu data band holds `guRotateF`@0x800C81D0, `guAlignF`@0x800C81A0.)

**The drop-extern *hoist* vs. the verbatim-static *carve*.** Be precise about scope: this is a
**function-local** `static` (declared inside the fn, persists across calls, *internal linkage*), not
a file-scope `static`. Compiled verbatim, the compiler emits it as a **local** symbol (`dtor.2` in the
`.o`'s symbol table), so two siblings' identically-named function-local statics **never collide**:
they are distinct locals in distinct objects (`guRotateF`'s `dtor`@0x800C81D0 and `guRotateRPYF`'s
`dtor`@0x800C81E0 coexist fine as verbatim locals). The S52 drop-extern fast path *hoists* the
function-local static up into a file-scope `extern <type> <name>;` (giving it external linkage); that
is what introduces a name, and a second sibling can't hoist to the **same** file-scope name
(`guRotateRPYF` already hoisted `dtor` + claimed its `symbol_addrs` entry, so a second `extern float
dtor;` is a duplicate symbol / binds the wrong vram). So two ways place a function-local FP static:
**(a) hoist** under a *unique* file-scope name (curated `<fn>_dtor`, or the raw `D_<vram>` dlabel) +
symbol add, no `.data` emitted; **(b) keep it verbatim** (preserving the function-local semantics)
and carve a `.data` subseg for the file: `[<rom>, .data, lib<...>/<file>]` + a continuation
`[<rom+ext>, data]`. The carve is the more verbatim option (no source edit, no hoist). **If you carve,
size the extent from the compiled object, not the static's byte size:**
`mips-linux-gnu-objdump -s -j .data build/.../<file>.o` (the section is alignment-padded: `dtor` is
4B but `rotate.o(.data)` is **16B** = 4B + 12B pad), cross-checked against the baserom bytes to the
next data symbol. A short carve double-counts (the compiled `.data` injects its full padded extent
while the un-carved tail still emits the original bytes) → the ROM grows and the whole data segment
reflows (S61: a 4B carve grew the ROM +16B; the misleading `cmp` first-diff was at 0x1008, an
artifact of the size-grown file, not the real divergence). Clean-rebuild to verify. (S61 `guRotateF`:
16B carve `0xA35D0`→`0xA35E0`.)

**Unreferenced carve: locate the offset by ROM byte-search.** The carved `.data` bytes may be
entirely unreferenced: file-scope / function-local `static`s that no function reads, but which KMC GCC
2.7.2 still emits (an explicitly-`=0`-initialized static lands in `.data` not `.bss`, and an unused
initialized static is not elided at `-O3`). Then there is no `%hi/%lo(D_<vram>)` in any body: the
S61/S96 "size from the address band" step has nothing to read, and `pick_target`'s `defines-data`
flag names the symbols but can't bind a vram. **Locate it by byte-search:**
`mips-linux-gnu-objdump -s -j .data build/.../<file>.o` for the compiled bytes, then search the
baserom for that exact pattern (`xxd baserom.z64 | grep '<hex words>'`) → the ROM offset is the carve
start. The link order does not place it immediately after the previous mirror's carve; other files'
`.data` interleave (S100 `reverb.c`'s 0x20 sat at `0xA3560`, 0x100 B past drvrnew's `0xA3460` tail).
Confirm the extent against the next baserom data symbol, then 3-way split as usual. (S100 `reverb.c`:
`L_INC[]`={0x10,0x10,0x20} + `val`=0.0 + `lastval`=-10.0 (`0xC1200000`) + `blob`=0, 0x20 at `0xA3560`.)
Tooling follow-up (S100 #1, deferred off-cadence ranker change): `pick_target.py` could tag
`defines-data:<g>;unref` when the flagged global has no `%lo` ref in the owning fn bodies, signalling
the gate to byte-search rather than asm-recover.

**Carve timing: execution, never the plan gate.** A `.data` (or any ld-section) sibling
carved from the C compile cannot land at the `/sprint-plan` gate: the gate validates the *stub*
state, where an `INCLUDE_ASM` `.text`-only object emits no `.data`, so the sibling carves an empty
range and shifts every following data byte → the gate green-ROM SHA check fails. The gate flips
**text only**; add the ld-section split during execution, **together with the verbatim body** (which
makes the `.o` emit the real `.data`). The stub's `%lo(D_<vram>)` resolves from the existing generic
`data` subseg in the meantime. (S68 `gu/align.c`: deferred the `[0xA35A0, .data, libultra/gu/align]`
carve from gate to the body step; the verbatim twin of S61's carve, first-build match.)

**Symbol-add vs. section-carve: the gate-safe drop-def sub-case.** The S68 "execution, never
the gate" rule is specifically about an **ld-section sibling carve** (a `[<rom>, .data, …]` /
`.bss` / `.rodata` yaml subseg), which carves an empty range against the `.text`-only stub and shifts
every following data byte → gate SHA break. It does **not** cover a plain `symbol_addrs.txt` data
entry that merely **names an existing auto `D_<vram>` label** (the global already lives in a generic
`data`/`bss` subseg; the add only renames the dlabel, emits no new section). That symbol-add is
**SHA-neutral at the stub stage** and so may be performed at the plan gate alongside the function
symbol, saving the mid-execution second `make extract` the recover-extern otherwise forces. The tell
that a drop-def is the gate-safe symbol-add kind (not a carve): the dropped globals' vrams are already
visible as `%lo(D_<vram>)` in the **scaffold `.s`** (so they resolve from an existing region, no new
section needed). When in doubt, the gate `make extract && make` green-ROM SHA is the proof: naming a
pre-existing `D_` label is inert, a section carve is not. (S81 `io/siacs.c`: the 3 SI globals
`__osSiAccessQueueEnabled`@0x800C8210 / `__osSiAccessQueue`@0x801EFFB0 / `siAccessBuf`@0x800FAA00 were
all visible as `D_<vram>` in the scaffold asm; placing them at the gate would have avoided the 2nd
extract. Contrast the gu carves S61/S68/S73, which inject real `.data` and must stay at execution.)

**Validate a `D_<vram>` rename with a clean rebuild, not an incremental `make`.** The symbol-add
is SHA-neutral, but `make` does **not** track the INCLUDE_ASM `.s` dependency of the stub object. After
the gate `make extract` renames `D_<vram>`→`<name>` in the scaffold `.s`, an *incremental* `make` does
not recompile the stub `.o` (its `.c` is unchanged) → the stale object still references the now-removed
`D_<vram>` auto-symbol → `undefined reference to D_<vram>` link failure, masked by a stale-`.z64`
false-positive SHA. Run `make clean && make extract && make` (or `rm` the affected stub `.o`) to
validate the rename. This holds whether the rename happens at the gate or mid-execution, so the gate
pre-place's only real saving over deferring is one fewer `make extract`; both still need the clean
rebuild. (S82 `io/controller.c`: gate-placing `__osEepromTimerMsg`@0x8012F4DC et al. failed the
incremental link with `undefined reference to D_8012F4DC`; `make clean` produced the green ROM.)

**Source-side detector: the coddog-band backstop.** The S52 `data-static`/`rodata-literal`
pre-flag is **asm-side** (it classifies the fn's `lwc1/ldc1 %lo(D_<addr>)`), and it does **not** fire
on an un-named **coddog** candidate: `gu/position.c` (`func_800A9C60`, a 99.99 coddog mirror) ranked
a clean pts-3 with *no* data-static/defines-data flag, hiding its `dtor` carve, even though guPositionF
loads `%lo(D_800C81B0)`. So `pick_target.py` now also reads the resolved **upstream source**:
`defines_local_static_data()` greps function-body `static <type> <name> = <init>;` and merges the
names into the `defines-data:<name>` hazard on **both** the named-upstream and the coddog re-scan paths
(the S72 coddog trap re-scan ran only `defines_data_globals`, which skips `static` and scans only
brace-depth 0, doubly blind to a fn-local static). This is the source-side backstop the coddog band
needed. Sizing is unchanged from S61: a single `static float` → a **16B** `.data` carve (4B + 12B pad).
(S73 `gu/position.c`: 16B carve `[0xA35B0, .data, libultra/gu/position]`→`[0xA35C0, data]` for
random's `xseed` remainder; align/rotate are the identical 16B precedents; first-build SHA match.)

**Provenance:** carve-vs-drop decision: S96 (drvrnew dual `.data`+`.rodata` carve), S97 (shared ⇒
drop, sl.c `alGlobals`), S116 (field-shared ⇒ carve + canonical addend, nucontgbpakmgr), S142
(fn-local ⇒ sole-referrer carve, aud_samples); carve extent/sizing: S117 (`.o` `.data` section size,
nucontmgr), S61 (compiled-object extent, hoist-vs-carve, guRotateF); unref localization: S100 (ROM
byte-search, reverb.c), S138 (localize-by-value + `data-static` tooling follow-up, n_reverb);
pre-flags: S104 (`data-carve` init-static array), S52 (`data-static` FP, dtor), S73 (coddog
source-side backstop, gu/position); drop-def fast paths: S42 (verbatim-body drop-def, osSetEventMesg),
S45 (shared-data-blob fn-local static, osGbpakReadId); carve timing: S68 (execution not gate,
gu/align), S81 (gate-safe symbol-add, siacs), S82 (clean-rebuild validate, controller).

---

## needs-header

**Rule:** Include-resolvability is a pick/DoR hazard. Two cases, distinguished at the gate.

**Trigger:** `pick_target.py` flag `needs-header:<inc>` (greps every upstream `#include` against the
project `-I` set: `include/{,libultra,libultra/internal,libkmc,libnusys}`; the grep is `#ifdef`-blind
so a dead-`_DEBUG` include can over-flag, so confirm against the upstream).

**Sub-cases / variants:**

**Vendorable annotation.** `pick_target` tags each missing header `<inc>(vendorable)` when it
is copyable from upstream: a public companion under an `UPSTREAM_INC_ROOTS` include dir (copy into
an `-I` dir) or a source-private header found by basename under an `UPSTREAM_SRC_ROOTS` source tree
(`ultralib/src/**`, copy source-relative next to the mirror, e.g. `xstdio.h`, `guint.h`). A vendorable
header is a one-time header-vendor *enabler* (pts `needs-copy` +1), **not** a `blk` DoR reject; only
a *bare* (untagged) header sets `blocked` → `blk`: one present in-tree but unreachable (deferred `-I`),
or absent everywhere (a system header). This closed the recurring "blk that's actually a 1pt cp"
gate-time re-diagnosis (S49 guint.h, S53 PR-band, S54 sprintf's `xstdio.h`/`string.h`).

**Procedure:**
- **Missing-but-copyable / `(vendorable)`** → copy the companion header into the tree (a public one
  into its `-I` dir like `assert.h` for `visetmode.c`; a source-private one source-relative next to
  the mirror like `xstdio.h` → `src/libultra/libc/`).
- **Band-local quote-include (already resolved)** → a `#include "x.h"` resolves source-relative to
  the dir the mirror compiles in, so once the first band sibling has shipped its companion at
  `src/lib<...>/<band>/x.h` (e.g. `gu/guint.h`, copied alongside `gu/random.c`), every
  later sibling that quote-includes it is already clean, no enabler. Since `missing_includes`
  takes the mirror dir and drops these, so they no longer over-flag as `needs-header`/`blk`. This
  is the include-side of `#open-band-fast-path`; the over-flag only persists for an angle-include
  (`<x.h>`), which is never source-relative.
- **Unindexed `-I` path** → a deferred Makefile enabler; defer the pick unless the PO adds the path.
  The audio band's bare `#include <libaudio.h>` (header sits at `include/libultra/PR/libaudio.h`,
  but there is no `-I include/libultra/PR`) is the standing example: flagging an audio leaf as a
  clean flip without that path is a false-clean. Prefer `<PR/x.h>`-style `monegi/` upstreams.

**`.inc.c` body-include vendoring (n_audio_sc).** Some upstreams `#include` a `.inc.c`
**fragment inside a function body** (not a header): the n_audio_sc N_MICRO command-stream files do
`#include "inc/<x>.inc.c"` in the `#else` of an `#ifdef N_MICRO`, where the fragment is a few bare
statements (`n_aInterleave(ptr++); n_aSaveBuffer(...);`), not a TU. Three rules:
- **Vendorable, not `blk`.** The fragment lives in the upstream `src/inc/` tree, so it is a +1
  `needs-copy` enabler like any source-private header, copied source-relative next to the mirror, but
  **renamed `.inc.c` → `.inc` locally** (upstream `inc/n_save_add01.inc.c` → `src/libnaudio/inc/`
  `n_save_add01.inc`). `include_is_vendorable` matches the upstream `#include "inc/<x>.inc.c"` by its
  full source-relative path under `UPSTREAM_SRC_ROOTS` (the n_audio_sc `src/` is registered there), so
  it no longer false-flags `blk`. Before that the detector scanned only `.h` basenames and missed the
  whole inc set; that masked it as a DoR reject when it was a 1pt cp.
- **Not a standalone TU.** The build's source discovery is `find $(SRC_DIR) -name '*.c'`, so vendor
  the local copy with the **`.inc` extension** (not `.c`/`.inc.c`): `find -name '*.c'` then never
  sweeps it in as a standalone object (it is a few bare statements, `parse error before '++'` if
  compiled alone). Update the host `.c`'s `#include` to the `.inc` name to match. The `.inc` extension
  also keeps clang-tidy's `bugprone-suspicious-include` quiet — a `.c` include trips it. (The `.inc`
  rename (2026-06-29) retired the prior `.inc.c` local suffix and the Makefile `! -name '*.inc.c'`
  exclusion.)
- **Do not clang-format the fragment.** It is a body-include with function-body indentation; running
  `clang-format-22` on it standalone reindents to column 0 (it parses as a TU). Keep it verbatim;
  codegen is unaffected (preprocessor text-inclusion, whitespace-irrelevant). Format only the host
  `.c`.

These files also branch on `N_MICRO`, an n_audio_sc-wide define: see `#needs-define` (the
`-DN_MICRO=1` `LIBNAUDIO_CFLAGS` pin), the same class as the `-DF3DEX_GBI_2` libultra pin.

**Provenance:** vendorable annotation: S54 (the `(vendorable)` tag + `needs-copy` pts, retiring the
"blk that's a 1pt cp" re-diagnosis; sprintf `xstdio.h`/`string.h`); band precedents: S49 (`guint.h`),
S53 (PR-band); band-local quote-include drop: S50 (`missing_includes` takes the mirror dir); `.inc.c`
body-include vendoring: S133 (n_audio_sc).

---

## per-library standard-C-header isolation

**Rule:** libultra and libkmc each ship their own standard C headers and they are not interchangeable
(libultra's `stdlib.h` defines `lldiv_t`; libkmc's lacks it). The `-I` order puts `include/libultra`
before `include/libkmc`, but neither holds the std headers directly, so a bare `#include "stdlib.h"`
from any source falls through to `include/libkmc/stdlib.h`. A verbatim libultra mirror needing a
libultra-only type therefore fails against the libkmc header. **This is a pick/DoR blind spot:**
`needs-header` greps resolvability, not which library's header resolves nor whether it has the needed
types: a resolvable-but-wrong-library include passes the grep and the gate build-check (the
`INCLUDE_ASM` scaffold never compiles the C body), surfacing only mid-execution (like `calls-unplaced`).

**Trigger:** Mirroring a libultra leaf that includes a bare standard header (`stdlib.h`, …).

**Procedure:** Fix in the build system, never by adding the type to the libkmc header (that pollutes
a verbatim libkmc mirror). Vendor the libultra std header verbatim from
`ultralib/include/compiler/gcc/` (KMC GCC 2.7.2 → the `gcc` variant) to
`include/libultra/compiler/gcc/`, and **prepend** `-I include/libultra/compiler/gcc` to
`LIBULTRA_CFLAGS` only. Verify per-file with `tools/cc/gcc -M -nostdinc <profile -I set> src/…`: the
libultra source must resolve to `include/libultra/compiler/gcc/stdlib.h`, the libkmc source still to
`include/libkmc/stdlib.h`.

**Provenance:** S40 (`ldiv.c` → `lldiv` → `lldiv_t`).

---

## Compile profiles (libkmc -O, libultra -O3)

**Rule:** The two libs were built with different `-O` levels; the scheduler differs, so the same C +
same compiler at the wrong level produces a byte mismatch.

- **libkmc = `-O`** (per `libkmc/src/genn64.bat`), not `-O2`. At `-O` rand.c stores `next` between
  the two `addiu` ops; at `-O2` it moves to the end.
  - **CORDIC `double↔long long` cvt helper.** The libkmc math mirrors (`atan.c`, `sin.c`)
    convert `XLONG = (double)expr * MBIT` where the source double can be negative, yet KMC GCC at `-O`
    emits **`__fixunsdfdi`** (the unsigned `double→u64` helper, @0x800B3C20), never `__fixdfdi`. So a
    libkmc CORDIC C-mirror needs only `__fixunsdfdi` + `__floatdidf` (@0x800B3D40) placed; there is no
    signed-cvt-helper enabler to recover. Confirmed identical in both atan.c and sin.c (verify at the
    gate by reading the subseg's jal list, never by assuming `__fixdfdi` from the C signedness). The
    long-long shifts (`x>>i`) are inlined (no `__ashrdi3`/shift-helper jal).
- **libultra = `-O3 -fsigned-char`** with `MIPS_VERSION=-mips3` for VERSION_J (ultralib gcc.mk,
  but see the char-signedness note). Global CFLAGS uses `-mips3` (changed from `-mips2`).
  `-O3` enables inlining of small same-TU functions and affects delay-slot scheduling.
  **Char signedness is `-fsigned-char`, not ultralib-J's `-funsigned-char`.** ultralib's
  `gcc.mk` adds `-funsigned-char` for VERSION_J, but this ROM's libultra was built signed: a full
  `make clean` rebuild under `-fsigned-char` reproduced the baserom SHA-1 exactly (every
  current libultra C file matches signed). See `#char-signedness` for the symptom + the per-file
  override mechanism if a future TU ever needs the opposite.

**Trigger:** A libkmc match locking at ~0.9 across every C rewrite (suspect scheduler-level); a
libultra match locking at ~0.9 with unexpected register usage or wrong delay-slot instructions.

**Procedure:** `mk/libkmc.mk` and `mk/libultra.mk` carve `LIBKMC_CFLAGS := $(subst -O2,-O,$(CFLAGS))` and
`LIBULTRA_CFLAGS := $(subst -O2,-O3,$(CFLAGS)) -fsigned-char -DBUILD_VERSION=VERSION_J`, each via a
specificity-winning pattern rule. `decomp_loop.py` auto-applies the right profile when a matching
`src/libkmc/<basename>.c` or `src/libultra/<relpath>.c` exists. If a match locks at ~0.9, verify the
loop is using the correct profile (compile `base.c` with `-O` directly, or pass `LIBULTRA=1` /
`--profile libultra`) before iterating further on the C.

**Provenance:** S33 (`-mips3`); S112/S113 (libkmc CORDIC `__fixunsdfdi` helper); S65 (`-fsigned-char`
band default).

---

## char-signedness (libultra is -fsigned-char)

**Rule:** This ROM's libultra was compiled with **signed `char`** (`-fsigned-char`), a ROM-proven
deviation from ultralib's documented VERSION_J profile (gcc.mk adds `-funsigned-char` for D–J). The
band default `LIBULTRA_CFLAGS` therefore carries `-fsigned-char`. Most libultra C is
char-signedness-*ambiguous* (matches under either flag), so this only ever surfaces on a file whose
codegen actually depends on `char` signedness.

**Symptom (the discriminator):** a verbatim C mirror SHA-misses, and the only diff is byte-load
signedness: the ROM loads `lb` + `sll/sra` sign-extend (and may emit a phantom empty stack frame),
while the wrong flag gives `lbu` + `andi 0xff`. The exposing pattern is a `char` lvalue compared
against a **non-zero** value (e.g. `strchr`'s `*s != ch`); a comparison against literal `0` is
signedness-invariant (`strlen`/`memcpy` matched under both: `memcpy` fully, `strlen`'s `lb` is the
compiler's free choice for a `!= 0` test).

**Procedure:**
1. The band default is already `-fsigned-char`, so a new libultra mirror compiles signed with no
   action. If a mirror SHA-misses with the `lbu/andi` vs `lb/sll-sra` signature, you have the inverse
   case (a TU that wants the *opposite* signedness).
2. Per-file override mechanism (add in `mk/libultra.mk`; the explicit-target var beats the `libultra/%.o` pattern):
   `$(BUILD_DIR)/$(SRC_DIR)/libultra/<rel>.o: C_PROFILE_CFLAGS = $(subst -fsigned-char,-funsigned-char,$(LIBULTRA_CFLAGS))`
   (or the reverse subst). Append-and-last-wins also works since gcc takes the final `-f*-char`.
3. To re-settle the band default after several such files, run `make clean && make` with the global
   flag flipped and compare the ROM SHA-1: the authoritative test (a clean rebuild under
   `-fsigned-char` reproduced the baserom exactly, proving the global default, not a per-file patch).

**Distinct from** the per-field **cast-divergence** hazard (a 2.0L-vs-J `(type)` widening on one
struct field's high word): that is a source/version difference; this is a whole-TU compiler flag.

**Provenance:** S65 (string.c `strchr`/`strlen` exposed it; the per-file override was the initial
fix, then the clean-rebuild test proved `-fsigned-char` is the correct band default and the override
was removed).

**Related — per-declaration type-choice levers for classical game (-O2) code (S208).** Distinct from
the whole-TU flag above: in hand-authored classical C the DECLARED type of a byte global or a fn return
steers codegen, and you pick the type by matching the ROM's instruction, not by semantics.
- **Store-const materialization.** Storing `-1` to a byte global emits `li vN,-1` (`addiu vN,$0,-1`) if
  the global is `extern s8`, but `li vN,0xff` (`ori`) if `extern u8` — the stored byte is identical
  (`sb` low 8 bits) but the immediate-load instruction differs. Match the ROM's `li -1`/`li 0xff` to
  pick `s8`/`u8`. S208 `func_8005DC50` needed `s8 D_800C1FF4-7` for `= -1` → `li v0,-1`.
- **Return-type re-extension.** A callee declared to return `s8` makes the CALLER re-sign-extend the
  result (`sll/sra …,0x18`) before use; declaring the callee `s32` drops the re-extension, and the
  callee's own body is byte-identical (its `lb` already sign-extends the byte into the full register).
  When the ROM caller has no `sll/sra` on the returned byte, type the callee `s32`. S208 `func_8005D274`
  matched only after `func_8005D23C`'s return was changed `s8`→`s32`.

---

## assert-strip (bare upstream assert vs NDEBUG)

**Rule:** This build defines **neither `NDEBUG` nor `_DEBUG`** (only `_FINALROM`). The in-tree
`assert.h` keys solely off `NDEBUG`: undefined → `assert(EX)` expands to
`((EX)?(void)0:__assert(...))`, which **emits a branch + `jal __assert`** (plus the `__FILE__` /
line / stringized-expr args). The ROM's libultra was a release build with asserts stripped, so a
verbatim mirror of a file whose upstream calls `assert()` **outside** any `#ifdef _DEBUG` compiles in
assert code the ROM does not have → SHA-miss.

**Symptom:** a clean verbatim io/cont/pfs/vi/si mirror SHA-misses (or the isolated `.text` is
*larger* than its ROM slot) and the extra bytes are a `beq/bne` + `jal __assert`. The tell: the
function is the *same size* as a sibling that has no assert (S72: epirawread's 0x170 subseg == its
twin epirawwrite's 0x170, so the ROM carries no assert code for the bare `assert(data != NULL)`).

**Procedure:**
1. Wrap the bare upstream assert(s) in `#ifdef _DEBUG`, the in-tree, banked convention. `sirawread.c`
   does exactly this: upstream has bare `assert((devAddr & 0x3) == 0); assert(data != NULL);`, the
   banked mirror guards them in `#ifdef _DEBUG`. With `_DEBUG` undefined the assert tokens are never
   compiled → zero code → matches the release ROM. Fold a bare assert adjacent to an existing
   `#ifdef _DEBUG` block (the `devAddr & 0x3` __osError class) into that same block.
2. The proof stays the full-make ROM SHA-1. Do not instead define `NDEBUG` globally; it would also
   silence asserts the ROM might genuinely retain elsewhere, and the band has not been rebuilt under
   it; the `#ifdef _DEBUG` wrap is the surgical, per-call strip.
3. `assert.h` need not be included once every assert is `_DEBUG`-wrapped (the token never reaches the
   macro phase), but mirroring the upstream include is harmless.
4. **An assert that is the sole body of an `if` cannot be wrapped alone:** `#ifdef _DEBUG`-wrapping
   just the assert leaves the `if (...)` bodyless under non-`_DEBUG` → a syntax error. Wrap the whole
   `if`+assert in the `#ifdef _DEBUG` block (S106 sched.c:575 `if (avail & OS_SC_DP == 0) assert(...)`,
   an always-false dead branch: the `&`-vs-`==` precedence makes the guard `avail & 0`, so the whole
   thing is zero code in the release ROM either way; wrapping it is SHA-neutral). Same for an assert
   that is the body of any other bodyless control statement.
5. **Count every assert with `assert\s*\(` (allow whitespace before the paren).** `pick_target`'s
   `bare_asserts` already does (`re.findall(r"\bassert\s*\(", ...)`), so its `bare-assert:N` count is
   authoritative; the agent's manual strip grep must match it. S106 sched.c had 9 bare asserts but a
   manual `^\s*assert\(` grep found only 7: it missed `assert (t->msgQ)` and `assert ( (type == …))`
   (a space before the paren), so 2 asserts compiled in on the first build (`.text` +0x80 `jal
   __assert`, `.rodata` +0x50 stringized-expr/`__FILE__`) → SHA-miss, fixed by wrapping the 2. Always
   reconcile the manual count against pick's `bare-assert:N` before declaring the strip complete.

**Banked instances:** sirawread.c, sirawwrite.c, visetmode.c, viswapbuf.c, visetevent.c (vi/si bare
asserts), epirawread.c (S72, pi EPI band; `assert(data != NULL)` sat *outside* the `_DEBUG` block),
sched.c (S106, 9 bare asserts, the heaviest assert-strip mirror; 2 were `assert (` space-variants +
1 was an `if`-body, steps 4-5).

**Provenance:** S72 (epirawread.c; the read-twin's bare assert would have emitted code, caught by the
read==write subseg-size tell + the banked sirawread convention). A `bare-assert` advisory
`pick_target` flag (scan a mirror's upstream for a non-`_DEBUG`-guarded `assert(`) is a noted
deferred enhancement; see `BACKLOG.md ## Carry-overs`.

---

## .rodata sibling-yaml pattern

**Rule:** When GCC emits a `.rodata` constant for a C file whose text is at one ROM offset but whose
`.rodata` must land at a different ROM offset, add a dot-prefixed `.rodata` subseg at the correct
ROM offset with the same name as the `c` subseg. Splat matches siblings by name; the dot-prefix
subseg's `out_path()` returns the compiled C object, placing `build/src/xxx.o(.rodata)` at the
correct VRAM. `should_self_split()` = False for dot-prefix types (no asm file extracted), and
`auto_link_sections` won't insert a duplicate. When the same mirror also defines file-scope `static`
initialized arrays, this `.rodata` carve pairs with a `.data` carve; see the
[dual-section carve note](#defines-data).

**Trigger:** A libultra/libkmc/libnaudio file with a compiler-generated rodata constant at a different
ROM offset than its text (e.g. a `2^32` double, or the `MAX_RATIO` 1.99996 double a resampler clamps
against). `pick_target.py` pre-flags this as `rodata-literal:0x<vram>[,0x<vram>…]` on a mirror
candidate whose asm loads an anonymous pooled FP constant (`ldc1/lwc1 %lo(D_<addr>)`), so the sibling
split is a known DoR enabler, not a finalize surprise. The scan runs on both the named-upstream path
(`append_upstream_hazards`) and the coddog/audio path (`_append_coddog_trap_hazards`): originally the
literal scan was named-path-only, so an n_audio_sc coddog mirror's FP-pool double (S134
`n_resample` `MAX_RATIO` @0x800D2190) was an unflagged first-build SHA-miss while the rodata-jtbl
analog already rode the shared recover battery. The address is classified by segment first
(data-segment statics are `data-static`, not `rodata-literal`; see #defines-data). The pre-flag lists the full extent: GCC moves a
pooled `double` into an FP register pair via an `lw` pair + `mtc1` (not `ldc1`), so a literal's 2nd
(and further) word is an integer `lw %lo(D_<addr>)` invisible to the FP-only scan. pick_target adds
the rodata-band `lw` refs so the split width is sized correctly (S52 `guLookAtReflectF`: `-1.0`
@0x800D2510 via `ldc1` + `1.0` @0x800D2518 via an `lw` pair = a 16-byte block, not 8). The pre-flag
also appends `;carve-end=0x<vram>`: a multi-`du` dlabel block's trailing word has no `%lo`
of its own, so the scan's max referenced literal can understate the carve end (S64 `lookathil`: the
`.double 0` @0x800D2508 sat inside the `D_800D2500` block, so the scan's max 0x800D2500 understated
the 0x800D2510 carve end = the `lookatref` `.rodata` boundary). `carve-end` is the next `.rodata`
subseg boundary, an upper bound that is exact when the mirror's literals are the subseg tail (the common
case); the finalize carve is still `.o`-sized, but the gate now sees the planned extent.

**Sub-cases / variants:**

**`twin-of:<file>` (sibling-carve hint).** When a mirror candidate sits in a directory that already
holds a banked sibling with a proven `.data`/`.rodata` ld-section carve, `pick_target.py` flags
`twin-of:<file>` naming that sibling, so the matching carve is expected here and priced at the gate.
Routes to this section or #defines-data per which section the sibling carved.
**Generic-subseg-bound carve = exact extent, no split.** When a carve start or end coincides
with an existing generic `[off, (ro)data]` subseg boundary, that generic subseg's opposite boundary
is the exact carve extent: the linker already split the section there, so the carve is a 1-line
attribute flip of the existing generic subseg (`[off, data]` → `[off, .data, path]`), not a split,
and `carve-end`'s upper bound is exact (the S93 xldtob single-line case, generalized). S101 `env.c`:
both sections were whole-subseg flips. `.data` `eqpower[128]` = the entire generic `[0xA3460, data]`
(0x100 B, vram 0x800C8060); `.rodata` jtbl+13 literals = the entire generic `[0xAD6F0, rodata]`
(0xF0 B, vram 0x800D22F0). pick's `carve-end=0x800D25C0` over-stated (real end 0x800D23E0 = the next
named `.rodata` boundary 0xAD7E0). After the env|filter split, env is a single-file-pack and the sole
owner, so the `;owner-per-member` marker is moot.
**Carve-start widening: the reported min can understate the carve start.** The FP/`lw` scans
see only scalar `%lo` loads; a `.rodata` block that opens with a file-scope `static const <type>
name[]` array begins at the array base (an `addiu %lo` address-of) and may carry string literals
("NaN"/"Inf") ahead of the scalar pool, both invisible to the scans, so `min(rodata_lits)` is too
high (xldtob.c: the FP scan's min 0x800D2820 sat 0x50 B inside the block, which actually starts at
the `pows[]` dlabel 0x800D27D0). `pick_target.py` widens the carve-start to the rodata subseg
boundary (symmetric to `carve-end`), but only when the upstream defines a file-scope `static const`
array (`defines_file_static_const_array`): a `const` static is file-private rodata, so the whole
code-segment rodata subseg is this object's own, which makes the widening FP-safe. (The direct
`addiu %lo` band scan that would find the base without the source gate was reverted: in the
`.data` band it could not tell a file's own static from a shared cross-file extern; the `static
const` source gate confines this to rodata where that ambiguity does not arise.)

**Carve-start past a foreign leading symbol; the `.o` section size is the extent oracle.** A
generic `[off, rodata]` subseg can begin with a symbol owned by a different, already-banked file;
then the carve must start at the target's first symbol, not the subseg boundary (so the carve-start
widening above does not apply: the leading bytes are not this object's). S104 `xprintf`: the generic
`[0xADA40, rodata]` led with `__libm_qnan_f`@0xADA40 (cosf/sinf's NaN const, referenced via its
symbol), so xprintf's carve started 0x10 later at fchar `[0xADA50, .rodata, libultra/libc/xprintf]`
(0x178 B, bounded by xldtob's `[0xADBD0]`), leaving `__libm_qnan_f` in a 0x10 B generic remnant. The
authoritative extent for both the `.data` and `.rodata` carve is the compiled object's section
size: `mips-linux-gnu-objdump -h build/src/<path>.o` (xprintf.o: `.data` 0x50 = spaces[33]+pad +
zeroes[33]+pad, `.rodata` 0x178 = fchar+fbit+"hlL"+jtbl). Read it once the body compiles (even at a
link-failing build) to size each carve exactly, rather than inferring from the asm `%lo` span.

**Named vs anonymous pool: no difference; the named-rodata caveat is retired.** A pooled
constant block whose words are named in `ghidra_symbols.txt` (e.g. cosf's
`kCoeff4`/`kCoeff3`/…/`kInvPi`/`kPiHi`/`kPiLo`/`kZero`/`zOneHalf1`/`kOneHalf2` at 0x800D2460..0x24B8)
carves exactly like an anonymous `D_<addr>` pool. Splat extracts the dot-prefixed `.rodata`
subseg cleanly even though those addresses carry user data labels: the labels become harmless
absolute symbols, the bytes come from the compiled `.o`, and `--allow-multiple-definition` tolerates
the overlap. The S64 `cosf` carry-over's "named-rodata may collide at the `.rodata` carve" warning was
a phantom, verified benign. The one real step a named pool adds is a confidence check:
byte-compare the compiled `.o(.rodata)` against the ROM bytes at the target offset first (a
verbatim mirror's pool must equal upstream; if it doesn't, the source/version is wrong, discard the
mirror), then carve as usual.

**Switch jump table: same carve, automatic byte-match.** A `switch` with a contiguous-ish case
range compiles to a `jtbl_<addr>` in the code-segment `.rodata`, a block of `.word .L<addr>` entries
that are the function's own internal labels (the case-body targets), reached by an indexed
`lui %hi(jtbl_…)` / `lw %lo(jtbl_…)` / `jr`. The carve is identical to a literal pool: split the
autogen asm rodata at the `jtbl_<addr>` offset and insert `[0x<JTBL_ROM>, .rodata, path/to/file]`
(dot-prefixed, same name as the `c` subseg). Two properties make this the easiest rodata sibling:
(1) the byte-match is automatic for a verbatim mirror, since the C compiler re-emits the same table
because the case-body absolute addresses are identical when the text is verbatim (no
`.o(.rodata)`-vs-ROM confidence check needed, unlike a named FP pool); (2) the trailing
alignment-pad word (S76 `__osDevMgrMain`: 7 case `.word`s + 1 zero pad = 0x20, but GCC's `.o(.rodata)`
emits only 0x1C) reproduces from the linker's subseg-boundary zero-fill: carve the full extent up
to the next `.rodata` subseg boundary (`0x<JTBL_ROM>`..next-subseg) and the hole zero-fills to match.
**Why this is a finalize surprise the gate cannot catch (the reason for the pre-flag):** while the
subseg is an `INCLUDE_ASM` stub, the switch jtbl is still valid asm referencing live `.L<addr>`
labels, so `make extract && make` is green at the gate; the break only appears at first execution-time
make, when the C body deletes those labels and the still-asm `jtbl_<addr>` (in a neighbour rodata `.o`)
link-fails with `undefined reference to .L<addr>`. `pick_target.py` pre-flags this as
`rodata-jtbl:0x<JTBL_VRAM>` (the jump-table analog of `rodata-literal`, scanned in the shared
recover-battery so it prices both named-upstream and coddog candidates) so the carve is a known DoR
enabler, not a first-build link error.

**c-combined pack: the carve owner is per-member, not the primary.** The rodata-literal/jtbl
scans span the whole subseg (one `.c` → one `.o` → one `.rodata`, so a sibling fn's pooled
literals belong to the same carve). That is correct for a single-file pack, but a c-combined
pack (≥2 distinct upstream `.c` in one asm subseg) lumps both members' rodata onto the primary row,
yet the carve owner is the member file whose function actually references each literal/jtbl. S98
`[0x811A0]` `c-combined:2file[mainbus|resample]`: alMainBusPull's row carried resample's
`rodata-literal:0x800D23E0` (MAX_RATIO double, alResamplePull's `ldc1 0x23e0`) +
`rodata-jtbl:0x800D23E8` (alResampleParam's switch), but mainbus.c is carve-free (alMainBusPull
references no 0x800D2 rodata, all immediates). `pick_target.py` now suffixes the c-combined rodata
hazards with `;owner-per-member` so the gate does not carve the primary `.c` by default. **Confirm
the true owner at execution by the pre-carve build's undefined-`.L<addr>` link-error**: copy each
member verbatim and `make`; the link-fail names the orphaned jtbl `.word .L<addr>` entries
(`AD6F0.rodata.o:(.rodata+0x114): undefined reference to .L800A6188`), and those `.L<addr>` labels
fall inside the owning function, so the carve belongs to that member's file. Two precision caveats on
a c-combined pack: (a) `carve-end` is doubly loose, since it can run past an intervening sibling's rodata
(S98 env's `carve-end=0x800D2410` swallows resample's already-carved 0x800D23E0..0x2410), so size the
real carve from the owning member's `.o(.rodata)`; (b) re-running `pick_target` after a split can
mis-attribute via a stale `asm/<ROM>.s`: splat leaves the old per-ROM listing when a subseg
flips to `c`, so the primary's scan still sees the now-separate sibling's instructions. Full
per-member attribution (scan each member fn's body for the ref + tag the owning stem, incl. the
coddog jtbl path) is a BACKLOG tooling follow-up; the `;owner-per-member` marker + this link-error
confirm step cover the gate today.

**Procedure:** Split the adjacent autogenerated asm rodata subseg so the new `.rodata` subseg covers
only the correct bytes (the full flagged extent, rounded to the literal block), then insert
`[0x<RODATA_ROM>, .rodata, path/to/file]` (dot-prefixed, same name as the `c` subseg). **First check
whether splat already bounded the carve: no split when the generic subseg's extent already
matches.** Splat's per-TU auto-segmentation often already brackets the mirror's rodata as its own
generic `[0x<ROM>, rodata]` subseg; compare its `[start, next-start)` extent to the `.o(.rodata)`
size before computing a split point. When they match (xldtob.c: `[0xADBD0, rodata]` already spanned
0xADBD0→0xADC40 = 0x70, exactly the `.o(.rodata)`), the carve is a 1-line attribute change
(`rodata` → `.rodata, path/to/file`), not a split. This saves the split arithmetic and is the common case
for cleanly-segmented libc/gu mirrors. **Timing: do this during execution with the body, not at the
plan gate.** Like the `.data` carve, a `.rodata`
sibling added against an `INCLUDE_ASM` stub carves an empty range (the stub emits no `.rodata`) and
reflows the rodata segment, failing the gate green-ROM check (see `#defines-data` "Carve timing").
The gate flips text only.

**Provenance:** S38 (`osAiSetFrequency`: split `[0xAD5E0, rodata]` at 0xAD6A0; inserted
`[0xAD6A0, .rodata, libultra/monegi/ai/aisetfreq]` = 8-byte double + 8-byte pad at 0x800D22A0).
S48 (`__osViSwapContext`: same `2^32` double; split `[0xAD9C0, rodata]` at 0xAD9E0; inserted
`[0xAD9E0, .rodata, libultra/io/viswapcontext]`; this sprint added the `rodata-literal` pre-flag).
S55 (whole-subseg rodata scan: one `.c` → one `.o` → one `.rodata`, so a sibling fn's pooled literals
share the carve).
S66 (`sinf`: anonymous pool, split `[0xAD960, rodata]` → `[0xAD960, .rodata, libultra/gu/sinf]`,
exact `.o`-size 0x60 fit; `cosf`: named pool, split `[0xAD6F0, rodata]` at 0xAD860 → inserted
`[0xAD860, .rodata, libultra/gu/cosf]`, retiring the named-rodata collision caveat as a phantom).
S68 (`.rodata` carve timing: the gate flips text only, so carve the sibling with the body, not against
an `INCLUDE_ASM` stub).
S76 (`__osDevMgrMain`: first switch jump table sibling, `switch (mb->hdr.type)` → `jtbl_800D2280`;
split `[0xAD5E0, rodata]` at 0xAD680 → inserted `[0xAD680, .rodata, libultra/io/devmgr]` = 7 case
`.word`s + 1 zero pad to 0x20; byte-matched first build with no confidence check; added the
`rodata-jtbl` pre-flag).
S92 (reverted the direct `addiu %lo` band scan; the `defines_file_static_const_array` source gate
replaced it, confining carve-start widening to rodata).
S93 (`xldtob.c`: `static const ldouble pows[]` + "NaN"/"Inf"/"0" strings + 1.0/1e8 literals = 0x70;
the generic `[0xADBD0, rodata]` subseg already bounded the exact extent → 1-line attribute change
`rodata` → `.rodata, libultra/libc/xldtob`, no split; added the carve-start widening pre-flag, where
`defines_file_static_const_array` source-gates the rodata-subseg-start carve-start, since the FP scan
missed the `pows[]` base 0x800D27D0 by 0x50 B).
S96 (`drvrnew.c`: the dual-section carve pair, jtbl+consts `.rodata` here + `*_PARAMS[]` `.data`
there).
S98 (`[0x811A0]` `c-combined:2file[mainbus|resample]`: split text at 0x81310, then resample's `.rodata`
carve split `[0xAD6F0, rodata]` 3-way → inserted `[0xAD7E0, .rodata, libultra/audio/resample]` = the
MAX_RATIO double `D_800D23E0` (8B) + `jtbl_800D23E8` (10 words, 40B) = 0x30; mainbus.c carve-free.
First owner-per-member case; added the `;owner-per-member` c-combined marker after the
whole-pack scan over-attributed resample's rodata to the mainbus primary row).
S112 (`atan.c`: first libkmc C-mirror; generic `[0xADC40, rodata]` (`cordic_atan_divisor_2_60`
+ 10 pooled FP literals + the `"atan2"` string, 0x60 B) already bounded the extent → 1-line
attribute flip `rodata` → `.rodata, libkmc/atan`, no split; byte-matched first build).

**Stale orphan after a retype-carve: expected, harmless.** Flipping a generic `[ADDR, rodata]`
→ `[ADDR, .rodata, <file>]` leaves the pre-carve `asm/data/<ADDR>.rodata.s` (and, on an incremental
build, a stale `build/asm/data/<ADDR>.rodata.o`) on disk: `make extract` does not prune a removed
subseg's per-file output, and `make clean` wipes `build/` but not `asm/`. Both are gitignored and
absent from `mariogolf64.ld` (unlinked), so a clean-rebuild ROM SHA-1 == baserom is proof they're
inert. Do not mistake the leftover `.s` at verify time for a double-carve (a double-link would
overlap and break the SHA, so a green clean-rebuild already rules it out).

**Interleaved-partial-TU carve: make each fn's data TU-OWNED in the right C form, one carve over the
whole `.o(.rodata)` extent.** When a partial-banked classical TU (a `src/<seg>.c` still holding
`INCLUDE_ASM` siblings) has MULTIPLE compiler-rodata fns whose rodata is INTERLEAVED with other data in
the ROM — e.g. `[jtblA @X][a 0x170 data table @Y][jtblB @Z]`, not the two jtbls adjacent — you cannot
carve each fn's rodata as its own dot-prefix subseg (splat places a `.c`'s whole `.o(.rodata)` at ONE
vram; two subsegs with the same `.c` name double-place it). Instead exploit that GCC emits the whole
TU's `.o(.rodata)` **grouped per-function in SOURCE order** (a fn's own const pool + its jump table,
before the next fn's), then carve the ONE `.o(.rodata)` block over its full extent. The trick is making
each interleaved item TU-OWNED as the C form that reproduces the ROM's layout AND its `.text`:
- **A compiler jump table** comes for free from writing the `switch` (see #switch-jtbl-dispatch).
- **A data table the fn indexes** must be **INDIVIDUAL `static const T name[N]` objects, NOT a 2D
  array and NOT `extern`.** A 2D array `t[R][C]` lets GCC fold a sibling row `t[k+1] = &t[k]+C` into a
  single `addiu base,+C` (1 insn short per reference vs the ROM's independent `%hi/%lo` per row);
  `extern` (data left in the blob) makes the `.o(.rodata)` emit ONLY the jump tables, which then pack
  contiguously (`[jtblA][jtblB]`, 0x60 apart) instead of the ROM's `[jtblA][table][jtblB]` spacing — a
  whole-ROM byte shift (the `.text` size is right, so the shift is a same-size-region MISPLACEMENT, not
  a `#short-text-shifts-flowing-bss` length bug: check the *rodata* symbol addresses, not the .text).
  Individual `static const` reproduces both the independent per-row `%hi/%lo` loads AND (emitted in
  declaration order between the two jump tables) the interleaved layout. Confirm with
  `objdump -s -j .rodata build/src/<path>.o`: the section must read `[jtblA][table0..N][jtblB]` at the
  ROM's sizes; then extend the single `[X, .rodata, <path>]` subseg to cover `X..(X+.o(.rodata)size)`.
  S185 `resolve_shot_quality_table` (jtbl_800CC530 + 23× `static const char q[16]` + jtbl_800CC700 =
  0x230 at 0xA7930; a 2D array was 10 insns short, `extern` was a 0x30 whole-ROM shift).

---

## IO_WRITE/IO_READ isolation artifact

**Rule:** Functions that access N64 MMIO via `IO_WRITE`/`IO_READ` always score ≠ 0 in isolation: the
macros expand to `*(vu32*)<literal_addr>`, baking in constants, while the reference asm object uses
symbolic relocations (`%hi(AI_DRAM_ADDR_REG)`) that assemble to placeholder zeros. asm-differ sees
mismatched `lui` immediates (up to 600-point penalty, `total_rows == match_count`,
`top_mismatches: []`). No C edit fixes this; it is isolation-only.

**Trigger:** Target writes/reads MMIO registers; score stuck at a flat penalty with empty
`top_mismatches`.

**Procedure:** Verify the C logic matches the asm instruction-by-instruction (it will, if the
upstream is correct), inline the body, run the in-tree byte-`cmp` spot-check, then `make` + ROM
SHA-1. Do not waste iterations.

**Provenance:** S34 (`osAiSetNextBuffer`, score=600 throughout, spot-check match).

---

## Assembler differences + byte-cmp spot-check

**Rule:** `src/%` is assembled by KMC `as` (`tools/cc/as`), not modern `mips-linux-gnu-as`. KMC `as`
emits `addu` for `move dst,zero` (encoding `0x…1021`, matching the original ROM) where modern `as`
emits `or` (`0x…1025`), and auto-pads `.text` to 16-byte section alignment. `asm/%` keeps modern `as`
(raw asm uses modern-only directives). Because objdump renders both `…1021` and `…1025` as
`move v0,zero`, mnemonic-level diff silently false-positives. **Spot-check by byte-level `cmp` of
raw `.text` only.**

**Never judge delay slots / instruction counts from GCC `-S` output — objdump the ASSEMBLED `.o`
(S177).** GCC emits in `.set reorder`/macro mode, where a branch's fall-through instruction sits
TEXTUALLY right under the branch (looking like a filled delay slot) and unfilled slots show only a
`#nop` COMMENT. The KMC assembler then fills the real slot — often with a `nop` it could not fill from
the fall-through (e.g. a load whose base/result conflicts with the branch's tested reg cannot be
hoisted into a non-annulling slot). So a `-S` read miscounts by ±1 and mis-attributes a "filled" slot.
S177 wrongly diagnosed `heap3_get_largest_free` as "1 word short, needs a synthetic no-op" from the
`-S` text — the clean inline-head source was already a byte-exact 21-word match once ASSEMBLED. Always
`mips-linux-gnu-objdump -d build/src/<...>.o` (or `tools/cc/as` then objdump), never the `.s`.

**objdump `-dz` is MANDATORY when word-diffing the assembled `.o`, and keep EVERY line (S183).** Plain
`objdump -d` COLLAPSES a run of identical zero words (`nop; nop`, or a zero-filled `.text`) to a single
`...` line, and a normalizer like `awk 'NF>2'` then silently DROPS that `...` line — so a byte-perfect
match with 2 real post-`mflo`/`mfhi` hazard `nop`s reads as a FALSE "2 nops missing" near-miss. S183
burned a multi-step `mflo`-hazard chase (KMC-`as` interlocks / GCC `#nop`-comment) on `func_80052264`
that was really an objdump-collapse artifact — the nops WERE present; the diff tooling hid them. Use
`mips-linux-gnu-objdump -dz` (disassemble zeroes, no `...` collapse) and word-diff EVERY line (do not
`awk 'NF>2'`-filter). The residual there was a single scratch register, not a missing instruction.

**Trigger:** Finalizing a classical match (the spot-check step). Mnemonic diff looks clean but you
need ground truth.

**Procedure:**
- `cmp` raw `.text` bytes of the in-tree compiled object vs the reference object.
- **Split-subseg target:** `decomp_loop.py` resolves the function to the old pre-split segment label
  and references the combined parent object. Use `build/asm/<subseg_hex_offset>.o` for the specific
  subseg (e.g. `build/asm/7E350.o`), not the combined parent (`build/asm/7E330.o`).
- **Struct-field reloc-addend artifact (classical libultra):** when a C file accesses struct fields
  via a base symbol, the compiled object encodes the struct offset as an inline LO16 addend
  (`sh v0, 0x32(at)`) while the reference uses a zero-placeholder + per-field label
  (`sh v0, 0x0(at)` + `R_MIPS_LO16 D_800C9532`). Both resolve identically after linking. When the
  only `cmp` differences are inline addend bytes on `sh`/`sw`/`lw` struct-field stores/loads and/or
  IO_WRITE literal addresses, proceed directly to `make` + ROM SHA-1 without C iteration.

`include/macro.inc` had its `.internal _MACRO_INC_GUARD` line removed so KMC `as` parses it cleanly.
`permuter_settings.toml` mirrors the build's `src/%` pipeline (`mk/src.mk`).

**Provenance:** S39 (struct-field reloc addend encoding).

---

## Multi-function-segment splitting (pack)

**Rule:** Splat often packs several functions, sometimes from different upstream files, into one
`[0x<rom>, asm]` subseg. When more than one upstream file lives in the subseg, split it before
flipping rather than flipping the whole block.

**Trigger:** `pick_target.py` flag `pack:<n>fn[fn=stem,...]` (different stems → multi-file pack).
Also `c-combined:<n>file[stem1|stem2|…]`: the C analog of `combined-subseg`, firing when ≥2
distinct C upstream files share one asm subseg, naming the split targets. It pre-prices the split
and surfaces a cheap clean leaf that a big combined subseg buries past smallest-first (e.g. the sp
register-shim pack `func_800B16A0` → `3file[sprawdma|spsetpc|spsetstat]`, hidden under `upstream
none` because its unattributed `func_` primary set the column). Caveat: a member whose ROM symbol
is a `#pragma weak` alias (e.g. `cosf`, whose `gu/cosf.c` defines `__cosf`) shows as `=?` in the pack
and is not counted in `c-combined`, since `upstream_index` keys on the defined name, not the alias; such
a leaf still needs manual identification (S64 `cosf` was found by disassembly, recorded in BACKLOG).

**Sub-cases / variants:**

**Decompose strategy — carve a pack's jal-free + rodata-free sub-slice first.** A `none`-upstream
classical pack priced 8/13 by the 8-gate can hold a clean, low-risk sub-slice: a contiguous run of
members that are all (a) `jal`-free (no callee resolution) and (b) reference no TU-local rodata (no
`rodata-jtbl`/`rodata-straddle`/`rodata-literal` on those members — an already-placed/named jtbl or
data table is an extern ref, which does not block a carve), whose start (and, if not the pack tail,
end) is a 16-aligned ROM boundary. Such members compile position-independently (branches are
fn-local, data refs are `%hi/%lo` externs), so splitting them into their own `.o` is byte-neutral —
carve just that sub-slice as `[0x<16aligned>, c, <tree>/<lead-fn>]` and leave the rest asm. This
sidesteps the harder members (a jtbl-carrying head, a `jal`-heavy tail) and banks the easy accessors
atomically. S156 `func_80051E90` (6-fn pack, pts-13): the first 2 fns carry `jtbl_800CCC30`; the
lower 4 (`func_80052070`/`func_800520DC`/`func_80052100`/`func_80052168`) are jal-free with no local
rodata, so they carved cleanly at the 16-aligned `0x2D470` boundary and banked as a seed-5 sprint,
leaving the jtbl pair asm. The ranker does not yet auto-suggest the sub-slice (a tracked follow-up);
apply this manually at the plan gate by reading the pack's `.s` for `jal`s and rodata `%lo` refs.

**Inter-file stray leaf: `unattrib-leaf:0x<vram>`.** Within a `c-combined` pack, a lone `=?`
member whose nearest named-C members before and after resolve to different stems straddles the
file boundary — the split must consciously assign it to one singleton, or a silent `?` rides into the
wrong side. `pick_target.py` flags its vram, but only for a lone straddler (a clean 2-file split with
one stray leaf); a whole foreign TU interleaved in (e.g. the `__assert`/`nuboot` game-boot region's 11
interspersed game `?`s) is the messier `pack`/`upstream-fncount-mismatch`/`game-region-mirror` case,
not a stray boundary leaf, so it does not fire. Motivating case: the pre-split S120 `[0x7D970]`
`c-combined:2file[nucontgbpakfwrite|nusimgr]` pack, where `func_800A2780` (a 0xC-byte leaf returning
`&0x800F77D0`, in neither upstream source) sat exactly between `nuContGBPakFwrite` and `nuSiMgrInit`.
The S120 split at the 16-aligned `0x7DB80` boundary left it in the trailing `[0x7DB80,asm]` subseg
with `nusimgr` (a carry-over), but the flag would have surfaced it at the gate.

**Resolution rule — a leaf returning `&<static>` is same-TU, not foreign.** S120's "in neither
upstream source" framing for `func_800A2780` was incomplete: the 0xC leaf is `return siMgrStack;` —
its `&0x800F77D0` target is the base of `nusimgr.c`'s own file-static `siMgrStack` (siMgrThread
0x800F7620 + 0x1B0 = 0x800F77D0). A static is file-local, so a function that takes its address must be
compiled in the same TU. The leaf therefore is `nusimgr.c` — an MG64-added leading accessor the
2.07 upstream lacks — and banks with it (written `return siMgrStack;`, `func_` name kept), not as a
foreign micro-TU. **So when an `unattrib-leaf` / stray boundary leaf returns `&<addr>`, resolve `<addr>`
against the placed static map of the two neighbors before deciding foreign-vs-same-TU: an address
inside a neighbor's static (file-local) range proves same-TU; only an address in a global/other-TU
range (or an unrelated MMIO/const) leaves it genuinely foreign.** (A `pick_target` `leaf-returns-static:
<file>` annotation that resolves the leaf's `lui/addiu` target against the static map would surface this
at the gate; deferred tooling.)

**Not every pack splits: `single-file-pack`.** When every pack member resolves to one
upstream C file (one stem, no `=?`/asm members), `pick_target.py` emits `single-file-pack:<n>fn[…]`
instead of `pack:<n>fn[…]`. This is the atomic-verbatim-mirror class (guPerspectiveF+guPerspective
S55, guLookAtHiliteF+guLookAtHilite S64, guTranslateF+guTranslate S67): the whole subseg is one
upstream file, so there is no upstream-file boundary to split at. Copy it verbatim and bank/spike in
one shot (route to `#upstream-mirror-pattern`, not the split procedure below). The `pts` seed is
unchanged (the pack penalty keys on `nfns>1`, not the kind), so the relabel is display-only; it just
stops the gate reading an atomic mirror as a split-required blocker. A mixed asm+C pack or a
multi-stem pack keeps `pack`.

**Foreign TU bundled in a single-stem pack: `upstream-fncount-mismatch:<m>vs<n>`.** The
mirror-image hazard: a pack has one named C stem but more functions (`m`) than that upstream `.c`
defines (`n`), so the surplus members are a separate TU sharing the subseg. Split it off and mirror
only the upstream's `n` fns. This is the named-symbol analog of `coddog-fncount-mismatch` (which keys on the
coddog identity). Motivating case: `_Printf`/xprintf's `pack:3fn[_Printf=xprintf,_Putfld=xprintf,
func_800B1580=?]`. xprintf.c defines 2 fns, the pack holds 3, so `func_800B1580` (a separate
`__osDpDeviceBusy` TU) was split off at its 16-aligned boundary before the verbatim mirror. This
relies on an accurate upstream function count: `_iter_upstream_functions` is a depth-aware scan (not
the column-anchored `UPSTREAM_DEF_RE`) so it counts every def shape (ANSI, K&R, single-token
implicit-int K&R (`_xatan(u,v)`), and stray-leading-space headers) and skips protos / `#define`
macro headers / doc-comment signatures; an under-count there would false-fire this on a genuine
single-file pack (the nugfxtaskmgr ` void nuGfxTaskStart(...)` near-miss). Advisory/display-only.

**The split-off TU's carried label is a hint, not a source attribution.** When you split a
foreign TU off and leave it `asm` for a later sprint, whatever you call it in the split-sprint's
notes is a quick guess: re-derive its real source with coddog + the asm at the next gate, never
trust the carried label. S104 split `func_800B1580` off xprintf and called it the "`__osDpDeviceBusy`
TU"; S105 picked it up and the gate found it was actually `osDpSetNextBuffer` (`src/io/dpsetnextbuf.c`,
coddog@99.99): the fn calls `__osDpDeviceBusy`, it isn't it. The verbatim mirror banked first-try
once the source was correctly attributed. (Standard practice already; codified because the imprecise
carried label could mislead a less careful pass.)

**Procedure:** Insert a new `[0x<offset>, asm]` line at the boundary between upstream files (read the
first instruction's vram from the asm). Two cases: (a) upstream-mirror on the first file → flip the
first chunk to `[0x<seg>, c, lib<name>/<basename>]`, leave the rest `asm`; (b) classical (statics
block the mirror) → flip the first chunk to `[0x<seg>, c]` so the scaffold only carries
`INCLUDE_ASM` stubs for the functions you're targeting. Same principle for overlay-BSS packing.

**Provenance:** 0x8E620 (`rand`+`srand` from `rand.c` packed with `_xsincos`+`sin`+`cos`+`tan` from
`sin.c`).
S122 (a `&<static>` leaf resolving to `nusimgr.c`'s `siMgrStack` base is same-TU; it banks with
`nusimgr.c`, not as a foreign micro-TU).

**Per-distribution upstream caveat:** `pick_target.py`'s `upstream` column and the gate's
"upstream availability" note come from the pack name-map, not the bytes. A fn the map calls a
mirror may ship as hand-asm `.s` in one distribution (libultra_modern) yet have been compiled from
the original combined `.c` in this ROM. Before flagging the `.s`-mapped fn classical/hand-asm,
disasm-probe it for compiled-C structure (stack frame + nested-loop counters + `slti`/`bnez` loop
tails). S51 `guMtxCatF` mapped to `mtxcatf.s` (hand-asm upstream) but the disasm was a textbook
compiled nested-loop matmul: both pack fns were clean C from the original SGI `mtxcatf.c`. See also
the non16align combined-subseg case when fn2 is tight-packed.

---

## register-reuse nudge (classical regalloc)

**Rule:** When a classical match locks just short with the only diff being which scratch register
holds an intermediate (same value), split the single expression into two statements over one lvalue
to force GCC to reuse the register. The value is identical; the split changes only register-allocation
pressure.

**Trigger:** asm-differ shows e.g. `srl a0,…` in the target vs `srl v1,…` in yours, same value.

**Procedure:** `bit = (status>>8)&1;` (compiler used temp `v1`) → `bit = status>>8; bit &= 1;`
(reused `a0`).

**Provenance:** S11 (flipped score 400→0 in one iteration).

**Variant — inverted-guard for a `return DEFAULT` tail.** For `if (cond) return A; return
DEFAULT;` where the guard variable lands in the wrong scratch reg (mine `slti v0`/`beqz v0`, target
`slti v1`/`beqz v1`) — same branch encoding, result correctly in `v0`, only the guard temp differs —
invert the guard: `if (!cond) return DEFAULT; return A;`. The two forms emit the identical `slti`+
`beqz` (gcc computes `cond` and branches on its negation either way), but inverting swaps which value
is the fall-through, which flips the pseudo coloring so the guard temp goes to `v1` and the returned
value to `v0` (no trailing `move v0,v1`; the `beqz` delay slot becomes `move v0,zero`/`nop`). Root
cause: `REG_ALLOC_ORDER` is undefined in the KMC `config/mips/mips.h`, so gcc 2.7.2 falls back to
default ascending order (`$2`/`v0` before `$3`/`v1`); whichever pseudo is processed first greedily
grabs `v0`, and inverting the guard reorders that. S156 `func_800520DC` + the `func_80052070`
scenario-tail (`if (scenario_mode_id >= 12) return 0; return D_801B6098;`).

**Variant — array-index `+` operand order picks the `v0` accumulator.** For
`arr[termA + termB]` where both terms are strength-reduced multiplies, gcc computes the right `+`
operand's term into `v0` (the accumulator that receives the final `addu v0,v0,v1`) and the left into
`v1`. So to make the target's "computed-into-`v0` term" match, put that term on the right of the `+`.
S156: `arg0*200 + arg1*10` put `arg1*10` in `v0` (target did `a1*10` first); `scenario*12 +
D_801B6098*2` put `D_801B6098*2` in `v0` (target loaded `D_801B6098` first). Same value, same
instructions, only the two multiply blocks swap order. **Why (KMC gcc 2.7.2 `expr.c:5248-5290`,
`both_summands`):** after expanding both operands, the `PLUS`-sum path reassociates and "puts a
multiplication first" (5289-5290 swap) + folds constants; the emitted order of the two multiply
subtrees is driven by that reorder, so swapping the source order of the two terms flips which global
is loaded first. Empirical rule: if a near-miss only differs by which term is loaded/accumulated
first, try both `A + B` orders. S159 `func_80051FCC` matched with `scenario*12 + D_801B6098*2` (the
cheap `D_801B6098*2` on the right → loaded first).

**Variant — branch-likely (`beqzl`) on a coalesced return-var; invert the branch.** A
`return-DEFAULT` tail can miss not on the guard-temp coloring (the inverted-guard variant above) but on the
branch form: `if (cond) return VAR; return CONST;` where `VAR` coalesces into `v0` makes gcc emit a
branch-likely (`beqzl`) that annuls the lone `li v0,CONST` in the delay slot (so `v0` keeps `VAR` on
the not-taken path) — 4 insns. If the ROM instead uses a plain `beqz` + `li v0,CONST` (delay, always
run) + `addu/move v0,<scratch>,0` on the fall-through (i.e. `VAR` is held in a scratch reg, not `v0`)
— 5 insns — invert the branch so the constant is the early return: `if (!cond) return CONST; return
VAR;`. That makes `VAR` no longer the coalesced-into-`v0` fall-through value, so it lands in a scratch
reg and the single-insn-skip annul pattern no longer fires → the plain `beqz` + `move` form. **Why
(KMC gcc 2.7.2 `reorg.c:1141-1211`, `optimize_skip`):** the comment at 1161-1166 states it directly —
when a conditional branch "goes around a single insn", gcc inverts+annuls the jump ("the same effect
in fewer insns"), which is the `beqzl`. That optimization only applies when the skipped insn (`li
v0,CONST`) is the lone difference, i.e. when `VAR` already occupies `v0`; inverting so `CONST` is the
early return removes the single-skip shape. S159 `func_80051FCC` scenario tail: `if
(scenario_mode_id < 12) return scenario_mode_id; return 8;` emitted `beqzl` (sm→v0); inverting to `if
(scenario_mode_id >= 12) return 8; return scenario_mode_id;` gave the ROM's `beqz; li v0,8; addu
v0,a0,0`. (Sibling of the guard-temp inverted-guard variant above and of
[#return-type-is-load-bearing](#return-type-is-load-bearing): all three turn on what occupies `v0`.)

**Variant — address-select for a pointer/index `a0↔a1` swap + `beql`-vs-`bne` branch.** A
`cond ? p2[i] : p1[i]` value-select (where `p1`/`p2` are switch-picked pointers and `i` is a loaded
index) can lock on a PERVASIVE register swap: the ROM puts the pointer in `$a1` and the index in `$a0`
(`addu v0,a1,a0`), yours the reverse — repeated across every switch case's pointer load (`addiu a0,`
vs `addiu a1,`) — plus the guard compiles `beql cond,2` + a `p1=p2` move where the ROM has `bne cond,2`
with the address `addu` in the delay slot. Fix by selecting the ADDRESS, not the pointer:
`cond ? &p2[i] : &p1[i]` then deref. GCC then computes `p1+i` and `p2+i` in the two branch arms
(matching the ROM's `bne cond; addu v0,p1,i; addu v0,p2,i`) instead of a pointer-move + shared index,
which also settles the pointer→`$a1` / index→`$a0` coloring. S185 `resolve_shot_quality_table`: 22
register/branch diffs → 3. (Same family as the array-index-`+`-operand-order variant above: which value
"owns" the low arg reg is decided by how the source spells the address computation.)

**Variant — load-use interlock vs hoisted independent load (`v0`/`v1` scratch swap) is IRREDUCIBLE;
classify and carry, do NOT permuter.** A 2-instruction schedule swap where the ROM ACCEPTS a load-use
interlock (`lbu v0; addiu ...,v0` back-to-back, reusing the dying load reg) and your build HOISTS an
independent load into the load-shadow (`lbu v0; lh v1,<indep>; addiu v0,...`) — flipping which of
`v0`/`v1` holds each value and the downstream `mult`/op operand order, everything else byte-identical —
is a deterministic KMC gcc 2.7.2 sched+regalloc **fixed point**, not a source-reachable near-miss.
Root cause (source-proven, S185 sched.c fan-out): the load result-latency is 3 (`config/mips/mips.md`
`define_function_unit "memory"`), so the bottom-up list scheduler (`sched.c` queue-by-latency) pulls the
independent load ≥3 slots ahead of the `mult`, into the earlier load's shadow; that hoist lengthens the
load's pre-reload live range (`sched.c` `sched_reg_live_length` feedback) so the allocator gives it a
FRESH reg — whereas the ROM allocated it to the dying load's reg (reuse), whose write-after-read
anti-dep (`sched.c` `sched_analyze_1`) then makes the hoist ILLEGAL. The schedule is a pure function of
the allocation and vice-versa (self-consistent). No faithful C rewrite of the immediate dataflow reaches
it — explicit temp, split statement, mult-operand swap, and load-operand-first are all DAG-identical and
inert; only a genuine surrounding register-pressure difference (an extra value live across the region)
could flip it, which by definition cannot exist when the rest of the fn is byte-identical. **The
permuter cannot help** (it mutates C; no C mutation changes the DAG the allocator sees). Recognize on
sight and carry as a structural-complete spike (kin to `#pervasive-regalloc-classical-main` /
`#loop-weight-and-live-length-regalloc-steering` / the `#abs-coalescing` fresh-reg LAW: same
"which scratch reg holds the intermediate, decided upstream of source" class).

---

## isolated-compile caveat

**Rule:** The execution loop compiles `nonmatchings/<func>/base.c` standalone; KMC GCC's
regalloc/scheduling can differ from the in-tree compile. That is why every match runs an in-tree
byte-`cmp` spot-check before being declared. If iteration mismatches cluster around extern address
loads (HI/LO16 relocs against project-local symbols), those are isolation-only; the spot-check is
the truth.

**Trigger:** Score ≠ 0 in isolation but the mismatches are HI/LO16 reloc address loads.

**Fast recognition signal:** `decomp_loop.py` reports a non-zero `score` with empty
`top_mismatches` and `match_count == total_rows` (every row matched, yet a residual score). That
combination is definitionally an isolation artifact: asm-differ found no mnemonic-row diff, so the
score is pure reloc/addend noise (struct-field LO16 addends like `pfs->queue`/`pfs->channel`, extern
HI/LO16 calls against now-placed symbols). It is not a near-miss: do not iterate C, do not run the
permuter. Go straight to the in-tree spot-check / full-make ROM SHA-1, which is byte-authoritative.
S43 `osGbpakGetStatus` scored 15 / 99.8% this way (76/76 rows, empty mismatches) and the full make
matched the baserom unchanged.

**Inverse trap: the isolated score can under-report an intra-fn instruction-scheduling reorder.**
The artifact signal above is a false-clear direction (isolated looks worse than reality).
The opposite also happens: isolated looks better than reality. When your candidate and the target
share the same instructions but in a different order (a scheduling reorder, e.g. a load hoisted to
the top of a block vs kept near its use), asm-differ's alignment matches the moved instruction across
its move and nets only the surrounding reloc noise, so the isolated `score`/`match_count` reads
near-perfect while the full-make ROM SHA still misses. S150 `cfb_setup`: the isolated diff showed
44/45 rows (only the `D_800B67A4+0x2` vs `D_800B67A6` reloc-addend row), yet the full make missed on
an else-branch `lw framebuf[2]` that the target hoisted to the block head and the build kept late.
**Recipe:** when the isolated diff reads "near-perfect / artifact" but the full-make SHA misses, do
not trust the isolated score. Localize with `cmp -l build/mariogolf64.z64 baserom.z64 | head`: the
first field is the 1-based decimal byte offset, so `rom = offset - 1` (hex it, map to the function
via the subseg rom base), and the differing bytes point at the exact mismatched instructions. Fix
the source (an ordering swap, or force an early read into a temp to hoist a load) and re-`cmp` to
converge. The full-make ROM SHA-1 is the only authority; the isolated score is advisory in both
directions.

**Register-permutation case (S191): the isolated score is IDENTICAL across a reg nudge, so an
unchanged score is NOT "no progress."** When a fn has extern refs (reloc noise) AND its only real
diff is a register permutation (the same instructions, same order, one value in a different reg),
asm-differ's row-matcher normalizes register numbers, so both the wrong and the right allocation align
every row and net the SAME reloc-noise score. S191 `func_80037E50` scored an identical 3320 / 0.1487
(39/39 rows, empty `top_mismatches`) with `quality` in `$a0` (wrong) and in `$v1` (byte-exact) — the
decomp_loop score could not distinguish them. **Recipe:** for an extern-ref fn whose residual is a
register choice, iterate on the **in-tree `.o` objdump register operands** (ignore the `lui`/`addiu`/
`jal` reloc-immediate slots, which read `0x0` unresolved), NOT the decomp_loop score; a nudge that
flips the score by zero can still be the one that lands the match. Pair this with the
`#loop-weight-and-live-length-regalloc-steering` `.greg`-dump method to see which hard reg the target
var needs free.

**Procedure:** Trust the in-tree spot-check / full-make SHA, not the isolated score.

**Flipped-subseg / partial-one-tu case (S187): the isolated reference is STALE, not merely noisy.**
Once you flip the subseg to `c` and start banking fns into `src/<file>.c`, the isolated
`nonmatching-func` / `decomp_loop.py` path reads `build/asm/<seg>.o` (built at bootstrap), which no
longer carries the flipped fns as standalone objects. For a fn you seed AFTER the flip, the differ
has no valid base: it reports empty `base_text` rows and a bogus low percent (S187 `func_80077C18`
read 0.21 with the C already ~93% correct). Do NOT read that as a near-miss regression. For an in-tree
partial one-tu, the authoritative per-fn check is `mips-linux-gnu-objdump -d build/src/<tree>/<file>.o`
sliced to the fn vs its `asm/nonmatchings/<seg>/<lead>/<func>.s`, diffed after normalizing
objdump's pseudo-ops (`move rd,rs` ≡ `addu rd,rs,zero`; `li rd,N` ≡ `addiu rd,zero,N`; `nop` ≡
`sll zero,zero,0`) and masking reloc/immediate operands (`%hi`/`%lo`/branch targets/frame offsets) —
the raw mnemonic diff over-reports by counting those display-only forms as mismatches. Use this
objdump slice, not the stale isolated score, to measure a post-flip fn; the full-make ROM SHA-1
remains the only authority.

**Mid-TU standalone-offset case (S206): a byte-exact fn scores >0 purely from its TU position.**
When you seed a fn that lives in the MIDDLE of a multi-fn TU (not the lead fn) as a lone
`nonmatchings/<func>/base.c`, it compiles at TU offset 0, but the ROM reference has it at its true
offset (the preceding fns' bytes come first). asm-differ's branch-target normalization then mis-flags
the fn's OWN internal branches (their absolute targets differ by the offset delta), netting a residual
score on an otherwise byte-identical body — and `ignore_addr_diffs` does NOT cover this (it is a
branch-target artifact, not a reloc-immediate one). S206 `func_800779A8` (4th fn of
`func_800772B0.c`) read ~1200 / ~6 branch rows flagged despite being 75/75 byte-exact.
**Recipe:** put the PRECEDING same-TU fns as `INCLUDE_ASM("asm/nonmatchings/<seg>", <fn>)` lines
ABOVE the target in base.c, so it lands at its true TU offset (0x6f8 for 779A8) and the standalone
score reads 0. This is a positioning fix only; the body was already correct. Kin to the S187 stale-
reference case above — both are "the isolated path can't see the real TU layout," resolved by the
full-make ROM SHA-1 either way.

---

## Decompile-vs-asm authority

**Rule:** The asm (`disassemble_function`) is ground truth, not the Ghidra decompile. The decompiler
can be silently wrong on classical leaves. The standard classical seed is the combined m2c-body +
Ghidra-typed-context seed: m2c translates the asm into the compiled seed body, typed by a Ghidra-MCP
struct context (`ghidra_ctx.c`); the decompile is a shape/type reference only, never the body. Verify
the m2c seed against the asm block, since m2c can also be silently wrong.

**Seed roles.** `tools/seed_c.py` writes `base.c` with the m2c output as the compiled body, the
decompile as a `#if 0` reference (suppressed when degenerate), and the asm as the labeled authority.
Its JSON `body_source` is `m2c`, or `ghidra`/`stub` when m2c fails.

**Provenance:** S11 (Ghidra rendered `func_800AB600`'s return as `return 0` when the asm returns
`(status>>8)&1`).

**A `_NON_MATCHING`-suffixed MCP decompile = distrust the whole body, not just types.** When
the Ghidra function name in the MCP decompile carries a `.NON_MATCHING` suffix (or the plate comment
cites a prior non-matching attempt), the cached pseudocode can be actively wrong, not merely
imprecise: it may return a literal `0` with no body, or render a phantom operation the bytes do not
contain. S156's four accessors: `func_80052100`/`func_80052168` both decompiled to `return 0;` (no
body at all), and `func_80052070`/`func_800520DC` showed a phantom `DAT_801b6098 >> 0x1f` where the
raw bytes `8C426098` are a plain `lw` with no shift. The asm-first fast-path ignored all of it and
matched. Treat a `_NON_MATCHING` decompile as shape-only-if-that (often not even shape); the `.s` is
the sole authority.

---

## open-band fast-path

**Rule:** Once a band has ≥2 banked siblings (e.g. `monegi/thread/`, `monegi/vi/`), a candidate
`pick_target.py` reports with no hazard may skip the agent's manual per-ref symbol-presence
re-grep at the gate; `pick_target.py`'s own ref-grep is authoritative. The fast-path never overrides
a flagged hazard: a `defines-data`/`refs-unplaced`/`needs-header` flag routes normally even inside a
warm band. The gate build-check stays load-bearing regardless.

**Trigger:** `band` = `warm` and `hazards` = `-`.

**Provenance:** S14.

---

## non16align

**Rule:** KMC `as` auto-pads `.text` to 16-byte section alignment, so a non-16-aligned subseg can
mis-place downstream bytes.

**Trigger:** `pick_target.py` flag `non16align`.

**Procedure:** Split the yaml at the alignment boundary or add a `.balign`.

**Sub-cases / variants:**

**Combined-subseg case:** When a 2-fn pack has fn2 packed tight after fn1 at a non-16 offset
(fn1's size is not a 16-multiple), you cannot split into per-fn subsegs: KMC `as` pads fn1's
standalone `.o` `.text` to 16, inserting bytes that shift fn2 downstream and breaking the ROM SHA-1
on a bare stub flip. The fix is the opposite of the normal pack split: flip the whole pack to one
combined `[0x<rom>, c, lib<name>/<basename>]` subseg holding both functions in one `.o` (both
`INCLUDE_ASM` stubs, then decompile each). Padding then lands only at the object's end, matching the
original single-`.o` layout. S51 `guMtxCatF`(0xDC)+`guMtxXFMF`(0xAC, at non-16 `0x848AC`) → one
`gu/mtxcatf.c`. A bare-stub split flip is the gate-build canary: SHA-miss with a correctly-named
stub ⇒ alignment, not a bad offset.

**Cleanup after a reverted split:** when the gate first tried a per-fn split and then reverted to the
combined subseg, splat leaves the abandoned attempt's `src/lib<name>/<fn2>.c` stub + its
`asm/nonmatchings/.../<fn2>/` dir on disk (splat never deletes files for removed subsegs). A blind
`git add -A` then commits the orphan stub. Before committing the bank, `git rm` the stale
`src/.../<fn2>.c` and `rm -rf` its asm scaffold dir (S51: orphan `gu/mtxxfmf.c` swept in, removed in a
fixup commit).

---

## trailing-alignment pad after a C mirror

**Rule:** splat extracts the whole subseg slot: the function plus the `0x00000000` nop padding that
fills the gap up to the next subseg. When that next subseg sits at an alignment above 16 (32/64/128),
the pad is larger than the ≤12B a compiler's 16-byte `.text` alignment emits. A verbatim C mirror
therefore compiles short of the slot, the ROM shrinks, and everything downstream shifts → a SHA-miss
in the execution middle. It is invisible to every gate check: the `INCLUDE_ASM` stub carries the pad,
so the gate build is green; only the real C body drops it (the late-surfacing class). The
sibling guard: a function whose 16-aligned size already fills its slot (the same-subseg neighbor)
mirrors clean; only the one preceding a higher-aligned boundary pays.

**Trigger:** `pick_target.py` flag `trailing-pad:<n>B@<align>`. `<n>` = the residual pad bytes
beyond the compiler's 16-align, `<align>` = the next boundary's alignment (e.g. `96B@128`). Pre-flags
it at the gate so the split is priced, not localized mid-execution. (`docs/hazards.md:122`: the `.ld`
does not `ALIGN` between subsegs, so the pad must live in an object; there is no linker fallback. This
is the C-mirror dual of the KMC-`as` 16-pad note in `#asm-mirror-vendoring`.)

**Procedure:** flip the function subseg to `c` as usual, then split a nop-pad `[0x<gcc_o_end>, asm]`
subseg between it and the next subseg. `<gcc_o_end>` = the function subseg ROM offset + the compiled
`.o` `.text` size (`mips-linux-gnu-objdump -h build/.../<file>.o` → the 16-aligned `.text` extent);
splat re-extracts the residual nops there. The compiler's own 16-align nops (the `.o`'s tail) match the
baserom nops at those addresses; the pad subseg supplies the rest up to the higher-aligned boundary.
A pure-nop subseg is never a pick: `pick_target.py` skips any all-nop asm subseg (`code_end_rom` None
with a present listing), which also retired 8 pre-existing all-nop overlay stubs (`func_ovl*_801F4A30`).

**Worked example (`__osContRamWrite`):** function 0x204 (516B) → GCC `.o` `.text` 0x210 (528B,
16-aligned) → real slot 0x270 (624B) up to the 128-aligned `osAfterPreNMI` (0x800AF880). Split
`[0x8AC20, asm]` (= 0x8AA10 + 0x210) carries the residual 0x60 (96B) of nops. Re-extract + `make` →
ROM SHA-1 == baserom. The body itself never diverged (129 instrs byte-identical); the gap was purely
the trailing pad. The contramread sibling (slot == 16-aligned fn size) mirrored clean, no split.

**Provenance:** late-surfacing class: S18, S44; flag and `__osContRamWrite` worked example: S79.

---

## intrinsic-likely / maybe-upstream (signature hints)

**Rule:** Two advisory flags from the signature matcher.
- `intrinsic-likely`: a register/FPU/cache/TLB shim, not a classical target. If the detail names a
  vendorable ultralib TU (`intrinsic-likely:os/getcount.s`), asm-mirror it (see
  `#asm-mirror-vendoring`). `intrinsic-likely:cp0-asm(identify-TU)` is an un-named privileged hand-asm
  subseg (also `#asm-mirror-vendoring`; identify the TU first). A bare `intrinsic-likely` (no `:<tu>`,
  no `cp0-asm`) is a no-source shim → plain `hasm`.
- `maybe-upstream:<lib>:<bases>`: an un-named subseg (`func_<addr>`) that the signature matcher
  thinks is an un-named SDK mirror. Verify against the candidate upstream at the gate
  before treating it as classical. **A definitive (`>=CODDOG_MIRROR_PCT`) `coddog-mirror` on the same
  row suppresses this guess**: coddog has named the exact
  upstream file, so the weaker IDF guess is redundant noise that can point at the wrong file (S132
  `func_800A0800` listed `n_synstopvoice,n_synstartvoiceparam,n_synstartvoice` while the coddog-mirror
  correctly named `n_synallocvoice.c`, the actual source of both fns). A sub-threshold coddog hit
  stays advisory, so the guess is retained there as a second opinion.

**Trigger:** `pick_target.py` flags `intrinsic-likely` / `maybe-upstream:…`.

**Provenance:** S13 (un-named SDK mirror trap); S75 (coddog-mirror suppresses the IDF guess for
libultra); S132 (guess-suppression extended to audio).

For a definitive (not IDF-guess) version of `maybe-upstream`, see `#coddog-cross-ref`: it pairs the
un-named fn with its ultralib identity by instruction-hash match.

---

## coddog cross-ref

**Rule:** `pick_target.py` classifies an un-named (`func_<addr>`) subseg as `upstream none` →
classical, even when it is a verbatim ultralib mirror. The names being absent is the only reason.
crc.c was mis-seeded pts-13 classical (`none` + `pack:2fn`) but is a trivial verbatim
2-fn mirror. coddog (`compare2`, reloc-masked instruction hashes) pairs each MG64 fn with the
ultralib fn it matches, revealing the source file: this turns the `none`/classical guess into a known
mirror target. Per the sweep, ~all of the remaining libultra band is verbatim-mirrorable.

**When:** an `upstream none` candidate in the libultra vram range (`0x800A_xxxx`–`0x800B_xxxx`)
carrying `pack` / `jal-count-mismatch` / `maybe-upstream`, before committing it to the classical loop.

**Procedure:**
1. Run the sweep: `make coddog-sweep` (or `tools/coddog_sweep.sh`). Needs a fresh `make` (MG64 ELF),
   a built ultralib (`~/development/repos/ultralib/build/J`, the VERSION_J pin), and the coddog
   binary. It builds a combined ultralib-J ELF (KMC objects need
   `objcopy -R .mdebug -R .reginfo …` to normalize the symtab + strip ECOFF, then
   `ld -r --allow-multiple-definition`; objdiff cannot read a `.a` archive), runs `compare2`, and
   writes `tools/coddog/coddog_map.tsv` (gitignored: local, build-dependent).
2. `pick_target.py` (`build_coddog_index`) reads the map if present: a candidate whose lead fn is in
   the map gets a `coddog-mirror:<file>@<pct>` hazard; a ≥99% non-audio hit is re-priced
   `upstream libultra` so `seed_points` drops it off the `classical and pack` → 13 path (crc.c:
   13 → 3). Audio hits stay advisory (the one-time audio-header enabler is not modeled). Absent map
   → ranking unchanged (the committed golden is map-free; `CODDOG_MAP` env overrides the path).
3. Treat a `coddog-mirror` candidate as `#upstream-mirror-pattern` (verbatim cp from the matched
   `.c`); the `@<pct>` is the confidence (99.99 = byte-verbatim once placed). Not every coddog
   match is an atomic verbatim cp; see step 4.
4. **Trap re-scan:** because the candidate is un-named (`func_<addr>`), its own
   `defines-data` / `file-static` / `needs-header` detectors (which key off the *named* upstream)
   never ran, so a coddog match to a file that defines data or a file-scope static looked clean
   under the bare flag. `build_rows` now re-runs those three *file-level* (name-independent)
   detectors on the coddog-resolved `.c` and appends their hazards (and `seed_points` re-prices via
   `drop`/`needs_copy`). So a `coddog-mirror` candidate carrying `file-static` / `defines-data`
   routes to `#file-static-bss-layout-conflict` / `#defines-data` (a `.data`/`.bss` sibling carve or
   classical), not an atomic verbatim cp. Motivating case: `func_800AC110 → piacs.c` ranks pts-5
   (not the bare-coddog pts-3) because piacs.c defines `__osPiAccessQueueEnabled` + a
   `static OSMesg piAccessBuf[]`; `osMotorStop → motor.c` likewise surfaces
   `defines-data:__osMotorinitialized[...]`.

5. **Twin disambiguation:** coddog's reloc-masked hashes can pair a candidate with a
   near-identical twin file rather than its real source. `func_800AC110`+`__osSiGetAccess`+
   `__osSiRelAccess` (the SI access-queue subseg) coddog-matched `src/io/piacs.c@99.99`, but its
   named members name `siacs`. `pick_target.py` cross-checks the coddog basename against the pack's
   *named* members' upstream basenames and, on disagreement, appends `coddog-twin:<matched>!=<member-src>`.
   Mirror from `<member-src>` (siacs.c), not the coddog `<matched>` file (piacs.c). A @99.99 twin
   is byte-identical once placed (the body is the same either way), so this only removes a manual
   reconcile step, but it pins which upstream the agent copies + which symbol names it places. No-ops
   when the candidate has no named member (nothing to disagree with) or the basenames agree.

6. **Structural-fingerprint guards (a @99% match that is not a source attribution).** coddog
   hashes a fn's branch/call shape, so a small source can fingerprint-match a much larger, unrelated
   pack. Two name-independent guards fire when one coddog `.c` claims a whole multi-fn pack:
   - **`coddog-fncount-mismatch:<m>vs<n>`**: the matched source defines fewer fns (`m`) than the
     pack holds (`n`), so it cannot be the sole source → the pack is multi-file. Under-count only (a
     true single source may define more via version/`_DEBUG`-gated extras). The check now also
     runs when the coddog identity is carried by an un-named tail member (not the pack leader), the
     case the primary-only check missed. `func_80050400`'s leader is absent from the map but a tail
     member carries `llcvt.c` (8 fns vs the 11fn pack). The same under-count check now also runs
     in the `_resolve_audio` pass (libmus/libnaudio/nuaulstl), where it had been omitted — `al_init`'s
     13fn pack coddog-matches `player_fx.c`@99.99 but that file defines 6 fns → `coddog-fncount-mismatch:6vs13`
     (was a bare, clean-looking mirror). Motivated by the `0x7BDE0` 2-file pack: its sub-coddog-floor
     4-instr `n_alSynDelete` leaf went unmatched, leaving `n_synsetfxmix.c`@99.99 looking single-file
     until hand-disassembly at the gate revealed the second file.
   - **`coddog-structural:<file>@<pct>`**: the matched source's meaningful-LOC implies a compiled
     size far below the subseg's (`subseg_bytes > 64 × source_LOC`). `llcvt.c` is 8 trivial `return d;`
     conversion stubs (~250 B) yet coddog matched it @99.99 to three distinct subsegs (2032 B / 2912 B /
     7728 B). Advisory/display-only, the size-dimension companion to the fn-count guard. When either
     guard is on a coddog row, do not treat the `coddog-mirror` flag as a single-file mirror identity.
     - **`llcvt.c` is not linked at all (workhorse-linked / wrapper-absent).** Beyond the
       structural over-match: MG64 never links `llcvt.c`. Its 8 stubs are thin wrappers that `jal` the
       libgcc soft-float workhorses (`__fixdfdi`, `__floatdidf`, …); KMC GCC emits calls to those
       workhorses directly, so the workhorses are present (`__floatdidf` @0x800B3D40,
       `__fixunsdfdi` @0x800B3C20, in the libkmc/libgcc band, already named) while the `__d_to_ll`…
       `__ull_to_f` wrapper TU is absent everywhere (name files + asm). So every `__d_to_ll @99.99`
       row is a reloc-masked FP on the stack→`jal`→return stub shape (`func_800504E8` calls a game fn;
       `spawn_object_simple` is a game fn). Lesson for a future planner: a tiny-stub coddog identity
       can mean the source TU was never linked, not just over-matched; confirm the *workhorse*
       symbols, not the wrapper, before chasing the `.c`.
   - **Not done (carry-over):** a `.data`-carve detector for a file's own initialized file-statics
     (the `_Litob` ldigs/udigs class). An `addiu %lo(D_<addr>)` into the `.data` range cannot be
     distinguished from a shared extern reference by asm alone (the same instruction serves both), and
     `refs-unplaced` has gaps, so a naive scan mis-routes cross-file externs to a phantom carve. Deferred
     to a dedicated tooling sprint; see `BACKLOG.md ## Carry-overs`.

**Trigger:** `pick_target.py` flags `coddog-mirror:<file>@<pct>` (only when the map exists); any
`file-static` / `defines-data` / `needs-header` on the *same row* is the trap re-scan; a
`coddog-twin:<matched>!=<member-src>` on the same row means coddog named the twin, so mirror from
`<member-src>`; a `coddog-fncount-mismatch` / `coddog-structural` on the same row means the @99%
match is a structural fingerprint, not a source attribution (the pack is multi-file / mis-attributed).

**Provenance:** S71 (crc.c mis-seed; the recipe + reusable tool memory in `coddog-ultralib-crossref`);
S72 (the trap re-scan: coddog-mirror candidates can hide a defines-data/file-static BSS trap);
S81 (`io/siacs.c`: coddog named the twin `piacs.c`, named members → real source `siacs.c`; the
gate-safe drop-def symbol-add note also landed in #defines-data); S88 (`coddog-fncount-mismatch`);
S92 (the fncount check extended to tail-carried identities + the `coddog-structural` size guard,
both retiring the llcvt false-positive class, where one tiny source fingerprint-matched 3 subsegs);
S108 (`llcvt.c` never linked: KMC emits the libgcc soft-float workhorses directly, the wrapper TU is
absent, so a tiny-stub coddog identity can mean the source was never linked);
S131 (the fncount under-count check ported into the `_resolve_audio` pass, where it had been omitted;
surfaced by the `0x7BDE0` 2-file pack whose sub-floor `n_alSynDelete` leaf hid the second file).

**Sub-cases / variants:**

**Nusys sweep (libnusys analog):** the same machinery pointed at the nusys-2.07 library instead of
ultralib. `make coddog-sweep-nusys` runs `tools/build_nusys_ref.sh` (compile every
`nusys/src/mainlib/*.c` with the project's KMC GCC + `LIBNUSYS_CFLAGS`, the in-tree mirror recipe,
into `build/nusys-ref/`, then merge to one relocatable ELF) then `tools/nusys_sweep.sh` (`compare2`
MG64 vs that ELF, writing `tools/coddog/nusys_map.tsv`, format
`mgname<TAB>nusysname<TAB>mainlib/<file>.c<TAB>pct`). `pick_target.py` (`build_coddog_nusys_index`)
reads it as a separate additive pass: a `>=99%` hit on an un-named subseg flags
`coddog-mirror:mainlib/<file>.c@<pct>` and re-prices `upstream libnusys` (libnusys is already a
first-class upstream lib, so the column / `--lib` / `seed_points` need no other change). The pass is
keyed on the nusys map alone, so an absent `nusys_map.tsv` leaves libultra ranking byte-identical;
the source-path prefix (`mainlib/` vs `src/`) tells the two maps apart downstream, and re-pricing
routes `coddog-mirror:mainlib/...` to `src/libnusys/`. Caveat: nusys has many tiny (5 to 9
instruction) wrappers (`nuGfxSetUcodeFifo`, `nuContGBPakMgrInit`) that coddog reports at 99.99%
against unrelated small fns, the same structural-fingerprint class as steps 5 and 6; smallest-first
+ the gate read filter them (a big pack mis-re-priced to libnusys ranks last by size). **Provenance:**
this sweep.

**Nusys version divergence: MG64's libnusys is not uniformly nusys-2.07.** `build_nusys_ref.sh`
compiles the pinned 2.07, so `coddog-mirror:mainlib/<file>.c@99.99` is a structural match, not a
byte-exact source pin. Some files are an earlier revision: `nucontmgr.c` is **2.05** (the 2.06/2.07
rewrite changed `nuContMgrInit`'s loop from `for(...;cnt++){ ...; bitmask<<=1; }` to the comma-operator
`for(...; bitmask<<=1, cnt++)`, which the KMC compiler emits 2 instrs shorter, `-0x10` `.text`, so the
`.ld`'s concatenated subsegs cascade a ROM-wide `-0x10` shift). **The tell:** a clean verbatim mirror
whose compiled `.o` `.text`/`.data` section size differs from the subseg size despite the coddog hit
(here `.text` 0x330 vs 0x340). **Recovery: version-hunt.** Compile every available nusys revision
(`~/development/repos/nusys/src/{1.10,1.20,2.00,2.05,2.06}` + the 2.07 pin) and pick the one whose
`objdump -h` `.text`/`.data` sizes match the subseg; 2.00 and 2.05 are code-identical (only JP-Shift_JIS
vs EN comments), so prefer the English 2.05. Tooling follow-up: `build_nusys_ref` could sweep multiple
versions, or `pick_target` could cross-check the coddog hit against the subseg byte-size and flag a
`coddog-size-mismatch` when they disagree. **Provenance:** S117 (nucontmgr.c is a 2.05 revision, not
the pinned 2.07).

**Audio libraries (libmus / libnaudio / nuaulstl): a multi-version, multi-compiler sweep that pins
the toolchain too.** MG64 embeds libmus (the Software Creations `mus_*` sequence player) plus an
n_audio synth layer. These had no prebuilt reference at the project profile and an unknown
version/compiler, so `tools/build_audio_refs.sh` builds a `{compiler × opt × version}` matrix from
the manifest `tools/audio_ref_versions.tsv` (one row per lib+version; the script expands compilers ×
opts). Compilers: `kmc` (project KMC gcc 2.7.2, the proven path), `ido53`/`ido71` (IDO `cc` via
ido-static-recomp, the authentic vendor compiler). Each cell → `build/audio-ref/<cell>/ref.o`;
`tools/audio_sweep.sh` fingerprints MG64 against every cell → `tools/coddog/audio/<cell>_map.tsv`;
`tools/audio_pin.py` ranks cells per lib by `(#exact-100%, #>=99%, #total, mean%)`, writes the
canonical `tools/coddog/<lib>_map.tsv` + `tools/coddog/audio_pins.tsv` (lib → pinned src root), and a
`PIN_REPORT.md`. `pick_target.py`'s `_resolve_audio` pass (the libmus/libnaudio/nuaulstl analog of
`_resolve_nusys`) reads those maps + pins: a `>=99%` member hit flags `coddog-mirror:<file>@<pct>`,
re-runs the trap battery against the pinned `.c`, and re-prices `upstream <lib>`. A single-identity
multi-fn pack gets the `coddog-fncount-mismatch` under-count guard + the `coddog-structural`
size guard, so a coddog `.c` that defines fewer fns than the pack is surfaced as multi-file rather
than a clean single-file mirror. Absent maps leave ranking byte-identical (additive-pass invariant).
Run via `make coddog-sweep-audio`.

**Audio toolchain gotchas (resolved):** (1) the PC-distribution SDK headers/source (`~/n64sdk`,
`~/development/sdks/n64-sdk`) have **CRLF** line terminators that IDO's IRIX `cfe` mishandles at
`\`-continued macros (`gbi.h` combiner macros → "Illegal macro parameter name"); the build stages
CR-stripped copies for IDO cells (KMC/GNU cpp tolerates CR and uses the project headers directly).
(2) IDO emits IRIX `.options` + `.gptab.*` sections GNU `ld -r` can't represent → stripped after
compile. (3) IDO `uopt -O3` SIGSEGVs on some TUs (logged + skipped, best-effort). **Result (this
sweep):** libmus pins to KMC gcc `-O2` with the n64sdkmod source — `mus_cmd_*`↔`F*` and
`func_<vram>`↔`__MusInt*` at 100% — confirming the audio engine is libmus built by the game's KMC
toolchain (IDO cells matched 0, ruling IDO out). The `.text`-size cross-check is implicit in coddog's
per-fn pct (an exact-100% match is byte-identical incl. size; a wrong version shows structural ~99%
twins but few exact hits, the same discriminator as the nusys version-hunt above).

**The n_audio_sc `n_syn*` setter vein is empirically clean-verbatim @99.99 (6/6).** Every
`n_alSyn*` setter mirror banked so far has been a byte-verbatim first-build despite carrying
`body-divergence-suspect:<file>@99.99`: S129 banked SetPan/SetPitch/StartVoice/StopVoice (4/4) and
S130 banked AddPlayer/SetVol (2/2), all Match on the first `make`. These setters are tiny
(`__n_allocParam` → fill an `ALParam`/`ALStartParam` → `n_alEnvmixerParam`, or a list-prepend under an
interrupt mask), so for this homogeneous vein the `@99.99` body-divergence flag is a reliable false
fire and the diagnosis pass (`#cross-jump-tail-merge`) can stay lightweight (a quick asm-vs-upstream
read of the `#else`/`SAMPLE_ROUND` branch, not a full divergence hunt). This is an empirical narrowing
of the `coddog 99.99 == STRUCTURE` guard for one well-characterized family, not a license to skip the
body check for heavier n_audio_sc files (`n_synthesizer.c`, `n_reverb.c`) or other libs.

**c-combined decompose + the `c-combined-undercount` file-count reconcile + body-divergence
post-decompose suppression.** The last libnaudio asm subseg `func_8009E4B0` was a 3-file
`c-combined` pack (n_auxbus | n_drvrNew | n_env) the named-symbol index priced as only `c-combined:2file`
(the n_env members were all un-named `func_<addr>`, so the named index missed them), while coddog
fingerprinted all three files. Two tooling refinements:
- **`c-combined-undercount:<named>vs<coddog>`** (`_append_coddog_aux`): when the distinct
  coddog-mirror file count exceeds the `c-combined` hazard's named-symbol file count, the pack spans
  more upstream files than the named index sees → flag it (`2vs3` here). The file analog of
  `coddog-fncount-mismatch`; tells the gate to decompose at more boundaries than `c-combined` lists and
  to hand-trace the extra file's boundary from its named member fns (the per-file vram-boundary +
  carve-extent pricing the gate still hand-traces is a tracked follow-up — it needs the member asm
  `%hi/%lo` refs at pricing time).
- **Body-divergence suppression now keys on `up_lib == libnaudio` + a clean single-source shape**
  (single-file-pack or a plain single-fn row), not the single-file-pack shape alone. A `c-combined`
  pack decomposed at the gate becomes per-file rows, and the smallest (n_auxbus, 1 fn) has no pack
  hazard, so the old single-file-pack-only key would re-flag it. `single_cod` (exactly 1 distinct
  coddog file) still gates it, so a still-combined multi-coddog pack keeps the hedge (the S123
  customization guard); the libnaudio restriction keeps the libnusys S121/S127 FORCESTOP hedge. S139
  banked n_auxbus + n_drvrNew (×2 fns) verbatim first-build after the decompose, all
  `body-divergence-suspect@99.99` false → **9 consecutive false on n_audio_sc (S133-S139).**

**Audio C-name-index mis-resolution de-blk (`_deblk_audio_variant_misresolve`).** The game's
libnaudio C-name index can resolve a fn's `up_path` to the **non-sc** `libnaudio/src/<f>.c` variant,
whose `#include "add/<f>_addNN.c"` body-fragments live in a tree the project does not mirror, so the
named-upstream pass prices them non-vendorable and sets `blocked` → the row shows a false `blk`. The
authoritative source is the coddog-confirmed **n_audio_sc** `<f>.c` with vendorable
`inc/<f>_addNN.inc.c` fragments (the N_MICRO command-stream bodies). The bogus `add/*.c` block hid
three pickable mirrors in a row (`n_save` S133, `n_resample` S134, `n_load` S135 — each a clean
first-build atomic mirror once the false `blk` was seen through). `_resolve_audio` now lifts it: when
a definitive (`>=CODDOG_MIRROR_PCT`) audio coddog-mirror replaced `up_path` with a different
`mirror_path`, that coddog source is authoritative — it drops the wrong-variant `needs-header` hazard
and re-derives `blocked` from the coddog source's (vendorable) includes only. A genuinely blocked
audio row (the libmus `aud_*` DAG: real `libmus_config.h` / `libaudio.h` non-vendorable headers on the
coddog source itself) stays `blk`, because the re-derive still finds a hard block. See also
`#static-name-collision` for the companion static-name guard the same mirror surfaces.

---

## static-name-collision (an upstream file-static reuses a placed global name)

**Flag:** `static-name-collision:<name>@<existing-addr>` (advisory, audio-scoped). Fires when a
coddog-mirror's upstream `.c` defines a function whose verbatim name is already a curated symbol in
`symbol_addrs.txt`/`ghidra_symbols.txt` placed at a **different** vram (the name is in `placed_symbols`
but is **not** one of this subseg's member fns). The classic case is an N_MICRO file-static mirroring a
non-micro twin that already holds the name: S135 `n_load.c`'s static `_decodeChunk` (this subseg's
instance at `0x8009FA14`) versus the placed `_decodeChunk = 0x800A4E3C` (the non-micro audio decoder's
static).

**Why it matters:** a file-scope `static` is **file-local** — it emits no global symbol — so the verbatim
mirror keeps the upstream name in the C body and needs **no** `symbol_addrs.txt` entry. Adding one would
be a duplicate global label (two `_decodeChunk` symbols at different vrams) and multiply-defines at the
gate scaffold, breaking the green-ROM check. The flag tells the gate up-front: keep the static
file-local, do not add a colliding `symbol_addrs` entry. Confirm the static has no **external** refs (a
`jal func_<vram>` only from within its own subseg `.s`) — then the file-local static is correct and
self-contained. S135 `n_load` carried `_decodeChunk` this way; `func_8009E4B0` (n_env/n_auxbus/
n_drvrNew) shows the same on `_pullSubFrame`/`_getRate`/`_getVol`.

**It is benign, not a problem to solve.** When the colliding names are the mirror file's own
file-statics (the usual case), there is **no** real collision: the statics stay `static` C, emit no global
symbol, and the only action the flag implies is the no-op the verbatim track already takes (add
nothing to `symbol_addrs`). The "collision" only bites if you try to globalize one of these statics (the
S135 `_decodeChunk` multiply-define). So read the flag as "these N names stay file-local," not as a
blocker. S140 `n_env.c` banked the `_pullSubFrame`/`_getRate`/`_getVol` trio first-try with zero
collision (all three left `func_<vram>` in the scaffold, kept their upstream `static` names in the C).

---

## libmus-bundled-n_audio duplicate (a SUPPORT_NAUDIO libmus archive links its OWN n_audio synth copy)

**Pattern (not yet auto-flagged; a `game-embedded:libmus` / `coddog-bundled-dup` pricing tell is a
deferred follow-up).** The `-DSUPPORT_NAUDIO` libmus archive **statically links** its own copy of the
n_audio synth driver (the `alInit`/`alSyn*` family + the FX-change `Custom*` replacements from
`player_fx.c`), a **duplicate** of the standalone n_audio_sc copies that already live in the libnaudio
region (`n_al*`). In MG64 the bundled copy sits in the `[0x78330]` `al_init`/player_fx candidate
(`coddog-fncount-mismatch:6vs13` = player_fx's 6 fns + ~7 bundled-synth fns). The two copies are the
same source compiled into two archives; the original linker resolved each archive's calls to its own
copy. Our flat decomp namespace cannot, so the copies need **distinct** decomp names.

**The call resolves through two macro layers to the bundled copy, not the standalone one.** A libmus
mirror's `alInit(...)` expands `alInit` -> (`n_libaudio_sn_sc.h`) `n_alInit` -> (`player_fx.h`, under
`SUPPORT_NAUDIO`+`SUPPORT_FXCHANGE`) `CustomInit`. So the verbatim source call lands on `CustomInit`
= the bundled FX-change synth init (S143: `0x8009CF30`, which ghidra mis-named `al_init`). The
standalone n_audio_sc `n_alInit` (S143: `0x800A0730`) is dead (zero xrefs) -- nothing reaches it
because libmus, not the standalone n_audio manager, drives the synth.

**Why it matters / procedure.** Do not read the `alInit`->`n_alInit` alias as "resolve to the placed
`n_al*` standalone symbol" (wrong address -> SHA-miss) and do not reach for a `#undef`/`#define`
redirect hack: the verbatim macro chain already points at the bundled copy. Before concluding a
duplicate-symbol blocker, read `player_fx.h` for the `#define n_alInit CustomInit` (and the
`alSyn*`->`Custom*` replacements) -- that line is the whole resolution. Then add `CustomInit = <bundled
vram>; // type:func` (recover-extern; overrides the wrong ghidra `al_init` via the `rom:` qualifier,
`#wrong-ghidra-name-override`) and the verbatim mirror compiles + matches. When the `[0x78330]`
bundled-synth subseg itself banks, name its fns by their libmus/player_fx identities (`CustomInit`,
`CustomSynNew`, `CustomAllocFX`, ...), distinct from the standalone `n_al*` -- per-region distinct
names, bank-stock-carry-custom. This is the audio-band specialization of the game-embedded duplicate
theme (the libmus/nualstl fns compiled into game audio TUs, no object boundary).

**Provenance:** S143 `aud_thread.c` `__MusIntAudManInit`: the lone `alInit(&__libmus_alglobals,
&syn_config)` call resolved verbatim to `CustomInit@0x8009CF30` via the two-layer chain; banked C
first build. Standalone `n_alInit@0x800A0730` confirmed dead (Ghidra `get_xrefs_to` = none).

---

## vendored-header-incomplete (a reconstructed header is `(already-vendored)` yet missing a macro)

**Rule:** The project's vendored internal headers (`include/libultra/internal/controller.h`,
`siint.h`, …) are reconstructed, not verbatim ultralib copies (see the
`//some version of this almost certainly existed` comment in `controller.h`). So a header can be
`(already-vendored)` (the file exists and its basename resolves on the `-I` set, so `needs-header`
stays silent) yet not define a function-like helper macro the upstream `.c` invokes, or define
it in an incompatible form (object-like when the source calls it `MACRO()`). The result is a
mid-execution compile error after the verbatim copy lands (`parse error before ')'` for a
function-like call on an object-like macro; an `undefined reference` / implicit-decl for a missing
one), invisible to the plan gate (the `INCLUDE_ASM` stub carries the original asm, no header
exercise). This is the reconstructed-header dual of `#stale-vendored-header` (there the file is a
stripped *revision*; here it is missing/mis-formed *macros*).

**Trigger:** not yet auto-flagged (a robust `needs-macro:<MACRO>@<hdr>` detector is a deferred
follow-up; distinguishing a function-like macro invocation from a real call needs preprocessing, so
a naive `UPPER(` grep false-fires). **Manual gate check for a mirror candidate:** grep the upstream
`.c` for the ALL-CAPS helper macros it invokes (`SELECT_BANK(`, `SET_ACTIVEBANK_TO_ZERO(`, `ERRCK(`,
…) and confirm each is `#define`d in a resolvable header with a compatible arity (function-like
`#define NAME(` for a `NAME(...)` call site). Compare against ultralib's gated definition.

**Procedure:** align the vendored header to the pinned version (`-DBUILD_VERSION=VERSION_J`): add the
missing macro / fix the arity verbatim from ultralib's version-active definition, keeping the
mirror `.c` verbatim. Guard the blast radius first: `grep -rn '<MACRO>' src/` to confirm no other
banked consumer depends on the old form (S88: `SET_ACTIVEBANK_TO_ZERO`/`SELECT_BANK` had zero other
consumers, so the form-change + addition were safe). Editing a shared vendored header → the banking
SHA-1 must come from a clean rebuild (`make clean && make extract && make`), not an incremental
build (`#clean-rebuild-after-shared-header-edit`).

**Provenance:** S88 (`io/contpfs.c`: vendored `controller.h` was missing `SELECT_BANK` entirely and
had an object-like `SET_ACTIVEBANK_TO_ZERO` vs the source's `SET_ACTIVEBANK_TO_ZERO()` →
5 parse errors; aligned both to VERSION_J, added `SELECT_BANK`, made the macro function-like, no
other consumers, clean-rebuild, banked seed-only first try). The `(already-vendored)` no-op tag
(S59) prices the *file's existence*, not its *macro completeness*; that gap is this hazard.

---

## crlf-vendored-header (a copied SDK header breaks KMC cpp's `\` continuations)

**Rule:** N64 SDK source ships with CRLF (DOS) line endings. A vendored header copied verbatim keeps
its CRLF, and KMC GCC 2.7.2's preprocessor treats a `\` at end-of-line as a line-continuation **only** when
the `\` is immediately followed by the newline. With CRLF the `\` is followed by `\r`, so **every**
multi-line macro continuation breaks: the `#define` ends early, its body's following lines fall through
to the C parser as code, and you get a silent cascade of `parse error` (reported at the macro-definition
lines, not the call site) plus `stray '\' in program`. The `.c` mirror itself can be fine; the bad file
is the included header. Plain single-line `#define`s are unaffected, so the first error lands at the
first `\`-continuation macro in the header.

**Trigger:** after vendoring an SDK-sourced header, the build throws a run of `parse error before <tok>`
at the header's macro-definition lines (often the first function-like macro) + `stray '\' in program`.
`file include/<lib>/<hdr>.h` reports `CRLF line terminators`.

**Procedure:** strip CR on copy — `sed -i 's/\r$//' include/<lib>/<hdr>.h` (or `dos2unix`). CRLF-clean
the header before relying on it. (Comments/whitespace are codegen-neutral, so this never affects the
ROM; it only un-breaks cpp.) This generalizes the `#upstream-mirror-pattern` CRLF note (which covered
nusys `.c` sources) to vendored headers. `include/libnualstl/nualstl.h` from nusys-2.05 had CRLF;
its `\`-continued seq-player macros cascaded into ~12 parse errors until stripped (S128). The git-managed
n64sdkmod headers (e.g. `libmus.h`) were already LF and fine, so suspect the SDK-extracted ones.

---

## stale-vendored-header

**Rule:** A vendored header can resolve as a *file* (so `needs-header` stays silent) yet be a stripped
revision whose *content* is missing constants the build expects; the gap is the content, not the
path. The specific case: the in-tree `include/libultra/PR/os_version.h` was the 2.0L release header,
which defines only `OS_MAJOR/MINOR_VERSION` and none of the `VERSION_D..L` ordinal constants the
ultralib gcc.mk profile uses. An upstream mirror with a `#if BUILD_VERSION < VERSION_K` guard then
sees `VERSION_K` (and the `-DBUILD_VERSION=VERSION_J` token) as undefined → cpp reads both as `0` →
the guard mis-evaluates and the guarded function silently vanishes into a link error, not a compile
error (S60: `gu/mtxcatl.c`'s `guMtxXFML` dropped, `undefined reference to guMtxXFML` at link).

**Trigger:** `pick_target.py` flag `stale-header:os_version.h(<VERSION_X>,…)`, fires when an upstream
file's `#if BUILD_VERSION <op> VERSION_X` references a token the os_version.h on the candidate's `-I`
set does not `#define` (and the lib actually sets `-DBUILD_VERSION`). Distinct from `needs-header`
(file absent) and `needs-define` (a non-version gating define).

**Procedure:**
- Add the missing `VERSION_*` constants to `include/libultra/PR/os_version.h`, verbatim from the
  ultralib gcc.mk `os_version.h` (`VERSION_D 1` … `VERSION_L 9`). This is a one-time additive
  header-content vendor enabler, not a `blk`.
- **Additive + SHA-safe by construction:** every *existing* in-tree guard compares against
  `VERSION_J` and the build sets `BUILD_VERSION=VERSION_J`, so `>= VERSION_J` stays true and
  `< VERSION_J` stays false whether the tokens are `0`-vs-`0` (stripped) or `7`-vs-`7` (vendored);
  only a `< VERSION_K`-style guard (the one that was wrong) flips. Leave `OS_MAJOR/MINOR_VERSION`
  untouched (the upstream form needs `BUILD_VERSION_STRING`, which is not defined here).
- **Verify with a clean rebuild** (see #clean-rebuild-after-shared-header-edit): the build has no
  header-dependency tracking, so a `make` after the edit only recompiles the file you touched; the
  other os_version.h consumers keep stale objects and a divergence would hide until the next clean.

**Provenance:** S60 (gu/mtxcatl.c: `guMtxXFML` under `#if BUILD_VERSION < VERSION_K`; the in-tree
os_version.h was the 2.0L revision; the `pick_target` `_strip_inactive_version_branches`/`build_ord`
machinery already existed for ref/call scanning, reused here for the detector).

**Sub-cases / variants:**

**partial-twin subset (`coddog-partial:<m>of<n>fn`).** coddog matches per-function; when its
corpus splits a combined source into per-fn files (`mtxidentf.c`, `mtxl2f.c`) and a multi-fn subseg's
fns match **only** some of them, the bare `coddog-mirror`/`coddog-twin` flags over-promise a clean
single-file mirror. `pick_target` flags `coddog-partial` when ≥2 **distinct** per-fn twin files matched
a pack covering fewer fns than it holds (`len(cod_members) < nfns`): the multi-twin companion to
`coddog-fncount-mismatch` (which fires only at `len(distinct)==1`). The un-matched fns are **not**
verified upstream → per-fn verify before mirroring. **Provenance:** S103 (`func_800660A0`:
`mtxidentf.c`+`mtxl2f.c` @100 matched only `guMtxIdentF`/`guMtxL2F`; the combined `mtxutil.c`'s
`guMtxF2L` (Monegi clamp variant) + `guMtxIdent` (-O2 non-inline) diverged from every available
upstream, so the planned verbatim cp failed first build, banked classical via #game-region-mirror--o2-profile).

---

## game-region mirror (-O2 profile)

**Rule:** A libultra/SDK source can be statically linked into the game (not the libultra code
band): its subseg sits at a low rom/vram, and the build compiles it with the game `CFLAGS` (-O2),
**not** `LIBULTRA_CFLAGS` (-O3 via `$(subst -O2,-O3,…)`). The build selects the profile by src path
prefix (the `mk/lib*.mk` `C_PROFILE_CFLAGS` overrides): only `src/libultra/%` and `src/libkmc/%`
get the lib profiles; everything else is -O2. So a
game-embedded SDK mirror placed under `src/libultra/` compiles at the wrong -O3 → `-finline-functions`
inlines its small callees (the ROM, at -O2, keeps the `jal`s). Symptom: a verbatim mirror where a
caller fn balloons (e.g. `guMtxIdent` 240 B vs ROM 60 B) because -O3 inlined `guMtxIdentF`/`guMtxF2L`.

**Trigger:** `pick_target.py` flag `game-region-mirror:0x<vram>`, fires when a row's `up_lib ==
"libultra"` and its rom is below the lowest-rom `libultra/` subseg (the libultra code band start).
Advisory (display-only).

**Procedure:**
- Mirror the file under a -O2 path, **not** `src/libultra/…`. The project convention is `src/mgu/`
  for the game-embedded ultralib gu/mgu matrix source (the Monegi variant); a local `.clang-format`
  (`BasedOnStyle: Google` + `SortIncludes: Never`) formats it like the other library trees (see
  CLAUDE.md). Any non-`libultra/`,
  non-`libkmc/` path gives -O2; pick one that reflects the source (`src/mgu/`, `src/main/…`).
- Includes: the game `-I` set lacks `src/libultra/gu` and `include/libultra/PR`, so a source-private
  companion (`guint.h`) won't resolve; include via the public path instead (`#include <ultra64.h>`
  pulls `<PR/gu.h>` → `FTOFIX32`/`FIX32TOF` + `<PR/gbi.h>` → `Mtx`).
- A divergent fn (a Monegi-modified variant absent from upstream) is then a normal classical
  reconstruction on the -O2 file (the verbatim siblings stay byte-exact); see the float-literal note in
  #mirror-cast-divergence-sign--vs-zero-extend for the single-precision FP gotcha.

**`game-embedded` sub-case — not standalone-carvable.** A game-region mirror can be worse than
"compile at -O2": the upstream file's functions are compiled into a larger game TU, tight-packed (no
16-byte object padding) against game functions, so there is **no** object boundary to carve a standalone
mirror at. The natural end of the matched block lands on a non-16 address with real game code (not
nops) immediately after; a standalone `[..,c,..]` subseg there SHA-misses because KMC `as` 16-pads the
`.o` `.text` and shifts the tail (#non16align — the gate-build canary: a correctly-named stub that
SHA-misses is alignment, not a bad offset). `pick_target.py` flags this as `game-embedded:0x<vram>`
(the synthesis of `game-region-mirror` + a coddog-subset signal — `coddog-fncount-mismatch` /
`coddog-structural` / `coddog-partial` — i.e. only some of the subseg's fns match the upstream).
- **Plan a mixed carve, not a seed-only mirror.** Carve `[A, B)` where **both** A and B are 16-aligned
  function boundaries (a 16-aligned carve always matches byte-wise, no padding inserted, regardless of
  the true object boundary). The carve pulls in the adjacent game fns between the lib block and the next
  16-aligned boundary; write the lib fns as verbatim mirrors and the game fns classical (or carry them
  `INCLUDE_ASM`). Put the file under a game path (`src/main/…`), since it holds game functions.
- **Find the boundaries from the asm:** the matched block's natural end is usually non-16; scan forward
  to the next function start that is `vram % 16 == 0`. S128 audio_mgr.c: nualstl3's 4 nuAuStl* fns end
  at 0x8005F048 (non-16, real bgm code follows) → extend to the next 16-aligned fn `bgm_load_song_from_rom`
  @0x8005F090, carving `[0x3A1D0..0x3A490)` = 4 mirror + 2 game bgm fns, all 16-aligned, ROM SHA match.

**Provenance:** S103 (`func_800660A0`'s gu mtxutil tail @0x80067B00, inside a game pack: the 4 fns
`guMtxF2L`/`guMtxL2F`/`guMtxIdentF`/`guMtxIdent` banked at `src/mgu/mtxutil.c` -O2; the initial
`src/libultra/gu/mtxutil.c` -O3 placement inlined `guMtxIdent` and never matched).

**Game-embedded gu-variant is NOT byte-identical to stock ultralib (verify against the BUILD .o, not
just the source) (S188).** A main-segment FP-math fn that STRUCTURALLY matches an ultralib gu fn is
often a game-recompiled *variant*, not a verbatim copy. Cross-check the ultralib BUILD object
(`~/development/repos/ultralib/build/J/libgultra_rom/src/**/*.o`, disassembled), not only the `.c`:
S188's `convert_and_pack_floats_to_fixed` @0x80065DD8 is `gu/mtxutil.c guMtxF2L`, but ultralib's OWN
VERSION_J build of `guMtxF2L` uses `<`/`slti` loop bounds and keeps the redundant `& 0xffff0000`,
while the MG64 copy uses `!=` (held-const `bne`) and DROPS the redundant mask (the `sll 16` already
zeroes the low half). So the match needed: upstream source + `!=` loop bounds + dropped-redundant-mask
(3 game-divergence levers), NOT a verbatim `cp`. **Naming:** a SECOND embedded copy of a gu fn keeps
its descriptive Ghidra name (the canonical SDK name is already taken by the first copy: `guMtxF2L`
lives at 0x80067B00, so 0x80065DD8 stays `convert_and_pack_floats_to_fixed`).

**m2c-with-Mtx4f context + the 2D-index combine_givs de-bias (S188).** Seed a game gu/mgu math pack
with the standard combined seed, writing the `ghidra_ctx.c` as `common.h` + the `Mtx4f`=`float[4][4]`
typedef from the Ghidra DB + the loose camera globals. Ghidra typically has NO camera/matrix struct, just
loose float globals + the `Mtx4f` typedef; the typedef is the lever. A 4x4-matrix fn m2c-seeded as a
`void*`/`f32*` walking-pointer form triggers loop.c `combine_givs` base-bias (a biased base reg + neg
offsets); rewriting with `f32 m[4][4]` params and 2D `a[i][k]`/`b[k][j]` indexing produces the ROM's
direct-immediate-offset form (base + k*rowsize + j*4). S188 cracked the in-place matmul
`func_80065D5C` this way (see also [#indexed-vs-pointer loop](#indexed-vs-pointer-loop-strength-reduction)).

---

## clean-rebuild-after-shared-header-edit

**Rule:** The build tracks no header dependencies: a `make` recompiles only the `.c` files whose
objects are missing or older than the source, not the consumers of a header you edited. So when a
mirror/enabler edits a *shared vendored header* (e.g. `include/libultra/PR/os_version.h`, included
transitively by many libultra TUs), the post-edit verification **must** be a clean rebuild, or a stale
object can mask a divergence the header change introduced elsewhere.

**Trigger:** the finalize/enabler step modifies a header under `include/` that more than one already-
banked TU includes (directly or via `guint.h`/`gu.h`/etc.).

**Procedure:**
- `make clean && make extract && make`, then `sha1sum build/mariogolf64.z64` == baserom. The clean
  rebuild recompiles every consumer against the new header; only then is the SHA-1 a real guard.
- A plain incremental `make` is fine while iterating the *mirror file itself* (rm its stale `.o` if
  the header change doesn't retrigger it), but the banking SHA-1 must come from the clean build.

**Provenance:** S60 (the os_version.h VERSION_* add: incremental `make` left `guMtxXFML` undefined
until the touched object was removed; a `make clean` rebuild then confirmed the add was globally
SHA-safe across all os_version.h consumers).

### shared-callee RENAME

The same no-header-dep-tracking gap bites a different way when a bank renames a shared callee/global
in `symbol_addrs.txt` (a recover-extern), not just edits a header. Renaming e.g. `func_800A1148` to
`__n_allocParam` and re-running `make extract` regenerates every `.s` that references it to the new
name, but `make` does **not** recompile the still-`INCLUDE_ASM` sibling `.c` files (their `.c` did not
change, and the build tracks no dep on the `.s` the macro includes). So their stale `.o` still
references the old `func_<vram>` name, and the link fails `undefined reference to func_<vram>` even
though the regenerated `.s` is correct.

**Trigger:** banking one member of a homogeneous sibling set that share a recovered callee/global,
while the other members are still `INCLUDE_ASM` stubs; link error names the pre-rename `func_<vram>`/
`D_<vram>`.

**Procedure:** for a homogeneous sibling set sharing recovered externs, write **all** the sibling bodies
before building — each becomes a fresh-compiled `.c` referencing the new name, so no stale stub `.o`
survives. Otherwise `rm` (or `touch` the source of) the still-stub `.o` after the rename to force its
recompile. The shared extern only needs recovering once (it serves the whole set).

**Provenance:** S129 (n_synsetpan recovered `__n_allocParam`/`n_alEnvmixerParam`/`n_syn`; the
n_synsetpitch/startvoice/stopvoice stubs link-failed on stale `func_800A1148` until all 4 bodies were
written and the set rebuilt together).

---

## needs-define

**Rule:** A function guarded by a preprocessor define not active for its lib won't compile into the
build as written.

**Trigger:** `pick_target.py` flag `needs-define:<def>`.

**Procedure:** Confirm the gating define against the upstream. If it is not in the lib's active
define set, the leaf is not a clean flip: defer, or handle the define at the gate.

### N_MICRO library-wide pin

The n_audio_sc upstream Makefile builds the whole library with `-DN_MICRO=1` (the "micro" command
stream). Files that branch on it (`n_save.c`, `n_resample.c`, `n_reverb.c`, `n_env.c`, `n_load.c`,
`n_synthesizer.c`) emit the micro path; with **no** define they take the longer non-micro path and
SHA-miss. The fix is a library-wide pin: `-DN_MICRO=1` in `LIBNAUDIO_CFLAGS` (`mk/libnaudio.mk`), the
same standing-pin pattern as `-DF3DEX_GBI_2`. The setter mirrors (`n_syn*.c`) have no N_MICRO branch,
so the pin is byte-neutral for them. `pick_target` parses `LIBNAUDIO_CFLAGS` into the libnaudio
active-define set, so N_MICRO reads as satisfied and a libnaudio mirror does not false-flag
`needs-define:N_MICRO`. The first miss surfaced at the full-make ROM SHA-1 (`n_save.c` built the
4-command non-micro path vs the target's 2 commands), invisible to the gate stub — the same
late-surface class as the GBI sub-case below. See `#needs-header` for the paired `.inc.c`
body-include the N_MICRO branch pulls in.

**Provenance:** S133 (n_audio_sc `-DN_MICRO=1` library-wide pin; parsed into libnaudio's
active-define set).

### GBI-microcode define

A subtler sub-case: not a `#ifdef DEFINE` wrapping the whole function, but an object-like macro
whose value is GBI-microcode-guarded, used inside an otherwise-verbatim body. `PR/sptask.h`:

```c
#if (defined(F3DEX_GBI)||defined(F3DLP_GBI)||defined(F3DEX_GBI_2))
#define OS_YIELD_DATA_SIZE 0xc00
#else
#define OS_YIELD_DATA_SIZE 0x900
#endif
```

ultralib builds `libgultra_rom` with a global default `GBIDEFINE := -DF3DEX_GBI`. MG64 runs the
F3DEX2 microcode, so the project pins `-DF3DEX_GBI_2` in `LIBULTRA_CFLAGS` (`mk/libultra.mk`). With **no**
GBI define the macro silently takes the `#else` value. The standing pin resolves this for every
libultra mirror; it is documented here because the failure mode is invisible to every gate check.

**The tell (`sptask.c`):** the mirror compiles and links clean (all symbols resolve), but the
full-make ROM SHA-1 misses by exactly one word: an `addiu rX, rX, imm` whose immediate is off by
the macro delta (built `0x8FC` vs baserom `0xBFC`, i.e. `OS_YIELD_DATA_SIZE - 4` at `0x900` vs
`0xc00`). It is invisible to the gate because the `INCLUDE_ASM` stub never compiles the body (same
late-surface class as #needs-define USE_EPI, the cross-lib-header S40, the macro-hidden-extern S41).

**Localize a linked-but-SHA-missed mirror (S44 `.o`-diff, byte form):** diff the subseg bytes
between `baserom.z64` and `build/mariogolf64.z64` and group differing bytes into 4-byte instruction
words (`dd` is blocked, so use a venv python read of both files at the ROM offset). A single differing
word with an off-by-constant immediate points straight at a header-macro / enum value divergence; a
whole-body divergence is a different (version / cast / char-signedness) class.

**Pre-flag:** `pick_target.py` flags `needs-define:<GBI def>` when a candidate's body uses a
GBI-value-guarded macro and no guard define is active for the lib (`gbi_value_guard_needs_define`,
keyed off the parsed `LIBULTRA_CFLAGS` define set). Dormant while `-DF3DEX_GBI_2` stands, by design.

**Provenance:** S83 (`sptask.c`'s `OS_YIELD_DATA_SIZE` immediate proved the standing `-DF3DEX_GBI_2`
pin over the `#else` value).

### Version-rev `#define` value divergence

The same link-clean / one-word-SHA-miss class as the S83 GBI sub-case, but the gating is **not** a build
flag: it is the vendored upstream version diverging from the game's library rev on a plain object-like
`#define` value. A byte-verbatim drop-static `nusimgr.c` (libnusys) mirror SHA-missed by exactly
one byte, vram 0x800A27E0 `li a1,6` (build) vs `5` (ROM) — the `osCreateThread` thread-id arg. Root
cause: vendored nusys-2.07 `include/libnusys/nusys.h` has `NU_CONT_THREAD_ID 6`, but MG64's nusys rev
compacts the controller/SI thread to slot 5 (the IDs run idle=1/rmon=2/main=3/gfx=4/[audio-gap=5]/
cont=6 in 2.07; MG64 drops the audio slot so cont=5).

**The tell vs a reloc:** the lone differing word is a `li`/`addiu` whose immediate is a small literal
(an enum/macro value), **not** a `lui`+`addiu`/`lw` hi/lo pair carrying a relocated address. An immediate
mismatch is a `#define`/enum value; a hi/lo address mismatch is a symbol-placement problem. Localize
with the S44 `.o`-diff byte form (venv-python region cmp of `baserom.z64` vs `build/mariogolf64.z64`
at the ROM offset) → the single off-by-constant immediate → grep the macro back through the header.

**Fix + blast radius:** correct the vendored header to the game's value, then **grep `src/` +
`include/` for every consumer of the macro before editing** — if the candidate is the sole consumer
(`nusimgr.c` was the only `NU_CONT_THREAD_ID`/`NU_SI_THREAD_ID` user in the tree), the change is
collateral-free; otherwise a co-consumer that was matched at the old value will break. The build tracks
no header deps, so bank from a `make clean && make extract && make` (the shared-header-edit rule), not
an incremental build.

**Provenance:** S122 (`nusimgr.c` drop-static mirror; `NU_CONT_THREAD_ID` 6→5 vendored-header value
fix for MG64's compacted nusys rev).

### VERSION_K-gated statement present in MG64's J build

The ultralib reconstruction's `#if BUILD_VERSION >= VERSION_K` gates are sometimes too aggressive
for MG64's actual VERSION_J build: a call/statement the source gates K-only is in fact present in the
J ROM. `pick_target.py`'s `_strip_inactive_version_branches` strips `>= VERSION_K` under J, so the
mirror compiles short of the asm by whole instructions.

**`initialize.c`, two instances in one file (verify each independently):**
- `__osSetWatchLo(0x4900000)` was gated `#if BUILD_VERSION >= VERSION_K`; un-gating it to
  `>= VERSION_J` restored the missing `jal __osSetWatchLo; lui a0,0x490`, the exact 8-byte/2-instr
  miss.
- `createSpeedParam`'s body, by contrast, has a `#elif BUILD_VERSION == VERSION_J` branch
  (reconstruction lines 210-224) so it did compile under J as-is. Check the J branch exists before
  assuming a K-gate needs un-gating; not every K-gated thing is missing under J.

**The tell:** the mirror compiles + links clean, but the full-make ROM SHA-1 misses by an exact
multiple of 8 bytes (whole instructions) localized to one contiguous region, distinct from the
GBI/enum off-by-constant *immediate* class above. Cross-check the `.o` function size against the asm:
the next symbol's `nm` offset vs the asm `nonmatching <fn>, 0x<size>` comment (`__osInitialize_common`
compiled `0x228` vs baserom `0x230`).

**Localize:** disassemble the `.o` (`mips-linux-gnu-objdump -d build/<path>.o`) and the baserom fn
(Ghidra MCP `disassemble_function`), align; the divergence is a contiguous run of asm-only
instructions matching a `>= VERSION_K`-gated statement in the upstream (the 8-byte shift then
propagates cleanly through the rest).

**Fix:** change only that one gate `>= VERSION_K` → `>= VERSION_J` in the mirrored copy (a
near-verbatim version-gate edit, same class as #char-signedness / #assert-strip; confirm the gated
callee is a placed symbol). **Not** a blanket un-gate of every K-block.

**Provenance:** S85 (`initialize.c`'s `__osSetWatchLo` K-gate un-gated to `>= VERSION_J`; the exact
N×8B whole-instruction SHA-miss tell).

---

## header-renames-symbol (vendored header rewrites the curated symbol)

**Rule:** A (transitively-)vendored libultra header can rewrite the candidate's curated function name
via a source-compat macro. `os_host.h`: `#define __osInitialize_common() osInitialize()` (the K-era
worker name → the J public name). When the mirror body defines `void __osInitialize_common() {...}`,
the function-like macro fires and the object exports `osInitialize` instead, leaving the curated symbol
the entry stub / callers reference (`__osInitialize_common`) undefined at link. This is a recurring
class, not a one-off.

**Trigger:** `pick_target.py` flag `header-renames-symbol:<fn>@<header>` (scans the candidate's
transitively-included resolvable headers for `#define <curated_leader>...`), or a link
`undefined reference to <curated_fn>` where the `.o` `nm` shows the function exported under a
**different** name. Invisible to the gate stub build: the macro bites only a real function definition,
which an `INCLUDE_ASM` stub never has (same late-surface class as #needs-define).

**Procedure:** Add `#undef <curated_fn>` to the mirrored `.c` after the `#include`s (so the
transitive header's macro is undone) and before the function definition; then the
name-macro/`INITIALIZE_FUNC` reconcile exports the curated symbol. SHA-neutral (a symbol-name change,
not a byte change). Same one-line fix as the `#undef nuGfxInit`.

**Caveats:**

**Not this hazard (the contrast case):** if the upstream does not define a function named
`<curated_fn>` (the curated name is a macro alias for a **different** symbol the upstream defines,
namely the macro's RHS), then the body never contains the `<curated_fn>` token, no `#undef` is needed,
and the real issue is a mislabeled ghidra name → see `#wrong-ghidra-name-override`. `pick_target.py`
now suppresses `header-renames-symbol` in that case (`_upstream_defines_function` gate) and emits
`wrong-ghidra-name` instead.

**Provenance:** first instance: S31 (`nuGfxInit`, nusys.h); second instance + `pick_target.py` flag:
S85 (`initialize.c`); the `_upstream_defines_function` contrast/suppression case: S102.

---

## wrong-ghidra-name-override (correct a mislabeled symbol without sync-names)

**Rule:** `ghidra_symbols.txt` can name a function vram with the **wrong** symbol: a macro alias rather
than the real function. motor.c: ghidra labels 0x800AE380 `osMotorStop`, but os_motor.h
`#define osMotorStop(x) __osMotorAccess((x), MOTOR_STOP)` makes that a *macro*; the function the
VERSION_J build defines at 0x800AE380 is `__osMotorAccess` (verified in `build/J/libgultra_rom/motor.o`:
`T __osMotorAccess`@0, no `osMotorStop` symbol). A verbatim mirror names the body `__osMotorAccess`
(correct), so the still-asm callers' relocs (`jal osMotorStop`) and the C object (`__osMotorAccess`)
disagree → `undefined reference`.

**Why not `make sync-names`:** the canonical fix (rename in Ghidra → sync) is a full
`--export-to-decomp --write-in-place` regen, still destructive (~250-symbol Ghidra↔decomp drift).
And you can't hand-edit `ghidra_symbols.txt` (sync-owned) nor naively add the correct name
to `symbol_addrs.txt` (same vram already in ghidra_symbols → splat dup error).

**Trigger:** `pick_target.py` flag `wrong-ghidra-name:<ghidra_name>-><correct_name>@<header>`
fires when a header macro `#define <ghidra_name>(...)` exists, the version-stripped upstream does not
define a function named `<ghidra_name>`, and the macro's RHS leading symbol is defined in the upstream.
It is the distinguishing companion of `#header-renames-symbol` (which fires when the body does define
the macro name → a real `#undef`).

**The override mechanism is not limited to macro aliases.** `pick_target.py` only auto-flags the
macro-alias case, but the `rom:`-qualifier override below corrects any wrong `ghidra_symbols.txt` name a
coddog/upstream cross-ref proves wrong — a plain bad guess, not just a macro. audio_mgr.c (S128):
`audio_config_init`@0x8005EDD0 was really `nuAuStlMgrInit`, and the libmus callees `mus_initialize`@
0x8009973C / `mus_play_song_ptr`@0x800999F0 were really `MusInitialize` / `MusStartSong` (coddog
libmus_map confirmed); the RSP-boot sprint likewise corrected `audio_sched_thread_entry`. In each the
ghidra name is a non-canonical guess, the upstream name is canonical, and the override resolves both the
mirror body and every still-asm caller to the correct name. Treat a coddog/upstream-confirmed wrong name
the same as a macro alias even though no `pick_target.py` flag fires.

**Procedure (the non-destructive override):**
1. Add a `symbol_addrs.txt` maintainer-override for the correct name with a `rom:` qualifier:
   `<correct_name> = 0x<vram>; // rom:0x<off> type:func`. The `rom:` (or `segment:`) qualifier is
   load-bearing: splat's dup-symbol error (`util/symbols.py`, the `have_same_rom_addresses and
   same_segment` test) fires only when a same-vram symbol shares both rom and segment; a bare entry has
   rom=None (== the ghidra entry's None) + same segment → clash. The qualifier makes
   `have_same_rom_addresses` False → no clash.
2. `symbol_addrs.txt` is loaded first (splat `initialize`, before `ghidra_symbols.txt`), so the
   override wins the reference: both the scaffolded `INCLUDE_ASM` stub and the still-asm callers' relocs
   resolve to `<correct_name>`. (The link also runs `--allow-multiple-definition` as a safety net.)
3. Name the mirror body `<correct_name>` (verbatim, since it is the real upstream name). No `#undef` needed:
   the body never contains the macro-name token, so the alias macro is inert (this is why
   `#header-renames-symbol` does not apply: the curated name is the macro's RHS).
4. The stale `ghidra_symbols.txt` entry + the override coexist deliberately; the override wins.
   **Cross-repo follow-up:** rename the vram in the Ghidra workspace to `<correct_name>` so the
   source-of-truth matches; a future reconciled sync can drop the override.

**A callee the new C body calls can need the same override.** The override is not only for the
function being banked. A verbatim/classical body that calls a still-asm sibling by its vendored-header
upstream name needs a `rom:` override for the callee too when `ghidra_symbols.txt` labels that vram with
a different name. `aud_thread.c` (S144) called `__MusIntDmaProcess` (the `aud_dma.h` name); ghidra had
`mus_dma_process`@0x8009DA8C, so the C linked with `undefined reference to __MusIntDmaProcess`. **The
tell is a link-time `undefined reference` to an upstream callee name, not a gate-time scaffold mismatch**
— the gate `INCLUDE_ASM` stub referenced the ghidra name and built green, so it surfaces only when the
new C object links. `pick_target.py` does not flag this (the callee lives in a different still-asm TU);
read the new body's vendored-header callees against `ghidra_symbols.txt` before building, or let the
link error surface it, then add `<upstream> = 0x<vram>; // rom:0x<off> type:func` (same mechanism).

**Aliasing an already-placed `symbol_addrs` name at the same vram → reference it, do not duplicate.**
Distinct from the ghidra-clash case above: when the name your C body needs is an alias of a
symbol already placed in `symbol_addrs.txt` at the same vram, do not add a second `symbol_addrs` entry —
`symbol_addrs` has no duplicate-vram support (`grep -oE '= 0x[0-9A-Fa-f]+;' symbol_addrs.txt | sort |
uniq -d` is empty project-wide; a 2nd entry at one vram risks dropping the canonical name's link).
Instead reference the placed name via a local `#undef`/`#define` (or an `extern` decl) in the C.
`aud_thread.c`: `MICROCODE_CODE` is `n_aspMainTextStart`@0x800B3F20, but that vram is already placed as
the canonical boundary name `rspbootTextEnd` (rspboot text end == aspMain text start), so the C did
`extern long long int rspbootTextEnd[]; #undef MICROCODE_CODE; #define MICROCODE_CODE rspbootTextEnd`.
(The `rom:` override above is for a symbol_addrs↔ghidra clash; this is a symbol_addrs↔symbol_addrs alias,
which the override does not solve.)

**Verify at the gate:** after the flip + override, `make extract`, then confirm (a) the scaffolded
`src/.../<file>.c` stub is `INCLUDE_ASM(..., <correct_name>)` and (b) the still-asm caller's
`asm/<seg>.s` relocs `jal <correct_name>`; both prove the override won before you write the body.

**Provenance:** the mislabeled-symbol class + the `wrong-ghidra-name` flag: S102 (`motor.c`); the
destructive full-regen drift count: S20/S87; the same-vram alias → reference-not-duplicate variant:
S144 (`aud_thread.c`, `MICROCODE_CODE`).

---

## make sync-names eviction recovery

**Rule:** Running `make sync-names` mid-sprint can evict symbols from `ghidra_symbols.txt` (Ghidra
lacks a curated name → it vanishes; or a function was renamed → old spelling disappears). In-flight C
referencing the old symbol then fails to link. Prevention: `make sync-names` is gate-only.

**Trigger:** `make` fails with `undefined reference to <sym>` for a symbol that was in
`ghidra_symbols.txt` before the sprint, and `make sync-names` ran mid-sprint.

**Procedure:** Add the missing symbol to `symbol_addrs.txt` as a data extern
(`<name> = 0x<ADDR>; // size:0x<n>`, add-only, disjoint), then `make extract && make`. This restores
it via the splat-side file.

---

## stale top-level asm label sync

**Rule:** After a gate rename or a `symbol_addrs.txt` add, `make extract` regenerates the
per-function stub in `asm/nonmatchings/...` but does not update the stale top-level `asm/<seg>.s`
(no longer regenerated once the subseg is c-flipped). `decomp_loop.py` uses the top-level file, so a
stale label breaks loop resolution or masks the real score with a reloc-name mismatch.

**Trigger:** Loop reports "no `glabel <newname>` found", or asm-differ penalises every load/store to
a renamed global.

**Procedure:**
- **Function rename** (e.g. `__osPiRelAcces` → `__osPiRelAccess`): update the old label(s) in the
  stale `asm/<seg>.s` (3 occurrences: `nonmatching`, `glabel`, `endlabel`), then rebuild the
  reference object (`make build/asm/<seg>.o`).
- **Data global add** (e.g. `hdwrBugFlag = 0x800C7EC0`): replace all `D_<ADDR>` occurrences in
  `asm/<seg>.s` with the new name, then rebuild (`asm/7EFE0.s`, 3× `D_800C7EC0` → `hdwrBugFlag`).

**Provenance:** data-global worked example: S34 (`asm/7EFE0.s`).

---

## stale parent asm relic (find_segment mis-resolution after a decompose-split)

**Rule:** A subseg SPLIT (decomposing `[0x<A>, asm]` into `[0x<A>, c, …]` + `[0x<B>, c, …]`) leaves the
**pre-split** top-level `asm/<A>.s` relic on disk -- the multi-function file covering the WHOLE original
range. `make extract` writes the new per-fn ground truth to `asm/nonmatchings/<tree>/<fn>/` but does NOT
delete or regenerate that relic (verified: its mtime is unchanged across extract). Both the parent relic
AND the correct child `asm/<B>.s` then declare `glabel <child_fn>`, and `dc.find_segment` globs `asm/*.s`
**sorted**, returning the FIRST match -- so a child at a numerically larger offset (`4C620.s`) loses to the
stale parent (`4C3D0.s`), and `decomp_loop.py` builds a MULTI-function reference object. asm-differ then
mis-aligns the 1-fn candidate against the N-fn reference and reports a bogus near-match (`base_text=""`,
`match_count == total_rows`, a large score) -- NOT the `#isolated-compile-caveat` artifact (there the
rows carry real text; here the reference rows are empty because it is the wrong, longer object).

**Trigger:** `decomp_loop` on a just-split classical fn reports a high score whose top mismatches are all
current-only (`>`) rows with EMPTY `base_text`, and the JSON `reference_path` names the PARENT segment
(`build/asm/<A>.o`), not the fn's own child segment. Objdump the reference
(`mips-linux-gnu-objdump -d build/asm/<A>.o`) and it holds several functions, not one.

**By-hand fix (until the tooling fix lands):** move the stale parent relic out of `asm/` so find_segment
resolves to the correct 1-fn child: `mv asm/<A>.s <scratch>/ && rm -f build/asm/<A>.o`, then re-run
`decomp_loop`. The relic is a gitignored regen artifact, not used by `INCLUDE_ASM` (that reads
`asm/nonmatchings/…`) or the full build, so removing it is safe and durable (extract does not recreate
it). This recurs on EVERY classical-endgame decompose-split; the tooling fix (prefer the
`asm/nonmatchings/<tree>/<fn>/<fn>.s` target as the reference, or skip a seg file whose glabel set spans a
now-`c` sibling) is a golden-gated `tools/` branch item (see BACKLOG).

**Also fires on a WHOLE-PACK flip, not just a decompose-split (S186).** Flipping `[0x<A>, asm]` →
`[0x<A>, c, <tree>/<file>]` (no split) leaves the pre-flip `asm/<A>.s` relic AND any older enclosing
relic (S186: BOTH `asm/440A0.s` and `asm/43810.s` — the latter a stale wider-range relic from the S166
`lz_decompress_simple` carve — shadow-declared the pack's fns). Worse, for a whole-pack flip the per-fn
ground truth lives ONLY in `asm/nonmatchings/<tree>/<file>/<fn>.s`, so the seg-object reference model
does not fit AT ALL (there is no correct child `asm/<B>.s` to resolve to). Two options: (1) move every
shadowing relic aside (`mv asm/<X>.s <scratch>/ && rm -f build/asm/<X>.o`) — safe/gitignored/durable —
but `decomp_loop` still can't build a 1-fn reference for a whole-pack flip; (2) skip `decomp_loop` for
whole-pack-flip fns and diff the FULL build directly: `mips-linux-gnu-objdump -d
build/src/<tree>/<file>.o` for your fn vs `asm/nonmatchings/<tree>/<file>/<fn>.s`. S186 used (2). This
strengthens the tracked tooling fix: prefer `asm/nonmatchings/<tree>/<fn>/<fn>.s` as the reference (and
assemble a 1-fn reference object from it) instead of the whole-seg `build/asm/<seg>.o`.

**Provenance:** S168 `func_80071220` (split from `[0x4C3D0]` at S167): the stale `asm/4C3D0.s` (4 original
funcs) shadowed the correct `asm/4C620.s` (1 fn), giving a false 94/100 with an all-empty `base_text`.
S186 `func_80069BCC` (whole-pack flip of `[0x440A0]`): `asm/440A0.s` + `asm/43810.s` both shadowed;
fell back to full-build objdump diff.

---

## caller-evict

**Rule:** Adding a curated name for an un-named `func_<vram>` to `symbol_addrs.txt` (a common gate
enabler when naming a coddog / upstream-mirror leader) makes `make extract` rename that symbol in the
scaffold. Any already-banked C file that hard-codes the old `func_<vram>` name (an `extern` decl +
call) then fails to link with `undefined reference to func_<vram>`. Same class as
`#stale-top-level-asm-label-sync`, but reaching the gate via a symbol add rather than `make sync-names`.

**Pre-flag:** `pick_target.py` flags `caller-evict:<func_vram>@<file>[;…]`; it walks `src/`
for every un-named member a banked C file references by name (INCLUDE_ASM stub lines excluded). When
the flip will name that `func_`, the listed caller's call site must be renamed in the same flip.
Display-only (does not change `pts`); the fixup is one line and SHA-neutral (same address).

**Trigger:** the gate's green-ROM `make extract && make` fails with `undefined reference to
func_<vram>` from a banked C object after a `symbol_addrs.txt` add.

**Procedure:** rename the call site(s) in the flagged banked C file from `func_<vram>` to the curated
name (both the `extern` declaration and the call). The codegen is identical (same `jal` target), so
the ROM SHA-1 is unchanged. Adding `__osSpGetStatus`=0x800B16A0 evicted `src/main/func_800AB600.c`
(`extern u32 func_800B16A0(void)` + `func_800B16A0()` → `__osSpGetStatus`).

**Sub-cases / variants:**

**Companion case A: multi-global mirror flip evicts still-asm callers.** The dual direction:
flipping a mirror whose source defines ≥2 global functions makes the C object export those globals
under their real names (osCreateScheduler, osScAddClient, …). Any still-asm file that called them by
the old `func_<vram>` name then link-fails (`undefined reference to func_<vram>`): the asm reloc
points at a name nothing defines. `pick_target`'s `caller-evict` only walks banked C callers, so an
asm caller is invisible at the gate; it surfaces as the execution-time link error. Fix: add each
externally-referenced global's curated name to `symbol_addrs.txt`; `make extract` re-extracts the asm
caller with the new name and it resolves against the mirror's def. sched.c: still-asm mus_dma
(`asm/78D10.s`) called `func_800AB798`/`func_800AB880` → named `osScAddClient`=0x800AB798 +
`osScGetCmdQ`=0x800AB880 (the file's other globals, osScRemoveClient/__scTaskReady, had no external
caller → no add needed). Name only the globals the link error names.

**Companion case B: a mirror's recover-callee is an already-banked `func_`.** A new mirror
calls a libultra fn by name (`osSpTaskYielded`); the recover resolves not to still-asm but to a
function already banked classically under its `func_<vram>` placeholder (`func_800AB600`, banked S11
as a "main" leaf: it was the un-named `osSpTaskYielded` all along, revealed by the mirror's call).
Fix: rename the banked func_ to the libultra name (symbol_addrs add + the C body's function name) and
match the header signature (`OSYieldResult osSpTaskYielded(OSTask *tp)` from sptask.h). Critically,
keep the verified classical body, not the upstream-verbatim form: the S11 match used
`bit = (status>>8)&1` and the upstream uses `(status & SP_STATUS_YIELDED) ? OS_TASK_YIELDED : 0`,
which can codegen differently (`srl;andi` vs `andi;sltu`) at the game `-O2` profile. Do not relocate
the file into `src/libultra/` (would force the `-O3` band → possible divergence; see
`#game-region-mirror--o2-profile`); leave it where it banked (the file name stays `func_<vram>.c`, a
cosmetic mismatch, optional follow-up rename).

**Provenance:** the `caller-evict` flag + the banked-C worked example: S77 (`func_800AB600.c`,
`__osSpGetStatus`); both companion cases (still-asm caller, recover-callee): S106.

---

## Display lists

**Rule:** MG64 uses **F3DEX2** microcode (gspF3DEX2.fifo 2.08). Include `<PR/gbi.h>` when the target
manipulates `Gfx*`. **A `src/main/` (or `src/overlay_*/`) game TU that builds display lists needs the
F3DEX2 build profile** (`mk/main.mk`: `MAIN_CFLAGS = $(CFLAGS) -DF3DEX_GBI_2`) — `-DF3DEX_GBI_2`
selects the F3DEX2 GBI opcodes in `PR/gbi.h` (`G_RDPHALF_1=0xE1`/`G_RDPHALF_2=0xF1`, vs the F3DEX
`0xB4`/`0xB3`). Without it the RSP-command opcodes are wrong; the RDP commands (PipeSync/SetCombine/
SetOtherMode) are ucode-independent so they match either way. This updates the "main/ needs zero
mk edits" convention: the F3DEX2 profile is a standing enabler for any main-segment DL TU. **Caveat:**
`decomp_loop.py`'s default compile omits `-DF3DEX_GBI_2`, so a **macro-based** DL seed's isolated RSP
opcodes read `0xB4`/`0xB3` (a false diff, looks like a body bug). Pass **`--profile main`** (the
now-live main/F3DEX2 profile, alongside its libkmc/libultra detectors) for any macro-based main/ DL
fn; that adds `-DF3DEX_GBI_2` so the gDP macros expand to the F3DEX2 `0xE2..`/`0xE1`/`0xF1` opcodes.
**Raw-literal DL words are define-independent** (a `gfx->words.w0 = 0xE7000000` store is the same byte
value under either ucode), so a raw-word seed needs no profile — only macro seeds do. `setup-permuter.sh
--main` / `permuter_settings_main.toml` carry the same define for the permuter (S190).

**Procedure (static dlists in rodata):** run `~/development/repos/n64-tools/src/gfxdis-rom/gfxdis`
(build once with `make -C ~/development/repos/n64-tools`). `gfxdis` only handles static dlists.

**Procedure (dynamic builders — first main-seg DL TU).** A dynamic builder writes command words
into a running `Gfx*` cursor. Decode + reconstruct, do not hand-transcribe:
- **Decode the command words** with `gfxdis.f3dex2` (`~/development/repos/n64-tools/src/gfxdis/`; the
  F3DEX2 variant, not gfxdis.f3d/f3db/f3dex): `gfxdis.f3dex2 -x -w <concatenated-hex-words>` prints the
  `gsDP*`/`gsSP*` macros. `extract_dlist.py` parses `sw`-immediate stores and folds primitive
  sequences. gfxdis does not fold the higher-level texture-load composites
  (LoadTextureBlock/Tile/TLUT/MultiBlock) — scan the decoded opcodes for the load primitives (SETTIMG
  0xFD / SETTILE 0xF5 / LOADBLOCK 0xF3 / LOADTILE 0xF4 / LOADTLUT 0xF0 / SETTILESIZE 0xF2) to know if a
  higher-level macro must be reconstructed by hand (helper: `dl_fold_check.py`, tracked for promotion
  to `tools/`).
- **Write it with the stock dynamic GBI macros**, matching the game's idiom (see the n64demos
  `~/development/n64/n64demos` tile_rect2d + kantan-demos `.../kantan/2d/src/main/2d.c`): cache
  `Gfx *gfx = *glistp;` (or use the global cursor `glistp++` directly), emit `gDPPipeSync(gfx++)`,
  `gDPSetCycleType(gfx++, …)`, `gDPSetPrimColor(gfx++, …)`, `gSPTextureRectangle(gfx++, ulx, uly, lrx,
  lry, tile, s, t, dsdx, dtdy)`, then `*glistp = gfx;`. A per-entry helper takes `Gfx **glistp` (the
  demo signature). The global DL cursor's idiomatic decomp name is `glistp` (a `Gfx*`).
- **Mask-narrowing lesson (do not hand-inline a texrect on a mask-constant hunch).** A `andi 0xFFC` in
  the ROM where the stock `gSPTextureRectangle` macro masks `0xFFF` is not proof of a custom/inline
  texrect. GBI texrect coords are **10.2 fixed (12-bit)**, and the macro packs each via
  `_SHIFTL(_, _, 12)` = `& 0xFFF`. When the caller passes a `<<2` (pixel→10.2) coord, GCC knows the low
  2 bits are 0 and **narrows** the emitted mask: `((coord<<2) & 0xFFF) << 12` → `andi 0xFFC` on the
  shifted x-fields, while `(coord<<2) & 0xFFF` (un-shifted y-fields) stays `andi 0xFFF` — both matching
  the ROM. Test the stock `gSPTextureRectangle` before concluding "custom": the first attempt wrongly
  hand-inlined the texrect, then the stock macro proved byte-identical (S151). Coord locals are
  typically `u16` (per the kantan demo + the permuter's rediscovery); see
  `#permuter-setup-for-kmc-toolchain-mirrors` for the coord-width regalloc lever.

**Dynamic-builder post-increment idiom + composite folding (2nd main-seg DL TU).** From
`func_8006ED34` (a two-texture fog/scroll screen filter):
- **`gDPxxx(gfx++)` post-increment is load-bearing, not cosmetic.** When the cursor is a local
  `Gfx *gfx` whose address is taken (`emit_per_phase_fog_state(&gfx)`), gcc forces it to a stack slot
  and every command spills the advanced pointer (dead intermediate spills to `sp+off`). The stock
  macros expand `_g = (Gfx*)(pkt); _g->words.w0/w1 = …` with no self-increment, so the caller's `gfx++`
  supplies the advance. The post-increment form (write via the old pointer then advance) matches the
  ROM's base+offset-write-then-spill-advance codegen; a hand pre-store-then-advance form
  (`gfx->words.w0=..; gfx++`) emits an extra pointer `addiu` at each call/return boundary. So build with
  `gDPxxx(gfx++)` and cache `Gfx *gfx = *glistp; …; *glistp = gfx;` (the func_800500E0/func_8005029C idiom).
- **gfxdis.f3dex2 -f does fold the texture-load composites** (corrects the "gfxdis does not fold" note
  above): `gfxdis.f3dex2 -f <binfile-of-BE-u32-words>` emits `gsDPLoadTextureBlock`/`gsDPLoadMultiBlock`
  folded from the 7-primitive SETTIMG/SETTILE/LOADSYNC/LOADBLOCK/PIPESYNC/SETTILE/SETTILESIZE run. The
  dynamic `gDPLoadTextureBlock(gfx++, …)` works by textual substitution — `gfx++` is pasted into all 7
  sub-macros, so one call advances the cursor 7 (matching the ROM's 7 spills). Verified byte-exact by
  full-make.
- **Recipe — bank raw, then refine.** First bank the verbatim command words (a
  `g = dl++, g->words.w0 = .., g->words.w1 = ..` blob) for a guaranteed score-0 match; then (at review
  or a refine pass) decode with gfxdis and rewrite to stock macros, re-verifying byte-exact by
  full-make. Feed a dynamic address (texture image / scroll coord / vtx pointer) to gfxdis as a
  placeholder word, then swap the real expression back into the macro arg. gfxdis input quirks for MG64
  asm: `extract_dlist.py` wants bare mnemonics at line start, so strip the `/* … */` prefix and the `$`
  from registers (`sed -E 's|/\*[^*]*\*/||; s/\$//g; s/^[[:space:]]+//'`). Decode FP-looking words with
  `tools/fpdecode.py` before writing a C literal.
- **Macro param/field-name collision (parse trap).** A word-write helper macro whose parameter shares a
  name with a struct field it writes (`#define G(w0, w1) g->words.w0 = (w0)`) silently rewrites
  `g->words.w0` → `g->words.<arg>` via preprocessor token replacement → KMC-gcc `parse error`. The
  isolated `decomp_loop` base.c may use safe param names (`a,b`) and pass, so only the in-tree build
  catches it. Use distinct param names (`cw0/cw1`) — or just the stock GBI macros.

**RCP-init / framebuffer-clear idiom + the `& ~7` game-mod (3rd main-seg DL TU, S179).** The
`gfxRCPInit` / `gfxClearCfb` reference is `~/development/n64/n64demos/nusys/*/src/main/graphic.c`
(nu2/nu3/nuxbus). Match it against a viewport/segment/camera-preamble + scissor/depth+color-image +
fill-clear pack (S179 DCE0 pack `func_800328E0`):
- **Fill color = double-`GPACK_RGBA5551`.** `gDPSetFillColor(gfx++, (GPACK_RGBA5551(r,g,b,1) << 16 |
  GPACK_RGBA5551(r,g,b,1)))` — the demo idiom (game fills substitute RGB globals/args for the demo's
  literal `0,0,0`). Z clear uses `GPACK_ZDZ(G_MAXFBZ,0)` the same way.
- **Physical addresses via `OS_K0_TO_PHYSICAL` (macro, the `subu 0x80000000`) for the Z-buffer and
  `osVirtualToPhysical()` (the `jal`) for the CFB** — the demo's exact split.
- **The `& ~7` (8-byte align, `and $reg,-8`) on the physical address is GAME-SPECIFIC, not
  auto-inserted.** The demo has NO `& ~7`; MG64's `gDPSetColorImage`/`gDPSetDepthImage` args need it
  byte-exact (`OS_K0_TO_PHYSICAL(nuGfxZBuffer) & ~7`, `osVirtualToPhysical(nuGfxCfb_ptr) & ~7`) — S179
  `func_80032B78` matched WITH it (dropping it is 4 insns short). gSetImage stores the arg raw, so the
  mask lives in the source.
- **Idiom split determines matchability:** a helper taking `Gfx **glistp` (`Gfx *gfx = *glistp; …
  gDPxxx(gfx++); *glistp = gfx;`) matches cleanly (func_800328E0/func_80032B78 — the `*glistp` deref is
  a param-pointer, not a symbol-load). A fn using the **global `glistp++` directly** (13 stores of the
  `%lo(glistp)` symbol, 1 load) whose fill color is computed from **global** vars is the
  **scheduler-load-pair wall** (S179 carry `func_800329F4`): GCC 2.7.2's sched1 hoists the color
  global symbol-load into glistp's 2-cycle load-latency shadow, where the ROM defers the color load to
  its 6th-command slot — a full register cascade (77/97 words), structurally-complete otherwise.
  - **This wall has a FAITHFUL FIX — it is NOT permuter-class (S180 retired the S179 "escalate to the
    permuter" verdict).** It is the [#mem-in-struct-scheduling-lever](#mem-in-struct-scheduling-lever):
    the color globals as plain `s32` no-alias the `mem/s` `*glistp` stores, so `true_dependence`
    (`sched.c:817`, guard 834-836) gives the load empty LOG_LINKS → it is a free root, and the sticky
    `LAUNCH_PRIORITY` (0x7f000001) "birthing-insn" boost (`sched.c:3902`/2543; `priority()` early-returns
    at 1435 so it never decays; `rank_for_schedule` keys priority first at 2395, so it DOMINATES — not a
    tie-break) floats it backward on the load-use latency to the shadow. **Retype the RGB triple as one
    `Color {s32 r,g,b;}` struct** (or `s32[3]`): the loads become `mem/s`, may-alias the `*glistp`
    writes, and are NOT ready until those stores are placed → absent from the ready set when the shadow
    opens → the constant `lui`s fill it = the ROM's deferred schedule. Regalloc is 100% downstream of
    the schedule (leaf, no `$s` regs), so this snaps the whole 77/97 permutation exact. **Second
    required ingredient:** the color compute must land AFTER the fill-color w0 store — use the demo
    `gfxClearCfb` idiom (inline the double-`GPACK_RGBA5551` in the `gDPSetFillColor` arg, evaluated at
    w1) OR a temp `c` computed after the w0 assignment; a leading `c = …;` statement caps at 61/97.
    Data-side: declare the struct symbol in `symbol_addrs.txt` (`clear_color = 0x800B7840; //
    type:Color size:0xC`) — the data stays as-is in `main_data`, no carve. **Tell it is a color
    struct:** a SIBLING setter writes the N globals as N consecutive words (S180 `func_800329D8` writes
    `D_800B7840/44/48` from `a0/a1/a2`), and the fill fn reads them as R/G/B channels. The faithful
    double-GPACK, `-mips2`/`-mips3`, `vs32` volatile, local-pointer idiom, and every color-expr /
    associativity / reorder form ALL still hoist while the colors are plain scalars (18-30/97) — the
    retype is the unique lever, so do NOT reach for the permuter first. Found by the two-agents-per-wall
    GCC-source fan-out (S180; both agents independently converged on the `mem/s` lever). Permuter only
    as a last-resort fallback (and then **without `--best-only`**, the S160 plateau doctrine).

**Header constant-staging scheduling wall (a DL emitter can be irreducible, not just FP).** A
`glistp++` DL builder with a long fixed-command header (many `gDPxxx(gfx++)` in a row) can plateau on
a whole-header instruction-scheduling divergence even when the body is byte-faithful and the fn is a
0-jal / 0-FP "leaf": the ROM **front-stages the header command constants to scattered stack slots**
(reusing one scratch reg: `lui s7;ori s7;sw s7,STACK` repeated) **and precomputes ALL command
addresses into distinct registers** (`move v0,s8;addiu s8,8;move v1,s8;addiu s8,8;…` = running-ptr
snapshots) before storing, while faithful `gDPxxx(gfx++)` C keeps the constants in registers and
interleaves (smaller frame, ~10% fewer instrs). This is GCC's list scheduler choosing a
compute-all-then-store batch the equivalent C doesn't trigger; the words/ops are identical, only the
schedule differs. It is NOT the `#mem-in-struct-scheduling-lever` (no global load to retype) and the
**permuter does not crack it** (S190 `func_8004E5A0` font-blitter: 225/233 rows, permuter
`--best-only` plateaued 10065→6235 over 8000+ iters, no match). Treat such a header as a structural
near-match **carry**, not a bank; a debug/HUD DL TU where the non-FP fns are all such emitters is
**partial-bank-expected-ZERO** (see `BACKLOG.md` for the pts-detector follow-up). Diagnose by the
frame-size + instr-count gap (ROM larger) with rows structurally aligned, not by a body diff.

**Provenance:** S148 (the "main/ needs zero mk edits" convention this rule updates); S151 (first
main-seg DL TU: the dynamic-builder decode-and-reconstruct procedure and the mask-narrowing lesson);
S160 (2nd main-seg DL TU: the post-increment idiom and composite folding); S179 (3rd main-seg DL TU:
the m2c-body + gfxdis.f3dex2-`extract_dlist.py` seed combo, the RCP-clear/`& ~7`-game-mod idiom, and
the global-`glistp++` scheduler-load-pair wall); S180 (BANKED that wall byte-exact via the
color-struct `#mem-in-struct-scheduling-lever` — the n64demos `gfxClearCfb` global-`glistp++` idiom is
the faithful reference, confirmed by `~/development/n64/n64demos/nusys/nu2/src/main/graphic.c`); S190
(the header constant-staging scheduling wall — a permuter-resistant DL-emitter carry class).

---

## data-rodata-carve

**Trigger:** resolving the anonymous `.data`/`.rodata` blocks in a whole library region into named
`.data`/`.rodata, libultra/<tu>` subsegs (e.g. the libultra `.data` block 0xA32D0–0xA5668).

**Rule:** A libultra `.data` region is a contiguous link-order run; each TU is carved independently by
splitting the anonymous `[0xXXXX, data]` around its `[start, end)`.

**Procedure:**

**Attribution oracle.** `make extract` (splat) prints `Rodata segment 'X' may belong to the text
segment 'Y'` (splat `rodata.py`) for each anonymous rodata referenced by a single function; capture
stderr. `.data` blocks get no such hint: attribute by grepping the **defining** upstream file in
`~/development/repos/ultralib/src` for the symbol (`OSThread *__osRunQueue;`, not just an `extern`),
then confirm placed-status in the yaml. `../drmario64/lib/ultralib/src` is the cross-ref for TUs MG64
hasn't placed; a block drmario64 also leaves generic (`RO_<vram>`) is genuinely unattributable, so
leave it a blob.

**Three carve kinds:**

1. **Placed drop-def restore.** The TU is placed but mirrored "drop-def" (its data def was turned
   into an `extern`). Restore the upstream initializer in declaration = ROM-address order
   (`extern T x;` → `T x = <upstream init>;`), confirm bytes vs `asm/data/<blk>.data.s`. Function
   statics (`dtor`, `nintendo[]`, `xseed`) go back as `static` (file-scope static byte-matches the
   function-local form). `symbol_addrs.txt` is add-only, so leave the extern; the restored def coexists
   via `--allow-multiple-definition` (same address).

2. **Not-placed data TU vendor.** Pure-data upstream TUs (`io/vitbl.c` osViModeTable, `vimodes/*.c`)
   are copied verbatim (`PRinternal/viint.h`→`viint.h` rewrite). Code+data TUs whose `.text` is still
   asm (`os/thread.c`, `io/vi.c`) get a data-only file emitting just the globals (empty `.text`,
   no text subseg); see the two gotchas below.

3. **asm-mirror un-strip.** A stripped-jtbl `hasm` keeps its tables as blobs only while their
   target labels aren't exported. Once the vendored `.text` re-exports the jtbl-target `.L<addr>`
   labels, append the tables back to the `.s` as `.section .rodata`/`.section .data` and carve; the
   `.word .L<addr>` resolves locally (S111 exceptasm: `__osIntOffTable`+`__osIntTable` rodata,
   `__osHwIntTable`+`__osPiIntTable` data). A stripped-jtbl hasm's tables are carve-able, not
   permanent blobs (corrects the earlier "must stay anon" framing).

**The four SHA/link gotchas (all were masked by an ungated `sha1sum`; use
`tools/verify-rom.sh`):**

- **0x10 `.data`-size-pad boundary.** GCC pads each `.o`'s `.data` *size* up to 0x10. The carve's
  next-subseg boundary is the 0x10-aligned end of the TU's data, not the last symbol's end: the
  "Automatically generated and unreferenced pad" `D_<vram>` syms are that trailing pad (initialize:
  0x14 data → next subseg at +0x20). Wrong boundary = exact-N×byte SHA miss.
- **reloc-to-bss needs placement.** A restored pointer init to an uninitialized `.bss` symbol
  (`OSTimer* __osTimerList = &__osBaseTimer;`) is a `.data` reloc; if the bss target isn't a placed
  symbol the link fails `undefined reference`. Name it in `symbol_addrs.txt` at its baserom bss vram
  (read from the ROM pointer value); do not `define` it in the `.c` (that perturbs bss layout and the
  pointer value won't match).
- **cross-TU-split data needs non-`static`.** Upstream keeps file data `static` because the function
  using it is in the same `.c`. When MG64 has split that function into its own TU (`io/viinit.c`'s
  `__osViInit` does `extern __OSViContext vi[2]`), the data must be **global** (drop `static`) so the
  split TU links. Bytes are identical; only visibility changes.
- **verbatim mirror `needs-define`.** A verbatim data TU can use a header macro the project's vendored
  header lacks (`vitbl.c` → `VI_CTRL_ANTIALIAS_MODE_0`, absent from the MG64 `rcp.h` which had only
  `_1/_2/_3`). Add the missing define from the `~/development/repos/ultralib` authority; a shared-header
  edit then needs a clean rebuild (`#clean-rebuild-after-shared-header-edit`).

**Provenance:** S111 (swept the libultra `.data` block 0xA32D0–0xA5668; named the four SHA/link gotchas
and the exceptasm jtbl un-strip); S107 (the stripped-jtbl `hasm` "must stay anon" framing this
corrects).

## same-TU inline mismatch (definition-order + cross-TU split)

**Trigger:** A fn builds + links clean but ROM SHA-misses, and the in-tree-vs-target objdump shows my
build **inlines a small static callee the ROM keeps as `jal`** (build longer than target, the callee's
body appears inline), or the reverse. The classic case is a per-frame/dispatch fn that calls a helper
the ROM out-of-lines.

**Rule.** GCC 2.7.2 `-O3` inlines a small static callee only when it is **visible** (defined, not just
forward-declared) **within the same TU** at the call site. So an over-inline has two independent cures;
pick by where the callee actually lives in the original:

1. **Cross-TU split (callee is in a different `.o` than its caller).** Split the carve at the real
   `.o` boundary so caller and callee land in separate TUs → cross-TU `jal`. Boundaries must be
   **16-aligned** (`.text`/`.rodata` sections are `2**4`-aligned). Use the **rodata-gap diagnostic** to
   decide if this is even possible: if two fns' `.rodata` constants are **< 16 bytes apart**, they are
   in the **same `.o`** (two separate 16-aligned `.o` rodata sections can never be < 16 B apart), so
   the split is impossible — go to cure 2. (S147: `mus_cmd_envelope`@D_800D1FB0 and
   `func_8009AB18`@D_800D1FB8 are 8 B apart ⇒ one `.o`; `func_8009BC58`/`allocate_object_slot` vs their
   command-handler callers were cross-`.o` ⇒ split at 0x8009C540 banked them.)
2. **Same-TU definition order (callee shares the caller's `.o`).** Make the caller a **separate static
   helper defined before the callee**, so the callee is only **forward-declared** at the helper's call
   site → GCC can't inline it (`jal`), while the tiny helper itself inlines into its parent. (S147:
   `__MusIntFifoProcess` (the fifo drain) defined above `mus_fifo_dispatch`, with a forward decl of the
   dispatch, kept the dispatch out-of-line; the drain inlined into `__MusIntMain`. Canonical libmus has
   the same order: `player_fifo.inc.c` drain @73, dispatch @101.) A static helper called once is
   inlined+eliminated, so it adds no symbol and does not shift the layout.

**Two more `__MusIntMain` match details that compound an inline mismatch:**

- **SUPPORT_PROFILER on.** 2× `jal osGetCount` bracketing the body + `g_mus_cpu_last`/`g_mus_cpu_worst`
  stores at the end (the +15 insns vs a profiler-off reference). The `_mus_cpu_*` globals existing in
  `ghidra_symbols` is the tell; the matched reference games (PPL/drmario64) have it off.
- **Cross-TU helper hand-inlined.** A helper the one-TU reference games just *call* (and GCC inlines,
  e.g. `Fstop`) is **cross-TU** here, so GCC can't inline it — reproduce by **manually inlining its
  field-clears** in the body. The ROM showing **no `jal` to the helper** + inline field stores is the
  tell.

**Signed-subtraction comparison tell.** When the ROM shows `subu rd,a,b; bgez/bltz` (not `slt`/`sltu`)
for a frame/counter/index `<` compare, write it as **`(s32)(a - b) < 0`**, not `a < b`. `a < b` on
unsigned fields → `sltu`; an `(s32)a < (s32)b` cast → `slt`; only the explicit difference-vs-0 form
emits `subu + bgez`. (S147: `__MusIntMain`'s 4 `*_frame < channel_frame` tests, all on `unsigned long`
fields, needed `(s32)(cp->X_frame - cp->channel_frame) < 0`.)

**Before declaring a same-TU inline mismatch unbankable, build the matched reference games and compare
the source structure** (defn order, separate-helper vs hand-inline, config flags), not just the bodies.
`__MusIntMain` was twice wrongly written off (once "compiler wall", once "permanent same-TU carry");
both fell to a structural fix found by disassembling PPL's byte-exact `-O3` `__MusIntMain`. The matched
libmus games are `../drmario64`, `../hm64-decomp`, `../snowboardkids2-decomp`, `../puzzleleague64`
(KMC gcc; PPL builds byte-exact at the same `-O3 -mips3`); ROMs in `~/games/N64/`. See
[the "rule out body before compiler wall" memory] and `#cross-jump-tail-merge` (the sibling
shorter-build symptom).

**Provenance:** S147 — the libmus `__MusIntMain` match: the drain hand-inline that pulled
`mus_fifo_dispatch`'s switch inline (+24 insns) seeded the Trigger; the SUPPORT_PROFILER-on 2×
`osGetCount` bracket + `g_mus_cpu_*` stores and the cross-TU `Fstop` hand-inline are the compounding
details; the class was twice wrongly written off and both times fell to a structural fix from PPL's
byte-exact `-O3` build.

## cross-jump-tail-merge

**Trigger:** A verbatim/near-verbatim mirror builds + links clean but ROM SHA-misses, and the
in-tree-vs-target objdump shows the build is **shorter than the target** (instruction count < target),
with collateral `-0x10`-style address shifts on every symbol after the short function (their code
matches; only referenced addresses shifted).

**Rule:**

**Rule out a game-modified body first.** This symptom is not proof
of a compiler cross-jump merge; it is far more often a **body-semantics divergence** the literal
upstream lacks. `contRmbControl` was declared an "exhaustively-proven-unbankable cross-jump wall" and
carried it 5 sprints (+ a 145k-iter permuter run), then banked it byte-exact (full ROM SHA-1) with a
**one-branch body fix and no compiler change**. The real cause: MG64's FORCESTOP case is game-modified
— on `osMotorInit` failure it sets `state = STOPPED`, on success `state = STOPPING; counter = 2` (an
`if/else`), where the nusys/papermario upstream sets `state = STOPPING` unconditionally.
- **The tell:** the target's FORCESTOP epilogue has two `sb v0,6(s0)` state stores with different
  values (`li v0,1` in one branch; `li v0,2` from the `bnez` delay slot in the other). A single
  unconditional `state =` cannot emit two differing state stores → the body has a branch the upstream
  lacks. **Read the target's store sequence and values, not just the control-flow shape.**
- **Rule:** a "shorter + collateral-shift" mirror miss is a **body bug until proven a compiler
  artifact**. A store your upstream body cannot produce (extra branch, different constant, doubled
  field) is the fix. Same class as `#struct-init-loop` (a dup-store body artifact, also "count-short").

- **Tell from block-reorder (`#near-verbatim-mirror-jal-count-mismatch`).** cross-jump = build instr
  count **< target** (a merge deleted instrs). block-reorder = **equal** count, same insns reordered.
  Measure the count first.
- **The actual cross-jump pass (why a body fix moves it).** `jump.c:find_cross_jump` can merge two
  identical `j .L_ret; sh <reg>,<off>(<base>)` tails via the `minimum=1` "cross-jump to code before the
  label" path (jump.c ~1978): **one** matching insn before the shared epilogue label suffices
  (`--minimum` → `minimum <= 0` after a single match). So the merge is driven by basic-block **layout**
  — which store lands immediately before the epilogue label — and the layout is driven by the body.
  Change the body → change the layout → the "merge" moves or vanishes. The pass is always-on
  (`toplev.c:3142` `jump_optimize(insns, 1, 1, 0)`, gated only by `optimize > 0`; no disabling flag —
  `-fno-thread-jumps` gates the separate `thread_jumps` pass), which is exactly why its layout effects
  get misread as an irreducible compiler wall.

**No compiler cross-jump wall is confirmed in this project.** The sole candidate (`contRmbControl`)
was a body bug. If you ever reach byte-identical-unmerged tails with a body proven to emit the target's
exact stores + values, only then suspect a compiler-build divergence — and re-audit the body once more
first. A genuinely stuck mirror fn then falls back to: partial-bank the matching siblings as C + carry
the stuck fn as `INCLUDE_ASM` (ROM stays green; forward-decl `extern` since the asm `glabel` is
`.globl`), or `hasm`-split it. A `jump.c` patch is not a banking path (open toolchain research only).

**Sibling rule — rule out stock-plus-insert before treating a carry as from-scratch custom.**
The plan/seed-time inverse of the above triage. A carry tagged "MG64-custom body → classical" is
**stock-source-plus-a-small-insert until proven otherwise** — re-diff the asm against the stock upstream
after the library's headers are vendored, before assuming from-scratch classical work.
`__MusIntThreadProcess` was carried as an "MG64-custom body" with `last_task` called a "custom
sched-state global"; once `aud_sched.h` was vendored (exposing the stock `musSched` vtable + the
`__MusIntSched_{install,waitframe,dotask}` macros @ offsets 0/4/8), the body was the stock libmus 3.14
thread-proc and `last_task` was the stock func-static — only a ~4-instr pause/mute insert (`if (paused) {
osAiSetNextBuffer(silence, 0x10); continue; }`) was genuinely MG64-custom. Seeding from the stock source
+ the insert banked it near-verbatim first build vs hand-writing ~100 instrs. This generalizes the
rule-out-body-before-compiler-wall discipline to the carry-triage direction: a "custom body" label set
before the headers are in place over-prices the work — defer the classical-vs-mirror verdict until the
stock diff is possible.

**Check the build-config / struct-size axis too — a `@99.99` miss is not always a body edit.** A
`body-divergence-suspect:<file>@<pct>` miss has three non-body causes to clear before concluding the
body diverges, distinguished by the offset pattern: (1) cross-jump = build instr count **< target**;
(2) block-reorder (`#near-verbatim-mirror-jal-count-mismatch`) = **equal** count, insns reordered;
(3) **build-config struct-size drift** (`_FINALROM` / `_DEBUG`; see the `_FINALROM` note under
[`#upstream-mirror-pattern`](#upstream-mirror-pattern)) = equal count, but one function's stack-frame
immediates (`addiu sp`, `sw/lw ra`, trailing-local `sp`-offsets) all shift by the same struct delta
while field stores are unchanged. `aud_sched.c`'s `__OsSchedDoTask` was a `@99.99` flagged file whose
miss was cause (3) — a 16 B `OSScTask` `_FINALROM` size drift, body byte-stock; the `@99.99` was coddog
structural noise (func_/ghidra naming), not a body change. Read the offset pattern (count-short vs
reordered vs uniform-frame-shift vs an extra store/branch with a new value) before assuming a
game-modified body.

**Companion to the sibling rule — ASM-verify each fn in a "mostly stock" mirror; a carry-over's per-fn
"likely stock" label is a hypothesis, not a finding.** The sibling rule above guards against
over-pricing a carry ("custom body" that is really stock-plus-insert); the inverse under-prices just as
often — a carry-over checklist that pre-labels "the N stock fns mirror verbatim" can hide per-fn
game-modification. `aud_dma.c` was carried as "4 stock fns + 1 game-modified (DmaSample)";
ASM-first found 3 of the "4 stock" diverged: `__MusIntDmaInit` stock+1-insert (persists a count global),
`__MusIntDmaProcess`'s whole second half rewritten (a flat-array `keep_count` ageing pass under
`osSetIntMask` replacing the upstream linked-list free-walk), `__MusIntDmaSample` heavy. All banked
in-sprint, but the per-fn divergence was the work, not the lone labeled carry. **Disassemble and diff
every member fn vs the stock upstream before trusting a "likely stock" label** — the same per-fn
ASM-authority (`#decompile-vs-asm-authority`) applied member-by-member, not file-wide.

**The inverse lever-set — preventing a false tail-merge in a game-modified body.** The section
top rules out a merge you cannot stop (it is a body artifact); this is the opposite — a merge GCC
performs that you must block to match. `__MusIntDmaSample`: an early `if (cond) return NULL;` plus
a later `if (!free_buffer) return dma_buffer_head;` (where `free_buffer == dma_buffer_head` and
`!free_buffer`, so GCC value-propagated the second to `return NULL`) got tail-merged into one block; the
target keeps them separate because its `return dma_buffer_head` block is shared with a third
non-NULL-provable path, blocking the simplification. Four reusable KMC GCC 2.7.2 -O3 codegen levers, each
verified against the ELF-disasm word-diff (count first, then per-instruction — 5 of 6 fns hit the target
instr-count on first compile, isolating the lone short/reordered fn fast):
- **explicit `else`** on an early-return flips the condition's branch direction (`bnez`↔`beqz`): `if (c)
  return; else x;` placed the return-block inline (target) where `if (c) return; x;` reordered it.
- **a shared `goto fail;`** for two identical-tail failure returns merges them into one block before
  value-prop can specialize one — so a `return GLOBAL` reached from a non-NULL path is neither simplified
  to `return NULL` nor false-merged with an unrelated `return NULL`.
- **goto-skip** (`…success path…; goto ok; fail: return X; ok: …`) reproduces a failure-block placed
  mid-function (the target's `j <continue>` over the failure return), not pushed to the function end.
- **compare operand order** (`a->f > b->f` vs `b->f < a->f`) controls which operand loads first inside a
  min/compare loop.

**The classical continue-loop tail-merge — nested-if forces the branch-likely (S172).** A CLASSICAL
(non-mirror) instance of the same "block a merge GCC performs" pattern, and the primary source lever
for it. A guard-then-continue inside a top-tested loop — `while(1){ …; if (x < lo) { x++; continue; }
if (x < hi) *x = v; x++; }` — has two identical `x++; j <loop-top>` tails (the `continue` one and the
bottom one). GCC 2.7.2 **cross-jumps them into one block** (both paths reach a single `x++; j` = a
double-jump), where the ROM keeps them separate: the `x < lo` branch is a **branch-likely** (`bnezl`/
`beqzl`) whose **annulled delay slot** holds its own `x++`, and the bottom `x++` sits in the `j`'s
delay slot. Re-express the guard as a **nested `if/else` with a duplicated increment in each arm** —
`if (x >= lo) { if (x < hi) *x = v; x++; } else { x++; }` — and GCC fills the `x < lo` branch's delay
slot with the annulled `x++` (emitting the `bnezl`) instead of merging the tails. Banked
`print_string_at_grid` S172 (the S171 `#pervasive-regalloc-classical-main` carry): 25/25 word-exact.
This is the source lever to try **before** the permuter when a classical loop's only structural miss is
a cross-jumped `{x++;continue;}` guard vs a ROM branch-likely.
- **Companion reg-cycle lever — lazy global-base load (S172).** After the nested-if fixes the
  structure, a residual **cyclic register permutation** (the target reuses a freed argument register for
  a running pointer) closes by loading the array base **lazily**: reference the global directly
  (`&G[idx]`, `&G[N]`, `x >= G`) instead of pinning it in a `T *base = G;` local. The local forces the
  base load early (before the offset), so the base gets a fresh register; the direct reference defers it
  **past** the offset computation, letting the offset's arg-register free up and be reused for the base
  — snapping the `{f,base,dst,end,c}` 5-cycle on `print_string_at_grid`. Generalizes the S171
  `&base[i]` index-group lever (`#indexed-vs-pointer-loop-strength-reduction`) from op-order to
  load-timing. Pair it with the nested-if lever above.

**Provenance:** rule-out-body-first (`contRmbControl`): S121 (the 5-sprint "cross-jump wall" carry + a
145k-iter permuter run) → S127 (the one-branch FORCESTOP body fix, byte-exact, no compiler change).
Carry-triage siblings: S143 (`__MusIntThreadProcess` carried as a "custom body") → S144 (the
stock-plus-insert rule-out); S145 (`aud_sched.c` `__OsSchedDoTask`, the `_FINALROM` struct-size axis);
S146 (`aud_dma.c` per-fn ASM-verify + the false-tail-merge lever-set on `__MusIntDmaSample`).

## struct-init-loop (dup-store / dual-induction-var)

**Trigger:** A near-verbatim mirror's **array-of-struct init loop** (`for(i){ arr[i].next = &arr[i+1];
arr[i].f = …; }` over a large struct) builds + links clean but ROM SHA-misses, the build is **shorter
than the target** (instr count < target, ~2 instrs short, with the usual `-0x10`-style collateral
address shifts on everything after it), and the in-tree-vs-target objdump shows the target storing
**one field twice** (the same value to the same offset at the loop body's head and tail) via a
**running address pointer**, while your build stores it once via indexed `lui %hi(base); addu …,v1`.

**Rule:** This is not a cross-jump wall (`#cross-jump-tail-merge`, which deletes instrs) and not
permuter territory (the byte-match is far below 0.97) — it is a **source artifact**: the original loop
body has a **duplicate field assignment** the literal upstream lacks.

**Procedure:**

- **Mechanism.** Taking an address into the array (`next = &arr[i+1]`) makes gcc 2.7.2 strength-reduce
  that to a running induction var (e.g. `a1 = &arr[i+1]`, stride = sizeof). A second assignment of a
  field whose address is a fixed offset below the next element (`arr[i].msgQ` at `next_elem - 8`) lets
  gcc derive a second running pointer (`a0 = a1 - 8`) and store that field via `0(a0)` at both the
  head and tail of the body — the **double-store tell**. Your single-assignment source gives gcc one
  IV (it picks one field for the running pointer, indexes the rest), so it is 1–2 instrs short and the
  registers/addressing cascade-diverge.
- **Fix.** Re-add the duplicate field assignment at the loop **tail** (the MG64 edit artifact:
  `nuGfxTaskMgrInit` has a trailing `nuGfxTask[cnt].msgQ = &nuGfxTaskMgrMesgQ;` after the field block).
  That single line reproduces gcc's dual-IV + double-store and lands the exact instr count → byte-match
  first build after. Match driver: the asm store order (`next, msgQ, …, msgQ`) — a field that appears
  at both ends of the store sequence is your duplicate-assignment cue.
- **Watch the value defines too.** MG64 also needs `yield_data_size = OS_YIELD_DATA_SIZE` (0xC00), not
  the 2.07 `NU_GFX_YIELD_BUF_SIZE` (= `OS_YIELD_DATA_SIZE + 0x10` = 0xC10) — the MG64 rev dropped the
  `+0x10` (a `#needs-define` version value). Size the extern array (`extern T arr[N]`, not `arr[]`) so
  gcc has the bound. The asm is authoritative for the store order and the immediate values; the
  upstream `.c` is the shape only.

(A `pick_target.py` `struct-init-loop` detection tag — a multi-store loop whose asm has a doubled
field-store offset — is a tracked follow-up; until then this is recognized by the build-shorter +
doubled-store-offset tell at match time.)

**Provenance:** S124 (`nuGfxTaskMgrInit`: the trailing `msgQ` dup-store that reproduces gcc's dual-IV,
and the `yield_data_size = OS_YIELD_DATA_SIZE` 0xC00 vs 2.07 `NU_GFX_YIELD_BUF_SIZE` 0xC10 value fix).

## permuter setup for KMC-toolchain mirrors

**Rule:** Running decomp-permuter on a `libnusys`/`libultra`/`libkmc` (KMC-toolchain) function needs
three fixes the generic setup misses.

**Procedure:**

- **(a) custom `--settings`.** The root `permuter_settings.toml` `compiler_command` is **generic** (`-I
  include` only, no `-DUSE_EPI` / per-library include paths), so `import.py` preprocessing fails on
  `#include <nusys.h>`. Pass `--settings <custom>.toml` whose `compiler_command` carries the file's real
  CFLAGS (the per-library `-I include/libnusys …` + `-D` defines), piped through `tools/cc/gcc -S` to
  `tools/cc/as`.
- **(b) KMC-safe `asm_prelude_file`.** decomp-permuter's default `prelude.inc` has `.set gp=64`, which
  KMC binutils-2.6 `tools/cc/as` rejects (`Expected comma after name gp`). Supply an `asm_prelude_file`
  that drops that line (harmless at `-G 0`).
- **(c) body + extracted target.** `import.py` accepts a `c_file` with the function body plus a target
  `.s` extracted from `asm/<seg>.s` (`glabel`..`endlabel`) — no `INCLUDE_ASM` / `mg_resolve_c_asm`
  round-trip is needed.

**Generalizes to game -O2 (main-profile) code.** The same three fixes apply to a `src/main/`
(or overlay) game fn, with the `compiler_command` mirroring `MAIN_CFLAGS` (`$(CFLAGS)` + all the base
`-I` + `-DF3DEX_GBI_2` for a DL fn) piped `tools/cc/gcc -S | tools/cc/as -EB -mips2 -G 0 -I include`.
Without `-DF3DEX_GBI_2` a DL fn never converges (wrong RSP opcodes). **S167 confirmed the full recipe
end-to-end** for a `src/main/` fn: scratch settings with `gcc -S -nostdinc -G 0 -mips3 -mgp32 -mfp32
-mno-abicalls -O2` + the full base `-I` set + `-DINCLUDE_ASM_USE_MACRO_INC -D_LANGUAGE_C -D_FINALROM
-DF3DEX_GBI_2`, piped to `tools/cc/as -EB -mips2 -G 0 -I include`, and a one-line
`sed -i '/^.set gp=64$/d' <dir>/target.s` after import (fix (b)) -- it built base+target and ran 43k
iterations cleanly. **DONE (S179): `permuter_settings_main.toml` + `setup-permuter.sh --main <func>`
now exist** — the flag passes `--settings permuter_settings_main.toml` to `import.py`, which bakes the
game -O2/F3DEX2 `compiler_command` (full `-I`/`-D` set + `-mips3`) and a **modern-GAS
`assembler_command`** (`mips-linux-gnu-as -march=vr4300 -32 -EB -I include --no-pad-sections`) that
assembles the `.set gp=64` target `.s` directly (replacing the `sed`-strip fix (b); the target's
explicit `addu`→0x1021 matches KMC-as `move`→0x1021, so the mixed assemblers stay byte-consistent). So
a main/ DL fn is now a one-flag setup. **Reminder for a scheduler/regalloc plateau: run WITHOUT
`--best-only`** (the equal-score-plateau case above) — S179 `func_800329F4` stalled at 2765/5210 WITH
`--best-only`, the classic symptom that the fix needs a same-score intermediate transform before a
second move reaches 0. **Coord/local
integer width
(`u16`/`s16` vs `s32`) is a first-class permuter lever for frame/regalloc near-misses:** S151
`func_800500E0` was a byte-perfect structure that locked ~185 on a register-allocation + a phantom
`-16` stack frame (a reload spill-slot artifact reachable only through register pressure); the permuter
cracked it by retyping the two texrect coords `u16 left; s16 right;` (which the kantan demo confirms is
the idiomatic coord type), then a final commutative operand-order swap (`offset + (x0+half)`) closed
the last instruction. When a classical match is a rows-aligned regalloc/frame near-miss with no
externs (isolated == in-tree), run the permuter even below the 0.97 asm-differ gate (asm-differ
normalizes registers, so its `percent` under-reports a pure-regalloc miss). **This extends to a pure
prologue-scheduling swap** (S160 `func_8006EA90`): when `match_count == total_rows` and the register
allocation is identical but `percent` sits ~0.90 because a handful of reordered early insns cascade the
score, the structure is provably right — the miss is which of two equal-priority preheader insn groups
the post-reload scheduler emits first (`sched.c` `rank_for_schedule` breaks priority ties by `INSN_LUID`
= physical/hoist order; see the gcc-source cross-ref above). Run the permuter without `--best-only` (the
equal-score plateau case: the fix needs an intermediate transform that keeps the same score before a
second move reaches 0 — the winner was a `scale = 0.3f` in-loop assignment plus a pointer alias that
reordered the FP-const load ahead of the base pointers). Structure levers that got it to score-0-modulo-
rodata first: hoist the invariant base pointer inside the loop (fixes `off+base` addu operand order) and
express the offset as a strength-reduced IV (`off = 0x40 + i*0x10`, moves its init late).

**Committed main-profile setup.** `tools/permuter_settings_main.toml` (MAIN_CFLAGS:
`-mips3 -mgp32 -mfp32 -mno-abicalls -O2` + all base `-I` + `-DF3DEX_GBI_2`, `tools/cc/gcc -S | tools/cc/as -EB -mips3 -G 0 -I include`, verified to reproduce the in-tree `.o`) and `tools/kmc_main_prelude.inc`
(the decomp-permuter `prelude.inc` minus its `.set gp=64` line, which KMC binutils-2.6 `as` rejects
with `Expected comma after name gp`) are checked in — pass `--settings tools/permuter_settings_main.toml`
to `import.py`. Gotcha: the extracted target `.s` must keep the `.LXXXX:` local-label lines
(`awk '/^glabel <fn>/{p=1}/^endlabel/{p=0}p'`, not a `grep` of only the `/* */` instruction rows) —
dropping them leaves the intra-function branches referencing undefined labels and `as` fails with
`Can not represent relocation in this object file format`. For the full whole-function playbook this
setup feeds, see `#pervasive-regalloc-classical-main`.

**Two KMC-gcc permuter tuning facts:**
- **`perm_sameline` is a no-op for KMC gcc 2.7.2.** gcc 2.7.2 ignores source line numbers, so
  same-line source produces byte-identical codegen (verified empirically; unlike IDO, where
  same-lineness is a real scheduling lever). Do not weight `perm_sameline` up in `settings.toml` for
  this project — it burns iterations for zero effect. The effective regalloc/scheduling passes here
  are `perm_temp_for_expr`, `perm_refer_to_var`, `perm_ins_block`, `perm_reorder_stmts` (default
  weights are fine; over-customizing can starve these).
- **`--best-only` cannot cross equal-score plateaus.** A 1-instruction miss often needs an
  intermediate transform that keeps the same score (e.g. a temp that just moves which instruction
  pair is swapped) before a second transform reaches 0. `run-permuter.sh`'s default `--best-only`
  (monotonic-improvement) gets stuck on such a plateau (250k+ iterations flat at score 60/50).
  For a plateau case, drop `--best-only` (default simulated-annealing accepts equal/worse moves), or
  seed `base.c` past the plateau by hand. (But first check whether the miss is a known structure
  hazard — the stalls were all the indexed-vs-pointer loop form, see
  `#indexed-vs-pointer-loop-strength-reduction`; a structural fix beats a permuter grind.)

**Two more S169 facts:**
- **`do{ body }while(0)` is a hand-seedable schedule lever.** Wrapping an `if` (or a small block) in
  `do{ ... }while(0)` shifts the -O2 instruction schedule without changing the logic. S169
  `func_80076640`'s `if(fabsf(cosPitch)<0.1f)` gimbal test scheduled the const-load FIRST (no stall,
  1 instr short); the permuter found `do{ if(fabsf(cosPitch)<0.1f){...} }while(0)` (plus a
  `cosPitch = cosf(pitch)` temp) which forces const-load-LAST and re-adds the `mtc1`->`c.lt.s` stall
  nop, fixing the count 77->78. Seed it by hand when a byte-exact-structure fn is one instr short/long
  around a branch. (It did NOT fix the residual register swap; see `#abs-coalescing-reg-swap`.)
- **Venv gotcha: `import.py` needs `toml`.** Run the permuter tools through the venv
  (`venv/bin/python3 ...`, or `mg_activate_venv` as `setup-permuter.sh` does); a bare
  `./tools/decomp-permuter/import.py ...` fails `ModuleNotFoundError: No module named 'toml'` even
  though the venv has it. And `mg_resolve_c_asm` needs an `INCLUDE_ASM` stub, so for an already-inlined
  one-tu fn pass `import.py <src.c> <awk-sliced target.s> --settings tools/permuter_settings_main.toml`
  directly (the src file is still in the build, so its main-profile compile command is extracted).

- **Inline-asm base.c fails pycparser — b64literal-wrap the line by hand (S176).** A `base.c`
  containing a `__asm__ __volatile__(...)` (e.g. the `#capturing-ra` `addu %0,$31,$0` read) makes
  `permuter.py` abort with `Syntax error in base.c … before: __volatile__`. import.py's
  `-D__asm__(...)=_permuter_ignore_line __asm__(__VA_ARGS__)` macro does **not** fire here because
  `__volatile__` sits between `__asm__` and `(`, so the function-like macro never matches (and cpp
  won't re-scan after `-D__volatile__=` strips it). Manually convert that one line to the pragma
  import.py would have emitted (`import.py:481-484`): `#pragma _permuter b64literal <base64 of the
  original line>` (e.g. `venv/bin/python3 -c "import base64;print(base64.b64encode(open('L').read().strip().encode()).decode())"`).
  pycparser parses the pragma (skipped by the randomizer, so the asm line is held fixed) and the
  permuter decodes it back to the real `__asm__` for each candidate compile. This is how S176's
  `heap_alloc` (ra-capture inline asm) was made permuter-loadable; the base scored 1595 and drove down
  to 605 (the register lever, not the permuter, closed it — see `#loop-weight-and-live-length-regalloc-steering`).

- **Setup on a fn ALREADY inlined as C in a multi-fn file (S182).** `setup-permuter.sh` /
  `mg_resolve_c_asm` (tools/lib.sh) resolve the C file by grepping for `INCLUDE_ASM(.*, <fn>);` — which
  is **gone** once the fn is a C body (a near-match you want to permute in place). The resolver then
  sets `C_FILE=` empty and `set -e` aborts SILENTLY (no output). **Workaround:** bypass the resolver and
  call `import.py` directly with the containing C file + the exact per-fn target `.s` (venv-run):
  `venv/bin/python3 tools/decomp-permuter/import.py --settings permuter_settings_main.toml
  src/<seg>.c asm/nonmatchings/<tree>/<file>/<fn>.s`. import.py picks the target fn by the `.s`
  filename, so a multi-fn `src/<seg>.c` is fine. (Golden-gated tooling follow-up: teach
  `mg_resolve_c_asm` to also resolve a fn defined as C — grep for the `<fn>(` definition + resolve the
  asm at `asm/nonmatchings/**/<fn>/<fn>.s`; kin to the S170/S168 `find_segment` follow-ups. Until then,
  the direct-import recipe applies by hand.)

**Provenance:** S121 (contRmbControl: the three KMC-toolchain fixes); S151 (generalized to the game
-O2 main-profile + the coord-width permuter lever); S157 (KMC-gcc tuning: `perm_sameline` no-op,
`--best-only` plateaus); S158 (committed `permuter_settings_main.toml` + `kmc_main_prelude.inc`);
S169 (`do{}while(0)` schedule lever + the venv/inlined-fn import path); S176 (the b64literal inline-asm
base.c fix); S182 (direct-import bypass for an already-inlined multi-fn-file near-match).

## NU_DEBUG-stock-not-custom (carried perf fn triage)

**Rule:** A `libnusys` carry framed as "heavily game-customized, classical RE" is often not custom at
all — it is the **stock upstream body that failed to match because the file was compiled without
`NU_DEBUG`**, so the `#ifdef NU_DEBUG` performance machinery is absent from the C while the asm has it
(osGetTime / osDpSetStatus / __udivdi3 calls).

**Trigger:** The tell: **the carried fns are exactly the upstream fns that carry `#ifdef NU_DEBUG`
blocks** (nusched's 4 NU_DEBUG fns — nuScCreateScheduler / nuScEventHandler /
nuScExecuteAudio / nuScExecuteGraphics — carried as "custom"; nuScExecuteAudio is a pure
stock-NU_DEBUG mirror).

**Procedure:**

- **Triage before classical-RE.** Diff the carried fn's asm against the upstream's **NU_DEBUG** body
  (not the non-debug body). If the calls line up, it is stock: bank it as a mirror.
- **Fix recipe** (S125 nuScExecuteAudio, banked first build). (a) `#define NU_DEBUG` at the top of the
  `.c` before `#include <nusys.h>` (matches the MG64 TU compile); (b) validate the perf/debug struct
  offsets vs the asm and fix the vendored header (see the perf-struct note in `#upstream-mirror-pattern`
  — MG64's `NUDebTaskPerf` lacked the 2.07 `markerTime[10]`); (c) drop-def the `static` perf pointers
  (`debTaskPerfPtr` etc.) to extern at their asm-recovered vrams. The stock body is then verbatim
  (drop-static).
- **Caveat.** NU_DEBUG-on covers only the carried perf fns; the already-banked stock siblings have no
  NU_DEBUG blocks so the file-wide `#define` does not perturb them. A fn with NU_DEBUG-stock perf code
  plus genuine MG64 edits (swap-gate, game hooks) is still classical on top of the NU_DEBUG stock
  skeleton (S125 nuScEventHandler / ExecuteGraphics).

**Provenance:** S123 (carried nusched's 4 NU_DEBUG fns as "custom"); S125 (found the stock-NU_DEBUG
mirror class + the fix recipe).

## libnusys inline-div mflo-hazard nop

**Trigger:** A `libnusys` fn with an **inline integer division** (`a / b` compiled to `divu` + `mflo`,
not a `__udivdi3` call) can build + link clean and be **byte-perfect except for 2 missing `nop`s after
`mflo`** (the VR4300 mflo→consumer hazard padding) — a `mflo; nop; nop; <use>` in the target vs
`mflo; <use>` in your build. Net: the build is 2 instrs short with the usual collateral address shifts.

**Procedure:**

- **Root cause** (S125 nuScEventHandler). KMC gcc emits the GNU `div` **macro**; the assembler
  (`tools/cc/as`) expands it and inserts the hazard nops — but only when it can see the consumer is too
  close. When gcc -O2 schedules the mflo consumer **into a loop-back `j`'s delay slot** (`mflo; j;
  addiu` — consumer 2 slots after mflo across the branch), `as` does not pad, while the original build
  emitted `mflo; nop; nop; j; addiu` (consumer 3 slots after). A standalone `int d(int a,int b){return
  a/b - 3;}` does get the 2 nops by default, so it is **not a blanket missing flag** — it is the
  loop-back-delay-slot scheduling that suppresses the pad.
- **Flags do not help.** KMC gcc/as reject `-mfix4300`, `-mcpu=vr4300`, `-mtune=vr4300`,
  `-mfix-vr4300`, `-Wa,-mfix-vr4300` (modern-gcc/binutils flags absent in the KMC 2.7.2 / binutils-2.6
  toolchain).
- **Not a permuter wall.** This was earlier framed as a permuter candidate, then nuScEventHandler
  banked with no permuter: the 2 nops appear **naturally** once the function's data + volatility
  scaffold is complete — specifically `nuScRetraceCounter` as a proper **per-TU volatile** (see
  `#volatile-global tell`) and `nuDebTaskPerf` placed in `symbol_addrs`. The earlier "missing nops" was
  a **scaffold artifact** (an incomplete/header-flipped attempt scheduled the consumer differently), not
  a KMC-as scheduling wall. **Lesson: complete the per-TU-volatile + data scaffold and re-diff before
  concluding a mflo-hazard near-miss needs the permuter.** Most "byte-perfect except 2 nops" libnusys
  near-misses are an unfinished scaffold, not an assembler interaction.
- **Resolution ladder.** (a) **Finish the scaffold first** — per-TU volatile globals + every referenced
  data symbol placed, then re-diff; the nops usually resolve. (b) Only if a complete-scaffold build
  still SHA-misses by exactly the 2 nops is it a genuine scheduler/assembler wall → **permuter** (a source
  form that keeps the mflo consumer out of the `j` delay slot, so it lands ≥3 slots after mflo, lets `as`
  insert the nops; `#permuter-setup-for-kmc-toolchain-mirrors`). Do not treat the 2-nop gap as a "try
  harder C" iteration — it is a scheduler/assembler interaction, not a logic mismatch.

**Provenance:** S125 (root cause on nuScEventHandler; first framed as a permuter candidate); S126
(overturned that — banked nuScEventHandler with no permuter once the per-TU-volatile + data scaffold
was complete; the 2-nop gap was a scaffold artifact, not an assembler wall).

## volatile-global tell (dead-reload + recompute-not-CSE)

**Rule:** Declare a volatile-tell mirror global `vu32`/`vs32` (the ultra64 volatile types), not
`u32`/`s32`.

**Trigger:** A `libnusys`/game mirror global that the asm accesses with **a dead reload immediately
after a store** (`lw t,X; addiu t,t,1; sw t,X; lw t,X` where the 4th load's value is discarded) and/or
is **re-read fresh on every reference instead of CSE'd** (e.g. `a - b` recomputed at each `==` compare
rather than computed once) is **`volatile`** in the original.

**Procedure:**

- **Tells.** (1) The dead reload-after-store is the **`volatile x++`** signature (gcc 2.7.2 re-reads
  the volatile lvalue). (2) The per-read recompute (no common-subexpression elimination of `a - b`)
  means both operands are volatile, so each read is a fresh load. S125 nuScEventHandler:
  `nuScRetraceCounter`, `D_801B68E0`, `D_800B678C`, `D_8012F4D4` are all volatile.
- **Consequence for source form.** Because volatile blocks CSE, a sub-expression reused across several
  comparisons (`(nuScRetraceCounter - D_8012F4D4)` against `0x20` and `0x36`) must be captured in a
  **single non-volatile local** (`s32 frame = a - b;`) so gcc computes it once and reuses it — matching
  the target's single `subu` + two compares. Inlining the volatile expression at each compare instead
  emits a reload+recompute per site and lets gcc canonicalize `(a-b)==c` to `(a-c)==b` (wrong codegen).
  Also place that local's computation in program order relative to the other volatile reads so the
  scheduler reproduces the target load order (compute `frame` inside the `>=0x1F` block, before
  the non-volatile pointer test, so the volatile reloads precede the pointer load).
- **Inverse lever, `const`-extern forces cross-call CSE into a callee-saved reg.** The mirror of the
  volatile tell: a FIXED rodata constant read from an **extern global** and used across a `jal`
  RELOADS on each use (gcc assumes the call may write the global) unless the extern is declared
  `const`, which lets gcc keep it in ONE callee-saved reg (a single load, reused across the calls).
  S169 `func_80076640` read a rad-to-deg double (`D_800D1868` / `D_800D18F0`) across two `guRotateF`
  calls; the plain extern reloaded (an extra `ldc1` plus a smaller frame), while
  `extern const f64 D_800D1868;` produced the target's single callee-saved load. Declare `const` for
  any extern rodata constant a classical/mirror fn reads across a call; use `vu32`/`vs32` (above) for
  the opposite, when a per-read reload is wanted.
- **Shared-header caution.** Flipping a header-declared global to `vu32` (e.g. `nuScRetraceCounter` in
  `nusys.h`) changes codegen for every consumer — re-verify the already-banked consumers on a clean-rebuild
  SHA check (no header-dep tracking, so an incremental build hides the breakage; `#clean-rebuild-after-shared-header-edit`).
- **Per-TU volatile — the global is volatile in one TU, plain in others.** A global can read
  `volatile` in the scheduler TU (dead-reload tell) yet `u32` in a sibling TU that is already banked
  non-volatile. The original compiled each TU against its own declaration. Replicating it: **keep the
  shared header non-volatile** (so the banked consumers stay matching) and add a **localized
  `extern volatile u32 <g>;` redeclaration in the volatile TU only** (after the header include). GCC 2.7.2
  accepts the qualifier-mismatch redeclaration with a benign `warning: type mismatch with previous
  external decl` and emits per-access `lui`/`lo` absolute volatile addressing — matching the reference.
  (S126 nuScEventHandler: `nuScRetraceCounter` is volatile in `nusched.c`, plain `u32` in `nugfxtaskmgr.c`
  + `nucontrmbmgr.c`.)
- **Do not use a cast macro for per-TU volatile.** `#define g (*(vu32 *)&g)` does not reproduce the
  codegen: GCC computes `&g` once into a held register and accesses `0(reg)` (and burns an extra saved
  reg in the prologue), instead of the reference's per-access `lui %hi(g); lw lo(g)` absolute addressing.
  Use the redeclaration, not the cast.
- **Diagnosis when a shared-header volatile flip SHA-misses.** A single-read **modulo/division**
  (`x % n`, `x / n`) is volatile-sensitive (it changes scheduling/CSE) — S126 `nucontrmbmgr.c`'s lone
  `nuScRetraceCounter % nuContRmbSearchTime` grew `.text` +0x10 under the flip, cascading
  `main_RODATA_END`/BSS by +0x10 into a ~21M-byte ROM diff; a plain reload-pair read is volatile-neutral
  (S126 `nugfxtaskmgr.c`). To find the grower fast, **map-diff the per-object section sizes**:
  `diff <(grep '\.text.*0x' base.map) <(grep '\.text.*0x' edits.map)` after a clean build of each — the
  one object whose `.text`/`.rodata` size changed is the volatile-sensitive consumer.

**Provenance:** S125 (the dead-reload / recompute-not-CSE tells + single-local capture on
nuScEventHandler); S126 (per-TU volatile — volatile in one TU, plain in banked siblings; the
modulo/division volatile-sensitivity + map-diff locator).

---

## -O0 boot/SDK-glue file profile (a per-file opt-level exception)

**Rule:** a few game/SDK-glue TUs shipped compiled at **-O0**, not the -O2 game profile (nor the -O3
lib profile) the rest of the segment uses. The nusys boot file (`src/libnusys/nuboot.c` = `nuBoot` +
`idle`, the cart entry) is the known case. -O0 codegen is unmistakable and is the tell:

- the **frame pointer is kept** (`addu $fp, $sp, $zero`; objdump `move s8,sp`, and the epilogue
  `addu $sp, $fp, $zero` / `move sp,s8`) — -O1+ omits it here;
- address loads are **recomputed inline**, not CSE'd into a saved register across calls;
- an **unused parameter is spilled** to its stack home (`sw $a0, 0xNN($fp)`);
- delay slots are still filled — so it is -O0 *with* `-fdelayed-branch` (the KMC default), not raw -O0.

**Trigger / symptom:** a clean-looking asm-first seed (the C logic is right) builds but the full-make
ROM SHA-1 misses, and an isolated disasm of the built `.o` shows the codegen tells above while the
target shows fp + recompute + arg-spill. This is an **opt-level mismatch, not a C-logic bug** — do not
iterate the C or reach for the permuter. Pin the level with the profile-probe
(`#profile-probe`); at -O0 the body matches
(only same-TU section-relative reloc reps may differ, e.g. `.text+0x6c` vs the `idle` symbol, which
link identically).

**Procedure:** add a **file-specific** mk override (a file target is more specific than the tree
pattern, so it wins): `$(BUILD_DIR)/$(SRC_DIR)/<tree>/<file>.o: C_PROFILE_CFLAGS := $(subst
-O2,-O0,$(CFLAGS))`, placed in the tree's `mk/<lib>.mk` (or a new fragment `include`d after
`mk/src.mk`). Never a `<tree>/%.o` pattern — the sibling TUs in the same tree stay -O2/-O3. S149
`nuboot.o` overrode to -O0 in `mk/libnusys.mk`, beating the `libnusys/%.o` -O2 pattern; `idle` was
byte-exact at -O0 (53 instrs).

**Provenance:** S149 (nusys boot file `nuboot.c` = `nuBoot` + `idle` at -O0; the per-file mk override
that beats the tree pattern).

---

## profile-probe (pinning the flags of a SHA-missing build)

**Use:** when a built `.o` SHA-misses and you suspect the **compile flags** (opt level, `-g`,
`-fdelayed-branch`, frame pointer), not the C. `tools/profile_probe.py --seg <seg> --func <fn>`
assembles the target subseg `.s` into a relocatable object and KMC-compiles the candidate
`src/<seg>.c` at each of a set of flag combos, then diffs the normalized `objdump -dr` (instruction
stream + reloc-symbol lines) per function — pinning the matching flags in seconds, no full make.

**Gotchas baked into the tool:** assemble the target with **modern GAS**
(`mips-linux-gnu-as -I include`), not `cpp -P | as` (cpp silently empties `.text` and the pipe
"succeeds") and not KMC `as` (rejects `.set gp=64`); the candidate uses the KMC `gcc -S` + KMC `as`
pipeline (`mk/src.mk`). Both objdumped with `mips-linux-gnu-objdump`, so the unresolved relocs (zeroed
fields) compare directly. Normalizer note: POSIX awk has no `\s` — use `[[:space:]]` (the `\s` bug made
the first comparison match only reloc lines and read as false 0-diffs). Pairs with
`#-o0-bootsdk-glue-file-profile`.

**Provenance:** S149 (the profile-probe grind: assemble the target with modern GAS; the
`\s`-vs-`[[:space:]]` awk-normalizer false-0-diff bug).

**TU-wide probe when >=2 alloc-artifact walls CLUSTER in one TU (S182).** When a single classical TU
banks its simple fns clean at `-O2` but two-or-more register-pressure-heavy fns each hit a distinct
regalloc-*artifact* wall (a DEAD frame the source can't force, a non-coalesced register copy the `-O2`
build coalesces away, a spill the build doesn't emit — the "MINE is more optimal than the target"
class), do NOT profile-probe-then-carry each fn in isolation. Run **one TU-wide profile-probe first**:
a single subtle flag/patchlevel that makes GCC coalesce-less or spill-more would flip MULTIPLE fns at
once, so the whole-TU probe is cheaper than N per-fn dives and, if it hits, converts several carries to
banks together. If the TU-wide probe finds nothing (the simple fns already pin the profile, and no flag
reproduces the target's less-optimal allocation), THEN the walls are genuine per-fn 2.7.2
patchlevel/build artifacts and each carries individually. S182 `func_8004D190.c` clustered a dead-frame
wall (`func_8004D4B8`) and a coalescing wall (`func_8004D5F0`) in one TU; both are the "more-optimal-
than-target" class. Cf. `func_80076640` (S181) and `func_8004DC44` (S172-S175) reaching the same
per-fn "2.7.2 patchlevel artifact" verdict. Pairs with `#dead-frame-reload-artifact-regalloc-wall` and
`#cross-project-matched-corpus-mining`.

---

## overlapping symbols — allow_duplicated (two instances share one official static name)

**Rule:** splat dedups `symbol_addrs.txt` by symbol name: two entries with the same name at different
vrams error `Duplicate symbol detected` (splat `util/symbols.py`, the `seen_symbols` map) unless both
entries set `allow_duplicated:True`. This bites when one official name legitimately recurs across two
instances — MG64's two nusys instances (the mapped libnusys + the game-embedded nuboot) each
have their own `IdleThread` / `MainThread` / `IdleStack` / `nuIdleFunc` / `idle` (file-statics in stock
nuboot, file-local per TU, but the curated name is shared).

**Procedure:** when naming an instance's symbol with an official name the second instance also uses,
append `allow_duplicated:True` to the `symbol_addrs.txt` entry. It is harmless when no duplicate yet
exists (splat only checks when a dup is present), and both entries need it, so set it pre-emptively on
the first instance to prepare for the second.

**Caveat — sync-managed names can't take the flag by hand.** `idle`/`mainproc` live in
`ghidra_symbols.txt` (sync-owned, never hand-edited), which carries no `allow_duplicated`. A future
second-instance `idle` cannot coexist via a hand-edit; it needs the sync tool to emit the flag, or a
`symbol_addrs` `rom:`-qualifier override path (cf. `#wrong-ghidra-name-override`).
Distinct from `#static-name-collision`
(same name, same TU, no global emitted, no action); here the names are placed globals at distinct vrams.

**Provenance:** S149 (MG64's two nusys instances — mapped libnusys + game-embedded nuboot; added
`IdleThread`/`MainThread`/`IdleStack`/`nuIdleFunc` with `allow_duplicated:True`).

---

## vendored-header inversion (a curated libultra header diverges from the ultralib pin)

**Rule:** a vendored libultra header can ship a macro that is wrong vs the upstream pin — not merely
reformatted but functionally **inverted**. `include/libultra/PR/os_host.h` had
`#define __osInitialize_common() osInitialize()`, the inverse of upstream's
`#define osInitialize() __osInitialize_common()`. The real VERSION_J symbol is `__osInitialize_common`
(Ghidra correct) and `osInitialize()` is the public macro over it; the inverted macro forced a
work-around (`#undef __osInitialize_common` + a non-upstream `INITIALIZE_FUNC __osInitialize_common` in
`src/libultra/os/initialize.c`) that silently deviated from the pin.

**Tell:** a public-API rename "resists" — you want to call the SDK public name (`osInitialize()`) but
the symbol is the `__*` internal, and `#wrong-ghidra-name-override`
does not fire (the name is C-defined, not a bare ghidra label, and the header macro is reversed).
Before assuming a ghidra mislabel, **diff the vendored header against `~/development/repos/ultralib`**
(the pin) — the header itself may be inverted. Fix the header to the upstream direction (trim to MG64's
only config, e.g. the `_FINALROM` branch) and revert the work-around to upstream; the ROM stays
byte-exact (the symbol name is unchanged).

**Audit (`tools/audit_libultra_headers.py`):** a macro-RHS diff of all 91
`include/libultra/**/*.h` vs the pin found only two real issues — this `os_host.h` inversion and
`rcp.h` `VI_CTRL_PIXEL_ADV_MASK` (`0x01000`, should be `0x0F000` for the [15:12] field; unused in-tree,
latent). Everything else was version-conditional branches MG64 matches (`EPI_SYNC`/`SELECT_BANK`
`>= VERSION_J`), value-equal cosmetics (`OS_STATE_*` `1` vs `(1<<0)`, `OS_MESG_TYPE_*` whitespace),
masked-vs-unmasked VI macros (identical for the fixed VI timings), or the moot `assert` `#EX` (asserts
compiled out — 0 `__assert` calls in `sched.o`). **Audit the macro RHS, ignore `#if BUILD_VERSION`
branches** (the tool can't evaluate them — they false-positive; hand-check each against the
`>= VERSION_J` branch).

**Provenance:** S149 (`os_host.h` `__osInitialize_common`/`osInitialize()` macro inversion +
`initialize.c` work-around; `tools/audit_libultra_headers.py` macro-RHS audit of all 91 headers,
which also flagged latent `rcp.h` `VI_CTRL_PIXEL_ADV_MASK`).

---

## double-sqrt fast-math (bare sqrt.d needs a per-file -ffast-math override)

**Rule:** a game/main TU that computes a magnitude with `sqrt()` (double) or `sqrtf()` (single) needs
a per-file `-ffast-math` override to emit the ROM's bare `sqrt.d`/`sqrt.s`. **Both precisions are the
same mechanism** — this is mode-agnostic; there is no separate single-precision path. Without
`-ffast-math`, KMC GCC 2.7.2 emits a guarded inline: the `sqrt.d`/`sqrt.s` opcode + a
`c.eq.d`/`c.eq.s $fN,$fN` self-equality (NaN) test + `bc1t` that falls back to `jal sqrt`/`jal sqrtf`
for the errno/domain path (needs a stack frame to save `$ra`). `-ffast-math` drops the errno/NaN guard,
leaving the bare unguarded opcode a leaf ROM fn has.

**Mechanism (verified vs KMC gcc 2.7.2 source):** `sqrt`, `sqrtf`, `sqrtl` are all registered as
`BUILT_IN_FSQRT` (`c-decl.c:3230-3232`), active by default unless `-fno-builtin` — so `sqrtf` is a
builtin, contra the old "sqrtf never inlines" claim. `expand_builtin` handles all three identically
(`expr.c:7243`, one `case BUILT_IN_FSQRT`; only the optab mode differs). At `! optimize` it calls the
library fn; otherwise `expand_unop` emits the backend insn — `sqrtdf2` / `sqrtsf2` (`mips.md:1497/1506`),
both gated `TARGET_HARD_FLOAT && HAVE_SQRT_P()` = `mips_isa >= 2` (`mips.h:463`), satisfied by `-mips3`.
Then `if (! flag_fast_math)` (`expr.c:7299`) it appends the NaN guard + errno `jal`. So `flag_fast_math`
is the only lever, for either precision.

**Tell:** the ROM fn is a leaf (no frame) whose only sqrt is a bare `sqrt.d`/`sqrt.s` with no
`c.eq.d`/`c.eq.s`+`bc1t` NaN guard and no `jal sqrt`/`jal sqrtf`. The default-profile build emits the
guarded form (extra frame + `c.eq`/`bc1t` + `jal` fallback) — e.g. `build/src/libultra/gu/align.o`
(libultra -O3, no -ffast-math) shows `sqrt.s` immediately followed by `c.eq.s`/`bc1t`, the reference
guarded shape.

**Procedure:** add a **file-specific** mk override (a file target beats the tree `%.o` pattern):
`$(BUILD_DIR)/$(SRC_DIR)/main/<file>.o: C_PROFILE_CFLAGS := $(MAIN_CFLAGS) -ffast-math` in
`mk/main.mk`. Declare the prototype in the TU (`double sqrt(double);` or `extern f32 sqrtf(f32);`); a
matching redeclaration keeps the builtin, and no `#pragma intrinsic` is needed. Never a `main/%.o`
pattern — the sibling main/ TUs keep the plain profile (`mgu/mtxutil`'s float math matched without
`-ffast-math`, so it is per-file like the `#-o0-bootsdk-glue-file-profile` override). `-ffast-math` is
the only errno-drop flag in this compiler (`-fno-math-errno` / `-funsafe-math-optimizations` do not
exist in 2.7.2). The `#pragma intrinsic(sqrtf)` in `include/libultra/PR/gu.h` is `#ifdef __sgi` (dead
on KMC) and irrelevant; the hand-written `src/libultra/gu/sqrtf.s` leaf is only the `-O0`/non-inlined
fallback, not the intrinsic path — do not reach for a "sqrtf-intrinsic path" for single precision.

**Provenance:** S152 (double `vector_magnitude_safe` / `calculate_hypotenuse_safe`, the range-scaling
fns); S153 (single `hypotf_2d`).

---

## top-tested-loop goto local-hoist (matching an un-inverted -O2 loop)

**Rule:** GCC 2.7.2 `expand_end_loop` (stmt.c, the "roll the entry test to the end" reorder) inverts
every structured top-tested loop at -O2 — a `while` / `for` / `for(;;)+break` whose test is at the top
becomes a guard-`j` + body-first + bottom test, and the delay-slot filler then annuls the back-branch
into a **branch-likely** (`beql`/`bnel`; `BRANCH_LIKELY_P() = mips_isa >= 2` in `config/mips/mips.h`,
so any `-mips2`+ target may emit it). When the ROM loop is **top-tested with plain `beq`/`bne`** (falls
straight into the test, a `j` back-edge, delay slots filled from fall-through, no branch-likely), no
structured loop reproduces it — reconstruct the control flow with explicit **`goto`s**. `loop.c` only
optimizes loops marked with `NOTE_INSN_LOOP_BEG`/`_END` notes, which only the structured loop
constructs emit (stmt.c `expand_start_loop`), so a goto-loop is invisible to `expand_end_loop` and is
never inverted.

**The hoist corollary:** because `loop.c` also ignores goto-loops, their loop-invariant constants are
not hoisted — the goto-loop re-materializes them every iteration (its `j` back-edge targets the
constant loads). To match a ROM that hoists them (constants in the preamble, `j` back-edge targets the
*test*), declare each invariant as a **local variable** initialized before the loop (`s32 off =
0xE0000000; u32 bound = 0xBFFFFFFF;`): the register allocator keeps the local live across the loop = a
manual hoist. The local-decl order also fixes the preamble load order (declare/use the one the ROM
loads first, first). Write the compare in the ROM's operand form (e.g. `bound < (u32)(x + off)`
reproduces `sltu vN, bound, x+off`, not the swapped `sltu vN, x+off, 0xC0000000`).

**The hoist corollary, inverted — when the ROM DOES hoist and the constant is COMPILER-generated
(S171).** The corollary above hoists *source-level* invariants via a local decl. But a
compiler-generated constant — the magic multiplier of a `/`/`%` by a constant (`0x66666667` for /10·k,
`0x1B4E81B5` for /4800), or a small literal like `' '` used in the loop — has no source variable to
declare, so a goto-loop CANNOT hoist it and re-materializes it every iteration. When the ROM hoists
these (magics/literals in the preamble, `j` back-edge targets the test), the fix is the OPPOSITE of a
goto-loop: use a **structured `while(1){ … if(exit) break; … }`**, which carries the
`NOTE_INSN_LOOP` markers so `loop.c` hoists the magic-constant loads — and when the loop's exit test
reads memory (`c = *p++`), `expand_end_loop` does NOT rotate it (the non-fixed memory read blocks the
roll-to-end, same predicate as `check_dbra_loop`), so the structured `while(1)` stays **top-tested**,
giving BOTH the hoist and the un-inverted shape. S171 `func_8004DAF4` (a scrollback console-puts with
`/40` + `%4800` + a `' '`-fill loop): the goto-loop rematerialized `0x66666667`/`0x1B4E81B5`/`0x20`
each iteration; swapping to `while(1){…break}` hoisted them to `t2`/`t0`/`t1` and matched byte-exact.
**Decide by what's hoisted:** ROM hoists a source invariant you can name → goto-loop + local decl; ROM
hoists a compiler magic/literal → structured `while(1)`; ROM hoists nothing (re-materializes) → plain
goto-loop.

**The SELECTIVE-hoist case — goto de-hoists a compiler magic the structured loop wrongly hoists
(S172).** A fourth case sits between "hoists everything" and "hoists nothing": the ROM does a
**pressure-limited partial hoist** — it hoists the loop-invariant *array bases* to held registers but
**re-materializes a compiler-generated `%`/`/` magic** at the loop tail each iteration (loop.c ran out
of hoisting registers after the bases and left the magic in the loop). A structured `do-while`/`while(1)`
hoists **both** (bases and magic → too many held constants); a plain **goto** outer loop de-hoists
**both** (loop.c skips it → bases re-loaded too). The fix that gets BOTH right (**S173 resolution of
the S172 "partial fix"**): keep the outer loop `goto` (de-hoists the magic → re-materialized at the
tail, matching the ROM), AND **pre-declare each array base as a pointer variable initialized before the
`goto`-loop label** (`u8 *grid = D_800DAF60; u8 *ring = D_800DB410;` … `outer: … grid[dst] … goto
outer;`). Because loop.c is invisible to the goto-loop, **program order is the only hoist mechanism**:
the base `la`/`lui+addiu` executes once (textually outside the back-edge region) and nothing re-derives
it — a manual base-hoist that leaves the magic correctly de-hoisted. So the goto is NOT a partial lever
here; base-pointer vars restore the base hoist. Declare the bases AFTER the entry guard (`if(n<=0)
return;`) so they land in the loop preheader (after the `blez`), like the ROM. S172/S173 `func_8004DC44`
(a ring-buffer→grid blit with `%4800` wrap): do-while hoisted `0x1B4E81B5` to `t1`; the outer-goto +
pre-declared base-pointer vars gives base-hoist + magic-remat with **operations 100% matching** — the
only residual is then a dead spill frame + register permutation, which routes to the permuter/carry
(`#dead-frame-reload-artifact-regalloc-wall`). **Tell:** a structured-loop build is byte-close but a
compiler magic is held in a register across the loop where the ROM re-loads `lui/ori` at the tail, AND
the ROM still holds other invariants (bases) hoisted — use goto + base-pointer vars, not a structured loop.

**Tell / distinguishing it from a bug:** the build is byte-exact except the loop is shape-shifted (the
shift/body block emitted before the test, a guard `j` to the bottom, `beql` where the ROM has plain
`beq`, and/or the `j` back-edge targets the constant-load block instead of the test). This is a
loop-form codegen mismatch, not a C-logic bug — do not iterate the condition expression or reach for
the permuter. Confirm the direction against a matched sibling: a single-condition ROM loop that is
inverted (guard + bottom test, e.g. `func_8005029C`'s `for(i != count)`, S151) proves inversion is the
compiler default, not a flag; a top-tested multi-`||` ROM loop that is not inverted is the goto case.
Pairs with `#double-sqrt-fast-math` (both were the same range-scaling fns).

**The reversal corollary (a distinct loop.c pass).** `expand_end_loop` inverts (above); a
separate `loop.c` pass, `check_dbra_loop`, reverses a structured count-only loop into a
decrement-and-branch. When the loop variable is used only to count (dead in the body), starts at 0, and
the trip count is a constant, `-O2` rewrites `for(i=0;i<N;i++)` to count down (`li vN, N-1` / `addiu vN,
-1` / `bgez`) because the down-test is one insn cheaper. When the ROM keeps the up-count (`addu
vN,zero,zero` / `addiu +1` / `sltiu vN, N` / `bnez`), no structured loop reproduces it — the `for` and
the `do-while` both reverse (verified `crc16_ccitt`'s 8-bit CRC loop, S154) — so the same goto-loop fix
applies: a goto loop carries no `NOTE_INSN_LOOP` notes and is invisible to `loop.c`, so
`check_dbra_loop` never reverses it, and the delay-slot filler still hoists the loop-top condition
recompute into the back-branch slot. Tell: byte-exact except the counter runs backwards
(`li vN,<N-1>`/`addiu -1`/`bgez` vs the ROM's `addiu +1`/`sltiu vN,<N>`/`bnez`). The reversal is blocked
by a `jal` in the loop, a non-fixed memory read, or any use of the counter in the body (`no_use_except_counting`
in `loop.c:5761`), so a natural loop matches when one of those holds — try natural forms first, goto only
after they demonstrably reverse.

**Sub-lever — `for(;;)`+`break` vs `while` for an UN-rotated top-test loop (S184).** Distinct from the
reversal above: when the ROM keeps a pointer/sentinel loop as a single top-test with an UNCONDITIONAL `j`
back-edge (loop top = the `lbu`/test; both the mid-loop `continue` and the tail `j` target it), a
`while((c=*s++)!=0){…}` build LOOP-ROTATES — GCC copies the exit test to the loop bottom (a second
`lbu … ; bnez … , top` re-entry), +2 instrs, so the child is over-long and shifts everything after it.
Re-spell as an infinite loop with an explicit break: `for(;;){ u8 c=*s++; if(c==0) break; … }` — an
already-infinite loop has no top test to rotate, so GCC emits the ROM's single top-test + `j`-back shape.
S184 nested `func_80043C20` (a bounded string-appender `while((c=*s++)) if(pos<end)*pos++=c;`) was 19
instrs as a `while` (rotated) and the exact 17-instr ROM form as `for(;;){…;if(!c)break;…}`.

**Preamble-order vs regalloc coupling — when the decl-order hoist lever can't decouple (S208).** The
hoist corollary says "declare/use the one the ROM loads first, first" to fix preamble load order. That
lever has a failure mode: when the two preamble loads target SPECIFIC coupled hard regs (a counter in a
v-reg vs a base pointer in an arg reg), flipping the decl order flips BOTH the schedule AND the
register assignment together, so you can get right-order+wrong-regs or right-regs+wrong-order but not
both from source alone. S208 `func_8005B070` (a backward 5-count sentinel search): the goto loop nailed
the exact instruction set + registers (`i`=`v1`, `p`=`a1`, plain `bne` back-edge, `addiu -4` in the
delay), but the ROM emits `li v1,5` BEFORE `la a1` while every right-regs source form emits `la` first;
declaring `i` first put `li` first but swapped the regs (`i`→`a1`, `p`→`v1`). A 1-instruction schedule
transposition with coupled regalloc = route to the permuter (it perturbs schedule and regalloc
independently), do not keep spelling source. **Method that proved it (S208):** compile 3–4 candidate
source spellings straight to `.s` with the project compiler — `COMPILER_PATH=tools/cc tools/cc/gcc -S
-G 0 -mips3 -mgp32 -mfp32 -mno-abicalls -O2 -o t.s t.c` on a tiny standalone snippet (stub the callees,
`extern` the globals) — and read the codegen side-by-side. It is a build-free, flowing-bss-immune
codegen oracle: it isolated the natural-loop peel (`bnel`) vs the goto-loop plain-`bne`, and exposed
the decl-order/regalloc coupling, without a single full `make`+diff cycle. Prefer it for any
shape/schedule/regalloc question before iterating in-tree.

**Provenance:** established: S152 (`vector_magnitude_safe` / `calculate_hypotenuse_safe` range-scaling
loops, shared with `#double-sqrt-fast-math`); reversal corollary: S154 (the `check_dbra_loop`
count-only reversal); rotation corollary: S184 (`for(;;)`+`break` for an un-rotated top-test loop);
preamble-order/regalloc coupling + the `gcc -S` codegen-oracle method: S208 (`func_8005B070`).

## decomposed-one-tu rodata alignment split (a counter-case to the 8-point decompose gate)

**Rule:** when the 8-point decompose gate splits a one-tu into N `.c`/`.o` files, the **last** piece's
`.rodata` can be followed **in memory** by the **next** TU's higher-aligned constant (a `.double`, an
8-aligned jumptable). The build force-4-aligns every asm `.rodata` section (`OBJCOPY_ALIGN :=
--set-section-alignment .rodata=4 ...` in `Makefile`, plus `ASFLAGS --no-pad-sections`), so the
asm-sourced next-TU rodata cannot self-8-align at the `.o` boundary: it lands right after the split
piece's rodata (which ends 4/8 bytes short of the boundary) and every downstream data symbol shifts.
The C `.o` recipe (`mk/src.mk`, KMC `as`) does not get that objcopy, so a C `.o`'s rodata keeps its
natural (higher) alignment.

**Tell:** clean per-function match (isolated diff shows only relocations), full-make SHA-miss, and
hundreds of **scattered** single-byte diffs across the whole text, each `built = base - 4` (or `- 8`)
— the low byte of a `%lo(D_xxxx)` for every data symbol past the shifted boundary. `verify-rom.sh`
fails while the `.o` is byte-perfect.

**Fix:** do not decompose a one-tu whose tail rodata abuts a higher-aligned next-TU constant — keep
the whole one-tu as **one** `.c` file. An internal higher-aligned constant (S154 func_8006A000's two
`2^31` cast `double`s, 16-align) makes the combined section 16/8-aligned, and KMC `as` pads its tail
to the boundary (S154 48B = 2 doubles + 2 string literals + pad, ending exactly at `0x800D1440` where
the next TU's 8-aligned double sits). This also keeps format strings as **actual** C literals rather
than `extern D_xxxx[]` refs into the generic asm blob (see
[[rodata-strings-as-literals-via-tu-combine]]). Splat's per-subseg `align:` is segment-level only
(gated by `ld_align_segment_vram_end: False`), so it cannot force an intra-section pad. Weigh
rodata-alignment adjacency before decomposing a one-tu at the plan gate.

## capturing $ra (return address) as a call argument

**Rule:** a ROM function that logs its caller's PC does `addu aN, $ra, $0` (copy the return-address
register into an arg reg) right after the prologue `sw ra`. `__builtin_return_address(0)` does not
produce this on KMC gcc 2.7.2: `RETURN_ADDR_RTX` is undefined for MIPS (`config/mips/`), so
`expand_builtin_return_addr` (`expr.c:7199`) falls back to a `MEM(frame + Pmode_size)` load — a
wrong-offset stack read (`lw aN, 4(sp)`), not the register. A `register u32 ra asm("$31")` reads `$ra`
with the correct `addu` encoding but confuses the prologue scheduler: it delays the `sw ra` below the
string-address load, so the delay-slot filler leaves an unfilled jal slot (`+1 nop`, everything shifts).

**Fix:** a `volatile` inline-asm read is the reliable form —
`__asm__ __volatile__("addu %0, $31, $0" : "=r"(ra));` then use `ra`. gcc allocates `%0` directly to
the arg register (giving the exact `addu aN, $ra, $0`), and the `volatile` barrier keeps the prologue
first so the format-string `addiu` fills the jal delay slot (ROM order; S154 `report_div_error`).
Tell you need this: the ROM reads `$ra` (reg 31) as a printf/log arg; the naive builtin emits a
stack-slot `lw`.

**Re-confirmed S176** (`heap_alloc` OOM `osSyncPrintf(fmt, ra)`): swapping the inline asm for
`__builtin_return_address(0)` on the exact function emitted `lw s5,4(sp)` (a `MEM(frame+4)` read,
`expr.c:7199`, since `RETURN_ADDR_RTX` is undefined for MIPS in `config/mips/`), NOT the ROM's
`addu s5,ra,0`. The `__asm__ __volatile__("addu %0, $31, $0" : "=r"(ra))` form is required; gcc parks
`ra` in a callee-saved reg (here `$s5`) because it is live across the intervening `jal`s.

## indexed-vs-pointer loop (strength-reduction preheader ordering)

**Rule:** for a sentinel-terminated (`!= -1`) array walk, the ROM's scheduling around the loop
(entry-branch delay-slot fill, and whether a loop-invariant constant is hoisted) depends on whether
the source iterates by **index** (`for(i=0; a[i]!=X; i++){ v=a[i]; use(v); }`, a `u32` index + a
value temp) or by **pointer** (`p=a; do{ use(*p); p++; }while(*p!=X)`). The two forms are
**byte-identical** in isolation (gcc strength-reduces `a[i]` to a pointer either way), so a small
isolated compile hides the difference — but they **diverge** inside a full TU. **Tell:** a
seg/segment/id-list walk (or any `-1`/sentinel-terminated array loop) that matches the whole
function except a 1-instruction swap in the loop preheader — a `move reg,base` vs a `li` constant
filling the entry `beq`/`bne` delay slot, or a loop-invariant `-1` hoisted to an outer loop's
preheader (an extra `li aN,-1` before the loop and a shifted scratch reg). When you see that,
**try the indexed + value-temp form first** before reaching for the permuter. S157
`load_overlay`/`unload_overlay`/`func_80025F18` all matched only in the indexed form
(`for(byte_index=0; seg[byte_index]!=-1; byte_index++){ byte = seg[byte_index]; ...}`), after the
pointer form left a `move v1,s2`-in-the-delay-slot miss (load/unload) and a `-1`-hoisted-to-a3 miss
(F18). Also fold multiple `if(cond) continue;` guards into **one** `if(a||b||c) continue;` when the
ROM uses a single combined test (F18's three range guards).

**Sub-lever — index a pointer variable, not the array symbol (S168).** Within the indexed form, the
walk must index a pointer *variable* (`Type *t = ARR; ... t[i]`), not the global array symbol
directly (`ARR[i]`). The symbol form keeps `&ARR` as a `%hi/%lo` constant and recomputes
`base + i*stride` every iteration (`lui;addu;lb 0(tmp)`, an extra insn/iter); the pointer-variable
form loads the base once and folds it into the strength-reduced giv (`lb 0(p)`, the ROM's form). Both
are "indexed", but only the pointer-variable spelling matches when the ROM folds the base into the
IV. S168 `func_80071220`'s 30-entry `tag==-1` walk matched byte-exact only after
`D_801B7118[i]` → `Entry *table = D_801B7118; table[i]` (the loop went from a base-reload each
iteration to the ROM's single `move v1,a0` dual-IV).

**Sub-lever — index-grouping `&base[i]` fixes the pointer-add operand ORDER (S171).** When a running
pointer is a base plus a computed index (`dst = base + row*40 + col`), C associativity groups it
`(base + row*40) + col` — the build adds `base` BEFORE `col`, so `base` is materialized early (into
whatever reg the allocator picks) and `col` stays live an extra step. If the ROM instead computes the
INDEX first and adds `base` last (`addu vN, row*40, col` then `addu ptr, vN, base`), it frees the
`col` arg-reg the instant the index is formed and REUSES it for the running pointer. Force the ROM's
order by grouping the index in a subscript: `dst = &base[row*40 + col]` emits `row*40+col` first, then
`+base`. S171 `print_string_at_grid`: the `&base[i]` spelling flipped the op order to match (row*40+col
then +base) — a prerequisite for the ROM's `dst`-in-`col`'s-register allocation. (A necessary op-order
fix, not always sufficient: the remaining allocno permutation may still need the permuter.)

**Sub-lever — dual-IV needs BOTH an explicit pointer AND the running offset (S171).** When a copy/scan
loop dereferences a pointer but ALSO needs the running integer offset — for a per-row `&arr[off]`
recompute across an outer loop, or an `off %= N` wrap — the ROM keeps a dual induction: a
strength-reduced pointer (giv) for the load/store AND the offset (biv) for the recompute/wrap.
Offset-only C (`arr[off]` with `off++`) makes gcc RE-INDEX `&arr[off]` each iteration (`lui;addu;lb`,
fewer setup insns but wrong shape); declare BOTH — `u8 *p = &arr[off]; … *p = …; off++; p++;` — to get
the ROM's pointer-increment inner loop plus the persistent offset. S171 `func_8004DC44` (renders N rows
from a ring buffer, offset wrapping `%4800` per row): adding explicit `dp`/`sp` alongside `src`/`dst`
took the opcode structure from 71→75 insns, byte-for-byte the target's dual-IV (the residual is then
pure allocno/frame permutation).

**Sub-lever — `&ARR[i]`-recompute de-biases a `combine_givs`-biased base register (S184).** A
struct-array STORE loop can byte-match the ROM in every way EXCEPT the base register's bias: the ROM keeps
the running pointer at the ELEMENT START (init `addiu base,%lo(ARR)` addend 0, positive field stores
`sw v0,0x18(base)`…`sb v0,0x30(base)`), but a carried `T *dst = ARR; …dst->f…; dst++;` build biases the
base to the MAX accessed offset (init `addiu base,%lo(ARR)+0x30`, NEGATIVE stores `sw v0,-0x18(base)`…
`sb v0,0(base)`) — same instruction count, same structure, only the immediates differ. **Cause:** with a
carried pointer, GCC 2.7.2 `loop.c` `combine_givs` reduces the per-field store-address givs
(`base+0x18`,`base+0x1C`,…,`base+0x30`) to ONE combined giv whose base = the first-in-list giv (the max
offset), expressing the rest as negative deltas. **Fix:** compute the per-element pointer INSIDE the loop
as a strength-reduced giv of the counter — `T *dst = &ARR[i];` (not carried `dst++`). The recompute form
keeps the biv at the element base (positive offsets = the ROM). Cracked `func_800444B8` (the
`D_800BB258[i]` copy loop, s0 biased +0x30) and `func_80043C64`'s per-club dump loop (s0 biased +0x1C) in
S184; both matched after `cs = &D_800BB258[club];` inside the loop. (Kin to the S168/S171 pointer-variable
levers above, but the tell is the base *bias immediate*, not a base *reload*.)

**Why (KMC gcc 2.7.2, grounded — verified against the source):** `scan_loop` runs
`move_movables` (invariant hoist, `loop.c:966`) before `strength_reduce` (`loop.c:976`). The hoisted
loop constants (the store value, the `-1` terminator) are inserted immediately before `loop_start`
by move_movables. Then:
- **Indexed `a[i]`:** the walk pointer is a strength-reduced *general* induction variable (giv); its
  initialization emits via `emit_iv_add_mult(bl->initial_value, …, loop_start)` (`loop.c:666`), also
  inserted immediately before `loop_start` — i.e. after the already-hoisted constants. Preheader
  order = `[li const][… ][move giv-ptr]`.
- **Pointer `p = a`:** `p` is a *basic* induction variable (biv); `move p,base` is original preheader
  code, sitting before the constants move_movables inserts. Order = `[move biv-ptr][li const]`.
- The delay-slot filler (`reorg.c` `fill_slots_from_thread`) fills the entry-check `beq` delay with
  the first preheader instruction → the **constant** (indexed, matches the ROM) vs the **move**
  (pointer, miss). The same reordering keeps a nested-loop `-1` from being hoisted to the outer
  preheader.

**Assembler note (binutils 2.6):** gcc wraps the branch + its delay slot in `.set noreorder` /
`.set nomacro`, so the assembler does not touch the delay-slot fill (it is gcc's reorg). binutils
only expands macros (`move`→`addu` `0x…21`, `li`→`addiu`/`lui+ori`) and schedules the `.set reorder`
spans. **False lead retired:** KMC gcc 2.7.2 ignores source line numbers — same-line source produces
byte-identical codegen (verified empirically), unlike IDO. So the permuter's `perm_sameline` pass is
a no-op for this toolchain; do not weight it (see `#permuter-setup-for-kmc-toolchain-mirrors`).

**gcc-source cross-ref (regalloc / scheduling misses generally):** ground a "structure is right,
scheduling/regalloc is wrong" miss in the KMC gcc 2.7.2 source at `~/development/repos/mips-gcc-2.7.2`
(memory `kmc-compiler-source-locations`): register assignment order is `global.c` `allocno_compare`
(priority `= floor_log2(n_refs)*n_refs/live_length * size`; higher → earlier hard reg → drove the
S157 param-reuse fix for the s0/s1/s2 rotation); loop-invariant hoist vs induction-var init ordering
is `loop.c` `move_movables`/`strength_reduce` (above); delay-slot fill is `reorg.c`
`fill_slots_from_thread`; loop inversion is `stmt.c` `expand_end_loop` and `loop.c` `check_dbra_loop`.

## pervasive-regalloc-classical-main

**Rule.** A structurally-correct classical fn that locks high on a pervasive hard-register
permutation is the whole-function register-allocation wall; resolve it with the ordered playbook
below.

**Trigger:** a classical (usually `src/main/`, game-O2) function whose C is **structurally** correct
(asm-differ rows all align, `match_count == total_rows`) but locks with a **high** score because the
register allocation is **pervasively** wrong (a systematic hard-reg permutation like `i:s4↔s5`, plus
scratch-reg swaps, spill-slot ordering, and delay-slot scheduling). Not a 1-2 instruction near-miss.
This is the hardest classical class.

**Triage tell — a pure-integer decoder/codec is this wall, not a "clean leaf".** At the plan
gate, `0-jal + no-float + no-rodata/data + args-only` reads as a **low-risk** seed-5 leaf — but a tight
decoder/codec (LZ/RLE, CRC, a bit-stream/ring-buffer walker: many back-to-back `sll`/`srl`, a
software-pipelined load-store copy chain, a self-recursive or ring-buffer index, a control-word bit
loop) is the **opposite**. The dense scheduling + a handful of long-lived pointers packed into `t0-t9`
puts it squarely on **this** wall, and it will need the permuter/fan-out, not a first-build match. Price
such a fn as a pervasive-regalloc unit (**seed 8+, expect permuter**), **not** seed-5. S164 mis-priced a
3-fn LZ trio (`lz_decompress_simple.c`) seed-5 and carried all 3 (`dma` matched only because its
residual was reloc artifacts; `simple`/`extended` reached structural-complete but locked on the
register permutation — `control` grabs `$v0` first via its short live range, target keeps it in `$a2`).
`pick_target.py` has no codec signal yet (tracked follow-up); until then the gate applies this by
reading the fn's shape.

**Resolution upgrade — this wall is source-steerable, not permuter-only.** The codec triage
tell was re-confirmed (both LZ leaves hit it), but a codec/decoder is no longer only "structural-
complete + carry": the loop-weight / live-length levers in `#loop-weight-and-live-length-regalloc-
steering` can carry it to a full byte **match** (S166 cracked `lz_decompress_simple`, the project's
hardest wall, 8600→0). Price it seed-8+/expect-permuter still, but before concluding "irreducible,"
run the loop-weight/live-length source levers **first** (they precede the permuter); the permuter closes
only the residual allocno-number tiebreak, and on a goto-loop fn it must run **safe-passes-only** (see
`#permuter-goto-backedge-liveness-unsound`).

**Seed-order sub-lever — a scalar/record init fn: seed in FIELD/ROW order, NOT Ghidra's statement
order (S189).** For a fn that is just a run of scalar global stores (a table/record initializer), the
Ghidra decompile reconstructs the **compiler-scheduled** store order, which has already collapsed the
reused-constant live ranges (it groups the two `= 0x800` stores adjacent so one register serves both,
etc.). Seeding in that order reproduces the *collapse*, not the ROM: GCC then packs the reused
constant into a scratch reg (`v0`) instead of the ROM's front-loaded dedicated reg. Re-seed in the
human **row/field order** (record[0].f0, .f1, .f2, record[1].f0, …): that spreads each reused
constant's first and last use apart so its live range crosses the intervening stores, forcing GCC to
front-load it into its own register (`li a0,0x800` / `li v1,-0x100` up top) exactly as the ROM does.
S189 `func_800676B0` (9 `s16` record-field stores + a `sw`) missed in Ghidra's order (`0x800`→`v0`,
collapsed) and matched first build in row order (`0x800`→`a0`, `-0x100`→`v1`, front-loaded). Tell:
byte-exact structure, only the *register/immediate* holding a reused constant differs, and the two
reuses of that constant are adjacent in your seed. Cheap to try before any deeper lever.

**The playbook (in order):**

1. **Build an exact-symbol isolated base.** Use **per-field** structs so each accessed field is its **own**
   base symbol at offset 0 (`Esc D_801B711A[]` for the score field, `Eho D_801B711C[]` for hole,
   etc.), **not** one combined struct + folded offset (`D_801B7118[i].score` = base+`i*stride`+2). Two
   reasons: (a) the decomp-permuter's scorer and asm-differ both count a reloc-symbol/addend mismatch
   (`D_801B7118+2` vs the target's `D_801B711A`) as a permanent nonzero **floor** even though they **link** to
   identical bytes, so exact symbols are required for the permuter to reach 0; (b) the offset-folding
   itself changes **scheduling** (see `#struct-access-folding-changes-scheduling`). The combined-struct
   form is fine for the final in-tree file (it links identically); use per-field only for the
   permuter/measurement base, then translate back.

2. **Apply ref-count / live-range levers — decl order is inert.** KMC gcc 2.7.2 allocates by
   `global.c` allocno priority (`floor_log2(n_refs)*n_refs/live_length`), **not** declaration order
   (verified S158: every decl permutation gave the identical object). So to flip which of two
   variables gets the earlier hard register, change a **ref count** or a **live range**, not the decl order.
   Levers that worked: **param/var-reuse** (clamp/compute into an existing arg in place instead of a
   fresh local — `if (arg0 > 0x12) arg0 = 0x12;` drops a `base` local; use `i - 1` inline instead of a
   `seq` counter — this is the S157 trick generalized); **init-early / compute-late** to lengthen or
   shorten a live range by even one instruction (S158 func_80068308: computing `lo` as the **last**
   prologue statement dropped its live range 27→26 insns, enough to outrank `found` for s2);
   **struct alignment** (align-2 the copied entry struct so a 6-byte struct-copy emits word+halfword
   `lwl/lwr`+`lh/sh` instead of word+`lb`/`sb`); **LICM-alias defeat** (store through a symbol-less
   const-pointer base `T *rp = D_XXXX; rp[j].field = …` so GCC's `memrefs_conflict_p` can't prove
   non-aliasing and does **not** hoist a loop-invariant load that the ROM recomputes — an indexed
   `D_XXXX[j]` with a distinct symbol disambiguates and wrongly hoists); **loop form** (for vs
   do-while vs pointer-walk changes the strength-reduced giv-init placement in the preheader);
   **operand/eval order** (swap `a > b` operands, or cast pointer arith to integer to force
   offset-first `addu`); and the **return type** (see `#return-type-is-load-bearing`).

3. **Then run the boosted-weight permuter** from the best structural base to close the residual
   spill-slot ordering + scheduling. Boost the stack-layout passes in `settings.toml`
   `[weight_overrides]`: `perm_reorder_decls`, `perm_pad_var_decl`, `perm_reorder_stmts`,
   `perm_temp_for_expr`, `perm_commutative`, `perm_randomize_function_type`. Run **annealing** (drop
   `--best-only`, which stalls on plateaus). If it plateaus, seed `base.c` with the current best
   `output-<score>/source.c` and re-run. S158's fns closed at permuter iterations in the hundreds to
   low-thousands **once** the structural base was right; from a wrong base they plateaued indefinitely
   (the permuter cannot invent the param-reuse / align / LICM levers — do those by hand first).
   **On a goto-loop fn, restrict `[weight_overrides]` to safe passes only** (`perm_reorder_decls`,
   `perm_pad_var_decl`, `perm_randomize_function_type`) and zero the var-reuse passes: the reuse
   passes produce semantically-invalid sub-floor minima that also poison the resume seed (S166, two
   instances; see `#permuter-goto-backedge-liveness-unsound`). Apply the source-side loop-weight /
   live-length levers (`#loop-weight-and-live-length-regalloc-steering`) **before** this step — they can
   carry the match on their own, leaving the permuter only the allocno-number tiebreak.

**Metric caveat (do not trust the score for "matched"):** the permuter's own score and asm-differ's
`current_score` weight **very** differently (S158: permuter 895 == asm-differ 6620 == 66 diff rows), and
**both** count link-identical reloc artifacts as false diffs — the per-field/combined addend
(`D_801B711A` vs `D_801B7118+2`) and the intra-file `jal` shown as a `.text`-relative reloc (isolated
target.o) vs a named-symbol reloc (in-tree). **Verify** a match by a **raw-instruction** diff of the **linked**
bytes (`objdump -d … | awk '{$1=""}'` both sides) or the full-make ROM SHA-1 — not the score, which
floors above 0 on these artifacts.

**Process — multi-agent fan-out is the tool for this wall.** Solo attempts + a solo permuter plateau;
S158 cracked it with a worktree fan-out (`isolation: worktree`): each agent **symlinks** the gitignored
toolchain (`tools/cc`, `venv`, `tools/decomp-permuter`) from the main repo into its worktree, `cp`s
the isolated base + builds the reference object, then sweeps logic-preserving structural variants
(measuring each) and runs a boosted permuter. The **systematic** order-sweep agent (many variants, each
measured) is what discovered the param-reuse / align / return-type levers the permuter alone could
not. Run one agent per remaining function.

**Fan-out robustness.** A shared-account **session-usage limit** can kill every agent mid-run with
no partial-result handoff (S164: all 3 agents died at once on the reset boundary). Harden the fan-out:
(a) each agent `cp`s its best `base.c` to a durable path (e.g. `scratchpad/best_<fn>.c`) on **every** new
best score, so a mid-run kill leaves the best state, not whatever was last written; (b) stagger/cap
the concurrent permuter agents (`-j 4` each, not `nproc-1`) to bound the account-usage burn; (c) the
usage window **resets** on a clock (the kill message names the reset time) — a killed fan-out can be
**re-launched** verbatim after the reset (resume from the on-disk `base.c`), so a limit-kill is a pause,
not a dead end. Agents that only touch their own `nonmatchings/<fn>/` + isolated `decomp_loop.py` +
their own permuter dir do **not** need worktrees (no `src/`/`make`/yaml writes → no shared-tree race);
skip the `isolation: worktree` cost when the work is measurement-only.

**Provenance.** whole-function regalloc wall established on all three non-trivial fns of a 5-fn one-tu
(`func_80067D40.c`: a 226-instr FP/trig/RNG generator, a 137-instr sort/rank, a 76-instr table
builder), all cracked: S158; codec-triage tell: S164; source-steerable resolution: S166.

**FP-camera-math sub-case: the extra-callee-saved-FP-reg product-hoist artifact (S188).** A camera /
projection FP-math pack (`set_camera_matrices_*`, RPY/matrix builders, perspective-project) is a
pervasive FP-regalloc/scheduling wall class, the FP analogue of the S183 integer dispatch-cluster.
The recurring irreducible tell: the ROM hoists ALL of a fn's products before any store (high
simultaneous FP liveness) and parks one product in a 6th callee-saved FP reg (`$f30`/`fs5`, frame
grows -0x48 → -0x50), while faithful C lets GCC interleave compute-and-store → 5 callee-saved regs +
a callee-saved permutation of the sin/cos values. This is **priority-driven** allocation: `config/mips/mips.h`
defines NO `REG_ALLOC_ORDER` (default ascending), so the assignment comes purely from live-range
priority, and it is **NOT forceable from source** — neither inline expressions nor explicit product
temps make GCC reserve the 6th reg (nothing crosses a call after the sin/cos, so the value doesn't
*need* callee-saved; GCC just prices it there). Confirm-don't-thrash: read the ultralib gu source for
the shape (e.g. `guRotateRPYF` for an RPY builder), verify the values/product-order match, then carry.
The permuter plateaus on this class (S188 `func_80065898`: 1470→670 over 117k iters, no match).
S188 banked the 3 structural fns (dot-product, 2D-indexed matmul, guMtxF2L-variant) and carried 8
FP-regalloc walls (`func_80065A1C` compiler-source-confirmed irreducible).

## loop-weight and live-length regalloc steering

**Rule.** The KMC gcc 2.7.2 allocno priority is steerable from C source along two independent axes,
loop-weight (the numerator) and live-length (the denominator); apply these source-side levers to a
structurally-complete pervasive-regalloc fn before concluding it is irreducible.

**Trigger:** a `#pervasive-regalloc-classical-main` fn that is **structurally** complete but locks on a
hard-register permutation the permuter alone plateaus on — specifically, the miss is **which** of two
values wins an earlier caller-saved reg. Before concluding "irreducible," the KMC gcc 2.7.2 allocno
priority `floor_log2(n_refs)*n_refs / live_length` (global.c:594-601) is **steerable** from C source along
two independent axes.

**Axis 1 — loop-weight (the numerator).** `reg_n_refs` is **loop-depth-weighted**: flow.c:2067
`reg_n_refs[regno] += loop_depth` (also :2315/:2501/:2711). `loop_depth` starts 1 (flow.c:434) and
increments +1 **only** at a `NOTE_INSN_LOOP_BEG` (flow.c:441), which `expand_start_loop` emits for **every**
structured `for`/`while`/`do` (stmt.c:2171) and which a `goto`-loop **never** emits (expand_goto ends in a
bare `emit_jump`, stmt.c:833). So a value referenced inside a **structured** loop gets its refs ×(depth); a
goto-loop body stays ×1. You can only **raise** priority via nesting (weight floor 1), never lower it.
  - **Asymmetric nesting decouples two registers.** Structure **only** the loop holding the value you want
    **higher** one level deeper; keep the competitor's loop a goto (shallower). A symmetric structure moves
    **both** together (verified: wrapping the whole outer dispatch in `for(;;)` weights control **and** the
    copy pointer equally, ratio preserved, no match). S166 seated `control`→$a2 by structuring **only** the
    inner decode-dispatch as `do { <inner literal while>; if(…) goto loop_top; … } while(1)` so
    control's decode refs weight ×2 while the 16-store raw-copy block (in the **outer** goto-loop) stayed
    ×1 — control overtakes the copy pointer for $a2. Exact flow.c:2067 differential.

**Axis 2 — live-length (the denominator).** priority = numerator / L, so **lengthening** a value's live
range **drops** its priority (counterintuitive; caching/shortening L **raises** it). Reference the value at a
later program point to lengthen L. But L can be **pinned**: `lz_decompress_extended`'s param is a binary
t1↔t8 switch (rc-alias present→t8, absent→t1, target t6 unreachable in between) because its 5 exit
stores are reachable only after the decode loop iterates, so param is irreducibly live across the whole
loop — a live-length **conflict** that is a genuine floor (carried raw-185).

**Numerator hygiene — web-split avoidance.** Write `x <<= k; x |= K;` (in-place self-assign = **one** web,
full n_refs) **not** `x = (x<<k)|K` (a nested subexpr spawns a temp → local-alloc splits the read-web from
the build-web → halves the value's n_refs). Same for pointer init: `p = base; p += n;` **not**
`p = (T*)(base+n)`. S166 needed the self-assign form on both the control-word build and the entry
pointer bump to keep control's refs high enough to clear the copy pointer.

**Zero-goto is impossible on a matched goto-heavy fn.** A goto-loop and a
structured loop are **not** interchangeable, for three independent, source-cited reasons — so an
all-structured rewrite of a matched goto fn generally cannot byte-match (S166, 3-agent source proof):
  1. **Loop-weight (above):** any structured loop emits the LOOP_BEG note → +1 depth → repermutes the
     outer body's hard regs (flow.c:2067 → global.c:594-601; MIPS defines no REG_ALLOC_ORDER, so
     find_reg takes the lowest free reg and a priority flip changes who claims it).
  2. **Loop optimizer:** loop.c only sees note-delimited loops (loop.c:349-361), so a goto-loop escapes
     invariant motion + IV strength reduction; a structured loop **always** runs `strength_reduce`
     (loop.c:975), adding preheader IV setup + shifting load/store displacements (~5 extra instrs).
  3. **No BB reorder:** gcc 2.7.2 `rest_of_compilation` (toplev.c:2647) has no basic-block-reorder pass;
     final.c emits in RTL-chain (source) order, so only a goto+label can place a block **off** the
     fall-through path (an out-of-line return, a two-branch dispatch join). Inlining such a block
     inverts the branch polarity (jump.c reverses the now-fall-through jump). Also: two separate
     `if(c) goto L;` emit two short-circuit branches; merging to `if(a||b)` / `if(!a&&b)` computes a
     combined boolean (xori/and) instead — **not** equivalent.
  Corollary: variable **renames** + **comments** are 100% codegen-neutral (verified via isolated `.s` diff +
  full-make ROM SHA-1), so the **cleanest matching** version keeps the compiler-forced control flow and
  improves only names/labels/comments (S166 reworked `lz_decompress_simple` this way, byte-exact). Do
  **not** "clean up" a matched goto fn by structuring its loops.

**Axis 3 — local-alloc pre-emption (make a call-crossing PARAM a global quantity, S176).** A
**parameter** (or any local) that is born in the entry block, dies early there, yet must survive an
intervening `jal` is a **call-crossing LOCAL quantity**: `local-alloc.c` `find_free_reg` picks
`call_used_reg_set`-avoiding regs (~:2103-2106) then scans hard regs **ascending** (~:2158-2182; MIPS
defines no `REG_ALLOC_ORDER`), so it parks the value in the **lowest free callee-saved reg, `$s0`** —
*before* global alloc runs. That pre-occupancy makes the true loop-heavy vars (`best_rem`, `best`)
conflict with `$s0` in `global.c global_conflicts`, so `find_reg`'s lowest-free-reg pass gives them
`$s1`/`$s2` and the param grabs `$s0` — a 3-way rotation vs the target. **Fix:** make the param a
**global** quantity so local-alloc skips it entirely: reference it in BOTH the entry block AND the
loop body by **mutating it in place** — `p = f(p);` (self-assign), NOT a fresh `q = f(p);`. Now
global alloc assigns all three purely by `allocno_compare` priority (`best_rem` > `best` > param),
and the lowest-free-reg pass hands out `$s0=best_rem, $s1=best, $s2=param` — the target. S176
`heap_alloc`: renaming param `size`→`need` and writing `need = (need + 0x17) & ~7;` in place flipped
`{need,best_rem,best}` off `s0/s1/s2` to the target `best_rem/best/need`, 119/119. The permuter alone
plateaued (1595→605); the source lever cracked it. Verify with the `.lreg` dump: the fixed version
shows **no** pseudo assigned to hard reg 16/17/18 in local-alloc (the param is now global). This axis
is complementary to Axes 1–2: those steer *global* priority; Axis 3 removes a *local-alloc*
pre-emption that pins the ordering before priority is even consulted.

**Diagnostic — the `-dg`/`-dl` allocno dumps.** Compile a candidate with `-dg` (global) / `-dl`
(local): `COMPILER_PATH=tools/cc tools/cc/gcc -S -G0 -mips3 -mgp32 -mfp32 -mno-abicalls -O2 -I include
-dg -o out.s cand.c` writes `cand.c.greg` (and `.lreg`). Read the header: `;; N regs to allocate: …`
is the **priority order** (highest first), `;; K conflicts: …` lists conflicting pseudos/hard-regs
(a bare number < FIRST_PSEUDO_REGISTER is a hard reg — `16`=`$s0`), `;; K preferences: …` is the
copy-preference, and `;; Register dispositions:` / `Register K in HH` is the final assignment. This
turns "which value wins `$s0`" from guesswork into a read: find the pseudo pinned to 16 in `.lreg`
(local pre-emption, Axis 3) vs a copy-preference for 16 in `.greg` (steer via Axes 1–2). Run it
**before** the permuter on any structural-complete register permutation.

**Axis 6 — expression eval-order / reassociation (a symbol-address base in a 3-term pointer sum).**
When the miss is "my var landed in `$a0`, the ROM keeps it in `$v1`", read `.greg`: the
`;; N conflicts:` line shows the var's global allocno conflicting with the wanted hard reg, and the
`;; Register dispositions` line shows where it went. S191 `func_80037E50`: `quality`(allocno 76)
conflicted with BOTH `$v0`(2) and `$v1`(3) → forced to `$a0`(4), because the return expression
`quality*K + base + arg3*K2` (with `base` a **symbol address**, i.e. a loop-invariant constant) was
**reassociated** by GCC to group the constant base with the variable term — `quality*K + (arg3*K2 +
base)` — which uses `$v1` as a **second accumulator** during `quality`'s live range. The **faithful**
fix is to write the pointer arithmetic **stepwise** so the eval order is pinned to a single
accumulator: `p = base + quality*K; return p + arg3*K2;` forces `(base + quality*K)` first
(complex operand `quality*K` evaluated first, then `+base`), freeing the other temp reg for the var
(→ `$v1`), then `+ arg3*K2`. A flat `A + base + C` expression lets GCC reassociate; stepwise
statements defeat it. This is byte-exact and permuter-free; it also removes the block-reorder step
(write the branch that falls through to the common tail last: `if (cond) return general; return
edge;`). Run it **before** the permuter on any extern-symbol-base pointer-return regalloc permutation.

**Process.** The lever set was **derived** by a compiler-source fan-out: 3 read-only mechanism-RE agents
over gcc-2.7.2 (flow/global/local-alloc/reload/sched/reorg/stmt/jump/loop/toplev) + N measurement
agents iterating from the isolated base — it turned a 3×-"irreducible" verdict into a byte match. See
`#compiler-source-fan-out-escalation-above-the-permuter`. This is the **source-side** precursor to
`#pervasive-regalloc-classical-main` step 3 (the boosted permuter): apply loop-weight/live-length
**first**; the permuter (safe passes only on a goto-loop) closes only the residual allocno-number tiebreak.

**Provenance.** S166 (source-side loop-weight/live-length levers; cracked `lz_decompress_simple`, the
project's hardest wall, 8600→0, carried and declared "irreducible" 3×). S176 (Axis 3 param-in-place +
the `-dg`/`-dl` diagnostic; cracked `heap_alloc`'s 3-way `{best_rem,best,need}` permutation the
permuter plateaued on at 605). S178 (Axis-5 define-point liveness + inline-sentinel; cracked
`heap3_alloc`, the S177 mask-rotation "wall", byte-exact with no permuter). S191 (Axis 6 eval-order /
reassociation `.greg`-read; cracked `func_80037E50`'s `quality` `$a0`→`$v1` via stepwise pointer arith,
byte-exact, no permuter).

## permuter goto-backedge liveness unsound (var-reuse passes corrupt live-across-backedge values)

**Trigger:** on a goto-loop function, a decomp-permuter "best" that beats your hand-derived structural
floor by a suspiciously large margin. decomp-permuter's liveness analysis does not model `goto`
back-edges, so its variable-reuse passes (`perm_temp_for_expr`, `perm_split_assignment`,
`perm_ins_block`, `perm_reorder_stmts`, `perm_condition`) treat a variable that is live across a
back-edge as a dead temp and reuse it — a semantically-invalid mutation that scores below the valid
floor but can never reach a valid 0, and (worse) poisons a resume seed so the wall looks reducible when
it is not.

**Two instances.** In S166, `lz_decompress_simple` hit raw-104 vs a valid floor of 146 via
`acc=*p; uVar2=acc` clobbering the live control word; `lz_decompress_extended` hit decomp_loop 20400 /
perm 3805 vs a valid floor 208 via `perm_split_assignment` rewriting the live ring index
`ridx = ridx+1 & 0x7ff` into a constant-2 clobber across every back-edge — a corrupt resume seed that
mis-measured the wall for a full sub-phase until caught.

**Fix/rule.** On a goto-loop fn, restrict `[weight_overrides]` to semantics-safe passes only
(`perm_reorder_decls`, `perm_pad_var_decl`, `perm_randomize_function_type` — these only renumber
allocnos, cannot change semantics) and set the var-reuse passes to 0. Always re-verify any sub-floor
permuter output with a manual liveness check across every goto back-edge before trusting it as a match
or a resume seed. Bonus: allocno-renumber-only is the correct tool for a pure register-permutation
residual anyway (what a `#pervasive-regalloc-classical-main` tail usually is). See
`#pervasive-regalloc-classical-main` step 3 and `#permuter-setup-for-kmc-toolchain-mirrors`.

**Axis-4: caller-saved competitor count (a constant loop-invariant is a competitor a variable one is
not; S177 `func_8004E2DC` = WALL).** When a call-crossing loop-invariant (e.g. a heap `head`) is a
**variable** address (`&arr[i]`), it lives in a CALLEE-saved reg (it crosses the `osSetIntMask` call →
`$s3`), so it is NOT a caller-saved competitor for the loop's temps. The SAME value as a **constant**
(`&arr[3]`) is re-materializable, so GCC keeps it caller-saved — an extra competitor in the loop. That
one extra competitor flips a call-argument copy-preference: with N competitors an interrupt-`mask`
(live whole-fn, copy-prefers `$a0` from `osSetIntMask(mask)`) loses/wins `$a0` differently. The
variable-index `heap_alloc` matched (5 caller-saved competitors, `mask`→`$a0`); the slot-3 constant
`heap_alloc` has 6, and the ROM's 6-value allocation needs `mask`→`$t1` / `bsize`→`$a0`. **S177 declared
this a WALL ("no clean lever; carry"). S178 REFUTED it — the clean lever is Axis-5 below.** The S177
error was fixating on `bsize`'s missing `$a0` copy-pref and testing only "force head callee-saved" (the
wrong polarity); the real steer is head's DEFINE POINT relative to the call. **Diagnosis:** count the
caller-saved values live across the loop; a constant that "should" be a base pointer but is caller-saved
is the tell.

**Axis-5: define-point liveness (a constant crosses a call → callee-saved; define it AFTER the call to
keep it caller-saved, S178 `func_8004E2DC`).** Whether a re-materializable constant loop-invariant
(`head = &heap_slots[3]`) is caller- or callee-saved is controlled by WHERE in the source it is first
materialized relative to the guarding call. Define it BEFORE `osSetIntMask(1)` and GCC keeps it live
across the call → callee-saved `$s3` → bumps `ra` to a 6th saved reg → displaces `mask` onto `$a0`
(the S177 near-miss). Define it AFTER the call and it stays caller-saved → `ra`→`$s3` → `mask`→`$t1`
(the ROM). So MOVE the `head = &arr[K]` (and any anchor-pointer setup) to just below the call. This is
complementary to Axis-3 (which controls a call-crossing PARAM); Axis-5 controls a call-crossing CONSTANT
by its materialization point. **Then close the residual head↔const swap with the INLINE-SENTINEL form:**
drop the `head` local entirely and write the loop guard as `block != &arr[K]` off the same base as the
`.next` load, so CSE derives the sentinel with one `addiu v1,v1,-8` and the allocator copies it to the
loop reg (`move t0,v1`) exactly as the ROM does (a precomputed `head` local, OR a `&arr[K]-off` anchor,
instead pins the sentinel into one reg with no copy, one instruction short). S178 `heap3_alloc` banked
byte-exact from these two levers — no permuter, no cross-project mining. Verify with the `-dg` dump:
`mask` should show hard-reg 9 (`$t1`), not 4 (`$a0`).

## return-type is load-bearing

**Trigger:** a `void`-semantics function (no used return value; the ROM falls off the end) whose C is
otherwise structurally correct still mis-allocates at the loop-entry / a delay slot, and the miss
resists every body-level lever.

**Fix:** declare the function `s32` (not `void`), with no return statement. A non-void return type
reserves `$v0` as the return register, which changes the register allocation / delay-slot fill enough
to match a ROM whose original TU returned an (unused) value. `func_80067D74` matched only with `s32`
return (S158); `void` differed at the entry-`beqz` delay slot; `s32`/`u32`/`long long` all matched
(`s64`/`u64` fail to compile with no return), so use the clean `s32`. Found via the permuter's
`perm_randomize_function_type` pass — weight it up when a void classical fn won't close.

## struct-access-folding-changes-scheduling

**Trigger:** a struct-array function that byte-matches when written with per-field base symbols but
not when written with a combined struct, even though the two forms link to identical addresses.

**Cause:** for a combined global struct array, `D_801B7118[i].score` folds the field offset into the
`%lo` (`sh v, %lo(D_801B7118+2)(hi+i*stride)`, addend 2); a per-field symbol `D_801B711A[i].score`
uses `%lo(D_801B711A)` (addend 0). The linked bytes are identical, but GCC 2.7.2's instruction
selection / scheduling for the two address forms differs (S158: the combined form changed the
loop-entry `beqz` delay-slot fill on `func_80067D74`). When a struct-array classical fn won't match,
split the accessed fields into separate per-field base symbols (`Eid@D_801B7118` / `Esc@D_801B711A` /
`Eho@D_801B711C`, each a struct whose field is at offset 0). A 6-byte struct copy can still use one of
these (align-2 the type so the copy emits `lwl/lwr`+`lh/sh`).

**Param-struct at the gate — check Ghidra, but verify vs the asm.** When a classical fn takes a
`<T> *param_1` accessed as `param_1[N]` + `*(u16 *)(param_1 + k)` casts, its original source likely used
a real struct, and struct member access (proper field types) can change instruction selection /
scheduling / regalloc vs int-array indexing. So at the plan gate, query Ghidra for an existing param
struct (`search_data_types`, `get_struct_layout`). Two cautions: (1) the Ghidra struct may be mis-RE'd
— S164's `LzDecompressState` was marked packed (align 1, `ring_buffer@0x16`, size 0x24) but the asm
ground truth is naturally aligned (`flags@0x14` u16 + 2 pad, `ring@0x18`, `ridx@0x1C` u16, `run@0x1E`
s16, `hidx@0x20` u16, `hbase@0x24`, size ≥0x28) — reconcile field offsets/alignment/size against the
load/store widths in the asm before trusting it (surface a corrected layout as a cross-repo Ghidra
follow-up). (2) It is not a cure-all: struct-typing S164's `lz_decompress_simple` param (`u8*` fields)
gave the identical score + register permutation, because that fn's miss was internal allocno priority
(a control var, not a param field). Test it, but if the diff shows the permutation is on internal
temps/pointers (not the param loads), the struct won't move it — go to the permuter/fan-out.

**`(&PLACED)[-N]` for a struct-folded global that is NOT a placed symbol (S183).** When the ROM accesses
an UNNAMED address via a base derived from a nearby PLACED symbol — e.g. `s7 = &D_801B60A0 - 0xC` then
`lw 0(s7)` (= `D_801B6094`, unnamed) and `lw 4(s7)` (= `D_801B6098`) — writing `extern s32 D_801B6094;`
link-fails (splat never generated that `D_` name; nothing references 0x801B6094 by symbol). Reference it
as a **negative index off the placed neighbor**: `(&D_801B60A0)[-3]` (= addr 0xC below, an `s32`) and
`(&D_801B60A0)[-2]` (= `D_801B6098`). This BOTH compiles (resolves through the placed `D_801B60A0`) AND
reproduces the ROM's shared `base - 0xC` derivation (GCC CSEs `&D_801B60A0` and offsets from it), instead
of re-materializing each `%hi/%lo` separately. Keep any access the ROM does DIRECTLY (`%hi/%lo(D_801B6098)`
in a pre-loop bound check) as the plain `D_801B6098` symbol — the negative-index form is only for the
struct-folded (base-relative) accesses. S183 `func_800525C4` reproduced the frame + all reg-saves + the
`addiu base,base,-12` this way (residual was a downstream register-permutation cascade, a
`#pervasive-regalloc-classical-main` wall, not the addressing).

## switch-jtbl-dispatch (compiler jump table + sparse inner cases)

**Context:** a classical fn that dispatches on a small dense index (`switch (x)` with cases `0..N`,
`sltiu x,N+1` bound-check) via a compiler-generated `.rodata` jump table (`jtbl_<vram>`, `jr $v0`),
often with a per-case sparse secondary dispatch returning constants. `func_80051E90` (a
course/hole yardage lookup: `switch(course)` over 8 cases, each a sparse `hole` dispatch returning
golf-yardage constants, default 200).

**Three levers for a byte-exact match:**

1. **`switch` for the jtbl dispatch only; `if`-chains for sparse inner cases.** A `switch` on the
   dense outer index emits the jump table you want (one `jtbl_<vram>` to carve). But a `switch` on the
   sparse inner values (e.g. `hole ∈ {2,4,9,10,11,12,13,16}`) risks gcc emitting a second table (a
   `casesi`/range table), which is a second rodata blob to carve and a different code shape. Write the
   sparse inner dispatch as an `if (x == K1) …; if (x == K2) …;` chain so it always compiles to a
   linear `beq/bne` comparison chain (gcc sorts the tests ascending, matching the ROM's order). Net:
   exactly one compiler table in the TU.

2. **`a == K1 || a == K2` compiles branchless — split it into two `if`s.** For a case that returns the
   same value for two inputs, `if (a == 10 || a == 16) return V;` compiles to a branchless
   `xori/sltiu` per test + `or` + one `beqz` (a bitwise merge, no short-circuit). If the ROM uses the
   short-circuit branch form (`beq a,10,ret; … bne a,16,default`), write two separate statements
   (`if (a == 10) return V; if (a == 16) return V;`). This also lets gcc cross-jump the second test's
   "check-K-else-default" tail into a sibling case's identical block (S159: case 0's `bne hole,16 →
   default` merged into case 5's `bne hole,17 → default`, with the compare constant riding in the
   shared `v0`). The cross-jump is automatic once the tail shapes match; you only need the branch
   (not branchless) form.

3. **`.rodata` sibling carve for the jump table (see
   [#rodata-sibling-yaml-pattern](#rodata-sibling-yaml-pattern)).** The compiler table lands in the C
   object's `.rodata`; carve it so it places at the ROM's `jtbl_<vram>`. The text flip is the gate
   enabler; the rodata carve lands at body time (a stub emits no rodata). Split the generic rodata
   subseg around the table's extent (S159: `jtbl_800CCC30` = 8 × 4B = 0x20 at rom 0xA8030, 8-aligned,
   flanked by unrelated strings → `[0xA8030, .rodata, main/func_80051E90]` + `[0xA8050, rodata]`
   tail). This is the first carve of a compiler switch table (prior carves were FP-literal /
   const-array rodata); the mechanics are identical (attribute + split at 16/word-aligned bounds).

4. **The switch value's SIGNEDNESS picks `sltiu` vs `slti` for the bound-check (a one-instr lever).**
   The dispatch bound-check is `sltiu x,N+1` when the switch value is UNSIGNED and `slti x,N+1` when
   SIGNED. So a per-fn build that is byte-exact EXCEPT a lone `slti`↔`sltiu` at the switch entry is a
   **global-typedness** fix, not a control-flow one: retype the switch-value `extern` (`u32` → `sltiu`,
   `s32` → `slti`). Applies to any range/bound compare on a global, not only jtbl dispatch. S186
   `func_8006955C` (sparse mode switch, cases 0/6/10): `D_800BA9FC` retyped `s32`→`u32` gave the ROM's
   `sltiu`; the whole branch-chain dispatch (incl. the `beql` branch-likely delay slots) was already
   byte-identical, so the type was the sole residual.

**Provenance:** S159 `func_80051E90` (2/2 fns, no permuter; all three levers + the operand-order and
branch-likely nudges in [#register-reuse-nudge-classical-regalloc](#register-reuse-nudge-classical-regalloc));
S186 `func_8006955C` (lever 4, sltiu/slti signedness).

## offset-0-symbol re-materialization (fixed-global field RMW)

**Trigger:** a clean classical/mirror fn is byte-exact except a read-modify-write on a **fixed global
struct/array field** at a NON-zero offset. The ROM **re-materializes** the address (`lui r,%hi(SYM);
lw r,%lo(SYM)(r)` … `lui at,%hi(SYM); sw v0,%lo(SYM)(at)`), but the build folds it into a shared
**base register** (`la $t, ARR+off; lw 0($t); …; sw 0($t)`), and that base reg reuse cascades a
register permutation through the rest of the fn.

**Mechanism (KMC GCC 2.7.2, dumped from cse.c + global.c).** GCC re-materializes `%hi/%lo` ONLY for a
`symbol+0` address. Any `symbol+offset` — an array element `ARR[k].field` or a struct field at a
non-zero member offset — is an rtx CSE recognizes as a common sub-expression and hoists into a `la`
base register that it REUSES across the load and the store (and any sibling field access). That base
register is one more caller-saved competitor, so it also shifts the surrounding allocation.

**Lever.** Reference the offending field as its OWN offset-0 `extern`, aliasing the array-element
address:
```c
extern Slot D_800DC6E0[];
extern s32  D_800DC738;        /* == &D_800DC6E0[3].total (offset 0x58), its own offset-0 symbol */
...
    D_800DC738 += block->size;  /* re-materializes %hi/%lo; NOT `D_800DC6E0[3].total += …` (base reg) */
```
The final link resolves `D_800DC738` and `D_800DC6E0+0x58` to the same address, so the bytes are
identical; the only change is the addressing FORM the compiler picks, which re-materializes and
un-reserves the base register. This frequently **cascades the whole allocation into place** for free
(S177 `heap3_free`: the offset-0 `D_800DC738` for `total +=` re-materialized AND pushed `block->prev`
→`$a3`, `mask`→`$t0` — the ROM's exact assignment, from that one edit).

**Scope + non-firing note.** A field accessed ONCE (a plain read, or a write-only store) already
re-materializes in the array form (`D_800DC6E0[3].unk_14 = max` emits `lui/sw %lo`), because a single
access has no common sub-expression to fold — so leave those as the struct/array form and only switch
the RMW (`+=`/`-=`) field. Multiple accesses to the SAME field across disjoint branches also
re-materialize (no CSE across the branch). This is the INVERSE of
[#mem-in-struct-scheduling-lever](#mem-in-struct-scheduling-lever) (which retypes a fixed global AS a
struct member to change scheduling); here you split a struct field OUT to its own symbol to change the
addressing form. Provenance: S177 `heap3_free`, found by a GCC-source subagent fan-out (see
[#compiler-source-fan-out-escalation-above-the-permuter](#compiler-source-fan-out-escalation-above-the-permuter)).

**Naming the alias + splat overlap (S178).** When the alias is promoted to a curated name, the
offset-0 symbol overlaps the enclosing array symbol (e.g. `heap3_total_free`@0x800DC738 sits inside
`heap_slots[4]`@0x800DC6E0). splat TOLERATES the overlap (it truncates the enclosing symbol with a
`Range check triggered` WARNING and keeps the alias separate), but to keep `make extract` clean, size
the enclosing symbol to STOP at the first alias: `heap_slots = 0x800DC6E0; // size:0x58` +
`heap3_total_free = 0x800DC738; // size:0x4` + `heap3_largest_free = 0x800DC73C; // size:0x4` partitions
the 0x60 array's last two words as the aliases with no warning. The C array-form access
(`heap_slots[3].total_free` = `heap_slots+0x58`) still link-resolves through the addend; only the
compiler-chosen form differs (verify by full-make SHA-1).

**Converse (S187): an INDEXED struct-array field folds to the splat per-field symbol for free.** When
splat has named each field of a fixed struct-array as its own `D_<addr>` symbol (e.g. the group table
at `D_80105140` where `.count`@+4 = `D_80105144`, `.unk08`@+8 = `D_80105148`, … each got a distinct
`D_` name), you do NOT need to declare or reference those per-field externs. Type the base as the
struct array (`extern SparkGroup D_80105140[];`) and write `D_80105140[i].count = 0`: the compiler
emits `lui at,%hi(D_80105140); addu at,at,<i*stride>; sw ...,%lo(D_80105140+4)(at)`, and `%lo(base+4)`
link-resolves to exactly the field's own `D_80105144` symbol in the disassembly. So the index-register
form (`%hi/%lo(base) + i*stride` with the field offset in `%lo`) is the SAME bytes whether the source
names the field symbol or the struct member — write the struct member. (This is the loop/indexed
counterpart of the single-fixed-field RMW lever above: there you SPLIT a field out to its own symbol to
force re-materialization; here the struct-fold already reproduces the per-field symbols, so keep the
struct.) S187 `func_80077C18`/`func_80077DEC` (the `D_80105140` `SparkGroup[]` table).

## volatile-view CSE reload (force a just-stored global to reload)

**Trigger:** a clean classical/mirror fn is byte-exact except the ROM **RELOADS a global struct field
right after storing it**, with NO intervening varying-address store to invalidate it — most often a
self-referential list init `x.prev = &x; x.next = x.prev;` where the ROM does `sw v1,prev; lw
v1,prev; sw v1,next` (reload) but the build does `sw v1,prev; sw v1,next` (forwards v1, one load
short). Distinct from the child-fn case where an intervening `node->field = …` store through a runtime
pointer legitimately triggers the reload via CSE varying-address invalidation.

**Mechanism (KMC GCC 2.7.2, cse.c).** Store-to-load forwarding of a plain absolute-addressed global IS
the -O2 default: a store enters its dest MEM into the CSE table equivalenced to the stored value
(`cse.c:7358`), so a later read of the same `symbol+0` address forwards the register — no reload. A
faithful `next = prev` therefore reuses the stored reg. There is NO non-volatile source that both keeps
the absolute stores AND reloads: forcing the reload by un-aliasing the read (separate symbol) makes the
scheduler HOIST the read above the store (stale value), and register pressure does not trigger it.

**Lever — a per-ACCESS volatile view.** Cast the lvalues to `volatile`-qualified pointers so the struct
itself stays non-volatile (siblings unaffected):
```c
*(volatile s32*)&x.size  = 0;               /* volatile STORE: pins ahead of the reload */
*(volatile s32*)&x.state = HEAP_BLOCK_HEAD; /* volatile STORE */
*(T* volatile*)&x.prev   = &x;              /* volatile STORE */
x.next = *(T* volatile*)&x.prev;            /* volatile READ = the RELOAD (do_not_record) */
x.total = 0;                                 /* non-volatile: fills the reload's load-delay slot */
```
A `MEM_VOLATILE_P` read hits `do_not_record` (`cse.c:1942`) so it is never looked up/forwarded → the
load survives as the reload, AND (unlike the separate-symbol trick) it keeps the true store→read
dependency so the load stays adjacent to the store in the right register. **The non-obvious part
(scheduling):** GCC 2.7.2's list scheduler creates NO dependency between a volatile store and
independent NON-volatile stores, so any field that must schedule AHEAD of the reload (here size/state)
must ALSO be volatile — else it floats down into the reload's load-delay shadow. Leave the delay-slot
filler field (here total) non-volatile. **Provenance:** S178 `heap3_init` (the S177 CSE-reload "wall",
refuted). Found by the ADVERSARIAL agent of a two-agents-per-wall fan-out
([#compiler-source-fan-out-escalation-above-the-permuter](#compiler-source-fan-out-escalation-above-the-permuter));
the primary cse-only agent tried volatile-on-prev-only, saw it float, and wrongly declared "unreachable".

## mem-in-struct scheduling lever (model a fixed global as a struct/array member)

**Trigger:** a classical fn's global load/store schedules differently than the ROM and every body
lever fails. Two shapes: (a) the compiler pipelines independent global-load/pointer-store pairs into
several FP scratch regs (`$f0/$f2/$f4`) where the ROM keeps strict pairs reusing one reg (`$f0`); or
(b) the compiler hoists a plain global load (e.g. a `& K` flag test) above a pointer store the ROM
keeps late (so the ROM leaves the guard-branch delay slot a `nop`, and the build fills it + shifts
the register allocation).

**Cause (gcc 2.7.2 `sched.c`, the memory-dependency model the instruction scheduler uses):**
`true_dependence` (~line 817) treats two MEMs as independent (reorderable) when one is `MEM_IN_STRUCT`
at a varying address and the other is non-`MEM_IN_STRUCT` at a fixed address (the rule commented at
`sched.c:797`). A pointer store `*(T*)p` is MEM_IN_STRUCT + varying; a scalar global `D_xxx` is
non-struct + fixed. So the scheduler judges `store-via-pointer` and `load-of-scalar-global`
non-conflicting and freely reorders/hoists. `memrefs_conflict_p(symbol, reg)` itself returns 1
(may-conflict), so the MEM_IN_STRUCT terms are the sole discriminator.

**Fix:** make the fixed global a struct/array member so its load becomes MEM_IN_STRUCT → the term
flips → the pair conflicts → the scheduler serializes (strict pairs) or cannot hoist (late load).
Concretely: a triple of contiguous floats read as a vector is a `Vec3f` global (`extern Vec3f g;
… g.x/g.y/g.z`); a lone flag word becomes `extern u16 g[]; … g[0] & K` (a 1-element array is
MEM_IN_STRUCT via `ARRAY_REF`). The store side is usually already MEM_IN_STRUCT (`p[i]`, `p->f`), so
only the load side needs retyping. S162 used this twice in one file: `func_80076500` (six scalar
globals → two `Vec3f` constants → strict `$f0` pairs) and `func_80076558` (`D_800FBDA6` → `[0]`
struct-flag → late load → `nop` in the guard delay slot → i allocated to `a1` → exact 58-instr match).
No permuter; found by reading `~/development/repos/mips-gcc-2.7.2/sched.c`.

**Extends to DISPLAY-LIST fns (S180), not just classical scheduling misses.** A global-`glistp++` DL
fill fn whose fill color comes from N separate `D_` global scalars hoists the color loads into the
`glistp` load-shadow (the [#display-lists](#display-lists) scheduler-load-pair wall) for exactly this
reason: plain-scalar color loads no-alias the `mem/s` `*glistp` stores → free roots → the sticky
`LAUNCH_PRIORITY` birthing-boost floats them to the shadow. Retype the color scalars as one
`Color {s32 r,g,b;}` struct (or `s32[3]`) → the loads become `mem/s`, may-alias the `*glistp` writes,
and are pinned after the stores = the ROM's deferred schedule (S180 `func_800329F4`, byte-exact, no
permuter). The mechanical **tell it is one struct**: a sibling setter writes the N globals as N
consecutive words (`func_800329D8` writes `D_800B7840/44/48` from `a0/a1/a2`). Second required
ingredient for a DL fill: the color compute must land after the fill-color w0 store (inline the pack
in the `gDPSetFillColor` arg — the demo `gfxClearCfb` idiom — or use a temp computed after w0). See
[#display-lists](#display-lists) for the full DL playbook.

**Extends to CSE-invalidation RELOADS (S186), a second pass that reads the same `MEM_IN_STRUCT_P`
flag.** Same lever, different symptom + different compiler pass. When a fixed global is READ, then a
**nonscalar** store `arr[runtime_idx] = 0` runs, then the global is READ again with no intervening
store to it, the ROM RE-LOADS the global (2 `lw`s) but the build CSE-forwards it (1 `lw`, the cached
value reused — 1 load short, and the reg it was kept in cascades). This is `cse.c`, not `sched.c`:
`note_mem_written` (~7538) grades a varying-address store into `{sp,var,nonscalar,all}` — an
`int_array[var]=0` lowers (expr.c ARRAY_REF→INDIRECT_REF, ~4620) to a `(mem/s (plus reg const))` and
is graded **`nonscalar`** (MEM_IN_STRUCT + non-QImode, so NOT `all`). `invalidate_memory` (~1700)
then purges a cached load `p` only when `all || (nonscalar && p->in_struct) || cse_rtx_addr_varies_p`.
A bare-scalar `extern s32 G;` load has `in_struct=0` → the `nonscalar` store does NOT purge it → CSE
forwards (build, 1 load). Read the global as an **array element** (`extern s32 G[]; … G[0]`) → its
load gets `MEM_IN_STRUCT_P` → `nonscalar && in_struct` is true → purged → the second read RELOADS
(ROM, 2 loads). **`volatile` is the WRONG lever here** — it hoists the reload ABOVE the store into a
preserved reg (does not match the ROM's load-after-store). The offset-0 array form (`G[0]`) is the
byte-safe minimal fix (identical `%hi/%lo(G)` reloc). Local to the reloading TU: sibling files that
read `G` as a bare scalar still match (no intervening nonscalar store between two reads), but every
read of `G` **in the same TU** must switch to `G[0]` (byte-neutral for a single read — same `lw`).
S186 `func_80069BCC`: `D_80105B20[D_801B6098[0]] = 0; if (D_801B6098[0] == 0)`, full-fn byte-exact,
no permuter (root-caused by a `~/development/repos/mips-gcc-2.7.2/cse.c` subagent fan-out).

**Confirming tell it is separate symbols (not one shared struct base):** the ROM re-emits `lui at,
%hi(sym)` per access even for addresses that share the same %hi (all 0x8010) — a single struct/array
base would CSE to one `lui`. Separate `lui`s ⟹ separate symbols; the struct-member reloc addend
(`%lo(D_xxx)+4` == `%lo(D_xxx+4)`) resolves to the same bytes as the next symbol, so a `Vec3f` view
that references only the base symbol is byte-safe (verify by full-make ROM SHA-1, an
[isolated-compile caveat](#isolated-compile-caveat): asm-differ shows `sym+4` vs `sym_next` as a diff).
An array-of-struct `T g[]` (stride = sizeof struct, fields folded into `%lo(at)`) reproduces the ROM's
"separate `lui %hi(Cxx)` per field, fresh recompute per use" shape when the parallel field symbols
(C50/C54/C58) are 4-apart at stride 12 — the ROM is `struct{f32 x,y,z;} g[]`, not three `f32[]`.
Cross-refs [#struct-access-folding-changes-scheduling](#struct-access-folding-changes-scheduling) and
[#return-type-is-load-bearing](#return-type-is-load-bearing) (same "types are load-bearing for codegen"
class).

## short-text shifts flowing-bss (a length miss surfaces as a SIBLING's wrong data addr)

**Trigger:** a classical fn compiles cleanly but full-make ROM SHA-1 misses, and a **sibling** fn in
the same file reads the wrong data address — its `%lo(D_xxx)` resolves off by a fixed delta, with the
`.bss` symbol map showing a whole `0x8010xxxx` region shifted by that delta. Easy to misdiagnose as a
data-placement / symbol bug in the sibling.

**Cause:** the game's `.bss` (e.g. `main_bss`, symbols at `0x800D2930+`) is interleaved in the main
segment and flows after the `.text` that precedes it in ROM order. When a fn is N bytes short (or
long) of its target length, everything after it — including that flowing `.bss` — shifts by N. So a
byte miss in fn A (wrong .text length) manifests as wrong data addresses in a sibling fn B that reads
those `.bss` globals. B's codegen may be perfectly correct.

**Diagnose:** `objdump -h build/src/<seg>/<file>.o` `.text` size vs the subseg's reserved span (the
yaml `[start..next]` extent). Built `.text` 0x130 vs reserved 0x140 → `func_80076558` was 0x10 short →
the `0x8010xxxx` `.bss` shifted -0x10 → `func_80076500`'s `%lo(D_80105B6C)` read `0x80105B5C` (S162).
The `build/mariogolf64.map` shows the shifted region re-syncing at the next fixed-address symbol.

**Fix:** correct the **short** fn's length (its codegen), not the sibling's reloc — the sibling is a
symptom. Guard: when a same-file sibling's length is still wrong, do not trust an isolated per-fn
`objdump`'s resolved addresses; gate on the full-make ROM SHA-1 with all fns at their correct length.
`func_80076500` looked "wrong" (bytes `c4205b5c`) purely because `func_80076558` was 0x10 short (S162);
both matched the instant `func_80076558` reached its exact 0xE8 length (via the
[mem-in-struct lever](#mem-in-struct-scheduling-lever) above).

**Long-text variant — a decomposed-subseg OVERFLOW shifts the flowing `.bss` SYMBOLS themselves
(S170).** The mirror image of the short case, and easier to misdiagnose: a classical fn N bytes *too
long* overflows its decomposed subseg's reserved span, and the shift surfaces not as a sibling's
reload but as the auto-`.bss` symbols floating. Those symbols (`D_<vram>` defined in the
splat-generated `asm/data/<seg>.bss.s`, placed by cumulative `.main_bss` object order) ALL move to
`name+N` — which reads like symbol-table / reloc corruption (`D_800DC6E0` resolving to `0x800DC6F0`,
the head-asm sibling's `%lo` going wrong too), not a length bug. S170: `func_8004DDE4` compiled 2 instrs
(8 B) long → object `.text` rounded `0xF0→0x100` → overflowed the 240 B `[0x29170,0x29260)` slice → every
`D_800DC6xx` bss symbol shifted +0x10. **Diagnose the SAME way and FIRST — object `.text` size vs the
subseg span** (`objdump -h build/src/<seg>/<file>.o` vs the yaml `[start..next)` extent), before chasing
the symbol addresses; the symbol shift is a downstream symptom, and `asm/data/<seg>.bss.s` being
gitignored hides it from `git status`. The 2-instr overflow itself was a `&D_arr[i]` self-store
re-derived instead of reusing the live pointer; see the
[struct-array-of-BSS direct-index lever](#struct-array-of-bss-direct-index-vs-base-pointer-var) below.
**Self-ref variant — the SAME fn's OWN data ref reads `+N` (S208).** The most self-misleading form:
a wrong-length fn's shift surfaces on **its own** `%lo(D_xxx)`, not a sibling's, so it reads like the
symbol itself is mis-mapped. S208 (partial-bank `func_80059BA0.c`): two over-long switch bodies
(`func_8005D218`+`func_8005D2E4`, +2 instrs each = +0x10 total) shifted the flowing `.bss`, so
`func_8005D218`'s own `%lo(D_80105DC1)` resolved `0x80105DD1` (+0x10) and the `map` showed
`D_80105DC1` at `0x80105dd1` with a `.NON_MATCHING` suffix — a convincing "nonmatching-bss wall"
mirage. It is NOT a wall: a matched sibling reading the same region at exact length
(`func_8005B0A0`/`D_800C2B28`) proves the region resolves fine. **Confirm by reverting** the
suspect body to `INCLUDE_ASM` and rebuilding — if the symbol snaps back to its named address
(`D_80105DC1` → `0x80105dc1`), the `+N` was the symptom of your instr-count miss, and the fix is the
codegen length (match instruction count exactly), never the symbol ref. Diagnose with `objdump -h`
(object `.text` size vs the yaml span) FIRST — faster and less ambiguous than reading `map` addresses.
The `+N` == instr-count-miss × 4.

**Tooling note (S170).** `decomp_loop.py`'s `find_segment` can't locate a fn whose subseg is already
flipped to `c` (its asm is under `asm/nonmatchings/<seg>/`, not top-level `asm/<off>.s`), so the
asm-first fast-path miss-recovery fails with `no glabel found`. Workaround: manual
`mips-linux-gnu-objdump -d build/src/<seg>/<file>.o` vs the `asm/nonmatchings/<seg>/<fn>.s` hex (the
reloc-hi/lo diffs there are the [isolated-compile caveat](#isolated-compile-caveat); gate on the
full-make SHA). A `--target-s <path>` arg for `decomp_loop` is a tracked tooling follow-up.

**Permuter workaround for a flipped-subseg fn (S171).** The same post-flip gap breaks
`setup-permuter.sh` two ways: `mg_resolve_c_asm` can't find an `INCLUDE_ASM(…, <fn>)` line once the fn
is inlined C, and `import.py`'s default preprocess set is bare `-I include` (misses
`include/libultra`, so `PR/ultratypes.h` fails). **Hand-build the scaffold** in
`nonmatchings/<fn>/`: (1) copy an existing MAIN-seg `compile.sh` (e.g. `nonmatchings/func_80076640/`)
and `sed` the func name — it carries the full profile `-mips3 -G 0 -O2 -I include -I include/libultra
-I include/libultra/internal -I include/lib{kmc,nusys,mus,nualstl,naudio} -DF3DEX_GBI_2 -D_FINALROM`;
(2) write `settings.toml` (`func_name`, `compiler_type = "gcc"`); (3) `cpp -P -undef base.c.raw … <same
-I set> -DPERMUTER …` to a **self-contained** `base.c` (the permuter's own preprocess is bare, so it
must be pre-expanded); (4) `cp build/src/<seg>/<file>.o nonmatchings/<fn>/target.o` — the current
INCLUDE_ASM build object holds the real target bytes for that fn. Then `./run-permuter.sh <fn>`
(`mg_find_permuter_dir` globs `nonmatchings/<fn>*`, so it finds the hand-built dir). This is how S171
scaffolded `print_string_at_grid` + `func_8004DC44` after their flip. Same tracked tooling follow-up
(teach `decomp_loop`/`setup-permuter` to search `asm/nonmatchings/**` + derive the main-seg `-I` set).

## struct-array-of-BSS direct-index vs base-pointer var

**Trigger:** a classical fn accesses a global struct-array in `.bss` (`extern Slot D_<vram>[];` with a
runtime index `[i]`), and the build's field addressing diverges from the ROM: the ROM re-derives each
field via `%hi(D_<field>)/%lo` off the scaled index (a distinct reloc per field, e.g. `lw %lo(D_800DC6F0)`
for `.total` at +0x10), but the build keeps a base pointer and uses immediate offsets (`lw 16(v1)`),
producing FEWER, different instructions. Or the reverse: a self-referential store the ROM writes through
a live pointer (`sw v1,0xC(v1)`) the build re-derives via `%hi/%lo` (2 extra instrs → a subseg-length
[overflow](#short-text-shifts-flowing-bss-a-length-miss-surfaces-as-a-siblings-wrong-data-addr)).

**Cause (KMC gcc 2.7.2):** `Type *s = &D_arr[i]; ... s->field` materializes the full pointer `&D_arr[i]`
once and CSEs it, so every field access is `offset(s)` (base+immediate). Writing `D_arr[i].field`
*directly* (no pointer variable) makes GCC re-materialize `%hi(D_arr)+i*stride` per access and fold the
field offset into the `%lo` addend (`%lo(D_arr)+0x10` = `%lo(D_800DC6F0)`, which the disassembler names
by the effective symbol) — one reloc per field, matching the ROM. The original source almost always
used direct `arr[i].field` indexing; the reloc addend resolving to a different `D_<vram>` name than the
base is the [isolated-compile caveat](#isolated-compile-caveat), harmless under the full-make link.

**Fix:** default to **direct `D_arr[i].field` indexing**, NOT a `Type *s = &D_arr[i]` base-pointer var.
Reach for a pointer variable ONLY for the specific store the ROM expresses as `base+offset` off a live
`&D_arr[i]` — typically a self-referential init like `arr[i].prev = &arr[i]`: writing `s->prev = s`
(pointer form) emits `sw v1,0xC(v1)` (1 instr, reusing the self-ptr `v1` as base), while the direct
`D_arr[i].prev = &D_arr[i]` re-derives the address (3 instrs). S170 `func_8004DDE4` needed the mix — the
lone `prev=s` self-store through the pointer, `next`/scalars direct — to hit the exact 24-instr length;
`func_8004DD70`/`func_8004DE44` were all-direct. See also
[struct-access-folding-changes-scheduling](#struct-access-folding-changes-scheduling) (a related but
distinct scheduling effect of combining per-field base symbols into one struct).

## goto-dispatch branch-toward vs branchless (constant dispatch through a shared return)

**Trigger:** a classical fn dispatches a small `switch`-like selector to a few constant results returned
through one shared variable, and the ROM emits, per case, a branch-likely `beql cond, RETURN` with the
`li v,CONST` stolen into the annulled delay slot. Every structured idiom (`if`/`else if`, `switch`,
ternary, `{body; goto end}`, do-while+break) locks with a pervasive basic-block-layout miss (same
length, ~half the rows differ by form/position, not a 1-2 insn near-miss), and a lone innermost
`if (x == K) v = CONST;` (with `v` provably 0 in an all-constant arm) branchless-if-converts to
`mask & CONST`. `-O1/-O2/-O3` and every `-fno-schedule*`/`-fno-*` flag give the same wrong layout.

**Cause (KMC gcc 2.7.2, cited):** a structured `if (cond) { body }` lowers via `do_jump` with an
`if_false_label` (`expr.c:9532-9545`) to a branch around the body (`bne cond, skip`; body inline on the
fall-through). MIPS has no annul-true slot (`mips.md:127` = nil), so a fall-through body can never be
pulled into a branch-likely delay slot; only `optimize_skip` (`reorg.c:1141,1167-1172`) can rescue it,
and only for the one case whose skip-target abuts the shared return (the last). To branch toward the
body (`beq cond, body`), the then-clause must be a bare jump: `if (cond) goto L;` hands `do_jump` an
`if_true_label` (`expr.c:9522`), and `jump.c:1743` ("condjump over an unconditional jump") inverts it.
Once branch-toward, `fill_slots_from_thread` (`reorg.c:3257,3404-3616`) steals the single-insn body
into the annulled `beql`. A `li`/`addiu` const is annul-eligible (`type=arith`, `dslot=no`); a memory
load (`lh`) is `dslot=yes` (`mips.md:79-82`) and can never ride the slot. Stock gcc, no KMC patches.

**Fix:** write the dispatch as `if (selector == K) goto L_k;` per case, `goto end;` for the default,
then the bodies out of line after the tests, each `v = CONST; goto end;`, and a single `end: return v;`.
Order the body labels so any load-valued case is declared **last** (it becomes a plain `beq` to a tail
block that falls through into the shared return). **Gotoless is provably impossible** here (the lone
`if (x==K) v=CONST` always branchless-if-converts). This is the Code Complete ch17 documented-goto case
(emulating a structured dispatch, forward-only, all labels used) -- keep the explanatory comment so it
is not "simplified" back to a switch/if-else. The permuter plateaus on it (extent 700; it cannot invent
the polarity flip); crack it via the fan-out compiler-source dive + isolated-scoring harness (see
[#compiler-source-fan-out-escalation-above-the-permuter](#compiler-source-fan-out-escalation-above-the-permuter)).

**Dispatch lever ladder — try `switch` first for a dense selector with a shared post-store (S168).**
The per-case documented-goto above is the fix for the *constant-return-through-a-shared-var* shape
(S163). A different but adjacent shape -- a **dense** `1..N` selector where each case sets a
pointer/field plus a flag, then a **shared** post-dispatch `if(flag) *dst=…` -- matches as a plain
`switch(sel){ case K: …; break; … }`, and the ladder is: `if/else-if` branches **AWAY**
(`bne sel,K,skip`, wrong polarity); the per-case **goto** form branches toward but GCC **tail-merges**
the identical `flag=1; goto store` tails across cases (collapsing the ROM's separate per-case
`li flag,1`); only `switch` branches toward **and** keeps each case's tail distinct (a per-case `break`
into the shared store). So for a dense-selector-with-shared-store, try `switch` **first**; reserve the
per-case-goto fix for the S163 constant-return shape where the switch is a jump table
(`#switch-jtbl-dispatch`) or the lone case branchless-if-converts. S168 `func_80071220`'s 4-way
`sel` dispatch (1/2/3 + default, each picking a `rec` field ptr + a set flag, then a shared
`if(set) *dst=1`) matched byte-exact as a `switch` after the goto form tail-merged the shared `set=1`.

**Provenance:** S163 `get_club_meter_extent` (`src/main/func_80043AF0.c`): flag-gated (putter) dispatch
on `category` returning golf-yardage constants (or `power_a` / `power_a*1.1f`), result through `$v1`,
shared `jr ra; move v0,v1`.

## call-result a0-vs-v0 single-allocno (force a scratch reg via both-arm reuse)

**Trigger:** a classical fn's call result (or any value) is held by the ROM in a scratch GPR (`$a0`)
with `move a0,v0` right after the `jal` (often in a branch delay slot) and a trailing `move v0,a0` at
the return, but my build coalesces the value straight into `$v0` (no moves, 2-3 insns shorter) so the
whole int/FP chain runs in `$v0`. Distinct-variable / extra-use / operand-swap / if-else-factor levers
do not move it.

**Cause (KMC gcc 2.7.2, cited):** the call-result copy `pseudo = $v0` gives the pseudo a copy preference
for `$v0` (`global.c:1005-1034`); `find_reg` scans hard regs ascending (`REG_ALLOC_ORDER` undefined ->
`$2`/`$v0` first) and `$v0` is free across the value's live range, so the preference wins and the pseudo
coalesces into `$v0`. Bumping ref-count / live-length does not help: it still copy-prefers a free `$v0`.

**Fix:** make the value **one** reused variable assigned in **both** arms of an `if/else`, each arm the
full expression. That makes it a single global allocno whose live range now conflicts with `$v0` (the
preference no longer wins a free reg), so `find_reg`'s ascending scan lands on `$a0`. gcc cross-jumps /
tail-merges the two arms back into one select + one op, so the source stays clean (no object
duplication). The permuter plateaus (units 235); cracked by an empirical isolated-harness agent +
the `global.c` allocno dump.

**Provenance:** S163 `get_club_meter_units` (`src/main/func_80043AF0.c`): `units = get_club_meter_extent
(...); if (putter) units = (s32)((f32)units * A); else units = (s32)((f32)units * B); return units;` ->
the ROM's `$a0` chain + `move a0,v0` / `move v0,a0` bookends.

## compiler-source fan-out (escalation above the permuter)

**When:** a classical fn locks with a pervasive BB-layout / regalloc / scheduling miss (not a 1-2 insn
near-miss) that resists every source idiom and the permuter plateaus (it cannot invent a structural
polarity / allocation lever -- extent 700, units 235). Before carrying it as a spike, escalate to
a compiler-source dive.

**How (the winning pattern):** fan out parallel subagents over the KMC gcc 2.7.2 tree
(`~/development/repos/mips-gcc-2.7.2`, see the KMC-compiler-source memory) and binutils 2.6
(`~/development/repos/mips-binutils-2.6`), one per RTL pass -- `reorg.c` (delay-slot / branch-likely /
annul eligibility), `jump.c`+`flow.c`+`stmt.c` (block layout / cross-jump / `do_jump` polarity), and
`config/mips/{mips.md,mips.c,mips.h}` (patterns, `define_delay`, `REG_ALLOC_ORDER`) -- to recover the
exact mechanism (and prove which source shapes are impossible). In parallel, run an empirical agent on
an isolated-scoring harness (compile the exact `src/main` -O2 profile standalone, normalize branch/jal
target addresses, print an aligned TGT-vs-CAND diff + a layout-shift-insensitive diff count) that tries
30-60+ source variations. The mechanism agents prove why; the empirical agent finds the source. This
tier cracked two permuter-plateau fns in S163 that would otherwise have carried.

**Per-FUNCTION fan-out for a sibling set (S167).** When a decompose head holds several sibling fns
that share a divergence class (e.g. all access one call-return game-state struct), fan out **one
subagent per FUNCTION** in parallel, not one per RTL pass. Give each a strict **input contract**: the
exact instruction-level divergence you have ALREADY isolated (mine-vs-target, register by register),
the paths to read (the target `asm/nonmatchings/.../<fn>.s` + your own `objdump`/`M_<fn>.txt` + the C
source lines), the specific mechanism question, and the two repo roots -- so the agent derives the
lever instead of rediscovering the diff. The payoff is a **unifying model**: S167's `func_800710C4`
agent found the `func_8005AF50()` return is a game-save **struct** (`SaveBlock{u8 pad[0xF4]; s8
tbl[6][0x12]; ...}`), and modelling the accesses as `base->tbl[i][j]` (COMPONENT_REF bitpos keeps
`+0xf4` explicit, `expr.c:4882`; MEM_IN_STRUCT scheduling) fixed **both** siblings byte-exact; the
`func_8007117C` agent independently found the loop levers (`p[j]` index form -> biv-elimination
synthesizes `end=start+N`, `loop.c:6165`; `s32`-load for `lb` not `lbu`, `mips.c:1029`). The third
agent PROVED a fundamental wall (see `#cse-make-regs-eqv-branch-fold`), which is a valid, budget-saving
outcome -- carry fast on a proven-impossible, don't grind. Verify each lever with an isolated
reloc-aware byte-cmp (`objcopy --only-section=.text <fn>.o` vs the ROM at the fn's rom offset), NOT the
full-make SHA (which is all-or-nothing across the file).

**Applies to dead-frame / pure-regalloc walls, not just BB-layout (S173).** The tier is the right
escalation for a `#dead-frame-reload-artifact-regalloc-wall` too. Fan out **one subagent per allocation
mechanism**: `config/mips/mips.c` (frame-size / `MIPS_STACK_ALIGN` / prologue emit), `reload1.c` +
`global.c` + `local-alloc.c` (spill slot commit + `allocno_compare` / `qty_compare_1` priority +
`find_free_reg`), `loop.c` (LICM / `NOTE_INSN_LOOP` goto-invisibility / strength reduction), and
`expmed.c` + `optabs.c` (divide/multiply-by-constant operand + pseudo creation order). Give each the
isolated `objdump -dr` diff vs `target.o` and have them **verify with gcc RTL dumps** (`-dr` rtl, `-dl`
lreg, `-dg` greg, `-dS` sched) on a scratch compile, not just source-read. The payoff is often a
**dump-verified negative**: S173 `func_8004DC44` characterized the residual `v0`/`v1` swap as a
life-length-dominated `local-alloc.c` priority (magic scores 6666 vs the dividend's 1666 → grabs `$v0`).
**S174 CAVEAT — a "dump-verified negative" can be over-scoped.** S173 called the swap "unflippable /
unrecoverable"; S174 showed that was wrong — `return g/40` reproduces the ROM's `/40` bytes exactly (the
swap is flippable-in-isolation via a reg-2 SET → the suggestion pass; see
`#signed-divide-const-v0v1-quotient-destination`). The correct negative is narrower: dividend→`$v0` is
unreachable *in a void/callless/returnless loop-fed leaf*, not universally. Lesson: state a regalloc
negative with its **exact enabling context**, and before declaring "unflippable" run the
cross-project sweep below — a single-function dive can miss a lever another codebase exhibits.

**Cross-project matched-corpus mining (S174).** For a reg-alloc / scheduling wall, the highest-value
escalation is to fan out **one subagent per same-toolchain N64 decomp** that shares the compiler
(KMC gcc 2.7.2/2.7: `../marioparty`, `../marioparty2`, `../marioparty3`, `../snowboardkids2-decomp`,
`../drmario64` [EGCS+KMC], `../puzzleleague64` [IDO+KMC], `../hm64-decomp`; `../papermario` is gcc 2.8.1,
weaker signal). Give each the exact asm signature and have it (1) confirm the per-TU compiler, (2) grep
the disassembly/`build/*.o` for the idiom, (3) classify the register outcome, (4) extract the C source +
**provenance** of any MATCHED example that shows the wanted polarity. A matched analog IS the lever (its
C reveals the source shape); the *absence* of one across all projects is itself strong evidence the ROM's
form is a non-source pressure artifact → carry with confidence. Pair with RTL pass dumps (`-dl`/`-dg`/
`-ds`) on your own scratch compile as ground truth. S174 ran this over 8 projects to correct the S173
over-scoped negative and pin the quotient-destination lever. Proving un-source-reachability IS the
deliverable when the answer is "carry"; save the near-match seed (`docs/wip/`) so the retry starts one
artifact away.

**Permuter-reseed from the best candidate, not the clean seed (S175).** When re-running the permuter on
a same-fn regalloc carry, seed it from the **best prior candidate** (`nonmatchings/<fn>/output-<score>-*/
source.c`), not the hand-written clean seed, and **reseed again from each new best**. The permuter's
randomizer explores a neighborhood around its base; a frame-bearing / structurally-closer candidate opens
a different neighborhood than the clean seed. S175 `func_8004DC44` broke a **3-sprint-stuck floor** this
way: vA-seeded runs never beat 14450 in ~430k cumulative iters (S173+S174), but reseeding from the
frame-bearing 14450 candidate reached 13530, then reseeding from 13530 reached 13235 (frame + schedule +
all operations matching the ROM; only the divide-register cascade remaining). The lower score does not
imply a reachable zero — a permuter that keeps improving the *matchable* parts (loop/guard shape) while
the true blocker is a source-unreachable allocation decision will asymptote above 0 (S175 confirmed the
divide stayed swapped at every new best). But it produces the tightest documented near-match for the
carry, and a genuinely-new best is worth the two bounded runs. Preserve the best `output-*/` dirs in
`docs/wip/` provenance so the next retry reseeds from them.

**Fan out BEFORE declaring a wall OR reaching for a synthetic-no-op permuter match (S177).** On a
regalloc/scheduling near-match, the compiler-source fan-out is the FIRST escalation, not the last: of
four S177 near-misses it flipped two apparent-walls to CLEAN banks and proved two real walls.
(a) `heap3_free` — a mechanism agent found the offset-0-symbol re-materialization rule in cse.c/global.c
(the winning clean lever, `#offset-0-symbol-re-materialization`). (b) `heap3_get_largest_free` — an agent
caught that the "1 word short / needs a synthetic no-op" verdict was a `-S` reorder-mode MISREAD (the
clean source already matched on the assembled object; see the Assembler-differences `-S` note). Without
the fan-out, (a) would have carried and (b) would have banked an unnecessary `p = p + 0` no-op (banned).
(c)+(d) `func_8004E1E0` / `func_8004E2DC` — agents claimed to PROVE the CSE-reload and mask-rotation
walls from cse.c/global.c. **Both proofs were WRONG (S178 banked both);** see the two-agents-per-wall
correction below. **Doctrine: a regalloc/CSE/scheduling near-match that resists 2-3 hand levers routes
here, before the permuter and before "wall".** The empirical agent must judge the ASSEMBLED `.o`, never `-S`.

**Two agents per wall — never trust a single "unreachable", even a source-proved one (S178).** S177's
mechanism agents PROVED `func_8004E1E0` (CSE reload) and `func_8004E2DC` (mask rotation) unreachable
from the compiler source; S178 ran the fan-out again with **two agents per wall — a primary and an
adversarial one attacking from orthogonal angles (volatile / aliasing / scheduling / liveness)** — and
the adversarial agent cracked BOTH, byte-exact. A source-cited "unreachable" is a HYPOTHESIS, not a
proof: the primary agent proved only that ITS candidate class forwards/allocates a certain way, not
that no faithful C reaches the ROM. The levers it missed: (c) a per-access **volatile view** of the
reloaded field ([#volatile-view-cse-reload](#volatile-view-cse-reload)) — the primary tried
volatile-on-one-field-only and saw it float, without also volatiling the fields that must schedule
ahead; (d) the **define-after-call** caller-saved placement plus an **inline sentinel** (no `head`
local) so CSE derives it from the load base and copies it to the loop reg — the primary tested only
"force head callee-saved" (the wrong polarity). **Doctrine: require a second, adversarial agent before
accepting any "unreachable" verdict; each finds what the other's candidate set structurally cannot.**
Both walls were S177 carries the BACKLOG had flagged "do NOT retry without a new mechanism" — the
second-agent fan-out WAS that mechanism.

**Pair ORTHOGONAL LENSES, not two of the same (S180, 2nd confirmation).** S180 cracked the DL
scheduler-load-pair wall (`func_800329F4`, a `docs/hazards.md`-documented "escalate to the permuter"
carry) by fanning out **a scheduler/priority agent and a memory-model/alias agent** (plus an
assembler rule-out). The priority agent's track — hunt a faithful source REORDER that lowers the
color load's schedule priority — was a **proven dead end** (every color-expr / associativity / reorder
form stalled at 18-30/97, because the load carries a sticky `LAUNCH_PRIORITY` boost that dominates,
not a tie-break). The alias agent's track — could a faithful type change add a memory DEPENDENCE that
pins the loads? — found the `mem/s` color-struct lever ([#mem-in-struct-scheduling-lever](#mem-in-struct-scheduling-lever)),
byte-exact. **Both agents independently converged on the same `mem/s` lever = strong corroboration**,
but the alias lens reached it directly while the priority lens could only prove its own class
couldn't. Lesson: when assigning the two agents, give them **structurally different lenses** (here:
list-scheduler priority vs. the `true_dependence` memory-alias model), so one covers what the other's
candidate set cannot. The assembler rule-out (gas `.set reorder` is a faithful 1:1 transcriber under
`-mips2`) correctly scoped the wall to GCC before the lever hunt.

**A WALLED permuter is a dive TARGET, not an automatic carry (S209).** The escalation is not only for a
*slow* permuter — it beats a fully **plateaued** one. `func_8005C458` (a 6×6 non-zero-cell grid counter)
walled the permuter at **~1.27M iters / score 55** — a pure-regalloc allocno permutation (the row base
and the loop counter swapped, plus one commutative `addu` operand order) — yet the compiler-source dive
cracked it **byte-exact with ZERO permuter iterations**. Why the permuter cannot: both winning levers are
SOURCE RESTRUCTURES outside its neighborhood — (a) introduce an intermediate call-result copy
(`u8 *ret = func_8005AF50(); …; s8 *base = ret;`) to defer `base`'s materialization and shorten its
live-length ([#local-alloc-qty-permutation](#local-alloc-qty-permutation), the global.c live-length
steer), and (b) convert pointer arithmetic to INTEGER arithmetic to flip a commutative operand order
([#integer-arith-commutative-operand-order](#integer-arith-commutative-operand-order)). The permuter
reorders decls / splits-merges temps / hoists consts / wraps `do{}while(0)`; it never invents a semantic
intermediate copy nor a pointer→int retype. **Doctrine: a pure-regalloc allocno/qty permutation that
WALLS the permuter routes HERE (compiler-source dive), not to an automatic carry.** The whole S209
grid-counter family (C458/C4B4/C5B4/C614) banked this way, 0 permuter — see [#grid-counter-double-loop](#grid-counter-double-loop).

---

## cse make_regs_eqv branch-fold (reused-var canonical fold on a `?:`-with-flag store)

**Symptom:** a classical fn is **byte-exact except a 3-word branch-direction triple** in a
store-then-print (or store) tail: mine `beqz X; op a1,v0,K; move a1,v0` vs the target `bnez X; move
a1,v0; op a1,v0,K` (or the mirror). The value being stored/passed is `cond ? (t|K) : t` where
`t = f(loaded)` and the **same variable holds the loaded value AND the final result** (so the load is
`lb/lbu a1` into the arg register). Everything else -- regalloc, the `t` computation, the compare --
matches. S167 `func_80070FD0` (a COM-win byte: `t=count|(old&0x80); nv=(D_801B60C5==0)?t|0x80:t`).

**Root cause (`cse.c make_regs_eqv`:840-862):** the default arm's plain copy `old = t` merges the two
into one quantity, and `old` becomes the **canonical** register because it **outlives `t` and its
last use (the store/printf) crosses the post-branch EBB boundary** (the merge label has 2 preds). cse
then rewrites the *other* arm's `t|K` -> `old|K`, so `t` dies single-use and global.c coalesces it
onto `$a1` -- the fold (1 instr short, but the correct `bnez` polarity). The only form that keeps `t`
separate makes the **default** arm a non-copy (`old = t|K; if(!cond) old = t`), which gives `beqz`
polarity instead. **Branch-direction and the fold are LOCKED:** `(cond?t|K:t)` has exactly two C
shapes and each pins one of {right-polarity+fold, separate-t+wrong-polarity}.

**Verdict -- carry fast, do NOT grind.** This is **not reachable from equivalent single-TU C**: the
load-in-`$a1` requires reusing the loaded var as the arg, which forces the canonical fold. Proven at
S167 by ~35 hand variants + 43k permuter iterations (the permuter only makes equivalent transforms,
so it cannot escape it either) + a full cse/combine/greg RTL-dump trace. The target was compiled from
a shape not recoverable from the byte behavior (a different flag/print data-flow, a helper, or a
macro). Recognize the symptom, bank the file's other fns (`#cross-jump-tail-merge` mixed partial), and
**carry this one for game-source insight** -- not another permuter run. Save the near-match
(`beqz`-polarity, byte-exact minus the 3 words) so the retry starts one lever away.

---

## abs-coalescing reg-swap

**Symptom:** a byte-exact-structure classical fn locks at a **small (3-instr) register swap** in an
`if(fabsf(x) < K)` unary-op-then-const-compare: the target computes `abs.s f2,f0` (abs into a FRESH
reg, keeping the operand's reg `f0`) then loads the const into `f0`; mine emits `abs.s f0,f0`
(in-place, coalescing operand->result) and loads the const into `f2`, so the `abs`/`mtc1`/`c.lt.s`
operands are register-swapped. Everything else matches. S169 `func_80076640`
(`if(fabsf(cosf(pitch)) < 0.1f) pitch += 0.3490659f`; 75/78 near-match, ONLY the 3 abs-region regs
differ). Note the `0.3490659f` literal is exact (0x3EB2B8C4); `0.34906584f` is 2 ULP low (0x3EB2B8C2).

**Root cause (S181, source-proven):** gcc 2.7.2 `combine_regs` (local-alloc.c:1813-1817) records the
operand's hard reg (`f0`) as an ARITHMETIC suggestion on the abs-result qty **unconditionally whenever
`abssf2` reads a dying hard reg**; the suggested-reg pre-pass (local-alloc.c:1469-1477) then assigns
the abs that `f0` BEFORE the const qty reaches the fallback pass — and a literal const has no register
source, so `combine_regs` can never give it a competing suggestion. So the abs claims the operand's
low reg (in-place) and the const takes a fresh reg — the reverse of the ROM. The `absSF2` pattern
(mips.md:1578, `=f`/`f`, no `0` matching-constraint) merely PERMITS in-place, never forces it; gas is
inert (`abs.s` is a real opcode, never macro-expanded). Which value "owns" the low reg is this
local-alloc suggested-pass tie, not a scheduling choice. The ROM's fresh-reg form corresponds to the
abs result being GLOBAL-allocated (`global.c`), which needs a cross-block use of the abs result — a
state a block-local abs-of-a-dying-hard-reg deterministically cannot reach at this profile.

**The fresh-reg unary-float LAW (cross-project, byte-cmp-proven on the identical KMC GCC 2.7.2).**
Fresh-reg `abs.s`/`neg.s` (D != S) is emitted **iff** the RESULT is reused (>=2 uses / lives across a
call) OR the OPERAND is kept live past the op OR the operand is a non-hard-reg (memory/struct-field)
value. A **single-use result with a dying hard-reg operand ALWAYS coalesces in-place.** Verified across
5 same-compiler byte-matched corpora (Mario Party 1/2/3, Dr. Mario 64, Snowboard Kids 2; Harvest Moon
64 + Puzzle League 64 are same-compiler NULLs — no float abs in the whole game, the idiom is rare).
gcc 2.8.1 (Paper Mario) has a WEAKER coalescer (fresh even for a dying single-use *pseudo* operand)
but STILL coalesces a dying *call-return* operand in-place — so the compiler VERSION is not the lever.
Use this law to classify any FP unary-op reg-swap on sight: single-use + dying hard-reg = irreducible.

**Verdict, carry (all faithful/flag levers EXHAUSTED; not do-not-retry).** S181 closed every avenue
with source-grounded rigor: 4 GCC-source lenses + an 8-project cross-project mining sweep
(`#cross-project-matched-corpus-mining`) + a 12-flag profile-probe (O1/O2/O3/-ffast-math/-g/
-fno-schedule-insns/-fno-delayed-branch/... ALL in-place) + a direct **2.8.1 cross-compile** (in-place
AND 80 insns, strictly worse; and the same-TU sibling `func_80076778` is matched at 2.7.2, so the TU
is PROVABLY 2.7.2 — not a wrong-version pin). MG64's exact signature (dying + hard-reg + single-use +
single-precision -> fresh) appears in NO project at EITHER compiler version, so the ROM's form is a
2.7.2 patchlevel/build artifact the reconstruction 2.7.2 cannot reproduce; a source reuse can't be the
answer (it adds an instruction the 78-insn ROM lacks, and a DEAD reuse is DCE'd before allocation). The
permuter is proven futile (it only mutates source shape; plateaued at the swap over 338k iters).
Recognize the symptom via the LAW above, bank the file's matched fns (one-tu mixed-partial), and carry
with the 75/78 near-match saved (`base.c`: the `do{}while(0)` + `cosf`-temp wrapper is load-bearing for
the schedule). Retry ONLY on a genuinely-NEW mechanism (a faithful source shape that keeps the abs
result or operand live at ZERO added instructions, or the exact original build binary) — do NOT re-run
the source dive / permuter / flag probe / 2.8.1 cross-compile (S181 exhausted all four).

---

## cross-project matched-corpus mining

**When:** a structural-complete classical fn locks on a codegen/regalloc artifact that the
`#compiler-source-fan-out-escalation-above-the-permuter` dive has proven **unreachable from faithful C**
at our profile, and you need either (a) the missing source idiom, or (b) confirmation the ROM's form is
a compiler-config/patchlevel origin (not a source shape). This is the escalation ABOVE the source dive
and the permuter -- it reads OTHER games' matched decomps as a byte-exact corpus of "what the real
compiler actually emits" for the exact pattern. First flagged as the last untried lever in the S175
`#signed-divide-const-v0v1-quotient-destination` note; first EXECUTED as a method in S181
(`#abs-coalescing-reg-swap`).

**The sibling-project compiler map (relevance gate).** Only a project on the SAME compiler is
authoritative; a different compiler is at most mechanism-informative. As of S181, the local N64 decomps
that share MG64's **KMC GCC 2.7.2** are: `marioparty` (MP1), `marioparty2`, `marioparty3`, `drmario64`
(us/gw), `snowboardkids2-decomp`, `hm64-decomp`, `puzzleleague64` -- all `-mips3 -mgp32 -mfp32`, game
code at `-O1` (MP1/MP2/MP3) or `-O2` (drmario64/sbk2/MG64); the discriminator for a regalloc tie holds
within a single `-O` level, so the O1-vs-O2 default is not a confound. `papermario` game code is **gcc
2.8.1** (same egcs/local-alloc lineage, one minor version off) -- mechanism-informative but NOT
authoritative; its 2.7.2 `os/*` libultra is `-O3` and typically has no float-abs. Confirm each
project's compiler from its `Makefile`/`permuter_settings.toml`/`configure` before trusting a hit.

**Method (fan out one agent per project).** Give each a shared context file (the exact target-vs-mine
asm, the pattern signature, the relevance gate) and have it: (1) confirm the compiler+version+flags;
(2) grep `src/` for the idiom AND scan the byte-exact ROM asm -- an `INCLUDE_ASM` stub is byte-exact
ORIGINAL-compiler output, so it authoritatively shows what the real compiler does even where no matched
C exists yet; (3) for any hit, report the matched C SOURCE + the asm allocation (in-place vs fresh) +
that TU's `-O`, prioritizing an example that matches YOUR signature; (4) verdict: transferable source
idiom, or a compiler-config confirmation. A project with the identical compiler but ZERO instances of
the idiom (e.g. no float abs in the whole game) is a clean NULL -- it confirms the toolchain and the
idiom's rarity, neither confirms nor denies. Read-only; agents touch nothing.

**What it buys.** S181's 8-project sweep established a byte-cmp-proven LAW (the fresh-reg unary-float
rule in `#abs-coalescing-reg-swap`) from 5 same-compiler corpora, and -- with a direct 2.8.1
cross-compile -- proved MG64's exact signature is produced by NO project at EITHER compiler version,
converting an open "untried lever" carry into a definitively-closed one. A NULL result (no corpus has
your signature) is itself the answer: the ROM's form is a build artifact, carry it. Pair the sweep with
a **compile-with-the-other-compiler probe** when a version origin is suspected (fetch the sibling
project's gcc binary, e.g. `papermario/tools/build/cc/gcc/gcc` = 2.8.1, and compile your isolated TU
with it): if the OTHER version reproduces the ROM form, you have a wrong-pin; if it does not (S181:
2.8.1 gave in-place + 80 insns, worse) and a same-TU sibling already matches at your pinned version,
the pin is confirmed correct and the artifact is a patchlevel micro-divergence.

---

## dead-frame reload-artifact regalloc-wall

**Symptom:** a classical fn whose structure, scheduling, loop-hoisting, and instruction sequence are
**fully matched** to the ROM, and the ONLY residual is that the target **reserves a dead stack frame**
— `addiu sp,sp,-N` in the prologue + `addiu sp,sp,+N` in the epilogue with **zero `sp`-relative
load/store between them** — that your build (a leaf fn with no spill) does not, PLUS the pervasive
**register permutation the frame drives** (a first-load `v0`/`v1` swap that cascades, `mfhi t3` vs `t4`,
two locals swapped like `dst`/`row` = `a3`↔`t0`). Confirm the dead frame by grepping the target for
`sp)` inside the fn: none = the `-N`/`+N` is a pure reserved slot, not a real spill. S172
`func_8004DC44` (ring-buffer→grid blit, target reserves a dead 8-byte frame; structure otherwise
byte-identical after the `#top-tested-loop-goto-local-hoist` selective-hoist fix).

**Root cause:** GCC 2.7.2 `reload` assigned a **spill slot** to a pseudo (counted into `frame_size` via
`get_frame_size()`; `mips.c` `MIPS_STACK_ALIGN` rounds one 4-byte slot up to 8; the `addiu sp` emits ONLY
when `get_frame_size()>0` post-reload, and is gcc-emitted, NOT assembler-injected — S173 dump-verified),
then eliminated the actual spill store/load because the value was available in a register at the spill
point — leaving the slot allocated but never accessed (a "dead frame"). No callee-saved regs are involved
(no `s0-s7`, no `ra` save), so the whole frame is that one eliminated spill. Which pseudo spills, and the
register permutation that follows, are set by the exact register-pressure/allocation-order at reload — an
internal artifact.

**S173 deep dive (4 GCC-2.7.2/binutils-2.6 subagents, ~35 variants, 275k permuter iters — dump-verified).**
Two refinements to the S172 framing, both important:
1. **The dead frame IS reachable** (retract "no source trigger for the frame"). A **structured** outer loop
   produces the spill (its LICM hoists an extra invariant into a held reg, raising pressure), and the
   **permuter hit a frame-bearing 75-insn candidate** from the improved seed (frame at the exact ROM
   position). The frame and the goto-vs-structured control-flow choice are **orthogonal** — by reload time
   the loop history is gone (just basic blocks + a conflict graph).
2. **The true, un-source-reachable wall is the coupled register permutation, rooted in a `v0`/`v1` swap in
   a signed-divide-by-constant.** For `x / 40`: `expmed.c` `force_reg`'s the dividend first, then
   `expand_mult_highpart`/`optabs.c copy_to_mode_reg` creates the magic (`0x66666667`) as a **later** pseudo;
   `local-alloc.c`'s **life-length-dominated** priority (`floor_log2(refs)*refs*size/life`, deliberately
   matched to `global.c allocno_compare`) scores the tiny-live-range magic **6666 vs the dividend's 1666**,
   so the magic is allocated first and `find_free_reg` (ascending scan, no `REG_ALLOC_ORDER` on MIPS) hands
   it `$v0`. The ROM assigns dividend→`$v0`, magic→`$v1` with an **outwardly identical instruction sequence**;
   the model above predicts the opposite and reproduces every build — the ROM's assignment is a mechanism
   the model does not capture, so its divide source-shape is **unrecoverable from asm**. Not flippable by
   reorder, extra dividend refs (life grows in lockstep), explicit reciprocal-multiply (`(s64)x*magic>>32`,
   real `mult`, moves the dividend reg but magic stays `$v0`), interleave, or tie-break temps.

**S174 CORRECTION — the divide-swap is FLIPPABLE-IN-ISOLATION, not "irreducible"; retract "unrecoverable
from asm."** An 8-project cross-decomp sweep (all KMC gcc 2.7.2: marioparty1/2/3, snowboardkids2,
drmario64, hm64, puzzleleague64) + ~12 compiler-source subagents + RTL pass dumps (`-dl`/`-dg`/`-ds`)
pinned the real mechanism, and it is a **quotient-destination / register-coalescing** effect, NOT an
unrepeatable artifact. See the dedicated playbook `#signed-divide-const-v0v1-quotient-destination`. In
one line: **dividend→`$v0` requires a physical reg-2 SET (a `(set $v0 …)`/`(set … $v0)` copy) adjacent
to the divide chain**, which lands the chain in local-alloc's **suggestion pass** (`local-alloc.c`
1466-1477 + `combine_regs` 1798-1838) *before* the life-priority general pass. `return g/40` supplies
it (the return copy coalesces back through the in-place `sra`/`subu` chain to the dividend) and
reproduces the ROM's `/40` bytes **exactly**. The reason `func_8004DC44` still can't be matched: it is a
**void, callless, returnless leaf whose quotient feeds arithmetic then a loop-carried store** — it emits
**no reg-2 mention anywhere**, so the chain falls to the general pass where the 2-insn magic (shortest
life) deterministically wins `$v0`. Every faithful lever fails *in that context*: 24 control-flow × src
combos, multi-term dividends, all associativity, and the sched1 lifetime lever (`life_magic >
⅔·life_dividend`; the loop-setup pressure sinks the magic to life ~3 in every form) — all magic→`$v0`.
A `register asm("$2")` binding forces the dividend (28 diffs, down from 38) but is unfaithful **and**
incomplete (leaves the quotient intermediate in `v1` where the ROM uses `a3`, and the frame absent).
So the corrected framing is **"flippable-in-isolation; a void-loop-fed leaf is deterministically
magic→`$v0` by local-alloc"** — the ROM's coordinated dividend-`$v0` + quotient-`a3` + dead-frame is a
sched1/pressure state this toolchain does not produce from any source-equivalent void-leaf input (SA-A
`local-alloc` + SA-B `sched.c`/`reload1.c`, both source-grounded). The dead frame is a **co-symptom** of
the same 3-live-value pressure peak, NOT a cause: reload never reassigns an already-allocated pseudo's
hard reg (only spills to memory), so "add a frame to force `$v0`" is false.

**Verdict — permuter or carry; the frame is reachable but the divide-swap is not (in a void loop-fed leaf).** Once structure +
scheduling + hoisting are settled and the residual is the dead frame + its driven permutation, route to
the permuter (it can reach the frame; it must ALSO flip the divide-swap in the same candidate — low odds)
or **carry**. Do NOT grind source levers for the frame (address-taking a local forces a **live** frame with
real `sp` loads the ROM lacks) and do NOT grind the divide-swap in a **void loop-fed leaf** (per the S174
correction above it is flippable-in-isolation but deterministically magic→`$v0` here — see
`#signed-divide-const-v0v1-quotient-destination`). Save the
structurally-settled near-match so the retry starts from ops-100%-match: for `func_8004DC44` the seed is
**pre-declared base-pointer vars + structured inner `for` + goto outer** (`#top-tested-loop-goto-local-hoist`;
the base-hoist the S172 seed lacked). Sibling to `#pervasive-regalloc-classical-main`,
`#cse-make-regs-eqv-branch-fold`, and `#abs-coalescing-reg-swap`. A **dump-verified negative** (proven
can't-be-source-fixed) is itself a valid, valuable outcome — it converts an open grind into a documented
carry (`#compiler-source-fan-out-escalation-above-the-permuter`).

**Provenance:** S172 `func_8004DC44` (the S171 `print_string_at_grid.c` regalloc-wall carry; its sibling
`print_string_at_grid` banked S172 via `#cross-jump-tail-merge` nested-if). **S173 re-carry** after the
deep compiler-source dive above; improved seed + full analysis in `docs/wip/func_8004DC44.wip.md`. **S174
re-carry** after the cross-project + coalescing correction above (retired the "irreducible/unrecoverable"
framing) — see `#signed-divide-const-v0v1-quotient-destination`.

## signed-divide-const v0/v1 quotient-destination

**Symptom:** a signed divide-by-constant (reciprocal-magic highpart-multiply: `mult div,magic; mfhi;
sra hi,k; sra div,div,31; subu`) is byte-close but the **dividend and the magic constant occupy the
wrong two registers** — your build puts the short-lived magic in the numerically-LOWER reg (`$v0`) and
the dividend in the higher (`$v1`), while the ROM has the reverse (dividend→`$v0`, magic→`$v1`), or vice
versa. Common magics: `0x66666667` (/5,/10,/20,/40,/80), `0x55555556` (/3), `0x2AAAAAAB` (/6),
`0x38E38E39` (/9), `0x51EB851F` (/100), `0x1B4E81B5` (/4800), `0x92492493` (/7 or /14 w/ add-back). The
sign-extended reg (`sra r,r,0x1f`) IS the dividend; the `lui 0x<magic>` reg is the magic. **The add-back
shift disambiguates the divisor for a shared magic:** `0x92492493` + `sra hi,2` = `/7`; the SAME magic +
`sra hi,3` (one more shift, since 14 = 7<<1) = `/14`. S184 `func_80044470` divided the loop index by 14
(shift-3); a `/7` C emitted shift-2 and missed by that one bit — read the post-`mfhi` `sra` amount, not
just the magic, to pin the divisor.

**Root cause (RTL-dump + `local-alloc.c`/`sched.c` grounded, S174).** Neither operand carries a hard-reg
suggestion by default (a `li` of the magic and a `lw`/compute of the dividend, tied to no hard reg), so
both fall to local-alloc's **general (life-priority) pass**: `qty_compare` priority `=
floor_log2(refs)*refs*size / (death-birth)`, higher wins, and `find_free_reg` scans hard regs ascending
(MIPS has **no `REG_ALLOC_ORDER`**), so the earlier-ordered qty takes the lower reg. The magic's
2-insn life makes `pri_magic` (~6666) ≫ `pri_dividend` (~1666), so **magic→`$v0` is the DEFAULT** for a
plain dividend. **The dividend wins `$v0` only when it out-lives-or-out-suggests the magic**, via one of
these levers (empirically confirmed across 8 KMC gcc 2.7.2 decomps):

1. **Quotient reaches `$v0` directly (a reg-2 SET → the suggestion pass).** `return x/40;`, or `y=x/40;
   return y;` — the return copy `(set $v0 quotient)` records a `$v0` **copy-suggestion** (`combine_regs`,
   `local-alloc.c` 1824-1838) on the divide chain's qty (dividend/sign/quotient share one qty via the
   in-place `sra`/`subu` union, 1840-1885), and the **suggestion pass** (1466-1477) pins it to `$v0`
   *before* the general pass runs. **ANY arithmetic on the quotient before it reaches `$v0` breaks this**
   (`-(x/40)`, `x/40+K`, a store, a call-arg all give magic→`$v0`). This is the cleanest lever, and the
   only one available to a fn that returns the quotient.
2. **Multi-term dividend `(a±b)/K`.** The extra operand load steals `$v0` from the magic (can flip even
   under a store). Changes the asm (adds the operand load), so it only helps if the ROM's dividend is
   genuinely multi-term.
3. **Magic CSE-shared across ≥2 same-family divides** (`/5,/10,/20,/40,/80` all emit `lui 0x6666;ori
   0x6667`, e.g. `x/10%10` = two `/10`s): the shared magic's live range outlasts the single-use dividend
   → dividend wins. (marioparty3 `UpdatePlayerBoardStatus` A/B is the decisive matched proof.)
4. **Magic loop-hoisted to a callee-saved reg** (divide inside a loop, loop-invariant magic → `$sN`):
   the per-iteration dividend then wins any low temp.
5. **Indexed struct/array-member or call-return dividend** (`base[i].f/K`, `f()/K`): the dividend is
   freshly materialized into `$v0` at the divide (pressure-dependent, not deterministic).

**The BLOCKER (why a void loop-fed leaf can't flip):** a **void, callless, returnless** leaf whose
quotient feeds arithmetic then a **loop-carried store** emits **no reg-2 mention anywhere** → no
suggestion (lever 1 unavailable), the loop-carried `src` is a multi-block pseudo (global.c, not unioned
with the local divide chain), and the loop-setup register pressure sinks the magic to life ~3 in **every**
control-flow structure / associativity / schedule (so `life_magic > ⅔·life_dividend` is unreachable).
`func_8004DC44` is exactly this. The `register asm("$2")` binding creates a synthetic reg-2 SET and does
force the dividend to `$v0`, but it is non-idiomatic (shows as `asm` in the decompile) **and** does not
reproduce the ROM's coordinated allocation (quotient→`a3`, dead frame) — not a faithful match (S175:
applied to the frame-correct base it is *worse*, 51 diffs — the forced dividend leaves the quotient in
`$v1` where the ROM uses a fresh `$a3`, proving the ROM state is a *coordinated* `{dividend→$v0,
quotient→$a3-fresh, dead-frame}` coloring, not a single-reg pin).

**Do NOT reach for the plain `register` keyword (no `asm`) as a lever — it is a zero-`.text`-effect
no-op at -O2** (S175, controlled A/B: `s32 seed` vs `register s32 seed` at identical structure →
`.text` byte-identical; both keep the swap). Source-proven three ways on `mips-gcc-2.7.2`: (a) at -O2
`obey_regdecls==0`, so `DECL_REGISTER` is ignored (`stmt.c:3364`), and a plain local ALSO gets
`REG_USERVAR_P` (`stmt.c:3390`) → register-vs-plain RTL is identical; (b) `REG_USERVAR_P` has **0 hits**
in `local-alloc.c` and `global.c` — it is absent from the `qty_compare` priority, the `find_free_reg`
scan, and the suggestion machinery, so it cannot reorder the dividend-vs-magic allocno; (c) the flag
*does* survive into the multiply's dividend operand (`force_reg` passes a REG through, `explow.c:638`;
no `PROMOTE_MODE` on MIPS → no stripping SUBREG; the magic is always a fresh `copy_to_mode_reg` pseudo,
`optabs.c:474`) but per (b) allocation never reads it, so reaching it is moot. A specific hard reg needs
`register T x asm("$N")` (routes `toplev.c:2601` → `varasm.c:536`), which is the unfaithful hack above.
**S178 re-confirmed the split empirically:** on `func_8004E2DC`, plain `register OSIntMask mask` was a
`.text` no-op, but `register OSIntMask mask asm("$9")` DID force `mask`→`$t1` at -O2 (all registers then
matched the ROM, leaving only a 2-instr scheduler swap). So `asm("$N")` is a valid DIAGNOSTIC to confirm
"which register does the ROM want" — but it stays an unfaithful hack for a bank (and here still left a
residual), so a clean SOURCE lever must reproduce the same allocation (S178 did it via the
[define-after-call caller-saved placement](#loop-weight-and-live-length-regalloc-steering) + inline sentinel).

**Escalation.** For a same-toolchain reg-alloc wall, mine the OTHER N64 decomps for a **matched** analog
of the exact pattern (see `#compiler-source-fan-out-escalation-above-the-permuter` for the cross-project
+ RTL-dump methodology); if none exists (as here — no matched plain-global single-magic dividend wins
`$v0` in a real fn across 8 projects), the ROM's assignment is a scheduling/pressure state the toolchain
doesn't reproduce → carry. Sibling to `#dead-frame-reload-artifact-regalloc-wall` (the frame is a
co-symptom of the same pressure peak) and `#register-reuse-nudge-classical-regalloc`.

**Provenance:** S174 `func_8004DC44` cross-project sweep (marioparty1/2/3, snowboardkids2, drmario64,
hm64, puzzleleague64; matched dividend-`$v0` examples: puzzleleague64 `gTheGame.menu[i].unk_4/100`
indexed, hm64 `(a+b+c)/3` multi-term, marioparty3 `x/10%10` CSE). Corrects the S172/S173
`#dead-frame-reload-artifact-regalloc-wall` "irreducible" framing.

## nested-function static-chain spill

**Rule.** A **leaf** function that opens with `addiu sp,sp,-8` + a `sw $v0,0(sp)` that is **never
reloaded** — a dead spill of the *incoming* `$v0` — and whose single caller sets `$v0 = &sp[N]` (a
pointer into the caller's OWN frame) right before **each** `jal` to it, is a **GCC nested function**.
The dead store is GCC saving the incoming **static chain**: MIPS o32 passes the static chain in
`$v0` (`config/mips/mips.h` `#define STATIC_CHAIN_REGNUM (GP_REG_FIRST + 2)` = `$2`). For any
lexically-nested function, `function.c` `expand_function_start` (~:5011-5023) grabs the first stack
slot (offset 0) and `emit_move_insn(last_ptr, static_chain_incoming_rtx)` → `sw v0,0(sp)`; the body
never reads the chain, so it is a dead spill. The caller side is `calls.c:293`
`emit_move_insn(static_chain_rtx, static_chain_value)` → the parent loads `$v0 = &<its frame>` before
the call. This is **NOT** a `#dead-frame-reload-artifact-regalloc-wall` (that has no source trigger;
this one does — the nesting).

**Tell (the discriminator).** Distinguish from a random dead-frame artifact by reading the CALLER:
if the caller materializes `$v0 = &sp[K]` (an `addiu v0,sp,K`) into the `jal`'s live range for **each**
call site, it is the static chain, and the callee is nested inside that caller.

**Real bank.** Write the function as a C **nested function** inside its caller/parent (same TU):
```c
void parent(void) { /* = the caller */
  void child(u32 a, u32 b) { ... }   /* the nested fn; GCC mangles the symbol to child.N */
  ... child(x, y); ...               /* parent sets $v0 = static chain before the jal */
}
```
The mangled `child.N` symbol is irrelevant to the ROM-byte oracle (splat names by address). The
parent and child are **one compilation unit** — they cannot be split into separate `.c` files.

**Decompose-gate corollary (orphaned nested child).** A subseg decompose can **orphan** a nested
child from its parent: the child's `.text` sits *before* the parent's (GCC emits the nested fn
ahead), so the true TU boundary is the child's address (often non-16-aligned), not the parent's. When
a pack member's caller passes a static chain, keep the parent + child in **one** increment, or carry
the child to the parent's sprint. Do NOT bank the child alone. (S176: `func_8004E184` at `0x8004E184`
is nested in `func_8004E1E0` at `0x8004E1E0`; the true TU boundary `0x8004E184` is non-16-aligned, so
cluster A's `0x295E0` split over-reached by one fn — the child was carried.) `pick_target.py` flagging
a `static-chain-callee` (a fn whose caller does `addiu v0,sp,K` before its `jal`) is a tracked
follow-up.

**Callee-side tell + pre-classify the whole pack at seed time (S191).** You do not need the caller to
spot a nested child: the child's OWN entry carries the tell. Two forms, both read straight off the
child's `.s`: (a) **dead-spill leaf** — `addiu sp,-8; sw v0,0(sp)` with `$v0` never reloaded (a nested
fn that does NOT use its static chain; GCC still frames it, "pure-leaf still framed when nested"); and
(b) **chain-using** — `sw v0,X(sp); lw v0,OFF(v0)` (or `move sN,v0` then `lw ...,OFF(sN)`), the child
dereferencing the parent's frame through the incoming `$v0`. Either way `$v0` is meaningful on entry,
which is impossible for a standalone o32 fn (`$v0` is not an incoming arg). **Because GCC emits nested
children immediately BEFORE their parent, a run of such fns sitting just ahead of a big FP/complex fn
are its children.** So at pack-seed time, scan every member's `.s` for the tell FIRST and route the
nested children to carry-with-parent BEFORE seeding — do not waste an m2c seed + iterate loop on an
orphan that cannot bank standalone. S191 `get_table_entry.c`: 6 of 11 fns were nested children of two
FP-wall parents (`update_ball_physics`, `init_ball_for_shot`); pre-classifying focused the seed effort
on the 3 true standalone fns (all banked). The `static-chain-callee` `pick_target.py` follow-up should
also emit the callee-side `nested-child:<parent>` tag from this dead-`$v0`-spill / `$v0`-base-load
signature (no caller scan needed).

**Recombine-to-land-the-child (the bank enabler, S177).** To place the child at its non-16-aligned
address, RECOMBINE the decomposed pack into ONE object: at the gate, remove the inner `[<child-addr>,
asm]`-side subseg line so the C subseg spans the WHOLE pack from its 16-aligned start, and add
`INCLUDE_ASM` stubs for the pulled-in fns. The object then starts at the 16-aligned pack head, and GCC
emits the nested child just ahead of the parent — landing it at its true mid-object offset with no
alignment gap. (S177: removed `[0x295E0, asm]`, extended `[0x29260, c, main/func_8004DE60]` over the
whole 9-fn pack; `func_8004E184` then compiled to `0x8004E184` as `func_8004E184.N` inside
`func_8004E1E0`.)

**CSE-reload wall on the PARENT (S177 carry, subagent-verified).** The child can bank perfectly and
the PARENT still be a wall. `func_8004E1E0`'s init does `D_800DC6E0[3].next = D_800DC6E0[3].prev`,
which the ROM emits as a **RELOAD** of the just-stored field (`lui v1,%hi(D_800DC734); lw
v1,%lo(...)`), but KMC GCC 2.7.2 -O2 value-FORWARDS it (reuses the register) for any faithful C. The
reload is a CSE **varying-address invalidation** (cse.c `note_mem_written` sets `nonscalar` only for a
`(plus reg off)` runtime base/index, purging the in-struct cache → reload); the ROM's field stores are
pure ABSOLUTE (`(symbol+off)`, non-varying → precise invalidate → forward). Absolute stores + a reload
are **mutually exclusive** under this profile (the reload-triggering register is the same one that
blocks the absolute fold), and register pressure does NOT trigger it (pressure-tested negative). So the
init is unreachable from faithful C AND the permuter is blocked (pycparser rejects the nested fn) — a
genuine carry, well-characterized like [#signed-divide-const-v0v1-quotient-destination](#signed-divide-const-v0v1-quotient-destination)
and [#dead-frame-reload-artifact-regalloc-wall](#dead-frame-reload-artifact-regalloc-wall).

**Recipe confirmed end-to-end (S184, 4 nested fns).** The "Real bank" recipe byte-matched two full pairs
in `src/main/func_80043C20.c`: `func_80044470` ⊂ `func_800444B8` (divide-by-14 table lookup) and
`func_80043C20` ⊂ `func_80043C64` (a `for(;;){c=*s++; if(!c)break; if(pos<end)*pos++=c;}` string-appender
that reaches the parent's `buf_pos`/`buf_end` at static-chain `+0x100`/`+0x104`). Three confirmations:
(1) **Child-emits-before-parent lands the child.** GCC outputs the nested child immediately BEFORE the
parent, so writing the parent's C at the source position where the CHILD's lead-vram stub sat places the
child at that lead vram — `func_80043C20` is the pack lead (0x80043C20) and banked by defining
`func_80043C64` first with the child nested inside it (child → local `func_80043C20.2` at offset 0).
(2) **A pure leaf helper is still framed when nested.** A nested child that references NO parent local
(e.g. the `lerp` helpers `(s32)((f64)from + (f64)(to-from)*(f64)t)`, `t` an `f32` in `$a2`) STILL gets the
`addiu sp,-8; sw v0,0(sp)` chain prologue — compiled standalone it is 14 instrs with no frame, nested it is
17. So a leaf math helper whose ONLY caller sets `$v0 = &sp[K]` before the `jal` MUST be written nested to
match; a standalone def will never emit the frame. (3) **Parent can wall while the child matches.**
`predict_shot_distance` + `predict_shot_distance_variant` carried on `#pervasive-regalloc-classical-main` /
`#abs-coalescing-reg-swap` while their nested `lerp_int_v2`/`lerp_int` children were byte-exact — the pair
carries together (the child cannot bank without the parent's `.text`).

**Standalone reproduction (UB; do NOT prefer over the real nested form).** A byte-exact standalone
`.c` (no parent, no inline asm) can force the same spill: `volatile u32 a = (u32)uninit_ptr;` reads an
**uninitialized** pointer local (which local-alloc parks in `$v0`, the first allocable GPR since MIPS
has no `REG_ALLOC_ORDER`), the `volatile` store survives DSE (`flow.c` `insn_dead_p && !INSN_VOLATILE`)
and reserves the 8-byte frame, then the same reg is reused for the real value → `sw v0,0(sp)`. This is
UB and is **not** the original source; use only if the parent is genuinely unavailable and the PO
accepts a documented pseudo-fakematch. `#capturing-ra` (which also reads a fixed reg) and
`#dead-frame-reload-artifact-regalloc-wall` are the sibling reg/frame hazards.

**Provenance:** S176 `func_8004E184` (slot-3 list-insert leaf, a nested fn of the heap-init
`func_8004E1E0`); mechanism dumped from `mips-gcc-2.7.2` (`mips.h` STATIC_CHAIN_REGNUM, `function.c`
expand_function_start, `calls.c` static_chain pass). Two subagents converged: one on the nested-fn
structure, one on the standalone UB reproduction; PO chose to carry for the real nested form.

## local-alloc qty-permutation (1-basic-block reg swap, permuter-appropriate)

**Symptom (S204 `func_80050428`).** A straight-line classical fn (no branches) byte-matches the ROM in
mnemonic sequence but locks on an s-register **permutation** (e.g. target `off→s4, slot→s3, base→s2`;
build `off→s1, slot→s4, base→s3`) plus one independent-store schedule move. Not structural, not FP.

**Root cause (gcc-2.7.2 codegen dive).** A fn with ONE basic block is register-allocated by
**`local-alloc.c` (QTYs), NOT `global.c`** — confirm with the `.flow` dump (`1 basic blocks`). Local
allocation orders QTYs by priority `floor_log2(n_refs)·n_refs·size / (death−birth)` (`local-alloc.c`
`qty_compare` :1579), tie-broken by lower qty number (:1622). `find_free_reg` (:2073) returns the
**lowest-numbered free callee-saved reg** (MIPS has no `REG_ALLOC_ORDER`, so the scan is ascending
`$16=s0…$23=s7,$30=s8`), so the register NUMBER a value gets is just its rank in the priority order,
minus regs already live in its range. When two competitors' priorities are close (e.g. a 2-ref
short-life index vs a 3-ref long-life pointer) they swap, rotating the whole assignment by one.

**Do NOT hand-iterate the source.** GCC's pre-alloc scheduler normalizes QTY births, so decl-order,
expression-association, and pointer-hoist rewrites mostly DON'T move the permutation (S204: 5 source
levers, zero movement). This is exactly the local-allocation space the **decomp-permuter** explores —
run it (`setup-permuter.sh --main`, then `run-permuter.sh <fn> --stop-on-zero`). S204 found score 0 at
iteration ~14250. Two winning shapes the permuter surfaces here recur:
- **Reference a global INLINE, not via a pointer local** (`(u32)(D_E473F0 + x)`, not `u8* base =
  D_E473F0; base + x`). The `base` copy creates an extra QTY that shifts the priority tie-breaks; the
  inline form matches the ROM's qty numbering. (A leftover dead `u8* base;` decl the permuter keeps is a
  no-op; remove it and re-verify SHA.)
- **Cache a re-read value in a local to PIN an independent store's schedule slot.** The ROM stores
  `D_8012D3A8 = index` between the `end` and `size` stores; the build hoisted it early. Reading
  `size_val = *(u32*)size_aligned;` into a local right before the `D_8012D3A8` store forces that order.

Also: **local (stack-slot) layout follows array DECL order** — `func_80050428`'s two 16-aligned DMA
scratch buffers landed at `sp+0x1F`/`sp+0x5F` per which array was declared first; a 2-byte
`27b1005f`↔`27b1001f` miss is fixed by swapping the two array declarations (not a codegen lever).

**`nonmatching-func` isolated object can DIVERGE from in-tree for a 1-BB fn.** S204's isolated
`make nonmatching-func` / `decomp_loop` object showed a DIFFERENT local-alloc result than the in-tree
build of the same fn (the minimal isolated context perturbs local allocation). **Gate on the in-tree
object byte-`cmp` + the full ROM SHA-1, not the isolated score**, for a one-basic-block fn. (The
isolated near-miss score is still a fine permuter SEED; just don't trust it as the match oracle.)

**Multi-BB analog — `global.c`, not `local-alloc.c` (S209 `func_8005C458`).** A fn with a NESTED loop
has multiple basic blocks, so its cross-BB pseudos are allocated by **`global.c` (allocnos)**, the
global analog of the QTY rule above (confirm with the `.greg` dump vs `.lreg`). `allocno_compare` orders
allocnos by priority `floor_log2(n_refs)·n_refs·size / live_length` (**`global.c`:594-601**), higher
first, tie-broken by lower allocno number; `find_reg` scans hard regs **ascending** (no `REG_ALLOC_ORDER`
in `mips.h`), so the register NUMBER a value gets is just its rank in the priority order. When two
competitors have EQUAL `n_refs`, **live_length decides** — the shorter-lived one wins the lower reg.

**Allocno live-length steer (the S209 C458 lever).** To flip which of two equal-ref values takes the
lower caller-saved reg, change one's LIVE LENGTH at the source. `s8 *base = (s8*)func_8005AF50()`
coalesces `base` with the `$v0` return, so its defining `move` materializes FIRST (right after the call)
→ long live range; the row counter `i` (a later `li`) then out-prioritizes it and grabs the lower reg
(direct: base live_length 14 > i 12 → i wins `$a2`, base `$a3`). An **intermediate copy** —
`u8 *ret = func_8005AF50(); s32 count=0,i=0,n=6; s8 *base = ret;` — gives `base` a SEPARATE pseudo whose
`move $a2,$v0` is deferred to AFTER the const inits → short live range (base live_length drops to 11 <
i's 13) → base out-prioritizes `i` and takes `$a2` (and count lands `$a1`, end `$a0`). This is the exact
lever the permuter cannot reach — it never restructures the call materialization (see the walled-permuter
note in [#compiler-source-fan-out-escalation-above-the-permuter](#compiler-source-fan-out-escalation-above-the-permuter)).
Cross-ref [#loop-weight-and-live-length-regalloc-steering](#loop-weight-and-live-length-regalloc-steering).

**FOUNDATIONAL — the KMC `cc1` has NO instruction scheduler (S209, confirmed).** `INSN_SCHEDULING` is
undefined for this MIPS build, so `-fschedule-insns` / `-fschedule-insns2` are **byte no-ops** (verified:
the `.o` is identical with and without them). Therefore **SOURCE EMIT-ORDER is the only control over
instruction ordering** — there is no post-pass that reorders. Every apparent "schedule transposition" in
these playbooks (S208 B070 `li`/`la`; S209 C5B4 `li $t0,6` placement; the base-copy materialization
above) is really RTL EMISSION order = source statement/decl order. Reach for source restructuring, NOT a
scheduling barrier. (The one reorderer that remains is `loop.c` via `NOTE_INSN_LOOP_BEG` — a loop
optimization, not a scheduler; see the goto-loop-vs-structured-loop codegen memory.)

## cse-derived-pointer base-canonicalization

**Symptom (S205 `func_8005E380`).** A large straight-line fn takes ONE pointer parameter `T* p`,
derives a fixed-offset sub-object pointer `sub = &p->big_substruct` (constant offset, e.g. `OSThread*
thread` → `ctx = &thread->context` at +0x20), and does MANY `sub->field` accesses. Every build keeps
the **parameter** in a callee reg (`move $16,$4`) and folds the sub-offset into each displacement
(`0x11C($16)` for `ctx->pc`); the ROM instead materializes the sub-pointer as the base
(`addiu $17,$4,0x20`, then `0xFC($17)`) with the buffer/other pointer in the other callee reg. Result:
a **pervasive** base-register + uniform-displacement-offset diff on EVERY field access. The isolated
`decomp_loop` shows `match_count == total_rows`, `top_mismatches == []`, yet a LOW percent (S205: 0.48)
— looks like the `#io_write/io_read`/isolated-compile artifact, but the in-tree `diff.py` confirms it
is a REAL pervasive near-miss (see the isolation-caveat note in `## Execution loop`).

**Root cause (gcc-2.7.2, confirmed by exhaustive bisect).** CSE (`cse.c` `fold_rtx` /
`simplify_plus_minus` address canonicalization) always canonicalizes `(plus (plus param C) off)` to
`(plus param (C+off))` — it prefers the **base PARAMETER** as the single canonical base and eliminates
the `param+C` intermediate, extending the parameter's live range across the calls (hence the
`move $16,$4` preservation). It will not keep the derived `param+C` as the base even when the ROM does.
The choice of WHICH of {param, sub, buf} keeps a callee reg is then a local-alloc tie, but the
canonicalization itself is upstream and deterministic.

**Levers that DO NOT work (all proven inert, S205 — do not re-try):**
- Source (8): `register` kw, `(T*)((u8*)p + C)` char\* cast, an eager `sub`+`id` temp block, decl
  order, and reading a pre-substruct field via `sub[-k]` (GCC **re-folds** `ctx[-3]` back to
  `thread+0x14`). None move the score off the base fold.
- Flags/opt (19): `-O0/-O1/-O2/-O3`, `-g`, `-funroll-loops`,
  `-fno-{gcse,cse-follow-jumps,cse-skip-blocks,rerun-cse-after-loop,expensive-optimizations,defer-pop,strength-reduce,schedule-insns,schedule-insns2,...}`. Every one folds.
- Permuter (`--main`, best-only, ~45k iters): valid-floor only ~1200 permuter-score (base ~27760 /
  0.48 decomp_loop); anything lower is **semantically INVALID** (drops a print call, or uses `sub`
  uninitialized) — the same UB-drift failure mode as the `func_8003E004` permuter runs.

**Verdict.** This is a compiler wall distinct from `#local-alloc-qty-permutation` (that keeps the
right base, permutes only s-regs — permuter-crackable) and from `#move_movables` FP-hoist
(`func_8003E004`). It is NOT blind-retryable: it needs a from-scratch permuter seeded PAST the base
fold, or a source form that forces the `param+C` materialization (none found across 8 forms). RE the
body fully (it is 100% structural), commit a gold in-file root-cause note, and **carry** — do not burn
sprints re-attempting the same levers. Detector signal for `pick_target.py` (queued, off-cadence
golden-gated): single pointer param + large sub-struct-base dump access pattern + FP-reg reads →
price permuter/carry-expected (kin to the S158/S177/S183/S203/S204 regalloc-heavy pts follow-up).

## integer-arith commutative operand order (pointer_int_sum pins pointer-first)

**Symptom (S209 `func_8005C458` family).** A fn is byte-exact except a single commutative `addu` whose
two register operands are SWAPPED vs the ROM: ROM `addu $a0,$t1,$v1` (n-first), build
`addu $a0,$v1,$t1` (p-first). The swapped value is a pointer+int sum (`end = p + n`).

**Root cause (gcc-2.7.2 C front end).** For `pointer + integer`, `c-typeck.c` `pointer_int_sum` always
builds the tree with the **pointer as operand 0**, so the RTL `plus` is `(plus p n)` → `addu d,p,n`
regardless of source spelling (writing `n + p` folds straight back to `p + n`). The commutative
canonicalization is upstream and deterministic; no decl-order / `register` / re-association source lever
moves it while the sum stays pointer arithmetic (all verified inert, S209).

**Lever.** Do the address math in the INTEGER domain so it is an ordinary commutative int add kept in
SOURCE operand order: `u32 p = (u32)base + 0x1320; u32 end = n + p;` emits `addu end,n,p` (n-first).
Dereference through a cast — `*(s8*)p` (use `s8*` for the signed `lb`, not the `u8*`→`lbu`). The
`(u32)`/`(s8*)` casts are register no-ops (zero extra instructions). Pairs with the base-copy live-length
lever; see [#grid-counter-double-loop](#grid-counter-double-loop).

**Provenance.** S209 C458/C4B4/C5B4/C614 (all banked byte-exact, no permuter).

## setup-block instruction order (a pre-base loop-limit const must be a variable)

**Symptom (S209 `func_8005C5B4` / `func_8005C614`).** A one-instruction transposition in the post-call
setup block: the ROM emits the loop-limit `li $t0,6` BEFORE the `move $a2,$v0` base capture; the build
emits it AFTER. Everything else byte-exact.

**Root cause.** With no scheduler (see the no-scheduler note in
[#local-alloc-qty-permutation](#local-alloc-qty-permutation)), the setup block is emitted in pure source
order. A loop limit written as a bare literal in the exit test (`while (i != 6)`) is materialized lazily
at first use — after the base copy. Declaring it as a VARIABLE among the pre-base consts forces its `li`
to emit with them, before the base capture.

**Lever.** When the inner extent and the outer limit DIFFER (so they are not one shared `n`), declare
BOTH as pre-base variables: `s32 n = 18, lim = 6;` (before `s8 *base = ret;`), and test
`while (i != lim)`. This orders `li $t1,0x12` and `li $t0,6` ahead of `move $a2,$v0`. (When inner ==
outer, one shared `n` suffices — the C458 6×6 case, off 0x1320.)

**Provenance.** S209 C5B4 (`>=50`) / C614 (`==108`), 6×18 grid, off 0xA84, row stride 0x12.

## struct-copy block-move path (alignment picks lwl/lwr vs word-loop)

**Symptom (S209 `func_8005B28C`).** A whole-struct copy `dst = *src` (or `arr[idx] = *src`) matches or
misses depending on whether GCC emits a plain aligned word loop or a runtime-aligned `lwl`/`lwr` dual
path. The ROM here is the aligned form (a 6×4-word unrolled loop + a 2-word tail, NO `lwl`/`lwr`).

**Root cause.** GCC's block-move expansion picks the path from the struct TYPE's ALIGNMENT. A 1-byte-
aligned struct (`u8` members, e.g. `u8 buf[104]`) forces a runtime align-check + an `lwl`/`lwr`
unaligned dual path; a WORD-aligned struct (members are `s32`, so the type is 4-aligned) emits only the
aligned word-loop + tail.

**Lever.** Model the record with word-aligned members so the type alignment selects the aligned path.
S209 B28C: declaring the 104-byte record as `struct { s32 unk_00[26]; }` (not `u8[104]`) yielded the
ROM's aligned block-move, byte-exact. Match the ROM's path by picking the struct alignment.

**Provenance.** S209 B28C (banked; `D_80131510[idx] = *src` + `func_8005DF54(func_8005AF50(), 1)`).

## gas .set-reorder delay-slot fill (textual layout != machine)

**Symptom (S209 `func_8005B070`).** The `-S` output shows a branch immediately followed by an
instruction (e.g. `beq $2,$0,.L; addu $3,$3,-1`), which READS like a filled delay slot — but the
assembled object has a `nop` in the slot and the `addu` AFTER it. A "the delay is already filled, we are
1 insn short of the ROM's nop" verdict taken off the `.s` is a MISREAD.

**Root cause (binutils-2.6 gas).** Under `.set reorder`, gas fills a branch delay slot by swapping the
branch with the **PRECEDING** instruction ONLY (never the following one), and inserts a `nop` when that
swap is unsafe — e.g. the preceding insn writes a register the branch reads (`tc-mips.c` ~L1525-1660).
For B070 the preceding `slt` writes `$v0` and the `beq` reads `$v0`, so the swap is blocked → `nop`; the
textual next insn is just the fall-through, emitted after the slot.

**Lever / rule.** Judge delay slots on the ASSEMBLED object, never the `.s`: `objdump -d` the `.o`. This
is the assembler analog of the compiler-source-fan-out doctrine "the empirical agent must judge the `.o`,
never `-S`." Cross-ref [#assembler-differences--byte-cmp-spot-check](#assembler-differences--byte-cmp-spot-check).

**Provenance.** S209 B070 (banked, no permuter). The while-form `while (i!=0){ if(arg0>=*p) break; i--;
p--; }` was already byte-exact once assembled.

## reorg optimize_skip annulled bnel (single-skipped-insn branch-likely)

**Symptom (S209 `func_8005D308`).** A ternary / `if` chain that should emit a branch-LIKELY `bnel`
(annulled, e.g. `0x54400005`) to a shared exit instead emits a negated `beql` + an extra `j`, or a plain
(non-annulled) branch — off by the annul bit and often +1 instruction.

**Root cause (gcc-2.7.2 reorg.c).** The annulled `bnel` comes ONLY from `optimize_skip`
(`reorg.c`:1112-1215), gated (`reorg.c`:2960) by `delay_list == 0`: it fires when a conditional branch
skips exactly ONE insn AND the delay slot is still empty after backward fill. A ternary / if-else-if
branches AWAY (`beql` + extra `j`); a pre-assigned default gets the right direction but a PLAIN branch
when backward fill already grabbed the preceding `addu` (`delay_list != 0` → `optimize_skip` gated out).

**Lever.** Shape the source so the branch skips exactly ONE fall-through insn with the delay still empty:
put the single skipped store in the ELSE arm — `if (x >= 6) { … } else r = x + 0x1EB;` — so the
fall-through is one skippable insn and `optimize_skip` fires the annulled `bnel`.

**Provenance.** S209 D308 (banked, no permuter).

## switch tight merged-default (shared case-0/default label + case-1 last)

**Symptom (S209 `func_8005D218` / `func_8005D2E4`).** A 2-value switch (`(x==1)?A:B`) should compile to
a tight `beqz`/`beq` fall-through with the default MERGED into case 0 (no extra `j`); naive
`switch`/`if`/ternary forms emit `bne` + `j` + a duplicated `move` (+1 instr), or fold to a branchless
`xori`/`sltu`/`subu`/`andi`.

**Root cause (gcc-2.7.2 jump.c).** Writing two separate zero-returning blocks lets -O2 `jump2`
cross-jumping merge case-0 into the default, which places a `j default` BEFORE the case-1 body → fires
the "conditional jump over an unconditional jump" rule (`jump.c`:1739) → `invert_jump` (`jump.c`:1757)
rewrites the case-1 dispatch to `bne` + an extra `j` (the +1).

**Lever.** Make `case 0:` and `default:` SHARE one label and emit `case 1` LAST:
`switch (x) { case 0: default: return B; case 1: return A; }`. The shared label puts one `B`-return
`code_label` physically before the case-1 body, so there is nothing to cross-jump; the case-1 dispatch
stays `beq`, the `j default` is deleted as a fall-through, and reorg fills the `beq` delays → the exact
tight merged-default.

**Provenance.** S209 D218 (`(x==1)?7:0`) / D2E4 (`(x==1)?157:158`), both banked, no permuter.

## grid-counter double-loop (non-zero-cell counter idiom)

**Symptom (S209; ≥4 instances in `func_80059BA0.c`: `func_8005C458`/`C4B4`/`C5B4`/`C614`).** A small
(~23-insn, one `jal`) fn calls `func_8005AF50()`, then walks a fixed R×C byte grid at a constant base
offset counting non-zero cells, and returns a predicate on the count (`== 36`, `>= 30`, `>= 50`,
`== 108`). The game reuses this idiom heavily — expect more. The instruction sequence RE's cleanly but
the naive source locks on a base/counter regalloc swap + a commutative operand order, and the permuter
WALLS (C458: ~1.27M iters / score 55).

**Recipe (byte-exact, no permuter) — combine the S209 levers:**
- **Base via an intermediate copy.** `u8 *ret = func_8005AF50(); s32 count=0,i=0,n=…; s8 *base = ret;`
  so `base` out-prioritizes the row counter for `$a2`
  ([#local-alloc-qty-permutation](#local-alloc-qty-permutation), the allocno live-length steer).
- **Integer-domain end.** `u32 p=(u32)base+OFF; u32 end=n+p; do { count += (*(s8*)p != 0); p++; } while
  (p != end);` for the n-first `addu` ([#integer-arith-commutative-operand-order](#integer-arith-commutative-operand-order)).
- **Shared vs unshared `n`.** Inner-extent == outer-limit (6×6, off 0x1320) → one `n` serves both;
  they differ (6×18, off 0xA84, row stride 0x12) → `n=18` for the inner end and a SEPARATE pre-base
  **variable** `lim=6` for the outer test ([#setup-block-instruction-order](#setup-block-instruction-order)).
- **Row advance + predicate.** `base += stride; i++;` then the return predicate sets the final
  `slti`/`xori`/`sltiu` (`== 36` → `xori,0x24`+`sltiu,1`; `>= 50` → `slti,0x32`+`xori,1`; etc.).
- **`count`→code DISPATCH-TAIL variant (S210 `func_8005C510`).** The same grid-counter body can end in a
  `count`-to-code dispatch instead of a boolean predicate: a `bne`-chain that maps discrete counts to
  small codes, `if(count==4)return 1; if(count==8)return 2; …; return (count==20)?5:0;` (the LAST case
  folds to the branchless `xori/sltiu/negu/andi` form). Write the whole tail as an `if`-chain of equality
  tests + a final ternary for the branchless case; it banks byte-exact on top of the grid-counter body,
  no permuter. Do NOT reach for a `switch` (the ROM is a plain equality `bne`-chain, not a jump table).

**Provenance.** S209 C458 (`==36`), C4B4 (`>=30`), C5B4 (`>=50`), C614 (`==108`) — all banked byte-exact,
0 permuter, via the compiler-source dive ([#compiler-source-fan-out-escalation-above-the-permuter](#compiler-source-fan-out-escalation-above-the-permuter)).
S210 `func_8005C510` (6×6 grid, `count`→code dispatch tail) banked first-build reusing the same levers —
the family is a reliable `family-of:func_8005C458` bank (see the tracked ranker follow-up in
[#base-register-vs-displacement](#base-register-vs-displacement)).

## base-register-vs-displacement (full symbol/base materialized vs %lo-in-displacement)

**Symptom (S210; `func_8005DE88` / `func_8005AF80` / `func_8005CEE0`).** A classical fn is byte-exact
except a run of N register-only (`r`) diff rows all on ONE data-access chain: the ROM materializes a
FULL base address into a register and dereferences with a `0` (or small) displacement
(`lhu/lw/sb …,0(reg)` after a `lui;addiu` la-pair, or a walking base pointer `0(v1)`), where the build
keeps `%hi(sym)` in a scratch reg, adds the variable index, and folds the constant tail (`%lo(sym)`, or
a fixed struct offset like `0xDC0`) into the load/store DISPLACEMENT (`lhu …,%lo(sym)(reg)` /
`sw …,0xDC0(reg)`). Functionally identical; the divergence is purely which register holds the base and
where the constant lives.

**What FAILS / backfires (do not reach for these first):**
- **Struct-array access** (`D_800C28E4[b].field[c]`) and **explicit intermediate pointers**
  (`T *row = base + i*S; row[c]`) both perturb the WHOLE function's register allocation — S210 CEE0 went
  from 4 `r` rows (raw inline pointer arith) to 9 `r` rows (pervasive shift) with either. The LEAST-
  divergent form is raw inline pointer arithmetic (`*(u16*)((u8*)SYM + b*14 + c*2)`), so start there.
- The **permuter does not flip it** (S210: AF80 ~9500 iters base 220, B0B4 ~9500 base 1990 — no crack):
  it is an addressing-mode + allocno-coloring decision, not a source-reachable permutation.
- This is DISTINCT from [#integer-arith-commutative-operand-order] (which fixes operand ORDER, e.g.
  `n+p` vs `p+n`) and from [#struct-array-of-bss-direct-index-vs-base-pointer-var] (a bss RMW base-reg
  choice). Here the base is a full symbol/pointer materialization vs a `%hi`+`%lo`-displacement split.

**Status.** Open near-match class; no reliable source lever found in S210. Escalation is a dedicated
compiler-source dive (mips.c `print_operand_address` / `simple_memory_operand` + reload's address
legitimization) or [#cross-project-matched-corpus-mining]. Related base-register cases where a lever DID
land: [#mem-in-struct-scheduling-lever], [#offset-0-symbol-re-materialization].

### Tracked ranker follow-up: `family-of:<banked-fn>` (S210)

pick_target smallest-first repeatedly surfaced these allocno/addressing walls on a HARD residual tail
(the S208/S209 easy wins mined out), while the ONE S210 bank (`func_8005C510`) came from pattern-matching
an already-banked FAMILY (the C458 grid-counters), not from the smallest raw fn. FOLLOW-UP: add a
`family-of:<banked-fn>` ranker signal — flag an unbanked fn whose asm shares a banked sibling's shape
(same lead helper call, e.g. `func_8005AF50`; same size band; same coddog cluster) so the gate prefers a
known-bankable-pattern sibling over the smallest raw fn. NOT applied live at the S210 review: a
pick_target/pick_target_hazards change must run on the golden-gated tooling branch
(`make test-tools`, byte-identical goldens; see the tooling-refactor-style convention), not a retro
in-place edit. Until then the gate applies it MANUALLY by reading the coddog/call tags for a family match.

### Data-global in a shifted `.NON_MATCHING` carve (S210 `func_8005DE88` blocker)

A separate but adjacent trap: C-referencing a data global that lives in a `.NON_MATCHING` data section
whose alias is placed +0x10 off its name (S210: `D_801323E5`@0x801323d5, `D_800C1FFC`@0x800c1fec)
CORRUPTS the whole region on `make extract` — even already-banked siblings start mismatching
(`func_8005AF74` returning `D_801323A0` flipped from 0x801323a0 to 0x80132390). Adding an absolute to
`symbol_addrs.txt` does NOT win over the shifted section symbol. **DIAGNOSTIC before referencing any
`D_<addr>` from new C:** `grep D_<region> build/mariogolf64.map | grep -v NON_MATCHING` — if the mapped
address != the symbol name, the region is a shifted `.NON_MATCHING` carve; do NOT C-reference it until
the data section is properly carved/placed (see [#defines-data] / [#data-rodata-carve]). Cleanly-placed
globals show `.NON_MATCHING` at the SAME address as the real symbol (safe). This is the data-carve
enabler that blocks `func_8005DE88` (logic fully decoded; carried pending the carve).

## value-select-if-else vs branch-likely (the `p ? field : sentinel` accessor idiom)

**Symptom (S211; `func_80056464` / `func_80056494` / `func_8005642C`).** A tiny accessor calls a
lookup that returns a pointer, then returns a field-or-sentinel: the ROM emits the NON-annulled
value-select layout `bnez v0,.Lval; nop; j .Lend; li v0,SENT (delay); .Lval: lh v0,OFF(v0); .Lend:`
(5 insns). The natural early-return `if (p == NULL) return SENT; return p->field;` AND the ternary
`return p ? p->field : SENT;` BOTH collapse to the SHORTER branch-likely form
`beqzl v0,.Lend; li v0,SENT (annulled delay); lh v0,OFF(v0)` (3 insns) — 2 insns short each.

**Fix.** Write the explicit VALUE-SELECT if-else assigning a result var, testing the null case first
so the sentinel is the inline "then":
```c
s32 result;
if (p == NULL) { result = SENT; } else { result = p->field; }
return result;
```
This emits the target's `bnez/nop/j/li` layout. The distinguisher is that GCC materializes the
sentinel into a named local whose two definitions live in separate basic blocks, defeating the
branch-likely annul it applies to a single conditional-expression value.

**Detection when wrong.** A 2-insn-per-fn length deficit cascades a whole-file symbol shift, so
`cmp build/mariogolf64.z64 baserom.z64` shows THOUSANDS of scattered ±1-byte diffs (every reference to
a now-shifted symbol), NOT a localized per-fn near-miss — `asm-differ diff.py` shows each fn internally
clean-but-shifted. Sibling of [#top-tested-loop-goto-local-hoist] (both are -O2 branch-form matches).

## delay-slot-fill of a null-guard `beqz` (body-first insn safe-on-the-taken-path)

**Symptom (S211; `func_800564F0` `if(cs) cs[0x189]=1` / `func_80055738` `if(cs){p=cs+i*8; …}`).** A
null-guarded block matches byte-for-byte EXCEPT the guard `beqz`'s delay slot: the ROM leaves it `nop`;
the build fills it with the block's first computation (`li v0,1`, `sll v0,s0,3`), so the build is 1
insn SHORT and shifts the rest of the file.

**Root cause (the predictive rule).** GCC 2.7.2 `reorg` steals the block's first instruction into the
guard `beqz`'s (non-annulled) delay slot ONLY when that instruction is SAFE to execute on the TAKEN
(pointer-is-null) path. So:
- Body-first DEREFERENCES the guarded pointer (`lwc1 $f0,0x10(v0)` / `lw v0,0(v0)`) — would fault at
  the near-null address on the taken path, so reorg CANNOT steal it and correctly leaves the `nop`.
  These match for FREE (S211 `func_80056264`, `seek_current_frame_by`, `func_80056520`).
- Body-first is pointer-INDEPENDENT (`li CONST`, `sll index*K`) — safe on both paths, so reorg steals
  it into the delay slot. The ROM's reference build did NOT (a reorg-heuristic/patchlevel divergence),
  leaving `nop`. **Faithful C cannot make a safe op unsafe**, so this is a hard near-match.

**What FAILS.** `if`, `if/else`, early-return, and hoisting the independent op before the guard all
fail to suppress the fill (early-hoist trades the fill for a `move v1,v0` reg-swap, worse). Contrast the
store-a-VARIABLE case (`func_80056238` `cs[0x85]=arg1`): its body-first `sb arg,OFF(v0)` derefs the
pointer, so it correctly emits `bnel`+store-in-delay and matches. Escalation:
[#cross-project-matched-corpus-mining] for a KMC-2.7.2 sibling, or a `reorg.c fill_simple_delay_slots`
patchlevel probe; carry otherwise.

## default-return-var must init AFTER the call (caller-saved sentinel frame lever)

**Symptom (S211; `lookup_animation_by_id`, via systematic-debugging + gcc-2.7.2 source).** A fn returns
a default sentinel (`result = 0`) overwritten only on some path. Seeding `s32 result = 0;` BEFORE the
`get_character_state()` (or any) call makes `result` LIVE ACROSS the call, so GCC pins it to a
callee-saved register (`s1`) → an extra `sw sN`/`lw sN` pair → the frame grows (`0x18` → `0x20`) and a
pervasive downstream regalloc cascade (plus a spurious `beqzl`). The target keeps the sentinel in a
CALLER-saved arg register (`a1`) and sets it in the guard branch's delay slot, POST-call.

**Fix.** Initialize the default AFTER the call so it never crosses it:
```c
u8 *cs = get_character_state(id);
s32 result = 0;   /* after the call → caller-saved a1, frame 0x18 */
```
Diagnostic: a frame-size mismatch plus exactly one extra saved `sN` register at the top of the diff is
this, not a body bug. General to any default-then-conditionally-overwrite return and the whole
`get_character_state` accessor family. (S211 `lookup_animation_by_id`: this dropped the score 2588→1340
and byte-matched the prologue/setup; the residual loop shape is a separate
[#goto-loop-vs-structured-loop-codegen] / [#top-tested-loop-goto-local-hoist] wall, carried.)
