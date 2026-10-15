---
type: Reference
title: Decomp tooling entrypoints
description: Selects supported commands for candidate discovery, matching iteration, and documentation maintenance.
sources:
  - resource: ../tools/pick_target.py
    title: Candidate-selection CLI
  - resource: ../tools/decomp_loop.py
    title: Per-function compile and diff CLI
  - resource: ../tools/rom_build.py
    title: ROM build interface
  - resource: ../tools/decomp_references.py
    title: Advisory C-reference discovery
  - resource: ../tools/decomp_campaign.py
    title: Native campaign queues and serial m2c prepasses
  - resource: ../tools/decomp_budget.py
    title: Deadline and process-group enforcement
---

# Decomp tooling entrypoints

Run tools with `uv run --locked python` from the repository root. uv synchronizes the shared
`.venv`; no activation or separate pip installation is needed. The default binary tree is
`build/us`. Set `MG64_BUILD_DIR` when using a different tree; Python remains in the root `.venv`. The
[oracle table](workflow/loop.md#oracles) owns each tool's authority and failure modes; scores and
isolated objects are iteration signals, not banking gates.

## Discover an increment

| Need | Command |
| --- | --- |
| Main-bank candidate ranking (current milestone) | `uv run --locked python tools/pick_target.py --segment main -n 12` |
| Main-bank candidates including carries | `uv run --locked python tools/pick_target.py --segment main --include-stuck -n 12` |
| Main-bank library/path/name substring | `uv run --locked python tools/pick_target.py --segment main --lib libultra -n 12` |
| Leaves in all main-bank C hosts, including libraries | `uv run --locked python tools/pick_target.py --loose-stubs main` |
| Include carried/nested/intrinsic leaves | `uv run --locked python tools/pick_target.py --loose-stubs main --all` |
| YAML-bank host-file closure ranking | `uv run --locked python tools/pick_target.py --file-close main` |
| Prior reconstruction evidence | `uv run --locked python tools/pick_target.py --carried-check <fn>...` |
| Nested-function coupling | `uv run --locked python tools/pick_target.py --nested-check <fn>...` |

`-n` bounds whole-tree and loose-stub candidate rows (default 20); filtering happens before
the limit. `--all` includes hidden loose-stub classes, not unlimited output. Add `--json` to
either ranking mode for candidate records without prose. In `--file-close`, `-n` instead
sets the maximum remaining stubs per host. Inspect a bounded shortlist before expanding.
Discovery can exit 1 when no fresh candidates exist, even when `--all` prints carries;
run independent discovery modes separately rather than chaining them with `&&`.

`--segment` filters by VRAM range; `--lib` is a substring filter, not an equivalent segment selector.
Whole-subsegment ranking and loose-stub ranking answer different questions. A `fresh` classification
is a ceiling, not evidence that a leaf will match easily.

Agent selection follows the [main-completion gate](../.omp/skills/mg64-decomp/SKILL.md#main-before-overlays).
`--loose-stubs main` and `--file-close main` enumerate the YAML bank's C hosts, including libraries
and nested source directories, not arbitrary files under `src/main/`. They omit assembly packs;
inspect those with whole-subsegment ranking. An empty shortlist does not authorize overlay work.

`--carried-check` flags names from backlog carry-overs and WIP filenames, including scoping notes.
Read the note before treating the classification as a terminal wall. `--nested-check` identifies
parent-coupled leaves. Both commands exit nonzero when they find their respective risk.
A carried pack leader does not classify its other members; inspect member assembly and run the
individual checks before reopening a carry. Carry age and file closure are not new source leverage.

## Iterate a function

| Need | Entry point | Read first |
| --- | --- | --- |
| Recover an isolated seed body | `uv run --locked python tools/seed_c.py --func <fn> --parent src/<seg>.c` | [Source-recovery gate](workflow/loop.md#source-recovery) |
| Fetch live Ghidra context before seeding | Add `--ghidra` to `seed_c.py`; the local service must be running | [Headless Ghidra](ghidra.md#agent-access-and-context) |
| Compile/diff a seed or refresh a carry | `uv run --locked python tools/decomp_loop.py --func <fn>` | [Measurement and carry recovery](workflow/loop.md#measurement) |
| Select an isolation profile | `decomp_loop.py --profile auto` uses the parent TU; explicit profiles include every key in `tools/build_profiles.json` | [Build profiles](../tools/build_profiles.json) |
| Import a permuter case | `./setup-permuter.sh <fn>` uses the isolated seed when present and the parent TU's exact flags | [Permuter playbook](hazards.md#permuter-setup-for-kmc-toolchain-mirrors) |
| Run or score an imported case | `./run-permuter.sh <fn>` / `./score-permuter.sh <fn>` | [Permuter playbook](hazards.md#permuter-setup-for-kmc-toolchain-mirrors) |
| Bank after integration | `tools/verify-rom.sh` | [Execution details](workflow/loop.md#execution-loop-inline) |

The editable candidate lives under `nonmatchings/<fn>/base.c`; assembly (`target.s`) and optional
Ghidra pseudocode remain separate reference artifacts. Successful seed JSON reports `body_source`
and artifact paths; failed body recovery leaves diagnostics, not an empty candidate.
Do not confuse candidates with the whole-ROM WIP
build tree. Use the [hazard lookup](hazard-index.md), [residual lookup](hazard-symptoms.md), and
[source levers](levers.md) for the measured question. [WIP evidence](wip/index.md) records prior
constraints and ruled-out hypotheses, conditional on their measured base.
For a small clear asm-first leaf, write the isolated `base.c` directly and reuse existing types;
seeding and Ghidra are optional context recovery, not prerequisites. Search eligibility is owned
by the [loop reference](workflow/loop.md#permuter-eligibility).

The authoritative object build interface is `uv run --locked python tools/rom_build.py build --object <object-path>`.
Use the parent TU's profile from `tools/build_profiles.json` for isolated compilation too.
For pass diagnostics, use the existing `rom_build.py compile --profile-source` interface
described in [compiler-source escalation](workflow/loop.md#compiler-source-escalation), not
a hand-assembled compiler command.

## References and native campaigns

For reference lookup, queue preparation, campaign scheduling, or deadlines, use the
[campaign and reference guide](campaigns.md). The overview here preserves this discovery
anchor for existing links; it does not own campaign execution policy.

| Need | Command |
| --- | --- |
| Advisory C references for a target | `uv run --locked python tools/decomp_references.py --func <fn> -n 3` |
| Include another checkout | Add repeatable `--reference-root <checkout>` |
| Prepare a native `/decomp` queue | `uv run --locked python tools/decomp_campaign.py --segment main --pool-size 60 -n 12 --by-similarity` |
| Select exact labels | `decomp_campaign.py --func <fn> --func <other> -n 2` |
| Partition an analysis queue | Add `--shard 1/4`; shared operations remain serialized |
| Run an isolated m2c prepass | Add `--prepass --seconds 1800` |
| Bound a command and descendants | `uv run --locked python tools/decomp_budget.py --seconds 600 --reserve 120 -- <command> <args>...` |

Reference scores and queue records are advisory; neither source presence nor a provisional
zero banks code. The native [decomp skill](../.omp/skills/mg64-decomp/SKILL.md#campaigns)
owns execution, and only the parent owns shared builds, MCP, and source integration. See
[campaign guide](campaigns.md) for selection defaults, provenance, prepass exclusions,
deadline semantics, and read-only examples.

## Maintain tooling and docs

- `uv run --locked pytest tests/tooling` (or `ctest --preset tooling` after configure) — tooling tests.
- `uv run --locked python tools/hazard_anchors.py check` — frozen hazard-anchor integrity.
- [Fingerprint sweeps](../tools/coddog/README.md) — optional local reference builds; maps are
  gitignored and depend on local artifacts, not a fresh-checkout requirement.

Oh My Pi discovers BasedPyright and Ruff from the locked `.venv`; no project LSP wrapper
is required. If the session predates environment installation, reload native LSP discovery.
Use references/definitions for Python tooling changes. Python type intelligence does not
model KMC GCC's C extensions or prove target code generation.

Keep MCP, builds, layout changes, and source edits serial. See
[documentation conventions](documentation.md) before reorganizing an instruction surface.

