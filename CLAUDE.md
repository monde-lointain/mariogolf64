# Mario Golf 64 Decomp workflow

This project decompiles Mario Golf 64 (N64) one function at a time. Tooling: splat (file
splitting), KMC GCC 2.7.2 (`tools/cc/gcc`), m2c (asm→C seed), asm-differ (library mode),
decomp-permuter (escalation), Ghidra MCP bridge (live decompile + struct/symbol lookup, port 8089,
all calls pass `program="baserom.z64"`), coddog (`make coddog-sweep` — fingerprints MG64 fns vs
ultralib VERSION_J to reveal an un-named `func_`'s upstream source; see `docs/hazards.md#coddog-cross-ref`).

This file is the always-loaded core: the workflow model, the Scrum cadence, and the every-sprint
rules. Detail lives in two on-demand references:

- `docs/hazards.md` — the hazard playbooks (what to do when `pick_target.py` flags a hazard). The
  hazard index below maps each flag to its section.
- `docs/coding-style.md` — the C quality bar + naming conventions for code promoted into the tree.

## Workflow at a glance

Two slash commands are the Scrum gates. Between them, the agent runs the execution loop inline —
one function at a time, no per-function stop, a single serial session (no MCP lock). `SPRINT.md`
is the resume surface if the middle spans context windows.

## Slash commands

- **`/sprint-plan [scope]`** — Plan gate. Ranks candidates with `tools/pick_target.py`
  (smallest-first), proposes one small increment + its enablers, gets PO approval, then performs and
  validates the flip (subseg yaml edit + `symbol_addrs.txt`, optional `make sync-names`, then
  `make extract && make` for the green ROM), and writes `SPRINT.md`. Then runs the execution loop
  over the committed backlog.
- **`/sprint-review`** — Review gate. Verifies the ROM-SHA-1 DoD on the increment, takes PO scope
  sign-off, and is the single apply-point for the sprint's buffered workflow-improvement edits;
  appends the `RETRO.md` digest + `BACKLOG.md` carry-overs.

## Execution loop (inline)

After `/sprint-plan` writes `SPRINT.md`, work the committed backlog autonomously, smallest-first.
Ghidra MCP is used inline at seed time. For each target function:

1. **Resolve** the function's subseg in `mariogolf64.yaml`. `hasm` → permanent asm, skip. The
   subseg was already flipped to `c` at the gate.

2. **Mirror branch** (the `upstream` column is a lib, not `none`): copy the upstream `.c` verbatim
   to its mirrored `src/lib<name>/...` path; add missing headers verbatim under
   `include/lib<name>/...`; skip clang-format; `make` → ROM SHA-1 must equal the baserom. The
   proof is the full-`make` SHA-1 (verbatim copy + green ROM = match); no byte-`cmp`. See
   `docs/hazards.md#upstream-mirror-pattern`. Before declaring a clean mirror, reconcile the
   upstream's call + data-ref list (one level of macro expansion included) against the name files,
   and handle any flagged hazard via the index below.
   - **libultra source is `~/development/repos/ultralib` (gcc.mk / KMC-N64 profile,
     `-DBUILD_VERSION=VERSION_J`) — NOT `libultra_modern`** (deprecated 2.0L, casts diverge; it is
     an `additional working dir` but is not the source). This pin holds for both the C mirror and the
     hand-asm `intrinsic-likely:<tu>.s` asm-mirror (`docs/hazards.md#asm-mirror-vendoring`): the
     project mirrors ultralib's `gcc.mk` profile (`LIBULTRA_CFLAGS` for C, `LIBULTRA_ASFLAGS` for
     vendored asm TUs). libkmc → `~/development/repos/libkmc`, libnusys → the n64sdkmod nusys tree.

3. **Classical branch** (no upstream, or a hazard routes here): seed → iterate → spot-check →
   finalize.
   - **Seed** `nonmatchings/<func>/base.c`: fetch the Ghidra decompile via MCP, then
     `venv/bin/python3 tools/seed_c.py --func <placeholder> --parent src/<seg>.c`. The asm
     (`disassemble_function`) is ground truth, not the Ghidra decompile (see
     `docs/hazards.md#decompile-vs-asm-authority`). Use the decompile for shape/types; translate
     the logic from the instruction listing.
   - **Iterate** ≤25 times: `venv/bin/python3 tools/decomp_loop.py --func <placeholder>` → parse
     JSON. `score == 0` is a candidate; 5 consecutive `compile_ok == False` means a broken seed
     (stop); else read the top mismatches, edit `base.c`, re-run. Permuter (`./run-permuter.sh`)
     only at `percent ≥ 0.97`. For a near-miss whose only diff is which scratch register holds an
     intermediate, see `docs/hazards.md#register-reuse-nudge-classical-regalloc`.
   - **Spot-check** (only at score 0): byte-level `cmp` of the in-tree compiled `.text` vs the
     isolated one. The `cmp` is the truth, not the mnemonic diff (see
     `docs/hazards.md#assembler-differences--byte-cmp-spot-check`). A non-zero score with **empty
     `top_mismatches` AND `match_count == total_rows`** is an isolation artifact (struct-field reloc
     addend / extern HI/LO16), not a near-miss — go straight to the in-tree spot-check / full-make
     SHA-1, do not iterate C or reach for the permuter (see `docs/hazards.md#isolated-compile-caveat`).
   - **Finalize** (only if the spot-check passes): inline the body into `src/<seg>.c`, drop the
     `INCLUDE_ASM` line, `clang-format -i` (skip under `src/libultra/` & `src/libkmc/`), then `make`
     until `build/mariogolf64.z64: OK` and SHA-1 == baserom.

4. **Bank** the function (only at score 0 + spot-check + full-`make` SHA match). Give it its curated
   Ghidra name (add to `symbol_addrs.txt`, rename in the body, re-`make`). `git commit` — **stage the
   `make extract`-regenerated artifacts too** (`undefined_syms_auto.txt` + `mariogolf64.ld`); they
   change on a subseg flip / `symbol_addrs.txt` add and must travel with the commit, or the regen
   bleeds into the next sprint's dirty tree (S85→S86: a `D_→named` BOOT_GLOBALS regen left
   uncommitted surfaced as a stray diff at the next gate). `git status` should be clean after commit.
   Append a standup line to `SPRINT.md`, and record the run's numbered "Suggested workflow
   improvements" into `SPRINT.md`'s buffer — applied only at `/sprint-review`, never mid-sprint.

A function that locks `< 0.97 percent`, needs the permuter, or hits a BSS-layout/alignment wall is a
spike: note it, carry its file to `BACKLOG.md ## Carry-overs`, and move on. Don't weaken the DoD to
bank a spike. The increment (the `src/<seg>.c` file) banks only when its last `INCLUDE_ASM` stub is
gone.

## Scrum operating model

The tactical match-loop runs inside a thin Scrum cadence (McConnell, *More Effective Agile*).
Artifacts: `BACKLOG.md` (PO-owned ordering rationale + enablers + carry-overs), `SPRINT.md`
(gitignored ephemeral board + resume surface), `RETRO.md` (consolidated digest), `VELOCITY.md`
(story-point dashboard). Target selection is `tools/pick_target.py`, not a stored roadmap.

- **Roles.** Product Owner = the user (backlog priority, goal approval, scope sign-off, retro
  selection). Development Team = the agent (executes the gate enablers after PO approval) + `tools/`
  + the KMC-GCC/asm-differ/ROM-SHA-1 oracle.
- **Cycle.** `/sprint-plan` (PO approves a goal + small backlog; agent performs + validates the flip
  enablers) → autonomous execution → `/sprint-review` (DoD verify, scope sign-off, retro) → repeat.
- **Sprint = one `src/<seg>.c` file to md5-candidate (0 `INCLUDE_ASM` stubs), or one cohesive subseg
  cluster.** The function is the task; the file/cluster is the increment. Cap small early — 1
  upstream file, or ≤ ~3–4 functions — so the middle fits a context window. For a homogeneous
  sibling set (same upstream pattern, same uniform enabler), fill the ≤3–4 cap; per-file
  all-or-nothing banking makes the marginal sibling near-free. A heterogeneous batch (mixed hazards)
  trims to the cleanest 2. Review fires when every committed item is banked or spiked/carried.
- **Definition of Ready.** Subseg flippable (not `hasm`); coarse size known; upstream-mirror
  availability noted; hazards flagged. Enablers (subseg flip + `make extract`, multi-file split,
  `symbol_addrs.txt` additions) are performed by the agent at the plan gate after the PO approves
  the goal/scope, and validated there: `make extract && make` must still produce the green baserom
  ROM with the new stubs. This gate build-check is load-bearing — it surfaced a missing-`cpp`
  toolchain regression that was silently emptying every asm object. A name sync (`make sync-names`)
  is gate-only, never mid-sprint (a rename breaks in-flight links until `make extract && make`).
- **Definition of Done (binary; the oracle already exists).** Per-function = score 0 + byte-`cmp`
  spot-check + inlined + clang-format + full `make` → `build/mariogolf64.z64: OK`, SHA-1 == baserom +
  committed. Per-sprint = every fn in the file inlined (0 stubs) + ROM SHA-1 matches + committed.
  The ROM SHA-1 is green at every commit (un-decompiled parts fill from extracted asm), so at review
  the `make`+SHA-1 paste is a regression guard; the value delta is the matched-count /
  md5-candidate-files number. Never commit a non-matching fn.
- **Spike + carry-over.** A function that blocks its file's DoD (stuck-far < 0.97, needs permuter,
  BSS-layout-conflict, subseg-alignment) is a spike: note it, carry its file/cluster to the next
  sprint, count credit at the function level. Don't weaken DoD to bank a spike.
- **Quality counter-metric.** Track stuck-far + permuter-escalated + carried + re-opened per sprint,
  reported next to the match count so the count can't be gamed by premature spiking.
- **Process changes are retro-gated.** The "Suggested workflow improvements" the execution loop
  emits are recorded into `SPRINT.md` during the sprint and applied only at `/sprint-review` — never
  mid-sprint — so the sprint's matching behavior stays fixed and its measurement stays clean. The
  retro is the single apply-point for edits to `CLAUDE.md` / `docs/*` / `tools/*.py` / the two
  command files.
- **Two-gate close.** Review accepts the product (DoD + scope); retro improves the process (the
  single apply-point for tooling edits).
- **Resume protocol.** On a fresh mid-sprint session: read `SPRINT.md`, reconcile banked work via
  `git log` since the snapshot, and verify Ghidra MCP connectivity (`list_instances`, port 8089)
  before resuming.

## Story points

Lightweight estimation over the Scrum cadence. Scale: Fibonacci 1,2,3,5,8,13. Current phase
`regime: mirror`. `VELOCITY.md` is the committed dashboard with the full rules + anchors; this is the
summary.

- **Deterministic seed (v1).** `pick_target.py`'s `pts` column is the a-priori seed (a pure function
  of `size`, `upstream`, `band`, `nfns`, `hazards`). A cluster seed is the sum of its files' seeds.
  `pts` is display-only — it does not change the smallest-first sort. A `blk` seed = an un-pickable
  needs-header (a DoR reject).
- **The 8-point decompose gate (v1).** A seed of 8 or 13 must NOT run as a normal 1-increment sprint
  — decompose it (split the subseg at the upstream-file/function boundary) or pull a scaffolding
  enabler as the goal instead. Applied at the `/sprint-plan` gate. **Verbatim-mirror exemption
  (S64; generalized S69).** A seed-8/13 increment may run as a normal 1-increment sprint when ALL
  hold: (a) `regime: mirror` + a verbatim copy of a single upstream file — **a drop-def mirror
  qualifies** (the function bodies are byte-verbatim; only the file-scope data DEFS become `extern`
  decls, which emit nothing, so it banks atomically exactly like a pure `cp`; S86 `os/timerintr.c`,
  pts-8 single-file-pack, banked first-try seed-only), (b) the decompose path is
  mechanically blocked — the increment is a `single-file-pack` (every member fn comes from ONE
  upstream `.c`), so there is no inter-file boundary to split at and an intra-file split can't be
  independently mirrored. **This holds regardless of inner-boundary 16-alignment** — S64
  `lookathil`'s inner boundary was `non16align`, S69 `lookat`'s was 16-aligned, both decompose-blocked
  (you cannot mirror half a source file). `pick_target.py`'s `single-file-pack:<n>fn[…]` tag (S67) IS
  the signal; the old `non16align`-on-the-inner-boundary test was one mechanical case of it, not the
  rule. (c) every callee is placed + both/all names curated (the "no residual variance" condition).
  The gate's all-or-nothing concern guards CLASSICAL iteration stalls; a verbatim single-file mirror
  banks atomically (or is a quick spike), so a size-only 8/13 is a false fire — the same false-flag
  class the hazard detectors keep retiring. Document the exemption in `SPRINT.md ## Estimate`; the
  increment stays seed-only (S64 `gu/lookathil.c` + S69 `gu/lookat.c`, both pts-13, banked first-try).
  The exemption never covers classical or multi-file packs (a `pack`/`c-combined` of ≥2 distinct
  upstream files decomposes at the file boundary as usual).
- **Per-file all-or-nothing banking.** Points bank per file: a spiked/carried file scores 0 pt, a
  banked sibling still counts. This is a separate ledger from the function-level quality
  counter-metric.
- **Realized tier + residual (v2 — active since Sprint 11).** Scored at review on the **classical
  track only**: start at the seed band, +1 per {stuck-far / permuter / re-attempt / novel
  bank-gotcha / mid-sprint split / carry-or-reopen}, −1 for a verbatim first-try ≤1 fix-iteration;
  per-file then summed. Residual = realized − seed. A classical/mixed increment uses the two-pass
  freeze (plan-time seed committed before `src/`, realized in a second commit). The **mirror track
  stays seed-only** (a point mass with no residual variance), so a `regime: mirror` sprint logs
  banked = seed. The realized tier is agent-scored and subjective — the real anti-gaming guards stay
  the per-file all-or-nothing bank + the quality counter-metric, not the freeze. Full rules +
  rolling-5 + re-anchor in `VELOCITY.md`.

Velocity is a planning indicator, not a performance target (McConnell Ch. 19) — reported next to the
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
  `make test-tools` runs the `tests/tooling/` characterization suite (pytest); refactor tooling
  under it, and `REGEN_GOLDEN=1` to refresh golden snapshots after an intended behavior change.
- **File-level matching.** A source file matches only when every function in it matches. Promoting a
  match replaces one `INCLUDE_ASM` stub with the C body. No md5 win until the last stub in a file is
  gone.
- **`symbol_addrs.txt` is add-only; never delete an entry.** The agent may add maintainer-override
  entries: a function is `name = 0x<vram>; // type:func`, a data extern is `name = 0x<vram>;
  // size:0x<n>` (no `type:func`). `reloc_addrs.txt` is never touched. `ghidra_symbols.txt` is owned
  by `sync_decomp_names.py` (modify only via `make sync-names`, never hand-edit). The two name files
  must stay disjoint — never add a `symbol_addrs.txt` entry for an address already in
  `ghidra_symbols.txt` (splat errors on duplicates).
- **yaml authority.** The agent edits `mariogolf64.yaml` subseg flip/split/path-qualifier lines only
  (never the segment/memory-map structure); `mariogolf64.ld` and segment boundaries are off-limits.
  Subseg flips, multi-file splits, and the `make extract` that regenerates the scaffold are gate
  actions (or inline when a split is needed mid-flight).
- **`hasm` subsegments stay raw asm forever** (entry stub, `__muldi3` libkmc math module, RSP
  microcode entries). The execution loop refuses these.
- **Permuter** (`./run-permuter.sh`) runs only when asm-differ's `percent` ≥ 0.97. Below that,
  iterate on C or reconsider whether the subseg should be `hasm`.
- **Decomp is authoritative for names** (per the Ghidra-workspace
  `docs/re/coordination/decomp_coordination.md`).
- **Match finalization is three steps:** inline body into `src/<seg>.c` → `clang-format -i`
  (skip under `src/libultra/` & `src/libkmc/`) → full `make` until ROM SHA-1 matches. Spot-check
  passing ≠ ROM matching; the final `make` proves the match. Gate the `sha1sum` on `make`
  succeeding (confirm the `build/mariogolf64.z64: OK` line first) — a failed link leaves the previous
  `.z64` in `build/`, so `sha1sum` on a stale ROM false-positives.
- **Clean-rebuild when an enabler edits a shared vendored header.** The build tracks no header deps,
  so an incremental `make` recompiles only the file you touched, not the other consumers of a header
  you changed. When a mirror/enabler edits a widely-included header (e.g.
  `include/libultra/PR/os_version.h`), the **banking** SHA-1 must come from `make clean && make
  extract && make`, not an incremental build (`docs/hazards.md#clean-rebuild-after-shared-header-edit`).
- **Never clang-format library code under `src/libultra/` or `src/libkmc/`.** These are verbatim
  upstream copies; reformatting defeats cross-referencing. Both dirs carry a local `.clang-format`
  with `DisableFormat: true`.
- **Use ultra64.h types** (`s32`/`u64`/`vu32`/`f32`/…) in decomp C, never raw
  `int`/`long long`/`volatile unsigned long`.
- **Prompt authoring.** Any slash command in this project follows `PROMPT_GUIDELINES.md` at the
  project root when created or modified.

### C naming (compact — full guide in `docs/coding-style.md`)

| Identifier            | Convention   | Example                |
| --------------------- | ------------ | ---------------------- |
| Function / variable / parameter / member | `lower_case` | `frame_count` |
| Struct / Enum (type)  | `CamelCase`  | `PacketHeader`         |
| Enum constant / macro / global+static const | `UPPER_CASE` | `MAX_CONNECTIONS` |

When stitching m2c output, rename its `temp_*` / `local_*` / `arg_*` synthetics before promoting.

### Hazard index

When `pick_target.py` flags a hazard (or a match shows its symptom), read the matching
`docs/hazards.md` section:

| pick_target.py hazard / symptom | read in `docs/hazards.md` |
| ------------------------------- | ------------------------- |
| (upstream column is a lib)      | #upstream-mirror-pattern |
| `refs-unplaced:<g>@0x…`         | #recover-extern-refs-unplaced |
| `calls-unplaced:<fn>@0x…`       | #calls-unplaced-function-callee-dual |
| `jal-count-mismatch:<C>vs<asm>` | #near-verbatim-mirror-jal-count-mismatch |
| `file-static`                   | #file-static-bss-layout-conflict |
| `defines-data:<g>` / `data-static:<addr>` | #defines-data |
| `twin-of:<file>` (mirror dir holds a banked sibling with the same ld-section carve) | #defines-data / #rodata-sibling-yaml-pattern |
| `needs-header:<inc>`            | #needs-header |
| `stale-header:os_version.h(<V>)`| #stale-vendored-header |
| `needs-define:<def>`            | #needs-define |
| (clean mirror SHA-miss, exact N×8B / fn shorter than its asm size; a `>= VERSION_K`-gated stmt MG64-J actually has) | #needs-define |
| `header-renames-symbol:<fn>@<hdr>` (S85: a transitively-vendored header rewrites the curated symbol via a macro, e.g. os_host.h `#define __osInitialize_common() osInitialize()` → needs `#undef <fn>`; S31 nuGfxInit class) | #header-renames-symbol |
| `pack:<n>fn[…]`                 | #multi-function-segment-splitting-pack |
| `single-file-pack:<n>fn[…]` (all members → one upstream C file; atomic verbatim mirror, NO split) | #upstream-mirror-pattern |
| `c-combined:<n>file[…]` (≥2 distinct C upstream files in one asm subseg) | #multi-function-segment-splitting-pack |
| `non16align`                    | #non16align |
| `trailing-pad:<n>B@<align>` (a verbatim C mirror compiles short of its subseg slot — the slot has nop pad to a >16-aligned next subseg the compiler's 16-align can't fill; split a nop-pad `[..,asm]` subseg) | #trailing-alignment-pad-after-a-c-mirror |
| `intrinsic-likely:<tu>.s` (vendorable ultralib asm TU) | #asm-mirror-vendoring |
| `intrinsic-likely:<tu>.s(has-rodata:<sym>)` (S84: the vendorable .s carries a `.rodata`/`.data` section → vendor `.text` only + strip the data block, keep it as the renamed generic blob) | #asm-mirror-vendoring |
| `intrinsic-likely:cp0-asm(identify-TU)` (un-named privileged hand-asm: TLB/CP0/eret, name unresolved) | #asm-mirror-vendoring |
| `combined-subseg:<n>tu[…]` (≥2 distinct asm TUs in one subseg) | #asm-mirror-vendoring |
| `intrinsic-likely` (bare) / `maybe-upstream:…` | #intrinsic-likely--maybe-upstream-signature-hints |
| `coddog-mirror:<file>@<pct>` (coddog matched an un-named/`none` candidate to an ultralib fn → verbatim mirror, not classical; S72: a same-row `file-static`/`defines-data`/`needs-header` is the coddog trap re-scan → not an atomic cp) | #coddog-cross-ref |
| `coddog-twin:<matched>!=<member-src>` (S81: coddog matched a near-identical TWIN file; the named members name the real source — mirror from `<member-src>`, not `<matched>`) | #coddog-cross-ref |
| (libultra leaf, bare std header)| #per-library-standard-c-header-isolation |
| (match locks ~0.9, lib target)  | #compile-profiles-libkmc--o-libultra--o3 |
| (compiler rodata, wrong offset) / `rodata-literal:<addr>` | #rodata-sibling-yaml-pattern |
| `rodata-jtbl:<addr>` (a `switch` jump table the mirror re-emits; the gate's text-only flip can't catch the carve — the jtbl stays valid asm until the body lands — so it's priced at the gate) | #rodata-sibling-yaml-pattern |
| (MMIO fn, flat score, empty top_mismatches) | #io_writeio_read-isolation-artifact |
| (macro-hidden extern)           | #macro-hidden-recover-extern |
| (warm band, no hazard)          | #open-band-fast-path |
| (`undefined reference` after mid-sprint sync-names) | #make-sync-names-eviction-recovery |
| `caller-evict:<func_vram>@<file>` (a banked C file calls an un-named member by name; naming it at the gate evicts the caller) | #caller-evict |
| (loop can't find label / reloc-name mismatch) | #stale-top-level-asm-label-sync |
| (clean mirror SHA-miss, one field's high word) | #mirror-cast-divergence-sign--vs-zero-extend |
| (clean mirror SHA-miss, char load lb/sll-sra vs lbu/andi) | #char-signedness |
| (clean mirror SHA-miss, extra `jal __assert` / bare upstream `assert()`) | #assert-strip |
| (Gfx* manipulation)             | #display-lists |

## Cross-repo sync (Ghidra workspace at `~/development/reversing/ghidra/mariogolf64/`)

- **Names (Ghidra → decomp):** `make sync-names` pulls curated function + data names into
  `ghidra_symbols.txt` (gate-only). The sync skips any address already in `symbol_addrs.txt` (the
  two files stay disjoint). See `docs/hazards.md#make-sync-names-eviction-recovery` for the
  mid-sprint-break recovery.
- **Names (decomp → Ghidra):** the agent writes the curated name into `symbol_addrs.txt` directly
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
