#!/usr/bin/env python3
"""Tests that the decompile selectors agree on which functions are tier 1, using one ledger rule."""

import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parent))

import difficulty_ledger as dl  # noqa: E402
import list_decomp_candidates as ldc  # noqa: E402
import score_functions as sf  # noqa: E402


def fn(name):
    return SimpleNamespace(name=name)


class SelectTierTest(unittest.TestCase):
    scored = [(fn('func_A'), 0.1), (fn('func_B'), 0.2), (fn('func_C'), 0.3), (fn('func_D'), 0.4)]
    tiers = {'func_B': 2, 'func_C': 'human'}

    def test_tier_one_drops_every_function_with_a_failure(self):
        kept = sf.select_tier(self.scored, self.tiers, 1)
        self.assertEqual([f.name for f, _ in kept], ['func_A', 'func_D'])

    def test_tier_two_keeps_only_tier_two(self):
        kept = sf.select_tier(self.scored, self.tiers, 2)
        self.assertEqual([f.name for f, _ in kept], ['func_B'])

    def test_tier_three_is_empty_when_nothing_qualifies(self):
        kept = sf.select_tier(self.scored, self.tiers, 3)
        self.assertEqual(kept, [])

    def test_order_and_scores_are_preserved(self):
        kept = sf.select_tier(self.scored, {}, 1)
        self.assertEqual(kept, self.scored)


class ListCandidatesAgreementTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name).resolve()
        src = self.root / 'src' / 'main'
        src.mkdir(parents=True)
        (src / 'x.c').write_text(
            'INCLUDE_ASM("asm/nonmatchings/main", func_A);\n'
            'INCLUDE_ASM("asm/nonmatchings/main", func_B);\n'
            'INCLUDE_ASM("asm/nonmatchings/main", func_C);\n'
            'INCLUDE_ASM("asm/nonmatchings/main", func_D);\n'
        )
        # A note in a --id style environment must count, not only nonmatchings/<func>/.
        (self.root / 'nonmatchings' / 'func_B-7').mkdir(parents=True)
        (self.root / 'nonmatchings' / 'func_B-7' / dl.NEAR_MISS_NOTE).write_text('x\n')
        self.source = src / 'x.c'
        self.names = ['func_A', 'func_B', 'func_C', 'func_D']

    def tearDown(self):
        self._tmp.cleanup()

    def tiers(self):
        records = [{'function': 'func_C', 'env': 'nonmatchings/func_C-1', 'tier': 1,
                    'best_pct': 0.0, 'outcome': 'timeout', 'ts': '2024-01-01T00:00:00+00:00',
                    'reason': 'r', 'attempts': 0, 'model': None, 'run_id': None, 'task': None,
                    'worktree': str(self.root)}]
        return dl.failed_tiers(self.root, records)

    def test_default_listing_is_exactly_the_tier_one_set(self):
        tiers = self.tiers()
        rows, _, failed_skipped = ldc.collect_candidates([self.source], False, self.root, tiers)
        listed = {r['func'] for r in rows}
        tier_one = {n for n in self.names if dl.tier_of(n, tiers) == 1}
        self.assertEqual(listed, tier_one)
        self.assertEqual(listed, {'func_A', 'func_D'})
        self.assertEqual(failed_skipped, 2)

    def test_score_functions_tier_one_matches_the_list_default(self):
        tiers = self.tiers()
        scored = [(fn(n), 0.0) for n in self.names]
        kept = sf.select_tier(scored, tiers, 1)
        rows, _, _ = ldc.collect_candidates([self.source], False, self.root, tiers)
        self.assertEqual({f.name for f, _ in kept}, {r['func'] for r in rows})

    def test_include_flag_lists_failed_functions_with_their_tier(self):
        tiers = self.tiers()
        rows, _, failed_skipped = ldc.collect_candidates([self.source], True, self.root, tiers)
        self.assertEqual({r['func']: r['tier'] for r in rows if r['tier'] != 1},
                         {'func_B': 2, 'func_C': 2})
        self.assertEqual(failed_skipped, 0)


if __name__ == '__main__':
    unittest.main()
