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

Use the active native `task` schema; do not copy its prompt, add role wrappers, or
create project agents for bundled roles. Batch independent slices in one call:

- `context`: common goal, resource ownership, shared interfaces, and evidence paths.
- Each `task`: target/non-goals, current residual hypothesis, relevant source levers,
  and the observable deliverable. Children do not inherit the parent's conversation.
- `solutionSpace`: what reasoning remains open, not workload size.

Pass paths and narrow ranges, not complete backlog/history/context dumps. Large shared
payloads belong in `local://`; candidate evidence stays under `nonmatchings/<fn>/`.
Use the most specific available agent; unknown-file exploratory research is read-only
`scout` work. Existing functions, coupled nested pairs, and distinct residual hypotheses
are useful slices; at most two analysis angles per function.

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


Use auto-delivered results and `agent://` messaging; inspect `history://` only when the
result omits necessary evidence. Keep working after dispatch; wait only when blocked.
Native `/handoff` compacts the current session, not a worker-result transport. Do not
create a handoff document per worker or promise agent revival across lost sessions.

### Return evidence

Return the best proposed candidate/patch, required types/externs/helpers, and its next
measurable prediction. Distinguish explicitly:

- **Observed:** supplied or parent-measured baseline, command/profile/object, counts,
  frame, registers/order/operand residual, and source/assembly locations.
- **Hypothesis:** proposed source lever, expected effect, compiler `file:line` attribution
  when available, and what would disprove it. Uncompiled proposals are unmeasured.
- **Integration risks:** actual relocations, emitted rodata/pools, conflicting declarations,
  nested coupling, and source quality. No empty-asm barrier as a bank candidate; decode
  display-list words against SDK macros before integration.

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
