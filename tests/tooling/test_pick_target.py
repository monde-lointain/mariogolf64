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


@pytest.fixture(autouse=True)
def _no_live_nusys_map(monkeypatch):
    """Keep the live, gitignored, build-dependent nusys coddog map (tools/coddog/nusys_map.tsv,
    written by tools/nusys_sweep.sh) out of every test so the goldens stay reproducible — the same
    reason each golden neutralizes CODDOG_MAP. A nusys-specific test can override NUSYS_CODDOG_MAP."""
    monkeypatch.setenv("NUSYS_CODDOG_MAP", "/nonexistent/nusys_map.tsv")


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
    monkeypatch.setattr(
        pt,
        "_intree_asm_macros",
        lambda: frozenset({"K0BASE", "C0_ENTRYHI", "LEAF", "END"}),
    )
    (tmp_path / "os").mkdir()
    (tmp_path / "os" / "fake.s").write_text(
        '#include "PR/R4300.h"      /* TLB stuff for the RDBPORT comment */\n'
        ".text\n"
        "LEAF(fake)\n"
        "    li      t0, K0BASE\n"  # defined → no flag
        "    mtc0    t0, C0_ENTRYHI\n"  # defined
        "    li      t1, MISSING_MACRO\n"  # undefined → flag
        "    li      t2, ANOTHER_GAP\n"  # undefined → flag
        "END(fake)\n"
    )
    miss = pt.vendorable_tu_missing_defines("os/fake.s")
    assert miss == ["ANOTHER_GAP", "MISSING_MACRO"], miss
    # R4300 (from the include path) and RDBPORT/TLB (comment words) must NOT leak through.
    assert "R4300" not in miss and "RDBPORT" not in miss and "TLB" not in miss


def test_gbi_value_guard_needs_define(tmp_path, monkeypatch):
    """S83 retro #1: a mirror candidate using a GBI-value-guarded macro (OS_YIELD_DATA_SIZE) is
    flagged needs-define when NO guard microcode define is active, and clean when one is. Also
    confirms LIBULTRA_CFLAGS's -DF3DEX_GBI_2 / -DBUILD_VERSION reach the libultra active-define set."""
    pt = load_tool("pick_target")

    # Part A: the live Makefile's LIBULTRA_CFLAGS defines now reach the libultra active set
    # (previously libultra fell through to the bare main CFLAGS, hiding the GBI define).
    pt._parse_makefile_defines.cache_clear()
    libu = pt._active_defines_for_lib("libultra")
    assert "F3DEX_GBI_2" in libu and "BUILD_VERSION" in libu

    # Part B: synthetic header with the PR/sptask.h guard shape + a candidate that uses the macro.
    inc = tmp_path / "inc"
    inc.mkdir()
    (inc / "sptask.h").write_text(
        "#if (defined(F3DEX_GBI)||defined(F3DLP_GBI)||defined(F3DEX_GBI_2))\n"
        "#define\tOS_YIELD_DATA_SIZE\t\t0xc00\n"
        "#else\n"
        "#define OS_YIELD_DATA_SIZE 0x900\n"
        "#endif\n"
    )
    cand = tmp_path / "sptask.c"
    cand.write_text("void f(void){ g(p + OS_YIELD_DATA_SIZE - 4); }\n")

    monkeypatch.setattr(pt, "INCLUDE_DIRS", [str(inc)])
    monkeypatch.setattr(pt, "LIB_EXTRA_INCLUDE_DIRS", {})
    pt._gbi_guarded_macros.cache_clear()

    # the value-guard shape is detected (guard defines preserved in source order)
    assert pt._scan_value_guards((inc / "sptask.h").read_text()) == {
        "OS_YIELD_DATA_SIZE": ("F3DEX_GBI", "F3DLP_GBI", "F3DEX_GBI_2")
    }

    # no GBI define active → flagged, canonical F3DEX_GBI_2 reported
    monkeypatch.setattr(
        pt, "_active_defines_for_lib", lambda lib: frozenset({"_FINALROM"})
    )
    assert pt.gbi_value_guard_needs_define(str(cand), "libultra") == "F3DEX_GBI_2"

    # F3DEX_GBI_2 active → the macro resolves, no flag (the standing LIBULTRA_CFLAGS case)
    monkeypatch.setattr(
        pt, "_active_defines_for_lib", lambda lib: frozenset({"F3DEX_GBI_2"})
    )
    assert pt.gbi_value_guard_needs_define(str(cand), "libultra") is None

    # a candidate that never uses the macro → no flag even with nothing active
    monkeypatch.setattr(pt, "_active_defines_for_lib", lambda lib: frozenset())
    other = tmp_path / "other.c"
    other.write_text("void h(void){ return; }\n")
    assert pt.gbi_value_guard_needs_define(str(other), "libultra") is None
    pt._gbi_guarded_macros.cache_clear()


def test_header_renames_symbol(tmp_path, monkeypatch):
    """S85 retro #1: a (transitively-)vendored header function-like macro that rewrites the curated
    symbol (`os_host.h`: `#define __osInitialize_common() osInitialize()`, the K->J shim) is
    pre-flagged so the `#undef <fn>` enabler is priced at the gate. Verifies transitive reach,
    exact-name (`\\b`) matching, and no false-flag for a sibling the shim does not rename."""
    pt = load_tool("pick_target")
    inc = tmp_path / "inc"
    inc.mkdir()
    # transitive chain: candidate.c -> os_internal.h -> os_host.h (the renaming shim)
    (inc / "os_host.h").write_text("#define __osInitialize_common() osInitialize()\n")
    (inc / "os_internal.h").write_text('#include "os_host.h"\n')
    cand = tmp_path / "initialize.c"
    cand.write_text('#include "os_internal.h"\nvoid __osInitialize_common(){ }\n')
    monkeypatch.setattr(pt, "INCLUDE_DIRS", [str(inc)])

    # the curated leader is renamed by a transitively-included header → flagged with that header
    assert pt.header_renames_symbol(str(cand), "__osInitialize_common") == "os_host.h"
    # a sibling the shim does NOT rename → no flag
    assert pt.header_renames_symbol(str(cand), "create_speed_param") is None
    # exact-name boundary: a shorter name sharing the prefix must not match (\b after the name)
    assert pt.header_renames_symbol(str(cand), "__osInitialize") is None
    # a candidate whose headers do not resolve / do not rename → no flag
    other = tmp_path / "other.c"
    other.write_text('#include "os_internal.h"\nvoid g(){ }\n')
    assert pt.header_renames_symbol(str(other), "g") is None


def test_wrong_ghidra_name_override(tmp_path, monkeypatch):
    """S102: a vendored header function-like macro that renames the curated symbol to a DIFFERENT
    symbol the upstream actually defines (os_motor.h `#define osMotorStop(x) __osMotorAccess(...)`,
    while motor.c defines `__osMotorAccess`, NOT `osMotorStop`) is a header-renames-symbol FALSE
    fire — the body never contains the `osMotorStop` token, so no `#undef` is needed.
    _upstream_defines_function distinguishes it from the S85 source-compat rename (body DOES define
    the macro name); _macro_alias_target names the correction target for the wrong-ghidra-name tag."""
    pt = load_tool("pick_target")
    inc = tmp_path / "inc"
    inc.mkdir()
    (inc / "os_motor.h").write_text(
        "#define osMotorStop(x) __osMotorAccess((x), 0)\n"
        "extern s32 __osMotorAccess(OSPfs *, s32);\n"
    )
    cand = tmp_path / "motor.c"
    cand.write_text(
        '#include "os_motor.h"\n'
        "s32 __osMotorAccess(OSPfs* pfs, s32 flag) { return flag; }\n"
        "s32 osMotorInit(OSMesgQueue* mq, OSPfs* pfs, int ch) { return 0; }\n"
    )
    monkeypatch.setattr(pt, "INCLUDE_DIRS", [str(inc)])

    # the header macro-renames osMotorStop ...
    assert pt.header_renames_symbol(str(cand), "osMotorStop") == "os_motor.h"
    # ... but the body defines __osMotorAccess (the macro RHS), NOT osMotorStop → S102 false fire
    assert pt._upstream_defines_function(str(cand), "osMotorStop") is False
    assert pt._upstream_defines_function(str(cand), "__osMotorAccess") is True
    # a symbol the body genuinely defines (the S85 real-rename contrast) → True
    assert pt._upstream_defines_function(str(cand), "osMotorInit") is True
    # the correction target is the macro's RHS leading identifier
    assert pt._macro_alias_target(str(cand), "osMotorStop") == "__osMotorAccess"
    # a name with no aliasing macro → no target
    assert pt._macro_alias_target(str(cand), "osMotorInit") is None


def test_pick_target_json_golden(golden_dir, regen, monkeypatch):
    # The S71 coddog map (tools/coddog/coddog_map.tsv) is gitignored + build-dependent, so the
    # committed golden is map-free; neutralize any local map for a reproducible snapshot.
    monkeypatch.setenv("CODDOG_MAP", "/nonexistent/coddog_map.tsv")
    proc = run_tool("pick_target", "--json", "-n", ROWS)
    assert proc.returncode == 0, proc.stderr
    rows = json.loads(proc.stdout)
    assert isinstance(rows, list) and rows, (
        "expected a non-empty list of candidate rows"
    )

    gpath = golden_dir / "pick_target.json"
    if regen or not gpath.exists():
        gpath.write_text(json.dumps(rows, indent=2, sort_keys=True) + "\n")
        pytest.skip(f"golden regenerated: {gpath.name}")
    expected = json.loads(gpath.read_text())
    assert rows == expected


def test_pick_target_deep_json_golden(golden_dir, regen, monkeypatch):
    """Deeper-row companion to the -n 50 golden: pin ALL rows (the backlog is ~108), so the long
    tail of hazard detectors that only fire on rows 51+ is byte-pinned, not just spot-asserted.
    Coddog stays off here (base behavior); the coddog path is pinned by the coddog golden below."""
    monkeypatch.setenv("CODDOG_MAP", "/nonexistent/coddog_map.tsv")
    proc = run_tool("pick_target", "--json", "-n", "200")
    assert proc.returncode == 0, proc.stderr
    rows = json.loads(proc.stdout)
    assert isinstance(rows, list) and len(rows) > 50, (
        "expected the full backlog, deeper than -n 50"
    )

    gpath = golden_dir / "pick_target_deep.json"
    if regen or not gpath.exists():
        gpath.write_text(json.dumps(rows, indent=2, sort_keys=True) + "\n")
        pytest.skip(f"golden regenerated: {gpath.name}")
    assert rows == json.loads(gpath.read_text())


def test_pick_target_coddog_json_golden(golden_dir, regen, monkeypatch):
    """Coddog-ENABLED golden: the -n 50 / deep goldens run coddog OFF (the live map is gitignored +
    build-dependent), so the coddog re-ranking path — coddog-mirror flagging, the non-audio
    re-pricing, the audio advisory-only branch, and the `@`/`.split` detail parsing — was never
    byte-pinned. Drive pick_target with a COMMITTED synthetic map (golden/coddog_map_fixture.tsv,
    the proven func_800A0730 non-audio + func_800A09E0 audio entries) and snapshot, so the coddog
    detail strings are a stable contract. This is the gate for Phase-A Hazard.coddog_* methods +
    the cluster-7 (coddog) extraction."""
    fixture = golden_dir / "coddog_map_fixture.tsv"
    monkeypatch.setenv("CODDOG_MAP", str(fixture))
    proc = run_tool("pick_target", "--json", "-n", "200")
    assert proc.returncode == 0, proc.stderr
    rows = json.loads(proc.stdout)
    # sanity: the fixture's non-audio hit re-prices to libultra, the audio hit stays none-but-flagged
    by_func = {r["func"]: r for r in rows}
    assert "coddog-mirror:src/io/fake.c@99.99" in by_func["func_800A0730"]["hazards"]
    assert "coddog-mirror:src/audio/fake.c@99.99" in by_func["func_800A09E0"]["hazards"]

    gpath = golden_dir / "pick_target_coddog.json"
    if regen or not gpath.exists():
        gpath.write_text(json.dumps(rows, indent=2, sort_keys=True) + "\n")
        pytest.skip(f"golden regenerated: {gpath.name}")
    assert rows == json.loads(gpath.read_text())


def test_pick_target_ranked_by_descending_score(golden_dir, regen, monkeypatch):
    """Lock the ranking contract: rows are ordered by `score`, highest first.

    (`score` folds size + bonuses/penalties; the smallest-first intent lives
    inside the score, not in a raw size sort.)
    """
    monkeypatch.setenv("CODDOG_MAP", "/nonexistent/coddog_map.tsv")
    proc = run_tool("pick_target", "--json", "-n", ROWS)
    rows = json.loads(proc.stdout)
    scores = [r["score"] for r in rows]
    assert scores == sorted(scores, reverse=True), (
        "rows must be ranked by descending score"
    )


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
    rows = {
        r["func"]: r
        for r in json.loads(run_tool("pick_target", "--json", "-n", "200").stdout)
    }

    hit = rows.get("func_800A0730")
    assert hit is not None and "coddog-mirror:src/io/fake.c@99.99" in hit["hazards"]
    assert (
        hit["upstream"] == "libultra" and hit["pts"] < 13
    )  # re-priced off the classical-pack 13

    audio = rows.get(
        "func_800A09E0"
    )  # audio hit: flagged but NOT re-priced (header-gated)
    assert (
        audio is not None and "coddog-mirror:src/audio/fake.c@99.99" in audio["hazards"]
    )
    assert audio["upstream"] == "none"


def test_lib_filter_surfaces_audio_coddog_mirror(tmp_path, monkeypatch):
    """S94: `--lib <scope>` surfaces a coddog-mirror row whose matched source is in-scope, even when
    the row stayed `upstream none` (audio coddog hits are flagged but NOT re-priced -> none, since
    they were header-`-I`-gated, S71). Before this fix a clean audio mirror like auxbus.c was
    invisible under `--lib libultra` (no `libultra/` path qualifier, no curated name) so the gate had
    to survey by the coddog column (the S55 "--lib is misleading" caveat). The coddog map IS the
    ultralib sweep, so any coddog-mirror match -> libultra; a sub-path scope (`--lib audio`) matches
    via the matched-source path substring."""
    mapf = tmp_path / "coddog_map.tsv"
    mapf.write_text("func_800A09E0\t__alFakeAudio\tsrc/audio/fake.c\t99.99\n")
    monkeypatch.setenv("CODDOG_MAP", str(mapf))

    # --lib libultra: the audio coddog row (upstream none) is now in scope.
    ultra = {
        r["func"]
        for r in json.loads(
            run_tool("pick_target", "--lib", "libultra", "--json", "-n", "400").stdout
        )
    }
    assert "func_800A09E0" in ultra, "audio coddog row missing under --lib libultra"

    # --lib audio: the same row surfaces via the matched-source path substring.
    aud = {
        r["func"]
        for r in json.loads(
            run_tool("pick_target", "--lib", "audio", "--json", "-n", "400").stdout
        )
    }
    assert "func_800A09E0" in aud, "audio coddog row missing under --lib audio"

    # Guard: a scope matching no path/name/up_lib/coddog-source still excludes the row (the filter
    # is widened for in-scope coddog hits only, not disabled).
    none = {
        r["func"]
        for r in json.loads(
            run_tool("pick_target", "--lib", "zzznomatch", "--json", "-n", "400").stdout
        )
    }
    assert "func_800A09E0" not in none


def test_coddog_trap_rescan(tmp_path, monkeypatch):
    """S72: a coddog-matched upstream's trap detectors (defines-data / file-static / needs-header)
    re-run on the resolved `.c`, so a verbatim-mirror candidate's hidden BSS/data trap is priced at
    the gate, not discovered mid-sprint. Map a none-classified candidate to a REAL ultralib file
    with the trap — src/io/piacs.c DEFINES `__osPiAccessQueueEnabled` + a `static OSMesg piAccessBuf`
    — and assert both hazards surface (and the seed re-prices off the bare-coddog clean look)."""
    mapf = tmp_path / "coddog_map.tsv"
    mapf.write_text("func_800A0730\t__osPiCreateAccessQueue\tsrc/io/piacs.c\t99.99\n")
    monkeypatch.setenv("CODDOG_MAP", str(mapf))
    rows = {
        r["func"]: r
        for r in json.loads(run_tool("pick_target", "--json", "-n", "200").stdout)
    }

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
    base = {
        r["func"]: r
        for r in json.loads(run_tool("pick_target", "--json", "-n", "400").stdout)
    }
    assert "maybe-upstream" in base["func_800A07B0"]["hazards"], (
        "baseline expects the IDF guess"
    )

    # Definitive non-audio coddog hit -> maybe-upstream suppressed; coddog-mirror stands alone.
    mapf = tmp_path / "coddog_map.tsv"
    mapf.write_text("func_800A07B0\t__osFakeMirror\tsrc/io/fake.c\t99.99\n")
    monkeypatch.setenv("CODDOG_MAP", str(mapf))
    hit = {
        r["func"]: r
        for r in json.loads(run_tool("pick_target", "--json", "-n", "400").stdout)
    }["func_800A07B0"]
    assert "coddog-mirror:src/io/fake.c@99.99" in hit["hazards"]
    assert "maybe-upstream" not in hit["hazards"], hit["hazards"]

    # Audio coddog hit is advisory-only (header-gated, not re-priced) -> the guess is retained.
    mapf.write_text("func_800A07B0\t__alFakeAudio\tsrc/audio/fake.c\t99.99\n")
    monkeypatch.setenv("CODDOG_MAP", str(mapf))
    aud = {
        r["func"]: r
        for r in json.loads(run_tool("pick_target", "--json", "-n", "400").stdout)
    }["func_800A07B0"]
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
    pt.src_func_callers.cache_clear()
    callers = pt.src_func_callers()
    assert callers.get("func_800B16A0") == ["src/main/caller.c"], callers
    assert "func_deadbeef" not in callers and "func_DEADBEEF" not in callers
    pt.src_func_callers.cache_clear()  # don't leak the tmp-ROOT walk into later tests


def _coddog_rows(pt, monkeypatch, *, ui, carried, coddog):
    """Drive build_rows over ONE synthetic 2-fn asm subseg with the file-reading helpers stubbed,
    so the new coddog-tail logic is tested in isolation (no tree deps). Leader is `namedLeader`
    (named, in ui+carried); tail is `func_1100`."""

    class A:
        lib = None
        include_stuck = False
        n = 999

    monkeypatch.setattr(
        pt, "parse_subsegs", lambda: [(0x1000, "asm", None), (0x2000, "asm", None)]
    )
    monkeypatch.setattr(
        pt,
        "classify_subseg",
        lambda off, typ, path, size, _ui: (
            ("asm-flip", ["namedLeader", "func_1100"], []) if off == 0x1000 else None
        ),
    )
    monkeypatch.setattr(pt, "src_func_callers", lambda: {})
    monkeypatch.setattr(pt, "append_upstream_hazards", lambda *a, **k: ("warm", False))
    monkeypatch.setattr(
        pt,
        "score_row",
        lambda cand, hz, carr: {
            "func": cand.primary,
            "rom": hex(cand.off),
            "upstream": cand.up_lib,
            "score": 0,
            "size": cand.size,
            "hazards": ",".join(
                h.detail for h in hz if h.kind == pt.HAZARD_CODDOG_MIRROR
            ),
        },
    )
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
        pt,
        monkeypatch,
        ui={
            "namedLeader": ("libultra", "/x/wrong.c")
        },  # mis-attributed (prototype) upstream
        carried={"namedLeader"},  # name-dropped in the digest log
        coddog={"func_1100": ("src/io/pfsisplug.c", 99.99)},
    )
    got = [r for r in rows if r["func"] == "namedLeader"]
    assert got, "a coddog-identified subseg must survive the carried name-drop filter"
    assert "src/io/pfsisplug.c@99.99" in got[0]["hazards"], got[0]["hazards"]
    assert got[0]["upstream"] == "libultra"


def test_carried_namedrop_still_drops_without_coddog(monkeypatch):
    """The carried filter still de-ranks a genuinely-carried leader when NO member is coddog'd — the
    S78 exemption is scoped strictly to definitive coddog identities, not a blanket disable."""
    pt = load_tool("pick_target")
    rows = _coddog_rows(
        pt,
        monkeypatch,
        ui={"namedLeader": ("libultra", "/x/wrong.c")},
        carried={"namedLeader"},
        coddog={},
    )  # no coddog identity -> the name-drop drop still applies
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
        "typedef struct {\n    int rxsize;\n} __OSContRequesFormatShort;\n"
    )
    monkeypatch.setattr(pt, "INCLUDE_DIRS", [str(inc), str(inc / "internal")])
    # exact prefixed path misses; basename `controller.h` resolves under internal/
    assert pt._resolve_include("PRinternal/controller.h") == str(
        inc / "internal" / "controller.h"
    )
    # a bare basename absent everywhere still returns None (no false resolution)
    assert pt._resolve_include("nope.h") is None
    cfile = tmp_path / "src.c"
    cfile.write_text(
        '#include "PRinternal/controller.h"\n'
        "void f(void){ __OSContRequesFormatShort t; }\n"
    )
    assert "__OSContRequesFormatShort" in pt.declared_type_names(str(cfile))
    # ...so refs_unplaced excludes it (a TYPE, not an unplaced data extern)
    assert "__OSContRequesFormatShort" not in pt.refs_unplaced(
        str(cfile), set(), "libultra"
    )


def test_refs_unplaced_drops_initializer_only_self_defined_global(tmp_path):
    """S86 #1: a `__`-global the .c itself DEFINES but references ONLY inside another global's
    depth-0 initializer is drop-def-resolved — drop-def deletes that initializer, so it owes no
    symbol_addrs recovery. timerintr.c's `__osBaseTimer` (named only in
    `OSTimer* __osTimerList = &__osBaseTimer;`) must NOT be refs_unplaced, while `__osViIntrCount`
    (defined here AND assigned in a function body) MUST stay flagged."""
    pt = load_tool("pick_target")
    cfile = tmp_path / "timerintr.c"
    cfile.write_text(
        "u32 __osViIntrCount;\n"
        "OSTimer __osBaseTimer;\n"
        "OSTimer* __osTimerList = &__osBaseTimer;\n"
        "void __osTimerServicesInit(void) {\n"
        "    __osViIntrCount = 0;\n"
        "    __osTimerList->next = __osTimerList;\n"
        "}\n"
    )
    refs = pt.refs_unplaced(str(cfile), set(), "libultra")
    assert (
        "__osBaseTimer" not in refs
    )  # initializer-only self-def → drop-def removes it
    assert "__osViIntrCount" in refs  # body-referenced → still owed a recovery
    assert (
        "__osTimerList" in refs
    )  # body-referenced (->next) → still owed (if unplaced)


def test_call_divergence_strips_inactive_version_branch(monkeypatch):
    """S80 #2: call_divergence drops the inactive `#if BUILD_VERSION` side before counting C calls,
    so an `#else` branch's call does not double-count against the active branch's (pfsgetstatus's
    non-J `__osPfsRequestOneChannel(channel)` inflated 6→7, a phantom `7vs6` on a byte-clean mirror).
    Without the lib's build_ord (None → 0) the strip is a no-op and the phantom returns."""
    pt = load_tool("pick_target")
    # _upstream_body returns the brace body (no signature line), so name only real call sites.
    body = (
        "#if BUILD_VERSION >= VERSION_J\n"
        "    foo(a, b);\n"
        "#else\n"
        "    foo(a);\n"  # dead under J — must NOT count
        "#endif\n"
        "    bar();\n"
    )
    monkeypatch.setattr(pt, "_upstream_body", lambda cp, pr: body)
    monkeypatch.setattr(pt, "_asm_jal_count", lambda off, pr: 2)  # J build: foo + bar
    monkeypatch.setattr(
        pt,
        "_build_version_ord",
        lambda lib: load_tool("build_config")._VERSION_ORD["VERSION_J"] if lib else 0,
    )
    assert (
        pt.call_divergence(0x1000, "ff", "x.c", "libultra") is None
    )  # strip → 2vs2 → no hazard
    d = pt.call_divergence(0x1000, "ff", "x.c", None)  # no strip → foo doubles → 3vs2
    assert d is not None and d.detail == "3vs2"


def test_call_divergence_libnusys_intmask_version_artifact(monkeypatch):
    """S118 #2: a libnusys jal-count-mismatch whose surplus C calls are exactly the nusys-2.05+
    `osSetIntMask` body wrapper (absent from the pre-2.05 leaf asm) is annotated `(version-artifact?)`
    so smallest-first is not deterred from a clean near-verbatim drop (nuContRmbModeSet `2vs0`). The
    flag is libnusys-only and requires the surplus (n_c - n_asm) to equal the osSetIntMask count."""
    pt = load_tool("pick_target")
    body = (
        "    OSIntMask mask;\n"
        "    mask = osSetIntMask(OS_IM_NONE);\n"
        "    foo(a);\n"
        "    osSetIntMask(mask);\n"
    )  # 3 calls C-side; leaf asm omits the 2 wrapper calls
    monkeypatch.setattr(pt, "_upstream_body", lambda cp, pr: body)
    monkeypatch.setattr(
        pt, "_asm_jal_count", lambda off, pr: 1
    )  # leaf-ish: only foo survives
    monkeypatch.setattr(pt, "_build_version_ord", lambda lib: 0)
    d = pt.call_divergence(0x1000, "ff", "x.c", "libnusys")
    assert d is not None and d.detail == "3vs1(version-artifact?)"
    # non-libnusys: same body, no version annotation (the wrapper tell is nusys-family specific)
    d2 = pt.call_divergence(0x1000, "ff", "x.c", "libultra")
    assert d2 is not None and d2.detail == "3vs1"


def test_block_reorder_sibling_libnusys_family():
    """S120 #1: a libnusys candidate carrying the block-reorder tell — an UNexplained jal-mismatch +
    NO coddog-mirror (the reorder breaks coddog's fingerprint) — whose upstream basename is in a
    BLOCK_REORDER_FAMILIES prefix names the banked sibling whose CheckConnector/RAM-enable swap
    replays, so the gate applies it up-front. A coddog match, a `(version-artifact?)` clean-drop, a
    non-libnusys lib, or an out-of-family file all suppress; the registered sibling never self-flags."""
    pt = load_tool("pick_target")
    H = load_tool("pick_target_hazards").Hazard
    jal = [H.jal_count_mismatch(5, 10)]  # the unexplained-mismatch tell
    assert (
        pt._block_reorder_sibling("a/nucontgbpakfwrite.c", jal, "libnusys")
        == "nucontgbpakfread.c"
    )
    assert (
        pt._block_reorder_sibling("a/nucontgbpakfread.c", jal, "libnusys") is None
    )  # no self-flag
    assert (
        pt._block_reorder_sibling(  # coddog match explains the structure → suppress
            "a/nucontgbpakfwrite.c", jal + [H.coddog_mirror("x.c", 99.0)], "libnusys"
        )
        is None
    )
    va = [
        H.jal_count_mismatch(2, 0, version_artifact=True)
    ]  # clean-drop, not the reorder tell
    assert pt._block_reorder_sibling("a/nucontgbpakfwrite.c", va, "libnusys") is None
    assert (
        pt._block_reorder_sibling("a/nucontgbpakfwrite.c", jal, "libultra") is None
    )  # wrong lib
    assert (
        pt._block_reorder_sibling("a/nusched.c", jal, "libnusys") is None
    )  # out of family
    assert (
        pt._block_reorder_sibling("a/nucontgbpakfwrite.c", [], "libnusys") is None
    )  # no mismatch


def test_straddling_unattrib_inter_file_leaf():
    """S120 #2: _straddling_unattrib returns the vrams of `?` members whose nearest named-C members
    before AND after resolve to DIFFERENT stems (an inter-file leaf the split must place) — and only
    those. Models the pre-split S120 [0x7D970] pack: Fwrite(A), func_800A2780(?), nuSiMgrInit(B),
    nuSiSendMesg(B), then 3 trailing `?` within B. Leading and same-stem-flanked `?` do NOT straddle."""
    pt = load_tool("pick_target")
    fns = [
        "nuContGBPakFwrite",
        "func_800A2780",
        "nuSiMgrInit",
        "nuSiSendMesg",
        "func_800A2888",
        "func_800A28A8",
        "func_800A28C8",
    ]
    stems = ["nucontgbpakfwrite", None, "nusimgr", "nusimgr", None, None, None]
    unattrib = {"func_800A2780", "func_800A2888", "func_800A28A8", "func_800A28C8"}
    name_vram = {nm: 0x800A2570 + i * 0x10 for i, nm in enumerate(fns)}
    assert pt._straddling_unattrib(fns, stems, unattrib, name_vram) == [
        name_vram["func_800A2780"]
    ]
    # a LEADING `?` (no named member before it) does NOT straddle
    assert (
        pt._straddling_unattrib(
            ["func_x", "alSynNew", "alSynDelete"],
            [None, "synthesizer", "syndelete"],
            {"func_x"},
            {"func_x": 1, "alSynNew": 2, "alSynDelete": 3},
        )
        == []
    )
    # a `?` flanked by the SAME stem both sides (within one file) does NOT straddle
    assert (
        pt._straddling_unattrib(
            ["a", "func_y", "b"],
            ["nusimgr", None, "nusimgr"],
            {"func_y"},
            {"a": 1, "func_y": 2, "b": 3},
        )
        == []
    )


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
    monkeypatch.setattr(
        pt,
        "classify_subseg",
        lambda off, typ, path, size, _ui: (
            "asm-flip",
            ["namedLeader", "func_1100"],
            [],
        ),
    )
    monkeypatch.setattr(pt, "src_func_callers", lambda: {})
    monkeypatch.setattr(pt, "append_upstream_hazards", lambda *a, **k: ("warm", False))
    # asm-reading helpers: off=0x1000 is synthetic, return empty so only the upstream .c is read.
    monkeypatch.setattr(pt, "recover_unplaced_vram", lambda off: [])
    monkeypatch.setattr(pt, "recover_unplaced_call_vram", lambda off, primary: [])
    monkeypatch.setattr(pt, "rodata_jtbls", lambda off: [])
    monkeypatch.setattr(
        pt,
        "score_row",
        lambda cand, hz, carr: {
            "func": cand.primary,
            "upstream": cand.up_lib,
            "score": 0,
            "size": cand.size,
            "hazards": ",".join(
                h.kind + (":" + h.detail if h.detail else "") for h in hz
            ),
        },
    )
    rows = pt.build_rows(
        A(),
        {"namedLeader": ("libultra", "/x/wrong.c")},
        set(),
        {},
        {"func_1100": ("src/io/piacs.c", 99.99)},
    )
    hit = next(r for r in rows if r["func"] == "namedLeader")
    assert "coddog-mirror:src/io/piacs.c@99.99" in hit["hazards"]
    assert "file-static" in hit["hazards"], hit["hazards"]
    assert "defines-data:__osPiAccessQueueEnabled" in hit["hazards"], hit["hazards"]


def _hazard_rows(pt, monkeypatch, *, subs, classify, ui, coddog):
    """Drive build_rows over synthetic subsegs with file-reading helpers stubbed; score_row emits
    every hazard kind:detail so the assertion can match the advisory flags. Shared by the S103
    coddog-partial + game-region-mirror tests."""

    class A:
        lib = None
        include_stuck = False
        n = 999

    monkeypatch.setattr(pt, "parse_subsegs", lambda: subs)
    monkeypatch.setattr(pt, "classify_subseg", classify)
    monkeypatch.setattr(pt, "src_func_callers", lambda: {})
    monkeypatch.setattr(pt, "append_upstream_hazards", lambda *a, **k: ("warm", False))
    monkeypatch.setattr(
        pt, "_coddog_upstream_path", lambda cfile: None
    )  # skip trap-rescan/fncount file reads
    monkeypatch.setattr(pt, "subseg_vram", lambda off: 0x80067B00)
    monkeypatch.setattr(
        pt,
        "score_row",
        lambda cand, hz, carr: {
            "func": cand.primary,
            "upstream": cand.up_lib,
            "score": 0,
            "size": cand.size,
            "hazards": ",".join(
                h.kind + (":" + h.detail if h.detail else "") for h in hz
            ),
        },
    )
    return pt.build_rows(A(), ui, set(), {}, coddog)


def test_coddog_partial_twin_subset(monkeypatch):
    """S103: coddog matched 2 DISTINCT per-fn TWIN files (mtxidentf.c + mtxl2f.c) to a multi-fn pack,
    covering only a SUBSET of its fns → a per-fn fingerprint set, NOT a single-file mirror (the real
    src is the combined mtxutil.c; guMtxF2L (clamp) + guMtxIdent (inline) diverged). Flag coddog-partial
    so the gate per-fn-verifies. The multi-twin companion to the len(distinct)==1-only fncount-mismatch."""
    pt = load_tool("pick_target")
    rows = _hazard_rows(
        pt,
        monkeypatch,
        subs=[(0x1000, "asm", None)],
        classify=lambda off, typ, path, size, _ui: (
            "asm-flip",
            ["leader", "guMtxL2F", "guMtxIdentF"],
            [],
        ),
        ui={
            "leader": ("libultra", "/x/wrong.c"),  # named leader, distinct upstream
            "guMtxL2F": (
                "libultra",
                "/x/mtxutil.c",
            ),  # members named -> combined mtxutil
            "guMtxIdentF": ("libultra", "/x/mtxutil.c"),
        },
        coddog={
            "guMtxL2F": ("src/mgu/mtxl2f.c", 100.0),  # 2 distinct per-fn twins
            "guMtxIdentF": ("src/mgu/mtxidentf.c", 100.0),
        },
    )
    hz = next(r for r in rows if r["func"] == "leader")["hazards"]
    assert "coddog-partial:2of3fn" in hz, hz  # 2 coddog'd of 3 pack fns
    assert "coddog-twin:" in hz  # the twins that drove the partial flag


def test_game_region_mirror_below_libultra_band(monkeypatch):
    """S103: a libultra-source mirror whose rom is BELOW the libultra code band is game-linked at -O2,
    not -O3 — flag game-region-mirror so the gate routes it to a -O2 path (src/mgu/), not src/libultra/
    (which forces -O3 → wrong inlining). Band start = lowest-rom `libultra/` subseg (0x9000 here)."""
    pt = load_tool("pick_target")
    rows = _hazard_rows(
        pt,
        monkeypatch,
        subs=[
            (0x1000, "asm", None),
            (0x9000, "c", "libultra/io/pimgr"),
        ],  # band starts at 0x9000
        classify=lambda off, typ, path, size, _ui: (
            ("asm-flip", ["leader"], []) if off == 0x1000 else None
        ),
        ui={
            "leader": ("libultra", "/x/gu/mtxutil.c")
        },  # libultra source, but rom 0x1000 < 0x9000
        coddog={},
    )
    hz = next(r for r in rows if r["func"] == "leader")["hazards"]
    assert "game-region-mirror:0x80067B00" in hz, hz
    # a libultra subseg ABOVE the band must NOT be flagged: re-run with the source row at 0xA000
    rows2 = _hazard_rows(
        pt,
        monkeypatch,
        subs=[
            (0x9000, "c", "libultra/io/pimgr"),
            (0xA000, "asm", None),
        ],  # band starts at 0x9000
        classify=lambda off, typ, path, size, _ui: (
            ("asm-flip", ["leader"], []) if off == 0xA000 else None
        ),
        ui={"leader": ("libultra", "/x/gu/mtxutil.c")},
        coddog={},
    )
    hz2 = next(r for r in rows2 if r["func"] == "leader")["hazards"]
    assert "game-region-mirror" not in hz2, hz2  # 0xA000 > band start -> no flag


def test_coddog_nusys_repricing(monkeypatch):
    """The libnusys analog of the libultra coddog re-price (build_coddog_nusys_index / nusys_sweep.sh):
    an un-named subseg whose fn coddog matched (>=PCT) to a nusys mainlib source is flagged
    coddog-mirror:mainlib/<file>.c and re-priced upstream=libnusys, so seed_points drops it off the
    classical path. A SEPARATE additive pass keyed on the nusys_index, so an absent nusys map
    (nusys_index={}) is a no-op — the guarantee the goldens' autouse neutralization relies on."""
    pt = load_tool("pick_target")

    class A:
        lib = None
        include_stuck = False
        n = 999

    monkeypatch.setattr(pt, "parse_subsegs", lambda: [(0x1000, "asm", None)])
    monkeypatch.setattr(
        pt,
        "classify_subseg",
        lambda off, typ, path, size, _ui: ("asm-flip", ["func_1000"], []),
    )
    monkeypatch.setattr(pt, "src_func_callers", lambda: {})
    monkeypatch.setattr(pt, "append_upstream_hazards", lambda *a, **k: ("warm", False))
    monkeypatch.setattr(
        pt, "_coddog_nusys_path", lambda cfile: None
    )  # skip trap-rescan file read
    monkeypatch.setattr(
        pt, "_coddog_source_banked", lambda s: False
    )  # tree-independent
    monkeypatch.setattr(
        pt, "signature_hint", lambda off, primary, si: None
    )  # no libultra IDF guess
    monkeypatch.setattr(pt, "subseg_vram", lambda off: 0x800A0000)
    monkeypatch.setattr(
        pt,
        "score_row",
        lambda cand, hz, carr: {
            "func": cand.primary,
            "upstream": cand.up_lib,
            "score": 0,
            "size": cand.size,
            "hazards": ",".join(
                h.kind + (":" + h.detail if h.detail else "") for h in hz
            ),
        },
    )
    # un-named func_1000 coddog-matched a nusys source -> re-price libnusys + flag.
    rows = pt.build_rows(
        A(), {}, set(), {}, {}, {"func_1000": ("mainlib/nusimgr.c", 99.99)}
    )
    row = next(r for r in rows if r["func"] == "func_1000")
    assert row["upstream"] == "libnusys", row
    assert "coddog-mirror:mainlib/nusimgr.c@99.99" in row["hazards"], row["hazards"]
    # absent nusys map -> no-op: stays unclassified (the libultra path is untouched).
    rows0 = pt.build_rows(A(), {}, set(), {}, {}, {})
    assert next(r for r in rows0 if r["func"] == "func_1000")["upstream"] is None


def test_drop_static_mirror_hazard(tmp_path):
    """S87 retro #3: a coddog-confirmed verbatim mirror whose data-shaping hazards are the pure-.bss
    family (file-static + defines-data + refs-unplaced, NO carve signal) is re-framed as ONE
    drop-to-extern enabler — not a carve/classical spike (the false-flag the vimgr carry-over carried).
    Gated by: a >=PCT non-audio coddog identity AND a file-static AND no rodata-literal/data-static/
    rodata-jtbl. Count = file-scope static lines + defines-data globals (a func-local static is a known
    under-count, recovered at the gate)."""
    pt = load_tool("pick_target")
    H = pt.Hazard
    cod = H(pt.HAZARD_CODDOG_MIRROR, "src/io/vimgr.c@99.99")
    fs = H(pt.HAZARD_FILE_STATIC)
    dd = H(pt.HAZARD_DEFINES_DATA, "__osViDevMgr")

    # the pure-.bss cluster → tagged (no path → bare `bss`, no count)
    got = pt.drop_static_mirror_hazard(None, [cod, fs, dd])
    assert got is not None and got.render() == "drop-static-mirror:bss"

    # a real upstream path → a count (2 file-scope statics + 1 global = 3bss)
    up = tmp_path / "vimgr.c"
    up.write_text(
        "OSDevMgr __osViDevMgr = { 0 };\n"
        "static OSThread viThread;\n"
        "static OSMesgQueue viEventQueue;\n"
        "void osCreateViManager(int p){ (void)viThread; }\n"
    )
    got = pt.drop_static_mirror_hazard(str(up), [cod, fs, dd])
    assert got is not None and got.detail == "3bss", got

    # a carve signal (rodata-jtbl / data-static / rodata-literal) disqualifies the pure-.bss drop
    for carve in (
        pt.HAZARD_RODATA_JTBL,
        pt.HAZARD_DATA_STATIC,
        pt.HAZARD_RODATA_LITERAL,
    ):
        assert (
            pt.drop_static_mirror_hazard(None, [cod, fs, dd, H(carve, "0x800D0000")])
            is None
        )

    # no file-static (a plain drop-def mirror) → not this tag
    assert pt.drop_static_mirror_hazard(None, [cod, dd]) is None
    # an audio or sub-threshold coddog hit is advisory, not a confirmed mirror → no tag
    assert (
        pt.drop_static_mirror_hazard(
            None, [H(pt.HAZARD_CODDOG_MIRROR, "src/audio/x.c@99.99"), fs]
        )
        is None
    )
    assert (
        pt.drop_static_mirror_hazard(
            None, [H(pt.HAZARD_CODDOG_MIRROR, "src/io/x.c@88.00"), fs]
        )
        is None
    )
    # no coddog identity at all → no tag (the file-static still routes via the classical/carve playbook)
    assert pt.drop_static_mirror_hazard(None, [fs, dd]) is None


def test_file_static_const_array_widens_rodata_carve_start(tmp_path):
    """S93: the rodata-literal carve-start under-states a `.rodata` block that OPENS with a file-scope
    `static const <type> name[]` array — the FP scan sees only scalar `ldc1/lwc1 %lo` loads, missing
    the array base (`addiu %lo` address-of) + string-literal pointers, so min(rodata_lits) is 0x50 B
    too high (xldtob.c: pows[] dlabel 0x800D27D0 vs the FP scan's 0x800D2820). defines_file_static_const_array
    detects the file-PRIVATE const array that makes widening the carve-start to the subseg boundary
    FP-safe (the `static` keyword bars cross-file linkage — the whole code-seg rodata subseg is this
    object's own; the direct `addiu %lo` scan was reverted S92 for cross-file FP in .data)."""
    pt = load_tool("pick_target")

    # a file-scope `static const` ARRAY → detected (the widening signal)
    up = tmp_path / "xldtob.c"
    up.write_text(
        '#include "stdlib.h"\n'
        "static const ldouble pows[] = {10e0L, 10e1L, 10e3L};\n"
        "void _Ldtob(int code) { (void)pows[code]; }\n"
    )
    assert pt.defines_file_static_const_array(str(up)) == ["pows"]

    # a NON-const mutable array (xlitob's ldigs/udigs) → NOT detected: it lands in .data (its own
    # carve), so the rodata widening must not fire (the S92 reversion's over-fire class).
    mutable = tmp_path / "xlitob.c"
    mutable.write_text(
        'static char ldigs[] = "0123456789abcdef";\n'
        "void _Litob(int code) { (void)ldigs[code]; }\n"
    )
    assert pt.defines_file_static_const_array(str(mutable)) == []

    # a brace-depth>=1 function-local const array is not file-scope → excluded (depth tracking)
    local = tmp_path / "loc.c"
    local.write_text(
        "void f(int i) {\n    static const int t[2] = {1, 2};\n    (void)t[i];\n}\n"
    )
    assert pt.defines_file_static_const_array(str(local)) == []

    # a scalar const (not an array) is not this signal
    scalar = tmp_path / "sc.c"
    scalar.write_text("static const float dtor = 3.14;\nvoid g(void){}\n")
    assert pt.defines_file_static_const_array(str(scalar)) == []


def test_iter_upstream_functions_def_shapes(tmp_path):
    """S104: the depth-aware scanner counts every def shape exactly — skips a proto / `#define` /
    doc-comment, counts a single-token implicit-int K&R def (`_xatan(u,v)`) and a stray-leading-space
    header. The xprintf retro root cause: the old regex over-counted a proto and under-counted these."""
    pt = load_tool("pick_target")
    c = tmp_path / "u.c"
    c.write_text(
        "/* banner: double atan(double) */\n"  # doc-comment signature → NOT a def
        "#define PUT(x) sink(x)\n"  # function-like macro header → NOT a def
        "static void _Putfld(int x);\n"  # forward declaration → NOT a def
        "int _Printf(void* pfn(int), int a) {\n"
        "    if (a) { PUT(a); }\n"  # in-body call/control → NOT a def
        "    return 0;\n"
        "}\n"
        "_xatan(u, v)\n"  # single-token implicit-int K&R def
        "int u, v;\n"
        "{ return u; }\n"
        " void leading(void) { return; }\n"  # stray-leading-space top-level def
        "static void _Putfld(int x) { (void)x; }\n"
    )
    names = [n for n, _ in pt._iter_upstream_functions(c.read_text())]
    assert names == ["_Printf", "_xatan", "leading", "_Putfld"]
    assert pt._upstream_file_func_count("libultra", str(c)) == 4


def test_jal_count_drops_local_macros():
    """S104 #1: a function-like macro DEFINED IN the upstream .c (xprintf's PUT/PAD) is not a jal —
    PUT wraps a `(*pfn)(…)` jalr. _c_jal_count must drop local macros, else a callback-heavy mirror
    reads as a stripped re-impl (the phantom `14vs3`)."""
    pt = load_tool("pick_target")
    body = "{ PUT(a); PUT(b); PAD(c); realcall(d); }"
    assert pt._c_jal_count(body) == 4  # no local-macro set: PUT/PAD counted
    assert (
        pt._c_jal_count(body, {"PUT", "PAD"}) == 1
    )  # local macros dropped → only realcall


def test_calls_unplaced_skips_fn_ptr_param(tmp_path):
    """S104 #1: a function-pointer parameter (`pfn`) invoked via the pointer is a jalr, not a jal to a
    named symbol, so it must not surface as calls-unplaced (the phantom `calls-unplaced:pfn`)."""
    pt = load_tool("pick_target")
    c = tmp_path / "p.c"
    c.write_text(
        "int run(void* pfn(int, char*), int x) {\n    pfn(x, 0);\n    realcall(x);\n}\n"
    )
    got = pt.calls_unplaced(str(c), "run", set(), "libultra")
    assert "pfn" not in got and "realcall" in got


def test_upstream_fncount_mismatch_and_data_carve(tmp_path):
    """S104 #2/#3: a single-stem pack with MORE fns than the upstream defines (a foreign TU bundled in)
    and the .data init-static-array detector (xprintf spaces/zeroes class)."""
    pt = load_tool("pick_target")
    # #3: NON-const initialized file-scope static array → .data carve; const/scalar/local excluded.
    c = tmp_path / "x.c"
    c.write_text(
        'static char spaces[] = "   ";\n'
        "static const char fchar[] = {1, 2};\n"
        "static int scalar = 5;\n"
        "void f(void) { static int loc[2] = {1, 2}; (void)loc; }\n"
    )
    assert pt.defines_file_static_init_array(str(c)) == ["spaces"]
    assert pt.defines_file_static_const_array(str(c)) == [
        "fchar"
    ]  # unchanged: const stays rodata


# --- refactor pre-flight characterization (tooling/refactor-pick-target) -------------------
# These lock CURRENT behavior (captured live, not a spec) ahead of the pick_target.py refactor
# so the seed-weight-table (Phase 1b), the Candidate parameter object (Phase 2), and the
# module split are unit-guarded, not only golden-guarded. See the plan's Phase 0.


def test_pick_target_table_golden(golden_dir, regen, monkeypatch):
    """Lock the HUMAN TABLE output — the format /sprint-plan actually consumes
    (`venv/bin/python3 tools/pick_target.py -n 12`, .claude/commands/sprint-plan.md:47),
    which the --json golden does NOT cover. A diff after refactoring = a regression in the
    gate's input. Map-free for reproducibility, like the json golden."""
    monkeypatch.setenv("CODDOG_MAP", "/nonexistent/coddog_map.tsv")
    proc = run_tool("pick_target", "-n", ROWS)
    assert proc.returncode == 0, proc.stderr
    table = proc.stdout
    assert table.strip(), "expected a non-empty table"
    gpath = golden_dir / "pick_target_table.txt"
    if regen or not gpath.exists():
        gpath.write_text(table)
        pytest.skip(f"golden regenerated: {gpath.name}")
    assert table == gpath.read_text()


def test_seed_points_characterization():
    """Lock seed_points' a-priori story-point seed across the path/enabler matrix BEFORE the
    if-ladder (`:1743-1771`) becomes a HAZARD_SEED_WEIGHT table (Phase 1b)."""
    pt = load_tool("pick_target")
    H = pt.Hazard
    sp = pt.seed_points  # (size, upstream, band, nfns, hazards, blocked) -> seed
    assert sp(100, "none", "-", 1, [], False) == 5  # small single classical
    assert sp(100, "libultra", "warm", 1, [], False) == 1  # warm enabler-free mirror
    assert sp(100, "libultra", "cold", 1, [], False) == 2  # cold mirror
    assert sp(100, "libultra", "warm", 2, [], False) == 2  # + pack
    assert sp(2000, "libultra", "warm", 1, [], False) == 13  # huge -> must decompose
    assert sp(100, "none", "-", 2, [], False) == 13  # classical pack -> 13
    assert sp(100, "none", "-", 1, [], True) == "blk"  # blocked -> un-pickable
    assert sp(100, "libultra", "warm", 1, [H(pt.HAZARD_FILE_STATIC)], False) == 2
    assert (
        sp(100, "libultra", "warm", 1, [H(pt.HAZARD_NEEDS_HEADER, "x.h")], False) == 2
    )
    assert (
        sp(100, "libultra", "warm", 1, [H(pt.HAZARD_REFS_UNPLACED, "a@0x1")], False)
        == 2
    )
    assert sp(100, "libultra", "cold", 4, [], False) == 8  # large pack must decompose
    assert (
        sp(100, "none", "-", 1, [H(pt.HAZARD_FILE_STATIC)], False) == 8
    )  # classical+drop gate
    assert [pt.snap_fib(n) for n in (0, 1, 2, 3, 4, 5, 6, 9)] == [
        1,
        1,
        2,
        3,
        5,
        5,
        8,
        13,
    ]
    # S119: unexplained jal-count-mismatch + NO coddog-mirror = probable near-verbatim → +1 over the
    # mirror floor (the verbatim cp SHA-misses, needs an asm-driven diagnosis + hand-edit).
    JAL = pt.HAZARD_JAL_COUNT_MISMATCH
    assert (
        sp(100, "libnusys", "cold", 1, [H(JAL, "5vs9")], False) == 3
    )  # cold 2 +1 near-verbatim
    # A coddog-mirror structural match explains the row as a clean verbatim mirror → NO bump.
    assert (
        sp(
            100,
            "libnusys",
            "cold",
            1,
            [H(JAL, "5vs9"), H(pt.HAZARD_CODDOG_MIRROR, "x.c@99.99")],
            False,
        )
        == 2
    )
    # The (version-artifact?) annotation is a known clean-drop → NO bump (S118 nuContRmbModeSet).
    assert (
        sp(100, "libnusys", "cold", 1, [H(JAL, "2vs0(version-artifact?)")], False) == 2
    )


def test_score_row_characterization():
    """Lock score_row's dict shape + score arithmetic
    (-size + UPSTREAM_BONUS(200) + BAND_WARM_BONUS(64) - CARRYOVER_PENALTY(1000)) and the
    rendered hazard string, BEFORE Phase 2 collapses its 10 args into a Candidate."""
    pt = load_tool("pick_target")
    H = pt.Hazard
    KEYS = [
        "func",
        "rom",
        "vram",
        "size",
        "nfns",
        "pts",
        "kind",
        "upstream",
        "band",
        "hazards",
        "score",
    ]
    C = pt.Candidate
    r = pt.score_row(
        C(0x99999, "func_x", "asm-flip", ["func_x"], 100, "libultra", "warm", False),
        [H(pt.HAZARD_FILE_STATIC), H(pt.HAZARD_NEEDS_HEADER, "x.h")],
        set(),
    )
    assert list(r.keys()) == KEYS
    assert r["vram"] == "?"  # synthetic off: no asm listing
    assert r["hazards"] == "file-static,needs-header:x.h"
    assert r["score"] == -100 + 200 + 64  # 164: upstream + warm bonuses
    r2 = pt.score_row(
        C(0x99999, "func_y", "asm-flip", ["func_y", "func_z"], 64, None, "-", False),
        [],
        {"func_y"},
    )
    assert r2["hazards"] == "-"  # no hazards renders "-"
    assert r2["score"] == -64 - 1000  # carried penalty, no bonuses
    assert (pt.UPSTREAM_BONUS, pt.BAND_WARM_BONUS, pt.CARRYOVER_PENALTY) == (
        200,
        64,
        1000,
    )


def test_hazard_detail_accessors():
    """Lock the Hazard detail encode/decode now that parse knowledge is centralized on the class
    (Phase A: the `@`/`!=`/`;owner-per-member` ad-hoc splits scattered across build_rows are gone).
    Covers mark_owner_per_member explicitly — it was the one re-parsed surface with no prior test."""
    pt = load_tool("pick_target")
    H = pt.Hazard

    # coddog_mirror classmethod encodes `<file>@<pct>` (2-decimal) and round-trips through accessors.
    h = H.coddog_mirror("src/io/contquery.c", 99.99)
    assert h.render() == "coddog-mirror:src/io/contquery.c@99.99"
    assert h.coddog_file() == "src/io/contquery.c"
    assert h.coddog_pct() == 99.99
    assert (
        h.coddog_source() == "src/io/contquery.c"
    )  # rsplit form, same when no `@` in path
    # malformed pct degrades to 0.0 (the old _coddog_pct contract)
    assert H(pt.HAZARD_CODDOG_MIRROR, "src/io/x.c").coddog_pct() == 0.0

    # twin_file pulls the primary `<stem>` out of `<stem>!=<alt,...>`
    assert H(pt.HAZARD_CODDOG_TWIN, "siacs!=piacs,siacs").twin_file() == "siacs"

    # mark_owner_per_member: idempotent in-place append, None-detail safe
    j = H(pt.HAZARD_RODATA_JTBL, "0x800D23E8")
    j.mark_owner_per_member()
    assert j.detail == "0x800D23E8;owner-per-member"
    j.mark_owner_per_member()  # second call is a no-op
    assert j.detail == "0x800D23E8;owner-per-member"
    n = H(pt.HAZARD_RODATA_LITERAL)
    n.mark_owner_per_member()
    assert n.detail == ";owner-per-member"
