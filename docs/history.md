---
type: Reference
title: Sprint history and reconstruction evidence
description: Distinguishes current workflow authority from time-scoped sprint records and function reconstruction notes.
---

# Sprint history and reconstruction evidence

## Current rules versus recorded evidence

The native [decomp skill](../.omp/skills/mg64-decomp/SKILL.md) owns current selection, iteration,
banking, and review guidance. Historical records may retain old compiler assumptions, counts, process
rules, or citations to workflow sections that have since moved. Historical instructions are not
current policy.

## Project records

- [BACKLOG.md](../BACKLOG.md) — Current ordering context and carry-overs, alongside historical entries.
- [RETRO.md](../RETRO.md) — Newest-first sprint digests and measured results; retained as historical evidence.
- [VELOCITY.md](../VELOCITY.md) — Historical velocity measurements; retain the original record.

These root artifacts retain their existing filenames and heading anchors. Adding concept metadata
does not rewrite historical entry prose. They are not renamed to `log.md`: their existing structure
and citations carry project-specific meaning.

`SPRINT.md` may be used as a task-specific resume note. Its absence in a fresh checkout is normal.
The current workflow and resume guidance are in the [native decomp skill](../.omp/skills/mg64-decomp/SKILL.md).

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
