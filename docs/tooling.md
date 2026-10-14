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
| Whole-tree candidate ranking | `uv run --locked python tools/pick_target.py -n 12` |
| Segment-range candidates | `uv run --locked python tools/pick_target.py --segment main -n 12` |
| Library/path/name substring | `uv run --locked python tools/pick_target.py --lib libultra -n 12` |
| Leaves in already-C files | `uv run --locked python tools/pick_target.py --loose-stubs main` |
| Include carried/nested/intrinsic leaves | `uv run --locked python tools/pick_target.py --loose-stubs main --all` |
| Host-file closure ranking | `uv run --locked python tools/pick_target.py --file-close main` |
| Prior reconstruction evidence | `uv run --locked python tools/pick_target.py --carried-check <fn>...` |
| Nested-function coupling | `uv run --locked python tools/pick_target.py --nested-check <fn>...` |

`-n` bounds whole-tree and loose-stub candidate rows (default 20); filtering happens before
the limit. `--all` includes hidden loose-stub classes, not unlimited output. Add `--json` to
either ranking mode for candidate records without prose. In `--file-close`, `-n` instead
sets the maximum remaining stubs per host. Inspect a bounded shortlist before expanding.

`--segment` filters by VRAM range; `--lib` is a substring filter, not an equivalent segment selector.
Whole-subsegment ranking and loose-stub ranking answer different questions. A `fresh` classification
is a ceiling, not evidence that a leaf will match easily.

`--carried-check` flags names from backlog carry-overs and WIP filenames, including scoping notes.
Read the note before treating the classification as a terminal wall. `--nested-check` identifies
parent-coupled leaves. Both commands exit nonzero when they find their respective risk.

## Iterate a function

| Need | Entry point | Read first |
| --- | --- | --- |
| Generate a seed scaffold | `uv run --locked python tools/seed_c.py --func <fn> --parent src/<seg>.c` | [Execution details](workflow/loop.md#execution-loop-inline) |
| Fetch live Ghidra context before seeding | Add `--ghidra` to `seed_c.py`; the local service must be running | [Headless Ghidra](ghidra.md#agent-access-and-context) |
| Compile and diff a seed | `uv run --locked python tools/decomp_loop.py --func <fn>` | [Oracles](workflow/loop.md#oracles) |
| Select an isolation profile | `decomp_loop.py --profile auto` uses the parent TU; explicit profiles include every key in `tools/build_profiles.json` | [Build profiles](../tools/build_profiles.json) |
| Import a permuter case | `./setup-permuter.sh <fn>` uses the isolated seed when present and the parent TU's exact flags | [Permuter playbook](hazards.md#permuter-setup-for-kmc-toolchain-mirrors) |
| Run or score an imported case | `./run-permuter.sh <fn>` / `./score-permuter.sh <fn>` | [Permuter playbook](hazards.md#permuter-setup-for-kmc-toolchain-mirrors) |
| Bank after integration | `tools/verify-rom.sh` | [Execution details](workflow/loop.md#execution-loop-inline) |

The editable candidate lives under `nonmatchings/<fn>/base.c`; assembly (`target.s`) and optional
Ghidra pseudocode remain separate reference artifacts. The seed JSON reports `body_source` and
artifact paths, not duplicated inline assembly. Do not confuse candidates with the whole-ROM WIP
build tree. Use the [hazard lookup](hazard-index.md), [residual lookup](hazard-symptoms.md), and
[source levers](levers.md) for the measured question. [WIP evidence](wip/index.md) records prior
constraints and ruled-out hypotheses, conditional on their measured base.

The authoritative object build interface is `uv run --locked python tools/rom_build.py build --object <object-path>`.
Compile profiles are defined by `tools/build_profiles.json`; use the same source-specific profile for
isolated iteration as the ROM build.

## References and native campaigns

| Need | Command |
| --- | --- |
| Similar C references for an exact label | `uv run --locked python tools/decomp_references.py --func <fn> -n 3` |
| Include another checkout without merging it | Add `--reference-root <checkout>` (repeatable) |
| Prepare a native `/decomp` queue | `uv run --locked python tools/decomp_campaign.py --segment main --pool-size 60 -n 12 --by-similarity` |
| Select explicit targets, including carries | `decomp_campaign.py --func <fn> --func <other> -n 2` |
| Partition an analysis queue | Add `--shard 1/4`; never run shared compiles/MCP/source edits concurrently |
| Cheap isolated m2c prepass | Add `--prepass --seconds 1800`; seeds/compiles run serially and matching source stays untouched |
| Bound a costly command and its descendants | `uv run --locked python tools/decomp_budget.py --seconds 600 --reserve 120 -- <command> <args>...` |

References compare original-ROM assembly shape, opcode distribution, and shared callees, then
point to C implementations without retained assembly stubs. Output includes source/target hashes,
ROM/bank/offset identity, and coverage failures. No reference is `null`, not a measured zero.
The scores are advisory, not matching percentages or difficulty estimates. Source discovery is
conservative and unverified: inspect conditional compilation and remeasure any source you reuse.
An ambiguous assembly label is rejected rather than resolved by the first file.

Explicit reference roots must declare the same ROM SHA-1 and have extracted reference assembly
under `build/us/work/asm`; the active checkout honors `MG64_BUILD_DIR`. Discovery reads fresh files
on each invocation, so newly visible C can be used before a merge. Neither a declared checksum nor
a source-present result proves that checkout passed the ROM gate. Recheck hashes before reuse.

Queue preparation is read-only. `--pool-size` bounds the existing loose-stub ranker's eligible
shortlist; similarity, sharding, and the final `-n` operate within that disclosed shortlist.
Default selection preserves the ranker's carried/nested/intrinsic/pool exclusions. Explicit
labels retain coupling tags; the prepass skips coupled/intrinsic targets, known library source
trees, already-C functions, and any existing candidate directory. Choose upstream source first.
`provisional-zero` still requires source review, integration, and the sole ROM verifier.

The [native skill](../.omp/skills/mg64-decomp/SKILL.md#campaigns) owns campaign execution. Feed it
the queue rather than starting another agent runner. Start one absolute deadline when a target
gets execution ownership, then pass it to each expensive command. For example, in the persistent
shell, set `MG64_DEADLINE_UNIX` once for a 30-minute candidate:

```sh
export MG64_DEADLINE_UNIX=$(( $(date +%s) + 1800 ))
uv run --locked python tools/decomp_budget.py --seconds 600 --reserve 120 -- \
  uv run --locked python tools/decomp_loop.py --func <fn>
```

Unset or replace the deadline before the next target. A duration alone bounds one command;
the inherited deadline prevents successive commands from extending the candidate budget.
`run-permuter.sh` uses the same guard, defaults to 600 seconds, and accepts a numeric
`MG64_PERMUTER_SECONDS` override. It reserves 120 seconds when a deadline is inherited.
Expiry exits 124 and retains artifacts without a bank verdict. Cleanup uses a procps-compatible
`ps` to include observed descendant sessions: nested guards get graceful termination followed
by bounded hard escalation and reaping. Remaining owned descendants are stopped even when
the direct command exits successfully. This is lifecycle containment, not a sandbox for hostile
daemonizing commands. The cleanup grace can extend command return beyond the execution deadline.
This is a resource budget, not an arbitrary retry cap or evidence of an impossible source match.

The prepass lock excludes another prepass, not manual builds or Ghidra: the parent must still
serialize every shared operation. There are no automatic source integrations, commits, hard
resets, or multi-worktree build workers.

## Maintain tooling and docs

- `uv run --locked pytest tests/tooling` (or `ctest --preset tooling` after configure) — tooling tests.
- `uv run --locked python tools/hazard_anchors.py check` — frozen hazard-anchor integrity.
- [Fingerprint sweeps](../tools/coddog/README.md) — optional local reference builds; maps are
  gitignored and depend on local artifacts, not a fresh-checkout requirement.

Oh My Pi discovers BasedPyright and Ruff from the locked `.venv`; no project LSP wrapper is
required. If the session predates environment installation, reload native LSP discovery.
Use references/definitions for Python tooling changes. Python type intelligence does not
model KMC GCC's C extensions or prove target code generation.

Keep MCP, builds, layout changes, and source edits serial. See
[documentation conventions](documentation.md) before reorganizing an instruction surface.
