"""Characterization golden for tools/pick_target.py.

pick_target is pure-static (a deterministic function of the repo's yaml + asm +
symbol files), so its `--json` output is a stable snapshot. This is the crown-
jewel golden: it exercises build_rows, the hazard detectors, signature_hint, and
the scoring/sort in one shot. A diff after refactoring = a behavior regression.

The golden is a *refactor-session* snapshot tied to current repo state; if the
yaml/asm/symbols change for unrelated reasons, regenerate with REGEN_GOLDEN=1.
"""
from __future__ import annotations

import json

import pytest

from conftest import golden_dir, load_tool, regen, run_tool  # noqa: F401

ROWS = "50"


def test_vendorable_tu_missing_defines(tmp_path, monkeypatch):
    """S57 retro #1: the asm-mirror needs-define pre-check (vendorable_tu_missing_defines).

    A vendored ultralib .s must assemble under the in-tree `-I` headers; a macro none of them
    define is a needs-define enabler. Verifies: clean backlog TUs report nothing; an undefined
    UPPER_CASE macro is flagged; comment words and the `#include` path token never false-flag;
    lower-case register names are ignored."""
    pt = load_tool("pick_target")

    # The whole current vendoring backlog is self-contained (all macros ship in-tree).
    for rel in ("os/maptlbrdb.s", "os/probetlb.s", "gu/sqrtf.s", "libc/bcopy.s"):
        assert pt.vendorable_tu_missing_defines(rel) == [], rel
    assert pt.vendorable_tu_missing_defines(None) == []
    assert pt.vendorable_tu_missing_defines("os/does_not_exist.s") == []

    # Synthetic TU: point LIBULTRA at a temp tree, seed the macro denominator directly.
    monkeypatch.setattr(pt, "LIBULTRA", str(tmp_path))
    monkeypatch.setattr(pt, "_INTREE_ASM_MACROS", {"K0BASE", "C0_ENTRYHI", "LEAF", "END"})
    (tmp_path / "os").mkdir()
    (tmp_path / "os" / "fake.s").write_text(
        '#include "PR/R4300.h"      /* TLB stuff for the RDBPORT comment */\n'
        ".text\n"
        "LEAF(fake)\n"
        "    li      t0, K0BASE\n"            # defined → no flag
        "    mtc0    t0, C0_ENTRYHI\n"        # defined
        "    li      t1, MISSING_MACRO\n"     # undefined → flag
        "    li      t2, ANOTHER_GAP\n"       # undefined → flag
        "END(fake)\n"
    )
    miss = pt.vendorable_tu_missing_defines("os/fake.s")
    assert miss == ["ANOTHER_GAP", "MISSING_MACRO"], miss
    # R4300 (from the include path) and RDBPORT/TLB (comment words) must NOT leak through.
    assert "R4300" not in miss and "RDBPORT" not in miss and "TLB" not in miss


def test_pick_target_json_golden(golden_dir, regen, monkeypatch):
    # The S71 coddog map (tools/coddog/coddog_map.tsv) is gitignored + build-dependent, so the
    # committed golden is map-free; neutralize any local map for a reproducible snapshot.
    monkeypatch.setenv("CODDOG_MAP", "/nonexistent/coddog_map.tsv")
    proc = run_tool("pick_target", "--json", "-n", ROWS)
    assert proc.returncode == 0, proc.stderr
    rows = json.loads(proc.stdout)
    assert isinstance(rows, list) and rows, "expected a non-empty list of candidate rows"

    gpath = golden_dir / "pick_target.json"
    if regen or not gpath.exists():
        gpath.write_text(json.dumps(rows, indent=2, sort_keys=True) + "\n")
        pytest.skip(f"golden regenerated: {gpath.name}")
    expected = json.loads(gpath.read_text())
    assert rows == expected


def test_pick_target_ranked_by_descending_score(golden_dir, regen, monkeypatch):
    """Lock the ranking contract: rows are ordered by `score`, highest first.

    (`score` folds size + bonuses/penalties; the smallest-first intent lives
    inside the score, not in a raw size sort.)
    """
    monkeypatch.setenv("CODDOG_MAP", "/nonexistent/coddog_map.tsv")
    proc = run_tool("pick_target", "--json", "-n", ROWS)
    rows = json.loads(proc.stdout)
    scores = [r["score"] for r in rows]
    assert scores == sorted(scores, reverse=True), "rows must be ranked by descending score"


def test_coddog_mirror_repricing(tmp_path, monkeypatch):
    """S71: an optional coddog map re-prices/flags a none-classified verbatim mirror.

    A candidate pick_target sees as `upstream none` (un-named fns) but coddog matched ≥99% to a
    non-audio ultralib file is re-priced as a libultra mirror + flagged coddog-mirror:<file>@<pct>;
    a <99% or audio hit stays advisory (flag only). Absent map → unchanged (covered by the golden)."""
    # A none-classified pack candidate to target: func_800A0730 (pts-13 none pack:2fn) from the golden.
    mapf = tmp_path / "coddog_map.tsv"
    mapf.write_text(
        "func_800A0730\t__osFakeMirror\tsrc/io/fake.c\t99.99\n"
        "func_800A09E0\t__alFakeAudio\tsrc/audio/fake.c\t99.99\n"
    )
    monkeypatch.setenv("CODDOG_MAP", str(mapf))
    rows = {r["func"]: r for r in json.loads(run_tool("pick_target", "--json", "-n", "200").stdout)}

    hit = rows.get("func_800A0730")
    assert hit is not None and "coddog-mirror:src/io/fake.c@99.99" in hit["hazards"]
    assert hit["upstream"] == "libultra" and hit["pts"] < 13  # re-priced off the classical-pack 13

    audio = rows.get("func_800A09E0")  # audio hit: flagged but NOT re-priced (header-gated)
    assert audio is not None and "coddog-mirror:src/audio/fake.c@99.99" in audio["hazards"]
    assert audio["upstream"] == "none"


def test_coddog_trap_rescan(tmp_path, monkeypatch):
    """S72: a coddog-matched upstream's trap detectors (defines-data / file-static / needs-header)
    re-run on the resolved `.c`, so a verbatim-mirror candidate's hidden BSS/data trap is priced at
    the gate, not discovered mid-sprint. Map a none-classified candidate to a REAL ultralib file
    with the trap — src/io/piacs.c DEFINES `__osPiAccessQueueEnabled` + a `static OSMesg piAccessBuf`
    — and assert both hazards surface (and the seed re-prices off the bare-coddog clean look)."""
    mapf = tmp_path / "coddog_map.tsv"
    mapf.write_text("func_800A0730\t__osPiCreateAccessQueue\tsrc/io/piacs.c\t99.99\n")
    monkeypatch.setenv("CODDOG_MAP", str(mapf))
    rows = {r["func"]: r for r in json.loads(run_tool("pick_target", "--json", "-n", "200").stdout)}

    hit = rows.get("func_800A0730")
    assert hit is not None, "mapped candidate missing from output"
    assert "coddog-mirror:src/io/piacs.c@99.99" in hit["hazards"]
    # The trap the bare coddog flag hid: piacs.c is NOT an atomic verbatim cp.
    assert "file-static" in hit["hazards"], hit["hazards"]
    assert "defines-data:" in hit["hazards"], hit["hazards"]
    # Re-priced as a libultra mirror (≥99% non-audio) but the defines-data/file-static `drop`
    # lifts the seed above a bare clean io mirror (so the gate sees the real cost).
    assert hit["upstream"] == "libultra"


def test_coddog_suppresses_maybe_upstream(tmp_path, monkeypatch):
    """S75: a definitive (>=CODDOG_MIRROR_PCT, non-audio) coddog identity suppresses the weaker
    maybe-upstream IDF guess on the same row. Once coddog has named the fn's exact upstream file,
    the IDF guess is redundant noise that can point at the WRONG file (S75 func_800A7190 carried
    coddog-mirror:src/io/contquery.c@99.99 AND a mis-pointed maybe-upstream:voice*). An audio or
    sub-threshold coddog hit stays advisory, so the guess is NOT suppressed there."""
    # Baseline, map-free: func_800A07B0 is an un-named candidate carrying the IDF guess.
    monkeypatch.setenv("CODDOG_MAP", "/nonexistent/coddog_map.tsv")
    base = {r["func"]: r for r in json.loads(run_tool("pick_target", "--json", "-n", "400").stdout)}
    assert "maybe-upstream" in base["func_800A07B0"]["hazards"], "baseline expects the IDF guess"

    # Definitive non-audio coddog hit -> maybe-upstream suppressed; coddog-mirror stands alone.
    mapf = tmp_path / "coddog_map.tsv"
    mapf.write_text("func_800A07B0\t__osFakeMirror\tsrc/io/fake.c\t99.99\n")
    monkeypatch.setenv("CODDOG_MAP", str(mapf))
    hit = {r["func"]: r for r in json.loads(run_tool("pick_target", "--json", "-n", "400").stdout)}["func_800A07B0"]
    assert "coddog-mirror:src/io/fake.c@99.99" in hit["hazards"]
    assert "maybe-upstream" not in hit["hazards"], hit["hazards"]

    # Audio coddog hit is advisory-only (header-gated, not re-priced) -> the guess is retained.
    mapf.write_text("func_800A07B0\t__alFakeAudio\tsrc/audio/fake.c\t99.99\n")
    monkeypatch.setenv("CODDOG_MAP", str(mapf))
    aud = {r["func"]: r for r in json.loads(run_tool("pick_target", "--json", "-n", "400").stdout)}["func_800A07B0"]
    assert "coddog-mirror:src/audio/fake.c@99.99" in aud["hazards"]
    assert "maybe-upstream" in aud["hazards"], aud["hazards"]


def test_caller_evict_flag(tmp_path, monkeypatch):
    """S77 retro #1: src_func_callers() prices the gate caller-eviction.

    Naming an un-named func_<vram> (a gate symbol add) makes splat rename it; a banked C file that
    hard-codes the old func_ name then fails to link (S77 hit it: __osSpGetStatus add evicted the
    banked caller func_800AB600.c). The helper maps each func_ token to the banked callers that
    reference it, EXCLUDING INCLUDE_ASM stub lines (the func_'s own scaffold is regenerated, not an
    eviction). Verifies: a real call site is mapped; the own-stub line is not counted; an
    un-referenced func_ is absent."""
    pt = load_tool("pick_target")
    src = tmp_path / "src"
    (src / "main").mkdir(parents=True)
    (src / "libultra" / "io").mkdir(parents=True)
    # banked caller: references func_800B16A0 by name (extern decl + call)
    (src / "main" / "caller.c").write_text(
        "extern u32 func_800B16A0(void);\n"
        "u32 wrapper(void) { return func_800B16A0(); }\n"
    )
    # the func_'s own scaffold stub — its INCLUDE_ASM line must be EXCLUDED, not an eviction
    (src / "libultra" / "io" / "spgetstat.c").write_text(
        '#include "common.h"\n'
        'INCLUDE_ASM("asm/nonmatchings/libultra/io/spgetstat", func_800B16A0);\n'
    )
    monkeypatch.setattr(pt, "ROOT", str(tmp_path))
    monkeypatch.setattr(pt, "_SRC_CALLER_CACHE", None)
    callers = pt.src_func_callers()
    assert callers.get("func_800B16A0") == ["src/main/caller.c"], callers
    assert "func_deadbeef" not in callers and "func_DEADBEEF" not in callers


def _coddog_rows(pt, monkeypatch, *, ui, carried, coddog):
    """Drive build_rows over ONE synthetic 2-fn asm subseg with the file-reading helpers stubbed,
    so the new coddog-tail logic is tested in isolation (no tree deps). Leader is `namedLeader`
    (named, in ui+carried); tail is `func_1100`."""
    class A:
        lib = None
        include_stuck = False
        n = 999
    monkeypatch.setattr(pt, "parse_subsegs", lambda: [(0x1000, "asm", None), (0x2000, "asm", None)])
    monkeypatch.setattr(pt, "classify_subseg",
                        lambda off, typ, path, size, _ui:
                        ("asm-flip", ["namedLeader", "func_1100"], []) if off == 0x1000 else None)
    monkeypatch.setattr(pt, "src_func_callers", lambda: {})
    monkeypatch.setattr(pt, "append_upstream_hazards", lambda *a, **k: ("warm", False))
    monkeypatch.setattr(pt, "score_row",
                        lambda off, primary, kind, fns, size, up_lib, band, blocked, hz, carr: {
                            "func": primary, "rom": hex(off), "upstream": up_lib,
                            "score": 0, "size": size,
                            "hazards": ",".join(h.detail for h in hz
                                                if h.kind == pt.HAZARD_CODDOG_MIRROR)})
    return pt.build_rows(A(), ui, carried, {}, coddog)


def test_coddog_tail_overrides_carried_namedrop(monkeypatch):
    """S78: a multi-fn asm subseg's mirror identity often lives in its UN-NAMED tail, not its named
    (or upstream-mis-attributed) leader. carry_over_names() scoops every backticked token from the
    BACKLOG digest log, so a banked callee name-dropped in a carry-over paragraph (S45 did this to
    `__osGbpakSetBank`) falsely de-ranks the whole subseg. A definitive (>=PCT, non-audio) coddog
    hit on ANY member must (a) surface that identity even when the leader carries a (wrong) upstream
    path, and (b) override the carried name-drop drop. Real case: [0x8CE90] = __osGbpakSetBank
    (named, mis-attributed to gbpakreadwrite.c via a forward prototype) + tail func_->pfsisplug.c."""
    pt = load_tool("pick_target")
    rows = _coddog_rows(
        pt, monkeypatch,
        ui={"namedLeader": ("libultra", "/x/wrong.c")},   # mis-attributed (prototype) upstream
        carried={"namedLeader"},                          # name-dropped in the digest log
        coddog={"func_1100": ("src/io/pfsisplug.c", 99.99)})
    got = [r for r in rows if r["func"] == "namedLeader"]
    assert got, "a coddog-identified subseg must survive the carried name-drop filter"
    assert "src/io/pfsisplug.c@99.99" in got[0]["hazards"], got[0]["hazards"]
    assert got[0]["upstream"] == "libultra"


def test_carried_namedrop_still_drops_without_coddog(monkeypatch):
    """The carried filter still de-ranks a genuinely-carried leader when NO member is coddog'd — the
    S78 exemption is scoped strictly to definitive coddog identities, not a blanket disable."""
    pt = load_tool("pick_target")
    rows = _coddog_rows(
        pt, monkeypatch,
        ui={"namedLeader": ("libultra", "/x/wrong.c")},
        carried={"namedLeader"},
        coddog={})  # no coddog identity -> the name-drop drop still applies
    assert not [r for r in rows if r["func"] == "namedLeader"]


def test_resolve_include_vendored_basename_fallback(tmp_path, monkeypatch):
    """S80 #1: a vendored-prefix include (`PRinternal/controller.h`) whose in-tree copy drops the
    prefix (`.../internal/controller.h`) resolves via a BASENAME fallback, so the header walk scans
    it and refs_unplaced no longer phantom-flags a struct TYPE it typedefs as an unplaced data extern
    (the __OSContRequesFormatShort false-positive on pfsgetstatus.c)."""
    pt = load_tool("pick_target")
    inc = tmp_path / "inc"
    (inc / "internal").mkdir(parents=True)
    (inc / "internal" / "controller.h").write_text(
        "typedef struct {\n    int rxsize;\n} __OSContRequesFormatShort;\n")
    monkeypatch.setattr(pt, "INCLUDE_DIRS", [str(inc), str(inc / "internal")])
    # exact prefixed path misses; basename `controller.h` resolves under internal/
    assert pt._resolve_include("PRinternal/controller.h") == str(inc / "internal" / "controller.h")
    # a bare basename absent everywhere still returns None (no false resolution)
    assert pt._resolve_include("nope.h") is None
    cfile = tmp_path / "src.c"
    cfile.write_text('#include "PRinternal/controller.h"\n'
                     "void f(void){ __OSContRequesFormatShort t; }\n")
    assert "__OSContRequesFormatShort" in pt.declared_type_names(str(cfile))
    # ...so refs_unplaced excludes it (a TYPE, not an unplaced data extern)
    assert "__OSContRequesFormatShort" not in pt.refs_unplaced(str(cfile), set(), "libultra")


def test_call_divergence_strips_inactive_version_branch(monkeypatch):
    """S80 #2: call_divergence drops the inactive `#if BUILD_VERSION` side before counting C calls,
    so an `#else` branch's call does not double-count against the active branch's (pfsgetstatus's
    non-J `__osPfsRequestOneChannel(channel)` inflated 6→7, a phantom `7vs6` on a byte-clean mirror).
    Without the lib's build_ord (None → 0) the strip is a no-op and the phantom returns."""
    pt = load_tool("pick_target")
    # _upstream_body returns the brace body (no signature line), so name only real call sites.
    body = ("#if BUILD_VERSION >= VERSION_J\n"
            "    foo(a, b);\n"
            "#else\n"
            "    foo(a);\n"        # dead under J — must NOT count
            "#endif\n"
            "    bar();\n")
    monkeypatch.setattr(pt, "_upstream_body", lambda cp, pr: body)
    monkeypatch.setattr(pt, "_asm_jal_count", lambda off, pr: 2)  # J build: foo + bar
    monkeypatch.setattr(pt, "_build_version_ord",
                        lambda lib: pt._VERSION_ORD["VERSION_J"] if lib else 0)
    assert pt.call_divergence(0x1000, "ff", "x.c", "libultra") is None  # strip → 2vs2 → no hazard
    d = pt.call_divergence(0x1000, "ff", "x.c", None)                   # no strip → foo doubles → 3vs2
    assert d is not None and d.detail == "3vs2"


def test_coddog_tail_trap_rescan(monkeypatch):
    """S80 #3: a coddog identity carried by an UN-NAMED TAIL member (not the named leader) re-runs
    the file-level trap battery on the resolved upstream, so its defines-data/file-static is priced.
    initialize.c's coddog hit keys on the sibling `create_speed_param`, not leader
    `__osInitialize_common`, so before this only the S78 bare coddog flag surfaced — its defines-data
    `.data` carve went un-priced. Map the tail to the real trap file src/io/piacs.c and assert
    defines-data + file-static appear via the tail path."""
    pt = load_tool("pick_target")

    class A:
        lib = None
        include_stuck = False
        n = 999
    monkeypatch.setattr(pt, "parse_subsegs", lambda: [(0x1000, "asm", None)])
    monkeypatch.setattr(pt, "classify_subseg",
                        lambda off, typ, path, size, _ui: ("asm-flip", ["namedLeader", "func_1100"], []))
    monkeypatch.setattr(pt, "src_func_callers", lambda: {})
    monkeypatch.setattr(pt, "append_upstream_hazards", lambda *a, **k: ("warm", False))
    # asm-reading helpers: off=0x1000 is synthetic, return empty so only the upstream .c is read.
    monkeypatch.setattr(pt, "recover_unplaced_vram", lambda off: [])
    monkeypatch.setattr(pt, "recover_unplaced_call_vram", lambda off, primary: [])
    monkeypatch.setattr(pt, "rodata_jtbls", lambda off: [])
    monkeypatch.setattr(pt, "score_row",
                        lambda off, primary, kind, fns, size, up_lib, band, blocked, hz, carr: {
                            "func": primary, "upstream": up_lib, "score": 0, "size": size,
                            "hazards": ",".join(h.kind + (":" + h.detail if h.detail else "") for h in hz)})
    rows = pt.build_rows(A(), {"namedLeader": ("libultra", "/x/wrong.c")}, set(), {},
                         {"func_1100": ("src/io/piacs.c", 99.99)})
    hit = next(r for r in rows if r["func"] == "namedLeader")
    assert "coddog-mirror:src/io/piacs.c@99.99" in hit["hazards"]
    assert "file-static" in hit["hazards"], hit["hazards"]
    assert "defines-data:__osPiAccessQueueEnabled" in hit["hazards"], hit["hazards"]
