---
type: Reference
title: Decomp tooling entrypoints
description: Selects the existing command for candidate discovery, matching iteration, and documentation maintenance.
sources:
  - resource: ../Makefile
    title: Maintained targets
  - resource: ../tools/pick_target.py
    title: Candidate-selection CLI
  - resource: ../tools/decomp_loop.py
    title: Per-function compile and diff CLI
---

# Decomp tooling entrypoints

Run commands from the repository root under `venv/bin/python3`. Install the toolchain and submodules
with the [build guide](build.md) first. The [oracle table](workflow/loop.md#oracles) owns each tool's
authority and failure modes; scores and isolated objects are iteration signals, not banking gates.

## Discover an increment

| Need | Command |
| --- | --- |
| Whole-tree candidate ranking | `venv/bin/python3 tools/pick_target.py -n 12` |
| Segment-range candidates | `venv/bin/python3 tools/pick_target.py --segment main -n 12` |
| Library/path/name substring | `venv/bin/python3 tools/pick_target.py --lib libultra -n 12` |
| Leaves in already-C files | `venv/bin/python3 tools/pick_target.py --loose-stubs main` |
| Include carried/nested/intrinsic leaves | `venv/bin/python3 tools/pick_target.py --loose-stubs main --all` |
| Host-file closure ranking | `venv/bin/python3 tools/pick_target.py --file-close main` |
| Prior reconstruction evidence | `venv/bin/python3 tools/pick_target.py --carried-check <fn>...` |
| Nested-function coupling | `venv/bin/python3 tools/pick_target.py --nested-check <fn>...` |

`--segment` filters by VRAM range; `--lib` is a substring filter, not an equivalent segment selector.
Passing a recognized segment name to `--lib` is rejected. Whole-subsegment ranking and loose-stub
ranking answer different questions; an empty segment ranking does not prove all of its C-file leaves
are finished. A `fresh` classification is a ceiling, not evidence that a leaf will match easily.

`--carried-check` flags names from backlog carry-overs and WIP filenames, including scoping notes.
Read the note before treating the classification as a terminal wall. `--nested-check` identifies
parent-coupled leaves. Both commands exit nonzero when they find their respective risk.
Follow the [plan gate](workflow/gates.md#sprint-plan-procedure) before editing a decomp increment.

## Iterate a committed function

| Need | Entry point | Read first |
| --- | --- | --- |
| Generate a seed scaffold | `venv/bin/python3 tools/seed_c.py --func <fn> --parent src/<seg>.c` | [Execution loop](workflow/loop.md#execution-loop-inline) |
| Compile and diff a seed | `venv/bin/python3 tools/decomp_loop.py --func <fn>` | [Oracles](workflow/loop.md#oracles) |
| Select an isolation profile | `decomp_loop.py --profile auto\|libkmc\|libultra\|main\|default` | [Build profiles](hazards.md#compile-profiles-libkmc--o-libultra--o3) |
| Import a game-code permuter case | `./setup-permuter.sh --main <fn>` | [Permuter playbook](hazards.md#permuter-setup-for-kmc-toolchain-mirrors) |
| Run or score an imported case | `./run-permuter.sh <fn>` / `./score-permuter.sh <fn>` | [Permuter playbook](hazards.md#permuter-setup-for-kmc-toolchain-mirrors) |
| Bank after integration | `tools/verify-rom.sh` | [Execution loop](workflow/loop.md#execution-loop-inline) |

The scaffold lives under `nonmatchings/<fn>/`; do not confuse it with the whole-ROM
`build/nonmatching/` tree. Use the [hazard lookup](hazard-index.md) for candidate/build risks,
[residual lookup](hazard-symptoms.md) for observed mismatches, and [source levers](levers.md) for a
named transformation. [WIP evidence](wip/index.md) records previous constraints and ruled-out levers.

## Maintain tooling and docs

- `make test-tools` — existing characterization and prompt-contract suite.
- `make prompt-lint` — hard prompt checks followed by the advisory report.
- `venv/bin/python3 tools/hazard_anchors.py check` — frozen hazard-anchor integrity.
- `make setup-dev`, then `make check-all` — optional local Python analysis gate.
- [Fingerprint sweeps](../tools/coddog/README.md) — optional local reference builds; maps are
  gitignored and depend on local artifacts. They are not a fresh-checkout requirement.

Keep MCP, builds, layout changes, and source edits serial. Process/tooling suggestions made during a
sprint are buffered for review unless the Product Owner explicitly requests out-of-band rework.
See [documentation conventions](documentation.md) before reorganizing an instruction surface.
