---
type: Guide
title: Mario Golf 64 decomp agent workflow
description: Routes agents to the sprint gates, execution loop, hazards, fan-out rules, and coding guidance they need for each phase.
---

# Mario Golf 64 Decomp agent workflow

<role>
Start here to route into the project workflow. The append-only logs cite this path; its headings are
frozen anchors. Read the phase reference you need rather than opening `docs/workflow/` wholesale.
</role>

Decompile Mario Golf 64 (N64) one function at a time toward a byte-exact ROM. The ROM SHA-1 against
`baserom.z64` is the sole arbiter of a match, and [`tools/verify-rom.sh`](../tools/verify-rom.sh)
is the bank gate.

## Workflow at a glance

Each sprint has a plan gate, an autonomous execution loop, and a review gate. `SPRINT.md` is the
resume surface. The [gates](workflow/gates.md#scrum-operating-model) own the Scrum contract;
the [loop](workflow/loop.md#execution-loop-inline) owns execution and oracle authority.

- **Effort.** Run the gates as cheap triage; use higher effort for classical decomp and permuter work.
- **Parallelism.** Read-only context gathering may run in parallel. Keep MCP, build, YAML, and source
  edits serial because the session holds no MCP lock and the build is one shared tree.
- **Delegation.** Follow the [trigger table](workflow/fan-out.md#when-to-fan-out), then fill in the
  [dispatch contract](fanout-prompt.md). Fan-out is not the default.

## Where to read

| Phase or need | Read |
| --- | --- |
| Plan or review | [Plan](workflow/gates.md#sprint-plan-procedure) or [review](workflow/gates.md#sprint-review-procedure) |
| Execution | [Function-matching loop](workflow/loop.md#execution-loop-inline) |
| Candidate/build hazard | [Flag lookup](hazard-index.md#flag-to-section), then the selected playbook |
| Matching residual | [Symptom lookup](hazard-symptoms.md#symptom-to-section), then the selected playbook |
| Crack slice or fan-out | [Fan-out rules](workflow/fan-out.md) and [dispatch contract](fanout-prompt.md) |
| Writing or naming C | [`docs/coding-style.md`](coding-style.md) |
| Editing a prompt surface | [`docs/prompt-style.md`](prompt-style.md) |

## Slash commands

- **Claude Code:** `/sprint-plan [scope]`, `/decomp-loop`, `/sprint-review`.
- **Codex:** `$mg64-sprint-plan [scope]`, `$mg64-decomp-loop`, `$mg64-sprint-review`.

Invocation behavior is in the [gate entry points](workflow/gates.md#gate-entry-points).

## Scrum operating model

The [Scrum contract](workflow/gates.md#scrum-operating-model) defines roles, sprint sizing,
readiness, completion, spikes, carry-overs, retro-gating, and resume behavior.

## Story points

The [estimation rubric](workflow/gates.md#story-points) defines the seed, 8-point decompose gate,
exemptions, per-file banking, and realized tier.

## Execution loop

Use the [loop procedure](workflow/loop.md#execution-loop-inline) and
[oracle table](workflow/loop.md#oracles) for tool authority and failure modes.

## Conventions

Use the [every-sprint conventions](workflow/loop.md#conventions-every-sprint) for cross-cutting rules.

## Hazard index

Use the [flag lookup](hazard-index.md) or [symptom lookup](hazard-symptoms.md) to select a
[playbook](hazards.md#playbook-index).

## Cross-repo sync

Use the [Ghidra synchronization procedure](workflow/loop.md#cross-repo-sync-ghidra-workspace-at-developmentreversingghidramariogolf64).
