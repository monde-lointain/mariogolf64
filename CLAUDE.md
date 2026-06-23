# Mario Golf 64 Decomp workflow

This project decompiles Mario Golf 64 (N64) one function at a time. Tooling: splat (file splitting),
KMC GCC 2.7.2 (`tools/cc/gcc`), m2c (asm to C seed), asm-differ (library mode), decomp-permuter
(escalation), the Ghidra MCP bridge (live decompile + struct/symbol lookup, port 8089; every call
passes `program="baserom.z64"`), and coddog (`make coddog-sweep`: fingerprints MG64 fns against
ultralib VERSION_J to reveal an un-named `func_`'s upstream source; see
`docs/hazards.md#coddog-cross-ref`).

This file is the always-loaded core: the workflow model, the Scrum cadence, and the every-sprint
rules. Detail lives in two on-demand references:

- `docs/hazards.md`: the hazard playbooks (what to do when `pick_target.py` flags a hazard). The
  hazard index below maps each flag to its section.
- `docs/coding-style.md`: the C quality bar and naming conventions for code promoted into the tree.

Prompt authoring for this workflow follows `PROMPT_GUIDELINES.md` at the project root (see
Conventions).

## Workflow at a glance

Two slash commands are the Scrum gates. Between them the agent runs the execution loop inline: one
function at a time, no per-function stop, a single serial session (no MCP lock). `SPRINT.md` is the
resume surface when the middle spans context windows.

Effort: run the gates as cheap triage; run classical decomp and the permuter at higher effort, where
the extra reasoning pays off. Parallelism: read-only context gathering may run in parallel (read
`BACKLOG.md`, run `pick_target.py`, and read the flagged hazard sections at once); MCP, build, and
edit steps stay serial, since the session holds no MCP lock and the build is one shared tree.

## Slash commands

- **`/sprint-plan [scope]`**: Plan gate. Ranks candidates with `tools/pick_target.py`
  (smallest-first), proposes one small increment plus its enablers, gets PO approval, then performs
  and validates the flip (subseg yaml edit + `symbol_addrs.txt`, optional `make sync-names`, then
  `make extract && make` for the green ROM), and writes `SPRINT.md`. Then runs the execution loop over
  the committed backlog.
- **`/sprint-review`**: Review gate. Verifies the ROM-SHA-1 DoD on the increment, takes PO scope
  sign-off, and is the single apply-point for the sprint's buffered workflow-improvement edits. It
  appends the `RETRO.md` digest and `BACKLOG.md` carry-overs.

## Execution loop (inline)

After `/sprint-plan` writes `SPRINT.md`, work the committed backlog autonomously, smallest-first.
Ghidra MCP is used inline at seed time. For each target function:

1. **Resolve** the function's subseg in `mariogolf64.yaml`. `hasm` means permanent asm: skip it. The
   subseg was already flipped to `c` at the gate.

2. **Mirror branch** (the `upstream` column is a lib, not `none`): copy the upstream `.c` verbatim to
   its mirrored `src/lib<name>/...` path; add any missing headers verbatim under
   `include/lib<name>/...`; then `clang-format-22 -i` the copied files (these trees are now formatted;
   formatting does not change codegen); `make`, and the ROM SHA-1 must equal the baserom. The proof is
   the full-`make` SHA-1 (verbatim-then-formatted copy plus green ROM equals match); no byte-`cmp`. See
   `docs/hazards.md#upstream-mirror-pattern`. Before declaring a clean mirror, reconcile the
   upstream's call and data-ref list (including one level of macro expansion) against the name files,
   and handle any flagged hazard via the index below.
   - libultra source is `~/development/repos/ultralib` (gcc.mk / KMC-N64 profile,
     `-DBUILD_VERSION=VERSION_J`), NOT `libultra_modern` (deprecated 2.0L, casts diverge; it is an
     `additional working dir` but is not the source). This pin holds for both the C mirror and the
     hand-asm `intrinsic-likely:<tu>.s` asm-mirror (`docs/hazards.md#asm-mirror-vendoring`): the
     project mirrors ultralib's `gcc.mk` profile (`LIBULTRA_CFLAGS` for C, `LIBULTRA_ASFLAGS` for
     vendored asm TUs, both in `mk/libultra.mk`). libkmc is `~/development/repos/libkmc`; libnusys is
     the n64sdkmod nusys tree.

3. **Classical branch** (no upstream, or a hazard routes here): seed, iterate, spot-check, finalize.
   - **Seed** `nonmatchings/<func>/base.c`: fetch the Ghidra decompile via MCP, then
     `venv/bin/python3 tools/seed_c.py --func <placeholder> --parent src/<seg>.c`. The asm
     (`disassemble_function`) is ground truth, not the Ghidra decompile (see
     `docs/hazards.md#decompile-vs-asm-authority`). Use the decompile for shape and types; translate
     the logic from the instruction listing.
   - **Iterate** at most 25 times: `venv/bin/python3 tools/decomp_loop.py --func <placeholder>`, then
     parse the JSON. `score == 0` is a candidate; 5 consecutive `compile_ok == False` means a broken
     seed, so stop; otherwise read the top mismatches, edit `base.c`, and re-run. Run the permuter
     (`./run-permuter.sh`) only at `percent >= 0.97`. For a near-miss whose only diff is which scratch
     register holds an intermediate, see `docs/hazards.md#register-reuse-nudge-classical-regalloc`.
   - **Spot-check** (only at score 0): byte-level `cmp` of the in-tree compiled `.text` against the
     isolated one. The `cmp` is the truth, not the mnemonic diff (see
     `docs/hazards.md#assembler-differences--byte-cmp-spot-check`). A non-zero score with empty
     `top_mismatches` AND `match_count == total_rows` is an isolation artifact (struct-field reloc
     addend, or extern HI/LO16), not a near-miss: go straight to the in-tree spot-check and full-make
     SHA-1; do not iterate C or reach for the permuter (see `docs/hazards.md#isolated-compile-caveat`).
   - **Finalize** (only if the spot-check passes): inline the body into `src/<seg>.c`, drop the
     `INCLUDE_ASM` line, `clang-format-22 -i` (now applies to every tree, including `src/libultra/`,
     `src/libkmc/`, `src/libnusys/`, and `src/mgu/`), then `make` until `build/mariogolf64.z64: OK`
     and SHA-1 == baserom.

4. **Bank** the function (only at score 0 plus spot-check plus full-`make` SHA match). Give it its
   curated Ghidra name (add to `symbol_addrs.txt`, rename in the body, re-`make`). On `git commit`,
   stage the `make extract`-regenerated artifacts too (`undefined_syms_auto.txt` and
   `mariogolf64.ld`): they change on a subseg flip or `symbol_addrs.txt` add and must travel with the
   commit, or the regen bleeds into the next sprint's dirty tree (S85 to S86: a `D_`-to-named
   BOOT_GLOBALS regen left uncommitted surfaced as a stray diff at the next gate). `git status` should
   be clean after commit. Append a standup line to `SPRINT.md`, and record the run's numbered
   "Suggested workflow improvements" into the `SPRINT.md` buffer, applied only at `/sprint-review`,
   never mid-sprint.

A function that locks below `0.97 percent`, needs the permuter, or hits a BSS-layout or alignment
wall is a spike: note it, carry its file to `BACKLOG.md ## Carry-overs`, and move on. Do not weaken
the DoD to bank a spike. The increment (the `src/<seg>.c` file) banks only when its last
`INCLUDE_ASM` stub is gone.

The context window auto-compacts as it fills, so do not stop early on budget concerns; save progress
to `SPRINT.md` (standup line + suggestion buffer) as each function banks, so a fresh window resumes
from it.

## Scrum operating model

The tactical match-loop runs inside a thin Scrum cadence (McConnell, *More Effective Agile*).
Artifacts: `BACKLOG.md` (PO-owned ordering rationale, enablers, carry-overs), `SPRINT.md` (gitignored
ephemeral board and resume surface), `RETRO.md` (consolidated digest), `VELOCITY.md` (story-point
dashboard). Target selection is `tools/pick_target.py`, not a stored roadmap.

- **Roles.** Product Owner is the user (backlog priority, goal approval, scope sign-off, retro
  selection). Development Team is the agent (executes the gate enablers after PO approval) plus
  `tools/` plus the KMC-GCC / asm-differ / ROM-SHA-1 oracle.
- **Cycle.** `/sprint-plan` (PO approves a goal and small backlog; agent performs and validates the
  flip enablers), then autonomous execution, then `/sprint-review` (DoD verify, scope sign-off,
  retro), then repeat.
- **Sprint = one `src/<seg>.c` file to md5-candidate (0 `INCLUDE_ASM` stubs), or one cohesive subseg
  cluster.** The function is the task; the file or cluster is the increment. Cap small early: 1
  upstream file, or about 3 to 4 functions, so the middle fits a context window. For a homogeneous
  sibling set (same upstream pattern, same uniform enabler), fill the 3-to-4 cap, since per-file
  all-or-nothing banking makes the marginal sibling near-free. A heterogeneous batch (mixed hazards)
  trims to the cleanest 2. Review fires when every committed item is banked or spiked/carried.
- **Definition of Ready.** Subseg flippable (not `hasm`); coarse size known; upstream-mirror
  availability noted; hazards flagged. Enablers (subseg flip plus `make extract`, multi-file split,
  `symbol_addrs.txt` additions) are performed by the agent at the plan gate after the PO approves the
  goal/scope, and validated there: `make extract && make` must still produce the green baserom ROM
  with the new stubs. This gate build-check is load-bearing: it once surfaced a missing-`cpp`
  toolchain regression that was silently emptying every asm object. A name sync (`make sync-names`) is
  gate-only, never mid-sprint (a rename breaks in-flight links until `make extract && make`).
- **Definition of Done (binary; the oracle already exists).** Per-function: score 0, byte-`cmp`
  spot-check, inlined, clang-format, full `make` to `build/mariogolf64.z64: OK`, SHA-1 == baserom, and
  committed. Per-sprint: every fn in the file inlined (0 stubs), ROM SHA-1 matches, committed. The ROM
  SHA-1 is green at every commit (un-decompiled parts fill from extracted asm), so at review the
  `make` + SHA-1 paste is a regression guard; the value delta is the matched-count /
  md5-candidate-files number. **Never commit a non-matching fn.**
- **Spike + carry-over.** A function that blocks its file's DoD (stuck-far below 0.97, needs permuter,
  BSS-layout-conflict, subseg-alignment) is a spike: note it, carry its file or cluster to the next
  sprint, and count credit at the function level. Do not weaken the DoD to bank a spike.
- **Quality counter-metric.** Track stuck-far + permuter-escalated + carried + re-opened per sprint,
  reported next to the match count so the count cannot be gamed by premature spiking.
- **Process changes are retro-gated.** The "Suggested workflow improvements" the execution loop emits
  are recorded into `SPRINT.md` during the sprint and applied only at `/sprint-review`, never
  mid-sprint, so the sprint's matching behavior stays fixed and its measurement stays clean. The retro
  is the single apply-point for edits to `CLAUDE.md`, `docs/*`, `tools/*.py`, and the two command
  files. A PO-directed out-of-band rework of these surfaces is the one exception, by direct request.
- **Two-gate close.** Review accepts the product (DoD plus scope); retro improves the process (the
  single apply-point for tooling edits).
- **Resume protocol.** On a fresh mid-sprint session: read `SPRINT.md`, reconcile banked work via
  `git log` since the snapshot, and verify Ghidra MCP connectivity (`list_instances`, port 8089)
  before resuming. The context window auto-compacts, so continue from `SPRINT.md` rather than wrapping
  up early.

## Story points

Lightweight estimation over the Scrum cadence. Scale: Fibonacci 1, 2, 3, 5, 8, 13. Current phase
`regime: mirror`. `VELOCITY.md` is the committed dashboard with the full rules and anchors; this is
the summary.

- **Deterministic seed (v1).** `pick_target.py`'s `pts` column is the a-priori seed (a pure function
  of `size`, `upstream`, `band`, `nfns`, `hazards`). A cluster seed is the sum of its files' seeds.
  `pts` is display-only: it does not change the smallest-first sort. A `blk` seed is an un-pickable
  needs-header (a DoR reject); swap the increment rather than commit it.
- **The 8-point decompose gate (v1).** A seed of 8 or 13 must NOT run as a normal 1-increment sprint:
  decompose it (split the subseg at the upstream-file or function boundary) or pull a scaffolding
  enabler as the goal instead. Applied at the `/sprint-plan` gate. This prevents an all-or-nothing
  bank stall. Expect it to fire once the mirror band is mined out and classical units dominate.
  - **Verbatim-mirror exemption (S64; generalized S69).** A seed-8/13 increment may run as a normal
    1-increment sprint when ALL of these hold:
    - (a) `regime: mirror` plus a verbatim copy of a single upstream file. A drop-def mirror
      qualifies: the function bodies are byte-verbatim, and only the file-scope data DEFS become
      `extern` decls, which emit nothing, so it banks atomically exactly like a pure `cp` (S86
      `os/timerintr.c`, pts-8 single-file-pack, banked first-try seed-only). A `.data`-carve mirror
      qualifies even more directly: the data DEFS STAY defined and the `.data` is carved to its placed
      vram, so it is a pure verbatim `cp` of the whole file (S116 `nucontgbpakmgr.c`, pts-8
      single-file-pack, banked first-try seed-only, the first libnusys `.data` carve).
    - (b) The decompose path is mechanically blocked: the increment is a `single-file-pack` (every
      member fn comes from ONE upstream `.c`), so there is no inter-file boundary to split at and an
      intra-file split cannot be independently mirrored. This holds regardless of inner-boundary
      16-alignment: S64 `lookathil`'s inner boundary was `non16align`, S69 `lookat`'s was 16-aligned,
      and both were decompose-blocked (you cannot mirror half a source file). `pick_target.py`'s
      `single-file-pack:<n>fn[…]` tag (S67) is the signal; the old `non16align`-on-the-inner-boundary
      test was one mechanical case of it, not the rule.
    - (c) Every callee is placed and all names are curated (the "no residual variance" condition).

    The gate's all-or-nothing concern guards CLASSICAL iteration stalls; a verbatim single-file mirror
    banks atomically (or is a quick spike), so a size-only 8/13 is a false fire, the same false-flag
    class the hazard detectors keep retiring. Document the exemption in `SPRINT.md ## Estimate`; the
    increment stays seed-only (S64 `gu/lookathil.c` + S69 `gu/lookat.c`, both pts-13, banked
    first-try). The exemption never covers classical or multi-file packs (a `pack` or `c-combined` of
    2 or more distinct upstream files decomposes at the file boundary as usual).
    - **Sub-100 coddog hedge (S121; corrected S127).** A `single-file-pack` with a sub-100 coddog score
      (e.g. `@99.99`, the same near-verbatim tell that flags block-reorders) still qualifies for the
      exemption, but budget a **body-divergence diagnosis pass**: the "banks atomically OR is a quick
      spike" assumption can be violated by a per-fn divergence (block-reorder, or a **game-modified
      body** — an extra branch/store the literal upstream lacks) that turns the "quick spike" into a
      PARTIAL bank (S121 `nucontrmbmgr.c`: 8/9 banked C, 1 carried as `INCLUDE_ASM`). Hedge the estimate
      "<=1 re-attempt" and expect a per-file score of 0 pt if the file ends partial (the matched-fn
      count is the value signal, not the file point). **Do NOT conclude "compiler wall" before proving
      the body emits the target's exact stores + values:** the S121 carry was misframed as an
      unbankable `#cross-jump-tail-merge` for 5 sprints, then banked S127 with a one-branch FORCESTOP
      fix (`state = STOPPED` on `osMotorInit` error). See `#cross-jump-tail-merge`.
    - **coddog 99.99 == STRUCTURE, not bytes; the exemption-GUARD (S123).** A `coddog-mirror:<f>@99.99`
      can mask a HEAVILY game-customized file where MOST bodies diverge, not just a block-reorder
      (S123 `nusched.c`: a game scheduler that shares only the nusys skeleton). The verbatim-mirror
      exemption must NOT fire when the pack carries a **customization tell**: a `jal` to a non-lib
      `func_<vram>` game callee (a callee that is not `os*`/`nuSc*`/`al*`/lib), OR a large
      `jal-count-mismatch` not explained by a known macro/version artifact. Those signal pervasive
      per-fn divergence -> route to the CLASSICAL track with the **mixed bank-stock-carry-custom**
      plan (next bullet), NOT a seed-only atomic mirror. Verify BODIES before trusting a 99.99 row:
      diff the asm against the upstream for the heavy functions, and run the nusys/libultra version
      triage (`docs/hazards.md#upstream-mirror-pattern`) before concluding the divergence is custom.
      (pick_target.py automation to price the non-lib-`func_`-callee tell is a tracked follow-up;
      until then the gate applies this guard by reading the pack's asm callees.)
    - **Mixed mirror+INCLUDE_ASM partial bank is first-class (S121 generalized, S123).** A
      `coddog-mirror` file can be PARTIALLY stock: some fns byte-match the upstream, others are
      game-customized. The right play is **bank-stock-carry-custom** — write the stock fns as C and
      keep the customized fns as `INCLUDE_ASM` in the same `src/<seg>.c` (the ROM stays green; the
      file is partial / not md5-candidate until the customized fns are classically decompiled). Plan
      such a file as a `regime: mixed` increment, not a seed-only mirror: per-file all-or-nothing
      means the partial file banks 0 pt, and the matched-fn count is the value signal (S123 nusched.c:
      10/14 banked C, 4 carried, 0 pt, +10 matched).
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

## Conventions (every-sprint)

These apply regardless of hazard. Hazard-specific procedures are in `docs/hazards.md` (see the index
below).

- **One function at a time.** `pick_target.py` ranks (smallest-first); you pick the target.
- **Scratch dir** `nonmatchings/<func>/` (gitignored, shared with the permuter).
- **Python tools run via the venv:** `venv/bin/python3 tools/X.py` (system python lacks asm-differ
  deps and is PEP-668-locked).
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
- **yaml authority.** The agent edits `mariogolf64.yaml` subseg flip/split/path-qualifier lines only
  (never the segment or memory-map structure); `mariogolf64.ld` and segment boundaries are off-limits.
  Subseg flips, multi-file splits, and the `make extract` that regenerates the scaffold are gate
  actions (or inline when a split is needed mid-flight).
- **`hasm` subsegments stay raw asm forever** (entry stub, `__muldi3` libkmc math module, RSP
  microcode entries). The execution loop refuses these. The project sets `hasm_in_src_path: True`, so
  a `hasm` `.s` lives under `src/<dir>/<stem>.s` (each `hasm` yaml line carries a `<dir>/<stem>` name
  qualifier) and its object builds to `build/src/<dir>/<stem>.o` via a path-based pattern rule in the
  `mk/*.mk` fragments (the top `Makefile` is a thin skeleton that `include`s them) that picks the
  assembler by tree (KMC for `src/libultra/` and `src/libkmc/`, modern GAS for the rest).
- **Permuter** (`./run-permuter.sh`) runs only when asm-differ's `percent` is at least 0.97. Below
  that, iterate on C or reconsider whether the subseg should be `hasm`.
- **Decomp is authoritative for names** (per the Ghidra-workspace
  `docs/re/coordination/decomp_coordination.md`).
- **Match finalization is three steps:** inline the body into `src/<seg>.c`, `clang-format-22 -i`
  (now applies to every tree, including `src/libultra/`, `src/libkmc/`, `src/libnusys/`, and
  `src/mgu/`), then full `make` until ROM SHA-1 matches.
  Spot-check passing is not ROM matching; the final `make` proves the match. Gate the `sha1sum` on
  `make` succeeding (confirm the `build/mariogolf64.z64: OK` line first): a failed link leaves the
  previous `.z64` in `build/`, so `sha1sum` on a stale ROM false-positives. **Always verify with
  `tools/verify-rom.sh [--extract]`, never a hand-rolled `make ... ; sha1sum`**: the helper asserts
  the `OK` line before trusting the hash. S111 burned a whole review on a hand-rolled ungated
  `sha1sum` that read a coincidentally-green stale ROM and reported MATCH across 3 commits that never
  built (a missing `VI_CTRL_ANTIALIAS_MODE_0` define plus 2 unresolved carve symbols).
- **Clean-rebuild when an enabler edits a shared vendored header.** The build tracks no header deps,
  so an incremental `make` recompiles only the file you touched, not the other consumers of a header
  you changed. When a mirror or enabler edits a widely-included header (e.g.
  `include/libultra/PR/os_version.h`), the banking SHA-1 must come from `make clean && make extract &&
  make`, not an incremental build (`docs/hazards.md#clean-rebuild-after-shared-header-edit`).
- **Library code under `src/libultra/`, `src/libkmc/`, `src/libnusys/`, and `src/mgu/` is
  clang-format-22 formatted** (stock Google with `SortIncludes: Never`; each dir carries a local
  `.clang-format` = `BasedOnStyle: Google` + `SortIncludes: Never`, which SUPERSEDES the old
  `DisableFormat: true`). These trees were reworked (2026-06-23) to the Code Complete ch31/ch32
  layout + comment style and deliberately diverge from the upstream SOURCE formatting, so they no
  longer line up line-for-line against the vendor `.c` for visual cross-referencing; the ROM stays
  byte-identical (comments/whitespace do not affect codegen, and coddog's fingerprint is asm-based,
  not source-text). Format any new or edited file in these trees with `clang-format-22 -i`, like the
  rest of the tree. `src/mgu/` (S103) holds the game-embedded ultralib gu/mgu matrix source (the
  Monegi variant, compiled at the game `-O2` profile, NOT the libultra `-O3` band; see
  `docs/hazards.md#game-region-mirror-o2-profile`).
- **Use ultra64.h types** (`s32`/`u64`/`vu32`/`f32`/...) in decomp C, never raw
  `int`/`long long`/`volatile unsigned long`.
- **Prompt authoring.** Every prompt surface in this project (`CLAUDE.md`, `docs/*`,
  `.claude/commands/*`, and the artifact templates) follows `PROMPT_GUIDELINES.md` at the project root
  when created or modified. That file is a managed copy of the canonical guide; re-sync its
  `Prompting Claude Opus 4.8` section when Anthropic updates the upstream.

### C naming (compact; full guide in `docs/coding-style.md`)

| Identifier            | Convention   | Example                |
| --------------------- | ------------ | ---------------------- |
| Function / variable / parameter / member | `lower_case` | `frame_count` |
| Struct / Enum (type)  | `CamelCase`  | `PacketHeader`         |
| Enum constant / macro / global+static const | `UPPER_CASE` | `MAX_CONNECTIONS` |

That table is the case convention. Naming quality (descriptive, problem-oriented, length-by-scope)
follows `docs/coding-style.md`. When stitching m2c output, rename its `temp_*` / `local_*` / `arg_*`
synthetics before promoting. This applies to classical / hand-authored C only; verbatim mirrors keep
their upstream names unchanged.

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
| `coddog-structural:<file>@<pct>` | #coddog-cross-ref |
| `coddog-partial:<m>of<n>fn` | #coddog-cross-ref |
| `game-region-mirror:0x<vram>` | #game-region-mirror-o2-profile |
| libultra leaf, bare std header | #per-library-standard-c-header-isolation |
| match locks ~0.9 on a lib target | #compile-profiles-libkmc--o-libultra--o3 |
| compiler rodata wrong offset / `rodata-literal:<addr>` | #rodata-sibling-yaml-pattern |
| `rodata-jtbl:<addr>` | #rodata-sibling-yaml-pattern |
| `…;owner-per-member` on a `rodata-jtbl`/`rodata-literal` | #rodata-sibling-yaml-pattern |
| MMIO fn, flat score, empty top_mismatches | #io_writeio_read-isolation-artifact |
| warm band, no hazard | #open-band-fast-path |
| `undefined reference` after mid-sprint sync-names | #make-sync-names-eviction-recovery |
| `caller-evict:<func_vram>@<file>` | #caller-evict |
| loop cannot find label / reloc-name mismatch | #stale-top-level-asm-label-sync |
| clean mirror SHA-miss, one field's high word | #mirror-cast-divergence-sign--vs-zero-extend |
| clean mirror SHA-miss, char load lb/sll-sra vs lbu/andi | #char-signedness |
| clean mirror SHA-miss, extra `jal __assert` / bare `assert()` / `bare-assert:<n>` | #assert-strip |
| clean mirror SHA-miss, same insn count reordered / jal-mismatch + no `coddog-mirror` | #near-verbatim-mirror-jal-count-mismatch |
| clean mirror SHA-miss, build instr-count < target (shorter) / collateral post-fn addr shifts | #cross-jump-tail-merge |
| `body-divergence-suspect:<file>@<pct>` | #cross-jump-tail-merge |
| array-of-struct init loop shorter than target + a field stored twice (doubled store-offset) | #struct-init-loop-dup-store--dual-induction-var |
| permuter on a KMC-toolchain (libnusys/libultra/libkmc) mirror fn | #permuter-setup-for-kmc-toolchain-mirrors |
| libnusys carry whose carried fns == the upstream's `#ifdef NU_DEBUG` fns | #nu_debug-stock-not-custom-carried-perf-fn-triage |
| libnusys inline `divu`, build byte-perfect except 2 missing `nop`s after `mflo` | #libnusys-inline-div-mflo-hazard-nop |
| mirror global w/ dead-reload-after-store on `x++` or recompute-not-CSE of `a-b` | #volatile-global-tell-dead-reload--recompute-not-cse |
| Gfx* manipulation | #display-lists |

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
- **Structs:** pull from Ghidra MCP, cross-check against `re_tracking/structs.yml`, append to
  `include/structs.h`. Discrepancies between MCP-live and the snapshot are surfaced and block the
  write.
- **Memory map:** read-only on both sides.
- **Forbidden agent edits:** `ghidra_symbols.txt` (except via `make sync-names`), `mariogolf64.ld`,
  `re_tracking/*.yml`, `reloc_addrs.txt`. Agent-editable: `mariogolf64.yaml` subseg
  flip/split/path-qualifier lines only, and `symbol_addrs.txt` add-only.
