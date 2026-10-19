#!/usr/bin/env python3
"""List decompiled (matched) auto-named MG64 functions as a JSON array.

Only C bodies in src/ named func_<ROM-address> are included, grouped by source
file. Files with more matches come first; names are sorted within each file.
With --by-reference-use the names are reordered by how many unmatched functions
list each as a similar reference in their top-N (most used first).

Example (from the repo root):
  python3 tools/list_matched_functions.py
  python3 tools/list_matched_functions.py --by-reference-use --top-n 3
"""

import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Optional, Sequence

sys.path.insert(0, str(Path(__file__).resolve().parent))

from find_similar_functions import (  # noqa: E402
    build_function_index,
    collect_unmatched,
    find_similar_functions,
    get_project_root,
    scan_c_file,
)


def matched_functions_by_file(root: Path) -> Dict[str, List[str]]:
    groups: Dict[str, List[str]] = defaultdict(list)
    seen = set()
    for c_path in sorted((root / 'src').rglob('*.c')):
        rel = c_path.relative_to(root).as_posix()
        for fn in scan_c_file(c_path):
            if fn.kind != 'def' or not fn.name.startswith('func_') or fn.name in seen:
                continue
            seen.add(fn.name)
            groups[rel].append(fn.name)
    for names in groups.values():
        names.sort()
    return groups


def reference_rows(root: Path, top_n: int) -> List[List[str]]:
    """[unmatched function, its top-N similar decompiled functions...] for each unmatched function."""
    index = build_function_index(root)
    rows: List[List[str]] = []
    for name, fn in collect_unmatched(root, root / 'asm' / 'nonmatchings').items():
        ranked = find_similar_functions(fn, index, top_n=top_n)
        rows.append([name] + [ref for ref, _ in ranked])
    return rows


def order_by_reference_use(names: Sequence[str], rows: Sequence[Sequence[str]]) -> List[str]:
    """Order names by how many rows reference them (most first; ties keep input order).

    Each row is [target, ref...]. The target is not a reference, and a ref counts once per row.
    """
    uses: Dict[str, int] = {}
    for row in rows:
        for ref in set(row[1:]):
            uses[ref] = uses.get(ref, 0) + 1
    return sorted(names, key=lambda name: -uses.get(name, 0))


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--by-reference-use', action='store_true',
                        help='Order by how many unmatched functions use each as a similar reference')
    parser.add_argument('--top-n', type=int, default=3, metavar='N',
                        help='Similar references per unmatched function (default: 3)')
    args = parser.parse_args(argv)

    root = get_project_root()
    groups = matched_functions_by_file(root)
    ordered = sorted(groups.items(), key=lambda item: (-len(item[1]), item[0]))
    names: List[str] = []
    for _, funcs in ordered:
        names.extend(funcs)
    if args.by_reference_use:
        names = order_by_reference_use(names, reference_rows(root, args.top_n))
    print(json.dumps(names))
    return 0


if __name__ == '__main__':
    sys.exit(main())
