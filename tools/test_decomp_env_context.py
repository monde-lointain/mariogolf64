#!/usr/bin/env python3
"""Behavior tests for tools/decomp_env_context.py against a fake repository."""

import contextlib
import io
import json
import os
import stat
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import decomp_env_context as ctx  # noqa: E402

SCRIPT = Path(__file__).resolve().parent / 'decomp_env_context.py'

C_HEADER = '''#ifndef FAKE_H
#define FAKE_H

s32 target_fn(s32 a);
s32 target_fn_helper(s32 a);

#endif
'''

C_TARGET = '''#include "fake.h"

s32 target_fn(s32 a)
{
    if (a > 0)
    {
        return target_fn(a - 1);
    }
    return target_fn_helper(a);
}
'''

C_CALLER = '''#include "fake.h"

// target_fn is only mentioned in this comment
s32 caller_one(s32 a)
{
    return target_fn(a) * 2;
}

s32 c_done(void)
{
    return target_fn_helper(1);
}
'''

C_WRAPPER = '''#include "fake.h"

INCLUDE_ASM("asm/nonmatchings/main/target_fn", target_fn);

s32 wrap_one(s32 a)
{
    return target_fn(a);
}
'''

SHADOW_CALLER = 's32 shadow(void)\n{\n    return target_fn(2);\n}\n'
SKIPPED_CALLER = 's32 skipped_caller(void)\n{\n    return target_fn(3);\n}\n'


def func_source(name, lines, body='    x += 1;'):
    """A C function spanning exactly `lines` lines."""
    return '\n'.join([f's32 {name}(void)', '{', *([body] * (lines - 3)), '}']) + '\n'


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


def line_of(text, needle):
    for number, line in enumerate(text.splitlines(), 1):
        if needle in line:
            return number
    raise AssertionError(f'{needle!r} not in fixture')


def make_repo(root):
    files = {
        'include/fake.h': C_HEADER,
        'src/main/target.c': C_TARGET,
        'src/main/caller.c': C_CALLER,
        'src/main/wrapper.c': C_WRAPPER,
        'src/main/ref_small.c': func_source('ref_small', 4, body='    return a;').replace('(void)', '(s32 a)'),
        'src/main/ref_exact.c': func_source('ref_exact', 80),
        'src/main/ref_long.c': func_source('ref_long', 81),
        'src/nonmatchings/shadow.c': SHADOW_CALLER,
        'asm/nonmatchings/main/target_fn.s': 'nonmatching target_fn, 0x80000000\n    jal target_fn\n',
        'asm/nonmatchings/main/undec_a.s': 'nonmatching undec_a, 0x80000100\n    jal target_fn\n',
        'asm/nonmatchings/main/c_done.s': 'nonmatching c_done, 0x80000200\n    jal target_fn\n',
        'asm/nonmatchings/main/no_call.s': 'nonmatching no_call, 0x80000300\n    jal other_fn\n',
        'asm/nonmatchings/main/near_miss.s': 'nonmatching near_miss, 0x80000400\n    jal target_fn_helper\n',
    }
    for name in sorted(ctx.SKIPPED_BASENAMES):
        files[f'src/main/{name}'] = SKIPPED_CALLER
    for rel, text in files.items():
        write(root / rel, text)


def ledger_record(function, ts, tier, outcome, best_pct, reason):
    return {
        'ts': ts, 'function': function, 'task': 'task', 'tier': tier, 'model': 'model',
        'run_id': 'run', 'attempts': 1, 'best_pct': best_pct, 'outcome': outcome,
        'reason': reason, 'env': 'env', 'worktree': 'worktree',
    }


class ParseRefsTest(unittest.TestCase):
    def test_json_array(self):
        self.assertEqual(ctx.parse_refs('["alpha", "beta_2"]'), ['alpha', 'beta_2'])

    def test_comma_and_space_separated(self):
        self.assertEqual(ctx.parse_refs('alpha, beta gamma'), ['alpha', 'beta', 'gamma'])

    def test_bracketed_list_without_json_quotes(self):
        self.assertEqual(ctx.parse_refs('[alpha, beta]'), ['alpha', 'beta'])

    def test_non_string_json_falls_back_to_splitting(self):
        self.assertEqual(ctx.parse_refs('["alpha", 3]'), ['alpha'])

    def test_empty_forms_give_no_names(self):
        for value in (None, '', '   ', '[]'):
            with self.subTest(value=value):
                self.assertEqual(ctx.parse_refs(value), [])

    def test_invalid_and_duplicate_names_are_dropped(self):
        self.assertEqual(ctx.parse_refs('1bad a-b good good'), ['good'])


class ContextTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name).resolve()
        make_repo(self.root)
        self.env = self.root / 'nonmatchings' / 'target_fn-x'
        self.env.mkdir(parents=True)
        self.ledger = self.root / 'ledger.jsonl'  # absent unless a test writes it
        self.environ = {'MG64_LEDGER': str(self.ledger)}

    def run_context(self, function='target_fn', refs=(), env=None, build_timeout=30):
        return ctx.build_context(
            self.root, env or self.env, function, list(refs),
            environ=self.environ, build_timeout=build_timeout,
        )

    def write_build(self, body):
        script = self.env / 'build.sh'
        script.write_text('#!/bin/sh\n' + body)
        script.chmod(stat.S_IRWXU)

    def write_ledger(self, records):
        self.ledger.write_text(''.join(json.dumps(rec) + '\n' for rec in records))

    # callers and declarations

    def test_callers_list_path_line_and_text(self):
        out = self.run_context()
        caller_line = line_of(C_CALLER, 'return target_fn(a) * 2;')
        wrap_line = line_of(C_WRAPPER, 'return target_fn(a);')
        self.assertIn(f'src/main/caller.c:{caller_line}: return target_fn(a) * 2;', out)
        self.assertIn(f'src/main/wrapper.c:{wrap_line}: return target_fn(a);', out)

    def test_callers_skip_comments_includes_own_body_and_scratch_files(self):
        out = self.run_context()
        self.assertNotIn('mentioned in this comment', out)
        self.assertNotIn('INCLUDE_ASM', out)
        self.assertNotIn('src/main/target.c:', out)  # definition and its recursive call
        self.assertNotIn('target_fn_helper', out)  # word boundary: a different name
        self.assertNotIn('return target_fn(2);', out)  # under src/nonmatchings/
        for name in sorted(ctx.SKIPPED_BASENAMES):
            self.assertNotIn('return target_fn(3);', out, name)

    def test_callers_cap_at_twelve_with_more_count(self):
        many = ''.join(f's32 many_{i:02d}(s32 a) {{ return target_fn(a); }}\n' for i in range(14))
        write(self.root / 'src/main/many.c', many)
        out = self.run_context()
        caller_lines = [line for line in out.splitlines() if line.startswith('src/')]
        self.assertEqual(len(caller_lines), 12)
        self.assertIn('(4 more)', out)

    def test_declarations_listed_in_their_own_section(self):
        out = self.run_context()
        decl_line = line_of(C_HEADER, 's32 target_fn(s32 a);')
        self.assertIn('<declarations>\n' f'include/fake.h:{decl_line}: s32 target_fn(s32 a);\n', out)
        self.assertNotIn('fake.h:', out.split('<declarations>')[0])

    def test_undecompiled_callers_list_stubs_that_call_the_function(self):
        out = self.run_context()
        self.assertIn('\nUndecompiled callers (asm): undec_a\n', out)

    def test_undecompiled_callers_cap_at_eight_with_more_count(self):
        for i in range(9):
            write(self.root / f'asm/nonmatchings/main/undec_{i:02d}.s',
                  f'nonmatching undec_{i:02d}, 0x8000{i:04x}\n    jal target_fn\n')
        out = self.run_context()
        names = [f'undec_{i:02d}' for i in range(8)]
        expected = f'Undecompiled callers (asm): {", ".join(names)} (2 more)'
        self.assertIn(f'\n{expected}\n', out)

    # references

    def test_reference_body_included_within_the_80_line_limit(self):
        out = self.run_context(refs=['ref_small', 'ref_exact'])
        self.assertIn('<reference name="ref_small" path="src/main/ref_small.c" lines="1-4">', out)
        self.assertIn('    return a;', out)
        self.assertIn('<reference name="ref_exact" path="src/main/ref_exact.c" lines="1-80">', out)

    def test_reference_over_the_limit_is_path_only(self):
        out = self.run_context(refs=['ref_long'])
        self.assertIn('<reference name="ref_long" path="src/main/ref_long.c" lines="1-81"/>', out)
        self.assertNotIn('x += 1', out)

    def test_unknown_and_own_names_are_skipped(self):
        out = self.run_context(refs=['no_such_fn', 'target_fn'])
        self.assertNotIn('name="no_such_fn"', out)
        self.assertNotIn('name="target_fn"', out)
        self.assertNotIn('<references>', out)

    def test_references_keep_argument_order(self):
        out = self.run_context(refs=['ref_exact', 'ref_small'])
        self.assertLess(out.index('name="ref_exact"'), out.index('name="ref_small"'))

    # budget

    def test_budget_drops_reference_bodies_until_under_limit(self):
        wide = '    x += 1;  /* ' + 'p' * 60 + ' */'
        write(self.root / 'src/main/bulky_a.c', func_source('bulky_a', 80, body=wide))
        write(self.root / 'src/main/bulky_b.c', func_source('bulky_b', 80, body=wide))
        out = self.run_context(refs=['ref_small', 'bulky_a', 'bulky_b'])
        self.assertLessEqual(len(out.encode('utf-8')), ctx.BUDGET_BYTES)
        self.assertEqual(out.count('/>'), 1)  # one body dropped, its reference kept path-only
        self.assertIn('    return a;', out)  # the small body survives

    def test_budget_drops_trailing_references_when_paths_alone_overflow(self):
        one_liners = ''.join(f's32 rf_{i:03d}(void) {{ return 0; }}\n' for i in range(150))
        write(self.root / 'src/main/many_refs.c', one_liners)
        out = self.run_context(refs=[f'rf_{i:03d}' for i in range(150)])
        self.assertLessEqual(len(out.encode('utf-8')), ctx.BUDGET_BYTES)
        self.assertIn('name="rf_000"', out)
        self.assertNotIn('name="rf_149"', out)
        self.assertIn('</references>', out)

    def test_empty_context_renders_nothing(self):
        self.assertEqual(self.run_context(function='lonely_fn'), '')

    # baseline

    def test_baseline_reports_score_and_next_line(self):
        self.write_build('echo "compiling base.c"\necho "NEXT: copy base.c"\necho "Score: 91.234% (12 differences)"\nexit 0\n')
        out = self.run_context()
        self.assertIn('<baseline>\nScore: 91.234% (12 differences)\nNEXT: copy base.c\n</baseline>', out)
        self.assertNotIn('compiling base.c', out)

    def test_baseline_failure_shows_first_fifteen_lines_with_paths_shortened(self):
        body = 'echo "$(pwd)/base.c:4: error: nope"\n' + ''.join(f'echo "line {i}"\n' for i in range(1, 21))
        self.write_build(body + 'exit 2\n')
        out = self.run_context()
        self.assertIn('\nbase.c:4: error: nope\n', out)
        self.assertNotIn(str(self.env), out)
        self.assertIn('\nline 14\n', out)
        self.assertNotIn('\nline 15\n', out)  # 1 error line + 14 build lines = 15 shown

    def test_baseline_failure_with_no_output_reports_exit_status(self):
        self.write_build('exit 3\n')
        self.assertIn('<baseline>\nbuild.sh exited 3 with no output\n</baseline>', self.run_context())

    def test_baseline_timeout_is_reported_not_raised(self):
        self.write_build('exec sleep 5\n')
        out = self.run_context(build_timeout=1)
        self.assertIn('build.sh base.c timed out after 1 s', out)

    def test_baseline_omitted_without_build_script(self):
        self.assertNotIn('<baseline>', self.run_context())

    # prior attempts

    def test_prior_attempts_newest_first_capped_at_five_and_first_in_output(self):
        self.write_ledger([
            ledger_record('target_fn', '2025-01-03T00:00:00Z', 1, 'stalled', 81.25, 'stall at 81'),
            ledger_record('target_fn', '2025-01-01T00:00:00Z', 1, 'timeout', 40.0, 'timeout early'),
            ledger_record('other_fn', '2025-01-09T00:00:00Z', 2, 'matched', 100.0, 'other function'),
            ledger_record('target_fn', '2025-01-05T00:00:00Z', 2, 'near_miss', 99.5, 'one diff left'),
            ledger_record('target_fn', '2025-01-02T00:00:00Z', 2, 'gave_up', 50.0, 'gave up'),
            ledger_record('target_fn', '2025-01-04T00:00:00Z', 3, 'integration_failed', 92.0, 'link error'),
            ledger_record('target_fn', '2025-01-06T00:00:00Z', 3, 'matched', 100.0, 'matched it'),
        ])
        out = self.run_context()
        expected = '\n'.join([
            '<prior_attempts>',
            'tier 3 matched best 100.000%: matched it',
            'tier 2 near_miss best 99.500%: one diff left',
            'tier 3 integration_failed best 92.000%: link error',
            'tier 1 stalled best 81.250%: stall at 81',
            'tier 2 gave_up best 50.000%: gave up',
            '</prior_attempts>',
            '',
        ])
        self.assertTrue(out.startswith(expected))
        self.assertNotIn('timeout early', out)
        self.assertNotIn('other function', out)
        self.assertLess(out.index('<prior_attempts>'), out.index('<callers>'))

    def test_missing_ledger_omits_section_silently(self):
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            out = self.run_context()
        self.assertNotIn('<prior_attempts>', out)
        self.assertEqual(stderr.getvalue(), '')

    def test_corrupt_ledger_warns_and_keeps_the_rest_of_the_context(self):
        self.ledger.write_text('not json\n')
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            out = self.run_context()
        self.assertNotIn('<prior_attempts>', out)
        self.assertIn('<callers>', out)
        self.assertIn('prior attempts unavailable', stderr.getvalue())

    # command line

    def cli_env(self):
        env = dict(os.environ)
        env['MG64_LEDGER'] = str(self.ledger)
        return env

    def test_cli_prints_context_for_relative_env_from_repo_root(self):
        result = subprocess.run(
            [sys.executable, str(SCRIPT), '--env', 'nonmatchings/target_fn-x',
             '--function', 'target_fn', '--refs', '["ref_small"]'],
            cwd=self.root, env=self.cli_env(), capture_output=True, text=True, timeout=120,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(result.stdout.startswith('<callers>\n'))
        self.assertIn('<reference name="ref_small"', result.stdout)

    def test_cli_without_refs_uses_most_similar_matched_functions(self):
        body = ('    /* 0 80000000 27BDFFE8 */  addiu      $sp, $sp, -0x18\n'
                '    /* 4 80000004 03E00008 */  jr         $ra\n'
                '    /* 8 80000008 00000000 */  nop\n')
        write(self.root / 'asm/nonmatchings/main/target_fn.s', f'glabel target_fn\n{body}endlabel target_fn\n')
        write(self.root / 'asm/main/ref_small.s', f'glabel ref_small\n{body}endlabel ref_small\n')
        run = lambda *extra: subprocess.run(  # noqa: E731
            [sys.executable, str(SCRIPT), '--env', 'nonmatchings/target_fn-x', '--function', 'target_fn', *extra],
            cwd=self.root, env=self.cli_env(), capture_output=True, text=True, timeout=120,
        )
        default, empty = run(), run('--refs', '')
        self.assertEqual(default.returncode, 0, default.stderr)
        self.assertIn('<reference name="ref_small"', default.stdout)
        self.assertEqual(empty.returncode, 0, empty.stderr)
        self.assertNotIn('<references>', empty.stdout)

    def test_cli_missing_env_directory_fails(self):
        result = subprocess.run(
            [sys.executable, str(SCRIPT), '--env', 'nonmatchings/absent', '--function', 'target_fn'],
            cwd=self.root, env=self.cli_env(), capture_output=True, text=True, timeout=120,
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn('no workspace directory', result.stderr)


if __name__ == '__main__':
    unittest.main()
