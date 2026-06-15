#!/usr/bin/env python3
"""Parse `coddog compare2` output (stdin) into tools/coddog/coddog_map.tsv (S71).

Each compare2 line is "MGNAME - ULNAME (NN.NN%)"; join ULNAME -> source file via the sym2file map
and emit `mgname<TAB>ulname<TAB>ulfile<TAB>pct`. Invoked from tools/coddog_sweep.sh; see
docs/hazards.md#coddog-cross-ref. Usage: coddog ... | parse_map.py <sym2file.tsv> <out.tsv>"""
import re
import sys

sym2file = {}
for ln in open(sys.argv[1]):
    p = ln.rstrip("\n").split("\t")
    if len(p) == 2:
        sym2file.setdefault(p[0], p[1])

n = 0
with open(sys.argv[2], "w") as out:
    for ln in sys.stdin:
        m = re.match(r"^(\S+) - (\S+) \((\d+\.\d+)%\)$", ln.strip())
        if not m:
            continue
        mg, ul, pct = m.group(1), m.group(2), m.group(3)
        out.write(f"{mg}\t{ul}\t{sym2file.get(ul, '??')}\t{pct}\n")
        n += 1
print(f"coddog_sweep: wrote {n} matches to {sys.argv[2]}", file=sys.stderr)
