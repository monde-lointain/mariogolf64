#!/usr/bin/env python3
"""Tests for integrate-shards. Hermetic: every repository is built in a tempdir."""

import subprocess
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent / 'integrate-shards'

STUBS = 'INCLUDE_ASM("asm/f", func_A);\n\nmiddle\n\nINCLUDE_ASM("asm/f", func_B);\n'


def git(cwd, *args):
    return subprocess.run(['git', '-c', 'user.name=t', '-c', 'user.email=t@t', *args], cwd=cwd,
                          check=True, capture_output=True, text=True).stdout.strip()


def commit(cwd, path, text, message):
    (cwd / path).write_text(text)
    git(cwd, 'add', path)
    git(cwd, 'commit', '-q', '-m', message)


class IntegrateShardsTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.repo = Path(tmp.name)
        git(self.repo, 'init', '-q', '-b', 'dev')
        (self.repo / 'tools').mkdir()
        (self.repo / 'src').mkdir()
        build = self.repo / 'tools' / 'build-and-verify.sh'
        build.write_text('#!/bin/sh\n! grep -q broken src/f.c\n')
        build.chmod(0o755)
        git(self.repo, 'add', 'tools')
        commit(self.repo, 'src/f.c', STUBS, 'base')
        git(self.repo, 'checkout', '-q', '-b', 'shard')

    def run_on_dev(self):
        git(self.repo, 'checkout', '-q', 'dev')
        return subprocess.run([str(SCRIPT), 'shard'], cwd=self.repo, capture_output=True, text=True)

    def subjects(self):
        return git(self.repo, 'log', '--reverse', '--format=%s').splitlines()

    def test_source_only_commit_with_any_subject_is_picked(self):
        commit(self.repo, 'src/f.c', STUBS.replace('middle', 'typed'), 'Type func_A arg as u16')

        proc = self.run_on_dev()

        self.assertEqual(proc.returncode, 0, proc.stdout)
        self.assertEqual(self.subjects(), ['base', 'Type func_A arg as u16'])

    def test_decompile_commit_that_touches_a_tool_stops_the_branch(self):
        (self.repo / 'tools' / 'extra.py').write_text('x\n')
        git(self.repo, 'add', 'tools/extra.py')
        commit(self.repo, 'src/f.c', STUBS.replace('INCLUDE_ASM("asm/f", func_A);', 'void func_A(void) {}'),
               'Decompile func_A')

        proc = self.run_on_dev()

        self.assertEqual(proc.returncode, 1)
        self.assertEqual(self.subjects(), ['base'])

    def test_files_outside_the_source_paths_stop_the_branch_before_later_matches(self):
        commit(self.repo, 'src/f.c', STUBS.replace('INCLUDE_ASM("asm/f", func_A);', 'void func_A(void) {}'),
               'Decompile func_A')
        commit(self.repo, 'notes', 'x\n', 'harness: change something')
        commit(self.repo, 'src/f.c', (self.repo / 'src/f.c').read_text().replace(
            'INCLUDE_ASM("asm/f", func_B);', 'void func_B(void) {}'), 'Decompile func_B')

        proc = self.run_on_dev()

        self.assertEqual(proc.returncode, 1)
        self.assertEqual(self.subjects(), ['base', 'Decompile func_A'])

    def test_review_commit_picked_by_hand_with_edits_is_skipped(self):
        commit(self.repo, 'notes', 'x\n', 'harness: change something')
        shard_commit = git(self.repo, 'rev-parse', 'HEAD')
        git(self.repo, 'checkout', '-q', 'dev')
        git(self.repo, 'cherry-pick', '-n', shard_commit)
        (self.repo / 'notes').write_text('x\nresolved by hand\n')
        git(self.repo, 'add', 'notes')
        git(self.repo, 'commit', '-q', '-C', shard_commit)

        proc = self.run_on_dev()

        self.assertEqual(proc.returncode, 0, proc.stdout)
        self.assertEqual(self.subjects(), ['base', 'harness: change something'])

    def test_match_already_integrated_differently_is_skipped(self):
        commit(self.repo, 'src/f.c', STUBS.replace('INCLUDE_ASM("asm/f", func_A);', 'void func_A(void) {}'),
               'Decompile func_A')
        git(self.repo, 'checkout', '-q', 'dev')
        commit(self.repo, 'src/f.c', STUBS.replace('INCLUDE_ASM("asm/f", func_A);', 'void func_A(void) {\n}'),
               'Decompile func_A')

        proc = self.run_on_dev()

        self.assertEqual(proc.returncode, 0, proc.stdout)
        self.assertEqual(self.subjects(), ['base', 'Decompile func_A'])

    def test_match_that_breaks_the_build_is_dropped(self):
        commit(self.repo, 'src/f.c', STUBS.replace('INCLUDE_ASM("asm/f", func_A);', 'broken'),
               'Decompile func_A')

        proc = self.run_on_dev()

        self.assertEqual(proc.returncode, 1)
        self.assertEqual(self.subjects(), ['base'])
        self.assertEqual(git(self.repo, 'status', '--porcelain'), '')

    def test_unresolved_conflict_leaves_a_clean_tree(self):
        commit(self.repo, 'src/f.c', STUBS.replace('middle', 'shard'), 'Decompile func_A')
        git(self.repo, 'checkout', '-q', 'dev')
        commit(self.repo, 'src/f.c', STUBS.replace('middle', 'dev'), 'Decompile func_B')

        proc = self.run_on_dev()

        self.assertEqual(proc.returncode, 1)
        self.assertEqual(self.subjects(), ['base', 'Decompile func_B'])
        self.assertEqual(git(self.repo, 'status', '--porcelain'), '')


if __name__ == '__main__':
    unittest.main()
