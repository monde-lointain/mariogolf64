"""Pins every Hazard detail-format factory's render() output.

The wider goldens only exercise hazard kinds that actually appear in the deep/-coddog-fixture runs,
so a verbatim-copy typo in a factory for a rarely-emitted kind would pass the suite silently. This
characterizes each factory directly (golden-independent). Expected strings are built from the named
HAZARD_* constant, so a wrong detail format AND a wrong kind constant are both caught.
"""

from __future__ import annotations

from conftest import load_tool

h = load_tool("pick_target_hazards")
H = h.Hazard


def test_pack_factories():
    assert (
        H.pack(h.HAZARD_PACK, 3, ["a=foo", "b=?"]).render()
        == f"{h.HAZARD_PACK}:3fn[a=foo,b=?]"
    )
    assert (
        H.pack(h.HAZARD_SINGLE_FILE_PACK, 2, ["a=foo", "b=foo"]).render()
        == f"{h.HAZARD_SINGLE_FILE_PACK}:2fn[a=foo,b=foo]"
    )
    assert (
        H.upstream_fncount_mismatch(3, 2).render()
        == f"{h.HAZARD_UPSTREAM_FNCOUNT_MISMATCH}:3vs2"
    )
    assert H.c_combined(["b", "a"]).render() == f"{h.HAZARD_C_COMBINED}:2file[a|b]"


def test_scalar_factories():
    assert H.jal_count_mismatch(2, 0).render() == f"{h.HAZARD_JAL_COUNT_MISMATCH}:2vs0"
    assert (
        H.maybe_upstream("libultra", ["f1", "f2"]).render()
        == f"{h.HAZARD_MAYBE_UPSTREAM}:libultra:f1,f2"
    )
    assert H.trailing_pad(16, 64).render() == f"{h.HAZARD_TRAILING_PAD}:16B@64"
    assert H.remaining(5).render() == f"{h.HAZARD_REMAINING}:5"
    assert H.bare_assert(2).render() == f"{h.HAZARD_BARE_ASSERT}:2"
    assert H.twin_of("foo").render() == f"{h.HAZARD_TWIN_OF}:foo"
    assert H.drop_static_mirror(3).render() == f"{h.HAZARD_DROP_STATIC_MIRROR}:3bss"
    assert H.drop_static_mirror(0).render() == f"{h.HAZARD_DROP_STATIC_MIRROR}:bss"


def test_address_list_factories():
    assert (
        H.rodata_jtbl([0x80001000, 0x80000000]).render()
        == f"{h.HAZARD_RODATA_JTBL}:0x80000000,0x80001000"
    )
    assert H.data_static([0x80000000]).render() == f"{h.HAZARD_DATA_STATIC}:0x80000000"
    assert H.defines_data(["a", "b"]).render() == f"{h.HAZARD_DEFINES_DATA}:a,b"
    assert (
        H.needs_header(["h.h(adapt)"]).render() == f"{h.HAZARD_NEEDS_HEADER}:h.h(adapt)"
    )
    assert (
        H.caller_evict(["fn@a", "fn2@b,c"]).render()
        == f"{h.HAZARD_CALLER_EVICT}:fn@a;fn2@b,c"
    )
    assert (
        H.refs_unplaced(["g@0x80001000", "h"]).render()
        == f"{h.HAZARD_REFS_UNPLACED}:g@0x80001000,h"
    )
    assert (
        H.calls_unplaced(["__assertBreak"]).render()
        == f"{h.HAZARD_CALLS_UNPLACED}:__assertBreak"
    )
    assert H.data_carve(["arr1", "arr2"]).render() == f"{h.HAZARD_DATA_CARVE}:arr1,arr2"
    assert (
        H.unattrib_leaf([0x800A2790, 0x800A2780]).render()
        == f"{h.HAZARD_UNATTRIB_LEAF}:0x800A2780,0x800A2790"
    )
    assert (
        H.block_reorder_sibling("nucontgbpakfread.c").render()
        == f"{h.HAZARD_BLOCK_REORDER_SIBLING}:nucontgbpakfread.c"
    )


def test_header_factories():
    assert (
        H.header_renames_symbol("fn", "h.h").render()
        == f"{h.HAZARD_HEADER_RENAMES_SYMBOL}:fn@h.h"
    )
    assert (
        H.wrong_ghidra_name("g", "c", "h.h").render()
        == f"{h.HAZARD_WRONG_GHIDRA_NAME}:g->c@h.h"
    )
    assert (
        H.stale_header(["VERSION_K"]).render()
        == f"{h.HAZARD_STALE_HEADER}:os_version.h(VERSION_K)"
    )


def test_coddog_factories():
    assert (
        H.coddog_twin("foo", ["b", "a"]).render() == f"{h.HAZARD_CODDOG_TWIN}:foo!=a,b"
    )
    assert (
        H.coddog_fncount_mismatch(2, 3).render()
        == f"{h.HAZARD_CODDOG_FNCOUNT_MISMATCH}:2vs3"
    )
    assert (
        H.coddog_structural("f.c", 99.0).render()
        == f"{h.HAZARD_CODDOG_STRUCTURAL}:f.c@99.00"
    )
    assert H.coddog_partial(2, 12).render() == f"{h.HAZARD_CODDOG_PARTIAL}:2of12fn"
    assert (
        H.coddog_source_banked("rotate.c").render()
        == f"{h.HAZARD_CODDOG_SOURCE_BANKED}:rotate.c"
    )
    # coddog_twin pairs with twin_file(); coddog_structural with coddog_pct()
    assert H.coddog_twin("foo", ["a", "b"]).twin_file() == "foo"
    assert H.coddog_structural("f.c", 99.0).coddog_pct() == 99.0
    # body_divergence_suspect (S127): sub-100 coddog-mirror that may mask a game-modified body
    assert (
        H.body_divergence_suspect("f.c", 99.99).render()
        == f"{h.HAZARD_BODY_DIVERGENCE_SUSPECT}:f.c@99.99"
    )


def test_no_detail_factories():
    # bare kinds render to just the constant (no colon/detail)
    assert H.one_tu().render() == h.HAZARD_ONE_TU
    assert H.non16align().render() == h.HAZARD_NON16ALIGN
    assert H.file_static().render() == h.HAZARD_FILE_STATIC


def test_passthrough_detail_factories():
    # detail is pre-assembled by the caller; the factory just attaches the kind
    assert (
        H.combined_subseg("2tu[a,b]").render() == f"{h.HAZARD_COMBINED_SUBSEG}:2tu[a,b]"
    )
    assert (
        H.intrinsic_likely("bcopy.s(kmc-as)").render()
        == f"{h.HAZARD_INTRINSIC_LIKELY}:bcopy.s(kmc-as)"
    )
    assert (
        H.needs_define("F3DEX_GBI_2").render() == f"{h.HAZARD_NEEDS_DEFINE}:F3DEX_GBI_2"
    )
    assert (
        H.rodata_literal("0x80001000").render()
        == f"{h.HAZARD_RODATA_LITERAL}:0x80001000"
    )


def test_game_region_mirror():
    assert (
        H.game_region_mirror(0x800298E0, 0x298E0).render()
        == f"{h.HAZARD_GAME_REGION_MIRROR}:0x800298E0"
    )
    assert (
        H.game_region_mirror(None, 0x298E0).render()
        == f"{h.HAZARD_GAME_REGION_MIRROR}:0x298e0"
    )
