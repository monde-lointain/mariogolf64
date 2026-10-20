#!/usr/bin/env python3
"""Tests for difficulty_ledger.py. Every tree and ledger lives in a tempdir."""

import hashlib
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import difficulty_ledger as dl  # noqa: E402

NOW = 1_700_000_000.0

def record(function, env, tier, best=0.0, outcome='timeout', ts='2024-01-01T00:00:00+00:00',
            worktree='/tmp/mg64'):
    return {'ts': ts, 'function': function, 'task': None, 'tier': tier, 'model': None,
            'run_id': None, 'attempts': 0, 'best_pct': best, 'outcome': outcome,
            'reason': 'r', 'env': env, 'worktree': worktree}


def failure(tier, best=0.0, outcome='timeout', function='func_X', env='nonmatchings/func_X-1'):
    return record(function, env, tier, best, outcome)


class TreeCase(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name).resolve()
        (self.root / 'nonmatchings').mkdir()
        self.ledger = self.root / 'mg64' / 'difficulty.jsonl'

    def tearDown(self):
        self._tmp.cleanup()

    def make_env(self, name, *, run=None, match_lines=(), near_miss=False):
        env = self.root / 'nonmatchings' / name
        env.mkdir(parents=True)
        (env / 'target.s').write_text('')
        if run is not None:
            (env / dl.RUN_FILE).write_text(json.dumps(run))
        if match_lines:
            (env / 'match_log.txt').write_text('\n'.join(match_lines) + '\n')
        if near_miss:
            (env / dl.NEAR_MISS_NOTE).write_text('near miss\n')
        return env


class TierFromFailuresTest(unittest.TestCase):
    def test_no_failures_is_tier_1(self):
        self.assertEqual(dl.tier_from_failures([]), 1)

    def test_failed_tier_1_escalates_to_tier_2_regardless_of_progress(self):
        self.assertEqual(dl.tier_from_failures([failure(1, 99.5, 'near_miss')]), 2)
        self.assertEqual(dl.tier_from_failures([failure(1, 0.0)]), 2)

    def test_tier_2_failure_at_exactly_90_percent_goes_to_tier_3(self):
        self.assertEqual(dl.tier_from_failures([failure(1), failure(2, 90.0)]), 3)

    def test_tier_2_failure_just_below_90_goes_to_human(self):
        self.assertEqual(dl.tier_from_failures([failure(1), failure(2, 89.999)]), 'human')

    def test_best_tier_2_progress_decides_regardless_of_order(self):
        self.assertEqual(dl.tier_from_failures([failure(1), failure(2, 40.0), failure(2, 91.0)]), 3)
        self.assertEqual(dl.tier_from_failures([failure(1), failure(2, 91.0), failure(2, 40.0)]), 3)

    def test_failure_at_tier_3_goes_to_human(self):
        self.assertEqual(dl.tier_from_failures([failure(1), failure(2, 95.0), failure(3, 99.0)]), 'human')

    def test_human_stays_human(self):
        self.assertEqual(dl.tier_from_failures([failure('human', 0.0)]), 'human')


class FailureHistoryTest(TreeCase):
    def test_matched_record_clears_history(self):
        records = [
            record('func_X', 'nonmatchings/func_X-1', 1),
            record('func_X', 'nonmatchings/func_X-2', 1, outcome='matched'),
        ]
        self.assertNotIn('func_X', dl.failure_history(records))
        self.assertEqual(dl.failed_tiers(self.root, records), {})

    def test_latest_record_per_environment_wins(self):
        records = [
            record('func_X', 'nonmatchings/func_X-1', 2, 95.0, ts='2024-01-01T00:00:00+00:00'),
            record('func_X', 'nonmatchings/func_X-1', 2, 40.0, ts='2024-01-02T00:00:00+00:00'),
        ]
        self.assertEqual(dl.failed_tiers(self.root, records)['func_X'], 'human')

    def test_separate_environments_both_count(self):
        records = [
            record('func_X', 'nonmatchings/func_X-1', 2, 95.0),
            record('func_X', 'nonmatchings/func_X-2', 1, 0.0),
        ]
        self.assertEqual(dl.failed_tiers(self.root, records)['func_X'], 3)


class NearMissNoteTest(TreeCase):
    def test_note_without_ledger_record_is_a_tier_1_failure(self):
        self.make_env('func_A-1', near_miss=True)
        self.assertEqual(dl.failed_tiers(self.root, [])['func_A'], 2)

    def test_note_uses_run_file_tier_and_match_log_progress(self):
        self.make_env(
            'func_A-1',
            run={'function': 'func_A', 'tier': 2},
            match_lines=['base_1.c 93.000%'],
            near_miss=True,
        )
        self.assertEqual(dl.failed_tiers(self.root, [])['func_A'], 3)

    def test_note_at_nonmatchings_root_counts_too(self):
        (self.root / 'nonmatchings' / 'func_B').mkdir()
        (self.root / 'nonmatchings' / 'func_B' / dl.NEAR_MISS_NOTE).write_text('x\n')
        self.assertEqual(dl.failed_tiers(self.root, [])['func_B'], 2)

    def test_recorded_environment_is_not_double_counted(self):
        self.make_env('func_A-1', run={'function': 'func_A', 'tier': 2}, near_miss=True)
        records = [record('func_A', 'nonmatchings/func_A-1', 2, 50.0, outcome='near_miss',
                          worktree=str(self.root))]
        history = dl.failure_history(records + dl.near_miss_records(self.root, records))
        self.assertEqual(len(history['func_A']), 1)

    def test_matched_ledger_record_overrides_note(self):
        self.make_env('func_A-1', near_miss=True)
        records = [record('func_A', 'nonmatchings/func_A-1', 1, outcome='matched',
                          worktree=str(self.root))]
        self.assertNotIn('func_A', dl.failed_tiers(self.root, records))


class EnvProgressTest(TreeCase):
    def test_counts_every_attempt_and_the_best_certified_percent(self):
        env = self.make_env('func_A-1', match_lines=[
            'base_1.c 45.300%',
            'base_2.c 91.250%',
            'base_3.c 100.000%',
        ])
        attempts, best = dl.env_progress(env)
        self.assertEqual(attempts, 3)
        self.assertEqual(best, 91.25)

    def test_uncertified_full_match_is_not_credited(self):
        env = self.make_env('func_A-1', match_lines=['base_1.c 100.000%'])
        self.assertEqual(dl.env_progress(env), (1, 0.0))

    def test_certified_full_match_is_credited(self):
        env = self.make_env('func_A-1')
        matches = env / '.matches'
        matches.mkdir()
        body = b'int f(void) { return 0; }\n'
        digest = hashlib.sha256(body).hexdigest()
        (matches / f'base_4-{digest}.c').write_bytes(body)
        (env / 'match_log.txt').write_text(
            f'.matches/base_4-{digest}.c 100.000% exact sha256={digest}\n')
        self.assertEqual(dl.env_progress(env), (1, 100.0))

    def test_missing_match_log_is_zero_attempts(self):
        env = self.make_env('func_A-1')
        self.assertEqual(dl.env_progress(env), (0, 0.0))


class RecordTest(TreeCase):
    def test_nigel_environment_supplies_task_tier_model_and_run(self):
        env = self.make_env('func_A-1', match_lines=['base_1.c 60.000%', 'base_2.c 70.000%'])
        environ = {
            'NIGEL_TASK_NAME': 'decompile-tier2',
            'NIGEL_RUN_ID': 'run-7',
            'NIGEL_AGENT_FLAGS': '--model anthropic/claude-haiku-5-5:xhigh --verbose',
        }
        rec = dl.build_record(self.root, env, 'func_A', 'near_miss', 'stuck at 70%',
                              records=[], environ=environ, now=NOW)
        self.assertEqual(rec['tier'], 2)
        self.assertEqual(rec['task'], 'decompile-tier2')
        self.assertEqual(rec['run_id'], 'run-7')
        self.assertEqual(rec['model'], 'anthropic/claude-haiku-5-5:xhigh')
        self.assertEqual(rec['attempts'], 2)
        self.assertEqual(rec['best_pct'], 70.0)
        self.assertEqual(rec['outcome'], 'near_miss')
        self.assertEqual(rec['env'], 'nonmatchings/func_A-1')
        self.assertEqual(rec['worktree'], str(self.root))
        self.assertEqual(rec['function'], 'func_A')

    def test_run_file_supplies_tier_when_environment_is_silent(self):
        env = self.make_env('func_A-1', run={'function': 'func_A', 'tier': 2, 'model': 'm:xhigh',
                                            'run_id': 'r9', 'task': 'decompile-tier2'})
        rec = dl.build_record(self.root, env, 'func_A', 'stalled', 'plateau',
                              records=[], environ={}, now=NOW)
        self.assertEqual((rec['tier'], rec['model'], rec['run_id']), (2, 'm:xhigh', 'r9'))

    def test_ledger_tier_is_the_fallback_without_run_information(self):
        env = self.make_env('func_A-1')
        records = [record('func_A', 'nonmatchings/func_A-0', 1)]
        rec = dl.build_record(self.root, env, 'func_A', 'timeout', 'out of time',
                              records=records, environ={}, now=NOW)
        self.assertEqual(rec['tier'], 2)
        self.assertIsNone(rec['model'])
        self.assertIsNone(rec['task'])

    def test_model_is_null_when_flags_name_no_model(self):
        env = self.make_env('func_A-1')
        rec = dl.build_record(self.root, env, 'func_A', 'gave_up', 'no idea',
                              records=[], environ={'NIGEL_AGENT_FLAGS': '--thinking xhigh'}, now=NOW)
        self.assertIsNone(rec['model'])

    def test_run_file_for_another_function_is_rejected(self):
        env = self.make_env('func_A-1', run={'function': 'func_B', 'tier': 1})
        with self.assertRaises(dl.LedgerError):
            dl.build_record(self.root, env, 'func_A', 'timeout', 'x', records=[], environ={}, now=NOW)

    def test_reason_must_be_one_line(self):
        env = self.make_env('func_A-1')
        with self.assertRaises(dl.LedgerError):
            dl.build_record(self.root, env, 'func_A', 'timeout', 'two\nlines',
                            records=[], environ={}, now=NOW)

    def test_unknown_outcome_is_rejected(self):
        env = self.make_env('func_A-1')
        with self.assertRaises(dl.LedgerError):
            dl.build_record(self.root, env, 'func_A', 'done', 'x', records=[], environ={}, now=NOW)

    def test_matched_without_match_log_records_full_score(self):
        # build.sh logs base_N attempts only; a function matched by the m2c
        # baseline (base.c) has no match_log.txt but is a verified 100% match.
        env = self.make_env('func_A-1')
        rec = dl.build_record(self.root, env, 'func_A', 'matched', 'baseline matched',
                              records=[], environ={}, now=NOW)
        self.assertEqual((rec['attempts'], rec['best_pct']), (0, 100.0))

    def test_failure_keeps_logged_best(self):
        env = self.make_env('func_A-1')
        rec = dl.build_record(self.root, env, 'func_A', 'stalled', 'plateau',
                              records=[], environ={}, now=NOW)
        self.assertEqual(rec['best_pct'], 0.0)


class WriteRunTest(TreeCase):
    def test_task_maps_to_budget_and_deadline_comes_from_nigel(self):
        env = self.make_env('func_A-1')
        environ = {
            'NIGEL_TASK_NAME': 'decompile-tier2',
            'NIGEL_RUN_ID': 'run-1',
            'NIGEL_AGENT_FLAGS': '--model anthropic/claude-haiku-5-5:xhigh',
            'NIGEL_TIMEOUT_DEADLINE_UNIX': '1700007200',
        }
        run = dl.write_run(env, 'func_A', environ, fallback_tier=1, now=NOW)
        self.assertEqual(run['tier'], 2)
        self.assertEqual(run['stall_limit'], dl.TIER_BUDGETS[2].stall_limit)
        self.assertEqual(run['deadline_unix'], 1700007200.0)
        self.assertEqual(run['model'], 'anthropic/claude-haiku-5-5:xhigh')
        self.assertEqual(dl.read_run(env)['tier'], 2)

    def test_deadline_falls_back_to_the_task_budget(self):
        env = self.make_env('func_A-1')
        environ = {'NIGEL_TASK_NAME': 'decompile-tier3', 'NIGEL_RUN_ID': 'run-2'}
        run = dl.write_run(env, 'func_A', environ, fallback_tier=1, now=NOW)
        self.assertEqual(run['tier'], 3)
        self.assertEqual(run['stall_limit'], dl.TIER_BUDGETS[3].stall_limit)
        self.assertEqual(run['deadline_unix'], NOW + 240 * 60)

    def test_unmapped_task_uses_fallback_tier_and_no_deadline_without_run_id(self):
        env = self.make_env('func_A-1')
        run = dl.write_run(env, 'func_A', {}, fallback_tier=2, now=NOW)
        self.assertEqual(run['tier'], 2)
        self.assertEqual(run['stall_limit'], dl.TIER_BUDGETS[2].stall_limit)
        self.assertIsNone(run['deadline_unix'])
        self.assertIsNone(run['run_id'])

    def test_human_tier_has_no_stall_override(self):
        env = self.make_env('func_A-1')
        run = dl.write_run(env, 'func_A', {}, fallback_tier='human', now=NOW)
        self.assertEqual(run['tier'], 'human')
        self.assertIsNone(run['stall_limit'])


class HarvestTest(TreeCase):
    def run_info(self, deadline, run_id='run-1', tier=1):
        return {'function': 'func_A', 'task': 'decompile-similar', 'tier': tier,
                'model': None, 'run_id': run_id, 'started': '2023-11-14T00:00:00+00:00',
                'deadline_unix': deadline, 'stall_limit': 20}

    def test_backfills_timeout_for_a_run_past_its_deadline(self):
        self.make_env('func_A-1', run=self.run_info(NOW - 1))
        added = dl.harvest(self.root, self.ledger, now=NOW, integrated=set())
        self.assertEqual(len(added), 1)
        self.assertEqual(added[0]['outcome'], 'timeout')
        self.assertEqual(added[0]['env'], 'nonmatchings/func_A-1')
        self.assertIn('harvested', added[0]['reason'])
        self.assertEqual(dl.read_records(self.ledger), added)

    def test_leaves_runs_inside_their_deadline_alone(self):
        self.make_env('func_A-1', run=self.run_info(NOW + 60))
        self.assertEqual(dl.harvest(self.root, self.ledger, now=NOW, integrated=set()), [])
        self.assertFalse(self.ledger.exists())

    def test_skips_environments_without_a_nigel_run(self):
        self.make_env('func_A-1', run=self.run_info(NOW - 1, run_id=None))
        self.assertEqual(dl.harvest(self.root, self.ledger, now=NOW, integrated=set()), [])

    def test_is_idempotent(self):
        self.make_env('func_A-1', run=self.run_info(NOW - 1))
        dl.harvest(self.root, self.ledger, now=NOW, integrated=set())
        self.assertEqual(dl.harvest(self.root, self.ledger, now=NOW, integrated=set()), [])
        self.assertEqual(len(dl.read_records(self.ledger)), 1)

    def test_integrated_function_is_recorded_as_matched_before_the_deadline(self):
        self.make_env('func_A-1', run=self.run_info(NOW + 60))
        added = dl.harvest(self.root, self.ledger, now=NOW, integrated={'func_A'})
        self.assertEqual([r['outcome'] for r in added], ['matched'])

    def test_note_past_deadline_is_recorded_as_near_miss(self):
        self.make_env('func_A-1', run=self.run_info(NOW - 1), near_miss=True)
        added = dl.harvest(self.root, self.ledger, now=NOW, integrated=set())
        self.assertEqual([r['outcome'] for r in added], ['near_miss'])

    def test_stall_in_match_log_is_recorded_as_stalled_before_the_deadline(self):
        info = self.run_info(NOW + 60)
        info['stall_limit'] = 2
        self.make_env('func_A-1', run=info,
                      match_lines=['base_1.c 80.000%', 'base_2.c 70.000%', 'base_3.c 80.000%'])
        added = dl.harvest(self.root, self.ledger, now=NOW, integrated=set())
        self.assertEqual([r['outcome'] for r in added], ['stalled'])
        self.assertEqual(added[0]['attempts'], 3)

    def test_progress_inside_the_stall_limit_is_left_alone(self):
        info = self.run_info(NOW + 60)
        info['stall_limit'] = 2
        self.make_env('func_A-1', run=info,
                      match_lines=['base_1.c 80.000%', 'base_2.c 70.000%', 'base_3.c 81.000%'])
        self.assertEqual(dl.harvest(self.root, self.ledger, now=NOW, integrated=set()), [])


class LedgerIoTest(TreeCase):
    def test_append_then_read_round_trips(self):
        rec = record('func_A', 'nonmatchings/func_A-1', 1)
        dl.append_record(self.ledger, rec)
        dl.append_record(self.ledger, rec)
        self.assertEqual(dl.read_records(self.ledger), [rec, rec])

    def test_missing_ledger_reads_as_empty(self):
        self.assertEqual(dl.read_records(self.ledger), [])

    def test_malformed_line_raises_with_its_line_number(self):
        self.ledger.parent.mkdir(parents=True)
        self.ledger.write_text(json.dumps(record('func_A', 'e', 1)) + '\n{bad\n')
        with self.assertRaises(dl.LedgerError) as ctx:
            dl.read_records(self.ledger)
        self.assertIn(':2:', str(ctx.exception))

    def test_record_missing_a_field_is_rejected(self):
        self.ledger.parent.mkdir(parents=True)
        partial = record('func_A', 'e', 1)
        del partial['best_pct']
        self.ledger.write_text(json.dumps(partial) + '\n')
        with self.assertRaises(dl.LedgerError):
            dl.read_records(self.ledger)

    def test_environment_variable_overrides_the_ledger_path(self):
        target = self.root / 'elsewhere.jsonl'
        self.assertEqual(dl.ledger_path(environ={dl.LEDGER_ENV: str(target)}), target)

    def test_default_ledger_lives_in_the_git_common_dir(self):
        subprocess.run(['git', 'init', '-q', str(self.root)], check=True)
        got = dl.ledger_path(environ={}, cwd=self.root)
        self.assertEqual(got.resolve(), (self.root / '.git' / 'mg64' / 'difficulty.jsonl').resolve())


class CliShowTest(TreeCase):
    def test_show_filters_by_tier(self):
        records = [record('func_A', 'nonmatchings/func_A-1', 1),
                   record('func_B', 'nonmatchings/func_B-1', 2, 95.0)]
        for rec in records:
            dl.append_record(self.ledger, rec)
        tiers = dl.failed_tiers(self.root, records)
        self.assertEqual(tiers, {'func_A': 2, 'func_B': 3})
        self.assertEqual(dl.shown_rows(self.root, records, tier='3')[0]['function'], 'func_B')
        self.assertEqual(dl.shown_rows(self.root, records, tier=None)[0]['function'], 'func_A')


if __name__ == '__main__':
    unittest.main()
