---
type: Tool
title: "Selection, ranking and status tools"
description: "Scripts that report progress, rank unmatched functions, find similar matched code, batch-run m2c, and report or clean per-function environments."
tags: [selection, status, workflow]
---

Run with the project venv (`.venv/bin/python3`) when third-party imports fail. All print `--help`.

# Progress and ranking

| Command | Purpose |
|---|---|
| `python3 tools/progress.py [--segments] [-u] [-o report.json]` | Code and data progress of the last build, by bytes and by functions or data symbols; `-u` lists unmatched functions largest first. See [progress](progress.md). |
| `python3 tools/score_functions.py [path] [--json] [--by-size \| --by-similarity \| --by-coddog-similarity] [--top-n N] [--tier 1\|2\|3]` | Rank unmatched functions in `asm/nonmatchings` by difficulty, size or similarity. `--tier` keeps only functions in that [difficulty tier](difficulty-ledger.md); the default is tier 1 (no failed attempt). `--by-similarity` and `--by-coddog-similarity` list the most similar already-decompiled functions per entry. It is the `candidate_source` of the nigel decompile tasks. |
| `python3 tools/find_similar_functions.py <function> [-n N] [-t T] [--coddog-similarity] [-v]` | Matched functions similar to an unmatched one, as references for matching. `--coddog-similarity` needs `coddog` on `PATH` (`cargo install coddog`). |
| `python3 tools/rank_decompile_similar_impact.py [--limit N] [--ranking overall\|reach] [--json]` | Rank unmatched functions by direct progress plus how much matching them improves other `decompile-similar` targets. Reads `task-runner/decompile-similar/ignored.log`. |
| `python3 tools/list_decomp_candidates.py [--summary] [--include-failed] <c files...>` | `INCLUDE_ASM` functions in the given C files, smallest first, with their difficulty tier. Skips functions above tier 1 in the [difficulty ledger](difficulty-ledger.md); a `NEAR_MISS.md` note with no ledger record also counts as a failure. `--include-failed` lists them too. |
| `python3 tools/list_matched_functions.py [--by-reference-use] [--top-n N]` | JSON list of matched function names (candidate source of the `cleanup-matched` task). `--by-reference-use` orders them by how many unmatched functions list them among their top-N similar references, most used first. |

# m2c

| Command | Purpose |
|---|---|
| `python3 tools/m2c_one_shot.py [--dry-run] [--limit N] [function...]` | Run m2c on unmatched functions and score each through the environment's `build.sh`. Without `--dry-run` it inlines exact matches into the owning C file. |
| `python3 tools/m2ctx.py <file.c>` | Write `ctx.c`, a preprocessed context for m2c or decomp.me, using the file's [compiler profile](/project/compiler-profiles.md). GCC nested function definitions are removed from the context, since m2c's parser rejects them. |

# Environments and scratches

| Command | Purpose |
|---|---|
| `python3 tools/report_nonmatching_matches.py [--full-only \| --partial-only] [--scan-root DIR] [--no-scratches]` | Certified 100% local matches and decomp.me scratches not yet integrated into `src/`, plus partial matches. |
| `python3 tools/find_unmatched_nonmatchings.py` | Environments whose best certified logged attempt is below 100% and whose function is not yet in `src/`. |
| `tools/nonmatching_match_log.py` | Library: parses `match_log.txt`; trusts a 100% entry only when its `.matches/` archive still hashes to the recorded sha256. |
| `python3 tools/fetch_scratches.py [--update]` | Pull decomp.me scratches into `tools/scratches.json` (git-ignored). |
| `tools/decomp-env-cleanup [--delete] [--verbose] [--force]` | Remove `nonmatchings/` directories for functions that are already matched (dry run unless `--delete`). Stale directories holding unconsolidated `LEARNINGS.md`/`NEAR_MISS.md` notes are kept unless `--force` (see [list-unconsolidated-learnings](list-unconsolidated-learnings.md)). |
| `python3 tools/difficulty_ledger.py record \| harvest \| show \| write-run` | Per-function difficulty ledger: record an outcome, back-fill harvested outcomes, list tiers, write an environment's `.run.json`. See [difficulty-ledger](difficulty-ledger.md). |

The certification logic and `m2c_one_shot.py`'s inlining decide what counts as a match; treat edits to them as [comparison changes](/harness/changing-the-harness.md).
