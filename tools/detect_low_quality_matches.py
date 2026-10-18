#!/usr/bin/env python3
"""Find decompiled functions whose C contains low-quality match patterns.

Patterns flagged (after comments and preprocessor lines are removed):
  - ((T*)expr)[index]         pointer-cast indexing instead of typed arrays/fields
  - (T*)base +/- offset       raw pointer arithmetic
  - *(T*)((u8*)...)           byte-offset dereference
  - ->unkXX                   unnamed struct fields
  - goto                      control flow that should be structured
  - temp_*, var_*, phi_*      m2c temporaries that were never renamed

Badness score = violations per non-blank line of the function (higher is worse).
Only functions with at least one violation are reported.

Examples (from the repo root):
  python3 tools/detect_low_quality_matches.py src
  python3 tools/detect_low_quality_matches.py src --exhaustive --verbose
  python3 tools/detect_low_quality_matches.py src --json
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, NamedTuple, Optional, Sequence, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))

from find_similar_functions import clean_c_source, scan_c_functions  # noqa: E402

# Library-style code that is not part of the game's decompiled matches.
SKIP_DIRS = frozenset({'libultra', 'libkmc', 'libnusys', 'libmus', 'libnaudio'})

VIOLATION_PATTERNS = (
    re.compile(r'\(\s*\(\s*\w+\s*\*\s*\)\s*[^)]+\)\s*\[\s*[^]]+\]'),
    re.compile(r'\(\s*\w+\s*\*\s*\)\s*[^+\-;,)]+\s*[+\-]\s*(?:0x)?[0-9A-Fa-f]+'),
    re.compile(r'\*\s*\(\s*\w+\s*\*\s*\)\s*\(\s*\(\s*u8\s*\*\s*\)'),
    re.compile(r'->\s*unk[0-9A-Fa-f]+'),
    re.compile(r'\bgoto\b'),
    re.compile(r'\b(?:temp|var|phi)_\w+'),
)


class FunctionStats(NamedTuple):
    violations: int
    lines: int

    @property
    def score(self) -> float:
        return self.violations / max(self.lines, 1)


def analyze_source(text: str) -> Dict[str, FunctionStats]:
    """Violating functions in one C source text, keyed by function name."""
    clean = clean_c_source(text)
    stats: Dict[str, FunctionStats] = {}
    for fn in scan_c_functions(clean):
        if fn.kind != 'def':
            continue
        span = clean[fn.start_offset:fn.end_offset]
        violations = sum(len(pattern.findall(span)) for pattern in VIOLATION_PATTERNS)
        if not violations:
            continue
        lines = sum(1 for line in span.split('\n') if line.strip())
        stats.setdefault(fn.name, FunctionStats(violations, lines))
    return stats


def is_git_ignored(filepath: Path) -> bool:
    try:
        result = subprocess.run(['git', 'check-ignore', '-q', str(filepath)],
                                capture_output=True, timeout=1)
    except (OSError, subprocess.TimeoutExpired):
        return False
    return result.returncode == 0


def iter_c_files(path: Path) -> List[Path]:
    if path.is_file():
        return [path] if path.suffix == '.c' else []
    files = []
    for c_path in sorted(path.rglob('*.c')):
        if any(part in SKIP_DIRS for part in c_path.relative_to(path).parts):
            continue
        if is_git_ignored(c_path):
            continue
        files.append(c_path)
    return files


def analyze_path(path: Path) -> Dict[Tuple[str, str], FunctionStats]:
    results: Dict[Tuple[str, str], FunctionStats] = {}
    for c_path in iter_c_files(path):
        text = c_path.read_text(errors='replace')
        for name, stats in analyze_source(text).items():
            results[(str(c_path), name)] = stats
    return results


def rank_functions(stats: Dict[Tuple[str, str], FunctionStats]) -> List[Tuple[str, str, FunctionStats]]:
    ranked = [(file, name, st) for (file, name), st in stats.items()]
    return sorted(ranked, key=lambda item: (-item[2].score, item[0], item[1]))


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description='Find decompiled MG64 functions with low-quality match patterns.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
By default, prints only the worst function name.
Use --exhaustive to list all violating functions ranked by badness.

Examples:
  %(prog)s src                       # worst function
  %(prog)s src --exhaustive          # rank all violating functions
  %(prog)s src/main/func_8002A640.c --exhaustive
        """,
    )
    parser.add_argument('path', help='C file or directory to analyze')
    parser.add_argument('--exhaustive', action='store_true',
                        help='Show all violating functions ranked by badness')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Show file paths and detailed stats')
    parser.add_argument('--json', action='store_true',
                        help='Output [[file, function], ...] pairs as a JSON array')
    parser.add_argument('--limit', type=int, default=None,
                        help='Limit the number of results (for --json or --exhaustive)')
    args = parser.parse_args(argv)

    path = Path(args.path)
    if not path.exists():
        print(f"Error: {args.path} does not exist", file=sys.stderr)
        return 1

    stats = analyze_path(path)
    if not stats:
        if args.json:
            print(json.dumps([]))
        else:
            print("No violations found", file=sys.stderr)
        return 0

    ranked = rank_functions(stats)

    if args.json:
        results = [[file, name] for file, name, _ in ranked]
        if args.limit is not None:
            results = results[:args.limit]
        print(json.dumps(results))
        return 0

    if args.exhaustive:
        display = ranked[:args.limit] if args.limit is not None else ranked
        if args.verbose:
            print("Functions ranked by badness (violations per line):")
            print("=" * 90)
            for i, (file, name, st) in enumerate(display, 1):
                print(f"{i:3d}. {name:40s} {st.score:6.3f}  ({st.violations}/{st.lines})  {file}")
        else:
            for _, name, _ in display:
                print(name)
    else:
        worst_file, worst_name, worst_stats = ranked[0]
        if args.verbose:
            print(f"{worst_name} (score: {worst_stats.score:.3f}, "
                  f"{worst_stats.violations} violations in {worst_stats.lines} lines, {worst_file})")
        else:
            print(worst_name)
    return 0


if __name__ == '__main__':
    sys.exit(main())
