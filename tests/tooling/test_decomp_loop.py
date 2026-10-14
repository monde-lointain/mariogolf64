"""Behavioral parsing and bare-sqrt detection tests for decomp_loop.py."""

from __future__ import annotations


import pytest
from conftest import load_tool

dl = load_tool("decomp_loop")


@pytest.mark.parametrize(
    "replacement",
    [
        "   4:\t30427ffe\tandi\tv0,v0,0x7ffe\n",
        "   4:\t30437fff\tandi\tv1,v0,0x7fff\n",
        "",
    ],
)
def test_real_differ_reports_operand_changes_and_deleted_delay_slot(replacement):
    header = (
        "\nexample.o:     file format elf32-tradbigmips\n"
        "\nDisassembly of section .text:\n"
        "\n00000000 <example>:\n"
        "   0:\t03e00008\tjr\tra\n"
    )
    delay_slot = "   4:\t30427fff\tandi\tv0,v0,0x7fff\n"
    same = dl.score_diff(dl.run_asm_differ(header + delay_slot, header + delay_slot))
    raw = dl.run_asm_differ(header + delay_slot, header + replacement)
    changed = dl.score_diff(raw)
    assert same["score"] == 0
    assert same["match_count"] == 2
    assert changed["score"] > 0
    assert changed["match_count"] == 1
    assert "andi" in changed["top_mismatches"][0]["base_text"]
    assert dl.score_diff(raw, max_mismatches=0)["top_mismatches"] == []


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


