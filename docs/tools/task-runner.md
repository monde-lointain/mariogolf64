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
| `verify_command` | `./tools/build-and-verify.sh` |
| `success_command` | `git add -A && git commit -m "task-runner($TASK_NAME): fix $CANDIDATE"` (tasks override it) |
| `reset_command` | `git clean -f; git reset --hard HEAD || true` |

`task-runner/<task>/task.yaml` keys: `candidate_source`, `prompt` or `template`, `agent_flags`, `accept_best_effort`, `timeout`, `success_command`, `verify_command`. `verify_command` overrides the config's and takes the same `$CANDIDATE` and `$TASK_NAME` variables as `success_command`. Prompt variables: `$INPUT`, `$INPUT[n]`, `$INPUT[n:]`, `$INPUT["key"]` (index and slice need an array candidate, else interpolation fails) and `$TASK_ID`. nigel exports `NIGEL_TASK_NAME`, `NIGEL_CANDIDATE`, `NIGEL_RUN_ID`, `NIGEL_AGENT` and `NIGEL_AGENT_FLAGS` to the agent and to the verify, success and reset commands. With a `timeout`, nigel exports `NIGEL_TIMEOUT_SECONDS` and `NIGEL_TIMEOUT_DEADLINE_UNIX`, which the [policy hook](/harness/policy-hook.md) uses to bound permuter runs.

# Worktree rule

`reset_command` runs at startup and after every failed candidate and wipes uncommitted, non-ignored changes. Run nigel in a dedicated worktree, never in a checkout with uncommitted work. Create one with [new-worktree](new-worktree.md), which brings the ignored inputs (baserom, toolchain, `.venv`, submodules) along, and shard candidates across worktrees:

```bash
tools/new-worktree mg64-a && cd ../mg64-a
nigel decompile-similar --shard 1/3
```

Commits made in a worktree stay on its branch; review and merge `harness:` commits from there (`git log --oneline --grep '^harness:'`).

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
| `split-asm-segments` | `asm` entries in `mariogolf64.yaml` | Run [split-asm-segments](split-asm-segments.md) on the offset; fix the boundary by hand only if it prints FAILED |
| `consolidate-learnings` | `list_unconsolidated_learnings.py --json` | Merge verified KMC GCC lessons from `nonmatchings/*/LEARNINGS.md` and `NEAR_MISS.md` into `docs/reference/kmc-gcc/`, then `--mark` the note consolidated. Commits only `docs/reference/kmc-gcc` |

The candidate-source scripts are in [selection](selection-and-status.md) and [quality checkers](quality-checkers.md).

Decompile tasks escalate by [difficulty tier](difficulty-ledger.md): a function is offered to tier 2 or 3 only after failing the tier below. Every decompile task runs `--model anthropic/claude-haiku-5-5:xhigh` (`agent_flags`); the budget and strategy change between tiers, the model does not.
