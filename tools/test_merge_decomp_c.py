#!/usr/bin/env python3
"""Tests for the merge-decomp-c git merge driver. Hermetic: every repository is built in a tempdir.

Each test cherry-picks one shard's match onto dev after dev matched a neighbouring
function, which is how parallel shards conflict.
"""

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

DRIVER = Path(__file__).resolve().parent / 'merge-decomp-c'

BASE = '''#include "common.h"

extern s32 D_0;

INCLUDE_ASM("asm/f", func_A);

INCLUDE_ASM("asm/f", func_B);

INCLUDE_ASM("asm/f", func_C);
'''


def git(cwd, *args, env=None):
    return subprocess.run(['git', '-c', 'user.name=t', '-c', 'user.email=t@t', *args], cwd=cwd,
                          capture_output=True, text=True, env=env)


class MergeDecompCTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.repo = Path(tmp.name)
        self.file = self.repo / 'src' / 'f.c'
        (self.repo / 'tools').mkdir()
        self.file.parent.mkdir()
        shutil.copy(DRIVER, self.repo / 'tools' / DRIVER.name)
        (self.repo / '.gitattributes').write_text('src/**/*.c merge=decomp-c\n')
        git(self.repo, 'init', '-q', '-b', 'dev')
        git(self.repo, 'config', 'merge.decomp-c.driver', f'tools/{DRIVER.name} %O %A %B %L')
        self.commit(BASE, 'base')

    def commit(self, text, message):
        self.file.write_text(text)
        git(self.repo, 'add', '-A')
        git(self.repo, 'commit', '-q', '-m', message)

    def pick(self, dev_text, shard_text, env=None):
        """Commit shard_text on a shard branch and dev_text on dev, then cherry-pick the shard's."""
        git(self.repo, 'checkout', '-q', '-b', 'shard')
        self.commit(shard_text, 'shard match')
        git(self.repo, 'checkout', '-q', 'dev')
        self.commit(dev_text, 'dev match')
        return git(self.repo, 'cherry-pick', 'shard', env=env)

    def test_neighbouring_matches_merge_with_declarations_combined(self):
        # Each side declares what its function uses right above it, so the hunks touch.
        dev = BASE.replace('INCLUDE_ASM("asm/f", func_A);\n',
                           'extern s32 D_1;\nextern s32 D_0;\n\nvoid func_A(void) {\n    D_1 = D_0;\n}\n')
        shard = BASE.replace('INCLUDE_ASM("asm/f", func_A);\n',
                             'INCLUDE_ASM("asm/f", func_A);\nextern s32 D_1;\nextern s32 D_2;\n')
        shard = shard.replace('INCLUDE_ASM("asm/f", func_B);', 'void func_B(void) {\n    D_1 = D_2;\n}')

        proc = self.pick(dev, shard)

        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertEqual(self.file.read_text(), '''#include "common.h"

extern s32 D_0;

extern s32 D_1;

void func_A(void) {
    D_1 = D_0;
}

extern s32 D_2;

void func_B(void) {
    D_1 = D_2;
}

INCLUDE_ASM("asm/f", func_C);
''')

    def conflicted(self, dev_extern, shard_extern):
        dev = BASE.replace('INCLUDE_ASM("asm/f", func_A);\n', f'{dev_extern}\n\nvoid func_A(void) {{\n}}\n')
        shard = BASE.replace('INCLUDE_ASM("asm/f", func_A);\n',
                             f'INCLUDE_ASM("asm/f", func_A);\n{shard_extern}\n')
        shard = shard.replace('INCLUDE_ASM("asm/f", func_B);', 'void func_B(void) {\n}')
        return dev, shard

    def test_symbol_declared_with_two_types_stays_conflicted(self):
        proc = self.pick(*self.conflicted('extern u8* D_1;', 'extern void* D_1;'))

        self.assertNotEqual(proc.returncode, 0)
        self.assertIn('<<<<<<<', self.file.read_text())

    def test_same_function_defined_differently_stays_conflicted(self):
        dev = BASE.replace('INCLUDE_ASM("asm/f", func_B);', 'void func_B(void) {\n    D_0 = 1;\n}')
        shard = BASE.replace('INCLUDE_ASM("asm/f", func_B);', 'void func_B(void) {\n    D_0 = 2;\n}')

        proc = self.pick(dev, shard)

        self.assertNotEqual(proc.returncode, 0)
        self.assertIn('<<<<<<<', self.file.read_text())

    def test_variable_definition_is_not_merged_as_a_declaration(self):
        # Data definitions are laid out in source order; merging them is not mechanical.
        proc = self.pick(*self.conflicted('extern s32 D_1;', 's32 D_2;'))

        self.assertNotEqual(proc.returncode, 0)
        self.assertIn('<<<<<<<', self.file.read_text())

    def test_favor_resolves_what_the_rules_cannot(self):
        env = dict(os.environ, MG64_MERGE_FAVOR='ours')
        proc = self.pick(*self.conflicted('extern u8* D_1;', 'extern void* D_1;'), env=env)

        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        text = self.file.read_text()
        self.assertIn('extern u8* D_1;', text)
        self.assertNotIn('void* D_1', text)


if __name__ == '__main__':
    unittest.main()
