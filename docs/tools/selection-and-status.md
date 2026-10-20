---
type: Tool
title: "Selection, ranking and status tools"
description: "Scripts that report progress, rank unmatched functions, find similar matched code, batch-run m2c, and report or clean per-function environments."
tags: [selection, status, workflow]
---

Scripts print their options with `--help`. Run them with the project venv (`.venv/bin/python3`) when third-party imports fail. `nonmatching_match_log.py` is a helper module with no command line.

# Progress and ranking

| Command | Purpose |
|---|---|
| `python3 tools/progress.py [--segments] [-u] [-o report.json]` | Code and data progress of the last build. See [progress](/tools/progress.md). |
| `python3 tools/score_functions.py [path] [--json] [--by-size \| --by-similarity \| --by-coddog-similarity] [--top-n N] [--tier 1\|2\|3] [--score-func NAME] [--min-score S] [--max-score S]` | Rank unmatched functions in `asm/nonmatchings` (or `path`) by difficulty, size or similarity. `--by-similarity` lists each function with its most similar decompiled functions; `--by-coddog-similarity` lists coddog matches whose C body is in `src/`. `--top-n` sets the references per function (default 5). `--tier` keeps only functions at one [difficulty tier](/tools/difficulty-ledger.md); the default is tier 1, which has no failure history. This output is the `candidate_source` of the decompile tasks in [task-runner](/tools/task-runner.md). |
| `python3 tools/find_similar_functions.py <function> [-n N] [-t T] [--coddog-similarity] [--json] [-v]` | Matched functions similar to an unmatched one, as references for matching. `-n` sets how many to show (default 10); `-t` sets the minimum similarity (0 to 1). `--coddog-similarity` shows coddog matches (threshold 0.5) whose C body is in `src/`, and needs `coddog` on `PATH` (`cargo install coddog`). |
| `python3 tools/list_decomp_candidates.py <files…> [--summary] [--include-failed]` | `INCLUDE_ASM` functions in the given C files, smallest assembly first, each with its difficulty tier. Functions above tier 1 are skipped unless `--include-failed` is given, which includes functions with failure history (tier 2 and up) with their tier. Failure history comes from the [difficulty ledger](/tools/difficulty-ledger.md), where a `NEAR_MISS.md` note without a ledger record also counts as a failure. `--summary` prints only the totals. |
| `python3 tools/list_matched_functions.py [--by-reference-use] [--top-n N]` | JSON array of matched, auto-named function names (the candidate source of the `cleanup-matched` task). `--top-n` sets the similar references per unmatched function (default 3). `--by-reference-use` orders the names by how many unmatched functions list them among those references, most used first. |

# m2c

| Command | Purpose |
|---|---|
| `python3 tools/m2c_one_shot.py [functions…] [--dry-run] [--limit N] [--json] [--m2c-timeout S] [--build-timeout S]` | Run m2c on unmatched functions and score each through the decomp-env template's `build.sh`. Without `--dry-run` it inlines exact matches into the owning C file. `--limit` caps the candidates attempted (default 0, all). `--m2c-timeout` defaults to 30 seconds per m2c run and `--build-timeout` to 60 seconds per compile and score step. |
| `python3 tools/m2ctx.py <file.c>` | Write `ctx.c` at the repository root: a preprocessed context for m2c or decomp.me, built with the file's [compiler profile](/project/compiler-profiles.md). GCC nested function definitions are removed from the context, since m2c's parser rejects them. |

# Environments and scratches

| Command | Purpose |
|---|---|
| `python3 tools/report_nonmatching_matches.py [--full-only \| --partial-only] [--scan-root DIR]… [--scratches PATH \| --no-scratches] [--min-percent P] [--repo-root DIR]` | Certified 100% local matches and decomp.me scratches not yet integrated into `src/`, plus partial matches. `--scratches` defaults to `tools/scratches.json` under each scanned root when it exists. |
| `python3 tools/find_unmatched_nonmatchings.py [--repo-root DIR]` | Environments whose best certified logged attempt is below 100% and whose function is not yet in `src/`. |
| `tools/nonmatching_match_log.py` | Helper module: parses `match_log.txt`, and trusts a 100% entry only when its `.matches/` archive still hashes to the recorded sha256. |
| `python3 tools/fetch_scratches.py [--update] [--cursor CURSOR]` | Pull decomp.me scratches into `tools/scratches.json` (git-ignored). `--update` fetches only new or changed scratches; `--cursor` resumes from the cursor printed on an error exit. |
| `tools/decomp-env-cleanup [--delete] [--verbose] [--force]` | Remove `nonmatchings/` directories for functions that are already matched. Dry run unless `--delete`. Stale directories holding unconsolidated `LEARNINGS.md` or `NEAR_MISS.md` notes are kept unless `--force` (see [list-unconsolidated-learnings](/tools/list-unconsolidated-learnings.md)). |
| `python3 tools/difficulty_ledger.py record \| harvest \| show \| write-run` | Per-function difficulty ledger: record an outcome, back-fill harvested outcomes, list functions with failure history, write an environment's `.run.json`. See [difficulty-ledger](/tools/difficulty-ledger.md). |

The certification logic and `m2c_one_shot.py`'s inlining decide what counts as a match; treat edits to them as [comparison changes](/harness/changing-the-harness.md).
