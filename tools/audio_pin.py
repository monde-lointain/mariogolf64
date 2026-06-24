#!/usr/bin/env python3
"""Pin the MG64 audio library version + compiler + flags from the coddog sweep maps.

Aggregates every tools/coddog/audio/<cell>_map.tsv (written by tools/audio_sweep.sh over the
build/audio-ref/<cell>/ matrix from tools/build_audio_refs.sh) and, per library family
(libmus / libnaudio / nuaulstl), ranks the matrix cells by match quality to pick the cell whose
(version, compiler, opt) best reproduces the ROM. For each pinned lib it writes the canonical
tools/coddog/<lib>_map.tsv that tools/pick_target.py consumes, records the pinned source root in
tools/coddog/audio_pins.tsv (lib -> srcdir, read by the pick_target audio resolver), and emits a
human-readable tools/coddog/audio/PIN_REPORT.md. See docs/hazards.md#coddog-cross-ref.

Ranking key per cell: (#exact-100% matches, #>=99% matches, #total matches, mean pct). An exact-100%
count is the strongest version discriminator — a wrong version still shows structural ~99% twins but
few byte-exact hits (the S117 size-discriminator idea, read off coddog's own per-fn pct).

Usage:  venv/bin/python3 tools/audio_pin.py
Env:    none (paths are repo-relative).
"""

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = os.path.join(ROOT, "tools/audio_ref_versions.tsv")
AUDIO_MAP_DIR = os.path.join(ROOT, "tools/coddog/audio")
CODDOG_DIR = os.path.join(ROOT, "tools/coddog")
PINS_TSV = os.path.join(CODDOG_DIR, "audio_pins.tsv")
REPORT = os.path.join(AUDIO_MAP_DIR, "PIN_REPORT.md")

EXACT = 100.0
MIRROR_PCT = (
    99.0  # mirror-grade match floor (matches pick_target's CODDOG_MIRROR_PCT = 0.99)
)


def expanduser(p):
    return os.path.expanduser(p)


def load_manifest():
    """(lib, ver) -> expanded srcdir, from the matrix manifest."""
    src = {}
    if not os.path.isfile(MANIFEST):
        return src
    with open(MANIFEST) as f:
        for line in f:
            if line.lstrip().startswith("#") or not line.strip():
                continue
            parts = line.rstrip("\n").split("\t")
            if len(parts) < 3:
                continue
            lib, ver, srcdir = parts[0], parts[1], parts[2]
            src[(lib, ver)] = expanduser(srcdir)
    return src


COMPILERS = ("kmc", "ido53", "ido71")


def parse_cell(cell):
    """Split a cell-id `<lib>_<ver>_<compiler>_<opt>` -> (lib, ver, compiler, opt). Parsed from the
    RIGHT (opt, then compiler are fixed tokens) so a ver with underscores (n64sdkmod_naudio) is kept
    whole. Returns None if the tail isn't a known compiler/opt."""
    parts = cell.split("_")
    if len(parts) < 4:
        return None
    opt, compiler = parts[-1], parts[-2]
    if compiler not in COMPILERS:
        return None
    lib = parts[0]
    ver = "_".join(parts[1:-2])
    return (lib, ver, compiler, opt)


def load_cells():
    """cell -> (lib, ver, compiler, opt), enumerated from the swept maps present in AUDIO_MAP_DIR.
    Independent of build/audio-ref/SUMMARY.tsv (which a filtered re-build overwrites); the maps are the
    real evidence the sweep produced."""
    cells = {}
    if not os.path.isdir(AUDIO_MAP_DIR):
        return cells
    for fn in os.listdir(AUDIO_MAP_DIR):
        if not fn.endswith("_map.tsv"):
            continue
        cell = fn[: -len("_map.tsv")]
        meta = parse_cell(cell)
        if meta:
            cells[cell] = meta
    return cells


def read_map(path):
    """List of (mg, ref, srcfile, pct) from a coddog map tsv."""
    rows = []
    try:
        with open(path, errors="ignore") as f:
            for line in f:
                p = line.rstrip("\n").split("\t")
                if len(p) != 4:
                    continue
                try:
                    rows.append((p[0], p[1], p[2], float(p[3])))
                except ValueError:
                    continue
    except OSError:
        pass
    return rows


def clean_ge99_rows(rows):
    """The >=99% rows whose REF is matched by exactly one MG function. A ref matched by several MG
    funcs is a tiny-wrapper structural collision (e.g. nuAuStlHeapAlloc fingerprint-matching unrelated
    game leaves at 99.99%), the documented coddog noise class — dropping shared refs leaves the real
    1:1 mirror identities and keeps the canonical map from re-pricing game functions."""
    ge99 = [r for r in rows if r[3] >= MIRROR_PCT]
    refcount = {}
    for _, ref, _, _ in ge99:
        refcount[ref] = refcount.get(ref, 0) + 1
    return [r for r in ge99 if refcount[r[1]] == 1]


def cell_stats(rows):
    exact = sum(1 for _, _, _, pct in rows if pct >= EXACT)
    ge99 = sum(1 for _, _, _, pct in rows if pct >= MIRROR_PCT)
    clean = len(clean_ge99_rows(rows))
    total = len(rows)
    ge = [pct for _, _, _, pct in rows if pct >= MIRROR_PCT]
    mean99 = sum(ge) / len(ge) if ge else 0.0
    return exact, ge99, clean, total, mean99


def main():
    manifest_src = load_manifest()
    cells = load_cells()
    if not cells:
        print(
            f"audio_pin: no maps in {AUDIO_MAP_DIR} (run tools/audio_sweep.sh first)",
            file=sys.stderr,
        )
        return 1

    # Gather per-cell stats from the swept maps, grouped by lib.
    by_lib = {}
    for cell, (lib, ver, compiler, opt) in cells.items():
        mp = os.path.join(AUDIO_MAP_DIR, f"{cell}_map.tsv")
        rows = read_map(mp)
        exact, ge99, clean, total, mean99 = cell_stats(rows)
        by_lib.setdefault(lib, []).append(
            {
                "cell": cell,
                "ver": ver,
                "compiler": compiler,
                "opt": opt,
                "exact": exact,
                "ge99": ge99,
                "clean": clean,
                "total": total,
                "mean99": mean99,
                "rows": rows,
            }
        )

    os.makedirs(AUDIO_MAP_DIR, exist_ok=True)
    pins = []
    report = [
        "# Audio library version pin\n",
        "Generated by `tools/audio_pin.py` from the coddog sweep matrix "
        "(`tools/coddog/audio/*_map.tsv`). Ranking key per cell: "
        "(#exact-100%, #clean>=99%, #>=99%, mean%). `clean` = >=99% matches with a 1:1 ref "
        "(drops tiny-wrapper structural collisions); the canonical map keeps only clean rows.\n",
    ]

    for lib in sorted(by_lib):
        ranked = sorted(
            by_lib[lib],
            key=lambda c: (c["exact"], c["clean"], c["ge99"], c["mean99"]),
            reverse=True,
        )
        report.append(f"\n## {lib}\n")
        report.append(
            "| rank | ver | compiler | opt | exact100 | clean>=99 | >=99 | total | mean% |"
        )
        report.append("| ---: | --- | --- | --- | ---: | ---: | ---: | ---: | ---: |")
        for i, c in enumerate(ranked[:12]):
            report.append(
                f"| {i + 1} | {c['ver']} | {c['compiler']} | {c['opt']} | "
                f"{c['exact']} | {c['clean']} | {c['ge99']} | {c['total']} | {c['mean99']:.2f} |"
            )
        win = ranked[0]
        if win["clean"] == 0:
            report.append(
                f"\n**{lib}: NO clean (1:1, >=99%) match in MG64** "
                f"(best cell {win['cell']}: {win['ge99']} >=99% but all tiny-wrapper "
                f"collisions). This library is likely NOT linked into the ROM.\n"
            )
            continue

        srcdir = manifest_src.get((lib, win["ver"]), "")
        pins.append((lib, win["ver"], win["compiler"], win["opt"], srcdir))
        # canonical map = the winning cell's CLEAN rows only (collision-pruned), the set pick_target
        # may safely re-price; pick_target re-applies its own >= CODDOG_MIRROR_PCT threshold.
        clean_rows = sorted(clean_ge99_rows(win["rows"]), key=lambda r: -r[3])
        dst_map = os.path.join(CODDOG_DIR, f"{lib}_map.tsv")
        with open(dst_map, "w") as d:
            for mg, ref, sf, pct in clean_rows:
                d.write(f"{mg}\t{ref}\t{sf}\t{pct:.2f}\n")
        dropped = win["ge99"] - win["clean"]
        note = f" ({dropped} collision rows pruned)" if dropped else ""
        report.append(
            f"\n**PIN {lib}: ver=`{win['ver']}` compiler=`{win['compiler']}` opt=`-{win['opt']}` "
            f"-> {win['exact']} exact / {win['clean']} clean>=99% / {win['ge99']} >=99% "
            f"(mean {win['mean99']:.2f}%)**{note}. Canonical map ({win['clean']} rows) -> "
            f"`tools/coddog/{lib}_map.tsv`; src `{srcdir}`.\n"
        )
        # a few representative clean matches (exact first)
        sample = sorted(clean_rows, key=lambda r: -r[3])[:8]
        if sample:
            report.append(
                "Sample matches: "
                + ", ".join(
                    f"`{mg}`->`{ref}`({sf},{pct:.0f}%)" for mg, ref, sf, pct in sample
                )
                + ".\n"
            )

    with open(PINS_TSV, "w") as f:
        f.write("# lib\tver\tcompiler\topt\tsrcdir  (written by tools/audio_pin.py)\n")
        for row in pins:
            f.write("\t".join(row) + "\n")

    with open(REPORT, "w") as f:
        f.write("\n".join(report) + "\n")

    print(f"audio_pin: pinned {len(pins)} libs -> {PINS_TSV}", file=sys.stderr)
    for lib, ver, comp, opt, _ in pins:
        print(f"  {lib}: {ver} / {comp} / -{opt}", file=sys.stderr)
    print(f"audio_pin: report -> {REPORT}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
