"""Behavioral parsing and bare-sqrt detection tests for decomp_loop.py."""

from __future__ import annotations


import pytest
from conftest import load_tool

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


# --- detect_needs_fastmath / resolve_profile (pure) -----------------------
# Lock the `main`/F3DEX2 + fast-math profile detector (S151/S152).


def _write_target_s(tmp_path, placeholder, body):
    d = tmp_path / placeholder
    d.mkdir()
    (d / "target.s").write_text(f"glabel {placeholder}\n{body}")


def test_detect_needs_fastmath_positive(tmp_path, monkeypatch):
    # A bare FPU `sqrt.s` (real asm format: /* ROM VRAM WORD */ mnemonic).
    _write_target_s(
        tmp_path,
        "func_8003E790",
        "    /* 19B90 8003E790 27BDFFE8 */  addiu   $sp, $sp, -0x18\n"
        "    /* 19B98 8003E798 46007384 */  sqrt.s  $fa1, $fa1\n"
        "    /* 19B9C 8003E79C 03E00008 */  jr      $ra\n",
    )
    monkeypatch.setattr(dl, "NONMATCHINGS_DIR", tmp_path)
    assert dl.detect_needs_fastmath("func_8003E790", "193D0") is True


def test_detect_needs_fastmath_negative_library_call(tmp_path, monkeypatch):
    # `jal sqrt` (library call) is NOT the bare FPU opcode — must not fire.
    _write_target_s(
        tmp_path,
        "func_80012345",
        "    /* 0000 80012345 3C048001 */  lui   $a0, 0x8001\n"
        "    /* 0004 80012349 0C001234 */  jal   sqrt\n"
        "    /* 0008 8001234D 03E00008 */  jr    $ra\n",
    )
    monkeypatch.setattr(dl, "NONMATCHINGS_DIR", tmp_path)
    assert dl.detect_needs_fastmath("func_80012345", "1050") is False


