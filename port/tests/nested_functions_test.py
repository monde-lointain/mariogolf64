"""Tests the move of nested functions to file scope (nested_functions.py).

The checks on text pin down what the move writes; those that compile and
run its output, with the C compiler that --compiler names, show that it
keeps the program's meaning, writes through captures included.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "cmake"))

from nested_functions import NestedFunctionError, hoist_nested_functions  # noqa: E402

COMPILER: str | None = None

# A captured parameter's pointer, spelled through the type that the
# parameter has, which an array parameter's declaration doesn't give
INT_PARAMETER = "__typeof__(0 ? *(int (*))0 : *(int (*))0)"


def hoist(text: str) -> str:
    return hoist_nested_functions(text, "game.c")


def code_lines(text: str) -> list[str]:
    """The text's lines without line markers or blank lines."""
    return [
        line.strip() for line in text.splitlines() if line.strip() and not line.startswith("#line")
    ]


class TestUnchangedSources(unittest.TestCase):
    def test_without_nested_functions(self):
        text = (
            "#include <x.h>\nstatic int f_Count;\nint Add(int a, int b) {\n    return a + b;\n}\n"
        )
        self.assertEqual(hoist(text), text)

    def test_empty_source(self):
        self.assertEqual(hoist(""), "")

    def test_initializer_braces(self):
        text = "int f_Table[2] = { 1, 2 };\nvoid Run(void) {\n    int a[2] = { 3, 4 };\n}\n"
        self.assertEqual(hoist(text), text)


class TestMovedFunctions(unittest.TestCase):
    def test_pure_function(self):
        text = (
            "int Outer(int x) {\n"
            "    int Twice(int y) {\n"
            "        return y * 2;\n"
            "    }\n"
            "    return Twice(x);\n"
            "}\n"
        )
        lines = code_lines(hoist(text))
        self.assertIn("static int Twice(int y);", lines)
        self.assertIn("return Twice(x);", lines)
        self.assertLess(lines.index("static int Twice(int y) {"), lines.index("int Outer(int x) {"))

    def test_scalar_capture(self):
        text = (
            "int Outer(int x) {\n"
            "    int total = 0;\n"
            "    void Add(int y) {\n"
            "        total += y + x;\n"
            "    }\n"
            "    Add(1);\n"
            "    return total;\n"
            "}\n"
        )
        out = hoist(text)
        self.assertIn("static void Add(int y, int (*total), {} (*x)) {{".format(INT_PARAMETER), out)
        self.assertIn("(*total) += y + (*x);", out)
        self.assertIn("Add(1, &total, &x);", out)

    def test_array_capture(self):
        text = (
            "float Outer(void) {\n"
            "    float corners[3][2];\n"
            "    float First(void) {\n"
            "        return corners[0][1];\n"
            "    }\n"
            "    return First();\n"
            "}\n"
        )
        out = hoist(text)
        self.assertIn("static float First(float (*corners) [ 3 ] [ 2 ]) {", out)
        self.assertIn("return (*corners)[0][1];", out)
        self.assertIn("return First(&corners);", out)

    def test_captures_in_sorted_order(self):
        text = (
            "void Outer(int b, int a) {\n"
            "    void Use(void) {\n"
            "        a = b;\n"
            "    }\n"
            "    Use();\n"
            "}\n"
        )
        self.assertIn("Use(&a, &b);", hoist(text))

    def test_member_named_as_capture(self):
        text = (
            "struct P { int x; };\n"
            "int Outer(struct P *p, int x) {\n"
            "    int Get(void) {\n"
            "        return p->x + x;\n"
            "    }\n"
            "    return Get();\n"
            "}\n"
        )
        self.assertIn("return (*p)->x + (*x);", hoist(text))

    def test_lines_kept(self):
        text = (
            "int Outer(int x) {\n"
            "    int Twice(void) {\n"
            "        return x * 2;\n"
            "    }\n"
            "    return Twice();\n"
            "}\n"
        )
        out = hoist(text)
        # Each piece's marker gives its first line the source's number
        markers = [
            (m.start(), int(m.group(1))) for m in re.finditer(r'^#line (\d+) "game.c"$', out, re.M)
        ]
        self.assertEqual([line for _, line in markers], [2, 1])
        after_parent_marker = out[markers[-1][0] :].splitlines()[1:]
        self.assertEqual(after_parent_marker[4].strip(), "return Twice(&x);")

    def test_auto_declaration_removed(self):
        text = (
            "int Outer(int x) {\n"
            "    auto inline int Get(void);\n"
            "    int y = Get();\n"
            "    inline int Get(void) {\n"
            "        return x;\n"
            "    }\n"
            "    return y;\n"
            "}\n"
        )
        out = hoist(text)
        self.assertNotIn("auto", out)
        self.assertIn("static inline int Get({} (*x));".format(INT_PARAMETER), out)
        self.assertIn("int y = Get(&x);", out)


class TestCapturesBetweenNestedFunctions(unittest.TestCase):
    def test_callee_captures_passed_through(self):
        text = (
            "int Outer(int x) {\n"
            "    int Inner(void) {\n"
            "        return x;\n"
            "    }\n"
            "    int Middle(void) {\n"
            "        return Inner() + 1;\n"
            "    }\n"
            "    return Middle();\n"
            "}\n"
        )
        out = hoist(text)
        self.assertIn("static int Middle({} (*x)) {{".format(INT_PARAMETER), out)
        self.assertIn("return Inner(x) + 1;", out)
        self.assertIn("return Middle(&x);", out)

    def test_two_deep(self):
        text = (
            "int Outer(int x) {\n"
            "    int Middle(int y) {\n"
            "        int Inner(void) {\n"
            "            return x + y;\n"
            "        }\n"
            "        return Inner();\n"
            "    }\n"
            "    return Middle(1);\n"
            "}\n"
        )
        out = hoist(text)
        self.assertIn("static int Middle(int y, {} (*x)) {{".format(INT_PARAMETER), out)
        self.assertIn("return Middle(1, &x);", out)
        # Inner takes Middle's pointer by its address, and y's
        self.assertIn("return (*(*x)) + (*y);", out)
        self.assertIn("return Inner(&x, &y);", out)

    def test_inner_parameter_not_captured(self):
        text = (
            "int Outer(int x) {\n"
            "    int Middle(void) {\n"
            "        int Inner(int x) {\n"
            "            return x;\n"
            "        }\n"
            "        return Inner(1);\n"
            "    }\n"
            "    return Middle() + x;\n"
            "}\n"
        )
        out = hoist(text)
        self.assertIn("static int Middle(void) {", out)
        self.assertIn("return Middle() + x;", out)


class TestConditionals(unittest.TestCase):
    def test_captures_passed_in_later_branch(self):
        text = (
            "int Outer(int x) {\n"
            "    int Get(void) {\n"
            "        return x;\n"
            "    }\n"
            "#ifdef FIRST\n"
            "    return Get();\n"
            "#else\n"
            "    return Get() + 1;\n"
            "#endif\n"
            "}\n"
        )
        out = hoist(text)
        self.assertIn("return Get(&x);", out)
        self.assertIn("return Get(&x) + 1;", out)

    def test_parent_header_in_branches(self):
        text = (
            "#ifdef FIRST\n"
            "int Outer(int x) {\n"
            "#else\n"
            "int Outer(int x, int unused) {\n"
            "#endif\n"
            "    int Get(void) {\n"
            "        return x;\n"
            "    }\n"
            "    return Get();\n"
            "}\n"
        )
        out = hoist(text)
        # The moved function goes before the conditional, not inside it
        self.assertLess(out.index("static int Get("), out.index("#ifdef FIRST"))
        self.assertIn("return Get(&x);", out)

    def test_nested_function_in_conditional(self):
        text = (
            "int Outer(int x) {\n"
            "#ifdef FIRST\n"
            "    int Get(void) {\n"
            "        return x;\n"
            "    }\n"
            "#endif\n"
            "    return 0;\n"
            "}\n"
        )
        with self.assertRaisesRegex(NestedFunctionError, "conditional"):
            hoist(text)


class TestMacros(unittest.TestCase):
    def test_macro_call_passes_captures(self):
        text = (
            "#define ADD_TWICE(n) \\\n"
            "    Add(n); \\\n"
            "    Add(n)\n"
            "int Outer(void) {\n"
            "    int total = 0;\n"
            "    void Add(int y) {\n"
            "        total += y;\n"
            "    }\n"
            "    ADD_TWICE(2);\n"
            "    return total;\n"
            "}\n"
        )
        out = hoist(text)
        self.assertIn("    Add(n, &total); \\\n    Add(n, &total)\n", out)

    def test_macro_naming_capture_in_nested_function(self):
        text = (
            "#define TOTAL total\n"
            "int Outer(void) {\n"
            "    int total = 0;\n"
            "    void Add(int y) {\n"
            "        TOTAL += y;\n"
            "        total += y;\n"
            "    }\n"
            "    Add(1);\n"
            "    return total;\n"
            "}\n"
        )
        with self.assertRaisesRegex(NestedFunctionError, "names its parent's total"):
            hoist(text)


class TestErrors(unittest.TestCase):
    def test_address_taken(self):
        text = (
            "int Outer(int x) {\n"
            "    int Get(void) {\n"
            "        return x;\n"
            "    }\n"
            "    int (*get)(void) = Get;\n"
            "    return get();\n"
            "}\n"
        )
        with self.assertRaisesRegex(NestedFunctionError, "address is taken"):
            hoist(text)

    def test_capture_shadowed_by_caller(self):
        text = (
            "int Outer(int x) {\n"
            "    int Inner(void) {\n"
            "        return x;\n"
            "    }\n"
            "    int Middle(void) {\n"
            "        int x = 2;\n"
            "        return Inner() + x;\n"
            "    }\n"
            "    return Middle();\n"
            "}\n"
        )
        with self.assertRaisesRegex(NestedFunctionError, "declares a x of its own"):
            hoist(text)

    def test_unbalanced_braces(self):
        with self.assertRaisesRegex(NestedFunctionError, "unbalanced"):
            hoist("int Outer(void) {\n    int Get(void) {\n        return 1;\n}\n")

    def test_unended_conditional(self):
        with self.assertRaisesRegex(NestedFunctionError, "without #endif"):
            hoist("#ifdef A\nint x;\n")

    def test_stray_endif(self):
        with self.assertRaisesRegex(NestedFunctionError, "without #if"):
            hoist("#endif\n")


@unittest.skipIf(COMPILER is None, "no compiler given")
class TestCompiledMeaning(unittest.TestCase):
    """Compiles and runs the moved functions, whose results print."""

    def run_program(self, text: str) -> str:
        with tempfile.TemporaryDirectory() as directory:
            source = pathlib.Path(directory, "program.c")
            program = pathlib.Path(directory, "program")
            source.write_text(hoist(text))
            subprocess.run([COMPILER, "-std=gnu11", "-o", str(program), str(source)], check=True)
            return subprocess.run([str(program)], check=True, capture_output=True, text=True).stdout

    def test_writes_seen_on_both_sides(self):
        text = (
            "#include <stdio.h>\n"
            "int main(void) {\n"
            "    int count = 1;\n"
            "    int values[3] = { 0, 0, 0 };\n"
            "    void Record(int value) {\n"
            "        values[count++] = value;\n"
            "    }\n"
            "    void RecordTwice(int value) {\n"
            "        Record(value);\n"
            "        Record(value + count);\n"
            "    }\n"
            "    RecordTwice(5);\n"
            "    count = 0;\n"
            "    Record(9);\n"
            '    printf("%d %d %d %d\\n", count, values[0], values[1], values[2]);\n'
            "    return 0;\n"
            "}\n"
        )
        self.assertEqual(self.run_program(text), "1 9 5 7\n")

    def test_two_deep(self):
        text = (
            "#include <stdio.h>\n"
            "int main(void) {\n"
            "    int total = 0;\n"
            "    void AddTwice(int y) {\n"
            "        void Add(void) {\n"
            "            total += y;\n"
            "        }\n"
            "        Add();\n"
            "        y++;\n"
            "        Add();\n"
            "    }\n"
            "    AddTwice(3);\n"
            '    printf("%d\\n", total);\n'
            "    return 0;\n"
            "}\n"
        )
        self.assertEqual(self.run_program(text), "7\n")

    def test_parameter_arrays(self):
        text = (
            "#include <stdio.h>\n"
            "typedef int Row[2];\n"
            "static int Sum(Row rows[], int list[2]) {\n"
            "    int Corner(void) {\n"
            "        return rows[1][1] + list[1];\n"
            "    }\n"
            "    return Corner();\n"
            "}\n"
            "int main(void) {\n"
            "    Row rows[2] = { { 1, 2 }, { 3, 4 } };\n"
            "    int list[2] = { 10, 20 };\n"
            '    printf("%d\\n", Sum(rows, list));\n'
            "    return 0;\n"
            "}\n"
        )
        self.assertEqual(self.run_program(text), "24\n")


def main() -> None:
    global COMPILER
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler")
    arguments, rest = parser.parse_known_args()
    COMPILER = arguments.compiler
    TestCompiledMeaning.__unittest_skip__ = COMPILER is None
    unittest.main(argv=[sys.argv[0]] + rest)


if __name__ == "__main__":
    main()
