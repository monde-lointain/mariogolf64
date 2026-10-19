#!/usr/bin/env python3
"""Try m2c on unmatched MG64 functions and inline exact matches.

For each function under asm/nonmatchings/ that a src C file pulls in with
INCLUDE_ASM, this script:
  1. decompiles its assembly with m2c (--target mips-gcc-c), using the owning
     C file's preprocessed context (m2ctx.import_c_file; no ctx.c is written);
  2. compiles the candidate with the owning file's ROM compiler profile and
     scores it against the target object (decomp-env-template/build.sh);
  3. for an exact match, replaces that function's INCLUDE_ASM line with the
     generated C, after confirming the owning file still compiles.

All scratch files live in a temporary directory. A C file is only written when
a match is verified and --dry-run is not given. Asm-only segments (asm/<rom>.s)
have no owning C file; they are considered only when named explicitly.

Examples (from the repo root):
  .venv/bin/python3 tools/m2c_one_shot.py --dry-run --limit 5
  .venv/bin/python3 tools/m2c_one_shot.py init_sky_pool_and_world_state --dry-run
"""

import argparse
import json
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))

import m2ctx  # noqa: E402
from find_similar_functions import collect_unmatched  # noqa: E402
from list_decomp_candidates import IncludeAsm, find_include_asm_in_text  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
ASM_NONMATCHINGS = 'asm/nonmatchings/'
HELPER_DIR = ROOT / 'tools' / 'decomp-env-template'
BUILD_SH = HELPER_DIR / 'build.sh'
PRELUDE_INC = HELPER_DIR / 'prelude.inc'
HELPER_FILES = ('objdump.py', 'normalize_asm.py', 'dist.py')
KMC_CC = ROOT / 'tools' / 'kmc-cc'
INCLUDE_DIR = ROOT / 'include'
M2C_TARGET = 'mips-gcc-c'
AS_FLAGS = ['-EB', '-march=vr4300', '-mtune=vr4300']
AS_NAMES = ('mips-linux-gnu-as', 'mips64-linux-gnu-as', 'mips64-elf-as')
GENERATED_HEADER = '#include "common.h"\n\n'

SCORE_RE = re.compile(r'^Score: ([0-9.]+)% \((\d+) differences\)', re.M)
EXACT_RE = re.compile(r'^Verified exact match: yes$', re.M)


@dataclass
class Candidate:
    name: str
    asm_rel: str
    owner: Optional[IncludeAsm]
    skip: str = ''  # skip reason; empty when eligible


@dataclass
class Outcome:
    name: str
    status: str
    detail: str
    where: str


def first_error_line(text: str) -> str:
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    for line in lines:
        if 'error' in line.lower():
            return line[:200]
    return lines[-1][:200] if lines else ''


def m2c_failure_reason(stdout: str, stderr: str) -> str:
    """m2c reports why it gave up on stdout as a /* ... */ comment; return its first line."""
    for line in (stderr + '\n' + stdout).splitlines():
        text = line.strip()
        if text and text not in ('/*', '*/') and not text.startswith('Decompilation failure'):
            return text[:200]
    return ''


def replace_include_asm(
    text: str, asm_rel: str, func: str, replacement: str
) -> Tuple[Optional[str], str]:
    """Swap the single INCLUDE_ASM statement for asm_rel/func with replacement.

    Returns (new_text, '') on success or (None, reason) when the statement is
    missing, duplicated, or shares its line with other code.
    """
    hits = [
        item for item in find_include_asm_in_text(text, '')
        if item.asm_rel == asm_rel and item.func == func
    ]
    if not hits:
        return None, 'INCLUDE_ASM not found'
    if len(hits) > 1:
        return None, 'INCLUDE_ASM appears more than once'
    lines = text.split('\n')
    index = hits[0].line - 1
    alone = re.compile(
        r'^\s*INCLUDE_ASM\s*\(\s*"[^"]+"\s*,\s*' + re.escape(func) + r'\s*\)\s*;?\s*$'
    )
    if not alone.match(lines[index]):
        return None, 'INCLUDE_ASM is not alone on its line'
    lines[index:index + 1] = replacement.split('\n')
    return '\n'.join(lines), ''


def discover(include_segments: bool) -> List[Candidate]:
    # collect_unmatched forces segments on for the asm/ root, so scope by directory.
    target = ROOT / 'asm' if include_segments else ROOT / 'asm' / 'nonmatchings'
    functions = collect_unmatched(ROOT, target, include_segments=include_segments)
    owners: Dict[Tuple[str, str], List[IncludeAsm]] = defaultdict(list)
    for c_path in sorted((ROOT / 'src').rglob('*.c')):
        rel = c_path.relative_to(ROOT).as_posix()
        for item in find_include_asm_in_text(c_path.read_text(errors='replace'), rel):
            owners[(item.asm_rel, item.func)].append(item)

    candidates: List[Candidate] = []
    for fn in functions.values():
        if not fn.file_path.startswith(ASM_NONMATCHINGS):
            candidates.append(Candidate(fn.name, fn.file_path, None, 'skip:asm-only-segment'))
            continue
        hits = owners.get((fn.file_path, fn.name), [])
        if not hits:
            candidates.append(Candidate(fn.name, fn.file_path, None, 'skip:no-c-owner'))
        elif len(hits) > 1:
            candidates.append(Candidate(fn.name, fn.file_path, None, 'skip:ambiguous-owner'))
        else:
            candidates.append(Candidate(fn.name, fn.file_path, hits[0]))
    return candidates


def select_candidates(names: List[str]) -> List[Candidate]:
    """Default: every nonmatchings function. Named: only those, segments included."""
    candidates = discover(include_segments=bool(names))
    if not names:
        return candidates

    by_name = {c.name: c for c in candidates}
    selected: List[Candidate] = []
    for arg in names:
        if arg.endswith('.s'):
            try:
                rel = Path(arg).resolve().relative_to(ROOT.resolve()).as_posix()
            except ValueError:
                raise SystemExit(f'error: {arg} is outside the repo')
            matches = [c for c in candidates if c.asm_rel == rel]
            if not matches:
                raise SystemExit(f'error: no unmatched function in {arg}')
        else:
            if arg not in by_name:
                raise SystemExit(f'error: no unmatched function named {arg}')
            matches = [by_name[arg]]
        for c in matches:
            if c not in selected:
                selected.append(c)
    return selected


def find_m2c() -> str:
    venv = ROOT / '.venv' / 'bin' / 'm2c'
    if venv.is_file():
        return str(venv)
    found = shutil.which('m2c')
    if found:
        return found
    raise SystemExit('error: m2c not found (.venv/bin/m2c or PATH)')


def find_as() -> str:
    for name in AS_NAMES:
        found = shutil.which(name)
        if found:
            return found
    raise SystemExit(f'error: MIPS assembler not found ({", ".join(AS_NAMES)})')


def profile_cflags(owner_rel: str) -> List[str]:
    out = subprocess.run(
        ['make', '-s', '--no-print-directory', 'print-cflags', f'SRC={owner_rel}'],
        cwd=ROOT, check=True, capture_output=True, text=True,
    ).stdout
    return shlex.split(out)


class OwnerContexts:
    """Per owning C file: m2c context path and compiler flags, computed once."""

    def __init__(self, workdir: Path):
        self.workdir = workdir
        self.cache: Dict[str, Tuple[Optional[Path], List[str], str]] = {}

    def get(self, owner_rel: str) -> Tuple[Optional[Path], List[str], str]:
        if owner_rel not in self.cache:
            self.cache[owner_rel] = self._load(owner_rel)
        return self.cache[owner_rel]

    def _load(self, owner_rel: str) -> Tuple[Optional[Path], List[str], str]:
        try:
            cflags = profile_cflags(owner_rel)
            text = m2ctx.import_c_file(str(ROOT / owner_rel))
        except SystemExit as exc:
            return None, [], f'context generation exited with {exc.code}'
        except subprocess.CalledProcessError as exc:
            return None, [], f'context or profile step failed (exit {exc.returncode})'
        self.workdir.mkdir(parents=True, exist_ok=True)
        ctx_path = self.workdir / owner_rel.replace('/', '__')
        ctx_path.write_text(text)
        return ctx_path, cflags, ''


def assemble_target(asm_path: Path, workdir: Path, as_bin: str) -> str:
    """Write target.o for one function. Returns '' on success, else an error line."""
    target_s = workdir / 'target.s'
    target_s.write_text(PRELUDE_INC.read_text() + '\n' + asm_path.read_text())
    proc = subprocess.run(
        [as_bin, *AS_FLAGS, '-I', str(INCLUDE_DIR), '-o', str(workdir / 'target.o'), str(target_s)],
        cwd=ROOT, capture_output=True, text=True,
    )
    return '' if proc.returncode == 0 else first_error_line(proc.stderr + proc.stdout)


def run_m2c(m2c: str, asm_path: Path, ctx_path: Path, timeout: float) -> Tuple[str, str]:
    """Return (generated C, '') or ('', reason)."""
    cmd = [m2c, '--target', M2C_TARGET, '--context', str(ctx_path), str(asm_path)]
    try:
        proc = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return '', f'timeout after {timeout:g}s'
    if proc.returncode != 0:
        return '', m2c_failure_reason(proc.stdout, proc.stderr) or f'exit {proc.returncode}'
    if not proc.stdout.strip():
        return '', 'empty output'
    return proc.stdout, ''


def score_candidate(c_text: str, owner_rel: str, workdir: Path, timeout: float) -> Tuple[str, str]:
    """Return (status, detail); status is match, miss:compile, miss:build or miss:score."""
    (workdir / 'base.c').write_text(GENERATED_HEADER + c_text)
    (workdir / '.source_file').write_text(owner_rel + '\n')
    try:
        proc = subprocess.run(
            ['bash', str(BUILD_SH), 'base.c'],
            cwd=workdir, capture_output=True, text=True, timeout=timeout,
        )
    except subprocess.TimeoutExpired:
        return 'miss:build', f'timeout after {timeout:g}s'
    output = proc.stdout + proc.stderr
    if proc.returncode != 0:
        return 'miss:compile', first_error_line(output)
    score = SCORE_RE.search(output)
    if not score:
        return 'miss:build', 'no score line'
    percent, differences = score.group(1), int(score.group(2))
    if EXACT_RE.search(output) and float(percent) == 100.0 and differences == 0:
        return 'match', ''
    return 'miss:score', f'{percent}% ({differences} differences)'


def owner_compile_error(owner_rel: str, new_text: str, cflags: List[str],
                        workdir: Path, timeout: float) -> str:
    """Compile the edited owning file in a scratch copy. Returns '' when it builds."""
    scratch = workdir / 'owner'
    scratch.mkdir(exist_ok=True)
    source = scratch / Path(owner_rel).name
    source.write_text(new_text)
    cmd = [str(KMC_CC), *cflags, '-I', str((ROOT / owner_rel).parent),
           str(source), '-o', str(scratch / 'owner.o')]
    try:
        proc = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return f'compile timeout after {timeout:g}s'
    return '' if proc.returncode == 0 else first_error_line(proc.stderr + proc.stdout)


def process(cand: Candidate, contexts: OwnerContexts, scratch_root: Path,
            m2c: str, as_bin: str, args: argparse.Namespace) -> Outcome:
    if cand.owner is None:
        return Outcome(cand.name, cand.skip, '', cand.asm_rel)

    owner_rel = cand.owner.source
    where = f'{owner_rel}:{cand.owner.line}'
    ctx_path, cflags, error = contexts.get(owner_rel)
    if ctx_path is None:
        return Outcome(cand.name, 'skip:context', error, where)

    workdir = Path(tempfile.mkdtemp(prefix=f'{cand.name}-', dir=scratch_root))
    for helper in HELPER_FILES:
        (workdir / helper).symlink_to(HELPER_DIR / helper)

    asm_path = ROOT / cand.asm_rel
    error = assemble_target(asm_path, workdir, as_bin)
    if error:
        return Outcome(cand.name, 'skip:target-asm', error, where)

    c_text, error = run_m2c(m2c, asm_path, ctx_path, args.m2c_timeout)
    if error:
        return Outcome(cand.name, 'miss:m2c', error, where)

    status, detail = score_candidate(c_text, owner_rel, workdir, args.build_timeout)
    if status != 'match':
        return Outcome(cand.name, status, detail, where)

    owner_path = ROOT / owner_rel
    new_text, error = replace_include_asm(
        owner_path.read_text(), cand.asm_rel, cand.name, c_text.strip('\n')
    )
    if new_text is None:
        return Outcome(cand.name, 'skip:include-line', error, where)

    error = owner_compile_error(owner_rel, new_text, cflags, workdir, args.build_timeout)
    if error:
        return Outcome(cand.name, 'skip:inline-compile', error, where)

    if args.dry_run:
        return Outcome(cand.name, 'match', f'dry-run: would inline into {where}', where)
    owner_path.write_text(new_text)
    return Outcome(cand.name, 'match', f'inlined into {where}', where)


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument(
        'functions', nargs='*',
        help='function names or asm/*.s paths (default: all nonmatchings functions)',
    )
    parser.add_argument('--limit', type=int, default=0,
                        help='attempt at most N eligible candidates (0 = all)')
    parser.add_argument('--dry-run', action='store_true',
                        help='score and verify, but do not edit any C file')
    parser.add_argument('--json', action='store_true', help='print results as JSON')
    parser.add_argument('--m2c-timeout', type=float, default=30.0,
                        help='seconds allowed per m2c run (default: 30)')
    parser.add_argument('--build-timeout', type=float, default=60.0,
                        help='seconds allowed per compile/score step (default: 60)')
    args = parser.parse_args(argv)

    m2c = find_m2c()
    as_bin = find_as()
    candidates = select_candidates(args.functions)

    selected: List[Candidate] = []
    attempts = 0
    for cand in candidates:
        if cand.skip:
            selected.append(cand)
            continue
        if args.limit and attempts >= args.limit:
            continue
        attempts += 1
        selected.append(cand)

    results: List[Outcome] = []
    with tempfile.TemporaryDirectory(prefix='m2c_one_shot.') as tmp:
        scratch_root = Path(tmp)
        contexts = OwnerContexts(scratch_root / 'contexts')
        for index, cand in enumerate(selected, 1):
            outcome = process(cand, contexts, scratch_root, m2c, as_bin, args)
            results.append(outcome)
            if not args.json:
                detail = f'  {outcome.detail}' if outcome.detail else ''
                print(f'[{index}/{len(selected)}] {outcome.status} {outcome.name} '
                      f'{outcome.where}{detail}')

    counts = Counter(outcome.status for outcome in results)
    if args.json:
        print(json.dumps({
            'summary': dict(sorted(counts.items())),
            'results': [asdict(outcome) for outcome in results],
        }, indent=2))
    else:
        summary = ', '.join(f'{key}={value}' for key, value in sorted(counts.items()))
        print(f'Summary: {summary or "no candidates selected"}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
