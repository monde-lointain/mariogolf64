"""Tests for check_pointer_arithmetic.py using temporary C trees (no make or ROM needed)."""

import contextlib
import io
import sys
import tempfile
import unittest
from datetime import datetime
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parent))

import check_pointer_arithmetic as checker  # noqa: E402
from pycparser import parse_file  # noqa: E402


ARITHMETIC = "typedef unsigned char u8;\nint f(void *p) { return *((u8*)p + 4); }\n"


def parse_without_make(source: Path):
    """Stand-in for parse_source: the same cpp run, with the compat defines but no profile flags."""
    return parse_file(str(source), use_cpp=True, cpp_path="gcc",
                      cpp_args=["-E", *checker.PYCPARSER_COMPAT_ARGS])


class PointerArithmeticTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name).resolve()
        for patcher in (
            mock.patch.object(checker, "ROOT", self.root),
            mock.patch.object(checker, "parse_source", parse_without_make),
            mock.patch.object(checker, "get_git_blame_timestamp", return_value=None),
        ):
            patcher.start()
            self.addCleanup(patcher.stop)

    def write(self, relative: str, text: str) -> Path:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def check(self, relative: str, text: str) -> list[dict]:
        return checker.check_file(self.write(relative, text))

    def test_flags_cast_pointer_plus_offset(self):
        found = self.check("src/main/a.c", ARITHMETIC)
        self.assertEqual([(v["type"], v["line"]) for v in found], [("u8*", 2)])

    def test_flags_struct_pointer_minus_index(self):
        found = self.check(
            "src/main/a.c",
            "struct Foo { int a; };\nint g(void *p, int i) { return ((struct Foo*)p - i)->a; }\n",
        )
        self.assertEqual([(v["type"], v["line"]) for v in found], [("struct Foo*", 2)])

    def test_flags_inner_cast_in_nested_dereference_once(self):
        found = self.check(
            "src/main/a.c",
            "typedef int s32;\nint f(void *p) { return *(s32*)((char*)p + 0x10); }\n",
        )
        self.assertEqual([v["type"] for v in found], ["char*"])

    def test_ignores_non_pointer_casts_and_plain_arithmetic(self):
        found = self.check(
            "src/main/a.c",
            "int f(int x, int *p) { return (int)x + 1 + *(p + 1); }\n"
            "int g(void *p, void *q) { return (char*)p == q; }\n",
        )
        self.assertEqual(found, [])

    def test_violation_in_included_header_is_attributed_to_header(self):
        header = self.write("src/main/hdr.h", "static int peek(void *p) { return *((unsigned char*)p + 1); }\n")
        found = self.check("src/main/a.c", '#include "hdr.h"\nint g(void *p) { return peek(p); }\n')
        self.assertEqual([Path(v["file"]).resolve() for v in found], [header.resolve()])
        self.assertEqual(found[0]["type"], "unsigned char*")

    def test_unparseable_file_returns_none_and_warns(self):
        source = self.write("src/main/bad.c", "int f( { return ; }\n")
        with contextlib.redirect_stderr(io.StringIO()) as err:
            self.assertIsNone(checker.check_file(source))
        self.assertIn("Could not parse src/main/bad.c", err.getvalue())

    def test_collect_sources_takes_project_owned_c_files_only(self):
        owned = self.write("src/main/a.c", ARITHMETIC)
        self.write("src/main/a.h", "")
        self.write("src/main/notes.txt", "")
        self.write("src/libultra/b.c", ARITHMETIC)
        self.assertEqual(checker.collect_sources([self.root / "src"], include_vendored=False), [owned.resolve()])
        vendored = checker.collect_sources([self.root / "src"], include_vendored=True)
        self.assertEqual([p.name for p in vendored], ["b.c", "a.c"])

    def test_check_files_skips_vendored_violations_unless_requested(self):
        self.write("src/main/a.c", ARITHMETIC)
        self.write("src/libultra/b.c", ARITHMETIC)
        sources = checker.collect_sources([self.root / "src"], include_vendored=False)
        by_file, unparsed = checker.check_files(sources, include_vendored=False, verbose=False)
        self.assertEqual(list(by_file), ["src/main/a.c"])
        self.assertEqual(unparsed, [])

        sources = checker.collect_sources([self.root / "src"], include_vendored=True)
        by_file, _ = checker.check_files(sources, include_vendored=True, verbose=False)
        self.assertEqual(sorted(by_file), ["src/libultra/b.c", "src/main/a.c"])

    def test_header_violation_reported_once_across_including_sources(self):
        self.write("src/main/hdr.h", "static int peek(void *p) { return *((unsigned char*)p + 1); }\n")
        self.write("src/main/a.c", '#include "hdr.h"\nint g(void *p) { return peek(p); }\n')
        self.write("src/main/b.c", '#include "hdr.h"\nint h(void *p) { return peek(p); }\n')
        sources = checker.collect_sources([self.root / "src"], include_vendored=False)
        by_file, unparsed = checker.check_files(sources, include_vendored=False, verbose=False)
        self.assertEqual(list(by_file), ["src/main/hdr.h"])
        self.assertEqual(len(by_file["src/main/hdr.h"]), 1)
        self.assertEqual(unparsed, [])

    def test_format_violation_with_and_without_date(self):
        violation = {"line": 3, "column": 9, "type": "u8*", "timestamp": datetime(2020, 1, 2, 12, 0)}
        self.assertEqual(
            checker.format_violation("src/main/a.c", violation),
            "src/main/a.c:3:9: [2020-01-02] pointer arithmetic with cast: (u8*)ptr + offset",
        )
        violation["timestamp"] = None
        self.assertEqual(
            checker.format_violation("src/main/a.c", violation),
            "src/main/a.c:3:9: pointer arithmetic with cast: (u8*)ptr + offset",
        )

    def test_main_lists_oldest_first_with_unknown_dates_last(self):
        lines = [""] * 40
        lines[0] = "typedef unsigned char u8;"
        for index, line in ((9, 10), (19, 20), (29, 30)):
            lines[index] = f"int f{line}(void *p) {{ return *((u8*)p + {line}); }}"
        self.write("src/main/a.c", "\n".join(lines) + "\n")
        timestamps = {10: datetime(2024, 1, 1), 20: None, 30: datetime(2019, 1, 1)}
        out = io.StringIO()
        with mock.patch.object(checker, "get_git_blame_timestamp", side_effect=lambda f, n: timestamps[n]), \
                contextlib.redirect_stdout(out):
            code = checker.main(["src/main/a.c"])
        self.assertEqual(code, 0)
        report = out.getvalue()
        self.assertIn("Found 3 violation(s) in 1 file(s):", report)
        entries = [line for line in report.splitlines() if "pointer arithmetic" in line]
        self.assertEqual(len(entries), 3)
        self.assertTrue(entries[0].startswith("src/main/a.c:30:") and "[2019-01-01]" in entries[0])
        self.assertTrue(entries[1].startswith("src/main/a.c:10:") and "[2024-01-01]" in entries[1])
        self.assertTrue(entries[2].startswith("src/main/a.c:20:") and "[" not in entries[2])

    def test_main_exit_codes(self):
        self.write("src/main/a.c", ARITHMETIC)
        self.write("src/main/clean.c", "int f(int x) { return x + 1; }\n")
        self.write("src/main/bad.c", "int f( { return ; }\n")
        self.write("src/libultra/b.c", ARITHMETIC)
        quiet = io.StringIO()
        with contextlib.redirect_stdout(quiet), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(checker.main(["src/main/a.c"]), 0)
            self.assertEqual(checker.main(["src/main/a.c", "--strict"]), 1)
            self.assertEqual(checker.main(["src/main/clean.c", "--strict"]), 0)
            self.assertEqual(checker.main(["src/main/bad.c", "--strict"]), 1)
            self.assertEqual(checker.main(["src/main/bad.c"]), 0)
            self.assertEqual(checker.main(["src/nope"]), 2)
            self.assertEqual(checker.main(["src/libultra/b.c"]), 2)
            self.assertEqual(checker.main(["src/libultra/b.c", "--include-vendored"]), 0)


if __name__ == "__main__":
    unittest.main()
