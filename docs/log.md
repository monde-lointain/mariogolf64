# Docs Update Log

## 2026-10-18
* **Initialization**: Converted `AGENTS.md`, `README.md`, `DECOMPILATION_LEARNINGS.md`, `tools/HARNESS.md`, `docker/README.md`, `tools/data-differ/README.md` and the omp skills into this bundle. `AGENTS.md` and `README.md` remain as short entry points that link here; the other files were removed and the skills became symlinks into `procedures/`.
* **Update**: Env scaffolding no longer links `DECOMPILATION_LEARNINGS.md` into `nonmatchings/<function>/`; the env brief reaches this bundle through `../../docs/`.
* **Update**: The policy hook now allows the read-only `make -s print-cflags` query that the docs and skills already told agents to run ([policy hook](/harness/policy-hook.md)).
* **Update**: Added coverage that was missing: `src/mgu` and overlays' profile, optional dependencies (`coddog`, `flock`, `xxd`), `decomp-env` flags and generated files, `data_diff.py` size resolution, nigel layout and task catalog, `.bss` status.
* **Update**: Merged the identical `match-data-file` and `match-data-segment` skills, folded `convert-asset-array` into [add-asset](/procedures/add-asset.md), and made skill names equal their directory names.
* **Update**: Reconciled [KMC GCC learnings](/reference/kmc-gcc/index.md) with the code-quality rules: inline assembly, empty barriers and fixed-register locals are prohibited, not "last resort".
* **Update**: Aligned stop rules with `tools/decomp-env-template/build.sh` (it warns after 20 attempts without improvement; it never issues a hard stop).
* **Update**: `build.sh` now prints a binding `STOP:` line at the stall limit (`MG64_STALL_LIMIT`, `.run.json`, default 20); [decompile-function](/procedures/decompile-function.md) records every stop in the [difficulty ledger](/tools/difficulty-ledger.md). The permuter left the agent loop and is documented as an offline tool ([decomp-permuter](/procedures/decomp-permuter.md)).
* **Added**: [verify-candidate](/tools/verify-candidate.md) (nigel's verify gate), [difficulty-ledger](/tools/difficulty-ledger.md) (tiers and escalation), [pipeline-metrics](/tools/pipeline-metrics.md), [list-unconsolidated-learnings](/tools/list-unconsolidated-learnings.md) and [new-worktree](/tools/new-worktree.md).
* **Update**: [task-runner](/tools/task-runner.md) documents the tiered decompile tasks (Haiku 5.5 at xhigh, budgets escalate by tier), per-task `verify_command`, the `NIGEL_*` variables, worktree setup through `new-worktree`, and the `consolidate-learnings` task.
* **Update**: [KMC GCC reference](/reference/kmc-gcc/index.md) corrected and extended from Snowboard Kids 2's learnings (same compiler); only claims reproduced by compile probes were merged.
* **Removed**: the `split-asm-segments` tool, task and doc, and assembly-only-segment scanning in the selection tools: every `asm` subsegment of `mariogolf64.yaml` is now a C file of `INCLUDE_ASM` stubs.
* **Update**: [decompile-function](/procedures/decompile-function.md) fixes the definition name (the `INCLUDE_ASM` name, not a caller's alias) and the commit subjects agents write.
