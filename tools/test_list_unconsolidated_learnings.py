#!/usr/bin/env python3
"""Tests for list_unconsolidated_learnings.py. Hermetic: every tree is built in a tempdir."""

import contextlib
import io
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import list_unconsolidated_learnings as lul  # noqa: E402


class LearningsTreeTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name).resolve()
        self.ledger = self.root / 'ledger' / 'consolidated-learnings.txt'

    def write(self, rel, text):
        path = self.root / 'nonmatchings' / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def unconsolidated(self):
        return lul.unconsolidated(self.root, self.ledger)

    def run_main(self, *argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = lul.main(list(argv), root=self.root, ledger=self.ledger)
        return code, out.getvalue()


class ListUnconsolidatedTest(LearningsTreeTest):
    def test_lists_learnings_and_near_miss_notes(self):
        self.write('a/LEARNINGS.md', 'stack frames differ\n')
        self.write('b/NEAR_MISS.md', 'off by one register\n')
        self.assertEqual(
            self.unconsolidated(),
            ['nonmatchings/a/LEARNINGS.md', 'nonmatchings/b/NEAR_MISS.md'],
        )

    def test_skips_blank_notes_and_other_names(self):
        self.write('c/LEARNINGS.md', '\n  \n')
        self.write('d/notes.md', 'not a learnings file\n')
        self.write('e/f/LEARNINGS.md', 'nested too deep\n')
        self.write('LEARNINGS.md', 'directly under nonmatchings\n')
        self.assertEqual(self.unconsolidated(), [])


class ConsolidationLedgerTest(LearningsTreeTest):
    def test_marked_note_is_no_longer_listed(self):
        self.write('a/LEARNINGS.md', 'lesson\n')
        lul.mark_consolidated(self.root, self.ledger, 'nonmatchings/a/LEARNINGS.md')
        self.assertEqual(self.unconsolidated(), [])

    def test_edited_note_becomes_unconsolidated_again(self):
        path = self.write('a/LEARNINGS.md', 'lesson\n')
        lul.mark_consolidated(self.root, self.ledger, 'nonmatchings/a/LEARNINGS.md')
        path.write_text('lesson and a second one\n')
        self.assertEqual(self.unconsolidated(), ['nonmatchings/a/LEARNINGS.md'])

    def test_mark_is_idempotent_and_keeps_other_entries(self):
        self.write('a/LEARNINGS.md', 'one\n')
        self.write('b/NEAR_MISS.md', 'two\n')
        lul.mark_consolidated(self.root, self.ledger, 'nonmatchings/a/LEARNINGS.md')
        lul.mark_consolidated(self.root, self.ledger, 'nonmatchings/b/NEAR_MISS.md')
        lul.mark_consolidated(self.root, self.ledger, 'nonmatchings/a/LEARNINGS.md')
        self.assertEqual(len(self.ledger.read_text().splitlines()), 2)
        self.assertEqual(self.unconsolidated(), [])

    def test_mark_accepts_absolute_path(self):
        path = self.write('a/LEARNINGS.md', 'lesson\n')
        lul.mark_consolidated(self.root, self.ledger, str(path))
        self.assertEqual(self.unconsolidated(), [])

    def test_mark_rejects_files_outside_nonmatchings(self):
        self.write('a/notes.md', 'wrong name\n')
        outside = self.root / 'src' / 'main' / 'LEARNINGS.md'
        outside.parent.mkdir(parents=True)
        outside.write_text('wrong place\n')
        for bad in ('nonmatchings/a/notes.md', 'src/main/LEARNINGS.md', 'nonmatchings/a/x/LEARNINGS.md'):
            with self.subTest(path=bad), self.assertRaises(ValueError):
                lul.mark_consolidated(self.root, self.ledger, bad)
        self.assertFalse(self.ledger.exists())

    def test_mark_missing_note_fails(self):
        with self.assertRaises(FileNotFoundError):
            lul.mark_consolidated(self.root, self.ledger, 'nonmatchings/ghost/LEARNINGS.md')
        self.assertFalse(self.ledger.exists())


class CommandLineTest(LearningsTreeTest):
    def test_default_output_is_one_path_per_line(self):
        self.write('a/LEARNINGS.md', 'lesson\n')
        code, out = self.run_main()
        self.assertEqual(code, 0)
        self.assertEqual(out.splitlines(), ['nonmatchings/a/LEARNINGS.md'])

    def test_json_output_is_array_of_repo_paths(self):
        self.write('a/LEARNINGS.md', 'lesson\n')
        self.write('b/NEAR_MISS.md', 'near\n')
        code, out = self.run_main('--json')
        self.assertEqual(code, 0)
        self.assertEqual(
            json.loads(out),
            ['nonmatchings/a/LEARNINGS.md', 'nonmatchings/b/NEAR_MISS.md'],
        )

    def test_mark_then_listing_is_empty_json(self):
        self.write('a/LEARNINGS.md', 'lesson\n')
        self.assertEqual(self.run_main('--mark', 'nonmatchings/a/LEARNINGS.md')[0], 0)
        code, out = self.run_main('--json')
        self.assertEqual((code, json.loads(out)), (0, []))

    def test_mark_error_exits_nonzero(self):
        self.write('a/notes.md', 'wrong name\n')
        code, out = self.run_main('--mark', 'nonmatchings/a/notes.md')
        self.assertNotEqual(code, 0)
        self.assertEqual(out, '')


class LedgerLocationTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.base = Path(tmp.name).resolve()

    def git(self, *args, cwd):
        subprocess.run(['git', *args], cwd=cwd, check=True, capture_output=True, text=True)

    def test_worktrees_share_one_ledger(self):
        main = self.base / 'main'
        main.mkdir()
        self.git('init', '-q', cwd=main)
        self.git('-c', 'user.name=t', '-c', 'user.email=t@example.invalid',
                 'commit', '-q', '--allow-empty', '-m', 'init', cwd=main)
        worktree = self.base / 'wt'
        self.git('worktree', 'add', '-q', '--detach', str(worktree), cwd=main)

        main_ledger = lul.ledger_path(main)
        self.assertEqual(main_ledger.resolve(), (main / '.git' / 'mg64' / 'consolidated-learnings.txt').resolve())
        self.assertEqual(lul.ledger_path(worktree).resolve(), main_ledger.resolve())


if __name__ == '__main__':
    unittest.main()
