#!/usr/bin/env python3
"""Tests for new-worktree. Hermetic: every repository is built in a tempdir."""

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent / 'new-worktree'


def git(cwd, *args):
    return subprocess.run(['git', *args], cwd=cwd, check=True, capture_output=True, text=True).stdout.strip()


def make_main_checkout(root, with_inputs=True):
    main = root / 'mainrepo'
    (main / 'tools').mkdir(parents=True)
    shutil.copy2(SCRIPT, main / 'tools' / 'new-worktree')
    (main / '.gitignore').write_text('*.z64\ntools/gcc_kmc/\n.venv/\n')
    (main / 'README.md').write_text('main\n')
    git(main, 'init', '-q', '-b', 'dev')
    git(main, 'config', 'user.email', 'test@example.com')
    git(main, 'config', 'user.name', 'Test')
    git(main, 'config', 'commit.gpgsign', 'false')
    git(main, 'add', '.')
    git(main, 'commit', '-q', '-m', 'init')
    if with_inputs:
        (main / 'baserom.z64').write_bytes(b'ROM' * 1000)
        (main / 'tools' / 'gcc_kmc').mkdir()
        (main / 'tools' / 'gcc_kmc' / 'gcc').write_text('fake compiler\n')
        (main / '.venv' / 'bin').mkdir(parents=True)
        (main / '.venv' / 'pyvenv.cfg').write_text('home = /nowhere\n')
    return main


def run_tool(cwd, main, *args):
    return subprocess.run(
        [str(main / 'tools' / 'new-worktree'), *args],
        cwd=cwd, capture_output=True, text=True,
    )


class NewWorktreeTest(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)

    def tearDown(self):
        self._tmp.cleanup()

    def test_creates_sibling_worktree_with_build_inputs(self):
        main = make_main_checkout(self.root)
        proc = run_tool(main, main, 'feat', '--branch', 'feat-branch')
        self.assertEqual(proc.returncode, 0, proc.stderr)

        target = self.root / 'feat'
        self.assertTrue(target.is_dir())
        self.assertEqual(git(target, 'rev-parse', '--abbrev-ref', 'HEAD'), 'feat-branch')
        self.assertEqual((target / 'baserom.z64').read_bytes(), (main / 'baserom.z64').read_bytes())
        self.assertEqual((target / 'tools' / 'gcc_kmc' / 'gcc').read_text(), 'fake compiler\n')
        self.assertTrue((target / '.venv').is_symlink())
        self.assertEqual(os.path.realpath(target / '.venv'), os.path.realpath(main / '.venv'))

    def test_default_branch_is_the_name(self):
        main = make_main_checkout(self.root)
        self.assertEqual(run_tool(main, main, 'plain').returncode, 0)
        self.assertEqual(git(self.root / 'plain', 'rev-parse', '--abbrev-ref', 'HEAD'), 'plain')

    def test_sibling_of_main_when_run_from_linked_worktree(self):
        main = make_main_checkout(self.root)
        self.assertEqual(run_tool(main, main, 'first').returncode, 0)
        proc = run_tool(self.root / 'first', main, 'second')
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertTrue((self.root / 'second').is_dir())
        self.assertFalse((self.root / 'first' / 'second').exists())

    def test_base_option_selects_the_start_commit(self):
        main = make_main_checkout(self.root)
        old = git(main, 'rev-parse', 'HEAD')
        (main / 'README.md').write_text('newer\n')
        git(main, 'commit', '-q', '-am', 'newer')
        proc = run_tool(main, main, 'old', '--base', old)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(git(self.root / 'old', 'rev-parse', 'HEAD'), old)

    def test_refuses_existing_target_without_touching_it(self):
        main = make_main_checkout(self.root)
        (self.root / 'taken').mkdir()
        (self.root / 'taken' / 'keep.txt').write_text('mine\n')
        proc = run_tool(main, main, 'taken')
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn('already exists', proc.stderr)
        self.assertEqual((self.root / 'taken' / 'keep.txt').read_text(), 'mine\n')
        self.assertEqual(len(git(main, 'worktree', 'list').splitlines()), 1)

    def test_refuses_existing_branch(self):
        main = make_main_checkout(self.root)
        git(main, 'branch', 'taken-branch')
        proc = run_tool(main, main, 'fresh', '--branch', 'taken-branch')
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn('already exists', proc.stderr)
        self.assertFalse((self.root / 'fresh').exists())

    def test_rejects_names_with_path_separators(self):
        main = make_main_checkout(self.root)
        proc = run_tool(main, main, 'a/b')
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn('must match', proc.stderr)

    def test_missing_inputs_warn_but_worktree_is_created(self):
        main = make_main_checkout(self.root, with_inputs=False)
        proc = run_tool(main, main, 'bare')
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertTrue((self.root / 'bare').is_dir())
        self.assertFalse((self.root / 'bare' / 'baserom.z64').exists())
        self.assertIn('no baserom.z64', proc.stderr)
        self.assertIn('no tools/gcc_kmc', proc.stderr)
        self.assertIn('no .venv', proc.stderr)


if __name__ == '__main__':
    unittest.main()
