---
type: Workflow
title: Subagent fan-out
description: Defines independent analysis slices, serialized measurement ownership, and compact evidence returns using native Oh My Pi tasks.
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
role wrappers. Put shared bank/resource constraints and background in `context`; each task
gets a self-contained target, narrow evidence paths, and one unresolved question. Partition
independent evidence, not duplicate whole-function investigations. Workers do not inherit
the parent conversation.

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


### Return evidence

Return the best proposed candidate/patch, required types/externs/helpers, and its next
measurable prediction. Distinguish explicitly:

- **Observed:** exact target bank/ROM offset and source/assembly locations. Cite existing
  measurement/context artifacts rather than copying their scores, flags, or full type context.
  Signature/layout proposals must satisfy the [source-recovery gate](loop.md#source-recovery);
  a prologue-only or stale-register inference is not a recovered call contract.
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

## Accepting a proposal

Accept a source lever and a falsifiable prediction, not the worker's confidence or completion
status. The parent applies [source eligibility](loop.md#permuter-eligibility) and
[residual diagnosis](loop.md#residual-diagnosis) to the measured base before interpreting a
compiler-pass or wall claim. Preserve one reproducible carry in the existing WIP note;
native agent artifacts are transient communication, not a second durable ledger.

