#!/usr/bin/env python3
"""Find decompiled (matched) MG64 functions that resemble an unmatched function.

Library module shared by score_functions.py, project_status.py and
detect_low_quality_matches.py, plus a CLI for a single function:

    python3 tools/find_similar_functions.py <function>
    python3 tools/find_similar_functions.py <function> --top 5 --threshold 0.4
    python3 tools/find_similar_functions.py <function> --coddog-similarity
    python3 tools/find_similar_functions.py <function> --json

References are C functions defined in src/ (their full disassembly is
asm/<dir>/<file>.s). Candidates are unmatched asm/nonmatchings/**/*.s stubs.
"""

import argparse
import bisect
import functools
import json
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

try:
    import Levenshtein as _Levenshtein
except ImportError:  # Pure-Python fallback below gives the same ratio.
    _Levenshtein = None


SIMILARITY_WEIGHTS = {'instruction': 0.35, 'control': 0.25, 'data': 0.20, 'structural': 0.20}
SHORT_FUNCTION_LIMIT = 50
NEAR_OFFSET_DELTA = 8

BRANCH_MNEMONICS = frozenset({
    'b', 'bal', 'beq', 'bne', 'beql', 'bnel', 'beqz', 'bnez', 'beqzl', 'bnezl',
    'blez', 'bgtz', 'bltz', 'bgez', 'blezl', 'bgtzl', 'bltzl', 'bgezl',
    'bltzal', 'bgezal', 'bltzall', 'bgezall',
    'bc1f', 'bc1t', 'bc1fl', 'bc1tl',
})

# Registers kept verbatim when normalizing; every other register collapses to its class.
KEPT_REGISTERS = frozenset({'$zero', '$at', '$sp', '$fp', '$ra', '$gp', '$k0', '$k1'})

_INSTR_RE = re.compile(r'^\s*/\*\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]{8}\s+[0-9A-Fa-f]{8}\s+\*/\s+(\S.*?)\s*$')
_LABEL_DEF_RE = re.compile(r'^\s*\.L\w+:')
_GLABEL_RE = re.compile(r'^glabel\s+(\S+)')
_ENDLABEL_RE = re.compile(r'^endlabel\s+(\S+)')
_NONMATCHING_RE = re.compile(r'^nonmatching\s+(\S+?),\s*(0x[0-9A-Fa-f]+|\d+)')
_OPERAND_TOKEN_RE = re.compile(r'%(?:hi|lo)\([^)]*\)|\.L\w+|\$\w+|0x[0-9A-Fa-f]+|[A-Za-z_][\w.]*')
_STACK_RE = re.compile(r'^\$sp,\s*\$sp,\s*(-?(?:0x[0-9A-Fa-f]+|\d+))')
_DATA_OFFSET_RE = re.compile(r'(-?(?:0x[0-9A-Fa-f]+|\d+))\(\$(\w+)\)')

_COMMENT_OR_STRING_RE = re.compile(
    r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)
_C_TOKEN_RE = re.compile(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'|[{}();]')
_INCLUDE_ASM_RE = re.compile(r'INCLUDE_ASM\s*\(\s*"[^"]*"\s*,\s*([A-Za-z_]\w*)\s*\)\s*$')

_CODDOG_LINE_RE = re.compile(r'^(\d+(?:\.\d+)?)%\s*-\s*(\S.*?)\s*$')
_DECOMPILED_SUFFIX_RE = re.compile(r'\s*\(decompiled\)\s*$')


class CoddogError(RuntimeError):
    """coddog is missing or failed."""


# ---------------------------------------------------------------------------
# Paths and file discovery
# ---------------------------------------------------------------------------

def get_project_root() -> Path:
    return Path(__file__).resolve().parent.parent


def unmatched_asm_files(root: Path) -> List[Path]:
    """Assembly stubs that hold unmatched functions (asm/nonmatchings/**/*.s).

    Full disassembly files (asm/<dir>/<file>.s) are matched code and are never included.
    """
    files: List[Path] = []
    nonmatchings = root / 'asm' / 'nonmatchings'
    if nonmatchings.is_dir():
        files.extend(sorted(nonmatchings.rglob('*.s')))
    return files


def collect_unmatched(root: Path, target: Path) -> Dict[str, 'ParsedFunction']:
    """Parse unmatched functions under target (a directory or one .s file)."""
    target = target.resolve()
    if target.is_file():
        files = [target]
    else:
        files = [
            f for f in unmatched_asm_files(root)
            if target == f.resolve() or target in f.resolve().parents
        ]
    result: Dict[str, ParsedFunction] = {}
    for path in files:
        for fn in parse_asm_file(path, root):
            if fn.name in result:
                print(f"warning: duplicate unmatched function {fn.name} in {fn.file_path}", file=sys.stderr)
                continue
            result[fn.name] = fn
    return result


def load_unmatched(root: Path) -> Dict[str, 'ParsedFunction']:
    return collect_unmatched(root, root / 'asm')


# ---------------------------------------------------------------------------
# Assembly parsing
# ---------------------------------------------------------------------------

@dataclass
class ParsedFunction:
    name: str
    file_path: str
    instructions: List[str]
    branch_count: int
    jump_count: int
    label_count: int
    stack_size: int
    data_offsets: List[int]
    control_flow: str
    size_bytes: int

    @property
    def instruction_count(self) -> int:
        return len(self.instructions)

    @functools.cached_property
    def trigrams(self) -> frozenset:
        seq = self.instructions
        if len(seq) < 3:
            return frozenset([tuple(seq)])
        return frozenset(zip(seq, seq[1:], seq[2:]))

    @functools.cached_property
    def codes(self) -> str:
        return ''.join(_token_char(tok) for tok in self.instructions)

    @functools.cached_property
    def offset_set(self) -> frozenset:
        return frozenset(self.data_offsets)


def _parse_int(text: str) -> int:
    text = text.strip()
    negative = text.startswith('-')
    body = text.lstrip('-')
    value = int(body, 16) if body[:2].lower() == '0x' else int(body)
    return -value if negative else value


def _norm_reg(tok: str) -> str:
    if tok in KEPT_REGISTERS:
        return tok
    if len(tok) > 1 and tok[1] in 'tavsfkr':
        return '$' + tok[1]
    return '$r'


def _norm_token(match: re.Match) -> str:
    tok = match.group(0)
    if tok[0] == '%':
        return tok[:3] + '(SYM)'
    if tok[0] == '.':
        return 'LBL'
    if tok[0] == '$':
        return _norm_reg(tok)
    if tok.startswith('0x'):
        return 'ADDR' if len(tok) >= 6 else tok
    return 'SYM'


def normalize_instruction(mnem: str, ops: str) -> str:
    """Canonical form: registers collapsed by class, symbols/labels/addresses abstracted."""
    if mnem == 'jal':
        return 'jal FUNC'
    return f"{mnem} {_OPERAND_TOKEN_RE.sub(_norm_token, ops)}".rstrip()


def _control_code(mnem: str) -> Optional[str]:
    if mnem in BRANCH_MNEMONICS:
        return 'B'
    if mnem == 'jal':
        return 'C'
    if mnem == 'j':
        return 'J'
    if mnem == 'jr':
        return 'R'
    return None


def parse_function_content(name: str, file_path: str, instr_texts: Sequence[str],
                           label_count: int = 0, size_bytes: Optional[int] = None) -> ParsedFunction:
    instructions: List[str] = []
    control: List[str] = []
    branches = jumps = stack = 0
    offsets: List[int] = []
    for text in instr_texts:
        parts = text.split(None, 1)
        if not parts:
            continue
        mnem = parts[0]
        ops = parts[1] if len(parts) > 1 else ''
        instructions.append(normalize_instruction(mnem, ops))
        code = _control_code(mnem)
        if code is not None:
            control.append(code)
            if code == 'B':
                branches += 1
            elif code in 'CJ':
                jumps += 1
        if mnem == 'addiu' and stack == 0:
            m = _STACK_RE.match(ops)
            if m:
                stack = abs(_parse_int(m.group(1)))
        for m in _DATA_OFFSET_RE.finditer(ops):
            if m.group(2) not in ('sp', 'fp'):
                offsets.append(_parse_int(m.group(1)))
    if size_bytes is None:
        size_bytes = 4 * len(instructions)
    return ParsedFunction(
        name=name,
        file_path=file_path,
        instructions=instructions,
        branch_count=branches,
        jump_count=jumps,
        label_count=label_count,
        stack_size=stack,
        data_offsets=offsets,
        control_flow=''.join(control),
        size_bytes=size_bytes,
    )


def parse_asm_file(path: Path, root: Path) -> List[ParsedFunction]:
    """Parse every glabel/endlabel function in an assembly file.

    Skips data labels (D_*, jtbl_*) and functions with no instructions.
    """
    rel = path.resolve().relative_to(root.resolve()).as_posix()
    functions: List[ParsedFunction] = []
    name: Optional[str] = None
    instr: List[str] = []
    labels = 0
    header: Optional[Tuple[str, int]] = None
    size: Optional[int] = None

    def finish() -> None:
        if name is None or not instr or name.startswith(('D_', 'jtbl_')):
            return
        functions.append(parse_function_content(name, rel, instr, labels, size))

    for raw in path.read_text(errors='replace').splitlines():
        line = raw.rstrip()
        m = _NONMATCHING_RE.match(line)
        if m:
            header = (m.group(1), _parse_int(m.group(2)))
            continue
        m = _GLABEL_RE.match(line)
        if m:
            finish()
            name = m.group(1)
            instr, labels = [], 0
            size = header[1] if header and header[0] == name else None
            header = None
            continue
        if _ENDLABEL_RE.match(line):
            finish()
            name = None
            continue
        if name is None:
            continue
        if _LABEL_DEF_RE.match(line):
            labels += 1
            continue
        m = _INSTR_RE.match(line)
        if m:
            instr.append(m.group(1))
    finish()
    return functions


# ---------------------------------------------------------------------------
# C source scanning (matched functions)
# ---------------------------------------------------------------------------

@dataclass
class CFunction:
    name: str
    kind: str  # 'def' for a C body, 'stub' for INCLUDE_ASM
    start_line: int
    end_line: int
    start_offset: int
    end_offset: int


def _blank_preprocessor(text: str) -> str:
    lines = text.split('\n')
    continued = False
    for idx, line in enumerate(lines):
        if continued or line.lstrip().startswith('#'):
            continued = line.rstrip().endswith('\\')
            lines[idx] = ''
        else:
            continued = False
    return '\n'.join(lines)


def clean_c_source(text: str) -> str:
    """Blank comments and preprocessor lines. Keeps newlines and string literals."""
    def repl(match: re.Match) -> str:
        s = match.group(0)
        return re.sub(r'[^\n]', ' ', s) if s[0] == '/' else s
    return _blank_preprocessor(_COMMENT_OR_STRING_RE.sub(repl, text))


def _function_from_header(header: str) -> Optional[str]:
    """Name of a function defined by a header like `u8* func(args)`, else None."""
    h = header.strip()
    if '=' in h or not h.endswith(')'):
        return None
    depth = 0
    for j in range(len(h) - 1, -1, -1):
        if h[j] == ')':
            depth += 1
        elif h[j] == '(':
            depth -= 1
            if depth == 0:
                m = re.search(r'([A-Za-z_]\w*)\s*$', h[:j])
                return m.group(1) if m else None
    return None


def scan_c_functions(clean: str) -> List[CFunction]:
    """Top-level function definitions and INCLUDE_ASM stubs in cleaned C text."""
    line_starts = [0] + [m.end() for m in re.finditer(r'\n', clean)]

    def line_of(offset: int) -> int:
        return bisect.bisect_right(line_starts, offset)

    funcs: List[CFunction] = []
    depth = 0
    paren = 0
    stmt_start = 0
    pending: Optional[Tuple[str, int]] = None
    for m in _C_TOKEN_RE.finditer(clean):
        tok = m.group(0)
        pos = m.start()
        if tok[0] in '"\'':
            continue
        if tok == '(':
            paren += 1
        elif tok == ')':
            paren -= 1
        elif tok == '{':
            if depth == 0 and paren == 0:
                header = clean[stmt_start:pos]
                name = _function_from_header(header)
                pending = (name, stmt_start + len(header) - len(header.lstrip())) if name else None
            depth += 1
        elif tok == '}':
            depth -= 1
            if depth == 0:
                if pending is not None:
                    name, off = pending
                    funcs.append(CFunction(name, 'def', line_of(off), line_of(pos), off, pos + 1))
                pending = None
                stmt_start = pos + 1
                paren = 0
        elif tok == ';' and depth == 0:
            stmt = clean[stmt_start:pos]
            m2 = _INCLUDE_ASM_RE.search(stmt)
            if m2:
                off = stmt_start + len(stmt) - len(stmt.lstrip())
                funcs.append(CFunction(m2.group(1), 'stub', line_of(off), line_of(pos), off, pos + 1))
            stmt_start = pos + 1
            paren = 0
    return funcs


def scan_c_file(path: Path) -> List[CFunction]:
    return scan_c_functions(clean_c_source(path.read_text(errors='replace')))


def scan_src_definitions(root: Path) -> Dict[str, str]:
    """Map of function name -> repo-relative C file for every C body in src/."""
    defs: Dict[str, str] = {}
    for c_path in sorted((root / 'src').rglob('*.c')):
        rel = c_path.relative_to(root).as_posix()
        for fn in scan_c_file(c_path):
            if fn.kind == 'def':
                defs.setdefault(fn.name, rel)
    return defs


def build_function_index(root: Path) -> Dict[str, ParsedFunction]:
    """Matched reference functions: C bodies in src/ with their disassembly from asm/."""
    index: Dict[str, ParsedFunction] = {}
    for c_path in sorted((root / 'src').rglob('*.c')):
        rel_src = c_path.relative_to(root / 'src')
        defs = [fn for fn in scan_c_file(c_path) if fn.kind == 'def']
        if not defs:
            continue
        asm_path = root / 'asm' / rel_src.with_suffix('.s')
        if not asm_path.is_file():
            print(f"warning: no disassembly for {c_path.relative_to(root).as_posix()}", file=sys.stderr)
            continue
        by_name = {fn.name: fn for fn in parse_asm_file(asm_path, root)}
        for d in defs:
            fn = by_name.get(d.name)
            if fn is None or d.name in index:
                continue
            fn.file_path = c_path.relative_to(root).as_posix()
            index[d.name] = fn
    return index


# ---------------------------------------------------------------------------
# Similarity
# ---------------------------------------------------------------------------

_TOKEN_IDS: Dict[str, str] = {}


def _token_char(tok: str) -> str:
    ch = _TOKEN_IDS.get(tok)
    if ch is None:
        ch = chr(0x100 + len(_TOKEN_IDS))
        _TOKEN_IDS[tok] = ch
    return ch


def _lcs_len(a: str, b: str) -> int:
    if len(a) < len(b):
        a, b = b, a
    prev = [0] * (len(b) + 1)
    for ca in a:
        cur = [0]
        for j, cb in enumerate(b, 1):
            cur.append(prev[j - 1] + 1 if ca == cb else max(prev[j], cur[j - 1]))
        prev = cur
    return prev[-1]


def _ratio(a: str, b: str) -> float:
    """Levenshtein ratio (indel-based, same as Levenshtein.ratio)."""
    total = len(a) + len(b)
    if total == 0:
        return 1.0
    if _Levenshtein is not None:
        return _Levenshtein.ratio(a, b)
    return 2.0 * _lcs_len(a, b) / total


def _jaccard(a: frozenset, b: frozenset) -> float:
    if not a and not b:
        return 1.0
    return len(a & b) / len(a | b)


def _min_max(x: int, y: int) -> float:
    high = max(x, y)
    if high == 0:
        return 1.0
    return min(x, y) / high


def instruction_similarity(a: ParsedFunction, b: ParsedFunction) -> float:
    score = _jaccard(a.trigrams, b.trigrams)
    if a.instruction_count < SHORT_FUNCTION_LIMIT and b.instruction_count < SHORT_FUNCTION_LIMIT:
        score = (score + _ratio(a.codes, b.codes)) / 2
    return score


def control_flow_similarity(a: ParsedFunction, b: ParsedFunction) -> float:
    return 0.7 * _ratio(a.control_flow, b.control_flow) + 0.3 * _min_max(a.branch_count, b.branch_count)


def data_access_similarity(a: ParsedFunction, b: ParsedFunction) -> float:
    set_a, set_b = a.offset_set, b.offset_set
    if not set_a and not set_b:
        return 0.5
    if not set_a or not set_b:
        return 0.0
    exact = len(set_a & set_b) / len(set_a | set_b)
    near_a = sum(1 for x in set_a if any(abs(x - y) <= NEAR_OFFSET_DELTA for y in set_b))
    near_b = sum(1 for y in set_b if any(abs(y - x) <= NEAR_OFFSET_DELTA for x in set_a))
    near = (near_a + near_b) / (len(set_a) + len(set_b))
    return 0.6 * exact + 0.4 * near


def structural_similarity(a: ParsedFunction, b: ParsedFunction) -> float:
    return (0.3 * _min_max(a.instruction_count, b.instruction_count)
            + 0.25 * _min_max(a.branch_count, b.branch_count)
            + 0.25 * _min_max(a.jump_count, b.jump_count)
            + 0.20 * _min_max(a.stack_size, b.stack_size))


def similarity_breakdown(a: ParsedFunction, b: ParsedFunction) -> Dict[str, float]:
    return {
        'instruction': instruction_similarity(a, b),
        'control': control_flow_similarity(a, b),
        'data': data_access_similarity(a, b),
        'structural': structural_similarity(a, b),
    }


def calculate_similarity(a: ParsedFunction, b: ParsedFunction) -> float:
    parts = similarity_breakdown(a, b)
    return sum(SIMILARITY_WEIGHTS[key] * value for key, value in parts.items())


def find_similar_functions(target: ParsedFunction, candidates: Dict[str, ParsedFunction],
                           top_n: int = 5, threshold: float = 0.0) -> List[Tuple[str, float]]:
    scored = []
    for name, cand in candidates.items():
        if name == target.name:
            continue
        score = calculate_similarity(target, cand)
        if score >= threshold:
            scored.append((name, score))
    scored.sort(key=lambda item: (-item[1], item[0]))
    return scored[:top_n]


# ---------------------------------------------------------------------------
# coddog
# ---------------------------------------------------------------------------

def get_coddog_similarity(name: str, root: Path, src_defs: Dict[str, str],
                          threshold: float = 0.5) -> List[Tuple[str, float]]:
    """coddog matches for name, keeping only references whose C body is in src/.

    coddog labels any function without an asm/nonmatchings stub as "(decompiled)",
    including hand-written assembly functions (hasm), so src_defs is the authority here.
    """
    if shutil.which('coddog') is None:
        raise CoddogError('coddog is not on PATH (install it with `cargo install coddog`)')
    try:
        proc = subprocess.run(
            ['coddog', 'match', name, '-t', f'{threshold}'],
            cwd=root, capture_output=True, text=True, timeout=300,
        )
    except subprocess.TimeoutExpired as exc:
        raise CoddogError(f'coddog match {name} timed out') from exc
    if proc.returncode != 0:
        detail = (proc.stderr or proc.stdout).strip()
        raise CoddogError(f'coddog match {name} failed: {detail}')
    matches: List[Tuple[str, float]] = []
    for line in proc.stdout.splitlines():
        m = _CODDOG_LINE_RE.match(line.strip())
        if not m:
            continue
        ref = _DECOMPILED_SUFFIX_RE.sub('', m.group(2)).strip()
        if ref == name or ref not in src_defs:
            continue
        matches.append((ref, float(m.group(1))))
    return matches


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _describe(fn: ParsedFunction) -> str:
    return (f"{fn.name}  ({fn.file_path})\n"
            f"  instructions={fn.instruction_count} branches={fn.branch_count} "
            f"jumps={fn.jump_count} labels={fn.label_count} stack={fn.stack_size}")


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description='Find decompiled functions similar to an unmatched MG64 function.')
    parser.add_argument('function', help='Unmatched function name (asm/nonmatchings or assembly segment)')
    parser.add_argument('--top', '-n', type=int, default=10, metavar='N',
                        help='Number of similar functions to show (default: 10)')
    parser.add_argument('--threshold', '-t', type=float, default=0.0, metavar='T',
                        help='Minimum similarity score, 0-1 (default: 0)')
    parser.add_argument('--coddog-similarity', action='store_true',
                        help='Show coddog matches (-t 0.5) whose C body is in src/')
    parser.add_argument('--json', action='store_true',
                        help='Print [function, similar_1, ...] as JSON')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Show per-component similarity breakdown')
    args = parser.parse_args(argv)

    root = get_project_root()
    unmatched = load_unmatched(root)
    target = unmatched.get(args.function)
    if target is None:
        print(f"Error: '{args.function}' is not an unmatched function "
              f"(searched asm/nonmatchings)", file=sys.stderr)
        return 1

    breakdowns: Dict[str, Dict[str, float]] = {}
    if args.coddog_similarity:
        src_defs = scan_src_definitions(root)
        try:
            ranked = get_coddog_similarity(target.name, root, src_defs)
        except CoddogError as exc:
            print(f"Error: {exc}", file=sys.stderr)
            return 1
        ranked = ranked[:args.top]
        sources = {name: src_defs[name] for name, _ in ranked}
    else:
        index = build_function_index(root)
        ranked = find_similar_functions(target, index, args.top, args.threshold)
        sources = {name: index[name].file_path for name, _ in ranked}
        breakdowns = {name: similarity_breakdown(target, index[name]) for name, _ in ranked}

    if args.json:
        print(json.dumps([target.name] + [name for name, _ in ranked]))
        return 0

    print(_describe(target))
    if not ranked:
        print('  no similar decompiled functions found')
    for name, score in ranked:
        shown = f"{score:6.2f}" if args.coddog_similarity else f"{score:6.4f}"
        print(f"  {shown} {name}  ({sources[name]})")
        if args.verbose and name in breakdowns:
            parts = breakdowns[name]
            print('           ' + '  '.join(f"{k}={v:.3f}" for k, v in parts.items()))
    return 0


if __name__ == '__main__':
    sys.exit(main())
