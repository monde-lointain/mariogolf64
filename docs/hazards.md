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

**`#pragma weak` alias mirror (S66, no special handling).** A libultra C file whose ROM/curated
symbol is a *weak alias* mirrors verbatim with zero edits. `gu/cosf.c` / `gu/sinf.c` carry
`#pragma weak cosf = __cosf` + `#define fcos __cosf`, so the defined function is `__cosf` and `cosf`
is a weak alias at the same address. KMC gcc 2.7.2 emits both symbols; the linker resolves the
curated `cosf` (from `ghidra_symbols.txt` / `symbol_addrs.txt`) to the subseg address and `__cosf`
rides along — compiles + matches clean, no `#pragma`/define edit. This is the C analog of the
asm-mirror `WEAK(bcopy, _bcopy)` alias (S58 bcopy `_bcopy=bcopy`). `pick_target.py` keys the upstream
index by the weak alias too (S66 `PRAGMA_WEAK_RE`), so a weak-aliased pack member resolves to its
`.c` (`cosf=cosf`) instead of the opaque `cosf=?` that buried it in a `c-combined` pack. Watch for a
shared `__`-prefixed data extern reached only through the weak fn (S66: `__libm_qnan_f`, the NaN-path
return refd by *both* cosf and sinf as an anonymous `D_<addr>`) — a recover-extern the
`refs-unplaced` flag misses when the fn is a hidden pack member (see #recover-extern).

**Provenance:** S15 unlocked the libnusys band by vendoring one `include/libnusys/nusys.h` +
`-I include/libnusys`; `-I include/libultra/PR` is the precedent for unblocking a band. S51 is the
cautionary case for step 1's "do not consult `libultra_modern`": its split `gu/` layout
(hand-asm `mtxcatf.s` + C `mtxxfmf.c`) mis-suggested `guMtxCatF` was hand-asm at the gate, while
ultralib's `src/gu/mtxcatf.c` (the sole source) holds BOTH fns as clean C — which the ROM matched
verbatim. Reach for ultralib first; a `libultra_modern` `.s` is not evidence the ROM fn is hand-asm.

---

## asm-mirror vendoring

**Rule:** The asm analog of the upstream-mirror pattern, for a libultra function that is genuinely
hand-written assembly (a register/FPU/cache/TLB primitive — the `intrinsic-likely` shims). Instead of
leaving it as an anonymous `asm/<rom>.s` disassembly or a bare `hasm`, vendor the **real ultralib
`.s` TU** into `src/libultra/<dir>/` and build it. The proof is the full-`make` ROM SHA-1, same as
the C mirror. Pilot: S56 (`getcount`/`getcause`/`getsr`/`setcompare`).

**Trigger:** `pick_target.py` flags `intrinsic-likely:<tu>.s` (a vendorable ultralib TU exists) on
an `asm-flip` candidate. A bare `intrinsic-likely` (no `:<tu>`) is a no-source shim → plain `hasm`.

`intrinsic-likely:cp0-asm(identify-TU)` (S70 #1) is the same asm-mirror class for an **un-named**
`func_<addr>` subseg whose body holds a privileged op gcc never emits (`tlbwi`/`tlbwr`/`tlbp`/`tlbr`,
`mfc0`/`mtc0`/`dmfc0`/`dmtc0`/`cfc0`/`ctc0`, `cfc1`/`ctc1`, `cache`, `eret`). The op proves a
vendorable ultralib source exists but the un-named primary can't resolve the `.s`, so the gate
**identifies the TU first** — one MCP `disassemble_function` names it by its CP0/TLB signature
(e.g. `mfc0/mtc0 Index/EntryHi + tlbwi` ⇒ `osMapTLB`/`osUnmapTLB`, S70) — then vendors it per the
procedure below. This is *broader* than the pure-shim `intrinsic_likely`: it fires even when the body
has real control flow or `jal`s around the privileged op (`osMapTLB`'s branch/lw logic; an
exception/context dispatcher's `mtc0`+`jal`, e.g. the mislabeled `audio_sched_thread_entry`
0x800B3E50). The point is negative as much as positive — such a subseg is hand-asm, **never** a
classical decomp target, so smallest-first must not surface it as one. If no ultralib source turns up
for the identified TU, it falls back to plain `hasm`.

**Procedure:**
1. **Copy the ultralib `.s` verbatim** from `~/development/repos/ultralib/src/<dir>/<file>.s` to
   `src/libultra/<dir>/<file>.s` (mirror the subdir, same as the C pattern).
2. **Vendor any missing asm macros** into the project headers (verbatim from ultralib). The MONEGI
   `include/sys/asm.h` lacked `MFC0`/`MTC0` (S56 added them). The TU's `#include "PR/R4300.h"`
   resolves via `-I include/libultra`; `sys/asm.h`/`sys/regdef.h` via `-I include`.
   `pick_target.py` pre-flags this as `intrinsic-likely:<tu>.s(needs-define:<MACROS>)` when the
   vendorable `.s` references an `UPPER_CASE` macro none of the in-tree asm `-I` headers
   (`include`, `include/libultra`, `include/libultra/PR`, `include/libultra/compiler/gcc`) define —
   so the enabler is priced at the gate, not at a failing vendor-compile (S57 retro #1). The whole
   current cache/TLB/gu backlog is self-contained (all `DCACHE_*`/`C_*`/`TLB*`/`C0_*` ship in
   `include/libultra/PR/R4300.h`, `RDB_*` in `PR/rdb.h`) → the pre-check reports nothing for it.
3. **Assemble with ultralib's EXACT toolchain + flags — KMC/N64 gcc, gcc.mk profile — NOT modern
   `mips-linux-gnu as`.** This is the load-bearing step: KMC `as` pads each function's `.text` up to
   its 16-byte ROM slot, so the verbatim 0xC TU lands as the 0x10 the ROM expects. Modern `as` emits
   the bare 0xC and every following subseg shifts → SHA-1 break (the entire S56 failure mode before
   the toolchain switch). The Makefile carries this as `LIBULTRA_ASFLAGS` + the `VENDOR_ASM`
   `<rom>:<src>` map + a `define`/`foreach`/`eval` rule that builds `build/asm/<rom>.o` from the
   vendored `.s` (explicit rule wins over the `asm/%.s` pattern rule). Add a `<rom>:<src>` pair to
   extend. (This mirrors how `LIBULTRA_CFLAGS` mirrors ultralib's C profile, and the `8EC50.o`
   `mmuldi3.s` override is the prior art for a single vendored-asm `.o`.)
4. **Flip the subseg `asm` → `hasm`** in `mariogolf64.yaml`. splat's `hasm` `split()` writes the
   `.s` only if absent, so the existing `asm/<rom>.s` is kept (vestigial reference, like `8EC50.s`)
   and never regenerated; `pick_target.py` skips `hasm` (it classifies only `asm`/`c`). The build
   gets the `.o` from the `VENDOR_ASM` rule, not from `asm/<rom>.s`.
5. `make extract && make` → ROM SHA-1 must equal the baserom. Verify the vendored `.o`'s `.text`
   size equals the subseg size (`mips-linux-gnu-size -A build/asm/<rom>.o`).

**Caveats:**
- The `.ld` does **not** `ALIGN` between subsegs (only segment-level `ALIGN(__romPos,16)`), so the
  pad must live in the object — there is no linker fallback. This is why step 3's KMC padding is
  mandatory, not cosmetic.
- Per-TU version-match is unproven beyond the S56 reg shims (S57 added 4 cache/TLB primitives, all
  first-try clean). Each new TU needs its own ROM-SHA-1 verify; a macro-expansion divergence (e.g.
  an aliased `WEAK(bcopy, _bcopy)` whose project `WEAK` macro is empty) can need extra handling.
- **SHA-breaker bisect (a batch of N flips, one bad TU).** The gate `make` validates all flipped
  subsegs at once, so a single version-mismatched TU breaks the whole-ROM SHA-1 without naming the
  culprit. To localize: flip the suspect subseg back to `asm` in `mariogolf64.yaml` (its `asm/<rom>.s`
  disassembly is still present — splat never deleted it), rebuild, and check the SHA-1. SHA goes
  green → that TU was the breaker (it is a spike: leave it `asm`, carry it to `BACKLOG.md`); SHA
  still red → keep flipping suspects back until it greens. Bisect a large batch by reverting half
  the flips at a time. The reverted TU's vendored `.s` stays in `src/libultra/` (unused while `asm`)
  for a later retry against a different ultralib revision. Banked TUs in the same batch are unaffected
  — each subseg's `.o` is independent.
- Do **not** blanket-vendor a mixed `pack:` (e.g. `osSetIntMask` shares its subseg with the C
  mirrors `osCreatePiManager`/`__osEPiRawStartDma`); split first and vendor only the asm member.
- **Combined-subseg sub-pattern (≥2 distinct asm TUs in one subseg).** When one `asm` subseg holds
  ≥2 asm-ONLY functions (no C mirror) from *different* ultralib `.s` files, `pick_target.py` flags
  `combined-subseg:<n>tu[a.s|b.s]`. Split the subseg at each TU boundary first (one `hasm` subseg per
  `.s`, the asm analog of the multi-file C-pack split), add one `VENDOR_ASM` pair per TU, then vendor
  each `.s` verbatim. `make extract` writes the new split subseg's `asm/<rom>.s` (absent → splat
  writes it once); the build still takes each `.o` from its `VENDOR_ASM` rule. Like the
  `intrinsic-likely` path, the flag carries `(needs-define:<MACROS>)` (the union across the pack's
  TUs) when any pack `.s` references an `UPPER_CASE` asm macro the in-tree `-I` headers lack, so the
  define enabler is priced at the gate, not at a failing vendor-compile (S63: `setfpccsr.s` →
  `CFC1`/`CTC1`, which were absent because S56 vendored only `MFC0`/`MTC0` — vendor the missing
  macros into `include/sys/asm.h` verbatim from ultralib `sys/asm.h`). S62:
  `[0x823B0,asm]` held `osInvalDCache`+`osInvalICache` → split at 0x82460 → two `hasm` + two
  `VENDOR_ASM` pairs (slots 0xB0 + 0x80). A mixed pack with a C-mirrorable member splits the same way
  but vendors only the asm TUs and C-mirrors the rest (S63: `[0x8CA50,asm]` = 3 reg-shim TUs + the
  C-mirror `__osSpDeviceBusy` → 3 `hasm` + 1 `c, libultra/io/sp`). Distinct from the two cases above: (a) the mixed-pack caveat
  fires when ≥1 member has a C mirror (a member with a C upstream is excluded from the TU count, so
  the flag does NOT fire); (b) a *partial-TU* pack whose asm members share ONE `.s` (e.g.
  `__osDisableInt`/`__osRestoreInt` both in `setintmask.s`) is a single distinct TU → also does NOT
  fire, and needs a harder partial-TU carve (still a spike).

**Provenance:** S56 (4 reg shims; KMC-as padding the load-bearing discovery, per PO directive to use
ultralib's exact flags). S57 (4 cache/TLB primitives — `osWritebackDCacheAll`/`osWritebackDCache`/
`osUnmapTLBAll`/`__osProbeTLB`, all first-try clean; added the `needs-define` pre-check + this bisect
protocol). S58 (3 cross-dir TUs: `sqrtf`/`osMapTLBRdb`/`bcopy`, clearing the `WEAK`-alias + FPU-op
cases). S62 (`osInvalDCache`+`osInvalICache` — the combined-subseg split sub-pattern + the
`combined-subseg` pre-flag). S63 (the `[0x8CA50]` reg-shim "set" family `setfpccsr`/`setsr`/
`setwatchlo` + the C-mirror `__osSpDeviceBusy` — first mixed asm-mirror + C-mirror subseg-clear;
extended the `needs-define` pre-check to the `combined-subseg` path after `CFC1`/`CTC1` surfaced at a
failing vendor-compile).

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

**Renamed-vs-substituted callee — body-compare before editing (S61).** When the upstream calls
`fooBar` but the jal target is already named *something else* (`calls-unplaced:fooBar` flagged, yet
the asm `jal`s a placed symbol with a different name), there are two cases and they need opposite
fixes: **(1) renamed** — same function, decomp gave it a different name → the mirror is still
verbatim, just confirm the name and leave the body (no edit; the placed name binds the jal); **(2)
substituted** — this ROM calls a *genuinely different* function for that slot → edit the C body to
call the placed name (a known-edit near-verbatim mirror). **Do not assume (1).** Disassemble the
target and body-compare it against the upstream callee's prebuilt `.o`
(`mips-linux-gnu-objdump -d ~/development/repos/ultralib/build/J/.../<callee>.o`): stack frame, error
paths, sqrt/branch handling. A fast first discriminator is the address — a libultra callee lives in
the 0x800A_0000+ block; a target in the game code region (low 0x8002_xxxx) is almost certainly a game
substitution. (S61 `guRotateF`: upstream `guNormalize`, but the ROM `jal`s `vec3f_normalize`@0x80029900
— body-compare proved a *different* fn (game region; −0x28 frame + `osSyncPrintf` degenerate-input
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

**Trigger:** `pick_target.py` flag `file-static`. **Static *functions* do not count** — a
file-scope `static <type> <name>(...);` (proto/def) shares the mirror's TU and is no BSS hazard, so
`has_file_scope_static` skips it (S54: sprintf's `static void* proutSprintf(...);` was a false flag
that mis-routed a clean 2-fn mirror toward the classical loop). The detector strips
`__attribute__((...))` before the declarator check so an attributed static *array*
(`static OSMesg buf[N] __attribute__((aligned(8)));`) still flags. Known gap: an *initialized*
static (`static T x[] = {...};`) carries `=` and is invisible to the regex — not flagged (caught at
the gate / link if a sub-fn of a decompose pack is later mirrored).

**Procedure:** Fall to the classical loop with the `static` **dropped** in the C body. The linker
then resolves to the splat-side global at the correct vram.

**Provenance:** `rand` (file-scope-static pre-flight); `sprintf` (static-function false-flag, S54).

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

**Function-local static FP constant — the `data-static` pre-flag (S52).** A function-local
`static float`/`double` initializer (e.g. `static float dtor = 3.1415926/180.0`) is the FP analogue
of the S45 array case: the mirror re-emits its initialized bytes into the data segment, shifting it
→ full-make SHA miss. Unlike the array case it *is* visible in asm — the fn loads it via
`lwc1/ldc1 %lo(D_<addr>)` — so `pick_target.py` pre-flags it `data-static:0x<vram>`. Crucially this
is **distinct from `rodata-literal`**: pick_target classifies the `%lo(D_)` address by segment
(vram − the fn's code-segment delta → ROM offset; in a `rodata` subseg ⇒ rodata-literal sibling
split, else ⇒ `data-static`). The two enablers differ — `data-static` is the S49 fast path here (drop
the fn-local `static` → a file-scope `extern <type> <name>;` + add the symbol add-only at its real
vram so `make extract` renames the `D_<vram>` dlabel; no `.data` emitted, `.text` reloc identical),
**not** a `.rodata` sibling split. (S49 `xseed`@0x800C81C0; S52 `dtor`@0x800C81E0 for `guRotateRPYF`;
the same gu data band holds `guRotateF`@0x800C81D0, `guAlignF`@0x800C81A0.)

**The drop-extern *hoist* vs. the verbatim-static *carve* (S61).** Be precise about scope: this is a
**function-local** `static` (declared inside the fn, persists across calls, *internal linkage*) — not
a file-scope `static`. Compiled verbatim, the compiler emits it as a **local** symbol (`dtor.2` in the
`.o`'s symbol table), so two siblings' identically-named function-local statics **never collide** —
they are distinct locals in distinct objects (`guRotateF`'s `dtor`@0x800C81D0 and `guRotateRPYF`'s
`dtor`@0x800C81E0 coexist fine as verbatim locals). The S52 drop-extern fast path *hoists* the
function-local static up into a file-scope `extern <type> <name>;` (giving it external linkage); that
is what introduces a name — and a second sibling can't hoist to the **same** file-scope name
(`guRotateRPYF` already hoisted `dtor` + claimed its `symbol_addrs` entry, so a second `extern float
dtor;` is a duplicate symbol / binds the wrong vram). So two ways place a function-local FP static:
**(a) hoist** under a *unique* file-scope name (curated `<fn>_dtor`, or the raw `D_<vram>` dlabel) +
symbol add — no `.data` emitted; **(b) keep it verbatim** (preserving the function-local semantics)
and carve a `.data` subseg for the file — `[<rom>, .data, lib<...>/<file>]` + a continuation
`[<rom+ext>, data]`. The carve is the more verbatim option (no source edit, no hoist). **If you carve,
size the extent from the COMPILED object, not the static's byte size** —
`mips-linux-gnu-objdump -s -j .data build/.../<file>.o` (the section is alignment-padded: `dtor` is
4B but `rotate.o(.data)` is **16B** = 4B + 12B pad), cross-checked against the baserom bytes to the
next data symbol. A short carve double-counts (the compiled `.data` injects its full padded extent
while the un-carved tail still emits the original bytes) → the ROM grows and the whole data segment
reflows (S61: a 4B carve grew the ROM +16B; the misleading `cmp` first-diff was at 0x1008, an
artifact of the size-grown file, not the real divergence). Clean-rebuild to verify. (S61 `guRotateF`:
16B carve `0xA35D0`→`0xA35E0`.)

**Carve timing — execution, never the plan gate (S68).** A `.data` (or any ld-section) sibling
carved from the C compile cannot land at the `/sprint-plan` gate: the gate validates the *stub*
state, where an `INCLUDE_ASM` `.text`-only object emits no `.data`, so the sibling carves an empty
range and shifts every following data byte → the gate green-ROM SHA check fails. The gate flips
**text only**; add the ld-section split during execution, **together with the verbatim body** (which
makes the `.o` emit the real `.data`). The stub's `%lo(D_<vram>)` resolves from the existing generic
`data` subseg in the meantime. (S68 `gu/align.c`: deferred the `[0xA35A0, .data, libultra/gu/align]`
carve from gate to the body step; the verbatim twin of S61's carve, first-build match.)

---

## needs-header

**Rule:** Include-resolvability is a pick/DoR hazard. Two cases, distinguished at the gate.

**Trigger:** `pick_target.py` flag `needs-header:<inc>` (greps every upstream `#include` against the
project `-I` set: `include/{,libultra,libultra/internal,libkmc,libnusys}`; the grep is `#ifdef`-blind
so a dead-`_DEBUG` include can over-flag — confirm against the upstream).

**Vendorable annotation (S54).** `pick_target` tags each missing header `<inc>(vendorable)` when it
is copyable from upstream — a public companion under an `UPSTREAM_INC_ROOTS` include dir (copy into
an `-I` dir) OR a source-private header found by basename under an `UPSTREAM_SRC_ROOTS` source tree
(`ultralib/src/**`, copy source-relative next to the mirror — `xstdio.h`, `guint.h`). A vendorable
header is a one-time header-vendor *enabler* (pts `needs-copy` +1), **not** a `blk` DoR reject; only
a *bare* (untagged) header — present in-tree but unreachable (deferred `-I`), or absent everywhere (a
system header) — sets `blocked` → `blk`. This closed the recurring "blk that's actually a 1pt cp"
gate-time re-diagnosis (S49 guint.h, S53 PR-band, S54 sprintf's `xstdio.h`/`string.h`).

**Procedure:**
- **Missing-but-copyable / `(vendorable)`** → copy the companion header into the tree (a public one
  into its `-I` dir like `assert.h` for `visetmode.c`; a source-private one source-relative next to
  the mirror like `xstdio.h` → `src/libultra/libc/`).
- **Band-local quote-include (already resolved)** → a `#include "x.h"` resolves source-relative to
  the dir the mirror compiles in, so once the first band sibling has shipped its companion at
  `src/lib<...>/<band>/x.h` (e.g. `gu/guint.h`, copied alongside `gu/random.c` in S49), every
  later sibling that quote-includes it is already clean — no enabler. Since S50 `missing_includes`
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
- **libultra = `-O3 -fsigned-char`** with `MIPS_VERSION=-mips3` for VERSION_J (ultralib gcc.mk —
  but see the char-signedness note). Global CFLAGS uses `-mips3` (changed from `-mips2` in S33).
  `-O3` enables inlining of small same-TU functions and affects delay-slot scheduling.
  **Char signedness is `-fsigned-char`, NOT ultralib-J's `-funsigned-char` (S65).** ultralib's
  `gcc.mk` adds `-funsigned-char` for VERSION_J, but this ROM's libultra was built signed: an S65
  full `make clean` rebuild under `-fsigned-char` reproduced the baserom SHA-1 exactly (every
  current libultra C file matches signed). See `#char-signedness` for the symptom + the per-file
  override mechanism if a future TU ever needs the opposite.

**Trigger:** A libkmc match locking at ~0.9 across every C rewrite (suspect scheduler-level); a
libultra match locking at ~0.9 with unexpected register usage or wrong delay-slot instructions.

**Procedure:** The Makefile carves `LIBKMC_CFLAGS := $(subst -O2,-O,$(CFLAGS))` and
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
signedness — the ROM loads `lb` + `sll/sra` sign-extend (and may emit a phantom empty stack frame),
while the wrong flag gives `lbu` + `andi 0xff`. The exposing pattern is a `char` lvalue compared
against a **non-zero** value (e.g. `strchr`'s `*s != ch`); a comparison against literal `0` is
signedness-invariant (`strlen`/`memcpy` matched under both — `memcpy` fully, `strlen`'s `lb` is the
compiler's free choice for a `!= 0` test).

**Procedure:**
1. The band default is already `-fsigned-char` (S65) — a new libultra mirror compiles signed with no
   action. If a mirror SHA-misses with the `lbu/andi` vs `lb/sll-sra` signature, you have the inverse
   case (a TU that wants the *opposite* signedness).
2. Per-file override mechanism (the explicit-target var beats the `libultra/%.o` pattern):
   `$(BUILD_DIR)/$(SRC_DIR)/libultra/<rel>.o: C_PROFILE_CFLAGS = $(subst -fsigned-char,-funsigned-char,$(LIBULTRA_CFLAGS))`
   (or the reverse subst). Append-and-last-wins also works since gcc takes the final `-f*-char`.
3. To re-settle the band default after several such files, run `make clean && make` with the global
   flag flipped and compare the ROM SHA-1 — the S65 authoritative test (a clean rebuild under
   `-fsigned-char` reproduced the baserom exactly, proving the global default, not a per-file patch).

**Distinct from** the per-field **cast-divergence** hazard (a 2.0L-vs-J `(type)` widening on one
struct field's high word) — that is a source/version difference; this is a whole-TU compiler flag.

**Provenance:** S65 (string.c `strchr`/`strlen` exposed it; the per-file override was the initial
fix, then the clean-rebuild test proved `-fsigned-char` is the correct band default and the override
was removed).

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
`rodata-literal:0x<vram>[,0x<vram>…]` on a mirror candidate whose asm loads an anonymous pooled FP
constant (`ldc1/lwc1 %lo(D_<addr>)`), so the sibling split is a known DoR enabler, not a finalize
surprise. The address is classified by segment first (data-segment statics are `data-static`, not
`rodata-literal` — see #defines-data, S52). The pre-flag lists the **full extent**: GCC moves a
pooled `double` into an FP register pair via an `lw` pair + `mtc1` (not `ldc1`), so a literal's 2nd
(+ further) word is an *integer* `lw %lo(D_<addr>)` invisible to the FP-only scan — pick_target adds
the rodata-band `lw` refs so the split width is sized correctly (S52 `guLookAtReflectF`: `-1.0`
@0x800D2510 via `ldc1` + `1.0` @0x800D2518 via an `lw` pair = a 16-byte block, not 8). The pre-flag
also appends `;carve-end=0x<vram>` (S64 #2): a multi-`du` dlabel block's trailing word has no `%lo`
of its own, so the scan's max referenced literal can understate the carve end (S64 `lookathil`: the
`.double 0` @0x800D2508 sat inside the `D_800D2500` block, so the scan's max 0x800D2500 understated
the 0x800D2510 carve end = the `lookatref` `.rodata` boundary). `carve-end` is the next `.rodata`
subseg boundary (an upper bound, exact when the mirror's literals are the subseg tail — the common
case); the finalize carve is still `.o`-sized, but the gate now sees the planned extent.

**Named vs anonymous pool (S66): no difference — the named-rodata CAVEAT is retired.** A pooled
constant block whose words are *named* in `ghidra_symbols.txt` (e.g. cosf's
`kCoeff4`/`kCoeff3`/…/`kInvPi`/`kPiHi`/`kPiLo`/`kZero`/`zOneHalf1`/`kOneHalf2` at 0x800D2460..0x24B8)
carves **exactly** like an anonymous `D_<addr>` pool. Splat extracts the dot-prefixed `.rodata`
subseg cleanly even though those addresses carry user data labels: the labels become harmless
absolute symbols, the bytes come from the compiled `.o`, and `--allow-multiple-definition` tolerates
the overlap. The S64 `cosf` carry-over's "named-rodata may collide at the `.rodata` carve" warning was
a phantom — verified benign. The one real step a named pool adds is a *confidence check*:
byte-compare the compiled `.o(.rodata)` against the ROM bytes at the target offset **first** (a
verbatim mirror's pool must equal upstream — if it doesn't, the source/version is wrong, discard the
mirror), then carve as usual.

**Procedure:** Split the adjacent autogenerated asm rodata subseg so the new `.rodata` subseg covers
only the correct bytes (the full flagged extent, rounded to the literal block), then insert
`[0x<RODATA_ROM>, .rodata, path/to/file]` (dot-prefixed, same name as the `c` subseg). **Timing: do
this during execution with the body, not at the plan gate** — like the `.data` carve, a `.rodata`
sibling added against an `INCLUDE_ASM` stub carves an empty range (the stub emits no `.rodata`) and
reflows the rodata segment, failing the gate green-ROM check (see `#defines-data` "Carve timing",
S68). The gate flips text only.

**Provenance:** S38 (`osAiSetFrequency`: split `[0xAD5E0, rodata]` at 0xAD6A0; inserted
`[0xAD6A0, .rodata, libultra/monegi/ai/aisetfreq]` = 8-byte double + 8-byte pad at 0x800D22A0).
S48 (`__osViSwapContext`: same `2^32` double; split `[0xAD9C0, rodata]` at 0xAD9E0; inserted
`[0xAD9E0, .rodata, libultra/io/viswapcontext]`; this sprint added the `rodata-literal` pre-flag).
S66 (`sinf`: anonymous pool, split `[0xAD960, rodata]` → `[0xAD960, .rodata, libultra/gu/sinf]`,
exact `.o`-size 0x60 fit; `cosf`: **named** pool, split `[0xAD6F0, rodata]` at 0xAD860 → inserted
`[0xAD860, .rodata, libultra/gu/cosf]` — retired the named-rodata collision CAVEAT as a phantom).

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
Also `c-combined:<n>file[stem1|stem2|…]` (S64 #3): the C analog of `combined-subseg` — fires when ≥2
*distinct* C upstream files share one asm subseg, naming the split targets. It pre-prices the split
+ surfaces a cheap clean leaf that a big combined subseg buries past smallest-first (e.g. the sp
register-shim pack `func_800B16A0` → `3file[sprawdma|spsetpc|spsetstat]`, hidden under `upstream
none` because its unattributed `func_` primary set the column). **Caveat:** a member whose ROM symbol
is a `#pragma weak` alias (e.g. `cosf`, whose `gu/cosf.c` defines `__cosf`) shows as `=?` in the pack
and is *not* counted in `c-combined` — `upstream_index` keys on the defined name, not the alias; such
a leaf still needs manual identification (S64 `cosf` was found by disassembly, recorded in BACKLOG).

**Not every pack splits — `single-file-pack` (S67 #2).** When *every* pack member resolves to one
upstream C file (one stem, no `=?`/asm members), `pick_target.py` emits `single-file-pack:<n>fn[…]`
instead of `pack:<n>fn[…]`. This is the atomic-verbatim-mirror class (guPerspectiveF+guPerspective
S55, guLookAtHiliteF+guLookAtHilite S64, guTranslateF+guTranslate S67): the whole subseg IS one
upstream file, so there is NO upstream-file boundary to split at — copy it verbatim and bank/spike in
one shot (route to `#upstream-mirror-pattern`, not the split procedure below). The `pts` seed is
unchanged (the pack penalty keys on `nfns>1`, not the kind), so the relabel is display-only — it just
stops the gate reading an atomic mirror as a split-required blocker. A mixed asm+C pack or a
multi-stem pack keeps `pack`.

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
compiled nested-loop matmul — both pack fns were clean C from the original SGI `mtxcatf.c`. See also
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

**Combined-subseg case (S51):** When a 2-fn pack has fn2 packed tight after fn1 at a non-16 offset
(fn1's size is not a 16-multiple), you CANNOT split into per-fn subsegs — KMC `as` pads fn1's
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

## intrinsic-likely / maybe-upstream (signature hints)

**Rule:** Two advisory flags from the signature matcher.
- `intrinsic-likely` — a register/FPU/cache/TLB shim, not a classical target. If the detail names a
  vendorable ultralib TU (`intrinsic-likely:os/getcount.s`), asm-mirror it (see
  `#asm-mirror-vendoring`); `intrinsic-likely:cp0-asm(identify-TU)` is an un-named privileged hand-asm
  subseg (also `#asm-mirror-vendoring` — identify the TU first); a bare `intrinsic-likely` (no `:<tu>`,
  no `cp0-asm`) is a no-source shim → plain `hasm`.
- `maybe-upstream:<lib>:<bases>` — an un-named subseg (`func_<addr>`) that the signature matcher
  thinks is an un-named SDK mirror (the S13 trap). Verify against the candidate upstream at the gate
  before treating it as classical.

**Trigger:** `pick_target.py` flags `intrinsic-likely` / `maybe-upstream:…`.

**Provenance:** S13 (un-named SDK mirror trap).

For a definitive (not IDF-guess) version of `maybe-upstream`, see `#coddog-cross-ref` — it pairs the
un-named fn with its ultralib identity by instruction-hash match.

---

## coddog cross-ref

**Rule:** `pick_target.py` classifies an un-named (`func_<addr>`) subseg as `upstream none` →
classical, even when it is a verbatim ultralib mirror. The names being absent is the ONLY reason.
**S71 crc.c** was mis-seeded **pts-13 classical** (`none` + `pack:2fn`) but is a trivial verbatim
2-fn mirror. coddog (`compare2`, reloc-masked instruction hashes) pairs each MG64 fn with the
ultralib fn it matches, revealing the source file — turning the `none`/classical guess into a known
mirror target. Per the S71 sweep, ~all of the remaining libultra band is verbatim-mirrorable.

**When:** an `upstream none` candidate in the libultra vram range (`0x800A_xxxx`–`0x800B_xxxx`)
carrying `pack` / `jal-count-mismatch` / `maybe-upstream`, before committing it to the classical loop.

**Procedure:**
1. Run the sweep: `make coddog-sweep` (or `tools/coddog_sweep.sh`). Needs a fresh `make` (MG64 ELF),
   a built ultralib (`~/development/repos/ultralib/build/J`, the VERSION_J pin), and the coddog
   binary. It builds a combined ultralib-J ELF (KMC objects need
   `objcopy -R .mdebug -R .reginfo …` to normalize the symtab + strip ECOFF, then
   `ld -r --allow-multiple-definition`; objdiff cannot read a `.a` archive), runs `compare2`, and
   writes `tools/coddog/coddog_map.tsv` (gitignored — local, build-dependent).
2. `pick_target.py` (`build_coddog_index`) reads the map if present: a candidate whose lead fn is in
   the map gets a `coddog-mirror:<file>@<pct>` hazard; a **≥99% non-audio** hit is re-priced
   `upstream libultra` so `seed_points` drops it off the `classical and pack` → 13 path (crc.c:
   13 → 3). Audio hits stay advisory (the one-time audio-header enabler is not modeled). Absent map
   → ranking unchanged (the committed golden is map-free; `CODDOG_MAP` env overrides the path).
3. Treat a `coddog-mirror` candidate as `#upstream-mirror-pattern` (verbatim cp from the matched
   `.c`); the `@<pct>` is the confidence (99.99 = byte-verbatim once placed).

**Trigger:** `pick_target.py` flags `coddog-mirror:<file>@<pct>` (only when the map exists).

**Provenance:** S71 (crc.c mis-seed; the recipe + reusable tool memory in `coddog-ultralib-crossref`).

---

## stale-vendored-header

**Rule:** A vendored header can resolve as a *file* (so `needs-header` stays silent) yet be a stripped
revision whose *content* is missing constants the build expects — the gap is the content, not the
path. The specific case: the in-tree `include/libultra/PR/os_version.h` was the 2.0L release header,
which defines only `OS_MAJOR/MINOR_VERSION` and **none** of the `VERSION_D..L` ordinal constants the
ultralib gcc.mk profile uses. An upstream mirror with a `#if BUILD_VERSION < VERSION_K` guard then
sees `VERSION_K` (and the `-DBUILD_VERSION=VERSION_J` token) as undefined → cpp reads both as `0` →
the guard mis-evaluates and the guarded function silently vanishes into a link error, not a compile
error (S60: `gu/mtxcatl.c`'s `guMtxXFML` dropped, `undefined reference to guMtxXFML` at link).

**Trigger:** `pick_target.py` flag `stale-header:os_version.h(<VERSION_X>,…)` — fires when an upstream
file's `#if BUILD_VERSION <op> VERSION_X` references a token the os_version.h on the candidate's `-I`
set does not `#define` (and the lib actually sets `-DBUILD_VERSION`). Distinct from `needs-header`
(file absent) and `needs-define` (a non-version gating define).

**Procedure:**
- Add the missing `VERSION_*` constants to `include/libultra/PR/os_version.h`, verbatim from the
  ultralib gcc.mk `os_version.h` (`VERSION_D 1` … `VERSION_L 9`). This is a one-time additive
  header-content vendor enabler, not a `blk`.
- **Additive + SHA-safe by construction:** every *existing* in-tree guard compares against
  `VERSION_J` and the build sets `BUILD_VERSION=VERSION_J`, so `>= VERSION_J` stays true and
  `< VERSION_J` stays false whether the tokens are `0`-vs-`0` (stripped) or `7`-vs-`7` (vendored) —
  only a `< VERSION_K`-style guard (the one that was wrong) flips. Leave `OS_MAJOR/MINOR_VERSION`
  untouched (the upstream form needs `BUILD_VERSION_STRING`, which is not defined here).
- **Verify with a clean rebuild** (see #clean-rebuild-after-shared-header-edit) — the build has no
  header-dependency tracking, so a `make` after the edit only recompiles the file you touched; the
  other os_version.h consumers keep stale objects and a divergence would hide until the next clean.

**Provenance:** S60 (gu/mtxcatl.c — `guMtxXFML` under `#if BUILD_VERSION < VERSION_K`; the in-tree
os_version.h was the 2.0L revision; the `pick_target` `_strip_inactive_version_branches`/`build_ord`
machinery already existed for ref/call scanning, reused here for the detector).

---

## clean-rebuild-after-shared-header-edit

**Rule:** The build tracks no header dependencies — a `make` recompiles only the `.c` files whose
objects are missing or older than the source, **not** the consumers of a header you edited. So when a
mirror/enabler edits a *shared vendored header* (e.g. `include/libultra/PR/os_version.h`, included
transitively by many libultra TUs), the post-edit verification MUST be a clean rebuild, or a stale
object can mask a divergence the header change introduced elsewhere.

**Trigger:** the finalize/enabler step modifies a header under `include/` that more than one already-
banked TU includes (directly or via `guint.h`/`gu.h`/etc.).

**Procedure:**
- `make clean && make extract && make`, then `sha1sum build/mariogolf64.z64` == baserom. The clean
  rebuild recompiles every consumer against the new header; only then is the SHA-1 a real guard.
- A plain incremental `make` is fine while iterating the *mirror file itself* (rm its stale `.o` if
  the header change doesn't retrigger it), but the **banking** SHA-1 must come from the clean build.

**Provenance:** S60 (the os_version.h VERSION_* add — incremental `make` left `guMtxXFML` undefined
until the touched object was removed; a `make clean` rebuild then confirmed the add was globally
SHA-safe across all os_version.h consumers).

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
