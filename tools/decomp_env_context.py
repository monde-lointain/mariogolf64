#!/usr/bin/env python3
"""Context block for a decomp-env workspace, printed to stdout.

Sections, in order (each omitted when empty):
  <prior_attempts>  this function's difficulty-ledger records, newest first (max 5)
  <callers>         C callers in src/ and include/, then undecompiled asm callers
  <declarations>    prototypes and externs of the function
  <references>      C definitions of the --refs names; bodies over 80 lines are path-only
  <baseline>        score of `./build.sh base.c` run in the workspace, or its first error lines

Run from the repository root:
  tools/decomp_env_context.py --env nonmatchings/<name> --function <name> [--refs LIST]

--refs takes a JSON array of names, or comma/space separated names. decomp-env renders
AGENTS.md with this output in place of the `$context` line. Output stays within
BUDGET_BYTES: reference bodies are dropped largest-first to fit.
Stdlib only; reuses tools/difficulty_ledger.py and tools/find_similar_functions.py.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import signal
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping, Optional, Sequence

import difficulty_ledger as dl
from find_similar_functions import clean_c_source, scan_c_functions, unmatched_asm_files

BUDGET_BYTES = 10_000
CALLER_LINES_MAX = 12
UNDECOMPILED_MAX = 8
DECLARATIONS_MAX = 5
PRIOR_ATTEMPTS_MAX = 5
REFERENCE_BODY_MAX_LINES = 80
BASELINE_ERROR_LINES = 15
BASELINE_TIMEOUT_S = 120
LINE_TEXT_MAX = 200
# decomp.me export and scratch copy: not project source.
SKIPPED_BASENAMES = frozenset({'ctx.c', 'temp.c'})

_IDENT_RE = re.compile(r'[A-Za-z_]\w*')
_WORD_RE = re.compile(r'[A-Za-z_]\w*')
_REF_SPLIT_RE = re.compile(r'[\s,\[\]"\']+')
_JSON_PIECE_SPLIT_RE = re.compile(r'[\s,]+')
_NON_CALLER_LINE_RE = re.compile(r'\b(?:INCLUDE_ASM|INCLUDE_RODATA|GLOBAL_ASM)\s*\(')
# Words that can precede a call in a statement, so `return f(x);` is not a declaration.
_STATEMENT_WORDS = frozenset({'return', 'else', 'case', 'goto', 'do', 'break', 'continue', 'sizeof'})


@dataclass(frozen=True)
class Source:
    rel: str
    raw: list[str]
    clean: list[str]  # comments and preprocessor lines blanked; same line numbers as raw
    defs: dict[str, tuple[int, int]]  # function name -> (first line, last line), C bodies only


@dataclass(frozen=True)
class Reference:
    name: str
    path: str
    start: int
    end: int
    body: Optional[str]  # None when the definition is longer than REFERENCE_BODY_MAX_LINES


def _clip(text: str, limit: int = LINE_TEXT_MAX) -> str:
    return text if len(text) <= limit else text[: limit - 3] + '...'


def parse_refs(text: Optional[str]) -> list[str]:
    """Identifier names from --refs: a JSON array of strings, or comma/space separated names."""
    if text is None or not text.strip():
        return []
    stripped = text.strip()
    pieces: Optional[list[str]] = None
    if stripped.startswith('['):
        try:
            value = json.loads(stripped)
        except ValueError:
            value = None
        if isinstance(value, list) and all(isinstance(item, str) for item in value):
            pieces = [piece for item in value for piece in _JSON_PIECE_SPLIT_RE.split(item)]
    if pieces is None:
        pieces = _REF_SPLIT_RE.split(stripped)
    names: list[str] = []
    for piece in pieces:
        if _IDENT_RE.fullmatch(piece) and piece not in names:
            names.append(piece)
    return names


def _searchable(path: Path, root: Path) -> bool:
    return 'nonmatchings' not in path.relative_to(root).parts and path.name not in SKIPPED_BASENAMES


def _load(path: Path, root: Path, scan_defs: bool) -> Source:
    text = path.read_text(errors='replace')
    raw = text.split('\n')
    clean_text = clean_c_source(text)
    clean = clean_text.split('\n')
    if len(clean) != len(raw):  # comment blanking keeps newlines; guard the line mapping anyway
        clean = raw
    defs: dict[str, tuple[int, int]] = {}
    if scan_defs:
        for fn in scan_c_functions(clean_text):
            if fn.kind == 'def':
                defs.setdefault(fn.name, (fn.start_line, fn.end_line))
    return Source(path.relative_to(root).as_posix(), raw, clean, defs)


def load_sources(root: Path) -> tuple[list[Source], list[Source]]:
    """(C files under src/, headers under include/), without nonmatchings/ and scratch files."""
    c_paths = [p for p in sorted((root / 'src').rglob('*.c')) if _searchable(p, root)]
    header_paths = [p for p in sorted((root / 'include').rglob('*.h')) if _searchable(p, root)]
    return ([_load(p, root, True) for p in c_paths], [_load(p, root, False) for p in header_paths])


def scan_mentions(
    sources: Sequence[Source], function: str, own_span: Optional[tuple[str, int, int]]
) -> tuple[list[tuple[str, int, str]], list[tuple[str, int, str]]]:
    """(callers, declarations) as (path, line, text) tuples, sorted by path then line."""
    mention = re.compile(rf'\b{re.escape(function)}\b')
    decl_re = re.compile(rf'(?P<type>[A-Za-z_][\w \t\*]*?)\s*\b{re.escape(function)}\s*\([^;{{}}]*\)\s*;$')
    callers: list[tuple[str, int, str]] = []
    decls: list[tuple[str, int, str]] = []
    for src in sources:
        for index, clean in enumerate(src.clean):
            if not mention.search(clean):
                continue
            lineno = index + 1
            text = _clip(src.raw[index].strip())
            decl = decl_re.match(clean.strip())
            if decl is not None:
                words = _WORD_RE.findall(decl.group('type'))
                if words and words[-1] not in _STATEMENT_WORDS:
                    decls.append((src.rel, lineno, text))
                    continue
            if _NON_CALLER_LINE_RE.search(clean):
                continue
            if own_span is not None and src.rel == own_span[0] and own_span[1] <= lineno <= own_span[2]:
                continue
            callers.append((src.rel, lineno, text))
    callers.sort(key=lambda hit: (hit[0], hit[1]))
    decls.sort(key=lambda hit: (hit[0], hit[1]))
    return callers, decls


def undecompiled_callers(root: Path, function: str, c_defined: set[str]) -> list[str]:
    """Names of asm/nonmatchings stubs that `jal` the function and have no C body yet."""
    call_re = re.compile(rf'\bjal\s+{re.escape(function)}\b')
    names: set[str] = set()
    for path in unmatched_asm_files(root):
        caller = path.stem
        if caller == function or caller in c_defined:
            continue
        if call_re.search(path.read_text(errors='replace')):
            names.add(caller)
    return sorted(names)


def _shorten(line: str, env: Path, root: Path) -> str:
    """Drop the absolute and relative workspace and repo prefixes from build output."""
    for prefix in (f'{env}/', f'{os.path.relpath(env, root)}/', f'{root}/'):
        line = line.replace(prefix, '')
    return line


def baseline_lines(env: Path, root: Path, timeout: float) -> Optional[list[str]]:
    """Score line of `./build.sh base.c` (plus any NEXT: line), else its first error lines.

    base.c is not a base_<N>.c attempt, so build.sh does not log it to match_log.txt.
    """
    if not (env / 'build.sh').is_file():
        return None
    # A new session, so a timeout also kills the compiler children that hold stdout open.
    try:
        proc = subprocess.Popen(
            ['./build.sh', 'base.c'],
            cwd=env,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
    except OSError as exc:
        return [f'could not run build.sh: {exc}']
    try:
        stdout, _ = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        os.killpg(proc.pid, signal.SIGKILL)
        proc.communicate()
        return [f'build.sh base.c timed out after {timeout:g} s']

    env_abs = env.resolve()
    root_abs = root.resolve()
    output = stdout.decode('utf-8', errors='replace').splitlines()
    score = next((line for line in output if line.startswith('Score: ')), None)
    if proc.returncode == 0 and score is not None:
        selected = [score] + [line for line in output if line.startswith('NEXT:')][:1]
    else:
        # Errors decide what to fix first; warnings only fill the remaining lines.
        errors = [line for line in output if 'warning:' not in line]
        warnings = [line for line in output if 'warning:' in line]
        selected = (errors + warnings)[:BASELINE_ERROR_LINES] or [f'build.sh exited {proc.returncode} with no output']
    return [_clip(_shorten(line, env_abs, root_abs)) for line in selected]


def prior_attempt_lines(root: Path, function: str, environ: Optional[Mapping[str, str]] = None) -> list[str]:
    """Ledger records for the function, newest first, as `tier <n> <outcome> best <pct>%: <reason>`."""
    try:
        records = dl.read_records(dl.ledger_path(environ=environ, cwd=root))
    except (dl.LedgerError, OSError) as exc:
        print(f'warning: prior attempts unavailable: {exc}', file=sys.stderr)
        return []
    mine = [(index, rec) for index, rec in enumerate(records) if rec['function'] == function]
    # Ledger order is append order, so a timestamp tie goes to the later record.
    mine.sort(key=lambda item: (item[1]['ts'], item[0]), reverse=True)
    return [
        _clip(f"tier {rec['tier']} {rec['outcome']} best {rec['best_pct']:.3f}%: {rec['reason']}")
        for _, rec in mine[:PRIOR_ATTEMPTS_MAX]
    ]


def _reference_lines(ref: Reference, with_body: bool) -> list[str]:
    head = f'<reference name="{ref.name}" path="{ref.path}" lines="{ref.start}-{ref.end}"'
    if with_body and ref.body is not None:
        return [head + '>', ref.body, '</reference>']
    return [head + '/>']


def _render(
    prior: list[str],
    callers: list[str],
    decls: list[str],
    references: list[Reference],
    bodies_on: list[bool],
    baseline: Optional[list[str]],
) -> str:
    out: list[str] = []
    if prior:
        out += ['<prior_attempts>', *prior, '</prior_attempts>']
    if callers:
        out += ['<callers>', *callers, '</callers>']
    if decls:
        out += ['<declarations>', *decls, '</declarations>']
    if references:
        out.append('<references>')
        for ref, with_body in zip(references, bodies_on):
            out += _reference_lines(ref, with_body)
        out.append('</references>')
    if baseline:
        out += ['<baseline>', *baseline, '</baseline>']
    return ''.join(line + '\n' for line in out)


def _fit(
    prior: list[str],
    callers: list[str],
    decls: list[str],
    references: list[Reference],
    baseline: Optional[list[str]],
) -> str:
    """Render within BUDGET_BYTES: drop reference bodies largest-first, then trailing references."""
    bodies_on = [ref.body is not None for ref in references]
    while True:
        text = _render(prior, callers, decls, references, bodies_on, baseline)
        if len(text.encode('utf-8')) <= BUDGET_BYTES:
            return text
        shown = [i for i, on in enumerate(bodies_on) if on]
        if shown:
            victim = max(shown, key=lambda i: len(references[i].body or ''))
            bodies_on[victim] = False
            continue
        if references:
            references = references[:-1]
            bodies_on = bodies_on[:-1]
            continue
        # Caps on the other sections keep this unreachable; cut at a line boundary regardless.
        cut = text.encode('utf-8')[:BUDGET_BYTES].decode('utf-8', errors='ignore')
        return cut[: cut.rfind('\n') + 1]


def build_context(
    root: Path,
    env: Path,
    function: str,
    refs: Sequence[str],
    *,
    environ: Optional[Mapping[str, str]] = None,
    build_timeout: float = BASELINE_TIMEOUT_S,
) -> str:
    src_sources, header_sources = load_sources(root)
    index: dict[str, tuple[Source, int, int]] = {}
    for src in src_sources:
        for name, (start, end) in src.defs.items():
            index.setdefault(name, (src, start, end))

    own = index.get(function)
    own_span = (own[0].rel, own[1], own[2]) if own is not None else None
    callers, decls = scan_mentions(src_sources + header_sources, function, own_span)
    caller_lines = [f'{rel}:{lineno}: {text}' for rel, lineno, text in callers[:CALLER_LINES_MAX]]
    if len(callers) > CALLER_LINES_MAX:
        caller_lines.append(f'({len(callers) - CALLER_LINES_MAX} more)')
    undecompiled = undecompiled_callers(root, function, set(index))
    if undecompiled:
        shown = ', '.join(undecompiled[:UNDECOMPILED_MAX])
        extra = f' ({len(undecompiled) - UNDECOMPILED_MAX} more)' if len(undecompiled) > UNDECOMPILED_MAX else ''
        caller_lines.append(f'Undecompiled callers (asm): {shown}{extra}')

    decl_lines = [f'{rel}:{lineno}: {text}' for rel, lineno, text in decls[:DECLARATIONS_MAX]]
    if len(decls) > DECLARATIONS_MAX:
        decl_lines.append(f'({len(decls) - DECLARATIONS_MAX} more)')

    references: list[Reference] = []
    for name in refs:
        hit = index.get(name)
        if name == function or hit is None:
            continue
        src, start, end = hit
        body = '\n'.join(src.raw[start - 1 : end]) if end - start + 1 <= REFERENCE_BODY_MAX_LINES else None
        references.append(Reference(name, src.rel, start, end, body))

    prior = prior_attempt_lines(root, function, environ)
    baseline = baseline_lines(env, root, build_timeout)
    return _fit(prior, caller_lines, decl_lines, references, baseline)


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description='Print the $context block for a decomp-env workspace.')
    parser.add_argument('--env', required=True, help='workspace directory (relative to the repo root or absolute)')
    parser.add_argument('--function', required=True, help='function name')
    parser.add_argument('--refs', default='', help='JSON array or comma/space separated function names')
    args = parser.parse_args(argv)

    root = Path.cwd()
    env = Path(args.env)
    if not env.is_absolute():
        env = root / env
    if not env.is_dir():
        print(f'error: no workspace directory {args.env}', file=sys.stderr)
        return 1
    text = build_context(root, env, args.function, parse_refs(args.refs))
    sys.stdout.buffer.write(text.encode('utf-8'))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
