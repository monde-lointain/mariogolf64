---
type: Tool
title: "Difficulty ledger and tiers"
description: "Shared record of each decompilation attempt's outcome, the difficulty tier that follows from a function's failure history, and the per-environment .run.json that sets the stall limit."
tags: [nigel, automation, workflow]
resource: ../../tools/difficulty_ledger.py
---

Each finished decompile attempt adds one JSON line to a shared ledger. A function's tier follows from its failure history, which is how nigel escalates by difficulty.

# Location

`$MG64_LEDGER` if set, else `<git common dir>/mg64/difficulty.jsonl`. The common dir is shared by every worktree and sits under `.git`, so the reset between nigel attempts leaves it alone.

# Records

| Field | Meaning |
|---|---|
| `ts` | UTC time, ISO 8601, to the second |
| `function`, `env`, `worktree` | function name, environment path relative to the repo, repo root |
| `task`, `tier`, `model`, `run_id` | nigel task, tier (`1`, `2`, `3` or `human`), model from the agent flags, nigel run id |
| `attempts`, `best_pct` | attempts and best certified percent in the environment's `match_log.txt` |
| `outcome` | `matched`, `near_miss`, `stalled`, `timeout`, `integration_failed` or `gave_up` |
| `reason` | one line |

## Counting records

- The latest record for a (function, worktree, env) is the last one in file order. `ts` does not decide it.
- A `matched` record for a function, in any worktree or environment, clears all of that function's failure history. Failure records for it are ignored wherever they sit in the file.

# Tiers

| Tier | Given to | Budget and strategy | Served by |
|---|---|---|---|
| 1 | no failure | 60 min, stall limit 7 | [`decompile-similar`, `decompile-coddog-similar`](/tools/task-runner.md#task-catalog) |
| 2 | a tier-1 failure | 120 min, stall limit 12; rewrite from the assembly when progress plateaus | `decompile-tier2` |
| 3 | a tier-2 failure with a best of at least 90% | 240 min, stall limit 40; try [matching-method](/procedures/matching-method.md) approaches that earlier attempts did not | `decompile-tier3` |
| `human` | a tier-3 failure, a tier-2 failure below 90%, or any `integration_failed` | no nigel task; stall limit 20 (the build.sh default) | none: `show --tier human` lists them |

`tier_from_failures` sets the tier from a function's failure records, first rule that applies:

- any `integration_failed`: `human`. The C already matches; what blocks it is rodata or data ownership, which a larger matching budget does not fix.
- none: tier 1.
- highest is 1: tier 2.
- highest is 2: tier 3 if the best percent among its tier-2 failures is at least 90 (`TIER3_MIN_BEST_PCT`), else `human`.
- highest is 3 or `human`: `human`.

A `NEAR_MISS.md` in an environment with no ledger record counts as an implicit `near_miss` failure. Its tier comes from that environment's `.run.json` (1 if absent), and its attempts and best come from `match_log.txt`. Its `ts` is the note's modification time. `show` and tier selection both count it. It is not written to the file until `harvest` records it.

Later tiers' briefs list up to five of the function's newest ledger records under `<prior_attempts>`. The ledger holds no strategy text; the strategy lives in each task's `task.yaml` (see [task-runner](/tools/task-runner.md)).

# Commands

```bash
python3 tools/difficulty_ledger.py record <function> --outcome <outcome> --reason "<one line>" [--env <dir>]
python3 tools/difficulty_ledger.py harvest
python3 tools/difficulty_ledger.py show [--tier 1|2|3|human] [--json]
python3 tools/difficulty_ledger.py write-run <env> <function>
```

## `record`

- `--outcome` (required): one of the six outcomes above. Agents record only `integration_failed` and `gave_up` (see `decomp-env-template/AGENTS.md`); `harvest` writes the other four.
- `--reason` (required): one non-empty line.
- `--env <dir>`: default is the cwd if it holds `target.s`; else the newest (by mtime) `nonmatchings/<function>` or `nonmatchings/<function>-*` directory holding `target.s`. No match: exit 1.
- Attempts and best come from the environment's `match_log.txt`. `matched` forces `best_pct` to 100.0, because build.sh does not log the base.c baseline.
- Task, run id and model come from `NIGEL_TASK_NAME`, `NIGEL_RUN_ID` and `NIGEL_AGENT_FLAGS`, else from `<env>/.run.json`. The `.run.json` must name the same function.
- Tier: `TASK_TIERS` for the task; else the tier in `.run.json`; else the function's tier from the ledger.
- Stdout: `recorded <function> tier=<t> outcome=<o> attempts=<n> best=<x.xxx>`.

## `harvest`

Records outcomes for environments whose nigel run has no ledger record yet. It considers only environments under `nonmatchings/` whose `.run.json` has a `run_id`, and skips any (function, worktree, env) that already has a record. The function is `.run.json`'s `function`, else the directory name.

The first matching rule decides the outcome:

| Condition | Outcome |
|---|---|
| function is in `src/` | `matched` |
| `NEAR_MISS.md` present | `near_miss` once `deadline_unix` has passed; until then the environment is skipped |
| `match_log.txt` reached `stall_limit` | `stalled`, whatever the deadline |
| `deadline_unix` has passed | `timeout` |
| none | skipped |

Each record is written with the `.run.json` values only. Stderr gets `harvested <function> <outcome> <env>` per record; stdout stays empty. Exit 1 on an unreadable ledger or run file.

Every decompile task's `candidate_source` runs `harvest` first, so an unreadable ledger fails the task (see [task-runner](/tools/task-runner.md)).

## `show`

One row per function with failure history, from ledger records plus implicit near-miss records, sorted by function name:

| Field | Value |
|---|---|
| `function` | name |
| `tier` | integer or `human` |
| `failures` | number of its latest failure records (one per environment) |
| `best_pct` | highest `best_pct` among them |
| `last_outcome`, `last_reason` | from the newest failure by `ts` |
| `last_env` | `--json` only |

Text output is tab-separated with the header `function	tier	failures	best_pct	last_outcome	last_reason`. `--json` prints a list of objects with the keys above.

`--tier 1` always prints nothing: every failure yields tier 2 or higher. The command then writes a note to stderr pointing to `score_functions.py --tier 1`.

## `write-run`

Writes `<env>/.run.json` and prints nothing. `decomp-env` calls it when it creates an environment (`tools/decomp-env`). If the call fails, `decomp-env` prints a warning and build.sh falls back to its default stall limit.

# `.run.json`

| Key | Value |
|---|---|
| `function` | function name |
| `task`, `run_id` | `NIGEL_TASK_NAME` and `NIGEL_RUN_ID`; `null` outside nigel |
| `tier` | `TASK_TIERS` for the task; else the function's ledger tier |
| `model` | the `--model` value in `NIGEL_AGENT_FLAGS`; `null` if absent |
| `started` | UTC ISO 8601 time written |
| `deadline_unix` | `NIGEL_TIMEOUT_DEADLINE_UNIX` if set; else now plus the tier budget when `run_id` is set and the tier has a budget; else `null` |
| `stall_limit` | the tier's stall limit; `null` for `human` |

# Stall limit

build.sh takes the limit from, in order:

1. `MG64_STALL_LIMIT`, which must be a positive integer (else build.sh exits 1).
2. `stall_limit` in `.run.json` (`null` falls through).
3. 20.

build.sh stops the run once that many attempts have passed without a strictly higher best percent.

# Selectors

- `score_functions.py --tier 1|2|3` keeps one tier. Its default is tier 1, so the decompile candidate sources skip failed functions.
- `list_decomp_candidates.py` skips functions above tier 1 unless `--include-failed` is given. Each row shows its tier. Details are in [selection and status](/tools/selection-and-status.md).
