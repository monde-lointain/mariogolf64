"""Assembly slicing must preserve control-flow destinations and function boundaries."""

from __future__ import annotations

import re

from conftest import load_tool

ef = load_tool("extract_functions")


def test_preserves_branch_and_jump_table_destinations():
    source = """glabel dispatch
    /* 0 80000000 10800003 */ beqz $a0, .Ldefault
    /* 4 80000004 00000000 */ nop
    /* 8 80000008 00400008 */ jr $v0
    /* C 8000000C 00000000 */ nop
  jlabel .Lcase0
    /* 10 80000010 10000003 */ b .Lreturn
    /* 14 80000014 24020001 */ addiu $v0, $zero, 1
  jlabel .Lcase1
    /* 18 80000018 10000001 */ b .Lreturn
    /* 1C 8000001C 24020002 */ addiu $v0, $zero, 2
  .Ldefault:
    /* 20 80000020 00001021 */ addu $v0, $zero, $zero
  .Lreturn:
    /* 24 80000024 03E00008 */ jr $ra
    /* 28 80000028 00000000 */ nop
endlabel dispatch
"""
    _, body = ef.extract_functions(source)[0]
    destinations = set(re.findall(r"(?m)^\s*(\.L\w+):", body))
    assert destinations == {".Lcase0", ".Lcase1", ".Ldefault", ".Lreturn"}
    branch_targets = set(re.findall(r"\b(?:beqz|b)\s+[^\n]*?(\.L\w+)", body))
    assert branch_targets <= destinations


def test_jump_labels_remain_local_to_their_function():
    source = """glabel first
  jlabel .Lfirst_case
    /* 0 80000000 03E00008 */ jr $ra
    /* 4 80000004 00000000 */ nop
endlabel first
glabel second
  jlabel .Lsecond_case
    /* 8 80000008 03E00008 */ jr $ra
    /* C 8000000C 00000000 */ nop
endlabel second
"""
    functions = dict(ef.extract_functions(source))
    destinations = {
        name: set(re.findall(r"(?m)^\s*(\.L\w+):", body))
        for name, body in functions.items()
    }
    assert destinations == {
        "first": {".Lfirst_case"},
        "second": {".Lsecond_case"},
    }
