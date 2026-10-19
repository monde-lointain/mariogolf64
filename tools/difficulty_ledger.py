#!/usr/bin/env python3
"""Difficulty ledger: how hard each decompilation target has been, and which tier it belongs to.

Every decompile environment (nonmatchings/<FUNC>[-<id>]) that ends without a match leaves
one JSON record in a shared ledger. A function's tier follows from its failure history;
score_functions.py --tier and list_decomp_candidates.py both use failed_tiers() for it.

Tiers: 1 = no failures; 2 = failed tier 1; 3 = failed tier 2 with best_pct >= 90;
'human' = failed tier 3, or failed tier 2 below 90. A NEAR_MISS.md note in an environment
with no ledger record counts as a failure too.

Ledger path: $MG64_LEDGER, else <git common dir>/mg64/difficulty.jsonl (shared by every
worktree). Commands:
  record <function> --outcome O --reason R   append this environment's outcome
  harvest                                    back-fill outcomes for finished nigel runs
  show [--tier 1|2|3|human] [--json]         list functions with failure history
  write-run <env> <function>                 write <env>/.run.json (called by decomp-env)
See docs/tools/difficulty-ledger.md.
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import os
import shlex
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Mapping, Optional, Sequence

from nonmatching_match_log import (
    certified_attempt_path,
    integrated_function_names,
    parse_match_log_line,
)

HUMAN = 'human'
LEDGER_ENV = 'MG64_LEDGER'
RUN_FILE = '.run.json'
NEAR_MISS_NOTE = 'NEAR_MISS.md'
MATCH_LOG = 'match_log.txt'
OUTCOMES = ('matched', 'near_miss', 'stalled', 'timeout', 'integration_failed', 'gave_up')
TIER3_MIN_BEST_PCT = 90.0
RECORD_FIELDS = ('ts', 'function', 'task', 'tier', 'model', 'run_id', 'attempts',
                 'best_pct', 'outcome', 'reason', 'env', 'worktree')


@dataclasses.dataclass(frozen=True)
class TierBudget:
    timeout_minutes: int
    stall_limit: int


# Budgets per tier (plan C3). Top-n and strategy are in the task.yaml that serves each tier.
TIER_BUDGETS = {
    1: TierBudget(timeout_minutes=60, stall_limit=20),
    2: TierBudget(timeout_minutes=120, stall_limit=30),
    3: TierBudget(timeout_minutes=240, stall_limit=40),
}

# Which nigel task runs at which tier. Unmapped tasks fall back to the run file or ledger.
TASK_TIERS = {
    'decompile-similar': 1,
    'decompile-unmatched': 1,
    'decompile-coddog-similar': 1,
    'decompile-tier2': 2,
    'decompile-tier3': 3,
}


class LedgerError(RuntimeError):
    """A ledger record, run file or argument is invalid; the message says where."""


def _git(args: Sequence[str], cwd=None) -> str:
    proc = subprocess.run(['git', *args], cwd=cwd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise LedgerError(f"git {' '.join(args)} failed: {proc.stderr.strip()}")
    return proc.stdout.strip()


def _iso(ts: float) -> str:
    return datetime.fromtimestamp(ts, tz=timezone.utc).isoformat(timespec='seconds')


def _valid_tier(value) -> bool:
    return value == HUMAN or (type(value) is int and value in TIER_BUDGETS)


def _check_reason(reason: str) -> None:
    if not reason.strip() or '\n' in reason or '\r' in reason:
        raise LedgerError('reason must be one non-empty line')


def ledger_path(environ: Optional[Mapping[str, str]] = None, cwd=None) -> Path:
    env = os.environ if environ is None else environ
    override = env.get(LEDGER_ENV)
    if override:
        return Path(override)
    common = _git(['rev-parse', '--path-format=absolute', '--git-common-dir'], cwd=cwd)
    return Path(common) / 'mg64' / 'difficulty.jsonl'


# --- ledger records -------------------------------------------------------

def _validate(rec, where: str) -> None:
    if not isinstance(rec, dict):
        raise LedgerError(f'{where}: record is not a JSON object')
    missing = [key for key in RECORD_FIELDS if key not in rec]
    if missing:
        raise LedgerError(f"{where}: missing field(s) {', '.join(missing)}")
    if rec['outcome'] not in OUTCOMES:
        raise LedgerError(f"{where}: unknown outcome {rec['outcome']!r}")
    if not _valid_tier(rec['tier']):
        raise LedgerError(f"{where}: invalid tier {rec['tier']!r}")


def read_records(path: Path) -> list[dict]:
    if not path.exists():
        return []
    records = []
    for lineno, line in enumerate(path.read_text().splitlines(), 1):
        if not line.strip():
            continue
        where = f'{path}:{lineno}'
        try:
            rec = json.loads(line)
        except json.JSONDecodeError as exc:
            raise LedgerError(f'{where}: invalid JSON ({exc.msg})') from exc
        _validate(rec, where)
        records.append(rec)
    return records


def append_record(path: Path, rec: dict) -> None:
    _validate(rec, str(path))
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('a') as fh:
        fh.write(json.dumps(rec) + '\n')


def _key(rec: dict) -> tuple[str, str, str]:
    # The same env path exists in every worktree, so the worktree is part of the identity.
    return (rec['function'], rec['worktree'], rec['env'])


def failure_history(records: Sequence[dict]) -> dict[str, list[dict]]:
    """Latest non-matched record per (function, worktree, env); a matched record clears a function."""
    latest: dict[tuple[str, str, str], dict] = {}
    cleared: set[str] = set()
    for rec in records:
        if rec['outcome'] == 'matched':
            cleared.add(rec['function'])
            continue
        latest[_key(rec)] = rec
    history: dict[str, list[dict]] = {}
    for rec in latest.values():
        if rec['function'] not in cleared:
            history.setdefault(rec['function'], []).append(rec)
    return history


# --- environments ---------------------------------------------------------

def _env_dirs(root: Path) -> list[Path]:
    base = root / 'nonmatchings'
    if not base.is_dir():
        return []
    return sorted(p for p in base.iterdir() if p.is_dir())


def _env_rel(root: Path, env_dir: Path) -> str:
    try:
        return env_dir.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return str(env_dir)


def _function_of_env(env_dir: Path) -> str:
    return env_dir.name.split('-', 1)[0]


def read_run(env_dir: Path) -> Optional[dict]:
    path = env_dir / RUN_FILE
    if not path.is_file():
        return None
    try:
        data = json.loads(path.read_text())
    except json.JSONDecodeError as exc:
        raise LedgerError(f'{path}: invalid JSON ({exc.msg})') from exc
    if not isinstance(data, dict):
        raise LedgerError(f'{path}: not a JSON object')
    return data


def write_run(env_dir: Path, function: str, environ: Mapping[str, str], *,
              fallback_tier, now: float) -> dict:
    """Write <env>/.run.json from the nigel environment; returns the written dict."""
    task = environ.get('NIGEL_TASK_NAME') or None
    run_id = environ.get('NIGEL_RUN_ID') or None
    tier = TASK_TIERS.get(task) if task else None
    if tier is None:
        tier = fallback_tier
    budget = TIER_BUDGETS.get(tier)
    raw_deadline = environ.get('NIGEL_TIMEOUT_DEADLINE_UNIX')
    if raw_deadline:
        deadline = float(raw_deadline)
    elif run_id and budget:
        deadline = now + budget.timeout_minutes * 60
    else:
        deadline = None
    run = {
        'function': function,
        'task': task,
        'tier': tier,
        'model': model_from_flags(environ.get('NIGEL_AGENT_FLAGS')),
        'run_id': run_id,
        'started': _iso(now),
        'deadline_unix': deadline,
        'stall_limit': budget.stall_limit if budget else None,
    }
    (env_dir / RUN_FILE).write_text(json.dumps(run, indent=2) + '\n')
    return run


def model_from_flags(flags: Optional[str]) -> Optional[str]:
    """The value of --model in an agent flags string, or None."""
    if not flags:
        return None
    try:
        tokens = shlex.split(flags)
    except ValueError:
        tokens = flags.split()
    for i, token in enumerate(tokens):
        if token == '--model' and i + 1 < len(tokens):
            return tokens[i + 1]
        if token.startswith('--model='):
            return token.split('=', 1)[1]
    return None


def env_progress(env_dir: Path) -> tuple[int, float]:
    """(attempts, best certified percent) from the environment's match log."""
    log = env_dir / MATCH_LOG
    if not log.is_file():
        return 0, 0.0
    attempts = 0
    best = 0.0
    for line in log.read_text(errors='replace').splitlines():
        entry = parse_match_log_line(line)
        if entry is None:
            continue
        attempts += 1
        if certified_attempt_path(entry, env_dir) is not None:
            best = max(best, entry.percent)
    return attempts, best


def near_miss_records(root: Path, records: Sequence[dict]) -> list[dict]:
    """Implicit failure records for NEAR_MISS.md notes that have no ledger record yet."""
    recorded = {_key(rec) for rec in records}
    found = []
    for env_dir in _env_dirs(root):
        note = env_dir / NEAR_MISS_NOTE
        if not note.is_file():
            continue
        function = _function_of_env(env_dir)
        env = _env_rel(root, env_dir)
        if (function, str(root.resolve()), env) in recorded:
            continue
        run = read_run(env_dir) or {}
        tier = run.get('tier') if _valid_tier(run.get('tier')) else 1
        attempts, best = env_progress(env_dir)
        found.append({
            'ts': _iso(note.stat().st_mtime),
            'function': function,
            'task': run.get('task'),
            'tier': tier,
            'model': run.get('model'),
            'run_id': run.get('run_id'),
            'attempts': attempts,
            'best_pct': best,
            'outcome': 'near_miss',
            'reason': 'NEAR_MISS.md note without a ledger record',
            'env': env,
            'worktree': str(root.resolve()),
        })
    return found


# --- tiers ----------------------------------------------------------------

def _rank(tier) -> int:
    return 4 if tier == HUMAN else tier


def tier_from_failures(failures: Sequence[dict]) -> object:
    """The tier a function belongs to, given its failure records (see module docstring)."""
    if not failures:
        return 1
    worst = max(_rank(f['tier']) for f in failures)
    if worst == 1:
        return 2
    if worst == 2:
        best = max(f['best_pct'] for f in failures if f['tier'] == 2)
        return 3 if best >= TIER3_MIN_BEST_PCT else HUMAN
    return HUMAN


def failed_tiers(root: Path, records: Sequence[dict]) -> dict[str, object]:
    """{function: tier} for every function with failure history; others are tier 1."""
    history = failure_history(list(records) + near_miss_records(root, records))
    return {name: tier_from_failures(fails) for name, fails in history.items()}


def tier_of(function: str, tiers: Mapping[str, object]) -> object:
    return tiers.get(function, 1)


# --- building and harvesting records --------------------------------------

def build_record(root: Path, env_dir: Path, function: str, outcome: str, reason: str, *,
                 records: Sequence[dict], environ: Optional[Mapping[str, str]] = None,
                 now: Optional[float] = None) -> dict:
    environ = os.environ if environ is None else environ
    now = time.time() if now is None else now
    if outcome not in OUTCOMES:
        raise LedgerError(f"unknown outcome {outcome!r}; expected one of {', '.join(OUTCOMES)}")
    _check_reason(reason)
    run = read_run(env_dir) or {}
    if run.get('function', function) != function:
        raise LedgerError(
            f"{env_dir / RUN_FILE} is for {run.get('function')!r}, not {function!r}")
    task = environ.get('NIGEL_TASK_NAME') or run.get('task') or None
    run_id = environ.get('NIGEL_RUN_ID') or run.get('run_id') or None
    model = model_from_flags(environ.get('NIGEL_AGENT_FLAGS')) or run.get('model') or None
    tier = TASK_TIERS.get(task) if task else None
    if tier is None and _valid_tier(run.get('tier')):
        tier = run['tier']
    if tier is None:
        tier = tier_of(function, failed_tiers(root, records))
    attempts, best = env_progress(env_dir)
    if outcome == 'matched':
        # matched is only recorded after integration verified the ROM, and build.sh does not
        # log the base.c baseline, so a baseline match would otherwise read as 0%.
        best = 100.0
    return {
        'ts': _iso(now),
        'function': function,
        'task': task,
        'tier': tier,
        'model': model,
        'run_id': run_id,
        'attempts': attempts,
        'best_pct': best,
        'outcome': outcome,
        'reason': reason,
        'env': _env_rel(root, env_dir),
        'worktree': str(root.resolve()),
    }


def harvest(root: Path, ledger: Path, *, now: float, integrated: Optional[set] = None) -> list[dict]:
    """Record outcomes for finished nigel runs that have no ledger record yet.

    A function now in src/ is recorded as matched. A run past its deadline is recorded as
    near_miss when it left a NEAR_MISS.md note, else as timeout. Runs still inside their
    deadline are left alone.
    """
    records = read_records(ledger)
    if integrated is None:
        integrated = integrated_function_names(root)
    recorded = {_key(rec) for rec in records}
    added = []
    for env_dir in _env_dirs(root):
        run = read_run(env_dir)
        if not run or not run.get('run_id'):
            continue
        function = run.get('function') or _function_of_env(env_dir)
        key = (function, str(root.resolve()), _env_rel(root, env_dir))
        if key in recorded:
            continue
        deadline = run.get('deadline_unix')
        past = deadline is not None and now >= deadline
        if function in integrated:
            outcome, reason = 'matched', 'integrated into src/ (harvested)'
        elif past and (env_dir / NEAR_MISS_NOTE).is_file():
            outcome, reason = 'near_miss', 'NEAR_MISS.md note past run deadline (harvested)'
        elif past:
            outcome, reason = 'timeout', 'no ledger record before run deadline (harvested)'
        else:
            continue
        rec = build_record(root, env_dir, function, outcome, reason,
                           records=records + added, environ={}, now=now)
        append_record(ledger, rec)
        added.append(rec)
        recorded.add(key)
    return added


def shown_rows(root: Path, records: Sequence[dict], tier=None) -> list[dict]:
    """One row per function with failure history, optionally only those at `tier`."""
    history = failure_history(list(records) + near_miss_records(root, records))
    rows = []
    for function in sorted(history):
        fails = history[function]
        function_tier = tier_from_failures(fails)
        if tier is not None and str(function_tier) != str(tier):
            continue
        last = max(fails, key=lambda rec: rec['ts'])
        rows.append({
            'function': function,
            'tier': function_tier,
            'failures': len(fails),
            'best_pct': max(rec['best_pct'] for rec in fails),
            'last_outcome': last['outcome'],
            'last_reason': last['reason'],
            'last_env': last['env'],
        })
    return rows


# --- command line ---------------------------------------------------------

def _repo_root(cwd=None) -> Path:
    return Path(_git(['rev-parse', '--show-toplevel'], cwd=cwd)).resolve()


def _find_env(root: Path, function: str, cwd: Path) -> Path:
    if (cwd / 'target.s').is_file():
        return cwd
    matches = [d for d in _env_dirs(root)
               if (d.name == function or d.name.startswith(function + '-'))
               and (d / 'target.s').is_file()]
    if not matches:
        raise LedgerError(f'no decompile environment for {function} under {root}/nonmatchings; '
                          'run from it or pass --env')
    return max(matches, key=lambda d: (d.stat().st_mtime, d.name))


def _cmd_record(args) -> int:
    root = _repo_root()
    ledger = ledger_path(cwd=root)
    env_dir = (Path(args.env).resolve() if args.env
               else _find_env(root, args.function, Path.cwd().resolve()))
    rec = build_record(root, env_dir, args.function, args.outcome, args.reason,
                       records=read_records(ledger))
    append_record(ledger, rec)
    print(f"recorded {rec['function']} tier={rec['tier']} outcome={rec['outcome']} "
          f"attempts={rec['attempts']} best={rec['best_pct']:.3f}")
    return 0


def _cmd_harvest(_args) -> int:
    root = _repo_root()
    ledger = ledger_path(cwd=root)
    added = harvest(root, ledger, now=time.time())
    # stdout belongs to the candidate list: progress goes to stderr only.
    for rec in added:
        print(f"harvested {rec['function']} {rec['outcome']} {rec['env']}", file=sys.stderr)
    return 0


def _cmd_show(args) -> int:
    root = _repo_root()
    rows = shown_rows(root, read_records(ledger_path(cwd=root)), tier=args.tier)
    if args.tier == '1':
        print('note: this lists only functions with failure history; '
              'tier 1 candidates are from score_functions.py --tier 1', file=sys.stderr)
    if args.json:
        print(json.dumps(rows, indent=2))
        return 0
    print('function\ttier\tfailures\tbest_pct\tlast_outcome\tlast_reason')
    for row in rows:
        print(f"{row['function']}\t{row['tier']}\t{row['failures']}\t{row['best_pct']:.3f}\t"
              f"{row['last_outcome']}\t{row['last_reason']}")
    return 0


def _cmd_write_run(args) -> int:
    env_dir = Path(args.env).resolve()
    root = _repo_root(cwd=env_dir)
    fallback = tier_of(args.function, failed_tiers(root, read_records(ledger_path(cwd=root))))
    write_run(env_dir, args.function, os.environ, fallback_tier=fallback, now=time.time())
    return 0


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = parser.add_subparsers(dest='command', required=True)

    rec = sub.add_parser('record', help="append this environment's outcome to the ledger")
    rec.add_argument('function')
    rec.add_argument('--outcome', required=True, choices=OUTCOMES)
    rec.add_argument('--reason', required=True, help='one line')
    rec.add_argument('--env', help='decompile environment (default: this directory or '
                                   'nonmatchings/<function>[-id])')
    rec.set_defaults(run=_cmd_record)

    har = sub.add_parser('harvest', help='back-fill outcomes for finished nigel runs')
    har.set_defaults(run=_cmd_harvest)

    show = sub.add_parser('show', help='list functions with failure history')
    show.add_argument('--tier', choices=['1', '2', '3', HUMAN])
    show.add_argument('--json', action='store_true')
    show.set_defaults(run=_cmd_show)

    wr = sub.add_parser('write-run', help='write <env>/.run.json from the nigel environment')
    wr.add_argument('env')
    wr.add_argument('function')
    wr.set_defaults(run=_cmd_write_run)

    args = parser.parse_args(argv)
    try:
        return args.run(args)
    except LedgerError as exc:
        print(f'difficulty_ledger: {exc}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
