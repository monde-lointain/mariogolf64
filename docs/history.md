---
type: Reference
title: Sprint history and reconstruction evidence
description: Distinguishes current workflow authority from time-scoped sprint records and function reconstruction notes.
---

# Sprint history and reconstruction evidence

## Current rules versus recorded evidence

The [sprint gates](workflow/gates.md) own approval, readiness, scope, and story-point rules;
the [execution loop](workflow/loop.md) owns matching and banking. Use those guides for current action.
Historical records may retain old compiler assumptions, counts, process rules, or citations to a
workflow section that has since moved. A historical instruction is not a second current policy.

## Sprint records

- [BACKLOG.md](../BACKLOG.md) — Product Owner ordering rationale, enablers, and carry-overs; its
  dated figures describe their recorded sprint, not a live candidate census.
- [RETRO.md](../RETRO.md) — newest-first sprint digests, measured results, and accepted improvements.
- [VELOCITY.md](../VELOCITY.md) — seed/realized points and regime-specific history; do not compare
  velocities across regimes or treat early introductory wording as the current phase.

These root artifacts retain their existing filenames and heading anchors. Adding concept metadata
does not rewrite historical entry prose. They are not renamed to `log.md`: their existing structure
and citations carry project-specific meaning.

`SPRINT.md` is the gitignored active board and resume surface, not a distributed bundle concept.
Absence in a fresh checkout is normal. Its instruction sections follow
[prompt style](prompt-style.md); the resume procedure is in the
[execution loop](workflow/loop.md#execution-loop-inline).

## Function evidence

- [Reconstruction notes](wip/index.md) — residuals, scoping work, source levers tried, and constraints.
- [Banked evidence](wip/banked/index.md) — prior near-match notes kept after their recorded resolution.

Filenames are part of candidate discovery: `--carried-check` uses WIP note stems, so moving an active
note to change a classification changes implementation behavior. A scoping or nested-function note
is not necessarily a terminal wall. Read the evidence and inspect the current source before deciding
whether to reopen it. A recorded count or score is comparable only with the same measurement method;
see the [oracle table](workflow/loop.md#oracles).

Sprint tags in hazard playbooks point back to this evidence. Do not remove evidence merely because
its former hypothesis was superseded; change the current rule and record the new result separately.
