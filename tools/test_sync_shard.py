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


def run(cwd, branch=None, candidate=None):
    env = {k: v for k, v in os.environ.items() if k not in ('MG64_SYNC_BRANCH', 'NIGEL_CANDIDATE')}
    if branch:
        env['MG64_SYNC_BRANCH'] = branch
    if candidate:
        env['NIGEL_CANDIDATE'] = candidate
    return subprocess.run([str(SCRIPT)], cwd=cwd, env=env, capture_output=True, text=True)


class SyncShardTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.repo = Path(tmp.name)
        git(self.repo, 'init', '-q', '-b', 'dev')
        (self.repo / 'tools').mkdir()
        self.build = self.repo / 'tools' / 'build-and-verify.sh'
        self.build.write_text('#!/bin/sh\nexit 0\n')
        self.build.chmod(0o755)
        git(self.repo, 'add', 'tools')
        commit(self.repo, 'a.c', 'head\nx\ntail\n', 'base')
        git(self.repo, 'checkout', '-q', '-b', 'shard')

    def test_without_sync_branch_nothing_moves(self):
        head = commit(self.repo, 'b.c', 'shard\n', 'shard work')
        self.assertEqual(run(self.repo).returncode, 0)
        self.assertEqual(git(self.repo, 'rev-parse', 'HEAD'), head)

    def test_recheck_of_a_candidate_does_not_move_head(self):
        # nigel re-runs the candidate source with NIGEL_CANDIDATE set to judge the agent's
        # work; HEAD moving then would be mistaken for an agent commit.
        head = commit(self.repo, 'b.c', 'shard\n', 'shard work')
        git(self.repo, 'checkout', '-q', 'dev')
        commit(self.repo, 'd.c', 'fix\n', 'fix from another shard')
        git(self.repo, 'checkout', '-q', 'shard')

        self.assertEqual(run(self.repo, 'dev', candidate='["func_A"]').returncode, 0)
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

    def test_conflict_that_loses_shard_work_keeps_the_branch(self):
        # Both sides replace the same line: preferring dev would drop the shard's match.
        head = commit(self.repo, 'a.c', 'head\nshard match\ntail\n', 'shard match')
        git(self.repo, 'checkout', '-q', 'dev')
        commit(self.repo, 'a.c', 'head\nother match\ntail\n', 'other match')
        git(self.repo, 'checkout', '-q', 'shard')

        proc = run(self.repo, 'dev')

        self.assertEqual(proc.returncode, 0)
        self.assertIn('conflicted', proc.stderr)
        self.assertEqual(git(self.repo, 'rev-parse', 'HEAD'), head)
        self.assertEqual(git(self.repo, 'status', '--porcelain'), '')

    def test_commit_integrated_with_a_resolved_conflict_drops_out(self):
        # The integrator picked the shard's commit after resolving a conflict by hand,
        # so its patch no longer matches the original and a plain rebase conflicts.
        commit(self.repo, 'a.c', 'head\nx\nextern a;\ntail\n', 'shard match')
        git(self.repo, 'checkout', '-q', 'dev')
        commit(self.repo, 'a.c', 'head\nx\nextern b;\ntail\n', 'other match')
        commit(self.repo, 'a.c', 'head\nx\nextern b;\nextern a;\ntail\n', 'resolved pick')
        git(self.repo, 'checkout', '-q', 'shard')

        proc = run(self.repo, 'dev')

        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(git(self.repo, 'rev-parse', 'HEAD'), git(self.repo, 'rev-parse', 'dev'))

    def test_failing_build_after_conflict_retry_keeps_the_branch(self):
        head = commit(self.repo, 'a.c', 'head\nx\nextern a;\ntail\n', 'shard match')
        git(self.repo, 'checkout', '-q', 'dev')
        commit(self.repo, 'a.c', 'head\nx\nextern b;\ntail\n', 'other match')
        commit(self.repo, 'a.c', 'head\nx\nextern b;\nextern a;\ntail\n', 'resolved pick')
        self.build.write_text('#!/bin/sh\nexit 1\n')
        commit(self.repo, 'tools/build-and-verify.sh', '#!/bin/sh\nexit 1\n', 'break build')
        git(self.repo, 'checkout', '-q', 'shard')

        proc = run(self.repo, 'dev')

        self.assertIn('conflicted', proc.stderr)
        self.assertEqual(git(self.repo, 'rev-parse', 'HEAD'), head)

    def src_base(self):
        (self.repo / 'src').mkdir()
        commit(self.repo, 'src/x.c', 'head\nINCLUDE_ASM("p", func_A);\ntail\n', 'stubs')
        git(self.repo, 'checkout', '-q', 'shard')
        git(self.repo, 'rebase', '-q', 'dev')

    def test_integrated_match_with_retyped_declaration_drops_out(self):
        # The integrator merged the shard's match with another shard's and kept the
        # other declaration type: the shard's exact extern line is gone, its match is not.
        git(self.repo, 'checkout', '-q', 'dev')
        self.src_base()
        commit(self.repo, 'src/x.c', 'head\nextern void* d;\nvoid func_A(void) {}\ntail\n', 'shard match')
        git(self.repo, 'checkout', '-q', 'dev')
        commit(self.repo, 'src/x.c', 'head\nextern u8* d;\nINCLUDE_ASM("p", func_A);\ntail\n', 'other match')
        commit(self.repo, 'src/x.c', 'head\nextern u8* d;\nvoid func_A(void) {}\ntail\n', 'resolved pick')
        git(self.repo, 'checkout', '-q', 'shard')

        proc = run(self.repo, 'dev')

        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(git(self.repo, 'rev-parse', 'HEAD'), git(self.repo, 'rev-parse', 'dev'))

    def test_sync_that_restores_an_include_asm_keeps_the_branch(self):
        git(self.repo, 'checkout', '-q', 'dev')
        self.src_base()
        head = commit(self.repo, 'src/x.c', 'head\nvoid func_A(void) {}\ntail\n', 'shard match')
        git(self.repo, 'checkout', '-q', 'dev')
        commit(self.repo, 'src/x.c', 'head\nINCLUDE_ASM("p", func_A); /* note */\ntail\n', 'dev edit')
        git(self.repo, 'checkout', '-q', 'shard')

        proc = run(self.repo, 'dev')

        self.assertIn('conflicted', proc.stderr)
        self.assertEqual(git(self.repo, 'rev-parse', 'HEAD'), head)


if __name__ == '__main__':
    unittest.main()
