#!/usr/bin/env python3
"""Tests for split_asm_segments.py. Hermetic: every tree is built in a tempdir.

The build is replaced by a fake that does what splat's extraction does for a new
`c` entry: it writes src/<segment>/<name>.c with two INCLUDE_ASM stubs, then
returns the scripted exit code.
"""

import argparse
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from split_asm_segments import (  # noqa: E402
    SplitError,
    commit_message,
    parse_asm_subsegments,
    parse_offset,
    rewrite_line,
    split_all,
    split_subsegment,
)

YAML = (
    'options:\n'
    '  basename: mg64\n'
    'segments:\n'
    '  - name: main\n'
    '    type: code\n'
    '    start: 0x1050\n'
    '    vram: 0x80025C50\n'
    '    subsegments:\n'
    '      - [0x1050, c, main/func_80025D30]\n'
    '      - [0x19AC0, asm]\n'
    '      - [0x19B00, asm]\n'
    '  - name: overlay_8\n'
    '    type: code\n'
    '    start: 0x14EA70\n'
    '    vram: 0x801F4A30\n'
    '    symbol_name_format: ovl8_$VRAM\n'
    '    subsegments:\n'
    '      - [0x14EA70, asm]\n'
    '      - [0x14F5E0, asm]\n'
    '      - [0x14F600, bin]\n'
    '  - name: overlay_gap\n'
    '    type: code\n'
    '    start: 0x11BA50\n'
    '    vram: 0x80400000\n'
    '    symbol_name_format: ovlgap_$VRAM\n'
    '    subsegments:\n'
    '      - [0x11BA50, asm]\n'
)


def make_tree(root, yaml_text=YAML):
    (root / 'src' / 'main').mkdir(parents=True)
    (root / 'src' / 'main' / 'func_80025D30.c').write_text('/* already split */\n')
    (root / 'mariogolf64.yaml').write_text(yaml_text)
    return root


def extract_like(fail_entries=(), calls=None):
    """Fake build: writes stub C files for every c entry without one, like splat does."""
    def verify(root, log):
        log.write_text('fake build log\n')
        text = (root / 'mariogolf64.yaml').read_text()
        for line in text.splitlines():
            if ', c, ' not in line:
                continue
            entry = line.split(', c, ')[1].rstrip(']')
            path = root / 'src' / f'{entry}.c'
            if path.exists():
                continue
            if calls is not None:
                calls.append(entry)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                f'INCLUDE_ASM("asm/nonmatchings/{entry}", func_a);\n'
                f'INCLUDE_ASM("asm/nonmatchings/{entry}", func_b);\n'
            )
            if entry in fail_entries:
                return 1
        return 0
    return verify


class ParseTest(unittest.TestCase):
    def test_lists_only_asm_lines_with_their_owning_segment(self):
        subs = parse_asm_subsegments(YAML)
        self.assertEqual(
            [(s.offset, s.segment) for s in subs],
            [(0x19AC0, 'main'), (0x19B00, 'main'), (0x14EA70, 'overlay_8'),
             (0x14F5E0, 'overlay_8'), (0x11BA50, 'overlay_gap')],
        )

    def test_main_names_use_plain_vram(self):
        subs = {s.offset: s for s in parse_asm_subsegments(YAML)}
        # vram 0x80025C50 + (0x19AC0 - 0x1050)
        self.assertEqual(subs[0x19AC0].name, 'func_8003E6C0')
        self.assertEqual(subs[0x19B00].c_entry, 'main/func_8003E700')

    def test_overlay_names_use_symbol_name_format(self):
        subs = {s.offset: s for s in parse_asm_subsegments(YAML)}
        # vram 0x801F4A30 + (0x14F5E0 - 0x14EA70)
        self.assertEqual(subs[0x14F5E0].name, 'func_ovl8_801F55A0')
        self.assertEqual(subs[0x14F5E0].c_entry, 'overlay_8/func_ovl8_801F55A0')
        self.assertEqual(subs[0x11BA50].name, 'func_ovlgap_80400000')

    def test_rewrite_changes_only_the_target_line_and_keeps_line_endings(self):
        text = YAML.replace('\n', '\r\n')
        sub = next(s for s in parse_asm_subsegments(text) if s.offset == 0x19B00)
        new = rewrite_line(text, sub, sub.c_entry)
        before = text.splitlines(keepends=True)
        after = new.splitlines(keepends=True)
        self.assertEqual(len(before), len(after))
        changed = [i for i, (a, b) in enumerate(zip(before, after)) if a != b]
        self.assertEqual(changed, [sub.line_index])
        self.assertEqual(after[sub.line_index], '      - [0x19B00, c, main/func_8003E700]\r\n')

    def test_parse_offset_accepts_hex_with_or_without_prefix(self):
        self.assertEqual(parse_offset('0x19AC0'), 0x19AC0)
        self.assertEqual(parse_offset('19ac0'), 0x19AC0)
        with self.assertRaises(argparse.ArgumentTypeError):
            parse_offset('xyz')


class SplitSubsegmentTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = make_tree(Path(self._tmp.name))

    def tearDown(self):
        self._tmp.cleanup()

    def test_success_keeps_yaml_line_and_generated_file(self):
        result = split_subsegment(self.root, 0x19AC0, extract_like())
        self.assertTrue(result.kept, result.reason)
        self.assertEqual(result.functions, 2)
        self.assertEqual(result.c_entry, 'main/func_8003E6C0')
        self.assertTrue((self.root / 'src' / 'main' / 'func_8003E6C0.c').is_file())
        yaml_text = (self.root / 'mariogolf64.yaml').read_text()
        self.assertIn('      - [0x19AC0, c, main/func_8003E6C0]\n', yaml_text)
        self.assertIn('      - [0x19B00, asm]\n', yaml_text)
        self.assertIsNone(result.log)

    def test_failure_restores_yaml_bytes_and_removes_generated_file(self):
        before = (self.root / 'mariogolf64.yaml').read_bytes()
        result = split_subsegment(self.root, 0x19AC0, extract_like(fail_entries={'main/func_8003E6C0'}))
        self.assertFalse(result.kept)
        self.assertIn('exited 1', result.reason)
        self.assertEqual((self.root / 'mariogolf64.yaml').read_bytes(), before)
        self.assertFalse((self.root / 'src' / 'main' / 'func_8003E6C0.c').exists())
        self.assertTrue(result.log is not None and result.log.exists())
        result.log.unlink()

    def test_build_that_passes_without_the_c_file_is_a_failure(self):
        before = (self.root / 'mariogolf64.yaml').read_bytes()
        result = split_subsegment(self.root, 0x19AC0, lambda root, log: 0)
        self.assertFalse(result.kept)
        self.assertIn('was not generated', result.reason)
        self.assertEqual((self.root / 'mariogolf64.yaml').read_bytes(), before)

    def test_interrupted_build_restores_yaml_and_reraises(self):
        before = (self.root / 'mariogolf64.yaml').read_bytes()

        def interrupted(root, log):
            raise RuntimeError('ctrl-c')

        with self.assertRaises(RuntimeError):
            split_subsegment(self.root, 0x19AC0, interrupted)
        self.assertEqual((self.root / 'mariogolf64.yaml').read_bytes(), before)

    def test_refuses_existing_c_file_without_running_the_build(self):
        existing = self.root / 'src' / 'main' / 'func_8003E6C0.c'
        existing.parent.mkdir(parents=True, exist_ok=True)
        existing.write_text('keep me\n')
        before = (self.root / 'mariogolf64.yaml').read_bytes()

        def must_not_run(root, log):
            raise AssertionError('build should not run')

        with self.assertRaises(SplitError):
            split_subsegment(self.root, 0x19AC0, must_not_run)
        self.assertEqual((self.root / 'mariogolf64.yaml').read_bytes(), before)
        self.assertEqual(existing.read_text(), 'keep me\n')

    def test_unknown_offset_is_refused(self):
        with self.assertRaises(SplitError):
            split_subsegment(self.root, 0x14F600, extract_like())  # bin, not asm
        with self.assertRaises(SplitError):
            split_subsegment(self.root, 0x1050, extract_like())  # already c


class SplitAllTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = make_tree(Path(self._tmp.name))

    def tearDown(self):
        self._tmp.cleanup()

    def test_attempts_in_file_order_and_keeps_going_past_failures(self):
        calls = []
        results = split_all(self.root, extract_like(fail_entries={'main/func_8003E700'}, calls=calls))
        self.assertEqual(
            [r.offset for r in results],
            [0x19AC0, 0x19B00, 0x14EA70, 0x14F5E0, 0x11BA50],
        )
        self.assertEqual([r.kept for r in results], [True, False, True, True, True])
        self.assertEqual(calls, [
            'main/func_8003E6C0', 'main/func_8003E700', 'overlay_8/func_ovl8_801F4A30',
            'overlay_8/func_ovl8_801F55A0', 'overlay_gap/func_ovlgap_80400000',
        ])
        yaml_text = (self.root / 'mariogolf64.yaml').read_text()
        self.assertIn('[0x19AC0, c, main/func_8003E6C0]', yaml_text)
        self.assertIn('[0x19B00, asm]', yaml_text)  # failed attempt rolled back
        self.assertIn('[0x14F5E0, c, overlay_8/func_ovl8_801F55A0]', yaml_text)
        self.assertFalse((self.root / 'src' / 'main' / 'func_8003E700.c').exists())

    def test_refused_existing_file_is_reported_as_failure(self):
        existing = self.root / 'src' / 'overlay_gap' / 'func_ovlgap_80400000.c'
        existing.parent.mkdir(parents=True)
        existing.write_text('keep\n')
        results = split_all(self.root, extract_like())
        refused = [r for r in results if r.offset == 0x11BA50][0]
        self.assertFalse(refused.kept)
        self.assertIn('already exists', refused.reason)
        self.assertEqual(existing.read_text(), 'keep\n')


@unittest.skipUnless(shutil.which('git'), 'git is required')
class CommitTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = make_tree(Path(self._tmp.name))
        git = ['git', '-C', str(self.root)]
        subprocess.run([*git, 'init', '-q', '-b', 'dev'], check=True)
        subprocess.run([*git, 'config', 'user.email', 'test@example.com'], check=True)
        subprocess.run([*git, 'config', 'user.name', 'Test'], check=True)
        subprocess.run([*git, 'config', 'commit.gpgsign', 'false'], check=True)
        subprocess.run([*git, 'add', '-A'], check=True)
        subprocess.run([*git, 'commit', '-q', '-m', 'init'], check=True)

    def tearDown(self):
        self._tmp.cleanup()

    def test_commit_contains_only_the_split_and_leaves_other_staged_files_alone(self):
        from split_asm_segments import commit_split
        (self.root / 'unrelated.txt').write_text('staged by someone else\n')
        subprocess.run(['git', '-C', str(self.root), 'add', 'unrelated.txt'], check=True)
        results = split_all(self.root, extract_like(), commit=True)
        self.assertEqual(sum(r.kept for r in results), 5)
        log = subprocess.run(
            ['git', '-C', str(self.root), 'log', '--format=%s', '--name-only'],
            capture_output=True, text=True, check=True,
        ).stdout
        self.assertIn('Split asm subsegment 0x19AC0 into C', log)
        self.assertIn('src/main/func_8003E6C0.c', log)
        staged = subprocess.run(
            ['git', '-C', str(self.root), 'diff', '--cached', '--name-only'],
            capture_output=True, text=True, check=True,
        ).stdout.split()
        self.assertEqual(staged, ['unrelated.txt'])
        self.assertIsNotNone(commit_split)

    def test_commit_message_subject_fits_convention(self):
        from split_asm_segments import SplitResult
        message = commit_message(SplitResult(0x19AC0, 'main/func_8003E6C0', True, 2, None, ''))
        subject = message.splitlines()[0]
        self.assertLessEqual(len(subject), 50)
        self.assertIn('2 INCLUDE_ASM stub(s)', message)


if __name__ == '__main__':
    unittest.main()
