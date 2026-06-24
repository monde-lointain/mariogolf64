"""Structured-basis-testing net for pick_target._build_row (Code Complete 22.3).

_build_row has a cyclomatic complexity of ~92; the golden suite leaves ~15 branch
sides in its coddog/nusys identity-resolution tail untested (measured via the
scratch basis-gap analyzer). Those are exactly the seams Phase F will extract, so
they need a deterministic net FIRST.

The tail branches key off on-disk upstream files (`_coddog_*_path`, `_meaningful_loc`)
and the live nusys map, which a subprocess golden cannot reach reproducibly. So
these are ISOLATED unit tests (CC 22.5 scaffolding/stubs): load the module
in-process, stub `classify_subseg` + the resolvers, drive `_build_row` directly,
and assert on the row's `hazards`/`upstream`. They characterize CURRENT behavior
(regression guard, CC "compatibility with old data"); they are not new-behavior
tests.
"""

from __future__ import annotations

import types

import pytest
from conftest import load_tool


@pytest.fixture
def pt():
    return load_tool("pick_target")


def _row(
    pt,
    monkeypatch,
    *,
    kind="classical",
    fns,
    size=4000,
    upstream_index=None,
    coddog_index=None,
    nusys_index=None,
    audio_indexes=None,
    audio_roots=None,
    lib=None,
    libultra_band_start=None,
    carried=frozenset(),
    stubs=None,
):
    """Drive pt._build_row in isolation with every on-disk/IO dependency stubbed
    to a deterministic default; per-test `stubs` override individual helpers."""
    upstream_index = upstream_index or {}
    coddog_index = coddog_index or {}
    nusys_index = nusys_index or {}
    audio_indexes = audio_indexes or {}
    audio_roots = audio_roots or {}
    defaults = {
        "classify_subseg": lambda off, typ, path, sz, ui: (kind, list(fns), []),
        "src_func_callers": lambda: {},
        "subseg_vram": lambda off: 0x80100000,
        "_append_coddog_twin_hazard": lambda cfile, fns_, ui, hz: None,
        "_append_coddog_trap_hazards": lambda off, primary, clp, lib_, hz: False,
        "_coddog_upstream_path": lambda cfile: None,
        "_coddog_nusys_path": lambda cfile: None,
        "_upstream_file_func_count": lambda lib_, clp: 0,
        "_meaningful_loc": lambda clp: 0,
        "_coddog_source_banked": lambda s: False,
        "drop_static_mirror_hazard": lambda mp, hz: None,
        "_block_reorder_sibling": lambda up, hz, lib_: None,
    }
    defaults.update(stubs or {})
    for name, fn in defaults.items():
        monkeypatch.setattr(pt, name, fn)
    args = types.SimpleNamespace(lib=lib, include_stuck=False)
    return pt._build_row(
        0x1000,
        "asm",
        None,
        size,
        args,
        upstream_index,
        carried,
        {},
        coddog_index,
        nusys_index,
        audio_indexes,
        audio_roots,
        libultra_band_start,
    )


# --- nusys block (L2895-2917): entirely untested by the goldens -------------


def test_nusys_single_identity_structural(pt, monkeypatch):
    """Multi-fn pack whose members all coddog-match ONE nusys source, big enough vs
    the source's LOC: re-priced libnusys + coddog-structural (size-ratio guard)."""
    row = _row(
        pt,
        monkeypatch,
        fns=["func_A", "func_B"],
        size=100000,
        nusys_index={
            "func_A": ("mainlib/foo.c", 99.99),
            "func_B": ("mainlib/foo.c", 99.99),
        },
        stubs={
            "_coddog_nusys_path": lambda cfile: "/x/foo.c",
            "_meaningful_loc": lambda clp: 5,
        },
    )
    assert row["upstream"] == "libnusys"
    assert "coddog-mirror:mainlib/foo.c@99.99" in row["hazards"]
    assert "coddog-structural:mainlib/foo.c@99.99" in row["hazards"]


def test_nusys_single_identity_not_structural(pt, monkeypatch):
    """Same shape, but the source is large vs the subseg: no structural guard fires
    (covers the FALSE side of the size-ratio decision)."""
    row = _row(
        pt,
        monkeypatch,
        fns=["func_A", "func_B"],
        size=200,
        nusys_index={
            "func_A": ("mainlib/foo.c", 99.99),
            "func_B": ("mainlib/foo.c", 99.99),
        },
        stubs={
            "_coddog_nusys_path": lambda cfile: "/x/foo.c",
            "_meaningful_loc": lambda clp: 9000,
        },
    )
    assert row["upstream"] == "libnusys"
    assert "coddog-mirror:mainlib/foo.c@99.99" in row["hazards"]
    assert "coddog-structural" not in row["hazards"]


def test_nusys_absent_map_leaves_row_classical(pt, monkeypatch):
    """No nusys map -> the block guard (`and nusys_index`) is false, row stays
    classical/none. Guards the additive-pass invariant."""
    row = _row(pt, monkeypatch, fns=["func_A", "func_B"], nusys_index={})
    assert row["upstream"] == "none"
    assert "coddog-mirror" not in row["hazards"]


def test_nusys_map_present_but_no_member_match(pt, monkeypatch):
    """nusys map present but matching NO member (`nz` empty -> FALSE side of `if nz`):
    the block enters its guard but flags nothing, row stays classical/none."""
    row = _row(
        pt,
        monkeypatch,
        fns=["func_A"],
        nusys_index={"func_ZZZ": ("mainlib/x.c", 99.99)},
    )
    assert row["upstream"] == "none"
    assert "coddog-mirror" not in row["hazards"]


def test_nusys_dedup_skips_already_flagged_cfile(pt, monkeypatch):
    """An AUDIO libultra coddog hit (flagged but NOT re-priced, up_lib stays None) on
    the SAME cfile the nusys map carries: the nusys loop sees it in `already` and
    `continue`s (TRUE side of `if cfile in already`)."""
    row = _row(
        pt,
        monkeypatch,
        fns=["func_A"],
        size=300,
        coddog_index={"func_A": ("src/audio/foo.c", 99.99)},
        nusys_index={"func_A": ("src/audio/foo.c", 99.99)},
    )
    # the single coddog-mirror flag is the audio one; nusys did not add a duplicate
    assert row["hazards"].count("coddog-mirror:src/audio/foo.c@99.99") == 1


def test_nusys_up_lib_already_libnusys_on_entry(pt, monkeypatch):
    """up_lib resolved to libnusys from the name index BEFORE the nusys block ->
    the block still runs (guard allows libnusys) but the `up_lib is None` fallback
    is skipped (FALSE side, L2916)."""
    row = _row(
        pt,
        monkeypatch,
        fns=["func_A"],
        size=300,
        upstream_index={"func_A": ("libnusys", None)},
        nusys_index={"func_A": ("mainlib/foo.c", 99.99)},
        stubs={"_coddog_nusys_path": lambda cfile: "/x/foo.c"},
    )
    assert row["upstream"] == "libnusys"
    assert "coddog-mirror:mainlib/foo.c@99.99" in row["hazards"]


# --- primary coddog block (L2806-2832): fncount-mismatch guards -------------


def test_primary_coddog_fncount_mismatch(pt, monkeypatch):
    """Un-named multi-fn pack whose primary coddog-matches a libultra .c that defines
    FEWER fns than the pack holds -> coddog-fncount-mismatch (not a single-file pack)."""
    row = _row(
        pt,
        monkeypatch,
        fns=["func_A", "func_B", "func_C"],
        size=8000,
        coddog_index={"func_A": ("src/io/foo.c", 99.99)},
        stubs={
            "_coddog_upstream_path": lambda cfile: "/x/foo.c",
            "_upstream_file_func_count": lambda lib_, clp: 1,
        },
    )
    assert row["upstream"] == "libultra"
    assert "coddog-fncount-mismatch:1vs3" in row["hazards"]


def test_primary_coddog_single_fn_no_fncount_guard(pt, monkeypatch):
    """Single-fn primary coddog hit: the fncount guard is skipped (FALSE side of
    `len(fns) > 1`)."""
    row = _row(
        pt,
        monkeypatch,
        fns=["func_A"],
        size=2000,
        coddog_index={"func_A": ("src/io/foo.c", 99.99)},
        stubs={
            "_coddog_upstream_path": lambda cfile: "/x/foo.c",
            "_upstream_file_func_count": lambda lib_, clp: 1,
        },
    )
    assert row["upstream"] == "libultra"
    assert "coddog-fncount-mismatch" not in row["hazards"]


# --- tail-identity block (L2840-2885): guards when the id is on a TAIL member -


def test_tail_identity_fncount_and_structural(pt, monkeypatch):
    """Named/un-named leader NOT in coddog_index, but a TAIL member matches ONE
    libultra .c defining fewer fns, big subseg vs LOC -> both tail guards fire and
    the row re-prices to libultra (up_lib fallback, L2884)."""
    row = _row(
        pt,
        monkeypatch,
        fns=["func_A", "func_B", "func_C"],
        size=100000,
        coddog_index={"func_B": ("src/io/bar.c", 99.99)},
        stubs={
            "_coddog_upstream_path": lambda cfile: "/x/bar.c",
            "_upstream_file_func_count": lambda lib_, clp: 1,
            "_meaningful_loc": lambda clp: 5,
        },
    )
    assert row["upstream"] == "libultra"
    assert "coddog-fncount-mismatch:1vs3" in row["hazards"]
    assert "coddog-structural:src/io/bar.c@99.99" in row["hazards"]


# --- singletons -------------------------------------------------------------


def test_asm_tu_libultra_mirror(pt, monkeypatch):
    """A hand-asm libultra mirror (asm-flip, no .c, but present in the asm-TU index)
    is labelled libultra for the column/filter (L2780->2786)."""
    row = _row(
        pt,
        monkeypatch,
        kind="asm-flip",
        fns=["func_A"],
        size=300,
        stubs={"build_asm_tu_index": lambda: {"func_A": "bcopy.s"}},
    )
    assert row["upstream"] == "libultra"


def test_block_reorder_sibling(pt, monkeypatch):
    """A block-reorder family hit names the banked sibling (L2971->2972)."""
    row = _row(
        pt,
        monkeypatch,
        fns=["func_A"],
        size=300,
        stubs={"_block_reorder_sibling": lambda up, hz, lib_: "siblings.c"},
    )
    assert "block-reorder-sibling:siblings.c" in row["hazards"]
