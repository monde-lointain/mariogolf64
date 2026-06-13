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

from conftest import golden_dir, regen, run_tool  # noqa: F401

ROWS = "50"


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
