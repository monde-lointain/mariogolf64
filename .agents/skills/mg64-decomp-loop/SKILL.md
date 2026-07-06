---
name: mg64-decomp-loop
description: Execution loop for Mario Golf 64 decompilation. Use when working a committed SPRINT.md backlog, matching functions, running classical decomp, using Ghidra/m2c/asm-differ/decomp-permuter, banking functions, spiking carry-overs, or continuing an open MG64 sprint.
---

# MG64 Decomp Loop

Read `docs/agent-workflow.md` before acting. Follow `## Execution loop`, `## Conventions`, `## Cross-repo sync`, and any hazard sections cited by the target row.

## Workflow

1. Read `SPRINT.md`; abort if no committed backlog exists.
2. Work backlog items smallest-first, one function at a time, with no per-function PO stop.
3. Keep MCP, build, yaml, and source edits serial. Use parallel work only for read-only context gathering or explicitly safe isolated subagent fan-out.
4. For mirror targets, copy upstream C/headers as documented, format only the allowed trees, reconcile refs/calls, and prove the match with full `make` ROM SHA-1.
5. For classical targets, seed from asm + m2c + Ghidra typed context, iterate with asm-differ, spot-check bytes, inline only score-0 matches, and run full `make`.
6. Escalate to decomp-permuter or high reasoning only when the target reaches the documented threshold or hazard path. High reasoning is recommended guidance, not repo config.
7. When a function banks, append the standup line and suggestion buffer to `SPRINT.md`, commit the green match, and continue.
8. When a function blocks its file DoD, record it as stuck-far/permuter/carry as documented and continue only when the sprint rules allow.

Never commit a non-matching function. The ROM SHA-1 oracle is binding.
