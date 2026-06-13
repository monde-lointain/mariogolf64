"""Characterization tests for tools/extract_functions.py (pure formatting logic)."""
from __future__ import annotations

from conftest import load_tool

ef = load_tool("extract_functions")


def test_format_line_label():
    assert ef.format_line("  .L80099F64:") == ".L80099F64:"


def test_format_line_instruction_strips_dollars_and_aligns():
    line = "    /* 75354 80099F54 14800003 */  bnez       $a0, .L80099F64"
    # operands lose $, instr left-justified to width 11 after an 8-space indent
    assert ef.format_line(line) == "        bnez       a0, .L80099F64"


def test_format_line_instruction_no_operands():
    assert ef.format_line("    /* 0 0 0 */  nop") == "        nop"


def test_format_line_skips_non_matching():
    assert ef.format_line("glabel foo") is None
    assert ef.format_line("") is None


def test_extract_functions_single():
    content = "\n".join([
        "glabel test_fn",
        "    /* 0 80000000 27BDFFE8 */  addiu      $sp, $sp, -0x18",
        "  .L80000004:",
        "    /* 4 80000004 03E00008 */  jr         $ra",
        "endlabel test_fn",
    ])
    funcs = ef.extract_functions(content)
    assert len(funcs) == 1
    name, body = funcs[0]
    assert name == "test_fn"
    assert body.splitlines() == [
        "test_fn",
        "        addiu      sp, sp, -0x18",
        ".L80000004:",
        "        jr         ra",
    ]


def test_extract_functions_multiple():
    content = "\n".join([
        "glabel a",
        "    /* 0 0 0 */  nop",
        "endlabel a",
        "glabel b",
        "    /* 0 0 0 */  nop",
        "endlabel b",
    ])
    names = [n for n, _ in ef.extract_functions(content)]
    assert names == ["a", "b"]
