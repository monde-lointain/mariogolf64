#!/usr/bin/env python3
"""Tests for m2c_one_shot.py pure helpers. Hermetic: no m2c, compiler, or ROM needed."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from m2c_one_shot import first_error_line, m2c_failure_reason, replace_include_asm  # noqa: E402

ASM_REL = 'asm/nonmatchings/main/a/func_one.s'
STATEMENT = 'INCLUDE_ASM("asm/nonmatchings/main/a", func_one);'


class ReplaceIncludeAsmTest(unittest.TestCase):
    def test_swaps_the_statement_and_keeps_other_lines(self):
        text = f'int x;\n{STATEMENT}\nint y;\n'
        new, error = replace_include_asm(text, ASM_REL, 'func_one', 'void func_one(void) {\n}')
        self.assertEqual(error, '')
        self.assertEqual(new, 'int x;\nvoid func_one(void) {\n}\nint y;\n')

    def test_missing_statement_is_reported(self):
        new, error = replace_include_asm('int x;\n', ASM_REL, 'func_one', 'void f(void) {}')
        self.assertIsNone(new)
        self.assertIn('not found', error)

    def test_commented_statement_is_not_a_target(self):
        text = f'/* {STATEMENT} */\n'
        new, error = replace_include_asm(text, ASM_REL, 'func_one', 'void f(void) {}')
        self.assertIsNone(new)
        self.assertIn('not found', error)

    def test_duplicate_statement_is_refused(self):
        text = f'{STATEMENT}\n{STATEMENT}\n'
        new, error = replace_include_asm(text, ASM_REL, 'func_one', 'void f(void) {}')
        self.assertIsNone(new)
        self.assertIn('more than once', error)

    def test_statement_sharing_a_line_is_refused(self):
        text = f'static int y; {STATEMENT}\n'
        new, error = replace_include_asm(text, ASM_REL, 'func_one', 'void f(void) {}')
        self.assertIsNone(new)
        self.assertIn('not alone', error)


class FailureTextTest(unittest.TestCase):
    def test_m2c_reason_is_first_line_of_failure_comment(self):
        stdout = (
            '/*\n'
            'Decompilation failure in function emit_course_terrain_dl:\n'
            '\n'
            'Found jr instruction at emit_course_terrain_dl.s line 75, but the '
            'corresponding jump table is not provided.\n'
            '\n'
            '*/\n'
        )
        self.assertEqual(
            m2c_failure_reason(stdout, ''),
            'Found jr instruction at emit_course_terrain_dl.s line 75, but the '
            'corresponding jump table is not provided.',
        )

    def test_m2c_reason_prefers_stderr(self):
        self.assertEqual(m2c_failure_reason('/*\n*/\n', 'boom\n'), 'boom')

    def test_m2c_reason_empty_when_no_text(self):
        self.assertEqual(m2c_failure_reason('', ''), '')

    def test_first_error_line_prefers_error_lines(self):
        output = 'base.c:4: parse error before `?\'\nnoise\n'
        self.assertEqual(first_error_line(output), "base.c:4: parse error before `?'")

    def test_first_error_line_falls_back_to_last_line(self):
        self.assertEqual(first_error_line('first\nsecond\n'), 'second')
        self.assertEqual(first_error_line(''), '')


if __name__ == '__main__':
    unittest.main()
