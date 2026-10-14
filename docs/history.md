---
type: Reference
title: Sprint history and reconstruction evidence
description: Distinguishes current workflow authority from time-scoped sprint records and function reconstruction notes.
sources:
  - resource: ../tools/pick_target_score.py
    title: Carry-over and WIP filename discovery
  - resource: ../tools/pick_target_classify.py
    title: Filename-specific carried-residual heuristics
  - resource: ../.omp/skills/mg64-decomp/SKILL.md
    title: Current workflow and durable evidence contract
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

Treat note titles, scores, `TERMINAL`/`CARRIED` labels, and recorded bank claims as evidence from
their cited attempt, not a fresh report on the current source. Even the `banked/` directory is a
recorded-resolution archive, not another acceptance gate. Inspect the current stub/source and
reproduce a measurement before reusing its conclusions; only the ROM verifier authorizes banking.

Filenames are executable discovery inputs, not just document organization. `carried_wall_names()`
unions `BACKLOG.md` carry-overs with **immediate** `docs/wip/*.md` filename stems; it does not recurse
into `banked/`. Separate residual heuristics read the exact `docs/wip/<fn>.near-match.md` path.
Do not rename or relocate a note merely to improve navigation: regroup its index entry instead.
A scoping or nested-function note is not necessarily a terminal wall.

A recorded count or score is comparable only with the same measurement method and source/profile.
See the [oracle table](workflow/loop.md#oracles). On resume, cite an existing measurement/source
pair where available; do not transcribe a fresh score into historical prose or invent missing proof.

Sprint tags in hazard playbooks point back to this evidence. Do not remove evidence merely because
its former hypothesis was superseded; change the current rule and record the new result separately.
