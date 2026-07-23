# Mario Golf 64 Decomp agent workflow

<role>
Operate the Mario Golf 64 (N64) decompilation workflow, banking one matched function at a time
toward a byte-exact ROM. The ROM SHA-1 against the baserom is the sole arbiter of a match. Two
Scrum gates (`sprint-plan`, `sprint-review`) wrap an inline execution loop run autonomously,
smallest-first, in a single serial session. Follow the procedure that applies; every instruction here
supports the byte-exact oracle.
</role>

<context>

This project decompiles Mario Golf 64 (N64) one function at a time. Tooling: splat (file splitting),
KMC GCC 2.7.2 (`tools/cc/gcc`), m2c (`tools/m2c`; the seed-body generator, driven with a Ghidra-typed
struct context), asm-differ (library mode), decomp-permuter (escalation), the Ghidra MCP bridge (live
decompile + struct/symbol lookup, port 8089; every call passes `program="baserom.z64"`), and coddog (`make coddog-sweep`: fingerprints MG64 fns against
ultralib VERSION_J to reveal an un-named `func_`'s upstream source; see
`docs/hazards.md#coddog-cross-ref`).

This file is the shared workflow source: the workflow model, the Scrum cadence, and the every-sprint
rules. The concise always-loaded wrappers are `AGENTS.md` and `CLAUDE.md`. Detail lives in two
on-demand references:

- `docs/hazards.md`: the hazard playbooks (what to do when `pick_target.py` flags a hazard). The
  hazard index below maps each flag to its section.
- `docs/coding-style.md`: the C quality bar and naming conventions for code promoted into the tree.

Prompt authoring for this workflow follows the project style in `docs/prompt-style.md`.

</context>

<workflow_overview>

## Workflow at a glance

Two gate workflows bound each sprint. Between them the agent runs the execution loop inline: one
function at a time, no per-function stop, a single serial session (no MCP lock). `SPRINT.md` is the
resume surface when the middle spans context windows.

- **Effort.** Run the gates as cheap triage; run classical decomp and the permuter at higher effort,
  where the extra reasoning pays off.
- **Parallelism.** Read-only context gathering may run in parallel (read `BACKLOG.md`, run
  `pick_target.py`, and read the flagged hazard sections at once). Keep MCP, build, and edit steps
  serial, since the session holds no MCP lock and the build is one shared tree.
  - **Subagent decomp fan-out (a large classical pack's hard tail, S184).** `make nonmatching-func
    FUNC=<f>` compiles a per-function `nonmatchings/<f>/base.c` to a per-function `nonmatchings/<f>/current.o`
    and reads the shared `build/asm/<seg>.o` reference read-only (built once at bootstrap), so
    `tools/decomp_loop.py` on DISTINCT functions is parallel-safe. For a big pack, dispatch one subagent
    per hard standalone (or per nested pair) to iterate to score-0 in isolation and RETURN the matching C
    + struct/extern additions; the orchestrator then integrates each and runs the full-make + ROM-SHA-1
    gate SERIALLY (never full-`make` in the main thread while subagents run — that races the shared
    `build/`). S184 fanned 7 subagents over the golf-pack tail; both isolated matches held in-tree, the
    rest surfaced as well-characterized regalloc/rodata carries. Seed each subagent with the canonical
    struct/extern preamble + the levers so its base.c stays reconcilable at integration. S206 extended
    the recipe to an all-FP one-tu pack: 3 subagents over the FP-math tail returned 2 fully-RE'd S158
    regalloc carries + 1 unexpected byte-match (`func_800779A8`, cracked via a precise local-alloc
    coloring lever the orchestrator would not have found inline), so the fan-out earns its keep as
    FP-regalloc CHARACTERIZATION (each carry gets a `docs/wip/<fn>.near-match.md`), not just as a
    match-finder. Give each FP subagent the m2c-seed step + the FP levers (`f32 v[N]` stack-array for a
    dead-frame store block, float-diff computation ORDER, `while` vs `if{do-while}` loop form, split a
    temp to pin a value in `$f12`, non-negated `bc1fl` polarity).
  - **Fan out compiler-source subagents UP FRONT for a suspected-walled c-stub tail (S218).** When
    a c-stub `remaining:N` continuation's next smallest-first vein is suspected regalloc/ABI-walled
    (e.g. a getter/setter family sharing a div-index-into-table + struct-cell-write idiom, kin to the
    S158/S208 walls), do NOT iterate-then-permuter sequentially. Dispatch one subagent per target with
    the gcc-2.7.2 (`~/development/repos/mips-gcc-2.7.2`) + binutils-2.6 (`~/development/repos/mips-binutils-2.6`)
    pins in the prompt and have each seed asm-first, iterate isolated (`make nonmatching-func FUNC=<f>
    MAIN=1`), then root-cause any residual to a diverging pass with a `file:line` citation. One parallel
    pass then returns a definitive verdict per fn (byte-match OR a `docs/wip/<fn>.near-match.md`-grade
    wall with the pass named), instead of N sequential permuter setups. S218 fanned 4 over the
    `get_tile_attribute.c` non-FP getter/setter tail: 1 byte-match (`func_80041B98`) + 3 root-caused
    walls (2 `#local-alloc-qty-permutation`, 1 `$v0`-arg ABI on `func_80041E8C`) in one pass. The
    permuter stays post-root-cause; for a `#local-alloc-qty-permutation` verdict it is skipped outright
    (project history = 0 cracks), and corpus-mining is the escalation.
    - **Subagent hand-off contract (S235): end every crack-subagent prompt with an explicit "send your
      FINAL verdict to `main` via SendMessage BEFORE going idle — an `idle_notification` is NOT a
      deliverable."** In S235, 3 of 4 fan-out subagents went idle without sending their result, costing
      ~3 orchestrator re-pings to extract each verdict. Also tell each subagent that a permuter it
      launches in the background must have its result reported when it exits (the subagent may go idle
      while the permuter runs, then must wake and SendMessage the final byte-match/exhausted outcome, not
      just idle). The orchestrator holds ALL integration (src edit + full-make ROM-SHA-1 gate) until
      every subagent AND its permuter has reported, so a full-make never races the shared `build/` while
      an isolated subagent compile or permuter build is in flight. **S250 re-validated the contract: all
      3 fan-out subagents sent FINAL verdicts via SendMessage with ZERO orchestrator re-pings (vs S235's
      3-of-4 silent-idle) — the explicit "an `idle_notification` is NOT a deliverable" clause is
      load-bearing; keep it verbatim in every fan-out prompt.**
  - **A mined fresh-pack's mid-logic tail is a wall-class cluster, not a smallest-first vein (S224).**
    Once a fresh `none` pack's cheap leaves (getters/setters/predicates/dispatch) are banked, the
    residual mid-logic fns (the loops+struct-base+`bnel` tells) concentrate on a small set of documented
    no-source-lever classes — `#base-register-vs-displacement`, `#indexed-vs-pointer-loop-strength-reduction`,
    `#value-select-if-else-vs-branch-likely`/address-fold — that iterate to a fully-RE'd near-match but
    do NOT bank and are not permuter-reachable (extra-instruction or addressing-mode divergence). S224
    banked 1 clean dispatch fn then hit 3 such walls (`func_8006DF84`/`func_8006DEB4`/`func_8006D058`)
    back-to-back in `func_8006A2C0.c`; the next tier (`func_8006D38C`/`D214`/`CE88`) is the same combo.
    So at the plan gate, when a pack's banked-count has plateaued and the smallest remaining fns carry
    these tells, prefer a FRESH pack or an escalation slice (compiler-source fan-out / corpus-mining)
    over another smallest-first continuation of the mined pack — the continuation yields
    wall-characterizations (RETRO value), not banks. `pick_target.py` pricing this "plateaued-pack
    mid-logic-tail" tell is a tracked follow-up (see `BACKLOG.md` ranker follow-ups).
    - **But the compiler-source fan-out on such a tail is often a BANK slice, not just characterization
      (S232).** When the plateaued tail's smallest fns are already fully-RE'd DOCUMENTED near-match walls
      (in-file near-match comments, even ones carrying a prior `file:line` compiler-source verdict),
      dispatch one gcc-2.7.2 + binutils-2.6 subagent per wall to reproduce-and-crack, not to
      re-characterize. S232 fanned 3 over the `func_80052FE0.c` tail and cracked ALL THREE — a documented
      `#local-alloc-qty-permutation` (`||` single-store steers `global.c` allocno priority), a
      `#delay-slot-fill-across-call` (`void`→`s32` return-reg liveness, reorg.c:3374-3376), and a
      `#cross-jump-tail-merge` (`goto`-split the return tails) — each refuting its prior
      "not source-leverable" verdict. So a prior wall verdict (even one with a pass citation) is a
      HYPOTHESIS, not a terminal state: the citation may name the right pass yet miss the source lever
      that steers it. Elevate the crack-attempt fan-out ABOVE "prefer a fresh pack" when the plateaued
      tail carries fully-RE'd near-match comments; fall back to a fresh pack only if the fan-out returns
      genuine walls. See `docs/hazards.md` (the three sections above) and the memory
      `revalidate-old-carries-stale-wall`.
      - **S260 sharpened the trigger: a `file:line`-cited "PROVEN WALL" is the HIGHEST-priority
        re-open target when a new structural lever lands, not a reason to skip it.** S260 banked
        `find_keyframe_offset_by_tag`, whose S213 doc was the strongest terminal verdict in the tree
        (a compiler-source dive citing `loop.c:505-545`'s first-iteration peel + the CSE cascade it
        drives, "no source form yields {no-peel + re-derive + reload + result-in-s1}"). The citation
        was correct and the "no source form exists" conclusion drawn from it was not: the goto loop
        removes loop.c from the picture entirely, peel included. So when a sprint gains a new
        structural lever (a goto loop, a struct-view for a neighbour read, an out-of-line handler),
        the DoR sort should put the carries whose cited pass that lever DISABLES at the FRONT, however
        strongly worded their verdict — the more source-dive backing a verdict has, the more precisely
        it names the pass a structural lever can now sidestep.
      - **But not every wall is steerable — the fan-out ALSO earns its keep by returning terminal
        no-lever VERDICTS (S233).** S233 fanned 3 gcc-2.7.2 + binutils-2.6 subagents over a fresh
        `raycast_terrain.c` FP/collision pack's near-match tail: 1 genuine CRACK+bank
        (`clamp_min_distance_from_target` 505→0, a `sched.c:834 true_dependence` fixed-vs-varying
        address lever — read an adjacent global via a varying pointer to pin a floated struct-field
        store) plus 2 definitively root-caused TERMINAL no-lever verdicts (`get_surface_type` /
        `func_8003DE80`: a shared block-LOCAL constant materialized to hide the R4000 load-latency,
        `mips.md:153-155` — my build is 1 instr SHORTER, the ROM lost a scheduler coin) plus the
        assembler ruled out (a 3rd binutils subagent confirmed all divergences are pure gcc codegen).
        KEY ASYMMETRY vs S232: S232's walls were `global.c` ref-count-steerable (crackable); S233's
        constant walls were block-LOCAL `local-alloc.c` scheduler coins (NOT steerable — the S232
        `||`-ref-count lever does not reach `local-alloc`). So the fan-out's deliverable is BOTH
        cracks AND pass-cited terminal verdicts: a no-lever verdict with a `file:line` citation is a
        REAL result — it retires the wall so no future sprint re-grinds it (write it into the
        `docs/wip/<fn>.near-match.md` + the hazard entry). Do NOT expect a fresh-pack tail to crack
        at the S232 3/3 rate: S232's tail was `global.c` walls; a fan-out's job is to SORT the tail
        into {crack, terminal-verdict}, not to crack everything.

</workflow_overview>

<slash_commands>

## Slash commands

- **Codex:** use `$mg64-sprint-plan [scope]`, `$mg64-decomp-loop`, and `$mg64-sprint-review`.
- **Claude Code:** use `/sprint-plan [scope]` and `/sprint-review`.
- **`sprint-plan [scope]`**: Plan gate. Ranks candidates with `tools/pick_target.py`
  (smallest-first), proposes one small increment plus its enablers, gets PO approval, then performs
  and validates the flip (subseg yaml edit + `symbol_addrs.txt`, optional `make sync-names`, then
  `make extract && make` for the green ROM), and writes `SPRINT.md`. Then runs the execution loop over
  the committed backlog.
- **`sprint-review`**: Review gate. Verifies the ROM-SHA-1 DoD on the increment, takes PO scope
  sign-off, and is the single apply-point for the sprint's buffered workflow-improvement edits. It
  appends the `RETRO.md` digest and `BACKLOG.md` carry-overs.

</slash_commands>

<execution_loop>

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
     stock OS structs), so the RE'd struct may live in a sibling decomp, not the Ghidra DB — check
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
       classical main logic pack (`get_table_entry.c`: context = common.h + the Ghidra-DB
       `TerrainAttrEntry` + a synthesized 0xB8 `ShotInitRecord` + game externs; all 3 standalone fns
       banked, and both levers above surfaced there).
     - **asm-first seed fast-path (MCP-independent, for small fns ~<40 instrs).** The splat
       `.s` under `asm/nonmatchings/<seg>/<func>/<func>.s` is the same ground truth as
       `disassemble_function`, so a small classical fn does not need MCP: hand-translate straight from
       the `.s` (resolve callee/global names + types from the name files / call-site arg setup),
       write the body directly into `src/<seg>.c` (declare each `extern`; auto `func_`/`D_` symbols
       resolve from their home subseg), and gate on the full-`make` ROM SHA-1. Skip
       seed_c.py/base.c/decomp_loop entirely unless the first build misses, then drop into the
       isolated Iterate loop below. Use this when Ghidra MCP is unavailable (`list_instances` empty)
       or the fn is small enough that the decompile adds no shape/type value. The fast-path banked a
       2-fn 176B pack this way, first build, MCP down (S148).
       - **Per-fn oracle for the fast-path = `objdump -d` of the FRESH object, NOT `diff.py` (S244).**
         With no base.c/decomp_loop, iterate on register/scheduling diffs by rebuilding just the one
         object and disassembling it: `find build -name '<obj>.o' -delete && make build/<path>/<obj>.o
         && mips-linux-gnu-objdump -d build/<path>/<obj>.o | awk '/<fn>:/,/<next>:/'`. The object's
         `%hi/%lo` show as `0x0` (unresolved) but the register allocation, instruction order, and
         immediates are the ground truth. `diff.py` reads `build/*.map` which an incremental per-object
         build does NOT refresh, so it lies (S244 `func_80079940`: diff.py showed byte-clean while the
         fresh object was dual-base-split). Use `diff.py` only right after a full `make` (map + objects
         consistent) for the reloc-resolved view; gate every bank on `tools/verify-rom.sh`. See the
         memory [[subagent-diff-crack-not-a-bank]].
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
     `.bss`, so fix the short fn, not the sibling). And rule out a **missing callee prototype**: a
     callee with no `extern` prototype in scope makes gcc assume implicit-int, which flips
     regalloc/scheduling (a base-materialize hoist across a `jal`) and reads as a scheduling wall —
     declare every callee with its real signature before reaching for the permuter
     (`docs/hazards.md#callee-prototype-is-load-bearing-missing-prototype--implicit-int`, S225).
     And rule out a **GCC nested function** BEFORE calling a leaf a `$v0`-arg wall or a dead-frame
     coin: a leaf that spills the INCOMING `$v0` with no reload (`addiu sp,-8; sw $v0,0(sp)` never
     reloaded) is the nested-function prologue homing the static chain (`STATIC_CHAIN_REGNUM =
     GP_REG_FIRST+2 = $2 = $v0`, mips.h:1310) — the chain is homed even when the child reads no
     parent variable (unused -> dead store; a chain-USING child instead RELOADS `$v0` and reads its
     arg/data through it). The tell on the CALLER side is `addiu $v0,$sp,K` (address of a parent
     local = the chain) persisting to the `jal`. A clean nested child reproduces the exact bytes with
     no `volatile`/uninitialized contrivance; bank it inside its parent's TU as a nested function, or
     (if the parent inlined the call and left an orphaned out-of-line body with zero ROM `jal`/fn-ptr
     xrefs) carry the orphan with a standalone `volatile s32 x = <uninit>;` byte-repro stand-in. S248
     proved `func_8008E164`/`lerp_s32` is an orphaned nested child and re-priced the `func_80092E10`
     "$v0-arg wall" carry to a chain-USED nested child (crackable once its parent is decompiled);
     see `docs/hazards.md#nested-function-static-chain-spill` and the memory
     `dead-frame-dead-v0-store-crack`.
   - **Spot-check** (only at score 0): byte-level `cmp` of the in-tree compiled `.text` against the
     isolated one. The `cmp` is the truth, not the mnemonic diff (see
     `docs/hazards.md#assembler-differences--byte-cmp-spot-check`). A non-zero score with empty
     `top_mismatches` and `match_count == total_rows` is an isolation artifact (struct-field reloc
     addend, or extern HI/LO16), not a near-miss: go straight to the in-tree spot-check and full-make
     SHA-1; do not iterate C or reach for the permuter (see `docs/hazards.md#isolated-compile-caveat`).
     **But this signal is only an isolation artifact at HIGH percent** (a few reloc/HI-LO rows against
     an otherwise-matching body); at LOW percent (S205 `func_8005E380`: 0.48 with 535/535 rows +
     empty `top_mismatches`) it is a REAL pervasive near-miss whose per-row diffs asm-differ groups
     out of `top_mismatches` (e.g. a `#cse-derived-pointer-base-canonicalization` base-reg fold hitting
     every access). **Disambiguate with the in-tree `tools/asm-differ/diff.py <func>`** (diffs the real
     build against the ROM via the mapfile): if it too diverges, it is a genuine near-miss/wall — do
     NOT apply the isolation-caveat shortcut, root-cause the pervasive diff instead.
     - **The incremental `diff.py` verdict is STALE-prone; gate crack/bank decisions on the full-make
       ROM-SHA-1, not on `diff.py` (S242, recurred ~6x).** After `make build/<obj>.o` + `diff.py <fn>`,
       diff.py reads the object + `build/*.map` which an incremental per-object build does NOT fully
       refresh vs the linked ROM, so it lies in BOTH directions: S242 `func_80075010` showed 5 diff rows
       yet was byte-exact, and `func_80076138` showed 0 rows yet the ROM mismatched. Bare `diff.py <fn>`
       with no fresh build also spills the whole segment (~1024 "rows"). Treat `diff.py` as an iteration
       hint only; confirm every score-0/bank with `tools/verify-rom.sh` (full-make ROM-SHA-1), and
       `find build -name '<obj>.o*' -delete` before a single-fn diff you actually trust. See the memory
       `subagent-diff-crack-not-a-bank` (now covers the orchestrator's own diff.py, not just subagents').
       - **Stale-detector (S257): the SAME `diff.py` score twice in a row after a real source edit means
         STALE, not "the edit had no effect."** S257 got an identical score across FOUR materially
         different sources (a ternary vs an if/else vs two different clamp idioms); `find build -name
         '<obj>.o' -delete` did NOT clear it — only a full relink (`tools/verify-rom.sh` / `make` to the
         ELF) refreshed the mapfile diff.py reads. The escape that always works mid-iteration is
         `mips-linux-gnu-objdump -d build/src/<tree>/<obj>.o` plus an instruction-count check against the
         `.s` header (`head -1 asm/nonmatchings/<seg>/<f>/<f>.s` gives `0x<size>`; instrs = size/4). An
         instruction count that already matches the ROM turns a "which register" question into a pure
         permutation question, and one that does not tells you the length gap directly — both are more
         actionable than a score. Do NOT spend more than one iteration on an unchanged score.
   - **Finalize** (only if the spot-check passes): inline the body into `src/<seg>.c`, drop the
     `INCLUDE_ASM` line, `clang-format-22 -i` (now applies to every tree, including `src/libultra/`,
     `src/libkmc/`, `src/libnusys/`, and `src/mgu/`), then `make` until `build/mariogolf64.z64: OK`
     and SHA-1 == baserom.

4. **Bank** the function (only at score 0 plus spot-check plus full-`make` SHA match):
   a. Give it its curated Ghidra name: add to `symbol_addrs.txt`, rename in the body, re-`make`
      (`make extract && make` when the rename must reach still-asm callers). **Before renaming, grep
      already-committed C callers: `grep -rn '\bfunc_<addr>\b' src` (S247).** The `make extract` regen
      only rewrites STILL-ASM refs via `undefined_syms_auto`; a fn called from a committed C TU by its
      auto `func_<addr>` name will `undefined reference`-fail the link after the rename. If any C
      caller exists, either sed those files to the new name in the same commit or KEEP the auto name
      (a getter/predicate over a generic global read across unrelated subsystems is usually best left
      auto-named — a domain-guess is likely wrong; S247 kept `func_800959F8`).
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
f(void){}` DIRECTLY (not an `INCLUDE_ASM` stub) for a 2-instr `jr ra; nop` leaf, so a freshly-opened
pack's `INCLUDE_ASM` count is BELOW its function count by the number of such empties (S228
`func_800453E0.c`: 37 stubs for 40 fns — 3 empty leaves pre-C at scaffold time, free byte-matches). So
the matched-fn count is `fn-count − remaining-INCLUDE_ASM`, NOT the stub-count delta, and the plan
gate's "N stubs" figure is not the total-fn count. Grep `INCLUDE_ASM` for the true remaining work; count
the empty auto-C leaves toward banked.

The context window auto-compacts as it fills, so do not stop early on budget concerns; save progress
to `SPRINT.md` (standup line + suggestion buffer) as each function banks, so a fresh window resumes
from it.

</execution_loop>

<scrum_model>

## Scrum operating model

The tactical match-loop runs inside a thin Scrum cadence (McConnell, *More Effective Agile*).
Artifacts: `BACKLOG.md` (PO-owned ordering rationale, enablers, carry-overs), `SPRINT.md` (gitignored
ephemeral board and resume surface), `RETRO.md` (consolidated digest), `VELOCITY.md` (story-point
dashboard). Target selection is `tools/pick_target.py`, not a stored roadmap.

- **Roles.** Product Owner is the user (backlog priority, goal approval, scope sign-off, retro
  selection). Development Team is the agent (executes the gate enablers after PO approval) plus
  `tools/` plus the KMC-GCC / asm-differ / ROM-SHA-1 oracle.
- **Cycle.** `sprint-plan` (PO approves a goal and small backlog; agent performs and validates the
  flip enablers), then autonomous execution, then `sprint-review` (DoD verify, scope sign-off,
  retro), then repeat.
- **Sprint = one `src/<seg>.c` file to md5-candidate (0 `INCLUDE_ASM` stubs), or one cohesive subseg
  cluster.** The function is the task; the file or cluster is the increment. Cap small early: 1
  upstream file, or about 3 to 4 functions, so the middle fits a context window. For a homogeneous
  sibling set (same upstream pattern, same uniform enabler), fill the 3-to-4 cap, since per-file
  all-or-nothing banking makes the marginal sibling near-free. A heterogeneous batch (mixed hazards)
  trims to the cleanest 2. Review fires when every committed item is banked or spiked/carried.
- **Definition of Ready.**
  - Subseg flippable (not `hasm`); coarse size known; upstream-mirror availability noted; hazards
    flagged.
  - **Size FRESH-pack leaves from the extracted `.s` headers, NOT vram gaps (S247).** In a
    multi-fn asm-flip pack, curated-named fns interleave the `func_<vram>` ones, so a leaf's size
    computed from adjacent-vram deltas is wrong (S247 mis-sized `func_8009232C`/`per_hole_...`/
    `func_8009351C` as tiny when they were 0x4CC-0xAE4, and missed the real tiny leaves
    `func_8008FF14`/`func_800959F8`/`func_800934CC`). Either (a) commit the backlog as "N smallest-first
    leaves TBD at extract" and re-sort by `head -1 asm/nonmatchings/<seg>/<f>/<f>.s` (`nonmatching <f>,
    0x<size>`) once the flip is done, or (b) if you must name leaves at the gate, flip+extract FIRST
    then size from the `.s`. (Fold into the `carried-wall`/sizing ranker follow-ups in `BACKLOG.md`.)
  - **For a c-stub CONTINUATION (an already-`c` file), grep the target `src/<file>.c` for pre-existing
    near-match / carry comments on each candidate leaf BEFORE committing it (S232).** `pick_target.py`'s
    smallest-first sort and any FP/jal tell-filter do NOT see the in-file wall comments a prior sprint
    wrote above a carried `INCLUDE_ASM` stub, so the smallest remaining leaves are often exactly the
    documented walls. `grep -nE 'INCLUDE_ASM|/\*' src/<file>.c` (or read the stub's preceding comment):
    a leaf with a fully-RE'd near-match comment is a carried wall, not a fresh tractable leaf. Committing
    it is fine IF the goal is a crack-attempt slice (compiler-source fan-out, S232 cracked 3/3) — but
    label it as such, do not price it as a clean leaf. (A `pick_target.py` `carried-wall:<fn>` detector
    that reads the in-file comment is a tracked ranker follow-up; see `BACKLOG.md`.)
    - **ALSO grep the `BACKLOG.md` carry list by fn name (S239+S240 DoR miss, RECURRED).** A wall
      characterized in a PRIOR sprint often lives ONLY in the `BACKLOG.md ## Carry-overs` entry (or a
      `docs/wip/<fn>.near-match.md`), NOT as an in-file comment above the stub — so the in-file grep above
      misses it and the leaf re-surfaces as "fresh." S239 (`func_8006C8CC`) and S240
      (`func_8006D164`/`func_8006DF84`) both re-committed already-characterized BACKLOG carries this way.
      At the plan gate, `grep -n '<candidate_fn>' BACKLOG.md` for EACH committed leaf; if it is a listed
      carry, either skip it or label the commit a crack-attempt/deepen slice (not a fresh leaf). Write new
      wall characterizations to `docs/wip/<fn>.near-match.md` AT DISCOVERY (not only in the retro digest),
      so the next sprint's DoR finds them. Fold into the `carried-wall:<fn>` ranker follow-up.
  - **A carried wall's near-match doc can be wrong about the function's SEMANTICS, not just its
    verdict — re-derive behaviour from the `.s` before accepting a stated residual (S258).** S252
    recorded `func_800824E4` as a THREE-argument packer with `b = arg2` on the negative path and
    built a terminal delay-slot-coin verdict on top of that reading; `$a2` is written in the entry
    branch's DELAY SLOT before any read, so there is no third argument and `b = 0xFF` is an ordinary
    shared statement before the if-chain (which is precisely why it fills the delay slot). Corrected,
    the function was 20/20 instructions with one differing operand and banked the same day. S254's
    `func_80088A90` verdict likewise named two "unreachable from faithful C" features that both fell
    out of source once the emit block used the SDK macro. So at the start of a crack-attempt slice,
    read the target's `.s` end to end and re-derive the signature and dataflow FIRST, then read the
    doc's residual. A register written in a branch delay slot before its first read is a shared
    pre-branch statement, not an argument. Extends the memory `revalidate-old-carries-stale-wall`
    from stale builds to stale RE.
  - Enablers (subseg flip plus `make extract`, multi-file split, `symbol_addrs.txt` additions) are
    performed by the agent at the plan gate after the PO approves the goal/scope, and validated
    there: `make extract && make` must still produce the green baserom ROM with the new stubs. This
    gate build-check is load-bearing: it once surfaced a missing-`cpp` toolchain regression that was
    silently emptying every asm object.
  - Run `make sync-names` only at the gate; a mid-sprint rename breaks in-flight links until the next
    `make extract && make`.
- **Definition of Done (binary; the oracle already exists).**
  - Per-function: score 0, byte-`cmp` spot-check, inlined, clang-format, full `make` to
    `build/mariogolf64.z64: OK`, SHA-1 == baserom, and committed.
  - Per-sprint: every fn in the file inlined (0 stubs), ROM SHA-1 matches, committed.
  - The ROM SHA-1 is green at every commit (un-decompiled parts fill from extracted asm), so at
    review the `make` + SHA-1 paste is a regression guard; the value delta is the matched-count /
    md5-candidate-files number. **Never commit a non-matching fn.**
- **Spike + carry-over.** A function that blocks its file's DoD (stuck-far below 0.97, needs permuter,
  BSS-layout-conflict, subseg-alignment) is a spike: note it, carry its file or cluster to the next
  sprint, and count credit at the function level. Hold the DoD firm; a spike is carried, not banked.
- **Quality counter-metric.** Track stuck-far + permuter-escalated + carried + re-opened per sprint,
  reported next to the match count so the count cannot be gamed by premature spiking.
- **Process changes are retro-gated.** The "Suggested workflow improvements" the execution loop emits
  are recorded into `SPRINT.md` during the sprint and applied at `sprint-review` only, not
  mid-sprint, so the sprint's matching behavior stays fixed and its measurement stays clean. The
  retro is the single apply-point for edits to `AGENTS.md`, `CLAUDE.md`, `docs/*`, `tools/*.py`,
  `.agents/skills/*`, and the Claude command files. A PO-directed out-of-band rework is the exception, by direct
  request.
- **Two-gate close.** Review accepts the product (DoD plus scope); retro improves the process (the
  single apply-point for tooling edits).
- **Resume protocol.** On a fresh mid-sprint session: read `SPRINT.md`, reconcile banked work via
  `git log` since the snapshot, and verify Ghidra MCP connectivity (`list_instances`, port 8089)
  before resuming. The context window auto-compacts, so continue from `SPRINT.md` rather than wrapping
  up early.

</scrum_model>

<story_points>

## Story points

Lightweight estimation over the Scrum cadence. Scale: Fibonacci 1, 2, 3, 5, 8, 13. Current phase
`regime: mirror`. `VELOCITY.md` is the committed dashboard with the full rules and anchors; this is
the summary.

- **Deterministic seed (v1).** `pick_target.py`'s `pts` column is the a-priori seed (a pure function
  of `size`, `upstream`, `band`, `nfns`, `hazards`). A cluster seed is the sum of its files' seeds.
  `pts` is display-only: it does not change the smallest-first sort. A `blk` seed is an un-pickable
  needs-header (a DoR reject); swap the increment rather than commit it.
- **The 8-point decompose gate (v1).** Route a seed of 8 or 13 through the decompose gate before it
  runs as a 1-increment sprint: decompose it (split the subseg at the upstream-file or function
  boundary) or pull a scaffolding enabler as the goal instead. Applied at the `sprint-plan` gate. This prevents an all-or-nothing
  bank stall. Expect it to fire once the mirror band is mined out and classical units dominate.
  - **Small classical pack exemption (now folded into the ranker).** A `one-tu`
    classical pack is mechanically non-decomposable (non-16-aligned inner boundaries, or the fns share
    a TU's rodata/data, so you cannot independently compile half a one-tu), so the 8-gate's "must
    decompose" verdict is a false fire: it banks atomically as one vertical slice. **The ranker folds
    this into the `pts` column** (`tools/pick_target.py seed_points`): a one-tu classical pack of
    `nfns<4` now seeds 3/5/8 by size (tiny <256 B → 3, mid → 5, big >=768 B → 8; deweight to 3 if
    `jal-free`; +1 if `rodata-straddle`), not the old flat 8/13 (S155). So the gate no longer fires
    on these, and this bullet is now the banking-behavior note (bank atomically, or a quick spike;
    no permuter-class control flow) rather than a pts workaround. The residual manual override the
    ranker still leaves: a big/huge or 4+fn one-tu pack the ranker prices 8/13 that you nonetheless
    know banks atomically (it can't decompose) — run it seed-only and record it in
    `SPRINT.md ## Estimate`. The historical a1 (`<256B AND <=2fn`) / a2 (`0-jal size-agnostic`) branches
    are the empirical anchors the ranker was calibrated to (see VELOCITY.md ## Seed rubric, S155
    re-anchor). S148 `overlay_10/func_ovl10_801F4A40` (2fn, 176B, one-tu) and S150 `main/func_80029250`
    (`cfb_setup`+`cfb_set_num`, 2fn, 496B, one-tu, 0-jal) both banked seed-only; both now seed 3/5 (not
    13) automatically.
  - **Verbatim-mirror exemption.** A seed-8/13 increment may run as a normal
    1-increment sprint when all of these hold:
    - (a) `regime: mirror` plus a verbatim copy of a single upstream file. A drop-def mirror
      qualifies: the function bodies are byte-verbatim, and only the file-scope data defs become
      `extern` decls, which emit nothing, so it banks atomically exactly like a pure `cp` (S86
      `os/timerintr.c`, pts-8 single-file-pack, banked first-try seed-only). A `.data`-carve mirror
      qualifies even more directly: the data defs stay defined and the `.data` is carved to its placed
      vram, so it is a pure verbatim `cp` of the whole file (S116 `nucontgbpakmgr.c`, pts-8
      single-file-pack, banked first-try seed-only, the first libnusys `.data` carve).
    - (b) The decompose path is mechanically blocked: the increment is a `single-file-pack` (every
      member fn comes from one upstream `.c`), so there is no inter-file boundary to split at and an
      intra-file split cannot be independently mirrored. This holds regardless of inner-boundary
      16-alignment: S64 `lookathil`'s inner boundary was `non16align`, S69 `lookat`'s was 16-aligned,
      and both were decompose-blocked (you cannot mirror half a source file). `pick_target.py`'s
      `single-file-pack:<n>fn[…]` tag is the signal (S67); the old `non16align`-on-the-inner-boundary
      test was one mechanical case of it, not the rule.
    - (c) Every callee is placed and all names are curated (the "no residual variance" condition).

    The gate's all-or-nothing concern guards classical iteration stalls; a verbatim single-file mirror
    banks atomically (or is a quick spike), so a size-only 8/13 is a false fire, the same false-flag
    class the hazard detectors keep retiring. Document the exemption in `SPRINT.md ## Estimate`; the
    increment stays seed-only (S64 `gu/lookathil.c` + S69 `gu/lookat.c`, both pts-13, banked
    first-try). The exemption never covers classical or multi-file packs (a `pack` or `c-combined` of
    2 or more distinct upstream files decomposes at the file boundary as usual).
    - **Sub-100 coddog hedge.** A `single-file-pack` with a sub-100 coddog score
      (e.g. `@99.99`, the same near-verbatim tell that flags block-reorders) still qualifies for the
      exemption, but budget a **body-divergence diagnosis pass**: the "banks atomically or is a quick
      spike" assumption can be violated by a per-fn divergence (block-reorder, or a **game-modified
      body** — an extra branch/store the literal upstream lacks) that turns the "quick spike" into a
      partial bank (S121 `nucontrmbmgr.c`: 8/9 banked C, 1 carried as `INCLUDE_ASM`). Hedge the estimate
      "<=1 re-attempt" and expect a per-file score of 0 pt if the file ends partial (the matched-fn
      count is the value signal, not the file point). **Prove the body emits the target's exact
      stores + values before concluding "compiler wall":** the S121 carry was misframed as an
      unbankable `#cross-jump-tail-merge` for 5 sprints, then banked S127 with a one-branch force-stop
      fix (`state = STOPPED` on `osMotorInit` error). See `#cross-jump-tail-merge`.
    - **coddog 99.99 == structure, not bytes; the exemption-guard.** A `coddog-mirror:<f>@99.99`
      can mask a heavily game-customized file where most bodies diverge, not just a block-reorder
      (S123 `nusched.c`: a game scheduler that shares only the nusys skeleton). The verbatim-mirror
      exemption does not apply when the pack carries a **customization tell**: a `jal` to a non-lib
      `func_<vram>` game callee (a callee that is not `os*`/`nuSc*`/`al*`/lib), or a large
      `jal-count-mismatch` not explained by a known macro/version artifact. Those signal pervasive
      per-fn divergence -> route to the classical track with the **mixed bank-stock-carry-custom**
      plan (next bullet), not a seed-only atomic mirror. Verify bodies before trusting a 99.99 row:
      diff the asm against the upstream for the heavy functions, and run the nusys/libultra version
      triage (`docs/hazards.md#upstream-mirror-pattern`) before concluding the divergence is custom.
      (pick_target.py automation to price the non-lib-`func_`-callee tell is a tracked follow-up;
      until then the gate applies this guard by reading the pack's asm callees.)
    - **New-audio-sub-lib band-open is a small incremental enabler when the n_audio_sc header DAG is
      shared.** An earlier hedge said "libmus needs a full header-vendoring sprint first + reset
      body-divergence to full"; in practice libnaudio had pre-paid the shared audio header base
      (libmus.h, n_libaudio_sc.h, libaudio.h, the SC internal hdrs + the base `-I include/libmus
      include/libnaudio`), so opening libmus was ~3 small headers + a 2-line `mk/libmus.mk` profile +
      a mechanical `pick_target.py` add, and the `@100.00` leaf `lib_memory.c` banked first-build
      seed-only (S140 hedge, S141 reality). Reframe a sibling audio lib that shares the n_audio_sc
      header DAG as a cheap incremental enabler (each `.c` vendors only its own private `aud_*.h` at
      bank time), not a full header sprint. Keep the body-divergence hedge per-coddog-score — `@100.00`
      = byte-identical, trust the verbatim mirror; `@99.99` = the diagnosis-pass / exemption-guard
      above — not a blanket-full-by-lib reset.
    - **Mixed mirror+INCLUDE_ASM partial bank is first-class** (S121 generalized to S123). A
      `coddog-mirror` file can be partially stock: some fns byte-match the upstream, others are
      game-customized. The right play is **bank-stock-carry-custom** — write the stock fns as C and
      keep the customized fns as `INCLUDE_ASM` in the same `src/<seg>.c` (the ROM stays green; the
      file is partial / not md5-candidate until the customized fns are classically decompiled). Plan
      such a file as a `regime: mixed` increment, not a seed-only mirror: per-file all-or-nothing
      means the partial file banks 0 pt, and the matched-fn count is the value signal (S123 nusched.c:
      10/14 banked C, 4 carried, 0 pt, +10 matched).
      - **Extends to a CLASSICAL one-tu with SHARED rodata (S169).** A one-tu classical pack can
        partial-bank too: write the matched fns as C and keep the unmatched fns as `INCLUDE_ASM` in the
        same `src/<seg>.c`, **as long as the TU's shared rodata (strings, FP-double pool constants)
        stays in the extracted blob and every fn references it `extern`** (no `.rodata` carve). The ROM
        stays green off the matched fns. So a hard FP fn does not block banking its tractable siblings,
        and the one-tu is NOT strictly atomic-or-nothing when the rodata is referenced extern rather
        than emitted as literals. S169 `func_80076640.c` banked `func_80076778` (+1) as C while
        `func_80076640` (score-25 reg-swap) and `func_8007680C` (S158-class FP) stayed `INCLUDE_ASM`,
        all referencing the shared `ACAD0` rodata blob. (Emitting the rodata as source LITERALS would
        force the carve and re-impose atomicity, so prefer extern refs for a partial one-tu; see
        `docs/hazards.md#rodata-sibling-yaml-pattern`.)
- **Per-file all-or-nothing banking.** Points bank per file: a spiked/carried file scores 0 pt, a
  banked sibling still counts. This is a separate ledger from the function-level quality
  counter-metric.
- **Realized tier + residual (v2, active since Sprint 11).** Scored at review on the classical track
  only: start at the seed band, +1 per {stuck-far / permuter / re-attempt / novel bank-gotcha /
  mid-sprint split / carry-or-reopen}, and -1 for a verbatim first-try with at most 1 fix-iteration;
  per-file, then summed. Residual = realized minus seed. A classical/mixed increment uses the
  two-pass freeze (plan-time seed committed before `src/`, realized in a second commit). The mirror
  track stays seed-only (a point mass with no residual variance), so a `regime: mirror` sprint logs
  banked = seed. The realized tier is agent-scored and subjective, so the real anti-gaming guards stay
  the per-file all-or-nothing bank plus the quality counter-metric, not the freeze. Full rules,
  rolling-5, and re-anchor in `VELOCITY.md`.

Velocity is a planning indicator, not a performance target (McConnell Ch. 19), reported next to the
quality counter-metric, which (with per-file all-or-nothing banking) is the real anti-gaming guard.

</story_points>

<conventions>

## Conventions (every-sprint)

These apply regardless of hazard. Hazard-specific procedures are in `docs/hazards.md` (see the index
below).

- **One function at a time.** `pick_target.py` ranks (smallest-first); you pick the target.
- **Never rewrite a partial-bank `src/<seg>.c` with a scripted whole-region splice (S257; RECURRED
  S259).** The rule below was already written and was still violated — reverting ONE function to
  `INCLUDE_ASM` with an `s[:i] + new + s[j:]` splice deleted two banked one-line siblings
  (`func_8006D1FC`, `func_8006D208`) that happened to sit between the anchors, surfacing only as an
  `undefined reference` at link. What was missing is a MECHANICAL guard, so: **use `Edit` with an
  exact `old_string`. If a script is genuinely needed, assert BOTH counts across the rewrite** —
  `grep -c 'INCLUDE_ASM'` (must change by exactly the number of functions promoted, 0 for an in-place
  body edit) AND the file's function list, diffed before and after. The original S257 occurrence was
  the same mechanism in the other direction: a splice between two anchors silently deleted three
  `INCLUDE_ASM` stubs and their multi-line carry comments. Both times the anchors looked adjacent in
  the author's head and were not adjacent in the file.
- **Scratch dir** `nonmatchings/<func>/` (gitignored, shared with the permuter).
- **Python tools run via the venv:** `venv/bin/python3 tools/X.py` (system python lacks asm-differ
  deps and is PEP-668-locked).
- **Per-function iteration oracle: `tools/cmpfn.sh <func> [<object>]` (S258).** Diffs the extracted
  `asm/nonmatchings/**/<func>.s` against a freshly built object, normalising register prefixes,
  `%hi/%lo`, immediates, the splat `(0xX >> 16)` spellings, `move`/`li` aliases (including
  `beqz`/`bnez` and their branch-likely forms), the SDK FP register names (`fv0`/`fs1` vs `f0`/`f22`),
  and EXTERNAL branch/jal targets, so only real differences show. Its first line is the instruction
  COUNT of each side — the most actionable number when a body is structurally right but the wrong
  length. Unlike `diff.py` it reads the object directly, so it never goes stale after an incremental
  `make build/src/<tree>/<obj>.o`. It is an ITERATION oracle only: every bank still gates on
  `tools/verify-rom.sh`.
  - **INTERNAL branch targets are position-relative deltas, NOT a placeholder (S260 fix).** Before
    S260 the tool collapsed every branch target to `T`, so it could not see a redirected back edge and
    reported such a function byte-clean: S260 `collect_keyframe_events_at` was a `cmpfn`-clean 54/54
    whose loop back edge went one instruction too far and broke the full-make ROM. It now rewrites a
    `.L<vram>` (asm side) or `<fn+0xNN>` (object side) target to a signed `@Dp<n>`/`@Dm<n>` distance in
    instructions, so a redirected edge shows as a real diff (`@Dm14` vs `@Dm13`) while a correct
    internal branch cancels cleanly. Even so, `cmpfn` is an ITERATION oracle: a `cmpfn`-clean function
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
  any `src/<tree>/%.c` with the default **-O2 game profile** (`C_PROFILE_CFLAGS = $(CFLAGS)`; the
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
  - **But an EXACT instruction count plus a one-operand residual is a permuter target regardless of
    what `percent` reads (S258).** The 0.97 gate exists to keep the permuter off structurally-wrong
    bodies; once the instruction count matches the ROM and only a register or operand CHOICE
    differs, it is the right tool even at a lower percent. S258 ran four: `func_800824E4` (20/20
    instrs, ONE differing operand) hit **score 0 in 72 iterations** with a spelling no hand-iteration
    produces (`b = shade; b = r - b;`, breaking a cse equivalence class between two registers holding
    the same constant), while the three larger permutations all plateaued (`func_80087CB0` 480->265
    in 60k, `func_80088A90` 870->520 in 31k, `init_sky_pool_and_world_state` 615->545 in 91k). So the
    payoff shape is exact-count-plus-one-operand; a multi-register permutation is not.
  - **The permuter (asm-differ) is BLIND to internal branch TARGETS — a permuter score of 0 on a
    pure-branch-target residual is a FALSE POSITIVE (S261).** asm-differ normalises a branch to a
    local label and does not distinguish `bne …,<label@0x7c>` from `bne …,<label@0x80>`, the same
    blind spot S260 fixed in `tools/cmpfn.sh`. S261 imported the exact 54/54
    `collect_keyframe_events_at` body; the permuter reported `base score = 0` / "Found zero score!"
    while the real object was `fff2` and the ROM `fff1` (its own target.o was correctly `fff1`). So
    when the SOLE residual is an internal back-edge/branch TARGET (not an instruction or a register
    choice), the permuter cannot score it — do not trust a permuter 0/low there; gate on `objdump` or
    `tools/verify-rom.sh`. (This is a distinct failure mode from the stale-object one in
    [[subagent-diff-crack-not-a-bank]]: here the permuter's oracle is correct but its scorer is
    blind.) A back-edge-target residual is a gcc first-load-peel coin (see the same-field-peel entry in
    `docs/hazards.md`), not a permutation.
  - **A recorded "below the 0.97 gate, not permuter-eligible" verdict belongs to the BODY that was
    measured, not to the function (S259).** `func_8006CE88` carried "isolated score 5360 (pct 0.553)
    ... NOT permuter-eligible here"; that percent was measured on a body with a 2-instruction
    structural deficit. Fixing the deficit took it to an exact instruction count, after which the
    permuter's base score was **55** and it banked the same session. Re-measure after every structural
    fix — count reaching exact, a loop shape corrected, an addressing form matched — before quoting
    an old percent to rule the permuter out.
  - **Banking a GCC nested function makes the permuter unavailable for the WHOLE TU.** `import.py`
    runs pycparser, which aborts on a nested function definition (`Syntax error in base.c ... before:
    {`), so no other function in that file can be imported either. Workaround: copy the TU with the
    nested-function parent deleted, place the copy INSIDE the repo (import.py rejects a path outside
    the project root — "Can't find root dir of project!"), and import from that.
  - **`setup-permuter.sh` resolves the C file by grepping for an `INCLUDE_ASM` stub, so it fails
    silently (exit 0, no output) once the body is inlined as C.** Call
    `venv/bin/python3 ./tools/decomp-permuter/import.py --settings permuter_settings_main.toml
    <c-file> <asm-file>` directly instead. The import COPIES the source into `nonmatchings/<fn>/`, so
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
  `sha1sum` that read a coincidentally-green stale ROM and reported MATCH across 3 commits that never
  built (a missing `VI_CTRL_ANTIALIAS_MODE_0` define plus 2 unresolved carve symbols).
- **Clean-rebuild when an enabler edits a shared vendored header.** The build tracks no header deps,
  so an incremental `make` recompiles only the file you touched, not the other consumers of a header
  you changed. When a mirror or enabler edits a widely-included header (e.g.
  `include/libultra/PR/os_version.h`), the banking SHA-1 must come from `make clean && make extract &&
  make`, not an incremental build (`docs/hazards.md#clean-rebuild-after-shared-header-edit`).
- **Library code under `src/libultra/`, `src/libkmc/`, `src/libnusys/`, `src/libnaudio/`,
  `src/libmus/`, and `src/mgu/` (plus the audio-lib include trees `include/libnaudio/`,
  `include/libnualstl/`, and `include/libmus/`) is clang-format-22 formatted** (stock Google with `SortIncludes: Never`; each dir carries a local
  `.clang-format` = `BasedOnStyle: Google` + `SortIncludes: Never`, which SUPERSEDES the old
  `DisableFormat: true`). These trees were reworked (2026-06-23) to the Code Complete ch31/ch32
  layout + comment style and deliberately diverge from the upstream SOURCE formatting, so they no
  longer line up line-for-line against the vendor `.c` for visual cross-referencing; the ROM stays
  byte-identical (comments/whitespace do not affect codegen, and coddog's fingerprint is asm-based,
  not source-text). Format any new or edited file in these trees with `clang-format-22 -i`, like the
  rest of the tree. `src/mgu/` (S103) holds the game-embedded ultralib gu/mgu matrix source (the
  Monegi variant, compiled at the game `-O2` profile, NOT the libultra `-O3` band; see
  `docs/hazards.md#game-region-mirror--o2-profile`).
- **Vendored-header placement (PO directive, S129).** When a mirror needs headers vendored, split them
  by the SDK's own public/internal layout: a **public** header (the SDK's `include/` side, what a
  consumer `#include`s) goes to `include/<lib>/`; a **source-private/internal** header (the SDK's
  `src/` side) goes to `src/<lib>/` mirroring the include sub-structure. Both the `include/<lib>/`
  and `src/<lib>/` audio trees carry a local `.clang-format` = `BasedOnStyle: Google` +
  `SortIncludes: Never` and are `clang-format-22` formatted (the audio-lib include trees
  `include/libnaudio/`, `include/libnualstl/`, and `include/libmus/` were reworked to the Code
  Complete ch31/ch32 style on 2026-06-29; the prior "include trees stay verbatim/unformatted" default
  no longer holds for them). `SortIncludes: Never` is load-bearing: stock Google sorts `#include`s and
  can break a byte-exact match. When an internal header shares a name with an existing one on the `-I` path (e.g.
  the n_audio_sc `synthInternals.h` vs `include/libultra/internal/synthInternals.h`), the build
  profile PREPENDS `-I src/<lib>` so the vendored SC copy wins (`mk/libnaudio.mk`, S129). Add the new
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

<hazard_index>

### Hazard index

When `pick_target.py` flags a hazard (or a match shows its symptom), read the matching
`docs/hazards.md` section. The detail behind each flag lives in that section, not here.

| pick_target.py flag / symptom | docs/hazards.md section |
| ----------------------------- | ----------------------- |
| upstream column is a lib | #upstream-mirror-pattern |
| `single-file-pack:<n>fn[…]` | #upstream-mirror-pattern |
| `upstream-fncount-mismatch:<m>vs<n>` | #upstream-mirror-pattern |
| `one-tu` | #upstream-mirror-pattern / #non16align |
| `refs-unplaced:<g>@0x…` | #recover-extern-refs-unplaced |
| `calls-unplaced:<fn>@0x…` | #calls-unplaced-function-callee-dual |
| macro-hidden extern | #macro-hidden-recover-extern |
| `jal-count-mismatch:<C>vs<asm>` | #near-verbatim-mirror-jal-count-mismatch |
| `block-reorder-sibling:<file>` | #near-verbatim-mirror-jal-count-mismatch |
| `file-static` | #file-static-bss-layout-conflict |
| `drop-static-mirror:<n>bss` | #file-static-bss-layout-conflict |
| `defines-data:<g>` / `data-static:<addr>` | #defines-data |
| `data-carve:<names>` | #defines-data |
| `twin-of:<file>` | #defines-data / #rodata-sibling-yaml-pattern |
| whole-region `.data`/`.rodata` sweep | #data-rodata-carve |
| `needs-header:<inc>` | #needs-header |
| `stale-header:os_version.h(<V>)` | #stale-vendored-header |
| `needs-define:<def>` | #needs-define |
| clean mirror SHA-miss, exact N×8B / fn shorter than its asm | #needs-define |
| clean mirror SHA-miss, single `li`/`addiu` immediate byte (not a reloc'd hi/lo) | #needs-define |
| `header-renames-symbol:<fn>@<hdr>` | #header-renames-symbol |
| `wrong-ghidra-name:<ghidra>-><correct>@<hdr>` | #wrong-ghidra-name-override |
| mirror parse error / undefined ref on a helper macro | #vendored-header-incomplete |
| parse-error cascade at macro-def lines / `stray '\'` after vendoring an SDK header | #crlf-vendored-header |
| `pack:<n>fn[…]` | #multi-function-segment-splitting-pack |
| `c-combined:<n>file[…]` | #multi-function-segment-splitting-pack |
| `unattrib-leaf:<addr>` | #multi-function-segment-splitting-pack |
| `non16align` | #non16align |
| `trailing-pad:<n>B@<align>` | #trailing-alignment-pad-after-a-c-mirror |
| `intrinsic-likely:<tu>.s` (and `(kmc-as)`, `(has-rodata:<sym>)`, `(asm-mirror-jtbl:<head>)`) | #asm-mirror-vendoring |
| `intrinsic-likely:cp0-asm(identify-TU)` | #asm-mirror-vendoring |
| `combined-subseg:<n>tu[…]` | #asm-mirror-vendoring |
| `intrinsic-likely` (bare) / `maybe-upstream:…` | #intrinsic-likely--maybe-upstream-signature-hints |
| `coddog-mirror:<file>@<pct>` | #coddog-cross-ref |
| `coddog-twin:<matched>!=<member-src>` | #coddog-cross-ref |
| `coddog-fncount-mismatch:<m>vs<n>` | #coddog-cross-ref |
| `c-combined-undercount:<m>vs<n>` | #coddog-cross-ref |
| `coddog-structural:<file>@<pct>` | #coddog-cross-ref |
| `coddog-partial:<m>of<n>fn` | #coddog-cross-ref |
| `static-name-collision:<name>@<addr>` | #static-name-collision |
| official static name shared across two instances / splat `Duplicate symbol detected` | #overlapping-symbols--allow_duplicated |
| SUPPORT_NAUDIO libmus mirror `alInit`→dead `n_al*` / bundled-synth `fncount-mismatch` | #libmus-bundled-n_audio-duplicate |
| `game-region-mirror:0x<vram>` | #game-region-mirror--o2-profile |
| `game-embedded:0x<vram>` | #game-region-mirror--o2-profile |
| clean asm-first seed, full-make SHA-miss, build .o shows fp-kept + no-CSE + arg-spill | #-o0-bootsdk-glue-file-profile |
| build SHA-miss, suspect compile flags (opt/-g/-fdelayed-branch) not the C | #profile-probe |
| public-API rename "resists" (`__*` symbol), vendored header macro may be inverted vs ultralib | #vendored-header-inversion |
| libultra leaf, bare std header | #per-library-standard-c-header-isolation |
| match locks ~0.9 on a lib target | #compile-profiles-libkmc--o-libultra--o3 |
| compiler rodata wrong offset / `rodata-literal:<addr>` | #rodata-sibling-yaml-pattern |
| `rodata-jtbl:<addr>` | #rodata-sibling-yaml-pattern |
| `…;owner-per-member` on a `rodata-jtbl`/`rodata-literal` | #rodata-sibling-yaml-pattern |
| MMIO fn, flat score, empty top_mismatches | #io_writeio_read-isolation-artifact |
| warm band, no hazard | #open-band-fast-path |
| `undefined reference` after mid-sprint sync-names | #make-sync-names-eviction-recovery |
| `caller-evict:<func_vram>@<file>` | #caller-evict |
| `carried-wall:<fn>[…][;characterization-only]` | #base-register-vs-displacement |
| loop cannot find label / reloc-name mismatch | #stale-top-level-asm-label-sync |
| decomp_loop bogus near-match (empty `base_text`, ref names the parent seg) just after a subseg split | #stale-parent-asm-relic-find_segment-mis-resolution-after-a-decompose-split |
| clean mirror SHA-miss, one field's high word | #mirror-cast-divergence-sign--vs-zero-extend |
| clean mirror SHA-miss, char load lb/sll-sra vs lbu/andi | #char-signedness |
| clean mirror SHA-miss, extra `jal __assert` / bare `assert()` / `bare-assert:<n>` | #assert-strip |
| clean mirror SHA-miss, same insn count reordered / jal-mismatch + no `coddog-mirror` | #near-verbatim-mirror-jal-count-mismatch |
| clean mirror SHA-miss, build instr-count < target (shorter) / collateral post-fn addr shifts | #cross-jump-tail-merge |
| classical top-tested loop's `if(x<lo){x++;continue;}` guard: build cross-jumps the two `x++;j loop` tails vs ROM branch-likely (`bnezl`+annulled `x++`) — re-express as nested `if(x>=lo){…;x++}else{x++}` (+ lazy global-base load for the reg cycle) | #cross-jump-tail-merge |
| `body-divergence-suspect:<file>@<pct>` | #cross-jump-tail-merge |
| build over-inlines a small callee the ROM `jal`s (or reverse), same/cross TU | #same-tu-inline-mismatch-definition-order--cross-tu-split |
| ROM `subu`+`bgez`/`bltz` for a `<` compare (not `slt`/`sltu`) | #same-tu-inline-mismatch-definition-order--cross-tu-split |
| clean mirror SHA-miss, one fn's frame immediates shift by a fixed delta (`_FINALROM`/build-config struct size) | #upstream-mirror-pattern |
| array-of-struct init loop shorter than target + a field stored twice (doubled store-offset) | #struct-init-loop-dup-store--dual-induction-var |
| permuter on a KMC-toolchain (libnusys/libultra/libkmc) mirror fn | #permuter-setup-for-kmc-toolchain-mirrors |
| libnusys carry whose carried fns == the upstream's `#ifdef NU_DEBUG` fns | #nu_debug-stock-not-custom-carried-perf-fn-triage |
| libnusys inline `divu`, build byte-perfect except 2 missing `nop`s after `mflo` | #libnusys-inline-div-mflo-hazard-nop |
| mirror global w/ dead-reload-after-store on `x++` or recompute-not-CSE of `a-b` | #volatile-global-tell-dead-reload--recompute-not-cse |
| Gfx* manipulation | #display-lists |
| global-`glistp++` DL fn with fill color computed from GLOBAL vars: color symbol-load hoisted into the glistp load-shadow (full reg cascade, structurally complete). FAITHFUL fix = retype the color triple as a `Color {s32 r,g,b;}` struct (mem-in-struct defers the loads) + inline the pack after the fill-color w0 store, NOT the permuter (S180); `& ~7` on a phys addr is game-specific | #display-lists |
| permuter on a `src/main/`/overlay -O2/F3DEX2 fn (`setup-permuter.sh --main`) | #permuter-setup-for-kmc-toolchain-mirrors |
| ROM has bare `sqrt.d`/`sqrt.s`, build links `jal sqrt`/`jal sqrtf` or a guarded `sqrt.{d,s}`+`c.eq.{d,s}`/`bc1t` | #double-sqrt-fast-math |
| ROM loop top-tested plain `beq`/`bne`, build inverts to guard-`j`+`beql` or reloads loop-invariant constants | #top-tested-loop-goto-local-hoist |
| ROM up-counts a loop (`addiu +1`/`sltiu`), build reverses to `li N-1`/`addiu -1`/`bgez` | #top-tested-loop-goto-local-hoist |
| near-match, ROM HOISTS a compiler-generated div/mod magic (`0x66666667`/`0x1B4E81B5`) or a loop char-literal into the preamble but the goto-loop rematerializes it each iter (use structured `while(1){…break}`) | #top-tested-loop-goto-local-hoist |
| ROM SELECTIVELY hoists (holds invariant array bases in regs but re-materializes a `%`/`/` magic at the loop tail); structured loop hoists both, plain goto de-hoists both (goto = partial fix) | #top-tested-loop-goto-local-hoist |
| clean per-fn match, full-make SHA-miss, hundreds of scattered 1-byte `%lo` diffs all `base-4` (decomposed one-tu rodata split) | #decomposed-one-tu-rodata-alignment-split |
| ROM reads `$ra` (reg 31) as a printf/log arg; `__builtin_return_address(0)` emits a stack-slot `lw` | #capturing-ra-return-address-as-a-call-argument |
| sentinel (`!=-1`) array walk matches except a 1-instr preheader swap (`move base` vs `li` const in the entry-`beq` delay slot, or a `-1` hoisted to an outer loop) | #indexed-vs-pointer-loop-strength-reduction |
| copy/scan loop re-indexes `arr[off]` each iter (insn count SHORT vs ROM's pointer+offset dual-IV), or a running ptr-add groups base-before-index (`base+i*s+c` vs the ROM's `&base[i*s+c]`) | #indexed-vs-pointer-loop-strength-reduction |
| ROM re-materializes `%hi(SYM)+idx` per access (no walking pointer) or keeps a loop bound inline at the exit test, and every structured spelling comes out SHORT | #goto-loop--loopc-never-runs-defeating-strength-reduction-and-bound-hoisting |
| ROM re-reads a count/bound global at MORE THAN ONE nesting level; build caches it in one pseudo and comes out a few instrs short | #multi-level-bound-re-read-array-element-form-not-a-cached-pointer |
| leaf spills the incoming `$v0` and never reloads it, and a near-match doc calls it a "spurious dead frame, not a nested fn" | #nested-function-static-chain-spill (check the CALLER for `addiu $v0,$sp,K`; writing it nested banks the parent too) |
| body byte-exact except 2-3 independent loads emitted in the wrong ORDER, register-to-value mapping already correct | one local reused for two successive values — split it (memory `one-variable-reuse-reorders-loads`) |
| build emits `addiu rX,<elemreg>,C; addu rX,<globreg>,rX` where the ROM emits `addiu rX,<globreg>,C` (same count, swapped operands + downstream reg permutation) | #fold-associate-which-operand-of-a-3-term-sum-carries-the-constant |
| string loop: ROM has a redundant `andi rX,rY,0xFF` after an `lbu`, or a `beql` whose annulled slot holds a one-instruction handler | #string-classify-loop-the-redundant-char-andi-and-the-branch-likely-handler |
| hand-rolled raw-DL-word block (per-glyph/per-sprite `u32` stores through a manual cursor) called a terminal regalloc/reorg wall | #display-lists (S258: find the gbi.h macro first) |
| classical fn structurally correct (rows align) but locks high on a pervasive hard-reg permutation (`i:s4↔s5`) + spill-slot order + scheduling | #pervasive-regalloc-classical-main |
| `void` classical fn mis-allocates at loop-entry/delay-slot, resists every body lever | #return-type-is-load-bearing |
| struct-array fn byte-matches with per-field base symbols but not the combined struct (link-identical) | #struct-access-folding-changes-scheduling |
| classical `switch(x)` dispatch via a compiler jump table (`jtbl_<vram>`, `sltiu`+`jr $v0`), esp. w/ sparse inner cases or `a==K1\|\|K2` | #switch-jtbl-dispatch |
| clean per-fn match, full-make SHA-miss, lone `slti`<->`sltiu` at a switch/range bound-check (global signedness) | #switch-jtbl-dispatch |
| clean fn byte-exact except a fixed-global re-load after a nonscalar `arr[idx]=0` store (build CSE-forwards, 1 load short); read the global as `G[0]` array-elem for MEM_IN_STRUCT_P | #mem-in-struct-scheduling-lever |
| ROM cond-branch is plain `beqz`+`li v0,CONST`+`move v0,<scratch>` but build emits branch-likely `beqzl` skipping the lone `li v0,CONST` (return-var coalesced to v0) | #register-reuse-nudge-classical-regalloc |
| classical fn's global load/store schedules differently (build pipelines indep load-stores the ROM keeps strict-`$f0`-pairs, or hoists a `& K` flag load past a pointer store the ROM keeps late+`nop`) | #mem-in-struct-scheduling-lever |
| clean fn byte-exact except a fixed-global struct/array field RMW (`+=`/`-=`): build folds `ARR[k].field` into a `la` base reg where the ROM re-materializes `%hi/%lo` (+ a cascading reg permutation) | #offset-0-symbol-re-materialization |
| clean fn byte-exact except the ROM RELOADS a just-stored global field with no intervening store (`sw v1,f; lw v1,f; sw v1,g` for `g=f`); build forwards the stored reg (1 load short) | #volatile-view-cse-reload |
| structural-complete regalloc miss where a re-materializable CONSTANT loop-invariant (`&arr[K]`) is callee-saved (crosses the guarding call) but the ROM wants it caller-saved, displacing a call-arg copy-pref (mask→$a0 vs $t1); move its define point AFTER the call + inline the sentinel | #loop-weight-and-live-length-regalloc-steering |
| delay-slot / instruction-count analysis off by ±1 (a "1 word short" / "needs a synthetic no-op" verdict read from GCC `-S`, not the assembled `.o`) | #assembler-differences--byte-cmp-spot-check |
| structural-complete regalloc miss where a CONSTANT loop-invariant (`&arr[K]`) is caller-saved (a competitor) but the variable-index sibling's `&arr[i]` is callee-saved; flips a call-arg copy-pref (mask→`$a0`) | #loop-weight-and-live-length-regalloc-steering |
| classical fn full-make SHA-miss and a same-file sibling reads wrong data addr (`%lo` off a fixed delta, whole `0x8010xxxx` .bss region shifted) | #short-text-shifts-flowing-bss |
| classical fn full-make SHA-miss where a too-LONG fn overflows its decomposed subseg and the auto-`.bss` `D_<vram>` symbols ALL float to `name+N` (looks like symbol/reloc corruption, not a length bug) | #short-text-shifts-flowing-bss |
| classical struct-array-of-`.bss` fn: build keeps a base pointer (`offset(v1)`) where ROM re-derives each field via `%hi/%lo(D_<field>)`, or the reverse on an `&arr[i]` self-store (`sw v1,0xC(v1)` vs re-derived) | #struct-array-of-bss-direct-index-vs-base-pointer-var |
| classical constant-dispatch (small selector to CONST results via a shared return var; ROM per-case `beql cond,RETURN`) locks pervasive BB-layout, resists if-else/switch/ternary/goto-end, lone `if(x==K)v=CONST` branchless-if-converts | #goto-dispatch-branch-toward-vs-branchless |
| classical call result the ROM holds in `$a0` (`move a0,v0` / `move v0,a0` bookends) but build coalesces into `$v0` (shorter); distinct-var/extra-use levers fail | #call-result-a0-vs-v0-single-allocno |
| pervasive classical BB-layout/regalloc/scheduling miss resists every idiom and the permuter plateaus | #compiler-source-fan-out-escalation-above-the-permuter |
| source dive proves a regalloc/codegen artifact unreachable from faithful C; need the missing idiom OR a compiler-config/patchlevel confirmation (mine sibling KMC-2.7.2 decomps + a cross-compile probe) | #cross-project-matched-corpus-mining |
| classical fn byte-exact except a 3-word branch-direction triple (bnez/beqz+delay) on a `cond?t\|K:t` store/print through a reused loaded-var arg | #cse-make-regs-eqv-branch-fold |
| classical fn byte-exact except a 3-instr reg swap in `if(fabsf(x)<K)` (target `abs.s f2,f0`+const in `f0`; build `abs.s f0,f0` in-place+const in `f2`); permuter plateaus | #abs-coalescing-reg-swap |
| structural-complete regalloc miss = which value wins an earlier caller-saved reg; before "irreducible" | #loop-weight-and-live-length-regalloc-steering |
| structural-complete regalloc miss where a call-crossing PARAM/local grabs `$s0` and rotates the loop vars off `s0/s1/s2` (local-alloc pre-empts before global priority); fix = mutate the param IN PLACE (`p=f(p)`) to make it a global qty; diagnose with the `-dg`/`-dl` allocno dumps | #loop-weight-and-live-length-regalloc-steering |
| tempted to structure/clean a matched goto-loop fn's loops; zero-goto rewrite attempt | #loop-weight-and-live-length-regalloc-steering |
| leaf fn opens `addiu sp,-8`+`sw $v0,0(sp)` never reloaded (dead spill of incoming `$v0`) AND its caller sets `$v0=&sp[K]` before each `jal` = GCC nested function (static chain in `$v0`); bank as a nested fn in the parent's TU, or carry the orphaned child | #nested-function-static-chain-spill |
| leaf reads its arg/data via incoming `$v0` (`move a0,v0`/`lw x,K(v0)`) with a caller `addiu $v0,$sp,K` before the `jal` = chain-USED GCC nested function (NOT a `$v0`-arg-convention wall); crack once its parent's TU is decompiled by writing it nested | #nested-function-static-chain-spill |
| permuter base.c with `__asm__ __volatile__(...)` aborts pycparser (`before: __volatile__`); b64literal-wrap that line by hand | #permuter-setup-for-kmc-toolchain-mirrors |
| permuter "best" on a goto-loop fn beats the hand-derived structural floor by a suspicious margin | #permuter-goto-backedge-liveness-unsound |
| classical fn structure/scheduling/hoisting fully matched, only residual = target reserves a DEAD stack frame (`addiu sp,-N`/`+N`, zero `sp)` access) + the reg permutation it drives; no source trigger (address-taken forces real sp loads) | #dead-frame-reload-artifact-regalloc-wall |
| classical fn byte-identical body, ONLY residual = the two `addiu sp` frame immediates + `ra` slot offset, NO reg permutation / NO signed-divide (PURE dead frame) — CRACKABLE via `s32 unused[(delta)/4]` (delta = ROM_frame − 0x18) | #dead-frame-reload-artifact-regalloc-wall (S251 pure-variant subsection) |
| >=2 alloc-artifact walls (dead-frame / non-coalesced reg-copy / target-spills-but-build-doesn't = "mine more optimal than target") CLUSTER in one classical TU whose simple fns bank clean | #profile-probe (run ONE TU-wide probe before N per-fn dives) |
| signed divide-by-const dividend/magic in the wrong two regs (`sra r,r,0x1f` reg = dividend, `lui 0x<magic>` reg = magic); flippable-in-isolation (return/reg-2-SET → local-alloc suggestion pass), but a VOID/callless/returnless loop-fed leaf is deterministically magic-in-low-reg; cross-project matched-corpus mining is the escalation | #signed-divide-const-v0v1-quotient-destination |
| tempted to use the plain `register` keyword (no `asm`) as a regalloc match lever — it is a zero-`.text`-effect no-op at -O2 (REG_USERVAR_P absent from local-alloc/global priority; DECL_REGISTER ignored when obey_regdecls==0) | #signed-divide-const-v0v1-quotient-destination |
| straight-line (1 basic block, `.flow` dump) classical fn locks on a pure s-register permutation + 1 independent-store schedule move; source levers don't move it | #local-alloc-qty-permutation |
| `nonmatching-func`/`decomp_loop` isolated object diverges from the in-tree build of the same 1-BB fn | #local-alloc-qty-permutation |
| large straight-line dump fn (one `T* p` param, `sub=&p->big_substruct` at fixed offset, many `sub->field` accesses): ROM materializes `p+C` as base (`addiu sN,a0,C`), build keeps the PARAM base + folds `+C` into every displacement; pervasive base-reg + uniform-offset diff, `match_count==total_rows`+empty `top_mismatches` at LOW percent | #cse-derived-pointer-base-canonicalization |
| clean fn byte-exact except N `r` rows on ONE data-access chain: ROM materializes a full base addr into a reg + `0(reg)` deref, build keeps `%hi`+index + folds `%lo`/const into the load/store DISPLACEMENT; struct-array or `T* row=` intermediates BACKFIRE (pervasive regalloc shift), permuter does not flip it | #base-register-vs-displacement |
| near-match at MID percent (not high, not near-zero) where EVERY residual row is a `sym+K`-vs-sibling reloc-addend on contiguous globals (struct/array base+addend form vs ROM separate per-field symbols); link-both-and-cmp with real addresses proves byte-exact -> isolation artifact, NOT a base-vs-disp wall | #isolated-compile-caveat |
| new C references a `D_<addr>` global whose `build/*.map` addr != its name (shifted `.NON_MATCHING` carve, e.g. name+0x10); referencing it corrupts the whole region incl. banked siblings | #base-register-vs-displacement (data-carve subsection) / #defines-data |
| `p ? field : sentinel` accessor (call returns ptr, return a field-or-default): build emits short branch-likely `beqzl` vs ROM `bnez/nop/j/li`; ternary + early-return both collapse; whole-file ±1 `cmp` cascade from the 2-insn deficit | #value-select-if-else-vs-branch-likely |
| null-guard `if(p){…}` byte-exact except the guard `beqz` delay slot (ROM `nop`, build steals the block's first insn); fires when body-first is pointer-INDEPENDENT (`li`/`sll`), matches free when body-first DEREFS the guarded ptr | #delay-slot-fill-of-a-null-guard-beqz |
| default-sentinel return var (`result=0`/`-1`) forces an extra saved `sN` + frame grows `0x18`->`0x20` + regalloc cascade because it is init BEFORE a call (crosses it -> callee-saved); init it AFTER the call | #default-return-var-must-init-after-call |

</hazard_index>

</conventions>

<cross_repo_sync>

## Cross-repo sync (Ghidra workspace at `~/development/reversing/ghidra/mariogolf64/`)

- **Names (Ghidra to decomp):** `make sync-names` pulls curated function and data names into
  `ghidra_symbols.txt` (gate-only). The sync skips any address already in `symbol_addrs.txt` (the two
  files stay disjoint). See `docs/hazards.md#make-sync-names-eviction-recovery` for the
  mid-sprint-break recovery.
- **Names (decomp to Ghidra):** the agent writes the curated name into `symbol_addrs.txt` directly
  (add-only, disjoint), then `make extract` + `make` to confirm the ROM stays green. Propagating the
  name back into the Ghidra workspace
  (`python3 ~/development/reversing/ghidra/mariogolf64/scripts/sync_decomp_names.py
  --import-from-decomp`) is a cross-repo follow-up, surfaced in the final response.
  - **Renaming a symbol that already lives in `ghidra_symbols.txt` (S151).** You cannot hand-edit
    `ghidra_symbols.txt` (owned by the sync) and cannot add the address to `symbol_addrs.txt` while
    the old name is still in `ghidra_symbols.txt` (splat errors on the duplicate address). The move:
    add the new name to `symbol_addrs.txt` (add-only), then `make sync-names` — `sync_decomp_names.py`
    DROPS any address present in `symbol_addrs.txt` from the generated `ghidra_symbols.txt`, so the old
    name evaporates and the two files stay disjoint (verify `make sync-names` reports `removed=1`,
    `renamed=0`). S151 renamed `gfx_dl_write_cursor` -> `glistp` this way.
  - **Ghidra MCP idiomatic-name bypass (S151).** The MG64 Ghidra MCP enforces Hungarian `g_`+type
    globals and REJECTS a plain idiomatic name (`glistp`, the SGI/nusys demo name) on `rename_data`/
    `set_global` (`name_quality`/`missing_g_prefix`). Bypass per-call with
    `rename_or_label(address, name, strict_mode="off")` — `strict_mode` is `off`/`warn`/`enforce`; the
    decomp is authoritative for names, so `off` is correct when the decomp wants the demo idiom over
    `g_pGlistp`. (memory: [[ghidra-mcp-naming-enforcement-bypass]].)
- **Structs:** pull from Ghidra MCP, cross-check against `re_tracking/structs.yml`, append to
  `include/structs.h`. Discrepancies between MCP-live and the snapshot are surfaced and block the
  write.
- **Memory map:** read-only on both sides.
- **Forbidden agent edits:** `ghidra_symbols.txt` (except via `make sync-names`), `mariogolf64.ld`,
  `re_tracking/*.yml`, `reloc_addrs.txt`. Agent-editable: `mariogolf64.yaml` subseg
  flip/split/path-qualifier lines only, and `symbol_addrs.txt` add-only.

</cross_repo_sync>
