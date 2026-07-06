---
description: Plan gate. Rank with pick_target.py, propose a sprint goal + small committed backlog, get PO approval, then perform + validate the flip, write SPRINT.md.
args:
  - name: scope
    description: Optional subsystem filter (e.g. `audio`, `libkmc`, `rsp`). Matches `^[a-z][a-z0-9_-]*$`. No arg means the whole active phase.
    required: false
allowed-tools: Bash, Read, Write, Edit, AskUserQuestion
---

# /sprint-plan, the plan gate

SCOPE = `$ARGUMENTS`

Read `docs/agent-workflow.md` before acting. Follow `## Slash commands`, `## Scrum operating model`,
`## Story points`, and the `sprint-plan` procedure.

## Critical command rules

- Validate non-empty `SCOPE` against `^[a-z][a-z0-9_-]*$`; abort on mismatch.
- Gather read-only context in parallel when useful. Keep build, MCP, yaml, and edit steps serial.
- Ask the Product Owner to approve the proposed goal, committed backlog, enablers, snapshot, and story-point estimate before performing gate enablers.
- Gate enablers are limited to `mariogolf64.yaml` subseg flip/split/path-qualifier lines, `symbol_addrs.txt` add-only, and optional `make sync-names`.
- Validate with `make extract && make`; require `build/mariogolf64.z64: OK` and SHA-1 `e2c4e7a905b29529b49a1619a401fe699224829b`.
- Write `SPRINT.md` only after the validation is green.
- Hand off to the inline execution loop in `docs/agent-workflow.md ## Execution loop`; do not stop for per-function PO approval.
