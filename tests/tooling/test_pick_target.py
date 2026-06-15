"""Characterization golden for tools/pick_target.py.

pick_target is pure-static (a deterministic function of the repo's yaml + asm +
symbol files), so its `--json` output is a stable snapshot. This is the crown-
jewel golden: it exercises build_rows, the hazard detectors, signature_hint, and
the scoring/sort in one shot. A diff after refactoring = a behavior regression.

The golden is a *refactor-session* snapshot tied to current repo state; if the
yaml/asm/symbols change for unrelated reasons, regenerate with REGEN_GOLDEN=1.
"""
from __future__ import annotations

import json

import pytest

from conftest import golden_dir, load_tool, regen, run_tool  # noqa: F401

ROWS = "50"


def test_vendorable_tu_missing_defines(tmp_path, monkeypatch):
    """S57 retro #1: the asm-mirror needs-define pre-check (vendorable_tu_missing_defines).

    A vendored ultralib .s must assemble under the in-tree `-I` headers; a macro none of them
    define is a needs-define enabler. Verifies: clean backlog TUs report nothing; an undefined
    UPPER_CASE macro is flagged; comment words and the `#include` path token never false-flag;
    lower-case register names are ignored."""
    pt = load_tool("pick_target")

    # The whole current vendoring backlog is self-contained (all macros ship in-tree).
    for rel in ("os/maptlbrdb.s", "os/probetlb.s", "gu/sqrtf.s", "libc/bcopy.s"):
        assert pt.vendorable_tu_missing_defines(rel) == [], rel
    assert pt.vendorable_tu_missing_defines(None) == []
    assert pt.vendorable_tu_missing_defines("os/does_not_exist.s") == []

    # Synthetic TU: point LIBULTRA at a temp tree, seed the macro denominator directly.
    monkeypatch.setattr(pt, "LIBULTRA", str(tmp_path))
    monkeypatch.setattr(pt, "_INTREE_ASM_MACROS", {"K0BASE", "C0_ENTRYHI", "LEAF", "END"})
    (tmp_path / "os").mkdir()
    (tmp_path / "os" / "fake.s").write_text(
        '#include "PR/R4300.h"      /* TLB stuff for the RDBPORT comment */\n'
        ".text\n"
        "LEAF(fake)\n"
        "    li      t0, K0BASE\n"            # defined → no flag
        "    mtc0    t0, C0_ENTRYHI\n"        # defined
        "    li      t1, MISSING_MACRO\n"     # undefined → flag
        "    li      t2, ANOTHER_GAP\n"       # undefined → flag
        "END(fake)\n"
    )
    miss = pt.vendorable_tu_missing_defines("os/fake.s")
    assert miss == ["ANOTHER_GAP", "MISSING_MACRO"], miss
    # R4300 (from the include path) and RDBPORT/TLB (comment words) must NOT leak through.
    assert "R4300" not in miss and "RDBPORT" not in miss and "TLB" not in miss


def test_pick_target_json_golden(golden_dir, regen, monkeypatch):
    # The S71 coddog map (tools/coddog/coddog_map.tsv) is gitignored + build-dependent, so the
    # committed golden is map-free; neutralize any local map for a reproducible snapshot.
    monkeypatch.setenv("CODDOG_MAP", "/nonexistent/coddog_map.tsv")
    proc = run_tool("pick_target", "--json", "-n", ROWS)
    assert proc.returncode == 0, proc.stderr
    rows = json.loads(proc.stdout)
    assert isinstance(rows, list) and rows, "expected a non-empty list of candidate rows"

    gpath = golden_dir / "pick_target.json"
    if regen or not gpath.exists():
        gpath.write_text(json.dumps(rows, indent=2, sort_keys=True) + "\n")
        pytest.skip(f"golden regenerated: {gpath.name}")
    expected = json.loads(gpath.read_text())
    assert rows == expected


def test_pick_target_ranked_by_descending_score(golden_dir, regen, monkeypatch):
    """Lock the ranking contract: rows are ordered by `score`, highest first.

    (`score` folds size + bonuses/penalties; the smallest-first intent lives
    inside the score, not in a raw size sort.)
    """
    monkeypatch.setenv("CODDOG_MAP", "/nonexistent/coddog_map.tsv")
    proc = run_tool("pick_target", "--json", "-n", ROWS)
    rows = json.loads(proc.stdout)
    scores = [r["score"] for r in rows]
    assert scores == sorted(scores, reverse=True), "rows must be ranked by descending score"


def test_coddog_mirror_repricing(tmp_path, monkeypatch):
    """S71: an optional coddog map re-prices/flags a none-classified verbatim mirror.

    A candidate pick_target sees as `upstream none` (un-named fns) but coddog matched ≥99% to a
    non-audio ultralib file is re-priced as a libultra mirror + flagged coddog-mirror:<file>@<pct>;
    a <99% or audio hit stays advisory (flag only). Absent map → unchanged (covered by the golden)."""
    # A none-classified pack candidate to target: func_800A0730 (pts-13 none pack:2fn) from the golden.
    mapf = tmp_path / "coddog_map.tsv"
    mapf.write_text(
        "func_800A0730\t__osFakeMirror\tsrc/io/fake.c\t99.99\n"
        "func_800A09E0\t__alFakeAudio\tsrc/audio/fake.c\t99.99\n"
    )
    monkeypatch.setenv("CODDOG_MAP", str(mapf))
    rows = {r["func"]: r for r in json.loads(run_tool("pick_target", "--json", "-n", "200").stdout)}

    hit = rows.get("func_800A0730")
    assert hit is not None and "coddog-mirror:src/io/fake.c@99.99" in hit["hazards"]
    assert hit["upstream"] == "libultra" and hit["pts"] < 13  # re-priced off the classical-pack 13

    audio = rows.get("func_800A09E0")  # audio hit: flagged but NOT re-priced (header-gated)
    assert audio is not None and "coddog-mirror:src/audio/fake.c@99.99" in audio["hazards"]
    assert audio["upstream"] == "none"


def test_coddog_trap_rescan(tmp_path, monkeypatch):
    """S72: a coddog-matched upstream's trap detectors (defines-data / file-static / needs-header)
    re-run on the resolved `.c`, so a verbatim-mirror candidate's hidden BSS/data trap is priced at
    the gate, not discovered mid-sprint. Map a none-classified candidate to a REAL ultralib file
    with the trap — src/io/piacs.c DEFINES `__osPiAccessQueueEnabled` + a `static OSMesg piAccessBuf`
    — and assert both hazards surface (and the seed re-prices off the bare-coddog clean look)."""
    mapf = tmp_path / "coddog_map.tsv"
    mapf.write_text("func_800A0730\t__osPiCreateAccessQueue\tsrc/io/piacs.c\t99.99\n")
    monkeypatch.setenv("CODDOG_MAP", str(mapf))
    rows = {r["func"]: r for r in json.loads(run_tool("pick_target", "--json", "-n", "200").stdout)}

    hit = rows.get("func_800A0730")
    assert hit is not None, "mapped candidate missing from output"
    assert "coddog-mirror:src/io/piacs.c@99.99" in hit["hazards"]
    # The trap the bare coddog flag hid: piacs.c is NOT an atomic verbatim cp.
    assert "file-static" in hit["hazards"], hit["hazards"]
    assert "defines-data:" in hit["hazards"], hit["hazards"]
    # Re-priced as a libultra mirror (≥99% non-audio) but the defines-data/file-static `drop`
    # lifts the seed above a bare clean io mirror (so the gate sees the real cost).
    assert hit["upstream"] == "libultra"
