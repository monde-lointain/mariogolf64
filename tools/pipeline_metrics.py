#!/usr/bin/env python3
"""Report nigel pipeline metrics for the Mario Golf 64 decompilation.

Inputs (each reported as present or absent; a missing input is never an error):
  task-runner/*/outcomes.log          nigel outcomes, one JSON object per candidate (nigel README, "outcomes.log")
  ~/.omp/agent/sessions/*/*.jsonl     omp session transcripts, joined by omp_session_id; nested
                                      subagent transcripts under <session>/ are summed in
  nonmatchings/*/match_log.txt        per-environment attempts (certified full matches)
  $MG64_LEDGER or <common-dir>/mg64/difficulty.jsonl   difficulty ledger (tools/difficulty_ledger.py)
  <common-dir>/mg64/progress.jsonl    project progress snapshots written by --snapshot

Progress figures come from the source tree (matched C functions in src/, unmatched
asm/nonmatchings stubs), so a snapshot needs no build. tools/progress.py measures a
build in full, data included.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import statistics
import subprocess
import sys
from collections import Counter
from dataclasses import dataclass, field
from datetime import date, datetime
from pathlib import Path
from typing import Any, Mapping, Sequence

sys.path.insert(0, str(Path(__file__).resolve().parent))

from find_similar_functions import build_function_index, get_project_root, load_unmatched  # noqa: E402
from nonmatching_match_log import (  # noqa: E402
    FULL_MATCH_PERCENT,
    base_workspace_name,
    certified_attempt_path,
    parse_match_log_line,
)

MATCH_OUTCOMES = frozenset({"FIXED"})
SESSIONS_ROOT = Path.home() / ".omp" / "agent" / "sessions"
PROGRESS_DIR = "mg64"
PROGRESS_NAME = "progress.jsonl"
LEDGER_NAME = "difficulty.jsonl"
STALL_PERCENTILE = 85
TREND_ROWS = 20

MODEL_FLAG_RE = re.compile(r"--model[= ]+(\S+)")
THINKING_SUFFIX_RE = re.compile(r":(?:off|minimal|low|medium|high|xhigh)$")


@dataclass(frozen=True)
class CandidateRun:
    worktree: str
    task: str
    candidate: str
    outcome: str
    duration_s: float | None
    agent_flags: str
    omp_session_id: str | None


@dataclass
class SessionUsage:
    cost_usd: float = 0.0
    tokens: int = 0
    models: Counter = field(default_factory=Counter)
    files: int = 0


@dataclass(frozen=True)
class EnvResult:
    env: str
    function: str
    attempts_to_match: int | None
    total_attempts: int


@dataclass
class _Group:
    candidates: int = 0
    outcomes: Counter = field(default_factory=Counter)
    matches: int = 0
    durations: list[float] = field(default_factory=list)
    sessions_joined: int = 0
    sessions_missing: int = 0
    cost_usd: float = 0.0
    tokens: int = 0

    def add(self, run: CandidateRun, usage: SessionUsage | None) -> None:
        self.candidates += 1
        self.outcomes[run.outcome] += 1
        if run.outcome in MATCH_OUTCOMES:
            self.matches += 1
        if run.duration_s is not None:
            self.durations.append(run.duration_s)
        if usage is None:
            self.sessions_missing += 1
        else:
            self.sessions_joined += 1
            self.cost_usd += usage.cost_usd
            self.tokens += usage.tokens


def _number(value: Any) -> float | None:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    return float(value)


def _int(value: Any) -> int | None:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    return int(value)


def parse_ts(value: Any) -> datetime | None:
    if not isinstance(value, str) or not value:
        return None
    text = value[:-1] + "+00:00" if value.endswith("Z") else value
    try:
        return datetime.fromisoformat(text)
    except ValueError:
        return None


def _on_or_after(ts: datetime | None, since: date | None) -> bool:
    if since is None:
        return True
    return ts is not None and ts.date() >= since


def _record_day(record: Mapping[str, Any]) -> date | None:
    value = record.get("date")
    if not isinstance(value, str):
        return None
    try:
        return date.fromisoformat(value)
    except ValueError:
        return None


def read_jsonl(path: Path) -> tuple[list[dict[str, Any]], int]:
    """Return JSON object lines and the count of skipped lines. Unreadable files are empty."""
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return [], 0
    records: list[dict[str, Any]] = []
    malformed = 0
    for line in text.splitlines():
        if not line.strip():
            continue
        try:
            value = json.loads(line)
        except json.JSONDecodeError:
            malformed += 1
            continue
        if isinstance(value, dict):
            records.append(value)
        else:
            malformed += 1
    return records, malformed


def read_candidate_runs(worktree: Path, since: date | None) -> tuple[list[CandidateRun], int, int]:
    runs: list[CandidateRun] = []
    files = 0
    malformed = 0
    for log in sorted((worktree / "task-runner").glob("*/outcomes.log")):
        files += 1
        records, bad = read_jsonl(log)
        malformed += bad
        for record in records:
            outcome = record.get("outcome")
            if not isinstance(outcome, str):
                malformed += 1
                continue
            if not _on_or_after(parse_ts(record.get("ts_start")), since):
                continue
            session_id = record.get("omp_session_id")
            runs.append(CandidateRun(
                worktree=str(worktree),
                task=str(record.get("task") or log.parent.name),
                candidate=str(record.get("candidate") or ""),
                outcome=outcome,
                duration_s=_number(record.get("duration_s")),
                agent_flags=str(record.get("agent_flags") or ""),
                omp_session_id=session_id if isinstance(session_id, str) and session_id else None,
            ))
    return runs, files, malformed


def _header_id(path: Path) -> str | None:
    try:
        with path.open("r", encoding="utf-8", errors="replace") as handle:
            for _, line in zip(range(5), handle):
                try:
                    value = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if isinstance(value, dict) and value.get("type") == "session":
                    session_id = value.get("id")
                    return session_id if isinstance(session_id, str) and session_id else None
    except OSError:
        return None
    return None


def index_sessions(sessions_root: Path) -> dict[str, Path]:
    """Map header session ids and file stems to top-level session files that have a header."""
    index: dict[str, Path] = {}
    if not sessions_root.is_dir():
        return index
    for path in sorted(sessions_root.glob("*/*.jsonl")):
        header_id = _header_id(path)
        if header_id is None:
            continue
        index.setdefault(header_id, path)
        index.setdefault(path.stem, path)
    return index


def _add_usage(usage: SessionUsage, raw: Any) -> None:
    if not isinstance(raw, dict):
        return
    cost = raw.get("cost")
    total = _number(cost.get("total")) if isinstance(cost, dict) else None
    if total is not None:
        usage.cost_usd += total
    tokens = _int(raw.get("totalTokens"))
    if tokens is None:
        tokens = sum(_int(raw.get(key)) or 0 for key in ("input", "output", "cacheRead", "cacheWrite"))
    usage.tokens += tokens


def _model_key(message: Mapping[str, Any]) -> str | None:
    model = message.get("model")
    if not isinstance(model, str) or not model:
        return None
    provider = message.get("provider")
    if isinstance(provider, str) and provider and not model.startswith(f"{provider}/"):
        return f"{provider}/{model}"
    return model


def session_usage(path: Path) -> SessionUsage:
    """Sum cost and tokens over a session file and its nested subagent transcripts."""
    usage = SessionUsage()
    stem_dir = path.parent / path.stem
    nested = sorted(stem_dir.rglob("*.jsonl")) if stem_dir.is_dir() else []
    for file in [path, *nested]:
        usage.files += 1
        records, _ = read_jsonl(file)
        for entry in records:
            kind = entry.get("type")
            if kind == "message":
                message = entry.get("message")
                if not isinstance(message, dict) or message.get("role") != "assistant":
                    continue
                _add_usage(usage, message.get("usage"))
                key = _model_key(message)
                if key is not None:
                    usage.models[key] += 1
            elif kind == "model_usage":
                _add_usage(usage, entry.get("usage"))
    return usage


def _model_from_flags(flags: str) -> str | None:
    match = MODEL_FLAG_RE.search(flags)
    if match is None:
        return None
    return THINKING_SUFFIX_RE.sub("", match.group(1))


def run_model(usage: SessionUsage | None, flags: str) -> str:
    if usage is not None and usage.models:
        return usage.models.most_common(1)[0][0]
    return _model_from_flags(flags) or "unknown"


def nearest_rank(values: Sequence[float], pct: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    rank = max(1, math.ceil(pct * len(ordered) / 100))
    return ordered[min(rank, len(ordered)) - 1]


def _percentiles(values: Sequence[int]) -> dict[str, Any]:
    return {
        "n": len(values),
        "p50": nearest_rank(values, 50),
        "p85": nearest_rank(values, STALL_PERCENTILE),
        "max": max(values) if values else None,
    }


def scan_environments(worktree: Path) -> list[EnvResult]:
    results: list[EnvResult] = []
    for log in sorted((worktree / "nonmatchings").glob("*/match_log.txt")):
        env = log.parent
        try:
            lines = log.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            lines = []
        attempts = 0
        to_match: int | None = None
        for line in lines:
            entry = parse_match_log_line(line)
            if entry is None:
                continue
            attempts += 1
            if (to_match is None and entry.percent >= FULL_MATCH_PERCENT
                    and certified_attempt_path(entry, env) is not None):
                to_match = attempts
        results.append(EnvResult(env=env.name, function=base_workspace_name(env),
                                 attempts_to_match=to_match, total_attempts=attempts))
    return results


def ledger_path(common_dir: Path | None, environ: Mapping[str, str] | None = None) -> Path | None:
    env = os.environ if environ is None else environ
    override = env.get("MG64_LEDGER")
    if override:
        return Path(override)
    if common_dir is None:
        return None
    return common_dir / PROGRESS_DIR / LEDGER_NAME


def _git(cwd: Path, *args: str) -> str | None:
    try:
        result = subprocess.run(["git", "-C", str(cwd), *args], capture_output=True, text=True, check=False)
    except OSError:
        return None
    if result.returncode != 0:
        return None
    return result.stdout


def git_common_dir(root: Path) -> Path | None:
    out = _git(root, "rev-parse", "--path-format=absolute", "--git-common-dir")
    if not out or not out.strip():
        return None
    return Path(out.strip())


def git_worktrees(root: Path) -> list[Path]:
    out = _git(root, "worktree", "list", "--porcelain")
    if out is None:
        return [root]
    paths = [Path(line[len("worktree "):]) for line in out.splitlines() if line.startswith("worktree ")]
    return paths or [root]


def _ledger_stats(records: Sequence[Mapping[str, Any]]) -> dict[str, Any]:
    by_outcome: Counter = Counter()
    by_tier: Counter = Counter()
    failed_records = 0
    failed_attempts: list[int] = []
    for record in records:
        outcome = record.get("outcome")
        key = outcome if isinstance(outcome, str) else "unknown"
        by_outcome[key] += 1
        tier = record.get("tier")
        by_tier[str(tier) if tier is not None else "unknown"] += 1
        if key != "matched":
            failed_records += 1
            attempts = _int(record.get("attempts"))
            if attempts is not None:
                failed_attempts.append(attempts)
    return {
        "records": len(records),
        "by_outcome": dict(sorted(by_outcome.items())),
        "by_tier": dict(sorted(by_tier.items())),
        "failed": {
            "records": failed_records,
            "attempts_total": sum(failed_attempts),
            "attempts": _percentiles(failed_attempts),
        },
    }


def read_progress(progress_file: Path, since: date | None = None) -> list[dict[str, Any]]:
    """Snapshot records in file order; since keeps records dated on or after it."""
    if not progress_file.is_file():
        return []
    records, _ = read_jsonl(progress_file)
    kept = []
    for record in records:
        day = _record_day(record)
        if day is None or (since is not None and day < since):
            continue
        kept.append(record)
    return kept


def _diff(current: Mapping[str, Any], previous: Mapping[str, Any] | None, key: str) -> float | None:
    if previous is None:
        return None
    now, before = _number(current.get(key)), _number(previous.get(key))
    if now is None or before is None:
        return None
    return now - before


def _with_trend(history: Sequence[Mapping[str, Any]]) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    previous: Mapping[str, Any] | None = None
    for record in history:
        row = dict(record)
        row["delta_matched_functions"] = _diff(record, previous, "matched_functions")
        row["delta_bytes_pct"] = _diff(record, previous, "bytes_pct")
        rows.append(row)
        previous = record
    return rows


def _row(task: str, model: str, group: _Group) -> dict[str, Any]:
    wall_s = sum(group.durations)
    wall_hours = wall_s / 3600 if wall_s else None
    joined = group.sessions_joined
    return {
        "task": task,
        "model": model,
        "candidates": group.candidates,
        "outcomes": dict(sorted(group.outcomes.items())),
        "matches": group.matches,
        "match_rate": group.matches / group.candidates if group.candidates else None,
        "wall_hours": wall_hours,
        "matches_per_hour": group.matches / wall_hours if wall_hours else None,
        "duration_mean_s": statistics.fmean(group.durations) if group.durations else None,
        "duration_median_s": statistics.median(group.durations) if group.durations else None,
        "sessions_joined": joined,
        "sessions_missing": group.sessions_missing,
        "cost_usd": group.cost_usd if joined else None,
        "cost_per_run_usd": group.cost_usd / joined if joined else None,
        "cost_per_match_usd": group.cost_usd / group.matches if joined and group.matches else None,
        "tokens": group.tokens if joined else None,
        "tokens_per_run": group.tokens / joined if joined else None,
        "tokens_per_match": group.tokens / group.matches if joined and group.matches else None,
    }


def build_report(worktrees: Sequence[Path], *, sessions_root: Path, ledger_file: Path | None,
                 progress_file: Path | None, since: date | None = None) -> dict[str, Any]:
    runs: list[CandidateRun] = []
    outcome_files = 0
    outcome_malformed = 0
    env_results: list[EnvResult] = []
    for worktree in worktrees:
        worktree_runs, files, bad = read_candidate_runs(worktree, since)
        runs.extend(worktree_runs)
        outcome_files += files
        outcome_malformed += bad
        env_results.extend(scan_environments(worktree))

    sessions = index_sessions(sessions_root)
    usage_cache: dict[Path, SessionUsage] = {}
    groups: dict[tuple[str, str], _Group] = {}
    total = _Group()
    for run in runs:
        usage: SessionUsage | None = None
        path = sessions.get(run.omp_session_id) if run.omp_session_id else None
        if path is not None:
            if path not in usage_cache:
                usage_cache[path] = session_usage(path)
            usage = usage_cache[path]
        model = run_model(usage, run.agent_flags)
        groups.setdefault((run.task, model), _Group()).add(run, usage)
        total.add(run, usage)

    joined = total.sessions_joined
    missing = total.sessions_missing

    ledger_info: dict[str, Any] = {
        "status": "absent",
        "path": str(ledger_file) if ledger_file is not None else None,
        "records": 0,
        "malformed": 0,
    }
    ledger_records: list[dict[str, Any]] = []
    if ledger_file is not None and ledger_file.is_file():
        raw, bad = read_jsonl(ledger_file)
        ledger_info.update(status="present", malformed=bad)
        ledger_records = [r for r in raw if _on_or_after(parse_ts(r.get("ts")), since)]
        ledger_info["records"] = len(ledger_records)

    matched_envs = [r for r in env_results if r.attempts_to_match is not None]
    attempts_values = [r.attempts_to_match for r in matched_envs if r.attempts_to_match is not None]
    attempts = _percentiles(attempts_values)
    attempts["stall_limit_recommendation"] = attempts["p85"]
    attempts["matched_envs"] = [
        {"env": r.env, "function": r.function, "attempts_to_match": r.attempts_to_match}
        for r in matched_envs
    ]

    history = read_progress(progress_file) if progress_file is not None else []
    snapshots = [row for row in _with_trend(history)
                 if since is None or (_record_day(row) is not None and _record_day(row) >= since)]

    return {
        "since": since.isoformat() if since else None,
        "worktrees": [str(w) for w in worktrees],
        "inputs": {
            "outcomes": {
                "status": "present" if outcome_files else "absent",
                "files": outcome_files,
                "runs": len(runs),
                "malformed": outcome_malformed,
            },
            "sessions": {
                "status": "present" if sessions else "absent",
                "root": str(sessions_root),
                "indexed": len(set(sessions.values())),
                "joined": joined,
                "missing": missing,
            },
            "match_logs": {
                "status": "present" if env_results else "absent",
                "environments": len(env_results),
                "matched": len(matched_envs),
            },
            "ledger": ledger_info,
            "progress": {
                "status": "present" if progress_file is not None and progress_file.is_file() else "absent",
                "path": str(progress_file) if progress_file is not None else None,
                "snapshots": len(snapshots),
            },
        },
        "totals": _row("ALL", "ALL", total),
        "tasks": [_row(task, model, groups[(task, model)]) for task, model in sorted(groups)],
        "attempts_to_match": attempts,
        "ledger": _ledger_stats(ledger_records),
        "progress": {"snapshots": snapshots},
    }


def project_figures(root: Path) -> dict[str, Any]:
    """Code progress from the source tree: matched C functions and asm/nonmatchings stubs."""
    matched = build_function_index(root)
    unmatched = {name: fn for name, fn in load_unmatched(root).items() if name not in matched}
    return progress_figures(
        {name: fn.size_bytes for name, fn in matched.items()},
        {name: fn.size_bytes for name, fn in unmatched.items()},
    )


def progress_figures(matched_sizes: Mapping[str, int], unmatched_sizes: Mapping[str, int]) -> dict[str, Any]:
    matched_bytes = sum(matched_sizes.values())
    unmatched_bytes = sum(unmatched_sizes.values())
    total = matched_bytes + unmatched_bytes
    return {
        "matched_functions": len(matched_sizes),
        "unmatched_functions": len(unmatched_sizes),
        "matched_bytes": matched_bytes,
        "unmatched_bytes": unmatched_bytes,
        "bytes_pct": round(matched_bytes / total * 100, 4) if total else 0.0,
    }


def append_snapshot(progress_file: Path, figures: Mapping[str, Any], *, worktree: Path,
                    today: date, now: datetime) -> dict[str, Any]:
    record = {
        "date": today.isoformat(),
        "ts": now.isoformat(timespec="seconds"),
        "worktree": str(worktree),
        **figures,
    }
    progress_file.parent.mkdir(parents=True, exist_ok=True)
    with progress_file.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(record, sort_keys=True) + "\n")
    return record


def _fmt(value: Any, spec: str = "") -> str:
    if value is None:
        return "-"
    return format(value, spec)


def _status_line(label: str, info: Mapping[str, Any], present: str, absent: str) -> str:
    if info.get("status") == "present":
        return f"  {label:<18}: {present}"
    return f"  {label:<18}: absent ({absent})"


def render_text(report: Mapping[str, Any]) -> str:
    inputs = report["inputs"]
    lines = [f"Pipeline metrics (since {report['since'] or 'all time'}, {len(report['worktrees'])} worktree(s))", ""]
    lines.append("Inputs")
    lines.append(_status_line("outcomes.log", inputs["outcomes"],
                              f"{inputs['outcomes']['runs']} run(s) in {inputs['outcomes']['files']} file(s), "
                              f"{inputs['outcomes']['malformed']} malformed line(s) skipped",
                              "no task-runner/*/outcomes.log"))
    lines.append(_status_line("omp sessions", inputs["sessions"],
                              f"{inputs['sessions']['indexed']} indexed, {inputs['sessions']['joined']} run(s) joined, "
                              f"{inputs['sessions']['missing']} without a session",
                              f"no session files under {inputs['sessions']['root']}"))
    lines.append(_status_line("match_log.txt", inputs["match_logs"],
                              f"{inputs['match_logs']['environments']} environment(s), "
                              f"{inputs['match_logs']['matched']} with a certified full match",
                              "no nonmatchings/*/match_log.txt"))
    lines.append(_status_line("difficulty ledger", inputs["ledger"],
                              f"{inputs['ledger']['records']} record(s) at {inputs['ledger']['path']}",
                              inputs["ledger"]["path"] or "not in a git repository and MG64_LEDGER unset"))
    lines.append(_status_line("progress.jsonl", inputs["progress"],
                              f"{inputs['progress']['snapshots']} snapshot(s) at {inputs['progress']['path']}",
                              inputs["progress"]["path"] or "not in a git repository"))

    lines.append("")
    lines.append("Per task and model")
    header = (f"  {'task':<22}{'model':<34}{'runs':>6}{'match':>7}{'rate':>7}{'m/h':>7}"
              f"{'med s':>8}{'$/run':>9}{'$/match':>9}{'tok/run':>10}")
    lines.append(header)
    rows = list(report["tasks"]) + [report["totals"]]
    for row in rows:
        rate = _fmt(row["match_rate"] * 100 if row["match_rate"] is not None else None, ".1f")
        lines.append(
            f"  {row['task']:<22}{row['model']:<34}{row['candidates']:>6}{row['matches']:>7}"
            f"{rate + '%' if rate != '-' else '-':>7}"
            f"{_fmt(row['matches_per_hour'], '.2f'):>7}"
            f"{_fmt(row['duration_median_s'], '.0f'):>8}"
            f"{_fmt(row['cost_per_run_usd'], '.3f'):>9}"
            f"{_fmt(row['cost_per_match_usd'], '.3f'):>9}"
            f"{_fmt(row['tokens_per_run'], '.0f'):>10}"
        )

    attempts = report["attempts_to_match"]
    lines.append("")
    lines.append("Attempts to match (certified full matches)")
    if attempts["n"] == 0:
        lines.append("  no certified matches yet")
        lines.append("  recommended stall limit (p85): absent (no certified matches)")
    else:
        lines.append(f"  n={attempts['n']}  p50 {attempts['p50']}  p85 {attempts['p85']}  max {attempts['max']}")
        lines.append(f"  recommended stall limit (p85): {attempts['stall_limit_recommendation']}")

    ledger = report["ledger"]
    failed = ledger["failed"]
    lines.append("")
    lines.append(f"Difficulty ledger ({ledger['records']} record(s))")
    if ledger["records"]:
        lines.append("  by outcome: " + ", ".join(f"{k} {v}" for k, v in ledger["by_outcome"].items()))
        lines.append("  by tier: " + ", ".join(f"{k}={v}" for k, v in ledger["by_tier"].items()))
        p = failed["attempts"]
        lines.append(f"  failed: {failed['records']} record(s), {failed['attempts_total']} attempt(s); "
                     f"attempts p50 {_fmt(p['p50'])}, p85 {_fmt(p['p85'])}, max {_fmt(p['max'])}")
    else:
        lines.append("  no records")

    snapshots = report["progress"]["snapshots"]
    lines.append("")
    lines.append("Progress trend")
    if not snapshots:
        lines.append("  no snapshots (run with --snapshot)")
    for snap in snapshots[-TREND_ROWS:]:
        delta = ""
        if snap.get("delta_matched_functions") is not None:
            delta = f"  (+{snap['delta_matched_functions']} functions, {snap['delta_bytes_pct']:+.4f} pts)"
        lines.append(f"  {snap['date']}  matched {snap['matched_functions']}  "
                     f"unmatched {snap['unmatched_functions']}  bytes {snap['bytes_pct']:.4f}%{delta}")
    return "\n".join(lines) + "\n"


def _date_arg(text: str) -> date:
    try:
        return date.fromisoformat(text)
    except ValueError as exc:
        raise argparse.ArgumentTypeError(f"expected YYYY-MM-DD, got {text!r}") from exc


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Report nigel pipeline metrics: outcomes, cost, attempts, ledger, progress.")
    parser.add_argument("--since", type=_date_arg, metavar="DATE",
                        help="only outcomes, ledger records and snapshots dated on or after DATE")
    parser.add_argument("--json", action="store_true", help="print the report as JSON")
    parser.add_argument("--worktree", action="append", type=Path, metavar="PATH",
                        help="worktree to read (repeatable); default: every entry of git worktree list")
    parser.add_argument("--snapshot", action="store_true",
                        help="append current progress figures to progress.jsonl before reporting")
    args = parser.parse_args(argv)

    root = get_project_root()
    common = git_common_dir(root)
    progress_file = common / PROGRESS_DIR / PROGRESS_NAME if common is not None else None

    if args.snapshot:
        if progress_file is None:
            print("error: --snapshot needs a git repository to locate progress.jsonl", file=sys.stderr)
            return 1
        now = datetime.now().astimezone()
        append_snapshot(progress_file, project_figures(root), worktree=root,
                        today=now.date(), now=now)

    worktrees = [p.resolve() for p in args.worktree] if args.worktree else git_worktrees(root)
    report = build_report(
        worktrees,
        sessions_root=SESSIONS_ROOT,
        ledger_file=ledger_path(common),
        progress_file=progress_file,
        since=args.since,
    )
    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print(render_text(report), end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
