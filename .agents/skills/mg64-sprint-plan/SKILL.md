---
name: mg64-sprint-plan
description: Plan gate for the Mario Golf 64 decomp workflow. Use when the user invokes $mg64-sprint-plan, asks to plan the next MG64 sprint, rank decomp candidates, choose a sprint increment, perform gate enablers, validate the scaffold build, or write SPRINT.md.
---

# MG64 Sprint Plan

Read `docs/workflow/gates.md` before acting. Follow `## Sprint-plan procedure`,
`## Scrum operating model`, and `## Story points`.

Outcome: an approved goal, a small committed backlog, a green validated flip, and a written
`SPRINT.md`. Stop if the Product Owner does not approve. Never perform a gate enabler before
approval, and never validate with a hand-rolled `make; sha1sum` -- `tools/verify-rom.sh` is the gate.

Validate the optional scope argument against `^[a-z][a-z0-9_-]*$` and abort on mismatch.

Hand off explicitly when the gate closes: tell the user to invoke `$mg64-decomp-loop`. Skills do not
chain automatically.
