---
type: Reference
title: "Harness code map"
description: "Where each build, scoring, selection and policing job lives; which pieces are comparison-class; where the tests are."
tags: [harness, map]
---

Read [changing the harness](changing-the-harness.md) before editing anything here: it says which changes need extra evidence.

# Build and checksum (comparison)

These decide whether the ROM matches.

* `Makefile`: extraction (`make extract`), compile, link, and the SHA1 check. The `COMPILER PROFILES` section holds per-directory and per-file flags ([profiles](/project/compiler-profiles.md)).
* `tools/build-and-verify.sh`: re-extract, rebuild, check `mariogolf64.sha1` ([tool](/tools/build-and-verify.md)). It is the body of the verify gate and of the pre-commit hook.
* `tools/verify-candidate`: nigel's `verify_command`. Checks protected paths, the baserom hash and (with `--decompiled`) that the function is in `src/` without `INCLUDE_ASM`, then runs build-and-verify ([tool](/tools/verify-candidate.md)). Its test is `tools/test_verify_candidate.py`.
* `tools/git-hooks/pre-commit`, installed by `tools/install-git-hooks.sh` during `make setup` ([hook](git-hook-and-ci.md)).
* `tools/n64crc.py`: fixes the ROM header CRC for `make nonmatching`.
* `tools/kmc-cc`, `tools/gcc_kmc/`: the pinned KMC GCC 2.7.2 toolchain. **Protected.**

# Per-function environments (comparison)

`tools/decomp-env` symlinks every file in `tools/decomp-env-template/` into `nonmatchings/<function>[-N]/`, so editing `build.sh` inside an environment edits the template for every environment. `AGENTS.md` is the exception: it is rendered with the function name substituted, so edit the template. It also symlinks `.omp` and writes `target.s`, `target.o`, `base.c` and `.source_file` ([tool](/tools/decomp-env.md)).

* `build.sh <file.c>`: compile with the owning file's profile, normalize, score with `dist.py`, append to `match_log.txt`, print a `STOP:` line at the stall limit.
* `dist.py`: the score; penalty weights copied from decomp-permuter's scorer.
* `normalize_asm.py`, `objdump.py`: normalize dumps so symbol-only differences do not count. Scoring is limited to the target symbol (`--symbol`); branch targets and `.text` offsets are rebased to its start. A `jal` to a nested child prints the child's source name (`name.N` becomes `name`), as the target object does.
* `diff.sh`, `map_asm_to_c.py`: inspection only.
* `AGENTS.md`: the per-environment agent brief. `prelude.inc`: assembler prelude for `target.s`.
* `tools/decomp-env-cleanup`: delete stale environments.

# Match bookkeeping and selection

* `tools/nonmatching_match_log.py`: parses `match_log.txt` and certifies exact matches (comparison: it defines what "100%" means to the reporters).
* `tools/report_nonmatching_matches.py`, `tools/find_unmatched_nonmatchings.py`: reports built on it.
* `tools/m2c_one_shot.py`: m2c plus `build.sh` scoring; inlines exact matches into `src/` (comparison: it writes source on a match decision).
* Workflow: `progress.py` ([tool](/tools/progress.md)), `score_functions.py` (nigel `candidate_source`), `list_decomp_candidates.py`, `rank_decompile_similar_impact.py`, `find_similar_functions.py`, `list_matched_functions.py`, `fetch_scratches.py` ([catalog](/tools/selection-and-status.md)).

# Data matching and quality checkers (comparison)

* `tools/data-differ/data_diff.py` (wrapper `tools/diff-data`): [data-differ](/tools/data-differ.md).
* `tools/check_pointer_arithmetic.py`, `tools/detect_low_quality_matches.py`, `tools/fix_redundant_externs.py` decide what is an acceptable match. Header and type hygiene finders are workflow ([catalog](/tools/quality-checkers.md)).

# Permuter

* `tools/permuter` imports a candidate with the owning file's profile ([tool](/tools/permuter.md)). `tools/decomp-permuter/` and `tools/asm-differ/` are vendored submodules, **protected**.

# Agent configuration

* `AGENTS.md`: project instructions loaded into every session, including the short form of the [ground rules](/project/ground-rules.md); deliberately short, it routes into `docs/`. Environments get a rendered copy of the template brief; the root file also reaches them because omp walks up from the working directory.
* `.omp/config.yml`, `.omp/skills/*/SKILL.md` (symlinks into `docs/procedures/`). Environments link `.omp`, so its settings apply there.
* `.omp/hooks/pre/mg64-policy.ts`: the tool-call guard ([policy hook](policy-hook.md)).
* `task-runner/config.yaml`, `task-runner/<task>/task.yaml`: nigel tasks ([task-runner](/tools/task-runner.md)).
* `docs/`: this bundle.

# Tests

* Python tools: `.venv/bin/python3 -m unittest discover -s tools -p 'test_*.py'`. Tests import the tools' third-party dependencies, so use the venv interpreter.
* Policy hook: `bun test ./.omp/hooks/tests/mg64-policy.test.ts`.
* Whole pipeline: `./tools/build-and-verify.sh`.
