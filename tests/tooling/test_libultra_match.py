"""Characterization tests for tools/libultra_match.py.

The wider tooling suite does not cover libultra_match at all, so this pins the one behavior the
@functools.cache encapsulation of the module-global _SRC_CACHE touches: member_src's archive-member
-> src-relpath mapping and its splitext fallback. Fixture-based (monkeypatch SRC_OBJ_TREE at a tmp
tree) so it characterizes real mapping behavior even on a checkout where ultralib isn't built --
otherwise an empty/absent SRC_OBJ_TREE would silently pin only the fallback.
"""
from __future__ import annotations

from conftest import load_tool


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
