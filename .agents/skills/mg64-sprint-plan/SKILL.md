---
name: mg64-sprint-plan
description: Plan gate for the Mario Golf 64 decomp workflow. Use when the user invokes $mg64-sprint-plan, asks to plan the next MG64 sprint, rank decomp candidates, choose a sprint increment, perform gate enablers, validate the scaffold build, or write SPRINT.md.
---

# MG64 Sprint Plan

Read `docs/agent-workflow.md` before acting. Follow `## Slash commands`, `## Scrum operating model`, `## Story points`, and the plan-gate procedure.

## Workflow

1. Validate the optional scope argument against `^[a-z][a-z0-9_-]*$`; abort on mismatch.
2. Read `BACKLOG.md`, run `venv/bin/python3 tools/pick_target.py -n 12`, and add `--lib <scope>` when a valid scope is supplied.
3. Pick the smallest coherent increment, honoring carry-overs, hazards, the 8-point gate, and DoR rules in `docs/agent-workflow.md`.
4. Present the goal, committed backlog, gate enablers, snapshot, and story-point estimate to the Product Owner. Use `request_user_input` when available; otherwise ask directly.
5. After approval, perform only gate enablers: `mariogolf64.yaml` subseg flip/split/path-qualifier lines, `symbol_addrs.txt` add-only, and optional `make sync-names`.
6. Validate with `make extract && make`; require `build/mariogolf64.z64: OK` and SHA-1 `e2c4e7a905b29529b49a1619a401fe699224829b`.
7. Write `SPRINT.md`. If the committed regime is classical or mixed, make the seed-freeze commit as documented.
8. Hand off explicitly: tell the user to invoke `$mg64-decomp-loop` for execution. Do not assume automatic skill chaining.

Keep this gate cheap. Use medium reasoning unless the candidate selection itself becomes ambiguous.
