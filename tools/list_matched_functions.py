#!/usr/bin/env python3
"""List decompiled (matched) auto-named MG64 functions as a JSON array.

Only C bodies in src/ named func_<ROM-address> are included, grouped by source
file. Files with more matches come first; names are sorted within each file.

Example (from the repo root):
  python3 tools/list_matched_functions.py
"""

import json
import sys
from collections import defaultdict
from pathlib import Path
from typing import Dict, List

sys.path.insert(0, str(Path(__file__).resolve().parent))

from find_similar_functions import get_project_root, scan_c_file  # noqa: E402


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


def main() -> int:
    groups = matched_functions_by_file(get_project_root())
    ordered = sorted(groups.items(), key=lambda item: (-len(item[1]), item[0]))
    names: List[str] = []
    for _, funcs in ordered:
        names.extend(funcs)
    print(json.dumps(names))
    return 0


if __name__ == '__main__':
    sys.exit(main())
