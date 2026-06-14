"""Unit coverage for tools/decomp_asm.py asm-scanning primitives.

Synthetic-fixture tests (not goldens): a hand-written `asm/<ROM>.s` listing is
written to a tmp file and asm_path is monkeypatched to it, so the assertions are
independent of live repo state. The focus is the whole-subseg rodata scan: a
`.rodata` sibling places the *whole compiled object's* `.rodata`, so a pooled FP
literal loaded by a *non-primary* pack function must be in the flagged extent
(S55: guPerspective's 0x800D2540..0x2558 were undercounted by the old per-primary
scan, sizing the sibling split to 4 of 8 doubles).
"""
from __future__ import annotations

import pytest

from conftest import load_tool

# A 2-function subseg listing. The PRIMARY (fnA) pools one double; the SIBLING
# (fnB) pools a different one. The old per-primary scan saw only fnA's literal.
TWO_FN_ASM = """\
glabel fnA
/* 1000 800A98E0 3C01800D */  lui   $at, %hi(D_800D2520)
/* 1004 800A98E4 D4222520 */  ldc1  $f2, %lo(D_800D2520)($at)
/* 1008 800A98E8 03E00008 */  jr    $ra
endlabel fnA
glabel fnB
/* 100C 800A9A90 3C01800D */  lui   $at, %hi(D_800D2540)
/* 1010 800A9A94 D4202540 */  ldc1  $f0, %lo(D_800D2540)($at)
/* 1014 800A9A98 8C220004 */  lw    $v0, %lo(D_800D2544)($at)
/* 1018 800A9A9C 03E00008 */  jr    $ra
endlabel fnB
"""


@pytest.fixture
def asm(tmp_path, monkeypatch):
    """Load decomp_asm with asm_path pinned to a synthetic 2-function listing."""
    mod = load_tool("decomp_asm")
    f = tmp_path / "DEADBE.s"
    f.write_text(TWO_FN_ASM)
    monkeypatch.setattr(mod, "asm_path", lambda rom_off: str(f))
    return mod


def test_iter_subseg_body_spans_all_functions(asm):
    names = [ln for ln in asm.iter_subseg_body(0xDEADBE) if "ldc1" in ln]
    assert len(names) == 2, "whole-subseg walk must yield both functions' bodies"


def test_iter_function_body_is_per_function(asm):
    """Guards the contrast: the per-function walk still stops at the fn boundary."""
    a_lits = [ln for ln in asm.iter_function_body(0xDEADBE, "fnA") if "ldc1" in ln]
    b_lits = [ln for ln in asm.iter_function_body(0xDEADBE, "fnB") if "ldc1" in ln]
    assert len(a_lits) == 1 and len(b_lits) == 1


def test_rodata_literals_include_sibling_function(asm):
    """The S55 fix: a sibling fn's pooled FP literal is in the flagged extent."""
    assert asm.rodata_literals(0xDEADBE) == [0x800D2520, 0x800D2540]


def test_rodata_word_refs_include_sibling_function(asm):
    """Integer 2nd-word refs are also scanned whole-subseg (sibling fnB's lw)."""
    assert asm.rodata_word_refs(0xDEADBE) == [0x800D2544]


def test_scans_absent_file_return_empty(asm, monkeypatch):
    monkeypatch.setattr(asm, "asm_path", lambda rom_off: "/no/such/asm.s")
    assert asm.rodata_literals(0xDEADBE) == []
    assert list(asm.iter_subseg_body(0xDEADBE)) == []
