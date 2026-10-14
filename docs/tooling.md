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
---

# Decomp tooling entrypoints

Use the configured interpreter `<binary>/venv/bin/python3` from the repository root; the default
binary tree is `build/us`. Set `MG64_BUILD_DIR` when using a different tree. The
[oracle table](workflow/loop.md#oracles) owns each tool's authority and failure modes; scores and
isolated objects are iteration signals, not banking gates.

## Discover an increment

| Need | Command |
| --- | --- |
| Whole-tree candidate ranking | `<binary>/venv/bin/python3 tools/pick_target.py -n 12` |
| Segment-range candidates | `<binary>/venv/bin/python3 tools/pick_target.py --segment main -n 12` |
| Library/path/name substring | `<binary>/venv/bin/python3 tools/pick_target.py --lib libultra -n 12` |
| Leaves in already-C files | `<binary>/venv/bin/python3 tools/pick_target.py --loose-stubs main` |
| Include carried/nested/intrinsic leaves | `<binary>/venv/bin/python3 tools/pick_target.py --loose-stubs main --all` |
| Host-file closure ranking | `<binary>/venv/bin/python3 tools/pick_target.py --file-close main` |
| Prior reconstruction evidence | `<binary>/venv/bin/python3 tools/pick_target.py --carried-check <fn>...` |
| Nested-function coupling | `<binary>/venv/bin/python3 tools/pick_target.py --nested-check <fn>...` |

`--segment` filters by VRAM range; `--lib` is a substring filter, not an equivalent segment selector.
Whole-subsegment ranking and loose-stub ranking answer different questions. A `fresh` classification
is a ceiling, not evidence that a leaf will match easily.

`--carried-check` flags names from backlog carry-overs and WIP filenames, including scoping notes.
Read the note before treating the classification as a terminal wall. `--nested-check` identifies
parent-coupled leaves. Both commands exit nonzero when they find their respective risk.

## Iterate a function

| Need | Entry point | Read first |
| --- | --- | --- |
| Generate a seed scaffold | `<binary>/venv/bin/python3 tools/seed_c.py --func <fn> --parent src/<seg>.c` | [Execution details](workflow/loop.md#execution-loop-inline) |
| Compile and diff a seed | `<binary>/venv/bin/python3 tools/decomp_loop.py --func <fn>` | [Oracles](workflow/loop.md#oracles) |
| Select an isolation profile | `decomp_loop.py --profile auto` uses the parent TU; explicit profiles include every key in `tools/build_profiles.json` | [Build profiles](../tools/build_profiles.json) |
| Import a permuter case | `./setup-permuter.sh <fn>` uses the isolated seed when present and the parent TU's exact flags | [Permuter playbook](hazards.md#permuter-setup-for-kmc-toolchain-mirrors) |
| Run or score an imported case | `./run-permuter.sh <fn>` / `./score-permuter.sh <fn>` | [Permuter playbook](hazards.md#permuter-setup-for-kmc-toolchain-mirrors) |
| Bank after integration | `tools/verify-rom.sh` | [Execution details](workflow/loop.md#execution-loop-inline) |

The candidate scaffold lives under `nonmatchings/<fn>/`; do not confuse it with the whole-ROM
WIP build tree. Use the [hazard lookup](hazard-index.md) for candidate/build risks,
[residual lookup](hazard-symptoms.md) for observed mismatches, and [source levers](levers.md) for a
named transformation. [WIP evidence](wip/index.md) records prior constraints and ruled-out levers.

The authoritative object build interface is `<binary>/venv/bin/python3 tools/rom_build.py build --object <object-path>`.
Compile profiles are defined by `tools/build_profiles.json`; use the same source-specific profile for
isolated iteration as the ROM build.

## Maintain tooling and docs

- `cmake --build build/us --target test-tools` — ROM-free tooling tests.
- `<binary>/venv/bin/python3 tools/hazard_anchors.py check` — frozen hazard-anchor integrity.
- [Fingerprint sweeps](../tools/coddog/README.md) — optional local reference builds; maps are
  gitignored and depend on local artifacts, not a fresh-checkout requirement.

Keep MCP, builds, layout changes, and source edits serial. See
[documentation conventions](documentation.md) before reorganizing an instruction surface.
