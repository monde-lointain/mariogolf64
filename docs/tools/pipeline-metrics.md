---
type: Tool
title: "Pipeline metrics report"
description: "tools/pipeline_metrics.py: nigel outcomes joined with omp session cost, certified attempts to match, the difficulty ledger and a progress trend."
tags: [metrics, nigel, workflow, status]
resource: ../../tools/pipeline_metrics.py
---

```
.venv/bin/python3 tools/pipeline_metrics.py [--since WHEN] [--until WHEN] [--worktree PATH]... [--json] [--snapshot]
```

Run with the project Python ([python environment](/project/setup-and-build.md#python-environment)). The report reads existing records and writes nothing, except `--snapshot`. It does not decide matches: certification is defined in [nonmatching_match_log.py](/tools/selection-and-status.md#nonmatching_match_logpy).

# Inputs

Each input is reported as `present` or `absent`, with its counts. A missing input is never an error.

| Input | Location | Written by |
|---|---|---|
| Candidate outcomes | `task-runner/*/outcomes.log` in each worktree, one JSON object per line | nigel, see [task runner](/tools/task-runner.md) |
| Session transcripts | `~/.omp/agent/sessions/*/*.jsonl` (`SESSIONS_ROOT` in the module) | omp |
| Attempts | `nonmatchings/*/match_log.txt` in each worktree | each environment's `build.sh` (`tools/decomp-env-template/build.sh`) |
| Difficulty ledger | `$MG64_LEDGER`, else `<git-common-dir>/mg64/difficulty.jsonl` | [difficulty ledger](/tools/difficulty-ledger.md) `record` and `harvest` |
| Progress snapshots | `<git-common-dir>/mg64/progress.jsonl` | `--snapshot` |

Worktrees are the `--worktree PATH` values (repeatable), or every entry of `git worktree list --porcelain` by default, falling back to the project root.

Each outcome line supplies `task` (default: the `task-runner/<task>/` directory name), `candidate`, `outcome`, `duration_s`, `omp_session_id`, `agent_flags` and `ts_start`.

# Time window

- `--since WHEN` is inclusive; `--until WHEN` is exclusive.
- `WHEN` is a `YYYY-MM-DD` date (local midnight) or an ISO 8601 datetime. An offset is honoured; a naive datetime is local time.
- `--until DATE` includes the whole day: the bound becomes the midnight after `DATE`.
- Filtered by: outcome `ts_start`, ledger record `ts`, snapshot `ts` (local midnight of `date` when `ts` is missing). A record with no usable timestamp is excluded when a bound is set.
- Not filtered: match logs, and session transcripts. A joined run's cost is its whole session.

To compare before and after a commit, split the window at the commit time:

```
.venv/bin/python3 tools/pipeline_metrics.py --until "$(git log -1 --format=%cI <commit>)"
.venv/bin/python3 tools/pipeline_metrics.py --since "$(git log -1 --format=%cI <commit>)"
```

# Joins and metrics

- **Match**: `outcome == "FIXED"` only. Any other outcome, such as `FIXED_BUT_REVERTED`, `NOT_FIXED` or `BUILD_FAILED`, is a non-match.
- **Session join**: `omp_session_id` is looked up among top-level session files, by header `id` or by file stem. Files without a valid header are not indexed. A run with no id, or an id that joins no session, counts in `sessions_missing` and adds no cost or tokens.
- **Cost and tokens**: a joined session is parsed once per invocation. It sums `usage.cost.total` and the token fields of every assistant message and every `model_usage` entry, across the top-level file and every `*.jsonl` nested under `<stem>/` (subagents). Tokens are `totalTokens` when present, else input + output + cacheRead + cacheWrite.
- **Model**: the most common `provider/model` among the session's assistant messages. With no joined session, or none in it, it is the `--model` value in `agent_flags` with a thinking suffix (`:off` to `:xhigh`) removed. Otherwise `unknown`.
- **Rows** are grouped by `(task, model)`, plus an `ALL` row (`totals`) over every run.

| JSON field | Definition |
|---|---|
| `match_rate` | matches / candidates |
| `cost_per_run_usd` | joined spend / joined runs |
| `cost_per_match_usd` | joined spend, failed runs included / matches |
| `matches_per_hour` | matches / summed `duration_s` of every candidate in the row, in hours (JSON only) |
| `cost_per_kb_usd` | joined spend / matched KB |
| `minutes_per_kb` | summed `duration_s` of every candidate in the row, in minutes / matched KB |

The text table shows `cost_per_run_usd`, `cost_per_match_usd`, `cost_per_kb_usd` and `minutes_per_kb`. A null value prints as `-`.

## Matched size

- A match's size is that of the function its `candidate` names (the first element when the candidate is a JSON array string).
- Sizes come from the project root, not from `--worktree` paths: unmatched `asm/nonmatchings` stubs, overridden by matched C functions in `src/`.
- A match with no size counts in `matches_unsized` and adds no bytes; the text report notes how many.
- KB is 1024 bytes. `$/KB` is `-` unless the row has matched bytes and a joined session. `min/KB` is `-` unless the row has matched bytes and durations.

# Attempts to match

- Each `match_log.txt` line is parsed by `parse_match_log_line`. Lines that do not parse are skipped, uncounted.
- **Attempts to match** is the 1-based index of the first line at 100% (`FULL_MATCH_PERCENT`) that `certified_attempt_path` accepts. An uncertified 100% line does not count.
- Each environment's function is its directory name before the first `-`.
- Percentiles (`n`, `p50`, `p85`, `max`) use nearest rank over matched environments.
- **Recommended stall limit** is the p85 of attempts to match over matched environments. It is advisory: the report changes no limit ([stall limit](/tools/difficulty-ledger.md#stall-limit) owns how `build.sh` picks one). With no certified match it is `absent` (`null` in JSON).
- The time window does not filter match logs.

# Ledger

- Records are read from the ledger file; malformed lines are counted in `inputs.ledger.malformed` and skipped.
- The window filters records by `ts`.
- `ledger.by_outcome` and `ledger.by_tier` count every windowed record. A record with no string `outcome`, or no `tier`, counts under `unknown`.
- A record is failed when its outcome is not `matched`, including `unknown`. `ledger.failed.attempts` gives percentiles of the `attempts` field over failed records that have one; `attempts_total` is their sum.

# Progress trend

- `--snapshot` runs before the report, so the new snapshot appears in the trend. It appends `{date, ts, worktree, matched_functions, unmatched_functions, matched_bytes, unmatched_bytes, bytes_pct}` to `progress.jsonl`. `worktree` is the project root. Outside git it exits 1.
- Figures come from the source tree, so no build is needed:
  - matched: C functions defined in `src/`;
  - unmatched: `asm/nonmatchings` stubs whose name is not matched. A stub's size is the `nonmatching <name>, <size>` header directly above its `glabel`, else 4 × its instruction count;
  - `bytes_pct`: matched bytes / all bytes × 100, rounded to four decimals.
- Hand-written assembly and data are not counted. [progress](/tools/progress.md) measures a build in full.
- Each trend row carries `delta_matched_functions` and `delta_bytes_pct` against the previous snapshot in the full history. The window then keeps rows whose moment (`ts`, else local midnight of `date`) falls in it. The text shows the last 20 (`TREND_ROWS`).
- Snapshot records without a valid `date` are skipped.

# Output and exit codes

- Text by default. `--json` prints the whole report with sorted keys. Top-level keys: `since`, `until`, `worktrees`, `inputs`, `totals`, `tasks`, `attempts_to_match`, `ledger`, `progress`.
- `0`: report printed.
- `1`: `--snapshot` outside a git repository, so `progress.jsonl` cannot be located.
- `2`: a `--since` or `--until` value that is neither a date nor an ISO datetime (argparse).

# Limits

- Malformed JSON lines are skipped and counted; unreadable files are treated as empty. Outcome records without a string `outcome` are counted as malformed and skipped. Unlike ledger records, they do not appear under `unknown`.
- Costs come from omp's usage fields and are only as complete as those fields.
- Each session file is parsed once per invocation, so large transcripts dominate runtime.
- Session transcripts are not windowed (see [Time window](#time-window)).
- Changing how a match is counted, or how a certified attempt is read, changes these figures. Treat such a change as a comparison change; see the [code map](/harness/code-map.md). Outcome names are written by nigel; see the [task runner](/tools/task-runner.md).
