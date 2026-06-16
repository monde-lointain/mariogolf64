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
__osCurrentHandle, S41) is flagged, not deferred to a mid-execution link failure.

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
import sys

# Asm-scanning layer (the per-`asm/<ROM>.s` body walk + the scans over it). Self-contained
# and stdlib-only; imported here so this module keeps the yaml/upstream/hazard/scoring logic.
# _FRAME_IMMS is shared with the C-side _c_signature below.
from decomp_asm import (
    _asm_jal_count,
    _asm_signature,
    _FRAME_IMMS,
    asm_functions,
    asm_function_addrs,
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

# C-source text strippers (extracted from this module; re-imported so call sites here read
# unchanged). Pure string->string, stdlib-only; see tools/cpreprocess.py.
from cpreprocess import (
    _strip_comments,
    _strip_dead_blocks,
    _strip_define_lines,
    _strip_string_literals,
)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
YAML = os.path.join(ROOT, "mariogolf64.yaml")
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
NAME_FILES = [os.path.join(ROOT, "symbol_addrs.txt"), os.path.join(ROOT, "ghidra_symbols.txt")]
# S71: optional coddog cross-ref map (mgname<TAB>ulname<TAB>ulfile<TAB>pct), written by
# tools/coddog_sweep.sh. Absent by default → build_coddog_index() returns {} and ranking is
# unchanged. A ≥CODDOG_MIRROR_PCT non-audio hit re-prices an un-named/none candidate as a libultra
# mirror (crc.c was mis-seeded pts-13 classical; coddog proved it a verbatim 2-fn mirror). See
# docs/hazards.md#coddog-cross-ref.
CODDOG_MAP = os.environ.get("CODDOG_MAP", os.path.join(ROOT, "tools/coddog/coddog_map.tsv"))
CODDOG_MIRROR_PCT = 99.0

# Subseg line: `- [0x8DF10, c, libultra/monegi/rdp/dp]` or `- [0x8D230, asm]`.
# (bss entries use the `{ type: bss, ... }` brace form and are intentionally skipped.)
SUBSEG_RE = re.compile(
    r"^\s*-\s*\[\s*0x([0-9A-Fa-f]+)\s*,\s*([a-z]+)\s*(?:,\s*([^\]]+?))?\s*\]"
)
# Approximate C function-definition: a return-type-led line ending in `name(`.
UPSTREAM_DEF_RE = re.compile(r"^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w+)\s*\(", re.M)
# `#pragma weak <alias> = <impl>` exports <alias> at the implementation's address. The def loop
# only keys the impl name (gu/cosf.c defines `fcos`/`__cosf`), so the ROM/curated weak alias `cosf`
# was absent from the upstream index → a pack member read `cosf=?` and hid cosf.c from the
# c-combined member labels (S66). Keying the alias too lets a weak-aliased SDK fn resolve to its
# upstream file (cosf/sinf/fcos/fsin), the C analog of ASM_TU_DEF_RE's WEAK arm (bcopy → _bcopy).
PRAGMA_WEAK_RE = re.compile(r"^\s*#pragma\s+weak\s+([A-Za-z_]\w+)\s*=\s*[A-Za-z_]\w+", re.M)
# A hand-asm TU's exported entry: `LEAF(osGetCount)` / `XLEAF(name)` / `WEAK(bcopy, _bcopy)`
# (sys/asm.h macros). The WEAK arm catches the public name of an aliased TU (bcopy → _bcopy).
# Keys build_asm_tu_index so an intrinsic-likely shim can name its vendorable ultralib .s TU.
ASM_TU_DEF_RE = re.compile(r"^\s*(?:X?LEAF|WEAK)\(\s*([A-Za-z_]\w+)", re.M)
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
    for d in ("include", "include/libultra", "include/libultra/PR", "include/libultra/compiler/gcc")
]
FILE_STATIC_RE = re.compile(r"^\s*static\b[^=]*;\s*$")
# A file-scope data-global *definition* (external linkage): `<type> <name> [= init];`.
# Applied only at brace-depth 0 (see defines_data_globals) so function-body statements
# and struct members never match. Captures the variable name.
DATA_GLOBAL_DEF_RE = re.compile(
    r"^(?:(?:struct|union|enum)\s+)?[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w+)"
    r"\s*(?:\[([^\]]*)\]\s*)?(?:=[^;]*)?;\s*$"
)
# S73: a function-local *initialized* static: `static <type> <name> [= init];` inside a body.
# defines_data_globals can't see it (it skips `static` lines AND scans only brace-depth 0), but
# KMC GCC 2.7.2 emits it into the TU's .data exactly like a file-scope global, so a verbatim mirror
# re-emits the bytes and needs the same .data sibling carve. The non-greedy prefix backtracks so the
# captured name is the last identifier before `=` (gu/position.c's `static float dtor = π/180`).
LOCAL_STATIC_DATA_RE = re.compile(
    r"^static\b[^;{}]*?\b([A-Za-z_]\w+)\s*(?:\[[^\]]*\])?\s*=[^;]*;\s*$"
)
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
# The project's quoted/angle include search dirs (Makefile CFLAGS `-I` set; -nostdinc removed S32 — stdarg.h ships at include/stdarg.h for correct MIPS GCC 2.7.2 vararg ABI).
INCLUDE_DIRS = [
    os.path.join(ROOT, d)
    for d in ("include", "include/libultra", "include/libultra/internal", "include/libkmc", "include/libnusys")
]
PROJECT_INC = os.path.join(ROOT, "include")
# Per-lib extra `-I` dirs the Makefile's profile CFLAGS add on top of the base INCLUDE_DIRS, so a
# mirror's include resolution must match the profile it actually compiles under. LIBULTRA_CFLAGS
# (Makefile) prepends `-I include/libultra/compiler/gcc` and appends `-I include/libultra/PR`, so a
# libultra mirror resolves <libaudio.h>/<os_internal.h>/<ultraerror.h> and the rest of the PR/ band
# through these — missing_includes must include them for a libultra candidate or it false-flags
# needs-header → blk (S53: alHeapDBAlloc + the whole audio/PR mirror band were hidden this way).
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
    # S46: the PI-band PRinternal/piint.h copy came from here.
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
# vendorable (a cheap source-relative cp), not a blocked DoR reject (S49 guint.h, S54 xstdio.h).
UPSTREAM_SRC_ROOTS = [
    LIBULTRA,  # ~/development/repos/ultralib/src
]
UPSTREAM_BONUS = 200
# The libultra boot-region globals (fixed RAM 0x80000300-0x8000031C). A verbatim mirror references
# these by name (osRomBase, osMemSize, …) but the asm bakes them as raw lui/addiu immediates that
# splat auto-labels `D_800003xx` — so refs_unplaced's `__`-prefix / declared-extern grep misses
# them and the mirror link-fails on first compile. Surfacing them with their known vram lets the
# gate add the recover-extern from this table, no asm-data-recovery pass needed (S46: osRomBase).
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
            with open(self.path, errors="ignore") as f:  # OSError propagates; callers keep try/except
                self._text = f.read()
            self._read = True
        return self._text


# --- Hazards ---------------------------------------------------------------
# A hazard is a (kind, optional detail) pair, not a formatted string: the kind is
# one of the constants below (the shared vocabulary producers emit and seed_points
# classifies on), and the detail carries the kind-specific payload (the symbol
# list, the count pair, the upstream short-list). render() is the ONLY place a
# hazard becomes a string — `kind` for a bare flag, `kind:detail` otherwise — so
# the printed/JSON form stays exactly what the gate + the hazard index read.
HAZARD_FILE_STATIC = "file-static"
HAZARD_DEFINES_DATA = "defines-data"
HAZARD_REFS_UNPLACED = "refs-unplaced"
HAZARD_CALLS_UNPLACED = "calls-unplaced"
HAZARD_NEEDS_HEADER = "needs-header"
HAZARD_STALE_HEADER = "stale-header"
HAZARD_NEEDS_DEFINE = "needs-define"
HAZARD_JAL_COUNT_MISMATCH = "jal-count-mismatch"
HAZARD_PACK = "pack"
HAZARD_SINGLE_FILE_PACK = "single-file-pack"
HAZARD_ONE_TU = "one-tu"  # S88: every inner fn boundary of a pack is non-16-aligned → ONE .o (the
# linker 16-aligns each .o's .text start, so any second .o begins on a 16 boundary). Confirms a
# single-file-pack structurally even when members are un-named (the coddog-mirror case), AND it is
# the S51 non16align signal that a per-fn decompose split is mechanically blocked.
HAZARD_CODDOG_FNCOUNT_MISMATCH = "coddog-fncount-mismatch"  # S88: a coddog-mirror file defines FEWER
# fns than the pack holds (settime.c 1fn coddog-matched a 6fn os pack) → a structural fingerprint
# match, NOT a source attribution; the pack's real source is multi-file. Under-count direction only
# (a true single source can define MORE, via version/_DEBUG-gated extras, e.g. contpfs 9 vs ROM 7).
# S92: the under-count check now ALSO fires when the coddog identity is TAIL-carried (func_80050400's
# leader is not in coddog_index; a tail member carries llcvt.c, 8 stubs vs the 11fn pack) — the old
# check ran only in the primary block, so a tail-carried phantom surfaced as a clean single source.
HAZARD_CODDOG_STRUCTURAL = "coddog-structural"  # S92: a coddog-mirror hit whose matched source's
# expected compiled size is far below the matched subseg's (llcvt.c's ~8 trivial `return d;` stubs,
# ~250B, vs a 2032B/11fn subseg — coddog matched the same llcvt.c to THREE distinct subsegs) → a
# structural fingerprint match, not a source attribution. The size-dimension companion to
# coddog-fncount-mismatch (the fn-count dimension); advisory (display-only, does not re-price).
CODDOG_STRUCTURAL_BYTES_PER_LOC = 64  # subseg bytes / source meaningful-LOC above this, on a
# single-identity multi-fn pack, marks the coddog match structural (conservative: ~4x a dense -O2 fn)
HAZARD_COMBINED_SUBSEG = "combined-subseg"
HAZARD_C_COMBINED = "c-combined"
HAZARD_NON16ALIGN = "non16align"
HAZARD_INTRINSIC_LIKELY = "intrinsic-likely"
HAZARD_MAYBE_UPSTREAM = "maybe-upstream"
HAZARD_RODATA_LITERAL = "rodata-literal"
HAZARD_RODATA_JTBL = "rodata-jtbl"  # S76: a switch jump table the mirror re-emits → .rodata sibling carve
HAZARD_DATA_STATIC = "data-static"
HAZARD_TWIN_OF = "twin-of"
HAZARD_REMAINING = "remaining"
HAZARD_CODDOG_MIRROR = "coddog-mirror"  # S71: a coddog compare2 match to an ultralib fn
HAZARD_CODDOG_TWIN = "coddog-twin"  # S81: coddog matched a near-identical TWIN file (piacs.c) but
# the named members name the real source (siacs); mirror from the member-named source, not the file
HAZARD_CALLER_EVICT = "caller-evict"  # S77: an un-named func_ member a banked C file calls by name
HAZARD_TRAILING_PAD = "trailing-pad"  # S79: trailing nop pad to a higher-aligned next subseg a C mirror can't emit
HAZARD_DROP_STATIC_MIRROR = "drop-static-mirror"  # S87: a coddog-confirmed verbatim mirror whose
# file-static + defines-data + refs-unplaced cluster is ONE S81 drop-to-extern enabler (pure .bss,
# no carve) — re-frames the scary cluster so the gate prices it as a seed-only N-symbol mirror, not
# a carve/classical spike; see docs/hazards.md#file-static-bss-layout-conflict
HAZARD_HEADER_RENAMES_SYMBOL = "header-renames-symbol"  # S85: a (transitively-)vendored header
# rewrites the candidate's curated symbol via a macro `#define <curated_fn>(...) <other>` (os_host.h
# K->J shim `#define __osInitialize_common() osInitialize()`; S31 nuGfxInit was the 1st instance) →
# needs a `#undef <curated_fn>` enabler in the mirror; see docs/hazards.md#header-renames-symbol


@dataclasses.dataclass
class Hazard:
    """One flagged hazard: a `kind` constant + an optional `detail` payload.

    `detail` holds everything after the kind's colon verbatim — a comma list
    (`defines-data:a,b`), a count pair (`jal-count-mismatch:2vs1`), a bracketed
    pack (`pack:2fn[a=foo]`), or a colon-bearing short-list (`maybe-upstream:
    libultra:f1,f2`). Keep it a single string; render() does not re-split it."""

    kind: str
    detail: str | None = None

    def render(self) -> str:
        return self.kind if self.detail is None else f"{self.kind}:{self.detail}"


def parse_subsegs():
    """Return all `[0x..,type,..]` subsegs as (rom_off:int, type, path|None), sorted by offset."""
    subs = []
    with open(YAML) as f:
        for line in f:
            m = SUBSEG_RE.match(line)
            if not m:
                continue
            off = int(m.group(1), 16)
            path = m.group(3).strip() if m.group(3) else None
            subs.append((off, m.group(2), path))
    subs.sort(key=lambda s: s[0])
    return subs


@functools.lru_cache(maxsize=1)
def _rodata_rom_ranges():
    """[(start_rom, end_rom)] of every `rodata`/`.rodata` subseg, for classifying a `%lo(D_<vram>)`
    literal. A constant whose ROM offset (vram − the fn's code-segment delta) lands in one of these
    ranges is a compiler-pooled rodata literal the mirror re-emits → a `.rodata` sibling split
    (S38/S48); one that lands elsewhere is a function-local `static` the mirror re-emits into the
    data segment → a recover-extern + file-scope-extern drop (S49). The two enablers differ, so the
    gate must be routed to the right one."""
    subs = parse_subsegs()
    ranges = []
    for i, (off, typ, _path) in enumerate(subs):
        if typ in ("rodata", ".rodata"):
            end = subs[i + 1][0] if i + 1 < len(subs) else off
            ranges.append((off, end))
    return tuple(ranges)


def _literal_in_rodata(vram, off):
    """True when the `%lo(D_<vram>)` constant loaded by the fn at ROM `off` lives in the code
    segment's pooled `.rodata` (vs. the data segment). Text and pooled rodata share one segment
    delta, so vram − (fn_vram − off) is the constant's ROM offset; test it against the rodata
    subseg ranges. Returns False when the fn's vram is unknown (asm listing absent)."""
    fn_vram = subseg_vram(off)
    if fn_vram is None:
        return False
    rom = vram - (fn_vram - off)
    return any(start <= rom < end for start, end in _rodata_rom_ranges())


def _rodata_carve_end_vram(off, lit_vram):
    """VRAM of the rodata-subseg boundary that ends the carve containing `lit_vram` (S64 #2).

    The rodata-literal scan reports the `%lo(D_…)`-referenced literals, but the sibling-split's
    carve extends to the next `.rodata` subseg boundary, which can run past the last referenced
    literal: a multi-`du` dlabel block's trailing word has no `%lo` of its own (S64 lookathil —
    the 0x800D2508 `.double 0` inside the D_800D2500 block, so the scan's max 0x800D2500 understated
    the 0x800D2510 carve end = the lookatref `.rodata` boundary). Pre-noting the boundary turns the
    finalize-time `.o`-sized carve into a planned extent at the gate. Returns the end vram (upper
    bound: the whole subseg end, exact when the mirror's literals are the subseg tail, the common
    case), or None when the fn vram is unknown or the literal is not in a code-segment rodata range."""
    fn_vram = subseg_vram(off)
    if fn_vram is None:
        return None
    delta = fn_vram - off
    rom = lit_vram - delta
    for start, end in _rodata_rom_ranges():
        if start <= rom < end:
            return end + delta
    return None


def build_upstream_index():
    """Map upstream function name -> (lib, path), scanning libultra/libkmc once.

    Two determinism fixes (S60). (1) Sort the glob so a name defined in multiple upstream files
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
            # Weak aliases export the ROM-visible name at the impl's address (S66 cosf/sinf).
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


_INTREE_ASM_MACROS = None


def _intree_asm_macros():
    """Every `#define`d name reachable under the vendored-asm `-I` set (ASM_VENDOR_INCLUDE_DIRS).

    Cached. The denominator for vendorable_tu_missing_defines: a macro a vendored .s references but
    that appears in none of these headers can't assemble under LIBULTRA_ASFLAGS."""
    global _INTREE_ASM_MACROS
    if _INTREE_ASM_MACROS is None:
        macros = set()
        for d in ASM_VENDOR_INCLUDE_DIRS:
            for h in glob.glob(os.path.join(d, "**", "*.h"), recursive=True):
                try:
                    macros.update(re.findall(r"#define\s+(\w+)", open(h, errors="ignore").read()))
                except OSError:
                    continue
        _INTREE_ASM_MACROS = macros
    return _INTREE_ASM_MACROS


def vendorable_tu_missing_defines(rel):
    """Sorted UPPER_CASE macros a vendorable ultralib .s (path relative to LIBULTRA) references but
    that the in-tree asm `-I` headers don't define — a needs-define enabler the gate must satisfy
    before vendoring (S57 retro #1). Strips C comments + `#include` lines first so a `/* TLB */`
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
    # S84 #2: a macro the .s `#define`s itself (e.g. setintmask.s's `#define MI_INTR_MASK ...`,
    # often unused) is not a missing enabler — subtract the TU's own defines before flagging.
    local = set(re.findall(r"#define\s+(\w+)", text))
    return sorted({t for t in MACRO_REF_RE.findall(text)} - defined - local)


def vendorable_tu_data_symbols(rel):
    """Exported symbols a vendorable ultralib .s (path relative to LIBULTRA) defines in a NON-.text
    section (.rdata/.rodata/.data/.sdata/.bss) — a has-rodata enabler (S84 #3). A vendored .s with
    such a section is NOT a clean .text-only VENDOR_ASM cp: splat auto-links a hasm .o's data
    sections at the END of each output section (out of address order), so the bytes duplicate +
    misplace -> SHA break. The fix (docs/hazards.md#asm-mirror-vendoring) vendors .text only +
    strips the data block, keeping that data as the existing extracted generic blob renamed to this
    symbol via a `symbol_addrs` add. Flagging it prices the strip+rename enabler at the gate rather
    than discovering it at a failing vendor-build (S84 setintmask / __osRcpImTable). Empty for a
    .text-only TU (the whole current vendoring backlog)."""
    if not rel:
        return []
    spath = os.path.join(LIBULTRA, rel)
    try:
        text = open(spath, errors="ignore").read()
    except OSError:
        return []
    text = C_COMMENT_RE.sub("", text)
    # S91 #1: scan only the ACTIVE data sections. A `#ifndef _FINALROM` / version-gated EXPORT
    # (S91: exceptasm.s's `__osCauseTable_pt` lives in `#ifndef _FINALROM`) is NOT emitted under
    # MG64's `-D_FINALROM -DBUILD_VERSION=VERSION_J` asm profile, so listing it over-counts the
    # rodata-strip enabler. Drop the dead + inactive-version blocks before the section scan so the
    # priced has-rodata set is what the vendored `.o` actually carries, not the all-branches union.
    text = _strip_inactive_version_branches(_strip_dead_blocks(text), _build_version_ord("libultra"))
    m = re.search(r"^\s*\.(rdata|rodata|data|sdata|bss|sbss)\b", text, re.M)
    if not m:
        return []
    tail = text[m.end():]
    syms = re.findall(r"(?:EXPORT|XLEAF|glabel|dlabel)\s*\(?\s*(\w+)", tail)
    syms += re.findall(r"\.globl\s+(\w+)", tail)
    return sorted(set(syms))


# `.word` operand that is a label reference (not a numeric literal / arithmetic). A symbolic-pointer
# table — its operands are identifiers, optionally with +/- displacement.
_WORD_LABEL_OPERAND_RE = re.compile(r"^[A-Za-z_]\w*(?:\s*[+-]\s*\w+)?$")


def vendorable_tu_jtbl(rel):
    """Sorted heads of any SYMBOLIC-pointer table (`.word <label>, …`) a vendorable ultralib `.s`
    defines in its ACTIVE `.rdata`/`.rodata`/`.data` section (S91 #2). Such a table — a switch jump
    table (`__osIntTable: .word redispatch, sw1, …`, the active `__osException` `jr`s through it) or
    any function-pointer table — is what makes a `.text`-only asm-mirror a SPIKE, not an S84
    strip-and-rename: splat extracts the table to a SEPARATE rodata blob with SYMBOLIC `.word .L…`
    entries pointing at `.text`-internal labels; vendoring the `.text` as hasm makes `asm/<rom>.s`
    vestigial so those labels vanish → the blob's refs go UNDEFINED at link, and the table can't be
    carve-placed either (the VENDOR_ASM `.o` is `build/asm/<rom>.o`, no src-path subseg; a hasm `.o`'s
    rodata auto-links at the section END → wrong addr → SHA break). So this is the negative signal: a
    heavy asm-mirror like exceptasm is NOT the S84 has-rodata replay its `data_symbols` flag suggests.
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
    text = _strip_inactive_version_branches(_strip_dead_blocks(text), _build_version_ord("libultra"))
    m = re.search(r"^\s*\.(rdata|rodata|data|sdata)\b", text, re.M)
    if not m:
        return []
    heads, cur = [], None
    for line in text[m.end():].splitlines():
        s = line.strip()
        if not s:
            continue
        lbl = re.match(r"(?:EXPORT|XLEAF|glabel|dlabel)\s*\(?\s*(\w+)|(\w+)\s*:", s)
        if lbl:
            cur = lbl.group(1) or lbl.group(2)
            continue
        mw = re.match(r"\.word\s+(.+)$", s)
        if mw and any(_WORD_LABEL_OPERAND_RE.match(op.strip()) for op in mw.group(1).split(",")):
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
    "if", "for", "while", "switch", "return", "sizeof", "do", "else", "case",
    "defined", "OS_LOG_FLOAT", "assert",
    # Inline predicate macros that expand to field comparisons, emitting NO jal — counting them
    # as C calls over-inflates the jal-count C side (the S42 osSendMesg `MQ_IS_FULL` 7vs6 FP) and
    # pollutes the maybe-upstream callee signature. They carry no cross-build identity.
    "MQ_IS_FULL", "MQ_IS_EMPTY",
    # GCC attribute artifacts surfaced by macro_hidden_text: the macros.h `ALIGNED(x)` /
    # `STACK(...)` family expand to `__attribute__((aligned(x)))`, so the lowercase `aligned`
    # attribute keyword (and `__attribute__` itself) read as `name(` call tokens and mis-flag as
    # an unplaced callee (S87 vimgr: `static STACK(...) ALIGNED(0x10)` → phantom `calls-unplaced:aligned`).
    # The macro NAMES (ALIGNED/STACK/ARRLEN/ALIGN8) are already dropped via macro_names; these are
    # their expansion residue, which macro_names does not cover.
    "aligned", "__attribute__",
}


def _c_signature(body):
    """(callee set, constant set) for an SDK function body (C source)."""
    callees = {n for n in C_CALL_RE.findall(body) if n not in _C_NONCALL}
    consts = {c.lower() for c in C_INT_RE.findall(body) if c.lower() not in _FRAME_IMMS}
    return callees, consts


def _iter_upstream_functions(text):
    """Yield (name, body) for each top-level function in an SDK .c, body = brace-matched."""
    for m in UPSTREAM_DEF_RE.finditer(text):
        brace = text.find("{", m.end() - 1)
        if brace < 0:
            continue
        depth, i = 0, brace
        while i < len(text):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        yield m.group(1), text[brace:i + 1]


@functools.lru_cache(maxsize=None)
def _upstream_file_func_count(lib, cpath):
    """Count top-level function DEFINITIONS in an upstream .c (version-active branches only).

    Used by the single-file-pack test to recognize a pack whose trailing member(s) are unnamed
    (`func_<addr>`): if the pack's one named C stem defines exactly nfns functions, the unnamed
    members are that file's other functions, so the pack atomic-mirrors the whole file (S68 #3 —
    the gu F-variant + s16-wrapper idiom: `guAlignF` named + `func_<addr>` = `guAlign`, both in
    align.c). Conservative — an exact count match only; a mismatch falls back to the `pack` flag."""
    try:
        with open(cpath, errors="ignore") as f:
            text = f.read()
    except OSError:
        return 0
    text = _strip_inactive_version_branches(text, _build_version_ord(lib))
    return sum(1 for _ in _iter_upstream_functions(text))


@functools.lru_cache(maxsize=None)
def _meaningful_loc(cpath):
    """Count non-blank, non-comment, non-preprocessor source lines of an upstream .c — a crude proxy
    for its compiled .text size, used by the S92 coddog-structural size-ratio guard. Line-based
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
    r"^\s*-\s*\[\s*0x[0-9A-Fa-f]+\s*,\s*(\.(?:data|rodata|bss))\s*,\s*([^\]]+?)\s*\]")


@functools.lru_cache(maxsize=1)
def _static_carve_siblings():
    """{yaml_dir: {".data"/".rodata"/".bss": {basename,...}}} for every ld-section sibling subseg —
    the dirs already holding a banked file with a proven static carve. Feeds the twin-of hint
    (S68 #2): a candidate that re-emits a function-local static (data-static / rodata-literal)
    whose mirror dir already carved the same section type has an established playbook (align.c was
    the verbatim twin of S61 rotate.c — same `libultra/gu` dir, same `.data` dtor carve)."""
    out = {}
    with open(YAML) as f:
        for line in f:
            m = _LD_SIBLING_RE.match(line)
            if not m:
                continue
            d, b = os.path.split(m.group(2).strip())
            out.setdefault(d, {".data": set(), ".rodata": set(), ".bss": set()})[m.group(1)].add(b)
    return out


def build_coddog_index():
    """S71: load the optional coddog cross-ref map -> {mgname: (ulfile, pct)}.

    The map (tools/coddog/coddog_map.tsv, written by tools/coddog_sweep.sh) pairs each MG64
    function with the ultralib fn coddog matched it to. Absent/garbled lines are skipped; an
    absent file returns {} (ranking unchanged). Unlike signature_hint's IDF guess, a coddog hit is
    an actual instruction-hash match — definitive enough to re-price a none-classified verbatim
    mirror (see build_rows). Format per line: mgname<TAB>ulname<TAB>ulfile<TAB>pct."""
    idx = {}
    try:
        with open(CODDOG_MAP, errors="ignore") as f:
            for line in f:
                parts = line.rstrip("\n").split("\t")
                if len(parts) != 4:
                    continue
                mg, _ul, ulfile, pct = parts
                try:
                    idx[mg] = (ulfile, float(pct))
                except ValueError:
                    continue
    except OSError:
        return {}
    return idx


def _coddog_upstream_path(cfile):
    """Resolve a coddog map `ulfile` (e.g. `src/io/piacs.c`, repo-root-relative) to an on-disk
    path, or None. The map's paths are relative to the ultralib repo root; LIBULTRA is `<root>/src`.
    Used by build_rows to re-run the file-level trap detectors on a coddog-matched upstream."""
    p = os.path.join(os.path.dirname(LIBULTRA), cfile)
    return p if os.path.isfile(p) else None


def _append_coddog_twin_hazard(cfile, fns, upstream_index, hazards):
    """S81: coddog (compare2, reloc-masked) can match a near-identical TWIN file rather than the
    candidate's real source -- the SI access-queue subseg (`func_800AC110`+`__osSiGetAccess`+
    `__osSiRelAccess`) coddog-matched `src/io/piacs.c@99.99`, but its named members name `siacs`.
    Cross-check the coddog-matched basename against the pack's *named* members' upstream basenames;
    on disagreement flag `coddog-twin:<matched>!=<member-src>` so the gate mirrors from the
    member-named source (siacs.c), not the coddog file (piacs.c). A @99.99 twin is byte-identical
    once placed, so the body is the same either way -- this only removes a manual SI/PI reconcile
    step. No-ops when no member is named (nothing to disagree with) or the basenames agree."""
    cstem = os.path.splitext(os.path.basename(cfile))[0]
    member_stems = []
    for fn in fns:
        _, fn_up = upstream_index.get(fn, (None, None))
        if fn_up:
            s = os.path.splitext(os.path.basename(fn_up))[0]
            if s not in member_stems:
                member_stems.append(s)
    if member_stems and cstem not in member_stems:
        hazards.append(Hazard(HAZARD_CODDOG_TWIN,
                              f"{cstem}!={','.join(sorted(member_stems))}"))


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
    return Hazard(HAZARD_MAYBE_UPSTREAM, f"{lib}:" + ",".join(bases))


# --- Upstream-vs-ROM call-divergence (the S18 nuContInit catch) ----------------------
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

_IFDEF_OPEN_RE = re.compile(r'^\s*#\s*ifdef\s+([A-Za-z_][A-Za-z0-9_]*)')
_PP_OPENING_DIRECTIVE_RE = re.compile(r'^\s*#\s*if(?:def|ndef)?\b')
_PP_ENDIF_LINE_RE = re.compile(r'^\s*#\s*endif\b')
_MAKEFILE_DEFINE_RE = re.compile(r'-D([A-Za-z_][A-Za-z0-9_]*)(?:=[^\s]*)?')


# Version-conditional stripping. The build compiles ONE side of `#if BUILD_VERSION <op> VERSION_X`
# (libultra is `-DBUILD_VERSION=VERSION_J`), so the inactive side's refs/calls are phantom — they
# never reach the real object. S47 `osCartRomInit`: `CartRomHandle` + `osPiRawReadIo` live only in
# the non-J `#else` branch, yet refs/calls-unplaced flagged them. Only the *exact* form
# `BUILD_VERSION <op> VERSION_X` is evaluated; a compound condition (`&&`/`||`) or any other `#if`
# is opaque (both branches kept, nesting tracked so #else/#endif still pair correctly). A lib with
# no `-DBUILD_VERSION` (libkmc/libnusys) yields ordinal 0 → no-op (those upstreams don't gate on it).
_VERSION_ORD = {f"VERSION_{c}": i for i, c in enumerate("DEFGHIJKL", start=1)}
_BUILD_VERSION_VAL_RE = re.compile(r"-DBUILD_VERSION=(VERSION_[A-Z])\b")
_BUILD_VERSION_IF_RE = re.compile(
    r"^\s*#\s*if\s+BUILD_VERSION\s*(>=|<=|==|!=|>|<)\s*(VERSION_[A-Z])\s*$"
)
_PP_ELSE_LINE_RE = re.compile(r"^\s*#\s*else\b")
_LIB_CFLAGS_VAR = {
    "libultra": "LIBULTRA_CFLAGS",
    "libkmc": "LIBKMC_CFLAGS",
    "libnusys": "LIBNUSYS_CFLAGS",
}


@functools.cache
def _build_version_ord_by_var():
    """{Makefile CFLAGS var: BUILD_VERSION ordinal} from `-DBUILD_VERSION=VERSION_X` (cached)."""
    out = {}
    try:
        with open(os.path.join(ROOT, "Makefile")) as f:
            for line in f:
                m = re.match(r"^\s*(CFLAGS|LIBKMC_CFLAGS|LIBNUSYS_CFLAGS|LIBULTRA_CFLAGS)\s*[:+]?=", line)
                if m:
                    v = _BUILD_VERSION_VAL_RE.search(line)
                    if v:
                        out[m.group(1)] = _VERSION_ORD.get(v.group(1), 0)
    except OSError:
        pass
    return out


def _build_version_ord(lib):
    """BUILD_VERSION ordinal effective for an upstream library (0 if the lib sets no -DBUILD_VERSION)."""
    return _build_version_ord_by_var().get(_LIB_CFLAGS_VAR.get(lib or "", "CFLAGS"), 0)


def _eval_build_version(op, ver, ord_):
    rhs = _VERSION_ORD.get(ver, 0)
    return {
        ">=": ord_ >= rhs, "<=": ord_ <= rhs, ">": ord_ > rhs,
        "<": ord_ < rhs, "==": ord_ == rhs, "!=": ord_ != rhs,
    }[op]


def _strip_inactive_version_branches(text, build_ord):
    """Drop the dead side of `#if BUILD_VERSION <op> VERSION_X [#else] #endif` for `build_ord`.
    Non-BUILD_VERSION conditionals pass through unchanged (both branches kept); nesting is tracked
    so #else/#endif pair to the right directive. build_ord 0 → no-op."""
    if not build_ord:
        return text
    out, stack = [], []  # frame: {"ver": bool, "emit": bool}; line live iff all frames emit
    live = lambda: all(fr["emit"] for fr in stack)
    for line in text.splitlines():
        s = line.lstrip()
        m = _BUILD_VERSION_IF_RE.match(s)
        if m:
            stack.append({"ver": True, "emit": _eval_build_version(m.group(1), m.group(2), build_ord)})
            continue  # drop the #if directive itself
        if _PP_OPENING_DIRECTIVE_RE.match(s):
            keep = live()
            stack.append({"ver": False, "emit": True})
            if keep:
                out.append(line)
            continue
        if _PP_ELSE_LINE_RE.match(s):
            if stack and stack[-1]["ver"]:
                stack[-1]["emit"] = not stack[-1]["emit"]
                continue  # drop the #else of a resolved version branch
            if live():
                out.append(line)
            continue
        if _PP_ENDIF_LINE_RE.match(s):
            if stack:
                ver = stack.pop()["ver"]
                if not ver and live():
                    out.append(line)
                continue
            out.append(line)
            continue
        if live():
            out.append(line)
    return "\n".join(out)


_DEFINE_VERSION_RE = re.compile(r"^\s*#\s*define\s+(VERSION_[A-Z])\b")


@functools.cache
def _os_version_defined_tokens(lib):
    """The VERSION_<X> tokens the os_version.h resolvable on a `lib` candidate's effective -I set
    actually `#define`s. The denominator for stale_version_header: the in-tree os_version.h can
    resolve as a FILE (so needs-header stays silent) yet be a stripped revision that defines none of
    the VERSION_* constants the gcc.mk profile expects (S60: the 2.0L header). Empty frozenset when
    no os_version.h resolves at all."""
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
    reads each as 0 and silently mis-evaluates the guard. S60: gu/mtxcatl.c's `#if BUILD_VERSION <
    VERSION_K` evaluated `0 < 0` against the stripped 2.0L os_version.h and dropped guMtxXFML to a
    link error. A `stale-header:os_version.h(VERSION_K)` hazard = vendor the missing constant(s) into
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
    """Return {lib: frozenset(defines)} by parsing the project Makefile (cached:
    one parse per process). 'libkmc'/'libnusys'/'libultra' each inherit from the main
    CFLAGS set plus their own profile additions. The libultra profile carries
    -DBUILD_VERSION + -DF3DEX_GBI_2 (S83) — without parsing LIBULTRA_CFLAGS the GBI
    microcode define is invisible, so a GBI-value-guarded macro (OS_YIELD_DATA_SIZE)
    false-flags as needs-define on a build that already defines it."""
    makefile = os.path.join(ROOT, "Makefile")
    raw = {"main": set(), "libkmc": set(), "libnusys": set(), "libultra": set()}
    try:
        with open(makefile) as f:
            for line in f:
                for var, key in [("CFLAGS", "main"), ("LIBKMC_CFLAGS", "libkmc"),
                                 ("LIBNUSYS_CFLAGS", "libnusys"), ("LIBULTRA_CFLAGS", "libultra")]:
                    if re.match(rf'^\s*{var}\s*[:+]?=', line):
                        raw[key].update(m.group(1) for m in _MAKEFILE_DEFINE_RE.finditer(line))
    except OSError:
        pass
    for k in ("libkmc", "libnusys", "libultra"):
        raw[k] |= raw["main"]
    return {k: frozenset(v) for k, v in raw.items()}


def _active_defines_for_lib(lib):
    """Effective -D defines for an upstream library key (libultra, libkmc, libnusys)."""
    key = {"libkmc": "libkmc", "libnusys": "libnusys", "libultra": "libultra"}.get(lib or "", "main")
    return _parse_makefile_defines()[key]


def function_gating_define(up_cpath, primary):
    """If `primary`'s entire body is wrapped by a single top-level `#ifdef DEFINE`, return
    DEFINE; else return None.  Detects build-define gates like USE_EPI where the fn compiles
    to an empty stub without the define — distinct from inner feature-flag conditionals."""
    body = _upstream_body(up_cpath, primary)
    if body is None:
        return None
    lines = [l for l in body[1:-1].splitlines()
             if l.strip() and not l.lstrip().startswith('//')]
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
    if any(l.strip() for l in lines[matching_i + 1:]):
        return None
    return define


# GBI microcode macros that gate object-like macro VALUES in PR/sptask.h (OS_YIELD_DATA_SIZE,
# OS_YIELD_AUDIO_SIZE). ultralib builds libgultra_rom with a global -DF3DEX_GBI default; MG64 runs
# F3DEX2, so the project pins -DF3DEX_GBI_2 in LIBULTRA_CFLAGS (S83). With NONE defined the macro
# silently takes the #else value, mismatching the ROM by one word (sptask's
# IO_READ(...+OS_YIELD_DATA_SIZE-4): 0x900 vs the baserom's 0xc00) — invisible to the gate stub.
GBI_MICROCODE_DEFINES = ("F3D_GBI", "F3DEX_GBI", "F3DLP_GBI", "F3DEX_GBI_2")
_OBJ_DEFINE_RE = re.compile(r'^\s*#\s*define\s+([A-Za-z_]\w*)\s+(\S.*?)\s*$')
_GBI_COND_RE = re.compile(r'defined\s*\(\s*([A-Za-z_]\w*)\s*\)|\bifdef\s+([A-Za-z_]\w*)')


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
        if re.match(r'^#\s*if', s):
            guard = tuple(d for grp in _GBI_COND_RE.findall(s) for d in grp
                          if d and d in GBI_MICROCODE_DEFINES)
            if guard:
                if_vals, else_vals, cur, depth, j = {}, {}, None, 1, i + 1
                cur = if_vals
                while j < n and depth > 0:
                    t = lines[j].strip()
                    if re.match(r'^#\s*if', t):
                        depth += 1
                    elif re.match(r'^#\s*endif', t):
                        depth -= 1
                        if depth == 0:
                            break
                    elif depth == 1 and re.match(r'^#\s*else', t):
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
    an inactive define so the 1-word SHA-miss (S83 sptask) is paid at the gate, not in execution."""
    guarded = _gbi_guarded_macros(lib)
    if not guarded:
        return None
    try:
        body = _strip_comments(UpstreamSource.get(cpath).text)
    except OSError:
        return None
    active = _active_defines_for_lib(lib)
    used = set(re.findall(r'[A-Za-z_]\w*', body))
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


def _c_jal_count(body):
    """C-side jal count for `body`: `name(` occurrences minus control keywords AND function-like
    macros. A macro *invocation* emits no jal of its own — OS_USEC_TO_CYCLES expands to arithmetic,
    ERRCK to assignment+branch (the S43 gbpak `6vs5`/`6vs4` FPs), va_start to a builtin (the S43
    sprintf `2vs1` FP), MQ_IS_FULL to a field compare (the S42 osSendMesg `7vs6` FP). Generalises
    the S42 hardcoded `_C_NONCALL` predicate-macro list to the project-wide `all_func_macros()`
    table (S41), so every invoked macro is dropped, not just the two that were named. One level only
    (a macro whose body wraps exactly one real call would under-count by 1 — rare, and the gate
    reconciles the upstream call list against the asm regardless); the bias is conservative because
    the recurring failure mode is *over*-counting predicate/arithmetic macros as phantom calls."""
    macros = all_func_macros()
    return sum(
        1
        for name in C_CALL_RE.findall(body)
        if name not in _C_NONCALL and name not in macros
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
    # mirror (S80 pfsgetstatus: the non-J `__osPfsRequestOneChannel(channel)` inflated 6→7, a false
    # `7vs6`). refs_unplaced strips the same way (S47); without the lib's build_ord this is a no-op.
    body = _strip_inactive_version_branches(body, _build_version_ord(lib))
    n_c = _c_jal_count(_strip_define_lines(_strip_string_literals(_strip_dead_blocks(body))))
    if n_c == n_asm:
        return None
    return Hazard(HAZARD_JAL_COUNT_MISMATCH, f"{n_c}vs{n_asm}")


def _is_static_func_proto(line):
    """A `static ...;` line that declares a FUNCTION (prototype: `static T f(...);`), not a
    variable. Such a static function shares the mirror's TU and is NOT a BSS-layout hazard, so it
    must not flag file-static (S54: sprintf's `static void* proutSprintf(...);`). A variable is kept
    flagged, including a function-*pointer* var (`static T (*fp)(...);`, the `(*`) and an attributed
    array (`static T a[N] __attribute__((aligned(8)));` — its `aligned(8)` parens must NOT read as a
    call declarator, so strip `__attribute__((...))` first; S54 caught gfxThread's nuGfxMesgBuf)."""
    s = re.sub(r"__attribute__\s*\(\(.*?\)\)", "", line)  # drop attribute groups (their parens)
    i = s.find("(")
    if i == -1:
        return False  # no declarator parens → a plain variable
    if s[i + 1:i + 2] == "*":
        return False  # `(*name)(...)` → function-pointer variable
    if "[" in s[:i]:
        return False  # `name[dim] ...(` → array variable (a stray paren past the subscript)
    return True  # `name(...)` → function prototype/definition


def has_file_scope_static(cpath):
    """True if the upstream .c declares a file-scope static *variable* (BSS-layout hazard →
    classical loop). A file-scope static *function* (prototype) is excluded — it is not a data
    hazard (S54)."""
    for line in UpstreamSource.get(cpath).text.splitlines():
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
    like needs-header: `__osThreadTail` et al. in thread.c (osDequeueThread) is the motivating case."""
    names = []
    depth = 0
    in_kr_params = False  # between a K&R `name(args)` header and its `{` body
    for raw in UpstreamSource.get(cpath).text.splitlines():
        line = raw.strip()
        if (depth == 0 and not in_kr_params and line and not line.startswith(
                ("#", "//", "*", "}", "static", "extern", "typedef"))):
            m = DATA_GLOBAL_DEF_RE.match(line)
            if m and "(" not in line.split("=", 1)[0]:
                # Surface the array dimension (`name[DIM]`) so the gate sizes the symbol_addrs
                # entry mechanically — a scalar is 0x4, an array is stride×count (S20 rule;
                # S42 __osEventStateTab[OS_NUM_EVENTS]). The element/stride + macro resolution
                # still happens at the gate; this just flags array-vs-scalar without opening
                # the source.
                names.append(f"{m.group(1)}[{m.group(2)}]" if m.group(2) is not None else m.group(1))
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
    static function protos / function-pointer inits (reuses _is_static_func_proto). Motivating case
    (S73): gu/position.c's `static float dtor = 3.1415926/180.0;` inside guPositionF needed a
    0x10-byte .data carve that the file-scope detector — and the S72 coddog re-scan — both missed."""
    names = []
    depth = 0
    for raw in UpstreamSource.get(cpath).text.splitlines():
        line = raw.strip()
        if depth >= 1 and line.startswith("static") and "=" in line \
                and not _is_static_func_proto(line):
            m = LOCAL_STATIC_DATA_RE.match(line)
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

_SRC_CALLER_CACHE = None


def src_func_callers():
    """S77: map every un-named `func_<vram>` token to the banked C files that reference it by name
    (outside an INCLUDE_ASM stub line). Built once per process by walking `src/`.

    Motivates the `caller-evict` gate flag: when the gate ADDS a curated symbol for an un-named
    `func_<vram>` member (so splat renames it), any already-banked C file that hard-codes the old
    `func_<vram>` name fails to link (undefined reference) until its call site is renamed too. This
    is the stale-label-sync hazard reaching the gate via a symbol ADD rather than `make sync-names`.
    S77 hit it live: adding `__osSpGetStatus`=0x800B16A0 evicted the banked caller
    `src/main/func_800AB600.c`. Surfacing the caller here prices the one-line rename fixup at the
    gate instead of as a build-check link error."""
    global _SRC_CALLER_CACHE
    if _SRC_CALLER_CACHE is not None:
        return _SRC_CALLER_CACHE
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
    _SRC_CALLER_CACHE = {k: sorted(v) for k, v in callers.items()}
    return _SRC_CALLER_CACHE


# An SDK-internal global is `__`-prefixed (e.g. __osRunQueue). Uppercase macros ([A-Z]-led) and
# CamelCase types never match, so this rarely false-flags a non-symbol token.
SDK_GLOBAL_RE = re.compile(r"__[A-Za-z]\w+")

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
# data extern — without it, SDK_GLOBAL_RE flags the type as a phantom recover-extern (S48:
# __OSViContext, a 0x30-byte struct in viint.h, surfaced in the refs-unplaced list).
TYPEDEF_CLOSE_RE = re.compile(r"}\s*([A-Za-z_]\w*)\s*(?:\[[^\]]*\]|__attribute__\s*\([^;]*\))?\s*;", re.M)
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
    refs_unplaced phantom-flags a struct TYPE as an unplaced data extern (S80: __OSContRequesFormatShort,
    a typedef in PRinternal/controller.h, was mis-flagged refs-unplaced on pfsgetstatus.c)."""
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
    libmus's, …) that SDK_GLOBAL_RE alone misses — the S16 false-clean motivated it. Bounded depth
    keeps a header cycle finite."""
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
# macro-hidden recover-extern (S41: EPI_SYNC in piint.h → __osCurrentHandle, invisible to the
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
                    plist = [p.strip() for p in params.split(",") if p.strip() not in ("", "...")]
                    macros.setdefault(name, (plist, body))
    return macros


def macro_hidden_text(cpath):
    """One-level macro expansion of `cpath`: the replacement bodies of the function-like macros the
    .c *invokes*, plus the set of all function-like macro names in scope. Lets refs_unplaced /
    calls_unplaced see a data extern or callee that a verbatim mirror inlines through a macro
    (EPI_SYNC → __osCurrentHandle, S41) — invisible to the body grep and the gate build-check.
    One level only: the directly-invoked macros' bodies; macros THEY invoke are not recursed (the
    gate confirms against the asm). Each macro's own parameter names are stripped from its body so
    a placeholder like va_start's `__AP`/`__LASTARG` is not mistaken for a referenced global.
    Returns (expansion_text, macro_names)."""
    try:
        text = UpstreamSource.get(cpath).text
    except OSError:
        return "", set()
    macros = all_func_macros()
    body = _strip_comments(text)
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
    symbol_addrs recovery. Motivating case (S86 timerintr.c): `__osBaseTimer` is named ONLY in the
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
    non-`__` library globals like libnusys's nuGfxTaskSpool, the S16 false-clean), referenced in
    the file, never called anywhere (so functions — which splat auto-resolves via
    undefined_funcs_auto — are excluded), and absent from the name set. A function *pointer*
    passed by bare name could over-flag; the gate confirms against the upstream."""
    try:
        text = UpstreamSource.get(cpath).text
    except OSError:
        return []
    # Drop the inactive `#if BUILD_VERSION` side so a ref in the dead branch (S47: `CartRomHandle`
    # in the non-J `#else`) is not phantom-flagged.
    text = _strip_inactive_version_branches(text, _build_version_ord(lib))
    # Append one-level macro expansion so a global referenced only inside an invoked library macro
    # (EPI_SYNC → __osCurrentHandle, S41) is visible to the same `__`-prefix / declared-extern grep.
    macro_text, _ = macro_hidden_text(cpath)
    scan = text + "\n" + macro_text
    # `__`-prefixed SDK globals (libultra/libkmc) + any data extern the .c's headers *declare*
    # (libnusys/libmus non-`__` globals SDK_GLOBAL_RE misses — the S16 nuGfxTaskSpool false-clean).
    tokens = (
        set(SDK_GLOBAL_RE.findall(scan))
        | declared_extern_data(cpath)
        # Known libultra boot-region globals (non-`__`-prefixed, asm-baked as D_800003xx) the
        # `__`/declared-extern grep would otherwise miss — see BOOT_GLOBALS.
        | {g for g in BOOT_GLOBALS if re.search(r"\b" + re.escape(g) + r"\b", scan)}
    )
    # Type names the headers typedef (e.g. __OSViContext) are referenced in declarations, not as
    # data — drop them so a `__`-prefixed TYPE is not flagged as a phantom recover-extern (S48).
    types = declared_type_names(cpath)
    unplaced = []
    for tok in tokens:
        if tok in placed or tok.startswith("__builtin_"):  # GCC intrinsic, never a linked global
            continue
        if tok in types:  # a typedef'd type, not a data extern
            continue
        # Only flag names the .c (or its invoked macros) actually references.
        if not re.search(r"\b" + re.escape(tok) + r"\b", scan):
            continue
        # Called anywhere (`tok(` allowing whitespace) → a function, not a data extern → skip.
        if re.search(re.escape(tok) + r"\s*\(", scan):
            continue
        unplaced.append(tok)
    # S86: drop a token THIS .c defines as a file-scope data global but no function body references
    # by name — it lives only in another global's depth-0 initializer (`__osTimerList =
    # &__osBaseTimer`), which drop-def removes, so it is not an unplaced extern owed a recovery.
    defined_here = {n.split("[", 1)[0] for n in defines_data_globals(cpath)}
    if defined_here:
        body_names = _names_in_function_bodies(text)
        unplaced = [t for t in unplaced if not (t in defined_here and t not in body_names)]
    return sorted(unplaced)


def calls_unplaced(cpath, primary, placed, lib=None):
    """Functions the upstream .c *calls* by name that are absent from BOTH name files — the
    function dual of refs_unplaced. A verbatim mirror link-FAILS on these once the C body calls
    them by name: splat's undefined_funcs_auto only resolves a callee whose vram carries a name,
    so a callee labelled `func_<addr>` in the asm (unnamed in both files) has no `<name>` symbol
    for the C reference to bind. The gate build-check can't catch this — the INCLUDE_ASM scaffold
    resolves the jal directly, so the symbol is only needed once the C body calls by name (S23:
    osPiGetCmdQueue@0x800B06F0, missed by refs_unplaced precisely because it *excludes* anything
    called). A non-empty result is the `calls-unplaced` DoR hazard → the gate recovers each
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
    # Append one-level macro expansion so a callee invoked only inside a library macro is seen;
    # exclude the macro names themselves (a macro body that invokes UPDATE_REG/WAIT_ON_IOBUSY etc.
    # must not flag those as unplaced functions — they inline, they don't link).
    macro_text, macro_names = macro_hidden_text(cpath)
    # Drop the inactive `#if BUILD_VERSION` side first (S47: `osPiRawReadIo` is called only in the
    # non-J `#else` branch) so its calls are not phantom-flagged.
    text = _strip_inactive_version_branches(text, _build_version_ord(lib))
    text = _strip_define_lines(text)  # an in-body `#define NAME (expr)` is a macro def, not a call (S61)
    body = _strip_string_literals(_strip_dead_blocks(_strip_comments(text + "\n" + macro_text)))
    called = {
        n
        for n in C_CALL_RE.findall(body)
        if n not in _C_NONCALL and n not in macro_names and not n.startswith("__builtin_")
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
    what keeps that band from false-flagging needs-header → blk (S53: alHeapDBAlloc). `lib=None`
    uses the base set only.

    A quote-include (`#include "x.h"`) resolves source-relative to the dir the mirror compiles
    in, so a band-local companion already shipped at <mirror_dir>/x.h (e.g. gu/guint.h, copied
    alongside the first gu/ sibling in S49) is NOT missing even though it is absent from the -I
    set — the band-open fast path (S50). `mirror_dir` is the in-tree dir the source will land in
    (band_mirror_dir); None disables the source-relative check."""
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
        # normpath collapses a `..` segment (S60: a relative `../gu/guint.h` from a sibling-dir
        # upstream source resolves to the already-vendored gu/ copy, not a phantom needs-header).
        is_quote = '"' in m.group(0)
        if is_quote and mirror_dir and os.path.exists(os.path.normpath(os.path.join(mirror_dir, inc))):
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
    correctly NOT-vendorable.) S46: the basename collision was masking the whole PRinternal/ PI
    band as false-`blk`."""
    if inc in _project_inc_index():
        return False  # present in-tree but unreachable → unindexed -I, a deferred enabler not a cp
    for root in UPSTREAM_INC_ROOTS:
        if os.path.exists(os.path.join(root, inc)):
            return True  # public companion header → copy into an -I dir
    return os.path.basename(inc) in _upstream_src_headers()  # source-private → source-relative cp


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
    the in-tree layout and the include line must change (SHA-neutral — declarations only). S74
    surfaces the adapt target so the gate prices the include edit (`PRinternal/controller.h` →
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


def include_is_already_vendored(inc, lib):
    """True if a *missing* include needs NO header CP work: its full path is unreachable (so
    `missing_includes` flagged it), but its BASENAME already resolves under the current `-I` set, so
    the established include-path adaptation (drop the upstream prefix) resolves it. S59:
    `osGbpakCheckConnector`'s `PRinternal/controller.h` was tagged `(vendorable)` but the
    sibling-vendored `include/libultra/internal/controller.h` was already on the io band's `-I` set.
    A strict refinement of vendorable: same non-blocking class, no cp, BUT it does cost a one-line
    include adaptation (see `already_vendored_intree_path`, surfaced in the tag since S74). lib
    selects the band's extra `-I` dirs (LIB_EXTRA)."""
    return already_vendored_intree_path(inc, lib) is not None


def _tagged_missing_includes(cpath, lib):
    """(tagged_headers, blocked) for cpath's unreachable includes, tagged by enabler load: bare =
    a real block (deferred -I / system header → pts 'blk'), `(vendorable)` = a one-time source cp,
    `(already-vendored)` = free (the basename already resolves under the -I set). `blocked` counts
    only the bare ones. Shared by the named-upstream battery (append_upstream_hazards) and the S72
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
            # location (S74: `PRinternal/controller.h` → `internal/controller.h`). Surface the
            # target so the gate prices the edit instead of hitting it at first build.
            return f"{inc}(already-vendored,adapt->{adapt})"
        return f"{inc}(vendorable)"

    return [_tag(inc) for inc in missing], any(include_is_blocked(inc) for inc in missing)


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
    ("drop", frozenset({HAZARD_FILE_STATIC, HAZARD_DEFINES_DATA})),  # → classical fallback
    ("needs_copy", frozenset({HAZARD_NEEDS_HEADER})),                # companion-header copy
    ("needs_define", frozenset({HAZARD_NEEDS_DEFINE})),              # Makefile build-define enabler
    ("recover", frozenset({HAZARD_REFS_UNPLACED, HAZARD_CALLS_UNPLACED})),  # asm-data-recovery
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
    drop, needs_copy, recover = present["drop"], present["needs_copy"], present["recover"]
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
    base = 1 if band == "warm" else 2  # warm = enabler-free; cold = symbol recovery / cold deps
    base += sum(present.values())  # one bump per present enabler group (drop/copy/define/recover)
    if pack or big:
        base += 1
    return snap_fib(base)


def carry_over_names():
    """Function names genuinely PARKED in BACKLOG.md ## Carry-overs (de-ranked from the
    default ranker; build_rows skips a row whose primary is in here unless --include-stuck).

    A parked carry-over is correctly absent from `pick_target` output BY DESIGN — retrieve it
    with `--include-stuck` or by reading the BACKLOG (S90: `osCreatePiManager`/pimgr was found
    via the carry-over, not the ranker, and that is the intended path; it was not a filter bug).

    Two scope guards narrow the S45/S75 over-scoop, where every backticked prose token (a macro
    `STACK`, a name-dropped banked callee, a tool name) was carried — so a *still-asm* primary
    could be silently dropped just by being prose-mentioned:
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
        # would start the region mid-archive and truncate the genuine carry-overs (S90 bug).
        parts = re.split(r"(?m)^## Carry-overs", f.read(), maxsplit=1)
    if len(parts) <= 1:
        return names
    region = parts[1]
    archive = re.search(r"^- \*\*Sprint \d+\b", region, re.M)  # guard (1): bound the live region
    if archive:
        region = region[: archive.start()]
    placed = placed_symbols()  # guard (2): real symbols only (names file ∪ func_<vram>)
    for tok in re.findall(r"`([A-Za-z_]\w+)`", region):
        if tok in placed or _FUNC_TOKEN_RE.fullmatch(tok):
            names.add(tok)
    return names


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
        # Skip a pure-nop pad subseg (S79): a trailing-alignment pad split off as its own `[..,asm]`
        # subseg (the trailing-pad remedy below) carries a splat glabel but is 0x00000000 nops only —
        # never a decomp target. `subseg_vram` non-None (listing present) + `code_end_rom` None (no
        # non-nop instruction) is the all-nop signature; a real fn always has a non-nop body.
        if subseg_vram(off) is not None and code_end_rom(off) is None:
            return None
        hazards = []
        if size and size % 16 != 0:
            hazards.append(Hazard(HAZARD_NON16ALIGN))
        # Trailing-alignment pad (S79): splat extracts the whole subseg slot (function + the nop
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
            # own 16-align can't fill (contramwrite → 128-aligned osAfterPreNMI). A residual at a
            # merely-16-aligned boundary is the delay-slot-nop measurement artifact (code_end stops
            # at the last non-nop, undercounting a real `jr ra; nop` tail by one 16-step) — GCC
            # 16-aligns past it anyway, so excluding align<=16 removes that false fire.
            if residual >= 16 and align > 16:
                hazards.append(Hazard(HAZARD_TRAILING_PAD, f"{residual}B@{align}"))
        if len(fns) > 1:
            # List each fn + its upstream basename so the gate can tell a multi-file pack
            # (different basenames → split at the upstream-file boundary, e.g. dpsetstat+dpctr)
            # from a single-file pack, without hand-disassembling asm/<rom>.s.
            members = []
            asm_index = build_asm_tu_index()
            asm_tus = []  # distinct vendorable .s TUs among asm-ONLY members (no C mirror)
            c_stems = []  # distinct C-upstream stems among members (multi-file C-mirror pack)
            c_files = []  # parallel to c_stems: the (lib, cpath) of each distinct C stem
            c_members = 0  # members that resolved to a C upstream (for the single-file-pack test)
            for fn in fns:
                fn_lib, fn_up = upstream_index.get(fn, (None, None))
                if fn_up:
                    stem = os.path.splitext(os.path.basename(fn_up))[0]
                    members.append(f"{fn}={stem}")
                    c_members += 1
                    if stem not in c_stems:
                        c_stems.append(stem)
                        c_files.append((fn_lib, fn_up))
                    continue
                # No C upstream. Count a member's asm TU only here: a member with a C mirror
                # is a C-mirror pack-split target (the pack hazard's basenames), not an asm-vendor
                # TU — ultralib may ship a .s variant (e.g. gu translate.s) the ROM doesn't use.
                # Without this gate a C-mirror gu pack would mis-flag as asm. Name the .s in the
                # member label so a MIXED asm+C pack reads as `bzero=bzero.s` not an opaque
                # `bzero=?` (S65 #1: the [0x860C0] libc pack = bzero.s + string.c was unreadable).
                t = asm_index.get(fn)
                if t:
                    members.append(f"{fn}={os.path.basename(t)}")
                    if t not in asm_tus:
                        asm_tus.append(t)
                else:
                    members.append(f"{fn}=?")
            # A pack whose every member resolves to ONE upstream C file is NOT a split-required
            # blocker — it is an atomic verbatim mirror (guPerspectiveF+guPerspective S55,
            # guLookAtHiliteF+guLookAtHilite S64, guTranslateF+guTranslate S67), banked or spiked
            # in one shot. Tag it `single-file-pack` (informational, → #upstream-mirror-pattern) so
            # the gate stops reading it as the multi-file `pack` that needs an upstream-file split
            # (S67 #2). The pts seed is unchanged (seed_points keys the pack penalty on nfns>1, not
            # the kind), so this is display-only. A MIXED asm+C or multi-stem pack keeps `pack`.
            single_file = c_members == len(fns) and len(c_stems) == 1
            # S68 #3: a pack with unnamed (`func_<addr>`) member(s) still atomic-mirrors when its
            # one named C stem's upstream file defines exactly len(fns) functions — the unnamed
            # members are that file's other functions (the gu F-variant + s16-wrapper idiom:
            # guAlignF named + func_800A794C = guAlign, both in align.c). Without this an unnamed
            # wrapper mislabels the pack as the multi-file `pack` that needs an upstream-file split.
            # Conservative: one named stem + an exact function-count match; else falls back to pack.
            if not single_file and len(c_stems) == 1 and 1 <= c_members < len(fns):
                if _upstream_file_func_count(*c_files[0]) == len(fns):
                    single_file = True
            pack_kind = HAZARD_SINGLE_FILE_PACK if single_file else HAZARD_PACK
            hazards.append(Hazard(pack_kind, f"{len(fns)}fn[" + ",".join(members) + "]"))
            # S88 one-tu: if EVERY inner fn boundary is non-16-aligned, the pack is ONE .o (the
            # linker 16-aligns each .o's .text start, so a 2nd .o would begin on a 16 boundary).
            # Confirms a single-file-pack structurally even when members are un-named (the
            # coddog-mirror case, where the named-stem single_file test can't fire), and marks a
            # per-fn decompose split as mechanically blocked (S51 non16align). Sufficient, not
            # necessary: a one-.o pack with a 16-multiple-sized member won't fire (conservative).
            addrs = [a for _, a in asm_function_addrs(off)]
            if len(addrs) == len(fns) and all(a % 16 != 0 for a in addrs[1:]):
                hazards.append(Hazard(HAZARD_ONE_TU))
            # C analog of combined-subseg (S64 #3): ≥2 *distinct* C upstream files share one asm
            # subseg → a multi-file C-mirror pack the gate splits at the upstream-file boundary, then
            # mirrors each verbatim. The pack basenames already encode this, but a big combined subseg
            # (e.g. 0x82B80 = align.c + cosf.c + lookat.c, 3072B) ranks by its WHOLE size and buries a
            # cheap clean leaf (cosf, a zero-callee gu/cosf.c mirror) past smallest-first; surfacing
            # `c-combined:Nfile[…]` prices the split + names the leaves so the gate evaluates them
            # without hand-disassembling asm/<rom>.s. A single-file pack (guPerspectiveF+guPerspective,
            # both = perspective) has one stem → does NOT fire.
            if len(c_stems) > 1:
                hazards.append(Hazard(HAZARD_C_COMBINED,
                                      f"{len(c_stems)}file[" + "|".join(sorted(c_stems)) + "]"))
            # Combined-subseg sub-pattern: ≥2 asm-ONLY members from *distinct* vendorable ultralib
            # .s files (the asm analog of a multi-file C pack). The gate must split the subseg at
            # the TU boundary, then vendor each .s verbatim (S62 invaldcache|invalicache). Surfacing
            # it here prices the split before hand-disassembling asm/<rom>.s. A pack whose asm
            # members share one .s (a partial-TU split, e.g. __osDisableInt/__osRestoreInt in
            # setintmask.s) has a single distinct TU → does NOT fire (different, harder hazard).
            if len(asm_tus) > 1:
                bn = "|".join(sorted(os.path.basename(t) for t in asm_tus))
                detail = f"{len(asm_tus)}tu[{bn}]"
                # Price the needs-define enabler for the split the same way the intrinsic-likely
                # path does (S63 #1): a vendorable .s in the pack may reference an UPPER_CASE asm
                # macro the in-tree `-I` headers lack (setfpccsr.s → CFC1/CTC1), and the gate must
                # vendor it before the split, not discover it at a failing vendor-compile. Union
                # the misses across the pack's TUs so one (needs-define:…) prices the whole split.
                miss = sorted({m for t in asm_tus for m in vendorable_tu_missing_defines(t)})
                if miss:
                    detail = f"{detail}(needs-define:{','.join(miss)})"
                hazards.append(Hazard(HAZARD_COMBINED_SUBSEG, detail))
        # Hand-asm detection → asm-mirror candidate, not a classical target. Two cases collapse
        # here: a *pure* register/FPU shim (intrinsic_likely) and a privileged TU that does real
        # work around an op C can't emit (privileged_asm: a tlbwi/mtc0/eret with branches+loads or
        # calls — osMapTLB/osUnmapTLB, exception dispatch; S70 #1). Both vendor verbatim from the
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
                # S84 #3: a vendorable .s with a non-.text section can't be a clean .text-only cp —
                # pre-flag the strip+rename enabler (docs/hazards.md#asm-mirror-vendoring).
                data_syms = vendorable_tu_data_symbols(rel)
                if data_syms:
                    detail = f"{detail}(has-rodata:{','.join(data_syms)})"
                # S91 #2: a SYMBOLIC-pointer table in the active data section (a switch jtbl / fn-ptr
                # table) makes the .text-only asm-mirror a SPIKE, NOT the S84 strip-and-rename the
                # has-rodata flag implies — the table extracts to a separate blob with vestigial
                # .text-label refs and can't be carve-placed. Flag it so the gate doesn't mis-frame a
                # heavy asm-mirror (S91 exceptasm: __osIntTable jtbl) as a clean replay.
                jtbls = vendorable_tu_jtbl(rel)
                if jtbls:
                    detail = f"{detail}(asm-mirror-jtbl:{','.join(jtbls)})"
            elif prim_privileged:
                # An un-named func_<addr> with a privileged op: the name can't resolve the TU, but
                # the privileged op proves a vendorable ultralib source exists. Tag it cp0-asm so the
                # gate identifies + vendors it (a single MCP disasm names it by its CP0/TLB signature
                # — osMapTLB/osUnmapTLB), instead of reading the BARE intrinsic-likely as a no-source
                # shim and parking it (the 14-sprint carry-over this retires).
                detail = "cp0-asm(identify-TU)"
            else:
                detail = None  # genuine no-source shim (handwritten leaf, no privileged op, no TU)
            hazards.append(Hazard(HAZARD_INTRINSIC_LIKELY, detail))
        return "asm-flip", fns, hazards
    if typ == "c" and path:
        cpath = os.path.join(ROOT, "src", path + ".c")
        if not os.path.exists(cpath):
            return None
        text = open(cpath).read()
        fns = re.findall(r"INCLUDE_ASM\([^,]+,\s*([A-Za-z_]\w+)", text)
        if not fns:  # 0 stubs => already an md5-candidate
            return None
        return "c-stub", fns, [Hazard(HAZARD_REMAINING, str(len(fns)))]
    return None


def _append_recover_hazards(off, primary, up_path, up_lib, hazards):
    """Append refs-unplaced + calls-unplaced (the symbol-recovery battery, with inline-vram
    annotation when the binding is unambiguous) for an upstream .c, in-place. Shared by the
    named-upstream battery (append_upstream_hazards) and the S74 coddog trap re-scan, so a
    coddog-resolved mirror prices its recover-extern load identically to a named one — mirroring how
    `_tagged_missing_includes` is shared so both price headers identically. An un-named (`func_`)
    subseg blocks the named-keyed scan, so without this the recover-extern load is invisible at the
    gate and only surfaces at first build (S74 contreaddata: 3 SI callees + 3 data globals)."""
    placed = placed_symbols()
    unplaced = refs_unplaced(up_path, placed, up_lib)
    if unplaced:
        # Annotate the recovered vram inline when the binding is unambiguous (one unplaced
        # name ∩ one asm candidate) so the gate copy-pastes the symbol_addrs entry instead
        # of re-running MCP disassemble_function (S12 retro #1). Ambiguous → bare names.
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
        hazards.append(Hazard(HAZARD_REFS_UNPLACED, ",".join(refs)))  # asm-data-recovery before flip
    unplaced_calls = calls_unplaced(up_path, primary, placed, up_lib)
    if unplaced_calls:
        # Annotate the recovered vram inline when unambiguous (one unplaced call ∩ one
        # unnamed jal), mirroring refs-unplaced, so the gate copy-pastes the func entry.
        ccands = recover_unplaced_call_vram(off, primary)
        if len(unplaced_calls) == 1 and len(ccands) == 1:
            crefs = [f"{unplaced_calls[0]}@0x{ccands[0]:08X}"]
        else:
            crefs = unplaced_calls
        hazards.append(Hazard(HAZARD_CALLS_UNPLACED, ",".join(crefs)))  # recover func symbol before flip
    # Switch jump table (S76): a `switch` compiles to a `jtbl_<addr>` in the code-segment `.rodata`
    # whose `.word` entries are the fn's own internal `.L<addr>` labels. Flipping the subseg text->C
    # deletes those labels, so the still-asm rodata jtbl link-breaks (undefined-`.L<addr>` ref) unless
    # a `.rodata` sibling carve places the C-re-emitted table. The gate's text-only green-ROM check
    # cannot catch this by construction (the jtbl stays valid asm until the C body lands), so it must
    # be priced at the gate — the jump-table analog of rodata-literal (a verbatim mirror re-emits a
    # byte-identical table: same case-body absolute addresses; #rodata-sibling-yaml-pattern). Scanned
    # here in the SHARED battery so it prices both the named-upstream and coddog paths (S76
    # __osDevMgrMain is NAMED, so the S72/S73 coddog re-scan never reached it). Display-only like
    # rodata-literal: the carve is a mechanical near-free enabler, so no seed_points bump.
    jtbls = [a for a in rodata_jtbls(off) if _literal_in_rodata(a, off)]
    if jtbls:
        hazards.append(Hazard(HAZARD_RODATA_JTBL, ",".join(f"0x{a:08X}" for a in sorted(jtbls))))


def header_renames_symbol(cpath, primary, _depth=0, _seen=None):
    """A (transitively-)included vendored header that rewrites the candidate's curated symbol via a
    macro `#define <primary>...` — a K->J source-compat shim (`os_host.h`:
    `#define __osInitialize_common() osInitialize()`; S31 nuGfxInit was the 1st instance, S85 the
    2nd → a real class). The macro bites ONLY the real body's function definition, so it is invisible
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
    define_re = re.compile(r'^\s*#\s*define\s+' + re.escape(primary) + r'\b')
    includes = []
    try:
        text = UpstreamSource.get(cpath).text  # errors='ignore' (was 'replace'; immaterial for ASCII)
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
    S78 cod_members scan) prices its traps identically to one keyed on the primary (the S72 block):
    S80 found initialize.c's defines-data `.data` carve invisible because its coddog hit keys on the
    sibling `create_speed_param`, not the named leader `__osInitialize_common`, so only the S78 block
    fired and it surfaced the bare coddog flag WITHOUT the trap battery the S72 block runs."""
    clib = up_lib or "libultra"
    if has_file_scope_static(cl_path):
        hazards.append(Hazard(HAZARD_FILE_STATIC))
    cdefs = defines_data_globals(cl_path) + defines_local_static_data(cl_path)  # S73: + fn-local statics
    if cdefs:
        hazards.append(Hazard(HAZARD_DEFINES_DATA, ",".join(cdefs)))
    blocked = False
    ctagged, cblk = _tagged_missing_includes(cl_path, clib)
    if ctagged:
        hazards.append(Hazard(HAZARD_NEEDS_HEADER, ",".join(ctagged)))
        blocked = cblk
    _append_recover_hazards(off, primary, cl_path, clib, hazards)
    ren = header_renames_symbol(cl_path, primary)
    if ren:
        hazards.append(Hazard(HAZARD_HEADER_RENAMES_SYMBOL, f"{primary}@{ren}"))
    return blocked


def append_upstream_hazards(off, primary, up_lib, up_path, hazards):
    """Append the upstream-mirror hazards for a named candidate (in-place, in the
    order the gate reads them) and return (band, blocked)."""
    blocked = False
    if has_file_scope_static(up_path):
        hazards.append(Hazard(HAZARD_FILE_STATIC))  # route to the classical loop, not the mirror
    data_defs = defines_data_globals(up_path) + defines_local_static_data(up_path)  # S73: + fn-local statics
    if data_defs:
        hazards.append(Hazard(HAZARD_DEFINES_DATA, ",".join(data_defs)))  # .data analogue → classical loop
    _append_recover_hazards(off, primary, up_path, up_lib, hazards)
    mirror_dir = band_mirror_dir(up_lib, up_path)  # also reused below for band warmth
    tagged, hdr_blocked = _tagged_missing_includes(up_path, up_lib)
    if tagged:
        hazards.append(Hazard(HAZARD_NEEDS_HEADER, ",".join(tagged)))
        blocked = blocked or hdr_blocked
    stale = stale_version_header(up_path, up_lib)
    if stale:
        # A version-conditional fn silently dropped: os_version.h resolves but is a stripped revision
        # missing the referenced VERSION_* constant. A one-time additive header-content vendor, not a
        # block — keep it off `blocked` (it does not gate the DoR, only warns the gate). S60.
        hazards.append(Hazard(HAZARD_STALE_HEADER, "os_version.h(" + ",".join(stale) + ")"))
    gating = function_gating_define(up_path, primary)
    if gating and gating not in _active_defines_for_lib(up_lib):
        hazards.append(Hazard(HAZARD_NEEDS_DEFINE, gating))
    gbi_def = gbi_value_guard_needs_define(up_path, up_lib)
    if gbi_def:
        hazards.append(Hazard(HAZARD_NEEDS_DEFINE, gbi_def))  # GBI-value-guarded macro (S83 OS_YIELD_DATA_SIZE)
    divergence = call_divergence(off, primary, up_path, up_lib)
    if divergence:
        hazards.append(divergence)  # near-verbatim mirror: reconcile call list at the gate
    ren = header_renames_symbol(up_path, primary)
    if ren:
        hazards.append(Hazard(HAZARD_HEADER_RENAMES_SYMBOL, f"{primary}@{ren}"))  # needs #undef (S85)
    # Anonymous `%lo(D_<addr>)` constant loads split into two enablers by segment (S52):
    #   - code-segment `.rodata` → compiler-pooled literal the mirror re-emits → a `.rodata` sibling
    #     split at finalize (docs/hazards.md#rodata-sibling-yaml-pattern, S38/S48).
    #   - data segment → a function-local `static` the mirror re-emits → recover-extern + drop the
    #     static to a file-scope `extern` (docs/hazards.md#defines-data, S49 fast path).
    # The FP-only scan (ldc1/lwc1) seeds both; integer `lw` refs that land in the rodata band add the
    # companion words of a pooled `double` (GCC's `lw` pair + `mtc1`) so the sibling-split extent is
    # sized in full, not just its first word. `lw` refs in the data segment are ordinary data refs
    # (refs-unplaced/defines-data already cover them) and are dropped here. Both scans span the whole
    # subseg (every pack function), since the `.rodata` sibling places the whole object's `.rodata`
    # (S55: guPerspective's pooled doubles, missed by the old per-primary scan).
    rodata_lits, data_statics = [], []
    for a in rodata_literals(off):
        (rodata_lits if _literal_in_rodata(a, off) else data_statics).append(a)
    for a in rodata_word_refs(off):
        if _literal_in_rodata(a, off) and a not in rodata_lits:
            rodata_lits.append(a)
    if rodata_lits:
        # Pre-note the full vram extent as a DoR enabler so it is not a finalize-time SHA-miss (S48).
        detail = ",".join(f"0x{a:08X}" for a in sorted(rodata_lits))
        # Append the carve-end boundary (S64 #2): the sibling-split runs to the next `.rodata`
        # subseg boundary, which can exceed the last `%lo`-referenced literal (a multi-`du` dlabel
        # block's trailing word has no ref of its own).
        end = _rodata_carve_end_vram(off, max(rodata_lits))
        if end and end > max(rodata_lits):
            detail += f";carve-end=0x{end:08X}"
        hazards.append(Hazard(HAZARD_RODATA_LITERAL, detail))
    if data_statics:
        # A function-local static the mirror must drop to a file-scope extern + recover (S49).
        hazards.append(Hazard(HAZARD_DATA_STATIC, ",".join(f"0x{a:08X}" for a in sorted(data_statics))))
    # twin-of hint (S68 #2): when the candidate re-emits a function-local static (data-static /
    # rodata-literal) AND its mirror dir already holds a banked sibling that carved the same
    # ld-section, name that sibling so the gate reaches for the established carve playbook instead
    # of re-deriving it (align.c was the verbatim twin of S61 rotate.c — same `libultra/gu` dir,
    # same `.data` dtor carve + substituted-callee edit). Prefer a same-section twin; else any.
    if data_statics or rodata_lits:
        tree = dict(UPSTREAM_TREES).get(up_lib, LIBULTRA)
        rel = os.path.relpath(up_path, tree)
        cand_dir = os.path.join(up_lib, os.path.dirname(rel))
        cand_stem = os.path.splitext(os.path.basename(rel))[0]
        sibs = _static_carve_siblings().get(cand_dir)
        if sibs:
            want = ".data" if data_statics else ".rodata"
            pool = sorted(sibs[want] - {cand_stem}) \
                or sorted((sibs[".data"] | sibs[".rodata"] | sibs[".bss"]) - {cand_stem})
            if pool:
                hazards.append(Hazard(HAZARD_TWIN_OF, pool[0]))
    band = "warm" if band_is_warm(mirror_dir) else "cold"
    return band, blocked


def _file_scope_static_count(cpath):
    """Number of file-scope static *variable* declarations (the uninitialized .bss family
    FILE_STATIC_RE matches; static function protos excluded). One matching line == one .bss symbol,
    counted without fragile declarator extraction (a `STACK(name, size)` macro hides the name). Used
    for the drop-static-mirror count; the func-local uninitialized static (S87 vimgr's `retrace`) is a
    known under-count — it has no file-scope line and is recovered at the gate alongside the rest."""
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
    """S87: re-frame a coddog-confirmed verbatim mirror's .bss-family hazard cluster as ONE
    drop-to-extern enabler, or None. When a >=CODDOG_MIRROR_PCT non-audio `coddog-mirror` identity is
    on the row AND a `file-static` is present AND NO carve signal is (no rodata-literal / data-static /
    rodata-jtbl), the file-static + defines-data + refs-unplaced flags are NOT a carve/classical spike:
    every uninitialized file-scope static/global is dropped to a sized `extern` placed at its main_bss
    vram (pure .bss = no ROM bytes = no carve, the S81 siacs pattern). Returns a `drop-static-mirror:
    <n>bss` Hazard so the gate prices a seed-only N-symbol mirror instead of the scary 4-flag cluster.
    The co-listed file-static/defines-data/refs-unplaced stay (seed_points + the gate's per-symbol
    recovery read them); this tag is the leading verdict that they are one enabler. (S87 vimgr.c: the
    carry-over's '.bss carve' framing was the false-flag this retires — banked seed-only, first build.)

    Advisory + graceful: a candidate with a NONZERO-initialized global (real .data, not modelled as a
    distinct flag) that slips the condition degrades to the documented carve playbook when the gate
    SHA misses — never a silent wrong bank."""
    kinds = {h.kind for h in hazards}
    if HAZARD_FILE_STATIC not in kinds:
        return None
    if kinds & {HAZARD_RODATA_LITERAL, HAZARD_DATA_STATIC, HAZARD_RODATA_JTBL}:
        return None  # a carve signal disqualifies the pure-.bss drop
    definitive = any(
        h.kind == HAZARD_CODDOG_MIRROR and not h.detail.split("@", 1)[0].startswith("src/audio")
        and _coddog_pct(h.detail) >= CODDOG_MIRROR_PCT
        for h in hazards
    )
    if not definitive:
        return None
    n = (_file_scope_static_count(mirror_path) + len(defines_data_globals(mirror_path))
         if mirror_path and os.path.isfile(mirror_path) else 0)
    return Hazard(HAZARD_DROP_STATIC_MIRROR, f"{n}bss" if n else "bss")


def _coddog_pct(detail):
    """Parse the `<file>@<pct>` coddog hazard detail's percentage, or 0.0 if malformed."""
    try:
        return float(detail.split("@", 1)[1])
    except (IndexError, ValueError):
        return 0.0


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
        "func": cand.primary, "rom": f"0x{cand.off:X}",
        "vram": f"0x{vram:08X}" if vram is not None else "?",
        "size": cand.size, "nfns": len(cand.fns),
        "pts": seed_points(cand.size, cand.up_lib or "none", cand.band, len(cand.fns), hazards, cand.blocked),
        "kind": cand.kind, "upstream": cand.up_lib or "none", "band": cand.band,
        "hazards": hz, "score": score,
    }


def build_rows(args, upstream_index, carried, sig_index, coddog_index=None):
    coddog_index = coddog_index or {}
    subs = parse_subsegs()
    rows = []
    for i, (off, typ, path) in enumerate(subs):
        size = subs[i + 1][0] - off if i + 1 < len(subs) else 0
        classified = classify_subseg(off, typ, path, size, upstream_index)
        if classified is None:
            continue
        kind, fns, hazards = classified

        # S77: an un-named `func_<vram>` member a banked C file calls by name will be EVICTED when
        # the gate adds its curated symbol (splat renames it → the caller's hard-coded name fails to
        # link). Flag the caller so the one-line rename fixup is priced at the gate, not discovered
        # as a build-check link error. Display-only (seed_points ignores this kind).
        _callers = src_func_callers()
        _evicts = [f"{fn}@{','.join(_callers[fn])}" for fn in fns
                   if fn.startswith("func_") and fn in _callers]
        if _evicts:
            hazards.append(Hazard(HAZARD_CALLER_EVICT, ";".join(_evicts)))

        primary = fns[0]
        up_lib, up_path = upstream_index.get(primary, (None, None))
        band = "-"
        blocked = False
        mirror_path = up_path  # the .c the drop-static-mirror count reads; coddog identity overrides below
        if up_path:
            band, blocked = append_upstream_hazards(off, primary, up_lib, up_path, hazards)
        elif kind == "asm-flip":
            if build_asm_tu_index().get(primary):
                # A hand-asm libultra mirror (no `.c` def, so absent from upstream_index): bcopy,
                # __osSetFpcCsr, the cache/TLB leaves. Label it libultra for the column/filter/pts
                # so `--lib libultra` surfaces it. The `.s` TU path already rides the
                # intrinsic-likely hazard from classify_subseg; do NOT set up_path (it feeds the
                # C-only append_upstream_hazards) — the asm-mirror-vendoring route, not a C mirror.
                up_lib = "libultra"
            elif primary.startswith("func_"):
                # No name-index hit + un-named: it may still be an un-named SDK mirror (the S13
                # trap). Run the signature matcher; an advisory hit flags the gate to verify.
                # S75: but skip it when coddog already holds a definitive (>=CODDOG_MIRROR_PCT,
                # non-audio) identity for this fn — the maybe-upstream IDF guess is then redundant
                # noise that can point at the WRONG file (S75 func_800A7190 carried both
                # coddog-mirror:src/io/contquery.c@99.99 AND a mis-pointed
                # maybe-upstream:libultra:voicesetadconverter,...). Let the coddog hit (appended
                # below) stand as the sole upstream signal.
                cod = coddog_index.get(primary)
                cod_definitive = bool(cod) and cod[1] >= CODDOG_MIRROR_PCT \
                    and not cod[0].startswith("src/audio")
                if not cod_definitive:
                    hint = signature_hint(off, primary, sig_index)
                    if hint:
                        hazards.append(hint)

        # S71 coddog cross-ref: a candidate coddog matched to an ultralib fn but that the C-name
        # index missed (un-named / `none`) is a verbatim-mirror target mis-seen as classical. Flag
        # the match always; re-price a ≥CODDOG_MIRROR_PCT *non-audio* hit as a libultra mirror so
        # `seed_points` drops it off the `classical and pack` -> 13 path (crc.c: pts-13 -> 3). Audio
        # hits stay advisory (they still need the one-time audio-header enabler, not modeled here).
        if not up_path and primary in coddog_index:
            cfile, cpct = coddog_index[primary]
            hazards.append(Hazard(HAZARD_CODDOG_MIRROR, f"{cfile}@{cpct:.2f}"))
            _append_coddog_twin_hazard(cfile, fns, upstream_index, hazards)
            if up_lib is None and cpct >= CODDOG_MIRROR_PCT and not cfile.startswith("src/audio"):
                up_lib = "libultra"
            # S72: the coddog-resolved upstream is a real `.c`, but its trap detectors never ran.
            # An un-named (`func_<addr>`) subseg's defines-data / file-static / needs-header key off
            # the *named* upstream (absent here), so a verbatim-mirror candidate that DEFINES data or
            # a file-scope static looked clean under the bare coddog flag. Re-run the three file-level
            # (name-independent) detectors on the resolved `.c` so the gate prices the trap — and
            # seed_points re-prices it (drop/needs_copy) — instead of a mid-sprint BSS-layout stall.
            # Motivating case: `func_800AC110 → piacs.c` (pts-3 "clean") DEFINES __osPiAccessQueueEnabled
            # + `static OSMesg piAccessBuf[]` → defines-data + file-static, a route-to-classical trap.
            # S72/S74: re-run the file-level trap battery (file-static, defines-data, needs-header)
            # AND union refs/calls-unplaced over the coddog-resolved upstream — the un-named `func_`
            # blocked the named-keyed scan in append_upstream_hazards, so a coddog mirror's traps +
            # recover-extern load were invisible at the gate (S74 contreaddata: 3 SI callees + 3 data
            # globals). Shared with the S78 tail-identity block via _append_coddog_trap_hazards.
            cl_path = _coddog_upstream_path(cfile)
            if cl_path:
                mirror_path = cl_path  # the confirmed coddog identity is the file we mirror + count
                blocked = _append_coddog_trap_hazards(off, primary, cl_path, up_lib, hazards) or blocked
                # S88: a coddog hit on a multi-fn pack can be a STRUCTURAL fingerprint match, not a
                # source attribution — settime.c (1 fn) coddog-matched the 6fn os pack [0x526B0]
                # (settime's `__osCurrentTime = time` is structurally a sub-shape of a timer fn).
                # If the matched file defines FEWER fns than the pack holds, it cannot be the sole
                # source → the pack is multi-file; flag so the gate doesn't read it as a
                # single-file-pack mirror. Under-count direction ONLY: a true single source may
                # define MORE (version/_DEBUG-gated extras — contpfs.c 9 raw vs 7 ROM fns), which is
                # fine, so `matched > nfns` is never flagged (would false-fire on contpfs).
                if len(fns) > 1:
                    matched_n = _upstream_file_func_count(up_lib or "libultra", cl_path)
                    if 0 < matched_n < len(fns):
                        hazards.append(Hazard(HAZARD_CODDOG_FNCOUNT_MISMATCH,
                                              f"{matched_n}vs{len(fns)}"))

        # S78: a multi-fn asm subseg's mirror IDENTITY often lives in its UN-NAMED tail, not its
        # named (or mis-attributed) leader. The S71 block above keys coddog on `primary` AND fires
        # only when `not up_path`, so a named-leader subseg whose tail `func_<addr>` members coddog-
        # match an ultralib `.c` was invisible (S76 devmgr; S78 gbpaksetbank+pfsisplug @0x8CE90: leader
        # __osGbpakSetBank is named AND mis-attributed to gbpakreadwrite.c via a forward prototype, so
        # its tail func_800B1B50→pfsisplug.c carried the real identity). Scan ALL members; surface each
        # distinct coddog `.c` identity the primary block did not already flag, and label the subseg
        # libultra so seed_points + the column price it as the mirror it is.
        cod_members = [(fn, coddog_index[fn][0], coddog_index[fn][1]) for fn in fns
                       if fn in coddog_index and coddog_index[fn][1] >= CODDOG_MIRROR_PCT
                       and not coddog_index[fn][0].startswith("src/audio")]
        if cod_members:
            already = {h.detail.split("@", 1)[0] for h in hazards if h.kind == HAZARD_CODDOG_MIRROR}
            for cfile in sorted({c for _, c, _ in cod_members}):
                if cfile in already:
                    continue
                cpct = max(p for _, c, p in cod_members if c == cfile)
                hazards.append(Hazard(HAZARD_CODDOG_MIRROR, f"{cfile}@{cpct:.2f}"))
                _append_coddog_twin_hazard(cfile, fns, upstream_index, hazards)
                # S80: surface the tail-identity file's traps too. The S72 block above keys on the
                # primary, so a coddog hit carried by an un-named TAIL member (initialize.c's hit is
                # on the sibling `create_speed_param`, not leader `__osInitialize_common`) reached
                # here with only the bare coddog flag — its defines-data `.data` carve / file-static
                # went un-priced. `already` excludes the primary's cfile, so no double-scan.
                cl_path = _coddog_upstream_path(cfile)
                if cl_path:
                    mirror_path = cl_path or mirror_path  # the coddog identity is the real mirror source
                    blocked = _append_coddog_trap_hazards(off, primary, cl_path, up_lib, hazards) or blocked
            # S92: the under-count fncount-mismatch + the structural size-ratio guards run in the
            # PRIMARY coddog block (above) only when the identity is on `primary`. When the WHOLE
            # pack resolves to ONE coddog .c via a TAIL member (func_80050400's leader is not in
            # coddog_index; a tail member carries llcvt.c — 8 trivial `return d;` stubs, ~250B — yet
            # the subseg is 2032B/11fn, and coddog matched that same llcvt.c to THREE subsegs),
            # neither guard ran, so the phantom surfaced as a clean single-source mirror. Re-run both
            # here when the pack has exactly one coddog identity (dedup the fncount flag w/ primary).
            distinct = sorted({c for _, c, _ in cod_members})
            if len(fns) > 1 and len(distinct) == 1:
                cl1 = _coddog_upstream_path(distinct[0])
                if cl1:
                    matched_n = _upstream_file_func_count(up_lib or "libultra", cl1)
                    if 0 < matched_n < len(fns) \
                            and not any(h.kind == HAZARD_CODDOG_FNCOUNT_MISMATCH for h in hazards):
                        hazards.append(Hazard(HAZARD_CODDOG_FNCOUNT_MISMATCH,
                                              f"{matched_n}vs{len(fns)}"))
                    loc = _meaningful_loc(cl1)
                    if loc and size > CODDOG_STRUCTURAL_BYTES_PER_LOC * loc:
                        cpct = max(p for _, c, p in cod_members if c == distinct[0])
                        hazards.append(Hazard(HAZARD_CODDOG_STRUCTURAL, f"{distinct[0]}@{cpct:.2f}"))
            if up_lib is None:
                up_lib = "libultra"

        if args.lib and args.lib not in (path or "") and (up_lib or "") != args.lib \
                and not any(args.lib in n for n in fns):
            continue
        # The `carried` filter de-ranks BACKLOG carry-overs (intentional — a parked carry-over is
        # retrieved via --include-stuck / the BACKLOG, NOT smallest-first; S90 pimgr confirmed this is
        # by design, not a bug). carry_over_names() is now region+symbol scoped (S90) so a prose
        # name-drop of a still-asm function no longer silently drops it; the cod_members override is
        # the older backstop for a definitively-coddog'd subseg whose leader was prose-mentioned
        # (S45 `__osGbpakSetBank`-as-callee, S78 coddog-identity-wins).
        if primary in carried and not args.include_stuck and not cod_members:
            continue

        # S87: re-frame a coddog-confirmed pure-.bss file-static cluster as one drop-to-extern enabler
        # (not a carve/classical spike) — appended last so it reads as the leading verdict over the
        # file-static/defines-data/refs-unplaced flags it summarizes.
        dsm = drop_static_mirror_hazard(mirror_path, hazards)
        if dsm:
            hazards.append(dsm)

        cand = Candidate(off, primary, kind, fns, size, up_lib, band, blocked)
        rows.append(score_row(cand, hazards, carried))
    rows.sort(key=lambda r: (-r["score"], r["size"]))
    return rows[: args.n]


def main():
    ap = argparse.ArgumentParser(description="Rank the next decomp target, smallest-first.")
    ap.add_argument("--lib", help="substring filter on subseg path or function name")
    ap.add_argument("-n", type=int, default=20, help="max rows (default 20)")
    ap.add_argument("--include-stuck", action="store_true", help="include BACKLOG carry-overs")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    rows = build_rows(args, build_upstream_index(), carry_over_names(),
                      build_signature_index(), build_coddog_index())
    if args.json:
        print(json.dumps(rows, indent=1))
        return
    if not rows:
        print("(no candidates — every asm subseg flipped and every c file at 0 stubs?)")
        return
    print(f"{'func':28} {'rom':>9} {'vram':>10} {'size':>5} {'nfn':>3} {'pts':>3} {'kind':8} "
          f"{'upstream':9} {'band':4} hazards")
    for r in rows:
        print(f"{r['func']:28} {r['rom']:>9} {r['vram']:>10} {r['size']:>5} {r['nfns']:>3} "
              f"{str(r['pts']):>3} {r['kind']:8} {r['upstream']:9} {r['band']:4} {r['hazards']}")


if __name__ == "__main__":
    main()
