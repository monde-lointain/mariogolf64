#!/usr/bin/env python3
"""List INCLUDE_ASM functions in MG64 C files as decompilation candidates.

Each `INCLUDE_ASM("<dir>", <name>);` statement in the given files is one
candidate; its assembly is `<dir>/<name>.s`. Commented-out statements are
ignored. Functions with failure history in the difficulty ledger (tier 2 or higher) are
skipped unless --include-failed is given; each row shows its tier.

Output is sorted smallest assembly first (ties by source and line), one row per
candidate, followed by a `# total_include_asm=...` trailer line.

Examples (from the repo root):
  python3 tools/list_decomp_candidates.py src/main/func_8002A640.c
  python3 tools/list_decomp_candidates.py src/overlay_15/func_ovl15_801FB510.c --summary
"""

import argparse
import posixpath
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Mapping, Optional, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))

from find_similar_functions import clean_c_source  # noqa: E402
from difficulty_ledger import (  # noqa: E402
    LedgerError,
    failed_tiers,
    ledger_path,
    read_records,
    tier_of,
)

ROOT = Path(__file__).resolve().parent.parent

INCLUDE_ASM_RE = re.compile(r'INCLUDE_ASM\s*\(\s*"([^"]+)"\s*,\s*([A-Za-z_]\w*)\s*\)')


@dataclass(frozen=True)
class IncludeAsm:
    source: str  # repo-relative C file (posix)
    line: int  # 1-based line of the INCLUDE_ASM statement
    folder: str  # e.g. asm/nonmatchings/main/func_8002A640
    func: str

    @property
    def asm_rel(self) -> str:
        return posixpath.normpath(f'{self.folder}/{self.func}.s')


def find_include_asm_in_text(text: str, source: str) -> List[IncludeAsm]:
    """Return INCLUDE_ASM statements in C source text, ignoring comments/macros."""
    rows: List[IncludeAsm] = []
    cleaned = clean_c_source(text)
    for line_no, line in enumerate(cleaned.split('\n'), 1):
        match = INCLUDE_ASM_RE.search(line)
        if match:
            rows.append(IncludeAsm(source, line_no, match.group(1), match.group(2)))
    return rows


def find_include_asm(source_file: Path, root: Path = ROOT) -> List[IncludeAsm]:
    rel = source_file.resolve().relative_to(root.resolve()).as_posix()
    return find_include_asm_in_text(source_file.read_text(errors='replace'), rel)



def collect_candidates(
    files: List[Path],
    include_failed: bool,
    root: Path,
    tiers: Mapping[str, object],
) -> Tuple[List[dict], int, int]:
    """Return (rows, total_statements, failed_skipped) for the given C files.

    tiers maps function name -> difficulty tier (difficulty_ledger.failed_tiers).
    Functions above tier 1 are skipped unless include_failed is set; rows carry the tier.
    """
    rows: List[dict] = []
    total = 0
    failed_skipped = 0
    for source_file in files:
        for item in find_include_asm(source_file, root):
            total += 1
            tier = tier_of(item.func, tiers)
            if tier != 1 and not include_failed:
                failed_skipped += 1
                continue
            asm_path = root / item.asm_rel
            rows.append({
                'func': item.func,
                'source': item.source,
                'line': item.line,
                'asm': item.asm_rel,
                'asm_size': asm_path.stat().st_size if asm_path.is_file() else -1,
                'tier': tier,
            })
    rows.sort(key=lambda r: (r['asm_size'] < 0, r['asm_size'], r['source'], r['line']))
    return rows, total, failed_skipped


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('files', nargs='+', type=Path, help='C files to scan for INCLUDE_ASM')
    parser.add_argument(
        '--include-failed',
        action='store_true',
        help='include functions with failure history (tier 2 and up), with their tier',
    )
    parser.add_argument(
        '--summary',
        action='store_true',
        help='print only the totals trailer, not the candidate rows',
    )
    args = parser.parse_args(argv)

    for path in args.files:
        if not path.is_file():
            print(f'error: cannot read {path}', file=sys.stderr)
            return 1

    try:
        tiers = failed_tiers(ROOT, read_records(ledger_path(cwd=ROOT)))
    except LedgerError as exc:
        print(f'error: {exc}', file=sys.stderr)
        return 1

    rows, total, failed_skipped = collect_candidates(args.files, args.include_failed, ROOT, tiers)
    eligible = len(rows)
    if not args.summary:
        print('func\tsource:line\tasm_size\ttier\tasm')
        for r in rows:
            size = str(r['asm_size']) if r['asm_size'] >= 0 else 'missing'
            print(f"{r['func']}\t{r['source']}:{r['line']}\t{size}\t{r['tier']}\t{r['asm']}")
    print(f'# total_include_asm={total} eligible={eligible} failed_skipped={failed_skipped}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
