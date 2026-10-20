---
type: Reference
title: "Harness code map"
description: "Where each build, scoring, selection and policing job lives; which pieces are comparison-class; where the tests are."
tags: [harness, map]
---

Read [changing the harness](/harness/changing-the-harness.md) before editing anything here: it says which changes need extra evidence. Comparison-class entries below decide what counts as a match or what is written into `src/`.

# Build and checksum (comparison)

* `Makefile`: extraction (`make extract`), compile, link and the SHA1 check. The `COMPILER PROFILES` section holds per-directory and per-file flags ([profiles](/project/compiler-profiles.md)).
* `tools/build-and-verify.sh`: re-extract, rebuild, check `mariogolf64.sha1` ([tool](/tools/build-and-verify.md)). It is the body of the verify gate and of the pre-commit hook.
* `tools/verify-candidate`: nigel's `verify_command` ([tool](/tools/verify-candidate.md)).
* `tools/git-hooks/pre-commit`, installed by `tools/install-git-hooks.sh` during `make setup` ([hook](/harness/git-hook-and-ci.md)).
* `tools/Makefile`: installs the KMC toolchain. CI runs `make -C tools`.
* `tools/n64crc.py`: fixes the ROM header CRC. Only the `nonmatching` target runs it.
* `tools/kmc-cc` and `tools/gcc_kmc/`: the pinned KMC toolchain. **Protected.**

# Per-function environments (comparison)

* `tools/decomp-env`: creates `nonmatchings/<function>[-N]/` from `tools/decomp-env-template/`, renders its `AGENTS.md`, and writes the bootstrap files ([tool](/tools/decomp-env.md)).
* `tools/decomp-env-template/build.sh`: compiles one attempt with the owning file's profile, normalizes it, scores it, appends to `match_log.txt`, and prints `STOP:` at the stall limit ([decomp-env](/tools/decomp-env.md)).
* `tools/decomp-env-template/dist.py`: the score. Its `PENALTY_*` constants are copied from decomp-permuter's `scorer.py`.
* `tools/decomp-env-template/normalize_asm.py`, `objdump.py`: normalize dumps before scoring, so symbol-only differences do not count. `--symbol` limits both the dump and the score to the target function; branch targets and `.text` offsets are rebased to its start, and a `jal` to a nested child is written with the child's source name.
* `tools/decomp-env-template/diff.sh`, `map_asm_to_c.py`: inspection only.
* `tools/decomp-env-template/AGENTS.md`: the per-environment agent brief. `prelude.inc`: assembler prelude for `target.s`.
* `tools/decomp_env_context.py`: the brief's context block, rendered at bootstrap.
* `tools/decomp-env-cleanup`: removes environments for functions that are already matched ([selection and status](/tools/selection-and-status.md)).

# Match bookkeeping and selection

* `tools/nonmatching_match_log.py`: parses `match_log.txt` and certifies exact matches. Comparison: it defines what "100%" means to the reporters.
* `tools/report_nonmatching_matches.py`: report built on it.
* `tools/m2c_one_shot.py`: m2c plus `build.sh` scoring; inlines exact matches into `src/`. Comparison: it writes source on a match decision.
* `tools/m2ctx.py`: writes `ctx.c`, a preprocessed context file for m2c and decomp.me. The [policy hook](/harness/policy-hook.md) blocks reads of it.
* Selection and status: `progress.py` ([tool](/tools/progress.md)), `score_functions.py`, `list_decomp_candidates.py`, `find_similar_functions.py`, `list_matched_functions.py`, `fetch_scratches.py`, `difficulty_ledger.py` ([tool](/tools/difficulty-ledger.md)), `pipeline_metrics.py` ([tool](/tools/pipeline-metrics.md)). Catalogue: [selection and status](/tools/selection-and-status.md).
* Learnings: `list_unconsolidated_learnings.py` ([tool](/tools/list-unconsolidated-learnings.md)).
* Shards and worktrees: `tools/sync-shard`, `tools/integrate-shards`, `tools/new-worktree` ([tool](/tools/new-worktree.md)), and the merge drivers `tools/merge-decomp-c` and `tools/merge-splat-yaml` ([shard integration](/tools/shard-integration.md)).

# Data matching and quality checkers (comparison)

* `tools/data-differ/data_diff.py` (wrapper `tools/diff-data`): [data-differ](/tools/data-differ.md).
* `tools/check_pointer_arithmetic.py`, `tools/detect_low_quality_matches.py`, `tools/fix_redundant_externs.py` decide what is an acceptable match.
* Candidate finders `tools/find_*.py`: workflow, not comparison. [Quality checkers](/tools/quality-checkers.md) catalogues them.

# Permuter

* `tools/permuter` imports a candidate with the owning file's profile ([tool](/tools/permuter.md)).
* `tools/decomp-permuter/` and `tools/asm-differ/` are vendored submodules (`.gitmodules`). **Protected.**

# Agent configuration

* `AGENTS.md`: project instructions loaded into every session, including the short form of the [ground rules](/project/ground-rules.md). It is deliberately short and routes into `docs/`. Environments get a rendered copy of the template brief.
* `docs/`: this bundle. Its authoring rules are in [docs conventions](/harness/docs-conventions.md).
* `.omp/config.yml`, `.omp/skills/*/SKILL.md` (symlinks into `docs/procedures/`). Environments link `.omp`, so its settings apply there.
* `.omp/hooks/pre/mg64-policy.ts`: the tool-call guard ([policy hook](/harness/policy-hook.md)).
* `.omp/hooks/pre/mg64-unfinished-run.ts`: continues a nigel decompile session that stops before its run has an outcome ([`pending`](/tools/difficulty-ledger.md#pending)).
* `task-runner/config.yaml`, `task-runner/<task>/task.yaml`: nigel tasks ([task-runner](/tools/task-runner.md)).

# Tests

The unit tests run under the venv interpreter, because the tools they import need third-party packages (`pycparser`, `pyyaml`). CI runs none of them ([git hook and CI](/harness/git-hook-and-ci.md)).

```bash
.venv/bin/python3 -m unittest discover -s tools -p 'test_*.py'
bun test ./.omp/hooks/tests/mg64-policy.test.ts
```

`./tools/build-and-verify.sh` is the whole-pipeline check, not a unit test.

| Test file (`tools/`) | Covers |
|---|---|
| `test_build_sh.py` | `decomp-env-template/build.sh` (skipped without MIPS binutils, make or flock) |
| `test_check_pointer_arithmetic.py` | `check_pointer_arithmetic.py` |
| `test_decomp_env_cleanup.py` | `decomp-env-cleanup` |
| `test_decomp_env_context.py` | `decomp_env_context.py` |
| `test_difficulty_ledger.py` | `difficulty_ledger.py` |
| `test_dist_jumptable.py` | `decomp-env-template/dist.py` |
| `test_fix_redundant_externs.py` | `fix_redundant_externs.py` |
| `test_integrate_shards.py` | `integrate-shards` |
| `test_list_decomp_candidates.py` | `list_decomp_candidates.py` |
| `test_list_matched_functions.py` | `list_matched_functions.py` |
| `test_list_unconsolidated_learnings.py` | `list_unconsolidated_learnings.py` |
| `test_m2c_one_shot.py` | `m2c_one_shot.py` |
| `test_m2ctx.py` | `m2ctx.py` |
| `test_merge_decomp_c.py` | `merge-decomp-c` |
| `test_merge_splat_yaml.py` | `merge-splat-yaml` |
| `test_new_worktree.py` | `new-worktree` |
| `test_nonmatching_match_log.py` | `nonmatching_match_log.py` |
| `test_objdump_symbols.py` | `decomp-env-template/objdump.py` |
| `test_pipeline_metrics.py` | `pipeline_metrics.py` |
| `test_progress.py` | `progress.py` |
| `test_report_nonmatching_matches.py` | `report_nonmatching_matches.py` |
| `test_selection_tiers.py` | `score_functions.py`, `list_decomp_candidates.py`, and difficulty ledger tier selection |
| `test_sync_shard.py` | `sync-shard` |
| `test_target_prelude.py` | `decomp-env-template/prelude.inc` (skipped without `mips-linux-gnu-as` and `mips-linux-gnu-objdump`) |
| `test_verify_candidate.py` | `verify-candidate` (includes INCLUDE_ASM and pointer-arithmetic checks) |
