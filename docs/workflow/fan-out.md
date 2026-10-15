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
role wrappers. Put shared bank/resource constraints and target identity in `context`; each
slice gets one unresolved question, the evidence needed to answer it, and a concrete return.
Validate the live stub/host before dispatch; include the measured source/profile and
assembly/dump paths plus any coupled parent/child boundary. Workers do not inherit the
parent conversation. Partition evidence, not duplicate whole-function dives.

**The parent owns candidate/source edits, compiles, builds, Ghidra MCP, YAML changes,
integration, formatting, and ROM verification.** Workers return proposals. They do
not run checks, use MCP, launch permuters, or edit shared sources. General harness
advice to delegate investigation plus editing does not override this matching-resource
contract. The parent measures proposals serially with the ROM source profile and sends
back only the diagnostic needed for the next analysis.

Campaign shards divide analysis queues, not execution ownership. Reference research uses
[existing discovery](../campaigns.md); source presence or similarity is not a match.

### Return evidence

Return the requested findings with file/line or artifact citations and explicit unknowns.
Steering updates the evidence; it does not replace the assignment. The final response must
still contain the complete requested result, not only an acknowledgement or progress update.

- **Observed:** inspected target/reference identity and relevant evidence. Cite the existing
  measurement/source pair instead of copying its full context. Signature/layout conclusions
  must satisfy the [source-recovery gate](loop.md#source-recovery).
- **Proposed:** for a decompilation slice, give the source lever or patch, required types, and
  a falsifiable instruction-level prediction. For research, give the supported conclusion
  and its limits; do not invent a candidate or experiment to fill a template.
- **Risks:** unresolved contracts, relocation/pool/nested coupling, and
  [source eligibility](loop.md#permuter-eligibility), where relevant.

For reference research, report each explicitly requested root as inspected, unavailable, or not
inspected; never silently substitute a similarly named checkout. Missing coverage is not evidence
that a source or implementation does not exist.

A read-only worker cannot report a fresh compile or score. The parent measures proposals
against the cited base under the ROM source profile, then applies
[source eligibility](loop.md#permuter-eligibility) and [residual diagnosis](loop.md#residual-diagnosis).
A compiler citation or completed agent task is not match proof. Keep reproducible carries in
the existing WIP note; native messages and task artifacts support coordination, not a second ledger.

