#!/usr/bin/env python3
"""Rank unmatched Mario Golf 64 functions for decompilation.

Modes:
  (default)                 easiest first by difficulty score
  --by-size                 fewest instructions first
  --by-coddog-similarity    each function with coddog matches whose C body is in src/

Tiers (--tier 1|2|3, default 1) select by difficulty-ledger tier: tier 1 is every
function with no failure history; tiers 2 and 3 are functions that failed at the lower
tiers. See docs/tools/difficulty-ledger.md.

Examples (from the repo root):
  python3 tools/score_functions.py asm/nonmatchings --json
  python3 tools/score_functions.py asm/nonmatchings --json --by-coddog-similarity
"""

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Dict, List, Mapping, Optional, Sequence, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parent))

from find_similar_functions import (  # noqa: E402
    CoddogError,
    ParsedFunction,
    collect_unmatched,
    get_coddog_similarity,
    get_project_root,
    scan_src_definitions,
)
from difficulty_ledger import (  # noqa: E402
    LedgerError,
    failed_tiers,
    ledger_path,
    read_records,
    tier_of,
)

# Logistic model over standardized features [instructions, branches, jumps, labels].
# This is a ranking heuristic, not a calibrated probability.
_FEATURE_MEANS = (34.27065527065527, 1.6666666666666667, 3.1880341880341883, 1.98005698005698)
_FEATURE_STDS = (24.763225638334454, 2.047860394102145, 3.200600790997309, 2.3803926026229827)
_FEATURE_COEFFS = (2.499706543629367, -0.46648920346754463, -1.61606926820365, 0.4911494991317799)
_INTERCEPT = -0.5155412977000488


def decompilation_difficulty_score(instructions: int, branches: int, jumps: int, labels: int) -> float:
    """Difficulty in (0, 1). Higher means harder to decompile."""
    logit = _INTERCEPT
    for value, mean, std, coeff in zip((instructions, branches, jumps, labels),
                                       _FEATURE_MEANS, _FEATURE_STDS, _FEATURE_COEFFS):
        logit += coeff * (value - mean) / std
    if logit >= 0:
        return 1.0 / (1.0 + math.exp(-logit))
    expo = math.exp(logit)
    return expo / (1.0 + expo)


def difficulty(fn: ParsedFunction) -> float:
    return decompilation_difficulty_score(fn.instruction_count, fn.branch_count,
                                          fn.jump_count, fn.label_count)


def _ordered(scored: List[Tuple[ParsedFunction, float]], by_size: bool) -> List[Tuple[ParsedFunction, float]]:
    if by_size:
        return sorted(scored, key=lambda item: (item[0].instruction_count, item[0].name))
    return sorted(scored, key=lambda item: (item[1], item[0].name))


def select_tier(
    scored: List[Tuple[ParsedFunction, float]],
    tiers: Mapping[str, object],
    tier: int,
) -> List[Tuple[ParsedFunction, float]]:
    """Keep the functions at difficulty-ledger tier `tier`."""
    return [item for item in scored if tier_of(item[0].name, tiers) == tier]


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description='Rank unmatched Mario Golf 64 functions by difficulty, size, or similarity.')
    parser.add_argument('path', nargs='?', default='asm/nonmatchings',
                        help='Unmatched .s file or directory (default: asm/nonmatchings)')
    parser.add_argument('--json', action='store_true', help='Print JSON to stdout')
    parser.add_argument('--min-score', type=float, metavar='MIN', help='Minimum difficulty score (0-1)')
    parser.add_argument('--max-score', type=float, metavar='MAX', help='Maximum difficulty score (0-1)')
    parser.add_argument('--tier', type=int, choices=(1, 2, 3), default=1,
                        help='Only functions at this difficulty-ledger tier (default: 1)')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--by-size', action='store_true',
                      help='Order by instruction count, smallest first')
    mode.add_argument('--by-coddog-similarity', action='store_true',
                      help='List each function with coddog matches whose C body is in src/')
    parser.add_argument('--top-n', type=int, default=5, metavar='N',
                        help='Coddog matches per entry (default: 5)')
    args = parser.parse_args(argv)

    root = get_project_root()
    target = Path(args.path)
    if not target.is_absolute():
        target = root / target
    if not target.exists():
        print(f"Error: path not found: {args.path}", file=sys.stderr)
        return 1

    functions: Dict[str, ParsedFunction] = collect_unmatched(root, target)
    scored = [(fn, difficulty(fn)) for fn in functions.values()]
    if args.min_score is not None:
        scored = [item for item in scored if item[1] >= args.min_score]
    if args.max_score is not None:
        scored = [item for item in scored if item[1] <= args.max_score]

    try:
        tiers = failed_tiers(root, read_records(ledger_path(cwd=root)))
    except LedgerError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1
    scored = select_tier(scored, tiers, args.tier)

    if args.by_coddog_similarity:
        src_defs = scan_src_definitions(root)
        ordered = _ordered(scored, args.by_size)
        rows = []
        for i, (fn, _) in enumerate(ordered, 1):
            print(f"[{i}/{len(ordered)}] {fn.name}", file=sys.stderr)
            try:
                matches = get_coddog_similarity(fn.name, root, src_defs)
            except CoddogError as exc:
                print(f"Error: {exc}", file=sys.stderr)
                return 1
            if not matches:
                continue
            matches = matches[:args.top_n]
            rows.append((matches[0][1], fn.name, matches))
        rows.sort(key=lambda row: (-row[0], row[1]))
        if args.json:
            print(json.dumps([[name] + [ref for ref, _ in matches] for _, name, matches in rows]))
        else:
            for _, name, matches in rows:
                print(name)
                for ref, pct in matches:
                    print(f"    {pct:6.2f}%  {ref}  ({src_defs[ref]})")
        return 0

    ordered = _ordered(scored, args.by_size)
    if args.json:
        print(json.dumps([fn.name for fn, _ in ordered]))
    else:
        for fn, score in ordered:
            print(f"{score:.4f}  {fn.name}  {fn.instruction_count} instr  {fn.file_path}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
