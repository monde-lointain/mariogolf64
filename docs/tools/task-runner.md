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
nigel <task> --shard 1/4
```

# Configuration

`task-runner/config.yaml`:

| Key | Value here |
|---|---|
| `agent` | `omp` (nigel adds `-p --mode json --approval-mode yolo`) |
| `verify_command` | `./tools/verify-candidate` ([gate](verify-candidate.md): protected paths, baserom hash, then `build-and-verify.sh`) |
| `success_command` | `git add -A && git commit -m "task-runner($TASK_NAME): fix $CANDIDATE"` (tasks override it) |
| `reset_command` | `git clean -f; git reset --hard HEAD || true` |

`task-runner/<task>/task.yaml` keys: `candidate_source`, `prompt` or `template`, `agent_flags`, `accept_best_effort`, `timeout`, `success_command`, `verify_command`. A task's `verify_command` replaces the config's when nigel checks a candidate's changes, and takes the same `$CANDIDATE` and `$TASK_NAME` variables as `success_command`; the decompile tasks use it for `verify-candidate --decompiled $CANDIDATE`. Build checks after a reset (startup, or a discarded candidate) always use the config's command, because the clean tree would fail a candidate gate. Prompt variables: `$INPUT`, `$INPUT[n]`, `$INPUT[n:]`, `$INPUT["key"]` (index and slice need an array candidate, else interpolation fails) and `$TASK_ID`. nigel exports `NIGEL_TASK_NAME`, `NIGEL_CANDIDATE`, `NIGEL_RUN_ID`, `NIGEL_AGENT` and `NIGEL_AGENT_FLAGS` to the agent and to the verify, success and reset commands. With a `timeout`, nigel exports `NIGEL_TIMEOUT_SECONDS` and `NIGEL_TIMEOUT_DEADLINE_UNIX`, which the [policy hook](/harness/policy-hook.md) uses to bound permuter runs.

# Worktree rule

`reset_command` runs at startup and after every failed candidate and wipes uncommitted, non-ignored changes. Run nigel in a dedicated worktree, never in a checkout with uncommitted work. Create one with [new-worktree](new-worktree.md), which brings the ignored inputs (baserom, toolchain, `.venv`, submodules) along, and shard candidates across worktrees:

```bash
tools/new-worktree mg64-a && cd ../mg64-a
MG64_SYNC_BRANCH=dev nigel decompile-similar --shard 1/3
```

Commits made in a worktree stay on its branch until they are integrated. `tools/integrate-shards mg64-a mg64-b …`, run in the main checkout on `dev`, cherry-picks each shard's commits that `git cherry` lists as missing and keeps each one only if `build-and-verify.sh` passes. It skips matches already integrated, and stops a shard at any commit that touches a path outside `src/`, `include/`, `docs/`, `symbol_addrs.txt` and `mariogolf64.yaml` (harness code, tasks, hooks), at a conflict it cannot resolve, or at a failed build, leaving that commit for review. It exits 1 when it stopped somewhere, so a loop such as `while tools/integrate-shards mg64-a mg64-b mg64-c mg64-d; do sleep 600; done` only needs a person when it ends. Conflicts in `src/**/*.c` go through the `tools/merge-decomp-c` merge driver (`.gitattributes`; configured by `tools/install-git-hooks.sh`). It resolves the usual conflict between neighbouring matches: a definition replaces the other side's `INCLUDE_ASM` of the same function, and both sides' `extern` declarations are combined without repeating earlier ones. It leaves a conflict for a person when one symbol is declared with two types, a function is defined two ways, or the block holds anything else.

With `MG64_SYNC_BRANCH` set, the decompile tasks' candidate source first runs `tools/sync-shard`, which rebases the shard onto that branch when nigel selects a candidate (not in the re-check after the agent, where `NIGEL_CANDIDATE` is set). Integrated commits drop out, and fixes merged from other shards (types, headers, harness) reach every shard. If the rebase conflicts, for example on a commit you picked with a hand-resolved conflict, it is retried preferring the integration branch's side. The result is kept only if every function the shard decompiled is still decompiled, every line it added outside `src/` and `include/` survives, and `build-and-verify.sh` passes; any other rebase that moves the shard must pass `build-and-verify.sh` too. Otherwise the shard keeps running unsynced.

# Task catalog

| Task | Candidate source | Does |
|---|---|---|
| `decompile-unmatched` | `difficulty_ledger.py harvest && score_functions.py asm/nonmatchings --json` | Tier 1. Starts from the top-3 similar matched functions (`score_functions.py --by-similarity --score-func`), then [decompile-function](/procedures/decompile-function.md) |
| `decompile-similar` | `difficulty_ledger.py harvest && score_functions.py ... --by-similarity --top-n 3` | Tier 1, 60 min. Same, with similar matched functions as hints |
| `decompile-coddog-similar` | `difficulty_ledger.py harvest && score_functions.py ... --by-coddog-similarity` | Tier 1, 60 min. Hints from coddog |
| `decompile-tier2` | `difficulty_ledger.py harvest && score_functions.py ... --by-similarity --top-n 5 --tier 2` | Tier 2, 120 min: five references, clean rewrite on plateau |
| `decompile-tier3` | `difficulty_ledger.py harvest && score_functions.py ... --by-similarity --top-n 5 --tier 3` | Tier 3, 240 min: try all strategies, do not give up |
| `cleanup-matched` | `list_matched_functions.py --by-reference-use` | [cleanup-function](/procedures/cleanup-function.md): names and purpose (`template.md`), most-referenced functions first |
| `fix-low-quality` | `detect_low_quality_matches.py src --json` | [cleanup-function](/procedures/cleanup-function.md): artefacts and types (`template.md`, candidates are `[file, function]`) |
| `cleanup-files` | `src/main/func_*.c`, `src/overlay_*/func_*.c` | Clean and rename a file with [rename-file](/procedures/rename-file.md) |
| `cleanup-structs`, `deduplicate-structs`, `remove-struct-unions` | struct/header finders | Consolidate and name structs |
| `clean-headers`, `introduce-headers` | header finders / source files | Header ownership and new headers |
| `consolidate-global-types`, `fix-symbol-footprints` | global-type / oversized-symbol finders | Fix global types and symbol sizes |
| `name-functions-and-symbols` | source files | Rename with [rename-global](/procedures/rename-global.md) and [rename-file](/procedures/rename-file.md) |
| `match-rodata-segments` | unnamed `rodata` entries in `mariogolf64.yaml` | [match-data-segment](/procedures/match-data-segment.md) |
| `consolidate-learnings` | `list_unconsolidated_learnings.py --json` | Merge verified KMC GCC lessons from `nonmatchings/*/LEARNINGS.md` and `NEAR_MISS.md` into `docs/reference/kmc-gcc/`, then `--mark` the note consolidated. Commits only `docs/reference/kmc-gcc` |

The candidate-source scripts are in [selection](selection-and-status.md) and [quality checkers](quality-checkers.md).

Decompile tasks escalate by [difficulty tier](difficulty-ledger.md): a function is offered to tier 2 or 3 only after failing the tier below. Every decompile task runs `--model openai-codex/gpt-6-luna:xhigh` (`agent_flags`); the budget and strategy change between tiers, the model does not.
