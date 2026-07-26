# Execution loop and every-sprint conventions

<role>
The inline execution loop that runs between the two sprint gates, the oracles it trusts, and the
conventions that hold regardless of hazard. Read this while banking functions. The gates are
`docs/workflow/gates.md`; fan-out decisions are `docs/workflow/fan-out.md`.
</role>

## Toolchain

This project decompiles Mario Golf 64 (N64) one function at a time. Tooling: splat (file splitting),
KMC GCC 2.7.2 (`tools/cc/gcc`), m2c (`tools/m2c`; the seed-body generator, driven with a Ghidra-typed
struct context), asm-differ (library mode), decomp-permuter (escalation), the Ghidra MCP bridge (live
decompile + struct/symbol lookup, port 8089; every call passes `program="baserom.z64"`), and coddog (`make coddog-sweep`: fingerprints MG64 fns against
ultralib VERSION_J to reveal an un-named `func_`'s upstream source; see
`docs/hazards.md#coddog-cross-ref`).

The always-loaded wrappers are `CLAUDE.md` and `AGENTS.md`, and their routing table names the file for
each phase. Detail lives in on-demand references: `docs/hazard-index.md` maps each
`pick_target.py` flag to its `docs/hazards.md` playbook, `docs/coding-style.md` holds the C quality bar
for code promoted into the tree, and `docs/levers.md` indexes the named source levers.

Prompt authoring for this workflow follows the project style in `docs/prompt-style.md`.

## Execution loop (inline)

After `sprint-plan` writes `SPRINT.md`, work the committed backlog autonomously, smallest-first.
Ghidra MCP is used inline at seed time. For each target function:

1. **Resolve** the function's subseg in `mariogolf64.yaml`. `hasm` means permanent asm: skip it. The
   subseg was already flipped to `c` at the gate.

2. **Mirror branch** (the `upstream` column is a lib, not `none`):
   a. Copy the upstream `.c` verbatim to its mirrored `src/lib<name>/...` path.
   b. Add any missing headers verbatim under `include/lib<name>/...`.
   c. `clang-format-22 -i` the copied files (these trees are now formatted; formatting does not
      change codegen).
   d. `make`; the ROM SHA-1 must equal the baserom. The proof is the full-`make` SHA-1
      (verbatim-then-formatted copy plus green ROM equals match); no byte-`cmp`. See
      `docs/hazards.md#upstream-mirror-pattern`.
   e. Before declaring a clean mirror, reconcile the upstream's call and data-ref list (including one
      level of macro expansion) against the name files, and handle any flagged hazard via the index
      below.
   - **Note, libultra source pin.** libultra source is `~/development/repos/ultralib` (gcc.mk /
     KMC-N64 profile, `-DBUILD_VERSION=VERSION_J`); use it, not `libultra_modern` (deprecated 2.0L,
     casts diverge; it is an `additional working dir` but is not the source). This pin holds for both
     the C mirror and the hand-asm `intrinsic-likely:<tu>.s` asm-mirror
     (`docs/hazards.md#asm-mirror-vendoring`): the project mirrors ultralib's `gcc.mk` profile
     (`LIBULTRA_CFLAGS` for C, `LIBULTRA_ASFLAGS` for vendored asm TUs, both in `mk/libultra.mk`).
     libkmc is `~/development/repos/libkmc`; libnusys is the n64sdkmod nusys tree. The vendored
     `include/libultra/**` headers can diverge from this pin (an inverted/wrong-value macro):
     `tools/audit_libultra_headers.py` macro-diffs them vs ultralib (see
     `docs/hazards.md#vendored-header-inversion`).

3. **Classical branch** (no upstream, or a hazard routes here): seed, iterate, spot-check, finalize.
   - **Seed** `nonmatchings/<func>/base.c` with the combined m2c-body + Ghidra-typed-context seed.
     m2c translates the asm into the compiled seed body, typed by a Ghidra-MCP struct context; the
     Ghidra decompile drops to a shape/type reference and the asm stays the sole authority (see
     `docs/hazards.md#decompile-vs-asm-authority`). Fetch three things from Ghidra MCP: the asm
     (`disassemble_function`), the decompile (write it to `nonmatchings/<func>/ghidra.c`), and the
     RE'd struct defs plus signatures. Write the struct context to `nonmatchings/<func>/ghidra_ctx.c`
     = `#include "common.h"` (OS structs + `os*`/nusys externs) + the RE'd struct defs + best-guess
     `extern`s for the pack's game callees and globals. Then run `venv/bin/python3 tools/seed_c.py
     --func <placeholder> --parent src/<seg>.c`, which drives the vendored `tools/m2c` (target
     `mips-gcc-c`) with the Ghidra struct context and the parent types, and emits m2c's output as the
     body. Ghidra often has not named a game-specific control struct (it shows loose stack vars +
     stock OS structs), so the RE'd struct may live in a sibling decomp, not the Ghidra db — check
     both. m2c keeps a rodata string as an `extern D_<addr>` data ref (ideal for a partial one-tu: no
     carve). Rename m2c's `temp_*`/`local_*`/`arg_*` synthetics and reconcile its extern preamble
     against the concrete externs during Iterate.
     - **When the seed body is not m2c.** seed_c.py's JSON reports `body_source`. If m2c fails
       (`body_source: "ghidra"` or `"stub"`), the body falls back to the sanitized decompile or a TODO
       stub; translate from the inlined asm block. A large auto-generated parent context can exceed
       m2c's parser, so a clean `ghidra_ctx.c` is the reliable context, not `--parent` alone.
     - **Two recurring seed-refinement levers.** (a) A bounds-clamp array index wants the ternary
       `(idx < N) ? idx : 0` form (branchless `sltiu`/`negu`/`and`); the m2c-emitted
       `&arr[idx & -(idx<N)]` form branch-folds the mask into a `beqz`. (b) An extern-symbol-base
       pointer return with a variable index wants stepwise pointer arith (`p = base + a*K; return p +
       b*K2;`), not a flat `a*K + base + b*K2` (GCC reassociates the flat form; see
       `docs/hazards.md#loop-weight-and-live-length-regalloc-steering` Axis 6).
     - **Provenance.** S186 seeded the whole `lz_compress_extended_dma.c` terrain-loader pack this way
       (context = common.h + the sibling `lz_decompress_simple.c`'s `LzDecompressState` + game
       externs); the call-glue seeds were byte-faithful first-build. S191 confirmed the recipe on a
       classical main logic pack (`get_table_entry.c`: context = common.h + the Ghidra-db
       `TerrainAttrEntry` + a synthesized 0xB8 `ShotInitRecord` + game externs; all 3 standalone fns
       banked, and both levers above surfaced there).
     - **asm-first seed fast-path (MCP-independent, for small fns ~<40 instrs).** The splat
       `.s` is the same ground truth as
       `disassemble_function`, so a small classical fn does not need MCP: hand-translate straight from
       the `.s` (resolve callee/global names + types from the name files / call-site arg setup),
       write the body directly into `src/<seg>.c` (declare each `extern`; auto `func_`/`D_` symbols
       resolve from their home subseg), and gate on the full-`make` ROM SHA-1. Skip
       seed_c.py/base.c/decomp_loop entirely unless the first build misses, then drop into the
       isolated Iterate loop below. Use this when Ghidra MCP is unavailable (`list_instances` empty)
       or the fn is small enough that the decompile adds no shape/type value. The fast-path banked a
       2-fn 176B pack this way, first build, MCP down (S148).
       - **The `.s` sits under the host subseg's directory, not the function's own.** The path is
         `asm/nonmatchings/<seg>/<subseg-stem>/<func>.s`, where `<subseg-stem>` is the lead-fn
         placeholder that names the whole subseg. For a loose stub inside an already-`c` file, which
         is now the main-segment default, a per-function directory does not exist at all.
       - **Per-fn oracle for the fast-path = `objdump -d` of the fresh object, not `diff.py` (S244).**
         With no base.c/decomp_loop, iterate on register/scheduling diffs by rebuilding just the one
         object and disassembling it: `find build -name '<obj>.o' -delete && make build/<path>/<obj>.o
         && mips-linux-gnu-objdump -d build/<path>/<obj>.o | awk '/<fn>:/,/<next>:/'`. The object's
         `%hi/%lo` show as `0x0` (unresolved) but the register allocation, instruction order, and
         immediates are the ground truth. `diff.py` reads `build/*.map` which an incremental per-object
         build does not refresh, so it lies (S244 `func_80079940`: diff.py showed byte-clean while the
         fresh object was dual-base-split). Use `diff.py` only right after a full `make` (map + objects
         consistent) for the reloc-resolved view; gate every bank on `tools/verify-rom.sh`. See the
         memory `docs/levers.md` (subagent diff crack not a bank).
   - **Iterate** at most 25 times: `venv/bin/python3 tools/decomp_loop.py --func <placeholder>`, then
     parse the JSON. `score == 0` is a candidate; 5 consecutive `compile_ok == False` means a broken
     seed, so stop; otherwise read the top mismatches, edit `base.c`, and re-run. Run the permuter
     (`./run-permuter.sh`) only at `percent >= 0.97`. For a near-miss whose only diff is which scratch
     register holds an intermediate, see `docs/hazards.md#register-reuse-nudge-classical-regalloc`.
     Before the permuter, rule out two source-typing fixes that both close without it (S162): a
     scheduling miss on a global load/store (pipelined vs strict-`$f0`-pairs, or a hoisted `& K` flag
     load) is often the `docs/hazards.md#mem-in-struct-scheduling-lever` (retype the fixed global as a
     struct/array member); and a full-make SHA-miss where a same-file sibling reads a wrong data
     address is `docs/hazards.md#short-text-shifts-flowing-bss` (a length miss shifts the flowing
     `.bss`, so fix the short fn, not the sibling). And rule out a missing callee prototype: a
     callee with no `extern` prototype in scope makes gcc assume implicit-int, which flips
     regalloc/scheduling (a base-materialize hoist across a `jal`) and reads as a scheduling wall —
     declare every callee with its real signature before reaching for the permuter
     (`docs/hazards.md#callee-prototype-is-load-bearing-missing-prototype--implicit-int`, S225).
     And rule out a GCC nested function before calling a leaf a `$v0`-arg wall or a dead-frame
     coin: a leaf that spills the incoming `$v0` with no reload (`addiu sp,-8; sw $v0,0(sp)` never
     reloaded) is the nested-function prologue homing the static chain (`STATIC_CHAIN_REGNUM =
     GP_REG_FIRST+2 = $2 = $v0`, mips.h:1310) — the chain is homed even when the child reads no
     parent variable (unused -> dead store; a chain-using child instead reloads `$v0` and reads its
     arg/data through it). The tell on the caller side is `addiu $v0,$sp,K` (address of a parent
     local = the chain) persisting to the `jal`. A clean nested child reproduces the exact bytes with
     no `volatile`/uninitialized contrivance; bank it inside its parent's TU as a nested function, or
     (if the parent inlined the call and left an orphaned out-of-line body with zero ROM `jal`/fn-ptr
     xrefs) carry the orphan with a standalone `volatile s32 x = <uninit>;` byte-repro stand-in. S248
     proved `func_8008E164`/`lerp_s32` is an orphaned nested child and re-priced the `func_80092E10`
     "$v0-arg wall" carry to a chain-used nested child (crackable once its parent is decompiled);
     see `docs/hazards.md#nested-function-static-chain-spill` and the memory
     `dead-frame-dead-v0-store-crack`.
   - **Spot-check** (only at score 0): byte-level `cmp` of the in-tree compiled `.text` against the
     isolated one. The `cmp` is the truth, not the mnemonic diff (see
     `docs/hazards.md#assembler-differences--byte-cmp-spot-check`). A non-zero score with empty
     `top_mismatches` and `match_count == total_rows` is an isolation artifact (struct-field reloc
     addend, or extern HI/LO16), not a near-miss: go straight to the in-tree spot-check and full-make
     SHA-1; do not iterate C or reach for the permuter (see `docs/hazards.md#isolated-compile-caveat`).
     **But this signal is only an isolation artifact at high percent** (a few reloc/HI-LO rows against
     an otherwise-matching body); at low percent (S205 `func_8005E380`: 0.48 with 535/535 rows +
     empty `top_mismatches`) it is a real pervasive near-miss whose per-row diffs asm-differ groups
     out of `top_mismatches` (e.g. a `#cse-derived-pointer-base-canonicalization` base-reg fold hitting
     every access). **Disambiguate with the in-tree `tools/asm-differ/diff.py <func>`** (diffs the real
     build against the ROM via the mapfile): if it too diverges, it is a genuine near-miss/wall — do
     Not apply the isolation-caveat shortcut, root-cause the pervasive diff instead.
     - **`diff.py` is an iteration hint, never a crack or bank verdict.** Its two staleness failure
       directions and the same-score-twice detector are stated once in
       `docs/workflow/loop.md ## Oracles`; the escape when it goes stale mid-iteration is the
       `objdump -dz` and `.s`-header rows of that same table.
   - **Finalize** (only if the spot-check passes): inline the body into `src/<seg>.c`, drop the
     `INCLUDE_ASM` line, `clang-format-22 -i` (now applies to every tree, including `src/libultra/`,
     `src/libkmc/`, `src/libnusys/`, and `src/mgu/`), then `make` until `build/mariogolf64.z64: OK`
     and SHA-1 == baserom.

4. **Bank** the function (only at score 0 plus spot-check plus full-`make` SHA match):
   a. Give it its curated Ghidra name: add to `symbol_addrs.txt`, rename in the body, re-`make`
      (`make extract && make` when the rename must reach still-asm callers). **Before renaming, grep
      already-committed C callers: `grep -rn '\bfunc_<addr>\b' src` (S247).** The `make extract` regen
      only rewrites still-ASM refs via `undefined_syms_auto`; a fn called from a committed C TU by its
      auto `func_<addr>` name will `undefined reference`-fail the link after the rename. If any C
      caller exists, either sed those files to the new name in the same commit or keep the auto name
      (a getter/predicate over a generic global read across unrelated subsystems is usually best left
      auto-named — a domain-guess is likely wrong; S247 kept `func_800959F8`).
      A rename reaching a still-asm caller has two failure modes at the gate make, both handled in
      `docs/workflow/loop.md ## Conventions` under the curated-rename bullet: a stale parent `.o`
      linking against the old
      name, and a splat gap relic whose `.s` will not regenerate at all.
   b. On `git commit`, stage the `make extract`-regenerated artifacts too (`undefined_syms_auto.txt`
      and `mariogolf64.ld`): they change on a subseg flip or `symbol_addrs.txt` add and must travel
      with the commit, or the regen bleeds into the next sprint's dirty tree (a `D_`-to-named
      BOOT_GLOBALS regen left uncommitted surfaced as a stray diff at the next gate, S85 to S86).
      `git status` should be clean after commit.
   c. Append a standup line to `SPRINT.md`, and record the run's numbered "Suggested workflow
      improvements" into the `SPRINT.md` buffer; apply these at `sprint-review` only, not
      mid-sprint.

A function that locks below `0.97 percent`, needs the permuter, or hits a BSS-layout or alignment
wall is a spike: note it, carry its file to `BACKLOG.md ## Carry-overs`, and move on. Hold the DoD
firm; a spike is carried, not banked. **Before declaring a same-TU inline mismatch (a callee my build
inlines but the ROM `jal`s, or the reverse) a permanent carry, build the matched reference games and
compare the source structure** (defn order, separate-helper vs hand-inline, config flags), not just the
bodies: `__MusIntMain` was twice wrongly written off as a compiler/same-TU wall, and both times
fell to a structural fix (a drain-before-dispatch helper + the `(s32)(a-b)<0` signed compare) found
by disassembling PPL's byte-exact `-O3` build (S147). See
`docs/hazards.md#same-tu-inline-mismatch-definition-order--cross-tu-split`. The increment (the
`src/<seg>.c` file) banks only when its last
`INCLUDE_ASM` stub is gone.

**Empty-leaf auto-C: stub-count < fn-count (S228).** `make extract` emits a body-less `void
f(void){}` directly (not an `INCLUDE_ASM` stub) for a 2-instr `jr ra; nop` leaf, so a freshly-opened
pack's `INCLUDE_ASM` count is below its function count by the number of such empties (S228
`func_800453E0.c`: 37 stubs for 40 fns — 3 empty leaves pre-C at scaffold time, free byte-matches). So
the matched-fn count is `fn-count − remaining-INCLUDE_ASM`, not the stub-count delta, and the plan
gate's "N stubs" figure is not the total-fn count. Grep `INCLUDE_ASM` for the true remaining work; count
the empty auto-C leaves toward banked.

The context window auto-compacts as it fills, so do not stop early on budget concerns; save progress
to `SPRINT.md` (standup line + suggestion buffer) as each function banks, so a fresh window resumes
from it.

## Oracles

**A function is banked only when `tools/verify-rom.sh` exits 0. Every other tool below is an
iteration signal.**

Each row states what a tool is authoritative for and how it fails, once. Do not restate a failure
mode as an imperative at each use site: the ROM SHA-1 is an external oracle, so a match cannot be
asserted, only measured, and re-check layers stacked on top of it buy nothing.

| Tool | Authoritative for | Known failure mode | Trusted when |
| --- | --- | --- | --- |
| `tools/verify-rom.sh [--extract]` | Whether a function or increment is banked. Full `make` plus SHA-1 against the `sha1:` key in `mariogolf64.yaml`. | None. It exists because a failed `make` leaves the previous `build/mariogolf64.z64` in place, so a bare `sha1sum` reads a stale green ROM. S111 burned a whole review that way, reporting a match across three commits that never built. | Always. The only bank gate. Never a hand-rolled `make ...; sha1sum`. |
| `tools/cmpfn.sh <fn> [<obj>]` | The per-iteration instruction stream and the instruction count of each side. Reads the object directly, so it never goes stale after an incremental `make build/src/<tree>/<obj>.o`. | Normalizes register prefixes, `%hi`/`%lo`, immediates, splat `(0xX >> 16)` spellings, `move`/`li` aliases, SDK FP register names and external branch/jal targets, so a difference in any of those is invisible. It collapsed internal branch targets until S260, where a back edge one instruction too far read byte-clean; and it normalized the frame immediate until S276, where a frame-size delta read as reg-perm-only. Both are surfaced now, as `@Dp<n>`/`@Dm<n>` and `[frame rom=.. mine=..]`. | As an iteration oracle, on a rebuilt object. A cmpfn-clean function is not a bank. |
| `mips-linux-gnu-objdump -dz` | Assembled ground truth: register allocation, instruction order, immediates, nop placement. | `-d` without `z` collapses runs of identical zero words to one `...` line, so a nop run under-counts. In an unlinked object `%hi`/`%lo` read `0x0`. | Any time, on a freshly built object. Always `-dz`; keep every line. |
| `tools/asm-differ/diff.py <fn>` | The reloc-resolved view of the linked build against the ROM. | Reads `build/*.map`, which an incremental per-object build does not refresh, so it fails in both directions: byte-clean on a dual-base-split object, and five diff rows on a byte-exact function while a mismatching one showed zero. The same score twice after a real source edit means stale, not "no effect" -- deleting the `.o` does not clear it, only a relink does. With no fresh build it spills the whole segment. | Immediately after a full `make`, for the reloc-resolved view only. Never as a crack or bank verdict. |
| `tools/decomp_loop.py --func` (`score`, `percent`) | The isolated per-function compile signal during Iterate. `score == 0` is a candidate. | A non-zero score with empty `top_mismatches` and `match_count == total_rows` is an isolation artifact at high percent, but a real pervasive near-miss at low percent. Five consecutive `compile_ok == False` means a broken seed. | During Iterate. Disambiguate the empty-`top_mismatches` case with `diff.py` right after a full `make`; if that diverges too, it is a genuine wall. |
| byte-`cmp` of raw `.text` | The spot-check at score 0: in-tree object against the isolated one. | A mnemonic diff false-positives because KMC `as` and modern `as` encode `move dst,zero` differently while objdump renders both as `move`. Inline LO16 struct-field addends differ in the object and resolve identically at link. | At score 0 only, on raw `.text` bytes. An addend-only difference goes straight to the full make. |
| decomp-permuter score | Nothing on its own. It searches; it does not decide. | Blind to internal branch targets: asm-differ normalizes a branch to a local label, so a pure back-edge residual scores zero falsely. `import.py` aborts on a nested function definition, disabling the permuter for that whole TU. `setup-permuter.sh` exits 0 with no output once the body is inlined as C -- call `import.py` directly with `--settings permuter_settings_main.toml`. | At `percent >= 0.97`, or at exact instruction count with a one-operand residual regardless of percent. Not on a multi-register permutation or an FP-schedule coin: those plateau, but a plateaued run's best candidate is still a lever (S287, see the escalation bullet). A permuter zero is confirmed by `verify-rom.sh`, never by itself. |
| Ghidra MCP decompile | Shape and type reference at seed time. | Not authoritative; the `.s` is. Overlay vram repeats above `0x801F4A30`, so seed overlay functions by ROM offset or the splat `.s`, not by vram. | As a shape and type hint. `disassemble_function` equals the splat `.s`, so a small function does not need MCP at all. |
| `head -1 asm/nonmatchings/<seg>/<f>/<f>.s` | The ROM-side byte size (`nonmatching <f>, 0x<size>`); instructions = size/4. | Sizing a leaf from adjacent-vram deltas is wrong in a multi-function pack, because curated-named functions interleave the `func_<vram>` ones. | Always, for both the instruction-count target and plan-gate sizing. |

Two notes that are two-tool interactions rather than properties of any one tool.

**Residual classification routes the lever.** From a fresh object: a differing count is a structural
deficit, so fix that before anything else; an exact count with differing registers is a coloring or
`local-alloc` question; an exact count with differing order is a scheduler question. Do not argue
register pressure, live length or "needs an Nth register" from a body that is not at exact instruction
count -- two strongly-worded terminal verdicts were refuted that way once their bodies reached exact
count. The register count is itself a structural symptom, so "my build uses one more callee-saved
register than the ROM" is never on its own evidence of a coloring problem: an extra induction
variable, an aliasing exemption that lets loads clump, or a wrong return type each show up first as a
surplus register. Fix the structure, then re-count.

**A `docs/wip/<fn>.near-match.md` records a hypothesis, not a measurement.** Its stated instruction
count, its residual class, and even its reading of the function's signature have each been wrong.
Re-derive with `venv/bin/python3 tools/pick_target.py --refresh-residual <fn>` before acting on one. A
register written in a branch delay slot before its first read is a shared pre-branch statement, not an
argument.


## Conventions (every-sprint)

These apply regardless of hazard. Hazard-specific procedures are in `docs/hazards.md` (see the index
below).

- **One function at a time.** `pick_target.py` ranks (smallest-first); you pick the target.
- **Never rewrite a partial-bank `src/<seg>.c` with a scripted whole-region splice (S257; recurred
  S259).** The rule below was already written and was still violated — reverting one function to
  `INCLUDE_ASM` with an `s[:i] + new + s[j:]` splice deleted two banked one-line siblings
  (`func_8006D1FC`, `func_8006D208`) that happened to sit between the anchors, surfacing only as an
  `undefined reference` at link. What was missing is a mechanical guard, so: **use `Edit` with an
  exact `old_string`. If a script is genuinely needed, assert both counts across the rewrite** —
  `grep -c 'INCLUDE_ASM'` (must change by exactly the number of functions promoted, 0 for an in-place
  body edit) and the file's function list, diffed before and after. The original S257 occurrence was
  the same mechanism in the other direction: a splice between two anchors silently deleted three
  `INCLUDE_ASM` stubs and their multi-line carry comments. Both times the anchors looked adjacent in
  the author's head and were not adjacent in the file.
- **Never `rm -rf` an `asm/nonmatchings/<seg>/<stem>/` directory (or bulk-delete its `.s`) to "force
  a regen" (S271, hard rule).** splat's `c`-mode `make extract` does not reproduce every still-asm
  stub: it leaves disassembly gaps at some curated / decompose-split function addresses, so those
  `.s` are stale-persistent relics the build depends on — and `asm/nonmatchings/` is gitignored, so a
  deleted relic is not recoverable with `git`. S271 `rm -rf`'d `bgm_load_song_from_rom/` chasing a
  stale-`.o` link error and destroyed 6 curated stubs (`init_per_player_state`,
  `gen_terrain_detail_texture`, `init_terrain_vertex_texcoords`, `emit_per_phase_fog_state`,
  `emit_terrain_state_prefix_block`, `emit_course_terrain_dl`) + `func_8005DDAC` that `make extract`
  would not regenerate. This is the `#stale-parent-asm-relic` / `#stale-top-level-asm-label-sync`
  hazard class made fatal. To refresh a stub, `Edit` it or delete only the single `.s` you will then
  re-verify — never a directory. If a relic is already lost, recover it with
  `tools/recover_stub.sh <0xsubseg-off> <fn>...` (flips the subseg `c`->`asm` so splat disassembles
  the full range, carves the byte-identical per-fn block back, flips to `c`); see
  `docs/hazards.md#stale-persistent-nonmatchings-relic-recovery`.
- **A curated rename that reaches a still-asm caller: force-delete the whole tree's objects before the
  gate make.** `make extract` regenerates the caller `.s` with the new name, but the incremental build
  links a stale `.o` still carrying the old auto name (`undefined reference`). Run
  `find build -path '*/src/<tree>/*.o' -delete` for the whole tree, e.g. `src/main`, rather than
  guessing at parents: a single flag rename has hit several stale parents at once.
  - If a still-asm caller's `.s` does not pick up the new name on `make extract` at all, it is a splat
    disassembly gap relic, not a stale object. Recover it with
    `tools/recover_stub.sh 0x<parent_subseg_off> <caller_fn>`, then `make extract && make`. Do not
    `rm` it: `asm/nonmatchings/` is gitignored, so a deleted relic has no `git restore`.
  - Provenance: S270 the single-parent case; S271 generalized it to the whole tree; S281 the
    gap-relic variant.
- **Scratch dir** `nonmatchings/<func>/` (gitignored, shared with the permuter).
- **Python tools run via the venv:** `venv/bin/python3 tools/X.py` (system python lacks asm-differ
  deps and is PEP-668-locked).
- **Per-function iteration oracle: `tools/cmpfn.sh <func> [<object>]` (S258).** Diffs the extracted
  `asm/nonmatchings/**/<func>.s` against a freshly built object, normalising register prefixes,
  `%hi/%lo`, immediates, the splat `(0xX >> 16)` spellings, `move`/`li` aliases (including
  `beqz`/`bnez` and their branch-likely forms), the SDK FP register names (`fv0`/`fs1` vs `f0`/`f22`),
  and external branch/jal targets, so only real differences show. Its first line is the instruction
  Count of each side — the most actionable number when a body is structurally right but the wrong
  length. Unlike `diff.py` it reads the object directly, so it never goes stale after an incremental
  `make build/src/<tree>/<obj>.o`. It is an iteration oracle only: every bank still gates on
  `tools/verify-rom.sh`.
  - **Internal branch targets are position-relative deltas, not a placeholder (S260 fix).** Before
    S260 the tool collapsed every branch target to `T`, so it could not see a redirected back edge and
    reported such a function byte-clean: S260 `collect_keyframe_events_at` was a `cmpfn`-clean 54/54
    whose loop back edge went one instruction too far and broke the full-make ROM. It now rewrites a
    `.L<vram>` (asm side) or `<fn+0xNN>` (object side) target to a signed `@Dp<n>`/`@Dm<n>` distance in
    instructions, so a redirected edge shows as a real diff (`@Dm14` vs `@Dm13`) while a correct
    internal branch cancels cleanly. Even so, `cmpfn` is an iteration oracle: a `cmpfn`-clean function
    is not a bank until `tools/verify-rom.sh` (full-make ROM SHA-1) says so.
- **Shared tool helpers** live in `tools/decomp_common.py` (venv re-exec, path constants, asm/symbol
  regexes, `emit`/`log`, `find_segment`, SDK-path config) and `tools/lib.sh` (shell wrappers).
  `make test-tools` runs the `tests/tooling/` characterization suite (pytest); refactor tooling under
  it, and use `REGEN_GOLDEN=1` to refresh golden snapshots after an intended behavior change.
- **File-level matching.** A source file matches only when every function in it matches. Promoting a
  match replaces one `INCLUDE_ASM` stub with the C body. No md5 win until the last stub in a file is
  gone.
- **`symbol_addrs.txt` is add-only; never delete an entry.** The agent may add maintainer-override
  entries: a function is `name = 0x<vram>; // type:func`; a data extern is `name = 0x<vram>;
  // size:0x<n>` (no `type:func`). `reloc_addrs.txt` is never touched. `ghidra_symbols.txt` is owned
  by `sync_decomp_names.py` (modify only via `make sync-names`, never hand-edit). The two name files
  must stay disjoint: never add a `symbol_addrs.txt` entry for an address already in
  `ghidra_symbols.txt` (splat errors on duplicates).
- **yaml authority.** Agent-editable in `mariogolf64.yaml`: subseg flip/split/path-qualifier lines.
  Leave the segment and memory-map structure, `mariogolf64.ld`, and segment boundaries unchanged.
  Subseg flips, multi-file splits, and the `make extract` that regenerates the scaffold are gate
  actions (or inline when a split is needed mid-flight).
- **Game/overlay-code path convention (the classical endgame).** A flipped non-lib subseg is
  pathed `<tree>/<stem>`: main-segment game code under `main/<stem>` (e.g. `main/func_80099490`),
  overlay code under `overlay_<N>/<stem>` (e.g. `[0x1508E0, c, overlay_10/func_ovl10_801F4A40]` ->
  `src/overlay_10/func_ovl10_801F4A40.c`), `<stem>` being the lead-fn placeholder (`func_<vram>`) (S148).
  No per-tree `mk/*.mk` fragment is needed: the generic `mk/src.mk` rule (`%` spans slashes) builds
  any `src/<tree>/%.c` with the default -O2 game profile (`C_PROFILE_CFLAGS = $(CFLAGS)`; the
  `mk/lib*.mk` overrides are more-specific and win only for their own `src/lib*/` trees). So a brand-
  new `src/overlay_<N>/` tree builds with zero mk edits for plain game code (but a DL TU needs the
  F3DEX2 profile, and a boot/SDK-glue TU an -O0 override — the two mk exceptions below). Overlay vram
  is reused across overlays (>=0x801F4A30 is ambiguous; see the Ghidra-map memory), so seed overlay
  fns by ROM offset / the splat `.s`, not MCP-by-vram.
  - **The `main` tree needs the F3DEX2 build profile for display-list code.** MG64 is F3DEX2
    (gspF3DEX2.fifo), so a `src/main/` game TU that builds display lists needs `-DF3DEX_GBI_2` to get
    the F3DEX2 GBI opcodes (`G_RDPHALF_1=0xE1`/`G_RDPHALF_2=0xF1`, not F3DEX `0xB4`/`0xB3`). This is a
    standing profile (not a per-file override like -O0): `mk/main.mk` sets
    `MAIN_CFLAGS = $(CFLAGS) -DF3DEX_GBI_2` and `$(BUILD_DIR)/$(SRC_DIR)/main/%.o: C_PROFILE_CFLAGS =
    $(MAIN_CFLAGS)` (included after `mk/libmus.mk`, before `mk/src.mk`) (S151). Non-DL main/ code is
    unaffected (the define only changes gbi.h/sptask.h consumers). See
    `docs/hazards.md#display-lists` (the DL reconstruction workflow + the mask-narrowing lesson).
  - **Exception: nusys/SDK-template main-segment code goes to its library tree, not `main/`.**
    A main-segment subseg that is actually a game-embedded copy of an SDK file (the `idle=nuboot`
    coddog tell on `nuboot.c`; a `nuSc*`/`nu*` template) is pathed under its library tree,
    `libnusys/<file>` (not `main/<stem>`), so it sits with the rest of that library. The placement is
    unchanged by this: libnusys subsegs already interleave the main segment by path qualifier, so the
    carve is a yaml path-qualifier edit only (`[0x.., c, main/<stem>]` -> `[0x.., c, libnusys/<file>]`),
    the `.text` stays at its vram. (`pick_target.py` could route the `idle=nuboot`/nusys-template tell
    to the library path — a tracked follow-up; for now the gate applies this by reading the coddog
    tag.) S149 carved `main/main` -> `libnusys/nuboot`.
  - **A per-file -O0 override is the one mk edit a game/SDK-glue tree may need.** Most game/
    overlay code is -O2 (above), but a boot/SDK-glue TU can be -O0 (the codegen tell: fp kept, no CSE,
    unused-arg spill; see `docs/hazards.md#-o0-bootsdk-glue-file-profile`). Add a file-specific
    override (`$(BUILD_DIR)/$(SRC_DIR)/<tree>/<file>.o: C_PROFILE_CFLAGS := $(subst -O2,-O0,$(CFLAGS))`),
    never a `<tree>/%.o` pattern (the siblings stay -O2). S149 `libnusys/nuboot.o` overrode to -O0 in
    `mk/libnusys.mk`, beating that fragment's `libnusys/%.o` -O2 pattern.
- **`hasm` subsegments stay raw asm forever** (entry stub, `__muldi3` libkmc math module, RSP
  microcode entries). The execution loop refuses these. The project sets `hasm_in_src_path: True`, so
  a `hasm` `.s` lives under `src/<dir>/<stem>.s` (each `hasm` yaml line carries a `<dir>/<stem>` name
  qualifier) and its object builds to `build/src/<dir>/<stem>.o` via a path-based pattern rule in the
  `mk/*.mk` fragments (the top `Makefile` is a thin skeleton that `include`s them) that picks the
  assembler by tree (KMC for `src/libultra/` and `src/libkmc/`, modern GAS for the rest).
- **Permuter** (`./run-permuter.sh`) runs only when asm-differ's `percent` is at least 0.97. Below
  that, iterate on C or reconsider whether the subseg should be `hasm`.
  - **But an exact instruction count plus a one-operand residual is a permuter target regardless of
    what `percent` reads (S258).** The 0.97 gate exists to keep the permuter off structurally-wrong
    bodies; once the instruction count matches the ROM and only a register or operand choice
    differs, it is the right tool even at a lower percent. S258 ran four: `func_800824E4` (20/20
    instrs, one differing operand) hit score 0 in 72 iterations with a spelling no hand-iteration
    produces (`b = shade; b = r - b;`, breaking a cse equivalence class between two registers holding
    the same constant), while the three larger permutations all plateaued (`func_80087CB0` 480->265
    in 60k, `func_80088A90` 870->520 in 31k, `init_sky_pool_and_world_state` 615->545 in 91k). So the
    payoff shape is exact-count-plus-one-operand; a multi-register permutation is not.
  - **On a register permutation the permuter's deliverable is a lever, not a zero.** Do not
    wait for score 0 and do not read a plateau as terminal: read the best candidate's *source* diff.
    S287 `func_8005C674` was hand-plateaued at 245/245 with 14 differing operands in three clusters;
    the permuter never scored 0 (base 515, best 70 in 130k iterations, 60 on a re-import), but its
    best candidate differed from `base.c` by one line -- it had assigned a subscript into an existing
    local before the test. Applying that reuse by hand cut the residual to 6 operands, and
    generalizing it (`docs/levers.md` variable-reuse-is-a-per-register-lever) closed the function in
    three more builds. So run it, then
    `diff nonmatchings/<fn>/base.c nonmatchings/<fn>/output-<best>-1/source.c` and generalize what it
    did, discarding its `volatile`-on-a-prototype style hacks. This is the actionable half of the
    S282 bound that the guaranteed deliverable is {crack or pass-cited verdict}.
  - **The permuter (asm-differ) is blind to internal branch targets — a permuter score of 0 on a
    pure-branch-target residual is a false positive (S261).** asm-differ normalises a branch to a
    local label and does not distinguish `bne …,<label@0x7c>` from `bne …,<label@0x80>`, the same
    blind spot S260 fixed in `tools/cmpfn.sh`. S261 imported the exact 54/54
    `collect_keyframe_events_at` body; the permuter reported `base score = 0` / "Found zero score!"
    while the real object was `fff2` and the ROM `fff1` (its own target.o was correctly `fff1`). So
    when the sole residual is an internal back-edge/branch target (not an instruction or a register
    choice), the permuter cannot score it — do not trust a permuter 0/low there; gate on `objdump` or
    `tools/verify-rom.sh`. (This is a distinct failure mode from the stale-object one in
    `docs/levers.md` (subagent diff crack not a bank): here the permuter's oracle is correct but its scorer is
    blind.) A back-edge-target residual is a gcc first-load-peel coin (see the same-field-peel entry in
    `docs/hazards.md`), not a permutation.
  - **A recorded "below the 0.97 gate, not permuter-eligible" verdict belongs to the body that was
    measured, not to the function (S259).** `func_8006CE88` carried "isolated score 5360 (pct 0.553)
    ... Not permuter-eligible here"; that percent was measured on a body with a 2-instruction
    structural deficit. Fixing the deficit took it to an exact instruction count, after which the
    permuter's base score was 55 and it banked the same session. Re-measure after every structural
    fix — count reaching exact, a loop shape corrected, an addressing form matched — before quoting
    an old percent to rule the permuter out.
  - **Banking a GCC nested function makes the permuter unavailable for the whole TU.** `import.py`
    runs pycparser, which aborts on a nested function definition (`Syntax error in base.c ... before:
    {`), so no other function in that file can be imported either. Workaround: copy the TU with the
    nested-function parent deleted, place the copy inside the repo (import.py rejects a path outside
    the project root — "Can't find root dir of project!"), and import from that.
  - **`setup-permuter.sh` resolves the C file by grepping for an `INCLUDE_ASM` stub, so it fails
    silently (exit 0, no output) once the body is inlined as C.** Call
    `venv/bin/python3 ./tools/decomp-permuter/import.py --settings permuter_settings_main.toml
    <c-file> <asm-file>` directly instead. The import copies the source into `nonmatchings/<fn>/`, so
    the tree can be reverted to `INCLUDE_ASM` (keeping the ROM green) immediately while the permuter
    runs in the background.
- **Decomp is authoritative for names** (per the Ghidra-workspace
  `docs/re/coordination/decomp_coordination.md`).
- **Match finalization is three steps:** inline the body into `src/<seg>.c`, `clang-format-22 -i`
  (now applies to every tree, including `src/libultra/`, `src/libkmc/`, `src/libnusys/`, and
  `src/mgu/`), then full `make` until ROM SHA-1 matches.
  Spot-check passing is not ROM matching; the final `make` proves the match. Gate the `sha1sum` on
  `make` succeeding (confirm the `build/mariogolf64.z64: OK` line first): a failed link leaves the
  previous `.z64` in `build/`, so `sha1sum` on a stale ROM false-positives. **Verify every ROM with
  `tools/verify-rom.sh [--extract]`, not a hand-rolled `make ... ; sha1sum`**: the helper asserts
  the `OK` line before trusting the hash. S111 burned a whole review on a hand-rolled ungated
  `sha1sum` that read a coincidentally-green stale ROM and reported match across 3 commits that never
  built (a missing `VI_CTRL_ANTIALIAS_MODE_0` define plus 2 unresolved carve symbols).
- **Clean-rebuild when an enabler edits a shared vendored header.** The build tracks no header deps,
  so an incremental `make` recompiles only the file you touched, not the other consumers of a header
  you changed. When a mirror or enabler edits a widely-included header (e.g.
  `include/libultra/PR/os_version.h`), the banking SHA-1 must come from `make clean && make extract &&
  make`, not an incremental build (`docs/hazards.md#clean-rebuild-after-shared-header-edit`).
- **Library code under `src/libultra/`, `src/libkmc/`, `src/libnusys/`, `src/libnaudio/`,
  `src/libmus/`, and `src/mgu/` (plus the audio-lib include trees `include/libnaudio/`,
  `include/libnualstl/`, and `include/libmus/`) is clang-format-22 formatted** (stock Google with `SortIncludes: Never`; each dir carries a local
  `.clang-format` = `BasedOnStyle: Google` + `SortIncludes: Never`, which supersedes the old
  `DisableFormat: true`). These trees were reworked (2026-06-23) to the Code Complete ch31/ch32
  layout + comment style and deliberately diverge from the upstream source formatting, so they no
  longer line up line-for-line against the vendor `.c` for visual cross-referencing; the ROM stays
  byte-identical (comments/whitespace do not affect codegen, and coddog's fingerprint is asm-based,
  not source-text). Format any new or edited file in these trees with `clang-format-22 -i`, like the
  rest of the tree. `src/mgu/` (S103) holds the game-embedded ultralib gu/mgu matrix source (the
  Monegi variant, compiled at the game `-O2` profile, not the libultra `-O3` band; see
  `docs/hazards.md#game-region-mirror--o2-profile`).
- **Vendored-header placement (PO directive, S129).** When a mirror needs headers vendored, split them
  by the SDK's own public/internal layout: a public header (the SDK's `include/` side, what a
  consumer `#include`s) goes to `include/<lib>/`; a source-private/internal header (the SDK's
  `src/` side) goes to `src/<lib>/` mirroring the include sub-structure. Both the `include/<lib>/`
  and `src/<lib>/` audio trees carry a local `.clang-format` = `BasedOnStyle: Google` +
  `SortIncludes: Never` and are `clang-format-22` formatted (the audio-lib include trees
  `include/libnaudio/`, `include/libnualstl/`, and `include/libmus/` were reworked to the Code
  Complete ch31/ch32 style on 2026-06-29; the prior "include trees stay verbatim/unformatted" default
  no longer holds for them). `SortIncludes: Never` is load-bearing: stock Google sorts `#include`s and
  can break a byte-exact match. When an internal header shares a name with an existing one on the `-I` path (e.g.
  the n_audio_sc `synthInternals.h` vs `include/libultra/internal/synthInternals.h`), the build
  profile prepends `-I src/<lib>` so the vendored SC copy wins (`mk/libnaudio.mk`, S129). Add the new
  tree's profile include dirs to `pick_target.py`'s `LIB_EXTRA_INCLUDE_DIRS`/`INCLUDE_DIRS` so its
  band stops false-flagging `needs-header → blk`.
- **Use ultra64.h types** in decomp C (`s32`/`u64`/`vu32`/`f32`/...) for every integer and float;
  these replace raw `int`/`long long`/`volatile unsigned long`.
- **Prompt authoring.** Every prompt surface in this project (`AGENTS.md`, `CLAUDE.md`, `docs/*`,
  `.agents/skills/*`, `.claude/commands/*`, and artifact templates) follows `docs/prompt-style.md`
  when created or modified. Its conformance checklist is the gate to run before committing a
  prompt-surface edit.

### C naming (compact; full guide in `docs/coding-style.md`)

| Identifier            | Convention   | Example                |
| --------------------- | ------------ | ---------------------- |
| Function / variable / parameter / member | `lower_case` | `frame_count` |
| Struct / Enum (type)  | `CamelCase`  | `PacketHeader`         |
| Enum constant / macro / global+static const | `UPPER_CASE` | `MAX_CONNECTIONS` |

That table is the case convention. Naming quality (descriptive, problem-oriented, length-by-scope)
follows `docs/coding-style.md`. The m2c seed body is compiled, so rename its `temp_*` / `local_*` /
`arg_*` synthetics before promoting. This applies to classical / hand-authored C only; verbatim mirrors keep
their upstream names unchanged.

## Cross-repo sync (Ghidra workspace at `~/development/reversing/ghidra/mariogolf64/`)
