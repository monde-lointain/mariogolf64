#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import importlib.machinery
import importlib.util
import io
import os
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest import mock

TOOLS_DIR = Path(__file__).resolve().parent
SCRIPT = TOOLS_DIR / "verify-candidate"
sys.path.insert(0, str(TOOLS_DIR))
import check_pointer_arithmetic as checker  # noqa: E402  (tools/ is not on sys.path under unittest)

BASEROM = b"reference rom bytes for the fixture\n"
BASEROM_SHA1 = hashlib.sha1(BASEROM).hexdigest()

# Pointer arithmetic on a cast; the checker reports this as a violation.
CAST_OFFSET = "char g(void *p) { return *((char*)p + 4); }\n"


def load_gate():
    loader = importlib.machinery.SourceFileLoader("verify_candidate", str(SCRIPT))
    spec = importlib.util.spec_from_loader("verify_candidate", loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


gate = load_gate()


class Build:
    """Stands in for ./tools/build-and-verify.sh and records how often it ran."""

    def __init__(self, code: int = 0) -> None:
        self.code = code
        self.calls = 0

    def __call__(self, root: Path) -> int:
        self.calls += 1
        return self.code


def git(cwd: Path, *args: str) -> None:
    subprocess.run(
        [
            "git",
            "-c", "user.name=t",
            "-c", "user.email=t@t",
            "-c", "protocol.file.allow=always",
            *args,
        ],
        cwd=cwd,
        check=True,
        capture_output=True,
    )


def write(root: Path, rel: str, text: str) -> None:
    path = root / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


class GateTestCase(unittest.TestCase):
    def setUp(self) -> None:
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.tmp = Path(tmp.name)
        self.root = self.tmp / "repo"
        self.root.mkdir()
        git(self.root, "init", "-q", "-b", "dev")
        write(self.root, "mariogolf64.sha1", f"{BASEROM_SHA1}  build/mariogolf64.z64\n")
        write(self.root, "tools/kmc-cc", "#!/bin/sh\n")
        write(self.root, "src/main/done.c", "void func_a(void) {\n    return;\n}\n")
        write(self.root, "src/main/prefix.c", "void func_bb(void) {\n}\n")
        write(self.root, "src/main/left.c", 'INCLUDE_ASM("asm/nonmatchings/main", func_b);\n')
        git(self.root, "add", ".")
        git(self.root, "commit", "-qm", "fixture")
        # Untracked and gitignored in the real repo, so it is written after the commit.
        (self.root / "baserom.z64").write_bytes(BASEROM)

    def gate_run(self, argv, build=None):
        build = build if build is not None else Build()
        err, out = io.StringIO(), io.StringIO()
        with redirect_stderr(err), redirect_stdout(out):
            code = gate.main(argv, root=self.root, build=build)
        return code, err.getvalue(), build

    def add_submodule(self) -> Path:
        upstream = self.tmp / "upstream"
        upstream.mkdir()
        git(upstream, "init", "-q", "-b", "dev")
        write(upstream, "a.txt", "a\n")
        git(upstream, "add", ".")
        git(upstream, "commit", "-qm", "upstream")
        git(self.root, "submodule", "add", "-q", str(upstream), "tools/asm-differ")
        git(self.root, "commit", "-qm", "add submodule")
        return self.root / "tools/asm-differ"


class IncludeAsmCheckTest(GateTestCase):
    def test_include_asm_left_in_place_is_rejected_though_build_passes(self):
        # The ROM still matches with func_b linked from asm/, so build-and-verify
        # alone would accept this state. The gate must reject it before building.
        code, err, build = self.gate_run(["--decompiled", "func_b"], Build(0))
        self.assertNotEqual(code, 0)
        self.assertIn("func_b", err)
        self.assertIn("INCLUDE_ASM", err)
        self.assertIn("src/main/left.c:1", err)
        self.assertEqual(len(err.strip().splitlines()), 1)
        self.assertEqual(build.calls, 0)

    def test_decompiled_function_passes_on_clean_tree(self):
        code, err, build = self.gate_run(["--decompiled", "func_a"])
        self.assertEqual(code, 0, err)
        self.assertEqual(build.calls, 1)

    def test_include_asm_match_is_exact_name(self):
        # func_b is INCLUDE_ASM'd; func_bb is defined in C and must not be blocked by it.
        code, err, _ = self.gate_run(["--decompiled", "func_bb"])
        self.assertEqual(code, 0, err)

    def test_commented_out_include_asm_does_not_block(self):
        write(
            self.root,
            "src/main/commented.c",
            '// INCLUDE_ASM("asm/nonmatchings/main", func_d);\nvoid func_d(void) {\n}\n',
        )
        code, err, _ = self.gate_run(["--decompiled", "func_d"])
        self.assertEqual(code, 0, err)

    def test_comment_markers_inside_strings_do_not_hide_definitions(self):
        write(
            self.root,
            "src/main/strs.c",
            'static const char *tag = "/*";\nvoid func_e(void) {\n}\n/* done */\n',
        )
        code, err, _ = self.gate_run(["--decompiled", "func_e"])
        self.assertEqual(code, 0, err)


class DefinitionCheckTest(GateTestCase):
    def test_missing_function_is_rejected(self):
        code, err, build = self.gate_run(["--decompiled", "func_zzz"])
        self.assertNotEqual(code, 0)
        self.assertIn("func_zzz", err)
        self.assertIn("not defined", err)
        self.assertEqual(build.calls, 0)

    def test_prototype_is_not_a_definition(self):
        write(self.root, "src/main/proto.c", "void func_c(void);\n")
        code, err, _ = self.gate_run(["--decompiled", "func_c"])
        self.assertNotEqual(code, 0)
        self.assertIn("not defined", err)

    def test_call_site_is_not_a_definition(self):
        write(
            self.root,
            "src/main/caller.c",
            "void caller(void) {\n    if (func_g(1)) {\n        return;\n    }\n}\n",
        )
        code, err, _ = self.gate_run(["--decompiled", "func_g"])
        self.assertNotEqual(code, 0)
        self.assertIn("not defined", err)


class CandidateTest(GateTestCase):
    def test_candidate_key_forms_name_the_same_function(self):
        values = [
            '["func_a","func_x","func_y"]',
            "func_a",
            "asm/nonmatchings/main/func_a.s",
            "func_a (../mg64-a)",
            '"func_a"',
        ]
        for value in values:
            with self.subTest(value=value):
                self.assertEqual(gate.function_from_candidate(value), "func_a")

    def test_unusable_candidate_is_rejected(self):
        for value in ["", "[]", "[1]", "[not json", "1x", "func a"]:
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    gate.function_from_candidate(value)

    def test_json_candidate_key_is_accepted_by_gate(self):
        code, err, _ = self.gate_run(["--decompiled", '["func_a","func_x"]'])
        self.assertEqual(code, 0, err)


class ProtectedPathTest(GateTestCase):
    def test_edited_protected_sha1_is_rejected_before_build(self):
        write(self.root, "mariogolf64.sha1", "0" * 40 + "  build/mariogolf64.z64\n")
        code, err, build = self.gate_run(["--decompiled", "func_a"])
        self.assertNotEqual(code, 0)
        self.assertIn("mariogolf64.sha1", err)
        self.assertEqual(build.calls, 0)

    def test_edited_vendored_tool_is_rejected(self):
        write(self.root, "tools/kmc-cc", "#!/bin/sh\necho tampered\n")
        code, err, build = self.gate_run([])
        self.assertNotEqual(code, 0)
        self.assertIn("tools/kmc-cc", err)
        self.assertEqual(build.calls, 0)

    def test_untracked_file_in_protected_submodule_is_rejected(self):
        sub = self.add_submodule()
        write(sub, "scratch.txt", "x\n")
        code, err, build = self.gate_run([])
        self.assertNotEqual(code, 0)
        self.assertIn("tools/asm-differ", err)
        self.assertEqual(build.calls, 0)

    def test_advanced_protected_submodule_head_is_rejected(self):
        sub = self.add_submodule()
        write(sub, "b.txt", "b\n")
        git(sub, "add", ".")
        git(sub, "commit", "-qm", "advance")
        code, err, build = self.gate_run([])
        self.assertNotEqual(code, 0)
        self.assertIn("tools/asm-differ", err)
        self.assertEqual(build.calls, 0)


class BaseromTest(GateTestCase):
    def test_baserom_with_wrong_hash_is_rejected(self):
        (self.root / "baserom.z64").write_bytes(b"not the reference rom\n")
        code, err, build = self.gate_run([])
        self.assertNotEqual(code, 0)
        self.assertIn("baserom.z64", err)
        self.assertEqual(build.calls, 0)

    def test_missing_baserom_is_rejected(self):
        (self.root / "baserom.z64").unlink()
        code, err, build = self.gate_run([])
        self.assertNotEqual(code, 0)
        self.assertIn("baserom.z64", err)
        self.assertEqual(build.calls, 0)


class BuildTest(GateTestCase):
    def test_build_failure_is_rejected(self):
        code, err, build = self.gate_run(["--decompiled", "func_a"], Build(2))
        self.assertNotEqual(code, 0)
        self.assertIn("build-and-verify", err)
        self.assertIn("exit 2", err)
        self.assertEqual(build.calls, 1)

    def test_global_gate_without_decompiled_passes_on_clean_tree(self):
        # Global tasks have no function to check; the left-in-place INCLUDE_ASM is ignored.
        code, err, build = self.gate_run([])
        self.assertEqual(code, 0, err)
        self.assertEqual(build.calls, 1)

    def test_script_is_executable(self):
        self.assertTrue(os.access(SCRIPT, os.X_OK))


class PointerArithmeticTest(GateTestCase):
    def setUp(self) -> None:
        super().setUp()
        for patcher in (
            mock.patch.object(checker, "ROOT", self.root),
            # Skips `make print-cflags`, which the fixture repo cannot run.
            mock.patch.object(checker, "profile_flags", return_value=[]),
        ):
            patcher.start()
            self.addCleanup(patcher.stop)

    def commit_file(self, rel: str) -> None:
        git(self.root, "add", rel)
        git(self.root, "commit", "-qm", f"commit {rel}")

    def test_violation_on_added_line_is_rejected_before_build(self):
        write(self.root, "src/main/done.c", "void func_a(void) {\n    return;\n}\n" + CAST_OFFSET)
        code, err, build = self.gate_run([])
        self.assertNotEqual(code, 0)
        self.assertEqual(build.calls, 0)
        self.assertIn('src/main/done.c:4 "(char*)p + 4"', err)
        self.assertEqual(len(err.strip().splitlines()), 1)

    def test_violation_on_untouched_line_passes(self):
        # Prepending a line moves the committed violation to line 5; only line 1 changed.
        write(self.root, "src/main/done.c", "void func_a(void) {\n    return;\n}\n" + CAST_OFFSET)
        self.commit_file("src/main/done.c")
        write(self.root, "src/main/done.c", "/* note */\n" + "void func_a(void) {\n    return;\n}\n" + CAST_OFFSET)
        code, err, build = self.gate_run([])
        self.assertEqual(code, 0, err)
        self.assertEqual(build.calls, 1)

    def test_edited_violating_line_is_rejected(self):
        write(self.root, "src/main/done.c", "void func_a(void) {\n    return;\n}\n" + CAST_OFFSET)
        self.commit_file("src/main/done.c")
        write(self.root, "src/main/done.c", "void func_a(void) {\n    return;\n}\n" + CAST_OFFSET.replace("+ 4", "+ 8"))
        code, err, build = self.gate_run([])
        self.assertNotEqual(code, 0)
        self.assertEqual(build.calls, 0)
        self.assertIn('src/main/done.c:4 "(char*)p + 8"', err)

    def test_staged_violation_counts_against_head(self):
        write(self.root, "src/main/done.c", "void func_a(void) {\n    return;\n}\n" + CAST_OFFSET)
        git(self.root, "add", "src/main/done.c")
        code, err, build = self.gate_run([])
        self.assertNotEqual(code, 0)
        self.assertIn('src/main/done.c:4 "(char*)p + 4"', err)

    def test_new_untracked_file_is_checked_in_full(self):
        write(self.root, "src/main/new.c", CAST_OFFSET)
        code, err, build = self.gate_run([])
        self.assertNotEqual(code, 0)
        self.assertEqual(build.calls, 0)
        self.assertIn('src/main/new.c:1 "(char*)p + 4"', err)

    def test_unparsable_changed_file_warns_and_passes(self):
        write(self.root, "src/main/broken.c", "int func_x( {\n")
        code, err, build = self.gate_run([])
        self.assertEqual(code, 0, err)
        self.assertEqual(build.calls, 1)
        self.assertIn("Warning", err)
        self.assertIn("broken.c", err)

    def test_vendored_source_is_not_checked(self):
        write(self.root, "src/libultra/x.c", CAST_OFFSET)
        code, err, build = self.gate_run([])
        self.assertEqual(code, 0, err)
        self.assertEqual(build.calls, 1)


if __name__ == "__main__":
    unittest.main()
