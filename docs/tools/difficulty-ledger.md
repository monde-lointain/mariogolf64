---
type: Tool
title: "Difficulty ledger and tiers"
description: "Shared record of each decompilation attempt's outcome, the difficulty tier that follows from a function's failure history, and the per-environment .run.json that sets the stall limit."
tags: [nigel, automation, workflow]
resource: ../../tools/difficulty_ledger.py
---

Each decompile environment that ends without a match leaves one JSON line in a shared ledger. A function's tier follows from its failure history, so nigel escalates by difficulty: a function reaches tier 2 only after failing tier 1, and tier 3 only after failing tier 2 with a best match of at least 90%.

# Location

`$MG64_LEDGER` if set, else `<git common dir>/mg64/difficulty.jsonl`. The common dir is shared by every worktree and sits under `.git`, so nigel's `reset_command` does not touch it.

# Records

| Field | Meaning |
|---|---|
| `ts` | UTC time, ISO 8601 |
| `function`, `env`, `worktree` | function name, environment path relative to the repo, repo root |
| `task`, `tier`, `model`, `run_id` | nigel task, its tier (1, 2, 3 or `human`), model from the agent flags, nigel run id |
| `attempts`, `best_pct` | attempts and best certified percent in the environment's `match_log.txt` |
| `outcome` | `matched`, `near_miss`, `stalled`, `timeout`, `integration_failed` or `gave_up` |
| `reason` | one line |

Only the latest record per (function, worktree, env) counts. A `matched` record clears that function's failure history.

# Tiers

| Tier | Given to | Budget and strategy | Served by |
|---|---|---|---|
| 1 | no failed attempt | 60 min, stall limit 7, top-3 similar references | `decompile-similar`, `decompile-unmatched`, `decompile-coddog-similar` |
| 2 | failed tier 1 | 120 min, stall limit 12, top-5 references, clean rewrite on plateau | `decompile-tier2` |
| 3 | failed tier 2 with best of at least 90% | 240 min, stall limit 40, top-5 references, try all strategies | `decompile-tier3` |
| `human` | failed tier 3, or failed tier 2 below 90% | no nigel task; stall limit 20 (the build.sh default) | none: `show --tier human` lists them |

A `NEAR_MISS.md` note in an environment with no ledger record counts as a failure, at the tier its `.run.json` gives (default 1). Top-n and the strategy text live in each task's `task.yaml`, not in the ledger.

# Commands

```bash
python3 tools/difficulty_ledger.py record <function> --outcome <outcome> --reason "<one line>" [--env <dir>]
python3 tools/difficulty_ledger.py harvest
python3 tools/difficulty_ledger.py show [--tier 1|2|3|human] [--json]
python3 tools/difficulty_ledger.py write-run <env> <function>
```

- `record` (run from the repo root, or from the environment directory). `--env <dir>` names the environment. Without it, the environment is the cwd if it holds `target.s`, else the newest `nonmatchings/<function>[-*]` directory holding `target.s`. It reads attempts and best percent from the environment's `match_log.txt`, and task, tier, model and run id from the `NIGEL_*` variables (`NIGEL_TASK_NAME`, `NIGEL_RUN_ID`, `NIGEL_AGENT_FLAGS`) or else from `<env>/.run.json`.
- `harvest` back-fills records for environments whose run finished without one. A function now in `src/` is recorded as `matched`. A run whose `match_log.txt` reached its stall limit is recorded as `stalled`. A run past its deadline is recorded as `near_miss` if it has a `NEAR_MISS.md` note, else as `timeout`. Other runs inside their deadline are skipped. Agents therefore record only `gave_up` and `integration_failed`. Progress goes to stderr; stdout stays clean for the candidate lists. Every decompile task runs `harvest` in its candidate source, so an unreadable ledger fails the task.
- `show` lists functions with failure history: tier, failures, best percent, last outcome and reason.
- `write-run` writes `<env>/.run.json`. decomp-env calls it when it creates an environment.

## `.run.json`

| Key | Value |
|---|---|
| `function`, `task`, `tier`, `model`, `run_id` | as in the record |
| `started`, `deadline_unix` | the run deadline: `NIGEL_TIMEOUT_DEADLINE_UNIX`, else now plus the tier budget; `null` outside nigel |
| `stall_limit` | stall limit for the tier; `null` for `human` (build.sh then uses 20) |

build.sh reads `MG64_STALL_LIMIT`, then `stall_limit` here, then 20.

# Selectors

- `score_functions.py --tier 1|2|3` keeps one tier. The default is tier 1, so the candidate sources of the decompile tasks skip failed functions.
- `list_decomp_candidates.py` lists tier 1 only, unless `--include-failed` is given. Each row shows the tier.
