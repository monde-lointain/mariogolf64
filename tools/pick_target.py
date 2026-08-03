#!/usr/bin/env python3
"""pick_target.py — rank the next decomp target, smallest-first.

Pure-static ranker (no Ghidra MCP — the agent seeds from MCP inline during the
execution loop). Enumerates flippable `asm` subsegs and partially-matched `c`
files from mariogolf64.yaml + asm/ + src/, flags upstream-mirror availability
(libultra/libkmc), band warmth (a mirror dir with an already-banked sibling →
an enabler-free flip), and hazards (file-scope static, file-scope data-global
definition, refs-unplaced = a data extern referenced but absent from both name
files, non-16-aligned subseg, multi-function pack, needs-header = an upstream
include unresolved under the project -I set, jal-count-mismatch = the upstream
.c's call count disagrees with the ROM fn's jal count = a build divergence =>
near-verbatim mirror), and prints a smallest-first table for `/sprint-plan`.
The refs-unplaced / calls-unplaced grep follows one level of macro expansion so a
global or callee inlined only through an invoked library macro (EPI_SYNC →
__osCurrentHandle) is flagged, not deferred to a mid-execution link failure.

Usage:
  venv/bin/python3 tools/pick_target.py [--lib SUBSTR] [--segment SEG] [-n N] [--include-stuck] [--json]

`--segment main` is a vram-RANGE filter (main-segment game code), distinct from the `--lib`
SUBSTRING filter: `--lib main` matches only the coddog `mainlib/*` packs (the game-embedded 2nd
libnusys instance), NOT the classical main-segment `none` subsegs; the vram range does.

Replaces the old `/decomp-lead` roadmap: target selection is now a tool the plan
gate calls, mirroring marioparty7's pick_target.py.
"""

import argparse
import dataclasses
import json
import os

# BUILD_VERSION config + version-conditional stripping (shared by the C-side and asm-TU scanners),
# extracted to a leaf. _build_version_ord / _strip_inactive_version_branches plus the 3 PP/version
# regexes the staying preprocessor scanners reference are re-imported here.

# C-source text strippers (extracted from this module; re-imported so call sites here read
# unchanged). Pure string->string, stdlib-only; see tools/cpreprocess.py.

# Asm-scanning layer (the per-`asm/<ROM>.s` body walk + the scans over it). Self-contained
# and stdlib-only; imported here so this module keeps the yaml/upstream/hazard/scoring logic.
from decomp_asm import (
    subseg_vram,
)

# Hazard taxonomy (the Hazard record + its HAZARD_* kind vocabulary + coddog tuning),
# extracted to a stdlib-only leaf so every module imports it without a cycle; call sites
# (Hazard(...), HAZARD_*) read unchanged. See tools/pick_target_hazards.py.
# Hazard kinds the ranker still references directly: bare/pass-through constructions (file-static,
# one-tu, non16align, combined-subseg, intrinsic-likely, needs-define, rodata-literal), the pack
# kind args, and `.kind ==` comparisons. The detail-FORMAT kinds are now emitted via Hazard.<kind>()
# factories, so their constants live only in pick_target_hazards (not re-imported here).
# Re-exports the HAZARD_* namespace: tests/tooling/test_pick_target.py references
# these as pt.HAZARD_* (a transitional contract). After Phase E most are no longer
# used by pick_target's own code, hence the statement-level F401 suppression - drop
# it once the tests migrate to pick_target_hazards.HAZARD_*.
from pick_target_hazards import (  # noqa: F401
    CODDOG_MIRROR_PCT,
    CODDOG_STRUCTURAL_BYTES_PER_LOC,
    HAZARD_CALLS_UNPLACED,
    HAZARD_CODDOG_FNCOUNT_MISMATCH,
    HAZARD_CODDOG_MIRROR,
    HAZARD_CODDOG_SOURCE_BANKED,
    HAZARD_CODDOG_STRUCTURAL,
    HAZARD_CODDOG_TWIN,
    HAZARD_COMBINED_SUBSEG,
    HAZARD_DATA_STATIC,
    HAZARD_DEFINES_DATA,
    HAZARD_FILE_STATIC,
    HAZARD_GAME_EMBEDDED,
    HAZARD_INTRINSIC_LIKELY,
    HAZARD_JAL_COUNT_MISMATCH,
    HAZARD_JAL_FREE,
    HAZARD_MAYBE_UPSTREAM,
    HAZARD_NEEDS_DEFINE,
    HAZARD_NEEDS_HEADER,
    HAZARD_NON16ALIGN,
    HAZARD_ONE_TU,
    HAZARD_PACK,
    HAZARD_REFS_UNPLACED,
    HAZARD_RODATA_JTBL,
    HAZARD_RODATA_LITERAL,
    HAZARD_RODATA_STRADDLE,
    HAZARD_SINGLE_FILE_PACK,
    Hazard,
)

# splat-yaml subseg parsing + .rodata carve-range helpers, extracted to a self-contained leaf;
# call sites (parse_subsegs(), the _rodata_* carve helpers) read unchanged. See tools/pick_target_yaml.py.
from pick_target_yaml import (
    parse_subsegs,
)

from pick_target_config import (  # noqa: F401  (re-export: pt.<CONST> contract)
    ASM_INCLUDE_LINE_RE,
    ASM_TU_DEF_RE,
    ASM_VENDOR_INCLUDE_DIRS,
    AUDIO_CODDOG_LIBS,
    AUDIO_CODDOG_MAPS,
    AUDIO_PINS,
    BACKLOG,
    BAND_WARM_BONUS,
    BIG_FN_BYTES,
    BLOCK_REORDER_FAMILIES,
    BOOT_GLOBALS,
    CARRYOVER_PENALTY,
    CODDOG_MAP,
    C_COMMENT_RE,
    DATA_GLOBAL_DEF_RE,
    FILE_STATIC_CONST_ARRAY_RE,
    FILE_STATIC_INIT_ARRAY_RE,
    FILE_STATIC_RE,
    HUGE_FN_BYTES,
    INCLUDE_DIRS,
    INCLUDE_RE,
    KMC_ASM_TU_DEF_RE,
    LIBKMC,
    LIBMUS,
    LIBNAUDIO,
    LIBNUSYS,
    LIBULTRA,
    LIB_EXTRA_INCLUDE_DIRS,
    LOCAL_STATIC_DATA_RE,
    MACRO_REF_RE,
    NAME_FILES,
    NUSYS_CODDOG_MAP,
    PACK_DECOMPOSE_NFNS,
    PRAGMA_WEAK_RE,
    PROJECT_INC,
    ROOT,
    UPSTREAM_BONUS,
    UPSTREAM_DEF_RE,
    UPSTREAM_INC_ROOTS,
    UPSTREAM_SRC_ROOTS,
    UPSTREAM_TREES,
    UpstreamSource,
    _DEF_NONNAME_KW,
    _N64SDK,
)
from pick_target_index import (  # noqa: F401  (re-export: pt.<name> contract)
    C_CALL_RE,
    C_INT_RE,
    _C_NONCALL,
    _DEFINE_VERSION_RE,
    _IFDEF_OPEN_RE,
    _LD_SIBLING_RE,
    _MAKEFILE_DEFINE_RE,
    _NONCODE_RE,
    _WORD_LABEL_OPERAND_RE,
    _append_coddog_twin_hazard,
    _blank_noncode,
    _c_signature,
    _coddog_audio_path,
    _coddog_nusys_path,
    _coddog_source_banked,
    _coddog_upstream_path,
    _intree_asm_macros,
    _iter_upstream_functions,
    _load_coddog_map,
    _meaningful_loc,
    _os_version_defined_tokens,
    _static_carve_siblings,
    _upstream_file_func_count,
    build_asm_tu_index,
    build_audio_indexes,
    build_audio_pin_roots,
    build_coddog_index,
    build_coddog_nusys_index,
    build_kmc_asm_tu_index,
    build_signature_index,
    build_upstream_index,
    signature_hint,
    vendorable_tu_data_symbols,
    vendorable_tu_jtbl,
    vendorable_tu_missing_defines,
)


from pick_target_classify import (  # noqa: F401  (re-export: pt.<name> contract)
    EXTERN_DATA_DECL_RE,
    FN_PTR_TYPEDEF_RE,
    FUNC_MACRO_DEF_RE,
    GBI_MICROCODE_DEFINES,
    SDK_GLOBAL_RE,
    TYPEDEF_CLOSE_RE,
    TYPEDEF_SIMPLE_RE,
    _C_PREDEF_MACROS,
    _DEBUG_ONLY_CALLEES,
    _FUNC_TOKEN_RE,
    _GBI_COND_RE,
    _LOCAL_INCLUDE_RE,
    _MACRO_DEF_NAME_RE,
    _OBJ_DEFINE_RE,
    _active_defines_for_lib,
    _append_coddog_trap_hazards,
    _append_header_version_hazards,
    _append_recover_hazards,
    _append_rodata_carve_hazards,
    _block_reorder_sibling,
    _c_combined_member_paths,
    _c_jal_count,
    _classify_asm_mirror_hazards,
    _classify_pack_hazards,
    _fn_ptr_param_names,
    _gbi_guarded_macros,
    _is_static_func_proto,
    _local_header_macro_names,
    _macro_alias_target,
    _macro_single_real_call,
    _names_in_function_bodies,
    _parse_makefile_defines,
    _project_inc_index,
    _reconcile_calls_unplaced,
    _resolve_include,
    _scan_value_guards,
    _straddling_unattrib,
    _tagged_missing_includes,
    _upstream_body,
    _upstream_defines_function,
    _upstream_src_headers,
    all_fn_ptr_typedefs,
    all_func_macros,
    already_vendored_intree_path,
    append_upstream_hazards,
    band_is_warm,
    band_mirror_dir,
    bare_asserts,
    call_divergence,
    calls_unplaced,
    classify_subseg,
    declared_extern_data,
    declared_type_names,
    defines_data_globals,
    defines_file_static_const_array,
    defines_file_static_init_array,
    defines_local_static_data,
    function_gating_define,
    gbi_value_guard_needs_define,
    has_file_scope_static,
    header_renames_symbol,
    include_is_blocked,
    include_is_vendorable,
    macro_hidden_text,
    missing_includes,
    placed_symbol_addrs,
    placed_symbols,
    refs_unplaced,
    src_func_callers,
    stale_version_header,
)










































































































from pick_target_score import (  # noqa: F401  (re-export: pt.<name> contract)
    Candidate,
    _SEED_ENABLER_GROUPS,
    _STUB_FN_RE,
    _append_caller_evict,
    _append_coddog_aux,
    _file_scope_static_count,
    carried_wall_names,
    carry_over_names,
    drop_static_mirror_hazard,
    loose_stubs,
    nested_child_tell,
    nested_parent_tell,
    nested_parents_of,
    nested_tell,
    score_row,
    seed_points,
    snap_fib,
)












































@dataclasses.dataclass
class _RowState:
    """The per-subseg identity _build_row resolves across its named-upstream + coddog/nusys passes.
    up_lib/up_path/band/blocked/mirror_path/cod_members are default-then-rebound IN SEQUENCE: the
    named-upstream pass seeds the identity, then each coddog pass can override it (mirror_path -> the
    confirmed coddog `.c`; up_lib -> the matched library). Because a later pass reads state an earlier
    one set, the resolve helpers take and mutate ONE _RowState rather than threading a tuple, and
    their call-order is load-bearing."""

    off: int
    size: int
    kind: str
    fns: list
    hazards: list
    primary: str
    up_lib: "str | None"
    up_path: "str | None"
    band: str
    blocked: bool
    mirror_path: "str | None"
    cod_members: list

    @classmethod
    def create(cls, off, size, kind, fns, hazards, upstream_index):
        primary = fns[0]
        up_lib, up_path = upstream_index.get(primary, (None, None))
        return cls(
            off=off,
            size=size,
            kind=kind,
            fns=fns,
            hazards=hazards,
            primary=primary,
            up_lib=up_lib,
            up_path=up_path,
            band="-",
            blocked=False,
            # the .c the drop-static-mirror count reads; coddog identity overrides below
            mirror_path=up_path,
            cod_members=[],
        )


def _resolve_named_upstream(st, upstream_index, coddog_index, sig_index):
    """Seed st's upstream identity from the C-name index. A named `.c` upstream runs the C-mirror
    trap battery (sets band/blocked); an asm-flip with no `.c` def is labelled libultra when it is a
    hand-asm TU mirror, else an un-named `func_` gets an advisory signature hint — unless coddog
    already holds a definitive (non-audio, >=CODDOG_MIRROR_PCT) identity, in which case the hint is
    redundant noise and is skipped."""
    if st.up_path:
        member_paths = _c_combined_member_paths(st.fns, st.up_path, upstream_index)
        st.band, st.blocked = append_upstream_hazards(
            st.off, st.primary, st.up_lib, st.up_path, st.hazards, member_paths
        )
    elif st.kind == "asm-flip":
        if build_asm_tu_index().get(st.primary):
            # A hand-asm libultra mirror (no `.c` def, so absent from upstream_index): bcopy,
            # __osSetFpcCsr, the cache/TLB leaves. Label it libultra for the column/filter/pts
            # so `--lib libultra` surfaces it. The `.s` TU path already rides the
            # intrinsic-likely hazard from classify_subseg; do NOT set up_path (it feeds the
            # C-only append_upstream_hazards) — the asm-mirror-vendoring route, not a C mirror.
            st.up_lib = "libultra"
        elif st.primary.startswith("func_"):
            # No name-index hit + un-named: it may still be an un-named SDK mirror. Run the
            # signature matcher; an advisory hit flags the gate to verify. But skip it when coddog
            # already holds a definitive (>=CODDOG_MIRROR_PCT, non-audio) identity for this fn — the
            # maybe-upstream IDF guess is then redundant noise that can point at the WRONG file. Let
            # the coddog hit (appended below) stand as the sole upstream signal.
            cod = coddog_index.get(st.primary)
            cod_definitive = (
                bool(cod)
                and cod[1] >= CODDOG_MIRROR_PCT
                and not cod[0].startswith("src/audio")
            )
            if not cod_definitive:
                hint = signature_hint(st.off, st.primary, sig_index)
                if hint:
                    st.hazards.append(hint)


def _resolve_primary_coddog(st, coddog_index, upstream_index):
    """coddog cross-ref keyed on the PRIMARY fn, fired only when the C-name index missed it. A coddog
    match is then a verbatim-mirror target mis-seen as classical: flag it, re-price a non-audio
    >=CODDOG_MIRROR_PCT hit to libultra, and re-run the file-level trap battery + the multi-fn
    fncount-mismatch guard over the coddog-resolved `.c`."""
    # coddog cross-ref: a candidate coddog matched to an ultralib fn but that the C-name index
    # missed (un-named / `none`) is a verbatim-mirror target mis-seen as classical. Flag the match
    # always; re-price a ≥CODDOG_MIRROR_PCT *non-audio* hit as a libultra mirror so `seed_points`
    # drops it off the `classical and pack` -> 13 path. Audio hits stay advisory (they still need the
    # one-time audio-header enabler, not modeled here).
    if not st.up_path and st.primary in coddog_index:
        cfile, cpct = coddog_index[st.primary]
        st.hazards.append(Hazard.coddog_mirror(cfile, cpct))
        _append_coddog_twin_hazard(cfile, st.fns, upstream_index, st.hazards)
        if (
            st.up_lib is None
            and cpct >= CODDOG_MIRROR_PCT
            and not cfile.startswith("src/audio")
        ):
            st.up_lib = "libultra"
        # The coddog-resolved upstream is a real `.c`, but its trap detectors never ran: an un-named
        # (`func_<addr>`) subseg's defines-data / file-static / needs-header key off the *named*
        # upstream (absent here), so a verbatim-mirror candidate that DEFINES data or a file-scope
        # static looked clean under the bare coddog flag. Re-run the file-level trap battery
        # (file-static, defines-data, needs-header) AND union refs/calls-unplaced over the
        # coddog-resolved upstream so the gate prices the trap — and seed_points re-prices it
        # (drop/needs_copy) — instead of a mid-sprint BSS-layout stall. Shared with the tail-identity
        # block below via _append_coddog_trap_hazards.
        cl_path = _coddog_upstream_path(cfile)
        if cl_path:
            st.mirror_path = (
                cl_path  # the confirmed coddog identity is the file we mirror + count
            )
            st.blocked = (
                _append_coddog_trap_hazards(
                    st.off, st.primary, cl_path, st.up_lib, st.hazards
                )
                or st.blocked
            )
            # A coddog hit on a multi-fn pack can be a STRUCTURAL fingerprint match, not a source
            # attribution. If the matched file defines FEWER fns than the pack holds, it cannot be
            # the sole source → the pack is multi-file; flag so the gate doesn't read it as a
            # single-file-pack mirror. Under-count direction ONLY: a true single source may define
            # MORE (version/_DEBUG-gated extras), so `matched > nfns` is never flagged.
            if len(st.fns) > 1:
                matched_n = _upstream_file_func_count(st.up_lib or "libultra", cl_path)
                if 0 < matched_n < len(st.fns):
                    st.hazards.append(
                        Hazard.coddog_fncount_mismatch(matched_n, len(st.fns))
                    )


def _append_coddog_mirror_sources(st, members, path_resolver, lib, *, twin_index=None):
    """Shared loop of the tail-coddog and nusys passes (the libultra/libnusys duplication factored
    out): for each DISTINCT source in `members` (each a (fn, cfile, pct) tuple) not already flagged,
    append a coddog-mirror hazard, optionally a twin hazard (when twin_index is given), resolve the
    source `.c` via path_resolver, set st.mirror_path, and run the trap battery under `lib`. Returns
    the sorted distinct cfile list the callers' single-identity fncount/structural guards key off.

    The two passes still differ above and below this loop (audio filter, twin/fncount/partial guards,
    final re-price), so only the loop is shared — a full merge would need a flag per difference."""
    already = {h.coddog_file() for h in st.hazards if h.is_coddog_mirror()}
    for cfile in sorted({c for _, c, _ in members}):
        if cfile in already:
            continue
        cpct = max(p for _, c, p in members if c == cfile)
        st.hazards.append(Hazard.coddog_mirror(cfile, cpct))
        if twin_index is not None:
            _append_coddog_twin_hazard(cfile, st.fns, twin_index, st.hazards)
        cl_path = path_resolver(cfile)
        if cl_path:
            st.mirror_path = (
                cl_path  # the confirmed coddog identity is the file we mirror + count
            )
            st.blocked = (
                _append_coddog_trap_hazards(
                    st.off, st.primary, cl_path, lib, st.hazards
                )
                or st.blocked
            )
    return sorted({c for _, c, _ in members})


def _resolve_tail_coddog(st, coddog_index, upstream_index):
    """coddog cross-ref over ALL members (the tail-identity pass). A multi-fn subseg's mirror
    identity often lives in an UN-NAMED tail member, not the leader the primary pass keys on; scan
    every member, surface each distinct coddog `.c` the primary pass missed, re-run its trap battery,
    and re-apply the single-identity fncount/structural + multi-twin partial guards. Sets
    st.cod_members (read by the carry-over filter) and re-prices the subseg to libultra."""
    # A multi-fn asm subseg's mirror IDENTITY often lives in its UN-NAMED tail, not its named (or
    # mis-attributed) leader. The primary block above keys coddog on `primary` AND fires only when
    # `not up_path`, so a named-leader subseg whose tail `func_<addr>` members coddog-match an
    # ultralib `.c` was invisible. Scan ALL members; surface each distinct coddog `.c` identity the
    # primary block did not already flag, and label the subseg libultra so seed_points + the column
    # price it as the mirror it is.
    cod_members = st.cod_members = [
        (fn, coddog_index[fn][0], coddog_index[fn][1])
        for fn in st.fns
        if fn in coddog_index
        and coddog_index[fn][1] >= CODDOG_MIRROR_PCT
        and not coddog_index[fn][0].startswith("src/audio")
    ]
    if cod_members:
        # Surface each tail-identity file's traps too (the primary pass keys on the primary, so a
        # coddog hit carried by an un-named TAIL member reached here with only the bare coddog flag).
        distinct = _append_coddog_mirror_sources(
            st,
            cod_members,
            _coddog_upstream_path,
            st.up_lib,
            twin_index=upstream_index,
        )
        # The under-count fncount-mismatch + structural size-ratio guards run in the PRIMARY coddog
        # block (above) only when the identity is on `primary`. When the WHOLE pack resolves to ONE
        # coddog .c via a TAIL member, neither guard ran, so the phantom surfaced as a clean
        # single-source mirror. Re-run both here when the pack has exactly one coddog identity (dedup
        # the fncount flag w/ primary).
        if len(st.fns) > 1 and len(distinct) == 1:
            cl1 = _coddog_upstream_path(distinct[0])
            if cl1:
                matched_n = _upstream_file_func_count(st.up_lib or "libultra", cl1)
                if 0 < matched_n < len(st.fns) and not any(
                    h.kind == HAZARD_CODDOG_FNCOUNT_MISMATCH for h in st.hazards
                ):
                    st.hazards.append(
                        Hazard.coddog_fncount_mismatch(matched_n, len(st.fns))
                    )
                loc = _meaningful_loc(cl1)
                if loc and st.size > CODDOG_STRUCTURAL_BYTES_PER_LOC * loc:
                    cpct = max(p for _, c, p in cod_members if c == distinct[0])
                    st.hazards.append(Hazard.coddog_structural(distinct[0], cpct))
        # ≥2 DISTINCT per-fn TWIN files matched to ONE multi-fn subseg, covering only a SUBSET of its
        # fns → a per-fn fingerprint set, NOT a single-file mirror (the un-matched fns diverge from
        # the combined source). The multi-twin companion to the len==1-only coddog-fncount-mismatch.
        # Advisory: per-fn verify, do NOT read it as a clean mirror.
        twin_files = {h.twin_file() for h in st.hazards if h.kind == HAZARD_CODDOG_TWIN}
        if (
            len(distinct) >= 2
            and len(twin_files) >= 2
            and len(cod_members) < len(st.fns)
        ):
            st.hazards.append(Hazard.coddog_partial(len(cod_members), len(st.fns)))
        if st.up_lib is None:
            st.up_lib = "libultra"


def _resolve_nusys(st, nusys_index):
    """The libnusys analog of the libultra coddog passes, kept SEPARATE so an absent nusys map leaves
    libultra ranking byte-identical. Fires only when no libultra coddog/name identity won (up_lib
    None or already libnusys); scans primary + all members, flags + re-runs the trap battery against
    the real nusys `.c`, applies the single-identity structural guard, and re-prices to libnusys."""
    # Nusys coddog cross-ref: the libnusys analog of the libultra blocks above, kept as a SEPARATE
    # additive pass so an absent nusys_map.tsv leaves libultra ranking byte-identical. Fires only when
    # no libultra coddog/name identity won (up_lib still None, or already libnusys). Scans primary +
    # all members; for each distinct nusys source matched at >= CODDOG_MIRROR_PCT, flags coddog-mirror,
    # re-runs the file-level trap battery against the real nusys `.c` (libnusys clib), and re-prices
    # the subseg as a libnusys mirror so seed_points drops it off the classical path. A single-identity
    # multi-fn pack gets the structural size-ratio guard (a tiny source fingerprint-matched to a big
    # subseg — the `*FuncSet` one-liner twins coddog reports at 99.99%).
    if st.up_lib in (None, "libnusys") and nusys_index:
        nz = [
            (fn, nusys_index[fn][0], nusys_index[fn][1])
            for fn in st.fns
            if fn in nusys_index and nusys_index[fn][1] >= CODDOG_MIRROR_PCT
        ]
        if nz:
            distinct = _append_coddog_mirror_sources(
                st, nz, _coddog_nusys_path, "libnusys"
            )
            if len(st.fns) > 1 and len(distinct) == 1:
                cl1 = _coddog_nusys_path(distinct[0])
                loc = _meaningful_loc(cl1) if cl1 else 0
                if loc and st.size > CODDOG_STRUCTURAL_BYTES_PER_LOC * loc:
                    cpct = max(p for _, c, p in nz if c == distinct[0])
                    st.hazards.append(Hazard.coddog_structural(distinct[0], cpct))
            if st.up_lib is None:
                st.up_lib = "libnusys"


def _resolve_audio(st, audio_indexes, audio_roots):
    """The libmus / libnaudio / nuaulstl analog of _resolve_nusys, kept SEPARATE so absent audio maps
    leave libultra/libnusys ranking byte-identical. The game links libmus (the `mus_*` sequence
    player) plus an n_audio synth layer; tools/audio_pin.py picks the matching version+compiler+flags
    and writes the canonical per-lib maps. Iterate the audio libs in order; the first whose members
    coddog-match at >= CODDOG_MIRROR_PCT claims the subseg: flag coddog-mirror, re-run the trap battery
    against the pinned `.c` (clib=lib), apply the single-identity structural guard, and re-price to
    that lib so seed_points drops it off the classical path. up_lib gating makes it one-lib-per-subseg
    and idempotent across the three passes."""
    for lib in AUDIO_CODDOG_LIBS:
        index = audio_indexes.get(lib) or {}
        if not index or st.up_lib not in (None, lib):
            continue
        az = [
            (fn, index[fn][0], index[fn][1])
            for fn in st.fns
            if fn in index and index[fn][1] >= CODDOG_MIRROR_PCT
        ]
        if not az:
            continue

        def resolver(cfile, _lib=lib):
            return _coddog_audio_path(cfile, audio_roots, _lib)

        distinct = _append_coddog_mirror_sources(st, az, resolver, lib)
        if len(st.fns) > 1 and len(distinct) == 1:
            cl1 = resolver(distinct[0])
            # The pack resolves to ONE coddog `.c`, but that file may define FEWER fns than the pack
            # holds: the surplus members are a FOREIGN TU (a separate upstream `.c`) bundled in the
            # subseg, often a sub-coddog-floor leaf the per-member match missed (S131: n_synsetfxmix.c
            # @99.99 matched func_800A09F0, but the 4-instr n_alSynDelete leaf at func_800A09E0 is below
            # coddog's fingerprint floor, so it went unmatched and len(distinct) stayed 1). Flag
            # `coddog-fncount-mismatch:<matched>vs<pack>` so the gate splits at the file boundary
            # instead of hand-disassembling the subseg. Ports the libultra tail-coddog guard
            # (_resolve_tail_coddog) into the audio path, where it was missing. Advisory.
            if cl1:
                matched_n = _upstream_file_func_count(lib, cl1)
                if 0 < matched_n < len(st.fns) and not any(
                    h.kind == HAZARD_CODDOG_FNCOUNT_MISMATCH for h in st.hazards
                ):
                    st.hazards.append(
                        Hazard.coddog_fncount_mismatch(matched_n, len(st.fns))
                    )
            loc = _meaningful_loc(cl1) if cl1 else 0
            if loc and st.size > CODDOG_STRUCTURAL_BYTES_PER_LOC * loc:
                cpct = max(p for _, c, p in az if c == distinct[0])
                st.hazards.append(Hazard.coddog_structural(distinct[0], cpct))
        if st.up_lib is None:
            st.up_lib = lib
    _deblk_audio_variant_misresolve(st)
    _append_static_name_collisions(st)


def _deblk_audio_variant_misresolve(st):
    """Lift a FALSE `blk` an audio row inherited from the C-name index resolving `up_path` to the
    WRONG source variant. The game's libnaudio C-name index can point `up_path` at the NON-sc
    `libnaudio/src/<f>.c`, whose `#include "add/<f>_addNN.c"` body-fragments live in a tree the project
    does NOT mirror (so they price NON-vendorable -> the named pass set `blocked` -> `blk`), while the
    AUTHORITATIVE source is the coddog-confirmed n_audio_sc `<f>.c` with VENDORABLE
    `inc/<f>_addNN.inc.c` fragments. The bogus `add/*.c` block masked the pickable mirror (it hid
    n_save S133, n_resample S134, n_load S135). When a definitive audio coddog-mirror replaced
    `up_path` with a DIFFERENT `mirror_path`, that coddog source is authoritative: drop the
    wrong-variant needs-header hazard and re-derive `blocked` from the coddog source's (vendorable)
    includes only. S135."""
    if not (
        st.blocked
        and st.up_path
        and st.mirror_path
        and st.mirror_path != st.up_path
        and any(
            h.is_coddog_mirror() and h.coddog_pct() >= CODDOG_MIRROR_PCT
            for h in st.hazards
        )
    ):
        return
    named_tagged, named_blk = _tagged_missing_includes(st.up_path, st.up_lib)
    if not named_blk:
        return  # the named pass was not the block source — leave blocked as-is
    bogus = ",".join(named_tagged)
    st.hazards[:] = [
        h
        for h in st.hazards
        if not (h.kind == HAZARD_NEEDS_HEADER and h.detail == bogus)
    ]
    _, st.blocked = _tagged_missing_includes(st.mirror_path, st.up_lib)


def _append_static_name_collisions(st):
    """Flag each upstream function whose verbatim name is ALREADY a curated symbol placed OUTSIDE this
    subseg (present in placed_symbols but absent from st.fns) -> a FILE-STATIC whose name reuses a
    global that names a DIFFERENT vram (a non-micro twin holds it). Naming THIS subseg's instance as
    that global would multiply-define the label at the gate scaffold, so the gate keeps it file-local
    instead of adding a colliding symbol_addrs entry. Keyed on the coddog `mirror_path`; audio-scoped
    (the n_audio_sc N_MICRO statics mirror non-micro twins, e.g. _decodeChunk @0x8009FA14 vs the placed
    _decodeChunk @0x800A4E3C). Advisory. S135."""
    if not st.mirror_path or not any(h.is_coddog_mirror() for h in st.hazards):
        return
    try:
        text = UpstreamSource.get(st.mirror_path).text
    except OSError:
        return
    placed = placed_symbol_addrs()
    members = set(st.fns)
    seen = set()
    for name, _body in _iter_upstream_functions(text):
        if name in members or name in seen or name not in placed:
            continue
        seen.add(name)
        st.hazards.append(Hazard.static_name_collision(name, placed[name]))


# vram RANGES for the `--segment` filter (distinct from the `--lib` substring). main-SEGMENT game
# code is 0x80025C50..0x801F4A2F; overlays reuse vram >= 0x801F4A30, so the main upper bound also
# excludes them (memory: mg64-ghidra-main-segment-map). Add a new segment here when a range is defined.
SEGMENT_RANGES = {
    "main": (0x80025C50, 0x801F4A2F),
}


def _row_filtered(st, args, path, carried, cod_srcs):
    """Return True to DROP the row. Skip conditions: (0) the `--segment` vram-range filter — a row
    whose subseg vram falls outside the requested segment's range; (1) the --lib scope filter — a row
    whose path / coddog-source / up_lib / member names don't match the requested library; and (2) a
    de-ranked BACKLOG carry-over (retrieved via --include-stuck or the BACKLOG, NOT smallest-first;
    carry_over_names() is region+symbol scoped, and a definitively-coddog'd subseg whose leader was
    merely prose-mentioned overrides the drop via st.cod_members)."""
    # (0) `--segment main` catches the classical main-segment `none` subsegs that `--lib main` (a
    # substring match on the coddog `mainlib/*` packs) misses; keyed on the subseg vram, not a string.
    seg = getattr(args, "segment", None)
    if seg:
        lo, hi = SEGMENT_RANGES.get(seg, (None, None))
        if lo is not None:
            v = subseg_vram(st.off)
            if v is None or not (lo <= v <= hi):
                return True
    # `--lib audio` is a SCOPE ALIAS for the game's audio libraries (libmus / libnaudio / nuaulstl),
    # not a literal substring: an n_audio_sc mirror whose coddog source is a bare basename (e.g.
    # "n_resample.c") and whose up_lib is "libnaudio" would otherwise DROP, since "audio" is not a
    # substring of "n_resample.c", the up_lib test below is exact-match ("libnaudio" != "audio"), and
    # its un-named `func_` members carry no "audio" either. Pre-fix, an audio row surfaced under
    # `--lib audio` only by accident (n_load via its `src/audio/load.c` coddog variant; the libnusys
    # audio_system_boot via its fn name), so the de-blk'd n_resample/n_reverb rows were invisible on
    # the audio frontier (S134). Keying the alias on up_lib makes `--lib audio` show the whole audio
    # band uniformly; the incidental path/fn substring matches below still hold (no regression).
    audio_scope = args.lib == "audio" and st.up_lib in AUDIO_CODDOG_LIBS
    cod_in_scope = audio_scope or (
        bool(args.lib)
        and bool(cod_srcs)
        and (args.lib == "libultra" or any(args.lib in s for s in cod_srcs))
    )
    if (
        args.lib
        and not cod_in_scope
        and args.lib not in (path or "")
        and (st.up_lib or "") != args.lib
        and not any(args.lib in n for n in st.fns)
    ):
        return True
    return st.primary in carried and not args.include_stuck and not st.cod_members


@dataclasses.dataclass(frozen=True)
class Indexes:
    """The cross-reference catalog build_rows/_build_row/_resolve_* consult together:
    the six lookup tables that always travel as a unit. Bundled into one parameter
    object so the per-subseg _build_row takes one catalog instead of six indexes
    (Introduce Parameter Object). coddog/nusys/audio/audio_roots are normalized to {}
    by build_rows before construction."""

    upstream: dict
    signature: tuple
    coddog: dict
    nusys: dict
    audio: dict
    audio_roots: dict

    @classmethod
    def build(cls):
        """Construct the full catalog from the on-disk index builders (the six
        build_*_index passes). Used by main(); tests construct Indexes directly with
        synthetic maps."""
        return cls(
            build_upstream_index(),
            build_signature_index(),
            build_coddog_index(),
            build_coddog_nusys_index(),
            build_audio_indexes(),
            build_audio_pin_roots(),
        )


def _build_row(off, typ, path, size, args, idx, carried, _libultra_band_start):
    """Classify one subseg and produce its scored row dict, or None to skip it (bss,
    scope-filtered, or a de-ranked carry-over). The per-subseg body of build_rows;
    the resolved identity lives on a _RowState (st) the coddog/nusys passes rebind in
    place as they run. `idx` is the Indexes catalog, unpacked below into the local
    index names the body and resolve passes use."""
    upstream_index = idx.upstream
    sig_index = idx.signature
    coddog_index = idx.coddog
    nusys_index = idx.nusys
    audio_indexes = idx.audio
    audio_roots = idx.audio_roots
    classified = classify_subseg(off, typ, path, size, upstream_index)
    if classified is None:
        return None
    kind, fns, hazards = classified

    _append_caller_evict(fns, hazards)

    st = _RowState.create(off, size, kind, fns, hazards, upstream_index)
    _resolve_named_upstream(st, upstream_index, coddog_index, sig_index)
    _resolve_primary_coddog(st, coddog_index, upstream_index)
    _resolve_tail_coddog(st, coddog_index, upstream_index)
    _resolve_nusys(st, nusys_index)
    _resolve_audio(st, audio_indexes, audio_roots)

    # Suppress the `maybe-upstream` signature guess once a DEFINITIVE (>=CODDOG_MIRROR_PCT)
    # `coddog-mirror` is on the row. `_resolve_named_upstream` skips the guess only when the
    # *libultra* coddog_index holds the fn (`cod_definitive`); an AUDIO mirror's identity arrives
    # later from `_resolve_audio`'s separate audio_indexes, so the guess was appended before the
    # coddog-mirror existed and stayed as noise that points at the WRONG file (S132 func_800A0800:
    # maybe-upstream listed n_synstopvoice / n_synstartvoiceparam / n_synstartvoice while the
    # coddog-mirror correctly named n_synallocvoice.c, both fns from it). The libnaudio tree is stood
    # up (S129), so an audio coddog @>=pct is now a bankable identity, not a header-gated advisory.
    # A SUB-threshold coddog hit stays advisory, so its guess is retained as a second opinion.
    if any(h.is_coddog_mirror() and h.coddog_pct() >= CODDOG_MIRROR_PCT for h in st.hazards):
        st.hazards[:] = [h for h in st.hazards if h.kind != HAZARD_MAYBE_UPSTREAM]

    # A `coddog-mirror` hazard pins the row to an ultralib(libultra) source even when up_lib stayed
    # None — audio coddog hits are deliberately NOT re-priced to libultra (they were header-`-I`-
    # gated), so a clean audio mirror surfaced as `upstream none` and `--lib libultra` skipped it.
    # The coddog map IS the ultralib sweep, so any coddog-mirror match means libultra; also honor a
    # sub-path scope (e.g. `--lib audio` -> src/audio/...) via the matched-source path substring.
    cod_srcs = [h.coddog_source() for h in st.hazards if h.is_coddog_mirror()]
    _append_coddog_aux(st.hazards, cod_srcs)
    # A libultra-source mirror whose vram is BELOW the libultra code band is game-linked at -O2, not
    # -O3 — route it to a -O2 path (src/mgu/…), NOT src/libultra/ (which forces -O3 → wrong
    # auto-inlining). Gated on up_lib == "libultra" (excludes audio coddog hits, which stay
    # un-re-priced + above the band anyway).
    if (
        st.up_lib == "libultra"
        and _libultra_band_start is not None
        and st.off < _libultra_band_start
    ):
        v = subseg_vram(st.off)
        st.hazards.append(Hazard.game_region_mirror(v, st.off))
    # A game-region coddog-mirror that covers only a SUBSET of its subseg (fncount-mismatch /
    # structural / partial) is game-EMBEDDED: the upstream fns are compiled into a larger game TU,
    # tight-packed with game fns, so a standalone carve SHA-misses on the non-16 tail. Plan a mixed
    # 16-aligned carve, not a seed-only mirror (S128 audio_mgr.c). Synthesized last, from the
    # assembled hazard set, so it reads as the leading verdict over the flags it summarizes.
    ge = Hazard.game_embedded_for(st.hazards, subseg_vram(st.off))
    if ge:
        st.hazards.append(ge)
    if _row_filtered(st, args, path, carried, cod_srcs):
        return None

    # Re-frame a coddog-confirmed pure-.bss file-static cluster as one drop-to-extern enabler (not a
    # carve/classical spike) — appended last so it reads as the leading verdict over the
    # file-static/defines-data/refs-unplaced flags it summarizes.
    dsm = drop_static_mirror_hazard(st.mirror_path, st.hazards)
    if dsm:
        st.hazards.append(dsm)

    # Block-reorder tell (libnusys unexplained jal-mismatch + no coddog) in a family with a BANKED
    # block-reorder mirror → name the sibling so the gate applies the SAME CheckConnector/RAM-enable
    # swap up-front (appended after coddog so the no-coddog test sees the final hazard set).
    brs = _block_reorder_sibling(st.up_path, st.hazards, st.up_lib)
    if brs:
        st.hazards.append(Hazard.block_reorder_sibling(brs))

    cand = Candidate(
        st.off, st.primary, st.kind, st.fns, st.size, st.up_lib, st.band, st.blocked
    )
    return score_row(cand, st.hazards, carried)


def _refresh_residual(fns):
    """Crack-slice STEP 0 helper (S282): rebuild each FUNC's isolated object and cmpfn it, printing
    the FRESH instruction count + diff-row count so a carry's stale doc (count AND residual class are
    both hypotheses, S268) is re-derived before it is trusted. Requires an existing base.c seed."""
    import glob
    import subprocess

    src = os.path.join(ROOT, "src")
    tree_flag = {"main": "MAIN=1", "libultra": "LIBULTRA=1", "libkmc": "LIBKMC=1"}
    rc = 0
    for fn in fns:
        cfile = None
        for cpath in glob.glob(os.path.join(src, "*", "*.c")):
            try:
                with open(cpath) as f:
                    if fn in _STUB_FN_RE.findall(f.read()):
                        cfile = cpath
                        break
            except OSError:
                continue
        if cfile is None:
            print(f"?? {fn}: no INCLUDE_ASM stub in src/*/*.c")
            rc = 1
            continue
        tree = os.path.relpath(cfile, src).split(os.sep)[0]
        flag = tree_flag.get(tree, "")
        seed = os.path.join(ROOT, "nonmatchings", fn, "base.c")
        if not os.path.exists(seed):
            print(f"!! {fn} [{tree}]: no nonmatchings/{fn}/base.c seed — seed it first")
            rc = 1
            continue
        cmd = ["make", "nonmatching-func", f"FUNC={fn}"] + ([flag] if flag else [])
        b = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
        if b.returncode != 0:
            print(f"!! {fn} [{tree}]: build failed:\n{b.stderr.strip()[-400:]}")
            rc = 1
            continue
        obj = os.path.join("nonmatchings", fn, "current.o")
        c = subprocess.run(
            ["tools/cmpfn.sh", fn, obj], cwd=ROOT, capture_output=True, text=True
        )
        lines = c.stdout.splitlines()
        head = lines[0] if lines else "(cmpfn: no output)"
        ndiff = sum(1 for line in lines if line[:1] in "<>")
        print(f"{fn} [{tree}]: {head}  diffs={ndiff}")
    return rc


def build_rows(args, idx, carried):
    subs = parse_subsegs()
    # The lowest-rom `libultra/` subseg = the start of the libultra code band. A libultra-source
    # mirror BELOW it is game-region (-O2), not libultra-band (-O3) — feeds HAZARD_GAME_REGION_MIRROR.
    _libultra_band_start = min(
        (o for o, _, p in subs if (p or "").startswith("libultra/")), default=None
    )
    rows = []
    for i, (off, typ, path) in enumerate(subs):
        size = subs[i + 1][0] - off if i + 1 < len(subs) else 0
        row = _build_row(
            off, typ, path, size, args, idx, carried, _libultra_band_start
        )
        if row is not None:
            rows.append(row)
    # Phantom de-rank (S147): a row whose attribution is ONLY a low-confidence phantom signal
    # (`maybe-upstream` hint, or `coddog-source-banked` match to an already-banked source) sinks below
    # every genuine candidate, so phantoms stop crowding the top-N and burying real work behind size.
    # A genuine fresh mirror (coddog-mirror to an UNbanked source) carries no phantom-risk hazard, so
    # it is unaffected. The agent must verify bodies before chasing any phantom-risk row.
    # Both phantom kinds always carry a detail, so they render as "<kind>:<detail>" tokens inside the
    # comma-joined r["hazards"] string; a "<kind>:" substring test is exact.
    _phantom_tokens = (HAZARD_MAYBE_UPSTREAM + ":", HAZARD_CODDOG_SOURCE_BANKED + ":")

    def _phantom(r):
        return 1 if any(tok in r["hazards"] for tok in _phantom_tokens) else 0

    # Deferred-mirror de-rank (S161): a `game-embedded` coddog-mirror (coddog-mirror + game-region +
    # a subset signal → a MIXED 16-aligned game-region carve, NOT a seed-only verbatim mirror) sinks
    # below every clean classical `none` / genuine-mirror candidate, so the smallest-first top-N stops
    # surfacing the llcvt/settime embedded packs the PO defers until that track opens. Still ABOVE pure
    # phantoms. The token always renders as `game-embedded:0x<vram>`, so a `<kind>:` substring is exact.
    _deferred_token = HAZARD_GAME_EMBEDDED + ":"

    def _deferred(r):
        return 1 if _deferred_token in r["hazards"] else 0

    rows.sort(key=lambda r: (_phantom(r), _deferred(r), -r["score"], r["size"]))
    return rows[: args.n]


def main():
    ap = argparse.ArgumentParser(
        description="Rank the next decomp target, smallest-first."
    )
    ap.add_argument("--lib", help="substring filter on subseg path or function name")
    ap.add_argument(
        "--segment",
        help="vram-range segment filter (e.g. `main`), distinct from the --lib substring; "
        f"segments: {', '.join(sorted(SEGMENT_RANGES))}",
    )
    ap.add_argument("-n", type=int, default=20, help="max rows (default 20)")
    ap.add_argument(
        "--include-stuck", action="store_true", help="include BACKLOG carry-overs"
    )
    ap.add_argument("--json", action="store_true")
    ap.add_argument(
        "--carried-check",
        nargs="+",
        metavar="FUNC",
        help="DoR gate: for each FUNC report CARRIED-WALL (owns a docs/wip note or a BACKLOG "
        "carry-over entry) vs fresh. Use before committing a hand-mined partial-pack leaf.",
    )
    ap.add_argument(
        "--nested-check",
        nargs="+",
        metavar="FUNC",
        help="DoR gate: for each FUNC report NESTED-CHILD (its .s prologue spills an incoming $v0 "
        "static chain = a GCC nested function, banks with its parent, not standalone) vs standalone. "
        "Use on the smallest leaves of a fresh classical pack before committing them.",
    )
    ap.add_argument(
        "--refresh-residual",
        nargs="+",
        metavar="FUNC",
        help="Crack-slice STEP 0 (S282/S268): for each FUNC rebuild its nonmatchings/FUNC/base.c "
        "(make nonmatching-func, tree-derived MAIN/LIBULTRA/LIBKMC flag) and run tools/cmpfn.sh, "
        "printing the FRESH instruction count (rom vs mine) + diff-row count. A carried-wall doc's "
        "stated COUNT and residual CLASS are BOTH hypotheses; re-derive from a fresh build before "
        "trusting the doc. Requires an existing base.c seed.",
    )
    ap.add_argument(
        "--loose-stubs",
        metavar="SEG",
        help="Enumerate still-INCLUDE_ASM stubs inside ALREADY-`c` files of src/SEG/, smallest-first, "
        "each tagged fresh / CARRIED-WALL / NESTED-CHILD / INTRINSIC-HASM. Surfaces fresh individual "
        "leaves the whole-subseg pack ranker misses (S273). Default hides carried+nested+intrinsic; "
        "--all shows every stub.",
    )
    ap.add_argument(
        "--all",
        action="store_true",
        help="with --loose-stubs, list every stub (incl. carried-wall/nested/intrinsic-hasm), not just fresh ones.",
    )
    ap.add_argument(
        "--file-close",
        metavar="SEG",
        help="Rank the still-INCLUDE_ASM stubs of src/SEG/ by how close their HOST FILE is to zero "
        "stubs, so the gate can see which single function takes a file to md5-candidate. Points bank "
        "per file, so in a mined-out segment a one-stub host is the only place one function is worth "
        "a point (S317 closed two that way). Shows hosts with <= --n stubs, fewest first.",
    )
    args = ap.parse_args()

    # `--lib` is a substring filter, so a segment name passed to it does not error: it silently
    # matches whatever coddog path or member name happens to contain the word and returns unrelated
    # packs. Reject the one case we can name for certain.
    if args.lib and args.lib in SEGMENT_RANGES:
        ap.error(
            f"--lib {args.lib} is a segment, not a library. --lib is a substring filter over the "
            f"subseg path, coddog source, up_lib and member names, so it would silently return "
            f"unrelated packs. Use --segment {args.lib}, or --loose-stubs {args.lib} for the "
            f"individual stubs inside already-`c` files."
        )

    if args.file_close:
        # Points bank per FILE (gates.md ## Story points), so a leaf's value is not only its own
        # size: the last stub in a host is worth the whole file's md5-candidate flip, and every
        # other stub in that host is worth zero until it lands. --loose-stubs sorts by leaf size and
        # cannot see this, which is why S317's goal ("close main's single-stub files") was found by
        # a hand grep at the gate rather than by the ranker.
        stubs = loose_stubs(args.file_close)
        by_file = {}
        for s_ in stubs:
            by_file.setdefault(s_["file"], []).append(s_)
        rows = sorted(by_file.items(), key=lambda kv: (len(kv[1]), kv[0]))
        shown = 0
        for rel, members in rows:
            if len(members) > args.n:
                break
            shown += 1
            print(f"{len(members)} stub(s)  {rel}")
            for m in sorted(members, key=lambda x: (x["size"] is None, x["size"] or 0, x["fn"])):
                sz = "?" if m["size"] is None else str(m["size"])
                tags = []
                if m["carried"]:
                    tags.append("CARRIED-WALL")
                if m["nested"]:
                    tags.append("NESTED-CHILD")
                if m.get("intrinsic"):
                    tags.append("INTRINSIC-HASM")
                if m.get("wall_class"):
                    tags.append(m["wall_class"])
                if m.get("fp_class"):
                    tags.append(m["fp_class"])
                print(f"    {sz:>6}  {m['fn']:28} {'  '.join(tags)}")
        n_one = sum(1 for _, m in rows if len(m) == 1)
        print(
            f"# {len(rows)} src/{args.file_close}/ files still hold stubs; {n_one} are one stub from "
            f"md5-candidate ({shown} host(s) shown at <= {args.n} stubs). A CARRIED-WALL tag on a "
            f"one-stub host is a price, not a veto: it is the file's last function by construction."
        )
        return

    if args.loose_stubs:
        stubs = loose_stubs(args.loose_stubs)

        def _is_fresh(s):
            # `dl-emitter` (S275's `raw-dl-emitter`, renamed S294) is a PRICING tag and counts as
            # fresh: the leaf needs the DL reconstruction recipe, which is not a wall. S258 retired
            # the underlying verdict; S293+S294 then banked the seven smallest members of the class
            # 7/7 at 154-307 instructions, zero permuter runs, zero compiler-source dives.
            # `jtbl-dispatch` was excluded wholesale from S275 to S306 on one leaf's alignment
            # wall. S307 split that verdict per leaf (jtbl_carve_tell): a `jtbl-carveable` table is
            # 8-aligned on both edges AND has no foreign rodata between it and the host's existing
            # carve, so its bank-time carve is a single subseg line -- S307's kSetMultiTLB was that
            # shape and banked on the first build. `jtbl-carve-blocked` (alignment),
            # `pool-cohort:` (needs its sibling banked in the same commit) and
            # `carve-pool-blocked:` (S310: the host's carve PRECEDES the table and asm-owned rodata
            # sits in the gap, so it cannot be extended forward to reach it) stay excluded.
            return (
                not s["carried"]
                and not s["nested"]
                and not s.get("intrinsic")
                and (
                    s.get("wall_class") in (None, "", "dl-emitter")
                    or s.get("jtbl_carve") == "jtbl-carveable"
                )
                # pool-blocked: the body would emit a `.rodata` template the ROM keeps in a pool
                # shared with still-asm siblings, so it cannot bank until they are C -- a bank-time
                # blocker like jtbl-dispatch, not a codegen verdict (S300).
                and not s.get("pool_blocked")
            )

        shown = stubs if args.all else [s for s in stubs if _is_fresh(s)]
        print(f"{'size':>6} {'status':20} {'func':28} file")
        for s in shown:
            if s["carried"]:
                status = "CARRIED-WALL"
            elif s["nested"]:
                status = "NESTED"
            elif s.get("intrinsic"):
                status = "INTRINSIC-HASM"
            elif s.get("pool_blocked"):
                status = "POOL-BLOCKED"
            elif s.get("wall_class") == "jtbl-dispatch" and s.get("jtbl_carve"):
                # The carve verdict, not the bare class: carveable counts in fresh, blocked is the
                # alignment wall, pool-cohort names the sibling it must bank with (S307).
                status = s["jtbl_carve"].upper()
            elif s.get("wall_class"):
                # dl-emitter: counts as fresh, tagged for pricing only (S294).
                status = s["wall_class"].upper()
            else:
                status = "fresh"
            sz = "?" if s["size"] is None else str(s["size"])
            # A dl-twin is a bank-order signal, not a wall: an equal DL command-word multiset means
            # the same body shape, so banking either one makes the other a near-mechanical replay
            # (S294 func_80031450 / func_8009351C). Order twins adjacently in the committed backlog.
            twin = f"  dl-twin:{s['dl_twin']}" if s.get("dl_twin") else ""
            # An FP mnemonic COUNT does not price a leaf; what the floats feed does (S295). fp-coord
            # = converted out to integers (cheap), fp-sched = computed and stored (the S276/S277/S290
            # class), fp-mixed = real float arithmetic, price as risk. Advisory: not in `fresh`.
            fpc = f"  {s['fp_class']}" if s.get("fp_class") else ""
            # A stub's own `.asciz` references are its cheapest provenance: S307's func_8005342C
            # printed `kSetMultiTLB : ...`, which named the routine, its signature and its
            # page-mode table before any build, with no upstream copy to coddog against.
            strs = "".join(f'  "{t}"' for t in s.get("strings", []))
            # A DEWEIGHT, not a wall: one callee accounting for every `jal` means the leaf is call
            # glue over a single (usually already-`c`) helper, so a high jal count is not size-like
            # risk here (S311 func_80095DE0, 611 instr / 39 jal / one target, banked on build 2).
            sc = f"  single-callee:{s['single_callee']}" if s.get("single_callee") else ""
            print(f"{sz:>6} {status:20} {s['fn']:28} {s['file']}{twin}{fpc}{sc}{strs}")
        n_fresh = sum(1 for s in stubs if _is_fresh(s))
        print(
            f"# {len(stubs)} stubs in src/{args.loose_stubs}/: {n_fresh} fresh, "
            f"{sum(s['carried'] for s in stubs)} carried-wall, "
            f"{sum(s['nested'] for s in stubs)} nested, "
            f"{sum(bool(s.get('intrinsic')) for s in stubs)} intrinsic-hasm, "
            f"{sum(bool(s.get('pool_blocked')) for s in stubs)} pool-blocked "
            f"(block-move template in an uncarved pool; banks when the pool-owning siblings do, S300), "
            f"{sum(s.get('wall_class') == 'jtbl-dispatch' for s in stubs)} jtbl-dispatch "
            f"({sum(s.get('jtbl_carve') == 'jtbl-carveable' for s in stubs)} carveable and counted "
            f"in fresh, {sum(s.get('jtbl_carve') == 'jtbl-carve-blocked' for s in stubs)} "
            f"alignment-blocked, "
            f"{sum(str(s.get('jtbl_carve', '')).startswith('pool-cohort') for s in stubs)} "
            f"pool-cohort, "
            f"{sum(str(s.get('jtbl_carve', '')).startswith('carve-pool-blocked') for s in stubs)} "
            f"carve-pool-blocked (host carve precedes the table with asm-owned rodata in the gap; "
            f"banks when those owners are C, S310), S307), "
            f"{sum(s.get('wall_class') == 'dl-emitter' for s in stubs)} dl-emitter "
            f"(pricing tag, counted in fresh; wall framing retired S294 after 7/7 banked), "
            f"{sum(bool(s.get('dl_twin')) for s in stubs)} in dl-twin groups, "
            f"FP by consumer: {sum(s.get('fp_class') == 'fp-coord' for s in stubs)} fp-coord "
            f"(converted out, cheap), "
            f"{sum(s.get('fp_class') == 'fp-sched' for s in stubs)} fp-sched, "
            f"{sum(s.get('fp_class') == 'fp-mixed' for s in stubs)} fp-mixed (advisory, S295)"
        )
        if args.loose_stubs == "main":
            # S280 plateau advisory: `fresh` here is a CEILING, not a clean pool -- the
            # FP-scheduler / value-select-branch-likely / register-allocation (coloring OR the
            # nested-fn arg-pointer "pressure") walls are NOT .s-detectable and read `fresh`, then
            # wall at attempt. Measured flagged-fresh bank-rate: S273 3/3, S274 2/4, S279 1/3,
            # S280 0/2 (unaided). NB the S280 walls were NOT terminal: a PO-directed compiler-source
            # fan-out cracked both fully-RE'd exact-count carries. So for UNAIDED smallest-first,
            # prefer a FRESH non-main pack; for a fully-RE'd exact-count / structural carry, a
            # gcc-2.7.2 fan-out slice out-yields another smallest-first main continuation.
            # S286 family-locality clause: the plateau is measured over UNRELATED fresh leaves. A
            # fresh leaf that is a structural sibling of an already-banked fn in the same file is
            # not plateau-bound -- the banked sibling supplies the types, externs, frame shape and
            # most of the body. S285 3/3 and S286 3/3, both the func_80095A10.c camera family.
            print(
                "# ADVISORY (main plateau, S280): `fresh` is a CEILING not a clean pool "
                "(FP-sched / value-select / register-alloc walls read fresh). Unaided main "
                "bank-rate is low (S273 3/3 -> S280 0/2); prefer a FRESH non-main pack for "
                "smallest-first, OR a compiler-source fan-out on a fully-RE'd exact-count carry "
                "(cracked both S280 carries). EXCEPTION (S286): a fresh leaf that is a structural "
                "sibling of an already-banked fn in the SAME file is not plateau-bound "
                "(S285 3/3, S286 3/3, both func_80095A10.c; S287 3/3 in func_80059BA0.c). "
                "EXCEPTION (S287): a heavy-FP leaf whose rodata constants match fdlibm is a "
                "TRANSCRIPTION, not a wall -- func_80059BA0.c is the game's embedded libm "
                "(fabsf/atanf/atan2f banked; func_80059BC0 = acosf by its pS0-pS5/qS1-qS4 "
                "coefficients). Check the constants before pricing a heavy-FP main leaf. "
                "MEASURE FP the right way (S290): count FP MNEMONICS "
                "(lwc1|swc1|mtc1|mfc1|<op>.s|<op>.d|cvt.|c.<cc>.s|bc1) in the `.s`, never "
                "`grep '$f[0-9]'` -- the register grep returns 0 on a heavily-FP function here and "
                "prices it as a clean integer leaf, the opposite of its class. "
                "S290 dropped func_8005D3B8 on that corrected count (36 swc1 / 31 lwc1 / "
                "22 cvt.s.w over two game float tables, not an fdlibm pool). "
                "But the COUNT is not the class (S295): read the `fp-coord`/`fp-sched`/`fp-mixed` "
                "column, which classifies by what consumes the floats. S295 priced func_800880A0 "
                "(fp=31) as its designated drop and it banked byte-exact on the first build -- all "
                "31 mnemonics were 10.2 coordinate conversion in a composite emitter. More "
                "generally, the tells filter CLASSES, not difficulty within a class: both leaves "
                "that cost iterations that sprint read clean on every tell the ranker has. "
                "POOL STATE (S305): main's fresh vein is spent -- one row remains "
                "(draw_terrain_aim_grid, 0x1514, the largest in the dl-emitter class). The "
                "smallest-first choice inside this segment is gone, so the next main slice is a "
                "deliberate pick between that one big row and a characterised carry, and the "
                "sibling-locality exception above is the only thing that still prices a main leaf "
                "cheaply. S305 itself: the two fresh rows it took banked 1/2, the miss reaching "
                "968/974 with the ROM's frame on a scheduler placement."
            )
        raise SystemExit(0 if n_fresh else 1)

    if args.refresh_residual:
        raise SystemExit(_refresh_residual(args.refresh_residual))

    if args.carried_check:
        walls = carried_wall_names()
        any_wall = False
        for fn in args.carried_check:
            hit = fn in walls
            any_wall = any_wall or hit
            print(f"{'CARRIED-WALL' if hit else 'fresh':13} {fn}")
        raise SystemExit(1 if any_wall else 0)

    if args.nested_check:
        any_nested = False
        for fn in args.nested_check:
            child = nested_child_tell(fn)
            parent = nested_parent_tell(fn)
            any_nested = any_nested or child or parent
            label = (
                "NESTED-CHILD"
                if child
                else "NESTED-PARENT" if parent else "standalone"
            )
            # A child banks INSIDE its parent, so the verdict is only a price once the parent is
            # named (S310). The scan is over `asm/nonmatchings/**`, so a relic `.s` for an
            # already-banked caller can appear -- check it is still `INCLUDE_ASM` before pricing.
            note = ""
            if child:
                parents = nested_parents_of(fn)
                if parents:
                    note = "  nested-parent:" + ",".join(parents)
            print(f"{label:13} {fn}{note}")
        raise SystemExit(1 if any_nested else 0)

    rows = build_rows(args, Indexes.build(), carry_over_names())
    if args.json:
        print(json.dumps(rows, indent=1))
        return
    if not rows:
        print("(no candidates — every asm subseg flipped and every c file at 0 stubs?)")
        return
    print(
        f"{'func':28} {'rom':>9} {'vram':>10} {'size':>5} {'nfn':>3} {'pts':>3} {'kind':8} "
        f"{'upstream':9} {'band':4} hazards"
    )
    for r in rows:
        print(
            f"{r['func']:28} {r['rom']:>9} {r['vram']:>10} {r['size']:>5} {r['nfns']:>3} "
            f"{str(r['pts']):>3} {r['kind']:8} {r['upstream']:9} {r['band']:4} {r['hazards']}"
        )


if __name__ == "__main__":
    main()
