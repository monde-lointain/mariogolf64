---
type: Tool
title: "task-runner (nigel)"
description: "nigel tasks that run an omp agent over candidate lists with verify, commit and reset handling; layout, task catalog, worktree rules."
tags: [nigel, automation]
resource: ../../task-runner/config.yaml
---

[nigel](https://github.com/cdlewis/nigel) runs a **candidate source**, hands each candidate to an agent with a templated prompt, runs `verify_command`, and commits or resets. A candidate counts as done when it no longer appears in the source; `accept_best_effort: true` keeps partial progress instead. Candidates already processed are recorded in `<task>/ignored.log` (delete entries to retry). nigel reads `nigel/`, falling back to `task-runner/`, which this repo uses.

```bash
nigel --list
nigel decompile-similar
nigel <task> --limit 10 --dry-run --verbose
nigel <task> --claims "$(git rev-parse --git-common-dir)/mg64/claims/<task>"
```

# Configuration

`task-runner/config.yaml`:

| Key | Value here |
|---|---|
| `agent` | `omp` (nigel adds `-p --mode json --approval-mode yolo`) |
| `verify_command` | `./tools/verify-candidate` ([gate](/tools/verify-candidate.md): protected paths, baserom hash, new pointer arithmetic on changed lines, then `build-and-verify.sh`) |
| `success_command` | `git add -A && git commit -m "task-runner($TASK_NAME): fix $CANDIDATE"` (tasks override it) |
| `reset_command` | `git clean -f; git reset --hard HEAD || true` |

`task-runner/<task>/task.yaml` keys: `candidate_source`, `prompt` or `template`, `agent_flags`, `accept_best_effort`, `timeout`, `success_command`, `verify_command`. A task's `verify_command` replaces the config's when nigel checks a candidate's changes, and takes the same `$CANDIDATE` and `$TASK_NAME` variables as `success_command`; the decompile tasks use it for `verify-candidate --decompiled $CANDIDATE`. Build checks after a reset (startup, or a discarded candidate) always use the config's command, because the clean tree would fail a candidate gate. Prompt variables: `$INPUT`, `$INPUT[n]`, `$INPUT[n:]`, `$INPUT["key"]` (index and slice need an array candidate, else interpolation fails) and `$TASK_ID`. nigel exports `NIGEL_TASK_NAME`, `NIGEL_CANDIDATE`, `NIGEL_RUN_ID`, `NIGEL_AGENT` and `NIGEL_AGENT_FLAGS` to the agent and to the verify, success and reset commands. With a `timeout`, nigel exports `NIGEL_TIMEOUT_SECONDS` and `NIGEL_TIMEOUT_DEADLINE_UNIX`, which the [policy hook](/harness/policy-hook.md) uses to bound permuter runs.

# Worktree rule

`reset_command` runs at startup and after every failed candidate and wipes uncommitted, non-ignored changes. Run nigel in a dedicated worktree, never in a checkout with uncommitted work. Create one with [new-worktree](/tools/new-worktree.md), which brings the ignored inputs (baserom, toolchain, `.venv`, submodules) along, and run one worker per worktree. Agents inherit nigel's environment and do not run direnv, so put `.venv/bin` on `PATH`; otherwise `python3 tools/asm-differ/diff.py` and the pycparser checkers fail on missing modules:

```bash
tools/new-worktree mg64-a && cd ../mg64-a
PATH="$PWD/.venv/bin:$PATH" MG64_SYNC_BRANCH=dev \
  nigel decompile-similar --claims "$(git rev-parse --git-common-dir)/mg64/claims/decompile-similar"
```

Workers of one task share its claims directory: each takes the first candidate that no other worker has claimed, so a candidate runs once across the worktrees. A claim is never released; a restarted worker keeps its own, and deleting a claim file reruns that candidate. A claim is keyed by the whole candidate, which is why the decompile candidates are bare function names: with the references in the candidate, two worktrees whose matched sets differ would claim the same function under two keys.

`difficulty_ledger.py harvest` records a run as `timeout` once its deadline passes, so killing nigel mid-candidate counts that function as a failure. `kill -QUIT <nigel pid>` stops it after the current candidate; `SIGINT` and `SIGTERM` kill the candidate. Keep nigel's output in a log file (`>> task-runner/<task>/nigel-run.log 2>&1`); `agent.log` holds only the agent stream.

Shard worktrees sync and integrate their commits with `tools/sync-shard`, `tools/integrate-shards` and the `decomp-c` merge driver; see [shard integration](/tools/shard-integration.md).

# Task catalog

Each decompile candidate source runs `tools/sync-shard && difficulty_ledger.py harvest && score_functions.py asm/nonmatchings --json` with the flags shown, which lists function names, easiest first. The four decompile tasks share `verify_command` `./tools/verify-candidate --decompiled $CANDIDATE` and a success command that stages tracked changes plus new files under `src/` and `include/` (a header or a split-off C file the match needs) and commits `Decompile <function>` only when something is staged.

| Task | Candidate source | Timeout | Notes |
|---|---|---|---|
| `decompile-similar` | `score_functions.py … --json` | 60 min | Tier 1. |
| `decompile-coddog-similar` | `score_functions.py … --by-coddog-similarity` | 60 min | Tier 1. Candidates are `[function, coddog matches…]`; the first match is passed as `--refs`. |
| `decompile-tier2` | `score_functions.py … --tier 2` | 120 min | Tier 2. The tier-1 reasons are in the brief's `<prior_attempts>`. Rewrites from the assembly when progress plateaus. |
| `decompile-tier3` | `score_functions.py … --tier 3` | 240 min | Tier 3. The tier-2 reasons are in the brief's `<prior_attempts>`. Tries every approach in the matching method that earlier attempts did not. |
| `cleanup-matched` | `python3 tools/list_matched_functions.py --by-reference-use` | 60 min | [cleanup-function](/procedures/cleanup-function.md) with `template.md`, most-referenced functions first. Config success command. |
| `fix-low-quality` | `python3 tools/detect_low_quality_matches.py src --json` | 30 min | [cleanup-function](/procedures/cleanup-function.md) with `template.md`. Config success command. |
| `cleanup-files` | `ls -1 src/main/func_*.c src/overlay_*/func_*.c 2>/dev/null` | 30 min | Cleans and renames each file with [rename-file](/procedures/rename-file.md). Commits `cleaned up $CANDIDATE`. |
| `cleanup-structs` | `python3 tools/find_source_header_pairs.py --json` | 60 min | Consolidates and names structs. Commits `clean up structs: $CANDIDATE`. |
| `deduplicate-structs` | `ls -1 src/main/*.c src/mgu/*.c src/overlay_*/*.c` | 60 min | Deduplicates structs. `accept_best_effort`. Config success command. |
| `remove-struct-unions` | `python3 tools/find_struct_unions.py --json` | 60 min | Removes unnecessary struct unions. `--thinking xhigh`. Commits `remove unnecessary struct unions from $CANDIDATE`. |
| `clean-headers` | `python3 tools/find_header_hygiene_candidates.py --json` | 60 min | Header ownership. `--thinking xhigh`. Commits `clean header ownership: $CANDIDATE`. |
| `introduce-headers` | `ls -1 src/main/*.c src/mgu/*.c src/overlay_*/*.c` | 30 min | Adds a header for the candidate's source file. Commits `Added header for $CANDIDATE`. |
| `consolidate-global-types` | `python3 tools/find_inconsistent_global_types.py` | 30 min | Makes a global's type consistent across files. Commits `consolidate global type: $CANDIDATE`. |
| `fix-symbol-footprints` | `python3 tools/find_oversized_symbols.py` | 60 min | Fixes oversized symbols. Commits `fix symbol footprint: $CANDIDATE`. |
| `name-functions-and-symbols` | `ls -1 src/main/*.c src/mgu/*.c src/overlay_*/*.c` | none | Renames with [rename-global](/procedures/rename-global.md) and [rename-file](/procedures/rename-file.md). Commits `name-functions-and-symbols: $CANDIDATE`. |
| `match-rodata-segments` | `sed -nE` over the unnamed `rodata` entries in `mariogolf64.yaml` | 60 min | [match-data-segment](/procedures/match-data-segment.md). Commits `task-runner($TASK_NAME): match rodata segment $CANDIDATE`. |
| `consolidate-learnings` | `python3 tools/list_unconsolidated_learnings.py --json` | 30 min | Merges verified KMC GCC lessons from `nonmatchings/*/LEARNINGS.md` and `NEAR_MISS.md` into `docs/reference/kmc-gcc/`, then `--mark`s the note. `accept_best_effort`. Commits only `docs/reference/kmc-gcc`. |

The candidate-source scripts are in [selection](/tools/selection-and-status.md) and [quality checkers](/tools/quality-checkers.md).

Decompile tasks escalate by [difficulty tier](/tools/difficulty-ledger.md): a function is offered to tier 2 or 3 only after failing the tier below. Every decompile task runs Haiku (`agent_flags`): tier-1 tasks at `:high` effort, tiers 2 and 3 at `:xhigh`. Each prompt runs `tools/decomp-env --bootstrap-only --id $TASK_ID <function>`, whose brief carries the five most similar matched functions as references, and points the agent at the rendered `AGENTS.md`, which is the whole per-function brief ([decompile-function](/procedures/decompile-function.md)).
