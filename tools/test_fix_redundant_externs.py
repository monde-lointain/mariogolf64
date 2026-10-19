#!/usr/bin/env python3

import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import fix_redundant_externs as fixer  # noqa: E402


class RedundantExternFixerTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.root = Path(self.temp_dir.name).resolve()
        (self.root / "src" / "main").mkdir(parents=True)
        (self.root / "include").mkdir()

    def tearDown(self):
        self.temp_dir.cleanup()

    def write(self, relative_path, text):
        path = self.root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def violations(self, source, include_vendored=False):
        return fixer.find_violations(
            [source],
            fixer.header_index(self.root, include_vendored=include_vendored),
            self.root,
        )

    def fix(self, source, violations):
        fixer.fix_file(source, violations, self.root)

    def test_detects_and_fixes_multiline_extern(self):
        self.write(
            "src/main/render.h",
            "void drawThing(s32 id, void (*callback)(void));\n",
        )
        source = self.write(
            "src/main/render_user.c",
            '#include "common.h"\n\n'
            "extern void drawThing(s32 id,\n"
            "                          void (*callback)(void));\n"
            "extern void sourceOnly(void);\n",
        )

        violations = self.violations(source)
        self.assertEqual([item.declaration.name for item in violations], ["drawThing"])

        self.fix(source, violations)
        self.assertEqual(
            source.read_text(),
            '#include "common.h"\n'
            '#include "render.h"\n\n'
            "extern void sourceOnly(void);\n",
        )

    def test_reuses_an_existing_include(self):
        self.write("src/main/task.h", "void runTask(void);\n")
        source = self.write(
            "src/main/task_user.c",
            '#include "task.h"\n\nextern void runTask(void);\n',
        )

        violations = self.violations(source)
        self.fix(source, violations)

        self.assertEqual(source.read_text(), '#include "task.h"\n\n')

    def test_ignores_comments_variables_and_function_pointer_variables(self):
        self.write(
            "src/main/task.h",
            "void runTask(void);\nvoid callbackTarget(void);\n",
        )
        source = self.write(
            "src/main/task_user.c",
            "/* extern void runTask(void); */\n"
            "extern s32 runTaskCount;\n"
            "extern void (*callbackTarget)(void);\n",
        )

        self.assertEqual(self.violations(source), [])

    def test_ignores_function_pointer_array_variables(self):
        self.write("src/main/task.h", "void callbackTarget(void);\n")
        source = self.write(
            "src/main/task_user.c",
            "extern void (*callbackTarget[])(void);\n",
        )

        self.assertEqual(self.violations(source), [])

    def test_prefers_already_included_header(self):
        self.write("src/main/one.h", "void runTask(void);\n")
        self.write("src/main/two.h", "void runTask(void);\n")
        source = self.write(
            "src/main/task_user.c",
            '#include "two.h"\n\nextern void runTask(void);\n',
        )

        violations = self.violations(source)
        self.assertEqual(violations[0].selected_header, Path("src/main/two.h"))

    def test_reports_equal_header_choices_as_ambiguous(self):
        self.write("src/main/one.h", "void runTask(void);\n")
        self.write("src/main/two.h", "void runTask(void);\n")
        source = self.write("src/main/task_user.c", "extern void runTask(void);\n")

        violations = self.violations(source)
        self.assertIsNone(violations[0].selected_header)
        self.fix(source, violations)
        self.assertEqual(source.read_text(), "extern void runTask(void);\n")

    def test_prefers_source_corresponding_header(self):
        self.write("src/main/player.h", "void updatePlayer(void);\n")
        self.write("src/main/shared.h", "void updatePlayer(void);\n")
        source = self.write(
            "src/main/player.c",
            "extern void updatePlayer(void);\n",
        )

        violations = self.violations(source)
        self.assertEqual(violations[0].selected_header, Path("src/main/player.h"))

    def test_ignores_incompatible_header_signatures(self):
        self.write(
            "src/main/math.h",
            "s32 calculate(void *value);\n"
            "void rotate(void *matrix, s16 x, s16 y);\n",
        )
        source = self.write(
            "src/main/math_user.c",
            "extern void calculate(void *value);\n"
            "extern void rotate(void *matrix, s16 x, s16 y, ...);\n",
        )

        self.assertEqual(self.violations(source), [])

    def test_does_not_add_header_that_conflicts_with_another_local_extern(self):
        self.write(
            "src/main/math.h",
            "void rotateX(void *matrix, s16 angle);\n"
            "void rotateYX(void *matrix, s16 x, s16 y);\n",
        )
        source = self.write(
            "src/main/math_user.c",
            "extern void rotateX(void *matrix, s16 angle);\n"
            "extern void rotateYX(void *matrix, s16 x, s16 y, ...);\n",
        )

        self.assertEqual(self.violations(source), [])

    def test_moves_late_same_file_forward_declaration(self):
        source = self.write(
            "src/main/sound.c",
            '#include "sound.h"\n\n'
            "void firstFunction(void) {\n"
            "}\n\n"
            "extern void enqueue(void *position);\n\n"
            "void enqueue(void *position) {\n"
            "}\n",
        )

        violations = self.violations(source)
        self.assertEqual(len(violations), 1)
        self.assertTrue(violations[0].same_file_forward)

        self.fix(source, violations)
        self.assertEqual(
            source.read_text(),
            '#include "sound.h"\n\n'
            "extern void enqueue(void *position);\n\n"
            "void firstFunction(void) {\n"
            "}\n\n"
            "void enqueue(void *position) {\n"
            "}\n",
        )

    def test_allows_void_pointer_to_match_typed_pointer_header(self):
        self.write(
            "src/main/math.h",
            "void multiply(FixedMatrix3s left, FixedMatrix3s right);\n",
        )
        source = self.write(
            "src/main/math_user.c",
            "extern void multiply(void *left, void *right);\n",
        )

        violations = self.violations(source)
        self.assertEqual(violations[0].selected_header, Path("src/main/math.h"))

    def test_accepts_one_compatible_conditional_header_prototype(self):
        self.write(
            "src/main/math.h",
            "#ifdef IMPLEMENTATION\n"
            "void rotate(Matrix matrix, s16 x, s16 y);\n"
            "#else\n"
            "void rotate(Matrix matrix, s16 x, s16 y, ...);\n"
            "#endif\n"
            "void multiply(Matrix left, Matrix right);\n",
        )
        source = self.write(
            "src/main/math_user.c",
            "extern void rotate(void *matrix, s16 x, s16 y, ...);\n"
            "extern void multiply(void *left, void *right);\n",
        )

        violations = self.violations(source)
        self.assertEqual(
            [violation.declaration.name for violation in violations],
            ["rotate", "multiply"],
        )

    def test_moves_forward_declaration_outside_non_matching_block(self):
        source = self.write(
            "src/main/compress.c",
            '#include "compress.h"\n\n'
            'INCLUDE_ASM("asm/nonmatchings/main/compress", compress);\n'
            "#ifdef NON_MATCHING\n"
            "s32 compress(u8 *source) {\n"
            "    return 0;\n"
            "}\n"
            "#endif\n\n"
            "extern s32 compress(u8 *source);\n",
        )

        violations = self.violations(source)
        self.fix(source, violations)

        self.assertEqual(
            source.read_text(),
            '#include "compress.h"\n\n'
            'INCLUDE_ASM("asm/nonmatchings/main/compress", compress);\n'
            "extern s32 compress(u8 *source);\n\n"
            "#ifdef NON_MATCHING\n"
            "s32 compress(u8 *source) {\n"
            "    return 0;\n"
            "}\n"
            "#endif\n\n",
        )

    def test_skips_vendored_headers_unless_requested(self):
        self.write("include/libultra/PR/os.h", "void osInitialize(void);\n")
        source = self.write(
            "src/main/runtime.c",
            "extern void osInitialize(void);\n",
        )

        self.assertEqual(self.violations(source), [])

        violations = self.violations(source, include_vendored=True)
        self.assertEqual(
            violations[0].selected_header, Path("include/libultra/PR/os.h")
        )
        self.fix(source, violations)
        self.assertEqual(source.read_text(), '#include "libultra/PR/os.h"\n')

    def test_common_header_is_spelled_relative_to_include(self):
        self.write("include/common.h", "void commonFn(void);\n")
        source = self.write(
            "src/main/common_user.c",
            "extern void commonFn(void);\n",
        )

        violations = self.violations(source)
        self.fix(source, violations)
        self.assertEqual(source.read_text(), '#include "common.h"\n')

    def test_include_spelling_crosses_source_directories(self):
        self.write("src/main/rom_asset.h", "void loadAsset(void);\n")
        source = self.write(
            "src/overlay_15/func_ovl15_load.c",
            "extern void loadAsset(void);\n",
        )

        violations = self.violations(source)
        self.fix(source, violations)
        self.assertEqual(source.read_text(), '#include "../main/rom_asset.h"\n')

    def test_source_files_skips_vendored_and_non_c_files(self):
        owned = self.write("src/main/a.c", "")
        self.write("src/main/b.h", "")
        self.write("src/libultra/c.c", "")

        self.assertEqual(
            list(fixer.source_files([self.root / "src"], self.root)),
            [owned.resolve()],
        )


class MainTests(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.root = Path(self.temp_dir.name).resolve()
        (self.root / "src" / "main").mkdir(parents=True)
        (self.root / "include").mkdir()
        (self.root / "src" / "main" / "render.h").write_text(
            "void drawThing(void);\n"
        )
        self.source = self.root / "src" / "main" / "render_user.c"
        self.source.write_text("#include \"common.h\"\n\nextern void drawThing(void);\n")

    def tearDown(self):
        self.temp_dir.cleanup()

    def run_main(self, argv):
        stdout = io.StringIO()
        stderr = io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            code = fixer.main(argv, root=self.root)
        return code, stdout.getvalue(), stderr.getvalue()

    def test_check_mode_reports_without_editing(self):
        original = self.source.read_text()
        code, stdout, stderr = self.run_main(["src"])

        self.assertEqual(code, 1)
        self.assertEqual(
            stdout,
            "src/main/render_user.c:3: redundant extern for drawThing; "
            "declared by src/main/render.h\n",
        )
        self.assertEqual(stderr, "found 1 redundant extern(s)\n")
        self.assertEqual(self.source.read_text(), original)

    def test_fix_mode_rewrites_and_exits_zero(self):
        code, stdout, _ = self.run_main(["src", "--fix"])

        self.assertEqual(code, 0)
        self.assertEqual(stdout.splitlines()[-1], "fixed 1 redundant extern(s); 0 ambiguous")
        self.assertEqual(
            self.source.read_text(),
            '#include "common.h"\n#include "render.h"\n\n',
        )

    def test_missing_path_exits_two(self):
        code, _, stderr = self.run_main(["src/missing"])

        self.assertEqual(code, 2)
        self.assertIn("path does not exist", stderr)

    def test_vendored_file_argument_exits_two(self):
        vendored = self.root / "src" / "libultra" / "x.c"
        vendored.parent.mkdir(parents=True)
        vendored.write_text("")

        code, _, stderr = self.run_main(["src/libultra/x.c"])

        self.assertEqual(code, 2)
        self.assertIn("not a project-owned C file", stderr)


if __name__ == "__main__":
    unittest.main()
