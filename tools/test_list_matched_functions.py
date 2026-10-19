#!/usr/bin/env python3
"""Tests for list_matched_functions.py --by-reference-use ordering."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import list_matched_functions as lmf  # noqa: E402


class OrderByReferenceUseTest(unittest.TestCase):
    def test_most_referenced_first_then_input_order(self):
        names = ['M1', 'M2', 'M3', 'M4']
        rows = [['U1', 'M3', 'M1'], ['U2', 'M3', 'M2'], ['U3', 'M3', 'M1']]
        self.assertEqual(lmf.order_by_reference_use(names, rows), ['M3', 'M1', 'M2', 'M4'])

    def test_ties_keep_input_order(self):
        names = ['M4', 'M2']
        rows = [['U1', 'M2'], ['U2', 'M4']]
        self.assertEqual(lmf.order_by_reference_use(names, rows), ['M4', 'M2'])

    def test_a_reference_counts_once_per_target(self):
        names = ['M1', 'M2']
        rows = [['U1', 'M2', 'M2'], ['U2', 'M1']]
        self.assertEqual(lmf.order_by_reference_use(names, rows), ['M1', 'M2'])

    def test_output_contains_only_the_given_names(self):
        names = ['M1']
        rows = [['U1', 'M9', 'M1']]
        self.assertEqual(lmf.order_by_reference_use(names, rows), ['M1'])


if __name__ == '__main__':
    unittest.main()
