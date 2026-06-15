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


# A subseg whose PRIMARY pools an FP literal (D_) and whose SIBLINGS each do an
# indexed `switch` via a jump table (jtbl_), one plain + one overlay-form. The jtbl
# scan must pick the jtbl_ vrams whole-subseg and must NOT confuse them with the D_
# literal (a distinct `%lo(jtbl_…)` vs `%lo(D_…)` form). S76 __osDevMgrMain.
JTBL_ASM = """\
glabel fnA
/* 2000 800A3700 3C01800D */  lui   $at, %hi(D_800D2520)
/* 2004 800A3704 D4222520 */  ldc1  $f2, %lo(D_800D2520)($at)
/* 2008 800A3708 03E00008 */  jr    $ra
endlabel fnA
glabel fnB
/* 200C 800A3854 3C01800D */  lui   $at, %hi(jtbl_800D2280)
/* 2010 800A3858 8C222280 */  lw    $v0, %lo(jtbl_800D2280)($at)
/* 2014 800A385C 00400008 */  jr    $v0
endlabel fnB
glabel fnC
/* 2018 801F4F44 3C018023 */  lui   $at, %hi(jtbl_ovl3_80231428)
/* 201C 801F4F48 8C221428 */  lw    $v0, %lo(jtbl_ovl3_80231428)($at)
/* 2020 801F4F4C 00400008 */  jr    $v0
endlabel fnC
"""


@pytest.fixture
def jtbl_asm(tmp_path, monkeypatch):
    """Load decomp_asm with asm_path pinned to a synthetic switch-jtbl listing."""
    mod = load_tool("decomp_asm")
    f = tmp_path / "BADA55.s"
    f.write_text(JTBL_ASM)
    monkeypatch.setattr(mod, "asm_path", lambda rom_off: str(f))
    return mod


def test_rodata_jtbls_whole_subseg_plain_and_overlay(jtbl_asm):
    """A switch jump table (jtbl_) is picked whole-subseg, plain + overlay forms (S76)."""
    assert jtbl_asm.rodata_jtbls(0xBADA55) == [0x800D2280, 0x80231428]


def test_rodata_jtbls_excludes_fp_literal(jtbl_asm):
    """The jtbl scan is distinct from the FP-literal (D_) scan — no D_ vram leaks in."""
    assert 0x800D2520 not in jtbl_asm.rodata_jtbls(0xBADA55)


def test_rodata_jtbls_absent_file(jtbl_asm, monkeypatch):
    monkeypatch.setattr(jtbl_asm, "asm_path", lambda rom_off: "/no/such/asm.s")
    assert jtbl_asm.rodata_jtbls(0xBADA55) == []


# privileged_asm vs intrinsic_likely (S70 #1). Three shapes, each a distinct case:
#   privTlb  — a tlbwi + branch/lw logic, NO jal (osMapTLB/osUnmapTLB shape): intrinsic_likely
#              false (work isn't all-INTRINSIC_OPS, no `handwritten` tag), privileged_asm TRUE.
#   privCall — an mtc0 + jal (the audio_sched_thread_entry / exception-dispatch shape):
#              intrinsic_likely false (a jal → "decompilable" early-out), privileged_asm TRUE.
#   plainLeaf— ordinary lui/lw leaf, no privileged op: BOTH false (a genuine classical target).
PRIV_ASM = """\
glabel privTlb
/* 1000 800A0000 3C018000 */  lui   $at, 0x8000
/* 1004 800A0004 10200002 */  beqz  $at, .L800A0010
/* 1008 800A0008 00000000 */   nop
/* 100C 800A000C 42000002 */  tlbwi
.L800A0010:
/* 1010 800A0010 03E00008 */  jr    $ra
endlabel privTlb
glabel privCall
/* 1014 800A0014 40043000 */  mfc0  $a0, $6
/* 1018 800A0018 0C000010 */  jal   func_80000040
/* 101C 800A001C 00000000 */   nop
/* 1020 800A0020 40870000 */  mtc0  $a3, $0
/* 1024 800A0024 03E00008 */  jr    $ra
endlabel privCall
glabel plainLeaf
/* 1028 800A0028 3C01800D */  lui   $at, %hi(D_800D2520)
/* 102C 800A002C 8C220000 */  lw    $v0, 0x0($at)
/* 1030 800A0030 03E00008 */  jr    $ra
endlabel plainLeaf
"""


@pytest.fixture
def priv_asm(tmp_path, monkeypatch):
    mod = load_tool("decomp_asm")
    f = tmp_path / "C0FFEE.s"
    f.write_text(PRIV_ASM)
    monkeypatch.setattr(mod, "asm_path", lambda rom_off: str(f))
    return mod


def test_privileged_asm_flags_tlb_leaf(priv_asm):
    """A tlbwi with surrounding control flow (no jal) — the osMapTLB/osUnmapTLB shape that
    intrinsic_likely misses (work isn't all CP0-moves/sqrt)."""
    assert priv_asm.privileged_asm(0xC0FFEE, "privTlb") is True
    assert priv_asm.intrinsic_likely(0xC0FFEE, "privTlb") is False


def test_privileged_asm_flags_cp0_with_call(priv_asm):
    """An mtc0 alongside a jal — the exception-dispatch / audio_sched_thread_entry shape;
    intrinsic_likely early-outs on the jal, but it is still hand-asm."""
    assert priv_asm.privileged_asm(0xC0FFEE, "privCall") is True
    assert priv_asm.intrinsic_likely(0xC0FFEE, "privCall") is False


def test_privileged_asm_ignores_plain_leaf(priv_asm):
    """No privileged op → a genuine classical target, not an asm-mirror candidate."""
    assert priv_asm.privileged_asm(0xC0FFEE, "plainLeaf") is False


def test_privileged_asm_absent_file(priv_asm, monkeypatch):
    monkeypatch.setattr(priv_asm, "asm_path", lambda rom_off: "/no/such/asm.s")
    assert priv_asm.privileged_asm(0xC0FFEE, "privTlb") is False
