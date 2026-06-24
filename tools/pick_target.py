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
import functools
import glob
import json
import math
import os
import re

import decomp_common as dc

# BUILD_VERSION config + version-conditional stripping (shared by the C-side and asm-TU scanners),
# extracted to a leaf. _build_version_ord / _strip_inactive_version_branches plus the 3 PP/version
# regexes the staying preprocessor scanners reference are re-imported here.
from build_config import (
    _BUILD_VERSION_IF_RE,
    _PP_ENDIF_LINE_RE,
    _PP_OPENING_DIRECTIVE_RE,
    _build_version_ord,
    _strip_inactive_version_branches,
)

# C-source text strippers (extracted from this module; re-imported so call sites here read
# unchanged). Pure string->string, stdlib-only; see tools/cpreprocess.py.
from cpreprocess import (
    _strip_comments,
    _strip_dead_blocks,
    _strip_define_lines,
    _strip_string_literals,
)

# Asm-scanning layer (the per-`asm/<ROM>.s` body walk + the scans over it). Self-contained
# and stdlib-only; imported here so this module keeps the yaml/upstream/hazard/scoring logic.
# _FRAME_IMMS is shared with the C-side _c_signature below.
from decomp_asm import (
    _FRAME_IMMS,
    _asm_jal_count,
    _asm_signature,
    asm_function_addrs,
    asm_functions,
    code_end_rom,
    intrinsic_likely,
    privileged_asm,
    recover_unplaced_call_vram,
    recover_unplaced_vram,
    rodata_jtbls,
    rodata_literals,
    rodata_word_refs,
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
    YAML,
    _literal_in_rodata,
    _rodata_carve_end_vram,
    _rodata_carve_start_vram,
    parse_subsegs,
)

ROOT = dc.PROJECT_ROOT  # literal, symlink-stable root (see decomp_common.PROJECT_ROOT)
LIBULTRA = os.path.expanduser("~/development/repos/ultralib/src")
LIBKMC = os.path.expanduser("~/development/repos/libkmc/src")
# The game also links libnusys / libmus / libnaudio (KMC N64 SDK). Their functions are
# mirrorable verbatim exactly like libultra/libkmc; scanning their source here lets the
# name index map the *named* ones (nuGfxSwapCfb, nuContRamWrite, …) to an upstream instead
# of mislabeling them `none`. The un-named `func_<vram>` mirrors are caught separately by
# the signature matcher (signature_hint). Roots point at each lib's `.c` source tree.
_N64SDK = os.path.expanduser("~/development/repos/n64sdkmod/packages")
LIBNUSYS = os.path.join(_N64SDK, "libnusys/usr/src/PR/libsrc/nusys-2.07")
LIBNAUDIO = os.path.join(_N64SDK, "libnaudio/usr/src/PR/libsrc")
LIBMUS = os.path.join(_N64SDK, "libmus/usr/src/PR/libsrc/libmus/src")
# lib-name -> source tree root. The single source of truth for build_upstream_index,
# band_mirror_dir, and the signature index. Order = name-collision precedence (first wins).
UPSTREAM_TREES = (
    ("libultra", LIBULTRA),
    ("libkmc", LIBKMC),
    ("libnusys", LIBNUSYS),
    ("libnaudio", LIBNAUDIO),
    ("libmus", LIBMUS),
)
BACKLOG = os.path.join(ROOT, "BACKLOG.md")
# The two disjoint symbol-name files. A symbol absent from BOTH gets no linker address; for a
# *data* extern that means an undefined reference (see refs_unplaced). Functions are exempt —
# splat auto-resolves them via undefined_funcs_auto.txt.
NAME_FILES = [
    os.path.join(ROOT, "symbol_addrs.txt"),
    os.path.join(ROOT, "ghidra_symbols.txt"),
]
# Optional coddog cross-ref map (mgname<TAB>ulname<TAB>ulfile<TAB>pct), written by
# tools/coddog_sweep.sh. Absent by default → build_coddog_index() returns {} and ranking is
# unchanged. A ≥CODDOG_MIRROR_PCT non-audio hit re-prices an un-named/none candidate as a libultra
# mirror. See docs/hazards.md#coddog-cross-ref.
CODDOG_MAP = os.environ.get(
    "CODDOG_MAP", os.path.join(ROOT, "tools/coddog/coddog_map.tsv")
)
# Optional nusys coddog cross-ref map — the libnusys analog of CODDOG_MAP, written by
# tools/nusys_sweep.sh. Absent by default → build_coddog_nusys_index() returns {} and ranking is
# unchanged. A ≥CODDOG_MIRROR_PCT hit re-prices an un-named/none candidate as a libnusys mirror. Its
# `cfile` paths are `mainlib/<base>.c` (nusys-2.07-src-relative), resolved by _coddog_nusys_path.
NUSYS_CODDOG_MAP = os.environ.get(
    "NUSYS_CODDOG_MAP", os.path.join(ROOT, "tools/coddog/nusys_map.tsv")
)
# Optional audio coddog cross-ref maps — libmus / libnaudio / nuaulstl, written by
# tools/audio_sweep.sh and pinned (version+compiler+flags) by tools/audio_pin.py, which also writes
# audio_pins.tsv (lib -> the pinned on-disk source root). Absent by default → build_audio_indexes()
# returns {} per lib and ranking is unchanged. A ≥CODDOG_MIRROR_PCT hit re-prices an un-named/none
# candidate as that lib's verbatim mirror. The canonical `<lib>_map.tsv` is the winning matrix cell's
# map; its `cfile` paths are source basenames resolved against the pinned root. See
# docs/hazards.md#coddog-cross-ref.
AUDIO_CODDOG_LIBS = ("libmus", "libnaudio", "nuaulstl")
AUDIO_CODDOG_MAPS = {
    lib: os.environ.get(
        f"{lib.upper()}_CODDOG_MAP", os.path.join(ROOT, f"tools/coddog/{lib}_map.tsv")
    )
    for lib in AUDIO_CODDOG_LIBS
}
AUDIO_PINS = os.environ.get(
    "AUDIO_PINS", os.path.join(ROOT, "tools/coddog/audio_pins.tsv")
)

# Approximate C function-definition: a return-type-led line ending in `name(`.
UPSTREAM_DEF_RE = re.compile(r"^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w+)\s*\(", re.M)
# Keywords that can lead a `<word>(` at a statement boundary but are NOT a function name — control
# flow (an indented `if (`/`switch (`) and type/storage qualifiers (the `int (*tbl[])(void)` decl,
# whose token before the first `(` is `int`). _iter_upstream_functions filters these.
_DEF_NONNAME_KW = frozenset(
    (
        "if",
        "while",
        "for",
        "switch",
        "else",
        "do",
        "return",
        "case",
        "goto",
        "sizeof",
        "default",
        "int",
        "char",
        "short",
        "long",
        "unsigned",
        "signed",
        "float",
        "double",
        "void",
        "struct",
        "union",
        "enum",
        "const",
        "static",
        "extern",
        "volatile",
        "register",
        "typedef",
        "auto",
    )
)
# `#pragma weak <alias> = <impl>` exports <alias> at the implementation's address. The def loop
# only keys the impl name (gu/cosf.c defines `fcos`/`__cosf`), so the ROM/curated weak alias `cosf`
# was absent from the upstream index → a pack member read `cosf=?` and hid cosf.c from the
# c-combined member labels. Keying the alias too lets a weak-aliased SDK fn resolve to its
# upstream file (cosf/sinf/fcos/fsin), the C analog of ASM_TU_DEF_RE's WEAK arm (bcopy → _bcopy).
PRAGMA_WEAK_RE = re.compile(
    r"^\s*#pragma\s+weak\s+([A-Za-z_]\w+)\s*=\s*[A-Za-z_]\w+", re.M
)
# A hand-asm TU's exported entry: `LEAF(osGetCount)` / `XLEAF(name)` / `WEAK(bcopy, _bcopy)`
# (sys/asm.h macros). The WEAK arm catches the public name of an aliased TU (bcopy → _bcopy).
# Keys build_asm_tu_index so an intrinsic-likely shim can name its vendorable ultralib .s TU.
ASM_TU_DEF_RE = re.compile(r"^\s*(?:X?LEAF|WEAK)\(\s*([A-Za-z_]\w+)", re.M)
# libkmc hand-asm TUs (soft-float / 64-bit math: mmuldi3.s, mcvtld.s) export entries with a bare
# `.globl name` — KMC register conventions, assembled via the KMC `as` path (NOT ultralib's
# LEAF/XLEAF macros + LIBULTRA_ASFLAGS). Keys build_kmc_asm_tu_index so a libkmc asm-only TU the
# pure-shim/privileged tests miss (a branchy cvt routine) still names its vendorable .s + the
# kmc-as mechanism. See docs/hazards.md#asm-mirror-vendoring (the kmc-as sub-lane).
KMC_ASM_TU_DEF_RE = re.compile(r"^\s*\.globl\s+([A-Za-z_]\w+)", re.M)
# A vendorable asm TU references CPU/cache/TLB constants by macro (K0BASE, DCACHE_SIZE, C0_ENTRYHI,
# RDB_BASE_VIRTUAL_ADDR, …). The gate must vendor-compile it under LIBULTRA_ASFLAGS, so a macro the
# in-tree headers don't define is a needs-define enabler (e.g. osMapTLBRdb's RDB_* would block if
# PR/rdb.h were absent). MACRO_REF_RE matches an UPPER_CASE token (≥3 chars so register names like
# t0/a0/ra — lower-case — and 2-char directives never match); C_COMMENT_RE/ASM_INCLUDE_LINE_RE strip
# the noise sources (a `/* TLB */` comment word, the `R4300` in an `#include "PR/R4300.h"` path).
MACRO_REF_RE = re.compile(r"\b[A-Z][A-Z0-9_]{2,}\b")
C_COMMENT_RE = re.compile(r"/\*.*?\*/", re.S)
ASM_INCLUDE_LINE_RE = re.compile(r"^\s*#\s*include.*$", re.M)
# The `-I` set LIBULTRA_ASFLAGS assembles a vendored .s under (Makefile). A macro defined in none of
# these headers is genuinely missing — the asm analog of missing_includes' header reachability.
ASM_VENDOR_INCLUDE_DIRS = [
    os.path.join(ROOT, d)
    for d in (
        "include",
        "include/libultra",
        "include/libultra/PR",
        "include/libultra/compiler/gcc",
    )
]
FILE_STATIC_RE = re.compile(r"^\s*static\b[^=]*;\s*$")
# A file-scope data-global *definition* (external linkage): `<type> <name> [= init];`.
# Applied only at brace-depth 0 (see defines_data_globals) so function-body statements
# and struct members never match. Captures the variable name.
DATA_GLOBAL_DEF_RE = re.compile(
    r"^(?:(?:struct|union|enum)\s+)?[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w+)"
    r"\s*(?:\[([^\]]*)\]\s*)?(?:=[^;]*)?;\s*$"
)
# A function-local *initialized* static: `static <type> <name> [= init];` inside a body.
# defines_data_globals can't see it (it skips `static` lines AND scans only brace-depth 0), but
# KMC GCC 2.7.2 emits it into the TU's .data exactly like a file-scope global, so a verbatim mirror
# re-emits the bytes and needs the same .data sibling carve. The non-greedy prefix backtracks so the
# captured name is the last identifier before `=` (gu/position.c's `static float dtor = π/180`).
LOCAL_STATIC_DATA_RE = re.compile(
    r"^static\b[^;{}]*?\b([A-Za-z_]\w+)\s*(?:\[[^\]]*\])?\s*=[^;]*;\s*$"
)
# A file-scope `static const <type> <name>[...] = ...` array. `const` static is file-PRIVATE
# rodata (the `static` keyword bars cross-file linkage), so its presence makes widening the
# rodata-literal carve-start to the subseg boundary FP-safe (the scan misses the array base
# `addiu %lo` + string-literal pointers). Non-greedy prefix backtracks to the last id before `[`
# (xldtob.c's `static const ldouble pows[] = {...}`). The initializer may run past the line end (a
# multi-line `{...}`), so this matches the opening decl `... [...] =`, not the trailing `;`.
FILE_STATIC_CONST_ARRAY_RE = re.compile(
    r"^static\s+const\b[^;{}]*?\b([A-Za-z_]\w+)\s*\[[^\]]*\]\s*="
)
# A file-scope NON-const INITIALIZED static array (`static <type> <name>[…] = …;`) → .data (not
# rodata). The verbatim mirror re-emits it, so it needs a .data sibling carve at the recovered vram
# (e.g. xprintf spaces/zeroes, env eqpower, _Litob ldigs/udigs). The `(?!const)`
# lookahead defers const arrays to FILE_STATIC_CONST_ARRAY_RE (rodata); `[^;{}=]*?` keeps the type span
# off the initializer. Uninitialized statics (no `=`) are .bss (file-static / drop-static), not this.
FILE_STATIC_INIT_ARRAY_RE = re.compile(
    r"^static\s+(?!const\b)[^;{}=]*?\b([A-Za-z_]\w+)\s*\[[^\]]*\]\s*="
)
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
# The project's quoted/angle include search dirs (Makefile CFLAGS `-I` set; -nostdinc removed — stdarg.h ships at include/stdarg.h for correct MIPS GCC 2.7.2 vararg ABI).
INCLUDE_DIRS = [
    os.path.join(ROOT, d)
    for d in (
        "include",
        "include/libultra",
        "include/libultra/internal",
        "include/libkmc",
        "include/libnusys",
    )
]
PROJECT_INC = os.path.join(ROOT, "include")
# Per-lib extra `-I` dirs the Makefile's profile CFLAGS add on top of the base INCLUDE_DIRS, so a
# mirror's include resolution must match the profile it actually compiles under. LIBULTRA_CFLAGS
# (Makefile) prepends `-I include/libultra/compiler/gcc` and appends `-I include/libultra/PR`, so a
# libultra mirror resolves <libaudio.h>/<os_internal.h>/<ultraerror.h> and the rest of the PR/ band
# through these — missing_includes must include them for a libultra candidate or it false-flags
# needs-header → blk (it once hid the whole audio/PR mirror band this way).
# libkmc/libnusys CFLAGS add no extra `-I`, so they map to the base set.
LIB_EXTRA_INCLUDE_DIRS = {
    "libultra": [
        os.path.join(ROOT, "include/libultra/compiler/gcc"),
        os.path.join(ROOT, "include/libultra/PR"),
    ],
}
# Upstream include trees a missing companion header can be *copied from* (the execution-middle
# mirror, e.g. assert.h). A header absent here and absent from the project tree is a system
# header we don't ship (blocked); a header present in the project tree but unreachable under the
# candidate's effective `-I` set (INCLUDE_DIRS + LIB_EXTRA_INCLUDE_DIRS[lib]) is an unindexed-`-I`
# case (blocked, a deferred Makefile enabler). See classify of needs-header in seed_points.
UPSTREAM_INC_ROOTS = [
    # ultralib is the sole libultra mirror source (LIBULTRA = ultralib/src), so its include tree is
    # the libultra companion-header donor — it ships the `PRinternal/` prefix the mirrors `#include`.
    # The PI-band PRinternal/piint.h copy came from here.
    os.path.expanduser("~/development/repos/ultralib/include"),
    os.path.expanduser("~/development/repos/libkmc/include"),
    os.path.expanduser(
        "~/development/repos/n64sdkmod/packages/libnusys/usr/src/PR/libsrc/nusys-2.07/nusys/include"
    ),
]
# Upstream library *source* trees — the donors for SOURCE-PRIVATE companion headers (e.g.
# ultralib `src/libc/xstdio.h`, `src/gu/guint.h`) that a mirror `#include`s with quotes and that
# get copied source-relative next to the mirrored .c, NOT into an -I dir. Distinct from
# UPSTREAM_INC_ROOTS (the public -I header donors). A `needs-header:<h>` whose `<h>` lives here is
# vendorable (a cheap source-relative cp), not a blocked DoR reject (e.g. guint.h, xstdio.h).
UPSTREAM_SRC_ROOTS = [
    LIBULTRA,  # ~/development/repos/ultralib/src
]
UPSTREAM_BONUS = 200
# The libultra boot-region globals (fixed RAM 0x80000300-0x8000031C). A verbatim mirror references
# these by name (osRomBase, osMemSize, …) but the asm bakes them as raw lui/addiu immediates that
# splat auto-labels `D_800003xx` — so refs_unplaced's `__`-prefix / declared-extern grep misses
# them and the mirror link-fails on first compile. Surfacing them with their known vram lets the
# gate add the recover-extern from this table, no asm-data-recovery pass needed.
BOOT_GLOBALS = {
    "osTvType": 0x80000300,
    "osRomType": 0x80000304,
    "osRomBase": 0x80000308,
    "osResetType": 0x8000030C,
    "osCicId": 0x80000310,
    "osVersion": 0x80000314,
    "osMemSize": 0x80000318,
    "osAppNMIBuffer": 0x8000031C,
}
CARRYOVER_PENALTY = 1000
# A warm band (the candidate's mirror dir already holds a banked sibling) means its
# companion headers + callee symbols are in-tree → an enabler-free flip. Worth ~one
# leaf-function of size: it lifts a warm candidate over a band-cold one of similar
# size (the tiebreak intent) without overriding large size gaps. Validated twice —
# the si pair (Sprint 4) and vi pair (Sprint 5) both banked at one-file cost off warm bands.
BAND_WARM_BONUS = 64
# Story-point seed size thresholds (bytes): a "big" leaf trips the enabler gate
# (decompose/scaffold first), a "huge" one must always decompose (never a
# 1-increment sprint). A pack of this many functions decomposes regardless of path.
BIG_FN_BYTES = 768
HUGE_FN_BYTES = 1536
PACK_DECOMPOSE_NFNS = 4

# Banked block-reorder mirror families, keyed by upstream-file basename PREFIX. MG64's per-file nusys
# revision reorders two source blocks vs every archived SDK (the GBPak F-variants run the RAM-enable
# block before nuContGBPakCheckConnector, `ram=0` in the range-check delay slot). A new same-family fn
# carrying the block-reorder tell (unexplained jal-mismatch + no coddog-mirror) needs the SAME
# block-swap; the value is the banked sibling .c that confirms it. Seeded S119 nucontgbpakfread ->
# S120 nucontgbpakfwrite (banked first-build once the swap was applied up-front). See
# docs/hazards.md#near-verbatim-mirror-jal-count-mismatch.
BLOCK_REORDER_FAMILIES = {"nucontgbpak": "nucontgbpakfread.c"}


# --- Upstream source cache -------------------------------------------------
class UpstreamSource:
    """Cached view of one upstream source/header file (Phase 2b). `UpstreamSource.get(path).text`
    reads the file once (errors='ignore') and memoizes it for the process, so the ~10+ producers
    that each inspect the SAME upstream .c per candidate (file-static / defines-data / refs- &
    calls-unplaced / missing-includes / call-divergence / version-header / ...) read it ONCE instead
    of re-open()ing it every time. Retires the per-producer redundant IO + the Feature Envy of a
    pile of functions that each open a path they were handed. The producers keep their (cpath, ...)
    signatures and route their read through here, so the test seams are unchanged. `.text` raises
    OSError exactly like open(), so callers keep their own try/except."""

    _cache: "dict[str, UpstreamSource]" = {}

    def __init__(self, path):
        self.path = path
        self._text = None
        self._read = False

    @classmethod
    def get(cls, path):
        inst = cls._cache.get(path)
        if inst is None:
            inst = cls._cache[path] = cls(path)
        return inst

    @property
    def text(self):
        if not self._read:
            with open(
                self.path, errors="ignore"
            ) as f:  # OSError propagates; callers keep try/except
                self._text = f.read()
            self._read = True
        return self._text


def build_upstream_index():
    """Map upstream function name -> (lib, path), scanning libultra/libkmc once.

    Two determinism fixes. (1) Sort the glob so a name defined in multiple upstream files
    resolves to the alphabetically-first directory, not whatever the filesystem yields first: gu/
    beats mgu/ — MG64 mirrors libgultra, not the fast-gu `mgu` variant — so a pack member like
    guMtxXFML (in both gu/mtxcatl.c under a version guard and mgu/mtxxfml.c) co-locates with its
    sibling guMtxCatL in gu/mtxcatl.c instead of mislabeling to mgu (which cascaded a phantom
    `../gu/guint.h` needs-header FP). (2) Strip version-inactive branches first, so a def behind a
    dead `#if BUILD_VERSION` guard for this build's ordinal doesn't register a phantom attribution."""
    index = {}
    for lib, tree in UPSTREAM_TREES:
        if not os.path.isdir(tree):
            continue
        build_ord = _build_version_ord(lib)
        for cpath in sorted(glob.glob(os.path.join(tree, "**", "*.c"), recursive=True)):
            try:
                with open(cpath, errors="ignore") as f:
                    text = f.read()
            except OSError:
                continue
            text = _strip_inactive_version_branches(text, build_ord)
            for m in UPSTREAM_DEF_RE.finditer(text):
                index.setdefault(m.group(1), (lib, cpath))
            # Weak aliases export the ROM-visible name at the impl's address (e.g. cosf/sinf).
            for m in PRAGMA_WEAK_RE.finditer(text):
                index.setdefault(m.group(1), (lib, cpath))
    return index


@functools.lru_cache(maxsize=1)
def build_asm_tu_index():
    """Map hand-asm function name -> its .s path (relative to the libultra source tree).

    Scans the ultralib `.s` TUs (LEAF/XLEAF entries) so an `intrinsic-likely` shim can name
    the vendorable upstream asm TU it mirrors (see docs/hazards.md#asm-mirror-vendoring), the
    asm analog of build_upstream_index's C map. A bare `intrinsic-likely` (no detail) then
    means a genuine no-source shim; `intrinsic-likely:os/getcount.s` means a vendorable TU."""
    index = {}
    if os.path.isdir(LIBULTRA):
        for spath in glob.glob(os.path.join(LIBULTRA, "**", "*.s"), recursive=True):
            try:
                with open(spath, errors="ignore") as f:
                    text = f.read()
            except OSError:
                continue
            for m in ASM_TU_DEF_RE.finditer(text):
                index.setdefault(m.group(1), os.path.relpath(spath, LIBULTRA))
    return index


@functools.lru_cache(maxsize=1)
def build_kmc_asm_tu_index():
    """Map libkmc hand-asm function name -> its .s basename (the KMC-as asm-mirror lane).

    The libkmc analog of build_asm_tu_index. libkmc soft-float / 64-bit math TUs (mmuldi3.s,
    mcvtld.s) vendor verbatim via the KMC `as` path (KMC register conventions + `.include
    "mips_as.h"`), a distinct mechanism from ultralib's LIBULTRA_ASFLAGS rule — they build under the
    path-based `build/src/libkmc/%.o: src/libkmc/%.s` KMC-`as` pattern rule. A primary
    whose name matches here gets `intrinsic-likely:<tu>.s(kmc-as)` so the gate vendors it with the
    KMC-as recipe (docs/hazards.md#asm-mirror-vendoring). Basename only: a libkmc TU's enabler
    surface differs from ultralib's, so it must NOT feed the LIBULTRA-relative vendorable_tu_*
    helpers. Only the asm-ONLY libkmc TUs are the target set — a libkmc fn with a C source
    (memset/strcmp/rand) resolves via the C upstream index and is excluded at the emission site."""
    index = {}
    if os.path.isdir(LIBKMC):
        for spath in glob.glob(os.path.join(LIBKMC, "*.s")):
            try:
                with open(spath, errors="ignore") as f:
                    text = f.read()
            except OSError:
                continue
            for m in KMC_ASM_TU_DEF_RE.finditer(text):
                index.setdefault(m.group(1), os.path.basename(spath))
    return index


@functools.cache
def _intree_asm_macros():
    """Every `#define`d name reachable under the vendored-asm `-I` set (ASM_VENDOR_INCLUDE_DIRS).

    Cached. The denominator for vendorable_tu_missing_defines: a macro a vendored .s references but
    that appears in none of these headers can't assemble under LIBULTRA_ASFLAGS."""
    macros = set()
    for d in ASM_VENDOR_INCLUDE_DIRS:
        for h in glob.glob(os.path.join(d, "**", "*.h"), recursive=True):
            try:
                macros.update(
                    re.findall(r"#define\s+(\w+)", open(h, errors="ignore").read())
                )
            except OSError:
                continue
    return frozenset(macros)


def vendorable_tu_missing_defines(rel):
    """Sorted UPPER_CASE macros a vendorable ultralib .s (path relative to LIBULTRA) references but
    that the in-tree asm `-I` headers don't define — a needs-define enabler the gate must satisfy
    before vendoring. Strips C comments + `#include` lines first so a `/* TLB */`
    comment word or the `R4300` in an `#include "PR/R4300.h"` path can't false-flag. Empty for a
    self-contained TU (verified empty for the whole current vendoring backlog — all cache/TLB/gu
    macros ship in include/libultra/PR/R4300.h, RDB_* in PR/rdb.h)."""
    if not rel:
        return []
    spath = os.path.join(LIBULTRA, rel)
    try:
        text = open(spath, errors="ignore").read()
    except OSError:
        return []
    text = ASM_INCLUDE_LINE_RE.sub("", C_COMMENT_RE.sub("", text))
    defined = _intree_asm_macros()
    # A macro the .s `#define`s itself (e.g. setintmask.s's `#define MI_INTR_MASK ...`, often unused)
    # is not a missing enabler — subtract the TU's own defines before flagging.
    local = set(re.findall(r"#define\s+(\w+)", text))
    return sorted({t for t in MACRO_REF_RE.findall(text)} - defined - local)


def vendorable_tu_data_symbols(rel):
    """Exported symbols a vendorable ultralib .s (path relative to LIBULTRA) defines in a NON-.text
    section (.rdata/.rodata/.data/.sdata/.bss) — a has-rodata enabler. A vendored .s with
    such a section is NOT a clean .text-only mirror cp: splat auto-links a hasm .o's data
    sections at the END of each output section (out of address order), so the bytes duplicate +
    misplace -> SHA break. The fix (docs/hazards.md#asm-mirror-vendoring) vendors .text only +
    strips the data block, keeping that data as the existing extracted generic blob renamed to this
    symbol via a `symbol_addrs` add. Flagging it prices the strip+rename enabler at the gate rather
    than discovering it at a failing vendor-build. Empty for a
    .text-only TU (the whole current vendoring backlog)."""
    if not rel:
        return []
    spath = os.path.join(LIBULTRA, rel)
    try:
        text = open(spath, errors="ignore").read()
    except OSError:
        return []
    text = C_COMMENT_RE.sub("", text)
    # Scan only the ACTIVE data sections. A `#ifndef _FINALROM` / version-gated EXPORT
    # (e.g. exceptasm.s's `__osCauseTable_pt` lives in `#ifndef _FINALROM`) is NOT emitted under
    # MG64's `-D_FINALROM -DBUILD_VERSION=VERSION_J` asm profile, so listing it over-counts the
    # rodata-strip enabler. Drop the dead + inactive-version blocks before the section scan so the
    # priced has-rodata set is what the vendored `.o` actually carries, not the all-branches union.
    text = _strip_inactive_version_branches(
        _strip_dead_blocks(text), _build_version_ord("libultra")
    )
    m = re.search(r"^\s*\.(rdata|rodata|data|sdata|bss|sbss)\b", text, re.M)
    if not m:
        return []
    tail = text[m.end() :]
    syms = re.findall(r"(?:EXPORT|XLEAF|glabel|dlabel)\s*\(?\s*(\w+)", tail)
    syms += re.findall(r"\.globl\s+(\w+)", tail)
    return sorted(set(syms))


# `.word` operand that is a label reference (not a numeric literal / arithmetic). A symbolic-pointer
# table — its operands are identifiers, optionally with +/- displacement.
_WORD_LABEL_OPERAND_RE = re.compile(r"^[A-Za-z_]\w*(?:\s*[+-]\s*\w+)?$")


def vendorable_tu_jtbl(rel):
    """Sorted heads of any SYMBOLIC-pointer table (`.word <label>, …`) a vendorable ultralib `.s`
    defines in its ACTIVE `.rdata`/`.rodata`/`.data` section. Such a table — a switch jump table
    (`__osIntTable: .word redispatch, sw1, …`, the active `__osException` `jr`s through it) or
    any function-pointer table — is what makes a `.text`-only asm-mirror a SPIKE, not a clean
    strip-and-rename: splat extracts the table to a SEPARATE rodata blob with SYMBOLIC `.word .L…`
    entries pointing at `.text`-internal labels; vendoring the `.text`-only ultralib `.s` (which does
    not define splat's `.L<addr>` labels) leaves the blob's refs UNDEFINED at link, and the table can't
    be carve-placed either (a hasm `.o`'s rodata auto-links at the section END → wrong addr → SHA
    break). The fix re-exports the labels from the vendored `.text`. So this is the negative signal: a
    heavy asm-mirror like exceptasm is NOT the has-rodata replay its `data_symbols` flag suggests.
    Numeric tables (`__osHwIntTable: .word 0, 0`) are NOT flagged (they strip-and-rename cleanly).
    See docs/hazards.md#asm-mirror-vendoring. Empty for a `.text`-only or numeric-data TU."""
    if not rel:
        return []
    spath = os.path.join(LIBULTRA, rel)
    try:
        text = open(spath, errors="ignore").read()
    except OSError:
        return []
    text = C_COMMENT_RE.sub("", text)
    text = _strip_inactive_version_branches(
        _strip_dead_blocks(text), _build_version_ord("libultra")
    )
    m = re.search(r"^\s*\.(rdata|rodata|data|sdata)\b", text, re.M)
    if not m:
        return []
    heads, cur = [], None
    for line in text[m.end() :].splitlines():
        s = line.strip()
        if not s:
            continue
        lbl = re.match(r"(?:EXPORT|XLEAF|glabel|dlabel)\s*\(?\s*(\w+)|(\w+)\s*:", s)
        if lbl:
            cur = lbl.group(1) or lbl.group(2)
            continue
        mw = re.match(r"\.word\s+(.+)$", s)
        if mw and any(
            _WORD_LABEL_OPERAND_RE.match(op.strip()) for op in mw.group(1).split(",")
        ):
            if cur:
                heads.append(cur)
    return sorted(set(heads))


# --- Signature matcher (un-named func_<vram> mirror detection) -----------------------
#
# build_upstream_index() is keyed on the curated *name*, so an SDK function that splat left
# as `func_<vram>` (no Ghidra name yet) never maps — it falls out as `upstream: none` and gets
# mislabeled a classical leaf. That mislabel is what burned Sprint 13 (osViSetYScale and
# __osPfsSelectBank both surfaced as "classical"). The fix, modeled on coddog's register-
# stripped opcode signature but done WITHOUT compiling the SDK trees: compare the target's
# *callee set* (its `jal` targets, already symbolic in the asm) against every SDK source
# function's call set, weighted by inverse document frequency. A rare callee (__osContRamWrite,
# called by a handful of pfs fns) is near-decisive; a common one (the __osDisableInt/
# __osRestoreInt int-disable pair, in dozens of fns) contributes little — so a unique-callee
# target gets one confident hit and a common-callee target (the vi setters) gets a correctly
# *ambiguous* short list. The result is advisory: a `maybe-upstream:<lib>:<files>` hazard that
# tells the gate to asm-vs-upstream-check before committing a classical framing — it never
# silently reclassifies. Constants are a weak secondary signal (SDK C uses macros, not raw
# numbers), so callees carry the verdict.

# C identifiers used as call targets: `name(`. Excludes control keywords (see _C_NONCALL).
# (The asm-side CALL_INSN_RE/IMM_INSN_RE + _asm_signature live in decomp_asm; _FRAME_IMMS is
# imported from there so both sides drop the same stack-frame offsets.)
C_CALL_RE = re.compile(r"\b([A-Za-z_]\w+)\s*\(")
C_INT_RE = re.compile(r"\b(0x[0-9A-Fa-f]+|\d+)\b")
_C_NONCALL = {
    "if",
    "for",
    "while",
    "switch",
    "return",
    "sizeof",
    "do",
    "else",
    "case",
    "defined",
    "OS_LOG_FLOAT",
    "assert",
    # Inline predicate macros that expand to field comparisons, emitting NO jal — counting them
    # as C calls over-inflates the jal-count C side (a `MQ_IS_FULL` reads as a call but is none) and
    # pollutes the maybe-upstream callee signature. They carry no cross-build identity.
    "MQ_IS_FULL",
    "MQ_IS_EMPTY",
    # GCC attribute artifacts surfaced by macro_hidden_text: the macros.h `ALIGNED(x)` /
    # `STACK(...)` family expand to `__attribute__((aligned(x)))`, so the lowercase `aligned`
    # attribute keyword (and `__attribute__` itself) read as `name(` call tokens and mis-flag as
    # an unplaced callee (`static STACK(...) ALIGNED(0x10)` → phantom `calls-unplaced:aligned`).
    # The macro NAMES (ALIGNED/STACK/ARRLEN/ALIGN8) are already dropped via macro_names; these are
    # their expansion residue, which macro_names does not cover.
    "aligned",
    "__attribute__",
}


def _c_signature(body):
    """(callee set, constant set) for an SDK function body (C source)."""
    callees = {n for n in C_CALL_RE.findall(body) if n not in _C_NONCALL}
    consts = {c.lower() for c in C_INT_RE.findall(body) if c.lower() not in _FRAME_IMMS}
    return callees, consts


_NONCODE_RE = re.compile(
    r"//[^\n]*"  # line comment
    r"|/\*.*?\*/"  # block comment
    r'|"(?:\\.|[^"\\])*"'  # string literal
    r"|'(?:\\.|[^'\\])*'"  # char literal
    r"|(?m:^[ \t]*\#(?:[^\n]*\\\n)*[^\n]*)",  # preprocessor directive (+ `\` continuations)
    re.S,
)


def _blank_noncode(text):
    """Return `text` with comments, string/char literals, and preprocessor-directive lines replaced by
    spaces, PRESERVING length + newlines so offsets map back to the original. Lets the def scanner
    ignore `(`/`{`/`;` inside a comment, a `"name("` literal, or a `#define PUT(s,n)` macro header,
    while the caller still slices each function body from the UNMODIFIED text (directives intact, which
    call_divergence's dead-block scan needs — blanking them in the body would re-count a dead
    `#ifdef NU_DEBUG` osSyncPrintf as a phantom `jal-count-mismatch`)."""
    return _NONCODE_RE.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), text)


def _iter_upstream_functions(text):
    """Yield (name, body) for each top-level (depth-0) function DEFINITION in an SDK .c, body =
    brace-matched from the ORIGINAL text. A self-contained depth-aware scan over a length-preserving
    blanked copy (NOT the column-anchored UPSTREAM_DEF_RE, which stays conservative for the symbol
    index at build_upstream_index): driven by depth-0 `(`, so it handles every def shape uniformly and
    never miscounts:
      - ANSI `T name(params) {`, K&R `T name(params) decls; {`, AND single-token implicit-int K&R
        `name(params) {` (libkmc `_xatan(u,v,atanp)`, which the two-token regex could not split);
      - leading-indented headers (nusys ` void nuGfxTaskStart(...)`, a stray-space top-level def the
        column-0 anchor missed → an under-count that mis-fired upstream-fncount-mismatch);
      - depth-awareness excludes an in-body call (`foo(x)` at depth>=1) that the broadened match would
        otherwise mistake for a def — the reason UPSTREAM_DEF_RE stays column-anchored elsewhere;
      - the blanked copy hides a doc-comment signature (`double atan(double)` in atan.c's banner), a
        `"name("` literal, and a `#define MACRO(x)` header from the scan.
    The name = the identifier immediately before the `(`; a control/type/qualifier keyword there
    (`if (`, `int (*tbl[])(...)`) is not a function name (_DEF_NONNAME_KW). A forward declaration
    (`)` then `;`) is skipped. The body's braces are consumed in one pass so depth stays correct."""
    scan = _blank_noncode(text)
    n = len(scan)
    i = depth = 0
    while i < n:
        c = scan[i]
        if c == "{":
            depth += 1
            i += 1
            continue
        if c == "}":
            depth = max(0, depth - 1)
            i += 1
            continue
        if depth != 0 or c != "(":
            i += 1
            continue
        # A depth-0 '(': the identifier ending just before it is the candidate function name.
        j = i - 1
        while j >= 0 and scan[j] in " \t\r\n":
            j -= 1
        k = j
        while k >= 0 and (scan[k].isalnum() or scan[k] == "_"):
            k -= 1
        name = scan[k + 1 : j + 1]
        if (
            not name
            or not (name[0].isalpha() or name[0] == "_")
            or name in _DEF_NONNAME_KW
        ):
            i += 1
            continue
        # Match this param-list ')'.
        d2, p = 0, i
        while p < n:
            if scan[p] == "(":
                d2 += 1
            elif scan[p] == ")":
                d2 -= 1
                if d2 == 0:
                    break
            p += 1
        if p >= n:
            break
        q = p + 1
        while q < n and scan[q] in " \t\r\n":
            q += 1
        if (
            q < n and scan[q] == ";"
        ):  # `T name(...);` forward declaration / prototype — no body
            i = p + 1
            continue
        brace = scan.find("{", p)
        if brace < 0:
            break
        d3, b = 0, brace
        while b < n:
            if scan[b] == "{":
                d3 += 1
            elif scan[b] == "}":
                d3 -= 1
                if d3 == 0:
                    break
            b += 1
        yield name, text[brace : b + 1]
        i = b + 1  # skip the consumed body so depth stays 0


@functools.cache
def _upstream_file_func_count(lib, cpath):
    """Count top-level function DEFINITIONS in an upstream .c (version-active branches only).

    Used by the single-file-pack test to recognize a pack whose trailing member(s) are unnamed
    (`func_<addr>`): if the pack's one named C stem defines exactly nfns functions, the unnamed
    members are that file's other functions, so the pack atomic-mirrors the whole file (the gu
    F-variant + s16-wrapper idiom: `guAlignF` named + `func_<addr>` = `guAlign`, both in align.c).
    Conservative — an exact count match only; a mismatch falls back to the `pack` flag."""
    try:
        with open(cpath, errors="ignore") as f:
            text = f.read()
    except OSError:
        return 0
    text = _strip_inactive_version_branches(text, _build_version_ord(lib))
    return sum(1 for _ in _iter_upstream_functions(text))


@functools.cache
def _meaningful_loc(cpath):
    """Count non-blank, non-comment, non-preprocessor source lines of an upstream .c — a crude proxy
    for its compiled .text size, used by the coddog-structural size-ratio guard. Line-based
    (blanks, `//`/`*`/`/*` comment lines, and `#`-directives dropped); a multi-line block comment's
    body lines start `*` and are skipped, so the proxy errs low (safe: a low LOC only RAISES the
    bytes/LOC ratio toward the threshold, never masks a real mismatch). Returns 0 on read failure."""
    try:
        with open(cpath, errors="ignore") as f:
            text = f.read()
    except OSError:
        return 0
    n = 0
    for line in text.splitlines():
        s = line.strip()
        if not s or s.startswith(("//", "#", "*", "/*")):
            continue
        n += 1
    return n


# A dot-prefixed ld-section sibling line, e.g. `- [0xA35D0, .data, libultra/gu/rotate]`. The shared
# SUBSEG_RE types as `[a-z]+` (no leading dot), so parse_subsegs SKIPS these — scan them directly.
_LD_SIBLING_RE = re.compile(
    r"^\s*-\s*\[\s*0x[0-9A-Fa-f]+\s*,\s*(\.(?:data|rodata|bss))\s*,\s*([^\]]+?)\s*\]"
)


@functools.lru_cache(maxsize=1)
def _static_carve_siblings():
    """{yaml_dir: {".data"/".rodata"/".bss": {basename,...}}} for every ld-section sibling subseg —
    the dirs already holding a banked file with a proven static carve. Feeds the twin-of hint:
    a candidate that re-emits a function-local static (data-static / rodata-literal) whose mirror
    dir already carved the same section type has an established playbook (align.c was the verbatim
    twin of rotate.c — same `libultra/gu` dir, same `.data` dtor carve)."""
    out = {}
    with open(YAML) as f:
        for line in f:
            m = _LD_SIBLING_RE.match(line)
            if not m:
                continue
            d, b = os.path.split(m.group(2).strip())
            out.setdefault(d, {".data": set(), ".rodata": set(), ".bss": set()})[
                m.group(1)
            ].add(b)
    return out


def _load_coddog_map(path):
    """Parse a coddog cross-ref map (mgname<TAB>name<TAB>file<TAB>pct) -> {mgname: (file, pct)}.
    Absent/garbled lines are skipped; an absent file returns {} (ranking unchanged). A coddog hit is
    an actual instruction-hash match — definitive enough to re-price a none-classified verbatim
    mirror (see build_rows). Shared by the libultra (build_coddog_index) and libnusys
    (build_coddog_nusys_index) loaders."""
    idx = {}
    try:
        with open(path, errors="ignore") as f:
            for line in f:
                parts = line.rstrip("\n").split("\t")
                if len(parts) != 4:
                    continue
                mg, _name, cfile, pct = parts
                try:
                    idx[mg] = (cfile, float(pct))
                except ValueError:
                    continue
    except OSError:
        return {}
    return idx


def build_coddog_index():
    """Load the optional libultra coddog cross-ref map -> {mgname: (ulfile, pct)} (written by
    tools/coddog_sweep.sh; format mgname<TAB>ulname<TAB>ulfile<TAB>pct)."""
    return _load_coddog_map(CODDOG_MAP)


def build_coddog_nusys_index():
    """Load the optional nusys coddog cross-ref map -> {mgname: (cfile, pct)} — the libnusys analog
    of build_coddog_index, written by tools/nusys_sweep.sh. cfile is `mainlib/<base>.c`, resolved by
    _coddog_nusys_path; absent file -> {} (ranking unchanged)."""
    return _load_coddog_map(NUSYS_CODDOG_MAP)


def _coddog_upstream_path(cfile):
    """Resolve a coddog map `ulfile` (e.g. `src/io/piacs.c`, repo-root-relative) to an on-disk
    path, or None. The map's paths are relative to the ultralib repo root; LIBULTRA is `<root>/src`.
    Used by build_rows to re-run the file-level trap detectors on a coddog-matched upstream."""
    p = os.path.join(os.path.dirname(LIBULTRA), cfile)
    return p if os.path.isfile(p) else None


def _coddog_nusys_path(cfile):
    """Resolve a nusys coddog map `cfile` (`mainlib/<base>.c`, nusys-2.07-src-relative) to an on-disk
    path, or None. The libnusys analog of _coddog_upstream_path; the 2.07 source lives under
    `<LIBNUSYS>/nusys/src/`."""
    p = os.path.join(LIBNUSYS, "nusys/src", cfile)
    return p if os.path.isfile(p) else None


def build_audio_indexes():
    """Load the optional audio coddog maps -> {lib: {mgname: (cfile, pct)}} for each of
    AUDIO_CODDOG_LIBS (libmus / libnaudio / nuaulstl), written by tools/audio_sweep.sh and pinned by
    tools/audio_pin.py. Each `cfile` is a source basename resolved by _coddog_audio_path against the
    pinned root. An absent map -> {} for that lib (ranking unchanged)."""
    return {lib: _load_coddog_map(path) for lib, path in AUDIO_CODDOG_MAPS.items()}


def build_audio_pin_roots():
    """Parse tools/coddog/audio_pins.tsv -> {lib: pinned_srcdir}. audio_pin.py records the winning
    matrix cell's source root per pinned lib; _coddog_audio_path joins map basenames against it.
    Absent file -> {} (the audio resolver then can't resolve a path and skips its trap re-scan, but
    still flags the coddog-mirror hit)."""
    roots = {}
    try:
        with open(AUDIO_PINS, errors="ignore") as f:
            for line in f:
                if line.lstrip().startswith("#") or not line.strip():
                    continue
                parts = line.rstrip("\n").split("\t")
                if len(parts) >= 5:
                    roots[parts[0]] = os.path.expanduser(parts[4])
    except OSError:
        return {}
    return roots


def _coddog_audio_path(cfile, roots, lib):
    """Resolve an audio coddog map `cfile` (a source basename) to an on-disk path under the pinned
    `lib` root (from build_audio_pin_roots), or None. The libmus/libnaudio/nuaulstl analog of
    _coddog_nusys_path."""
    root = roots.get(lib)
    if not root:
        return None
    p = os.path.join(root, cfile)
    return p if os.path.isfile(p) else None


def _coddog_source_banked(cod_src):
    """True if a coddog-matched source's project mirror is ALREADY banked (0-stub) in-tree. A
    coddog-mirror hit to such a file can't be a fresh source attribution (the source is fully
    decompiled), so the match is necessarily structural — a DSP/stub fingerprint coincidence, the
    sibling of coddog-fncount-mismatch / coddog-structural. `cod_src` is the source path the coddog
    map carries: a nusys hit (`mainlib/<base>.c`, upstream-relative) mirrors at the FLAT
    `src/libnusys/<base>.c` (the in-tree tree drops the upstream `mainlib/` dir); a libultra hit
    (`src/audio/load.c`, ultralib-repo-relative) mirrors at `src/libultra/<reldir>/<base>`."""
    if cod_src.startswith("mainlib/"):
        mp = os.path.join(ROOT, "src", "libnusys", cod_src[len("mainlib/") :])
    else:
        rel = cod_src[len("src/") :] if cod_src.startswith("src/") else cod_src
        mp = os.path.join(ROOT, "src", "libultra", rel)
    try:
        with open(mp, errors="ignore") as f:
            return "INCLUDE_ASM" not in f.read()
    except OSError:
        return False


def _append_coddog_twin_hazard(cfile, fns, upstream_index, hazards):
    """coddog (compare2, reloc-masked) can match a near-identical TWIN file rather than the
    candidate's real source -- the SI access-queue subseg coddog-matched `src/io/piacs.c@99.99`,
    but its named members name `siacs`. Cross-check the coddog-matched basename against the pack's
    *named* members' upstream basenames; on disagreement flag `coddog-twin:<matched>!=<member-src>`
    so the gate mirrors from the member-named source (siacs.c), not the coddog file (piacs.c). A
    @99.99 twin is byte-identical once placed, so the body is the same either way -- this only
    removes a manual SI/PI reconcile step. No-ops when no member is named (nothing to disagree with)
    or the basenames agree."""
    cstem = os.path.splitext(os.path.basename(cfile))[0]
    member_stems = []
    for fn in fns:
        _, fn_up = upstream_index.get(fn, (None, None))
        if fn_up:
            s = os.path.splitext(os.path.basename(fn_up))[0]
            if s not in member_stems:
                member_stems.append(s)
    if member_stems and cstem not in member_stems:
        hazards.append(Hazard.coddog_twin(cstem, member_stems))


def build_signature_index():
    """All SDK functions as (lib, basename, name, callees, consts) + the callee document
    frequency (how many SDK fns call each name) for IDF weighting. One pass over the trees."""
    entries = []
    df = {}
    for lib, tree in UPSTREAM_TREES:
        if not os.path.isdir(tree):
            continue
        for cpath in glob.glob(os.path.join(tree, "**", "*.c"), recursive=True):
            try:
                with open(cpath, errors="ignore") as f:
                    text = f.read()
            except OSError:
                continue
            base = os.path.splitext(os.path.basename(cpath))[0]
            for name, body in _iter_upstream_functions(text):
                callees, consts = _c_signature(body)
                callees.discard(name)  # ignore self-recursion
                if not callees:
                    continue
                entries.append((lib, base, name, callees, consts))
                for c in callees:
                    df[c] = df.get(c, 0) + 1
    return entries, df


def signature_hint(rom_off, primary, sig_index):
    """Advisory `maybe-upstream:<lib>:<f1>[,<f2>…]` hazard for an un-named candidate, or None.
    Scores each SDK fn by IDF-weighted shared-callee mass over the target's total callee mass;
    a small constant-overlap bonus breaks ties. Returns the top matches above a confidence
    floor — the gate confirms by reading the upstream (never a silent reclassification)."""
    entries, df = sig_index
    t_callees, t_consts = _asm_signature(rom_off, primary)
    if not t_callees:
        return None  # pure leaf / only internal calls — not signature-able this way
    n_docs = max(len(entries), 1)
    weight = {c: math.log(1 + n_docs / (1 + df.get(c, 0))) for c in t_callees}
    total = sum(weight.values()) or 1.0
    scored = []
    for lib, base, name, callees, consts in entries:
        shared = t_callees & callees
        if not shared:
            continue
        score = sum(weight[c] for c in shared) / total
        if t_consts and consts:
            score += 0.15 * len(t_consts & consts) / len(t_consts)
        scored.append((score, lib, base, name))
    if not scored:
        return None
    scored.sort(reverse=True)
    best = scored[0][0]
    if best < 0.5:
        return None  # too weak to be worth the gate's attention
    # Keep candidates within 85% of the top score (the ambiguous-tie short list).
    lib = scored[0][1]
    bases, seen = [], set()
    for score, l, base, name in scored:
        if l != lib or score < best * 0.85:
            continue
        if base not in seen:
            seen.add(base)
            bases.append(base)
        if len(bases) >= 3:
            break
    return Hazard.maybe_upstream(lib, bases)


# --- Upstream-vs-ROM call-divergence (the nuContInit catch) ----------------------
#
# A mapped upstream .c can disagree with THIS ROM's build: nuContInit's upstream calls four
# managers, but the ROM's asm has only three jals (no nuContPakMgrInit) — a verbatim mirror
# would emit an extra call and never match. That is not a missing symbol (refs_unplaced) nor a
# missing definition (defines_data_globals); it is a build divergence. Flag it advisory so the
# gate reconciles the upstream call list against the ROM's jals before declaring a clean mirror
# (and routes it to the near-verbatim "drop-one-line" mirror sub-case). Count-based, not
# set-based: counting raw jals (symbolic AND func_) sidesteps the symbolization mismatch a
# name-set diff would hit. Dead `#ifdef _DEBUG` / `#ifndef _FINALROM` / `#if 0` blocks are
# stripped first (their debug calls compile out of the real build); macro/inlined calls can
# still skew the count, so it stays advisory — the gate confirms by disassembling.

_IFDEF_OPEN_RE = re.compile(r"^\s*#\s*ifdef\s+([A-Za-z_][A-Za-z0-9_]*)")
_MAKEFILE_DEFINE_RE = re.compile(r"-D([A-Za-z_][A-Za-z0-9_]*)(?:=[^\s]*)?")

# BUILD_VERSION config + version-conditional stripping moved to build_config.py (the shared leaf
# both the C-side and asm-TU scanners use); _build_version_ord / _strip_inactive_version_branches
# and the shared PP/BUILD_VERSION regexes are re-imported at the top of this module.

_DEFINE_VERSION_RE = re.compile(r"^\s*#\s*define\s+(VERSION_[A-Z])\b")


@functools.cache
def _os_version_defined_tokens(lib):
    """The VERSION_<X> tokens the os_version.h resolvable on a `lib` candidate's effective -I set
    actually `#define`s. The denominator for stale_version_header: the in-tree os_version.h can
    resolve as a FILE (so needs-header stays silent) yet be a stripped revision that defines none of
    the VERSION_* constants the gcc.mk profile expects (e.g. a stripped 2.0L header). Empty frozenset
    when no os_version.h resolves at all."""
    search_dirs = INCLUDE_DIRS + LIB_EXTRA_INCLUDE_DIRS.get(lib or "", [])
    for d in search_dirs:
        p = os.path.join(d, "os_version.h")
        if os.path.exists(p):
            toks = set()
            with open(p, errors="ignore") as f:
                for line in f:
                    m = _DEFINE_VERSION_RE.match(line)
                    if m:
                        toks.add(m.group(1))
            return frozenset(toks)
    return frozenset()


def stale_version_header(up_path, lib):
    """Sorted VERSION_<X> tokens an upstream file's `#if BUILD_VERSION <op> VERSION_X` guards
    reference but the in-tree os_version.h (on the candidate's -I set) does NOT `#define` — so cpp
    reads each as 0 and silently mis-evaluates the guard (e.g. gu/mtxcatl.c's `#if BUILD_VERSION <
    VERSION_K` evaluates `0 < 0` against a stripped 2.0L os_version.h and drops guMtxXFML to a link
    error). A `stale-header:os_version.h(VERSION_K)` hazard = vendor the missing constant(s) into
    os_version.h before the flip (additive; clean-rebuild-verify since every other guard compares the
    -DBUILD_VERSION token and evaluates identically). Distinct from needs-header: the header file
    exists and resolves — the gap is its CONTENT. Returns [] when the lib sets no -DBUILD_VERSION
    (its guards don't gate on VERSION_*) or every referenced token is defined."""
    if not _build_version_ord(lib):
        return []
    refs = set()
    try:
        text = UpstreamSource.get(up_path).text
    except OSError:
        return []
    for line in text.splitlines():
        m = _BUILD_VERSION_IF_RE.match(line.lstrip())
        if m:
            refs.add(m.group(2))
    if not refs:
        return []
    defined = _os_version_defined_tokens(lib)
    return sorted(t for t in refs if t not in defined)


@functools.cache
def _parse_makefile_defines():
    """Return {lib: frozenset(defines)} by parsing the project build files (cached:
    one parse per process). The base CFLAGS live in the top Makefile; the per-library
    profiles live in the included mk/*.mk fragments, so scan both.
    'libkmc'/'libnusys'/'libultra' each inherit from the main CFLAGS set plus their own
    profile additions. The libultra profile carries -DBUILD_VERSION + -DF3DEX_GBI_2
    — without parsing LIBULTRA_CFLAGS the GBI microcode define is invisible, so a
    GBI-value-guarded macro (OS_YIELD_DATA_SIZE) false-flags as needs-define on a build
    that already defines it."""
    build_files = [os.path.join(ROOT, "Makefile")] + sorted(
        glob.glob(os.path.join(ROOT, "mk", "*.mk"))
    )
    raw = {"main": set(), "libkmc": set(), "libnusys": set(), "libultra": set()}
    for path in build_files:
        try:
            with open(path) as f:
                for line in f:
                    for var, key in [
                        ("CFLAGS", "main"),
                        ("LIBKMC_CFLAGS", "libkmc"),
                        ("LIBNUSYS_CFLAGS", "libnusys"),
                        ("LIBULTRA_CFLAGS", "libultra"),
                    ]:
                        if re.match(rf"^\s*{var}\s*[:+]?=", line):
                            raw[key].update(
                                m.group(1) for m in _MAKEFILE_DEFINE_RE.finditer(line)
                            )
        except OSError:
            continue
    for k in ("libkmc", "libnusys", "libultra"):
        raw[k] |= raw["main"]
    return {k: frozenset(v) for k, v in raw.items()}


def _active_defines_for_lib(lib):
    """Effective -D defines for an upstream library key (libultra, libkmc, libnusys)."""
    key = {"libkmc": "libkmc", "libnusys": "libnusys", "libultra": "libultra"}.get(
        lib or "", "main"
    )
    return _parse_makefile_defines()[key]


def function_gating_define(up_cpath, primary):
    """If `primary`'s entire body is wrapped by a single top-level `#ifdef DEFINE`, return
    DEFINE; else return None.  Detects build-define gates like USE_EPI where the fn compiles
    to an empty stub without the define — distinct from inner feature-flag conditionals."""
    body = _upstream_body(up_cpath, primary)
    if body is None:
        return None
    lines = [
        l
        for l in body[1:-1].splitlines()
        if l.strip() and not l.lstrip().startswith("//")
    ]
    if not lines:
        return None
    m = _IFDEF_OPEN_RE.match(lines[0])
    if not m:
        return None
    define = m.group(1)
    depth, matching_i = 0, None
    for i, line in enumerate(lines):
        s = line.strip()
        if _PP_OPENING_DIRECTIVE_RE.match(s):
            depth += 1
        elif _PP_ENDIF_LINE_RE.match(s):
            depth -= 1
            if depth == 0:
                matching_i = i
                break
    if matching_i is None:
        return None
    if any(l.strip() for l in lines[matching_i + 1 :]):
        return None
    return define


# GBI microcode macros that gate object-like macro VALUES in PR/sptask.h (OS_YIELD_DATA_SIZE,
# OS_YIELD_AUDIO_SIZE). ultralib builds libgultra_rom with a global -DF3DEX_GBI default; MG64 runs
# F3DEX2, so the project pins -DF3DEX_GBI_2 in LIBULTRA_CFLAGS. With NONE defined the macro
# silently takes the #else value, mismatching the ROM by one word (sptask's
# IO_READ(...+OS_YIELD_DATA_SIZE-4): 0x900 vs the baserom's 0xc00) — invisible to the gate stub.
GBI_MICROCODE_DEFINES = ("F3D_GBI", "F3DEX_GBI", "F3DLP_GBI", "F3DEX_GBI_2")
_OBJ_DEFINE_RE = re.compile(r"^\s*#\s*define\s+([A-Za-z_]\w*)\s+(\S.*?)\s*$")
_GBI_COND_RE = re.compile(
    r"defined\s*\(\s*([A-Za-z_]\w*)\s*\)|\bifdef\s+([A-Za-z_]\w*)"
)


def _scan_value_guards(text):
    """Object-like macros in `text` whose value is gated by a GBI-microcode `#if`/`#ifdef` block
    with a DIFFERENT `#else` value. Returns {macro: tuple(guard_defines)}. The PR/sptask.h shape:
        #if (defined(F3DEX_GBI)||defined(F3DLP_GBI)||defined(F3DEX_GBI_2))
        #define OS_YIELD_DATA_SIZE 0xc00
        #else
        #define OS_YIELD_DATA_SIZE 0x900
        #endif
    """
    lines = text.splitlines()
    n, i, out = len(lines), 0, {}
    while i < n:
        s = lines[i].strip()
        if re.match(r"^#\s*if", s):
            guard = tuple(
                d
                for grp in _GBI_COND_RE.findall(s)
                for d in grp
                if d and d in GBI_MICROCODE_DEFINES
            )
            if guard:
                if_vals, else_vals, cur, depth, j = {}, {}, None, 1, i + 1
                cur = if_vals
                while j < n and depth > 0:
                    t = lines[j].strip()
                    if re.match(r"^#\s*if", t):
                        depth += 1
                    elif re.match(r"^#\s*endif", t):
                        depth -= 1
                        if depth == 0:
                            break
                    elif depth == 1 and re.match(r"^#\s*else", t):
                        cur = else_vals
                    elif depth == 1:
                        md = _OBJ_DEFINE_RE.match(lines[j])
                        if md:
                            cur[md.group(1)] = md.group(2)
                    j += 1
                for name, v1 in if_vals.items():
                    if else_vals.get(name, v1) != v1:
                        out[name] = guard
                i = j
        i += 1
    return out


@functools.cache
def _gbi_guarded_macros(lib):
    """{macro: tuple(guard_defines)} for GBI-value-guarded object-like macros under `lib`'s effective
    -I set (INCLUDE_DIRS + LIB_EXTRA_INCLUDE_DIRS[lib]). Cached per lib (one header walk per run)."""
    out = {}
    for d in INCLUDE_DIRS + LIB_EXTRA_INCLUDE_DIRS.get(lib or "", []):
        if not os.path.isdir(d):
            continue
        for root, _dirs, files in os.walk(d):
            for fn in files:
                if not fn.endswith((".h", ".inc")):
                    continue
                try:
                    text = open(os.path.join(root, fn), errors="ignore").read()
                except OSError:
                    continue
                out.update(_scan_value_guards(text))
    return out


def gbi_value_guard_needs_define(cpath, lib):
    """The GBI-microcode define a mirror candidate needs because its body uses a GBI-value-guarded
    macro (OS_YIELD_DATA_SIZE) and NO guard define is active for `lib` — else None. Returns the
    canonical F3DEX_GBI_2 when it satisfies the guard, else the first guard define. Dormant for
    libultra now (the standing -DF3DEX_GBI_2 resolves the value), but prices a candidate guarded by
    an inactive define so the 1-word SHA-miss is paid at the gate, not in execution."""
    guarded = _gbi_guarded_macros(lib)
    if not guarded:
        return None
    try:
        body = _strip_comments(UpstreamSource.get(cpath).text)
    except OSError:
        return None
    active = _active_defines_for_lib(lib)
    used = set(re.findall(r"[A-Za-z_]\w*", body))
    for name, guard in guarded.items():
        if name in used and not (set(guard) & active):
            return "F3DEX_GBI_2" if "F3DEX_GBI_2" in guard else guard[0]
    return None


def _upstream_body(up_cpath, primary):
    """The brace-matched body of `primary` in its upstream .c, or None."""
    try:
        text = UpstreamSource.get(up_cpath).text
    except OSError:
        return None
    for name, body in _iter_upstream_functions(text):
        if name == primary:
            return body
    return None


def _c_jal_count(body, local_macros=()):
    """C-side jal count for `body`: `name(` occurrences minus control keywords AND function-like
    macros. A macro *invocation* emits no jal of its own — OS_USEC_TO_CYCLES expands to arithmetic,
    ERRCK to assignment+branch, va_start to a builtin, MQ_IS_FULL to a field compare. Drops every
    invoked macro via the project-wide `all_func_macros()` table, not just a hardcoded predicate
    list. `local_macros` adds the function-like macros DEFINED IN the upstream .c itself (e.g.
    xprintf.c's `PUT`/`PAD`/`ATOI`, invisible to `all_func_macros()` which scans only headers) —
    PUT/PAD each wrap a `(*pfn)(…)`
    indirect output that compiles to `jalr`, not a named `jal`, so counting them inflated _Printf's
    C side to 14 vs the ROM's 3 jals = a phantom `14vs3` near-verbatim-mirror flag on a clean mirror.
    One level only (a macro whose body wraps exactly one real call would under-count by 1 — rare, and
    the gate reconciles the upstream call list against the asm regardless); the bias is conservative
    because the recurring failure mode is *over*-counting predicate/arithmetic/callback macros."""
    macros = all_func_macros()
    local = set(local_macros)
    return sum(
        1
        for name in C_CALL_RE.findall(body)
        if name not in _C_NONCALL and name not in macros and name not in local
    )


def call_divergence(rom_off, primary, up_cpath, lib=None):
    """Advisory `jal-count-mismatch:<C>vs<asm>` when the upstream call count for `primary` differs
    from the ROM fn's jal count — an upstream-vs-ROM build divergence. None when they agree, the
    body/asm is unreadable, or the count can't be taken. Advisory only; the gate confirms."""
    body = _upstream_body(up_cpath, primary)
    if body is None:
        return None
    n_asm = _asm_jal_count(rom_off, primary)
    if n_asm is None:
        return None
    # Drop the inactive `#if BUILD_VERSION` side FIRST (needs the directives intact): an `#else`
    # branch's call double-counts against the active branch's → a phantom mismatch on a byte-clean
    # mirror (e.g. pfsgetstatus's non-J `__osPfsRequestOneChannel(channel)` inflates 6→7, a false
    # `7vs6`). refs_unplaced strips the same way; without the lib's build_ord this is a no-op.
    body = _strip_inactive_version_branches(body, _build_version_ord(lib))
    # Local function-like macros the .c #defines itself (e.g. xprintf's PUT/PAD/ATOI) are not in
    # all_func_macros() (headers only) → drop them too, else a callback-wrapping macro reads as a call.
    try:
        local_macros = set(
            re.findall(
                r"^\s*#\s*define\s+([A-Za-z_]\w*)\s*\(",
                UpstreamSource.get(up_cpath).text,
                re.M,
            )
        )
    except OSError:
        local_macros = set()
    stripped = _strip_define_lines(_strip_string_literals(_strip_dead_blocks(body)))
    n_c = _c_jal_count(stripped, local_macros)
    if n_c == n_asm:
        return None
    # libnusys per-file version divergence: the cont/RMB family added an `osSetIntMask(OS_IM_NONE)`
    # body wrapper (decl + set + restore) in nusys-2.05, kept through 2.07. When this ROM's fn is the
    # pre-2.05 leaf variant, the upstream's surplus jals are exactly those wrapper calls → the
    # mismatch is a version artifact, not a logic divergence. Annotate so smallest-first is not
    # deterred from a clean near-verbatim drop (S118 nuContRmbModeSet `2vs0`). The asm-side direction
    # only (upstream has MORE: n_c > n_asm); see docs/hazards.md#near-verbatim-mirror-jal-count-mismatch.
    n_wrap = len(re.findall(r"\bosSetIntMask\b", stripped))
    version_artifact = lib == "libnusys" and n_wrap > 0 and (n_c - n_asm) == n_wrap
    return Hazard.jal_count_mismatch(n_c, n_asm, version_artifact=version_artifact)


def _block_reorder_sibling(up_path, hazards, lib):
    """The banked same-family block-reorder mirror (.c basename) whose CheckConnector/RAM-enable swap
    replays on THIS candidate, or None. Fires on a libnusys fn carrying the block-reorder tell — an
    UNexplained jal-mismatch (NOT a `(version-artifact?)` clean-drop) AND NO coddog-mirror (a reorder
    breaks coddog's fingerprint, so "no coddog" IS the tell) — whose upstream-file basename is in a
    BLOCK_REORDER_FAMILIES prefix. Advisory: documents that the verbatim cp needs the SAME swap the
    sibling needed, so the gate applies it up-front rather than rediscovering it via a re-attempt
    (S119 -> S120). Never flags the registered sibling against itself."""
    if lib != "libnusys" or not up_path:
        return None
    jal_unexplained = any(h.is_unexplained_jal() for h in hazards)
    has_coddog = any(h.is_coddog_mirror() for h in hazards)
    if not (jal_unexplained and not has_coddog):
        return None
    base = os.path.basename(up_path)
    stem = os.path.splitext(base)[0]
    for prefix, sibling in BLOCK_REORDER_FAMILIES.items():
        if stem.startswith(prefix) and base != sibling:
            return sibling
    return None


def _is_static_func_proto(line):
    """A `static ...;` line that declares a FUNCTION (prototype: `static T f(...);`), not a
    variable. Such a static function shares the mirror's TU and is NOT a BSS-layout hazard, so it
    must not flag file-static (e.g. sprintf's `static void* proutSprintf(...);`). A variable is kept
    flagged, including a function-*pointer* var (`static T (*fp)(...);`, the `(*`) and an attributed
    array (`static T a[N] __attribute__((aligned(8)));` — its `aligned(8)` parens must NOT read as a
    call declarator, so strip `__attribute__((...))` first)."""
    s = re.sub(
        r"__attribute__\s*\(\(.*?\)\)", "", line
    )  # drop attribute groups (their parens)
    i = s.find("(")
    if i == -1:
        return False  # no declarator parens → a plain variable
    if s[i + 1 : i + 2] == "*":
        return False  # `(*name)(...)` → function-pointer variable
    if "[" in s[:i]:
        return False  # `name[dim] ...(` → array variable (a stray paren past the subscript)
    return True  # `name(...)` → function prototype/definition


def has_file_scope_static(cpath):
    """True if the upstream .c declares a file-scope static *variable* (BSS-layout hazard →
    classical loop). A file-scope static *function* (prototype) is excluded — it is not a data
    hazard. Scan comment-STRIPPED text — FILE_STATIC_RE anchors on `;\\s*$`, so a trailing
    `/* ... */` after the `;` (e.g. `static OSMesgQueue PiMesgQ __attribute__((aligned(8)));
    /* PI message queue */`) would silently defeat the match. Strip the dead `#if BUILD_VERSION` side
    first so an inactive-`#else`-branch file-static (e.g. motor.c's <VERSION_J statics) is not
    phantom-flagged — only the active VERSION_J branch's file-static is real."""
    text = _strip_inactive_version_branches(
        UpstreamSource.get(cpath).text, _build_version_ord("libultra")
    )
    for line in _strip_comments(text).splitlines():
        stripped = line.lstrip()
        if stripped.startswith(("//", "*")):
            continue
        if FILE_STATIC_RE.match(line) and not _is_static_func_proto(line):
            return True
    return False


def defines_data_globals(cpath):
    """File-scope data-global *definitions* with external linkage (not static/extern) in the
    upstream .c. A verbatim mirror would emit these into .data/.bss and collide with the bytes
    splat already extracts + symbolizes at their original vram — the .data analogue of the
    file-static BSS hazard. Returns the defined names; a non-empty result is the `defines-data`
    DoR hazard → route to the classical loop with the data defs dropped (the fn references the
    splat-placed globals via extern). Heuristic (brace-depth + line shape), confirmed at the gate
    like needs-header: `__osThreadTail` et al. in thread.c (osDequeueThread) is a typical case.
    Scan comment-STRIPPED text — a `/* ... ( ... */` banner (e.g. a `Copyright (C) 1997` line)
    contains a `(` that would falsely trip the K&R-param guard below, silently suppressing EVERY
    subsequent depth-0 global. Strip the dead `#if BUILD_VERSION` side first so an
    inactive-`#else`-branch defined global is not phantom-flagged."""
    names = []
    depth = 0
    in_kr_params = False  # between a K&R `name(args)` header and its `{` body
    text = _strip_inactive_version_branches(
        UpstreamSource.get(cpath).text, _build_version_ord("libultra")
    )
    for raw in _strip_comments(text).splitlines():
        line = raw.strip()
        if (
            depth == 0
            and not in_kr_params
            and line
            and not line.startswith(
                ("#", "//", "*", "}", "static", "extern", "typedef")
            )
        ):
            m = DATA_GLOBAL_DEF_RE.match(line)
            if m and "(" not in line.split("=", 1)[0]:
                # Surface the array dimension (`name[DIM]`) so the gate sizes the symbol_addrs
                # entry mechanically — a scalar is 0x4, an array is stride×count (e.g.
                # __osEventStateTab[OS_NUM_EVENTS]). The element/stride + macro resolution
                # still happens at the gate; this just flags array-vs-scalar without opening
                # the source.
                names.append(
                    f"{m.group(1)}[{m.group(2)}]"
                    if m.group(2) is not None
                    else m.group(1)
                )
        # A depth-0 function header (K&R or ANSI) opens a param region where `type name;`
        # lines are parameters, not globals — skip until the body brace. K&R math in libkmc
        # (`_xatan(u,v,atanp)` then `XLONG u,v;`) would otherwise false-flag every param.
        if depth == 0 and "(" in line and not line.endswith(";") and "{" not in line:
            in_kr_params = True
        if "{" in raw:
            in_kr_params = False
        depth += raw.count("{") - raw.count("}")
        if depth < 0:
            depth = 0
    return names


def defines_local_static_data(cpath):
    """Function-local *initialized* statics (brace-depth >= 1) the mirror re-emits into .data — the
    inside-a-function companion to defines_data_globals (which skips `static` and scans only depth 0).
    Returns the defined names; the caller merges them into the `defines-data` hazard so the gate plans
    the .data sibling carve and seed_points re-prices the mirror off the clean-cp floor. Excludes
    static function protos / function-pointer inits (reuses _is_static_func_proto). Typical case:
    gu/position.c's `static float dtor = 3.1415926/180.0;` inside guPositionF needs a 0x10-byte
    .data carve that the file-scope detector — and the coddog re-scan — both miss. Strip the dead
    `#if BUILD_VERSION` side first (symmetric with the file-scope scans)."""
    names = []
    depth = 0
    text = _strip_inactive_version_branches(
        UpstreamSource.get(cpath).text, _build_version_ord("libultra")
    )
    for raw in text.splitlines():
        line = raw.strip()
        if (
            depth >= 1
            and line.startswith("static")
            and "=" in line
            and not _is_static_func_proto(line)
        ):
            m = LOCAL_STATIC_DATA_RE.match(line)
            if m:
                names.append(m.group(1))
        depth += raw.count("{") - raw.count("}")
        if depth < 0:
            depth = 0
    return names


def bare_asserts(cpath):
    """Count `assert(` calls NOT guarded by a dead `#ifdef _DEBUG`/`#if 0` block in the upstream .c.
    This build defines neither NDEBUG nor _DEBUG, and `<assert.h>` expands `assert(EX)` to a live
    `__assert(...)` call, so a verbatim mirror of a file with a BARE (non-_DEBUG-guarded) assert
    compiles in a branch + `jal __assert` the release ROM lacks → SHA-miss + an `#ifdef _DEBUG` wrap
    (docs/hazards.md#assert-strip). Counts post-`_strip_dead_blocks`, so an upstream assert already in
    the in-tree `#ifdef _DEBUG` convention does NOT fire (the token never reaches the macro phase).
    `\\bassert` won't match `__assert` (no word boundary after `_`). Pre-flagging prices the wrap at
    the gate."""
    try:
        text = UpstreamSource.get(cpath).text
    except OSError:
        return 0
    return len(re.findall(r"\bassert\s*\(", _strip_dead_blocks(_strip_comments(text))))


def _c_combined_member_paths(fns, up_path, upstream_index):
    """The distinct C upstreams of a c-combined pack's NON-primary members. The primary-keyed mirror
    battery (append_upstream_hazards) scans only fns[0]'s upstream, so a SECONDARY member file's
    defined global / bare assert is invisible at the gate (e.g. sl.c's `alGlobals` defines-data,
    missed by a primary-only scan when save.c is the pack primary). Returns member cpaths != up_path,
    de-duped + order-preserving; empty for a single-fn or single-file pack (no behavior change off the
    c-combined path)."""
    seen, paths = set(), []
    for fn in fns[1:]:
        _, p = upstream_index.get(fn, (None, None))
        if p and p != up_path and p not in seen:
            seen.add(p)
            paths.append(p)
    return paths


def defines_file_static_const_array(cpath):
    """File-scope `static const <type> <name>[...] = {...};` arrays in the upstream .c. A `const`
    static is file-PRIVATE rodata (the `static` keyword bars cross-file linkage), so when one is
    present the whole code-segment `.rodata` carve is this object's own — it is then safe to widen
    the rodata-literal carve-start to the subseg boundary. The FP-literal scan sees only the scalar
    `ldc1/lwc1 %lo` loads, missing the array base (an `addiu %lo` address-of) + string-literal
    pointers, so the reported carve-start under-states the real extent. A direct `addiu %lo` scan was
    reverted (cross-file FP in the `.data` band); this `static const` source gate keeps the
    rodata-only widening FP-safe. Returns the array names; a non-empty result authorises the
    carve-start widening in `_append_recover_hazards`. Typical case: xldtob.c's
    `static const ldouble pows[]`, whose dlabel + NaN/Inf strings start below the FP scan's min."""
    names = []
    depth = 0
    for raw in UpstreamSource.get(cpath).text.splitlines():
        if depth == 0:
            m = FILE_STATIC_CONST_ARRAY_RE.match(raw.strip())
            if m:
                names.append(m.group(1))
        depth += raw.count("{") - raw.count("}")
        if depth < 0:
            depth = 0
    return names


def defines_file_static_init_array(cpath):
    """File-scope NON-const INITIALIZED `static <type> <name>[…] = <init>;` arrays in the upstream .c
    — the .data analogue of defines_file_static_const_array (the rodata/const case). A verbatim mirror
    re-emits these into its OWN .data, so each needs a `.data` sibling carve at the asm-recovered vram.
    This class (`xprintf` spaces/zeroes, env `eqpower[128]`, _Litob `ldigs`/`udigs`) is one no other
    detector catches: FILE_STATIC_RE excludes `=`-initialized lines (.bss only), defines_data_globals
    skips `static`, defines_local_static_data is depth>=1. `static` bars cross-file linkage → the array
    is file-PRIVATE, so the source-scan sidesteps the own-vs-cross-file-extern blocker (the reason the
    asm `addiu %lo` detector was reverted). The caller fires `data-carve` ONLY on the single-file (non
    c-combined) subset (a c-combined pack's per-member up_path can mis-attribute the carve). Returns
    the array names; the
    vram + exact extent are recovered from the asm at the gate (the .data carve is a mechanical step)."""
    names = []
    depth = 0
    text = _strip_inactive_version_branches(
        UpstreamSource.get(cpath).text, _build_version_ord("libultra")
    )
    for raw in _strip_comments(text).splitlines():
        if depth == 0:
            m = FILE_STATIC_INIT_ARRAY_RE.match(raw.strip())
            if m:
                names.append(m.group(1))
        depth += raw.count("{") - raw.count("}")
        if depth < 0:
            depth = 0
    return names


@functools.cache
def placed_symbols():
    """Set of every symbol name in the two name files (symbol_addrs + ghidra),
    built once per process. A name in this set has a linker address; one absent
    from it does not (a data extern then link-fails). Format: `name = 0x...; // ...`."""
    names = set()
    for nf in NAME_FILES:
        if not os.path.exists(nf):
            continue
        with open(nf, errors="ignore") as f:
            for line in f:
                m = re.match(r"\s*([A-Za-z_]\w+)\s*=", line)
                if m:
                    names.add(m.group(1))
    return names


# An un-named decomp symbol: `func_<vram>` or `func_ovl<n>_<vram>` (the splat scaffold placeholder).
_FUNC_TOKEN_RE = re.compile(r"\bfunc_(?:ovl\d+_)?[0-9A-Fa-f]{6,8}\b")


@functools.cache
def src_func_callers():
    """Map every un-named `func_<vram>` token to the banked C files that reference it by name
    (outside an INCLUDE_ASM stub line). Built once per process by walking `src/`.

    Motivates the `caller-evict` gate flag: when the gate ADDS a curated symbol for an un-named
    `func_<vram>` member (so splat renames it), any already-banked C file that hard-codes the old
    `func_<vram>` name fails to link (undefined reference) until its call site is renamed too. This
    is the stale-label-sync hazard reaching the gate via a symbol ADD rather than `make sync-names`.
    Surfacing the caller here prices the one-line rename fixup at the gate instead of as a build-check
    link error."""
    callers: dict[str, set] = {}
    src_root = os.path.join(ROOT, "src")
    for dirpath, _dirs, files in os.walk(src_root):
        for fname in files:
            if not fname.endswith(".c"):
                continue
            fpath = os.path.join(dirpath, fname)
            try:
                with open(fpath, errors="ignore") as f:
                    lines = f.read().splitlines()
            except OSError:
                continue
            rel = os.path.relpath(fpath, ROOT)
            for ln in lines:
                if "INCLUDE_ASM" in ln:
                    continue  # the func_'s own scaffold stub is regenerated, not an eviction
                for tok in _FUNC_TOKEN_RE.findall(ln):
                    callers.setdefault(tok, set()).add(rel)
    return {k: sorted(v) for k, v in callers.items()}


# An SDK-internal global is `__`-prefixed (e.g. __osRunQueue). Uppercase macros ([A-Z]-led) and
# CamelCase types never match, so this rarely false-flags a non-symbol token.
SDK_GLOBAL_RE = re.compile(r"__[A-Za-z]\w+")

# Compiler predefined macros: `__`-prefixed so SDK_GLOBAL_RE matches them, but they expand to a
# string/int literal at compile time — never a linked global. drvrnew's debug-heap call
# `alHeapDBAlloc((u8*)__FILE__, __LINE__, ...)` surfaced `__FILE__,__LINE__` as a phantom
# refs-unplaced (here under an inactive `#ifdef _DEBUG`, but they'd be false even when active).
# The reverb/env synthesis siblings carry the same macro, so skip these like `__builtin_`.
_C_PREDEF_MACROS = frozenset(
    {
        "__FILE__",
        "__LINE__",
        "__DATE__",
        "__TIME__",
        "__TIMESTAMP__",
        "__BASE_FILE__",
        "__func__",
        "__FUNCTION__",
        "__PRETTY_FUNCTION__",
    }
)

# A data extern declaration in a header: `extern <type...> <name>[opt-array];` with NO '(' before
# the `;` (the lookahead drops function prototypes and function-pointer externs). Captures the
# final identifier — the declared global's name (e.g. `extern volatile u32 nuGfxTaskSpool;`).
EXTERN_DATA_DECL_RE = re.compile(
    r"^\s*extern\s+(?![^;\n]*\()[^;\n]*?\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*;", re.M
)


# A typedef'd type *name* in a header, in the two forms the SDK uses: a struct/union/enum body
# close (`} __OSViContext;` — including array/`__attribute__` tails) and a simple alias
# (`typedef u32 OSId;`). Captures the alias identifier. This lets refs_unplaced drop a `__`-prefixed
# token that is actually a TYPE used in a declaration (`register __OSViContext* vc;`) rather than a
# data extern — without it, SDK_GLOBAL_RE flags the type as a phantom recover-extern (e.g.
# __OSViContext, a 0x30-byte struct in viint.h, would surface in the refs-unplaced list).
TYPEDEF_CLOSE_RE = re.compile(
    r"}\s*([A-Za-z_]\w*)\s*(?:\[[^\]]*\]|__attribute__\s*\([^;]*\))?\s*;", re.M
)
TYPEDEF_SIMPLE_RE = re.compile(r"^\s*typedef\s+[^;{}\n]*?\b([A-Za-z_]\w*)\s*;", re.M)


def declared_type_names(cpath, _depth=0, _seen=None):
    """Type names typedef'd in the headers `cpath` pulls in (recursively, within the project -I
    set), mirroring declared_extern_data's bounded header walk. Used to exclude type tokens from
    refs_unplaced so a `__`-prefixed TYPE (e.g. __OSViContext) is not mistaken for an unplaced data
    extern. A file-scope `} var;` instance is rare in SDK headers and gate-confirmed, so the small
    over-exclusion risk is acceptable."""
    if _seen is None:
        _seen = set()
    names = set()
    try:
        text = UpstreamSource.get(cpath).text
    except OSError:
        return names
    names.update(TYPEDEF_CLOSE_RE.findall(text))
    names.update(TYPEDEF_SIMPLE_RE.findall(text))
    if _depth >= 4:
        return names
    for line in text.splitlines():
        im = INCLUDE_RE.match(line)
        if not im:
            continue
        header = _resolve_include(im.group(1))
        if header and header not in _seen:
            _seen.add(header)
            names |= declared_type_names(header, _depth + 1, _seen)
    return names


def _resolve_include(inc):
    """First INCLUDE_DIRS hit for an `#include` target, or None (an unindexed / system header).
    Falls back to a BASENAME match when the full relative path misses: an upstream `.c` includes a
    vendored header by its SDK prefix (`PRinternal/controller.h`) but the in-tree copy drops the
    prefix (`include/libultra/internal/controller.h`) — the same already-vendored adaptation
    `already_vendored_intree_path` resolves for needs-header. Without it the header is never scanned,
    so declared_type_names / declared_extern_data miss every typedef/extern it declares and
    refs_unplaced phantom-flags a struct TYPE as an unplaced data extern (e.g. a typedef in
    PRinternal/controller.h would mis-flag refs-unplaced on pfsgetstatus.c)."""
    for d in INCLUDE_DIRS:
        path = os.path.join(d, inc)
        if os.path.exists(path):
            return path
    base = os.path.basename(inc)
    if base != inc:  # had a directory prefix that missed → try the vendored basename
        for d in INCLUDE_DIRS:
            path = os.path.join(d, base)
            if os.path.exists(path):
                return path
    return None


def declared_extern_data(cpath, _depth=0, _seen=None):
    """Names declared as *data* externs in the headers `cpath` pulls in (recursively, only within
    the project -I set). Function prototypes are excluded by EXTERN_DATA_DECL_RE. This is what lets
    refs_unplaced catch non-`__`-prefixed library globals (libnusys's nuGfxTaskSpool/nuGfxDisplay,
    libmus's, …) that SDK_GLOBAL_RE alone misses. Bounded depth keeps a header cycle finite."""
    if _seen is None:
        _seen = set()
    names = set()
    try:
        text = UpstreamSource.get(cpath).text
    except OSError:
        return names
    names.update(EXTERN_DATA_DECL_RE.findall(text))
    if _depth >= 4:
        return names
    for line in text.splitlines():
        im = INCLUDE_RE.match(line)
        if not im:
            continue
        header = _resolve_include(im.group(1))
        if header and header not in _seen:
            _seen.add(header)
            names |= declared_extern_data(header, _depth + 1, _seen)
    return names


# A function-like macro definition in a header: `#define NAME(args) replacement`, the replacement
# spanning '\'-continued lines. Captures NAME and the full (possibly multi-line) body. A verbatim
# mirror that *invokes* such a macro inlines its body, so any data extern / callee the macro
# references is pulled into the object though it never appears in the .c source — the
# macro-hidden recover-extern (e.g. EPI_SYNC in piint.h → __osCurrentHandle, invisible to the
# .c-body grep AND the gate build-check alike, surfacing only when the body links).
FUNC_MACRO_DEF_RE = re.compile(
    r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)\(([^)]*)\)[ \t]*((?:.*\\\n)*.*)", re.M
)


@functools.cache
def all_func_macros():
    """{name: replacement-body} for every function-like macro defined anywhere under the project
    -I set, scanned once per process. A project-wide table (not a per-file header walk) so the
    macro-name exclusion in calls_unplaced is COMPLETE — a macro pulled into an expansion through
    a nested invocation (IO_READ → PHYS_TO_K1) is still recognised as a macro even when its own
    defining header sits beyond the invoking file's include depth, so it is never mis-flagged as
    an unplaced call."""
    macros = {}
    for d in INCLUDE_DIRS:
        for root, _dirs, files in os.walk(d):
            for fn in files:
                if not fn.endswith((".h", ".inc")):
                    continue
                try:
                    text = open(os.path.join(root, fn), errors="ignore").read()
                except OSError:
                    continue
                for name, params, body in FUNC_MACRO_DEF_RE.findall(text):
                    plist = [
                        p.strip()
                        for p in params.split(",")
                        if p.strip() not in ("", "...")
                    ]
                    macros.setdefault(name, (plist, body))
    return macros


def macro_hidden_text(cpath):
    """One-level macro expansion of `cpath`: the replacement bodies of the function-like macros the
    .c *invokes*, plus the set of all function-like macro names in scope. Lets refs_unplaced /
    calls_unplaced see a data extern or callee that a verbatim mirror inlines through a macro
    (EPI_SYNC → __osCurrentHandle) — invisible to the body grep and the gate build-check.
    One level only: the directly-invoked macros' bodies; macros THEY invoke are not recursed (the
    gate confirms against the asm). Each macro's own parameter names are stripped from its body so
    a placeholder like va_start's `__AP`/`__LASTARG` is not mistaken for a referenced global.
    Returns (expansion_text, macro_names)."""
    try:
        text = UpstreamSource.get(cpath).text
    except OSError:
        return "", set()
    macros = all_func_macros()
    # Drop dead `#ifdef _DEBUG/NU_DEBUG/AUD_PROFILE` / `#ifndef _FINALROM` / `#if 0` blocks before
    # finding invocations, so a macro invoked ONLY inside a dead block (e.g. a `#ifdef AUD_PROFILE`
    # profile call in the audio band) is not expanded into a phantom refs/calls ref.
    body = _strip_dead_blocks(_strip_comments(text))
    parts = []
    for name, (params, mbody) in macros.items():
        if not re.search(r"\b" + re.escape(name) + r"\s*\(", body):
            continue
        for p in params:
            mbody = re.sub(r"\b" + re.escape(p) + r"\b", " ", mbody)
        parts.append(mbody)
    return "\n".join(parts), set(macros)


def _names_in_function_bodies(text):
    """Identifiers appearing at brace-depth >= 1 (function bodies + any aggregate `= {...}`
    initializer) in the comment/string-stripped text. refs_unplaced uses this to drop a `__`-token
    that THIS .c itself defines as a file-scope data global yet no function body references by name:
    drop-def externs/removes that def plus any holder initializer, so the token needs no
    symbol_addrs recovery. Typical case (timerintr.c): `__osBaseTimer` is named ONLY in the
    depth-0 initializer `OSTimer* __osTimerList = &__osBaseTimer;` — drop-def deletes that line, so
    no extern/placement is owed. Conservative: a token inside a file-scope aggregate `= {...}`
    initializer counts as in-body and stays flagged (a harmless EXTRA recover, never a missed one)."""
    clean = _strip_comments(text)
    body = []
    depth = 0
    for ch in clean:
        if ch == "{":
            depth += 1
        elif ch == "}":
            if depth > 0:
                depth -= 1
        elif depth >= 1:
            body.append(ch)
    return set(re.findall(r"[A-Za-z_]\w*", "".join(body)))


def refs_unplaced(cpath, placed, lib=None):
    """Data externs the upstream .c *references* but that are absent from BOTH name files — the
    dual of `defines-data` (which catches definitions). The motivating case: osYieldThread reads
    `&__osRunQueue`, a global whose address the asm bakes as a raw lui/addiu immediate (no symbol)
    but which a verbatim C mirror turns into an undefined extern → link failure. A non-empty
    result is the `refs-unplaced` DoR hazard → the execution middle must recover each name's vram
    via asm-data-recovery (disassemble the target fn, read the lui/addiu HI/LO16 pair, add the
    extern to symbol_addrs.txt) BEFORE the flip links. Heuristic, gate-confirmed: a `__`-prefixed
    token OR a name the .c's headers declare as a data extern (via declared_extern_data — catches
    non-`__` library globals like libnusys's nuGfxTaskSpool), referenced in
    the file, never called anywhere (so functions — which splat auto-resolves via
    undefined_funcs_auto — are excluded), and absent from the name set. A function *pointer*
    passed by bare name could over-flag; the gate confirms against the upstream."""
    try:
        text = UpstreamSource.get(cpath).text
    except OSError:
        return []
    # Drop the inactive `#if BUILD_VERSION` side so a ref in the dead branch (e.g. `CartRomHandle`
    # in a non-J `#else`) is not phantom-flagged.
    text = _strip_inactive_version_branches(text, _build_version_ord(lib))
    # Also drop dead `#ifdef _DEBUG/NU_DEBUG/AUD_PROFILE` / `#ifndef _FINALROM` / `#if 0` blocks
    # (symmetric with calls_unplaced) so a data extern referenced ONLY in a dead branch (e.g. an
    # AUD_PROFILE-guarded `extern u32 ...[]` decl + its use) is not phantom-flagged.
    text = _strip_dead_blocks(text)
    # Append one-level macro expansion so a global referenced only inside an invoked library macro
    # (EPI_SYNC → __osCurrentHandle) is visible to the same `__`-prefix / declared-extern grep.
    macro_text, _ = macro_hidden_text(cpath)
    scan = text + "\n" + macro_text
    # `__`-prefixed SDK globals (libultra/libkmc) + any data extern the .c's headers *declare*
    # (libnusys/libmus non-`__` globals SDK_GLOBAL_RE misses, e.g. nuGfxTaskSpool).
    tokens = (
        set(SDK_GLOBAL_RE.findall(scan))
        | declared_extern_data(cpath)
        # Known libultra boot-region globals (non-`__`-prefixed, asm-baked as D_800003xx) the
        # `__`/declared-extern grep would otherwise miss — see BOOT_GLOBALS.
        | {g for g in BOOT_GLOBALS if re.search(r"\b" + re.escape(g) + r"\b", scan)}
    )
    # Type names the headers typedef (e.g. __OSViContext) are referenced in declarations, not as
    # data — drop them so a `__`-prefixed TYPE is not flagged as a phantom recover-extern.
    types = declared_type_names(cpath)
    unplaced = []
    for tok in tokens:
        if tok in placed or tok.startswith("__builtin_") or tok in _C_PREDEF_MACROS:
            continue  # placed, GCC intrinsic, or compiler predefined macro — never a linked global
        if tok in types:  # a typedef'd type, not a data extern
            continue
        # Only flag names the .c (or its invoked macros) actually references.
        if not re.search(r"\b" + re.escape(tok) + r"\b", scan):
            continue
        # Called anywhere (`tok(` allowing whitespace) → a function, not a data extern → skip.
        if re.search(re.escape(tok) + r"\s*\(", scan):
            continue
        unplaced.append(tok)
    # Drop a token THIS .c defines as a file-scope data global but no function body references by
    # name — it lives only in another global's depth-0 initializer (`__osTimerList =
    # &__osBaseTimer`), which drop-def removes, so it is not an unplaced extern owed a recovery.
    defined_here = {n.split("[", 1)[0] for n in defines_data_globals(cpath)}
    if defined_here:
        body_names = _names_in_function_bodies(text)
        unplaced = [
            t for t in unplaced if not (t in defined_here and t not in body_names)
        ]
    return sorted(unplaced)


def _fn_ptr_param_names(text):
    """Names declared as function-pointer PARAMETERS in any function header — `T name(args)` (the
    implicit-fn-ptr param form, e.g. xprintf's `void* pfn(void*,const char*,size_t)`) or
    `T (*name)(args)`. These read as a call to C_CALL_RE but a verbatim mirror invokes them via
    `jalr` through the pointer, not a `jal` to a named symbol, so they must not flag calls-unplaced
    (e.g. _Printf's `pfn` would be a phantom `calls-unplaced:pfn`). Scans each def header's matched
    param list only, so an in-body real call of the same spelling elsewhere is unaffected."""
    names = set()
    for m in UPSTREAM_DEF_RE.finditer(text):
        depth, p = 0, m.end() - 1  # at the header '('
        while p < len(text):
            if text[p] == "(":
                depth += 1
            elif text[p] == ")":
                depth -= 1
                if depth == 0:
                    break
            p += 1
        params = text[m.end() : p]
        names.update(
            re.findall(r"\(\s*\*\s*([A-Za-z_]\w+)\s*\)\s*\(", params)
        )  # T (*name)(...)
        names.update(re.findall(r"\b([A-Za-z_]\w+)\s*\(", params))  # T name(...)
    return names


def calls_unplaced(cpath, primary, placed, lib=None):
    """Functions the upstream .c *calls* by name that are absent from BOTH name files — the
    function dual of refs_unplaced. A verbatim mirror link-FAILS on these once the C body calls
    them by name: splat's undefined_funcs_auto only resolves a callee whose vram carries a name,
    so a callee labelled `func_<addr>` in the asm (unnamed in both files) has no `<name>` symbol
    for the C reference to bind. The gate build-check can't catch this — the INCLUDE_ASM scaffold
    resolves the jal directly, so the symbol is only needed once the C body calls by name (e.g.
    osPiGetCmdQueue, missed by refs_unplaced precisely because it *excludes* anything called).
    A non-empty result is the `calls-unplaced` DoR hazard → the gate recovers each
    callee's vram from its asm jal target and adds `<name> = 0x<addr>; // type:func` to
    symbol_addrs.txt (add-only) BEFORE the flip. Heuristic, gate-confirmed: a `name(` call token
    in the dead-block-stripped body (so a dead `#ifdef _DEBUG` call like __osError does not
    over-flag), not a control keyword, not defined in the same file (those resolve locally), and
    absent from placed. A function-like macro could over-flag; the gate reconciles against the
    asm jals (the same disassemble pass that confirms the recover-extern vram)."""
    try:
        text = UpstreamSource.get(cpath).text
    except OSError:
        return []
    defined = {name for name, _ in _iter_upstream_functions(text)}
    fn_ptr_params = _fn_ptr_param_names(
        text
    )  # a fn-ptr param (`pfn`) is a jalr, not a jal
    # Append one-level macro expansion so a callee invoked only inside a library macro is seen;
    # exclude the macro names themselves (a macro body that invokes UPDATE_REG/WAIT_ON_IOBUSY etc.
    # must not flag those as unplaced functions — they inline, they don't link).
    macro_text, macro_names = macro_hidden_text(cpath)
    # Drop the inactive `#if BUILD_VERSION` side first (e.g. `osPiRawReadIo` called only in a non-J
    # `#else` branch) so its calls are not phantom-flagged.
    text = _strip_inactive_version_branches(text, _build_version_ord(lib))
    # Same-file function-like macros (`#define READFORMAT(ptr) ((__OSContRamReadFormat*)(ptr))` in
    # motor.c) expand to casts/exprs, not jals — collect their names so a `READFORMAT(ptr)` use is
    # not flagged as an unplaced function. macro_hidden_text only captures HEADER macros, so the .c's
    # own function-like #defines need this separate scan.
    local_macro_names = set(
        re.findall(r"^\s*#\s*define\s+([A-Za-z_]\w*)\s*\(", text, re.M)
    )
    text = _strip_define_lines(
        text
    )  # an in-body `#define NAME (expr)` is a macro def, not a call
    body = _strip_string_literals(
        _strip_dead_blocks(_strip_comments(text + "\n" + macro_text))
    )
    called = {
        n
        for n in C_CALL_RE.findall(body)
        if n not in _C_NONCALL
        and n not in macro_names
        and n not in local_macro_names
        and n not in fn_ptr_params
        and not n.startswith("__builtin_")
    }
    return sorted(n for n in called if n not in placed and n not in defined)


def band_mirror_dir(lib, up_path):
    """Project mirror directory for an upstream file: src/lib<...>/<upstream-rel-dir>."""
    tree = dict(UPSTREAM_TREES).get(lib, LIBULTRA)
    rel = os.path.relpath(up_path, tree)
    return os.path.join(ROOT, "src", lib, os.path.dirname(rel))


def band_is_warm(mirror_dir):
    """True if the candidate's mirror dir already holds a banked (0-stub) sibling .c — its
    companion headers + callee symbols are already in-tree, so the flip needs no enablers."""
    if not os.path.isdir(mirror_dir):
        return False
    for cpath in glob.glob(os.path.join(mirror_dir, "*.c")):
        try:
            with open(cpath, errors="ignore") as f:
                if "INCLUDE_ASM" not in f.read():
                    return True
        except OSError:
            continue
    return False


def missing_includes(cpath, mirror_dir=None, lib=None):
    """Upstream includes that don't resolve under the candidate's effective -I set (-nostdinc, so
    every header must come from a -I dir). A non-empty result is the `needs-header` DoR hazard:
    the verbatim mirror won't compile until each is satisfied — copy the companion header
    (execution-middle, e.g. assert.h) or add a -I path (deferred enabler). Conservative: scans all
    `#include` lines regardless of `#ifdef` state, so a dead `#ifdef _DEBUG` include may over-flag
    — the gate confirms.

    The effective -I set is the base INCLUDE_DIRS plus LIB_EXTRA_INCLUDE_DIRS[lib] — the profile
    CFLAGS the source actually compiles under. A libultra mirror resolves <libaudio.h> and the rest
    of the PR/ band through `-I include/libultra/PR` (LIBULTRA_CFLAGS); passing `lib='libultra'` is
    what keeps that band from false-flagging needs-header → blk. `lib=None` uses the base set only.

    A quote-include (`#include "x.h"`) resolves source-relative to the dir the mirror compiles
    in, so a band-local companion already shipped at <mirror_dir>/x.h (e.g. a gu/guint.h copied
    alongside the first gu/ sibling) is NOT missing even though it is absent from the -I set — the
    band-open fast path. `mirror_dir` is the in-tree dir the source will land in (band_mirror_dir);
    None disables the source-relative check."""
    search_dirs = INCLUDE_DIRS + LIB_EXTRA_INCLUDE_DIRS.get(lib or "", [])
    missing = []
    for line in UpstreamSource.get(cpath).text.splitlines():
        m = INCLUDE_RE.match(line)
        if not m:
            continue
        inc = m.group(1)
        if any(os.path.exists(os.path.join(d, inc)) for d in search_dirs):
            continue
        # A quote-include resolves source-relative once the band-local companion is in-tree.
        # normpath collapses a `..` segment (a relative `../gu/guint.h` from a sibling-dir
        # upstream source resolves to the already-vendored gu/ copy, not a phantom needs-header).
        is_quote = '"' in m.group(0)
        if (
            is_quote
            and mirror_dir
            and os.path.exists(os.path.normpath(os.path.join(mirror_dir, inc)))
        ):
            continue
        if inc not in missing:
            missing.append(inc)
    return missing


@functools.cache
def _project_inc_index():
    """Set of every header under the project `include/` tree (both its root-relative path and
    its basename), built once per process — used to tell an unreachable-but-present header
    (unindexed `-I`, defer) from a genuinely-absent one (copyable from upstream, or a system
    header)."""
    idx = set()
    for f in glob.glob(os.path.join(PROJECT_INC, "**", "*"), recursive=True):
        if os.path.isfile(f):
            idx.add(os.path.relpath(f, PROJECT_INC))
            idx.add(os.path.basename(f))
    return idx


@functools.cache
def _upstream_src_headers():
    """Basenames of every `.h` under the upstream library SOURCE trees (UPSTREAM_SRC_ROOTS) — the
    source-private companion headers (xstdio.h, guint.h) a mirror copies source-relative next to
    its .c. Built once per process. Distinct from the public -I headers (UPSTREAM_INC_ROOTS)."""
    names = set()
    for root in UPSTREAM_SRC_ROOTS:
        for f in glob.glob(os.path.join(root, "**", "*.h"), recursive=True):
            names.add(os.path.basename(f))
    return names


def include_is_vendorable(inc):
    """True if a *missing* include is copyable from upstream (a cheap header-vendor enabler), not a
    hard block: present at `<upstream-include-root>/<inc>` (companion-copy into an -I dir, e.g.
    assert.h) OR a source-private header found under an upstream source tree (source-relative copy
    next to the mirror, e.g. guint.h/xstdio.h). NOT vendorable when it's in the project tree but
    unreachable (deferred -I) or absent everywhere (system header). Heuristic, gate-confirmed.

    The project-tree check is by the EXACT include path, not basename: a header shipped in-tree
    under a *different* prefix (e.g. `internal/piint.h`) does NOT make `PRinternal/piint.h`
    reachable — that's a cheap companion-copy, not a deferred -I. (_project_inc_index also stores
    basenames, so a bare-name include like `libaudio.h` still matches the basename entry and stays
    correctly NOT-vendorable.) Without the exact-path check, the basename collision would mask the
    whole PRinternal/ PI band as false-`blk`."""
    if inc in _project_inc_index():
        return False  # present in-tree but unreachable → unindexed -I, a deferred enabler not a cp
    for root in UPSTREAM_INC_ROOTS:
        if os.path.exists(os.path.join(root, inc)):
            return True  # public companion header → copy into an -I dir
    return (
        os.path.basename(inc) in _upstream_src_headers()
    )  # source-private → source-relative cp


def include_is_blocked(inc):
    """Classify a *missing* include (one `missing_includes` already found unreachable). Blocked =
    can't be fixed by a header copy = NOT vendorable: either present in the project `include/` tree
    but unreachable under the current `-I` set (unindexed `-I` — a deferred Makefile enabler), or
    absent everywhere (a system header we don't ship). The complement of include_is_vendorable."""
    return not include_is_vendorable(inc)


def already_vendored_intree_path(inc, lib):
    """For a *missing* include (full path unreachable, so `missing_includes` flagged it) whose
    BASENAME resolves under the current `-I` set: the in-tree path the verbatim include must be
    ADAPTED to (the established drop-the-prefix fix-up — `PRinternal/controller.h` → `"controller.h"`,
    resolved from `include/libultra/internal/`). None when the basename does not resolve.

    Returned relative to the project `include/` root for a compact gate hint. Every
    `(already-vendored)` header IS a needs-include-adapt case by construction: it reached the tagger
    only because its full path already failed `missing_includes`, so the upstream prefix differs from
    the in-tree layout and the include line must change (SHA-neutral — declarations only). Surfacing
    the adapt target lets the gate price the include edit (`PRinternal/controller.h` →
    `internal/controller.h`) instead of hitting `No such file` at first build."""
    search_dirs = INCLUDE_DIRS + LIB_EXTRA_INCLUDE_DIRS.get(lib or "", [])
    base = os.path.basename(inc)
    for d in search_dirs:
        cand = os.path.join(d, base)
        if os.path.exists(cand):
            try:
                return os.path.relpath(cand, PROJECT_INC)
            except ValueError:
                return cand
    return None


def _tagged_missing_includes(cpath, lib):
    """(tagged_headers, blocked) for cpath's unreachable includes, tagged by enabler load: bare =
    a real block (deferred -I / system header → pts 'blk'), `(vendorable)` = a one-time source cp,
    `(already-vendored)` = free (the basename already resolves under the -I set). `blocked` counts
    only the bare ones. Shared by the named-upstream battery (append_upstream_hazards) and the
    coddog trap re-scan (build_rows), so both price headers identically."""
    missing = missing_includes(cpath, band_mirror_dir(lib, cpath), lib)
    if not missing:
        return [], False

    def _tag(inc):
        if include_is_blocked(inc):
            return inc
        adapt = already_vendored_intree_path(inc, lib)
        if adapt is not None:
            # Non-blocking, no cp — but the verbatim include line must be adapted to the in-tree
            # location (`PRinternal/controller.h` → `internal/controller.h`). Surface the
            # target so the gate prices the edit instead of hitting it at first build.
            return f"{inc}(already-vendored,adapt->{adapt})"
        return f"{inc}(vendorable)"

    return [_tag(inc) for inc in missing], any(
        include_is_blocked(inc) for inc in missing
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


def _straddling_unattrib(fns, fn_stems, unattrib, name_addr):
    """Vrams of `?` (unattributed) pack members that STRADDLE a c-combined file boundary — the
    nearest named-C member BEFORE and AFTER resolve to DIFFERENT upstream stems. `fn_stems[i]` is
    fn[i]'s C-upstream stem or None; `unattrib` is the set of fns rendered `=?` (no C, no asm TU);
    `name_addr` maps a fn name -> its vram. A front/trailing `?` (no named member on one side) or a
    `?` flanked by the SAME stem (a within-file unnamed fn) does NOT straddle. Pure (unit-tested
    without a ROM)."""
    out = []
    for i, fn in enumerate(fns):
        if fn not in unattrib or fn not in name_addr:
            continue
        prev_stem = next(
            (fn_stems[j] for j in range(i - 1, -1, -1) if fn_stems[j]), None
        )
        next_stem = next(
            (fn_stems[j] for j in range(i + 1, len(fns)) if fn_stems[j]), None
        )
        if prev_stem and next_stem and prev_stem != next_stem:
            out.append(name_addr[fn])
    return out


def _classify_pack_hazards(off, fns, upstream_index):
    """Hazards for a multi-fn asm subseg (a pack): the member breakdown plus the
    pack-shape flags (single-file vs multi-file pack, foreign-TU fn-count mismatch,
    one-.o, c-combined, combined asm-TU). Assumes len(fns) > 1; returns them in order."""
    hz = []
    # List each fn + its upstream basename so the gate can tell a multi-file pack
    # (different basenames → split at the upstream-file boundary) from a single-file
    # pack, without hand-disassembling asm/<rom>.s.
    members = []
    asm_index = build_asm_tu_index()
    asm_tus = []  # distinct vendorable .s TUs among asm-ONLY members (no C mirror)
    c_stems = []  # distinct C-upstream stems among members (multi-file C-mirror pack)
    c_files = []  # parallel to c_stems: the (lib, cpath) of each distinct C stem
    c_members = (
        0  # members that resolved to a C upstream (for the single-file-pack test)
    )
    fn_stems = []  # parallel to fns: each member's C-upstream stem, or None (asm-TU or `?`)
    unattrib = (
        set()
    )  # fns rendered `=?` (no C upstream, no asm TU) — the unattributed-leaf candidates
    for fn in fns:
        fn_lib, fn_up = upstream_index.get(fn, (None, None))
        if fn_up:
            stem = os.path.splitext(os.path.basename(fn_up))[0]
            members.append(f"{fn}={stem}")
            fn_stems.append(stem)
            c_members += 1
            if stem not in c_stems:
                c_stems.append(stem)
                c_files.append((fn_lib, fn_up))
            continue
        fn_stems.append(None)
        # No C upstream. Count a member's asm TU only here: a member with a C mirror
        # is a C-mirror pack-split target, not an asm-vendor TU (ultralib may ship a .s
        # variant the ROM doesn't use). Name the .s in the member label so a MIXED asm+C
        # pack reads as `bzero=bzero.s`, not an opaque `bzero=?`.
        t = asm_index.get(fn)
        if t:
            members.append(f"{fn}={os.path.basename(t)}")
            if t not in asm_tus:
                asm_tus.append(t)
        else:
            members.append(f"{fn}=?")
            unattrib.add(fn)
    # A pack whose every member resolves to ONE upstream C file is NOT a split-required
    # blocker — it is an atomic verbatim mirror (the gu F-variant + wrapper idiom), banked
    # or spiked in one shot. Tag it `single-file-pack` so the gate stops reading it as the
    # multi-file `pack` that needs an upstream-file split. Display-only (the pts seed keys the
    # pack penalty on nfns>1, not the kind). A MIXED asm+C or multi-stem pack keeps `pack`.
    single_file = c_members == len(fns) and len(c_stems) == 1
    # A pack with unnamed (`func_<addr>`) member(s) still atomic-mirrors when its one named C
    # stem's upstream file defines exactly len(fns) functions — the unnamed members are that
    # file's other functions (e.g. guAlignF named + an unnamed guAlign, both in align.c).
    # Conservative: one named stem + an exact function-count match; else falls back to pack.
    if not single_file and len(c_stems) == 1 and 1 <= c_members < len(fns):
        if _upstream_file_func_count(*c_files[0]) == len(fns):
            single_file = True
    pack_kind = HAZARD_SINGLE_FILE_PACK if single_file else HAZARD_PACK
    hz.append(Hazard.pack(pack_kind, len(fns), members))
    # One named C stem but the pack has MORE fns than that .c defines → the surplus members
    # are a FOREIGN TU bundled in the subseg (split it off, mirror only the upstream's fns).
    # The named-symbol analog of coddog-fncount-mismatch. Guard `c_members <= fc` (the named
    # members must fit the file's defs, else the attribution is suspect) and `not single_file`.
    # Advisory (display-only).
    if not single_file and len(c_stems) == 1 and c_members:
        fc = _upstream_file_func_count(*c_files[0])
        if fc and len(fns) > fc and c_members <= fc:
            hz.append(Hazard.upstream_fncount_mismatch(len(fns), fc))
    # one-tu: if EVERY inner fn boundary is non-16-aligned, the pack is ONE .o (the linker
    # 16-aligns each .o's .text start, so a 2nd .o would begin on a 16 boundary). Confirms a
    # single-file-pack structurally even when members are un-named (the coddog-mirror case),
    # and marks a per-fn decompose split as mechanically blocked. Sufficient, not necessary:
    # a one-.o pack with a 16-multiple-sized member won't fire (conservative).
    addrs = [a for _, a in asm_function_addrs(off)]
    if len(addrs) == len(fns) and all(a % 16 != 0 for a in addrs[1:]):
        hz.append(Hazard.one_tu())
    # C analog of combined-subseg: ≥2 *distinct* C upstream files share one asm subseg → a
    # multi-file C-mirror pack the gate splits at the upstream-file boundary, then mirrors each
    # verbatim. A big combined subseg ranks by its WHOLE size and buries a cheap clean leaf past
    # smallest-first; surfacing `c-combined:Nfile[…]` prices the split + names the leaves. A
    # single-file pack has one stem → does NOT fire.
    if len(c_stems) > 1:
        hz.append(Hazard.c_combined(c_stems))
        # A SINGLE `?` leaf STRADDLING the file boundary (different named-C stems before and after it)
        # must be assigned to one singleton by the split — flag its vram so the gate accounts for it
        # rather than letting a silent `?` ride into the wrong side (S120 func_800A2780). Gated to a
        # LONE straddler: a clean 2-file split with one stray leaf, NOT a whole foreign TU interleaved
        # in (the __assert/nuboot game-boot region has 11 interspersed game `?`s — that messy bundle
        # is the pack/upstream-fncount-mismatch case, not a stray boundary leaf). Front/trailing `?`
        # (within one file) never straddle. asm_function_addrs gives ROM offsets → +subseg vram delta.
        base_v = subseg_vram(off)
        name_vram = {nm: base_v + (a - off) for nm, a in asm_function_addrs(off)}
        straddlers = _straddling_unattrib(fns, fn_stems, unattrib, name_vram)
        if len(straddlers) == 1:
            hz.append(Hazard.unattrib_leaf(straddlers))
    # Combined-subseg sub-pattern: ≥2 asm-ONLY members from *distinct* vendorable ultralib .s
    # files (the asm analog of a multi-file C pack). The gate splits the subseg at the TU
    # boundary, then vendors each .s verbatim. A pack whose asm members share one .s (a
    # partial-TU split) has a single distinct TU → does NOT fire (different, harder hazard).
    if len(asm_tus) > 1:
        bn = "|".join(sorted(os.path.basename(t) for t in asm_tus))
        detail = f"{len(asm_tus)}tu[{bn}]"
        # Price the needs-define enabler for the split the same way the intrinsic-likely path
        # does: a vendorable .s may reference an UPPER_CASE asm macro the in-tree `-I` headers
        # lack (e.g. setfpccsr.s → CFC1/CTC1), and the gate must vendor it before the split.
        # Union the misses across the pack's TUs so one (needs-define:…) prices the whole split.
        miss = sorted({m for t in asm_tus for m in vendorable_tu_missing_defines(t)})
        if miss:
            detail = f"{detail}(needs-define:{','.join(miss)})"
        hz.append(Hazard.combined_subseg(detail))
    return hz


def _classify_asm_mirror_hazards(off, fns, upstream_index):
    """The hand-asm / asm-mirror hazard for an asm subseg: a pure register/FPU shim or a
    privileged CP0/TLB TU (both vendor verbatim from the ultralib .s), or a libkmc
    soft-float TU (the kmc-as path). Returns 0 or 1 intrinsic-likely hazard."""
    hz = []
    # Hand-asm detection → asm-mirror candidate, not a classical target. Two cases collapse
    # here: a *pure* register/FPU shim (intrinsic_likely) and a privileged TU that does real
    # work around an op C can't emit (privileged_asm: a tlbwi/mtc0/eret with branches+loads or
    # calls — osMapTLB/osUnmapTLB, exception dispatch). Both vendor verbatim from the
    # ultralib .s (docs/hazards.md#asm-mirror-vendoring).
    prim_intrinsic = intrinsic_likely(off, fns[0])
    prim_privileged = privileged_asm(off, fns[0])
    if prim_intrinsic or prim_privileged:
        # Carry the vendorable ultralib TU path when the primary's name matches a LEAF
        # (intrinsic-likely:os/getcount.s, …:os/setintmask.s) so the gate asm-mirrors it
        # directly. When the TU is vendorable, pre-flag any macro the in-tree asm `-I` headers
        # don't define (…(needs-define:RDB_BASE_VIRTUAL_ADDR,…)) so the gate pays the enabler
        # before the verbatim copy, not at a failing vendor-compile.
        rel = build_asm_tu_index().get(fns[0])
        if rel:
            detail = rel
            miss = vendorable_tu_missing_defines(rel)
            if miss:
                detail = f"{detail}(needs-define:{','.join(miss)})"
            # A vendorable .s with a non-.text section can't be a clean .text-only cp —
            # pre-flag the strip+rename enabler (docs/hazards.md#asm-mirror-vendoring).
            data_syms = vendorable_tu_data_symbols(rel)
            if data_syms:
                detail = f"{detail}(has-rodata:{','.join(data_syms)})"
            # A SYMBOLIC-pointer table in the active data section (a switch jtbl / fn-ptr table)
            # needs the LABEL-EXPORT procedure on top of the strip-and-rename. The table extracts
            # to a separate, already-address-placed blob that keeps symbolic .word .L<addr> refs
            # after the flip; vendor .text-only and RE-EXPORT those .L<addr> labels in the vendored
            # .text so the blob resolves. Flag it so the gate runs the label-export procedure, not
            # a bare has-rodata replay (docs/hazards.md#asm-mirror-vendoring, the asm-mirror-jtbl case).
            jtbls = vendorable_tu_jtbl(rel)
            if jtbls:
                detail = f"{detail}(asm-mirror-jtbl:{','.join(jtbls)})"
        elif prim_privileged:
            # An un-named func_<addr> with a privileged op: the name can't resolve the TU, but
            # the privileged op proves a vendorable ultralib source exists. Tag it cp0-asm so the
            # gate identifies + vendors it (a single MCP disasm names it by its CP0/TLB signature),
            # instead of reading the BARE intrinsic-likely as a no-source shim and parking it.
            detail = "cp0-asm(identify-TU)"
        else:
            detail = None  # genuine no-source shim (handwritten leaf, no privileged op, no TU)
        hz.append(Hazard.intrinsic_likely(detail))
    else:
        # A libkmc soft-float / 64-bit math TU the pure-shim + privileged tests both miss (a
        # branchy cvt routine like mcvtld.s, no CP0/FPU-ctrl op, no `handwritten` tag) is STILL a
        # verbatim KMC-as asm-mirror, not a classical decomp target. If the primary matches a
        # libkmc .s `.globl` AND the subseg has no C upstream, name the .s + the kmc-as mechanism
        # so the gate vendors it via the KMC `as` path (`.include "mips_as.h"` via -I src/libkmc,
        # the `li 0xffffffff`->`addiu` edit), not LIBULTRA_ASFLAGS. The `not in upstream_index`
        # guard excludes the C-mirrorable libkmc files, leaving only the asm-ONLY TUs.
        # See docs/hazards.md#asm-mirror-vendoring (kmc-as sub-lane).
        kmc_tu = build_kmc_asm_tu_index().get(fns[0])
        if kmc_tu and fns[0] not in upstream_index:
            hz.append(Hazard.intrinsic_likely(f"{kmc_tu}(kmc-as)"))
    return hz


def classify_subseg(off, typ, path, size, upstream_index):
    """Classify one subseg into (kind, fns, hazards), or None to skip it.

    Covers the two pickable subseg shapes: a flippable `asm` block (with its
    align/pack/intrinsic hazards) and a partially-matched `c` file (remaining
    INCLUDE_ASM stub count). bss + non-pickable rows return None.
    """
    if typ == "asm":
        fns = asm_functions(off)
        if not fns:
            return None
        # Skip a pure-nop pad subseg: a trailing-alignment pad split off as its own `[..,asm]`
        # subseg (the trailing-pad remedy below) carries a splat glabel but is 0x00000000 nops only —
        # never a decomp target. `subseg_vram` non-None (listing present) + `code_end_rom` None (no
        # non-nop instruction) is the all-nop signature; a real fn always has a non-nop body.
        if subseg_vram(off) is not None and code_end_rom(off) is None:
            return None
        hazards = []
        if size and size % 16 != 0:
            hazards.append(Hazard.non16align())
        # Trailing-alignment pad: splat extracts the whole subseg slot (function + the nop
        # padding up to the next, higher-aligned subseg). A flipped C mirror's compiler only
        # 16-aligns its `.text`, so a pad beyond that 16B fill is dropped → ROM short → SHA-miss
        # in the execution middle, invisible to the gate (the INCLUDE_ASM stub carries the pad).
        # Pre-flag the residual pad-subseg byte count + the next boundary's alignment so the gate
        # prices the `[0x<gcc_o_end>, asm]` split up front. FP-guarded: residual is `size` minus
        # the function's own 16-aligned size, so a fn that already fills its slot (e.g. the
        # contramread sibling) yields 0 and is not flagged. See
        # docs/hazards.md#trailing-alignment-pad-after-a-c-mirror.
        code_end = code_end_rom(off)
        if size and code_end is not None:
            gcc_aligned = (code_end - off + 15) & ~15
            residual = size - gcc_aligned
            next_vram = subseg_vram(off)
            align = (next_vram + size) & -(next_vram + size) if next_vram else 0
            # Require the next boundary to be aligned ABOVE 16: that is the exact condition GCC's
            # own 16-align can't fill. A residual at a merely-16-aligned boundary is the
            # delay-slot-nop measurement artifact (code_end stops at the last non-nop, undercounting
            # a real `jr ra; nop` tail by one 16-step) — GCC 16-aligns past it anyway, so excluding
            # align<=16 removes that false fire.
            if residual >= 16 and align > 16:
                hazards.append(Hazard.trailing_pad(residual, align))
        if len(fns) > 1:
            hazards.extend(_classify_pack_hazards(off, fns, upstream_index))
        hazards.extend(_classify_asm_mirror_hazards(off, fns, upstream_index))
        return "asm-flip", fns, hazards
    if typ == "c" and path:
        cpath = os.path.join(ROOT, "src", path + ".c")
        if not os.path.exists(cpath):
            return None
        text = open(cpath).read()
        fns = re.findall(r"INCLUDE_ASM\([^,]+,\s*([A-Za-z_]\w+)", text)
        if not fns:  # 0 stubs => already an md5-candidate
            return None
        return "c-stub", fns, [Hazard.remaining(len(fns))]
    return None


def _append_recover_hazards(off, primary, up_path, up_lib, hazards):
    """Append refs-unplaced + calls-unplaced (the symbol-recovery battery, with inline-vram
    annotation when the binding is unambiguous) for an upstream .c, in-place. Shared by the
    named-upstream battery (append_upstream_hazards) and the coddog trap re-scan, so a
    coddog-resolved mirror prices its recover-extern load identically to a named one — mirroring how
    `_tagged_missing_includes` is shared so both price headers identically. An un-named (`func_`)
    subseg blocks the named-keyed scan, so without this the recover-extern load is invisible at the
    gate and only surfaces at first build."""
    placed = placed_symbols()
    unplaced = refs_unplaced(up_path, placed, up_lib)
    if unplaced:
        # Annotate the recovered vram inline when the binding is unambiguous (one unplaced
        # name ∩ one asm candidate) so the gate copy-pastes the symbol_addrs entry instead
        # of re-running MCP disassemble_function. Ambiguous → bare names.
        cands = recover_unplaced_vram(off)
        if len(unplaced) == 1 and len(cands) == 1:
            refs = [f"{unplaced[0]}@0x{cands[0]:08X}"]
        else:
            refs = unplaced
        # A boot-region global carries its vram in BOOT_GLOBALS, so annotate it inline regardless
        # of the single-candidate disambiguation above (the gate copy-pastes the recover-extern).
        refs = [
            f"{r}@0x{BOOT_GLOBALS[r]:08X}" if r in BOOT_GLOBALS else r for r in refs
        ]
        hazards.append(Hazard.refs_unplaced(refs))  # asm-data-recovery before flip
    unplaced_calls = calls_unplaced(up_path, primary, placed, up_lib)
    if unplaced_calls:
        # Annotate the recovered vram inline when unambiguous (one unplaced call ∩ one
        # unnamed jal), mirroring refs-unplaced, so the gate copy-pastes the func entry.
        ccands = recover_unplaced_call_vram(off, primary)
        if len(unplaced_calls) == 1 and len(ccands) == 1:
            crefs = [f"{unplaced_calls[0]}@0x{ccands[0]:08X}"]
        else:
            crefs = unplaced_calls
        hazards.append(Hazard.calls_unplaced(crefs))  # recover func symbol before flip
    # Switch jump table: a `switch` compiles to a `jtbl_<addr>` in the code-segment `.rodata` whose
    # `.word` entries are the fn's own internal `.L<addr>` labels. Flipping the subseg text->C
    # deletes those labels, so the still-asm rodata jtbl link-breaks (undefined-`.L<addr>` ref) unless
    # a `.rodata` sibling carve places the C-re-emitted table. The gate's text-only green-ROM check
    # cannot catch this by construction (the jtbl stays valid asm until the C body lands), so it must
    # be priced at the gate — the jump-table analog of rodata-literal (a verbatim mirror re-emits a
    # byte-identical table: same case-body absolute addresses; #rodata-sibling-yaml-pattern). Scanned
    # here in the SHARED battery so it prices both the named-upstream and coddog paths (a NAMED
    # jtbl-owner is never reached by the coddog re-scan otherwise). Display-only like rodata-literal:
    # the carve is a mechanical near-free enabler, so no seed_points bump.
    jtbls = [a for a in rodata_jtbls(off) if _literal_in_rodata(a, off)]
    if jtbls:
        hazards.append(Hazard.rodata_jtbl(jtbls))


def _upstream_defines_function(cpath, name):
    """True if the version-stripped upstream .c defines a function named `name` at brace-depth 0.
    Gates header_renames_symbol against a macro-ALIAS false fire. os_motor.h's
    `#define osMotorStop(x) __osMotorAccess((x), MOTOR_STOP)` macro-renames a symbol, but a `#undef`
    is only needed when the BODY actually defines a function named `primary` (the source-compat case,
    where the upstream defines the curated name and the macro rewrites it). Under VERSION_J motor.c
    defines `__osMotorAccess` (the macro's RHS), NOT
    `osMotorStop`, so the curated name is the RHS — the `osMotorStop` token never appears in the body
    and no #undef is needed. Returns False ⇒ suppress the hazard."""
    try:
        text = UpstreamSource.get(cpath).text
    except OSError:
        return False
    text = _strip_inactive_version_branches(text, _build_version_ord("libultra"))
    return any(n == name for n, _ in _iter_upstream_functions(text))


def _macro_alias_target(cpath, primary, _depth=0, _seen=None):
    """The first identifier in the body of a `#define <primary>(...) <body>` macro found in the
    include tree, or None. Paired with header_renames_symbol to name the wrong-ghidra-name
    correction — os_motor.h's `#define osMotorStop(x) __osMotorAccess((x), MOTOR_STOP)` → the symbol
    `__osMotorAccess` that the vram is really named (vs the ghidra mislabel `osMotorStop`). Matches
    only in HEADERS (`_depth > 0`), like header_renames_symbol."""
    if not cpath or _depth > 4 or not os.path.exists(cpath):
        return None
    if _seen is None:
        _seen = set()
    rp = os.path.realpath(cpath)
    if rp in _seen:
        return None
    _seen.add(rp)
    define_re = re.compile(
        r"^\s*#\s*define\s+" + re.escape(primary) + r"\s*\([^)]*\)\s*(\S.*)$"
    )
    id_re = re.compile(r"[A-Za-z_]\w*")
    includes = []
    try:
        text = UpstreamSource.get(cpath).text
    except OSError:
        return None
    for line in text.splitlines():
        if _depth > 0:
            m = define_re.match(line)
            if m:
                ids = id_re.findall(m.group(1))
                return ids[0] if ids else None
        m = INCLUDE_RE.match(line)
        if m:
            includes.append(m.group(1))
    for inc in includes:
        hdr = _resolve_include(inc)
        if hdr:
            hit = _macro_alias_target(hdr, primary, _depth + 1, _seen)
            if hit:
                return hit
    return None


def header_renames_symbol(cpath, primary, _depth=0, _seen=None):
    """A (transitively-)included vendored header that rewrites the candidate's curated symbol via a
    macro `#define <primary>...` — a K->J source-compat shim (e.g. `os_host.h`:
    `#define __osInitialize_common() osInitialize()`). The macro bites ONLY the real body's function
    definition, so it is invisible
    to pick_target's other scans AND the gate stub build (an INCLUDE_ASM stub never compiles the
    body); it surfaces as a link symbol-mismatch (the caller wants the curated name, but the C
    exports the rewritten name). Returns the renaming header's basename so the gate prices a
    `#undef <primary>` enabler, or None. Recurses the `#include` tree (bounded depth + seen-set);
    matches `#define <primary>` only in HEADERS (`_depth > 0`), not the candidate `.c` itself.
    Scans only the PRIMARY (leader) name — both known instances rename the leader."""
    if not cpath or _depth > 4 or not os.path.exists(cpath):
        return None
    if _seen is None:
        _seen = set()
    rp = os.path.realpath(cpath)
    if rp in _seen:
        return None
    _seen.add(rp)
    define_re = re.compile(r"^\s*#\s*define\s+" + re.escape(primary) + r"\b")
    includes = []
    try:
        text = UpstreamSource.get(
            cpath
        ).text  # errors='ignore' (was 'replace'; immaterial for ASCII)
    except OSError:
        return None
    for line in text.splitlines():
        if _depth > 0 and define_re.match(line):
            return os.path.basename(cpath)
        m = INCLUDE_RE.match(line)
        if m:
            includes.append(m.group(1))
    for inc in includes:
        hdr = _resolve_include(inc)
        if hdr:
            hit = header_renames_symbol(hdr, primary, _depth + 1, _seen)
            if hit:
                return hit
    return None


def _append_coddog_trap_hazards(off, primary, cl_path, up_lib, hazards):
    """The file-level trap re-scan for a coddog-resolved upstream `.c` (file-static, defines-data,
    needs-header, recover-extern battery), in-place; returns whether a needs-header tag blocks the
    DoR. Shared by BOTH coddog paths so a coddog identity carried by an UN-NAMED tail member (the
    tail cod_members scan) prices its traps identically to one keyed on the primary — otherwise a
    tail-member coddog hit surfaces the bare coddog flag WITHOUT the trap battery (e.g. a
    defines-data `.data` carve on a sibling whose hit keys on a non-leader name)."""
    clib = up_lib or "libultra"
    if has_file_scope_static(cl_path):
        hazards.append(Hazard.file_static())
    cdefs = defines_data_globals(cl_path) + defines_local_static_data(
        cl_path
    )  # + fn-local statics
    if cdefs:
        hazards.append(Hazard.defines_data(cdefs))
    blocked = False
    ctagged, cblk = _tagged_missing_includes(cl_path, clib)
    if ctagged:
        hazards.append(Hazard.needs_header(ctagged))
        blocked = cblk
    _append_recover_hazards(off, primary, cl_path, clib, hazards)
    ren = header_renames_symbol(cl_path, primary)
    if ren and _upstream_defines_function(
        cl_path, primary
    ):  # skip macro-alias false fire
        hazards.append(Hazard.header_renames_symbol(primary, ren))
    elif (
        ren
    ):  # primary is a macro alias for a DIFFERENT upstream symbol → wrong ghidra name
        tgt = _macro_alias_target(cl_path, primary)
        if tgt and _upstream_defines_function(cl_path, tgt):
            hazards.append(Hazard.wrong_ghidra_name(primary, tgt, ren))
    return blocked


def _append_header_version_hazards(off, primary, up_path, up_lib, hazards):
    """Append the include / stale-header / needs-define / call-divergence / header-rename
    hazards for a named upstream candidate (in place, in order). Returns whether a missing
    header BLOCKS the candidate (an unresolved -I path)."""
    blocked = False
    tagged, hdr_blocked = _tagged_missing_includes(up_path, up_lib)
    if tagged:
        hazards.append(Hazard.needs_header(tagged))
        blocked = blocked or hdr_blocked
    stale = stale_version_header(up_path, up_lib)
    if stale:
        # A version-conditional fn silently dropped: os_version.h resolves but is a stripped revision
        # missing the referenced VERSION_* constant. A one-time additive header-content vendor, not a
        # block — keep it off `blocked` (it does not gate the DoR, only warns the gate).
        hazards.append(Hazard.stale_header(stale))
    gating = function_gating_define(up_path, primary)
    if gating and gating not in _active_defines_for_lib(up_lib):
        hazards.append(Hazard.needs_define(gating))
    gbi_def = gbi_value_guard_needs_define(up_path, up_lib)
    if gbi_def:
        hazards.append(
            Hazard.needs_define(gbi_def)
        )  # GBI-value-guarded macro (e.g. OS_YIELD_DATA_SIZE)
    divergence = call_divergence(off, primary, up_path, up_lib)
    if divergence:
        hazards.append(
            divergence
        )  # near-verbatim mirror: reconcile call list at the gate
    ren = header_renames_symbol(up_path, primary)
    if ren and _upstream_defines_function(
        up_path, primary
    ):  # skip macro-alias false fire
        hazards.append(Hazard.header_renames_symbol(primary, ren))  # needs #undef
    elif (
        ren
    ):  # primary is a macro alias for a DIFFERENT upstream symbol → wrong ghidra name
        tgt = _macro_alias_target(up_path, primary)
        if tgt and _upstream_defines_function(up_path, tgt):
            hazards.append(Hazard.wrong_ghidra_name(primary, tgt, ren))
    return blocked


def _append_rodata_carve_hazards(off, up_path, hazards):
    """Append the rodata-literal sibling-carve + data-static recover hazards for an upstream
    candidate (in place). Returns (rodata_lits, data_statics) for the twin-of section that
    follows."""
    # Anonymous `%lo(D_<addr>)` constant loads split into two enablers by segment:
    #   - code-segment `.rodata` → compiler-pooled literal the mirror re-emits → a `.rodata` sibling
    #     split at finalize (docs/hazards.md#rodata-sibling-yaml-pattern).
    #   - data segment → a function-local `static` the mirror re-emits → recover-extern + drop the
    #     static to a file-scope `extern` (docs/hazards.md#defines-data).
    # The FP-only scan (ldc1/lwc1) seeds both; integer `lw` refs that land in the rodata band add the
    # companion words of a pooled `double` (GCC's `lw` pair + `mtc1`) so the sibling-split extent is
    # sized in full, not just its first word. `lw` refs in the data segment are ordinary data refs
    # (refs-unplaced/defines-data already cover them) and are dropped here. Both scans span the whole
    # subseg (every pack function), since the `.rodata` sibling places the whole object's `.rodata`.
    rodata_lits, data_statics = [], []
    for a in rodata_literals(off):
        (rodata_lits if _literal_in_rodata(a, off) else data_statics).append(a)
    for a in rodata_word_refs(off):
        if _literal_in_rodata(a, off) and a not in rodata_lits:
            rodata_lits.append(a)
    # Carve-start widening: the FP-literal/lw scans see only scalar `%lo` loads, so min(rodata_lits)
    # under-states a rodata block that opens with a file-scope `static const` array base (an
    # `addiu %lo` address-of, missed by both scans) or string literals. When the upstream defines such
    # a file-PRIVATE const array, the whole code-segment rodata subseg is this object's own → widen
    # the carve-start to the subseg boundary. The `static const` source gate keeps it FP-safe (a bare
    # `addiu %lo` scan false-fires on cross-file .data).
    if rodata_lits and defines_file_static_const_array(up_path):
        start = _rodata_carve_start_vram(off, min(rodata_lits))
        if start is not None and start < min(rodata_lits):
            rodata_lits.append(start)
    if rodata_lits:
        # Pre-note the full vram extent as a DoR enabler so it is not a finalize-time SHA-miss.
        detail = ",".join(f"0x{a:08X}" for a in sorted(rodata_lits))
        # Append the carve-end boundary: the sibling-split runs to the next `.rodata` subseg
        # boundary, which can exceed the last `%lo`-referenced literal (a multi-`du` dlabel block's
        # trailing word has no ref of its own).
        end = _rodata_carve_end_vram(off, max(rodata_lits))
        if end and end > max(rodata_lits):
            detail += f";carve-end=0x{end:08X}"
        hazards.append(Hazard.rodata_literal(detail))
    if data_statics:
        # A function-local static the mirror must drop to a file-scope extern + recover.
        hazards.append(Hazard.data_static(data_statics))
    return rodata_lits, data_statics


def append_upstream_hazards(off, primary, up_lib, up_path, hazards, member_paths=()):
    """Append the upstream-mirror hazards for a named candidate (in-place, in the
    order the gate reads them) and return (band, blocked). `member_paths` are a c-combined pack's
    non-primary member upstreams: the file-static + defines-data + bare-assert scans union over them
    so a SECONDARY member file's file-scope static / defined global / bare assert is priced at the
    gate, not discovered at execution (e.g. sl.c's `alGlobals`, or a member's drop-static load, missed
    by a primary-only scan). The recover-extern / needs-header / call-divergence battery stays
    primary-keyed (member refs-unplaced is a separate deferred follow-up)."""
    blocked = False
    # file-static over the primary + c-combined members: a SECONDARY member's file-scope static is a
    # pack-level drop-static enabler, so the gate must see it — the same member-union the defines-data
    # scan below already does. A primary-only scan would miss it.
    if any(has_file_scope_static(p) for p in (up_path, *member_paths)):
        hazards.append(
            Hazard.file_static()
        )  # route to the classical loop, not the mirror
    # defines-data over the primary + c-combined members (+ fn-local statics).
    data_defs = list(
        dict.fromkeys(  # de-dup, order-preserving
            d
            for p in (up_path, *member_paths)
            for d in defines_data_globals(p) + defines_local_static_data(p)
        )
    )
    if data_defs:
        hazards.append(
            Hazard.defines_data(data_defs)
        )  # .data analogue → classical loop
    # File-scope NON-const initialized static arrays (e.g. xprintf spaces/zeroes) the verbatim mirror
    # re-emits → a `.data` sibling carve. Single-file ONLY (`not member_paths`): a c-combined pack's
    # per-member up_path can mis-attribute, so defer the multi-file case.
    if not member_paths:
        init_arrays = list(dict.fromkeys(defines_file_static_init_array(up_path)))
        if init_arrays:
            hazards.append(
                Hazard.data_carve(init_arrays)
            )  # .data carve at recovered vram
    # Bare (non-_DEBUG-guarded) asserts a verbatim mirror would compile in (assert-strip pre-flag).
    n_assert = sum(bare_asserts(p) for p in (up_path, *member_paths))
    if n_assert:
        hazards.append(Hazard.bare_assert(n_assert))
    _append_recover_hazards(off, primary, up_path, up_lib, hazards)
    mirror_dir = band_mirror_dir(up_lib, up_path)  # also reused below for band warmth
    blocked = (
        _append_header_version_hazards(off, primary, up_path, up_lib, hazards)
        or blocked
    )
    rodata_lits, data_statics = _append_rodata_carve_hazards(off, up_path, hazards)
    # twin-of hint: when the candidate re-emits a function-local static (data-static / rodata-literal)
    # AND its mirror dir already holds a banked sibling that carved the same ld-section, name that
    # sibling so the gate reaches for the established carve playbook instead of re-deriving it (e.g.
    # align.c was the verbatim twin of rotate.c — same `libultra/gu` dir, same `.data` dtor carve).
    # Prefer a same-section twin; else any.
    if data_statics or rodata_lits:
        tree = dict(UPSTREAM_TREES).get(up_lib, LIBULTRA)
        rel = os.path.relpath(up_path, tree)
        cand_dir = os.path.join(up_lib, os.path.dirname(rel))
        cand_stem = os.path.splitext(os.path.basename(rel))[0]
        sibs = _static_carve_siblings().get(cand_dir)
        if sibs:
            want = ".data" if data_statics else ".rodata"
            pool = sorted(sibs[want] - {cand_stem}) or sorted(
                (sibs[".data"] | sibs[".rodata"] | sibs[".bss"]) - {cand_stem}
            )
            if pool:
                hazards.append(Hazard.twin_of(pool[0]))
    # owner-per-member marker: the rodata-literal/jtbl scans span the WHOLE subseg, which is correct
    # for a single-file pack (one .c -> one .o -> one .rodata) but OVER-attributes for a c-combined
    # (multi-file) pack — both members' pooled rodata lands on the PRIMARY row, yet the carve owner is
    # the member file whose function actually references it (a carve-free primary can carry a sibling's
    # rodata-literal + rodata-jtbl). Mark the multi-file case so the gate does not carve the primary
    # `.c` by default — the true owner is confirmed at execution by the pre-carve build's
    # undefined-`.L<addr>` link-error (the jtbl `.word` entries name the owning function). Fires ONLY
    # when member_paths is non-empty (a c-combined pack), so a single-file pack is untouched. Full
    # per-member attribution + label-range-bounded carve-end: a BACKLOG tooling follow-up.
    if member_paths:
        for h in hazards:
            if h.is_rodata_owner():
                h.mark_owner_per_member()
    band = "warm" if band_is_warm(mirror_dir) else "cold"
    return band, blocked


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
    """Two advisory coddog passes over the resolved hazard set: flag a match to an ALREADY-BANKED
    source (the mirror is fully decompiled, so the hit is a fingerprint coincidence, not a fresh
    attribution), and flag a SUB-100 coddog-mirror as body-divergence-suspect (it may mask a
    game-modified body; the gate runs a store-value diagnosis before declaring a clean mirror or a
    compiler wall, S127)."""
    for s in sorted({c for c in cod_srcs if _coddog_source_banked(c)}):
        hazards.append(Hazard.coddog_source_banked(os.path.basename(s)))
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
            loc = _meaningful_loc(cl1) if cl1 else 0
            if loc and st.size > CODDOG_STRUCTURAL_BYTES_PER_LOC * loc:
                cpct = max(p for _, c, p in az if c == distinct[0])
                st.hazards.append(Hazard.coddog_structural(distinct[0], cpct))
        if st.up_lib is None:
            st.up_lib = lib


def _row_filtered(st, args, path, carried, cod_srcs):
    """Return True to DROP the row. Two skip conditions: (1) the --lib scope filter — a row whose
    path / coddog-source / up_lib / member names don't match the requested library; and (2) a
    de-ranked BACKLOG carry-over (retrieved via --include-stuck or the BACKLOG, NOT smallest-first;
    carry_over_names() is region+symbol scoped, and a definitively-coddog'd subseg whose leader was
    merely prose-mentioned overrides the drop via st.cod_members)."""
    cod_in_scope = (
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


def _build_row(
    off,
    typ,
    path,
    size,
    args,
    upstream_index,
    carried,
    sig_index,
    coddog_index,
    nusys_index,
    audio_indexes,
    audio_roots,
    _libultra_band_start,
):
    """Classify one subseg and produce its scored row dict, or None to skip it (bss,
    scope-filtered, or a de-ranked carry-over). The per-subseg body of build_rows;
    the resolved identity lives on a _RowState (st) the coddog/nusys passes rebind in
    place as they run."""
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


def build_rows(
    args,
    upstream_index,
    carried,
    sig_index,
    coddog_index=None,
    nusys_index=None,
    audio_indexes=None,
    audio_roots=None,
):
    coddog_index = coddog_index or {}
    nusys_index = nusys_index or {}
    audio_indexes = audio_indexes or {}
    audio_roots = audio_roots or {}
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
            off,
            typ,
            path,
            size,
            args,
            upstream_index,
            carried,
            sig_index,
            coddog_index,
            nusys_index,
            audio_indexes,
            audio_roots,
            _libultra_band_start,
        )
        if row is not None:
            rows.append(row)
    rows.sort(key=lambda r: (-r["score"], r["size"]))
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

    rows = build_rows(
        args,
        build_upstream_index(),
        carry_over_names(),
        build_signature_index(),
        build_coddog_index(),
        build_coddog_nusys_index(),
        build_audio_indexes(),
        build_audio_pin_roots(),
    )
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
