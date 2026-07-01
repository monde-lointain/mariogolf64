#!/usr/bin/env python3
"""pick_target_config.py — shared paths, thresholds, regexes, and the UpstreamSource
cache for the pick_target ranker. Extracted from pick_target.py as a stdlib-only leaf
(imports only os/re/decomp_common) so every pick_target_* module imports it without a
cycle; call sites read the constants unchanged (pick_target re-exports them)."""

import os
import re

import decomp_common as dc


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
        "include/libnaudio",
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
    # LIBNAUDIO_CFLAGS (Makefile) prepends `-I src/libnaudio` (the n_audio_sc source-private internal
    # headers n_synthInternals.h/synthInternals.h/n_abi.h) and appends `-I include/libultra/PR` (the
    # bare <os_internal.h>/<ultraerror.h>/<libaudio.h> the n_syn*.c mirrors include), so a libnaudio
    # mirror resolves its 3-header DAG through these. Without them the whole n_syn* band false-flags
    # needs-header → blk (S129: the n_syn* family was hidden this way until the headers were vendored).
    "libnaudio": [
        os.path.join(ROOT, "src/libnaudio"),
        os.path.join(ROOT, "include/libultra/PR"),
    ],
    # LIBMUS_CFLAGS (mk/libmus.mk) prepends `-I src/libmus` (the libmus source-private internal
    # headers libmus_config.h/lib_memory.h/aud_*.h); the public libmus.h and bare
    # <libaudio.h>/<n_libaudio_*.h> resolve through include/libmus + include/libultra/PR + the base
    # include/libnaudio set. Without these a libmus mirror false-flags needs-header → blk (S141:
    # lib_memory.c was hidden this way until libmus_config.h/lib_memory.h were vendored). Each libmus
    # file vendors its own aud_*.h as it is banked, so a row still un-vendored stays blk on its own
    # headers (expected — the band opens incrementally, like libnaudio's n_syn* did).
    "libmus": [
        os.path.join(ROOT, "src/libmus"),
        os.path.join(ROOT, "include/libmus"),
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
# Also the donor for `.inc.c` body-includes (`#include "inc/<x>.inc.c"` inside a function body, e.g.
# the n_audio_sc N_MICRO command-stream fragments) — include_is_vendorable checks the full
# source-relative path here, so an `inc/*.inc.c` is a +1 vendorable enabler, not a `blk` DoR reject.
UPSTREAM_SRC_ROOTS = [
    LIBULTRA,  # ~/development/repos/ultralib/src
    os.path.expanduser(
        "~/development/repos/n64sdkmod/packages/libnaudio/usr/src/PR/libsrc/n_audio_sc/src"
    ),  # n_audio_sc (libnaudio) source-private headers + inc/*.inc.c body-includes
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
