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
import glob
import json
import math
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
YAML = os.path.join(ROOT, "mariogolf64.yaml")
LIBULTRA = os.path.expanduser("~/development/repos/libultra_modern/src")
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

# Subseg line: `- [0x8DF10, c, libultra/monegi/rdp/dp]` or `- [0x8D230, asm]`.
# (bss entries use the `{ type: bss, ... }` brace form and are intentionally skipped.)
SUBSEG_RE = re.compile(
    r"^\s*-\s*\[\s*0x([0-9A-Fa-f]+)\s*,\s*([a-z]+)\s*(?:,\s*([^\]]+?))?\s*\]"
)
GLABEL_RE = re.compile(r"^\s*glabel\s+(\S+)")
# Approximate C function-definition: a return-type-led line ending in `name(`.
UPSTREAM_DEF_RE = re.compile(r"^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w+)\s*\(", re.M)
FILE_STATIC_RE = re.compile(r"^\s*static\b[^=]*;\s*$")
# A file-scope data-global *definition* (external linkage): `<type> <name> [= init];`.
# Applied only at brace-depth 0 (see defines_data_globals) so function-body statements
# and struct members never match. Captures the variable name.
DATA_GLOBAL_DEF_RE = re.compile(
    r"^(?:(?:struct|union|enum)\s+)?[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w+)"
    r"\s*(?:\[[^\]]*\]\s*)?(?:=[^;]*)?;\s*$"
)
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
# The project's quoted/angle include search dirs (Makefile CFLAGS `-I` set; -nostdinc removed S32 — stdarg.h ships at include/stdarg.h for correct MIPS GCC 2.7.2 vararg ABI).
INCLUDE_DIRS = [
    os.path.join(ROOT, d)
    for d in ("include", "include/libultra", "include/libultra/internal", "include/libkmc", "include/libnusys")
]
PROJECT_INC = os.path.join(ROOT, "include")
# Upstream include trees a missing companion header can be *copied from* (the execution-middle
# mirror, e.g. assert.h). A header absent here and absent from the project tree is a system
# header we don't ship (blocked); a header present in the project tree but unreachable under
# INCLUDE_DIRS is an unindexed-`-I` case (blocked, a deferred Makefile enabler — the audio
# band's <libaudio.h> at include/libultra/PR/). See classify of needs-header in seed_points.
UPSTREAM_INC_ROOTS = [
    os.path.expanduser("~/development/repos/libultra_modern/include"),
    os.path.expanduser("~/development/repos/libkmc/include"),
    os.path.expanduser(
        "~/development/repos/n64sdkmod/packages/libnusys/usr/src/PR/libsrc/nusys-2.07/nusys/include"
    ),
]
UPSTREAM_BONUS = 200
CARRYOVER_PENALTY = 1000
# A warm band (the candidate's mirror dir already holds a banked sibling) means its
# companion headers + callee symbols are in-tree → an enabler-free flip. Worth ~one
# leaf-function of size: it lifts a warm candidate over a band-cold one of similar
# size (the tiebreak intent) without overriding large size gaps. Validated twice —
# the si pair (Sprint 4) and vi pair (Sprint 5) both banked at one-file cost off warm bands.
BAND_WARM_BONUS = 64


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


def asm_functions(rom_off):
    """glabel names defined in asm/<ROM>.s for this subseg (curated if already synced)."""
    path = os.path.join(ROOT, "asm", f"{rom_off:X}.s")
    if not os.path.exists(path):
        return []
    names = []
    with open(path) as f:
        for line in f:
            m = GLABEL_RE.match(line)
            if m:
                names.append(m.group(1))
    return names


# `/* ROM VRAM BYTES */` instruction comment — the second field is the authoritative vram
# splat emitted for that ROM offset. Surfacing it spares the gate a rom→vram derivation for
# the mandatory recover-extern re-confirm (S22: a hand-guessed flat `rom + <const>` resolved
# mid-function and returned a wrong containing fn — read the vram, never guess it).
VRAM_COMMENT_RE = re.compile(r"/\*\s*[0-9A-Fa-f]+\s+([0-9A-Fa-f]{8})\s+[0-9A-Fa-f]{8}\s*\*/")


def subseg_vram(rom_off):
    """The target subseg's start vram, read from the first instruction comment in
    asm/<ROM>.s. Returns an int, or None when the asm listing is absent/unreadable."""
    path = os.path.join(ROOT, "asm", f"{rom_off:X}.s")
    if not os.path.exists(path):
        return None
    with open(path) as f:
        for line in f:
            m = VRAM_COMMENT_RE.search(line)
            if m:
                return int(m.group(1), 16)
    return None


# Ops C can't emit (or won't schedule into a leaf delay slot): CP0 register moves
# and a bare FPU sqrt. A tiny leaf whose only work is one of these is really `hasm`,
# not a classical target — flag it so smallest-first stops surfacing it as the pick.
INTRINSIC_OPS = {
    "mfc0", "mtc0", "dmfc0", "dmtc0", "cfc0", "ctc0",
    "sqrt.s", "sqrt.d",
}
INSN_RE = re.compile(r"/\*\s*[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s*\*/\s+(\S+)")


def intrinsic_likely(rom_off, primary):
    """True if the primary fn is a register/FPU intrinsic shim (mfc0/mtc0/sqrt + jr),
    not C-expressible. Reads the fn's body in asm/<ROM>.s: a leaf (no `jal`) whose
    work instructions (excluding jr/nop) are all CP0-moves/sqrt, or one spimdisasm
    tagged `handwritten`."""
    path = os.path.join(ROOT, "asm", f"{rom_off:X}.s")
    if not os.path.exists(path):
        return False
    in_fn = False
    mnemonics = []
    handwritten = False
    with open(path) as f:
        for line in f:
            m = GLABEL_RE.match(line)
            if m:
                if in_fn:           # reached the next function — stop
                    break
                in_fn = m.group(1) == primary
                continue
            if not in_fn:
                continue
            if line.lstrip().startswith("endlabel"):
                break
            if "handwritten" in line:
                handwritten = True
            insn = INSN_RE.search(line)
            if insn:
                mnemonics.append(insn.group(1))
    if "jal" in mnemonics:          # a wrapper/has calls → decompilable
        return False
    work = [m for m in mnemonics if m not in ("jr", "nop")]
    if work and all(m in INTRINSIC_OPS for m in work):
        return True
    return handwritten             # spimdisasm-tagged handwritten leaf (no jal)


def build_upstream_index():
    """Map upstream function name -> (lib, path), scanning libultra/libkmc once."""
    index = {}
    for lib, tree in UPSTREAM_TREES:
        if not os.path.isdir(tree):
            continue
        for cpath in glob.glob(os.path.join(tree, "**", "*.c"), recursive=True):
            try:
                with open(cpath, errors="ignore") as f:
                    text = f.read()
            except OSError:
                continue
            for m in UPSTREAM_DEF_RE.finditer(text):
                index.setdefault(m.group(1), (lib, cpath))
    return index


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

CALL_INSN_RE = re.compile(r"\bjal\s+([A-Za-z_]\w+)")
# Immediate-bearing ALU/load-imm ops; the operand we want is the trailing immediate literal.
IMM_INSN_RE = re.compile(
    r"\b(?:addiu|ori|andi|xori|lui|li|slti|sltiu|daddiu)\b.*?(-?0x[0-9A-Fa-f]+)\s*$")
# C identifiers used as call targets: `name(`. Excludes control keywords (see _C_NONCALL).
C_CALL_RE = re.compile(r"\b([A-Za-z_]\w+)\s*\(")
C_INT_RE = re.compile(r"\b(0x[0-9A-Fa-f]+|\d+)\b")
_C_NONCALL = {
    "if", "for", "while", "switch", "return", "sizeof", "do", "else", "case",
    "defined", "OS_LOG_FLOAT", "assert",
}
# Stack-frame / register-save offsets dominate addiu/sw immediates and carry no identity;
# ignore them so the constant signal isn't pure noise. (Kept tiny — callees do the real work.)
_FRAME_IMMS = {"0x10", "0x14", "0x18", "0x1c", "0x20", "0x24", "0x28", "-0x10",
               "-0x14", "-0x18", "-0x1c", "-0x20", "-0x24", "-0x28"}


def _asm_signature(rom_off, primary):
    """(callee set, constant set) for the target fn from asm/<ROM>.s. Callees exclude internal
    `func_*`/`.L*` targets (only symbolic SDK-ish names carry cross-build identity)."""
    path = os.path.join(ROOT, "asm", f"{rom_off:X}.s")
    if not os.path.exists(path):
        return set(), set()
    callees, consts = set(), set()
    in_fn = False
    with open(path) as f:
        for line in f:
            m = GLABEL_RE.match(line)
            if m:
                if in_fn:
                    break
                in_fn = m.group(1) == primary
                continue
            if not in_fn:
                continue
            if line.lstrip().startswith("endlabel"):
                break
            cm = CALL_INSN_RE.search(line)
            if cm and not cm.group(1).startswith("func_"):
                callees.add(cm.group(1))
            im = IMM_INSN_RE.search(line)
            if im and im.group(1).lower() not in _FRAME_IMMS:
                consts.add(im.group(1).lower())
    return callees, consts


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
    return f"maybe-upstream:{lib}:" + ",".join(bases)


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

_DEAD_OPEN_RE = re.compile(r"#\s*if(?:def\s+(?:_DEBUG|NU_DEBUG)|ndef\s+_FINALROM|\s+0)\b")
_PP_IF_RE = re.compile(r"#\s*if")
_PP_ENDIF_RE = re.compile(r"#\s*endif")
_STR_LIT_RE = re.compile(r'"[^"\\]*(?:\\.[^"\\]*)*"')
_IFDEF_OPEN_RE = re.compile(r'^\s*#\s*ifdef\s+([A-Za-z_][A-Za-z0-9_]*)')
_PP_OPENING_DIRECTIVE_RE = re.compile(r'^\s*#\s*if(?:def|ndef)?\b')
_PP_ENDIF_LINE_RE = re.compile(r'^\s*#\s*endif\b')
_MAKEFILE_DEFINE_RE = re.compile(r'-D([A-Za-z_][A-Za-z0-9_]*)(?:=[^\s]*)?')


def _strip_string_literals(text):
    """Replace string literal contents with empty quotes to prevent C_CALL_RE false matches."""
    return _STR_LIT_RE.sub('""', text)


def _strip_dead_blocks(text):
    """Drop `#ifdef _DEBUG` / `#ifdef NU_DEBUG` / `#ifndef _FINALROM` / `#if 0` regions (matched #endif, nested)."""
    out, in_dead, nest = [], False, 0
    for line in text.splitlines():
        s = line.lstrip()
        if in_dead:
            if _PP_IF_RE.match(s):
                nest += 1
            elif _PP_ENDIF_RE.match(s):
                if nest == 0:
                    in_dead = False
                else:
                    nest -= 1
            continue
        if _DEAD_OPEN_RE.match(s):
            in_dead, nest = True, 0
            continue
        out.append(line)
    return "\n".join(out)


_PROJECT_DEFINES = None


def _parse_makefile_defines():
    """Return {lib: frozenset(defines)} by parsing the project Makefile.
    'libkmc' and 'libnusys' inherit from the main CFLAGS set plus their own additions."""
    makefile = os.path.join(ROOT, "Makefile")
    raw = {"main": set(), "libkmc": set(), "libnusys": set()}
    try:
        with open(makefile) as f:
            for line in f:
                for var, key in [("CFLAGS", "main"), ("LIBKMC_CFLAGS", "libkmc"), ("LIBNUSYS_CFLAGS", "libnusys")]:
                    if re.match(rf'^\s*{var}\s*[:+]?=', line):
                        raw[key].update(m.group(1) for m in _MAKEFILE_DEFINE_RE.finditer(line))
    except OSError:
        pass
    raw["libkmc"] |= raw["main"]
    raw["libnusys"] |= raw["main"]
    return {k: frozenset(v) for k, v in raw.items()}


def _active_defines_for_lib(lib):
    """Effective -D defines for an upstream library key (libultra, libkmc, libnusys)."""
    global _PROJECT_DEFINES
    if _PROJECT_DEFINES is None:
        _PROJECT_DEFINES = _parse_makefile_defines()
    key = {"libkmc": "libkmc", "libnusys": "libnusys"}.get(lib or "", "main")
    return _PROJECT_DEFINES[key]


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


def _upstream_body(up_cpath, primary):
    """The brace-matched body of `primary` in its upstream .c, or None."""
    try:
        text = open(up_cpath, errors="ignore").read()
    except OSError:
        return None
    for name, body in _iter_upstream_functions(text):
        if name == primary:
            return body
    return None


def _asm_jal_count(rom_off, primary):
    """Count of `jal <name>` in `primary`'s body in asm/<ROM>.s (incl. func_ targets), or None
    when the asm is absent (e.g. the subseg was already flipped to c)."""
    path = os.path.join(ROOT, "asm", f"{rom_off:X}.s")
    if not os.path.exists(path):
        return None
    in_fn, n = False, 0
    with open(path) as f:
        for line in f:
            m = GLABEL_RE.match(line)
            if m:
                if in_fn:
                    break
                in_fn = m.group(1) == primary
                continue
            if not in_fn:
                continue
            if line.lstrip().startswith("endlabel"):
                break
            if CALL_INSN_RE.search(line):
                n += 1
    return n


def call_divergence(rom_off, primary, up_cpath):
    """Advisory `jal-count-mismatch:<C>vs<asm>` when the upstream call count for `primary` differs
    from the ROM fn's jal count — an upstream-vs-ROM build divergence. None when they agree, the
    body/asm is unreadable, or the count can't be taken. Advisory only; the gate confirms."""
    body = _upstream_body(up_cpath, primary)
    if body is None:
        return None
    n_asm = _asm_jal_count(rom_off, primary)
    if n_asm is None:
        return None
    n_c = len([n for n in C_CALL_RE.findall(_strip_string_literals(_strip_dead_blocks(body))) if n not in _C_NONCALL])
    if n_c == n_asm:
        return None
    return f"jal-count-mismatch:{n_c}vs{n_asm}"


def has_file_scope_static(cpath):
    """True if the upstream .c declares a file-scope static (BSS-layout hazard → classical loop)."""
    with open(cpath, errors="ignore") as f:
        for line in f:
            stripped = line.lstrip()
            if stripped.startswith(("//", "*")):
                continue
            if FILE_STATIC_RE.match(line):
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
    with open(cpath, errors="ignore") as f:
        for raw in f:
            line = raw.strip()
            if (depth == 0 and not in_kr_params and line and not line.startswith(
                    ("#", "//", "*", "}", "static", "extern", "typedef"))):
                m = DATA_GLOBAL_DEF_RE.match(line)
                if m and "(" not in line.split("=", 1)[0]:
                    names.append(m.group(1))
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


_PLACED_SYMBOLS = None


def placed_symbols():
    """Lazily-built set of every symbol name in the two name files (symbol_addrs + ghidra). A
    name in this set has a linker address; one absent from it does not (a data extern then
    link-fails). Format: `name = 0x...; // ...`."""
    global _PLACED_SYMBOLS
    if _PLACED_SYMBOLS is None:
        names = set()
        for nf in NAME_FILES:
            if not os.path.exists(nf):
                continue
            with open(nf, errors="ignore") as f:
                for line in f:
                    m = re.match(r"\s*([A-Za-z_]\w+)\s*=", line)
                    if m:
                        names.add(m.group(1))
        _PLACED_SYMBOLS = names
    return _PLACED_SYMBOLS


# An SDK-internal global is `__`-prefixed (e.g. __osRunQueue). Uppercase macros ([A-Z]-led) and
# CamelCase types never match, so this rarely false-flags a non-symbol token.
SDK_GLOBAL_RE = re.compile(r"__[A-Za-z]\w+")

# A data extern declaration in a header: `extern <type...> <name>[opt-array];` with NO '(' before
# the `;` (the lookahead drops function prototypes and function-pointer externs). Captures the
# final identifier — the declared global's name (e.g. `extern volatile u32 nuGfxTaskSpool;`).
EXTERN_DATA_DECL_RE = re.compile(
    r"^\s*extern\s+(?![^;\n]*\()[^;\n]*?\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*;", re.M
)


def _resolve_include(inc):
    """First INCLUDE_DIRS hit for an `#include` target, or None (an unindexed / system header)."""
    for d in INCLUDE_DIRS:
        path = os.path.join(d, inc)
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
        text = open(cpath, errors="ignore").read()
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


_ALL_FUNC_MACROS = None


def all_func_macros():
    """{name: replacement-body} for every function-like macro defined anywhere under the project
    -I set, scanned once and cached. A project-wide table (not a per-file header walk) so the
    macro-name exclusion in calls_unplaced is COMPLETE — a macro pulled into an expansion through
    a nested invocation (IO_READ → PHYS_TO_K1) is still recognised as a macro even when its own
    defining header sits beyond the invoking file's include depth, so it is never mis-flagged as
    an unplaced call."""
    global _ALL_FUNC_MACROS
    if _ALL_FUNC_MACROS is None:
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
        _ALL_FUNC_MACROS = macros
    return _ALL_FUNC_MACROS


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
        text = open(cpath, errors="ignore").read()
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


def refs_unplaced(cpath, placed):
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
        text = open(cpath, errors="ignore").read()
    except OSError:
        return []
    # Append one-level macro expansion so a global referenced only inside an invoked library macro
    # (EPI_SYNC → __osCurrentHandle, S41) is visible to the same `__`-prefix / declared-extern grep.
    macro_text, _ = macro_hidden_text(cpath)
    scan = text + "\n" + macro_text
    # `__`-prefixed SDK globals (libultra/libkmc) + any data extern the .c's headers *declare*
    # (libnusys/libmus non-`__` globals SDK_GLOBAL_RE misses — the S16 nuGfxTaskSpool false-clean).
    tokens = set(SDK_GLOBAL_RE.findall(scan)) | declared_extern_data(cpath)
    unplaced = []
    for tok in tokens:
        if tok in placed or tok.startswith("__builtin_"):  # GCC intrinsic, never a linked global
            continue
        # Only flag names the .c (or its invoked macros) actually references.
        if not re.search(r"\b" + re.escape(tok) + r"\b", scan):
            continue
        # Called anywhere (`tok(` allowing whitespace) → a function, not a data extern → skip.
        if re.search(re.escape(tok) + r"\s*\(", scan):
            continue
        unplaced.append(tok)
    return sorted(unplaced)


# splat emits `D_<8-hex-vaddr>` auto-labels for any referenced data address WITHOUT a name in
# the two name files; a *placed* symbol shows its real name instead. So every `D_<addr>` in a
# subseg's asm is, by construction, an unplaced data reference — and the recovered vram is the
# label itself (no MCP `disassemble_function` needed). The fallback `lui reg,0xHHHH` + `addiu`
# pair (raw immediate, not %hi(...)) catches a non-D_ materialization.
DATA_LABEL_RE = re.compile(r"\bD_([0-9A-Fa-f]{8})\b")
RAW_HILO_RE = re.compile(
    r"\blui\s+\$\w+,\s*0x([0-9A-Fa-f]{4})\b.*?\b(?:addiu|lw|sw|lh|sh|lhu|lbu|lb|sb)\s"
    r"[^\n]*?(-?0x[0-9A-Fa-f]+)",
    re.DOTALL,
)


def recover_unplaced_vram(rom_off):
    """Candidate vrams of the unplaced data this subseg references, read from `asm/<ROM>.s`
    deterministically (the asm-data-recovery the execution middle would otherwise do via MCP).
    Returns a sorted list of distinct addresses. The caller binds a name→vram only when the
    mapping is unambiguous (exactly one unplaced upstream name ∩ one candidate) — the
    osYieldThread floor case; multi-extern stays a hint. The gate still confirms before any
    symbol_addrs add, so a stray local-rodata D_ over-listing is harmless."""
    path = os.path.join(ROOT, "asm", f"{rom_off:X}.s")
    if not os.path.exists(path):
        return []
    try:
        text = open(path, errors="ignore").read()
    except OSError:
        return []
    addrs = {int(h, 16) for h in DATA_LABEL_RE.findall(text)}
    for hi, lo in RAW_HILO_RE.findall(text):
        addrs.add(((int(hi, 16) << 16) + int(lo, 16)) & 0xFFFFFFFF)
    return sorted(addrs)


# splat labels a callee whose vram is in NEITHER name file `func_<8-hex>` in the asm jal; a
# *placed* callee shows its real name. So every `jal func_<addr>` is, by construction, an
# unplaced *function* reference — the function dual of the unnamed-data `D_<addr>` label.
FUNC_JAL_RE = re.compile(r"\bjal\s+func_([0-9A-Fa-f]{8})\b")

# Strip C comments and string literals so neither a copyright header
# (`/* ... Copyright (C) ... Ohki ... */`) nor a format string
# (`osSyncPrintf("... address(0x%X) ...")`) can masquerade as a `name(` call token.
_BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
_LINE_COMMENT_RE = re.compile(r"//[^\n]*")
_STRING_LIT_RE = re.compile(r'"(?:\\.|[^"\\\n])*"')


def _strip_comments(text):
    """Blank C comments then string literals (comments first, so a `"` inside one is gone)."""
    text = _LINE_COMMENT_RE.sub("", _BLOCK_COMMENT_RE.sub(" ", text))
    return _STRING_LIT_RE.sub('""', text)


def calls_unplaced(cpath, primary, placed):
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
        text = open(cpath, errors="ignore").read()
    except OSError:
        return []
    defined = {name for name, _ in _iter_upstream_functions(text)}
    # Append one-level macro expansion so a callee invoked only inside a library macro is seen;
    # exclude the macro names themselves (a macro body that invokes UPDATE_REG/WAIT_ON_IOBUSY etc.
    # must not flag those as unplaced functions — they inline, they don't link).
    macro_text, macro_names = macro_hidden_text(cpath)
    body = _strip_string_literals(_strip_dead_blocks(_strip_comments(text + "\n" + macro_text)))
    called = {
        n
        for n in C_CALL_RE.findall(body)
        if n not in _C_NONCALL and n not in macro_names and not n.startswith("__builtin_")
    }
    return sorted(n for n in called if n not in placed and n not in defined)


def recover_unplaced_call_vram(rom_off, primary):
    """Addresses of `primary`'s *unnamed* jal targets (`jal func_<hex>`) in asm/<ROM>.s — the
    callees splat couldn't name because they're absent from both name files. The function dual of
    recover_unplaced_vram (data). Returns a sorted list of distinct addresses; the caller binds
    name→vram only when unambiguous (one unplaced upstream call ∩ one unnamed jal). The gate
    confirms before any symbol_addrs add."""
    path = os.path.join(ROOT, "asm", f"{rom_off:X}.s")
    if not os.path.exists(path):
        return []
    in_fn, addrs = False, set()
    with open(path) as f:
        for line in f:
            m = GLABEL_RE.match(line)
            if m:
                if in_fn:
                    break
                in_fn = m.group(1) == primary
                continue
            if not in_fn:
                continue
            if line.lstrip().startswith("endlabel"):
                break
            for h in FUNC_JAL_RE.findall(line):
                addrs.add(int(h, 16))
    return sorted(addrs)


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


def missing_includes(cpath):
    """Upstream includes that don't resolve under the project -I set (-nostdinc, so every
    header must come from INCLUDE_DIRS). A non-empty result is the `needs-header` DoR hazard:
    the verbatim mirror won't compile until each is satisfied — copy the companion header
    (execution-middle, e.g. assert.h) or add a -I path (deferred enabler, e.g. the audio band's
    <libaudio.h> at include/libultra/PR/). Conservative: scans all `#include` lines regardless
    of `#ifdef` state, so a dead `#ifdef _DEBUG` include may over-flag — the gate confirms."""
    missing = []
    with open(cpath, errors="ignore") as f:
        for line in f:
            m = INCLUDE_RE.match(line)
            if not m:
                continue
            inc = m.group(1)
            if not any(os.path.exists(os.path.join(d, inc)) for d in INCLUDE_DIRS):
                if inc not in missing:
                    missing.append(inc)
    return missing


_PROJECT_INC_INDEX = None


def _project_inc_index():
    """Lazily-built set of every header under the project `include/` tree (both its root-relative
    path and its basename) — used to tell an unreachable-but-present header (unindexed `-I`,
    defer) from a genuinely-absent one (copyable from upstream, or a system header)."""
    global _PROJECT_INC_INDEX
    if _PROJECT_INC_INDEX is None:
        idx = set()
        for f in glob.glob(os.path.join(PROJECT_INC, "**", "*"), recursive=True):
            if os.path.isfile(f):
                idx.add(os.path.relpath(f, PROJECT_INC))
                idx.add(os.path.basename(f))
        _PROJECT_INC_INDEX = idx
    return _PROJECT_INC_INDEX


def include_is_blocked(inc):
    """Classify a *missing* include (one `missing_includes` already found unreachable). Blocked =
    can't be fixed by a companion-header copy: either it's present in the project `include/` tree
    but unreachable under the current `-I` set (unindexed `-I` — a deferred Makefile enabler), or
    it's absent everywhere (a system header we don't ship). Copyable (NOT blocked) = absent from
    the project tree but present at `<upstream-include-root>/<inc>` → mirror it in mid-execution
    (assert.h). Heuristic, gate-confirmed (basename match can rarely over-flag)."""
    idx = _project_inc_index()
    if inc in idx or os.path.basename(inc) in idx:
        return True  # present in-tree but unreachable → unindexed -I, defer
    for root in UPSTREAM_INC_ROOTS:
        if os.path.exists(os.path.join(root, inc)):
            return False  # absent in-tree, copyable from upstream → companion-copy
    return True  # nowhere → system header / unavailable


def snap_fib(n):
    """Round a raw additive score up to the nearest Fibonacci ladder rung (1,2,3,5,8,13)."""
    for f in (1, 2, 3, 5, 8, 13):
        if n <= f:
            return f
    return 13


def seed_points(size, upstream, band, nfns, hazards, blocked):
    """Deterministic a-priori story-point seed (Fibonacci 1,2,3,5,8,13) for one increment —
    the v1 story-point system (see VELOCITY.md). MG64's effort axis is PATH (upstream-mirror vs
    classical-loop) + ENABLER LOAD (band warmth, hazards), not raw bytes; the byte gates (768 /
    1536) are dormant in the current <256 B regime and only bite for large/classical fns.
    `hazards` is the joined hazard string; `blocked` is True when any needs-header is un-pickable
    (see include_is_blocked). Returns an int, or 'blk' for a blocked (un-pickable) target.
    Display-only — does NOT influence the smallest-first sort. A committed cluster seed is the
    SUM of its files' seeds (banked per-file, not all-or-nothing across the cluster)."""
    if blocked:
        return "blk"  # un-pickable until the -I path is added / header shipped — a DoR reject
    classical = upstream == "none"
    pack = nfns > 1
    drop = ("file-static" in hazards) or ("defines-data" in hazards)  # → classical fallback
    needs_copy = "needs-header" in hazards  # a copyable companion header (blocked ones already 'blk')
    needs_define = "needs-define" in hazards  # Makefile build-define enabler (e.g. USE_EPI in LIBNUSYS_CFLAGS)
    recover = ("refs-unplaced" in hazards) or ("calls-unplaced" in hazards)  # symbol-recovery enabler before the mirror links
    big = size >= 768
    huge = size >= 1536
    if huge or (classical and pack):
        return 13  # must decompose; never a 1-increment sprint
    if nfns >= 4:
        return 8  # a large pack must decompose regardless of path (hits the 8-gate)
    if classical and (big or pack or drop or needs_copy or recover):
        return 8  # enabler gate — decompose / scaffold first
    if classical:
        return 5  # small single classical fn — unproven regime, high variance
    base = 1 if band == "warm" else 2  # warm = enabler-free; cold = symbol recovery / cold deps
    if drop:
        base += 1  # file-static / defines-data drop → classical fallback
    if needs_copy:
        base += 1  # companion-header copy
    if needs_define:
        base += 1  # Makefile build-define enabler
    if recover:
        base += 1  # asm-data-recovery of an unplaced data extern (S2 __osThreadTail pattern)
    if pack or big:
        base += 1
    return snap_fib(base)


def carry_over_names():
    """Function names parked in BACKLOG.md ## Carry-overs (de-ranked)."""
    names = set()
    if not os.path.exists(BACKLOG):
        return names
    with open(BACKLOG) as f:
        parts = f.read().split("## Carry-overs", 1)
    if len(parts) > 1:
        names.update(re.findall(r"`([A-Za-z_]\w+)`", parts[1]))
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
        hazards = []
        if size and size % 16 != 0:
            hazards.append("non16align")
        if len(fns) > 1:
            # List each fn + its upstream basename so the gate can tell a multi-file pack
            # (different basenames → split at the upstream-file boundary, e.g. dpsetstat+dpctr)
            # from a single-file pack, without hand-disassembling asm/<rom>.s.
            members = []
            for fn in fns:
                _, fn_up = upstream_index.get(fn, (None, None))
                stem = os.path.splitext(os.path.basename(fn_up))[0] if fn_up else "?"
                members.append(f"{fn}={stem}")
            hazards.append(f"pack:{len(fns)}fn[" + ",".join(members) + "]")
        if intrinsic_likely(off, fns[0]):
            hazards.append("intrinsic-likely")  # register/FPU shim → hasm, not a classical target
        return "asm-flip", fns, hazards
    if typ == "c" and path:
        cpath = os.path.join(ROOT, "src", path + ".c")
        if not os.path.exists(cpath):
            return None
        text = open(cpath).read()
        fns = re.findall(r"INCLUDE_ASM\([^,]+,\s*([A-Za-z_]\w+)", text)
        if not fns:  # 0 stubs => already an md5-candidate
            return None
        return "c-stub", fns, [f"remaining:{len(fns)}"]
    return None


def append_upstream_hazards(off, primary, up_lib, up_path, hazards):
    """Append the upstream-mirror hazards for a named candidate (in-place, in the
    order the gate reads them) and return (band, blocked)."""
    blocked = False
    if has_file_scope_static(up_path):
        hazards.append("file-static")  # route to the classical loop, not the mirror
    data_defs = defines_data_globals(up_path)
    if data_defs:
        hazards.append("defines-data:" + ",".join(data_defs))  # .data analogue → classical loop
    unplaced = refs_unplaced(up_path, placed_symbols())
    if unplaced:
        # Annotate the recovered vram inline when the binding is unambiguous (one unplaced
        # name ∩ one asm candidate) so the gate copy-pastes the symbol_addrs entry instead
        # of re-running MCP disassemble_function (S12 retro #1). Ambiguous → bare names.
        cands = recover_unplaced_vram(off)
        if len(unplaced) == 1 and len(cands) == 1:
            refs = [f"{unplaced[0]}@0x{cands[0]:08X}"]
        else:
            refs = unplaced
        hazards.append("refs-unplaced:" + ",".join(refs))  # asm-data-recovery before flip
    unplaced_calls = calls_unplaced(up_path, primary, placed_symbols())
    if unplaced_calls:
        # Annotate the recovered vram inline when unambiguous (one unplaced call ∩ one
        # unnamed jal), mirroring refs-unplaced, so the gate copy-pastes the func entry.
        ccands = recover_unplaced_call_vram(off, primary)
        if len(unplaced_calls) == 1 and len(ccands) == 1:
            crefs = [f"{unplaced_calls[0]}@0x{ccands[0]:08X}"]
        else:
            crefs = unplaced_calls
        hazards.append("calls-unplaced:" + ",".join(crefs))  # recover func symbol before flip
    missing = missing_includes(up_path)
    if missing:
        hazards.append("needs-header:" + ",".join(missing))
        blocked = any(include_is_blocked(inc) for inc in missing)
    gating = function_gating_define(up_path, primary)
    if gating and gating not in _active_defines_for_lib(up_lib):
        hazards.append(f"needs-define:{gating}")
    divergence = call_divergence(off, primary, up_path)
    if divergence:
        hazards.append(divergence)  # near-verbatim mirror: reconcile call list at the gate
    band = "warm" if band_is_warm(band_mirror_dir(up_lib, up_path)) else "cold"
    return band, blocked


def score_row(off, primary, kind, fns, size, up_lib, band, blocked, hazards, carried):
    """Assemble the ranked row dict (score folds size + upstream/warm/carry bonuses)."""
    score = -size + (UPSTREAM_BONUS if up_lib else 0)
    if band == "warm":
        score += BAND_WARM_BONUS  # prefer enabler-free warm-band siblings over equally-small cold ones
    if primary in carried:
        score -= CARRYOVER_PENALTY
    hz = ",".join(hazards) or "-"
    vram = subseg_vram(off)
    return {
        "func": primary, "rom": f"0x{off:X}",
        "vram": f"0x{vram:08X}" if vram is not None else "?",
        "size": size, "nfns": len(fns),
        "pts": seed_points(size, up_lib or "none", band, len(fns), hz, blocked),
        "kind": kind, "upstream": up_lib or "none", "band": band,
        "hazards": hz, "score": score,
    }


def build_rows(args, upstream_index, carried, sig_index):
    subs = parse_subsegs()
    rows = []
    for i, (off, typ, path) in enumerate(subs):
        size = subs[i + 1][0] - off if i + 1 < len(subs) else 0
        classified = classify_subseg(off, typ, path, size, upstream_index)
        if classified is None:
            continue
        kind, fns, hazards = classified

        primary = fns[0]
        up_lib, up_path = upstream_index.get(primary, (None, None))
        band = "-"
        blocked = False
        if up_path:
            band, blocked = append_upstream_hazards(off, primary, up_lib, up_path, hazards)
        elif kind == "asm-flip" and primary.startswith("func_"):
            # No name-index hit + un-named: it may still be an un-named SDK mirror (the S13
            # trap). Run the signature matcher; an advisory hit flags the gate to verify.
            hint = signature_hint(off, primary, sig_index)
            if hint:
                hazards.append(hint)

        if args.lib and args.lib not in (path or "") and not any(args.lib in n for n in fns):
            continue
        if primary in carried and not args.include_stuck:
            continue

        rows.append(score_row(off, primary, kind, fns, size, up_lib, band, blocked, hazards, carried))
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
                      build_signature_index())
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
