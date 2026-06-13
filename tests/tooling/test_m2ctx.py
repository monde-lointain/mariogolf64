"""Characterization test for tools/m2ctx.py import_c_file.

Integration-flavored (shells out to host `gcc -E`), but cheap and deterministic.
Locks the two behaviors the refactor must preserve: project macros survive
preprocessing, and stock GCC predefined macros are stripped from the output.
"""
from __future__ import annotations

from pathlib import Path

from conftest import ROOT, load_tool

m2ctx = load_tool("m2ctx")


def test_import_c_file_keeps_project_macro_strips_stock(tmp_path: Path):
    src = ROOT / "_m2ctx_charn_test.c"
    src.write_text("#define MG_TEST_MACRO 1234\nint mg_test_symbol;\n")
    try:
        out = m2ctx.import_c_file(str(src))
    finally:
        src.unlink(missing_ok=True)

    # Project macro defined in the input survives.
    assert "MG_TEST_MACRO" in out
    assert "1234" in out
    # The declaration text survives preprocessing.
    assert "mg_test_symbol" in out
    # A stock predefined macro (e.g. __STDC__) is stripped, not duplicated.
    assert "#define __STDC__" not in out
