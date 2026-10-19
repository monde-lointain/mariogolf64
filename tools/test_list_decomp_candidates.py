#!/usr/bin/env python3
"""Tests for list_decomp_candidates.py. Hermetic: every tree is built in a tempdir."""

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from list_decomp_candidates import collect_candidates, find_include_asm_in_text  # noqa: E402


class FindIncludeAsmTest(unittest.TestCase):
    def test_reports_line_and_asm_path(self):
        text = (
            '#include "common.h"\n'
            'INCLUDE_ASM("asm/nonmatchings/main/a", func_one);\n'
            '\n'
            'void f(void) {}\n'
            'INCLUDE_ASM("asm/nonmatchings/main/a", func_two);\n'
        )
        rows = find_include_asm_in_text(text, 'src/main/a.c')
        self.assertEqual(
            [(r.line, r.func, r.asm_rel) for r in rows],
            [
                (2, 'func_one', 'asm/nonmatchings/main/a/func_one.s'),
                (5, 'func_two', 'asm/nonmatchings/main/a/func_two.s'),
            ],
        )

    def test_ignores_commented_out_statements(self):
        text = (
            '/* INCLUDE_ASM("asm/nonmatchings/main/a", old_one); */\n'
            '// INCLUDE_ASM("asm/nonmatchings/main/a", old_two);\n'
        )
        self.assertEqual(find_include_asm_in_text(text, 'src/main/a.c'), [])


class CollectCandidatesTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.src = self.write(
            'src/main/a.c',
            'INCLUDE_ASM("asm/nonmatchings/main/a", big);\n'
            'INCLUDE_ASM("asm/nonmatchings/main/a", small);\n'
            'INCLUDE_ASM("asm/nonmatchings/main/a", missing_asm);\n'
            'INCLUDE_ASM("asm/nonmatchings/main/a", parked);\n',
        )
        self.write('asm/nonmatchings/main/a/big.s', 'x' * 300)
        self.write('asm/nonmatchings/main/a/small.s', 'x' * 10)
        self.write('asm/nonmatchings/main/a/parked.s', 'x' * 50)
        self.write('nonmatchings/parked/NEAR_MISS.md', 'close but no cigar\n')

    def write(self, rel, text):
        path = self.root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def test_orders_by_size_with_missing_asm_last(self):
        rows, total, near_miss_skipped = collect_candidates(
            [self.src], include_near_misses=True, root=self.root
        )
        self.assertEqual(total, 4)
        self.assertEqual(near_miss_skipped, 0)
        self.assertEqual(
            [(r['func'], r['asm_size'], r['near_miss']) for r in rows],
            [
                ('small', 10, False),
                ('parked', 50, True),
                ('big', 300, False),
                ('missing_asm', -1, False),
            ],
        )

    def test_skips_near_misses_by_default(self):
        rows, total, near_miss_skipped = collect_candidates(
            [self.src], include_near_misses=False, root=self.root
        )
        self.assertEqual(total, 4)
        self.assertEqual(near_miss_skipped, 1)
        self.assertNotIn('parked', [r['func'] for r in rows])


if __name__ == '__main__':
    unittest.main()
