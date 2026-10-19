#!/usr/bin/env python3
"""Report Mario Golf 64 decompilation progress from the source tree.

Progress is weighted by function bytes:
  - matched:   C bodies in src/ (sized from their full disassembly, asm/<dir>/<file>.s)
  - unmatched: asm/nonmatchings stubs
               (sized from their `nonmatching <name>, <size>` headers)

Data segments are not counted. The ROM is not compared, so the report reflects
the source tree rather than any particular build.

Example (from the repo root):
  python3 tools/project_status.py
  python3 tools/project_status.py --verbose
"""

import argparse
import sys
from pathlib import Path
from typing import Optional, Sequence

sys.path.insert(0, str(Path(__file__).resolve().parent))

from find_similar_functions import build_function_index, get_project_root, load_unmatched  # noqa: E402


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description='Report MG64 decompilation progress.')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='List unmatched functions, largest first')
    args = parser.parse_args(argv)

    root = get_project_root()
    print("Scanning matched functions...", file=sys.stderr)
    matched = build_function_index(root)
    print("Scanning unmatched functions...", file=sys.stderr)
    unmatched = {name: fn for name, fn in load_unmatched(root).items() if name not in matched}

    matched_bytes = sum(fn.size_bytes for fn in matched.values())
    unmatched_bytes = sum(fn.size_bytes for fn in unmatched.values())
    total = matched_bytes + unmatched_bytes
    pct = matched_bytes / total * 100 if total else 0.0

    print(f"\n{'=' * 60}")
    print(f"Project Status: {pct:.2f}% matched (by function bytes)")
    print(f"{'=' * 60}")
    print(f"Matching bytes: {matched_bytes:,} / {total:,}")
    print(f"Non-matching bytes: {unmatched_bytes:,}")
    print(f"Matched functions: {len(matched)}")
    print(f"Unmatched functions: {len(unmatched)}")

    if not unmatched:
        print("\nAll functions are matched.")
    elif args.verbose:
        print(f"\nUnmatched functions (largest first):")
        print(f"{'=' * 60}")
        for fn in sorted(unmatched.values(), key=lambda f: (-f.size_bytes, f.name)):
            print(f"  0x{fn.size_bytes:05X}  {fn.name}  ({fn.file_path})")
    print(f"{'=' * 60}\n")
    return 0


if __name__ == '__main__':
    sys.exit(main())
