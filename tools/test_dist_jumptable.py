#!/usr/bin/env python3
"""Tests for jump-table reference normalization in tools/decomp-env-template/dist.py.

A target's jump-table reference (jtbl_<overlay>_<hex>) and a candidate's .rodata reference name the
same table; the scorer must treat them as equal or a function with a switch can never reach an
exact match. The loader imports dist.py by path because the template is not a package.
"""

import importlib.util
import unittest
from pathlib import Path

TEMPLATE = Path(__file__).resolve().parent / 'decomp-env-template'


def load_dist():
    spec = importlib.util.spec_from_file_location('dist', TEMPLATE / 'dist.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


dist = load_dist()


class JumpTableNormalizationTest(unittest.TestCase):
    def test_overlay_named_table_normalizes_to_rodata(self):
        line = 'lw    v0,%lo(jtbl_ovl11_8024C738)(at)'
        self.assertEqual(dist.normalize_jumptable_references(line), 'lw    v0,%lo(.rodata)(at)')

    def test_hex_named_table_still_normalizes_to_rodata(self):
        line = 'lui    at,%hi(jtbl_8009E998_9F598)'
        self.assertEqual(dist.normalize_jumptable_references(line), 'lui    at,%hi(.rodata)')

    def test_plain_symbol_is_left_alone(self):
        line = 'lui    at,%hi(game_mode)'
        self.assertEqual(dist.normalize_jumptable_references(line), line)


class JumpTableScoringTest(unittest.TestCase):
    def test_overlay_table_reference_against_rodata_scores_exact(self):
        target = [
            'lui    at,%hi(jtbl_ovl11_8024C738)',
            'lw    v0,%lo(jtbl_ovl11_8024C738)(at)',
            'jr    ra',
        ]
        candidate = [
            'lui    at,%hi(.rodata)',
            'lw    v0,%lo(.rodata)(at)',
            'jr    ra',
        ]
        score, _, _, exact = dist.score_files(target, candidate)
        self.assertEqual(score, 0)
        self.assertTrue(exact)

    def test_different_plain_symbol_still_scores_nonzero(self):
        target = ['lui    at,%hi(game_mode)', 'jr    ra']
        candidate = ['lui    at,%hi(D_800C4140)', 'jr    ra']
        score, _, _, exact = dist.score_files(target, candidate)
        self.assertGreater(score, 0)
        self.assertFalse(exact)


if __name__ == '__main__':
    unittest.main()
