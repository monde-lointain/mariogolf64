#!/usr/bin/env python3

from __future__ import annotations

import contextlib
import hashlib
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from datetime import date, datetime, timezone
from pathlib import Path
from typing import Any, Callable
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))

import pipeline_metrics as pm  # noqa: E402


def write_jsonl(path: Path, records: list[Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [record if isinstance(record, str) else json.dumps(record) for record in records]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def outcome_record(
    result: str = "FIXED",
    *,
    task: str = "decompile-similar",
    candidate: str = "base_1",
    duration_s: float = 600.0,
    ts: str = "2026-10-02T10:00:00Z",
    session_id: str | None = None,
    flags: str = "--model anthropic/claude-haiku-5-5:xhigh",
    details: str = "",
) -> dict[str, Any]:
    return {
        "ts_start": ts,
        "ts_end": ts,
        "duration_s": duration_s,
        "task": task,
        "candidate": candidate,
        "run_id": "run-1",
        "outcome": result,
        "details": details,
        "agent": "omp",
        "agent_flags": flags,
        "commit": "abc123" if result == "FIXED" else None,
        "omp_session_id": session_id,
    }


def assistant(cost: float, *, input_tokens: int = 100, output_tokens: int = 20,
              total_tokens: int | None = None, model: str = "claude-haiku-5-5",
              provider: str = "anthropic") -> dict[str, Any]:
    usage: dict[str, Any] = {
        "input": input_tokens,
        "output": output_tokens,
        "cacheRead": 0,
        "cacheWrite": 0,
        "cost": {"total": cost},
    }
    if total_tokens is not None:
        usage["totalTokens"] = total_tokens
    return {
        "type": "message",
        "message": {"role": "assistant", "provider": provider, "model": model, "usage": usage},
    }


def session_file(path: Path, session_id: str, entries: list[Any]) -> Path:
    header = {
        "type": "session",
        "version": 3,
        "id": session_id,
        "timestamp": "2026-10-02T10:00:00.000Z",
        "cwd": "/projects/retro/n64/mariogolf64",
    }
    write_jsonl(path, [{"type": "title", "v": 1, "title": ""}, header, *entries])
    return path


def ledger_record(outcome: str, *, attempts: int | None, tier: Any = 1,
                  ts: str = "2026-10-02T10:00:00Z") -> dict[str, Any]:
    return {
        "ts": ts,
        "function": "fn",
        "task": "decompile-similar",
        "tier": tier,
        "model": "anthropic/claude-haiku-5-5",
        "run_id": "run-1",
        "attempts": attempts,
        "best_pct": 80.0,
        "outcome": outcome,
        "reason": "test",
        "env": "nonmatchings/fn",
        "worktree": "/wt",
    }


def env_dir(worktree: Path, name: str) -> Path:
    path = worktree / "nonmatchings" / name
    path.mkdir(parents=True, exist_ok=True)
    return path


def write_match_log(env: Path, lines: list[str]) -> None:
    (env / "match_log.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def certified_line(env: Path, stem: str, body: str, *, on_disk: str | None = None) -> str:
    """Archive body under .matches/ and return the exact 100% match_log line for it.

    on_disk, when given, is written instead of body, so the archive no longer matches its hash.
    """
    digest = hashlib.sha256(body.encode()).hexdigest()
    archive = env / ".matches" / f"{stem}-{digest}.c"
    archive.parent.mkdir(parents=True, exist_ok=True)
    archive.write_text(on_disk if on_disk is not None else body, encoding="utf-8")
    return f".matches/{stem}-{digest}.c 100.000% exact sha256={digest} source=base_9.c"


def git(cwd: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", *args], cwd=cwd, check=True, capture_output=True, text=True,
        env={**os.environ, "GIT_CONFIG_NOSYSTEM": "1"},
    )
    return result.stdout


class FixtureCase(unittest.TestCase):
    def setUp(self) -> None:
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.worktree = self.root / "wt"
        self.worktree.mkdir()
        self.sessions = self.root / "sessions"
        self.sessions.mkdir()

    def write_outcomes(self, records: list[Any], task: str = "decompile-similar") -> None:
        write_jsonl(self.worktree / "task-runner" / task / "outcomes.log", records)

    def report(self, **overrides: Any) -> dict[str, Any]:
        kwargs: dict[str, Any] = {
            "sessions_root": self.sessions,
            "ledger_file": None,
            "progress_file": None,
            "since": None,
            "until": None,
            "sizes": {},
        }
        kwargs.update(overrides)
        return pm.build_report([self.worktree], **kwargs)


class OutcomeCountingTest(FixtureCase):
    def test_only_fixed_is_a_match(self) -> None:
        self.write_outcomes([
            outcome_record("FIXED"),
            outcome_record("FIXED_BUT_REVERTED"),
            outcome_record("no-changes"),
            outcome_record("NOT_FIXED"),
            outcome_record("BEST_EFFORT", details="timeout - partial"),
            outcome_record("BUILD_FAILED", details="timeout - reverted"),
        ])

        report = self.report()

        self.assertEqual(report["totals"]["matches"], 1)
        self.assertEqual(report["totals"]["candidates"], 6)
        self.assertEqual(report["totals"]["outcomes"]["FIXED_BUT_REVERTED"], 1)
        self.assertEqual(report["totals"]["outcomes"]["no-changes"], 1)
        self.assertEqual(report["totals"]["outcomes"]["BUILD_FAILED"], 1)
        self.assertNotIn("timeout", report["totals"]["outcomes"])

    def test_outcome_counts_are_per_task_row(self) -> None:
        self.write_outcomes([outcome_record("FIXED")])
        self.write_outcomes([outcome_record("NOT_FIXED", task="cleanup-matched")], task="cleanup-matched")

        rows = {row["task"]: row for row in self.report()["tasks"]}

        self.assertEqual(rows["decompile-similar"]["matches"], 1)
        self.assertEqual(rows["cleanup-matched"]["matches"], 0)
        self.assertEqual(rows["cleanup-matched"]["candidates"], 1)


class SessionCostTest(FixtureCase):
    def setUp(self) -> None:
        super().setUp()
        self.main_path = session_file(
            self.sessions / "-projects-retro-n64-mariogolf64" / "2026-10-02T10-00-00-000Z_run.jsonl",
            "sess-main",
            [
                {"type": "message", "message": {"role": "user", "content": "go"}},
                assistant(0.5, input_tokens=100, output_tokens=20, total_tokens=120),
                assistant(0.25, input_tokens=10, output_tokens=5, total_tokens=15),
                {
                    "type": "model_usage",
                    "provider": "openai-codex",
                    "model": "gpt-6-luna",
                    "purpose": "auto-thinking",
                    "usage": {"input": 1, "output": 1, "totalTokens": 2, "cost": {"total": 0.1}},
                },
            ],
        )
        # Subagent transcript nested under the parent's stem directory; no totalTokens,
        # so tokens fall back to the sum of the four components.
        nested = self.main_path.parent / self.main_path.stem / "Port" / "Port.jsonl"
        session_file(nested, "sess-sub", [assistant(0.2, input_tokens=30, output_tokens=10,
                                                    model="claude-opus-5-5")])

    def test_cost_and_tokens_include_assistant_model_usage_and_nested(self) -> None:
        usage = pm.session_usage(self.main_path)

        self.assertAlmostEqual(usage.cost_usd, 1.05)
        self.assertEqual(usage.tokens, 120 + 15 + 2 + 40)
        self.assertEqual(usage.files, 2)

    def test_model_key_comes_from_assistant_messages_only(self) -> None:
        usage = pm.session_usage(self.main_path)

        self.assertEqual(usage.models.most_common(1)[0][0], "anthropic/claude-haiku-5-5")
        self.assertNotIn("openai-codex/gpt-6-luna", usage.models)

    def test_run_is_joined_by_header_id_not_file_name(self) -> None:
        self.write_outcomes([outcome_record("FIXED", session_id="sess-main")])

        report = self.report()

        self.assertEqual(report["inputs"]["sessions"]["joined"], 1)
        self.assertAlmostEqual(report["totals"]["cost_usd"], 1.05)
        self.assertEqual(report["totals"]["tokens"], 177)

    def test_unknown_session_is_absent_not_zero_cost(self) -> None:
        self.write_outcomes([outcome_record("FIXED", session_id="no-such-session")])

        report = self.report()

        self.assertEqual(report["inputs"]["sessions"]["missing"], 1)
        self.assertEqual(report["totals"]["sessions_joined"], 0)
        self.assertIsNone(report["totals"]["cost_usd"])
        self.assertIsNone(report["totals"]["cost_per_run_usd"])

    def test_run_without_session_id_counts_as_missing(self) -> None:
        self.write_outcomes([outcome_record("FIXED", session_id=None)])

        report = self.report()

        self.assertEqual(report["inputs"]["sessions"]["missing"], 1)
        self.assertIsNone(report["totals"]["cost_usd"])

    def test_cost_per_match_divides_all_spend_by_matches(self) -> None:
        session_file(self.sessions / "-x" / "2026-10-02T11-00-00-000Z_other.jsonl", "sess-cheap",
                     [assistant(0.25)])
        self.write_outcomes([
            outcome_record("FIXED", session_id="sess-main"),
            outcome_record("NOT_FIXED", session_id="sess-cheap"),
        ])

        totals = self.report()["totals"]

        self.assertEqual(totals["matches"], 1)
        self.assertAlmostEqual(totals["cost_usd"], 1.05 + 0.25)
        self.assertAlmostEqual(totals["cost_per_match_usd"], 1.30)


class ModelAttributionTest(FixtureCase):
    def test_session_model_wins_over_agent_flags(self) -> None:
        session_file(self.sessions / "-x" / "2026-10-02T10-00-00-000Z_a.jsonl", "sess-opus",
                     [assistant(0.1, model="claude-opus-5-5")])
        self.write_outcomes([outcome_record("FIXED", session_id="sess-opus",
                                            flags="--model anthropic/claude-haiku-5-5:xhigh")])

        row = self.report()["tasks"][0]

        self.assertEqual(row["model"], "anthropic/claude-opus-5-5")

    def test_falls_back_to_model_flag_without_thinking_suffix(self) -> None:
        self.write_outcomes([outcome_record("FIXED", session_id=None,
                                            flags="--model anthropic/claude-haiku-5-5:xhigh")])

        row = self.report()["tasks"][0]

        self.assertEqual(row["model"], "anthropic/claude-haiku-5-5")

    def test_unknown_when_no_session_and_no_model_flag(self) -> None:
        self.write_outcomes([outcome_record("FIXED", session_id=None, flags="-p --mode json")])

        self.assertEqual(self.report()["tasks"][0]["model"], "unknown")

    def test_rows_split_by_model(self) -> None:
        self.write_outcomes([
            outcome_record("FIXED", session_id=None, flags="--model anthropic/claude-haiku-5-5"),
            outcome_record("NOT_FIXED", session_id=None, flags="--model anthropic/claude-opus-5-5"),
        ])

        models = {row["model"] for row in self.report()["tasks"]}

        self.assertEqual(models, {"anthropic/claude-haiku-5-5", "anthropic/claude-opus-5-5"})


class TimingTest(FixtureCase):
    def test_matches_per_hour_uses_summed_wall_time(self) -> None:
        self.write_outcomes([
            outcome_record("FIXED", duration_s=1800.0),
            outcome_record("FIXED", duration_s=1800.0),
        ])

        totals = self.report()["totals"]

        self.assertAlmostEqual(totals["wall_hours"], 1.0)
        self.assertAlmostEqual(totals["matches_per_hour"], 2.0)

    def test_median_and_mean_duration_with_even_count(self) -> None:
        self.write_outcomes([outcome_record(duration_s=d) for d in (100.0, 200.0, 300.0, 400.0)])

        totals = self.report()["totals"]

        self.assertAlmostEqual(totals["duration_median_s"], 250.0)
        self.assertAlmostEqual(totals["duration_mean_s"], 250.0)


def local_iso(*args: int) -> str:
    """A local wall-clock moment as an ISO string with its UTC offset."""
    return datetime(*args).astimezone().isoformat()


class SinceFilterTest(FixtureCase):
    def test_since_drops_older_outcomes_ledger_and_snapshots(self) -> None:
        self.write_outcomes([
            outcome_record("FIXED", ts=local_iso(2026, 9, 29, 12)),
            outcome_record("NOT_FIXED", ts=local_iso(2026, 10, 2, 9)),
        ])
        ledger = self.root / "difficulty.jsonl"
        write_jsonl(ledger, [
            ledger_record("stalled", attempts=5, ts=local_iso(2026, 9, 29, 12)),
            ledger_record("stalled", attempts=7, ts=local_iso(2026, 10, 2, 12)),
        ])
        progress = self.root / "progress.jsonl"
        for day, matched in (("2026-09-29", 1), ("2026-10-02", 2)):
            pm.append_snapshot(progress, pm.progress_figures({"a": matched}, {}),
                               worktree=self.worktree, today=date.fromisoformat(day),
                               now=datetime.fromisoformat(f"{day}T12:00:00+00:00"))

        report = self.report(ledger_file=ledger, progress_file=progress,
                             since=pm._bound_arg("2026-10-01", end_of_day=False))

        self.assertEqual(report["totals"]["candidates"], 1)
        self.assertEqual(report["ledger"]["records"], 1)
        self.assertEqual([s["date"] for s in report["progress"]["snapshots"]], ["2026-10-02"])

    def test_until_excludes_its_moment_and_a_date_includes_its_day(self) -> None:
        self.write_outcomes([
            outcome_record("FIXED", ts=local_iso(2026, 10, 2, 23, 59, 59)),
            outcome_record("NOT_FIXED", ts=local_iso(2026, 10, 3)),
        ])
        ledger = self.root / "difficulty.jsonl"
        write_jsonl(ledger, [
            ledger_record("stalled", attempts=5, ts=local_iso(2026, 10, 2, 23, 59, 59)),
            ledger_record("stalled", attempts=7, ts=local_iso(2026, 10, 3)),
        ])

        report = self.report(ledger_file=ledger, until=pm._bound_arg("2026-10-02", end_of_day=True))

        self.assertEqual(report["totals"]["candidates"], 1)
        self.assertEqual(report["totals"]["matches"], 1)
        self.assertEqual(report["ledger"]["records"], 1)

    def test_since_datetime_is_an_exact_instant(self) -> None:
        self.write_outcomes([
            outcome_record("FIXED", ts="2026-10-19T22:40:59Z"),
            outcome_record("NOT_FIXED", ts="2026-10-19T22:41:00Z"),
        ])

        report = self.report(since=pm._bound_arg("2026-10-19T17:41:00-05:00", end_of_day=False))

        self.assertEqual(report["totals"]["candidates"], 1)
        self.assertEqual(report["totals"]["matches"], 0)

    def test_windows_partition_runs_by_day(self) -> None:
        self.write_outcomes([
            outcome_record("FIXED", ts=local_iso(2026, 10, 1, 12)),
            outcome_record("FIXED", ts=local_iso(2026, 10, 2, 12)),
            outcome_record("FIXED", ts=local_iso(2026, 10, 3, 12)),
        ])

        before = self.report(until=pm._bound_arg("2026-10-02", end_of_day=False))["totals"]["candidates"]
        during = self.report(since=pm._bound_arg("2026-10-02", end_of_day=False),
                             until=pm._bound_arg("2026-10-02", end_of_day=True))["totals"]["candidates"]
        after = self.report(since=pm._bound_arg("2026-10-03", end_of_day=False))["totals"]["candidates"]

        self.assertEqual((before, during, after), (1, 1, 1))

    def test_since_drops_outcomes_without_timestamp(self) -> None:
        record = outcome_record("FIXED")
        record["ts_start"] = None
        self.write_outcomes([record])

        report = self.report(since=pm._bound_arg("2026-10-01", end_of_day=False))

        self.assertEqual(report["totals"]["candidates"], 0)

    def test_bounds_are_local_midnight_or_local_datetime(self) -> None:
        self.assertEqual(pm._bound_arg("2026-10-02", end_of_day=False), datetime(2026, 10, 2).astimezone())
        self.assertEqual(pm._bound_arg("2026-10-02", end_of_day=True), datetime(2026, 10, 3).astimezone())
        self.assertEqual(pm._bound_arg("2026-10-02T12:00:00", end_of_day=False),
                         datetime(2026, 10, 2, 12).astimezone())


class MatchedSizeTest(FixtureCase):
    def test_matched_bytes_sum_sizes_of_matched_functions(self) -> None:
        self.write_outcomes([
            outcome_record("FIXED", candidate="func_a"),
            outcome_record("FIXED", candidate='["func_b", "ref_1"]'),
            outcome_record("NOT_FIXED", candidate="func_a"),
            outcome_record("FIXED", candidate="base_1"),
        ])

        report = self.report(sizes={"func_a": 100, "func_b": 300})

        totals = report["totals"]
        self.assertEqual((totals["matches"], totals["matches_unsized"], totals["matched_bytes"]), (3, 1, 400))
        self.assertIn("1 match(es) with no function size", pm.render_text(report))

    def test_per_kb_figures_divide_cost_and_minutes_by_matched_kib(self) -> None:
        for session_id, cost in (("sess-a", 2.0), ("sess-b", 1.0)):
            session_file(self.sessions / "-x" / f"2026-10-02T10-00-00-000Z_{session_id}.jsonl", session_id,
                         [assistant(cost)])
        self.write_outcomes([
            outcome_record("FIXED", candidate="func_a", duration_s=1800.0, session_id="sess-a"),
            outcome_record("NOT_FIXED", duration_s=1800.0, session_id="sess-b"),
            outcome_record("NOT_FIXED", duration_s=1200.0),
        ])

        totals = self.report(sizes={"func_a": 2048})["totals"]

        self.assertEqual(totals["matched_bytes"], 2048)
        self.assertAlmostEqual(totals["cost_per_kb_usd"], 1.5)
        self.assertAlmostEqual(totals["minutes_per_kb"], 40.0)

    def test_per_kb_figures_are_none_without_matched_bytes(self) -> None:
        self.write_outcomes([outcome_record("NOT_FIXED", candidate="func_a", duration_s=600.0)])

        totals = self.report(sizes={"func_a": 2048})["totals"]

        self.assertIsNone(totals["cost_per_kb_usd"])
        self.assertIsNone(totals["minutes_per_kb"])


class AbsentAndMalformedInputTest(FixtureCase):
    def test_empty_inputs_are_reported_absent_without_error(self) -> None:
        report = self.report(ledger_file=self.root / "missing.jsonl")

        self.assertEqual(report["inputs"]["outcomes"]["status"], "absent")
        self.assertEqual(report["inputs"]["sessions"]["status"], "absent")
        self.assertEqual(report["inputs"]["match_logs"]["status"], "absent")
        self.assertEqual(report["inputs"]["ledger"]["status"], "absent")
        self.assertEqual(report["inputs"]["progress"]["status"], "absent")
        self.assertEqual(report["tasks"], [])
        self.assertEqual(report["attempts_to_match"]["n"], 0)
        self.assertIsNone(report["attempts_to_match"]["stall_limit_recommendation"])

    def test_text_report_names_absent_inputs(self) -> None:
        text = pm.render_text(self.report())

        self.assertIn("outcomes.log", text)
        self.assertIn("absent", text)
        self.assertIn("recommended stall limit (p85): absent", text)

    def test_malformed_lines_are_skipped_and_counted(self) -> None:
        path = self.worktree / "task-runner" / "decompile-similar" / "outcomes.log"
        write_jsonl(path, [
            outcome_record("FIXED"),
            "{not json",
            '{"task": "decompile-similar", "ts_start": "2026-10-02T10:00:00Z"}',
        ])

        report = self.report()

        self.assertEqual(report["inputs"]["outcomes"]["runs"], 1)
        self.assertEqual(report["inputs"]["outcomes"]["malformed"], 2)
        self.assertEqual(report["totals"]["matches"], 1)

    def test_read_candidate_runs_only_reads_task_runner_outcome_logs(self) -> None:
        self.write_outcomes([outcome_record("FIXED")])
        write_jsonl(self.worktree / "task-runner" / "decompile-similar" / "other.log",
                    [outcome_record("FIXED")])
        write_jsonl(self.worktree / "nonmatchings" / "fn" / "outcomes.log",
                    [outcome_record("FIXED")])

        runs, files, malformed = pm.read_candidate_runs(self.worktree, since=None)

        self.assertEqual(files, 1)
        self.assertEqual(len(runs), 1)
        self.assertEqual(malformed, 0)

    def test_session_index_maps_header_id_and_stem_and_skips_headerless_files(self) -> None:
        session_file(self.sessions / "-x" / "2026-10-02T10-00-00-000Z_a.jsonl", "hdr-1", [])
        write_jsonl(self.sessions / "-x" / "headerless.jsonl", [{"type": "message"}])

        index = pm.index_sessions(self.sessions)

        self.assertEqual(index["hdr-1"].name, "2026-10-02T10-00-00-000Z_a.jsonl")
        self.assertEqual(index["2026-10-02T10-00-00-000Z_a"].name, "2026-10-02T10-00-00-000Z_a.jsonl")
        self.assertNotIn("headerless", index)


class AttemptsToMatchTest(FixtureCase):
    def test_attempts_to_match_is_first_certified_full_match(self) -> None:
        env = env_dir(self.worktree, "ChangeFoo")
        write_match_log(env, [
            "base_1.c 40.000%",
            "base_2.c 55.000%",
            "base_3.c 99.000%",
            certified_line(env, "ChangeFoo", "int f(void) { return 1; }\n"),
            "base_5.c 99.000%",
        ])

        result = pm.scan_environments(self.worktree)

        self.assertEqual(len(result), 1)
        self.assertEqual(result[0].attempts_to_match, 4)
        self.assertEqual(result[0].total_attempts, 5)
        self.assertEqual(result[0].function, "ChangeFoo")

    def test_legacy_full_match_without_archive_is_not_a_match(self) -> None:
        env = env_dir(self.worktree, "Legacy")
        write_match_log(env, ["base_1.c 100.000%"])

        self.assertIsNone(pm.scan_environments(self.worktree)[0].attempts_to_match)

    def test_modified_archive_is_not_a_match(self) -> None:
        env = env_dir(self.worktree, "Edited")
        write_match_log(env, [certified_line(env, "Edited", "int a;\n", on_disk="int b;\n")])

        self.assertIsNone(pm.scan_environments(self.worktree)[0].attempts_to_match)

    def test_below_full_and_uncertified_full_lines_never_count(self) -> None:
        env = env_dir(self.worktree, "Partial")
        write_match_log(env, ["base_1.c 99.999%", "base_2.c 100%"])

        self.assertIsNone(pm.scan_environments(self.worktree)[0].attempts_to_match)

    def test_nearest_rank_percentiles(self) -> None:
        values = list(range(1, 21))

        self.assertEqual(pm.nearest_rank(values, 50), 10)
        self.assertEqual(pm.nearest_rank(values, 85), 17)
        self.assertEqual(pm.nearest_rank(values, 100), 20)
        self.assertEqual(pm.nearest_rank([1, 2, 3, 4], 50), 2)
        self.assertEqual(pm.nearest_rank([1, 2, 3, 4], 85), 4)
        self.assertEqual(pm.nearest_rank([5], 85), 5)
        self.assertIsNone(pm.nearest_rank([], 85))

    def test_stall_recommendation_is_p85_of_matched_environments(self) -> None:
        for attempt in range(1, 21):
            env = env_dir(self.worktree, f"Fn{attempt}")
            body = f"int fn{attempt}(void) {{ return {attempt}; }}\n"
            lines = [f"base_{i}.c 10.000%" for i in range(1, attempt)]
            lines.append(certified_line(env, f"Fn{attempt}", body))
            write_match_log(env, lines)
        still_open = env_dir(self.worktree, "Open")
        write_match_log(still_open, [f"base_{i}.c 50.000%" for i in range(1, 51)])

        attempts = self.report()["attempts_to_match"]

        self.assertEqual(attempts["n"], 20)
        self.assertEqual(attempts["p50"], 10)
        self.assertEqual(attempts["p85"], 17)
        self.assertEqual(attempts["max"], 20)
        self.assertEqual(attempts["stall_limit_recommendation"], 17)
        self.assertIn("recommended stall limit (p85): 17", pm.render_text(self.report()))


class LedgerTest(FixtureCase):
    def test_failed_attempt_stats_exclude_matched_records(self) -> None:
        ledger = self.root / "difficulty.jsonl"
        write_jsonl(ledger, [
            ledger_record("matched", attempts=3, tier=1),
            ledger_record("stalled", attempts=10, tier=1),
            ledger_record("timeout", attempts=30, tier=2),
            ledger_record("gave_up", attempts=50, tier="human"),
        ])

        stats = self.report(ledger_file=ledger)["ledger"]

        self.assertEqual(stats["records"], 4)
        self.assertEqual(stats["by_outcome"], {"matched": 1, "stalled": 1, "timeout": 1, "gave_up": 1})
        self.assertEqual(stats["by_tier"], {"1": 2, "2": 1, "human": 1})
        self.assertEqual(stats["failed"]["records"], 3)
        self.assertEqual(stats["failed"]["attempts_total"], 90)
        self.assertEqual(stats["failed"]["attempts"]["p50"], 30)
        self.assertEqual(stats["failed"]["attempts"]["max"], 50)

    def test_record_without_attempts_counts_outcome_but_not_attempts(self) -> None:
        ledger = self.root / "difficulty.jsonl"
        write_jsonl(ledger, [ledger_record("stalled", attempts=None), ledger_record("stalled", attempts=4)])

        stats = self.report(ledger_file=ledger)["ledger"]

        self.assertEqual(stats["by_outcome"]["stalled"], 2)
        self.assertEqual(stats["failed"]["attempts_total"], 4)

    def test_ledger_path_env_overrides_default(self) -> None:
        self.assertEqual(pm.ledger_path(self.root, {"MG64_LEDGER": "/x/y.jsonl"}), Path("/x/y.jsonl"))

    def test_ledger_path_is_none_outside_git(self) -> None:
        self.assertIsNone(pm.ledger_path(None, {}))


class GitLayoutTest(unittest.TestCase):
    def setUp(self) -> None:
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.main = self.root / "main"
        self.main.mkdir()
        git(self.main, "init", "-q", "-b", "dev")
        git(self.main, "-c", "user.name=t", "-c", "user.email=t@example.invalid",
            "commit", "--allow-empty", "-q", "-m", "init")
        self.linked = self.root / "linked"
        git(self.main, "worktree", "add", "-q", str(self.linked))

    def test_ledger_default_is_shared_by_worktrees(self) -> None:
        common = pm.git_common_dir(self.linked)

        self.assertEqual(common.resolve(), (self.main / ".git").resolve())
        self.assertEqual(
            pm.ledger_path(common, {}).resolve(),
            (self.main / ".git" / "mg64" / "difficulty.jsonl").resolve(),
        )

    def test_git_worktrees_lists_main_and_linked(self) -> None:
        listed = {path.resolve() for path in pm.git_worktrees(self.main)}

        self.assertEqual(listed, {self.main.resolve(), self.linked.resolve()})

    def test_git_common_dir_is_none_outside_a_repository(self) -> None:
        self.assertIsNone(pm.git_common_dir(self.root / "not-a-repo-dir-that-does-not-exist"))


class ProgressTest(FixtureCase):
    def test_progress_figures_count_functions_and_bytes(self) -> None:
        figures = pm.progress_figures({"a": 30, "b": 70}, {"c": 100})

        self.assertEqual(figures["matched_functions"], 2)
        self.assertEqual(figures["unmatched_functions"], 1)
        self.assertEqual(figures["matched_bytes"], 100)
        self.assertEqual(figures["unmatched_bytes"], 100)
        self.assertAlmostEqual(figures["bytes_pct"], 50.0)

    def test_progress_figures_with_no_functions_is_zero_percent(self) -> None:
        self.assertEqual(pm.progress_figures({}, {})["bytes_pct"], 0.0)

    def test_snapshot_appends_one_line_per_call_creating_parent_dir(self) -> None:
        progress = self.root / "mg64" / "progress.jsonl"
        now = datetime(2026, 10, 2, 9, 30, tzinfo=timezone.utc)

        pm.append_snapshot(progress, pm.progress_figures({"a": 1}, {"b": 1}),
                           worktree=self.worktree, today=date(2026, 10, 2), now=now)
        pm.append_snapshot(progress, pm.progress_figures({"a": 1, "b": 1}, {}),
                           worktree=self.worktree, today=date(2026, 10, 3), now=now)

        lines = progress.read_text(encoding="utf-8").splitlines()
        self.assertEqual(len(lines), 2)
        records = pm.read_progress(progress)
        self.assertEqual([r["date"] for r in records], ["2026-10-02", "2026-10-03"])
        self.assertAlmostEqual(records[1]["bytes_pct"], 100.0)
        self.assertEqual(records[0]["worktree"], str(self.worktree))

    def test_read_progress_skips_malformed_lines(self) -> None:
        progress = self.root / "progress.jsonl"
        write_jsonl(progress, [{"date": "2026-10-02", "matched_functions": 1}, "garbage"])

        self.assertEqual(len(pm.read_progress(progress)), 1)

    def test_trend_reports_delta_from_previous_snapshot(self) -> None:
        progress = self.root / "progress.jsonl"
        now = datetime(2026, 10, 2, tzinfo=timezone.utc)
        pm.append_snapshot(progress, pm.progress_figures({"a": 1}, {"b": 1}),
                           worktree=self.worktree, today=date(2026, 10, 1), now=now)
        pm.append_snapshot(progress, pm.progress_figures({"a": 1, "b": 1}, {}),
                           worktree=self.worktree, today=date(2026, 10, 2), now=now)

        snapshots = self.report(progress_file=progress)["progress"]["snapshots"]

        self.assertEqual(snapshots[0].get("delta_matched_functions"), None)
        self.assertEqual(snapshots[1]["delta_matched_functions"], 1)
        self.assertAlmostEqual(snapshots[1]["delta_bytes_pct"], 50.0)


class CliTest(FixtureCase):
    def test_json_report_with_absent_inputs(self) -> None:
        ledger = self.root / "none.jsonl"
        buffer = io.StringIO()
        with mock.patch.object(pm, "SESSIONS_ROOT", self.sessions / "missing"), \
                mock.patch.dict(os.environ, {"MG64_LEDGER": str(ledger)}), \
                contextlib.redirect_stdout(buffer):
            code = pm.main(["--json", "--worktree", str(self.worktree)])

        report = json.loads(buffer.getvalue())
        self.assertEqual(code, 0)
        self.assertEqual(report["inputs"]["ledger"]["status"], "absent")
        self.assertEqual(report["inputs"]["ledger"]["path"], str(ledger))
        self.assertEqual(report["tasks"], [])

    def test_bounds_must_be_dates_or_datetimes(self) -> None:
        for flag in ("--since", "--until"):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                pm.main([flag, "2026-13-01"])


if __name__ == "__main__":
    unittest.main()
