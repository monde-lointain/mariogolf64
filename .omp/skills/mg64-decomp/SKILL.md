---
name: mg64-decomp
description: Select, decompile, iterate, bank, and review Mario Golf 64 functions while preserving matching, source-edit, profile, and shared-resource constraints.
---

# Mario Golf 64 decomp workflow

Use this workflow for function selection, classical decompilation, upstream mirrors, bank decisions, and review. The ROM SHA-1 against `baserom.z64` is the only bank authority: `tools/verify-rom.sh` must succeed. All other comparisons are iteration signals.

Use the configured Python interpreter `uv run --locked python`; the default binary tree is `build/us`.

## Select

1. Rank candidates with `uv run --locked python tools/pick_target.py -n 12`; use `--lib`, `--segment`, or `--loose-stubs` to scope. Read only the selected candidate's backlog/WIP evidence. Check carried walls and nested children before pricing hand-selected leaves (`--carried-check`, `--nested-check`). Inspect its assembly and source comments; ranker signals are not proof of tractability.
   `--lib` filters substrings, not segment ranges. Size leaves from their assembly headers, not adjacent VRAM deltas. Re-derive carried signatures, declarations, instruction counts, and residuals from current evidence; a prior compiler-pass verdict is a hypothesis.
2. Route hazards through [the hazard index](../../../docs/hazard-index.md), then read the selected playbook. Consult [C coding style](../../../docs/coding-style.md) for source conventions.
3. Resolve the source subsegment in `mariogolf64.yaml`. Change only explicitly required subsegment flip/split/path lines; never edit generated extraction outputs by hand. Preserve `INCLUDE_ASM` for unbanked functions. Do not change C, headers, or YAML unless the task requires that source change.
   A compiler jump table couples source and rodata: carve it only when the C switch regenerates its symbol.
4. Keep candidate experiments isolated under `nonmatchings/<func>/`; do not run concurrent builds. The build tree and Ghidra MCP are shared resources: serialize access to both.
   Work inline by default. Delegate only independent analysis that justifies separate contexts; the parent serializes candidate compiles, builds, MCP, and source integration. Use the native `task` contract and [fan-out guidance](../../../docs/workflow/fan-out.md), not a copied dispatch prompt.

## Iterate

- For an upstream mirror, copy the pinned upstream source and required headers, then validate with `tools/verify-rom.sh`. Check names, calls, data references, and routed hazards before treating it as a clean mirror.
- For classical work, read the function's extracted assembly as ground truth. Use Ghidra decompilation and types only as shape/context evidence. Seed `nonmatchings/<func>/base.c` with `uv run --locked python tools/seed_c.py --func <func> --parent src/<seg>.c`, then iterate with `uv run --locked python tools/decomp_loop.py --func <func>`. Inspect the measured residual and re-derive its class from a fresh object before choosing a source lever.
  Add `--ghidra` to fetch live context from the running [bank-aware database](../../../docs/ghidra.md#agent-access-and-context); the seed resolves ROM/VRAM from source assembly. Omit `--parent` when its partial types conflict. Ghidra is optional, never a bank gate.
- A score of zero is only a candidate: byte-compare the isolated and in-tree `.text`, then inline the body, remove its stub, format, and run the full ROM verifier. Escalate to the permuter at `percent >= 0.97`, or exact instruction count with a single-operand residual; carry a measured, documented near-match rather than claiming a match.
- Preserve profile fidelity. Isolated compilation must use the same per-source profile and flags as the ROM build; use `tools/rom_build.py` and the authoritative `tools/build_profiles.json` rather than hand-assembled compiler commands. Never infer a matching result from an isolated compile alone.

## Bank

A function is banked only after `tools/verify-rom.sh` exits successfully. Do not substitute a direct hash, object comparison, or incremental diff for this full-ROM oracle. Before renaming a function, check committed C callers and update them with the rename. Stage source inputs only; generated extraction artifacts stay ignored in the binary tree. A file is complete only when all its `INCLUDE_ASM` stubs are gone. Keep unfinished work isolated and describe measured residuals in `docs/wip/<func>.near-match.md`.

## Review and resume

Review the banked source and verifier result, and distinguish verified matches from carried candidates. Count functions as well as remaining stubs: extraction may already emit empty leaves as C. Preserve backlog carry-overs and technical evidence; update task-specific notes as required. On resume, reconcile the current task state with committed changes and the tracked backlog before continuing.

## Domain references

- [Execution details and tool authority](../../../docs/workflow/loop.md)
- [Candidate and symptom routing](../../../docs/index.md)
- [Subagent isolation and fan-out](../../../docs/workflow/fan-out.md)
- [Build profiles and verification](../../../docs/build.md)
