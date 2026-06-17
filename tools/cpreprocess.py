#!/usr/bin/env python3
"""C-source text strippers for pick_target.py.

The hand-rolled mini-preprocessor pick_target uses to neutralize comments, string
literals, dead `#ifdef _DEBUG`/`#if 0` blocks, and in-body `#define` lines before its
call/ref scans. Pure string->string (stdlib + `re` only); imported back into pick_target
so its call sites read unchanged, the same facade pattern decomp_asm.py uses. Imports
nothing from pick_target (the dependency stays one-directional and acyclic).

Scope note: the version-conditional stripper (_strip_inactive_version_branches) lives in
build_config.py, not here -- it is coupled to the build-version config (_VERSION_ORD /
_build_version_ord / the Makefile-define machinery), so it sits alongside that config
cluster in its own leaf rather than with these pure C-text strippers.
"""
import re

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


_STR_LIT_RE = re.compile(r'"[^"\\]*(?:\\.[^"\\]*)*"')


def _strip_string_literals(text):
    """Replace string literal contents with empty quotes to prevent C_CALL_RE false matches."""
    return _STR_LIT_RE.sub('""', text)


_DEAD_OPEN_RE = re.compile(r"#\s*if(?:def\s+(?:_DEBUG|NU_DEBUG|AUD_PROFILE)|ndef\s+_FINALROM|\s+0)\b")
_PP_IF_RE = re.compile(r"#\s*if")
_PP_ENDIF_RE = re.compile(r"#\s*endif")


def _strip_dead_blocks(text):
    """Drop `#ifdef _DEBUG` / `#ifdef NU_DEBUG` / `#ifdef AUD_PROFILE` / `#ifndef _FINALROM` / `#if 0`
    regions (matched #endif, nested). The audio band's debug-counter macros (the PROFILE_AUD()
    timing calls + their gating `extern u32 ...[]` decl) are `#ifdef AUD_PROFILE`-guarded and MG64
    does not define it, so those refs/calls are phantom -- without this, an audio mirror would carry
    a false `refs-unplaced` on the counter."""
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


_DEFINE_LINE_RE = re.compile(r"^[ \t]*#[ \t]*define\b")


def _strip_define_lines(text):
    """Drop `#define` directive lines (including `\\`-continuations). A `#define NAME (expr)`
    object/function-like macro DEFINITION is not a call site, yet its `NAME (` matches C_CALL_RE and
    inflates both the C-side jal count and the calls-unplaced set (e.g. a `#define xxsine (x * sine)`
    would be counted as a phantom call → a false `jal-count-mismatch` + `calls-unplaced:xxsine` that
    mis-routes a clean near-verbatim mirror toward the classical loop). Strip the source's own
    #define lines before
    the call scan (the one-level macro EXPANSION bodies appended by macro_hidden_text carry no
    #define directives, so macro-hidden-extern detection is unaffected)."""
    out, cont = [], False
    for line in text.splitlines():
        if cont:
            cont = line.rstrip().endswith("\\")
            continue
        if _DEFINE_LINE_RE.match(line):
            cont = line.rstrip().endswith("\\")
            continue
        out.append(line)
    return "\n".join(out)
