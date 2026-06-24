"""Characterization tests for tools/decomp_loop.py.

Two layers:
  - score_diff: pure parsing of asm-differ's raw dict (synthetic fixtures).
  - end-to-end: run the real loop on `rand` (a banked, score-0 leaf) and lock
    the resulting score JSON as a golden snapshot.
"""

from __future__ import annotations

import json

import pytest

from conftest import ROOT, golden_dir, load_tool, regen, run_tool  # noqa: F401

dl = load_tool("decomp_loop")


# --- score_diff (pure) ----------------------------------------------------


def _row(key_base, key_cur, text="insn"):
    mk = lambda k: {"key": k, "text": [{"text": text}]} if k is not None else {}
    return {"base": mk(key_base), "current": mk(key_cur)}


def test_score_diff_perfect_match_uses_current_score():
    raw = {
        "rows": [_row("a", "a"), _row("b", "b")],
        "current_score": 0,
        "max_score": 8,
    }
    out = dl.score_diff(raw)
    assert out["score"] == 0
    assert out["percent"] == 1.0
    assert out["total_rows"] == 2
    assert out["match_count"] == 2
    assert out["top_mismatches"] == []


def test_score_diff_mismatch_reports_score_and_top():
    raw = {
        "rows": [_row("a", "a"), _row("b", "x", text="bad")],
        "current_score": 4,
        "max_score": 8,
    }
    out = dl.score_diff(raw, max_mismatches=5)
    assert out["score"] == 4
    assert out["percent"] == pytest.approx(0.5)
    assert out["match_count"] == 1
    assert len(out["top_mismatches"]) == 1
    assert out["top_mismatches"][0]["row_idx"] == 1


def test_score_diff_respects_max_mismatches():
    raw = {
        "rows": [_row("a", "x"), _row("b", "y"), _row("c", "z")],
        "current_score": 9,
        "max_score": 12,
    }
    out = dl.score_diff(raw, max_mismatches=0)
    assert out["top_mismatches"] == []


# --- end-to-end golden ----------------------------------------------------

GOLDEN_FN = "rand"
STABLE_KEYS = (
    "compile_ok",
    "placeholder",
    "segment",
    "score",
    "percent",
    "total_rows",
    "match_count",
    "max_score",
    "reference_path",
    "current_path",
)


def test_decomp_loop_rand_scores_zero(golden_dir, regen):
    # The loop needs TWO transient gitignored scratch inputs to coexist:
    #   1. nonmatchings/<fn>/base.c (from seed_c), and
    #   2. a top-level asm/<seg>.s declaring `glabel <fn>`, the reference the
    #      loop diffs against (resolved by decomp_loop's own find_segment).
    # `rand` is banked (subseg `c`), so splat never emits asm/<seg>.s for it on a
    # clean tree -- input 2 only lingers as stale leftover from before banking.
    # Both are gitignored: present during a refactor session (the vise), absent on
    # a clean/re-extracted checkout, where this end-to-end check skips and the
    # score_diff unit tests above carry the characterization load.
    base_c = ROOT / "nonmatchings" / GOLDEN_FN / "base.c"
    if not base_c.exists():
        pytest.skip(
            f"{base_c.relative_to(ROOT)} absent (gitignored scratch); run seed_c first"
        )
    if dl.dc.find_segment(GOLDEN_FN) is None:
        pytest.skip(
            f"no `glabel {GOLDEN_FN}` in any asm/*.s (banked fn; reference asm absent on a clean tree)"
        )

    proc = run_tool("decomp_loop", "--func", GOLDEN_FN, "--score-only")
    assert proc.returncode == 0, proc.stderr
    result = json.loads(proc.stdout)
    assert result.get("compile_ok") is True, proc.stdout
    assert result.get("score") == 0

    snapshot = {k: result[k] for k in STABLE_KEYS if k in result}
    gpath = golden_dir / "decomp_loop_rand.json"
    if regen or not gpath.exists():
        gpath.write_text(json.dumps(snapshot, indent=2, sort_keys=True) + "\n")
        pytest.skip(f"golden regenerated: {gpath.name}")
    expected = json.loads(gpath.read_text())
    assert snapshot == expected
