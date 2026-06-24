#!/usr/bin/env python3
"""BUILD_VERSION config + version-conditional source stripping for pick_target.

The shared leaf both the C-side scanners (call_divergence, refs/calls-unplaced, file-static,
defines-data, ...) and the asm-TU vendoring scanners depend on: the per-library BUILD_VERSION
ordinal (read from the Makefile's `-DBUILD_VERSION=VERSION_X`) and the `#if BUILD_VERSION <op>
VERSION_X` dead-branch stripper. Stdlib + decomp_common only (imports nothing from pick_target, so
the dependency stays one-directional and acyclic); pick_target re-imports what it still references.

The build compiles ONE side of `#if BUILD_VERSION <op> VERSION_X` (libultra is
`-DBUILD_VERSION=VERSION_J`), so the inactive side's refs/calls are phantom -- they never reach the
real object. E.g. osCartRomInit's `CartRomHandle` + `osPiRawReadIo` live only in the non-J `#else`
branch, yet refs/calls-unplaced flagged them. Only the *exact* form `BUILD_VERSION <op> VERSION_X`
is evaluated; a compound condition (`&&`/`||`) or any other `#if` is opaque (both branches kept,
nesting tracked so #else/#endif still pair correctly). A lib with no `-DBUILD_VERSION`
(libkmc/libnusys) yields ordinal 0 -> no-op (those upstreams don't gate on it).
"""

import functools
import os
import re

from decomp_common import PROJECT_ROOT as ROOT

# Shared with pick_target's staying preprocessor scanners (re-imported there); also used by
# _strip_inactive_version_branches below.
_PP_OPENING_DIRECTIVE_RE = re.compile(r"^\s*#\s*if(?:def|ndef)?\b")
_PP_ENDIF_LINE_RE = re.compile(r"^\s*#\s*endif\b")

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
                m = re.match(
                    r"^\s*(CFLAGS|LIBKMC_CFLAGS|LIBNUSYS_CFLAGS|LIBULTRA_CFLAGS)\s*[:+]?=",
                    line,
                )
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
        ">=": ord_ >= rhs,
        "<=": ord_ <= rhs,
        ">": ord_ > rhs,
        "<": ord_ < rhs,
        "==": ord_ == rhs,
        "!=": ord_ != rhs,
    }[op]


def _strip_inactive_version_branches(text, build_ord):
    """Drop the dead side of `#if BUILD_VERSION <op> VERSION_X [#else] #endif` for `build_ord`.
    Non-BUILD_VERSION conditionals pass through unchanged (both branches kept); nesting is tracked
    so #else/#endif pair to the right directive. build_ord 0 → no-op."""
    if not build_ord:
        return text
    out, stack = (
        [],
        [],
    )  # frame: {"ver": bool, "emit": bool}; line live iff all frames emit
    live = lambda: all(fr["emit"] for fr in stack)
    for line in text.splitlines():
        s = line.lstrip()
        m = _BUILD_VERSION_IF_RE.match(s)
        if m:
            stack.append(
                {
                    "ver": True,
                    "emit": _eval_build_version(m.group(1), m.group(2), build_ord),
                }
            )
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
