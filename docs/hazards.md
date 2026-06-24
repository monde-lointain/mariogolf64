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

## Upstream-mirror pattern

**Rule:** When a target maps to a known SDK upstream, copy the upstream `.c` verbatim instead of
hand-decompiling. The proof is the full-`make` ROM SHA-1 (verbatim copy + green ROM = match): no
iteration, no byte-`cmp` (that guard is classical-loop-only).

**Trigger:** `pick_target.py`'s `upstream` column is `libultra` / `libkmc` / `libnusys` / `libmus`
(not `none`). Three related flags also land here:
- `single-file-pack:<n>fn[…]`: every member fn comes from ONE upstream `.c`, so the pack is an atomic
  verbatim mirror with no split.
- `upstream-fncount-mismatch:<m>vs<n>` (S104): a single-named-stem pack holds MORE fns than the
  upstream `.c` defines, so a foreign TU is bundled in the subseg. Split it off and mirror only the
  upstream's fns. This is the named-symbol analog of `coddog-fncount-mismatch` (xprintf's
  `func_800B1580` `__osDpDeviceBusy` TU). Advisory.
- `one-tu` (S88): every inner fn boundary is non-16-aligned, so the subseg is ONE `.o`. This confirms
  a single-file-pack even for un-named coddog packs, and marks per-fn decompose as blocked (see also
  #non16align).

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
4. Skip clang-format under `src/libultra/` and `src/libkmc/` (verbatim cross-reference copies; both
   dirs carry a local `.clang-format` with `DisableFormat: true`).
5. `make`, and the ROM SHA-1 must equal the baserom.

Before declaring a clean verbatim mirror, reconcile the upstream's full call + data-ref list
(including one level of library-macro expansion) against the name files; see the recover-extern and
near-verbatim sections. Known-edit sub-cases: file-static drop, defines-data drop, near-verbatim
line-drop, recover-extern placement.

**Sub-cases / variants:**

**Authoritative symbol set via `nm` (S102).** For a mirror that is `#if BUILD_VERSION`-branched or
inline-ambiguous (a `pack` where a static helper may or may not be inlined), read the matching
prebuilt object rather than reasoning from the source alone:
`mips-linux-gnu-nm ~/development/repos/ultralib/build/J/libgultra_rom/src/<dir>/<file>.o`. It gives
the definitive VERSION_J symbol set: which functions are global (`T`) vs absent (inlined), which data
is local/static (`b`/`d`) vs global, and the exact `.text` offsets. S102 motor.c: nm settled that the
0x800AE380 fn is `T __osMotorAccess` (not the ghidra `osMotorStop`; see #wrong-ghidra-name-override),
that `__osMakeMotorData` is inlined (so `pack:2fn`, not 3), and that `__MotorDataBuf` is a local `b`
(so drop-static-to-extern, not a defined global). `libgultra_rom` is the release/ROM profile that
matches MG64's build; `libgultra`/`libgultra_d` are the debug profiles.

**libnusys multi-version triage before concluding "custom" (S123).** The pinned libnusys source is
nusys-2.07 (`coddog-sweep-nusys` builds its ref), but MG64 forked an EARLIER rev with game edits, so a
2.07 mirror can SHA-miss or only structurally match. Before declaring a function MG64-custom, triage
across ALL on-disk nusys versions and pin the rev from the ROM:
- Sources: `~/n64sdk/.../nusys/src/nusys-{1.10,1.20,2.00}/nusys/*.c` (note CRLF + Shift-JIS comments;
  UTF-clean or strip comments when copying) plus the pinned `~/development/repos/n64sdkmod/.../nusys-2.07/`.
- Pin the rev with two ROM-side tells: (1) `strings baserom.z64 | grep -i NuSystem` — ABSENT means the
  `nuVersion[]="NuSystem"NU_VERSION` marker was removed (so NOT a stock 2.07 with its `.data` string;
  `nuScRetraceCounter` is then uninitialized bss, no `.data` carve). (2) per-function feature diffs —
  e.g. the `nuScAddClient` PRENMI-dispatch block is a 1998/12 (2.06/2.07) addition; its presence in
  the asm dates the fork ≥2.06 even when the version string is gone (S123 nusched = ~2.07-minus-nuVersion).
- A function that matches NO version (game-fn callees, added display/swap blocks, a tvtype hang-guard)
  is genuinely MG64-custom -> classical track. A function that matches a SPECIFIC older version is a
  near-verbatim mirror of THAT version (the per-file/per-fn version rule, #near-verbatim-mirror-jal-count-mismatch).
This generalizes the libultra coddog cross-ref (`#coddog-cross-ref`) and the S117 nusys-version-hunt to
the per-FUNCTION grain: in one subseg, stock fns and custom fns coexist (bank-stock-carry-custom).

**`#pragma weak` alias mirror (S66, no special handling).** A libultra C file whose ROM/curated
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

**Perf/debug struct version drift (S125).** A vendored header struct that is referenced ONLY by
carried / `INCLUDE_ASM` fns is **UNVALIDATED** — it was copied from the upstream version verbatim and
never checked against the asm. When the first consumer is finally decompiled, the layout can be a
different library rev: MG64's `nusys.h` `NUDebTaskPerf` carried the 2.07 `markerTime[10]` field
(a 1999/05/30 feature) that MG64's pre-1999 rev lacks, so `auTaskCnt` was at 0x59 / `auTaskTime` at
0x1A0 in the header but the asm needs 0x9 / 0x150 (struct size 0x1F0, asm-confirmed via the
`idx * 0x1F0` array stride and the `lbu …,0x9` count load). Validate perf/debug struct offsets against
the asm BEFORE banking the first perf-using fn; fix the vendored header to the game's rev (drop
markerTime). A header struct is not ground truth until a matching fn proves its offsets.

**Header CONSTANTS reach codegen too — validate each against the asm (S128, generalizes the perf-struct
rule).** Not just struct offsets: any vendored-header `#define` that flows into an instruction
(array sizes, message counts, loop bounds, masks) must be reconciled with the asm before banking. MG64's
nualstl `NU_AU_MESG_MAX` is 2 (the `osCreateMesgQueue(&nuAuMesgQ, …, 2)` count in the asm), not the
stock 2.05 source's 4; the vendored `include/libnualstl/nualstl.h` was edited to 2. Read the literal in
the target instruction (the `li`/`addiu` immediate), do not trust the stock constant — a wrong constant
is a single-immediate SHA-miss the same class as a wrong struct offset.

**Provenance:** S15 unlocked the libnusys band by vendoring one `include/libnusys/nusys.h` +
`-I include/libnusys`; `-I include/libultra/PR` is the precedent for unblocking a band. S51 is the
cautionary case for step 1's "do not consult `libultra_modern`": its split `gu/` layout (hand-asm
`mtxcatf.s` + C `mtxxfmf.c`) mis-suggested `guMtxCatF` was hand-asm at the gate, while ultralib's
`src/gu/mtxcatf.c` (the sole source) holds BOTH fns as clean C, which the ROM matched verbatim. Reach
for ultralib first; a `libultra_modern` `.s` is not evidence the ROM fn is hand-asm.

---

## asm-mirror vendoring

**Rule:** The asm analog of the upstream-mirror pattern, for a libultra function that is genuinely
hand-written assembly (a register/FPU/cache/TLB primitive, the `intrinsic-likely` shims). Instead of
leaving it as an anonymous `asm/<rom>.s` disassembly or a bare `hasm`, vendor the real ultralib `.s`
TU into `src/libultra/<dir>/` and build it. The proof is the full-`make` ROM SHA-1, same as the C
mirror. Pilot: S56 (`getcount`/`getcause`/`getsr`/`setcompare`).

**Trigger:** `pick_target.py` flags `intrinsic-likely:<tu>.s` (a vendorable ultralib TU exists) on an
`asm-flip` candidate. A bare `intrinsic-likely` (no `:<tu>`) is a no-source shim, so plain `hasm`.

**Sub-trigger, `intrinsic-likely:cp0-asm(identify-TU)` (S70 #1).** This is the same asm-mirror class
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
2. **Vendor any missing asm macros** into the project headers (verbatim from ultralib). The MONEGI
   `include/sys/asm.h` lacked `MFC0`/`MTC0` (S56 added them). The TU's `#include "PR/R4300.h"`
   resolves via `-I include/libultra`; `sys/asm.h`/`sys/regdef.h` via `-I include`.
   `pick_target.py` pre-flags this as `intrinsic-likely:<tu>.s(needs-define:<MACROS>)` when the
   vendorable `.s` references an `UPPER_CASE` macro that none of the in-tree asm `-I` headers
   (`include`, `include/libultra`, `include/libultra/PR`, `include/libultra/compiler/gcc`) define, so
   the enabler is priced at the gate, not at a failing vendor-compile (S57 retro #1). The whole
   current cache/TLB/gu backlog is self-contained (all `DCACHE_*`/`C_*`/`TLB*`/`C0_*` ship in
   `include/libultra/PR/R4300.h`, `RDB_*` in `PR/rdb.h`), so the pre-check reports nothing for it.
3. **Assemble with ultralib's exact toolchain and flags: KMC/N64 gcc, gcc.mk profile, NOT modern
   `mips-linux-gnu as`.** This is the load-bearing step: KMC `as` pads each function's `.text` up to
   its 16-byte ROM slot, so the verbatim 0xC TU lands as the 0x10 the ROM expects. Modern `as` emits
   the bare 0xC and every following subseg shifts, breaking the SHA-1 (the entire S56 failure mode
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
- **Vendored `.s` carrying a non-`.text` section (`.rodata`/`.data`/`.bss`) → vendor `.text` only
  (S84).** splat auto-links a `hasm` `.o`'s `.data`/`.rodata`/`.bss` at the **END** of each output
  section, NOT in address order. See the generated `mariogolf64.ld`: the
  `build/src/libultra/os/invaldcache.o(.rodata)` / `writebackdcache.o(.rodata)` lines sit at the
  `.rodata` section tail, after the address-ordered C/data-file
  siblings. For a TU whose data section is empty this is a harmless 0-byte line; for a TU with a real
  data section (S84 `setintmask.s`'s `.rdata __osRcpImTable`, a 0x80-byte / 64-`.half` LUT) the bytes
  would be emitted a **second** time at the wrong offset → every following rodata byte shifts → SHA-1
  break. **Fix:** copy the `.s` but **strip the data block** (the `.rdata`/`.data` directive through
  EOF), vendoring only `.text`; keep that data as the existing splat-extracted generic blob
  (`asm/data/<rom>.rodata.s`), renamed to the upstream symbol via a `symbol_addrs.txt` add
  (`__osRcpImTable = 0x800D2200; // size:0x80`). splat then renames `D_<vram>` → the symbol in BOTH
  the generic blob (which now *defines* it) and every referencer, **including cross-TU ones** (S84:
  the exception dispatcher `asm/8AF90.s` also loads `__osRcpImTable`), and the vendored `.text`'s
  `%hi/%lo(<sym>)` resolves to it. It is a `D_<vram>` rename across asm, so it needs a clean rebuild
  (`make clean && make extract && make`; see `#defines-data`). The vendored `.s` keeps its
  `.globl <sym>` line (now a harmless external declaration). Afterward confirm
  `build/src/libultra/<dir>/<stem>.o` is `.text`-only
  (`mips-linux-gnu-objdump -h build/src/libultra/<dir>/<stem>.o`), so the auto-link line carries 0 bytes.
  `pick_target.py` pre-flags this as `intrinsic-likely:<tu>.s(has-rodata:<sym>)` (S84 #3) so the
  strip+rename enabler is priced at the gate. (Keeping the table in the shared blob is arguably *more*
  correct than carving it into one TU when it is referenced cross-TU, as `__osRcpImTable` is.)
  - **The has-rodata pre-flag is gated to the ACTIVE build (S91 #1).** `vendorable_tu_data_symbols`
    strips `#ifndef _FINALROM` + inactive `BUILD_VERSION` branches before scanning the data section, so
    an EXPORT only the debug/other-version build emits is NOT listed (S91: exceptasm.s's
    `__osCauseTable_pt` lives in `#ifndef _FINALROM` → excluded under the `-D_FINALROM` asm profile; the
    real active rodata is `__osIntOffTable`/`__osIntTable`/`__osHwIntTable`/`__osPiIntTable`). Price the
    strip set the vendored `.o` actually carries, not the all-branches union.
- **A SYMBOLIC-pointer table in the data section → the LABEL-EXPORT procedure (S107; was mis-framed
  a spike at S91).** The S84 strip-and-rename works for a *numeric* LUT (`.word 0,0` / `.half …`): the
  bytes are self-contained, so keeping the extracted blob + renaming `D_<vram>` resolves cleanly. A
  table of `.word <label>` entries needs ONE more step. Such a table is a switch jump table
  (`__osIntTable: .word redispatch, sw1, …`, whose active `__osException` `jr`s through it) or any
  function-pointer table.
  splat extracts such a table to a SEPARATE rodata blob (`asm/data/<rom>.rodata.s`) with the entries
  emitted SYMBOLICALLY (`.word .L800B00A0, …`) as references to `.text`-internal labels (these are
  global: `jlabel` = `.global`+label, `include/macro.inc`). Two paths FAIL:
  (a) vendoring `.text`-only and doing nothing makes `asm/<rom>.s` go vestigial, so the `.L…` labels
  the blob references are no longer DEFINED → undefined at link;
  (b) carve-placing the table in the vendored `.o` fails because a `hasm` `.o`'s `.rodata` auto-links
  at the section END → wrong addr → SHA break. **The fix is neither: RE-EXPORT the labels** (S91's
  listed-but-untried option, proven S107 exceptasm). The blob lives
  in its OWN data subseg (`asm/data/<rom>.rodata.s`), which is **already address-placed and survives
  the `.text` flip**, and after the flip it KEEPS emitting `.word .L<addr>` (symbolic, not literal).
  So make the vendored `.text` DEFINE those labels:
  1. **Phase 1: rename-isolation (do FIRST, subseg still `asm`).** `symbol_addrs` add-only: rename the
     jtbl head (`jtbl_<addr>` → its upstream name, e.g. `__osIntTable`) AND every byte/word/data table
     the `.text` references by upstream name (`D_<vram>` → `__osIntOffTable`/`__osHwIntTable`/…), plus
     any externalized scratch (`__osThreadSave`). `make extract && make` → must stay GREEN. This proves
     the renames don't break the still-`asm` build and that the jtbl rename PRESERVES the symbolic
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
  table's head symbol), now a ROUTINE label-export asm-mirror, NOT a spike. (S107 exceptasm is the
  worked example: 8-fn OS exception/dispatch TU, 9 jtbl targets, banked first-try after the mechanism.)
- **Combined-subseg sub-pattern (≥2 distinct asm TUs in one subseg).** When one `asm` subseg holds
  ≥2 asm-ONLY functions (no C mirror) from *different* ultralib `.s` files, `pick_target.py` flags
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
  the flag does NOT fire); (b) a *partial-TU* pack whose asm members share ONE `.s` (e.g.
  `__osDisableInt`/`__osRestoreInt` both in `setintmask.s`) is a single distinct TU → also does NOT
  fire, and needs a harder partial-TU carve (still a spike).

**KMC-as sub-lane (libkmc soft-float / 64-bit math TUs).** libkmc's hand-asm TUs (`mmuldi3.s`
`__muldi3`; `mcvtld.s` `__fixdfdi`/`__fixunsdfdi`+`__floatdidf`) are NOT ultralib `LEAF`/`XLEAF` TUs
and do NOT assemble under `LIBULTRA_ASFLAGS`. They use **KMC register conventions** (`move dst,zero`
→ `addu` encoding) and must be assembled with the **KMC assembler** `$(KMC_AS)`, the same toolchain
the in-tree disassembly-reassembly path uses. `mk/libkmc.mk` carries these under a **path-based pattern
rule** `build/src/libkmc/%.o: src/libkmc/%.s` (separate from the ultralib `src/libultra/%.o` rule,
which hard-codes the `LIBULTRA_ASFLAGS` profile): `$(KMC_AS) -EB -mips2 -I src/libkmc -o $@.tmp $<;
cp $@.tmp $@; rm $@.tmp` (no objcopy; the 16-byte-slot pad happens inside KMC `as`).
`pick_target.py` flags it `intrinsic-likely:<tu>.s(kmc-as)` so the
gate reaches for the KMC-as recipe, not the ultralib one: the libkmc analog of the ultralib
`intrinsic-likely:<tu>.s` (S109). It fires when an asm-only primary the pure-shim + privileged tests
both MISS (a branchy cvt routine, no CP0/FPU-ctrl op, no `handwritten` tag) matches a libkmc `.s`
`.globl` AND has no C upstream; the `not in upstream_index` guard excludes the C-mirrorable libkmc
files (`memset`/`strcmp`/`rand` resolve via the C index), leaving only the asm-ONLY math TUs. Three
KMC-as specifics:
- **`.include "mips_as.h"`.** A libkmc `.s` may `.include` its tiny KMC header (`mcvtld.s` →
  `mips_as.h`, one line `.equ FPU,1`). Vendor that header alongside the `.s` (`src/libkmc/mips_as.h`)
  and add `-I src/libkmc` to the KMC-as rule. (`mmuldi3.s` is self-contained and needs neither.)
- **`li $X,0xffffffff` → `addiu $X,$0,-1` (the documented encoding edit).** KMC `as` expands
  `li reg,0xffffffff` to a 2-insn `lui+ori`; the ROM uses the 1-insn `addiu reg,$0,-1`
  (`2403ffff`/`2407ffff`). Each extra word shifts the rest of the TU down → SHA-1 break (the
  `.align`/trailing-nop pad absorbs the size so the FUNCTION boundaries hold, but the bytes diverge).
  Rewrite each such `li` to the explicit `addiu`, the **only** allowed edit to the vendored libkmc
  `.s`, touching no real `.text` semantics. Same divergence already applied to the vendored
  `mmuldi3.s` (six `li $9,0xffffffff`); S109 applied two in `mcvtld.s`. Diagnose by `cmp`-ing the
  assembled `.o`'s `.text` against the baserom bytes (Python byte-slice; `dd` is hook-blocked): a
  one-word downstream shift starting at a `lui …ffff` / `ori …ffff` pair is this exact case.
- **Multi-fn TU spanning >1 splat subseg → merge to ONE `hasm`.** A KMC-as TU defining several
  functions (`mcvtld.s` = `__fixunsdfdi`@0x8F020 + `__floatdidf`@0x8F140) extracts as several
  ADJACENT `asm` subsegs (one per fn). One source `.s` → one `.o`, so merge them into a single
  `[<first-rom>, hasm, libkmc/<stem>]` (delete the later subseg line(s)); the merged
  subseg's extent (to the next subseg) must equal the TU's assembled size. The asm analog of the C
  `single-file-pack` (one upstream file → one atomic mirror, no inter-fn split). 0 `symbol_addrs`
  adds when both fns are already named (S109).

**Provenance:** S56 (4 reg shims; KMC-as padding the load-bearing discovery, per PO directive to use
ultralib's exact flags). S57 (4 cache/TLB primitives `osWritebackDCacheAll`/`osWritebackDCache`/
`osUnmapTLBAll`/`__osProbeTLB`, all first-try clean; added the `needs-define` pre-check + this bisect
protocol). S58 (3 cross-dir TUs: `sqrtf`/`osMapTLBRdb`/`bcopy`, clearing the `WEAK`-alias + FPU-op
cases). S62 (`osInvalDCache`+`osInvalICache`: the combined-subseg split sub-pattern + the
`combined-subseg` pre-flag). S63 (the `[0x8CA50]` reg-shim "set" family `setfpccsr`/`setsr`/
`setwatchlo` + the C-mirror `__osSpDeviceBusy`: first mixed asm-mirror + C-mirror subseg-clear;
extended the `needs-define` pre-check to the `combined-subseg` path after `CFC1`/`CTC1` surfaced at a
failing vendor-compile). S84 (`osSetIntMask` from `[0x7E360]` mixed pack: first vendored `.s` with a
`.rodata` LUT; added the `has-rodata` pre-flag + the vendor-`.text`-only / strip+rename-the-blob
sub-case above; `pimgr` C member carried, `epirawdma` C member banked same sprint). S109 (`mcvtld.s` `__fixunsdfdi`+`__floatdidf`: first
documented **KMC-as sub-lane** TU: the libkmc `$(KMC_AS)` explicit-rule path + `.include
"mips_as.h"` via `-I src/libkmc` + the `li 0xffffffff`→`addiu` encoding edit + the multi-subseg
merge-to-one-`hasm`; the `intrinsic-likely:<tu>.s(kmc-as)` flag).

---

## recover-extern (refs-unplaced)

**Rule:** A clean verbatim leaf often references a global the upstream declares `extern` (defined in
not-yet-decompiled lib code) that is absent from both name files. A *referenced* extern is safe to
place; recover its vram deterministically and add it to `symbol_addrs.txt` before the flip. This is
the lowest-risk mirror and batches well (S19 banked the `nuGfx*FuncSet` trio at one-file risk).

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

**Contiguous `.bss`-block fast-path (S90):** a file-static drop-to-extern mirror often references a
whole RUN of adjacent `.bss` statics declared together in the upstream `.c` (e.g. pimgr.c's
`piThread` / `piThreadStack` / `piEventQueue` / `piEventBuf`). Recover the entire block from a SINGLE
`disassemble_function` of the one fn that touches them: each base is a `lui/addiu` HI/LO16 pair (or a
`STACK_START` top-of-stack value = base + sizeof), and each **size is the gap to the next symbol in
the run**, cross-checked against the C type (`OSThread`=0x1B0, `OSMesgQueue`=0x18, `OSMesg[1]`=0x4,
`STACK(_,N)`=`ALIGN8(N)`). The run is laid out in source-declaration order at consecutive `main_bss`
vrams (pimgr's block sat immediately below the prior io file's `piAccessBuf`), so one disassembly plus
the gap arithmetic yields every `symbol_addrs` size:-extern at once, with no per-symbol re-disassembly.
**Stack-top-equals-named-adjacent-symbol tell (S115).** A thread-stack `static T Stack[N]` passes its
*top* (`Stack + N`, stacks grow down) as the `osCreateThread` sp arg, so its asm `%hi/%lo` resolves to
`Stack`'s **end**, not its base. When that end lands exactly on an ALREADY-PLACED symbol from a
DIFFERENT TU, the asm shows `%lo(<NamedSymbol>)` (a real name, not a `D_<addr>` auto-label): that named
symbol's addr IS the stack's exact end, so `base = <NamedSymbol> addr - STACK_SIZE` with zero gap
arithmetic. (S115 `nugfxthread.c` `GfxStack`: sp arg `%lo(PiMesgQ)`=0x800F74A0, `PiMesgQ` placed
S99/nupiinit.c → `GfxStack` base 0x800F54A0, size 0x2000 = `NU_GFX_STACK_SIZE`.) A `D_<addr>` label
there means the next symbol is itself still un-named — fall back to `base = top - sizeof`.

**Unindexed-upstream mirror → no auto refs-unplaced (S112).** A mirror candidate whose `upstream`
column is `none` (a coddog-match, a de-ranked carry-over, or any source not in the upstream index)
gets **no** refs-unplaced scan at all; the hazard is computed only for a candidate with an indexed
upstream `.c`. So an inline `extern <type> <name>[];` data dep declared in the upstream BODY (not a
header) won't be auto-flagged; recover it manually at the gate from the asm `%hi/%lo` and place it
add-only before the flip. NB the *detection* is not the gap: `EXTERN_DATA_DECL_RE` +
`declared_extern_data` already match the inline array / macro-type form (`extern XLONG _atbl[];` →
`_atbl`) once the upstream is indexed. S112 `atan.c`/`sin.c` (libkmc math, upstream `none`, de-ranked
carry-overs) needed `_atbl`@0x800C9690 placed by hand; indexing libkmc math was rejected as low-value
(carry-overs) plus reclassification-risk.

**Provenance:** S12 (inline-vram idea), S19 (3/3 trio), S20 (indexed-array base correction), S22
(flat-guess trap), S90 (contiguous `.bss`-block sized by inter-symbol gaps), S112 (unindexed-mirror
no-scan note).

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

**Renamed-vs-substituted callee, body-compare before editing (S61).** When the upstream calls
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

**Provenance:** S23 (`osEPiStartDma` → `osPiGetCmdQueue`@0x800B06F0); S61 (`guRotateF` substituted
`vec3f_normalize`).

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
  them into one shared tail itself, which IS the `jal N-1`. Copy verbatim and let the compiler do
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
- **Jal-less dropped block (jal count can MATCH).** A dropped block need not contain a call, so the
  jal count can agree while the build still diverges and `pick_target.py` flags nothing. A verbatim
  mirror that links fine but misses the ROM SHA-1, with no flagged hazard, often means this ROM omits
  a jal-less upstream guard/early-return (or replaces a multi-call block with a one-liner). Don't
  iterate C blind: `.o`-diff against the baserom asm first (same first-SHA-miss remedy as the cast
  divergence below). The dropped lines are deterministic from the asm structure. (S45 `osGbpakReadWrite`
  drops `if (size == 0) return 0;`, a jal-less early-return that folds into the later `blez` size<=0
  path; `osGbpakReadId` replaces the upstream `if(bcmp){ write-temp; reread; recheck }` retry block
  (5 calls + a `temp[32]` local) with `if (bcmp(...)) return 4;`, where the jal count *did* diverge.)
- **nusys per-file version wrapper (libnusys int-mask drop).** A libnusys mirror can mismatch
  because MG64's build pins an OLDER nusys revision *per file* than the in-tree default (the
  n64sdkmod nusys-2.07 tree); the version is NOT uniform (S117 established the divergence). The
  concrete recurring delta in the `cont`/RMB family: nusys-2.05 wrapped the function body in an
  `osSetIntMask(OS_IM_NONE)` ... `osSetIntMask(mask)` pair (a `OSIntMask mask;` decl + the two
  calls), kept through 2.07; the pre-2.05 (2.00/1.x) source omits it. **Tell:** MG64's asm is a
  **leaf** (no `addiu $sp,-N` / `sw $ra`, 0 jals) but the 2.05+ upstream wraps the body in two
  `osSetIntMask` jals → a `jal-count-mismatch:2vs0`-style flag whose surplus is exactly the wrapper.
  **Confirmed clean drop:** stays a mirror — copy the 2.07-sdk file verbatim, drop the 3-line
  int-mask wrapper (keep the English comments), which IS the 2.00 leaf code; ROM SHA-1 is the proof.
  A sibling whose code is version-identical (comments-only diff) copies straight from 2.07-sdk.
  Diagnose the version with the per-revision source set under `~/development/repos/nusys/src/<ver>/`
  (`grep -c osSetIntMask <ver>/nusys/<file>.c`). `pick_target.py` now annotates this case
  `jal-count-mismatch:<c>vs<asm>(version-artifact?)` for libnusys so smallest-first is not deterred
  (S118 #2). (S118 `nuContRmbModeSet` — leaf 0-jal = 2.00 variant, int-mask dropped; sibling
  `nuContRmbForceStop` was version-identical and copied straight from 2.07-sdk.)
- **nusys per-file version BLOCK REORDER (no count signal; "no coddog" is the tell).** A per-file
  version divergence need not change the call/instruction COUNT — it can be pure block ORDER. MG64's
  per-file nusys revision can order two top-level blocks differently from every archived rev, and no
  available source reproduces it. **Nothing count-based fires:** the jal count agrees with the asm and
  the instruction count is identical; the only standing tell is the ABSENCE of a `coddog-mirror:`
  match (a reorder breaks coddog's structural fingerprint, so "no coddog" on a jal-flagged libnusys
  target is a divergence signal, not noise — `pick_target.py` now prices a jal-mismatch-without-coddog
  target one above the mirror floor, S119 #1). **Symptom:** the verbatim cp builds + links clean but
  the ROM SHA-1 misses; an in-tree-`.o` vs baserom-asm objdump diff shows the SAME instruction count,
  same ops, REORDERED (with cascading regalloc differences). **Procedure:** read the asm's jal/block
  order (which call/block executes first), reconstruct the true source order — a C compiler does NOT
  reorder across an early-return-gated side-effecting call, so the asm order IS the source order —
  hand-swap the blocks in the closest English rev (2.07), and verify insn-identical (`objdump -d`
  diff of the in-tree `.o` vs `build/asm/<rom>.o`) plus full-make ROM SHA-1. (S119 `nuContGBPakFread`:
  the RAM-enable block — `bzero` / `data[31]=…` / `nuContGBPakWrite` — runs BEFORE
  `nuContGBPakCheckConnector` (`ram=0` in the range-check `beqz` delay slot, `jal CheckConnector` at
  the skip-label), but every archived 1.20/2.00/2.05/2.06/2.07 is CheckConnector-first; swapping the
  two source blocks gave the byte match. Its `jal-count-mismatch:5vs9` was an unrelated MACRO
  artifact: nusys.h `nuContGBPakRead`/`Write` expand to `nuContGBPakReadWrite`, so 9 asm jals == 9
  expanded call sites — confirm by expanding the macros before counting, do not read it as 4 dropped
  calls.) Generalizes the S117/S118 "nusys version is per-file" finding from wrapper-PRESENCE to
  block-ORDER divergence.

**Sub-cases / variants:**

**Sibling-replay (S120).** Once one fn in a family is confirmed a block-reorder mirror, its siblings
replay the SAME swap: `nuContGBPakFwrite` (S120) is the direct sibling of S119's `nuContGBPakFread`
(its asm runs the RAM-enable block before `nuContGBPakCheckConnector` identically, with the same
benign `nuContGBPakRead`/`Write`→`ReadWrite` macro `jal-count-mismatch:5vs10`). Applying the swap
UP-FRONT from the asm + the sibling precedent banked it FIRST-BUILD, 0 iteration (vs S119, which
discovered the reorder via a re-attempt). `pick_target.py` now surfaces
`block-reorder-sibling:<sibling.c>` on a libnusys candidate carrying the tell (unexplained
jal-mismatch + no `coddog-mirror`) whose upstream-file basename is in a known block-reorder family
(`BLOCK_REORDER_FAMILIES`, seeded `nucontgbpak*`), so the gate plans the swap instead of re-deriving
it. The flag is advisory: the seed keeps its +1 near-verbatim risk (the hand-edit is still real at
plan time), but a sibling-known swap that banks first-try scores the realized −1 verbatim-first-try
tier (VELOCITY S120 #3).

**Provenance:** S18, S30, S35, S45, S47, S118, S119, S120.

---

## Mirror cast divergence (sign- vs zero-extend)

**Rule:** A verbatim mirror can match every instruction except the width-extension of one 64-bit
context/field assignment, when this ROM's source casts differently from the upstream `.c`. KMC GCC
emits `sra rd,rs,0x1f` for a sign-extending `(s64)(s32)x` but `move rd,zero` (writing the high word)
for a zero-extending `(u64)(u32)x`: same low word, different high word. Invisible to every gate
check (jal count, ref/header grep) and the `INCLUDE_ASM` gate build; surfaces only as a full-make
ROM SHA-1 miss in the execution middle (same late-surfacing class as jal-count-mismatch S18 /
wrong-lib-header S40).

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

**Sub-cases / variants:**

**S103 float-literal single-vs-double (classical FP reconstruction).** When a classical/mirror
reconstruction adds a float comparison or clamp against a literal (`if (x < -32768.0)`), a **bare
floating literal is `double`**: KMC GCC promotes the `f32` operand (`cvt.d.s`), compares as double
(`c.lt.d`), and **loads the double constant from `.rodata`** (`lui at,0; ldc1`). The ROM, built from
a `float`-literal source, compares **single** (`c.lt.s`) with the constant **inline** (`lui/mtc1`, no
rodata). Symptom: insn count matches but the compares are `.d`, plus a spurious `.rodata` double the
mirror would need to carve. **Fix:** suffix every FP literal in a compare/clamp/assignment with `f`
(`-32768.0f`, `32766.0f`) so it stays single-precision and inline. Provenance: S103 (`guMtxF2L`'s
Monegi clamp, `if (mf[i][j*2] < -32768.0f) … > 32766.0f`, consts `0xc7000000`/`0x46fffc00`; bare
doubles first compiled `c.lt.d`+`cvt.d.s`+a rodata pair before the `f` suffix fixed it in one
iteration). See #game-region-mirror-o2-profile for the same sprint's profile gotcha.

---

## file-static (BSS-layout-conflict)

**Rule:** A file-scope `static <type> <name>;` (BSS global) in the upstream blocks the verbatim
mirror: KMC GCC emits a section-relative `.bss` reloc, the linker can't place the local BSS at the
splat-side `next` slot, the 0x10-aligned chunk inserts ahead and shifts every downstream BSS symbol,
breaking every code reloc into that range (10,853 ROM bytes diff for `rand`). Function-local
`static` is fine; only file-scope is the problem.

**Trigger:** `pick_target.py` flag `file-static`. **Static *functions* do not count:** a
file-scope `static <type> <name>(...);` (proto/def) shares the mirror's TU and is no BSS hazard, so
`has_file_scope_static` skips it (S54: sprintf's `static void* proutSprintf(...);` was a false flag
that mis-routed a clean 2-fn mirror toward the classical loop). The detector strips
`__attribute__((...))` before the declarator check so an attributed static *array*
(`static OSMesg buf[N] __attribute__((aligned(8)));`) still flags. **S99: the scan runs on
comment-STRIPPED text.** `FILE_STATIC_RE` anchors on `;\s*$`, so a trailing `/* ... */` after the
`;` (nupiinit.c's `static OSMesgQueue PiMesgQ __attribute__((aligned(8)));  /* PI message queue */`)
silently defeated the match pre-fix; the same comment-strip un-suppressed `defines-data` (a
`/* ... ( ... */` banner like `Copyright (C) 1997` falsely tripped the K&R-param guard, hiding every
subsequent depth-0 global across the whole nusys band). **Also S99: `file-static` now unions over a
c-combined pack's members** (like `defines-data` since S97): a SECONDARY member's file-scope static
(`nuContGBPakFwrite`'s `nusimgr` member) is a pack-level drop-static enabler the primary-only scan
missed. Known gap: an *initialized*
static (`static T x[] = {...};`) carries `=` and is invisible to the regex, so it is not flagged
(caught at the gate / link if a sub-fn of a decompose pack is later mirrored).

**Procedure:** Fall to the classical loop with the `static` **dropped** in the C body. The linker
then resolves to the splat-side global at the correct vram.

**Sub-cases / variants:**

**Uninitialized file-static is a drop-to-extern MIRROR, not a carve/classical spike (S87).** Be
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
places at the wrong address). Escalate to a `.data` **carve** (the `rand`/S61 playbook) ONLY when a
static carries a **nonzero** initializer (real `.data` bytes that DO shift the image); `={0}` is still
`.bss`. (S87 `io/vimgr.c`: 6 file-statics + 2 globals + 1 func-local static, all dropped to externs,
0 carve, first-build SHA match; the carry-over's "heavy `.bss` carve" framing was the false-flag this
retires.)

**hasm referrers need a byte-neutral symbol sync (S117).** A BSS drop-to-extern (or recover-extern)
names a `D_<vram>` that `make extract` re-labels everywhere EXCEPT `hasm` files (the entry stub,
vendored asm), which are never re-extracted. If a `hasm` referenced the old `D_<vram>`, naming the
symbol orphans that reference (`undefined reference to D_<vram>` at link). S117: naming
`nuContNum`=0x8010C2D0 broke `src/entry/entry.s`, which loads that address as the `_start` initial
`$sp` (the boot stack top IS `&nuContNum`, the highest BSS global, so the boot stack grows down through
the lower BSS and abandons it before `nucontmgr` runs). Fix: a byte-neutral `D_<vram>`->`<name>` edit in
the `hasm` (same address => identical `%hi/%lo`, ROM SHA-1 unchanged). Anticipate it at the gate: grep
`src/**/*.s` for the `D_<vram>` of each to-be-named drop-def/recover-extern symbol BEFORE the build, so
the sync is planned, not a mid-build link error. (A [`caller-evict`](#caller-evict) sub-case for data
symbols.)

**The `drop-static-mirror:<n>bss` re-frame tag (S87).** When a `coddog-mirror:<file>@≥99` (non-audio)
is on the row, a `file-static` is present, and there is **no** carve signal (`rodata-literal` /
`data-static` / `rodata-jtbl`), `pick_target.py` appends `drop-static-mirror:<n>bss`: the leading
verdict that the co-listed `file-static` + `defines-data` + `refs-unplaced` flags are **one
drop-to-extern enabler** (the pure-`.bss` case above), not the scary 4-flag carve cluster they read as.
`<n>` counts the detector-visible defined `.bss` symbols (file-scope static lines + `defines-data`
globals); a func-local static is a known under-count, recovered at the gate with the rest. **A second
under-count class (S115): a file-scope UNINITIALIZED non-static global that the upstream header already
declares `extern`** (S115 `nugfxthread.c`'s `nuGfxMesgQ`) is a `.bss` drop-def but is NOT in the
`<n>bss` tally — it carries no `=` (so `defines-data`'s K&R-guarded init scan skips it) yet is not a
file-scope `static` either, so it falls between both detectors and surfaces only under `refs-unplaced`.
The gate's per-symbol `refs-unplaced` recovery still places it, so the tally undercount is graceful
(use the union of `drop-static-mirror` + `defines-data` + `refs-unplaced` as the recover set, not the
`<n>` alone). The cluster
flags stay (the gate's per-symbol recovery + `seed_points` read them); the tag tells the gate to price
a **seed-only N-symbol mirror**. A nonzero-initialized global that slips the gate degrades gracefully
to the carve playbook on the SHA miss, never a silent wrong bank.

**Batch-adding a pack's recovered statics transiently reds the build (S99; data-symbol caller-evict).**
When drop-static-mirroring a multi-member pack one member at a time, add each member's recovered `.bss`
statics to `symbol_addrs.txt` **with (or just before) writing that member's body**, not all up front.
A static added while its consuming member is still an `INCLUDE_ASM` stub **evicts the asm stub's
sub-field auto-labels** (`SramHandle = 0x800F7580` killed the still-asm `nupiinitsram` stub's
`D_800F758C`/`D_800F7584`/… → undefined-reference link errors), exactly like the `#caller-evict` /
`make-sync-names` eviction but for a DATA symbol. It is benign and resolves the moment the consuming
body lands (the C references `SramHandle` directly, no `D_` labels), but a red build between the symbol
add and the last body is expected, not a regression. (S99 `nuPiInitSram`: added all 3 pack statics at
once, red until both pi bodies landed, then SHA-clean.)

**Provenance:** `rand` (file-scope-static pre-flight); `sprintf` (static-function false-flag, S54);
`io/vimgr.c` (S87: uninitialized file-static = pure-`.bss` drop-to-extern mirror, the
`drop-static-mirror` tag); `nupiinit`/`nupiinitsram` (S99: 3-file c-combined drop-static pack;
comment-strip + member-union detector fix; batch-add transient-red note).

---

## defines-data

**Rule:** A leaf that *defines* a placed global (the `.data`/`.bss` analogue of file-static) hits
the same BSS-layout-conflict. Route to the classical loop with the definition dropped. (Distinct
from `refs-unplaced`, where a merely *referenced* extern is safe to place.)

**Trigger:** `pick_target.py` flag `defines-data:<g>,...`. Note `__osDequeueThread` is a
`defines-data` false-clean even inside the warm thread band; it does not fast-path.

**Sub-cases / variants:**

**Dual-section carve (S96).** A verbatim coddog mirror can need BOTH a `.data` carve (here) AND a
`.rodata` carve ([`.rodata sibling-yaml pattern`](#rodata-sibling-yaml-pattern)) in the SAME
increment when the upstream file has file-scope `static` *initialized* arrays **and** a `switch` /
pooled FP constants. drvrnew.c (the al synth driver) had both: six `static s32 *_PARAMS[]` arrays
+ a `switch(fxType)` jtbl + SCALE/CONVERT/2³² f64 consts. Size each carve from the `%hi(D_<vram>)`
address band: a `0x800Cxxxx`-range ref is `.data` (carved out of `main_data` with a 3-way
split: `[start,data]` / `[carve,.data,<file>]` / `[end,data]`), a `0x800D2xxx`/`jtbl_` ref is
`.rodata` (its own subseg → attribute-change carve, or split). Both are execution-time (NOT gate)
enablers, landed with the real body; confirm the C-object section sizes match the carve extents
(drvrnew.o: `.data` 0x190 = 100 `s32`, `.rodata` 0x40). The `static` arrays do NOT trip
`file-static` (that's the BSS/uninitialized hazard); initialized statics are a `.data` carve, not a
classical reroute.

**`.data` init-static-array pre-flag (S104).** A file-scope NON-const initialized `static <type>
<name>[…] = <init>;` array (`.data` the verbatim mirror re-emits) is now pre-flagged
`data-carve:<names>` by `pick_target.py`, the `.data` analogue of the `static const` rodata-widening
signal. It is the recurring S92/S101 un-flagged class no other detector caught (`FILE_STATIC_RE`
excludes `=`-initialized lines, `defines_data_globals` skips `static`, `defines_local_static_data` is
depth≥1): xprintf's `spaces`/`zeroes`, env's `eqpower[128]`, _Litob's `ldigs`/`udigs`. `static` bars
cross-file linkage, so the array is file-PRIVATE and the source-scan sidesteps the S92 own-vs-extern
blocker (the reason the asm `addiu %lo` scan was reverted). Fired ONLY on the single-file (non
`c-combined`) subset (S101 safe slice; the multi-file per-member attribution stays a BACKLOG
follow-up). Advisory/display-only: recover the vram + exact extent from the asm at execution, then
carve as above (confirm against the `.o` `.data` section size, per the rodata extent-oracle note).

**Shared global ⇒ DROP, never carve (S97).** The carve-vs-drop choice is not free when the defined
global is *shared*, referenced by OTHER still-asm subsegs via the splat `D_<vram>` name. Then the
**DROP-to-extern is mandatory** and a carve cannot work: a carve makes the mirror `.o` define the C
*name* (e.g. `alGlobals`) while the un-flipped siblings still reference `D_<vram>`, so they go
unresolved; bridging them with a `symbol_addrs` entry then creates an absolute-symbol-vs-`.data`
double-definition. DROP is clean: one splat-side def (the `make extract` rename names the existing
`.data` dlabel), and every referrer (the mirror's `extern` + all sibling asm) resolves to it. Carve
ONLY when THIS file is the **sole referrer** (a file-private initialized array/const with content the
compiler must emit, like drvrnew.c's `static s32 *_PARAMS[]`). Tell them apart by grepping the `D_<vram>`
referrers across `asm/` + `src/`: >1 distinct subseg ⇒ shared ⇒ DROP. S97 `sl.c` `alGlobals`
(`D_800C8180`, refd by env @804D0 + fx @815C0 + sl @82160) dropped to the `libaudio.h` extern +
`alGlobals=0x800C8180 // size:0x4`; the 16 B `.data` (4 B ptr + 12 B section-align pad) stayed
splat-side. `pick_target.py` now unions `defines-data` over a `c-combined` pack's member upstreams
(S97), so a secondary member's defined global is priced at the gate, not at execution.

**Shared at a FIELD, and undroppable ⇒ carve + canonical-addend (S116).** Two refinements to the
S97 share-check. (1) Sweep the WHOLE object range `base..base+size`, not just the base `D_<vram>`: a
sibling that reads a *field* references `D_<base+N>` (a different label), so a base-only grep misses
it and wrongly reads "sole-referrer." (2) "Shared ⇒ DROP" needs an **"AND droppable"** qualifier — a
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

**Carve extent = the `.o` `.data` SECTION size (16-aligned), not the symbol-content sum (S117).** The
compiler pads `.data` up to the section's 16-byte alignment, so the carve runs to the next 16 boundary,
PAST the last placed symbol. `nucontmgr.c`'s `.data` content is 0x24 (`nuContReadFunc` 0x4 + `funcList`
0x14 + `nuContCallBack` 0xC) but the `.o` `.data` SECTION is **0x30** (0xC of END padding); carving 0x24
left the next subseg 0xC inside the section, so the link placed the 0x30 section over it and shifted the
data segment. Always size the carve from `mips-linux-gnu-objdump -h build/.../<file>.o` `.data`, never the
sum of `symbol_addrs` sizes; the ROM zero-padding at `carve_end..section_end` is the mirror's, owned by the
carve (it matches the compiler's `.data` tail-padding bytes).

**Procedure:** Classical loop, drop the definition; the linker resolves to the splat-side global.

**Verbatim-body fast path (S42).** When the *only* edit from upstream is dropping the file-scope
definitions (the function body is otherwise verbatim), this is a known-edit **mirror**, not a true
classical target: prove it by full-make ROM SHA-1 like any mirror, skip the seed / `decomp_loop` /
spot-check cycle. The dropped def changes only the `.data`/`.bss` allocation (moved splat-side); the
`.text` is byte-identical once the externs land at their placed vrams, so the body matches on the
first build. S42 `osSetEventMesg` (drop `__osEventStateTab` + `__osPreNMI`, add one `extern`) matched
in one `make`. Only reach for the classical seed loop when dropping the def forces a *body* change
(e.g. an initializer the asm computes inline). Place each dropped global add-only in
`symbol_addrs.txt` at its asm-recovered vram (scalar `// size:0x4`, array `// size:stride×count`);
`pick_target.py` surfaces the array dimension as `defines-data:<name>[DIM]`.

**Function-local statics in a shared data blob (S45).** The fast path also covers a *function-local*
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

**Function-local static FP constant: the `data-static` pre-flag (S52).** A function-local
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

**The drop-extern *hoist* vs. the verbatim-static *carve* (S61).** Be precise about scope: this is a
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
size the extent from the COMPILED object, not the static's byte size:**
`mips-linux-gnu-objdump -s -j .data build/.../<file>.o` (the section is alignment-padded: `dtor` is
4B but `rotate.o(.data)` is **16B** = 4B + 12B pad), cross-checked against the baserom bytes to the
next data symbol. A short carve double-counts (the compiled `.data` injects its full padded extent
while the un-carved tail still emits the original bytes) → the ROM grows and the whole data segment
reflows (S61: a 4B carve grew the ROM +16B; the misleading `cmp` first-diff was at 0x1008, an
artifact of the size-grown file, not the real divergence). Clean-rebuild to verify. (S61 `guRotateF`:
16B carve `0xA35D0`→`0xA35E0`.)

**Unreferenced carve: locate the offset by ROM byte-search (S100).** The carved `.data` bytes may be
entirely UNREFERENCED: file-scope / function-local `static`s that no function reads, but which KMC GCC
2.7.2 still emits (an explicitly-`=0`-initialized static lands in `.data` not `.bss`, and an unused
initialized static is NOT elided at `-O3`). Then there is no `%hi/%lo(D_<vram>)` in any body: the
S61/S96 "size from the address band" step has nothing to read, and `pick_target`'s `defines-data`
flag names the symbols but can't bind a vram. **Locate it by byte-search:**
`mips-linux-gnu-objdump -s -j .data build/.../<file>.o` for the compiled bytes, then search the
baserom for that exact pattern (`xxd baserom.z64 | grep '<hex words>'`) → the ROM offset is the carve
start. The link order does NOT place it immediately after the previous mirror's carve; other files'
`.data` interleave (S100 `reverb.c`'s 0x20 sat at `0xA3560`, 0x100 B past drvrnew's `0xA3460` tail).
Confirm the extent against the next baserom data symbol, then 3-way split as usual. (S100 `reverb.c`:
`L_INC[]`={0x10,0x10,0x20} + `val`=0.0 + `lastval`=-10.0 (`0xC1200000`) + `blob`=0, 0x20 at `0xA3560`.)
Tooling follow-up (S100 #1, deferred off-cadence ranker change): `pick_target.py` could tag
`defines-data:<g>;unref` when the flagged global has no `%lo` ref in the owning fn bodies, signalling
the gate to byte-search rather than asm-recover.

**Carve timing: execution, never the plan gate (S68).** A `.data` (or any ld-section) sibling
carved from the C compile cannot land at the `/sprint-plan` gate: the gate validates the *stub*
state, where an `INCLUDE_ASM` `.text`-only object emits no `.data`, so the sibling carves an empty
range and shifts every following data byte → the gate green-ROM SHA check fails. The gate flips
**text only**; add the ld-section split during execution, **together with the verbatim body** (which
makes the `.o` emit the real `.data`). The stub's `%lo(D_<vram>)` resolves from the existing generic
`data` subseg in the meantime. (S68 `gu/align.c`: deferred the `[0xA35A0, .data, libultra/gu/align]`
carve from gate to the body step; the verbatim twin of S61's carve, first-build match.)

**Symbol-add vs. section-carve: the gate-safe drop-def sub-case (S81).** The S68 "execution, never
the gate" rule is specifically about an **ld-section sibling carve** (a `[<rom>, .data, …]` /
`.bss` / `.rodata` yaml subseg), which carves an empty range against the `.text`-only stub and shifts
every following data byte → gate SHA break. It does **NOT** cover a plain `symbol_addrs.txt` data
entry that merely **names an existing auto `D_<vram>` label** (the global already lives in a generic
`data`/`bss` subseg; the add only renames the dlabel, emits no new section). That symbol-add is
**SHA-neutral at the stub stage** and so MAY be performed at the plan gate alongside the function
symbol, saving the mid-execution second `make extract` the recover-extern otherwise forces. The tell
that a drop-def is the gate-safe symbol-add kind (not a carve): the dropped globals' vrams are already
visible as `%lo(D_<vram>)` in the **scaffold `.s`** (so they resolve from an existing region, no new
section needed). When in doubt, the gate `make extract && make` green-ROM SHA is the proof: naming a
pre-existing `D_` label is inert, a section carve is not. (S81 `io/siacs.c`: the 3 SI globals
`__osSiAccessQueueEnabled`@0x800C8210 / `__osSiAccessQueue`@0x801EFFB0 / `siAccessBuf`@0x800FAA00 were
all visible as `D_<vram>` in the scaffold asm; placing them at the gate would have avoided the 2nd
extract. Contrast the gu carves S61/S68/S73, which inject real `.data` and MUST stay at execution.)

**Validate a `D_<vram>` rename with a CLEAN rebuild, not an incremental `make` (S82).** The symbol-add
is SHA-neutral, but `make` does **not** track the INCLUDE_ASM `.s` dependency of the stub object. After
the gate `make extract` renames `D_<vram>`→`<name>` in the scaffold `.s`, an *incremental* `make` does
NOT recompile the stub `.o` (its `.c` is unchanged) → the stale object still references the now-removed
`D_<vram>` auto-symbol → `undefined reference to D_<vram>` link failure, masked by a stale-`.z64`
false-positive SHA. Run `make clean && make extract && make` (or `rm` the affected stub `.o`) to
validate the rename. This holds whether the rename happens at the gate OR mid-execution, so the gate
pre-place's only real saving over deferring is one fewer `make extract`; both still need the clean
rebuild. (S82 `io/controller.c`: gate-placing `__osEepromTimerMsg`@0x8012F4DC et al. failed the
incremental link with `undefined reference to D_8012F4DC`; `make clean` produced the green ROM.)

**Source-side detector: the coddog-band backstop (S73).** The S52 `data-static`/`rodata-literal`
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

---

## needs-header

**Rule:** Include-resolvability is a pick/DoR hazard. Two cases, distinguished at the gate.

**Trigger:** `pick_target.py` flag `needs-header:<inc>` (greps every upstream `#include` against the
project `-I` set: `include/{,libultra,libultra/internal,libkmc,libnusys}`; the grep is `#ifdef`-blind
so a dead-`_DEBUG` include can over-flag, so confirm against the upstream).

**Sub-cases / variants:**

**Vendorable annotation (S54).** `pick_target` tags each missing header `<inc>(vendorable)` when it
is copyable from upstream: a public companion under an `UPSTREAM_INC_ROOTS` include dir (copy into
an `-I` dir) OR a source-private header found by basename under an `UPSTREAM_SRC_ROOTS` source tree
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
  `src/lib<...>/<band>/x.h` (e.g. `gu/guint.h`, copied alongside `gu/random.c` in S49), every
  later sibling that quote-includes it is already clean, no enabler. Since S50 `missing_includes`
  takes the mirror dir and drops these, so they no longer over-flag as `needs-header`/`blk`. This
  is the include-side of `#open-band-fast-path`; the over-flag only persists for an angle-include
  (`<x.h>`), which is never source-relative.
- **Unindexed `-I` path** → a deferred Makefile enabler; defer the pick unless the PO adds the path.
  The audio band's bare `#include <libaudio.h>` (header sits at `include/libultra/PR/libaudio.h`,
  but there is no `-I include/libultra/PR`) is the standing example: flagging an audio leaf as a
  clean flip without that path is a false-clean. Prefer `<PR/x.h>`-style `monegi/` upstreams.

---

## per-library standard-C-header isolation

**Rule:** libultra and libkmc each ship their own standard C headers and they are NOT interchangeable
(libultra's `stdlib.h` defines `lldiv_t`; libkmc's lacks it). The `-I` order puts `include/libultra`
before `include/libkmc`, but neither holds the std headers directly, so a bare `#include "stdlib.h"`
from any source falls through to `include/libkmc/stdlib.h`. A verbatim libultra mirror needing a
libultra-only type therefore fails against the libkmc header. **This is a pick/DoR blind spot:**
`needs-header` greps resolvability, not which library's header resolves nor whether it has the needed
types: a resolvable-but-wrong-library include passes the grep AND the gate build-check (the
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
  - **CORDIC `double↔long long` cvt helper (S112/S113).** The libkmc math mirrors (`atan.c`, `sin.c`)
    convert `XLONG = (double)expr * MBIT` where the source double can be negative, yet KMC GCC at `-O`
    emits **`__fixunsdfdi`** (the UNSIGNED `double→u64` helper, @0x800B3C20), never `__fixdfdi`. So a
    libkmc CORDIC C-mirror needs only `__fixunsdfdi` + `__floatdidf` (@0x800B3D40) placed; there is no
    signed-cvt-helper enabler to recover. Confirmed identical in both atan.c and sin.c (verify at the
    gate by reading the subseg's jal list, never by assuming `__fixdfdi` from the C signedness). The
    long-long shifts (`x>>i`) are inlined (no `__ashrdi3`/shift-helper jal).
- **libultra = `-O3 -fsigned-char`** with `MIPS_VERSION=-mips3` for VERSION_J (ultralib gcc.mk,
  but see the char-signedness note). Global CFLAGS uses `-mips3` (changed from `-mips2` in S33).
  `-O3` enables inlining of small same-TU functions and affects delay-slot scheduling.
  **Char signedness is `-fsigned-char`, NOT ultralib-J's `-funsigned-char` (S65).** ultralib's
  `gcc.mk` adds `-funsigned-char` for VERSION_J, but this ROM's libultra was built signed: an S65
  full `make clean` rebuild under `-fsigned-char` reproduced the baserom SHA-1 exactly (every
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

**Provenance:** S33 (`-mips3`).

---

## char-signedness (libultra is -fsigned-char)

**Rule:** This ROM's libultra was compiled with **signed `char`** (`-fsigned-char`), a ROM-proven
deviation from ultralib's documented VERSION_J profile (gcc.mk adds `-funsigned-char` for D–J). The
band default `LIBULTRA_CFLAGS` therefore carries `-fsigned-char`. Most libultra C is
char-signedness-*ambiguous* (matches under either flag), so this only ever surfaces on a file whose
codegen actually depends on `char` signedness.

**Symptom (the S65 discriminator):** a verbatim C mirror SHA-misses, and the only diff is byte-load
signedness: the ROM loads `lb` + `sll/sra` sign-extend (and may emit a phantom empty stack frame),
while the wrong flag gives `lbu` + `andi 0xff`. The exposing pattern is a `char` lvalue compared
against a **non-zero** value (e.g. `strchr`'s `*s != ch`); a comparison against literal `0` is
signedness-invariant (`strlen`/`memcpy` matched under both: `memcpy` fully, `strlen`'s `lb` is the
compiler's free choice for a `!= 0` test).

**Procedure:**
1. The band default is already `-fsigned-char` (S65), so a new libultra mirror compiles signed with no
   action. If a mirror SHA-misses with the `lbu/andi` vs `lb/sll-sra` signature, you have the inverse
   case (a TU that wants the *opposite* signedness).
2. Per-file override mechanism (add in `mk/libultra.mk`; the explicit-target var beats the `libultra/%.o` pattern):
   `$(BUILD_DIR)/$(SRC_DIR)/libultra/<rel>.o: C_PROFILE_CFLAGS = $(subst -fsigned-char,-funsigned-char,$(LIBULTRA_CFLAGS))`
   (or the reverse subst). Append-and-last-wins also works since gcc takes the final `-f*-char`.
3. To re-settle the band default after several such files, run `make clean && make` with the global
   flag flipped and compare the ROM SHA-1: the S65 authoritative test (a clean rebuild under
   `-fsigned-char` reproduced the baserom exactly, proving the global default, not a per-file patch).

**Distinct from** the per-field **cast-divergence** hazard (a 2.0L-vs-J `(type)` widening on one
struct field's high word): that is a source/version difference; this is a whole-TU compiler flag.

**Provenance:** S65 (string.c `strchr`/`strlen` exposed it; the per-file override was the initial
fix, then the clean-rebuild test proved `-fsigned-char` is the correct band default and the override
was removed).

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
4. **An assert that is the SOLE body of an `if` cannot be wrapped alone:** `#ifdef _DEBUG`-wrapping
   just the assert leaves the `if (...)` bodyless under non-`_DEBUG` → a syntax error. Wrap the WHOLE
   `if`+assert in the `#ifdef _DEBUG` block (S106 sched.c:575 `if (avail & OS_SC_DP == 0) assert(...)`,
   an always-false dead branch: the `&`-vs-`==` precedence makes the guard `avail & 0`, so the whole
   thing is zero code in the release ROM either way; wrapping it is SHA-neutral). Same for an assert
   that is the body of any other bodyless control statement.
5. **Count every assert with `assert\s*\(` (allow whitespace before the paren).** `pick_target`'s
   `bare_asserts` already does (`re.findall(r"\bassert\s*\(", ...)`), so its `bare-assert:N` count is
   authoritative; the agent's manual strip grep MUST match it. S106 sched.c had 9 bare asserts but a
   manual `^\s*assert\(` grep found only 7: it missed `assert (t->msgQ)` and `assert ( (type == …))`
   (a SPACE before the paren), so 2 asserts compiled in on the first build (`.text` +0x80 `jal
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
[dual-section carve note](#defines-data) (S96 drvrnew.c: jtbl+consts `.rodata` here + `*_PARAMS[]`
`.data` there).

**Trigger:** A libultra/libkmc file with a compiler-generated rodata constant at a different ROM
offset than its text (e.g. a `2^32` double). `pick_target.py` pre-flags this as
`rodata-literal:0x<vram>[,0x<vram>…]` on a mirror candidate whose asm loads an anonymous pooled FP
constant (`ldc1/lwc1 %lo(D_<addr>)`), so the sibling split is a known DoR enabler, not a finalize
surprise. The address is classified by segment first (data-segment statics are `data-static`, not
`rodata-literal`; see #defines-data, S52). The pre-flag lists the full extent: GCC moves a
pooled `double` into an FP register pair via an `lw` pair + `mtc1` (not `ldc1`), so a literal's 2nd
(and further) word is an integer `lw %lo(D_<addr>)` invisible to the FP-only scan. pick_target adds
the rodata-band `lw` refs so the split width is sized correctly (S52 `guLookAtReflectF`: `-1.0`
@0x800D2510 via `ldc1` + `1.0` @0x800D2518 via an `lw` pair = a 16-byte block, not 8). The pre-flag
also appends `;carve-end=0x<vram>` (S64 #2): a multi-`du` dlabel block's trailing word has no `%lo`
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
**Generic-subseg-bound carve = exact extent, no split (S101).** When a carve start or end coincides
with an existing generic `[off, (ro)data]` subseg boundary, that generic subseg's opposite boundary
is the exact carve extent: the linker already split the section there, so the carve is a 1-line
attribute flip of the existing generic subseg (`[off, data]` → `[off, .data, path]`), not a split,
and `carve-end`'s upper bound is exact (the S93 xldtob single-line case, generalized). S101 `env.c`:
both sections were whole-subseg flips. `.data` `eqpower[128]` = the entire generic `[0xA3460, data]`
(0x100 B, vram 0x800C8060); `.rodata` jtbl+13 literals = the entire generic `[0xAD6F0, rodata]`
(0xF0 B, vram 0x800D22F0). pick's `carve-end=0x800D25C0` over-stated (real end 0x800D23E0 = the next
named `.rodata` boundary 0xAD7E0). After the env|filter split, env is a single-file-pack and the sole
owner, so the `;owner-per-member` marker is moot.
**Carve-START widening (S93): the reported min can understate the carve start.** The FP/`lw` scans
see only scalar `%lo` loads; a `.rodata` block that opens with a file-scope `static const <type>
name[]` array begins at the array base (an `addiu %lo` address-of) and may carry string literals
("NaN"/"Inf") ahead of the scalar pool, both invisible to the scans, so `min(rodata_lits)` is too
high (xldtob.c: the FP scan's min 0x800D2820 sat 0x50 B inside the block, which actually starts at
the `pows[]` dlabel 0x800D27D0). `pick_target.py` widens the carve-start to the rodata subseg
boundary (symmetric to `carve-end`), but only when the upstream defines a file-scope `static const`
array (`defines_file_static_const_array`): a `const` static is file-private rodata, so the whole
code-segment rodata subseg is this object's own, which makes the widening FP-safe. (The direct
`addiu %lo` band scan that would find the base without the source gate was reverted S92: in the
`.data` band it could not tell a file's own static from a shared cross-file extern; the `static
const` source gate confines this to rodata where that ambiguity does not arise.)

**Carve-START past a FOREIGN leading symbol; the `.o` section size is the extent oracle (S104).** A
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

**Named vs anonymous pool (S66): no difference; the named-rodata caveat is retired.** A pooled
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

**Switch jump table (S76): same carve, automatic byte-match.** A `switch` with a contiguous-ish case
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

**c-combined pack: the carve owner is per-member, not the primary (S98).** The rodata-literal/jtbl
scans span the whole subseg (S55: one `.c` → one `.o` → one `.rodata`, so a sibling fn's pooled
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
whether splat already bounded the carve (S93): no split when the generic subseg's extent already
matches.** Splat's per-TU auto-segmentation often already brackets the mirror's rodata as its own
generic `[0x<ROM>, rodata]` subseg; compare its `[start, next-start)` extent to the `.o(.rodata)`
size before computing a split point. When they match (xldtob.c: `[0xADBD0, rodata]` already spanned
0xADBD0→0xADC40 = 0x70, exactly the `.o(.rodata)`), the carve is a 1-line attribute change
(`rodata` → `.rodata, path/to/file`), not a split. This saves the split arithmetic and is the common case
for cleanly-segmented libc/gu mirrors. **Timing: do this during execution with the body, not at the
plan gate.** Like the `.data` carve, a `.rodata`
sibling added against an `INCLUDE_ASM` stub carves an empty range (the stub emits no `.rodata`) and
reflows the rodata segment, failing the gate green-ROM check (see `#defines-data` "Carve timing",
S68). The gate flips text only.

**Provenance:** S38 (`osAiSetFrequency`: split `[0xAD5E0, rodata]` at 0xAD6A0; inserted
`[0xAD6A0, .rodata, libultra/monegi/ai/aisetfreq]` = 8-byte double + 8-byte pad at 0x800D22A0).
S48 (`__osViSwapContext`: same `2^32` double; split `[0xAD9C0, rodata]` at 0xAD9E0; inserted
`[0xAD9E0, .rodata, libultra/io/viswapcontext]`; this sprint added the `rodata-literal` pre-flag).
S66 (`sinf`: anonymous pool, split `[0xAD960, rodata]` → `[0xAD960, .rodata, libultra/gu/sinf]`,
exact `.o`-size 0x60 fit; `cosf`: named pool, split `[0xAD6F0, rodata]` at 0xAD860 → inserted
`[0xAD860, .rodata, libultra/gu/cosf]`, retiring the named-rodata collision caveat as a phantom).
S76 (`__osDevMgrMain`: first switch jump table sibling, `switch (mb->hdr.type)` → `jtbl_800D2280`;
split `[0xAD5E0, rodata]` at 0xAD680 → inserted `[0xAD680, .rodata, libultra/io/devmgr]` = 7 case
`.word`s + 1 zero pad to 0x20; byte-matched first build with no confidence check; added the
`rodata-jtbl` pre-flag).
S93 (`xldtob.c`: `static const ldouble pows[]` + "NaN"/"Inf"/"0" strings + 1.0/1e8 literals = 0x70;
the generic `[0xADBD0, rodata]` subseg already bounded the exact extent → 1-line attribute change
`rodata` → `.rodata, libultra/libc/xldtob`, no split; added the carve-start widening pre-flag, where
`defines_file_static_const_array` source-gates the rodata-subseg-start carve-start, since the FP scan
missed the `pows[]` base 0x800D27D0 by 0x50 B).
S98 (`[0x811A0]` `c-combined:2file[mainbus|resample]`: split text at 0x81310, then resample's `.rodata`
carve split `[0xAD6F0, rodata]` 3-way → inserted `[0xAD7E0, .rodata, libultra/audio/resample]` = the
MAX_RATIO double `D_800D23E0` (8B) + `jtbl_800D23E8` (10 words, 40B) = 0x30; mainbus.c carve-free.
First owner-per-member case; added the `;owner-per-member` c-combined marker after the
whole-pack scan over-attributed resample's rodata to the mainbus primary row).
S112 (`atan.c`: first libkmc C-mirror; generic `[0xADC40, rodata]` (`cordic_atan_divisor_2_60`
+ 10 pooled FP literals + the `"atan2"` string, 0x60 B) already bounded the extent → 1-line
attribute flip `rodata` → `.rodata, libkmc/atan`, no split; byte-matched first build).

**Stale orphan after a retype-carve (S112): expected, harmless.** Flipping a generic `[ADDR, rodata]`
→ `[ADDR, .rodata, <file>]` leaves the pre-carve `asm/data/<ADDR>.rodata.s` (and, on an incremental
build, a stale `build/asm/data/<ADDR>.rodata.o`) on disk: `make extract` does not prune a removed
subseg's per-file output, and `make clean` wipes `build/` but not `asm/`. Both are gitignored and
absent from `mariogolf64.ld` (unlinked), so a clean-rebuild ROM SHA-1 == baserom is proof they're
inert. Do not mistake the leftover `.s` at verify time for a double-carve (a double-link would
overlap and break the SHA, so a green clean-rebuild already rules it out).

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

**Provenance:** S34 (`osAiSetNextBuffer`, score=600 throughout, spot-check MATCH).

---

## Assembler differences + byte-cmp spot-check

**Rule:** `src/%` is assembled by KMC `as` (`tools/cc/as`), not modern `mips-linux-gnu-as`. KMC `as`
emits `addu` for `move dst,zero` (encoding `0x…1021`, matching the original ROM) where modern `as`
emits `or` (`0x…1025`), and auto-pads `.text` to 16-byte section alignment. `asm/%` keeps modern `as`
(raw asm uses modern-only directives). Because objdump renders both `…1021` and `…1025` as
`move v0,zero`, mnemonic-level diff silently false-positives. **Spot-check by byte-level `cmp` of
raw `.text` only.**

**Trigger:** Finalizing a classical match (the spot-check step). Mnemonic diff looks clean but you
need ground truth.

**Procedure:**
- `cmp` raw `.text` bytes of the in-tree compiled object vs the reference object.
- **Split-subseg target:** `decomp_loop.py` resolves the function to the OLD pre-split segment label
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
Also `c-combined:<n>file[stem1|stem2|…]` (S64 #3): the C analog of `combined-subseg`, firing when ≥2
distinct C upstream files share one asm subseg, naming the split targets. It pre-prices the split
and surfaces a cheap clean leaf that a big combined subseg buries past smallest-first (e.g. the sp
register-shim pack `func_800B16A0` → `3file[sprawdma|spsetpc|spsetstat]`, hidden under `upstream
none` because its unattributed `func_` primary set the column). Caveat: a member whose ROM symbol
is a `#pragma weak` alias (e.g. `cosf`, whose `gu/cosf.c` defines `__cosf`) shows as `=?` in the pack
and is not counted in `c-combined`, since `upstream_index` keys on the defined name, not the alias; such
a leaf still needs manual identification (S64 `cosf` was found by disassembly, recorded in BACKLOG).

**Sub-cases / variants:**

**Inter-file stray leaf: `unattrib-leaf:0x<vram>` (S120).** Within a `c-combined` pack, a lone `=?`
member whose nearest named-C members BEFORE and AFTER resolve to DIFFERENT stems straddles the
file boundary — the split must consciously assign it to one singleton, or a silent `?` rides into the
wrong side. `pick_target.py` flags its vram, but only for a LONE straddler (a clean 2-file split with
one stray leaf); a whole foreign TU interleaved in (e.g. the `__assert`/`nuboot` game-boot region's 11
interspersed game `?`s) is the messier `pack`/`upstream-fncount-mismatch`/`game-region-mirror` case,
not a stray boundary leaf, so it does NOT fire. Motivating case: the pre-split S120 `[0x7D970]`
`c-combined:2file[nucontgbpakfwrite|nusimgr]` pack, where `func_800A2780` (a 0xC-byte leaf returning
`&0x800F77D0`, in NEITHER upstream source) sat exactly between `nuContGBPakFwrite` and `nuSiMgrInit`.
The S120 split at the 16-aligned `0x7DB80` boundary left it in the trailing `[0x7DB80,asm]` subseg
with `nusimgr` (a carry-over), but the flag would have surfaced it at the gate.

**Resolution rule — a leaf returning `&<static>` is SAME-TU, not foreign (S122).** S120's "in NEITHER
upstream source" framing for `func_800A2780` was incomplete: the 0xC leaf is `return siMgrStack;` —
its `&0x800F77D0` target is the BASE of `nusimgr.c`'s own file-static `siMgrStack` (siMgrThread
0x800F7620 + 0x1B0 = 0x800F77D0). A static is file-local, so a function that takes its address MUST be
compiled in the same TU. The leaf therefore IS `nusimgr.c` — an MG64-added leading accessor the
2.07 upstream lacks — and banks WITH it (written `return siMgrStack;`, `func_` name kept), NOT as a
foreign micro-TU. **So when an `unattrib-leaf` / stray boundary leaf returns `&<addr>`, resolve `<addr>`
against the placed static map of the two neighbors BEFORE deciding foreign-vs-same-TU: an address
inside a neighbor's static (file-local) range proves same-TU; only an address in a global/other-TU
range (or an unrelated MMIO/const) leaves it genuinely foreign.** (A `pick_target` `leaf-returns-static:
<file>` annotation that resolves the leaf's `lui/addiu` target against the static map would surface this
at the gate; deferred tooling.)

**Not every pack splits: `single-file-pack` (S67 #2).** When every pack member resolves to one
upstream C file (one stem, no `=?`/asm members), `pick_target.py` emits `single-file-pack:<n>fn[…]`
instead of `pack:<n>fn[…]`. This is the atomic-verbatim-mirror class (guPerspectiveF+guPerspective
S55, guLookAtHiliteF+guLookAtHilite S64, guTranslateF+guTranslate S67): the whole subseg is one
upstream file, so there is no upstream-file boundary to split at. Copy it verbatim and bank/spike in
one shot (route to `#upstream-mirror-pattern`, not the split procedure below). The `pts` seed is
unchanged (the pack penalty keys on `nfns>1`, not the kind), so the relabel is display-only; it just
stops the gate reading an atomic mirror as a split-required blocker. A mixed asm+C pack or a
multi-stem pack keeps `pack`.

**Foreign TU bundled in a single-stem pack: `upstream-fncount-mismatch:<m>vs<n>` (S104).** The
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

**The split-off TU's carried label is a HINT, not a source attribution (S105).** When you split a
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

**Per-distribution upstream caveat (S51):** `pick_target.py`'s `upstream` column and the gate's
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

---

## isolated-compile caveat

**Rule:** The execution loop compiles `nonmatchings/<func>/base.c` standalone; KMC GCC's
regalloc/scheduling can differ from the in-tree compile. That is why every match runs an in-tree
byte-`cmp` spot-check before being declared. If iteration mismatches cluster around extern address
loads (HI/LO16 relocs against project-local symbols), those are isolation-only; the spot-check is
the truth.

**Trigger:** Score ≠ 0 in isolation but the mismatches are HI/LO16 reloc address loads.

**Fast recognition signal (S43):** `decomp_loop.py` reports a non-zero `score` with empty
`top_mismatches` AND `match_count == total_rows` (every row matched, yet a residual score). That
combination is definitionally an isolation artifact: asm-differ found no mnemonic-row diff, so the
score is pure reloc/addend noise (struct-field LO16 addends like `pfs->queue`/`pfs->channel`, extern
HI/LO16 calls against now-placed symbols). It is not a near-miss: do not iterate C, do not run the
permuter. Go straight to the in-tree spot-check / full-make ROM SHA-1, which is byte-authoritative.
S43 `osGbpakGetStatus` scored 15 / 99.8% this way (76/76 rows, empty mismatches) and the full make
matched the baserom unchanged.

**Procedure:** Trust the in-tree spot-check, not the isolated score.

---

## Decompile-vs-asm authority

**Rule:** The asm (`disassemble_function`) is ground truth, not the Ghidra decompile. The decompiler
can be silently wrong on classical leaves. Use the decompile for shape/types; translate the logic
from the instruction listing.

**Provenance:** S11 (Ghidra rendered `func_800AB600`'s return as `return 0` when the asm returns
`(status>>8)&1`).

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

**Combined-subseg case (S51):** When a 2-fn pack has fn2 packed tight after fn1 at a non-16 offset
(fn1's size is not a 16-multiple), you cannot split into per-fn subsegs: KMC `as` pads fn1's
standalone `.o` `.text` to 16, inserting bytes that shift fn2 downstream and breaking the ROM SHA-1
on a bare stub flip. The fix is the opposite of the normal pack split: flip the whole pack to one
combined `[0x<rom>, c, lib<name>/<basename>]` subseg holding BOTH functions in one `.o` (both
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

**Rule:** splat extracts the whole subseg slot: the function PLUS the `0x00000000` nop padding that
fills the gap up to the next subseg. When that next subseg sits at an alignment above 16 (32/64/128),
the pad is larger than the ≤12B a compiler's 16-byte `.text` alignment emits. A verbatim C mirror
therefore compiles short of the slot, the ROM shrinks, and everything downstream shifts → a SHA-miss
in the execution middle. It is invisible to every gate check: the `INCLUDE_ASM` stub carries the pad,
so the gate build is green; only the real C body drops it (the S18/S44 late-surfacing class). The
sibling guard: a function whose 16-aligned size already fills its slot (the same-subseg neighbor)
mirrors clean; only the one preceding a higher-aligned boundary pays.

**Trigger:** `pick_target.py` flag `trailing-pad:<n>B@<align>` (S79). `<n>` = the residual pad bytes
beyond the compiler's 16-align, `<align>` = the next boundary's alignment (e.g. `96B@128`). Pre-flags
it at the gate so the split is priced, not localized mid-execution. (`docs/hazards.md:122`: the `.ld`
does NOT `ALIGN` between subsegs, so the pad must live in an object; there is no linker fallback. This
is the C-mirror dual of the KMC-`as` 16-pad note in `#asm-mirror-vendoring`.)

**Procedure:** flip the function subseg to `c` as usual, then split a nop-pad `[0x<gcc_o_end>, asm]`
subseg between it and the next subseg. `<gcc_o_end>` = the function subseg ROM offset + the compiled
`.o` `.text` size (`mips-linux-gnu-objdump -h build/.../<file>.o` → the 16-aligned `.text` extent);
splat re-extracts the residual nops there. The compiler's own 16-align nops (the `.o`'s tail) match the
baserom nops at those addresses; the pad subseg supplies the rest up to the higher-aligned boundary.
A pure-nop subseg is never a pick: `pick_target.py` skips any all-nop asm subseg (`code_end_rom` None
with a present listing), which also retired 8 pre-existing all-nop overlay stubs (`func_ovl*_801F4A30`).

**Worked example (S79 `__osContRamWrite`):** function 0x204 (516B) → GCC `.o` `.text` 0x210 (528B,
16-aligned) → real slot 0x270 (624B) up to the 128-aligned `osAfterPreNMI` (0x800AF880). Split
`[0x8AC20, asm]` (= 0x8AA10 + 0x210) carries the residual 0x60 (96B) of nops. Re-extract + `make` →
ROM SHA-1 == baserom. The body itself never diverged (129 instrs byte-identical); the gap was purely
the trailing pad. The contramread sibling (slot == 16-aligned fn size) mirrored clean, no split.


---

## intrinsic-likely / maybe-upstream (signature hints)

**Rule:** Two advisory flags from the signature matcher.
- `intrinsic-likely`: a register/FPU/cache/TLB shim, not a classical target. If the detail names a
  vendorable ultralib TU (`intrinsic-likely:os/getcount.s`), asm-mirror it (see
  `#asm-mirror-vendoring`). `intrinsic-likely:cp0-asm(identify-TU)` is an un-named privileged hand-asm
  subseg (also `#asm-mirror-vendoring`; identify the TU first). A bare `intrinsic-likely` (no `:<tu>`,
  no `cp0-asm`) is a no-source shim → plain `hasm`.
- `maybe-upstream:<lib>:<bases>`: an un-named subseg (`func_<addr>`) that the signature matcher
  thinks is an un-named SDK mirror (the S13 trap). Verify against the candidate upstream at the gate
  before treating it as classical.

**Trigger:** `pick_target.py` flags `intrinsic-likely` / `maybe-upstream:…`.

**Provenance:** S13 (un-named SDK mirror trap).

For a definitive (not IDF-guess) version of `maybe-upstream`, see `#coddog-cross-ref`: it pairs the
un-named fn with its ultralib identity by instruction-hash match.

---

## coddog cross-ref

**Rule:** `pick_target.py` classifies an un-named (`func_<addr>`) subseg as `upstream none` →
classical, even when it is a verbatim ultralib mirror. The names being absent is the only reason.
S71 crc.c was mis-seeded pts-13 classical (`none` + `pack:2fn`) but is a trivial verbatim
2-fn mirror. coddog (`compare2`, reloc-masked instruction hashes) pairs each MG64 fn with the
ultralib fn it matches, revealing the source file: this turns the `none`/classical guess into a known
mirror target. Per the S71 sweep, ~all of the remaining libultra band is verbatim-mirrorable.

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
   `.c`); the `@<pct>` is the confidence (99.99 = byte-verbatim once placed). NOT every coddog
   match is an atomic verbatim cp; see step 4.
4. **S72 trap re-scan:** because the candidate is un-named (`func_<addr>`), its own
   `defines-data` / `file-static` / `needs-header` detectors (which key off the *named* upstream)
   never ran, so a coddog match to a file that defines data or a file-scope static looked clean
   under the bare flag. `build_rows` now re-runs those three *file-level* (name-independent)
   detectors on the coddog-resolved `.c` and appends their hazards (and `seed_points` re-prices via
   `drop`/`needs_copy`). So a `coddog-mirror` candidate carrying `file-static` / `defines-data`
   routes to `#file-static-bss-layout-conflict` / `#defines-data` (a `.data`/`.bss` sibling carve or
   classical), NOT an atomic verbatim cp. Motivating case: `func_800AC110 → piacs.c` ranks pts-5
   (not the bare-coddog pts-3) because piacs.c DEFINES `__osPiAccessQueueEnabled` + a
   `static OSMesg piAccessBuf[]`; `osMotorStop → motor.c` likewise surfaces
   `defines-data:__osMotorinitialized[...]`.

5. **S81 twin disambiguation:** coddog's reloc-masked hashes can pair a candidate with a
   near-identical twin file rather than its real source. `func_800AC110`+`__osSiGetAccess`+
   `__osSiRelAccess` (the SI access-queue subseg) coddog-matched `src/io/piacs.c@99.99`, but its
   named members name `siacs`. `pick_target.py` cross-checks the coddog basename against the pack's
   *named* members' upstream basenames and, on disagreement, appends `coddog-twin:<matched>!=<member-src>`.
   Mirror from `<member-src>` (siacs.c), not the coddog `<matched>` file (piacs.c). A @99.99 twin
   is byte-identical once placed (the body is the same either way), so this only removes a manual
   reconcile step, but it pins which upstream the agent copies + which symbol names it places. No-ops
   when the candidate has no named member (nothing to disagree with) or the basenames agree.

6. **S88/S92 structural-fingerprint guards (a @99% match that is NOT a source attribution).** coddog
   hashes a fn's branch/call SHAPE, so a small source can fingerprint-match a much larger, unrelated
   pack. Two name-independent guards fire when ONE coddog `.c` claims a whole multi-fn pack:
   - **`coddog-fncount-mismatch:<m>vs<n>`** (S88): the matched source defines FEWER fns (`m`) than the
     pack holds (`n`), so it cannot be the sole source → the pack is multi-file. Under-count only (a
     true single source may define MORE via version/`_DEBUG`-gated extras). S92: the check now also
     runs when the coddog identity is carried by an un-named TAIL member (not the pack leader), the
     case the S88 primary-only check missed. `func_80050400`'s leader is absent from the map but a tail
     member carries `llcvt.c` (8 fns vs the 11fn pack).
   - **`coddog-structural:<file>@<pct>`** (S92): the matched source's meaningful-LOC implies a compiled
     size far below the subseg's (`subseg_bytes > 64 × source_LOC`). `llcvt.c` is 8 trivial `return d;`
     conversion stubs (~250 B) yet coddog matched it @99.99 to THREE distinct subsegs (2032 B / 2912 B /
     7728 B). Advisory/display-only, the size-dimension companion to the fn-count guard. When either
     guard is on a coddog row, do NOT treat the `coddog-mirror` flag as a single-file mirror identity.
     - **S108: `llcvt.c` is not linked at all (workhorse-linked / wrapper-absent).** Beyond the
       structural over-match: MG64 never links `llcvt.c`. Its 8 stubs are thin wrappers that `jal` the
       libgcc soft-float workhorses (`__fixdfdi`, `__floatdidf`, …); KMC GCC emits calls to those
       workhorses directly, so the workhorses ARE present (`__floatdidf` @0x800B3D40,
       `__fixunsdfdi` @0x800B3C20, in the libkmc/libgcc band, already named) while the `__d_to_ll`…
       `__ull_to_f` wrapper TU is absent everywhere (name files + asm). So every `__d_to_ll @99.99`
       row is a reloc-masked FP on the stack→`jal`→return stub shape (`func_800504E8` calls a game fn;
       `spawn_object_simple` IS a game fn). Lesson for a future planner: a tiny-stub coddog identity
       can mean the source TU was never linked, not just over-matched; confirm the *workhorse*
       symbols, not the wrapper, before chasing the `.c`.
   - **NOT done (S92 carry-over):** a `.data`-carve detector for a file's OWN initialized file-statics
     (the `_Litob` ldigs/udigs class). An `addiu %lo(D_<addr>)` into the `.data` range cannot be
     distinguished from a SHARED extern reference by asm alone (the same instruction serves both), and
     `refs-unplaced` has gaps, so a naive scan mis-routes cross-file externs to a phantom carve. Deferred
     to a dedicated tooling sprint; see `BACKLOG.md ## Carry-overs`.

**Trigger:** `pick_target.py` flags `coddog-mirror:<file>@<pct>` (only when the map exists); any
`file-static` / `defines-data` / `needs-header` on the *same row* is the S72 trap re-scan; a
`coddog-twin:<matched>!=<member-src>` on the same row means coddog named the twin, so mirror from
`<member-src>`; a `coddog-fncount-mismatch` / `coddog-structural` on the same row means the @99%
match is a structural fingerprint, not a source attribution (the pack is multi-file / mis-attributed).

**Provenance:** S71 (crc.c mis-seed; the recipe + reusable tool memory in `coddog-ultralib-crossref`);
S72 (the trap re-scan: coddog-mirror candidates can hide a defines-data/file-static BSS trap);
S81 (`io/siacs.c`: coddog named the twin `piacs.c`, named members → real source `siacs.c`; the
gate-safe drop-def symbol-add note also landed in #defines-data); S88 (`coddog-fncount-mismatch`);
S92 (the fncount check extended to tail-carried identities + the `coddog-structural` size guard,
both retiring the llcvt false-positive class, where one tiny source fingerprint-matched 3 subsegs).

**Sub-cases / variants:**

**Nusys sweep (libnusys analog):** the same machinery pointed at the nusys-2.07 library instead of
ultralib. `make coddog-sweep-nusys` runs `tools/build_nusys_ref.sh` (compile every
`nusys/src/mainlib/*.c` with the project's KMC GCC + `LIBNUSYS_CFLAGS`, the in-tree mirror recipe,
into `build/nusys-ref/`, then merge to one relocatable ELF) then `tools/nusys_sweep.sh` (`compare2`
MG64 vs that ELF, writing `tools/coddog/nusys_map.tsv`, format
`mgname<TAB>nusysname<TAB>mainlib/<file>.c<TAB>pct`). `pick_target.py` (`build_coddog_nusys_index`)
reads it as a SEPARATE additive pass: a `>=99%` hit on an un-named subseg flags
`coddog-mirror:mainlib/<file>.c@<pct>` and re-prices `upstream libnusys` (libnusys is already a
first-class upstream lib, so the column / `--lib` / `seed_points` need no other change). The pass is
keyed on the nusys map alone, so an absent `nusys_map.tsv` leaves libultra ranking byte-identical;
the source-path prefix (`mainlib/` vs `src/`) tells the two maps apart downstream, and re-pricing
routes `coddog-mirror:mainlib/...` to `src/libnusys/`. Caveat: nusys has many tiny (5 to 9
instruction) wrappers (`nuGfxSetUcodeFifo`, `nuContGBPakMgrInit`) that coddog reports at 99.99%
against unrelated small fns, the same structural-fingerprint class as steps 5 and 6; smallest-first
+ the gate read filter them (a big pack mis-re-priced to libnusys ranks last by size). **Provenance:**
this sweep.

**Nusys version divergence (S117): MG64's libnusys is NOT uniformly nusys-2.07.** `build_nusys_ref.sh`
compiles the pinned 2.07, so `coddog-mirror:mainlib/<file>.c@99.99` is a STRUCTURAL match, not a
byte-exact source pin. Some files are an EARLIER revision: `nucontmgr.c` is **2.05** (the 2.06/2.07
rewrite changed `nuContMgrInit`'s loop from `for(...;cnt++){ ...; bitmask<<=1; }` to the comma-operator
`for(...; bitmask<<=1, cnt++)`, which the KMC compiler emits 2 instrs shorter, `-0x10` `.text`, so the
`.ld`'s concatenated subsegs cascade a ROM-wide `-0x10` shift). **The tell:** a clean verbatim mirror
whose compiled `.o` `.text`/`.data` section size differs from the subseg size despite the coddog hit
(here `.text` 0x330 vs 0x340). **Recovery: version-hunt.** Compile every available nusys revision
(`~/development/repos/nusys/src/{1.10,1.20,2.00,2.05,2.06}` + the 2.07 pin) and pick the one whose
`objdump -h` `.text`/`.data` sizes match the subseg; 2.00 and 2.05 are code-identical (only JP-Shift_JIS
vs EN comments), so prefer the English 2.05. Tooling follow-up: `build_nusys_ref` could sweep multiple
versions, or `pick_target` could cross-check the coddog hit against the subseg byte-size and flag a
`coddog-size-mismatch` when they disagree.

**Audio libraries (libmus / libnaudio / nuaulstl): a multi-version, multi-compiler sweep that pins
the toolchain too.** MG64 embeds libmus (the Software Creations `mus_*` sequence player) plus an
n_audio synth layer. These had no prebuilt reference at the project profile AND an unknown
version/compiler, so `tools/build_audio_refs.sh` builds a `{compiler × opt × version}` MATRIX from
the manifest `tools/audio_ref_versions.tsv` (one row per lib+version; the script expands compilers ×
opts). Compilers: `kmc` (project KMC gcc 2.7.2, the proven path), `ido53`/`ido71` (IDO `cc` via
ido-static-recomp, the authentic vendor compiler). Each cell → `build/audio-ref/<cell>/ref.o`;
`tools/audio_sweep.sh` fingerprints MG64 against every cell → `tools/coddog/audio/<cell>_map.tsv`;
`tools/audio_pin.py` ranks cells per lib by `(#exact-100%, #>=99%, #total, mean%)`, writes the
canonical `tools/coddog/<lib>_map.tsv` + `tools/coddog/audio_pins.tsv` (lib → pinned src root), and a
`PIN_REPORT.md`. `pick_target.py`'s `_resolve_audio` pass (the libmus/libnaudio/nuaulstl analog of
`_resolve_nusys`) reads those maps + pins: a `>=99%` member hit flags `coddog-mirror:<file>@<pct>`,
re-runs the trap battery against the pinned `.c`, and re-prices `upstream <lib>`. Absent maps leave
ranking byte-identical (additive-pass invariant). Run via `make coddog-sweep-audio`.

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

**Rule:** N64 SDK source ships with CRLF (DOS) line endings. A vendored HEADER copied verbatim keeps
its CRLF, and KMC GCC 2.7.2's preprocessor treats a `\` at end-of-line as a line-continuation ONLY when
the `\` is immediately followed by the newline. With CRLF the `\` is followed by `\r`, so EVERY
multi-line macro continuation breaks: the `#define` ends early, its body's following lines fall through
to the C parser as code, and you get a SILENT cascade of `parse error` (reported at the macro-DEFINITION
lines, not the call site) plus `stray '\' in program`. The `.c` mirror itself can be fine; the bad file
is the included header. Plain single-line `#define`s are unaffected, so the first error lands at the
first `\`-continuation macro in the header.

**Trigger:** after vendoring an SDK-sourced header, the build throws a run of `parse error before <tok>`
at the header's macro-definition lines (often the FIRST function-like macro) + `stray '\' in program`.
`file include/<lib>/<hdr>.h` reports `CRLF line terminators`.

**Procedure:** strip CR on copy — `sed -i 's/\r$//' include/<lib>/<hdr>.h` (or `dos2unix`). CRLF-clean
the header before relying on it. (Comments/whitespace are codegen-neutral, so this never affects the
ROM; it only un-breaks cpp.) This generalizes the `#upstream-mirror-pattern` CRLF note (which covered
nusys `.c` SOURCES) to vendored headers. S128: `include/libnualstl/nualstl.h` from nusys-2.05 had CRLF;
its `\`-continued seq-player macros cascaded into ~12 parse errors until stripped. The git-managed
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

**S103 partial-twin subset (`coddog-partial:<m>of<n>fn`).** coddog matches per-FUNCTION; when its
corpus splits a combined source into per-fn files (`mtxidentf.c`, `mtxl2f.c`) and a multi-fn subseg's
fns match ONLY SOME of them, the bare `coddog-mirror`/`coddog-twin` flags over-promise a clean
single-file mirror. `pick_target` flags `coddog-partial` when ≥2 DISTINCT per-fn twin files matched
a pack covering fewer fns than it holds (`len(cod_members) < nfns`): the multi-twin companion to
`coddog-fncount-mismatch` (which fires only at `len(distinct)==1`). The un-matched fns are NOT
verified upstream → per-fn verify before mirroring. **Provenance:** S103 (`func_800660A0`:
`mtxidentf.c`+`mtxl2f.c` @100 matched only `guMtxIdentF`/`guMtxL2F`; the combined `mtxutil.c`'s
`guMtxF2L` (Monegi clamp variant) + `guMtxIdent` (-O2 non-inline) diverged from every available
upstream, so the planned verbatim cp failed first build, banked classical via #game-region-mirror-o2-profile).

---

## game-region mirror (-O2 profile)

**Rule:** A libultra/SDK source can be statically linked into the GAME (not the libultra code
band): its subseg sits at a low rom/vram, and the build compiles it with the game `CFLAGS` (-O2),
NOT `LIBULTRA_CFLAGS` (-O3 via `$(subst -O2,-O3,…)`). The build selects the profile by src path
prefix (the `mk/lib*.mk` `C_PROFILE_CFLAGS` overrides): only `src/libultra/%` and `src/libkmc/%`
get the lib profiles; everything else is -O2. So a
game-embedded SDK mirror placed under `src/libultra/` compiles at the WRONG -O3 → `-finline-functions`
inlines its small callees (the ROM, at -O2, keeps the `jal`s). Symptom: a verbatim mirror where a
caller fn balloons (e.g. `guMtxIdent` 240 B vs ROM 60 B) because -O3 inlined `guMtxIdentF`/`guMtxF2L`.

**Trigger:** `pick_target.py` flag `game-region-mirror:0x<vram>`, fires when a row's `up_lib ==
"libultra"` and its rom is below the lowest-rom `libultra/` subseg (the libultra code band start).
Advisory (display-only).

**Procedure:**
- Mirror the file under a -O2 path, NOT `src/libultra/…`. The project convention is `src/mgu/`
  for the game-embedded ultralib gu/mgu matrix source (the Monegi variant); a `.clang-format`
  (`DisableFormat: true`) guards it as a verbatim-upstream dir (see CLAUDE.md). Any non-`libultra/`,
  non-`libkmc/` path gives -O2; pick one that reflects the source (`src/mgu/`, `src/main/…`).
- Includes: the game `-I` set lacks `src/libultra/gu` and `include/libultra/PR`, so a source-private
  companion (`guint.h`) won't resolve; include via the public path instead (`#include <ultra64.h>`
  pulls `<PR/gu.h>` → `FTOFIX32`/`FIX32TOF` + `<PR/gbi.h>` → `Mtx`).
- A divergent fn (a Monegi-modified variant absent from upstream) is then a normal classical
  reconstruction on the -O2 file (the verbatim siblings stay byte-exact); see the float-literal note in
  #mirror-cast-divergence-sign--vs-zero-extend for the single-precision FP gotcha.

**`game-embedded` sub-case — not standalone-carvable (S128).** A game-region mirror can be worse than
"compile at -O2": the upstream file's functions are compiled INTO a larger game TU, tight-packed (no
16-byte object padding) against game functions, so there is NO object boundary to carve a standalone
mirror at. The natural end of the matched block lands on a non-16 address with REAL game code (not
nops) immediately after; a standalone `[..,c,..]` subseg there SHA-misses because KMC `as` 16-pads the
`.o` `.text` and shifts the tail (#non16align — the gate-build canary: a correctly-named stub that
SHA-misses is alignment, not a bad offset). `pick_target.py` flags this as `game-embedded:0x<vram>`
(the synthesis of `game-region-mirror` + a coddog-subset signal — `coddog-fncount-mismatch` /
`coddog-structural` / `coddog-partial` — i.e. only SOME of the subseg's fns match the upstream).
- **Plan a MIXED carve, not a seed-only mirror.** Carve `[A, B)` where BOTH A and B are 16-aligned
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

---

## clean-rebuild-after-shared-header-edit

**Rule:** The build tracks no header dependencies: a `make` recompiles only the `.c` files whose
objects are missing or older than the source, not the consumers of a header you edited. So when a
mirror/enabler edits a *shared vendored header* (e.g. `include/libultra/PR/os_version.h`, included
transitively by many libultra TUs), the post-edit verification MUST be a clean rebuild, or a stale
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

---

## needs-define

**Rule:** A function guarded by a preprocessor define not active for its lib won't compile into the
build as written.

**Trigger:** `pick_target.py` flag `needs-define:<def>`.

**Procedure:** Confirm the gating define against the upstream. If it is not in the lib's active
define set, the leaf is not a clean flip: defer, or handle the define at the gate.

### GBI-microcode define (S83: value-guarded macro, not a whole-body gate)

A subtler sub-case: not a `#ifdef DEFINE` wrapping the whole function, but an object-like macro
whose VALUE is GBI-microcode-guarded, used inside an otherwise-verbatim body. `PR/sptask.h`:

```c
#if (defined(F3DEX_GBI)||defined(F3DLP_GBI)||defined(F3DEX_GBI_2))
#define OS_YIELD_DATA_SIZE 0xc00
#else
#define OS_YIELD_DATA_SIZE 0x900
#endif
```

ultralib builds `libgultra_rom` with a global default `GBIDEFINE := -DF3DEX_GBI`. MG64 runs the
F3DEX2 microcode, so the project pins `-DF3DEX_GBI_2` in `LIBULTRA_CFLAGS` (`mk/libultra.mk`). With NO
GBI define the macro silently takes the `#else` value. The standing pin resolves this for every
libultra mirror; it is documented here because the failure mode is invisible to every gate check.

**The tell (S83 `sptask.c`):** the mirror compiles and LINKS clean (all symbols resolve), but the
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

### Version-rev `#define` VALUE divergence (S122: single `li`/`addiu` immediate byte)

The same link-clean / one-word-SHA-miss class as the S83 GBI sub-case, but the gating is NOT a build
flag: it is the vendored upstream version diverging from the GAME's library rev on a plain object-like
`#define` value. S122 `nusimgr.c` (libnusys): a byte-verbatim drop-static mirror SHA-missed by exactly
ONE byte, vram 0x800A27E0 `li a1,6` (build) vs `5` (ROM) — the `osCreateThread` thread-id arg. Root
cause: vendored nusys-2.07 `include/libnusys/nusys.h` has `NU_CONT_THREAD_ID 6`, but MG64's nusys rev
compacts the controller/SI thread to slot 5 (the IDs run idle=1/rmon=2/main=3/gfx=4/[audio-gap=5]/
cont=6 in 2.07; MG64 drops the audio slot so cont=5).

**The tell vs a reloc:** the lone differing word is a `li`/`addiu` whose immediate is a small literal
(an enum/macro VALUE), NOT a `lui`+`addiu`/`lw` hi/lo PAIR carrying a relocated address. An immediate
mismatch is a `#define`/enum value; a hi/lo address mismatch is a symbol-placement problem. Localize
with the S44 `.o`-diff byte form (venv-python region cmp of `baserom.z64` vs `build/mariogolf64.z64`
at the ROM offset) → the single off-by-constant immediate → grep the macro back through the header.

**Fix + blast radius:** correct the vendored header to the game's value, then **grep `src/` +
`include/` for every consumer of the macro before editing** — if the candidate is the sole consumer
(S122: `nusimgr.c` was the only `NU_CONT_THREAD_ID`/`NU_SI_THREAD_ID` user in the tree), the change is
collateral-free; otherwise a co-consumer that was matched at the OLD value will break. The build tracks
no header deps, so bank from a `make clean && make extract && make` (the shared-header-edit rule), not
an incremental build.

### VERSION_K-gated statement present in MG64's J build (S85: exact N×8B SHA-miss)

The ultralib reconstruction's `#if BUILD_VERSION >= VERSION_K` gates are sometimes too aggressive
for MG64's actual VERSION_J build: a call/statement the source gates K-only is in fact present in the
J ROM. `pick_target.py`'s `_strip_inactive_version_branches` strips `>= VERSION_K` under J, so the
mirror compiles short of the asm by whole instructions.

**S85 `initialize.c`, two instances in one file (verify each independently):**
- `__osSetWatchLo(0x4900000)` was gated `#if BUILD_VERSION >= VERSION_K`; un-gating it to
  `>= VERSION_J` restored the missing `jal __osSetWatchLo; lui a0,0x490`, the EXACT 8-byte/2-instr
  miss.
- `createSpeedParam`'s body, by contrast, has a `#elif BUILD_VERSION == VERSION_J` branch
  (reconstruction lines 210-224) so it DID compile under J as-is. Check the J branch exists before
  assuming a K-gate needs un-gating; not every K-gated thing is missing under J.

**The tell:** the mirror compiles + LINKS clean, but the full-make ROM SHA-1 misses by an exact
multiple of 8 bytes (whole instructions) localized to ONE contiguous region, distinct from the
GBI/enum off-by-constant *immediate* class above. Cross-check the `.o` function size against the asm:
the next symbol's `nm` offset vs the asm `nonmatching <fn>, 0x<size>` comment (S85
`__osInitialize_common` compiled `0x228` vs baserom `0x230`).

**Localize:** disassemble the `.o` (`mips-linux-gnu-objdump -d build/<path>.o`) and the baserom fn
(Ghidra MCP `disassemble_function`), align; the divergence is a contiguous run of asm-only
instructions matching a `>= VERSION_K`-gated statement in the upstream (the 8-byte shift then
propagates cleanly through the rest).

**Fix:** change ONLY that one gate `>= VERSION_K` → `>= VERSION_J` in the mirrored copy (a
near-verbatim version-gate edit, same class as #char-signedness / #assert-strip; confirm the gated
callee is a placed symbol). NOT a blanket un-gate of every K-block.

---

## header-renames-symbol (vendored header rewrites the curated symbol)

**Rule:** A (transitively-)vendored libultra header can rewrite the candidate's curated function name
via a source-compat macro. `os_host.h`: `#define __osInitialize_common() osInitialize()` (the K-era
worker name → the J public name). When the mirror body defines `void __osInitialize_common() {...}`,
the function-like macro fires and the object exports `osInitialize` instead, leaving the curated symbol
the entry stub / callers reference (`__osInitialize_common`) undefined at link. S31 `nuGfxInit`
(nusys.h) was the 1st instance, S85 `initialize.c` the 2nd: a real class.

**Trigger:** `pick_target.py` flag `header-renames-symbol:<fn>@<header>` (S85; scans the candidate's
transitively-included resolvable headers for `#define <curated_leader>...`), OR a link
`undefined reference to <curated_fn>` where the `.o` `nm` shows the function exported under a
DIFFERENT name. Invisible to the gate stub build: the macro bites only a real function definition,
which an `INCLUDE_ASM` stub never has (same late-surface class as #needs-define).

**Procedure:** Add `#undef <curated_fn>` to the mirrored `.c` AFTER the `#include`s (so the
transitive header's macro is undone) and before the function definition; then the
name-macro/`INITIALIZE_FUNC` reconcile exports the curated symbol. SHA-neutral (a symbol-name change,
not a byte change). Same one-line fix as the S31 `#undef nuGfxInit`.

**Caveats:**

**NOT this hazard (the S102 contrast):** if the upstream does NOT define a function named
`<curated_fn>` (the curated name is a macro ALIAS for a DIFFERENT symbol the upstream defines, namely
the macro's RHS), then the body never contains the `<curated_fn>` token, no `#undef` is needed, and the
real issue is a mislabeled ghidra name → see `#wrong-ghidra-name-override`. `pick_target.py` now
suppresses `header-renames-symbol` in that case (`_upstream_defines_function` gate) and emits
`wrong-ghidra-name` instead.

---

## wrong-ghidra-name-override (correct a mislabeled symbol without sync-names)

**Rule:** `ghidra_symbols.txt` can name a function vram with the WRONG symbol: a macro ALIAS rather
than the real function. S102 motor.c: ghidra labels 0x800AE380 `osMotorStop`, but os_motor.h
`#define osMotorStop(x) __osMotorAccess((x), MOTOR_STOP)` makes that a *macro*; the function the
VERSION_J build defines at 0x800AE380 is `__osMotorAccess` (verified in `build/J/libgultra_rom/motor.o`:
`T __osMotorAccess`@0, no `osMotorStop` symbol). A verbatim mirror names the body `__osMotorAccess`
(correct), so the still-asm callers' relocs (`jal osMotorStop`) and the C object (`__osMotorAccess`)
disagree → `undefined reference`.

**Why not `make sync-names`:** the canonical fix (rename in Ghidra → sync) is a FULL
`--export-to-decomp --write-in-place` regen, still destructive (~250-symbol Ghidra↔decomp drift,
S20/S87). And you can't hand-edit `ghidra_symbols.txt` (sync-owned) nor naively add the correct name
to `symbol_addrs.txt` (same vram already in ghidra_symbols → splat dup error).

**Trigger:** `pick_target.py` flag `wrong-ghidra-name:<ghidra_name>-><correct_name>@<header>` (S102)
fires when a header macro `#define <ghidra_name>(...)` exists, the version-stripped upstream does NOT
define a function named `<ghidra_name>`, and the macro's RHS leading symbol IS defined in the upstream.
It is the distinguishing companion of `#header-renames-symbol` (which fires when the body DOES define
the macro name → a real `#undef`).

**The override MECHANISM is not limited to macro aliases (S128).** `pick_target.py` only auto-flags the
macro-alias case, but the `rom:`-qualifier override below corrects ANY wrong `ghidra_symbols.txt` name a
coddog/upstream cross-ref proves wrong — a plain bad GUESS, not just a macro. S128 audio_mgr.c:
`audio_config_init`@0x8005EDD0 was really `nuAuStlMgrInit`, and the libmus callees `mus_initialize`@
0x8009973C / `mus_play_song_ptr`@0x800999F0 were really `MusInitialize` / `MusStartSong` (coddog
libmus_map confirmed); the RSP-boot sprint likewise corrected `audio_sched_thread_entry`. In each the
ghidra name is a non-canonical guess, the upstream name is canonical, and the override resolves both the
mirror body and every still-asm caller to the correct name. Treat a coddog/upstream-confirmed wrong name
the same as a macro alias even though no `pick_target.py` flag fires.

**Procedure (the non-destructive override):**
1. Add a `symbol_addrs.txt` maintainer-override for the CORRECT name with a `rom:` qualifier:
   `<correct_name> = 0x<vram>; // rom:0x<off> type:func`. The `rom:` (or `segment:`) qualifier is
   load-bearing: splat's dup-symbol error (`util/symbols.py`, the `have_same_rom_addresses and
   same_segment` test) fires ONLY when a same-vram symbol shares BOTH rom AND segment; a bare entry has
   rom=None (== the ghidra entry's None) + same segment → clash. The qualifier makes
   `have_same_rom_addresses` False → no clash.
2. `symbol_addrs.txt` is loaded FIRST (splat `initialize`, before `ghidra_symbols.txt`), so the
   override WINS the reference: both the scaffolded `INCLUDE_ASM` stub and the still-asm callers' relocs
   resolve to `<correct_name>`. (The link also runs `--allow-multiple-definition` as a safety net.)
3. Name the mirror body `<correct_name>` (verbatim, since it is the real upstream name). NO `#undef` needed:
   the body never contains the macro-name token, so the alias macro is inert (this is why
   `#header-renames-symbol` does NOT apply: the curated name IS the macro's RHS).
4. The stale `ghidra_symbols.txt` entry + the override coexist deliberately; the override wins.
   **Cross-repo follow-up:** rename the vram in the Ghidra workspace to `<correct_name>` so the
   source-of-truth matches; a future reconciled sync can drop the override.

**Verify at the gate:** after the flip + override, `make extract`, then confirm (a) the scaffolded
`src/.../<file>.c` stub is `INCLUDE_ASM(..., <correct_name>)` and (b) the still-asm caller's
`asm/<seg>.s` relocs `jal <correct_name>`; both prove the override won before you write the body.

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
per-function stub in `asm/nonmatchings/...` but does NOT update the stale top-level `asm/<seg>.s`
(no longer regenerated once the subseg is c-flipped). `decomp_loop.py` uses the top-level file, so a
stale label breaks loop resolution or masks the real score with a reloc-name mismatch.

**Trigger:** Loop reports "no `glabel <newname>` found", or asm-differ penalises every load/store to
a renamed global.

**Procedure:**
- **Function rename** (e.g. `__osPiRelAcces` → `__osPiRelAccess`): update the old label(s) in the
  stale `asm/<seg>.s` (3 occurrences: `nonmatching`, `glabel`, `endlabel`), then rebuild the
  reference object (`make build/asm/<seg>.o`).
- **Data global add** (e.g. `hdwrBugFlag = 0x800C7EC0`): replace all `D_<ADDR>` occurrences in
  `asm/<seg>.s` with the new name, then rebuild (S34: `asm/7EFE0.s`, 3× `D_800C7EC0` → `hdwrBugFlag`).

---

## caller-evict

**Rule:** Adding a curated name for an un-named `func_<vram>` to `symbol_addrs.txt` (a common gate
enabler when naming a coddog / upstream-mirror leader) makes `make extract` rename that symbol in the
scaffold. Any ALREADY-BANKED C file that hard-codes the old `func_<vram>` name (an `extern` decl +
call) then fails to link with `undefined reference to func_<vram>`. Same class as
`#stale-top-level-asm-label-sync`, but reaching the gate via a symbol ADD rather than `make sync-names`.

**Pre-flag:** `pick_target.py` flags `caller-evict:<func_vram>@<file>[;…]` (S77); it walks `src/`
for every un-named member a banked C file references by name (INCLUDE_ASM stub lines excluded). When
the flip will name that `func_`, the listed caller's call site must be renamed in the same flip.
Display-only (does not change `pts`); the fixup is one line and SHA-neutral (same address).

**Trigger:** the gate's green-ROM `make extract && make` fails with `undefined reference to
func_<vram>` from a banked C object after a `symbol_addrs.txt` add.

**Procedure:** rename the call site(s) in the flagged banked C file from `func_<vram>` to the curated
name (both the `extern` declaration and the call). The codegen is identical (same `jal` target), so
the ROM SHA-1 is unchanged. S77: adding `__osSpGetStatus`=0x800B16A0 evicted `src/main/func_800AB600.c`
(`extern u32 func_800B16A0(void)` + `func_800B16A0()` → `__osSpGetStatus`).

**Sub-cases / variants:**

**Companion case A: multi-global mirror flip evicts STILL-ASM callers (S106).** The dual direction:
flipping a mirror whose source defines ≥2 GLOBAL functions makes the C object export those globals
under their real names (osCreateScheduler, osScAddClient, …). Any STILL-ASM file that called them by
the old `func_<vram>` name then link-fails (`undefined reference to func_<vram>`): the asm reloc
points at a name nothing defines. `pick_target`'s `caller-evict` only walks banked C callers, so an
asm caller is invisible at the gate; it surfaces as the execution-time link error. Fix: add each
externally-referenced global's curated name to `symbol_addrs.txt`; `make extract` re-extracts the asm
caller with the new name and it resolves against the mirror's def. S106 sched.c: still-asm mus_dma
(`asm/78D10.s`) called `func_800AB798`/`func_800AB880` → named `osScAddClient`=0x800AB798 +
`osScGetCmdQ`=0x800AB880 (the file's other globals, osScRemoveClient/__scTaskReady, had no external
caller → no add needed). Name only the globals the link error names.

**Companion case B: a mirror's recover-callee IS an already-banked `func_` (S106).** A new mirror
calls a libultra fn by name (`osSpTaskYielded`); the recover resolves not to still-asm but to a
function ALREADY banked classically under its `func_<vram>` placeholder (`func_800AB600`, banked S11
as a "main" leaf: it was the un-named `osSpTaskYielded` all along, revealed by the mirror's call).
Fix: RENAME the banked func_ to the libultra name (symbol_addrs add + the C body's function name) AND
match the header signature (`OSYieldResult osSpTaskYielded(OSTask *tp)` from sptask.h). Critically,
keep the VERIFIED classical body, not the upstream-verbatim form: the S11 match used
`bit = (status>>8)&1` and the upstream uses `(status & SP_STATUS_YIELDED) ? OS_TASK_YIELDED : 0`,
which can codegen differently (`srl;andi` vs `andi;sltu`) at the game `-O2` profile. Do NOT relocate
the file into `src/libultra/` (would force the `-O3` band → possible divergence; see
`#game-region-mirror-o2-profile`); leave it where it banked (the file name stays `func_<vram>.c`, a
cosmetic mismatch, optional follow-up rename).

---

## Display lists

**Rule:** F3DEX2 microcode. Include `<PR/gbi.h>` when the target manipulates `Gfx*`.

**Procedure:** For static dlists in rodata, run `~/development/repos/n64-tools/src/gfxdis-rom/gfxdis`
(build once with `make -C ~/development/repos/n64-tools`). `gfxdis` only handles static dlists;
dynamic builders are hand-decompiled.

---

## data-rodata-carve

**Trigger:** resolving the anonymous `.data`/`.rodata` blocks in a whole library region into named
`.data`/`.rodata, libultra/<tu>` subsegs (S111 swept the libultra `.data` block 0xA32D0–0xA5668).

**Rule:** A libultra `.data` region is a contiguous link-order run; each TU is carved independently by
splitting the anonymous `[0xXXXX, data]` around its `[start, end)`.

**Procedure:**

**Attribution oracle.** `make extract` (splat) prints `Rodata segment 'X' may belong to the text
segment 'Y'` (splat `rodata.py`) for each anonymous rodata referenced by a single function; capture
stderr. `.data` blocks get NO such hint: attribute by grepping the **defining** upstream file in
`~/development/repos/ultralib/src` for the symbol (`OSThread *__osRunQueue;`, not just an `extern`),
then confirm placed-status in the yaml. `../drmario64/lib/ultralib/src` is the cross-ref for TUs MG64
hasn't placed; a block drmario64 also leaves generic (`RO_<vram>`) is genuinely unattributable, so
leave it a blob.

**Three carve kinds:**

1. **Placed drop-def restore.** The TU is placed but mirrored "drop-def" (its data DEF was turned
   into an `extern`). Restore the upstream initializer in declaration = ROM-address order
   (`extern T x;` → `T x = <upstream init>;`), confirm bytes vs `asm/data/<blk>.data.s`. Function
   statics (`dtor`, `nintendo[]`, `xseed`) go back as `static` (file-scope static byte-matches the
   function-local form). `symbol_addrs.txt` is add-only, so leave the extern; the restored def coexists
   via `--allow-multiple-definition` (same address).

2. **Not-placed data TU vendor.** Pure-data upstream TUs (`io/vitbl.c` osViModeTable, `vimodes/*.c`)
   are copied verbatim (`PRinternal/viint.h`→`viint.h` rewrite). Code+data TUs whose `.text` is still
   asm (`os/thread.c`, `io/vi.c`) get a data-only file emitting just the globals (empty `.text`,
   no text subseg); see the two GOTCHAS below.

3. **asm-mirror un-strip.** An S107 stripped-jtbl `hasm` keeps its tables as blobs only while their
   target labels aren't exported. Once the vendored `.text` re-exports the jtbl-target `.L<addr>`
   labels, append the tables back to the `.s` as `.section .rodata`/`.section .data` and carve; the
   `.word .L<addr>` resolves locally (S111 exceptasm: `__osIntOffTable`+`__osIntTable` rodata,
   `__osHwIntTable`+`__osPiIntTable` data). A stripped-jtbl hasm's tables are CARVE-ABLE, not
   permanent blobs (corrects the S107 "must stay anon" framing).

**The four SHA/link gotchas (S111, all were masked by an ungated `sha1sum`; use
`tools/verify-rom.sh`):**

- **0x10 `.data`-size-pad boundary.** GCC pads each `.o`'s `.data` *size* up to 0x10. The carve's
  next-subseg boundary is the 0x10-aligned end of the TU's data, NOT the last symbol's end: the
  "Automatically generated and unreferenced pad" `D_<vram>` syms ARE that trailing pad (initialize:
  0x14 data → next subseg at +0x20). Wrong boundary = exact-N×byte SHA miss.
- **reloc-to-bss needs placement.** A restored pointer init to an uninitialized `.bss` symbol
  (`OSTimer* __osTimerList = &__osBaseTimer;`) is a `.data` reloc; if the bss target isn't a placed
  symbol the link fails `undefined reference`. Name it in `symbol_addrs.txt` at its baserom bss vram
  (read from the ROM pointer value); do NOT `define` it in the `.c` (that perturbs bss layout and the
  pointer value won't match).
- **cross-TU-split data needs non-`static`.** Upstream keeps file data `static` because the function
  using it is in the same `.c`. When MG64 has split that function into its own TU (`io/viinit.c`'s
  `__osViInit` does `extern __OSViContext vi[2]`), the data must be **global** (drop `static`) so the
  split TU links. Bytes are identical; only visibility changes.
- **verbatim mirror `needs-define`.** A verbatim data TU can use a header macro the project's vendored
  header lacks (`vitbl.c` → `VI_CTRL_ANTIALIAS_MODE_0`, absent from the MG64 `rcp.h` which had only
  `_1/_2/_3`). Add the missing define from the `~/development/repos/ultralib` authority; a shared-header
  edit then needs a clean rebuild (`#clean-rebuild-after-shared-header-edit`).

## cross-jump-tail-merge

**Trigger:** A verbatim/near-verbatim mirror builds + links clean but ROM SHA-misses, and the
in-tree-vs-target objdump shows the build is **SHORTER than the target** (instruction COUNT < target),
with collateral `-0x10`-style address shifts on every symbol AFTER the short function (their CODE
matches; only referenced addresses shifted).

**Rule:**

**RULE OUT A GAME-MODIFIED BODY FIRST (S127, the contRmbControl reversal).** This symptom is NOT proof
of a compiler cross-jump merge; it is far more often a **body-semantics divergence** the literal
upstream lacks. S121 declared `contRmbControl` an "exhaustively-proven-unbankable cross-jump wall" and
carried it 5 sprints (+ a 145k-iter permuter run); S127 banked it byte-exact (full ROM SHA-1) with a
**one-branch body fix and NO compiler change**. The real cause: MG64's FORCESTOP case is game-modified
— on `osMotorInit` FAILURE it sets `state = STOPPED`, on SUCCESS `state = STOPPING; counter = 2` (an
`if/else`), where the nusys/papermario upstream sets `state = STOPPING` UNCONDITIONALLY.
- **The tell:** the target's FORCESTOP epilogue has TWO `sb v0,6(s0)` state stores with DIFFERENT
  values (`li v0,1` in one branch; `li v0,2` from the `bnez` delay slot in the other). A single
  unconditional `state =` cannot emit two differing state stores → the body has a branch the upstream
  lacks. **Read the target's store SEQUENCE and VALUES, not just the control-flow shape.**
- **Rule:** a "shorter + collateral-shift" mirror miss is a **body bug until proven a compiler
  artifact**. A store your upstream body cannot produce (extra branch, different constant, doubled
  field) IS the fix. Same class as `#struct-init-loop` (a dup-store body artifact, also "count-short").

- **Tell from block-reorder (`#near-verbatim-mirror-jal-count-mismatch`).** cross-jump = build instr
  COUNT **< target** (a merge deleted instrs). block-reorder = **equal** count, same insns reordered.
  Measure the count first.
- **The actual cross-jump pass (why a BODY fix moves it).** `jump.c:find_cross_jump` can merge two
  identical `j .L_ret; sh <reg>,<off>(<base>)` tails via the `minimum=1` "cross-jump to code before the
  label" path (jump.c ~1978): **one** matching insn before the shared epilogue label suffices
  (`--minimum` → `minimum <= 0` after a single match). So the merge is driven by basic-block **LAYOUT**
  — which store lands immediately before the epilogue label — and the layout is driven by the BODY.
  Change the body → change the layout → the "merge" moves or vanishes. The pass is always-on
  (`toplev.c:3142` `jump_optimize(insns, 1, 1, 0)`, gated only by `optimize > 0`; no disabling flag —
  `-fno-thread-jumps` gates the SEPARATE `thread_jumps` pass), which is exactly why its layout effects
  get misread as an irreducible compiler wall.

**No compiler cross-jump WALL is confirmed in this project.** The sole candidate (S121 contRmbControl)
was a body bug. If you ever reach byte-identical-unmerged tails with a body PROVEN to emit the target's
exact stores + values, only then suspect a compiler-build divergence — and re-audit the body once more
first. A genuinely stuck mirror fn then falls back to: partial-bank the matching siblings as C + carry
the stuck fn as `INCLUDE_ASM` (ROM stays green; forward-decl `extern` since the asm `glabel` is
`.globl`), or `hasm`-split it. A `jump.c` patch is NOT a banking path (open toolchain research only).

## struct-init-loop (dup-store / dual-induction-var)

**Trigger:** A near-verbatim mirror's **array-of-struct init loop** (`for(i){ arr[i].next = &arr[i+1];
arr[i].f = …; }` over a large struct) builds + links clean but ROM SHA-misses, the build is **SHORTER
than the target** (instr COUNT < target, ~2 instrs short, with the usual `-0x10`-style collateral
address shifts on everything after it), and the in-tree-vs-target objdump shows the target storing
**one field TWICE** (the same value to the same offset at the loop body's HEAD and TAIL) via a
**running address pointer**, while your build stores it once via indexed `lui %hi(base); addu …,v1`.

**Rule:** This is NOT a cross-jump wall (`#cross-jump-tail-merge`, which DELETES instrs) and NOT
permuter territory (the byte-match is far below 0.97) — it is a **source artifact**: the original loop
body has a **duplicate field assignment** the literal upstream lacks.

**Procedure:**

- **Mechanism.** Taking an address INTO the array (`next = &arr[i+1]`) makes gcc 2.7.2 strength-reduce
  that to a running induction var (e.g. `a1 = &arr[i+1]`, stride = sizeof). A SECOND assignment of a
  field whose address is a fixed offset below the next element (`arr[i].msgQ` at `next_elem - 8`) lets
  gcc derive a SECOND running pointer (`a0 = a1 - 8`) and store that field via `0(a0)` at both the
  head and tail of the body — the **double-store tell**. Your single-assignment source gives gcc one
  IV (it picks one field for the running pointer, indexes the rest), so it is 1–2 instrs short and the
  registers/addressing cascade-diverge.
- **Fix.** Re-add the duplicate field assignment at the loop **tail** (the MG64 edit artifact: S124
  `nuGfxTaskMgrInit` has a trailing `nuGfxTask[cnt].msgQ = &nuGfxTaskMgrMesgQ;` after the field block).
  That single line reproduces gcc's dual-IV + double-store and lands the exact instr count → byte-match
  first build after. Match driver: the asm STORE ORDER (`next, msgQ, …, msgQ`) — a field that appears
  at both ends of the store sequence is your duplicate-assignment cue.
- **Watch the value defines too.** S124 also needed `yield_data_size = OS_YIELD_DATA_SIZE` (0xC00), NOT
  the 2.07 `NU_GFX_YIELD_BUF_SIZE` (= `OS_YIELD_DATA_SIZE + 0x10` = 0xC10) — the MG64 rev dropped the
  `+0x10` (a `#needs-define` version value). Size the extern array (`extern T arr[N]`, not `arr[]`) so
  gcc has the bound. The asm is authoritative for the store ORDER and the immediate values; the
  upstream `.c` is the SHAPE only.

(A `pick_target.py` `struct-init-loop` detection tag — a multi-store loop whose asm has a doubled
field-store offset — is a tracked follow-up; until then this is recognized by the build-shorter +
doubled-store-offset tell at match time.)

## permuter setup for KMC-toolchain mirrors

**Rule:** Running decomp-permuter on a `libnusys`/`libultra`/`libkmc` (KMC-toolchain) function needs
three fixes the generic setup misses (hit S121 contRmbControl).

**Procedure:**

- **(a) custom `--settings`.** The root `permuter_settings.toml` `compiler_command` is GENERIC (`-I
  include` only, no `-DUSE_EPI` / per-library include paths), so `import.py` preprocessing fails on
  `#include <nusys.h>`. Pass `--settings <custom>.toml` whose `compiler_command` carries the file's real
  CFLAGS (the per-library `-I include/libnusys …` + `-D` defines), piped through `tools/cc/gcc -S` to
  `tools/cc/as`.
- **(b) KMC-safe `asm_prelude_file`.** decomp-permuter's default `prelude.inc` has `.set gp=64`, which
  KMC binutils-2.6 `tools/cc/as` REJECTS (`Expected comma after name gp`). Supply an `asm_prelude_file`
  that drops that line (harmless at `-G 0`).
- **(c) body + extracted target.** `import.py` accepts a `c_file` with the function BODY plus a target
  `.s` extracted from `asm/<seg>.s` (`glabel`..`endlabel`) — no `INCLUDE_ASM` / `mg_resolve_c_asm`
  round-trip is needed.

## NU_DEBUG-stock-not-custom (carried perf fn triage)

**Rule:** A `libnusys` carry framed as "heavily game-customized, classical RE" is often NOT custom at
all — it is the **stock upstream body that failed to match because the file was compiled WITHOUT
`NU_DEBUG`**, so the `#ifdef NU_DEBUG` performance machinery is absent from the C while the asm has it
(osGetTime / osDpSetStatus / __udivdi3 calls).

**Trigger:** The tell: **the carried fns are EXACTLY the upstream fns that carry `#ifdef NU_DEBUG`
blocks** (S123 carried nusched's 4 NU_DEBUG fns — nuScCreateScheduler / nuScEventHandler /
nuScExecuteAudio / nuScExecuteGraphics — as "custom"; S125 found nuScExecuteAudio is a pure
stock-NU_DEBUG mirror).

**Procedure:**

- **Triage before classical-RE.** Diff the carried fn's asm against the upstream's **NU_DEBUG** body
  (not the non-debug body). If the calls line up, it is stock: bank it as a mirror.
- **Fix recipe (S125 nuScExecuteAudio, banked first build).** (a) `#define NU_DEBUG` at the top of the
  `.c` BEFORE `#include <nusys.h>` (matches the MG64 TU compile); (b) validate the perf/debug struct
  offsets vs the asm and fix the vendored header (see the perf-struct note in `#upstream-mirror-pattern`
  — MG64's `NUDebTaskPerf` lacked the 2.07 `markerTime[10]`); (c) drop-def the `static` perf pointers
  (`debTaskPerfPtr` etc.) to extern at their asm-recovered vrams. The stock body is then verbatim
  (drop-static).
- **Caveat.** NU_DEBUG-on covers ONLY the carried perf fns; the already-banked stock siblings have no
  NU_DEBUG blocks so the file-wide `#define` does not perturb them. A fn with NU_DEBUG-stock perf code
  PLUS genuine MG64 edits (swap-gate, game hooks) is still classical on top of the NU_DEBUG stock
  skeleton (S125 nuScEventHandler / ExecuteGraphics).

## libnusys inline-div mflo-hazard nop

**Trigger:** A `libnusys` fn with an **inline integer division** (`a / b` compiled to `divu` + `mflo`,
not a `__udivdi3` call) can build + link clean and be **byte-perfect EXCEPT for 2 missing `nop`s after
`mflo`** (the VR4300 mflo→consumer hazard padding) — a `mflo; nop; nop; <use>` in the target vs
`mflo; <use>` in your build. Net: the build is 2 instrs short with the usual collateral address shifts.

**Procedure:**

- **Root cause (S125 nuScEventHandler).** KMC gcc emits the GNU `div` **macro**; the assembler
  (`tools/cc/as`) expands it and inserts the hazard nops — but ONLY when it can see the consumer is too
  close. When gcc -O2 schedules the mflo consumer **into a loop-back `j`'s delay slot** (`mflo; j;
  addiu` — consumer 2 slots after mflo across the branch), `as` does NOT pad, while the original build
  emitted `mflo; nop; nop; j; addiu` (consumer 3 slots after). A standalone `int d(int a,int b){return
  a/b - 3;}` DOES get the 2 nops by default, so it is **not a blanket missing flag** — it is the
  loop-back-delay-slot scheduling that suppresses the pad.
- **Flags do not help.** KMC gcc/as REJECT `-mfix4300`, `-mcpu=vr4300`, `-mtune=vr4300`,
  `-mfix-vr4300`, `-Wa,-mfix-vr4300` (modern-gcc/binutils flags absent in the KMC 2.7.2 / binutils-2.6
  toolchain).
- **NOT a permuter wall (S126 overturns S125).** S125 framed this as a permuter candidate. S126 banked
  nuScEventHandler with NO permuter: the 2 nops appear **naturally** once the function's data + volatility
  scaffold is COMPLETE — specifically `nuScRetraceCounter` as a proper **per-TU volatile** (see
  `#volatile-global tell`) AND `nuDebTaskPerf` placed in `symbol_addrs`. The S125 "missing nops" was a
  **scaffold artifact** (an incomplete/header-flipped attempt scheduled the consumer differently), not a
  KMC-as scheduling wall. **Lesson: complete the per-TU-volatile + data scaffold and RE-DIFF before
  concluding a mflo-hazard near-miss needs the permuter.** Most "byte-perfect except 2 nops" libnusys
  near-misses are an unfinished scaffold, not an assembler interaction.
- **Resolution ladder.** (a) **Finish the scaffold first** — per-TU volatile globals + every referenced
  data symbol placed, then re-diff; the nops usually resolve (S126). (b) Only if a complete-scaffold build
  still SHA-misses by exactly the 2 nops is it a genuine scheduler/assembler wall → **permuter** (a source
  form that keeps the mflo consumer OUT of the `j` delay slot, so it lands ≥3 slots after mflo, lets `as`
  insert the nops; `#permuter-setup-for-kmc-toolchain-mirrors`). Do NOT treat the 2-nop gap as a "try
  harder C" iteration — it is a scheduler/assembler interaction, not a logic mismatch.

## volatile-global tell (dead-reload + recompute-not-CSE)

**Trigger:** A `libnusys`/game mirror global that the asm accesses with **a dead reload immediately
after a store** (`lw t,X; addiu t,t,1; sw t,X; lw t,X` where the 4th load's value is discarded) and/or
is **re-read fresh on every reference instead of CSE'd** (e.g. `a - b` recomputed at each `==` compare
rather than computed once) is **`volatile`** in the original.

**Rule:** Declare it `vu32`/`vs32` (the ultra64 volatile types), not `u32`/`s32`.

**Procedure:**

- **Tells.** (1) The dead reload-after-store is the **`volatile x++`** signature (gcc 2.7.2 re-reads
  the volatile lvalue). (2) The per-read recompute (no common-subexpression elimination of `a - b`)
  means BOTH operands are volatile, so each read is a fresh load. S125 nuScEventHandler:
  `nuScRetraceCounter`, `D_801B68E0`, `D_800B678C`, `D_8012F4D4` are all volatile.
- **Consequence for source form.** Because volatile blocks CSE, a sub-expression reused across several
  comparisons (`(nuScRetraceCounter - D_8012F4D4)` against `0x20` and `0x36`) must be captured in a
  **single non-volatile local** (`s32 frame = a - b;`) so gcc computes it once and reuses it — matching
  the target's single `subu` + two compares. Inlining the volatile expression at each compare instead
  emits a reload+recompute per site AND lets gcc canonicalize `(a-b)==c` to `(a-c)==b` (wrong codegen).
  Also place that local's computation in program order relative to the other volatile reads so the
  scheduler reproduces the target load order (S125: compute `frame` inside the `>=0x1F` block, before
  the non-volatile pointer test, so the volatile reloads precede the pointer load).
- **Shared-header caution.** Flipping a header-declared global to `vu32` (e.g. `nuScRetraceCounter` in
  `nusys.h`) changes codegen for EVERY consumer — re-verify the already-banked consumers on a clean-rebuild
  SHA check (no header-dep tracking, so an incremental build hides the breakage; `#clean-rebuild-after-shared-header-edit`).
- **Per-TU volatile (S126) — the global is volatile in ONE TU, plain in others.** A global can read
  `volatile` in the scheduler TU (dead-reload tell) yet `u32` in a sibling TU that is already BANKED
  non-volatile. The original compiled each TU against its own declaration. Replicating it: **keep the
  shared header non-volatile** (so the banked consumers stay matching) and add a **localized
  `extern volatile u32 <g>;` redeclaration in the volatile TU only** (after the header include). GCC 2.7.2
  accepts the qualifier-mismatch redeclaration with a benign `warning: type mismatch with previous
  external decl` and emits per-access `lui`/`lo` absolute volatile addressing — matching the reference.
  (S126 nuScEventHandler: `nuScRetraceCounter` is volatile in `nusched.c`, plain `u32` in `nugfxtaskmgr.c`
  + `nucontrmbmgr.c`.)
- **Do NOT use a cast macro for per-TU volatile.** `#define g (*(vu32 *)&g)` does NOT reproduce the
  codegen: GCC computes `&g` once into a held register and accesses `0(reg)` (and burns an extra saved
  reg in the prologue), instead of the reference's per-access `lui %hi(g); lw lo(g)` absolute addressing.
  Use the redeclaration, not the cast.
- **Diagnosis when a shared-header volatile flip SHA-misses.** A single-read **modulo/division**
  (`x % n`, `x / n`) IS volatile-sensitive (it changes scheduling/CSE) — S126 `nucontrmbmgr.c`'s lone
  `nuScRetraceCounter % nuContRmbSearchTime` grew `.text` +0x10 under the flip, cascading
  `main_RODATA_END`/BSS by +0x10 into a ~21M-byte ROM diff; a plain reload-pair read is volatile-NEUTRAL
  (S126 `nugfxtaskmgr.c`). To find the grower fast, **map-diff the per-object section sizes**:
  `diff <(grep '\.text.*0x' base.map) <(grep '\.text.*0x' edits.map)` after a clean build of each — the
  one object whose `.text`/`.rodata` size changed is the volatile-sensitive consumer.
