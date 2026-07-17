#!/usr/bin/env python3
"""pick_target_classify.py — extracted from pick_target.py."""

import functools
import glob
import os
import re

from build_config import (
    _BUILD_VERSION_IF_RE,
    _PP_ENDIF_LINE_RE,
    _PP_OPENING_DIRECTIVE_RE,
    _build_version_ord,
    _strip_inactive_define_branches,
    _strip_inactive_version_branches,
)
from cpreprocess import (
    _strip_comments,
    _strip_dead_blocks,
    _strip_define_lines,
    _strip_string_literals,
)
from decomp_asm import (
    _asm_jal_count,
    asm_function_addrs,
    asm_functions,
    code_end_rom,
    intrinsic_likely,
    privileged_asm,
    recover_unplaced_call_vram,
    recover_unplaced_vram,
    rodata_double_literals,
    rodata_jtbls,
    rodata_literals,
    rodata_word_refs,
    subseg_vram,
)
from pick_target_config import (
    BLOCK_REORDER_FAMILIES,
    BOOT_GLOBALS,
    DATA_GLOBAL_DEF_RE,
    FILE_STATIC_CONST_ARRAY_RE,
    FILE_STATIC_INIT_ARRAY_RE,
    FILE_STATIC_RE,
    INCLUDE_DIRS,
    INCLUDE_RE,
    LIBULTRA,
    LIB_EXTRA_INCLUDE_DIRS,
    LOCAL_STATIC_DATA_RE,
    NAME_FILES,
    PROJECT_INC,
    ROOT,
    UPSTREAM_DEF_RE,
    UPSTREAM_INC_ROOTS,
    UPSTREAM_SRC_ROOTS,
    UPSTREAM_TREES,
    UpstreamSource,
)
from pick_target_hazards import (
    HAZARD_DATA_STATIC,
    HAZARD_PACK,
    HAZARD_RODATA_LITERAL,
    HAZARD_SINGLE_FILE_PACK,
    Hazard,
)
from pick_target_index import (
    C_CALL_RE,
    _C_NONCALL,
    _IFDEF_OPEN_RE,
    _MAKEFILE_DEFINE_RE,
    _iter_upstream_functions,
    _os_version_defined_tokens,
    _static_carve_siblings,
    _upstream_file_func_count,
    build_asm_tu_index,
    build_kmc_asm_tu_index,
    vendorable_tu_data_symbols,
    vendorable_tu_jtbl,
    vendorable_tu_missing_defines,
)
from pick_target_yaml import _literal_in_rodata, _rodata_carve_end_vram, _rodata_carve_start_vram

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
    'libkmc'/'libnusys'/'libultra'/'libnaudio' each inherit from the main CFLAGS set plus their own
    profile additions. The libultra profile carries -DBUILD_VERSION + -DF3DEX_GBI_2
    — without parsing LIBULTRA_CFLAGS the GBI microcode define is invisible, so a
    GBI-value-guarded macro (OS_YIELD_DATA_SIZE) false-flags as needs-define on a build
    that already defines it. The libnaudio profile carries -DN_MICRO=1 (the n_audio_sc
    micro command-stream variant): without parsing LIBNAUDIO_CFLAGS an N_MICRO-guarded
    function (n_save.c et al.) false-flags as needs-define on a build that already defines it."""
    build_files = [os.path.join(ROOT, "Makefile")] + sorted(
        glob.glob(os.path.join(ROOT, "mk", "*.mk"))
    )
    raw = {
        "main": set(),
        "libkmc": set(),
        "libnusys": set(),
        "libultra": set(),
        "libnaudio": set(),
        "libmus": set(),
    }
    for path in build_files:
        try:
            with open(path) as f:
                for line in f:
                    for var, key in [
                        ("CFLAGS", "main"),
                        ("LIBKMC_CFLAGS", "libkmc"),
                        ("LIBNUSYS_CFLAGS", "libnusys"),
                        ("LIBULTRA_CFLAGS", "libultra"),
                        ("LIBNAUDIO_CFLAGS", "libnaudio"),
                        ("LIBMUS_CFLAGS", "libmus"),
                    ]:
                        if re.match(rf"^\s*{var}\s*[:+]?=", line):
                            raw[key].update(
                                m.group(1) for m in _MAKEFILE_DEFINE_RE.finditer(line)
                            )
        except OSError:
            continue
    for k in ("libkmc", "libnusys", "libultra", "libnaudio", "libmus"):
        raw[k] |= raw["main"]
    return {k: frozenset(v) for k, v in raw.items()}

def _active_defines_for_lib(lib):
    """Effective -D defines for an upstream library key (libultra, libkmc, libnusys, libnaudio, libmus)."""
    key = {
        "libkmc": "libkmc",
        "libnusys": "libnusys",
        "libultra": "libultra",
        "libnaudio": "libnaudio",
        "libmus": "libmus",
    }.get(lib or "", "main")
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

def _macro_single_real_call(name, macros):
    """True if the function-like macro `name`'s replacement body wraps EXACTLY ONE real named call
    (e.g. `alHeapAlloc(hp,n,sz)` -> `alHeapDBAlloc(0,0,hp,n,sz)`). Such a macro emits one `jal` per
    invocation, but `_c_jal_count` drops it like any macro (most emit none), under-counting the C side
    by one per invocation. One level only — the body's own nested macros are not recursed, matching the
    one-level expansion the gate reconciles against the asm. S136."""
    entry = macros.get(name)
    if not entry:
        return False
    plist, body = entry
    pset = set(plist)
    inner = [
        n
        for n in C_CALL_RE.findall(body)
        if n not in _C_NONCALL and n not in macros and n not in pset
    ]
    return len(inner) == 1

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
    # Same for the profile's `-D` define gates (libnaudio `-DN_MICRO=1`): the non-micro `_getRate`'s
    # `_frexpf`/`_ldexpf` inflated n_env's `jal-count-mismatch:20vs15` on a byte-clean N_MICRO mirror.
    body = _strip_inactive_define_branches(body, _active_defines_for_lib(lib))
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
    # Macro-expansion artifact: a function-like macro that wraps exactly one real call (alHeapAlloc ->
    # alHeapDBAlloc) emits one jal per invocation, but _c_jal_count drops it -> the C side under-counts.
    # When the asm surplus (n_asm > n_c) is EXACTLY the count of such single-real-call macro invocations,
    # the mismatch is the macro expansion, not a logic divergence (S136 n_synthesizer 3vs8 = 5 alHeapAlloc).
    macros = all_func_macros()
    n_macro_calls = sum(
        1
        for name in C_CALL_RE.findall(stripped)
        if name in macros and _macro_single_real_call(name, macros)
    )
    macro_artifact = n_macro_calls > 0 and (n_asm - n_c) == n_macro_calls
    return Hazard.jal_count_mismatch(
        n_c, n_asm, version_artifact=version_artifact, macro_artifact=macro_artifact
    )

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

@functools.cache
def placed_symbol_addrs():
    """name -> address string for every curated symbol in the two name files (first wins) — the
    name->addr companion to placed_symbols(), built once per process. Used by the
    static-name-collision scan to cite the EXISTING placed address."""
    addrs = {}
    for nf in NAME_FILES:
        if not os.path.exists(nf):
            continue
        with open(nf, errors="ignore") as f:
            for line in f:
                m = re.match(r"\s*([A-Za-z_]\w+)\s*=\s*(0x[0-9A-Fa-f]+)", line)
                if m:
                    addrs.setdefault(m.group(1), m.group(2))
    return addrs

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

# A function-pointer TYPEDEF in a header: `typedef <ret> (*NAME)(args);` — captures NAME. A
# parameter typed by such a typedef (`ALDMANew dmaNew`) is invoked by a verbatim mirror through a
# `jalr` on the pointer, not a `jal` to a named symbol, so it must not flag calls-unplaced. Unlike
# the explicit `T (*name)(args)` param form (caught syntactically in _fn_ptr_param_names), a
# typedef'd param reads as a plain scalar (`ALDMANew dmaNew`), so it needs the typedef NAME set.
FN_PTR_TYPEDEF_RE = re.compile(
    r"typedef\b[^;{}]*?\(\s*\*\s*([A-Za-z_]\w*)\s*\)\s*\("
)

@functools.cache
def all_fn_ptr_typedefs():
    """The set of function-pointer typedef NAMEs defined anywhere under the project -I set, scanned
    once per process (the all_func_macros analog for typedefs). Project-wide so a param typed by a
    typedef defined beyond the .c's own include depth is still recognized — e.g. n_drvrNew.c's
    `alN_PVoiceNew(.., ALDMANew dmaNew, ..)` invokes `dmaNew(..)` via jalr, and ALDMANew lives in
    include/libultra/PR/libaudio.h (`typedef ALDMAproc (*ALDMANew)(void*)`), so without this scan
    calls_unplaced phantom-flags `calls-unplaced:dmaNew` (S139). Recurs across the audio `*New`
    constructors. The build SHA-1 oracle still catches any real divergence; this drops a phantom."""
    names = set()
    for d in INCLUDE_DIRS:
        for root, _dirs, files in os.walk(d):
            for fn in files:
                if not fn.endswith((".h", ".inc")):
                    continue
                try:
                    text = open(os.path.join(root, fn), errors="ignore").read()
                except OSError:
                    continue
                names.update(FN_PTR_TYPEDEF_RE.findall(text))
    return names

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
    # Same for the profile's `-D` define gates (`-DN_MICRO=1`): a data extern referenced only in the
    # dead side of `#ifndef N_MICRO` is phantom; symmetric with calls_unplaced/call_divergence.
    text = _strip_inactive_define_branches(text, _active_defines_for_lib(lib))
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
    fn_ptr_typedefs = all_fn_ptr_typedefs()  # typedef'd fn-ptr params (`ALDMANew dmaNew`)
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
        # A param typed by a fn-ptr typedef (`ALDMANew dmaNew`) reads as a plain scalar, so the
        # syntactic forms above miss it — match `<typedef> name` against the project typedef set.
        for td in fn_ptr_typedefs:
            names.update(
                re.findall(r"\b" + re.escape(td) + r"\s+\*?\s*([A-Za-z_]\w+)", params)
            )
    return names

_LOCAL_INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)

_MACRO_DEF_NAME_RE = re.compile(r"^\s*#\s*define\s+([A-Za-z_]\w*)", re.M)

def _local_header_macro_names(cpath):
    """Object- and function-like macro names `#define`d in the `"..."`-quoted headers the .c
    includes, resolved relative to the .c's own directory. all_func_macros() scans only the fixed
    libultra/PR header set, so a band-internal header (n_audio_sc's n_synthInternals.h, which
    `#define`s SAMPLE184) is invisible to it and its macros over-flag calls_unplaced as phantom
    callees. A macro inlines to arithmetic and never emits a named `jal`, so excluding these is
    always safe (it can only remove a false flag, never hide a real callee)."""
    names = set()
    try:
        text = UpstreamSource.get(cpath).text
    except OSError:
        return names
    base = os.path.dirname(cpath)
    for inc in _LOCAL_INCLUDE_RE.findall(text):
        hpath = os.path.normpath(os.path.join(base, inc))
        try:
            names.update(_MACRO_DEF_NAME_RE.findall(UpstreamSource.get(hpath).text))
        except OSError:
            continue
    return names

# Functions that appear ONLY inside `#ifdef _DEBUG`-guarded code (libaudio's ALFailIf expands to
# `__osError`, <assert.h> to `__assert`) that this release build strips. The hand-rolled CPP
# emulation can capture the _DEBUG side of a dual macro definition (all_func_macros reads the first
# `#define ALFailIf`, the _DEBUG one with __osError), over-flagging them; the asm reconciliation
# below drops them when the ROM fn has no unnamed `jal` budget for them. Used only to choose WHICH
# surplus name to drop, never to drop below the asm budget.
_DEBUG_ONLY_CALLEES = frozenset({"__osError", "__assert", "__osAssert", "osSyncPrintf"})

def _reconcile_calls_unplaced(flagged, rom_off, primary, unnamed_jal_addrs):
    """Ground-truth filter over `flagged` (a `calls_unplaced` result) using the candidate fn's asm
    `jal` evidence. The count of distinct unnamed `jal func_<addr>` targets is a HARD budget on how
    many flagged names can be real: each real unplaced callee IS exactly one unnamed jal, so a real
    callee is always within budget and never dropped. When more names are flagged than the asm has
    unnamed jals, the surplus are phantoms the CPP-emulation over-kept (a dead `#ifdef _DEBUG` call
    all_func_macros captured); drop the known debug-only family first, up to the surplus, and keep
    the rest (conservative — an extra flag the gate reconciles beats a dropped real callee). budget
    0 with the asm present → every flag is phantom (no unnamed callee exists). Asm absent (the
    candidate has no extracted body to read) → keep all, since the budget is unknown."""
    if not flagged:
        return flagged
    if _asm_jal_count(rom_off, primary) is None:  # asm not extracted → budget unknown, keep all
        return flagged
    budget = len(unnamed_jal_addrs)
    if budget >= len(flagged):
        return flagged
    if budget == 0:
        return []  # no unnamed callee in the asm → all flagged names are phantom
    surplus = len(flagged) - budget
    drop = [n for n in sorted(flagged) if n in _DEBUG_ONLY_CALLEES][:surplus]
    return [n for n in flagged if n not in drop]

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
    absent from placed. A function-like macro or a dead `#ifdef _DEBUG` callee the CPP-emulation
    over-keeps could still over-flag; `_reconcile_calls_unplaced` (applied at the call site) drops
    those against the asm jal budget — the automated form of the old hand reconcile at the gate."""
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
    # Same for the profile's `-D` define gates: a callee only in the dead side of `#ifndef N_MICRO`
    # (n_env's non-micro `__pow`/`_frexpf`/`_ldexpf`) is phantom under `-DN_MICRO=1`, not unplaced.
    text = _strip_inactive_define_branches(text, _active_defines_for_lib(lib))
    # Same-file function-like macros (`#define READFORMAT(ptr) ((__OSContRamReadFormat*)(ptr))` in
    # motor.c) expand to casts/exprs, not jals — collect their names so a `READFORMAT(ptr)` use is
    # not flagged as an unplaced function. macro_hidden_text only captures HEADER macros, so the .c's
    # own function-like #defines need this separate scan.
    local_macro_names = set(
        re.findall(r"^\s*#\s*define\s+([A-Za-z_]\w*)\s*\(", text, re.M)
    )
    # macro_names (from all_func_macros) covers only the fixed libultra/PR header set, so a macro
    # from a `"..."`-included band-internal header (n_audio_sc's SAMPLE184) would over-flag; add it.
    local_header_macros = _local_header_macro_names(cpath)
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
        and n not in local_header_macros
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
    # Source-relative full-path match under a SOURCE tree: a body-include (`inc/<x>.inc.c`) or a
    # subdir-qualified source-private header, copied source-relative next to the mirror's .c.
    for root in UPSTREAM_SRC_ROOTS:
        if os.path.exists(os.path.join(root, inc)):
            return True  # body-include / source-relative cp (e.g. inc/n_save_add01.inc.c)
    return (
        os.path.basename(inc) in _upstream_src_headers()
    )  # source-private → source-relative cp (bare basename)

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
    named_addrs = asm_function_addrs(off)
    addrs = [a for _, a in named_addrs]
    is_one_tu = len(addrs) == len(fns) and all(a % 16 != 0 for a in addrs[1:])
    if is_one_tu:
        hz.append(Hazard.one_tu())
    # For a PURE-classical one-tu pack (no C upstream, no vendorable asm TU = the game-code set
    # seed_points size-grades), surface the two decompose-cost signals right after one-tu, in a
    # DETERMINISTIC position (keeps the golden diff minimal):
    #   jal-free       = every member has 0 `jal` → no callee-resolution work (pts a2-deweight).
    #   rodata-straddle = the pooled .rodata holds a >=8-aligned `ldc1` double → decomposing hits the
    #                     OBJCOPY_ALIGN wall (S154); gate advice to keep the one-tu as ONE .c.
    if is_one_tu and not c_stems and not asm_tus:
        jal_counts = [_asm_jal_count(off, nm) for nm, _ in named_addrs]
        if jal_counts and all(c == 0 for c in jal_counts):  # None (asm absent) => unknown, not 0
            hz.append(Hazard.jal_free())
        doubles = [a for a in rodata_double_literals(off) if _literal_in_rodata(a, off)]
        if doubles:
            hz.append(Hazard.rodata_straddle(doubles))
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

def _wip_near_match_path(fn):
    """Path to a function's carried-wall characterization doc (docs/wip/<fn>.near-match.md)."""
    return os.path.join(ROOT, "docs", "wip", f"{fn}.near-match.md")

def _wip_is_regalloc_coin(fn):
    """True when a fn's wip doc marks a terminal below-gate regalloc/allocno coin (not
    permuter-attempted, S242). Cheap substring probe of the characterization text."""
    try:
        text = open(_wip_near_match_path(fn)).read().lower()
    except OSError:
        return False
    return "regalloc coin" in text or "allocno_compare" in text

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
        hz = [Hazard.remaining(len(fns))]
        # carried-wall detector (S241): a remaining leaf with a docs/wip/<fn>.near-match.md file is an
        # already-characterized wall a prior sprint carried. The smallest-first sort + tell-filters
        # don't see it, so it re-surfaces as "fresh" (S239/S240/S241 DoR-miss). Surface the wip'd
        # leaves here so the plan gate labels them crack-attempts, not fresh leaves. Cheap first cut =
        # wip-file existence (a BACKLOG-carry cross-ref is the fuller follow-up). Advisory only.
        walled = [fn for fn in fns if os.path.exists(_wip_near_match_path(fn))]
        if walled:
            # A below-gate regalloc/allocno coin (terminal, NOT permuter-attempted) is sub-tagged so the
            # gate skips both a fresh-leaf re-price and a wasted permuter run (S242 func_80076138). Detect
            # via a marker in the wip doc (case-insensitive "regalloc coin" or "allocno_compare").
            coin = [fn for fn in walled if _wip_is_regalloc_coin(fn)]
            hz.append(Hazard.carried_wall(walled, coin))
        return "c-stub", fns, hz
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
    ccands = recover_unplaced_call_vram(off, primary) if unplaced_calls else []
    # Drop phantom flags (a macro that inlines, or a dead `#ifdef _DEBUG` callee the CPP-emulation
    # over-kept) against the asm jal budget before annotating — the real callees are exactly the
    # unnamed jals, so this never hides one.
    unplaced_calls = _reconcile_calls_unplaced(unplaced_calls, off, primary, ccands)
    if unplaced_calls:
        # Annotate the recovered vram inline when unambiguous (one unplaced call ∩ one
        # unnamed jal), mirroring refs-unplaced, so the gate copy-pastes the func entry.
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
    # rodata-literal sibling-carve scan: the FP-pool (ldc1/lwc1 %lo(D_)) double/const that the C
    # compile emits into the code-segment .rodata at a placed vram. append_upstream_hazards runs this
    # on the NAMED-upstream path, but this coddog path otherwise skipped it, so a coddog-resolved /
    # n_audio_sc mirror's MAX_RATIO-style literal was an unflagged first-build SHA-miss (S134
    # n_resample @0x800D2190; the libultra sibling resample @0x800D23E0 was flagged ONLY because it
    # rides a NAMED c-combined row). The rodata-jtbl analog already rides _append_recover_hazards
    # above; this pairs the literal scan into the coddog path. Dedup-guarded so a c-combined coddog
    # pack's per-cfile re-entry (the _append_coddog_mirror_sources loop) does not double-append — the
    # scan is keyed on `off`, so it returns the same set on every call. S134.
    if not any(
        h.kind in (HAZARD_RODATA_LITERAL, HAZARD_DATA_STATIC) for h in hazards
    ):
        _append_rodata_carve_hazards(off, cl_path, hazards)
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
        slits = sorted(rodata_lits)
        detail = ",".join(f"0x{a:08X}" for a in slits)
        # Carve EXTENT-END: the file's own `.rodata` ends at the last literal + its width (the min
        # consecutive gap — 8 for a pooled double, 4 for a float; 8 for a lone literal). This is the
        # length the mirror's `.rodata` actually emits, distinct from carve-end below. When extent-end
        # < carve-end, the carve is a SPLIT of a larger generic subseg (carve [start, extent-end),
        # leave the remainder generic), NOT a whole-subseg flip — so the gate knows split-vs-whole
        # without an objdump of the built object (S136 n_synthesizer emitted 0x20 = 0x800D21E0..2200
        # inside a 0xA0 generic subseg, whose carve-end pool boundary was a misleading 0x800D2930).
        gaps = [b - a for a, b in zip(slits, slits[1:]) if b > a]
        width = min(gaps) if gaps else 8
        detail += f";extent-end=0x{max(slits) + width:08X}"
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

