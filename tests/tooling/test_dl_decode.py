"""Characterization tests for the three display-list decoders (S303).

`dl_decode.py` re-sorts an emitter's packet stores into display-list order;
`rdp_word.py` and `combine_word.py` turn the words it prints back into macro
arguments. They are read-only analysis tools, so the net is a golden of their
stdout on a hand-written fixture `.s` plus a few fixed word pairs whose macros
were confirmed against a host `gbi.h` harness.
"""

from __future__ import annotations

from conftest import run_tool

FIXTURE = "tests/tooling/golden/dl_decode_fixture.s"


def _golden(golden_dir, regen, name, text):
    path = golden_dir / name
    if regen:
        path.write_text(text)
    assert path.read_text() == text


def test_dl_decode_orders_packets(golden_dir, regen):
    """The fixture stores its second packet through a spilled pointer, so the
    program order and the DL order differ; the tool must print DL order."""
    result = run_tool("dl_decode", FIXTURE)
    assert result.returncode == 0, result.stderr
    assert "+0     (8000001C) 0xE7000000" in result.stdout
    _golden(golden_dir, regen, "dl_decode.txt", result.stdout)


def test_rdp_word_decodes_tile_and_rect(golden_dir, regen):
    result = run_tool(
        "rdp_word",
        "F5100400/07000000",
        "F40880C0/070A40CC",
        "F3000000/071FF200",
        "E4020080/0000007C",
        "FF10001F/00000000",
    )
    assert result.returncode == 0, result.stderr
    assert "line=2" in result.stdout
    _golden(golden_dir, regen, "rdp_word.txt", result.stdout)


def test_combine_word_decodes_setcombine(golden_dir, regen):
    result = run_tool("combine_word", "FCFFFFFF/FFFCF87C", "FC157E2A/33FDFCFE")
    assert result.returncode == 0, result.stderr
    assert "gDPSetCombineLERP" in result.stdout
    _golden(golden_dir, regen, "combine_word.txt", result.stdout)
