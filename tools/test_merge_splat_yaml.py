#!/usr/bin/env python3
"""Tests for the merge-splat-yaml git merge driver. Hermetic: every repository is built in a tempdir.

Each test cherry-picks one shard's subsegment split onto dev after dev split a
neighbouring range, which is how parallel shards conflict in mariogolf64.yaml.
"""

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

DRIVER = Path(__file__).resolve().parent / 'merge-splat-yaml'

BASE = '''segments:
  - name: overlay_0
    subsegments:
      - [0xC7A40, c, overlay_0/a]
      - [0xD8F70, data]
      - [0xDE2F0, .rodata, overlay_0/x]
      - [0xDE320, rodata]
      - [0xDE6D8, .rodata, overlay_0/a]
      - [0xDE708, rodata]
      - { start: 0xDE9B0, type: bss, vram: 0x802256B0 }
'''


def git(cwd, *args, env=None):
    return subprocess.run(['git', '-c', 'user.name=t', '-c', 'user.email=t@t', *args], cwd=cwd,
                          capture_output=True, text=True, env=env)


class MergeSplatYamlTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.repo = Path(tmp.name)
        self.file = self.repo / 'mariogolf64.yaml'
        (self.repo / 'tools').mkdir()
        shutil.copy(DRIVER, self.repo / 'tools' / DRIVER.name)
        (self.repo / '.gitattributes').write_text('mariogolf64.yaml merge=splat-yaml\n')
        git(self.repo, 'init', '-q', '-b', 'dev')
        git(self.repo, 'config', 'merge.splat-yaml.driver', f'tools/{DRIVER.name} %O %A %B %L')
        self.commit(BASE, 'base')

    def commit(self, text, message):
        self.file.write_text(text)
        git(self.repo, 'add', '-A')
        git(self.repo, 'commit', '-q', '-m', message)

    def pick(self, dev_text, shard_text, env=None):
        git(self.repo, 'checkout', '-q', '-b', 'shard')
        self.commit(shard_text, 'shard split')
        git(self.repo, 'checkout', '-q', 'dev')
        self.commit(dev_text, 'dev split')
        return git(self.repo, 'cherry-pick', 'shard', env=env)

    def test_neighbouring_splits_merge_by_offset(self):
        # dev widens x's rodata; the shard splits a new file b and moves a's rodata start.
        dev = BASE.replace('[0xDE320, rodata]', '[0xDE380, rodata]')
        shard = BASE.replace('      - [0xC7A40, c, overlay_0/a]\n',
                             '      - [0xC7A40, c, overlay_0/a]\n      - [0xC9670, c, overlay_0/b]\n')
        shard = shard.replace('      - [0xDE6D8, .rodata, overlay_0/a]\n',
                              '      - [0xDE5B8, .rodata, overlay_0/a]\n      - [0xDE610, rodata]\n'
                              '      - [0xDE6D8, .rodata, overlay_0/b]\n')
        result = self.pick(dev, shard)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.file.read_text(), BASE
                         .replace('      - [0xC7A40, c, overlay_0/a]\n',
                                  '      - [0xC7A40, c, overlay_0/a]\n      - [0xC9670, c, overlay_0/b]\n')
                         .replace('      - [0xDE320, rodata]\n      - [0xDE6D8, .rodata, overlay_0/a]\n',
                                  '      - [0xDE380, rodata]\n      - [0xDE5B8, .rodata, overlay_0/a]\n'
                                  '      - [0xDE610, rodata]\n      - [0xDE6D8, .rodata, overlay_0/b]\n'))

    def test_same_offset_changed_differently_stays_conflicted(self):
        dev = BASE.replace('[0xDE320, rodata]', '[0xDE320, .rodata, overlay_0/y]\n      - [0xDE340, rodata]')
        shard = BASE.replace('[0xDE320, rodata]', '[0xDE320, .rodata, overlay_0/z]\n      - [0xDE360, rodata]')
        result = self.pick(dev, shard)
        self.assertNotEqual(result.returncode, 0)
        text = self.file.read_text()
        self.assertIn('<<<<<<< ', text)
        self.assertNotIn('|||||||', text)

    def test_favor_resolves_remaining_blocks_to_that_side(self):
        dev = BASE.replace('[0xDE320, rodata]', '[0xDE320, .rodata, overlay_0/y]\n      - [0xDE340, rodata]')
        shard = BASE.replace('[0xDE320, rodata]', '[0xDE320, .rodata, overlay_0/z]\n      - [0xDE360, rodata]')
        self.pick(dev, shard, env=dict(os.environ, MG64_MERGE_FAVOR='ours'))
        self.assertEqual(self.file.read_text(), dev)  # the pick is then empty, like -X ours


if __name__ == '__main__':
    unittest.main()
