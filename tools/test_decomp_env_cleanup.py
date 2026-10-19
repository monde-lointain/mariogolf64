#!/usr/bin/env python3
"""Tests for decomp-env-cleanup's unconsolidated-learnings guard.

Hermetic: the script and the lister are copied into a tempdir that is a git repo, so the
script's ROOT_DIR and the consolidation ledger both live there.
"""

import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parent


class DecompEnvCleanupLearningsTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name).resolve()
        (self.root / 'tools').mkdir()
        shutil.copy(TOOLS / 'decomp-env-cleanup', self.root / 'tools' / 'decomp-env-cleanup')
        shutil.copy(TOOLS / 'list_unconsolidated_learnings.py', self.root / 'tools' / 'list_unconsolidated_learnings.py')
        subprocess.run(['git', 'init', '-q'], cwd=self.root, check=True)

        # Every env is "matched": asm/nonmatchings is absent, so no function is unmatched.
        self.env('foo-1', 'LEARNINGS.md', 'unconsolidated lesson\n')
        self.env('bar', None, None)
        self.env('baz', 'LEARNINGS.md', 'lesson already consolidated\n')
        self.lister('--mark', 'nonmatchings/baz/LEARNINGS.md')

    def env(self, name, note, text):
        path = self.root / 'nonmatchings' / name
        path.mkdir(parents=True, exist_ok=True)
        (path / 'target.s').write_text('.text\n')
        if note:
            (path / note).write_text(text)

    def lister(self, *args):
        return subprocess.run(
            [sys.executable, str(self.root / 'tools' / 'list_unconsolidated_learnings.py'), *args],
            cwd=self.root, check=True, capture_output=True, text=True,
        )

    def cleanup(self, *args):
        env = dict(os.environ)
        env['PATH'] = f"{Path(sys.executable).parent}{os.pathsep}{env.get('PATH', '')}"
        return subprocess.run(
            ['bash', str(self.root / 'tools' / 'decomp-env-cleanup'), *args],
            cwd=self.root, env=env, capture_output=True, text=True,
        )

    def existing(self):
        return sorted(p.name for p in (self.root / 'nonmatchings').iterdir())

    def test_dry_run_keeps_env_with_unconsolidated_learnings(self):
        result = self.cleanup('--verbose')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('KEEP:  foo-1 (unconsolidated learnings', result.stdout)
        self.assertIn('Would remove 2 dirs', result.stdout)
        self.assertEqual(self.existing(), ['bar', 'baz', 'foo-1'])

    def test_delete_removes_matched_envs_without_learnings(self):
        result = self.cleanup('--delete')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.existing(), ['foo-1'])

    def test_force_removes_env_with_unconsolidated_learnings(self):
        result = self.cleanup('--delete', '--force')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.existing(), [])

    def test_consolidating_the_note_releases_the_env(self):
        self.lister('--mark', 'nonmatchings/foo-1/LEARNINGS.md')
        result = self.cleanup('--delete')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.existing(), [])


if __name__ == '__main__':
    unittest.main()
