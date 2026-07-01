"""Characterization tests for tools/libultra_match.py.

The wider tooling suite does not cover libultra_match at all, so this pins the one behavior the
@functools.cache encapsulation of the module-global _SRC_CACHE touches: member_src's archive-member
-> src-relpath mapping and its splitext fallback. Fixture-based (monkeypatch SRC_OBJ_TREE at a tmp
tree) so it characterizes real mapping behavior even on a checkout where ultralib isn't built --
otherwise an empty/absent SRC_OBJ_TREE would silently pin only the fallback.
"""

from __future__ import annotations

import os

import pytest
from conftest import golden_dir, load_tool, regen  # noqa: F401
from conftest import run_tool


def test_worklist_golden(golden_dir, regen):
    """End-to-end characterization of the whole matcher pipeline (partition -> calibrate ->
    match -> emit-worklist) in one shot; a diff after refactoring = a behavior change. Env-gated:
    the union reference archives must be present (skipped on a checkout with no built ultralib).
    HOME-normalized so the golden is host-portable."""
    lm = load_tool("libultra_match")
    if not any(os.path.exists(a) for a in lm.ARCHIVES):
        pytest.skip("no libultra reference archive present")
    proc = run_tool("libultra_match", "--min-insns", "5")
    assert proc.returncode == 0, proc.stderr
    out = proc.stdout.replace(os.path.expanduser("~"), "~")
    gpath = golden_dir / "libultra_match.txt"
    if regen or not gpath.exists():
        gpath.write_text(out)
        pytest.skip(f"golden regenerated: {gpath.name}")
    assert out == gpath.read_text()


def test_member_src_index_hit_and_fallback(tmp_path, monkeypatch):
    lm = load_tool("libultra_match")

    # fixture object tree: members under subdirs (basenames match archive members)
    (tmp_path / "gu").mkdir()
    (tmp_path / "gu" / "rotate.o").write_bytes(b"")
    (tmp_path / "os").mkdir()
    (tmp_path / "os" / "setintmask.o").write_bytes(b"")
    monkeypatch.setattr(lm, "SRC_OBJ_TREE", str(tmp_path))

    # index hit: basename -> relpath stem (drops .o, keeps the subdir)
    assert lm.member_src("rotate.o") == "gu/rotate"
    assert lm.member_src("setintmask.o") == "os/setintmask"
    # fallback: member absent from the tree -> splitext of the bare basename
    assert lm.member_src("does_not_exist.o") == "does_not_exist"
