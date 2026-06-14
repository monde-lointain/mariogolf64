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


def test_pick_target_json_golden(golden_dir, regen):
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


def test_pick_target_ranked_by_descending_score(golden_dir, regen):
    """Lock the ranking contract: rows are ordered by `score`, highest first.

    (`score` folds size + bonuses/penalties; the smallest-first intent lives
    inside the score, not in a raw size sort.)
    """
    proc = run_tool("pick_target", "--json", "-n", ROWS)
    rows = json.loads(proc.stdout)
    scores = [r["score"] for r in rows]
    assert scores == sorted(scores, reverse=True), "rows must be ranked by descending score"
