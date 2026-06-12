#!/usr/bin/env python3
"""pick_target.py — rank the next decomp target, smallest-first.

Pure-static ranker (no Ghidra MCP — the agent seeds from MCP inline during the
execution loop). Enumerates flippable `asm` subsegs and partially-matched `c`
files from mariogolf64.yaml + asm/ + src/, flags upstream-mirror availability
(libultra/libkmc), band warmth (a mirror dir with an already-banked sibling →
an enabler-free flip), and hazards (file-scope static, file-scope data-global
definition, refs-unplaced = a data extern referenced but absent from both name
files, non-16-aligned subseg, multi-function pack, needs-header = an upstream
include unresolved under the project -I set), and prints a smallest-first table
for `/sprint-plan`.

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
# The two disjoint symbol-name files. A symbol absent from BOTH gets no linker address; for a
# *data* extern that means an undefined reference (see refs_unplaced). Functions are exempt —
# splat auto-resolves them via undefined_funcs_auto.txt.
NAME_FILES = [os.path.join(ROOT, "symbol_addrs.txt"), os.path.join(ROOT, "ghidra_symbols.txt")]

# Subseg line: `- [0x8DF10, c, libultra/monegi/rdp/dp]` or `- [0x8D230, asm]`.
# (bss entries use the `{ type: bss, ... }` brace form and are intentionally skipped.)
SUBSEG_RE = re.compile(
    r"^\s*-\s*\[\s*0x([0-9A-Fa-f]+)\s*,\s*([a-z]+)\s*(?:,\s*([^\]]+?))?\s*\]"
)
GLABEL_RE = re.compile(r"^\s*glabel\s+(\S+)")
# Approximate C function-definition: a return-type-led line ending in `name(`.
UPSTREAM_DEF_RE = re.compile(r"^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w+)\s*\(", re.M)
FILE_STATIC_RE = re.compile(r"^\s*static\b[^=]*;\s*$")
# A file-scope data-global *definition* (external linkage): `<type> <name> [= init];`.
# Applied only at brace-depth 0 (see defines_data_globals) so function-body statements
# and struct members never match. Captures the variable name.
DATA_GLOBAL_DEF_RE = re.compile(
    r"^(?:(?:struct|union|enum)\s+)?[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w+)"
    r"\s*(?:\[[^\]]*\]\s*)?(?:=[^;]*)?;\s*$"
)
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
# The project's quoted/angle include search dirs (Makefile CFLAGS `-I` set, under -nostdinc).
INCLUDE_DIRS = [
    os.path.join(ROOT, d)
    for d in ("include", "include/libultra", "include/libultra/internal", "include/libkmc")
]
PROJECT_INC = os.path.join(ROOT, "include")
# Upstream include trees a missing companion header can be *copied from* (the execution-middle
# mirror, e.g. assert.h). A header absent here and absent from the project tree is a system
# header we don't ship (blocked); a header present in the project tree but unreachable under
# INCLUDE_DIRS is an unindexed-`-I` case (blocked, a deferred Makefile enabler — the audio
# band's <libaudio.h> at include/libultra/PR/). See classify of needs-header in seed_points.
UPSTREAM_INC_ROOTS = [
    os.path.expanduser("~/development/repos/libultra_modern/include"),
    os.path.expanduser("~/development/repos/libkmc/include"),
]
UPSTREAM_BONUS = 200
CARRYOVER_PENALTY = 1000
# A warm band (the candidate's mirror dir already holds a banked sibling) means its
# companion headers + callee symbols are in-tree → an enabler-free flip. Worth ~one
# leaf-function of size: it lifts a warm candidate over a band-cold one of similar
# size (the tiebreak intent) without overriding large size gaps. Validated twice —
# the si pair (Sprint 4) and vi pair (Sprint 5) both banked at one-file cost off warm bands.
BAND_WARM_BONUS = 64


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


def defines_data_globals(cpath):
    """File-scope data-global *definitions* with external linkage (not static/extern) in the
    upstream .c. A verbatim mirror would emit these into .data/.bss and collide with the bytes
    splat already extracts + symbolizes at their original vram — the .data analogue of the
    file-static BSS hazard. Returns the defined names; a non-empty result is the `defines-data`
    DoR hazard → route to the classical loop with the data defs dropped (the fn references the
    splat-placed globals via extern). Heuristic (brace-depth + line shape), confirmed at the gate
    like needs-header: `__osThreadTail` et al. in thread.c (osDequeueThread) is the motivating case."""
    names = []
    depth = 0
    in_kr_params = False  # between a K&R `name(args)` header and its `{` body
    with open(cpath, errors="ignore") as f:
        for raw in f:
            line = raw.strip()
            if (depth == 0 and not in_kr_params and line and not line.startswith(
                    ("#", "//", "*", "}", "static", "extern", "typedef"))):
                m = DATA_GLOBAL_DEF_RE.match(line)
                if m and "(" not in line.split("=", 1)[0]:
                    names.append(m.group(1))
            # A depth-0 function header (K&R or ANSI) opens a param region where `type name;`
            # lines are parameters, not globals — skip until the body brace. K&R math in libkmc
            # (`_xatan(u,v,atanp)` then `XLONG u,v;`) would otherwise false-flag every param.
            if depth == 0 and "(" in line and not line.endswith(";") and "{" not in line:
                in_kr_params = True
            if "{" in raw:
                in_kr_params = False
            depth += raw.count("{") - raw.count("}")
            if depth < 0:
                depth = 0
    return names


_PLACED_SYMBOLS = None


def placed_symbols():
    """Lazily-built set of every symbol name in the two name files (symbol_addrs + ghidra). A
    name in this set has a linker address; one absent from it does not (a data extern then
    link-fails). Format: `name = 0x...; // ...`."""
    global _PLACED_SYMBOLS
    if _PLACED_SYMBOLS is None:
        names = set()
        for nf in NAME_FILES:
            if not os.path.exists(nf):
                continue
            with open(nf, errors="ignore") as f:
                for line in f:
                    m = re.match(r"\s*([A-Za-z_]\w+)\s*=", line)
                    if m:
                        names.add(m.group(1))
        _PLACED_SYMBOLS = names
    return _PLACED_SYMBOLS


# An SDK-internal global is `__`-prefixed (e.g. __osRunQueue). Uppercase macros ([A-Z]-led) and
# CamelCase types never match, so this rarely false-flags a non-symbol token.
SDK_GLOBAL_RE = re.compile(r"__[A-Za-z]\w+")


def refs_unplaced(cpath, placed):
    """Data externs the upstream .c *references* but that are absent from BOTH name files — the
    dual of `defines-data` (which catches definitions). The motivating case: osYieldThread reads
    `&__osRunQueue`, a global whose address the asm bakes as a raw lui/addiu immediate (no symbol)
    but which a verbatim C mirror turns into an undefined extern → link failure. A non-empty
    result is the `refs-unplaced` DoR hazard → the execution middle must recover each name's vram
    via asm-data-recovery (disassemble the target fn, read the lui/addiu HI/LO16 pair, add the
    extern to symbol_addrs.txt) BEFORE the flip links. Heuristic, gate-confirmed: a `__`-prefixed
    token, never called anywhere in the file (so functions — which splat auto-resolves via
    undefined_funcs_auto — are excluded), and absent from the name set. A function *pointer*
    passed by bare name could over-flag; the gate confirms against the upstream."""
    try:
        text = open(cpath, errors="ignore").read()
    except OSError:
        return []
    tokens = set(SDK_GLOBAL_RE.findall(text))
    unplaced = []
    for tok in tokens:
        if tok in placed:
            continue
        # Called anywhere (`tok(` allowing whitespace) → a function, not a data extern → skip.
        if re.search(re.escape(tok) + r"\s*\(", text):
            continue
        unplaced.append(tok)
    return sorted(unplaced)


def band_mirror_dir(lib, up_path):
    """Project mirror directory for an upstream file: src/lib<...>/<upstream-rel-dir>."""
    tree = LIBULTRA if lib == "libultra" else LIBKMC
    rel = os.path.relpath(up_path, tree)
    return os.path.join(ROOT, "src", lib, os.path.dirname(rel))


def band_is_warm(mirror_dir):
    """True if the candidate's mirror dir already holds a banked (0-stub) sibling .c — its
    companion headers + callee symbols are already in-tree, so the flip needs no enablers."""
    if not os.path.isdir(mirror_dir):
        return False
    for cpath in glob.glob(os.path.join(mirror_dir, "*.c")):
        try:
            with open(cpath, errors="ignore") as f:
                if "INCLUDE_ASM" not in f.read():
                    return True
        except OSError:
            continue
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


_PROJECT_INC_INDEX = None


def _project_inc_index():
    """Lazily-built set of every header under the project `include/` tree (both its root-relative
    path and its basename) — used to tell an unreachable-but-present header (unindexed `-I`,
    defer) from a genuinely-absent one (copyable from upstream, or a system header)."""
    global _PROJECT_INC_INDEX
    if _PROJECT_INC_INDEX is None:
        idx = set()
        for f in glob.glob(os.path.join(PROJECT_INC, "**", "*"), recursive=True):
            if os.path.isfile(f):
                idx.add(os.path.relpath(f, PROJECT_INC))
                idx.add(os.path.basename(f))
        _PROJECT_INC_INDEX = idx
    return _PROJECT_INC_INDEX


def include_is_blocked(inc):
    """Classify a *missing* include (one `missing_includes` already found unreachable). Blocked =
    can't be fixed by a companion-header copy: either it's present in the project `include/` tree
    but unreachable under the current `-I` set (unindexed `-I` — a deferred Makefile enabler), or
    it's absent everywhere (a system header we don't ship). Copyable (NOT blocked) = absent from
    the project tree but present at `<upstream-include-root>/<inc>` → mirror it in mid-execution
    (assert.h). Heuristic, gate-confirmed (basename match can rarely over-flag)."""
    idx = _project_inc_index()
    if inc in idx or os.path.basename(inc) in idx:
        return True  # present in-tree but unreachable → unindexed -I, defer
    for root in UPSTREAM_INC_ROOTS:
        if os.path.exists(os.path.join(root, inc)):
            return False  # absent in-tree, copyable from upstream → companion-copy
    return True  # nowhere → system header / unavailable


def snap_fib(n):
    """Round a raw additive score up to the nearest Fibonacci ladder rung (1,2,3,5,8,13)."""
    for f in (1, 2, 3, 5, 8, 13):
        if n <= f:
            return f
    return 13


def seed_points(size, upstream, band, nfns, hazards, blocked):
    """Deterministic a-priori story-point seed (Fibonacci 1,2,3,5,8,13) for one increment —
    the v1 story-point system (see VELOCITY.md). MG64's effort axis is PATH (upstream-mirror vs
    classical-loop) + ENABLER LOAD (band warmth, hazards), not raw bytes; the byte gates (768 /
    1536) are dormant in the current <256 B regime and only bite for large/classical fns.
    `hazards` is the joined hazard string; `blocked` is True when any needs-header is un-pickable
    (see include_is_blocked). Returns an int, or 'blk' for a blocked (un-pickable) target.
    Display-only — does NOT influence the smallest-first sort. A committed cluster seed is the
    SUM of its files' seeds (banked per-file, not all-or-nothing across the cluster)."""
    if blocked:
        return "blk"  # un-pickable until the -I path is added / header shipped — a DoR reject
    classical = upstream == "none"
    pack = nfns > 1
    drop = ("file-static" in hazards) or ("defines-data" in hazards)  # → classical fallback
    needs_copy = "needs-header" in hazards  # a copyable companion header (blocked ones already 'blk')
    recover = "refs-unplaced" in hazards  # asm-data-recovery enabler before the mirror links
    big = size >= 768
    huge = size >= 1536
    if huge or (classical and pack):
        return 13  # must decompose; never a 1-increment sprint
    if nfns >= 4:
        return 8  # a large pack must decompose regardless of path (hits the 8-gate)
    if classical and (big or pack or drop or needs_copy or recover):
        return 8  # enabler gate — decompose / scaffold first
    if classical:
        return 5  # small single classical fn — unproven regime, high variance
    base = 1 if band == "warm" else 2  # warm = enabler-free; cold = symbol recovery / cold deps
    if drop:
        base += 1  # file-static / defines-data drop → classical fallback
    if needs_copy:
        base += 1  # companion-header copy
    if recover:
        base += 1  # asm-data-recovery of an unplaced data extern (S2 __osThreadTail pattern)
    if pack or big:
        base += 1
    return snap_fib(base)


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
        band = "-"
        blocked = False
        if up_path:
            if has_file_scope_static(up_path):
                hazards.append("file-static")  # route to the classical loop, not the mirror
            data_defs = defines_data_globals(up_path)
            if data_defs:
                hazards.append("defines-data:" + ",".join(data_defs))  # .data analogue → classical loop
            unplaced = refs_unplaced(up_path, placed_symbols())
            if unplaced:
                hazards.append("refs-unplaced:" + ",".join(unplaced))  # asm-data-recovery before flip
            missing = missing_includes(up_path)
            if missing:
                hazards.append("needs-header:" + ",".join(missing))
                blocked = any(include_is_blocked(inc) for inc in missing)
            band = "warm" if band_is_warm(band_mirror_dir(up_lib, up_path)) else "cold"

        if args.lib and args.lib not in (path or "") and not any(args.lib in n for n in fns):
            continue
        if primary in carried and not args.include_stuck:
            continue

        score = -size + (UPSTREAM_BONUS if up_lib else 0)
        if band == "warm":
            score += BAND_WARM_BONUS  # prefer enabler-free warm-band siblings over equally-small cold ones
        if primary in carried:
            score -= CARRYOVER_PENALTY
        hz = ",".join(hazards) or "-"
        rows.append({
            "func": primary, "rom": f"0x{off:X}", "size": size, "nfns": len(fns),
            "pts": seed_points(size, up_lib or "none", band, len(fns), hz, blocked),
            "kind": kind, "upstream": up_lib or "none", "band": band,
            "hazards": hz, "score": score,
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
    print(f"{'func':28} {'rom':>9} {'size':>5} {'nfn':>3} {'pts':>3} {'kind':8} "
          f"{'upstream':9} {'band':4} hazards")
    for r in rows:
        print(f"{r['func']:28} {r['rom']:>9} {r['size']:>5} {r['nfns']:>3} {str(r['pts']):>3} "
              f"{r['kind']:8} {r['upstream']:9} {r['band']:4} {r['hazards']}")


if __name__ == "__main__":
    main()
