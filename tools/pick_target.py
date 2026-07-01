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
  venv/bin/python3 tools/pick_target.py [--lib SUBSTR] [-n N] [--include-stuck] [--json]

Replaces the old `/decomp-lead` roadmap: target selection is now a tool the plan
gate calls, mirroring marioparty7's pick_target.py.
"""

import argparse
import dataclasses
import json
import os
import re

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
    HAZARD_INTRINSIC_LIKELY,
    HAZARD_JAL_COUNT_MISMATCH,
    HAZARD_MAYBE_UPSTREAM,
    HAZARD_NEEDS_DEFINE,
    HAZARD_NEEDS_HEADER,
    HAZARD_NON16ALIGN,
    HAZARD_ONE_TU,
    HAZARD_PACK,
    HAZARD_REFS_UNPLACED,
    HAZARD_RODATA_JTBL,
    HAZARD_RODATA_LITERAL,
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










































































































def snap_fib(n):
    """Round a raw additive score up to the nearest Fibonacci ladder rung (1,2,3,5,8,13)."""
    for f in (1, 2, 3, 5, 8, 13):
        if n <= f:
            return f
    return 13


# Mirror-path story-point enablers, GROUPED (Phase 1b seed-weight table). Each present group
# adds 1 to the seed base; a group with >1 member kind contributes a SINGLE bump (preserving the
# old `drop`/`recover` OR-pairs: FILE_STATIC + DEFINES_DATA both present is still one `drop`). The
# names are reused by the classical enabler gate. Adding a hazard's mirror-path seed cost is now an
# edit to this table, not to seed_points' body (the Phase-1b de-shotgun of the if-ladder).
_SEED_ENABLER_GROUPS = (
    (
        "drop",
        frozenset({HAZARD_FILE_STATIC, HAZARD_DEFINES_DATA}),
    ),  # → classical fallback
    ("needs_copy", frozenset({HAZARD_NEEDS_HEADER})),  # companion-header copy
    ("needs_define", frozenset({HAZARD_NEEDS_DEFINE})),  # Makefile build-define enabler
    (
        "recover",
        frozenset({HAZARD_REFS_UNPLACED, HAZARD_CALLS_UNPLACED}),
    ),  # asm-data-recovery
)


def seed_points(size, upstream, band, nfns, hazards, blocked):
    """Deterministic a-priori story-point seed (Fibonacci 1,2,3,5,8,13) for one increment —
    the v1 story-point system (see VELOCITY.md). MG64's effort axis is PATH (upstream-mirror vs
    classical-loop) + ENABLER LOAD (band warmth, hazards), not raw bytes; the byte gates (768 /
    1536) are dormant in the current <256 B regime and only bite for large/classical fns.
    `hazards` is the list of Hazard objects; `blocked` is True when any needs-header is un-pickable
    (see include_is_blocked). Returns an int, or 'blk' for a blocked (un-pickable) target.
    Display-only — does NOT influence the smallest-first sort. A committed cluster seed is the
    SUM of its files' seeds (banked per-file, not all-or-nothing across the cluster)."""
    if blocked:
        return "blk"  # un-pickable until the -I path is added / header shipped — a DoR reject
    kinds = {h.kind for h in hazards}
    classical = upstream == "none"
    pack = nfns > 1
    # Which enabler groups are present (table-driven). Each contributes one base bump below; the
    # classical gate keys on the curated subset drop/needs_copy/recover.
    present = {name: bool(kinds & ks) for name, ks in _SEED_ENABLER_GROUPS}
    drop, needs_copy, recover = (
        present["drop"],
        present["needs_copy"],
        present["recover"],
    )
    big = size >= BIG_FN_BYTES
    huge = size >= HUGE_FN_BYTES
    if huge or (classical and pack):
        return 13  # must decompose; never a 1-increment sprint
    if nfns >= PACK_DECOMPOSE_NFNS:
        return 8  # a large pack must decompose regardless of path (hits the 8-gate)
    if classical and (big or pack or drop or needs_copy or recover):
        return 8  # enabler gate — decompose / scaffold first
    if classical:
        return 5  # small single classical fn — unproven regime, high variance
    base = (
        1 if band == "warm" else 2
    )  # warm = enabler-free; cold = symbol recovery / cold deps
    base += sum(
        present.values()
    )  # one bump per present enabler group (drop/copy/define/recover)
    # Unexplained jal-count-mismatch (NOT a `(version-artifact?)` clean-drop) with NO coddog-mirror
    # structural match is a probable near-verbatim version/reorder divergence, not a clean verbatim
    # cp: the verbatim copy SHA-misses and needs an asm-driven diagnosis + hand-edit. Price it one
    # above the mirror floor so the gate plans the near-verbatim risk instead of exempting it as a
    # clean verbatim. A coddog-mirror match (structural fingerprint) or the (version-artifact?)
    # annotation explains the mismatch → no bump; "no coddog" is itself the divergence tell, since a
    # reorder breaks coddog's fingerprint. (S119 nuContGBPakFread: jal `5vs9` macro-artifact + no
    # coddog, planned 2pt clean mirror, realized 3pt block-reorder near-verbatim.) See
    # docs/hazards.md#near-verbatim-mirror-jal-count-mismatch.
    jal_unexplained = any(h.is_unexplained_jal() for h in hazards)
    # A coddog-mirror SUPPRESSES the near-verbatim bump only when it is a CLEAN structural identity.
    # A coddog-structural / coddog-source-banked hit is the S123/S124 customization-MASK tell: the
    # 99.99 fingerprint matches a game-DIVERGENT body (or an already-banked false source), not a
    # verbatim cp. It must NOT suppress the bump, and on a (single-file-)pack it earns the same
    # game-modified-risk +1 — most-of-the-bodies-diverge, route to classical/mixed, not a seed-only
    # mirror (S123 nusched.c coddog-source-banked@99.99 pack ran as a 10/14 mixed bank). The non-lib
    # `jal func_<vram>` game-callee tell is a stronger signal still, but needs the call list here — a
    # tracked follow-up; until then a masking coddog on a pack carries the risk price. See
    # docs/hazards.md#coddog-cross-ref and CLAUDE.md ## Story points (exemption-GUARD).
    coddog_masks = any(h.is_coddog_mask() for h in hazards)
    has_clean_coddog = any(h.is_coddog_mirror() for h in hazards) and not coddog_masks
    if (jal_unexplained and not has_clean_coddog) or (coddog_masks and pack):
        base += 1  # near-verbatim / game-modified risk (the verbatim cp won't match; plan classical)
    if pack or big:
        base += 1
    return snap_fib(base)


def carry_over_names():
    """Function names genuinely PARKED in BACKLOG.md ## Carry-overs (de-ranked from the
    default ranker; build_rows skips a row whose primary is in here unless --include-stuck).

    A parked carry-over is correctly absent from `pick_target` output BY DESIGN — retrieve it
    with `--include-stuck` or by reading the BACKLOG (e.g. pimgr is found via the carry-over, not
    the ranker, and that is the intended path, not a filter bug).

    Two scope guards narrow an over-scoop, where every backticked prose token (a macro `STACK`, a
    name-dropped banked callee, a tool name) was carried — so a *still-asm* primary could be
    silently dropped just by being prose-mentioned:
      (1) stop at the first historical `- **Sprint NN: … BANKED` paragraph — that append-only
          banked-sprint archive lives under the same heading but is NOT parked work; and
      (2) keep only tokens that are real symbols (placed in a name file, or a `func_<vram>`
          placeholder) — drops macro/tool/keyword noise.
    A genuine parked leader (osMotorStop, osCreateScheduler) still de-ranks; the archive's
    name-drops of still-asm functions no longer do."""
    names = set()
    if not os.path.exists(BACKLOG):
        return names
    with open(BACKLOG) as f:
        # Anchor on the HEADING (start-of-line), not a mid-line prose mention of
        # "## Carry-overs" (several digest paragraphs reference it in backticks) — a string split
        # would start the region mid-archive and truncate the genuine carry-overs.
        parts = re.split(r"(?m)^## Carry-overs", f.read(), maxsplit=1)
    if len(parts) <= 1:
        return names
    region = parts[1]
    archive = re.search(
        r"^- \*\*Sprint \d+\b", region, re.M
    )  # guard (1): bound the live region
    if archive:
        region = region[: archive.start()]
    placed = placed_symbols()  # guard (2): real symbols only (names file ∪ func_<vram>)
    for tok in re.findall(r"`([A-Za-z_]\w+)`", region):
        if tok in placed or _FUNC_TOKEN_RE.fullmatch(tok):
            names.add(tok)
    return names


























def _file_scope_static_count(cpath):
    """Number of file-scope static *variable* declarations (the uninitialized .bss family
    FILE_STATIC_RE matches; static function protos excluded). One matching line == one .bss symbol,
    counted without fragile declarator extraction (a `STACK(name, size)` macro hides the name). Used
    for the drop-static-mirror count; a func-local uninitialized static is a known under-count — it
    has no file-scope line and is recovered at the gate alongside the rest."""
    n = 0
    try:
        text = UpstreamSource.get(cpath).text
    except OSError:
        return 0
    for line in text.splitlines():
        s = line.lstrip()
        if s.startswith(("//", "*")):
            continue
        if FILE_STATIC_RE.match(line) and not _is_static_func_proto(line):
            n += 1
    return n


def drop_static_mirror_hazard(mirror_path, hazards):
    """Re-frame a coddog-confirmed verbatim mirror's .bss-family hazard cluster as ONE
    drop-to-extern enabler, or None. When a >=CODDOG_MIRROR_PCT non-audio `coddog-mirror` identity is
    on the row AND a `file-static` is present AND NO carve signal is (no rodata-literal / data-static /
    rodata-jtbl), the file-static + defines-data + refs-unplaced flags are NOT a carve/classical spike:
    every uninitialized file-scope static/global is dropped to a sized `extern` placed at its main_bss
    vram (pure .bss = no ROM bytes = no carve). Returns a `drop-static-mirror:<n>bss` Hazard so the
    gate prices a seed-only N-symbol mirror instead of the scary 4-flag cluster. The co-listed
    file-static/defines-data/refs-unplaced stay (seed_points + the gate's per-symbol recovery read
    them); this tag is the leading verdict that they are one enabler.

    Advisory + graceful: a candidate with a NONZERO-initialized global (real .data, not modelled as a
    distinct flag) that slips the condition degrades to the documented carve playbook when the gate
    SHA misses — never a silent wrong bank."""
    if not any(h.is_file_static() for h in hazards):
        return None
    if any(h.is_carve_signal() for h in hazards):
        return None  # a carve signal disqualifies the pure-.bss drop
    definitive = any(
        h.is_coddog_mirror()
        and not h.coddog_file().startswith("src/audio")
        and h.coddog_pct() >= CODDOG_MIRROR_PCT
        for h in hazards
    )
    if not definitive:
        return None
    n = (
        _file_scope_static_count(mirror_path) + len(defines_data_globals(mirror_path))
        if mirror_path and os.path.isfile(mirror_path)
        else 0
    )
    return Hazard.drop_static_mirror(n)


@dataclasses.dataclass
class Candidate:
    """A ranked subseg's resolved identity + path/band verdict, assembled in build_rows once its
    upstream/coddog branches settle, then passed as ONE argument to score_row (Phase 2a: collapses
    score_row's former 10-parameter list to 3 and retires the (off, primary, up_lib, ...) data
    clump). `fns` is the full member list (len == nfns); `up_lib` is None for a classical target."""

    off: int
    primary: str
    kind: str
    fns: list
    size: int
    up_lib: "str | None"
    band: str
    blocked: bool


def score_row(cand, hazards, carried):
    """Assemble the ranked row dict (score folds size + upstream/warm/carry bonuses)."""
    score = -cand.size + (UPSTREAM_BONUS if cand.up_lib else 0)
    if cand.band == "warm":
        score += BAND_WARM_BONUS  # prefer enabler-free warm-band siblings over equally-small cold ones
    if cand.primary in carried:
        score -= CARRYOVER_PENALTY
    hz = ",".join(h.render() for h in hazards) or "-"
    vram = subseg_vram(cand.off)
    return {
        "func": cand.primary,
        "rom": f"0x{cand.off:X}",
        "vram": f"0x{vram:08X}" if vram is not None else "?",
        "size": cand.size,
        "nfns": len(cand.fns),
        "pts": seed_points(
            cand.size,
            cand.up_lib or "none",
            cand.band,
            len(cand.fns),
            hazards,
            cand.blocked,
        ),
        "kind": cand.kind,
        "upstream": cand.up_lib or "none",
        "band": cand.band,
        "hazards": hz,
        "score": score,
    }


def _append_caller_evict(fns, hazards):
    """Flag each un-named `func_<vram>` member a banked C file calls BY NAME: the gate's curated-symbol
    rename EVICTS it (splat renames the member -> the caller's hard-coded name fails to link), so the
    one-line fixup is priced up-front, not discovered as a build-check link error. Display-only
    (seed_points ignores HAZARD_CALLER_EVICT)."""
    callers = src_func_callers()
    evicts = [
        f"{fn}@{','.join(callers[fn])}"
        for fn in fns
        if fn.startswith("func_") and fn in callers
    ]
    if evicts:
        hazards.append(Hazard.caller_evict(evicts))


def _append_coddog_aux(hazards, cod_srcs):
    """Three advisory coddog passes over the resolved hazard set: flag a match to an ALREADY-BANKED
    source (the mirror is fully decompiled, so the hit is a fingerprint coincidence, not a fresh
    attribution); flag a `c-combined-undercount` when coddog fingerprints MORE upstream files than the
    named-symbol c-combined sees (extra files' members are un-named); and flag a SUB-100 coddog-mirror
    as body-divergence-suspect (it may mask a game-modified body; the gate runs a store-value diagnosis
    before declaring a clean mirror or a compiler wall, S127)."""
    for s in sorted({c for c in cod_srcs if _coddog_source_banked(c)}):
        hazards.append(Hazard.coddog_source_banked(os.path.basename(s)))
    # De-weight the body-divergence hedge on a SINGLE-coddog row whose only divergence signal is a
    # jal-count-mismatch ALREADY explained as an artifact (version wrapper / single-real-call macro
    # expansion): the surplus jals are accounted for, so the sub-100 coddog is a literal/rodata-carve
    # near-match, not a game-modified body — the rodata-literal/jtbl flags carry the real first-build
    # signal. Keep the hedge for a MULTI-coddog pack (genuinely suspect, most bodies diverge) and for
    # an UNexplained mismatch (a real corroborator). S136 n_synthesizer: 3vs8(macro-artifact?) + the
    # lone n_synthesizer.c coddog -> suppressed; banked atomically as a clean mirror + rodata carve.
    cod_files = {h.coddog_file() for h in hazards if h.is_coddog_mirror()}
    single_cod = len(cod_files) == 1
    # c-combined-undercount: the named-symbol c-combined file count is BELOW the distinct coddog-mirror
    # file count → the pack spans more upstream files than the named index sees (the extra files'
    # members are `func_<addr>`, un-named, so c-combined misses them but coddog fingerprints them). The
    # FILE analog of the S131 coddog-fncount-mismatch under-count guard: tells the gate to decompose at
    # MORE boundaries than c-combined lists, and to hand-trace the extra file's boundary from its named
    # member fns. S139 func_8009E4B0: c-combined saw n_auxbus|n_drvrNew (2), coddog saw +n_env (3) ->
    # `2vs3`. Advisory (display-only). The per-file vram-boundary + carve-extent pricing the gate still
    # hand-traces is a tracked follow-up (it needs the member asm %hi/%lo refs at pricing time).
    cc = next((h for h in hazards if h.is_c_combined()), None)
    if cc is not None and len(cod_files) > cc.c_combined_count():
        hazards.append(Hazard.c_combined_undercount(cc.c_combined_count(), len(cod_files)))
    suppress_body_div = single_cod and any(h.is_jal_artifact() for h in hazards)
    # Also suppress on an n_audio_sc N_MICRO clean single-source row: a single coddog hit on a
    # libnaudio / n_audio_sc source whose row is a single-file-pack OR a plain single-fn file (no
    # pack hazard at all). The sub-100 (e.g. @99.99) on these is a STRUCTURAL artifact of N_MICRO
    # macro / inc-include expansion diverging from the non-micro upstream coddog fingerprint, NOT a
    # game-modified body — it fired FALSE on 9 consecutive n_audio_sc mirrors (S133-S139, all banked
    # atomically as clean verbatim mirrors + carves; S139 added n_auxbus + n_drvrNew x2 after a
    # c-combined DECOMPOSE). Keying on `up_lib == libnaudio` + a clean single-source SHAPE (rather
    # than the single-file-pack shape alone) is load-bearing: a c-combined pack DECOMPOSED at the gate
    # becomes per-file single-fn rows (n_auxbus is 1 fn → no pack hazard), which the old
    # single-file-pack-only key would re-flag. `single_cod` already excludes a still-combined
    # multi-coddog pack (n != 1 distinct coddog files → hedge kept, the S123 customization guard). The
    # libnaudio restriction is load-bearing the other way: a libnusys single-file-pack @99.99 CAN be a
    # real game-modified body (S121/S127 contRmbControl FORCESTOP), so it keeps the hedge. The build
    # SHA-1 oracle still catches any real divergence; this only drops a 9x-false up-front warning. See
    # docs/hazards.md#coddog-cross-ref.
    if not suppress_body_div and single_cod:
        clean_single_source = any(
            h.kind == HAZARD_SINGLE_FILE_PACK for h in hazards
        ) or not any(h.kind in (HAZARD_PACK, HAZARD_SINGLE_FILE_PACK) for h in hazards)
        naudio_clean = (
            bool(cod_srcs)
            and all(("n_audio_sc" in s or "libnaudio" in s) for s in cod_srcs)
            and clean_single_source
        )
        suppress_body_div = naudio_clean
    if not suppress_body_div:
        for cf in sorted(
            {
                h.coddog_file()
                for h in hazards
                if h.is_coddog_mirror() and h.coddog_pct() < 100.0
            }
        ):
            pct = max(
                h.coddog_pct()
                for h in hazards
                if h.is_coddog_mirror() and h.coddog_file() == cf
            )
            hazards.append(Hazard.body_divergence_suspect(cf, pct))


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


def _row_filtered(st, args, path, carried, cod_srcs):
    """Return True to DROP the row. Two skip conditions: (1) the --lib scope filter — a row whose
    path / coddog-source / up_lib / member names don't match the requested library; and (2) a
    de-ranked BACKLOG carry-over (retrieved via --include-stuck or the BACKLOG, NOT smallest-first;
    carry_over_names() is region+symbol scoped, and a definitively-coddog'd subseg whose leader was
    merely prose-mentioned overrides the drop via st.cod_members)."""
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

    rows.sort(key=lambda r: (_phantom(r), -r["score"], r["size"]))
    return rows[: args.n]


def main():
    ap = argparse.ArgumentParser(
        description="Rank the next decomp target, smallest-first."
    )
    ap.add_argument("--lib", help="substring filter on subseg path or function name")
    ap.add_argument("-n", type=int, default=20, help="max rows (default 20)")
    ap.add_argument(
        "--include-stuck", action="store_true", help="include BACKLOG carry-overs"
    )
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

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
