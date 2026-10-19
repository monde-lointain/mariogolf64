# Harness code map

The harness is everything that builds, scores, selects, or polices the decompilation, as opposed to the decompiled source itself. Read `AGENTS.md` § Changing the harness before editing anything listed here. That section says which changes need extra evidence.

## Build and checksum (comparison)

These decide whether the ROM matches, so they are the final arbiter of every match.

- `Makefile`: extraction (`make extract`), compile, link, and the SHA1 check. The `COMPILER PROFILES` section holds per-directory and per-file compiler flags. A profile override is a source-level change: the checksum verifies it.
- `tools/build-and-verify.sh`: re-extracts, rebuilds, and checks `mariogolf64.sha1`. `verify_command` for nigel and the body of the pre-commit hook.
- `tools/git-hooks/pre-commit` (installed by `tools/install-git-hooks.sh` during `make setup`): rejects staged changes to `mariogolf64.sha1`, then runs `build-and-verify.sh`.
- `tools/n64crc.py`: fixes the ROM header CRC for `make nonmatching`.
- `tools/kmc-cc`, `tools/gcc_kmc/`: the pinned KMC GCC 2.7.2 toolchain. **Protected** (policy hook); never edited.

## Per-function environments (comparison)

`tools/decomp-env` symlinks every file in `tools/decomp-env-template/` into `nonmatchings/<function>[-N]/`, so editing `build.sh` inside an environment edits the template for every environment. The exception is `AGENTS.md`, which is rendered with the function name substituted, so edit the template to change it. It also symlinks `.omp` and `DECOMPILATION_LEARNINGS.md`, and writes `target.s`, `base.c` (m2c), and `.source_file`.

- `build.sh <file.c>`: compiles a candidate with the owning file's profile (`make print-cflags`), dumps and normalizes both objects, scores them with `dist.py`, and appends to `match_log.txt` (`100.000% exact sha256=…` for byte-identical code). It prints a stall warning when attempts stop improving.
- `dist.py`: the score, using penalty weights copied from decomp-permuter's scorer.
- `normalize_asm.py`, `objdump.py`: normalize dumps so that symbol-only differences do not count.
- `diff.sh`, `map_asm_to_c.py`: inspection only.
- `AGENTS.md`: the per-environment agent brief. `prelude.inc`: assembler prelude for `target.s`.
- `tools/decomp-env-cleanup`: deletes stale `nonmatchings/` directories whose function is already matched (`--delete`; a dry run otherwise).

## Match bookkeeping and selection

- `tools/nonmatching_match_log.py`: parses `match_log.txt` and certifies exact matches (comparison: it decides what "100%" means for the reporters below).
- `tools/report_nonmatching_matches.py`, `tools/find_unmatched_nonmatchings.py`: reports built on it.
- `tools/m2c_one_shot.py`: m2c plus `build.sh` scoring, and it inlines exact matches into `src/` (comparison: it writes source on a match decision).
- `tools/project_status.py`, `tools/score_functions.py`, `tools/list_decomp_candidates.py`, `tools/rank_decompile_similar_impact.py`, `tools/find_similar_functions.py`, `tools/list_matched_functions.py`: progress and work selection (workflow). `score_functions.py` is the nigel `candidate_source` for the decompile tasks.
- `tools/fetch_scratches.py`: pulls decomp.me scratches into `tools/scratches.json`.

## Data matching (comparison)

- `tools/data-differ/data_diff.py` (wrapper `tools/diff-data`): compares data symbols between the built ELF and the ROM. Sizes come from `symbol_addrs.txt`, the ELF, or `asm/data`.

## Code-quality checkers

These decide what counts as an acceptable match, so treat them as comparison changes.

- `tools/check_pointer_arithmetic.py`, `tools/detect_low_quality_matches.py`, `tools/fix_redundant_externs.py`.
- Header and type hygiene (workflow): `tools/analyse_headers.py`, `tools/find_header_hygiene_candidates.py`, `tools/find_source_header_pairs.py`, `tools/find_inconsistent_global_types.py`, `tools/find_oversized_symbols.py`, `tools/find_struct_unions.py`, `tools/m2ctx.py`.

## Permuter

- `tools/permuter`: imports a candidate into decomp-permuter with the owning file's profile (workflow).
- `tools/decomp-permuter/`, `tools/asm-differ/`: vendored submodules. **Protected**.

## Agent configuration

- `AGENTS.md`: project instructions for agents working on the repo. Environments get their own rendered copy of the template brief instead of the root file.
- `.omp/RULES.md`: sticky rules carried on every request. Because environments link `.omp`, they also apply there.
- `.omp/config.yml`, `.omp/skills/*/SKILL.md`: omp settings and skills.
- `.omp/hooks/pre/mg64-policy.ts`: the tool-call policy (guardrail). It blocks `asm/` writes, vendored-tool writes, `mariogolf64.sha1`, `ctx.c`/`temp.c`, direct `make`/splat, and untimed permuter runs. Tests: `.omp/hooks/tests/mg64-policy.test.ts`.
- `task-runner/config.yaml`, `task-runner/<task>/task.yaml`: nigel tasks. `reset_command` (`git clean -f; git reset --hard HEAD`) discards uncommitted work after every failed candidate.
- `DECOMPILATION_LEARNINGS.md`: curated generic compiler knowledge (rules in `AGENTS.md`).

## Tests

- Python tools: `.venv/bin/python3 -m unittest discover -s tools -p 'test_*.py'`. The tests import the tools' third-party dependencies, so use the venv interpreter; the system `python3` fails on import.
- Policy hook: `bun test ./.omp/hooks/tests/mg64-policy.test.ts`.
- Whole pipeline: `./tools/build-and-verify.sh`.
