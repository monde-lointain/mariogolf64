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

`task-runner/<task>/task.yaml` keys: `candidate_source`, `prompt` or `template`, `agent_flags`, `accept_best_effort`, `timeout`, `success_command`. Prompt variables: `$INPUT`, `$INPUT[n]`, `$INPUT[n:]`, `$INPUT["key"]` (index and slice need an array candidate, else interpolation fails) and `$TASK_ID`. With a `timeout`, nigel exports `NIGEL_TIMEOUT_SECONDS` and `NIGEL_TIMEOUT_DEADLINE_UNIX`, which the [policy hook](/harness/policy-hook.md) uses to bound permuter runs.

# Worktree rule

`reset_command` runs at startup and after every failed candidate and wipes uncommitted, non-ignored changes. Run nigel in a dedicated worktree, never in a checkout with uncommitted work. A new worktree needs its own ignored inputs:

```bash
git worktree add ../mg64-a && cd ../mg64-a
git submodule update --init
make setup
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
cp <main checkout>/baserom.z64 .
```

Commits made in a worktree stay on its branch; review and merge `harness:` commits from there (`git log --oneline --grep '^harness:'`).

# Task catalog

| Task | Candidate source | Does |
|---|---|---|
| `decompile-unmatched` | `score_functions.py asm/nonmatchings --json` | Match one function with [decompile-function](/procedures/decompile-function.md) |
| `decompile-similar` | `score_functions.py ... --by-similarity --top-n 3` | Same, with similar matched functions as hints |
| `decompile-coddog-similar` | `score_functions.py ... --by-coddog-similarity` | Same, hints from coddog |
| `cleanup-matched` | `list_matched_functions.py` | [cleanup-function](/procedures/cleanup-function.md): names and purpose (`template.md`) |
| `fix-low-quality` | `detect_low_quality_matches.py src --json` | [cleanup-function](/procedures/cleanup-function.md): artefacts and types (`template.md`, candidates are `[file, function]`) |
| `cleanup-files` | `src/main/func_*.c`, `src/overlay_*/func_*.c` | Clean and rename a file with [rename-file](/procedures/rename-file.md) |
| `cleanup-structs`, `deduplicate-structs`, `remove-struct-unions` | struct/header finders | Consolidate and name structs |
| `clean-headers`, `introduce-headers` | header finders / source files | Header ownership and new headers |
| `consolidate-global-types`, `fix-symbol-footprints` | global-type / oversized-symbol finders | Fix global types and symbol sizes |
| `name-functions-and-symbols` | source files | Rename with [rename-global](/procedures/rename-global.md) and [rename-file](/procedures/rename-file.md) |
| `match-rodata-segments` | unnamed `rodata` entries in `mariogolf64.yaml` | [match-data-segment](/procedures/match-data-segment.md) |
| `split-asm-segments` | `asm` entries in `mariogolf64.yaml` | Convert an assembly-only segment to a C file of `INCLUDE_ASM` stubs |

The candidate-source scripts are in [selection](selection-and-status.md) and [quality checkers](quality-checkers.md).
