"""Characterization tests for tools/seed_c.py pure helpers.

These lock the behavior Phase 2 refactors touch (sanitize_ghidra_body, the
auto-extern classifier, parent-scan helpers) so a structural refactor can be
proven behavior-preserving.
"""

from __future__ import annotations

from pathlib import Path

import pytest
from conftest import load_tool

seed = load_tool("seed_c")


# --- stitch_base_c (golden) ----------------------------------------------
# Locks the assembled base.c text. Exercises every section: rodata warning,
# parent externs, sibling asm, auto-externs, m2c reference, the asm ground-truth
# block, and the sanitized ghidra body. Three variants cover the body-routing
# branches: a trusted decompile, a degenerate (_NON_MATCHING) decompile, and an
# absent ghidra.c (MCP-down / asm-first).


def _stitch_inputs(tmp_path: Path):
    (tmp_path / "m2c.c").write_text("int m2c_ref(void) { return 0; }\n")
    (tmp_path / "ghidra.c").write_text(
        "/* [MM12] copied from ELF */\nundefined4 returns_0(void)\n{\n    return 0;\n}\n"
    )
    (tmp_path / "target.s").write_text(
        "glabel func_80012345\n"
        "/* 0000 3C048001 */  lui   $a0, 0x8001\n"
        "/* 0004 03E00008 */  jr    $ra\n"
        "/* 0008 24840010 */  addiu $a0, $a0, 0x10\n"
    )
    return dict(
        out_dir=tmp_path,
        placeholder="func_80012345",
        externs=["extern u32 gFoo;"],
        sibling_asm_names=["func_80012345", "siblingFn"],
        auto_externs=["extern void *D_80022222;"],
        m2c_path=tmp_path / "m2c.c",
        ghidra_path=tmp_path / "ghidra.c",
        missing_rodata=["D_80099999"],
        target_s_path=tmp_path / "target.s",
    )


def _assert_golden(produced: str, gpath, regen):
    if regen or not gpath.exists():
        gpath.write_text(produced)
        pytest.skip(f"golden regenerated: {gpath.name}")
    assert produced == gpath.read_text()


def test_stitch_base_c_golden(tmp_path, golden_dir, regen):
    """Trusted decompile: body kept as the active start, asm block above it."""
    kwargs = _stitch_inputs(tmp_path)
    out_path = seed.stitch_base_c(seed.BaseCSpec(**kwargs))
    _assert_golden(out_path.read_text(), golden_dir / "seed_c_base.c", regen)


def test_stitch_base_c_degenerate_golden(tmp_path, golden_dir, regen):
    """Degenerate decompile: SUSPECT banner routes the agent to the asm block."""
    kwargs = _stitch_inputs(tmp_path)
    kwargs["ghidra_degenerate"] = True
    out_path = seed.stitch_base_c(seed.BaseCSpec(**kwargs))
    _assert_golden(
        out_path.read_text(), golden_dir / "seed_c_base_degenerate.c", regen
    )


def test_stitch_base_c_no_ghidra_golden(tmp_path, golden_dir, regen):
    """Absent ghidra.c (MCP down): TODO body points at the asm ground truth."""
    kwargs = _stitch_inputs(tmp_path)
    kwargs["ghidra_path"] = tmp_path / "missing-ghidra.c"  # does not exist
    out_path = seed.stitch_base_c(seed.BaseCSpec(**kwargs))
    _assert_golden(
        out_path.read_text(), golden_dir / "seed_c_base_no_ghidra.c", regen
    )


# --- is_degenerate_ghidra_body -------------------------------------------


def test_degenerate_non_matching_suffix():
    # S156: a `_NON_MATCHING`-suffixed decompile is untrustworthy whole-body.
    body = "undefined4 func_80052100_NON_MATCHING(void)\n{\n    return 0;\n}\n"
    assert seed.is_degenerate_ghidra_body(body) is True


def test_degenerate_phantom_shift_flagged_via_suffix():
    # S156 phantom `>> 0x1f` on a plain lw — carried a _NON_MATCHING name.
    body = (
        "undefined4 func_80052070_NON_MATCHING(void)\n"
        "{\n    return DAT_801b6098 >> 0x1f;\n}\n"
    )
    assert seed.is_degenerate_ghidra_body(body) is True


def test_degenerate_return_const_without_suffix():
    # A bare `return 0;` shell is degenerate even without the suffix tell.
    assert seed.is_degenerate_ghidra_body("s32 f(void)\n{\n    return 0;\n}\n") is True
    assert seed.is_degenerate_ghidra_body("void f(void)\n{\n}\n") is True


def test_real_body_not_degenerate():
    # A genuine multi-statement body must NOT be flagged (conservative).
    body = (
        "s32 f(s32 a)\n{\n    s32 x;\n    x = a + 1;\n"
        "    foo(x);\n    return x;\n}\n"
    )
    assert seed.is_degenerate_ghidra_body(body) is False


def test_terse_real_expression_not_degenerate():
    # `return a + 1;` is terse but real (not a bare const) — not flagged.
    body = "s32 f(s32 a)\n{\n    return a + 1;\n}\n"
    assert seed.is_degenerate_ghidra_body(body) is False


# --- sanitize_ghidra_body -------------------------------------------------


def test_sanitize_maps_ghidra_types():
    body = "undefined4 func_80012345(void)\n{\n    byte x;\n    ulonglong y;\n    return;\n}"
    out = seed.sanitize_ghidra_body(body, "func_80012345")
    assert "u32 func_80012345(void)" in out
    assert "u8 x;" in out
    assert "u64 y;" in out
    assert "byte" not in out and "undefined4" not in out


def test_sanitize_renames_function_to_placeholder():
    body = "void returns_0(void)\n{\n    return;\n}"
    out = seed.sanitize_ghidra_body(body, "func_DEADBEEF")
    assert "void func_DEADBEEF(void)" in out
    assert "returns_0" not in out


def test_sanitize_strips_ghidra_audit_header():
    body = "/* [MM12] copied from ELF foo */\nvoid func_80012345(void)\n{\n}"
    out = seed.sanitize_ghidra_body(body, "func_80012345")
    assert "[MM12]" not in out
    assert out.endswith("\n")


# --- auto_externs_for_hi_lo ----------------------------------------------


def test_auto_externs_classification():
    labels = {
        "jtbl_80100000",
        "func_80011111",
        "D_80022222",
        "D_80033333",
        "someGlobal",
    }
    out = seed.auto_externs_for_hi_lo(
        labels,
        placeholder="func_80099999",
        sibling_asm_names=[],
        parent_externs=[],
        pointer_labels={"D_80033333"},
    )
    joined = "\n".join(out)
    assert "extern void *jtbl_80100000[];" in joined
    assert "extern void func_80011111(void);" in joined
    assert "extern u32 D_80022222;" in joined  # non-pointer D_ -> u32
    assert "extern void *D_80033333;" in joined  # pointer-detected D_
    assert "extern char someGlobal[];" in joined  # fallthrough


def test_auto_externs_skips_known_and_placeholder():
    labels = {"func_80099999", "AI_STATUS_REG", "siblingFn", "alreadyExtern"}
    out = seed.auto_externs_for_hi_lo(
        labels,
        placeholder="func_80099999",
        sibling_asm_names=["siblingFn"],
        parent_externs=["extern u32 alreadyExtern;"],
        pointer_labels=set(),
    )
    assert out == []  # all four are skipped


# --- parent_has_real_c / collect_parent_externs ---------------------------


def test_parent_has_real_c_stub_only(tmp_path: Path):
    p = tmp_path / "stub.c"
    p.write_text('#include <ultra64.h>\n\nINCLUDE_ASM("asm/x", func_80099999);\n')
    assert seed.parent_has_real_c(p) is False


def test_parent_has_real_c_with_real_code(tmp_path: Path):
    p = tmp_path / "real.c"
    p.write_text("#include <ultra64.h>\n\nvoid foo(void) { }\n")
    assert seed.parent_has_real_c(p) is True


def test_slice_rodata_pointer_detection(tmp_path, monkeypatch):
    data = tmp_path / "data"
    data.mkdir()
    (data / "x.rodata.s").write_text(
        "dlabel D_80001000\n.word func_80002000\nenddlabel D_80001000\n"
        "dlabel D_80001004\n.word 0x12345678\nenddlabel D_80001004\n"
    )
    monkeypatch.setattr(seed, "ASM_DATA_DIR", data)
    out = tmp_path / "out.rodata.s"
    path, missing, pointers = seed.slice_rodata(
        {"D_80001000", "D_80001004", "D_80009999"}, out
    )
    assert "D_80001000" in pointers  # single `.word <symbol>` => pointer-typed
    assert "D_80001004" not in pointers  # `.word <hex imm>` => not a pointer
    assert missing == ["D_80009999"]  # absent from all data files
    assert path == out and out.exists()


def test_collect_parent_externs(tmp_path: Path):
    p = tmp_path / "parent.c"
    p.write_text(
        "extern u32 gFoo;\n"
        'INCLUDE_ASM("asm/x", func_80011111);\n'
        'INCLUDE_ASM("asm/x", func_80022222);\n'
    )
    externs, asm_names = seed.collect_parent_externs(p)
    assert externs == ["extern u32 gFoo;"]
    assert asm_names == ["func_80011111", "func_80022222"]
