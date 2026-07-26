---
description: Execution loop. Work the committed SPRINT.md backlog autonomously, smallest-first, banking one matched function at a time. Also the mid-sprint resume entry point.
allowed-tools: Bash, Read, Write, Edit, AskUserQuestion, Task
---

# /decomp-loop, the execution loop

Read `docs/workflow/loop.md` before acting. Follow `## Execution loop`, `## Oracles`, and
`## Conventions`.

For a crack slice or any fan-out decision, also read `docs/workflow/fan-out.md`.

Outcome: every item on the committed backlog is banked or carried with a characterized verdict, and
the ROM SHA-1 is green at every commit.

## Command rules

- Work the `SPRINT.md` committed backlog smallest-first. It carries standing Product Owner approval,
  so do not stop for per-function sign-off.
- A function banks only when `tools/verify-rom.sh` exits 0. Every other signal is an iteration hint.
- Never commit a non-matching function. A function that walls is carried, not banked.
- Keep Ghidra MCP, build, yaml, and source edits serial. Read-only context gathering may run in
  parallel.
- Append a standup line to `SPRINT.md` as each function banks, and record suggested workflow
  improvements into its buffer. Those apply at `/sprint-review` only, never mid-sprint.

## Resuming mid-sprint

A fresh session starts here rather than reconstructing state from memory:

1. Read `SPRINT.md` for the goal, committed backlog, and standup log.
2. Reconcile what actually banked with `git log` since the snapshot -- the log is the truth, the
   standup line is a claim.
3. Check Ghidra MCP connectivity (`list_instances`, port 8089) before any seed that needs it.

The context window compacts automatically, so remaining context is not a reason to stop early.
