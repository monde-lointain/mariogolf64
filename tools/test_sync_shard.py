#!/usr/bin/env python3
"""Tests for sync-shard. Hermetic: every repository is built in a tempdir."""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent / 'sync-shard'


def git(cwd, *args):
    return subprocess.run(['git', '-c', 'user.name=t', '-c', 'user.email=t@t', *args], cwd=cwd,
                          check=True, capture_output=True, text=True).stdout.strip()


def commit(cwd, path, text, message):
    (cwd / path).write_text(text)
    git(cwd, 'add', path)
    git(cwd, 'commit', '-q', '-m', message)
    return git(cwd, 'rev-parse', 'HEAD')


def run(cwd, branch=None):
    env = {k: v for k, v in os.environ.items() if k != 'MG64_SYNC_BRANCH'}
    if branch:
        env['MG64_SYNC_BRANCH'] = branch
    return subprocess.run([str(SCRIPT)], cwd=cwd, env=env, capture_output=True, text=True)


class SyncShardTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.repo = Path(tmp.name)
        git(self.repo, 'init', '-q', '-b', 'dev')
        commit(self.repo, 'a.c', '1\n', 'base')
        git(self.repo, 'checkout', '-q', '-b', 'shard')

    def test_without_sync_branch_nothing_moves(self):
        head = commit(self.repo, 'b.c', 'shard\n', 'shard work')
        self.assertEqual(run(self.repo).returncode, 0)
        self.assertEqual(git(self.repo, 'rev-parse', 'HEAD'), head)

    def test_rebases_and_drops_commits_already_integrated(self):
        picked = commit(self.repo, 'b.c', 'match\n', 'picked match')
        commit(self.repo, 'c.c', 'later\n', 'unmerged match')
        git(self.repo, 'checkout', '-q', 'dev')
        commit(self.repo, 'd.c', 'fix\n', 'fix from another shard')
        git(self.repo, 'cherry-pick', picked)
        git(self.repo, 'checkout', '-q', 'shard')

        proc = run(self.repo, 'dev')

        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(git(self.repo, 'log', '--format=%s', 'dev..shard'), 'unmerged match')
        self.assertTrue((self.repo / 'd.c').exists())

    def test_conflict_aborts_and_keeps_the_branch(self):
        head = commit(self.repo, 'a.c', 'shard\n', 'shard edit')
        git(self.repo, 'checkout', '-q', 'dev')
        commit(self.repo, 'a.c', 'dev\n', 'dev edit')
        git(self.repo, 'checkout', '-q', 'shard')

        proc = run(self.repo, 'dev')

        self.assertEqual(proc.returncode, 0)
        self.assertIn('conflicted', proc.stderr)
        self.assertEqual(git(self.repo, 'rev-parse', 'HEAD'), head)
        self.assertEqual(git(self.repo, 'status', '--porcelain'), '')


if __name__ == '__main__':
    unittest.main()
