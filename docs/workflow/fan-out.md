---
type: Workflow
title: Subagent fan-out
description: Defines independent analysis slices, serialized measurement ownership, and compact evidence returns using native Oh My Pi tasks.
---

# Subagent fan-out

Work inline unless the user requests parallel work or independent questions need separate
contexts. Scope targets and evidence first; parallelize reasoning, not shared execution.
Use the live native `task` schema and agent catalog rather than copied role prompts or wrappers.

Until the [main-completion gate](../../.omp/skills/mg64-decomp/SKILL.md#main-before-overlays)
passes, decompilation slices target the YAML `main` bank, including linked libraries.
The parent validates live stub/host and bank membership before dispatch and integration.
Read-only overlay references may support a named main target, not replace blocked main work.

## Dispatch contract

**The parent owns candidate/source edits, compiles, builds, Ghidra MCP, YAML changes,
integration, formatting and ROM verification.** Workers return proposals; they do not
run checks, use MCP, launch permuters or edit shared sources. This matching-resource
contract takes precedence over generic investigate-and-edit delegation advice.

Put shared bank/resource constraints in batch `context`. For each decompilation
residual question, provide only the evidence needed to distinguish remaining causes:

- The immutable measurement `.json`/`.c` pair, parent TU/profile and target assembly.
  Mutable `base.c` and old dumps are not a reproducible pair.
- The first differing instruction region, relevant compiler dump and dump-producing
  command, tied to that measured source/profile. Unknown dump provenance is missing
  evidence; the parent supplies or regenerates diagnostics serially.
- Already-rejected levers and their measured effect or invalidating reason. A worker
  proposing one again must identify what changed in its assumptions or source shape.
- One unresolved question and the observation that would settle it. If evidence is
  insufficient, request that observation instead of inventing a patch or a wall verdict.

Campaign shards divide analysis queues, not execution ownership. Reference lookup uses
[existing discovery](../campaigns.md); source presence or similarity is not match proof.

### Return evidence

- **Observed:** cite the inspected source/assembly/dump and distinguish fact, inference
  and unavailable evidence. Signatures/layouts satisfy the [source-recovery gate](loop.md#source-recovery),
  including widths, offsets, stride and caller/callee dataflow.
- **Proposed:** give one faithful source lever, required types and a falsifiable
  instruction-level prediction. Apply [source eligibility](loop.md#permuter-eligibility)
  before suggesting an experiment: invalid offsets or unsequenced argument assignments
  cannot test equivalent code generation.
  Reference research returns supported conclusions and limits, not a candidate to fill a template.
- **Limits:** name unresolved contracts, coupling and missing observations. For reference
  research, mark each requested root inspected, unavailable or not inspected; a failed
  search is not proof that an implementation does not exist.

A read-only worker cannot claim a fresh compile or score. The parent measures accepted
proposals against the cited base under the ROM source profile; an agent completion or
compiler citation is not match proof. Keep the selected base and rejected hypotheses
in existing WIP/BACKLOG evidence, not another ledger. Native task artifacts are coordination,
not durable bank authority.
