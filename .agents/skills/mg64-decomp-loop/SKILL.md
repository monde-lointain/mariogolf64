---
name: mg64-decomp-loop
description: Execution loop for Mario Golf 64 decompilation. Use when working a committed SPRINT.md backlog, matching functions, running classical decomp, using Ghidra/m2c/asm-differ/decomp-permuter, banking functions, spiking carry-overs, or continuing an open MG64 sprint.
---

# MG64 Decomp Loop

Read `docs/workflow/loop.md` before acting. Follow `## Execution loop`, `## Oracles`,
`## Conventions`, and `## Cross-repo sync`.

Route any flagged hazard through `docs/hazard-index.md` to its playbook section.

For a crack slice or any fan-out decision, read `docs/workflow/fan-out.md`, and dispatch each agent
with `docs/fanout-prompt.md` filled in.

Outcome: every item on the committed `SPRINT.md` backlog is banked or carried with a characterized
verdict, and the ROM SHA-1 is green at every commit. Abort if no committed backlog exists.

A function banks only when `tools/verify-rom.sh` exits 0; every other signal is an iteration hint.
Never commit a non-matching function. Work smallest-first with no per-function approval stop -- the
committed backlog carries standing approval. Keep MCP, build, yaml, and source edits serial; only
read-only context gathering runs in parallel.

Append the standup line and the suggestion buffer to `SPRINT.md` as each function banks. Those
suggestions apply at the review gate only, never mid-sprint.
