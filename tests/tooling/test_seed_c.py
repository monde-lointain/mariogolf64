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
# Locks the assembled base.c text before stitch_base_c's 8-arg signature is
# refactored into a parameter object. Exercises every section: rodata warning,
# parent externs, sibling asm, auto-externs, m2c reference, sanitized ghidra body.


def _stitch_inputs(tmp_path: Path):
    (tmp_path / "m2c.c").write_text("int m2c_ref(void) { return 0; }\n")
    (tmp_path / "ghidra.c").write_text(
        "/* [MM12] copied from ELF */\nundefined4 returns_0(void)\n{\n    return 0;\n}\n"
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
    )


def test_stitch_base_c_golden(tmp_path, golden_dir, regen):
    kwargs = _stitch_inputs(tmp_path)
    out_path = seed.stitch_base_c(seed.BaseCSpec(**kwargs))
    produced = out_path.read_text()

    gpath = golden_dir / "seed_c_base.c"
    if regen or not gpath.exists():
        gpath.write_text(produced)
        pytest.skip(f"golden regenerated: {gpath.name}")
    assert produced == gpath.read_text()


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
