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


def test_resolve_profile_main_explicit(monkeypatch):
    monkeypatch.setattr(dl, "detect_needs_fastmath", lambda p, s: False)
    prof = dl.resolve_profile("main", "func_80012345", "1050")
    assert (prof.main, prof.libkmc, prof.libultra, prof.fastmath) == (
        True,
        False,
        False,
        False,
    )


def test_resolve_profile_main_with_fastmath(monkeypatch):
    monkeypatch.setattr(dl, "detect_needs_fastmath", lambda p, s: True)
    prof = dl.resolve_profile("main", "func_8006A100", "6A000")
    assert prof.main is True and prof.fastmath is True


def test_resolve_profile_lib_never_gets_fastmath(monkeypatch):
    # A lib profile's CFLAGS are already ground truth — never add -ffast-math,
    # even if the asm has a bare sqrt (detector short-circuited out).
    monkeypatch.setattr(dl, "detect_needs_fastmath", lambda p, s: True)
    prof = dl.resolve_profile("libultra", "func_x", "1050")
    assert prof.libultra is True and prof.fastmath is False


def test_resolve_profile_auto_never_forces_main(monkeypatch):
    # auto must not guess `main` (the F3DEX define has no reliable asm tell).
    monkeypatch.setattr(dl, "detect_libkmc_profile", lambda p: False)
    monkeypatch.setattr(dl, "detect_libultra_profile", lambda p: False)
    monkeypatch.setattr(dl, "detect_needs_fastmath", lambda p, s: False)
    prof = dl.resolve_profile("auto", "func_80012345", "1050")
    assert (prof.main, prof.libkmc, prof.libultra) == (False, False, False)


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
