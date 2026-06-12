#!/usr/bin/env python3
"""pick_target.py — rank the next decomp target, smallest-first.

Pure-static ranker (no Ghidra MCP — the agent seeds from MCP inline during the
execution loop). Enumerates flippable `asm` subsegs and partially-matched `c`
files from mariogolf64.yaml + asm/ + src/, flags upstream-mirror availability
(libultra/libkmc) and hazards (file-scope static, non-16-aligned subseg,
multi-function pack, needs-header = an upstream include unresolved under the
project -I set), and prints a smallest-first table for `/sprint-plan`.

Usage:
  venv/bin/python3 tools/pick_target.py [--lib SUBSTR] [-n N] [--include-stuck] [--json]

Replaces the old `/decomp-lead` roadmap: target selection is now a tool the plan
gate calls, mirroring marioparty7's pick_target.py.
"""
import argparse
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
YAML = os.path.join(ROOT, "mariogolf64.yaml")
LIBULTRA = os.path.expanduser("~/development/repos/libultra_modern/src")
LIBKMC = os.path.expanduser("~/development/repos/libkmc/src")
BACKLOG = os.path.join(ROOT, "BACKLOG.md")

# Subseg line: `- [0x8DF10, c, libultra/monegi/rdp/dp]` or `- [0x8D230, asm]`.
# (bss entries use the `{ type: bss, ... }` brace form and are intentionally skipped.)
SUBSEG_RE = re.compile(
    r"^\s*-\s*\[\s*0x([0-9A-Fa-f]+)\s*,\s*([a-z]+)\s*(?:,\s*([^\]]+?))?\s*\]"
)
GLABEL_RE = re.compile(r"^\s*glabel\s+(\S+)")
# Approximate C function-definition: a return-type-led line ending in `name(`.
UPSTREAM_DEF_RE = re.compile(r"^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w+)\s*\(", re.M)
FILE_STATIC_RE = re.compile(r"^\s*static\b[^=]*;\s*$")
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
# The project's quoted/angle include search dirs (Makefile CFLAGS `-I` set, under -nostdinc).
INCLUDE_DIRS = [
    os.path.join(ROOT, d)
    for d in ("include", "include/libultra", "include/libultra/internal", "include/libkmc")
]
UPSTREAM_BONUS = 200
CARRYOVER_PENALTY = 1000


def parse_subsegs():
    """Return all `[0x..,type,..]` subsegs as (rom_off:int, type, path|None), sorted by offset."""
    subs = []
    with open(YAML) as f:
        for line in f:
            m = SUBSEG_RE.match(line)
            if not m:
                continue
            off = int(m.group(1), 16)
            path = m.group(3).strip() if m.group(3) else None
            subs.append((off, m.group(2), path))
    subs.sort(key=lambda s: s[0])
    return subs


def asm_functions(rom_off):
    """glabel names defined in asm/<ROM>.s for this subseg (curated if already synced)."""
    path = os.path.join(ROOT, "asm", f"{rom_off:X}.s")
    if not os.path.exists(path):
        return []
    names = []
    with open(path) as f:
        for line in f:
            m = GLABEL_RE.match(line)
            if m:
                names.append(m.group(1))
    return names


def build_upstream_index():
    """Map upstream function name -> (lib, path), scanning libultra/libkmc once."""
    index = {}
    for lib, tree in (("libultra", LIBULTRA), ("libkmc", LIBKMC)):
        if not os.path.isdir(tree):
            continue
        for cpath in glob.glob(os.path.join(tree, "**", "*.c"), recursive=True):
            try:
                with open(cpath, errors="ignore") as f:
                    text = f.read()
            except OSError:
                continue
            for m in UPSTREAM_DEF_RE.finditer(text):
                index.setdefault(m.group(1), (lib, cpath))
    return index


def has_file_scope_static(cpath):
    """True if the upstream .c declares a file-scope static (BSS-layout hazard → classical loop)."""
    with open(cpath, errors="ignore") as f:
        for line in f:
            stripped = line.lstrip()
            if stripped.startswith(("//", "*")):
                continue
            if FILE_STATIC_RE.match(line):
                return True
    return False


def missing_includes(cpath):
    """Upstream includes that don't resolve under the project -I set (-nostdinc, so every
    header must come from INCLUDE_DIRS). A non-empty result is the `needs-header` DoR hazard:
    the verbatim mirror won't compile until each is satisfied — copy the companion header
    (execution-middle, e.g. assert.h) or add a -I path (deferred enabler, e.g. the audio band's
    <libaudio.h> at include/libultra/PR/). Conservative: scans all `#include` lines regardless
    of `#ifdef` state, so a dead `#ifdef _DEBUG` include may over-flag — the gate confirms."""
    missing = []
    with open(cpath, errors="ignore") as f:
        for line in f:
            m = INCLUDE_RE.match(line)
            if not m:
                continue
            inc = m.group(1)
            if not any(os.path.exists(os.path.join(d, inc)) for d in INCLUDE_DIRS):
                if inc not in missing:
                    missing.append(inc)
    return missing


def carry_over_names():
    """Function names parked in BACKLOG.md ## Carry-overs (de-ranked)."""
    names = set()
    if not os.path.exists(BACKLOG):
        return names
    with open(BACKLOG) as f:
        parts = f.read().split("## Carry-overs", 1)
    if len(parts) > 1:
        names.update(re.findall(r"`([A-Za-z_]\w+)`", parts[1]))
    return names


def build_rows(args, upstream_index, carried):
    subs = parse_subsegs()
    rows = []
    for i, (off, typ, path) in enumerate(subs):
        size = subs[i + 1][0] - off if i + 1 < len(subs) else 0
        if typ == "asm":
            fns = asm_functions(off)
            if not fns:
                continue
            kind = "asm-flip"
            hazards = []
            if size and size % 16 != 0:
                hazards.append("non16align")
            if len(fns) > 1:
                hazards.append(f"pack:{len(fns)}fn")
        elif typ == "c" and path:
            cpath = os.path.join(ROOT, "src", path + ".c")
            if not os.path.exists(cpath):
                continue
            text = open(cpath).read()
            fns = re.findall(r"INCLUDE_ASM\([^,]+,\s*([A-Za-z_]\w+)", text)
            if not fns:  # 0 stubs => already an md5-candidate
                continue
            kind = "c-stub"
            hazards = [f"remaining:{len(fns)}"]
        else:
            continue

        primary = fns[0]
        up_lib, up_path = upstream_index.get(primary, (None, None))
        if up_path:
            if has_file_scope_static(up_path):
                hazards.append("file-static")  # route to the classical loop, not the mirror
            missing = missing_includes(up_path)
            if missing:
                hazards.append("needs-header:" + ",".join(missing))

        if args.lib and args.lib not in (path or "") and not any(args.lib in n for n in fns):
            continue
        if primary in carried and not args.include_stuck:
            continue

        score = -size + (UPSTREAM_BONUS if up_lib else 0)
        if primary in carried:
            score -= CARRYOVER_PENALTY
        rows.append({
            "func": primary, "rom": f"0x{off:X}", "size": size, "nfns": len(fns),
            "kind": kind, "upstream": up_lib or "none",
            "hazards": ",".join(hazards) or "-", "score": score,
        })
    rows.sort(key=lambda r: (-r["score"], r["size"]))
    return rows[: args.n]


def main():
    ap = argparse.ArgumentParser(description="Rank the next decomp target, smallest-first.")
    ap.add_argument("--lib", help="substring filter on subseg path or function name")
    ap.add_argument("-n", type=int, default=20, help="max rows (default 20)")
    ap.add_argument("--include-stuck", action="store_true", help="include BACKLOG carry-overs")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    rows = build_rows(args, build_upstream_index(), carry_over_names())
    if args.json:
        print(json.dumps(rows, indent=1))
        return
    if not rows:
        print("(no candidates — every asm subseg flipped and every c file at 0 stubs?)")
        return
    print(f"{'func':28} {'rom':>9} {'size':>5} {'nfn':>3} {'kind':8} {'upstream':9} hazards")
    for r in rows:
        print(f"{r['func']:28} {r['rom']:>9} {r['size']:>5} {r['nfns']:>3} "
              f"{r['kind']:8} {r['upstream']:9} {r['hazards']}")


if __name__ == "__main__":
    main()
