#!/usr/bin/env python3
"""Tests for tools/m2ctx.py's removal of GCC nested functions from the m2c context.

m2c parses its context with pycparser, which rejects GCC nested function definitions. The
context must still parse, so the nested definitions are dropped and the enclosing function and
its control-flow blocks are kept. Needs the venv interpreter (pycparser).
"""

import unittest

import pycparser

import m2ctx

NESTED_SOURCE = """\
typedef int s32;
typedef unsigned short u16;
s32 outer(s32 n) {
    s32 total;
    void step(void) {
        total += n;
    }
    inline s32 twice(s32 v) {
        return v * 2;
    }
    if (n > 0) {
        step();
    }
    while (total < 0) {
        total += twice(1);
    }
    return total;
}
s32 after(s32 n) {
    return n;
}
"""


def parses(text: str) -> bool:
    try:
        pycparser.CParser().parse(text)
    except pycparser.c_parser.ParseError:
        return False
    return True


class StripNestedFunctionsTest(unittest.TestCase):
    def test_nested_source_does_not_parse_before_stripping(self):
        self.assertFalse(parses(NESTED_SOURCE))

    def test_stripped_context_parses(self):
        self.assertTrue(parses(m2ctx.strip_nested_functions(NESTED_SOURCE)))

    def test_nested_definitions_removed_and_blocks_kept(self):
        out = m2ctx.strip_nested_functions(NESTED_SOURCE)
        self.assertNotIn("void step(void) {", out)
        self.assertNotIn("inline s32 twice", out)
        self.assertIn("s32 outer(s32 n) {", out)
        self.assertIn("if (n > 0) {", out)
        self.assertIn("while (total < 0) {", out)
        self.assertIn("s32 after(s32 n) {", out)

    def test_top_level_text_unchanged(self):
        plain = "typedef int s32;\ns32 f(s32 n) {\n    return n;\n}\n"
        self.assertEqual(m2ctx.strip_nested_functions(plain), plain)


if __name__ == "__main__":
    unittest.main()
