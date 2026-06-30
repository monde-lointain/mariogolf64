#!/usr/bin/env python3
"""audit_libultra_headers.py - diff MG64's vendored libultra header MACROS against
the ultralib pin (~/development/repos/ultralib), to catch INVERTED / wrong-value
#defines that the byte-exact oracle won't flag when the macro is unused or compiled
out. Motivated by the S149 os_host.h inversion. See
`docs/hazards.md#vendored-header-inversion-a-curated-libultra-header-diverges-from-the-ultralib-pin`.

Scope + caveat: this compares `#define` right-hand-sides only (the highest-bug-risk
surface, where the os_host.h inversion hid). It does NOT evaluate `#if BUILD_VERSION`
branches, so a macro with version variants will FALSE-POSITIVE against whichever
branch is textually last -- hand-check each flagged macro against the upstream
`>= VERSION_J` branch (MG64's pin) before acting. Most real diffs are cosmetic
(whitespace, `(1<<0)` vs `1`, defensive parens) or version-conditional; the S149 run
found exactly two real issues (os_host.h inversion, rcp.h VI_CTRL_PIXEL_ADV_MASK).

Usage: venv/bin/python3 tools/audit_libultra_headers.py [--ultralib <path>]
"""
from __future__ import annotations

import argparse
import glob
import os
import re

MG = "include/libultra"


def up_path(ultralib_inc: str, mg_rel: str) -> str:
    # MG64 renames the SDK's PRinternal/ tree to internal/.
    if mg_rel.startswith("internal/"):
        return os.path.join(ultralib_inc, "PRinternal", mg_rel[len("internal/"):])
    return os.path.join(ultralib_inc, mg_rel)


def strip_comments(s: str) -> str:
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    s = re.sub(r"//.*", "", s)
    return s


def extract_defines(path: str) -> dict | None:
    """name(+'()' if func-like) -> (arglist, normalized RHS). Last def wins (a
    version-conditional macro keeps its textually-last branch -- see caveat)."""
    if not os.path.exists(path):
        return None
    txt = open(path, errors="replace").read().replace("\\\n", " ")
    out = {}
    for line in txt.splitlines():
        m = re.match(r"\s*#\s*define\s+([A-Za-z_]\w*)(\([^)]*\))?\s*(.*)", line)
        if not m:
            continue
        rhs = re.sub(r"\s+", " ", strip_comments(m.group(3) or "")).strip()
        key = m.group(1) + ("()" if m.group(2) else "")
        out[key] = ((m.group(2) or "").strip(), rhs)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="Diff vendored libultra header macros vs the ultralib pin.")
    ap.add_argument("--ultralib", default=os.path.expanduser("~/development/repos/ultralib"))
    args = ap.parse_args()
    up_inc = os.path.join(args.ultralib, "include")

    diffs, missing = [], []
    for h in sorted(glob.glob(f"{MG}/**/*.h", recursive=True)):
        rel = os.path.relpath(h, MG)
        mg_d = extract_defines(h)
        up_d = extract_defines(up_path(up_inc, rel))
        if up_d is None:
            missing.append(rel)
            continue
        for key, (_, mg_rhs) in mg_d.items():
            if key in up_d and mg_rhs != up_d[key][1]:
                diffs.append((rel, key, mg_rhs, up_d[key][1]))

    print(f"=== MACRO RHS DIFFERENCES ({len(diffs)}) ===")
    print("(triage: ignore #if BUILD_VERSION false-positives + whitespace/paren cosmetics)\n")
    for rel, key, mg_rhs, up_rhs in diffs:
        print(f"[{rel}] {key}\n  MG64: {mg_rhs!r}\n  ULTL: {up_rhs!r}\n")
    print(f"=== headers with NO upstream counterpart ({len(missing)}, MG64-local) ===")
    print(", ".join(missing))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
