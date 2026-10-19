---
type: Tool
title: "Pipeline metrics report"
description: "tools/pipeline_metrics.py: nigel outcomes joined with omp session cost, certified attempts to match, the difficulty ledger and a progress trend."
tags: [metrics, nigel, workflow, status]
resource: ../../tools/pipeline_metrics.py
---

`.venv/bin/python3 tools/pipeline_metrics.py [--since WHEN] [--until WHEN] [--json] [--worktree PATH]... [--snapshot]`

Reports how the nigel decompile pipeline performs: how many candidates ran, how many matched, what each run cost in omp tokens and dollars, how many attempts a match took, what the difficulty ledger holds, and how project progress has moved. It reads existing records and writes nothing, except `--snapshot`. It does not decide matches; the certification rules are those of [nonmatching_match_log](/tools/selection-and-status.md) and `build.sh`.

# Inputs

Every input is reported as `present` or `absent` with its counts. A missing input is never an error; the report says what is missing and why.

| Input | Source | Written by |
|---|---|---|
| Candidate outcomes | `task-runner/*/outcomes.log` in each worktree (one JSON object per candidate) | nigel ([task runner](/tools/task-runner.md)) |
| Session transcripts | `~/.omp/agent/sessions/*/*.jsonl` (override via `SESSIONS_ROOT` in the module) | omp |
| Attempts to match | `nonmatchings/<env>/match_log.txt` in each worktree | `build.sh` |
| Difficulty ledger | `$MG64_LEDGER`, else `<git-common-dir>/mg64/difficulty.jsonl` | `tools/difficulty_ledger.py` record |
| Progress trend | `<git-common-dir>/mg64/progress.jsonl` | `--snapshot` |

The worktrees are every entry of `git worktree list`, or the paths given with `--worktree`. The ledger is read directly from the JSONL file, so the report does not depend on the ledger CLI.

# Time window

`--since WHEN` and `--until WHEN` limit the report to a window. Each `WHEN` is a `YYYY-MM-DD` date or an ISO 8601 datetime with an optional offset; a naive datetime is local time. A date is local midnight, and `--until DATE` includes that whole day. `--since` is inclusive and `--until` is exclusive. Outcomes filter on `ts_start`, ledger records on `ts`, and snapshots on `ts`, or on local midnight of `date` when `ts` is missing. Match logs are never filtered.

To compare before and after a commit, split the window at the commit time:

```
.venv/bin/python3 tools/pipeline_metrics.py --until "$(git log -1 --format=%cI <commit>)"
.venv/bin/python3 tools/pipeline_metrics.py --since "$(git log -1 --format=%cI <commit>)"
```

# Joins and metrics

- **Match** means `outcome == "FIXED"`. `FIXED_BUT_REVERTED`, `BEST_EFFORT`, `NOT_FIXED`, `BUILD_FAILED` and `no-changes` are non-matches. A timeout is recorded in `details` (`timeout - ...`), not as its own outcome.
- **Session join**: a run joins to a session by `omp_session_id`, matched against the header `id` or the file stem of a top-level session file. A run with a null or unknown id counts as `sessions_missing`; its cost is reported as `-`, never as zero.
- **Cost and tokens** of a run sum the assistant message `usage.cost.total` plus `model_usage` entries across the top-level session file and every nested `*.jsonl` under its `<stem>/` directory (subagents). Tokens use `totalTokens` when present, else input + output + cache read + cache write.
- **Model** is the most common `provider/model` among the session's assistant messages. Without a session, it is `--model X` from `agent_flags` with any `:thinking` suffix removed. Otherwise it is `unknown`.
- Rows are grouped by `(task, model)`, with an `ALL` totals row. `cost_per_run` averages over joined runs. `cost_per_match` divides all joined spend, including failed runs, by matches. `matches_per_hour` divides matches by the summed `duration_s`.
- **Per KB** normalises by matched code size. A match's size is that of the function its `candidate` names (the first element when the candidate is a JSON array). Sizes come from `asm/nonmatchings` stubs for unmatched functions and from the C bodies in `src/` for matched ones. A match with no known size counts as a match and adds no bytes; the text report notes how many. KB is 1024 bytes. `$/KB` divides the row's joined spend by KB, and `min/KB` divides its summed `duration_s`, in minutes, by KB. Both are `-` when the row has no matched bytes; `$/KB` also needs a joined session.

# Attempts to match

Each `match_log.txt` line is parsed by `parse_match_log_line`. An environment's attempts-to-match is the 1-based index of the first line at 100% that `certified_attempt_path` accepts; an uncertified 100% line does not count. Percentiles use nearest rank. The **recommended stall limit** is the p85 over certified matches, and it is advisory: `build.sh` keeps its default unless `MG64_STALL_LIMIT` or `.run.json` says otherwise. With no certified matches the recommendation is `absent`.

Match logs carry no timestamps, so the time window does not filter attempts.

# Ledger

Failed records are every record whose `outcome` is not `matched`. The report gives counts by outcome and by tier, and attempts percentiles for failed records. Records missing a string `outcome` are counted as malformed and skipped. The window filters by `ts`; see [Time window](#time-window).

# Progress trend

The progress figures come from the source tree, so a snapshot needs no build: matched functions are C functions in `src/`, unmatched ones are `asm/nonmatchings` stubs sized from their `nonmatching <name>, <size>` headers. The figures are matched and unmatched functions, matched and unmatched bytes, and `bytes_pct` = matched bytes / total bytes. Hand-written assembly and data are not counted; [progress](/tools/progress.md) measures a build in full. `--snapshot` appends one line `{date, ts, worktree, ...figures}` to `progress.jsonl`. Each row in the trend shows the change from the previous snapshot, computed over the full history; the time window then keeps the rows whose snapshot time falls inside it.

# Limits

- Malformed JSON lines are skipped and counted; unreadable files are treated as empty.
- The time window applies to outcomes, ledger records and snapshots. It never applies to match logs.
- Costs come from omp's own usage fields and are only as complete as those fields. Nested subagent transcripts are included.
- Each run that joins a session reads that session's files, so a large transcript dominates runtime.
- Changing how a match is counted, or how a certified attempt is read, changes what this report says. Treat such changes as [comparison changes](/harness/changing-the-harness.md). Outcome names are written by nigel; see the [task runner](/tools/task-runner.md).
