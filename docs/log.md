# Docs Update Log

## 2026-10-19
* **Update**: `objdump.py` resolves named globals through `symbol_addrs.txt` as well as auto-named `D_`/`func_` symbols, so a relocation against `g_terrain_tile_cache+0x98E` equals `D_80132D4A+0x944`. Before/after on `func_ovl4_80250FB4`'s probe: 98.462% (20 differences) to 100.000% exact ([decomp-env](/tools/decomp-env.md)).
* **Update**: Overlays (`src/overlay_*`) now compile with `-DF3DEX_GBI_2`, like `src/main`: the overlay display-list builder `func_ovl3_8022A090` matches the ROM only with F3DEX2 opcodes (`E3`/`E1`/`F1`). See [compiler profiles](/project/compiler-profiles.md).
* **Update**: `src/overlay_7/func_ovl7_80206680.c` joined `FAST_MATH_SRCS`: `func_ovl7_802068B4` is a bare `sqrt.s` in the ROM, which only `-ffast-math` emits.
* **Update**: `src/overlay_0/func_ovl0_801FD5F0.c` joined `FAST_MATH_SRCS`: `func_ovl0_801FD760` is a bare `sqrt.s` in the ROM, which only `-ffast-math` emits.
* **Update**: `symbol_addrs.txt` gives `g_abRosterUnlockFlags` `size:0x8` (4 x s16 roster slots) and adds the overlay_3 array `D_ovl3_8022FF1C` (`size:0x10`, s32). Splat then emits `symbol + offset` references, which `func_ovl3_80209500` needs for an exact match.
* **Added**: `tools/integrate-shards` cherry-picks shard matches onto `dev` and verifies the ROM after each one. It stops only for commits a person must review. The `tools/merge-decomp-c` merge driver (`.gitattributes`) resolves the extern/`INCLUDE_ASM` conflicts between neighbouring matches. Replaying this run's nine conflicted picks, it reproduced all eight hand resolutions byte for byte and left the type clash conflicted ([task-runner](/tools/task-runner.md)).

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
* **Update**: `objdump.py` renders `%lo`/`%hi` for relocations against auto-named `D_<VRAM>`/`func_<VRAM>` symbols by resolved address, so two names for one address compare equal. Before/after on `func_ovl7_80206718`: 98.793% (70 differences) to 100.000% exact ([decomp-env](/tools/decomp-env.md)).
* **Added**: `tools/sync-shard`, which rebases nigel shard worktrees onto `MG64_SYNC_BRANCH` between candidates ([task-runner](/tools/task-runner.md)).
