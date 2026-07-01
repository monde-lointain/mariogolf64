#!/usr/bin/env python3
"""pick_target_index.py — extracted from pick_target.py."""

import functools
import glob
import math
import os
import re

from build_config import _build_version_ord, _strip_inactive_version_branches
from cpreprocess import _strip_dead_blocks
from decomp_asm import _FRAME_IMMS, _asm_signature
from pick_target_config import (
    ASM_INCLUDE_LINE_RE,
    ASM_TU_DEF_RE,
    ASM_VENDOR_INCLUDE_DIRS,
    AUDIO_CODDOG_MAPS,
    AUDIO_PINS,
    CODDOG_MAP,
    C_COMMENT_RE,
    INCLUDE_DIRS,
    KMC_ASM_TU_DEF_RE,
    LIBKMC,
    LIBNUSYS,
    LIBULTRA,
    LIB_EXTRA_INCLUDE_DIRS,
    MACRO_REF_RE,
    NUSYS_CODDOG_MAP,
    PRAGMA_WEAK_RE,
    ROOT,
    UPSTREAM_DEF_RE,
    UPSTREAM_TREES,
    _DEF_NONNAME_KW,
)
from pick_target_hazards import Hazard
from pick_target_yaml import YAML

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
