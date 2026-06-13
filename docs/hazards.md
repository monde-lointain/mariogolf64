# Hazard playbooks (on-demand reference)

This is the detail layer for the MG64 execution loop. `CLAUDE.md` holds the always-loaded core
and a hazard index; read the matching section here when `tools/pick_target.py` flags a hazard or a
match behaves as one of the symptoms below. Each entry is **Rule / Trigger / Procedure /
Provenance**. The S-numbers are provenance tags pointing at the sprint that established the rule.

The deterministic-vram recovery and the byte-`cmp` rules are load-bearing: follow the procedures
exactly rather than re-deriving them.

---

## Upstream-mirror pattern

**Rule:** When a target maps to a known SDK upstream, copy the upstream `.c` verbatim instead of
hand-decompiling. The proof is the full-`make` ROM SHA-1 (verbatim copy + green ROM = match); no
iteration, no byte-`cmp` (that guard is classical-loop-only).

**Trigger:** `pick_target.py`'s `upstream` column is `libultra` / `libkmc` / `libnusys` / `libmus`
(not `none`).

**Procedure:**
1. Upstream roots: libultra `~/development/repos/ultralib/src/`, libkmc
   `~/development/repos/libkmc/src/`, libnusys
   `~/development/repos/n64sdkmod/packages/libnusys/usr/src/PR/libsrc/nusys-2.07/nusys/src/`.
   ultralib is the *sole* libultra source. The project builds `-DBUILD_VERSION=VERSION_J`, so
   version-conditional upstream code selects itself via ultralib's own `#if BUILD_VERSION` guards —
   no manual version picking. Do **not** consult `libultra_modern` (deprecated): it is 2.0L-only and
   its casts diverge from this VERSION_J build (see the cast-divergence section).
2. Project path mirrors the upstream path exactly: take the upstream path relative to its `src/`
   root and prepend `src/lib<name>/`. E.g. `ultralib/src/io/vigetcurrcontext.c` →
   `src/libultra/io/vigetcurrcontext.c`. Mirror ultralib's functional subdirs verbatim
   (`io/`, `os/`, `libc/`, `audio/`, `gu/`, …) — do not rename or flatten them.
   Companion headers mirror their upstream `include/` layout under `include/lib<name>/`.
3. Verbatim means verbatim: keep dead `#ifdef _DEBUG` blocks and their unconditional companion
   `#include`s (they compile out because `_DEBUG` is undefined). If such a block needs a header
   missing from the tree, copy that header rather than trimming the include.
4. Skip clang-format under `src/libultra/` and `src/libkmc/` (verbatim cross-reference copies; both
   dirs carry a local `.clang-format` with `DisableFormat: true`).
5. `make` → ROM SHA-1 must equal the baserom.

Before declaring a clean verbatim mirror, reconcile the upstream's full call + data-ref list
(including one level of library-macro expansion) against the name files — see the recover-extern
and near-verbatim sections. Known-edit sub-cases: file-static drop, defines-data drop, near-verbatim
line-drop, recover-extern placement.

**Provenance:** S15 unlocked the libnusys band by vendoring one `include/libnusys/nusys.h` +
`-I include/libnusys`; `-I include/libultra/PR` is the precedent for unblocking a band.

---

## recover-extern (refs-unplaced)

**Rule:** A clean verbatim leaf often references a global the upstream declares `extern` (defined in
not-yet-decompiled lib code) that is absent from both name files. A *referenced* extern is safe to
place; recover its vram deterministically and add it to `symbol_addrs.txt` before the flip. This is
the lowest-risk mirror and batches well (S19 banked the `nuGfx*FuncSet` trio at one-file risk).

**Trigger:** `pick_target.py` flag `refs-unplaced:<g>@0x<ADDR>` (vram inlined when the binding is
unambiguous — one unplaced name ∩ one asm candidate; otherwise bare names).

**Procedure (zero iteration):**
1. **Confirm the vram from the target fn's own `lui/addiu` HI/LO16 pair** via MCP
   `disassemble_function`. This re-confirm is mandatory. To address the call, look up the *target
   fn's own* vram first: if its curated name is already in `ghidra_symbols.txt`/`symbol_addrs.txt`
   read it there (authoritative), else derive it from the yaml `main` segment base
   (`vram 0x80025C00 @ rom 0x1000` → `vram = rom + 0x80024C00`). Never guess a flat
   `rom + <round constant>` (S22 caught a flat guess that resolved mid-function and silently
   returned a different ~1000 B fn — only the size mismatch vs the expected leaf flagged it).
2. **Indexed-struct-array externs:** the inlined `refs-unplaced` vram is the *field-access* address,
   not the array base. Recover the base by subtracting the `lbu/lw` displacement's struct-member
   offset (S20: `nuContRmbCtl` inlined 0x80104F57 = base + offsetof(`.mode`)=7 → base 0x80104F50).
3. **Add a data extern** to `symbol_addrs.txt` (add-only, disjoint from `ghidra_symbols.txt`):
   `<g> = 0x<ADDR>; // size:0x<n>` (no `type:func`). Size rule: scalar / function-pointer / word =
   `size:0x4`; array = `size:0x<stride×count>` (stride from the index multiply in the asm, count
   from the index bound / `MAXCONTROLLERS` — e.g. `nuContRmbCtl` 0x28 = 10×4, `nuContPfs` 0x1A0 =
   0x68×4).
4. Flip the subseg, verbatim `cp` the upstream, `make` → ROM SHA-1 is the proof.

**Provenance:** S12 (inline-vram idea), S19 (3/3 trio), S20 (indexed-array base correction), S22
(flat-guess trap).

---

## calls-unplaced (function-callee dual)

**Rule:** The same recovery applies to an unplaced *function* the leaf calls. A callee labelled
`func_<addr>` in the asm (absent from both name files) has no symbol for the verbatim C reference to
bind, so the mirror link-fails once the body calls it by name.

**Trigger:** `pick_target.py` flag `calls-unplaced:<fn>@0x<ADDR>` (vram inlined when one unplaced
call ∩ one `func_<addr>` jal). The gate build-check cannot catch this — the `INCLUDE_ASM` scaffold
resolves the jal directly, so the missing symbol only bites in the execution middle.

**Procedure:** The callee's vram **is** its jal target. Add `<fn> = 0x<ADDR>; // type:func` to
`symbol_addrs.txt` (add-only) at the gate, before the flip. Reconcile the full callee list — data
and functions — against the name files in the same disassemble pass that confirms a recover-extern
vram (the asm jal list is already in hand).

**Provenance:** S23 (`osEPiStartDma` → `osPiGetCmdQueue`@0x800B06F0).

---

## Macro-hidden recover-extern

**Rule:** A recover-extern (data or callee) can hide inside a library macro the leaf invokes rather
than appearing in the `.c` body, so neither the `.c`-source ref-grep nor the gate build-check sees
it and the mirror link-fails mid-execution. When mirroring a leaf that invokes a library macro
(`EPI_SYNC`, `EPI_*`, `WAIT_ON_IOBUSY`, …), expand the macro and reconcile its referenced
globals/callees too.

**Trigger:** `pick_target.py`'s `refs-unplaced`/`calls-unplaced` now follow one level of macro
expansion, so a macro-hidden extern usually surfaces as a normal flag. Confirm against the asm.

**Procedure:** Identical to recover-extern — read the unplaced global's vram from the target fn's
own `lui/addiu` HI/LO16 pair in the extracted asm, add the `symbol_addrs.txt` entry, `make extract
&& make`. Note: when the index (e.g. `domain*4`) is a separate `addu`, the `%hi/%lo` base is direct
— no field-offset subtraction.

**Provenance:** S41 (`__osEPiRawWriteIo` → `EPI_SYNC` references `__osCurrentHandle[domain]`;
`__osCurrentHandle` = D_800C7E90, an `OSPiHandle*[2]` array → `size:0x8`).

---

## Near-verbatim mirror (jal-count-mismatch)

**Rule:** This ROM's build can disagree with the upstream `.c` on which calls it makes. The fix
depends on the size of the difference.

**Trigger:** `pick_target.py` flag `jal-count-mismatch:<C>vs<asm>` (advisory — it strips dead
`#ifdef _DEBUG`/`#ifndef _FINALROM` first, but macro/inlined calls can skew the count). Confirm at
the gate by disassembling and comparing the jal list against the upstream call list.

**Procedure:**
- **Small (≤2): try verbatim first.** The mismatch may be a GCC -O3 tail-merge artifact (one shared
  `jal` for identical call+args across branches). Verbatim copy + `make`; only investigate
  line-drops if the SHA-1 doesn't match. (S35: `osStartThread` `9vs7` = three identical
  `__osEnqueueThread(&__osRunQueue, t)` sites merged.) **Do NOT hand-fold an early-return to force
  the count.** When the upstream has two *identical* tail blocks — an early-return
  `if (cond) { f(); return X; }` plus the same `f(); return X;` at the end — -O3 **cross-jumps**
  them into one shared tail itself, which IS the `jal N-1`. Copy verbatim and let the compiler do
  it; rewriting to `if (!cond) { body }` changes register allocation and can emit a *different-sized*
  function. **Wrong-size diagnostic:** if a verbatim/near-verbatim mirror's built function differs
  from the baserom in *instruction count* (not just a local field), the cause is regalloc /
  tail-merge, not a logic bug — every downstream function then shifts and the whole ROM mismatches
  (a far worse symptom than a one-row diff). Restore the verbatim structure before chasing it. (S47:
  `osCartRomInit` `6vs5` — hand-folding the `if(!first){rel;return;}` early-return compiled 0x10
  short (1 vs 2 callee-saved regs); the verbatim source cross-jumped to the exact baserom regalloc.)
- **Large (>2): default to classical.** The ROM may implement different logic (different branches,
  args, stripped control flow). Disassemble and compare structure vs the upstream body before
  routing; only a confirmed clean line-drop stays a mirror. (S30: `osSetTimer` `5vs2` was a
  stripped classical, not a 3-call drop.)
- **Confirmed clean drop:** stays a mirror — copy verbatim, drop the diverging line(s)
  (deterministic from the asm jal list), `make` → ROM SHA-1 is the proof. (S18: `nuContInit` drops
  the absent `nuContPakMgrInit` call.)
- **Jal-less dropped block (jal count can MATCH).** A dropped block need not contain a call, so the
  jal count can agree while the build still diverges — `pick_target.py` flags nothing. A verbatim
  mirror that links fine but misses the ROM SHA-1, with no flagged hazard, often means this ROM omits
  a jal-less upstream guard/early-return (or replaces a multi-call block with a one-liner). Don't
  iterate C blind: `.o`-diff against the baserom asm first (same first-SHA-miss remedy as the cast
  divergence below). The dropped lines are deterministic from the asm structure. (S45 `osGbpakReadWrite`
  drops `if (size == 0) return 0;` — a jal-less early-return that folds into the later `blez` size<=0
  path; `osGbpakReadId` replaces the upstream `if(bcmp){ write-temp; reread; recheck }` retry block
  — 5 calls + a `temp[32]` local — with `if (bcmp(...)) return 4;`, where the jal count *did* diverge.)

**Provenance:** S18, S30, S35, S45, S47.

---

## Mirror cast divergence (sign- vs zero-extend)

**Rule:** A verbatim mirror can match every instruction except the width-extension of one 64-bit
context/field assignment, when this ROM's source casts differently from the upstream `.c`. KMC GCC
emits `sra rd,rs,0x1f` for a sign-extending `(s64)(s32)x` but `move rd,zero` (writing the high word)
for a zero-extending `(u64)(u32)x` — same low word, different high word. Invisible to every gate
check (jal count, ref/header grep) and the `INCLUDE_ASM` gate build; surfaces only as a full-make
ROM SHA-1 miss in the execution middle (same late-surfacing class as jal-count-mismatch S18 /
wrong-lib-header S40).

**Trigger:** a clean-mirror flip (no hazard flagged) whose verbatim copy links fine but the ROM
SHA-1 doesn't match, and the byte diff is isolated to one field's high word.

**Procedure:** on the first SHA miss of a context-building / pointer-into-64-bit-field mirror, diff
the compiled `.o` (`mips-linux-gnu-objdump -d build/.../<file>.o`) against the baserom asm before
assuming a deeper problem. If the only delta is `sra rd,rs,0x1f` (baserom) vs `move rd,zero` (yours)
or vice-versa, flip that one cast: `(u64)(u32)x` ↔ `(s64)(s32)x`. Match the sibling fields —
VERSION_J commonly sign-extends the whole `OSContext` block, so align an outlier zero-extend to its
neighbors.

**Provenance:** S44 (`osCreateThread`: `context.ra = (s64)(s32)__osCleanupThread` sign-extend, vs
the old 2.0L upstream's `(u64)(u32)` zero-extend; sibling `sp`/`a0` already sign-extended). This
divergence is one reason the project standardized on ultralib VERSION_J as the sole upstream.

---

## file-static (BSS-layout-conflict)

**Rule:** A file-scope `static <type> <name>;` (BSS global) in the upstream blocks the verbatim
mirror: KMC GCC emits a section-relative `.bss` reloc, the linker can't place the local BSS at the
splat-side `next` slot, the 0x10-aligned chunk inserts ahead and shifts every downstream BSS symbol,
breaking every code reloc into that range (10,853 ROM bytes diff for `rand`). Function-local
`static` is fine — only file-scope is the problem.

**Trigger:** `pick_target.py` flag `file-static`.

**Procedure:** Fall to the classical loop with the `static` **dropped** in the C body. The linker
then resolves to the splat-side global at the correct vram.

**Provenance:** `rand` (file-scope-static pre-flight).

---

## defines-data

**Rule:** A leaf that *defines* a placed global (the `.data`/`.bss` analogue of file-static) hits
the same BSS-layout-conflict. Route to the classical loop with the definition dropped. (Distinct
from `refs-unplaced`, where a merely *referenced* extern is safe to place.)

**Trigger:** `pick_target.py` flag `defines-data:<g>,...`. Note `__osDequeueThread` is a
`defines-data` false-clean even inside the warm thread band — it does not fast-path.

**Procedure:** Classical loop, drop the definition; the linker resolves to the splat-side global.

**Verbatim-body fast path (S42).** When the *only* edit from upstream is dropping the file-scope
definitions (the function body is otherwise verbatim), this is a known-edit **mirror**, not a true
classical target — prove it by full-make ROM SHA-1 like any mirror, skip the seed / `decomp_loop` /
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
(`extern u8 nintendo[48];` — size it so `sizeof`/`ARRLEN` still compiles; an incomplete `[]` errors
on `sizeof applied to an incomplete type`), then add the symbol add-only at its real vram so
`make extract` renames the `main_data` dlabel. No `.data` is emitted, no segment shift, and the
`.text` reloc target is identical (`%hi/%lo` either way). The splat `D_<vram>` name is its real vram;
a `.NON_MATCHING`-suffixed map address is an alias, not the storage. (S45 `osGbpakReadId`:
`nintendo`@0x800C93F0 size:0x30, `mmc_type`@0x800C9420 size:0x14.)

---

## needs-header

**Rule:** Include-resolvability is a pick/DoR hazard. Two cases, distinguished at the gate.

**Trigger:** `pick_target.py` flag `needs-header:<inc>` (greps every upstream `#include` against the
project `-I` set: `include/{,libultra,libultra/internal,libkmc,libnusys}`; the grep is `#ifdef`-blind
so a dead-`_DEBUG` include can over-flag — confirm against the upstream).

**Procedure:**
- **Missing-but-copyable** → copy the companion header into the tree in the execution middle (e.g.
  `assert.h` for `visetmode.c`).
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
types — a resolvable-but-wrong-library include passes the grep AND the gate build-check (the
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
- **libultra = `-O3 -funsigned-char`** with `MIPS_VERSION=-mips3` for VERSION_J (ultralib gcc.mk).
  Global CFLAGS uses `-mips3` (changed from `-mips2` in S33). `-O3` enables inlining of small
  same-TU functions and affects delay-slot scheduling.

**Trigger:** A libkmc match locking at ~0.9 across every C rewrite (suspect scheduler-level); a
libultra match locking at ~0.9 with unexpected register usage or wrong delay-slot instructions.

**Procedure:** The Makefile carves `LIBKMC_CFLAGS := $(subst -O2,-O,$(CFLAGS))` and
`LIBULTRA_CFLAGS := $(subst -O2,-O3,$(CFLAGS)) -funsigned-char -DBUILD_VERSION=VERSION_J`, each via a
specificity-winning pattern rule. `decomp_loop.py` auto-applies the right profile when a matching
`src/libkmc/<basename>.c` or `src/libultra/<relpath>.c` exists. If a match locks at ~0.9, verify the
loop is using the correct profile (compile `base.c` with `-O` directly, or pass `LIBULTRA=1` /
`--profile libultra`) before iterating further on the C.

**Provenance:** S33 (`-mips3`).

---

## .rodata sibling-yaml pattern

**Rule:** When GCC emits a `.rodata` constant for a C file whose text is at one ROM offset but whose
`.rodata` must land at a different ROM offset, add a **dot-prefixed** `.rodata` subseg at the correct
ROM offset with the **same name** as the `c` subseg. Splat matches siblings by name; the dot-prefix
subseg's `out_path()` returns the compiled C object, placing `build/src/xxx.o(.rodata)` at the
correct VRAM. `should_self_split()` = False for dot-prefix types (no asm file extracted), and
`auto_link_sections` won't insert a duplicate.

**Trigger:** A libultra/libkmc file with a compiler-generated rodata constant at a different ROM
offset than its text (e.g. a `2^32` double). `pick_target.py` pre-flags this as
`rodata-literal:0x<vram>` on a mirror candidate whose asm loads an anonymous pooled FP constant
(`ldc1/lwc1 %lo(D_<addr>)`), so the sibling split is a known DoR enabler, not a finalize surprise.

**Procedure:** Split the adjacent autogenerated asm rodata subseg so the new `.rodata` subseg covers
only the correct bytes, then insert `[0x<RODATA_ROM>, .rodata, path/to/file]` (dot-prefixed, same
name as the `c` subseg).

**Provenance:** S38 (`osAiSetFrequency`: split `[0xAD5E0, rodata]` at 0xAD6A0; inserted
`[0xAD6A0, .rodata, libultra/monegi/ai/aisetfreq]` = 8-byte double + 8-byte pad at 0x800D22A0).
S48 (`__osViSwapContext`: same `2^32` double; split `[0xAD9C0, rodata]` at 0xAD9E0; inserted
`[0xAD9E0, .rodata, libultra/io/viswapcontext]`; this sprint added the `rodata-literal` pre-flag).

---

## IO_WRITE/IO_READ isolation artifact

**Rule:** Functions that access N64 MMIO via `IO_WRITE`/`IO_READ` always score ≠ 0 in isolation: the
macros expand to `*(vu32*)<literal_addr>`, baking in constants, while the reference asm object uses
symbolic relocations (`%hi(AI_DRAM_ADDR_REG)`) that assemble to placeholder zeros. asm-differ sees
mismatched `lui` immediates (up to 600-point penalty, `total_rows == match_count`,
`top_mismatches: []`). No C edit fixes this — it is isolation-only.

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
`move v0,zero`, mnemonic-level diff silently false-positives — **spot-check by byte-level `cmp` of
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
`permuter_settings.toml` mirrors the Makefile's `src/%` pipeline.

**Provenance:** S39 (struct-field reloc addend encoding).

---

## Multi-function-segment splitting (pack)

**Rule:** Splat often packs several functions — sometimes from different upstream files — into one
`[0x<rom>, asm]` subseg. When more than one upstream file lives in the subseg, split it before
flipping rather than flipping the whole block.

**Trigger:** `pick_target.py` flag `pack:<n>fn[fn=stem,...]` (different stems → multi-file pack).

**Procedure:** Insert a new `[0x<offset>, asm]` line at the boundary between upstream files (read the
first instruction's vram from the asm). Two cases: (a) upstream-mirror on the first file → flip the
first chunk to `[0x<seg>, c, lib<name>/<basename>]`, leave the rest `asm`; (b) classical (statics
block the mirror) → flip the first chunk to `[0x<seg>, c]` so the scaffold only carries
`INCLUDE_ASM` stubs for the functions you're targeting. Same principle for overlay-BSS packing.

**Provenance:** 0x8E620 (`rand`+`srand` from `rand.c` packed with `_xsincos`+`sin`+`cos`+`tan` from
`sin.c`).

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
loads (HI/LO16 relocs against project-local symbols), those are isolation-only — the spot-check is
the truth.

**Trigger:** Score ≠ 0 in isolation but the mismatches are HI/LO16 reloc address loads.

**Fast recognition signal (S43):** `decomp_loop.py` reports a **non-zero `score` with empty
`top_mismatches` AND `match_count == total_rows`** (every row matched, yet a residual score). That
combination is definitionally an isolation artifact — asm-differ found no mnemonic-row diff, so the
score is pure reloc/addend noise (struct-field LO16 addends like `pfs->queue`/`pfs->channel`, extern
HI/LO16 calls against now-placed symbols). It is NOT a near-miss: do not iterate C, do not run the
permuter. Go straight to the in-tree spot-check / full-make ROM SHA-1, which is byte-authoritative.
S43 `osGbpakGetStatus` scored 15 / 99.8% this way (76/76 rows, empty mismatches) and the full make
matched the baserom unchanged.

**Procedure:** Trust the in-tree spot-check, not the isolated score.

---

## Decompile-vs-asm authority

**Rule:** The asm (`disassemble_function`) is ground truth, not the Ghidra decompile. The decompiler
can be silently wrong on classical leaves. Use the decompile for shape/types; translate the actual
logic from the instruction listing.

**Provenance:** S11 (Ghidra rendered `func_800AB600`'s return as `return 0` when the asm returns
`(status>>8)&1`).

---

## open-band fast-path

**Rule:** Once a band has ≥2 banked siblings (e.g. `monegi/thread/`, `monegi/vi/`), a candidate
`pick_target.py` reports with **no hazard** may skip the agent's manual per-ref symbol-presence
re-grep at the gate — `pick_target.py`'s own ref-grep is authoritative. The fast-path never overrides
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

---

## intrinsic-likely / maybe-upstream (signature hints)

**Rule:** Two advisory flags from the signature matcher.
- `intrinsic-likely` — a register/FPU shim → `hasm`, not a classical target. Refuse it.
- `maybe-upstream:<lib>:<bases>` — an un-named subseg (`func_<addr>`) that the signature matcher
  thinks is an un-named SDK mirror (the S13 trap). Verify against the candidate upstream at the gate
  before treating it as classical.

**Trigger:** `pick_target.py` flags `intrinsic-likely` / `maybe-upstream:…`.

**Provenance:** S13 (un-named SDK mirror trap).

---

## needs-define

**Rule:** A function guarded by a preprocessor define not active for its lib won't compile into the
build as written.

**Trigger:** `pick_target.py` flag `needs-define:<def>`.

**Procedure:** Confirm the gating define against the upstream; if it is not in the lib's active
define set, the leaf is not a clean flip — defer or handle the define at the gate.

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

## Display lists

**Rule:** F3DEX2 microcode. Include `<PR/gbi.h>` when the target manipulates `Gfx*`.

**Procedure:** For static dlists in rodata, run `~/development/repos/n64-tools/src/gfxdis-rom/gfxdis`
(build once with `make -C ~/development/repos/n64-tools`). `gfxdis` only handles static dlists;
dynamic builders are hand-decompiled.
