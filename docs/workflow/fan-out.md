---
type: Workflow
title: Subagent fan-out
description: Defines independent analysis slices, serialized measurement ownership, and compact evidence returns using native Oh My Pi tasks.
sources:
  - resource: ../../RETRO.md
    title: Historical sprint evidence cited as case law
---

# Subagent fan-out

Work inline unless two or more independent analysis slices justify separate contexts,
or the user requests parallel work. Scope the functions and carried evidence before
choosing slices. Routine selection, small leaves, and ROM-verifier questions do not
need another agent. Parallelize reasoning, not the shared compiler or database.

Until the [main-completion gate](../../.omp/skills/mg64-decomp/SKILL.md#main-before-overlays)
passes, every decompilation slice must target the YAML `main` bank, including its linked libraries.
Put that bank scope and the overlay exclusion in shared `context`; workers must not substitute
an overlay when a main target is blocked. The parent validates bank membership before dispatch
and integration. Read-only overlay reference analysis may support a named main target.

## Dispatch contract

Use the active native `task` schema and bundled agent roles, not copied prompts or project
role wrappers. Batch independent reasoning slices; supply the exact bank/ROM target, parent
resource ownership, narrow evidence paths, and one unresolved question per slice. At most two
analysis angles per function. Workers do not inherit the parent conversation.

**The parent owns candidate/source edits, compiles, builds, Ghidra MCP, YAML changes,
integration, formatting, and ROM verification.** Workers return proposals. They do
not run checks, use MCP, launch permuters, or edit shared sources. General harness
advice to delegate investigation plus editing does not override this matching-resource
contract. The parent measures proposals serially with the ROM source profile and sends
back only the diagnostic needed for the next analysis.

Campaign sharding partitions reasoning queues by stable ROM/bank/offset identity. It does not
change execution ownership or license concurrent worktree builds. Use
[reference discovery and campaigns](../campaigns.md) to inspect another checkout's new C without
merging it; workers return reference identity/hashes and a source hypothesis, not a claimed bank.
The parent rechecks that evidence before measuring or integrating it.


Use native result delivery; send messages only for new evidence or steering, not routine check-ins.
Keep working after dispatch and wait only when blocked. Native task artifacts carry proposals;
do not create a handoff/result ledger per worker.

### Return evidence

Return the best proposed candidate/patch, required types/externs/helpers, and its next
measurable prediction. Distinguish explicitly:

- **Observed:** exact target bank/ROM offset and source/assembly locations. Cite existing
  measurement/context artifacts rather than copying their scores, flags, or full type context.
  When used, name the relevant signature/type source and its uncertainty.
- **Hypothesis:** one source lever, predicted instruction change, and what would disprove it.
  Uncompiled proposals are unmeasured.
- **Integration risks:** relocations, rodata/pool ownership, conflicting declarations, nested
  coupling, and [source eligibility](loop.md#permuter-eligibility). Decode DL words against SDK macros.

For reference research, report each explicitly requested root as inspected, unavailable, or not
inspected; never silently substitute a similarly named checkout. Missing coverage is not evidence
that a source or implementation does not exist.

A worker cannot supply a fresh measurement while forbidden to compile. Do not require
or accept fabricated scores to fill a return template. Only the parent's
[ROM verifier](loop.md#oracles) can authorize banking; native task completion is not
match evidence. Durable carries belong in the existing WIP note, not agent transcripts.

## What a fan-out is worth

A useful result is either a measured improvement or a supported hypothesis that narrows
further work—not a promise that every slice cracks. Before accepting a wall verdict:

1. Reach exact instruction count before arguing allocation/pressure; materialize values
   the ROM hoists or defers and rule out nested static-chain spills.
2. Ask what the verdict did **not** measure. Showing one instruction cannot move does
   not prove the unwanted hoist cannot be removed.
3. Treat cited compiler passes as revisable hypotheses. A new control-flow structure can
   bypass the pass; re-evaluate inherited carries when a new structural lever appears.
4. Re-measure the changed base before reusing rejected levers or eligibility scores.

## Wall classes a fan-out commonly returns

Route the measured residual through [residual lookup](../hazard-symptoms.md), then read
only its playbook. Common carry classes include base-register/displacement,
indexed-versus-pointer strength reduction, branch-likely value selection, and local
allocation permutations. Delay-slot liveness and cross-jump tail structure have useful
source levers; a class label alone never proves a wall. See [source levers](../levers.md).

## Case law

[RETRO.md](../../RETRO.md) retains the original measurements and reasoning. Relevant
sprints: 232–233 (fan-out outcomes), 260 (bypassing a cited pass), 272 (exact count before
pressure), 300 (the unmeasured side of a wall claim), and 318 (permuter-derived levers).
Read only the cited excerpt; historical verdicts are not current policy.
