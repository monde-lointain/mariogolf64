#!/usr/bin/env python3
"""Tests for the stall stop in tools/decomp-env-template/build.sh.

Runs the real build.sh inside a decomp-environment-shaped directory of a throwaway project root.
The root's compiler is a stub that assembles the candidate, which is written as MIPS assembly, so
scoring is real but needs no KMC toolchain. Needs the mips-linux-gnu binutils, make and flock;
the tests are skipped without them.
"""

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
TEMPLATE = REPO / 'tools' / 'decomp-env-template'
HELPERS = ('build.sh', 'dist.py', 'normalize_asm.py', 'objdump.py')
FUNCTION = 'demo_fn'


TARGET_ASM = f'''    .macro glabel name
    .globl \\name
\\name:
    .endm
    .set noreorder
glabel {FUNCTION}
    addu $v0, $a0, $a1
    jr $ra
    nop
'''
# One wrong register: scores below 100% and never matches, so every attempt after the first is a
# non-improvement.
CANDIDATE_ASM = TARGET_ASM.replace('$a1', '$a0')
# A branch makes the target's absolute addresses depend on where it sits in the object.
BRANCHING_TARGET_ASM = f'''    .macro glabel name
    .globl \\name
\\name:
    .endm
    .set noreorder
glabel {FUNCTION}
    beqz $a0, 1f
    nop
    addu $v0, $a0, $a1
1:
    jr $ra
    nop
'''
# GCC emits a nested function as a local text symbol placed before its parent. The target object
# holds only the parent.
CHILD_ASM = '''    .local child_fn
child_fn:
    addu $v0, $a0, $a0
    jr $ra
    nop
'''
KMC_STUB = '''#!/bin/sh
# Stand-in for tools/kmc-cc: the candidate is already assembly.
out=""
src=""
while [ $# -gt 0 ]; do
    case "$1" in
        -o) out="$2"; shift 2 ;;
        -*) shift ;;
        *) src="$1"; shift ;;
    esac
done
exec mips-linux-gnu-as -EB -march=vr4300 -mtune=vr4300 -o "$out" "$src"
'''
TOOLS = ('make', 'flock', 'mips-linux-gnu-as', 'mips-linux-gnu-nm', 'mips-linux-gnu-objdump')


@unittest.skipUnless(all(shutil.which(tool) for tool in TOOLS), 'needs make, flock and mips-linux-gnu binutils')
class StallStopTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        root = Path(tmp.name)
        (root / 'Makefile').write_text('print-cflags:\n\t@echo -O2\n')
        (root / 'tools').mkdir()
        kmc = root / 'tools' / 'kmc-cc'
        kmc.write_text(KMC_STUB)
        kmc.chmod(0o755)
        self.env = root / 'nonmatchings' / FUNCTION
        self.env.mkdir(parents=True)
        for name in HELPERS:
            (self.env / name).symlink_to(TEMPLATE / name)
        (self.env / 'target.s').write_text(TARGET_ASM)
        subprocess.run(['mips-linux-gnu-as', '-EB', '-march=vr4300', '-o', 'target.o', 'target.s'],
                       cwd=self.env, check=True)
        self.attempts = 0

    def attempt(self, env=None, name=None, content=CANDIDATE_ASM):
        """Score the next base_<N>.c attempt (or `name`) with `content`."""
        if name is None:
            self.attempts += 1
            name = f'base_{self.attempts}.c'
        (self.env / name).write_text(content)
        run_env = {key: value for key, value in os.environ.items() if key != 'MG64_STALL_LIMIT'}
        run_env.update(env or {})
        return subprocess.run(['bash', './build.sh', name], cwd=self.env, env=run_env,
                              capture_output=True, text=True)

    @staticmethod
    def stop_lines(proc):
        return [line for line in proc.stdout.splitlines() if line.startswith('STOP:')]

    def test_local_text_symbol_is_scored_like_a_global_one(self):
        # GCC nested functions are emitted as local symbols (nm `t`), not global `T`.
        local_asm = TARGET_ASM.replace('    .globl \\name\n', '')
        proc = self.attempt(name='base_1.c', content=local_asm)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertIn('Verified exact match: yes', proc.stdout)

    def test_stop_line_once_env_limit_is_reached_and_repeats(self):
        runs = [self.attempt({'MG64_STALL_LIMIT': '2'}) for _ in range(4)]
        for proc in runs:
            self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertEqual(self.stop_lines(runs[0]), [])
        self.assertEqual(self.stop_lines(runs[1]), [])
        self.assertEqual(len(self.stop_lines(runs[2])), 1)
        self.assertEqual(len(self.stop_lines(runs[3])), 1)

    def use_target(self, asm):
        (self.env / 'target.s').write_text(asm)
        subprocess.run(['mips-linux-gnu-as', '-EB', '-march=vr4300', '-o', 'target.o', 'target.s'],
                       cwd=self.env, check=True)

    def test_local_child_before_target_does_not_count_against_it(self):
        self.use_target(BRANCHING_TARGET_ASM)
        candidate = BRANCHING_TARGET_ASM.replace(f'glabel {FUNCTION}', CHILD_ASM + f'glabel {FUNCTION}')
        proc = self.attempt(name='base_1.c', content=candidate)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertIn('Verified exact match: yes', proc.stdout)

    def test_child_before_target_still_counts_target_differences(self):
        self.use_target(BRANCHING_TARGET_ASM)
        wrong = BRANCHING_TARGET_ASM.replace('$a1', '$a0')
        candidate = wrong.replace(f'glabel {FUNCTION}', CHILD_ASM + f'glabel {FUNCTION}')
        proc = self.attempt(name='base_1.c', content=candidate)
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)
        self.assertIn('Verified exact match: no', proc.stdout)

    def test_match_log_format_is_unchanged(self):
        self.attempt({'MG64_STALL_LIMIT': '2'})
        lines = (self.env / 'match_log.txt').read_text().splitlines()
        self.assertEqual(len(lines), 1)
        name, score = lines[0].split()
        self.assertEqual(name, 'base_1.c')
        self.assertTrue(score.endswith('%'), lines[0])
        float(score[:-1])

    def test_limit_comes_from_run_json(self):
        (self.env / '.run.json').write_text('{"tier": 2, "stall_limit": 2}')
        runs = [self.attempt() for _ in range(3)]
        self.assertEqual(self.stop_lines(runs[1]), [])
        self.assertEqual(len(self.stop_lines(runs[2])), 1)

    def test_env_limit_overrides_run_json(self):
        (self.env / '.run.json').write_text('{"stall_limit": 2}')
        runs = [self.attempt({'MG64_STALL_LIMIT': '4'}) for _ in range(5)]
        self.assertEqual([i for i, proc in enumerate(runs, 1) if self.stop_lines(proc)], [5])

    def test_default_limit_is_twenty(self):
        runs = [self.attempt() for _ in range(21)]
        self.assertEqual([i for i, proc in enumerate(runs, 1) if self.stop_lines(proc)], [21])

    def test_stop_line_names_the_ledger_record_step(self):
        runs = [self.attempt({'MG64_STALL_LIMIT': '1'}) for _ in range(2)]
        (line,) = self.stop_lines(runs[1])
        self.assertIn(f'difficulty_ledger.py" record {FUNCTION} --outcome stalled --reason', line)
        self.assertIn('Do not make another attempt', line)
        self.assertNotIn('Consider stopping', line)

    def test_invalid_env_limits_are_errors_before_scoring(self):
        for value in ('soon', '0', '-3', '2.5'):
            with self.subTest(MG64_STALL_LIMIT=value):
                proc = self.attempt({'MG64_STALL_LIMIT': value}, name='base_1.c')
                self.assertNotEqual(proc.returncode, 0)
                self.assertIn('ERROR', proc.stdout + proc.stderr)
                self.assertFalse((self.env / 'match_log.txt').exists())

    def test_invalid_run_json_limits_are_errors_before_scoring(self):
        for text in ('{"stall_limit": ', '{"stall_limit": 0}', '{"stall_limit": "many"}', '[1, 2]'):
            with self.subTest(run_json=text):
                (self.env / '.run.json').write_text(text)
                proc = self.attempt(name='base_1.c')
                self.assertNotEqual(proc.returncode, 0)
                self.assertIn('ERROR', proc.stdout + proc.stderr)
                self.assertFalse((self.env / 'match_log.txt').exists())


if __name__ == '__main__':
    unittest.main()
