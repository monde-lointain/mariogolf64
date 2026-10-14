---
type: Workflow
title: Decomp execution details
description: Records matching procedure, tool authority, and technical conventions for decompilation.
---

# Execution details and every-function conventions

<role>
This reference preserves technical matching procedure and tool authority. The native
[`mg64-decomp` skill](../../.omp/skills/mg64-decomp/SKILL.md) owns candidate selection and the
select/iterate/bank/review workflow. Consult [fan-out guidance](fan-out.md) when delegation is useful.
</role>

**Navigate:** [Toolchain](#toolchain) · [Execution loop](#execution-loop-inline) ·
[Oracles](#oracles) · [Conventions](#conventions-every-sprint).

## Toolchain

This project decompiles Mario Golf 64 (N64) one function at a time. Tooling includes splat (file
splitting), KMC GCC 2.7.2 (configured by the build system), m2c (seed-body generation), asm-differ
(library mode), decomp-permuter (escalation), the Ghidra MCP bridge (decompile and struct/symbol
lookup; every call passes `program="baserom.z64"`), and coddog (see
`docs/hazards.md#coddog-cross-ref`).

Use the [documentation index](../index.md) to select hazards, source levers, C style, or
prompt-authoring guidance. [AGENTS.md](../../AGENTS.md) is the sole always-loaded project context.

## Execution loop (inline)

Read the tracked backlog and select an increment using the native decomp workflow. Use Ghidra MCP
inline at seed time where it adds value. For each target function:

1. **Resolve** the function's subsegment in `mariogolf64.yaml`. `hasm` means permanent asm: skip it.
   Flip an `asm` subsegment to `c` only when required for the selected source work.

2. **Mirror branch** (the `upstream` column is a lib, not `none`):
   a. Copy the upstream `.c` verbatim to its mirrored `src/lib<name>/...` path.
   b. Add any missing headers verbatim under `include/lib<name>/...`.
   c. `clang-format-23 -i` the copied files (these trees are now formatted; formatting does not
      change codegen).
   d. Run `tools/verify-rom.sh`; require exit 0. The full-ROM gate proves the
      verbatim-then-formatted copy; mirror banking needs no byte-`cmp`.
      See `docs/hazards.md#upstream-mirror-pattern`.
   e. Before declaring a clean mirror, reconcile the upstream's call and data-ref list (including one
      level of macro expansion) against the name files, and handle any flagged hazard via the index
      below.
   - **Note, libultra source pin.** libultra source is `~/development/repos/ultralib` (gcc.mk /
     KMC-N64 profile, `-DBUILD_VERSION=VERSION_J`); use it, not `libultra_modern` (deprecated 2.0L,
     casts diverge; it is an `additional working dir` but is not the source). This pin holds for both
     the C mirror and the hand-asm `intrinsic-likely:<tu>.s` asm-mirror
     (`docs/hazards.md#asm-mirror-vendoring`): preserve the profile in
     `tools/build_profiles.json`. libkmc is `~/development/repos/libkmc`; libnusys is the n64sdkmod
     nusys tree.
     `include/libultra/**` headers can diverge from this pin through an inverted or wrong-value macro;
     `tools/audit_libultra_headers.py` compares them with ultralib
     (`docs/hazards.md#vendored-header-inversion`).

3. **Classical branch** (no upstream, or a hazard routes here): seed, iterate, spot-check, finalize.
   - **Seed** `nonmatchings/<func>/base.c` with the combined m2c-body + Ghidra-typed-context seed.
     m2c translates the asm into the compiled seed body, typed by a Ghidra-MCP struct context; the
     Ghidra decompile drops to a shape/type reference and the asm stays the sole authority (see
     `docs/hazards.md#decompile-vs-asm-authority`). Fetch three things from Ghidra MCP: the asm
     (`disassemble_function`), the decompile (write it to `nonmatchings/<func>/ghidra.c`), and the
     RE'd struct defs plus signatures. Write the context to `nonmatchings/<func>/ghidra_ctx.c` as
     `#include "common.h"` (OS structs + `os*`/nusys externs), the RE'd struct defs, and best-guess
     `extern`s for the pack's game callees and globals. Then run
     `uv run --locked python tools/seed_c.py --func <placeholder> --parent src/<seg>.c`, which
     drives vendored `tools/m2c` (`mips-gcc-c`) with the Ghidra struct context and parent types.
     Ghidra often has not named a game-specific control struct; check sibling decompilations as well
     as the Ghidra database. m2c preserves rodata strings as `extern D_<addr>` references, ideal for
     a partial one-TU without a carve. Rename m2c synthetics and reconcile its extern preamble with
     concrete declarations during iteration.
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
     - **Provenance.** The recipe's context is `common.h` plus the RE'd struct plus game externs,
       whether the struct comes from a sibling decomp or the Ghidra db (S191 `get_table_entry.c`).
     - **Asm-first seed fast-path.** For a small function whose `.s` supplies enough shape and type
       evidence, hand-translate into `nonmatchings/<func>/base.c` and use the same isolated iteration
       and bank steps below. Ghidra and m2c are optional inputs, not prerequisite ceremonies.
       Find the stub under the host subsegment's directory, not a guessed per-function directory:
       `<binary>/work/asm/nonmatchings/<tree>/<subseg-stem>/<func>.s`.
   - **Iterate** with `uv run --locked python tools/decomp_loop.py --func <placeholder>`, then
     parse the JSON. `score == 0` is a candidate; 5 consecutive `compile_ok == False` means a broken
     seed, so stop; otherwise read the top mismatches, edit `base.c`, and re-run. Run the permuter
     (`./run-permuter.sh`) only at `percent >= 0.97`. For a near-miss whose only diff is which scratch
     register holds an intermediate, see `docs/hazards.md#register-reuse-nudge-classical-regalloc`.
     Before the permuter, rule out two source-typing fixes that both close without it (S162): a
     scheduling miss on a global load/store (pipelined vs strict-`$f0`-pairs, or a hoisted `& K` flag
     load) is often the `docs/hazards.md#mem-in-struct-scheduling-lever` (retype the fixed global as a
     struct/array member); and a full-build SHA-miss where a same-file sibling reads a wrong data
     address is `docs/hazards.md#short-text-shifts-flowing-bss` (a length miss shifts the flowing
     `.bss`, so fix the short fn, not the sibling). Rule out a missing callee prototype too: without
     an `extern` prototype, GCC assumes implicit-int, which changes register allocation and scheduling
     (`docs/hazards.md#callee-prototype-is-load-bearing-missing-prototype--implicit-int`, S225).
     Rule out a GCC nested function before calling a leaf a `$v0`-arg wall or a dead-frame coin.
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
     `docs/hazards.md#assembler-differences--byte-cmp-spot-check`). On a non-zero score with empty
     `top_mismatches`, read the `decomp_loop.py` row in the Oracles table below before
     iterating C or reaching for the permuter: whether that is an isolation artifact or a real
     near-miss turns on the percent, and that row states the test.
   - **Finalize** (only if the spot-check passes): inline the body into `src/<seg>.c`, drop the
     `INCLUDE_ASM` line, run `clang-format-23 -i`, then `tools/verify-rom.sh`; require exit 0.

4. **Bank** only after the full-ROM gate passes; classical candidates also require score 0 and
   the byte spot-check above.
   a. Add its curated name to `symbol_addrs.txt`, rename the body, and run
      `tools/verify-rom.sh --extract` when the rename must reach still-asm callers. Before renaming,
      inspect committed C callers for `func_<addr>` (S247): extraction rewrites still-ASM references,
      not C calls. Migrate any C caller in the same change or keep the auto name when a domain guess
      is uncertain.
   b. Let extraction regenerate link inputs in the binary tree; never hand-edit or stage generated
      extraction artifacts. The full-ROM gate catches link failures, including stale callers and
      unrecoverable Splat gaps.
   c. Keep task notes current enough to resume and distinguish banked work from measured carries.

A function that locks below `0.97 percent`, needs the permuter, or hits a BSS-layout or alignment
wall is a spike: note it, carry its file to `BACKLOG.md ## Carry-overs`, and move on. Hold the DoD
firm; a spike is carried, not banked. **A carry's `docs/wip/<fn>.base.c` is self-contained or it is
not a carry (S319).** Every type and inlined helper the host does not declare travels with it, in the
exact variant measured (two copies of S319's `lz_expand` differing by one statement's placement give
52 against 20 rows under an identical caller), and it is verified before the sprint closes by pasting
it into a clean tree and re-measuring `cmpfn`: S318's omitted a typedef and a `static inline`, so the
restore built a 189-instruction body at the wrong frame, reading as a collapsed function rather than
a missing declaration. Fallback: the `nonmatchings/<fn>-N/` permuter trees hold the original body. **Before declaring a same-TU inline mismatch (a callee my build
inlines but the ROM `jal`s, or the reverse) a permanent carry, build the matched reference games and
compare the source structure** (defn order, separate-helper vs hand-inline, config flags), not just the
bodies: `__MusIntMain` was twice wrongly written off as a compiler/same-TU wall, and both times
fell to a structural fix (a drain-before-dispatch helper + the `(s32)(a-b)<0` signed compare) found
by disassembling PPL's byte-exact `-O3` build (S147). See
`docs/hazards.md#same-tu-inline-mismatch-definition-order--cross-tu-split`. The increment (the
`src/<seg>.c` file) banks only when its last
`INCLUDE_ASM` stub is gone.

**Empty-leaf auto-C: stub-count < fn-count (S228).** Extraction may emit a body-less `void
f(void){}` directly (not an `INCLUDE_ASM` stub) for a 2-instr `jr ra; nop` leaf, so a newly extracted
pack's stub count can be below its function count by the number of empty leaves (S228
`func_800453E0.c`: 37 stubs for 40 fns — 3 empty leaves were already C). The matched-function count
is `fn-count − remaining-INCLUDE_ASM`, not the stub-count delta. Grep `INCLUDE_ASM` for true remaining
work and count empty auto-C leaves as matched.

Save concise task state as functions bank or become measured carries so a fresh session can resume.

## Oracles

**A function is banked only when `tools/verify-rom.sh` exits 0; every other tool below is an
iteration signal.**

Each row states what a tool is authoritative for and how it fails, once. Do not restate a failure mode
as an imperative at each use site: the ROM SHA-1 is an external oracle, so a match cannot be
asserted, only measured, and re-check layers stacked on top of it buy nothing.

| Tool | Authoritative for | Known failure mode | Trusted when |
| --- | --- | --- | --- |
| `tools/verify-rom.sh [--extract]` | Whether a function or increment is banked. Successful configured build plus SHA-1 against the value in `mariogolf64.yaml`. | A failed build must not leave a stale ROM that can be accepted; the verifier requires a successful build before hashing. | Always. The only bank gate. |
| `tools/cmpfn.sh <fn> [<obj>]` | Per-iteration instruction stream and instruction counts. | Normalizes registers, immediates, aliases, and external targets; its output is not a byte comparison. | During iteration only. |
| `mips-linux-gnu-objdump -dz` | Assembled ground truth: register allocation, instruction order, immediates, nop placement. | `-d` without `z` collapses runs of identical zero words; unlinked `%hi`/`%lo` read as zero. | On a freshly built object. |
| `tools/asm-differ/diff.py <fn>` | Reloc-resolved view of a linked build against the ROM. | Reads link maps, which object-only builds do not refresh; stale maps may misrepresent residuals. | After a fresh full build, for reloc-resolved iteration evidence only. |
| `tools/decomp_loop.py --func` (`score`, `percent`) | Isolated per-function compile signal. `score == 0` is a candidate. | Empty mismatch output can be an isolation artifact at high percent; repeated compile failures signal a broken seed. | During iteration, never as a bank verdict. |
| byte-`cmp` of raw `.text` | Spot-check at score zero: in-tree object against isolated object. | Mnemonic diffs may disagree because assemblers encode equivalent instructions differently; relocation addends may resolve identically at link. | At score zero, before integration. |
| decomp-permuter score | Nothing on its own. It searches; it does not decide. | Blind to internal branch targets: asm-differ normalizes a branch to a local label, so a pure back-edge residual can score zero falsely. It also tolerates an extra instruction. Use `./setup-permuter.sh <fn>` to import the isolated seed with the parent TU's configured flags; the wrapper selects the uv-managed project Python and needs no Makefile or hand-patched compiler script. Nested GCC functions require `tools/permuter_import.sh` to remove unrelated nested-function parents. | At `percent >= 0.97`, or exact instruction count with a one-operand residual. Not a multi-register permutation or FP-schedule coin. |
| gcc `-dL` (append to the file's real flags; writes `<file>.c.loop`) | Every `loop.c` decision: per-loop insn count, each biv's init value, per-movable `savings S (life L)` moved / not-desirable / `matches`, per-giv reduced or `not worth while, X vs Y`. | Whole-file; filter. Reports decisions, not a match. | Any hoisted-constant, biv or strength-reduced-address residual. Read `savings` and `lifetime` off it and compute loop.c:1631 or :3823; S297 estimated and was wrong twice. |
| gcc `-dg` (`.greg`; `-dl` gives `.lreg`) | An allocno's hard-register conflicts and preferences, the `;; N regs to allocate` priority order, and the `;; Register dispositions` pseudo-to-hard-reg map. | Whole-file; filter on `;; Function <name>`. Gives the order, not the arithmetic behind it -- that is `allocno_report.py`. | Whenever a residual is register names only: the order plus the map turns it into a priority-order diff (S318). Also when the order already favours the allocno that lost (S312: `n` conflicted with `$v0`, no preference, so no rewrite reached the ROM's register). |
| Ghidra MCP decompile | Shape and type reference at seed time. | Not authoritative; the `.s` is. Overlay vram repeats above `0x801F4A30`, so seed overlay functions by ROM offset or the splat `.s`, not by vram. | As a shape and type hint. `disassemble_function` equals the splat `.s`, so a small function does not need MCP at all. |
| `tools/dl_decode.py <fn>.s` (+ `tools/rdp_word.py`, `tools/combine_word.py`) | A display-list emitter's packet stream, in list order. A big emitter precomputes one `Gfx *` per packet and spills most of them, so `.s` order and list order differ by up to 40 packets (S303 `func_8002CDA8`). The word decoders turn a `<w0>/<w1>` pair into RDP fields / `gDPSetCombineLERP` args. | Constant-propagation only: a word built from a runtime value prints symbolically, and it cannot say which composite macro produced a block. Settle both on the host: `gcc -m32 -DF3DEX_GBI_2 -D_LANGUAGE_C -I include -I include/libultra -I include/libultra/PR` with local `_SHIFTL`/`_SHIFTR` and `G_ON`/`G_OFF` -- and pass `p++`, not `p`, since a composite macro re-evaluates `pkt` once per packet. | Before writing C for a DL emitter. |
| mnemonic histogram, `.s` against a fresh object (`grep -oE '  [a-z0-9.]+ '` piped to `sort \| uniq -c`, and its `objdump -dz` twin) | Whether a residual is structural or a permutation, in two calls and no build. | A multiset: says nothing about *where*. Normalise the alias pairs (`addu`/`move`, `li`/`addiu`) or it reports false deltas. | Before reading any `cmpfn` hunk. It found both S304 classes `cmpfn` normalisation hid: a branch-form divergence read as "pure allocation", and a `lui` +1 / `addiu` -1 naming an address-CSE defect behind a 491-row diff. |
| `head -1 asm/nonmatchings/<seg>/<f>/<f>.s` | The ROM-side byte size (`nonmatching <f>, 0x<size>`); instructions = size/4. | Sizing a leaf from adjacent-vram deltas is wrong in a multi-function pack, because curated-named functions interleave the `func_<vram>` ones. | Always, for both the instruction-count target and plan-gate sizing. |

Check `tools/` before hand-rolling a comparison: `seg_diff.py` localises a residual by region,
`gbi_match.py` resolves a DL word to its macro; both were re-invented in S306.

Two notes that are two-tool interactions rather than properties of any one tool.

**Residual classification routes the lever.** Count by region first, splitting at the loop-head
labels, and histogram mnemonics per region; on a permutation `cmpfn`'s hunk alignment is
meaningless, and `+5` against an adjacent `-5` is one displacement, not five missing insns (S297).
Then, from a fresh object: a differing count is a structural deficit, so fix that first; an exact
count with differing registers is a coloring or `local-alloc` question; an exact count with differing
order is a scheduler question. Do not argue register pressure or live length from a body not at exact
count -- two terminal verdicts were refuted that way once their bodies reached it. The register count
is itself a structural symptom, so "one more callee-saved register than the ROM" is never on its own
evidence of a coloring problem: an extra induction variable, an aliasing exemption that lets loads
clump, or a wrong return type each show up first as a surplus register. So does one pointer or index
spanning two loops -- split it per loop before reading priorities (S316). Fix the structure, then
re-count.

Once the count is exact and the residual is registers, the next action is named:
`uv run --locked python tools/allocno_report.py <src.c> <fn>` prints every allocno in
`global.c allocno_compare` order with its `n_refs`, `live_length` and the computed
`floor_log2(refs)*refs/live_length`, plus the `local-alloc` quantities by live length. Read the ROM's
register order off the `.s` and find where the two orders diverge; the arithmetic then says which fix
applies. A priority gap wants refs or live length changed (S296: one merged counter gave `frame` 21
refs and 5833 against the segment mask's 1875, rotating five registers; splitting it to 7 refs and
1068 landed all five). A tie wants allocno order changed, which block-scoping one of the pair does
(S296: `keyframe` and `keyframeIndex` both scored 6666). Shorten the web structurally first:
`break` routing a local `return var;` through a shared `return` moved `func_80052CF0`'s result past
two allocnos where the permuter never zeroed (S316). Not a last resort for the terminal permutation
class it was introduced for -- it is how you stop guessing.

**Write and read a `docs/wip/<fn>.near-match.md` in two halves: what was measured (counts, frame,
which registers and which ordering differ, what was built) and what was attributed to it (which
pass, why).** Only the first half is evidence. Re-derive with
`uv run --locked python tools/pick_target.py --refresh-residual <fn>`, then re-derive the attribution
before acting on it: S264's "combine `nonzero_bits`, source-invariant" coin was `fold_range_test`,
and S272 re-confirmed the wrong pass by re-testing the same three variants instead of the claim
(S315). Its lever rows are conditional the same way -- each was measured against one shape, so
record that base and re-run the rejected rows once the residual has moved (S302 banked
`func_8008D3F4` on a lever S301 had rejected).


## Conventions (every-sprint)

These apply regardless of hazard. Hazard-specific procedures are in `docs/hazards.md` (see the index
below).

- **One function at a time.** `pick_target.py` ranks; choose a target from measured evidence.
- **Source edits.** Use the active harness's anchored edit tool; scope each change to the target
  function and preserve banked siblings. The parent owns integration and shared-resource operations;
  see the [dispatch contract](fan-out.md#dispatch-contract).
- **Extraction.** Use the CMake extraction flow, not bulk deletion of generated assembly.
  `tools/extract_rom.py` recovers required missing stubs from full-range ROM disassembly without
  modifying curated YAML. Keep candidate evidence under `nonmatchings/` separate from build outputs.
- **Scratch dir** `nonmatchings/<func>/` (gitignored, shared with the permuter).
- **Stop a background command with the harness. Never `pkill -f` on any pattern that appears in the
  command you are running (S289, recurred S301).** The pattern also matches the tool call's own shell
  wrapper, so the kill takes out the calling shell and every later step in that call silently does
  not run. No form of the pattern is safe, not even a bare tool name: use the harness's stop action.
- **Python tools** use the shared uv-managed project environment:
  `uv run --locked python`.
- **Shared tool helpers** live in `tools/decomp_common.py` (venv re-exec, path constants, asm/symbol
  regexes, `emit`/`log`, `find_segment`, SDK-path config) and `tools/lib.sh` (shell wrappers).
- **File-level completion.** Promoting a match replaces one `INCLUDE_ASM` stub with its C body.
  A file is complete only after its last stub is gone.
- **`symbol_addrs.txt` is add-only; never delete an entry.** The agent may add maintainer-override
  entries: a function is `name = 0x<vram>; // type:func`; a data extern is `name = 0x<vram>;
- **Name-file ownership.** `reloc_addrs.txt` is never touched. `ghidra_symbols.txt` is generated by
  the supported name-sync workflow; never hand-edit it. Keep the two name files disjoint so splat
  does not encounter duplicate symbols.
- **YAML authority.** Agent-editable `mariogolf64.yaml` changes are limited to subsegment
  flip/split/path-qualifier lines. Leave segment and memory-map structure, `mariogolf64.ld`, and
  segment boundaries unchanged. Use `tools/verify-rom.sh --extract` for supported extraction.
- **Game/overlay paths and profiles.** A flipped non-library subsegment uses `<tree>/<stem>`:
  main-segment game code under `main/<stem>`, overlay code under `overlay_<N>/<stem>`. The stem is
  the lead-function placeholder (`func_<vram>`) (S148). Overlay VRAM is reused across overlays, so
  identify a target by ROM offset or the extracted `.s`, not MCP lookup by VRAM.
- Main display-list code uses the F3DEX2 profile (`-DF3DEX_GBI_2`); ordinary main code uses the
  standard game profile. SDK-template code embedded in the main segment belongs under its library
  tree (for example `libnusys/<file>`), not `main/`. A boot/SDK-glue file may require its own `-O0`
  file override. These requirements live in `tools/build_profiles.json`; preserve the matching profile
  rather than recreating legacy make-rule examples.
- **`hasm` subsegments stay raw asm forever** (entry stub, `__muldi3` libkmc math module, RSP
  microcode entries). Their source-layout qualifiers and assembler selection are controlled by
  `mariogolf64.yaml` and the build configuration. Inspect the generated build configuration instead
  of inferring object paths or assembler rules from legacy build files.
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
  - **On a register permutation the permuter's deliverable is a lever, not a zero.** Never wait for
    score 0, never read a plateau as terminal, and re-read the best candidate's *source* diff after
    **every** hand fix rather than once: `diff nonmatchings/<fn>/base.c
    nonmatchings/<fn>/output-<best>-1/source.c`. Read it for the knob it exposes, not the diff to
    apply -- a candidate can be semantically wrong, or cost instructions, and still name the
    mechanism; `tools/allocno_report.py` then says which legitimate perturbation reaches it. Discard
    its `volatile`-on-a-prototype hacks, and re-measure every candidate with `cmpfn` rather than
    trusting its score -- the two disagree often (S318: 4 of 8 candidates paid, worth 4-32 rows each,
    while three that improved the score were inert or worse applied). This is the actionable half of
    the S282 bound that the guaranteed deliverable is {crack or pass-cited verdict}. Case law, all
    with plateaued runs that still paid: S287 (a subscript assigned into an existing local before the
    test), S288 (two levers from two imports, the second visible only once the first reached exact
    count), S290 (a wrong retype that still landed the three permuted registers), S318 (three
    `do {} while (0)` regions and a mask-into-a-temp split). Tally: 8/13 candidates yield a lever,
    0/13 runs a zero.
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
  - **Import without poisoning the matching tree.** `./setup-permuter.sh <fn>` uses an existing
    `nonmatchings/<fn>/base.c` seed and derives the compiler profile from its source parent.
    For a prepared copy or explicit assembly input, use
    `python3 tools/rom_build.py import-permuter --source <parent.c> --input <candidate.c> --asm <target.s>`.
    Generated workspaces retain that parent profile. Keep the root source on its matching stub
    until the candidate is ready for the full-ROM gate.
- **Decomp is authoritative for names** (per the Ghidra-workspace
  `docs/re/coordination/decomp_coordination.md`).
- **Match finalization.** Follow the Finalize and Bank steps in the [execution loop](#execution-loop-inline).
  The [oracle table](#oracles) distinguishes the byte spot-check from the full-ROM gate.
- **Shared header edits.** CMake tracks compiler-discovered includes and rebuilds dependent objects.
  Run `tools/verify-rom.sh`; extraction is not required for a header-only change. A stale dependent
  is a dependency-graph defect to reproduce and fix, not a reason to routinely clean the tree.
- **Curated renames.** Extraction publishes changed assembly and regenerates the link inputs.
  Run `tools/verify-rom.sh --extract` after a curated metadata change. If an old reference survives,
  inspect the generated assembly and dependency graph before deleting outputs.
- **Library code under `src/libultra/`, `src/libkmc/`, `src/libnusys/`, `src/libnaudio/`,
  `src/libmus/`, and `src/mgu/` (plus the audio-lib include trees `include/libnaudio/`,
  `include/libnualstl/`, and `include/libmus/`) is clang-format-22 formatted** (stock Google with `SortIncludes: Never`; each dir carries a local
  `.clang-format` = `BasedOnStyle: Google` + `SortIncludes: Never`, which supersedes the old
  `DisableFormat: true`). These trees were reworked (2026-06-23) to the Code Complete ch31/ch32
  layout + comment style and deliberately diverge from the upstream source formatting, so they no
  longer line up line-for-line against the vendor `.c` for visual cross-referencing; the ROM stays
  byte-identical (comments/whitespace do not affect codegen, and coddog's fingerprint is asm-based,
  not source-text). Format any new or edited file in these trees with `clang-format-23 -i`, like the
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
  can break a byte-exact match. When an internal header shares a name with another on the include
  path, make sure the per-source profile orders the intended vendored copy first. Keep
  `tools/build_profiles.json` and candidate-ranking include-path assumptions aligned.
- **Use ultra64.h types** in decomp C (`s32`/`u64`/`vu32`/`f32`/...) for every integer and float;
  these replace raw `int`/`long long`/`volatile unsigned long`.
- **Prompt authoring.** Project instructions follow the concise guidance in `docs/prompt-style.md`.
  Root `AGENTS.md` is the sole always-loaded context; native Oh My Pi skill and command provide the
  decomp workflow entry points.

### C naming (compact; full guide in `docs/coding-style.md`)

Use the [C naming guide](../coding-style.md#c-naming-conventions) as the single convention.
Rename m2c synthetics before promotion when matching permits; upstream mirrors retain their names.

